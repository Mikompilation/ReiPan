/* ==========================================================================
 *  album/prg/album_mem.h
 *
 *  FF3 0x128e30..0x128f38 (FF2 album_mem.c, data 0x390c00): the album
 *  screens' staging buffer. FF2 borrowed it from the tail of the 3D DMA
 *  packet ring (dmaVif1Resize); FF3 takes it from the system heap instead.
 * ======================================================================== */
#ifndef _ALBUM_PRG_ALBUM_MEM_H
#define _ALBUM_PRG_ALBUM_MEM_H

#include "sdk/scetypes.h"

#include <stddef.h>                     // offsetof

// AlbumMemMain's step (FF2's three; FF3 skips the resize wait and goes 0 -> 2).
enum
{
    ALBUM_MEM_STEP_ALLOC_REQ = 0,   // AlbumMemInit / AlbumMemFree: allocate on the next Main
    ALBUM_MEM_STEP_WAIT      = 1,   // FF2's resize wait; unused by FF3
    ALBUM_MEM_STEP_READY     = 2,   // Main answers 1
};

// FF3 0x390c00 (0xc bytes on the EE). layout: host -- mem_addr is a pointer;
// the offsets before it are FF3's (sw 0xc00 / sb 0xc04 / sw 0xc08).
typedef struct
{
    u_int  mem_size;                // +0x0: bytes AlbumMemMain allocates (0 = none)
    char   step;                    // +0x4: ALBUM_MEM_STEP_* (read with lb)
    void  *mem_addr;                // +0x8: the buffer, or NULL
} ALBUM_MEM_CTRL;
static_assert(offsetof(ALBUM_MEM_CTRL, mem_size) == 0x0, "ALBUM_MEM_CTRL.mem_size");
static_assert(offsetof(ALBUM_MEM_CTRL, step) == 0x4, "ALBUM_MEM_CTRL.step");
static_assert(offsetof(ALBUM_MEM_CTRL, mem_addr) == 0x8, "ALBUM_MEM_CTRL.mem_addr");

extern ALBUM_MEM_CTRL album_mem_ctrl;                               // FF3 0x00390c00

// Request a buffer of get_size bytes (file / line are FF2's debug tags; FF3 ignores them).
void  AlbumMemInit(u_int get_size, const char *file, int line);    // FF3 0x00128e30
// Allocate on the first call (returns 0), then 1.
int   AlbumMemMain(void);                                           // FF3 0x00128e48
void *GetAlbumMemAddr(void);                                        // FF3 0x00128ec0
// Give the buffer back to the system heap and reset the request.
void  AlbumMemFree(const char *file, int line);                     // FF3 0x00128ed0

#endif /* _ALBUM_PRG_ALBUM_MEM_H */
