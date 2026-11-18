// album/prg/album_mem.c -- FF3 0x128e30..0x128f38, FF2 album_mem.c (4 functions, data 0x390c00).
//
// The staging buffer the album load / save screens copy a memory-card album
// through (album_save.c asks for 0x100000). FF2 borrowed it from the tail of
// the 3D DMA packet ring with dmaVif1Resize and cleared it; FF3 allocates it
// from the system heap, does not clear it, and has no resize wait (step goes
// straight from 0 to 2). FF2's debug printf / size assert are gone; the
// file / line arguments stay in the prototypes because the callers still
// pass them (FF3 ignores them).

#include "album/prg/album_mem.h"

#include "common/heapctrl.h"                // heapCtrlMalloc / heapCtrlFree
#include "system/os/sys_heap.h"             // GetSystemHeapWrkP

ALBUM_MEM_CTRL album_mem_ctrl;              // FF3 0x00390c00

// FF3 0x00128e30
void AlbumMemInit(u_int get_size, const char *file, int line)
{
    (void)file;
    (void)line;

    // FF3 does not free a buffer still held here; its callers pair Init with Free.
    album_mem_ctrl.mem_size = get_size;
    album_mem_ctrl.mem_addr = NULL;
    album_mem_ctrl.step     = ALBUM_MEM_STEP_ALLOC_REQ;
}

// FF3 0x00128e48
int AlbumMemMain(void)
{
    if (album_mem_ctrl.step != ALBUM_MEM_STEP_ALLOC_REQ)
    {
        return 1;
    }

    // A zero size (after AlbumMemFree) allocates nothing; mem_addr stays as it is.
    if (album_mem_ctrl.mem_size != 0)
    {
        album_mem_ctrl.mem_addr = heapCtrlMalloc(GetSystemHeapWrkP(), album_mem_ctrl.mem_size, 0, 0, 0);
    }

    album_mem_ctrl.step = ALBUM_MEM_STEP_READY;

    return 0;
}

// FF3 0x00128ec0
void *GetAlbumMemAddr(void)
{
    return album_mem_ctrl.mem_addr;
}

// FF3 0x00128ed0
void AlbumMemFree(const char *file, int line)
{
    (void)file;
    (void)line;

    if (album_mem_ctrl.mem_addr != NULL)
    {
        heapCtrlFree(GetSystemHeapWrkP(), album_mem_ctrl.mem_addr, 0, 0, 0);
    }

    album_mem_ctrl.mem_size = 0;
    album_mem_ctrl.mem_addr = NULL;
    album_mem_ctrl.step     = ALBUM_MEM_STEP_ALLOC_REQ;
}
