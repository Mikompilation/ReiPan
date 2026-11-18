/* ==========================================================================
 *  system/mc/prg/mc_close.c
 *
 *  Close a card file (FF3 0x200968..0x200ae8). From MioPan
 *  system/mc/prg/mc_close.c (FF2), the same code in FF3 without the assert:
 *  -100 spends no retry, a busy card (-200) spends one and waits.
 * ======================================================================== */
#include "system/mc/prg/mc_close.h"

#include "sdk/libmc.h"
#include "system/mc/prg/mc_cmn.h"           // MemoryCardExeEndSync

struct MC_FILE_CLOSE_CTRL                   /* 0x8, sbss 0x373588 */
{
    /* 0x00 */ char step;                   // 0 request, 1 wait, 2 drain a busy card
    /* 0x01 */ char retry_cnt;
    /* 0x04 */ int  fd;
};
static_assert(sizeof(MC_FILE_CLOSE_CTRL) == 0x8, "MC_FILE_CLOSE_CTRL is 8 bytes");

static MC_FILE_CLOSE_CTRL mc_file_close_ctrl;   // sbss 0x373588

// FF3 0x00200968; FF2 MemoryCardFileCloseInit 0x001dfb20.
void MemoryCardFileCloseInit(int fd)
{
    mc_file_close_ctrl.step = 0;
    mc_file_close_ctrl.retry_cnt = 4;
    mc_file_close_ctrl.fd = fd;
}

// FF3 0x00200988; FF2 MemoryCardFileCloseMain 0x001dfb38.
int MemoryCardFileCloseMain(void)
{
    int result = 0;
    int res = 0;
    int mc_res;

    if (mc_file_close_ctrl.step == 0)
    {
        mc_res = MemoryCardFileCloseReq(mc_file_close_ctrl.fd);
        if (mc_res == 0)
        {
            mc_file_close_ctrl.step = 1;
        }
        else if (mc_file_close_ctrl.retry_cnt < 1)
        {
            res = -10;
        }
        else if (mc_res != sceMcErrNotInit)
        {
            mc_file_close_ctrl.retry_cnt--;
            if (mc_res == sceMcErrBusy)
                mc_file_close_ctrl.step = 2;
        }
    }

    if (mc_file_close_ctrl.step == 1)
    {
        mc_res = MemoryCardExeEndSync(&result);
        if (mc_res == 1)
        {
            switch (result)
            {
            case sceMcResSucceed:
                res = 1;
                break;
            case sceMcResNoFormat:
            case sceMcResNoEntry:
            case sceMcResDeniedPermit:
                res = result;
                break;
            default:
                res = -0x14;
                break;
            }
        }
        else if (mc_res == -1)
        {
            mc_file_close_ctrl.step = 0;
        }
    }

    if (mc_file_close_ctrl.step == 2 && MemoryCardExeEndSync(&result) != 0)
        mc_file_close_ctrl.step = 0;

    return res;
}

// FF3 0x00200ad0; FF2 MemoryCardFileCloseReq 0x001dfc70.
int MemoryCardFileCloseReq(int fd)
{
    return sceMcClose(fd);
}
