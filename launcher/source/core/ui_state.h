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
    int carouselIndex = 0;   // in library.visible(), Mittelcover des Fächers
    int listIndex = 0;       // absolut in library.visible()
    bool listOpen = false;   // Liste von rechts eingeblendet; Focus::List/Letters nur dann
    int letterIndex = 0;     // in library.letters()
    std::string query;
    int keyboardIndex = 0;   // in keyboardKeys()
    bool searchInResults = false;
    int resultIndex = 0;     // in searchResults()
    int homeIndex = 0;       // in homeEntries()
    int systemsIndex = 0;    // System als int, Enum-Reihenfolge
    SystemSet systems = SystemSet::defaults();  // Quelle fuer library.setEnabled()
    bool noUsb = false;  // "USB nicht gefunden": nur A (-> Retry), Home und Retry-Klick wirken
};

enum class Action { Up, Down, Left, Right, A, B, Plus, Minus, One, Two, Home, PointerClick };

// Steuerkreuz-Vertrag des Hauptbildschirms, Anordnung Leiste oben, darunter der Fächer über
// library.visible(); die Liste (A-Z | Zeilen) wird bei Bedarf von rechts eingeblendet:
// - Tabs: links/rechts Tab wechseln, runter oder A -> Fächer. Ein Tabwechsel behaelt das
//   Spiel, wenn es im neuen Tab vorkommt, sonst Index 0.
// - Fächer: links/rechts +-1, 1/2 +-10 (bei offener Liste seitenweise wie in der Liste),
//   hoch -> Tabs, runter -> Liste oeffnen (auf dem Spiel des Fächers), A startet.
// - Liste: hoch/runter Zeile (seitenuebergreifend); hoch nur bei absolutem Index 0 -> Tabs.
//   Links -> A-Z, rechts tut nichts. 1/2 blaettern seitenweise, A startet, B schliesst.
//   Jede Auswahl in der Liste setzt auch carouselIndex (Mittelcover = gewaehltes Spiel).
// - A-Z: hoch/runter Buchstabe, A springt in die Liste, rechts -> Liste,
//   links oder B -> Liste schliessen, Fokus Fächer.
// Zeiger: Klick auf einen Tab waehlt ihn und setzt den Fokus in den Fächer.

// Regionstypen fuer den Hit-Test des Renderers: pointerHit = {int(Region), Index}.
enum class Region {
    None = 0,        // Klick ins Leere (schliesst ein offenes Overlay, sonst die Liste)
    Tab,             // Index: tabIndex (0 = Alle)
    CarouselLeft,    // Index: Abstand zur Mitte (1 oder 2)
    CarouselCenter,  // Index egal
    CarouselRight,   // Index: Abstand zur Mitte (1 oder 2)
    ListRow,         // Index: Zeile 0..9 auf der aktuellen Seite. Klick auf die bereits gewaehlte
                     // Zeile (Fokus Liste) startet sofort, sonst nur Auswahl.
    Letter,          // Index in library.letters()
    SearchField,     // Index egal
    ListToggle,      // Index egal (Pille "Liste" in der Leiste: oeffnet/schliesst die Liste)
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

// Ziffern + QWERTZ: "1".."0" | "q".."p" | "a".."l" | "y".."m" | "Leer" "Löschen" (UTF-8), flach.
const std::vector<std::string>& keyboardKeys();
// Tastenanzahl je Reihe: {10, 10, 9, 7, 2}; Reihen liegen in keyboardKeys() hintereinander.
const std::vector<int>& keyboardRows();
// "Wii-Menü", "Homebrew Channel", "Ausschalten" (UTF-8).
const std::vector<std::string>& homeEntries();

Command apply(UiState& s, Library& library, Action a, std::optional<std::pair<int, int>> pointerHit);

// Zeiger schwebt ueber einer Region: setzt in den Overlays die Auswahl, loest nie etwas aus.
// Im Hauptbild (Overlay::None) wirkt Hover nicht auf den Zustand.
void pointerHover(UiState& s, const Library& library, std::optional<std::pair<int, int>> pointerHit);

// Tab des Systems waehlen (wie ein Tabwechsel). Ist das System nicht aktiviert (nicht in
// library.tabs()), passiert nichts und der Tab bleibt unveraendert.
void selectSystemTab(UiState& s, Library& library, System sys);

// visible()[carouselIndex], sonst nullptr.
const Game* carouselGame(const UiState& s, const Library& library);

// Live-Treffer der Suche: library.search(query, kSearchResultLimit).
std::vector<const Game*> searchResults(const UiState& s, const Library& library);
