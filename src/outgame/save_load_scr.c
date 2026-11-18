/* ==========================================================================
 *  outgame/save_load_scr.c
 *
 *  The memory-card load screen (see save_load_scr.h). FF3 0x130950..0x132118,
 *  FF3's own code; the save screen at 0x132118 repeats every helper over
 *  McSaveGame's work.
 * ======================================================================== */
#include "outgame/save_load_scr.h"

#include <new>                          // operator new / delete
#include <string.h>                     // strlen, strncmp

#include "common/cmn_fade_layer.h"      // GetCmnFadeLayer, CmnFadeLayer*
#include "graphics/graph2d/message.h"   // GetMsgDataAddr, PrintMsg_ArrangeOneLine
#include "graphics/motion/anicode.h"    // B2D_HOOK_REC, B2dSetHookList, YesNoSetMsg, YesNoSetSel, SetCurColorPacked, SetDefaultGsDrawState
#include "ingame/menu/anim_2d.h"        // LayoutAnim*
#include "outgame/save_info_disp.h"     // SaveInfo*
#include "system/b2d/B2dCtl.h"          // B2dCtlFindItem, B2dCtlSetItemHook, B2dGetDrawItem
#include "system/b2d/B2dGridSpr.h"      // B2D_GRID_SPR
#include "system/b2d/B2dSpr.h"          // B2dSprSetDisp
#include "system/b2d/BpCtl.h"           // BpCtlGetHandleByFileNo, B2dCtlHasItemName, B2dDrawSpriteDirect
#include "system/b2d/BpObj.h"           // BP_OBJ, BpObj*
#include "system/eeiop/cd_file_dat.h"   // SAVELOAD_PHF
#include "system/mc/prg/mc_set_data.h"  // GetMemoryCardPlayData*
#include "system/os/system.h"           // GetSysWrk, SYS_WRK::GetLanguage
#include "ingame/menu/menu_mouse.h"    // PC: the mouse in menus

// ──────────────────────────────────────────────────────────────────────
// Data
// ──────────────────────────────────────────────────────────────────────

#define SLR_ANIM_YESNO      0xb         // layout anim type of the yes/no window
#define SLR_MSG_KIND        0x83        // message kind of McLoadGame's messages
#define SLR_YESNO_NONE      2           // YesNoSetSel: no choices shown
#define SLR_LA_OPEN         2           // LAYOUT_ANIM.state
#define SLR_LA_CLOSED       4
#define SLR_SLOT_STEP       0x6e        // x distance between two slots
#define SLR_FADE_ITEM       0x403       // item the window and the fade layer fade

// Layout items (ROM 0x34f058: {id, name} pairs; the code reads the ids).
enum
{
    SLR_ITEM_TITLE       = 0x3ed,       // the save screen's title, hidden here
    SLR_ITEM_ARROW       = 0x3ef,       // "R sankaku"
    SLR_ITEM_ARROW_FLEA  = 0x3ee,       // "R sankaku_flea"
    SLR_ITEM_ARROW2      = 0x400,       // "R sankaku"
    SLR_ITEM_ARROW_FLEA2 = 0x3ff,       // "R sankaku_flea"
    SLR_ITEM_CSL         = 0x3f1,       // "R csl"
    SLR_ITEM_CSL_FLEA    = 0x3fc,       // "R csl_flea"
    SLR_ITEM_CLEAR       = 0x3f0,       // "R clear"
    SLR_ITEM_CARD_SLOT   = 0x40b,       // "slot_M"
    SLR_ITEM_PLAY_TIME   = 0x40d,
    SLR_ITEM_DIFFICULTY  = 0x413,
    SLR_ITEM_CHAPTER     = 0x40e,
    SLR_ITEM_PLAYER      = 0x412,
    SLR_ITEM_INFO1C      = 0x411,
    SLR_ITEM_INFO1C_NUM  = 0x40c,
    SLR_ITEM_NONE        = 0x418,
    SLR_ITEM_INFO1D      = 0x414,
};

// ROM 0x34f058..: the five thumbnails "mapsnap1".."mapsnap5", then the
// unselected / selected slot numbers "R No1_hisentaku".. / "R No1_sentaku"..
static const int slr_thumb_item[SAVE_LOAD_SLOTS]   = { 0x405, 0x406, 0x407, 0x408, 0x409 };
static const int slr_slot_off_item[SAVE_LOAD_SLOTS] = { 0x3f2, 0x3f3, 0x3f4, 0x3f5, 0x3f6 };
static const int slr_slot_on_item[SAVE_LOAD_SLOTS]  = { 0x3f7, 0x3f8, 0x3f9, 0x3fa, 0x3fb };
static const int slr_csl_item[2]   = { SLR_ITEM_CSL, SLR_ITEM_CSL_FLEA };
static const int slr_arrow_item[4] = { SLR_ITEM_ARROW, SLR_ITEM_ARROW2, SLR_ITEM_ARROW_FLEA, SLR_ITEM_ARROW_FLEA2 };

static const char slr_label_in[]  = "in";       // sdata 0x36baa0
static const char slr_label_out[] = "out";      // sdata 0x36baa8

static int           save_load_scr_phase;       // sdata gp-0x7dc: SaveLoadScrCreate's phase (no reader)
static B2D_GRID_SPR *save_load_scr_csl[2];      // sdata gp-0x7d8: cursors on the chosen slot
static int           save_load_scr_anim = -1;   // sdata gp-0x7d0: yes/no window slot, -1 none
static int           save_load_scr_ask;         // sdata gp-0x7cc: 1 while the choices show
static BP_OBJ       *save_load_scr_obj;         // sdata gp-0x7a60: the SAVELOAD_PHF layout
static int           save_load_scr_alpha[SAVE_LOAD_SLOTS];  // bss 0x390c78: slot fades 0..0x80
static B2D_GRID_SPR *save_load_scr_arrow[4];    // bss 0x390c90: arrows on the chosen slot

static int SaveLoadScrFile(void)
{
    return GetSysWrk()->GetLanguage() + SAVELOAD_PHF;
}

static int SaveLoadScrHandle(void)
{
    return BpCtlGetHandleByFileNo(SaveLoadScrFile());
}

// McLoadGame steps that hide the slots (5, 12, 13 of the slot screen).
static int SaveLoadScrIsHideStep(void)
{
    return mc_load_wrk.mode == MC_LOAD_MODE_SLOTS
        && (mc_load_wrk.step == 5 || mc_load_wrk.step == 12 || mc_load_wrk.step == 13);
}

// The text item value the info hooks pass on (EE item +0x124).
static int SaveLoadScrDrawPri(void)
{
    return B2dGetDrawItem()->part[B2D_PART_TEXT].value;
}

// ──────────────────────────────────────────────────────────────────────
// Slot fades
// ──────────────────────────────────────────────────────────────────────

// FF3 0x00130950.
static void SaveLoadScrFadeReset(void)
{
    for (int i = 0; i < SAVE_LOAD_SLOTS; i++)
        save_load_scr_alpha[i] = 0;
}

// A slot fades in (0x10 a frame) while McLoadGame has read it, and is 0
// otherwise; all are 0 in the steps that hide the slots.
// FF3 0x00130980.
static void SaveLoadScrFadeUpdate(void)
{
    if (SaveLoadScrIsHideStep())
    {
        SaveLoadScrFadeReset();
        return;
    }
    for (int i = 0; i < SAVE_LOAD_SLOTS; i++)
    {
        if (mc_load_wrk.tex[i] == nullptr || mc_load_wrk.chap_tex[i] == nullptr
            || mc_load_wrk.snap_file[i] < 0 || mc_load_wrk.chap_file[i] < 0)
        {
            if (save_load_scr_alpha[i] != 0)
                save_load_scr_alpha[i] = 0;
        }
        else
        {
            int alpha = save_load_scr_alpha[i] + 0x10;
            if (alpha > 0x7f)
                alpha = 0x80;
            save_load_scr_alpha[i] = alpha;
        }
    }
}

// FF3 0x00130a90.
static int SaveLoadScrIsSlotShown(int slot)
{
    if (SaveLoadScrIsHideStep())
        return 0;
    return GetMemoryCardPlayDataFlg(slot) != 0
        && mc_load_wrk.snap_file[slot] >= 0 && mc_load_wrk.chap_file[slot] >= 0
        && mc_load_wrk.tex[slot] != nullptr && mc_load_wrk.chap_tex[slot] != nullptr;
}

// FF3 0x00130b60.
static int SaveLoadScrFadeAlpha(int slot, int alpha)
{
    return alpha * save_load_scr_alpha[slot] / 128;
}

// Shown and holding a save (flag 1).
static int SaveLoadScrIsSlotData(int slot)
{
    return SaveLoadScrIsSlotShown(slot) && mc_load_wrk.slot_flg[slot] == 1;
}

// ──────────────────────────────────────────────────────────────────────
// Item hooks
// ──────────────────────────────────────────────────────────────────────

// The save screen's title items are not shown on the load screen.
// FF3 0x00130b90.
// callcheck: BpCtlGetHandleByFileNo, GetLanguage, GetSysWrk (SaveLoadScrHandle)
static void SaveLoadScrHideSaveTitle(void)
{
    int handle = SaveLoadScrHandle();

    B2dSprSetDisp(B2dCtlFindItem(handle, SLR_ITEM_TITLE), 0);
    if (B2dCtlHasItemName(handle, "R ttl_save2") == 0)
        return;
    B2dSprSetDisp(B2dCtlFindItemByName(handle, "R ttl_save2"), 0);
}

// Items "mapsnap1".."mapsnap5": the slot's thumbnail (the slot is the
// name's last digit).
// FF3 0x00130c50.
// callcheck: B2dGetDrawItem, SaveLoadScrIsSlotShown (SaveLoadScrDrawPri, SaveLoadScrIsSlotData)
static int SaveLoadScrDrawThumb(const B2D_HOOK_ARGS *args)
{
    const char *name = (const char *)args->a0;
    const int  *xy   = (const int *)args->t1;
    int         slot = name[strlen("mapsnap1") - 1] - '1';

    if (SaveLoadScrIsSlotData(slot))
    {
        int pri = SaveLoadScrDrawPri();

        SetCurColorPacked((u_int)args->t3);
        SaveInfoDrawThumb(mc_load_wrk.tex[slot], xy[0], xy[1],
                          (u_char)SaveLoadScrFadeAlpha(slot, (int)args->stack[2]), pri);
        SetDefaultGsDrawState();
    }
    return 0;
}

// FF3 0x00130d28.
// callcheck: BpCtlGetHandleByFileNo, GetLanguage, GetSysWrk (SaveLoadScrHandle)
static void SaveLoadScrSetThumbHooks(void)
{
    int handle = SaveLoadScrHandle();

    for (int i = 0; i < SAVE_LOAD_SLOTS; i++)
    {
        B2D_ITEM_HOOK hook = { SaveLoadScrDrawThumb };
        B2dCtlSetItemHook(B2dCtlFindItem(handle, slr_thumb_item[i]), &hook);
    }
}

// Item "slot_M": memory card slot 1, centred.
// FF3 0x00130dc8.
// callcheck: B2dGetDrawItem (SaveLoadScrDrawPri)
static int SaveLoadScrDrawCardSlot(const B2D_HOOK_ARGS *args)
{
    const int *xy   = (const int *)args->t1;
    int        slot = mc_load_wrk.slot;
    int        pri  = SaveLoadScrDrawPri();

    SetCurColorPacked((u_int)args->t3);
    SaveInfoPrintCardSlot(0, (xy[0] + xy[6]) / 2, xy[1], (int)args->a1,
                          SaveLoadScrFadeAlpha(slot, (int)args->stack[2]), pri);
    SetDefaultGsDrawState();
    return 0;
}

// FF3 0x00130e70.
// callcheck: B2dGetDrawItem, SaveLoadScrIsSlotShown (SaveLoadScrDrawPri, SaveLoadScrIsSlotData)
static int SaveLoadScrDrawPlayTime(const B2D_HOOK_ARGS *args)
{
    const int *xy   = (const int *)args->t1;
    int        slot = mc_load_wrk.slot;

    if (SaveLoadScrIsSlotData(slot))
    {
        int       pri = SaveLoadScrDrawPri();
        PLAY_TIME time;

        SetCurColorPacked((u_int)args->t3);
        time = *GetMemoryCardPlayDataPlayTime(mc_load_wrk.slot);
        SaveInfoPrintPlayTime(&time, xy[0], xy[1], (int)args->a1,
                              SaveLoadScrFadeAlpha(slot, (int)args->stack[2]), pri);
        SetDefaultGsDrawState();
    }
    return 0;
}

// FF3 0x00130f60.
// callcheck: B2dGetDrawItem, SaveLoadScrIsSlotShown (SaveLoadScrDrawPri, SaveLoadScrIsSlotData)
static int SaveLoadScrDrawDifficulty(const B2D_HOOK_ARGS *args)
{
    const int *xy   = (const int *)args->t1;
    int        slot = mc_load_wrk.slot;

    if (SaveLoadScrIsSlotData(slot))
    {
        int pri = SaveLoadScrDrawPri();

        SetCurColorPacked((u_int)args->t3);
        SaveInfoPrintDifficulty(GetMemoryCardPlayDataDifficulty(mc_load_wrk.slot), xy[0], xy[1],
                                (int)args->a1, SaveLoadScrFadeAlpha(slot, (int)args->stack[2]), pri);
        SetDefaultGsDrawState();
    }
    return 0;
}

// Bad data (flag -1) prints message (0x69, 0x2b); a save its chapter.
// FF3 only restores the draw state after the chapter.
// FF3 0x00131030.
// callcheck: B2dGetDrawItem (SaveLoadScrDrawPri)
static int SaveLoadScrDrawChapter(const B2D_HOOK_ARGS *args)
{
    const int *xy   = (const int *)args->t1;
    int        slot = mc_load_wrk.slot;

    if (SaveLoadScrIsSlotShown(slot))
    {
        int pri = SaveLoadScrDrawPri();

        SetCurColorPacked((u_int)args->t3);
        if (mc_load_wrk.slot_flg[slot] == -1)
        {
            PrintMsg_ArrangeOneLine(GetMsgDataAddr(0x69, 0x2b), xy[0], xy[1], 0x2d,
                                    SaveLoadScrFadeAlpha(slot, (int)args->stack[2]), pri, 0, 0, 0);
        }
        if (mc_load_wrk.slot_flg[slot] == 1)
        {
            int flag = GetMemoryCardPlayDataFinderFlg(mc_load_wrk.slot);
            int no   = GetMemoryCardPlayDataChapter(mc_load_wrk.slot);

            SaveInfoPrintChapter(no, xy[0], xy[1], SaveLoadScrFadeAlpha(slot, (int)args->stack[2]),
                                 pri, flag);
            SetDefaultGsDrawState();
        }
    }
    return 0;
}

// FF3 0x00131180.
// callcheck: B2dGetDrawItem, SaveLoadScrIsSlotShown (SaveLoadScrDrawPri, SaveLoadScrIsSlotData)
static int SaveLoadScrDrawPlayer(const B2D_HOOK_ARGS *args)
{
    const int *xy   = (const int *)args->t1;
    int        slot = mc_load_wrk.slot;

    if (SaveLoadScrIsSlotData(slot))
    {
        int pri = SaveLoadScrDrawPri();

        SetCurColorPacked((u_int)args->t3);
        SaveInfoPrintPlayer(GetMemoryCardPlayDataPlayer(slot), xy[0], xy[1],
                            SaveLoadScrFadeAlpha(slot, (int)args->stack[2]), pri);
        SetDefaultGsDrawState();
    }
    return 0;
}

// Items 0x411 / 0x40c: the one named "NUM.." prints save byte +0x1c + 1,
// the other message (0x69, 9); nothing when the byte is negative.
// FF3 0x00131240.
// callcheck: B2dGetDrawItem, SaveLoadScrIsSlotShown (SaveLoadScrDrawPri, SaveLoadScrIsSlotData)
static int SaveLoadScrDrawInfo1C(const B2D_HOOK_ARGS *args)
{
    const char *name = (const char *)args->a0;
    const int  *xy   = (const int *)args->t1;
    int         slot = mc_load_wrk.slot;

    if (SaveLoadScrIsSlotData(slot))
    {
        int pri = SaveLoadScrDrawPri();
        int n;

        SetCurColorPacked((u_int)args->t3);
        n = GetMemoryCardPlayDataB04(slot);
        if (n >= 0)
        {
            if (strncmp(name, "NUM", 3) != 0)       // sdata 0x36ba98
                n = -1;
            SaveInfoPrintNumber(n, xy[0], xy[1], (int)args->a1,
                                SaveLoadScrFadeAlpha(slot, (int)args->stack[2]), pri);
        }
        SetDefaultGsDrawState();
    }
    return 0;
}

// Item 0x418 is never drawn.
// FF3 0x00131378 (no Ghidra function before 2026-10-14).
// callcheck: SaveLoadScrIsSlotShown (SaveLoadScrIsSlotData)
static int SaveLoadScrDrawNone(const B2D_HOOK_ARGS *args)
{
    int slot = mc_load_wrk.slot;

    (void)args;
    if (SaveLoadScrIsSlotData(slot))
        SetDefaultGsDrawState();
    return 0;
}

// FF3 0x001313d8.
// callcheck: B2dGetDrawItem, SaveLoadScrIsSlotShown (SaveLoadScrDrawPri, SaveLoadScrIsSlotData)
static int SaveLoadScrDrawInfo1D(const B2D_HOOK_ARGS *args)
{
    const int *xy   = (const int *)args->t1;
    int        slot = mc_load_wrk.slot;

    if (SaveLoadScrIsSlotData(slot))
    {
        int pri = SaveLoadScrDrawPri();
        int flag, n;

        SetCurColorPacked((u_int)args->t3);
        flag = GetMemoryCardPlayDataFinderFlg(mc_load_wrk.slot);
        n = GetMemoryCardPlayDataB02(slot);
        if (n > 0)
            SaveInfoPrintInfo1D(n, xy[0] + 0x10, xy[1], SaveLoadScrFadeAlpha(slot, (int)args->stack[2]),
                                pri, flag);
        SetDefaultGsDrawState();
    }
    return 0;
}

static const B2D_HOOK_REC save_load_scr_hooks[] =      // ROM 0x34ecd0
{
    { SaveLoadScrDrawCardSlot,   0, SLR_ITEM_CARD_SLOT,  nullptr },
    { SaveLoadScrDrawPlayTime,   0, SLR_ITEM_PLAY_TIME,  nullptr },
    { SaveLoadScrDrawDifficulty, 0, SLR_ITEM_DIFFICULTY, nullptr },
    { SaveLoadScrDrawChapter,    0, SLR_ITEM_CHAPTER,    nullptr },
    { SaveLoadScrDrawPlayer,     0, SLR_ITEM_PLAYER,     nullptr },
    { SaveLoadScrDrawInfo1C,     0, SLR_ITEM_INFO1C,     nullptr },
    { SaveLoadScrDrawInfo1C,     0, SLR_ITEM_INFO1C_NUM, nullptr },
    { SaveLoadScrDrawNone,       0, SLR_ITEM_NONE,       nullptr },
    { SaveLoadScrDrawInfo1D,     0, SLR_ITEM_INFO1D,     nullptr },
    { nullptr,                  -1, -1,                  nullptr },
};

// FF3 copies the ROM list to the stack first.
// FF3 0x001314b8.
// callcheck: BpCtlGetHandleByFileNo, GetLanguage, GetSysWrk (SaveLoadScrHandle)
static void SaveLoadScrSetHooks(void)
{
    B2dSetHookList(SaveLoadScrHandle(), save_load_scr_hooks);
}

// ──────────────────────────────────────────────────────────────────────
// Cursor, arrows, slot numbers
// ──────────────────────────────────────────────────────────────────────

// FF3 0x00131538.
static int SaveLoadScrIsSelectStep(void)
{
    return mc_load_wrk.mode == MC_LOAD_MODE_SLOTS && mc_load_wrk.step == MC_LOAD_STEP_SELECT;
}

// FF3 0x00131570.
// callcheck: BpCtlGetHandleByFileNo, GetLanguage, GetSysWrk (SaveLoadScrHandle)
static void SaveLoadScrShowCslFlea(int disp)
{
    B2dSprSetDisp(B2dCtlFindItem(SaveLoadScrHandle(), SLR_ITEM_CSL_FLEA), disp);
}

// FF3 0x001315d0.
static void SaveLoadScrCslUpdate(void)
{
    SaveLoadScrShowCslFlea(SaveLoadScrIsSelectStep() != 0);
    for (int i = 0; i < 2; i++)
        save_load_scr_csl[i]->SetPos(mc_load_wrk.slot, 0);
}

// FF3 0x00131648.
// callcheck: operator_new, GetLanguage, GetSysWrk, B2dGridSprCtor (new B2D_GRID_SPR, SaveLoadScrFile)
static void SaveLoadScrCslCreate(void)
{
    for (int i = 0; i < 2; i++)
    {
        B2D_GRID_SPR *spr = new B2D_GRID_SPR(SaveLoadScrFile(), slr_csl_item[i], SLR_SLOT_STEP, 0);
        save_load_scr_csl[i] = spr;
    }
    SaveLoadScrCslUpdate();
}

// FF3 0x001316e8.
// callcheck: operator_delete, B2dGridSprDtor (delete)
static void SaveLoadScrCslRelease(void)
{
    for (int i = 0; i < 2; i++)
    {
        B2D_GRID_SPR *spr = save_load_scr_csl[i];

        if (spr != nullptr)
        {
            delete spr;
        }
        save_load_scr_csl[i] = nullptr;
    }
}

// FF3 0x00131748.
// callcheck: BpCtlGetHandleByFileNo, GetLanguage, GetSysWrk (SaveLoadScrHandle)
static void SaveLoadScrShowArrowFlea(int disp)
{
    int handle = SaveLoadScrHandle();

    B2dSprSetDisp(B2dCtlFindItem(handle, SLR_ITEM_ARROW_FLEA), disp);
    B2dSprSetDisp(B2dCtlFindItem(handle, SLR_ITEM_ARROW_FLEA2), disp);
}

// FF3 0x001317e0.
// callcheck: BpCtlGetHandleByFileNo, GetLanguage, GetSysWrk (SaveLoadScrHandle)
static void SaveLoadScrShowArrow(int disp)
{
    int handle = SaveLoadScrHandle();

    B2dSprSetDisp(B2dCtlFindItem(handle, SLR_ITEM_ARROW), disp);
    B2dSprSetDisp(B2dCtlFindItem(handle, SLR_ITEM_ARROW2), disp);
}

// FF3 0x00131878.
static void SaveLoadScrArrowUpdate(void)
{
    int sel = SaveLoadScrIsSelectStep();

    SaveLoadScrShowArrowFlea(sel != 0 ? 1 : 0);
    SaveLoadScrShowArrow(sel != 0);
    for (int i = 0; i < 4; i++)
        save_load_scr_arrow[i]->SetPos(mc_load_wrk.slot, 0);
}

// FF3 0x00131908.
// callcheck: operator_new, GetLanguage, GetSysWrk, B2dGridSprCtor (new B2D_GRID_SPR, SaveLoadScrFile)
static void SaveLoadScrArrowCreate(void)
{
    for (int i = 0; i < 4; i++)
    {
        B2D_GRID_SPR *spr = new B2D_GRID_SPR(SaveLoadScrFile(), slr_arrow_item[i], SLR_SLOT_STEP, 0);
        save_load_scr_arrow[i] = spr;
    }
    SaveLoadScrArrowUpdate();
}

// FF3 0x001319b0.
// callcheck: operator_delete, B2dGridSprDtor (delete)
static void SaveLoadScrArrowRelease(void)
{
    for (int i = 0; i < 4; i++)
    {
        B2D_GRID_SPR *spr = save_load_scr_arrow[i];

        if (spr != nullptr)
        {
            delete spr;
        }
        save_load_scr_arrow[i] = nullptr;
    }
}

// The chosen slot shows its selected number, the others the plain one.
// FF3 0x00131a18.
// callcheck: BpCtlGetHandleByFileNo, GetLanguage, GetSysWrk (SaveLoadScrHandle)
static void SaveLoadScrSlotSelUpdate(void)
{
    int handle = SaveLoadScrHandle();

    for (int i = 0; i < SAVE_LOAD_SLOTS; i++)
    {
        B2D_ITEM *off = B2dCtlFindItem(handle, slr_slot_off_item[i]);
        B2D_ITEM *on  = B2dCtlFindItem(handle, slr_slot_on_item[i]);
        int       other = i != mc_load_wrk.slot;

        B2dSprSetDisp(off, other ? 1 : 0);
        B2dSprSetDisp(on, !other);
    }
}

// FF3 0x00131b08.
static void SaveLoadScrSlotSelInit(void)
{
    SaveLoadScrSlotSelUpdate();
}

// Item "R clear" (a sprite): drawn once per slot whose save has cleared
// the game, SLR_SLOT_STEP apart.
// FF3 0x00131b20 (no Ghidra function before 2026-10-14).
// callcheck: SaveLoadScrIsSlotShown (SaveLoadScrIsSlotData)
static int SaveLoadScrDrawClear(const B2D_HOOK_ARGS *args)
{
    intptr_t x = args->t1;

    for (int slot = 0; slot < SAVE_LOAD_SLOTS; slot++, x += SLR_SLOT_STEP)
    {
        if (SaveLoadScrIsSlotData(slot) && GetMemoryCardPlayDataClearNum(slot) > 0)
        {
            B2D_HOOK_ARGS a = *args;

            a.t1 = x;
            a.stack[1] = SaveLoadScrFadeAlpha(slot, (int)args->stack[1]);
            B2dDrawSpriteDirect(&a);
        }
    }
    SetDefaultGsDrawState();
    return 0;
}

// FF3 0x00131c40.
// callcheck: BpCtlGetHandleByFileNo, GetLanguage, GetSysWrk (SaveLoadScrHandle)
static void SaveLoadScrSetClearHook(void)
{
    B2D_ITEM_HOOK hook = { SaveLoadScrDrawClear };

    B2dCtlSetItemHook(B2dCtlFindItem(SaveLoadScrHandle(), SLR_ITEM_CLEAR), &hook);
}

// ──────────────────────────────────────────────────────────────────────
// The yes/no window
// ──────────────────────────────────────────────────────────────────────

// FF3 0x00131ca0.
static int SaveLoadScrIsAskStep(void)
{
    return mc_load_wrk.mode == MC_LOAD_MODE_SLOTS && mc_load_wrk.step == MC_LOAD_STEP_ASK;
}

// Keeps the window open (again once it has closed) with McLoadGame's
// message; the yes/no choices only in the ask step.
// FF3 0x00131cd8.
static void SaveLoadScrMsgUpdate(void)
{
    int sel;

    if (save_load_scr_anim != -1 && LayoutAnimGetState(save_load_scr_anim) == SLR_LA_CLOSED)
    {
        LayoutAnimClearFlag(save_load_scr_anim, LAYOUT_ANIM_LABEL_LAYOUT);
        LayoutAnimReset(save_load_scr_anim);
        save_load_scr_anim = -1;
    }
    if (save_load_scr_anim == -1)
    {
        save_load_scr_anim = LayoutAnimStart(SLR_ANIM_YESNO, 0);
        // PC: FF3 sets the flag and alpha before the -1 check (through a
        // null slot when none was free).
        if (save_load_scr_anim == -1)
            return;
        LayoutAnimSetFlag(save_load_scr_anim, LAYOUT_ANIM_LABEL_LAYOUT);
        LayoutAnimSetAlpha(save_load_scr_anim, 0);
    }

    if (SaveLoadScrIsAskStep() == 0)
    {
        if (save_load_scr_ask == 1)
            save_load_scr_ask = 0;
        YesNoSetMsg(SLR_MSG_KIND, mc_load_wrk.msg);
        sel = SLR_YESNO_NONE;
    }
    else
    {
        if (save_load_scr_ask == 0)
            save_load_scr_ask = 1;
        YesNoSetMsg(SLR_MSG_KIND, mc_load_wrk.msg);
        sel = mc_load_wrk.yesno;
    }
    YesNoSetSel(sel);
}

// FF3 0x00131dd8.
static void SaveLoadScrMsgInit(void)
{
    save_load_scr_anim = -1;
    save_load_scr_ask = SaveLoadScrIsAskStep() != 0;
    SaveLoadScrMsgUpdate();
}

// FF3 0x00131e10.
static int SaveLoadScrMsgIsReady(void)
{
    return LayoutAnimIsDataReady(save_load_scr_anim) != 0;
}

// FF3 0x00131e38.
static void SaveLoadScrMsgReset(void)
{
    if (save_load_scr_anim != -1)
    {
        LayoutAnimClearFlag(save_load_scr_anim, LAYOUT_ANIM_LABEL_LAYOUT);
        LayoutAnimReset(save_load_scr_anim);
        save_load_scr_anim = -1;
    }
}

// FF3 0x00131e78.
int SaveLoadScrMouseBox(float *x, float *y, float *w, float *h)
{
    ReiPan_MenuBox box;

    if (!ReiPan_MenuMouseItemBox(BpCtlGetHandleByFileNo(SaveLoadScrFile()), SLR_ITEM_CSL, &box))
        return 0;
    *x = box.x;
    *y = box.y;
    *w = box.w;
    *h = box.h;
    return 1;
}

float SaveLoadScrMouseStep(void)
{
    return (float)SLR_SLOT_STEP;
}

int SaveLoadScrMsgIsOpen(void)
{
    if (save_load_scr_anim != -1 && LayoutAnimGetState(save_load_scr_anim) != SLR_LA_OPEN)
        return 0;
    return 1;
}

// ──────────────────────────────────────────────────────────────────────
// Phase 68's interface
// ──────────────────────────────────────────────────────────────────────

// FF3 0x00131eb8.
// callcheck: operator_new, GetLanguage, GetSysWrk, BpObjCtor (new BP_OBJ, SaveLoadScrFile)
void SaveLoadScrCreate(int phase)
{
    BP_OBJ *obj;

    save_load_scr_phase = phase;
    obj = new BP_OBJ(SaveLoadScrFile());
    save_load_scr_obj = obj;
    SaveLoadScrMsgInit();
}

// Layout and window loaded: both fade item 0x403, the fade layer shows.
// FF3 0x00131f18.
int SaveLoadScrIsLoaded(void)
{
    if (save_load_scr_obj->Load() == 0)
        return 0;
    if (SaveLoadScrMsgIsReady() == 0)
        return 0;
    LayoutAnimSetAlphaItemById(save_load_scr_anim, save_load_scr_obj->GetHandle(), SLR_FADE_ITEM);
    GetCmnFadeLayer()->SetItem(save_load_scr_obj->GetHandle(), SLR_FADE_ITEM);
    GetCmnFadeLayer()->Show(0);
    return 1;
}

// FF3 0x00131fb0.
void SaveLoadScrIn(void)
{
    save_load_scr_obj->In(slr_label_in);
    SaveLoadScrHideSaveTitle();
    SaveLoadScrSetThumbHooks();
    SaveLoadScrSetHooks();
    SaveLoadScrCslCreate();
    SaveLoadScrArrowCreate();
    SaveLoadScrSlotSelInit();
    SaveLoadScrSetClearHook();
    SaveLoadScrFadeReset();
}

// FF3 0x00132010.
int SaveLoadScrIsInEnd(void)
{
    return save_load_scr_obj->IsInEnd() != 0;
}

// FF3 0x00132038.
void SaveLoadScrMain(void)
{
    SaveLoadScrCslUpdate();
    SaveLoadScrArrowUpdate();
    SaveLoadScrSlotSelUpdate();
    SaveLoadScrMsgUpdate();
    SaveLoadScrFadeUpdate();
}

// FF3 0x00132070.
int SaveLoadScrOutWait(void)
{
    return save_load_scr_obj->Out(slr_label_out) != 0;
}

// FF3 0x00132098.
// callcheck: operator_delete (operator delete)
void SaveLoadScrRelease(void)
{
    BP_OBJ *obj;

    GetCmnFadeLayer()->SetItemHandle(nullptr);
    GetCmnFadeLayer()->Term();
    SaveLoadScrCslRelease();
    SaveLoadScrArrowRelease();
    SaveLoadScrMsgReset();
    obj = save_load_scr_obj;
    if (obj != nullptr)
    {
        obj->Release();
        operator delete(obj);
    }
    save_load_scr_obj = nullptr;
}
