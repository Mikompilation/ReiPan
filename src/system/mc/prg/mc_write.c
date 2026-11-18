/* ==========================================================================
 *  system/mc/prg/mc_write.c
 *
 *  sceMcWrite wrapper with retries (FF3 0x203160..0x203310). From MioPan
 *  system/mc/prg/mc_write.c (FF2), the same code in FF3 without the assert
 *  on a -100 request result (FF3 just asks again next frame, without using
 *  up a retry). The twin of mc_read.c; a short write is not an error.
 * ======================================================================== */
#include "system/mc/prg/mc_write.h"

#include <stddef.h>

#include "sdk/libmc.h"
#include "system/mc/prg/mc_close.h"         // MemoryCardFileClose*
#include "system/mc/prg/mc_cmn.h"           // MemoryCardExeEndSync

// layout: host -- data_addr is a pointer (EE 4 bytes; struct 0x14).
struct MC_FILE_WRITE_CTRL                   /* 0x14, bss 0x3f5ea8 */
{
    /* 0x00 */ char  step;                  // 0 request, 1 wait, 2 close after failure, 3 wait out a busy card
    /* 0x01 */ char  retry_cnt;
    /* 0x04 */ int   fd;
    /* 0x08 */ int   size;
    /* 0x0c */ int   error;
    /* 0x10 */ void *data_addr;
};
static_assert(offsetof(MC_FILE_WRITE_CTRL, retry_cnt) == 0x01, "MC_FILE_WRITE_CTRL.retry_cnt");
static_assert(offsetof(MC_FILE_WRITE_CTRL, fd) == 0x04, "MC_FILE_WRITE_CTRL.fd");
static_assert(offsetof(MC_FILE_WRITE_CTRL, size) == 0x08, "MC_FILE_WRITE_CTRL.size");
static_assert(offsetof(MC_FILE_WRITE_CTRL, error) == 0x0c, "MC_FILE_WRITE_CTRL.error");

static MC_FILE_WRITE_CTRL mc_file_write_ctrl;   // bss 0x3f5ea8

// FF3 0x00203160; FF2 MemoryCardFileWriteInit 0x001e2db0.
void MemoryCardFileWriteInit(int fd, void *data_addr, int size)
{
    mc_file_write_ctrl.step = 0;
    mc_file_write_ctrl.retry_cnt = 4;
    mc_file_write_ctrl.fd = fd;
    mc_file_write_ctrl.size = size;
    mc_file_write_ctrl.data_addr = data_addr;
    mc_file_write_ctrl.error = 0;
}

// FF3 0x00203188; FF2 MemoryCardFileWriteMain 0x001e2dd8.
int MemoryCardFileWriteMain(void)
{
    int res = 0;
    int result = 0;
    int mc_res;

    if (mc_file_write_ctrl.step == 0)
    {
        mc_res = MemoryCardFileWriteReq(mc_file_write_ctrl.fd, mc_file_write_ctrl.data_addr,
                                        mc_file_write_ctrl.size);
        if (mc_res == 0)
        {
            mc_file_write_ctrl.step = 1;
        }
        else if (mc_file_write_ctrl.retry_cnt < 1)
        {
            mc_file_write_ctrl.error = -10;
            MemoryCardFileCloseInit(mc_file_write_ctrl.fd);
            mc_file_write_ctrl.step = 2;
        }
        else if (mc_res == -100)
        {
            // sceMcInit not called: FF2 asserted here; FF3 retries without counting
        }
        else if (mc_res == -200)
        {
            mc_file_write_ctrl.step = 3;
        }
        else
        {
            mc_file_write_ctrl.retry_cnt--;
        }
    }

    if (mc_file_write_ctrl.step == 1)
    {
        mc_res = MemoryCardExeEndSync(&result);
        if (mc_res == 1)
        {
            res = 1;
            if (result < 0)
            {
                switch (result)
                {
                case -2:
                case -3:
                case -4:
                case -5:
                case -8:
                    res = result;
                    break;
                default:
                    res = -20;
                    break;
                }
            }
        }
        else if (mc_res == -1)
        {
            mc_file_write_ctrl.step = 0;
        }
    }

    if (mc_file_write_ctrl.step == 2 && MemoryCardFileCloseMain() != 0)
    {
        res = mc_file_write_ctrl.error;
    }

    if (mc_file_write_ctrl.step == 3 && MemoryCardExeEndSync(&result) != 0)
    {
        mc_file_write_ctrl.step = 0;
    }

    return res;
}

// FF3 0x002032e8; FF2 MemoryCardFileWriteReq 0x001e2f60.
int MemoryCardFileWriteReq(int fd, void *data_addr, int size)
{
    return sceMcWrite(fd, data_addr, size);
}
