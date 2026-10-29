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
// Länge prüft bootDol vorher (kMaxArgsSize). DollZ-komprimierte DOLs bekommen wie in
// USB Loader GX eine genullte Struktur (argvMagic = 0), sonst überschreibt der Booter
// Entry+8, wo bei DollZ Code liegt.
void writeArgv(const std::vector<std::string>& args, bool dollz) {
    if (dollz) {
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

bool isElf(const u8* d, size_t size) { return size >= 4 && std::memcmp(d, "\x7f" "ELF", 4) == 0; }

// ELF: ausführbar (ET_EXEC) und PowerPC (EM_PPC).
bool validElf(const u8* d, size_t size) {
    return size >= 52 && be16(d + 16) == 2 && be16(d + 18) == 20;
}

// DOL-Header (big endian): 7 Text- + 11 Datensektionen, Offsets 0x00, Adressen 0x48,
// Größen 0x90; BSS 0xD8/0xDC; Entry 0xE0. Sektionen müssen in der Datei liegen.
bool validDol(const u8* d, size_t size) {
    if (size < 0x100) return false;
    u32 entry = be32(d + 0xE0);
    if (!inMem1(entry) && !inMem2(entry)) return false;
    for (int i = 0; i < 18; ++i) {
        u32 off = be32(d + 0x00 + i * 4);
        u32 len = be32(d + 0x90 + i * 4);
        if (len == 0) continue;
        if (off < 0x100 || off > size || len > size - off) return false;
    }
    u32 bss = be32(d + 0xD8);
    u32 bssLen = be32(d + 0xDC);
    if (bssLen > 0) {
        // BSS in MEM1 (MEM2 nur bei MEM2-DOLs), Ende einschließlich.
        u32 last = bss + bssLen - 1;
        bool mem1 = inMem1(bss) && inMem1(last) && last >= bss;
        bool mem2 = inMem2(entry) && inMem2(bss) && inMem2(last) && last >= bss;
        if (!mem1 && !mem2) return false;
    }
    return true;
}

// libogc prüft beim exit() den HBC-Reload-Stub ("STUB" / "HAXX"). Ohne die Magic fällt
// exit() auf SYS_RETURNTOMENU zurück, also Wii-Menü statt HBC.
void clearHbcStub() {
    *reinterpret_cast<vu32*>(0x80001804) = 0;
    *reinterpret_cast<vu32*>(0x80001808) = 0;
    DCFlushRange(reinterpret_cast<void*>(0x80001800), 32);
}

}  // namespace

bool bootDol(const LaunchPlan& plan, WiiFs& fs) {
    // 1. Prüfen, solange nichts verändert ist: Größe, Header, Argumente, Heap.
    struct stat st;
    if (stat(plan.dolPath.c_str(), &st) != 0) return false;
    size_t size = static_cast<size_t>(st.st_size);
    if (size == 0 || size > kMaxExecSize) return false;

    std::vector<std::string> args;
    args.reserve(plan.args.size() + 1);
    args.push_back(plan.dolPath);
    args.insert(args.end(), plan.args.begin(), plan.args.end());
    size_t argsLength = sizeof(struct __argv) + 1;
    for (const std::string& a : args) argsLength += a.size() + 1;
    if (argsLength > kMaxArgsSize) return false;

    bool dollz = false;
    {
        std::vector<uint8_t> data;
        if (!fs.readBytes(plan.dolPath, 0, size, data)) return false;
        bool elf = isElf(data.data(), size);
        if (elf) {
            if (!validElf(data.data(), size)) return false;
        } else if (!validDol(data.data(), size)) {
            return false;
        }
        dollz = !elf && data[0x100] == 0x3C;
        // Der eigene Heap darf nicht in den Zielbereich gewachsen sein.
        if (reinterpret_cast<u32>(SYS_GetArena2Lo()) > reinterpret_cast<u32>(kExecAddr)) return false;

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

    writeArgv(args, dollz);

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
    WII_Initialize();
    for (u64 id : kHbcTitles) WII_LaunchTitle(id);  // kehrt nur bei Fehler zurück
    returnToWiiMenu();
}

void powerOff() { SYS_ResetSystem(SYS_POWEROFF_STANDBY, 0, 0); }
