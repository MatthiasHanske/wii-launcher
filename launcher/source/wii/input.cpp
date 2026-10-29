#include "wii/input.h"

#include <wiiuse/wpad.h>

void inputInit(int width, int height) {
    WPAD_SetDataFormat(WPAD_CHAN_0, WPAD_FMT_BTNS_ACC_IR);
    WPAD_SetVRes(WPAD_CHAN_0, static_cast<u32>(width), static_cast<u32>(height));
}

InputState pollInput() {
    WPAD_ScanPads();
    InputState s;
    s.pressed = WPAD_ButtonsDown(WPAD_CHAN_0);
    s.held = WPAD_ButtonsHeld(WPAD_CHAN_0);
    ir_t ir;
    WPAD_IR(WPAD_CHAN_0, &ir);
    if (ir.valid) {
        s.pointerValid = true;
        s.pointerX = ir.x;
        s.pointerY = ir.y;
    }
    return s;
}
