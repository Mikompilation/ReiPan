/* ==========================================================================
 *  ingame/gameover_menu.c
 *
 *  The game over menu (see gameover_menu.h). FF3 0x133960..0x1340f8,
 *  FF3's own code: ingame/savepoint/savepoint_menu.c over GAMEOVER_PHF,
 *  with MenuCsr 0x28, message kind 0x6c and rows 40 px apart. What
 *  differs from the save point menu is commented where it happens.
 * ======================================================================== */
#include "ingame/gameover_menu.h"

#include <new>                          // operator new / delete

#include "common/cmn_fade_layer.h"      // GetCmnFadeLayer, CmnFadeLayer*
#include "graphics/motion/anicode.h"    // YesNoSetMsg, YesNoSetSel, MsgPrintAt, LabelAlignGetX, GetLabelSuffixType, SetCurColorPacked, SetDefaultGsDrawState
#include "ingame/menu/anim_2d.h"        // LayoutAnim*
#include "ingame/menu/menu_csr.h"       // MenuCsr*
#include "ingame/savepoint/savepoint_menu.h"  // MENU_PHASE_ROW
#include "ingame/sound/snd_ctrl.h"      // SndSePlay
#include "main/gphase.h"                // GetGPhaseCtrl
#include "system/b2d/B2dCtl.h"          // B2dCtlFindItem, B2dCtlSetItemHook, B2dGetDrawItem
#include "system/b2d/BpObj.h"           // BP_OBJ, BpObj*
#include "system/eeiop/cd_file_dat.h"   // GAMEOVER_PHF
#include "system/os/system.h"           // GetSysWrk, SYS_WRK::GetLanguage
#include "system/pad/pad.h"             // pad_wrk
#include "system/pad/pad_ctrl.h"        // CheckPadTrgPlaySe, pad_key
#include "ingame/menu/menu_mouse.h"    // PC: the mouse in menus

// ──────────────────────────────────────────────────────────────────────
// Data
// ──────────────────────────────────────────────────────────────────────

#define GOM_CSR             0x28        // MenuCsr id
#define GOM_MSG_KIND        0x6c        // message kind of the rows and help lines
#define GOM_ANIM_YESNO      6           // layout anim type of the yes/no window
#define GOM_LA_OPEN         2           // LAYOUT_ANIM.state
#define GOM_YESNO_NONE      2           // YesNoSetSel: no choices shown
#define GOM_ASK_KIND        0x66        // the question: (0x66, 3)
#define GOM_ASK_MSG         3
#define GOM_ITEM_FADE       1000        // item the window and the fade layer fade
#define GOM_ITEM_CSR        0x3ee       // the cursor item
#define GOM_CSR_DY          0x28        // rows 40 px apart
#define GOM_CSR_YOFS        10
#define GOM_ROW_DY          10          // the text moves 10 px per row (+10)
#define GOM_FROM_PHASE      10          // GPHASE_CTRL.from 10: a new visit
#define GOM_PAD_LR          0xa000      // left / right (the yes/no choice)
#define GOM_COLOR_ON        0x97cdd380  // row under the cursor
#define GOM_COLOR_OFF       0x47797480

#define SE_CSR_MOVE         0x1a9b0000
#define SE_CANCEL           0x1a9b0001
#define SE_OK               0x1a9b0003

// ROM 0x34eea8 (sdata 0x36bad0 points at it). The title row (phase 5)
// asks first (ingame/story_save.c's STORY_SAVE_PHASE_TITLE rule).
static const MENU_PHASE_ROW gameover_menu_row[] =
{
    { 0x41b, 1, 4, 8 },
    { 0x3f0, 2, 5, 5 },                 // Title_Mode; cancel jumps here
    { -1, -1, -1, -1 },
};

static const char gom_label_in[]  = "in";       // sdata 0x36bad8
static const char gom_label_out[] = "out";      // sdata 0x36bae0

static BP_OBJ *gameover_menu_obj;               // sdata gp-0x7a1c
static short   gameover_menu_row_keep;          // sbss gp-0x7ac: cursor row kept between visits, -1 none
static int     gameover_menu_row_num;           // sbss gp-0x7a8
static int     gameover_menu_phase;             // sbss gp-0x7a4: the chosen row's phase
static int     gameover_menu_anim;              // sbss gp-0x7a0: yes/no window slot, -1 none
static int     gameover_menu_ask;               // sbss gp-0x79c: 1 while the question shows
static char    gameover_menu_no;                // sbss gp-0x798: 1 "no" selected

// ──────────────────────────────────────────────────────────────────────
// The yes/no window
// ──────────────────────────────────────────────────────────────────────

// ask 0: the cursor row's help line, no choices; ask 1: the question,
// starting on "no".
// FF3 0x00133960.
static void GameOverMenuMsgUpdate(int ask)
{
    if (gameover_menu_anim == -1)
    {
        gameover_menu_anim = LayoutAnimStart(GOM_ANIM_YESNO, 0);
        // PC: FF3 sets the alpha and the flag before the -1 check (through
        // a null slot when none was free).
        if (gameover_menu_anim == -1)
            return;
        LayoutAnimSetAlpha(gameover_menu_anim, 0);
        LayoutAnimSetFlag(gameover_menu_anim, LAYOUT_ANIM_LABEL_LAYOUT);
    }

    if (ask == 0)
    {
        gameover_menu_ask = 0;
        YesNoSetMsg(GOM_MSG_KIND, gameover_menu_row[MenuCsrGetPos(GOM_CSR)].help);
        YesNoSetSel(GOM_YESNO_NONE);
    }
    else
    {
        if (gameover_menu_ask == 0)
        {
            gameover_menu_no = 1;
            gameover_menu_ask = 1;
        }
        YesNoSetMsg(GOM_ASK_KIND, GOM_ASK_MSG);
        YesNoSetSel(gameover_menu_no);
    }
}

// FF3 0x00133a30.
static void GameOverMenuMsgInit(void)
{
    gameover_menu_ask = 0;
    gameover_menu_anim = -1;
    GameOverMenuMsgUpdate(0);
}

// FF3 0x00133a58.
static int GameOverMenuMsgIsReady(void)
{
    return LayoutAnimIsDataReady(gameover_menu_anim) != 0;
}

// FF3 0x00133a80.
static void GameOverMenuMsgReset(void)
{
    if (gameover_menu_anim != -1)
    {
        LayoutAnimClearFlag(gameover_menu_anim, LAYOUT_ANIM_LABEL_LAYOUT);
        LayoutAnimSetAlphaItem(gameover_menu_anim, nullptr);
        LayoutAnimReset(gameover_menu_anim);
        gameover_menu_anim = -1;
    }
}

// ──────────────────────────────────────────────────────────────────────
// Phase 40's interface
// ──────────────────────────────────────────────────────────────────────

// A visit requested by phase 10 starts on the first row, a return from
// the chosen phase on the row it left.
// FF3 0x00133ad0.
// callcheck: operator_new, BpObjCtor (new BP_OBJ)
void GameOverMenuCreate(void)
{
    BP_OBJ *obj;

    gameover_menu_phase = -1;
    obj = new BP_OBJ(GetSysWrk()->GetLanguage() + GAMEOVER_PHF);
    gameover_menu_obj = obj;

    if (GetGPhaseCtrl()->from == GOM_FROM_PHASE)
        gameover_menu_row_keep = -1;

    gameover_menu_row_num = 0;
    while (gameover_menu_row[gameover_menu_row_num].id != -1)
        gameover_menu_row_num++;

    MenuCsrInit(GOM_CSR, 1, (short)gameover_menu_row_num, 0);
    if (gameover_menu_row_keep != -1)
        MenuCsrSetPos(GOM_CSR, gameover_menu_row_keep);
    GameOverMenuMsgInit();
}

// Item hook of the rows: the row is the item name's 5th character
// ('1'..), its text aligned by the name's suffix and lit under the cursor.
// Unlike the save point menu the text moves down 10 px per row (+10).
// FF3 0x00133bc8 (created this session; it was the tail of
// GameOverMenuCreate's body).
static int GameOverMenuDrawRow(const B2D_HOOK_ARGS *args)
{
    const char *name  = (const char *)args->a0;
    const int  *xy    = (const int *)args->t1;
    int         row   = name[4] - '1';
    B2D_ITEM   *item  = B2dGetDrawItem();
    int         type  = GetLabelSuffixType(item->name);
    int         x     = LabelAlignGetX(type, xy);

    SetCurColorPacked(row == MenuCsrGetPos(GOM_CSR) ? GOM_COLOR_ON : GOM_COLOR_OFF);
    MsgPrintAt(GOM_MSG_KIND, gameover_menu_row[row].msg, x, xy[1] + row * GOM_ROW_DY + GOM_ROW_DY,
               (int)args->stack[2], item->part[B2D_PART_TEXT].value, 0, 0, type);
    SetDefaultGsDrawState();
    return 0;
}

// Layout and window loaded: "in" plays, both fade item 1000, the fade
// layer shows, the cursor and the row hooks are set.
// FF3 0x00133cb0.
int GameOverMenuIsLoaded(void)
{
    if (gameover_menu_obj->Load() == 0)
        return 0;
    if (GameOverMenuMsgIsReady() == 0)
        return 0;

    gameover_menu_obj->In(gom_label_in);
    LayoutAnimSetAlphaItemById(gameover_menu_anim, gameover_menu_obj->GetHandle(), GOM_ITEM_FADE);
    GetCmnFadeLayer()->SetItem(gameover_menu_obj->GetHandle(), GOM_ITEM_FADE);
    GetCmnFadeLayer()->Show(0);
    gameover_menu_obj->AddCsr(GOM_ITEM_CSR, 0, GOM_CSR_DY, GOM_CSR_YOFS);
    gameover_menu_obj->SetCsrPos(GOM_ITEM_CSR, 0, MenuCsrGetPos(GOM_CSR));

    for (const MENU_PHASE_ROW *row = gameover_menu_row; row->id != -1; row++)
    {
        B2D_ITEM_HOOK hook = { GameOverMenuDrawRow };
        B2dCtlSetItemHook(B2dCtlFindItem(gameover_menu_obj->GetHandle(), row->id), &hook);
    }
    return 1;
}

// FF3 0x00133df0.
int GameOverMenuIsInEnd(void)
{
    return gameover_menu_obj->IsInEnd() != 0;
}

// Up / down move the cursor; cancel jumps to the last row and chooses it,
// OK chooses the cursor row. Unlike the save point menu the help line is
// set first, before the cursor moves.
// FF3 0x00133e18.
int GameOverMenuSelect(void)
{
    GameOverMenuMsgUpdate(0);
    MenuMouseCsrObjId(GOM_CSR, gameover_menu_obj, GOM_ITEM_CSR);    // PC
    MenuCsrUpdate(GOM_CSR, 0);
    if (MenuCsrIsMovedSe(GOM_CSR, SE_CSR_MOVE))
    {
        gameover_menu_obj->SetCsrPos(GOM_ITEM_CSR, 0, MenuCsrGetPos(GOM_CSR));
        return 0;
    }
    if (CheckPadTrgPlaySe((u_short)pad_key[PAD_KEY_CANCEL], SE_CANCEL, 0))
    {
        MenuCsrSetPos(GOM_CSR, (short)(gameover_menu_row_num - 1));
        gameover_menu_obj->SetCsrPos(GOM_ITEM_CSR, 0, MenuCsrGetPos(GOM_CSR));
        gameover_menu_phase = gameover_menu_row[MenuCsrGetPos(GOM_CSR)].phase;
        return 1;
    }
    if (CheckPadTrgPlaySe((u_short)pad_key[PAD_KEY_OK], -1, 0))
    {
        gameover_menu_phase = gameover_menu_row[MenuCsrGetPos(GOM_CSR)].phase;
        SndSePlay(SE_OK, 100, 0x1000, 0, nullptr, 0);
        return 1;
    }
    return 0;
}

// The question once the window is open: left / right switch, OK answers
// (1 yes, -1 no), cancel answers no.
// FF3 0x00133f50.
int GameOverMenuConfirm(void)
{
    GameOverMenuMsgUpdate(1);
    if (LayoutAnimGetState(gameover_menu_anim) != GOM_LA_OPEN)
        return 0;

    if (CheckPadTrgPlaySe(GOM_PAD_LR, SE_CSR_MOVE, 0))
    {
        gameover_menu_no = 1 - gameover_menu_no;
    }
    else if (CheckPadTrgPlaySe((u_short)pad_key[PAD_KEY_OK], SE_OK, 0))
    {
        return gameover_menu_no != 0 ? -1 : 1;
    }
    else if (CheckPadTrgPlaySe((u_short)pad_key[PAD_KEY_CANCEL], SE_CANCEL, 0))
    {
        gameover_menu_no = 1;
        return -1;
    }
    return 0;
}

// FF3 0x00134010.
int GameOverMenuGetPhase(void)
{
    return gameover_menu_phase;
}

// FF3 0x00134018.
int GameOverMenuOutWait(void)
{
    return gameover_menu_obj->Out(gom_label_out) != 0;
}

// Keeps the cursor row for the return, frees the cursor, the fade layer's
// item, the window and the layout.
// FF3 0x00134040.
// callcheck: operator_delete (operator delete)
void GameOverMenuRelease(void)
{
    BP_OBJ *obj;

    gameover_menu_row_keep = MenuCsrGetPos(GOM_CSR);
    MenuCsrRelease(GOM_CSR);
    pad_wrk[0].stick_dpad = 0;
    gameover_menu_obj->FreeCsr();
    GetCmnFadeLayer()->SetItemHandle(nullptr);
    GetCmnFadeLayer()->Term();
    GameOverMenuMsgReset();
    obj = gameover_menu_obj;
    if (obj != nullptr)
    {
        obj->Release();
        operator delete(obj);
    }
    gameover_menu_obj = nullptr;
}
