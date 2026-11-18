/* ==========================================================================
 *  eekernel.h  (EE kernel / libgraph entry points — PC-port shim)
 *
 *  Declarations (not implementations) for the handful of EE runtime services
 *  the reconstruction calls: semaphores, cache/exception/DMAC handlers, the GS
 *  interrupt-mask register, and scePrintf.  On the PC these are stubbed/ported
 *  under src/sdk as the port progresses; here they only need to be declared so
 *  the callers compile.
 * ======================================================================== */

#ifndef _EEKERNEL_H
#define _EEKERNEL_H

#include "scetypes.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ---- threads / semaphores (libkernel) --------------------------------- */
/* Field layout verbatim from the prototype's debug info (types.txt). */
struct SemaParam
{
    int          currentCount;
    int          maxCount;
    int          initCount;
    int          numWaitThreads;
    unsigned int attr;
    unsigned int option;
};

int ReiPan_CreateSema(struct SemaParam *param);
int ReiPan_DeleteSema(int sema_id);
int ReiPan_SignalSema(int sema_id);
int ReiPan_iSignalSema(int sema_id);
int ReiPan_WaitSema(int sema_id);
int ReiPan_PollSema(int sema_id);

/* ---- threads (libkernel) ---------------------------------------------- */
struct ThreadParam
{
    int          status;

    void (*entry)(void *dummy);
    void        *stack;
    int          stackSize;
    void        *gpReg;
    int          initPriority;
    int          currentPriority;
    unsigned int attr;
    unsigned int option;
    int          waitType;
    int          waitId;
    int          wakeupCount;
};

int  ReiPan_CreateThread(struct ThreadParam *param);
int  ReiPan_StartThread(int thread_id, void *arg);

/* PORT: a host thread cannot be pre-empted from outside, so Terminate only
 * marks the slot dormant and Delete detaches it and gives the slot back.  Both
 * are safe for the one caller in the tree -- playPssEnd() tears down
 * videoDecMain(), which returns of its own accord as soon as the decoder
 * reports end of stream. */
int  ReiPan_TerminateThread(int thread_id);
int  ReiPan_DeleteThread(int thread_id);

int  ReiPan_SuspendThread(int thread_id);

/* Claim the suspend mutex for the span the loader thread spends reading load
 * queue entries -- see the comment on these in eekernel.cpp. */
void ReiPan_LoaderCriticalEnter(void);
void ReiPan_LoaderCriticalLeave(void);
int  ReiPan_ResumeThread(int thread_id);
int  ReiPan_GetThreadId(void);
int  ReiPan_SleepThread(void);
int  ReiPan_WakeupThread(int thread_id);
int  ReiPan_ChangeThreadPriority(int thread_id, int priority);
int  ReiPan_ReferThreadStatus(int thread_id, struct ThreadParam *info);

/* ---- cache / exception / DMAC handlers -------------------------------- */
typedef void (*ReiPanDebugHandler)(u_int stat, u_int cause, u_int epc,
                                   u_int bva, u_int bpa, u_long128 *gpr);
typedef int (*ReiPanDmacHandler)(int channel);
typedef int (*ReiPanDmacHandler2)(int channel, void *arg, void *addr);

void ReiPan_FlushCache(int mode);
int  ReiPan_SetDebugHandler(int cause, ReiPanDebugHandler handler);
int  ReiPan_AddDmacHandler(int channel, ReiPanDmacHandler handler, int next);
int  ReiPan_AddDmacHandler2(int channel, ReiPanDmacHandler2 handler, int next, void *arg);
int  ReiPan_EnableDmac(int channel);

/* ---- INTC handlers ----------------------------------------------------- */
/* Cause 2 is INTC_VBLANK_START, the only one the tree registers for: playpss.c
 * hangs its PAL frame-rate divider off it.
 *
 * PORT: the registry is real -- Add returns a handle Remove takes back, and
 * ReiPan_IntcRaise() dispatches -- but NOTHING calls ReiPan_IntcRaise() yet,
 * because the port's frame loop has no vblank interrupt to hang it off.  The
 * consequence is confined and named at the call site: videoDecMain() would park
 * on its semaphore for ever if a real decoder ever ran it in PAL mode. */
typedef int (*ReiPanIntcHandler)(int cause);

int  ReiPan_AddIntcHandler(int cause, ReiPanIntcHandler handler, int next);
int  ReiPan_RemoveIntcHandler(int cause, int handler_id);
int  ReiPan_EnableIntc(int cause);
int  ReiPan_DisableIntc(int cause);
void ReiPan_IntcRaise(int cause);

/* Device reset helpers. */
int  ReiPan_sceDevVif0Reset(void);
int  ReiPan_sceDevVu0Reset(void);
int  ReiPan_sceDevVif1Reset(void);

/* EE COP0 pipeline sync + interrupt enable (asm intrinsics on the target). */
#ifndef SYNC
#define SYNC(t) ((void)0)
#endif
#ifndef EI
#define EI()    ((void)0)
#endif

/* ---- GS interrupt-mask register --------------------------------------- */
u_long ReiPan_sceGsGetIMR(void);
u_long ReiPan_sceGsPutIMR(u_long imr);

/* scePrintf is provided as an ambient `#define scePrintf printf` by
 * pc_prefix.h (force-included), so it is intentionally not declared here. */

#ifdef __cplusplus
}
#endif

/* VU0 CFC2 (read a COP2 control register) — no VU0 on the host. */
#ifndef REIPAN_EEKERNEL_NO_COMPAT_MACROS
#define CreateSema             ReiPan_CreateSema
#define DeleteSema             ReiPan_DeleteSema
#define SignalSema             ReiPan_SignalSema
#define iSignalSema            ReiPan_iSignalSema
#define WaitSema               ReiPan_WaitSema
#define PollSema               ReiPan_PollSema
#define CreateThread           ReiPan_CreateThread
#define StartThread            ReiPan_StartThread
#define TerminateThread        ReiPan_TerminateThread
#define DeleteThread           ReiPan_DeleteThread
#define SuspendThread          ReiPan_SuspendThread
#define ResumeThread           ReiPan_ResumeThread
#define GetThreadId            ReiPan_GetThreadId
#define SleepThread            ReiPan_SleepThread
#define WakeupThread           ReiPan_WakeupThread
#define ChangeThreadPriority   ReiPan_ChangeThreadPriority
#define ReferThreadStatus      ReiPan_ReferThreadStatus
#define FlushCache             ReiPan_FlushCache
#define SetDebugHandler        ReiPan_SetDebugHandler
#define AddDmacHandler         ReiPan_AddDmacHandler
#define AddDmacHandler2        ReiPan_AddDmacHandler2
#define EnableDmac             ReiPan_EnableDmac
#define AddIntcHandler         ReiPan_AddIntcHandler
#define RemoveIntcHandler      ReiPan_RemoveIntcHandler
#define EnableIntc             ReiPan_EnableIntc
#define DisableIntc            ReiPan_DisableIntc
#define sceDevVif0Reset        ReiPan_sceDevVif0Reset
#define sceDevVu0Reset         ReiPan_sceDevVu0Reset
#define sceDevVif1Reset        ReiPan_sceDevVif1Reset
#define sceGsGetIMR            ReiPan_sceGsGetIMR
#define sceGsPutIMR            ReiPan_sceGsPutIMR
#endif

#ifndef _cfc2
#define _cfc2(reg) (0u)
#endif

#endif /* _EEKERNEL_H */
