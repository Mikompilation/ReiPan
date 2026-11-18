/* ==========================================================================
 *  system/mc/prg/mc_del_file.c
 *
 *  Delete one card file, or an empty directory (FF3 0x200f98..0x201144):
 *  request sceMcDelete, retry a refused request, then wait for the result.
 *  From MioPan system/mc/prg/mc_del_file.c (FF2): the same code in FF3
 *  without the asserts on -100 (sceMcInit missing) and -210 (bad name),
 *  which now just request again without using up a retry, and without the
 *  warning on -4. -4 (no such entry) counts as deleted, so
 *  MemoryCardAllFileDelMain can walk a stale listing; -2, -5 and -6 pass
 *  through, every other failure becomes -20. A busy request (-200) parks
 *  in step 2 without spending a retry. Used by mc_del_all_file.c and
 *  mc_del_dir.c.
 * ======================================================================== */
#include "system/mc/prg/mc_del_file.h"

#include <stddef.h>
#include <string.h>

#include "sdk/libmc.h"
#include "system/mc/prg/mc_cmn.h"          // MemoryCardExeEndSync

struct MC_FILE_DEL_CTRL                     /* 0x44, bss 0x3f5828 */
{
    /* 0x00 */ char step;                   // 0 request, 1 wait for the delete, 2 drain a busy request
    /* 0x01 */ char retry_cnt;              // request retries left (starts at 4)
    /* 0x04 */ int  port;
    /* 0x08 */ int  slot;
    /* 0x0c */ char name[0x37];             // full path, e.g. "/<dir>/<file>"
};
static_assert(sizeof(MC_FILE_DEL_CTRL) == 0x44, "MC_FILE_DEL_CTRL size");
static_assert(offsetof(MC_FILE_DEL_CTRL, retry_cnt) == 0x01, "MC_FILE_DEL_CTRL.retry_cnt");
static_assert(offsetof(MC_FILE_DEL_CTRL, port) == 0x04, "MC_FILE_DEL_CTRL.port");
static_assert(offsetof(MC_FILE_DEL_CTRL, slot) == 0x08, "MC_FILE_DEL_CTRL.slot");
static_assert(offsetof(MC_FILE_DEL_CTRL, name) == 0x0c, "MC_FILE_DEL_CTRL.name");

static MC_FILE_DEL_CTRL mc_file_del_ctrl;   // bss 0x3f5828

// FF3 0x00200f98; FF2 MemoryCardFileDelInit 0x001e0120.
void MemoryCardFileDelInit(int port, int slot, char *name)
{
    mc_file_del_ctrl.step = 0;
    mc_file_del_ctrl.retry_cnt = 4;
    mc_file_del_ctrl.port = port;
    mc_file_del_ctrl.slot = slot;

    memset(mc_file_del_ctrl.name, 0, sizeof(mc_file_del_ctrl.name));
    strcpy(mc_file_del_ctrl.name, name);
}

// FF3 0x00200ff8; FF2 MemoryCardFileDelMain 0x001e0188.
// 1 deleted (or already gone), 0 busy, -2/-5/-6 passed through,
// -10 out of retries, -20 failed.
int MemoryCardFileDelMain(void)
{
    int res = 0;
    int mc_res;
    int result = 0;

    if (mc_file_del_ctrl.step == 0)
    {
        mc_res = MemoryCardFileDelReq(mc_file_del_ctrl.port, mc_file_del_ctrl.slot, mc_file_del_ctrl.name);

        if (mc_res == 0)
        {
            mc_file_del_ctrl.step = 1;
        }
        else if (mc_file_del_ctrl.retry_cnt <= 0)
        {
            res = -10;
        }
        else if (mc_res == -200)            // busy: wait it out, no retry spent
        {
            mc_file_del_ctrl.step = 2;
        }
        else if (mc_res != -100 && mc_res != -210)  // FF2 asserted on these two
        {
            mc_file_del_ctrl.retry_cnt--;
        }
    }

    if (mc_file_del_ctrl.step == 1)
    {
        mc_res = MemoryCardExeEndSync(&result);
        if (mc_res == 1)
        {
            res = 1;
            switch (result)
            {
            case 0:
            case -4:                        // no such entry: nothing to delete (FF2 warned)
                break;

            case -2:
            case -5:
            case -6:
                res = result;
                break;

            default:
                res = -20;
                break;
            }
        }
        else if (mc_res == -1)
        {
            mc_file_del_ctrl.step = 0;
        }
    }

    if (mc_file_del_ctrl.step == 2)
    {
        if (MemoryCardExeEndSync(&result) != 0)
        {
            mc_file_del_ctrl.step = 0;
        }
    }

    return res;
}

// FF3 0x00201130; FF2 MemoryCardFileDelReq 0x001e0318.
int MemoryCardFileDelReq(int port, int slot, char *name)
{
    return sceMcDelete(port, slot, name);
}
