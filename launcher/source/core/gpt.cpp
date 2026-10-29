#include "core/gpt.h"

#include <cstring>

namespace {

constexpr size_t kSectorSize = 512;
constexpr size_t kEntriesBufSize = 16384;  // 32 Sektoren ab LBA 2
constexpr uint64_t kEntriesBufLba = 2;

uint32_t le32(const uint8_t* p) {
    return static_cast<uint32_t>(p[0]) | (static_cast<uint32_t>(p[1]) << 8) |
           (static_cast<uint32_t>(p[2]) << 16) | (static_cast<uint32_t>(p[3]) << 24);
}

uint64_t le64(const uint8_t* p) {
    return static_cast<uint64_t>(le32(p)) | (static_cast<uint64_t>(le32(p + 4)) << 32);
}

// Partitionen ohne Nutzdaten, die vor der eigentlichen Datenpartition liegen können
// (macOS: EFI-Systempartition, Windows: Microsoft Reserved). Typ-GUID im On-Disk-Format.
const uint8_t kSkipTypes[][16] = {
    {0x28, 0x73, 0x2A, 0xC1, 0x1F, 0xF8, 0xD2, 0x11, 0xBA, 0x4B, 0x00, 0xA0, 0xC9, 0x3E, 0xC9, 0x3B},
    {0x16, 0xE3, 0xC9, 0xE3, 0x5C, 0x0B, 0xB8, 0x4D, 0x81, 0x7D, 0xF9, 0x2D, 0xF0, 0x02, 0x15, 0xAE},
};

bool isSkippedType(const uint8_t* guid) {
    for (const auto& t : kSkipTypes)
        if (std::memcmp(guid, t, 16) == 0) return true;
    return false;
}

bool isFatType(uint8_t t) {
    return t == 0x0B || t == 0x0C || t == 0x06 || t == 0x0E || t == 0x07;
}

std::optional<uint64_t> fromGpt(const uint8_t* sector1, const uint8_t* entries) {
    if (std::memcmp(sector1, "EFI PART", 8) != 0) return std::nullopt;

    uint64_t arrayLba = le64(sector1 + 72);
    uint32_t count = le32(sector1 + 80);
    uint32_t entrySize = le32(sector1 + 84);
    if (entrySize < 128 || arrayLba < kEntriesBufLba) return std::nullopt;

    uint64_t base = (arrayLba - kEntriesBufLba) * kSectorSize;
    if (base >= kEntriesBufSize) return std::nullopt;

    for (uint32_t i = 0; i < count; ++i) {
        uint64_t off = base + static_cast<uint64_t>(i) * entrySize;
        if (off + 128 > kEntriesBufSize) break;
        const uint8_t* e = entries + off;
        bool used = false;
        for (int b = 0; b < 16; ++b) used = used || e[b] != 0;
        if (!used || isSkippedType(e)) continue;
        uint64_t first = le64(e + 32);
        if (first != 0) return first;
    }
    return std::nullopt;
}

}  // namespace

std::optional<uint64_t> findFirstPartitionLba(const uint8_t* sector0, const uint8_t* sector1,
                                              const uint8_t* entries) {
    for (int i = 0; i < 4; ++i) {
        const uint8_t* e = sector0 + 446 + i * 16;
        if (e[4] == 0xEE) break;  // Schutz-MBR: GPT ist maßgeblich
        uint32_t start = le32(e + 8);
        if (isFatType(e[4]) && start != 0) return start;
    }
    return fromGpt(sector1, entries);
}
