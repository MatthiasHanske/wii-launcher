#include "core/library.h"

#include <algorithm>
#include <utility>

#include "core/titles.h"

Library::Library(std::vector<Game> games) : games_(std::move(games)) {
    std::stable_sort(games_.begin(), games_.end(),
                     [](const Game& a, const Game& b) { return a.sortKey < b.sortKey; });
    searchKeys_.reserve(games_.size());
    for (const Game& g : games_) searchKeys_.push_back(searchKey(g.title));
    rebuild();
}

void Library::setEnabled(const SystemSet& s) {
    enabled_ = s;
    if (tab_ && !enabled_.get(*tab_)) tab_.reset();
    rebuild();
}

void Library::setTab(std::optional<System> tab) {
    tab_ = tab;
    rebuild();
}

std::vector<System> Library::tabs() const {
    std::vector<System> out;
    for (int i = 0; i < 7; ++i)
        if (enabled_.enabled[i]) out.push_back(static_cast<System>(i));
    return out;
}

void Library::rebuild() {
    std::fill(firstIdx_, firstIdx_ + 256, -1);
    visible_.clear();
    visible_.reserve(games_.size());
    bool single = tab_.has_value() && enabled_.get(*tab_);
    for (const Game& g : games_) {
        if (!enabled_.get(g.system)) continue;
        if (single && g.system != *tab_) continue;
        int& f = firstIdx_[static_cast<unsigned char>(g.letter)];
        if (f < 0) f = static_cast<int>(visible_.size());
        visible_.push_back(&g);
    }
}

std::vector<char> Library::letters() const {
    std::vector<char> out;
    if (firstIdx_[static_cast<unsigned char>('#')] >= 0) out.push_back('#');
    for (int c = 'A'; c <= 'Z'; ++c)
        if (firstIdx_[c] >= 0) out.push_back(static_cast<char>(c));
    return out;
}

int Library::firstIndexForLetter(char c) const {
    return firstIdx_[static_cast<unsigned char>(c)];
}

std::vector<const Game*> Library::search(const std::string& query, size_t limit) const {
    std::vector<const Game*> out;
    if (limit == 0) return out;
    std::string q = searchKey(query);
    if (q.empty()) return out;
    for (size_t i = 0; i < games_.size(); ++i) {
        const Game& g = games_[i];
        if (!enabled_.get(g.system)) continue;
        if (searchKeys_[i].find(q) != std::string::npos) {
            out.push_back(&g);
            if (out.size() >= limit) break;
        }
    }
    return out;
}

int Library::pageCount() const {
    return static_cast<int>((visible_.size() + kPageSize - 1) / kPageSize);
}

std::vector<const Game*> Library::page(int index) const {
    if (index < 0 || index >= pageCount()) return {};
    size_t b = static_cast<size_t>(index) * kPageSize;
    size_t e = std::min(visible_.size(), b + kPageSize);
    return std::vector<const Game*>(visible_.begin() + b, visible_.begin() + e);
}
