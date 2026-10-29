#pragma once
// Header-Prüfung für DOL/ELF vor dem Start über den app_booter.
// Kopie der Prüfungen aus launcher/source/wii/boot.cpp (inspectElf, inspectDol,
// acceptsArgv), damit der Forwarder keinen Launcher-Code linkt.
#include <gctypes.h>
#include <stddef.h>

// Obergrenze für struct __argv + Befehlszeile an 0x93300800.
#define DOLCHECK_MAX_ARGS_SIZE 0x8000

typedef struct {
    bool elf;
    // Datei-Offset der Einsprungadresse (dort liegt bei libogc-Programmen "_arg" an +4).
    size_t entryOffset;
} ExecInfo;

// true, wenn die Datei ein ladbares ELF oder DOL ist.
bool dolcheck_inspect(const u8* d, size_t size, ExecInfo* info);

// true, wenn das Programm argv erwartet ("_arg" an Entry+4, kein DollZ).
bool dolcheck_accepts_argv(const u8* d, size_t size, const ExecInfo* info);
