#include "core/recent.h"

#include <cstddef>

namespace {

bool parseInt64(const std::string& s, int64_t& out) {
    if (s.empty() || s.size() > 18) return false;
    int64_t v = 0;
    for (char c : s) {
        if (c < '0' || c > '9') return false;
        v = v * 10 + (c - '0');
    }
    out = v;
    return true;
}

bool parseLine(const std::string& line, RecentEntry& e) {
    std::string f[4];
    int n = 0;
    size_t start = 0;
    for (size_t i = 0; i <= line.size(); ++i) {
        if (i == line.size() || line[i] == '\t') {
            if (n >= 4) return false;
            f[n++] = line.substr(start, i - start);
            start = i + 1;
        }
    }
    if (n != 4) return false;
    int64_t ts;
    if (!parseInt64(f[0], ts)) return false;
    auto sys = systemFromKey(f[1]);
    if (!sys) return false;
    e.timestamp = ts;
    e.system = *sys;
    e.dir = f[2];
    e.file = f[3];
    return true;
}

bool sameGame(const RecentEntry& e, const std::string& dir, const std::string& file) {
    return e.dir == dir && e.file == file;
}

}  // namespace

std::vector<RecentEntry> parseRecent(const std::string& text) {
    std::vector<RecentEntry> out;
    size_t start = 0;
    while (start <= text.size()) {
        size_t nl = text.find('\n', start);
        if (nl == std::string::npos) nl = text.size();
        std::string line = text.substr(start, nl - start);
        if (!line.empty() && line.back() == '\r') line.pop_back();
        RecentEntry e;
        if (!line.empty() && parseLine(line, e)) out.push_back(e);
        start = nl + 1;
    }
    return out;
}

std::string serializeRecent(const std::vector<RecentEntry>& entries) {
    std::string s;
    for (const RecentEntry& e : entries) {
        s += std::to_string(e.timestamp);
        s += '\t';
        s += systemKey(e.system);
        s += '\t';
        s += e.dir;
        s += '\t';
        s += e.file;
        s += '\n';
    }
    return s;
}

void pushRecent(std::vector<RecentEntry>& entries, const RecentEntry& e) {
    for (size_t i = 0; i < entries.size();) {
        if (sameGame(entries[i], e.dir, e.file))
            entries.erase(entries.begin() + i);
        else
            ++i;
    }
    entries.insert(entries.begin(), e);
    if (entries.size() > static_cast<size_t>(kRecentMax)) entries.resize(kRecentMax);
}

std::vector<const Game*> resolveRecent(const std::vector<RecentEntry>& entries,
                                       const std::vector<Game>& all) {
    std::vector<const Game*> out;
    for (const RecentEntry& e : entries) {
        for (const Game& g : all) {
            if (g.dir != e.dir || g.file != e.file) continue;
            bool dup = false;
            for (const Game* o : out)
                if (o == &g) dup = true;
            if (!dup) out.push_back(&g);
            break;
        }
    }
    return out;
}

std::vector<const Game*> recentForLibrary(const std::vector<RecentEntry>& entries, const Library& library) {
    std::vector<const Game*> out;
    for (const Game* g : resolveRecent(entries, library.games()))
        if (library.enabled().get(g->system)) out.push_back(g);
    return out;
}
