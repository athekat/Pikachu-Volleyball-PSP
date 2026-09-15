/*
 * render.h - GU-based 2D rendering (the "View").
 *
 * The game renders in the original 432x304 logical space; render.c scales it
 * (aspect-preserving, letterboxed) to the 480x272 PSP screen.
 */
#ifndef PIKA_RENDER_H
#define PIKA_RENDER_H

#include "physics.h"

void render_init(void);
void render_begin_frame(void);
void render_end_frame(void);   /* draws fade overlay, swaps buffers */

/* Intro (man with briefcase) */
void render_draw_intro_mark(int frameCounter);

/* Menu */
void render_draw_menu(int frameCounter);

/* Game view: full scene in z-order (bg -> clouds/wave -> players/ball -> scores) */
void render_game_scene(PikaPhysics *p, const int scores[2]);

/* Overlay messages (drawn on top of the scene) */
void render_draw_game_start_message(int frameCounter, int frameTotal);
void render_draw_ready_message(int on);
void render_toggle_ready_message(void);
void render_draw_game_end_message(int frameCounter);

/* Fade in/out overlay */
void render_set_black_alpha(float alpha);
void render_change_black_alpha(float delta);

#endif /* PIKA_RENDER_H */
