/* ==========================================================================
 *  album/prg/album_phase.h
 *
 *  FF3 0x128a58..0x128e30 (FF3-only, between album_save.c and album_mem.c):
 *  the album's memory-card phases, 27 Album_Save and 28 Album_Load, linked
 *  under Album_Menu (rows Save / Load) or Album_Open (Load). Each one runs
 *  its screen (album_save_scr.c / album_load_scr.c) around the card machine
 *  (album_save.c / album_load.c) and goes back to the phase that linked it.
 *  Data: sdata 0x36b938 (the load result), sbss 0x372c80 (the return
 *  phase), 0x372c84 (the step).
 * ======================================================================== */
#ifndef _ALBUM_PRG_ALBUM_PHASE_H
#define _ALBUM_PRG_ALBUM_PHASE_H

// The card load's result: 1 when phase 28 loaded an album. Album_Open clears
// it before linking phase 28 and reads it when the pages come back.
void AlbumLoadResultSet(int loaded);    // FF3 0x00128a58
int  AlbumLoadResultGet(void);          // FF3 0x00128a60

// Phase 27, Album_Save
int  init_Album_Save(void);             // FF3 0x00128a68
int  end_Album_Save(void);              // FF3 0x00128aa8
int  one_Album_Save(void);              // FF3 0x00128ab8

// Phase 28, Album_Load
int  init_Album_Load(void);             // FF3 0x00128c28
int  end_Album_Load(void);              // FF3 0x00128c68
int  one_Album_Load(void);              // FF3 0x00128c78

#endif /* _ALBUM_PRG_ALBUM_PHASE_H */
