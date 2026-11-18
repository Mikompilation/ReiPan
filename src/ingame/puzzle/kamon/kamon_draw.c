/* ==========================================================================
 *  ingame/puzzle/kamon/kamon_draw.c
 *
 *  The kamon puzzle's tables and draw (see kamon_draw.h).
 *  FF3 0x248f80..0x249930, FF3's own code.
 * ======================================================================== */
#include "ingame/puzzle/kamon/kamon_draw.h"

#include "ingame/char_model_sub.h"              // CharClampf
#include "ingame/puzzle/kamon/kamon_pzl.h"      // kamon_board
#include "ingame/puzzle/puzzle.h"               // PuzzleGetLayoutFile
#include "main/gphase.h"                        // GetGPhaseCtrl
#include "system/b2d/B2dCtl.h"                  // B2dCtlFindItem
#include "system/b2d/B2dSpr.h"                  // B2dSprSetDisp / SetPos / SetAlpha / FreezePart
#include "system/b2d/BpCtl.h"                   // BpCtlGetHandleByFileNo
#include "system/os/system.h"                   // GetFrameTimeScale

// ──────────────────────────────────────────────────────────────────────
// Data
// ──────────────────────────────────────────────────────────────────────

// The one board (data 0x361c78 links, 0x361c50 definition): six moves, the
// cursor on slot 0, pieces 1 3 0 4 in slots 0..3 and the hole in slot 4.
static const KAMON_LINK_DEF kamon_board0_link[8] =      // data 0x361c78
{
    { 1, 2 }, { 2, 3 }, { 3, 4 }, { 4, 0 }, { 4, 1 }, { 4, 2 }, { 0, 2 }, { 3, 1 },
};

static const KAMON_BOARD_DEF kamon_board_def[1] =       // data 0x361c50
{
    { 6, 0, { 1, 3, 0, 4, KAMON_HOLE }, 8, kamon_board0_link },
};

// Every link the layout can draw (data 0x361cb8) and its two items (data
// 0x361d08, 6 bytes a row): the line, and the line lit when it joins the
// hole and the cursor.
#define KAMON_LINK_TBL_NUM  10

static const KAMON_LINK_DEF kamon_link_tbl[KAMON_LINK_TBL_NUM] =   // data 0x361cb8
{
    { 0, 1 }, { 1, 2 }, { 2, 3 }, { 3, 4 }, { 4, 0 },
    { 4, 1 }, { 0, 2 }, { 3, 1 }, { 4, 2 }, { 0, 3 },
};

struct KAMON_LINK_ITEM                  /* 0x6 */
{
    /* 0x0 */ short line;
    /* 0x2 */ short lit;
    /* 0x4 */ short _4;                 // 0, unread
};
static_assert(sizeof(KAMON_LINK_ITEM) == 0x6, "KAMON_LINK_ITEM is 6 bytes");

static const KAMON_LINK_ITEM kamon_link_item[KAMON_LINK_TBL_NUM] =  // data 0x361d08
{
    { 1055, 1056, 0 }, { 1037, 1038, 0 }, { 1040, 1046, 0 }, { 1035, 1036, 0 }, { 1019, 1016, 0 },
    { 1033, 1034, 0 }, { 1051, 1052, 0 }, { 1018, 1017, 0 }, { 1032, 1031, 0 }, { 1053, 1054, 0 },
};

// Layout items (data 0x361d48, 6 bytes a row). Rows 0..4 by piece id: the
// crest sprite, its cursor frame, and the group placed at the piece's
// position. Rows 5..9 by slot: only .group is read (the slot's frame).
struct KAMON_ITEM                       /* 0x6 */
{
    /* 0x0 */ short sprite;
    /* 0x2 */ short csr;
    /* 0x4 */ short group;
};
static_assert(sizeof(KAMON_ITEM) == 0x6, "KAMON_ITEM is 6 bytes");
static_assert(offsetof(KAMON_ITEM, group) == 0x4, "KAMON_ITEM.group");

static const KAMON_ITEM kamon_item[KAMON_SLOT_NUM * 2] =    // data 0x361d48
{
    { 1026, 1039, 1066 }, { 1027, 1061, 1078 }, { 1028, 1062, 1079 }, { 1029, 1063, 1084 }, { 1030, 1064, 1085 },
    {    0, 1011, 1060 }, {    0, 1012, 1059 }, {    0, 1013, 1058 }, {    0, 1014, 1057 }, {    0, 1015, 1010 },
};

static const KAMON_POS kamon_slot_pos[KAMON_SLOT_NUM] =    // data 0x330c28
{
    { 367.0f, 62.0f }, { 418.0f, 215.0f }, { 288.0f, 311.0f }, { 156.0f, 215.0f }, { 206.0f, 62.0f },
};

#define KAMON_ITEM_REST     1001        // 1001..1009: the moves-left digits
#define KAMON_ITEM_REST_NUM 9
#define KAMON_ITEM_FREEZE   1060        // KamonDrawPieceFadeIn freezes this item, not the group

static const short kamon_btn_item[4] = { 1080, 1081, 1082, 1083 };     // sdata 0x371f58

#define KAMON_FADE_IN_LEN   8.0f        // frames (sdata 0x371f48 / 0x371f60)
#define KAMON_GLOW_LEN      30.0f       // frames (sdata 0x371f50 / 0x371f64)

static int   kamon_fade_in_on;          // sbss 0x3738a0
static int   kamon_glow_on;             // sbss 0x3738a4
static float kamon_fade_in_t;           // sbss 0x3738a8
static float kamon_glow_t;              // sbss 0x3738ac

// ──────────────────────────────────────────────────────────────────────
// Tables
// ──────────────────────────────────────────────────────────────────────

// FF3 0x00248f80.

const KAMON_BOARD_DEF *KamonGetBoardDef(int idx)
{
    return &kamon_board_def[idx];
}

// FF3 0x00249888.

const KAMON_POS *KamonGetSlotPos(int slot)
{
    return &kamon_slot_pos[slot];
}

// ──────────────────────────────────────────────────────────────────────
// Draw
// ──────────────────────────────────────────────────────────────────────

// The moves left: one digit item of nine, none at 0.
// FF3 0x00248fa0.

static void KamonDrawRest(void)
{
    int rest = kamon_board->rest;
    int handle = BpCtlGetHandleByFileNo(PuzzleGetLayoutFile());

    if (rest == 0)
        return;
    for (int i = 0; i < KAMON_ITEM_REST_NUM; i++)
        B2dSprSetDisp(B2dCtlFindItem(handle, KAMON_ITEM_REST + i), i == rest - 1);
}

// The slot frames, then each piece's group at the piece's position (parts 6
// and 7 frozen so the animation does not move it back), the crest, and the
// cursor frame on the cursor's slot.
// FF3 0x00249028.

static void KamonDrawPieces(void)
{
    int handle = BpCtlGetHandleByFileNo(PuzzleGetLayoutFile());

    for (int i = 0; i < KAMON_SLOT_NUM; i++)
    {
        B2D_ITEM *frame  = B2dCtlFindItem(handle, kamon_item[KAMON_SLOT_NUM + i].group);
        B2D_ITEM *csr    = B2dCtlFindItem(handle, kamon_item[i].csr);
        B2D_ITEM *sprite = B2dCtlFindItem(handle, kamon_item[i].sprite);

        kamon_board->GetSlot(i);        // FF3 calls it and drops the result
        B2dSprSetDisp(csr, 0);
        B2dSprSetDisp(sprite, 0);
        B2dSprSetDisp(frame, 0);
        B2dSprSetDisp(frame, 1);
    }

    for (int i = 0; i < KAMON_SLOT_NUM; i++)
    {
        int id = (*kamon_board->GetSlot(i))->id;
        if (id == KAMON_HOLE)
            continue;

        B2D_ITEM *sprite = B2dCtlFindItem(handle, kamon_item[id].sprite);
        short group_id = kamon_item[id].group;
        B2D_ITEM *group  = B2dCtlFindItem(handle, group_id);
        B2D_ITEM *csr    = B2dCtlFindItem(handle, kamon_item[id].csr);

        if (kamon_board->cursor == i)
            B2dSprSetDisp(csr, 1);
        KAMON_PIECE *p = kamon_board->GetPiece(id);
        B2dSprSetPos(group, (int)p->x, (int)p->y);
        B2dSprFreezePart(handle, group_id, 6, 0);
        B2dSprFreezePart(handle, group_id, 7, 0);
        B2dSprSetDisp(sprite, 1);
    }
}

// Each link the board has is drawn as a line, except the one joining the
// hole and the cursor, which is drawn lit.
// FF3 0x00249230.

static void KamonDrawLinks(void)
{
    int handle = BpCtlGetHandleByFileNo(PuzzleGetLayoutFile());

    for (int i = 0; i < KAMON_LINK_TBL_NUM; i++)
    {
        KAMON_LINK link(kamon_link_tbl[i].a, kamon_link_tbl[i].b);
        B2D_ITEM *line = B2dCtlFindItem(handle, kamon_link_item[i].line);

        int disp = 0;
        if (kamon_board->HasLink(&link) != 0 && kamon_board->IsHoleLink(&link) == 0)
            disp = 1;
        B2dSprSetDisp(line, disp);

        B2D_ITEM *lit = B2dCtlFindItem(handle, kamon_link_item[i].lit);
        B2dSprSetDisp(lit, kamon_board->IsHoleLink(&link) != 0);
    }
}

// Solved: the missing piece (the hole's slot's own crest) fades in at the
// hole over 8 frames.
// FF3 0x00249358.

static void KamonDrawPieceFadeIn(void)
{
    if (kamon_fade_in_on == 0)
        return;

    int handle = BpCtlGetHandleByFileNo(PuzzleGetLayoutFile());
    for (int i = 0; i < KAMON_SLOT_NUM; i++)
    {
        if ((*kamon_board->GetSlot(i))->id != KAMON_HOLE)
            continue;

        B2D_ITEM *sprite = B2dCtlFindItem(handle, kamon_item[i].sprite);
        B2D_ITEM *group  = B2dCtlFindItem(handle, kamon_item[i].group);
        const KAMON_POS *pos = KamonGetSlotPos(i);
        B2dSprSetPos(group, (int)pos->x, (int)pos->y);
        B2dSprFreezePart(handle, KAMON_ITEM_FREEZE, 6, 0);
        B2dSprFreezePart(handle, KAMON_ITEM_FREEZE, 7, 0);
        B2dSprFreezePart(handle, KAMON_ITEM_FREEZE, 10, 0);
        B2dSprSetAlpha(sprite, (int)(kamon_fade_in_t * 128.0f * 0.125f));
        B2dSprSetDisp(sprite, 1);
    }

    float t = kamon_fade_in_t + GetFrameTimeScale();
    if (t < 0.0f)
        kamon_fade_in_t = 0.0f;
    else if (t <= KAMON_FADE_IN_LEN)
        kamon_fade_in_t = t;
    else
        kamon_fade_in_t = KAMON_FADE_IN_LEN;
}

// Solved, after the fade-in: every link line, lit line and cursor frame
// shows, the ones not shown before fading in over 30 frames.
// FF3 0x00249508.

static void KamonDrawClearGlow(void)
{
    if (kamon_glow_on == 0)
        return;

    int handle = BpCtlGetHandleByFileNo(PuzzleGetLayoutFile());
    int alpha = (int)((kamon_glow_t * 128.0f) / KAMON_GLOW_LEN);

    for (int i = 0; i < KAMON_LINK_TBL_NUM; i++)
    {
        KAMON_LINK link(kamon_link_tbl[i].a, kamon_link_tbl[i].b);
        int hole_link = kamon_board->IsHoleLink(&link);

        short id = kamon_link_item[i].line;
        B2D_ITEM *item = B2dCtlFindItem(handle, id);
        B2dSprSetDisp(item, 1);
        B2dSprFreezePart(handle, id, 10, 0);
        if (hole_link != 0)
            B2dSprSetAlpha(item, alpha);

        id = kamon_link_item[i].lit;
        item = B2dCtlFindItem(handle, id);
        B2dSprSetDisp(item, 1);
        B2dSprFreezePart(handle, id, 10, 0);
        if (hole_link == 0)
            B2dSprSetAlpha(item, alpha);
    }

    for (int i = 0; i < KAMON_SLOT_NUM; i++)
    {
        int id = (*kamon_board->GetSlot(i))->id;
        if (id == KAMON_HOLE)
            id = i;

        short csr_id = kamon_item[id].csr;
        B2D_ITEM *csr = B2dCtlFindItem(handle, csr_id);
        short group_id = kamon_item[id].group;
        B2dSprFreezePart(handle, group_id, 6, 0);
        B2dSprFreezePart(handle, group_id, 7, 0);
        B2dSprFreezePart(handle, csr_id, 10, 0);
        B2dSprSetDisp(csr, 1);
        if (i != kamon_board->cursor)
            B2dSprSetAlpha(csr, alpha);
    }

    kamon_glow_t = kamon_glow_t + GetFrameTimeScale();
    kamon_glow_t = CharClampf(kamon_glow_t, 0.0f, KAMON_GLOW_LEN);
}

// The button guide, hidden while a phase-12 message is up.
// FF3 0x00249788.

static void KamonDrawButtons(void)
{
    int handle = BpCtlGetHandleByFileNo(PuzzleGetLayoutFile());

    for (int i = 0; i < 4; i++)
    {
        B2D_ITEM *item = B2dCtlFindItem(handle, kamon_btn_item[i]);
        B2dSprSetDisp(item, GetGPhaseCtrl()->leaf->id != 12);
    }
}

// FF3 0x00249838.

void KamonFadeReset(void)
{
    kamon_fade_in_on = 0;
    kamon_glow_on = 0;
}

// FF3 0x00249848.

void KamonDraw(void)
{
    KamonDrawRest();
    KamonDrawPieces();
    KamonDrawLinks();
    KamonDrawButtons();
    KamonDrawPieceFadeIn();
    KamonDrawClearGlow();
}

// ──────────────────────────────────────────────────────────────────────
// The fades
// ──────────────────────────────────────────────────────────────────────

// FF3 0x002498a0.

void KamonPieceFadeInStart(void)
{
    kamon_fade_in_t = 0.0f;
    kamon_fade_in_on = 1;
}

// FF3 0x002498b0.

int KamonPieceFadeInIsEnd(void)
{
    return kamon_fade_in_t >= KAMON_FADE_IN_LEN;
}

// FF3 0x002498d8.

void KamonClearGlowStart(void)
{
    kamon_glow_t = 0.0f;
    kamon_glow_on = 1;
}

// FF3 0x002498e8.

int KamonClearGlowIsEnd(void)
{
    return kamon_glow_t >= KAMON_GLOW_LEN;
}

// FF3 0x00249910 is compiler output: the atexit thunk of the GPhaseCtrl
// singleton (GPhaseCtrlTerm(0x41e468)); it marks the end of the file.
