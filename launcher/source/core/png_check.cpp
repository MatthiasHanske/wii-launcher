#include "core/png_check.h"

#include <cstring>

namespace {

uint32_t be32(const uint8_t* p) {
    return (uint32_t(p[0]) << 24) | (uint32_t(p[1]) << 16) | (uint32_t(p[2]) << 8) | uint32_t(p[3]);
}

const uint8_t kSignature[8] = {0x89, 'P', 'N', 'G', '\r', '\n', 0x1A, '\n'};

}  // namespace

bool checkPng(const uint8_t* data, size_t size, PngInfo& info) {
    if (data == nullptr || size < 8 || std::memcmp(data, kSignature, 8) != 0) return false;

    PngInfo found;
    size_t pos = 8;
    bool first = true;
    while (true) {
        if (size - pos < 12) return false;  // Länge + Typ + CRC passen nicht mehr
        uint32_t len = be32(data + pos);
        const uint8_t* type = data + pos + 4;
        if (len > size - pos - 12) return false;  // Chunk ragt über das Ende
        const uint8_t* body = data + pos + 8;

        if (first) {
            if (std::memcmp(type, "IHDR", 4) != 0 || len != 13) return false;
            found.width = be32(body);
            found.height = be32(body + 4);
            if (found.width == 0 || found.height == 0 || found.width > kPngMaxSide ||
                found.height > kPngMaxSide)
                return false;
            first = false;
        }
        pos += 12 + size_t(len);
        if (std::memcmp(type, "IEND", 4) == 0) {
            info = found;
            return true;
        }
    }
}
