/* ==========================================================================
 *  album/prg/album_zoom_view.h
 *
 *  The album's enlarged photo page, layout side (FF3-only, FF3
 *  0x12e798..0x12f470, between album_load_scr.c and album_save_scr.c): what
 *  phases 53 Album_Zoom and 79 Album_ZoomSort (album_zoom.c) show. The page
 *  is ALBM_KAKUDAI_PHF (+ language; album layout object 1, album_view.c):
 *  "kakudaiA_in" / "kakudaiB_in" by the side shown, its two mount papers
 *  (textures 1000 / 0x3ea) replaced by the album kind's ALBM<k>_DAISI1 /
 *  DAISI2_TM2 loaded over them, the photo (item 0x3fe), its room, date,
 *  score and subjects printed in the kind's ink colour, the protect and
 *  flag3 marks; the sort list's "kakudai_sorton" / "kakudai_sortoff" play on
 *  ALBM_CMN.
 * ======================================================================== */
#ifndef _ALBUM_PRG_ALBUM_ZOOM_VIEW_H
#define _ALBUM_PRG_ALBUM_ZOOM_VIEW_H

#include <stddef.h>                     // offsetof

// One mount paper's load (bss 0x390c60 / 0x390c68).
struct ALBUM_ZOOM_PAPER                 /* 0x08 */
{
    /* 0x00 */ int load_id;             // FileLoadReqEE id while loading, else -1
    /* 0x04 */ int file;                // the file asked for last, -1 none
};
static_assert(sizeof(ALBUM_ZOOM_PAPER) == 0x08, "ALBUM_ZOOM_PAPER is 8 bytes on the EE");
static_assert(offsetof(ALBUM_ZOOM_PAPER, file) == 0x04, "ALBUM_ZOOM_PAPER.file");

// Phase 53's sequence (init_ / end_Album_Zoom) and both phases' frames.
void AlbumZoomViewInit(void);           // FF3 0x0012f270: both paper loads idle, alpha 0
int  AlbumZoomViewIn(void);             // FF3 0x0012f288: 1 once the papers are in and the "in" started
int  AlbumZoomViewIsInEnd(void);        // FF3 0x0012f320
void AlbumZoomViewUpdate(void);         // FF3 0x0012f360: the pages and the photo's marks
int  AlbumZoomViewOut(void);            // FF3 0x0012f390: 1 when the "out" is done
void AlbumZoomViewRelease(void);        // FF3 0x0012f3e0: cancels the paper loads
int  AlbumZoomViewPlaySort(int on);     // FF3 0x0012f3f8: kakudai_sorton / sortoff, 1 when played

#endif /* _ALBUM_PRG_ALBUM_ZOOM_VIEW_H */
