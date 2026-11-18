/* ==========================================================================
 *  system/eeiop/file_cache.h
 *
 *  The shared file cache (FF3-only, 0x219540..0x219b48): one global table
 *  of 48 entries, each holding one file loaded whole into the system heap
 *  with a reference count. Users that want the same file share its buffer;
 *  the last release frees it (or cancels the load still in flight). The
 *  buffers together may take at most FILE_CACHE_SIZE_MAX bytes.
 *
 *  Users: the effect textures (EffKind1DTex*), Menu_DrawTexMulti, the album
 *  editor, the game-model loader (unassigned@001d2a40); IngamePre polls the
 *  loads every frame, IngameEnd and GameSystemEnd release everything.
 *
 *  The link-order guess put this in the FF2 movie-room slots, hence its old
 *  names (MovieRoomLoadReq & co.); it has nothing to do with the movie room.
 * ======================================================================== */
#ifndef _SYSTEM_EEIOP_FILE_CACHE_H
#define _SYSTEM_EEIOP_FILE_CACHE_H

#include <stddef.h>                     // offsetof

#include "sdk/scetypes.h"

#define FILE_CACHE_ENT_NUM  48              // 0x3c0 / 0x14
#define FILE_CACHE_SIZE_MAX 0x860000        // FileCacheReq's cap on the buffers' total

// One cached file. layout: host -- addr is a pointer (EE 0x14 bytes: done,
// addr, file_no, ref, size at 0x00 / 0x04 / 0x08 / 0x0c / 0x10).
struct FILE_CACHE_ENT
{
    /* 0x00 */ int   done;                  // FileLoadIsEnd2 result once polled; 0 while loading
    /* 0x04 */ void *addr;                  // system-heap buffer, nullptr when none
    /* 0x08 */ int   file_no;               // CD_FILE_DAT; -1: free
    /* 0x0c */ int   ref;                   // users holding the buffer
    /* 0x10 */ u_int size;                  // GetFileSize of file_no

    // The static init's per-entry construction / destruction (0x219a38).
    FILE_CACHE_ENT() { Clear(); }
    ~FILE_CACHE_ENT() { Release(); }

    void  Clear();                          // FF3 0x00219828
    void *Load(int file_no);                // FF3 0x00219848: the buffer, nullptr when none
    int   Poll();                           // FF3 0x00219910: 1 when a pending load was polled
    int   Release();                        // FF3 0x002199a0: 1 when the last user let go
};
static_assert(offsetof(FILE_CACHE_ENT, done) == 0x00, "FILE_CACHE_ENT.done");

// layout: host -- the entries hold pointers (EE 0x3c0 bytes, data 0x32d700).
struct FILE_CACHE
{
    FILE_CACHE_ENT ent[FILE_CACHE_ENT_NUM];

    // The static init's construction / destruction of file_cache (0x219a38).
    FILE_CACHE() { ClearAll(); }
    ~FILE_CACHE() { ReleaseAll(); }

    void  ClearAll();                       // FF3 0x00219540
    void  PollAll();                        // FF3 0x00219580
    void  Release(int file_no);             // FF3 0x002195c0
    int   IsLoaded(int file_no);            // FF3 0x00219620: the entry's done flag, 0 when absent
    int   UsedSize();                       // FF3 0x00219650
    void *Req(int file_no);                 // FF3 0x00219688: the buffer, nullptr when missing or full
    void  ReleaseAll();                     // FF3 0x00219798
};
static_assert(offsetof(FILE_CACHE, ent) == 0x00, "FILE_CACHE.ent");

extern FILE_CACHE file_cache;               // data 0x32d700

#endif /* _SYSTEM_EEIOP_FILE_CACHE_H */
