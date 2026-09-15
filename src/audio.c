/*
 * audio.c - PSP audio playback.
 *
 * SFX are 16-bit mono 44100 Hz; played on a small round-robin pool of mono
 * channels with left/right panning (matching the original's stereo model).
 * BGM is streamed in 2048-sample chunks by a single persistent thread.
 */
#include <pspaudio.h>
#include <pspkernel.h>
#include <string.h>
#include "audio.h"
#include "audio_layout.h"

/* Raw asset data, embedded by the Makefile via bin2o (symbols <name>_start). */
extern unsigned char sfx_bin_start[];
extern unsigned char bgm_raw_start[];

#define SFX_VOL    0x2CCC  /* 0x8000 * 0.35  (proper SFX volume) */
#define BGM_VOL    0x199A  /* 0x8000 * 0.20  (proper BGM volume) */
#define BGM_CHUNK  2048
#define NUM_SFX_CH 4
#define BGM_CH     NUM_SFX_CH
#define SFX_BUF_CAP 0x10000

static short g_sfx_buf[NUM_SFX_CH][SFX_BUF_CAP];
static int g_sfx_reserve = 0;
static int g_next_sfx = 0;
static volatile int g_bgm_running = 0;
static SceUID g_bgm_thread = -1;

static int bgm_thread_main(SceSize args, void *argp) {
    (void)args;
    (void)argp;
    unsigned int off = 0;
    for (;;) {
        if (g_bgm_running) {
            sceAudioOutputBlocking(BGM_CH, BGM_VOL, bgm_raw_start + off * 2);
            off += BGM_CHUNK;
            if (off >= BGM_TOTAL_SAMPLES) {
                off = 0;
            }
        } else {
            sceKernelDelayThread(10000);
            off = 0;
        }
    }
    return 0;
}

void audio_init(void) {
    int maxn = 64;
    for (int i = 0; i < SFX_COUNT; i++) {
        int n = (int)g_sfx_info[i].nsamples;
        n = (n + 63) & ~63;
        if (n > maxn) {
            maxn = n;
        }
    }
    g_sfx_reserve = maxn;

    for (int ch = 0; ch < NUM_SFX_CH; ch++) {
        sceAudioChReserve(ch, maxn, PSP_AUDIO_FORMAT_MONO);
    }
    sceAudioChReserve(BGM_CH, BGM_CHUNK, PSP_AUDIO_FORMAT_MONO);
    g_next_sfx = 0;
}

void audio_shutdown(void) {
    audio_stop_bgm();
}

void audio_play_sfx(int sfx_id, int pan) {
    if (sfx_id < 0 || sfx_id >= SFX_COUNT) {
        return;
    }
    const SfxInfo *si = &g_sfx_info[sfx_id];
    int ch = g_next_sfx;
    g_next_sfx = (g_next_sfx + 1) % NUM_SFX_CH;

    short *dst = g_sfx_buf[ch];
    memcpy(dst, sfx_bin_start + si->offset, si->nsamples * 2);
    memset(dst + si->nsamples, 0,
           (size_t)(g_sfx_reserve - (int)si->nsamples) * 2);

    int l = SFX_VOL;
    int r = SFX_VOL;
    if (pan < 0) {
        r = SFX_VOL / 4;
    } else if (pan > 0) {
        l = SFX_VOL / 4;
    }
    sceAudioOutputPanned(ch, l, r, dst);
}

void audio_start_bgm(void) {
    if (g_bgm_thread < 0) {
        g_bgm_thread = sceKernelCreateThread("pika_bgm", bgm_thread_main,
                                             0x12, 0x1000, 0, NULL);
        if (g_bgm_thread >= 0) {
            sceKernelStartThread(g_bgm_thread, 0, NULL);
        }
    }
    g_bgm_running = 1;
}

void audio_stop_bgm(void) {
    g_bgm_running = 0;
}
