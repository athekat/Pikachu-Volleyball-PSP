#ifndef STUB_PSPKERNEL_H
#define STUB_PSPKERNEL_H
typedef int SceUID;
typedef unsigned int SceSize;
void sceKernelExitGame(void);
int sceKernelCreateThread(const char *name, void *entry, int prio, int stack, int attr, void *arg);
int sceKernelStartThread(int thid, unsigned int arglen, void *argp);
int sceKernelDelayThread(unsigned int usec);
int sceKernelSleepThreadCB(void);
int sceKernelCreateCallback(const char *name, void *func, void *arg);
int sceKernelRegisterExitCallback(int cbid);
unsigned int sceKernelGetSystemTimeLow(void);
#define THREAD_ATTR_USER 0x80000000u
#define THREAD_ATTR_VFPU 0x00004000u
#define PSP_MODULE_INFO(name, major, minor, rev)
#define PSP_MAIN_THREAD_ATTR(x)
#define PSP_HEAP_SIZE_KB(x)
#endif
