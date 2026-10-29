#include "wii/boot.h"

#include <gccore.h>
#include <ogc/es.h>
#include <ogc/ios.h>
#include <ogc/machine/processor.h>
#include <ogc/system.h>
#include <ogc/wiilaunch.h>
#include <sys/stat.h>
#include <wiiuse/wpad.h>

#include <cstring>
#include <string>
#include <vector>

#include "app_booter_bin.h"

namespace {

// Adressen passend zu booter/source/main.c und booter/source/link.ld.
u8* const kExecAddr = reinterpret_cast<u8*>(0x92000000);
u8* const kBooterAddr = reinterpret_cast<u8*>(0x93000000);
u8* const kArgsAddr = reinterpret_cast<u8*>(0x93300800);
// Platz zwischen Programm und Booter: 0x92000000..0x93000000.
constexpr size_t kMaxExecSize = 0x01000000;
// Obergrenze für struct __argv + Befehlszeile (weit unter dem IPC-Heap von libogc).
constexpr size_t kMaxArgsSize = 0x8000;

constexpr u32 kPriiloaderMagic = 0x50756e65;  // "Pune": System Menu, kein Autoboot
constexpr u64 kIos58 = 0x000000010000003AULL;

typedef void (*Entry)(void);

bool ios58Installed() {
    u32 size = 0;
    return ES_GetStoredTMDSize(kIos58, &size) >= 0 && size > 0;
}

// Wie SetupARGV in USB Loader GX: Struktur am Anfang, Befehlszeile (NUL-getrennt) direkt dahinter.
// Länge prüft bootDol vorher (kMaxArgsSize). Programme ohne "_arg"-Slot bekommen eine
// genullte Struktur (argvMagic = 0), dann kopiert der Booter nichts nach Entry+8.
void writeArgv(const std::vector<std::string>& args, bool withArgv) {
    if (!withArgv) {
        std::memset(kArgsAddr, 0, sizeof(struct __argv));
        DCFlushRange(kArgsAddr, sizeof(struct __argv));
        return;
    }
    size_t length = 1;
    for (const std::string& a : args) length += a.size() + 1;

    struct __argv* argv = reinterpret_cast<struct __argv*>(kArgsAddr);
    std::memset(argv, 0, sizeof(struct __argv));
    char* cmd = reinterpret_cast<char*>(kArgsAddr + sizeof(struct __argv));
    size_t pos = 0;
    for (const std::string& a : args) {
        std::memcpy(cmd + pos, a.c_str(), a.size() + 1);
        pos += a.size() + 1;
    }
    cmd[length - 1] = '\0';

    argv->argvMagic = ARGV_MAGIC;
    argv->commandLine = cmd;
    argv->length = static_cast<int>(length);
    argv->argc = static_cast<int>(args.size());
    argv->argv = &argv->commandLine;
    argv->endARGV = argv->argv + 1;
    DCFlushRange(kArgsAddr, sizeof(struct __argv) + length);
}

u32 be32(const u8* p) { return (u32(p[0]) << 24) | (u32(p[1]) << 16) | (u32(p[2]) << 8) | p[3]; }
u16 be16(const u8* p) { return static_cast<u16>((p[0] << 8) | p[1]); }

bool inMem1(u32 a) { return a >= 0x80000000 && a < 0x81800000; }
bool inMem2(u32 a) { return a >= 0x90000000 && a < 0x94000000; }

// Liest Ausschnitte der Programmdatei: aus dem Speicher (bootDol, ganze Datei geladen) oder
// per readBytes von der SD (canBoot, nur Header, Programmheader und Entry+4..8).
class ExecReader {
public:
    ExecReader(const u8* data, size_t size) : data_(data), size_(size) {}
    ExecReader(WiiFs& fs, const std::string& path, size_t size) : fs_(&fs), path_(&path), size_(size) {}

    size_t size() const { return size_; }

    // false, wenn der Bereich nicht ganz in der Datei liegt oder das Lesen scheitert.
    bool read(size_t off, size_t len, std::vector<u8>& out) {
        if (off > size_ || len > size_ - off) return false;
        if (data_) {
            out.assign(data_ + off, data_ + off + len);
            return true;
        }
        readError_ = !fs_->readBytes(*path_, off, len, out) || out.size() != len;
        return !readError_;
    }
    bool readError() const { return readError_; }

private:
    const u8* data_ = nullptr;
    WiiFs* fs_ = nullptr;
    const std::string* path_ = nullptr;
    size_t size_ = 0;
    bool readError_ = false;
};

// Ergebnis der Header-Prüfung.
struct ExecInfo {
    bool elf = false;
    // Datei-Offset der Einsprungadresse; die libogc-crt0 legt dort "b ...; .ascii \"_arg\""
    // ab, der Booter kopiert struct __argv nach Entry+8.
    size_t entryOffset = 0;
};

// Physische Bereiche, die der Booter während des Ladens selbst braucht: Programmkopie bis
// Booter-Ende inkl. Stack (link.ld: 0x93000000 + Code + 0x8000 Stack = 0x93008540) und die
// Argumente. Ein Programm, das dorthin lädt, würde sich beim Laden selbst überschreiben.
bool overlapsReserved(u32 addr, u32 len) {
    if (len == 0) return false;
    uint64_t start = addr & 0x3FFFFFFF;
    uint64_t end = start + len;
    auto hits = [&](uint64_t lo, uint64_t hi) { return start < hi && lo < end; };
    return hits(0x12000000, 0x13008540) ||
           hits(0x13300800, 0x13300800 + kMaxArgsSize);
}

// ELF wie load_elf_image im Booter: ET_EXEC, EM_PPC, Programmheader vorhanden und 32 Byte
// groß, jedes PT_LOAD-Segment in der Datei, filesz <= memsz, Ziel (p_paddr, auf 0x8...
// normiert) nicht im reservierten Bereich, Entry in einem geladenen Segment.
bool inspectElf(ExecReader& r, ExecInfo& info) {
    const size_t size = r.size();
    std::vector<u8> h;
    if (size < 52 || !r.read(0, 52, h)) return false;
    const u8* d = h.data();
    if (be16(d + 16) != 2 || be16(d + 18) != 20) return false;
    u32 phoff = be32(d + 28);
    u16 phentsize = be16(d + 42);
    u16 phnum = be16(d + 44);
    if (phoff == 0 || phnum == 0 || phentsize != 32) return false;
    if (phoff > size || static_cast<uint64_t>(phnum) * 32 > size - phoff) return false;
    u32 entry = (be32(d + 24) & 0x3FFFFFFF) | 0x80000000;
    std::vector<u8> phs;
    if (!r.read(phoff, phnum * 32u, phs)) return false;
    bool entryFound = false;
    for (u16 i = 0; i < phnum; ++i) {
        const u8* ph = phs.data() + i * 32u;
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
            info.entryOffset = off + (entry - dest);
        }
    }
    info.elf = true;
    return entryFound;
}

// DOL-Header (big endian): 7 Text- + 11 Datensektionen, Offsets 0x00, Adressen 0x48,
// Größen 0x90; BSS 0xD8/0xDC; Entry 0xE0. Wie load_dol_image im Booter gelten Sektionen
// mit Größe 0 oder Adresse < 0x100 als nicht geladen.
bool inspectDol(ExecReader& r, ExecInfo& info) {
    const size_t size = r.size();
    std::vector<u8> h;
    if (size <= 0x100 || !r.read(0, 0x100, h)) return false;
    const u8* d = h.data();
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
                info.entryOffset = off + (entry - addr);
            }
        }
    }
    if (!anyText || !entryFound) return false;
    u32 bss = be32(d + 0xD8);
    u32 bssLen = be32(d + 0xDC);
    if (bssLen > 0) {
        // BSS in MEM1 (MEM2 nur bei MEM2-DOLs), Ende einschließlich.
        u32 last = bss + bssLen - 1;
        bool mem1 = inMem1(bss) && inMem1(last) && last >= bss;
        bool mem2 = inMem2(entry) && inMem2(bss) && inMem2(last) && last >= bss;
        if (!mem1 && !mem2) return false;
        if (overlapsReserved(bss, bssLen)) return false;
    }
    info.elf = false;
    return true;
}

// argv nur, wenn an Entry+4 "_arg" steht (wie der HBC). Sonst läge an Entry+8 Code, den
// der Booter mit 24 Byte argv überschreiben würde (z. B. HackMii-Installer boot.elf).
// DollZ-gepackte DOLs (Byte 0x100 == 0x3C) bekommen zusätzlich nie argv.
bool acceptsArgv(ExecReader& r, const ExecInfo& info) {
    std::vector<u8> b;
    if (!info.elf && (!r.read(0x100, 1, b) || b[0] == 0x3C)) return false;
    if (!r.read(info.entryOffset, 8, b)) return false;
    return be32(b.data() + 4) == ARGV_MAGIC;
}

// Gemeinsame Prüfung von canBoot und bootDol (schreibt nichts). reason nur bei false gesetzt.
bool checkExec(ExecReader& r, const std::string& path, bool& withArgv, std::string& reason) {
    std::vector<u8> magic;
    if (r.size() < 4) {
        reason = "Kein g\xC3\xBCltiges Programm: " + path;
        return false;
    }
    if (!r.read(0, 4, magic)) {
        reason = "Nicht lesbar: " + path;
        return false;
    }
    bool elf = std::memcmp(magic.data(), "\x7f" "ELF", 4) == 0;
    ExecInfo info;
    bool valid = elf ? inspectElf(r, info) : inspectDol(r, info);
    if (valid) withArgv = acceptsArgv(r, info);
    if (r.readError()) {
        reason = "Nicht lesbar: " + path;
        return false;
    }
    if (!valid) {
        reason = "Kein g\xC3\xBCltiges Programm: " + path;
        return false;
    }
    return true;
}

// argv[0] = dolPath, dann plan.args.
std::vector<std::string> buildArgs(const LaunchPlan& plan) {
    std::vector<std::string> args;
    args.reserve(plan.args.size() + 1);
    args.push_back(plan.dolPath);
    args.insert(args.end(), plan.args.begin(), plan.args.end());
    return args;
}

bool argsFit(const std::vector<std::string>& args) {
    size_t argsLength = sizeof(struct __argv) + 1;
    for (const std::string& a : args) argsLength += a.size() + 1;
    return argsLength <= kMaxArgsSize;
}

// Der eigene Heap darf nicht in den Zielbereich gewachsen sein.
bool heapBelowExec() { return reinterpret_cast<u32>(SYS_GetArena2Lo()) <= reinterpret_cast<u32>(kExecAddr); }

// libogc prüft beim exit() den HBC-Reload-Stub ("STUB" / "HAXX"). Ohne die Magic fällt
// exit() auf SYS_RETURNTOMENU zurück, also Wii-Menü statt HBC.
void clearHbcStub() {
    *reinterpret_cast<vu32*>(0x80001804) = 0;
    *reinterpret_cast<vu32*>(0x80001808) = 0;
    DCFlushRange(reinterpret_cast<void*>(0x80001800), 32);
}

}  // namespace

bool canBoot(const LaunchPlan& plan, WiiFs& fs, std::string& reason) {
    if (plan.dolPath.empty()) {
        reason = "Kein Startprogramm bekannt";
        return false;
    }
    struct stat st;
    if (stat(plan.dolPath.c_str(), &st) != 0) {
        reason = "Fehlt auf der SD: " + plan.dolPath;
        return false;
    }
    size_t size = static_cast<size_t>(st.st_size);
    if (size == 0 || size > kMaxExecSize) {
        reason = (size == 0 ? "Leere Datei: " : "Gr\xC3\xB6\xC3\x9F" "er als 16 MB: ") + plan.dolPath;
        return false;
    }
    if (!argsFit(buildArgs(plan))) {
        reason = "Startargumente zu lang";
        return false;
    }
    ExecReader r(fs, plan.dolPath, size);
    bool withArgv = false;
    if (!checkExec(r, plan.dolPath, withArgv, reason)) return false;
    if (!heapBelowExec()) {
        reason = "Zu wenig Speicher";
        return false;
    }
    return true;
}

bool bootDol(const LaunchPlan& plan, WiiFs& fs) {
    // 1. Prüfen, solange nichts verändert ist: Größe, Header, Argumente, Heap. Gleiche
    // Prüfung wie canBoot, hier auf der ganz gelesenen Datei.
    struct stat st;
    if (stat(plan.dolPath.c_str(), &st) != 0) return false;
    size_t size = static_cast<size_t>(st.st_size);
    if (size == 0 || size > kMaxExecSize) return false;

    std::vector<std::string> args = buildArgs(plan);
    if (!argsFit(args)) return false;

    bool withArgv = false;
    {
        std::vector<uint8_t> data;
        if (!fs.readBytes(plan.dolPath, 0, size, data) || data.size() != size) return false;
        ExecReader r(data.data(), size);
        std::string reason;
        if (!checkExec(r, plan.dolPath, withArgv, reason)) return false;
        if (!heapBelowExec()) return false;

        // 2. Erst jetzt Konfigurationsdateien schreiben.
        for (const auto& file : plan.filesToWrite)
            if (!fs.writeText(file.first, file.second)) return false;

        std::memcpy(kExecAddr, data.data(), size);
    }
    DCFlushRange(kExecAddr, size);

    // 3. Ab hier kein Zurück mehr. Reihenfolge wie RunAppbooter in USB Loader GX:
    // IOS-Reload vor argv und Booter, damit nichts davon den Reload überleben muss.
    clearHbcStub();

    VIDEO_SetBlack(true);
    VIDEO_Flush();
    VIDEO_WaitVSync();

    WPAD_Shutdown();
    fs.unmountAll();

    if (IOS_GetVersion() != 58 && ios58Installed()) IOS_ReloadIOS(58);  // Fehler egal

    writeArgv(args, withArgv);

    std::memcpy(kBooterAddr, app_booter_bin, app_booter_bin_size);
    DCFlushRange(kBooterAddr, app_booter_bin_size);
    ICInvalidateRange(kBooterAddr, app_booter_bin_size);

    GX_AbortFrame();
    GX_Flush();

    SYS_ResetSystem(SYS_SHUTDOWN, 0, 0);
    u32 level;
    _CPU_ISR_Disable(level);
    // libogc 3.x hat kein __exception_closeall() mehr. Interrupts sind aus, crt0 des
    // Booters löscht MSR[EE] erneut, und das gestartete Programm setzt eigene Vektoren.
    reinterpret_cast<Entry>(kBooterAddr)();
    _CPU_ISR_Restore(level);
    return false;
}

void returnToWiiMenu() {
    *reinterpret_cast<vu32*>(0x8132FFFB) = kPriiloaderMagic;  // unausgerichtet, wie Priiloader
    *reinterpret_cast<vu32*>(0x817FEFF0) = kPriiloaderMagic;
    DCFlushRange(reinterpret_cast<void*>(0x8132FFFB), 4);
    DCFlushRange(reinterpret_cast<void*>(0x817FEFF0), 4);
    SYS_ResetSystem(SYS_RETURNTOMENU, 0, 0);
}

void launchHbc() {
    static const u64 kHbcTitles[] = {
        0x0001000148415858ULL,  // HAXX
        0x000100014A4F4449ULL,  // JODI
        0x000100014C554C5AULL,  // LULZ
        0x000100014F484243ULL,  // OHBC
    };
    WPAD_Shutdown();
    WII_Initialize();
    for (u64 id : kHbcTitles) WII_LaunchTitle(id);  // kehrt nur bei Fehler zurück
    returnToWiiMenu();
}

void powerOff() { SYS_ResetSystem(SYS_POWEROFF_STANDBY, 0, 0); }
