/* ==========================================================================
 *  ingame/puzzle/kaza/kaza_draw.c
 *
 *  The kaza puzzle's board definitions and draw (see kaza_draw.h).
 *  FF3 0x24acc8..0x24af48, FF3's own code.
 * ======================================================================== */
#include "ingame/puzzle/kaza/kaza_draw.h"

#include "ingame/puzzle/kaza/kaza_pzl.h"        // kaza_board
#include "ingame/puzzle/puzzle.h"               // PuzzleGetLayoutFile
#include "main/gphase.h"                        // GetGPhaseCtrl
#include "system/b2d/B2dCtl.h"                  // B2dCtlFindItem
#include "system/b2d/B2dSpr.h"                  // B2dSprSetDisp / SetRot / FreezePart
#include "system/b2d/BpCtl.h"                   // BpCtlGetHandleByFileNo
#include "ingame/menu/menu_mouse.h"    // PC: the mouse in menus

// ──────────────────────────────────────────────────────────────────────
// Data
// ──────────────────────────────────────────────────────────────────────

// The two boards (PuzzleGetIdx 0 and 1; layouts PZL_KAZA1 / PZL_KAZA2): the
// moves, then each ring's mark on sides 0..3 before any turn.
static const KAZA_BOARD_DEF kaza_board_def[2] =         // data 0x361da0
{
    { 4, { { 2, 0, 3, 0 }, { 1, 2, 1, 0 }, { 2, 0, 1, 3 }, { 1, 3, 1, 1 }, { 2, 1, 3, 0 } } },
    { 5, { { 0, 3, 0, 2 }, { 3, 1, 0, 0 }, { 0, 1, 3, 2 }, { 2, 0, 1, 0 }, { 2, 1, 0, 0 } } },
};

// Layout items by ring: the cursor frame (ring 4's is never shown) and the
// ring sprite turned by B2dSprSetRot.
static const short kaza_csr_item[KAZA_RING_NUM]  = { 1012, 1029, 1023, 1026, 1026 };  // data 0x361e48
static const short kaza_ring_item[KAZA_RING_NUM] = { 1020, 1017, 1019, 1018, 1016 };  // data 0x361e58

#define KAZA_ITEM_REST      0x3ea       // moves left: digits 1..9 as items 0x3ea..0x3f2
#define KAZA_ITEM_REST_NUM  9
#define KAZA_ITEM_BUTTONS   0x3f5       // the button guide
#define KAZA_PHASE_MSG      12

// FF3 0x0024acc8.

const KAZA_BOARD_DEF *KazaGetBoardDef(int idx)
{
    return &kaza_board_def[idx];
}

// ──────────────────────────────────────────────────────────────────────
// The layout
// ──────────────────────────────────────────────────────────────────────

// The moves left: one digit item of nine; at 0 the last one stays as it was.
// FF3 0x0024ace8.

static void KazaDrawRest(void)
{
    int rest = kaza_board->rest;
    int handle = BpCtlGetHandleByFileNo(PuzzleGetLayoutFile());

    if (rest == 0)
        return;
    for (int i = 0; i < KAZA_ITEM_REST_NUM; i++)
        B2dSprSetDisp(B2dCtlFindItem(handle, KAZA_ITEM_REST + i), i == rest - 1);
}

// Each ring at its angle (part 16, the rotation, frozen so the layout's
// animation leaves it), and the cursor frame on the chosen outer ring.
// FF3 0x0024ad70.

static void KazaDrawRings(void)
{
    int handle = BpCtlGetHandleByFileNo(PuzzleGetLayoutFile());

    for (int i = 0; i < KAZA_RING_NUM; i++)
    {
        B2D_ITEM *csr  = B2dCtlFindItem(handle, kaza_csr_item[i]);
        B2D_ITEM *ring = B2dCtlFindItem(handle, kaza_ring_item[i]);

        B2dSprFreezePart(handle, kaza_ring_item[i], 16, 0);
        B2dSprSetRot(ring, (int)kaza_board->ring[i].angle);
        if (i != KAZA_RING_CENTRE)
            B2dSprSetDisp(csr, i == kaza_board->cursor);
    }
}

// The button guide, hidden while a phase-12 message is up.
// FF3 0x0024ae98.

static void KazaDrawButtons(void)
{
    int handle = BpCtlGetHandleByFileNo(PuzzleGetLayoutFile());
    B2D_ITEM *item = B2dCtlFindItem(handle, KAZA_ITEM_BUTTONS);

    B2dSprSetDisp(item, GetGPhaseCtrl()->leaf->id != KAZA_PHASE_MSG);
}

// FF3 0x0024af00.

void KazaDraw(void)
{
    KazaDrawRest();
    KazaDrawRings();
    KazaDrawButtons();
}

// PC: the mouse. The centre turns with whichever ring is chosen, so only the
// four outer rings are targets.
void KazaDrawMouse(int csr)
{
    static const int guide[3] = { -1, REIPAN_MENU_CROSS, REIPAN_MENU_TRIANGLE };
    const int handle = BpCtlGetHandleByFileNo(PuzzleGetLayoutFile());

    MenuMouseGuide(handle, "R caption", guide, 3);
    for (int ring = 0; ring < KAZA_RING_NUM - 1; ring++)
        MenuMouseCsrId(csr, ring, handle, kaza_ring_item[ring]);
}
