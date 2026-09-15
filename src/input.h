/*
 * input.h - PSP controller input (D-pad, analog, buttons).
 */
#ifndef PIKA_INPUT_H
#define PIKA_INPUT_H

#include <pspctrl.h>

typedef struct {
    SceCtrlData pad;        /* current frame */
    unsigned int prevButtons;
    unsigned int justPressed; /* buttons newly pressed this frame (edge) */
} Input;

void input_init(Input *in);
void input_poll(Input *in);

/* Button held this frame. */
static inline int input_down(const Input *in, unsigned int btn) {
    return (in->pad.Buttons & btn) != 0;
}

/* Button freshly pressed this frame (edge-triggered). */
static inline int input_pressed(const Input *in, unsigned int btn) {
    return (in->justPressed & btn) != 0;
}

#endif /* PIKA_INPUT_H */
