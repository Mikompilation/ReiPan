/* ==========================================================================
 *  ingame/puzzle/kusi/kusi_board.c
 *
 *  The kusi puzzle's board (see kusi_board.h): links, pieces and their
 *  slides, the board's moves and undo. FF3 0x247710..0x248658, FF3's own
 *  code.
 * ======================================================================== */
#include "ingame/puzzle/kusi/kusi_board.h"

#include "ingame/puzzle/kusi/kusi_draw.h"       // KusiGetCellPos
#include "system/os/system.h"                   // GetFrameTimeScale

// The directions' bits and cell steps, in the order AddLinks tries them
// (sdata 0x371f08 / 0x371f10, four bytes each).
static const signed char kusi_dir_bit[4]  = { KUSI_DIR_UP, KUSI_DIR_RIGHT, KUSI_DIR_DOWN, KUSI_DIR_LEFT };
static const signed char kusi_dir_step[4] = { -KUSI_CELL_W, 1, KUSI_CELL_W, -1 };

#define KUSI_SLIDE_SPEED    0.05f       // of the way a frame (sdata 0x371ef8)
#define KUSI_SLIDE_SNAP     0.99999f    // sdata 0x371efc
#define KUSI_SLIDE_END      1.0f        // sdata 0x371f00

// ──────────────────────────────────────────────────────────────────────
// KUSI_LINK
// ──────────────────────────────────────────────────────────────────────

// FF3 0x00247720 (and its C2 copy 0x00247710).

KUSI_LINK::KUSI_LINK()
{
    b = -1;
    a = -1;
}

// FF3 0x00247730.

void KUSI_LINK::Set(int a, int b)
{
    this->a = a;
    this->b = b;
}

// FF3 0x00247740.

int KUSI_LINK::IsSame(const KUSI_LINK *other) const
{
    if (a == other->a && b == other->b)
        return 1;
    if (a == other->b && b == other->a)
        return 1;
    return 0;
}

// ──────────────────────────────────────────────────────────────────────
// KUSI_PIECE
// ──────────────────────────────────────────────────────────────────────

// FF3 0x00247790.

void KUSI_PIECE::Init(int cell, int moves, const KUSI_POS *pos)
{
    this->cell = cell;
    this->moves = moves;
    t = 0.0f;
    x = pos->x;
    y = pos->y;
    from_x = pos->x;
    from_y = pos->y;
    to_x = pos->x;
    to_y = pos->y;
}

// FF3 0x002477d0.

void KUSI_PIECE::MoveTo(const KUSI_POS *pos)
{
    t = 0.0f;
    to_x = pos->x;
    to_y = pos->y;
    from_y = y;
    from_x = x;
}

// FF3 0x002477f8.

int KUSI_PIECE::IsMoving() const
{
    if (x != to_x)
        return 1;
    return y != to_y;
}

// The slide: 1/20 of the way a frame (PAL: scaled), snapped at the end.
// FF3 0x00247830.

void KUSI_PIECE::Step()
{
    t = t + GetFrameTimeScale() * KUSI_SLIDE_SPEED;
    if (t >= KUSI_SLIDE_SNAP)
        t = KUSI_SLIDE_END;

    float r = 0.0f;
    if (t >= 0.0f)
    {
        r = KUSI_SLIDE_END;
        if (t <= KUSI_SLIDE_END)
            r = t;
    }
    int done = r >= KUSI_SLIDE_END;
    t = r;

    y = from_y + (to_y - from_y) * r;
    x = from_x + (to_x - from_x) * r;
    if (done)
    {
        y = to_y;
        x = to_x;
    }
}

// FF3 0x00247910.

int KUSI_PIECE::GetPrevCell() const
{
    if (history_top == history)
        return -1;
    return history_top[-1];
}

// Past 15 moves the oldest cells are no longer remembered (the piece still
// moves). FF3 0x00247930.

int KUSI_PIECE::Push(int cell)
{
    if (GetDone() < KUSI_HISTORY_NUM)
        *history_top++ = this->cell;
    this->cell = cell;
    return 1;
}

// FF3 0x00247970. With nothing to pop FF3 reads history[-1], which is
// .cell itself, so the piece stays; PC: says so instead of reading it.

int KUSI_PIECE::Pop()
{
    if (history_top != history)
    {
        cell = history_top[-1];
        history_top--;
    }
    return 1;
}

// ──────────────────────────────────────────────────────────────────────
// KUSI_BOARD
// ──────────────────────────────────────────────────────────────────────

// The links from every cell's bits, then a piece on each cell with moves
// (in cell order) and the cells that exist.
// FF3 0x00247b78 (and its C2 copy 0x00247998).
// callcheck: KusiLinkCtor, operator_new_array (link[] members; new KUSI_PIECE[])

KUSI_BOARD::KUSI_BOARD(const KUSI_BOARD_DEF *def)
{
    int cells[12];

    history_top = history;
    piece = nullptr;
    selected = nullptr;
    piece_num = 0;
    link_num = 0;
    move_cnt = 0;
    this->def = def;
    rest_max = def->rest;

    for (int i = 0; i < KUSI_CELL_NUM; i++)
        AddLinks(i, def->link[i]);

    for (int i = 0; i < KUSI_CELL_NUM; i++)
    {
        if (this->def->moves[i] > 0)
            cells[piece_num++] = i;
    }

    piece = new KUSI_PIECE[piece_num];
    for (int i = 0; i < piece_num; i++)
    {
        int cell = cells[i];
        piece[i].Init(cell, this->def->moves[cell], KusiGetCellPos(cell));
    }

    for (int i = 0; i < KUSI_CELL_NUM; i++)
        open[i] = def->moves[i] >= 0;
}

// FF3 0x00247e10 (and its copy 0x00247d58).
// callcheck: operator_delete_array (delete[] piece)

KUSI_BOARD::~KUSI_BOARD()
{
    if (piece != nullptr)
        delete[] piece;
}

// Each direction bit of the cell links it to that neighbour, once.
// FF3 0x00247ec8 (FF2's SaveLoadClearNumberDisp matched it).
// callcheck: KusiLinkIsSame (IsSame)

void KUSI_BOARD::AddLinks(int cell, signed char link)
{
    for (int i = 0; i < 4; i++)
    {
        if ((link & kusi_dir_bit[i]) == 0)
            continue;

        KUSI_LINK add(cell, cell + kusi_dir_step[i]);
        int found = 0;
        for (int j = 0; j < link_num; j++)
        {
            if (this->link[j].IsSame(&add))
            {
                found = 1;
                break;
            }
        }
        if (!found)
        {
            this->link[link_num] = add;
            link_num++;
        }
    }
}

// FF3 0x00248018.

void KUSI_BOARD::Select(int cell)
{
    for (int i = 0; i < piece_num; i++)
    {
        if (piece[i].cell == cell)
        {
            selected = &piece[i];
            return;
        }
    }
}

// FF3 0x00248058.

KUSI_PIECE *KUSI_BOARD::GetPiece(int i)
{
    return &piece[i];
}

// FF3 0x00248070.

int *KUSI_BOARD::GetOpen(int cell)
{
    return &open[cell];
}

// FF3 0x002480a0.

int KUSI_BOARD::IsSolved()
{
    for (int i = 0; i < piece_num; i++)
    {
        if (piece[i].moves != piece[i].GetDone())
            return 0;
    }
    return 1;
}

// Never true: a board without moves left is solved. Its one caller drops
// the result. FF3 0x002480f0.
// callcheck: KusiBoardIsSolved, KusiBoardGetRest

int KUSI_BOARD::IsFailed()
{
    if (IsSolved() != 0)
        return 0;
    return GetRest() < 1;
}

// The selected piece can go to a cell: linked, free, not where it just
// came from, and it has moves left.
// FF3 0x00248138.
// callcheck: KusiBoardHasLink, KusiBoardIsFree, KusiPieceGetPrevCell, KusiBoardGetRest

int KUSI_BOARD::CanMove(int cell)
{
    if (selected == nullptr)
        return 0;

    KUSI_LINK link(selected->cell, cell);
    if (HasLink(&link) == 0)
        return 0;
    if (IsFree(cell) == 0)
        return 0;
    if (cell == selected->GetPrevCell())
        return 0;
    if (selected->GetRest() <= 0)
        return 0;
    return GetRest() > 0;
}

// The selected piece, or the last one moved (an undo outside a turn).
// FF3 0x002481e8.
// callcheck: KusiBoardGetLastPiece, KusiPieceMoveTo

void KUSI_BOARD::SlideTo(const KUSI_POS *pos)
{
    KUSI_PIECE *p = selected;

    if (p == nullptr && (p = GetLastPiece()) == nullptr)
        return;
    p->MoveTo(pos);
}

// FF3 0x00248238.
// callcheck: KusiBoardCanMove, KusiPiecePush

int KUSI_BOARD::Move(int cell)
{
    if (CanMove(cell) == 0)
        return 0;

    selected->Push(cell);
    if (history_top - history < KUSI_HISTORY_NUM)
        *history_top++ = selected;
    move_cnt++;
    return 1;
}

// FF3 0x002482c0.

int KUSI_BOARD::HasHistory()
{
    return history_top != history;
}

// FF3 0x002482d8.

KUSI_PIECE *KUSI_BOARD::GetLastPiece()
{
    if (history_top == history)
        return nullptr;
    return history_top[-1];
}

// FF3 0x002482f8.
// callcheck: KusiBoardGetLastPiece, KusiPieceGetPrevCell

int KUSI_BOARD::GetPrevCell()
{
    KUSI_PIECE *p = selected;

    if (p == nullptr && (p = GetLastPiece()) == nullptr)
        return -1;
    return p->GetPrevCell();
}

// The last move taken back.
// FF3 0x00248340 (FF2's SavePointStartReq matched it).
// callcheck: KusiBoardHasHistory, KusiPiecePop

int KUSI_BOARD::Undo()
{
    if (HasHistory() == 0)
        return 0;

    KUSI_PIECE *p = history_top[-1];
    if (selected != nullptr)
        move_cnt--;
    p->Pop();
    if (history_top != history)
        history_top--;
    return 1;
}

// FF3 0x002483b8.
// callcheck: KusiLinkIsSame (IsSame)

int KUSI_BOARD::HasLink(const KUSI_LINK *link)
{
    for (int i = 0; i < link_num; i++)
    {
        if (link->IsSame(&this->link[i]))
            return 1;
    }
    return 0;
}

// FF3 0x00248430.

int KUSI_BOARD::GetRest()
{
    int rest = 0;

    for (int i = 0; i < piece_num; i++)
        rest += piece[i].GetRest();
    return rest;
}

// FF3 0x00248478.
// callcheck: KusiBoardGetOpen

int KUSI_BOARD::IsFree(int cell)
{
    if (*GetOpen(cell) == 0)
        return 0;
    for (int i = 0; i < piece_num; i++)
    {
        if (piece[i].cell == cell)
            return 0;
    }
    return 1;
}

// FF3 0x002484f0.
// callcheck: KusiBoardGetOpen

int KUSI_BOARD::CanSelect(int cell)
{
    if (*GetOpen(cell) == 0)
        return 0;
    for (int i = 0; i < piece_num; i++)
    {
        if (piece[i].cell == cell && piece[i].GetRest() > 0)
            return 1;
    }
    return 0;
}

// FF3 0x00248580.
// callcheck: KusiPieceIsMoving

int KUSI_BOARD::IsMoving()
{
    for (int i = 0; i < piece_num; i++)
    {
        if (piece[i].IsMoving() != 0)
            return 1;
    }
    return 0;
}

// FF3 0x002485f8.
// callcheck: KusiPieceStep

void KUSI_BOARD::Step()
{
    for (int i = 0; i < piece_num; i++)
        piece[i].Step();
}
