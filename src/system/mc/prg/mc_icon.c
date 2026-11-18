/* ==========================================================================
 *  system/mc/prg/mc_icon.c
 *
 *  Icon model write (FF3 0x2012a0..0x201420). From MioPan
 *  system/mc/prg/mc_icon.c (FF2), checked against FF3: the same steps
 *  without the assert; MemoryCardIconInit only forgets the buffer pointer
 *  (FF2 freed a leftover buffer there).
 * ======================================================================== */
#include "system/mc/prg/mc_icon.h"

#include <stddef.h>
#include <string.h>

#include "system/mc/prg/mc_make_file.h"     // MemoryCardMakeNewFile*
#include "system/mc/prg/mc_set_data.h"      // icon paths / sizes, disc load, data buffer

struct MC_ICON_CTRL                         /* 0x14, bss 0x3f5880 */
{
    /* 0x00 */ char step;                   // 0/1 load from the disc, 2/3 write to the card
    /* 0x04 */ int  port;
    /* 0x08 */ int  slot;
    /* 0x0c */ int  dir_label;
    /* 0x10 */ int  icon_type;
};
static_assert(sizeof(MC_ICON_CTRL) == 0x14, "MC_ICON_CTRL is 0x14 bytes");
static_assert(offsetof(MC_ICON_CTRL, dir_label) == 0x0c, "MC_ICON_CTRL.dir_label");

static void        *icon_data_addr;         // sdata 0x371114 (gp-0x23dc)
static MC_ICON_CTRL mc_icon_ctrl;           // bss 0x3f5880

// FF3 0x002012a0; FF2 MemoryCardIconInit 0x001e04a8.
void MemoryCardIconInit(int port, int slot, int dir_label, int icon_type)
{
    icon_data_addr = nullptr;
    mc_icon_ctrl.step = 0;
    mc_icon_ctrl.icon_type = icon_type;
    mc_icon_ctrl.port = port;
    mc_icon_ctrl.slot = slot;
    mc_icon_ctrl.dir_label = dir_label;
}

// FF3 0x002012c8; FF2 MemoryCardIconMain 0x001e04e0.
int MemoryCardIconMain(void)
{
    char path_name[64];
    int res = 0;
    int mc_res;

    memset(path_name, 0, 55);

    switch (mc_icon_ctrl.step)
    {
    case 0:
        icon_data_addr = GetDataMemoryArea(GetIconDataSize(mc_icon_ctrl.dir_label, mc_icon_ctrl.icon_type));
        MemoryCardDataLoadReq(icon_data_addr, GetIconDataLabel(mc_icon_ctrl.dir_label, mc_icon_ctrl.icon_type));
        mc_icon_ctrl.step = 1;
        // fall through
    case 1:
        if (MemoryCardDataLoadWait() != 0)
            mc_icon_ctrl.step = 2;
        break;

    case 2:
        MemoryCardSetIconFilePath(path_name, mc_icon_ctrl.dir_label, mc_icon_ctrl.icon_type);
        MemoryCardMakeNewFileInit(mc_icon_ctrl.port, mc_icon_ctrl.slot, path_name, icon_data_addr,
                                  GetIconDataSize(mc_icon_ctrl.dir_label, mc_icon_ctrl.icon_type));
        mc_icon_ctrl.step = 3;
        // fall through
    case 3:
        mc_res = MemoryCardMakeNewFileMain();
        if (mc_res == 1)
            res = 1;
        else if (mc_res < 0)
            res = mc_res;
        break;
    }

    if (res != 0)
        LiberateMemoryCardIconDataMem();
    return res;
}

// FF2 tested the pointer and passed it; FF3 hands LiberateDataMemoryArea its
// address, which tests and clears it.
// FF3 0x00201408; FF2 LiberateMemoryCardIconDataMem 0x001e0648.
void LiberateMemoryCardIconDataMem(void)
{
    LiberateDataMemoryArea(&icon_data_addr);
}
