#include "wii/texcache.h"

#include <grrlib.h>

#include <cstdio>
#include <vector>

#include "core/png_check.h"

namespace {

// Obergrenze für eine einzelne Datei; Cover liegen bei 20..200 KB.
constexpr long kMaxFileBytes = 4 * 1024 * 1024;

bool readFile(const std::string& path, std::vector<uint8_t>& out) {
    FILE* f = std::fopen(path.c_str(), "rb");
    if (!f) return false;
    bool ok = false;
    if (std::fseek(f, 0, SEEK_END) == 0) {
        long size = std::ftell(f);
        if (size > 0 && size <= kMaxFileBytes && std::fseek(f, 0, SEEK_SET) == 0) {
            out.resize(static_cast<size_t>(size));
            ok = std::fread(out.data(), 1, out.size(), f) == out.size();
        }
    }
    std::fclose(f);
    return ok;
}

size_t textureBytes(uint32_t w, uint32_t h) { return size_t(w) * size_t(h) * 4; }  // RGBA8

}  // namespace

TexCache::TexCache(size_t maxBytes) : maxBytes_(maxBytes) {}

TexCache::~TexCache() { clear(); }

const Texture* TexCache::get(const std::string& path) {
    auto it = entries_.find(path);
    if (it != entries_.end()) {
        it->second.frame = frame_;
        lru_.touch(path);
        return &it->second.tex;
    }
    if (failed_.count(path)) return nullptr;

    std::vector<uint8_t> data;
    PngInfo info;
    if (!readFile(path, data) || !checkPng(data.data(), data.size(), info)) {
        failed_.insert(path);
        return nullptr;
    }

    // Vor dem Dekodieren Platz schaffen (nur mit Texturen aus früheren Bildern).
    size_t bytes = textureBytes(info.width, info.height);
    while (lru_.totalBytes() + bytes > maxBytes_ && evictOldestUnused()) {
    }

    GRRLIB_texImg* img = GRRLIB_LoadTexturePNG(data.data());
    // GRRLIB liefert auch bei Dekodierfehler ein Objekt, dann mit data == NULL.
    if (!img || !img->data || img->w == 0 || img->h == 0) {
        if (img) GRRLIB_FreeTexture(img);
        failed_.insert(path);
        return nullptr;
    }

    Entry& e = entries_[path];
    e.tex.img = img;
    e.tex.w = static_cast<int>(img->w);
    e.tex.h = static_cast<int>(img->h);
    e.frame = frame_;
    lru_.insert(path, textureBytes(img->w, img->h));
    return &e.tex;
}

bool TexCache::evictOldestUnused() {
    const std::string* oldest = lru_.oldest();
    if (!oldest) return false;
    auto it = entries_.find(*oldest);
    if (it != entries_.end() && it->second.frame == frame_) return false;
    release(lru_.evict());
    return true;
}

void TexCache::release(const std::string& path) {
    auto it = entries_.find(path);
    if (it == entries_.end()) return;
    GRRLIB_FreeTexture(it->second.tex.img);
    entries_.erase(it);
}

void TexCache::evictUnused() {
    ++frame_;  // ab jetzt gilt nichts mehr als "im laufenden Bild benutzt"
    while (lru_.totalBytes() > maxBytes_ && evictOldestUnused()) {
    }
}

void TexCache::clear() {
    while (lru_.size() > 0) release(lru_.evict());
    failed_.clear();
}
