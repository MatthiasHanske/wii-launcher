#!/usr/bin/env bash
# Kopiert Launcher und Forwarder auf die SD-Karte (/Volumes/WII).
# Aufruf: tools/deploy-sd.sh [--dry-run] [--force-tpl]
#   --force-tpl ueberschreibt eine vorhandene launcher/dosbox.conf.tpl
set -euo pipefail

SD="${WII_SD:-/Volumes/WII}"
USB="${WII_USB:-/Volumes/WII-ROMS}"
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
DRY=0
FORCE_TPL=0

for arg in "$@"; do
  case "$arg" in
    --dry-run) DRY=1 ;;
    --force-tpl) FORCE_TPL=1 ;;
    *) echo "Aufruf: $0 [--dry-run] [--force-tpl]" >&2; exit 1 ;;
  esac
done

if ! { [ -d "$SD" ] && df "$SD" >/dev/null 2>&1 && mount | grep -F " on ${SD} " >/dev/null; }; then
  if [ "$DRY" = 1 ]; then
    echo "Hinweis: $SD ist nicht gemountet (dry-run laeuft trotzdem)." >&2
  else
    echo "Abbruch: $SD ist nicht gemountet." >&2
    exit 1
  fi
fi

LAUNCHER_DOL="$ROOT/launcher/launcher.dol"
FORWARDER_DOL="$ROOT/forwarder/forwarder.dol"
META="$ROOT/launcher/meta.xml"
ICON="$ROOT/launcher/icon.png"

for f in "$LAUNCHER_DOL" "$FORWARDER_DOL" "$META" "$ICON"; do
  [ -f "$f" ] || { echo "Abbruch: fehlt $f (erst 'make')" >&2; exit 1; }
done

SUMMARY=()
run() { if [ "$DRY" = 1 ]; then echo "[dry-run] $*"; else "$@"; fi; }
note() { SUMMARY+=("$1"); }

APP="$SD/apps/launcher"
LDIR="$SD/launcher"

run mkdir -p "$APP" "$LDIR" "$LDIR/covers"
run cp "$LAUNCHER_DOL" "$APP/boot.dol";  note "kopiert  apps/launcher/boot.dol"
run cp "$META" "$APP/meta.xml";          note "kopiert  apps/launcher/meta.xml"
run cp "$ICON" "$APP/icon.png";          note "kopiert  apps/launcher/icon.png"
run cp "$FORWARDER_DOL" "$SD/forwarder.dol"; note "kopiert  forwarder.dol"

# settings.txt nur anlegen, wenn es fehlt
if [ -f "$LDIR/settings.txt" ]; then
  note "belassen launcher/settings.txt (vorhanden)"
elif [ "$DRY" = 1 ]; then
  echo "[dry-run] settings.txt mit Defaults anlegen"; note "angelegt launcher/settings.txt (dry-run)"
else
  printf 'systems=wii,gc,n64\nextra_root=\n' > "$LDIR/settings.txt"
  note "angelegt launcher/settings.txt (Defaults)"
fi

# dosbox.conf.tpl: Kopie der DOSBox-Wii-Konfiguration, sonst Minimalvorlage
SRC_CONF="$SD/apps/dosbox-wii/dosbox.conf"
if [ -f "$LDIR/dosbox.conf.tpl" ] && [ "$FORCE_TPL" = 0 ]; then
  note "belassen launcher/dosbox.conf.tpl (vorhanden, --force-tpl zum Ueberschreiben)"
elif [ -f "$SRC_CONF" ]; then
  run cp "$SRC_CONF" "$LDIR/dosbox.conf.tpl"; note "kopiert  launcher/dosbox.conf.tpl (aus dosbox-wii)"
elif [ "$DRY" = 1 ]; then
  echo "[dry-run] dosbox.conf.tpl aus Minimalvorlage"; note "angelegt launcher/dosbox.conf.tpl (Minimalvorlage, dry-run)"
else
  printf '[sdl]\nfullscreen=true\n\n[cpu]\ncore=dynamic\ncycles=auto\n' > "$LDIR/dosbox.conf.tpl"
  note "angelegt launcher/dosbox.conf.tpl (Minimalvorlage)"
fi

# SimCity 2000 Autoexec auf der USB-Platte
SC="$USB/dosbox/SimCity 2000"
if [ -d "$SC" ]; then
  if [ -f "$SC/autoexec.txt" ]; then
    note "belassen usb:/dosbox/SimCity 2000/autoexec.txt (vorhanden)"
  elif [ "$DRY" = 1 ]; then
    echo "[dry-run] autoexec.txt anlegen"; note "angelegt usb:/dosbox/SimCity 2000/autoexec.txt (dry-run)"
  else
    printf 'SC2000.EXE\n' > "$SC/autoexec.txt"
    note "angelegt usb:/dosbox/SimCity 2000/autoexec.txt"
  fi
else
  note "uebersprungen SimCity-2000-Autoexec ($SC nicht vorhanden)"
fi

# AppleDouble-Dateien entfernen (nur in den angefassten Ordnern)
CLEAN=("$APP" "$LDIR")
[ -d "$SC" ] && CLEAN+=("$SC")
N=0
for d in "${CLEAN[@]}"; do
  [ -d "$d" ] || continue
  while IFS= read -r -d '' f; do
    N=$((N+1))
    run rm -f "$f"
  done < <(find "$d" -name '._*' -type f -print0)
done
for f in "$SD"/._forwarder.dol; do
  if [ -e "$f" ]; then N=$((N+1)); run rm -f "$f"; fi
done
note "entfernt  $N AppleDouble-Datei(en)"

echo
if [ "$DRY" = 1 ]; then echo "Zusammenfassung (dry-run, nichts geschrieben):"; else echo "Zusammenfassung:"; fi
printf '  %s\n' "${SUMMARY[@]}"

# Zielprogramme aus launcher/source/core/launch.cpp: nur warnen, nicht abbrechen.
TARGETS=(
  apps/usbloader_gx/boot.dol
  apps/not64/boot.dol
  apps/snes9xgx/boot.dol
  apps/fceugx/boot.dol
  apps/genplus-gx/boot.dol
  apps/scummvm/boot.elf
  apps/dosbox-wii/boot.dol
)
MISSING=()
for t in "${TARGETS[@]}"; do
  [ -f "$SD/$t" ] || MISSING+=("$t")
done
if [ "${#MISSING[@]}" -gt 0 ]; then
  echo
  echo "Warnung: diese Startprogramme fehlen auf der SD (Spiele dieser Systeme starten nicht):"
  printf '  %s\n' "${MISSING[@]}"
fi
echo "Die SD wird nicht ausgeworfen."
