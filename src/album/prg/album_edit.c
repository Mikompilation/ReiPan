/* ==========================================================================
 *  album/prg/album_edit.c
 *
 *  The album's phases and the helpers they share (FF3 0x1229a0..0x125318,
 *  FF3's own code in FF2 album_edit.c's slot; album_edit.h has the map).
 *  Data:
 *    - sdata 0x36b910..0x36b920: the key list's row, whether the help
 *      viewer is up, how the album was opened (from the title, over the
 *      outgame backdrop object, album_open_arg);
 *    - sbss 0x372c30..0x372c58: the key list rows, the confirm's default /
 *      answer / flags, the copy source, Album_Main's exit / return phase /
 *      state / pending help message, the init / end step and the phases'
 *      states, the menu and key masks, the help viewer's slot;
 *    - bss 0x390bf0: the order of each sort key (flips each sort).
 *  The page drawing is album_view.c's; the photo data album.c's; the
 *  cursors on the photo grid album_disp.c's.
 * ======================================================================== */
#include "album/prg/album_edit.h"

#include "album/prg/album.h"            // AlbumInfo*, AlbumPhoto*, AlbumEditInit / End, AlbumIngameFlag*
#include "album/prg/album_disp.h"       // album_edit_side / photo, album_sort_key, the grid / dst / sort cursors
#include "album/prg/album_phase.h"      // AlbumLoadResultSet / Get (phase 28)
#include "album/prg/album_view.h"       // AlbumView*, album_layout_obj
#include "common/cmn_fade_layer.h"      // GetCmnFadeLayer, capCtl
#include "graphics/graph2d/fade.h"      // FadeInReq, FadeOutReq, GetFadeState
#include "graphics/motion/anicode.h"    // MsgViewSetMode, MsgViewSetMsg
#include "ingame/map/MapLoad.h"         // MapLoadGetFreeArea
#include "ingame/menu/anim_2d.h"        // LayoutAnim*
#include "ingame/menu/menu_csr.h"       // MenuCsr*
#include "ingame/sound/snd_ctrl.h"      // SndSePlay
#include "main/gphase.h"
#include "main/gphase_tree.h"           // GPHASE_TREE::Reset
#include "outgame/outgame_bg.h"         // OutgameBg*
#include "system/b2d/BpCtl.h"           // BpArenaSet, BpCtlMain
#include "system/b2d/BpObj.h"           // BP_OBJ::GetHandle
#include "system/eeiop/cd_file_dat.h"   // ALBMICON0_TM2, ALBM0_TM2, OUTGAME_BG_PSS, BGM_03_MENU3_STR_STR
#include "system/eeiop/file_cache.h"    // file_cache
#include "system/pad/pad_ctrl.h"        // pad_key, CheckPadTrgPlaySe
#include "ingame/menu/menu_mouse.h"    // PC: the mouse in menus

#define ALBUM_MENU_CSR      0x1a        // MenuCsr ids: the menu rows (album_disp.c's sort list)
#define ALBUM_KEY_CSR       0x34        // the sort keys (Album_Sort)
#define ALBUM_CONF_CSR      0x4e        // yes / no (Album_Confirm)

#define ALBUM_SE_MOVE       0x1a9b0000
#define ALBUM_SE_CANCEL     0x1a9b0001
#define ALBUM_SE_BUZZ       0x1a9b0002
#define ALBUM_SE_OK         0x1a9b0003

#define ALBUM_MSG           0x59        // the album's message type

#define ALBUM_COVER_NUM     9           // album kinds 0..8: cover TM2s ALBMICON0 + kind, ALBM0 + kind
#define ALBUM_KIND_COVER_MAX 8          // covers a player picks: 1..8 (0 is the camera album)

#define ALBUM_ARENA_DEFAULT 0x8f8000    // the Bp arena when the map has no free buffer (EE)
#define ALBUM_ARENA_SIZE    0x345000

#define PHASE_TITLE_MENU    0x11        // phases (architecture.md "phases")
#define PHASE_ALBUM_MAIN    0x08
#define PHASE_ALBUM_OPEN    0x18
#define PHASE_ALBUM_COPY    0x19
#define PHASE_ALBUM_MENU    0x1a
#define PHASE_ALBUM_LOAD    0x1c
#define PHASE_ALBUM_HELP    0x36
#define PHASE_ALBUM_CONFIRM 0x4e
#define PHASE_START_MENU_MISSION_SAVE 0x73  // a return phase that runs LayoutAnimMain itself

#define ALBUM_HELP_FIRST    0x2d        // Album_Help messages (ALBUM_MSG ids): on opening,
#define ALBUM_HELP_EDIT     0x33        //   after Album_Open (also the viewer's default),
#define ALBUM_HELP_LEAVE    0x30        //   on cancel, before the leave confirm

// ──────────────────────────────────────────────────────────────────────
// Data
// ──────────────────────────────────────────────────────────────────────

s_char album_key_sel;                   // sdata 0x36b910
static int album_help_open;             // sdata 0x36b914: Album_Help was opened by Album_Main
static int album_from_title;            // sdata 0x36b918: opened from the title menu
static int album_bg_obj_used;           // sdata 0x36b91c: opened over the outgame backdrop object
int    album_open_arg;                  // sdata 0x36b920 (AlbumSetIngame, album.c)

// The key list's rows (sbss 0x372c30). FF3 sizes it 3 but AlbumKeyCsrInit
// writes five, so rows 3 / 4 land on album_conf_default / album_conf_pos
// (0x372c33 / 0x372c34) as -1; both are set again before anything reads
// them, so the host keeps five rows of its own.
static s_char album_key_list[ALBUM_KEY_ROW_NUM];
s_char album_conf_default;              // sbss 0x372c33
s_char album_conf_pos;                  // sbss 0x372c34
static u_char album_conf_flag;          // sbss 0x372c35: ALBUM_CONF_*
static s_char album_copy_side;          // sbss 0x372c38: the copy source
static s_char album_copy_photo;         // sbss 0x372c39
static int    album_main_exit;          // sbss 0x372c3c: leave to album_main_return
static int    album_main_return;        // sbss 0x372c40: the phase the album came from
static s_char album_main_state;         // sbss 0x372c44: Album_Main: 0 open, 1 edit, 2 cancel, 3 confirm answered
static int    album_help_msg;           // sbss 0x372c48: help message for Album_Help, -1 none
static int    album_bg_started;         // sbss 0x372c4c: OUTGAME_BG_PSS started
static s_char album_main_step;          // sbss 0x372c50: init / end step
static s_char album_open_state;         // sbss 0x372c51: Album_Open
static s_char album_copy_state;         // sbss 0x372c52: Album_Copy: 0 cursor, 1 overwrite asked
static s_char album_menu_state;         // sbss 0x372c53: Album_Menu
static u_char album_menu_mask;          // sbss 0x372c54: enabled menu rows
static u_char album_key_mask;           // sbss 0x372c55: enabled sort keys
static int    album_help_anim;          // sbss 0x372c58: Album_Help's LayoutAnim slot

#define ALBUM_SORT_KEY_ORDER_NUM 3      // AlbumSortPhoto keys: time, score, protect
static int album_sort_order[ALBUM_SORT_KEY_ORDER_NUM];  // bss 0x390bf0

// The help lines: ALBUM_MSG ids for message slots 0..2, -1 none (data 0x34dd48).
#define ALBUM_GUIDE_NUM 17
static const int album_edit_guide[ALBUM_GUIDE_NUM][ALBUM_VIEW_MSG_NUM] =
{
    { 13, 11, 12 }, { 14, 11, 12 }, { 15, 11, 12 }, { 16, -1, -1 },     //  0..2 Album_Open per side, 3 the cover
    { 22, 20, 21 }, { 23, -1, -1 }, { 24, 20, 21 }, { 25, -1, -1 },     //  4 overwrite?, 5 protected, 6 erase?, 7 protected
    { 26, 20, 21 }, { 27, -1, -1 }, { 28, -1, -1 }, { 29, -1, -1 },     //  8 leave?, 9..16 the menu rows
    { 30, -1, -1 }, { 31, -1, -1 }, { 32, -1, -1 }, { 33, 20, 21 },     // 15 load over this album?
    { 34, -1, -1 },
};

// The guide of each menu row per side (FF3 builds it on the stack).
static const s_char album_menu_guide[ALBUM_SIDE_NUM][ALBUM_KEY_ROW_NUM] =
{
    { 9, 10, 11, 13, 16 },
    { 9, 10, 12, 14, 16 },
};

// The phase of each menu row (data 0x34de18): the zoom page, Album_Sort,
// the card save, the card load, Album_Help.
static const int album_menu_phase[ALBUM_KEY_ROW_NUM] = { 0x35, 0x34, 0x1b, 0x1c, 0x36 };

// ──────────────────────────────────────────────────────────────────────
// Cursors
// ──────────────────────────────────────────────────────────────────────

// Keep the menu row under the cursor for AlbumSortCsrRestore (before the
// card screens take the screen) and release the list.
// FF3 0x001229a0.
void AlbumSortCsrSave(void)
{
    album_sort_key_req = -1;
    if (album_sort_key >= 0)
    {
        album_sort_key_req = album_sort_list[MenuCsrGetPos(ALBUM_MENU_CSR)];
        MenuCsrRelease(ALBUM_MENU_CSR);
    }
}

// A one-column list of the set bits of mask; mask 0: no list.
// FF3 0x001229f0.
void AlbumKeyCsrInit(int id, u_char mask)
{
    s_char num = 0;
    s_char i;

    if (mask == 0)
    {
        album_key_sel = -1;
        return;
    }
    for (i = 0; i < ALBUM_KEY_ROW_NUM; i++)
    {
        album_key_list[i] = -1;
        if ((mask >> i) & 1)
            album_key_list[num++] = i;
    }
    MenuCsrInit(id, 1, num, 0);
    album_key_sel = album_key_list[MenuCsrGetPos(id)];
}

// FF3 0x00122aa8.
int AlbumKeyCsrUpdate(int id)
{
    if (album_key_sel < 0)
        return 0;

    // PC: the mouse. The sprite stands on the row of the key.
    {
        ReiPan_MenuBox at;

        if (AlbumViewMouseSortBox(&at))
        {
            for (int i = 0; i < ALBUM_KEY_ROW_NUM && album_key_list[i] >= 0; i++)
            {
                ReiPan_MenuBox box = at;

                box.y += (float)(album_key_list[i] - album_key_sel) * ALBUM_VIEW_MOUSE_ROW_DY;
                MenuMouseCsr(id, i, &box);
            }
        }
    }

    MenuCsrUpdate(id, 0);
    if (!MenuCsrIsMovedSe(id, ALBUM_SE_MOVE))
        return 0;

    album_key_sel = album_key_list[MenuCsrGetPos(id)];
    return 1;
}

// FF3 0x00122b20.
void AlbumKeyCsrRelease(int id)
{
    if (album_key_sel >= 0)
        MenuCsrRelease(id);
}

// ──────────────────────────────────────────────────────────────────────
// Help lines and the confirm
// ──────────────────────────────────────────────────────────────────────

// FF3 0x00122b50.
void AlbumEditGuideSet(int guide)
{
    const int *ids = album_edit_guide[(s_char)guide];

    AlbumViewMsgSet(0, ALBUM_MSG, ids[0]);
    AlbumViewMsgSet(1, ALBUM_MSG, ids[1]);
    AlbumViewMsgSet(2, ALBUM_MSG, ids[2]);
    AlbumViewShowItem45a(1);
}

// FF3 0x00122bc0.
void AlbumEditGuideClear(void)
{
    AlbumViewMsgSet(0, -1, -1);
    AlbumViewMsgSet(1, -1, -1);
    AlbumViewMsgSet(2, -1, -1);
    AlbumViewShowCopyItem(1);
}

// FF3 0x00122c08.
void AlbumEditGuideSetMenu(void)
{
    AlbumEditGuideSet(album_menu_guide[AlbumDispGetCurAlbum()][album_sort_key]);
}

// Open Album_Confirm as a child of the calling phase: the guide's message
// and, unless def is -1, a yes / no cursor starting on def.
// FF3 0x00122c68.
void AlbumEditConfirmReq(int guide, int def)
{
    album_conf_default = (s_char)def;
    GetGPhaseCtrl()->SetNext(PHASE_ALBUM_CONFIRM, 1);
    AlbumViewSetFadeLayer(PHASE_ALBUM_CONFIRM);
    AlbumEditGuideSet(guide);
}

// FF3 0x00122cc8.
void AlbumConfCsrInit(void)
{
    MenuCsrInit(ALBUM_CONF_CSR, 2, 1, 0);
    album_conf_pos = (s_char)MenuCsrSetPos(ALBUM_CONF_CSR, album_conf_default);
}

// FF3 0x00122d00.
int AlbumConfCsrUpdate(void)
{
    int moved;

    // PC: the mouse.
    for (int pos = 0; pos < 2; pos++)
    {
        ReiPan_MenuBox box;

        if (AlbumViewMouseConfBox(pos, &box))
            MenuMouseCsr(ALBUM_CONF_CSR, pos, &box);
    }
    MenuCsrUpdate(ALBUM_CONF_CSR, 0);
    moved = MenuCsrIsMovedSe(ALBUM_CONF_CSR, ALBUM_SE_MOVE);
    if (moved)
        album_conf_pos = (s_char)MenuCsrGetPos(ALBUM_CONF_CSR);
    return moved != 0;
}

// FF3 0x00122d50.
void AlbumConfCsrRelease(void)
{
    MenuCsrRelease(ALBUM_CONF_CSR);
}

// ──────────────────────────────────────────────────────────────────────
// The cover and the confirm flags
// ──────────────────────────────────────────────────────────────────────

// The cover (kind 1..8, wrapping) of the side under the grid cursor: L1 /
// R1 when shoulder, else left / right. Never on the camera album.
// FF3 0x00122d68.
int AlbumEditCoverPad(int shoulder)
{
    s_char kind;
    s_char next;

    if (AlbumInfoIsCamera(album_edit_side))
        return 0;

    kind = AlbumInfoGetKind(album_edit_side);
    if (CheckPadTrgPlaySe(shoulder != 0 ? 0x0004 : 0x8000, ALBUM_SE_MOVE, 0))     // L1 / left
    {
        next = (s_char)(kind - 1);
        if (kind < 2)
            next = ALBUM_KIND_COVER_MAX;
    }
    else if (CheckPadTrgPlaySe(shoulder != 0 ? 0x0008 : 0x2000, ALBUM_SE_MOVE, 0)) // R1 / right
    {
        if (kind < ALBUM_KIND_COVER_MAX)
            next = (s_char)(kind + 1);
        else
            next = 1;
    }
    else
    {
        return 0;
    }
    AlbumInfoSetKind(album_edit_side, next);
    return 1;
}

// FF3 0x00122e40.
void AlbumEditConfFlagClearAll(void)
{
    album_conf_flag = 0;
}

// FF3 0x00122e48.
void AlbumEditConfFlagSet(u_char bits)
{
    album_conf_flag |= bits;
}

// FF3 0x00122e60.
void AlbumEditConfFlagClear(u_char bits)
{
    album_conf_flag &= (u_char)~bits;
}

// FF3 0x00122e78.
int AlbumEditConfFlagIsSet(u_char bits)
{
    return (album_conf_flag & bits) != 0;
}

// ──────────────────────────────────────────────────────────────────────
// Copy, erase, sort
// ──────────────────────────────────────────────────────────────────────

// FF3 0x00122e90.
int AlbumEditCurIsUsed(void)
{
    s_char side = AlbumDispGetCurAlbum();
    s_char no   = AlbumDispGetCurPhoto();

    return AlbumPhotoIsUsed(side, no) != 0;
}

// FF3 0x00122ec8.
int AlbumEditCopySrcSet(void)
{
    int used = AlbumEditCurIsUsed();

    if (used)
    {
        album_copy_side  = AlbumDispGetCurAlbum();
        album_copy_photo = AlbumDispGetCurPhoto();
        SndSePlay(ALBUM_SE_OK, 100, 0x1000, 0, nullptr, 0);
    }
    else
    {
        SndSePlay(ALBUM_SE_BUZZ, 100, 0x1000, 0, nullptr, 0);
    }
    return used != 0;
}

// FF3 0x00122f50.
int AlbumEditPasteCheck(void)
{
    s_char side = AlbumDispGetCurAlbum();
    s_char no   = AlbumDispGetCurPhoto();

    if (AlbumPhotoIsProtected(side, no))
    {
        SndSePlay(ALBUM_SE_BUZZ, 100, 0x1000, 0, nullptr, 0);
        return -1;
    }
    if (AlbumPhotoIsUsed(side, no))
    {
        SndSePlay(ALBUM_SE_OK, 100, 0x1000, 0, nullptr, 0);
        return 0;
    }
    SndSePlay(ALBUM_SE_OK, 100, 0x1000, 0, nullptr, 0);
    return 1;
}

// FF3 0x00123020.
int AlbumEditPasteExec(void)
{
    s_char src_photo = album_copy_photo;
    s_char src_side  = album_copy_side;
    s_char side      = AlbumDispGetCurAlbum();
    s_char no        = AlbumDispGetCurPhoto();

    return AlbumPhotoCopy(src_side, src_photo, side, no) != 0;
}

// FF3 0x00123080.
int AlbumEditEraseCheck(void)
{
    s_char side = AlbumDispGetCurAlbum();
    s_char no   = AlbumDispGetCurPhoto();

    if (!AlbumPhotoIsUsed(side, no))
    {
        SndSePlay(ALBUM_SE_BUZZ, 100, 0x1000, 0, nullptr, 0);
        return 0;
    }
    if (AlbumPhotoIsProtected(side, no))
    {
        SndSePlay(ALBUM_SE_BUZZ, 100, 0x1000, 0, nullptr, 0);
        return -1;
    }
    SndSePlay(ALBUM_SE_OK, 100, 0x1000, 0, nullptr, 0);
    return 1;
}

// FF3 0x00123150.
int AlbumEditEraseReq(void)
{
    return AlbumEditEraseCheck();
}

// FF3 0x00123170.
int AlbumEditEraseExec(void)
{
    s_char side = AlbumDispGetCurAlbum();
    s_char no   = AlbumDispGetCurPhoto();

    return AlbumPhotoErase(side, no) != 0;
}

// FF3 0x001231a8.
void AlbumEditSortOrderReset(void)
{
    for (s_char i = 0; i < ALBUM_SORT_KEY_ORDER_NUM; i++)
        album_sort_order[i] = 0;
}

// Sort the side by key in the key's order; a sort flips the order for the
// next time and re-shows the photo under the cursor.
// FF3 0x001231d8.
int AlbumEditSort(s_char key)
{
    s_char side   = AlbumDispGetCurAlbum();
    s_char no     = AlbumDispGetCurPhoto();
    int   *order  = &album_sort_order[key];
    int    sorted = AlbumSortPhoto(side, key, *order);

    if (sorted)
    {
        *order ^= 1;
        AlbumDispSetPhoto(side, no);
    }
    return sorted != 0;
}

// ──────────────────────────────────────────────────────────────────────
// The menu rows' tests
// ──────────────────────────────────────────────────────────────────────

// FF3 0x00123268.
int AlbumMenuCondHasPhoto(void)
{
    return AlbumGetPhotoNum(AlbumDispGetCurAlbum()) != 0;
}

// The three sort keys when the side has photos.
// FF3 0x00123290.
int AlbumMenuCondSortKeys(void)
{
    u_char mask = 0;

    if (AlbumGetPhotoNum(AlbumDispGetCurAlbum()) != 0)
    {
        for (s_char i = 0; i < ALBUM_SORT_KEY_ORDER_NUM; i++)
            mask |= (u_char)(1 << i);
    }
    return mask;
}

// FF3 0x001232f0.
int AlbumMenuCondSave(void)
{
    s_char side = AlbumDispGetCurAlbum();
    u_char num  = AlbumGetPhotoNum(side);

    return !AlbumInfoIsCamera(side) && num != 0;
}

// FF3 0x00123340.
int AlbumMenuCondLoad(void)
{
    return AlbumInfoIsCamera(AlbumDispGetCurAlbum()) == 0;
}

// FF3 0x00123368.
int AlbumMenuCondHelp(void)
{
    s_char side = AlbumDispGetCurAlbum();
    u_char num  = AlbumGetPhotoNum(side);

    return !AlbumInfoIsCamera(side) || num != 0;
}

// The row tests in menu order (data 0x34dd30; FF3 copies the list to the
// stack). Row 1's test is the key mask, non-zero when any key is on.
typedef int (*ALBUM_MENU_COND)(void);
static const ALBUM_MENU_COND album_menu_cond[ALBUM_KEY_ROW_NUM] =
{
    AlbumMenuCondHasPhoto, AlbumMenuCondSortKeys, AlbumMenuCondSave, AlbumMenuCondLoad, AlbumMenuCondHelp,
};

// FF3 0x001233c0.
// callcheck: AlbumMenuCondHasPhoto, AlbumMenuCondSortKeys, AlbumMenuCondSave, AlbumMenuCondLoad, AlbumMenuCondHelp (album_menu_cond)
u_char AlbumMenuGetRowMask(void)
{
    u_char mask = 0;

    for (s_char i = 0; i < ALBUM_KEY_ROW_NUM; i++)
    {
        if (album_menu_cond[i]() != 0)
            mask |= (u_char)(1 << i);
    }
    return mask;
}

// FF3 0x00123478.
int AlbumGetOpenArg(void)
{
    return album_open_arg;
}

// ──────────────────────────────────────────────────────────────────────
// Phase 8, Album_Main
// ──────────────────────────────────────────────────────────────────────

// The first loading step: the cover TM2s when album_open_arg asks for
// them, else the backdrop movie from the title, else the layouts.
static s_char AlbumMainFirstLoadStep(void)
{
    if (album_open_arg != 0)
        return 4;
    return album_from_title != 0 ? 6 : 8;
}

// Load the album: Bp arena on the map's free buffer, AlbumEditInit, then
// steps: 0..3 fade out and show the backdrop object, 4..5 the cover TM2s
// into the file cache, 6..7 the backdrop movie (from the title), 8..9 the
// album layouts and the menu BGM, 10..12 the backdrop object again, 13..14
// the grid cursor and the page "in".
// FF3 0x00123480.
// callcheck: FileCacheReq (file_cache.Req), FileCachePollAll (file_cache.PollAll), FileCacheIsLoaded (file_cache.IsLoaded)
int init_Album_Main(void)
{
    GPHASE_CTRL *ctrl = GetGPhaseCtrl();

    if (ctrl->leaf->state_cnt == 0)
    {
        u_int area = MapLoadGetFreeArea();

        if (area == 0)
            area = ALBUM_ARENA_DEFAULT;
        BpArenaSet(area, ALBUM_ARENA_SIZE);
        AlbumEditInit();
        album_bg_started  = 0;
        album_from_title  = 0;
        album_bg_obj_used = 0;
        if (GetGPhaseCtrl()->from == PHASE_TITLE_MENU)
            album_from_title = 1;
        else if (OutgameBgObjIsUsed())
            album_bg_obj_used = 1;

        if (album_bg_obj_used == 0)
            album_main_step = AlbumMainFirstLoadStep();
        else
            album_main_step = 0;
        GetGPhaseCtrl()->leaf->state_cnt = 1;
    }

    if (GetGPhaseCtrl()->leaf->state_cnt != 1)
        return 0;

    if (album_main_step == 0)
    {
        FadeOutReq(0, 0, 0, 10);
        album_main_step = 1;
    }
    if (album_main_step == 1 && GetFadeState())
    {
        OutgameBgObjShow();
        album_main_step = 2;
    }
    if (album_main_step == 2 && OutgameBgObjIsShown())
        album_main_step = 3;
    if (album_main_step == 3)
        album_main_step = AlbumMainFirstLoadStep();

    if (album_main_step == 4)
    {
        for (s_char i = 0; i < ALBUM_COVER_NUM; i++)
        {
            file_cache.Req(ALBM0_TM2 + i);
            file_cache.Req(ALBMICON0_TM2 + i);
        }
        album_main_step = 5;
    }
    if (album_main_step == 5)
    {
        int all = 1;

        file_cache.PollAll();
        for (s_char i = 0; i < ALBUM_COVER_NUM; i++)
        {
            if (!file_cache.IsLoaded(ALBM0_TM2 + i))
                all = 0;
            if (!file_cache.IsLoaded(ALBMICON0_TM2 + i))
                all = 0;
        }
        if (all)
            album_main_step = album_from_title != 0 ? 6 : 8;
    }

    if (album_main_step == 6)
    {
        OutgameBgStart(0, OUTGAME_BG_PSS, -1);
        album_bg_started = 1;
        album_main_step  = 7;
    }
    if (album_main_step == 7 && OutgameBgIsLoaded() && OutgameBgLoadWait())
        album_main_step = 8;

    if (album_main_step == 8)
    {
        AlbumViewLayoutCreate();
        album_main_step = 9;
    }
    if (album_main_step == 9 && AlbumViewLayoutLoad())
    {
        OutgameBgmPlay(BGM_03_MENU3_STR_STR);
        album_main_step = 10;
        if (album_from_title == 0)
            album_main_step = 13;
        if (album_bg_obj_used != 0)
            FadeInReq(0, 0, 0, 10);
    }

    if (album_main_step == 10 && OutgameBgObjIsReady())
    {
        OutgameBgDraw();
        OutgameBgObjShow();
        album_main_step = 11;
    }
    if (album_main_step == 11 && OutgameBgObjIsShown())
        album_main_step = 12;
    if (album_main_step == 12)
        album_main_step = 13;

    if (album_main_step == 13)
    {
        AlbumEditCsrInit();
        OutgameBgEnable();
        AlbumViewLayoutIn(album_open_arg);
        album_main_step = 14;
    }
    if (album_main_step == 14)
    {
        int ready = 1;

        if (album_from_title != 0)
            ready = OutgameBgIsReady() != 0;
        if (!AlbumViewLayoutIsInEnd())
            ready = 0;
        if (ready)
            album_main_step = 15;
    }

    if (album_main_step != 15)
        return 0;

    album_main_exit   = 0;
    album_main_return = GetGPhaseCtrl()->from;
    album_help_msg    = ALBUM_HELP_FIRST;
    album_main_state  = 0;
    album_help_open   = 0;
    AlbumEditSortOrderReset();
    AlbumEditConfFlagClearAll();
    AlbumViewMsgInit();
    return 1;
}

// Release the album: BGM and backdrop off, the layouts "out" and released,
// AlbumEditEnd, the backdrop and cover TM2s released, and over the
// backdrop object a fade back to it.
// FF3 0x001239c8.
// callcheck: FileCacheRelease (file_cache.Release)
int end_Album_Main(void)
{
    if (GetGPhaseCtrl()->leaf->state_cnt == 0)
    {
        GetGPhaseCtrl()->leaf->state_cnt = 1;
        if (album_main_return != PHASE_START_MENU_MISSION_SAVE)
            OutgameBgDisable();
        if (album_from_title == 0)
            OutgameBgmStop();
        album_main_step = (album_from_title == 0);
        if (album_bg_obj_used != 0)
            FadeOutReq(0, 0, 0, 10);
    }

    if (GetGPhaseCtrl()->leaf->state_cnt != 1)
        return 0;

    if (album_main_step == 0)
    {
        OutgameBgmStop();
        OutgameBgStop();
        album_main_step = 1;
    }
    if (album_main_step == 1)
    {
        int done = 1;

        if (album_from_title != 0)
            done = OutgameBgIsStopped() != 0;
        AlbumViewUpdatePages(1, 0);
        if (!AlbumViewLayoutOut())
            done = 0;
        if (done)
        {
            AlbumViewLayoutRelease();
            AlbumEditCsrRelease();
            AlbumEditEnd();
            album_main_step = 2;
            if (album_from_title == 0)
                album_main_step = album_open_arg != 0 ? 4 : 5;
        }
    }
    if (album_main_step == 2)
        album_main_step = 3;
    if (album_main_step == 3)
    {
        OutgameBgRelease();
        album_bg_started = 0;
        album_main_step  = album_open_arg != 0 ? 4 : 5;
    }
    if (album_main_step == 4)
    {
        for (s_char i = 0; i < ALBUM_COVER_NUM; i++)
        {
            file_cache.Release(ALBMICON0_TM2 + i);
            file_cache.Release(ALBM0_TM2 + i);
        }
        album_main_step = 5;
    }
    if (album_main_step == 5)
        album_main_step = album_bg_obj_used != 0 ? 6 : 9;
    if (album_main_step == 6 && GetFadeState())
    {
        OutgameBgObjHide();
        album_main_step = 7;
    }
    if (album_main_step == 7 && OutgameBgObjIsReady())
    {
        FadeInReq(0, 0, 0, 10);
        album_main_step = 8;
    }
    if (album_main_step == 8 && GetFadeState())
        album_main_step = 9;

    return album_main_step == 9;
}

// The edit page. First Album_Open (phase 24); a pending help message opens
// Album_Help (54) in game until the album has once been left; then the
// cover (L1 / R1), the grid, OK copy (25), circle erase (confirm 6, or 7 on
// a protected photo), square the menu (26), cancel the leave confirm (8).
// FF3 0x00123ce0.
int one_Album_Main(void)
{
    int r;

    if (album_main_exit != 0)
    {
        AlbumViewPageSetOn(0, 0);
        AlbumViewPageSetOn(1, 0);
        GetGPhaseCtrl()->SetNext(album_main_return, 0);
        return 1;
    }

    if (album_help_msg != -1)
    {
        if (album_help_open == 0)
        {
            if (AlbumIngameFlagGet() == 0)
            {
                album_help_open = 1;
                GetGPhaseCtrl()->SetNext(PHASE_ALBUM_HELP, 1);
                AlbumViewSetFadeLayer(PHASE_ALBUM_HELP);
                return 1;
            }
        }
        else
        {
            album_help_open = 0;
            GetGPhaseTree()->Reset();
        }
        album_help_msg = -1;
    }

    if (album_main_state == 0)
    {
        album_main_state = 1;
        album_help_msg   = ALBUM_HELP_EDIT;
        GetGPhaseCtrl()->SetNext(PHASE_ALBUM_OPEN, 0);
        AlbumViewSetFadeLayer(PHASE_ALBUM_OPEN);
        return 1;
    }

    AlbumViewUpdatePages(1, 0);

    if (album_main_state == 1)
    {
        AlbumViewPageCsrFreeze(0, 1);
        AlbumViewPageCsrFreeze(1, 1);
        if (AlbumEditCoverPad(1))
        {
        }
        else if (AlbumEditCsrUpdate(0))
        {
            AlbumViewSelectSlot(album_edit_side, album_edit_photo);
        }
        else if (CheckPadTrgPlaySe((u_short)pad_key[PAD_KEY_OK], -1, 0))
        {
            if (AlbumEditCopySrcSet())
            {
                GetGPhaseCtrl()->SetNext(PHASE_ALBUM_COPY, 0);
                AlbumViewSetFadeLayer(PHASE_ALBUM_COPY);
                return 1;
            }
        }
        else if (CheckPadTrgPlaySe(0x0020, -1, 0))                  // circle
        {
            r = AlbumEditEraseReq();
            if (r != 0)
            {
                if (r == 1)
                {
                    AlbumEditConfFlagSet(ALBUM_CONF_ERASE);
                    AlbumEditConfirmReq(6, 1);
                    album_main_state = 3;
                    return 1;
                }
                AlbumEditConfirmReq(7, -1);
                return 1;
            }
        }
        else if (CheckPadTrgPlaySe(0x0080, ALBUM_SE_OK, 0))         // square
        {
            GetGPhaseCtrl()->SetNext(PHASE_ALBUM_MENU, 0);
            AlbumViewSetFadeLayer(PHASE_ALBUM_MENU);
            return 1;
        }
        else if (CheckPadTrgPlaySe((u_short)pad_key[PAD_KEY_CANCEL], ALBUM_SE_CANCEL, 0))
        {
            // FF3 returns 1 without a SetNext here: the help line comes first.
            album_main_state = 2;
            album_help_msg   = ALBUM_HELP_LEAVE;
            return 1;
        }
    }

    if (album_main_state == 2)
    {
        album_main_state = 3;
        AlbumIngameFlagSet();
        AlbumEditConfirmReq(8, 1);
        return 1;
    }
    if (album_main_state != 3)
        return 0;

    // Back from Album_Confirm.
    if (AlbumEditConfFlagIsSet(ALBUM_CONF_ERASE))
    {
        if (album_conf_pos == 0)
            AlbumEditEraseExec();
        AlbumEditConfFlagClear(ALBUM_CONF_ERASE);
    }
    else if (album_conf_pos == 0)
    {
        album_main_exit = 1;
        return 0;
    }
    album_main_state = 1;
    AlbumEditGuideClear();
    return 0;
}

// FF3 0x00124050.
void pre_Album_Main(void)
{
    if (album_from_title != 0 && album_bg_started != 0)
        OutgameBgDraw();
    AlbumViewMsgStep();
}

// From the title the album runs the common fade layer and the Bp entries
// itself.
// FF3 0x00124080.
// callcheck: CmnFadeLayerUpdate (capCtl::Update)
void after_Album_Main(void)
{
    if (album_from_title != 0)
    {
        GetCmnFadeLayer()->Update();
        BpCtlMain();
    }
}

// ──────────────────────────────────────────────────────────────────────
// Phase 24, Album_Open
// ──────────────────────────────────────────────────────────────────────

// Side 1 first in game (side 0 is the camera album there), side 0 first
// from the title.
// FF3 0x001240b8.
int init_Album_Open(void)
{
    album_open_state = 0;
    album_edit_side  = AlbumIsIngame() != 0;
    return 1;
}

// FF3 0x001240e8.
int end_Album_Open(void)
{
    return 1;
}

// Per side: confirm (guide 0 in game, else 1 / 2 by side) yes -> a new
// album, choose its cover (left / right, OK); no -> the card load
// (phase 28); cancel leaves the album from side 0 or in game, else goes
// back to side 0. When both sides are set, the grid cursor goes to the
// first photo and the edit page starts.
// FF3 0x001240f0.
int one_Album_Open(void)
{
    s_char side;
    int    no;

    if (album_open_state == 0)
    {
        AlbumViewSelectPage(album_edit_side);
        AlbumViewArrowSetDisp(album_edit_side, 0);
        AlbumEditConfirmReq(AlbumIsIngame() != 0 ? 0 : (s_char)(album_edit_side + 1), 0);
        album_open_state = 1;
        AlbumInfoSetKind(album_edit_side, -1);
        return 1;
    }

    AlbumViewUpdatePages(1, 0);

    if (album_open_state == 1)
    {
        if (album_conf_pos == 0)
        {
            AlbumViewArrowSetDisp(album_edit_side, 1);
            album_open_state = 2;
            AlbumInfoSetKind(album_edit_side, 1);
            AlbumEditGuideSet(3);
        }
        else if (album_conf_pos == 1)
        {
            album_open_state = 3;
        }
        else
        {
            if (album_edit_side == 0 || AlbumIsIngame())
            {
                AlbumViewArrowSetDisp(album_edit_side, 0);
                album_main_exit = 1;
                GetGPhaseCtrl()->SetNext(PHASE_ALBUM_MAIN, 0);
                return 1;
            }
            album_edit_side  = 0;
            album_open_state = 0;
        }
    }

    if (album_open_state == 2 && !AlbumEditCoverPad(0))
    {
        if (CheckPadTrgPlaySe((u_short)pad_key[PAD_KEY_OK], ALBUM_SE_OK, 0))
            album_open_state = 5;
        else if (CheckPadTrgPlaySe((u_short)pad_key[PAD_KEY_CANCEL], ALBUM_SE_CANCEL, 0))
            album_open_state = 0;
    }

    if (album_open_state == 3)
    {
        AlbumViewPageSetOn(0, 0);
        AlbumViewPageSetOn(1, 0);
        if (AlbumViewPlayLoad(0))
        {
            AlbumLoadResultSet(0);
            album_open_state = 4;
            GetGPhaseCtrl()->SetNext(PHASE_ALBUM_LOAD, 1);
            AlbumViewSetFadeLayer(PHASE_ALBUM_LOAD);
            AlbumViewSetMsgVariant(0);
            return 1;
        }
    }

    if (album_open_state == 4)
    {
        // Back from the card load.
        GetGPhaseTree()->Reset();
        AlbumViewPageSetOn(0, 1);
        AlbumViewPageSetOn(1, 1);
        if (AlbumViewPlayLoad(1))
            album_open_state = AlbumLoadResultGet() != 0 ? 5 : 0;
    }

    if (album_open_state != 5)
        return 0;

    side = album_edit_side;
    if (side != 1)
    {
        album_edit_side  = 1;
        album_open_state = 0;
        return 0;
    }

    no = AlbumSearchUsedPhoto(0);
    if (no == -1)
    {
        no = AlbumSearchUsedPhoto(1);
        if (no != -1)
        {
            album_edit_photo = (s_char)no;
            album_edit_side  = side;
        }
    }
    else
    {
        album_edit_photo = (s_char)no;
        album_edit_side  = 0;
    }
    AlbumViewSelectSlot(album_edit_side, album_edit_photo);
    AlbumViewPageCsrSetOn(album_edit_side, 1);
    GetGPhaseCtrl()->SetNext(PHASE_ALBUM_MAIN, 0);
    AlbumViewSetFadeLayer(PHASE_ALBUM_MAIN);
    AlbumEditGuideClear();
    return 1;
}

// ──────────────────────────────────────────────────────────────────────
// Phase 25, Album_Copy
// ──────────────────────────────────────────────────────────────────────

// FF3 0x00124490.
int init_Album_Copy(void)
{
    AlbumEditConfFlagSet(ALBUM_CONF_COPY);
    AlbumViewPageCsrSetGray(AlbumDispGetCurAlbum(), 0x40);
    AlbumViewArrowSetDisp(AlbumDispGetCurAlbum(), 0);
    AlbumDstCsrInit();
    AlbumViewPageCsrSetPos(AlbumDispGetCurAlbum(), AlbumDispGetCurPhoto());
    album_copy_state = 0;
    return 1;
}

// FF3 0x00124508.
int end_Album_Copy(void)
{
    AlbumDstCsrRelease();
    AlbumViewPageCsrSetGray(AlbumDispGetCurAlbum(), 0x80);
    AlbumViewArrowSetDisp(AlbumDispGetCurAlbum(), 1);
    AlbumViewSelectSlot(AlbumDispGetCurAlbum(), AlbumDispGetCurPhoto());
    AlbumEditConfFlagClear(ALBUM_CONF_COPY);
    AlbumEditGuideClear();
    return 1;
}

// The destination cursor; OK pastes into a free slot at once, asks
// (guide 4) over a used one, refuses (guide 5) a protected one; cancel
// goes back without copying.
// FF3 0x00124580.
int one_Album_Copy(void)
{
    int r;

    AlbumViewUpdatePages(1, 0);
    if (album_copy_state == 0)
    {
        if (AlbumDstCsrUpdate())
        {
            AlbumViewPageCsrSetPos(AlbumDispGetCurAlbum(), AlbumDispGetCurPhoto());
            return 0;
        }
        if (!CheckPadTrgPlaySe((u_short)pad_key[PAD_KEY_OK], -1, 0))
        {
            if (!CheckPadTrgPlaySe((u_short)pad_key[PAD_KEY_CANCEL], ALBUM_SE_CANCEL, 0))
                return 0;
            GetGPhaseCtrl()->SetNext(PHASE_ALBUM_MAIN, 0);
            AlbumViewSetFadeLayer(PHASE_ALBUM_MAIN);
            return 1;
        }
        r = AlbumEditPasteCheck();
        if (r == 0)
        {
            album_copy_state = 1;
            AlbumEditConfirmReq(4, 1);
            return 1;
        }
        if (r != 1)
        {
            AlbumEditConfirmReq(5, -1);
            return 1;
        }
    }
    else
    {
        if (album_copy_state != 1)
            return 0;
        // Back from the overwrite confirm.
        if (album_conf_pos != 0)
        {
            album_copy_state = 0;
            AlbumEditGuideClear();
            return 0;
        }
    }

    AlbumEditPasteExec();
    GetGPhaseCtrl()->SetNext(PHASE_ALBUM_MAIN, 0);
    AlbumViewSetFadeLayer(PHASE_ALBUM_MAIN);
    return 1;
}

// ──────────────────────────────────────────────────────────────────────
// Phase 26, Album_Menu
// ──────────────────────────────────────────────────────────────────────

// The row under the menu cursor: -1 (buzz) when no row is on, 0 (buzz)
// when it is off, 1 to run it.
// FF3 0x001246d8.
static int AlbumMenuRowCheck(void)
{
    if (album_menu_mask == 0)
    {
        SndSePlay(ALBUM_SE_BUZZ, 100, 0x1000, 0, nullptr, 0);
        return -1;
    }
    if (((album_menu_mask >> album_sort_key) & 1) == 0)
    {
        SndSePlay(ALBUM_SE_BUZZ, 100, 0x1000, 0, nullptr, 0);
        return 0;
    }
    SndSePlay(ALBUM_SE_OK, 100, 0x1000, 0, nullptr, 0);
    return 1;
}

// FF3 0x00124778.
int init_Album_Menu(void)
{
    s_char side = AlbumDispGetCurAlbum();

    if (GetGPhaseCtrl()->leaf->state_cnt == 0)
    {
        u_char mask;

        album_menu_state = 0;
        AlbumViewPageCsrFreeze(0, 0);
        AlbumViewPageCsrFreeze(1, 0);
        AlbumViewArrowSetDisp(side, 0);
        mask = AlbumMenuGetRowMask();
        album_menu_mask = mask;
        AlbumSortCsrInit(mask, -1);
        AlbumViewMenuCsrSet(album_sort_key);
        AlbumViewMenuRowSetRgb(album_menu_mask);
        GetGPhaseCtrl()->leaf->state_cnt = 1;
    }

    if (GetGPhaseCtrl()->leaf->state_cnt != 1)
        return 0;

    if (album_sort_key >= 0)
        AlbumEditGuideSetMenu();
    return AlbumViewPlayMenu(1) != 0;
}

// FF3 0x00124890.
int end_Album_Menu(void)
{
    int done = AlbumViewPlayMenu(0);

    if (done)
    {
        AlbumSortCsrRelease();
        AlbumViewArrowSetDisp(AlbumDispGetCurAlbum(), 1);
        AlbumEditGuideClear();
    }
    return done != 0;
}

// FF3 0x001248e0.
void pre_Album_Menu(void)
{
}

// The menu: OK runs the row's phase -- the zoom page (0) and the card
// save / load (2 / 3, load after confirm 0xf) turn the page first, the
// zoom page as a sibling, the card screens linked below; cancel goes back
// to the edit page. States: 0 the cursor, 1 / 2 page turn out / back for
// the zoom page, 3 page turn out for the card, 4 / 5 back from it, 6 load
// confirm answered.
// FF3 0x001248e8.
// callcheck: GPhaseCtrlSetNext (GPHASE_CTRL::SetNext)
int one_Album_Menu(void)
{
    s_char side = AlbumDispGetCurAlbum();
    int    r;

    AlbumViewUpdatePages(1, album_menu_state == 1);

    if (album_menu_state == 4)
    {
        u_char mask = AlbumMenuGetRowMask();

        album_menu_mask = mask;
        AlbumSortCsrRestore(mask);
        AlbumViewMenuCsrSet(album_sort_key);
        AlbumViewMenuRowSetRgb(album_menu_mask);
        AlbumViewPageSetOn(0, 1);
        AlbumViewPageSetOn(1, 1);
        GetGPhaseTree()->Reset();
        AlbumEditGuideSetMenu();
        album_menu_state = 5;
    }
    if (album_menu_state == 5 && AlbumViewPlayPageTurn(1, side))
        album_menu_state = 0;
    if (album_menu_state == 2)
    {
        AlbumViewPageSetOn((s_char)(side ^ 1), 1);
        if (AlbumViewPlayPageTurn(1, side))
            album_menu_state = 0;
        AlbumEditGuideSetMenu();
    }

    if (album_menu_state == 0)
    {
        if (AlbumSortCsrUpdate())
        {
            AlbumViewMenuCsrSet(album_sort_key);
            AlbumEditGuideSetMenu();
        }
        else if (CheckPadTrgPlaySe((u_short)pad_key[PAD_KEY_CANCEL], ALBUM_SE_CANCEL, 0))
        {
            GetGPhaseCtrl()->SetNext(PHASE_ALBUM_MAIN, 0);
            AlbumViewSetFadeLayer(PHASE_ALBUM_MAIN);
            return 1;
        }
        else if (CheckPadTrgPlaySe((u_short)pad_key[PAD_KEY_OK], -1, 0) && (r = AlbumMenuRowCheck()) != 0)
        {
            if (r != 1)
                return 1;
            switch (album_sort_key)
            {
            case 0:
                album_menu_state = 1;
                break;
            case 2:
                album_menu_state = 3;
                break;
            case 3:
                AlbumEditConfirmReq(0xf, 1);
                album_menu_state = 6;
                return 1;
            default:
                GetGPhaseCtrl()->SetNext(album_menu_phase[album_sort_key], 0);
                AlbumViewSetFadeLayer(album_menu_phase[album_sort_key]);
                return 1;
            }
        }
    }

    if (album_menu_state == 1)
    {
        AlbumViewPageSetOn((s_char)(side ^ 1), 0);
        if (AlbumViewPlayPageTurn(0, side))
        {
            album_menu_state = 2;
            GetGPhaseCtrl()->SetNext(album_menu_phase[album_sort_key], 0);
            AlbumViewSetFadeLayer(album_menu_phase[album_sort_key]);
            return 1;
        }
    }
    if (album_menu_state == 6)
    {
        if (album_conf_pos == 0)
        {
            album_menu_state = 3;
        }
        else
        {
            album_menu_state = 0;
            AlbumEditGuideSetMenu();
        }
    }
    if (album_menu_state == 3)
    {
        AlbumViewPageSetOn(0, 0);
        AlbumViewPageSetOn(1, 0);
        if (AlbumViewPlayPageTurn(0, side))
        {
            album_menu_state = 4;
            GetGPhaseCtrl()->SetNext(album_menu_phase[album_sort_key], 1);
            AlbumViewSetFadeLayer(album_menu_phase[album_sort_key]);
            AlbumSortCsrSave();
            return 1;
        }
    }
    return 0;
}

// FF3 0x00124ce0.
void after_Album_Menu(void)
{
}

// ──────────────────────────────────────────────────────────────────────
// Phase 52, Album_Sort
// ──────────────────────────────────────────────────────────────────────

// As AlbumMenuRowCheck, for the key under the key cursor.
// FF3 0x00124ce8.
static int AlbumKeyRowCheck(void)
{
    if (album_key_mask == 0)
    {
        SndSePlay(ALBUM_SE_BUZZ, 100, 0x1000, 0, nullptr, 0);
        return -1;
    }
    if (((album_key_mask >> album_key_sel) & 1) == 0)
    {
        SndSePlay(ALBUM_SE_BUZZ, 100, 0x1000, 0, nullptr, 0);
        return 0;
    }
    SndSePlay(ALBUM_SE_OK, 100, 0x1000, 0, nullptr, 0);
    return 1;
}

// FF3 0x00124d88.
int init_Album_Sort(void)
{
    if (GetGPhaseCtrl()->leaf->state_cnt == 0)
    {
        u_char mask = (u_char)AlbumMenuCondSortKeys();

        album_key_mask = mask;
        AlbumKeyCsrInit(ALBUM_KEY_CSR, mask);
        AlbumViewSortCsrSet(album_key_sel);
        AlbumViewSortRowSetRgb(album_key_mask);
        GetGPhaseCtrl()->leaf->state_cnt = 1;
    }

    if (GetGPhaseCtrl()->leaf->state_cnt != 1)
        return 0;
    return AlbumViewPlaySort(1) != 0;
}

// FF3 0x00124e58.
int end_Album_Sort(void)
{
    int done;

    AlbumEditGuideSetMenu();
    done = AlbumViewPlaySort(0);
    if (done)
        AlbumKeyCsrRelease(ALBUM_KEY_CSR);
    return done != 0;
}

// FF3 0x00124e98.
void pre_Album_Sort(void)
{
}

// The key cursor; OK sorts the side by the key (again: the other way),
// cancel goes back to the menu.
// FF3 0x00124ea0.
int one_Album_Sort(void)
{
    int r;

    if (AlbumKeyCsrUpdate(ALBUM_KEY_CSR))
    {
        AlbumViewSortCsrSet(album_key_sel);
        return 0;
    }
    if (CheckPadTrgPlaySe((u_short)pad_key[PAD_KEY_CANCEL], ALBUM_SE_CANCEL, 0))
    {
        GetGPhaseCtrl()->SetNext(PHASE_ALBUM_MENU, 0);
        AlbumViewSetFadeLayer(PHASE_ALBUM_MENU);
        return 1;
    }
    if (!CheckPadTrgPlaySe((u_short)pad_key[PAD_KEY_OK], -1, 0))
        return 0;

    r = AlbumKeyRowCheck();
    if (r == 0)
        return 0;
    if (r != 1)
        return 1;
    if (AlbumEditSort(album_key_sel))
        AlbumViewUpdatePages(1, 0);
    return 0;
}

// FF3 0x00124f80.
void after_Album_Sort(void)
{
}

// ──────────────────────────────────────────────────────────────────────
// Phase 54, Album_Help
// ──────────────────────────────────────────────────────────────────────

// The message viewer (layout anim type 1, mode 2) on the pending help
// message, ALBUM_HELP_EDIT by default.
// FF3 0x00124f88.
int init_Album_Help(void)
{
    if (album_main_return != PHASE_START_MENU_MISSION_SAVE)
        LayoutAnimMain();
    album_help_anim = LayoutAnimStart(1, 0);
    MsgViewSetMode(MSG_VIEW_MODE_2);
    MsgViewSetMsg(ALBUM_MSG, album_help_msg != -1 ? album_help_msg : ALBUM_HELP_EDIT);
    return 1;
}

// FF3 0x00124ff0.
int end_Album_Help(void)
{
    return 1;
}

// When the viewer has closed (state 4): reset it, give the fade layer back
// to ALBM_CMN's item 0x3f1 and return.
// FF3 0x00124ff8.
// callcheck: CmnFadeLayerTerm (capCtl::Term), CmnFadeLayerSetItem (capCtl::SetItem), BpObjGetHandle (BP_OBJ::GetHandle)
int one_Album_Help(void)
{
    if (album_main_return != PHASE_START_MENU_MISSION_SAVE)
        LayoutAnimMain();
    if (LayoutAnimGetState(album_help_anim) != 4)
        return 0;

    LayoutAnimReset(album_help_anim);
    GetCmnFadeLayer()->Term();
    AlbumViewSetFadeLayer(GetGPhaseCtrl()->from);
    GetCmnFadeLayer()->SetItem(album_layout_obj[ALBUM_LAYOUT_CMN]->GetHandle(), 0x3f1);
    GetGPhaseCtrl()->SetNext(GetGPhaseCtrl()->from, 0);
    return 1;
}

// ──────────────────────────────────────────────────────────────────────
// Phase 78, Album_Confirm
// ──────────────────────────────────────────────────────────────────────

// FF3 0x001250d0.
int init_Album_Confirm(void)
{
    AlbumViewPageCsrFreeze(0, 0);
    AlbumViewPageCsrFreeze(1, 0);
    if (album_conf_default != -1)
    {
        AlbumConfCsrInit();
        AlbumViewSetMsgVariant((u_char)album_conf_pos);
    }
    return 1;
}

// FF3 0x00125120.
int end_Album_Confirm(void)
{
    if (album_conf_default != -1)
        AlbumConfCsrRelease();
    AlbumViewShowCopyItem(1);
    return 1;
}

// A message alone closes on OK or cancel; with choices, OK keeps
// album_conf_pos and cancel sets it to -1. Back to the phase that asked.
// FF3 0x00125158.
int one_Album_Confirm(void)
{
    AlbumViewUpdatePages(1, 0);

    if (album_conf_default == -1)
    {
        ReiPan_MenuMouseConfirm();      // PC: a click acknowledges
        if (!CheckPadTrgPlaySe((u_short)(pad_key[PAD_KEY_OK] | pad_key[PAD_KEY_CANCEL]), -1, 0))
        {
            CheckPadTrgPlaySe((u_short)pad_key[PAD_KEY_OK], ALBUM_SE_BUZZ, 0);
            return 0;
        }
        GetGPhaseCtrl()->SetNext(GetGPhaseCtrl()->from, 0);
        AlbumViewSetFadeLayer(GetGPhaseCtrl()->from);
        AlbumEditGuideClear();
        return 1;
    }

    if (AlbumConfCsrUpdate())
    {
        AlbumViewSetMsgVariant((u_char)album_conf_pos);
        return 0;
    }
    if (CheckPadTrgPlaySe((u_short)pad_key[PAD_KEY_CANCEL], -1, 0))
        album_conf_pos = -1;
    else if (!CheckPadTrgPlaySe((u_short)pad_key[PAD_KEY_OK], ALBUM_SE_OK, 0))
        return 0;

    GetGPhaseCtrl()->SetNext(GetGPhaseCtrl()->from, 0);
    AlbumViewSetFadeLayer(GetGPhaseCtrl()->from);
    return 1;
}
