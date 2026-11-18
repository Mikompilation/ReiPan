/* ==========================================================================
 *  album/prg/album_view.c
 *
 *  The album edit page's layout layer (see album_view.h). FF3
 *  0x12a068..0x12d9f8, FF3's own code in FF2 album_view.c's slot: the
 *  message slots, the pages' selection (frames, arrows, slot cursors, menu
 *  and sort rows), the cover pictures and thumbnails, then the photo marks,
 *  the current photo and its info, and the layouts' life and animations.
 *  The two FF2 album_save.c names the matcher gave this run's head
 *  (AlbumSaveDispMain, AlbumSaveAlbumSelDisp) were link-order guesses.
 * ======================================================================== */
#include "album/prg/album_view.h"

#include <new>                          // operator new / delete
#include <string.h>                     // strlen, strncmp

#include "album/prg/album.h"            // AlbumInfoGetKind, AlbumPhoto*, AlbumDispCtrlInit
#include "album/prg/album_disp.h"       // AlbumDispGetCur*, AlbumPhotoPrint*, AlbumPhotoDisp
#include "common/cmn_fade_layer.h"      // GetCmnFadeLayer, CmnFadeLayer*
#include "graphics/graph2d/message.h"   // GetMsgDataAddr, PrintMsg*, STR_DAT, DISP_STR
#include "graphics/gs_reg.h"            // GetGsRegCached, SetGsRegCached
#include "graphics/motion/anicode.h"    // B2D_HOOK_REC, B2dSetHookList, B2dDrawItemDefault, SetCurColorPacked, SetDefaultGsDrawState
#include "ingame/menu/menu_draw_tex.h"  // MENU_PIC (the cover pictures)
#include "system/b2d/B2dCtl.h"          // B2dCtlFindItem, B2dCtlSetItemHook, B2dGetDrawItem, B2D_PART_*
#include "system/b2d/B2dSpr.h"          // B2dSprSetDisp / SetAlpha / Freeze*
#include "system/b2d/BpCtl.h"           // BpCtlGetHandleByFileNo, B2dDrawSpriteDirect
#include "system/b2d/BpObj.h"           // BP_OBJ (layouts, cursors)
#include "system/eeiop/cd_file_dat.h"   // ALBM_*_PHF
#include "system/os/system.h"           // GetSysWrk, SYS_WRK::GetLanguage
#include "ingame/menu/menu_mouse.h"    // PC: the mouse in menus

// Defined further down (FF3 address order).
static void AlbumViewFrameSetAlpha(s_char side, u_char alpha);
static void AlbumViewArrowSetAlpha(s_char side, u_char alpha);
static void AlbumViewPageCsrSetAlpha(s_char side, u_char alpha);
static int  AlbumViewPageIsOn(s_char side);

// ──────────────────────────────────────────────────────────────────────
// Data
// ──────────────────────────────────────────────────────────────────────

#define AV_SIDE_DY          0x10b       // y distance between the two album pages' marks
#define AV_MARK_DX          0x42        // mark grid: 8 columns ...
#define AV_MARK_DY          0x2a        // ... by 2 rows
#define AV_MARK_COLS        8
#define AV_FADE_SPEED       0x1e        // capCtl::Switch / Hide

// Items of ALBM_HENSYU_PHF.
enum
{
    AV_ITEM_FADED_A    = 0x3e8,         // drawn at the page alpha (AlbumViewDrawFaded)
    AV_ITEM_FADED_B    = 0x3e9,
    AV_ITEM_PHOTO_SUB  = 0x3ee,         // 0x3ee..0x3f2: shown with the photo
    AV_ITEM_PROTECT    = 0x407,         // the protect mark
    AV_ITEM_FLAG3_OFF  = 0x408,         // one of these two by AlbumPhotoIsFlag3
    AV_ITEM_FLAG3_ON   = 0x409,
    AV_ITEM_PHOTO      = 0x40a,         // the current photo (AlbumViewDrawPhoto)
    AV_ITEM_PHOTO_BASE = 0x40b,         // under it; half alpha when the slot is empty
    AV_ITEM_ROOM       = 0x40d,
    AV_ITEM_SUBJECT_1  = 0x40e,         // names end in '1' / '2'
    AV_ITEM_SUBJECT_2  = 0x40f,
    AV_ITEM_DATE       = 0x411,
    AV_ITEM_SCORE_A    = 0x412,         // "NUM.." the digits, else the label
    AV_ITEM_SCORE_B    = 0x413,
    AV_ITEM_COPY       = 0x414,         // AlbumViewShowCopyItem
};
#define AV_PHOTO_SUB_NUM    5

// Items of ALBM_THUMBNAIL_PHF frozen by AlbumViewFreezeItems: 1000.. and 0x3f4..
#define AV_THUMB_ITEM_A     1000
#define AV_THUMB_ITEM_B     0x3f4
#define AV_THUMB_NUM        5

// ALBM_CMN_PHF: the photo marks (per album page 16 slots; ROM 0x34e490 /
// 0x34e4d0). The first item draws them all through its hook, the others
// are hidden.
static const int album_view_mark_item[2][ALBUM_PHOTO_MAX] =
{
    { 0x417, 0x413, 0x40f, 0x41a, 0x416, 0x412, 0x40e, 0x3e8,
      0x415, 0x411, 0x40d, 0x418, 0x414, 0x410, 0x40c, 0x426 },
    { 0x419, 0x3fc, 0x3fe, 0x400, 0x402, 0x404, 0x406, 0x408,
      0x3fb, 0x3fd, 0x3ff, 0x401, 0x403, 0x405, 0x407, 0x41b },
};

// ALBM_CMN_PHF: the protect marks (ROM 0x34e510, all but the first hidden)
// and the four items that draw them (ROM 0x34e550).
static const int album_view_protect_item[ALBUM_PHOTO_MAX] =
{
    0x42a, 0x437, 0x4d6, 0x4d7, 0x4d8, 0x4d9, 0x4da, 0x4db,
    0x4d5, 0x4dc, 0x4dd, 0x4de, 0x4df, 0x4e0, 0x4e1, 0x4e2,
};
static const int album_view_protect_hook_item[4] = { 0x436, 0x433, 0x435, 0x434 };

// An item id with its label (ROM tables of the edit page's menus).
// layout: host -- name is a pointer.
struct ALBUM_VIEW_ITEM_NAME
{
    int         id;
    const char *name;
};

static const ALBUM_VIEW_ITEM_NAME album_view_menu_row[5] =     // ROM 0x34e9a0
{
    { 0x3f6, "R menu_kakudai" }, { 0x3f5, "R menu_sort" }, { 0x3f4, "R menu_save" },
    { 0x3f3, "R menu_load" },    { 0x3f2, "R menu_help" },
};
static const ALBUM_VIEW_ITEM_NAME album_view_sort_row[3] =     // ROM 0x34e9f8
{
    { 0x3f9, "R sort_time" }, { 0x3f8, "R sort_pnt" }, { 0x3f7, "R sort_hogo" },
};

// Per album page: its four frame items (ROM 0x34ea30 / 0x34ea50, through
// sdata 0x36ba28) and its two arrows (ROM 0x34ea70 / 0x34ea80, sdata 0x36ba30).
static const ALBUM_VIEW_ITEM_NAME album_view_waku_side0[4] =
{
    { 0x41d, "R waku_B" }, { 0x41e, "R waku_B" }, { 0x41f, "R waku_U" }, { 0x420, "R waku_U" },
};
static const ALBUM_VIEW_ITEM_NAME album_view_waku_side1[4] =
{
    { 0x3ed, "R waku_B" }, { 0x3ec, "R waku_U" }, { 0x409, "R waku_B" }, { 0x40a, "R waku_U" },
};
static const ALBUM_VIEW_ITEM_NAME album_view_arw_side0[2] = { { 0x422, "R arw" }, { 0x421, "R arw" } };
static const ALBUM_VIEW_ITEM_NAME album_view_arw_side1[2] = { { 0x3ef, "R arw" }, { 0x40b, "R arw" } };
static const ALBUM_VIEW_ITEM_NAME *const album_view_waku[2] = { album_view_waku_side0, album_view_waku_side1 };
static const ALBUM_VIEW_ITEM_NAME *const album_view_arw[2]  = { album_view_arw_side0, album_view_arw_side1 };

// The five layouts, + language (ROM 0x34e880).
static const int album_layout_file[ALBUM_LAYOUT_NUM] =
{
    ALBM_HENSYU_PHF, ALBM_KAKUDAI_PHF, ALBM_THUMBNAIL_PHF, ALBM_CMN_PHF, ALBM_SENTAKU_PHF,
};

static const char av_label_in[]  = "hensyu_in";        // ROM 0x34e610
static const char av_label_out[] = "hensyu_out";       // ROM 0x34e620

BP_OBJ *album_layout_obj[ALBUM_LAYOUT_NUM];     // bss 0x2f15b0

static u_char album_view_page_alpha[ALBUM_SIDE_NUM];    // sbss 0x372cb8 (gp-0x838)
static int    album_view_flag3_disp[2] = { 1, 0 };      // sdata 0x36b9a8: AV_ITEM_FLAG3_OFF / _ON shown

static int AlbumViewHandle(int file)
{
    return BpCtlGetHandleByFileNo(GetSysWrk()->GetLanguage() + file);
}

// The text item value the info hooks pass on (EE item +0x124).
static int AlbumViewDrawPri(void)
{
    return B2dGetDrawItem()->part[B2D_PART_TEXT].value;
}

// The info items' alpha: the page alpha of the shown side, scaled by the
// inverse of message slot 0's alpha (1/128 floats 0x36b9b0 / b4 / bc / c0 / c4, one
// per hook).
static int AlbumViewInfoAlpha(s_char side)
{
    return (int)((float)(int)(AlbumViewMsgGetAlpha(0, 1) * album_view_page_alpha[side]) * (1.0f / 128.0f)) & 0xff;
}

// ──────────────────────────────────────────────────────────────────────
// The selection and message state (bss 0x390c10.., sbss gp-0x854..)
// ──────────────────────────────────────────────────────────────────────

#define AV_MSG_FADE_STEP    0x80        // AlbumViewMsgStep: one step, i.e. at once
#define AV_ROW_ON           0x80        // menu / sort row RGB, enabled
#define AV_ROW_OFF          0x40

// ALBM_CMN items.
enum
{
    AV_ITEM_MENU_CSR   = 0x3eb,         // BP_OBJ cursors (AlbumViewSelInit)
    AV_ITEM_SORT_CSR   = 0x424,
    AV_ITEM_FADE       = 0x3f1,         // the fade layer item (AlbumViewLayoutLoad)
    AV_ITEM_MSG_0      = 0x3fa,         // message slot 0, whole
    AV_ITEM_MSG_LINE_1 = 0x458,         // message slots 1 / 2, one line each
    AV_ITEM_MSG_LINE_2 = 0x459,
    AV_ITEM_ICON_A     = 0x464,         // two help icons swapping sprites by the variant
    AV_ITEM_ICON_B     = 0x465,
    AV_ITEM_45A        = 0x45a,
    AV_ITEM_THUMBS     = 0x438,         // draws every thumbnail (AlbumViewDrawThumbs)
};

#define AV_MSG_LINE_DX      0x24        // the one-line messages start right of the item

static ALBUM_VIEW_PAGE_CSR album_view_page_csr[ALBUM_SIDE_NUM];  // bss 0x390c10
static ALBUM_VIEW_MSG      album_view_msg[ALBUM_VIEW_MSG_NUM];   // bss 0x390c20

static s_char   album_view_sel_page;            // sbss 0x372c9c (gp-0x854): -1 none
static u_char   album_view_msg_variant;         // sbss 0x372c9d (gp-0x853)
static int      album_view_icon_v[2];           // sbss 0x372ca0 (gp-0x850): part V of 0x465 / 0x464
static MENU_PIC *album_view_cover[ALBUM_SIDE_NUM];  // sbss 0x372ca8 (gp-0x848)
static int      album_view_page_on[ALBUM_SIDE_NUM]; // sbss 0x372cb0 (gp-0x840)

// A hook's alpha times message slot no's (1/128 floats 0x36b970.., one
// per hook).
static int AlbumViewMsgScale(s_char no, intptr_t alpha)
{
    return (int)((float)(AlbumViewMsgGetAlpha(no, 0) * (int)alpha) * (1.0f / 128.0f)) & 0xff;
}

// ──────────────────────────────────────────────────────────────────────
// Message slots
// ──────────────────────────────────────────────────────────────────────

// FF3 0x0012a068.
void AlbumViewMsgSet(s_char no, int type, int id)
{
    album_view_msg[no].req_id   = id;
    album_view_msg[no].req_type = type;
}

// FF3 0x0012a090.
void AlbumViewMsgInit(void)
{
    for (s_char i = 0; i < ALBUM_VIEW_MSG_NUM; i++)
    {
        ALBUM_VIEW_MSG *msg = &album_view_msg[i];

        msg->req_type = -1;
        msg->type     = -1;
        msg->req_id   = -1;
        msg->id       = -1;
        msg->alpha    = 0;
    }
}

// FF3 0x0012a0d8.
void AlbumViewMsgStep(void)
{
    for (s_char i = 0; i < ALBUM_VIEW_MSG_NUM; i++)
    {
        ALBUM_VIEW_MSG *msg = &album_view_msg[i];

        if (msg->req_type == msg->type && msg->req_id == msg->id)
        {
            if (msg->req_type != -1 && msg->req_id != -1)
            {
                if (msg->alpha + AV_MSG_FADE_STEP < 0x80)
                    msg->alpha += AV_MSG_FADE_STEP;
                else
                    msg->alpha = 0x80;
            }
        }
        else if (msg->req_type != -1 && msg->req_id != -1)
        {
            msg->type = msg->req_type;
            msg->id   = msg->req_id;
        }
        else if (msg->alpha - AV_MSG_FADE_STEP > 0)
        {
            msg->alpha -= AV_MSG_FADE_STEP;
        }
        else
        {
            msg->type  = msg->req_type;
            msg->id    = msg->req_id;
            msg->alpha = 0;
        }
    }
}

// FF3 0x0012a1a8.
static int AlbumViewMsgGetType(s_char no)
{
    return album_view_msg[no].type;
}

// FF3 0x0012a1d0.
static int AlbumViewMsgGetId(s_char no)
{
    return album_view_msg[no].id;
}

// FF3 0x0012a1f8.
u_char AlbumViewMsgGetAlpha(s_char no, int invert)
{
    if (invert != 0)
        return (u_char)(0x80 - album_view_msg[no].alpha);
    return album_view_msg[no].alpha;
}

// ──────────────────────────────────────────────────────────────────────
// The fade layer item
// ──────────────────────────────────────────────────────────────────────

// Item 0x3f1: the sprite with TEX1_1 0x100000141, the old TEX1 restored.
// FF3 0x0012a240 (was FF2's AlbumSaveDispMain, a wrong low match).
static int AlbumViewDrawFadeItem(const B2D_HOOK_ARGS *args)
{
    u_long tex1 = *GetGsRegCached(0x14);

    SetGsRegCached(0x100000141ULL, 0x14, 1);    // TEX1_1
    int ret = B2dDrawSpriteDirect(args);
    SetGsRegCached(tex1, 0x14, 1);
    return ret;
}

static const B2D_HOOK_REC album_view_fade_hooks[] =       // built on the stack by FF3
{
    { AlbumViewDrawFadeItem, 0, AV_ITEM_FADE, nullptr },
    { nullptr,              -1, -1,           nullptr },
};

// FF3 0x0012a340.
// callcheck: BpCtlGetHandleByFileNo, GetLanguage, GetSysWrk (AlbumViewHandle)
static void AlbumViewSetFadeItemHook(void)
{
    B2dSetHookList(AlbumViewHandle(ALBM_CMN_PHF), album_view_fade_hooks);
}

// ──────────────────────────────────────────────────────────────────────
// The pages' frames, arrows and slot cursors
// ──────────────────────────────────────────────────────────────────────

// FF3 0x0012a3d0.
// callcheck: BpCtlGetHandleByFileNo, GetLanguage, GetSysWrk (AlbumViewHandle)
static void AlbumViewFrameSetDisp(s_char side, int disp)
{
    int handle = AlbumViewHandle(ALBM_CMN_PHF);

    for (s_char i = 0; i < 4; i++)
        B2dSprSetDisp(B2dCtlFindItem(handle, album_view_waku[side][i].id), disp);
}

// FF3 0x0012a488.
// callcheck: BpCtlGetHandleByFileNo, GetLanguage, GetSysWrk (AlbumViewHandle)
static void AlbumViewFrameSetAlpha(s_char side, u_char alpha)
{
    int handle = AlbumViewHandle(ALBM_CMN_PHF);

    for (s_char i = 0; i < 4; i++)
        B2dSprSetAlpha(B2dCtlFindItem(handle, album_view_waku[side][i].id), alpha);
}

// The camera album's page never shows its arrows.
// FF3 0x0012a540.
// callcheck: BpCtlGetHandleByFileNo, GetLanguage, GetSysWrk (AlbumViewHandle)
void AlbumViewArrowSetDisp(s_char side, int disp)
{
    if (AlbumInfoIsCamera(side) != 0 && disp != 0)
        return;

    int handle = AlbumViewHandle(ALBM_CMN_PHF);

    for (s_char i = 0; i < 2; i++)
        B2dSprSetDisp(B2dCtlFindItem(handle, album_view_arw[side][i].id), disp);
}

// FF3 0x0012a610.
// callcheck: BpCtlGetHandleByFileNo, GetLanguage, GetSysWrk (AlbumViewHandle)
static void AlbumViewArrowSetAlpha(s_char side, u_char alpha)
{
    int handle = AlbumViewHandle(ALBM_CMN_PHF);

    for (s_char i = 0; i < 2; i++)
        B2dSprSetAlpha(B2dCtlFindItem(handle, album_view_arw[side][i].id), alpha);
}

// FF3 0x0012a6c8.
void AlbumViewPageCsrSetOn(s_char side, int on)
{
    album_view_page_csr[side].on = on;
}

// FF3 0x0012a6e8.
static void AlbumViewPageCsrSetAlpha(s_char side, u_char alpha)
{
    album_view_page_csr[side].alpha = alpha;
}

// FF3 0x0012a710.
void AlbumViewPageCsrSetGray(s_char side, u_char gray)
{
    album_view_page_csr[side].gray = gray;
}

// FF3 0x0012a738.
// callcheck: BpCtlGetHandleByFileNo, GetLanguage, GetSysWrk (AlbumViewHandle)
void AlbumViewPageCsrFreeze(s_char side, int mode)
{
    B2dSprFreezeItem(AlbumViewHandle(ALBM_CMN_PHF), side == 0 ? 0x42b : 0x42d, mode);
}

// The selection items: drawn with TEX1_1 0x100000161.
// FF3 0x0012a7a0.
static int AlbumViewDrawSmooth(const B2D_HOOK_ARGS *args)
{
    SetGsRegCached(0x100000161ULL, 0x14, 1);    // TEX1_1
    return B2dDrawItemDefault(args);
}

static const B2D_HOOK_REC album_view_sel_hooks[] =        // ROM 0x34e2c8
{
    { AlbumViewDrawSmooth, 0, AV_ITEM_MENU_CSR, nullptr },
    { AlbumViewDrawSmooth, 0, AV_ITEM_SORT_CSR, nullptr },
    { AlbumViewDrawSmooth, 0, 0x3ea,            nullptr },
    { AlbumViewDrawSmooth, 0, 0x431,            nullptr },
    { AlbumViewDrawSmooth, 0, 0x423,            nullptr },
    { nullptr,            -1, -1,               nullptr },
};

static const char av_label_albmslct[] = "hensyu_albmslct";    // ROM 0x34e328

// FF3 copies the ROM list to the stack first.
// FF3 0x0012a880.
// callcheck: BpCtlGetHandleByFileNo, GetLanguage, GetSysWrk (AlbumViewHandle)
static void AlbumViewSelInit(void)
{
    for (s_char i = 0; i < ALBUM_SIDE_NUM; i++)
    {
        ALBUM_VIEW_PAGE_CSR *csr = &album_view_page_csr[i];

        csr->pos   = 0;
        csr->gray  = 0x80;
        csr->alpha = 0x80;
        csr->on    = 0;
    }
    album_layout_obj[ALBUM_LAYOUT_CMN]->AddCsr(AV_ITEM_MENU_CSR, 0, 0x25, 0);
    album_layout_obj[ALBUM_LAYOUT_CMN]->AddCsr(AV_ITEM_SORT_CSR, 0, 0x25, 0);
    B2dSetHookList(AlbumViewHandle(ALBM_CMN_PHF), album_view_sel_hooks);
    BpPlayLabel(AlbumViewHandle(ALBM_CMN_PHF), av_label_albmslct);

    for (s_char side = 0; side < ALBUM_SIDE_NUM; side++)
    {
        AlbumViewFrameSetAlpha(side, 0);
        AlbumViewFrameSetDisp(side, 0);
        AlbumViewArrowSetDisp(side, 0);
        AlbumViewPageCsrSetOn(side, 0);
    }
    album_view_sel_page = -1;
}

// FF3 0x0012aa08.
void AlbumViewSelectPage(s_char side)
{
    if (album_view_sel_page != side)
    {
        AlbumViewFrameSetDisp(side, 1);
        AlbumViewArrowSetDisp(side, 1);
        if (album_view_sel_page != -1)
        {
            AlbumViewFrameSetDisp(album_view_sel_page, 0);
            AlbumViewArrowSetDisp(album_view_sel_page, 0);
        }
    }
    album_view_sel_page = side;
}

// FF3 0x0012aa78.
void AlbumViewSelectSlot(s_char side, s_char pos)
{
    album_view_page_csr[side].pos = pos;
    if (album_view_sel_page != side)
    {
        AlbumViewFrameSetDisp(side, 1);
        AlbumViewArrowSetDisp(side, 1);
        AlbumViewPageCsrSetOn(side, 1);
        if (album_view_sel_page != -1)
        {
            AlbumViewFrameSetDisp(album_view_sel_page, 0);
            AlbumViewArrowSetDisp(album_view_sel_page, 0);
            AlbumViewPageCsrSetOn(album_view_sel_page, 0);
        }
    }
    album_view_sel_page = side;
}

// FF3 0x0012ab28.
void AlbumViewPageCsrSetPos(s_char side, s_char pos)
{
    album_view_page_csr[side].pos = pos;
    if (album_view_sel_page != side)
    {
        AlbumViewPageCsrSetOn(side, 1);
        album_view_sel_page = side;
    }
}

// ──────────────────────────────────────────────────────────────────────
// The menu and sort rows
// ──────────────────────────────────────────────────────────────────────

// FF3 0x0012ab88.
// callcheck: BpCtlGetHandleByFileNo, GetLanguage, GetSysWrk (AlbumViewHandle)
void AlbumViewMenuRowSetRgb(u_char mask)
{
    int handle = AlbumViewHandle(ALBM_CMN_PHF);

    for (s_char i = 0; i < 5; i++)
    {
        u_int c = (mask >> i & 1) ? AV_ROW_ON : AV_ROW_OFF;

        B2dSprSetRgb(B2dCtlFindItem(handle, album_view_menu_row[i].id), c, c, c);
    }
}

// FF3 0x0012ac50.
void AlbumViewMenuCsrSet(s_char pos)
{
    album_layout_obj[ALBUM_LAYOUT_CMN]->SetCsrDisp(AV_ITEM_MENU_CSR, pos >= 0);
    album_layout_obj[ALBUM_LAYOUT_CMN]->SetCsrPos(AV_ITEM_MENU_CSR, 0, pos);
}

// FF3 0x0012aca8 (was FF2's AlbumSaveAlbumSelDisp, a wrong low match).
// callcheck: BpCtlGetHandleByFileNo, GetLanguage, GetSysWrk (AlbumViewHandle)
void AlbumViewSortRowSetRgb(u_char mask)
{
    int handle = AlbumViewHandle(ALBM_CMN_PHF);

    for (s_char i = 0; i < 3; i++)
    {
        u_int c = (mask >> i & 1) ? AV_ROW_ON : AV_ROW_OFF;

        B2dSprSetRgb(B2dCtlFindItem(handle, album_view_sort_row[i].id), c, c, c);
    }
}

// FF3 0x0012ad70.
void AlbumViewSortCsrSet(s_char pos)
{
    album_layout_obj[ALBUM_LAYOUT_CMN]->SetCsrDisp(AV_ITEM_SORT_CSR, pos >= 0);
    album_layout_obj[ALBUM_LAYOUT_CMN]->SetCsrPos(AV_ITEM_SORT_CSR, 0, pos);
}

// FF3 0x0012adc8.
static void AlbumViewCsrFree(void)
{
    album_layout_obj[ALBUM_LAYOUT_CMN]->FreeCsr();
}

// ──────────────────────────────────────────────────────────────────────
// Help icons and message lines (ALBM_CMN)
// ──────────────────────────────────────────────────────────────────────

// FF3 0x0012ade8.
void AlbumViewSetMsgVariant(u_char variant)
{
    album_view_msg_variant = variant;
}

// Item 0x464 with the other variant's sprite (V), alpha times message 1's.
// FF3 0x0012adf8.
// callcheck: AlbumViewMsgGetAlpha (AlbumViewMsgScale)
static int AlbumViewDrawIconA(const B2D_HOOK_ARGS *args)
{
    B2D_HOOK_ARGS draw = *args;

    draw.stack[1] = AlbumViewMsgScale(1, args->stack[1]);
    draw.a2       = album_view_icon_v[(s_char)(album_view_msg_variant ^ 1)];
    return B2dDrawItemDefault(&draw);
}

// Item 0x465 with its variant's sprite.
// FF3 0x0012af08.
// callcheck: AlbumViewMsgGetAlpha (AlbumViewMsgScale)
static int AlbumViewDrawIconB(const B2D_HOOK_ARGS *args)
{
    B2D_HOOK_ARGS draw = *args;

    draw.stack[1] = AlbumViewMsgScale(1, args->stack[1]);
    draw.a2       = album_view_icon_v[(s_char)album_view_msg_variant];
    return B2dDrawItemDefault(&draw);
}

// Message slot no as one centred line right of the text item (t1 xy,
// stack[2] alpha), bright yellow when lit, else paler.
static void AlbumViewPrintMsgLine(const B2D_HOOK_ARGS *args, s_char no, int lit)
{
    const int *xy   = (const int *)args->t1;
    int        type = AlbumViewMsgGetType(no);
    int        id   = AlbumViewMsgGetId(no);

    if (type != -1 && id != -1)
    {
        int pri   = AlbumViewDrawPri();
        int alpha = AlbumViewMsgScale(no, args->stack[2]);

        if (lit)
            SetCurColorRGB(0xfb, 0xf4, 0xab);
        else
            SetCurColorRGB(0xd6, 0xf4, 0xab);
        PrintMsg_ArrangeOneLine(GetMsgDataAddr(type, id), xy[0] + AV_MSG_LINE_DX, xy[1],
                                MSG_COL_CURRENT, alpha, pri, 0, 0, 2);
    }
    SetDefaultGsDrawState();
}

// Item 0x459: message slot 2, lit on variant 1.
// FF3 0x0012b010 (no Ghidra function before 2026-10-20: inside 0x12af08).
// callcheck: AlbumViewMsgGetType, AlbumViewMsgGetId, AlbumViewMsgGetAlpha, B2dGetDrawItem, SetCurColorRGB, GetMsgDataAddr, PrintMsg_ArrangeOneLine, SetDefaultGsDrawState (AlbumViewPrintMsgLine)
static int AlbumViewDrawMsgLine2(const B2D_HOOK_ARGS *args)
{
    AlbumViewPrintMsgLine(args, 2, album_view_msg_variant == 1);
    return 0;
}

// Item 0x458: message slot 1, lit on variant 0.
// FF3 0x0012b130.
// callcheck: AlbumViewMsgGetType, AlbumViewMsgGetId, AlbumViewMsgGetAlpha, B2dGetDrawItem, SetCurColorRGB, GetMsgDataAddr, PrintMsg_ArrangeOneLine, SetDefaultGsDrawState (AlbumViewPrintMsgLine)
static int AlbumViewDrawMsgLine1(const B2D_HOOK_ARGS *args)
{
    AlbumViewPrintMsgLine(args, 1, album_view_msg_variant == 0);
    return 0;
}

// Item 0x3fa: message slot 0 printed whole in the item's colour.
// FF3 0x0012b250.
// callcheck: B2dGetDrawItem, AlbumViewMsgGetAlpha (AlbumViewDrawPri, AlbumViewMsgScale)
static int AlbumViewDrawMsg0(const B2D_HOOK_ARGS *args)
{
    const int *xy  = (const int *)args->t1;
    int        pri = AlbumViewDrawPri();

    SetCurColorPacked((u_int)args->t3);
    int type = AlbumViewMsgGetType(0);
    int id   = AlbumViewMsgGetId(0);
    if (type != -1 && id != -1)
    {
        STR_DAT  dat;
        DISP_STR disp;

        dat.str   = nullptr;
        dat.pos_x = xy[0];
        dat.pos_y = xy[1];
        dat.type  = 1;
        dat.r     = 0;
        dat.g     = 0;
        dat.b     = 0;
        dat.alpha = AlbumViewMsgScale(0, args->stack[2]);
        dat.pri   = pri;
        CopyStrDToStr(&disp, &dat);
        disp.str    = GetMsgDataAddr(type, id);
        disp.x_wide = 0;
        MsgColChange(&disp, MSG_COL_CURRENT);
        PrintMsgZ(&disp, 0);
    }
    SetDefaultGsDrawState();
    return 0;
}

static const B2D_HOOK_REC album_view_msg_hooks[] =        // ROM 0x34e338
{
    { AlbumViewDrawIconA,    0, AV_ITEM_ICON_A,     nullptr },
    { AlbumViewDrawIconB,    0, AV_ITEM_ICON_B,     nullptr },
    { AlbumViewDrawMsgLine2, 0, AV_ITEM_MSG_LINE_2, nullptr },
    { AlbumViewDrawMsgLine1, 0, AV_ITEM_MSG_LINE_1, nullptr },
    { AlbumViewDrawMsg0,     0, AV_ITEM_MSG_0,      nullptr },
    { nullptr,              -1, -1,                 nullptr },
};

// FF3 copies the ROM list to the stack first.
// FF3 0x0012b390.
// callcheck: BpCtlGetHandleByFileNo, GetLanguage, GetSysWrk (AlbumViewHandle)
static void AlbumViewSetMsgHooks(void)
{
    int handle = AlbumViewHandle(ALBM_CMN_PHF);

    B2dSetHookList(handle, album_view_msg_hooks);
    album_view_icon_v[0] = B2dSprGetPartData(B2dCtlFindItem(handle, AV_ITEM_ICON_B), B2D_PART_V);
    album_view_icon_v[1] = B2dSprGetPartData(B2dCtlFindItem(handle, AV_ITEM_ICON_A), B2D_PART_V);
}

// FF3 0x0012b458.
// callcheck: BpCtlGetHandleByFileNo, GetLanguage, GetSysWrk (AlbumViewHandle)
void AlbumViewShowItem45a(int on)
{
    B2dSprSetAlpha(B2dCtlFindItem(AlbumViewHandle(ALBM_CMN_PHF), AV_ITEM_45A), on != 0 ? 0x80 : 0);
}

// ──────────────────────────────────────────────────────────────────────
// The cover pictures (ALBM_THUMBNAIL)
// ──────────────────────────────────────────────────────────────────────

// FF3 0x0012b4c0.
static int AlbumViewPageIsOn(s_char side)
{
    return album_view_page_on[side];
}

// FF3 0x0012b4e0.
void AlbumViewPageSetOn(s_char side, int on)
{
    album_view_page_on[side] = on;
}

// Items 0x3f2 / 0x3ed (names end in 'A' / 'B', the side): the side's
// picture wants the album kind's cover (ALBM0_TM2 + kind), blank without
// an album or while the page is off.
// FF3 0x0012b500.
static int AlbumViewDrawCover(const B2D_HOOK_ARGS *args)
{
    const char *name = (const char *)args->a0;
    s_char      side = (s_char)(name[strlen(name) - 1] - 'A');
    s_char      kind = AlbumInfoGetKind(side);
    int         on   = AlbumViewPageIsOn(side);

    if (kind < 0 || on == 0)
        album_view_cover[side]->pic = MENU_DRAW_TEX_BLANK;
    else
        album_view_cover[side]->pic = kind + ALBM0_TM2;
    album_view_cover[side]->Step();
    return 0;
}

// The cover's five cells (u, v of each in the picture; ROM 0x34e398).
static const int album_view_cover_cell[5][2] =
{
    { 0x01, 0x01 }, { 0x01, 0x6e }, { 0x01, 0xdb }, { 0x56, 0xdb }, { 0xab, 0xdb },
};

// Items 0x3e8..0x3ec / 0x3f4..0x3f8 (children of the cover items; names
// end in the cell '1'..'5'): the side's picture draws that cell at the
// item's place, size, alpha and text pri.
// FF3 0x0012b598 (no Ghidra function before 2026-10-20: inside 0x12b500).
// callcheck: BpCtlGetHandleByFileNo, GetLanguage, GetSysWrk (AlbumViewHandle)
static int AlbumViewDrawCoverCell(const B2D_HOOK_ARGS *args)
{
    B2D_ITEM   *item   = B2dGetDrawItem();
    B2D_ITEM   *parent = B2dSprGetParent(AlbumViewHandle(ALBM_THUMBNAIL_PHF), item);
    const char *pname  = parent->name;
    s_char      side   = (s_char)(pname[strlen(pname) - 1] - 'A');
    s_char      cell   = (s_char)(item->name[strlen(item->name) - 1] - '1');
    int         w      = B2dSprGetPartData(item, B2D_PART_W);
    int         h      = B2dSprGetPartData(item, B2D_PART_H);
    int         x      = B2dSprGetPartData(item, B2D_PART_X);
    int         y      = B2dSprGetPartData(item, B2D_PART_Y);
    int         alpha  = B2dSprGetPartData(item, B2D_PART_ALPHA);
    int         pri    = B2dSprGetPartData(item, B2D_PART_TEXT);
    MENU_PIC   *pic    = album_view_cover[side];

    (void)args;
    pic->v     = album_view_cover_cell[cell][1];
    pic->u     = album_view_cover_cell[cell][0];
    pic->h     = h;
    pic->w     = w;
    pic->pri   = (short)pri;
    pic->x     = (short)x;
    pic->y     = (short)y;
    pic->z     = 0;
    pic->scale = (float)alpha * (1.0f / 128.0f);     // 0x36b984
    pic->Draw();
    pic->DrawNext();
    SetDefaultGsDrawState();
    return 0;
}

static const B2D_HOOK_REC album_view_cover_hooks[] =      // ROM 0x34e3c0
{
    { AlbumViewDrawCover,     0, 0x3f2, nullptr },
    { AlbumViewDrawCover,     0, 0x3ed, nullptr },
    { AlbumViewDrawCoverCell, 0, 0x3f8, nullptr },
    { AlbumViewDrawCoverCell, 0, 0x3f7, nullptr },
    { AlbumViewDrawCoverCell, 0, 0x3f6, nullptr },
    { AlbumViewDrawCoverCell, 0, 0x3f5, nullptr },
    { AlbumViewDrawCoverCell, 0, 0x3f4, nullptr },
    { AlbumViewDrawCoverCell, 0, 0x3e8, nullptr },
    { AlbumViewDrawCoverCell, 0, 0x3e9, nullptr },
    { AlbumViewDrawCoverCell, 0, 0x3ea, nullptr },
    { AlbumViewDrawCoverCell, 0, 0x3eb, nullptr },
    { AlbumViewDrawCoverCell, 0, 0x3ec, nullptr },
    { nullptr,               -1, -1,    nullptr },
};

// FF3 copies the ROM list to the stack first.
// FF3 0x0012b7b8.
// callcheck: BpCtlGetHandleByFileNo, GetLanguage, GetSysWrk, operator_new, MenuPicCtor (AlbumViewHandle, new MENU_PIC)
static void AlbumViewCoverInit(int pic_mode)
{
    B2dSetHookList(AlbumViewHandle(ALBM_THUMBNAIL_PHF), album_view_cover_hooks);
    for (s_char i = 0; i < ALBUM_SIDE_NUM; i++)
    {
        MENU_PIC *pic = new MENU_PIC(pic_mode);

        album_view_cover[i] = pic;
        pic->enable = 1;
        AlbumViewPageSetOn(i, 1);
    }
}

// FF3 0x0012b8b8.
// callcheck: MenuPicDtor, operator_delete (delete)
static void AlbumViewCoverRelease(void)
{
    for (s_char i = 0; i < ALBUM_SIDE_NUM; i++)
    {
        delete album_view_cover[i];
        album_view_cover[i] = nullptr;
    }
}

// ──────────────────────────────────────────────────────────────────────
// Page alpha
// ──────────────────────────────────────────────────────────────────────

// FF3 0x0012b928.
void AlbumViewPageAlphaReset(s_char side)
{
    album_view_page_alpha[side] = 0;
}

// FF3 0x0012b940.
u_char AlbumViewPageFadeIn(s_char side)
{
    u_char *alpha = &album_view_page_alpha[side];

    if (*alpha + 0x10 < 0x80)
        *alpha += 0x10;
    else
        *alpha = 0x80;
    return album_view_page_alpha[side];
}

// FF3 0x0012b988.
u_char AlbumViewPageFadeOut(s_char side)
{
    u_char *alpha = &album_view_page_alpha[side];

    if (*alpha - 0x20 < 1)
        *alpha = 0;
    else
        *alpha -= 0x20;
    return album_view_page_alpha[side];
}

// ──────────────────────────────────────────────────────────────────────
// The slot cursors and thumbnails (ALBM_CMN)
// ──────────────────────────────────────────────────────────────────────

// Items 0x42b / 0x3e9: the sprite at each page's cursor slot that is on
// (the 8 x 2 grid, the second page 0x10b lower), alpha the page alpha
// times the cursor's (1/128 float 0x36b988), RGB its grey (FF3 leaves
// the top byte of the packed word to whatever s0 held; the sprite draw
// ignores it).
// FF3 0x0012b9c8.
static int AlbumViewDrawPageCsr(const B2D_HOOK_ARGS *args)
{
    for (s_char side = 0; side < ALBUM_SIDE_NUM; side++)
    {
        const ALBUM_VIEW_PAGE_CSR *csr = &album_view_page_csr[side];

        if (csr->on != 0)
        {
            s_char        row  = csr->pos / AV_MARK_COLS;
            s_char        col  = (s_char)(csr->pos - row * AV_MARK_COLS);
            u_int         gray = csr->gray;
            B2D_HOOK_ARGS draw = *args;

            draw.t1       = args->t1 + col * AV_MARK_DX;
            draw.t2       = args->t2 + row * AV_MARK_DY + side * AV_SIDE_DY;
            draw.stack[1] = (int)((float)(album_view_page_alpha[side] * csr->alpha) * (1.0f / 128.0f)) & 0xff;
            draw.stack[2] = gray | gray << 8 | gray << 16;
            B2dDrawSpriteDirect(&draw);
        }
    }
    return 0;
}

static const B2D_ITEM_HOOK album_view_page_csr_hook = { AlbumViewDrawPageCsr };  // FF3 builds it on the stack

// sdata 0x36b990 / 0x36b998.
static const int album_view_csr_item[ALBUM_SIDE_NUM]      = { 0x42b, 0x3e9 };
static const int album_view_csr_item_hide[ALBUM_SIDE_NUM] = { 0x42d, 0x41c };

// FF3 0x0012bb60.
// callcheck: BpCtlGetHandleByFileNo, GetLanguage, GetSysWrk (AlbumViewHandle)
static void AlbumViewSetPageCsrHooks(void)
{
    int handle = AlbumViewHandle(ALBM_CMN_PHF);

    for (s_char i = 0; i < ALBUM_SIDE_NUM; i++)
    {
        int id = album_view_csr_item[i];

        B2dSprSetDisp(B2dCtlFindItem(handle, album_view_csr_item_hide[i]), 0);
        B2dSprFreezePart(handle, id, B2D_PART_ALPHA, 0);
        B2dCtlSetItemHook(B2dCtlFindItem(handle, id), &album_view_page_csr_hook);
    }
}

// Item 0x438 (a text item: a1 w, a2 h, t1 xy): the thumbnail of every used
// slot of both pages in the 8 x 2 grid, at the page's alpha.
// FF3 0x0012bc78.
// callcheck: B2dGetDrawItem (AlbumViewDrawPri)
static int AlbumViewDrawThumbs(const B2D_HOOK_ARGS *args)
{
    const int *xy     = (const int *)args->t1;
    int        pri    = AlbumViewDrawPri();
    int        y_side = 0;

    for (s_char side = 0; side < ALBUM_SIDE_NUM; side++, y_side += AV_SIDE_DY)
    {
        u_char alpha = album_view_page_alpha[side];

        for (s_char i = 0; i < ALBUM_PHOTO_MAX; i++)
        {
            s_char row = i / AV_MARK_COLS;
            s_char col = (s_char)(i - row * AV_MARK_COLS);

            if (AlbumPhotoIsUsed(side, i))
                AlbumPhotoThumbDisp(side, i, xy[0] + col * AV_MARK_DX, xy[1] + row * AV_MARK_DY + y_side + 2,
                                    (int)args->a1, (int)args->a2, alpha, pri);
        }
    }
    SetDefaultGsDrawState();
    return 0;
}

static const B2D_ITEM_HOOK album_view_thumb_hook = { AlbumViewDrawThumbs };  // FF3 builds it on the stack

// The thumbnail items of the two pages (sdata 0x36b9a0): 0x438 draws them
// all, the other 31 are hidden.
static const int album_view_thumb_item[ALBUM_SIDE_NUM] = { AV_ITEM_THUMBS, 0x448 };

// FF3 0x0012bdc0.
// callcheck: BpCtlGetHandleByFileNo, GetLanguage, GetSysWrk (AlbumViewHandle)
static void AlbumViewSetThumbHook(void)
{
    int handle = AlbumViewHandle(ALBM_CMN_PHF);

    for (s_char l = 0; l < ALBUM_SIDE_NUM; l++)
    {
        for (s_char i = (l == 0); i < ALBUM_PHOTO_MAX; i++)
            B2dSprSetDisp(B2dCtlFindItem(handle, album_view_thumb_item[l] + i), 0);
    }
    B2dSprFreezePart(handle, album_view_thumb_item[0], B2D_PART_ALPHA, 0);

    B2D_ITEM *item = B2dCtlFindItem(handle, album_view_thumb_item[0]);
    B2dSprSetAlpha(item, 0x80);
    B2dCtlSetItemHook(item, &album_view_thumb_hook);
}

// ──────────────────────────────────────────────────────────────────────
// The photo and protect marks (ALBM_CMN)
// ──────────────────────────────────────────────────────────────────────

// Item 0x417's sprite once per used photo slot of both album pages: slot i
// in an 8 x 2 grid (0x42 x 0x2a), the second page 0x10b lower, at the
// page's alpha.
// FF3 0x0012bef8 (was FF2's SetAlbumViewDispPhotoNo, a wrong low match).
static int AlbumViewDrawPhotoMarks(const B2D_HOOK_ARGS *args)
{
    int y_side = 0;

    for (s_char side = 0; side < ALBUM_SIDE_NUM; side++, y_side += AV_SIDE_DY)
    {
        u_char alpha = album_view_page_alpha[side];

        for (s_char i = 0; i < ALBUM_PHOTO_MAX; i++)
        {
            s_char row = i / AV_MARK_COLS;
            s_char col = i - row * AV_MARK_COLS;

            if (AlbumPhotoIsUsed(side, i))
            {
                B2D_HOOK_ARGS draw = *args;

                draw.t1       = args->t1 + col * AV_MARK_DX;
                draw.t2       = args->t2 + row * AV_MARK_DY + y_side;
                draw.stack[1] = alpha;
                B2dDrawSpriteDirect(&draw);
            }
        }
    }
    return 0;
}

static const B2D_ITEM_HOOK album_view_photo_mark_hook = { AlbumViewDrawPhotoMarks };  // FF3 builds it on the stack

// FF3 copies both ROM lists to the stack first.
// FF3 0x0012c058 (was AlbumViewTopPad, a wrong low FF2 match).
// callcheck: BpCtlGetHandleByFileNo, GetLanguage, GetSysWrk (AlbumViewHandle)
static void AlbumViewSetPhotoMarkHook(void)
{
    int handle = AlbumViewHandle(ALBM_CMN_PHF);

    for (s_char l = 0; l < 2; l++)
    {
        for (s_char i = (l == 0); i < ALBUM_PHOTO_MAX; i++)
            B2dSprSetDisp(B2dCtlFindItem(handle, album_view_mark_item[l][i]), 0);
    }
    B2dSprFreezePart(handle, album_view_mark_item[0][0], B2D_PART_ALPHA, 0);
    B2dCtlSetItemHook(B2dCtlFindItem(handle, album_view_mark_item[0][0]), &album_view_photo_mark_hook);
}

// As AlbumViewDrawPhotoMarks for the protected slots.
// FF3 0x0012c1f0 (was AlbumViewBaseDisp, a wrong low FF2 match).
static int AlbumViewDrawProtectMarks(const B2D_HOOK_ARGS *args)
{
    int y_side = 0;

    for (s_char side = 0; side < ALBUM_SIDE_NUM; side++, y_side += AV_SIDE_DY)
    {
        u_char alpha = album_view_page_alpha[side];

        for (s_char i = 0; i < ALBUM_PHOTO_MAX; i++)
        {
            s_char row = i / AV_MARK_COLS;
            s_char col = i - row * AV_MARK_COLS;

            if (AlbumPhotoIsProtected(side, i))
            {
                B2D_HOOK_ARGS draw = *args;

                draw.t1       = args->t1 + col * AV_MARK_DX;
                draw.t2       = args->t2 + row * AV_MARK_DY + y_side;
                draw.stack[1] = alpha;
                B2dDrawSpriteDirect(&draw);
            }
        }
    }
    return 0;
}

static const B2D_ITEM_HOOK album_view_protect_mark_hook = { AlbumViewDrawProtectMarks };

// FF3 copies the ROM lists to the stack first.
// FF3 0x0012c350.
// callcheck: BpCtlGetHandleByFileNo, GetLanguage, GetSysWrk (AlbumViewHandle)
static void AlbumViewSetProtectMarkHooks(void)
{
    int handle = AlbumViewHandle(ALBM_CMN_PHF);

    for (s_char i = 1; i < ALBUM_PHOTO_MAX; i++)
        B2dSprSetDisp(B2dCtlFindItem(handle, album_view_protect_item[i]), 0);

    for (s_char i = 0; i < 4; i++)
    {
        int id = album_view_protect_hook_item[i];

        B2dSprFreezePart(handle, id, B2D_PART_ALPHA, 0);
        B2dCtlSetItemHook(B2dCtlFindItem(handle, id), &album_view_protect_mark_hook);
    }
}

// ──────────────────────────────────────────────────────────────────────
// The current photo (ALBM_HENSYU)
// ──────────────────────────────────────────────────────────────────────

// Item 0x40a (a text item: a1 w, a2 h, t1 xy, stack[2] alpha): the shown
// side's current photo.
// FF3 0x0012c4b8.
// callcheck: B2dGetDrawItem (AlbumViewDrawPri)
static int AlbumViewDrawPhoto(const B2D_HOOK_ARGS *args)
{
    const int *xy  = (const int *)args->t1;
    int        pri = AlbumViewDrawPri();

    SetCurColorPacked((u_int)args->t3);
    s_char side = AlbumDispGetCurAlbum();
    s_char no   = AlbumDispGetCurPhoto();
    AlbumPhotoDisp(side, no, xy[0], xy[1], (int)args->a1, (int)args->a2, (u_char)args->stack[2], pri);
    SetDefaultGsDrawState();
    return 0;
}

static const B2D_HOOK_REC album_view_photo_hooks[] =      // built on the stack by FF3
{
    { AlbumViewDrawPhoto, 0, AV_ITEM_PHOTO, nullptr },
    { nullptr,           -1, -1,            nullptr },
};

// FF3 0x0012c558.
// callcheck: BpCtlGetHandleByFileNo, GetLanguage, GetSysWrk (AlbumViewHandle)
static void AlbumViewSetPhotoHook(void)
{
    B2dSetHookList(AlbumViewHandle(ALBM_HENSYU_PHF), album_view_photo_hooks);
    AlbumDispCtrlInit();
}

// The photo item and its base at alpha (hidden at 0; with an empty slot
// only the base, at half alpha), and the five items that go with them.
// FF3 0x0012c5f0.
// callcheck: BpCtlGetHandleByFileNo, GetLanguage, GetSysWrk (AlbumViewHandle)
static void AlbumViewShowPhoto(u_char alpha)
{
    int       handle = AlbumViewHandle(ALBM_HENSYU_PHF);
    B2D_ITEM *photo  = B2dCtlFindItem(handle, AV_ITEM_PHOTO);
    B2D_ITEM *base   = B2dCtlFindItem(handle, AV_ITEM_PHOTO_BASE);
    s_char    side   = AlbumDispGetCurAlbum();
    s_char    no     = AlbumDispGetCurPhoto();

    B2dSprSetAlpha(photo, alpha);
    B2dSprSetAlpha(base, alpha);
    if (alpha == 0)
    {
        B2dSprSetDisp(photo, 0);
        B2dSprSetDisp(base, 0);
    }
    else if (AlbumPhotoIsUsed(side, no) == 0)
    {
        B2dSprSetDisp(photo, 0);
        B2dSprSetDisp(base, 1);
        B2dSprSetAlpha(base, alpha >> 1);
    }
    else
    {
        B2dSprSetDisp(photo, 1);
        B2dSprSetDisp(base, 1);
    }

    for (s_char i = 0; i < AV_PHOTO_SUB_NUM; i++)
    {
        B2D_ITEM *item = B2dCtlFindItem(handle, AV_ITEM_PHOTO_SUB + i);

        B2dSprSetAlpha(item, alpha);
        B2dSprSetDisp(item, alpha != 0);
    }
}

// Items 0x408 / 0x409 at alpha: 0x409 for a photo with byte +3 set, 0x408
// for one without; an empty slot keeps the last choice.
// FF3 0x0012c778.
// callcheck: BpCtlGetHandleByFileNo, GetLanguage, GetSysWrk (AlbumViewHandle)
static void AlbumViewShowPhotoFlag3(u_char alpha)
{
    int    handle = AlbumViewHandle(ALBM_HENSYU_PHF);
    s_char side   = AlbumDispGetCurAlbum();
    s_char no     = AlbumDispGetCurPhoto();

    if (AlbumPhotoIsUsed(side, no))
    {
        if (AlbumPhotoIsFlag3(side, no) == 0)
        {
            album_view_flag3_disp[0] = 1;
            album_view_flag3_disp[1] = 0;
        }
        else
        {
            album_view_flag3_disp[1] = 1;
            album_view_flag3_disp[0] = 0;
        }
    }

    for (s_char i = 0; i < 2; i++)
    {
        B2D_ITEM *item = B2dCtlFindItem(handle, AV_ITEM_FLAG3_OFF + i);

        B2dSprSetAlpha(item, alpha);
        B2dSprSetDisp(item, alpha != 0 ? album_view_flag3_disp[i] : 0);
    }
}

// The protect mark at alpha, shown for a protected photo.
// FF3 0x0012c888.
// callcheck: BpCtlGetHandleByFileNo, GetLanguage, GetSysWrk (AlbumViewHandle)
static void AlbumViewShowProtect(u_char alpha)
{
    B2D_ITEM *item = B2dCtlFindItem(AlbumViewHandle(ALBM_HENSYU_PHF), AV_ITEM_PROTECT);
    s_char    side = AlbumDispGetCurAlbum();
    s_char    no   = AlbumDispGetCurPhoto();

    B2dSprSetAlpha(item, alpha);
    B2dSprSetDisp(item, alpha != 0 ? AlbumPhotoIsProtected(side, no) : 0);
}

// One album page: it fades out without an album (kind < 0) or while
// the page is off (AlbumViewPageIsOn), else in. With follow, the photo items take the alpha of
// the page (the other one with swap) unless it is the page shown; then the
// page's own frame items.
// FF3 0x0012c938.
static void AlbumViewUpdatePage(s_char side, int follow, int swap)
{
    s_char kind = AlbumInfoGetKind(side);
    int    on   = AlbumViewPageIsOn(side);

    if (kind < 0 || on == 0)
        AlbumViewPageFadeOut(side);
    else
        AlbumViewPageFadeIn(side);

    if (follow != 0)
    {
        if (swap != 0)
            side ^= 1;
        if (side != AlbumDispGetCurAlbum())
        {
            AlbumViewShowPhoto(album_view_page_alpha[side]);
            AlbumViewShowPhotoFlag3(album_view_page_alpha[side]);
            AlbumViewShowProtect(album_view_page_alpha[side]);
        }
    }
    AlbumViewFrameSetAlpha(side, album_view_page_alpha[side]);
    AlbumViewArrowSetAlpha(side, album_view_page_alpha[side]);
    AlbumViewPageCsrSetAlpha(side, album_view_page_alpha[side]);
}

// FF3 0x0012ca28.
static void AlbumViewInitItems(void)
{
    AlbumViewSetPageCsrHooks();
    AlbumViewSetThumbHook();
    AlbumViewSetPhotoMarkHook();
    AlbumViewSetProtectMarkHooks();
    AlbumViewSetPhotoHook();
    AlbumViewShowPhoto(0);
    AlbumViewShowPhotoFlag3(0);
    AlbumViewShowProtect(0);
}

// ──────────────────────────────────────────────────────────────────────
// The current photo's info (ALBM_HENSYU text items)
// ──────────────────────────────────────────────────────────────────────

// Item 0x40d: the room the photo was taken in.
// FF3 0x0012ca78.
// callcheck: B2dGetDrawItem, AlbumViewMsgGetAlpha (AlbumViewDrawPri, AlbumViewInfoAlpha)
static int AlbumViewDrawRoom(const B2D_HOOK_ARGS *args)
{
    const int *xy  = (const int *)args->t1;
    int        pri = AlbumViewDrawPri();

    SetCurColorPacked((u_int)args->t3);
    s_char side = AlbumDispGetCurAlbum();
    s_char no   = AlbumDispGetCurPhoto();
    AlbumPhotoPrintRoom(side, no, xy[0], xy[1], AlbumViewInfoAlpha(side), pri);
    SetDefaultGsDrawState();
    return 0;
}

// Item 0x411: the date (a1, the item's w, is the character step).
// FF3 0x0012cb40.
// callcheck: B2dGetDrawItem, AlbumViewMsgGetAlpha (AlbumViewDrawPri, AlbumViewInfoAlpha)
static int AlbumViewDrawDate(const B2D_HOOK_ARGS *args)
{
    const int *xy  = (const int *)args->t1;
    int        pri = AlbumViewDrawPri();

    SetCurColorPacked((u_int)args->t3);
    s_char side = AlbumDispGetCurAlbum();
    s_char no   = AlbumDispGetCurPhoto();
    AlbumPhotoPrintDate(side, no, xy[0], xy[1], (int)args->a1, AlbumViewInfoAlpha(side), pri);
    SetDefaultGsDrawState();
    return 0;
}

// Items 0x412 / 0x413: the score digits ("NUM.." items) or its label.
// FF3 0x0012cc18.
// callcheck: B2dGetDrawItem, AlbumViewMsgGetAlpha (AlbumViewDrawPri, AlbumViewInfoAlpha)
static int AlbumViewDrawScore(const B2D_HOOK_ARGS *args)
{
    const char *name = (const char *)args->a0;
    const int  *xy   = (const int *)args->t1;
    int         pri  = AlbumViewDrawPri();

    SetCurColorPacked((u_int)args->t3);
    s_char side  = AlbumDispGetCurAlbum();
    s_char no    = AlbumDispGetCurPhoto();
    int    alpha = AlbumViewInfoAlpha(side);
    if (strncmp(name, "NUM", 3) == 0)       // sdata 0x36b9b8
        AlbumPhotoPrintScore(side, no, xy[0], xy[1], (u_char)alpha, pri);
    else
        AlbumPhotoPrintScoreLabel(side, no, xy[0], xy[1], alpha, pri);
    SetDefaultGsDrawState();
    return 0;
}

// Items 0x40e / 0x40f: the photo's subjects, the name's last digit
// ('1'..) picking which.
// FF3 0x0012cd28 (no Ghidra function before 2026-10-14: inside 0x12cc18).
// callcheck: B2dGetDrawItem, AlbumViewMsgGetAlpha (AlbumViewDrawPri, AlbumViewInfoAlpha)
static int AlbumViewDrawSubject(const B2D_HOOK_ARGS *args)
{
    const char *name = (const char *)args->a0;
    const int  *xy   = (const int *)args->t1;
    int         pri  = AlbumViewDrawPri();

    SetCurColorPacked((u_int)args->t3);
    s_char side  = AlbumDispGetCurAlbum();
    s_char no    = AlbumDispGetCurPhoto();
    int    alpha = AlbumViewInfoAlpha(side);
    AlbumPhotoPrintSubject(name[strlen(name) - 1] - '1', side, no, xy[0], xy[1], alpha, pri);
    SetDefaultGsDrawState();
    return 0;
}

// Items 0x3e8 / 0x3e9: drawn as usual at the shown page's alpha (the
// sprite's alpha slot replaced; FF3 tail-calls the default).
// FF3 0x0012ce18.
// callcheck: AlbumViewMsgGetAlpha (AlbumViewInfoAlpha)
static int AlbumViewDrawFaded(const B2D_HOOK_ARGS *args)
{
    B2D_HOOK_ARGS draw = *args;

    draw.stack[1] = AlbumViewInfoAlpha(AlbumDispGetCurAlbum());
    return B2dDrawItemDefault(&draw);
}

static const B2D_HOOK_REC album_view_info_hooks[] =       // ROM 0x34e580
{
    { AlbumViewDrawRoom,    0, AV_ITEM_ROOM,      nullptr },
    { AlbumViewDrawDate,    0, AV_ITEM_DATE,      nullptr },
    { AlbumViewDrawScore,   0, AV_ITEM_SCORE_A,   nullptr },
    { AlbumViewDrawScore,   0, AV_ITEM_SCORE_B,   nullptr },
    { AlbumViewDrawSubject, 0, AV_ITEM_SUBJECT_1, nullptr },
    { AlbumViewDrawSubject, 0, AV_ITEM_SUBJECT_2, nullptr },
    { AlbumViewDrawFaded,   0, AV_ITEM_FADED_A,   nullptr },
    { AlbumViewDrawFaded,   0, AV_ITEM_FADED_B,   nullptr },
    { nullptr,             -1, -1,                nullptr },
};

// FF3 copies the ROM list to the stack first.
// FF3 0x0012cf20.
// callcheck: BpCtlGetHandleByFileNo, GetLanguage, GetSysWrk (AlbumViewHandle)
static void AlbumViewSetInfoHooks(void)
{
    B2dSetHookList(AlbumViewHandle(ALBM_HENSYU_PHF), album_view_info_hooks);
}

// FF3 0x0012cfa0.
// callcheck: BpCtlGetHandleByFileNo, GetLanguage, GetSysWrk (AlbumViewHandle)
void AlbumViewShowCopyItem(int show)
{
    B2D_ITEM *item = B2dCtlFindItem(AlbumViewHandle(ALBM_HENSYU_PHF), AV_ITEM_COPY);

    B2dSprSetAlpha(item, show != 0 ? 0x80 : 0);
    B2dSprSetDisp(item, show);
}

// Freezes the parts the edit page drives itself: the photo items of
// ALBM_HENSYU, the thumbnail strip's items, and on ALBM_CMN the pages'
// frames, the menu and sort rows' colour and the arrows' alpha.
// FF3 0x0012d020.
// callcheck: BpCtlGetHandleByFileNo, GetLanguage, GetSysWrk (AlbumViewHandle)
static void AlbumViewFreezeItems(void)
{
    int handle = AlbumViewHandle(ALBM_HENSYU_PHF);

    B2dSprFreezeItem(handle, AV_ITEM_PHOTO, 0);
    B2dSprFreezeItem(handle, AV_ITEM_PHOTO_BASE, 0);
    B2dSprFreezeItem(handle, AV_ITEM_FLAG3_OFF, 0);
    B2dSprFreezeItem(handle, AV_ITEM_FLAG3_ON, 0);
    B2dSprFreezeItem(handle, AV_ITEM_PROTECT, 0);
    for (s_char i = 0; i < AV_PHOTO_SUB_NUM; i++)
        B2dSprFreezeItem(handle, AV_ITEM_PHOTO_SUB + i, 0);

    handle = AlbumViewHandle(ALBM_THUMBNAIL_PHF);
    for (s_char i = 0; i < AV_THUMB_NUM; i++)
    {
        B2dSprFreezeItem(handle, AV_THUMB_ITEM_A + i, 0);
        B2dSprFreezeItem(handle, AV_THUMB_ITEM_B + i, 0);
    }

    handle = AlbumViewHandle(ALBM_CMN_PHF);
    for (s_char i = 0; i < 4; i++)
    {
        B2dSprFreezeItem(handle, album_view_waku[0][i].id, 0);
        B2dSprFreezeItem(handle, album_view_waku[1][i].id, 0);
    }
    for (s_char i = 0; i < 5; i++)
        B2dSprFreezePart(handle, album_view_menu_row[i].id, B2D_PART_RGB, 0);
    for (s_char i = 0; i < 3; i++)
        B2dSprFreezePart(handle, album_view_sort_row[i].id, B2D_PART_RGB, 0);
    for (s_char i = 0; i < 2; i++)
    {
        B2dSprFreezePart(handle, album_view_arw[0][i].id, B2D_PART_ALPHA, 0);
        B2dSprFreezePart(handle, album_view_arw[1][i].id, B2D_PART_ALPHA, 0);
    }
}

// ──────────────────────────────────────────────────────────────────────
// The layouts' life
// ──────────────────────────────────────────────────────────────────────

// FF3 0x0012d2c8.
// callcheck: operator_new, BpObjCtor (new BP_OBJ)
void AlbumViewLayoutCreate(void)
{
    for (s_char i = 0; i < ALBUM_LAYOUT_NUM; i++)
    {
        BP_OBJ *obj = new BP_OBJ(album_layout_file[i] + GetSysWrk()->GetLanguage());
        album_layout_obj[i] = obj;
    }
    AlbumViewPageAlphaReset(0);
    AlbumViewPageAlphaReset(1);
}

// All five loaded: ALBM_CMN's item 0x3f1 becomes the fade layer's item and
// the layer shows label 0.
// FF3 0x0012d388.
int AlbumViewLayoutLoad(void)
{
    int loaded = 1;

    for (s_char i = 0; i < ALBUM_LAYOUT_NUM; i++)
    {
        if (album_layout_obj[i]->Load() == 0)
            loaded = 0;
    }
    if (loaded)
    {
        GetCmnFadeLayer()->SetItem(album_layout_obj[ALBUM_LAYOUT_CMN]->GetHandle(), 0x3f1);
        GetCmnFadeLayer()->Show(0);
    }
    return loaded;
}

// FF3 0x0012d430.
void AlbumViewLayoutIn(int pic_mode)
{
    album_layout_obj[ALBUM_LAYOUT_HENSYU]->In(av_label_in);
    album_layout_obj[ALBUM_LAYOUT_THUMBNAIL]->In(av_label_in);
    album_layout_obj[ALBUM_LAYOUT_CMN]->In(av_label_in);
    AlbumViewFreezeItems();
    AlbumViewCoverInit(pic_mode);
    AlbumViewInitItems();
    AlbumViewSetInfoHooks();
    AlbumViewSelInit();
    AlbumViewSetMsgHooks();
    AlbumViewSetFadeItemHook();
}

// FF3 0x0012d4c8.
int AlbumViewLayoutIsInEnd(void)
{
    int hensyu = album_layout_obj[ALBUM_LAYOUT_HENSYU]->IsInEnd();
    int thumb  = album_layout_obj[ALBUM_LAYOUT_THUMBNAIL]->IsInEnd();
    int cmn    = album_layout_obj[ALBUM_LAYOUT_CMN]->IsInEnd();

    return cmn != 0 && thumb != 0 && hensyu != 0;
}

// FF3 0x0012d520.
void AlbumViewUpdatePages(int follow, int swap)
{
    AlbumViewUpdatePage(0, follow, swap);
    AlbumViewUpdatePage(1, follow, swap);
}

// FF3 0x0012d568.
int AlbumViewLayoutOut(void)
{
    int hensyu = album_layout_obj[ALBUM_LAYOUT_HENSYU]->Out(av_label_out);
    int thumb  = album_layout_obj[ALBUM_LAYOUT_THUMBNAIL]->Out(av_label_out);
    int cmn    = album_layout_obj[ALBUM_LAYOUT_CMN]->Out(av_label_out);

    return cmn != 0 && thumb != 0 && hensyu != 0;
}

// FF3 0x0012d5d8.
// callcheck: operator_delete (operator delete)
void AlbumViewLayoutRelease(void)
{
    GetCmnFadeLayer()->SetItemHandle(nullptr);
    GetCmnFadeLayer()->Term();
    AlbumViewCsrFree();
    AlbumViewCoverRelease();
    for (s_char i = 0; i < ALBUM_LAYOUT_NUM; i++)
    {
        BP_OBJ *obj = album_layout_obj[i];

        if (obj != nullptr)
        {
            obj->Release();
            operator delete(obj);
        }
        album_layout_obj[i] = nullptr;
    }
}

// ──────────────────────────────────────────────────────────────────────
// Page animations
// ──────────────────────────────────────────────────────────────────────

// FF3 0x0012d678.
int AlbumViewPlayMenu(int on)
{
    static const char *const label[2] = { "hensyu_menuoff", "hensyu_menuon" };    // sdata 0x36b9c8
    BP_OBJ *cmn  = album_layout_obj[ALBUM_LAYOUT_CMN];
    int     done = cmn->PlayOnce(label[on]);

    if (done != 0)
        cmn->PlayOnceReset();
    return done != 0;
}

// The page turn: the edit page and the frame play next / back, the
// thumbnail strip its A or B variant.
// FF3 0x0012d6e0.
int AlbumViewPlayPageTurn(int back, s_char thumb_b)
{
    static const char *const hensyu_label[2] = { "hensyu_next", "hensyu_back" };   // sdata 0x36b9d0
    static const char *const thumb_label[4]  =                                      // built on the stack
    {
        "hensyuA_next", "hensyuA_back", "hensyuB_next", "hensyuB_back",
    };
    static const char *const cmn_label[2]    = { "hensyu_next", "hensyu_back" };   // sdata 0x36b9d8

    int hensyu = album_layout_obj[ALBUM_LAYOUT_HENSYU]->PlayOnce(hensyu_label[back]);
    int thumb  = album_layout_obj[ALBUM_LAYOUT_THUMBNAIL]->PlayOnce(thumb_label[thumb_b * 2 + back]);
    int cmn    = album_layout_obj[ALBUM_LAYOUT_CMN]->PlayOnce(cmn_label[back]);
    int done   = cmn != 0 && thumb != 0 && hensyu != 0;

    if (done)
    {
        album_layout_obj[ALBUM_LAYOUT_HENSYU]->PlayOnceReset();
        album_layout_obj[ALBUM_LAYOUT_THUMBNAIL]->PlayOnceReset();
        album_layout_obj[ALBUM_LAYOUT_CMN]->PlayOnceReset();
    }
    return done;
}

// Into (in) or out of the card load: the edit page plays back / next, the
// thumbnail strip in / out, the frame load_in / load_out.
// FF3 0x0012d7f8.
int AlbumViewPlayLoad(int in)
{
    static const char *const hensyu_label[2] = { "hensyu_next", "hensyu_back" };   // sdata 0x36b9e0
    static const char *const thumb_label[2]  = { "hensyu_out", "hensyu_in" };      // sdata 0x36b9e8
    static const char *const cmn_label[2]    = { "load_out", "load_in" };          // sdata 0x36b9f8

    int hensyu = album_layout_obj[ALBUM_LAYOUT_HENSYU]->PlayOnce(hensyu_label[in]);
    int thumb  = album_layout_obj[ALBUM_LAYOUT_THUMBNAIL]->PlayOnce(thumb_label[in]);
    int cmn    = album_layout_obj[ALBUM_LAYOUT_CMN]->PlayOnce(cmn_label[in]);
    int done   = cmn != 0 && thumb != 0 && hensyu != 0;

    if (done)
    {
        album_layout_obj[ALBUM_LAYOUT_HENSYU]->PlayOnceReset();
        album_layout_obj[ALBUM_LAYOUT_THUMBNAIL]->PlayOnceReset();
        album_layout_obj[ALBUM_LAYOUT_CMN]->PlayOnceReset();
    }
    return done;
}

// FF3 0x0012d8d8.
int AlbumViewPlaySort(int on)
{
    static const char *const label[2] = { "hensyu_sortoff", "hensyu_sorton" };    // sdata 0x36ba00
    BP_OBJ *cmn  = album_layout_obj[ALBUM_LAYOUT_CMN];
    int     done = cmn->PlayOnce(label[on]);

    if (done != 0)
        cmn->PlayOnceReset();
    return done != 0;
}

// The fade layer under the album pages, by the game phase the album serves
// (phase ids, see architecture.md "phases"): label 0xd for phase 8, 0 for
// 0x18..0x1c / 0x34 / 0x4e / 0x4f / 0x6a, 4 for 0x35, 0xe for 0x36; any
// other phase hides it.
// FF3 0x0012d940.
void AlbumViewSetFadeLayer(int phase)
{
    int label = -1;

    switch (phase)
    {
    case 0x08:
        label = 0xd;
        break;
    case 0x18: case 0x19: case 0x1a: case 0x1b: case 0x1c:
    case 0x34: case 0x4e: case 0x4f: case 0x6a:
        label = 0;
        break;
    case 0x35:
        label = 4;
        break;
    case 0x36:
        label = 0xe;
        break;
    }
    if (label != -1)
    {
        GetCmnFadeLayer()->Switch(label, AV_FADE_SPEED);
        return;
    }
    GetCmnFadeLayer()->Hide(AV_FADE_SPEED);
}

// ──────────────────────────────────────────────────────────────────────
// PC: the mouse. Where the page grids and the row cursors are.
// ──────────────────────────────────────────────────────────────────────

// Cell (col, row) of a page's 8 x 2 grid. The mark items stay on the first
// cell and their hook draws them moved (AlbumViewDrawPageCsr).
int AlbumViewMouseCell(s_char side, int cell, ReiPan_MenuBox *box)
{
    if (!ReiPan_MenuMouseItemBox(AlbumViewHandle(ALBM_CMN_PHF), 0x417, box))
        return 0;
    box->x += (float)((cell % AV_MARK_COLS) * AV_MARK_DX);
    box->y += (float)((cell / AV_MARK_COLS) * AV_MARK_DY + side * AV_SIDE_DY);
    return 1;
}

// The menu's and the sort list's row cursors as drawn, on the rows they stand on.
int AlbumViewMouseMenuBox(ReiPan_MenuBox *box)
{
    return ReiPan_MenuMouseItemBox(AlbumViewHandle(ALBM_CMN_PHF), AV_ITEM_MENU_CSR, box);
}

int AlbumViewMouseSortBox(ReiPan_MenuBox *box)
{
    return ReiPan_MenuMouseItemBox(AlbumViewHandle(ALBM_CMN_PHF), AV_ITEM_SORT_CSR, box);
}

// The two answers of the album's own question: the words, centred on plates.
int AlbumViewMouseConfBox(int pos, ReiPan_MenuBox *box)
{
    ReiPan_MenuBox word;

    if (!ReiPan_MenuMouseItemBox(AlbumViewHandle(ALBM_CMN_PHF), pos == 0 ? AV_ITEM_MSG_LINE_1 : AV_ITEM_MSG_LINE_2, &word))
        return 0;
    box->x = word.x + word.w * 0.5f - 58.0f;
    box->y = word.y - 1.0f;
    box->w = 116.0f;
    box->h = 26.0f;
    return 1;
}
