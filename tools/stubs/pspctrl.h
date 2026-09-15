#ifndef STUB_PSPCTRL_H
#define STUB_PSPCTRL_H
typedef struct { unsigned int Buttons; unsigned char Lx, Ly; } SceCtrlData;
void sceCtrlSetSamplingCycle(int c);
void sceCtrlSetSamplingMode(int m);
int sceCtrlReadBufferPositive(SceCtrlData *pad, int n);
#define PSP_CTRL_MODE_ANALOG 1
#define PSP_CTRL_SELECT  0x000001
#define PSP_CTRL_START   0x000008
#define PSP_CTRL_UP      0x000010
#define PSP_CTRL_RIGHT   0x000020
#define PSP_CTRL_DOWN    0x000040
#define PSP_CTRL_LEFT    0x000080
#define PSP_CTRL_CROSS   0x00004000
#define PSP_CTRL_CIRCLE  0x00002000
#endif
