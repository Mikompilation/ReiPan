/* ==========================================================================
 *  album/prg/album_disp.h
 *
 *  FF3 0x121cd8..0x1229a0 (FF2 album_disp.c's place in the link order, FF3's
 *  own code): the album edit page's state and the draw helpers that read it.
 *    - the photo the photo decoder holds (album_disp_ctrl, album.c) and the
 *      photo / thumbnail drawers over a side's pixel buffer;
 *    - the info lines of a photo (room, date, score, subjects);
 *    - the album save manifest getters (save_data.c);
 *    - the shown side / photo and the three cursors that move them: the
 *      8x2 photo grid (MenuCsr id 8), the destination grid on the other
 *      side for copy / move (id 0x19), and the sort key list (id 0x1a).
 * ======================================================================== */
#ifndef _ALBUM_PRG_ALBUM_DISP_H
#define _ALBUM_PRG_ALBUM_DISP_H

#include "sdk/scetypes.h"

#define ALBUM_SORT_KEY_NUM  5           // bits of AlbumSortCsrInit's mask

// The page state (sdata 0x372c1c..0x372c2d; album_edit.c and the 0x1240e8 run
// write it too).
extern s_char album_edit_side;                      // sdata 0x372c1c: side under the grid cursor
extern s_char album_dst_side;                       // sdata 0x372c1d: destination side, -1 none
extern s_char album_edit_photo;                     // sdata 0x372c1e: photo under the grid cursor
extern s_char album_dst_photo[2];                   // sdata 0x372c20: destination photo per side, -1 unset
extern s_char album_sort_key;                       // sdata 0x372c22: selected sort key, -1 no list
extern s_char album_sort_list[ALBUM_SORT_KEY_NUM];  // sdata 0x372c28: keys of the list rows, -1 past the end
extern s_char album_sort_key_req;                   // sdata 0x372c2d: key to restore (AlbumSortCsrRestore), -1 none

// Photo [side][no] (nothing when unused). AlbumPhotoDisp decodes the photo
// first when the decoder holds another one.
void AlbumDispSetPhoto(s_char side, s_char no);     // FF3 0x00121cd8
void AlbumPhotoDisp(s_char side, s_char no, int x, int y, int w, int h, u_char alpha, int pri);       // FF3 0x00121d10
void AlbumPhotoThumbDisp(s_char side, s_char no, int x, int y, int w, int h, u_char alpha, int pri);  // FF3 0x00121e10

// Info lines of photo [side][no]; nothing when the slot is unused.
void AlbumPhotoPrintRoom(s_char side, s_char no, int x, int y, int alpha, int pri);             // FF3 0x00121ef0
void AlbumPhotoPrintDate(s_char side, s_char no, int x, int y, int step, int alpha, int pri);   // FF3 0x00121fa8
void AlbumPhotoPrintScore(s_char side, s_char no, int x, int y, u_char alpha, int pri);         // FF3 0x00122258
void AlbumPhotoPrintScoreLabel(s_char side, s_char no, int x, int y, int alpha, int pri);       // FF3 0x00122308
void AlbumPhotoPrintSubject(int idx, s_char side, s_char no, int x, int y, int alpha, int pri); // FF3 0x001223a0

// Save manifest entries (system/mc/dat/save_data.c): *addr = block, returns its size.
int AlbumIngameSaveGet(void **addr);    // FF3 0x00122490: {0x372c00, 0x8}
int AlbumSaveGetInfo(void **addr);      // FF3 0x001224a0: {album_cur_info[cur], 0x10}
int AlbumSaveGetData(void **addr);      // FF3 0x001224e0: {album_file[cur], 0x20c}
int AlbumSaveGetPhoto(void **addr);     // FF3 0x00122520: {album_photo_buf[cur], 0xe8000}

s_char AlbumDispGetCurAlbum(void);      // FF3 0x00122560: the destination side, else the grid's
s_char AlbumDispGetCurPhoto(void);      // FF3 0x00122578: its photo

// The photo grid (MenuCsr 8): side 0, photo = cursor.
void AlbumEditCsrInit(void);                // FF3 0x001225a0
int  AlbumEditCsrUpdate(int lock_side);     // FF3 0x001225f8: moved; a y wrap flips the side unless lock_side
void AlbumEditCsrRelease(void);             // FF3 0x00122670
// The destination grid (MenuCsr 0x19) on the other side.
void AlbumDstCsrInit(void);                 // FF3 0x00122698
int  AlbumDstCsrUpdate(void);               // FF3 0x00122740: moved
void AlbumDstCsrRelease(void);              // FF3 0x001227a0
// The sort key list (MenuCsr 0x1a): one row per set bit of mask.
void AlbumSortCsrInit(u_char mask, s_char key);  // FF3 0x001227c0: cursor on key (-1: keep)
int  AlbumSortCsrUpdate(void);              // FF3 0x001228d8: moved
void AlbumSortCsrRelease(void);             // FF3 0x00122940
void AlbumSortCsrRestore(u_char mask);      // FF3 0x00122968: AlbumSortCsrInit(mask, album_sort_key_req) once

#endif /* _ALBUM_PRG_ALBUM_DISP_H */
