#include "test.h"

#include <string>
#include <vector>

#include "core/library.h"
#include "core/titles.h"
#include "core/ui_state.h"

namespace {

Game mkGame(System s, const std::string& title) {
    Game g;
    g.system = s;
    g.title = title;
    g.sortKey = sortKey(title);
    g.letter = indexLetter(title);
    return g;
}

// Gleicher Aufbau wie in test_library.cpp: 12 Wii, 5 N64, 3 SNES, 5 NES.
std::vector<Game> uiFixture() {
    std::vector<Game> v;
    const char* wii[] = {"Mario Kart Wii", "New Super Mario Bros. Wii", "Super Smash Bros. Brawl",
                         "Wii Sports", "Wii Play", "Zelda Skyward Sword", "Metroid Prime 3",
                         "Just Dance 3", "Sonic Colors", "Super Paper Mario", "Kirby's Epic Yarn",
                         "007 Quantum of Solace"};
    const char* n64[] = {"Super Mario 64", "Mario Kart 64", "Star Fox 64", "GoldenEye 007",
                         "Banjo-Kazooie"};
    const char* snes[] = {"Super Mario World", "Chrono Trigger", "F-Zero"};
    const char* nes[] = {"Super Mario Bros.", "Mega Man 2", "Metroid", "Contra", "Duck Tales"};
    for (auto t : wii) v.push_back(mkGame(System::Wii, t));
    for (auto t : n64) v.push_back(mkGame(System::N64, t));
    for (auto t : snes) v.push_back(mkGame(System::SNES, t));
    for (auto t : nes) v.push_back(mkGame(System::NES, t));
    return v;
}

Command press(UiState& s, Library& lib, Action a) { return apply(s, lib, a, std::nullopt); }

Command click(UiState& s, Library& lib, Region r, int idx) {
    return apply(s, lib, Action::PointerClick, std::make_pair(static_cast<int>(r), idx));
}

// Liste offen, Fokus in der Liste (wie nach Steuerkreuz runter im Fächer).
void openList(UiState& s) {
    s.listOpen = true;
    s.focus = Focus::List;
}

int keyIndex(const std::string& label) {
    const auto& keys = keyboardKeys();
    for (size_t i = 0; i < keys.size(); ++i)
        if (keys[i] == label) return static_cast<int>(i);
    return -1;
}

}  // namespace

TEST(ui_defaults) {
    UiState s;
    CHECK(s.focus == Focus::Carousel);
    CHECK(s.overlay == Overlay::None);
    CHECK_EQ(s.tabIndex, 0);
    CHECK_EQ(s.listIndex, 0);
    CHECK(!s.listOpen);
    CHECK(s.systems.get(System::Wii) && s.systems.get(System::GameCube) && s.systems.get(System::N64));
    CHECK(!s.systems.get(System::NES));
}

TEST(ui_tab_change_resets_indices) {
    Library lib(uiFixture());
    UiState s;
    // Alle: "007 Quantum of Solace" (Wii) steht vorn, Zeile 2 ist ein N64-Spiel.
    openList(s);
    press(s, lib, Action::Down);
    press(s, lib, Action::Down);
    CHECK_EQ(s.listIndex, 2);
    CHECK_EQ(s.carouselIndex, 2);
    CHECK(lib.visible()[2]->system != System::Wii);
    s.focus = Focus::Tabs;
    press(s, lib, Action::Right);  // Alle -> Wii: Spiel nicht enthalten -> Index 0
    CHECK_EQ(s.tabIndex, 1);
    CHECK_EQ(s.listIndex, 0);
    CHECK_EQ(s.carouselIndex, 0);
    CHECK_EQ(s.letterIndex, 0);
    CHECK(s.listOpen);  // Tabwechsel laesst die Liste offen
    CHECK_EQ(lib.visible().size(), (size_t)12);
    for (auto g : lib.visible()) CHECK(g->system == System::Wii);
    // Tabs: Alle, Wii, GameCube, N64 -> rechts geklemmt bei 3
    for (int i = 0; i < 5; ++i) press(s, lib, Action::Right);
    CHECK_EQ(s.tabIndex, 3);
    CHECK_EQ(lib.visible().size(), (size_t)5);
    for (int i = 0; i < 5; ++i) press(s, lib, Action::Left);
    CHECK_EQ(s.tabIndex, 0);
    CHECK_EQ(lib.visible().size(), (size_t)17);
}

TEST(ui_tabs_down_and_a_go_to_carousel) {
    Library lib(uiFixture());
    UiState s;
    s.focus = Focus::Tabs;
    press(s, lib, Action::Down);
    CHECK(s.focus == Focus::Carousel);
    s.focus = Focus::Tabs;
    CHECK_EQ(press(s, lib, Action::A).kind, Command::None);
    CHECK(s.focus == Focus::Carousel);
    press(s, lib, Action::Down);
    CHECK(s.focus == Focus::List);
    CHECK(s.listOpen);
}

TEST(ui_list_a_launches_selected) {
    Library lib(uiFixture());
    UiState s;
    openList(s);
    press(s, lib, Action::Down);
    press(s, lib, Action::Down);
    press(s, lib, Action::Down);
    Command c = press(s, lib, Action::A);
    CHECK_EQ(c.kind, Command::Launch);
    CHECK(c.game == lib.visible()[3]);
    CHECK_EQ(s.carouselIndex, 3);
    CHECK(carouselGame(s, lib) == lib.visible()[3]);
}

TEST(ui_list_up_at_index_zero_goes_to_tabs) {
    Library lib(uiFixture());
    UiState s;
    openList(s);
    press(s, lib, Action::Down);
    press(s, lib, Action::Up);
    CHECK(s.focus == Focus::List);
    CHECK_EQ(s.listIndex, 0);
    press(s, lib, Action::Up);
    CHECK(s.focus == Focus::Tabs);
}

TEST(ui_list_up_on_first_row_of_page_goes_to_previous_page) {
    Library lib(uiFixture());  // 17 Spiele -> Seiten 10 + 7
    UiState s;
    openList(s);
    press(s, lib, Action::Two);
    CHECK_EQ(s.listIndex, 10);
    press(s, lib, Action::Up);  // Seite 2, Zeile 0 -> Seite 1, Zeile 9
    CHECK(s.focus == Focus::List);
    CHECK_EQ(s.listIndex, 9);
    CHECK_EQ(s.carouselIndex, 9);
    for (int i = 0; i < 9; ++i) press(s, lib, Action::Up);
    CHECK_EQ(s.listIndex, 0);
    CHECK(s.focus == Focus::List);
    press(s, lib, Action::Up);
    CHECK(s.focus == Focus::Tabs);
}

TEST(ui_list_down_crosses_page_and_stops_at_end) {
    Library lib(uiFixture());  // Alle: 17 Spiele -> Seiten 10 + 7
    UiState s;
    openList(s);
    for (int i = 0; i < 9; ++i) press(s, lib, Action::Down);
    CHECK_EQ(s.listIndex, 9);
    press(s, lib, Action::Down);  // letzte Zeile Seite 1 -> Seite 2, Zeile 0
    CHECK_EQ(s.listIndex, 10);
    CHECK_EQ(s.carouselIndex, 10);
    for (int i = 0; i < 20; ++i) press(s, lib, Action::Down);
    CHECK_EQ(s.listIndex, 16);
    press(s, lib, Action::One);
    CHECK_EQ(s.listIndex, 6);
    press(s, lib, Action::Two);
    CHECK_EQ(s.listIndex, 16);
}

TEST(ui_list_paging_clamps) {
    Library lib(uiFixture());
    UiState s;
    openList(s);
    press(s, lib, Action::One);
    CHECK_EQ(s.listIndex, 0);
    press(s, lib, Action::Down);
    press(s, lib, Action::Two);
    CHECK_EQ(s.listIndex, 11);
    press(s, lib, Action::Two);
    CHECK_EQ(s.listIndex, 11);
    press(s, lib, Action::One);
    CHECK_EQ(s.listIndex, 1);
    CHECK_EQ(s.carouselIndex, 1);
}

// Anordnung Karussell | A-Z | Liste: links/rechts bewegt den Fokus räumlich.
TEST(ui_left_right_follow_layout) {
    Library lib(uiFixture());
    UiState s;
    openList(s);
    press(s, lib, Action::Down);
    press(s, lib, Action::Right);  // rechte Spalte: nichts
    CHECK(s.focus == Focus::List);
    CHECK_EQ(s.listIndex, 1);
    press(s, lib, Action::Left);
    CHECK(s.focus == Focus::Letters);
    press(s, lib, Action::Right);
    CHECK(s.focus == Focus::List);
    press(s, lib, Action::Left);
    press(s, lib, Action::Left);  // A-Z -> Fächer, Liste zu
    CHECK(s.focus == Focus::Carousel);
    CHECK(!s.listOpen);
    CHECK_EQ(s.carouselIndex, 1);  // Fächer steht auf dem in der Liste gewaehlten Spiel
}

TEST(ui_letters_jump) {
    Library lib(uiFixture());
    UiState s;
    s.listOpen = true;
    s.focus = Focus::Letters;
    s.letterIndex = 0;
    std::vector<char> letters = lib.letters();
    int target = -1;
    for (size_t i = 0; i < letters.size(); ++i)
        if (letters[i] == 'S') target = static_cast<int>(i);
    CHECK(target > 0);
    for (int i = 0; i < target; ++i) press(s, lib, Action::Down);
    CHECK_EQ(s.letterIndex, target);
    press(s, lib, Action::A);
    CHECK(s.focus == Focus::List);
    CHECK_EQ(s.listIndex, lib.firstIndexForLetter('S'));
    CHECK_EQ(lib.visible()[s.listIndex]->letter, 'S');
    // Klemmen oben und unten
    s.focus = Focus::Letters;
    for (int i = 0; i < 40; ++i) press(s, lib, Action::Down);
    CHECK_EQ(s.letterIndex, (int)letters.size() - 1);
    for (int i = 0; i < 40; ++i) press(s, lib, Action::Up);
    CHECK_EQ(s.letterIndex, 0);
}

TEST(ui_carousel_moves_over_visible) {
    Library lib(uiFixture());
    UiState s;
    CHECK(carouselGame(s, lib) == lib.visible()[0]);
    press(s, lib, Action::Right);
    press(s, lib, Action::Right);
    press(s, lib, Action::Right);
    CHECK_EQ(s.carouselIndex, 3);
    press(s, lib, Action::Left);
    CHECK_EQ(s.carouselIndex, 2);
    Command c = press(s, lib, Action::A);
    CHECK_EQ(c.kind, Command::Launch);
    CHECK(c.game == lib.visible()[2]);
    for (int i = 0; i < 5; ++i) press(s, lib, Action::Left);
    CHECK_EQ(s.carouselIndex, 0);
    for (int i = 0; i < 30; ++i) press(s, lib, Action::Right);
    CHECK_EQ(s.carouselIndex, 16);
}



TEST(ui_search_mar_launches_first_result) {
    Library lib(uiFixture());
    UiState s;
    press(s, lib, Action::Minus);
    CHECK(s.overlay == Overlay::Search);
    // 'm' per Steuerkreuz: 1 -> 3x runter (y) -> 6x rechts (m)
    s.keyboardIndex = 0;
    CHECK_EQ(keyboardKeys()[0], std::string("1"));
    press(s, lib, Action::Down);
    press(s, lib, Action::Down);
    press(s, lib, Action::Down);
    for (int i = 0; i < 6; ++i) press(s, lib, Action::Right);
    CHECK_EQ(s.keyboardIndex, keyIndex("m"));
    press(s, lib, Action::A);
    s.keyboardIndex = keyIndex("a");
    press(s, lib, Action::A);
    s.keyboardIndex = keyIndex("r");
    press(s, lib, Action::A);
    CHECK_EQ(s.query, std::string("mar"));
    std::vector<const Game*> res = searchResults(s, lib);
    CHECK(!res.empty());
    CHECK(res == lib.search("mar", 8));
    press(s, lib, Action::Two);
    CHECK(s.searchInResults);
    CHECK_EQ(s.resultIndex, 0);
    Command c = press(s, lib, Action::A);
    CHECK_EQ(c.kind, Command::Launch);
    CHECK(c.game == res[0]);
}

TEST(ui_search_space_backspace_and_close) {
    Library lib(uiFixture());
    UiState s;
    press(s, lib, Action::Minus);
    click(s, lib, Region::Key, keyIndex("z"));
    click(s, lib, Region::Key, keyIndex("Leer"));
    CHECK_EQ(s.query, std::string("z "));
    click(s, lib, Region::Key, keyIndex("Löschen"));
    click(s, lib, Region::Key, keyIndex("Löschen"));
    click(s, lib, Region::Key, keyIndex("Löschen"));
    CHECK_EQ(s.query, std::string(""));
    press(s, lib, Action::B);
    CHECK(s.overlay == Overlay::None);
}

TEST(ui_keyboard_layout) {
    const auto& rows = keyboardRows();
    CHECK_EQ(rows.size(), (size_t)5);
    CHECK_EQ(rows[0], 10);
    CHECK_EQ(rows[1], 10);
    CHECK_EQ(rows[2], 9);
    CHECK_EQ(rows[3], 7);
    CHECK_EQ(rows[4], 2);
    int total = 0;
    for (int r : rows) total += r;
    CHECK_EQ(keyboardKeys().size(), (size_t)total);
    CHECK_EQ(keyboardKeys().size(), (size_t)38);
    CHECK_EQ(keyboardKeys()[0], std::string("1"));
    CHECK_EQ(keyboardKeys()[9], std::string("0"));
    CHECK_EQ(keyboardKeys()[10], std::string("q"));
    CHECK_EQ(keyboardKeys()[15], std::string("z"));
    CHECK_EQ(keyboardKeys()[29], std::string("y"));
    CHECK_EQ(keyIndex("Leer"), 36);
    CHECK_EQ(keyIndex("Löschen"), 37);
}

TEST(ui_keyboard_digit_row) {
    Library lib(uiFixture());
    UiState s;
    press(s, lib, Action::Minus);
    s.keyboardIndex = keyIndex("q");
    press(s, lib, Action::Up);
    CHECK_EQ(s.keyboardIndex, keyIndex("1"));
    press(s, lib, Action::Up);  // oberste Reihe: bleibt
    CHECK_EQ(s.keyboardIndex, keyIndex("1"));
    click(s, lib, Region::Key, keyIndex("0"));
    click(s, lib, Region::Key, keyIndex("0"));
    click(s, lib, Region::Key, keyIndex("7"));
    CHECK_EQ(s.query, std::string("007"));
    std::vector<const Game*> res = searchResults(s, lib);
    CHECK_EQ(res.size(), (size_t)2);  // 007 Quantum of Solace, GoldenEye 007
}

TEST(ui_search_umlaut_query_via_pointer) {
    std::vector<Game> v = uiFixture();
    v.push_back(mkGame(System::Wii, "Benjamin Bl\xc3\xbcmchen"));
    Library lib(std::move(v));
    UiState s;
    press(s, lib, Action::Minus);
    for (const char* k : {"b", "l", "u", "m"}) click(s, lib, Region::Key, keyIndex(k));
    std::vector<const Game*> res = searchResults(s, lib);
    CHECK_EQ(res.size(), (size_t)1);
    CHECK(!res.empty() && res[0]->title == "Benjamin Bl\xc3\xbcmchen");
}

TEST(ui_systems_toggle_nes) {
    Library lib(uiFixture());
    UiState s;
    press(s, lib, Action::Plus);
    CHECK(s.overlay == Overlay::Systems);
    for (int i = 0; i < 4; ++i) press(s, lib, Action::Down);  // Wii..NES
    CHECK_EQ(s.systemsIndex, static_cast<int>(System::NES));
    Command c = press(s, lib, Action::A);
    CHECK_EQ(c.kind, Command::ToggleSystem);
    CHECK(c.system == System::NES);
    CHECK(s.systems.get(System::NES));
    bool hasNes = false;
    for (System t : lib.tabs()) hasNes = hasNes || t == System::NES;
    CHECK(hasNes);
    CHECK_EQ(lib.visible().size(), (size_t)22);
    press(s, lib, Action::Plus);
    CHECK(s.overlay == Overlay::None);
}

TEST(ui_systems_disable_current_tab_resets_to_all) {
    Library lib(uiFixture());
    UiState s;
    s.focus = Focus::Tabs;
    press(s, lib, Action::Right);
    press(s, lib, Action::Right);
    press(s, lib, Action::Right);  // N64
    CHECK_EQ(s.tabIndex, 3);
    press(s, lib, Action::Plus);
    click(s, lib, Region::SystemToggle, static_cast<int>(System::N64));
    CHECK(!s.systems.get(System::N64));
    CHECK_EQ(s.tabIndex, 0);
    CHECK_EQ(lib.visible().size(), (size_t)12);
}

TEST(ui_home_wii_menu) {
    Library lib(uiFixture());
    UiState s;
    press(s, lib, Action::Home);
    CHECK(s.overlay == Overlay::Home);
    CHECK_EQ(homeEntries().size(), (size_t)3);
    CHECK_EQ(press(s, lib, Action::A).kind, Command::WiiMenu);
    press(s, lib, Action::Down);
    CHECK_EQ(press(s, lib, Action::A).kind, Command::Hbc);
    press(s, lib, Action::Down);
    press(s, lib, Action::Down);
    CHECK_EQ(press(s, lib, Action::A).kind, Command::PowerOff);
    press(s, lib, Action::Home);
    CHECK(s.overlay == Overlay::None);
    press(s, lib, Action::Home);
    press(s, lib, Action::B);
    CHECK(s.overlay == Overlay::None);
}

TEST(ui_pointer_list_row_second_click_launches) {
    Library lib(uiFixture());
    UiState s;
    openList(s);
    press(s, lib, Action::Two);  // Seite 2
    Command c = click(s, lib, Region::ListRow, 3);
    CHECK_EQ(c.kind, Command::None);
    CHECK_EQ(s.listIndex, 13);
    CHECK(s.focus == Focus::List);
    CHECK_EQ(s.carouselIndex, 13);
    c = click(s, lib, Region::ListRow, 3);
    CHECK_EQ(c.kind, Command::Launch);
    CHECK(c.game == lib.visible()[13]);
    // Zeile ausserhalb der Seite: nichts
    c = click(s, lib, Region::ListRow, 9);
    CHECK_EQ(c.kind, Command::None);
    CHECK_EQ(s.listIndex, 13);
}

TEST(ui_pointer_first_click_from_other_focus_only_selects) {
    Library lib(uiFixture());
    UiState s;
    s.listOpen = true;  // Fokus Fächer, listIndex 0
    Command c = click(s, lib, Region::ListRow, 0);
    CHECK_EQ(c.kind, Command::None);
    CHECK(s.focus == Focus::List);
}

TEST(ui_pointer_other_regions) {
    Library lib(uiFixture());
    UiState s;
    click(s, lib, Region::Tab, 1);
    CHECK_EQ(s.tabIndex, 1);
    CHECK_EQ(lib.visible().size(), (size_t)12);
    click(s, lib, Region::CarouselRight, 1);
    CHECK_EQ(s.carouselIndex, 1);
    click(s, lib, Region::CarouselRight, 2);  // zweite Stufe: Abstand 2
    CHECK_EQ(s.carouselIndex, 3);
    Command c = click(s, lib, Region::CarouselCenter, 0);
    CHECK_EQ(c.kind, Command::Launch);
    CHECK(c.game == lib.visible()[3]);
    click(s, lib, Region::CarouselLeft, 2);
    CHECK_EQ(s.carouselIndex, 1);
    click(s, lib, Region::CarouselLeft, 1);
    CHECK_EQ(s.carouselIndex, 0);
    click(s, lib, Region::CarouselRight, 7);  // kein gueltiger Abstand: nichts
    CHECK_EQ(s.carouselIndex, 0);
    s.listOpen = true;
    click(s, lib, Region::Letter, 1);
    CHECK(s.focus == Focus::List);
    CHECK_EQ(s.listIndex, lib.firstIndexForLetter(lib.letters()[1]));
    CHECK_EQ(s.carouselIndex, s.listIndex);
    click(s, lib, Region::SearchField, 0);
    CHECK(s.overlay == Overlay::Search);
    click(s, lib, Region::Key, keyIndex("w"));
    click(s, lib, Region::Key, keyIndex("i"));
    std::vector<const Game*> res = searchResults(s, lib);
    CHECK(!res.empty());
    c = click(s, lib, Region::Result, 0);
    CHECK_EQ(c.kind, Command::Launch);
    CHECK(c.game == res[0]);
    press(s, lib, Action::B);
    press(s, lib, Action::Home);
    c = click(s, lib, Region::HomeEntry, 2);
    CHECK_EQ(c.kind, Command::PowerOff);
    press(s, lib, Action::B);
    c = click(s, lib, Region::Retry, 0);
    CHECK_EQ(c.kind, Command::Retry);
}

TEST(hoverListRowDoesNotSelect) {
    Library lib(uiFixture());
    UiState s;
    s.listOpen = true;
    pointerHover(s, lib, std::make_pair(static_cast<int>(Region::ListRow), 4));
    pointerHover(s, lib, std::make_pair(static_cast<int>(Region::Letter), 2));
    CHECK_EQ(s.listIndex, 0);
    CHECK_EQ(s.carouselIndex, 0);
    CHECK_EQ(s.letterIndex, 0);
    CHECK(s.focus == Focus::Carousel);
}

TEST(ui_empty_library_is_safe) {
    Library lib(std::vector<Game>{});
    const Action actions[] = {Action::Up,   Action::Down,  Action::Left, Action::Right,
                              Action::A,    Action::B,     Action::One,  Action::Two};
    const Focus focuses[] = {Focus::Tabs, Focus::Carousel, Focus::List, Focus::Letters};
    const Overlay overlays[] = {Overlay::None, Overlay::Systems, Overlay::Search, Overlay::Home};
    UiState s;
    CHECK(carouselGame(s, lib) == nullptr);
    for (Overlay o : overlays) {
        for (Focus f : focuses) {
            for (Action a : actions) {
                s = UiState();
                s.overlay = o;
                s.focus = f;
                s.listOpen = f == Focus::List || f == Focus::Letters;
                Command c = press(s, lib, a);
                if (o != Overlay::Systems && o != Overlay::Home && o != Overlay::Search)
                    CHECK(c.kind != Command::Launch);
                CHECK(carouselGame(s, lib) == nullptr);
            }
            for (int r = 0; r <= static_cast<int>(Region::Retry); ++r) {
                for (int idx : {-1, 0, 1, 50}) {
                    s = UiState();
                    s.overlay = o;
                    s.focus = f;
                    Command c = click(s, lib, static_cast<Region>(r), idx);
                    CHECK(c.kind != Command::Launch);
                    pointerHover(s, lib, std::make_pair(r, idx));
                }
            }
        }
    }
    s = UiState();
    press(s, lib, Action::Plus);
    press(s, lib, Action::Minus);
    press(s, lib, Action::Home);
    CHECK(carouselGame(s, lib) == nullptr);
    CHECK(searchResults(s, lib).empty());
}

TEST(ui_tab_click_focuses_carousel) {
    Library lib(uiFixture());
    UiState s;
    s.focus = Focus::Tabs;
    click(s, lib, Region::Tab, 1);
    CHECK_EQ(s.tabIndex, 1);
    CHECK(s.focus == Focus::Carousel);
    CHECK_EQ(s.carouselIndex, 0);
    press(s, lib, Action::Right);  // Steuerkreuz blaettert jetzt Cover, nicht Tabs
    CHECK_EQ(s.tabIndex, 1);
    CHECK_EQ(s.carouselIndex, 1);
}

TEST(ui_out_of_range_pointer_ignored) {
    Library lib(uiFixture());
    UiState s;
    click(s, lib, Region::Tab, 99);
    CHECK_EQ(s.tabIndex, 0);
    click(s, lib, Region::Letter, -3);
    CHECK(s.focus == Focus::Carousel);
    Command c = apply(s, lib, Action::PointerClick, std::make_pair(999, 0));
    CHECK_EQ(c.kind, Command::None);
}

TEST(clickSelectsThenClickStarts) {
    Library lib(uiFixture());
    UiState s;
    s.listOpen = true;
    Command c = click(s, lib, Region::ListRow, 3);
    CHECK_EQ(c.kind, Command::None);
    CHECK_EQ(s.listIndex, 3);
    CHECK_EQ(s.carouselIndex, 3);
    CHECK(s.focus == Focus::List);
    c = click(s, lib, Region::ListRow, 5);  // andere Zeile: nur Auswahl
    CHECK_EQ(c.kind, Command::None);
    CHECK_EQ(s.listIndex, 5);
    c = click(s, lib, Region::ListRow, 5);
    CHECK_EQ(c.kind, Command::Launch);
    CHECK(c.game == lib.visible()[5]);
}

TEST(ui_no_usb_only_retry_and_home) {
    Library lib(uiFixture());
    const Focus focuses[] = {Focus::Tabs, Focus::Carousel, Focus::List, Focus::Letters};
    for (Focus f : focuses) {
        UiState s;
        s.noUsb = true;
        s.listOpen = true;
        s.focus = f;
        CHECK_EQ(press(s, lib, Action::A).kind, Command::Retry);
    }
    UiState s;
    s.noUsb = true;
    openList(s);
    const Action ignored[] = {Action::Up, Action::Down, Action::Left, Action::Right, Action::B,
                              Action::Plus, Action::Minus, Action::One, Action::Two};
    for (Action a : ignored) {
        CHECK_EQ(press(s, lib, a).kind, Command::None);
        CHECK(s.overlay == Overlay::None);
        CHECK(s.focus == Focus::List);
        CHECK_EQ(s.listIndex, 0);
        CHECK_EQ(s.tabIndex, 0);
    }
    CHECK_EQ(click(s, lib, Region::ListRow, 2).kind, Command::None);
    CHECK_EQ(s.listIndex, 0);
    CHECK_EQ(click(s, lib, Region::Retry, 0).kind, Command::Retry);
    press(s, lib, Action::Home);
    CHECK(s.overlay == Overlay::Home);
    press(s, lib, Action::Down);
    CHECK_EQ(press(s, lib, Action::A).kind, Command::Hbc);
    press(s, lib, Action::B);
    CHECK(s.overlay == Overlay::None);
    CHECK_EQ(press(s, lib, Action::A).kind, Command::Retry);
}

TEST(ui_list_selection_moves_carousel) {
    Library lib(uiFixture());
    UiState s;
    openList(s);
    press(s, lib, Action::Down);
    CHECK_EQ(s.carouselIndex, 1);
    CHECK(carouselGame(s, lib) == lib.visible()[1]);
    press(s, lib, Action::Two);  // Seite 2
    CHECK_EQ(s.listIndex, 11);
    CHECK_EQ(s.carouselIndex, 11);
    press(s, lib, Action::Left);  // A-Z
    press(s, lib, Action::A);     // Buchstabensprung
    CHECK_EQ(s.carouselIndex, s.listIndex);
    click(s, lib, Region::ListRow, 0);
    CHECK_EQ(s.carouselIndex, s.listIndex);
    CHECK(carouselGame(s, lib) == lib.visible()[s.listIndex]);
    // Liste zu: Fächer bleibt auf dem Spiel und blaettert von dort weiter
    int at = s.carouselIndex;
    press(s, lib, Action::B);
    CHECK(!s.listOpen);
    press(s, lib, Action::Right);
    CHECK_EQ(s.carouselIndex, at + 1);
}

TEST(ui_keyboard_down_from_bottom_row_enters_results) {
    Library lib(uiFixture());
    UiState s;
    press(s, lib, Action::Minus);
    s.keyboardIndex = keyIndex("Leer");
    press(s, lib, Action::Down);  // keine Treffer: bleibt
    CHECK(!s.searchInResults);
    CHECK_EQ(s.keyboardIndex, keyIndex("Leer"));
    s.query = "mar";
    s.keyboardIndex = keyIndex("L\xc3\xb6schen");
    press(s, lib, Action::Down);
    CHECK(s.searchInResults);
    CHECK_EQ(s.resultIndex, 0);
}

TEST(ui_systems_agree_with_library) {
    Library lib(uiFixture());
    UiState s;
    lib.setEnabled(s.systems);
    press(s, lib, Action::Plus);
    click(s, lib, Region::SystemToggle, static_cast<int>(System::NES));
    click(s, lib, Region::SystemToggle, static_cast<int>(System::Wii));
    press(s, lib, Action::Down);
    press(s, lib, Action::A);
    for (int i = 0; i < 7; ++i) CHECK_EQ(s.systems.enabled[i], lib.enabled().enabled[i]);
    CHECK(s.systems.get(System::NES));
    CHECK(!s.systems.get(System::Wii));
}

TEST(ui_tab_renumbered_when_earlier_system_disabled) {
    Library lib(uiFixture());
    UiState s;
    click(s, lib, Region::Tab, 3);  // Alle, Wii, GameCube, N64
    CHECK_EQ(s.tabIndex, 3);
    press(s, lib, Action::Plus);
    click(s, lib, Region::SystemToggle, static_cast<int>(System::GameCube));
    CHECK_EQ(s.tabIndex, 2);  // Alle, Wii, N64
    CHECK(lib.tabs()[s.tabIndex - 1] == System::N64);
    CHECK_EQ(lib.visible().size(), (size_t)5);
}

TEST(ui_result_index_clamps_when_results_shrink) {
    Library lib(uiFixture());
    UiState s;
    press(s, lib, Action::Minus);
    s.query = "mario";
    size_t before = searchResults(s, lib).size();
    CHECK(before > 2);
    press(s, lib, Action::Two);
    for (size_t i = 0; i < before; ++i) press(s, lib, Action::Down);
    CHECK_EQ(s.resultIndex, (int)before - 1);
    click(s, lib, Region::Key, keyIndex("Leer"));
    click(s, lib, Region::Key, keyIndex("k"));
    size_t after = searchResults(s, lib).size();
    CHECK(after > 0 && after < before);
    CHECK_EQ(s.resultIndex, (int)after - 1);
    // Loeschen im Treffer-Modus (per Zeiger) laesst den Index gueltig
    press(s, lib, Action::Two);
    click(s, lib, Region::Key, keyIndex("L\xc3\xb6schen"));
    CHECK(s.resultIndex >= 0 && s.resultIndex < (int)searchResults(s, lib).size());
}

// Home ersetzt die Suche; B schliesst Home zum Hauptbildschirm, nicht zurueck in die Suche.
TEST(ui_home_from_search_then_b_returns_to_main) {
    Library lib(uiFixture());
    UiState s;
    press(s, lib, Action::Minus);
    press(s, lib, Action::Home);
    CHECK(s.overlay == Overlay::Home);
    press(s, lib, Action::B);
    CHECK(s.overlay == Overlay::None);
}

TEST(ui_click_none_closes_overlay) {
    Library lib(uiFixture());
    UiState s;
    const Action openers[] = {Action::Plus, Action::Minus, Action::Home};
    for (Action o : openers) {
        press(s, lib, o);
        CHECK(s.overlay != Overlay::None);
        CHECK_EQ(click(s, lib, Region::None, 0).kind, Command::None);
        CHECK(s.overlay == Overlay::None);
    }
}

// --- Fächer und Listen-Einblendung --------------------------------------------------------

TEST(downOpensListAtCarouselGame) {
    Library lib(uiFixture());
    UiState s;
    for (int i = 0; i < 12; ++i) press(s, lib, Action::Right);
    CHECK_EQ(s.carouselIndex, 12);
    press(s, lib, Action::Down);
    CHECK(s.listOpen);
    CHECK(s.focus == Focus::List);
    CHECK_EQ(s.listIndex, 12);
    CHECK_EQ(lib.letters()[s.letterIndex], lib.visible()[12]->letter);
    CHECK(carouselGame(s, lib) == lib.visible()[12]);
}

TEST(bClosesListKeepsIndex) {
    Library lib(uiFixture());
    UiState s;
    press(s, lib, Action::Down);
    press(s, lib, Action::Down);
    press(s, lib, Action::Down);
    CHECK_EQ(s.listIndex, 2);
    press(s, lib, Action::B);
    CHECK(!s.listOpen);
    CHECK(s.focus == Focus::Carousel);
    CHECK_EQ(s.carouselIndex, 2);
    // B in der Buchstabenspalte schliesst ebenso
    press(s, lib, Action::Down);
    press(s, lib, Action::Left);
    CHECK(s.focus == Focus::Letters);
    press(s, lib, Action::B);
    CHECK(!s.listOpen);
    CHECK(s.focus == Focus::Carousel);
    CHECK_EQ(s.carouselIndex, 2);
    // B im Fächer: nichts
    press(s, lib, Action::B);
    CHECK(!s.listOpen);
    CHECK(s.focus == Focus::Carousel);
}

TEST(twoInCarouselJumpsTen) {
    Library lib(uiFixture());  // 17 Spiele
    UiState s;
    press(s, lib, Action::Two);
    CHECK_EQ(s.carouselIndex, 10);
    CHECK(!s.listOpen);
    press(s, lib, Action::Two);
    CHECK_EQ(s.carouselIndex, 16);
    press(s, lib, Action::One);
    CHECK_EQ(s.carouselIndex, 6);
    press(s, lib, Action::One);
    CHECK_EQ(s.carouselIndex, 0);
    CHECK(s.focus == Focus::Carousel);
}

TEST(tabChangeKeepsGameWhenVisible) {
    Library lib(uiFixture());
    UiState s;
    int wiiIndex = -1;
    for (size_t i = 0; i < lib.visible().size(); ++i)
        if (lib.visible()[i]->title == "Wii Sports") wiiIndex = static_cast<int>(i);
    CHECK(wiiIndex >= 0);
    s.carouselIndex = wiiIndex;
    const Game* g = carouselGame(s, lib);
    click(s, lib, Region::Tab, 1);  // Wii
    CHECK(carouselGame(s, lib) == g);
    CHECK_EQ(s.listIndex, s.carouselIndex);
    click(s, lib, Region::Tab, 0);  // zurueck zu Alle
    CHECK(carouselGame(s, lib) == g);
    CHECK_EQ(s.carouselIndex, wiiIndex);
    click(s, lib, Region::Tab, 3);  // N64: nicht enthalten -> 0
    CHECK_EQ(s.carouselIndex, 0);

    // Systemwechsel: Spiel bleibt, wenn sein System eingeschaltet bleibt
    click(s, lib, Region::Tab, 0);
    s.carouselIndex = wiiIndex;
    s.overlay = Overlay::Systems;
    click(s, lib, Region::SystemToggle, static_cast<int>(System::N64));
    CHECK(carouselGame(s, lib) == g);
    click(s, lib, Region::SystemToggle, static_cast<int>(System::Wii));
    CHECK(carouselGame(s, lib) != g);
    CHECK_EQ(s.carouselIndex, 0);
}

TEST(listToggleClickOpensAndCloses) {
    Library lib(uiFixture());
    UiState s;
    s.carouselIndex = 5;
    click(s, lib, Region::ListToggle, 0);
    CHECK(s.listOpen);
    CHECK(s.focus == Focus::List);
    CHECK_EQ(s.listIndex, 5);
    click(s, lib, Region::ListToggle, 0);
    CHECK(!s.listOpen);
    CHECK(s.focus == Focus::Carousel);
    CHECK_EQ(s.carouselIndex, 5);
}

TEST(noneClickClosesList) {
    Library lib(uiFixture());
    UiState s;
    click(s, lib, Region::None, 0);  // Liste zu: nichts
    CHECK(!s.listOpen);
    press(s, lib, Action::Down);
    CHECK(s.listOpen);
    click(s, lib, Region::None, 0);
    CHECK(!s.listOpen);
    CHECK(s.focus == Focus::Carousel);
    // Klick auf die Mitte startet auch bei offener Liste
    press(s, lib, Action::Down);
    Command c = click(s, lib, Region::CarouselCenter, 0);
    CHECK_EQ(c.kind, Command::Launch);
    CHECK(c.game == lib.visible()[s.carouselIndex]);
    // Overlay ueber offener Liste: Klick ins Leere schliesst nur das Overlay
    s.listOpen = true;
    press(s, lib, Action::Plus);
    click(s, lib, Region::None, 0);
    CHECK(s.overlay == Overlay::None);
    CHECK(s.listOpen);
}

TEST(normalizeClosesFocusListWhenListClosed) {
    Library lib(uiFixture());
    UiState s;
    s.focus = Focus::List;
    press(s, lib, Action::Right);  // Fokus Liste ohne offene Liste -> Fächer
    CHECK(s.focus == Focus::Carousel);
    CHECK_EQ(s.carouselIndex, 1);
    s.focus = Focus::Letters;
    press(s, lib, Action::Left);
    CHECK(s.focus == Focus::Carousel);
    // Zeilen und Buchstaben wirken nur bei offener Liste
    click(s, lib, Region::ListRow, 3);
    CHECK_EQ(s.listIndex, 0);
    CHECK(s.focus == Focus::Carousel);
    pointerHover(s, lib, std::make_pair(static_cast<int>(Region::Letter), 2));
    CHECK(s.focus == Focus::Carousel);
    // Indizes werden auf visible() geklemmt
    s.carouselIndex = 99;
    s.listIndex = 99;
    press(s, lib, Action::B);
    CHECK_EQ(s.carouselIndex, 16);
    CHECK_EQ(s.listIndex, 16);
}

TEST(emptyLibraryOpenCloseSafe) {
    Library lib(std::vector<Game>{});
    UiState s;
    press(s, lib, Action::Down);
    CHECK(s.listOpen);
    CHECK(carouselGame(s, lib) == nullptr);
    CHECK_EQ(press(s, lib, Action::A).kind, Command::None);
    press(s, lib, Action::Left);
    press(s, lib, Action::B);
    CHECK(!s.listOpen);
    click(s, lib, Region::ListToggle, 0);
    CHECK(s.listOpen);
    click(s, lib, Region::None, 0);
    CHECK(!s.listOpen);
    press(s, lib, Action::Two);
    press(s, lib, Action::Right);
    CHECK_EQ(s.carouselIndex, 0);
    CHECK(carouselGame(s, lib) == nullptr);
}
