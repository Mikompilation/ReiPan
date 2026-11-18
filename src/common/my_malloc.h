/* ==========================================================================
 *  common/my_malloc.h
 *
 *  The block-list allocator under heapctrl.c (FF2 my_malloc.c, FF3
 *  0x1ef1e0..0x1ef370). Every block is preceded by a MALLOC_HEADER; the
 *  headers form a list starting at MY_MALLOC.wrk, and each one owns the free
 *  span (vac_size) between its block and the next header.
 * ======================================================================== */
#ifndef _COMMON_MY_MALLOC_H
#define _COMMON_MY_MALLOC_H

#include <stddef.h>                     // offsetof
#include "sdk/scetypes.h"

// One block header (FF3 0x10): pre/next links, the block's size including
// this header, and the free bytes after the block.
// layout: host -- holds two list pointers (EE 4 bytes each, +0x0/+0x4); the
// sizes are at +0x8/+0xc on the EE.
struct MALLOC_HEADER                /* EE 0x10 */
{
    /* 0x0 */ MALLOC_HEADER *pre;
    /* 0x4 */ MALLOC_HEADER *next;
    /* 0x8 */ int            use_size;  // header + requested bytes
    /* 0xc */ int            vac_size;  // free bytes after the block
};
static_assert(offsetof(MALLOC_HEADER, pre) == 0, "MALLOC_HEADER.pre");

// One allocator (FF3 0x14; FF2 0x18 had the region size too, for the debug
// memory view FF3 dropped). wrk is the list root: its pre is always NULL and
// its free span starts at adrs.
// layout: host -- MALLOC_HEADER and adrs are pointers (EE: wrk 0x0, adrs 0x10).
struct MY_MALLOC                    /* EE 0x14 */
{
    /* 0x00 */ MALLOC_HEADER wrk;
    /* 0x10 */ void         *adrs;      // region base (host pointer)
};
static_assert(offsetof(MY_MALLOC, wrk) == 0, "MY_MALLOC.wrk");

void  my_mallocInit(MY_MALLOC *my_malloc, void *adrs, int size);
void *my_mallocMalloc(MY_MALLOC *my_malloc, int size, int align_bit);
void  my_mallocFree(MY_MALLOC *my_malloc, void *adrs);
int   my_mallocQueryTotalFreeMem(MY_MALLOC *my_malloc);
int   my_mallocQueryMaxFreeMem(MY_MALLOC *my_malloc);

#endif /* _COMMON_MY_MALLOC_H */
