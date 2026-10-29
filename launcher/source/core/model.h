#pragma once
#include <string>

enum class System { Wii, GameCube, N64, SNES, NES, Sega, Adventure };

inline const char* systemName(System s) {
    switch (s) {
        case System::Wii: return "Wii";
        case System::GameCube: return "GameCube";
        case System::N64: return "N64";
        case System::SNES: return "SNES";
        case System::NES: return "NES";
        case System::Sega: return "Sega";
        case System::Adventure: return "Adventure";
    }
    return "";
}

// Kurzschluessel fuer Pfade.
inline const char* systemKey(System s) {
    switch (s) {
        case System::Wii: return "wii";
        case System::GameCube: return "gc";
        case System::N64: return "n64";
        case System::SNES: return "snes";
        case System::NES: return "nes";
        case System::Sega: return "sega";
        case System::Adventure: return "dos";
    }
    return "";
}

struct Game {
    System system = System::Wii;
    std::string title, sortKey;
    char letter = '#';
    std::string gameId;
    std::string dir;     // Ordner (bei GameCube/Adventure der Spielordner)
    std::string file;    // Dateiname, leer bei GameCube/Adventure
    std::string subdir;  // Sega: md|sms|sg
};
