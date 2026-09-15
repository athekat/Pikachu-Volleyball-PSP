/*
 * main.c - PSP entry point for Pikachu Volleyball.
 *
 * Targets 6.60 PRO-C (and works under PPSSPP).  Home button exits via the
 * kernel exit callback.  The game runs at 25 fps; the main loop is vblank
 * synced (60 Hz) and advances the game every 2.4 vblanks via a fixed-point
 * accumulator.
 *
 * Controls:
 *   D-pad ......... player 1 movement / jump (also menu navigation)
 *   CROSS ......... player 1 power hit / confirm / skip
 *   START ......... pause
 *   SELECT ........ restart (back to intro)
 *   HOME .......... exit
 * (Player 2 has no controller bindings — there is no 1v1 on a single PSP.)
 */
#include <pspkernel.h>
#include <pspdisplay.h>
#include <pspctrl.h>
#include <stddef.h>
#include "game.h"
#include "render.h"
#include "audio.h"
#include "input.h"
#include "rand.h"

PSP_MODULE_INFO("PikachuVolleyball", 0, 1, 1);
PSP_MAIN_THREAD_ATTR(THREAD_ATTR_USER | THREAD_ATTR_VFPU);
PSP_HEAP_SIZE_KB(8192);

static volatile int g_exit_request = 0;

static int exit_callback(int arg1, int arg2, void *common) {
    (void)arg1;
    (void)arg2;
    (void)common;
    g_exit_request = 1;
    return 0;
}

static int callback_thread(SceSize args, void *argp) {
    (void)args;
    (void)argp;
    int cbid = sceKernelCreateCallback("ExitCallback", exit_callback, NULL);
    sceKernelRegisterExitCallback(cbid);
    sceKernelSleepThreadCB();
    return 0;
}

static void setup_callbacks(void) {
    int thid = sceKernelCreateThread("callback_thread", callback_thread,
                                     0x11, 0xFA0, 0, NULL);
    if (thid >= 0) {
        sceKernelStartThread(thid, 0, NULL);
    }
}

int main(void) {
    setup_callbacks();
    pika_srand((unsigned int)sceKernelGetSystemTimeLow());

    render_init();
    audio_init();

    Game game;
    game_init(&game);

    Input input;
    input_init(&input);

    int acc = 0;
    while (!g_exit_request) {
        sceDisplayWaitVblankStart();

        /* 25 fps = 2.4 vblanks = 12/5; accumulate in fifths of a vblank.
         * Input is polled once per game tick (not per vblank) so the
         * edge-triggered "pressed" state is computed at the same 25 fps rate
         * the game consumes it — otherwise a 16.7 ms edge is cleared between
         * ticks and button presses get dropped. */
        acc += 5;
        if (acc >= 12) {
            acc -= 12;
            input_poll(&input);

            if (input_pressed(&input, PSP_CTRL_START)) {
                game.paused = !game.paused;
            }
            if (input_pressed(&input, PSP_CTRL_SELECT)) {
                game_restart(&game);
            }

            game_tick(&game, &input);
        }
    }

    audio_shutdown();
    sceKernelExitGame();
    return 0;
}
