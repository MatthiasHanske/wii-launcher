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
