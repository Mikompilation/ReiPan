/* ==========================================================================
 *  system/mc/prg/mc_format.c
 *
 *  Format a memory card (FF3 0x201148..0x20129c): request sceMcFormat,
 *  retry a refused request, then wait for the result. From MioPan
 *  system/mc/prg/mc_format.c (FF2): the same code in FF3 without the
 *  assert on -100 (sceMcInit missing), which now just retries without
 *  using up a retry. -5 (denied) passes through, every other failure
 *  becomes -20. Used by album_save.c and game_data_save.c.
 * ======================================================================== */
#include "system/mc/prg/mc_format.h"

#include <stddef.h>

#include "sdk/libmc.h"
#include "system/mc/prg/mc_cmn.h"          // MemoryCardExeEndSync

struct MC_FORMAT_CTRL                       /* 0x0c, bss 0x3f5870 */
{
    /* 0x00 */ char step;                   // 0 request, 1 wait for the format, 2 drain a failed request
    /* 0x01 */ char retry_cnt;              // request retries left (starts at 4)
    /* 0x04 */ int  port;
    /* 0x08 */ int  slot;
};
static_assert(sizeof(MC_FORMAT_CTRL) == 0x0c, "MC_FORMAT_CTRL size");
static_assert(offsetof(MC_FORMAT_CTRL, retry_cnt) == 0x01, "MC_FORMAT_CTRL.retry_cnt");
static_assert(offsetof(MC_FORMAT_CTRL, port) == 0x04, "MC_FORMAT_CTRL.port");
static_assert(offsetof(MC_FORMAT_CTRL, slot) == 0x08, "MC_FORMAT_CTRL.slot");

static MC_FORMAT_CTRL mc_format_ctrl;       // bss 0x3f5870

// FF3 0x00201148; FF2 MemoryCardFormatInit 0x001e0330.
void MemoryCardFormatInit(int port, int slot)
{
    mc_format_ctrl.step = 0;
    mc_format_ctrl.retry_cnt = 4;
    mc_format_ctrl.port = port;
    mc_format_ctrl.slot = slot;
}

// FF3 0x00201168; FF2 MemoryCardFormatMain 0x001e0350.
// 1 formatted, 0 busy, -5 denied, -10 out of retries, -20 failed.
int MemoryCardFormatMain(void)
{
    int res = 0;
    int mc_res;
    int result = 0;

    if (mc_format_ctrl.step == 0)
    {
        mc_res = MemoryCardFormatReq(mc_format_ctrl.port, mc_format_ctrl.slot);

        if (mc_res == 0)
        {
            mc_format_ctrl.step = 1;
        }
        else if (mc_format_ctrl.retry_cnt <= 0)
        {
            res = -10;
        }
        else if (mc_res != -100)            // FF2 asserted here
        {
            mc_format_ctrl.retry_cnt--;
            if (mc_res == -200)
            {
                mc_format_ctrl.step = 2;
            }
        }
    }

    if (mc_format_ctrl.step == 1)
    {
        mc_res = MemoryCardExeEndSync(&result);
        if (mc_res == 1)
        {
            res = 1;
            if (result != 0)
            {
                res = result == -5 ? -5 : -20;
            }
        }
        else if (mc_res == -1)
        {
            mc_format_ctrl.step = 0;
        }
    }

    if (mc_format_ctrl.step == 2)
    {
        if (MemoryCardExeEndSync(&result) != 0)
        {
            mc_format_ctrl.step = 0;
        }
    }

    return res;
}

// FF3 0x00201288; FF2 MemoryCardFormatReq 0x001e0490.
int MemoryCardFormatReq(int port, int slot)
{
    return sceMcFormat(port, slot);
}
