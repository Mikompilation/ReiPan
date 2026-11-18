/* ==========================================================================
 *  ingame/menu/menu_mouse.h
 *
 *  PC: the mouse on the game's own cursors (no FF3 counterpart).  The input
 *  half is reipan/io/reipan_menu_mouse.h; this half knows the menus: a layout
 *  item drawn last frame is a target, and pointing at it puts a cursor (the
 *  grid cursors of common/csr_ctrl.h, whichever owns them) on a cell.
 *
 *  The cursor moves inside CsrPadCtrl, right after the pad would have moved
 *  it, and raises CSR_EV_MOVED: to the menu it is a step of the d-pad, so its
 *  sound and everything it redoes on a move run as they always do.  A menu
 *  that keeps a cursor of its own steps it with presses instead
 *  (ReiPan_MenuMouseSeekList).
 * ======================================================================== */
#ifndef _INGAME_MENU_MENU_MOUSE_H
#define _INGAME_MENU_MENU_MOUSE_H

#include "common/cmn_util.h"            // CSR_GRID
#include "common/csr_ctrl.h"
#include "ingame/menu/menu_csr.h"
#include "reipan/io/reipan_menu_mouse.h"
#include "system/b2d/B2dGridSpr.h"      // B2D_GRID_SPR
#include "system/b2d/BpObj.h"           // BP_OBJ

// Point cursor `csr` (a MenuCsr id) at `cell`, an item id of its grid, when
// the pointer is over `box`: hovering moves the cursor there, a click moves
// it and presses CROSS. 1 when the pointer is over the box. Call it before or
// after the frame's MenuCsrUpdate; a cell with no item is left alone.
int MenuMouseCsr(int csr, int cell, const ReiPan_MenuBox *box);
int MenuMouseCsrWork(MENU_CSR_WRK *wrk, int cell, const ReiPan_MenuBox *box);

// The same with the box being a layout item as drawn last frame; nothing when
// it did not draw.
int MenuMouseCsrName(int csr, int cell, int handle, const char *name);
int MenuMouseCsrId(int csr, int cell, int handle, int id);
int MenuMouseCsrWorkName(MENU_CSR_WRK *wrk, int cell, int handle, const char *name);
int MenuMouseCsrWorkId(MENU_CSR_WRK *wrk, int cell, int handle, int id);

// A list or grid whose cursor is a sprite that moves a fixed step per cell
// (B2D_GRID_SPR, BP_OBJ::AddCsr), which is most of the game's: the sprite is
// drawn on the cursor's cell, so every other cell on show is that box moved by
// the steps. The cursor's views say which cells are on show; a scrolled list
// needs nothing more. Call it each frame from the cursor's update.
void MenuMouseCsrGrid(MENU_CSR_WRK *wrk, const B2D_GRID_SPR *spr, int handle);
// The same from a box and steps: `at` is where the cursor's cell is drawn
// (MenuMouseCsrFrom), or the first cell on show when the sprite does not move
// (MenuMouseCsrOrigin).
void MenuMouseCsrFrom(MENU_CSR_WRK *wrk, const ReiPan_MenuBox *at, float dx, float dy);
void MenuMouseCsrOrigin(MENU_CSR_WRK *wrk, const ReiPan_MenuBox *at, float dx, float dy);
void MenuMouseCsrGridId(int csr, const B2D_GRID_SPR *spr, int handle);
// The same for a BP_OBJ's cursor `id`.
void MenuMouseCsrObj(MENU_CSR_WRK *wrk, BP_OBJ *obj, int id);
void MenuMouseCsrObjId(int csr, BP_OBJ *obj, int id);

// A cursor bar that is one layout item moved to the cursor's row or cell
// (usually a group): MenuMouseCsrFrom from where it was drawn.
void MenuMouseCsrBar(MENU_CSR_WRK *wrk, int handle, int id, float dx, float dy);

// A cursor the menu keeps itself. `at` is the box of the entry the cursor is on
// and the other `count` entries are the steps away from it; `can` (may be null)
// says which can be pointed at. Hovering moves the cursor, a click moves it and
// presses CROSS. Returns the cursor to store; its sound is the caller's.
int MenuMouseStepList(int cursor, const ReiPan_MenuBox *at, float dx, float dy, int count,
                      int (*can)(int index));

// The two-axis cursor of common/cmn_util.h (CSR_GRID): point axis `axis` of `g`
// at `pos`. As above, the move lands in CSR_GRID::Move, as a step of the d-pad.
int MenuMouseGrid(CSR_GRID *g, int axis, int pos, const ReiPan_MenuBox *box);
// From CSR_GRID::Move: the same claim, and the move asked for it: the step's
// sign and its axis in *axis, 0 for none.
void MenuMouseGridClaim(const CSR_GRID *g);
int  MenuMouseGridApply(CSR_GRID *g, int *axis);

// `count` rows from `first`, `dy` apart, widened to `w` when w > 0: row n is
// cell first_cell + n.
void MenuMouseCsrRows(int csr, int first_cell, const ReiPan_MenuBox *first, float w,
                      float dx, float dy, int count);

// A tab `index` of tabs the menu steps through with `dec` / `inc`, `cur` being
// the tab on show: a click on it presses the steps there. Tabs do not follow the
// pointer: leaving one should not need aiming around the others.
void MenuMouseTab(int cur, int index, const ReiPan_MenuBox *box, int dec, int inc);

// A button guide that is one picture of `rows` lines (the puzzles'): a click
// on line n presses buttons[n], -1 for a line that is no button ("choose").
void MenuMouseGuide(int handle, const char *name, const int *buttons, int rows);

// A box grown by a margin on each side: most items of a row are smaller than
// the row looks.
ReiPan_MenuBox MenuMouseBoxGrow(ReiPan_MenuBox box, float mx, float my);
// The smallest box around both.
ReiPan_MenuBox MenuMouseBoxUnion(ReiPan_MenuBox a, ReiPan_MenuBox b);

// Which answer of a Yes / No is selected, 0 yes, 1 no, from the hooks that draw
// its plates this frame (they run while the window is drawn, so it is the
// window of BpGetDrawHandle). Every Yes / No of the game has the words "yes_on"
// and "no_on", so with this the poll answers them all: pointing at the other
// answer steps LEFT toward Yes or RIGHT toward No, which both kinds of handler
// the game has (toggle, or set) take the same way, and a click answers.
void MenuMouseNoteChoice(int sel);

// A click anywhere presses the button of `mask` (sce pad bits, a pad_key entry
// as the game reads it): the screen waits for that button and nothing else.
void MenuMouseClickMask(u_int mask);

// Once per pad read, before the game runs (ReiPan_MenuMousePoll): what needs
// no screen of its own. A screen showing a button guide (capCtl's caption
// layout) is a menu, so it has the mouse, and a click on a hint presses that
// button; a Yes / No drawn last frame is answered.
void MenuMousePoll(void);

// From the pad checks: outside free play, whatever tests OK or cancel is a menu,
// so right click, Enter and Backspace work on it, hook of its own or not. In
// free play those buttons are the game's (the viewfinder, ...), never the mouse's.
void MenuMouseAskedPad(u_int mask);

// From CsrPadCtrl: a cursor being updated is a menu, so it has the mouse and
// the wheel steps it; and a move asked for this cursor lands now.
void MenuMouseCsrClaim(const CSR_ITEM *item);
void MenuMouseCsrApply(CSR_ITEM *item, CSR_VIEW *vx, CSR_VIEW *vy);

#endif /* _INGAME_MENU_MENU_MOUSE_H */
