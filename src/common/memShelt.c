// common/memShelt.c -- CmemShelt, a heap buffer that keeps a copy of a work
// block. FF3 0x00110368..0x001105f0; FF3-only (the class file sorts between
// CWindowMsg and CmenuCamBase; open-items.md "0x10bbb0..0x116fa0").
#include "common/memShelt.h"

#include "common/heapctrl.h"            // heapCtrlMalloc, heapCtrlFree
#include "system/os/sys_heap.h"         // GetSystemHeapWrkP

#include <string.h>                     // memcpy

// ──────────────────────────────────────────────────────────────────────
// FF3 0x00110368. Forgets the buffer without freeing it.

void CmemShelt::Clear()
{
    used = 0;
    buf  = nullptr;
    size = 0;
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x00110398 (MemSheltCtor; FF3 0x00110378 is the same body). Both
// tail-jump to CmemShelt::Clear (callcheck sees no call).

CmemShelt::CmemShelt()
{
    Clear();
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x00110400 (MemSheltCtorSize; FF3 0x001103b8 is the same body).

CmemShelt::CmemShelt(int size)
{
    Clear();
    Alloc(size);
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x00110470 (MemSheltDtor; the base one 0x00110448 is the same body,
// the deleting one 0x00110498 adds operator delete).

CmemShelt::~CmemShelt()
{
    Free();
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x001104d0.

void CmemShelt::Free()
{
    if (buf != nullptr)
    {
        heapCtrlFree(GetSystemHeapWrkP(), buf, 0, 0, 0);
        buf = nullptr;
    }
    used = 0;
    size = 0;
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x00110528.

void CmemShelt::Alloc(int size)
{
    if (this->buf != nullptr)
        Free();

    void *buf = heapCtrlMalloc(GetSystemHeapWrkP(), size, 0, 0, 0);
    this->size = size;
    this->buf  = buf;
    used = 0;
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x00110590.

void CmemShelt::Store(const void *src, int n)
{
    if (n == MEM_SHELT_ALL)
        n = size;

    used = n;
    memcpy(buf, src, n);
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x001105b8. The destination comes as a pointer to the pointer (the
// retry table's getters fill a record whose first word is it).

void CmemShelt::Restore(void *const *dst, int n)
{
    if (n == MEM_SHELT_ALL)
        n = size;

    memcpy(*dst, buf, n);
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x001105e8.

void CmemShelt::SetUsed(int n)
{
    used = n;
}
