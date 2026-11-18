/* ==========================================================================
 *  ingame/mission/mission_end.c
 *
 *  The end of an extra mission, FF3 0x1acc18..0x1ae918 (FF3-only; see
 *  mission_end.h): the mission loading screen's hooks, phase 50
 *  Story_Mission_End and its child phase 77 Story_Mission_Result with the
 *  three result layouts' hooks. Also the finder-mode work's getter
 *  GetFinderModeWrk (FF3 0x002b8ce0, an out-of-line copy among the inline
 *  bodies at the end of the binary).
 *
 *  The phase-50 flow: init records the result (FinderModeRecordResult),
 *  picks the pending completion notice and loads the three layouts while
 *  the BGM volume fades out; one starts phase 77. Phase 77 shows one
 *  screen -- CLEAR after a clear, FAILED, or BONUS for a notice -- and on
 *  OK goes back to 50 (which starts 77 again) while a notice is pending,
 *  else to phase 11 (the album snapshot in game, AlbumSetIngame(1, 0)).
 *  50's end fades the screen, frees the layouts and prepares the retry.
 *
 *  FF3 reads its singletons through gp-relative cached pointers (sdata
 *  gp-0xab0 the finder-mode work, gp-0xc60 the system work, gp-0xca4 the
 *  phase controller, gp-0xaec the option work); nothing depends on the
 *  cache, so calls go to the getters directly. FF3 0x001ae8f8
 *  (MissionEndAtexitGPhaseCtrl) is not written: the compiler's atexit thunk
 *  for the GetGPhaseCtrl static (GPhaseCtrlTerm), ending the file.
 * ======================================================================== */
#include "ingame/mission/mission_end.h"

#include "album/prg/album.h"                // AlbumSetIngame
#include "common/cmn_fade_layer.h"          // GetCmnFadeLayer, CmnFadeLayerIsReady, capCtl::Update
#include "common/cmn_time.h"                // PlayTimeToHms
#include "graphics/graph2d/fade.h"          // FadeOutReq, GetFadeState, FadeMain
#include "graphics/graph2d/message.h"       // MsgPrintPage, PrintNumber_N
#include "graphics/motion/anicode.h"        // B2D_HOOK_REC, B2dSetHookList, GetLabelSuffixType, SetCurColorPacked
#include "ingame/camera/finder_camera.h"    // FINDER_MODE_WRK, GetFinderModeWrk
#include "ingame/ingame.h"                  // IngameMain, IngameDrawSub
#include "ingame/menu/start_menu.h"         // StartMenuDrawTime
#include "ingame/mission/mission_list.h"    // Mission*, FINDER_BANK_ENTRY
#include "ingame/mission/mission_mode.h"    // FINDER_MODE_WRK
#include "ingame/mission/ExtraMissionShelt.h"  // GetExtraMissionShelt
#include "ingame/sound/snd_ctrl.h"          // SndSetVolume, SndBgm*
#include "main/gphase.h"                    // GetGPhaseCtrl
#include "outgame/option.h"                 // LayoutItemAddPos, PrintMsgPos, CmnFadeLayer*Item, OptionGetSndVolume0
#include "system/b2d/B2dCtl.h"              // B2dCtlFindItem, B2dCtlSetItemHook, B2dGetDrawItem
#include "system/b2d/B2dSpr.h"              // B2dSprSetDisp
#include "system/b2d/BpCtl.h"               // BpCtl*, BpPlayLabel, B2dDrawSpriteDirect
#include "system/eeiop/cd_file_dat.h"       // MISSION_*_PHF, BGM_07 / BGM_08
#include "system/eeiop/stream_auto.h"       // StreamAuto*
#include "system/os/system.h"               // GetSysWrk, GetPALMode
#include "system/pad/pad_ctrl.h"            // CheckPadTrgPlaySe, pad_key
#include "ingame/menu/menu_mouse.h"    // PC: the mouse in menus

// ──────────────────────────────────────────────────────────────────────
// Callees in runs not yet assigned to a file (prototypes here until their
// file is defined; move each block to its real header then).
// ──────────────────────────────────────────────────────────────────────


// ──────────────────────────────────────────────────────────────────────
// Data
// ──────────────────────────────────────────────────────────────────────

#define MISSION_LOAD_ITEM_NUMBER    0x400   // 0x400..0x409: the digit sprites 1..10
#define MISSION_LOAD_ITEM_INFO      0x41b   // the text list item
#define MISSION_LOAD_NUMBER_Y       0x35
#define MISSION_LOAD_GROUP_X        0x15e   // the group's digit (0x156 for "10")
#define MISSION_LOAD_GROUP10_X      0x156
#define MISSION_LOAD_NO_X           0x182   // the mission's digit
#define MISSION_LOAD_ROW_DY         0x22    // one text row

#define MISSION_END_SE_OK           0x1a9b0003
#define MISSION_END_PHASE_RESULT    0x4d    // 77 Story_Mission_Result
#define MISSION_END_PHASE_END       0x32    // 50 Story_Mission_End
#define MISSION_END_PHASE_EXIT      0xb     // 11

static int mission_load_alpha;          // sbss gp-0x354: MissionLoadDrawInfo's alpha, for the digits
static int mission_end_notice;          // sbss gp-0x34c: MISSION_NOTICE_*
static int mission_end_result_shown;    // sbss gp-0x348: the first result screen ended

static const char mission_end_label_in[]  = "in";      /* sdata 0x370388 */
static const char mission_end_label_out[] = "out";     /* sdata 0x370390 */

// PAL 25, NTSC 30 frames: one second (inlined in every FF3 use).
static int MissionEndSecond(void)
{
    return GetPALMode() != 0 ? 25 : 30;
}

static int MissionEndLanguage(void)
{
    return GetSysWrk()->GetLanguage();
}

// ══════════════════════════════════════════════════════════════════════
// The finder-mode work
// ══════════════════════════════════════════════════════════════════════

// ──────────────────────────────────────────────────────────────────────
// The running extra mission's work (bss 0x424ad0; FF3's function-local
// static with a guard byte, gp-0x780, and no constructor).
// FF3 0x002b8ce0.

FINDER_MODE_WRK *GetFinderModeWrk(void)
{
    static FINDER_MODE_WRK finder_mode_wrk;

    return &finder_mode_wrk;
}

// ══════════════════════════════════════════════════════════════════════
// The mission loading screen (MISSION_LOAD_PHF + language)
// ══════════════════════════════════════════════════════════════════════

// ──────────────────────────────────────────────────────────────────────
// Message 0x6f string 'base' of the running mission.
// FF3 0x001acc18.

static int MissionCurGetMsgNo(int base)
{
    FINDER_MODE_WRK *wrk = GetFinderModeWrk();

    return MissionGetMsgNo(base, wrk->mission_grp, wrk->mission_no);
}

// ──────────────────────────────────────────────────────────────────────
// List hook of item 0x41b: the mission's texts -- message 0x6f rows 1, 3,
// 4 and 5 (rows 0 and 2 are drawn elsewhere), then the player, the clear
// type and the time limit under the item. Keeps its alpha for the digits.
// FF3 0x001acc60 (no Ghidra function before 2026-10-20: inside 0x1acc18).
// callcheck: GetLanguage, GetSysWrk (in MissionEndLanguage)

static int MissionLoadDrawInfo(const B2D_HOOK_ARGS *args)
{
    static const int row_y[6] = { 0, 0, 0, 3, 5, 6 };  /* rodata 0x3587c8, copied to the stack */
    const char        *name  = (const char *)args->a0;
    int               *xy    = (int *)args->t1;
    int                alpha = (int)args->stack[2];
    FINDER_BANK_ENTRY *entry;
    float              pos[2];
    int                hms[4];
    int                type;
    int                i;

    entry = GetFinderModeWrk()->GetDef();
    SetCurColorPacked((u_int)args->t3);
    type = GetLabelSuffixType(name);
    for (i = 0; i < 6; i++) {
        if (i == 0 || i == 2) {
            continue;
        }
        pos[0] = 0.0f;
        pos[1] = 0.0f;
        LayoutItemAddPos(pos, name, MISSION_LOAD_PHF + MissionEndLanguage(), xy);
        pos[1] += (float)(row_y[i] * MISSION_LOAD_ROW_DY);
        PrintMsgPos(0x6f, MissionCurGetMsgNo(i), pos, 0x2d, alpha, 0, type);
    }

    pos[0] = 0.0f;
    pos[1] = 0.0f;
    LayoutItemAddPos(pos, name, MISSION_LOAD_PHF + MissionEndLanguage(), xy);
    pos[1] += 68.0f;                                    /* sdata 0x370378 */
    PrintMsgPos(100, (signed char)entry->plyr + 0x16, pos, 0x2d, alpha, 0, type);

    pos[0] = 0.0f;
    pos[1] = 0.0f;
    LayoutItemAddPos(pos, name, MISSION_LOAD_PHF + MissionEndLanguage(), xy);
    pos[1] += 34.0f;                                    /* sdata 0x37037c */
    PrintMsgPos(100, entry->type + 3, pos, 0x2d, alpha, 0, type);

    pos[0] = 0.0f;
    pos[1] = 0.0f;
    LayoutItemAddPos(pos, name, MISSION_LOAD_PHF + MissionEndLanguage(), xy);
    pos[1] += 136.0f;                                   /* sdata 0x370380 */
    if (entry->limit < 0) {
        PrintMsgPos(100, 0x28, pos, 0x2d, alpha, 0, type);
    } else {
        PlayTimeToHms(hms, (u_int)entry->limit);
        StartMenuDrawTime(hms, pos, alpha, 1);
    }
    SetDefaultGsDrawState();
    mission_load_alpha = alpha;
    return 0;
}

// ──────────────────────────────────────────────────────────────────────
// Item hook of the digit sprites 0x400..0x409 ("1".."10"): the item's
// sprite where the group number goes when it is the running group, and
// where the mission number goes when it is the running mission, in the
// text's alpha.
// FF3 0x001acf88 (no Ghidra function before 2026-10-20: inside 0x1acc18).

static int MissionLoadDrawNumber(const B2D_HOOK_ARGS *args)
{
    FINDER_MODE_WRK *wrk = GetFinderModeWrk();
    B2D_HOOK_ARGS    draw = *args;
    int              no;

    no = B2dGetDrawItem()->id - MISSION_LOAD_ITEM_NUMBER;
    draw.t2       = MISSION_LOAD_NUMBER_Y;
    draw.stack[1] = mission_load_alpha;
    if (wrk->mission_grp == no) {
        draw.t1 = (no == 9) ? MISSION_LOAD_GROUP10_X : MISSION_LOAD_GROUP_X;
        B2dDrawSpriteDirect(&draw);
    }
    if (wrk->mission_no == no) {
        draw.t1 = MISSION_LOAD_NO_X;
        B2dDrawSpriteDirect(&draw);
    }
    return 0;
}

// ──────────────────────────────────────────────────────────────────────
// CcmLoadingMission::OnReady: the loading layout's hooks.
// FF3 0x001ad0d0.
// callcheck: GetLanguage, GetSysWrk (in MissionEndLanguage)

void MissionLoadSetHooks(void)
{
    static const B2D_HOOK_REC hooks[] =                 /* built on the stack */
    {
        { MissionLoadDrawInfo, 0, MISSION_LOAD_ITEM_INFO, nullptr },
        { nullptr,            -1, -1,                     nullptr },
    };
    B2D_ITEM_HOOK hook;
    int           handle;
    int           i;

    handle = BpCtlGetHandleByFileNo(MISSION_LOAD_PHF + MissionEndLanguage());
    mission_load_alpha = 0;
    for (i = 0; i < 10; i++) {
        hook.func = MissionLoadDrawNumber;
        B2dCtlSetItemHook(B2dCtlFindItem(handle, MISSION_LOAD_ITEM_NUMBER + i), &hook);
    }
    B2dSetHookList(handle, hooks);
}

// ══════════════════════════════════════════════════════════════════════
// The result screens
// ══════════════════════════════════════════════════════════════════════

// ──────────────────────────────────────────────────────────────────────
// The completion notice still to show: "every mission topped" first, then
// "every mission played"; MISSION_NOTICE_NONE when neither is new.
// FF3 0x001ad1c8.

static int MissionEndGetNotice(void)
{
    if (MissionCompleteIsShown(1) == 0 && MissionIsComplete(1) != 0) {
        return MISSION_NOTICE_TOP;
    }
    if (MissionCompleteIsShown(0) == 0 && MissionIsComplete(0) != 0) {
        return MISSION_NOTICE_PLAYED;
    }
    return MISSION_NOTICE_NONE;
}

// ──────────────────────────────────────────────────────────────────────
// CLEAR on the first screen after a clear; afterwards (and after a
// failure) BONUS while a notice is pending, else FAILED.
// FF3 0x001ad228.

static int MissionEndGetScreen(void)
{
    FINDER_MODE_WRK *wrk = GetFinderModeWrk();

    if (mission_end_result_shown == 0 && wrk->result != 0) {
        return MISSION_SCREEN_CLEAR;
    }
    return (mission_end_notice == MISSION_NOTICE_NONE) ? MISSION_SCREEN_FAILED : MISSION_SCREEN_BONUS;
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x001ad280.
// callcheck: GetLanguage, GetSysWrk (in MissionEndLanguage)

static int MissionEndGetScreenFile(void)
{
    switch (MissionEndGetScreen()) {
    case MISSION_SCREEN_CLEAR:
        return MISSION_CLEAR_PHF + MissionEndLanguage();
    case MISSION_SCREEN_BONUS:
        return MISSION_BONUS_PHF + MissionEndLanguage();
    default:
        return MISSION_FAILED_PHF + MissionEndLanguage();
    }
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x001ad328.

static int MissionEndGetScreenBgm(void)
{
    switch (MissionEndGetScreen()) {
    case MISSION_SCREEN_CLEAR:
    case MISSION_SCREEN_BONUS:
        return BGM_07_SUCCESS_STR_STR;
    default:
        return BGM_08_FAILURE_STR_STR;
    }
}

// ──────────────────────────────────────────────────────────────────────
// The fade layer's item over the screen (CmnFadeLayerShowItem).
// FF3 0x001ad380.

static int MissionEndGetScreenItem(void)
{
    switch (MissionEndGetScreen()) {
    case MISSION_SCREEN_CLEAR:  return 0x40a;
    case MISSION_SCREEN_BONUS:  return 0x3f7;
    case MISSION_SCREEN_FAILED: return 0x3f6;
    default:                    return 1000;
    }
}

// ──────────────────────────────────────────────────────────────────────
// Every frame once the layouts are in: BONUS shows the notice's title;
// CLEAR shows the clear type's icon, the rank and, when this clear played
// the group's last mission, the next group's "open" sprites.
// FF3 0x001ad3d8.
// callcheck: GetLanguage, GetSysWrk (in MissionEndLanguage)

static void MissionEndUpdateScreen(void)
{
    static const short hide_item[6] = { 0x418, 0x419, 0x41a, 0x41b, 0x420, 0x421 };    /* built on the stack */
    static const short type_item[7] = { -1, -1, -1, 0x41b, 0x419, 0x41a, 0x418 };      /* built on the stack */
    static const short rank_item[6] = { 0x3ff, 0x3fe, 0x403, 0x402, 0x401, 0x400 };    /* rodata 0x358930 */
    FINDER_MODE_WRK   *wrk = GetFinderModeWrk();
    FINDER_BANK_ENTRY *entry;
    B2D_ITEM          *item;
    B2D_ITEM          *open_item;
    int                rank;
    int                group;
    int                file;
    int                handle;
    int                is_top;
    int                open;
    int                i;

    rank   = wrk->GetRank();
    group  = wrk->mission_grp;
    file   = MissionEndGetScreenFile();
    handle = BpCtlGetHandleByFileNo(file);

    if (file == MISSION_BONUS_PHF + MissionEndLanguage()) {
        B2D_ITEM *top_item    = B2dCtlFindItem(handle, 0x3fb);
        B2D_ITEM *played_item = B2dCtlFindItem(handle, 0x3fa);

        is_top = mission_end_notice != MISSION_NOTICE_PLAYED;
        B2dSprSetDisp(top_item, is_top ? 1 : 0);
        B2dSprSetDisp(played_item, !is_top);
    }

    if (file == MISSION_CLEAR_PHF + MissionEndLanguage()) {
        for (i = 0; i < 6; i++) {
            B2dSprSetDisp(B2dCtlFindItem(handle, hide_item[i]), 0);
        }
        entry = GetFinderModeWrk()->GetDef();
        // PC: FF3 indexes its 7-entry stack table unchecked; played missions
        // have types 0..6 (type 8 marks an unused row).
        if (entry->type >= 0 && entry->type < 7 && type_item[entry->type] >= 0) {
            B2dSprSetDisp(B2dCtlFindItem(handle, type_item[entry->type]), 1);
            if (entry->type == 3) {
                B2dSprSetDisp(B2dCtlFindItem(handle, 0x420), 1);
                B2dSprSetDisp(B2dCtlFindItem(handle, 0x421), 1);
            }
        }
        for (i = 0; i < 6; i++) {
            B2dSprSetDisp(B2dCtlFindItem(handle, rank_item[i]), i == rank);
        }
        open = MissionGroupGetOpen(group);
        open_item = B2dCtlFindItem(handle, 0x40b);
        B2dSprSetDisp(open_item, 0);
        for (i = 0; i < MISSION_GROUP_NUM; i++) {
            item = B2dCtlFindItem(handle, 0x3ed + i);
            if (open == 0 && i == group && MissionGroupIsAllPlayed(i) != 0) {
                B2dSprSetDisp(item, 1);
                B2dSprSetDisp(open_item, 1);
            } else {
                B2dSprSetDisp(item, 0);
            }
        }
    }
}

// ──────────────────────────────────────────────────────────────────────
// 1 when the three result layouts are loaded.
// FF3 0x001ad710.
// callcheck: GetLanguage, GetSysWrk (in MissionEndLanguage)

static int MissionEndIsLoaded(void)
{
    if (BpCtlIsDataReady(BpCtlGetHandleByFileNo(MISSION_BONUS_PHF + MissionEndLanguage())) == 0) {
        return 0;
    }
    if (BpCtlIsDataReady(BpCtlGetHandleByFileNo(MISSION_CLEAR_PHF + MissionEndLanguage())) == 0) {
        return 0;
    }
    if (BpCtlIsDataReady(BpCtlGetHandleByFileNo(MISSION_FAILED_PHF + MissionEndLanguage())) == 0) {
        return 0;
    }
    return 1;
}

// ──────────────────────────────────────────────────────────────────────
// Hook of item 0x3fc on BONUS: the notice's text, message 100 / 0x23
// (every mission played) or 0x24 (every mission topped).
// FF3 0x001ad7d0.
// callcheck: GetLanguage, GetSysWrk (in MissionEndLanguage)

static int MissionBonusDrawMsg(const B2D_HOOK_ARGS *args)
{
    const char *name  = (const char *)args->a0;
    int        *xy    = (int *)args->t1;
    int         alpha = (int)args->stack[2];
    float       pos[2] = { 0.0f, 0.0f };
    int         type;

    SetCurColorPacked((u_int)args->t3);
    type = GetLabelSuffixType(name);
    LayoutItemAddPos(pos, name, MISSION_BONUS_PHF + MissionEndLanguage(), xy);
    MsgPrintPage(100, (mission_end_notice != MISSION_NOTICE_PLAYED) ? 0x24 : 0x23,
                 (int)pos[0], (int)pos[1], 0x2d, alpha, 0, 0, 0, type);
    SetDefaultGsDrawState();
    return 0;
}

// ──────────────────────────────────────────────────────────────────────
// Hook of 0x411 (CLEAR) / 0x3f8 (FAILED): the mission's time, h:m:s.
// FF3 0x001ad8d0 (no Ghidra function before 2026-10-20: inside 0x1ad7d0).

static int MissionEndDrawTime(const B2D_HOOK_ARGS *args)
{
    FINDER_MODE_WRK *wrk = GetFinderModeWrk();
    const int       *xy  = (const int *)args->t1;
    float            pos[2] = { 0.0f, 0.0f };
    int              hms[4];

    SetCurColorPacked((u_int)args->t3);
    pos[0] += (float)xy[0];
    pos[1] += (float)xy[1];
    PlayTimeToHms(hms, (u_int)wrk->time);
    StartMenuDrawTime(hms, pos, (int)args->stack[2], 1);
    SetDefaultGsDrawState();
    return 0;
}

// ──────────────────────────────────────────────────────────────────────
// Hook of 0x412 / 0x3f9: the best single shot's score, six digits.
// FF3 0x001ad980.

static int MissionEndDrawBestShot(const B2D_HOOK_ARGS *args)
{
    const int *xy = (const int *)args->t1;

    SetCurColorPacked((u_int)args->t3);
    PrintNumber_N(GetFinderModeWrk()->best_shot, 6, xy[0], xy[1], 0x2d, (u_char)args->stack[2], 0, 1, 0);
    SetDefaultGsDrawState();
    return 0;
}

// ──────────────────────────────────────────────────────────────────────
// Hook of 0x413 / 0x3fa: the total score, six digits.
// FF3 0x001ad9f8.

static int MissionEndDrawScore(const B2D_HOOK_ARGS *args)
{
    const int *xy = (const int *)args->t1;

    SetCurColorPacked((u_int)args->t3);
    PrintNumber_N(GetFinderModeWrk()->score, 6, xy[0], xy[1], 0x2d, (u_char)args->stack[2], 0, 1, 0);
    SetDefaultGsDrawState();
    return 0;
}

// ──────────────────────────────────────────────────────────────────────
// Message 100 / 0x15 at the item, on 'file' (+ language).

static void MissionEndDrawLabel(const B2D_HOOK_ARGS *args, int file)
{
    const char *name = (const char *)args->a0;
    float       pos[2] = { 0.0f, 0.0f };
    int         type;

    SetCurColorPacked((u_int)args->t3);
    type = GetLabelSuffixType(name);
    LayoutItemAddPos(pos, name, file + MissionEndLanguage(), (int *)args->t1);
    PrintMsgPos(100, 0x15, pos, 0x2d, (int)args->stack[2], 0, type);
    SetDefaultGsDrawState();
}

// ──────────────────────────────────────────────────────────────────────
// Hook of 0x414 / 0x415 on CLEAR.
// FF3 0x001ada70.
// callcheck: GetLabelSuffixType, GetLanguage, GetSysWrk, LayoutItemAddPos, PrintMsgPos, SetCurColorPacked, SetDefaultGsDrawState (in MissionEndDrawLabel, MissionEndLanguage)

static int MissionClearDrawLabel(const B2D_HOOK_ARGS *args)
{
    MissionEndDrawLabel(args, MISSION_CLEAR_PHF);
    return 0;
}

// ──────────────────────────────────────────────────────────────────────
// Hook of 0x3fb / 0x3fc on FAILED.
// FF3 0x001adb20.
// callcheck: GetLabelSuffixType, GetLanguage, GetSysWrk, LayoutItemAddPos, PrintMsgPos, SetCurColorPacked, SetDefaultGsDrawState (in MissionEndDrawLabel, MissionEndLanguage)

static int MissionFailedDrawLabel(const B2D_HOOK_ARGS *args)
{
    MissionEndDrawLabel(args, MISSION_FAILED_PHF);
    return 0;
}

// ──────────────────────────────────────────────────────────────────────
// Hook of 0x41d on CLEAR: for the counting clear types 3 / 4 / 5 the
// count's label, message 100 / 0x25..0x27.
// FF3 0x001adbd0.
// callcheck: GetLanguage, GetSysWrk (in MissionEndLanguage)

static int MissionClearDrawUnit(const B2D_HOOK_ARGS *args)
{
    const char *name = (const char *)args->a0;
    float       pos[2];
    int         msg;
    int         type;

    switch (GetFinderModeWrk()->GetDef()->type) {
    case 3:  msg = 0x25; break;
    case 4:  msg = 0x26; break;
    case 5:  msg = 0x27; break;
    default: return 0;
    }
    pos[0] = 0.0f;
    pos[1] = 0.0f;
    SetCurColorPacked((u_int)args->t3);
    type = GetLabelSuffixType(name);
    LayoutItemAddPos(pos, name, MISSION_CLEAR_PHF + MissionEndLanguage(), (int *)args->t1);
    PrintMsgPos(100, msg, pos, 0x2d, (int)args->stack[2], 0, type);
    SetDefaultGsDrawState();
    return 0;
}

// ──────────────────────────────────────────────────────────────────────
// Hook of 0x41e on CLEAR: for clear types 3..5 the counted value, six
// digits (the position from LayoutItemAddPos is not used).
// FF3 0x001adce8 (no Ghidra function before 2026-10-20: inside 0x1adbd0).
// callcheck: GetLanguage, GetSysWrk (in MissionEndLanguage)

static int MissionClearDrawCount(const B2D_HOOK_ARGS *args)
{
    FINDER_MODE_WRK *wrk = GetFinderModeWrk();
    const int       *xy  = (const int *)args->t1;
    float            pos[2] = { 0.0f, 0.0f };

    if ((u_int)(wrk->GetDef()->type - 3) >= 3) {
        return 0;
    }
    SetCurColorPacked((u_int)args->t3);
    LayoutItemAddPos(pos, (const char *)args->a0, MISSION_CLEAR_PHF + MissionEndLanguage(),
                     (int *)args->t1);
    PrintNumber_N(wrk->GetValue(), 6, xy[0], xy[1], 0x2d, (u_char)args->stack[2], 0, 1, 0);
    SetDefaultGsDrawState();
    return 0;
}

// ──────────────────────────────────────────────────────────────────────
// Hook of 0x41f on CLEAR: for clear type 6 the time, h:m:s.
// FF3 0x001adde0.

static int MissionClearDrawClearTime(const B2D_HOOK_ARGS *args)
{
    FINDER_MODE_WRK *wrk = GetFinderModeWrk();
    const int       *xy  = (const int *)args->t1;
    float            pos[2] = { 0.0f, 0.0f };
    int              hms[4];

    if (wrk->GetDef()->type != 6) {
        return 0;
    }
    SetCurColorPacked((u_int)args->t3);
    pos[0] += (float)xy[0];
    pos[1] += (float)xy[1];
    PlayTimeToHms(hms, (u_int)wrk->time_348);
    StartMenuDrawTime(hms, pos, (int)args->stack[2], 1);
    SetDefaultGsDrawState();
    return 0;
}

static const B2D_HOOK_REC mission_bonus_hooks[] =      /* built on the stack */
{
    { MissionBonusDrawMsg,    0, 0x3fc, nullptr },
    { nullptr,               -1, -1,    nullptr },
};

static const B2D_HOOK_REC mission_clear_hooks[] =      /* rodata 0x358840, copied to the stack */
{
    { MissionEndDrawTime,        0, 0x411, nullptr },
    { MissionEndDrawBestShot,    0, 0x412, nullptr },
    { MissionEndDrawScore,       0, 0x413, nullptr },
    { MissionClearDrawLabel,     0, 0x414, nullptr },
    { MissionClearDrawLabel,     0, 0x415, nullptr },
    { MissionClearDrawUnit,      0, 0x41d, nullptr },
    { MissionClearDrawCount,     0, 0x41e, nullptr },
    { MissionClearDrawClearTime, 0, 0x41f, nullptr },
    { nullptr,                  -1, -1,    nullptr },
};

static const B2D_HOOK_REC mission_failed_hooks[] =     /* rodata 0x3588d0, copied to the stack */
{
    { MissionEndDrawTime,     0, 0x3f8, nullptr },
    { MissionEndDrawBestShot, 0, 0x3f9, nullptr },
    { MissionEndDrawScore,    0, 0x3fa, nullptr },
    { MissionFailedDrawLabel, 0, 0x3fb, nullptr },
    { MissionFailedDrawLabel, 0, 0x3fc, nullptr },
    { nullptr,               -1, -1,    nullptr },
};

// ──────────────────────────────────────────────────────────────────────
// FF3 0x001adeb0.
// callcheck: GetLanguage, GetSysWrk (in MissionEndLanguage)

static void MissionEndSetHooks(void)
{
    int bonus  = BpCtlGetHandleByFileNo(MISSION_BONUS_PHF + MissionEndLanguage());
    int clear  = BpCtlGetHandleByFileNo(MISSION_CLEAR_PHF + MissionEndLanguage());
    int failed = BpCtlGetHandleByFileNo(MISSION_FAILED_PHF + MissionEndLanguage());

    B2dSetHookList(bonus, mission_bonus_hooks);
    B2dSetHookList(clear, mission_clear_hooks);
    B2dSetHookList(failed, mission_failed_hooks);
}

// ══════════════════════════════════════════════════════════════════════
// Phase 50 Story_Mission_End
// ══════════════════════════════════════════════════════════════════════

// ──────────────────────────────────────────────────────────────────────
// Records the result and loads the result layouts while the BGM fades out
// over a second (state_wrk counts the frames); done once all is loaded,
// the second is over and every stream has stopped.
// FF3 0x001ae040.
// callcheck: GetLanguage, GetOptionWrk, GetPALMode, GetSysWrk (in MissionEndLanguage, MissionEndSecond, OptionGetSndVolume0)

int init_Story_Mission_End(void)
{
    FINDER_MODE_WRK *wrk = GetFinderModeWrk();
    GPHASE_NODE     *leaf = GetGPhaseCtrl()->leaf;
    int              second;
    int              cnt;

    if (leaf->state_cnt == 0) {
        StreamAutoSetPlayNum(0, MissionEndSecond());
        wrk->RecordResult();
        mission_end_notice = MissionEndGetNotice();
        mission_end_result_shown = 0;
        BpCtlLoadReq(MISSION_BONUS_PHF + MissionEndLanguage(), nullptr);
        BpCtlLoadReq(MISSION_CLEAR_PHF + MissionEndLanguage(), nullptr);
        BpCtlLoadReq(MISSION_FAILED_PHF + MissionEndLanguage(), nullptr);
        GetGPhaseCtrl()->leaf->state_cnt = 1;
    }

    second = MissionEndSecond();
    cnt = (short)GetGPhaseCtrl()->leaf->state_wrk + 1;
    if (cnt < 0) {
        cnt = 0;
    } else if (cnt > second) {
        cnt = second;
    }
    GetGPhaseCtrl()->leaf->state_wrk = (short)cnt;

    cnt = (short)GetGPhaseCtrl()->leaf->state_wrk;
    SndSetVolume(0, OptionGetSndVolume0() * (MissionEndSecond() - cnt) / MissionEndSecond());

    if (MissionEndIsLoaded() == 0) {
        return 0;
    }
    if ((short)GetGPhaseCtrl()->leaf->state_wrk < MissionEndSecond()) {
        return 0;
    }
    if (StreamAutoIsAllStop() == 0) {
        return 0;
    }
    MissionEndSetHooks();
    StreamAutoAllStop();
    StreamAutoSetPlayNum(2, 0);
    return 1;
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x001ae310.

void pre_Story_Mission_End(void)
{
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x001ae318.

int one_Story_Mission_End(void)
{
    GetGPhaseCtrl()->SetNext(MISSION_END_PHASE_RESULT, 0);
    return 1;
}

// ──────────────────────────────────────────────────────────────────────
// The game keeps running under the screens (IngameMain with every skip
// flag), drawn by IngameDrawSub; the fade layer on top.
// FF3 0x001ae358.

void after_Story_Mission_End(void)
{
    capCtl *layer;

    if (MissionEndIsLoaded() != 0) {
        MissionEndUpdateScreen();
    }
    IngameMain(0xffffffffu);
    IngameDrawSub(0);
    BpCtlMain();
    layer = GetCmnFadeLayer();
    if (CmnFadeLayerIsReady() != 0) {
        layer->Update();
    }
}

// ──────────────────────────────────────────────────────────────────────
// Fades out (at once after a failure, over a second after a clear), then
// frees the layouts, restores the BGM volume and prepares the retry.
// FF3 0x001ae3d0.
// callcheck: GetLanguage, GetOptionWrk, GetPALMode, GetSysWrk (in MissionEndLanguage, MissionEndSecond, OptionGetSndVolume0)

int end_Story_Mission_End(void)
{
    if (GetGPhaseCtrl()->leaf->state_cnt == 0) {
        FadeOutReq(0, 0, 0, (GetFinderModeWrk()->result == 0) ? 1 : (u_int)MissionEndSecond());
        GetGPhaseCtrl()->leaf->state_cnt = 1;
    }
    if (GetFadeState() == 0) {
        FadeMain();
        return 0;
    }
    BpCtlRelease(BpCtlGetHandleByFileNo(MISSION_BONUS_PHF + MissionEndLanguage()));
    BpCtlRelease(BpCtlGetHandleByFileNo(MISSION_CLEAR_PHF + MissionEndLanguage()));
    BpCtlRelease(BpCtlGetHandleByFileNo(MISSION_FAILED_PHF + MissionEndLanguage()));
    SndSetVolume(0, OptionGetSndVolume0());
    GetExtraMissionShelt()->CarryStore();
    FadeMain();
    return 1;
}

// ══════════════════════════════════════════════════════════════════════
// Phase 77 Story_Mission_Result
// ══════════════════════════════════════════════════════════════════════

// ──────────────────────────────────────────────────────────────────────
// Preloads the screen's BGM, then plays its "in" label with the BGM and
// the fade layer's item; done when the label has played.
// FF3 0x001ae598.

int init_Story_Mission_Result(void)
{
    GPHASE_NODE *leaf;
    int          file;
    int          handle;

    file   = MissionEndGetScreenFile();
    handle = BpCtlGetHandleByFileNo(file);
    leaf   = GetGPhaseCtrl()->leaf;
    if (leaf->state_cnt == 0) {
        SndBgmPreload(MissionEndGetScreenBgm(), 100, 0x1000, 0, nullptr, 0, 0);
        GetGPhaseCtrl()->leaf->state_cnt = 1;
        return 0;
    }
    if (leaf->state_cnt != 1) {
        return (BpCtlIsLoadEnd(file) != 0) ? 1 : 0;
    }
    if (SndBgmIsPreloaded(0) == 0) {
        return 0;
    }
    BpPlayLabel(BpCtlGetHandleByFileNo(file), mission_end_label_in);
    SndBgmStartPreloaded(0);
    CmnFadeLayerShowItem(10, handle, MissionEndGetScreenItem());
    GetGPhaseCtrl()->leaf->state_cnt = 2;
    return 0;
}

// ──────────────────────────────────────────────────────────────────────
// OK: a notice screen marks its notice shown; then back to phase 50 for
// the next notice, or out to phase 11.
// FF3 0x001ae700.

int one_Story_Mission_Result(void)
{
    ReiPan_MenuMouseConfirm();          // PC: a click goes on
    if (CheckPadTrgPlaySe((u_short)pad_key[PAD_KEY_OK], MISSION_END_SE_OK, 0) == 0) {
        return 0;
    }
    if (mission_end_result_shown != 0 && mission_end_notice != MISSION_NOTICE_NONE) {
        MissionCompleteSetShown(mission_end_notice);
    }
    if (MissionEndGetNotice() == MISSION_NOTICE_NONE) {
        GetGPhaseCtrl()->SetNext(MISSION_END_PHASE_EXIT, 0);
        AlbumSetIngame(1, 0);
        return 1;
    }
    GetGPhaseCtrl()->SetNext(MISSION_END_PHASE_END, 0);
    return 1;
}

// ──────────────────────────────────────────────────────────────────────
// Plays the "out" label; when it has played, ends the fade-layer item,
// opens the next group if this clear played the group's last mission
// (first screen only), picks the next notice and stops the BGM.
// FF3 0x001ae7d0.
// callcheck: GetPALMode (in MissionEndSecond)

int end_Story_Mission_Result(void)
{
    FINDER_MODE_WRK *wrk;
    int              file;

    file = MissionEndGetScreenFile();
    if (GetGPhaseCtrl()->leaf->state_cnt == 0) {
        BpPlayLabel(BpCtlGetHandleByFileNo(file), mission_end_label_out);
        GetGPhaseCtrl()->leaf->state_cnt = 1;
    }
    if (BpCtlIsLoadEnd(file) == 0) {
        return 0;
    }
    wrk = GetFinderModeWrk();
    CmnFadeLayerTermItem();
    if (mission_end_result_shown == 0 && MissionGroupIsAllPlayed(wrk->mission_grp) != 0) {
        MissionGroupSetOpen(wrk->mission_grp);
    }
    mission_end_notice = MissionEndGetNotice();
    mission_end_result_shown = 1;
    SndBgmStop(MissionEndSecond(), 0);
    return 1;
}
