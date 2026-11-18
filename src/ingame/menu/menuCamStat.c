// ingame/menu/menuCamStat.c -- CmenuCamStat, the camera menu's status page
// (film, lenses, camera functions, camera levels), and the lens table the
// lens page shares. FF3 0x001146a8..0x00116fa0 + 0x002ac1b8..0x002ac330;
// FF3-only (the class file sorts after CmenuCamStLens; open-items.md
// "0x10bbb0..0x116fa0").
#include "ingame/menu/menuCamStat.h"

#include <string.h>                     // strcmp, strlen, strstr

#include "graphics/motion/anicode.h"    // B2D_HOOK_REC, B2dSetHookList, MenuWdwSetMsg
#include "ingame/ingame_wrk.h"          // ingame_wrk
#include "ingame/map/MarkEvent.h"       // LERP_F
#include "ingame/menu/menuCamBasic.h"   // MenuCamGetLensWrk, MenuWdwSetMsgByTable
#include "ingame/menu/menuCamMain.h"    // GetMenuCamMain, CmenuCamMain::SetPage, MENU_CAM_PAGE_*
#include "ingame/photo/finder_photo.h"  // GetPhotoModeWrk, GetFilmWrk
#include "ingame/photo/fndr_parts.h"    // FILM_WRK, LENS_WRK, Film*, Lens*
#include "ingame/sound/snd_ctrl.h"      // SndSePlay
#include "system/b2d/B2dBase.h"         // CB2dBase, B2D_SEL, B2D_PLACE, B2dNumDrawFont
#include "system/b2d/B2dCtl.h"          // B2D_HOOK_ARGS, B2D_ITEM, B2dGetDrawItem
#include "system/b2d/BpCtl.h"           // BpGetDrawHandle, B2dDrawSpriteDirect
#include "system/os/system.h"           // GetSysWrk, SYS_WRK::GetLanguage
#include "system/pad/pad_ctrl.h"        // pad_key, CheckPadTrgPlaySe
#include "ingame/menu/menu_mouse.h"    // PC: the mouse in menus

// ──────────────────────────────────────────────────────────────────────
// Callees in runs whose file is not settled yet (unassigned@ in tus.json).
// Move each block to its real header when that run is defined.
// ──────────────────────────────────────────────────────────────────────

// ──────────────────────────────────────────────────────────────────────

#define MENU_CAM_STAT_MSG0      0x5a    // MenuWdwSetMsg's msg0 (the camera menu help texts)
#define MENU_CAM_STAT_SE_MOVE   0x1a9b0000
#define MENU_CAM_STAT_SE_CANCEL 0x1a9b0001
#define MENU_CAM_STAT_SE_OK     0x1a9b0003
#define MENU_CAM_STAT_SE_PAGE   0x1a9b0005
#define MENU_CAM_STAT_CHR       0xff    // Lens* chr: the current player

// Items (EE data 0x34db20, the SetHooks / hook ids).
#define MENU_CAM_STAT_ROW_ITEM      0x4c0   // + row - 1: the row highlights (B2D_SEL)
#define MENU_CAM_STAT_ROW_CSR       0x4c3   // the item whose alpha they copy
#define MENU_CAM_STAT_ONE_SLOT      0x48c   // R_renzana_01's place
#define MENU_CAM_STAT_THREE_SLOT    0x48f   // R_renzana_02's place
#define MENU_CAM_STAT_LENS_CSR_Y    0xdb
#define MENU_CAM_STAT_CAMLV_NUM     5

// The lens table (data 0x34d968): {Rei's lens, Kei's lens, item}.
static const MENU_CAM_LENS_ROW menu_cam_lens_row[MENU_CAM_LENS_ROW_NUM] =
{
    { { 0, 4 }, 0x483 },
    { { 1, 1 }, 0x484 },
    { { 2, 8 }, 0x485 },
    { { 3, 2 }, 0x486 },
    { { 6, 3 }, 0x487 },
    { { 7, 6 }, 0x488 },
    { { 5, 7 }, 0x489 },
    { { -1, 5 }, 0x48a },
};

// Equip slot -> its place (data 0x2efb48), cursor position -> equip slot
// (data 0x34d958).
static const char *const menu_cam_slot_place[LENS_SLOT_NUM] = { "ren_02", "ren_01", "ren_00" };
static const int menu_cam_slot_of_csr[LENS_SLOT_NUM] = { 0, 2, 1 };

// The lens list cursor's x per row (data 0x34d9c8).
static const int menu_cam_lens_csr_x[MENU_CAM_LENS_ROW_NUM] =
{
    0x34, 0x5c, 0x84, 0xac, 0xd4, 0xfc, 0x124, 0x14c,
};

// The lens icons (data 0x34d8e0).
// layout: host -- {name pointer, lens}; 8 bytes on the EE.
struct MENU_CAM_LENS_NAME
{
    const char *name;
    int         no;
};
static const MENU_CAM_LENS_NAME menu_cam_lens_name[] =
{
    { "R_renz_blast",  0 },
    { "R_renz_see",    4 },
    { "R_renz_zero",   1 },
    { "R_renz_stn",    8 },
    { "R_renz_crush",  2 },
    { "R_renz_serial", 3 },
    { "R_renz_slw",    6 },
    { "R_renz_track",  7 },
    { "R_renz_stp",    5 },
    { nullptr,        -1 },
};

// The camera functions in cursor order (data 0x34da78): the function, its
// icon (R_soubi_0<func + 1>, also R_soubi_L_0<func + 1> when on) and its
// place.
// layout: host -- {func, two pointers}; 0xc bytes on the EE.
struct MENU_CAM_FUNC
{
    int         func;                   // LENS_FUNC_*, -1 ends
    const char *item;                   // nothing reads it
    const char *place;
};
static const MENU_CAM_FUNC menu_cam_func[] =
{
    { 1, "R_soubi_02", "soubi_01" },
    { 2, "R_soubi_03", "soubi_02" },
    { 0, "R_soubi_01", "soubi_03" },
    { 4, "R_soubi_05", "soubi_04" },
    { 3, "R_soubi_04", "soubi_05" },
    { 5, "R_soubi_06", "soubi_06" },
    { 6, "R_soubi_07", "soubi_07" },
    { 7, "R_soubi_08", "soubi_08" },
    { 8, "R_soubi_09", "soubi_09" },
    { -1, nullptr, nullptr },
};

// The player's own camera abilities (data 0x34d930): item -> kind.
// layout: host -- {name pointer, kind}; 8 bytes on the EE.
struct MENU_CAM_CAMLV_SP
{
    const char *name;
    int         kind;
};
static const MENU_CAM_CAMLV_SP menu_cam_camlv_sp[] =
{
    { "R_koyuu_05", 0 },
    { "R_koyuu_03", 1 },
    { "R_koyuu_04", 2 },
    { "R_koyuu_02", 3 },
    { nullptr,     -1 },
};

// sdata gp-0x7dc0: the page the constructor made (its hooks read it).
static CmenuCamStat *menu_cam_stat;

// ──────────────────────────────────────────────────────────────────────
// Table lookups (out-of-line copies at the end of .text). nullptr when the
// key is missing.

// FF3 0x002ac1b8: by item name.
static const MENU_CAM_LENS_NAME *MenuCamStatFindLensByName(const MENU_CAM_LENS_NAME *tbl, const char *const *name)
{
    for (; tbl->name != nullptr; tbl++)
    {
        if (strcmp(*name, tbl->name) == 0)
            return tbl;
    }
    return nullptr;
}

// FF3 0x002ac240: by function.
static const MENU_CAM_FUNC *MenuCamStatFindFunc(const MENU_CAM_FUNC *tbl, const int *func)
{
    for (; tbl->func != -1; tbl++)
    {
        if (tbl->func == *func)
            return tbl;
    }
    return nullptr;
}

// FF3 0x002ac280: by kind.
static const MENU_CAM_CAMLV_SP *MenuCamStatFindCamLvSp(const MENU_CAM_CAMLV_SP *tbl, const int *kind)
{
    for (; tbl->name != nullptr; tbl++)
    {
        if (tbl->kind == *kind)
            return tbl;
    }
    return nullptr;
}

// ══════════════════════════════════════════════════════════════════════
// The lens table
// ══════════════════════════════════════════════════════════════════════

// ──────────────────────────────────────────────────────────────────────
// FF3 0x001146a8.

FILM_WRK *MenuCamGetFilmWrk(void)
{
    GetPhotoModeWrk();
    return GetFilmWrk();
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x001146c8.

int MenuCamIsLensMultiSlot(void)
{
    LENS_WRK *lens = MenuCamGetLensWrk();

    if (lens->HaveFunc(LENS_FUNC_SLOTS) != 0 && lens->GetFunc(LENS_FUNC_SLOTS) != 0)
        return 1;
    return 0;
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x00114720.

int MenuCamLensGetColumn(void)
{
    if (ingame_wrk.b00 == 0)
        return 0;
    if (ingame_wrk.b00 == 2)
        return 1;
    return -1;
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x00114748.

int MenuCamLensGetNo(int row)
{
    int col = MenuCamLensGetColumn();

    if (col < 0)
        return -1;
    return menu_cam_lens_row[row].no[col];
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x00114798.

int MenuCamLensFindRow(int no)
{
    int col = MenuCamLensGetColumn();

    // PC: FF3 reads the word before the table (0x34d964, 0) for column -1;
    // no caller gets here then (player 1 has no lenses).
    if (col < 0)
        return -1;
    for (int i = 0; i < MENU_CAM_LENS_ROW_NUM; i++)
    {
        if (menu_cam_lens_row[i].no[col] == no)
            return i;
    }
    return -1;
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x001147f8.

const MENU_CAM_LENS_ROW *MenuCamLensGetRow(int row)
{
    return &menu_cam_lens_row[row];
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x00114818.

int MenuCamLensGetRowNum(void)
{
    return MENU_CAM_LENS_ROW_NUM;
}

// ══════════════════════════════════════════════════════════════════════
// The page
// ══════════════════════════════════════════════════════════════════════

// ──────────────────────────────────────────────────────────────────────
// FF3 0x00114bf0 (MenuCamStatCtor; FF3 0x00114820 is the same body).
// callcheck: MenuCamBaseCtorBase (the base class constructor)
// callcheck: CsrGridInit, LerpFClear, B2dSelInit (grid[]'s, lerp[]'s and sel's constructors, implicit)

CmenuCamStat::CmenuCamStat()
{
    // Per grid (data 0x34d100): range and start of its one axis.
    static const struct { int min, max, cur, vert; } init[MENU_CAM_STAT_NUM] =
    {
        { 1, 3, 1, 1 },                 // TOP: rows film, lenses, functions
        { -1, 1, 0, 0 },                // FILM: previous / next
        { 0, 2, 0, 0 },                 // SLOT
        { 0, 8, 0, 0 },                 // FUNC
        { 0, 7, 0, 0 },                 // LENS
    };

    blink = 0;

    LENS_WRK *lens = MenuCamGetLensWrk();
    if (lens->GetHaveNum(MENU_CAM_STAT_CHR) == 0)
        grid[MENU_CAM_STAT_TOP].SetSkip(1, 2, 1);
    if (lens->GetHaveFuncNum() == 0)
        grid[MENU_CAM_STAT_TOP].SetSkip(1, 3, 1);

    for (int i = 0; i < MENU_CAM_STAT_NUM; i++)
    {
        CSR_GRID *g = &grid[i];
        if (init[i].vert == 1)
        {
            g->SetRange(0, 0, 0);
            g->SetRange(1, init[i].min, init[i].max);
            g->wrap = CSR_GRID_WRAP_Y;
            g->SetPos(1, init[i].cur);
            g->flags |= CSR_GRID_SE_Y;
        }
        else
        {
            g->SetRange(0, init[i].min, init[i].max);
            g->SetRange(1, 0, 0);
            g->wrap = CSR_GRID_WRAP_X;
            g->SetPos(0, init[i].cur);
            g->flags |= CSR_GRID_SE_X;
        }
        g->se = MENU_CAM_STAT_SE_MOVE;
    }
    grid[MENU_CAM_STAT_FILM].flags &= ~CSR_GRID_SE_X;

    // The top level's x: -1 / +1 switch pages (also L2 / R2), silently.
    grid[MENU_CAM_STAT_TOP].SetRange(0, -1, 1);
    grid[MENU_CAM_STAT_TOP].SetPos(0, 0);
    grid[MENU_CAM_STAT_TOP].se = MENU_CAM_STAT_SE_MOVE;
    grid[MENU_CAM_STAT_TOP].flags |= CSR_GRID_LR2 | CSR_GRID_SE_Y;

    // The lens list skips lenses not owned and starts on the first owned.
    int first = 1;
    for (int i = 0; i < MENU_CAM_LENS_ROW_NUM; i++)
    {
        int no = MenuCamLensGetNo(i);
        if (no < 0 || lens->IsHave((u_char)no, MENU_CAM_STAT_CHR) == 0)
            grid[MENU_CAM_STAT_LENS].SetSkip(0, i, 1);
        else if (first)
        {
            first = 0;
            grid[MENU_CAM_STAT_LENS].SetPos(0, i);
        }
    }

    // The same for the camera functions.
    first = 1;
    for (int i = 0; i < LENS_FUNC_NUM; i++)
    {
        if (lens->HaveFunc((u_char)menu_cam_func[i].func) == 0)
            grid[MENU_CAM_STAT_FUNC].SetSkip(0, i, 1);
        else if (first)
        {
            first = 0;
            grid[MENU_CAM_STAT_FUNC].SetPos(0, i);
        }
    }

    lerp[0].Set(1.0f, 0.3125f, 40.0f);   // sdata 0x36b740 / 0x36b744 / 0x36b748
    lerp[1].Set(0.3125f, 1.0f, 40.0f);
    blink = 0;                          // the blink starts at full alpha
    lerp[0].time = 0.0f;
    lerp[blink].Update();
    state = MENU_CAM_STAT_TOP;
    menu_cam_stat = this;
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x002ac2d0 (MenuCamStatDtor; the deleting one 0x002ac2f8 adds
// operator delete). The singleton is left as it is.
// callcheck: MenuCamBaseDtorBase (the base class destructor)

CmenuCamStat::~CmenuCamStat()
{
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x001157b8.

CmenuCamStat *GetMenuCamStat(void)
{
    return menu_cam_stat;
}

// ──────────────────────────────────────────────────────────────────────
// 1 when no row is entered or row is the one selected: hooks halve the
// alpha of the other rows' items. FF3 0x001157c0.

static int MenuCamStatIsRowLit(int row)
{
    CmenuCamStat *cs = GetMenuCamStat();

    if (cs->state == MENU_CAM_STAT_TOP)
        return 1;
    return cs->grid[MENU_CAM_STAT_TOP].axis[1].cur == row;
}

// ══════════════════════════════════════════════════════════════════════
// Steps (table 0x34daf0, by state). 1 ends the menu.
// ══════════════════════════════════════════════════════════════════════

// ──────────────────────────────────────────────────────────────────────
// Top level: the row highlight and help text; left / right switch to the
// lens page (-1; the basic page when no lens is owned) or the basic page
// (+1); OK enters the row, cancel closes the menu. FF3 0x00114fc0.

static int MenuCamStatStepTop(CmenuCamStat *cs)
{
    static const int row_msg[] = { 0, 0, 1, 2 };   // [0]: FF3 leaves it unset; rows are 1..3

    CSR_GRID *g   = &cs->grid[cs->state];
    int       row = g->axis[1].cur;

    cs->sel.Set(row - 1);
    MenuWdwSetMsg(MENU_CAM_STAT_MSG0, row_msg[row]);

    int page = g->axis[0].cur;
    if (page != 0 && MenuCamGetLensWrk()->GetHaveNum(MENU_CAM_STAT_CHR) == 0)
        page = 1;
    if (page == -1)
        GetMenuCamMain()->SetPage(MENU_CAM_PAGE_STLENS);
    else if (page == 1)
        GetMenuCamMain()->SetPage(MENU_CAM_PAGE_BASIC);

    if (page != 0)
    {
        SndSePlay(MENU_CAM_STAT_SE_PAGE, 100, 0x1000, 0, nullptr, 0);
        g->SetPos(0, 0);
        return 0;
    }
    if (CheckPadTrgPlaySe((u_short)pad_key[PAD_KEY_OK], MENU_CAM_STAT_SE_OK, 0) != 0)
    {
        cs->state = row;
        CmenuCamStat *self = GetMenuCamStat();      // the blink restarts at full alpha
        self->blink = 0;
        self->lerp[0].time = 0.0f;
        self->lerp[0].Update();
        return 0;
    }
    return CheckPadTrgPlaySe((u_short)pad_key[PAD_KEY_CANCEL], MENU_CAM_STAT_SE_CANCEL, 0) != 0;
}

// ──────────────────────────────────────────────────────────────────────
// Film: left / right load the previous / next film; OK or cancel go back.
// FF3 0x00115160.

static int MenuCamStatStepFilm(CmenuCamStat *cs)
{
    static const int film_msg[] =       // data 0x34d160: {film type, msg1}
    {
        0, 10, 1, 11, 2, 12, 3, 13, 4, 14, -1, -1,
    };

    FILM_WRK *film = MenuCamGetFilmWrk();
    CSR_GRID *g    = &cs->grid[cs->state];
    int       dir  = g->axis[0].cur;

    if (dir == -1)
        FilmSelectPrev(film);
    else if (dir == 1)
        FilmSelectNext(film);
    MenuWdwSetMsgByTable(film_msg, MENU_CAM_STAT_MSG0, FilmGetType(film));

    if (dir == 0)
    {
        if (CheckPadTrgPlaySe((u_short)pad_key[PAD_KEY_CANCEL], MENU_CAM_STAT_SE_CANCEL, 0) != 0)
            cs->state = MENU_CAM_STAT_TOP;
        else if (CheckPadTrgPlaySe((u_short)pad_key[PAD_KEY_OK], MENU_CAM_STAT_SE_OK, 0) != 0)
            cs->state = MENU_CAM_STAT_TOP;
    }
    else
        g->SetPos(0, 0);
    return 0;
}

// ──────────────────────────────────────────────────────────────────────
// Lenses: with three slots, pick the slot first (OK: the lens list,
// cancel: back); with one, straight to the list. FF3 0x001152a0.

static int MenuCamStatStepSlot(CmenuCamStat *cs)
{
    if (MenuCamIsLensMultiSlot() != 0)
    {
        MenuWdwSetMsg(MENU_CAM_STAT_MSG0, 3);
        if (CheckPadTrgPlaySe((u_short)pad_key[PAD_KEY_OK], MENU_CAM_STAT_SE_OK, 0) == 0)
        {
            if (CheckPadTrgPlaySe((u_short)pad_key[PAD_KEY_CANCEL], MENU_CAM_STAT_SE_CANCEL, 0) != 0)
                cs->state = MENU_CAM_STAT_TOP;
            return 0;
        }
        CmenuCamStat *self = GetMenuCamStat();      // the blink restarts at full alpha
        self->blink = 0;
        self->lerp[0].time = 0.0f;
        self->lerp[0].Update();
    }
    cs->state = MENU_CAM_STAT_LENS;
    return 0;
}

// ──────────────────────────────────────────────────────────────────────
// The lens list: OK equips the lens in the chosen slot, or empties the
// slot when the lens is already equipped; OK or cancel go back to the slot
// (three slots) or the top. FF3 0x00115358.

static int MenuCamStatStepLens(CmenuCamStat *cs)
{
    static const int lens_msg[] =       // data 0x34d190: {lens, msg1}, 9: equipped
    {
        0, 20, 1, 21, 2, 22, 3, 23, 4, 24, 5, 25, 6, 26, 7, 27, 8, 28, 9, 29, -1, -1,
    };

    u_char    no   = (u_char)MenuCamLensGetNo(cs->grid[cs->state].axis[0].cur);
    int       done = 0;
    LENS_WRK *lens = MenuCamGetLensWrk();
    u_char    key  = 9;

    if (lens->IsEquip(no, MENU_CAM_STAT_CHR) == 0)
        key = no;
    MenuWdwSetMsgByTable(lens_msg, MENU_CAM_STAT_MSG0, key);

    if (CheckPadTrgPlaySe((u_short)pad_key[PAD_KEY_OK], MENU_CAM_STAT_SE_OK, 0) != 0)
    {
        u_char slot = (u_char)menu_cam_slot_of_csr[cs->grid[MENU_CAM_STAT_SLOT].axis[0].cur];
        if (lens->IsEquip(no, MENU_CAM_STAT_CHR) == 0)
            lens->SetEquip(slot, no, MENU_CAM_STAT_CHR);
        else
            lens->Unequip(slot, MENU_CAM_STAT_CHR);
        done = 1;
    }
    if (CheckPadTrgPlaySe((u_short)pad_key[PAD_KEY_CANCEL], MENU_CAM_STAT_SE_CANCEL, 0) != 0 || done)
    {
        CmenuCamStat *self = GetMenuCamStat();      // the blink restarts at full alpha
        self->blink = 0;
        self->lerp[0].time = 0.0f;
        self->lerp[0].Update();
        cs->state = MenuCamIsLensMultiSlot() != 0 ? MENU_CAM_STAT_SLOT : MENU_CAM_STAT_TOP;
    }
    return 0;
}

// ──────────────────────────────────────────────────────────────────────
// Camera functions: OK switches the function on / off (switching
// LENS_FUNC_SLOTS off empties slots 1 and 2); cancel goes back.
// FF3 0x00115510.

static int MenuCamStatStepFunc(CmenuCamStat *cs)
{
    static const int func_msg[] =       // data 0x34d1e8: {LENS_FUNC_*, msg1}
    {
        0, 30, 1, 31, 2, 32, 3, 33, 4, 34, 5, 35, 6, 36, 7, 37, 8, 38, -1, -1,
    };

    int func = menu_cam_func[cs->grid[cs->state].axis[0].cur].func;

    MenuWdwSetMsgByTable(func_msg, MENU_CAM_STAT_MSG0, func);
    if (CheckPadTrgPlaySe((u_short)pad_key[PAD_KEY_OK], MENU_CAM_STAT_SE_OK, 0) == 0)
    {
        if (CheckPadTrgPlaySe((u_short)pad_key[PAD_KEY_CANCEL], MENU_CAM_STAT_SE_CANCEL, 0) != 0)
            cs->state = MENU_CAM_STAT_TOP;
        return 0;
    }

    LENS_WRK *lens = MenuCamGetLensWrk();
    if (lens->GetFunc((u_char)func) == 0)
        lens->SetFunc((u_char)func);
    else
        lens->ClearFunc((u_char)func);
    if (func == LENS_FUNC_SLOTS && MenuCamIsLensMultiSlot() == 0)
    {
        lens->Unequip(1, MENU_CAM_STAT_CHR);
        lens->Unequip(2, MENU_CAM_STAT_CHR);
        cs->grid[MENU_CAM_STAT_SLOT].SetPos(0, 0);
    }
    return 0;
}

// ──────────────────────────────────────────────────────────────────────
// PC: the mouse. The top level's rows run across the left panel from their
// titles down; the sub-levels point at the icons.

static void MenuCamStatMouse(CmenuCamStat *cs)
{
    static const struct { const char *title; float h; } rows[3] =
    {
        { "R_film_01", 52.0f }, { "R_renz_01", 98.0f }, { "R_soubikinou01", 68.0f },
    };
    CSR_GRID      *g = &cs->grid[cs->state];
    ReiPan_MenuBox box;

    switch (cs->state)
    {
    case MENU_CAM_STAT_TOP:
        for (int row = 1; row <= 3; row++)
        {
            if ((g->axis[1].skip & (1u << row)) == 0
                && ReiPan_MenuMouseNameBox(cs->handle, rows[row - 1].title, &box))
            {
                const ReiPan_MenuBox hit = { 45.0f, box.y - 4.0f, 360.0f, rows[row - 1].h };

                MenuMouseGrid(g, 1, row, &hit);
            }
        }
        break;
    case MENU_CAM_STAT_FILM:
        ReiPan_MenuMouseConfirm();      // the wheel changes the film, a click takes it
        break;
    case MENU_CAM_STAT_SLOT:
        for (int i = 0; i < LENS_SLOT_NUM; i++)
        {
            if (ReiPan_MenuMouseNameBox(cs->handle, menu_cam_slot_place[menu_cam_slot_of_csr[i]], &box))
                MenuMouseGrid(g, 0, i, &box);
        }
        break;
    case MENU_CAM_STAT_FUNC:
        for (int i = 0; i < LENS_FUNC_NUM; i++)
        {
            if ((g->axis[0].skip & (1u << i)) == 0
                && ReiPan_MenuMouseNameBox(cs->handle, menu_cam_func[i].place, &box))
                MenuMouseGrid(g, 0, i, &box);
        }
        break;
    case MENU_CAM_STAT_LENS:
        for (int i = 0; i < MENU_CAM_LENS_ROW_NUM; i++)
        {
            if ((g->axis[0].skip & (1u << i)) == 0
                && ReiPan_MenuMouseNameBox(cs->handle, menu_cam_lens_name[i].name, &box))
                MenuMouseGrid(g, 0, i, &box);
        }
        break;
    }
}

// ──────────────────────────────────────────────────────────────────────
// Per frame: the cursors' blink (lerp[0] fades out, lerp[1] back in), the
// state's cursor, then its step. FF3 0x00115688.

int CmenuCamStat::Step()
{
    // FF3's entries are {state, function}, 8 bytes (a pointer to member would
    // add an adjustment word), so the steps are file functions, not members.
    typedef int (*STAT_STEP)(CmenuCamStat *cs);
    static const struct { int state; STAT_STEP func; } steps[] =    // data 0x34daf0
    {
        { MENU_CAM_STAT_TOP,  MenuCamStatStepTop },
        { MENU_CAM_STAT_SLOT, MenuCamStatStepSlot },
        { MENU_CAM_STAT_FILM, MenuCamStatStepFilm },
        { MENU_CAM_STAT_FUNC, MenuCamStatStepFunc },
        { MENU_CAM_STAT_LENS, MenuCamStatStepLens },
        { -1, nullptr },
    };

    if (lerp[blink].Update() != 0)
    {
        if (++blink > 1)
            blink = 0;
        lerp[blink].time = 0.0f;
        lerp[blink].Update();
    }
    MenuCamStatMouse(this);             // PC
    grid[state].Move();

    for (int i = 0; i < MENU_CAM_STAT_NUM; i++)
    {
        if (steps[i].state == state)
            return steps[i].func(this);
    }
    return 0;
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x00115770 (GetSysWrk is cached in sdata gp-0xc60).

void CmenuCamStat::Load()
{
    CB2dBase::Load(GetSysWrk()->GetLanguage() + MENU_CAM_STAT_FILE);
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x00116f78 (Ghidra had it as FurnCtl's static initializer).

int CmenuCamStat::Draw()
{
    sel.Update();
    return 0;
}

// ══════════════════════════════════════════════════════════════════════
// Item hooks (list 0x34d590). Sprite hooks: t1, t2 x, y, stack[1] alpha;
// the two number hooks are text-item hooks: t1 the corners xy[8],
// stack[2] alpha.
// ══════════════════════════════════════════════════════════════════════

// ──────────────────────────────────────────────────────────────────────
// Item 0x47f: the film's type number (Type-07, -14, -61, -90, Zero).
// FF3 0x00115800.

static int MenuCamStatFilmTypeHook(const B2D_HOOK_ARGS *args)
{
    static const char font[] = "R_num_film_00";     // 0x34d238

    const int *xy    = (const int *)args->t1;
    int        alpha = (int)args->stack[2];
    int        value = -1;

    switch (FilmGetType(MenuCamGetFilmWrk()))
    {
    case 0: value = 7;  break;
    case 1: value = 14; break;
    case 2: value = 61; break;
    case 3: value = 90; break;
    case 4: value = 0;  break;
    }
    if (MenuCamStatIsRowLit(1) == 0)
        alpha /= 2;
    B2dNumDrawFont(value, BpGetDrawHandle(), font, xy[4], xy[5], (u_char)alpha, 2);
    return 0;
}

// ──────────────────────────────────────────────────────────────────────
// Item 0x47d: the shots left. FF3 0x001158d8.

static int MenuCamStatFilmNumHook(const B2D_HOOK_ARGS *args)
{
    static const char font[] = "R_num_film_00";     // 0x34d238

    const int *xy     = (const int *)args->t1;
    int        alpha  = (int)args->stack[2];
    int        handle = BpGetDrawHandle();

    if (MenuCamStatIsRowLit(1) == 0)
        alpha /= 2;
    B2dNumDrawFont(FilmGetNum(GetFilmWrk()), handle, font, xy[4], xy[5], (u_char)alpha, 2);
    return 0;
}

// ──────────────────────────────────────────────────────────────────────
// Items 0x457, 0x3fa: only while the film is being picked.
// FF3 0x00115968.

static int MenuCamStatFilmSelHook(const B2D_HOOK_ARGS *args)
{
    if (GetMenuCamStat()->state == MENU_CAM_STAT_FILM)
        B2dDrawSpriteDirect(args);
    return 0;
}

// ──────────────────────────────────────────────────────────────────────
// The R_renz_* lens icons: an owned lens at its equip slot, else at its
// list row. FF3 0x00115a60.

// callcheck: B2dPlaceInit (B2D_PLACE's constructor)
static int MenuCamStatLensIconHook(const B2D_HOOK_ARGS *args)
{
    int       xy[2];
    int       alpha = (int)args->stack[1];

    B2D_PLACE place(BpGetDrawHandle());
    B2D_ITEM *item = B2dGetDrawItem();
    if (MenuCamStatIsRowLit(2) == 0 && MenuCamStatIsRowLit(4) == 0)
        alpha /= 2;

    const MENU_CAM_LENS_NAME *ln = MenuCamStatFindLensByName(menu_cam_lens_name, &item->name);
    if (ln == nullptr)
        return 0;                       // PC: FF3 reads the lens through the nullptr
    int no = ln->no;

    LENS_WRK *lens = MenuCamGetLensWrk();
    if (lens->IsHave((u_char)no, MENU_CAM_STAT_CHR) == 0)
        return 0;

    int slot;
    for (slot = 0; slot < LENS_SLOT_NUM; slot++)
    {
        if (lens->GetEquip((u_char)slot, MENU_CAM_STAT_CHR) == no)
        {
            place.GetXYByName(menu_cam_slot_place[slot], xy);
            break;
        }
    }
    if (slot == LENS_SLOT_NUM)
    {
        int row = MenuCamLensFindRow(no);
        if (row < 0)
            return 0;
        place.GetXY(menu_cam_lens_row[row].item, xy);
    }

    B2D_HOOK_ARGS a = *args;
    a.t1       = xy[0];
    a.t2       = xy[1];
    a.stack[1] = alpha;
    B2dDrawSpriteDirect(&a);
    return 0;
}

// ──────────────────────────────────────────────────────────────────────
// Where the equip slot cursor goes: shown while a slot or a lens is
// picked; the blinking one (fixed 0) only while the slot is picked, with
// three slots. FF3 0x00115c18.

// callcheck: B2dPlaceInit (B2D_PLACE's constructor)
static int MenuCamStatSlotCsrPos(int fixed, int *x, int *y, int *alpha)
{
    CmenuCamStat *cs = GetMenuCamStat();

    if (cs->state == MENU_CAM_STAT_SLOT)
    {
        if (fixed == 0)
        {
            if (MenuCamIsLensMultiSlot() == 0)
                return 0;
            *alpha = (int)(cs->lerp[cs->blink].cur * (float)*alpha);
        }
    }
    else if (cs->state != MENU_CAM_STAT_LENS || fixed == 0)
        return 0;

    int       csr = cs->grid[MENU_CAM_STAT_SLOT].axis[0].cur;
    int       xy[2];
    B2D_PLACE place(BpGetDrawHandle());
    place.GetXYByName(menu_cam_slot_place[menu_cam_slot_of_csr[csr % LENS_SLOT_NUM]], xy);
    *x = xy[0];
    *y = xy[1];
    return 1;
}

// R_renzcasolu_01 (fixed) / _03 (blinking). FF3 0x00115d50 / 0x00115e20.
static int MenuCamStatSlotCsrDraw(const B2D_HOOK_ARGS *args, int fixed)
{
    int x     = (int)args->t1;
    int y     = (int)args->t2;
    int alpha = (int)args->stack[1];

    if (MenuCamStatSlotCsrPos(fixed, &x, &y, &alpha) != 0)
    {
        B2D_HOOK_ARGS a = *args;
        a.t1       = x - 0x12;
        a.t2       = y - 5;
        a.stack[1] = alpha;
        B2dDrawSpriteDirect(&a);
    }
    return 0;
}

static int MenuCamStatSlotCsrHook(const B2D_HOOK_ARGS *args)          // FF3 0x00115d50
{
    return MenuCamStatSlotCsrDraw(args, 1);
}

static int MenuCamStatSlotCsrBlinkHook(const B2D_HOOK_ARGS *args)     // FF3 0x00115e20
{
    return MenuCamStatSlotCsrDraw(args, 0);
}

// ──────────────────────────────────────────────────────────────────────
// The lens list cursor: its row while the list is up, -1 otherwise.
// FF3 0x00115ef0.

static int MenuCamStatLensCsrGet(int fixed, int *alpha)
{
    CmenuCamStat *cs  = GetMenuCamStat();
    int           row = cs->grid[MENU_CAM_STAT_TOP].axis[1].cur;

    if (row != 2 && row != 4)
        return -1;
    if (cs->state != MENU_CAM_STAT_LENS)
        return -1;
    if (fixed == 0)
        *alpha = (int)(cs->lerp[cs->blink].cur * (float)*alpha);
    return cs->grid[MENU_CAM_STAT_LENS].axis[0].cur;
}

// R_renzcasolu_02 (fixed) / _04 (blinking). FF3 0x00115f88 / 0x00116050.
static int MenuCamStatLensCsrDraw(const B2D_HOOK_ARGS *args, int fixed)
{
    int alpha = (int)args->stack[1];
    int row   = MenuCamStatLensCsrGet(fixed, &alpha);

    if (row >= 0)
    {
        B2D_HOOK_ARGS a = *args;
        a.t1       = menu_cam_lens_csr_x[row];
        a.t2       = MENU_CAM_STAT_LENS_CSR_Y;
        a.stack[1] = alpha;
        B2dDrawSpriteDirect(&a);
    }
    return 0;
}

static int MenuCamStatLensCsrHook(const B2D_HOOK_ARGS *args)          // FF3 0x00115f88
{
    return MenuCamStatLensCsrDraw(args, 1);
}

static int MenuCamStatLensCsrBlinkHook(const B2D_HOOK_ARGS *args)     // FF3 0x00116050
{
    return MenuCamStatLensCsrDraw(args, 0);
}

// ──────────────────────────────────────────────────────────────────────
// R_soubi_0N / R_soubi_L_0N: camera function N - 1, if owned, as its off
// or on (L) icon at its place. FF3 0x00116118.

// callcheck: B2dPlaceInit (B2D_PLACE's constructor)
static int MenuCamStatFuncIconHook(const B2D_HOOK_ARGS *args)
{
    static const char lit[] = "R_soubi_L";          // 0x34d260

    int       xy[2];
    int       alpha = (int)args->stack[1];

    B2D_PLACE place(BpGetDrawHandle());
    if (MenuCamStatIsRowLit(3) == 0)
        alpha /= 2;

    B2D_ITEM  *item = B2dGetDrawItem();
    int        on   = strstr(item->name, lit) != nullptr;
    LENS_WRK  *lens = MenuCamGetLensWrk();
    int        func = item->name[strlen(item->name) - 1] - '1';

    if (lens->HaveFunc((u_char)func) == 0)
        return 0;
    if ((lens->GetFunc((u_char)func) != 0) != on)
        return 0;

    const MENU_CAM_FUNC *f = MenuCamStatFindFunc(menu_cam_func, &func);
    if (f == nullptr)
        return 0;                       // PC: FF3 reads the place through the nullptr

    place.GetXYByName(f->place, xy);
    B2D_HOOK_ARGS a = *args;
    a.t1       = xy[0];
    a.t2       = xy[1];
    a.stack[1] = alpha;
    B2dDrawSpriteDirect(&a);
    return 0;
}

// ──────────────────────────────────────────────────────────────────────
// Where the function cursor goes (state FUNC only): place
// soubi_0<1 + cursor>, 9 to the left. FF3 0x00116298.

// callcheck: B2dPlaceInit (B2D_PLACE's constructor)
static int MenuCamStatFuncCsrPos(int fixed, int *x, int *y, int *alpha)
{
    CmenuCamStat *cs = GetMenuCamStat();

    if (cs->state != MENU_CAM_STAT_FUNC)
        return 0;
    if (fixed == 0)
        *alpha = (int)(cs->lerp[cs->blink].cur * (float)*alpha);

    char name[10] = "soubi_01";         // 0x34d270
    name[strlen(name) - 1] += (char)cs->grid[MENU_CAM_STAT_FUNC].axis[0].cur;

    int       xy[2];
    B2D_PLACE place(BpGetDrawHandle());
    place.GetXYByName(name, xy);
    *x = xy[0] - 9;
    *y = xy[1];
    return 1;
}

// R_soubicasoru_01 (fixed) / _02 (blinking). FF3 0x001163a0 / 0x00116468.
static int MenuCamStatFuncCsrDraw(const B2D_HOOK_ARGS *args, int fixed)
{
    int x     = (int)args->t1;
    int y     = (int)args->t2;
    int alpha = (int)args->stack[1];

    if (MenuCamStatFuncCsrPos(fixed, &x, &y, &alpha) != 0)
    {
        B2D_HOOK_ARGS a = *args;
        a.t1       = x;
        a.t2       = y;
        a.stack[1] = alpha;
        B2dDrawSpriteDirect(&a);
    }
    return 0;
}

static int MenuCamStatFuncCsrHook(const B2D_HOOK_ARGS *args)          // FF3 0x001163a0
{
    return MenuCamStatFuncCsrDraw(args, 1);
}

static int MenuCamStatFuncCsrBlinkHook(const B2D_HOOK_ARGS *args)     // FF3 0x00116468
{
    return MenuCamStatFuncCsrDraw(args, 0);
}

// ──────────────────────────────────────────────────────────────────────
// Row items: half alpha unless their row is lit.

// R_film_01/04/05/06. FF3 0x00116530.
static int MenuCamStatFilmItemHook(const B2D_HOOK_ARGS *args)
{
    B2D_HOOK_ARGS a = *args;

    if (MenuCamStatIsRowLit(1) == 0)
        a.stack[1] = (int)a.stack[1] / 2;
    B2dDrawSpriteDirect(&a);
    return 0;
}

// R_renz_01. FF3 0x00116610.
static int MenuCamStatLensItemHook(const B2D_HOOK_ARGS *args)
{
    B2D_HOOK_ARGS a = *args;

    if (MenuCamStatIsRowLit(2) == 0 && MenuCamStatIsRowLit(4) == 0)
        a.stack[1] = (int)a.stack[1] / 2;
    B2dDrawSpriteDirect(&a);
    return 0;
}

// R_soubikinou01. FF3 0x00116700.
static int MenuCamStatFuncItemHook(const B2D_HOOK_ARGS *args)
{
    B2D_HOOK_ARGS a = *args;

    if (MenuCamStatIsRowLit(3) == 0)
        a.stack[1] = (int)a.stack[1] / 2;
    B2dDrawSpriteDirect(&a);
    return 0;
}

// ──────────────────────────────────────────────────────────────────────
// The camera level pips (rows at items 0x34db20): R_para_02 the empty ones
// after the level up to base + 3 (FF3 0x001167e0), R_para_01 one per
// point (FF3 0x00116968), each a pip width apart.

static const int menu_cam_camlv_item[MENU_CAM_STAT_CAMLV_NUM] = { 0x414, 0x41f, 0x426, 0x45a, 0x45e };

// callcheck: B2dPlaceInit (B2D_PLACE's constructor)
static int MenuCamStatCamLvEmptyHook(const B2D_HOOK_ARGS *args)
{
    int       xy[2];

    B2D_PLACE place(BpGetDrawHandle());
    for (int i = 0; i < MENU_CAM_STAT_CAMLV_NUM; i++)
    {
        int kind = CamLvGetKind(i);
        if (kind == -1)
            return 0;
        place.GetXY(menu_cam_camlv_item[i], xy);
        int base = CamLvGetBase(kind);
        int lv   = CamLvGet(kind) + base;
        int w    = place.GetW(menu_cam_camlv_item[i]);
        xy[0] += lv * w;
        for (int n = lv; n < base + 3; n++)
        {
            B2D_HOOK_ARGS a = *args;
            a.t1 = xy[0];
            a.t2 = xy[1];
            B2dDrawSpriteDirect(&a);
            xy[0] += w;
        }
    }
    return 0;
}

// callcheck: B2dPlaceInit (B2D_PLACE's constructor)
static int MenuCamStatCamLvHook(const B2D_HOOK_ARGS *args)
{
    int       xy[2];

    B2D_PLACE place(BpGetDrawHandle());
    for (int i = 0; i < MENU_CAM_STAT_CAMLV_NUM; i++)
    {
        int kind = CamLvGetKind(i);
        if (kind == -1)
            return 0;
        place.GetXY(menu_cam_camlv_item[i], xy);
        int lv = CamLvGet(kind);
        lv += CamLvGetBase(kind);
        int w  = place.GetW(menu_cam_camlv_item[i]);
        for (int n = 0; n < lv; n++)
        {
            B2D_HOOK_ARGS a = *args;
            a.t1 = xy[0];
            a.t2 = xy[1];
            B2dDrawSpriteDirect(&a);
            xy[0] += w;
        }
    }
    return 0;
}

// ──────────────────────────────────────────────────────────────────────
// R_renzana_01 / _02: the one-slot or the three-slot frame, whichever
// LENS_FUNC_SLOTS says. FF3 0x00116ac0.

// callcheck: B2dPlaceInit (B2D_PLACE's constructor)
static int MenuCamStatSlotFrameHook(const B2D_HOOK_ARGS *args)
{
    int       xy[2];
    int       alpha = (int)args->stack[1];

    if (MenuCamStatIsRowLit(2) == 0 && MenuCamStatIsRowLit(4) == 0)
        alpha /= 2;
    B2D_PLACE place(BpGetDrawHandle());
    B2D_ITEM *item = B2dGetDrawItem();
    const char *name = MenuCamIsLensMultiSlot() != 0 ? "R_renzana_02" : "R_renzana_01";
    if (strcmp(item->name, name) != 0)
        return 0;

    place.GetXY(MenuCamIsLensMultiSlot() != 0 ? MENU_CAM_STAT_THREE_SLOT : MENU_CAM_STAT_ONE_SLOT, xy);
    B2D_HOOK_ARGS a = *args;
    a.t1       = xy[0];
    a.t2       = xy[1];
    a.stack[1] = alpha;
    B2dDrawSpriteDirect(&a);
    return 0;
}

// ──────────────────────────────────────────────────────────────────────
// R_renz_blank: over the list row of each equipped lens. FF3 0x00116c08.

// callcheck: B2dPlaceInit (B2D_PLACE's constructor)
static int MenuCamStatEquipMarkHook(const B2D_HOOK_ARGS *args)
{
    int       xy[2];
    int       alpha = (int)args->stack[1];

    if (MenuCamStatIsRowLit(2) == 0 && MenuCamStatIsRowLit(4) == 0)
        alpha /= 2;
    B2D_PLACE place(BpGetDrawHandle());
    LENS_WRK *lens = MenuCamGetLensWrk();
    for (int slot = 0; slot < LENS_SLOT_NUM; slot++)
    {
        int no = lens->GetEquip((u_char)slot, MENU_CAM_STAT_CHR);
        if (no < 0)
            continue;
        int row = MenuCamLensFindRow(no);
        if (row < 0)
            continue;
        place.GetXY(menu_cam_lens_row[row].item, xy);
        B2D_HOOK_ARGS a = *args;
        a.t1       = xy[0];
        a.t2       = xy[1];
        a.stack[1] = alpha;
        B2dDrawSpriteDirect(&a);
    }
    return 0;
}

// ──────────────────────────────────────────────────────────────────────
// R_koyuu_02..05: the player's own abilities (level slots 3, 4) at places
// koyuu01 / koyuu02. FF3 0x00116d60.

// callcheck: B2dPlaceInit (B2D_PLACE's constructor)
static int MenuCamStatCamLvSpHook(const B2D_HOOK_ARGS *args)
{
    static const char *const place_name[2] = { "koyuu01", "koyuu02" };  // sdata 0x36b778

    int       xy[2];

    B2D_PLACE place(BpGetDrawHandle());
    B2D_ITEM *item = B2dGetDrawItem();
    for (int i = 3; i < MENU_CAM_STAT_CAMLV_NUM; i++)
    {
        int kind = CamLvGetSpKind(i);

        const MENU_CAM_CAMLV_SP *sp = MenuCamStatFindCamLvSp(menu_cam_camlv_sp, &kind);
        if (sp == nullptr || strcmp(item->name, sp->name) != 0)
            continue;

        place.GetXYByName(place_name[i - 3], xy);
        B2D_HOOK_ARGS a = *args;
        a.t1 = xy[0];
        a.t2 = xy[1];
        B2dDrawSpriteDirect(&a);
        return 0;
    }
    return 0;
}

// ──────────────────────────────────────────────────────────────────────
// The row highlights and the item hooks. FF3 0x00116eb8.

void CmenuCamStat::SetHooks()
{
    static const B2D_HOOK_REC hooks[] =     // data 0x34d590
    {
        { MenuCamStatFilmTypeHook,     0, 0x47f, nullptr },
        { MenuCamStatFilmNumHook,      0, 0x47d, nullptr },
        { MenuCamStatFilmSelHook,      0, 0x457, nullptr },
        { MenuCamStatFilmSelHook,      0, 0x3fa, nullptr },
        { MenuCamStatCamLvSpHook,      0, 0, "R_koyuu_02" },
        { MenuCamStatCamLvSpHook,      0, 0, "R_koyuu_03" },
        { MenuCamStatCamLvSpHook,      0, 0, "R_koyuu_04" },
        { MenuCamStatCamLvSpHook,      0, 0, "R_koyuu_05" },
        { MenuCamStatLensIconHook,     0, 0, "R_renz_zero" },
        { MenuCamStatLensIconHook,     0, 0, "R_renz_stn" },
        { MenuCamStatLensIconHook,     0, 0, "R_renz_slw" },
        { MenuCamStatLensIconHook,     0, 0, "R_renz_stp" },
        { MenuCamStatLensIconHook,     0, 0, "R_renz_crush" },
        { MenuCamStatLensIconHook,     0, 0, "R_renz_blast" },
        { MenuCamStatLensIconHook,     0, 0, "R_renz_serial" },
        { MenuCamStatLensIconHook,     0, 0, "R_renz_see" },
        { MenuCamStatLensIconHook,     0, 0, "R_renz_track" },
        { MenuCamStatEquipMarkHook,    0, 0, "R_renz_blank" },
        { MenuCamStatFuncIconHook,     0, 0, "R_soubi_01" },
        { MenuCamStatFuncIconHook,     0, 0, "R_soubi_02" },
        { MenuCamStatFuncIconHook,     0, 0, "R_soubi_03" },
        { MenuCamStatFuncIconHook,     0, 0, "R_soubi_04" },
        { MenuCamStatFuncIconHook,     0, 0, "R_soubi_05" },
        { MenuCamStatFuncIconHook,     0, 0, "R_soubi_06" },
        { MenuCamStatFuncIconHook,     0, 0, "R_soubi_07" },
        { MenuCamStatFuncIconHook,     0, 0, "R_soubi_08" },
        { MenuCamStatFuncIconHook,     0, 0, "R_soubi_09" },
        { MenuCamStatFuncIconHook,     0, 0, "R_soubi_L_01" },
        { MenuCamStatFuncIconHook,     0, 0, "R_soubi_L_02" },
        { MenuCamStatFuncIconHook,     0, 0, "R_soubi_L_03" },
        { MenuCamStatFuncIconHook,     0, 0, "R_soubi_L_04" },
        { MenuCamStatFuncIconHook,     0, 0, "R_soubi_L_05" },
        { MenuCamStatFuncIconHook,     0, 0, "R_soubi_L_06" },
        { MenuCamStatFuncIconHook,     0, 0, "R_soubi_L_07" },
        { MenuCamStatFuncIconHook,     0, 0, "R_soubi_L_08" },
        { MenuCamStatFuncIconHook,     0, 0, "R_soubi_L_09" },
        { MenuCamStatFuncCsrHook,      0, 0, "R_soubicasoru_01" },
        { MenuCamStatFuncCsrBlinkHook, 0, 0, "R_soubicasoru_02" },
        { MenuCamStatCamLvHook,        0, 0, "R_para_01" },
        { MenuCamStatCamLvEmptyHook,   0, 0, "R_para_02" },
        { MenuCamStatSlotFrameHook,    0, 0, "R_renzana_01" },
        { MenuCamStatSlotFrameHook,    0, 0, "R_renzana_02" },
        { MenuCamStatSlotCsrHook,      0, 0, "R_renzcasolu_01" },
        { MenuCamStatLensCsrHook,      0, 0, "R_renzcasolu_02" },
        { MenuCamStatSlotCsrBlinkHook, 0, 0, "R_renzcasolu_03" },
        { MenuCamStatLensCsrBlinkHook, 0, 0, "R_renzcasolu_04" },
        { MenuCamStatLensItemHook,     0, 0, "R_renz_01" },
        { MenuCamStatFuncItemHook,     0, 0, "R_soubikinou01" },
        { MenuCamStatFilmItemHook,     0, 0, "R_film_01" },
        { MenuCamStatFilmItemHook,     0, 0, "R_film_04" },
        { MenuCamStatFilmItemHook,     0, 0, "R_film_05" },
        { MenuCamStatFilmItemHook,     0, 0, "R_film_06" },
        { nullptr, -1, -1, nullptr },
    };

    sel.Add(handle, MENU_CAM_STAT_ROW_ITEM + 0);
    sel.Add(handle, MENU_CAM_STAT_ROW_ITEM + 1);
    sel.Add(handle, MENU_CAM_STAT_ROW_ITEM + 2);
    sel.SetCsr(handle, MENU_CAM_STAT_ROW_CSR);
    sel.Set(0);
    B2dSetHookList(handle, hooks);
}
