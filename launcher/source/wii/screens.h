#pragma once
// Bildschirme des Launchers (Spec 4.1/4.6, Layout C mit Mini-Covern): Layout, Zeichnen und
// Hit-Test. Zeichnen und Hit-Test rechnen mit denselben Layout-Strukturen, damit Klickflächen
// und Bild übereinstimmen. Alle Koordinaten logisch (gfx::width() x 480), 32 px Sicherheitsrand.
//
// Aufruf nur zwischen gfx::beginFrame() und gfx::endFrame(); Covers::endFrame() danach.
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "core/library.h"
#include "core/model.h"
#include "core/ui_state.h"
#include "wii/gfx.h"
#include "wii/texcache.h"

namespace screens {

// {int(Region), Index}; nullopt = in einem Overlay-Panel, aber auf keinem Bedienelement.
using Hit = std::optional<std::pair<int, int>>;

// Farben (Spec 4.6): ein Akzent, heller Text, gedämpfter Text.
constexpr uint32_t kBg = 0x0F1218FF;
constexpr uint32_t kText = 0xE8E8E8FF;
constexpr uint32_t kMuted = 0x9AA3B5FF;
constexpr uint32_t kAccent = 0x3B82F6FF;
constexpr uint32_t kPanel = 0x171C26FF;
constexpr uint32_t kBorder = 0x2A3140FF;
constexpr uint32_t kChip = 0x222A38FF;
constexpr uint32_t kChipHover = 0x2E3748FF;
constexpr uint32_t kRowSel = 0x1E2432FF;
constexpr uint32_t kWhite = 0xFFFFFFFF;

// Schriftgrößen (Spec 4.6): Titel, Liste, Leiste/Hinweise.
constexpr int kSizeTitle = 22;
constexpr int kSizeList = 16;
constexpr int kSizeSmall = 14;

constexpr float kSafe = 32;

struct Rect {
    float x = 0, y = 0, w = 0, h = 0;
    bool contains(float px, float py) const { return px >= x && px < x + w && py >= y && py < y + h; }
};

// Cover von der SD (TexCache) plus eingebettete Systemlogos. Pro Bild werden höchstens
// kNewLoadsPerFrame Cover von der Karte geladen (weder im Cache noch als Fehlschlag gemerkt,
// also auch Nachladen nach Verdrängung), damit ein Seitenwechsel nicht ein Bild lang hängt;
// die übrigen zeigen solange eine leere Kachel.
class Covers {
public:
    static constexpr int kNewLoadsPerFrame = 2;

    explicit Covers(size_t budgetBytes);
    ~Covers();
    Covers(const Covers&) = delete;
    Covers& operator=(const Covers&) = delete;

    void loadLogos();  // nach gfx::init()
    void release();    // vor gfx::shutdown(): Cover und Logos freigeben

    const Texture* logo(System s) const;
    // nullptr: kein Cover (pending = false) oder noch nicht geladen (pending = true).
    const Texture* cover(const Game& g, bool& pending);

    void endFrame();  // nach gfx::endFrame()
    void forgetFailures();
    void clear();     // alle Cover freigeben (z. B. vor bootDol)

private:
    TexCache cache_;
    Texture logos_[7];
    int loadsLeft_ = kNewLoadsPerFrame;
};

// --- Layouts (geteilt von Zeichnen und Hit-Test) -----------------------------------------

struct TopBarLayout {
    std::vector<Rect> tabs;            // [0] = Alle, [i] = library.tabs()[i-1]
    std::vector<std::string> labels;   // ggf. gekürzt
    Rect search;
    bool searchCompact = false;        // nur Lupe, wenn die Tabs sonst nicht passen
};

constexpr int kLetterCells = 27;  // '#', A..Z
constexpr int kLetterRows = 14;   // zwei Spalten, spaltenweise

struct MainLayout {
    TopBarLayout bar;
    float leftX = 0, leftW = 0, splitX = 0;
    float labelY = 0;
    Rect carLeft, carCenter, carRight;          // Cover-Rechtecke
    Rect hitLeft, hitRight;                     // Klickflächen der Seitencover (volle Höhe)
    float titleY = 0, systemY = 0, hintsY = 0;
    float listX = 0, listW = 0;
    Rect rows[Library::kPageSize];
    int rowCount = 0;                           // Zeilen mit Spiel auf der aktuellen Seite
    Rect letterCells[kLetterCells];
    char letterChars[kLetterCells];
    int letterIndex[kLetterCells];              // Index in library.letters(), -1 = inaktiv
};

MainLayout mainLayout(const UiState& s, const Library& lib);

struct SystemsLayout {
    Rect panel;
    float titleY = 0;
    Rect rows[7];  // Enum-Reihenfolge
};
SystemsLayout systemsLayout();

struct SearchLayout {
    Rect panel;
    Rect field;
    std::vector<Rect> keys;  // wie keyboardKeys()
    float resultsY = 0;
    std::vector<Rect> results;  // je Treffer
};
SearchLayout searchLayout(const UiState& s, const Library& lib);

struct HomeLayout {
    Rect panel;
    Rect rows[3];
};
HomeLayout homeLayout();

struct NoUsbLayout {
    float titleY = 0, hintY = 0;
    Rect button;
};
NoUsbLayout noUsbLayout();

// --- Hit-Test ------------------------------------------------------------------------------

// Nur die oberste Ebene: offenes Overlay -> dessen Regionen, außerhalb des Panels Region::None,
// im Panel ohne Bedienelement nullopt. Hauptbild: Region::None, wenn nichts getroffen.
// recent wie für carouselGame(): CarouselLeft/Right gibt es nur, wenn dort ein Spiel liegt.
Hit hitTest(float x, float y, const UiState& s, const Library& lib, const std::vector<const Game*>& recent);

// --- Zeichnen ------------------------------------------------------------------------------

// Hauptbild. hover = aktueller Zeigertreffer (Hover-Hervorhebung), slide in [-1, 1]:
// Gleitversatz des Karussells (0 = in Ruhe).
void drawMain(const UiState& s, const Library& lib, const std::vector<const Game*>& recent,
              Covers& covers, const Hit& hover, float slide);
// "USB-Platte nicht gefunden" mit Knopf "Erneut suchen".
void drawNoUsb(const Hit& hover);
// Overlay o mit Deckkraft alpha (0..1, Ein-/Ausblenden), inklusive Abdunklung.
void drawOverlay(Overlay o, const UiState& s, const Library& lib, float alpha);
// Abdunklung + Panel mit Titel und Grund (Startfehler).
void drawMessagePanel(const std::string& title, const std::string& detail, float alpha);
// Ganzer Bildschirm: eine Zeile mittig (Ladehinweis, "Starte …").
void drawCenteredMessage(const std::string& title, const std::string& detail = std::string());
// Schwarz über allem, alpha 0..1 (Ausblenden vor dem Start).
void drawFade(float alpha);
void drawPointer(float x, float y);

}  // namespace screens
