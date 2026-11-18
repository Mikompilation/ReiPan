/* ==========================================================================
 *  ingame/puzzle/kamon/kamon_board.h
 *
 *  The kamon (family crest) puzzle's board: five slots joined by links, five
 *  crest pieces of which one is missing (its slot is the hole), a cursor on a
 *  slot next to the hole, a move budget and an undo stack. A move slides the
 *  cursor's piece along its link into the hole; the board is solved when
 *  every slot holds its own piece or the hole.
 *  FF3 0x249930..0x24a6c8, FF3's own code (FF2 has no such puzzle).
 * ======================================================================== */
#ifndef _INGAME_PUZZLE_KAMON_KAMON_BOARD_H
#define _INGAME_PUZZLE_KAMON_KAMON_BOARD_H

#include <stddef.h>                     // offsetof

#define KAMON_SLOT_NUM      5           // slots on the board
#define KAMON_PIECE_NUM     6           // piece ids 0..4, 5 = the hole
#define KAMON_HOLE          5           // the hole's piece id (and "no slot")
#define KAMON_HISTORY_NUM   9           // undo stack depth

// A slot position on screen (data 0x330c28, KamonGetSlotPos).
struct KAMON_POS                        /* 0x8 */
{
    /* 0x0 */ float x;
    /* 0x4 */ float y;
};
static_assert(sizeof(KAMON_POS) == 0x8, "KAMON_POS is 8 bytes");

// A link as the ROM's tables store it (board definitions, the draw table).
struct KAMON_LINK_DEF                   /* 0x8 */
{
    /* 0x0 */ int a;
    /* 0x4 */ int b;
};
static_assert(sizeof(KAMON_LINK_DEF) == 0x8, "KAMON_LINK_DEF is 8 bytes");

// Two slots joined on the board (either order), or one undo step.
struct KAMON_LINK                       /* 0x8 */
{
    KAMON_LINK();                       // FF3 0x00249940 (C2 copy 0x249930): no slots (5, 5)
    KAMON_LINK(int a, int b) : a(a), b(b) {}  // inline in FF3 (stack pairs)

    void Set(int a, int b);             // FF3 0x00249950
    int  GetA() const;                  // FF3 0x00249960
    int  GetB() const;                  // FF3 0x00249968
    int  IsSame(const KAMON_LINK *other) const;     // FF3 0x00249970: same pair, either order

    /* 0x0 */ int a;
    /* 0x4 */ int b;
};
static_assert(sizeof(KAMON_LINK) == 0x8, "KAMON_LINK is 8 bytes");
static_assert(offsetof(KAMON_LINK, b) == 0x4, "KAMON_LINK.b");

// A crest piece and its slide from one slot to another.
struct KAMON_PIECE                      /* 0x20 */
{
    void Init(int id, const KAMON_POS *pos);    // FF3 0x002499b0
    void MoveTo(const KAMON_POS *pos);          // FF3 0x002499f0
    int  IsMoving() const;                      // FF3 0x00249a18
    void Step();                                // FF3 0x00249a50

    /* 0x00 */ int   id;
    /* 0x04 */ float x;                 // where it is drawn
    /* 0x08 */ float y;
    /* 0x0c */ float from_x;            // the slide
    /* 0x10 */ float from_y;
    /* 0x14 */ float to_x;
    /* 0x18 */ float to_y;
    /* 0x1c */ float t;                 // 0..1
};
static_assert(sizeof(KAMON_PIECE) == 0x20, "KAMON_PIECE is 0x20 bytes");
static_assert(offsetof(KAMON_PIECE, from_x) == 0x0c, "KAMON_PIECE.from_x");
static_assert(offsetof(KAMON_PIECE, to_x) == 0x14, "KAMON_PIECE.to_x");
static_assert(offsetof(KAMON_PIECE, t) == 0x1c, "KAMON_PIECE.t");

// One board as the ROM defines it (data 0x361c50, 0x24 each; KamonGetBoardDef).
// layout: host -- the link pointer is 8 bytes here.
struct KAMON_BOARD_DEF
{
    /* 0x00 */ int rest;                        // moves allowed
    /* 0x04 */ int cursor;                      // first cursor slot
    /* 0x08 */ int slot_piece[KAMON_SLOT_NUM];  // the piece id in each slot (one KAMON_HOLE)
    /* 0x1c */ int link_num;
    /* 0x20 */ const KAMON_LINK_DEF *link;
};
static_assert(offsetof(KAMON_BOARD_DEF, slot_piece) == 0x08, "KAMON_BOARD_DEF.slot_piece");
static_assert(offsetof(KAMON_BOARD_DEF, link_num) == 0x1c, "KAMON_BOARD_DEF.link_num");

// The board in play (operator_new(0x138), sdata 0x371f18 kamon_board).
// layout: host -- slot, link and history_top are 8-byte pointers here.
struct KAMON_BOARD                      /* 0x138 on the EE */
{
    KAMON_BOARD(const KAMON_BOARD_DEF *def);    // FF3 0x00249ca8 (C2 copy 0x249b30)
    ~KAMON_BOARD();                             // FF3 0x00249ef8 (copy 0x249e20)

    KAMON_LINK   *GetLink(int i);               // FF3 0x00249fd0
    KAMON_PIECE **GetSlot(int slot);            // FF3 0x00249fe0
    KAMON_PIECE  *GetPiece(int id);             // FF3 0x00249ff8
    int  HasLink(const KAMON_LINK *link);       // FF3 0x0024a008
    int  IsSolved();                            // FF3 0x0024a088
    int  IsFailed();                            // FF3 0x0024a0c8
    int  Undo();                                // FF3 0x0024a110
    int  IsHoleLink(const KAMON_LINK *link);    // FF3 0x0024a1b0
    int  Move();                                // FF3 0x0024a240
    int  Swap(const KAMON_LINK *link);          // FF3 0x0024a348
    void DecRest();                             // FF3 0x0024a3e8
    void IncRest();                             // FF3 0x0024a408
    int  GetHole();                             // FF3 0x0024a430
    void SetCursor(int slot);                   // FF3 0x0024a470
    void CursorNext();                          // FF3 0x0024a478
    void CursorPrev();                          // FF3 0x0024a528
    int  IsMoving();                            // FF3 0x0024a5d0
    void Step();                                // FF3 0x0024a658

    /* 0x000 */ KAMON_PIECE  piece[KAMON_PIECE_NUM];    // by id
    /* 0x0c0 */ KAMON_PIECE *slot[KAMON_SLOT_NUM];      // the piece in each slot
    /* 0x0d4 */ KAMON_LINK  *link;                      // new[] link_num
    /* 0x0d8 */ KAMON_LINK   history[KAMON_HISTORY_NUM];    // the links moved along
    /* 0x120 */ KAMON_LINK  *history_top;
    /* 0x124 */ int link_num;
    /* 0x128 */ int rest;                       // moves left
    /* 0x12c */ int rest_prev;                  // rest before the last move or undo
    /* 0x130 */ int rest_max;
    /* 0x134 */ int cursor;                     // slot
};
static_assert(offsetof(KAMON_BOARD, slot) == 0xc0, "KAMON_BOARD.slot");

#endif /* _INGAME_PUZZLE_KAMON_KAMON_BOARD_H */
