/* ==========================================================================
 *  ingame/cine_menu.h
 *
 *  The cine film menu (FF3-only, FF3 0x149c98..0x14ac58; FF2 has no
 *  counterpart, the file name is descriptive -- link order filed it under
 *  FF2's ingame/enemy/alg_manage.c). The projector room's film list: a clone
 *  of the cassette menu (menu_cassette.c) on the EISYASITU_PHF layout. It
 *  lists the film items the player holds (MenuCsr list, 7-row view), asks
 *  before playing and hands the chosen film to the room's projector
 *  (CineFilmPlayReq, cine_film.c) while the layout slides out. It runs as
 *  phase 47 Story_CineMenu (event CINE_MENU_START, CINE_MENU_END waits for
 *  it to close). The film table here (item, PSS movie, photo window) is the
 *  one cine_film.c's flags are indexed by.
 * ======================================================================== */
#ifndef _INGAME_CINE_MENU_H
#define _INGAME_CINE_MENU_H

#include <stddef.h>                     // offsetof
#include "sdk/scetypes.h"
#include "ingame/cine_film.h"           // CINE_FILM_NUM

#define CINE_MENU_VIEW_ROWS 7           // rows the list shows at once
#define CINE_MENU_CSR       0x2f        // MenuCsr id of the list
#define CINE_MENU_PHASE     47          // Story_CineMenu

// One film (data 0x3508c8, 11 rows of 0x10).
struct CINE_FILM_DAT                    /* 0x10 */
{
    /* 0x00 */ int item;                // item id (0x3c..0x44), -1: not an item (a film's second cut)
    /* 0x04 */ int file;                // its PSS movie (SCENE91xxM_PSS ...)
    /* 0x08 */ int photo[2];            // {start, end} frames of its photo window, -1: none
};
static_assert(sizeof(CINE_FILM_DAT) == 0x10, "CINE_FILM_DAT is 0x10 bytes");
static_assert(offsetof(CINE_FILM_DAT, file) == 0x4, "CINE_FILM_DAT.file");
static_assert(offsetof(CINE_FILM_DAT, photo) == 0x8, "CINE_FILM_DAT.photo");

// The menu's state (sdata 0x36d9ec), shared by CineMenuCtrl (pad) and
// CineLayoutMain (draw). The cassette menu's numbering up to 8; a film
// plays on the projector after the menu, so there is no play state.
enum CINE_MENU_STATE
{
    CINE_MENU_ST_OPEN     = 0,          // open the message window
    CINE_MENU_ST_LOAD     = 1,          // layout and window loading, background darkening
    CINE_MENU_ST_IN       = 2,          // layout sliding in
    CINE_MENU_ST_LIST     = 3,          // choosing a film
    CINE_MENU_ST_ASK_ON   = 4,          // show the question
    CINE_MENU_ST_ASK      = 5,          // "play it?"
    CINE_MENU_ST_ASK_OFF  = 6,          // back to the help line
    CINE_MENU_ST_PLAY_OUT = 7,          // film requested, layout sliding out
    CINE_MENU_ST_CLOSE    = 8,          // layout sliding out to close
    CINE_MENU_ST_UNDARKEN = 9,          // background back to normal
    CINE_MENU_ST_END      = 10,         // closed
};

extern const CINE_FILM_DAT cine_film_tbl[CINE_FILM_NUM];    // data 0x3508c8

// The menu (pad side).
void CineMenuInit(short csr);           // FF3 0x00149c98
int  CineMenuCtrl(short csr);           // FF3 0x00149d00: 1 when closed
void CineMenuTerm(short csr);           // FF3 0x00149f58

// The layout (draw side).
void CineBgAlphaInit(void);             // FF3 0x00149fa0
int  CineBgFadeIn(void);                // FF3 0x00149fa8: 1 when dark
int  CineBgFadeOut(void);               // FF3 0x00149fd8: 1 when clear
void CineBgDraw(void);                  // FF3 0x0014a000
void CineMenuSetRowHook(void);          // FF3 0x0014a1b8
void CineScrollBarDraw(void);           // FF3 0x0014a1f8
void CineScrollBarAdd(void);            // FF3 0x0014a268
void CineScrollBarDel(void);            // FF3 0x0014a290
void CineCsrUpdate(void);               // FF3 0x0014a2a8
void CineCsrAdd(void);                  // FF3 0x0014a2e0
void CineCsrFree(void);                 // FF3 0x0014a310
void CineHelpSet(void);                 // FF3 0x0014a328
void CineHelpMode(int ask);             // FF3 0x0014a3a0
void CineMsgWinOpen(void);              // FF3 0x0014a3e8
int  CineMsgWinIsReady(void);           // FF3 0x0014a430
void CineMsgWinSetAlphaItem(int handle, int id);    // FF3 0x0014a458
void CineMsgWinClose(void);             // FF3 0x0014a480
void CineLayoutCreate(short csr);       // FF3 0x0014a4c8
void CineLayoutMain(void);              // FF3 0x0014a510
void CineLayoutRelease(void);           // FF3 0x0014a758

// The film table and the held-film list.
int  CineFilmTblGetNum(void);           // FF3 0x0014a7c8: 11
int  CineFilmTblGetItem(int no);        // FF3 0x0014a7d0: -1 for no < 0
int  CineFilmTblGetFile(int no);        // FF3 0x0014a7f8: the PSS file, -1 for no < 0 (CINE_FILM_PLAY)
int *CineFilmTblGetPhotoWin(int no);    // FF3 0x0014a820: {start, end} frames, nullptr for no < 0
int  CineFilmTblFindFile(int file);     // FF3 0x0014a840: the row of a PSS file, -1
int  CineFilmListCreate(void);          // FF3 0x0014a8a8: the count
int  CineFilmGetNum(void);              // FF3 0x0014a9a0
int  CineFilmGetListNo(int pos);        // FF3 0x0014a9b0: the row at list position pos
void CineFilmListRelease(void);         // FF3 0x0014a9c8
int  CineFilmGetHaveNum(void);          // FF3 0x0014aa08: film items held (CINE_FILM_NOT_HAVE)

// Phase 47 Story_CineMenu.
void CineMenuStateClear(void);          // FF3 0x0014aa90: CineInit, IngameInit
void CineMenuReq(void);                 // FF3 0x0014aa98: CINE_MENU_START
int  CineMenuGetState(void);            // FF3 0x0014ab08: 1 while open (CINE_MENU_END waits for 0)
int  init_Story_CineMenu(void);         // FF3 0x0014ab10
int  one_Story_CineMenu(void);          // FF3 0x0014ab78: 1 once closed
int  end_Story_CineMenu(void);          // FF3 0x0014abf0

#endif /* _INGAME_CINE_MENU_H */
