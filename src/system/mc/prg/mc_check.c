/* ==========================================================================
 *  system/mc/prg/mc_check.c
 *
 *  Card query, then directory listing (FF3 0x1ffe88..0x1fffd0). From MioPan
 *  system/mc/prg/mc_check.c (FF2), the same code in FF3 without the assert
 *  on an unknown step.
 * ======================================================================== */
#include "system/mc/prg/mc_check.h"

#include <stddef.h>
#include <string.h>

#include "system/mc/prg/mc_check_card.h"    // MemoryCardGetCardInfo*
#include "system/mc/prg/mc_check_dir.h"     // MemoryCardGetDirInfo*

struct MC_CHECK_CTRL                        /* 0x44, bss 0x3f5250 */
{
    /* 0x00 */ char step;                   // 0/1 card query, 2/3 listing
    /* 0x04 */ int  port;
    /* 0x08 */ int  slot;
    /* 0x0c */ char name[55];
};
static_assert(sizeof(MC_CHECK_CTRL) == 0x44, "MC_CHECK_CTRL is 0x44 bytes");
static_assert(offsetof(MC_CHECK_CTRL, name) == 0x0c, "MC_CHECK_CTRL.name");

static MC_CHECK_CTRL mc_check_ctrl;         // bss 0x3f5250

// FF3 0x001ffe88; FF2 MemoryCardCheckInit 0x001df098.
void MemoryCardCheckInit(int port, int slot, char *name)
{
    mc_check_ctrl.step = 0;
    mc_check_ctrl.port = port;
    mc_check_ctrl.slot = slot;
    memset(mc_check_ctrl.name, 0, sizeof(mc_check_ctrl.name));
    strcpy(mc_check_ctrl.name, name);
}

// A swapped card (-1) is not an error here: the listing reads the new one.
// FF3 0x001ffee0; FF2 MemoryCardCheckMain 0x001df0f8.
int MemoryCardCheckMain(void)
{
    int mc_res;

    switch (mc_check_ctrl.step)
    {
    case 0:
        MemoryCardGetCardInfoInit(mc_check_ctrl.port, mc_check_ctrl.slot);
        mc_check_ctrl.step = 1;
        // fall through
    case 1:
        mc_res = MemoryCardGetCardInfoMain();
        if (mc_res != 1)
        {
            if (mc_res >= 0)
                return 0;
            if (mc_res != -1)
                return mc_res;
        }
        mc_check_ctrl.step = 2;
        return 0;

    case 2:
        MemoryCardGetDirInfoInit(mc_check_ctrl.port, mc_check_ctrl.slot, mc_check_ctrl.name);
        mc_check_ctrl.step = 3;
        // fall through
    case 3:
        mc_res = MemoryCardGetDirInfoMain();
        if (mc_res == 1)
            return 1;
        if (mc_res < 0)
            return mc_res;
        return 0;
    }
    return 0;
}
