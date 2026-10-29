#include "core/scanner.h"

#include <algorithm>
#include <cctype>

#include "core/titles.h"

namespace {

std::string join(const std::string& a, const std::string& b) {
    std::string base = a;
    while (!base.empty() && base.back() == '/') base.pop_back();
    return base + "/" + b;
}

std::string lower(std::string s) {
    for (auto& c : s) c = (char)std::tolower((unsigned char)c);
    return s;
}

// Zerlegt "name.ext" in Basis und kleingeschriebene Endung (ohne Punkt).
void splitExt(const std::string& file, std::string& base, std::string& ext) {
    size_t dot = file.rfind('.');
    if (dot == std::string::npos || dot == 0) { base = file; ext.clear(); return; }
    base = file.substr(0, dot);
    ext = lower(file.substr(dot + 1));
}

bool hidden(const std::string& name) { return name.compare(0, 2, "._") == 0; }

bool hasExt(const std::string& ext, std::initializer_list<const char*> list) {
    for (const char* e : list) if (ext == e) return true;
    return false;
}

void fillTitle(Game& g, const std::string& base) {
    g.title = cleanTitle(base);
    g.sortKey = sortKey(g.title);
    g.letter = indexLetter(g.title);
}

void scanRoms(IFileSystem& fs, const std::string& dir, System sys, const std::string& subdir,
              std::initializer_list<const char*> exts, std::vector<Game>& out) {
    std::vector<DirEntry> entries;
    if (!fs.listDir(dir, entries)) return;
    for (const auto& e : entries) {
        if (e.isDir || hidden(e.name)) continue;
        std::string base, ext;
        splitExt(e.name, base, ext);
        if (!hasExt(ext, exts)) continue;
        Game g;
        g.system = sys;
        fillTitle(g, base);
        g.dir = dir;
        g.file = e.name;
        g.subdir = subdir;
        out.push_back(g);
    }
}

void scanWii(IFileSystem& fs, const std::string& root, std::vector<Game>& out) {
    std::string dir = join(root, "wbfs");
    std::vector<DirEntry> entries;
    if (!fs.listDir(dir, entries)) return;
    for (const auto& e : entries) {
        if (hidden(e.name)) continue;
        Game g;
        g.system = System::Wii;
        if (e.isDir) {
            std::string sub = join(dir, e.name);
            std::vector<DirEntry> inner;
            if (!fs.listDir(sub, inner)) continue;
            std::string wbfs;
            for (const auto& f : inner) {
                std::string b, x;
                splitExt(f.name, b, x);
                if (!f.isDir && !hidden(f.name) && x == "wbfs") { wbfs = f.name; break; }
            }
            if (wbfs.empty()) continue;
            fillTitle(g, e.name);
            auto id = extractGameId(e.name);
            if (id) g.gameId = *id;
            else { std::string b, x; splitExt(wbfs, b, x); if (b.size() == 6) g.gameId = b; }
            g.dir = sub;
            g.file = wbfs;
        } else {
            std::string base, ext;
            splitExt(e.name, base, ext);
            if (ext != "wbfs") continue;
            fillTitle(g, base);
            auto id = extractGameId(base);
            if (id) g.gameId = *id;
            g.dir = dir;
            g.file = e.name;
        }
        out.push_back(g);
    }
}

void scanGameCube(IFileSystem& fs, const std::string& root, std::vector<Game>& out) {
    std::string dir = join(root, "games");
    std::vector<DirEntry> entries;
    if (!fs.listDir(dir, entries)) return;
    for (const auto& e : entries) {
        if (!e.isDir || hidden(e.name)) continue;
        std::string sub = join(dir, e.name);
        std::string iso = join(sub, "game.iso");
        if (!fs.exists(iso)) continue;
        Game g;
        g.system = System::GameCube;
        fillTitle(g, e.name);
        auto id = extractGameId(e.name);
        if (id) {
            g.gameId = *id;
        } else {
            std::vector<uint8_t> hdr;
            if (fs.readBytes(iso, 0, 6, hdr) && hdr.size() == 6) {
                bool ok = true;
                for (uint8_t c : hdr) if (!std::isalnum(c)) ok = false;
                if (ok) g.gameId.assign(hdr.begin(), hdr.end());
            }
        }
        g.dir = sub;
        out.push_back(g);
    }
}

void scanAdventure(IFileSystem& fs, const std::string& root, const char* folder, std::vector<Game>& out) {
    std::string dir = join(root, folder);
    std::vector<DirEntry> entries;
    if (!fs.listDir(dir, entries)) return;
    for (const auto& e : entries) {
        if (!e.isDir || hidden(e.name)) continue;
        Game g;
        g.system = System::Adventure;
        fillTitle(g, e.name);
        g.dir = join(dir, e.name);
        out.push_back(g);
    }
}

}  // namespace

std::vector<Game> scanLibrary(IFileSystem& fs, const std::string& root) {
    std::vector<Game> out;
    scanWii(fs, root, out);
    scanGameCube(fs, root, out);
    scanRoms(fs, join(root, "not64/roms"), System::N64, "", {"z64", "n64", "v64"}, out);
    scanRoms(fs, join(root, "snes9xgx/roms"), System::SNES, "", {"sfc", "smc"}, out);
    scanRoms(fs, join(root, "fceugx/roms"), System::NES, "", {"nes"}, out);
    scanRoms(fs, join(root, "genplus/roms/md"), System::Sega, "md", {"gen", "md", "smd", "bin"}, out);
    scanRoms(fs, join(root, "genplus/roms/sms"), System::Sega, "sms", {"sms"}, out);
    scanRoms(fs, join(root, "genplus/roms/sg"), System::Sega, "sg", {"sg"}, out);
    scanAdventure(fs, root, "scummvm", out);
    scanAdventure(fs, root, "dosbox", out);

    std::stable_sort(out.begin(), out.end(), [](const Game& a, const Game& b) {
        if (a.sortKey != b.sortKey) return a.sortKey < b.sortKey;
        return a.title < b.title;
    });
    return out;
}
