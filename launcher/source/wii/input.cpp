#include "wii/input.h"

#include <wiiuse/wpad.h>

namespace {
constexpr int kChannels = 4;  // WPAD_CHAN_0..WPAD_CHAN_3
}

void inputInit(int width, int height) {
    for (int chan = WPAD_CHAN_0; chan < WPAD_CHAN_0 + kChannels; ++chan) {
        WPAD_SetDataFormat(chan, WPAD_FMT_BTNS_ACC_IR);
        WPAD_SetVRes(chan, static_cast<u32>(width), static_cast<u32>(height));
    }
}

InputState pollInput() {
    WPAD_ScanPads();
    InputState s;
    for (int chan = WPAD_CHAN_0; chan < WPAD_CHAN_0 + kChannels; ++chan) {
        s.pressed |= WPAD_ButtonsDown(chan) & kWiimoteButtons;
        s.held |= WPAD_ButtonsHeld(chan) & kWiimoteButtons;
        if (s.pointerValid) continue;
        ir_t ir{};
        WPAD_IR(chan, &ir);
        if (ir.valid) {
            s.pointerValid = true;
            s.pointerX = ir.x;
            s.pointerY = ir.y;
        }
    }
    return s;
}
