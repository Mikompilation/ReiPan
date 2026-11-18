/* ==========================================================================
 *  ingame/puzzle/rousoku/rousoku_board.h
 *
 *  The rousoku (candle) puzzle's board: two rings of nine candle places
 *  either side of a stone stake (cells 0..8 the left ring, 9..17 the right
 *  one, each ring's centre last). Each place has a vector (table 0x361a30),
 *  the shadow its candle casts: away from the candle's ring in x, stronger
 *  near the stake (5 beside it, 1 on the far side), and against its height
 *  in y. The lit candles' vectors add up to the stake's shadow, and the
 *  board is solved when that sum is (0, 0). Fixed places (bit 0x10) cannot be changed: lit they hold a
 *  burning candle, unlit they are empty; the player lights or puts out the
 *  movable candles. Each place has a flame object that grows, flickers,
 *  goes out or flares.
 *  FF3 0x245580..0x245d38, FF3's own code (FF2 has no such puzzle).
 * ======================================================================== */
#ifndef _INGAME_PUZZLE_ROUSOKU_ROUSOKU_BOARD_H
#define _INGAME_PUZZLE_ROUSOKU_ROUSOKU_BOARD_H

#include <stddef.h>                     // offsetof

#include "sdk/scetypes.h"               // u_int

#define ROUSOKU_CELL_NUM        18      // two rings of nine
#define ROUSOKU_HALF_CELLS      9       // cells 0..8 (left ring) / 9..17 (right ring)
#define ROUSOKU_HALF_NUM        2

// Cell bits.
#define ROUSOKU_CELL_LIT        0x01    // a burning candle
#define ROUSOKU_CELL_FIXED      0x10    // not movable: lit = a fixed candle, unlit = an empty place

// One board as the ROM defines it (data 0x361720, 0x48 each; RousokuGetBoardDef).
struct ROUSOKU_BOARD_DEF                /* 0x48 */
{
    /* 0x00 */ u_int cell[ROUSOKU_CELL_NUM];    // ROUSOKU_CELL_* at the start
};
static_assert(sizeof(ROUSOKU_BOARD_DEF) == 0x48, "ROUSOKU_BOARD_DEF is 0x48 bytes");

// The shadow's vector: the sum of the lit cells' positions.
struct ROUSOKU_SHADOW                   /* 0x4 */
{
    /* 0x0 */ short x;
    /* 0x2 */ short y;
};
static_assert(sizeof(ROUSOKU_SHADOW) == 0x4, "ROUSOKU_SHADOW is 4 bytes");

// ROUSOKU_FLAME.state.
enum ROUSOKU_FLAME_STATE
{
    ROUSOKU_FLAME_OFF       = 0,
    ROUSOKU_FLAME_LIGHTING  = 1,        // grows for 10 frames, then burns
    ROUSOKU_FLAME_BURN      = 2,        // flickers through effect_obj.c's 150-frame candle table
    ROUSOKU_FLAME_GOING_OUT = 3,        // 2 frames, then off
    ROUSOKU_FLAME_FLARE     = 4,        // 20 frames tall, then burns (the puzzle solved)
};

// A candle's flame: its state, the frame in it and the size the flame
// sprite is drawn at (RousokuDrawFlame; 0 = not drawn).
struct ROUSOKU_FLAME                    /* 0x10 */
{
    ROUSOKU_FLAME();                    // FF3 0x00245580

    void SetState(int state);           // FF3 0x00245598
    void UpdateSize();                  // FF3 0x002455f8
    void Step();                        // FF3 0x00245700

    /* 0x0 */ int   state;              // ROUSOKU_FLAME_*
    /* 0x4 */ int   timer;              // frames in the state (burning: the flicker frame 0..149)
    /* 0x8 */ float w;                  // sprite scale
    /* 0xc */ float h;
};
static_assert(sizeof(ROUSOKU_FLAME) == 0x10, "ROUSOKU_FLAME is 0x10 bytes");
static_assert(offsetof(ROUSOKU_FLAME, w) == 0x8, "ROUSOKU_FLAME.w");

// The board in play (operator_new(0xb4), sdata 0x371e48 rousoku_board).
// layout: host -- def and flame are pointers.
struct ROUSOKU_BOARD                    /* 0xb4 on the EE */
{
    ROUSOKU_BOARD(const ROUSOKU_BOARD_DEF *def);    // FF3 0x00245858 (C2 copy 0x2457e0)
    ~ROUSOKU_BOARD();                               // FF3 0x00245930 (copy 0x2458d0)

    void Reset();                                   // FF3 0x00245990: the board as defined
    void CheckHalves();                             // FF3 0x00245a30
    void Toggle();                                  // FF3 0x00245ac8: the cursor's movable candle
    ROUSOKU_SHADOW *CalcShadow();                   // FF3 0x00245b40
    int  IsBalanced();                              // FF3 0x00245be0: the shadow is (0, 0)
    void SaveShadow();                              // FF3 0x00245c18: shadow_prev = the shadow now
    void Step();                                    // FF3 0x00245c50: flames and the swing, each frame
    void LightAll();                                // FF3 0x00245cb8: solved

    /* 0x00 */ u_int          cell[ROUSOKU_CELL_NUM];   // ROUSOKU_CELL_*
    /* 0x48 */ ROUSOKU_SHADOW shadow;                   // CalcShadow's result
    /* 0x4c */ ROUSOKU_SHADOW shadow_prev;              // before the last move (the swing starts there)
    /* 0x50 */ const ROUSOKU_BOARD_DEF *def;
    /* 0x54 */ short          cursor;                   // cell 0..17
    /* 0x58 */ ROUSOKU_FLAME *flame[ROUSOKU_CELL_NUM];
    /* 0xa0 */ int            swing;                    // frames the shadow still swings, from 20
    /* 0xa4 */ int            half_movable[ROUSOKU_HALF_NUM];  // the half holds a movable candle
    /* 0xac */ int            show_shadow;
    /* 0xb0 */ int            show_buttons;             // the button guide (phase 104 sets it)
};
static_assert(offsetof(ROUSOKU_BOARD, shadow) == 0x48, "ROUSOKU_BOARD.shadow");
static_assert(offsetof(ROUSOKU_BOARD, shadow_prev) == 0x4c, "ROUSOKU_BOARD.shadow_prev");
static_assert(offsetof(ROUSOKU_BOARD, def) == 0x50, "ROUSOKU_BOARD.def");

#endif /* _INGAME_PUZZLE_ROUSOKU_ROUSOKU_BOARD_H */
