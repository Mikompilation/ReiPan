// common/Load.c -- CLoad, one file load into an owned or a given buffer.
// FF3 0x0010d8b8..0x0010dbe8; FF3-only (the class file sorts between
// LightCtl and MapDBuff; open-items.md "0x10bbb0..0x116fa0").
#include "common/Load.h"

#include "common/heapctrl.h"            // heapCtrlMalloc, heapCtrlFree
#include "system/eeiop/cddat.h"         // GetFileSize
#include "system/eeiop/fileload.h"      // FileLoadCancel
#include "system/os/eecdvd.h"           // LoadReqGetAddr, IsLoadEnd
#include "system/os/sys_heap.h"         // GetSystemHeapWrkP

// ──────────────────────────────────────────────────────────────────────
// FF3 0x0010d8e0 (LoadCtor; FF3 0x0010d8b8 is the same body).

CLoad::CLoad()
{
    mode    = LOAD_MODE_NONE;
    load_id = 0;
    size    = -1;
    addr    = nullptr;
    file    = -1;
}

// ──────────────────────────────────────────────────────────────────────
// Cancels a running load and frees an owned buffer.
// FF3 0x0010d970 (LoadDtor; the base one 0x0010d908 is the same body,
// the deleting one 0x0010d9d8 adds operator delete).

CLoad::~CLoad()
{
    Cancel();

    if (mode == LOAD_MODE_ALLOC)
    {
        heapCtrlFree(GetSystemHeapWrkP(), addr, 0, 0, 0);
        addr = nullptr;
    }
    else
    {
        addr = nullptr;
    }
}

// ──────────────────────────────────────────────────────────────────────
// Loads file into addr; an object that owns a heap buffer loads into that
// one instead. Ignored while a load runs. No callers.
// FF3 0x0010da40.

void CLoad::ReqAddr(int file, void *addr)
{
    int size;

    if (IsEnd() == 0)
        return;

    if (mode != LOAD_MODE_ALLOC)
    {
        this->addr = addr;
        mode = LOAD_MODE_ADDR;
    }

    this->file = file;
    size       = (int)GetFileSize(file);
    if (this->size == -1)
        this->size = size;

    LoadReqGetAddr(file, this->addr, &load_id);
}

// ──────────────────────────────────────────────────────────────────────
// Allocates size bytes (-1: the file's size) from the system heap and
// loads file into them; returns the buffer, nullptr while a load runs. A
// buffer the object already owns is not freed first (as FF3).
// FF3 0x0010dad8.

void *CLoad::ReqAlloc(int file, int size)
{
    if (IsEnd() == 0)
        return nullptr;

    this->file = file;
    mode = LOAD_MODE_ALLOC;
    if (size == -1)
        this->size = (int)GetFileSize(file);
    else
        this->size = size;

    addr = heapCtrlMalloc(GetSystemHeapWrkP(), this->size, 0, 0, 0);
    LoadReqGetAddr(file, addr, &load_id);

    return addr;
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x0010db80.

void CLoad::Cancel()
{
    if (load_id != 0)
        FileLoadCancel(load_id, nullptr, nullptr);
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x0010dbb8.

int CLoad::IsEnd()
{
    if (load_id != 0)
        return IsLoadEnd(load_id);

    return 1;
}
