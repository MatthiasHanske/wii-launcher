#include "wii/gfx.h"

#include <grrlib.h>
#include <ogc/conf.h>

#include <algorithm>
#include <cmath>
#include <vector>

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

std::wstring toWide(const std::string& utf8) {
    std::vector<uint32_t> cps = utf8Decode(utf8);
    std::wstring w;
    w.reserve(cps.size());
    for (uint32_t cp : cps) w.push_back(static_cast<wchar_t>(cp));
    return w;
}

}  // namespace

bool init() {
    if (GRRLIB_Init() != 0) return false;
    gWidth = CONF_GetAspectRatio() == CONF_ASPECT_16_9 ? 854 : 640;
    loadProjection();
    gFont = GRRLIB_LoadTTF(font_ttf, static_cast<s32>(font_ttf_size));
    return true;
}

void shutdown() {
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

void endFrame() { GRRLIB_Render(); }

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
    std::wstring w = toWide(utf8);
    GRRLIB_PrintfTTFW(static_cast<int>(std::lround(x)), static_cast<int>(std::lround(y)), gFont,
                      w.c_str(), static_cast<unsigned>(size), rgba);
}

float textWidth(int size, const std::string& utf8) {
    if (!gFont || utf8.empty() || size <= 0) return 0;
    std::wstring w = toWide(utf8);
    return static_cast<float>(GRRLIB_WidthTTFW(gFont, w.c_str(), static_cast<unsigned>(size)));
}

void image(const Texture& tex, float x, float y, float w, float h, float alpha) {
    if (!tex.img || tex.w <= 0 || tex.h <= 0 || w <= 0 || h <= 0) return;
    float a = std::clamp(alpha, 0.0f, 1.0f);
    u32 color = 0xFFFFFF00u | static_cast<u32>(std::lround(a * 255));
    GRRLIB_DrawImg(x, y, tex.img, 0, w / tex.w, h / tex.h, color);
}

void placeholder(float x, float y, float w, float h, const Texture* logo, const std::string& title) {
    gradientRect(x, y, w, h, 0x1F2937FF, 0x111827FF);
    constexpr int kTitleSize = 14;
    constexpr float kPad = 6;
    if (logo && logo->img) {
        float side = std::min({64.0f, w * 0.5f, h * 0.5f});
        image(*logo, x + (w - side) / 2, y + h * 0.18f, side, side, 0.9f);
    }
    std::string t = ellipsize(title, w - 2 * kPad,
                              [](const std::string& s) { return textWidth(kTitleSize, s); });
    if (!t.empty()) {
        float tw = textWidth(kTitleSize, t);
        text(x + (w - tw) / 2, y + h - kTitleSize - 2 * kPad, kTitleSize, 0xE5E7EBFF, t);
    }
}

}  // namespace gfx
