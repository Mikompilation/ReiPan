/* ==========================================================================
 *  ingame/puzzle/kusi/kusi_board.h
 *
 *  The kusi puzzle's board: a 3 x 3 grid of cells (some missing), joined by
 *  links between neighbours, and pieces that each must make an exact number
 *  of moves. A piece moves one linked cell at a time into a free cell, never
 *  straight back to the cell it came from; the board is solved when every
 *  piece has made all its moves. Each piece keeps the cells it came from,
 *  the board keeps the order the pieces moved in, so moves can be undone.
 *  FF3 0x247710..0x248658, FF3's own code (FF2 has no such puzzle).
 * ======================================================================== */
#ifndef _INGAME_PUZZLE_KUSI_KUSI_BOARD_H
#define _INGAME_PUZZLE_KUSI_KUSI_BOARD_H

#include <stddef.h>                     // offsetof

#define KUSI_CELL_NUM       9           // 3 x 3
#define KUSI_CELL_W         3           // cells a row
#define KUSI_LINK_NUM       12          // link slots (no bounds check in FF3; boards use <= 9)
#define KUSI_HISTORY_NUM    15          // a piece's cells / the board's moves remembered

// A cell's offset on screen (data 0x330be0, KusiGetCellPos): 98 x 86 apart.
struct KUSI_POS                         /* 0x8 */
{
    /* 0x0 */ float x;
    /* 0x4 */ float y;
};
static_assert(sizeof(KUSI_POS) == 0x8, "KUSI_POS is 8 bytes");

// One board as the ROM defines it (data 0x330b38, 0x38 each; KusiGetBoardDef).
struct KUSI_BOARD_DEF                   /* 0x38 */
{
    /* 0x00 */ int rest;                        // moves in all (the sum of moves[])
    /* 0x04 */ int _04;                         // 0, unread
    /* 0x08 */ int moves[KUSI_CELL_NUM];        // -1 no cell, 0 an empty cell, n a piece with n moves
    /* 0x2c */ signed char link[KUSI_CELL_NUM]; // KUSI_DIR_* bits: the cell's links
    /* 0x35 */ char _35[3];
};
static_assert(sizeof(KUSI_BOARD_DEF) == 0x38, "KUSI_BOARD_DEF is 0x38 bytes");
static_assert(offsetof(KUSI_BOARD_DEF, moves) == 0x08, "KUSI_BOARD_DEF.moves");
static_assert(offsetof(KUSI_BOARD_DEF, link) == 0x2c, "KUSI_BOARD_DEF.link");

// Directions (link bits, KusiCsrGetCell's argument).
#define KUSI_DIR_UP         1           // cell - 3
#define KUSI_DIR_RIGHT      2           // cell + 1
#define KUSI_DIR_DOWN       4           // cell + 3
#define KUSI_DIR_LEFT       8           // cell - 1

// Two cells joined on the board (either order).
struct KUSI_LINK                        /* 0x8 */
{
    KUSI_LINK();                        // FF3 0x00247720 (C2 copy 0x247710): no cells (-1, -1)
    KUSI_LINK(int a, int b) : a(a), b(b) {}   // inline in FF3 (stack pairs)

    void Set(int a, int b);             // FF3 0x00247730 (no caller)
    int  IsSame(const KUSI_LINK *other) const;  // FF3 0x00247740: same pair, either order

    /* 0x0 */ int a;
    /* 0x4 */ int b;
};
static_assert(sizeof(KUSI_LINK) == 0x8, "KUSI_LINK is 8 bytes");
static_assert(offsetof(KUSI_LINK, b) == 0x4, "KUSI_LINK.b");

// A piece, its moves and its slide from cell to cell.
// layout: host -- history_top is an 8-byte pointer here (EE 0x64 bytes).
struct KUSI_PIECE                       /* 0x64 on the EE */
{
    KUSI_PIECE() : history_top(history) {}  // inline in FF3 (the new[] loop)

    void Init(int cell, int moves, const KUSI_POS *pos);    // FF3 0x00247790
    void MoveTo(const KUSI_POS *pos);   // FF3 0x002477d0: start a slide
    int  IsMoving() const;              // FF3 0x002477f8
    void Step();                        // FF3 0x00247830
    int  GetPrevCell() const;           // FF3 0x00247910: -1 before the first move
    int  Push(int cell);                // FF3 0x00247930: move to a cell
    int  Pop();                         // FF3 0x00247970: back a cell

    // Moves made / left (inline in FF3: (top - history) >> 2).
    int  GetDone() const { return (int)(history_top - history); }
    int  GetRest() const { return moves - GetDone(); }

    /* 0x00 */ int   cell;
    /* 0x04 */ int   history[KUSI_HISTORY_NUM];     // the cells it came from
    /* 0x40 */ int  *history_top;
    /* 0x44 */ int   moves;             // moves it must make
    /* 0x48 */ float x;                 // where it is drawn
    /* 0x4c */ float y;
    /* 0x50 */ float from_x;            // the slide
    /* 0x54 */ float from_y;
    /* 0x58 */ float to_x;
    /* 0x5c */ float to_y;
    /* 0x60 */ float t;                 // 0..1
};
static_assert(offsetof(KUSI_PIECE, history) == 0x04, "KUSI_PIECE.history");
static_assert(offsetof(KUSI_PIECE, history_top) == 0x40, "KUSI_PIECE.history_top");

// The board in play (operator_new(0xe4), sdata 0x371eb0 kusi_board).
// layout: host -- the pointers are 8 bytes here.
struct KUSI_BOARD                       /* 0xe4 on the EE */
{
    KUSI_BOARD(const KUSI_BOARD_DEF *def);  // FF3 0x00247b78 (C2 copy 0x247998)
    ~KUSI_BOARD();                          // FF3 0x00247e10 (copy 0x247d58)

    void AddLinks(int cell, signed char link);  // FF3 0x00247ec8
    void Select(int cell);                  // FF3 0x00248018: the piece on the cell
    KUSI_PIECE *GetPiece(int i);            // FF3 0x00248058
    int *GetOpen(int cell);                 // FF3 0x00248070
    int  IsSolved();                        // FF3 0x002480a0: every piece made all its moves
    int  IsFailed();                        // FF3 0x002480f0
    int  CanMove(int cell);                 // FF3 0x00248138: the selected piece to a cell
    void SlideTo(const KUSI_POS *pos);      // FF3 0x002481e8
    int  Move(int cell);                    // FF3 0x00248238
    int  HasHistory();                      // FF3 0x002482c0
    KUSI_PIECE *GetLastPiece();             // FF3 0x002482d8
    int  GetPrevCell();                     // FF3 0x002482f8
    int  Undo();                            // FF3 0x00248340
    int  HasLink(const KUSI_LINK *link);    // FF3 0x002483b8
    int  GetRest();                         // FF3 0x00248430: moves left on the board
    int  IsFree(int cell);                  // FF3 0x00248478
    int  CanSelect(int cell);               // FF3 0x002484f0
    int  IsMoving();                        // FF3 0x00248580
    void Step();                            // FF3 0x002485f8

    /* 0x00 */ const KUSI_BOARD_DEF *def;
    /* 0x04 */ KUSI_PIECE *piece;                       // new[] piece_num
    /* 0x08 */ KUSI_PIECE *selected;                    // the piece being moved, or nullptr
    /* 0x0c */ KUSI_LINK   link[KUSI_LINK_NUM];
    /* 0x6c */ int         open[KUSI_CELL_NUM];         // the cell exists
    /* 0x90 */ KUSI_PIECE *history[KUSI_HISTORY_NUM];   // the pieces in the order they moved
    /* 0xcc */ KUSI_PIECE **history_top;
    /* 0xd0 */ int piece_num;
    /* 0xd4 */ int link_num;
    /* 0xd8 */ int rest_max;                            // def->rest
    /* 0xdc */ int move_cnt;                            // moves of the selected piece this turn
    /* 0xe0 */ int _e0;                                 // never touched
};

#endif /* _INGAME_PUZZLE_KUSI_KUSI_BOARD_H */
