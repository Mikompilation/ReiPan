/* ==========================================================================
 *  outgame/sel_menu.c
 *
 *  The game clear menu's select menu (see sel_menu.h). FF3 0x1ed658..
 *  0x1edd90, FF3's own code: ingame/gameover_menu.c over CLEARMENU_PHF,
 *  with MenuCsr 0x2b, the row list (and each row's message kind) set by
 *  the caller, rows 30 px apart and no cursor row kept between visits.
 *  What differs from the game over menu is commented where it happens.
 * ======================================================================== */
#include "outgame/sel_menu.h"

#include <new>                          // operator new / delete

#include "common/cmn_fade_layer.h"      // GetCmnFadeLayer, CmnFadeLayer*
#include "graphics/motion/anicode.h"    // YesNoSetMsg, YesNoSetSel, MsgPrintAt, LabelAlignGetX, GetLabelSuffixType, SetCurColorPacked, SetDefaultGsDrawState
#include "ingame/menu/anim_2d.h"        // LayoutAnim*
#include "ingame/menu/menu_csr.h"       // MenuCsr*
#include "system/b2d/B2dCtl.h"          // B2dCtlFindItem, B2dCtlSetItemHook, B2dGetDrawItem
#include "system/b2d/BpObj.h"           // BP_OBJ, BpObj*
#include "system/eeiop/cd_file_dat.h"   // CLEARMENU_PHF
#include "system/os/system.h"           // GetSysWrk, SYS_WRK::GetLanguage
#include "system/pad/pad.h"             // pad_wrk
#include "system/pad/pad_ctrl.h"        // CheckPadTrgPlaySe, pad_key
#include "ingame/menu/menu_mouse.h"    // PC: the mouse in menus

// ──────────────────────────────────────────────────────────────────────
// Data
// ──────────────────────────────────────────────────────────────────────

#define SEL_CSR             0x2b        // MenuCsr id
#define SEL_ANIM_YESNO      6           // layout anim type of the yes/no window
#define SEL_LA_OPEN         2           // LAYOUT_ANIM.state
#define SEL_YESNO_NONE      2           // YesNoSetSel: no choices shown
#define SEL_ASK_KIND        0x66        // the question: (0x66, 3)
#define SEL_ASK_MSG         3
#define SEL_ITEM_FADE       1000        // item the window and the fade layer fade
#define SEL_ITEM_CSR        0x3ee       // the cursor item
#define SEL_CSR_DY          0x1e        // rows 30 px apart (the game over menu: 40)
#define SEL_CSR_YOFS        0
#define SEL_PAD_LR          0xa000      // left / right (the yes/no choice)
#define SEL_COLOR_ON        0x97cdd380  // row under the cursor
#define SEL_COLOR_OFF       0x47797480

#define SE_CSR_MOVE         0x1a9b0000
#define SE_CANCEL           0x1a9b0001
#define SE_OK               0x1a9b0003

static const char sel_label_in[]  = "in";       // sdata 0x370e48
static const char sel_label_out[] = "out";      // sdata 0x370e50

static BP_OBJ             *sel_menu_obj;        // sdata gp-0x26b0
static const SEL_MENU_ROW *sel_menu_list;       // sbss gp-0xa0
static int                 sel_menu_row_num;    // sbss gp-0x9c
static int                 sel_menu_result;     // sbss gp-0x98: the chosen row's result
static int                 sel_menu_anim;       // sbss gp-0x94: yes/no window slot, -1 none
static int                 sel_menu_ask;        // sbss gp-0x90: 1 while the question shows
static char                sel_menu_no;         // sbss gp-0x8c: 1 "no" selected

// FF3 0x001ed658
void SelMenuSetList(const SEL_MENU_ROW *list)
{
    sel_menu_list = list;
}

// FF3 0x001ed660
static const SEL_MENU_ROW *SelMenuGetList(void)
{
    return sel_menu_list;
}

// ──────────────────────────────────────────────────────────────────────
// The yes/no window
// ──────────────────────────────────────────────────────────────────────

// ask 0: the cursor row's help line (in the row's own message kind), no
// choices; ask 1: the question, starting on "no".
// FF3 0x001ed668
static void SelMenuSetYesNo(int ask)
{
    const SEL_MENU_ROW *list;

    if (sel_menu_anim == -1)
    {
        sel_menu_anim = LayoutAnimStart(SEL_ANIM_YESNO, 0);
        // PC: FF3 sets the alpha and the flag before the -1 check (through
        // a null slot when none was free).
        if (sel_menu_anim == -1)
            return;
        LayoutAnimSetAlpha(sel_menu_anim, 0);
        LayoutAnimSetFlag(sel_menu_anim, LAYOUT_ANIM_LABEL_LAYOUT);
    }

    if (ask == 0)
    {
        sel_menu_ask = 0;
        list = SelMenuGetList();
        YesNoSetMsg(list[MenuCsrGetPos(SEL_CSR)].kind, list[MenuCsrGetPos(SEL_CSR)].help);
        YesNoSetSel(SEL_YESNO_NONE);
    }
    else
    {
        if (sel_menu_ask == 0)
        {
            sel_menu_no = 1;
            sel_menu_ask = 1;
        }
        YesNoSetMsg(SEL_ASK_KIND, SEL_ASK_MSG);
        YesNoSetSel(sel_menu_no);
    }
}

// FF3 0x001ed758
static void SelMenuMsgInit(void)
{
    sel_menu_ask = 0;
    sel_menu_anim = -1;
    SelMenuSetYesNo(0);
}

// FF3 0x001ed780
static int SelMenuMsgIsReady(void)
{
    return LayoutAnimIsDataReady(sel_menu_anim) != 0;
}

// FF3 0x001ed7a8
static void SelMenuMsgReset(void)
{
    if (sel_menu_anim != -1)
    {
        LayoutAnimClearFlag(sel_menu_anim, LAYOUT_ANIM_LABEL_LAYOUT);
        LayoutAnimSetAlphaItem(sel_menu_anim, nullptr);
        LayoutAnimReset(sel_menu_anim);
        sel_menu_anim = -1;
    }
}

// ──────────────────────────────────────────────────────────────────────
// Phase 43's interface
// ──────────────────────────────────────────────────────────────────────

// Unlike the game over menu every visit starts on the first row.
// FF3 0x001ed7f8
// callcheck: operator_new, BpObjCtor (new BP_OBJ)
void SelMenuInit(void)
{
    const SEL_MENU_ROW *row;
    BP_OBJ *obj;

    sel_menu_result = -1;
    obj = new BP_OBJ(GetSysWrk()->GetLanguage() + CLEARMENU_PHF);
    sel_menu_obj = obj;

    sel_menu_row_num = 0;
    for (row = SelMenuGetList(); row->id != -1; row++)
        sel_menu_row_num++;

    MenuCsrInit(SEL_CSR, 1, (short)sel_menu_row_num, 0);
    SelMenuMsgInit();
}

// Item hook of the rows: the row is the item name's 5th character
// ('1'..), its text aligned by the name's suffix and lit under the cursor.
// Unlike the game over menu the text stays at the item's y (no per-row
// offset).
// FF3 0x001ed8b0 (created this session; it was the tail of SelMenuInit's
// body).
static int SelMenuDrawRow(const B2D_HOOK_ARGS *args)
{
    const char         *name = (const char *)args->a0;
    const int          *xy   = (const int *)args->t1;
    int                 row  = name[4] - '1';
    const SEL_MENU_ROW *list = SelMenuGetList();
    B2D_ITEM           *item = B2dGetDrawItem();
    int                 type = GetLabelSuffixType(item->name);
    int                 x    = LabelAlignGetX(type, xy);

    SetCurColorPacked(row == MenuCsrGetPos(SEL_CSR) ? SEL_COLOR_ON : SEL_COLOR_OFF);
    MsgPrintAt(list[row].kind, list[row].msg, x, xy[1],
               (int)args->stack[2], item->part[B2D_PART_TEXT].value, 0, 0, type);
    SetDefaultGsDrawState();
    return 0;
}

// Layout and window loaded: "in" plays, both fade item 1000, the fade
// layer shows, the cursor and the row hooks are set.
// FF3 0x001ed998
int SelMenuLoad(void)
{
    if (sel_menu_obj->Load() == 0)
        return 0;
    if (SelMenuMsgIsReady() == 0)
        return 0;

    sel_menu_obj->In(sel_label_in);
    LayoutAnimSetAlphaItemById(sel_menu_anim, sel_menu_obj->GetHandle(), SEL_ITEM_FADE);
    GetCmnFadeLayer()->SetItem(sel_menu_obj->GetHandle(), SEL_ITEM_FADE);
    GetCmnFadeLayer()->Show(0);
    sel_menu_obj->AddCsr(SEL_ITEM_CSR, 0, SEL_CSR_DY, SEL_CSR_YOFS);
    sel_menu_obj->SetCsrPos(SEL_ITEM_CSR, 0, MenuCsrGetPos(SEL_CSR));

    for (const SEL_MENU_ROW *row = SelMenuGetList(); row->id != -1; row++)
    {
        B2D_ITEM_HOOK hook = { SelMenuDrawRow };
        B2dCtlSetItemHook(B2dCtlFindItem(sel_menu_obj->GetHandle(), row->id), &hook);
    }
    return 1;
}

// FF3 0x001edae0
int SelMenuIsInEnd(void)
{
    return sel_menu_obj->IsInEnd() != 0;
}

// Up / down move the cursor; cancel jumps to the last row and chooses it,
// OK chooses the cursor row. Unlike the game over menu the OK sound plays
// through CheckPadTrgPlaySe.
// FF3 0x001edb08
int SelMenuSelect(void)
{
    SelMenuSetYesNo(0);
    MenuMouseCsrObjId(SEL_CSR, sel_menu_obj, SEL_ITEM_CSR);         // PC
    MenuCsrUpdate(SEL_CSR, 0);
    if (MenuCsrIsMovedSe(SEL_CSR, SE_CSR_MOVE))
    {
        sel_menu_obj->SetCsrPos(SEL_ITEM_CSR, 0, MenuCsrGetPos(SEL_CSR));
        return 0;
    }
    if (CheckPadTrgPlaySe((u_short)pad_key[PAD_KEY_CANCEL], SE_CANCEL, 0))
    {
        MenuCsrSetPos(SEL_CSR, (short)(sel_menu_row_num - 1));
        sel_menu_obj->SetCsrPos(SEL_ITEM_CSR, 0, MenuCsrGetPos(SEL_CSR));
    }
    else if (!CheckPadTrgPlaySe((u_short)pad_key[PAD_KEY_OK], SE_OK, 0))
    {
        return 0;
    }
    sel_menu_result = SelMenuGetList()[MenuCsrGetPos(SEL_CSR)].result;
    return 1;
}

// The question once the window is open: left / right switch, OK answers
// (1 yes, -1 no), cancel answers no.
// FF3 0x001edc18
int SelMenuConfirm(void)
{
    SelMenuSetYesNo(1);
    if (LayoutAnimGetState(sel_menu_anim) != SEL_LA_OPEN)
        return 0;

    if (CheckPadTrgPlaySe(SEL_PAD_LR, SE_CSR_MOVE, 0))
    {
        sel_menu_no = 1 - sel_menu_no;
    }
    else if (CheckPadTrgPlaySe((u_short)pad_key[PAD_KEY_OK], SE_OK, 0))
    {
        return sel_menu_no != 0 ? -1 : 1;
    }
    else if (CheckPadTrgPlaySe((u_short)pad_key[PAD_KEY_CANCEL], SE_CANCEL, 0))
    {
        sel_menu_no = 1;
        return -1;
    }
    return 0;
}

// FF3 0x001edcd8
int SelMenuGetResult(void)
{
    return sel_menu_result;
}

// FF3 0x001edce0
int SelMenuIsOut(void)
{
    return sel_menu_obj->Out(sel_label_out) != 0;
}

// Frees the cursor, the fade layer's item, the window and the layout.
// FF3 0x001edd08
// callcheck: operator_delete (operator delete)
void SelMenuRelease(void)
{
    BP_OBJ *obj;

    MenuCsrRelease(SEL_CSR);
    pad_wrk[0].stick_dpad = 0;
    sel_menu_obj->FreeCsr();
    GetCmnFadeLayer()->SetItemHandle(nullptr);
    GetCmnFadeLayer()->Term();
    SelMenuMsgReset();
    obj = sel_menu_obj;
    if (obj != nullptr)
    {
        obj->Release();
        operator delete(obj);
    }
    sel_menu_obj = nullptr;
}
