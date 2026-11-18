/* ==========================================================================
 *  outgame/save_scr.h
 *
 *  The memory-card save screen (FF3-only, FF3 0x132118..0x133960): the
 *  SAVELOAD_PHF layout (+ language) of phase 69 Story_SaveGame_Mc
 *  (ingame/story_save_mc.c) over the outgame backdrop. The twin of the load
 *  screen (outgame/save_load_scr.c) over McSaveGame's work (mc_save_wrk,
 *  save_load/prg/game_data_save.h): the same items and ROM tables, its own
 *  steps -- the chosen slot fades out before it is written, the arrows only
 *  show when more than one slot can be chosen, and the load screen's title
 *  is the one hidden.
 * ======================================================================== */
#ifndef _OUTGAME_SAVE_SCR_H
#define _OUTGAME_SAVE_SCR_H

// Phase 69's sequence (ingame/story_save_mc.c).
void SaveScrCreate(int phase);          // FF3 0x001336f8: the layout object, the yes/no window
int  SaveScrIsLoaded(void);             // FF3 0x00133758: 1 when loaded (the fade layer then shows)
void SaveScrIn(void);                   // FF3 0x001337f0: plays "in", hooks and cursors
int  SaveScrIsInEnd(void);              // FF3 0x00133850
void SaveScrMain(void);                 // FF3 0x00133878: per frame
void SaveScrOut(void);                  // FF3 0x001338b0: empty
int  SaveScrOutWait(void);              // FF3 0x001338b8: plays "out"; 1 when done
void SaveScrRelease(void);              // FF3 0x001338e0

// McSaveGame's steps (save_load/prg/game_data_save.c).
int  SaveScrIsSlotFadedOut(void);       // FF3 0x00132388: the chosen slot has faded out (step 9)
int  SaveScrMsgIsOpen(void);            // FF3 0x001336b8: 1 unless the yes/no window is not open

// PC: the mouse. The slot cursor as drawn, and the distance between slots.
int   SaveScrMouseBox(float *x, float *y, float *w, float *h);
float SaveScrMouseStep(void);

#endif /* _OUTGAME_SAVE_SCR_H */
