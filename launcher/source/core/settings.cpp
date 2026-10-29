#include "core/settings.h"

#include <cstddef>

namespace {
const System kAll[] = {System::Wii, System::GameCube, System::N64, System::SNES,
                       System::NES, System::Sega,     System::Adventure};

SystemSet parseSystems(const std::string& v) {
    SystemSet set;
    size_t start = 0;
    while (start <= v.size()) {
        size_t c = v.find(',', start);
        if (c == std::string::npos) c = v.size();
        auto sys = systemFromKey(v.substr(start, c - start));
        if (sys) set.at(*sys) = true;
        start = c + 1;
    }
    return set;
}
}  // namespace

Settings parseSettings(const std::string& text) {
    Settings s;
    size_t start = 0;
    while (start <= text.size()) {
        size_t nl = text.find('\n', start);
        if (nl == std::string::npos) nl = text.size();
        std::string line = text.substr(start, nl - start);
        start = nl + 1;
        if (!line.empty() && line.back() == '\r') line.pop_back();
        size_t eq = line.find('=');
        if (eq == std::string::npos || eq == 0) continue;
        std::string key = line.substr(0, eq), val = line.substr(eq + 1);
        if (key == "systems")
            s.systems = parseSystems(val);
        else if (key == "extra_root")
            s.extraRoot = val;
    }
    return s;
}

std::string serializeSettings(const Settings& s) {
    std::string out = "systems=";
    bool first = true;
    for (System sys : kAll) {
        if (!s.systems.get(sys)) continue;
        if (!first) out += ',';
        out += systemKey(sys);
        first = false;
    }
    out += "\nextra_root=" + s.extraRoot + "\n";
    return out;
}
