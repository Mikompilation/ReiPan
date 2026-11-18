/* ==========================================================================
 *  common/Load.h
 *
 *  CLoad (FF3 RTTI, vtable 0x341018): one file load into a buffer the
 *  object allocates from the system heap (CLoad::ReqAlloc) or the caller
 *  gives (CLoad::ReqAddr), freed with the object (FF3 0x0010d8b8..0x0010dbe8;
 *  FF3-only, no FF2 file). CMapDBuff (ingame/map/MapDBuff.h) embeds one.
 * ======================================================================== */
#ifndef _COMMON_LOAD_H
#define _COMMON_LOAD_H

#include "sdk/scetypes.h"

#include <stddef.h>                     // offsetof

#define LOAD_MODE_NONE  0               // nothing requested
#define LOAD_MODE_ALLOC 1               // addr is the object's heap buffer (freed with it)
#define LOAD_MODE_ADDR  2               // addr is the caller's buffer

// 0x18 bytes on the EE.
// layout: host -- the vptr is 4 bytes on the EE: mode is at EE +4, addr +0x14.
class CLoad
{
public:
    CLoad();                            // FF3 0x0010d8e0 (LoadCtor; twin 0x0010d8b8)
    virtual ~CLoad();                   // EE vtable +0x00 0x0010d970 (base 0x0010d908), deleting +0x04 0x0010d9d8

    void ReqAddr(int file, void *addr); // FF3 0x0010da40: into addr (its own buffer when it has one)
    void *ReqAlloc(int file, int size); // FF3 0x0010dad8: into a new heap buffer (size -1: the file's); nullptr while a load runs
    void Cancel();                      // FF3 0x0010db80
    int IsEnd();                        // FF3 0x0010dbb8: 1 also when nothing was requested

    int   mode;                         // EE +0x04: LOAD_MODE_*
    int   load_id;                      // EE +0x08: LoadReqGetAddr's request id, 0 none
    int   file;                         // EE +0x0c: CD_FILE_DAT number, -1 none
    int   size;                         // EE +0x10: buffer bytes, -1 until known
    void *addr;                         // EE +0x14: the buffer
};
static_assert(offsetof(CLoad, load_id) == offsetof(CLoad, mode) + 4, "CLoad.load_id follows mode");
static_assert(offsetof(CLoad, size) == offsetof(CLoad, mode) + 0xc, "CLoad.size");
static_assert(offsetof(CLoad, addr) == offsetof(CLoad, mode) + 0x10, "CLoad.addr follows size");

#endif /* _COMMON_LOAD_H */
