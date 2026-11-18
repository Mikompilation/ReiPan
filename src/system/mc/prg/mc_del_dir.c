/* ==========================================================================
 *  system/mc/prg/mc_del_dir.c
 *
 *  Delete a whole save directory (FF3 0x200e68..0x200f98).
 *  From MioPan system/mc/prg/mc_del_dir.c (FF2): the same code in FF3
 *  without the assert on a bad step, which now just returns 0.
 *
 *  Step 2 deletes the directory through mc_del_file.c: on the card an empty
 *  directory is deleted the same way a file is.
 * ======================================================================== */
#include "system/mc/prg/mc_del_dir.h"

#include <stddef.h>
#include <string.h>

#include "system/mc/prg/mc_del_all_file.h"
#include "system/mc/prg/mc_del_file.h"
#include "system/mc/prg/mc_set_data.h"     // MemoryCardSetDirName

struct MC_DIR_DEL_CTRL                      /* 0x10, bss 0x3f5818 */
{
    /* 0x00 */ char step;                   // 0 start emptying, 1 emptying, 2 start rmdir, 3 rmdir
    /* 0x04 */ int  port;
    /* 0x08 */ int  slot;
    /* 0x0c */ int  dir_label;
};
static_assert(sizeof(MC_DIR_DEL_CTRL) == 0x10, "MC_DIR_DEL_CTRL size");
static_assert(offsetof(MC_DIR_DEL_CTRL, port) == 0x04, "MC_DIR_DEL_CTRL.port");
static_assert(offsetof(MC_DIR_DEL_CTRL, slot) == 0x08, "MC_DIR_DEL_CTRL.slot");
static_assert(offsetof(MC_DIR_DEL_CTRL, dir_label) == 0x0c, "MC_DIR_DEL_CTRL.dir_label");

static MC_DIR_DEL_CTRL mc_dir_del_ctrl;     // bss 0x3f5818

// FF3 0x00200e68; FF2 MemoryCardDirDelInit 0x001dffd0.
void MemoryCardDirDelInit(int port, int slot, int dir_label)
{
    mc_dir_del_ctrl.step = 0;
    mc_dir_del_ctrl.dir_label = dir_label;
    mc_dir_del_ctrl.port = port;
    mc_dir_del_ctrl.slot = slot;
}

// FF3 0x00200e88; FF2 MemoryCardDirDelMain 0x001dfff0.
int MemoryCardDirDelMain(void)
{
    int mc_res;
    char dir_name[0x15];

    memset(dir_name, 0, sizeof(dir_name));

    switch (mc_dir_del_ctrl.step)
    {
    case 0:
        MemoryCardAllFileDelInit(mc_dir_del_ctrl.port, mc_dir_del_ctrl.slot, mc_dir_del_ctrl.dir_label);
        mc_dir_del_ctrl.step = 1;
        /* fall through */

    case 1:
        mc_res = MemoryCardAllFileDelMain();
        if (mc_res == 1)
        {
            mc_dir_del_ctrl.step = 2;
            return 0;
        }
        break;

    case 2:
        MemoryCardSetDirName(dir_name, mc_dir_del_ctrl.dir_label);
        MemoryCardFileDelInit(mc_dir_del_ctrl.port, mc_dir_del_ctrl.slot, dir_name);
        mc_dir_del_ctrl.step = 3;
        /* fall through */

    case 3:
        mc_res = MemoryCardFileDelMain();
        if (mc_res == 1)
        {
            return 1;
        }
        break;

    default:                                // FF2 asserted here
        return 0;
    }

    return mc_res < 0 ? mc_res : 0;
}
