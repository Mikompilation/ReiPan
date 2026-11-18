/* ==========================================================================
 *  album/prg/album_save_scr.h
 *
 *  The album save screen (FF3-only, FF3 0x12f470..0x130210): what phase 27
 *  (one at 0x128ab8) shows around AlbumSaveMain (album/prg/album_save.c).
 *  The ALBM_SENTAKU_PHF layout (+ language; the album's layout object 4,
 *  album/prg/album_view.c) plays "in"/"out"; the slot select window (layout
 *  anim type 7) shows while a MEMORY CARD slot is picked, the yes/no window
 *  (type 0xb) carries album_save_ctrl's message, and the card's five albums
 *  fade in as a list (cursor 0x405) with the chosen album's cover, photo
 *  count, date and card slot. Everything reads album_save_ctrl. Same shape
 *  as outgame/save_load_scr.c.
 * ======================================================================== */
#ifndef _ALBUM_PRG_ALBUM_SAVE_SCR_H
#define _ALBUM_PRG_ALBUM_SAVE_SCR_H

// Phase 27's sequence (0x128ab8).
void AlbumSaveScrInit(void);            // FF3 0x00130050: list alpha 0 (after AlbumSaveCtrlInit)
int  AlbumSaveScrIn(int pic_mode);      // FF3 0x00130068: "in", cursor, windows, hooks; returns 1
int  AlbumSaveScrIsInEnd(void);         // FF3 0x001300f0
void AlbumSaveScrMain(void);            // FF3 0x00130118: per frame before AlbumSaveMain
void AlbumSaveScrClose(void);           // FF3 0x00130160: once AlbumSaveMain is done
int  AlbumSaveScrOutWait(void);         // FF3 0x00130180: "out"; 1 when it and both windows are done
void AlbumSaveScrRelease(void);         // FF3 0x001301d8

// AlbumSaveMain's pad steps wait for these before reading the pad.
int  AlbumSaveScrListIsIn(void);        // FF3 0x0012f710: the album list fully faded in
int  AlbumSaveScrSlotSelIsOpen(void);   // FF3 0x0012fb80: no slot window, or it is open
int  AlbumSaveScrMsgIsOpen(void);       // FF3 0x0012fdc0: no yes/no window, or it is open

// PC: the mouse. The row cursor as drawn, and the distance between rows.
int   AlbumSaveScrMouseBox(float *x, float *y, float *w, float *h);
float AlbumSaveScrMouseStep(void);

#endif /* _ALBUM_PRG_ALBUM_SAVE_SCR_H */
