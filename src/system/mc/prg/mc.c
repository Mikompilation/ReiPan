/* ==========================================================================
 *  system/mc/prg/mc.c
 *
 *  Memory-card subsystem brackets (FF3 0x1ffe18..0x1ffe88). From MioPan
 *  system/mc/prg/mc.c (FF2), the same three functions in FF3.
 * ======================================================================== */
#include "system/mc/prg/mc.h"

#include "sdk/libmc.h"
#include "system/mc/prg/mc_check_card.h"    // MemoryCardInfoCtrlInit
#include "system/mc/prg/mc_icon.h"          // LiberateMemoryCardIconDataMem
#include "system/mc/prg/mc_set_data.h"      // soft-reset lock, play-data head

static bool reipan_card_in_use;

bool MemoryCardIsInUse(void) { return reipan_card_in_use; }

// Brings libmc up; InitSystem calls it once, before anything touches the card.
// FF3 0x001ffe18; FF2 MemoryCardInit 0x001df010.
void MemoryCardInit(void)
{
    sceMcInit();
}

// A screen takes the card: soft reset off, the cached card facts forgotten,
// the play-data head cleared.
// FF3 0x001ffe30; FF2 MemoryCardExeInit 0x001df030.
void MemoryCardExeInit(void)
{
    reipan_card_in_use = true;
    MemoryCardSoftResetLock();
    MemoryCardInfoCtrlInit();
    MemoryCardPlayDataHeadInit();
}

// Drops the icon buffer, waits (blocking, mode 0) for the request in flight
// and allows the soft reset again.
// FF3 0x001ffe58; FF2 MemoryCardEnd 0x001df060.
void MemoryCardEnd(void)
{
    LiberateMemoryCardIconDataMem();
    sceMcSync(0, nullptr, nullptr);
    MemoryCardSoftResetUnlock();
    reipan_card_in_use = false;
}
