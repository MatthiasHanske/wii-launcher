#include "dolcheck.h"

#include <stdint.h>
#include <string.h>

static u32 be32(const u8* p) {
    return ((u32)p[0] << 24) | ((u32)p[1] << 16) | ((u32)p[2] << 8) | p[3];
}
static u16 be16(const u8* p) { return (u16)((p[0] << 8) | p[1]); }

static bool inMem1(u32 a) { return a >= 0x80000000 && a < 0x81800000; }
static bool inMem2(u32 a) { return a >= 0x90000000 && a < 0x94000000; }

// Bereiche, die der Booter beim Laden selbst braucht: Programmkopie bis Booter-Ende inkl.
// Stack (0x92000000..0x93008540) und die Argumente an 0x93300800.
static bool hits(uint64_t start, uint64_t end, uint64_t lo, uint64_t hi) {
    return start < hi && lo < end;
}
static bool overlapsReserved(u32 addr, u32 len) {
    if (len == 0) return false;
    uint64_t start = addr & 0x3FFFFFFF;
    uint64_t end = start + len;
    return hits(start, end, 0x12000000, 0x13008540) ||
           hits(start, end, 0x13300800, 0x13300800 + DOLCHECK_MAX_ARGS_SIZE);
}

static bool isElf(const u8* d, size_t size) {
    return size >= 4 && memcmp(d, "\x7f" "ELF", 4) == 0;
}

// ELF wie load_elf_image im Booter: ET_EXEC, EM_PPC, 32-Byte-Programmheader, PT_LOAD in
// der Datei, filesz <= memsz, Ziel nicht reserviert, Entry in einem geladenen Segment.
static bool inspectElf(const u8* d, size_t size, ExecInfo* info) {
    if (size < 52 || be16(d + 16) != 2 || be16(d + 18) != 20) return false;
    u32 phoff = be32(d + 28);
    u16 phentsize = be16(d + 42);
    u16 phnum = be16(d + 44);
    if (phoff == 0 || phnum == 0 || phentsize != 32) return false;
    if (phoff > size || (uint64_t)phnum * 32 > size - phoff) return false;
    u32 entry = (be32(d + 24) & 0x3FFFFFFF) | 0x80000000;
    bool entryFound = false;
    for (u16 i = 0; i < phnum; ++i) {
        const u8* ph = d + phoff + i * 32u;
        if (be32(ph) != 1) continue;  // PT_LOAD
        u32 off = be32(ph + 4);
        u32 dest = (be32(ph + 12) & 0x3FFFFFFF) | 0x80000000;
        u32 filesz = be32(ph + 16);
        u32 memsz = be32(ph + 20);
        if (filesz > memsz) return false;
        if (off > size || filesz > size - off) return false;
        if (overlapsReserved(dest, memsz)) return false;
        if (!entryFound && filesz > 0 && entry >= dest && entry - dest < filesz) {
            entryFound = true;
            info->entryOffset = off + (entry - dest);
        }
    }
    info->elf = true;
    return entryFound;
}

// DOL-Header: 7 Text- + 11 Datensektionen (Offsets 0x00, Adressen 0x48, Größen 0x90),
// BSS 0xD8/0xDC, Entry 0xE0. Sektionen mit Größe 0 oder Adresse < 0x100 gelten als leer.
static bool inspectDol(const u8* d, size_t size, ExecInfo* info) {
    if (size <= 0x100) return false;
    u32 entry = be32(d + 0xE0);
    if (!inMem1(entry) && !inMem2(entry)) return false;
    bool anyText = false;
    bool entryFound = false;
    for (int i = 0; i < 18; ++i) {
        u32 off = be32(d + 0x00 + i * 4);
        u32 addr = be32(d + 0x48 + i * 4);
        u32 len = be32(d + 0x90 + i * 4);
        if (len == 0 || addr < 0x100) continue;
        if (off < 0x100 || off > size || len > size - off) return false;
        if (overlapsReserved(addr, len)) return false;
        if (i < 7) {
            anyText = true;
            if (!entryFound && entry >= addr && entry - addr < len) {
                entryFound = true;
                info->entryOffset = off + (entry - addr);
            }
        }
    }
    if (!anyText || !entryFound) return false;
    u32 bss = be32(d + 0xD8);
    u32 bssLen = be32(d + 0xDC);
    if (bssLen > 0) {
        u32 last = bss + bssLen - 1;
        bool mem1 = inMem1(bss) && inMem1(last) && last >= bss;
        bool mem2 = inMem2(entry) && inMem2(bss) && inMem2(last) && last >= bss;
        if (!mem1 && !mem2) return false;
        if (overlapsReserved(bss, bssLen)) return false;
    }
    info->elf = false;
    return true;
}

bool dolcheck_inspect(const u8* d, size_t size, ExecInfo* info) {
    info->elf = false;
    info->entryOffset = 0;
    return isElf(d, size) ? inspectElf(d, size, info) : inspectDol(d, size, info);
}

bool dolcheck_accepts_argv(const u8* d, size_t size, const ExecInfo* info) {
    if (!info->elf && d[0x100] == 0x3C) return false;  // DollZ
    if (info->entryOffset > size || size - info->entryOffset < 8) return false;
    return be32(d + info->entryOffset + 4) == ARGV_MAGIC;
}
