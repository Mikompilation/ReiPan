/* ==========================================================================
 *  ingame/menu/menu_item.c
 *
 *  The item menu, phase 59 StoryMenu_Item; see menu_item.h.
 *  FF3 0x1eac20..0x1ecd30 + 0x2bccd8 (ItemMenuListFill).
 *
 *  The menu keeps one ITEM_MENU_LIST and one cursor (MENU_CSR_WRK, 1 x n,
 *  a 7-row view) per tab and a step (ItemMenuMain): 0 / 1 pass through to
 *  2, browse (cursor, the page keys a page when the tab has more than 7
 *  items, the tab keys the other tab, OK the item, cancel back to the story
 *  menu); 3 / 4 the "use it?" question (yes uses one), 5 / 6 the reason an
 *  item cannot be used, 7 / 8 "no items" when neither tab has any. Drawn by
 *  the layout's item hooks -- the names (item 0x406), the counts on tabA
 *  (0x40d), the two pictures (0x419, MENU_PIC), the bilinear "R wasi" -- plus
 *  a scroll bar and the row cursor stepped every frame (ItemMenuUpdate).
 *  The menu also runs the HUD layout (HUD_LAYOUT, opened with it).
 *
 *  Not written: FF3 0x001ecd10 (ItemMenuAtexitGPhaseCtrl), the GPhaseCtrl
 *  singleton's atexit thunk (as in every file that includes the getter).
 * ======================================================================== */
#include "ingame/menu/menu_item.h"

#include "common/cmn_fade_layer.h"      // GetCmnFadeLayer
#include "common/heapctrl.h"            // heapCtrlMalloc, heapCtrlFree
#include "graphics/graph2d/message.h"   // GetMsgDataAddr, PrintMsg_ArrangeOneLine, PrintNumber_N, SetCurColorRGB
#include "graphics/gs_reg.h"            // SetGsRegCached
#include "graphics/motion/anicode.h"    // MenuWdw*, B2D_HOOK_REC, B2dSetHookList, SetDefaultGsDrawState
#include "ingame/ingame.h"              // GPhaseCtrlRequest
#include "ingame/ingame_wrk.h"          // ingame_wrk (the world)
#include "ingame/item/prg/item.h"       // item_dat, ItemGetCnt, ItemCanUse, ItemUse, ItemGetWorldKind
#include "ingame/menu/anim_2d.h"        // LayoutAnim*, LayoutMsg*, HUD_LAYOUT
#include "ingame/menu/menu_bg.h"        // MenuGetTopPhase, MenuLayoutIsPlaying, MenuScrollArrowDisp
#include "ingame/menu/menu_csr_obj.h"   // MenuCsrObj*
#include "ingame/menu/menu_draw_tex.h"  // MENU_PIC
#include "ingame/photo/finder_photo.h"  // GetPhotoModeWrk, GetFilmWrk
#include "ingame/photo/fndr_parts.h"    // FilmGetType
#include "ingame/plyr/player.h"         // PlyrGetHpRate
#include "ingame/sound/snd_ctrl.h"      // SndSePlay
#include "main/gphase.h"                // GetGPhaseCtrl
#include "sdk/libgraph.h"               // SCE_GS_TEX1_1
#include "system/b2d/B2dCtl.h"          // B2dCtlFindItem, B2dCtlSetItemHook
#include "system/b2d/B2dGridSpr.h"      // B2D_GRID_SPR
#include "system/b2d/B2dSpr.h"          // B2dSprGetPartData
#include "system/b2d/BpCtl.h"           // BpCtl*, BpPlayLabel, B2dDrawSpriteDirect
#include "system/b2d/ScrollBar.h"       // B2d_ScrollBar3
#include "system/eeiop/cd_file_dat.h"   // ITEM_PHF
#include "system/os/sys_heap.h"         // GetSystemHeapWrkP
#include "system/os/system.h"           // GetSysWrk, SYS_WRK::GetLanguage
#include "system/pad/pad_ctrl.h"        // pad_key, CheckPadTrgPlaySe, CheckPadRptPlaySe
#include "ingame/menu/menu_mouse.h"    // PC: the mouse in menus

#define ITEM_VIEW_ROWS      7           // rows of the list shown at once
#define ITEM_ROW_DY         0x1e        // pixels between rows
#define ITEM_MSG_NAME       0x56        // message kind of the names (item_dat._0e)
#define ITEM_MSG_DESC       0x55        // message kind of the descriptions (item_dat._0e)
#define ITEM_MSG_MENU       0x61        // LayoutMsg: the questions and reasons (ITEM_ASK_*)
#define ITEM_LIST_MENU_ALL  0xf         // ITEM_MENU_LIST.menu of both lists

#define ITEM_ITEM_ALPHA     0x403       // layout item: the fade layer's and the window's alpha
#define ITEM_ITEM_PIC       0x419       // the pictures
#define ITEM_ITEM_NAME      0x406       // the names
#define ITEM_ITEM_CNT       0x40d       // the counts

#define ITEM_SE_MOVE        0x1a9b0000  // cursor moved
#define ITEM_SE_CANCEL      0x1a9b0001
#define ITEM_SE_BUZZER      0x1a9b0002
#define ITEM_SE_OK          0x1a9b0003
#define ITEM_SE_PAGE        0x1a9b0005  // tab / page changed

// LayoutMsg (0x61, n).
enum ITEM_ASK
{
    ITEM_ASK_CANNOT    = 0,             // cannot be used (ItemMenuUseCheck 1)
    ITEM_ASK_FILM      = 1,             // load this film?
    ITEM_ASK_USE       = 2,             // use it?
    ITEM_ASK_NOT_NOW   = 3,             // not now: no finder / full health (2)
    ITEM_ASK_LOADED    = 4,             // the film is loaded already (3)
    ITEM_ASK_EMPTY     = 5,             // no items
};

// The menu's steps (ItemMenuMain).
enum ITEM_MENU_STEP
{
    ITEM_STEP_START      = 0,           // set by cancel; passes to 2
    ITEM_STEP_START2     = 1,           // after a message; passes to 2
    ITEM_STEP_MAIN       = 2,           // browse
    ITEM_STEP_ASK        = 3,           // "use it?"
    ITEM_STEP_ASK_WAIT   = 4,
    ITEM_STEP_NG         = 5,           // why it cannot be used
    ITEM_STEP_NG_WAIT    = 6,
    ITEM_STEP_EMPTY      = 7,           // no items in either tab
    ITEM_STEP_EMPTY_WAIT = 8,
    ITEM_STEP_NUM        = 9,
};

static void  ItemMenuUpdate(void);
static short ItemMenuMain(void);
static void  ItemMenuInit(void);
static void  ItemMenuRelease(void);
static int   ItemMenuGetStep(void);
static int   ItemMenuSetStep(u_int step);
static int   ItemMenuGetTab(void);
static int   ItemMenuTabWrap(int tab);
static int   ItemMenuSetTab(int tab);
static int   ItemMenuTabChanged(void);
static int   ItemMenuFirstTab(void);
static void  ItemMenuListNew(int tab);
static void  ItemMenuListDelete(int tab);
static void  ItemMenuListRebuild(int tab);
static short ItemMenuListGetNum(int tab);
static short ItemMenuListGet(int tab, short i);
static short ItemMenuListGetCnt(int tab, short i);
static int   ItemMenuUseCheck(int tab, short i);
static int   ItemMenuCanUse(int tab, short i);
static void  ItemMenuUse(int tab, short i);
static void  ItemMenuCsrInit(int tab);
static void  ItemMenuCsrRelease(int tab);
static int   ItemMenuCsrUpdate(int tab);
static int   ItemMenuCsrPagePrev(int tab);
static int   ItemMenuCsrPageNext(int tab);
static void  ItemMenuCsrReset(int tab);
static short ItemMenuCsrGetIndex(int tab);
static short ItemMenuCsrGetPosY(int tab);
static short ItemMenuCsrGetTopY(int tab);
static void  ItemMenuPicCreate(void);
static void  ItemMenuPicDelete(void);
static void  ItemMenuBarCreate(void);
static void  ItemMenuBarDelete(void);
static void  ItemMenuBarDraw(void);
static void  ItemMenuCslCreate(void);
static void  ItemMenuCslDelete(void);
static void  ItemMenuCslUpdate(void);
static void  ItemMenuSetHooks(void);
static void  ItemMenuResetHooks(void);
static void  ItemMenuWdwSetMsg(void);

// ──────────────────────────────────────────────────────────────────────
// ITEM_MENU_LIST
// FF3 0x001eac20: the base-object constructor, the same body as
// FF3 0x001eac60 (ItemMenuListCtor, the complete-object one)

ITEM_MENU_LIST::ITEM_MENU_LIST(short type, short world, short menu)
{
    num         = 0;
    list        = nullptr;
    this->type  = (u_short)type;
    this->world = (u_short)world;
    this->menu  = (u_short)menu;
    Build();
}

// FF3 0x001eaca0: the base-object destructor, the same body as
// FF3 0x001eacb8 (ItemMenuListDtor)
ITEM_MENU_LIST::~ITEM_MENU_LIST()
{
    Free();
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x001eacd0

void ITEM_MENU_LIST::Build()
{
    short n = Fill(0);

    if (n != 0) {
        list = (short *)heapCtrlMalloc(GetSystemHeapWrkP(), n * sizeof(short), 0, 0, 0);
        num  = Fill(1);
    }
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x001ead40

void ITEM_MENU_LIST::Free()
{
    if (list != nullptr) {
        heapCtrlFree(GetSystemHeapWrkP(), list, 0, 0, 0);
        list = nullptr;
    }
    num = 0;
}

// ──────────────────────────────────────────────────────────────────────
// The items held (count > 0) that match the masks; with store (and a list)
// written into it.
// FF3 0x002bccd8 (out of line; ItemMenuListFill)

short ITEM_MENU_LIST::Fill(int store)
{
    short n = 0;
    short i;

    for (i = 0; i < ITEM_NUM; i++) {
        const ITEM_DAT *dat = &item_dat[i];

        if (*ItemGetCnt(i) <= 0)
            continue;
        if (type != 0xffff && (dat->type & type) == 0)
            continue;
        if (world != 0xffff && (dat->world & world) == 0)
            continue;
        if (menu != 0xffff && (dat->menu & menu) == 0)
            continue;
        if (store != 0 && list != nullptr)
            list[n] = i;
        n++;
    }
    return n;
}

// ──────────────────────────────────────────────────────────────────────
// The menu
// ──────────────────────────────────────────────────────────────────────

// The tabs' ITEM_TYPE_* masks (sdata 0x370e00).
static const short item_menu_list_type[ITEM_MENU_TAB_NUM] = { 0x0f, 0x30 };

// The tabs' labels (sdata 0x370dd0 / 0x370dd8 / 0x370df0).
static const char *const item_menu_label_in[ITEM_MENU_TAB_NUM]  = { "tabA_in", "tabB_in" };
static const char *const item_menu_label_out[ITEM_MENU_TAB_NUM] = { "tabA_out", "tabB_out" };
static const char *const item_menu_label_tab[ITEM_MENU_TAB_NUM] = { "tabA", "tabB" };

static int             item_menu_step;                  // bss 0x3ef2a0: ITEM_MENU_STEP
static int             item_menu_tab;                   // bss 0x3ef2a4
static int             item_menu_tab_old;               // bss 0x3ef2a8: ItemMenuTabChanged
static MENU_CSR_WRK    item_menu_csr[ITEM_MENU_TAB_NUM];    // bss 0x3ef2ac (0x3c each)
static ITEM_MENU_LIST *item_menu_list[ITEM_MENU_TAB_NUM];   // bss 0x3ef324
static int             item_menu_wdw_anim;              // sdata 0x370df8: the menu window (layout type 8)
static int             item_menu_msg_anim;              // sdata 0x370dfc: the question / reason (type 3)
static HUD_LAYOUT     *item_menu_hud;                   // sbss gp-0xd4
static MENU_PIC       *item_menu_pic[2];                // sbss gp-0xd0: the picture, its additive glow
static B2d_ScrollBar3 *item_menu_bar;                   // sbss gp-0xc8
static B2D_GRID_SPR   *item_menu_csl;                   // sbss gp-0xc4: the row cursor

// The menu layout (ITEM_PHF + language), as every use computes it.
static int ItemMenuFile(void)
{
    return GetSysWrk()->GetLanguage() + ITEM_PHF;
}

static int ItemMenuHandle(void)
{
    return BpCtlGetHandleByFileNo(ItemMenuFile());
}

// ──────────────────────────────────────────────────────────────────────
// Phase 59 StoryMenu_Item
// ──────────────────────────────────────────────────────────────────────

// FF3 0x001eadd0. Step 0: lists and cursors, the layout, the menu window
// and the HUD layout start loading; step 1: once all three are loaded, the
// HUD's label 0, the fade layer and window alpha on item 0x403, the window
// caption, the tab's "in" ("in_item0" and fade label 2 when the menu is
// empty), the pictures, scroll bar, row cursor and item hooks; step 2 runs
// the menu until the way in has played.
// callcheck: GetSysWrk, GetLanguage, BpCtlGetHandleByFileNo (ItemMenuFile / ItemMenuHandle)
// callcheck: operator_new, HudLayoutInit (new HUD_LAYOUT)

int init_StoryMenu_Item(void)
{
    int handle;

    switch (GetGPhaseCtrl()->leaf->state_cnt) {
    case 0:
        BpCtlMain();
        ItemMenuInit();
        BpCtlLoadReq(ItemMenuFile(), nullptr);
        item_menu_wdw_anim = LayoutAnimStart(8, 0);
        LayoutAnimSetFlag(item_menu_wdw_anim, LAYOUT_ANIM_WAIT | LAYOUT_ANIM_LABEL_LAYOUT);
        MenuWdwSetKeepOpen(1);
        item_menu_hud = new HUD_LAYOUT(1);
        item_menu_hud->a2 = 0x16;
        item_menu_hud->a1 = 0;
        GetGPhaseCtrl()->leaf->state_cnt = 1;
        return 0;

    case 1:
        BpCtlMain();
        if (BpCtlIsDataReady(ItemMenuHandle()) == 0)
            return 0;
        if (LayoutAnimIsDataReady(item_menu_wdw_anim) == 0)
            return 0;
        if (item_menu_hud->IsReady() == 0)
            return 0;
        item_menu_hud->Play(0);
        LayoutAnimClearFlag(item_menu_wdw_anim, LAYOUT_ANIM_WAIT);
        handle = ItemMenuHandle();
        GetCmnFadeLayer()->SetItem(handle, ITEM_ITEM_ALPHA);
        LayoutAnimSetAlphaItemById(item_menu_wdw_anim, handle, ITEM_ITEM_ALPHA);
        ItemMenuWdwSetMsg();
        if (ItemMenuGetStep() == ITEM_STEP_EMPTY) {
            BpPlayLabel(ItemMenuHandle(), "in_item0");          /* rodata 0x35b1a8 */
            GetCmnFadeLayer()->Show(2);
        } else {
            BpPlayLabel(ItemMenuHandle(), item_menu_label_in[ItemMenuGetTab()]);
            GetCmnFadeLayer()->Show(1);
        }
        ItemMenuPicCreate();
        ItemMenuBarCreate();
        ItemMenuCslCreate();
        ItemMenuSetHooks();
        GetGPhaseCtrl()->leaf->state_cnt = 2;
        return 0;

    case 2:
        ItemMenuUpdate();
        return MenuLayoutIsPlaying(ItemMenuFile()) == 0;
    }
    return 1;
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x001eb108: the tab's "out" ("out_item0" after "no items") and the
// HUD closes; once the layout has played everything is released. The ROM
// answers 1 without releasing anything when the layout has stopped but the
// HUD is not HUD_LAYOUT_DONE yet (kept: HudLayoutClose of a loaded HUD
// plays its label, so it can still be closing).
// callcheck: GetSysWrk, GetLanguage, BpCtlGetHandleByFileNo (ItemMenuFile / ItemMenuHandle)
// callcheck: HudLayoutRelease, operator_delete (delete item_menu_hud)

int end_StoryMenu_Item(void)
{
    ItemMenuUpdate();
    switch (GetGPhaseCtrl()->leaf->state_cnt) {
    case 0:
        if (ItemMenuGetStep() == ITEM_STEP_EMPTY_WAIT)
            BpPlayLabel(ItemMenuHandle(), "out_item0");         /* rodata 0x35b1b8 */
        else
            BpPlayLabel(ItemMenuHandle(), item_menu_label_out[ItemMenuGetTab()]);
        item_menu_hud->Close();
        GetGPhaseCtrl()->leaf->state_cnt = 1;
        return 0;

    case 1:
        if (MenuLayoutIsPlaying(ItemMenuFile()) != 0)
            return 0;
        if (item_menu_hud->state != HUD_LAYOUT_DONE)
            return 1;
        delete item_menu_hud;
        item_menu_hud = nullptr;
        LayoutAnimSetAlphaItem(item_menu_wdw_anim, nullptr);
        MenuWdwSetKeepOpen(0);
        LayoutAnimReset(item_menu_wdw_anim);
        ItemMenuPicDelete();
        ItemMenuBarDelete();
        ItemMenuCslDelete();
        ItemMenuResetHooks();
        GetCmnFadeLayer()->SetItemHandle(nullptr);
        BpCtlRelease(ItemMenuHandle());
        ItemMenuRelease();
        return 1;
    }
    return 1;
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x001eb310

int one_StoryMenu_Item(void)
{
    short next;

    ItemMenuUpdate();
    next = ItemMenuMain();
    if (next != -1)
        GPhaseCtrlRequest(next, 0);
    return next != -1;
}

// ──────────────────────────────────────────────────────────────────────
// Every frame of phase 59: the tab's label when the tab changed, the row
// cursor and scroll bar, the fade layer, the layout animations, the HUD
// and the Bp layouts.
// FF3 0x001eb358
// callcheck: GetSysWrk, GetLanguage, BpCtlGetHandleByFileNo (ItemMenuHandle)

static void ItemMenuUpdate(void)
{
    int tab = ItemMenuGetTab();

    if (ItemMenuTabChanged() != 0)
        BpPlayLabel(ItemMenuHandle(), item_menu_label_tab[tab]);
    ItemMenuCslUpdate();
    ItemMenuBarDraw();
    GetCmnFadeLayer()->Update();
    LayoutAnimMain();
    if (item_menu_hud != nullptr)
        item_menu_hud->Main();
    BpCtlMain();
}

// ──────────────────────────────────────────────────────────────────────
// The steps (see the file comment); returns the phase to go to, -1 none.
// FF3 0x001eb408

static short ItemMenuMain(void)
{
    int   tab   = ItemMenuGetTab();
    short index = ItemMenuCsrGetIndex(tab);
    short item  = ItemMenuListGet(tab, index);
    short next  = -1;
    int   page;

    switch (ItemMenuGetStep()) {
    case ITEM_STEP_START:
        ItemMenuSetStep(ITEM_STEP_START2);
        // fall through
    case ITEM_STEP_START2:
        ItemMenuSetStep(ITEM_STEP_MAIN);
        // fall through
    case ITEM_STEP_MAIN:
        ItemMenuWdwSetMsg();
        page = ItemMenuListGetNum(tab) > ITEM_VIEW_ROWS;
        // PC: the mouse. The two tabs are one picture; the first ends 138 in.
        {
            ReiPan_MenuBox strip;

            if (ReiPan_MenuMouseNameBox(ItemMenuHandle(), tab == 0 ? "tabA" : "tabB", &strip))
            {
                const ReiPan_MenuBox first  = { strip.x, strip.y, 138.0f, strip.h };
                const ReiPan_MenuBox second = { strip.x + 138.0f, strip.y, strip.w - 138.0f, strip.h };

                MenuMouseTab(tab, 0, &first, REIPAN_MENU_LEFT, REIPAN_MENU_LEFT);
                MenuMouseTab(tab, 1, &second, REIPAN_MENU_LEFT, REIPAN_MENU_LEFT);
            }
        }
        if (ItemMenuCsrUpdate(tab) != 0)
            return -1;
        if (page) {
            if (CheckPadRptPlaySe(pad_key[PAD_KEY_PAGE_PREV], -1, 0)) {
                SndSePlay(ITEM_SE_PAGE, 100, 0x1000, 0, 0, 0);
                ItemMenuCsrPagePrev(tab);
                return -1;
            }
            if (CheckPadRptPlaySe(pad_key[PAD_KEY_PAGE_NEXT], -1, 0)) {
                SndSePlay(ITEM_SE_PAGE, 100, 0x1000, 0, 0, 0);
                ItemMenuCsrPageNext(tab);
                return -1;
            }
        }
        // Either tab key (or left / right) goes to the other tab, if it
        // has items. The ROM steps +1 on TAB_PREV and -1 on TAB_NEXT, the
        // same tab with two.
        if (CheckPadRptPlaySe(pad_key[PAD_KEY_TAB_PREV] | 0x8000, -1, 0)) {
            tab = ItemMenuTabWrap(tab + 1);
        } else if (CheckPadRptPlaySe(pad_key[PAD_KEY_TAB_NEXT] | 0x2000, -1, 0)) {
            tab = ItemMenuTabWrap(tab - 1);
        } else if (CheckPadTrgPlaySe(pad_key[PAD_KEY_OK], -1, 0)) {
            if (ItemMenuListGetCnt(tab, index) <= 0)
                return -1;
            ItemMenuSetStep(ItemMenuCanUse(tab, index) ? ITEM_STEP_ASK : ITEM_STEP_NG);
            return -1;
        } else if (CheckPadTrgPlaySe(pad_key[PAD_KEY_CANCEL], -1, 0)) {
            SndSePlay(ITEM_SE_CANCEL, 100, 0x1000, 0, 0, 0);
            next = MenuGetTopPhase();
            ItemMenuSetStep(ITEM_STEP_START);
            return next;
        } else {
            return -1;
        }
        if (ItemMenuListGetNum(tab) <= 0) {
            SndSePlay(ITEM_SE_BUZZER, 100, 0x1000, 0, 0, 0);
        } else {
            SndSePlay(ITEM_SE_PAGE, 100, 0x1000, 0, 0, 0);
            ItemMenuSetTab(tab);
        }
        return -1;

    case ITEM_STEP_ASK:
        SndSePlay(ITEM_SE_OK, 100, 0x1000, 0, 0, 0);
        item_menu_msg_anim = LayoutAnimStart(3, 0);
        LayoutMsgSet(ITEM_MSG_MENU, item_dat[item].type == ITEM_TYPE_FILM ? ITEM_ASK_FILM : ITEM_ASK_USE);
        LayoutMsgSetType(1);
        LayoutMsgSetArg(1);
        LayoutAnimClearFlag(item_menu_msg_anim, LAYOUT_ANIM_HOLD);
        ItemMenuSetStep(ITEM_STEP_ASK_WAIT);
        // fall through
    case ITEM_STEP_ASK_WAIT:
        if (LayoutAnimGetState(item_menu_msg_anim) != 4)
            return -1;
        LayoutAnimReset(item_menu_msg_anim);
        if (LayoutMsgGetCsr() == 0)
            ItemMenuUse(tab, index);
        ItemMenuSetStep(ITEM_STEP_START2);
        ItemMenuFirstTab();
        return -1;

    case ITEM_STEP_NG:
        SndSePlay(ITEM_SE_BUZZER, 100, 0x1000, 0, 0, 0);
        item_menu_msg_anim = LayoutAnimStart(3, 0);
        switch (ItemMenuUseCheck(tab, index)) {
        case 1:
            LayoutMsgSet(ITEM_MSG_MENU, ITEM_ASK_CANNOT);
            break;
        case 2:
            LayoutMsgSet(ITEM_MSG_MENU, ITEM_ASK_NOT_NOW);
            break;
        case 3:
            LayoutMsgSet(ITEM_MSG_MENU, ITEM_ASK_LOADED);
            break;
        }
        LayoutMsgSetType(2);
        LayoutMsgSetArg(1);
        LayoutAnimClearFlag(item_menu_msg_anim, LAYOUT_ANIM_HOLD);
        ItemMenuSetStep(ITEM_STEP_NG_WAIT);
        // fall through
    case ITEM_STEP_NG_WAIT:
        if (LayoutAnimGetState(item_menu_msg_anim) != 4)
            return -1;
        LayoutAnimReset(item_menu_msg_anim);
        ItemMenuSetStep(ITEM_STEP_START2);
        return -1;

    case ITEM_STEP_EMPTY:
        GetCmnFadeLayer()->Switch(2, 0x1e);
        SndSePlay(ITEM_SE_BUZZER, 100, 0x1000, 0, 0, 0);
        item_menu_msg_anim = LayoutAnimStart(3, 0);
        LayoutMsgSet(ITEM_MSG_MENU, ITEM_ASK_EMPTY);
        LayoutMsgSetType(2);
        LayoutMsgSetArg(1);
        LayoutAnimClearFlag(item_menu_msg_anim, LAYOUT_ANIM_HOLD);
        ItemMenuSetStep(ITEM_STEP_EMPTY_WAIT);
        // fall through
    case ITEM_STEP_EMPTY_WAIT:
        if (LayoutAnimGetState(item_menu_msg_anim) != 4)
            return -1;
        SndSePlay(ITEM_SE_CANCEL, 100, 0x1000, 0, 0, 0);
        LayoutAnimReset(item_menu_msg_anim);
        return MenuGetTopPhase();
    }
    return -1;
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x001eb938: the lists, then the cursors (sized by them), then the
// first tab with items.

static void ItemMenuInit(void)
{
    ItemMenuListNew(0);
    ItemMenuListNew(1);
    ItemMenuCsrInit(0);
    ItemMenuCsrInit(1);
    item_menu_step = ITEM_STEP_START;
    item_menu_tab  = 0;
    ItemMenuFirstTab();
    item_menu_tab_old = item_menu_tab;
}

// FF3 0x001eb990
static void ItemMenuRelease(void)
{
    ItemMenuCsrRelease(0);
    ItemMenuCsrRelease(1);
    ItemMenuListDelete(0);
    ItemMenuListDelete(1);
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x001eb9c8 / 0x001eb9d8

static int ItemMenuGetStep(void)
{
    return item_menu_step;
}

// FF3 0x001eb9d8
static int ItemMenuSetStep(u_int step)
{
    if (step < ITEM_STEP_NUM)
        item_menu_step = (int)step;
    return item_menu_step;
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x001eb9f0 / 0x001eba00 (into 0..1) / 0x001eba60

static int ItemMenuGetTab(void)
{
    return item_menu_tab;
}

// FF3 0x001eba00
static int ItemMenuTabWrap(int tab)
{
    while (tab < 0)
        tab += ITEM_MENU_TAB_NUM;
    while (tab >= ITEM_MENU_TAB_NUM)
        tab -= ITEM_MENU_TAB_NUM;
    return tab;
}

// FF3 0x001eba60
static int ItemMenuSetTab(int tab)
{
    item_menu_tab = tab;
    return tab;
}

// ──────────────────────────────────────────────────────────────────────
// 1 once after the tab changed (ItemMenuUpdate plays the tab's label).
// FF3 0x001eba70

static int ItemMenuTabChanged(void)
{
    if (item_menu_tab_old != item_menu_tab) {
        item_menu_tab_old = item_menu_tab;
        return 1;
    }
    return 0;
}

// ──────────────────────────────────────────────────────────────────────
// From the current tab on, the first tab with items (1 when it is not the
// current one); none at all: the "no items" step (1).
// FF3 0x001ebaa0

static int ItemMenuFirstTab(void)
{
    int cur = ItemMenuGetTab();
    int i;

    for (i = 0; i < ITEM_MENU_TAB_NUM; i++) {
        int tab = ItemMenuTabWrap(cur + i);

        if (ItemMenuListGetNum(tab) > 0) {
            ItemMenuSetTab(tab);
            return i != 0;
        }
    }
    ItemMenuSetStep(ITEM_STEP_EMPTY);
    return 1;
}

// ──────────────────────────────────────────────────────────────────────
// The tab's list: its item types, the current world's items, every menu
// kind.
// FF3 0x001ebb20 (ItemMenuListNew)
// callcheck: operator_new, ItemMenuListCtor (new)

static void ItemMenuListNew(int tab)
{
    short world = (short)ItemGetWorldKind((s_char)ingame_wrk.b03);

    item_menu_list[tab] = new ITEM_MENU_LIST(item_menu_list_type[tab], world, ITEM_LIST_MENU_ALL);
}

// FF3 0x001ebbc8
// callcheck: ItemMenuListDtor, operator_delete (delete)
static void ItemMenuListDelete(int tab)
{
    if (item_menu_list[tab] != nullptr)
        delete item_menu_list[tab];
    item_menu_list[tab] = nullptr;
}

// FF3 0x001ebc20: after an item ran out.
static void ItemMenuListRebuild(int tab)
{
    ITEM_MENU_LIST *list = item_menu_list[tab];

    list->Free();
    list->Build();
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x001ebc60 / 0x001ebc80 (-1 outside the list) / 0x001ebcd0

static short ItemMenuListGetNum(int tab)
{
    return item_menu_list[tab]->num;
}

// FF3 0x001ebc80
static short ItemMenuListGet(int tab, short i)
{
    ITEM_MENU_LIST *list = item_menu_list[tab];

    if (i < 0 || i >= list->num)
        return -1;
    return list->list[i];
}

// FF3 0x001ebcd0: the count of item i, 0 outside the list.
static short ItemMenuListGetCnt(int tab, short i)
{
    u_short item = (u_short)ItemMenuListGet(tab, i);

    if (item < ITEM_NUM)
        return *ItemGetCnt((short)item);
    return 0;
}

// ──────────────────────────────────────────────────────────────────────
// Can item i be used now: 0 yes; 1 locked / not possible, or not a usable
// kind (ITEM_MENU_USE / _FILM); a film: 2 without the viewfinder, 3 when
// it is the film loaded; a recovery item: 2 at full health.
// FF3 0x001ebd58

static int ItemMenuUseCheck(int tab, short i)
{
    short item = ItemMenuListGet(tab, i);

    if (ItemCanUse(item, 1) == 0)
        return 1;
    if ((item_dat[item].menu & (ITEM_MENU_USE | ITEM_MENU_FILM)) == 0)
        return 1;
    switch ((u_short)item_dat[item].type) {
    case ITEM_TYPE_FILM:
        if (GetPhotoModeWrk()->IsFinderReady() == 0)
            return 2;
        if (FilmGetType(GetFilmWrk()) == item)
            return 3;
        return 0;

    case ITEM_TYPE_HP:
        if (PlyrGetHpRate() == 1.0f)                            /* sdata 0x370e04 */
            return 2;
        return 0;
    }
    return 0;
}

// FF3 0x001ebe40
static int ItemMenuCanUse(int tab, short i)
{
    return ItemMenuUseCheck(tab, i) == 0;
}

// ──────────────────────────────────────────────────────────────────────
// Uses one of item i; when it ran out the list is rebuilt and the cursor
// kept in it.
// FF3 0x001ebe60

static void ItemMenuUse(int tab, short i)
{
    if (ItemMenuCanUse(tab, i) == 0)
        return;
    if (ItemUse(ItemMenuListGet(tab, i), 1) != 0 && ItemMenuListGetCnt(tab, i) == 0) {
        ItemMenuListRebuild(tab);
        ItemMenuCsrReset(tab);
    }
}

// ──────────────────────────────────────────────────────────────────────
// The tab's cursor: one column, a row per item (one when empty), a 1 x 7
// view.
// FF3 0x001ebef0

static void ItemMenuCsrInit(int tab)
{
    MENU_CSR_WRK *wrk = &item_menu_csr[tab];
    short num = ItemMenuListGetNum(tab);

    MenuCsrObjInit(wrk, 1, (num != 0) ? num : 1, 0);
    MenuCsrObjSetViewX(wrk, 1, 0);
    MenuCsrObjSetViewY(wrk, ITEM_VIEW_ROWS, 0);
}

// FF3 0x001ebf68
static void ItemMenuCsrRelease(int tab)
{
    MenuCsrObjRelease(&item_menu_csr[tab]);
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x001ebf90 (was the FF2 match GetHaveItemTypeNum): 1 when the cursor
// moved (its SE played).

static int ItemMenuCsrUpdate(int tab)
{
    MENU_CSR_WRK *wrk = &item_menu_csr[tab];

    MenuMouseCsrGrid(wrk, item_menu_csl, ItemMenuHandle());     // PC
    MenuCsrObjUpdate(wrk, 0);
    return MenuCsrObjIsMovedSe(wrk, ITEM_SE_MOVE);
}

// FF3 0x001ebfd8
static int ItemMenuCsrPagePrev(int tab)
{
    return MenuCsrObjPagePrevY(&item_menu_csr[tab]);
}

// FF3 0x001ec000 (was the FF2 match KaiPuzzleSetFadeCmn)
static int ItemMenuCsrPageNext(int tab)
{
    return MenuCsrObjPageNextY(&item_menu_csr[tab]);
}

// ──────────────────────────────────────────────────────────────────────
// The cursor made again for the list's new size: the same item index (one
// back when it is past the end now) on the same view row.
// FF3 0x001ec028

static void ItemMenuCsrReset(int tab)
{
    MENU_CSR_WRK *wrk = &item_menu_csr[tab];
    short pos = MenuCsrObjGetPos(wrk);
    short row = MenuCsrObjGetViewPosY(wrk);

    ItemMenuCsrRelease(tab);
    ItemMenuCsrInit(tab);
    if (pos >= ItemMenuListGetNum(tab))
        pos = (short)(pos - 1);
    MenuCsrObjSetPos(wrk, pos, 0, row, -1, -1);
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x001ec0d0: the list index under the cursor.

static short ItemMenuCsrGetIndex(int tab)
{
    MENU_CSR_WRK *wrk = &item_menu_csr[tab];
    int row = MenuCsrObjGetViewPosY(wrk);

    return (short)(row + MenuCsrObjGetTopY(wrk));
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x001ec128 (no caller; was the FF2 match KaiPuzzleExeInit)

void ItemMenuCsrSetIndex(int tab, int index)
{
    int row  = ItemMenuCsrGetPosY(tab);
    int num  = ItemMenuListGetNum(tab);
    int page = (short)num;

    if (page > ITEM_VIEW_ROWS)
        page = ITEM_VIEW_ROWS;
    if (index < 0) {
        index = num - 1;
        row   = page - 1;
    }
    if (index >= num) {
        index = 0;
        row   = 0;
    } else if (index < row) {
        row = index;
    }
    if (num - index < page - row)
        row = page - (num - index);
    MenuCsrObjSetPos(&item_menu_csr[tab], (short)index, 0, (short)row, -1, -1);
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x001ec200 / 0x001ec230: the cursor's row in the view, the first row
// shown.

static short ItemMenuCsrGetPosY(int tab)
{
    return MenuCsrObjGetViewPosY(&item_menu_csr[tab]);
}

// FF3 0x001ec230
static short ItemMenuCsrGetTopY(int tab)
{
    return MenuCsrObjGetTopY(&item_menu_csr[tab]);
}

// ──────────────────────────────────────────────────────────────────────
// Item 0x419: the current item's picture (item_dat.pic) and its glow
// (pic + 1, additive), cross-faded to a new item once both slots have the
// next picture loaded; alpha from the item.
// FF3 0x001ec400

static int ItemMenuPicHook(const B2D_HOOK_ARGS *args)
{
    int   tab  = ItemMenuGetTab();
    short item = ItemMenuListGet(tab, ItemMenuCsrGetIndex(tab));
    float scale;
    int   ready, pic, i;

    if (item == -1)
        return 0;
    scale = (float)(int)args->stack[2] * (1.0f / 128.0f);       /* sdata 0x370e08 */
    ready = 1;
    for (i = 0; i < 2; i++) {
        if (item_menu_pic[i] != nullptr && item_menu_pic[i]->IsNextReady() == 0) {
            ready = 0;
            break;
        }
    }
    pic = item_dat[item].pic;
    for (i = 0; i < 2; i++) {
        if (item_menu_pic[i] != nullptr) {
            item_menu_pic[i]->enable = ready;
            item_menu_pic[i]->pic    = pic + i;
            item_menu_pic[i]->Step();
            item_menu_pic[i]->scale  = scale;
        }
    }
    item_menu_pic[0]->Draw();
    item_menu_pic[1]->Draw();
    item_menu_pic[0]->DrawNext();
    item_menu_pic[1]->DrawNext();
    SetDefaultGsDrawState();
    return 0;
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x001ec260: the two pictures at item 0x419's place, 256 x 256, the
// second one additive (ALPHA 0x48: Cs * As + Cd).
// callcheck: GetSysWrk, GetLanguage, BpCtlGetHandleByFileNo (ItemMenuHandle)
// callcheck: operator_new, MenuPicCtor (new)

static void ItemMenuPicCreate(void)
{
    B2D_ITEM *item = B2dCtlFindItem(ItemMenuHandle(), ITEM_ITEM_PIC);
    short x = (short)B2dSprGetPartData(item, B2D_PART_X);
    short y = (short)B2dSprGetPartData(item, B2D_PART_Y);
    B2D_ITEM_HOOK hook;
    int i;

    for (i = 0; i < 2; i++) {
        MENU_PIC *pic = new MENU_PIC(0);

        item_menu_pic[i] = pic;
        pic->v   = 0;
        pic->u   = 0;
        pic->h   = 0x100;
        pic->w   = 0x100;
        pic->pri = 0xa0;
        pic->x   = x;
        pic->y   = y;
        pic->z   = 0;
    }
    item_menu_pic[1]->alphar = 0x48;
    hook.func = ItemMenuPicHook;
    B2dCtlSetItemHook(item, &hook);
}

// FF3 0x001ec3a0 (was the FF2 match KaiPzlPrintCen)
// callcheck: MenuPicDtor, operator_delete (delete)
static void ItemMenuPicDelete(void)
{
    int i;

    for (i = 0; i < 2; i++) {
        if (item_menu_pic[i] != nullptr)
            delete item_menu_pic[i];
        item_menu_pic[i] = nullptr;
    }
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x001ec550 / 0x001ec5c8
// callcheck: GetSysWrk, GetLanguage (ItemMenuFile)
// callcheck: operator_new (new B2d_ScrollBar3)

static void ItemMenuBarCreate(void)
{
    item_menu_bar = new B2d_ScrollBar3(ItemMenuFile(), "slider", "R sld_U", "R sld_M", "R sld_B");  /* sdata 0x370e10.. */
}

// FF3 0x001ec5c8
static void ItemMenuBarDelete(void)
{
    if (item_menu_bar != nullptr)
        delete item_menu_bar;
    item_menu_bar = nullptr;
}

// ──────────────────────────────────────────────────────────────────────
// The scroll bar over the current tab's list and the up / down arrows.
// FF3 0x001ec600
// callcheck: GetSysWrk, GetLanguage, BpCtlGetHandleByFileNo (ItemMenuHandle)

static void ItemMenuBarDraw(void)
{
    int   tab;
    short top, row, num;
    int   handle;

    if (item_menu_bar == nullptr)
        return;
    tab    = ItemMenuGetTab();
    top    = ItemMenuCsrGetTopY(tab);
    row    = ItemMenuCsrGetPosY(tab);
    num    = ItemMenuListGetNum(tab);
    handle = ItemMenuHandle();
    item_menu_bar->SetRange(num, ITEM_VIEW_ROWS);
    item_menu_bar->Draw(row, top);
    MenuScrollArrowDisp(handle, 0x3fe, 0x3ee, 0x3ff, 0x3ef,
                        item_menu_bar->CanScrollUp(), item_menu_bar->CanScrollDown());
}

// ──────────────────────────────────────────────────────────────────────
// "R wasi": the sprite with bilinear filtering (TEX1 MMAG / MMIN linear).
// FF3 0x001ec898 (was the FF2 match KaiPuzzleDrawMsgOne)

static int ItemMenuWasiHook(const B2D_HOOK_ARGS *args)
{
    int ret;

    SetGsRegCached(0x100000161ULL, SCE_GS_TEX1_1, 1);
    ret = B2dDrawSpriteDirect(args);
    SetDefaultGsDrawState();
    return ret;
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x001ec728 / 0x001ec810 / 0x001ec850: the row cursor "csl", 30 px a
// row, on the cursor's view row; its create also hooks "R wasi" (the list
// is built on the stack).
// callcheck: GetSysWrk, GetLanguage, BpCtlGetHandleByFileNo (ItemMenuFile / ItemMenuHandle)
// callcheck: operator_new, B2dGridSprCtorByName2 (new B2D_GRID_SPR)

static void ItemMenuCslCreate(void)
{
    static const B2D_HOOK_REC hooks[2] =
    {
        { ItemMenuWasiHook, 0,  0,  "R wasi" },                 /* sdata 0x370e30 */
        { nullptr,          -1, -1, nullptr  },
    };

    item_menu_csl = new B2D_GRID_SPR(ItemMenuFile(), "csl", 0, ITEM_ROW_DY);   /* sdata 0x370e38 */
    B2dSetHookList(ItemMenuHandle(), hooks);
}

// FF3 0x001ec810
// callcheck: B2dGridSprDtor, operator_delete (delete)
static void ItemMenuCslDelete(void)
{
    if (item_menu_csl != nullptr)
        delete item_menu_csl;
    item_menu_csl = nullptr;
}

// FF3 0x001ec850
static void ItemMenuCslUpdate(void)
{
    if (item_menu_csl != nullptr)
        item_menu_csl->SetPos(0, ItemMenuCsrGetPosY(ItemMenuGetTab()));
}

// ──────────────────────────────────────────────────────────────────────
// The item hooks of the menu layout
// ──────────────────────────────────────────────────────────────────────

// Item 0x406 (a text item): the names of the 7 rows shown, the cursor's
// row in the highlight colour.
// FF3 0x001eca00

static int ItemMenuNameHook(const B2D_HOOK_ARGS *args)
{
    const int *xy    = (const int *)args->t1;
    int        alpha = (int)args->stack[2];
    int        tab   = ItemMenuGetTab();
    short      top   = ItemMenuCsrGetTopY(tab);
    short      row   = ItemMenuCsrGetPosY(tab);
    int        i, dy;

    for (i = 0, dy = 0; i < ITEM_VIEW_ROWS; i++, dy += ITEM_ROW_DY) {
        short item = ItemMenuListGet(tab, (short)(i + top));

        if (item != -1) {
            u_char *str = GetMsgDataAddr(ITEM_MSG_NAME, item_dat[item]._0e);

            if (str != nullptr) {
                SetCurColorRGB(0x19, 0x1b, 0x1f);
                if (i == row)
                    SetCurColorRGB(0xf1, 0xd6, 0x9b);
                PrintMsg_ArrangeOneLine(str, xy[0], xy[1] + dy, 0x2d, alpha, 0xa0, 0, 0, 0);
            }
        }
    }
    SetDefaultGsDrawState();
    return 0;
}

// ──────────────────────────────────────────────────────────────────────
// Item 0x40d: on tabA the counts (two digits) of the 7 rows shown, the
// colours as the names'. The count is read from tab 0's list (the ROM's
// constant; only tab 0 gets here).
// FF3 0x001ecb40

static int ItemMenuCntHook(const B2D_HOOK_ARGS *args)
{
    const int *xy    = (const int *)args->t1;
    int        alpha = (int)args->stack[2];
    int        tab   = ItemMenuGetTab();
    short      top   = ItemMenuCsrGetTopY(tab);
    short      row   = ItemMenuCsrGetPosY(tab);
    int        i, dy;

    for (i = 0, dy = 0; i < ITEM_VIEW_ROWS; i++, dy += ITEM_ROW_DY) {
        short item = ItemMenuListGet(tab, (short)(i + top));

        if (item == -1 || GetMsgDataAddr(ITEM_MSG_NAME, item_dat[item]._0e) == nullptr)
            continue;
        SetCurColorRGB(0x19, 0x1b, 0x1f);
        if (i == row)
            SetCurColorRGB(0xf1, 0xd6, 0x9b);
        if (tab == 0)
            PrintNumber_N(ItemMenuListGetCnt(0, (short)(i + top)), 2, xy[0], xy[1] + dy, 0x2d,
                          (u_char)alpha, 0xa0, 0, 0);
    }
    SetDefaultGsDrawState();
    return 0;
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x001ec978 (the list, rodata 0x35b250, is copied onto the stack)
// callcheck: GetSysWrk, GetLanguage, BpCtlGetHandleByFileNo (ItemMenuHandle)

static void ItemMenuSetHooks(void)
{
    static const B2D_HOOK_REC hooks[3] =
    {
        { ItemMenuNameHook, 0,  ITEM_ITEM_NAME, nullptr },
        { ItemMenuCntHook,  0,  ITEM_ITEM_CNT,  nullptr },
        { nullptr,          -1, -1,             nullptr },
    };

    B2dSetHookList(ItemMenuHandle(), hooks);
}

// FF3 0x001ec9f8: empty (jr ra).
static void ItemMenuResetHooks(void)
{
}

// ──────────────────────────────────────────────────────────────────────
// The menu window: the description of the item under the cursor, none
// without one.
// FF3 0x001ecca0

static void ItemMenuWdwSetMsg(void)
{
    int   tab  = ItemMenuGetTab();
    short item = ItemMenuListGet(tab, ItemMenuCsrGetIndex(tab));

    if (item == -1)
        MenuWdwSetMsg(-1, -1);
    else
        MenuWdwSetMsg(ITEM_MSG_DESC, item_dat[item]._0e);
}
