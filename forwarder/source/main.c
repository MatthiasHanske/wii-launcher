// Priiloader-Forwarder: startet sd:/apps/launcher/boot.dol über den app_booter (gleiches
// Verfahren wie launcher/source/wii/boot.cpp). Fällt bei jedem Fehler nach 5 Sekunden auf
// den Homebrew Channel zurück, danach aufs Wii-Menü (mit Priiloader-Magic, sonst würde
// Priiloader den Forwarder sofort wieder starten).
//
// Nur libogc + libfat, Ausgabe über die libogc-Konsole (fputs; printf aus
// libogc läuft über source/intprintf.c, damit das DOL unter 300 KB bleibt). Konsolenschrift ist CP437, daher
// stehen in den Meldungen keine Umlaute.
#include <gccore.h>
#include <ogc/es.h>
#include <ogc/ios.h>
#include <ogc/machine/processor.h>
#include <ogc/system.h>
#include <ogc/wiilaunch.h>
#include <sdcard/wiisd_io.h>
#include <fat.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "app_booter_bin.h"
#include "dolcheck.h"

#define LAUNCHER_PATH "sd:/apps/launcher/boot.dol"

// Adressen passend zu booter/source/main.c und booter/source/link.ld.
#define EXEC_ADDR ((u8*)0x92000000)
#define BOOTER_ADDR ((u8*)0x93000000)
#define ARGS_ADDR ((u8*)0x93300800)
#define MAX_EXEC_SIZE 0x01000000  // 0x92000000..0x93000000

#define PRIILOADER_MAGIC 0x50756e65  // "Pune": System Menu, kein Autoboot
#define IOS58 0x000000010000003AULL

typedef void (*Entry)(void);

static bool sdMounted = false;

static void initConsole(void) {
    VIDEO_Init();
    GXRModeObj* rmode = VIDEO_GetPreferredMode(NULL);
    void* xfb = MEM_K0_TO_K1(SYS_AllocateFramebuffer(rmode));
    CON_Init(xfb, 20, 20, rmode->fbWidth, rmode->xfbHeight, rmode->fbWidth * VI_DISPLAY_PIX_SZ);
    VIDEO_Configure(rmode);
    VIDEO_SetNextFramebuffer(xfb);
    VIDEO_SetBlack(false);
    VIDEO_Flush();
    VIDEO_WaitVSync();
    if (rmode->viTVMode & VI_NON_INTERLACE) VIDEO_WaitVSync();
    fputs("\n\n   Wii Launcher Forwarder\n\n", stdout);
}

static bool mountSd(void) {
    for (int attempt = 0; attempt < 3; ++attempt) {
        if (attempt > 0) usleep(500 * 1000);
        if (fatMountSimple("sd", &__io_wiisd)) {
            sdMounted = true;
            return true;
        }
    }
    return false;
}

static void unmountSd(void) {
    if (!sdMounted) return;
    fatUnmount("sd:");
    __io_wiisd.shutdown();
    sdMounted = false;
}

static bool ios58Installed(void) {
    u32 size = 0;
    return ES_GetStoredTMDSize(IOS58, &size) >= 0 && size > 0;
}

// Wie writeArgv in boot.cpp, nur mit argv[0] = LAUNCHER_PATH. Ohne "_arg"-Slot oder bei
// DollZ eine genullte Struktur (argvMagic = 0), dann kopiert der Booter nichts.
static void writeArgv(bool withArgv) {
    memset(ARGS_ADDR, 0, sizeof(struct __argv));
    if (!withArgv) {
        DCFlushRange(ARGS_ADDR, sizeof(struct __argv));
        return;
    }
    const size_t length = sizeof(LAUNCHER_PATH) + 1;  // Pfad + NUL, abschließendes NUL
    struct __argv* argv = (struct __argv*)ARGS_ADDR;
    char* cmd = (char*)(ARGS_ADDR + sizeof(struct __argv));
    memcpy(cmd, LAUNCHER_PATH, sizeof(LAUNCHER_PATH));
    cmd[length - 1] = '\0';

    argv->argvMagic = ARGV_MAGIC;
    argv->commandLine = cmd;
    argv->length = (int)length;
    argv->argc = 1;
    argv->argv = &argv->commandLine;
    argv->endARGV = argv->argv + 1;
    DCFlushRange(ARGS_ADDR, sizeof(struct __argv) + length);
}

// libogc prüft beim exit() den HBC-Reload-Stub; ohne Magic geht exit() ins Wii-Menü.
static void clearHbcStub(void) {
    *(vu32*)0x80001804 = 0;
    *(vu32*)0x80001808 = 0;
    DCFlushRange((void*)0x80001800, 32);
}

// Lädt und startet den Launcher. Kehrt nur vor dem Point of no Return zurück, dann mit
// einer Fehlerbeschreibung in *reason.
static void bootLauncher(const char** reason) {
    if (!mountSd()) {
        *reason = "Grund: SD-Karte konnte nicht eingebunden werden.";
        return;
    }
    struct stat st;
    if (stat(LAUNCHER_PATH, &st) != 0) {
        *reason = "Grund: " LAUNCHER_PATH " fehlt.";
        return;
    }
    size_t size = (size_t)st.st_size;
    if (size == 0 || size > MAX_EXEC_SIZE) {
        *reason = "Grund: boot.dol ist leer oder groesser als 16 MB.";
        return;
    }

    u8* data = (u8*)malloc(size);
    if (!data) {
        *reason = "Grund: nicht genug Speicher fuer boot.dol.";
        return;
    }
    FILE* f = fopen(LAUNCHER_PATH, "rb");
    size_t got = 0;
    if (f) {
        while (got < size) {
            size_t n = fread(data + got, 1, size - got, f);
            if (n == 0) break;
            got += n;
        }
        fclose(f);
    }
    if (got != size) {
        free(data);
        *reason = "Grund: boot.dol konnte nicht gelesen werden.";
        return;
    }

    ExecInfo info;
    if (!dolcheck_inspect(data, size, &info)) {
        free(data);
        *reason = "Grund: boot.dol ist kein gueltiges DOL/ELF.";
        return;
    }
    bool withArgv = dolcheck_accepts_argv(data, size, &info);
    // Der eigene Heap darf nicht in den Zielbereich gewachsen sein.
    if ((u32)SYS_GetArena2Lo() > (u32)EXEC_ADDR) {
        free(data);
        *reason = "Grund: Heap reicht in den Ladebereich.";
        return;
    }
    memcpy(EXEC_ADDR, data, size);
    free(data);
    DCFlushRange(EXEC_ADDR, size);

    // Ab hier kein Zurück. Reihenfolge wie bootDol im Launcher. WPAD ist nie
    // initialisiert, GX ebenfalls nicht (nur Konsole auf dem XFB), daher kein
    // WPAD_Shutdown und kein GX_AbortFrame.
    clearHbcStub();

    VIDEO_SetBlack(true);
    VIDEO_Flush();
    VIDEO_WaitVSync();

    unmountSd();

    if (IOS_GetVersion() != 58 && ios58Installed()) IOS_ReloadIOS(58);  // Fehler egal

    writeArgv(withArgv);

    memcpy(BOOTER_ADDR, app_booter_bin, app_booter_bin_size);
    DCFlushRange(BOOTER_ADDR, app_booter_bin_size);
    ICInvalidateRange(BOOTER_ADDR, app_booter_bin_size);

    SYS_ResetSystem(SYS_SHUTDOWN, 0, 0);
    u32 level;
    _CPU_ISR_Disable(level);
    ((Entry)BOOTER_ADDR)();
    _CPU_ISR_Restore(level);
    *reason = "Grund: Sprung in den Booter ist zurueckgekehrt.";
}

// Wii-Menü mit Priiloader-Magic, damit Priiloader den Autoboot überspringt
// (sonst Boot-Schleife Forwarder -> Fehler -> Forwarder).
static void returnToWiiMenu(void) {
    *(vu32*)0x8132FFFB = PRIILOADER_MAGIC;  // unausgerichtet, wie Priiloader
    *(vu32*)0x817FEFF0 = PRIILOADER_MAGIC;
    DCFlushRange((void*)0x8132FFFB, 4);
    DCFlushRange((void*)0x817FEFF0, 4);
    SYS_ResetSystem(SYS_RETURNTOMENU, 0, 0);
}

static void launchHbc(void) {
    static const u64 kHbcTitles[] = {
        0x0001000148415858ULL,  // HAXX
        0x000100014A4F4449ULL,  // JODI
        0x000100014C554C5AULL,  // LULZ
        0x000100014F484243ULL,  // OHBC
    };
    WII_Initialize();
    for (size_t i = 0; i < sizeof(kHbcTitles) / sizeof(kHbcTitles[0]); ++i)
        WII_LaunchTitle(kHbcTitles[i]);  // kehrt nur bei Fehler zurück
    returnToWiiMenu();
}

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;
    initConsole();
    fputs("   Starte " LAUNCHER_PATH " ...\n", stdout);

    const char* reason = "Grund: unbekannt.";
    bootLauncher(&reason);

    // Nur bei Fehler erreicht.
    unmountSd();
    VIDEO_SetBlack(false);
    VIDEO_Flush();
    fputs("\n   Launcher nicht gefunden auf SD. Starte Homebrew Channel in 5 Sekunden.\n", stdout);
    fputs("   ", stdout);
    fputs(reason, stdout);
    fputs("\n", stdout);
    fflush(stdout);
    sleep(5);

    launchHbc();
    return 0;
}
