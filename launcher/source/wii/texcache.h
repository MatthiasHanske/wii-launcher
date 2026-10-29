#pragma once
// PNG-Texturen von SD/USB, lazy geladen, mit Byte-Budget und LRU-Verdrängung.
//
// Zeiger aus get() bleiben gültig, bis der Eintrag verdrängt wird. Verdrängt werden nur
// Texturen, die im laufenden Bild (seit dem letzten evictUnused) nicht per get() geholt
// wurden: Die GPU zeichnet asynchron bis GRRLIB_Render, eine in diesem Bild schon
// gezeichnete Textur darf vorher nicht freigegeben werden. Ablauf pro Bild siehe gfx.h:
//     gfx::beginFrame(); ... cache.get(...) / gfx::image(...) ...; gfx::endFrame(); cache.endFrame();
// Liegt das Budget nur durch Texturen des laufenden Bilds darüber, wird es vorübergehend
// überschritten und nach dem Bild wieder eingehalten.
#include <cstddef>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <unordered_set>

#include "core/lru.h"
#include "wii/gfx.h"

class TexCache {
public:
    explicit TexCache(size_t maxBytes);
    ~TexCache();
    TexCache(const TexCache&) = delete;
    TexCache& operator=(const TexCache&) = delete;

    // nullptr, wenn die Datei fehlt, keine gültige PNG ist oder nicht dekodiert werden
    // kann. Fehlschläge werden gemerkt (kein erneuter Kartenzugriff in jedem Bild).
    const Texture* get(const std::string& path);

    // Nach endFrame() aufrufen: verdrängt die ältesten Einträge, bis das Budget passt,
    // und beginnt ein neues Bild.
    void evictUnused();
    // Dasselbe unter dem Namen des Bildprotokolls (gfx.h): nach gfx::endFrame() aufrufen.
    void endFrame() { evictUnused(); }

    // Gemerkte Fehlschläge vergessen (z. B. nachdem Cover nachgeladen wurden); geladene
    // Texturen bleiben.
    void forgetFailures() { failed_.clear(); }

    // Alles freigeben, auch gemerkte Fehlschläge (z. B. nach Austausch der Karte).
    void clear();

    size_t usedBytes() const { return lru_.totalBytes(); }
    size_t count() const { return lru_.size(); }

private:
    struct Entry {
        Texture tex;
        uint32_t frame = 0;  // Bildnummer der letzten Benutzung
    };

    // Ältesten Eintrag freigeben, sofern er nicht im laufenden Bild benutzt wurde.
    bool evictOldestUnused();
    void release(const std::string& path);

    size_t maxBytes_;
    uint32_t frame_ = 1;
    LruPolicy<std::string> lru_;
    std::unordered_map<std::string, Entry> entries_;
    std::unordered_set<std::string> failed_;
};
