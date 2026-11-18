/* ==========================================================================
 *  ingame/story_menu.c
 *
 *  The lock table and the story menu (phase 37). FF3 0x1f1600..0x1f20b8;
 *  see story_menu.h.
 *
 *  In address order: the lock table, the item table and its lookups,
 *  IsMenuEnable, the row list and cursor wrappers, the menu BGM switch and
 *  phase 37's init/end/one/pre/after.
 *  FF3 0x001f2090 is not written: the compiler's atexit thunk for the
 *  GPhaseCtrl singleton (GPhaseCtrlTerm), the file-end marker; nor the
 *  empty unreferenced function at 0x001f20b0 after it.
 * ======================================================================== */
#include "ingame/story_menu.h"
#include "common/reipan_stub.h"
#include "system/mc/dat/save_data.h"

#include <string.h>                                     // memset

#include "ingame/enemy/ene_main.h"                     // EneIsAnyAlive
#include "graphics/gs_reg.h"                            // ScreenCaptureSave / Disp
#include "ingame/enemy/enemy.h"                         // EneReqIsPending
#include "ingame/ingame.h"                              // GPhaseCtrlRequest
#include "ingame/ingame_wrk.h"                          // ingame_wrk.lock_tbl, plyr_wrk
#include "ingame/map/MapSky.h"                          // GetMapSky, MapSkyMovie*
#include "ingame/menu/menu_csr.h"                       // MenuCsr*
#include "ingame/menu/menu_bg.h"                      // MENU_LIST, MenuList*, MenuBg*
#include "ingame/event/prg/ev_command.h"                // EvSoundPause / Restart
#include "ingame/plyr/player.h"                         // PLYR_WRK flags, PlyrSetB260Mode2, PlyrIsBattle
#include "ingame/sound/snd_ctrl.h"                      // SndBgmPlay / Stop
#include "main/gphase.h"                                // GetGPhaseCtrl
#include "system/b2d/BpCtl.h"                           // BpCtlMain
#include "system/eeiop/cd_file_dat.h"                   // MENUBG_PHF, BGM_01_MENU1_STR_STR
#include "system/eeiop/snd_buffer.h"                    // SndBufAll*LoopSnd
#include "system/eeiop/stream_auto.h"                   // StreamAutoSetExclusiveMode
#include "system/os/system.h"                           // GetPALMode
#include "system/pad/pad.h"                             // pad_wrk[0].stick_dpad
#include "ingame/item/prg/item.h"      // ItemGetCnt
#include "ingame/menu/menu_file_list.h" // MemoMenuIsEnable

// ──────────────────────────────────────────────────────────────────────
// Data
// ──────────────────────────────────────────────────────────────────────


// FF3 data 0x35b670 (labels in sdata 0x370ec0.., "R camera" at 0x35b660).
static const STORY_MENU_ITEM story_menu_item[] =
{
    { "R map",    0, 0 },
    { "R item",   1, 1 },
    { "R camera", 2, 2 },
    { "R photo",  3, 3 },
    { "R file",   4, 4 },
    { "R memo",   5, 5 },
    { "R list",   6, 6 },
    { nullptr,   -1, 0 },
};

static short      story_menu_ret_phase;                 // sdata 0x3734a8 (gp-0x48)
static MENU_LIST *story_menu_list;                      // sdata 0x3734ac (gp-0x44)
static short      story_menu_bgm_mode;                  // sdata 0x3734aa (gp-0x46): StoryMenuGetBgmMode at init

// ──────────────────────────────────────────────────────────────────────
// Lock table
// ──────────────────────────────────────────────────────────────────────

#define LOCK_TBL_FLG    0x01                            // bit 0: LockTblSet / LockTblReset

static inline int LockTblGetCnt(u_char v)               // bits 1..7, signed
{
    return (s_char)v >> 1;
}

// FF3 0x001f1600.

void LockTblInit(void)
{
    memset(ingame_wrk.lock_tbl, 0, sizeof(ingame_wrk.lock_tbl));
}

// FF3 0x001f1628.

void LockTblSet(int no)
{
    ingame_wrk.lock_tbl[(u_char)no] |= LOCK_TBL_FLG;
}

// FF3 0x001f1648.

void LockTblReset(int no)
{
    ingame_wrk.lock_tbl[(u_char)no] &= ~LOCK_TBL_FLG;
}

// FF3 0x001f1668.

int IsLockTblSet(int no)
{
    return ingame_wrk.lock_tbl[(u_char)no] & LOCK_TBL_FLG;
}

// FF3 0x001f1690.

void LockTblInc(int no)
{
    u_char v = ingame_wrk.lock_tbl[(u_char)no];

    ingame_wrk.lock_tbl[(u_char)no] = (v & LOCK_TBL_FLG) | (u_char)((LockTblGetCnt(v) + 1) << 1);
}

// FF3 0x001f16c8.

void LockTblDec(int no)
{
    u_char v = ingame_wrk.lock_tbl[(u_char)no];

    ingame_wrk.lock_tbl[(u_char)no] = (v & LOCK_TBL_FLG) | (u_char)((LockTblGetCnt(v) - 1) << 1);
}

// FF3 0x001f1700.

int IsLockTblCnt(int no)
{
    return LockTblGetCnt(ingame_wrk.lock_tbl[(u_char)no]) != 0;
}

// FF3 0x001f1740.

int IsLockTblOn(int no)
{
    u_char v = ingame_wrk.lock_tbl[(u_char)no];

    return (v & LOCK_TBL_FLG) | (LockTblGetCnt(v) != 0);
}

// ──────────────────────────────────────────────────────────────────────
// Item table
// ──────────────────────────────────────────────────────────────────────

// FF3 0x001f1788.

short StoryMenuGetRetPhase(void)
{
    return story_menu_ret_phase;
}

// FF3 0x001f1790.

int StoryMenuGetItemNum(void)
{
    return 7;
}

// FF3 0x001f1798.

const char *StoryMenuGetItemLabel(int i)
{
    return story_menu_item[i].label;
}

// The entry with this id, nullptr if none.
// FF3 0x001f17b8.

const STORY_MENU_ITEM *StoryMenuFindItem(int id)
{
    const STORY_MENU_ITEM *item;

    for (item = story_menu_item; item->label != nullptr; item++)
    {
        if (item->id == id)
            return item;
    }
    return nullptr;
}

// FF3 0x001f17f8.

const char *StoryMenuGetItemLabelById(int id)
{
    return StoryMenuFindItem(id)->label;
}

// Help message of a list row: row 0 (the map) shows message 7 while lock
// 0x1b is on.
// FF3 0x001f1818.

int StoryMenuGetHelpMsg(int row)
{
    const STORY_MENU_ITEM *item;

    if (row == 0 && IsLockTblOn(0x1b))
        return 7;

    item = StoryMenuFindItem(MenuListGetId(story_menu_list, (short)row));
    if (item == nullptr)
        return 7;                   // PC: FF3 reads address 8 (unreachable with a valid row)
    return item->msg;
}

// The menu button may open the story menu: lock 0xf off, the player not in
// the PLYR_FLG_400000 / PLYR_FLG_1000000 states, and the current phase not
// 0x23.
// FF3 0x001f1870.

int IsMenuEnable(void)
{
    if (IsLockTblOn(0xf))
        return 0;
    if (ingame_wrk.plyr_wrk->flags & (PLYR_FLG_400000 | PLYR_FLG_1000000))
        return 0;
    return GetGPhaseCtrl()->leaf->id != 0x23;
}

// ──────────────────────────────────────────────────────────────────────
// Row list and cursor
// ──────────────────────────────────────────────────────────────────────

// The top menu's rows (item id -> phase): R map, R item, R camera (when
// the current player has item 8), R photo, R file,
// R memo (when the finder may open), R list (as R camera). The phase names
// in phases.py for 58..64 are FF2-context guesses and do not all fit.
// FF3 0x001f18e0 (was KaiPzlCancel, a wrong FF2 match).

MENU_LIST *StoryMenuListCreate(void)
{
    MENU_LIST *list = MenuListCreate(8);

    if (list != nullptr)
    {
        MenuListAdd(list, 0, 0x3a);
        MenuListAdd(list, 1, 0x3b);
        if (*ItemGetCnt(8) > 0)
            MenuListAdd(list, 2, 0x40);
        MenuListAdd(list, 3, 0x3c);
        MenuListAdd(list, 4, 0x3d);
        if (MemoMenuIsEnable(0))
            MenuListAdd(list, 5, 0x3e);
        if (*ItemGetCnt(8) > 0)
            MenuListAdd(list, 6, 0x3f);
    }
    return list;
}

// FF3 0x001f19c0.

void StoryMenuCsrInit(void)
{
    story_menu_list = StoryMenuListCreate();
    MenuCsrInit(STORY_MENU_CSR, 1, StoryMenuGetRowNum(), 0);
    MenuCsrSetViewX(STORY_MENU_CSR, 1, 0);
    MenuCsrSetViewY(STORY_MENU_CSR, 8, 0);
}

// FF3 0x001f1a18.

void StoryMenuCsrRelease(void)
{
    MenuCsrRelease(STORY_MENU_CSR);
    MenuListDelete(story_menu_list);
    story_menu_list = nullptr;
    PlyrSetB260Mode2();
}

// FF3 0x001f1a40.

short StoryMenuGetRowId(short row)
{
    return MenuListGetId(story_menu_list, row);
}

// FF3 0x001f1a68.

short StoryMenuGetRowPhase(short row)
{
    return MenuListGetPhase(story_menu_list, row);
}

// FF3 0x001f1a90.

short StoryMenuGetRowNum(void)
{
    return MenuListGetNum(story_menu_list);
}

// FF3 0x001f1ab0.

short StoryMenuGetCsrRow(void)
{
    return (short)(MenuCsrGetPosY(STORY_MENU_CSR) + MenuCsrGetTopY(STORY_MENU_CSR));
}

// FF3 0x001f1af0.

void StoryMenuCsrUpdate(void)
{
    MenuCsrUpdate(STORY_MENU_CSR, 0);
}

// FF3 0x001f1b10.

int StoryMenuCsrMovedSe(void)
{
    return MenuCsrIsMovedSe(STORY_MENU_CSR, 0x1a9b0000);
}

// ──────────────────────────────────────────────────────────────────────
// Menu BGM
// ──────────────────────────────────────────────────────────────────────

// 0: keep the game's sound (a live enemy is fought, ingame_wrk.b03 == 1,
// or the menu opens straight on the map row); 2: menu BGM. FF3 has a
// mode 1 (sounds paused, no BGM) in StoryMenuBgmIn/Out that this never
// returns.
// FF3 0x001f1b30.

int StoryMenuGetBgmMode(void)
{
    if (EneIsAnyAlive() && PlyrIsBattle() && EneStreamIsActive())
        return 0;
    if (ingame_wrk.b03 == 1)
        return 0;
    return GetGPhaseCtrl()->now != 0x3a ? 2 : 0;
}

// FF3 0x001f1bb8.

void StoryMenuBgmIn(void)
{
    story_menu_bgm_mode = (short)StoryMenuGetBgmMode();
    switch (story_menu_bgm_mode)
    {
    case 2:
        SndBufAllPauseLoopSnd();
        StreamAutoSetExclusiveMode(1, GetPALMode() ? 25 : 30);
        SndBgmPlay(BGM_01_MENU1_STR_STR, 100, 0x1000, GetPALMode() ? 25 : 30, nullptr, 1, 0);
        // fall through
    case 1:
        EvSoundPause();
        break;
    }
}

// FF3 0x001f1c70.

void StoryMenuBgmOut(void)
{
    switch (story_menu_bgm_mode)
    {
    case 2:
        SndBgmStop(GetPALMode() ? 25 : 30, 0);
        EvSoundRestart();
        StreamAutoSetExclusiveMode(0, GetPALMode() ? 25 : 30);
        // fall through
    case 1:
        SndBufAllRestartLoopSnd();
        break;
    }
}

// ──────────────────────────────────────────────────────────────────────
// Phase 37, Story_Menu
// ──────────────────────────────────────────────────────────────────────

// The leaf node's state_cnt steps the way in:
//   0 capture the screen (the menu's backdrop), darken 0, stop the sky movie;
//   1 wait for the sky movie to stop, then the cursor, the return phase
//     (0x20, or the requester when it is 0x22 / 0x2d), the left stick as
//     d-pad, the backdrop (no layout when entered on row 0x3a / 0x3e) and
//     the menu BGM;
//   2 wait for the backdrop.
// FF3 0x001f1d08.

int init_Story_Menu(void)
{
    BpCtlMain();
    switch (GetGPhaseCtrl()->leaf->state_cnt)
    {
    case 0:
        ScreenCaptureSave(2, 1);
        MenuBgSetDarken(0);
        GetMapSky()->MovieStop();
        GetGPhaseCtrl()->leaf->state_cnt = 1;
        break;

    case 1:
        ScreenCaptureDisp(0x80, 0x80, 0x80, 0x80);
        GetMapSky()->MovieProc(0);
        if (GetMapSky()->MovieIsNotStopping())
        {
            short from;
            short now;

            pad_wrk[0].stick_dpad = 1;
            StoryMenuCsrInit();
            story_menu_ret_phase = 0x20;
            from = GetGPhaseCtrl()->from;
            if (from == 0x22 || from == 0x2d)
                story_menu_ret_phase = from;
            now = GetGPhaseCtrl()->now;
            MenuBgStart(ingame_wrk.b03 != 1 ? MENUBG_PHF : MENUBG_GENJITU_PHF,
                        now != 0x3a && now != 0x3e);
            StoryMenuBgmIn();
            GetGPhaseCtrl()->leaf->state_cnt = 2;
        }
        break;

    case 2:
        ScreenCaptureDisp(0x80, 0x80, 0x80, 0x80);
        return MenuBgIsReady(-1) != 0;
    }
    return 0;
}

// The way out: 0 the BGM back; 1 wait for the backdrop release; 2 cursor
// off, the stick back, the sky movie restarts.
// FF3 0x001f1f38.

int end_Story_Menu(void)
{
    BpCtlMain();
    switch (GetGPhaseCtrl()->leaf->state_cnt)
    {
    case 0:
        StoryMenuBgmOut();
        GetGPhaseCtrl()->leaf->state_cnt = 1;
        // fall through
    case 1:
        if (MenuBgRelease())
            GetGPhaseCtrl()->leaf->state_cnt = 2;
        break;

    case 2:
        StoryMenuCsrRelease();
        pad_wrk[0].stick_dpad = 0;
        GetMapSky()->MovieRestart();
        return 1;
    }
    return 0;
}

// The menu itself runs in the children: go straight to 57 (the top list).
// FF3 0x001f2038.

int one_Story_Menu(void)
{
    GPhaseCtrlRequest(0x39, 0);
    return 1;
}

// The captured screen under the darken rectangle, every frame.
// FF3 0x001f2060.

void pre_Story_Menu(void)
{
    ScreenCaptureDisp(0x80, 0x80, 0x80, 0x80);
    MenuBgDraw();
}

// FF3 0x001f2088.

void after_Story_Menu(void)
{
}

