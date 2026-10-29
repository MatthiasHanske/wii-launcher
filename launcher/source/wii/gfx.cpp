#include "wii/gfx.h"

#include <ft2build.h>
#include FT_FREETYPE_H
#include <grrlib.h>
#include <ogc/conf.h>

#include <algorithm>
#include <cmath>
#include <unordered_map>
#include <vector>

#include "core/lru.h"
#include "core/text.h"
#include "font_ttf.h"

namespace gfx {
namespace {

constexpr int kHeight = 480;
int gWidth = 640;
GRRLIB_ttfFont* gFont = nullptr;

// Logische Fläche gWidth x 480 auf den ganzen EFB (rmode->fbWidth x efbHeight).
// Ersetzt die Pixel-Projektion aus GRRLIB_2dMode; Viewport bleibt der von GRRLIB.
void loadProjection() {
    Mtx44 m;
    guOrtho(m, 0, kHeight, 0, gWidth, 0, 1000.0f);
    GX_LoadProjectionMtx(m, GX_ORTHOGRAPHIC);
}

// ---- FreeType -----------------------------------------------------------------------

FT_Face face() { return gFont ? static_cast<FT_Face>(gFont->face) : nullptr; }

int gFaceSize = 0;
void setFaceSize(int size) {
    if (gFaceSize == size) return;
    FT_Set_Pixel_Sizes(face(), 0, static_cast<FT_UInt>(size));
    gFaceSize = size;
}

// Vorschub in Pixeln je (Codepoint, Größe). Inter hat kein von GRRLIB/FreeType genutztes
// kern-Table, die Summe der Vorschübe ist also die exakte Zeilenbreite.
std::unordered_map<uint64_t, int> gAdvances;

int advance(uint32_t cp, int size) {
    uint64_t key = (uint64_t(uint32_t(size)) << 32) | cp;
    auto it = gAdvances.find(key);
    if (it != gAdvances.end()) return it->second;
    int adv = 0;
    setFaceSize(size);
    if (FT_Load_Char(face(), cp, FT_LOAD_DEFAULT) == 0)
        adv = static_cast<int>((face()->glyph->advance.x + 32) >> 6);
    if (gAdvances.size() > 8192) gAdvances.clear();
    gAdvances.emplace(key, adv);
    return adv;
}

int widthOf(const std::vector<uint32_t>& cps, int size) {
    int w = 0;
    for (uint32_t cp : cps) w += advance(cp, size);
    return w;
}

// ---- Textcache ------------------------------------------------------------------------

constexpr size_t kTextBudget = 2 * 1024 * 1024;
constexpr int kTextPadX = 1;  // Platz für Glyphen mit negativem bitmap_left
constexpr int kMaxTexSide = 1024;

struct TextTex {
    GRRLIB_texImg* img = nullptr;
    int ascent = 0;      // Zeile der Grundlinie in der Textur
    uint32_t frame = 0;  // Bild der letzten Benutzung
};

uint32_t gFrame = 1;
LruPolicy<std::string> gTextLru;
std::unordered_map<std::string, TextTex> gTexts;
EllipsizeMemo gFitMemo;

int roundUp4(int v) { return (v + 3) & ~3; }

void releaseText(const std::string& key) {
    auto it = gTexts.find(key);
    if (it == gTexts.end()) return;
    GRRLIB_FreeTexture(it->second.img);
    gTexts.erase(it);
}

// Ältesten Eintrag freigeben, sofern er im laufenden Bild nicht gezeichnet wurde.
bool evictOldestText() {
    const std::string* oldest = gTextLru.oldest();
    if (!oldest) return false;
    auto it = gTexts.find(*oldest);
    if (it != gTexts.end() && it->second.frame == gFrame) return false;
    releaseText(gTextLru.evict());
    return true;
}

void clearTexts() {
    while (gTextLru.size() > 0) releaseText(gTextLru.evict());
}

// Rendert die Zeile weiß (Alpha = Glyphendeckung) in eine RGBA8-Textur.
bool renderText(const std::vector<uint32_t>& cps, int size, TextTex& out) {
    FT_Face f = face();
    setFaceSize(size);
    int ascent = static_cast<int>((f->size->metrics.ascender + 63) >> 6);
    int descent = static_cast<int>((-f->size->metrics.descender + 63) >> 6);
    int texW = std::min(kMaxTexSide, roundUp4(widthOf(cps, size) + 2 * kTextPadX));
    int texH = std::min(kMaxTexSide, roundUp4(std::max(1, ascent + descent)));

    size_t bytes = size_t(texW) * size_t(texH) * 4;
    while (gTextLru.totalBytes() + bytes > kTextBudget && evictOldestText()) {
    }
    GRRLIB_texImg* img = GRRLIB_CreateEmptyTexture(static_cast<u32>(texW), static_cast<u32>(texH));
    if (!img || !img->data) {
        if (img) GRRLIB_FreeTexture(img);
        return false;
    }
    GRRLIB_ClearTex(img);

    int pen = kTextPadX;
    for (uint32_t cp : cps) {
        int adv = advance(cp, size);  // vor FT_Load_Char: advance() kann den Slot belegen
        setFaceSize(size);
        if (FT_Load_Char(f, cp, FT_LOAD_RENDER) == 0) {
            const FT_GlyphSlot slot = f->glyph;
            const FT_Bitmap& bm = slot->bitmap;
            if (bm.pixel_mode == FT_PIXEL_MODE_GRAY) {
                int x0 = pen + slot->bitmap_left;
                int y0 = ascent - slot->bitmap_top;
                for (unsigned row = 0; row < bm.rows; ++row) {
                    int y = y0 + static_cast<int>(row);
                    if (y < 0 || y >= texH) continue;
                    const unsigned char* src = bm.buffer + row * bm.pitch;
                    for (unsigned col = 0; col < bm.width; ++col) {
                        int x = x0 + static_cast<int>(col);
                        if (src[col] == 0 || x < 0 || x >= texW) continue;
                        GRRLIB_SetPixelTotexImg(x, y, img, 0xFFFFFF00u | src[col]);
                    }
                }
            }
        }
        pen += adv;
        if (pen >= texW) break;
    }
    GRRLIB_FlushTex(img);

    out.img = img;
    out.ascent = ascent;
    return true;
}

}  // namespace

bool init() {
    if (GRRLIB_Init() != 0) return false;
    gWidth = CONF_GetAspectRatio() == CONF_ASPECT_16_9 ? 854 : 640;
    loadProjection();
    gFont = GRRLIB_LoadTTF(font_ttf, static_cast<s32>(font_ttf_size));
    gFaceSize = 0;
    return true;
}

void shutdown() {
    clearTexts();
    gAdvances.clear();
    gFitMemo.clear();
    if (gFont) {
        GRRLIB_FreeTTF(gFont);
        gFont = nullptr;
    }
    GRRLIB_Exit();
}

void beginFrame(uint32_t clearRgba) {
    // Jedes Bild neu setzen: GRRLIB_2dMode/3dMode (falls je benutzt) überschreiben sie.
    loadProjection();
    GRRLIB_FillScreen(clearRgba);
}

void endFrame() {
    GRRLIB_Render();
    ++gFrame;  // GPU ist fertig: nichts gilt mehr als "im laufenden Bild benutzt"
    while (gTextLru.totalBytes() > kTextBudget && evictOldestText()) {
    }
}

int width() { return gWidth; }
int height() { return kHeight; }
bool widescreen() { return gWidth != 640; }

void rect(float x, float y, float w, float h, uint32_t rgba) {
    if (w <= 0 || h <= 0) return;
    GRRLIB_Rectangle(x, y, w, h, rgba, true);
}

void gradientRect(float x, float y, float w, float h, uint32_t top, uint32_t bottom) {
    if (w <= 0 || h <= 0) return;
    guVector v[4] = {{x, y, 0}, {x + w, y, 0}, {x + w, y + h, 0}, {x, y + h, 0}};
    u32 c[4] = {top, top, bottom, bottom};
    GRRLIB_NGoneFilled(v, c, 4);
}

void roundRect(float x, float y, float w, float h, float radius, uint32_t rgba) {
    if (w <= 0 || h <= 0) return;
    float r = std::min(radius, std::min(w, h) / 2);
    if (r < 1) {
        rect(x, y, w, h, rgba);
        return;
    }
    // Ein konvexes Polygon (Dreiecksfächer), damit sich halbtransparente Teile nicht
    // doppelt überlagern.
    constexpr int kSeg = 6;  // Segmente je Viertelkreis
    guVector v[4 * (kSeg + 1)];
    u32 c[4 * (kSeg + 1)];
    const float cx[4] = {x + w - r, x + w - r, x + r, x + r};
    const float cy[4] = {y + r, y + h - r, y + h - r, y + r};
    int n = 0;
    for (int corner = 0; corner < 4; ++corner) {
        float start = -M_PI / 2 + corner * (M_PI / 2);  // oben rechts, unten rechts, ...
        for (int i = 0; i <= kSeg; ++i) {
            float a = start + (M_PI / 2) * i / kSeg;
            v[n] = {cx[corner] + r * std::cos(a), cy[corner] + r * std::sin(a), 0};
            c[n] = rgba;
            ++n;
        }
    }
    GRRLIB_NGoneFilled(v, c, n);
}

void ring(float cx, float cy, float radius, float thickness, uint32_t rgba) {
    // Als Folge von Vierecken zwischen Innen- und Außenkreis.
    constexpr int kSeg = 32;
    float ri = std::max(0.0f, radius - thickness / 2);
    float ro = radius + thickness / 2;
    u32 c[4] = {rgba, rgba, rgba, rgba};
    for (int i = 0; i < kSeg; ++i) {
        float a0 = 2 * M_PI * i / kSeg;
        float a1 = 2 * M_PI * (i + 1) / kSeg;
        guVector v[4] = {{cx + ro * std::cos(a0), cy + ro * std::sin(a0), 0},
                         {cx + ro * std::cos(a1), cy + ro * std::sin(a1), 0},
                         {cx + ri * std::cos(a1), cy + ri * std::sin(a1), 0},
                         {cx + ri * std::cos(a0), cy + ri * std::sin(a0), 0}};
        GRRLIB_NGoneFilled(v, c, 4);
    }
}

void text(float x, float y, int size, uint32_t rgba, const std::string& utf8) {
    if (!gFont || utf8.empty() || size <= 0) return;
    std::string key = std::to_string(size);
    key += '\x1F';
    key += utf8;

    auto it = gTexts.find(key);
    if (it == gTexts.end()) {
        TextTex t;
        if (!renderText(utf8Decode(utf8), size, t)) return;
        it = gTexts.emplace(key, t).first;
        gTextLru.insert(key, size_t(t.img->w) * size_t(t.img->h) * 4);
    } else {
        gTextLru.touch(key);
    }
    it->second.frame = gFrame;
    // ganze logische Pixel, sonst verschmiert die bilineare Filterung die Glyphen
    float dx = std::round(x) - kTextPadX;
    float dy = std::round(y) + size - it->second.ascent;
    GRRLIB_DrawImg(dx, dy, it->second.img, 0, 1, 1, rgba);
}

float textWidth(int size, const std::string& utf8) {
    if (!gFont || utf8.empty() || size <= 0) return 0;
    return static_cast<float>(widthOf(utf8Decode(utf8), size));
}

std::string fitText(int size, float maxWidth, const std::string& utf8) {
    return gFitMemo.get(utf8, maxWidth, size,
                        [size](const std::string& s) { return textWidth(size, s); });
}

void image(const Texture& tex, float x, float y, float w, float h, float alpha) {
    if (!tex.img || tex.w <= 0 || tex.h <= 0 || w <= 0 || h <= 0) return;
    float a = std::clamp(alpha, 0.0f, 1.0f);
    u32 color = 0xFFFFFF00u | static_cast<u32>(std::lround(a * 255));
    // Nur den echten Bildbereich, damit die Auffüllzeilen/-spalten nicht mitgestreckt werden.
    GRRLIB_DrawPart(x, y, 0, 0, static_cast<f32>(tex.w), static_cast<f32>(tex.h), tex.img, 0,
                    w / tex.w, h / tex.h, color);
}

void placeholder(float x, float y, float w, float h, const Texture* logo, const std::string& title) {
    gradientRect(x, y, w, h, 0x1F2937FF, 0x111827FF);
    constexpr int kTitleSize = 14;
    constexpr float kPad = 6;
    if (logo && logo->img) {
        float side = std::min({64.0f, w * 0.5f, h * 0.5f});
        image(*logo, x + (w - side) / 2, y + h * 0.18f, side, side, 0.9f);
    }
    // Nach dem ersten Bild: Memo-Treffer, gecachte Vorschübe, gecachte Textur.
    std::string t = fitText(kTitleSize, w - 2 * kPad, title);
    if (!t.empty()) {
        float tw = textWidth(kTitleSize, t);
        text(x + (w - tw) / 2, y + h - kTitleSize - 2 * kPad, kTitleSize, 0xE5E7EBFF, t);
    }
}

}  // namespace gfx
