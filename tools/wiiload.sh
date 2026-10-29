#!/usr/bin/env bash
# Schickt launcher/launcher.dol per wiiload an die Wii (HBC muss laufen).
set -euo pipefail

if [ $# -lt 1 ] || [ -z "${1:-}" ]; then
  echo "Aufruf: tools/wiiload.sh <Wii-IP>" >&2
  echo "Beispiel: tools/wiiload.sh 192.168.1.50" >&2
  exit 1
fi

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
DOL="$ROOT/launcher/launcher.dol"
WIILOAD_BIN="${WIILOAD_BIN:-/opt/devkitpro/tools/bin/wiiload}"

[ -f "$DOL" ] || { echo "Fehlt: $DOL (erst 'make launcher')" >&2; exit 1; }
[ -x "$WIILOAD_BIN" ] || { echo "wiiload nicht gefunden: $WIILOAD_BIN" >&2; exit 1; }

WIILOAD="tcp:$1" "$WIILOAD_BIN" "$DOL"
