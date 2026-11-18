/* ==========================================================================
 *  outgame/save_load_scr.h
 *
 *  The memory-card load screen (FF3-only, FF3 0x130950..0x132118): the
 *  SAVELOAD_PHF layout (+ language) of phase 68 Story_LoadGame_Mc
 *  (ingame/story_load_mc.c) over the outgame backdrop. Five save slots side
 *  by side, each with its thumbnail and texts fading in once McLoadGame has
 *  read it, the cursor and arrows on the chosen slot and the yes/no window
 *  with McLoadGame's message. The save screen (SaveScr*, 0x132118..) is its
 *  twin over McSaveGame's work.
 * ======================================================================== */
#ifndef _OUTGAME_SAVE_LOAD_SCR_H
#define _OUTGAME_SAVE_LOAD_SCR_H

#include "sdk/scetypes.h"

#include "save_load/prg/game_data_load.h"  // MC_LOAD_WRK, mc_load_wrk, SAVE_LOAD_SLOTS

// Phase 68's sequence (ingame/story_load_mc.c).
void SaveLoadScrCreate(int phase);      // FF3 0x00131eb8: the layout object, the yes/no window
int  SaveLoadScrIsLoaded(void);         // FF3 0x00131f18: 1 when loaded (the fade layer then shows)
void SaveLoadScrIn(void);               // FF3 0x00131fb0: plays "in", hooks and cursors
int  SaveLoadScrIsInEnd(void);          // FF3 0x00132010
void SaveLoadScrMain(void);             // FF3 0x00132038: per frame
int  SaveLoadScrOutWait(void);          // FF3 0x00132070: plays "out"; 1 when done
void SaveLoadScrRelease(void);          // FF3 0x00132098

int  SaveLoadScrMsgIsOpen(void);        // FF3 0x00131e78: 1 unless the yes/no window is not open

// PC: the mouse. The slot cursor as drawn, and the distance between slots.
int   SaveLoadScrMouseBox(float *x, float *y, float *w, float *h);
float SaveLoadScrMouseStep(void);

#endif /* _OUTGAME_SAVE_LOAD_SCR_H */
