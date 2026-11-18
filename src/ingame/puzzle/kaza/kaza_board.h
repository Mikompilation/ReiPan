/* ==========================================================================
 *  ingame/puzzle/kaza/kaza_board.h
 *
 *  The kaza puzzle's board: four outer rings around a centre ring, each
 *  showing a mark on each of its four sides. Turning an outer ring a quarter
 *  turns the centre ring a quarter the other way and costs a move; the board
 *  is solved when each side of the centre ring shows the mark of the outer
 *  ring it faces.
 *  FF3 0x24af48..0x24b490, FF3's own code (FF2 has no such puzzle).
 * ======================================================================== */
#ifndef _INGAME_PUZZLE_KAZA_KAZA_BOARD_H
#define _INGAME_PUZZLE_KAZA_KAZA_BOARD_H

#include <stddef.h>                     // offsetof

#define KAZA_RING_NUM       5           // rings 0..3 outer, 4 the centre
#define KAZA_RING_CENTRE    4
#define KAZA_SIDE_NUM       4           // quarter turns / sides of a ring

// One board as the ROM defines it (data 0x361da0, 0x54 each; KazaGetBoardDef).
struct KAZA_BOARD_DEF                   /* 0x54 */
{
    /* 0x00 */ int rest;                                // moves allowed
    /* 0x04 */ int mark[KAZA_RING_NUM][KAZA_SIDE_NUM];  // each ring's marks, unturned
};
static_assert(sizeof(KAZA_BOARD_DEF) == 0x54, "KAZA_BOARD_DEF is 0x54 bytes");
static_assert(offsetof(KAZA_BOARD_DEF, mark) == 0x04, "KAZA_BOARD_DEF.mark");

// A ring: the quarter turns it was given and the angle it is drawn at, which
// follows them a few degrees a frame.
// layout: host -- mark is an 8-byte pointer here.
struct KAZA_RING                        /* 0xc on the EE */
{
    void  Init(const int *mark);        // FF3 0x0024af48
    void  Turn(int dir);                // FF3 0x0024af58: dir 0 a quarter on, else back
    int   IsTurning();                  // FF3 0x0024afa0
    void  Step();                       // FF3 0x0024aff8
    int   GetMark(int side);            // FF3 0x0024b120: the mark now on side 0..3

    /* 0x0 */ int        pos;           // quarter turns, 0..3
    /* 0x4 */ float      angle;         // degrees, 0..360; pos * 90 at rest
    /* 0x8 */ const int *mark;          // KAZA_BOARD_DEF.mark[ring]
};
static_assert(offsetof(KAZA_RING, angle) == 0x4, "KAZA_RING.angle");

// The board in play (operator_new(0x44), sdata 0x371f78 kaza_board).
// layout: host -- the rings hold pointers.
struct KAZA_BOARD                       /* 0x44 on the EE */
{
    KAZA_BOARD(const KAZA_BOARD_DEF *def);      // FF3 0x0024b1b8 (copy 0x24b158)

    int  IsSolved();                            // FF3 0x0024b218
    int  IsFailed();                            // FF3 0x0024b310: not solved, no moves left
    int  IsTurning();                           // FF3 0x0024b358
    void Step();                                // FF3 0x0024b3a8
    void DecRest();                             // FF3 0x0024b3e8
    void SetCursor(int ring);                   // FF3 0x0024b400
    void TurnRing(int ring, int dir);           // FF3 0x0024b408: the centre turns the other way
    void TurnCursor(int dir);                   // FF3 0x0024b460: TurnRing(cursor) and a move

    /* 0x00 */ KAZA_RING ring[KAZA_RING_NUM];
    /* 0x3c */ int rest;                        // moves left
    /* 0x40 */ int cursor;                      // outer ring 0..3 (menu cursor 0x5f)
};

#endif /* _INGAME_PUZZLE_KAZA_KAZA_BOARD_H */
