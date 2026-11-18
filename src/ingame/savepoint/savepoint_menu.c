/* ==========================================================================
 *  ingame/savepoint/savepoint_menu.c
 *
 *  The save point menu (see savepoint_menu.h). FF3 0x135690..0x135e28,
 *  FF3's own code. FF3 0x00135e08 is the GPhaseCtrl atexit thunk that ends
 *  the file (compiler-generated, ~GPHASE_CTRL in gphase.h). A BP_OBJ of SAVEPOINT_PHF + language with a cursor
 *  (MenuCsr 0x29, BP_OBJ cursor item 0x3ee) over the rows of
 *  savepoint_menu_row, a yes/no window (LayoutAnim type 6) that shows the
 *  row's help line and asks before the title row, and an item hook that
 *  prints each row's text.
 * ======================================================================== */
#include "ingame/savepoint/savepoint_menu.h"

#include <new>                          // operator new / delete

#include "common/cmn_fade_layer.h"      // GetCmnFadeLayer, CmnFadeLayer*
#include "graphics/motion/anicode.h"    // YesNoSetMsg, YesNoSetSel, MsgPrintAt, LabelAlignGetX, GetLabelSuffixType, SetCurColorPacked, SetDefaultGsDrawState
#include "ingame/menu/anim_2d.h"        // LayoutAnim*
#include "ingame/menu/menu_csr.h"       // MenuCsr*
#include "ingame/sound/snd_ctrl.h"      // SndSePlay
#include "main/gphase.h"                // GetGPhaseCtrl
#include "system/b2d/B2dCtl.h"          // B2dCtlFindItem, B2dCtlSetItemHook, B2dGetDrawItem
#include "system/b2d/BpObj.h"           // BP_OBJ, BpObj*
#include "system/eeiop/cd_file_dat.h"   // SAVEPOINT_PHF
#include "system/os/system.h"           // GetSysWrk, SYS_WRK::GetLanguage
#include "system/pad/pad.h"             // pad_wrk
#include "system/pad/pad_ctrl.h"        // CheckPadTrgPlaySe, pad_key
#include "ingame/menu/menu_mouse.h"    // PC: the mouse in menus

// ──────────────────────────────────────────────────────────────────────
// Data
// ──────────────────────────────────────────────────────────────────────

#define SPM_CSR             0x29        // MenuCsr id
#define SPM_MSG_KIND        0x69        // message kind of the rows and help lines
#define SPM_ANIM_YESNO      6           // layout anim type of the yes/no window
#define SPM_LA_OPEN         2           // LAYOUT_ANIM.state
#define SPM_YESNO_NONE      2           // YesNoSetSel: no choices shown
#define SPM_ASK_KIND        0x66        // the question: (0x66, 3)
#define SPM_ASK_MSG         3
#define SPM_ITEM_FADE       0x3ec       // item the window and the fade layer fade
#define SPM_ITEM_CSR        0x3ee       // the cursor item
#define SPM_CSR_DY          0x1e        // rows 30 px apart
#define SPM_FROM_NORMAL     0x20        // GPHASE_CTRL.from Story_Normal: a new visit
#define SPM_PAD_LR          0xa000      // left / right (the yes/no choice)
#define SPM_COLOR_ON        0x97cdd380  // row under the cursor
#define SPM_COLOR_OFF       0x47797480

#define SE_CSR_MOVE         0x1a9b0000
#define SE_CANCEL           0x1a9b0001
#define SE_OK               0x1a9b0003

// ROM 0x34f160 (sdata 0x36bb28 points at it): save, phase 8, leave. The
// next sdata word 0x36bb2c points at a second table 0x34f1a0 that only
// differs in the last row's item (0x3ed); nothing reads it.
static const MENU_PHASE_ROW savepoint_menu_row[] =
{
    { 0x41b, 0, 4, 0x45 },              // Story_SaveGame_Mc
    { 0x3f0, 1, 5, 8 },
    { 0x3f1, 3, 7, -1 },                // leave; cancel jumps here
    { -1, -1, -1, -1 },
};

static const char spm_label_in[]  = "in";       // sdata 0x36bb38
static const char spm_label_out[] = "out";      // sdata 0x36bb40

static BP_OBJ *savepoint_menu_obj;              // sdata gp-0x79c0
static short   savepoint_menu_row_keep;         // sbss gp-0x784: cursor row kept between visits, -1 none
static int     savepoint_menu_row_num;          // sbss gp-0x780
static int     savepoint_menu_phase;            // sbss gp-0x77c: the chosen row's phase
static int     savepoint_menu_anim;             // sbss gp-0x778: yes/no window slot, -1 none
static int     savepoint_menu_ask;              // sbss gp-0x774: 1 while the question shows
static char    savepoint_menu_no;               // sbss gp-0x770: 1 "no" selected

// ──────────────────────────────────────────────────────────────────────
// The yes/no window
// ──────────────────────────────────────────────────────────────────────

// ask 0: the cursor row's help line, no choices; ask 1: the question,
// starting on "no".
// FF3 0x00135690.
static void SavePointMenuMsgUpdate(int ask)
{
    if (savepoint_menu_anim == -1)
    {
        savepoint_menu_anim = LayoutAnimStart(SPM_ANIM_YESNO, 0);
        // PC: FF3 sets the alpha and the flag before the -1 check (through
        // a null slot when none was free).
        if (savepoint_menu_anim == -1)
            return;
        LayoutAnimSetAlpha(savepoint_menu_anim, 0);
        LayoutAnimSetFlag(savepoint_menu_anim, LAYOUT_ANIM_LABEL_LAYOUT);
    }

    if (ask == 0)
    {
        savepoint_menu_ask = 0;
        YesNoSetMsg(SPM_MSG_KIND, savepoint_menu_row[MenuCsrGetPos(SPM_CSR)].help);
        YesNoSetSel(SPM_YESNO_NONE);
    }
    else
    {
        if (savepoint_menu_ask == 0)
        {
            savepoint_menu_no = 1;
            savepoint_menu_ask = 1;
        }
        YesNoSetMsg(SPM_ASK_KIND, SPM_ASK_MSG);
        YesNoSetSel(savepoint_menu_no);
    }
}

// FF3 0x00135760.
static void SavePointMenuMsgInit(void)
{
    savepoint_menu_ask = 0;
    savepoint_menu_anim = -1;
    SavePointMenuMsgUpdate(0);
}

// FF3 0x00135788.
static int SavePointMenuMsgIsReady(void)
{
    return LayoutAnimIsDataReady(savepoint_menu_anim) != 0;
}

// FF3 0x001357b0.
static void SavePointMenuMsgReset(void)
{
    if (savepoint_menu_anim != -1)
    {
        LayoutAnimClearFlag(savepoint_menu_anim, LAYOUT_ANIM_LABEL_LAYOUT);
        LayoutAnimSetAlphaItem(savepoint_menu_anim, nullptr);
        LayoutAnimReset(savepoint_menu_anim);
        savepoint_menu_anim = -1;
    }
}

// ──────────────────────────────────────────────────────────────────────
// Phase 41's interface
// ──────────────────────────────────────────────────────────────────────

// A new visit from Story_Normal starts on the first row, a return from
// the chosen phase on the row it left.
// FF3 0x00135800.
// callcheck: operator_new, BpObjCtor (new BP_OBJ)
void SavePointMenuCreate(void)
{
    BP_OBJ *obj;

    savepoint_menu_phase = -1;
    obj = new BP_OBJ(GetSysWrk()->GetLanguage() + SAVEPOINT_PHF);
    savepoint_menu_obj = obj;

    if (GetGPhaseCtrl()->from == SPM_FROM_NORMAL)
        savepoint_menu_row_keep = -1;

    savepoint_menu_row_num = 0;
    while (savepoint_menu_row[savepoint_menu_row_num].id != -1)
        savepoint_menu_row_num++;

    MenuCsrInit(SPM_CSR, 1, (short)savepoint_menu_row_num, 0);
    if (savepoint_menu_row_keep != -1)
        MenuCsrSetPos(SPM_CSR, savepoint_menu_row_keep);
    SavePointMenuMsgInit();
}

// Item hook of the rows: the row is the item name's 5th character
// ('1'..), its text aligned by the name's suffix and lit under the cursor.
// FF3 0x001358f8 (created this session; it was the tail of
// SavePointMenuCreate's body).
static int SavePointMenuDrawRow(const B2D_HOOK_ARGS *args)
{
    const char *name  = (const char *)args->a0;
    const int  *xy    = (const int *)args->t1;
    int         row   = name[4] - '1';
    B2D_ITEM   *item  = B2dGetDrawItem();
    int         type  = GetLabelSuffixType(item->name);
    int         x     = LabelAlignGetX(type, xy);

    SetCurColorPacked(row == MenuCsrGetPos(SPM_CSR) ? SPM_COLOR_ON : SPM_COLOR_OFF);
    MsgPrintAt(SPM_MSG_KIND, savepoint_menu_row[row].msg, x, xy[1], (int)args->stack[2],
               item->part[B2D_PART_TEXT].value, 0, 0, type);
    SetDefaultGsDrawState();
    return 0;
}

// Layout and window loaded: "in" plays, both fade item 0x3ec, the fade
// layer shows, the cursor and the row hooks are set.
// FF3 0x001359d0.
int SavePointMenuIsLoaded(void)
{
    if (savepoint_menu_obj->Load() == 0)
        return 0;
    if (SavePointMenuMsgIsReady() == 0)
        return 0;

    savepoint_menu_obj->In(spm_label_in);
    LayoutAnimSetAlphaItemById(savepoint_menu_anim, savepoint_menu_obj->GetHandle(), SPM_ITEM_FADE);
    GetCmnFadeLayer()->SetItem(savepoint_menu_obj->GetHandle(), SPM_ITEM_FADE);
    GetCmnFadeLayer()->Show(0);
    savepoint_menu_obj->AddCsr(SPM_ITEM_CSR, 0, SPM_CSR_DY, 0);
    savepoint_menu_obj->SetCsrPos(SPM_ITEM_CSR, 0, MenuCsrGetPos(SPM_CSR));

    for (const MENU_PHASE_ROW *row = savepoint_menu_row; row->id != -1; row++)
    {
        B2D_ITEM_HOOK hook = { SavePointMenuDrawRow };
        B2dCtlSetItemHook(B2dCtlFindItem(savepoint_menu_obj->GetHandle(), row->id), &hook);
    }
    return 1;
}

// FF3 0x00135b10.
int SavePointMenuIsInEnd(void)
{
    return savepoint_menu_obj->IsInEnd() != 0;
}

// Up / down move the cursor; cancel jumps to the last row (leave) and
// chooses it, OK chooses the cursor row. The help line follows the cursor.
// FF3 0x00135b38.
int SavePointMenuSelect(void)
{
    int chosen = 0;

    MenuMouseCsrObjId(SPM_CSR, savepoint_menu_obj, SPM_ITEM_CSR);   // PC
    MenuCsrUpdate(SPM_CSR, 0);
    if (MenuCsrIsMovedSe(SPM_CSR, SE_CSR_MOVE))
    {
        savepoint_menu_obj->SetCsrPos(SPM_ITEM_CSR, 0, MenuCsrGetPos(SPM_CSR));
    }
    else if (CheckPadTrgPlaySe((u_short)pad_key[PAD_KEY_CANCEL], SE_CANCEL, 0))
    {
        chosen = 1;
        MenuCsrSetPos(SPM_CSR, (short)(savepoint_menu_row_num - 1));
        savepoint_menu_obj->SetCsrPos(SPM_ITEM_CSR, 0, MenuCsrGetPos(SPM_CSR));
        savepoint_menu_phase = savepoint_menu_row[MenuCsrGetPos(SPM_CSR)].phase;
    }
    else if (CheckPadTrgPlaySe((u_short)pad_key[PAD_KEY_OK], -1, 0))
    {
        chosen = 1;
        savepoint_menu_phase = savepoint_menu_row[MenuCsrGetPos(SPM_CSR)].phase;
        SndSePlay(SE_OK, 100, 0x1000, 0, nullptr, 0);
    }
    SavePointMenuMsgUpdate(0);
    return chosen;
}

// The question once the window is open: left / right switch, OK answers
// (1 yes, -1 no), cancel answers no.
// FF3 0x00135c80.
int SavePointMenuConfirm(void)
{
    SavePointMenuMsgUpdate(1);
    if (LayoutAnimGetState(savepoint_menu_anim) != SPM_LA_OPEN)
        return 0;

    if (CheckPadTrgPlaySe(SPM_PAD_LR, SE_CSR_MOVE, 0))
    {
        savepoint_menu_no = 1 - savepoint_menu_no;
    }
    else if (CheckPadTrgPlaySe((u_short)pad_key[PAD_KEY_OK], SE_OK, 0))
    {
        return savepoint_menu_no != 0 ? -1 : 1;
    }
    else if (CheckPadTrgPlaySe((u_short)pad_key[PAD_KEY_CANCEL], SE_CANCEL, 0))
    {
        savepoint_menu_no = 1;
        return -1;
    }
    return 0;
}

// FF3 0x00135d40.
int SavePointMenuGetPhase(void)
{
    return savepoint_menu_phase;
}

// FF3 0x00135d48.
int SavePointMenuOutWait(void)
{
    return savepoint_menu_obj->Out(spm_label_out) != 0;
}

// Keeps the cursor row for the return, frees the cursor, the fade layer's
// item, the window and the layout.
// FF3 0x00135d70.
// callcheck: operator_delete (operator delete)
void SavePointMenuRelease(void)
{
    BP_OBJ *obj;

    savepoint_menu_row_keep = MenuCsrGetPos(SPM_CSR);
    MenuCsrRelease(SPM_CSR);
    pad_wrk[0].stick_dpad = 0;
    savepoint_menu_obj->FreeCsr();
    GetCmnFadeLayer()->SetItemHandle(nullptr);
    GetCmnFadeLayer()->Term();
    SavePointMenuMsgReset();
    obj = savepoint_menu_obj;
    if (obj != nullptr)
    {
        obj->Release();
        operator delete(obj);
    }
    savepoint_menu_obj = nullptr;
}
