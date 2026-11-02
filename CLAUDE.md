# Wii-Launcher: Hinweise für Claude Code

- Gesamtkontext (Konsole, SD, USB-Platte, Entscheidungen, offene Punkte): `docs/projektstand.md`. Zuerst lesen, nach Änderungen an Karte oder Platte fortschreiben.
- Spezifikationen und Pläne: `docs/superpowers/specs/`, `docs/superpowers/plans/`. Priiloader: `docs/priiloader.md`. Tests am Gerät: `docs/testprotokoll.md`.
- Toolchain: `export DEVKITPRO=/opt/devkitpro DEVKITPPC=/opt/devkitpro/devkitPPC PATH="/opt/devkitpro/tools/bin:/opt/devkitpro/devkitPPC/bin:$PATH"`.
  `make test` (Host-Tests) und `make` (launcher/launcher.dol) müssen vor jedem Merge grün sein. Deploy: `bash tools/deploy-sd.sh`, dann `diskutil eject /Volumes/WII`.
- Sprache: Code-Kommentare, Docs und Commits auf Deutsch, keine Gedankenstriche (—), keine Co-Authored-By-Zeilen, kein "Generated with"-Footer.
- Architektur: `core/` ist plattformfrei und host-testbar (ui_state, library, scanner, titles, launch, fit), `wii/` kapselt libogc/GRRLIB (gfx, screens, app, boot, fs_wii).
  Neue Logik zuerst in `core/` mit Test, dann in `wii/` anbinden.
- Änderungen an SD oder USB-Platte: Originale von Konfigdateien vorher sichern, nichts löschen, was nicht in `docs/projektstand.md` als entbehrlich steht; Doppelte nach `_doppelt/` verschieben statt löschen.
