/* ==========================================================================
 *  system/mc/prg/mc_make.c
 *
 *  Make a whole save directory on the card (FF3 0x2016e8..0x201828).
 *  From MioPan system/mc/prg/mc_make.c (FF2): the same code in FF3 without
 *  the asserts (NULL buffer, unknown step).
 * ======================================================================== */
#include "system/mc/prg/mc_make.h"

#include <stddef.h>
#include <string.h>

#include "system/mc/prg/mc_make_all_file.h"
#include "system/mc/prg/mc_make_dir.h"
#include "system/mc/prg/mc_set_data.h"     // MemoryCardSetDirName

// layout: host -- buff_addr is a pointer (EE 4 bytes, buff_size at +0x14, struct 0x18).
struct MC_NEW_MAKE_CTRL                     /* 0x18, bss 0x3f5cc0 */
{
    /* 0x00 */ char  step;                  // 0/1 mkdir, 2/3 make the files
    /* 0x04 */ int   port;
    /* 0x08 */ int   slot;
    /* 0x0c */ int   dir_label;
    /* 0x10 */ void *buff_addr;
    /* 0x14 */ int   buff_size;
};
static_assert(offsetof(MC_NEW_MAKE_CTRL, port) == 0x04, "MC_NEW_MAKE_CTRL.port");
static_assert(offsetof(MC_NEW_MAKE_CTRL, dir_label) == 0x0c, "MC_NEW_MAKE_CTRL.dir_label");

static MC_NEW_MAKE_CTRL mc_new_make_ctrl;   // bss 0x3f5cc0

// FF3 0x002016e8; FF2 MemoryCardNewMakeInit 0x001e0a28.
void MemoryCardNewMakeInit(int port, int slot, int dir_label, void *buff_addr, int buff_size)
{
    mc_new_make_ctrl.step = 0;
    mc_new_make_ctrl.port = port;
    mc_new_make_ctrl.slot = slot;
    mc_new_make_ctrl.dir_label = dir_label;
    mc_new_make_ctrl.buff_addr = buff_addr;
    mc_new_make_ctrl.buff_size = buff_size;
}

// A failing step's error is passed up unchanged.
// FF3 0x00201710; FF2 MemoryCardNewMakeMain 0x001e0a78.
int MemoryCardNewMakeMain(void)
{
    int mc_res;
    char dir_name[21];

    memset(dir_name, 0, sizeof(dir_name));

    switch (mc_new_make_ctrl.step)
    {
    case 0:
        MemoryCardSetDirName(dir_name, mc_new_make_ctrl.dir_label);
        MemoryCardMakeNewDirInit(mc_new_make_ctrl.port, mc_new_make_ctrl.slot, dir_name);
        mc_new_make_ctrl.step = 1;
        // fall through
    case 1:
        mc_res = MemoryCardMakeNewDirMain();
        if (mc_res == 1)
        {
            mc_new_make_ctrl.step = 2;
            return 0;
        }
        break;

    case 2:
        MemoryCardAllFileMakeInit(mc_new_make_ctrl.port, mc_new_make_ctrl.slot, mc_new_make_ctrl.dir_label,
                                  mc_new_make_ctrl.buff_addr, mc_new_make_ctrl.buff_size);
        mc_new_make_ctrl.step = 3;
        // fall through
    case 3:
        mc_res = MemoryCardAllFileMakeMain();
        if (mc_res == 1)
        {
            return 1;
        }
        break;

    default:
        return 0;
    }

    return mc_res < 0 ? mc_res : 0;
}
