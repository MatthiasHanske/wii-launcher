#include "wii/screens.h"

#include <grrlib.h>

#include <algorithm>
#include <cctype>
#include <cmath>

#include "core/covers.h"
#include "logo_dos_png.h"
#include "logo_gc_png.h"
#include "logo_n64_png.h"
#include "logo_nes_png.h"
#include "logo_sega_png.h"
#include "logo_snes_png.h"
#include "logo_wii_png.h"

namespace screens {

namespace {

// --- Maße (logische Pixel, bezogen auf 480 Zeilen) -------------------------------------

constexpr float kBarH = 44;                  // Leiste innerhalb des Sicherheitsrands
constexpr float kBarBottom = kSafe + kBarH;  // 76; Hintergrund der Leiste reicht bis y = 0
constexpr float kPillH = 28;
constexpr float kSearchW = 150;
constexpr float kSplit = 0.52f;              // Anteil der linken Spalte an der Inhaltsbreite

// Vorschau: Cover werden per fitRect in Spaltenbreite x kCenterH eingepasst (hochkant
// 160x224 wie die Wii/GC-Cover, quer bis zur vollen Spaltenbreite). kCenterW x kCenterH ist
// das Standardformat der Platzhalter-Kachel. Darunter Titel (ab y 352) und Systemzeile
// (bis etwa y 400); die Tastenhinweise beginnen bei kBottom - 18 = 430.
constexpr float kCenterW = 160, kCenterH = 224;
constexpr float kSideW = 88, kSideH = 123;   // Rahmen der Seitencover (Cover darin eingepasst)
constexpr float kCarouselTop = 112;
constexpr float kSlidePx = 36;               // Gleitweg des Karussells

constexpr float kListTop = 84;
constexpr float kRowH = 36;
constexpr float kMiniW = 24, kMiniH = 34;
constexpr float kLetterCell = 20, kLetterStep = 22;

constexpr float kBottom = 480 - kSafe;       // 448

float W() { return static_cast<float>(gfx::width()); }

uint32_t withAlpha(uint32_t c, float a) {
    a = std::clamp(a, 0.0f, 1.0f);
    uint32_t al = static_cast<uint32_t>(std::lround((c & 0xFF) * a));
    return (c & 0xFFFFFF00u) | al;
}

// Oberkante für Text, dessen Großbuchstaben um centerY zentriert sein sollen
// (Grundlinie bei y + size, Versalhöhe etwa 0.72 * size).
float textTop(float centerY, int size) { return std::round(centerY - 0.64f * size); }

void textIn(const Rect& r, int size, uint32_t c, const std::string& t, bool center) {
    std::string s = gfx::fitText(size, r.w, t);
    if (s.empty()) return;
    float x = r.x;
    if (center) x += (r.w - gfx::textWidth(size, s)) / 2;
    gfx::text(std::round(x), textTop(r.y + r.h / 2, size), size, c, s);
}

void roundRectR(const Rect& r, float radius, uint32_t c) { gfx::roundRect(r.x, r.y, r.w, r.h, radius, c); }

Rect inset(const Rect& r, float d) { return Rect{r.x + d, r.y + d, r.w - 2 * d, r.h - 2 * d}; }

// Auswahlrahmen im Akzent (2 px) mit dunkler Füllung.
void focusRow(const Rect& r, float alpha = 1) {
    roundRectR(r, 6, withAlpha(kAccent, alpha));
    roundRectR(inset(r, 2), 4, withAlpha(kRowSel, alpha));
}

void panel(const Rect& r, float alpha) {
    roundRectR(r, 10, withAlpha(kBorder, alpha));
    roundRectR(inset(r, 1), 9, withAlpha(kPanel, alpha));
}

void dim(float alpha) { gfx::rect(0, 0, W(), 480, withAlpha(0x0000009A, alpha)); }

void magnifier(float cx, float cy, uint32_t c) {
    gfx::ring(cx - 1, cy - 1, 4.5f, 2, c);
    for (int i = 0; i < 3; ++i) gfx::rect(cx + 2 + i * 1.5f, cy + 2 + i * 1.5f, 2, 2, c);
}

bool hitIs(const Hit& h, Region r, int index = -1) {
    return h && h->first == static_cast<int>(r) && (index < 0 || h->second == index);
}

const char* systemLabel(System s) { return s == System::Adventure ? "Adventure / DOS" : systemName(s); }

// --- Leiste ------------------------------------------------------------------------------

TopBarLayout barLayout(const Library& lib) {
    TopBarLayout b;
    b.labels.push_back("Alle");
    for (System s : lib.tabs()) b.labels.push_back(systemName(s));
    const int n = static_cast<int>(b.labels.size());

    std::vector<float> tw;
    for (const std::string& l : b.labels) tw.push_back(gfx::textWidth(kSizeSmall, l));

    struct Cfg { float pad, gap; bool compact; };
    const Cfg cfgs[] = {{12, 8, false}, {9, 6, false}, {9, 6, true}, {6, 4, true}};
    Cfg use = cfgs[3];
    float avail = 0;
    bool fits = false;
    for (const Cfg& c : cfgs) {
        float searchW = c.compact ? kPillH : kSearchW;
        avail = W() - 2 * kSafe - searchW - 12;
        float total = c.gap * (n - 1);
        for (float w : tw) total += w + 2 * c.pad;
        if (total <= avail) {
            use = c;
            fits = true;
            break;
        }
    }
    if (!fits) {  // letzter Ausweg: Beschriftungen kürzen
        float maxLabel = (avail - use.gap * (n - 1)) / n - 2 * use.pad;
        for (int i = 0; i < n; ++i) {
            b.labels[i] = gfx::fitText(kSizeSmall, maxLabel, b.labels[i]);
            tw[i] = gfx::textWidth(kSizeSmall, b.labels[i]);
        }
    }

    const float y = kSafe + (kBarH - kPillH) / 2;
    float x = kSafe;
    for (int i = 0; i < n; ++i) {
        float w = std::round(tw[i] + 2 * use.pad);
        b.tabs.push_back(Rect{x, y, w, kPillH});
        x += w + use.gap;
    }
    b.searchCompact = use.compact;
    float sw = use.compact ? kPillH : kSearchW;
    b.search = Rect{W() - kSafe - sw, y, sw, kPillH};
    return b;
}

void drawBar(const TopBarLayout& b, const UiState& s, const Hit& hover) {
    gfx::rect(0, 0, W(), kBarBottom, kPanel);
    gfx::rect(0, kBarBottom, W(), 1, kBorder);

    for (size_t i = 0; i < b.tabs.size(); ++i) {
        const Rect& r = b.tabs[i];
        bool active = static_cast<int>(i) == s.tabIndex;
        if (active && s.focus == Focus::Tabs && s.overlay == Overlay::None)
            roundRectR(inset(r, -3), kPillH / 2 + 3, kText);  // Fokusring um den aktiven Tab
        uint32_t fill = active ? kAccent : (hitIs(hover, Region::Tab, static_cast<int>(i)) ? kChipHover : kChip);
        roundRectR(r, kPillH / 2, fill);
        textIn(r, kSizeSmall, active ? kWhite : kMuted, b.labels[i], true);
    }

    const Rect& r = b.search;
    roundRectR(r, kPillH / 2, hitIs(hover, Region::SearchField) ? kChipHover : kChip);
    if (b.searchCompact) {
        magnifier(r.x + r.w / 2, r.y + r.h / 2, kMuted);
    } else {
        magnifier(r.x + 16, r.y + r.h / 2, kMuted);
        gfx::text(r.x + 30, textTop(r.y + r.h / 2, kSizeSmall), kSizeSmall, kMuted, "Suche");
    }
}

// --- Cover ---------------------------------------------------------------------------------

// Cover unverzerrt und zentriert in r einpassen; ohne Cover (noch ladend oder keins) füllt die
// Kachel r. Liefert die tatsächlich bemalte Fläche.
Rect drawCover(Covers& covers, const Game& g, const Rect& r, bool mini, float alpha = 1) {
    bool pending = false;
    const Texture* tex = covers.cover(g, pending);
    if (tex) {
        Rect f = fitRect(static_cast<float>(tex->w), static_cast<float>(tex->h), r);
        if (!f.empty()) {
            gfx::image(*tex, f.x, f.y, f.w, f.h, alpha);
            return f;
        }
    }
    if (pending) {
        gfx::gradientRect(r.x, r.y, r.w, r.h, withAlpha(0x1F2937FF, alpha), withAlpha(0x111827FF, alpha));
    } else {
        gfx::placeholder(r.x, r.y, r.w, r.h, covers.logo(g.system), mini ? std::string() : g.title);
    }
    return r;
}

// Nachbarn des Karussells (links/rechts von carouselIndex), nullptr wenn keiner.
void carouselSides(const UiState& s, const Library& lib, const std::vector<const Game*>& recent,
                   const Game*& left, const Game*& right) {
    const std::vector<const Game*>& source = recent.empty() ? lib.visible() : recent;
    const int ci = s.carouselIndex, n = static_cast<int>(source.size());
    left = (ci - 1 >= 0 && ci - 1 < n) ? source[ci - 1] : nullptr;
    right = (ci + 1 >= 0 && ci + 1 < n) ? source[ci + 1] : nullptr;
}

}  // namespace

// --- Covers --------------------------------------------------------------------------------

Covers::Covers(size_t budgetBytes) : cache_(budgetBytes) {}

Covers::~Covers() { release(); }

void Covers::loadLogos() {
    struct Src { const unsigned char* data; size_t size; };
    // Enum-Reihenfolge: Wii, GameCube, N64, SNES, NES, Sega, Adventure
    const Src src[7] = {{logo_wii_png, logo_wii_png_size},   {logo_gc_png, logo_gc_png_size},
                        {logo_n64_png, logo_n64_png_size},   {logo_snes_png, logo_snes_png_size},
                        {logo_nes_png, logo_nes_png_size},   {logo_sega_png, logo_sega_png_size},
                        {logo_dos_png, logo_dos_png_size}};
    for (int i = 0; i < 7; ++i)
        if (!logos_[i].img) loadPngTexture(src[i].data, src[i].size, logos_[i]);
}

void Covers::release() {
    clear();
    for (Texture& t : logos_) {
        if (t.img) GRRLIB_FreeTexture(t.img);
        t = Texture();
    }
}

const Texture* Covers::logo(System s) const {
    const Texture& t = logos_[static_cast<int>(s)];
    return t.img ? &t : nullptr;
}

const Texture* Covers::peek(const Game& g) const {
    std::string path = coverPath(g);
    return path.empty() ? nullptr : cache_.peek(path);
}

const Texture* Covers::cover(const Game& g, bool& pending) {
    pending = false;
    std::string path = coverPath(g);
    if (path.empty()) return nullptr;
    // Nur echte Kartenzugriffe zählen (auch Nachladen nach Verdrängung), Treffer und
    // gemerkte Fehlschläge kosten nichts.
    if (!cache_.contains(path) && !cache_.failed(path)) {
        if (loadsLeft_ <= 0) {
            pending = true;
            return nullptr;
        }
        --loadsLeft_;
    }
    return cache_.get(path);
}

void Covers::endFrame() {
    cache_.endFrame();
    loadsLeft_ = kNewLoadsPerFrame;
}

void Covers::forgetFailures() { cache_.forgetFailures(); }

void Covers::clear() { cache_.clear(); }

// --- Layouts -------------------------------------------------------------------------------

MainLayout mainLayout(const UiState& s, const Library& lib, float centerImgW, float centerImgH) {
    MainLayout m;
    m.bar = barLayout(lib);

    const float contentW = W() - 2 * kSafe;
    m.splitX = std::round(kSafe + contentW * kSplit);
    m.leftX = kSafe;
    m.leftW = m.splitX - 14 - kSafe;
    m.labelY = 88;

    m.carArea = Rect{m.leftX, kCarouselTop, m.leftW, kCenterH};
    m.carCenter = fitRect(centerImgW, centerImgH, m.carArea);
    if (m.carCenter.empty()) m.carCenter = fitRect(kCenterW, kCenterH, m.carArea);  // Standardformat
    // Seitencover neben der tatsächlichen Mitte; ist außen kein Platz, rücken sie hinter die
    // Mitte (bei 640 px schon im Hochformat) oder entfallen bei sehr breiten Covern.
    CarouselGeometry car = carouselGeometry(m.carArea, m.carCenter, kSideW, kSideH);
    m.carLeft = car.left;
    m.carRight = car.right;
    m.hitLeft = car.hitLeft;
    m.hitRight = car.hitRight;
    m.titleY = kCarouselTop + kCenterH + 14;
    m.systemY = m.titleY + 30;
    m.hintsY = kBottom - 18;

    const float lettersX = m.splitX + 14;
    std::vector<char> letters = lib.letters();
    for (int i = 0; i < kLetterCells; ++i) {
        int col = i / kLetterRows, row = i % kLetterRows;
        m.letterCells[i] = Rect{lettersX + col * kLetterStep, kListTop + 2 + row * kLetterStep, kLetterCell,
                                kLetterCell};
        char c = i == 0 ? '#' : static_cast<char>('A' + i - 1);
        m.letterChars[i] = c;
        m.letterIndex[i] = -1;
        for (size_t k = 0; k < letters.size(); ++k)
            if (letters[k] == c) m.letterIndex[i] = static_cast<int>(k);
    }

    m.listX = lettersX + 2 * kLetterStep - 2 + 12;
    m.listW = W() - kSafe - m.listX;
    for (int i = 0; i < Library::kPageSize; ++i) m.rows[i] = Rect{m.listX, kListTop + i * kRowH, m.listW, kRowH};
    int visible = static_cast<int>(lib.visible().size());
    int pageStart = (s.listIndex / Library::kPageSize) * Library::kPageSize;
    m.rowCount = std::clamp(visible - pageStart, 0, Library::kPageSize);
    return m;
}

SystemsLayout systemsLayout() {
    SystemsLayout l;
    const float w = 300, rowH = 36;
    const float h = 44 + 7 * rowH + 12;
    l.panel = Rect{std::round((W() - w) / 2), std::round((480 - h) / 2), w, h};
    l.titleY = l.panel.y + 16;
    for (int i = 0; i < 7; ++i) l.rows[i] = Rect{l.panel.x + 12, l.panel.y + 44 + i * rowH, w - 24, rowH};
    return l;
}

SearchLayout searchLayout(const UiState& s, const Library& lib) {
    SearchLayout l;
    const float w = std::min(W() - 2 * kSafe, 440.0f);
    const float x = std::round((W() - w) / 2), y = kSafe;
    l.field = Rect{x + 16, y + 16, w - 32, 36};

    // Fünf Tastenreihen (Ziffern, QWERTZ, Leer/Löschen) plus 8 Treffer: Panel endet bei
    // kSafe + 416 = 448 = kBottom (Rechnung: 60 + 5 * 32 - 4 + 12 + 8 * 22 + 12).
    const float key = 28, step = 32;
    const float kbW = 10 * step - 4;
    const float kbX = std::round(x + (w - kbW) / 2);
    const float ky = y + 60;
    const float rowOffset[4] = {0, 0, 16, 32};  // Ziffern, q-, a-, y-Reihe
    const auto& rows = keyboardRows();
    const size_t last = rows.size() - 1;
    for (size_t r = 0; r < rows.size(); ++r) {
        float ry = ky + r * step;
        if (r < last && r < 4) {
            for (int c = 0; c < rows[r]; ++c) l.keys.push_back(Rect{kbX + rowOffset[r] + c * step, ry, key, key});
        } else {  // Leer, Löschen
            l.keys.push_back(Rect{kbX + 16, ry, 180, key});
            l.keys.push_back(Rect{kbX + 16 + 184, ry, kbW - 16 - 184, key});
        }
    }
    l.resultsY = ky + rows.size() * step - 4 + 12;
    int n = static_cast<int>(searchResults(s, lib).size());
    for (int i = 0; i < n; ++i) l.results.push_back(Rect{x + 16, l.resultsY + i * 22, w - 32, 22});
    float h = (l.resultsY - y) + std::max(n, 1) * 22 + 12;
    l.panel = Rect{x, y, w, h};
    return l;
}

HomeLayout homeLayout() {
    HomeLayout l;
    const float w = 280, rowH = 40, h = 12 + 3 * rowH + 12;
    l.panel = Rect{std::round((W() - w) / 2), std::round((480 - h) / 2), w, h};
    for (int i = 0; i < 3; ++i) l.rows[i] = Rect{l.panel.x + 12, l.panel.y + 12 + i * rowH, w - 24, rowH};
    return l;
}

NoUsbLayout noUsbLayout() {
    NoUsbLayout l;
    l.titleY = 176;
    float bw = std::round(gfx::textWidth(kSizeList, "Erneut suchen") + 48);
    l.button = Rect{std::round((W() - bw) / 2), 222, bw, 40};
    l.hintY = 282;
    return l;
}

// --- Hit-Test ------------------------------------------------------------------------------

Hit hitTest(float x, float y, const UiState& s, const Library& lib, const std::vector<const Game*>& recent,
            const Covers* covers) {
    const auto none = std::make_pair(static_cast<int>(Region::None), 0);
    auto hit = [](Region r, int i) { return Hit(std::make_pair(static_cast<int>(r), i)); };

    switch (s.overlay) {
        case Overlay::Systems: {
            SystemsLayout l = systemsLayout();
            if (!l.panel.contains(x, y)) return none;
            for (int i = 0; i < 7; ++i)
                if (l.rows[i].contains(x, y)) return hit(Region::SystemToggle, i);
            return std::nullopt;
        }
        case Overlay::Search: {
            SearchLayout l = searchLayout(s, lib);
            if (!l.panel.contains(x, y)) return none;
            for (size_t i = 0; i < l.keys.size(); ++i)
                if (l.keys[i].contains(x, y)) return hit(Region::Key, static_cast<int>(i));
            for (size_t i = 0; i < l.results.size(); ++i)
                if (l.results[i].contains(x, y)) return hit(Region::Result, static_cast<int>(i));
            return std::nullopt;
        }
        case Overlay::Home: {
            HomeLayout l = homeLayout();
            if (!l.panel.contains(x, y)) return none;
            for (int i = 0; i < 3; ++i)
                if (l.rows[i].contains(x, y)) return hit(Region::HomeEntry, i);
            return std::nullopt;
        }
        case Overlay::None: break;
    }

    if (s.noUsb) {
        if (noUsbLayout().button.contains(x, y)) return hit(Region::Retry, 0);
        return none;
    }

    const Game* center = carouselGame(s, recent, lib);
    const Texture* centerTex = (covers && center) ? covers->peek(*center) : nullptr;
    MainLayout m = centerTex ? mainLayout(s, lib, static_cast<float>(centerTex->w), static_cast<float>(centerTex->h))
                             : mainLayout(s, lib);
    for (size_t i = 0; i < m.bar.tabs.size(); ++i)
        if (m.bar.tabs[i].contains(x, y)) return hit(Region::Tab, static_cast<int>(i));
    if (m.bar.search.contains(x, y)) return hit(Region::SearchField, 0);
    if (m.carCenter.contains(x, y)) return hit(Region::CarouselCenter, 0);
    const Game *left = nullptr, *right = nullptr;
    carouselSides(s, lib, recent, left, right);
    if (left && m.hitLeft.contains(x, y)) return hit(Region::CarouselLeft, 0);
    if (right && m.hitRight.contains(x, y)) return hit(Region::CarouselRight, 0);
    for (int i = 0; i < m.rowCount; ++i)
        if (m.rows[i].contains(x, y)) return hit(Region::ListRow, i);
    for (int i = 0; i < kLetterCells; ++i)
        if (m.letterIndex[i] >= 0 && m.letterCells[i].contains(x, y)) return hit(Region::Letter, m.letterIndex[i]);
    return none;
}

// --- Zeichnen ------------------------------------------------------------------------------

void drawMain(const UiState& s, const Library& lib, const std::vector<const Game*>& recent, Covers& covers,
              const Hit& hover, float slide) {
    // Das Mittelcover bestimmt die Geometrie des Karussells, deshalb zuerst holen (zählt zum
    // Ladebudget dieses Bilds; drawCover trifft danach den Cache).
    const Game* center = carouselGame(s, recent, lib);
    bool centerPending = false;
    const Texture* centerTex = center ? covers.cover(*center, centerPending) : nullptr;
    MainLayout m = centerTex ? mainLayout(s, lib, static_cast<float>(centerTex->w), static_cast<float>(centerTex->h))
                             : mainLayout(s, lib);
    drawBar(m.bar, s, hover);

    // Linke Spalte: Karussell
    if (!recent.empty()) gfx::text(m.leftX, m.labelY, kSizeSmall, kMuted, "Zuletzt gespielt");

    const Game *left = nullptr, *right = nullptr;
    carouselSides(s, lib, recent, left, right);
    // Gleiten: alles um dx versetzt, aber nie über die linke Spalte hinaus (breite Cover
    // gleiten entsprechend kürzer, das Ausblenden der Mitte bleibt).
    const float dx = std::round(slide * kSlidePx);
    const float colL = m.carArea.x, colR = m.carArea.x + m.carArea.w;
    auto shifted = [dx, colL, colR](const Rect& r) { return slideWithin(r, dx, colL, colR); };

    const Game* sides[2] = {left, right};
    const Rect sideRects[2] = {shifted(m.carLeft), shifted(m.carRight)};
    const Region sideRegions[2] = {Region::CarouselLeft, Region::CarouselRight};
    for (int i = 0; i < 2; ++i) {
        if (!sides[i] || sideRects[i].empty()) continue;
        Rect r = drawCover(covers, *sides[i], sideRects[i], false);
        bool hovered = hitIs(hover, sideRegions[i]) && s.overlay == Overlay::None;
        gfx::rect(r.x, r.y, r.w, r.h, hovered ? 0x00000040 : 0x00000080);  // abgedunkelt
    }
    if (center) {
        Rect r = shifted(m.carCenter);
        bool focused = s.focus == Focus::Carousel && s.overlay == Overlay::None;
        if (focused)
            gfx::rect(r.x - 3, r.y - 3, r.w + 6, r.h + 6, kAccent);
        else if (hitIs(hover, Region::CarouselCenter) && s.overlay == Overlay::None)
            gfx::rect(r.x - 2, r.y - 2, r.w + 4, r.h + 4, 0xE8E8E866);
        drawCover(covers, *center, r, false, 1.0f - 0.4f * std::fabs(slide));

        std::string t = gfx::fitText(kSizeTitle, m.leftW, center->title);
        gfx::text(std::round(m.leftX + (m.leftW - gfx::textWidth(kSizeTitle, t)) / 2), m.titleY, kSizeTitle, kText,
                  t);
        const char* sys = systemName(center->system);
        gfx::text(std::round(m.leftX + (m.leftW - gfx::textWidth(kSizeSmall, sys)) / 2), m.systemY, kSizeSmall,
                  kMuted, sys);
    }

    // Tastenhinweise (Entscheidung des Leads: nur diese drei, klein und gedämpft)
    {
        const char* hints[3] = {"+ Systeme", "\xE2\x80\x93 Suche", "1/2 Seite"};
        float widths[3], total = 0;
        for (int i = 0; i < 3; ++i) total += widths[i] = gfx::textWidth(kSizeSmall, hints[i]);
        float gap = std::clamp((m.leftW - total) / 2, 8.0f, 24.0f);
        float x = m.leftX;
        for (int i = 0; i < 3; ++i) {
            gfx::text(std::round(x), m.hintsY, kSizeSmall, kMuted, hints[i]);
            x += widths[i] + gap;
        }
    }

    // Trennlinie
    gfx::rect(m.splitX, kListTop, 1, Library::kPageSize * kRowH, kBorder);

    // Buchstabenspalte
    const auto& vis = lib.visible();
    char currentLetter = (s.listIndex >= 0 && s.listIndex < static_cast<int>(vis.size())) ? vis[s.listIndex]->letter : 0;
    for (int i = 0; i < kLetterCells; ++i) {
        const Rect& r = m.letterCells[i];
        std::string label(1, m.letterChars[i]);
        int li = m.letterIndex[i];
        if (li < 0) {
            textIn(r, kSizeSmall, withAlpha(kMuted, 0.35f), label, true);
            continue;
        }
        bool focused = s.focus == Focus::Letters && s.letterIndex == li && s.overlay == Overlay::None;
        uint32_t fill = focused ? kAccent : (m.letterChars[i] == currentLetter ? kChipHover : kRowSel);
        roundRectR(r, 4, fill);
        textIn(r, kSizeSmall, focused ? kWhite : kText, label, true);
    }

    // Liste
    if (vis.empty()) {
        gfx::text(m.listX + 6, textTop(m.rows[0].y + kRowH / 2, kSizeList), kSizeList, kMuted, "Keine Spiele");
    } else {
        int page = s.listIndex / Library::kPageSize;
        int sel = s.listIndex % Library::kPageSize;
        std::vector<const Game*> games = lib.page(page);
        for (size_t i = 0; i < games.size() && i < static_cast<size_t>(Library::kPageSize); ++i) {
            const Rect& r = m.rows[i];
            if (static_cast<int>(i) == sel) {
                if (s.focus == Focus::List && s.overlay == Overlay::None)
                    focusRow(r);
                else
                    roundRectR(r, 6, kRowSel);
            }
            drawCover(covers, *games[i], Rect{r.x + 6, r.y + (kRowH - kMiniH) / 2, kMiniW, kMiniH}, true);
            float tx = r.x + 6 + kMiniW + 10;
            Rect tr{tx, r.y, r.x + r.w - 8 - tx, r.h};
            textIn(tr, kSizeList, kText, games[i]->title, false);
        }
    }
}

void drawNoUsb(const Hit& hover, const std::string& reason) {
    NoUsbLayout l = noUsbLayout();
    const char* title = "USB-Platte nicht gefunden";
    gfx::text(std::round((W() - gfx::textWidth(kSizeTitle, title)) / 2), l.titleY, kSizeTitle, kText, title);
    roundRectR(l.button, 10, hitIs(hover, Region::Retry) ? 0x5B95F7FF : kAccent);
    textIn(l.button, kSizeList, kWhite, "Erneut suchen", true);
    const char* hint = "A: Erneut suchen";
    gfx::text(std::round((W() - gfx::textWidth(kSizeSmall, hint)) / 2), l.hintY, kSizeSmall, kMuted, hint);
    // Ergebnis von mountUsb (dasselbe steht im Diagnose-Log).
    if (!reason.empty()) {
        std::string r = gfx::fitText(kSizeSmall, W() - 2 * kSafe, reason);
        gfx::text(std::round((W() - gfx::textWidth(kSizeSmall, r)) / 2), l.hintY + 40, kSizeSmall, kMuted, r);
    }
}

void drawOverlay(Overlay o, const UiState& s, const Library& lib, float a) {
    if (o == Overlay::None || a <= 0) return;
    dim(a);
    switch (o) {
        case Overlay::Systems: {
            SystemsLayout l = systemsLayout();
            panel(l.panel, a);
            gfx::text(l.panel.x + 24, l.titleY, kSizeSmall, withAlpha(kMuted, a), "Systeme anzeigen");
            for (int i = 0; i < 7; ++i) {
                const Rect& r = l.rows[i];
                if (i == s.systemsIndex) focusRow(r, a);
                System sys = static_cast<System>(i);
                Rect tr{r.x + 12, r.y, r.w - 80, r.h};
                textIn(tr, kSizeList, withAlpha(kText, a), systemLabel(sys), false);
                bool on = s.systems.get(sys);
                Rect sw{r.x + r.w - 12 - 42, r.y + (r.h - 22) / 2, 42, 22};
                roundRectR(sw, 11, withAlpha(on ? kAccent : 0x3A4150FF, a));
                float kx = on ? sw.x + sw.w - 3 - 16 : sw.x + 3;
                gfx::roundRect(kx, sw.y + 3, 16, 16, 8, withAlpha(kWhite, a));
            }
            break;
        }
        case Overlay::Search: {
            SearchLayout l = searchLayout(s, lib);
            panel(l.panel, a);
            roundRectR(l.field, 6, withAlpha(kBg, a));
            float cy = l.field.y + l.field.h / 2;
            if (s.query.empty()) {
                magnifier(l.field.x + 18, cy, withAlpha(kMuted, a));
                gfx::text(l.field.x + 34, textTop(cy, kSizeList), kSizeList, withAlpha(kMuted, a), "Suchen");
            } else {
                std::string q = gfx::fitText(kSizeTitle, l.field.w - 28, s.query);
                float tx = l.field.x + 12;
                gfx::text(tx, textTop(cy, kSizeTitle), kSizeTitle, withAlpha(kText, a), q);
                gfx::rect(std::round(tx + gfx::textWidth(kSizeTitle, q) + 2), cy - 11, 2, 22, withAlpha(kAccent, a));
            }
            const auto& keys = keyboardKeys();
            for (size_t i = 0; i < l.keys.size() && i < keys.size(); ++i) {
                bool focused = !s.searchInResults && s.keyboardIndex == static_cast<int>(i);
                roundRectR(l.keys[i], 6, withAlpha(focused ? kAccent : kChip, a));
                std::string label = keys[i];
                if (label.size() == 1) label[0] = static_cast<char>(std::toupper(static_cast<unsigned char>(label[0])));
                int size = label.size() == 1 ? kSizeList : kSizeSmall;
                textIn(l.keys[i], size, withAlpha(focused ? kWhite : kText, a), label, true);
            }
            std::vector<const Game*> results = searchResults(s, lib);
            if (results.empty() && !s.query.empty()) {
                gfx::text(l.panel.x + 24, textTop(l.resultsY + 11, kSizeSmall), kSizeSmall, withAlpha(kMuted, a),
                          "Keine Treffer");
            }
            for (size_t i = 0; i < results.size() && i < l.results.size(); ++i) {
                const Rect& r = l.results[i];
                if (s.searchInResults && s.resultIndex == static_cast<int>(i)) focusRow(r, a);
                textIn(Rect{r.x + 8, r.y, r.w - 16, r.h}, kSizeList, withAlpha(kText, a), results[i]->title, false);
            }
            break;
        }
        case Overlay::Home: {
            HomeLayout l = homeLayout();
            panel(l.panel, a);
            const auto& entries = homeEntries();
            for (int i = 0; i < 3 && i < static_cast<int>(entries.size()); ++i) {
                const Rect& r = l.rows[i];
                if (i == s.homeIndex) focusRow(r, a);
                textIn(Rect{r.x + 12, r.y, r.w - 24, r.h}, kSizeList, withAlpha(kText, a), entries[i], false);
            }
            break;
        }
        case Overlay::None: break;
    }
}

void drawMessagePanel(const std::string& title, const std::string& detail, float a) {
    if (a <= 0) return;
    dim(a);
    float maxW = W() - 2 * kSafe;
    float tw = std::max(gfx::textWidth(kSizeTitle, title), gfx::textWidth(kSizeSmall, detail));
    float w = std::min(maxW, std::round(tw + 48));
    float h = detail.empty() ? 76 : 104;
    Rect p{std::round((W() - w) / 2), std::round((480 - h) / 2), w, h};
    panel(p, a);
    textIn(Rect{p.x + 24, p.y + 20, w - 48, 30}, kSizeTitle, withAlpha(kText, a), title, true);
    if (!detail.empty())
        textIn(Rect{p.x + 24, p.y + 60, w - 48, 22}, kSizeSmall, withAlpha(kMuted, a), detail, true);
}

void drawCenteredMessage(const std::string& title, const std::string& detail) {
    std::string t = gfx::fitText(kSizeTitle, W() - 2 * kSafe, title);
    gfx::text(std::round((W() - gfx::textWidth(kSizeTitle, t)) / 2), 214, kSizeTitle, kText, t);
    if (!detail.empty()) {
        std::string d = gfx::fitText(kSizeSmall, W() - 2 * kSafe, detail);
        gfx::text(std::round((W() - gfx::textWidth(kSizeSmall, d)) / 2), 250, kSizeSmall, kMuted, d);
    }
}

void drawFade(float alpha) {
    if (alpha <= 0) return;
    gfx::rect(0, 0, W(), 480, withAlpha(0x000000FF, alpha));
}

void drawPointer(float x, float y) {
    gfx::ring(x, y, 10, 7, 0x000000B0);  // 2 px dunkler Rand innen und außen
    gfx::ring(x, y, 10, 3, kWhite);
}

}  // namespace screens
