// ingame/menu/menuCamBasic.c -- CmenuCamBasic, the camera menu's basic
// functions page (the camera levels, bought up for points), and the two
// helpers the other pages share. FF3 0x001106d8..0x00112150 +
// 0x002ac048..0x002ac128; FF3-only (the class file sorts between
// CmenuCamBase and CmenuCamMain; open-items.md "0x10bbb0..0x116fa0").
#include "ingame/menu/menuCamBasic.h"

#include <string.h>                     // strcmp, strlen

#include "common/cmn_util.h"            // CSR_GRID, STEP_SEQ, CamLv*, UpCostGet, CamLvGetCost
#include "graphics/motion/anicode.h"    // B2D_HOOK_REC, B2dSetHookList, MenuWdwSetMsg, MenuWdwSetYesNo, MenuWdwGetResult
#include "ingame/ingame_wrk.h"          // ingame_wrk, IngameWrkGetPoint, IngameWrkSubPoint
#include "ingame/menu/menuCamMain.h"    // GetMenuCamMain, CmenuCamMain::SetPage, MENU_CAM_PAGE_*
#include "ingame/photo/finder_photo.h"  // GetPhotoModeWrk, GetLensWrk
#include "ingame/photo/fndr_parts.h"    // LENS_WRK
#include "ingame/sound/snd_ctrl.h"      // SndSePlay
#include "system/b2d/B2dBase.h"         // CB2dBase, B2D_PLACE, B2dNumDrawFont
#include "system/b2d/B2dCtl.h"          // B2D_HOOK_ARGS, B2D_ITEM, B2dGetDrawItem
#include "system/b2d/BpCtl.h"           // BpGetDrawHandle, BpSetBlendAlpha, B2dDrawSpriteDirect
#include "system/os/system.h"           // GetSysWrk, SYS_WRK::GetLanguage
#include "system/pad/pad_ctrl.h"        // pad_key, CheckPadTrgPlaySe
#include "ingame/menu/menu_mouse.h"    // PC: the mouse in menus

#define MENU_CAM_BASIC_MSG0         0x68    // MenuWdwSetMsg's msg0 (this page's help texts)
#define MENU_CAM_BASIC_SE_MOVE      0x1a9b0000
#define MENU_CAM_BASIC_SE_CANCEL    0x1a9b0001
#define MENU_CAM_BASIC_SE_OK        0x1a9b0003
#define MENU_CAM_BASIC_SE_PAGE      0x1a9b0005
#define MENU_CAM_BASIC_SE_BUY       0x1a9b0006
#define MENU_CAM_BASIC_CHR          0xff    // Lens* chr: the current player
#define MENU_CAM_BASIC_LV_MAX       3       // CamLvGet at the top level
#define MENU_CAM_BASIC_LV_UP_FRAME  0x22    // the level-up wait raises the level on this frame of the shine
#define MENU_CAM_BASIC_BAR_ITEM     0x56a   // where SetHooks puts the row bars (row 0's name item)

// The level-up messages (MenuWdwSetMsg msg1 under MENU_CAM_BASIC_MSG0).
#define MENU_CAM_BASIC_MSG_CONFIRM  0       // "spend the points?" with the yes/no
#define MENU_CAM_BASIC_MSG_NO_POINT 1       // not enough points
#define MENU_CAM_BASIC_MSG_MAX      2       // already at the top level

// ──────────────────────────────────────────────────────────────────────
// The level rows (data 0x34c990): one per CamLvGetKind slot, the items its
// parts sit at, its name items and its number item.
// layout: host -- three name pointers; 0x28 bytes on the EE (no at +0x00,
// name_item +0x04, lv_item +0x08, frame_item +0x0c, pip_item +0x10,
// pts_item +0x14, name_lit +0x18, name +0x1c, num +0x20, cost_id +0x24).
// ──────────────────────────────────────────────────────────────────────

struct MENU_CAM_BASIC_ROW
{
    int         no;                     // the row (CamLvGetKind slot), -1 ends
    int         name_item;              // the row's name (lit / plain, R_special_*), -1 ends
    int         lv_item;                // R_lve_0N: the level number
    int         frame_item;             // R_tamaana_00
    int         pip_item;               // the first pip (R_tamaana_01 / 02, R_tama_ao / kuro, the shine)
    int         pts_item;               // R_pts_01 / R_pts_03
    const char *name_lit;               // the name item when the row is selected
    const char *name;                   // ... and when not
    const char *num;                    // the cost number's text item
    int         cost_id;                // UpCostGet id (UP_COST_CAM_LV + no)
};

static const MENU_CAM_BASIC_ROW menu_cam_basic_row[] =
{
    { 0, 0x56a, 0x564, 0x551, 0x552, 0x54a, "R_maximum_l",     "R_maximum",      "Num_1buy06_R", UP_COST_CAM_LV + 0 },
    { 1, 0x56c, 0x565, 0x555, 0x556, 0x54b, "R_circle_l",      "R_circle",       "Num_2buy06_R", UP_COST_CAM_LV + 1 },
    { 2, 0x569, 0x566, 0x558, 0x559, 0x54c, "R_sensitivity_l", "R_sensitivity ", "num_3buy06_R", UP_COST_CAM_LV + 2 },
    { 3, 0x56f, 0x567, 0x55b, 0x55c, 0x54d, "R_special_l_00",  "R_special_00",   "num_4buy06_R", UP_COST_CAM_LV + 3 },
    { 4, 0x570, 0x568, 0x55e, 0x55f, 0x54e, "R_special_l_01",  "R_special_01",   "num_5buy06_R", UP_COST_CAM_LV + 4 },
    { -1, -1, -1, -1, -1, -1, nullptr, nullptr, nullptr, -1 },
};

// The special abilities' names (data 0x34ca80): CamLvGetSpKind -> items.
// layout: host -- {kind, two pointers}; 0xc bytes on the EE.
struct MENU_CAM_BASIC_SP
{
    int         kind;                   // CamLvGetSpKind, -1 ends
    const char *name_lit;               // nullptr ends
    const char *name;
};

static const MENU_CAM_BASIC_SP menu_cam_basic_sp[] =
{
    { 0, "R_special_l_03", "R_special_03" },
    { 1, "R_special_l_01", "R_special_01" },
    { 2, "R_special_l_02", "R_special_02" },
    { 3, "R_special_l_00", "R_special_00" },
    { -1, nullptr, nullptr },
};

// The row bars' y per row (data 0x34cac0; the special rows sit lower).
static const int menu_cam_basic_bar_y[] = { 0, 30, 60, 130, 160 };

// sdata 0x36b6e4 (gp-0x7e0c): the page the constructor made (its hooks read
// it); sdata 0x36b6e0 (gp-0x7e10): the level-up wait's frame count.
static CmenuCamBasic *menu_cam_basic;
static int            menu_cam_basic_lv_up_frame;

static int MenuCamBasicStepTop(STEP_SEQ *seq);
static int MenuCamBasicStepConfirm(STEP_SEQ *seq);
static int MenuCamBasicStepNotice(STEP_SEQ *seq);
static int MenuCamBasicStepLvUp(STEP_SEQ *seq);

// ──────────────────────────────────────────────────────────────────────
// Table lookups (out-of-line copies at the end of .text, and 0x00110f20).
// nullptr when the key is missing.

// FF3 0x002ac048: by the cost number's item name.
static const MENU_CAM_BASIC_ROW *MenuCamBasicFindRowByNum(const MENU_CAM_BASIC_ROW *tbl, const char *const *name)
{
    if (tbl->name_item == -1)
        return nullptr;
    while (strcmp(*name, tbl->num) != 0)
    {
        if (tbl[1].name_item == -1)
            return nullptr;
        tbl++;
    }
    return tbl;
}

// FF3 0x002ac0b0: a special ability by kind.
static const MENU_CAM_BASIC_SP *MenuCamBasicFindSpByKind(const MENU_CAM_BASIC_SP *tbl, const int *kind)
{
    if (tbl->name_lit == nullptr)
        return nullptr;
    while (tbl->kind != *kind)
    {
        if (tbl[1].name_lit == nullptr)
            return nullptr;
        tbl++;
    }
    return tbl;
}

// FF3 0x002ac0e8: by row number.
static const MENU_CAM_BASIC_ROW *MenuCamBasicFindRowByNo(const MENU_CAM_BASIC_ROW *tbl, const int *no)
{
    if (tbl->name_item == -1)
        return nullptr;
    while (tbl->no != *no)
    {
        if (tbl[1].name_item == -1)
            return nullptr;
        tbl++;
    }
    return tbl;
}

// FF3 0x00110f20: by the lit (lit != 0) or the plain name.
static const MENU_CAM_BASIC_ROW *MenuCamBasicFindRowByName(const char *name, int lit)
{
    for (const MENU_CAM_BASIC_ROW *r = menu_cam_basic_row; r->name_item >= 0; r++)
    {
        if (strcmp(lit != 0 ? r->name_lit : r->name, name) == 0)
            return r;
    }
    return nullptr;
}

// ══════════════════════════════════════════════════════════════════════
// The page
// ══════════════════════════════════════════════════════════════════════

// ──────────────────────────────────────────────────────────────────────
// FF3 0x00110d60 (MenuCamBasicCtor; FF3 0x00110c48 is the same body).
// callcheck: MenuCamBaseCtorBase (the base class constructor)
// callcheck: CsrGridInit, StepSeqInit (grid's and seq's constructors, implicit)

CmenuCamBasic::CmenuCamBasic()
{
    static const STEP_FUNC steps[] =    // data 0x34c500
    {
        MenuCamBasicStepTop, MenuCamBasicStepConfirm, MenuCamBasicStepNotice, MenuCamBasicStepLvUp,
        nullptr,
    };

    // x: -1 / +1 switch pages (also L2 / R2), silently; y: the rows, wrapping.
    grid.SetRange(0, -1, 1);
    grid.SetPos(0, 0);
    grid.se = MENU_CAM_BASIC_SE_MOVE;
    grid.flags |= CSR_GRID_LR2 | CSR_GRID_SE_Y;
    grid.SetRange(1, 0, CamLvSpGetEquip(CAM_LV_SP1) + CamLvSpGetEquip(CAM_LV_SP2) + 2);
    grid.wrap = CSR_GRID_WRAP_Y;
    seq.Add(steps);
    seq.owner = this;
    menu_cam_basic = this;
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x00110ea0 (MenuCamBasicDtor; the base one 0x00110e78 is the same
// body, the deleting one 0x00110ec8 adds operator delete). The singleton
// is left as it is.
// callcheck: MenuCamBaseDtorBase (the base class destructor)

CmenuCamBasic::~CmenuCamBasic()
{
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x00110f00.

CmenuCamBasic *GetMenuCamBasic(void)
{
    return menu_cam_basic;
}

// ──────────────────────────────────────────────────────────────────────
// Per frame: the step sequence; 1 (cancel on the top step) ends the menu.
// FF3 0x00110f08.

int CmenuCamBasic::Step()
{
    return seq.Run();
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x001120b8. (FF3 0x001120c0 after it, an empty void function, is
// unreferenced.)

int CmenuCamBasic::Draw()
{
    return 0;
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x00112070 (GetSysWrk is cached in sdata gp-0xc60).

void CmenuCamBasic::Load()
{
    CB2dBase::Load(GetSysWrk()->GetLanguage() + MENU_CAM_BASIC_FILE);
}

// ══════════════════════════════════════════════════════════════════════
// Steps (list 0x34c500). The sequencer's owner is the page.
// ══════════════════════════════════════════════════════════════════════

// ──────────────────────────────────────────────────────────────────────
// Top: the cursor, the row's help text, left / right switch to the status
// page (-1, also +1 without a lens) or the lens page (+1); a cursor move
// moves the row bars; OK on a row asks to buy its next level (or says it
// is at the top / the points are short), cancel ends the menu.
// FF3 0x001106d8.

// callcheck: B2dPlaceInit (B2D_PLACE's constructor)
static int MenuCamBasicStepTop(STEP_SEQ *seq)
{
    static const int row_msg[] =        // data 0x34c4a0: {row, or 3 + the special ability, msg1}
    {
        0, 3, 1, 4, 2, 5, 3, 7, 4, 6, 5, 8, 6, 9, -1, -1,
    };

    CmenuCamBasic *cb    = (CmenuCamBasic *)seq->owner;

    // PC: the mouse. The bar sits on the cursor's row; the rows are the table's
    // distances from it.
    {
        ReiPan_MenuBox bar;
        const CSR_AXIS *rows = &cb->grid.axis[1];

        if (ReiPan_MenuMouseNameBox(cb->handle, "bar_buy_00", &bar))
        {
            const int here = menu_cam_basic_bar_y[rows->cur];

            for (int i = rows->min; i <= rows->max; i++)
            {
                ReiPan_MenuBox box = bar;

                box.y += (float)(menu_cam_basic_bar_y[i] - here);
                MenuMouseGrid(&cb->grid, 1, i, &box);
            }
        }
    }

    int            moved = cb->grid.Move();
    int            row   = cb->grid.axis[1].cur;

    int key = row;
    if (row > 2)
        key = CamLvGetSpKind(row) + 3;
    MenuWdwSetMsgByTable(row_msg, MENU_CAM_BASIC_MSG0, key);

    int page = cb->grid.axis[0].cur;
    if (page != 0 && MenuCamGetLensWrk()->GetHaveNum(MENU_CAM_BASIC_CHR) == 0)
        page = -1;
    if (page == -1)
        GetMenuCamMain()->SetPage(MENU_CAM_PAGE_STAT);
    else if (page == 1)
        GetMenuCamMain()->SetPage(MENU_CAM_PAGE_STLENS);

    if (page != 0)
    {
        SndSePlay(MENU_CAM_BASIC_SE_PAGE, 100, 0x1000, 0, nullptr, 0);
        cb->grid.SetPos(0, 0);
        return STEP_CONT;
    }

    if (moved != 0)
    {
        B2D_PLACE place(cb->handle);
        int       y = menu_cam_basic_bar_y[row];
        place.SetXYByName("bar_buy_00", 0, y);
        place.SetXYByName("bar_buy_01", 0, y);
        return STEP_CONT;
    }

    if (CheckPadTrgPlaySe((u_short)pad_key[PAD_KEY_OK], MENU_CAM_BASIC_SE_OK, 0) != 0)
    {
        int lv = CamLvGet(CamLvGetKind(row));
        int msg;

        if (lv == MENU_CAM_BASIC_LV_MAX)
            msg = MENU_CAM_BASIC_MSG_MAX;
        else
        {
            // FF3 keys the cost by the row, not by its kind (the same for
            // rows 0..2; row 3 shows slot 4's ability when slot 3 is empty).
            int cost = CamLvGetCost(ingame_wrk.b00, row, lv);
            msg = (int)IngameWrkGetPoint() < cost ? MENU_CAM_BASIC_MSG_NO_POINT : -1;
        }

        if (msg < 0)
        {
            MenuWdwSetYesNo(1, 0);
            MenuWdwSetMsg(MENU_CAM_BASIC_MSG0, MENU_CAM_BASIC_MSG_CONFIRM);
            seq->Jump(MenuCamBasicStepConfirm);
        }
        else
        {
            MenuWdwSetMsg(MENU_CAM_BASIC_MSG0, msg);
            seq->Jump(MenuCamBasicStepNotice);
        }
        return STEP_NEXT;
    }
    if (CheckPadTrgPlaySe((u_short)pad_key[PAD_KEY_CANCEL], MENU_CAM_BASIC_SE_CANCEL, 0) != 0)
        return STEP_WAIT;               // Run returns 1: the menu closes
    return STEP_CONT;
}

// ──────────────────────────────────────────────────────────────────────
// The yes/no: yes plays the shine on the menu's background layout over the
// row's next pip, takes the points and waits for the level-up; no or
// cancel go back to the top. FF3 0x00110a88.

// callcheck: B2dPlaceInit (B2D_PLACE's constructor)
static int MenuCamBasicStepConfirm(STEP_SEQ *seq)
{
    int res = MenuWdwGetResult();

    if (res == MENU_WDW_RESULT_YES)
    {
        MenuWdwSetYesNo(0, 0);
        CB2dBase *bg = GetMenuCamMain()->bg;
        bg->Play("shine");              // sdata 0x36b6e8

        CmenuCamBasic *cb   = (CmenuCamBasic *)seq->owner;
        int            row  = cb->grid.axis[1].cur;
        int            kind = CamLvGetKind(row);
        int            lv   = CamLvGet(kind);
        int            base = CamLvGetBase(kind);
        int            xy[2];

        B2D_PLACE place(cb->handle);
        int item = menu_cam_basic_row[row].pip_item;
        place.GetXY(item, xy);
        int w = place.GetW(item);
        bg->SetPos(xy[0] + w * (lv + base) - 12, xy[1] - 11);

        IngameWrkSubPoint(CamLvGetCost(ingame_wrk.b00, row, lv));
        menu_cam_basic_lv_up_frame = 0;
        SndSePlay(MENU_CAM_BASIC_SE_BUY, 100, 0x1000, 0, nullptr, 0);
        seq->Jump(MenuCamBasicStepLvUp);
        return STEP_NEXT;
    }
    if (res < MENU_WDW_RESULT_YES || res > MENU_WDW_RESULT_CANCEL)
        return STEP_CONT;

    SndSePlay(MENU_CAM_BASIC_SE_CANCEL, 100, 0x1000, 0, nullptr, 0);
    MenuWdwSetYesNo(0, 0);
    seq->Jump(MenuCamBasicStepTop);
    return STEP_NEXT;
}

// ──────────────────────────────────────────────────────────────────────
// The "top level" / "not enough points" message: OK or cancel go back.
// FF3 0x00110a00.

static int MenuCamBasicStepNotice(STEP_SEQ *seq)
{
    ReiPan_MenuMouseConfirm();          // PC: a click acknowledges
    if (CheckPadTrgPlaySe((u_short)(pad_key[PAD_KEY_CANCEL] | pad_key[PAD_KEY_OK]), -1, 0) == 0)
        return STEP_CONT;
    SndSePlay(MENU_CAM_BASIC_SE_CANCEL, 100, 0x1000, 0, nullptr, 0);
    seq->Jump(MenuCamBasicStepTop);
    return STEP_NEXT;
}

// ──────────────────────────────────────────────────────────────────────
// Waits for the shine to end; the level goes up on its frame 0x22, so the
// pip fills under it. FF3 0x00110988.

static int MenuCamBasicStepLvUp(STEP_SEQ *seq)
{
    if (GetMenuCamMain()->bg->IsPlayEnd() == 0)
    {
        if (++menu_cam_basic_lv_up_frame == MENU_CAM_BASIC_LV_UP_FRAME)
        {
            CmenuCamBasic *cb = (CmenuCamBasic *)seq->owner;
            CamLvUp(CamLvGetKind(cb->grid.axis[1].cur));
        }
        return STEP_CONT;
    }
    seq->Jump(MenuCamBasicStepTop);
    return STEP_NEXT;
}

// ══════════════════════════════════════════════════════════════════════
// Item hooks (list 0x34c770). Sprite hooks: t1, t2 x, y; the two number
// hooks are text-item hooks: t1 the corners xy[8], stack[2] alpha,
// stack[3] the blend flags.
// ══════════════════════════════════════════════════════════════════════

// Draws the hook's sprite at (x, y).
static void MenuCamBasicDrawAt(const B2D_HOOK_ARGS *args, int x, int y)
{
    B2D_HOOK_ARGS a = *args;
    a.t1 = x;
    a.t2 = y;
    B2dDrawSpriteDirect(&a);
}

// ──────────────────────────────────────────────────────────────────────
// R_lve_00..05: the level number N, at every row whose level plus base
// points is N. FF3 0x00110fe0.

// callcheck: B2dPlaceInit (B2D_PLACE's constructor)
// callcheck: B2dDrawSpriteDirect (MenuCamBasicDrawAt)
static int MenuCamBasicLvNumHook(const B2D_HOOK_ARGS *args)
{
    int       xy[2];

    B2D_PLACE place(BpGetDrawHandle());
    B2D_ITEM *item = B2dGetDrawItem();
    int       n    = item->name[strlen(item->name) - 1] - '0';

    int i = 0;
    for (const MENU_CAM_BASIC_ROW *r = menu_cam_basic_row; r->name_item >= 0; r++)
    {
        int kind = CamLvGetKind(i++);
        if (kind == -1)
            return 0;
        int lv = CamLvGet(kind);
        if (n == lv + CamLvGetBase(kind))
        {
            place.GetXY(r->lv_item, xy);
            MenuCamBasicDrawAt(args, xy[0], xy[1]);
        }
    }
    return 0;
}

// ──────────────────────────────────────────────────────────────────────
// R_tamaana_00: the pip frame of each row. FF3 0x00111148.

// callcheck: B2dPlaceInit (B2D_PLACE's constructor)
// callcheck: B2dDrawSpriteDirect (MenuCamBasicDrawAt)
static int MenuCamBasicPipFrameHook(const B2D_HOOK_ARGS *args)
{
    int       xy[2];

    B2D_PLACE place(BpGetDrawHandle());
    int i = 0;
    for (const MENU_CAM_BASIC_ROW *r = menu_cam_basic_row; r->name_item >= 0; r++)
    {
        if (CamLvGetKind(i++) == -1)
            return 0;
        place.GetXY(r->frame_item, xy);
        MenuCamBasicDrawAt(args, xy[0], xy[1]);
    }
    return 0;
}

// ──────────────────────────────────────────────────────────────────────
// R_tamaana_01: a pip hole for every level the row can reach (base + 3).
// FF3 0x00111260.

// callcheck: B2dPlaceInit (B2D_PLACE's constructor)
// callcheck: B2dDrawSpriteDirect (MenuCamBasicDrawAt)
static int MenuCamBasicPipHoleHook(const B2D_HOOK_ARGS *args)
{
    int       xy[2];

    B2D_PLACE place(BpGetDrawHandle());
    int i = 0;
    for (const MENU_CAM_BASIC_ROW *r = menu_cam_basic_row; r->name_item >= 0; r++, i++)
    {
        int kind = CamLvGetKind(i);
        if (kind == -1)
            return 0;
        place.GetXY(r->pip_item, xy);
        int w = place.GetW(r->pip_item);
        for (int n = CamLvGetBase(kind) + 3; n > 0; n--)
        {
            MenuCamBasicDrawAt(args, xy[0], xy[1]);
            xy[0] += w;
        }
    }
    return 0;
}

// ──────────────────────────────────────────────────────────────────────
// R_tamaana_02: the end cap after the last hole. FF3 0x001113e8.

// callcheck: B2dPlaceInit (B2D_PLACE's constructor)
// callcheck: B2dDrawSpriteDirect (MenuCamBasicDrawAt)
static int MenuCamBasicPipEndHook(const B2D_HOOK_ARGS *args)
{
    int       xy[2];

    B2D_PLACE place(BpGetDrawHandle());
    int i = 0;
    for (const MENU_CAM_BASIC_ROW *r = menu_cam_basic_row; r->name_item >= 0; r++)
    {
        int kind = CamLvGetKind(i++);
        if (kind == -1)
            return 0;
        int base = CamLvGetBase(kind);
        place.GetXY(r->pip_item, xy);
        int w = place.GetW(r->pip_item);
        MenuCamBasicDrawAt(args, xy[0] + w * (base + 3), xy[1]);
    }
    return 0;
}

// ──────────────────────────────────────────────────────────────────────
// R_tama_ao: a filled pip per point (level + base). FF3 0x00111530.

// callcheck: B2dPlaceInit (B2D_PLACE's constructor)
// callcheck: B2dDrawSpriteDirect (MenuCamBasicDrawAt)
static int MenuCamBasicPipLitHook(const B2D_HOOK_ARGS *args)
{
    int       xy[2];

    B2D_PLACE place(BpGetDrawHandle());
    int i = 0;
    for (const MENU_CAM_BASIC_ROW *r = menu_cam_basic_row; r->name_item >= 0; r++, i++)
    {
        int kind = CamLvGetKind(i);
        if (kind == -1)
            return 0;
        place.GetXY(r->pip_item, xy);
        int w  = place.GetW(r->pip_item);
        int lv = CamLvGet(kind);
        for (int n = lv + CamLvGetBase(kind); n > 0; n--)
        {
            MenuCamBasicDrawAt(args, xy[0], xy[1]);
            xy[0] += w;
        }
    }
    return 0;
}

// ──────────────────────────────────────────────────────────────────────
// R_tama_kuro: the empty pips after the points, up to base + 3.
// FF3 0x001116c0.

// callcheck: B2dPlaceInit (B2D_PLACE's constructor)
// callcheck: B2dDrawSpriteDirect (MenuCamBasicDrawAt)
static int MenuCamBasicPipDarkHook(const B2D_HOOK_ARGS *args)
{
    int       xy[2];

    B2D_PLACE place(BpGetDrawHandle());
    int i = 0;
    for (const MENU_CAM_BASIC_ROW *r = menu_cam_basic_row; r->name_item >= 0; r++, i++)
    {
        int kind = CamLvGetKind(i);
        if (kind == -1)
            return 0;
        place.GetXY(r->pip_item, xy);
        int w    = place.GetW(r->pip_item);
        int base = CamLvGetBase(kind);
        int lv   = CamLvGet(kind) + base;
        xy[0] += w * lv;
        for (int n = lv; n < base + 3; n++)
        {
            MenuCamBasicDrawAt(args, xy[0], xy[1]);
            xy[0] += w;
        }
    }
    return 0;
}

// ──────────────────────────────────────────────────────────────────────
// R_pts_01 at the selected row, R_pts_03 at the others. FF3 0x00111868.

// callcheck: B2dPlaceInit (B2D_PLACE's constructor)
// callcheck: B2dDrawSpriteDirect (MenuCamBasicDrawAt)
static int MenuCamBasicPtsHook(const B2D_HOOK_ARGS *args)
{
    int       xy[2];

    B2D_PLACE place(BpGetDrawHandle());
    int       sel = strcmp(B2dGetDrawItem()->name, "R_pts_01") == 0;    // 0x34c530
    int       cur = GetMenuCamBasic()->grid.axis[1].cur;

    int i = 0;
    for (const MENU_CAM_BASIC_ROW *r = menu_cam_basic_row; r->name_item != -1; r++, i++)
    {
        if (CamLvGetKind(i) == -1)
            return 0;
        if ((cur == i) == (sel != 0))
        {
            place.GetXY(r->pts_item, xy);
            MenuCamBasicDrawAt(args, xy[0], xy[1]);
        }
    }
    return 0;
}

// ──────────────────────────────────────────────────────────────────────
// Num_point06_R: the points the player has. FF3 0x001119d8.

static int MenuCamBasicPointNumHook(const B2D_HOOK_ARGS *args)
{
    const int *xy = (const int *)args->t1;

    BpSetBlendAlpha((u_int)args->stack[3]);
    B2dNumDrawFont((int)IngameWrkGetPoint(), BpGetDrawHandle(), "R_num_point_0", xy[4], xy[5],
                   (u_char)args->stack[2], 0);      // 0x34c540
    return 0;
}

// ──────────────────────────────────────────────────────────────────────
// Num_1buy06_R..num_5buy06_R: the cost of the row's next level (0 at the
// top), in the bright digits on the selected row. FF3 0x00111a40.

static int MenuCamBasicCostNumHook(const B2D_HOOK_ARGS *args)
{
    const int *xy = (const int *)args->t1;

    B2D_ITEM *item = B2dGetDrawItem();
    const MENU_CAM_BASIC_ROW *r = MenuCamBasicFindRowByNum(menu_cam_basic_row, &item->name);
    if (r == nullptr)
        return 0;                       // PC: FF3 reads the row through the nullptr

    int kind = CamLvGetKind(r->no);
    if (kind == -1)
        return 0;

    int cost = 0;
    int lv   = CamLvGet(kind);
    if (lv < MENU_CAM_BASIC_LV_MAX)
        cost = UpCostGet(ingame_wrk.b00, r->cost_id, lv);

    const char *font = r->no == GetMenuCamBasic()->grid.axis[1].cur ? "R_num_buy_0" : "R_num_select_0";
    BpSetBlendAlpha((u_int)args->stack[3]);
    B2dNumDrawFont(cost, BpGetDrawHandle(), font, xy[4], xy[5], (u_char)args->stack[2], 0);
    return 0;
}

// ──────────────────────────────────────────────────────────────────────
// Where a special ability's name goes: the special slot (3, 4) holding
// the drawn item's ability, lit only on the selected row, plain only on
// the others; xy the row's name item. 0 when not drawn. FF3 0x00111b28.

// callcheck: B2dPlaceInit (B2D_PLACE's constructor)
static int MenuCamBasicSpPos(int *xy, int lit)
{
    B2D_PLACE place(BpGetDrawHandle());
    B2D_ITEM *item = B2dGetDrawItem();
    int       n    = 0;                 // shown special rows before this slot

    for (int slot = CAM_LV_SP1; slot <= CAM_LV_SP2; slot++)
    {
        int kind = CamLvGetSpKind(slot);
        if (kind == -1)
            continue;
        int idx = n++;

        const MENU_CAM_BASIC_SP *sp = MenuCamBasicFindSpByKind(menu_cam_basic_sp, &kind);
        if (sp == nullptr)
            continue;
        if (strcmp(lit != 0 ? sp->name_lit : sp->name, item->name) != 0)
            continue;

        int no = idx + 3;
        const MENU_CAM_BASIC_ROW *r = MenuCamBasicFindRowByNo(menu_cam_basic_row, &no);
        if (r == nullptr)
            return 0;                   // PC: FF3 reads the row through the nullptr (rows 3, 4 exist)
        int selected = GetMenuCamBasic()->grid.axis[1].cur == r->no;
        if (selected != (lit != 0))
            return 0;
        place.GetXY(r->name_item, xy);
        return 1;
    }
    return 0;
}

// R_special_l_00..03. FF3 0x00111c68.
// callcheck: B2dDrawSpriteDirect (MenuCamBasicDrawAt)
static int MenuCamBasicSpLitHook(const B2D_HOOK_ARGS *args)
{
    int xy[2];

    if (MenuCamBasicSpPos(xy, 1) != 0)
        MenuCamBasicDrawAt(args, xy[0], xy[1]);
    return 0;
}

// R_special_00..03. FF3 0x00111d20.
// callcheck: B2dDrawSpriteDirect (MenuCamBasicDrawAt)
static int MenuCamBasicSpHook(const B2D_HOOK_ARGS *args)
{
    int xy[2];

    if (MenuCamBasicSpPos(xy, 0) != 0)
        MenuCamBasicDrawAt(args, xy[0], xy[1]);
    return 0;
}

// ──────────────────────────────────────────────────────────────────────
// R_maximum_l / R_circle_l / R_sensitivity_l on the selected row, the
// plain names on the others, at the row's name item. FF3 0x00111dd8 /
// 0x00111ed0.

// callcheck: B2dPlaceInit (B2D_PLACE's constructor)
// callcheck: B2dDrawSpriteDirect (MenuCamBasicDrawAt)
static int MenuCamBasicNameDraw(const B2D_HOOK_ARGS *args, int lit)
{
    int       xy[2];

    B2D_PLACE place(BpGetDrawHandle());
    B2D_ITEM *item = B2dGetDrawItem();
    const MENU_CAM_BASIC_ROW *r = MenuCamBasicFindRowByName(item->name, lit);
    if (r == nullptr)
        return 0;                       // PC: FF3 reads the row through the nullptr

    int selected = GetMenuCamBasic()->grid.axis[1].cur == r->no;
    if (selected == (lit != 0))
    {
        place.GetXY(r->name_item, xy);
        MenuCamBasicDrawAt(args, xy[0], xy[1]);
    }
    return 0;
}

static int MenuCamBasicNameLitHook(const B2D_HOOK_ARGS *args)       // FF3 0x00111dd8
{
    return MenuCamBasicNameDraw(args, 1);
}

static int MenuCamBasicNameHook(const B2D_HOOK_ARGS *args)          // FF3 0x00111ed0
{
    return MenuCamBasicNameDraw(args, 0);
}

// ──────────────────────────────────────────────────────────────────────
// The item hooks, and the row bars to row 0. FF3 0x00111fc8.

// callcheck: B2dPlaceInit (B2D_PLACE's constructor)
void CmenuCamBasic::SetHooks()
{
    static const B2D_HOOK_REC hooks[] =     // data 0x34c770
    {
        { MenuCamBasicLvNumHook,    0, 0, "R_lve_00" },
        { MenuCamBasicLvNumHook,    0, 0, "R_lve_01" },
        { MenuCamBasicLvNumHook,    0, 0, "R_lve_02" },
        { MenuCamBasicLvNumHook,    0, 0, "R_lve_03" },
        { MenuCamBasicLvNumHook,    0, 0, "R_lve_04" },
        { MenuCamBasicLvNumHook,    0, 0, "R_lve_05" },
        { MenuCamBasicPipFrameHook, 0, 0, "R_tamaana_00" },
        { MenuCamBasicPipHoleHook,  0, 0, "R_tamaana_01" },
        { MenuCamBasicPipEndHook,   0, 0, "R_tamaana_02" },
        { MenuCamBasicPipLitHook,   0, 0, "R_tama_ao" },
        { MenuCamBasicPipDarkHook,  0, 0, "R_tama_kuro" },
        { MenuCamBasicNameLitHook,  0, 0, "R_maximum_l" },
        { MenuCamBasicNameLitHook,  0, 0, "R_circle_l" },
        { MenuCamBasicNameLitHook,  0, 0, "R_sensitivity_l" },
        { MenuCamBasicSpLitHook,    0, 0, "R_special_l_00" },
        { MenuCamBasicSpLitHook,    0, 0, "R_special_l_01" },
        { MenuCamBasicSpLitHook,    0, 0, "R_special_l_02" },
        { MenuCamBasicSpLitHook,    0, 0, "R_special_l_03" },
        { MenuCamBasicNameHook,     0, 0, "R_maximum" },
        { MenuCamBasicNameHook,     0, 0, "R_circle" },
        { MenuCamBasicNameHook,     0, 0, "R_sensitivity " },
        { MenuCamBasicSpHook,       0, 0, "R_special_00" },
        { MenuCamBasicSpHook,       0, 0, "R_special_01" },
        { MenuCamBasicSpHook,       0, 0, "R_special_02" },
        { MenuCamBasicSpHook,       0, 0, "R_special_03" },
        { MenuCamBasicPtsHook,      0, 0, "R_pts_01" },
        { MenuCamBasicPtsHook,      0, 0, "R_pts_03" },
        { MenuCamBasicPointNumHook, 0, 0, "Num_point06_R" },
        { MenuCamBasicCostNumHook,  0, 0, "Num_1buy06_R" },
        { MenuCamBasicCostNumHook,  0, 0, "Num_2buy06_R" },
        { MenuCamBasicCostNumHook,  0, 0, "num_3buy06_R" },
        { MenuCamBasicCostNumHook,  0, 0, "num_4buy06_R" },
        { MenuCamBasicCostNumHook,  0, 0, "num_5buy06_R" },
        { nullptr, -1, -1, nullptr },
    };

    B2dSetHookList(handle, hooks);
    B2D_PLACE place(handle);
    place.MoveItemByName(MENU_CAM_BASIC_BAR_ITEM, "bar_buy_00");
    place.MoveItemByName(MENU_CAM_BASIC_BAR_ITEM, "bar_buy_01");
}

// ══════════════════════════════════════════════════════════════════════
// Helpers the other pages call
// ══════════════════════════════════════════════════════════════════════

// ──────────────────────────────────────────────────────────────────────
// FF3 0x001120c8.

LENS_WRK *MenuCamGetLensWrk(void)
{
    GetPhotoModeWrk();
    return GetLensWrk();
}

// ──────────────────────────────────────────────────────────────────────
// The message of key in a {key, msg1} list ended by a negative key;
// nothing when key is missing. FF3 0x001120e8.

void MenuWdwSetMsgByTable(const int *pairs, int msg0, int key)
{
    for (; pairs[0] >= 0; pairs += 2)
    {
        if (pairs[0] == key)
        {
            MenuWdwSetMsg(msg0, pairs[1]);
            return;
        }
    }
}
