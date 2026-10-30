#include "test.h"

#include <string>
#include <vector>

#include "core/launch.h"
#include "fake_fs.h"

typedef std::vector<std::string> Args;

static Game mkGame(System s, const char* dir, const char* file = "", const char* id = "", const char* sub = "") {
    Game g;
    g.system = s;
    g.dir = dir;
    g.file = file;
    g.gameId = id;
    g.subdir = sub;
    return g;
}

static size_t countOf(const std::string& hay, const std::string& needle) {
    size_t n = 0;
    for (size_t p = hay.find(needle); p != std::string::npos; p = hay.find(needle, p + 1)) ++n;
    return n;
}

// Erste zu schreibende Datei, leer wenn keine (kein Absturz bei rotem Test).
static std::pair<std::string, std::string> first(const LaunchPlan& p) {
    return p.filesToWrite.empty() ? std::pair<std::string, std::string>() : p.filesToWrite[0];
}

static std::vector<DirEntry> simcityEntries() {
    return {{"SC2000.EXE", false}, {"INSTALL.EXE", false}, {"README.COM", false}, {"DATA", true}};
}

// --- Wii / GameCube ---

TEST(launch_wii_quickboot) {
    FakeFs fs;
    LaunchPlan p = makeLaunchPlan(mkGame(System::Wii, "usb:/wbfs/Mario Kart [RMCP01]", "RMCP01.wbfs", "RMCP01"), fs);
    CHECK_EQ(p.dolPath, "sd:/apps/usbloader_gx/boot.dol");
    CHECK(p.args == Args{"RMCP01"});
    CHECK(p.quickBoot);
    CHECK(p.filesToWrite.empty());
}

TEST(launch_gamecube_quickboot) {
    FakeFs fs;
    LaunchPlan p = makeLaunchPlan(mkGame(System::GameCube, "usb:/games/Zelda [GZLP01]", "", "GZLP01"), fs);
    CHECK_EQ(p.dolPath, "sd:/apps/usbloader_gx/boot.dol");
    CHECK(p.args == Args{"GZLP01"});
    CHECK(p.quickBoot);
}

TEST(launch_wii_without_id_opens_loader_list) {
    FakeFs fs;
    LaunchPlan p = makeLaunchPlan(mkGame(System::Wii, "usb:/wbfs/Unbekannt", "x.wbfs", ""), fs);
    CHECK_EQ(p.dolPath, "sd:/apps/usbloader_gx/boot.dol");
    CHECK(p.args.empty());
    CHECK(!p.quickBoot);
}

// --- Emulatoren mit Ordner + Datei ---

TEST(launch_snes) {
    FakeFs fs;
    LaunchPlan p = makeLaunchPlan(mkGame(System::SNES, "usb:/snes9xgx/roms", "Super Mario World (E).sfc"), fs);
    CHECK_EQ(p.dolPath, "sd:/apps/snes9xgx/boot.dol");
    CHECK(p.args == (Args{"usb:/snes9xgx/roms", "Super Mario World (E).sfc"}));
    CHECK(p.filesToWrite.empty());
}

TEST(launch_nes) {
    FakeFs fs;
    LaunchPlan p = makeLaunchPlan(mkGame(System::NES, "usb:/fceugx/roms", "Zelda (E).nes"), fs);
    CHECK_EQ(p.dolPath, "sd:/apps/fceugx/boot.dol");
    CHECK(p.args == (Args{"usb:/fceugx/roms", "Zelda (E).nes"}));
}

TEST(launch_sega_scanner_dir_already_contains_subdir) {
    // Der Scanner setzt fuer Sega dir = usb:/genplus/roms/md (Unterordner schon enthalten).
    FakeFs fs;
    LaunchPlan p = makeLaunchPlan(mkGame(System::Sega, "usb:/genplus/roms/md", "Sonic 2 (W) [!].gen", "", "md"), fs);
    CHECK_EQ(p.dolPath, "sd:/apps/genplus-gx/boot.dol");
    CHECK(p.args == (Args{"usb:/genplus/roms/md", "Sonic 2 (W) [!].gen"}));
}

TEST(launch_sega_system_dir_plus_subdir) {
    FakeFs fs;
    LaunchPlan p = makeLaunchPlan(mkGame(System::Sega, "usb:/genplus/roms", "Alex Kidd (E) [!].sms", "", "sms"), fs);
    CHECK_EQ(p.dolPath, "sd:/apps/genplus-gx/boot.dol");
    CHECK(p.args == (Args{"usb:/genplus/roms/sms", "Alex Kidd (E) [!].sms"}));
}

// --- N64 ---

TEST(launch_n64_replaces_rompath) {
    FakeFs fs;
    fs.addFile("sd:/not64/settings.cfg", std::string("Audio = 1\nrompath = sd:/x\nScreenMode = 0\n"));
    LaunchPlan p = makeLaunchPlan(mkGame(System::N64, "usb:/not64/roms", "Mario64.z64"), fs);
    CHECK_EQ(p.dolPath, "sd:/apps/not64/boot.dol");
    CHECK(p.args.empty());
    CHECK_EQ(p.filesToWrite.size(), (size_t)1);
    CHECK_EQ(first(p).first, "sd:/not64/settings.cfg");
    CHECK_EQ(first(p).second, "Audio = 1\nrompath = usb:/not64/roms\nScreenMode = 0\n");
    std::string unchanged;
    fs.readText("sd:/not64/settings.cfg", unchanged);
    CHECK_EQ(unchanged, "Audio = 1\nrompath = sd:/x\nScreenMode = 0\n");  // nichts selbst geschrieben
}

TEST(launch_n64_appends_rompath) {
    FakeFs fs;
    fs.addFile("sd:/not64/settings.cfg", std::string("Audio = 1\nScreenMode = 0"));
    LaunchPlan p = makeLaunchPlan(mkGame(System::N64, "usb:/not64/roms", "Mario64.z64"), fs);
    CHECK_EQ(p.filesToWrite.size(), (size_t)1);
    CHECK_EQ(first(p).second, "Audio = 1\nScreenMode = 0\nrompath = usb:/not64/roms\n");
}

TEST(launch_n64_without_config) {
    FakeFs fs;
    LaunchPlan p = makeLaunchPlan(mkGame(System::N64, "usb:/not64/roms", "Mario64.z64"), fs);
    CHECK_EQ(p.filesToWrite.size(), (size_t)1);
    CHECK_EQ(first(p).second, "rompath = usb:/not64/roms\n");
}

// --- ScummVM ---

TEST(launch_scummvm) {
    FakeFs fs;
    LaunchPlan p = makeLaunchPlan(mkGame(System::Adventure, "usb:/scummvm/Monkey Island"), fs);
    CHECK_EQ(p.dolPath, "sd:/apps/scummvm/boot.elf");
    CHECK(p.args == (Args{"--auto-detect", "--path=usb:/scummvm/Monkey Island", "--extrapath=sd:/apps/scummvm",
                          "--themepath=sd:/apps/scummvm"}));
    CHECK(p.filesToWrite.empty());
}

// --- Lesezugriff auf den Spielordner (USB nach dem Scan ausgehängt) ---

TEST(launch_reads_game_dir_only_for_dosbox) {
    CHECK(launchReadsGameDir(mkGame(System::Adventure, "usb:/dosbox/SimCity 2000")));
    CHECK(!launchReadsGameDir(mkGame(System::Adventure, "usb:/scummvm/Monkey Island")));
    CHECK(!launchReadsGameDir(mkGame(System::Wii, "usb:/wbfs/Mario Kart [RMCP01]", "", "RMCP01")));
    CHECK(!launchReadsGameDir(mkGame(System::SNES, "usb:/snes9xgx/roms", "Zelda.sfc")));
    CHECK(!launchReadsGameDir(mkGame(System::N64, "usb:/not64/roms", "Mario64.z64")));
    CHECK(!launchReadsGameDir(mkGame(System::Sega, "usb:/genplus/roms", "Sonic.md", "", "md")));
}

// --- DOSBox ---

TEST(launch_dosbox_simcity_without_autoexec) {
    FakeFs fs;
    fs.addFile("sd:/launcher/dosbox.conf.tpl", std::string("[sdl]\nfullscreen=true\n"));
    fs.addFile("usb:/dosbox/SimCity 2000/SC2000.EXE");
    fs.addFile("usb:/dosbox/SimCity 2000/INSTALL.EXE");
    fs.addFile("usb:/dosbox/SimCity 2000/README.COM");
    LaunchPlan p = makeLaunchPlan(mkGame(System::Adventure, "usb:/dosbox/SimCity 2000"), fs);
    CHECK_EQ(p.dolPath, "sd:/apps/dosbox-wii/boot.dol");
    CHECK(p.args.empty());
    CHECK_EQ(p.filesToWrite.size(), (size_t)1);
    CHECK_EQ(first(p).first, "sd:/apps/dosbox-wii/dosbox.conf");
    CHECK_EQ(first(p).second,
             "[sdl]\nfullscreen=true\n[autoexec]\nmount c \"usb:/dosbox/SimCity 2000\"\nc:\nSC2000.EXE\n");
    CHECK(!fs.exists("sd:/apps/dosbox-wii/dosbox.conf"));
}

TEST(launch_dosbox_simcity_with_autoexec) {
    FakeFs fs;
    fs.addFile("sd:/launcher/dosbox.conf.tpl", std::string("[cpu]\ncore=dynamic\n"));
    fs.addFile("usb:/dosbox/SimCity 2000/SC2000.EXE");
    fs.addFile("usb:/dosbox/SimCity 2000/autoexec.txt", std::string("  SC2VESA.BAT  \r\nignored\n"));
    LaunchPlan p = makeLaunchPlan(mkGame(System::Adventure, "usb:/dosbox/SimCity 2000"), fs);
    CHECK_EQ(p.filesToWrite.size(), (size_t)1);
    CHECK_EQ(first(p).second,
             "[cpu]\ncore=dynamic\n[autoexec]\nmount c \"usb:/dosbox/SimCity 2000\"\nc:\nSC2VESA.BAT\n");
}

TEST(launch_dosbox_template_with_empty_autoexec) {
    FakeFs fs;
    fs.addFile("sd:/launcher/dosbox.conf.tpl", std::string("[sdl]\nfullscreen=true\n\n[autoexec]\n\n[cpu]\ncore=dynamic\n"));
    fs.addFile("usb:/dosbox/Doom/DOOM.EXE");
    LaunchPlan p = makeLaunchPlan(mkGame(System::Adventure, "usb:/dosbox/Doom"), fs);
    CHECK_EQ(p.filesToWrite.size(), (size_t)1);
    std::string c = first(p).second;
    CHECK_EQ(countOf(c, "[autoexec]"), (size_t)1);
    CHECK_EQ(c, "[sdl]\nfullscreen=true\n\n[cpu]\ncore=dynamic\n[autoexec]\nmount c \"usb:/dosbox/Doom\"\nc:\nDOOM.EXE\n");
}

TEST(launch_dosbox_template_autoexec_with_old_commands_removed) {
    FakeFs fs;
    fs.addFile("sd:/launcher/dosbox.conf.tpl", std::string("[sdl]\nfullscreen=true\n[AutoExec]\nmount d sd:/\nold.exe\n"));
    fs.addDir("usb:/dosbox/Leer");
    LaunchPlan p = makeLaunchPlan(mkGame(System::Adventure, "usb:/dosbox/Leer"), fs);
    CHECK_EQ(p.filesToWrite.size(), (size_t)1);
    // Kein Startbefehl gefunden: nur cd, DOSBox-Prompt.
    CHECK_EQ(first(p).second, "[sdl]\nfullscreen=true\n[autoexec]\nmount c \"usb:/dosbox/Leer\"\nc:\n");
}

TEST(launch_dosbox_without_template) {
    FakeFs fs;
    fs.addFile("usb:/dosbox/Doom/DOOM.EXE");
    LaunchPlan p = makeLaunchPlan(mkGame(System::Adventure, "usb:/dosbox/Doom"), fs);
    CHECK_EQ(p.filesToWrite.size(), (size_t)1);
    CHECK_EQ(first(p).second, "[autoexec]\nmount c \"usb:/dosbox/Doom\"\nc:\nDOOM.EXE\n");
}

// --- pickDosCommand ---

TEST(pick_dos_simcity_without_autoexec) {
    CHECK_EQ(pickDosCommand(simcityEntries(), "SimCity 2000", std::nullopt), "SC2000.EXE");
}

TEST(pick_dos_simcity_with_autoexec) {
    CHECK_EQ(pickDosCommand(simcityEntries(), "SimCity 2000", std::string("SC2VESA.BAT")), "SC2VESA.BAT");
}

TEST(pick_dos_empty_autoexec_falls_back) {
    CHECK_EQ(pickDosCommand(simcityEntries(), "SimCity 2000", std::string("  \n")), "SC2000.EXE");
}

TEST(pick_dos_single_executable_case_insensitive) {
    std::vector<DirEntry> e = {{"game.exe", false}, {"README.TXT", false}, {"SUB.BAT", true}};
    CHECK_EQ(pickDosCommand(e, "Irgendwas", std::nullopt), "game.exe");
}

TEST(pick_dos_bat_beats_exe_on_tie) {
    std::vector<DirEntry> e = {{"MONKEY.EXE", false}, {"MONKEY.BAT", false}, {"SETUP.EXE", false}};
    CHECK_EQ(pickDosCommand(e, "Monkey Island", std::nullopt), "MONKEY.BAT");
}

TEST(pick_dos_longest_prefix_wins) {
    std::vector<DirEntry> e = {{"PRINCE.EXE", false}, {"PRI.BAT", false}};
    CHECK_EQ(pickDosCommand(e, "Prince of Persia", std::nullopt), "PRINCE.EXE");
}

TEST(pick_dos_no_good_match_is_empty) {
    std::vector<DirEntry> e = {{"SETUP.EXE", false}, {"INSTALL.EXE", false}, {"SE.BAT", false}};
    CHECK_EQ(pickDosCommand(e, "Secret of Monkey Island", std::nullopt), "");
    CHECK_EQ(pickDosCommand({}, "Leer", std::nullopt), "");
}
