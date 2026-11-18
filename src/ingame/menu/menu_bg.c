/* ==========================================================================
 *  ingame/menu/menu_bg.c
 *
 *  The story menu backdrop, the row lists and small layout helpers; see
 *  menu_bg.h. FF3 0x208370..0x2093c8, FF3-only (link order had put the run
 *  in FF2's menu_file.c; the FF2 name MenuFileNoReadFrameDisp at 0x2092f0
 *  was a wrong match).
 *
 *  The backdrop: MenuBgStart loads the layout and the two CLUT files,
 *  MenuBgIsReady (once everything is in) copies the first CLUT file twice as
 *  a double buffer, hooks items 0x3eb / 0x3ec (MenuBgClutHook) and EF_yuki
 *  (the snow) and plays "in". Each frame MenuBgDraw darkens the captured
 *  screen and steps the fade; the item hook lerps the CLUT from the mono
 *  to the blue one by it, once per frame into the other buffer.
 * ======================================================================== */
#include "ingame/menu/menu_bg.h"

#include <string.h>                     // memcpy

#include "common/cmn_fade_layer.h"      // GetCmnFadeLayer, capCtl::Term, CmnFadeLayerIsReady
#include "common/heapctrl.h"            // heapCtrlMalloc, heapCtrlFree
#include "graphics/effect/effect_oth.h" // SubFocus, EffMenuBgStart / End / Draw
#include "graphics/graph2d/message.h"   // GetMsgDataAddr, MsgSetSubtractMode, PrintMsg_ArrangeOneLine, MsgPrintPageMain, PrintNumber_N
#include "graphics/graph2d/tim2.h"      // Tim2GetFirstPicture
#include "graphics/gs_reg.h"            // SetGsRegCached, GetGsRegCached
#include "graphics/motion/anicode.h"    // B2D_HOOK_REC, B2dSetHookList, B2dDrawItemDefault, SetDefaultGsDrawState, SetCurColorPacked
#include "ingame/ingame_wrk.h"          // ingame_wrk.b03
#include "sdk/libgraph.h"               // SCE_GS_TEX1_1
#include "system/b2d/B2dCtl.h"          // B2dCtlFindItem, B2dCtlFindItemByName, B2dCtlSetItemHook
#include "system/b2d/B2dSpr.h"          // B2dSprFreezeItem, B2dSprFreezePart, B2dSprSetAlpha, B2dSprSetDisp, B2dSprGetPartData
#include "system/b2d/BpCtl.h"           // BpCtl*, B2dDrawSpriteDirect, BpPlayLabel*, B2dCtlGetLongestPart, B2dPartIsPlaying, B2dItemIsPlaying
#include "system/b2d/BpTex.h"           // BpTexSetClut
#include "system/eeiop/cd_file_dat.h"   // MNBG1_SITAJI_*_CL2
#include "system/eeiop/cddat.h"         // GetFileSize
#include "system/os/eecdvd.h"           // LoadReq, IsLoadEnd
#include "system/os/sys_heap.h"         // GetSystemHeapWrkP

// bss 0x3f6058
static MENU_BG_WRK menu_bg;

static int  MenuBgClutHook(const B2D_HOOK_ARGS *args);

// The backdrop layout's item hooks (rodata 0x35c5a0; FF3 copies the three
// records to the stack before B2dSetHookList).
static const B2D_HOOK_REC menu_bg_hooks[] =
{
    { MenuBgClutHook, 0, 0x3eb, nullptr },
    { MenuBgClutHook, 0, 0x3ec, nullptr },
    { nullptr,       -1,    -1, nullptr },
};

// ──────────────────────────────────────────────────────────────────────
// FF3 0x00208370 .. 0x002083c0: the work's accessors.

void MenuBgSetHandle(int handle)
{
    menu_bg.handle = handle;
}

int MenuBgGetHandle(void)
{
    return menu_bg.handle;
}

void MenuBgSetDir(int dir)
{
    menu_bg.dir = dir;
}

int MenuBgGetDir(void)
{
    return menu_bg.dir;
}

void MenuBgSetFade(int fade)
{
    menu_bg.fade = fade;
}

int MenuBgGetFade(void)
{
    return menu_bg.fade;
}

// ──────────────────────────────────────────────────────────────────────
// The snow on item EF_yuki, at the item's alpha.
// FF3 0x002083d0

static int MenuBgSnowHook(const B2D_HOOK_ARGS *args)
{
    float rate = (float)(int)args->stack[2] * (1.0f / 128.0f);  // sdata 0x3711d0

    EffMenuBgDraw(rate);
    FUN_0017de28();                     // FF3 passes rate too; the function is empty
    SetDefaultGsDrawState();
    return 0;
}

// Not in the other world (ingame_wrk.b03 1).
// FF3 0x00208420

static void MenuBgSnowStart(void)
{
    B2D_ITEM_HOOK hook;

    if (ingame_wrk.b03 != 1)
    {
        hook.func = MenuBgSnowHook;
        B2dCtlSetItemHook(B2dCtlFindItemByName(menu_bg.handle, "EF_yuki"), &hook);  // sdata 0x3711d8
        EffMenuBgStart(1);
    }
}

// FF3 0x00208480

static void MenuBgSnowEnd(void)
{
    if (ingame_wrk.b03 != 1)
    {
        EffMenuBgEnd(1);
    }
}

// ──────────────────────────────────────────────────────────────────────
// The 256 RGBA entries of dst's CLUT = from + (to - from) x rate, rate
// clamped to 0..1 (each a TIM2 whose first picture carries the CLUT after
// its image).
// FF3 0x002084b0

static u_char *MenuBgClutAddr(u_char *tim2)
{
    TIM2_PICTUREHEADER *pic = Tim2GetFirstPicture(tim2);

    return (u_char *)pic + pic->HeaderSize + pic->ImageSize;
}

static void MenuBgClutLerp(float rate, u_char *dst, u_char *to, u_char *from)
{
    u_char *a;
    u_char *b;
    u_char *d;
    int     i;
    int     c;

    if (1.0f < rate)                    // sdata 0x3711e0
        rate = 1.0f;
    if (rate < 0.0f)
        rate = 0.0f;

    a = MenuBgClutAddr(from);
    b = MenuBgClutAddr(to);
    d = MenuBgClutAddr(dst);
    for (i = 0; i < 0x100; i++)
    {
        for (c = 0; c < 4; c++)
        {
            d[i * 4 + c] = (u_char)(int)((float)(b[i * 4 + c] - a[i * 4 + c]) * rate + (float)a[i * 4 + c]);
        }
    }
}

// Once per frame (MenuBgDraw clears clut_done): the other buffer gets the
// lerp and becomes the shown one.
// FF3 0x00208648

static void MenuBgClutUpdate(float rate)
{
    if (menu_bg.clut_done == 0)
    {
        menu_bg.cur ^= 1;
        MenuBgClutLerp(rate, menu_bg.clut_buf[menu_bg.cur], menu_bg.clut_to, menu_bg.clut_from);
        menu_bg.clut_done = 1;
    }
}

// Items 0x3eb / 0x3ec (sprites): bilinear, the lerped CLUT, then the
// default sprite draw.
// FF3 0x002086a8

static int MenuBgClutHook(const B2D_HOOK_ARGS *args)
{
    SetGsRegCached(0x161, SCE_GS_TEX1_1, 1);    // MMAG / MMIN linear
    MenuBgClutUpdate((float)menu_bg.fade * (1.0f / 128.0f));  // sdata 0x3711e4
    BpTexSetClut(menu_bg.handle, (int)args->a0, menu_bg.clut_buf[menu_bg.cur]);
    B2dDrawItemDefault(args);
    SetDefaultGsDrawState();
    return 0;
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x002087c8

void MenuBgSetDarken(int alpha)
{
    menu_bg.darken = alpha;
}

// file: the backdrop layout (with layout non-zero). The CLUT files load
// into system heap buffers sized by the first one.
// FF3 0x002087d8

void MenuBgStart(int file, int layout)
{
    u_int size;

    menu_bg.dir       = 1;
    menu_bg.handle    = -1;
    menu_bg.darken    = 0;
    menu_bg.fade      = 0;
    menu_bg.layout    = layout;
    menu_bg.clut_done = 0;
    if (layout != 0)
    {
        size = GetFileSize(MNBG1_SITAJI_MONO_CL2);
        menu_bg.clut_from   = (u_char *)heapCtrlMalloc(GetSystemHeapWrkP(), size, 0, 0, 0);
        menu_bg.clut_to     = (u_char *)heapCtrlMalloc(GetSystemHeapWrkP(), size, 0, 0, 0);
        menu_bg.load_from   = LoadReq(MNBG1_SITAJI_MONO_CL2, menu_bg.clut_from);
        menu_bg.load_to     = LoadReq(MNBG1_SITAJI_BLUE_CL2, menu_bg.clut_to);
        menu_bg.clut_buf[0] = (u_char *)heapCtrlMalloc(GetSystemHeapWrkP(), size, 0, 0, 0);
        menu_bg.clut_buf[1] = (u_char *)heapCtrlMalloc(GetSystemHeapWrkP(), size, 0, 0, 0);
        menu_bg.cur         = 0;
        menu_bg.handle      = BpCtlLoadReq(file, nullptr);
    }
}

// 1 once the layout, both CLUT files and the common fade layer are in; then
// the hooks go on and "in" plays. label != -1: an item to hide.
// FF3 0x002088f8
// callcheck: GetCmnFadeLayer (FF3 passes the layer to CmnFadeLayerIsReady)

int MenuBgIsReady(int label)
{
    TIM2_PICTUREHEADER *pic;
    int size;

    if (menu_bg.layout == 0)
        return 1;
    if (BpCtlIsDataReadyAll() == 0)
        return 0;
    if (IsLoadEnd(menu_bg.load_from) == 0)
        return 0;
    if (IsLoadEnd(menu_bg.load_to) == 0)
        return 0;
    if (CmnFadeLayerIsReady() == 0)
        return 0;

    // the file up to its CLUT (headers and image); the lerp writes the CLUT
    pic  = Tim2GetFirstPicture(menu_bg.clut_from);
    size = (int)((u_char *)pic + pic->HeaderSize + pic->ImageSize - menu_bg.clut_from);
    memcpy(menu_bg.clut_buf[0], menu_bg.clut_from, size);
    memcpy(menu_bg.clut_buf[1], menu_bg.clut_from, size);
    B2dSetHookList(menu_bg.handle, menu_bg_hooks);
    MenuBgSnowStart();
    BpPlayLabel(menu_bg.handle, "in");  // sdata 0x3711e8
    if (label != -1)
    {
        B2dSprSetDisp(B2dCtlFindItem(menu_bg.handle, label), 0);
    }
    return 1;
}

// FF3 0x00208a50

static int MenuBgClamp(int v, int lo, int hi)
{
    if (v < lo)
        return lo;
    return v < hi ? v : hi;
}

// The darken rectangle over the whole screen, then the alphas step in dir.
// FF3 0x00208a70

void MenuBgDraw(void)
{
    int dir;

    // Cancel SubFocus's GS half-pixel bias for the host menu background.
    SubFocus(0.1f, 0.1f, 639.9f, 447.9f, 640.0f, 448.0f, 0.5f, 0.5f, menu_bg.darken);  // sdata 0x3711ec..
    dir = menu_bg.dir;
    menu_bg.darken    = MenuBgClamp(menu_bg.darken + dir * 7, 0, MENU_BG_DARKEN_MAX);
    menu_bg.fade      = MenuBgClamp(menu_bg.fade + dir, 0, MENU_BG_FADE_MAX);
    menu_bg.clut_done = 0;
}

// FF3 0x00208b38

int MenuBgIsInEnd(void)
{
    if (menu_bg.layout == 0)
        return 1;
    return B2dPartIsPlaying(B2dCtlGetLongestPart(menu_bg.handle)) == 0;
}

// FF3 0x00208b78

static void MenuBgFreezeItem(int handle, int id)
{
    B2dSprFreezePart(handle, id, B2D_PART_X, 0);
    B2dSprFreezePart(handle, id, B2D_PART_Y, 0);
}

// MenuBgStop for a backdrop that has barely started: under fade 15 the
// four moving items stop where they are and "cancel" plays instead of
// "out". Nothing in FF3 calls it.
// FF3 0x00208bc0

void MenuBgCancel(void)
{
    int         handle;
    const char *label;

    if (menu_bg.layout != 0)
    {
        handle = MenuBgGetHandle();
        if (MenuBgGetFade() < 0xf)
        {
            MenuBgFreezeItem(handle, 0x3fc);
            MenuBgFreezeItem(handle, 0x3fd);
            MenuBgFreezeItem(handle, 0x3eb);
            MenuBgFreezeItem(handle, 0x3ec);
            label = "cancel";           // sdata 0x371200
        }
        else
        {
            label = "out";              // sdata 0x371208
        }
        BpPlayLabelEx(handle, label, 0);
    }
    MenuBgSetDir(-1);
}

// FF3 0x00208c68

void MenuBgStop(void)
{
    if (menu_bg.layout != 0)
    {
        BpPlayLabelEx(MenuBgGetHandle(), "out", 0);  // sdata 0x371208
    }
    MenuBgSetDir(-1);
}

// 0 while the layout still plays; then everything is released.
// FF3 0x00208cb0

int MenuBgRelease(void)
{
    if (menu_bg.layout == 0)
    {
        GetCmnFadeLayer()->Term();
        return 1;
    }
    if (B2dPartIsPlaying(B2dCtlGetLongestPart(menu_bg.handle)) != 0)
    {
        return 0;
    }
    MenuBgSnowEnd();
    BpCtlRelease(menu_bg.handle);
    GetCmnFadeLayer()->Term();
    if (menu_bg.clut_from != nullptr)
    {
        heapCtrlFree(GetSystemHeapWrkP(), menu_bg.clut_from, 0, 0, 0);
        menu_bg.clut_from = nullptr;
    }
    if (menu_bg.clut_to != nullptr)
    {
        heapCtrlFree(GetSystemHeapWrkP(), menu_bg.clut_to, 0, 0, 0);
        menu_bg.clut_to = nullptr;
    }
    if (menu_bg.clut_buf[0] != nullptr)
    {
        heapCtrlFree(GetSystemHeapWrkP(), menu_bg.clut_buf[0], 0, 0, 0);
        menu_bg.clut_buf[0] = nullptr;
    }
    if (menu_bg.clut_buf[1] != nullptr)
    {
        heapCtrlFree(GetSystemHeapWrkP(), menu_bg.clut_buf[1], 0, 0, 0);
        menu_bg.clut_buf[1] = nullptr;
    }
    return 1;
}

// ──────────────────────────────────────────────────────────────────────
// Where a story menu page goes back to: phase 57 StoryMenu_Top.
// FF3 0x00208df8.

short MenuGetTopPhase(void)
{
    return 0x39;
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x00208e00

MENU_LIST *MenuListCreate(short max)
{
    MENU_LIST *list = (MENU_LIST *)heapCtrlMalloc(GetSystemHeapWrkP(), sizeof(MENU_LIST), 0, 0, 0);
    int        i;

    list->ent = (MENU_LIST_ENT *)heapCtrlMalloc(GetSystemHeapWrkP(), max * sizeof(MENU_LIST_ENT), 0, 0, 0);
    list->num = 0;
    list->max = max;
    for (i = 0; i < max; i++)
    {
        list->ent[i].phase = -1;
        list->ent[i].id    = 0;
    }
    return list;
}

// FF3 0x00208ea8

void MenuListDelete(MENU_LIST *list)
{
    if (list != nullptr)
    {
        heapCtrlFree(GetSystemHeapWrkP(), list->ent, 0, 0, 0);
        heapCtrlFree(GetSystemHeapWrkP(), list, 0, 0, 0);
    }
}

// FF3 0x00208f18

int MenuListAdd(MENU_LIST *list, short id, short phase)
{
    MENU_LIST_ENT *ent;

    if (list != nullptr && list->num < list->max)
    {
        ent = &list->ent[list->num];
        ent->phase = phase;
        ent->id    = id;
        list->num++;
        return 1;
    }
    return 0;
}

// FF3 0x00208f70

short MenuListGetNum(MENU_LIST *list)
{
    return list != nullptr ? list->num : 0;
}

// FF3 0x00208f88

short MenuListGetId(MENU_LIST *list, short row)
{
    if (list != nullptr && row >= 0 && row < list->num)
        return list->ent[row].id;
    return -1;
}

// FF3 0x00208fc8

short MenuListGetPhase(MENU_LIST *list, short row)
{
    if (list != nullptr && row >= 0 && row < list->num)
        return list->ent[row].phase;
    return -1;
}

// ──────────────────────────────────────────────────────────────────────
// 1 while the file's layout still plays a label (its longest part runs).
// FF3 0x00209048.

int MenuLayoutIsPlaying(int file)
{
    return B2dPartIsPlaying(B2dCtlGetLongestPart(BpCtlGetHandleByFileNo(file)));
}

// Nothing in FF3 calls it.
// FF3 0x00209070

int MenuLayoutItemIsPlaying(int file, const char *name)
{
    return B2dItemIsPlaying(B2dCtlFindItemByName(BpCtlGetHandleByFileNo(file), name));
}

// ──────────────────────────────────────────────────────────────────────
// Up arrow and its effect, then the down pair. Freeze mode 1 thaws: an arrow
// that can scroll keeps its layout animation.
// FF3 0x002090a8.
void MenuScrollArrowDisp(int handle, int up, int down, int up_fx, int down_fx,
                         int can_up, int can_down)
{
    B2D_ITEM *arrow = B2dCtlFindItem(handle, up);
    B2D_ITEM *fx = B2dCtlFindItem(handle, up_fx);

    if (can_up != 0)
    {
        B2dSprFreezeItem(handle, up, 1);
        B2dSprFreezeItem(handle, up_fx, 1);
        B2dSprSetAlpha(arrow, 0x80);
    }
    else
    {
        B2dSprFreezeItem(handle, up, 0);
        B2dSprFreezeItem(handle, up_fx, 0);
        B2dSprSetAlpha(arrow, 0x40);
    }
    B2dSprSetDisp(fx, can_up != 0);

    arrow = B2dCtlFindItem(handle, down);
    fx = B2dCtlFindItem(handle, down_fx);
    if (can_down != 0)
    {
        B2dSprFreezeItem(handle, down, 1);
        B2dSprFreezeItem(handle, down_fx, 1);
        B2dSprSetAlpha(arrow, 0x80);
    }
    else
    {
        B2dSprFreezeItem(handle, down, 0);
        B2dSprFreezeItem(handle, down_fx, 0);
        B2dSprSetAlpha(arrow, 0x40);
    }
    B2dSprSetDisp(fx, can_down != 0);
}

// ──────────────────────────────────────────────────────────────────────
// An item's alpha times its parent item's, 0..0xff. Nothing in FF3 calls it.
// FF3 0x00209240

int MenuItemGetAlpha(int handle, int id)
{
    B2D_ITEM *item   = B2dCtlFindItem(handle, id);
    int       alpha  = B2dSprGetPartData(item, B2D_PART_ALPHA);
    int       parent = B2dSprGetPartData(item, B2D_PART_PARENT);

    if (parent != -1)
    {
        alpha = alpha * B2dSprGetPartData(B2dCtlFindItem(handle, parent), B2D_PART_ALPHA) / 128;
    }
    if (alpha < 0)
        alpha = 0;
    if (alpha > 0xff)
        alpha = 0xff;
    return alpha;
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x002092f0

void B2dHookPrintMsgLine(int msg0, int msg1, int arrange, const int *rect, u_int rgb,
                         int alpha, u_int flags)
{
    u_char *str = GetMsgDataAddr(msg0, msg1);

    if (str == nullptr)
        return;
    SetCurColorPacked(rgb);
    if ((flags & 0x20) != 0)
    {
        MsgSetSubtractMode(1);
    }
    PrintMsg_ArrangeOneLine(str, rect[0], rect[1], MSG_COL_CURRENT, alpha, 0, 0, 0, arrange);
    MsgSetSubtractMode(0);
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x002093c8

void B2dHookPrintMsg(int msg0, int msg1, int arrange, const int *rect, u_int rgb,
                     int alpha, u_int flags)
{
    SetCurColorPacked(rgb);
    if ((flags & 0x20) != 0)
    {
        MsgSetSubtractMode(1);
    }
    MsgPrintPageMain(msg0, msg1, rect[0], rect[1], MSG_COL_CURRENT, alpha, 0, 0, 0, arrange);
    MsgSetSubtractMode(0);
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x00209470

void B2dHookPrintNumber(int num, int digits, u_char type, const int *rect, u_int rgb,
                        u_char alpha, u_int flags)
{
    SetCurColorPacked(rgb);
    if ((flags & 0x20) != 0)
    {
        MsgSetSubtractMode(1);
    }
    PrintNumber_N(num, digits, rect[0], rect[1], MSG_COL_CURRENT, alpha, 0, type, 0);
    MsgSetSubtractMode(0);
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x00209518

int B2dDrawSpriteDirectLinear(const B2D_HOOK_ARGS *args)
{
    u_long tex1 = *GetGsRegCached(SCE_GS_TEX1_1);
    int    ret;

    SetGsRegCached(0x161, SCE_GS_TEX1_1, 1);
    ret = B2dDrawSpriteDirect(args);
    SetGsRegCached(tex1, SCE_GS_TEX1_1, 1);
    return ret;
}
