/* ==========================================================================
 *  ingame/puzzle/rousoku/rousoku_draw.c
 *
 *  The rousoku puzzle's board definitions and layout hooks (see
 *  rousoku_draw.h). FF3 0x244e38..0x245580 + 0x2be300..0x2be570, FF3's
 *  own code.
 * ======================================================================== */
#include "ingame/puzzle/rousoku/rousoku_draw.h"

#include <math.h>                               // sqrtf

#include "common/vu0math.h"                     // g3dAtan2f, NormalizeAngle
#include "graphics/motion/anicode.h"            // B2D_HOOK_REC, B2dSetHookList, SetDefaultGsDrawState
#include "ingame/puzzle/puzzle.h"               // PuzzleGetLayoutFile
#include "ingame/puzzle/rousoku/rousoku_pzl.h"  // rousoku_board
#include "main/gphase.h"                        // GetGPhaseCtrl
#include "system/b2d/B2dCtl.h"                  // B2dGetDrawItem, B2dCtlFindItem / ByName
#include "system/b2d/B2dSpr.h"                  // B2dSprSetDisp / SetRgb / GetPartData / FreezePartByName
#include "system/b2d/BpCtl.h"                   // BpCtlGetHandleByFileNo, BpGetDrawHandle, B2dDrawSprite*

// ──────────────────────────────────────────────────────────────────────
// Data
// ──────────────────────────────────────────────────────────────────────

// The four boards (PuzzleGetIdx 0..3), cells 0..17 (ROUSOKU_CELL_*).
static const ROUSOKU_BOARD_DEF rousoku_board_def[ROUSOKU_BOARD_DEF_NUM] =     // data 0x361720
{
    { { 0x10, 0x10, 0x10, 0x10, 0x10, 0x11, 0x10, 0x11, 0x11, 0x00, 0x00, 0x00, 0x00, 0x10, 0x00, 0x00, 0x10, 0x10 } },
    { { 0x10, 0x10, 0x11, 0x11, 0x10, 0x11, 0x10, 0x10, 0x10, 0x01, 0x10, 0x10, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00 } },
    { { 0x10, 0x10, 0x11, 0x11, 0x10, 0x11, 0x10, 0x10, 0x10, 0x01, 0x11, 0x00, 0x00, 0x01, 0x01, 0x00, 0x10, 0x01 } },
    { { 0x10, 0x10, 0x00, 0x10, 0x10, 0x10, 0x11, 0x10, 0x01, 0x00, 0x00, 0x10, 0x00, 0x10, 0x00, 0x10, 0x10, 0x10 } },
};

// The layout's candle items by cell (data 0x3619c0) and their plates by
// cell (data 0x330af0).
static const int rousoku_candle_item[ROUSOKU_CELL_NUM] =
{
    1046, 1045, 1032, 1038, 1037, 1036, 1030, 1044, 1031, 1049, 1047, 1033, 1039, 1040, 1041, 1035, 1048, 1034,
};
static const char *const rousoku_plate_name[ROUSOKU_CELL_NUM] =
{
    "R_sara_00", "R_sara_05", "R_sara_07", "R_sara_08", "R_sara_04", "R_sara_03", "R_sara_02", "R_sara_01", "R_sara_06",
    "R_sara_09", "R_sara_10", "R_sara_11", "R_sara_12", "R_sara_13", "R_sara_17", "R_sara_16", "R_sara_14", "R_sara_15",
};

static const char rousoku_flame_fixed[]   = "R_rousokuhi_01";   // data 0x361960
static const char rousoku_flame_movable[] = "R_rousokuhi_00";   // data 0x361970
static const char rousoku_shadow_name[]   = "R_kuikage00";      // data 0x361980
static const char rousoku_light_name[]    = "R_light";          // sdata 0x371e78

#define ROUSOKU_ITEM_BUTTONS    0x3f7   // the button guide
#define ROUSOKU_PHASE_MSG       12

#define ROUSOKU_ALPHA_SCALE     (1.0f / 128.0f)     // sdata 0x371e60 / 0x371e6c
#define ROUSOKU_FLAME_PX        34.0f               // sdata 0x371e64: the flame's pivot
#define ROUSOKU_FLAME_PY        31.0f               // sdata 0x371e68
#define ROUSOKU_SWING_FRAMES    20.0f               // sdata 0x371e70

// The cursor glow: offset from the candle item, colour (alpha byte 0x80).
#define ROUSOKU_LIGHT_DX        0x35
#define ROUSOKU_LIGHT_DY        0x3e
#define ROUSOKU_LIGHT_MOVABLE   0x802a82f9u         // f9 82 2a, orange
#define ROUSOKU_LIGHT_FIXED     0x805341c0u         // c0 41 53, red

// The shadow: the vector is padded half a unit away from 0, its length
// scaled down to fit -5..5 x -3..3, drawn turned about its foot.
#define ROUSOKU_SHADOW_PAD      0.5f                // sdata 0x371e80
#define ROUSOKU_SHADOW_X_MIN    (-5.0f)             // sdata 0x371e84
#define ROUSOKU_SHADOW_X_MAX    5.0f                // sdata 0x371e8c
#define ROUSOKU_SHADOW_Y_MIN    (-3.0f)             // sdata 0x371e90
#define ROUSOKU_SHADOW_Y_MAX    3.0f                // sdata 0x371e94: also the unit length
#define ROUSOKU_SHADOW_PX       15.0f               // sdata 0x371ea0
#define ROUSOKU_SHADOW_PY       85.0f               // sdata 0x371ea4
#define ROUSOKU_SHADOW_DY       9

// FF3 0x00244e38.

const ROUSOKU_BOARD_DEF *RousokuGetBoardDef(int idx)
{
    return &rousoku_board_def[idx];
}

// ──────────────────────────────────────────────────────────────────────
// Hooks
// ──────────────────────────────────────────────────────────────────────

// The candle items (kind 2: a1 xy[8] in t1, t3 rgb, stack[2] alpha): the
// cell's flame at its size, the fixed candles' flame sprite for fixed
// cells.
// FF3 0x00244e58.

static int RousokuDrawFlame(const B2D_HOOK_ARGS *args)
{
    const B2D_ITEM *item = B2dGetDrawItem();
    const int      *xy   = (const int *)args->t1;
    u_int           rgb  = (u_int)args->t3;
    int             i;

    for (i = 0; i < ROUSOKU_CELL_NUM; i++)
    {
        if (rousoku_candle_item[i] == item->id)
            break;
    }
    // PC: FF3 goes on with cell -1 (a word before the board); the hook is
    // only set on the 18 candle items, so it never happens.
    if (i < ROUSOKU_CELL_NUM)
    {
        u_int                cell  = rousoku_board->cell[i];
        const ROUSOKU_FLAME *flame = rousoku_board->flame[i];

        if (flame->w != 0.0f && flame->h != 0.0f)
        {
            B2dDrawSpriteByName(BpGetDrawHandle(),
                                (cell & ROUSOKU_CELL_FIXED) ? rousoku_flame_fixed : rousoku_flame_movable,
                                xy[0], xy[1], rgb & 0xff, rgb >> 8 & 0xff, rgb >> 16 & 0xff,
                                (float)(int)args->stack[2] * ROUSOKU_ALPHA_SCALE, 0.0f,
                                flame->w, flame->h, ROUSOKU_FLAME_PX, ROUSOKU_FLAME_PY);
        }
    }
    SetDefaultGsDrawState();
    return 0;
}

// R_light (a sprite): the glow at the cursor's candle, orange on a movable
// candle, red on a fixed place.
// FF3 0x00244fa8.

static int RousokuDrawLight(const B2D_HOOK_ARGS *args)
{
    int csr = rousoku_board->cursor;

    if ((u_short)csr < ROUSOKU_CELL_NUM)
    {
        B2D_ITEM     *item = B2dCtlFindItem(BpGetDrawHandle(), rousoku_candle_item[csr]);
        B2D_HOOK_ARGS draw = *args;

        draw.t1 = B2dSprGetPartData(item, B2D_PART_X) - ROUSOKU_LIGHT_DX;
        draw.t2 = B2dSprGetPartData(item, B2D_PART_Y) - ROUSOKU_LIGHT_DY;
        draw.stack[2] = (rousoku_board->cell[csr] & ROUSOKU_CELL_FIXED) ? ROUSOKU_LIGHT_FIXED : ROUSOKU_LIGHT_MOVABLE;
        B2dDrawSpriteDirect(&draw);
    }
    SetDefaultGsDrawState();
    return 0;
}

// The shadow for vector (x, y), its foot at (px, py - 9).
// FF3 0x002be300.

static void RousokuDrawShadowAt(float x, float y, float alpha, int handle, int px, int py, u_int rgb)
{
    float sign;
    float scale;
    float rot;

    sign = 0.0f;
    if (x > 0.0f)
        sign = x / x;
    else if (x < 0.0f)
        sign = -(x / x);
    x += sign * ROUSOKU_SHADOW_PAD;

    sign = 0.0f;
    if (y > 0.0f)
        sign = y / y;
    else if (y < 0.0f)
        sign = -(y / y);
    y += sign * ROUSOKU_SHADOW_PAD;

    scale = 1.0f;
    if (x < ROUSOKU_SHADOW_X_MIN)
        scale = ROUSOKU_SHADOW_X_MIN / x;
    if (x > ROUSOKU_SHADOW_X_MAX)
        scale *= ROUSOKU_SHADOW_X_MAX / x;
    if (y < ROUSOKU_SHADOW_Y_MIN)
        scale *= ROUSOKU_SHADOW_Y_MIN / y;
    if (y > ROUSOKU_SHADOW_Y_MAX)
        scale *= ROUSOKU_SHADOW_Y_MAX / y;

    rot = NormalizeAngle(g3dAtan2f(x, y)) * 180.0f / 3.1415927f;   // sdata 0x371e98 / 0x371e9c
    B2dDrawSpriteByName(handle, rousoku_shadow_name, px, py - ROUSOKU_SHADOW_DY,
                        rgb & 0xff, rgb >> 8 & 0xff, rgb >> 16 & 0xff,
                        alpha, rot, 1.0f, scale * sqrtf(y * y + x * x) / ROUSOKU_SHADOW_Y_MAX,
                        ROUSOKU_SHADOW_PX, ROUSOKU_SHADOW_PY);
}

// R_kuikage00 (a sprite: t1 / t2 x / y, stack[1] alpha, stack[2] rgb): the
// shadow of the lit candles; while it swings after a move the shadow before
// the move fades out as the new one fades in.
// FF3 0x00245160.

static int RousokuDrawShadow(const B2D_HOOK_ARGS *args)
{
    if (rousoku_board->show_shadow != 0)
    {
        const ROUSOKU_SHADOW *s = rousoku_board->CalcShadow();
        short sy = s->y;
        short sx = s->x;
        int   handle = BpGetDrawHandle();
        float alpha = (float)(int)args->stack[1] * ROUSOKU_ALPHA_SCALE;

        if (rousoku_board->swing > 0)
        {
            float t = (float)rousoku_board->swing / ROUSOKU_SWING_FRAMES;

            RousokuDrawShadowAt((float)rousoku_board->shadow_prev.x, (float)rousoku_board->shadow_prev.y,
                                alpha * t, handle, (int)args->t1, (int)args->t2, (u_int)args->stack[2]);
            alpha *= 1.0f - t;
        }
        RousokuDrawShadowAt((float)sx, (float)sy, alpha, handle, (int)args->t1, (int)args->t2,
                            (u_int)args->stack[2]);
        SetDefaultGsDrawState();
    }
    return 0;
}

// ROM list 0x361990.
static const B2D_HOOK_REC rousoku_hooks[] =
{
    { RousokuDrawLight,  0, 0,  rousoku_light_name },
    { RousokuDrawShadow, 0, 0,  rousoku_shadow_name },
    { nullptr,          -1, -1, nullptr },
};

// ──────────────────────────────────────────────────────────────────────
// The layout
// ──────────────────────────────────────────────────────────────────────

// FF3 0x002452d8.

void RousokuDrawButtons(int show)
{
    int handle = BpCtlGetHandleByFileNo(PuzzleGetLayoutFile());
    B2D_ITEM *item = B2dCtlFindItem(handle, ROUSOKU_ITEM_BUTTONS);
    int disp = 0;

    if (show != 0)
        disp = GetGPhaseCtrl()->leaf->id != ROUSOKU_PHASE_MSG;
    B2dSprSetDisp(item, disp);
}

// The hooks, and the fixed places' plates: a fixed candle's plate tinted
// (its colour part frozen), an empty place's hidden.
// FF3 0x00245360.

void RousokuDrawInit(void)
{
    int handle = BpCtlGetHandleByFileNo(PuzzleGetLayoutFile());

    for (int i = 0; i < ROUSOKU_CELL_NUM; i++)
    {
        const B2D_HOOK_REC recs[] =     // built on the stack by FF3
        {
            { RousokuDrawFlame, 0, rousoku_candle_item[i], nullptr },
            { nullptr,         -1, -1,                     nullptr },
        };
        B2dSetHookList(handle, recs);
    }
    B2dSetHookList(handle, rousoku_hooks);     // FF3 copies the list to the stack first

    for (int i = 0; i < ROUSOKU_CELL_NUM; i++)
    {
        u_int       cell = rousoku_board->cell[i];
        const char *name = rousoku_plate_name[i];
        B2D_ITEM   *item;

        if ((cell & ROUSOKU_CELL_FIXED) == 0 || name == nullptr)
            continue;
        item = B2dCtlFindItemByName(handle, name);
        if (item == nullptr)
            continue;
        if ((cell & ROUSOKU_CELL_LIT) == 0)
        {
            B2dSprSetDisp(item, 0);
        }
        else
        {
            B2dSprSetRgb(item, 0xb4, 0x80, 0x80);
            B2dSprFreezePartByName(handle, name, B2D_PART_RGB, 0);
        }
    }
    RousokuDrawButtons(rousoku_board->show_buttons);
}

// FF3 0x00245518.

void RousokuDraw(void)
{
    rousoku_board->Step();
    RousokuDrawButtons(rousoku_board->show_buttons);
}

// FF3 0x00245548 is compiler output: the atexit thunk of the GPhaseCtrl
// singleton (GPhaseCtrlTerm(0x41e468)), whose C++ static gphase.c owns here;
// it marks the end of the file.

// PC: the mouse.
int RousokuDrawMouseBox(int cell, ReiPan_MenuBox *box)
{
    const int handle = BpCtlGetHandleByFileNo(PuzzleGetLayoutFile());

    if (!ReiPan_MenuMouseItemBox(handle, rousoku_candle_item[cell], box))
        return 0;
    box->h += 24.0f;                    // the flame's item ends above its plate
    return 1;
}
