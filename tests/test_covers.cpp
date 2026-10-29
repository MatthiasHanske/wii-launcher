#include "test.h"

#include "core/covers.h"

static Game mk(System s, const char* id, const char* dir, const char* file) {
    Game g;
    g.system = s;
    g.gameId = id;
    g.dir = dir;
    g.file = file;
    return g;
}

TEST(cover_path_wii_and_gc_use_game_id) {
    CHECK_EQ(coverPath(mk(System::Wii, "RMCP01", "usb:/wbfs/Mario Kart Wii [RMCP01]", "RMCP01.wbfs")),
             "sd:/config/images/2D/RMCP01.png");
    CHECK_EQ(coverPath(mk(System::GameCube, "GM4P01", "usb:/games/Mario Kart [GM4P01]", "")),
             "sd:/config/images/2D/GM4P01.png");
    CHECK_EQ(coverPath(mk(System::Wii, "", "usb:/wbfs", "x.wbfs")), "");
}

TEST(cover_path_emulators_use_file_stem) {
    CHECK_EQ(coverPath(mk(System::SNES, "", "usb:/snes9xgx/roms", "Super Mario World (Europe).sfc")),
             "sd:/launcher/covers/snes/Super Mario World (Europe).png");
    CHECK_EQ(coverPath(mk(System::N64, "", "usb:/not64/roms", "Super Mario 64 (E) [!].z64")),
             "sd:/launcher/covers/n64/Super Mario 64 (E) [!].png");
    CHECK_EQ(coverPath(mk(System::Sega, "", "usb:/genplus/roms/md", "Sonic.v1.1.md")),
             "sd:/launcher/covers/sega/Sonic.v1.1.png");
    CHECK_EQ(coverPath(mk(System::NES, "", "usb:/fceugx/roms", "Metroid")),
             "sd:/launcher/covers/nes/Metroid.png");
}

TEST(cover_path_adventure_uses_folder_name) {
    CHECK_EQ(coverPath(mk(System::Adventure, "", "usb:/scummvm/Monkey Island 2", "")),
             "sd:/launcher/covers/dos/Monkey Island 2.png");
    CHECK_EQ(coverPath(mk(System::Adventure, "", "usb:/dosbox/SimCity 2000/", "")),
             "sd:/launcher/covers/dos/SimCity 2000.png");
}
