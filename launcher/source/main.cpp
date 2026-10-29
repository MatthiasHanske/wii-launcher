#include <grrlib.h>
#include <wiiuse/wpad.h>

#include <vector>

#include "core/launch.h"
#include "core/model.h"
#include "core/scanner.h"
#include "wii/boot.h"
#include "wii/fs_wii.h"

// Vorläufige Rauchtest-Anzeige (Task 7), bis es eine Schrift gibt:
// - oben links: Quadrat USB (grün = eingehängt, rot = fehlgeschlagen), daneben SD
// - je System ein Balken, 1 px pro Spiel (max. 560 px; weißer Endstrich = abgeschnitten)
// - darunter je 10 gefundene Spiele (aufgerundet) ein 20-px-Quadrat, max. 60
namespace {

constexpr u32 kGreen = 0x22C55EFF;
constexpr u32 kRed = 0xEF4444FF;
constexpr u32 kWhite = 0xFFFFFFFF;

const System kSystems[] = {System::Wii, System::GameCube, System::N64, System::SNES,
                           System::NES, System::Sega,     System::Adventure};
const u32 kColors[] = {0x3B82F6FF, 0x8B5CF6FF, 0xF59E0BFF, 0xEC4899FF,
                       0x10B981FF, 0x06B6D4FF, 0xF97316FF};
constexpr int kSystemCount = sizeof(kSystems) / sizeof(kSystems[0]);

// TEMPORÄR (Task 8, Hardwaretests A/B/C): feste Startpläne auf Knopfdruck.
// A = Snes9x GX mit Super Mario World, 1 = USB Loader GX mit RMCP01,
// 2 = returnToWiiMenu(), Minus = launchHbc(). Fliegt raus, sobald es ein Menü gibt.
LaunchPlan testPlanSnes() {
    LaunchPlan p;
    p.dolPath = "sd:/apps/snes9xgx/boot.dol";
    p.args = {"usb:/snes9xgx/roms", "Super Mario World (Europe).sfc"};
    return p;
}

LaunchPlan testPlanUsbLoader() {
    LaunchPlan p;
    p.dolPath = "sd:/apps/usbloader_gx/boot.dol";
    p.args = {"RMCP01"};
    return p;
}

}  // namespace

int main() {
    GRRLIB_Init();
    WPAD_Init();

    WiiFs fs;
    bool sdOk = fs.mountSd();
    bool usbOk = fs.mountUsb(10, 500);

    int counts[kSystemCount] = {};
    int total = 0;
    if (usbOk) {
        std::vector<Game> games = scanLibrary(fs, "usb:/");
        total = static_cast<int>(games.size());
        for (const Game& g : games)
            for (int i = 0; i < kSystemCount; ++i)
                if (g.system == kSystems[i]) ++counts[i];
    }

    int squares = (total + 9) / 10;
    if (squares > 60) squares = 60;

    bool bootFailed = false;
    while (true) {
        WPAD_ScanPads();
        u32 down = WPAD_ButtonsDown(0);
        if (down & WPAD_BUTTON_HOME) break;
        // TEMPORÄR (Task 8): bootDol kehrt nur bei Fehler zurück -> rotes Feld oben rechts.
        if (down & WPAD_BUTTON_A) bootFailed = !bootDol(testPlanSnes(), fs);
        if (down & WPAD_BUTTON_1) bootFailed = !bootDol(testPlanUsbLoader(), fs);
        if (down & WPAD_BUTTON_2) {
            fs.unmountAll();
            GRRLIB_Exit();
            returnToWiiMenu();
        }
        if (down & WPAD_BUTTON_MINUS) {
            fs.unmountAll();
            GRRLIB_Exit();
            launchHbc();
        }

        GRRLIB_FillScreen(0x0F1218FF);
        GRRLIB_Rectangle(40, 30, 30, 30, usbOk ? kGreen : kRed, true);
        GRRLIB_Rectangle(80, 30, 30, 30, sdOk ? kGreen : kRed, true);
        if (bootFailed) GRRLIB_Rectangle(570, 30, 30, 30, kRed, true);

        for (int i = 0; i < kSystemCount; ++i) {
            int y = 80 + i * 28;
            int w = counts[i] > 560 ? 560 : counts[i];
            GRRLIB_Rectangle(40, y, 560, 20, 0x1F2937FF, true);
            if (w > 0) GRRLIB_Rectangle(40, y, w, 20, kColors[i], true);
            if (counts[i] > 560) GRRLIB_Rectangle(596, y, 4, 20, kWhite, true);
        }

        for (int i = 0; i < squares; ++i) {
            int x = 40 + (i % 20) * 28;
            int y = 290 + (i / 20) * 28;
            GRRLIB_Rectangle(x, y, 20, 20, kWhite, true);
        }

        GRRLIB_Render();
    }

    fs.unmountAll();
    GRRLIB_Exit();
    return 0;
}
