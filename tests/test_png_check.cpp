#include "test.h"

#include <cstdint>
#include <string>
#include <vector>

#include "core/png_check.h"

namespace {

void put32(std::vector<uint8_t>& v, uint32_t x) {
    v.push_back(x >> 24); v.push_back(x >> 16); v.push_back(x >> 8); v.push_back(x);
}

void chunk(std::vector<uint8_t>& v, const char* type, const std::vector<uint8_t>& data) {
    put32(v, (uint32_t)data.size());
    v.insert(v.end(), type, type + 4);
    v.insert(v.end(), data.begin(), data.end());
    put32(v, 0);  // CRC wird nicht geprüft (macht libpng)
}

std::vector<uint8_t> png(uint32_t w, uint32_t h, bool withIend = true) {
    std::vector<uint8_t> v = {0x89, 'P', 'N', 'G', '\r', '\n', 0x1A, '\n'};
    std::vector<uint8_t> ihdr;
    put32(ihdr, w); put32(ihdr, h);
    ihdr.insert(ihdr.end(), {8, 6, 0, 0, 0});
    chunk(v, "IHDR", ihdr);
    chunk(v, "IDAT", std::vector<uint8_t>(20, 0));
    if (withIend) chunk(v, "IEND", {});
    return v;
}

}  // namespace

TEST(png_check_accepts_wellformed) {
    std::vector<uint8_t> v = png(160, 224);
    PngInfo info;
    CHECK(checkPng(v.data(), v.size(), info));
    CHECK_EQ(info.width, 160u);
    CHECK_EQ(info.height, 224u);
}

TEST(png_check_rejects_short_and_bad_signature) {
    PngInfo info;
    std::vector<uint8_t> ten(10, 0x41);
    CHECK(!checkPng(ten.data(), ten.size(), info));
    std::vector<uint8_t> v = png(16, 16);
    v[1] = 'X';
    CHECK(!checkPng(v.data(), v.size(), info));
    CHECK(!checkPng(nullptr, 0, info));
}

TEST(png_check_rejects_truncated) {
    std::vector<uint8_t> v = png(16, 16);
    PngInfo info;
    for (size_t n = 8; n < v.size(); ++n) CHECK(!checkPng(v.data(), n, info));
}

TEST(png_check_requires_iend) {
    std::vector<uint8_t> v = png(16, 16, false);
    PngInfo info;
    CHECK(!checkPng(v.data(), v.size(), info));
}

TEST(png_check_requires_ihdr_first) {
    std::vector<uint8_t> v = png(16, 16);
    v[12] = 'X';  // Typ des ersten Chunks
    PngInfo info;
    CHECK(!checkPng(v.data(), v.size(), info));
}

TEST(png_check_rejects_huge_chunk_length) {
    std::vector<uint8_t> v = png(16, 16);
    // Länge des IDAT-Chunks (direkt nach IHDR: 8 + 25) auf 0xFFFFFFF0 setzen
    v[33] = 0xFF; v[34] = 0xFF; v[35] = 0xFF; v[36] = 0xF0;
    PngInfo info;
    CHECK(!checkPng(v.data(), v.size(), info));
}

TEST(png_check_rejects_bad_dimensions) {
    PngInfo info;
    std::vector<uint8_t> zero = png(0, 16);
    CHECK(!checkPng(zero.data(), zero.size(), info));
    std::vector<uint8_t> big = png(1025, 16);  // GX-Texturen höchstens 1024
    CHECK(!checkPng(big.data(), big.size(), info));
    std::vector<uint8_t> max = png(1024, 1024);
    CHECK(checkPng(max.data(), max.size(), info));
}
