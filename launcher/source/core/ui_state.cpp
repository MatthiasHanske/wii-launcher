#include "core/ui_state.h"

#include <algorithm>

namespace {

constexpr int kSystemCount = 7;

int clampi(int v, int lo, int hi) {
    if (hi < lo) return lo;
    return std::max(lo, std::min(v, hi));
}

int visibleCount(const Library& lib) { return static_cast<int>(lib.visible().size()); }

int carouselCount(const std::vector<const Game*>& recent, const Library& lib) {
    return recent.empty() ? visibleCount(lib) : static_cast<int>(recent.size());
}

Command launch(const Game* g) {
    Command c;
    if (g) {
        c.kind = Command::Launch;
        c.game = g;
    }
    return c;
}

Command simple(Command::Kind k) {
    Command c;
    c.kind = k;
    return c;
}

// Indizes in gueltige Bereiche holen, falls sich Library oder recent geaendert haben.
void normalize(UiState& s, Library& lib, const std::vector<const Game*>& recent) {
    int tabCount = 1 + static_cast<int>(lib.tabs().size());
    if (s.tabIndex < 0 || s.tabIndex >= tabCount) {
        s.tabIndex = 0;
        lib.setTab(std::nullopt);
    }
    s.listIndex = clampi(s.listIndex, 0, visibleCount(lib) - 1);
    s.letterIndex = clampi(s.letterIndex, 0, static_cast<int>(lib.letters().size()) - 1);
    s.carouselIndex = clampi(s.carouselIndex, 0, carouselCount(recent, lib) - 1);
    s.keyboardIndex = clampi(s.keyboardIndex, 0, static_cast<int>(keyboardKeys().size()) - 1);
    s.homeIndex = clampi(s.homeIndex, 0, static_cast<int>(homeEntries().size()) - 1);
    s.systemsIndex = clampi(s.systemsIndex, 0, kSystemCount - 1);
    if (s.carouselPreview && !s.systems.get(s.carouselPreview->system)) s.carouselPreview = nullptr;
    if (s.overlay == Overlay::Search) {
        int n = static_cast<int>(searchResults(s, lib).size());
        if (n == 0) s.searchInResults = false;
        s.resultIndex = clampi(s.resultIndex, 0, n - 1);
    } else {
        s.searchInResults = false;
        s.resultIndex = 0;
    }
}

void selectList(UiState& s, const Library& lib, int index) {
    if (index < 0 || index >= visibleCount(lib)) return;
    s.listIndex = index;
    s.carouselPreview = lib.visible()[index];
}

void selectTab(UiState& s, Library& lib, int index) {
    std::vector<System> tabs = lib.tabs();
    if (index < 0 || index > static_cast<int>(tabs.size()) || index == s.tabIndex) return;
    s.tabIndex = index;
    lib.setTab(index == 0 ? std::nullopt : std::optional<System>(tabs[index - 1]));
    s.listIndex = 0;
    s.letterIndex = 0;
    s.carouselPreview = nullptr;
}

// Mit aktiver Vorschau loescht der erste Schritt nur die Vorschau, Index bleibt.
void moveCarousel(UiState& s, const std::vector<const Game*>& recent, const Library& lib, int delta) {
    if (s.carouselPreview) {
        s.carouselPreview = nullptr;
        return;
    }
    s.carouselIndex = clampi(s.carouselIndex + delta, 0, carouselCount(recent, lib) - 1);
}

void jumpLetter(UiState& s, const Library& lib, int index) {
    std::vector<char> letters = lib.letters();
    if (index < 0 || index >= static_cast<int>(letters.size())) return;
    s.letterIndex = index;
    selectList(s, lib, lib.firstIndexForLetter(letters[index]));
    s.focus = Focus::List;
}

void page(UiState& s, const Library& lib, int delta) {
    int n = visibleCount(lib);
    if (n == 0) return;
    int cur = s.listIndex / Library::kPageSize;
    int next = clampi(cur + delta, 0, lib.pageCount() - 1);
    if (next == cur) return;
    int row = s.listIndex % Library::kPageSize;
    selectList(s, lib, std::min(next * Library::kPageSize + row, n - 1));
}

// Buchstabe des aktuell gewaehlten Spiels in letters() suchen.
void syncLetterToList(UiState& s, const Library& lib) {
    if (s.listIndex >= visibleCount(lib)) return;
    char c = lib.visible()[s.listIndex]->letter;
    std::vector<char> letters = lib.letters();
    for (size_t i = 0; i < letters.size(); ++i)
        if (letters[i] == c) s.letterIndex = static_cast<int>(i);
}

Command toggleSystem(UiState& s, Library& lib, int sys) {
    if (sys < 0 || sys >= kSystemCount) return Command();
    std::vector<System> before = lib.tabs();
    std::optional<System> current;
    if (s.tabIndex > 0 && s.tabIndex <= static_cast<int>(before.size())) current = before[s.tabIndex - 1];

    System system = static_cast<System>(sys);
    s.systemsIndex = sys;
    s.systems.at(system) = !s.systems.get(system);
    lib.setEnabled(s.systems);

    std::vector<System> after = lib.tabs();
    s.tabIndex = 0;
    if (current) {
        for (size_t i = 0; i < after.size(); ++i)
            if (after[i] == *current) s.tabIndex = static_cast<int>(i) + 1;
        if (s.tabIndex == 0) lib.setTab(std::nullopt);
    }
    s.listIndex = 0;
    s.letterIndex = 0;
    if (s.carouselPreview && !s.systems.get(s.carouselPreview->system)) s.carouselPreview = nullptr;

    Command c;
    c.kind = Command::ToggleSystem;
    c.system = system;
    return c;
}

void typeKey(UiState& s, int index) {
    const auto& keys = keyboardKeys();
    if (index < 0 || index >= static_cast<int>(keys.size())) return;
    s.keyboardIndex = index;
    s.searchInResults = false;  // resultIndex bleibt, normalize() klemmt ihn auf die neuen Treffer
    const std::string& k = keys[index];
    if (index == static_cast<int>(keys.size()) - 1) {  // Löschen
        if (!s.query.empty()) s.query.pop_back();       // Eingabe ist reines ASCII
    } else if (index == static_cast<int>(keys.size()) - 2) {  // Leer
        if (!s.query.empty() && s.query.size() < kMaxQueryLength) s.query.push_back(' ');
    } else if (s.query.size() < kMaxQueryLength) {
        s.query += k;
    }
}

// Reihe/Spalte einer flachen Tastaturposition.
void keyPos(int index, int& row, int& col) {
    const auto& rows = keyboardRows();
    int start = 0;
    for (size_t r = 0; r < rows.size(); ++r) {
        if (index < start + rows[r]) {
            row = static_cast<int>(r);
            col = index - start;
            return;
        }
        start += rows[r];
    }
    row = 0;
    col = 0;
}

int keyAt(int row, int col) {
    const auto& rows = keyboardRows();
    int start = 0;
    for (int r = 0; r < row; ++r) start += rows[r];
    return start + clampi(col, 0, rows[row] - 1);
}

void moveKeyboard(UiState& s, Action a) {
    const auto& rows = keyboardRows();
    int row, col;
    keyPos(s.keyboardIndex, row, col);
    switch (a) {
        case Action::Left: if (col > 0) s.keyboardIndex--; break;
        case Action::Right: if (col < rows[row] - 1) s.keyboardIndex++; break;
        case Action::Up: if (row > 0) s.keyboardIndex = keyAt(row - 1, col); break;
        case Action::Down:
            if (row < static_cast<int>(rows.size()) - 1) s.keyboardIndex = keyAt(row + 1, col);
            break;
        default: break;
    }
}

Command homeCommand(int index) {
    switch (index) {
        case 0: return simple(Command::WiiMenu);
        case 1: return simple(Command::Hbc);
        case 2: return simple(Command::PowerOff);
        default: return Command();
    }
}

void openOverlay(UiState& s, Overlay o) {
    s.overlay = o;
    if (o == Overlay::Search) {
        s.searchInResults = false;
        s.resultIndex = 0;
    } else if (o == Overlay::Home) {
        s.homeIndex = 0;
    }
}

Command applySystems(UiState& s, Library& lib, Action a) {
    switch (a) {
        case Action::Up: s.systemsIndex = clampi(s.systemsIndex - 1, 0, kSystemCount - 1); break;
        case Action::Down: s.systemsIndex = clampi(s.systemsIndex + 1, 0, kSystemCount - 1); break;
        case Action::A: return toggleSystem(s, lib, s.systemsIndex);
        case Action::B:
        case Action::Plus: s.overlay = Overlay::None; break;
        default: break;
    }
    return Command();
}

Command applySearch(UiState& s, const Library& lib, Action a) {
    if (a == Action::B || a == Action::Minus) {
        s.overlay = Overlay::None;
        return Command();
    }
    std::vector<const Game*> results = searchResults(s, lib);
    int n = static_cast<int>(results.size());
    if (s.searchInResults) {
        switch (a) {
            case Action::Up:
                if (s.resultIndex > 0) s.resultIndex--;
                else s.searchInResults = false;
                break;
            case Action::Down: s.resultIndex = clampi(s.resultIndex + 1, 0, n - 1); break;
            case Action::A:
                if (s.resultIndex < n) return launch(results[s.resultIndex]);
                break;
            case Action::One:
            case Action::Two: s.searchInResults = false; break;
            default: break;
        }
        return Command();
    }
    switch (a) {
        case Action::Down: {
            int row, col;
            keyPos(s.keyboardIndex, row, col);
            if (row < static_cast<int>(keyboardRows().size()) - 1) {
                moveKeyboard(s, a);
                break;
            }
            // unterste Reihe (Leer/Löschen): weiter in die Treffer wie Two
            if (n > 0) {
                s.searchInResults = true;
                s.resultIndex = 0;
            }
            break;
        }
        case Action::Up:
        case Action::Left:
        case Action::Right: moveKeyboard(s, a); break;
        case Action::A: typeKey(s, s.keyboardIndex); break;
        case Action::Two:
            if (n > 0) {
                s.searchInResults = true;
                s.resultIndex = 0;
            }
            break;
        default: break;
    }
    return Command();
}

Command applyHome(UiState& s, Action a) {
    int n = static_cast<int>(homeEntries().size());
    switch (a) {
        case Action::Up: s.homeIndex = clampi(s.homeIndex - 1, 0, n - 1); break;
        case Action::Down: s.homeIndex = clampi(s.homeIndex + 1, 0, n - 1); break;
        case Action::A: return homeCommand(s.homeIndex);
        case Action::B: s.overlay = Overlay::None; break;
        default: break;
    }
    return Command();
}

Command applyMain(UiState& s, Library& lib, const std::vector<const Game*>& recent, Action a) {
    switch (s.focus) {
        case Focus::Tabs:
            switch (a) {
                case Action::Left: selectTab(s, lib, s.tabIndex - 1); break;
                case Action::Right: selectTab(s, lib, s.tabIndex + 1); break;
                case Action::Down:
                case Action::A: s.focus = Focus::Carousel; break;
                default: break;
            }
            break;
        case Focus::Carousel:
            switch (a) {
                case Action::Left: moveCarousel(s, recent, lib, -1); break;
                case Action::Right: moveCarousel(s, recent, lib, +1); break;
                case Action::Up: s.focus = Focus::Tabs; break;
                case Action::Down: s.focus = Focus::List; break;
                case Action::A: return launch(carouselGame(s, recent, lib));
                default: break;
            }
            break;
        case Focus::List: {
            int n = visibleCount(lib);
            switch (a) {
                case Action::Up:
                    if (s.listIndex % Library::kPageSize == 0) s.focus = Focus::Tabs;
                    else selectList(s, lib, s.listIndex - 1);
                    break;
                case Action::Down:  // letzte Zeile einer Seite -> naechste Seite, Zeile 0
                    if (s.listIndex + 1 < n) selectList(s, lib, s.listIndex + 1);
                    break;
                case Action::Left:
                    syncLetterToList(s, lib);
                    s.focus = Focus::Letters;
                    break;
                case Action::Right: s.focus = Focus::Carousel; break;
                case Action::One: page(s, lib, -1); break;
                case Action::Two: page(s, lib, +1); break;
                case Action::A:
                    if (s.listIndex < n) return launch(lib.visible()[s.listIndex]);
                    break;
                default: break;
            }
            break;
        }
        case Focus::Letters: {
            int n = static_cast<int>(lib.letters().size());
            switch (a) {
                case Action::Up: s.letterIndex = clampi(s.letterIndex - 1, 0, n - 1); break;
                case Action::Down: s.letterIndex = clampi(s.letterIndex + 1, 0, n - 1); break;
                case Action::A: jumpLetter(s, lib, s.letterIndex); break;
                case Action::Right:
                case Action::B: s.focus = Focus::List; break;
                default: break;
            }
            break;
        }
    }
    return Command();
}

Command applyClick(UiState& s, Library& lib, const std::vector<const Game*>& recent, Region r, int idx) {
    if (r == Region::Retry) return simple(Command::Retry);

    if (s.overlay != Overlay::None) {
        if (r == Region::None) {
            s.overlay = Overlay::None;
            return Command();
        }
        switch (s.overlay) {
            case Overlay::Systems:
                if (r == Region::SystemToggle) return toggleSystem(s, lib, idx);
                break;
            case Overlay::Search:
                if (r == Region::Key) {
                    typeKey(s, idx);
                } else if (r == Region::Result) {
                    std::vector<const Game*> results = searchResults(s, lib);
                    if (idx >= 0 && idx < static_cast<int>(results.size())) {
                        s.searchInResults = true;
                        s.resultIndex = idx;
                        return launch(results[idx]);
                    }
                }
                break;
            case Overlay::Home:
                if (r == Region::HomeEntry && idx >= 0 && idx < static_cast<int>(homeEntries().size())) {
                    s.homeIndex = idx;
                    return homeCommand(idx);
                }
                break;
            case Overlay::None: break;
        }
        return Command();
    }

    switch (r) {
        case Region::Tab:
            if (idx >= 0 && idx <= static_cast<int>(lib.tabs().size())) {
                selectTab(s, lib, idx);
                s.focus = Focus::Tabs;
            }
            break;
        case Region::CarouselLeft:
            s.focus = Focus::Carousel;
            moveCarousel(s, recent, lib, -1);
            break;
        case Region::CarouselRight:
            s.focus = Focus::Carousel;
            moveCarousel(s, recent, lib, +1);
            break;
        case Region::CarouselCenter:
            s.focus = Focus::Carousel;
            return launch(carouselGame(s, recent, lib));
        case Region::ListRow: {
            if (idx < 0 || idx >= Library::kPageSize) break;
            int abs = (s.listIndex / Library::kPageSize) * Library::kPageSize + idx;
            if (abs >= visibleCount(lib)) break;
            bool wasSelected = s.focus == Focus::List && s.listIndex == abs;
            selectList(s, lib, abs);
            s.focus = Focus::List;
            if (wasSelected) return launch(lib.visible()[abs]);
            break;
        }
        case Region::Letter: jumpLetter(s, lib, idx); break;
        case Region::SearchField: openOverlay(s, Overlay::Search); break;
        default: break;
    }
    return Command();
}

}  // namespace

const std::vector<std::string>& keyboardKeys() {
    static const std::vector<std::string> keys = {
        "q", "w", "e", "r", "t", "z", "u", "i", "o", "p",
        "a", "s", "d", "f", "g", "h", "j", "k", "l",
        "y", "x", "c", "v", "b", "n", "m",
        "Leer", "L\xc3\xb6schen",
    };
    return keys;
}

const std::vector<int>& keyboardRows() {
    static const std::vector<int> rows = {10, 9, 7, 2};
    return rows;
}

const std::vector<std::string>& homeEntries() {
    static const std::vector<std::string> entries = {"Wii-Men\xc3\xbc", "Homebrew Channel", "Ausschalten"};
    return entries;
}

std::vector<const Game*> searchResults(const UiState& s, const Library& library) {
    return library.search(s.query, kSearchResultLimit);
}

const Game* carouselGame(const UiState& s, const std::vector<const Game*>& recent, const Library& library) {
    if (s.carouselPreview) return s.carouselPreview;
    int i = s.carouselIndex;
    if (!recent.empty()) return (i >= 0 && i < static_cast<int>(recent.size())) ? recent[i] : nullptr;
    const auto& vis = library.visible();
    return (i >= 0 && i < static_cast<int>(vis.size())) ? vis[i] : nullptr;
}

Command apply(UiState& s, Library& library, const std::vector<const Game*>& recent, Action a,
              std::optional<std::pair<int, int>> pointerHit) {
    normalize(s, library, recent);
    Command c;
    if (s.noUsb && s.overlay == Overlay::None) {
        // Fehlerbildschirm: A = Retry, Home oeffnet das Home-Menue, Retry-Klick, sonst nichts.
        if (a == Action::A) {
            c = simple(Command::Retry);
        } else if (a == Action::Home) {
            openOverlay(s, Overlay::Home);
        } else if (a == Action::PointerClick && pointerHit &&
                   pointerHit->first == static_cast<int>(Region::Retry)) {
            c = simple(Command::Retry);
        }
        normalize(s, library, recent);
        return c;
    }
    if (a == Action::PointerClick) {
        if (pointerHit && pointerHit->first >= 0 && pointerHit->first <= static_cast<int>(Region::Retry))
            c = applyClick(s, library, recent, static_cast<Region>(pointerHit->first), pointerHit->second);
    } else if (a == Action::Home) {
        // Home ersetzt jedes andere Overlay (z.B. Suche); B schliesst dann zum Hauptbildschirm,
        // nicht zurueck ins vorige Overlay.
        if (s.overlay == Overlay::Home) s.overlay = Overlay::None;
        else openOverlay(s, Overlay::Home);
    } else {
        switch (s.overlay) {
            case Overlay::Systems: c = applySystems(s, library, a); break;
            case Overlay::Search: c = applySearch(s, library, a); break;
            case Overlay::Home: c = applyHome(s, a); break;
            case Overlay::None:
                if (a == Action::Plus) openOverlay(s, Overlay::Systems);
                else if (a == Action::Minus) openOverlay(s, Overlay::Search);
                else c = applyMain(s, library, recent, a);
                break;
        }
    }
    normalize(s, library, recent);
    return c;
}

void pointerHover(UiState& s, const Library& library, std::optional<std::pair<int, int>> pointerHit) {
    if (!pointerHit) return;
    Region r = static_cast<Region>(pointerHit->first);
    int idx = pointerHit->second;
    if (pointerHit->first < 0 || pointerHit->first > static_cast<int>(Region::Retry)) return;
    switch (s.overlay) {
        case Overlay::None:
            if (r == Region::ListRow && idx >= 0 && idx < Library::kPageSize) {
                int abs = (s.listIndex / Library::kPageSize) * Library::kPageSize + idx;
                if (abs < visibleCount(library)) {
                    selectList(s, library, abs);
                    s.focus = Focus::List;
                }
            } else if (r == Region::Letter && idx >= 0 && idx < static_cast<int>(library.letters().size())) {
                s.letterIndex = idx;
                s.focus = Focus::Letters;
            }
            break;
        case Overlay::Search:
            if (r == Region::Key && idx >= 0 && idx < static_cast<int>(keyboardKeys().size())) {
                s.keyboardIndex = idx;
                s.searchInResults = false;
            } else if (r == Region::Result && idx >= 0 &&
                       idx < static_cast<int>(searchResults(s, library).size())) {
                s.resultIndex = idx;
                s.searchInResults = true;
            }
            break;
        case Overlay::Systems:
            if (r == Region::SystemToggle && idx >= 0 && idx < kSystemCount) s.systemsIndex = idx;
            break;
        case Overlay::Home:
            if (r == Region::HomeEntry && idx >= 0 && idx < static_cast<int>(homeEntries().size()))
                s.homeIndex = idx;
            break;
    }
}
