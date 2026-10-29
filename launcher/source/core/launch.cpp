#include "core/launch.h"

#include <cctype>

namespace {

const char* kUsbLoaderDol = "sd:/apps/usbloader_gx/boot.dol";
const char* kNot64Config = "sd:/not64/settings.cfg";
const char* kNot64RomPath = "usb:/not64/roms";
const char* kDosboxConf = "sd:/apps/dosbox-wii/dosbox.conf";
const char* kDosboxTemplate = "sd:/launcher/dosbox.conf.tpl";

std::string lower(std::string s) {
    for (auto& c : s) c = (char)std::tolower((unsigned char)c);
    return s;
}

std::string trim(const std::string& s) {
    size_t b = 0, e = s.size();
    while (b < e && std::isspace((unsigned char)s[b])) ++b;
    while (e > b && std::isspace((unsigned char)s[e - 1])) --e;
    return s.substr(b, e - b);
}

bool startsWith(const std::string& s, const std::string& prefix) {
    return s.compare(0, prefix.size(), prefix) == 0;
}

bool endsWith(const std::string& s, const std::string& suffix) {
    return s.size() >= suffix.size() && s.compare(s.size() - suffix.size(), suffix.size(), suffix) == 0;
}

// Zeilen ohne Zeilenende; ein abschliessendes '\n' erzeugt keine leere Extrazeile.
std::vector<std::string> splitLines(const std::string& text) {
    std::vector<std::string> lines;
    size_t start = 0;
    while (start < text.size()) {
        size_t nl = text.find('\n', start);
        if (nl == std::string::npos) nl = text.size();
        lines.push_back(text.substr(start, nl - start));
        start = nl + 1;
    }
    return lines;
}

std::string joinLines(const std::vector<std::string>& lines) {
    std::string out;
    for (const auto& l : lines) out += l + "\n";
    return out;
}

std::string baseName(std::string path) {
    while (!path.empty() && path.back() == '/') path.pop_back();
    size_t slash = path.rfind('/');
    return slash == std::string::npos ? path : path.substr(slash + 1);
}

// Kleinbuchstaben und Ziffern, alles andere faellt weg ("SimCity 2000" -> "simcity2000").
std::string alnumLower(const std::string& s) {
    std::string out;
    for (char c : s)
        if (std::isalnum((unsigned char)c)) out += (char)std::tolower((unsigned char)c);
    return out;
}

size_t commonPrefix(const std::string& a, const std::string& b) {
    size_t n = 0;
    while (n < a.size() && n < b.size() && a[n] == b[n]) ++n;
    return n;
}

bool isSubsequence(const std::string& needle, const std::string& hay) {
    size_t i = 0;
    for (char c : hay)
        if (i < needle.size() && needle[i] == c) ++i;
    return i == needle.size();
}

// Aehnlichkeit von Dateistamm und Ordnername: gemeinsamer Praefix. Ist der ganze Stamm eine
// DOS-Abkuerzung des Ordnernamens (gleicher Anfangsbuchstabe, alle Zeichen in Reihenfolge
// enthalten, z. B. SC2000 in SimCity 2000), zaehlt die volle Stammlaenge.
size_t nameScore(const std::string& stem, const std::string& folder) {
    size_t prefix = commonPrefix(stem, folder);
    if (prefix > 0 && isSubsequence(stem, folder)) return stem.size();
    return prefix;
}

bool isDosExecutable(const DirEntry& e, bool& isBat) {
    if (e.isDir) return false;
    std::string l = lower(e.name);
    isBat = endsWith(l, ".bat");
    return isBat || endsWith(l, ".exe");
}

LaunchPlan usbLoaderPlan(const Game& g) {
    LaunchPlan p;
    p.dolPath = kUsbLoaderDol;
    if (g.gameId.empty()) p.quickBoot = false;
    else p.args = {g.gameId};
    return p;
}

std::string segaDir(const Game& g) {
    // Der Scanner liefert dir bereits mit Unterordner (usb:/genplus/roms/md); nur anhaengen,
    // wenn dir der Systemordner ist.
    if (g.subdir.empty() || endsWith(g.dir, "/" + g.subdir)) return g.dir;
    std::string d = g.dir;
    while (!d.empty() && d.back() == '/') d.pop_back();
    return d + "/" + g.subdir;
}

std::string not64Config(IFileSystem& fs) {
    std::string text;
    fs.readText(kNot64Config, text);
    std::vector<std::string> lines = splitLines(text);
    const std::string wanted = std::string("rompath = ") + kNot64RomPath;
    bool replaced = false;
    for (auto& l : lines) {
        size_t eq = l.find('=');
        if (eq == std::string::npos || lower(trim(l.substr(0, eq))) != "rompath") continue;
        l = wanted;
        replaced = true;
    }
    if (!replaced) lines.push_back(wanted);
    return joinLines(lines);
}

bool isSectionHeader(const std::string& line) {
    std::string t = trim(line);
    return t.size() >= 2 && t.front() == '[' && t.back() == ']';
}

std::string dosboxConfig(const Game& g, IFileSystem& fs) {
    std::string tpl;
    if (!fs.readText(kDosboxTemplate, tpl)) tpl.clear();

    std::vector<std::string> kept;
    bool inAutoexec = false;
    for (const auto& l : splitLines(tpl)) {
        if (isSectionHeader(l)) inAutoexec = lower(trim(l)) == "[autoexec]";
        if (!inAutoexec) kept.push_back(l);
    }

    std::string folder = baseName(g.dir);
    std::vector<DirEntry> entries;
    fs.listDir(g.dir, entries);
    std::optional<std::string> autoexec;
    for (const auto& e : entries) {
        if (e.isDir || lower(e.name) != "autoexec.txt") continue;
        std::string text;
        if (fs.readText(g.dir + "/" + e.name, text)) autoexec = text;
        break;
    }
    std::string cmd = pickDosCommand(entries, folder, autoexec);

    kept.push_back("[autoexec]");
    // DOSBox 0.74 kennt keine langen Dateinamen: Spielordner direkt als C: mounten, kein cd.
    kept.push_back("mount c \"" + g.dir + "\"");
    kept.push_back("c:");
    if (!cmd.empty()) kept.push_back(cmd);
    return joinLines(kept);
}

}  // namespace

std::string pickDosCommand(const std::vector<DirEntry>& entries, const std::string& folderName,
                           std::optional<std::string> autoexecTxt) {
    if (autoexecTxt) {
        std::vector<std::string> lines = splitLines(*autoexecTxt);
        if (!lines.empty()) {
            std::string first = trim(lines[0]);
            if (!first.empty()) return first;
        }
    }

    std::vector<const DirEntry*> exes;
    std::vector<bool> bats;
    for (const auto& e : entries) {
        bool bat = false;
        if (!isDosExecutable(e, bat)) continue;
        exes.push_back(&e);
        bats.push_back(bat);
    }
    if (exes.size() == 1) return exes[0]->name;

    const std::string folder = alnumLower(folderName);
    std::string best;
    size_t bestScore = 0;
    bool bestBat = false;
    for (size_t i = 0; i < exes.size(); ++i) {
        const std::string& name = exes[i]->name;
        std::string stem = alnumLower(name.substr(0, name.rfind('.')));
        size_t score = nameScore(stem, folder);
        if (score > bestScore || (score == bestScore && bats[i] && !bestBat)) {
            best = name;
            bestScore = score;
            bestBat = bats[i];
        }
    }
    return bestScore >= 3 ? best : "";
}

LaunchPlan makeLaunchPlan(const Game& g, IFileSystem& fs) {
    LaunchPlan p;
    switch (g.system) {
        case System::Wii:
        case System::GameCube:
            return usbLoaderPlan(g);
        case System::SNES:
            p.dolPath = "sd:/apps/snes9xgx/boot.dol";
            p.args = {g.dir, g.file};
            return p;
        case System::NES:
            p.dolPath = "sd:/apps/fceugx/boot.dol";
            p.args = {g.dir, g.file};
            return p;
        case System::Sega:
            p.dolPath = "sd:/apps/genplus-gx/boot.dol";
            p.args = {segaDir(g), g.file};
            return p;
        case System::N64:
            p.dolPath = "sd:/apps/not64/boot.dol";
            p.filesToWrite.push_back({kNot64Config, not64Config(fs)});
            return p;
        case System::Adventure:
            if (startsWith(g.dir, "usb:/scummvm/")) {
                p.dolPath = "sd:/apps/scummvm/boot.elf";
                p.args = {"--auto-detect", "--path=" + g.dir, "--extrapath=sd:/apps/scummvm",
                          "--themepath=sd:/apps/scummvm"};
            } else {
                p.dolPath = "sd:/apps/dosbox-wii/boot.dol";
                p.filesToWrite.push_back({kDosboxConf, dosboxConfig(g, fs)});
            }
            return p;
    }
    return p;
}
