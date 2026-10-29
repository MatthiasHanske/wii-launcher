#include "test.h"

#include <string>
#include <vector>

#include "core/library.h"
#include "core/recent.h"

static RecentEntry mk(int64_t t, System s, const char* dir, const char* file) {
    return RecentEntry{t, s, dir, file};
}

TEST(recent_roundtrip) {
    std::vector<RecentEntry> v = {mk(200, System::Wii, "usb:/wbfs/A", "A.wbfs"),
                                  mk(100, System::SNES, "usb:/snes", "B.sfc")};
    std::string text = serializeRecent(v);
    CHECK_EQ(text, "200\twii\tusb:/wbfs/A\tA.wbfs\n100\tsnes\tusb:/snes\tB.sfc\n");
    auto back = parseRecent(text);
    CHECK_EQ(back.size(), (size_t)2);
    CHECK_EQ(back[0].timestamp, 200);
    CHECK(back[0].system == System::Wii);
    CHECK_EQ(back[1].file, "B.sfc");
    CHECK(back[1].system == System::SNES);
}

TEST(recent_roundtrip_empty_file) {
    std::vector<RecentEntry> v = {mk(5, System::GameCube, "usb:/games/Foo", "")};
    auto back = parseRecent(serializeRecent(v));
    CHECK_EQ(back.size(), (size_t)1);
    CHECK_EQ(back[0].dir, "usb:/games/Foo");
    CHECK_EQ(back[0].file, "");
}

TEST(recent_parse_skips_broken_lines) {
    auto v = parseRecent("kaputt\n123\twii\tusb:/x\tA.wbfs\n");
    CHECK_EQ(v.size(), (size_t)1);
    CHECK_EQ(v[0].timestamp, 123);
    CHECK_EQ(v[0].dir, "usb:/x");
}

TEST(recent_parse_rejects_bad_fields) {
    CHECK_EQ(parseRecent("abc\twii\td\tf\n").size(), (size_t)0);
    CHECK_EQ(parseRecent("1\tfoo\td\tf\n").size(), (size_t)0);
    CHECK_EQ(parseRecent("1\twii\td\n").size(), (size_t)0);
    CHECK_EQ(parseRecent("1\twii\td\tf\textra\n").size(), (size_t)0);
    CHECK_EQ(parseRecent("\n\n").size(), (size_t)0);
    CHECK_EQ(parseRecent("").size(), (size_t)0);
}

TEST(recent_parse_tolerates_crlf_and_missing_final_newline) {
    auto v = parseRecent("1\twii\td\tf.wbfs\r\n2\tgc\te\t");
    CHECK_EQ(v.size(), (size_t)2);
    CHECK_EQ(v[0].file, "f.wbfs");
    CHECK_EQ(v[1].file, "");
}

TEST(recent_push_dedupes_and_moves_front) {
    std::vector<RecentEntry> v;
    pushRecent(v, mk(1, System::Wii, "d1", "a"));
    pushRecent(v, mk(2, System::Wii, "d2", "b"));
    pushRecent(v, mk(3, System::Wii, "d1", "a"));
    CHECK_EQ(v.size(), (size_t)2);
    CHECK_EQ(v[0].timestamp, 3);
    CHECK_EQ(v[0].dir, "d1");
    CHECK_EQ(v[1].dir, "d2");
}

TEST(recent_push_caps_at_20) {
    std::vector<RecentEntry> v;
    for (int i = 0; i < 25; ++i) pushRecent(v, mk(i, System::NES, "d", std::to_string(i).c_str()));
    CHECK_EQ(v.size(), (size_t)kRecentMax);
    CHECK_EQ(kRecentMax, 20);
    CHECK_EQ(v[0].file, "24");
    CHECK_EQ(v[19].file, "5");
}

static Game mkGame(System s, const char* dir, const char* file) {
    Game g;
    g.system = s;
    g.dir = dir;
    g.file = file;
    g.title = file;
    return g;
}

TEST(recent_resolve_skips_deleted_keeps_order) {
    std::vector<Game> all = {mkGame(System::Wii, "d1", "a"), mkGame(System::Wii, "d2", "b"),
                             mkGame(System::GameCube, "d3", "")};
    std::vector<RecentEntry> r = {mk(3, System::GameCube, "d3", ""), mk(2, System::Wii, "gone", "x"),
                                  mk(1, System::Wii, "d1", "a")};
    auto out = resolveRecent(r, all);
    CHECK_EQ(out.size(), (size_t)2);
    CHECK(out[0] == &all[2]);
    CHECK(out[1] == &all[0]);
}

TEST(recent_resolve_no_duplicates) {
    std::vector<Game> all = {mkGame(System::Wii, "d1", "a")};
    std::vector<RecentEntry> r = {mk(2, System::Wii, "d1", "a"), mk(1, System::Wii, "d1", "a")};
    CHECK_EQ(resolveRecent(r, all).size(), (size_t)1);
}

TEST(recent_for_library_filters_disabled_systems) {
    std::vector<Game> games(3);
    games[0].system = System::Wii;  games[0].dir = "w"; games[0].file = "a"; games[0].sortKey = "a";
    games[1].system = System::SNES; games[1].dir = "s"; games[1].file = "b"; games[1].sortKey = "b";
    games[2].system = System::N64;  games[2].dir = "n"; games[2].file = "c"; games[2].sortKey = "c";
    Library lib(games);
    lib.setEnabled(SystemSet::defaults());  // Wii, GameCube, N64
    std::vector<RecentEntry> e = {mk(3, System::SNES, "s", "b"), mk(2, System::N64, "n", "c"),
                                  mk(1, System::Wii, "w", "a")};
    auto r = recentForLibrary(e, lib);
    CHECK_EQ(r.size(), (size_t)2);
    CHECK_EQ(r[0]->file, "c");
    CHECK_EQ(r[1]->file, "a");
    CHECK(r[0] == &lib.games()[2]);
    SystemSet all = SystemSet::defaults();
    all.at(System::SNES) = true;
    lib.setEnabled(all);
    CHECK_EQ(recentForLibrary(e, lib).size(), (size_t)3);
}
