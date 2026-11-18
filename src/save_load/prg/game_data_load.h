/* ==========================================================================
 *  save_load/prg/game_data_load.h
 *
 *  The memory-card load steps of phase 68 Story_LoadGame_Mc (FF3-only,
 *  FF3 0x1d4678..0x1d5b40): McLoadGameInit / Main / End and the step
 *  machine under them. It reads the play data heads of the five save files,
 *  loads each save's thumbnail and chapter title texture for the load screen
 *  (outgame/save_load_scr.c draws MC_LOAD_WRK), lets the player pick a file,
 *  then loads the system file and the chosen game file. The load twin of
 *  save_load/prg/game_data_save.c (FF2's file of that name only saves).
 * ======================================================================== */
#ifndef _SAVE_LOAD_PRG_GAME_DATA_LOAD_H
#define _SAVE_LOAD_PRG_GAME_DATA_LOAD_H

#include <stddef.h>                     // offsetof

#include "sdk/scetypes.h"

#define SAVE_LOAD_SLOTS     5           // save files on a card

// McLoadGame's work (data 0x3282e0, 0x74 bytes on the EE; McLoadGameInit
// and McLoadGameCtrlInit clear it). layout: host -- tex / chap_tex are
// pointer arrays; the fields up to slot_flg keep their EE offsets
// (EE: tex +0x18, snap_file +0x30, chap_tex +0x48, chap_file +0x60).
struct MC_LOAD_WRK
{
    /* 0x00 */ int    msg;              // message (0x83, msg) of the yes/no window (MLG_MSG_*)
    /* 0x04 */ int    end_timer;        // frames the "loaded" message has been up (step 14)
    /* 0x08 */ u_char mode;             // 0 new, 1 starting, 2 the slot screen (steps run), 3 finished
    /* 0x09 */ u_char step;             // in mode 2: MLG_STEP_*
    /* 0x0a */ s_char slot;             // cursor slot 0..4
    /* 0x0b */ u_char yesno;            // yes/no cursor: 1 no (the reset value), 0 yes
    /* 0x0c */ int    msg_timer;        // error messages: frames before a missing card may change them
    /* 0x10 */ s_char slot_flg[SAVE_LOAD_SLOTS];  // 1 a save, 0 none, -1 bad data
    /* 0x18 */ void  *tex[SAVE_LOAD_SLOTS];       // the slot's map snapshot TIM2 (nullptr: none)
    /* 0x30 */ int    snap_file[SAVE_LOAD_SLOTS]; // its file number; -1 none
    /* 0x48 */ void  *chap_tex[SAVE_LOAD_SLOTS];  // the slot's chapter title TIM2
    /* 0x60 */ int    chap_file[SAVE_LOAD_SLOTS]; // its file number; -1 none
};
static_assert(offsetof(MC_LOAD_WRK, end_timer) == 0x04, "MC_LOAD_WRK.end_timer");
static_assert(offsetof(MC_LOAD_WRK, mode) == 0x08, "MC_LOAD_WRK.mode");
static_assert(offsetof(MC_LOAD_WRK, step) == 0x09, "MC_LOAD_WRK.step");
static_assert(offsetof(MC_LOAD_WRK, slot) == 0x0a, "MC_LOAD_WRK.slot");
static_assert(offsetof(MC_LOAD_WRK, yesno) == 0x0b, "MC_LOAD_WRK.yesno");
static_assert(offsetof(MC_LOAD_WRK, msg_timer) == 0x0c, "MC_LOAD_WRK.msg_timer");
static_assert(offsetof(MC_LOAD_WRK, slot_flg) == 0x10, "MC_LOAD_WRK.slot_flg");

#define MC_LOAD_MODE_SLOTS  2           // MC_LOAD_WRK.mode while the steps run
#define MC_LOAD_MODE_END    3           // McLoadGameMain answers 1

// MC_LOAD_WRK.step (jump table 0x359f50). Each even "init" step falls
// through into the next one in the same frame.
enum MLG_STEP
{
    MLG_STEP_CHECK_INIT      = 0,       // look for the game directory
    MLG_STEP_CHECK_WAIT      = 1,
    MLG_STEP_HEAD_LOAD_INIT  = 2,       // card file 1: the five play data heads
    MLG_STEP_HEAD_LOAD_WAIT  = 3,
    MLG_STEP_SNAP_LOAD_INIT  = 4,       // the saves' thumbnails and chapter titles
    MLG_STEP_SNAP_LOAD_WAIT  = 5,
    MLG_STEP_SELECT          = 6,       // the player picks a slot
    MLG_STEP_ASK             = 7,       // load this file? yes / no
    MLG_STEP_SYS_LOAD_INIT   = 8,       // card file 0: the system file
    MLG_STEP_SYS_LOAD_WAIT   = 9,
    MLG_STEP_GAME_LOAD_INIT  = 10,      // card file slot + 2: the game
    MLG_STEP_GAME_LOAD_WAIT  = 11,
    MLG_STEP_ERROR_INIT      = 12,      // an error message
    MLG_STEP_ERROR_WAIT      = 13,
    MLG_STEP_END_CONF        = 14,      // "loaded"
};

// The load screen's names for the steps it tests.
#define MC_LOAD_STEP_SELECT MLG_STEP_SELECT
#define MC_LOAD_STEP_ASK    MLG_STEP_ASK

extern MC_LOAD_WRK mc_load_wrk;         // 0x3282e0

// Phase 68 (ingame/story_load_mc.c).
void McLoadGameInit(void);              // FF3 0x001d4678: takes the card, clears the work
int  McLoadGameMain(void);              // FF3 0x001d4788: 1 when done (title_start_flg.load 1: a game was loaded)
void McLoadGameEnd(void);               // FF3 0x001d5ac0: frees the buffers, gives the card back

#endif /* _SAVE_LOAD_PRG_GAME_DATA_LOAD_H */
