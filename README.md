# Wii Launcher

Startbildschirm für die Wii: startet Wii-Spiele, GameCube-Spiele und Emulator-Titel von der USB-Platte. Der Code ist zweigeteilt: `launcher/source/core/` enthält plattformfreie C++17-Logik, die auf dem Rechner getestet wird, `launcher/source/wii/` die libogc/GRRLIB-Anbindung. Lizenz: GPL-2.0-or-later.

Bauen braucht devkitPro (devkitPPC, GRRLIB, libfat; siehe `docs/toolchain.md`). `make launcher` erzeugt `launcher/launcher.dol` (das Top-Level-Makefile setzt `DEVKITPRO`/`DEVKITPPC` selbst), `make test` baut und startet die Host-Tests mit dem System-C++-Compiler, `make clean` räumt auf.

## Bedienung

Das Hauptbild zeigt oben die Leiste (Tabs je System, Pille „Liste", Suche) und darunter einen Fächer aus Covern über alle Spiele des aktiven Tabs, alphabetisch. Der Launcher startet im Tab „Wii", „Alle" liegt links davon. Beim Start und nach jedem Spielstart steht der Fächer auf dem zuletzt gespielten Spiel.

- Fächer: Steuerkreuz links/rechts blättert um ein Spiel (gehalten immer schneller), 1/2 springen zehn Spiele zurück/vor, A startet das Mittelcover. Ein Klick auf ein Seitencover springt dorthin, ein Klick auf die Mitte startet.
- Liste: Steuerkreuz runter oder Klick auf „Liste" blendet sie von rechts ein (A-Z-Spalte und zehn Zeilen). Das Mittelcover zeigt immer das gewählte Spiel. In der Liste blättern 1/2 seitenweise, A startet; ein Klick wählt eine Zeile, ein zweiter Klick startet sie.
- Liste schließen: B, Klick auf „Liste", Klick daneben oder Steuerkreuz links in der A-Z-Spalte.
- Steuerkreuz hoch führt zu den Tabs, + öffnet „Systeme", - die Suche, Home das Home-Menü.

## Bauen

```bash
make            # launcher/launcher.dol, forwarder/forwarder.dol und booter
make test       # Host-Tests der Core-Logik
```

## Deploy

`tools/deploy-sd.sh` kopiert den Launcher nach `/Volumes/WII/apps/launcher/` (`boot.dol`, `meta.xml`, `icon.png`), den Forwarder nach `/Volumes/WII/forwarder.dol` und legt bei Bedarf `/Volumes/WII/launcher/settings.txt` und `dosbox.conf.tpl` an (vorhandene Dateien bleiben unberührt, `--force-tpl` überschreibt `dosbox.conf.tpl`). Ist die USB-Platte `/Volumes/WII-ROMS` gemountet, entsteht außerdem `dosbox/SimCity 2000/autoexec.txt`. `._*`-Dateien werden entfernt, die SD wird nicht ausgeworfen. Mit `--dry-run` wird nur angezeigt, was passieren würde. Am Ende warnt das Skript, wenn Startprogramme fehlen, die der Launcher aufruft (USB Loader GX, Not64, Snes9x GX, FCEUGX, Genesis Plus GX, ScummVM, DOSBox), bricht deswegen aber nicht ab.

Rückweg aus Spielen: Vor dem Priiloader-Update landet man nach einem Spiel im Wii-Menü, nicht im HBC (der Launcher löscht den HBC-Stub vor dem Start).

Priiloader-Autostart einrichten: `docs/priiloader.md`. Toolchain: `docs/toolchain.md`. Testergebnisse: `docs/testprotokoll.md`.

## Wii-Upload

```bash
tools/wiiload.sh <Wii-IP>   # sendet launcher/launcher.dol, HBC muss laufen
```

## Cover

`tools/covers-emu.py` lädt Boxart-Cover aus den Libretro-Thumbnails für N64, SNES, NES, Sega und Adventure (ScummVM/DOSBox) nach `/Volumes/WII/launcher/covers/<System>/<ROM-Name>.png` (Adventure: `covers/dos/<Spielordner>.png`). Cover werden auf höchstens 320x228 verkleinert (Seitenverhältnis bleibt). Titel werden wie im Launcher normalisiert und per Name bzw. Ähnlichkeit (>= 0,85) zugeordnet.

```bash
python3 -I tools/covers-emu.py --dry-run             # nur Trefferbericht
python3 -I tools/covers-emu.py --system n64 --limit 20
python3 -I tools/covers-emu.py --system dos --refresh --only "Monkey"   # vorhandene Cover neu laden, nur passende Namen
```

Cover-Größen: Wii/GameCube nutzen die 2D-Cover von USB Loader GX (160x224), Emulator-Cover liegen mit erhaltenem Seitenverhältnis bei höchstens 320x228; der Launcher passt alle unverzerrt ein (Vorschau bis Spaltenbreite x 224).

Ohne `GITHUB_TOKEN` gilt das niedrige anonyme API-Limit; bei Bedarf `export GITHUB_TOKEN=$(gh auth token)`. `--refresh` überschreibt vorhandene Cover, `--only "<Teilstring>"` beschränkt auf ROMs bzw. Spielordner mit diesem Namensteil. Bei Adventure-Ordnern (Kurznamen) trifft auch ein zusammenhängender Teil des Libretro-Namens; ScummVM-Ordner suchen zuerst im Repo ScummVM, dann in DOS, dosbox-Ordner nur in DOS.

Die Cover unter `tools/dolphin-sd/` stammen von GameTDB und sind nur Testmaterial für den privaten Gebrauch; vor einer Veröffentlichung des Repos müssen sie durch eigene Bilder ersetzt werden.

## RVZ nach ISO

`python3 -I tools/rvz2iso.py <in.rvz> <out.iso>` wandelt Dolphin-RVZ-Images von GameCube-Spielen ohne Fremd-Binaries in rohe ISOs um (Python 3.14, Kompression NONE oder zstd, Junk-Daten werden per LFG rekonstruiert).
`--verify-only` dekodiert nur und gibt SHA-1/CRC32 aus, die sich direkt mit der Redump-DAT vergleichen lassen; Wii-Images werden abgelehnt.
