/* ==========================================================================
 *  ingame/menu/menuCamBase.h
 *
 *  CmenuCamBase (FF3 RTTI, vtable 0x3410b0, base CB2dBase): one page of the
 *  camera menu (phase 0x39, opened from the in-game file menu), a Bp layout
 *  with the six calls CmenuCamMain drives it by. The pages derive from it:
 *  CmenuCamStat (status), CmenuCamBasic (basic functions), CmenuCamStLens
 *  (lenses); CmenuCamMain also keeps a plain CmenuCamBase for its frame
 *  layout. FF3 0x001105f0..0x001106d8 + the default virtuals 0x002ac018..
 *  0x002ac048; FF3's C++ rewrite of FF2's menu_cam*.c (no shared code).
 * ======================================================================== */
#ifndef _INGAME_MENU_MENUCAMBASE_H
#define _INGAME_MENU_MENUCAMBASE_H

#include "sdk/scetypes.h"

#include "system/b2d/B2dBase.h"         // CB2dBase

// 8 bytes on the EE: the vptr (shared with CB2dBase), then CB2dBase's handle.
// layout: host -- the vptr is 8 bytes on 64-bit hosts; no own fields.
class CmenuCamBase : public CB2dBase
{
public:
    CmenuCamBase();                     // FF3 0x00110620 (MenuCamBaseCtor; twin 0x001105f0)
    virtual ~CmenuCamBase();            // EE vtable +0x00 0x00110678 (base 0x00110650), deleting +0x04 0x001106a0

    virtual int  Step();                // EE vtable +0x08 (0x002ac018: -1): per frame; nonzero ends the menu
    virtual int  Draw();                // EE vtable +0x0c (0x002ac020: -1): per frame after BpCtlMain
    virtual void SetHooks();            // EE vtable +0x10 (0x002ac028): item hook lists, on a fresh layout and on open
    virtual void Load();                // EE vtable +0x14 (0x002ac030): CB2dBase::Load of the page's layout
    using CB2dBase::Load;               // Load(file) stays visible beside the virtual
    virtual void Enter();               // EE vtable +0x18 (0x002ac038): each time the page comes in
    virtual void Setup();               // EE vtable +0x1c (0x002ac040): the first entry after a load, after SetHooks
};

#endif /* _INGAME_MENU_MENUCAMBASE_H */
