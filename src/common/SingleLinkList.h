/* ==========================================================================
 *  common/SingleLinkList.h
 *
 *  Singly linked lists on the effect heap (FF2 common/SingleLinkList.c;
 *  FF3 0x120800..0x120b14, same ten functions). Every cell is its own EFFECT_MALLOC
 *  block: the forward link, then a byte copy of the caller's element. The
 *  three accessors are header inlines in FF2 and FF3 alike (the callers
 *  carry their bodies). Layout from MioPan's port; FF3's callers read
 *  RegCount at +4 and pBeginCell at +8 (effect_obj.c's list deletes).
 * ======================================================================== */
#ifndef _COMMON_SINGLELINKLIST_H
#define _COMMON_SINGLELINKLIST_H

#include <stddef.h>                     // offsetof
#include "sdk/scetypes.h"

// One cell: the link, then the element body at 0x10.
// layout: host -- pNext is a pointer; the gap to 0x10 is spelled out so the
// body stays at the ROM's offset (SingleLinkListCellAlloc sizes off it).
struct SLL_CELL                         /* 0x10 + ElemSize */
{
    /* 0x00 */ SLL_CELL *pNext;
    u_char aPad[16 - sizeof(SLL_CELL *)];
    /* 0x10 */ char BodyStart;
};
static_assert(offsetof(SLL_CELL, BodyStart) == 0x10, "SLL_CELL body at 0x10");

// layout: host -- the two cell pointers are pointers (EE 4 bytes at +8 / +0xc).
struct SINGLE_LINK_LIST                 /* 0x10 on the EE */
{
    /* 0x00 */ u_int     ElemSize;
    /* 0x04 */ u_int     RegCount;
    /* 0x08 */ SLL_CELL *pBeginCell;
    /* 0x0c */ SLL_CELL *pEndCell;
};
static_assert(offsetof(SINGLE_LINK_LIST, RegCount) == 0x04, "SINGLE_LINK_LIST.RegCount");
static_assert(offsetof(SINGLE_LINK_LIST, pBeginCell) == 0x08, "SINGLE_LINK_LIST.pBeginCell");
static_assert(offsetof(SINGLE_LINK_LIST, pEndCell) == 0x08 + sizeof(SLL_CELL *), "SINGLE_LINK_LIST.pEndCell (host)");

// Registered element count; a null list reads as empty.
static inline u_int SingleLinkListRegCount(const SINGLE_LINK_LIST *pSLL)
{
    if (pSLL == nullptr)
        return 0;
    return pSLL->RegCount;
}

// First cell, or nullptr while the list is empty (tested through RegCount).
static inline SLL_CELL *SingleLinkListBeginCell(const SINGLE_LINK_LIST *pSLL)
{
    if (SingleLinkListRegCount(pSLL) == 0)
        return nullptr;
    return pSLL->pBeginCell;
}

static inline SLL_CELL *SingleLinkListNextCell(const SLL_CELL *pCell)
{
    if (pCell == nullptr)
        return nullptr;
    return pCell->pNext;
}

void  SingleLinkListInit(SINGLE_LINK_LIST *pSLL, u_int ElemSize);      // FF3 0x001208a0
// Removes every node (SingleLinkListRemoveBegin while +4 is non-zero), then
// EFFECT_FREEs the list; nullptr does nothing.
void  SingleLinkListFree(SINGLE_LINK_LIST *pSLL);   // FF3 0x001208c0
int   SingleLinkListRemove(SINGLE_LINK_LIST *pSLL, SLL_CELL *pDelCell); // FF3 0x00120a68
// SingleLinkListRemoveBegin until RegCount is 0, then RegCount = 0.
void  SingleLinkListAllCellFree(SINGLE_LINK_LIST *pSLL);                // FF3 0x00120918
void *SingleLinkListCellBodyPtr(const SLL_CELL *pCell);                 // FF3 0x00120b08
SINGLE_LINK_LIST *SingleLinkListAlloc(u_int ElemSize);                   // FF3 0x00120848: EFFECT_MALLOC'd list
SLL_CELL *SingleLinkListAddEnd(SINGLE_LINK_LIST *pSLL, const void *pData); // FF3 0x00120968: a byte copy of pData appended
int   SingleLinkListRemoveBegin(SINGLE_LINK_LIST *pSLL);                  // FF3 0x00120a00

#endif /* _COMMON_SINGLELINKLIST_H */
