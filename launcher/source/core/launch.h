#pragma once
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "core/fs.h"
#include "core/model.h"

// Was der Aufrufer zum Starten eines Spiels braucht. makeLaunchPlan schreibt selbst nichts;
// filesToWrite muss der Aufrufer vor dem Sprung in dolPath schreiben.
struct LaunchPlan {
    std::string dolPath;
    std::vector<std::string> args;
    std::vector<std::pair<std::string, std::string>> filesToWrite;  // Pfad, Inhalt
    // false: Wii/GameCube ohne bekannte Spiel-ID, USB Loader GX oeffnet nur seine Liste.
    bool quickBoot = true;
};

LaunchPlan makeLaunchPlan(const Game& game, IFileSystem& fs);

// Startbefehl fuer DOSBox: erste Zeile von autoexec.txt > einzige .BAT/.EXE > beste
// Namensaehnlichkeit zum Ordnernamen (mind. 3 Zeichen, .BAT vor .EXE) > leer.
std::string pickDosCommand(const std::vector<DirEntry>& entries, const std::string& folderName,
                           std::optional<std::string> autoexecTxt);
