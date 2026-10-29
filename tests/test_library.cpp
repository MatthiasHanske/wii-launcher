#include "test.h"

#include <chrono>
#include <string>
#include <vector>

#include "core/library.h"
#include "core/titles.h"

static Game mk(System s, const std::string& title) {
    Game g;
    g.system = s;
    g.title = title;
    g.sortKey = sortKey(title);
    g.letter = indexLetter(title);
    return g;
}

static std::vector<Game> fixture() {
    std::vector<Game> v;
    const char* wii[] = {"Mario Kart Wii", "New Super Mario Bros. Wii", "Super Smash Bros. Brawl",
                         "Wii Sports", "Wii Play", "Zelda Skyward Sword", "Metroid Prime 3",
                         "Just Dance 3", "Sonic Colors", "Super Paper Mario", "Kirby's Epic Yarn",
                         "007 Quantum of Solace"};
    const char* n64[] = {"Super Mario 64", "Mario Kart 64", "Star Fox 64", "GoldenEye 007",
                         "Banjo-Kazooie"};
    const char* snes[] = {"Super Mario World", "Chrono Trigger", "F-Zero"};
    const char* nes[] = {"Super Mario Bros.", "Mega Man 2", "Metroid", "Contra", "Duck Tales"};
    for (auto t : wii) v.push_back(mk(System::Wii, t));
    for (auto t : n64) v.push_back(mk(System::N64, t));
    for (auto t : snes) v.push_back(mk(System::SNES, t));
    for (auto t : nes) v.push_back(mk(System::NES, t));
    return v;
}

TEST(library_tabs_default) {
    Library lib(fixture());
    lib.setEnabled(SystemSet::defaults());
    std::vector<System> expect = {System::Wii, System::GameCube, System::N64};
    CHECK(lib.tabs() == expect);
}

TEST(library_tabs_with_snes) {
    Library lib(fixture());
    SystemSet s = SystemSet::defaults();
    s.at(System::SNES) = true;
    lib.setEnabled(s);
    std::vector<System> expect = {System::Wii, System::GameCube, System::N64, System::SNES};
    CHECK(lib.tabs() == expect);
}

TEST(library_all_and_single_tab) {
    Library lib(fixture());
    lib.setEnabled(SystemSet::defaults());
    lib.setTab(std::nullopt);
    CHECK_EQ(lib.visible().size(), (size_t)17);
    for (auto g : lib.visible()) CHECK(g->system == System::Wii || g->system == System::N64);
    lib.setTab(System::N64);
    CHECK_EQ(lib.visible().size(), (size_t)5);
    lib.setTab(System::NES);  // deaktiviert -> wie Alle
    CHECK_EQ(lib.visible().size(), (size_t)17);
    lib.setTab(System::GameCube);
    CHECK_EQ(lib.visible().size(), (size_t)0);
}

TEST(library_visible_sorted) {
    Library lib(fixture());
    lib.setEnabled(SystemSet::defaults());
    lib.setTab(std::nullopt);
    auto& v = lib.visible();
    for (size_t i = 1; i < v.size(); ++i) CHECK(v[i - 1]->sortKey <= v[i]->sortKey);
}

TEST(library_letters) {
    Library lib(fixture());
    lib.setEnabled(SystemSet::defaults());
    lib.setTab(std::nullopt);
    auto l = lib.letters();
    CHECK(!l.empty());
    CHECK_EQ(l.front(), '#');
    for (size_t i = 1; i < l.size(); ++i) CHECK(l[i - 1] < l[i]);
    auto has = [&](char c) { for (char x : l) if (x == c) return true; return false; };
    CHECK(has('M'));
    CHECK(has('S'));
    CHECK(has('B'));
    CHECK(!has('C'));  // Chrono Trigger nur SNES
    CHECK(!has('D'));  // Duck Tales nur NES
    CHECK(!has('Q'));
    lib.setTab(System::N64);
    auto l2 = lib.letters();
    CHECK_EQ(l2.size(), (size_t)4);  // B, G, M, S
}

TEST(library_first_index_for_letter) {
    Library lib(fixture());
    lib.setEnabled(SystemSet::defaults());
    lib.setTab(std::nullopt);
    int i = lib.firstIndexForLetter('S');
    CHECK(i >= 0);
    CHECK_EQ(lib.visible()[i]->letter, 'S');
    CHECK(i == 0 || lib.visible()[i - 1]->letter != 'S');
    CHECK_EQ(lib.firstIndexForLetter('Q'), -1);
    CHECK_EQ(lib.firstIndexForLetter('#'), 0);
}

TEST(library_search) {
    Library lib(fixture());
    lib.setEnabled(SystemSet::defaults());
    lib.setTab(System::N64);  // Tab wird ignoriert
    auto r = lib.search("mar", 10);
    CHECK(!r.empty());
    size_t marios = 0;
    for (auto g : r) {
        CHECK(g->system != System::NES && g->system != System::SNES);
        CHECK(g->title.find("Mario") != std::string::npos || g->title.find("Mar") != std::string::npos);
        ++marios;
    }
    CHECK_EQ(marios, (size_t)5);  // 3x Wii (MK, NSMB, SPM) + SM64 + MK64 -> 5
    CHECK_EQ(lib.search("MAR", 10).size(), r.size());
    CHECK_EQ(lib.search("mar", 2).size(), (size_t)2);
    CHECK(lib.search("", 10).empty());
    CHECK(lib.search("xyzzy", 10).empty());
}

TEST(library_pages) {
    Library lib(fixture());
    lib.setEnabled(SystemSet::defaults());
    lib.setTab(std::nullopt);
    CHECK_EQ(lib.pageCount(), 2);
    CHECK_EQ(lib.page(0).size(), (size_t)10);
    CHECK_EQ(lib.page(1).size(), (size_t)7);
    CHECK(lib.page(1)[0] == lib.visible()[10]);
    CHECK(lib.page(2).empty());
    CHECK(lib.page(-1).empty());
}

TEST(library_empty) {
    Library lib({});
    lib.setEnabled(SystemSet::defaults());
    lib.setTab(std::nullopt);
    CHECK_EQ(lib.pageCount(), 0);
    CHECK(lib.letters().empty());
    CHECK(lib.search("a", 10).empty());
    CHECK(lib.page(0).empty());
    CHECK_EQ(lib.firstIndexForLetter('A'), -1);
}

TEST(library_performance_2000) {
    std::vector<Game> v;
    for (int i = 0; i < 2000; ++i) {
        std::string t = "Game ";
        t += char('a' + i % 26);
        t += std::to_string(i);
        v.push_back(mk(i % 2 ? System::Wii : System::N64, t));
    }
    Library lib(std::move(v));
    lib.setEnabled(SystemSet::defaults());
    auto t0 = std::chrono::steady_clock::now();
    lib.setTab(std::nullopt);
    auto l = lib.letters();
    auto r = lib.search("a", 10);
    auto ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
    CHECK(ms < 50.0);
    CHECK_EQ(lib.visible().size(), (size_t)2000);
    CHECK_EQ(r.size(), (size_t)10);
    CHECK(!l.empty());
}
