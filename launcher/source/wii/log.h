#pragma once

// Diagnose-Log auf der SD: sd:/launcher/launcher.log (Anhängen, je Zeile öffnen, schreiben,
// schließen, damit nach einem Absturz oder Sprung alles auf der Karte steht). Zeilen vor
// logInit werden gepuffert und beim Öffnen geschrieben; nach logClose wird nichts mehr
// geschrieben (Geräte sind danach ausgehängt).
// Im Namensraum, weil ::logf die Float-Funktion aus <math.h> ist.
namespace diag {

// Nach dem Einhängen der SD aufrufen. Datei über 200 KB wird vorher geleert. Schreibt eine
// Kopfzeile mit Uhrzeit und Build-ID (git short hash, -DBUILD_ID).
void logInit();
// Eine Zeile (ohne abschließendes \n), printf-Format. Jede Zeile beginnt mit den
// Millisekunden seit Programmstart.
void logf(const char* fmt, ...) __attribute__((format(printf, 1, 2)));
// Ab jetzt nichts mehr schreiben (vor WiiFs::unmountAll).
void logClose();

}  // namespace diag
