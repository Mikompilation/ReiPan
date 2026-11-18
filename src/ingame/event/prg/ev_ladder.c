/* ==========================================================================
 *  ingame/event/prg/ev_ladder.c
 *
 *  The ladder list, FF3 0x199150..0x1992d0 (FF3-only, no FF2 counterpart;
 *  between ene_spe_obj.c and ev_change.c). HASIGO_ECB holds one 0x90-byte
 *  LADDER_DAT row per ladder; the list loads it with the event data and
 *  finds a ladder by id for the player's climb.
 * ======================================================================== */

#include "ingame/event/prg/ev_ladder.h"

#include "graphics/ecb.h"               // EcbGetSize, EcbGetNum, EcbGetData
#include "reipan/reipan_memory.h"       // PC: ReiPan_GetHostPointer
#include "system/eeiop/cd_file_dat.h"   // HASIGO_ECB
#include "system/os/eecdvd.h"           // LoadReqGetAddr, IsLoadEnd

// The table's EE address (LadderList::Load), after the event camera's VCI
// pack at 0xfcc000.
#define LADDER_LIST_ADDR 0xfcd000

// The one list (sdata gp-0x4d0). FF3 0x00199278
// (__static_initialization_and_destruction_0, initialize 1 / priority
// 0xffff) runs the ctor on it, FF3 0x001992b0 (_GLOBAL__I_) calls that; a
// static object's constructor does the same here. No _GLOBAL__D_: the class
// has no destructor.
static LadderList ladder_list;

// FF3 0x00199158 (complete; the base ctor FF3 0x00199150 is the same store).
// Only ecb is cleared; load_id stays as the zeroed sdata left it.

LadderList::LadderList()
{
    ecb = nullptr;
}

// FF3 0x00199160 (EventDataLoad).

void LadderList::Load()
{
    // PC: the fixed EE address goes through the emulated RAM block.
    ecb = ReiPan_GetHostPointer(LADDER_LIST_ADDR);
    LoadReqGetAddr(HASIGO_ECB, ecb, &load_id);
}

// FF3 0x00199190 (EventRelease). Nothing is freed: the table sits in the
// fixed event area.

void LadderList::Release()
{
    if (ecb != nullptr) {
        ecb = nullptr;
    }
}

// FF3 0x001991a8 (PlyrLadderStartBottom / Top). Only a table of 0x90-byte
// rows is searched; the whole table is walked and the last row with that
// id wins (FF3 does not stop at the first).

LADDER_DAT *LadderList::GetData(short id)
{
    LADDER_DAT *found = nullptr;
    int         num;
    int         i;

    if (ecb == nullptr) {
        return nullptr;
    }
    if (EcbGetSize(ecb) != (int)sizeof(LADDER_DAT)) {
        return nullptr;
    }

    num = EcbGetNum(ecb);
    for (i = 0; i < num; i++) {
        LADDER_DAT *dat = (LADDER_DAT *)EcbGetData(ecb, i);

        if (dat->id == id) {
            found = dat;
        }
    }
    return found;
}

// FF3 0x00199250 (EventDataIsLoadEnd).

int LadderList::IsLoadEnd()
{
    return ::IsLoadEnd(load_id);
}

// FF3 0x00199268.

LadderList *GetLadderList(void)
{
    return &ladder_list;
}
