#pragma once
// Wiimote auf Kanal 0 mit IR-Zeiger (Nunchuk und weitere Kanäle werden ignoriert).
// pressed/held enthalten immer nur die Knöpfe der Wiimote selbst (kWiimoteButtons); Bits
// von Erweiterungen (Nunchuk C/Z, Classic Controller) werden auch dann ausgeblendet, wenn
// ein Classic Controller steckt. Soll der später bedienen, gehört das hier ergänzt.
#include <cstdint>

// WPAD_BUTTON_2/1/B/A/MINUS/HOME/LEFT/RIGHT/DOWN/UP/PLUS (= WIIMOTE_BUTTON_ALL aus wiiuse)
constexpr uint32_t kWiimoteButtons = 0x1F9F;

struct InputState {
    float pointerX = 0;  // logische Pixel (siehe gfx::width/height)
    float pointerY = 0;
    bool pointerValid = false;
    uint32_t pressed = 0;  // WPAD_BUTTON_* seit dem letzten Aufruf gedrückt
    uint32_t held = 0;     // WPAD_BUTTON_* gerade gehalten
};

// Nach WPAD_Init: Datenformat mit IR und die Zeigerauflösung setzen (logische Größe).
void inputInit(int width, int height);

// WPAD_ScanPads und Zustand von Kanal 0 lesen. Einmal pro Bild aufrufen.
InputState pollInput();
