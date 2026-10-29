#!/bin/sh
# SD-Karte für Dolphin aus tools/dolphin-sd/ bauen.
#
# Dolphin hat dafür keinen Kommandozeilenbefehl. Ablauf in der Oberfläche:
#   1. Dolphin → Konfiguration → Wii → SD-Karte
#      (engl.: Config → Wii → SD Card Settings)
#   2. "SD-Karten-Synchronisationsordner" (SD Sync Folder) auf den unten ausgegebenen
#      Ordner setzen.
#   3. "Ordner in Datei konvertieren" (Convert Folder to File) drücken. Dolphin schreibt
#      daraus das SD-Image (Standard: <Dolphin-User>/Wii/sd.raw).
#   4. "SD-Karte einlegen" (Insert SD Card) aktivieren, launcher.dol starten
#      (Menü: make launcher; Grafik-Demo: make launcher LAUNCHER_GFX_DEMO=1).
# Alternativ "Ordner beim Start automatisch synchronisieren" aktivieren, dann
# konvertiert Dolphin bei jedem Emulationsstart neu.
#
# Inhalt:
#   config/images/2D/*.png     drei 2D-Cover (160x225) für den Texturcache
#   launcher/covers/wii/kaputt.png  10 Byte (PNG-Signatur + Müll): TexCache muss nullptr liefern
#   launcher/mock/...          Mini-Spielebaum (Dateien mit wenigen Bytes)
#   launcher/settings.txt      extra_root=sd:/launcher/mock, damit der Launcher ohne USB den
#                              Mock-Baum zeigt (für das Fehlerbild "USB-Platte nicht gefunden"
#                              die Zeile extra_root entfernen)
set -eu
here=$(cd "$(dirname "$0")" && pwd)
dir="$here/dolphin-sd"
[ -d "$dir" ] || { echo "fehlt: $dir" >&2; exit 1; }
echo "SD-Sync-Ordner für Dolphin:"
echo "  $dir"
echo "Dateien:"
(cd "$dir" && find . -type f ! -name .DS_Store | sort)
