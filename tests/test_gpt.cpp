#include "test.h"

#include <cstdint>
#include <optional>
#include <vector>

#include "core/gpt.h"

namespace {

struct Disk {
    std::vector<uint8_t> s0 = std::vector<uint8_t>(512, 0);
    std::vector<uint8_t> s1 = std::vector<uint8_t>(512, 0);
    std::vector<uint8_t> entries = std::vector<uint8_t>(16384, 0);
};

void putLe32(uint8_t* p, uint32_t v) {
    for (int i = 0; i < 4; ++i) p[i] = static_cast<uint8_t>(v >> (8 * i));
}

void putLe64(uint8_t* p, uint64_t v) {
    for (int i = 0; i < 8; ++i) p[i] = static_cast<uint8_t>(v >> (8 * i));
}

void setMbrEntry(Disk& d, int idx, uint8_t type, uint32_t start) {
    uint8_t* e = d.s0.data() + 446 + idx * 16;
    e[4] = type;
    putLe32(e + 8, start);
    d.s0[510] = 0x55;
    d.s0[511] = 0xAA;
}

void setGptHeader(Disk& d, const char* sig = "EFI PART") {
    for (int i = 0; i < 8; ++i) d.s1[i] = static_cast<uint8_t>(sig[i]);
    putLe64(d.s1.data() + 72, 2);
    putLe32(d.s1.data() + 80, 128);
    putLe32(d.s1.data() + 84, 128);
}

void setGptEntry(Disk& d, int idx, uint64_t firstLba) {
    uint8_t* e = d.entries.data() + idx * 128;
    e[0] = 0xA2;  // Typ-GUID != 0
    e[1] = 0xA0;
    putLe64(e + 32, firstLba);
}

std::optional<uint64_t> find(const Disk& d) {
    return findFirstPartitionLba(d.s0.data(), d.s1.data(), d.entries.data());
}

}  // namespace

TEST(gpt_mbr_fat32_lba) {
    Disk d;
    setMbrEntry(d, 0, 0x0C, 8192);
    auto r = find(d);
    CHECK(r.has_value());
    CHECK_EQ(r.value_or(0), (uint64_t)8192);
}

TEST(gpt_protective_mbr_uses_gpt_entry) {
    Disk d;
    setMbrEntry(d, 0, 0xEE, 1);
    setGptHeader(d);
    setGptEntry(d, 0, 2048);
    auto r = find(d);
    CHECK(r.has_value());
    CHECK_EQ(r.value_or(0), (uint64_t)2048);
}

TEST(gpt_empty_disk_is_nullopt) {
    Disk d;
    CHECK(!find(d).has_value());
}

TEST(gpt_bad_signature_is_nullopt) {
    Disk d;
    setMbrEntry(d, 0, 0xEE, 1);
    setGptHeader(d, "EFI PARX");
    setGptEntry(d, 0, 2048);
    CHECK(!find(d).has_value());
}

TEST(gpt_mbr_entry_with_start_zero_is_skipped) {
    Disk d;
    setMbrEntry(d, 0, 0x0C, 0);
    setMbrEntry(d, 1, 0x0B, 63);
    auto r = find(d);
    CHECK(r.has_value());
    CHECK_EQ(r.value_or(0), (uint64_t)63);
}

TEST(gpt_skips_unused_gpt_entries) {
    Disk d;
    setMbrEntry(d, 0, 0xEE, 1);
    setGptHeader(d);
    setGptEntry(d, 2, 4096);  // Einträge 0 und 1 bleiben leer (GUID 0)
    auto r = find(d);
    CHECK(r.has_value());
    CHECK_EQ(r.value_or(0), (uint64_t)4096);
}

namespace {
// Typ-GUIDs im On-Disk-Format (gemischtes Endian).
const uint8_t kEspGuid[16] = {0x28, 0x73, 0x2A, 0xC1, 0x1F, 0xF8, 0xD2, 0x11,
                              0xBA, 0x4B, 0x00, 0xA0, 0xC9, 0x3E, 0xC9, 0x3B};
const uint8_t kMsrGuid[16] = {0x16, 0xE3, 0xC9, 0xE3, 0x5C, 0x0B, 0xB8, 0x4D,
                              0x81, 0x7D, 0xF9, 0x2D, 0xF0, 0x02, 0x15, 0xAE};

void setGptType(Disk& d, int idx, const uint8_t* guid) {
    for (int i = 0; i < 16; ++i) d.entries[idx * 128 + i] = guid[i];
}
}  // namespace

TEST(gpt_skips_efi_system_partition) {
    Disk d;  // typisches macOS-Layout: ESP ab 40, Daten ab 409640
    setMbrEntry(d, 0, 0xEE, 1);
    setGptHeader(d);
    setGptEntry(d, 0, 40);
    setGptType(d, 0, kEspGuid);
    setGptEntry(d, 1, 409640);
    auto r = find(d);
    CHECK_EQ(r.value_or(0), (uint64_t)409640);
}

TEST(gpt_skips_microsoft_reserved_partition) {
    Disk d;  // typisches Windows-Layout: MSR ab 34, Daten ab 32768
    setMbrEntry(d, 0, 0xEE, 1);
    setGptHeader(d);
    setGptEntry(d, 0, 34);
    setGptType(d, 0, kMsrGuid);
    setGptEntry(d, 1, 32768);
    auto r = find(d);
    CHECK_EQ(r.value_or(0), (uint64_t)32768);
}

TEST(gpt_hybrid_mbr_fat_before_protective_wins) {
    Disk d;
    setMbrEntry(d, 0, 0x0C, 8192);
    setMbrEntry(d, 1, 0xEE, 1);
    setGptHeader(d);
    setGptEntry(d, 0, 2048);
    auto r = find(d);
    CHECK(r.has_value());
    CHECK_EQ(r.value_or(0), (uint64_t)8192);
}

TEST(gpt_entry_size_256_uses_header_stride) {
    Disk d;
    setMbrEntry(d, 0, 0xEE, 1);
    setGptHeader(d);
    putLe32(d.s1.data() + 84, 256);
    // Index 1 bei Schrittweite 256 liegt bei Byte 256 (= Index 2 bei 128).
    uint8_t* e = d.entries.data() + 256;
    e[0] = 0xA2;
    putLe64(e + 32, 10000);
    // Köder: bei falscher Schrittweite 128 würde Index 1 (Byte 128) gelesen.
    setGptEntry(d, 1, 777);
    auto r = find(d);
    CHECK(r.has_value());
    CHECK_EQ(r.value_or(0), (uint64_t)10000);
}
