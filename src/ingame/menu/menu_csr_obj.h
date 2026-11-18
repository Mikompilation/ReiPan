/* ==========================================================================
 *  ingame/menu/menu_csr_obj.h
 *
 *  Cursor objects (FF3-only, 0x20b148..0x20b590, after menu_draw_tex.c; the
 *  accessors 0x20b4d0..0x20b588 were once matched to FF2's menu_memo /
 *  menu_radio / menu_soul static ctors and font helpers): a menu keeps its own MENU_CSR_WRK
 *  (grid + x / y scroll views, ingame/menu/menu_csr.h) and works on it with
 *  these. They are the operations of menu_csr.c's MenuCsrWork* again, as a
 *  second out-of-line copy (MenuDispMain, MenuCam, finder_camera.c, item.c
 *  and FurnCtl.c call this one); MenuCsrObjSetPos also places the views.
 * ======================================================================== */
#ifndef _INGAME_MENU_MENU_CSR_OBJ_H
#define _INGAME_MENU_MENU_CSR_OBJ_H

#include "ingame/menu/menu_csr.h"       // MENU_CSR_WRK

void  MenuCsrObjInitTbl(MENU_CSR_WRK *wrk, int tbl_no, int cfg);    // FF3 0x0020b148: layout from MenuCsrGetTbl
void  MenuCsrObjInit(MENU_CSR_WRK *wrk, int w, int h, int cfg);     // FF3 0x0020b1a8: item k in cell k
void  MenuCsrObjSetViewX(MENU_CSR_WRK *wrk, int size, int margin);  // FF3 0x0020b290
void  MenuCsrObjSetViewY(MENU_CSR_WRK *wrk, int size, int margin);  // FF3 0x0020b2c0
void  MenuCsrObjUpdate(MENU_CSR_WRK *wrk, int pad);                 // FF3 0x0020b2f0: once per frame
void  MenuCsrObjRelease(MENU_CSR_WRK *wrk);                         // FF3 0x0020b318: frees its own table
int   MenuCsrObjIsMovedSe(MENU_CSR_WRK *wrk, int se);               // FF3 0x0020b378: moved this frame (plays se >= 0)
short MenuCsrObjSetPos(MENU_CSR_WRK *wrk, int pos, int pos_x, int pos_y, int top_x, int top_y);  // FF3 0x0020b3c8
short MenuCsrObjGetPos(MENU_CSR_WRK *wrk);                          // FF3 0x0020b4a8: the item under the cursor
short MenuCsrObjGetTopX(MENU_CSR_WRK *wrk);                         // FF3 0x0020b4b0: -1 when off
short MenuCsrObjGetTopY(MENU_CSR_WRK *wrk);                         // FF3 0x0020b4d0: -1 when off
short MenuCsrObjGetViewPosX(MENU_CSR_WRK *wrk);                     // FF3 0x0020b4f0: cursor column in the view, -1 when off
short MenuCsrObjGetViewPosY(MENU_CSR_WRK *wrk);                     // FF3 0x0020b510: cursor row in the view, -1 when off
int   MenuCsrObjPagePrevX(MENU_CSR_WRK *wrk);                       // FF3 0x0020b530
int   MenuCsrObjPageNextX(MENU_CSR_WRK *wrk);                       // FF3 0x0020b548
int   MenuCsrObjPagePrevY(MENU_CSR_WRK *wrk);                       // FF3 0x0020b560
int   MenuCsrObjPageNextY(MENU_CSR_WRK *wrk);                       // FF3 0x0020b578

#endif /* _INGAME_MENU_MENU_CSR_OBJ_H */
