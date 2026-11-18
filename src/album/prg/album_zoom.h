/* ==========================================================================
 *  album/prg/album_zoom.h
 *
 *  FF3 0x126738..0x126f68 (FF3-only, between album_load.c and album_save.c):
 *  the album's enlarged photo page (Album_Menu's first row, "menu_kakudai")
 *  as three game phases:
 *    - phase  53 Album_Zoom:        the enlarged photo of the side under the
 *                                   cursor (the kakudaiA / B screen,
 *                                   album_zoom_view.c); the photo
 *                                   cursor stays on that side, circle erases,
 *                                   OK protects, square opens the sort list,
 *                                   cancel goes back to Album_Menu;
 *    - phase  79 Album_ZoomSort:    the sort key list over it (MenuCsr 0x4f);
 *    - phase 106 Album_ZoomConfirm: a message window, with a yes / no cursor
 *                                   or alone, linked under Album_Zoom.
 *  Data: sbss 0x372c68..0x372c78.
 * ======================================================================== */
#ifndef _ALBUM_PRG_ALBUM_ZOOM_H
#define _ALBUM_PRG_ALBUM_ZOOM_H

// Phase 53, Album_Zoom
int  init_Album_Zoom(void);             // FF3 0x001267f8
int  end_Album_Zoom(void);              // FF3 0x00126900
void pre_Album_Zoom(void);              // FF3 0x00126930
int  one_Album_Zoom(void);              // FF3 0x00126938
void after_Album_Zoom(void);            // FF3 0x00126b20

// Phase 79, Album_ZoomSort
int  init_Album_ZoomSort(void);         // FF3 0x00126bd0
int  end_Album_ZoomSort(void);          // FF3 0x00126cb8
void pre_Album_ZoomSort(void);          // FF3 0x00126cf0
int  one_Album_ZoomSort(void);          // FF3 0x00126cf8
void after_Album_ZoomSort(void);        // FF3 0x00126de0

// Phase 106, Album_ZoomConfirm
int  init_Album_ZoomConfirm(void);      // FF3 0x00126de8
int  end_Album_ZoomConfirm(void);       // FF3 0x00126e90
int  one_Album_ZoomConfirm(void);       // FF3 0x00126eb8

#endif /* _ALBUM_PRG_ALBUM_ZOOM_H */
