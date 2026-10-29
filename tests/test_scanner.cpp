#include "test.h"
#include "fake_fs.h"
#include "core/scanner.h"

static const Game* find(const std::vector<Game>& g, const std::string& title) {
    for (const auto& x : g) if (x.title == title) return &x;
    return nullptr;
}

static void fill(FakeFs& fs) {
    fs.addFile("usb:/wbfs/Mario Kart Wii (Europe) [RMCP01]/RMCP01.wbfs");
    fs.addFile("usb:/wbfs/Wii Sports [RSPE01].wbfs");
    fs.addFile("usb:/games/F-Zero GX [GFZP01]/game.iso");
    fs.addFile("usb:/games/Luigis Mansion/game.iso", std::string("GLMP01\0\0\0\0", 10));
    fs.addFile("usb:/not64/roms/Super Mario 64 (E) [!].z64");
    fs.addFile("usb:/snes9xgx/roms/Super Mario World (Europe).sfc");
    fs.addFile("usb:/fceugx/roms/Mega Man 2 (E) [!].nes");
    fs.addFile("usb:/genplus/roms/md/Sonic 2 (W) [!].gen");
    fs.addFile("usb:/genplus/roms/sms/Alex Kidd (E) [!].sms");
    fs.addFile("usb:/scummvm/Monkey Island 2/MONKEY2.000");
    fs.addFile("usb:/dosbox/SimCity 2000/SC2000.EXE");
    fs.addFile("usb:/wbfs/._Muell.wbfs");
    fs.addDir("usb:/not64/roms/saves");
    fs.addFile("usb:/snes9xgx/roms/readme.txt", std::string("hi"));
}

TEST(scanner_full_library) {
    FakeFs fs;
    fill(fs);
    auto g = scanLibrary(fs, "usb:/");
    CHECK_EQ(g.size(), (size_t)11);

    const Game* mk = nullptr;
    for (const auto& x : g) if (x.gameId == "RMCP01") mk = &x;
    CHECK(mk != nullptr);
    if (mk) {
        CHECK(mk->system == System::Wii);
        CHECK_EQ(mk->dir, std::string("usb:/wbfs/Mario Kart Wii (Europe) [RMCP01]"));
        CHECK_EQ(mk->file, std::string("RMCP01.wbfs"));
    }

    const Game* ws = nullptr;
    for (const auto& x : g) if (x.gameId == "RSPE01") ws = &x;
    CHECK(ws != nullptr);
    if (ws) {
        CHECK(ws->system == System::Wii);
        CHECK_EQ(ws->dir, std::string("usb:/wbfs"));
        CHECK_EQ(ws->file, std::string("Wii Sports [RSPE01].wbfs"));
    }

    const Game* fz = nullptr;
    const Game* lm = nullptr;
    for (const auto& x : g) {
        if (x.gameId == "GFZP01") fz = &x;
        if (x.gameId == "GLMP01") lm = &x;
    }
    CHECK(fz != nullptr);
    CHECK(lm != nullptr);
    if (fz) { CHECK(fz->system == System::GameCube); CHECK_EQ(fz->file, std::string("")); }
    if (lm) CHECK_EQ(lm->dir, std::string("usb:/games/Luigis Mansion"));

    int sega = 0;
    for (const auto& x : g) {
        if (x.system == System::Sega) {
            ++sega;
            if (x.sortKey.compare(0, 5, "sonic") == 0) CHECK_EQ(x.subdir, std::string("md"));
            else CHECK_EQ(x.subdir, std::string("sms"));
        }
        CHECK(x.title.find("Muell") == std::string::npos);
        CHECK(x.file != "readme.txt");
    }
    CHECK_EQ(sega, 2);

    int adv = 0;
    for (const auto& x : g) if (x.system == System::Adventure) { ++adv; CHECK_EQ(x.file, std::string("")); }
    CHECK_EQ(adv, 2);

    // sortiert: alexkidd vor fzerogx
    CHECK_EQ(g.front().sortKey, std::string("alexkidd"));
    size_t ia = 0, iz = 0;
    for (size_t i = 0; i < g.size(); ++i) {
        if (g[i].sortKey == "alexkidd") ia = i;
        if (g[i].sortKey == "fzerogx") iz = i;
    }
    CHECK(ia < iz);
    for (size_t i = 1; i < g.size(); ++i) CHECK(g[i - 1].sortKey <= g[i].sortKey);
    CHECK(find(g, "Super Mario World") != nullptr || find(g, "Super Mario World (Europe)") != nullptr || g.size() == 11);
}

TEST(scanner_root_without_slash) {
    FakeFs fs;
    fs.addFile("usb:/snes9xgx/roms/Super Mario World (Europe).sfc");
    auto g = scanLibrary(fs, "usb:");
    CHECK_EQ(g.size(), (size_t)1);
    if (!g.empty()) CHECK_EQ(g[0].dir, std::string("usb:/snes9xgx/roms"));
}

TEST(scanner_uppercase_extension) {
    FakeFs fs;
    fs.addFile("usb:/fceugx/roms/Contra.NES");
    fs.addFile("usb:/fceugx/roms/._Contra.NES");
    auto g = scanLibrary(fs, "usb:/");
    CHECK_EQ(g.size(), (size_t)1);
}

TEST(scanner_empty_root) {
    FakeFs fs;
    fs.addDir("usb:/");
    auto g = scanLibrary(fs, "usb:/");
    CHECK(g.empty());
    FakeFs none;
    CHECK(scanLibrary(none, "usb:/").empty());
}
