/*
 * game.c - The Controller (faithful port of src/resources/js/pikavolley.js).
 *
 * Implements the intro/menu/round/game-end state machine, drives the physics
 * engine, maps PSP input, and calls the render/audio modules.
 */
#include <pspctrl.h>
#include "game.h"
#include "render.h"
#include "audio.h"
#include "audio_layout.h"

#define GROUND_HALF_WIDTH      216
#define NORMAL_FPS              25
#define SLOW_MOTION_FPS          5
#define SLOW_MOTION_FRAMES_NUM   6
#define WINNING_SCORE           15

/* Frame totals (identical to pikavolley.js frameTotal). */
#define FRAMES_INTRO                   165
#define FRAMES_AFTER_MENU_SELECTION     15
#define FRAMES_BEFORE_START_OF_NEW_GAME 15
#define FRAMES_START_OF_NEW_GAME        71
#define FRAMES_AFTER_END_OF_ROUND        5
#define FRAMES_BEFORE_START_OF_NEXT_ROUND 30
#define FRAMES_GAME_END                211
#define FRAMES_NO_INPUT_MENU           225

static void build_input(const Game *g, const Input *input, PikaUserInput in[2]);

static void intro_state(Game *g, PikaUserInput in[2]);
static void menu_state(Game *g, PikaUserInput in[2]);
static void after_menu_selection_state(Game *g);
static void before_start_of_new_game_state(Game *g);
static void start_of_new_game_state(Game *g);
static void round_state(Game *g, PikaUserInput in[2]);
static void after_end_of_round_state(Game *g);
static void before_start_of_next_round_state(Game *g);
static void play_sound_effect(Game *g);

void game_init(Game *g) {
    physics_init(&g->physics, 1, 1); /* both computer initially */
    g->state = ST_INTRO;
    g->frameCounter = 0;
    g->noInputFrameCounter = 0;
    g->slowMotionFramesLeft = 0;
    g->slowMotionNumOfSkippedFrames = 0;
    g->scores[0] = 0;
    g->scores[1] = 0;
    g->winningScore = WINNING_SCORE;
    g->gameEnded = 0;
    g->roundEnded = 0;
    g->isPlayer2Serve = 0;
    g->paused = 0;
}

void game_restart(Game *g) {
    g->frameCounter = 0;
    g->noInputFrameCounter = 0;
    g->slowMotionFramesLeft = 0;
    g->slowMotionNumOfSkippedFrames = 0;
    g->state = ST_INTRO;
}

/*
 * Map the PSP controller to the players.
 *   Player 1: D-pad (movement/jump) + CROSS (power hit).
 * Player 2 is intentionally unbound: there is no 1v1 on a single PSP, so the
 * second player is always driven by the computer AI (or inert in friend mode).
 * The computer AI overwrites the input for its own player inside physics.
 */
static void build_input(const Game *g, const Input *input, PikaUserInput in[2]) {
    (void)g;
    const SceCtrlData *pad = &input->pad;

    int x1 = 0, y1 = 0;
    if (pad->Buttons & PSP_CTRL_LEFT) x1 = -1;
    else if (pad->Buttons & PSP_CTRL_RIGHT) x1 = 1;
    if (pad->Buttons & PSP_CTRL_UP) y1 = -1;
    else if (pad->Buttons & PSP_CTRL_DOWN) y1 = 1;
    in[0].xDirection = x1;
    in[0].yDirection = y1;
    in[0].powerHit = input_pressed(input, PSP_CTRL_CROSS) ? 1 : 0;

    /* Player 2: no controller bindings. */
    in[1].xDirection = 0;
    in[1].yDirection = 0;
    in[1].powerHit = 0;
}

int game_tick(Game *g, const Input *input) {
    if (g->paused) {
        return 0;
    }
    if (g->slowMotionFramesLeft > 0) {
        g->slowMotionNumOfSkippedFrames++;
        if (g->slowMotionNumOfSkippedFrames %
                ((NORMAL_FPS + SLOW_MOTION_FPS / 2) / SLOW_MOTION_FPS) != 0) {
            return 0;
        }
        g->slowMotionFramesLeft--;
        g->slowMotionNumOfSkippedFrames = 0;
    }

    PikaUserInput in[2];
    build_input(g, input, in);

    render_begin_frame();

    switch (g->state) {
    case ST_INTRO: intro_state(g, in); break;
    case ST_MENU: menu_state(g, in); break;
    case ST_AFTER_MENU_SELECTION: after_menu_selection_state(g); break;
    case ST_BEFORE_START_OF_NEW_GAME: before_start_of_new_game_state(g); break;
    case ST_START_OF_NEW_GAME: start_of_new_game_state(g); break;
    case ST_ROUND: round_state(g, in); break;
    case ST_AFTER_END_OF_ROUND: after_end_of_round_state(g); break;
    case ST_BEFORE_START_OF_NEXT_ROUND: before_start_of_next_round_state(g); break;
    }

    render_end_frame();
    return 1;
}

/* ------------------------------------------------------------------ */
/* Intro: a man with a brief case                                     */
/* ------------------------------------------------------------------ */

static void intro_state(Game *g, PikaUserInput in[2]) {
    if (g->frameCounter == 0) {
        render_set_black_alpha(0.0f);
        audio_stop_bgm();
    }
    render_draw_intro_mark(g->frameCounter);
    g->frameCounter++;

    if (in[0].powerHit == 1 || in[1].powerHit == 1) {
        g->frameCounter = 0;
        g->state = ST_MENU;
        return;
    }
    if (g->frameCounter >= FRAMES_INTRO) {
        g->frameCounter = 0;
        g->state = ST_MENU;
    }
}

/* ------------------------------------------------------------------ */
/* Menu: select who to play with                                       */
/* ------------------------------------------------------------------ */

static void menu_state(Game *g, PikaUserInput in[2]) {
    if (g->frameCounter == 0) {
        render_set_black_alpha(0.0f);
    }
    render_draw_menu(g->frameCounter);
    g->frameCounter++;

    if (g->frameCounter < 71 &&
        (in[0].powerHit == 1 || in[1].powerHit == 1)) {
        g->frameCounter = 71;
        return;
    }
    if (g->frameCounter <= 71) {
        return;
    }

    g->noInputFrameCounter++;

    if (in[0].powerHit == 1 || in[1].powerHit == 1) {
        /* Human (player 1) vs computer. */
        g->physics.player1.isComputer = 0;
        g->physics.player2.isComputer = 1;
        audio_play_sfx(SFX_PIKACHU, 0);
        g->frameCounter = 0;
        g->noInputFrameCounter = 0;
        g->state = ST_AFTER_MENU_SELECTION;
        return;
    }

    if (g->noInputFrameCounter >= FRAMES_NO_INPUT_MENU) {
        /* No input: AI vs AI. */
        g->physics.player1.isComputer = 1;
        g->physics.player2.isComputer = 1;
        g->frameCounter = 0;
        g->noInputFrameCounter = 0;
        g->state = ST_AFTER_MENU_SELECTION;
    }
}

/* ------------------------------------------------------------------ */
/* Fade out after menu selection                                       */
/* ------------------------------------------------------------------ */

static void after_menu_selection_state(Game *g) {
    render_change_black_alpha(1.0f / 16.0f);
    g->frameCounter++;
    if (g->frameCounter >= FRAMES_AFTER_MENU_SELECTION) {
        g->frameCounter = 0;
        g->state = ST_BEFORE_START_OF_NEW_GAME;
    }
}

/* ------------------------------------------------------------------ */
/* Delay before start of new game                                      */
/* ------------------------------------------------------------------ */

static void before_start_of_new_game_state(Game *g) {
    g->frameCounter++;
    if (g->frameCounter >= FRAMES_BEFORE_START_OF_NEW_GAME) {
        g->frameCounter = 0;
        g->state = ST_START_OF_NEW_GAME;
    }
}

/* ------------------------------------------------------------------ */
/* Start of new game                                                   */
/* ------------------------------------------------------------------ */

static void start_of_new_game_state(Game *g) {
    if (g->frameCounter == 0) {
        g->gameEnded = 0;
        g->roundEnded = 0;
        g->isPlayer2Serve = 0;
        g->physics.player1.gameEnded = 0;
        g->physics.player1.isWinner = 0;
        g->physics.player2.gameEnded = 0;
        g->physics.player2.isWinner = 0;

        g->scores[0] = 0;
        g->scores[1] = 0;

        player_init_new_round(&g->physics.player1);
        player_init_new_round(&g->physics.player2);
        ball_init_new_round(&g->physics.ball, g->isPlayer2Serve);

        render_set_black_alpha(1.0f);
        audio_start_bgm();
    }

    render_game_scene(&g->physics, g->scores);
    render_draw_game_start_message(g->frameCounter, FRAMES_START_OF_NEW_GAME);
    render_change_black_alpha(-(1.0f / 17.0f));
    g->frameCounter++;

    if (g->frameCounter >= FRAMES_START_OF_NEW_GAME) {
        g->frameCounter = 0;
        render_set_black_alpha(0.0f);
        g->state = ST_ROUND;
    }
}

/* ------------------------------------------------------------------ */
/* Round                                                              */
/* ------------------------------------------------------------------ */

static void round_state(Game *g, PikaUserInput in[2]) {
    const int pressedPowerHit =
        in[0].powerHit == 1 || in[1].powerHit == 1;

    if (g->physics.player1.isComputer &&
        g->physics.player2.isComputer && pressedPowerHit) {
        g->frameCounter = 0;
        g->state = ST_INTRO;
        return;
    }

    const int isBallTouchingGround = physics_step(&g->physics, in);

    play_sound_effect(g);
    render_game_scene(&g->physics, g->scores);

    if (g->gameEnded) {
        render_draw_game_end_message(g->frameCounter);
        g->frameCounter++;
        if (g->frameCounter >= FRAMES_GAME_END ||
            (g->frameCounter >= 70 && pressedPowerHit)) {
            g->frameCounter = 0;
            g->state = ST_INTRO;
        }
        return;
    }

    if (isBallTouchingGround &&
        g->roundEnded == 0 &&
        g->gameEnded == 0) {
        if (g->physics.ball.punchEffectX < GROUND_HALF_WIDTH) {
            g->isPlayer2Serve = 1;
            g->scores[1] += 1;
            if (g->scores[1] >= g->winningScore) {
                g->gameEnded = 1;
                g->physics.player1.isWinner = 0;
                g->physics.player2.isWinner = 1;
                g->physics.player1.gameEnded = 1;
                g->physics.player2.gameEnded = 1;
            }
        } else {
            g->isPlayer2Serve = 0;
            g->scores[0] += 1;
            if (g->scores[0] >= g->winningScore) {
                g->gameEnded = 1;
                g->physics.player1.isWinner = 1;
                g->physics.player2.isWinner = 0;
                g->physics.player1.gameEnded = 1;
                g->physics.player2.gameEnded = 1;
            }
        }
        if (g->roundEnded == 0 && g->gameEnded == 0) {
            g->slowMotionFramesLeft = SLOW_MOTION_FRAMES_NUM;
        }
        g->roundEnded = 1;
    }

    if (g->roundEnded && !g->gameEnded) {
        if (g->slowMotionFramesLeft == 0) {
            render_change_black_alpha(1.0f / 16.0f);
            g->state = ST_AFTER_END_OF_ROUND;
        }
    }
}

/* ------------------------------------------------------------------ */
/* Fade out after end of round                                         */
/* ------------------------------------------------------------------ */

static void after_end_of_round_state(Game *g) {
    render_change_black_alpha(1.0f / 16.0f);
    g->frameCounter++;
    if (g->frameCounter >= FRAMES_AFTER_END_OF_ROUND) {
        g->frameCounter = 0;
        g->state = ST_BEFORE_START_OF_NEXT_ROUND;
    }
}

/* ------------------------------------------------------------------ */
/* Before start of next round                                          */
/* ------------------------------------------------------------------ */

static void before_start_of_next_round_state(Game *g) {
    if (g->frameCounter == 0) {
        render_set_black_alpha(1.0f);
        render_draw_ready_message(0);

        player_init_new_round(&g->physics.player1);
        player_init_new_round(&g->physics.player2);
        ball_init_new_round(&g->physics.ball, g->isPlayer2Serve);
    }

    render_game_scene(&g->physics, g->scores);
    render_change_black_alpha(-(1.0f / 16.0f));

    g->frameCounter++;
    if (g->frameCounter % 5 == 0) {
        render_toggle_ready_message();
    }

    if (g->frameCounter >= FRAMES_BEFORE_START_OF_NEXT_ROUND) {
        g->frameCounter = 0;
        render_draw_ready_message(0);
        render_set_black_alpha(0.0f);
        g->roundEnded = 0;
        g->state = ST_ROUND;
    }
}

/* ------------------------------------------------------------------ */
/* Sound effects (from pikavolley.js playSoundEffect)                  */
/* ------------------------------------------------------------------ */

static void play_sound_effect(Game *g) {
    for (int i = 0; i < 2; i++) {
        Player *player = (i == 0) ? &g->physics.player1 : &g->physics.player2;
        int pan = (i == 0) ? -1 : 1;
        if (player->snd_pipikachu) {
            audio_play_sfx(SFX_PIPIKACHU, pan);
            player->snd_pipikachu = 0;
        }
        if (player->snd_pika) {
            audio_play_sfx(SFX_PIKA, pan);
            player->snd_pika = 0;
        }
        if (player->snd_chu) {
            audio_play_sfx(SFX_CHU, pan);
            player->snd_chu = 0;
        }
    }
    Ball *ball = &g->physics.ball;
    int pan = 0;
    if (ball->punchEffectX < GROUND_HALF_WIDTH) {
        pan = -1;
    } else if (ball->punchEffectX > GROUND_HALF_WIDTH) {
        pan = 1;
    }
    if (ball->snd_powerHit) {
        audio_play_sfx(SFX_POWERHIT, pan);
        ball->snd_powerHit = 0;
    }
    if (ball->snd_ballTouchesGround) {
        audio_play_sfx(SFX_BALLTOUCHESGROUND, pan);
        ball->snd_ballTouchesGround = 0;
    }
}
