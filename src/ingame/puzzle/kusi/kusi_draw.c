/* ==========================================================================
 *  ingame/puzzle/kusi/kusi_draw.c
 *
 *  The kusi puzzle's tables and layout hooks (see kusi_draw.h).
 *  FF3 0x246958..0x247710, FF3's own code.
 * ======================================================================== */
#include "ingame/puzzle/kusi/kusi_draw.h"

#include <string.h>                             // memset

#include "graphics/motion/anicode.h"            // SetDefaultGsDrawState
#include "ingame/menu/menu_csr.h"               // MenuCsrGetPos
#include "ingame/puzzle/kusi/kusi_pzl.h"        // kusi_board, KusiCsrGetCell, KusiCapIsOn
#include "ingame/puzzle/puzzle.h"               // PuzzleGetLayoutFile
#include "main/gphase.h"                        // GetGPhaseCtrl
#include "system/b2d/B2dCtl.h"                  // B2dCtlFindItem / ByName, B2dCtlSetItemHook / 2, B2dGetDrawItem
#include "system/b2d/B2dSpr.h"                  // B2dSprSetDisp / SetAlpha / FreezePartByName
#include "system/b2d/BpCtl.h"                   // BpCtlGetHandleByFileNo, B2dDrawSpriteDirect, B2dDrawQuadDirect
#include "ingame/menu/menu_mouse.h"    // PC: the mouse in menus

// ──────────────────────────────────────────────────────────────────────
// Data
// ──────────────────────────────────────────────────────────────────────

// The three boards (PZL_KUSI_YLW / BL / RD), cells numbered 0..8 row by
// row: moves -1 = no cell, 0 = an empty cell, n = a piece that must make n
// moves; link = the KUSI_DIR_* bits each cell adds (a link is added once,
// from either end). data 0x330b38.
static const KUSI_BOARD_DEF kusi_board_def[3] =
{
    {  6, 0, { 3, -1, 2,  0, 0, 0, -1, 0, 1 }, { 4,  0, 0, 3, 10, 13, 0,  2, 9 }, { 0, 0, 0 } },
    {  8, 0, { 0,  2, -1, 0, 1, 3,  2, 0, 0 }, { 6,  8, 0, 7, 10, 12, 3, 10, 9 }, { 0, 0, 0 } },
    { 15, 0, { 3,  0, 0,  3, 2, 0,  3, 2, 2 }, { 2, 12, 4, 2, 15, 13, 2, 11, 9 }, { 0, 0, 0 } },
};

// The cells' offsets from the items' layout positions. data 0x330be0.
static const KUSI_POS kusi_cell_pos[KUSI_CELL_NUM] =
{
    {   0.0f,   0.0f }, {  98.0f,   0.0f }, { 196.0f,   0.0f },
    {   0.0f,  86.0f }, {  98.0f,  86.0f }, { 196.0f,  86.0f },
    {   0.0f, 172.0f }, {  98.0f, 172.0f }, { 196.0f, 172.0f },
};

#define KUSI_CELL_DX        0x62        // 98: the cursor's own grid step (KusiDrawCursor)
#define KUSI_CELL_DY        0x56        // 86

// Piece items, drawn once per piece at its position.
#define KUSI_ITEM_REST0     0x406       // 0 moves left
#define KUSI_ITEM_REST1     0x3fe       // 1 left
#define KUSI_ITEM_REST2     0x3ff       // 2 left
#define KUSI_ITEM_REST3     0x3fc       // 3 left
#define KUSI_ITEM_PIECE     0x3fd       // always
#define KUSI_ITEM_NOT_CSR   0x3f8       // not under the cursor
// Cursor items (sdata 0x371ee8).
#define KUSI_ITEM_CSR_PIECE 0x400       // on a piece, or the selected piece
#define KUSI_ITEM_CSR_CELL  0x403       // on a cell without a piece (and during a slide)
// Arrows round the piece in phase 92: the forward ones where it can move,
// the step-back ones (drawn v-flipped) where cancel takes it.
#define KUSI_ITEM_ARW_L     0x405       // forward; a rotated quad (hook2)
#define KUSI_ITEM_ARW_R     0x3fb       // forward; a rotated quad (hook2)
#define KUSI_ITEM_ARW_U     0x40b       // forward
#define KUSI_ITEM_ARW_D     0x402       // forward
#define KUSI_ITEM_BACK_L    0x3f0
#define KUSI_ITEM_BACK_R    0x3f9
#define KUSI_ITEM_BACK_U    0x3fa
#define KUSI_ITEM_BACK_D    0x40c

static const short kusi_piece_item[6] =
{
    KUSI_ITEM_REST0, KUSI_ITEM_REST1, KUSI_ITEM_REST2, KUSI_ITEM_REST3, KUSI_ITEM_PIECE, KUSI_ITEM_NOT_CSR,
};
static const short kusi_csr_item[2] = { KUSI_ITEM_CSR_PIECE, KUSI_ITEM_CSR_CELL };       // sdata 0x371ee8
static const short kusi_arrow_item[6] =
{
    KUSI_ITEM_ARW_D, KUSI_ITEM_ARW_U, KUSI_ITEM_BACK_D, KUSI_ITEM_BACK_U, KUSI_ITEM_BACK_L, KUSI_ITEM_BACK_R,
};
static const short kusi_arrow_quad_item[2] = { KUSI_ITEM_ARW_L, KUSI_ITEM_ARW_R };        // sdata 0x371ef0

// The button captions: A after a move (circle retries), B before the
// first move, C while a piece is selected.
#define KUSI_CAP_NUM        3
static const char kusi_cap_a[] = "R cap_A";     // sdata 0x371ed0
static const char kusi_cap_b[] = "R cap_B";     // sdata 0x371ed8
static const char kusi_cap_c[] = "R cap_C";     // sdata 0x371ee0
static short kusi_cap_alpha[KUSI_CAP_NUM];      // sbss 0x373888

#define KUSI_CAP_ALPHA_STEP 0x10
#define KUSI_CAP_ALPHA_MAX  0x80

#define KUSI_RGB_GRAY       0x80808080u // the cursor on a piece while none is selected
#define KUSI_RGB_SELECTED   0x804646fau // the selected piece's cursor (red) until solved
#define KUSI_FLIP_V         0x2000      // sprite flags: mirror v (the step-back arrows)

#define KUSI_PHASE_MSG      12
#define KUSI_PHASE_MOVE     92
#define KUSI_PHASE_SLIDE    93

// ──────────────────────────────────────────────────────────────────────
// Tables
// ──────────────────────────────────────────────────────────────────────

// FF3 0x00246958.

const KUSI_BOARD_DEF *KusiGetBoardDef(int idx)
{
    return &kusi_board_def[idx];
}

// FF3 0x002474d0.

const KUSI_POS *KusiGetCellPos(int cell)
{
    return &kusi_cell_pos[cell];
}

// FF3 0x00246978.
// callcheck: KusiBoardGetPiece

KUSI_PIECE *KusiFindPiece(int cell)
{
    int num = kusi_board->piece_num;

    for (int i = 0; i < num; i++)
    {
        KUSI_PIECE *p = kusi_board->GetPiece(i);
        if (p->cell == cell)
            return p;
    }
    return nullptr;
}

// ──────────────────────────────────────────────────────────────────────
// Arrows
// ──────────────────────────────────────────────────────────────────────

// The cell an arrow item points at from the cursor, -1 for none.
// FF3 0x002469f0.

static int KusiArrowGetCell(int item)
{
    int dir = 0;

    switch (item)
    {
        case KUSI_ITEM_BACK_L:
        case KUSI_ITEM_ARW_L:
            dir = KUSI_DIR_LEFT;
            break;
        case KUSI_ITEM_BACK_R:
        case KUSI_ITEM_ARW_R:
            dir = KUSI_DIR_RIGHT;
            break;
        case KUSI_ITEM_BACK_U:
        case KUSI_ITEM_ARW_U:
            dir = KUSI_DIR_UP;
            break;
        case KUSI_ITEM_ARW_D:
        case KUSI_ITEM_BACK_D:
            dir = KUSI_DIR_DOWN;
            break;
    }
    return KusiCsrGetCell(dir);
}

// FF3 0x00246a50.

static int KusiIsMoveArrow(int item)
{
    return item == KUSI_ITEM_ARW_D || item == KUSI_ITEM_ARW_R || item == KUSI_ITEM_ARW_L || item == KUSI_ITEM_ARW_U;
}

// FF3 0x00246a98.
// callcheck: KusiBoardCanMove

static int KusiArrowCanMove(int item)
{
    return kusi_board->CanMove(KusiArrowGetCell(item)) != 0;
}

// Cancel would step the piece back to the arrow's cell: it is the cell the
// piece came from, free, and the piece has moves left.
// FF3 0x00246ac8.
// callcheck: KusiBoardIsSolved, KusiBoardGetPrevCell, KusiBoardIsFree

static int KusiArrowCanBack(int item)
{
    int cell = KusiArrowGetCell(item);

    if (kusi_board->IsSolved() != 0 || cell == -1)
        return 0;
    if (cell != kusi_board->GetPrevCell())
        return 0;
    if (kusi_board->IsFree(cell) == 0)
        return 0;

    KUSI_PIECE *sel = kusi_board->selected;
    return sel == nullptr || sel->GetRest() > 0;
}

// ──────────────────────────────────────────────────────────────────────
// Layout hooks
// ──────────────────────────────────────────────────────────────────────

// R cap_C (a sprite).
// FF3 0x00246b70.

static int KusiDrawCap(const B2D_HOOK_ARGS *args)
{
    B2dDrawSpriteDirect(args);
    SetDefaultGsDrawState();
    return 0;
}

// The piece items (sprites), once per piece at its position: the moves-left
// mark that fits, the piece, and the frame of pieces not under the cursor.
// FF3 0x00246bc0.

static int KusiDrawPiece(const B2D_HOOK_ARGS *args)
{
    int num  = kusi_board->piece_num;
    int item = B2dGetDrawItem()->id;

    for (int i = 0; i < num; i++)
    {
        KUSI_PIECE *p = kusi_board->GetPiece(i);
        short x = (short)((int)p->x + (int)args->t1);
        short y = (short)((int)p->y + (int)args->t2);
        int done = p->GetDone();

        if (item == KUSI_ITEM_REST0 && p->moves != done)
            continue;
        if (item == KUSI_ITEM_REST1 && done != p->moves - 1)
            continue;
        if (item == KUSI_ITEM_REST2 && done != p->moves - 2)
            continue;
        if (item == KUSI_ITEM_REST3 && done != p->moves - 3)
            continue;
        if (item == KUSI_ITEM_NOT_CSR && p->cell == MenuCsrGetPos(KUSI_CSR))
            continue;

        B2D_HOOK_ARGS draw = *args;
        draw.t1 = x;
        draw.t2 = y;
        B2dDrawSpriteDirect(&draw);
    }
    return 0;
}

// The cursor (sprites). No piece selected: on the cursor's cell -- 0x400 in
// gray over a piece, 0x403 on an empty cell or while a piece slides. A piece
// selected: 0x400 on it, red until the board is solved.
// FF3 0x00246db8.
// callcheck: KusiBoardIsSolved

static int KusiDrawCursor(const B2D_HOOK_ARGS *args)
{
    int item = B2dGetDrawItem()->id;
    short pos = MenuCsrGetPos(KUSI_CSR);
    KUSI_PIECE *p = KusiFindPiece(pos);
    B2D_HOOK_ARGS draw = *args;
    short x, y;

    if (kusi_board->selected == nullptr)
    {
        y = (short)((pos / KUSI_CELL_W) * KUSI_CELL_DY + (int)args->t2);
        x = (short)((pos % KUSI_CELL_W) * KUSI_CELL_DX + (int)args->t1);
        if (p != nullptr && GetGPhaseCtrl()->leaf->id != KUSI_PHASE_SLIDE)
        {
            if (item != KUSI_ITEM_CSR_PIECE)
                return 0;
            draw.stack[2] = (intptr_t)KUSI_RGB_GRAY;
        }
        else if (item != KUSI_ITEM_CSR_CELL)
        {
            return 0;
        }
    }
    else
    {
        if (item != KUSI_ITEM_CSR_PIECE || p == nullptr)
            return 0;
        y = (short)((int)p->y + (int)args->t2);
        x = (short)((int)p->x + (int)args->t1);
        if (kusi_board->IsSolved() == 0)
            draw.stack[2] = (intptr_t)KUSI_RGB_SELECTED;
    }

    draw.t1 = x;
    draw.t2 = y;
    B2dDrawSpriteDirect(&draw);
    return 0;
}

// The arrows (sprites) round the cursor's piece while one is being moved
// (phase 92): forward where it can move, step-back (mirrored) where cancel
// would take it. FF3 0x00247000 (no Ghidra function before this session).

static int KusiDrawArrow(const B2D_HOOK_ARGS *args)
{
    int item = B2dGetDrawItem()->id;
    short pos = MenuCsrGetPos(KUSI_CSR);
    KUSI_PIECE *p;

    if (GetGPhaseCtrl()->leaf->id != KUSI_PHASE_MOVE || (p = KusiFindPiece(pos)) == nullptr)
        return 0;

    B2D_HOOK_ARGS draw = *args;
    draw.t2 = (short)((int)p->y + (int)args->t2);
    draw.t1 = (short)((int)p->x + (int)args->t1);

    if (KusiIsMoveArrow(item) && KusiArrowCanMove(item))
        B2dDrawSpriteDirect(&draw);
    if (!KusiIsMoveArrow(item) && KusiArrowCanBack(item))
    {
        draw.stack[3] = (u_int)args->stack[3] ^ KUSI_FLIP_V;
        B2dDrawSpriteDirect(&draw);
    }
    return 0;
}

// The left / right forward arrows, drawn as rotated quads (hook2: a1 the
// four corners, t1 the flags); the same rules as KusiDrawArrow. The corners
// are moved in the caller's array, as in FF3.
// FF3 0x002471c0.

static int KusiDrawArrowQuad(const B2D_HOOK_ARGS *args)
{
    int item = B2dGetDrawItem()->id;
    short pos = MenuCsrGetPos(KUSI_CSR);
    KUSI_PIECE *p;

    if (GetGPhaseCtrl()->leaf->id != KUSI_PHASE_MOVE || (p = KusiFindPiece(pos)) == nullptr)
        return 0;

    int *xy = (int *)args->a1;
    short dx = (short)(int)p->x;
    short dy = (short)(int)p->y;
    for (int i = 0; i < 4; i++)
    {
        xy[i * 2]     += dx;
        xy[i * 2 + 1] += dy;
    }

    if (KusiIsMoveArrow(item) && KusiArrowCanMove(item))
        B2dDrawQuadDirect(args);
    if (!KusiIsMoveArrow(item) && KusiArrowCanBack(item))
    {
        B2D_HOOK_ARGS draw = *args;
        draw.t1 = (u_int)args->t1 ^ KUSI_FLIP_V;
        B2dDrawQuadDirect(&draw);
    }
    return 0;
}

// ──────────────────────────────────────────────────────────────────────
// Captions
// ──────────────────────────────────────────────────────────────────────

// Once the board is in play, the caption that fits fades in 0x10 a frame
// and the other two fade out.
// FF3 0x00247348 (FF2's SaveLoadFileSelMsgDisp matched it).
// callcheck: KusiBoardGetRest

static void KusiCapAlphaStep(void)
{
    if (KusiCapIsOn() == 0)
        return;

    int on = 2;
    if (kusi_board->selected == nullptr)
        on = kusi_board->GetRest() == kusi_board->rest_max;

    for (int i = 0; i < KUSI_CAP_NUM; i++)
    {
        int a = (i == on) ? kusi_cap_alpha[i] + KUSI_CAP_ALPHA_STEP : kusi_cap_alpha[i] - KUSI_CAP_ALPHA_STEP;
        int v = 0;
        if (a >= 0)
        {
            v = KUSI_CAP_ALPHA_MAX;
            if (a <= KUSI_CAP_ALPHA_MAX)
                v = a;
        }
        kusi_cap_alpha[i] = (short)v;
    }
}

// The captions, part 10 (alpha) frozen against the layout's animation,
// hidden while phase 12 shows a message.
// FF3 0x002473e0.

static void KusiDrawCaps(void)
{
    static const char *const name[KUSI_CAP_NUM] = { kusi_cap_a, kusi_cap_b, kusi_cap_c };
    int handle = BpCtlGetHandleByFileNo(PuzzleGetLayoutFile());

    KusiCapAlphaStep();
    for (int i = 0; i < KUSI_CAP_NUM; i++)
    {
        B2D_ITEM *item = B2dCtlFindItemByName(handle, name[i]);
        B2dSprFreezePartByName(handle, name[i], 10, 0);
        B2dSprSetAlpha(item, kusi_cap_alpha[i]);
        B2dSprSetDisp(item, GetGPhaseCtrl()->leaf->id != KUSI_PHASE_MSG);
    }
}

// ──────────────────────────────────────────────────────────────────────
// Init / draw
// ──────────────────────────────────────────────────────────────────────

// FF3 builds each hook struct on the stack.
static const B2D_ITEM_HOOK kusi_piece_hook      = { KusiDrawPiece };
static const B2D_ITEM_HOOK kusi_csr_hook        = { KusiDrawCursor };
static const B2D_ITEM_HOOK kusi_arrow_hook      = { KusiDrawArrow };
static const B2D_ITEM_HOOK kusi_arrow_quad_hook = { KusiDrawArrowQuad };
static const B2D_ITEM_HOOK kusi_cap_hook        = { KusiDrawCap };

// FF3 0x002474e8.

void KusiDrawInit(void)
{
    int handle = BpCtlGetHandleByFileNo(PuzzleGetLayoutFile());

    for (int i = 0; i < 6; i++)
        B2dCtlSetItemHook(B2dCtlFindItem(handle, kusi_piece_item[i]), &kusi_piece_hook);
    for (int i = 0; i < 2; i++)
        B2dCtlSetItemHook(B2dCtlFindItem(handle, kusi_csr_item[i]), &kusi_csr_hook);
    for (int i = 0; i < 6; i++)
        B2dCtlSetItemHook(B2dCtlFindItem(handle, kusi_arrow_item[i]), &kusi_arrow_hook);
    for (int i = 0; i < 2; i++)
        B2dCtlSetItemHook2(B2dCtlFindItem(handle, kusi_arrow_quad_item[i]), &kusi_arrow_quad_hook);
    B2dCtlSetItemHook(B2dCtlFindItemByName(handle, kusi_cap_c), &kusi_cap_hook);
    memset(kusi_cap_alpha, 0, sizeof(kusi_cap_alpha));
}

// The layout draws itself through the hooks; this keeps the captions.
// FF3 0x002476d8 (a tail call).
// callcheck: KusiDrawCaps

void KusiDraw(void)
{
    KusiDrawCaps();
}

// FF3 0x002476f0 is compiler output: the atexit thunk of the GPhaseCtrl
// singleton (GPhaseCtrlTerm(0x41e468)), whose C++ static gphase.c owns here;
// it marks the end of the file.

// PC: the mouse. The cursor items stay on cell 0 and their hook draws them
// moved (KusiDrawCursor), so what they report is the first cell.
static void KusiDrawMouseGuide(void)
{
    static const int guide[4] = { -1, REIPAN_MENU_CROSS, REIPAN_MENU_TRIANGLE, REIPAN_MENU_CIRCLE };
    const int handle = BpCtlGetHandleByFileNo(PuzzleGetLayoutFile());

    MenuMouseGuide(handle, kusi_cap_a, guide, 4);
    MenuMouseGuide(handle, kusi_cap_b, guide, 4);
    MenuMouseGuide(handle, kusi_cap_c, guide, 4);
}

void KusiDrawMouseSelect(int csr)
{
    ReiPan_MenuBox box;

    KusiDrawMouseGuide();
    if (ReiPan_MenuMouseItemBox(BpCtlGetHandleByFileNo(PuzzleGetLayoutFile()), KUSI_ITEM_CSR_PIECE, &box))
        MenuMouseCsrOrigin(MenuCsrGetWork(csr), &box, (float)KUSI_CELL_DX, (float)KUSI_CELL_DY);
}

void KusiDrawMouseMove(void)
{
    static const struct { int dx; int dy; int button; } dirs[4] =
    {
        { 1, 0, REIPAN_MENU_RIGHT }, { -1, 0, REIPAN_MENU_LEFT }, { 0, -1, REIPAN_MENU_UP }, { 0, 1, REIPAN_MENU_DOWN },
    };
    const KUSI_PIECE *p = kusi_board->selected;
    ReiPan_MenuBox    box;

    ReiPan_MenuMouseBegin(0, nullptr);
    KusiDrawMouseGuide();
    if (p == nullptr
        || !ReiPan_MenuMouseItemBox(BpCtlGetHandleByFileNo(PuzzleGetLayoutFile()), KUSI_ITEM_CSR_PIECE, &box))
        return;

    box.x += p->x;
    box.y += p->y;
    for (const auto &dir : dirs)
    {
        ReiPan_MenuMouseButton(box.x + (float)(dir.dx * KUSI_CELL_DX), box.y + (float)(dir.dy * KUSI_CELL_DY),
                               box.w, box.h, dir.button);
    }
    ReiPan_MenuMouseButton(box.x, box.y, box.w, box.h, REIPAN_MENU_CROSS);
}
