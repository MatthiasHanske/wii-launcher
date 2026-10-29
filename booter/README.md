# app_booter

Kleiner Lader, den der Launcher nach `0x93000000` kopiert und anspringt. Er lädt das
Programm aus `0x92000000` (DOL oder ELF), übernimmt `struct __argv` von `0x93300800`
an Einsprungadresse + 8 und springt hinein.

- Herkunft: `resources/app_booter` aus WiiFlow Lite, https://github.com/Fledge68/WiiFlow_Lite
- Autor: Dimok (2011); `crt0.s`, `sync.c`, `link.ld` von Segher Boessenkool (2008-2009)
- Lizenz: GNU GPL, Version 2 (siehe `readme.txt`, `WIIFLOW_LICENSE`,
  http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt)

Änderungen gegenüber dem Original:

- `source/link.ld`: Ladeadresse `0x93000000` statt `0x93300000`, damit Code, BSS und Stack
  nicht in die Argumente bei `0x93300800` reichen.
- `Makefile`: Plattform-Defines `-DGEKKO -DHW_RVL` (libogc 3.x verlangt sie), PORTLIBS-Pfad
  und leeres `data`-Verzeichnis entfernt, `install` kopiert nach `../launcher/data/`.

Bauen: `make booter` im Repo-Wurzelverzeichnis erzeugt `booter/app_booter.bin`;
`make launcher` kopiert sie nach `launcher/data/` (bin2o -> `app_booter_bin.h`).
