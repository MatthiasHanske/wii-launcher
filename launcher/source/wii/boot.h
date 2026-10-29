#pragma once
#include "core/launch.h"
#include "wii/fs_wii.h"

// Startet plan.dolPath (DOL oder ELF) über den app_booter (booter/, GPL-2.0, Dimok).
// Ablauf: Größe (max. 16 MB) und Header prüfen (ELF: ET_EXEC/EM_PPC; DOL: Entry,
// Sektionen innerhalb der Datei, BSS), erst dann plan.filesToWrite schreiben, Datei nach
// 0x92000000. Danach kein Zurück: HBC-Stub löschen (Programm kehrt beim Beenden ins
// Wii-Menü zurück), WPAD aus, Geräte aushängen, IOS58 laden falls vorhanden, struct
// __argv an 0x93300800 (argv[0] = dolPath, dann plan.args; DollZ: genullt), app_booter
// nach 0x93000000, GX abbrechen, Sprung.
// Kommt nur bei Fehlern vor dem Point of no Return zurück; dann sind Mounts, WPAD und
// Video intakt, und Konfigurationsdateien sind nur bei einem Schreibfehler teilweise
// geschrieben.
// Aufrufer muss Audio vorher stoppen (ASND/AESND), bootDol fasst Audio nicht an.
// Nimmt WiiFs statt IFileSystem, weil vor dem Sprung unmountAll() nötig ist.
bool bootDol(const LaunchPlan& plan, WiiFs& fs);

// Wii-Menü laden. Setzt die Priiloader-Magic "Pune", damit Priiloader seinen Autoboot
// (z. B. Forwarder auf den Launcher) überspringt. Aufrufer hängt vorher Geräte aus.
void returnToWiiMenu();

// Homebrew Channel starten (HAXX, JODI, LULZ, OHBC der Reihe nach), sonst Wii-Menü.
// Aufrufer hängt vorher Geräte aus (WiiFs::unmountAll), wie bei returnToWiiMenu.
void launchHbc();

// Wii ausschalten (Standby). Aufrufer hängt vorher Geräte aus (WiiFs::unmountAll).
void powerOff();
