#pragma once

namespace app {

// Ganzes Programm: SD einhängen, Grafik und Wiimote starten, USB einhängen, Spiele scannen,
// Hauptschleife. Kehrt nie zurück: Fehlen SD oder Grafik, geht es ohne Anzeige ins Wii-Menü
// (Priiloader-Magic "Pune", kein erneuter Autoboot); sonst verlässt nur Home/Power/Reset
// oder ein Spielstart das Programm.
[[noreturn]] void run();

}  // namespace app
