/* ==========================================================================
 *  album/prg/scr_play.h
 *
 *  A u_short opcode-stream player (FF3-only; FF3 0x128f38..0x129968 + 0x2aca20, between
 *  album_mem.c and album_save.c in link order; file name invented). A script
 *  is an array of u_short opcodes: bits 15..11 the type (< 2: a frame opcode
 *  the player stops on, >= 2: a command run on the way), bits 10..9 the
 *  number of extra fields, bits 8..0 the fields (scr_op_shift/scr_op_mask).
 *  The player hands each opcode to a handler object (AnimeCodeInterpretor). Users so far:
 *  the model motion script (AnimeCodeManagerGame, ingame/ingame_wrk.c), where
 *  a type-1 opcode starts motion field 0 and its frames run until the motion
 *  ends (CanStep). Type-0 opcodes are loop control (field 0: 2 begins a loop
 *  of field 1 passes, 1 ends it); 0 ends the script.
 * ======================================================================== */
#ifndef _ALBUM_PRG_SCR_PLAY_H
#define _ALBUM_PRG_SCR_PLAY_H

#include <stddef.h>                 // offsetof
#include "sdk/scetypes.h"

// Opcode field tables, indexed [field count][field]: field i of `op` is
// (op >> scr_op_shift[n][i]) & scr_op_mask[n][i], n = (op >> 9) & 3.
extern const u_short scr_op_mask[4][4];         // data 0x34e248
extern const s_char  scr_op_shift[4][4];        // data 0x34e268

inline int ScrOpType(u_short op)  { return op >> 11; }
inline int ScrOpField(u_short op, int i)
{
    int n = (op >> 9) & 3;
    return (op >> scr_op_shift[n][i]) & scr_op_mask[n][i];
}

// FF3 RTTI name AnimeCodeInterpretor (FF2 anicode.c lineage; functions still ScrHdl*/Scr*).
// The handler interface: vtable 0x3415b0, four pure virtuals after the two
// destructor slots (0x002ad4a0 / deleting 0x002ad4b0).
// layout: host -- only the vptr (4 bytes on the EE, 8 here).
class AnimeCodeInterpretor
{
public:
    virtual ~AnimeCodeInterpretor() {}
    virtual void Frame(const u_short *op) = 0;      // EE vtable +0x08: the opcode the player stops on
    virtual void Command(const u_short *op) = 0;    // EE vtable +0x0c: opcodes of type >= 2
    virtual int  Query(const u_short *op) = 0;      // EE vtable +0x10
    virtual int  CanStep(void) = 0;                 // EE vtable +0x14: ScrPlayStep advances only when true
};
static_assert(sizeof(AnimeCodeInterpretor) == sizeof(void *), "AnimeCodeInterpretor is only its vptr");

// One entry of the cursor's loop stack: the loop-begin opcode and the
// passes left (0: for ever). A type-0 opcode with field 0 == 2 pushes one
// (count = field 1), field 0 == 1 ends the loop (ScrCurNormalize).
// layout: host -- pc is an EE pointer (8 bytes on the EE, 16 here).
struct SCR_STK
{
    /* 0x00 */ const u_short *pc;       // the loop-begin opcode (the cursor returns past it)
    /* 0x04 */ int            count;    // passes left; 0 loops for ever
};
static_assert(offsetof(SCR_STK, count) == sizeof(void *), "SCR_STK.count");

class AnimeCodeManager;

// The cursor at AnimeCodeManager +4 (0x28 bytes on the EE). FF3 treats it as
// an iterator: ScrPlayStart / ScrPlayGetLength build one on the stack, run
// ScrCurNormalize on it and assign it to the player's (the assignment is
// inlined: pc, end, the used stack entries through ScrStkCopy, owner), and
// compare cursors with an end cursor {nullptr, end 1}. Two cursors are equal
// when pc and end match or both are at the end, so "at the end" is the flag.
// layout: host -- pc, owner and sp are EE pointers; EE offsets below.
struct SCR_CUR                      /* 0x28 on the EE */
{
    /* 0x00 */ const u_short    *pc;    // current opcode
    /* 0x04 */ int               end;   // 1: *pc == 0 (also after construction; GameAnimeModel::IsAnimeEnd)
    /* 0x08 */ AnimeCodeManager *owner; // reset at each loop end; null for a scan (ScrPlayGetLength)
    /* 0x0c */ SCR_STK           stk[3];
    /* 0x24 */ SCR_STK          *sp;    // stack top (stk when empty)

    SCR_CUR &operator=(const SCR_CUR &c);   // inlined in FF3 0x00129248 / 0x001295d8
    void Normalize(void);           // FF3 0x00128f38 (ScrCurNormalize)
    int  AtEnd(void) const { return end != 0; }
};
static_assert(offsetof(SCR_CUR, end) == sizeof(void *), "SCR_CUR.end");
static_assert(offsetof(SCR_CUR, stk) == 3 * sizeof(void *), "SCR_CUR.stk");
static_assert(offsetof(SCR_CUR, sp) == offsetof(SCR_CUR, stk) + 3 * sizeof(SCR_STK), "SCR_CUR.sp");

// FF3 RTTI name AnimeCodeManager (functions still ScrPlay*).
// The player: constructor FF3 0x00129178, vtable 0x3412f0 (4 slots).
// layout: host -- the vptr and the pointers are 4 bytes on the EE, 8 here;
// EE offsets in the comments (0x38 bytes on the EE).
class AnimeCodeManager
{
public:
    AnimeCodeManager();                         // FF3 0x00129178 (ScrPlayCtor)
    virtual ~AnimeCodeManager() {}              // FF3 0x002ac9e8 (deleting: 0x002ac9f8)
    virtual int  Step(void);            // FF3 0x00129800 (ScrPlayStep)
    virtual void Reset(void) {}         // FF3 0x002aca18 (empty)

    void Start(int no, int skip);       // FF3 0x00129248 (ScrPlayStart): skip frame opcodes, then the handler's Frame
    int  RunCommands(void);             // FF3 0x00129490 (ScrPlayRunCommands): 1 when standing on a frame opcode
    int  GetLength(int no);             // FF3 0x001295d8 (ScrPlayGetLength): script no's length in frames

    /* 0x04 */ SCR_CUR         cur;
    /* 0x2c */ AnimeCodeInterpretor        *hdl;     // set by the owner (0x1d3ad0)
    /* 0x30 */ const u_short **scr_tbl; // scripts by number
    /* 0x34 */ int             scr_no;  // -1 after construction
};
static_assert(offsetof(AnimeCodeManager, cur) == sizeof(void *), "AnimeCodeManager.cur");

#endif /* _ALBUM_PRG_SCR_PLAY_H */
