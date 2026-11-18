/* ==========================================================================
 *  system/mc/prg/mc_make_dir.c
 *
 *  Make one save directory on the card (FF3 0x201a58..0x201bf0).
 *  From MioPan system/mc/prg/mc_make_dir.c (FF2): the same code in FF3
 *  without the asserts (-100 sceMcInit missing, -210 bad name), which
 *  now just retry without using up a retry.
 * ======================================================================== */
#include "system/mc/prg/mc_make_dir.h"

#include <stddef.h>
#include <string.h>

#include "sdk/libmc.h"
#include "system/mc/prg/mc_cmn.h"          // MemoryCardExeEndSync

struct MC_MAKE_DIR_CTRL                     /* 0x24, bss 0x3f5cf8 */
{
    /* 0x00 */ char step;                   // 0 request, 1 wait for the mkdir, 2 drain a failed request
    /* 0x01 */ char retry_cnt;              // request retries left (starts at 4)
    /* 0x04 */ int  port;
    /* 0x08 */ int  slot;
    /* 0x0c */ char name[0x15];
};
static_assert(sizeof(MC_MAKE_DIR_CTRL) == 0x24, "MC_MAKE_DIR_CTRL size");
static_assert(offsetof(MC_MAKE_DIR_CTRL, retry_cnt) == 0x01, "MC_MAKE_DIR_CTRL.retry_cnt");
static_assert(offsetof(MC_MAKE_DIR_CTRL, port) == 0x04, "MC_MAKE_DIR_CTRL.port");
static_assert(offsetof(MC_MAKE_DIR_CTRL, slot) == 0x08, "MC_MAKE_DIR_CTRL.slot");
static_assert(offsetof(MC_MAKE_DIR_CTRL, name) == 0x0c, "MC_MAKE_DIR_CTRL.name");

static MC_MAKE_DIR_CTRL mc_make_dir_ctrl;   // bss 0x3f5cf8

// FF3 0x00201a58; FF2 MemoryCardMakeNewDirInit 0x001e0e18.
void MemoryCardMakeNewDirInit(int port, int slot, char *name)
{
    mc_make_dir_ctrl.step = 0;
    mc_make_dir_ctrl.retry_cnt = 4;
    mc_make_dir_ctrl.port = port;
    mc_make_dir_ctrl.slot = slot;

    memset(mc_make_dir_ctrl.name, 0, sizeof(mc_make_dir_ctrl.name));
    strcpy(mc_make_dir_ctrl.name, name);
}

// FF3 0x00201ab8; FF2 MemoryCardMakeNewDirMain 0x001e0e80.
int MemoryCardMakeNewDirMain(void)
{
    int res = 0;
    int mc_res;
    int result = 0;

    if (mc_make_dir_ctrl.step == 0)
    {
        mc_res = MemoryCardMakeNewDirReq(mc_make_dir_ctrl.port, mc_make_dir_ctrl.slot, mc_make_dir_ctrl.name);

        if (mc_res == 0)
        {
            mc_make_dir_ctrl.step = 1;
        }
        else if (mc_make_dir_ctrl.retry_cnt <= 0)
        {
            res = -10;
        }
        else if (mc_res != -100 && mc_res != -210)  // FF2 asserted on these two
        {
            mc_make_dir_ctrl.retry_cnt--;
            if (mc_res == -200)
            {
                mc_make_dir_ctrl.step = 2;
            }
        }
    }

    if (mc_make_dir_ctrl.step == 1)
    {
        mc_res = MemoryCardExeEndSync(&result);
        if (mc_res == 1)
        {
            res = 1;
            if (result != 0)
            {
                res = -20;
            }
        }
        else if (mc_res == -1)
        {
            mc_make_dir_ctrl.step = 0;
        }
    }

    if (mc_make_dir_ctrl.step == 2)
    {
        if (MemoryCardExeEndSync(&result) != 0)
        {
            mc_make_dir_ctrl.step = 0;
        }
    }

    return res;
}

// FF3 0x00201bd8; FF2 MemoryCardMakeNewDirReq 0x001e0fd0.
int MemoryCardMakeNewDirReq(int port, int slot, char *name)
{
    return sceMcMkdir(port, slot, name);
}
