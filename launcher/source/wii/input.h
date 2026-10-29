#pragma once
// Wiimote auf Kanal 0 mit IR-Zeiger (Nunchuk und weitere Kanäle werden ignoriert).
#include <cstdint>

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
