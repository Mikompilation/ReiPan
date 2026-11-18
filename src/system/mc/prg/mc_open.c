/* ==========================================================================
 *  system/mc/prg/mc_open.c
 *
 *  Open a card file (FF3 0x201d80..0x201f28). From MioPan
 *  system/mc/prg/mc_open.c (FF2), the same code in FF3 without the asserts:
 *  -100 and -210 spend no retry, a busy card (-200) spends one and waits.
 * ======================================================================== */
#include "system/mc/prg/mc_open.h"

#include <stddef.h>

#include "sdk/libmc.h"
#include "system/mc/prg/mc_cmn.h"           // MemoryCardExeEndSync

// layout: host -- name is a pointer (EE 4 bytes, struct 0x14).
struct MC_FILE_OPEN_CTRL                    /* 0x14, bss 0x3f5d70 */
{
    /* 0x00 */ char  step;                  // 0 request, 1 wait, 2 drain a busy card
    /* 0x01 */ char  retry_cnt;
    /* 0x04 */ int   port;
    /* 0x08 */ int   slot;
    /* 0x0c */ int   mode;
    /* 0x10 */ char *name;
};
static_assert(offsetof(MC_FILE_OPEN_CTRL, port) == 0x04, "MC_FILE_OPEN_CTRL.port");
static_assert(offsetof(MC_FILE_OPEN_CTRL, mode) == 0x0c, "MC_FILE_OPEN_CTRL.mode");
static_assert(offsetof(MC_FILE_OPEN_CTRL, name) == 0x10, "MC_FILE_OPEN_CTRL.name");

static MC_FILE_OPEN_CTRL mc_file_open_ctrl;     // bss 0x3f5d70

// FF3 0x00201d80; FF2 MemoryCardFileOpenInit 0x001e1190.
void MemoryCardFileOpenInit(int port, int slot, char *name, int mode)
{
    mc_file_open_ctrl.step = 0;
    mc_file_open_ctrl.name = name;
    mc_file_open_ctrl.retry_cnt = 4;
    mc_file_open_ctrl.port = port;
    mc_file_open_ctrl.slot = slot;
    mc_file_open_ctrl.mode = mode;
}

// FF3 0x00201da8; FF2 MemoryCardFileOpenMain 0x001e11b8.
int MemoryCardFileOpenMain(int *fd)
{
    int result = 0;
    int res = 0;
    int mc_res;

    if (mc_file_open_ctrl.step == 0)
    {
        *fd = -1;
        mc_res = MemoryCardFileOpenReq(mc_file_open_ctrl.port, mc_file_open_ctrl.slot,
                                       mc_file_open_ctrl.name, mc_file_open_ctrl.mode);
        if (mc_res == 0)
        {
            mc_file_open_ctrl.step = 1;
        }
        else if (mc_file_open_ctrl.retry_cnt < 1)
        {
            res = -10;
        }
        else if (mc_res != sceMcErrNotInit && mc_res != sceMcErrEntryName)
        {
            mc_file_open_ctrl.retry_cnt--;
            if (mc_res == sceMcErrBusy)
                mc_file_open_ctrl.step = 2;
        }
    }

    if (mc_file_open_ctrl.step == 1)
    {
        mc_res = MemoryCardExeEndSync(&result);
        if (mc_res == 1)
        {
            if (result >= 0)
            {
                *fd = result;
                res = 1;
            }
            else
            {
                switch (result)
                {
                case sceMcResNoFormat:
                case sceMcResFullDevice:
                case sceMcResNoEntry:
                case sceMcResDeniedPermit:
                case sceMcResUpLimitHandle:
                    res = result;
                    break;
                default:
                    res = -0x14;
                    break;
                }
            }
        }
        else if (mc_res == -1)
        {
            mc_file_open_ctrl.step = 0;
        }
    }

    if (mc_file_open_ctrl.step == 2 && MemoryCardExeEndSync(&result) != 0)
        mc_file_open_ctrl.step = 0;

    return res;
}

// FF3 0x00201f10; FF2 MemoryCardFileOpenReq 0x001e1350.
int MemoryCardFileOpenReq(int port, int slot, char *name, int mode)
{
    return sceMcOpen(port, slot, name, mode);
}
