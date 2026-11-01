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
    // true: der Launcher wartet nach dem IOS-Reload, bis die USB-Platte lesbar ist, bevor er
    // springt. Für Emulatoren und ScummVM/DOSBox, die USB nur einmal beim Start einhängen.
    // false für Wii/GameCube: USB Loader GX wartet selbst auf die Platte.
    bool waitForUsb = false;
};

LaunchPlan makeLaunchPlan(const Game& game, IFileSystem& fs);

// true, wenn makeLaunchPlan Dateien aus game.dir liest (DOSBox: Ordnerinhalt, autoexec.txt).
// Der Launcher hängt die USB-Platte nach dem Scan aus und muss sie dafür kurz einhängen.
// Alle anderen Pläne bestehen nur aus Pfaden oder lesen von der SD.
bool launchReadsGameDir(const Game& game);

// Startbefehl fuer DOSBox: erste Zeile von autoexec.txt > einzige .BAT/.EXE > beste
// Namensaehnlichkeit zum Ordnernamen (mind. 3 Zeichen, .BAT vor .EXE) > leer.
std::string pickDosCommand(const std::vector<DirEntry>& entries, const std::string& folderName,
                           std::optional<std::string> autoexecTxt);
