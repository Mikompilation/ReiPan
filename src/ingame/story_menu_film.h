/* ==========================================================================
 *  ingame/story_menu_film.h
 *
 *  The film menu, phase 60 StoryMenu_Film (a child of Story_Menu 37, back
 *  to 57 StoryMenu_Top): the photos in the camera's album, 8 x 2 small
 *  pictures and the one under the cursor large with its room, date, score
 *  and subjects; protect / delete a photo and sort the album (time, score,
 *  protect; OK on the same row again flips the order). Layouts PHOTO_PHF
 *  and PHOTO_MENU_PHF (the sort menu), + GetLanguage().
 *  FF3 0x22ec88..0x230a38; FF3-only file, named like the other
 *  story_menu_*.c phases.
 * ======================================================================== */
#ifndef _INGAME_STORY_MENU_FILM_H
#define _INGAME_STORY_MENU_FILM_H

#include "sdk/scetypes.h"

// One cursor's layout (table at bss 0x3f7788, 2 records, filled by the
// file's static initializer 0x2308f0): cursor 0 the photo grid, cursor 1
// the sort menu.
struct FILM_CSR_CFG                     /* 0x10 */
{
    /* 0x00 */ short w;                 // columns (MenuCsrObjInit's w)
    /* 0x02 */ short h;                 // rows
    /* 0x04 */ short dx;                // pixels between columns
    /* 0x06 */ short dy;                // pixels between rows
    /* 0x08 */ int   item;              // the cursor sprite's item id
    /* 0x0c */ int   file;              // its layout file, before + GetLanguage()
};
static_assert(sizeof(FILM_CSR_CFG) == 0x10, "FILM_CSR_CFG is 0x10 bytes");
static_assert(offsetof(FILM_CSR_CFG, dy) == 0x06, "FILM_CSR_CFG.dy");
static_assert(offsetof(FILM_CSR_CFG, item) == 0x08, "FILM_CSR_CFG.item");
static_assert(offsetof(FILM_CSR_CFG, file) == 0x0c, "FILM_CSR_CFG.file");

int  init_StoryMenu_Film(void);         // FF3 0x0022ec88
int  one_StoryMenu_Film(void);          // FF3 0x0022f0b8
int  end_StoryMenu_Film(void);          // FF3 0x0022ef30

void FilmMenuSortInit(void);            // FF3 0x0022f628: IngameInit; sort order 1, the sort cursor on row 0

#endif /* _INGAME_STORY_MENU_FILM_H */
