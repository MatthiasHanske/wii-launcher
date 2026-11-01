#pragma once
#include <cstdint>
#include <string>
#include <vector>

#include "core/library.h"
#include "core/model.h"

constexpr int kRecentMax = 20;

struct RecentEntry {
    int64_t timestamp = 0;
    System system = System::Wii;
    std::string dir;
    std::string file;
};

// Zeilenformat: <timestamp>\t<systemKey>\t<dir>\t<file>; kaputte Zeilen werden uebersprungen.
std::vector<RecentEntry> parseRecent(const std::string& text);
std::string serializeRecent(const std::vector<RecentEntry>& entries);
// Duplikat (gleiches dir+file) entfernen, vorn einfuegen, auf kRecentMax kuerzen.
void pushRecent(std::vector<RecentEntry>& entries, const RecentEntry& e);
// Treffer ueber dir+file, Eintraege ohne Treffer entfallen, Reihenfolge bleibt.
std::vector<const Game*> resolveRecent(const std::vector<RecentEntry>& entries,
                                       const std::vector<Game>& all);
// Fuer die Startposition des Fächers: resolveRecent ueber library.games(), nur Spiele aktivierter Systeme.
std::vector<const Game*> recentForLibrary(const std::vector<RecentEntry>& entries, const Library& library);
