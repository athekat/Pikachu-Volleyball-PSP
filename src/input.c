/*
 * input.c - PSP controller input.
 */
#include "input.h"

void input_init(Input *in) {
    sceCtrlSetSamplingCycle(0);
    sceCtrlSetSamplingMode(PSP_CTRL_MODE_ANALOG);
    in->prevButtons = 0;
    in->justPressed = 0;
}

void input_poll(Input *in) {
    sceCtrlReadBufferPositive(&in->pad, 1);
    in->justPressed = in->pad.Buttons & ~in->prevButtons;
    in->prevButtons = in->pad.Buttons;
}
