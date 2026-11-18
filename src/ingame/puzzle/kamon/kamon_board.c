/* ==========================================================================
 *  ingame/puzzle/kamon/kamon_board.c
 *
 *  The kamon puzzle's board (see kamon_board.h): links, pieces, the board's
 *  moves, undo and cursor. FF3 0x249930..0x24a6c8, FF3's own code.
 * ======================================================================== */
#include "ingame/puzzle/kamon/kamon_board.h"

#include "sdk/scetypes.h"                        // u_int
#include "ingame/puzzle/kamon/kamon_draw.h"     // KamonGetSlotPos
#include "system/os/system.h"                   // GetFrameTimeScale

// ──────────────────────────────────────────────────────────────────────
// KAMON_LINK
// ──────────────────────────────────────────────────────────────────────

// FF3 0x00249940 (and its C2 copy 0x00249930).

KAMON_LINK::KAMON_LINK()
{
    b = KAMON_HOLE;
    a = KAMON_HOLE;
}

// FF3 0x00249950.

void KAMON_LINK::Set(int a, int b)
{
    this->a = a;
    this->b = b;
}

// FF3 0x00249960.

int KAMON_LINK::GetA() const
{
    return a;
}

// FF3 0x00249968.

int KAMON_LINK::GetB() const
{
    return b;
}

// FF3 0x00249970.

int KAMON_LINK::IsSame(const KAMON_LINK *other) const
{
    if (a == other->a && b == other->b)
        return 1;
    if (a == other->b && b == other->a)
        return 1;
    return 0;
}

// ──────────────────────────────────────────────────────────────────────
// KAMON_PIECE
// ──────────────────────────────────────────────────────────────────────

// FF3 0x002499b0.

void KAMON_PIECE::Init(int id, const KAMON_POS *pos)
{
    this->id = id;
    t = 0.0f;
    x = pos->x;
    y = pos->y;
    from_x = pos->x;
    from_y = pos->y;
    to_x = pos->x;
    to_y = pos->y;
}

// FF3 0x002499f0.

void KAMON_PIECE::MoveTo(const KAMON_POS *pos)
{
    t = 0.0f;
    to_x = pos->x;
    from_y = y;
    to_y = pos->y;
    from_x = x;
}

// FF3 0x00249a18.

int KAMON_PIECE::IsMoving() const
{
    if (x != to_x)
        return 1;
    return y != to_y;
}

// The slide: 1/20 of the way a frame (PAL: scaled), snapped at the end.
// FF3 0x00249a50.

void KAMON_PIECE::Step()
{
    t = t + GetFrameTimeScale() * 0.05f;
    if (t >= 0.99999f)
        t = 1.0f;

    float r = 0.0f;
    if (t >= 0.0f)
    {
        r = 1.0f;
        if (t <= 1.0f)
            r = t;
    }
    int done = r >= 1.0f;
    t = r;

    y = from_y + (to_y - from_y) * r;
    x = from_x + (to_x - from_x) * r;
    if (done)
    {
        y = to_y;
        x = to_x;
    }
}

// ──────────────────────────────────────────────────────────────────────
// KAMON_BOARD
// ──────────────────────────────────────────────────────────────────────

// The undo stack's entries start as (5, 5) (their constructor); the pieces
// not on the board stay unset, as in FF3.
// FF3 0x00249ca8 (and its C2 copy 0x00249b30).
// callcheck: KamonLinkCtor, operator_new_array (history[] and new KAMON_LINK[])

KAMON_BOARD::KAMON_BOARD(const KAMON_BOARD_DEF *def)
{
    history_top = history;
    rest = def->rest;
    rest_prev = def->rest;
    rest_max = def->rest;
    cursor = def->cursor;
    link_num = def->link_num;

    link = new KAMON_LINK[link_num];
    for (int i = 0; i < link_num; i++)
        link[i].Set(def->link[i].a, def->link[i].b);

    for (int i = 0; i < KAMON_SLOT_NUM; i++)
    {
        int id = def->slot_piece[i];
        piece[id].Init(id, KamonGetSlotPos(i));
        slot[i] = &piece[id];
    }
}

// FF3 0x00249ef8 (and its copy 0x00249e20).
// callcheck: operator_delete_array (delete[] link)

KAMON_BOARD::~KAMON_BOARD()
{
    if (link != nullptr)
        delete[] link;
}

// FF3 0x00249fd0.

KAMON_LINK *KAMON_BOARD::GetLink(int i)
{
    return &link[i];
}

// FF3 0x00249fe0.

KAMON_PIECE **KAMON_BOARD::GetSlot(int slot)
{
    return &this->slot[slot];
}

// FF3 0x00249ff8.

KAMON_PIECE *KAMON_BOARD::GetPiece(int id)
{
    return &piece[id];
}

// FF3 0x0024a008.

int KAMON_BOARD::HasLink(const KAMON_LINK *link)
{
    for (int i = 0; i < link_num; i++)
    {
        if (link->IsSame(GetLink(i)))
            return 1;
    }
    return 0;
}

// FF3 0x0024a088.

int KAMON_BOARD::IsSolved()
{
    for (int i = 0; i < KAMON_SLOT_NUM; i++)
    {
        int id = slot[i]->id;
        if (i != id && id != KAMON_HOLE)
            return 0;
    }
    return 1;
}

// FF3 0x0024a0c8.

int KAMON_BOARD::IsFailed()
{
    if (IsSolved() != 0)
        return 0;
    return rest < 1;
}

// Takes back the last move: swaps along the top of the undo stack and slides
// the piece back. 0 when no move was made.
// FF3 0x0024a110.

int KAMON_BOARD::Undo()
{
    if (rest == rest_max)
        return 0;

    int s = Swap(history_top - 1);
    IncRest();
    piece[slot[s]->id].MoveTo(KamonGetSlotPos(s));
    if (history_top != history)
        history_top--;
    return 1;
}

// FF3 0x0024a1b0.

int KAMON_BOARD::IsHoleLink(const KAMON_LINK *link)
{
    int hole = GetHole();

    if (link->GetA() == hole && link->GetB() == cursor)
        return 1;
    if (link->GetB() != hole)
        return 0;
    return link->GetA() == cursor;
}

// Slides the cursor's piece into the hole along their link, pushes the link
// on the undo stack (while there is room) and spends a move. 0 when no move
// is left.
// FF3 0x0024a240.

int KAMON_BOARD::Move()
{
    if (rest <= 0)
        return 0;

    int i;
    for (i = 0; i < link_num; i++)
    {
        if (IsHoleLink(&link[i]))
            break;
    }
    // PC: FF3 goes on with link[-1] when no link joins the cursor and the
    // hole; the cursor only ever stops on such a slot, so this never runs.
    if (i >= link_num)
        return 0;

    const KAMON_LINK *l = &link[i];
    int s = Swap(l);
    DecRest();
    if ((u_int)(history_top - history) < KAMON_HISTORY_NUM)
    {
        *history_top = *l;
        history_top++;
    }
    piece[slot[s]->id].MoveTo(KamonGetSlotPos(s));
    return 1;
}

// Swaps the two slots' pieces; the cursor moves with the piece (to the slot
// that does not hold the hole). Returns the cursor.
// FF3 0x0024a348.

int KAMON_BOARD::Swap(const KAMON_LINK *link)
{
    int a = link->GetA();
    int b = link->GetB();

    KAMON_PIECE *p = slot[a];
    slot[a] = slot[b];
    slot[b] = p;

    if (slot[a]->id == KAMON_HOLE)
        SetCursor(link->GetB());
    else
        SetCursor(link->GetA());
    return cursor;
}

// FF3 0x0024a3e8.

void KAMON_BOARD::DecRest()
{
    int n = rest - 1;
    rest_prev = rest;
    if (n < 0)
        n = 0;
    rest = n;
}

// FF3 0x0024a408.

void KAMON_BOARD::IncRest()
{
    int n = rest + 1;
    rest_prev = rest;
    rest = n;
    if (rest_max < n)
        rest = rest_max;
}

// FF3 0x0024a430.

int KAMON_BOARD::GetHole()
{
    for (int i = 0; i < KAMON_SLOT_NUM; i++)
    {
        if (slot[i]->id == KAMON_HOLE)
            return i;
    }
    return KAMON_HOLE;
}

// FF3 0x0024a470.

void KAMON_BOARD::SetCursor(int slot)
{
    cursor = slot;
}

// The next slot round the board (by index) that a link joins to the hole.
// FF3 0x0024a478.

void KAMON_BOARD::CursorNext()
{
    int cur = cursor;
    int hole = GetHole();

    for (int k = 1; k < KAMON_SLOT_NUM; k++)
    {
        int s = (cur + k) % KAMON_SLOT_NUM;
        if (s == hole)
            continue;
        KAMON_LINK l(s, hole);
        if (HasLink(&l))
        {
            cursor = s;
            return;
        }
    }
}

// FF3 0x0024a528.

void KAMON_BOARD::CursorPrev()
{
    int cur = cursor;
    int hole = GetHole();

    for (int k = KAMON_SLOT_NUM - 1; k > 0; k--)
    {
        int s = (cur + k) % KAMON_SLOT_NUM;
        if (s == hole)
            continue;
        KAMON_LINK l(s, hole);
        if (HasLink(&l))
        {
            cursor = s;
            return;
        }
    }
}

// FF3 0x0024a5d0.

int KAMON_BOARD::IsMoving()
{
    for (int i = 0; i < KAMON_SLOT_NUM; i++)
    {
        int id = slot[i]->id;
        if (id != KAMON_HOLE && GetPiece(id)->IsMoving())
            return 1;
    }
    return 0;
}

// FF3 0x0024a658.

void KAMON_BOARD::Step()
{
    for (int i = 0; i < KAMON_SLOT_NUM; i++)
    {
        int id = slot[i]->id;
        if (id != KAMON_HOLE)
            GetPiece(id)->Step();
    }
}
