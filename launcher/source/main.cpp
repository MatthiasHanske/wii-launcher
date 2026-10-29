#include "wii/app.h"
#ifdef LAUNCHER_GFX_DEMO
#include "wii/demo.h"
#include "wii/fs_wii.h"
#endif

int main() {
#ifdef LAUNCHER_GFX_DEMO
    {
        WiiFs demoFs;
        runGfxDemo(demoFs);
        return 0;
    }
#endif
    app::run();
    return 0;
}
