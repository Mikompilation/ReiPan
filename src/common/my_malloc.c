/* ==========================================================================
 *  common/my_malloc.c
 *
 *  The block-list allocator heapctrl.c guards with a sema (see my_malloc.h).
 *  FF3 0x1ef1e0..0x1ef370, right after heapctrl.c as in FF2; FF3 dropped the
 *  messages, the NULL/alignment checks and my_mallocDrawMemory.
 *
 *  PC: the header is sizeof(MALLOC_HEADER) (0x18 on x64), not 0x10, and the
 *  addresses are host pointers, so the alignment is done on uintptr_t.
 * ======================================================================== */
#include "common/my_malloc.h"

#include <stdint.h>

#define MY_MALLOC_HEADER_SIZE ((int)sizeof(MALLOC_HEADER))

// PC: GetAlignUp (utility2.c) works on 32-bit EE addresses; host pointers
// are 64-bit, so my_mallocMalloc rounds up here instead.
static uintptr_t my_mallocAlignUp(uintptr_t a, int power)
{
    uintptr_t mask = ((uintptr_t)1 << power) - 1;

    return (a + mask) & ~mask;
}

// ──────────────────────────────────────────────────────────────────────
// Lay the allocator over [adrs, adrs + size): one root with all of it free.
// FF3 0x001ef1e0; FF2 my_mallocInit (no alignment check, no size field).

void my_mallocInit(MY_MALLOC *my_malloc, void *adrs, int size)
{
    my_malloc->adrs         = adrs;
    my_malloc->wrk.vac_size = size;
    my_malloc->wrk.pre      = nullptr;
    my_malloc->wrk.next     = nullptr;
    my_malloc->wrk.use_size = 0;
}

// ──────────────────────────────────────────────────────────────────────
// First fit: the first node whose free span holds the aligned block gets a
// new header right after its own block; the new node takes the rest of the
// span. Blocks are aligned to 1 << align_bit (at least 16 bytes).
// FF3 0x001ef1f8; FF2 my_mallocMalloc (FF2 asked for one alignment unit more,
// and printed when nothing fit).

void *my_mallocMalloc(MY_MALLOC *my_malloc, int size, int align_bit)
{
    MALLOC_HEADER *wrk;
    MALLOC_HEADER *new_wrk;
    MALLOC_HEADER *next_wrk;
    uintptr_t      free_adrs;
    uintptr_t      ret;
    int            use_size;

    if (align_bit < 4)
        align_bit = 4;

    use_size = size + MY_MALLOC_HEADER_SIZE;

    // callcheck: GetAlignUp (my_mallocAlignUp: host pointers are 64-bit)
    for (wrk = &my_malloc->wrk; wrk != nullptr; wrk = wrk->next)
    {
        if ((((use_size >> align_bit) + 1) << align_bit) > wrk->vac_size)
            continue;

        // The root's span starts at the region base, every other node's
        // right after its own block.
        if (wrk->pre == nullptr)
            free_adrs = (uintptr_t)my_malloc->adrs;
        else
            free_adrs = (uintptr_t)wrk + (uintptr_t)wrk->use_size;

        ret     = my_mallocAlignUp(free_adrs + MY_MALLOC_HEADER_SIZE, align_bit);
        new_wrk = (MALLOC_HEADER *)(ret - MY_MALLOC_HEADER_SIZE);

        next_wrk = wrk->next;
        if (next_wrk != nullptr)
            next_wrk->pre = new_wrk;

        new_wrk->next     = next_wrk;
        new_wrk->pre      = wrk;
        wrk->next         = new_wrk;
        new_wrk->vac_size = wrk->vac_size - (int)((uintptr_t)new_wrk - free_adrs) - use_size;
        new_wrk->use_size = use_size;
        wrk->vac_size     = (int)((uintptr_t)new_wrk - free_adrs);
        return (void *)ret;
    }

    return nullptr;
}

// ──────────────────────────────────────────────────────────────────────
// Unlink the block at adrs; its bytes join the previous node's free span.
// An unknown address is ignored.
// FF3 0x001ef2c8; FF2 my_mallocFree (FF3 has no NULL check or message; the
// root is never handed out, so pre is never NULL here).

void my_mallocFree(MY_MALLOC *my_malloc, void *adrs)
{
    MALLOC_HEADER *wrk;

    for (wrk = &my_malloc->wrk; wrk != nullptr; wrk = wrk->next)
    {
        if ((void *)((uintptr_t)wrk + MY_MALLOC_HEADER_SIZE) != adrs)
            continue;

        if (wrk->next != nullptr)
            wrk->next->pre = wrk->pre;
        wrk->pre->next      = wrk->next;
        wrk->pre->vac_size += wrk->use_size + wrk->vac_size;
        return;
    }
}

// ──────────────────────────────────────────────────────────────────────
// Sum of all free spans.
// FF3 0x001ef320; FF2 my_mallocQueryTotalFreeMem.

int my_mallocQueryTotalFreeMem(MY_MALLOC *my_malloc)
{
    MALLOC_HEADER *wrk       = &my_malloc->wrk;
    int            free_size = 0;

    do
    {
        free_size += wrk->vac_size;
        wrk = wrk->next;
    } while (wrk != nullptr);

    return free_size;
}

// ──────────────────────────────────────────────────────────────────────
// Largest free span.
// FF3 0x001ef348; FF2 my_mallocQueryMaxFreeMem.

int my_mallocQueryMaxFreeMem(MY_MALLOC *my_malloc)
{
    MALLOC_HEADER *wrk           = &my_malloc->wrk;
    int            max_free_size = 0;

    do
    {
        if (max_free_size < wrk->vac_size)
            max_free_size = wrk->vac_size;
        wrk = wrk->next;
    } while (wrk != nullptr);

    return max_free_size;
}
