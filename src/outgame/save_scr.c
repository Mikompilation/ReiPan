/* ==========================================================================
 *  outgame/save_scr.c
 *
 *  The memory-card save screen (see save_scr.h). FF3 0x132118..0x133960,
 *  FF3's own code: outgame/save_load_scr.c repeated over McSaveGame's work.
 *  Item ids, the ROM item tables (0x34f058..) and the hook list layout are
 *  the load screen's; what differs is commented where it happens.
 * ======================================================================== */
#include "outgame/save_scr.h"

#include <new>                          // operator new / delete
#include <string.h>                     // strlen, strncmp

#include "common/cmn_fade_layer.h"      // GetCmnFadeLayer, CmnFadeLayer*
#include "graphics/graph2d/message.h"   // GetMsgDataAddr, PrintMsg_ArrangeOneLine
#include "graphics/motion/anicode.h"    // B2D_HOOK_REC, B2dSetHookList, YesNoSetMsg, YesNoSetSel, SetCurColorPacked, SetDefaultGsDrawState
#include "ingame/menu/anim_2d.h"        // LayoutAnim*
#include "outgame/save_info_disp.h"     // SaveInfo*
#include "save_load/prg/game_data_save.h"  // mc_save_wrk
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

#define SSC_ANIM_YESNO      0xb         // layout anim type of the yes/no window
#define SSC_MSG_KIND        0x83        // message kind of McSaveGame's messages
#define SSC_YESNO_NONE      2           // YesNoSetSel: no choices shown
#define SSC_LA_OPEN         2           // LAYOUT_ANIM.state
#define SSC_LA_CLOSED       4
#define SSC_SLOT_STEP       0x6e        // x distance between two slots
#define SSC_FADE_ITEM       0x403       // item the window and the fade layer fade
#define SSC_FADE_SPEED      0x10        // slot fade per frame

// Layout items of SAVELOAD_PHF (the load screen's ids, ROM 0x34f058 pairs).
enum
{
    SSC_ITEM_TITLE       = 0x3ec,       // the load screen's title, hidden here
    SSC_ITEM_ARROW       = 0x3ef,       // "R sankaku"
    SSC_ITEM_ARROW_FLEA  = 0x3ee,       // "R sankaku_flea"
    SSC_ITEM_ARROW2      = 0x400,       // "R sankaku"
    SSC_ITEM_ARROW_FLEA2 = 0x3ff,       // "R sankaku_flea"
    SSC_ITEM_CSL         = 0x3f1,       // "R csl"
    SSC_ITEM_CSL_FLEA    = 0x3fc,       // "R csl_flea"
    SSC_ITEM_CLEAR       = 0x3f0,       // "R clear"
    SSC_ITEM_CARD_SLOT   = 0x40b,       // "slot_M"
    SSC_ITEM_PLAY_TIME   = 0x40d,
    SSC_ITEM_DIFFICULTY  = 0x413,
    SSC_ITEM_CHAPTER     = 0x40e,
    SSC_ITEM_PLAYER      = 0x412,
    SSC_ITEM_INFO1C      = 0x411,
    SSC_ITEM_INFO1C_NUM  = 0x40c,
    SSC_ITEM_NONE        = 0x418,
    SSC_ITEM_INFO1D      = 0x414,
};

// ROM 0x34f058..: the five thumbnails "mapsnap1".."mapsnap5", the arrows
// (0x34f080), the cursors (0x34f0a0), then the unselected / selected slot
// numbers "R No1_hisentaku".. / "R No1_sentaku".. -- the tables the load
// screen reads.
static const int ssc_thumb_item[MC_SAVE_SLOTS]    = { 0x405, 0x406, 0x407, 0x408, 0x409 };
static const int ssc_slot_off_item[MC_SAVE_SLOTS] = { 0x3f2, 0x3f3, 0x3f4, 0x3f5, 0x3f6 };
static const int ssc_slot_on_item[MC_SAVE_SLOTS]  = { 0x3f7, 0x3f8, 0x3f9, 0x3fa, 0x3fb };
static const int ssc_csl_item[2]   = { SSC_ITEM_CSL, SSC_ITEM_CSL_FLEA };
static const int ssc_arrow_item[4] = { SSC_ITEM_ARROW, SSC_ITEM_ARROW2, SSC_ITEM_ARROW_FLEA, SSC_ITEM_ARROW_FLEA2 };

static const char ssc_label_in[]  = "in";       // sdata 0x36bac0
static const char ssc_label_out[] = "out";      // sdata 0x36bac8

static int           save_scr_phase;            // sdata gp-0x7c4: SaveScrCreate's phase (no reader)
static B2D_GRID_SPR *save_scr_csl[2];           // sdata gp-0x7c0: cursors on the chosen slot
static int           save_scr_anim = -1;        // sdata gp-0x7b8: yes/no window slot, -1 none
static int           save_scr_ask;              // sdata gp-0x7b4: 1 while the choices show
static BP_OBJ       *save_scr_obj;              // sdata gp-0x7a40: the SAVELOAD_PHF layout
static int           save_scr_alpha[MC_SAVE_SLOTS];  // bss 0x390ca0: slot fades 0..0x80
static B2D_GRID_SPR *save_scr_arrow[4];         // bss 0x390cb8: arrows on the chosen slot

static int SaveScrFile(void)
{
    return GetSysWrk()->GetLanguage() + SAVELOAD_PHF;
}

static int SaveScrHandle(void)
{
    return BpCtlGetHandleByFileNo(SaveScrFile());
}

// The text item value the info hooks pass on (EE item +0x124).
static int SaveScrDrawPri(void)
{
    return B2dGetDrawItem()->part[B2D_PART_TEXT].value;
}

// ──────────────────────────────────────────────────────────────────────
// Slot fades
// ──────────────────────────────────────────────────────────────────────

// FF3 0x00132118.
static void SaveScrFadeReset(void)
{
    for (int i = 0; i < MC_SAVE_SLOTS; i++)
        save_scr_alpha[i] = 0;
}

// A slot fades in (0x10 a frame) while McSaveGame has read it, and is 0
// otherwise. Unlike the load screen the fades hold in the steps that hide
// the slots (5, 14, 15, 17), and in step 9 the chosen slot fades out
// before it is written (McSaveGame waits on SaveScrIsSlotFadedOut).
// FF3 0x00132148.
static void SaveScrFadeUpdate(void)
{
    if (mc_save_wrk.mode == MC_SAVE_MODE_SLOTS)
    {
        switch (mc_save_wrk.step)
        {
        case MGS_STEP_SNAP_LOAD_WAIT:
        case MGS_STEP_ERROR_INIT:
        case MGS_STEP_ERROR_WAIT:
        case MGS_STEP_RELOAD_WAIT:
            return;
        case MC_SAVE_STEP_FADE:
        {
            int alpha = save_scr_alpha[mc_save_wrk.slot] - SSC_FADE_SPEED;

            if (alpha <= 0)
                alpha = 0;
            save_scr_alpha[mc_save_wrk.slot] = alpha;
            return;
        }
        default:
            break;
        }
    }
    for (int i = 0; i < MC_SAVE_SLOTS; i++)
    {
        if (mc_save_wrk.tex[i] == nullptr || mc_save_wrk.chap_tex[i] == nullptr
            || mc_save_wrk.snap_file[i] < 0 || mc_save_wrk.chap_file[i] < 0)
        {
            if (save_scr_alpha[i] != 0)
                save_scr_alpha[i] = 0;
        }
        else
        {
            int alpha = save_scr_alpha[i] + SSC_FADE_SPEED;
            if (alpha > 0x7f)
                alpha = 0x80;
            save_scr_alpha[i] = alpha;
        }
    }
}

// Steps 5, 14 and 15 hide every slot, step 17 the chosen one.
// FF3 0x00132270.
static int SaveScrIsSlotShown(int slot)
{
    if (mc_save_wrk.mode == MC_SAVE_MODE_SLOTS)
    {
        int step = mc_save_wrk.step;

        if (step == MGS_STEP_SNAP_LOAD_WAIT || step == MGS_STEP_ERROR_INIT
            || step == MGS_STEP_ERROR_WAIT)
            return 0;
        if (step == MGS_STEP_RELOAD_WAIT && slot == mc_save_wrk.slot)
            return 0;
    }
    return GetMemoryCardPlayDataFlg(slot) != 0
        && mc_save_wrk.snap_file[slot] >= 0 && mc_save_wrk.chap_file[slot] >= 0
        && mc_save_wrk.tex[slot] != nullptr && mc_save_wrk.chap_tex[slot] != nullptr;
}

// FF3 0x00132358.
static int SaveScrFadeAlpha(int slot, int alpha)
{
    return alpha * save_scr_alpha[slot] / 128;
}

// FF3 0x00132388.
int SaveScrIsSlotFadedOut(void)
{
    return save_scr_alpha[mc_save_wrk.slot] == 0;
}

// Shown and holding a save (flag 1).
static int SaveScrIsSlotData(int slot)
{
    return SaveScrIsSlotShown(slot) && mc_save_wrk.slot_flg[slot] == 1;
}

// ──────────────────────────────────────────────────────────────────────
// Item hooks
// ──────────────────────────────────────────────────────────────────────

// The load screen's title items are not shown on the save screen.
// FF3 0x001323b0.
// callcheck: BpCtlGetHandleByFileNo, GetLanguage, GetSysWrk (SaveScrHandle)
static void SaveScrHideLoadTitle(void)
{
    int handle = SaveScrHandle();

    B2dSprSetDisp(B2dCtlFindItem(handle, SSC_ITEM_TITLE), 0);
    if (B2dCtlHasItemName(handle, "R ttl_load2") == 0)
        return;
    B2dSprSetDisp(B2dCtlFindItemByName(handle, "R ttl_load2"), 0);
}

// Items "mapsnap1".."mapsnap5": the slot's thumbnail (the slot is the
// name's last digit).
// FF3 0x00132470.
// callcheck: B2dGetDrawItem, SaveScrIsSlotShown (SaveScrDrawPri, SaveScrIsSlotData)
static int SaveScrDrawThumb(const B2D_HOOK_ARGS *args)
{
    const char *name = (const char *)args->a0;
    const int  *xy   = (const int *)args->t1;
    int         slot = name[strlen("mapsnap1") - 1] - '1';

    if (SaveScrIsSlotData(slot))
    {
        int pri = SaveScrDrawPri();

        SetCurColorPacked((u_int)args->t3);
        SaveInfoDrawThumb(mc_save_wrk.tex[slot], xy[0], xy[1],
                          (u_char)SaveScrFadeAlpha(slot, (int)args->stack[2]), pri);
        SetDefaultGsDrawState();
    }
    return 0;
}

// FF3 0x00132548.
// callcheck: BpCtlGetHandleByFileNo, GetLanguage, GetSysWrk (SaveScrHandle)
static void SaveScrSetThumbHooks(void)
{
    int handle = SaveScrHandle();

    for (int i = 0; i < MC_SAVE_SLOTS; i++)
    {
        B2D_ITEM_HOOK hook = { SaveScrDrawThumb };
        B2dCtlSetItemHook(B2dCtlFindItem(handle, ssc_thumb_item[i]), &hook);
    }
}

// Item "slot_M": memory card slot 1, centred. Unlike the load screen's
// twin the alpha is not faded.
// FF3 0x001325e8.
// callcheck: B2dGetDrawItem (SaveScrDrawPri)
static int SaveScrDrawCardSlot(const B2D_HOOK_ARGS *args)
{
    const int *xy  = (const int *)args->t1;
    int        pri = SaveScrDrawPri();

    SetCurColorPacked((u_int)args->t3);
    SaveInfoPrintCardSlot(0, (xy[0] + xy[6]) / 2, xy[1], (int)args->a1, (int)args->stack[2], pri);
    SetDefaultGsDrawState();
    return 0;
}

// FF3 0x00132678.
// callcheck: B2dGetDrawItem, SaveScrIsSlotShown (SaveScrDrawPri, SaveScrIsSlotData)
static int SaveScrDrawPlayTime(const B2D_HOOK_ARGS *args)
{
    const int *xy   = (const int *)args->t1;
    int        slot = mc_save_wrk.slot;

    if (SaveScrIsSlotData(slot))
    {
        int       pri = SaveScrDrawPri();
        PLAY_TIME time;

        SetCurColorPacked((u_int)args->t3);
        time = *GetMemoryCardPlayDataPlayTime(mc_save_wrk.slot);
        SaveInfoPrintPlayTime(&time, xy[0], xy[1], (int)args->a1,
                              SaveScrFadeAlpha(slot, (int)args->stack[2]), pri);
        SetDefaultGsDrawState();
    }
    return 0;
}

// FF3 0x00132768.
// callcheck: B2dGetDrawItem, SaveScrIsSlotShown (SaveScrDrawPri, SaveScrIsSlotData)
static int SaveScrDrawDifficulty(const B2D_HOOK_ARGS *args)
{
    const int *xy   = (const int *)args->t1;
    int        slot = mc_save_wrk.slot;

    if (SaveScrIsSlotData(slot))
    {
        int pri = SaveScrDrawPri();

        SetCurColorPacked((u_int)args->t3);
        SaveInfoPrintDifficulty(GetMemoryCardPlayDataDifficulty(mc_save_wrk.slot), xy[0], xy[1],
                                (int)args->a1, SaveScrFadeAlpha(slot, (int)args->stack[2]), pri);
        SetDefaultGsDrawState();
    }
    return 0;
}

// Bad data (flag -1) prints message (0x69, 0x2b); a save its chapter.
// FF3 only restores the draw state after the chapter.
// FF3 0x00132838.
// callcheck: B2dGetDrawItem (SaveScrDrawPri)
static int SaveScrDrawChapter(const B2D_HOOK_ARGS *args)
{
    const int *xy   = (const int *)args->t1;
    int        slot = mc_save_wrk.slot;

    if (SaveScrIsSlotShown(slot))
    {
        int pri = SaveScrDrawPri();

        SetCurColorPacked((u_int)args->t3);
        if (mc_save_wrk.slot_flg[slot] == -1)
        {
            PrintMsg_ArrangeOneLine(GetMsgDataAddr(0x69, 0x2b), xy[0], xy[1], 0x2d,
                                    SaveScrFadeAlpha(slot, (int)args->stack[2]), pri, 0, 0, 0);
        }
        if (mc_save_wrk.slot_flg[slot] == 1)
        {
            int flag = GetMemoryCardPlayDataFinderFlg(mc_save_wrk.slot);
            int no   = GetMemoryCardPlayDataChapter(mc_save_wrk.slot);

            SaveInfoPrintChapter(no, xy[0], xy[1], SaveScrFadeAlpha(slot, (int)args->stack[2]),
                                 pri, flag);
            SetDefaultGsDrawState();
        }
    }
    return 0;
}

// FF3 0x00132988.
// callcheck: B2dGetDrawItem, SaveScrIsSlotShown (SaveScrDrawPri, SaveScrIsSlotData)
static int SaveScrDrawPlayer(const B2D_HOOK_ARGS *args)
{
    const int *xy   = (const int *)args->t1;
    int        slot = mc_save_wrk.slot;

    if (SaveScrIsSlotData(slot))
    {
        int pri = SaveScrDrawPri();

        SetCurColorPacked((u_int)args->t3);
        SaveInfoPrintPlayer(GetMemoryCardPlayDataPlayer(slot), xy[0], xy[1],
                            SaveScrFadeAlpha(slot, (int)args->stack[2]), pri);
        SetDefaultGsDrawState();
    }
    return 0;
}

// Items 0x411 / 0x40c: the one named "NUM.." prints save byte +0x1c + 1,
// the other message (0x69, 9); nothing when the byte is negative.
// FF3 0x00132a48.
// callcheck: B2dGetDrawItem, SaveScrIsSlotShown (SaveScrDrawPri, SaveScrIsSlotData)
static int SaveScrDrawInfo1C(const B2D_HOOK_ARGS *args)
{
    const char *name = (const char *)args->a0;
    const int  *xy   = (const int *)args->t1;
    int         slot = mc_save_wrk.slot;

    if (SaveScrIsSlotData(slot))
    {
        int pri = SaveScrDrawPri();
        int n;

        SetCurColorPacked((u_int)args->t3);
        n = GetMemoryCardPlayDataB04(slot);
        if (n >= 0)
        {
            if (strncmp(name, "NUM", 3) != 0)       // sdata 0x36bab8
                n = -1;
            SaveInfoPrintNumber(n, xy[0], xy[1], (int)args->a1,
                                SaveScrFadeAlpha(slot, (int)args->stack[2]), pri);
        }
        SetDefaultGsDrawState();
    }
    return 0;
}

// Item 0x418 is never drawn.
// FF3 0x00132b80 (no Ghidra function before 2026-10-14).
// callcheck: SaveScrIsSlotShown (SaveScrIsSlotData)
static int SaveScrDrawNone(const B2D_HOOK_ARGS *args)
{
    int slot = mc_save_wrk.slot;

    (void)args;
    if (SaveScrIsSlotData(slot))
        SetDefaultGsDrawState();
    return 0;
}

// FF3 0x00132be0.
// callcheck: B2dGetDrawItem, SaveScrIsSlotShown (SaveScrDrawPri, SaveScrIsSlotData)
static int SaveScrDrawInfo1D(const B2D_HOOK_ARGS *args)
{
    const int *xy   = (const int *)args->t1;
    int        slot = mc_save_wrk.slot;

    if (SaveScrIsSlotData(slot))
    {
        int pri = SaveScrDrawPri();
        int flag, n;

        SetCurColorPacked((u_int)args->t3);
        flag = GetMemoryCardPlayDataFinderFlg(mc_save_wrk.slot);
        n = GetMemoryCardPlayDataB02(slot);
        if (n > 0)
            SaveInfoPrintInfo1D(n, xy[0] + 0x10, xy[1], SaveScrFadeAlpha(slot, (int)args->stack[2]),
                                pri, flag);
        SetDefaultGsDrawState();
    }
    return 0;
}

static const B2D_HOOK_REC save_scr_hooks[] =    // ROM 0x34eda8
{
    { SaveScrDrawCardSlot,   0, SSC_ITEM_CARD_SLOT,  nullptr },
    { SaveScrDrawPlayTime,   0, SSC_ITEM_PLAY_TIME,  nullptr },
    { SaveScrDrawDifficulty, 0, SSC_ITEM_DIFFICULTY, nullptr },
    { SaveScrDrawChapter,    0, SSC_ITEM_CHAPTER,    nullptr },
    { SaveScrDrawPlayer,     0, SSC_ITEM_PLAYER,     nullptr },
    { SaveScrDrawInfo1C,     0, SSC_ITEM_INFO1C,     nullptr },
    { SaveScrDrawInfo1C,     0, SSC_ITEM_INFO1C_NUM, nullptr },
    { SaveScrDrawNone,       0, SSC_ITEM_NONE,       nullptr },
    { SaveScrDrawInfo1D,     0, SSC_ITEM_INFO1D,     nullptr },
    { nullptr,              -1, -1,                  nullptr },
};

// FF3 copies the ROM list to the stack first.
// FF3 0x00132cc0.
// callcheck: BpCtlGetHandleByFileNo, GetLanguage, GetSysWrk (SaveScrHandle)
static void SaveScrSetHooks(void)
{
    B2dSetHookList(SaveScrHandle(), save_scr_hooks);
}

// ──────────────────────────────────────────────────────────────────────
// Cursor, arrows, slot numbers
// ──────────────────────────────────────────────────────────────────────

// FF3 0x00132d40.
static int SaveScrIsSelectStep(void)
{
    return mc_save_wrk.mode == MC_SAVE_MODE_SLOTS && mc_save_wrk.step == MC_SAVE_STEP_SELECT;
}

// FF3 0x00132d78.
// callcheck: BpCtlGetHandleByFileNo, GetLanguage, GetSysWrk (SaveScrHandle)
static void SaveScrShowCslFlea(int disp)
{
    B2dSprSetDisp(B2dCtlFindItem(SaveScrHandle(), SSC_ITEM_CSL_FLEA), disp);
}

// FF3 0x00132dd8.
static void SaveScrCslUpdate(void)
{
    SaveScrShowCslFlea(SaveScrIsSelectStep() != 0);
    for (int i = 0; i < 2; i++)
        save_scr_csl[i]->SetPos(mc_save_wrk.slot, 0);
}

// FF3 0x00132e50.
// callcheck: operator_new, GetLanguage, GetSysWrk, B2dGridSprCtor (new B2D_GRID_SPR, SaveScrFile)
static void SaveScrCslCreate(void)
{
    for (int i = 0; i < 2; i++)
    {
        B2D_GRID_SPR *spr = new B2D_GRID_SPR(SaveScrFile(), ssc_csl_item[i], SSC_SLOT_STEP, 0);
        save_scr_csl[i] = spr;
    }
    SaveScrCslUpdate();
}

// FF3 0x00132ef0.
// callcheck: operator_delete, B2dGridSprDtor (delete)
static void SaveScrCslRelease(void)
{
    for (int i = 0; i < 2; i++)
    {
        B2D_GRID_SPR *spr = save_scr_csl[i];

        if (spr != nullptr)
        {
            delete spr;
        }
        save_scr_csl[i] = nullptr;
    }
}

// FF3 0x00132f50.
// callcheck: BpCtlGetHandleByFileNo, GetLanguage, GetSysWrk (SaveScrHandle)
static void SaveScrShowArrowFlea(int disp)
{
    int handle = SaveScrHandle();

    B2dSprSetDisp(B2dCtlFindItem(handle, SSC_ITEM_ARROW_FLEA), disp);
    B2dSprSetDisp(B2dCtlFindItem(handle, SSC_ITEM_ARROW_FLEA2), disp);
}

// FF3 0x00132fe8.
// callcheck: BpCtlGetHandleByFileNo, GetLanguage, GetSysWrk (SaveScrHandle)
static void SaveScrShowArrow(int disp)
{
    int handle = SaveScrHandle();

    B2dSprSetDisp(B2dCtlFindItem(handle, SSC_ITEM_ARROW), disp);
    B2dSprSetDisp(B2dCtlFindItem(handle, SSC_ITEM_ARROW2), disp);
}

// The arrows show in the select step only when the cursor can move (two
// or more selectable slots: the saves and one empty slot).
// FF3 0x00133080.
static void SaveScrArrowUpdate(void)
{
    int disp = SaveScrIsSelectStep() != 0 && mc_save_wrk.slot_num >= 2;

    SaveScrShowArrowFlea(disp);
    SaveScrShowArrow(disp);
    for (int i = 0; i < 4; i++)
        save_scr_arrow[i]->SetPos(mc_save_wrk.slot, 0);
}

// FF3 0x00133120.
// callcheck: operator_new, GetLanguage, GetSysWrk, B2dGridSprCtor (new B2D_GRID_SPR, SaveScrFile)
static void SaveScrArrowCreate(void)
{
    for (int i = 0; i < 4; i++)
    {
        B2D_GRID_SPR *spr = new B2D_GRID_SPR(SaveScrFile(), ssc_arrow_item[i], SSC_SLOT_STEP, 0);
        save_scr_arrow[i] = spr;
    }
    SaveScrArrowUpdate();
}

// FF3 0x001331c8.
// callcheck: operator_delete, B2dGridSprDtor (delete)
static void SaveScrArrowRelease(void)
{
    for (int i = 0; i < 4; i++)
    {
        B2D_GRID_SPR *spr = save_scr_arrow[i];

        if (spr != nullptr)
        {
            delete spr;
        }
        save_scr_arrow[i] = nullptr;
    }
}

// The chosen slot shows its selected number, the others the plain one.
// FF3 0x00133230.
// callcheck: BpCtlGetHandleByFileNo, GetLanguage, GetSysWrk (SaveScrHandle)
static void SaveScrSlotSelUpdate(void)
{
    int handle = SaveScrHandle();

    for (int i = 0; i < MC_SAVE_SLOTS; i++)
    {
        B2D_ITEM *off   = B2dCtlFindItem(handle, ssc_slot_off_item[i]);
        B2D_ITEM *on    = B2dCtlFindItem(handle, ssc_slot_on_item[i]);
        int       other = i != mc_save_wrk.slot;

        B2dSprSetDisp(off, other ? 1 : 0);
        B2dSprSetDisp(on, !other);
    }
}

// FF3 0x00133320.
static void SaveScrSlotSelInit(void)
{
    SaveScrSlotSelUpdate();
}

// Item "R clear" (a sprite): drawn once per slot whose save has cleared
// the game, SSC_SLOT_STEP apart.
// FF3 0x00133338 (no Ghidra function before 2026-10-14).
// callcheck: SaveScrIsSlotShown (SaveScrIsSlotData)
static int SaveScrDrawClear(const B2D_HOOK_ARGS *args)
{
    intptr_t x = args->t1;

    for (int slot = 0; slot < MC_SAVE_SLOTS; slot++, x += SSC_SLOT_STEP)
    {
        if (SaveScrIsSlotData(slot) && GetMemoryCardPlayDataClearNum(slot) > 0)
        {
            B2D_HOOK_ARGS a = *args;

            a.t1 = x;
            a.stack[1] = SaveScrFadeAlpha(slot, (int)args->stack[1]);
            B2dDrawSpriteDirect(&a);
        }
    }
    SetDefaultGsDrawState();
    return 0;
}

// FF3 0x00133460.
// callcheck: BpCtlGetHandleByFileNo, GetLanguage, GetSysWrk (SaveScrHandle)
static void SaveScrSetClearHook(void)
{
    B2D_ITEM_HOOK hook = { SaveScrDrawClear };

    B2dCtlSetItemHook(B2dCtlFindItem(SaveScrHandle(), SSC_ITEM_CLEAR), &hook);
}

// ──────────────────────────────────────────────────────────────────────
// The yes/no window
// ──────────────────────────────────────────────────────────────────────

// McSaveGame's questions (overwrite / new file / format ..): steps 7, 19,
// 20, 23, 24, 29, 30 of the slot screen (jump table 0x34ee48).
// FF3 0x001334c0.
static int SaveScrIsAskStep(void)
{
    if (mc_save_wrk.mode != MC_SAVE_MODE_SLOTS)
        return 0;
    switch (mc_save_wrk.step)
    {
    case MGS_STEP_ASK:
    case MGS_STEP_REMAKE_CONF_INIT:
    case MGS_STEP_REMAKE_CONF_WAIT:
    case MGS_STEP_NEW_CONF_INIT:
    case MGS_STEP_NEW_CONF_WAIT:
    case MGS_STEP_FORMAT_CONF_INIT:
    case MGS_STEP_FORMAT_CONF_WAIT:
        return 1;
    default:
        return 0;
    }
}

// Keeps the window open (again once it has closed) with McSaveGame's
// message; the yes/no choices only in the ask steps.
// FF3 0x00133518.
static void SaveScrMsgUpdate(void)
{
    int sel;

    if (save_scr_anim != -1 && LayoutAnimGetState(save_scr_anim) == SSC_LA_CLOSED)
    {
        LayoutAnimClearFlag(save_scr_anim, LAYOUT_ANIM_LABEL_LAYOUT);
        LayoutAnimReset(save_scr_anim);
        save_scr_anim = -1;
    }
    if (save_scr_anim == -1)
    {
        save_scr_anim = LayoutAnimStart(SSC_ANIM_YESNO, 0);
        // PC: FF3 sets the flag and alpha before the -1 check (through a
        // null slot when none was free).
        if (save_scr_anim == -1)
            return;
        LayoutAnimSetFlag(save_scr_anim, LAYOUT_ANIM_LABEL_LAYOUT);
        LayoutAnimSetAlpha(save_scr_anim, 0);
    }

    if (SaveScrIsAskStep() == 0)
    {
        if (save_scr_ask == 1)
            save_scr_ask = 0;
        YesNoSetMsg(SSC_MSG_KIND, mc_save_wrk.msg);
        sel = SSC_YESNO_NONE;
    }
    else
    {
        if (save_scr_ask == 0)
            save_scr_ask = 1;
        YesNoSetMsg(SSC_MSG_KIND, mc_save_wrk.msg);
        sel = mc_save_wrk.yesno;
    }
    YesNoSetSel(sel);
}

// FF3 0x00133618.
static void SaveScrMsgInit(void)
{
    save_scr_anim = -1;
    save_scr_ask = SaveScrIsAskStep() != 0;
    SaveScrMsgUpdate();
}

// FF3 0x00133650.
static int SaveScrMsgIsReady(void)
{
    return LayoutAnimIsDataReady(save_scr_anim) != 0;
}

// FF3 0x00133678.
static void SaveScrMsgReset(void)
{
    if (save_scr_anim != -1)
    {
        LayoutAnimClearFlag(save_scr_anim, LAYOUT_ANIM_LABEL_LAYOUT);
        LayoutAnimReset(save_scr_anim);
        save_scr_anim = -1;
    }
}

// FF3 0x001336b8.
int SaveScrMsgIsOpen(void)
{
    if (save_scr_anim != -1 && LayoutAnimGetState(save_scr_anim) != SSC_LA_OPEN)
        return 0;
    return 1;
}

// ──────────────────────────────────────────────────────────────────────
// Phase 69's interface
// ──────────────────────────────────────────────────────────────────────

// FF3 0x001336f8.
// callcheck: operator_new, GetLanguage, GetSysWrk, BpObjCtor (new BP_OBJ, SaveScrFile)
void SaveScrCreate(int phase)
{
    BP_OBJ *obj;

    save_scr_phase = phase;
    obj = new BP_OBJ(SaveScrFile());
    save_scr_obj = obj;
    SaveScrMsgInit();
}

// Layout and window loaded: both fade item 0x403, the fade layer shows.
// FF3 0x00133758.
int SaveScrIsLoaded(void)
{
    if (save_scr_obj->Load() == 0)
        return 0;
    if (SaveScrMsgIsReady() == 0)
        return 0;
    LayoutAnimSetAlphaItemById(save_scr_anim, save_scr_obj->GetHandle(), SSC_FADE_ITEM);
    GetCmnFadeLayer()->SetItem(save_scr_obj->GetHandle(), SSC_FADE_ITEM);
    GetCmnFadeLayer()->Show(0);
    return 1;
}

// FF3 0x001337f0.
void SaveScrIn(void)
{
    save_scr_obj->In(ssc_label_in);
    SaveScrHideLoadTitle();
    SaveScrSetThumbHooks();
    SaveScrSetHooks();
    SaveScrCslCreate();
    SaveScrArrowCreate();
    SaveScrSlotSelInit();
    SaveScrSetClearHook();
    SaveScrFadeReset();
}

// FF3 0x00133850.
int SaveScrIsInEnd(void)
{
    return save_scr_obj->IsInEnd() != 0;
}

// FF3 0x00133878.
void SaveScrMain(void)
{
    SaveScrCslUpdate();
    SaveScrArrowUpdate();
    SaveScrSlotSelUpdate();
    SaveScrMsgUpdate();
    SaveScrFadeUpdate();
}

// FF3 0x001338b0.
void SaveScrOut(void)
{
}

// FF3 0x001338b8.
int SaveScrOutWait(void)
{
    return save_scr_obj->Out(ssc_label_out) != 0;
}

// FF3 0x001338e0.
// callcheck: operator_delete (operator delete)
void SaveScrRelease(void)
{
    BP_OBJ *obj;

    GetCmnFadeLayer()->SetItemHandle(nullptr);
    GetCmnFadeLayer()->Term();
    SaveScrCslRelease();
    SaveScrArrowRelease();
    SaveScrMsgReset();
    obj = save_scr_obj;
    if (obj != nullptr)
    {
        obj->Release();
        operator delete(obj);
    }
    save_scr_obj = nullptr;
}

// PC: the mouse.
int SaveScrMouseBox(float *x, float *y, float *w, float *h)
{
    ReiPan_MenuBox box;

    if (!ReiPan_MenuMouseItemBox(SaveScrHandle(), SSC_ITEM_CSL, &box))
        return 0;
    *x = box.x;
    *y = box.y;
    *w = box.w;
    *h = box.h;
    return 1;
}

float SaveScrMouseStep(void)
{
    return (float)SSC_SLOT_STEP;
}
