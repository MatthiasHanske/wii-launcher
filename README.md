# Wii Launcher

Startbildschirm für die Wii: startet Wii-Spiele, GameCube-Spiele und Emulator-Titel von der USB-Platte. Der Code ist zweigeteilt: `launcher/source/core/` enthält plattformfreie C++17-Logik, die auf dem Rechner getestet wird, `launcher/source/wii/` die libogc/GRRLIB-Anbindung. Lizenz: GPL-2.0-or-later.

Bauen braucht devkitPro (devkitPPC, GRRLIB, libfat; siehe `docs/toolchain.md`). `make launcher` erzeugt `launcher/launcher.dol` (das Top-Level-Makefile setzt `DEVKITPRO`/`DEVKITPPC` selbst), `make test` baut und startet die Host-Tests mit dem System-C++-Compiler, `make clean` räumt auf.

## Bauen

```bash
make            # launcher/launcher.dol und forwarder/forwarder.dol
make test       # Host-Tests der Core-Logik
```

## Deploy

`tools/deploy-sd.sh` kopiert den Launcher nach `/Volumes/WII/apps/launcher/` (`boot.dol`, `meta.xml`, `icon.png`), den Forwarder nach `/Volumes/WII/forwarder.dol` und legt bei Bedarf `/Volumes/WII/launcher/settings.txt` und `dosbox.conf.tpl` an (vorhandene Einstellungen bleiben unberührt). Ist `/Volumes/WII-ROMS` gemountet, entsteht außerdem `dosbox/SimCity 2000/autoexec.txt`. `._*`-Dateien werden entfernt, die SD wird nicht ausgeworfen. Mit `--dry-run` wird nur angezeigt, was passieren würde.

Priiloader-Autostart einrichten: `docs/priiloader.md`. Toolchain: `docs/toolchain.md`. Testergebnisse: `docs/testprotokoll.md`.

## Wii-Upload

```bash
tools/wiiload.sh <Wii-IP>   # sendet launcher/launcher.dol, HBC muss laufen
```

## Cover

`tools/covers-emu.py` lädt Boxart-Cover aus den Libretro-Thumbnails für N64, SNES, NES und Sega nach `/Volumes/WII/launcher/covers/<System>/<ROM-Name>.png`. Titel werden wie im Launcher normalisiert und per Name bzw. Ähnlichkeit (>= 0,85) zugeordnet.

```bash
python3 -I tools/covers-emu.py --dry-run             # nur Trefferbericht
python3 -I tools/covers-emu.py --system n64 --limit 20
```

Ohne `GITHUB_TOKEN` gilt das niedrige anonyme API-Limit; bei Bedarf `export GITHUB_TOKEN=$(gh auth token)`. ScummVM- und DOS-Spiele bekommen kein Cover.
