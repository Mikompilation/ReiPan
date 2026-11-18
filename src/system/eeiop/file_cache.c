/* ==========================================================================
 *  system/eeiop/file_cache.c
 *
 *  The shared file cache (FF3 0x219540..0x219b48, FF3-only): FILE_CACHE is
 *  one global table (data 0x32d700) of 48 FILE_CACHE_ENT, each one file
 *  loaded whole into the system heap (FileLoadReqEE, priority 6) with a
 *  reference count. FileCacheReq hands out the buffer of a file already
 *  held (one more reference) or starts loading it into a free entry while
 *  the buffers stay within FILE_CACHE_SIZE_MAX; FileCachePollAll (IngamePre,
 *  every frame) records which loads finished; FileCacheRelease drops a
 *  reference and the last one frees the buffer, or cancels a load still in
 *  flight and frees it from the loader's callback.
 *
 *  Written from the FF3 decompilation; the link-order guess put the run in
 *  FF2's movie_room.c / movie_room_menu.c slots, but nothing here matches
 *  FF2 (the FF2 hits MovieRoomMenuCtrlInit, MovieRoomMenuTexLoadWait,
 *  MovieRoomMenuDisp and Init were wrong matches).
 * ======================================================================== */
#include "system/eeiop/file_cache.h"

#include "common/heapctrl.h"            // heapCtrlMalloc / heapCtrlFree
#include "system/eeiop/cddat.h"         // GetFileSize
#include "system/eeiop/fileload.h"      // FileLoadReqEE / FileLoadIsEnd2 / FileLoadCancel2
#include "system/os/sys_heap.h"         // GetSystemHeapWrkP

// ──────────────────────────────────────────────────────────────────────
// The table, built and torn down by the file's static init: FF3 0x00219a38
// (FileCache_static_init, __static_initialization_and_destruction_0) runs
// FileCacheEntClear on the 48 entries and then FileCacheClearAll when
// initialize is 1, FileCacheReleaseAll and then FileCacheEntRelease on the
// entries in reverse when it is 0; FF3 0x00219b08 (FileCache_GLOBAL_I, the
// .ctors entry at 0x2e9170) and FF3 0x00219b28 (FileCache_GLOBAL_D, .dtors
// 0x2e91b4) call it with (1, 0xffff) and (0, 0xffff). The C++ constructors
// and destructors of FILE_CACHE / FILE_CACHE_ENT do the same here, in the
// same order (members first on construction, last on destruction).
// callcheck: FileCacheEntClear, FileCacheClearAll, FileCacheReleaseAll and
// FileCacheEntRelease are reached through those constructors / destructors.

FILE_CACHE file_cache;                  // data 0x32d700

// ══════════════════════════════════════════════════════════════════════
// FILE_CACHE_ENT
// ══════════════════════════════════════════════════════════════════════

// ──────────────────────────────────────────────────────────────────────
// Free, nothing loaded.
// FF3 0x00219828 (FileCacheEntClear).

void FILE_CACHE_ENT::Clear()
{
    size = 0;
    file_no = -1;
    done = 0;
    addr = nullptr;
    ref = 0;
}

// ──────────────────────────────────────────────────────────────────────
// Takes a system-heap buffer the size of file_no and starts loading the file
// into it, with this as its only user. nullptr for a missing or empty file
// (the entry stays free) or when the heap is full (addr is then nullptr and
// the entry still free).
// FF3 0x00219848 (FileCacheEntLoad).

void *FILE_CACHE_ENT::Load(int file_no)
{
    u_int fsize = GetFileSize(file_no);

    if (GetFileSize(file_no) == (u_int)-1 || GetFileSize(file_no) == 0)
        return nullptr;

    addr = heapCtrlMalloc(GetSystemHeapWrkP(), fsize, 0, 0, 0);
    if (addr != nullptr)
    {
        size = fsize;
        ref = 1;
        done = 0;
        this->file_no = file_no;
        FileLoadReqEE(file_no, addr, 6, nullptr, nullptr);
    }
    return addr;
}

// ──────────────────────────────────────────────────────────────────────
// While the entry's load is pending, records whether it has finished.
// 1 when it polled, 0 for a free entry, no buffer, or a load already done.
// FF3 0x00219910 (FileCacheEntPoll).

int FILE_CACHE_ENT::Poll()
{
    if (file_no == -1)
        return 0;
    if (addr == nullptr || done != 0)
        return 0;

    done = FileLoadIsEnd2(file_no, addr);
    return 1;
}

// ──────────────────────────────────────────────────────────────────────
// The loader's callback for a cancelled load: the buffer goes back to the
// system heap once the loader has let go of it.
// FF3 0x00219968 (FileCacheFreeCB).

static void FileCacheFreeCB(void *adrs, void *arg)
{
    (void)arg;
    heapCtrlFree(GetSystemHeapWrkP(), adrs, 0, 0, 0);
}

// ──────────────────────────────────────────────────────────────────────
// One user lets go. The last one frees the buffer -- a load still in flight
// is cancelled and FileCacheFreeCB frees it -- and clears the entry; 1 then,
// 0 while users remain or for a free entry.
// FF3 0x002199a0 (FileCacheEntRelease).

int FILE_CACHE_ENT::Release()
{
    if (file_no == -1 || --ref != 0)
        return 0;

    if (addr != nullptr)
    {
        if (done == 0)
            FileLoadCancel2(file_no, addr, FileCacheFreeCB, nullptr);
        else
            heapCtrlFree(GetSystemHeapWrkP(), addr, 0, 0, 0);
    }
    Clear();
    return 1;
}

// ══════════════════════════════════════════════════════════════════════
// FILE_CACHE
// ══════════════════════════════════════════════════════════════════════

// ──────────────────────────────────────────────────────────────────────
// Forgets every entry without freeing anything (IngameInit, and the
// constructor).
// FF3 0x00219540 (FileCacheClearAll).

void FILE_CACHE::ClearAll()
{
    for (FILE_CACHE_ENT *e = ent; e < ent + FILE_CACHE_ENT_NUM; e++)
        e->Clear();
}

// ──────────────────────────────────────────────────────────────────────
// Polls every pending load (IngamePre, the album editor).
// FF3 0x00219580 (FileCachePollAll).

void FILE_CACHE::PollAll()
{
    for (FILE_CACHE_ENT *e = ent; e < ent + FILE_CACHE_ENT_NUM; e++)
        e->Poll();
}

// ──────────────────────────────────────────────────────────────────────
// One reference less on every entry holding file_no.
// FF3 0x002195c0 (FileCacheRelease).

void FILE_CACHE::Release(int file_no)
{
    for (FILE_CACHE_ENT *e = ent; e < ent + FILE_CACHE_ENT_NUM; e++)
    {
        if (e->file_no == file_no)
            e->Release();
    }
}

// ──────────────────────────────────────────────────────────────────────
// The done flag of the first entry holding file_no (non-zero once its load
// finished and was polled), 0 when no entry holds it.
// FF3 0x00219620 (FileCacheIsLoaded).

int FILE_CACHE::IsLoaded(int file_no)
{
    for (FILE_CACHE_ENT *e = ent; e < ent + FILE_CACHE_ENT_NUM; e++)
    {
        if (e->file_no == file_no)
            return e->done;
    }
    return 0;
}

// ──────────────────────────────────────────────────────────────────────
// Bytes the used entries' buffers take.
// FF3 0x00219650 (FileCacheUsedSize).

int FILE_CACHE::UsedSize()
{
    int total = 0;

    for (FILE_CACHE_ENT *e = ent; e < ent + FILE_CACHE_ENT_NUM; e++)
    {
        if (e->file_no != -1)
            total += e->size;
    }
    return total;
}

// ──────────────────────────────────────────────────────────────────────
// The buffer of file_no: an entry holding it gains a user; otherwise the
// first free entry loads it, unless that would take the buffers past
// FILE_CACHE_SIZE_MAX. nullptr for a missing or empty file, a full table or
// a full cache. The buffer is only valid once IsLoaded says so.
// FF3 0x00219688 (FileCacheReq).

void *FILE_CACHE::Req(int file_no)
{
    FILE_CACHE_ENT *e;

    if (GetFileSize(file_no) == (u_int)-1)
        return nullptr;
    if (GetFileSize(file_no) == 0)
        return nullptr;

    for (e = ent; e < ent + FILE_CACHE_ENT_NUM; e++)
    {
        if (e->file_no == file_no)
        {
            e->ref++;
            return e->addr;
        }
    }

    for (e = ent; e < ent + FILE_CACHE_ENT_NUM; e++)
    {
        if (e->file_no == -1)
        {
            if ((int)(UsedSize() + GetFileSize(file_no)) > FILE_CACHE_SIZE_MAX)
                return nullptr;
            return e->Load(file_no);
        }
    }
    return nullptr;
}

// ──────────────────────────────────────────────────────────────────────
// Drops every user of every entry, freeing all buffers (IngameEnd,
// GameSystemEnd, and the destructor).
// FF3 0x00219798 (FileCacheReleaseAll).

void FILE_CACHE::ReleaseAll()
{
    for (int i = 0; i < FILE_CACHE_ENT_NUM; i++)
    {
        if (ent[i].file_no != -1)
        {
            while (ent[i].Release() == 0)
                ;
        }
    }
}
