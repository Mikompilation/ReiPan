/* ==========================================================================
 *  system/os/sys_heap.c
 *
 *  The system and IOP heaps and the one-time heap layout InitMainSystem runs
 *  first (see sys_heap.h for the memory map).
 *
 *  FF2 had GetSystemHeapWrkP in system.c and a single system heap; FF3 adds
 *  the IOP heap (systemGetMem's) and a C++ heap, and initialises all three
 *  here. FF3 0x1e7560..0x1e75f0; the run before it is graphics/gs_reg.c.
 * ======================================================================== */

#include "sys_heap.h"

#include "common/cpp_heap.h"                // InitCppHeap

// ──────────────────────────────────────────────────────────────────────
// Statics / globals

static HEAP_WRK system_heap_wrk;            // data 0x328fc0
static HEAP_WRK iop_heap_wrk;               // bss  0x3ef130

// ──────────────────────────────────────────────────────────────────────
// FF3 0x001e7560; FF2 GetSystemHeapWrkP (system.c).

HEAP_WRK *GetSystemHeapWrkP(void)
{
    return &system_heap_wrk;
}

// ──────────────────────────────────────────────────────────────────────
// The heap behind systemGetMem/systemFreeMem (the EEIOP malloc).
// FF3 0x001e7570; new in FF3.

HEAP_WRK *GetIopHeapWrkP(void)
{
    return &iop_heap_wrk;
}

// ──────────────────────────────────────────────────────────────────────
// First step of InitMainSystem: the IOP heap at 0x860000 (512 KB), then the
// C++ and system heaps (a tail call in FF3).
// FF3 0x001e7580; new in FF3.

void InitSystemHeap(void)
{
    heapCtrlInit(GetIopHeapWrkP(), 0x860000, 0x80000);
    InitCppAndSystemHeap();
}

// ──────────────────────────────────────────────────────────────────────
// The C++ heap at 0x840000 (128 KB) and the system heap from 0x137c100 to
// the end of usable RAM, 0x1fa0000.
// FF3 0x001e75b0; new in FF3.

void InitCppAndSystemHeap(void)
{
    InitCppHeap(0x840000, 0x20000);
    heapCtrlInit(GetSystemHeapWrkP(), 0x137c100, 0xc23f00);
}
