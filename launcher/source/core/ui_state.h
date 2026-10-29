#pragma once
// Plattformfreie UI-Zustandsmaschine: Eingabe-Aktion rein, Zustand + Command raus.
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "core/library.h"
#include "core/model.h"

enum class Focus { Tabs, Carousel, List, Letters };
enum class Overlay { None, Systems, Search, Home };

struct UiState {
    Focus focus = Focus::Carousel;
    Overlay overlay = Overlay::None;
    int tabIndex = 0;        // 0 = Alle, i >= 1 = library.tabs()[i-1]
    int carouselIndex = 0;   // in recent, bei leerem recent in library.visible()
    int listIndex = 0;       // absolut in library.visible()
    int letterIndex = 0;     // in library.letters()
    std::string query;
    int keyboardIndex = 0;   // in keyboardKeys()
    bool searchInResults = false;
    int resultIndex = 0;     // in searchResults()
    int homeIndex = 0;       // in homeEntries()
    int systemsIndex = 0;    // System als int, Enum-Reihenfolge
    const Game* carouselPreview = nullptr;  // in der Liste angewaehltes Spiel
    SystemSet systems = SystemSet::defaults();  // Kopie, Library gibt ihr Set nicht heraus
};

enum class Action { Up, Down, Left, Right, A, B, Plus, Minus, One, Two, Home, PointerClick };

// Regionstypen fuer den Hit-Test des Renderers: pointerHit = {int(Region), Index}.
enum class Region {
    None = 0,        // Klick ins Leere (schliesst ein offenes Overlay)
    Tab,             // Index: tabIndex (0 = Alle)
    CarouselLeft,    // Index egal
    CarouselCenter,  // Index egal
    CarouselRight,   // Index egal
    ListRow,         // Index: Zeile 0..9 auf der aktuellen Seite
    Letter,          // Index in library.letters()
    SearchField,     // Index egal
    Key,             // Index in keyboardKeys()
    Result,          // Index in searchResults()
    SystemToggle,    // Index: int(System)
    HomeEntry,       // Index in homeEntries()
    Retry,           // Index egal ("USB nicht gefunden")
};

struct Command {
    enum Kind { None, Launch, ToggleSystem, WiiMenu, Hbc, PowerOff, Retry } kind = None;
    const Game* game = nullptr;
    System system = System::Wii;
};

constexpr int kSearchResultLimit = 8;
constexpr size_t kMaxQueryLength = 32;

// QWERTZ: "q".."p" | "a".."l" | "y".."m" | "Leer" "Löschen" (UTF-8), flach.
const std::vector<std::string>& keyboardKeys();
// Tastenanzahl je Reihe: {10, 9, 7, 2}; Reihen liegen in keyboardKeys() hintereinander.
const std::vector<int>& keyboardRows();
// "Wii-Menü", "Homebrew Channel", "Ausschalten" (UTF-8).
const std::vector<std::string>& homeEntries();

Command apply(UiState& s, Library& library, const std::vector<const Game*>& recent, Action a,
              std::optional<std::pair<int, int>> pointerHit);

// Zeiger schwebt ueber einer Region: setzt nur die Auswahl, loest nie etwas aus.
void pointerHover(UiState& s, const Library& library, std::optional<std::pair<int, int>> pointerHit);

// Vorschau, sonst recent[carouselIndex], sonst visible()[carouselIndex], sonst nullptr.
const Game* carouselGame(const UiState& s, const std::vector<const Game*>& recent, const Library& library);

// Live-Treffer der Suche: library.search(query, kSearchResultLimit).
std::vector<const Game*> searchResults(const UiState& s, const Library& library);
