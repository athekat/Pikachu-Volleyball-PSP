#ifndef STUB_PSPAUDIO_H
#define STUB_PSPAUDIO_H
#define PSP_AUDIO_FORMAT_MONO 0x10
#define PSP_AUDIO_FORMAT_STEREO 0x00
int sceAudioChReserve(int channel, int sample_count, int format);
int sceAudioOutputBlocking(int channel, int vol, void *buf);
int sceAudioOutputPanned(int channel, int left_vol, int right_vol, void *buf);
#endif
