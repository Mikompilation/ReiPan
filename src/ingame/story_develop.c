/* ==========================================================================
 *  ingame/story_develop.c
 *
 *  Developing films: the develop table and phases 38 Story_Develop / 65
 *  Story_DevelopFilm; see story_develop.h. FF3 0x1b3078..0x1b57d8 +
 *  0x2b91a8..0x2b94b0.
 *
 *  Phase 38 keeps a list of the films ready to be developed (DEVELOP_LIST),
 *  a cursor over it (MenuCsr 0x26), a menu step (DevelopMenuMain, 0..9) and
 *  a layout step (DevelopLayoutStep: load, "in", menu, "out", released). The
 *  GENZOU_PHF layout draws through item hooks: the current film's picture
 *  with its developed picture over it (item 0x41f, two MENU_PICs), the film
 *  strip (0x3f3 frames, 0x3ff / 0x402 thumbnails of films not developed /
 *  developed, scrolled towards the cursor) and the film's document title
 *  (0x3fe). The window (layout type 9) carries the film's message. When
 *  memos can be read, a second memo notice announces the pages a film
 *  revealed and can open the memo menu (the phase tree is reset when it
 *  comes back, so the layout loads again).
 *
 *  Not written: FF3 0x001b57a0 / 0x001b57c0, the GPhaseCtrl / GPhaseTree
 *  singletons' atexit thunks (as in every file that includes the getters).
 * ======================================================================== */
#include "ingame/story_develop.h"

#include "common/cmn_fade_layer.h"      // GetCmnFadeLayer
#include "common/heapctrl.h"            // heapCtrlMalloc, heapCtrlFree
#include "common/vu0math.h"             // Vu0Sinf
#include "graphics/scene/IngameScene.h"  // SleepFlgClear
#include "graphics/effect/effect_oth.h" // SubFocus
#include "graphics/graph2d/fade.h"      // FadeInReq, FadeOutReq, FadeMain, GetFadeState
#include "graphics/gs_reg.h"            // ScreenCaptureSave, ScreenCaptureDisp
#include "graphics/motion/anicode.h"    // B2D_HOOK_REC, B2dSetHookList, SetDefaultGsDrawState
#include "ingame/ingame.h"              // GPhaseCtrlRequest, IngameIsIdOver2e
#include "ingame/item/prg/item.h"       // ItemSetNum
#include "ingame/item/prg/soul_list.h"  // SoulListSetHave
#include "ingame/memo/memo.h"           // memo_flg, MemoSetPage, MEMO_NOTICE, MemoNoticeMenuOpen
#include "ingame/menu/anim_2d.h"        // LayoutAnim*, LayoutMsg*, Anim2dMsg*
#include "ingame/menu/menu_bg.h"        // MenuLayoutIsPlaying, MenuScrollArrowDisp, B2dHookPrintMsgLine
#include "ingame/menu/menu_csr.h"       // MenuCsr*
#include "ingame/menu/menu_draw_tex.h"  // MENU_PIC
#include "ingame/photo/n_plyr_camera.h" // PlyrCameraSetEcSub
#include "ingame/sound/snd_ctrl.h"      // SndSePlay, SndSeBank*
#include "ingame/story_menu_photo.h"    // DocFlagSet, DocFlagClear, DocGetMsgType, msg_pic_key_dat
#include "main/gphase.h"                // GetGPhaseCtrl, GetGPhaseTree
#include "main/gphase_tree.h"           // GPHASE_TREE::Reset
#include "system/b2d/B2dCtl.h"          // B2dCtlFindItem, B2D_HOOK_ARGS
#include "system/b2d/B2dSpr.h"          // B2dSprGetPartData
#include "system/b2d/BpCtl.h"           // BpCtl*, BpPlayLabel, BpSetBlendAlpha, B2dDrawSpriteDirect
#include "system/eeiop/cd_file_dat.h"   // GENZOU_PHF, MENU_GENZOU_BD_BD
#include "system/eeiop/snd_buffer.h"    // SndBufFadeStop
#include "system/os/sys_heap.h"         // GetSystemHeapWrkP
#include "system/os/system.h"           // GetPALMode
#include "system/pad/pad.h"             // pad_wrk[0].stick_dpad
#include "system/pad/pad_ctrl.h"        // pad_key, CheckPadTrgPlaySe
#include "ingame/menu/menu_file_list.h" // MemoMenuIsEnable
#include "ingame/menu/menu_mouse.h"    // PC: the mouse in menus

#define PHASE_STORY_DEVELOP         0x26    // phase 38
#define PHASE_STORY_DEVELOP_FILM    0x41    // phase 65

#define DEVELOP_CSR         0x26            // the MenuCsr slot
#define DEVELOP_MSG_TYPE    0x5c            // 0 "develop this film?", 1 every film developed

#define DEVELOP_SE_MOVE     0x1b790000      // the strip moved (MENU_GENZOU_BD)
#define DEVELOP_SE_DEVELOP  0x1b790001      // the picture develops (phase 65)
#define DEVELOP_SE_CANCEL   0x1a9b0001
#define DEVELOP_SE_BUZZER   0x1a9b0002
#define DEVELOP_SE_OK       0x1a9b0003

// The menu steps (DevelopMenuMain).
enum DEVELOP_STEP
{
    DEVELOP_STEP_IN       = 0,          // window message type
    DEVELOP_STEP_MAIN     = 1,          // browse the strip
    DEVELOP_STEP_ASK      = 2,          // "develop this film?"
    DEVELOP_STEP_ASK_WAIT = 3,
    DEVELOP_STEP_DEVELOP  = 4,          // develop, phase 65
    DEVELOP_STEP_RESULT   = 5,          // back from 65: the memo reward timer
    DEVELOP_STEP_MSG      = 6,          // the developed film's message
    DEVELOP_STEP_MSG_WAIT = 7,
    DEVELOP_STEP_ALL      = 8,          // "every film developed"
    DEVELOP_STEP_ALL_WAIT = 9,
    DEVELOP_STEP_NUM      = 10,
};

// The layout steps (DevelopLayoutStep).
enum DEVELOP_LAYOUT
{
    DEVELOP_LAYOUT_LOAD    = 0,
    DEVELOP_LAYOUT_WAIT    = 1,
    DEVELOP_LAYOUT_OPEN    = 2,         // "in", the fade layer, the hooks
    DEVELOP_LAYOUT_IN      = 3,         // "in" plays
    DEVELOP_LAYOUT_MENU    = 4,         // the menu steps run
    DEVELOP_LAYOUT_CLOSE   = 5,         // "out"
    DEVELOP_LAYOUT_OUT     = 6,         // "out" plays, then the layout goes
    DEVELOP_LAYOUT_END     = 7,
};

static int   DevelopMenuMain(void);
static void  DevelopLayoutStep(void);
static void  DevelopMenuUpdate(void);
static int   DevelopMemoMenuOpen(int step);
static int   DevelopFadeLabel(void);
static void  DevelopFadeLayerSwitch(void);
static void  DevelopMenuInit(void);
static void  DevelopMenuRelease(void);
static void  DevelopCsrInit(void);
static void  DevelopCsrRelease(void);
static int   DevelopCsrUpdate(void);
static int   DevelopGetStep(void);
static int   DevelopSetStep(u_int step);
static short DevelopCsrGetPos(void);
static short DevelopCsrSetPos(short pos);
static int   DevelopFindNext(int index);
static int   DevelopFindPrev(int index);
static short DevelopCsrGetFilm(void);
static short DevelopListGetNum(void);
static short DevelopListGet(short i);
static int   DevelopListIsDone(short i);
static int   DevelopListDevelop(short i);
static int   DevelopMenuHasRemain(void);
static void  DevelopFilmMemoReward(short film);
static void  DevelopMemoNoticeDisp(void);
static int   DevelopMemoNoticeMenuOpen(void);
static void  DevelopPicCreate(void);
static void  DevelopPicDelete(void);
static void  DevelopPicStep(void);
static void  DevelopThumbCreate(void);
static void  DevelopThumbDelete(void);
static void  DevelopThumbStep(void);
static MENU_PIC *DevelopThumbCur(void);
static void  DevelopMenuDummy(void);
static int   DevelopCanScrollUp(void);
static int   DevelopCanScrollDown(void);
static void  DevelopArrowDisp(void);
static void  DevelopWdwShow(void);
static void  DevelopWdwSetMsg(void);
static void  DevelopFilmInitSub(void);

// ──────────────────────────────────────────────────────────────────────
// The develop table
// ──────────────────────────────────────────────────────────────────────

// data 0x325260 (zeros in the ROM); save block 4.
DEVELOP_FILM develop_film[DEVELOP_FILM_NUM];

// rodata 0x359308
const DEVELOP_DAT develop_dat[DEVELOP_FILM_NUM] =
{
    //  no menu_pic msg      soul   doc            memo[3]                                          _1c new_game
    {  0, 1822, 0x22, 0, -1, { 0x6d, 0x6e }, { { 0x16, 1 }, { -1, -1 }, { -1, -1 } }, 0, 1 },
    {  1, 1846, 0x2c, 0, -1, { 0x70, 0x71 }, { {   -1, -1 }, { -1, -1 }, { -1, -1 } }, 0, 1 },
    {  2, 1754, 0x26, 0, -1, { 0x72, 0x73 }, { {   -1, -1 }, { -1, -1 }, { -1, -1 } }, 0, 1 },
    {  3, 1810, 0x1c, 0, -1, { 0x74, 0x75 }, { {   -1, -1 }, { -1, -1 }, { -1, -1 } }, 0, 1 },
    {  4, 1786, 0x10, 0, -1, { 0x76, 0x77 }, { {   -1, -1 }, { -1, -1 }, { -1, -1 } }, 0, 1 },
    {  5, 1814, 0x1e, 0, -1, { 0x78, 0x79 }, { {   -1, -1 }, { -1, -1 }, { -1, -1 } }, 0, 1 },
    {  6, 1806, 0x1a, 0, -1, { 0x7a, 0x7b }, { {   -1, -1 }, { -1, -1 }, { -1, -1 } }, 0, 1 },
    {  7, 1794, 0x14, 0, -1, { 0x7c, 0x7d }, { {   -1, -1 }, { -1, -1 }, { -1, -1 } }, 0, 1 },
    {  8, 1802, 0x18, 0, -1, { 0x7e, 0x7f }, { {   -1, -1 }, { -1, -1 }, { -1, -1 } }, 0, 1 },
    {  9, 1798, 0x16, 0, -1, { 0x80, 0x81 }, { { 0x1c, 1 }, { -1, -1 }, { -1, -1 } }, 0, 1 },
    { 10, 1782, 0x0e, 0, -1, { 0x83, 0x84 }, { { 0x08, 0 }, { -1, -1 }, { -1, -1 } }, 0, 1 },
    { 11, 1850, 0x28, 0, -1, { 0x85, 0x86 }, { {   -1, -1 }, { -1, -1 }, { -1, -1 } }, 0, 1 },
    { 12, 1790, 0x12, 0, -1, { 0x89, 0x8a }, { { 0x1b, 2 }, { -1, -1 }, { -1, -1 } }, 0, 1 },
    { 13, 1834, 0x2e, 0, -1, { 0x8f, 0x90 }, { { 0x17, 1 }, { -1, -1 }, { -1, -1 } }, 0, 1 },
    { 14, 1770, 0x08, 0, -1, { 0x91, 0x92 }, { {   -1, -1 }, { -1, -1 }, { -1, -1 } }, 0, 1 },
    { 15, 1838, 0x2a, 0, -1, { 0x93, 0x94 }, { { 0x19, 0 }, { -1, -1 }, { -1, -1 } }, 0, 1 },
    { 16, 1778, 0x0c, 0, -1, { 0x96, 0x97 }, { { 0x1f, 0 }, { -1, -1 }, { -1, -1 } }, 0, 1 },
    { 17, 1830, 0x30, 0, -1, { 0x99, 0x9a }, { {   -1, -1 }, { -1, -1 }, { -1, -1 } }, 0, 1 },
    { 18, 1758, 0x06, 0, -1, { 0x9b, 0x9c }, { { 0x07, 1 }, { -1, -1 }, { -1, -1 } }, 0, 1 },
    { 19, 1746, 0x00, 0, -1, { 0xa1, 0xa2 }, { { 0x1a, 0 }, { -1, -1 }, { -1, -1 } }, 0, 1 },
};

// ──────────────────────────────────────────────────────────────────────
// FF3 0x001b3078 (IngameInit)

void DevelopFilmInit(void)
{
    short i;

    DevelopFilmInitSub();
    for (i = 0; i < DEVELOP_FILM_NUM; i++) {
        develop_film[i].state = DEVELOP_STATE_NONE;
        develop_film[i].area  = 0;
        develop_film[i].arg   = 0;
    }
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x001b30c8 (IngameNewGameReset)

void DevelopFilmNewGame(void)
{
    short i;

    for (i = 0; i < DEVELOP_FILM_NUM; i++) {
        if (develop_dat[i].new_game != 0) {
            develop_film[i].state = DEVELOP_STATE_NONE;
            develop_film[i].area  = 0;
            develop_film[i].arg   = 0;
        }
    }
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x001b3118 (IngameEnd): empty.

void DevelopFilmEnd(void)
{
}

// ──────────────────────────────────────────────────────────────────────
// A film shot (SHOOT_DEVELOP_FILM, photo step 14): ready at once when
// IngameIsIdOver2e(area), else it waits for GET_DEVELOP_FILM.
// FF3 0x001b3130

int DevelopFilmShoot(short film, int area, char arg)
{
    DEVELOP_FILM *f = &develop_film[film];

    if (f->state != DEVELOP_STATE_NONE)
        return 0;
    f->area  = (s_char)area;
    f->arg   = (short)(s_char)arg;
    f->state = IngameIsIdOver2e(area) ? DEVELOP_STATE_READY : DEVELOP_STATE_SHOT;
    return 1;
}

// ──────────────────────────────────────────────────────────────────────
// GET_DEVELOP_FILM. FF3 0x001b31a8

int DevelopFilmGetAll(void)
{
    int   n = 0;
    short i;

    for (i = 0; i < DEVELOP_FILM_NUM; i++) {
        if (develop_film[i].state == DEVELOP_STATE_SHOT) {
            develop_film[i].state = DEVELOP_STATE_READY;
            n++;
        }
    }
    return n;
}

// ──────────────────────────────────────────────────────────────────────
// The film is developed: its ghost list entry, its document replaced.
// FF3 0x001b31f0

int DevelopFilmDevelop(short film)
{
    const DEVELOP_DAT *dat;

    if (develop_film[film].state != DEVELOP_STATE_READY)
        return 0;
    develop_film[film].state = DEVELOP_STATE_DONE;
    SleepFlgClear(2, film);
    dat = &develop_dat[film];
    if ((u_int)dat->soul < 0xd9)
        SoulListSetHave(dat->soul);
    if (dat->doc[0] < MSG_PIC_KEY_NUM)
        DocFlagClear((short)dat->doc[0]);
    if (dat->doc[1] < MSG_PIC_KEY_NUM)
        DocFlagSet((short)dat->doc[1]);
    return 1;
}

// ──────────────────────────────────────────────────────────────────────
// REMAIN_UNDEVELOP_FILMS. FF3 0x001b32c0

int DevelopFilmIsRemain(void)
{
    short i;

    for (i = 0; i < DEVELOP_FILM_NUM; i++) {
        if (develop_film[i].state == DEVELOP_STATE_READY)
            return 1;
    }
    return 0;
}

// ──────────────────────────────────────────────────────────────────────
// Item 0x10 (the undeveloped film) is held while a film is ready.
// FF3 0x001b3308

void DevelopFilmItemUpdate(void)
{
    ItemSetNum(0x10, DevelopFilmIsRemain() != 0 ? 1 : 0, 0);
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x001b3340 (no caller)

int DevelopFilmIsAreaOver(short film)
{
    return IngameIsIdOver2e(develop_film[film].area);
}

// ──────────────────────────────────────────────────────────────────────
// DEVELOP_REQUEST. FF3 0x001b3378

void DevelopReq(void)
{
    GPhaseCtrlRequest(PHASE_STORY_DEVELOP, 0);
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x001b3398 (save block table 0x332acc)

int DevelopFilmSaveGet(void **addr)
{
    *addr = develop_film;
    return sizeof(develop_film);
}

// FF3 0x001b33b0: empty (jr ra), called by DevelopFilmInit.
static void DevelopFilmInitSub(void)
{
}

// ──────────────────────────────────────────────────────────────────────
// DEVELOP_LIST
// FF3 0x001b33c8: the base-object constructor, the same body as
// FF3 0x001b33e8 (DevelopListCtor, the complete-object one)

DEVELOP_LIST::DEVELOP_LIST()
{
    num  = 0;
    list = nullptr;
    Build();
}

// FF3 0x001b3420
DEVELOP_LIST::~DEVELOP_LIST()
{
    Free();
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x001b3438

void DEVELOP_LIST::Build()
{
    short n = Fill(0);

    if (n != 0) {
        list = (short *)heapCtrlMalloc(GetSystemHeapWrkP(), n * sizeof(short), 0, 0, 0);
        num  = Fill(1);
    }
}

// ──────────────────────────────────────────────────────────────────────
// The films ready to be developed; with store (and a list) written into it.
// FF3 0x002b91a8 (out of line after DocListFill)

short DEVELOP_LIST::Fill(int store)
{
    short n = 0;
    short i;

    for (i = 0; i < DEVELOP_FILM_NUM; i++) {
        if (develop_film[i].state != DEVELOP_STATE_READY)
            continue;
        if (store != 0 && list != nullptr)
            list[n] = i;
        n++;
    }
    return n;
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x001b34a8

void DEVELOP_LIST::Free()
{
    if (list != nullptr) {
        heapCtrlFree(GetSystemHeapWrkP(), list, 0, 0, 0);
        list = nullptr;
    }
    num = 0;
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x001b34f8 (the ROM keeps a dead i < 0 test: -1 would read row -1)

int DEVELOP_LIST::HasRemain()
{
    short i;

    for (i = 0; i < num; i++) {
        if (develop_film[list[i]].state == DEVELOP_STATE_READY)
            return 1;
    }
    return 0;
}

// ──────────────────────────────────────────────────────────────────────
// The menu
// ──────────────────────────────────────────────────────────────────────

// The film strip: item positions and the scroll (bss 0x3e78f8, 0x20 bytes).
// layout: host -- pic is a pointer (EE 4 bytes at +0x1c); file-local.
struct DEVELOP_STRIP
{
    /* 0x00 */ int        x, y;         // item 0x3ff: the thumbnails' column
    /* 0x08 */ int        frame_x, frame_y;     // item 0x3f3: the film frames
    /* 0x10 */ int        num;          // thumbnails
    /* 0x14 */ int        dy;           // row height (item 0x3f3's h)
    /* 0x18 */ float      scroll;       // pixels, eased towards cursor * dy
    /* 0x1c */ MENU_PIC **pic;          // one per film
};

static int           develop_step;              // bss 0x3e78e8: DEVELOP_STEP
static DEVELOP_LIST *develop_list;              // bss 0x3e78ec
static MEMO_NOTICE  *develop_notice;            // bss 0x3e78f0
static DEVELOP_STRIP develop_strip;             // bss 0x3e78f8
static int           develop_ret_phase;         // sbss gp-0x2f8: GPHASE_CTRL.from at init
static int           develop_tree_reset;        // sbss gp-0x2f4: back from the memo menu
static int           develop_wdw_anim;          // sbss gp-0x2f0: the window (layout type 9)
static int           develop_count;             // sbss gp-0x2ec: films developed in this visit
static u_char        develop_layout_step;       // sbss gp-0x2e8: DEVELOP_LAYOUT
static MENU_PIC     *develop_pic;               // sbss gp-0x2e4: the film's picture
static MENU_PIC     *develop_pic_done;          // sbss gp-0x2e0: its developed picture
static int           develop_film_wait;         // sbss gp-0x2dc: phase 65's frames
static int           develop_film_se;           // sbss gp-0x2d8: phase 65's SE
static int           develop_reward_wait;       // sdata 0x370444: frames until the memo reward

// The layout as every use finds it.
static int DevelopHandle(void)
{
    return BpCtlGetHandleByFileNo(GENZOU_PHF);
}

// ──────────────────────────────────────────────────────────────────────
// Phase 38 Story_Develop
// ──────────────────────────────────────────────────────────────────────

// FF3 0x001b3580. Step 0: the menu, the layout and window start loading
// (layout step 2: it is open as soon as init ends), the SE bank, a capture
// of the frame and a fade out; step 1: once all is loaded and faded, "in"
// and the pictures.
// callcheck: BpCtlGetHandleByFileNo (DevelopHandle)
// callcheck: GetPALMode (fade time)

int init_Story_Develop(void)
{
    switch (GetGPhaseCtrl()->leaf->state_cnt) {
    case 0:
        pad_wrk[0].stick_dpad = 1;
        DevelopMenuInit();
        develop_ret_phase   = GetGPhaseCtrl()->from;
        develop_layout_step = DEVELOP_LAYOUT_OPEN;
        develop_tree_reset  = 0;
        BpCtlLoadReq(GENZOU_PHF, nullptr);
        develop_wdw_anim = LayoutAnimStart(9, 0);
        LayoutAnimSetFlag(develop_wdw_anim, LAYOUT_ANIM_WAIT | LAYOUT_ANIM_LABEL_LAYOUT);
        LayoutMsgSetKeepOpen(1);
        Anim2dMsgSetType(2);
        SndSeBankLoad(MENU_GENZOU_BD_BD);
        ScreenCaptureSave(2, 1);
        FadeOutReq(0, 0, 0, GetPALMode() ? 0xc : 0xf);
        develop_count = 0;
        GetGPhaseCtrl()->leaf->state_cnt = 1;
        break;

    case 1:
        break;

    default:
        return 0;
    }

    ScreenCaptureDisp(0x80, 0x80, 0x80, 0x80);
    FadeMain();
    BpCtlMain();
    if (BpCtlIsDataReady(DevelopHandle()) == 0 || LayoutAnimIsDataReady(develop_wdw_anim) == 0)
        return 0;
    if (SndSeBankIsLoaded(MENU_GENZOU_BD_BD) == 0)
        return 0;
    if (GetFadeState() == 0)
        return 0;
    BpPlayLabel(DevelopHandle(), "in");                         /* sdata 0x370440 */
    DevelopPicCreate();
    DevelopThumbCreate();
    return 1;
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x001b3748: fade in, then everything goes; the camera's spirit
// counters refresh when films were developed.
// callcheck: GetPALMode (fade time)

int end_Story_Develop(void)
{
    ScreenCaptureDisp(0x80, 0x80, 0x80, 0x80);
    FadeMain();
    switch (GetGPhaseCtrl()->leaf->state_cnt) {
    case 0:
        FadeInReq(0, 0, 0, GetPALMode() ? 0xc : 0xf);
        GetGPhaseCtrl()->leaf->state_cnt = 1;
        break;

    case 1:
        break;

    default:
        return 0;
    }

    if (GetFadeState() == 0)
        return 0;
    DevelopPicDelete();
    DevelopThumbDelete();
    DevelopMenuRelease();
    SndSeBankRelease(MENU_GENZOU_BD_BD, 0);
    pad_wrk[0].stick_dpad = 0;
    if (develop_count != 0) {
        PlyrCameraSetEcSub(0);
        PlyrCameraSetEcSub(1);
    }
    return 1;
}

// FF3 0x001b3850 / 0x001b3858: empty.
void pre_Story_Develop(void)
{
}

// FF3 0x001b3858
void after_Story_Develop(void)
{
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x001b3860: 1 once the layout is gone (cancelled, or the memo menu
// opened); the menu steps run while the layout is open.
// callcheck: GetGPhaseTree, GPhaseTreeReset (GetGPhaseTree()->Reset)

int one_Story_Develop(void)
{
    if (develop_tree_reset != 0) {
        GetGPhaseTree()->Reset();
        develop_tree_reset  = 0;
        develop_layout_step = DEVELOP_LAYOUT_LOAD;
    }
    DevelopLayoutStep();
    if (develop_layout_step == DEVELOP_LAYOUT_END)
        return 1;
    if (develop_layout_step == DEVELOP_LAYOUT_MENU)
        return DevelopMenuMain();
    return 0;
}

// ──────────────────────────────────────────────────────────────────────
// Every frame of phases 38 / 65. FF3 0x001b38d8

static void DevelopMenuUpdate(void)
{
    DevelopPicStep();
    DevelopThumbStep();
    DevelopArrowDisp();
    DevelopMenuDummy();
    GetCmnFadeLayer()->Update();
    LayoutAnimMain();
    BpCtlMain();
    if (memo_flg.open_flg != 0)
        DevelopMemoNoticeDisp();
}

// ──────────────────────────────────────────────────────────────────────
// The memo notice opens the memo menu: the layout closes at once and the
// menu comes back at step. FF3 0x001b3940
// callcheck: BpCtlGetHandleByFileNo (DevelopHandle)

static int DevelopMemoMenuOpen(int step)
{
    if (memo_flg.open_flg == 0 || DevelopMemoNoticeMenuOpen() == 0)
        return 0;
    LayoutAnimSetAlphaItem(develop_wdw_anim, nullptr);
    LayoutMsgSetKeepOpen(0);
    LayoutAnimReset(develop_wdw_anim);
    GetCmnFadeLayer()->SetItemHandle(nullptr);
    GetCmnFadeLayer()->Term();
    BpCtlRelease(DevelopHandle());
    develop_layout_step = DEVELOP_LAYOUT_END;
    develop_tree_reset  = 1;
    DevelopSetStep((u_int)step);
    return 1;
}

// ──────────────────────────────────────────────────────────────────────
// The steps (see DEVELOP_STEP); 1 when the frame left the phase (phase 65
// requested or the memo menu opened).
// FF3 0x001b39f0
// callcheck: GetPALMode (reward wait)

static int DevelopMenuMain(void)
{
    int   ret  = 0;
    int   step = DevelopGetStep();
    int   pos  = DevelopCsrGetPos();
    short film = DevelopListGet((short)pos);
    int   now, to;

    DevelopFadeLayerSwitch();
    switch (step) {
    case DEVELOP_STEP_IN:
        Anim2dMsgSetType(2);
        LayoutMsgSetArg(2);
        DevelopSetStep(DEVELOP_STEP_MAIN);
        // fall through
    case DEVELOP_STEP_MAIN:
        DevelopWdwSetMsg();
        if (DevelopCsrUpdate() != 0) {
            // The cursor moved: on to the nearest film still to develop.
            to  = -1;
            now = DevelopCsrGetPos();
            if (now < pos)
                to = DevelopFindPrev(now);
            if (pos < now)
                to = DevelopFindNext(now);
            if (to != -1 && to != now)
                DevelopCsrSetPos((short)to);
            SndSePlay(pos == to ? DEVELOP_SE_BUZZER : DEVELOP_SE_MOVE, 100, 0x1000, 0, 0, 0);
            return ret;
        }
        if (CheckPadTrgPlaySe(pad_key[PAD_KEY_OK], -1, 0)) {
            if (DevelopListIsDone((short)pos) != 0)
                return ret;
            SndSePlay(DEVELOP_SE_OK, 100, 0x1000, 0, 0, 0);
            DevelopSetStep(DEVELOP_STEP_ASK);
            return ret;
        }
        if (CheckPadTrgPlaySe(pad_key[PAD_KEY_CANCEL], -1, 0)) {
            SndSePlay(DEVELOP_SE_CANCEL, 100, 0x1000, 0, 0, 0);
            GPhaseCtrlRequest(develop_ret_phase, 0);
            develop_layout_step = DEVELOP_LAYOUT_CLOSE;
            return ret;
        }
        return DevelopMemoMenuOpen(DEVELOP_STEP_IN);

    case DEVELOP_STEP_ASK:
        Anim2dMsgSet(DEVELOP_MSG_TYPE, 0);
        Anim2dMsgSetMode(1, 0);
        LayoutMsgSetArg(1);
        DevelopSetStep(DEVELOP_STEP_ASK_WAIT);
        break;

    case DEVELOP_STEP_ASK_WAIT:
        if (LayoutMsgGetResult() == 0)
            return ret;
        DevelopSetStep(Anim2dMsgGetCsr() == 0 ? DEVELOP_STEP_DEVELOP : DEVELOP_STEP_IN);
        break;

    case DEVELOP_STEP_DEVELOP:
        ret = 1;
        Anim2dMsgSetType(2);
        Anim2dMsgSet(-1, -1);
        DevelopListDevelop((short)pos);
        GPhaseCtrlRequest(PHASE_STORY_DEVELOP_FILM, 0);
        DevelopSetStep(DEVELOP_STEP_RESULT);
        break;

    case DEVELOP_STEP_RESULT:
        develop_reward_wait = GetPALMode() ? 0x19 : 0x1e;
        DevelopSetStep(DEVELOP_STEP_MSG);
        break;

    case DEVELOP_STEP_MSG:
        Anim2dMsgSet(2, (short)(develop_dat[film].msg + 1));
        LayoutMsgSetArg(1);
        DevelopSetStep(DEVELOP_STEP_MSG_WAIT);
        break;

    case DEVELOP_STEP_MSG_WAIT:
        // The memo pages come a little after the message opens, or when it
        // is closed before that.
        if (develop_reward_wait > 0 && --develop_reward_wait == 0)
            DevelopFilmMemoReward(film);
        if (LayoutMsgGetResult() == 0)
            return DevelopMemoMenuOpen(DEVELOP_STEP_MSG);
        if (develop_reward_wait > 0)
            DevelopFilmMemoReward(film);
        to = DevelopFindNext(pos);
        if (to == -1) {
            DevelopSetStep(DEVELOP_STEP_ALL);
        } else {
            DevelopCsrSetPos((short)to);
            SndSePlay(DEVELOP_SE_MOVE, 100, 0x1000, 0, 0, 0);
            DevelopSetStep(DEVELOP_STEP_IN);
        }
        break;

    case DEVELOP_STEP_ALL:
        Anim2dMsgSet(DEVELOP_MSG_TYPE, 1);
        LayoutMsgSetArg(1);
        DevelopSetStep(DEVELOP_STEP_ALL_WAIT);
        break;

    case DEVELOP_STEP_ALL_WAIT:
        if (LayoutMsgGetResult() == 0)
            return DevelopMemoMenuOpen(DEVELOP_STEP_ALL);
        SndSePlay(DEVELOP_SE_CANCEL, 100, 0x1000, 0, 0, 0);
        GPhaseCtrlRequest(develop_ret_phase, 0);
        develop_layout_step = DEVELOP_LAYOUT_CLOSE;
        return ret;
    }
    return ret;
}

// ──────────────────────────────────────────────────────────────────────
// The fade layer's label: 0xc while the memo menu can be opened.
// FF3 0x001b3d90 / 0x001b3db8

static int DevelopFadeLabel(void)
{
    return MemoMenuIsEnable(0) ? 0xc : 0;
}

// FF3 0x001b3db8
static void DevelopFadeLayerSwitch(void)
{
    GetCmnFadeLayer()->Switch(DevelopFadeLabel(), 0x1e);
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x001b3df0: the list and a notice of its own; nothing left to
// develop starts on "every film developed".
// callcheck: operator_new, DevelopListCtor (new DEVELOP_LIST)
// callcheck: operator_new, MemoNoticeCtor (new MEMO_NOTICE)

static void DevelopMenuInit(void)
{
    develop_list   = new DEVELOP_LIST();
    develop_notice = new MEMO_NOTICE();
    develop_step   = (DevelopMenuHasRemain() == 0) ? DEVELOP_STEP_ALL : DEVELOP_STEP_IN;
    DevelopCsrInit();
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x001b3e70 (the pointers are not cleared, as in the ROM)
// callcheck: DevelopListDtor, operator_delete (delete DEVELOP_LIST)
// callcheck: MemoNoticeDtor, operator_delete (delete MEMO_NOTICE)

static void DevelopMenuRelease(void)
{
    DevelopCsrRelease();
    if (develop_list != nullptr)
        delete develop_list;
    if (develop_notice != nullptr)
        delete develop_notice;
    DevelopFilmItemUpdate();
}

// ──────────────────────────────────────────────────────────────────────
// The cursor: one column, a row per film (one when none).
// FF3 0x001b3ee0

static void DevelopCsrInit(void)
{
    short num = DevelopListGetNum();

    MenuCsrInit(DEVELOP_CSR, 1, (num != 0) ? num : 1, 1);
}

// FF3 0x001b3f20
static void DevelopCsrRelease(void)
{
    MenuCsrRelease(DEVELOP_CSR);
}

// FF3 0x001b3f38: 1 when the cursor moved (no SE of its own).
static int DevelopCsrUpdate(void)
{
    MenuCsrUpdate(DEVELOP_CSR, 0);
    return MenuCsrIsMovedSe(DEVELOP_CSR, -1);
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x001b3f60 / 0x001b3f70

static int DevelopGetStep(void)
{
    return develop_step;
}

// FF3 0x001b3f70
static int DevelopSetStep(u_int step)
{
    if (step < DEVELOP_STEP_NUM)
        develop_step = (int)step;
    return develop_step;
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x001b3f88 / 0x001b3fa8

static short DevelopCsrGetPos(void)
{
    return MenuCsrGetPos(DEVELOP_CSR);
}

// FF3 0x001b3fa8
static short DevelopCsrSetPos(short pos)
{
    MenuCsrSetPos(DEVELOP_CSR, pos);
    return MenuCsrGetPos(DEVELOP_CSR);
}

// ──────────────────────────────────────────────────────────────────────
// From index on, the first film not developed yet, else the last one
// before it; -1 when none remains. FF3 0x001b3fd8

static int DevelopFindNext(int index)
{
    int num, i;

    if (DevelopMenuHasRemain() != 0) {
        num = DevelopListGetNum();
        for (i = index; i < num; i++) {
            if (DevelopListIsDone((short)i) == 0)
                return i;
        }
        while (--index >= 0) {
            if (DevelopListIsDone((short)index) == 0)
                return index;
        }
    }
    return -1;
}

// ──────────────────────────────────────────────────────────────────────
// From index down, the first film not developed yet, else the first one
// after it; -1 when none remains. FF3 0x001b4080

static int DevelopFindPrev(int index)
{
    int num, i;

    if (DevelopMenuHasRemain() == 0)
        return -1;
    num = DevelopListGetNum();
    for (i = index; i >= 0; i--) {
        if (DevelopListIsDone((short)i) == 0)
            return i;
    }
    for (i = index + 1; i < num; i++) {
        if (DevelopListIsDone((short)i) == 0)
            return i;
    }
    return -1;
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x001b4128: the film under the cursor, -1 none.

static short DevelopCsrGetFilm(void)
{
    return DevelopListGet(DevelopCsrGetPos());
}

// FF3 0x001b4150
static short DevelopListGetNum(void)
{
    return develop_list->num;
}

// FF3 0x001b4160 (-1 outside the list)
static short DevelopListGet(short i)
{
    if (i < 0 || i >= develop_list->num)
        return -1;
    return develop_list->list[i];
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x001b41a8: no film there, or developed.

static int DevelopListIsDone(short i)
{
    short film = DevelopListGet(i);

    if (film == -1)
        return 1;
    return develop_film[film].state == DEVELOP_STATE_DONE;
}

// FF3 0x001b41f8
static int DevelopListDevelop(short i)
{
    short film = DevelopListGet(i);

    if ((u_short)film < DEVELOP_FILM_NUM)
        return DevelopFilmDevelop(film);
    return 0;
}

// FF3 0x001b4238
static int DevelopMenuHasRemain(void)
{
    return develop_list->HasRemain();
}

// ──────────────────────────────────────────────────────────────────────
// The film's memo pages; each one new to the player is announced.
// FF3 0x001b4258

static void DevelopFilmMemoReward(short film)
{
    const DEVELOP_DAT *dat = &develop_dat[film];
    int i;

    for (i = 0; i < 3; i++) {
        const DEVELOP_MEMO *m = &dat->memo[i];

        if ((u_short)m->memo < 0x21 && MemoSetPage(m->memo, m->page, 0) != 0)
            develop_notice->Add(m->memo);
    }
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x001b42f0 / 0x001b4328

static void DevelopMemoNoticeDisp(void)
{
    develop_notice->Step();
    develop_notice->DispHead(1);
}

// FF3 0x001b4328
static int DevelopMemoNoticeMenuOpen(void)
{
    return MemoNoticeMenuOpen(develop_notice, 1, 4);
}

// ──────────────────────────────────────────────────────────────────────
// One of DevelopPicCreate's two pictures (written twice inline in the ROM).

static MENU_PIC *DevelopPicNew(short x, short y)
{
    MENU_PIC *pic = new MENU_PIC(0);

    pic->v        = 0;
    pic->u        = 0;
    pic->h        = 0x100;
    pic->w        = 0x182;
    pic->pri      = 0xa0;
    pic->x        = x;
    pic->y        = y;
    pic->z        = 0;
    pic->fade_max = 0x80;
    pic->fade_min = 0;
    pic->enable   = 1;
    return pic;
}

// The film's picture and its developed picture over it, 0x182 x 0x100 at
// item 0x41f. FF3 0x001b4350
// callcheck: BpCtlGetHandleByFileNo (DevelopHandle)
// callcheck: operator_new, MenuPicCtor (DevelopPicNew: new MENU_PIC)

static void DevelopPicCreate(void)
{
    B2D_ITEM *item = B2dCtlFindItem(DevelopHandle(), 0x41f);
    short x = (short)B2dSprGetPartData(item, B2D_PART_X);
    short y = (short)B2dSprGetPartData(item, B2D_PART_Y);

    develop_pic      = DevelopPicNew(x, y);
    develop_pic_done = DevelopPicNew(x, y);
}

// FF3 0x001b44a8
// callcheck: MenuPicDtor, operator_delete (delete MENU_PIC)
static void DevelopPicDelete(void)
{
    if (develop_pic != nullptr)
        delete develop_pic;
    develop_pic = nullptr;
    if (develop_pic_done != nullptr)
        delete develop_pic_done;
    develop_pic_done = nullptr;
}

// ──────────────────────────────────────────────────────────────────────
// While browsing the pictures follow the cursor's film (the developed one
// blank until it is developed). FF3 0x001b4508

static void DevelopPicStep(void)
{
    short film = DevelopCsrGetFilm();
    short pic;

    if (film == -1)
        return;
    pic = develop_dat[film].menu_pic;
    if ((u_int)DevelopGetStep() < DEVELOP_STEP_ASK) {
        develop_pic->pic = pic;
        if (develop_film[film].state == DEVELOP_STATE_DONE)
            develop_pic_done->pic = (short)(pic + 1);
        else
            develop_pic_done->pic = -2;
    }
    develop_pic->Step();
    develop_pic_done->Step();
}

// ──────────────────────────────────────────────────────────────────────
// Item 0x41f (a box item: a1 w, a2 h, stack[2] alpha): both pictures, and
// while the developed one fades in after browsing a focus pulse over it.
// FF3 0x001b45d8

static int DevelopPicHook(const B2D_HOOK_ARGS *args)
{
    int   w = (int)args->a1;
    int   h = (int)args->a2;
    float scale, s, x, y;
    MENU_PIC *pic;

    if (develop_pic == nullptr)
        return 0;
    scale = (float)(int)args->stack[2] * (1.0f / 128.0f);   /* sdata 0x370448 */
    develop_pic->scale = scale;
    develop_pic->fix   = 1;
    pic = develop_pic;
    pic->Draw();
    pic->DrawNext();
    develop_pic_done->scale = scale;
    develop_pic_done->fix   = 1;
    pic = develop_pic_done;
    pic->Draw();
    pic->DrawNext();
    if (develop_pic_done->fade >= 0 && (u_int)DevelopGetStep() >= DEVELOP_STEP_ASK) {
        pic = develop_pic_done;
        s = Vu0Sinf((float)(int)((float)pic->GetFade() * pic->scale) * 3.1415927f  /* sdata 0x37044c */
                    * (1.0f / 128.0f));
        x = (float)develop_pic_done->x;
        y = (float)develop_pic_done->y;
        SubFocus(x, y, (float)(develop_pic_done->x + w), (float)(develop_pic_done->y + h),
                 (float)w, (float)h, x, y, (int)(s * 70.0f));    /* sdata 0x370450 */
    }
    SetDefaultGsDrawState();
    return 0;
}

// ──────────────────────────────────────────────────────────────────────
// The strip: the item positions and a 0x4a x 0x32 thumbnail per film.
// FF3 0x001b4768
// callcheck: BpCtlGetHandleByFileNo (DevelopHandle)
// callcheck: operator_new_array (new MENU_PIC *[])
// callcheck: operator_new, MenuPicCtor (new MENU_PIC)

static void DevelopThumbCreate(void)
{
    int       handle;
    B2D_ITEM *item;
    int       i;

    if (DevelopListGetNum() <= 0)
        return;
    handle = DevelopHandle();
    item = B2dCtlFindItem(handle, 0x3ff);
    develop_strip.x       = B2dSprGetPartData(item, B2D_PART_X);
    develop_strip.y       = B2dSprGetPartData(item, B2D_PART_Y);
    item = B2dCtlFindItem(handle, 0x3f3);
    develop_strip.frame_x = B2dSprGetPartData(item, B2D_PART_X);
    develop_strip.frame_y = B2dSprGetPartData(item, B2D_PART_Y);
    develop_strip.dy      = B2dSprGetPartData(item, B2D_PART_H);
    develop_strip.scroll  = 0.0f;
    develop_strip.num     = DevelopListGetNum();
    develop_strip.pic     = new MENU_PIC *[develop_strip.num];
    for (i = 0; i < develop_strip.num; i++) {
        MENU_PIC *pic = new MENU_PIC(0);

        develop_strip.pic[i] = pic;
        pic->v      = 0;
        pic->u      = 0;
        pic->h      = 0x32;
        pic->w      = 0x4a;
        pic->enable = 1;
    }
}

// FF3 0x001b48f0
// callcheck: MenuPicDtor, operator_delete (delete MENU_PIC)
// callcheck: operator_delete (delete[] of the array; the ROM frees it with operator_delete)
static void DevelopThumbDelete(void)
{
    int i;

    if (develop_strip.pic == nullptr)
        return;
    for (i = 0; i < develop_strip.num; i++) {
        if (develop_strip.pic[i] != nullptr)
            delete develop_strip.pic[i];
        develop_strip.pic[i] = nullptr;
    }
    delete[] develop_strip.pic;
    develop_strip.pic = nullptr;
}

// ──────────────────────────────────────────────────────────────────────
// The strip scrolls a quarter of the way to the cursor every frame (the
// last pixel at once); while browsing each thumbnail shows its film,
// developed or not. FF3 0x001b49a8

static void DevelopThumbStep(void)
{
    float target, step;
    int   i, y;

    if (DevelopListGetNum() <= 0 || develop_strip.pic == nullptr)
        return;
    target = (float)(DevelopCsrGetPos() * develop_strip.dy);
    step   = (target - develop_strip.scroll) * 0.25f;               /* sdata 0x370454 */
    if (-1.0f < step && step < 1.0f)                                /* sdata 0x370458 / 0x37045c */
        develop_strip.scroll = target;
    else
        develop_strip.scroll += step;

    for (i = 0; i < develop_strip.num; i++) {
        MENU_PIC *pic;

        y = i * develop_strip.dy - (int)(develop_strip.scroll + 0.5f) + develop_strip.y;   /* sdata 0x370460 */
        if ((u_int)DevelopGetStep() < DEVELOP_STEP_ASK) {
            short film = DevelopListGet((short)i);

            develop_strip.pic[i]->pic =
                (short)(develop_dat[film].menu_pic + (develop_film[film].state != DEVELOP_STATE_DONE ? 2 : 3));
        }
        pic = develop_strip.pic[i];
        pic->x   = (short)develop_strip.x;
        pic->y   = (short)y;
        pic->pri = 0xa0;
        pic->z   = 0;
        pic->Step();
    }
}

// The current film's thumbnail; nullptr without films (the ROM reads
// address 0 then; phase 65 only runs for a film).
static MENU_PIC *DevelopThumbCur(void)
{
    if (DevelopListGetNum() > 0 && develop_strip.pic != nullptr)
        return develop_strip.pic[DevelopCsrGetPos()];
    return nullptr;
}

// ──────────────────────────────────────────────────────────────────────
// Item 0x3f3: the film frame sprite, five rows more than there are films,
// down the scrolled strip (rows on screen only). FF3 0x001b4bc0

static int DevelopFrameHook(const B2D_HOOK_ARGS *args)
{
    B2D_HOOK_ARGS a = *args;
    int i, y;

    if (DevelopListGetNum() <= 0 || develop_strip.pic == nullptr)
        return 0;
    for (i = 0; i < develop_strip.num + 5; i++) {
        y = i * develop_strip.dy - (int)(develop_strip.scroll + 0.5f) + develop_strip.frame_y;   /* sdata 0x370464 */
        if (y < -0x1f || y >= 0x1a0 - develop_strip.dy)
            continue;
        a.t1 = develop_strip.frame_x;
        a.t2 = y;
        B2dDrawSpriteDirect(&a);
    }
    SetDefaultGsDrawState();
    return 0;
}

// ──────────────────────────────────────────────────────────────────────
// Items 0x3ff / 0x402 (box items: stack[2] alpha, stack[3] blend flags):
// the thumbnails of the films not developed / developed, rows on screen
// only (tested against the frame column's y, as the ROM does).
// FF3 0x001b4d20 (done 0) / 0x001b4e88 (done 1)

static void DevelopThumbDraw(const B2D_HOOK_ARGS *args, int done)
{
    float scale;
    int   i, y;

    if (DevelopListGetNum() <= 0 || develop_strip.pic == nullptr)
        return;
    BpSetBlendAlpha((u_int)args->stack[3]);
    scale = (float)(int)args->stack[2] * (1.0f / 128.0f);   /* sdata 0x370468 / 0x370470 */
    for (i = 0; i < develop_strip.num; i++) {
        short film = DevelopListGet((short)i);

        if ((develop_film[film].state == DEVELOP_STATE_DONE) != (done != 0))
            continue;
        y = i * develop_strip.dy - (int)(develop_strip.scroll + 0.5f) + develop_strip.frame_y;   /* sdata 0x37046c / 0x370474 */
        if (y < -0x1f || y >= 0x1a0 - develop_strip.dy)
            continue;
        develop_strip.pic[i]->scale = scale;
        develop_strip.pic[i]->Draw();
        develop_strip.pic[i]->DrawNext();
    }
    SetDefaultGsDrawState();
}

// FF3 0x001b4d20
// callcheck: BpSetBlendAlpha, DevelopListGet, DevelopListGetNum, MenuPicDraw, MenuPicDrawNext, SetDefaultGsDrawState (DevelopThumbDraw)
static int DevelopThumbHook(const B2D_HOOK_ARGS *args)
{
    DevelopThumbDraw(args, 0);
    return 0;
}

// FF3 0x001b4e88
// callcheck: BpSetBlendAlpha, DevelopListGet, DevelopListGetNum, MenuPicDraw, MenuPicDrawNext, SetDefaultGsDrawState (DevelopThumbDraw)
static int DevelopThumbDoneHook(const B2D_HOOK_ARGS *args)
{
    DevelopThumbDraw(args, 1);
    return 0;
}

// FF3 0x001b4ff0: empty (jr ra), called by DevelopMenuUpdate.
static void DevelopMenuDummy(void)
{
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x001b4ff8 / 0x001b5028: a film to develop before / after the cursor.

static int DevelopCanScrollUp(void)
{
    int pos = DevelopCsrGetPos();

    return DevelopFindPrev(pos - 1) < pos;
}

// FF3 0x001b5028
static int DevelopCanScrollDown(void)
{
    int pos = DevelopCsrGetPos();

    return pos < DevelopFindNext(pos + 1);
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x001b5060
// callcheck: BpCtlGetHandleByFileNo (DevelopHandle)

static void DevelopArrowDisp(void)
{
    int handle = DevelopHandle();
    int remain = DevelopMenuHasRemain();
    int up     = 0;
    int down   = 0;

    if (remain != 0)
        up = DevelopCanScrollUp() != 0;
    if (remain != 0)
        down = DevelopCanScrollDown() != 0;
    MenuScrollArrowDisp(handle, 0x400, 0x3fb, 0x401, 0x3fc, up, down);
}

// ──────────────────────────────────────────────────────────────────────
// Item 0x3fe (a box item: t1 its quad, t3 colour, stack[2] alpha, stack[3]
// flags): the title of the film's document centred on the item -- the new
// document once developed, except while phase 65 develops it.
// FF3 0x001b50e0

static int DevelopTitleHook(const B2D_HOOK_ARGS *args)
{
    const int *xy   = (const int *)args->t1;
    short      film = DevelopCsrGetFilm();
    int        after;
    u_short    doc;
    int        pos[2];

    if ((u_short)film < DEVELOP_FILM_NUM) {
        after = 0;
        if (GetGPhaseCtrl()->leaf->id != PHASE_STORY_DEVELOP_FILM
            && develop_film[film].state == DEVELOP_STATE_DONE)
            after = 1;
        doc = develop_dat[film].doc[after];
        if (doc < MSG_PIC_KEY_NUM) {
            pos[0] = (xy[0] + xy[6]) / 2;
            pos[1] = xy[1];
            B2dHookPrintMsgLine(DocGetMsgType((short)doc, 0), msg_pic_key_dat[(short)doc].msg1, 2, pos,
                                (u_int)args->t3, (int)args->stack[2], (u_int)args->stack[3]);
        }
    }
    SetDefaultGsDrawState();
    return 0;
}

// ──────────────────────────────────────────────────────────────────────
// Phase 65 Story_DevelopFilm
// ──────────────────────────────────────────────────────────────────────

// FF3 0x001b5220. Step 0: wait for the pictures' fades; 1: the developed
// picture (menu_pic + 1) and the strip thumbnail (+ 3) dissolve in over 150
// frames once loaded; 2: when both are loaded they start, with the SE.
// callcheck: DevelopListGetNum, DevelopCsrGetPos (DevelopThumbCur, inline in the ROM)

int init_Story_DevelopFilm(void)
{
    MENU_PIC *thumb;
    short     pic;

    switch (GetGPhaseCtrl()->leaf->state_cnt) {
    case 0:
        if (develop_pic_done->fade >= 0)
            break;
        thumb = DevelopThumbCur();
        if (thumb != nullptr && thumb->fade >= 0)   // PC: the ROM reads address 0x4c without films
            break;
        GetGPhaseCtrl()->leaf->state_cnt = 1;
        break;

    case 1:
        pic = develop_dat[DevelopCsrGetFilm()].menu_pic;
        develop_pic_done->enable   = 0;
        develop_pic_done->fade_len = 0x96;
        develop_pic_done->mode     = 1;
        develop_pic_done->fix      = 0;
        develop_pic_done->fade_max = 0xff;
        develop_pic_done->fade_min = 0;
        develop_pic_done->pic      = (short)(pic + 1);
        thumb = DevelopThumbCur();
        if (thumb != nullptr) {                     // PC: the ROM writes through address 0 without films
            thumb->enable   = 0;
            thumb->fade_len = 0x96;
            thumb->pic      = (short)(pic + 3);
        }
        develop_film_wait = 0;
        GetGPhaseCtrl()->leaf->state_cnt = 2;
        break;

    case 2:
        if (develop_pic_done->IsNextReady() == 0)
            break;
        thumb = DevelopThumbCur();
        if (thumb != nullptr && thumb->IsNextReady() == 0)
            break;
        develop_pic_done->enable = 1;
        if (thumb != nullptr)
            thumb->enable = 1;
        develop_film_se = SndSePlay(DEVELOP_SE_DEVELOP, 100, 0x1000, 0, 0, 0);
        return 1;
    }
    DevelopMenuUpdate();
    return 0;
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x001b55c8: back to phase 38 when the developed picture is in; after
// half a second OK / cancel cut it short.
// callcheck: DevelopListGetNum, DevelopCsrGetPos (DevelopThumbCur, inline in the ROM)

int one_Story_DevelopFilm(void)
{
    DevelopMenuUpdate();
    if (develop_pic_done->fade < 0) {
        GPhaseCtrlRequest(PHASE_STORY_DEVELOP, 0);
        return 1;
    }
    if (develop_film_wait < 0x1e) {
        develop_film_wait++;
        return 0;
    }
    ReiPan_MenuMouseConfirm();          // PC: a click goes on
    if (CheckPadTrgPlaySe(pad_key[PAD_KEY_OK], -1, 0) == 0
        && CheckPadTrgPlaySe(pad_key[PAD_KEY_CANCEL], -1, 0) == 0)
        return 0;
    develop_pic_done->CutFade();
    if (DevelopThumbCur() != nullptr)
        DevelopThumbCur()->CutFade();
    SndBufFadeStop(develop_film_se, 10);
    return 0;
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x001b5538: the pictures back to 8-frame cross-fades.
// callcheck: DevelopListGetNum, DevelopCsrGetPos (DevelopThumbCur, inline in the ROM)

int end_Story_DevelopFilm(void)
{
    MENU_PIC *thumb;

    develop_pic_done->fade_len = 8;
    develop_pic_done->mode     = 0;
    develop_pic_done->fade_max = 0x80;
    develop_pic_done->fade_min = 0;
    thumb = DevelopThumbCur();
    if (thumb != nullptr)
        thumb->fade_len = 8;
    develop_count++;
    return 1;
}

// ──────────────────────────────────────────────────────────────────────
// The window
// ──────────────────────────────────────────────────────────────────────

// FF3 0x001b56b8: the window's alpha follows item 0x421.
// callcheck: BpCtlGetHandleByFileNo (DevelopHandle)
static void DevelopWdwShow(void)
{
    LayoutAnimSetAlphaItemById(develop_wdw_anim, DevelopHandle(), 0x421);
}

// ──────────────────────────────────────────────────────────────────────
// The film's message (type 2): msg up to the question, msg + 1 after
// developing, none otherwise. FF3 0x001b56e0 (a 10-entry jump table at
// rodata 0x359600)

static void DevelopWdwSetMsg(void)
{
    short film = DevelopCsrGetFilm();
    int   step;

    if (film != -1) {
        step = DevelopGetStep();
        if ((u_int)step <= DEVELOP_STEP_RESULT) {
            Anim2dMsgSet(2, develop_dat[film].msg);
            return;
        }
        if (step == DEVELOP_STEP_MSG || step == DEVELOP_STEP_MSG_WAIT) {
            Anim2dMsgSet(2, (short)(develop_dat[film].msg + 1));
            return;
        }
    }
    Anim2dMsgSet(-1, -1);
}

// ──────────────────────────────────────────────────────────────────────
// The layout steps (see DEVELOP_LAYOUT). FF3 0x002b9210 (out of line; the
// hook list, rodata 0x359628, is copied onto the stack)
// callcheck: BpCtlGetHandleByFileNo (DevelopHandle)

static void DevelopLayoutStep(void)
{
    static const B2D_HOOK_REC hooks[6] =
    {
        { DevelopPicHook,       0,  0x41f, nullptr },
        { DevelopFrameHook,     0,  0x3f3, nullptr },
        { DevelopThumbHook,     0,  0x3ff, nullptr },
        { DevelopThumbDoneHook, 0,  0x402, nullptr },
        { DevelopTitleHook,     0,  0x3fe, nullptr },
        { nullptr,              -1, -1,    nullptr },
    };

    switch (develop_layout_step) {
    case DEVELOP_LAYOUT_LOAD:
        BpCtlLoadReq(GENZOU_PHF, nullptr);
        develop_wdw_anim = LayoutAnimStart(9, 0);
        LayoutAnimSetFlag(develop_wdw_anim, LAYOUT_ANIM_WAIT | LAYOUT_ANIM_LABEL_LAYOUT);
        LayoutMsgSetKeepOpen(1);
        Anim2dMsgSetType(2);
        develop_layout_step++;
        // fall through
    case DEVELOP_LAYOUT_WAIT:
        BpCtlMain();
        if (BpCtlIsDataReady(DevelopHandle()) != 0 && LayoutAnimIsDataReady(develop_wdw_anim) != 0)
            develop_layout_step++;
        return;

    case DEVELOP_LAYOUT_OPEN:
        LayoutAnimClearFlag(develop_wdw_anim, LAYOUT_ANIM_WAIT);
        BpPlayLabel(DevelopHandle(), "in");                     /* sdata 0x370440 */
        GetCmnFadeLayer()->SetItem(DevelopHandle(), 0x3f6);
        GetCmnFadeLayer()->Show(DevelopFadeLabel());
        LayoutAnimSetAlpha(develop_wdw_anim, 0x80);
        DevelopWdwShow();
        DevelopWdwSetMsg();
        B2dSetHookList(DevelopHandle(), hooks);
        develop_layout_step++;
        // fall through
    case DEVELOP_LAYOUT_IN:
        ScreenCaptureDisp(0x80, 0x80, 0x80, 0x80);
        FadeMain();
        DevelopMenuUpdate();
        if (MenuLayoutIsPlaying(GENZOU_PHF) != 0)
            return;
        develop_layout_step++;
        return;

    case DEVELOP_LAYOUT_MENU:
        DevelopMenuUpdate();
        return;

    case DEVELOP_LAYOUT_CLOSE:
        BpPlayLabel(DevelopHandle(), "out");                    /* sdata 0x370478 */
        develop_layout_step++;
        // fall through
    case DEVELOP_LAYOUT_OUT:
        ScreenCaptureDisp(0x80, 0x80, 0x80, 0x80);
        FadeMain();
        DevelopMenuUpdate();
        if (MenuLayoutIsPlaying(GENZOU_PHF) != 0)
            return;
        LayoutAnimSetAlphaItem(develop_wdw_anim, nullptr);
        LayoutMsgSetKeepOpen(0);
        LayoutAnimReset(develop_wdw_anim);
        GetCmnFadeLayer()->SetItemHandle(nullptr);
        GetCmnFadeLayer()->Term();
        BpCtlRelease(DevelopHandle());
        develop_layout_step++;
        return;
    }
}
