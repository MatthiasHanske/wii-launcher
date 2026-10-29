#include "test.h"

#include <string>

#include "core/titles.h"

TEST(cleanTitle_removes_region_and_id) {
    CHECK_EQ(cleanTitle("Mario Kart Wii (Europe) [RMCP01]"), "Mario Kart Wii");
    CHECK_EQ(cleanTitle("New Super Mario Bros. Wii (Europe) (Rev 2) [SMNP01]"), "New Super Mario Bros. Wii");
}

TEST(cleanTitle_moves_article_inside_subtitle) {
    CHECK_EQ(cleanTitle("drei Fragezeichen, Die - Das verfluchte Schloss (Germany) (Rev 1) [R4DDUS]"),
             "Die drei Fragezeichen - Das verfluchte Schloss");
}

TEST(cleanTitle_dump_tags) {
    CHECK_EQ(cleanTitle("Tony Hawk's Pro Skater 2 (E) [!]"), "Tony Hawk's Pro Skater 2");
    CHECK_EQ(cleanTitle("Sonic and Knuckles & Sonic 3 (W) [!]"), "Sonic and Knuckles & Sonic 3");
}

TEST(cleanTitle_underscores) {
    CHECK_EQ(cleanTitle("Day_of_the_Tentacle"), "Day of the Tentacle");
}

TEST(cleanTitle_many_tags) {
    CHECK_EQ(cleanTitle("Legend of Zelda, The (World) (Digital) (REV-G) (Aftermarket) (Pirate)"), "The Legend of Zelda");
}

TEST(cleanTitle_language_and_version_tags) {
    CHECK_EQ(cleanTitle("Game (Europe, Australia) (En,Fr,De,Es,It) (V1.1) (M5)"), "Game");
    CHECK_EQ(cleanTitle("Game (USA) (UE) (JUE) (PAL)"), "Game");
}

TEST(cleanTitle_keeps_unknown_parens) {
    CHECK_EQ(cleanTitle("Pac-Man (Arcade) (Europe)"), "Pac-Man (Arcade)");
}

TEST(cleanTitle_trailing_dash) {
    CHECK_EQ(cleanTitle("Game - (Europe)"), "Game");
}

TEST(sortKey_cases) {
    CHECK_EQ(sortKey("The Sims 3"), "sims3");
    CHECK_EQ(sortKey("Die drei ???"), "drei");
    CHECK_EQ(sortKey("Benjamin Bl\xC3\xBC" "mchen"), "benjaminblumchen");
    CHECK_EQ(sortKey("Der Gro\xC3\x9F" "e \xC3\x84rger"), "grossearger");
}

TEST(indexLetter_cases) {
    CHECK_EQ(indexLetter("The Sims 3"), 'S');
    CHECK_EQ(indexLetter("101-in-1 Party Megamix"), '#');
    CHECK_EQ(indexLetter("???"), '#');
}

TEST(extractGameId_cases) {
    CHECK(extractGameId("Mario Kart Wii (Europe) [RMCP01]").has_value());
    CHECK_EQ(extractGameId("Mario Kart Wii (Europe) [RMCP01]").value_or(""), "RMCP01");
    CHECK(!extractGameId("Super Mario World (Europe).sfc").has_value());
    CHECK(!extractGameId("Game [!]").has_value());
    CHECK(!extractGameId("Game [rmcp01]").has_value());
}

TEST(cleanTitle_single_two_letter_kept) {
    CHECK_EQ(cleanTitle("Resident Evil (HD)"), "Resident Evil (HD)");
    CHECK_EQ(cleanTitle("Mario Kart (DX) (Europe) (En,De)"), "Mario Kart (DX)");
}

TEST(cleanTitle_umlauts_survive) {
    CHECK_EQ(cleanTitle("Benjamin Bl\xC3\xBCmchen - T\xC3\xB6r\xC3\xB6\xC3\xB6\xC3\xB6! im Zoo (Germany) [SUZD7K]"),
             "Benjamin Bl\xC3\xBCmchen - T\xC3\xB6r\xC3\xB6\xC3\xB6\xC3\xB6! im Zoo");
    CHECK_EQ(cleanTitle("Die drei ???"), "Die drei ???");
}

TEST(cleanTitle_article_only_as_suffix) {
    CHECK_EQ(cleanTitle("Ja, Das ist Mario"), "Ja, Das ist Mario");
}
