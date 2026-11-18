/* thbase.h  (IOP thread manager — PC-port shim; see iop_host.h) */
#ifndef _IOP_THBASE_H
#define _IOP_THBASE_H

#include "iop_host.h"

/* TH_C — the only attribute iopsys.irx ever asks for. */
#define TH_C 0x02000000

/* The EE kernel shim declares a different struct under this name; see
 * iop_host.h. */
#define ThreadParam         IopThreadParam

/* The IOP SDK's names.  Redirected because CreateThread, ExitThread and
 * TerminateThread are Win32 API functions on this host. */
#define CreateThread        ReiPan_IopCreateThread
#define DeleteThread        ReiPan_IopDeleteThread
#define StartThread         ReiPan_IopStartThread
#define ExitThread          ReiPan_IopExitThread
#define TerminateThread     ReiPan_IopTerminateThread
#define GetThreadId         ReiPan_IopGetThreadId
#define ReferThreadStatus   ReiPan_IopReferThreadStatus
#define SleepThread         ReiPan_IopSleepThread
#define WakeupThread        ReiPan_IopWakeupThread
#define iWakeupThread       ReiPan_IopWakeupThread
#define DelayThread         ReiPan_IopDelayThread
#define USec2SysClock       ReiPan_IopUSec2SysClock

#endif /* _IOP_THBASE_H */
