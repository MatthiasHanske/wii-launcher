#include "wii/demo.h"

#include <grrlib.h>
#include <wiiuse/wpad.h>

#include <cstdio>
#include <string>

#include "core/png_check.h"
#include "logo_wii_png.h"
#include "wii/fs_wii.h"
#include "wii/gfx.h"
#include "wii/input.h"
#include "wii/texcache.h"

namespace {

constexpr size_t kTextureBudget = 24 * 1024 * 1024;
// Erstes vorhandenes Cover; tools/dolphin-sd enthält eigene Demo-Cover (tools/make-demo-covers.py).
const char* const kCoverPaths[] = {
    "sd:/config/images/2D/ZWRP01.png", "sd:/config/images/2D/ZLTP01.png",
    "sd:/config/images/2D/ZGLP01.png", "sd:/config/images/2D/ZOKP01.png"};
const char kBrokenPath[] = "sd:/launcher/covers/wii/kaputt.png";

}  // namespace

void runGfxDemo(WiiFs& fs) {
    if (!gfx::init()) return;
    WPAD_Init();
    inputInit(gfx::width(), gfx::height());
    bool sdOk = fs.mountSd();

    TexCache cache(kTextureBudget);
    Texture logo;
    PngInfo logoInfo;
    if (checkPng(logo_wii_png, logo_wii_png_size, logoInfo)) {
        logo.img = GRRLIB_LoadTexturePNG(logo_wii_png);
        if (logo.img && logo.img->data) {
            logo.w = static_cast<int>(logoInfo.width);
            logo.h = static_cast<int>(logoInfo.height);
            logo.texW = static_cast<int>(logo.img->w);
            logo.texH = static_cast<int>(logo.img->h);
        }
    }

    while (true) {
        InputState in = pollInput();
        if (in.pressed & WPAD_BUTTON_HOME) break;

        gfx::beginFrame(0x0F1218FF);
        const float W = static_cast<float>(gfx::width());

        gfx::text(40, 24, 22, 0xF9FAFBFF, "Wii Launcher \xE2\x80\x93 Grafik-Demo");
        gfx::text(40, 56, 16, 0xD1D5DBFF, "Schrift 16 px: \xC3\x84pfel, \xC3\x96l, \xC3\x9C" "bermut, Stra\xC3\x9F" "e \xE2\x80\xA6");
        char info[160];
        std::snprintf(info, sizeof info, "14 px: %dx%d %s, SD %s, Cache %u KB in %u Texturen",
                      gfx::width(), gfx::height(), gfx::widescreen() ? "(16:9)" : "(4:3)",
                      sdOk ? "ok" : "fehlt", unsigned(cache.usedBytes() / 1024),
                      unsigned(cache.count()));
        gfx::text(40, 82, 14, 0x9CA3AFFF, info);

        // Cover 160x224 (Seitenverhältnis der 2D-Cover)
        // Fehlschläge merkt sich der Cache, die Suche kostet also nur im ersten Bild.
        const Texture* cover = nullptr;
        for (const char* path : kCoverPaths)
            if ((cover = cache.get(path)) != nullptr) break;
        if (cover) {
            gfx::image(*cover, 40, 120, 160, 224, 1.0f);
        } else {
            gfx::rect(40, 120, 160, 224, 0x7F1D1DFF);
            gfx::text(52, 220, 14, 0xFFFFFFFF, "Cover fehlt");
        }

        gfx::placeholder(220, 120, 160, 224, logo.img ? &logo : nullptr,
                         "Wolkenrennen \xE2\x80\x93 Ein sehr langer Demo-Titel");

        // Panel mit runden Ecken, rechts verankert (nutzt bei 16:9 die zusätzliche Breite)
        float px = W - 40 - 220;
        gfx::roundRect(px, 120, 220, 224, 12, 0x1F2937CC);
        bool brokenNull = cache.get(kBrokenPath) == nullptr;
        gfx::text(px + 14, 136, 16, 0xF9FAFBFF, "Status");
        gfx::text(px + 14, 164, 14, brokenNull ? 0x22C55EFF : 0xEF4444FF,
                  brokenNull ? "kaputt.png: nullptr (ok)" : "kaputt.png: geladen?!");
        std::snprintf(info, sizeof info, "Zeiger: %s", in.pointerValid ? "ja" : "nein");
        gfx::text(px + 14, 186, 14, 0xD1D5DBFF, info);
        if (in.pointerValid) {
            std::snprintf(info, sizeof info, "x %.0f  y %.0f", in.pointerX, in.pointerY);
            gfx::text(px + 14, 208, 14, 0xD1D5DBFF, info);
        }
        gfx::text(px + 14, 316, 14, 0x9CA3AFFF, "Home: beenden");

        if (in.pointerValid) gfx::ring(in.pointerX, in.pointerY, 12, 3, 0xFFFFFFE6);

        gfx::endFrame();
        cache.endFrame();
    }

    cache.clear();
    if (logo.img) GRRLIB_FreeTexture(logo.img);
    fs.unmountAll();
    gfx::shutdown();
}
