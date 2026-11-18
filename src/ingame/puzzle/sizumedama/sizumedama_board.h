/* ==========================================================================
 *  ingame/puzzle/sizumedama/sizumedama_board.h
 *
 *  The sizumedama (stone pedestal) puzzle's board: four pedestals in a row
 *  and four stones (R sphere_A..D), each stone holding a voice clip. The
 *  player puts the stones on the pedestals; the board is solved when every
 *  pedestal holds the stone the definition asks for -- the clips then run
 *  in order (PAZZLE_SHIZUME_01..04) -- and the whole recording
 *  (PAZZLE_SHIZUME_STR) plays with its subtitles. A pedestal's stone can be
 *  played back on its own, its subtitle line shown under it.
 *  FF3 0x24ca90..0x24cf18, FF3's own code (FF2 has no such puzzle).
 * ======================================================================== */
#ifndef _INGAME_PUZZLE_SIZUMEDAMA_SIZUMEDAMA_BOARD_H
#define _INGAME_PUZZLE_SIZUMEDAMA_SIZUMEDAMA_BOARD_H

#include <stddef.h>                     // offsetof

#define SIZUME_PEDESTAL_NUM     4
#define SIZUME_STONE_NUM        4

// A stone's voice (and the clear recording's): the stone's name and
// subtitle lines in message group 0x78, -1 none; the stream file (its
// header file is file + 1).
struct SIZUME_VOICE                     /* 0xc */
{
    /* 0x0 */ int name_msg;             // the stone's name (spherename_M)
    /* 0x4 */ int line_msg;             // its subtitle line (jimaku_M)
    /* 0x8 */ int file;                 // the stream, < 0 none
};
static_assert(sizeof(SIZUME_VOICE) == 0xc, "SIZUME_VOICE is 0xc bytes");

// One board as the ROM defines it (data 0x361ec8, 0x5c each; SizumeGetBoardDef).
struct SIZUME_BOARD_DEF                 /* 0x5c */
{
    /* 0x00 */ int          init[SIZUME_PEDESTAL_NUM];      // the stone on each pedestal at the start, -1 none
    /* 0x10 */ int          answer[SIZUME_PEDESTAL_NUM];    // the stone each pedestal must hold
    /* 0x20 */ SIZUME_VOICE stone[SIZUME_STONE_NUM];
    /* 0x50 */ SIZUME_VOICE clear;                          // played once solved
};
static_assert(sizeof(SIZUME_BOARD_DEF) == 0x5c, "SIZUME_BOARD_DEF is 0x5c bytes");
static_assert(offsetof(SIZUME_BOARD_DEF, answer) == 0x10, "SIZUME_BOARD_DEF.answer");
static_assert(offsetof(SIZUME_BOARD_DEF, stone) == 0x20, "SIZUME_BOARD_DEF.stone");
static_assert(offsetof(SIZUME_BOARD_DEF, clear) == 0x50, "SIZUME_BOARD_DEF.clear");

// operator new 0x38 on the EE; no destructor.
// layout: host -- def is a pointer (8 bytes); EE offsets below.
class SIZUME_BOARD
{
public:
    SIZUME_BOARD(const SIZUME_BOARD_DEF *def);  // FF3 0x0024cad8 (copy 0x0024ca90)

    int  IsSolved();                    // FF3 0x0024cbf0: every pedestal holds its answer (sets solved)
    int  Put(int pedestal, int stone);  // FF3 0x0024cc38: 1 when the pedestal was empty and the stone free
    int  Take(int pedestal);            // FF3 0x0024cc78: 1 when the pedestal held a stone
    int  CountFree();                   // FF3 0x0024ccb8: stones not on a pedestal
    const SIZUME_VOICE *GetVoice();     // FF3 0x0024cce8: solved: clear, else the cursor pedestal's stone (nullptr empty)
    void PlayVoice();                   // FF3 0x0024cd50
    int  IsPlaying();                   // FF3 0x0024ce20
    void StopVoice();                   // FF3 0x0024ce68
    int  GetLine();                     // FF3 0x0024cec0: GetVoice's line_msg, -1 none
    int  GetStoneName(int stone);       // FF3 0x0024cef0: -1 for stone -1

    /* 0x00 */ int slot[SIZUME_PEDESTAL_NUM];   // the stone on each pedestal, -1 none
    /* 0x10 */ int used[SIZUME_STONE_NUM];      // the stone is on a pedestal
    /* 0x20 */ const SIZUME_BOARD_DEF *def;
    /* 0x24 */ int cursor;                      // the pedestal under cursor 100
    /* 0x28 */ int stone;                       // the stone under cursor 0x65, -1 none
    /* 0x2c */ int voice;                       // StreamAutoPlay id, 1 for the clear subtitle, -1 none
    /* 0x30 */ int solved;                      // set by IsSolved
    /* 0x34 */ int show_buttons;                // the button guide (SizumeDrawButtons)
};
static_assert(offsetof(SIZUME_BOARD, used) == 0x10, "SIZUME_BOARD.used");
static_assert(offsetof(SIZUME_BOARD, def) == 0x20, "SIZUME_BOARD.def");
static_assert(offsetof(SIZUME_BOARD, cursor) == 0x20 + sizeof(void *), "SIZUME_BOARD.cursor");
static_assert(offsetof(SIZUME_BOARD, show_buttons) == 0x30 + sizeof(void *), "SIZUME_BOARD.show_buttons");

#endif /* _INGAME_PUZZLE_SIZUMEDAMA_SIZUMEDAMA_BOARD_H */
