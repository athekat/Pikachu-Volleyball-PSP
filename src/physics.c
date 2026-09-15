/*
 * physics.c - Faithful C port of src/resources/js/physics.js.
 *
 * Constant names and values are taken directly from the JS reference.
 * Functions are transcribed statement-for-statement from the JS; integer
 * division in C truncates toward zero, matching the JS "| 0" idiom used in
 * the reference for the values involved here.
 */
#include "physics.h"
#include "rand.h"

#define GROUND_WIDTH                 432
#define GROUND_HALF_WIDTH            216
#define PLAYER_LENGTH                 64
#define PLAYER_HALF_LENGTH            32
#define PLAYER_TOUCHING_GROUND_Y_COORD 244
#define BALL_RADIUS                   20
#define BALL_TOUCHING_GROUND_Y_COORD 252
#define NET_PILLAR_HALF_WIDTH         25
#define NET_PILLAR_TOP_TOP_Y_COORD   176
#define NET_PILLAR_TOP_BOTTOM_Y_COORD 192
#define INFINITE_LOOP_LIMIT         1000

#define ABS(x) ((x) < 0 ? -(x) : (x))

/* Forward declarations. */
static void process_player_movement(Player *player, PikaUserInput *in,
                                    Player *other, Ball *ball);
static void process_game_end_frame(Player *player);
static void process_collision_ball_player(Ball *ball, int playerX,
                                          PikaUserInput *in, int playerState);
static void calculate_expected_landing_point_x(Ball *ball);
static void let_computer_decide_input(Player *player, Ball *ball,
                                      Player *other, PikaUserInput *in);
static int decide_whether_input_power_hit(Player *player, Ball *ball,
                                          Player *other, PikaUserInput *in);
static int expected_landing_point_x_when_power_hit(int ux, int uy, Ball *ball);
static int is_collision_ball_player(Ball *ball, int px, int py);
static int process_collision_ball_world(Ball *ball);

/* ------------------------------------------------------------------ */
/* Construction / round init                                           */
/* ------------------------------------------------------------------ */

void player_init(Player *pl, int isPlayer2, int isComputer) {
    pl->isPlayer2 = isPlayer2;
    pl->isComputer = isComputer;
    player_init_new_round(pl);
    pl->divingDirection = 0;
    pl->lyingDownDurationLeft = -1;
    pl->isWinner = 0;
    pl->gameEnded = 0;
    pl->computerWhereToStandBy = 0;
    pl->snd_pipikachu = 0;
    pl->snd_pika = 0;
    pl->snd_chu = 0;
}

void player_init_new_round(Player *pl) {
    pl->x = 36;
    if (pl->isPlayer2) {
        pl->x = GROUND_WIDTH - 36;
    }
    pl->y = PLAYER_TOUCHING_GROUND_Y_COORD;
    pl->yVelocity = 0;
    pl->isCollisionWithBallHappened = 0;
    pl->state = 0;
    pl->frameNumber = 0;
    pl->normalStatusArmSwingDirection = 1;
    pl->delayBeforeNextFrame = 0;
    pl->computerBoldness = pika_rand() % 5;
}

void ball_init(Ball *b) {
    ball_init_new_round(b, 0);
    b->expectedLandingPointX = 0;
    b->rotation = 0;
    b->fineRotation = 0;
    b->punchEffectX = 0;
    b->punchEffectY = 0;
    b->previousX = 0;
    b->previousPreviousX = 0;
    b->previousY = 0;
    b->previousPreviousY = 0;
    b->snd_powerHit = 0;
    b->snd_ballTouchesGround = 0;
}

void ball_init_new_round(Ball *b, int isPlayer2Serve) {
    b->x = 56;
    if (isPlayer2Serve) {
        b->x = GROUND_WIDTH - 56;
    }
    b->y = 0;
    b->xVelocity = 0;
    b->yVelocity = 1;
    b->punchEffectRadius = 0;
    b->isPowerHit = 0;
}

void physics_init(PikaPhysics *p, int p1Computer, int p2Computer) {
    player_init(&p->player1, 0, p1Computer);
    player_init(&p->player2, 1, p2Computer);
    ball_init(&p->ball);
}

/* ------------------------------------------------------------------ */
/* FUN_00403dd0 - the physics engine                                   */
/* ------------------------------------------------------------------ */

int physics_step(PikaPhysics *p, PikaUserInput in[2]) {
    Player *player1 = &p->player1;
    Player *player2 = &p->player2;
    Ball *ball = &p->ball;

    int isBallTouchingGround = process_collision_ball_world(ball);

    for (int i = 0; i < 2; i++) {
        Player *player = (i == 0) ? player1 : player2;
        Player *other = (i == 0) ? player2 : player1;

        calculate_expected_landing_point_x(ball);
        process_player_movement(player, &in[i], other, ball);
    }

    for (int i = 0; i < 2; i++) {
        Player *player = (i == 0) ? player1 : player2;

        if (is_collision_ball_player(ball, player->x, player->y)) {
            if (!player->isCollisionWithBallHappened) {
                process_collision_ball_player(ball, player->x, &in[i],
                                              player->state);
                player->isCollisionWithBallHappened = 1;
            }
        } else {
            player->isCollisionWithBallHappened = 0;
        }
    }

    return isBallTouchingGround;
}

/* ------------------------------------------------------------------ */
/* FUN_00403070 - ball/player AABB overlap                             */
/* ------------------------------------------------------------------ */

static int is_collision_ball_player(Ball *ball, int px, int py) {
    int diff = ball->x - px;
    if (ABS(diff) <= PLAYER_HALF_LENGTH) {
        diff = ball->y - py;
        if (ABS(diff) <= PLAYER_HALF_LENGTH) {
            return 1;
        }
    }
    return 0;
}

/* ------------------------------------------------------------------ */
/* FUN_00402dc0 - ball vs world collision + position                   */
/* ------------------------------------------------------------------ */

static int process_collision_ball_world(Ball *ball) {
    ball->previousPreviousX = ball->previousX;
    ball->previousPreviousY = ball->previousY;
    ball->previousX = ball->x;
    ball->previousY = ball->y;

    int futureFineRotation = ball->fineRotation + (ball->xVelocity / 2);
    if (futureFineRotation < 0) {
        futureFineRotation += 50;
    } else if (futureFineRotation > 50) {
        futureFineRotation += -50;
    }
    ball->fineRotation = futureFineRotation;
    ball->rotation = ball->fineRotation / 10;

    int futureBallX = ball->x + ball->xVelocity;
    if (futureBallX < BALL_RADIUS || futureBallX > GROUND_WIDTH) {
        ball->xVelocity = -ball->xVelocity;
    }

    int futureBallY = ball->y + ball->yVelocity;
    if (futureBallY < 0) {
        ball->yVelocity = 1;
    }

    if (ABS(ball->x - GROUND_HALF_WIDTH) < NET_PILLAR_HALF_WIDTH &&
        ball->y > NET_PILLAR_TOP_TOP_Y_COORD) {
        if (ball->y <= NET_PILLAR_TOP_BOTTOM_Y_COORD) {
            if (ball->yVelocity > 0) {
                ball->yVelocity = -ball->yVelocity;
            }
        } else {
            if (ball->x < GROUND_HALF_WIDTH) {
                ball->xVelocity = -ABS(ball->xVelocity);
            } else {
                ball->xVelocity = ABS(ball->xVelocity);
            }
        }
    }

    futureBallY = ball->y + ball->yVelocity;
    if (futureBallY > BALL_TOUCHING_GROUND_Y_COORD) {
        ball->snd_ballTouchesGround = 1;
        ball->yVelocity = -ball->yVelocity;
        ball->punchEffectX = ball->x;
        ball->y = BALL_TOUCHING_GROUND_Y_COORD;
        ball->punchEffectRadius = BALL_RADIUS;
        ball->punchEffectY = BALL_TOUCHING_GROUND_Y_COORD + BALL_RADIUS;
        return 1;
    }
    ball->y = futureBallY;
    ball->x = ball->x + ball->xVelocity;
    ball->yVelocity += 1;

    return 0;
}

/* ------------------------------------------------------------------ */
/* FUN_00401fc0 - player movement                                      */
/* ------------------------------------------------------------------ */

static void process_player_movement(Player *player, PikaUserInput *in,
                                    Player *other, Ball *ball) {
    if (player->isComputer) {
        let_computer_decide_input(player, ball, other, in);
    }

    /* if player is lying down.. don't move */
    if (player->state == 4) {
        player->lyingDownDurationLeft += -1;
        if (player->lyingDownDurationLeft < -1) {
            player->state = 0;
        }
        return;
    }

    /* x-direction movement */
    int playerVelocityX = 0;
    if (player->state < 5) {
        if (player->state < 3) {
            playerVelocityX = in->xDirection * 6;
        } else {
            playerVelocityX = player->divingDirection * 8;
        }
    }

    int futurePlayerX = player->x + playerVelocityX;
    player->x = futurePlayerX;

    /* x-direction world boundary */
    if (!player->isPlayer2) {
        if (futurePlayerX < PLAYER_HALF_LENGTH) {
            player->x = PLAYER_HALF_LENGTH;
        } else if (futurePlayerX > GROUND_HALF_WIDTH - PLAYER_HALF_LENGTH) {
            player->x = GROUND_HALF_WIDTH - PLAYER_HALF_LENGTH;
        }
    } else {
        if (futurePlayerX < GROUND_HALF_WIDTH + PLAYER_HALF_LENGTH) {
            player->x = GROUND_HALF_WIDTH + PLAYER_HALF_LENGTH;
        } else if (futurePlayerX > GROUND_WIDTH - PLAYER_HALF_LENGTH) {
            player->x = GROUND_WIDTH - PLAYER_HALF_LENGTH;
        }
    }

    /* jump */
    if (player->state < 3 &&
        in->yDirection == -1 &&
        player->y == PLAYER_TOUCHING_GROUND_Y_COORD) {
        player->yVelocity = -16;
        player->state = 1;
        player->frameNumber = 0;
        player->snd_chu = 1;
    }

    /* gravity */
    int futurePlayerY = player->y + player->yVelocity;
    player->y = futurePlayerY;
    if (futurePlayerY < PLAYER_TOUCHING_GROUND_Y_COORD) {
        player->yVelocity += 1;
    } else if (futurePlayerY > PLAYER_TOUCHING_GROUND_Y_COORD) {
        player->yVelocity = 0;
        player->y = PLAYER_TOUCHING_GROUND_Y_COORD;
        player->frameNumber = 0;
        if (player->state == 3) {
            player->state = 4;
            player->frameNumber = 0;
            player->lyingDownDurationLeft = 3;
        } else {
            player->state = 0;
        }
    }

    if (in->powerHit == 1) {
        if (player->state == 1) {
            /* jumping -> power hit */
            player->delayBeforeNextFrame = 5;
            player->frameNumber = 0;
            player->state = 2;
            player->snd_pika = 1;
        } else if (player->state == 0 && in->xDirection != 0) {
            /* diving */
            player->state = 3;
            player->frameNumber = 0;
            player->divingDirection = in->xDirection;
            player->yVelocity = -5;
            player->snd_chu = 1;
        }
    }

    if (player->state == 1) {
        player->frameNumber = (player->frameNumber + 1) % 3;
    } else if (player->state == 2) {
        if (player->delayBeforeNextFrame < 1) {
            player->frameNumber += 1;
            if (player->frameNumber > 4) {
                player->frameNumber = 0;
                player->state = 1;
            }
        } else {
            player->delayBeforeNextFrame -= 1;
        }
    } else if (player->state == 0) {
        player->delayBeforeNextFrame += 1;
        if (player->delayBeforeNextFrame > 3) {
            player->delayBeforeNextFrame = 0;
            int futureFrameNumber =
                player->frameNumber + player->normalStatusArmSwingDirection;
            if (futureFrameNumber < 0 || futureFrameNumber > 4) {
                player->normalStatusArmSwingDirection =
                    -player->normalStatusArmSwingDirection;
            }
            player->frameNumber =
                player->frameNumber + player->normalStatusArmSwingDirection;
        }
    }

    if (player->gameEnded) {
        if (player->state == 0) {
            if (player->isWinner) {
                player->state = 5;
                player->snd_pipikachu = 1;
            } else {
                player->state = 6;
            }
            player->delayBeforeNextFrame = 0;
            player->frameNumber = 0;
        }
        process_game_end_frame(player);
    }
}

/* ------------------------------------------------------------------ */
/* FUN_004025e0 - game-end win/lose animation frame                    */
/* ------------------------------------------------------------------ */

static void process_game_end_frame(Player *player) {
    if (player->gameEnded && player->frameNumber < 4) {
        player->delayBeforeNextFrame += 1;
        if (player->delayBeforeNextFrame > 4) {
            player->delayBeforeNextFrame = 0;
            player->frameNumber += 1;
        }
    }
}

/* ------------------------------------------------------------------ */
/* FUN_004030a0 - ball/player collision (velocity + landing point)     */
/* ------------------------------------------------------------------ */

static void process_collision_ball_player(Ball *ball, int playerX,
                                          PikaUserInput *in,
                                          int playerState) {
    if (ball->x < playerX) {
        ball->xVelocity = -(ABS(ball->x - playerX) / 3);
    } else if (ball->x > playerX) {
        ball->xVelocity = (ABS(ball->x - playerX) / 3);
    }

    if (ball->xVelocity == 0) {
        ball->xVelocity = (pika_rand() % 3) - 1;
    }

    int ballAbsYVelocity = ABS(ball->yVelocity);
    ball->yVelocity = -ballAbsYVelocity;

    if (ballAbsYVelocity < 15) {
        ball->yVelocity = -15;
    }

    if (playerState == 2) {
        /* jumping and power hitting */
        if (ball->x < GROUND_HALF_WIDTH) {
            ball->xVelocity = (ABS(in->xDirection) + 1) * 10;
        } else {
            ball->xVelocity = -(ABS(in->xDirection) + 1) * 10;
        }
        ball->punchEffectX = ball->x;
        ball->punchEffectY = ball->y;

        ball->yVelocity = ABS(ball->yVelocity) * in->yDirection * 2;
        ball->punchEffectRadius = BALL_RADIUS;
        ball->snd_powerHit = 1;

        ball->isPowerHit = 1;
    } else {
        ball->isPowerHit = 0;
    }

    calculate_expected_landing_point_x(ball);
}

/* ------------------------------------------------------------------ */
/* FUN_004031b0 - expected landing point X                             */
/* ------------------------------------------------------------------ */

static void calculate_expected_landing_point_x(Ball *ball) {
    int x = ball->x;
    int y = ball->y;
    int xv = ball->xVelocity;
    int yv = ball->yVelocity;
    int loopCounter = 0;

    for (;;) {
        loopCounter++;

        int futureX = xv + x;
        if (futureX < BALL_RADIUS || futureX > GROUND_WIDTH) {
            xv = -xv;
        }
        if (y + yv < 0) {
            yv = 1;
        }

        if (ABS(x - GROUND_HALF_WIDTH) < NET_PILLAR_HALF_WIDTH &&
            y > NET_PILLAR_TOP_TOP_Y_COORD) {
            /* Faithful to the original: "<" (not "<=") here. */
            if (y < NET_PILLAR_TOP_BOTTOM_Y_COORD) {
                if (yv > 0) {
                    yv = -yv;
                }
            } else {
                if (x < GROUND_HALF_WIDTH) {
                    xv = -ABS(xv);
                } else {
                    xv = ABS(xv);
                }
            }
        }

        y = y + yv;
        if (y > BALL_TOUCHING_GROUND_Y_COORD ||
            loopCounter >= INFINITE_LOOP_LIMIT) {
            break;
        }
        x = x + xv;
        yv += 1;
    }

    ball->expectedLandingPointX = x;
}

/* ------------------------------------------------------------------ */
/* FUN_00402360 - computer AI                                          */
/* ------------------------------------------------------------------ */

static void let_computer_decide_input(Player *player, Ball *ball,
                                      Player *other, PikaUserInput *in) {
    in->xDirection = 0;
    in->yDirection = 0;
    in->powerHit = 0;

    int virtualExpectedLandingPointX = ball->expectedLandingPointX;
    int leftBoundary = player->isPlayer2 * GROUND_HALF_WIDTH;

    if (ABS(ball->x - player->x) > 100 &&
        ABS(ball->xVelocity) < player->computerBoldness + 5) {
        if ((ball->expectedLandingPointX <= leftBoundary ||
             ball->expectedLandingPointX >=
                 player->isPlayer2 * GROUND_WIDTH + GROUND_HALF_WIDTH) &&
            player->computerWhereToStandBy == 0) {
            virtualExpectedLandingPointX =
                leftBoundary + (GROUND_HALF_WIDTH / 2);
        }
    }

    if (ABS(virtualExpectedLandingPointX - player->x) >
        player->computerBoldness + 8) {
        if (player->x < virtualExpectedLandingPointX) {
            in->xDirection = 1;
        } else {
            in->xDirection = -1;
        }
    } else if (pika_rand() % 20 == 0) {
        player->computerWhereToStandBy = pika_rand() % 2;
    }

    if (player->state == 0) {
        if (ABS(ball->xVelocity) < player->computerBoldness + 3 &&
            ABS(ball->x - player->x) < PLAYER_HALF_LENGTH &&
            ball->y > -36 &&
            ball->y < 10 * player->computerBoldness + 84 &&
            ball->yVelocity > 0) {
            in->yDirection = -1;
        }

        int lBoundary = player->isPlayer2 * GROUND_HALF_WIDTH;
        int rBoundary = (player->isPlayer2 + 1) * GROUND_HALF_WIDTH;
        if (ball->expectedLandingPointX > lBoundary &&
            ball->expectedLandingPointX < rBoundary &&
            ABS(ball->x - player->x) >
                player->computerBoldness * 5 + PLAYER_LENGTH &&
            ball->x > lBoundary &&
            ball->x < rBoundary &&
            ball->y > 174) {
            /* decide to dive */
            in->powerHit = 1;
            if (player->x < ball->x) {
                in->xDirection = 1;
            } else {
                in->xDirection = -1;
            }
        }
    } else if (player->state == 1 || player->state == 2) {
        if (ABS(ball->x - player->x) > 8) {
            if (player->x < ball->x) {
                in->xDirection = 1;
            } else {
                in->xDirection = -1;
            }
        }
        if (ABS(ball->x - player->x) < 48 &&
            ABS(ball->y - player->y) < 48) {
            int willInputPowerHit =
                decide_whether_input_power_hit(player, ball, other, in);
            if (willInputPowerHit) {
                in->powerHit = 1;
                if (ABS(other->x - player->x) < 80 &&
                    in->yDirection != -1) {
                    in->yDirection = -1;
                }
            }
        }
    }
}

/* ------------------------------------------------------------------ */
/* FUN_00402630 - power-hit decision                                   */
/* ------------------------------------------------------------------ */

static int decide_whether_input_power_hit(Player *player, Ball *ball,
                                          Player *other, PikaUserInput *in) {
    if (pika_rand() % 2 == 0) {
        for (int xDirection = 1; xDirection > -1; xDirection--) {
            for (int yDirection = -1; yDirection < 2; yDirection++) {
                int ep = expected_landing_point_x_when_power_hit(
                    xDirection, yDirection, ball);
                if ((ep <= player->isPlayer2 * GROUND_HALF_WIDTH ||
                     ep >= player->isPlayer2 * GROUND_WIDTH +
                               GROUND_HALF_WIDTH) &&
                    ABS(ep - other->x) > PLAYER_LENGTH) {
                    in->xDirection = xDirection;
                    in->yDirection = yDirection;
                    return 1;
                }
            }
        }
    } else {
        for (int xDirection = 1; xDirection > -1; xDirection--) {
            for (int yDirection = 1; yDirection > -2; yDirection--) {
                int ep = expected_landing_point_x_when_power_hit(
                    xDirection, yDirection, ball);
                if ((ep <= player->isPlayer2 * GROUND_HALF_WIDTH ||
                     ep >= player->isPlayer2 * GROUND_WIDTH +
                               GROUND_HALF_WIDTH) &&
                    ABS(ep - other->x) > PLAYER_LENGTH) {
                    in->xDirection = xDirection;
                    in->yDirection = yDirection;
                    return 1;
                }
            }
        }
    }
    return 0;
}

/* ------------------------------------------------------------------ */
/* FUN_00402870 - landing point if power hit                           */
/* ------------------------------------------------------------------ */

static int expected_landing_point_x_when_power_hit(int ux, int uy, Ball *ball) {
    int x = ball->x;
    int y = ball->y;
    int xv = ball->xVelocity;
    int yv = ball->yVelocity;

    if (x < GROUND_HALF_WIDTH) {
        xv = (ABS(ux) + 1) * 10;
    } else {
        xv = -(ABS(ux) + 1) * 10;
    }
    yv = ABS(yv) * uy * 2;

    int loopCounter = 0;
    for (;;) {
        loopCounter++;

        int futureX = x + xv;
        if (futureX < BALL_RADIUS || futureX > GROUND_WIDTH) {
            xv = -xv;
        }
        if (y + yv < 0) {
            yv = 1;
        }
        if (ABS(x - GROUND_HALF_WIDTH) < NET_PILLAR_HALF_WIDTH &&
            y > NET_PILLAR_TOP_TOP_Y_COORD) {
            /* Faithful to the original: the computer does not anticipate
             * the net-pillar bounce here. */
            if (yv > 0) {
                yv = -yv;
            }
        }
        y = y + yv;
        if (y > BALL_TOUCHING_GROUND_Y_COORD ||
            loopCounter >= INFINITE_LOOP_LIMIT) {
            return x;
        }
        x = x + xv;
        yv += 1;
    }
}
