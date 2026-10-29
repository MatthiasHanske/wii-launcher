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

Command press(UiState& s, Library& lib, const std::vector<const Game*>& recent, Action a) {
    return apply(s, lib, recent, a, std::nullopt);
}

Command click(UiState& s, Library& lib, const std::vector<const Game*>& recent, Region r, int idx) {
    return apply(s, lib, recent, Action::PointerClick, std::make_pair(static_cast<int>(r), idx));
}

int keyIndex(const std::string& label) {
    const auto& keys = keyboardKeys();
    for (size_t i = 0; i < keys.size(); ++i)
        if (keys[i] == label) return static_cast<int>(i);
    return -1;
}

const std::vector<const Game*> kNoRecent;

}  // namespace

TEST(ui_defaults) {
    UiState s;
    CHECK(s.focus == Focus::Carousel);
    CHECK(s.overlay == Overlay::None);
    CHECK_EQ(s.tabIndex, 0);
    CHECK_EQ(s.listIndex, 0);
    CHECK(s.carouselPreview == nullptr);
    CHECK(s.systems.get(System::Wii) && s.systems.get(System::GameCube) && s.systems.get(System::N64));
    CHECK(!s.systems.get(System::NES));
}

TEST(ui_tab_change_resets_indices) {
    Library lib(uiFixture());
    UiState s;
    s.focus = Focus::List;
    press(s, lib, kNoRecent, Action::Down);
    press(s, lib, kNoRecent, Action::Down);
    s.letterIndex = 3;
    CHECK_EQ(s.listIndex, 2);
    CHECK(s.carouselPreview != nullptr);
    s.focus = Focus::Tabs;
    press(s, lib, kNoRecent, Action::Right);  // Alle -> Wii
    CHECK_EQ(s.tabIndex, 1);
    CHECK_EQ(s.listIndex, 0);
    CHECK_EQ(s.letterIndex, 0);
    CHECK(s.carouselPreview == nullptr);
    CHECK_EQ(lib.visible().size(), (size_t)12);
    for (auto g : lib.visible()) CHECK(g->system == System::Wii);
    // Tabs: Alle, Wii, GameCube, N64 -> rechts geklemmt bei 3
    for (int i = 0; i < 5; ++i) press(s, lib, kNoRecent, Action::Right);
    CHECK_EQ(s.tabIndex, 3);
    CHECK_EQ(lib.visible().size(), (size_t)5);
    for (int i = 0; i < 5; ++i) press(s, lib, kNoRecent, Action::Left);
    CHECK_EQ(s.tabIndex, 0);
    CHECK_EQ(lib.visible().size(), (size_t)17);
}

TEST(ui_tabs_down_and_a_go_to_carousel) {
    Library lib(uiFixture());
    UiState s;
    s.focus = Focus::Tabs;
    press(s, lib, kNoRecent, Action::Down);
    CHECK(s.focus == Focus::Carousel);
    s.focus = Focus::Tabs;
    CHECK_EQ(press(s, lib, kNoRecent, Action::A).kind, Command::None);
    CHECK(s.focus == Focus::Carousel);
    press(s, lib, kNoRecent, Action::Down);
    CHECK(s.focus == Focus::List);
}

TEST(ui_list_a_launches_selected) {
    Library lib(uiFixture());
    UiState s;
    s.focus = Focus::List;
    press(s, lib, kNoRecent, Action::Down);
    press(s, lib, kNoRecent, Action::Down);
    press(s, lib, kNoRecent, Action::Down);
    Command c = press(s, lib, kNoRecent, Action::A);
    CHECK_EQ(c.kind, Command::Launch);
    CHECK(c.game == lib.visible()[3]);
    CHECK(s.carouselPreview == lib.visible()[3]);
    CHECK(carouselGame(s, kNoRecent, lib) == lib.visible()[3]);
}

TEST(ui_list_up_on_first_row_goes_to_tabs) {
    Library lib(uiFixture());
    UiState s;
    s.focus = Focus::List;
    press(s, lib, kNoRecent, Action::Down);
    press(s, lib, kNoRecent, Action::Up);
    CHECK(s.focus == Focus::List);
    CHECK_EQ(s.listIndex, 0);
    press(s, lib, kNoRecent, Action::Up);
    CHECK(s.focus == Focus::Tabs);
    // Auch auf Seite 2, Zeile 0
    s.focus = Focus::List;
    press(s, lib, kNoRecent, Action::Two);
    CHECK_EQ(s.listIndex, 10);
    press(s, lib, kNoRecent, Action::Up);
    CHECK(s.focus == Focus::Tabs);
}

TEST(ui_list_down_stays_on_page_and_at_end) {
    Library lib(uiFixture());  // Alle: 17 Spiele -> Seiten 10 + 7
    UiState s;
    s.focus = Focus::List;
    for (int i = 0; i < 15; ++i) press(s, lib, kNoRecent, Action::Down);
    CHECK_EQ(s.listIndex, 9);
    press(s, lib, kNoRecent, Action::Two);
    CHECK_EQ(s.listIndex, 16);  // Zeile 9 gibt es auf Seite 2 nicht -> letzter Eintrag
    press(s, lib, kNoRecent, Action::Down);
    CHECK_EQ(s.listIndex, 16);
}

TEST(ui_list_paging_clamps) {
    Library lib(uiFixture());
    UiState s;
    s.focus = Focus::List;
    press(s, lib, kNoRecent, Action::One);
    CHECK_EQ(s.listIndex, 0);
    press(s, lib, kNoRecent, Action::Down);
    press(s, lib, kNoRecent, Action::Two);
    CHECK_EQ(s.listIndex, 11);
    press(s, lib, kNoRecent, Action::Two);
    CHECK_EQ(s.listIndex, 11);
    press(s, lib, kNoRecent, Action::One);
    CHECK_EQ(s.listIndex, 1);
    CHECK(s.carouselPreview == lib.visible()[1]);
}

TEST(ui_list_left_right_switch_focus) {
    Library lib(uiFixture());
    UiState s;
    s.focus = Focus::List;
    press(s, lib, kNoRecent, Action::Left);
    CHECK(s.focus == Focus::Letters);
    s.focus = Focus::List;
    press(s, lib, kNoRecent, Action::Right);
    CHECK(s.focus == Focus::Carousel);
}

TEST(ui_letters_jump) {
    Library lib(uiFixture());
    UiState s;
    s.focus = Focus::Letters;
    s.letterIndex = 0;
    std::vector<char> letters = lib.letters();
    int target = -1;
    for (size_t i = 0; i < letters.size(); ++i)
        if (letters[i] == 'S') target = static_cast<int>(i);
    CHECK(target > 0);
    for (int i = 0; i < target; ++i) press(s, lib, kNoRecent, Action::Down);
    CHECK_EQ(s.letterIndex, target);
    press(s, lib, kNoRecent, Action::A);
    CHECK(s.focus == Focus::List);
    CHECK_EQ(s.listIndex, lib.firstIndexForLetter('S'));
    CHECK_EQ(lib.visible()[s.listIndex]->letter, 'S');
    // Klemmen oben und unten
    s.focus = Focus::Letters;
    for (int i = 0; i < 40; ++i) press(s, lib, kNoRecent, Action::Down);
    CHECK_EQ(s.letterIndex, (int)letters.size() - 1);
    for (int i = 0; i < 40; ++i) press(s, lib, kNoRecent, Action::Up);
    CHECK_EQ(s.letterIndex, 0);
}

TEST(ui_carousel_moves_and_launches_recent) {
    Library lib(uiFixture());
    std::vector<const Game*> recent = {lib.visible()[5], lib.visible()[2], lib.visible()[7]};
    UiState s;
    CHECK(carouselGame(s, recent, lib) == recent[0]);
    press(s, lib, recent, Action::Right);
    press(s, lib, recent, Action::Right);
    press(s, lib, recent, Action::Right);
    CHECK_EQ(s.carouselIndex, 2);
    press(s, lib, recent, Action::Left);
    CHECK_EQ(s.carouselIndex, 1);
    Command c = press(s, lib, recent, Action::A);
    CHECK_EQ(c.kind, Command::Launch);
    CHECK(c.game == recent[1]);
    for (int i = 0; i < 5; ++i) press(s, lib, recent, Action::Left);
    CHECK_EQ(s.carouselIndex, 0);
}

TEST(ui_carousel_falls_back_to_visible) {
    Library lib(uiFixture());
    UiState s;
    CHECK(carouselGame(s, kNoRecent, lib) == lib.visible()[0]);
    press(s, lib, kNoRecent, Action::Right);
    CHECK(carouselGame(s, kNoRecent, lib) == lib.visible()[1]);
}

TEST(ui_search_mar_launches_first_result) {
    Library lib(uiFixture());
    UiState s;
    press(s, lib, kNoRecent, Action::Minus);
    CHECK(s.overlay == Overlay::Search);
    // 'm' per Steuerkreuz: q -> 2x runter (y) -> 6x rechts (m)
    s.keyboardIndex = 0;
    press(s, lib, kNoRecent, Action::Down);
    press(s, lib, kNoRecent, Action::Down);
    for (int i = 0; i < 6; ++i) press(s, lib, kNoRecent, Action::Right);
    CHECK_EQ(s.keyboardIndex, keyIndex("m"));
    press(s, lib, kNoRecent, Action::A);
    s.keyboardIndex = keyIndex("a");
    press(s, lib, kNoRecent, Action::A);
    s.keyboardIndex = keyIndex("r");
    press(s, lib, kNoRecent, Action::A);
    CHECK_EQ(s.query, std::string("mar"));
    std::vector<const Game*> res = searchResults(s, lib);
    CHECK(!res.empty());
    CHECK(res == lib.search("mar", 8));
    press(s, lib, kNoRecent, Action::Two);
    CHECK(s.searchInResults);
    CHECK_EQ(s.resultIndex, 0);
    Command c = press(s, lib, kNoRecent, Action::A);
    CHECK_EQ(c.kind, Command::Launch);
    CHECK(c.game == res[0]);
}

TEST(ui_search_space_backspace_and_close) {
    Library lib(uiFixture());
    UiState s;
    press(s, lib, kNoRecent, Action::Minus);
    click(s, lib, kNoRecent, Region::Key, keyIndex("z"));
    click(s, lib, kNoRecent, Region::Key, keyIndex("Leer"));
    CHECK_EQ(s.query, std::string("z "));
    click(s, lib, kNoRecent, Region::Key, keyIndex("Löschen"));
    click(s, lib, kNoRecent, Region::Key, keyIndex("Löschen"));
    click(s, lib, kNoRecent, Region::Key, keyIndex("Löschen"));
    CHECK_EQ(s.query, std::string(""));
    press(s, lib, kNoRecent, Action::B);
    CHECK(s.overlay == Overlay::None);
}

TEST(ui_keyboard_layout) {
    const auto& rows = keyboardRows();
    CHECK_EQ(rows.size(), (size_t)4);
    CHECK_EQ(rows[0], 10);
    CHECK_EQ(rows[1], 9);
    CHECK_EQ(rows[2], 7);
    CHECK_EQ(rows[3], 2);
    CHECK_EQ(keyboardKeys().size(), (size_t)28);
    CHECK_EQ(keyboardKeys()[0], std::string("q"));
    CHECK_EQ(keyboardKeys()[5], std::string("z"));
    CHECK_EQ(keyboardKeys()[19], std::string("y"));
    CHECK_EQ(keyIndex("Leer"), 26);
    CHECK_EQ(keyIndex("Löschen"), 27);
}

TEST(ui_systems_toggle_nes) {
    Library lib(uiFixture());
    UiState s;
    press(s, lib, kNoRecent, Action::Plus);
    CHECK(s.overlay == Overlay::Systems);
    for (int i = 0; i < 4; ++i) press(s, lib, kNoRecent, Action::Down);  // Wii..NES
    CHECK_EQ(s.systemsIndex, static_cast<int>(System::NES));
    Command c = press(s, lib, kNoRecent, Action::A);
    CHECK_EQ(c.kind, Command::ToggleSystem);
    CHECK(c.system == System::NES);
    CHECK(s.systems.get(System::NES));
    bool hasNes = false;
    for (System t : lib.tabs()) hasNes = hasNes || t == System::NES;
    CHECK(hasNes);
    CHECK_EQ(lib.visible().size(), (size_t)22);
    press(s, lib, kNoRecent, Action::Plus);
    CHECK(s.overlay == Overlay::None);
}

TEST(ui_systems_disable_current_tab_resets_to_all) {
    Library lib(uiFixture());
    UiState s;
    s.focus = Focus::Tabs;
    press(s, lib, kNoRecent, Action::Right);
    press(s, lib, kNoRecent, Action::Right);
    press(s, lib, kNoRecent, Action::Right);  // N64
    CHECK_EQ(s.tabIndex, 3);
    press(s, lib, kNoRecent, Action::Plus);
    click(s, lib, kNoRecent, Region::SystemToggle, static_cast<int>(System::N64));
    CHECK(!s.systems.get(System::N64));
    CHECK_EQ(s.tabIndex, 0);
    CHECK_EQ(lib.visible().size(), (size_t)12);
}

TEST(ui_home_wii_menu) {
    Library lib(uiFixture());
    UiState s;
    press(s, lib, kNoRecent, Action::Home);
    CHECK(s.overlay == Overlay::Home);
    CHECK_EQ(homeEntries().size(), (size_t)3);
    CHECK_EQ(press(s, lib, kNoRecent, Action::A).kind, Command::WiiMenu);
    press(s, lib, kNoRecent, Action::Down);
    CHECK_EQ(press(s, lib, kNoRecent, Action::A).kind, Command::Hbc);
    press(s, lib, kNoRecent, Action::Down);
    press(s, lib, kNoRecent, Action::Down);
    CHECK_EQ(press(s, lib, kNoRecent, Action::A).kind, Command::PowerOff);
    press(s, lib, kNoRecent, Action::Home);
    CHECK(s.overlay == Overlay::None);
    press(s, lib, kNoRecent, Action::Home);
    press(s, lib, kNoRecent, Action::B);
    CHECK(s.overlay == Overlay::None);
}

TEST(ui_pointer_list_row_second_click_launches) {
    Library lib(uiFixture());
    UiState s;
    s.focus = Focus::List;
    press(s, lib, kNoRecent, Action::Two);  // Seite 2
    Command c = click(s, lib, kNoRecent, Region::ListRow, 3);
    CHECK_EQ(c.kind, Command::None);
    CHECK_EQ(s.listIndex, 13);
    CHECK(s.focus == Focus::List);
    CHECK(s.carouselPreview == lib.visible()[13]);
    c = click(s, lib, kNoRecent, Region::ListRow, 3);
    CHECK_EQ(c.kind, Command::Launch);
    CHECK(c.game == lib.visible()[13]);
    // Zeile ausserhalb der Seite: nichts
    c = click(s, lib, kNoRecent, Region::ListRow, 9);
    CHECK_EQ(c.kind, Command::None);
    CHECK_EQ(s.listIndex, 13);
}

TEST(ui_pointer_first_click_from_other_focus_only_selects) {
    Library lib(uiFixture());
    UiState s;  // Fokus Karussell, listIndex 0
    Command c = click(s, lib, kNoRecent, Region::ListRow, 0);
    CHECK_EQ(c.kind, Command::None);
    CHECK(s.focus == Focus::List);
}

TEST(ui_pointer_other_regions) {
    Library lib(uiFixture());
    std::vector<const Game*> recent = {lib.visible()[4], lib.visible()[6]};
    UiState s;
    click(s, lib, recent, Region::Tab, 1);
    CHECK_EQ(s.tabIndex, 1);
    CHECK_EQ(lib.visible().size(), (size_t)12);
    click(s, lib, recent, Region::CarouselRight, 0);
    CHECK_EQ(s.carouselIndex, 1);
    Command c = click(s, lib, recent, Region::CarouselCenter, 0);
    CHECK_EQ(c.kind, Command::Launch);
    CHECK(c.game == recent[1]);
    click(s, lib, recent, Region::CarouselLeft, 0);
    CHECK_EQ(s.carouselIndex, 0);
    click(s, lib, recent, Region::Letter, 1);
    CHECK(s.focus == Focus::List);
    CHECK_EQ(s.listIndex, lib.firstIndexForLetter(lib.letters()[1]));
    click(s, lib, recent, Region::SearchField, 0);
    CHECK(s.overlay == Overlay::Search);
    click(s, lib, recent, Region::Key, keyIndex("w"));
    click(s, lib, recent, Region::Key, keyIndex("i"));
    std::vector<const Game*> res = searchResults(s, lib);
    CHECK(!res.empty());
    c = click(s, lib, recent, Region::Result, 0);
    CHECK_EQ(c.kind, Command::Launch);
    CHECK(c.game == res[0]);
    press(s, lib, recent, Action::B);
    press(s, lib, recent, Action::Home);
    c = click(s, lib, recent, Region::HomeEntry, 2);
    CHECK_EQ(c.kind, Command::PowerOff);
    press(s, lib, recent, Action::B);
    c = click(s, lib, recent, Region::Retry, 0);
    CHECK_EQ(c.kind, Command::Retry);
}

TEST(ui_pointer_hover_selects_list_row) {
    Library lib(uiFixture());
    UiState s;
    pointerHover(s, lib, std::make_pair(static_cast<int>(Region::ListRow), 4));
    CHECK_EQ(s.listIndex, 4);
    CHECK(s.focus == Focus::List);
    CHECK(s.carouselPreview == lib.visible()[4]);
}

TEST(ui_empty_library_is_safe) {
    Library lib(std::vector<Game>{});
    std::vector<const Game*> recent;
    const Action actions[] = {Action::Up,   Action::Down,  Action::Left, Action::Right,
                              Action::A,    Action::B,     Action::One,  Action::Two};
    const Focus focuses[] = {Focus::Tabs, Focus::Carousel, Focus::List, Focus::Letters};
    const Overlay overlays[] = {Overlay::None, Overlay::Systems, Overlay::Search, Overlay::Home};
    UiState s;
    CHECK(carouselGame(s, recent, lib) == nullptr);
    for (Overlay o : overlays) {
        for (Focus f : focuses) {
            for (Action a : actions) {
                s = UiState();
                s.overlay = o;
                s.focus = f;
                Command c = press(s, lib, recent, a);
                if (o != Overlay::Systems && o != Overlay::Home && o != Overlay::Search)
                    CHECK(c.kind != Command::Launch);
                CHECK(carouselGame(s, recent, lib) == nullptr);
            }
            for (int r = 0; r <= static_cast<int>(Region::Retry); ++r) {
                for (int idx : {-1, 0, 1, 50}) {
                    s = UiState();
                    s.overlay = o;
                    s.focus = f;
                    Command c = click(s, lib, recent, static_cast<Region>(r), idx);
                    CHECK(c.kind != Command::Launch);
                    pointerHover(s, lib, std::make_pair(r, idx));
                }
            }
        }
    }
    s = UiState();
    press(s, lib, recent, Action::Plus);
    press(s, lib, recent, Action::Minus);
    press(s, lib, recent, Action::Home);
    CHECK(carouselGame(s, recent, lib) == nullptr);
    CHECK(searchResults(s, lib).empty());
}

TEST(ui_out_of_range_pointer_ignored) {
    Library lib(uiFixture());
    UiState s;
    click(s, lib, kNoRecent, Region::Tab, 99);
    CHECK_EQ(s.tabIndex, 0);
    click(s, lib, kNoRecent, Region::Letter, -3);
    CHECK(s.focus == Focus::Carousel);
    Command c = apply(s, lib, kNoRecent, Action::PointerClick, std::make_pair(999, 0));
    CHECK_EQ(c.kind, Command::None);
}
