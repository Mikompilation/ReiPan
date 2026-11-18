/* ==========================================================================
 *  ingame/puzzle/rousoku/rousoku_board.c
 *
 *  The rousoku puzzle's board and flames (see rousoku_board.h).
 *  FF3 0x245580..0x245d38, FF3's own code.
 * ======================================================================== */
#include "ingame/puzzle/rousoku/rousoku_board.h"

#include <string.h>                             // memset

#include "common/vu0math.h"                     // GetRandValF, GetRandValIByF
#include "graphics/effect/effect_obj.h"         // CandleFlameScaleWGet / HGet

// ──────────────────────────────────────────────────────────────────────
// Data
// ──────────────────────────────────────────────────────────────────────

// The shadow each cell's candle casts (x right, y up): the left ring's
// candles push it right, the right ring's left, more strongly the nearer
// they stand to the stake; the rings' centres are (3, 0) / (-3, 0).
// data 0x361a30.
static const ROUSOKU_SHADOW rousoku_cell_pos[ROUSOKU_CELL_NUM] =
{
    {  3, -2 }, {  4, -1 }, {  5,  0 }, {  4,  1 }, {  3,  2 }, {  2,  1 }, {  1,  0 }, {  2, -1 }, {  3,  0 },
    { -3, -2 }, { -4, -1 }, { -5,  0 }, { -4,  1 }, { -3,  2 }, { -2,  1 }, { -1,  0 }, { -2, -1 }, { -3,  0 },
};

// Flame sizes (w, h) by frame.
struct ROUSOKU_FLAME_SIZE               /* 0x8 */
{
    float w, h;
};
static_assert(sizeof(ROUSOKU_FLAME_SIZE) == 0x8, "ROUSOKU_FLAME_SIZE is 8 bytes");

static const ROUSOKU_FLAME_SIZE rousoku_flame_lighting[10] =     // data 0x361a78
{
    { 0.30f, 0.27f }, { 0.60f, 0.55f }, { 0.90f, 0.82f }, { 1.20f, 1.10f }, { 1.05f, 1.15f },
    { 0.90f, 1.20f }, { 0.95f, 1.15f }, { 1.00f, 1.10f }, { 1.03f, 0.97f }, { 1.05f, 0.95f },
};

static const ROUSOKU_FLAME_SIZE rousoku_flame_going_out[2] =     // data 0x361ac8
{
    { 1.40f, 0.60f }, { 0.40f, 0.20f },
};

static const ROUSOKU_FLAME_SIZE rousoku_flame_flare[20] =        // data 0x361ad8
{
    { 1.10f, 0.90f }, { 1.20f, 0.80f }, { 1.35f, 0.70f }, { 1.30f, 0.80f }, { 1.20f, 0.90f },
    { 1.00f, 1.00f }, { 0.90f, 1.15f }, { 0.88f, 1.25f }, { 0.85f, 1.30f }, { 0.82f, 1.45f },
    { 0.80f, 1.50f }, { 0.78f, 1.53f }, { 0.75f, 1.55f }, { 0.74f, 1.53f }, { 0.75f, 1.50f },
    { 0.80f, 1.40f }, { 0.90f, 1.20f }, { 1.00f, 1.00f }, { 1.10f, 0.90f }, { 1.05f, 0.95f },
};

#define ROUSOKU_LIGHTING_FRAMES     10
#define ROUSOKU_GOING_OUT_FRAMES    2
#define ROUSOKU_FLARE_FRAMES        20
#define ROUSOKU_BURN_FRAMES         150     // CandleFlameScaleData's rows
#define ROUSOKU_FLARE_RAND          0.95f   // sdata 0x371ea8
#define ROUSOKU_FLARE_ADD           0.1f    // sdata 0x371eac

// ──────────────────────────────────────────────────────────────────────
// ROUSOKU_FLAME
// ──────────────────────────────────────────────────────────────────────

// FF3 0x00245580.

ROUSOKU_FLAME::ROUSOKU_FLAME()
{
    state = ROUSOKU_FLAME_OFF;
    w = 0.0f;
    timer = 0;
    h = 0.0f;
}

// A new state starts at frame 0; a candle that starts burning picks a
// random flicker frame (one already burning keeps its frame).
// FF3 0x00245598.

void ROUSOKU_FLAME::SetState(int state)
{
    if (this->state != state)
        timer = 0;

    if (this->state == ROUSOKU_FLAME_BURN)
    {
        this->state = state;
    }
    else if (state == ROUSOKU_FLAME_BURN)
    {
        timer = GetRandValIByF(ROUSOKU_BURN_FRAMES);
        this->state = ROUSOKU_FLAME_BURN;
    }
    else
    {
        this->state = state;
    }
    UpdateSize();
}

// FF3 0x002455f8.

void ROUSOKU_FLAME::UpdateSize()
{
    switch (state)
    {
    case ROUSOKU_FLAME_OFF:
        w = 0.0f;
        h = 0.0f;
        break;
    case ROUSOKU_FLAME_LIGHTING:
        w = rousoku_flame_lighting[timer].w;
        h = rousoku_flame_lighting[timer].h;
        break;
    case ROUSOKU_FLAME_BURN:
        w = CandleFlameScaleWGet(0, timer);
        h = CandleFlameScaleHGet(0, timer);
        break;
    case ROUSOKU_FLAME_GOING_OUT:
        w = rousoku_flame_going_out[timer].w;
        h = rousoku_flame_going_out[timer].h;
        break;
    case ROUSOKU_FLAME_FLARE:
        w = rousoku_flame_flare[timer].w;
        h = rousoku_flame_flare[timer].h + (GetRandValF(ROUSOKU_FLARE_RAND) + ROUSOKU_FLARE_ADD);
        break;
    }
}

// FF3 0x00245700.

void ROUSOKU_FLAME::Step()
{
    switch (state)
    {
    case ROUSOKU_FLAME_OFF:
        timer = 0;
        break;
    case ROUSOKU_FLAME_LIGHTING:
        if (++timer >= ROUSOKU_LIGHTING_FRAMES)
        {
            state = ROUSOKU_FLAME_BURN;
            timer = GetRandValIByF(ROUSOKU_BURN_FRAMES);
        }
        break;
    case ROUSOKU_FLAME_BURN:
        timer = (timer + 1) % ROUSOKU_BURN_FRAMES;
        break;
    case ROUSOKU_FLAME_GOING_OUT:
        if (++timer >= ROUSOKU_GOING_OUT_FRAMES)
        {
            state = ROUSOKU_FLAME_OFF;
            timer = 0;
        }
        break;
    case ROUSOKU_FLAME_FLARE:
        if (++timer >= ROUSOKU_FLARE_FRAMES)
        {
            state = ROUSOKU_FLAME_BURN;
            timer = GetRandValIByF(ROUSOKU_BURN_FRAMES);
        }
        break;
    }
    UpdateSize();
}

// ──────────────────────────────────────────────────────────────────────
// ROUSOKU_BOARD
// ──────────────────────────────────────────────────────────────────────

// FF3 0x00245858 (and its C2 copy, FF3 0x002457e0).
// callcheck: operator_new, RousokuFlameCtor (new ROUSOKU_FLAME)

ROUSOKU_BOARD::ROUSOKU_BOARD(const ROUSOKU_BOARD_DEF *def)
{
    this->def = def;
    for (int i = 0; i < ROUSOKU_CELL_NUM; i++)
        flame[i] = new ROUSOKU_FLAME();
    Reset();
}

// FF3 0x00245930 (and its copy, FF3 0x002458d0).
// callcheck: operator_delete (delete flame[i])

ROUSOKU_BOARD::~ROUSOKU_BOARD()
{
    for (int i = 0; i < ROUSOKU_CELL_NUM; i++)
    {
        if (flame[i] != nullptr)
            delete flame[i];
    }
}

// The cells as defined, their candles burning or out, the cursor on cell 0
// (RousokuBoardCreate moves it), the shadow shown and still.
// FF3 0x00245990.

void ROUSOKU_BOARD::Reset()
{
    for (int i = 0; i < ROUSOKU_CELL_NUM; i++)
    {
        cell[i] = def->cell[i];
        flame[i]->SetState((cell[i] & ROUSOKU_CELL_LIT) ? ROUSOKU_FLAME_BURN : ROUSOKU_FLAME_OFF);
    }
    CheckHalves();
    SaveShadow();
    show_buttons = 0;
    show_shadow = 1;
    cursor = 0;
    swing = 0;
}

// FF3 0x00245a30.

void ROUSOKU_BOARD::CheckHalves()
{
    for (int half = 0; half < ROUSOKU_HALF_NUM; half++)
    {
        int start = half * ROUSOKU_HALF_CELLS;

        half_movable[half] = 0;
        for (int i = start; i < start + ROUSOKU_HALF_CELLS; i++)
        {
            if ((cell[i] & ROUSOKU_CELL_FIXED) == 0)
            {
                half_movable[half] = 1;
                break;
            }
        }
    }
}

// FF3 0x00245ac8.

void ROUSOKU_BOARD::Toggle()
{
    u_int *c = &cell[cursor];

    if (*c & ROUSOKU_CELL_FIXED)
        return;
    *c ^= ROUSOKU_CELL_LIT;
    flame[cursor]->SetState((*c & ROUSOKU_CELL_LIT) ? ROUSOKU_FLAME_LIGHTING : ROUSOKU_FLAME_GOING_OUT);
}

// FF3 0x00245b40.
// callcheck: memset

ROUSOKU_SHADOW *ROUSOKU_BOARD::CalcShadow()
{
    memset(&shadow, 0, sizeof(shadow));
    for (int i = 0; i < ROUSOKU_CELL_NUM; i++)
    {
        if (cell[i] & ROUSOKU_CELL_LIT)
        {
            shadow.y += rousoku_cell_pos[i].y;
            shadow.x += rousoku_cell_pos[i].x;
        }
    }
    return &shadow;
}

// FF3 0x00245be0.

int ROUSOKU_BOARD::IsBalanced()
{
    const ROUSOKU_SHADOW *s = CalcShadow();

    return s->x == 0 && s->y == 0;
}

// FF3 0x00245c18.
// callcheck: memmove

void ROUSOKU_BOARD::SaveShadow()
{
    CalcShadow();
    memmove(&shadow_prev, &shadow, sizeof(shadow_prev));
}

// FF3 0x00245c50.

void ROUSOKU_BOARD::Step()
{
    for (int i = 0; i < ROUSOKU_CELL_NUM; i++)
        flame[i]->Step();
    if (swing > 0)
        swing--;
}

// Solved: every movable candle that is out lights, and every lit candle
// flares.
// FF3 0x00245cb8.

void ROUSOKU_BOARD::LightAll()
{
    for (int i = 0; i < ROUSOKU_CELL_NUM; i++)
    {
        if (cell[i] == 0)
            cell[i] = ROUSOKU_CELL_LIT;
        if (cell[i] & ROUSOKU_CELL_LIT)
            flame[i]->SetState(ROUSOKU_FLAME_FLARE);
    }
}
