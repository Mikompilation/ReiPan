/* ==========================================================================
 *  common/SingleLinkList.c
 *
 *  Singly linked lists of fixed-size elements on the effect heap (see
 *  SingleLinkList.h). FF3 0x120800..0x120b14, the same ten functions as
 *  FF2's file in the same order; ported from MioPan and checked against
 *  every FF3 body.
 * ======================================================================== */
#include "common/SingleLinkList.h"

#include <string.h>                     // memcpy

#include "graphics/effect/effect.h"     // EFFECT_MALLOC / EFFECT_FREE

// ──────────────────────────────────────────────────────────────────────
// A cell: its own heap block, header + ElemSize body, link cleared.
// FF3 0x00120800
// callcheck: EFFECT_MALLOC (read as a macro)
static SLL_CELL *SingleLinkListCellAlloc(const SINGLE_LINK_LIST *pSLL)
{
    SLL_CELL *pCell = (SLL_CELL *)EFFECT_MALLOC((int)(pSLL->ElemSize + offsetof(SLL_CELL, BodyStart)));

    if (pCell != nullptr)
        pCell->pNext = nullptr;
    return pCell;
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x00120828
static void SingleLinkListCellFree(SLL_CELL *pCell)
{
    if (pCell != nullptr)
        EFFECT_FREE(pCell);
}

// ──────────────────────────────────────────────────────────────────────
// A new empty list for ElemSize-byte elements; nullptr for ElemSize 0 or
// when the heap is full.
// FF3 0x00120848
// callcheck: EFFECT_MALLOC (read as a macro)
SINGLE_LINK_LIST *SingleLinkListAlloc(u_int ElemSize)
{
    SINGLE_LINK_LIST *pSLL = nullptr;

    // layout: host -- the EE asks for its 0x10; the host struct is wider.
    if (ElemSize != 0 && (pSLL = (SINGLE_LINK_LIST *)EFFECT_MALLOC((int)sizeof(SINGLE_LINK_LIST))) != nullptr)
        SingleLinkListInit(pSLL, ElemSize);
    return pSLL;
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x001208a0
void SingleLinkListInit(SINGLE_LINK_LIST *pSLL, u_int ElemSize)
{
    if (pSLL != nullptr)
    {
        pSLL->ElemSize   = ElemSize;
        pSLL->pBeginCell = nullptr;
        pSLL->RegCount   = 0;
        pSLL->pEndCell   = nullptr;
    }
}

// ──────────────────────────────────────────────────────────────────────
// Every cell, then the list itself.
// FF3 0x001208c0
void SingleLinkListFree(SINGLE_LINK_LIST *pSLL)
{
    if (pSLL != nullptr)
    {
        while (SingleLinkListRegCount(pSLL) != 0)
            SingleLinkListRemoveBegin(pSLL);
        EFFECT_FREE(pSLL);
    }
}

// ──────────────────────────────────────────────────────────────────────
// Every cell; the list stays, empty, for reuse.
// FF3 0x00120918
void SingleLinkListAllCellFree(SINGLE_LINK_LIST *pSLL)
{
    if (pSLL != nullptr)
    {
        while (SingleLinkListRegCount(pSLL) != 0)
            SingleLinkListRemoveBegin(pSLL);
        pSLL->RegCount = 0;
    }
}

// ──────────────────────────────────────────────────────────────────────
// Appends a byte copy of ElemSize bytes at pData; the new cell, or nullptr.
// FF3 0x00120968
SLL_CELL *SingleLinkListAddEnd(SINGLE_LINK_LIST *pSLL, const void *pData)
{
    SLL_CELL *pCell;

    if (pSLL == nullptr || (pCell = SingleLinkListCellAlloc(pSLL)) == nullptr)
        return nullptr;

    memcpy(SingleLinkListCellBodyPtr(pCell), pData, pSLL->ElemSize);
    if (pSLL->pEndCell == nullptr)
        pSLL->pBeginCell = pCell;
    else
        pSLL->pEndCell->pNext = pCell;
    pSLL->pEndCell = pCell;
    pSLL->RegCount++;
    return pCell;
}

// ──────────────────────────────────────────────────────────────────────
// Drops the first cell: 1, or 0 when the list is empty (or nullptr).
// Emptying the list clears pEndCell so AddEnd starts over.
// FF3 0x00120a00
int SingleLinkListRemoveBegin(SINGLE_LINK_LIST *pSLL)
{
    SLL_CELL *pCell;

    if (SingleLinkListRegCount(pSLL) == 0)
        return 0;

    pCell = pSLL->pBeginCell;
    pSLL->pBeginCell = pCell->pNext;
    SingleLinkListCellFree(pCell);
    if (--pSLL->RegCount == 0)
        pSLL->pEndCell = nullptr;
    return 1;
}

// ──────────────────────────────────────────────────────────────────────
// Unlinks and frees pDelCell: 1, or 0 when the list is empty or the cell
// is not on it. The end-of-list test comes before the match, so a nullptr
// pDelCell fails instead of matching the terminator.
// FF3 0x00120a68
int SingleLinkListRemove(SINGLE_LINK_LIST *pSLL, SLL_CELL *pDelCell)
{
    SLL_CELL *pPrev;
    SLL_CELL *pCell;

    if (SingleLinkListRegCount(pSLL) == 0)
        return 0;
    if (pSLL->pBeginCell == pDelCell)
        return SingleLinkListRemoveBegin(pSLL);

    pPrev = pSLL->pBeginCell;
    for (;;)
    {
        pCell = pPrev->pNext;
        if (pCell == nullptr)
            return 0;
        if (pCell == pDelCell)
            break;
        pPrev = pCell;
    }

    pPrev->pNext = pDelCell->pNext;
    if (pSLL->pEndCell == pDelCell)
        pSLL->pEndCell = pPrev;
    SingleLinkListCellFree(pDelCell);
    pSLL->RegCount--;
    return 1;
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x00120b08
void *SingleLinkListCellBodyPtr(const SLL_CELL *pCell)
{
    return (void *)&pCell->BodyStart;
}
