#include "test.h"

#include <string>

#include "core/settings.h"

static bool sameSet(const SystemSet& a, const SystemSet& b) {
    for (int i = 0; i < 7; ++i)
        if (a.enabled[i] != b.enabled[i]) return false;
    return true;
}

TEST(settings_empty_gives_defaults) {
    Settings s = parseSettings("");
    CHECK(sameSet(s.systems, SystemSet::defaults()));
    CHECK_EQ(s.extraRoot, "");
}

TEST(settings_serialize_format) {
    Settings s;
    s.systems = SystemSet::defaults();
    CHECK_EQ(serializeSettings(s), "systems=wii,gc,n64\nextra_root=\n");
    s.systems.at(System::Adventure) = true;
    s.extraRoot = "sd:/launcher/mock";
    CHECK_EQ(serializeSettings(s), "systems=wii,gc,n64,dos\nextra_root=sd:/launcher/mock\n");
}

TEST(settings_roundtrip) {
    Settings s;
    s.systems = SystemSet();
    s.systems.at(System::SNES) = true;
    s.systems.at(System::Sega) = true;
    s.extraRoot = "sd:/x";
    Settings b = parseSettings(serializeSettings(s));
    CHECK(sameSet(b.systems, s.systems));
    CHECK_EQ(b.extraRoot, "sd:/x");
}

TEST(settings_empty_systems_means_none) {
    Settings s = parseSettings("systems=\nextra_root=\n");
    CHECK(sameSet(s.systems, SystemSet()));
    Settings b = parseSettings(serializeSettings(s));
    CHECK(sameSet(b.systems, SystemSet()));
}

TEST(settings_missing_systems_key_gives_defaults) {
    Settings s = parseSettings("extra_root=sd:/y\n");
    CHECK(sameSet(s.systems, SystemSet::defaults()));
    CHECK_EQ(s.extraRoot, "sd:/y");
}

TEST(settings_unknown_keys_and_junk_ignored) {
    Settings s = parseSettings("foo=bar\nmurks\n\n=x\nsystems=nes,bogus,wii\r\nextra_root=sd:/z\r\n");
    CHECK(s.systems.get(System::NES));
    CHECK(s.systems.get(System::Wii));
    CHECK(!s.systems.get(System::GameCube));
    CHECK_EQ(s.extraRoot, "sd:/z");
}
