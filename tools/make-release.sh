#!/usr/bin/env bash
# Baut den Launcher und packt dist/wii-launcher-<version>.zip (direkt auf die SD-Karte entpackbar).
# Aufruf: tools/make-release.sh [--skip-build]
#   --skip-build  vorhandene Artefakte (launcher.dol, forwarder.dol) verwenden, nichts bauen
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
SKIP=0
for arg in "$@"; do
  case "$arg" in
    --skip-build) SKIP=1 ;;
    *) echo "Aufruf: $0 [--skip-build]" >&2; exit 1 ;;
  esac
done

VERSION="$(sed -n 's:.*<version>\(.*\)</version>.*:\1:p' "$ROOT/launcher/meta.xml" | head -n 1)"
[ -n "$VERSION" ] || { echo "Abbruch: keine Version in launcher/meta.xml" >&2; exit 1; }

if [ "$SKIP" = 0 ]; then
  make -C "$ROOT"
fi

for f in launcher/launcher.dol forwarder/forwarder.dol launcher/meta.xml launcher/icon.png \
         LICENSE launcher/data/font-LICENSE.txt; do
  [ -f "$ROOT/$f" ] || { echo "Abbruch: fehlt $f (erst 'make')" >&2; exit 1; }
done

STAGE="$(mktemp -d)"
trap 'rm -rf "$STAGE"' EXIT

mkdir -p "$STAGE/apps/launcher" "$STAGE/wii-launcher"
cp "$ROOT/launcher/launcher.dol" "$STAGE/apps/launcher/boot.dol"
cp "$ROOT/launcher/meta.xml" "$STAGE/apps/launcher/meta.xml"
cp "$ROOT/launcher/icon.png" "$STAGE/apps/launcher/icon.png"
cp "$ROOT/forwarder/forwarder.dol" "$STAGE/forwarder.dol"
cp "$ROOT/LICENSE" "$STAGE/wii-launcher/LICENSE"
cp "$ROOT/launcher/data/font-LICENSE.txt" "$STAGE/wii-launcher/font-LICENSE.txt"

cat > "$STAGE/wii-launcher/LIESMICH.txt" <<TXT
Wii Launcher $VERSION
=====================

Startbildschirm (Homebrew) fuer die Nintendo Wii: startet Wii- und GameCube-Spiele
sowie Emulator-Titel von der USB-Platte.

Installation
------------
1. Den Inhalt dieses ZIP-Archivs in das Wurzelverzeichnis der SD-Karte entpacken.
   Es entstehen apps/launcher/, forwarder.dol und wii-launcher/.
2. SD-Karte in die Wii stecken, im Homebrew Channel "Launcher" starten.
3. Optional: Autostart ueber Priiloader. forwarder.dol ist der kleine Starter dafuer,
   die Schritte stehen in docs/priiloader.md im Quelltext-Repository.

Die Spiele, die Startprogramme (USB Loader GX, Not64, Snes9x GX, FCEUGX, Genesis Plus GX,
ScummVM, DOSBox) und die Cover bringt man selbst mit, siehe README im Repository.

Quelltext und Dokumentation: https://github.com/MatthiasHanske/wii-launcher

Lizenz
------
Der Launcher steht unter der GNU General Public License (Datei LICENSE im Repository:
GPL Version 2). Der enthaltene app_booter stammt aus WiiFlow Lite (GPL Version 2).
Die Schrift Inter steht unter der SIL Open Font License 1.1 (Datei font-LICENSE.txt).
Eingriffe an der Konsole erfolgen auf eigenes Risiko. Kein Bezug zu Nintendo.
TXT

find "$STAGE" \( -name '._*' -o -name '.DS_Store' \) -delete

mkdir -p "$ROOT/dist"
OUT="$ROOT/dist/wii-launcher-$VERSION.zip"
rm -f "$OUT"
(cd "$STAGE" && COPYFILE_DISABLE=1 zip -rqX "$OUT" apps forwarder.dol wii-launcher)
echo "Erzeugt: $OUT"
