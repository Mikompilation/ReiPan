/* ==========================================================================
 *  ingame/menu/menuCamStLens.h
 *
 *  CmenuCamStLens (FF3 RTTI, vtable 0x341130, base CmenuCamBase): page 2 of
 *  the camera menu, the lenses: the lenses the player owns as a list (six
 *  rows shown, a scroll bar), each with its level pips, bought up one
 *  level for points (yes/no in the MenuWdw, the "shine" play of the menu's
 *  background layout over the new pip); left / right switch to the basic
 *  or the status page. Without a lens the page only switches pages.
 *  Layout 0x124a + language. FF3 0x00112db0..0x001146a8 + 0x002ac128..
 *  0x002ac1b8; FF3-only.
 * ======================================================================== */
#ifndef _INGAME_MENU_MENUCAMSTLENS_H
#define _INGAME_MENU_MENUCAMSTLENS_H

#include "sdk/scetypes.h"

#include <stddef.h>                     // offsetof

#include "common/cmn_util.h"            // CSR_GRID, STEP_SEQ
#include "ingame/menu/menuCamBase.h"    // CmenuCamBase
#include "ingame/menu/menuCamStat.h"    // MENU_CAM_LENS_ROW_NUM
#include "ingame/menu/menu_csr.h"       // MENU_CSR_WRK

struct B2D_GRID_SPR;                    // system/b2d/B2dGridSpr.h
class  B2d_ScrollBar3;                  // system/b2d/ScrollBar.h

#define MENU_CAM_STLENS_FILE    0x124a  // the page layout, + GetLanguage()
#define MENU_CAM_STLENS_VIEW    6       // list rows shown at once

// 0x100 bytes on the EE (CmenuCamMain news it).
// layout: host -- the vptr is 8 bytes, csr and seq hold pointers; EE
// offsets in the comments (grid +0x34, csr +0x64, seq +0xa0, seq.owner is
// this). Nothing reads them by offset.
class CmenuCamStLens : public CmenuCamBase
{
public:
    CmenuCamStLens();                   // FF3 0x00113fa0 (MenuCamStLensCtor; twin 0x00113db0)
    virtual ~CmenuCamStLens();          // EE vtable +0x00 0x00114240, deleting +0x04 0x001142f0

    virtual int  Step();                // EE vtable +0x08 0x00114620: the scroll bar, seq.Run()
    virtual int  Draw();                // EE vtable +0x0c 0x00114698
    virtual void SetHooks();            // EE vtable +0x10 0x00114520
    virtual void Load();                // EE vtable +0x14 0x001143b0
    virtual void Enter();               // EE vtable +0x18 0x001145a8
    virtual void Setup();               // EE vtable +0x1c 0x001143f8

    MENU_CSR_WRK *Csr();                // the list cursor, nullptr when no lens is owned (inline in FF3)

    int             unk_08;             // EE +0x08: zeroed by the constructor; nothing seen reads it
    int             count;              // EE +0x0c: lenses owned (lens[] entries)
    int             lens[MENU_CAM_LENS_ROW_NUM + 1];    // EE +0x10: the owned lens numbers in menu_cam_lens_row order (+0x30 unused)
    CSR_GRID        grid;               // EE +0x34: x -1 / +1 switch pages (also L2 / R2)
    MENU_CSR_WRK    csr;                // EE +0x64: the list, 1 x count, view MENU_CAM_STLENS_VIEW rows
    STEP_SEQ        seq;                // EE +0xa0: top, confirm, notice, level-up wait, empty
    B2D_GRID_SPR   *csr_spr[2];         // EE +0xf4: the cursor row sprites (items 0x436, 0x438)
    B2d_ScrollBar3 *bar;                // EE +0xfc
};
static_assert(offsetof(CmenuCamStLens, count) == offsetof(CmenuCamStLens, unk_08) + 4, "CmenuCamStLens.count (EE +0x0c)");
static_assert(offsetof(CmenuCamStLens, lens) == offsetof(CmenuCamStLens, count) + 4, "CmenuCamStLens.lens (EE +0x10)");
static_assert(offsetof(CmenuCamStLens, grid) == offsetof(CmenuCamStLens, lens) + 0x24, "CmenuCamStLens.grid (EE +0x34)");

CmenuCamStLens *GetMenuCamStLens(void); // FF3 0x001143a8: sdata 0x36b718 (gp-0x7dd8)

#endif /* _INGAME_MENU_MENUCAMSTLENS_H */
