/* ==========================================================================
 *  ingame/menu/menuCamBasic.h
 *
 *  CmenuCamBasic (FF3 RTTI, vtable 0x3410e8, base CmenuCamBase): page 1 of
 *  the camera menu, the basic functions: the camera level rows (charge /
 *  finder / range and the player's two special abilities) as pips, each
 *  bought up one level for points (yes/no in the MenuWdw, the "shine" play
 *  of the menu's background layout over the new pip). Left / right on it
 *  switch to the status page or the lens page (layout 0x1245 + language).
 *  Also the two helpers the other pages call. FF3 0x001106d8..0x00112150 +
 *  0x002ac048..0x002ac128; FF3-only.
 * ======================================================================== */
#ifndef _INGAME_MENU_MENUCAMBASIC_H
#define _INGAME_MENU_MENUCAMBASIC_H

#include "sdk/scetypes.h"

#include "common/cmn_util.h"            // CSR_GRID, STEP_SEQ
#include "ingame/menu/menuCamBase.h"    // CmenuCamBase

#define MENU_CAM_BASIC_FILE 0x1245      // the page layout, + GetLanguage()

// 0x8c bytes on the EE (CmenuCamMain news it).
// layout: host -- the vptr is 8 bytes and seq holds pointers (aligned
// after grid on 64-bit hosts); EE offsets in the comments (grid +0x08,
// seq +0x38; seq.owner is this). Nothing reads them by offset.
class CmenuCamBasic : public CmenuCamBase
{
public:
    CmenuCamBasic();                    // FF3 0x00110d60 (MenuCamBasicCtor; twin 0x00110c48)
    virtual ~CmenuCamBasic();           // EE vtable +0x00 0x00110ea0 (base 0x00110e78), deleting +0x04 0x00110ec8

    virtual int  Step();                // EE vtable +0x08 0x00110f08: seq.Run()
    virtual int  Draw();                // EE vtable +0x0c 0x001120b8
    virtual void SetHooks();            // EE vtable +0x10 0x00111fc8
    virtual void Load();                // EE vtable +0x14 0x00112070

    CSR_GRID grid;                      // EE +0x08: x -1 / +1 switch pages, y the level row (0..2 + the special slots)
    STEP_SEQ seq;                       // EE +0x38: top, confirm, notice, level-up wait
};

CmenuCamBasic *GetMenuCamBasic(void);   // FF3 0x00110f00: sdata 0x36b6e4 (gp-0x7e0c)

// Helpers the other pages call.
struct LENS_WRK;                        // ingame/photo/fndr_parts.h

LENS_WRK *MenuCamGetLensWrk(void);                                  // FF3 0x001120c8: GetPhotoModeWrk(), GetLensWrk()
void MenuWdwSetMsgByTable(const int *pairs, int msg0, int key);     // FF3 0x001120e8: {key, msg1} pairs ended by key < 0

#endif /* _INGAME_MENU_MENUCAMBASIC_H */
