#pragma once
// Plattformfreie LRU-Buchführung nach Bytes. Hält nur Keys und Größen; das eigentliche
// Freigeben macht der Aufrufer (z. B. TexCache): solange totalBytes() > Budget, evict().
#include <cstddef>
#include <list>
#include <unordered_map>
#include <utility>

template <typename Key>
class LruPolicy {
public:
    // Neu einfügen oder (bei bekanntem Key) Größe ersetzen; der Key gilt danach als jüngster.
    void insert(const Key& key, size_t bytes) {
        auto it = index_.find(key);
        if (it != index_.end()) {
            total_ -= it->second->second;
            order_.erase(it->second);
        }
        order_.emplace_front(key, bytes);
        index_[key] = order_.begin();
        total_ += bytes;
    }

    // Als zuletzt benutzt markieren; unbekannte Keys werden ignoriert.
    void touch(const Key& key) {
        auto it = index_.find(key);
        if (it == index_.end()) return;
        order_.splice(order_.begin(), order_, it->second);
    }

    // Ältesten Eintrag entfernen und seinen Key liefern; leer -> Key{}.
    Key evict() {
        if (order_.empty()) return Key{};
        Key key = std::move(order_.back().first);
        total_ -= order_.back().second;
        order_.pop_back();
        index_.erase(key);
        return key;
    }

    bool erase(const Key& key) {
        auto it = index_.find(key);
        if (it == index_.end()) return false;
        total_ -= it->second->second;
        order_.erase(it->second);
        index_.erase(it);
        return true;
    }

    // Ältester Eintrag ohne Entfernen; leer -> nullptr. Zeiger gilt bis zur nächsten Änderung.
    const Key* oldest() const { return order_.empty() ? nullptr : &order_.back().first; }

    bool contains(const Key& key) const { return index_.count(key) != 0; }
    size_t totalBytes() const { return total_; }
    size_t size() const { return order_.size(); }

private:
    using Entry = std::pair<Key, size_t>;
    std::list<Entry> order_;  // vorne = jüngster
    std::unordered_map<Key, typename std::list<Entry>::iterator> index_;
    size_t total_ = 0;
};
