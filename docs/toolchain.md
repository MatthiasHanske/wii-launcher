# Toolchain (macOS, Apple Silicon)

Stand 2026-10-29, eingerichtet auf dem Entwicklungs-Mac.

| Komponente | Version / Quelle | Ort |
|---|---|---|
| devkitPro-Pacman | v6.0.2, `devkitpro-pacman-installer.pkg` (github.com/devkitPro/pacman) | `/usr/local/bin/dkp-pacman` |
| devkitPPC | gcc 16.2.0 (Paketgruppe `wii-dev`) | `/opt/devkitpro/devkitPPC` |
| libogc, libfat-ogc, wiiuse | aus `wii-dev` / `libfat-ogc` | `/opt/devkitpro/libogc` |
| ppc-libpng, ppc-freetype, ppc-libjpeg-turbo, ppc-zlib | dkp-pacman | `/opt/devkitpro/portlibs/ppc` |
| GRRLIB + libpngu | Git master, Commit `a426e6170cbd9a4a69851a705637d753a09f8294`, `make -C GRRLIB clean all install` | `portlibs/wii` (grrlib), `portlibs/ppc` (pngu) |
| wiiload | aus devkitPro tools | `/opt/devkitpro/tools/bin/wiiload` |
| Dolphin | Homebrew-Cask `dolphin` | `/Applications/Dolphin.app` |

## Umgebung

```bash
export DEVKITPRO=/opt/devkitpro
export DEVKITPPC=/opt/devkitpro/devkitPPC
export PATH="$DEVKITPRO/tools/bin:$DEVKITPPC/bin:$PATH"
```

Pruefen: `bash tools/check-toolchain.sh`

## Neu aufsetzen

1. Pacman-Installer (pkg) ausfuehren, braucht Admin-Rechte.
2. `sudo dkp-pacman -S --needed wii-dev libfat-ogc ppc-libpng ppc-freetype ppc-libjpeg-turbo ppc-zlib`
3. `git clone https://github.com/GRRLIB/GRRLIB.git /tmp/GRRLIB && git -C /tmp/GRRLIB checkout a426e6170cbd9a4a69851a705637d753a09f8294 && sudo make -C /tmp/GRRLIB/GRRLIB clean all install` (voller Klon, damit der gepinnte Commit erreichbar ist)
4. `brew install --cask dolphin`

## Upload auf die Wii

Wii im Homebrew Channel lassen (zeigt unten die IP), dann:

```bash
export WIILOAD=tcp:<Wii-IP>
wiiload launcher/launcher.dol
```
