/*
 * game.h - The Controller: state machine driving physics + rendering.
 *
 * Faithful port of src/resources/js/pikavolley.js.
 */
#ifndef PIKA_GAME_H
#define PIKA_GAME_H

#include "physics.h"
#include "input.h"

enum {
    ST_INTRO = 0,
    ST_MENU,
    ST_AFTER_MENU_SELECTION,
    ST_BEFORE_START_OF_NEW_GAME,
    ST_START_OF_NEW_GAME,
    ST_ROUND,
    ST_AFTER_END_OF_ROUND,
    ST_BEFORE_START_OF_NEXT_ROUND,
};

typedef struct {
    PikaPhysics physics;
    int state;
    int frameCounter;
    int noInputFrameCounter;
    int slowMotionFramesLeft;
    int slowMotionNumOfSkippedFrames;
    int scores[2];
    int winningScore;
    int gameEnded;
    int roundEnded;
    int isPlayer2Serve;
    int paused;
} Game;

void game_init(Game *g);

/*
 * Advance one game frame (25 fps) and render it.
 * Returns 1 if a frame was actually advanced + rendered, 0 if skipped
 * (paused or slow-motion).
 */
int game_tick(Game *g, const Input *input);

/* Return to the intro (restart). */
void game_restart(Game *g);

/* Pause toggle is handled by the caller via game->paused. */

#endif /* PIKA_GAME_H */
