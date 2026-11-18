// ingame/menu/menuCamStLens.c -- CmenuCamStLens, the camera menu's lens
// page (the owned lenses, their levels bought up for points).
// FF3 0x00112db0..0x001146a8 + 0x002ac128..0x002ac1b8; FF3-only (the class
// file sorts between CmenuCamMain and CmenuCamStat; open-items.md
// "0x10bbb0..0x116fa0").
#include "ingame/menu/menuCamStLens.h"

#include <string.h>                     // strcmp, strlen

#include "common/cmn_util.h"            // CSR_GRID, STEP_SEQ, UpCostGet
#include "graphics/motion/anicode.h"    // B2D_HOOK_REC, B2dSetHookList, MenuWdwSetMsg, MenuWdwSetYesNo, MenuWdwGetResult
#include "ingame/ingame_wrk.h"          // ingame_wrk, IngameWrkGetPoint, IngameWrkSubPoint
#include "ingame/menu/menuCamBasic.h"   // MenuCamGetLensWrk
#include "ingame/menu/menuCamMain.h"    // GetMenuCamMain, CmenuCamMain::SetPage, MENU_CAM_PAGE_*
#include "ingame/menu/menuCamStat.h"    // MenuCamLensGetRow, MenuCamLensGetRowNum
#include "ingame/menu/menu_csr_obj.h"   // MenuCsrObj*
#include "ingame/photo/fndr_parts.h"    // LENS_WRK, LENS_LEVEL_MAX
#include "ingame/sound/snd_ctrl.h"      // SndSePlay
#include "system/b2d/B2dBase.h"         // CB2dBase, B2D_PLACE, B2dNumDrawFont
#include "system/b2d/B2dCtl.h"          // B2D_HOOK_ARGS, B2D_ITEM, B2dGetDrawItem
#include "system/b2d/B2dGridSpr.h"      // B2D_GRID_SPR
#include "system/b2d/BpCtl.h"           // BpGetDrawHandle, BpSetBlendAlpha, B2dDrawSpriteDirect
#include "system/b2d/ScrollBar.h"       // B2d_ScrollBar3
#include "system/os/system.h"           // GetSysWrk, SYS_WRK::GetLanguage
#include "system/pad/pad_ctrl.h"        // pad_key, CheckPadTrgPlaySe
#include "ingame/menu/menu_mouse.h"    // PC: the mouse in menus

#define MENU_CAM_STLENS_MSG0        0x68    // MenuWdwSetMsg's msg0 (the camera menu's help texts)
#define MENU_CAM_STLENS_SE_MOVE     0x1a9b0000
#define MENU_CAM_STLENS_SE_CANCEL   0x1a9b0001
#define MENU_CAM_STLENS_SE_OK       0x1a9b0003
#define MENU_CAM_STLENS_SE_PAGE     0x1a9b0005
#define MENU_CAM_STLENS_SE_BUY      0x1a9b0006
#define MENU_CAM_STLENS_CHR         0xff    // Lens* chr: the current player
#define MENU_CAM_STLENS_LV_UP_FRAME 0x22    // the level-up wait raises the level on this frame of the shine
#define MENU_CAM_STLENS_CSR_ROW_DY  0x1f    // the cursor sprites' row step

// The level-up messages (MenuWdwSetMsg msg1 under MENU_CAM_STLENS_MSG0).
#define MENU_CAM_STLENS_MSG_CONFIRM  10     // "spend the points?" with the yes/no
#define MENU_CAM_STLENS_MSG_NO_POINT 0xb    // not enough points
#define MENU_CAM_STLENS_MSG_MAX      0xc    // already at the top level

// ──────────────────────────────────────────────────────────────────────
// The lenses' name items and help texts (data 0x34cf68), by lens number.
// layout: host -- name is a pointer; 0xc bytes on the EE.
// ──────────────────────────────────────────────────────────────────────

struct MENU_CAM_STLENS_NAME
{
    const char *name;                   // the lens's name item, nullptr ends
    int         lens;                   // the lens number, -1 ends
    int         msg;                    // its help text (msg1 under MENU_CAM_STLENS_MSG0)
};

#define MENU_CAM_STLENS_NAME_NUM 9

static const MENU_CAM_STLENS_NAME menu_cam_stlens_name[MENU_CAM_STLENS_NAME_NUM + 1] =
{
    { "R renz_zero_s",   1, 0x0e },
    { "R renz_stn_s",    8, 0x15 },
    { "R renz_slw_s",    6, 0x13 },
    { "R renz_stp_s",    5, 0x12 },
    { "R renz_crush_s",  2, 0x0f },
    { "R renz_blast_s",  0, 0x0d },
    { "R renz_serial_s", 3, 0x10 },
    { "R renz_see_s",    4, 0x11 },
    { "R renz_track_s",  7, 0x14 },
    { nullptr, -1, -1 },
};

// The view rows' first pip items (data 0x34cf28) and name slots (data
// 0x34cf48), -1 ends.
static const int menu_cam_stlens_pip_item[MENU_CAM_STLENS_VIEW + 1]  = { 0x47d, 0x480, 0x483, 0x486, 0x489, 0x48c, -1 };
static const int menu_cam_stlens_name_item[MENU_CAM_STLENS_VIEW + 1] = { 0x491, 0x492, 0x493, 0x494, 0x495, 0x496, -1 };

// The view rows (data 0x34d040): the cost number's text item, the row,
// the level digit's item and the R_pts_r_* place.
// layout: host -- two pointers; 0x10 bytes on the EE (num +0x00, row
// +0x04, lv_item +0x08, pts +0x0c).
struct MENU_CAM_STLENS_ROW
{
    const char *num;                    // Num_Nbuy06_renz_R, nullptr ends
    int         row;                    // the view row, -1 ends
    int         lv_item;                // where R_lve_r_0N goes
    const char *pts;                    // the item R_pts_r_01 / 03 is put at
};

static const MENU_CAM_STLENS_ROW menu_cam_stlens_row[MENU_CAM_STLENS_VIEW + 1] =
{
    { "Num_1buy06_renz_R", 0, 0x476, "1buy_renz_R" },
    { "Num_2buy06_renz_R", 1, 0x477, "2buy_renz_R" },
    { "Num_3buy06_renz_R", 2, 0x478, "3buy_renz_R" },
    { "Num_4buy06_renz_R", 3, 0x479, "4buy_renz_R" },
    { "Num_5buy06_renz_R", 4, 0x47a, "5buy_renz_R" },
    { "Num_6buy06_renz_R", 5, 0x47b, "6buy_renz_R" },
    { nullptr, -1, -1, nullptr },
};

// sdata 0x36b718 (gp-0x7dd8): the page the constructor made (its hooks read
// it); sdata 0x36b71c (gp-0x7dd4): the level-up wait's frame count.
static CmenuCamStLens *menu_cam_stlens;
static int             menu_cam_stlens_lv_up_frame;

static int MenuCamStLensStepTop(STEP_SEQ *seq);
static int MenuCamStLensStepConfirm(STEP_SEQ *seq);
static int MenuCamStLensStepNotice(STEP_SEQ *seq);
static int MenuCamStLensStepLvUp(STEP_SEQ *seq);
static int MenuCamStLensStepEmpty(STEP_SEQ *seq);

// ──────────────────────────────────────────────────────────────────────
// The row by its cost number's item name (an out-of-line copy at the end
// of .text); nullptr when missing. FF3 0x002ac128.

static const MENU_CAM_STLENS_ROW *MenuCamStLensFindRowByNum(const MENU_CAM_STLENS_ROW *tbl, const char *const *name)
{
    for (; tbl->row != -1; tbl++)
    {
        if (strcmp(*name, tbl->num) == 0)
            return tbl;
    }
    return nullptr;
}

// ══════════════════════════════════════════════════════════════════════
// Small helpers
// ══════════════════════════════════════════════════════════════════════

MENU_CSR_WRK *CmenuCamStLens::Csr()
{
    return count > 0 ? &csr : nullptr;
}

// ──────────────────────────────────────────────────────────────────────
// The lens on view row row (the list's top + row); -1 past the list or
// without one. FF3 0x00112db0.

static int MenuCamStLensGetViewLens(int row)
{
    CmenuCamStLens *sl = GetMenuCamStLens();
    MENU_CSR_WRK   *cw = sl->Csr();

    if (cw == nullptr)
        return -1;
    int i = MenuCsrObjGetTopY(cw) + row;
    if (i >= sl->count)
        return -1;
    return sl->lens[i];
}

// FF3 0x00112e38.
static int MenuCamStLensGetLevel(int lens)
{
    return MenuCamGetLensWrk()->GetLevel((u_char)lens, MENU_CAM_STLENS_CHR);
}

// FF3 0x00112e70.
static void MenuCamStLensLevelUp(int lens)
{
    MenuCamGetLensWrk()->LevelUp((u_char)lens, MENU_CAM_STLENS_CHR);
}

// The lens numbers are UpCostGet ids 0..8. FF3 0x00112ea8.
static int MenuCamStLensGetCost(int lens, int lv)
{
    return UpCostGet(ingame_wrk.b00, lens, lv);
}

// Left / right on the page grid: the basic page (-1) or the status page
// (+1), with the page SE; the grid goes back to 0. Shared tail of the top
// and the empty step.
static void MenuCamStLensSwitchPage(CmenuCamStLens *sl, int page)
{
    SndSePlay(MENU_CAM_STLENS_SE_PAGE, 100, 0x1000, 0, nullptr, 0);
    if (page == -1)
        GetMenuCamMain()->SetPage(MENU_CAM_PAGE_BASIC);
    else if (page == 1)
        GetMenuCamMain()->SetPage(MENU_CAM_PAGE_STAT);
    sl->grid.SetPos(0, 0);
}

// ══════════════════════════════════════════════════════════════════════
// The page
// ══════════════════════════════════════════════════════════════════════

// ──────────────────────────────────────────────────────────────────────
// FF3 0x00113fa0 (MenuCamStLensCtor; FF3 0x00113db0 is the same body).
// Lists the owned lenses in menu_cam_lens_row order (the player's
// column); without one the sequencer starts on the empty step.
// callcheck: MenuCamBaseCtorBase (the base class constructor)
// callcheck: CsrGridInit, StepSeqInit (grid's and seq's constructors, implicit)
// callcheck: LensIsHave (LENS_WRK::IsHave)

CmenuCamStLens::CmenuCamStLens()
    : unk_08(0), count(0), lens(), csr(), csr_spr(), bar(nullptr)
{
    static const STEP_FUNC steps[] =    // FF3 builds the list on the stack
    {
        MenuCamStLensStepTop, MenuCamStLensStepConfirm, MenuCamStLensStepNotice, MenuCamStLensStepLvUp,
        MenuCamStLensStepEmpty,
        nullptr,
    };

    // x: -1 / +1 switch pages (also L2 / R2), silently.
    grid.SetRange(0, -1, 1);
    grid.SetPos(0, 0);
    grid.flags |= CSR_GRID_LR2;
    seq.Add(steps);
    seq.owner = this;

    LENS_WRK *lw     = MenuCamGetLensWrk();
    int       column = ingame_wrk.b00 != 0;     // FF3: not MenuCamLensGetColumn (Miku reads Kei's column)
    int       rows   = MenuCamLensGetRowNum();

    count = 0;
    for (int r = 0; r < rows; r++)
    {
        int no = MenuCamLensGetRow(r)->no[column];
        if (no < 0)
            break;
        if (lw->IsHave((u_char)no, MENU_CAM_STLENS_CHR) != 0)
            lens[count++] = no;
    }

    int view = count;
    if (view > MENU_CAM_STLENS_VIEW)
        view = MENU_CAM_STLENS_VIEW;
    if (count < 1)
        seq.Jump(MenuCamStLensStepEmpty);
    else
    {
        MenuCsrObjInit(&csr, 1, (short)count, 0);
        MenuCsrObjSetViewX(&csr, 0, 0);
        MenuCsrObjSetViewY(&csr, (short)view, 0);
    }
    menu_cam_stlens = this;
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x00114240 (MenuCamStLensDtor; the deleting one 0x001142f0 adds
// operator delete). The singleton is left as it is.
// callcheck: B2dGridSprDtor, operator_delete (delete csr_spr[])
// callcheck: MenuCamBaseDtorBase (the base class destructor)

CmenuCamStLens::~CmenuCamStLens()
{
    for (int i = 0; i < 2; i++)
    {
        if (csr_spr[i] != nullptr)
        {
            delete csr_spr[i];
            csr_spr[i] = nullptr;
        }
    }
    if (bar != nullptr)
    {
        delete bar;                     // the deleting destructor, EE vtable +0x0c
        bar = nullptr;
    }
    if (count > 0)
        MenuCsrObjRelease(&csr);
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x001143a8.

CmenuCamStLens *GetMenuCamStLens(void)
{
    return menu_cam_stlens;
}

// ──────────────────────────────────────────────────────────────────────
// Per frame: the scroll bar, then the step sequence; 1 (cancel on the top
// or the empty step) ends the menu. FF3 0x00114620 (tail-calls StepSeqRun).

// callcheck: B2dScrollBar3Draw (B2d_ScrollBar3::Draw)
int CmenuCamStLens::Step()
{
    if (count > 0)
        bar->Draw(MenuCsrObjGetViewPosY(&csr), MenuCsrObjGetTopY(&csr));
    return seq.Run();
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x00114698. (FF3 0x001146a0 after it, an empty void function, is
// unreferenced.)

int CmenuCamStLens::Draw()
{
    return 0;
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x001143b0 (GetSysWrk is cached in sdata 0x372890).

void CmenuCamStLens::Load()
{
    CB2dBase::Load(GetSysWrk()->GetLanguage() + MENU_CAM_STLENS_FILE);
}

// ──────────────────────────────────────────────────────────────────────
// The first entry after a load: the two cursor row sprites (at the grid
// origin, a row 0x1f pixels down per list row), the in_etc play and the
// scroll bar. FF3 0x001143f8 (GetSysWrk cached in sdata gp-0xc60).

// callcheck: operator_new, B2dGridSprCtor (new B2D_GRID_SPR)
// callcheck: B2dScrollBar3Ctor (new B2d_ScrollBar3)
// callcheck: B2dBasePlay (CB2dBase::Play)
void CmenuCamStLens::Setup()
{
    static const int spr_item[2] = { 0x436, 0x438 };

    for (int i = 0; i < 2; i++)
    {
        B2D_GRID_SPR *spr = new B2D_GRID_SPR(GetSysWrk()->GetLanguage() + MENU_CAM_STLENS_FILE, spr_item[i],
                                             0, MENU_CAM_STLENS_CSR_ROW_DY);
        csr_spr[i] = spr;
        spr->y = 0;
        spr->x = 0;
    }
    Play("in_etc");                     // sdata 0x36b728
    bar = new B2d_ScrollBar3(GetSysWrk()->GetLanguage() + MENU_CAM_STLENS_FILE, 0x406, 0x3f8, 0x3f7, 0x3f6);
}

// ──────────────────────────────────────────────────────────────────────
// Each time the page comes in: the scroll bar's range and knob. Runs on an
// empty list too (the view functions answer -1 for the cleared cursor).
// FF3 0x001145a8.

// callcheck: B2dScrollBar3Draw (B2d_ScrollBar3::Draw)
void CmenuCamStLens::Enter()
{
    bar->SetRange((short)count, MENU_CAM_STLENS_VIEW);
    bar->Draw(MenuCsrObjGetViewPosY(&csr), MenuCsrObjGetTopY(&csr));
}

// ══════════════════════════════════════════════════════════════════════
// Steps (in the constructor's list). The sequencer's owner is the page.
// ══════════════════════════════════════════════════════════════════════

// ──────────────────────────────────────────────────────────────────────
// No lens owned: left / right switch pages, cancel ends the menu.
// FF3 0x00112ed0.

// callcheck: SndSePlay, GetMenuCamMain, MenuCamMainSetPage, CsrGridSetPos (MenuCamStLensSwitchPage)
static int MenuCamStLensStepEmpty(STEP_SEQ *seq)
{
    CmenuCamStLens *sl = (CmenuCamStLens *)seq->owner;

    sl->grid.Move();
    int page = sl->grid.axis[0].cur;
    if (page == 0)
    {
        if (CheckPadTrgPlaySe((u_short)pad_key[PAD_KEY_CANCEL], -1, 0) != 0)
            return STEP_WAIT;           // Run returns 1: the menu closes
        return STEP_CONT;
    }
    MenuCamStLensSwitchPage(sl, page);
    return STEP_CONT;
}

// ──────────────────────────────────────────────────────────────────────
// Top: the list cursor (with its SE), the lens's help text, the cursor row
// sprites; left / right switch pages (not on a frame the list cursor
// moved); OK on a lens asks to buy its next level (or says it is at the
// top / the points are short), cancel ends the menu. FF3 0x00112fa8.

// callcheck: SndSePlay, GetMenuCamMain, MenuCamMainSetPage, CsrGridSetPos (MenuCamStLensSwitchPage)
static int MenuCamStLensStepTop(STEP_SEQ *seq)
{
    CmenuCamStLens *sl = (CmenuCamStLens *)seq->owner;
    MENU_CSR_WRK   *cw = sl->Csr();

    if (cw == nullptr)
    {
        seq->Jump(MenuCamStLensStepEmpty);
        return STEP_NEXT;
    }

    MenuMouseCsrGrid(cw, sl->csr_spr[0], sl->handle);   // PC
    MenuCsrObjUpdate(cw, 0);
    MenuCsrObjIsMovedSe(cw, MENU_CAM_STLENS_SE_MOVE);
    int pos = MenuCsrObjGetViewPosY(cw);
    int top = MenuCsrObjGetTopY(cw);

    for (const MENU_CAM_STLENS_NAME *n = menu_cam_stlens_name; n->lens >= 0; n++)
    {
        if (n->lens == sl->lens[pos + top])
        {
            MenuWdwSetMsg(MENU_CAM_STLENS_MSG0, n->msg);
            break;
        }
    }
    sl->csr_spr[0]->SetPos(0, (short)pos);
    sl->csr_spr[1]->SetPos(0, (short)pos);

    sl->grid.Move();
    int page = sl->grid.axis[0].cur;
    if (cw->item.flags & CSR_EV_MOVED)
        return STEP_CONT;

    if (page != 0)
    {
        MenuCamStLensSwitchPage(sl, page);
        return STEP_CONT;
    }

    if (CheckPadTrgPlaySe((u_short)pad_key[PAD_KEY_OK], MENU_CAM_STLENS_SE_OK, 0) != 0)
    {
        int lens = MenuCamStLensGetViewLens(pos);
        int lv   = MenuCamStLensGetLevel(lens);
        int msg;

        if (lv == LENS_LEVEL_MAX)
            msg = MENU_CAM_STLENS_MSG_MAX;
        else
            msg = (int)IngameWrkGetPoint() < MenuCamStLensGetCost(lens, lv) ? MENU_CAM_STLENS_MSG_NO_POINT : -1;

        if (msg < 0)
        {
            MenuWdwSetYesNo(1, 0);
            MenuWdwSetMsg(MENU_CAM_STLENS_MSG0, MENU_CAM_STLENS_MSG_CONFIRM);
            seq->Jump(MenuCamStLensStepConfirm);
        }
        else
        {
            MenuWdwSetMsg(MENU_CAM_STLENS_MSG0, msg);
            seq->Jump(MenuCamStLensStepNotice);
        }
        return STEP_NEXT;
    }
    if (CheckPadTrgPlaySe((u_short)pad_key[PAD_KEY_CANCEL], MENU_CAM_STLENS_SE_CANCEL, 0) != 0)
        return STEP_WAIT;               // Run returns 1: the menu closes
    return STEP_CONT;
}

// ──────────────────────────────────────────────────────────────────────
// The yes/no: yes plays the shine on the menu's background layout over the
// lens's next pip, takes the points and waits for the level-up; no or
// cancel go back to the top. FF3 0x00113358.

// callcheck: B2dPlaceInit (B2D_PLACE's constructor)
static int MenuCamStLensStepConfirm(STEP_SEQ *seq)
{
    int res = MenuWdwGetResult();

    if (res == MENU_WDW_RESULT_YES)
    {
        MenuWdwSetYesNo(0, 0);
        CB2dBase *bg = GetMenuCamMain()->bg;
        bg->Play("shine");              // sdata 0x36b720

        CmenuCamStLens *sl   = (CmenuCamStLens *)seq->owner;
        int             pos  = MenuCsrObjGetViewPosY(sl->Csr());
        int             lens = MenuCamStLensGetViewLens(pos);
        int             lv   = MenuCamStLensGetLevel(lens);
        int             xy[2];

        B2D_PLACE place(sl->handle);
        int item = menu_cam_stlens_pip_item[pos];
        place.GetXY(item, xy);
        int w = place.GetW(item);
        bg->SetPos(xy[0] + w * lv - 12, xy[1] - 11);

        IngameWrkSubPoint(MenuCamStLensGetCost(lens, lv));
        menu_cam_stlens_lv_up_frame = 0;
        SndSePlay(MENU_CAM_STLENS_SE_BUY, 100, 0x1000, 0, nullptr, 0);
        seq->Jump(MenuCamStLensStepLvUp);
        return STEP_NEXT;
    }
    if (res < MENU_WDW_RESULT_YES || res > MENU_WDW_RESULT_CANCEL)
        return STEP_CONT;

    SndSePlay(MENU_CAM_STLENS_SE_CANCEL, 100, 0x1000, 0, nullptr, 0);
    MenuWdwSetYesNo(0, 0);
    seq->Jump(MenuCamStLensStepTop);
    return STEP_NEXT;
}

// ──────────────────────────────────────────────────────────────────────
// The "top level" / "not enough points" message: OK or cancel go back.
// FF3 0x00113240.

static int MenuCamStLensStepNotice(STEP_SEQ *seq)
{
    ReiPan_MenuMouseConfirm();          // PC: a click acknowledges
    if (CheckPadTrgPlaySe((u_short)(pad_key[PAD_KEY_CANCEL] | pad_key[PAD_KEY_OK]), -1, 0) == 0)
        return STEP_CONT;
    SndSePlay(MENU_CAM_STLENS_SE_CANCEL, 100, 0x1000, 0, nullptr, 0);
    seq->Jump(MenuCamStLensStepTop);
    return STEP_NEXT;
}

// ──────────────────────────────────────────────────────────────────────
// Waits for the shine to end; the lens levels up on its frame 0x22, so the
// pip fills under it. FF3 0x001132c8.

static int MenuCamStLensStepLvUp(STEP_SEQ *seq)
{
    if (GetMenuCamMain()->bg->IsPlayEnd() == 0)
    {
        if (++menu_cam_stlens_lv_up_frame == MENU_CAM_STLENS_LV_UP_FRAME)
        {
            CmenuCamStLens *sl = (CmenuCamStLens *)seq->owner;
            MenuCamStLensLevelUp(MenuCamStLensGetViewLens(MenuCsrObjGetViewPosY(sl->Csr())));
        }
        return STEP_CONT;
    }
    seq->Jump(MenuCamStLensStepTop);
    return STEP_NEXT;
}

// ══════════════════════════════════════════════════════════════════════
// Item hooks (list 0x34cd78). Sprite hooks: t1, t2 x, y; the two number
// hooks are text-item hooks: t1 the corners xy[8], stack[2] alpha,
// stack[3] the blend flags.
// ══════════════════════════════════════════════════════════════════════

// Draws the hook's sprite at (x, y).
static void MenuCamStLensDrawAt(const B2D_HOOK_ARGS *args, int x, int y)
{
    B2D_HOOK_ARGS a = *args;
    a.t1 = x;
    a.t2 = y;
    B2dDrawSpriteDirect(&a);
}

// ──────────────────────────────────────────────────────────────────────
// num_point06_renz_R: the points the player has. FF3 0x00113508.

static int MenuCamStLensPointNumHook(const B2D_HOOK_ARGS *args)
{
    const int *xy = (const int *)args->t1;

    BpSetBlendAlpha((u_int)args->stack[3]);
    B2dNumDrawFont((int)IngameWrkGetPoint(), BpGetDrawHandle(), "R_num_point_r_0", xy[4], xy[5],
                   (u_char)args->stack[2], 0);      // 0x34cb38
    return 0;
}

// ──────────────────────────────────────────────────────────────────────
// Num_1buy06_renz_R..Num_6buy06_renz_R: the cost of the view row's lens's
// next level (0 at the top), in the bright digits on the cursor row.
// FF3 0x00113570.

static int MenuCamStLensCostNumHook(const B2D_HOOK_ARGS *args)
{
    const int *xy = (const int *)args->t1;

    B2D_ITEM *item = B2dGetDrawItem();
    const MENU_CAM_STLENS_ROW *r = MenuCamStLensFindRowByNum(menu_cam_stlens_row, &item->name);
    if (r == nullptr)
        return 0;                       // PC: FF3 reads the row through the nullptr

    int lens = MenuCamStLensGetViewLens(r->row);
    if (lens < 0)
        return 0;

    int cost = 0;
    int lv   = MenuCamStLensGetLevel(lens);
    if (lv < LENS_LEVEL_MAX)
        cost = MenuCamStLensGetCost(lens, lv);

    MENU_CSR_WRK *cw = GetMenuCamStLens()->Csr();
    if (cw == nullptr)
        return 0;
    const char *font = r->row == MenuCsrObjGetViewPosY(cw) ? "R_num_buy_r_0" : "R_num_select_r_0";
    BpSetBlendAlpha((u_int)args->stack[3]);
    B2dNumDrawFont(cost, BpGetDrawHandle(), font, xy[4], xy[5], (u_char)args->stack[2], 0);
    return 0;
}

// ──────────────────────────────────────────────────────────────────────
// R_tama_r_kuro: the empty pips after the level, up to LENS_LEVEL_MAX, on
// each view row. FF3 0x00113670.

// callcheck: B2dPlaceInit (B2D_PLACE's constructor)
// callcheck: B2dDrawSpriteDirect (MenuCamStLensDrawAt)
static int MenuCamStLensPipDarkHook(const B2D_HOOK_ARGS *args)
{
    int xy[2];

    B2D_PLACE place(BpGetDrawHandle());
    int i = 0;
    do
    {
        int lens = MenuCamStLensGetViewLens(i);
        if (lens < 0)
            return 0;
        int lv = MenuCamStLensGetLevel(lens);
        place.GetXY(menu_cam_stlens_pip_item[i], xy);
        int w = place.GetW(menu_cam_stlens_pip_item[i]);
        xy[0] += w * lv;
        for (; lv < LENS_LEVEL_MAX; lv++)
        {
            MenuCamStLensDrawAt(args, xy[0], xy[1]);
            xy[0] += w;
        }
        i++;
    } while (menu_cam_stlens_pip_item[i] >= 0);
    return 0;
}

// ──────────────────────────────────────────────────────────────────────
// R_tama_r_ao: a filled pip per level on each view row. FF3 0x001137e0.

// callcheck: B2dPlaceInit (B2D_PLACE's constructor)
// callcheck: B2dDrawSpriteDirect (MenuCamStLensDrawAt)
static int MenuCamStLensPipLitHook(const B2D_HOOK_ARGS *args)
{
    int xy[2];

    B2D_PLACE place(BpGetDrawHandle());
    int i = 0;
    do
    {
        int lens = MenuCamStLensGetViewLens(i);
        if (lens < 0)
            return 0;
        int lv = MenuCamStLensGetLevel(lens);
        place.GetXY(menu_cam_stlens_pip_item[i], xy);
        int w = place.GetW(menu_cam_stlens_pip_item[i]);
        for (; lv > 0; lv--)
        {
            MenuCamStLensDrawAt(args, xy[0], xy[1]);
            xy[0] += w;
        }
        i++;
    } while (menu_cam_stlens_pip_item[i] >= 0);
    return 0;
}

// ──────────────────────────────────────────────────────────────────────
// R renz_*_s: the lens's name, on the view row that holds it (nothing
// when it is not owned or scrolled out). FF3 0x00113948.

// callcheck: B2dPlaceInit (B2D_PLACE's constructor)
// callcheck: B2dDrawSpriteDirect (MenuCamStLensDrawAt)
static int MenuCamStLensNameHook(const B2D_HOOK_ARGS *args)
{
    int xy[2];

    B2D_PLACE place(BpGetDrawHandle());
    B2D_ITEM *item = B2dGetDrawItem();
    int       no   = -1;

    for (int i = 0; i < MENU_CAM_STLENS_NAME_NUM; i++)
    {
        if (strcmp(item->name, menu_cam_stlens_name[i].name) == 0)
        {
            no = menu_cam_stlens_name[i].lens;
            break;
        }
    }

    CmenuCamStLens *sl = GetMenuCamStLens();
    MENU_CSR_WRK   *cw = sl->Csr();
    if (cw == nullptr)
        return 0;
    int top = MenuCsrObjGetTopY(cw);
    for (int i = 0; i < MENU_CAM_STLENS_VIEW; i++)
    {
        if (top + i >= sl->count)
            return 0;
        if (sl->lens[top + i] == no)
        {
            place.GetXY(menu_cam_stlens_name_item[i], xy);
            MenuCamStLensDrawAt(args, xy[0], xy[1]);
            return 0;
        }
    }
    return 0;
}

// ──────────────────────────────────────────────────────────────────────
// R_pts_r_01 on the cursor row, R_pts_r_03 on the other view rows.
// FF3 0x00113af0.

// callcheck: B2dPlaceInit (B2D_PLACE's constructor)
// callcheck: B2dDrawSpriteDirect (MenuCamStLensDrawAt)
static int MenuCamStLensPtsHook(const B2D_HOOK_ARGS *args)
{
    int xy[2];

    CmenuCamStLens *sl = GetMenuCamStLens();
    MENU_CSR_WRK   *cw = sl->Csr();
    if (cw == nullptr)
        return 0;

    int top = MenuCsrObjGetTopY(cw);
    int pos = MenuCsrObjGetViewPosY(cw);
    int sel = strcmp("R_pts_r_01", B2dGetDrawItem()->name) == 0;     // 0x34cb70
    B2D_PLACE place(BpGetDrawHandle());

    for (int i = 0; i < MENU_CAM_STLENS_VIEW; i++)
    {
        if (top + i >= sl->count)
            return 0;
        if ((i == pos) == (sel != 0))
        {
            place.GetXYByName(menu_cam_stlens_row[i].pts, xy);
            MenuCamStLensDrawAt(args, xy[0], xy[1]);
        }
    }
    return 0;
}

// ──────────────────────────────────────────────────────────────────────
// R_lve_r_00..05: the level digit N, on every view row whose lens is at
// level N. FF3 0x00113c70.

// callcheck: B2dPlaceInit (B2D_PLACE's constructor)
// callcheck: B2dDrawSpriteDirect (MenuCamStLensDrawAt)
static int MenuCamStLensLvNumHook(const B2D_HOOK_ARGS *args)
{
    int xy[2];

    B2D_PLACE place(BpGetDrawHandle());
    B2D_ITEM *item = B2dGetDrawItem();
    int       n    = item->name[strlen(item->name) - 1] - '0';

    for (int i = 0; i < MENU_CAM_STLENS_VIEW; i++)
    {
        int lens = MenuCamStLensGetViewLens(i);
        if (lens < 0)
            return 0;
        if (MenuCamStLensGetLevel(lens) == n)
        {
            place.GetXY(menu_cam_stlens_row[i].lv_item, xy);
            MenuCamStLensDrawAt(args, xy[0], xy[1]);
        }
    }
    return 0;
}

// ──────────────────────────────────────────────────────────────────────
// The item hooks; the cursor sprites are hidden without a lens.
// FF3 0x00114520 (copies the list to the stack first).

// callcheck: B2dGridSprSetDisp (B2D_GRID_SPR::SetDisp)
void CmenuCamStLens::SetHooks()
{
    static const B2D_HOOK_REC hooks[] =     // data 0x34cd78
    {
        { MenuCamStLensPipDarkHook,  0, 0, "R_tama_r_kuro" },
        { MenuCamStLensPipLitHook,   0, 0, "R_tama_r_ao" },
        { MenuCamStLensCostNumHook,  0, 0, "Num_1buy06_renz_R" },
        { MenuCamStLensCostNumHook,  0, 0, "Num_2buy06_renz_R" },
        { MenuCamStLensCostNumHook,  0, 0, "Num_3buy06_renz_R" },
        { MenuCamStLensCostNumHook,  0, 0, "Num_4buy06_renz_R" },
        { MenuCamStLensCostNumHook,  0, 0, "Num_5buy06_renz_R" },
        { MenuCamStLensCostNumHook,  0, 0, "Num_6buy06_renz_R" },
        { MenuCamStLensPtsHook,      0, 0, "R_pts_r_01" },
        { MenuCamStLensPtsHook,      0, 0, "R_pts_r_03" },
        { MenuCamStLensLvNumHook,    0, 0, "R_lve_r_00" },
        { MenuCamStLensLvNumHook,    0, 0, "R_lve_r_01" },
        { MenuCamStLensLvNumHook,    0, 0, "R_lve_r_02" },
        { MenuCamStLensLvNumHook,    0, 0, "R_lve_r_03" },
        { MenuCamStLensLvNumHook,    0, 0, "R_lve_r_04" },
        { MenuCamStLensLvNumHook,    0, 0, "R_lve_r_05" },
        { MenuCamStLensPointNumHook, 0, 0, "num_point06_renz_R" },
        { MenuCamStLensNameHook,     0, 0, "R renz_zero_s" },
        { MenuCamStLensNameHook,     0, 0, "R renz_track_s" },
        { MenuCamStLensNameHook,     0, 0, "R renz_see_s" },
        { MenuCamStLensNameHook,     0, 0, "R renz_serial_s" },
        { MenuCamStLensNameHook,     0, 0, "R renz_blast_s" },
        { MenuCamStLensNameHook,     0, 0, "R renz_crush_s" },
        { MenuCamStLensNameHook,     0, 0, "R renz_stp_s" },
        { MenuCamStLensNameHook,     0, 0, "R renz_slw_s" },
        { MenuCamStLensNameHook,     0, 0, "R renz_stn_s" },
        { nullptr, -1, -1, nullptr },
    };

    // CmenuCamMain calls SetHooks before Setup on a fresh page, so the first
    // time FF3 calls SetDisp on null sprites (the basic and status pages do
    // not switch here without a lens, so the empty page is hardly reached).
    if (count < 1)
    {
        for (int i = 0; i < 2; i++)
        {
            if (csr_spr[i] != nullptr)  // PC: FF3 does not check
                csr_spr[i]->SetDisp(0);
        }
    }
    B2dSetHookList(handle, hooks);
}
