#include <grrlib.h>
#include <wiiuse/wpad.h>

int main() {
    GRRLIB_Init();
    WPAD_Init();

    while (true) {
        WPAD_ScanPads();
        if (WPAD_ButtonsDown(0) & WPAD_BUTTON_HOME) break;

        GRRLIB_FillScreen(0x0F1218FF);
        GRRLIB_Rectangle(100, 100, 200, 60, 0x3B82F6FF, true);
        GRRLIB_Render();
    }

    GRRLIB_Exit();
    return 0;
}
