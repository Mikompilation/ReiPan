/* ==========================================================================
 *  system/mc/prg/mc_read.c
 *
 *  Read an open card file (FF3 0x201f28..0x2020e0). From MioPan
 *  system/mc/prg/mc_read.c (FF2), the same code in FF3 without the assert:
 *  -100 spends no retry, a busy card (-200) waits without spending one.
 * ======================================================================== */
#include "system/mc/prg/mc_read.h"

#include <stddef.h>

#include "sdk/libmc.h"
#include "sdk/scetypes.h"
#include "system/mc/prg/mc_close.h"         // MemoryCardFileClose*
#include "system/mc/prg/mc_cmn.h"           // MemoryCardExeEndSync

// layout: host -- data_addr is a pointer (EE 4 bytes, struct 0x14).
struct MC_FILE_READ_CTRL                    /* 0x14, bss 0x3f5d88 */
{
    /* 0x00 */ char  step;                  // 0 request, 1 wait, 2 close after an error, 3 drain a busy card
    /* 0x01 */ char  retry_cnt;
    /* 0x04 */ int   fd;
    /* 0x08 */ int   size;
    /* 0x0c */ int   error;                 // returned once the close in step 2 ends
    /* 0x10 */ void *data_addr;
};
static_assert(offsetof(MC_FILE_READ_CTRL, fd) == 0x04, "MC_FILE_READ_CTRL.fd");
static_assert(offsetof(MC_FILE_READ_CTRL, error) == 0x0c, "MC_FILE_READ_CTRL.error");
static_assert(offsetof(MC_FILE_READ_CTRL, data_addr) == 0x10, "MC_FILE_READ_CTRL.data_addr");

static MC_FILE_READ_CTRL mc_file_read_ctrl;     // bss 0x3f5d88

// FF3 0x00201f28; FF2 MemoryCardFileReadInit 0x001e1368.
void MemoryCardFileReadInit(int fd, void *data_addr, int size)
{
    mc_file_read_ctrl.step = 0;
    mc_file_read_ctrl.data_addr = data_addr;
    mc_file_read_ctrl.retry_cnt = 4;
    mc_file_read_ctrl.fd = fd;
    mc_file_read_ctrl.size = size;
    mc_file_read_ctrl.error = 0;
}

// FF3 0x00201f50; FF2 MemoryCardFileReadMain 0x001e1390.
int MemoryCardFileReadMain(void)
{
    int result = 0;
    int res = 0;
    int mc_res;

    if (mc_file_read_ctrl.step == 0)
    {
        mc_res = MemoryCardFileReadReq(mc_file_read_ctrl.fd, mc_file_read_ctrl.data_addr,
                                       mc_file_read_ctrl.size);
        if (mc_res == 0)
        {
            mc_file_read_ctrl.step = 1;
        }
        else if (mc_file_read_ctrl.retry_cnt < 1)
        {
            mc_file_read_ctrl.error = 1;
            MemoryCardFileCloseInit(mc_file_read_ctrl.fd);
            mc_file_read_ctrl.step = 2;
        }
        else if (mc_res != sceMcErrNotInit)
        {
            if (mc_res == sceMcErrBusy)
                mc_file_read_ctrl.step = 3;
            else
                mc_file_read_ctrl.retry_cnt--;
        }
    }

    if (mc_file_read_ctrl.step == 1)
    {
        mc_res = MemoryCardExeEndSync(&result);
        if (mc_res == 1)
        {
            if (result < 0)
            {
                // -5..-2 pass through, the rest is -0x14.
                res = (u_int)(result + 5) < 4 ? result : -0x14;
            }
            else if (mc_file_read_ctrl.size == result)
            {
                res = 1;
            }
            else
            {
                // Short read: -3, which the load screens show as damaged data.
                mc_file_read_ctrl.error = sceMcResFullDevice;
                MemoryCardFileCloseInit(mc_file_read_ctrl.fd);
                mc_file_read_ctrl.step = 2;
            }
        }
        else if (mc_res == -1)
        {
            mc_file_read_ctrl.step = 0;
        }
    }

    if (mc_file_read_ctrl.step == 2 && MemoryCardFileCloseMain() != 0)
        res = mc_file_read_ctrl.error;

    if (mc_file_read_ctrl.step == 3 && MemoryCardExeEndSync(&result) != 0)
        mc_file_read_ctrl.step = 0;

    return res;
}

// FF3 0x002020c8; FF2 MemoryCardFileReadReq 0x001e1530.
int MemoryCardFileReadReq(int fd, void *data_addr, int size)
{
    return sceMcRead(fd, data_addr, size);
}
