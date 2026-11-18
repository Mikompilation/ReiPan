/* ==========================================================================
 *  album/prg/album_load_scr.h
 *
 *  The album load screen (FF3-only, FF3 0x12d9f8..0x12e798): what phase 28
 *  (the glue at 0x128c78) shows around AlbumLoadMain (album/prg/album_load.c).
 *  A twin of album_save_scr.c, function for function, over album_load_ctrl:
 *  the ALBM_SENTAKU_PHF layout (+ language; the album's layout object 4,
 *  album/prg/album_view.c) plays "in"/"out"; the slot select window (layout
 *  anim type 7) shows while a MEMORY CARD slot is picked, the yes/no window
 *  (type 0xb) carries album_load_ctrl's message, and the card's five albums
 *  fade in as a list (cursor 0x405) with the chosen album's cover, photo
 *  count, date and card slot.
 * ======================================================================== */
#ifndef _ALBUM_PRG_ALBUM_LOAD_SCR_H
#define _ALBUM_PRG_ALBUM_LOAD_SCR_H

// Phase 28's sequence (0x128c78).
void AlbumLoadScrInit(void);            // FF3 0x0012e5d8: list alpha 0
int  AlbumLoadScrIn(int pic_mode);      // FF3 0x0012e5f0: "in", cursor, windows, hooks; returns 1
int  AlbumLoadScrIsInEnd(void);         // FF3 0x0012e678
void AlbumLoadScrMain(void);            // FF3 0x0012e6a0: per frame before AlbumLoadMain
void AlbumLoadScrClose(void);           // FF3 0x0012e6e8: once AlbumLoadMain is done
int  AlbumLoadScrOutWait(void);         // FF3 0x0012e708: "out"; 1 when it and both windows are done
void AlbumLoadScrRelease(void);         // FF3 0x0012e760

// AlbumLoadMain's pad steps wait for these before reading the pad.
int  AlbumLoadScrListIsIn(void);        // FF3 0x0012dcb8: the album list fully faded in
int  AlbumLoadScrSlotSelIsOpen(void);   // FF3 0x0012e128: no slot window, or it is open
int  AlbumLoadScrMsgIsOpen(void);       // FF3 0x0012e348: no yes/no window, or it is open

// PC: the mouse. The row cursor as drawn, and the distance between rows.
int   AlbumLoadScrMouseBox(float *x, float *y, float *w, float *h);
float AlbumLoadScrMouseStep(void);

#endif /* _ALBUM_PRG_ALBUM_LOAD_SCR_H */
