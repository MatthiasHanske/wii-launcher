#pragma once
// Zeichenschicht über GRRLIB. Alle Koordinaten sind logische Pixel:
// Höhe immer 480, Breite 640 (4:3) oder 854 (16:9 laut Systemeinstellung). Die Projektion
// bildet diese Fläche auf den ganzen Framebuffer ab; bei 16:9 staucht sie also horizontal
// um 3/4, und der Fernseher zieht das Bild wieder auf 16:9 auseinander. Inhalte erscheinen
// damit unverzerrt, Layouts bekommen bei 16:9 die zusätzliche Breite.
// Farben: 0xRRGGBBAA wie bei GRRLIB.
//
// Bildprotokoll (pro Bild genau einmal, in dieser Reihenfolge):
//     gfx::beginFrame();
//     ... zeichnen, TexCache::get(), gfx::text() ...
//     gfx::endFrame();     // GRRLIB_Render (wartet auf die GPU), danach Textcache aufräumen
//     covers.endFrame();   // jeder TexCache: Cover-Texturen über Budget freigeben
// Texturen, die im laufenden Bild benutzt wurden, werden vor endFrame nie freigegeben,
// weil die GPU erst in GRRLIB_Render fertig zeichnet.
#include <cstdint>
#include <string>

struct GRRLIB_texImg;

// Textur von GRRLIB (Speicher gehört dem Erzeuger, z. B. TexCache).
struct Texture {
    GRRLIB_texImg* img = nullptr;
    int w = 0;     // echte Bildgröße (aus der PNG)
    int h = 0;
    int texW = 0;  // Texturgröße, von PNGU auf Vielfache von 4 aufgefüllt
    int texH = 0;
};

namespace gfx {

// GRRLIB_Init, Projektion, Schrift laden. false wenn GRRLIB_Init fehlschlägt.
bool init();
// Textcache und Schrift freigeben, GRRLIB_Exit.
void shutdown();

void beginFrame(uint32_t clearRgba = 0x0F1218FF);
void endFrame();

int width();   // 640 oder 854
int height();  // 480
bool widescreen();

void rect(float x, float y, float w, float h, uint32_t rgba);
// Senkrechter Verlauf von top (oben) nach bottom (unten).
void gradientRect(float x, float y, float w, float h, uint32_t top, uint32_t bottom);
void roundRect(float x, float y, float w, float h, float radius, uint32_t rgba);
// Ring (Kreislinie mit Dicke), z. B. für den Zeiger.
void ring(float cx, float cy, float radius, float thickness, uint32_t rgba);

// y ist die Oberkante der Zeile, die Grundlinie liegt bei y + size.
// Jeder (utf8, size) wird einmal weiß in eine Textur gerendert (FreeType direkt) und beim
// Zeichnen mit rgba eingefärbt; Cache 2 MB, LRU. Zeilen über 1024 px werden abgeschnitten.
// Gerendert wird in physischen EFB-Pixeln (bei 16:9 horizontal gestaucht) und 1:1 gezeichnet.
void text(float x, float y, int size, uint32_t rgba, const std::string& utf8);
// Summe der gecachten Vorschübe je (Codepoint, Größe) in logischen Pixeln; ohne Kerning,
// wie text().
float textWidth(int size, const std::string& utf8);
// ellipsize mit Memo (je Text, Breite, Größe); nach dem ersten Aufruf ohne FreeType-Arbeit.
std::string fitText(int size, float maxWidth, const std::string& utf8);

// Textur (echter Bildbereich, ohne Auffüllung) gestreckt auf w x h, alpha 0..1.
void image(const Texture& tex, float x, float y, float w, float h, float alpha = 1.0f);

// Platzhalter-Kachel: dunkler Verlauf, logo (falls vorhanden) oben mittig, title unten
// (mit "…" gekürzt).
void placeholder(float x, float y, float w, float h, const Texture* logo, const std::string& title);

}  // namespace gfx
