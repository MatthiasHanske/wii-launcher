#pragma once
#include "core/launch.h"
#include "wii/fs_wii.h"

// Startet plan.dolPath (DOL oder ELF) über den app_booter (booter/, GPL-2.0, Dimok).
// Ablauf: plan.filesToWrite schreiben, Datei nach 0x92000000 laden, struct __argv an
// 0x93300800 (argv[0] = dolPath, dann plan.args), app_booter nach 0x93000000, HBC-Stub
// löschen (Programm kehrt beim Beenden ins Wii-Menü zurück), Geräte aushängen, IOS58
// laden falls vorhanden, Sprung. Kommt nur bei Fehlern vor dem Point of no Return
// zurück (Datei fehlt, leer, > 16 MB, Schreibfehler); dann sind Mounts und WPAD intakt.
// Nimmt WiiFs statt IFileSystem, weil vor dem Sprung unmountAll() nötig ist.
bool bootDol(const LaunchPlan& plan, WiiFs& fs);

// Wii-Menü laden. Setzt die Priiloader-Magic "Pune", damit Priiloader seinen Autoboot
// (z. B. Forwarder auf den Launcher) überspringt. Aufrufer hängt vorher Geräte aus.
void returnToWiiMenu();

// Homebrew Channel starten (HAXX, JODI, LULZ, OHBC der Reihe nach), sonst Wii-Menü.
void launchHbc();

// Wii ausschalten (Standby).
void powerOff();
