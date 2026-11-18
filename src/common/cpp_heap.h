/* ==========================================================================
 *  common/cpp_heap.h
 *
 *  FF3's C++ heap (FF3-only, 0x1efcf0..0x1efe20): the HEAP_WRK behind the
 *  game's operator new/delete, laid over 0x840000..0x860000 by
 *  InitCppAndSystemHeap (sys_heap.c).
 * ======================================================================== */
#ifndef _COMMON_CPP_HEAP_H
#define _COMMON_CPP_HEAP_H

#include <stddef.h>                     // size_t
#include "common/heapctrl.h"

void      InitCppHeap(u_int adrs, int size);
HEAP_WRK *GetCppHeapWrkP(void);

// FF3's global operator new / delete / new[] / delete[] (Ghidra names). PC:
// they are plain functions; `new` expressions in ReiPan use the host's
// allocator (see cpp_heap.c).
void *operator_new(size_t size);
void  operator_delete(void *p);
void *operator_new_array(size_t size);
void  operator_delete_array(void *p);

#endif /* _COMMON_CPP_HEAP_H */
