/* ==========================================================================
 *  outgame/autoload.h
 *
 *  Memory-card auto load at boot (FF2 outgame/autoload.c), FF3
 *  0x153d68..0x154720. Phase 14 AutoLoad_Main lives in outgame/boot.c in FF3
 *  and drives these; the message window and the backdrop on screen meanwhile
 *  are outgame/autoload_scr.c.
 * ======================================================================== */
#ifndef _OUTGAME_AUTOLOAD_H
#define _OUTGAME_AUTOLOAD_H

#include <stddef.h>                     // offsetof

// AUTO_LOAD_CTRL::mc_step values autoload_scr.c tests (FF3 0x153e40's
// switch: FF2's nine steps plus two more).
#define AL_STEP_ERROR_CONF  6           // error message, wait for OK
#define AL_STEP_DEF_START   7           // "start without saving?" yes/no
#define AL_STEP_EMPTY_ERROR 9           // no room on the card for the save directory, wait for OK
#define AL_STEP_EMPTY_ASK   10          // no room: "start anyway?" yes/no

// msg_id of the error that leaves the backdrop dark (AutoLoadScrMain).
#define AL_MSG_NO_BACKDROP  0x38

// The work block (FF2 kept it file-local; FF3's autoload_scr.c reads it).
// FF2's AUTO_LOAD_CTRL is {char step, char csr, short move_timer, int msg_id};
// FF3 adds an outer step in front and two frame counters.
struct AUTO_LOAD_CTRL                   /* 0x10, FF3 0x2f6400 */
{
    /* 0x00 */ char  step;              // 0..3: AutoLoadMain; 3 = finished
    /* 0x01 */ char  mc_step;           // the card check / load / message step (AL_STEP_*)
    /* 0x02 */ char  csr;               // yes/no cursor: 0 yes, 1 no (the default)
    /* 0x04 */ int   conf_timer;        // frames the "loaded" message has shown (ends at 150)
    /* 0x08 */ int   msg_id;            // message (0x83, msg_id) the window shows
    /* 0x0c */ int   wait_timer;        // error message hold: 75 PAL / 90 NTSC frames, 0: none
};
static_assert(sizeof(AUTO_LOAD_CTRL) == 0x10, "AUTO_LOAD_CTRL is 0x10 bytes");
static_assert(offsetof(AUTO_LOAD_CTRL, mc_step) == 0x01, "AUTO_LOAD_CTRL.mc_step");
static_assert(offsetof(AUTO_LOAD_CTRL, csr) == 0x02, "AUTO_LOAD_CTRL.csr");
static_assert(offsetof(AUTO_LOAD_CTRL, conf_timer) == 0x04, "AUTO_LOAD_CTRL.conf_timer");
static_assert(offsetof(AUTO_LOAD_CTRL, msg_id) == 0x08, "AUTO_LOAD_CTRL.msg_id");
static_assert(offsetof(AUTO_LOAD_CTRL, wait_timer) == 0x0c, "AUTO_LOAD_CTRL.wait_timer");

extern AUTO_LOAD_CTRL auto_load_ctrl;   // FF3 0x2f6400

void AutoLoadInit(void);                // init_AutoLoad_Main: takes the card (MemoryCardExeInit)
int  AutoLoadMain(void);                // steps on auto_load_ctrl.step; 1 when finished (step 3)
void AutoLoadMcEnd(void);               // end_AutoLoad_Main: MemoryCardEnd

#endif /* _OUTGAME_AUTOLOAD_H */
