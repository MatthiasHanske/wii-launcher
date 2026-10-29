#!/bin/bash
# Prueft, ob die Wii-Toolchain auf diesem Mac vollstaendig ist.
export DEVKITPRO=${DEVKITPRO:-/opt/devkitpro}
export DEVKITPPC=${DEVKITPPC:-/opt/devkitpro/devkitPPC}
export PATH="$DEVKITPRO/tools/bin:$DEVKITPPC/bin:/usr/local/bin:$PATH"

ok=(); missing=()
command -v powerpc-eabi-gcc >/dev/null && ok+=(devkitPPC) || missing+=(devkitPPC)
command -v dkp-pacman >/dev/null && ok+=(dkp-pacman) || missing+=(dkp-pacman)
command -v wiiload >/dev/null && ok+=(wiiload) || missing+=(wiiload)
[ -f "$DEVKITPRO/portlibs/wii/include/grrlib.h" ] && [ -f "$DEVKITPRO/portlibs/wii/lib/libgrrlib.a" ] \
  && [ -f "$DEVKITPRO/portlibs/ppc/lib/libpngu.a" ] && ok+=(grrlib) || missing+=(grrlib)
[ -d /Applications/Dolphin.app ] && ok+=(dolphin) || missing+=(dolphin)

if [ ${#missing[@]} -gt 0 ]; then
  echo "FEHLT: ${missing[*]}"
  echo "Siehe docs/toolchain.md"
  exit 1
fi
echo "OK: $(IFS=', '; echo "${ok[*]}")"
