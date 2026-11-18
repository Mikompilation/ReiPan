/* ==========================================================================
 *  ingame/story_menu_ghost_list.c
 *
 *  The ghost list, phases 63 StoryMenu_GhostList and 88
 *  StoryMenu_GhostListBonus; see story_menu_ghost_list.h.
 *  FF3 0x1d84f8..0x1da418.
 *
 *  The list is menu cursor 0x3f (1 x SoulListGetNum(), a 9-row view) over
 *  the soul list. Everything on screen is the G_LIST_PHF layout drawn by
 *  item hooks: the entry numbers (item 0x432) and names (0x427, which also
 *  draws the picture), the "new" marks (0x461), the photographed / total
 *  counts (0x43b / 0x43c, R_num_00 digits), and for the cursor's ghost --
 *  once it is photographed and read -- its picture (a MENU_PIC cross-fading
 *  GLIST_JG_000_TM2 + entry), title (0x440) between two ornaments (0x409 /
 *  0x3ee) and description page (0x43f); the detail panel slides in and out
 *  with the labels G_list_IN / G_list_OUT as the cursor moves. OK on a
 *  photographed entry not read yet reads it. A scroll bar follows the view.
 *  While a reached bonus is not handed out yet (SoulListGetNewBonus != 4)
 *  phase 63 goes to its child 88: the BGM stops, the "success" jingle
 *  plays, the bonus's group of items and message (0x447) show until OK,
 *  then the bonus is handed out and the menu BGM comes back.
 *
 *  Not written: FF3 0x001da3f8 (GhostListAtexitGPhaseCtrl), the GPhaseCtrl
 *  singleton's atexit thunk (as in every file that includes the getter).
 * ======================================================================== */
#include "ingame/story_menu_ghost_list.h"

#include "common/cmn_fade_layer.h"      // GetCmnFadeLayer, CmnFadeLayerIsReady
#include "graphics/graph2d/message.h"   // GetMsgDataAddr, MsgGetLineWidth, MsgPrintPage, PrintNumber, SetCurColorRGB
#include "graphics/motion/anicode.h"    // B2D_HOOK_REC, B2dSetHookList, GetLabelSuffixType, SetCurColorPacked, SetDefaultGsDrawState
#include "ingame/ingame_wrk.h"          // ingame_wrk (the world)
#include "ingame/item/prg/soul_list.h"  // SoulList*
#include "ingame/menu/menu_csr.h"       // MenuCsr*
#include "ingame/menu/menu_draw_tex.h"  // MENU_PIC, MENU_DRAW_TEX_BLANK
#include "ingame/sound/snd_ctrl.h"      // SndSePlay, SndBgm*
#include "main/gphase.h"                // GetGPhaseCtrl
#include "outgame/option.h"             // LayoutItemAddPos, PrintMsgPos, CmnFadeLayer*Item
#include "system/b2d/B2dCtl.h"          // B2D_HOOK_ARGS, B2D_ITEM_HOOK, B2dCtlFindItem, B2dCtlSetItemHook, B2dResetCurTex
#include "system/b2d/B2dSpr.h"          // B2dSprGetPartData, B2dSprSetPos, B2dSprSetDisp, B2dSprFreezePart
#include "system/b2d/BpCtl.h"           // BpCtl*, BpPlayLabel, B2dDrawSpriteDirect, B2dNum*
#include "system/b2d/ScrollBar.h"       // B2d_ScrollBar3
#include "system/eeiop/cd_file_dat.h"   // G_LIST_PHF, GLIST_JG_000_TM2, BGM_*
#include "system/os/system.h"           // GetSysWrk, SYS_WRK::GetLanguage, GetPALMode
#include "system/pad/pad_ctrl.h"        // pad_key, CheckPadTrgPlaySe, CheckPadRptPlaySe
#include "ingame/menu/menu_mouse.h"    // PC: the mouse in menus

#define GHOST_LIST_PHASE        0x3f    // phase 63; also the list's menu cursor id
#define GHOST_LIST_BONUS_PHASE  0x58    // phase 88
#define GHOST_LIST_TOP_PHASE    0x39    // phase 57 StoryMenu_Top
#define GHOST_LIST_VIEW_ROWS    9       // rows shown at once
#define GHOST_LIST_ROW_DY       0x20    // pixels between rows
#define GHOST_LIST_BONUS_NONE   4       // SoulListGetNewBonus: no bonus to hand out

#define GHOST_MSG               0x54    // message type of the ghosts (SoulListMsgNo)
#define GHOST_MSG_MENU          0x60    // the menu's own messages: 0..3 the bonuses, 4 "not photographed"
#define GHOST_MSG_UNKNOWN       4

#define GHOST_ITEM_PIC          0x44c   // the picture's place
#define GHOST_ITEM_FADE         0x3ef   // the fade layer's alpha item

#define GHOST_SE_MOVE           0x1a9b0000
#define GHOST_SE_CANCEL         0x1a9b0001
#define GHOST_SE_BUZZER         0x1a9b0002
#define GHOST_SE_OK             0x1a9b0003
#define GHOST_SE_PAGE           0x1a9b0005

static B2d_ScrollBar3 *ghost_list_bar;  // sbss 0x373340 (gp-0x1b0)
static MENU_PIC       *ghost_list_pic;  // sbss 0x373344 (gp-0x1ac)
static int             ghost_list_in;   // sbss 0x373348 (gp-0x1a8): the detail panel is in (G_list_IN played)

static void GhostListCsrSetPos(void);
static void GhostListBonusDisp(void);

// The layout (G_LIST_PHF + language), as every use computes it.
static int GhostListFile(void)
{
    return GetSysWrk()->GetLanguage() + G_LIST_PHF;
}

static int GhostListHandle(void)
{
    return BpCtlGetHandleByFileNo(GhostListFile());
}

// The frames of a second (the BGM fades, the jingle's wait).
static int GhostListFps(void)
{
    return GetPALMode() != 0 ? 25 : 30;
}

// ──────────────────────────────────────────────────────────────────────
// The picture
// ──────────────────────────────────────────────────────────────────────

// The ghost whose picture the MENU_PIC's slot cur (next: the incoming slot
// cur ^ 1) holds, -1 none.
// FF3 0x001d84f8 (was the FF2 match GameResultPicGetNo)

static int GhostListPicGetNo(int next)
{
    int slot, file;

    if (ghost_list_pic == nullptr)
        return -1;
    slot = ghost_list_pic->cur;
    if (next != 0)
        slot ^= 1;
    file = ghost_list_pic->load[slot]->file;
    if (file < 0)
        return -1;
    return file - GLIST_JG_000_TM2;
}

// That slot's alpha in the cross-fade, times the picture's scale (the
// caller's alpha / 128); 0 when the slot holds no file.
// FF3 0x001d8540

static int GhostListPicGetAlpha(int next)
{
    int   file;
    u_int fade;

    if (ghost_list_pic == nullptr)
        return 0;
    if (next == 0) {
        file = ghost_list_pic->load[ghost_list_pic->cur]->file;
        fade = ghost_list_pic->GetFade();
    } else {
        file = ghost_list_pic->load[ghost_list_pic->cur ^ 1]->file;
        fade = (u_char)(ghost_list_pic->fade_max - ghost_list_pic->GetFade());
    }
    if (file < 0)
        return 0;
    return (int)((float)(int)fade * ghost_list_pic->scale);
}

// x[0] / x[1]: the left / right end of a title str centred at x 0x1da.
// FF3 0x001d85e8 (was the FF2 match GameResultDisp)

static void GhostListTitleGetEnds(float *x, u_char *str)
{
    int half = (short)(MsgGetLineWidth(str, nullptr) + 0x1e) / 2;

    x[0] = (float)(0x1d8 - half);
    x[1] = (float)(half + 0x1dd);
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x001d8650 (was the FF2 match init_GameResult)
// callcheck: GetSysWrk, GetLanguage (GhostListFile)
// callcheck: operator_new, B2dScrollBar3Ctor (new B2d_ScrollBar3)

static void GhostListBarCreate(void)
{
    ghost_list_bar = new B2d_ScrollBar3(GhostListFile(), 0x3f1, 0x3fa, 0x3fc, 0x3fb);
    ghost_list_bar->SetRange((short)SoulListGetNum(), GHOST_LIST_VIEW_ROWS);
}

// FF3 0x001d86d8. PC: the pointer is cleared (the ROM leaves it dangling
// until the next init_StoryMenu_GhostList clears it).
static void GhostListBarDelete(void)
{
    if (ghost_list_bar != nullptr)
        delete ghost_list_bar;
    ghost_list_bar = nullptr;
}

// FF3 0x001d8708. PC: the pointer is cleared, as GhostListBarDelete.
// callcheck: MenuPicDtor, operator_delete (delete)
static void GhostListPicDelete(void)
{
    if (ghost_list_pic != nullptr)
        delete ghost_list_pic;
    ghost_list_pic = nullptr;
}

// The picture, 0xc0 x 0x80 at item 0x44c's place less (4, 5).
// FF3 0x001d8750 (was the FF2 match after_GameResult)
// callcheck: GetSysWrk, GetLanguage, BpCtlGetHandleByFileNo (GhostListHandle)
// callcheck: operator_new, MenuPicCtor (new)

static void GhostListPicCreate(void)
{
    B2D_ITEM *item = B2dCtlFindItem(GhostListHandle(), GHOST_ITEM_PIC);
    int       x    = B2dSprGetPartData(item, B2D_PART_X);
    int       y    = B2dSprGetPartData(item, B2D_PART_Y);
    MENU_PIC *pic  = new MENU_PIC(0);

    ghost_list_pic = pic;
    pic->v      = 0;
    pic->u      = 0;
    pic->w      = 0xc0;
    pic->h      = 0x80;
    pic->pri    = 0xa0;
    pic->x      = (short)(x - 4);
    pic->y      = (short)(y - 5);
    pic->z      = 0;
    pic->enable = 1;
}

// ──────────────────────────────────────────────────────────────────────
// The cursor sprites (items 0x40d / 0x40e, sdata 0x370af0) on the cursor's
// view row, their position parts frozen.
// FF3 0x001d8840 (was the FF2 match GameResultTopClearTime)
// callcheck: GetSysWrk, GetLanguage, BpCtlGetHandleByFileNo (GhostListHandle)

static void GhostListCsrSetPos(void)
{
    static const short items[2] = { 0x40d, 0x40e };    /* sdata 0x370af0 */
    int handle = GhostListHandle();
    int row    = MenuCsrGetPosY(GHOST_LIST_PHASE);
    int i;

    for (i = 0; i < 2; i++) {
        B2dSprSetPos(B2dCtlFindItem(handle, items[i]), 0, row << 5);
        B2dSprFreezePart(handle, items[i], B2D_PART_X, 0);
        B2dSprFreezePart(handle, items[i], B2D_PART_Y, 0);
    }
}

// ──────────────────────────────────────────────────────────────────────
// Phase 88: the four bonuses' groups of items hidden, the new bonus's
// shown (-1: no item).
// FF3 0x001d8930
// callcheck: GetSysWrk, GetLanguage, BpCtlGetHandleByFileNo (GhostListHandle)

static void GhostListBonusDisp(void)
{
    static const short items[4][3] =                    /* built on the stack */
    {
        { 0x443, 0x442, 0x444 },
        { 0x443, 0x441, 0x444 },
        { 0x443, 0x43e, 0x444 },
        { 0x445, 0x446, -1    },
    };
    int handle = GhostListHandle();
    int bonus  = SoulListGetNewBonus();
    int i, j;

    for (i = 0; i < 4; i++) {
        for (j = 0; j < 3; j++) {
            if (items[i][j] >= 0)
                B2dSprSetDisp(B2dCtlFindItem(handle, items[i][j]), 0);
        }
    }
    // PC: the ROM reads past its 12-entry stack table for bonus 4 (none);
    // phase 88 only runs while a bonus is new, so it never does.
    if (bonus < 0 || bonus >= 4)
        return;
    for (j = 0; j < 3; j++) {
        if (items[bonus][j] >= 0)
            B2dSprSetDisp(B2dCtlFindItem(handle, items[bonus][j]), 1);
    }
}

// ──────────────────────────────────────────────────────────────────────
// The item hooks of the layout
// ──────────────────────────────────────────────────────────────────────

// Sprite hook of item 0x3ee: the ornament at the right end of the shown
// ghost's title, for both picture slots with their alphas.
// FF3 0x001d8a78

static int GhostListTitleRightHook(const B2D_HOOK_ARGS *args)
{
    B2D_HOOK_ARGS a = *args;
    float         x[2];
    int           slot, alpha, no;

    for (slot = 0; slot < 2; slot++) {
        alpha = GhostListPicGetAlpha(slot);
        no    = GhostListPicGetNo(slot);
        if (no == -1)
            continue;
        GhostListTitleGetEnds(x, GetMsgDataAddr(GHOST_MSG, no * 3 + 1));   // SoulListMsgNo(1, no), inlined
        a.t1       = (int)x[1];
        a.stack[1] = alpha;
        B2dDrawSpriteDirect(&a);
    }
    return 0;
}

// Sprite hook of item 0x409: the same at the title's left end.
// FF3 0x001d8b88 (was the FF2 match GameResultTopScoreTime)

static int GhostListTitleLeftHook(const B2D_HOOK_ARGS *args)
{
    B2D_HOOK_ARGS a = *args;
    float         x[2];
    int           slot, alpha, no;

    for (slot = 0; slot < 2; slot++) {
        alpha = GhostListPicGetAlpha(slot);
        no    = GhostListPicGetNo(slot);
        if (no == -1)
            continue;
        GhostListTitleGetEnds(x, GetMsgDataAddr(GHOST_MSG, no * 3 + 1));
        a.t1       = (int)x[0];
        a.stack[1] = alpha;
        B2dDrawSpriteDirect(&a);
    }
    return 0;
}

// Sprite hook of item 0x461: the mark on every row shown whose ghost is
// photographed but not read yet.
// FF3 0x001d8c98 (was the FF2 match GameOverLoadMcSnapLoadWait)

static int GhostListNewMarkHook(const B2D_HOOK_ARGS *args)
{
    B2D_HOOK_ARGS a   = *args;
    int           top = MenuCsrGetTopY(GHOST_LIST_PHASE);
    int           i;

    for (i = 0; i < GHOST_LIST_VIEW_ROWS; i++, a.t2 += GHOST_LIST_ROW_DY) {
        if (SoulListIsHave(top + i) != 0 && SoulListIsRead(top + i) == 0)
            B2dDrawSpriteDirect(&a);
    }
    return 0;
}

// The cursor ghost's picture (blank until it is photographed and read),
// stepped and drawn with the hook's alpha; from GhostListNameHook with its
// arguments.
// FF3 0x001d8da0

static int GhostListPicDraw(const B2D_HOOK_ARGS *args)
{
    short pos = MenuCsrGetPos(GHOST_LIST_PHASE);
    float scale;

    if (SoulListIsHave(pos) != 0 && SoulListIsRead(pos) != 0)
        ghost_list_pic->pic = pos + GLIST_JG_000_TM2;
    else
        ghost_list_pic->pic = MENU_DRAW_TEX_BLANK;
    scale = (float)(int)args->stack[2] * (1.0f / 128.0f);          /* sdata 0x370af4 */
    ghost_list_pic->Step();
    ghost_list_pic->scale = scale;
    ghost_list_pic->Draw();
    ghost_list_pic->DrawNext();
    SetDefaultGsDrawState();
    return 0;
}

// value in the layout's number font (glyphs prefix'0'..'9'), right-aligned
// at pos; B2dNumDrawFont with a float position and no GS reset.
// FF3 0x001d9730 (was the FF2 match GameOverMenuTopTitleDisp)

static void GhostListNumDraw(int value, int handle, const char *prefix, const float *pos, u_char alpha)
{
    B2dNumSetFont(handle, prefix, nullptr);
    B2dNumSetTex();
    B2dNumDraw(value, (int)pos[0], (int)pos[1], alpha, 0);
    B2dResetCurTex();
}

// Text hook of item 0x43b: how many ghosts are photographed, at the item's
// corner 2.
// FF3 0x001d8e50
// callcheck: GetSysWrk, GetLanguage, BpCtlGetHandleByFileNo (GhostListHandle)

static int GhostListHaveNumHook(const B2D_HOOK_ARGS *args)
{
    const int *xy = (const int *)args->t1;
    float      pos[2];
    int        handle;

    pos[0] = (float)xy[4];
    pos[1] = (float)xy[5];
    handle = GhostListHandle();
    GhostListNumDraw(SoulListGetHaveNum(), handle, "R_num_00", pos, (u_char)args->stack[2]);   /* rdata 0x35a278 */
    return 0;
}

// Text hook of item 0x43c: how many there are (33, 217 after a clear),
// 17 px left with two digits.
// FF3 0x001d8ed8
// callcheck: GetSysWrk, GetLanguage, BpCtlGetHandleByFileNo (GhostListHandle)

static int GhostListTotalNumHook(const B2D_HOOK_ARGS *args)
{
    const int *xy = (const int *)args->t1;
    float      pos[2];
    int        num;

    pos[0] = (float)xy[4];
    pos[1] = (float)xy[5];
    num    = SoulListGetNum();
    if (num < 100)
        pos[0] -= 17.0f;                                            /* sdata 0x370af8 */
    GhostListNumDraw(num, GhostListHandle(), "R_num_00", pos, (u_char)args->stack[2]);
    return 0;
}

// A row's text colour: the cursor's row light, a photographed ghost dark
// (the entries from 0xc2 on brown), one not photographed grey.
static void GhostListSetRowColor(int row, int cursor_row, int no)
{
    if (row == cursor_row)
        SetCurColorRGB(0xf1, 0xd6, 0x9b);
    else if (SoulListIsHave(no) == 0)
        SetCurColorRGB(0xa4, 0xae, 0xc1);
    else if (no < 0xc2)
        SetCurColorRGB(0x19, 0x1b, 0x1f);
    else
        SetCurColorRGB(0x37, 0x30, 0x04);
}

// Text hook of item 0x432: the entry numbers (1..) of the rows shown,
// right-aligned (7 px a digit).
// FF3 0x001d8f88
// callcheck: GetSysWrk, GetLanguage (GhostListFile)
// callcheck: SetCurColorRGB, SoulListIsHave (GhostListSetRowColor)

static int GhostListNoHook(const B2D_HOOK_ARGS *args)
{
    const char *label = (const char *)args->a0;
    int        *rect  = (int *)args->t1;
    int         top   = MenuCsrGetTopY(GHOST_LIST_PHASE);
    int         row   = MenuCsrGetPosY(GHOST_LIST_PHASE);
    float       pos[2];
    int         i, no, n;

    for (i = 0; i < GHOST_LIST_VIEW_ROWS; i++) {
        no     = top + i;
        pos[0] = 0.0f;
        pos[1] = (float)(i * GHOST_LIST_ROW_DY);
        GhostListSetRowColor(i, row, no);
        LayoutItemAddPos(pos, label, GhostListFile(), rect);
        for (n = no + 1; n != 0; n /= 10)
            pos[0] -= 7.0f;                                         /* sdata 0x370afc */
        PrintNumber(no + 1, (int)pos[0], (int)pos[1], 0x2d, (u_char)args->stack[2], 0, 1);
    }
    SetDefaultGsDrawState();
    return 0;
}

// Text hook of item 0x427: the names of the rows shown ("not
// photographed" for the others), then the cursor ghost's picture.
// FF3 0x001d9128
// callcheck: GetSysWrk, GetLanguage (GhostListFile)
// callcheck: SetCurColorRGB, SoulListIsHave (GhostListSetRowColor)

static int GhostListNameHook(const B2D_HOOK_ARGS *args)
{
    const char *label = (const char *)args->a0;
    int        *rect  = (int *)args->t1;
    int         top   = MenuCsrGetTopY(GHOST_LIST_PHASE);
    int         row   = MenuCsrGetPosY(GHOST_LIST_PHASE);
    float       pos[2];
    int         i, no, arrange;

    for (i = 0; i < GHOST_LIST_VIEW_ROWS; i++) {
        no     = top + i;
        pos[0] = 0.0f;
        pos[1] = (float)(i * GHOST_LIST_ROW_DY);
        GhostListSetRowColor(i, row, no);
        arrange = GetLabelSuffixType(label);
        LayoutItemAddPos(pos, label, GhostListFile(), rect);
        if (SoulListIsHave(no) == 0)
            PrintMsgPos(GHOST_MSG_MENU, GHOST_MSG_UNKNOWN, pos, 0x2d, (int)args->stack[2], 0, arrange);
        else
            PrintMsgPos(GHOST_MSG, SoulListMsgNo(0, no), pos, 0x2d, (int)args->stack[2], 0, arrange);
    }
    GhostListPicDraw(args);
    SetDefaultGsDrawState();
    return 0;
}

// Text hook of item 0x440: the shown ghost's title, both picture slots.
// FF3 0x001d9320
// callcheck: GetSysWrk, GetLanguage (GhostListFile)

static int GhostListTitleHook(const B2D_HOOK_ARGS *args)
{
    const char *label = (const char *)args->a0;
    float       pos[2] = { 0.0f, 0.0f };
    int         arrange, slot, no;

    SetCurColorPacked((u_int)args->t3);
    arrange = GetLabelSuffixType(label);
    LayoutItemAddPos(pos, label, GhostListFile(), (int *)args->t1);
    for (slot = 0; slot < 2; slot++) {
        no = GhostListPicGetNo(slot);
        if (no != -1)
            PrintMsgPos(GHOST_MSG, SoulListMsgNo(1, no), pos, 0x2d, GhostListPicGetAlpha(slot), 0, arrange);
    }
    SetDefaultGsDrawState();
    return 0;
}

// Text hook of item 0x43f: the shown ghost's description, both slots.
// FF3 0x001d9420 (was the FF2 match GameOverMenuTopDisp)
// callcheck: GetSysWrk, GetLanguage (GhostListFile)

static int GhostListDescHook(const B2D_HOOK_ARGS *args)
{
    const char *label = (const char *)args->a0;
    float       pos[2] = { 0.0f, 0.0f };
    int         arrange, slot, no;

    SetCurColorPacked((u_int)args->t3);
    arrange = GetLabelSuffixType(label);
    LayoutItemAddPos(pos, label, GhostListFile(), (int *)args->t1);
    for (slot = 0; slot < 2; slot++) {
        no = GhostListPicGetNo(slot);
        if (no != -1)
            MsgPrintPage(GHOST_MSG, SoulListMsgNo(2, no), (int)pos[0], (int)pos[1], 0x2d,
                         GhostListPicGetAlpha(slot), 0, 0, 0, arrange);
    }
    SetDefaultGsDrawState();
    return 0;
}

// Text hook of item 0x447 (phase 88): the new bonus's message.
// FF3 0x001d9540
// callcheck: GetSysWrk, GetLanguage (GhostListFile)

static int GhostListBonusMsgHook(const B2D_HOOK_ARGS *args)
{
    const char *label = (const char *)args->a0;
    int         alpha = (int)args->stack[2];
    float       pos[2] = { 0.0f, 0.0f };
    int         arrange, bonus;

    if (alpha <= 0)
        return 0;
    SetCurColorPacked((u_int)args->t3);
    bonus = SoulListGetNewBonus();
    if (bonus != GHOST_LIST_BONUS_NONE) {
        arrange = GetLabelSuffixType(label);
        LayoutItemAddPos(pos, label, GhostListFile(), (int *)args->t1);
        MsgPrintPage(GHOST_MSG_MENU, bonus, (int)pos[0], (int)pos[1], 0x2d, alpha, 0, 0, 0, arrange);
    }
    SetDefaultGsDrawState();
    return 0;
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x001d9638 (the list, rdata 0x35a288, is copied onto the stack)
// callcheck: GetSysWrk, GetLanguage, BpCtlGetHandleByFileNo (GhostListHandle)

static void GhostListSetHooks(void)
{
    static const B2D_HOOK_REC hooks[8] =
    {
        { GhostListHaveNumHook,  0,  0x43b, nullptr },
        { GhostListTotalNumHook, 0,  0x43c, nullptr },
        { GhostListNoHook,       0,  0x432, nullptr },
        { GhostListNameHook,     0,  0x427, nullptr },
        { GhostListTitleHook,    0,  0x440, nullptr },
        { GhostListDescHook,     0,  0x43f, nullptr },
        { GhostListBonusMsgHook, 0,  0x447, nullptr },
        { nullptr,               -1, -1,    nullptr },
    };
    int           handle = GhostListHandle();
    B2D_ITEM_HOOK hook;

    hook.func = GhostListTitleRightHook;
    B2dCtlSetItemHook(B2dCtlFindItem(handle, 0x3ee), &hook);
    hook.func = GhostListTitleLeftHook;
    B2dCtlSetItemHook(B2dCtlFindItem(handle, 0x409), &hook);
    hook.func = GhostListNewMarkHook;
    B2dCtlSetItemHook(B2dCtlFindItem(handle, 0x461), &hook);
    B2dSetHookList(handle, hooks);
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x001d97a8

int SoulListMsgNo(int field, int no)
{
    return no * 3 + field;
}

// OK pressed on entry no: 1 when it is photographed and not read yet.
// FF3 0x001d97b8

static int GhostListCheckOk(int no)
{
    if (CheckPadTrgPlaySe(pad_key[PAD_KEY_OK], -1, 0) == 0)
        return 0;
    if (SoulListIsHave(no) == 0) {
        SndSePlay(GHOST_SE_BUZZER, 100, 0x1000, 0, nullptr, 0);
        return 0;
    }
    SndSePlay(GHOST_SE_OK, 100, 0x1000, 0, nullptr, 0);
    return SoulListIsRead(no) == 0;
}

// The detail panel follows the cursor: in for a photographed and read
// entry, out for any other.
// FF3 0x001d9b20
// callcheck: GetSysWrk, GetLanguage, BpCtlGetHandleByFileNo (GhostListHandle)

static void GhostListLabelUpdate(int no)
{
    if (SoulListIsHave(no) != 0 && SoulListIsRead(no) != 0) {
        if (ghost_list_in == 0)
            BpPlayLabel(GhostListHandle(), "G_list_IN");            /* rodata 0x35a308 */
        ghost_list_in = 1;
        return;
    }
    if (ghost_list_in != 0)
        BpPlayLabel(GhostListHandle(), "G_list_OUT");               /* rodata 0x35a318 */
    ghost_list_in = 0;
}

// ──────────────────────────────────────────────────────────────────────
// Phase 63 StoryMenu_GhostList
// ──────────────────────────────────────────────────────────────────────

// FF3 0x001d9860. Step 0: the cursor and the layout's load; step 1, once
// it is loaded: hooks, "IN", scroll bar, picture, the fade layer on item
// 0x3ef; step 2 waits for "IN" to end (the panel comes in when entry 0 is
// read). Every frame with the layout loaded puts the cursor sprites.
// callcheck: GetSysWrk, GetLanguage, BpCtlGetHandleByFileNo (GhostListFile / GhostListHandle)

int init_StoryMenu_GhostList(void)
{
    GPHASE_NODE *leaf = GetGPhaseCtrl()->leaf;
    int          ret  = 0;
    int          handle;

    if (leaf->state_cnt == 0) {
        MenuCsrInit(GHOST_LIST_PHASE, 1, (short)SoulListGetNum(), 0);
        MenuCsrSetViewX(GHOST_LIST_PHASE, 1, 0);
        MenuCsrSetViewY(GHOST_LIST_PHASE, GHOST_LIST_VIEW_ROWS, 0);
        BpCtlLoadReq(GhostListFile(), nullptr);
        ghost_list_bar = nullptr;
        GetGPhaseCtrl()->leaf->state_cnt = 1;
    } else if (leaf->state_cnt == 1) {
        if (BpCtlIsDataReady(GhostListHandle()) != 0) {
            handle = GhostListHandle();
            GhostListSetHooks();
            BpPlayLabel(GhostListHandle(), "IN");                   /* sdata 0x370b00 */
            GhostListBarCreate();
            GhostListPicCreate();
            CmnFadeLayerShowItem(9, handle, GHOST_ITEM_FADE);
            ghost_list_in = 0;
            GetGPhaseCtrl()->leaf->state_cnt = 2;
        }
    } else if (BpCtlIsLoadEnd(GhostListFile()) != 0) {
        ret = 1;
        if (SoulListIsHave(0) != 0 && SoulListIsRead(0) != 0) {
            BpPlayLabel(GhostListHandle(), "G_list_IN");
            ghost_list_in = 1;
        }
    }
    if (BpCtlIsDataReady(GhostListHandle()) != 0)
        GhostListCsrSetPos();
    return ret;
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x001d9bf0. A new bonus first opens phase 88; then the cursor (its
// move SE), OK reads a photographed entry (the panel comes in), cancel
// back to the story menu's top, the page keys page the list.
// callcheck: GetSysWrk, GetLanguage, BpCtlGetHandleByFileNo (GhostListHandle)

int one_StoryMenu_GhostList(void)
{
    short pos;

    if (SoulListGetNewBonus() != GHOST_LIST_BONUS_NONE) {
        GetGPhaseCtrl()->SetNext(GHOST_LIST_BONUS_PHASE, 0);
        return 1;
    }
    {
        // PC: the mouse. The two bar sprites sit on the cursor's row, 32 apart.
        ReiPan_MenuBox bar;

        if (ReiPan_MenuMouseItemBox(GhostListHandle(), 0x40d, &bar))
            MenuMouseCsrFrom(MenuCsrGetWork(GHOST_LIST_PHASE), &bar, 0.0f, 32.0f);
    }
    MenuCsrUpdate(GHOST_LIST_PHASE, 0);
    if (MenuCsrIsMovedSe(GHOST_LIST_PHASE, GHOST_SE_MOVE) == 0) {
        if (GhostListCheckOk(MenuCsrGetPos(GHOST_LIST_PHASE)) != 0) {
            SoulListSetRead(MenuCsrGetPos(GHOST_LIST_PHASE));
            BpPlayLabel(GhostListHandle(), "G_list_IN");
            ghost_list_in = 1;
            GhostListCsrSetPos();
            return 0;
        }
        if (CheckPadTrgPlaySe(pad_key[PAD_KEY_CANCEL], GHOST_SE_CANCEL, 0) != 0) {
            GetGPhaseCtrl()->SetNext(GHOST_LIST_TOP_PHASE, 0);
            return 1;
        }
        if (CheckPadRptPlaySe(pad_key[PAD_KEY_PAGE_PREV], GHOST_SE_PAGE, 0) != 0) {
            MenuCsrPagePrevY(GHOST_LIST_PHASE);
        } else if (CheckPadRptPlaySe(pad_key[PAD_KEY_PAGE_NEXT], GHOST_SE_PAGE, 0) != 0) {
            MenuCsrPageNextY(GHOST_LIST_PHASE);
        } else {
            GhostListCsrSetPos();
            return 0;
        }
    }
    pos = MenuCsrGetPos(GHOST_LIST_PHASE);
    GhostListLabelUpdate(pos);
    GhostListCsrSetPos();
    return 0;
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x001d9d88: "OUT", then once it has played everything is released.
// callcheck: GetSysWrk, GetLanguage, BpCtlGetHandleByFileNo (GhostListFile / GhostListHandle)

int end_StoryMenu_GhostList(void)
{
    if (GetGPhaseCtrl()->leaf->state_cnt == 0) {
        BpPlayLabel(GhostListHandle(), "OUT");                      /* sdata 0x370b08 */
        GetGPhaseCtrl()->leaf->state_cnt = 1;
    }
    GhostListCsrSetPos();
    if (BpCtlIsLoadEnd(GhostListFile()) == 0)
        return 0;
    MenuCsrRelease(GHOST_LIST_PHASE);
    BpCtlRelease(GhostListHandle());
    CmnFadeLayerTermItem();
    GhostListBarDelete();
    GhostListPicDelete();
    return 1;
}

// FF3 0x001d9eb0
void pre_StoryMenu_GhostList(void)
{
}

// ──────────────────────────────────────────────────────────────────────
// Every frame of phase 63 (and of its child 88): the scroll bar, the fade
// layer and the Bp layouts.
// FF3 0x001d9eb8

void after_StoryMenu_GhostList(void)
{
    short top = MenuCsrGetTopY(GHOST_LIST_PHASE);
    short row = MenuCsrGetPosY(GHOST_LIST_PHASE);
    capCtl *layer;

    if (ghost_list_bar != nullptr)
        ghost_list_bar->Draw(row, top);
    layer = GetCmnFadeLayer();
    if (CmnFadeLayerIsReady() != 0)
        layer->Update();
    BpCtlMain();
}

// ──────────────────────────────────────────────────────────────────────
// Phase 88 StoryMenu_GhostListBonus
// ──────────────────────────────────────────────────────────────────────

// FF3 0x001d9f38. Step 0: the BGM fades out (1 s; not in the other world)
// and the jingle preloads in slot 1; step 1 waits a second and for the
// preload, then plays it with "bonus_IN" and fade-layer label 10; step 2
// waits for "bonus_IN" to end.
// callcheck: GetSysWrk, GetLanguage, BpCtlGetHandleByFileNo (GhostListFile / GhostListHandle)
// callcheck: GetPALMode (GhostListFps)

int init_StoryMenu_GhostListBonus(void)
{
    GPHASE_NODE *leaf;
    int          wait, fps;

    if (GetGPhaseCtrl()->leaf->state_cnt == 0) {
        if (ingame_wrk.b03 == 0)
            SndBgmStop(GhostListFps(), 0);
        SndBgmPreload(BGM_07_SUCCESS_STR_STR, 100, 0x1000, 0, nullptr, 0, 1);
        GetGPhaseCtrl()->leaf->state_cnt = 1;
    }
    if (GetGPhaseCtrl()->leaf->state_cnt == 1) {
        leaf = GetGPhaseCtrl()->leaf;
        wait = (short)leaf->state_wrk + 1;
        fps  = GhostListFps();
        if (wait < 0)
            wait = 0;
        else if (wait > fps)
            wait = fps;
        GetGPhaseCtrl()->leaf->state_wrk = (short)wait;
        if (SndBgmIsPreloaded(1) != 0 && (short)GetGPhaseCtrl()->leaf->state_wrk >= GhostListFps()) {
            SndBgmStartPreloaded(1);
            BpPlayLabel(GhostListHandle(), "bonus_IN");             /* rodata 0x35a328 */
            CmnFadeLayerSwitchItem(10);
            GetGPhaseCtrl()->leaf->state_cnt = 2;
        }
    }
    if (GetGPhaseCtrl()->leaf->state_cnt != 2)
        return 0;
    GhostListBonusDisp();
    return BpCtlIsLoadEnd(GhostListFile()) != 0;
}

// FF3 0x001da1b0: OK back to the list.

int one_StoryMenu_GhostListBonus(void)
{
    GhostListBonusDisp();
    ReiPan_MenuMouseConfirm();          // PC: a click acknowledges
    if (CheckPadTrgPlaySe(pad_key[PAD_KEY_OK], GHOST_SE_OK, 0) == 0)
        return 0;
    GetGPhaseCtrl()->SetNext(GHOST_LIST_PHASE, 0);
    return 1;
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x001da220: "bonus_OUT", fade-layer label 9 and the jingle fades
// (1 s); once it has played the bonus is handed out, the menu BGM comes
// back when no other bonus waits (not in the other world), and the panel
// comes in for a read cursor entry.
// callcheck: GetSysWrk, GetLanguage, BpCtlGetHandleByFileNo (GhostListFile / GhostListHandle)
// callcheck: GetPALMode (GhostListFps)

int end_StoryMenu_GhostListBonus(void)
{
    short pos;

    if (GetGPhaseCtrl()->leaf->state_cnt == 0) {
        BpPlayLabel(GhostListHandle(), "bonus_OUT");                /* rodata 0x35a338 */
        CmnFadeLayerSwitchItem(9);
        SndBgmStop(GhostListFps(), 1);
        GetGPhaseCtrl()->leaf->state_cnt = 1;
    }
    GhostListBonusDisp();
    if (BpCtlIsLoadEnd(GhostListFile()) == 0)
        return 0;
    SoulListBonusSet(SoulListGetNewBonus());
    if (SoulListGetNewBonus() == GHOST_LIST_BONUS_NONE && ingame_wrk.b03 == 0)
        SndBgmPlay(BGM_01_MENU1_STR_STR, 100, 0x1000, GhostListFps(), nullptr, 1, 0);
    pos = MenuCsrGetPos(GHOST_LIST_PHASE);
    if (SoulListIsHave(pos) != 0 && SoulListIsRead(pos) != 0) {
        BpPlayLabel(GhostListHandle(), "G_list_IN");
        ghost_list_in = 1;
    }
    return 1;
}
