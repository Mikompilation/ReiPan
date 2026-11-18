/* ==========================================================================
 *  ingame/menu/menuCamStat.h
 *
 *  CmenuCamStat (FF3 RTTI, vtable 0x341168, base CmenuCamBase): page 0 of
 *  the camera menu, the one it opens on: the camera status. Rows: film
 *  (type and shots left, left/right picks the film), lenses (equip slots
 *  and the lens list), camera functions (on / off), plus the camera level
 *  pips; left/right on the top level switches to the other pages. Also the
 *  lens table helpers CmenuCamStLens shares. FF3 0x001146a8..0x00116fa0 +
 *  0x002ac1b8..0x002ac330; FF3-only.
 * ======================================================================== */
#ifndef _INGAME_MENU_MENUCAMSTAT_H
#define _INGAME_MENU_MENUCAMSTAT_H

#include "sdk/scetypes.h"

#include <stddef.h>                     // offsetof

#include "common/cmn_util.h"            // CSR_GRID
#include "ingame/map/MarkEvent.h"       // LERP_F
#include "ingame/menu/menuCamBase.h"    // CmenuCamBase
#include "system/b2d/B2dBase.h"         // B2D_SEL

struct FILM_WRK;                        // ingame/photo/fndr_parts.h

// ──────────────────────────────────────────────────────────────────────
// The lens table (data 0x34d968): the camera menu's eight lens rows, a
// lens per player column and the row's layout item.
// ──────────────────────────────────────────────────────────────────────

#define MENU_CAM_LENS_ROW_NUM   8

struct MENU_CAM_LENS_ROW                /* 0xc */
{
    /* 0x00 */ int no[2];               // lens number per MenuCamLensGetColumn (Rei, Kei), -1 none
    /* 0x08 */ int item;                // the row's layout item (its position)
};
static_assert(sizeof(MENU_CAM_LENS_ROW) == 0xc, "MENU_CAM_LENS_ROW is 0xc bytes");

FILM_WRK *MenuCamGetFilmWrk(void);                      // FF3 0x001146a8
int  MenuCamIsLensMultiSlot(void);                      // FF3 0x001146c8: LENS_FUNC_SLOTS owned and on
int  MenuCamLensGetColumn(void);                        // FF3 0x00114720: 0 Rei, 1 Kei, -1 (Miku: no lenses)
int  MenuCamLensGetNo(int row);                         // FF3 0x00114748: -1 none
int  MenuCamLensFindRow(int no);                        // FF3 0x00114798: -1 none
const MENU_CAM_LENS_ROW *MenuCamLensGetRow(int row);    // FF3 0x001147f8
int  MenuCamLensGetRowNum(void);                        // FF3 0x00114818: MENU_CAM_LENS_ROW_NUM

// ──────────────────────────────────────────────────────────────────────
// The page.
// ──────────────────────────────────────────────────────────────────────

// CmenuCamStat.state, also the CSR_GRID of grid[] that moves.
enum MENU_CAM_STAT_STATE
{
    MENU_CAM_STAT_TOP  = 0,             // pick a row (grid 0 y: 1..3), left / right: other pages
    MENU_CAM_STAT_FILM = 1,             // row 1: film select (grid 1 x: -1 / +1)
    MENU_CAM_STAT_SLOT = 2,             // row 2: lens equip slot (grid 2 x: 0..2) with LENS_FUNC_SLOTS
    MENU_CAM_STAT_FUNC = 3,             // row 3: camera functions (grid 3 x: 0..8)
    MENU_CAM_STAT_LENS = 4,             // the lens list (grid 4 x: 0..7)
    MENU_CAM_STAT_NUM  = 5,
};

#define MENU_CAM_STAT_FILE  0x124f      // the page layout, + GetLanguage()

// 0x1ac bytes on the EE (CmenuCamMain news it).
// layout: host -- the vptr is 8 bytes; EE offsets in the comments
// (grid +0x08, blink +0xf8, lerp +0xfc, state +0x124, sel +0x128).
class CmenuCamStat : public CmenuCamBase
{
public:
    CmenuCamStat();                     // FF3 0x00114bf0 (MenuCamStatCtor; twin 0x00114820)
    virtual ~CmenuCamStat();            // EE vtable +0x00 0x002ac2d0, deleting +0x04 0x002ac2f8

    virtual int  Step();                // EE vtable +0x08 0x00115688
    virtual int  Draw();                // EE vtable +0x0c 0x00116f78
    virtual void SetHooks();            // EE vtable +0x10 0x00116eb8
    virtual void Load();                // EE vtable +0x14 0x00115770

    CSR_GRID grid[MENU_CAM_STAT_NUM];   // EE +0x008: per MENU_CAM_STAT_STATE
    int      blink;                     // EE +0x0f8: lerp[] running (0 fade out, 1 fade in)
    LERP_F   lerp[2];                   // EE +0x0fc: the cursors' blink, 1.0 <-> 0.3125 over 40 frames
    int      state;                     // EE +0x124: MENU_CAM_STAT_STATE
    B2D_SEL  sel;                       // EE +0x128: the row highlights (items 0x4c0..0x4c2, cursor 0x4c3)
};
static_assert(offsetof(CmenuCamStat, blink) == offsetof(CmenuCamStat, grid) + MENU_CAM_STAT_NUM * sizeof(CSR_GRID), "CmenuCamStat.blink follows grid[] (EE +0xf8)");
static_assert(offsetof(CmenuCamStat, lerp) == offsetof(CmenuCamStat, blink) + 4, "CmenuCamStat.lerp (EE +0xfc)");
static_assert(offsetof(CmenuCamStat, state) == offsetof(CmenuCamStat, lerp) + 2 * sizeof(LERP_F), "CmenuCamStat.state (EE +0x124)");

CmenuCamStat *GetMenuCamStat(void);     // FF3 0x001157b8: sdata gp-0x7dc0

#endif /* _INGAME_MENU_MENUCAMSTAT_H */
