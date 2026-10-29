#!/usr/bin/env python3
"""Convert Dolphin RVZ images of GameCube discs into raw ISO files.

Pure Python (3.14+, uses the stdlib module compression.zstd). No external
binaries. Only GameCube discs (disc_type 1, no Wii partitions) with
compression NONE (0) or Zstandard (5) are supported.

Format reference: dolphin-emu docs/WiaAndRvz.md, DiscIO/WIABlob.cpp,
DiscIO/WIACompression.cpp (RVZPackDecompressor), DiscIO/LaggedFibonacciGenerator.cpp.

Usage:
    python3 -I tools/rvz2iso.py <in.rvz> <out.iso>
    python3 -I tools/rvz2iso.py --verify-only <in.rvz>
"""

import argparse
import hashlib
import os
import struct
import sys
import time
import zlib

try:
    from compression import zstd
except ImportError:  # pragma: no cover - depends on interpreter build
    zstd = None

RVZ_MAGIC = b"RVZ\x01"
RVZ_VERSION = 0x01000000              # Dolphin RVZ_VERSION
RVZ_VERSION_READ_COMPATIBLE = 0x00030000

HEADER1_SIZE = 0x48
HEADER2_MIN_SIZE = 0xDC - 7           # without compressor_data
DHEAD_SIZE = 0x80
BLOCK_SIZE = 0x8000                   # VolumeWii::BLOCK_TOTAL_SIZE
DISC_TYPE_GC = 1
DISC_TYPE_WII = 2
COMP_NONE = 0
COMP_ZSTD = 5
COMP_NAMES = {0: "NONE", 1: "PURGE", 2: "BZIP2", 3: "LZMA", 4: "LZMA2", 5: "ZSTD"}

RAW_ENTRY = struct.Struct(">QQII")    # data_offset, data_size, group_index, n_groups
GROUP_ENTRY = struct.Struct(">III")   # data_off4, data_size, rvz_packed_size


class RvzError(Exception):
    pass


# --------------------------------------------------------------------------
# Lagged Fibonacci generator (junk data), port of Dolphin's implementation.
# The state is kept as the 2084 output bytes (521 big-endian words). XOR is
# byte-order independent, so Forward() can work directly on those bytes.
# --------------------------------------------------------------------------
LFG_K = 521
LFG_J = 32
SEED_WORDS = 17
SEED_BYTES = SEED_WORDS * 4
LFG_BYTES = LFG_K * 4                 # 2084
_J_BYTES = LFG_J * 4                  # 128


def _xor(a, b):
    n = len(a)
    return (int.from_bytes(a, "big") ^ int.from_bytes(b, "big")).to_bytes(n, "big")


class LaggedFibonacciGenerator:
    __slots__ = ("buf", "pos")

    def __init__(self, seed):
        if len(seed) != SEED_BYTES:
            raise RvzError("junk seed must be 68 bytes")
        words = list(struct.unpack(">17I", seed)) + [0] * (LFG_K - SEED_WORDS)
        for i in range(SEED_WORDS, LFG_K):
            words[i] = (((words[i - 17] << 23) & 0xFFFFFFFF)
                        ^ (words[i - 16] >> 9) ^ words[i - 1])
        # "shift by 18 instead of 16" quirk, applied once up front like Dolphin
        words = [(x & 0xFF00FFFF) | ((x >> 2) & 0x00FF0000) for x in words]
        self.buf = bytearray(struct.pack(">521I", *words))
        self.pos = 0
        for _ in range(4):
            self._forward()

    def _forward(self):
        b = self.buf
        # words 0..31 ^= words 489..520
        b[0:_J_BYTES] = _xor(b[0:_J_BYTES], b[LFG_BYTES - _J_BYTES:LFG_BYTES])
        # words i ^= words i-32 for i in 32..520, sequential in blocks of 32
        for k in range(_J_BYTES, 16 * _J_BYTES + 1, _J_BYTES):
            end = min(k + _J_BYTES, LFG_BYTES)
            b[k:end] = _xor(b[k:end], b[k - _J_BYTES:end - _J_BYTES])

    def skip(self, count):
        self.pos += count
        while self.pos >= LFG_BYTES:
            self._forward()
            self.pos -= LFG_BYTES

    def get(self, count):
        out = bytearray()
        while count > 0:
            n = min(count, LFG_BYTES - self.pos)
            out += self.buf[self.pos:self.pos + n]
            self.pos += n
            count -= n
            if self.pos == LFG_BYTES:
                self._forward()
                self.pos = 0
        return out


# --------------------------------------------------------------------------
# Reader
# --------------------------------------------------------------------------
class Stats:
    def __init__(self):
        self.groups = 0
        self.zero_groups = 0
        self.zstd_groups = 0
        self.plain_groups = 0
        self.packed_groups = 0
        self.junk_segments = 0
        self.junk_bytes = 0
        self.data_segments = 0
        self.max_segment = 0


class RvzReader:
    def __init__(self, path):
        self.path = path
        self.f = open(path, "rb")
        self.stats = Stats()
        self._parse()

    def close(self):
        self.f.close()

    def _read_at(self, off, size):
        self.f.seek(off)
        data = self.f.read(size)
        if len(data) != size:
            raise RvzError(f"short read at 0x{off:x} ({len(data)}/{size} bytes)")
        return data

    def _decompress_table(self, off, size, expected):
        data = self._read_at(off, size)
        if self.compression == COMP_ZSTD:
            data = zstd.decompress(data)
        if len(data) != expected:
            raise RvzError(f"table at 0x{off:x}: {len(data)} bytes, expected {expected}")
        return data

    def _parse(self):
        h1 = self._read_at(0, HEADER1_SIZE)
        (magic, version, version_compat, h2_size, h2_hash,
         self.iso_size, self.rvz_size, h1_hash) = struct.unpack(">4sIII20sQQ20s", h1)
        if magic != RVZ_MAGIC:
            raise RvzError(f"not an RVZ file (magic {magic!r})")
        if RVZ_VERSION < version_compat or RVZ_VERSION_READ_COMPATIBLE > version:
            raise RvzError(f"unsupported RVZ version 0x{version:08x}")
        if hashlib.sha1(h1[:-20]).digest() != h1_hash:
            raise RvzError("header 1 SHA-1 mismatch")
        actual = os.fstat(self.f.fileno()).st_size
        if actual != self.rvz_size:
            raise RvzError(f"file size {actual} != header {self.rvz_size} (truncated?)")
        if h2_size < HEADER2_MIN_SIZE:
            raise RvzError("header 2 too small")
        h2 = self._read_at(HEADER1_SIZE, h2_size)
        if hashlib.sha1(h2).digest() != h2_hash:
            raise RvzError("header 2 SHA-1 mismatch")
        h2 = h2 + bytes(max(0, 0xDC - len(h2)))

        (self.disc_type, self.compression, self.compr_level,
         self.chunk_size) = struct.unpack_from(">IIiI", h2, 0)
        self.dhead = h2[0x10:0x10 + DHEAD_SIZE]
        n_part, part_size, part_off = struct.unpack_from(">IIQ", h2, 0x90)
        (n_raw, raw_off, raw_size, n_groups, group_off,
         group_size) = struct.unpack_from(">IQIIQI", h2, 0xB4)

        if self.disc_type == DISC_TYPE_WII:
            raise RvzError("Wii disc images are not supported (GameCube only)")
        if self.disc_type != DISC_TYPE_GC:
            raise RvzError(f"unsupported disc_type {self.disc_type} (only 1 = GameCube)")
        if n_part != 0:
            raise RvzError("image has Wii partition entries; not supported")
        if self.compression not in (COMP_NONE, COMP_ZSTD):
            name = COMP_NAMES.get(self.compression, str(self.compression))
            raise RvzError(f"unsupported compression {name}; only NONE and ZSTD are supported")
        if self.compression == COMP_ZSTD and zstd is None:
            raise RvzError("compression.zstd is not available (needs Python 3.14+)")
        cs = self.chunk_size
        pow2 = cs & (cs - 1) == 0
        if not ((cs >= BLOCK_SIZE and pow2) or (cs and cs % 0x200000 == 0)):
            raise RvzError(f"invalid chunk_size 0x{cs:x}")

        raw = self._decompress_table(raw_off, raw_size, n_raw * RAW_ENTRY.size)
        self.raw_entries = [RAW_ENTRY.unpack_from(raw, i * RAW_ENTRY.size)
                            for i in range(n_raw)]
        grp = self._decompress_table(group_off, group_size, n_groups * GROUP_ENTRY.size)
        self.groups = [GROUP_ENTRY.unpack_from(grp, i * GROUP_ENTRY.size)
                       for i in range(n_groups)]

    # -- group decoding ----------------------------------------------------
    def _unpack(self, packed, out_len, disc_offset):
        """Decode RVZ packing (data + junk segments) into out_len bytes."""
        st = self.stats
        out = bytearray()
        p = 0
        n = len(packed)
        while p < n:
            if p + 4 > n:
                raise RvzError(f"truncated segment header @disc 0x{disc_offset:x}")
            size = struct.unpack_from(">I", packed, p)[0]
            p += 4
            junk = size & 0x80000000
            size &= 0x7FFFFFFF
            if len(out) + size > out_len:
                raise RvzError(f"segment overruns group @disc 0x{disc_offset:x}: "
                               f"{len(out)}+{size} > {out_len}")
            st.max_segment = max(st.max_segment, size)
            if junk:
                if p + SEED_BYTES > n:
                    raise RvzError(f"truncated junk seed @disc 0x{disc_offset:x}")
                lfg = LaggedFibonacciGenerator(packed[p:p + SEED_BYTES])
                p += SEED_BYTES
                lfg.skip((disc_offset + len(out)) % BLOCK_SIZE)
                out += lfg.get(size)
                st.junk_segments += 1
                st.junk_bytes += size
            else:
                if p + size > n:
                    raise RvzError(f"truncated data segment @disc 0x{disc_offset:x}")
                out += packed[p:p + size]
                p += size
                st.data_segments += 1
        if len(out) != out_len:
            raise RvzError(f"group @disc 0x{disc_offset:x} unpacked to {len(out)} "
                           f"bytes, expected {out_len}")
        return out

    def _read_group(self, index, out_len, disc_offset):
        st = self.stats
        st.groups += 1
        off4, data_size, packed_size = self.groups[index]
        compressed = bool(data_size & 0x80000000)
        size = data_size & 0x7FFFFFFF
        if size == 0:
            st.zero_groups += 1
            return bytes(out_len)
        data = self._read_at(off4 << 2, size)
        if compressed and self.compression == COMP_ZSTD:
            try:
                data = zstd.decompress(data)
            except zstd.ZstdError as e:
                raise RvzError(f"zstd error in group {index} @disc 0x{disc_offset:x}: {e}")
            st.zstd_groups += 1
        else:
            st.plain_groups += 1
        if packed_size:
            if len(data) != packed_size:
                raise RvzError(f"group {index}: decompressed {len(data)} bytes, "
                               f"rvz_packed_size {packed_size}")
            st.packed_groups += 1
            return self._unpack(data, out_len, disc_offset)
        if len(data) != out_len:
            raise RvzError(f"group {index}: {len(data)} bytes, expected {out_len}")
        return data

    def iter_disc(self):
        """Yield (disc_offset, bytes) in ascending, gap-free order."""
        pos = 0
        for data_off, data_size, gidx, ngroups in sorted(self.raw_entries):
            if data_size == 0:
                continue
            skipped = data_off % BLOCK_SIZE
            start = data_off - skipped
            size = data_size + skipped
            if gidx + ngroups > len(self.groups):
                raise RvzError("raw data entry references missing groups")
            for i in range(ngroups):
                goff = start + i * self.chunk_size
                if goff >= start + size:
                    raise RvzError("more groups than raw data size allows")
                glen = min(self.chunk_size, size - i * self.chunk_size)
                if goff + glen <= pos:
                    continue
                if goff > pos:
                    raise RvzError(f"gap in disc data at 0x{pos:x}..0x{goff:x}")
                data = self._read_group(gidx + i, glen, goff)
                if goff < pos:
                    data = data[pos - goff:]
                yield pos, data
                pos += len(data)
        if pos != self.iso_size:
            raise RvzError(f"disc data ends at 0x{pos:x}, iso_file_size is 0x{self.iso_size:x}")


# --------------------------------------------------------------------------
def convert(src, dst, quiet=False):
    t0 = time.monotonic()
    r = RvzReader(src)
    try:
        game_id = r.dhead[:6].decode("ascii", "replace")
        if not quiet:
            print(f"{os.path.basename(src)}: {game_id}, iso_size={r.iso_size}, "
                  f"compression={COMP_NAMES.get(r.compression)} level={r.compr_level}, "
                  f"chunk=0x{r.chunk_size:x}, groups={len(r.groups)}", file=sys.stderr)
        sha1, md5, crc = hashlib.sha1(), hashlib.md5(), 0
        out = None
        tmp = None
        if dst:
            tmp = dst + ".part"
            out = open(tmp, "wb")
        written = 0
        last = -1
        dhead_mismatch = False
        try:
            for off, data in r.iter_disc():
                if off < DHEAD_SIZE:
                    n = min(DHEAD_SIZE - off, len(data))
                    if bytes(data[:n]) != r.dhead[off:off + n]:
                        dhead_mismatch = True
                    data = bytearray(data)
                    data[:n] = r.dhead[off:off + n]
                sha1.update(data)
                md5.update(data)
                crc = zlib.crc32(data, crc)
                if out:
                    out.write(data)
                written += len(data)
                pct = written * 100 // r.iso_size
                if not quiet and pct != last:
                    last = pct
                    el = time.monotonic() - t0
                    print(f"\r  {pct:3d}%  {written >> 20} / {r.iso_size >> 20} MiB  "
                          f"{el:6.1f}s", end="", file=sys.stderr, flush=True)
            if out:
                out.flush()
                os.fsync(out.fileno())
                out.close()
                out = None
                if os.path.getsize(tmp) != r.iso_size:
                    raise RvzError("output size mismatch")
                os.replace(tmp, dst)
        finally:
            if out:
                out.close()
            if tmp and os.path.exists(tmp):
                os.remove(tmp)  # only left over on failure
        st = r.stats
        result = {
            "game_id": game_id,
            "size": written,
            "sha1": sha1.hexdigest(),
            "md5": md5.hexdigest(),
            "crc32": f"{crc:08x}",
            "seconds": time.monotonic() - t0,
            "dhead_mismatch": dhead_mismatch,
            "stats": vars(st),
        }
        if not quiet:
            print(file=sys.stderr)
            print(f"size   {written}\ncrc32  {result['crc32']}\nmd5    {result['md5']}\n"
                  f"sha1   {result['sha1']}\ntime   {result['seconds']:.1f}s", file=sys.stderr)
            print(f"groups {st.groups} (zstd {st.zstd_groups}, plain {st.plain_groups}, "
                  f"zero {st.zero_groups}, packed {st.packed_groups}); segments data "
                  f"{st.data_segments}, junk {st.junk_segments} ({st.junk_bytes} bytes); "
                  f"largest segment {st.max_segment} <= chunk {r.chunk_size}", file=sys.stderr)
        if dhead_mismatch:
            print("warning: group data differs from header dhead in the first 0x80 "
                  "bytes; header copy was used", file=sys.stderr)
        return result
    finally:
        r.close()


def main(argv=None):
    ap = argparse.ArgumentParser(description="Convert GameCube RVZ images to raw ISO.")
    ap.add_argument("--verify-only", action="store_true",
                    help="decode everything and print checksums, write nothing")
    ap.add_argument("-q", "--quiet", action="store_true")
    ap.add_argument("input")
    ap.add_argument("output", nargs="?")
    a = ap.parse_args(argv)
    if not a.verify_only and not a.output:
        ap.error("output path required (or use --verify-only)")
    if a.output and os.path.exists(a.output):
        ap.error(f"{a.output} exists, refusing to overwrite")
    try:
        res = convert(a.input, None if a.verify_only else a.output, a.quiet)
    except RvzError as e:
        print(f"\nerror: {e}", file=sys.stderr)
        return 1
    print(f"{res['sha1']}  {res['crc32']}  {res['size']}  {a.output or a.input}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
