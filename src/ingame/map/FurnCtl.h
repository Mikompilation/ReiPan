/* ==========================================================================
 *  ingame/map/FurnCtl.h
 *
 *  Furniture (map object) control. Minimal so far: what reconstructed code
 *  calls.
 * ======================================================================== */
#ifndef _INGAME_MAP_FURNCTL_H
#define _INGAME_MAP_FURNCTL_H

#include "sdk/scetypes.h"

// CWindowMsg (SelWin*) moved to common/WindowMsg.h, FurnCtlReleaseAll to
// common/ProcCtl.h (ProcCtlReleaseAll) on 2026-10-14: this slot holds several
// FF3 class files (open-items.md).

// CGameClear and its GAME_CLEAR_ helpers (0x0010bba0..0x0010ce28; the
// FurnCtlFindInit / FurnCtlDeleteDrawFlgAll / FurnCtlGetSize names there were
// wrong FF2 matches) moved to ingame/clear/GameClear.h on 2026-10-15.

// CMapDBuff and the special-event puts (SET_SP_EVENT) moved to
// ingame/map/MapDBuff.h on 2026-10-15, CLoad to common/Load.h.

// CLightCtl (the blinking room lights, 0x0010ce28..0x0010d8b8) moved to
// ingame/map/LightCtl.h on 2026-10-15.

// CMarkDat (the map menu's floor layouts) moved to ingame/map/MarkDat.h on
// 2026-10-15, CMapRec to MapRec.h.

// LERP_F, the 2D map draw flags (FurnCtlFlag* -> MarkEventFlag*) and
// FUN_0010f8d8 (MarkEventIsSpecialRoom) moved to ingame/map/MarkEvent.h on
// 2026-10-15; CMarkDoor is in MarkDoor.h.

#endif /* _INGAME_MAP_FURNCTL_H */
