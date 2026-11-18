/* ==========================================================================
 *  common/heapctrl.c
 *
 *  The semaphore-guarded heap controller (see heapctrl.h). FF3
 *  0x1eefe8..0x1ef1e0, FF2's heapctrl.c without its message, SAFE_MALLOC and
 *  heapCtrlDrawMemory; my_malloc.c follows it.
 * ======================================================================== */
#include "common/heapctrl.h"

#include <eekernel.h>                   // CreateSema/WaitSema/SignalSema
#include "reipan/reipan_memory.h"       // ReiPan_GetHostPointer

// ──────────────────────────────────────────────────────────────────────
// Create the heap's access sema (one holder), then lay the heap out.
// FF3 0x001eefe8; FF2 heapCtrlInit (without the printf).

void heapCtrlInit(HEAP_WRK *wrk, u_int adrs, int size)
{
    struct SemaParam semap;

    semap.maxCount  = 1;
    semap.initCount = 1;
    wrk->memory_sema = CreateSema(&semap);
    heapCtrlReset(wrk, adrs, size);
}

// ──────────────────────────────────────────────────────────────────────
// Forget every block: the whole region is free again.
// FF3 0x001ef048; FF2 heapCtrlReset.

void heapCtrlReset(HEAP_WRK *wrk, u_int adrs, int size)
{
    WaitSema(wrk->memory_sema);
    // PC: the region is an EE address; the allocator works on host pointers.
    my_mallocInit(&wrk->malloc, ReiPan_GetHostPointer(adrs), size);
    wrk->heap_num = 0;
    SignalSema(wrk->memory_sema);
}

// ──────────────────────────────────────────────────────────────────────
// A 64-byte aligned block; counts it even when the allocation failed.
// FF3 0x001ef0a0; FF2 heapCtrlMalloc (FF2 counted only successes).

void *heapCtrlMalloc(HEAP_WRK *wrk, u_int size, int dbg0, int dbg1, int dbg2)
{
    void *ret;

    WaitSema(wrk->memory_sema);
    ret = my_mallocMalloc(&wrk->malloc, (int)size, 6);
    wrk->heap_num++;
    SignalSema(wrk->memory_sema);
    return ret;
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x001ef100; FF2 heapCtrlFree (count taken inside the sema in FF3).

void heapCtrlFree(HEAP_WRK *wrk, void *adrs, int dbg0, int dbg1, int dbg2)
{
    WaitSema(wrk->memory_sema);
    my_mallocFree(&wrk->malloc, adrs);
    wrk->heap_num--;
    SignalSema(wrk->memory_sema);
}

// ──────────────────────────────────────────────────────────────────────
// Total free bytes.
// FF3 0x001ef150; FF2 heapCtrlMemSize (its unused mode argument is gone).

int heapCtrlMemSize(HEAP_WRK *wrk)
{
    int ret;

    WaitSema(wrk->memory_sema);
    ret = my_mallocQueryTotalFreeMem(&wrk->malloc);
    SignalSema(wrk->memory_sema);
    return ret;
}

// ──────────────────────────────────────────────────────────────────────
// Largest single free span.
// FF3 0x001ef198; FF2 heapCtrlQueryMaxOneSize.

int heapCtrlQueryMaxOneSize(HEAP_WRK *wrk)
{
    int ret;

    WaitSema(wrk->memory_sema);
    ret = my_mallocQueryMaxFreeMem(&wrk->malloc);
    SignalSema(wrk->memory_sema);
    return ret;
}
