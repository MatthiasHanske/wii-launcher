#pragma once
#include <string>

#include "core/model.h"

// Coverpfad auf der SD, leer wenn es keinen geben kann (Wii/GameCube ohne Spiel-ID).
// Wii/GameCube: sd:/config/images/2D/<ID>.png (USB Loader GX)
// Emulatoren:   sd:/launcher/covers/<systemKey>/<Dateiname ohne Endung>.png
// Adventure:    sd:/launcher/covers/dos/<Ordnername>.png
std::string coverPath(const Game& g);
