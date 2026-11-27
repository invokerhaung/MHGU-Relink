// forge_wheel_setInputArea / forge_wheel_getPointer: the wheel mod (CraftWheel) tells the port where its wheel is, in the
// game's 1280x720 canvas, and the port hands it the finger that is on it. The Android wheel reads its pointer through
// forge_input_getTouch (runtime/touch.cpp touch_first_finger); this bridge lets it read the finger only while it is on
// the wheel, and lets the port keep the touch controls from acting on a finger the wheel owns (touch.cpp touch_input).
//
// forge_wheel_setInputArea(int mode, float x, float y, float radius): mode 0 off, 1 the game's wheel, 2 its editor;
// x, y, radius in canvas units. forge_wheel_getPointer(float *x, float *y): 0 no finger, 1 a finger on the wheel (its
// canvas point written), 2 cancelled (never here: the port has no cancel of its own).
#include "../hle.h"
#include "../touch.h"

HLE_FUNC("forge_wheel_setInputArea", f_wheel_area) {
    touch_wheel_area((int)(int32_t)ARG(0), ARGF(1), ARGF(2), ARGF(3));
}
HLE_FUNC("forge_wheel_getPointer", f_wheel_pointer) {
    float x = 0, y = 0;
    int state = touch_wheel_pointer(&x, &y);
    if (state == 1) {
        if (ARG(0)) STF(ARG(0), x);
        if (ARG(1)) STF(ARG(1), y);
    }
    RET((uint32_t)state);
}
