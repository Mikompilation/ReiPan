/* thsemap.h  (IOP semaphore manager — PC-port shim; see iop_host.h) */
#ifndef _IOP_THSEMAP_H
#define _IOP_THSEMAP_H

#include "iop_host.h"

/* The EE kernel shim declares a different struct under this name; see
 * iop_host.h. */
#define SemaParam           IopSemaParam

#define CreateSema          ReiPan_IopCreateSema
#define DeleteSema          ReiPan_IopDeleteSema
#define SignalSema          ReiPan_IopSignalSema
/* The interrupt-context form; there is no separate one on the host. */
#define iSignalSema         ReiPan_IopSignalSema
#define WaitSema            ReiPan_IopWaitSema
#define PollSema            ReiPan_IopPollSema
#define ReferSemaStatus     ReiPan_IopReferSemaStatus

#endif /* _IOP_THSEMAP_H */
