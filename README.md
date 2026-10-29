# Wii Launcher

Startbildschirm für die Wii: startet Wii-Spiele, GameCube-Spiele und Emulator-Titel von der USB-Platte. Der Code ist zweigeteilt: `launcher/source/core/` enthält plattformfreie C++17-Logik, die auf dem Rechner getestet wird, `launcher/source/wii/` die libogc/GRRLIB-Anbindung. Lizenz: GPL-2.0-or-later.

Bauen braucht devkitPro (devkitPPC, GRRLIB, libfat; siehe `docs/toolchain.md`). `make launcher` erzeugt `launcher/launcher.dol` (das Top-Level-Makefile setzt `DEVKITPRO`/`DEVKITPPC` selbst), `make test` baut und startet die Host-Tests mit dem System-C++-Compiler, `make clean` räumt auf.
