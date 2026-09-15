/*
 * host_test.c - Host-side sanity test for the physics port.
 *
 * Compile:  gcc -O2 -o /tmp/host_test src/physics.c src/rand.c tools/host_test.c
 *
 * Simulates several AI-vs-AI games and asserts the state remains within the
 * documented bounds (ball/player positions, valid states, finite values).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../src/physics.h"
#include "../src/rand.h"

#define GROUND_WIDTH  432
#define BALL_GROUND_Y 252

static int check_bounds(PikaPhysics *p, long frame, int *bad) {
    Player *p1 = &p->player1;
    Player *p2 = &p->player2;
    Ball *b = &p->ball;

    if (p1->x < 0 || p1->x > GROUND_WIDTH || p2->x < 0 || p2->x > GROUND_WIDTH) {
        printf("frame %ld: player X out of range p1=%d p2=%d\n", frame, p1->x, p2->x);
        (*bad)++;
    }
    if (p1->state < 0 || p1->state > 6 || p2->state < 0 || p2->state > 6) {
        printf("frame %ld: bad state p1=%d p2=%d\n", frame, p1->state, p2->state);
        (*bad)++;
    }
    if (b->x < -200 || b->x > GROUND_WIDTH + 200 || b->y < -200 || b->y > BALL_GROUND_Y + 10) {
        printf("frame %ld: ball out of range x=%d y=%d\n", frame, b->x, b->y);
        (*bad)++;
    }
    if (b->rotation < 0 || b->rotation > 5) {
        printf("frame %ld: bad ball rotation %d (fine %d)\n", frame, b->rotation, b->fineRotation);
        (*bad)++;
    }
    return 0;
}

/* Minimal score logic mirroring the controller, to drive round transitions. */
static void simulate_game(int seed, long max_frames) {
    PikaPhysics p;
    PikaUserInput in[2];
    pika_srand(seed);

    physics_init(&p, 1, 1); /* AI vs AI */

    int scores[2] = {0, 0};
    int isPlayer2Serve = 0;
    int roundEnded = 0;
    int gameEnded = 0;
    int bad = 0;
    int ballTouches = 0;

    for (long f = 0; f < max_frames && !gameEnded; f++) {
        in[0].xDirection = 0; in[0].yDirection = 0; in[0].powerHit = 0;
        in[1].xDirection = 0; in[1].yDirection = 0; in[1].powerHit = 0;

        int touched = physics_step(&p, in);
        (void)touched;

        /* consume sound flags (mirrors controller) */
        p.player1.snd_pipikachu = p.player1.snd_pika = p.player1.snd_chu = 0;
        p.player2.snd_pipikachu = p.player2.snd_pika = p.player2.snd_chu = 0;
        p.ball.snd_powerHit = p.ball.snd_ballTouchesGround = 0;

        check_bounds(&p, f, &bad);

        if (touched && !roundEnded && !gameEnded) {
            ballTouches++;
            if (p.ball.punchEffectX < 216) {
                isPlayer2Serve = 1;
                scores[1]++;
                if (scores[1] >= 15) {
                    gameEnded = 1;
                    p.player1.isWinner = 0; p.player2.isWinner = 1;
                    p.player1.gameEnded = 1; p.player2.gameEnded = 1;
                }
            } else {
                isPlayer2Serve = 0;
                scores[0]++;
                if (scores[0] >= 15) {
                    gameEnded = 1;
                    p.player1.isWinner = 1; p.player2.isWinner = 0;
                    p.player1.gameEnded = 1; p.player2.gameEnded = 1;
                }
            }
            roundEnded = 1;
        }

        if (roundEnded && !gameEnded) {
            /* after ~35 frames of round-end, reset for next round */
            static long end_frame = -1;
            if (end_frame < 0) end_frame = f;
            if (f - end_frame >= 35) {
                player_init_new_round(&p.player1);
                player_init_new_round(&p.player2);
                ball_init_new_round(&p.ball, isPlayer2Serve);
                roundEnded = 0;
                end_frame = -1;
            }
        }
    }

    printf("seed %u: frames=%ld touches=%d score=%d-%d gameEnded=%d bad=%d\n",
           seed, max_frames, ballTouches, scores[0], scores[1], gameEnded, bad);
    if (bad) exit(1);
}

int main(void) {
    /* A few thousand frames of AI-vs-AI, multiple seeds. */
    for (int seed = 1; seed <= 5; seed++) {
        simulate_game(seed, 20000);
    }
    printf("physics host test OK\n");
    return 0;
}
