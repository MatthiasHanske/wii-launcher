#pragma once
#include <cstddef>
#include <optional>
#include <string>
#include <vector>

#include "core/model.h"

struct SystemSet {
    bool enabled[7] = {false, false, false, false, false, false, false};

    static SystemSet defaults() {
        SystemSet s;
        s.at(System::Wii) = s.at(System::GameCube) = s.at(System::N64) = true;
        return s;
    }
    bool& at(System s) { return enabled[static_cast<int>(s)]; }
    bool get(System s) const { return enabled[static_cast<int>(s)]; }
};

class Library {
public:
    static constexpr int kPageSize = 10;

    explicit Library(std::vector<Game> games);
    Library(const Library&) = delete;  // visible_ zeigt in games_
    Library& operator=(const Library&) = delete;

    void setEnabled(const SystemSet& s);
    std::vector<System> tabs() const;                 // aktivierte Systeme, Enum-Reihenfolge
    void setTab(std::optional<System> tab);           // nullopt = Alle
    const std::vector<const Game*>& visible() const { return visible_; }
    std::vector<char> letters() const;                // '#' zuerst, dann A..Z
    int firstIndexForLetter(char c) const;            // -1 wenn keiner
    std::vector<const Game*> search(const std::string& query, size_t limit) const;
    int pageCount() const;
    std::vector<const Game*> page(int index) const;

private:
    void rebuild();

    std::vector<Game> games_;  // sortiert nach sortKey, wird nie umallokiert
    SystemSet enabled_ = SystemSet::defaults();
    std::optional<System> tab_;
    std::vector<const Game*> visible_;
    int firstIdx_[256];
};
