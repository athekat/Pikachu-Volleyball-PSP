#ifndef STUB_PSPGU_H
#define STUB_PSPGU_H

#define GU_DIRECT 0
#define GU_TRUE 1
#define GU_FALSE 0

#define GU_PSM_5551 0x0001
#define GU_PSM_5650 0x0002

#define GU_ALPHA_TEST 0
#define GU_DEPTH_TEST 1
#define GU_SCISSOR_TEST 2
#define GU_BLEND 4
#define GU_TEXTURE_2D 9
#define GU_ALWAYS 0
#define GU_ADD 0
#define GU_SRC_ALPHA 1
#define GU_ONE_MINUS_SRC_ALPHA 2
#define GU_TFX_MODULATE 0
#define GU_TCC_RGBA 2
#define GU_NEAREST 0
#define GU_LINEAR 1
#define GU_REPEAT 0
#define GU_CLAMP 1
#define GU_COLOR_BUFFER_BIT 1
#define GU_SPRITES 6
#define GU_TRIANGLE_STRIP 4
#define GU_TEXTURE_32BITF 1
#define GU_COLOR_8888 2
#define GU_VERTEX_32BITF 3
#define GU_TRANSFORM_2D 4

void sceGuInit(void);
void sceGuStart(int c, void *list);
void sceGuDrawBuffer(int psm, void *fbp, int bw);
void sceGuDispBuffer(int w, int h, void *fbp, int bw);
void sceGuOffset(int x, int y);
void sceGuViewport(int cx, int cy, int w, int h);
void sceGuDepthRange(int near, int far);
void sceGuScissor(int x, int y, int w, int h);
void sceGuEnable(int state);
void sceGuDisable(int state);
void sceGuDepthFunc(int func);
void sceGuClearColor(unsigned int color);
void sceGuClearDepth(unsigned int depth);
void sceGuClear(int flags);
void sceGuBlendFunc(int op, int src, int dst, int srcFix, int dstFix);
void sceGuTexMode(int tpsm, int maxmips, int a2, int swizzle);
void sceGuTexFunc(int tfunc, int tcc);
void sceGuTexFilter(int min, int mag);
void sceGuTexWrap(int s, int t);
void sceGuTexScale(float u, float v);
void sceGuTexOffset(float u, float v);
void sceGuTexImage(int mipmap, int w, int h, int tbw, const void *tbp);
void *sceGuGetMemory(int size);
void sceGuDrawArray(int prim, int vtype, int count, const void *indices, const void *vertices);
void sceGuFinish(void);
int sceGuSync(int sync, int what);
void sceGuDisplay(int on);
void sceGuSwapBuffers(void);
#endif
