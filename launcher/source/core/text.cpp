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
    // Größtes n (Anzahl Codepoints), für das trim(Präfix n) + "…" passt; n = 0 passt sicher.
    auto candidate = [&](size_t n) {
        std::string head = text.substr(0, n == 0 ? 0 : ends[n - 1]);
        while (!head.empty() && head.back() == ' ') head.pop_back();
        return head + kEllipsis;
    };
    size_t lo = 0, hi = ends.size();  // text ganz (n = ends.size()) passt nicht
    while (hi - lo > 1) {
        size_t mid = lo + (hi - lo) / 2;
        if (measure(candidate(mid)) <= maxWidth) lo = mid;
        else hi = mid;
    }
    return candidate(lo);
}

std::string EllipsizeMemo::get(const std::string& text, float maxWidth, int size,
                               const std::function<float(const std::string&)>& measure) {
    auto key = std::make_tuple(text, maxWidth, size);
    auto it = memo_.find(key);
    if (it != memo_.end()) return it->second;
    if (memo_.size() >= maxEntries_) memo_.clear();
    std::string result = ellipsize(text, maxWidth, measure);
    memo_.emplace(std::move(key), result);
    return result;
}
