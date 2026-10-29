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
// Länge prüft bootDol vorher (kMaxArgsSize).
void writeArgv(const std::vector<std::string>& args) {
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

// libogc prüft beim exit() den HBC-Reload-Stub ("STUB" / "HAXX"). Ohne die Magic fällt
// exit() auf SYS_RETURNTOMENU zurück, also Wii-Menü statt HBC.
void clearHbcStub() {
    *reinterpret_cast<vu32*>(0x80001804) = 0;
    *reinterpret_cast<vu32*>(0x80001808) = 0;
    DCFlushRange(reinterpret_cast<void*>(0x80001800), 32);
}

}  // namespace

bool bootDol(const LaunchPlan& plan, WiiFs& fs) {
    for (const auto& file : plan.filesToWrite)
        if (!fs.writeText(file.first, file.second)) return false;

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

    {
        std::vector<uint8_t> data;
        if (!fs.readBytes(plan.dolPath, 0, size, data)) return false;
        // Der eigene Heap darf nicht in den Zielbereich gewachsen sein.
        if (reinterpret_cast<u32>(SYS_GetArena2Lo()) > reinterpret_cast<u32>(kExecAddr)) return false;
        std::memcpy(kExecAddr, data.data(), size);
    }
    DCFlushRange(kExecAddr, size);

    // Ab hier kein Zurück mehr.
    writeArgv(args);

    std::memcpy(kBooterAddr, app_booter_bin, app_booter_bin_size);
    DCFlushRange(kBooterAddr, app_booter_bin_size);
    ICInvalidateRange(kBooterAddr, app_booter_bin_size);

    clearHbcStub();

    VIDEO_SetBlack(true);
    VIDEO_Flush();
    VIDEO_WaitVSync();

    WPAD_Shutdown();
    fs.unmountAll();

    if (IOS_GetVersion() != 58 && ios58Installed()) IOS_ReloadIOS(58);  // Fehler egal

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
