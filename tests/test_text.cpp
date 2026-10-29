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

TEST(ellipsize_binary_search_few_measure_calls) {
    std::string longText(200, 'x');
    int calls = 0;
    auto counting = [&calls](const std::string& s) { ++calls; return tenPx(s); };
    std::string r = ellipsize(longText, 505.0f, counting);
    CHECK_EQ(r, std::string(49, 'x') + "\xE2\x80\xA6");
    // ganz + "…" + log2(200) ~ 8 Schritte, linear wären es 150
    CHECK(calls <= 12);
}

TEST(ellipsize_binary_search_matches_linear_with_spaces) {
    // monotone Suche muss trotz entfernter Leerzeichen das längste passende Präfix finden
    const char* text = "a b c d e f g h";
    for (int w = 0; w <= 160; w += 5) {
        float maxW = static_cast<float>(w);
        std::string want;
        std::string s(text);
        if (tenPx(s) <= maxW) {
            want = s;
        } else if (tenPx("\xE2\x80\xA6") <= maxW) {
            want = "\xE2\x80\xA6";
            for (size_t n = s.size(); n > 0; --n) {
                std::string head = s.substr(0, n);
                while (!head.empty() && head.back() == ' ') head.pop_back();
                if (tenPx(head + "\xE2\x80\xA6") <= maxW) { want = head + "\xE2\x80\xA6"; break; }
            }
        }
        CHECK_EQ(ellipsize(s, maxW, tenPx), want);
    }
}

TEST(ellipsize_memo_hits_without_measuring) {
    EllipsizeMemo memo;
    int calls = 0;
    auto counting = [&calls](const std::string& s) { ++calls; return tenPx(s); };
    CHECK_EQ(memo.get("Zelda II", 50.0f, 14, counting), "Zeld\xE2\x80\xA6");
    CHECK(calls > 0);
    calls = 0;
    CHECK_EQ(memo.get("Zelda II", 50.0f, 14, counting), "Zeld\xE2\x80\xA6");
    CHECK_EQ(calls, 0);
    CHECK_EQ(memo.size(), (size_t)1);
    // andere Größe oder Breite ist ein eigener Eintrag
    memo.get("Zelda II", 50.0f, 16, counting);
    memo.get("Zelda II", 60.0f, 14, counting);
    CHECK_EQ(memo.size(), (size_t)3);
}

TEST(ellipsize_memo_clears_when_full) {
    EllipsizeMemo memo(512);
    for (int i = 0; i < 512; ++i) memo.get("t" + std::to_string(i), 100.0f, 14, tenPx);
    CHECK_EQ(memo.size(), (size_t)512);
    memo.get("noch einer", 100.0f, 14, tenPx);
    CHECK_EQ(memo.size(), (size_t)1);
}
