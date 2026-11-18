/* ==========================================================================
 *  outgame/outgame_bg.h
 *
 *  The outgame backdrop: what the title-side screens (options, start menu,
 *  the memory-card load and save, the album, the finder's photo screens)
 *  draw behind themselves. One backdrop at a time, chosen by type:
 *    0 the title's background movie (a TITLE_OBJ, OUTGAME_BG_PSS) faded in
 *      and out over 10 frames;
 *    1 a layout (a BpObj of the given file) played "in" / "out", one item
 *      optionally hidden;
 *    2 the menu backdrop (MenuBgStart(file, 1), ingame/menu/menu_file.c).
 *  Over it a cover layout (OUTGAME_TTLBG_PHF) plays "in" while the backdrop
 *  is enabled and "out" once it is disabled. Also the menu-BGM pair the load
 *  and save screens play, and the OutgameBgObj wrappers that show or hide
 *  the type-0 movie object.
 *  FF3 0x24fb28..0x250268, FF3-only file (name descriptive); its statics are
 *  sdata 0x372088..0x3720b4.
 * ======================================================================== */
#ifndef _OUTGAME_OUTGAME_BG_H
#define _OUTGAME_OUTGAME_BG_H

// OutgameBgStart's type.
enum OUTGAME_BG_TYPE
{
    OUTGAME_BG_NONE   = -1,             // after OutgameBgRelease
    OUTGAME_BG_MOVIE  = 0,              // TITLE_OBJ: file is a PSS
    OUTGAME_BG_LAYOUT = 1,              // BpObj: file is a layout; arg an item id to hide (-1 none)
    OUTGAME_BG_MENU   = 2,              // MenuBg: file is MenuBgStart's; arg MenuBgIsReady's label
};

void OutgameBgStart(int type, int file, int arg);   // FF3 0x0024fcf0
int  OutgameBgIsLoaded(void);                       // FF3 0x0024fdb8: 1 when the backdrop is loaded
int  OutgameBgLoadWait(void);                       // FF3 0x0024fb70: 1 when the cover is loaded
void OutgameBgEnable(void);                         // FF3 0x0024fbb0: the cover plays "in"
void OutgameBgDisable(void);                        // FF3 0x0024fbc0: the cover plays "out"
void OutgameBgDraw(void);                           // FF3 0x0024fea8: per frame
int  OutgameBgIsReady(void);                        // FF3 0x0024fef8: 1 when shown
void OutgameBgStop(void);                           // FF3 0x0024ff88
int  OutgameBgIsStopped(void);                      // FF3 0x0024ffb0: 1 when gone (cover too)
void OutgameBgRelease(void);                        // FF3 0x00250050

void OutgameBgmPlay(int file);                      // FF3 0x002500e8: exclusive BGM, loop SEs paused
void OutgameBgmStop(void);                          // FF3 0x00250138

int  OutgameBgObjIsUsed(void);                      // FF3 0x00250168: the backdrop is the movie (type 0)
void OutgameBgObjShow(void);                        // FF3 0x00250178
int  OutgameBgObjIsShown(void);                     // FF3 0x002501b0 (1: TitleObjIsShown is)
void OutgameBgObjHide(void);                        // FF3 0x002501f0
int  OutgameBgObjIsReady(void);                     // FF3 0x00250228

#endif /* _OUTGAME_OUTGAME_BG_H */
