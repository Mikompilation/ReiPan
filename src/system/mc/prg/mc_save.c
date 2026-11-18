/* ==========================================================================
 *  system/mc/prg/mc_save.c
 *
 *  Overwrite an existing card file: open, write, close (FF3
 *  0x2020e0..0x20226c). From MioPan system/mc/prg/mc_save.c (FF2), the same
 *  code in FF3 without the assert on an unknown step; the twin of mc_load.c
 *  with open mode 3 and a write in the middle.
 * ======================================================================== */
#include "system/mc/prg/mc_save.h"

#include <stddef.h>
#include <string.h>

#include "system/mc/prg/mc_close.h"
#include "system/mc/prg/mc_open.h"
#include "system/mc/prg/mc_write.h"

#define MC_OPEN_RDWR 3                      // sceMcOpen mode SCE_RDWR: no create bit, the file has to be there

// layout: host -- data_addr is a pointer (EE 4 bytes; name at +0x18, struct 0x50).
struct MC_SAVE_CTRL                         /* 0x50, bss 0x3f5da0 */
{
    /* 0x00 */ char  step;                  // 0/1 open, 2/3 write, 4/5 close
    /* 0x04 */ int   port;
    /* 0x08 */ int   slot;
    /* 0x0c */ int   size;
    /* 0x10 */ int   fd;
    /* 0x14 */ void *data_addr;
    /* 0x18 */ char  name[55];
};
static_assert(offsetof(MC_SAVE_CTRL, size) == 0x0c, "MC_SAVE_CTRL.size");
static_assert(offsetof(MC_SAVE_CTRL, fd) == 0x10, "MC_SAVE_CTRL.fd");

static MC_SAVE_CTRL mc_save_ctrl;           // bss 0x3f5da0

// FF3 0x002020e0; FF2 MemoryCardFileSaveInit 0x001e1548.
void MemoryCardFileSaveInit(int port, int slot, char *name, void *data_addr, int size)
{
    mc_save_ctrl.step = 0;
    mc_save_ctrl.port = port;
    mc_save_ctrl.slot = slot;
    mc_save_ctrl.size = size;
    mc_save_ctrl.fd = -1;
    mc_save_ctrl.data_addr = data_addr;
    memset(mc_save_ctrl.name, 0, sizeof(mc_save_ctrl.name));
    strcpy(mc_save_ctrl.name, name);
}

// A failing step's error is passed up unchanged.
// FF3 0x00202148; FF2 MemoryCardFileSaveMain 0x001e15b8.
int MemoryCardFileSaveMain(void)
{
    int mc_res;

    switch (mc_save_ctrl.step)
    {
    case 0:
        MemoryCardFileOpenInit(mc_save_ctrl.port, mc_save_ctrl.slot, mc_save_ctrl.name, MC_OPEN_RDWR);
        mc_save_ctrl.step = 1;
        // fall through
    case 1:
        mc_res = MemoryCardFileOpenMain(&mc_save_ctrl.fd);
        if (mc_res == 1)
        {
            mc_save_ctrl.step = 2;
            return 0;
        }
        break;

    case 2:
        MemoryCardFileWriteInit(mc_save_ctrl.fd, mc_save_ctrl.data_addr, mc_save_ctrl.size);
        mc_save_ctrl.step = 3;
        // fall through
    case 3:
        mc_res = MemoryCardFileWriteMain();
        if (mc_res == 1)
        {
            mc_save_ctrl.step = 4;
            return 0;
        }
        break;

    case 4:
        MemoryCardFileCloseInit(mc_save_ctrl.fd);
        mc_save_ctrl.step = 5;
        // fall through
    case 5:
        mc_res = MemoryCardFileCloseMain();
        if (mc_res == 1)
            return 1;
        break;

    default:
        return 0;
    }
    return mc_res < 0 ? mc_res : 0;
}
