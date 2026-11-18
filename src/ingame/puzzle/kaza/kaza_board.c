/* ==========================================================================
 *  ingame/puzzle/kaza/kaza_board.c
 *
 *  The kaza puzzle's board (see kaza_board.h): the rings and the board's
 *  turns. FF3 0x24af48..0x24b490, FF3's own code.
 * ======================================================================== */
#include "ingame/puzzle/kaza/kaza_board.h"

#include <math.h>                               // fabsf

#include "system/os/system.h"                   // GetFrameTimeScale

#define KAZA_QUARTER        90.0f       // sdata 0x371f88 / 0x371f90
#define KAZA_HALF           180.0f      // sdata 0x371f94
#define KAZA_FULL           360.0f      // sdata 0x371f9c

// ──────────────────────────────────────────────────────────────────────
// KAZA_RING
// ──────────────────────────────────────────────────────────────────────

// FF3 0x0024af48.

void KAZA_RING::Init(const int *mark)
{
    this->mark = mark;
    pos = 0;
    angle = 0.0f;
}

// FF3 0x0024af58.

void KAZA_RING::Turn(int dir)
{
    if (dir == 0)
    {
        pos++;
        if (pos > KAZA_SIDE_NUM - 1)
            pos = 0;
    }
    else
    {
        pos--;
        if (pos < 0)
            pos = KAZA_SIDE_NUM - 1;
    }
}

// FF3 0x0024afa0.

int KAZA_RING::IsTurning()
{
    return fabsf(angle - (float)pos * KAZA_QUARTER) > 0.1f;    // sdata 0x371f8c
}

// Two degrees a frame (at 60 fps) toward pos's quarter, the short way round,
// kept in 0..360; it lands exactly once within reach.
// FF3 0x0024aff8.

void KAZA_RING::Step()
{
    float step = GetFrameTimeScale();
    step = step + step;
    float target = (float)pos * KAZA_QUARTER;

    if (target - angle < 0.0f)
        step = -step;
    if (fabsf(target - angle) >= KAZA_HALF)
        step = -step;

    if (fabsf(target - angle) <= fabsf(step) + 0.0001f)        // sdata 0x371f98
        angle = target;
    else
        angle = angle + step;

    if (angle >= KAZA_FULL)
        angle = angle - KAZA_FULL;
    if (angle < 0.0f)
        angle = angle + KAZA_FULL;

    if (IsTurning() == 0)
        angle = target;
}

// FF3 0x0024b120.

int KAZA_RING::GetMark(int side)
{
    return mark[(side - pos + KAZA_SIDE_NUM) % KAZA_SIDE_NUM];
}

// ──────────────────────────────────────────────────────────────────────
// KAZA_BOARD
// ──────────────────────────────────────────────────────────────────────

// FF3 0x0024b1b8 (and its copy 0x0024b158).

KAZA_BOARD::KAZA_BOARD(const KAZA_BOARD_DEF *def)
{
    cursor = 0;
    rest = def->rest;
    for (int i = 0; i < KAZA_RING_NUM; i++)
        ring[i].Init(def->mark[i]);
}

// Each side of the centre ring against the outer ring it faces: centre side
// 3 with ring 0's side 1, 0 with ring 1's 2, 2 with ring 2's 0, 1 with ring
// 3's 3.
// FF3 0x0024b218.

int KAZA_BOARD::IsSolved()
{
    KAZA_RING *centre = &ring[KAZA_RING_CENTRE];

    if (centre->GetMark(3) != ring[0].GetMark(1))
        return 0;
    if (centre->GetMark(0) != ring[1].GetMark(2))
        return 0;
    if (centre->GetMark(2) != ring[2].GetMark(0))
        return 0;
    return centre->GetMark(1) == ring[3].GetMark(3);
}

// FF3 0x0024b310.

int KAZA_BOARD::IsFailed()
{
    if (IsSolved() != 0)
        return 0;
    return rest < 1;
}

// FF3 0x0024b358.

int KAZA_BOARD::IsTurning()
{
    for (int i = 0; i < KAZA_RING_NUM; i++)
    {
        if (ring[i].IsTurning() != 0)
            return 1;
    }
    return 0;
}

// FF3 0x0024b3a8.

void KAZA_BOARD::Step()
{
    for (int i = 0; i < KAZA_RING_NUM; i++)
        ring[i].Step();
}

// FF3 0x0024b3e8.

void KAZA_BOARD::DecRest()
{
    int n = rest - 1;
    if (n < 1)
        n = 0;
    rest = n;
}

// FF3 0x0024b400.

void KAZA_BOARD::SetCursor(int ring)
{
    cursor = ring;
}

// FF3 0x0024b408.

void KAZA_BOARD::TurnRing(int i, int dir)
{
    ring[i].Turn(dir);
    ring[KAZA_RING_CENTRE].Turn(dir == 0);
}

// FF3 0x0024b460.

void KAZA_BOARD::TurnCursor(int dir)
{
    TurnRing(cursor, dir);
    DecRest();
}
