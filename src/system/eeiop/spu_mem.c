/* ==========================================================================
 *  system/eeiop/spu_mem.c
 *
 *  SPU2 RAM allocator. 50 SPU_BUF records describe sound RAM as a chain of
 *  (used run, free run) pairs: allocating splits a free run, releasing folds
 *  a record and its free run back into the one before it. spu_buf[0] is the
 *  head, holds the reserved area and is never released, so no allocation
 *  lands at address 0.
 *
 *  From MioPan system/eeiop/spu_mem.c (FF2's) with FF3's int SPU addresses
 *  and SPUMemoryInit(start, size); FF3 drops every print (PrintBufferState
 *  is left empty, ReleaseSPUMemorySub's not-found message) and
 *  DrawSPUMemory. Replaces the temporary lib/snd_bridge.c.
 *  FF3 0x270cf8..0x2711f0.
 * ======================================================================== */
#include "system/eeiop/spu_mem.h"

#include "common/utility2.h"            // GetAlignUp
#include "sdk/eekernel.h"               // CreateSema / WaitSema / SignalSema

#define SPU_BUF_MAX         50
#define SPU_EFFECT_ALIGN    0x20000     // a reverb area ends on a 128 KB boundary

// One record: size bytes in use at adrs, then next_buf_len bytes free.
// layout: host -- next is a pointer (0x18 bytes on the EE); the records
// never leave this file.
typedef struct SPU_BUF
{
    int             use;
    int             adrs;
    int             end_adrs;           // FF2's, never used
    int             size;
    int             next_buf_len;
    struct SPU_BUF *next;
} SPU_BUF;

typedef int (*SPU_MEM_COND)(SPU_BUF *wrk, int adrs);

static SPU_BUF spu_buf[SPU_BUF_MAX];    /* bss 0x411240..0x4116f0 */
static int     spu_mem_sema;            /* sbss gp+0x5cc */

// ──────────────────────────────────────────────────────────────────────────
// First unused record, now marked used; nullptr when all 50 are.
// FF3 0x00270cf8; FF2 GetNewSPU_BUF.

static SPU_BUF *GetNewSPU_BUF(void)
{
    for (int i = 0; i < SPU_BUF_MAX; i++)
    {
        if (spu_buf[i].use == 0)
        {
            spu_buf[i].use = 1;
            return &spu_buf[i];
        }
    }

    return nullptr;
}

// FF3 0x00270d30; FF2 ReleaseSPU_BUF.

static void ReleaseSPU_BUF(SPU_BUF *wrk)
{
    wrk->use = 0;
}

// ──────────────────────────────────────────────────────────────────────────
// FF3 0x00270d38; FF2 SPUMemoryInit with the range as arguments and no print.

void SPUMemoryInit(int start, int size)
{
    struct SemaParam semap = {};
    SPU_BUF         *head;

    for (int i = 0; i < SPU_BUF_MAX; i++)
    {
        spu_buf[i].use = 0;
    }

    semap.maxCount  = 1;
    semap.initCount = 1;
    spu_mem_sema    = CreateSema(&semap);

    head               = GetNewSPU_BUF();
    head->size         = start;
    head->next_buf_len = size;
    head->next         = nullptr;
    head->adrs         = 0;
}

// ──────────────────────────────────────────────────────────────────────────
// Best fit: the free run that leaves the least behind.
// FF3 0x00270dc0; FF2 GetSPUMemory without the print.

int GetSPUMemory(int size)
{
    SPU_BUF *wrk;
    SPU_BUF *best_wrk    = nullptr;
    SPU_BUF *new_wrk;
    int      best_remain = 0;
    int      remain;
    int      ret;

    WaitSema(spu_mem_sema);

    size = (int)GetAlignUp(size, 6);
    for (wrk = spu_buf; wrk != nullptr; wrk = wrk->next)
    {
        remain = wrk->next_buf_len - size;
        if (remain >= 0 && (best_wrk == nullptr || remain < best_remain))
        {
            best_wrk    = wrk;
            best_remain = remain;
        }
    }

    ret = 0;
    if (best_wrk != nullptr)
    {
        new_wrk = GetNewSPU_BUF();
        if (new_wrk == nullptr)
        {
            // ROM bug, kept: returns without SignalSema, so every later call
            // would hang. 50 records are never all in use.
            return 0;
        }

        new_wrk->size         = size;
        new_wrk->next_buf_len = best_wrk->next_buf_len - size;
        new_wrk->next         = best_wrk->next;
        new_wrk->adrs         = best_wrk->adrs + best_wrk->size;
        best_wrk->next_buf_len = 0;
        best_wrk->next         = new_wrk;
        ret = new_wrk->adrs;
    }

    SignalSema(spu_mem_sema);
    return ret;
}

// ──────────────────────────────────────────────────────────────────────────
// Fold every record cond matches (with its free run) into the record before
// it. pre also steps onto a released record, as in the ROM; only one record
// ever matches. Returns 1 when one did.
// FF3 0x00270eb8; FF2 ReleaseSPUMemorySub without the not-found print.

static int ReleaseSPUMemorySub(int adrs, SPU_MEM_COND cond_func)
{
    SPU_BUF *pre = spu_buf;
    SPU_BUF *wrk;
    int      ret = 0;

    WaitSema(spu_mem_sema);

    for (wrk = pre->next; wrk != nullptr; pre = wrk, wrk = wrk->next)
    {
        if (cond_func(wrk, adrs))
        {
            ret                = 1;
            pre->next_buf_len += wrk->size + wrk->next_buf_len;
            pre->next          = wrk->next;
            ReleaseSPU_BUF(wrk);
        }
    }

    SignalSema(spu_mem_sema);
    return ret;
}

// FF3 0x00270f80; FF2 NormalMemoryCond.

static int NormalMemoryCond(SPU_BUF *wrk, int adrs)
{
    return wrk->adrs == adrs;
}

// FF3 0x00270f90; FF2 ReleaseSPUMemory.

int ReleaseSPUMemory(int adrs)
{
    return ReleaseSPUMemorySub(adrs, NormalMemoryCond);
}

// ──────────────────────────────────────────────────────────────────────────
// Reverb area of size bytes ending on a 128 KB boundary: the last free run
// that holds such an end wins, so the area sits as high as possible.
// FF3 0x00270fb0; FF2 GetSPUEffectMemory without the print.

int GetSPUEffectMemory(int size)
{
    SPU_BUF *wrk;
    SPU_BUF *best_wrk       = nullptr;
    SPU_BUF *new_wrk;
    int      final_end_adrs = 0;
    int      remain_size    = 0;
    int      end_adrs;
    int      next_adrs;
    int      next_align;
    int      ret;

    WaitSema(spu_mem_sema);

    for (wrk = spu_buf; wrk != nullptr; wrk = wrk->next)
    {
        end_adrs   = wrk->adrs + wrk->size;
        next_adrs  = end_adrs + wrk->next_buf_len;
        next_align = (int)GetAlignUp(end_adrs, 0x11);
        if (size < next_adrs - end_adrs && next_align <= next_adrs)
        {
            // The highest boundary inside the free run.
            do
            {
                next_align = (int)GetAlignUp(next_align + 1, 0x11);
            } while (next_align <= next_adrs);

            final_end_adrs = next_align - SPU_EFFECT_ALIGN;
            remain_size    = next_adrs - final_end_adrs;
            best_wrk       = wrk;
        }
    }

    ret = 0;
    if (final_end_adrs != 0)
    {
        ret = final_end_adrs - 1;

        new_wrk                = GetNewSPU_BUF();
        new_wrk->next_buf_len  = remain_size;
        new_wrk->next          = best_wrk->next;
        new_wrk->adrs          = final_end_adrs - size;
        best_wrk->next_buf_len -= size + remain_size;
        best_wrk->next         = new_wrk;
        new_wrk->size          = size;
    }

    SignalSema(spu_mem_sema);
    return ret;
}

// FF3 0x002710e0; FF2 EffectMemoryCond without the print: matches the end
// address GetSPUEffectMemory handed out.

static int EffectMemoryCond(SPU_BUF *wrk, int adrs)
{
    return wrk->adrs + wrk->size - 1 == adrs;
}

// FF3 0x00271100; FF2 ReleaseSPUEffectMemory.

int ReleaseSPUEffectMemory(int end_adrs)
{
    return ReleaseSPUMemorySub(end_adrs, EffectMemoryCond);
}

// FF3 0x00271120: FF2 PrintBufferState with its prints stripped.

void PrintBufferState(void)
{
}

// ──────────────────────────────────────────────────────────────────────────
// FF3 0x00271128; FF2 SPUQueryMaxFreeSize.

int SPUQueryMaxFreeSize(void)
{
    SPU_BUF *wrk = spu_buf;
    int      ret = 0;

    WaitSema(spu_mem_sema);
    do
    {
        if (ret < wrk->next_buf_len)
        {
            ret = wrk->next_buf_len;
        }
        wrk = wrk->next;
    } while (wrk != nullptr);
    SignalSema(spu_mem_sema);

    return ret;
}

// FF3 0x00271188; FF2 SPUQueryTotalFreeSize.

int SPUQueryTotalFreeSize(void)
{
    SPU_BUF *wrk = spu_buf;
    int      ret = 0;

    WaitSema(spu_mem_sema);
    do
    {
        ret += wrk->next_buf_len;
        wrk  = wrk->next;
    } while (wrk != nullptr);
    SignalSema(spu_mem_sema);

    return ret;
}
