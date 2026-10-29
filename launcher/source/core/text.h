#pragma once
// UTF-8-Hilfen für die Textanzeige (plattformfrei).
#include <cstdint>
#include <cstddef>
#include <functional>
#include <map>
#include <string>
#include <tuple>
#include <vector>

// UTF-8 -> Codepoints. Ungültige Bytes (lose Folgebytes, abgeschnittene oder überlange
// Sequenzen, Surrogates, > U+10FFFF) werden zu je einem U+FFFD.
std::vector<uint32_t> utf8Decode(const std::string& utf8);

// Kürzt text an Codepoint-Grenzen, bis measure(ergebnis) <= maxWidth, und hängt dann "…"
// an (Leerzeichen davor entfallen). Passt text ganz, kommt er unverändert zurück; passt
// nicht einmal "…", kommt "" zurück. Binäre Suche über die Präfixlänge (measure muss mit
// der Länge monoton wachsen), also O(log n) Aufrufe von measure.
std::string ellipsize(const std::string& text, float maxWidth,
                      const std::function<float(const std::string&)>& measure);

// Merkt sich ellipsize-Ergebnisse je (Text, maxWidth, Schriftgröße). Läuft die Tabelle über
// maxEntries, wird sie geleert (einfach und für Listen mit wiederkehrenden Titeln genug).
class EllipsizeMemo {
public:
    explicit EllipsizeMemo(size_t maxEntries = 512) : maxEntries_(maxEntries) {}

    std::string get(const std::string& text, float maxWidth, int size,
                    const std::function<float(const std::string&)>& measure);
    size_t size() const { return memo_.size(); }
    void clear() { memo_.clear(); }

private:
    size_t maxEntries_;
    std::map<std::tuple<std::string, float, int>, std::string> memo_;
};
