# Priiloader-Autoboot auf den Launcher

Ziel: Nach dem Einschalten und nach jeder Rückkehr aus einem Programm startet die Wii direkt den Launcher statt des
Wii-Menüs. Priiloader bootet dazu per Autoboot den kleinen Forwarder (`forwarder/forwarder.dol`, unter 300 KB), der
seinerseits `sd:/apps/launcher/boot.dol` lädt. Der Launcher selbst liegt nicht im NAND und lässt sich weiter einfach
über die SD-Karte aktualisieren.

Das Deploy-Script (`tools/deploy-sd.sh`, Task 13) legt `forwarder.dol` im Wurzelverzeichnis der SD-Karte ab
(`sd:/forwarder.dol`) und den Launcher unter `sd:/apps/launcher/boot.dol`. Vor den Schritten unten also einmal
`make all` und das Deploy-Script ausführen.

## Verhalten des Forwarders

- SD einbinden (3 Versuche im Abstand von 500 ms), `sd:/apps/launcher/boot.dol` prüfen (DOL/ELF-Header, max. 16 MB)
  und über den app_booter starten (gleiches Verfahren wie im Launcher, argv[0] = `sd:/apps/launcher/boot.dol`).
- Bei jedem Fehler (keine SD, Datei fehlt, Datei kaputt) erscheint
  `Launcher nicht gefunden auf SD. Starte Homebrew Channel in 5 Sekunden.` mit dem Grund in der Zeile darunter.
  Nach 5 Sekunden startet der Homebrew Channel (Titel HAXX, JODI, LULZ, OHBC der Reihe nach).
- Ist auch kein HBC installiert, geht es ins Wii-Menü. Der Forwarder setzt vorher das Priiloader-Magic-Word
  (`0x50756e65`), damit Priiloader das echte Wii-Menü zeigt und nicht wieder den Forwarder startet (keine
  Boot-Schleife).

## Schritte am Fernseher

### 1. NAND-Backup mit BootMii

Priiloader schreibt in den NAND. Vorher ein vollständiges Backup anlegen.

1. SD-Karte mit mindestens 600 MB freiem Platz einlegen.
2. Homebrew Channel starten, Home-Taste, "Launch BootMii".
3. In BootMii mit der Power-Taste der Konsole zum Zahnrad-Symbol (Optionen) wechseln, mit Reset bestätigen.
4. Das Symbol "Backup" (grüner Pfeil von der Wii zur SD) wählen und mit Reset starten. Das dauert einige Minuten.
5. Danach liegen `nand.bin` und `keys.bin` im Wurzelverzeichnis der SD. Beide Dateien auf den Rechner kopieren und
   dort aufbewahren.
6. Zurück zum Hauptbildschirm von BootMii und den Homebrew Channel wählen.

### 2. Priiloader 0.10.0 installieren

Die installierte Version 0.8b1 (2012) wird ersetzt; erst ab 0.10.0 sind die Magic-Words dokumentiert und verlässlich.

1. Im Homebrew Channel "Priiloader" starten (`sd:/apps/priiloader`, Version 0.10.0).
2. Im Installer die Taste für Installieren/Update drücken (Plus bzw. A, steht auf dem Bildschirm), nicht "Remove".
3. Warten, bis "Done" erscheint, dann den Installer beenden. Die Wii startet neu.

### 3. Forwarder installieren und Autoboot setzen

1. Wii ausschalten. Reset gedrückt halten und dabei die Wii einschalten, bis das Priiloader-Menü erscheint.
2. "Load/Install File" wählen und `sd:/forwarder.dol` auswählen, Installation bestätigen.
   (Liegt die Datei nicht im Wurzelverzeichnis: Deploy-Script erneut ausführen.)
3. Zurück ins Hauptmenü, "Settings" öffnen.
4. "Autoboot" auf "Installed File" stellen und speichern ("Save settings").
5. Zurück ins Hauptmenü und "Installed File" einmal von Hand starten: Der Launcher muss erscheinen.

## Rückweg prüfen

Nach der Einrichtung diese Fälle einmal durchgehen und im Testprotokoll (`docs/testprotokoll.md`) festhalten:

| Fall | Erwartung |
|---|---|
| Wii einschalten | Forwarder (kurz), danach der Launcher |
| Spiel/Emulator aus dem Launcher starten und dort beenden | Rückkehr in den Launcher (über Systemmenü-Neustart und Priiloader, ca. 5 bis 8 s) |
| Im Launcher Home, "Wii-Menü" | Echtes Wii-Menü, kein erneuter Autoboot |
| Reset gedrückt halten beim Einschalten | Priiloader-Menü |
| SD-Karte entfernt, Wii einschalten | Hinweis "Launcher nicht gefunden auf SD ...", nach 5 s Homebrew Channel |

## Autoboot zurücknehmen

1. Wii ausschalten, Reset gedrückt halten und einschalten, bis das Priiloader-Menü erscheint.
2. "Settings", "Autoboot" auf "System Menu" stellen und speichern.
3. Ab dann startet die Wii wieder normal ins Wii-Menü. Der installierte Forwarder bleibt in Priiloader liegen und
   stört nicht; er wird nur über "Installed File" von Hand gestartet.

Priiloader komplett entfernen: Priiloader-Installer aus dem HBC starten und "Remove" wählen. Im Notfall (Wii startet
nicht mehr ins Menü) das NAND-Backup aus Schritt 1 mit BootMii ("Restore") zurückspielen.
