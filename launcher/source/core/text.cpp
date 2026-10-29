#include "core/text.h"

namespace {

constexpr uint32_t kReplacement = 0xFFFD;
const char kEllipsis[] = "\xE2\x80\xA6";

bool isCont(unsigned char c) { return (c & 0xC0) == 0x80; }

// Länge der Sequenz ab s[i] in Bytes (gültig oder 1 für ein ungültiges Byte) und Codepoint.
size_t decodeOne(const std::string& s, size_t i, uint32_t& cp) {
    unsigned char c = static_cast<unsigned char>(s[i]);
    size_t len;
    uint32_t min;
    if (c < 0x80) { cp = c; return 1; }
    // C0/C1 könnten nur überlange Zweibyter einleiten, F5..FF nur Werte > U+10FFFF
    if (c == 0xC0 || c == 0xC1 || c >= 0xF5) { cp = kReplacement; return 1; }
    if ((c & 0xE0) == 0xC0) { len = 2; cp = c & 0x1F; min = 0x80; }
    else if ((c & 0xF0) == 0xE0) { len = 3; cp = c & 0x0F; min = 0x800; }
    else if ((c & 0xF8) == 0xF0) { len = 4; cp = c & 0x07; min = 0x10000; }
    else { cp = kReplacement; return 1; }
    if (i + len > s.size()) {
        // abgeschnitten: alle vorhandenen Folgebytes zusammen als ein Ersatzzeichen
        size_t n = 1;
        while (i + n < s.size() && isCont(static_cast<unsigned char>(s[i + n]))) ++n;
        cp = kReplacement;
        return n;
    }
    for (size_t k = 1; k < len; ++k) {
        unsigned char cc = static_cast<unsigned char>(s[i + k]);
        if (!isCont(cc)) { cp = kReplacement; return k; }
        cp = (cp << 6) | (cc & 0x3F);
    }
    if (cp < min || cp > 0x10FFFF || (cp >= 0xD800 && cp <= 0xDFFF)) cp = kReplacement;
    return len;
}

}  // namespace

std::vector<uint32_t> utf8Decode(const std::string& utf8) {
    std::vector<uint32_t> out;
    out.reserve(utf8.size());
    size_t i = 0;
    while (i < utf8.size()) {
        uint32_t cp;
        i += decodeOne(utf8, i, cp);
        out.push_back(cp);
    }
    return out;
}

std::string ellipsize(const std::string& text, float maxWidth,
                      const std::function<float(const std::string&)>& measure) {
    if (measure(text) <= maxWidth) return text;
    if (measure(kEllipsis) > maxWidth) return "";

    // Byte-Grenzen aller Codepoints sammeln, dann von lang nach kurz probieren.
    std::vector<size_t> ends;
    size_t i = 0;
    while (i < text.size()) {
        uint32_t cp;
        i += decodeOne(text, i, cp);
        ends.push_back(i);
    }
    for (size_t n = ends.size(); n-- > 0;) {
        std::string head = text.substr(0, ends[n]);
        while (!head.empty() && head.back() == ' ') head.pop_back();
        std::string candidate = head + kEllipsis;
        if (measure(candidate) <= maxWidth) return candidate;
    }
    return kEllipsis;
}
