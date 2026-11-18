/* ==========================================================================
 *  system/mc/prg/mc_icon_sys.c
 *
 *  Write icon.sys into a card directory (FF3 0x201420..0x201558). From
 *  MioPan system/mc/prg/mc_icon_sys.c (FF2), the same code in FF3 without
 *  the assert on a bad step (Main just returns 0).
 *
 *  Init builds the whole 964-byte sceMcIconSys into this module's .bss
 *  (MemoryCardSetIconSysData returns it by value; the ROM moves it through
 *  a stack temporary), so Main needs no staging buffer: it hands its own
 *  copy to mc_make_file.c.
 * ======================================================================== */
#include "system/mc/prg/mc_icon_sys.h"

#include <stddef.h>
#include <string.h>

#include "sdk/libmc.h"                      // sceMcIconSys
#include "system/mc/prg/mc_make_file.h"     // MemoryCardMakeNewFile*
#include "system/mc/prg/mc_set_data.h"      // MemoryCardSetIconSys*

struct MC_ICON_SYS_CTRL                     // 0x3d4, bss 0x3f5898
{
    /* 0x000 */ char         step;          // 0 start the write, 1 wait for it
    /* 0x004 */ int          port;
    /* 0x008 */ int          slot;
    /* 0x00c */ int          dir_label;
    /* 0x010 */ sceMcIconSys icon_sys;      // written verbatim (0x3c4 bytes)
};
static_assert(sizeof(sceMcIconSys) == 0x3c4, "sceMcIconSys");
static_assert(offsetof(MC_ICON_SYS_CTRL, port) == 0x004, "MC_ICON_SYS_CTRL.port");
static_assert(offsetof(MC_ICON_SYS_CTRL, slot) == 0x008, "MC_ICON_SYS_CTRL.slot");
static_assert(offsetof(MC_ICON_SYS_CTRL, dir_label) == 0x00c, "MC_ICON_SYS_CTRL.dir_label");
static_assert(offsetof(MC_ICON_SYS_CTRL, icon_sys) == 0x010, "MC_ICON_SYS_CTRL.icon_sys");
static_assert(sizeof(MC_ICON_SYS_CTRL) == 0x3d4, "MC_ICON_SYS_CTRL");

static MC_ICON_SYS_CTRL mc_icon_sys_ctrl;   // bss 0x3f5898

// FF3 0x00201420; FF2 MemoryCardIconSysInit 0x001e0678.
void MemoryCardIconSysInit(int port, int slot, int dir_label)
{
    mc_icon_sys_ctrl.step = 0;
    mc_icon_sys_ctrl.port = port;
    mc_icon_sys_ctrl.slot = slot;
    mc_icon_sys_ctrl.dir_label = dir_label;

    mc_icon_sys_ctrl.icon_sys = MemoryCardSetIconSysData(dir_label);
}

// FF3 0x002014a8; FF2 MemoryCardIconSysMain 0x001e07b8.
// 1 written, 0 busy, < 0 the make-file error.
int MemoryCardIconSysMain(void)
{
    int  mc_res;
    char path_name[0x37];

    memset(path_name, 0, sizeof(path_name));

    if (mc_icon_sys_ctrl.step == 0)
    {
        MemoryCardSetIconSysPath(path_name, mc_icon_sys_ctrl.dir_label);
        MemoryCardMakeNewFileInit(mc_icon_sys_ctrl.port, mc_icon_sys_ctrl.slot, path_name,
                                  &mc_icon_sys_ctrl.icon_sys, sizeof(mc_icon_sys_ctrl.icon_sys));
        mc_icon_sys_ctrl.step = 1;
    }
    else if (mc_icon_sys_ctrl.step != 1)
    {
        return 0;
    }

    mc_res = MemoryCardMakeNewFileMain();
    if (mc_res == 1)
    {
        return 1;
    }
    if (mc_res < 0)
    {
        return mc_res;
    }
    return 0;
}
