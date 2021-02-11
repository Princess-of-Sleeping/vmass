/**
 * \kernelgroup{SceSysEvent}
 * \usage{psp2kern/kernel/sysevent.h,SceKernelSuspendForDriver_stub}
 */

#ifndef _PSP2_KERNEL_SYSEVENT_H_
#define _PSP2_KERNEL_SYSEVENT_H_

#include <psp2kern/types.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef int (* SceSysEventCallback)(int resume, int eventid, void *args, void *opt);

SceUID ksceKernelRegisterSysEventHandler(const char *name, SceSysEventCallback cb, void *argp);
int ksceKernelUnregisterSysEventHandler(SceUID id);

#define SCE_SYS_EVENT_STATE_SUSPEND  (0)
#define SCE_SYS_EVENT_STATE_POWEROFF (1)
#define SCE_SYS_EVENT_STATE_REBOOT   (2)

#ifdef __cplusplus
}
#endif

#endif	/* _PSP2_KERNEL_SYSEVENT_H_ */
