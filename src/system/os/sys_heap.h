/* ==========================================================================
 *  system/os/sys_heap.h
 *
 *  FF3's system heaps: the getters of the system heap (everything game code
 *  allocates) and the IOP heap (systemGetMem/systemFreeMem, the EEIOP malloc),
 *  and their one-time layout in main RAM:
 *
 *      0x840000 .. 0x860000   C++ heap (128 KB, GetCppHeapWrkP)
 *      0x860000 .. 0x8e0000   IOP heap (512 KB, GetIopHeapWrkP)
 *      0x137c100 .. 0x1fa0000 system heap (~12.1 MB, GetSystemHeapWrkP)
 *
 *  FF2 kept GetSystemHeapWrkP in system.c; FF3 moved it here.
 *  FF3 extent: 0x1e7560..0x1e75f0.
 * ======================================================================== */
#ifndef _SYSTEM_OS_SYS_HEAP_H
#define _SYSTEM_OS_SYS_HEAP_H

#include "common/heapctrl.h"

HEAP_WRK *GetSystemHeapWrkP(void);
HEAP_WRK *GetIopHeapWrkP(void);
void      InitSystemHeap(void);
void      InitCppAndSystemHeap(void);

#endif /* _SYSTEM_OS_SYS_HEAP_H */
