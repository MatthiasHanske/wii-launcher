#pragma once
// Zeichenschicht über GRRLIB. Alle Koordinaten sind logische Pixel:
// Höhe immer 480, Breite 640 (4:3) oder 854 (16:9 laut Systemeinstellung). Die Projektion
// bildet diese Fläche auf den ganzen Framebuffer ab; bei 16:9 staucht sie also horizontal
// um 3/4, und der Fernseher zieht das Bild wieder auf 16:9 auseinander. Inhalte erscheinen
// damit unverzerrt, Layouts bekommen bei 16:9 die zusätzliche Breite.
// Farben: 0xRRGGBBAA wie bei GRRLIB.
#include <cstdint>
#include <string>

struct GRRLIB_texImg;

// Textur von GRRLIB (Speicher gehört dem Erzeuger, z. B. TexCache).
struct Texture {
    GRRLIB_texImg* img = nullptr;
    int w = 0;
    int h = 0;
};

namespace gfx {

// GRRLIB_Init, Projektion, Schrift laden. false wenn GRRLIB_Init fehlschlägt.
bool init();
// Schrift freigeben, GRRLIB_Exit.
void shutdown();

void beginFrame(uint32_t clearRgba = 0x0F1218FF);
void endFrame();  // GRRLIB_Render (wartet auf die GPU und zeigt das Bild)

int width();   // 640 oder 854
int height();  // 480
bool widescreen();

void rect(float x, float y, float w, float h, uint32_t rgba);
// Senkrechter Verlauf von top (oben) nach bottom (unten).
void gradientRect(float x, float y, float w, float h, uint32_t top, uint32_t bottom);
void roundRect(float x, float y, float w, float h, float radius, uint32_t rgba);
// Ring (Kreislinie mit Dicke), z. B. für den Zeiger.
void ring(float cx, float cy, float radius, float thickness, uint32_t rgba);

// y ist die Oberkante der Zeile (Grundlinie bei y + size). utf8 wird selbst dekodiert.
void text(float x, float y, int size, uint32_t rgba, const std::string& utf8);
float textWidth(int size, const std::string& utf8);

// Textur gestreckt auf w x h, alpha 0..1.
void image(const Texture& tex, float x, float y, float w, float h, float alpha = 1.0f);

// Platzhalter-Kachel: dunkler Verlauf, logo (falls vorhanden) oben mittig, title unten
// (mit "…" gekürzt).
void placeholder(float x, float y, float w, float h, const Texture* logo, const std::string& title);

}  // namespace gfx
