/*
 * audio.h - PSP audio (SFX with stereo panning + looping BGM).
 *
 * All audio is 16-bit mono 44100 Hz PCM.
 *   - SFX: 7 samples packed in assets/sfx.bin (see audio_layout.h).
 *   - BGM: assets/bgm.raw, streamed in chunks by a dedicated thread.
 */
#ifndef PIKA_AUDIO_H
#define PIKA_AUDIO_H

void audio_init(void);
void audio_shutdown(void);

/* pan: -1 left, 0 center, 1 right */
void audio_play_sfx(int sfx_id, int pan);

void audio_start_bgm(void);
void audio_stop_bgm(void);

#endif /* PIKA_AUDIO_H */
