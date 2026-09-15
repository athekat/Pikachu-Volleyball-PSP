/*
 * render.c - GU-based 2D rendering (the "View").
 *
 * Faithful port of the draw logic in src/resources/js/view.js and
 * src/resources/js/cloud_and_wave.js.
 *
 * The scene renders in the original 432x304 logical space and is scaled
 * (aspect-preserving, letterboxed) to the 480x272 PSP framebuffer.
 *
 * Atlas: the original 476x885 sheet was repacked into two 512x512 tiles
 * (PSP texture size limit) and stored as GU_PSM_5551 (1-bit alpha, which
 * matches the source art's hard 0/255 transparency exactly).  Frame
 * coordinates live in src/atlas.h.
 */
#include <pspkernel.h>
#include <pspdisplay.h>
#include <pspgu.h>
#include <pspge.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "render.h"
#include "atlas.h"
#include "rand.h"

/* Raw atlas tile data (embedded via bin2o; symbols are <name>_start). */
extern unsigned char sprite_sheet_0_start[];
extern unsigned char sprite_sheet_1_start[];

#define SCR_W 480
#define SCR_H 272
#define BUF_W 512

/* VRAM offsets (PSP-1000 has 2MB = 0x200000). */
#define VRAM_DRAW  0x000000                    /* draw buffer           */
#define VRAM_DISP  0x044000                    /* display buffer        */
#define VRAM_TILE0 0x088000                    /* atlas tile 0 (512x512) */
#define VRAM_TILE1 0x108000                    /* atlas tile 1          */

#define GAME_W 432
#define GAME_H 304
#define NUM_CLOUDS 10

typedef struct {
    float u, v;
    unsigned int color;
    float x, y, z;
} Vertex;

typedef struct {
    int topLeftPointX, topLeftPointY;
    int topLeftPointXVelocity;
    int sizeDiffTurnNumber;
} Cloud;

typedef struct {
    int verticalCoord, verticalCoordVelocity;
    int yCoords[GAME_W / 16];
} Wave;

/* ------------------------------------------------------------------ */
/* State                                                              */
/* ------------------------------------------------------------------ */
static void *g_list = NULL;
static void *g_tile_ptr[ATLAS_NUM_TILES];
static int g_cur_tile = -1;

static float g_scale = 1.0f;
static float g_offx = 0.0f;
static float g_offy = 0.0f;
static float g_blackAlpha = 0.0f;

/* Clipping region (the letterboxed playfield), in screen pixel coords. */
static int g_scissor_x = 0;
static int g_scissor_y = 0;
static int g_scissor_w = 0;
static int g_scissor_h = 0;

static Cloud g_clouds[NUM_CLOUDS];
static Wave g_wave;

/* intro */
static float g_markAlpha = 0.0f;

/* menu */
static float g_sachisoftAlpha = 0.0f;
static float g_sittingAlpha = 0.0f;
static int g_sittingDisp = 0;

/* ------------------------------------------------------------------ */
/* Low-level GU helpers                                               */
/* ------------------------------------------------------------------ */

static void gu_bind_tile(int tile) {
    if (tile == g_cur_tile) {
        return;
    }
    g_cur_tile = tile;
    /* tbw is the texture buffer width in PIXELS (sceGuTexImage converts to
     * the hardware's 16-byte block stride internally). */
    sceGuTexImage(0, ATLAS_TILE_SIZE, ATLAS_TILE_SIZE, ATLAS_TILE_SIZE,
                  g_tile_ptr[tile]);
}

/* Draw a textured quad from (u0,v0)-(u1,v1) to screen (x0,y0)-(x1,y1). */
static void gu_sprite(int tile, float u0, float v0, float u1, float v1,
                      float x0, float y0, float x1, float y1, int alpha) {
    gu_bind_tile(tile);
    Vertex *v = (Vertex *)sceGuGetMemory(4 * sizeof(Vertex));
    unsigned int col = 0x00FFFFFF | ((unsigned int)(alpha & 0xFF) << 24);
    /* Half-texel inset: sample texel CENTRES so NEAREST filtering never rounds
     * onto the neighbouring sprite in the atlas (shows as a 1px line on real
     * hardware, e.g. the net pole sitting flush against `black` in tile 1). */
    float du = (u1 > u0) ? 0.5f : -0.5f;
    float uu0 = u0 + du, uu1 = u1 - du;
    float vv0 = v0 + 0.5f, vv1 = v1 - 0.5f;
    v[0].u = uu0; v[0].v = vv0; v[0].color = col; v[0].x = x0; v[0].y = y0; v[0].z = 0.0f;
    v[1].u = uu1; v[1].v = vv0; v[1].color = col; v[1].x = x1; v[1].y = y0; v[1].z = 0.0f;
    v[2].u = uu0; v[2].v = vv1; v[2].color = col; v[2].x = x0; v[2].y = y1; v[2].z = 0.0f;
    v[3].u = uu1; v[3].v = vv1; v[3].color = col; v[3].x = x1; v[3].y = y1; v[3].z = 0.0f;
    sceGuDrawArray(GU_TRIANGLE_STRIP,
                   GU_TEXTURE_32BITF | GU_COLOR_8888 | GU_VERTEX_32BITF |
                       GU_TRANSFORM_2D,
                   4, 0, v);
}

static inline float sx(float gx) { return g_offx + gx * g_scale; }
static inline float sy(float gy) { return g_offy + gy * g_scale; }

/* Draw frame `tex` into game-space dest rect (top-left + size). */
static void draw_frame(int tex, float gx, float gy, float gw, float gh,
                       int alpha, int flip) {
    if (gw <= 0.001f || gh <= 0.001f) {
        return; /* degenerate (zero-size) sprite */
    }
    const AtlasFrame *f = &g_atlas_frames[tex];
    float u0 = (float)f->x, v0 = (float)f->y;
    float u1 = (float)(f->x + f->w), v1 = (float)(f->y + f->h);
    if (flip) {
        float t = u0; u0 = u1; u1 = t;
    }
    gu_sprite(f->tile, u0, v0, u1, v1, sx(gx), sy(gy), sx(gx + gw), sy(gy + gh), alpha);
}

/* top-left, natural size */
static void draw_xy(int tex, float gx, float gy) {
    const AtlasFrame *f = &g_atlas_frames[tex];
    draw_frame(tex, gx, gy, (float)f->w, (float)f->h, 255, 0);
}
/* centered, natural size */
static void draw_center(int tex, float gcx, float gcy, int alpha) {
    const AtlasFrame *f = &g_atlas_frames[tex];
    draw_frame(tex, gcx - f->w / 2.0f, gcy - f->h / 2.0f, (float)f->w, (float)f->h, alpha, 0);
}
/* centered, natural size, horizontally flipped */
static void draw_center_flip(int tex, float gcx, float gcy, int flip, int alpha) {
    const AtlasFrame *f = &g_atlas_frames[tex];
    draw_frame(tex, gcx - f->w / 2.0f, gcy - f->h / 2.0f, (float)f->w, (float)f->h, alpha, flip);
}
/* top-left, scaled to gw x gh */
static void draw_rect(int tex, float gx, float gy, float gw, float gh, int alpha) {
    draw_frame(tex, gx, gy, gw, gh, alpha, 0);
}

/* Texture id helpers (see atlas.h enum ordering). */
static int tex_ball(int n) { return TEX_BALL_BALL_0 + n; }        /* 0..4 */
static int tex_number(int n) { return TEX_NUMBER_NUMBER_0 + n; }  /* 0..9 */

/* Maps (state, frameNumber) to the 28-frame player sheet, matching
 * getFrameNumberForPlayerAnimatedSprite in view.js. */
static int tex_pikachu(int state, int frameNumber) {
    int idx;
    if (state < 4) {
        idx = 5 * state + frameNumber;
    } else if (state == 4) {
        idx = 17;
    } else {
        idx = 18 + 5 * (state - 5) + frameNumber;
    }
    return TEX_PIKACHU_PIKACHU_0_0 + idx;
}

/* ------------------------------------------------------------------ */
/* Background (FUN_00403b50 region: static tiles)                      */
/* ------------------------------------------------------------------ */

static void draw_background(void) {
    /* sky: 12 rows x 27 cols of 16x16 */
    for (int j = 0; j < 12; j++) {
        for (int i = 0; i < GAME_W / 16; i++) {
            draw_xy(TEX_OBJECTS_SKY_BLUE, (float)(16 * i), (float)(16 * j));
        }
    }
    /* mountain */
    draw_xy(TEX_OBJECTS_MOUNTAIN, 0.0f, 188.0f);
    /* ground_red */
    for (int i = 0; i < GAME_W / 16; i++) {
        draw_xy(TEX_OBJECTS_GROUND_RED, (float)(16 * i), 248.0f);
    }
    /* ground_line (skip both ends) */
    for (int i = 1; i < GAME_W / 16 - 1; i++) {
        draw_xy(TEX_OBJECTS_GROUND_LINE, (float)(16 * i), 264.0f);
    }
    draw_xy(TEX_OBJECTS_GROUND_LINE_LEFTMOST, 0.0f, 264.0f);
    draw_xy(TEX_OBJECTS_GROUND_LINE_RIGHTMOST, (float)(GAME_W - 16), 264.0f);
    /* ground_yellow: 2 rows */
    for (int j = 0; j < 2; j++) {
        for (int i = 0; i < GAME_W / 16; i++) {
            draw_xy(TEX_OBJECTS_GROUND_YELLOW, (float)(16 * i), (float)(280 + 16 * j));
        }
    }
    /* net pillar */
    draw_xy(TEX_OBJECTS_NET_PILLAR_TOP, 213.0f, 176.0f);
    for (int j = 0; j < 12; j++) {
        draw_xy(TEX_OBJECTS_NET_PILLAR, 213.0f, (float)(184 + 8 * j));
    }
}

/* ------------------------------------------------------------------ */
/* Clouds and wave (FUN_00404770)                                     */
/* ------------------------------------------------------------------ */

static void cloud_wave_engine(void) {
    for (int i = 0; i < NUM_CLOUDS; i++) {
        Cloud *c = &g_clouds[i];
        c->topLeftPointX += c->topLeftPointXVelocity;
        if (c->topLeftPointX > GAME_W) {
            c->topLeftPointX = -68;
            c->topLeftPointY = pika_rand() % 152;
            c->topLeftPointXVelocity = 1 + pika_rand() % 2;
        }
        c->sizeDiffTurnNumber = (c->sizeDiffTurnNumber + 1) % 11;
    }

    g_wave.verticalCoord += g_wave.verticalCoordVelocity;
    if (g_wave.verticalCoord > 32) {
        g_wave.verticalCoord = 32;
        g_wave.verticalCoordVelocity = -1;
    } else if (g_wave.verticalCoord < 0 && g_wave.verticalCoordVelocity < 0) {
        g_wave.verticalCoordVelocity = 2;
        g_wave.verticalCoord = -(pika_rand() % 40);
    }

    for (int i = 0; i < GAME_W / 16; i++) {
        g_wave.yCoords[i] = 314 - g_wave.verticalCoord + pika_rand() % 3;
    }
}

static void draw_clouds_wave(void) {
    cloud_wave_engine();

    for (int i = 0; i < NUM_CLOUDS; i++) {
        const Cloud *c = &g_clouds[i];
        int sizeDiff = 5 - (c->sizeDiffTurnNumber > 5 ? c->sizeDiffTurnNumber - 5 : 5 - c->sizeDiffTurnNumber);
        float x = (float)(c->topLeftPointX - sizeDiff);
        float y = (float)(c->topLeftPointY - sizeDiff);
        float w = (float)(48 + 2 * sizeDiff);
        float h = (float)(24 + 2 * sizeDiff);
        draw_rect(TEX_OBJECTS_CLOUD, x, y, w, h, 255);
    }

    for (int i = 0; i < GAME_W / 16; i++) {
        draw_xy(TEX_OBJECTS_WAVE, (float)(16 * i), (float)g_wave.yCoords[i]);
    }
}

/* ------------------------------------------------------------------ */
/* Intro                                                              */
/* ------------------------------------------------------------------ */

void render_draw_intro_mark(int frameCounter) {
    if (frameCounter == 0) {
        g_markAlpha = 0.0f;
        return;
    }
    if (frameCounter < 100) {
        g_markAlpha += 1.0f / 25.0f;
        if (g_markAlpha > 1.0f) g_markAlpha = 1.0f;
    } else {
        g_markAlpha -= 1.0f / 25.0f;
        if (g_markAlpha < 0.0f) g_markAlpha = 0.0f;
    }
    draw_center(TEX_MESSAGES_JA_MARK, 216.0f, 152.0f, (int)(g_markAlpha * 255.0f));
}

/* ------------------------------------------------------------------ */
/* Menu (FUN_004059f0 / 00405b70 / 00405d50 / 00405ca0 / 00405ec0)     */
/* ------------------------------------------------------------------ */

static void draw_fight_message(int fc) {
    static const int sizeArray[9] = {20, 22, 25, 27, 30, 27, 25, 22, 20};
    const AtlasFrame *f = &g_atlas_frames[TEX_MESSAGES_JA_FIGHT];
    float w = (float)f->w, h = (float)f->h;
    int halfW, halfH;
    if (fc < 30) {
        halfW = (int)((int)((float)fc * w / 30.0f) / 2);
        halfH = (int)((int)((float)fc * h / 30.0f) / 2);
    } else {
        int idx = (fc + 1) % 9;
        halfW = (int)((int)((float)sizeArray[idx] * w / 30.0f) / 2);
        halfH = (int)((int)((float)sizeArray[idx] * h / 30.0f) / 2);
    }
    draw_rect(TEX_MESSAGES_JA_FIGHT, 100.0f - halfW, 70.0f - halfH,
              (float)(halfW * 2), (float)(halfH * 2), 255);
}

static void draw_sachisoft(int fc) {
    if (fc == 0) g_sachisoftAlpha = 0.0f;
    g_sachisoftAlpha += 0.04f;
    if (g_sachisoftAlpha > 1.0f) g_sachisoftAlpha = 1.0f;
    if (fc > 70) g_sachisoftAlpha = 1.0f;
    const AtlasFrame *f = &g_atlas_frames[TEX_MESSAGES_COMMON_SACHISOFT];
    draw_frame(TEX_MESSAGES_COMMON_SACHISOFT, 216.0f - f->w / 2.0f, 264.0f,
               (float)f->w, (float)f->h, (int)(g_sachisoftAlpha * 255.0f), 0);
}

static void draw_sitting_pikachu_tiles(int fc) {
    const AtlasFrame *f = &g_atlas_frames[TEX_SITTING_PIKACHU];
    int w = f->w, h = f->h;
    g_sittingDisp = (g_sittingDisp + 2) % h;

    if (fc == 0) g_sittingAlpha = 0.0f;
    if (fc > 30) {
        g_sittingAlpha += 0.04f;
        if (g_sittingAlpha > 1.0f) g_sittingAlpha = 1.0f;
    }
    if (fc > 70) g_sittingAlpha = 1.0f;

    int rows = GAME_H / h + 2;
    int cols = GAME_W / w + 2;
    for (int j = 0; j < rows; j++) {
        for (int i = 0; i < cols; i++) {
            draw_frame(TEX_SITTING_PIKACHU, (float)(w * i - g_sittingDisp),
                       (float)(h * j - g_sittingDisp), (float)w, (float)h,
                       (int)(g_sittingAlpha * 255.0f), 0);
        }
    }
}

static void draw_pikachu_volleyball_message(int fc) {
    if (fc <= 30) return;
    const AtlasFrame *f = &g_atlas_frames[TEX_MESSAGES_JA_PIKACHU_VOLLEYBALL];
    float x = 140.0f, w = (float)f->w, h = (float)f->h;
    if (fc <= 44) {
        int xDiff = 195 - 15 * (fc - 30);
        x = 140.0f + xDiff;
    } else if (fc <= 55) {
        w = 200.0f - 15.0f * (fc - 44);
    } else if (fc <= 71) {
        w = 40.0f + 15.0f * (fc - 55);
    }
    draw_frame(TEX_MESSAGES_JA_PIKACHU_VOLLEYBALL, x, 80.0f, w, h, 255, 0);
}

static void draw_pokemon_message(int fc) {
    if (fc <= 71) return;
    draw_xy(TEX_MESSAGES_JA_POKEMON, 170.0f, 40.0f);
}

static void draw_with_who_messages(int fc) {
    if (fc <= 70) return;
    /* Only "with computer" is offered: 1v1 on a single PSP is not supported. */
    const AtlasFrame *f = &g_atlas_frames[TEX_MESSAGES_JA_WITH_COMPUTER];
    draw_frame(TEX_MESSAGES_JA_WITH_COMPUTER, 216.0f - f->w / 2.0f, 184.0f,
               (float)f->w, (float)f->h, 255, 0);
}

void render_draw_menu(int frameCounter) {
    /* z-order (back to front) matches view.js MenuView: background tiles
     * first, then text messages, with fight! on top. */
    draw_sitting_pikachu_tiles(frameCounter);
    draw_pokemon_message(frameCounter);
    draw_pikachu_volleyball_message(frameCounter);
    draw_with_who_messages(frameCounter);
    draw_sachisoft(frameCounter);
    draw_fight_message(frameCounter);
}

/* ------------------------------------------------------------------ */
/* Game scene                                                         */
/* ------------------------------------------------------------------ */

static void render_draw_players_ball(PikaPhysics *p) {
    Player *p1 = &p->player1;
    Player *p2 = &p->player2;
    Ball *ball = &p->ball;

    /* shadows (behind players/ball) */
    draw_center(TEX_OBJECTS_SHADOW, (float)p1->x, 273.0f, 255);
    draw_center(TEX_OBJECTS_SHADOW, (float)p2->x, 273.0f, 255);
    draw_center(TEX_OBJECTS_SHADOW, (float)ball->x, 273.0f, 255);

    /* player 1 (faces right by default) */
    int flip1 = 0;
    if (p1->state == 3 || p1->state == 4) {
        flip1 = (p1->divingDirection == -1) ? 1 : 0;
    }
    draw_center_flip(tex_pikachu(p1->state, p1->frameNumber),
                     (float)p1->x, (float)p1->y, flip1, 255);

    /* player 2 (faces left by default) */
    int flip2 = 1;
    if (p2->state == 3 || p2->state == 4) {
        flip2 = (p2->divingDirection == 1) ? 0 : 1;
    }
    draw_center_flip(tex_pikachu(p2->state, p2->frameNumber),
                     (float)p2->x, (float)p2->y, flip2, 255);

    /* ball trail (power hit) */
    if (ball->isPowerHit) {
        draw_center(TEX_BALL_BALL_HYPER, (float)ball->previousX, (float)ball->previousY, 255);
        draw_center(TEX_BALL_BALL_TRAIL, (float)ball->previousPreviousX, (float)ball->previousPreviousY, 255);
    }

    /* ball (rotation 5 = hyper ball glitch) */
    int ballTex = (ball->rotation == 5) ? TEX_BALL_BALL_HYPER : tex_ball(ball->rotation);
    draw_center(ballTex, (float)ball->x, (float)ball->y, 255);

    /* punch effect (radius decays in the view, as in the original) */
    if (ball->punchEffectRadius > 0) {
        ball->punchEffectRadius -= 2;
        float r = (float)ball->punchEffectRadius;
        if (r > 0.0f) {
            draw_frame(TEX_BALL_BALL_PUNCH, ball->punchEffectX - r,
                       ball->punchEffectY - r, 2.0f * r, 2.0f * r, 255, 0);
        }
    }
}

static void render_draw_scores(const int scores[2]) {
    /* board i: tens at (boardX, 10), units at (boardX+32, 10) */
    for (int i = 0; i < 2; i++) {
        float bx = (i == 0) ? 14.0f : (float)(GAME_W - 32 - 32 - 14);
        int score = scores[i];
        draw_xy(tex_number(score % 10), bx + 32.0f, 10.0f);
        if (score >= 10) {
            draw_xy(tex_number((score / 10) % 10), bx, 10.0f);
        }
    }
}

/* ------------------------------------------------------------------ */
/* Messages                                                           */
/* ------------------------------------------------------------------ */

void render_draw_game_start_message(int fc, int frameTotal) {
    if (fc == 0) {
        /* visible from frame 0 */
    } else if (fc >= frameTotal - 1) {
        return; /* hidden */
    }
    const AtlasFrame *f = &g_atlas_frames[TEX_MESSAGES_JA_GAME_START];
    float w = (float)f->w, h = (float)f->h;
    int halfW = (int)((w * fc) / 50.0f);
    int halfH = (int)((h * fc) / 50.0f);
    draw_frame(TEX_MESSAGES_JA_GAME_START, 216.0f - halfW, 50.0f + 2.0f * halfH,
               (float)(2 * halfW), (float)(2 * halfH), 255, 0);
}

static int g_readyVisible = 0;

void render_draw_ready_message(int on) {
    g_readyVisible = on;
}

void render_toggle_ready_message(void) {
    g_readyVisible = !g_readyVisible;
}

void render_draw_game_end_message(int fc) {
    const AtlasFrame *f = &g_atlas_frames[TEX_MESSAGES_COMMON_GAME_END];
    float w = (float)f->w, h = (float)f->h;
    float x, y, dw, dh;
    if (fc < 50) {
        int halfWInc = 2 * (int)(((50 - fc) * w) / 50.0f);
        int halfHInc = 2 * (int)(((50 - fc) * h) / 50.0f);
        x = 216.0f - w / 2.0f - halfWInc;
        y = 50.0f - halfHInc;
        dw = w + 2.0f * halfWInc;
        dh = h + 2.0f * halfHInc;
    } else {
        x = 216.0f - w / 2.0f;
        y = 50.0f;
        dw = w;
        dh = h;
    }
    draw_frame(TEX_MESSAGES_COMMON_GAME_END, x, y, dw, dh, 255, 0);
}

/* ------------------------------------------------------------------ */
/* Fade                                                               */
/* ------------------------------------------------------------------ */

void render_set_black_alpha(float alpha) {
    g_blackAlpha = alpha;
}

void render_change_black_alpha(float delta) {
    if (delta >= 0.0f) {
        g_blackAlpha += delta;
        if (g_blackAlpha > 1.0f) g_blackAlpha = 1.0f;
    } else {
        g_blackAlpha += delta;
        if (g_blackAlpha < 0.0f) g_blackAlpha = 0.0f;
    }
}

/* ------------------------------------------------------------------ */
/* Frame lifecycle                                                    */
/* ------------------------------------------------------------------ */

/* Draw the full game scene in z-order (background -> clouds/wave ->
 * shadows/players/ball -> punch -> scores).  Call once per frame in the
 * game states; messages are drawn afterwards by the controller. */
void render_game_scene(PikaPhysics *p, const int scores[2]) {
    draw_background();
    draw_clouds_wave();
    render_draw_players_ball(p);
    render_draw_scores(scores);
    if (g_readyVisible) {
        draw_xy(TEX_MESSAGES_COMMON_READY, 176.0f, 38.0f);
    }
}

void render_init(void) {
    g_list = malloc(0x40000); /* 256 KB display list */
    if (!g_list) {
        /* cannot proceed; note: display list is required */
        sceKernelExitGame();
    }

    sceGuInit();
    sceGuStart(GU_DIRECT, g_list);
    sceGuDrawBuffer(GU_PSM_5551, (void *)VRAM_DRAW, BUF_W);
    sceGuDispBuffer(SCR_W, SCR_H, (void *)VRAM_DISP, BUF_W);
    sceGuOffset(2048 - SCR_W / 2, 2048 - SCR_H / 2);
    sceGuViewport(2048, 2048, SCR_W, SCR_H);
    sceGuDepthRange(65535, 0);
    sceGuScissor(0, 0, SCR_W, SCR_H);
    sceGuEnable(GU_SCISSOR_TEST);
    sceGuEnable(GU_TEXTURE_2D);
    sceGuDisable(GU_DEPTH_TEST);
    sceGuDisable(GU_ALPHA_TEST);
    sceGuDepthFunc(GU_ALWAYS);
    sceGuClearColor(0xFF000000);
    sceGuClearDepth(0);
    sceGuBlendFunc(GU_ADD, GU_SRC_ALPHA, GU_ONE_MINUS_SRC_ALPHA, 0, 0);
    sceGuEnable(GU_BLEND);
    sceGuTexMode(GU_PSM_5551, 0, 0, GU_FALSE);
    sceGuTexFunc(GU_TFX_MODULATE, GU_TCC_RGBA);
    sceGuTexWrap(GU_CLAMP, GU_CLAMP);
    sceGuTexFilter(GU_NEAREST, GU_NEAREST);
    sceGuTexScale(1.0f / ATLAS_TILE_SIZE, 1.0f / ATLAS_TILE_SIZE);
    sceGuTexOffset(0.0f, 0.0f);
    sceGuFinish();
    sceGuSync(0, 0);

    /* Upload atlas tiles into VRAM (linear, unswizzled). */
    void *vram = sceGeEdramGetAddr();
    memcpy((char *)vram + VRAM_TILE0, sprite_sheet_0_start,
           ATLAS_TILE_SIZE * ATLAS_TILE_SIZE * 2);
    memcpy((char *)vram + VRAM_TILE1, sprite_sheet_1_start,
           ATLAS_TILE_SIZE * ATLAS_TILE_SIZE * 2);
    g_tile_ptr[0] = (char *)vram + VRAM_TILE0;
    g_tile_ptr[1] = (char *)vram + VRAM_TILE1;

    /* Aspect-preserving transform into the 480x272 screen. */
    float fx = (float)SCR_W / (float)GAME_W;
    float fy = (float)SCR_H / (float)GAME_H;
    g_scale = (fx < fy) ? fx : fy;
    g_offx = ((float)SCR_W - (float)GAME_W * g_scale) / 2.0f;
    g_offy = ((float)SCR_H - (float)GAME_H * g_scale) / 2.0f;

    /* Clip rendering to the playfield so sprites (clouds, scrolling tiles)
     * never bleed into the black letterbox bars.  Use floor for the top-left
     * and ceil for the bottom-right so the scissor FULLY covers the playfield
     * and never clips its edges (a 1px clip shows as a vertical line on real
     * hardware). */
    g_scissor_x = (int)floorf(g_offx);
    g_scissor_y = (int)floorf(g_offy);
    g_scissor_w = (int)ceilf(g_offx + (float)GAME_W * g_scale) - g_scissor_x;
    g_scissor_h = (int)ceilf(g_offy + (float)GAME_H * g_scale) - g_scissor_y;

    /* Cloud/wave model init (Cloud / Wave constructors). */
    for (int i = 0; i < NUM_CLOUDS; i++) {
        g_clouds[i].topLeftPointX = -68 + pika_rand() % (GAME_W + 68);
        g_clouds[i].topLeftPointY = pika_rand() % 152;
        g_clouds[i].topLeftPointXVelocity = 1 + pika_rand() % 2;
        g_clouds[i].sizeDiffTurnNumber = pika_rand() % 11;
    }
    g_wave.verticalCoord = 0;
    g_wave.verticalCoordVelocity = 2;
    for (int i = 0; i < GAME_W / 16; i++) {
        g_wave.yCoords[i] = 314;
    }

    g_blackAlpha = 0.0f;
    g_cur_tile = -1;
    g_readyVisible = 0;

    sceDisplayWaitVblankStart();
    sceGuDisplay(GU_TRUE);
}

void render_begin_frame(void) {
    sceGuStart(GU_DIRECT, g_list);
    /* Clear the FULL framebuffer (512-wide stride + letterbox) to black.
     * The clear honours the scissor, so set a full-buffer scissor first. */
    sceGuScissor(0, 0, BUF_W, SCR_H);
    sceGuEnable(GU_SCISSOR_TEST);
    sceGuClear(GU_COLOR_BUFFER_BIT);
    /* Then clip sprite rendering to the playfield. */
    sceGuScissor(g_scissor_x, g_scissor_y, g_scissor_w, g_scissor_h);
    /* (re)assert render state each frame for safety */
    sceGuEnable(GU_TEXTURE_2D);
    sceGuDisable(GU_DEPTH_TEST);
    sceGuDisable(GU_ALPHA_TEST);
    sceGuTexMode(GU_PSM_5551, 0, 0, GU_FALSE);
    sceGuTexFunc(GU_TFX_MODULATE, GU_TCC_RGBA);
    sceGuTexWrap(GU_CLAMP, GU_CLAMP);
    sceGuTexFilter(GU_NEAREST, GU_NEAREST);
    sceGuTexScale(1.0f / ATLAS_TILE_SIZE, 1.0f / ATLAS_TILE_SIZE);
    sceGuBlendFunc(GU_ADD, GU_SRC_ALPHA, GU_ONE_MINUS_SRC_ALPHA, 0, 0);
    sceGuEnable(GU_BLEND);
    g_cur_tile = -1;
}

void render_end_frame(void) {
    /* fade overlay on top */
    if (g_blackAlpha > 0.0001f) {
        draw_rect(TEX_OBJECTS_BLACK, 0.0f, 0.0f, (float)GAME_W, (float)GAME_H,
                  (int)(g_blackAlpha * 255.0f));
    }
    sceGuFinish();
    sceGuSync(0, 0);
    /* No vblank wait here: the main loop already syncs to vblank, and the
     * swap must happen right after it to avoid an extra frame of latency. */
    sceGuSwapBuffers();
}
