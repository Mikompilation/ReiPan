/* ==========================================================================
 *  album/prg/scr_play.c
 *
 *  The u_short opcode-stream player AnimeCodeManager (see scr_play.h). FF3
 *  0x128f38..0x129968 and the out-of-line stack copy 0x2aca20; the virtual
 *  destructor 0x2ac9e8 / 0x2ac9f8 and the empty Reset 0x2aca18 are inline in
 *  the header. Complete.
 * ======================================================================== */
#include "album/prg/scr_play.h"

#include "common/reipan_stub.h"

// ──────────────────────────────────────────────────────────────────────
// Opcode field tables [field count][field] (data 0x34e248 / 0x34e268).
// ──────────────────────────────────────────────────────────────────────

const u_short scr_op_mask[4][4] = {                     // data 0x34e248
    { 0x1ff, 0,   0,   0   },                           // one 9-bit field
    { 0x1f,  0xf, 0,   0   },                           // 5 + 4
    { 7,     7,   7,   0   },                           // 3 + 3 + 3
    { 7,     3,   3,   3   },                           // 3 + 2 + 2 + 2
};

const s_char scr_op_shift[4][4] = {                     // data 0x34e268
    { 0, 0, 0, 0 },
    { 4, 0, 0, 0 },
    { 6, 3, 0, 0 },
    { 6, 4, 2, 0 },
};

// ──────────────────────────────────────────────────────────────────────
// Empty cursor, no handler or scripts, script number -1.
// FF3 0x00129178 (ScrPlayCtor).

AnimeCodeManager::AnimeCodeManager()
{
    cur.pc    = nullptr;
    cur.owner = nullptr;
    cur.end   = 1;
    for (int i = 0; i < 3; i++) {
        cur.stk[i].pc    = nullptr;
        cur.stk[i].count = 0;
    }
    cur.sp  = cur.stk;
    scr_no  = -1;
    hdl     = nullptr;
    scr_tbl = nullptr;
}

// ──────────────────────────────────────────────────────────────────────
// The cursor
// ──────────────────────────────────────────────────────────────────────

// std::copy of loop-stack entries, out of line in FF3's weak-function area.
// FF3 0x002aca20 (ScrStkCopy)

static SCR_STK *ScrStkCopy(const SCR_STK *first, const SCR_STK *last, SCR_STK *dst)
{
    for (int n = (int)(last - first); n > 0; n--) {
        dst->pc    = first->pc;
        dst->count = first->count;
        first++;
        dst++;
    }
    return dst;
}

// The cursor assignment FF3 inlines in ScrPlayStart and ScrPlayGetLength:
// the used stack entries are copied and sp rebased onto this stack.
// (inlined in FF3 0x00129248 / 0x001295d8)

SCR_CUR &SCR_CUR::operator=(const SCR_CUR &c)
{
    if (this != &c) {
        pc  = c.pc;
        end = c.end;
        if (c.stk != stk) {
            ScrStkCopy(c.stk, c.sp, stk);
            sp = stk + (c.sp - c.stk);
        }
        owner = c.owner;
    }
    return *this;
}

// Settles the cursor on the next playable opcode: the end flag follows
// *pc == 0, and type-0 opcodes are loop control, consumed here. Field 0 == 2
// pushes (this opcode, field 1 or 0) while the stack has room; field 0 == 1
// returns to the loop's begin while passes are left (count 0: for ever),
// else pops, and resets the owner either way (the motion script's clock goes
// back to 0 at each pass). Other type-0 opcodes are skipped. Stops on the end
// or an opcode of type >= 1. FF3 recurses (a tail call); a loop here.
// FF3 0x00128f38 (ScrCurNormalize)
// callcheck: ScrCurNormalize (FF3's self tail call, the loop)

void SCR_CUR::Normalize(void)
{
    for (;;) {
        u_short op = *pc;

        end = (op == 0);
        if (op == 0 || ScrOpType(op) != 0)
            return;

        int ctrl = ScrOpField(op, 0);
        if (ctrl == 2) {
            int count = 0;
            if (((op >> 9) & 3) != 0)
                count = ScrOpField(op, 1);
            if ((u_int)(sp - stk) < 3) {
                sp->pc    = pc;
                sp->count = count;
                sp++;
            }
        } else if (ctrl == 1) {
            // PC guard: a loop end with an empty stack reads the entry
            // before stk in the ROM (the end flag and owner as pc / count);
            // here it only resets.
            if (sp != stk) {
                SCR_STK *top = sp - 1;
                if (top->count == 0 || --top->count != 0)
                    pc = top->pc;
                else
                    sp--;
            }
            if (owner != nullptr)
                owner->Reset();         // FF3: vtable +0x0c
        }
        pc++;
    }
}

// ──────────────────────────────────────────────────────────────────────
// The player
// ──────────────────────────────────────────────────────────────────────

// Starts script no: a fresh cursor (owned by this player) settles on the
// first playable opcode and becomes the player's, then skip frame opcodes
// are passed (commands on the way are skipped, not run; at the end the
// count just runs out). The commands before the next frame opcode run, and
// the handler gets that frame opcode. Nothing when the table has no script.
// FF3 0x00129248 (ScrPlayStart)
// callcheck: ScrStkCopy (inside the inlined SCR_CUR::operator=)

void AnimeCodeManager::Start(int no, int skip)
{
    SCR_CUR c;

    c.pc = scr_tbl[no];
    if (c.pc == nullptr)
        return;

    scr_no  = no;
    for (int i = 0; i < 3; i++) {
        c.stk[i].pc    = nullptr;
        c.stk[i].count = 0;
    }
    c.owner = this;
    c.sp    = c.stk;
    c.Normalize();
    cur = c;

    for (int i = 0; i < skip; ) {
        if (!cur.AtEnd()) {
            if (ScrOpType(*cur.pc) >= 2) {
                cur.pc++;
                cur.Normalize();
                continue;
            }
            cur.pc++;
            cur.Normalize();
        }
        i++;
    }

    if (RunCommands()) {
        u_short op = *cur.pc;
        hdl->Frame(&op);                // FF3: vtable +0x08, no null check
    }
}

// Hands the command opcodes (type >= 2) at the cursor to the handler and
// steps past them, up to a frame opcode or the end. 1 when the cursor stands
// on a frame opcode.
// FF3 0x00129490 (ScrPlayRunCommands)

int AnimeCodeManager::RunCommands(void)
{
    while (!cur.AtEnd() && ScrOpType(*cur.pc) >= 2) {
        if (hdl != nullptr) {
            u_short op = *cur.pc;
            hdl->Command(&op);          // FF3: vtable +0x0c
        }
        cur.pc++;
        cur.Normalize();
    }
    return !cur.AtEnd();
}

// Script no's length in frames: the handler's frame count (Query, the
// motion's frames) of every type-1 opcode, scanned straight to the end
// (loops are not followed; owner null, so nothing is reset), minus one per
// opcode, plus 1. 0 when the table has no script.
// FF3 0x001295d8 (ScrPlayGetLength)
// callcheck: ScrStkCopy (inside the inlined SCR_CUR::operator=)

int AnimeCodeManager::GetLength(int no)
{
    SCR_CUR it, c;
    int     sum = 0, n = 0;

    it.pc = scr_tbl[no];
    if (it.pc == nullptr)
        return 0;

    c.pc  = nullptr;
    c.end = 1;
    c.owner = nullptr;
    for (int i = 0; i < 3; i++) {
        c.stk[i].pc  = nullptr;
        c.stk[i].count = 0;
        it.stk[i].pc  = nullptr;
        it.stk[i].count = 0;
    }
    c.sp     = c.stk;
    it.owner = nullptr;
    it.sp    = it.stk;
    it.Normalize();
    c = it;

    for (;;) {
        u_short op = *c.pc;
        if (ScrOpType(op) == 1) {
            n++;
            sum += hdl->Query(&op);     // FF3: vtable +0x10, no null check
        }
        if (!c.AtEnd()) {
            c.pc++;
            c.end = (*c.pc == 0);
        }
        if (c.AtEnd())
            break;
    }
    return sum - n + 1;
}

// Steps to the next frame opcode once the handler says the current one is
// done (CanStep): the commands on the way run and the handler gets the new
// frame opcode. 0 at the end, without a handler, while the handler holds, or
// when the script ran out.
// FF3 0x00129800 (ScrPlayStep)

int AnimeCodeManager::Step(void)
{
    if (cur.AtEnd() || hdl == nullptr)
        return 0;
    if (hdl->CanStep() == 0)            // FF3: vtable +0x14
        return 0;

    cur.pc++;
    cur.Normalize();

    int ok = 0;
    if (!cur.AtEnd())
        ok = RunCommands();
    if (ok) {
        u_short op = *cur.pc;
        hdl->Frame(&op);                // FF3: vtable +0x08
    }
    return ok;
}
