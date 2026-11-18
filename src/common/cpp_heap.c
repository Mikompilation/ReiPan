/* ==========================================================================
 *  common/cpp_heap.c
 *
 *  The C++ heap (see cpp_heap.h). FF3-only, 0x1efcf0..0x1efe20, between the
 *  math helpers (RotVector*, Vu0Sinf, GetRandValF...) and a run of chunk-table
 *  accessors at 0x1efe20.
 *
 *  PC: FF3 routes every `new`/`delete` of the game through this 128 KB heap.
 *  ReiPan does not replace the host's global operator new: the platform layer
 *  and the C++ runtime allocate through it too, some before InitCppHeap runs.
 *  The FF3 functions are kept under their Ghidra names for code that needs
 *  FF3's heap behaviour (open-items.md).
 * ======================================================================== */
#include "common/cpp_heap.h"

static HEAP_WRK cpp_heap_wrk;           // data 0x329580

// ──────────────────────────────────────────────────────────────────────
// FF3 0x001efcf0; new in FF3 (sys_heap.c passes 0x840000, 0x20000).

void InitCppHeap(u_int adrs, int size)
{
    heapCtrlInit(GetCppHeapWrkP(), adrs, size);
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x001efd30; new in FF3.

HEAP_WRK *GetCppHeapWrkP(void)
{
    return &cpp_heap_wrk;
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x001efd40 (operator new(size_t)).

void *operator_new(size_t size)
{
    return heapCtrlMalloc(GetCppHeapWrkP(), (u_int)size, 0, 0, 0);
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x001efd78 (operator delete(void *)).

void operator_delete(void *p)
{
    heapCtrlFree(GetCppHeapWrkP(), p, 0, 0, 0);
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x001efdb0 (operator new[](size_t)).

void *operator_new_array(size_t size)
{
    return heapCtrlMalloc(GetCppHeapWrkP(), (u_int)size, 0, 0, 0);
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x001efde8 (operator delete[](void *); Ghidra's body runs on to
// 0x1eff18 over three frameless functions of the next file).

void operator_delete_array(void *p)
{
    heapCtrlFree(GetCppHeapWrkP(), p, 0, 0, 0);
}
