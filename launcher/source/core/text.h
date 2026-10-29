#pragma once
// UTF-8-Hilfen für die Textanzeige (plattformfrei).
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

// UTF-8 -> Codepoints. Ungültige Bytes (lose Folgebytes, abgeschnittene oder überlange
// Sequenzen, Surrogates, > U+10FFFF) werden zu je einem U+FFFD.
std::vector<uint32_t> utf8Decode(const std::string& utf8);

// Kürzt text an Codepoint-Grenzen, bis measure(ergebnis) <= maxWidth, und hängt dann "…"
// an (Leerzeichen davor entfallen). Passt text ganz, kommt er unverändert zurück; passt
// nicht einmal "…", kommt "" zurück.
std::string ellipsize(const std::string& text, float maxWidth,
                      const std::function<float(const std::string&)>& measure);
