/* timrman.h  (IOP hardware timers — PC-port shim; see iop_host.h) */
#ifndef _IOP_TIMRMAN_H
#define _IOP_TIMRMAN_H

#include "iop_host.h"

#define AllocHardTimer      ReiPan_IopAllocHardTimer
#define SetTimerHandler     ReiPan_IopSetTimerHandler
#define SetupHardTimer      ReiPan_IopSetupHardTimer
#define StartHardTimer      ReiPan_IopStartHardTimer
#define StopHardTimer       ReiPan_IopStopHardTimer

#endif /* _IOP_TIMRMAN_H */
