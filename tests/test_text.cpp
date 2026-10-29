#include "test.h"

#include <string>
#include <vector>

#include "core/text.h"

TEST(utf8_decode_ascii_and_multibyte) {
    // "Aä…😀" = 41 | C3 A4 | E2 80 A6 | F0 9F 98 80
    std::vector<uint32_t> cps = utf8Decode("A\xC3\xA4\xE2\x80\xA6\xF0\x9F\x98\x80");
    CHECK_EQ(cps.size(), (size_t)4);
    CHECK_EQ(cps[0], 0x41u);
    CHECK_EQ(cps[1], 0xE4u);
    CHECK_EQ(cps[2], 0x2026u);
    CHECK_EQ(cps[3], 0x1F600u);
}

TEST(utf8_decode_invalid_bytes_become_replacement) {
    // lose Folgebyte, abgeschnittene Sequenz am Ende, überlange Kodierung
    std::vector<uint32_t> cps = utf8Decode("a\x80" "b\xC0\xAF" "c\xE2\x80");
    std::vector<uint32_t> want = {'a', 0xFFFD, 'b', 0xFFFD, 0xFFFD, 'c', 0xFFFD};
    CHECK(cps == want);
}

TEST(utf8_decode_latin1_fallback_not_used) {
    // ein einzelnes 0xE4 (Latin-1 "ä") ist kein gültiges UTF-8
    std::vector<uint32_t> cps = utf8Decode("\xE4");
    CHECK_EQ(cps.size(), (size_t)1);
    CHECK_EQ(cps[0], 0xFFFDu);
}

// Messfunktion für Tests: jedes Zeichen (Codepoint) 10 px breit.
static float tenPx(const std::string& s) { return 10.0f * utf8Decode(s).size(); }

TEST(ellipsize_fits_unchanged) {
    CHECK_EQ(ellipsize("Mario", 50.0f, tenPx), "Mario");
    CHECK_EQ(ellipsize("", 0.0f, tenPx), "");
}

TEST(ellipsize_cuts_and_appends_ellipsis) {
    // 8 Zeichen, Platz für 5 -> 4 Zeichen + "…"
    CHECK_EQ(ellipsize("Zelda II", 50.0f, tenPx), "Zeld\xE2\x80\xA6");
}

TEST(ellipsize_drops_trailing_space_before_ellipsis) {
    CHECK_EQ(ellipsize("Mario Kart", 70.0f, tenPx), "Mario\xE2\x80\xA6");
}

TEST(ellipsize_respects_utf8_boundaries) {
    // "Pokémon Snap": é darf nicht zerschnitten werden
    CHECK_EQ(ellipsize("Pok\xC3\xA9mon Snap", 50.0f, tenPx), "Pok\xC3\xA9\xE2\x80\xA6");
}

TEST(ellipsize_too_narrow_for_anything) {
    CHECK_EQ(ellipsize("Mario", 5.0f, tenPx), "");
    CHECK_EQ(ellipsize("Mario", 10.0f, tenPx), "\xE2\x80\xA6");
}
