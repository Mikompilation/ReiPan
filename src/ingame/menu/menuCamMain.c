// ingame/menu/menuCamMain.c -- CmenuCamMain, the camera menu: frame, pages,
// HUD. FF3 0x00112150..0x00112db0; FF3-only (the class file sorts between
// CmenuCamBasic and CmenuCamStLens; open-items.md "0x10bbb0..0x116fa0").
#include "ingame/menu/menuCamMain.h"

#include "common/cmn_fade_layer.h"      // GetCmnFadeLayer, capCtl::SetItemHandle, capCtl::Show
#include "common/cmn_util.h"            // STEP_SEQ
#include "graphics/motion/anicode.h"    // B2D_HOOK_REC, B2dSetHookList, MenuWdwSetKeepOpen, MenuWdwSetYesNo
#include "ingame/menu/anim_2d.h"        // LayoutAnim*, LayoutMsgSetKeepOpen, HUD_LAYOUT
#include "ingame/menu/menuCamBase.h"    // CmenuCamBase
#include "ingame/menu/menuCamBasic.h"   // CmenuCamBasic
#include "ingame/menu/menuCamStLens.h"  // CmenuCamStLens
#include "ingame/menu/menuCamStat.h"    // CmenuCamStat
#include "system/b2d/B2dBase.h"         // CB2dBase, B2dBase*
#include "system/b2d/BpCtl.h"           // BpSetDrawGsRegs, B2dDrawSpriteDirect
#include "system/os/system.h"           // GetSysWrk, SYS_WRK::GetLanguage

#include <new>                          // operator new / delete
#include "ingame/menu/menu_mouse.h"    // PC: the mouse in menus

#define MENU_CAM_LAYOUT_ANIM        8   // LayoutAnimStart type
#define MENU_CAM_HUD_ARG            1   // HudLayoutInit's arg
#define MENU_CAM_HUD_A2             0x16    // HUD_LAYOUT.a2 (a B2dCtlSetPos argument)
#define MENU_CAM_FADE_LABEL         6   // capCtl::Show
#define MENU_CAM_ALPHA              0x80

// sdata 0x36b6f0 (gp-0x7e00): the menu the constructor made.
static CmenuCamMain *menu_cam_main;

static int MenuCamMainStepRun(STEP_SEQ *seq);
static int MenuCamMainStepSwitch(STEP_SEQ *seq);
static int MenuCamMainStepSwitchEnd(STEP_SEQ *seq);

// ──────────────────────────────────────────────────────────────────────
// FF3 0x00112290 (MenuCamMainCtor; FF3 0x00112150 is the same body).
// The new expressions (frame, pages, STEP_SEQ, CB2dBase):
// callcheck: operator_new, B2dBaseCtor, MenuCamBaseCtor, MenuCamStatCtor, MenuCamBasicCtor, MenuCamStLensCtor, StepSeqInit

CmenuCamMain::CmenuCamMain()
{
    static const STEP_FUNC steps[] =
    {
        MenuCamMainStepRun, MenuCamMainStepSwitch, MenuCamMainStepSwitchEnd,
        nullptr,
    };

    for (int i = 0; i < MENU_CAM_PAGE_NUM; i++)
        page[i] = nullptr;

    frame = new CmenuCamBase;
    page[MENU_CAM_PAGE_STAT]   = new CmenuCamStat;
    page[MENU_CAM_PAGE_BASIC]  = new CmenuCamBasic;
    page[MENU_CAM_PAGE_STLENS] = new CmenuCamStLens;
    seq = new STEP_SEQ;
    bg = new CB2dBase;

    next  = -1;
    anim  = -1;
    hud   = nullptr;
    cur   = MENU_CAM_PAGE_STAT;
    fresh = 0;
    // PC: FF3 leaves these to operator new; nothing reads them.
    unk_0c[0] = unk_0c[1] = 0;
    seq->Add(steps);
    seq->owner = this;
    menu_cam_main = this;
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x001124c8 (MenuCamMainDtor; the base one 0x001123d0 is the same
// body, the deleting one 0x001125c0 adds operator delete).
// callcheck: HudLayoutRelease, operator_delete (delete hud: its destructor, then operator delete; delete seq)

CmenuCamMain::~CmenuCamMain()
{
    for (int i = 0; i < MENU_CAM_PAGE_NUM; i++)
    {
        if (page[i] != nullptr)
            delete page[i];             // FF3: virtual deleting dtor (vtable +0x04)
    }
    if (frame != nullptr)
        delete frame;

    delete seq;

    if (hud != nullptr)
    {
        delete hud;
    }
    if (bg != nullptr)
        delete bg;                      // FF3: virtual deleting dtor (vtable +0x04)

    if (anim >= 0)
    {
        MenuWdwSetKeepOpen(0);
        LayoutAnimReset(anim);
        anim = -1;
    }
    menu_cam_main = nullptr;
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x001126b8.

CmenuCamBase *CmenuCamMain::GetPage()
{
    return page[cur];
}

// ──────────────────────────────────────────────────────────────────────
// Requests the frame, the current page, the layout animation, the HUD and
// the background. FF3 0x001126d0 (GetSysWrk is cached in sdata gp-0xc60).
// callcheck: operator_new, HudLayoutInit (new HUD_LAYOUT and its constructor)

void CmenuCamMain::Load()
{
    frame->Load(GetSysWrk()->GetLanguage() + MENU_CAM_FRAME_FILE);
    GetPage()->Load();
    anim = LayoutAnimStart(MENU_CAM_LAYOUT_ANIM, 0);

    HUD_LAYOUT *hud = new HUD_LAYOUT(MENU_CAM_HUD_ARG);
    this->hud = hud;
    hud->a2 = MENU_CAM_HUD_A2;
    hud->a1 = 0;

    LayoutAnimSetFlag(anim, 6);
    MenuWdwSetKeepOpen(1);
    MenuWdwSetYesNo(0, 0);
    bg->LoadLast(MENU_CAM_BG_FILE);
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x00112798.

int CmenuCamMain::IsReady()
{
    hud->Main();
    return GetPage()->IsReady() != 0
        && frame->IsReady() != 0
        && LayoutAnimIsDataReady(anim) != 0
        && hud->IsReady() != 0
        && bg->IsReady() != 0;
}

// ──────────────────────────────────────────────────────────────────────
// The frame's MENU_CAM_FADE_ITEM hook. FF3 0x00112818.

static int MenuCamMainFadeHook(const B2D_HOOK_ARGS *args)
{
    BpSetDrawGsRegs();
    B2dDrawSpriteDirect(args);
    return 0;
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x001128e0.

void CmenuCamMain::Open()
{
    static const char label_in[]    = "in";            // sdata 0x36b6f8
    static const char frame_in[]    = "cm_daisi_in";   // 0x34cb18

    GetPage()->SetHooks();
    GetPage()->Play(label_in);
    frame->Play(frame_in);
    GetCmnFadeLayer()->SetItemHandle(frame->FindItem(MENU_CAM_FADE_ITEM));
    GetCmnFadeLayer()->Show(MENU_CAM_FADE_LABEL);
    hud->Play(0);
    LayoutAnimClearFlag(anim, 2);
    LayoutAnimClearFlag(anim, 4);

    const B2D_HOOK_REC hooks[] =
    {
        { MenuCamMainFadeHook, 0, MENU_CAM_FADE_ITEM, nullptr },
        { nullptr, -1, -1, nullptr },
    };
    B2dSetHookList(frame->handle, hooks);
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x001129f8.

void CmenuCamMain::Close()
{
    static const char label_out[]   = "out";           // sdata 0x36b700
    static const char frame_out[]   = "cn_daisi_out";  // 0x34cb28

    GetPage()->Play(label_out);
    frame->Play(frame_out);
    hud->Close();
    if (anim >= 0)
    {
        LayoutMsgSetKeepOpen(0);
        LayoutAnimReset(anim);
        anim = -1;
    }
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x00112a68.

int CmenuCamMain::IsClosed()
{
    return GetPage()->IsPlayEnd() != 0
        && hud->state == HUD_LAYOUT_DONE;
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x00112ab0.

CmenuCamMain *GetMenuCamMain(void)
{
    return menu_cam_main;
}

// ──────────────────────────────────────────────────────────────────────
// Asks for page p; MenuCamMainStepRun starts the switch next frame. A page
// without a layout loads it now. FF3 0x00112ab8.

int CmenuCamMain::SetPage(int p)
{
    next = p;
    CmenuCamBase *pg = page[p];
    if (pg->handle == -1)
    {
        pg->Load();
        fresh = 1;
    }
    return 0;
}

// ──────────────────────────────────────────────────────────────────────
// Step 2: both pages played: the new one is current. FF3 0x00112b18.

static int MenuCamMainStepSwitchEnd(STEP_SEQ *seq)
{
    CmenuCamMain *cm = (CmenuCamMain *)seq->owner;

    if (cm->GetPage()->IsPlayEnd() == 0)
        return STEP_CONT;
    if (cm->page[cm->next]->IsPlayEnd() == 0)
        return STEP_CONT;

    cm->cur  = cm->next;
    cm->next = -1;
    seq->Jump(MenuCamMainStepRun);
    return STEP_NEXT;
}

// ──────────────────────────────────────────────────────────────────────
// Step 1: once the new page's layout is ready the old one plays out and
// the new one in. FF3 0x00112ba0.

static int MenuCamMainStepSwitch(STEP_SEQ *seq)
{
    static const char label_out[]   = "out_etc";       // sdata 0x36b708
    static const char label_in[]    = "in_etc";        // sdata 0x36b710

    CmenuCamMain *cm = (CmenuCamMain *)seq->owner;
    CmenuCamBase *pg = cm->page[cm->next];

    if (pg->IsReady() == 0)
        return STEP_CONT;

    cm->GetPage()->Play(label_out);
    if (cm->fresh != 0)
    {
        pg->SetHooks();
        pg->Setup();
        cm->fresh = 0;
    }
    pg->Play(label_in);
    pg->SetAlpha(MENU_CAM_ALPHA);
    pg->Enter();
    return STEP_NEXT;
}

// ──────────────────────────────────────────────────────────────────────
// Step 0: the page runs until a switch is asked for; a page whose Step
// returns nonzero ends the menu (STEP_WAIT: CmenuCamMain::Run returns 1).
// FF3 0x00112c68.

static int MenuCamMainStepRun(STEP_SEQ *seq)
{
    CmenuCamMain *cm = (CmenuCamMain *)seq->owner;

    if (cm->next != -1)
        return STEP_NEXT;
    if (cm->GetPage()->Step() == 0)
        return STEP_CONT;
    return STEP_WAIT;
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x00112cc0.

void CmenuCamMain::UpdateHud()
{
    if (hud != nullptr)
        hud->Main();
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x00112cf0.

int CmenuCamMain::Run()
{
    // PC: the mouse. The three tabs along the top of every page: the pages step
    // round them with L2 / R2.
    {
        static const ReiPan_MenuBox tabs[MENU_CAM_PAGE_NUM] =
        {
            { 0.0f, 58.0f, 169.0f, 40.0f }, { 169.0f, 58.0f, 158.0f, 40.0f }, { 327.0f, 58.0f, 157.0f, 40.0f },
        };

        for (int i = 0; i < MENU_CAM_PAGE_NUM; i++)
        {
            if (i != cur)
                MenuMouseTab(0, (i - cur + MENU_CAM_PAGE_NUM) % MENU_CAM_PAGE_NUM == 1 ? 1 : -1, &tabs[i],
                             REIPAN_MENU_L2, REIPAN_MENU_R2);
        }
    }

    int ret = seq->Run();
    UpdateHud();
    return ret;
}

// ──────────────────────────────────────────────────────────────────────
// The page's own drawing; while two pages play (step 2) the new one's too.
// FF3 0x00112d30.

void CmenuCamMain::Draw()
{
    GetPage()->Draw();
    if (seq->GetFunc() == MenuCamMainStepSwitchEnd && next != -1)
        page[next]->Draw();
}
