#include "core/titles.h"

#include <algorithm>
#include <cstddef>
#include <vector>

namespace {

std::string lower(std::string s) {
    for (char& c : s)
        if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
    return s;
}

std::string trim(const std::string& s) {
    size_t b = 0, e = s.size();
    while (b < e && (s[b] == ' ' || s[b] == '\t')) ++b;
    while (e > b && (s[e - 1] == ' ' || s[e - 1] == '\t')) --e;
    return s.substr(b, e - b);
}

std::string collapseSpaces(const std::string& s) {
    std::string out;
    bool space = false;
    for (char c : s) {
        if (c == ' ' || c == '\t') { space = true; continue; }
        if (space && !out.empty()) out += ' ';
        space = false;
        out += c;
    }
    return out;
}

bool startsWith(const std::string& s, const char* p) {
    return s.compare(0, std::char_traits<char>::length(p), p) == 0;
}

bool isDigit(char c) { return c >= '0' && c <= '9'; }

bool inList(const std::string& s, const char* const* list, size_t n) {
    for (size_t i = 0; i < n; ++i)
        if (s == list[i]) return true;
    return false;
}

const char* const kRegionWords[] = {
    "europe", "germany", "usa", "japan", "world", "australia", "scandinavia",
    "france", "spain", "italy", "netherlands", "uk", "korea", "brazil",
};
const char* const kCodes[] = {"u", "e", "j", "w", "ue", "ju", "jue", "g", "f", "i", "s", "pal", "ntsc"};
const char* const kFlags[] = {"digital", "aftermarket", "pirate", "unl", "!"};

const char* const kLangCodes[] = {"en", "fr", "de", "es", "it", "nl", "pt", "sv", "da", "no",
                                  "fi", "ja", "ko", "zh", "ru", "pl", "el", "tr", "cs", "hu"};

// Inhalt einer runden Klammer (ohne Klammern) ist ein Region-/Sprach-/Versions-Tag.
bool isTagContent(const std::string& raw) {
    std::string s = lower(trim(raw));
    if (s.empty()) return false;
    if (inList(s, kCodes, sizeof kCodes / sizeof *kCodes)) return true;
    if (inList(s, kFlags, sizeof kFlags / sizeof *kFlags)) return true;
    if (startsWith(s, "rev ") || startsWith(s, "rev-") || s == "rev") return true;
    if (s.size() >= 2 && (s[0] == 'v' || s[0] == 'm') && isDigit(s[1])) return true;

    // Kommagetrennte Liste: alles Regionen oder alles Sprachcodes.
    std::vector<std::string> parts;
    size_t start = 0;
    for (;;) {
        size_t c = s.find(',', start);
        parts.push_back(trim(s.substr(start, c == std::string::npos ? c : c - start)));
        if (c == std::string::npos) break;
        start = c + 1;
    }
    bool allRegion = true, allLang = true;
    for (const auto& p : parts) {
        if (!inList(p, kRegionWords, sizeof kRegionWords / sizeof *kRegionWords)) allRegion = false;
        if (!inList(p, kLangCodes, sizeof kLangCodes / sizeof *kLangCodes)) allLang = false;
    }
    return allRegion || (allLang && parts.size() >= 2);
}

// Entfernt alle [...] und erkannte (...) Gruppen.
std::string stripGroups(const std::string& in) {
    std::string out;
    size_t i = 0;
    while (i < in.size()) {
        char c = in[i];
        if (c == '[' || c == '(') {
            char close = c == '[' ? ']' : ')';
            size_t e = in.find(close, i + 1);
            if (e != std::string::npos) {
                if (c == '[' || isTagContent(in.substr(i + 1, e - i - 1))) {
                    i = e + 1;
                    out += ' ';
                    continue;
                }
            }
        }
        out += c;
        ++i;
    }
    return out;
}

const char* const kArticles[] = {"The", "Die", "Der", "Das"};

// "Foo, The - Bar" -> "The Foo - Bar"
std::string moveArticle(const std::string& s) {
    for (const char* art : kArticles) {
        std::string needle = std::string(", ") + art;
        size_t pos = 0;
        while ((pos = s.find(needle, pos)) != std::string::npos) {
            size_t after = pos + needle.size();
            if (after == s.size() || s.compare(after, 3, " - ") == 0)
                return std::string(art) + " " + s.substr(0, pos) + s.substr(after);
            pos = after;
        }
    }
    return s;
}

// Zweites Byte einer UTF-8-Sequenz C3 xx (U+00C0..U+00FF) auf ASCII falten, nullptr = unbekannt.
const char* foldC3(unsigned char d) {
    switch (d) {
        case 0x80: case 0x81: case 0x82: case 0x83: case 0x84: case 0x85:  // A-Varianten
        case 0xA0: case 0xA1: case 0xA2: case 0xA3: case 0xA4: case 0xA5: return "a";
        case 0x87: case 0xA7: return "c";
        case 0x88: case 0x89: case 0x8A: case 0x8B:
        case 0xA8: case 0xA9: case 0xAA: case 0xAB: return "e";
        case 0x8C: case 0x8D: case 0x8E: case 0x8F:
        case 0xAC: case 0xAD: case 0xAE: case 0xAF: return "i";
        case 0x91: case 0xB1: return "n";
        case 0x92: case 0x93: case 0x94: case 0x95: case 0x96:
        case 0xB2: case 0xB3: case 0xB4: case 0xB5: case 0xB6: return "o";
        case 0x99: case 0x9A: case 0x9B: case 0x9C:
        case 0xB9: case 0xBA: case 0xBB: case 0xBC: return "u";
        case 0x9F: return "ss";
        default: return nullptr;
    }
}

// Laenge einer UTF-8-Sequenz anhand des ersten Bytes (kaputte Bytes zaehlen als 1).
size_t utf8Length(unsigned char c) {
    if (c >= 0xF0 && c < 0xF8) return 4;
    if (c >= 0xE0) return 3;
    if (c >= 0xC0) return 2;
    return 1;
}

// Gemeinsamer Kern von sortKey und searchKey: klein, gefaltet, nur a-z0-9; alles andere wird
// zu ' ' (separator) oder entfaellt (Apostrophe ' ` und U+2018/U+2019).
std::string foldAscii(const std::string& s) {
    std::string out;
    for (size_t i = 0; i < s.size();) {
        unsigned char c = static_cast<unsigned char>(s[i]);
        if (c < 0x80) {
            ++i;
            if (c >= 'A' && c <= 'Z') out += static_cast<char>(c - 'A' + 'a');
            else if ((c >= 'a' && c <= 'z') || isDigit(static_cast<char>(c))) out += static_cast<char>(c);
            else if (c != '\'' && c != '`') out += ' ';
            continue;
        }
        size_t n = std::min(utf8Length(c), s.size() - i);
        if (n == 2 && c == 0xC3) {
            const char* f = foldC3(static_cast<unsigned char>(s[i + 1]));
            out += f ? f : " ";
        } else if (n == 3 && c == 0xE2 && static_cast<unsigned char>(s[i + 1]) == 0x80 &&
                   (static_cast<unsigned char>(s[i + 2]) == 0x98 || static_cast<unsigned char>(s[i + 2]) == 0x99)) {
            // typografische Apostrophe: entfallen
        } else {
            out += ' ';
        }
        i += n;
    }
    return out;
}

}  // namespace

std::string cleanTitle(const std::string& raw) {
    std::string s = raw;
    for (char& c : s)
        if (c == '_') c = ' ';
    s = collapseSpaces(stripGroups(s));
    s = collapseSpaces(moveArticle(s));
    while (s.size() >= 1 && s.back() == '-' && (s.size() == 1 || s[s.size() - 2] == ' ')) {
        s.pop_back();
        s = trim(s);
    }
    return s;
}

std::string sortKey(const std::string& title) {
    std::string s = lower(trim(title));
    static const char* const articles[] = {"the ", "die ", "der ", "das "};
    for (const char* a : articles) {
        if (startsWith(s, a)) { s.erase(0, 4); break; }
    }
    std::string out;
    for (char c : foldAscii(s))
        if (c != ' ') out += c;
    return out;
}

std::string searchKey(const std::string& text) { return trim(collapseSpaces(foldAscii(text))); }

char indexLetter(const std::string& title) {
    std::string k = sortKey(title);
    if (k.empty() || !(k[0] >= 'a' && k[0] <= 'z')) return '#';
    return static_cast<char>(k[0] - 'a' + 'A');
}

std::optional<std::string> extractGameId(const std::string& raw) {
    std::string s = trim(raw);
    if (s.size() < 8 || s.back() != ']' || s[s.size() - 8] != '[') return std::nullopt;
    std::string id = s.substr(s.size() - 7, 6);
    for (char c : id)
        if (!((c >= 'A' && c <= 'Z') || isDigit(c))) return std::nullopt;
    return id;
}
