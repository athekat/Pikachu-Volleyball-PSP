/*
 * physics.h - Faithful C port of src/resources/js/physics.js.
 *
 * This module is the physics engine reverse-engineered from the original
 * 1997 Windows game (machine-code functions at 00403dd0, 00402dc0, 00401fc0,
 * 004025e0, 004030a0, 004031b0, 00402360, 00402630, 00402870).
 *
 * Coordinate system (logical pixels, identical to the original):
 *   Ground width 432, height 304. X in [0,432] right-increasing,
 *   Y in [0,304] down-increasing. Ball radius 20. Player 64x64 (half 32).
 *
 * This file is PSP-independent (no PSP headers) so it can be unit-tested on
 * the host build machine.
 */
#ifndef PIKA_PHYSICS_H
#define PIKA_PHYSICS_H

/* ------------------------------------------------------------------ */
/* User input (one per player)                                        */
/* ------------------------------------------------------------------ */
typedef struct {
    int xDirection; /* 0: none, -1: left, 1: right */
    int yDirection; /* 0: none, -1: up, 1: down */
    int powerHit;   /* 0: no / auto-repeated, 1: fresh (edge-triggered) press */
} PikaUserInput;

/* ------------------------------------------------------------------ */
/* Player                                                             */
/* ------------------------------------------------------------------ */
typedef struct {
    int isPlayer2;                     /* on the right side? */
    int isComputer;                    /* controlled by computer? */
    int x, y;                          /* position (center) */
    int yVelocity;
    int isCollisionWithBallHappened;
    /* state: 0 normal, 1 jumping, 2 jumping_and_power_hitting,
     *        3 diving, 4 lying_down_after_diving, 5 win!, 6 lost.. */
    int state;
    int frameNumber;
    int normalStatusArmSwingDirection;
    int delayBeforeNextFrame;
    int divingDirection;               /* -1 left, 0 none, 1 right */
    int lyingDownDurationLeft;
    int isWinner;
    int gameEnded;
    int computerWhereToStandBy;        /* 0 or 1 */
    int computerBoldness;              /* 0..4 */
    /* sound-effect flags (consumed by the controller each frame) */
    int snd_pipikachu;
    int snd_pika;
    int snd_chu;
} Player;

/* ------------------------------------------------------------------ */
/* Ball                                                               */
/* ------------------------------------------------------------------ */
typedef struct {
    int x, y;                          /* position (center) */
    int xVelocity, yVelocity;
    int expectedLandingPointX;
    int rotation;                      /* 0..5 (5 = hyper ball glitch) */
    int fineRotation;                  /* 0..50 */
    int punchEffectX, punchEffectY;
    int punchEffectRadius;
    int previousX, previousPreviousX;
    int previousY, previousPreviousY;
    int isPowerHit;
    /* sound-effect flags */
    int snd_powerHit;
    int snd_ballTouchesGround;
} Ball;

/* ------------------------------------------------------------------ */
/* Physics pack                                                       */
/* ------------------------------------------------------------------ */
typedef struct {
    Player player1;
    Player player2;
    Ball ball;
} PikaPhysics;

void player_init(Player *pl, int isPlayer2, int isComputer);
void player_init_new_round(Player *pl);

void ball_init(Ball *b);
void ball_init_new_round(Ball *b, int isPlayer2Serve);

void physics_init(PikaPhysics *p, int p1Computer, int p2Computer);

/*
 * Advance one frame of physics.  in[0] is input for player1, in[1] for
 * player2.  For computer-controlled players the input is overwritten by the
 * AI decision.  Returns 1 if the ball touched the ground this frame.
 */
int physics_step(PikaPhysics *p, PikaUserInput in[2]);

#endif /* PIKA_PHYSICS_H */
