#pragma once
#include <string>

#include "core/launch.h"
#include "wii/fs_wii.h"

// Vorabprüfung ohne Schreibzugriff: Startprogramm bekannt, vorhanden (stat), Größe (max.
// 16 MB), Argumentlänge, DOL/ELF-Header wie in bootDol (liest nur Header, Programmheader und
// Entry+4..8, nicht die ganze Datei). Bei false steht in reason ein Anzeigetext (UTF-8),
// z. B. "Fehlt auf der SD: sd:/apps/not64/boot.dol". Der Aufrufer trägt das Spiel erst nach
// true in "Zuletzt gespielt" ein.
bool canBoot(const LaunchPlan& plan, WiiFs& fs, std::string& reason);

// Startet plan.dolPath (DOL oder ELF) über den app_booter (booter/, GPL-2.0, Dimok).
// Ablauf: Größe (max. 16 MB) und Header prüfen (ELF: ET_EXEC/EM_PPC; DOL: Entry,
// Sektionen innerhalb der Datei, BSS), erst dann plan.filesToWrite schreiben, Datei nach
// 0x92000000. Danach kein Zurück: HBC-Stub löschen (Programm kehrt beim Beenden ins
// Wii-Menü zurück), WPAD aus, Geräte aushängen, IOS58 immer neu laden falls installiert
// (wie HBC, WiiFlow, USB Loader GX: das Programm bekommt ein frisches IOS ohne offene
// Handles), SD kurz neu einhängen und Reload-Ergebnis, IOS-Stand und "bootDol: Sprung"
// loggen, wieder aushängen, struct __argv an 0x93300800 (argv[0] = dolPath, dann
// plan.args; DollZ: genullt), app_booter nach 0x93000000, GX abbrechen, Sprung.
// Kommt nur bei Fehlern vor dem Point of no Return zurück (reason = Anzeigetext wie bei
// canBoot); dann sind Mounts, WPAD und Video intakt, und Konfigurationsdateien sind nur bei
// einem Schreibfehler teilweise geschrieben. Schreibt jeden Schritt ins Diagnose-Log; letzte
// Zeile ist "bootDol: Sprung" (nach dem IOS-Reload, falls die SD sich neu einhängen ließ).
// Aufrufer muss Audio vorher stoppen (ASND/AESND), bootDol fasst Audio nicht an.
// Nimmt WiiFs statt IFileSystem, weil vor dem Sprung unmountAll() nötig ist.
bool bootDol(const LaunchPlan& plan, WiiFs& fs, std::string& reason);

// Logtext zum laufenden IOS: "IOS58 v6176, AHBPROT aus (0xFFFFFFFF, volle Hardware-Rechte)".
// AHBPROT ist HW_AHBPROT (0x0d800064); 0xFFFFFFFF heißt, der PPC hat vollen Hardwarezugriff.
std::string iosStateText();

// Wii-Menü laden. Setzt die Priiloader-Magic "Pune", damit Priiloader seinen Autoboot
// (z. B. Forwarder auf den Launcher) überspringt. Aufrufer hängt vorher Geräte aus.
void returnToWiiMenu();

// Homebrew Channel starten (HAXX, JODI, LULZ, OHBC der Reihe nach), sonst Wii-Menü.
// Aufrufer hängt vorher Geräte aus (WiiFs::unmountAll), wie bei returnToWiiMenu.
void launchHbc();

// Wii ausschalten (Standby). Aufrufer hängt vorher Geräte aus (WiiFs::unmountAll).
void powerOff();
