#pragma once
// Grafik-Demo für Dolphin/Wii (nur mit make launcher LAUNCHER_GFX_DEMO=1 aufgerufen).
class WiiFs;

// Initialisiert Grafik und Wiimote selbst, hängt SD ein, läuft bis Home, räumt auf.
void runGfxDemo(WiiFs& fs);
