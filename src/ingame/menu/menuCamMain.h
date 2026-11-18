/* ==========================================================================
 *  ingame/menu/menuCamMain.h
 *
 *  CmenuCamMain (FF3 RTTI, vtable 0x341120): the camera menu (phase 64
 *  StoryMenu_Camera, ingame/story_menu_camera.c; back to 0x39): a frame
 *  layout, three pages (CmenuCamStat / CmenuCamBasic / CmenuCamStLens) that
 *  it switches between with an out_etc / in_etc play, the HUD layout and a
 *  background layout. The phase's steps call Load, IsReady + Open, Run +
 *  Close, IsClosed, and Draw every frame. FF3 0x00112150..0x00112db0;
 *  FF3's C++ rewrite of FF2's menu_cam_main.c (no shared code).
 * ======================================================================== */
#ifndef _INGAME_MENU_MENUCAMMAIN_H
#define _INGAME_MENU_MENUCAMMAIN_H

#include "sdk/scetypes.h"

#include <stddef.h>                     // offsetof

class  CmenuCamBase;                    // ingame/menu/menuCamBase.h
struct CB2dBase;                        // system/b2d/B2dBase.h
struct HUD_LAYOUT;                      // ingame/menu/anim_2d.h
struct STEP_SEQ;                        // common/cmn_util.h

// CmenuCamMain.page[].
enum MENU_CAM_PAGE
{
    MENU_CAM_PAGE_STAT   = 0,           // CmenuCamStat, the page the menu opens on
    MENU_CAM_PAGE_BASIC  = 1,           // CmenuCamBasic
    MENU_CAM_PAGE_STLENS = 2,           // CmenuCamStLens
    MENU_CAM_PAGE_NUM    = 3,
};

#define MENU_CAM_FRAME_FILE 0x1240      // the frame layout, + GetLanguage()
#define MENU_CAM_BG_FILE    0x1254      // the background layout (CB2dBase::LoadLast)
#define MENU_CAM_FADE_ITEM  0x3ee       // the frame item the common fade layer and MenuCamMainFadeHook use

// 0x38 bytes on the EE.
// layout: host -- the vptr and the pointers are 4 bytes on the EE: next is
// at EE +4, frame +0x14, page[] +0x18, anim +0x24, seq +0x2c, hud +0x30,
// bg +0x34.
class CmenuCamMain
{
public:
    CmenuCamMain();                     // FF3 0x00112290 (MenuCamMainCtor; twin 0x00112150)
    virtual ~CmenuCamMain();            // EE vtable +0x00 0x001124c8 (base 0x001123d0), deleting +0x04 0x001125c0

    CmenuCamBase *GetPage();            // FF3 0x001126b8: page[cur]
    void Load();                        // FF3 0x001126d0
    int IsReady();                      // FF3 0x00112798: also runs the HUD
    void Open();                        // FF3 0x001128e0
    void Close();                       // FF3 0x001129f8
    int IsClosed();                     // FF3 0x00112a68
    int SetPage(int page);              // FF3 0x00112ab8: switch to MENU_CAM_PAGE page; 0
    void UpdateHud();                   // FF3 0x00112cc0
    int Run();                          // FF3 0x00112cf0: 1 once a page ended the menu
    void Draw();                        // FF3 0x00112d30

    int           next;                 // EE +0x04: MENU_CAM_PAGE being switched to, -1 none
    int           cur;                  // EE +0x08: MENU_CAM_PAGE shown
    int           unk_0c[2];            // EE +0x0c: not written by the constructor; nothing seen reads them
    CmenuCamBase *frame;                // EE +0x14: the frame layout (a plain CmenuCamBase)
    CmenuCamBase *page[MENU_CAM_PAGE_NUM];  // EE +0x18
    int           anim;                 // EE +0x24: LayoutAnimStart(8) slot, -1 none
    int           fresh;                // EE +0x28: 1: page[next] was just loaded (SetHooks + Setup when it comes in)
    STEP_SEQ     *seq;                  // EE +0x2c: MenuCamMainStepRun / StepSwitch / StepSwitchEnd
    HUD_LAYOUT   *hud;                  // EE +0x30
    CB2dBase     *bg;                   // EE +0x34: MENU_CAM_BG_FILE
};
static_assert(offsetof(CmenuCamMain, cur) == offsetof(CmenuCamMain, next) + 4, "CmenuCamMain.cur follows next");
static_assert(offsetof(CmenuCamMain, frame) == offsetof(CmenuCamMain, next) + 0x10, "CmenuCamMain.frame");
static_assert(offsetof(CmenuCamMain, anim) == offsetof(CmenuCamMain, page) + MENU_CAM_PAGE_NUM * sizeof(void *), "CmenuCamMain.anim follows page[]");
static_assert(offsetof(CmenuCamMain, fresh) == offsetof(CmenuCamMain, anim) + 4, "CmenuCamMain.fresh follows anim");

CmenuCamMain *GetMenuCamMain(void);                 // FF3 0x00112ab0: sdata 0x36b6f0 (gp-0x7e00), nullptr when none

#endif /* _INGAME_MENU_MENUCAMMAIN_H */
