/* ==========================================================================
 *  system/mc/prg/mc_load.c
 *
 *  Load one card file: open, read, close (FF3 0x201558..0x2016e8). From
 *  MioPan system/mc/prg/mc_load.c (FF2), the same code in FF3 without the
 *  assert on an unknown step.
 * ======================================================================== */
#include "system/mc/prg/mc_load.h"

#include <stddef.h>
#include <string.h>

#include "system/mc/prg/mc_close.h"
#include "system/mc/prg/mc_open.h"
#include "system/mc/prg/mc_read.h"

#define MC_OPEN_RDONLY 1                    // sceMcOpen mode SCE_RDONLY: the file has to be there

// layout: host -- data_addr is a pointer (EE 4 bytes; name at +0x18, struct 0x50).
struct MC_LOAD_CTRL                         /* 0x50, bss 0x3f5c70 */
{
    /* 0x00 */ char  step;                  // 0/1 open, 2/3 read, 4/5 close
    /* 0x04 */ int   port;
    /* 0x08 */ int   slot;
    /* 0x0c */ int   size;
    /* 0x10 */ int   fd;
    /* 0x14 */ void *data_addr;
    /* 0x18 */ char  name[55];
};
static_assert(offsetof(MC_LOAD_CTRL, size) == 0x0c, "MC_LOAD_CTRL.size");
static_assert(offsetof(MC_LOAD_CTRL, fd) == 0x10, "MC_LOAD_CTRL.fd");

static MC_LOAD_CTRL mc_load_ctrl;           // bss 0x3f5c70

// FF3 0x00201558; FF2 MemoryCardFileLoadInit 0x001e0880.
void MemoryCardFileLoadInit(int port, int slot, char *name, void *data_addr, int size)
{
    mc_load_ctrl.step = 0;
    mc_load_ctrl.fd = -1;
    mc_load_ctrl.port = port;
    mc_load_ctrl.slot = slot;
    mc_load_ctrl.size = size;
    mc_load_ctrl.data_addr = data_addr;
    memset(mc_load_ctrl.name, 0, sizeof(mc_load_ctrl.name));
    strcpy(mc_load_ctrl.name, name);
}

// A failing step's error is passed up unchanged.
// FF3 0x002015c0; FF2 MemoryCardFileLoadMain 0x001e08f0.
int MemoryCardFileLoadMain(void)
{
    int mc_res;

    switch (mc_load_ctrl.step)
    {
    case 0:
        MemoryCardFileOpenInit(mc_load_ctrl.port, mc_load_ctrl.slot, mc_load_ctrl.name, MC_OPEN_RDONLY);
        mc_load_ctrl.step = 1;
        // fall through
    case 1:
        mc_res = MemoryCardFileOpenMain(&mc_load_ctrl.fd);
        if (mc_res == 1)
        {
            mc_load_ctrl.step = 2;
            return 0;
        }
        break;

    case 2:
        MemoryCardFileReadInit(mc_load_ctrl.fd, mc_load_ctrl.data_addr, mc_load_ctrl.size);
        mc_load_ctrl.step = 3;
        // fall through
    case 3:
        mc_res = MemoryCardFileReadMain();
        if (mc_res == 1)
        {
            mc_load_ctrl.step = 4;
            return 0;
        }
        break;

    case 4:
        MemoryCardFileCloseInit(mc_load_ctrl.fd);
        mc_load_ctrl.step = 5;
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
