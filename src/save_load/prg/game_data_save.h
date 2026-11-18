/* ==========================================================================
 *  save_load/prg/game_data_save.h
 *
 *  The memory-card save of phase 69 (ingame/story_save_mc.c, FF3
 *  0x1d5b40..0x1d7a58): McSaveGameInit / Main and GameDataSaveEnd over the
 *  step machine (MGS_STEP_*). It reads the play data heads, shows the
 *  saves' thumbnails, lets the player pick a slot, then writes the system
 *  file, the game file and the heads; a card without the game directory,
 *  with a damaged one or unformatted is offered a new directory / format.
 *  FF2's game_data_save.c reshaped into McLoadGame's twin.
 * ======================================================================== */
#ifndef _SAVE_LOAD_PRG_GAME_DATA_SAVE_H
#define _SAVE_LOAD_PRG_GAME_DATA_SAVE_H

#include <stddef.h>                     // offsetof

#include "sdk/scetypes.h"

#define MC_SAVE_SLOTS       5           // save files on a card

// McSaveGame's work (data 0x328358): the save steps here drive it, the save
// screen outgame/save_scr.c draws it. Twin of MC_LOAD_WRK
// (save_load/prg/game_data_load.h) with slot_num / save_cnt and a format
// timer. layout: host -- tex / chap_tex are pointer arrays; the fields up
// to slot_flg keep their EE offsets (EE: tex +0x20, snap_file +0x38,
// chap_tex +0x50, chap_file +0x68).
struct MC_SAVE_WRK
{
    /* 0x00 */ int    msg;              // message (0x83, msg) of the yes/no window (MGS_MSG_* in the .c)
    /* 0x04 */ int    fmt_end_timer;    // frames "formatted" has been up (steps 33/34)
    /* 0x08 */ u_char mode;             // 0 new, 1 starting, 2 the slot screen (steps run), 3 finished
    /* 0x09 */ u_char step;             // in mode 2: MGS_STEP_*
    /* 0x0a */ s_char slot;             // cursor slot 0..slot_num-1
    /* 0x0b */ s_char slot_num;         // selectable slots: saves + 1, at most 5
    /* 0x0c */ u_char yesno;            // yes/no cursor: 1 no (the reset value), 0 yes
    /* 0x0d */ s_char save_cnt;         // card files written so far (of the three a save writes)
    /* 0x10 */ int    msg_timer;        // error messages: frames before a missing card may change them
    /* 0x14 */ s_char slot_flg[MC_SAVE_SLOTS];  // 1 a save, 0 none, -1 a write failed
    /* 0x19 */ u_char _019[7];
    /* 0x20 */ void  *tex[MC_SAVE_SLOTS];       // the slot's map snapshot TIM2 (nullptr: none)
    /* 0x34 */ int    _034;
    /* 0x38 */ int    snap_file[MC_SAVE_SLOTS]; // the thumbnail's file number; -1 none
    /* 0x4c */ int    _04c;
    /* 0x50 */ void  *chap_tex[MC_SAVE_SLOTS];  // the slot's chapter title TIM2
    /* 0x64 */ int    _064;
    /* 0x68 */ int    chap_file[MC_SAVE_SLOTS]; // its file number; -1 none
};
static_assert(offsetof(MC_SAVE_WRK, fmt_end_timer) == 0x04, "MC_SAVE_WRK.fmt_end_timer");
static_assert(offsetof(MC_SAVE_WRK, mode) == 0x08, "MC_SAVE_WRK.mode");
static_assert(offsetof(MC_SAVE_WRK, step) == 0x09, "MC_SAVE_WRK.step");
static_assert(offsetof(MC_SAVE_WRK, slot) == 0x0a, "MC_SAVE_WRK.slot");
static_assert(offsetof(MC_SAVE_WRK, slot_num) == 0x0b, "MC_SAVE_WRK.slot_num");
static_assert(offsetof(MC_SAVE_WRK, yesno) == 0x0c, "MC_SAVE_WRK.yesno");
static_assert(offsetof(MC_SAVE_WRK, save_cnt) == 0x0d, "MC_SAVE_WRK.save_cnt");
static_assert(offsetof(MC_SAVE_WRK, msg_timer) == 0x10, "MC_SAVE_WRK.msg_timer");
static_assert(offsetof(MC_SAVE_WRK, slot_flg) == 0x14, "MC_SAVE_WRK.slot_flg");

#define MC_SAVE_MODE_SLOTS  2           // MC_SAVE_WRK.mode while the steps run
#define MC_SAVE_MODE_END    3           // McSaveGameMain answers 1

// MC_SAVE_WRK.step (GameDataSaveMcMain 0x1d5cd0). Each "init" step falls
// through into the next one in the same frame. FF2's 31 steps plus the
// slot fade-out (8/9) and the reload of the written slot's textures
// (16/17); FF3 has no separate format confirmation init/wait pair.
enum MGS_STEP
{
    MGS_STEP_CHECK_INIT        = 0,     // look for the game directory
    MGS_STEP_CHECK_WAIT        = 1,
    MGS_STEP_HEAD_LOAD_INIT    = 2,     // card file 1: the five play data heads
    MGS_STEP_HEAD_LOAD_WAIT    = 3,
    MGS_STEP_SNAP_LOAD_INIT    = 4,     // the saves' thumbnails and chapter titles
    MGS_STEP_SNAP_LOAD_WAIT    = 5,
    MGS_STEP_SELECT            = 6,     // the player picks a slot
    MGS_STEP_ASK               = 7,     // save here (overwrite)? yes / no
    MGS_STEP_FADE_INIT         = 8,     // the chosen slot fades out
    MGS_STEP_FADE_WAIT         = 9,
    MGS_STEP_SYS_LOAD_INIT     = 10,    // card file 0: the system file (merged into the game's)
    MGS_STEP_SYS_LOAD_WAIT     = 11,
    MGS_STEP_SAVE_INIT         = 12,    // write system, game (slot + 2), heads
    MGS_STEP_SAVE_WAIT         = 13,
    MGS_STEP_ERROR_INIT        = 14,    // an error message
    MGS_STEP_ERROR_WAIT        = 15,
    MGS_STEP_RELOAD_INIT       = 16,    // the written slot's textures again
    MGS_STEP_RELOAD_WAIT       = 17,
    MGS_STEP_END_CONF          = 18,    // "saved"
    MGS_STEP_REMAKE_CONF_INIT  = 19,    // damaged directory: make it again?
    MGS_STEP_REMAKE_CONF_WAIT  = 20,
    MGS_STEP_DIR_DEL_INIT      = 21,    // delete the damaged directory
    MGS_STEP_DIR_DEL_WAIT      = 22,
    MGS_STEP_NEW_CONF_INIT     = 23,    // no game directory: make one?
    MGS_STEP_NEW_CONF_WAIT     = 24,
    MGS_STEP_NEW_MAKE_INIT     = 25,    // make the directory
    MGS_STEP_NEW_MAKE_WAIT     = 26,
    MGS_STEP_NEW_SAVE_INIT     = 27,    // write the first save (slot 0)
    MGS_STEP_NEW_SAVE_WAIT     = 28,
    MGS_STEP_FORMAT_CONF_INIT  = 29,    // unformatted card: format it?
    MGS_STEP_FORMAT_CONF_WAIT  = 30,
    MGS_STEP_FORMAT_INIT       = 31,
    MGS_STEP_FORMAT_WAIT       = 32,
    MGS_STEP_FORMAT_END_INIT   = 33,    // "formatted" for 60 frames, then the directory
    MGS_STEP_FORMAT_END_WAIT   = 34,
};

// The save screen's names for the steps it tests.
#define MC_SAVE_STEP_SELECT MGS_STEP_SELECT
#define MC_SAVE_STEP_FADE   MGS_STEP_FADE_WAIT

extern MC_SAVE_WRK mc_save_wrk;         // 0x328358

// Phase 69 (ingame/story_save_mc.c).
void McSaveGameInit(void);              // FF3 0x001d5b40: takes the card, clears the work
int  McSaveGameMain(void);              // FF3 0x001d5c50: 1 when done

void GameDataSaveEnd(void);             // FF3 0x001d79e0: frees the buffers, gives the card back

#endif /* _SAVE_LOAD_PRG_GAME_DATA_SAVE_H */
