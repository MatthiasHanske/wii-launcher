#include "core/covers.h"

namespace {

std::string baseName(const std::string& path) {
    std::string p = path;
    while (!p.empty() && p.back() == '/') p.pop_back();
    size_t slash = p.rfind('/');
    return slash == std::string::npos ? p : p.substr(slash + 1);
}

std::string stem(const std::string& file) {
    size_t dot = file.rfind('.');
    return (dot == std::string::npos || dot == 0) ? file : file.substr(0, dot);
}

}  // namespace

std::string coverPath(const Game& g) {
    switch (g.system) {
        case System::Wii:
        case System::GameCube:
            return g.gameId.empty() ? std::string() : "sd:/config/images/2D/" + g.gameId + ".png";
        case System::Adventure: {
            std::string name = baseName(g.dir);
            return name.empty() ? std::string() : "sd:/launcher/covers/dos/" + name + ".png";
        }
        default: {
            std::string s = stem(g.file);
            if (s.empty()) return std::string();
            return std::string("sd:/launcher/covers/") + systemKey(g.system) + "/" + s + ".png";
        }
    }
}
