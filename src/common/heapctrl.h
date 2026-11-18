/* ==========================================================================
 *  common/heapctrl.h
 *
 *  FF2's heap controller, FF3 0x1eefe8..0x1ef1e0: a semaphore around one
 *  my_malloc allocator plus a live-block count. The system, IOP and C++
 *  heaps are HEAP_WRKs (sys_heap.c, cpp_heap.c).
 * ======================================================================== */
#ifndef _COMMON_HEAPCTRL_H
#define _COMMON_HEAPCTRL_H

#include <stddef.h>                     // offsetof
#include "sdk/scetypes.h"
#include "common/my_malloc.h"

// One heap (FF3 0x20 bytes).
// layout: host -- MY_MALLOC holds pointers (EE: malloc 0x00, heap_num 0x14,
// memory_sema 0x18).
struct HEAP_WRK                     /* EE 0x20 */
{
    /* 0x00 */ MY_MALLOC malloc;
    /* 0x14 */ int       heap_num;      // live allocations (Malloc +1, Free -1)
    /* 0x18 */ int       memory_sema;   // access sema (CreateSema in heapCtrlInit)
};
static_assert(offsetof(HEAP_WRK, malloc) == 0, "HEAP_WRK.malloc");

// adrs is an EE address (the region in main RAM); heapCtrlReset maps it.
void  heapCtrlInit(HEAP_WRK *wrk, u_int adrs, int size);
void  heapCtrlReset(HEAP_WRK *wrk, u_int adrs, int size);

// Every FF3 call site passes three trailing zeros, which the retail body never
// reads (debug arguments compiled out); see systemGetMem (FF3 0x0025a190).
void *heapCtrlMalloc(HEAP_WRK *wrk, u_int size, int dbg0, int dbg1, int dbg2);
void  heapCtrlFree(HEAP_WRK *wrk, void *adrs, int dbg0, int dbg1, int dbg2);

int   heapCtrlMemSize(HEAP_WRK *wrk);           // total free bytes
int   heapCtrlQueryMaxOneSize(HEAP_WRK *wrk);   // largest free span

#endif /* _COMMON_HEAPCTRL_H */
