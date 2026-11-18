/* ==========================================================================
 *  system/mc/prg/mc_check_card.c
 *
 *  Card presence and capacity (FF3 0x200168..0x200460). From MioPan
 *  system/mc/prg/mc_check_card.c (FF2), checked against FF3 function by
 *  function: the same code without MemoryCardAssert (FF3 ignores the
 *  "sceMcInit forgets" -100 and bad-path -210 answers: no retry is spent).
 *  The busy answer -200 waits for the request in flight (step 2) without
 *  spending a retry either.
 * ======================================================================== */
#include "system/mc/prg/mc_check_card.h"

#include <stddef.h>
#include <string.h>

#include "sdk/libmc.h"
#include "system/mc/prg/mc_cmn.h"           // MemoryCardExeEndSync

struct MC_INFO                              /* 0x14, bss 0x3f5298 */
{
    /* 0x00 */ int port;
    /* 0x04 */ int slot;
    /* 0x08 */ int type;                    // sceMcGetInfo's out-parameters
    /* 0x0c */ int free;
    /* 0x10 */ int format;
};
static_assert(sizeof(MC_INFO) == 0x14, "MC_INFO is 0x14 bytes");
static_assert(offsetof(MC_INFO, type) == 0x08, "MC_INFO.type");
static_assert(offsetof(MC_INFO, format) == 0x10, "MC_INFO.format");

struct MC_CHECK_CARD_CTRL                   /* 0xc, bss 0x3f52b0 */
{
    /* 0x00 */ char step;                   // 0 request, 1 wait, 2 drain a busy card
    /* 0x01 */ char retry_cnt;
    /* 0x02 */ char unformat_flg;           // the card looked unformatted once
    /* 0x04 */ int  port;
    /* 0x08 */ int  slot;
};
static_assert(sizeof(MC_CHECK_CARD_CTRL) == 0xc, "MC_CHECK_CARD_CTRL is 0xc bytes");
static_assert(offsetof(MC_CHECK_CARD_CTRL, unformat_flg) == 0x02, "MC_CHECK_CARD_CTRL.unformat_flg");
static_assert(offsetof(MC_CHECK_CARD_CTRL, port) == 0x04, "MC_CHECK_CARD_CTRL.port");

struct MC_CHECK_CARD_EVERY_FRAME            /* 0x8, sbss 0x373570 */
{
    /* 0x00 */ int port;
    /* 0x04 */ int slot;
};
static_assert(sizeof(MC_CHECK_CARD_EVERY_FRAME) == 0x8, "MC_CHECK_CARD_EVERY_FRAME is 8 bytes");

static MC_INFO                   mc_info;                       // bss 0x3f5298
static MC_CHECK_CARD_CTRL        mc_check_card_ctrl;            // bss 0x3f52b0
static MC_CHECK_CARD_EVERY_FRAME mc_check_card_every_frame;     // sbss 0x373570

// FF3 0x00200168; FF2 MemoryCardInfoCtrlInit 0x001df418.
void MemoryCardInfoCtrlInit(void)
{
    memset(&mc_info, 0, sizeof(mc_info));
    mc_info.slot = -1;
    mc_info.port = -1;
}

// FF3 0x002001b0; FF2 MemoryCardGetCardInfoInit 0x001df458.
void MemoryCardGetCardInfoInit(int port, int slot)
{
    mc_check_card_ctrl.step = 0;
    mc_check_card_ctrl.slot = slot;
    mc_check_card_ctrl.retry_cnt = 4;
    mc_check_card_ctrl.port = port;
    mc_check_card_ctrl.unformat_flg = 0;
}

// FF3 0x002001d8; FF2 MemoryCardCheckEveryFrameInit 0x001df480.
void MemoryCardCheckEveryFrameInit(int port, int slot)
{
    mc_check_card_every_frame.port = port;
    mc_check_card_every_frame.slot = slot;
    MemoryCardGetCardInfoInit(port, slot);
}

// FF3 0x002001f8; FF2 MemoryCardGetCardInfoMain 0x001df4a8.
int MemoryCardGetCardInfoMain(void)
{
    int result = 0;
    int res = 0;
    int mc_res;

    if (mc_check_card_ctrl.step == 0)
    {
        mc_res = MemoryCardGetCardInfoReq(mc_check_card_ctrl.port, mc_check_card_ctrl.slot);
        if (mc_res == 0)
        {
            mc_check_card_ctrl.step = 1;
        }
        else if (mc_check_card_ctrl.retry_cnt < 1)
        {
            res = -10;
        }
        else if (mc_res != sceMcErrNotInit && mc_res != sceMcErrEntryName)
        {
            if (mc_res == sceMcErrBusy)
                mc_check_card_ctrl.step = 2;
            else
                mc_check_card_ctrl.retry_cnt--;
        }
    }

    if (mc_check_card_ctrl.step == 1)
    {
        mc_res = MemoryCardExeEndSync(&result);
        if (mc_res == 1)
        {
            if (GetAccessMemoryCardType() != 2)
            {
                res = -0x14;                // a PS1 card or a PDA
            }
            else if (GetAccessMemoryCardFormat() == 1 && result != -2)
            {
                if (result == 0)
                    res = 1;
                else if (result == -1 || result == -5)
                    res = result;
                else
                    res = -0x14;
            }
            else if (mc_check_card_ctrl.unformat_flg < 1)
            {
                // First look: ask once more before believing it.
                mc_check_card_ctrl.unformat_flg = 1;
                mc_check_card_ctrl.step = 0;
            }
            else
            {
                res = -2;
            }
        }
        else if (mc_res == -1)
        {
            mc_check_card_ctrl.step = 0;
        }
    }

    if (mc_check_card_ctrl.step == 2 && MemoryCardExeEndSync(&result) != 0)
        mc_check_card_ctrl.step = 0;

    return res;
}

// FF3 0x00200378; FF2 MemoryCardGetCardInfoReq 0x001df688.
int MemoryCardGetCardInfoReq(int port, int slot)
{
    mc_info.port = port;
    mc_info.slot = slot;
    return sceMcGetInfo(port, slot, &mc_info.type, &mc_info.free, &mc_info.format);
}

// FF3 0x002003a8; FF2 MemoryCardCheckEveryFrameMain 0x001df6b8.
int MemoryCardCheckEveryFrameMain(void)
{
    int res = MemoryCardGetCardInfoMain();

    if (res == 1)
    {
        MemoryCardGetCardInfoInit(mc_check_card_every_frame.port, mc_check_card_every_frame.slot);
        res = 1;
    }
    else if (res >= 0)
    {
        res = 0;
    }
    return res;
}

// FF3 0x002003f0; FF2 GetAccessMemoryCardPort 0x001df708.
int GetAccessMemoryCardPort(void)
{
    return (u_int)mc_info.port < 2 ? mc_info.port : -1;
}

// FF3 0x00200408; FF2 GetAccessMemoryCardSlot 0x001df720.
int GetAccessMemoryCardSlot(void)
{
    return (u_int)mc_info.slot < 2 ? mc_info.slot : -1;
}

// FF3 0x00200420.
int GetAccessMemoryCardType(void)
{
    return mc_info.type;
}

// FF3 0x00200430.
int GetAccessMemoryCardFreeCluster(void)
{
    return mc_info.free;
}

// FF3 0x00200440.
int GetAccessMemoryCardFormat(void)
{
    return mc_info.format;
}

// FF3 0x00200450.
void MemoryCardSetAccessPort(int port)
{
    mc_info.port = port;
}
