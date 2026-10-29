#include "wii/texcache.h"

#include <grrlib.h>
#include <pngu.h>

#include <cstdio>
#include <cstdlib>
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

// RGBA8 mit der von PNGU aufgefüllten Größe (Vielfache von 4).
size_t textureBytes(uint32_t w, uint32_t h) { return size_t((w + 3) & ~3u) * size_t((h + 3) & ~3u) * 4; }

// Pixel (x, y) einer 4x4-gekachelten RGBA8-Textur: je Kachel 32 Byte AR, dann 32 Byte GB.
inline size_t tileOffset(uint32_t texW, uint32_t x, uint32_t y) {
    return (size_t(y / 4) * (texW / 4) + x / 4) * 64 + ((y % 4) * 4 + (x % 4)) * 2;
}

// Auffüllspalten/-zeilen mit der letzten echten Spalte/Zeile füllen, damit die bilineare
// Filterung am Bildrand keinen Müll (oder Durchsichtiges) einmischt.
void fillPadding(uint8_t* d, uint32_t w, uint32_t h, uint32_t texW, uint32_t texH) {
    if (w == 0 || h == 0) return;
    for (uint32_t y = 0; y < h; ++y)
        for (uint32_t x = w; x < texW; ++x) {
            size_t src = tileOffset(texW, w - 1, y), dst = tileOffset(texW, x, y);
            d[dst] = d[src]; d[dst + 1] = d[src + 1];
            d[dst + 32] = d[src + 32]; d[dst + 33] = d[src + 33];
        }
    for (uint32_t y = h; y < texH; ++y)
        for (uint32_t x = 0; x < texW; ++x) {
            size_t src = tileOffset(texW, x, h - 1), dst = tileOffset(texW, x, y);
            d[dst] = d[src]; d[dst + 1] = d[src + 1];
            d[dst + 32] = d[src + 32]; d[dst + 33] = d[src + 33];
        }
}

// Wie GRRLIB_LoadTexturePNG, aber ohne dessen memset: GRRLIB löscht die ganze Textur, wenn
// PNGU auf Vielfache von 4 auffüllen musste (z. B. 2D-Cover 160x225 -> 160x228), das Cover
// wäre dann komplett durchsichtig.
GRRLIB_texImg* decodePng(const uint8_t* data) {
    IMGCTX ctx = PNGU_SelectImageFromBuffer(data);
    if (!ctx) return nullptr;
    GRRLIB_texImg* img = nullptr;
    PNGUPROP prop;
    if (PNGU_GetImageProperties(ctx, &prop) == PNGU_OK) {
        int w = 0, h = 0;
        uint8_t* pixels = PNGU_DecodeTo4x4RGBA8(ctx, prop.imgWidth, prop.imgHeight, &w, &h);
        if (pixels && (w <= 0 || h <= 0 || (w & 3) || (h & 3))) {
            std::free(pixels);
            pixels = nullptr;
        }
        if (pixels) {
            fillPadding(pixels, prop.imgWidth, prop.imgHeight, static_cast<uint32_t>(w), static_cast<uint32_t>(h));
            img = static_cast<GRRLIB_texImg*>(std::calloc(1, sizeof(GRRLIB_texImg)));
            if (!img) {
                std::free(pixels);
            } else {
                img->data = pixels;
                img->w = static_cast<u32>(w);
                img->h = static_cast<u32>(h);
                img->format = GX_TF_RGBA8;
                GRRLIB_SetHandle(img, 0, 0);
                GRRLIB_FlushTex(img);
            }
        }
    }
    PNGU_ReleaseImageContext(ctx);
    return img;
}

}  // namespace

bool loadPngTexture(const uint8_t* data, size_t size, Texture& out) {
    out = Texture();
    PngInfo info;
    if (!data || !checkPng(data, size, info)) return false;
    GRRLIB_texImg* img = decodePng(data);
    if (!img) return false;
    out.img = img;
    out.w = static_cast<int>(info.width);  // echte Größe; PNGU füllt auf Vielfache von 4 auf
    out.h = static_cast<int>(info.height);
    out.texW = static_cast<int>(img->w);
    out.texH = static_cast<int>(img->h);
    return true;
}

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

    GRRLIB_texImg* img = decodePng(data.data());
    if (!img || !img->data || img->w == 0 || img->h == 0) {
        if (img) GRRLIB_FreeTexture(img);
        failed_.insert(path);
        return nullptr;
    }

    Entry& e = entries_[path];
    e.tex.img = img;
    e.tex.w = static_cast<int>(info.width);  // echte Größe; PNGU füllt auf Vielfache von 4 auf
    e.tex.h = static_cast<int>(info.height);
    e.tex.texW = static_cast<int>(img->w);
    e.tex.texH = static_cast<int>(img->h);
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
