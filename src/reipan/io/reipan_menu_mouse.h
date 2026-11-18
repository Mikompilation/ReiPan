#ifndef REIPAN_MENU_MOUSE_H
#define REIPAN_MENU_MOUSE_H

#ifdef __cplusplus
extern "C" {
#endif

/*
 * The mouse in the game's own menus.
 *
 * The ROM's menus read nothing but the pad.  This lets each of them take the
 * mouse too, without changing what they do: a menu's pad handler asks for the
 * mouse once a frame, moves its own cursor to whatever the pointer is over,
 * and has a click press CROSS as if the player had -- so the menu runs the
 * very code it runs for the pad, sounds and all.
 *
 * Coordinates are the ROM's 640x448 screen, the space every menu draws its
 * sprites in; on a widened output they run past 0 and 640, as the picture
 * does.  Hover only moves a cursor when the pointer has actually moved, so a
 * pointer left resting does not fight the keyboard or the pad.
 *
 * Where things are comes from what was drawn: B2dCtlDraw reports every layout
 * item as it draws it, with the rectangle it really used, so a menu asks
 * "where did you draw item X last frame?" (ingame/menu/menu_mouse.h) rather
 * than keeping coordinates of its own.  An item that did not draw -- hidden,
 * faded out, not yet in -- is simply not a target.
 *
 * Asking for the mouse is what makes it the menu's.  While a menu asks -- this
 * frame or the last -- right click presses TRIANGLE (the game's back in all
 * three button layouts), the wheel presses the directions the menu says it
 * scrolls in, and Enter and Backspace are CROSS and TRIANGLE, the keys a PC
 * player reaches for.  The mouse is never the menu's while the ImGui overlay
 * holds it or is under it, or while an RmlUi screen is up.
 */

/* Logical pad buttons, ReiPan_InputButtonHeld()'s indices. */
#define REIPAN_MENU_UP       0
#define REIPAN_MENU_DOWN     1
#define REIPAN_MENU_LEFT     2
#define REIPAN_MENU_RIGHT    3
#define REIPAN_MENU_TRIANGLE 4
#define REIPAN_MENU_CROSS    5
#define REIPAN_MENU_SQUARE   6
#define REIPAN_MENU_CIRCLE   7
#define REIPAN_MENU_L1       8
#define REIPAN_MENU_L2       9
#define REIPAN_MENU_R1       10
#define REIPAN_MENU_R2       11
#define REIPAN_MENU_START    12
#define REIPAN_MENU_SELECT   13

/* What a menu asks for, ReiPan_MenuMouseBegin()'s `flags`. */
#define REIPAN_MM_WHEEL_VERTICAL   0x01  /* the wheel presses UP / DOWN       */
#define REIPAN_MM_WHEEL_HORIZONTAL 0x02  /* the wheel presses LEFT / RIGHT    */
#define REIPAN_MM_WHEEL_SHOULDERS  0x04  /* the wheel presses L1 / R1 (pages) */
#define REIPAN_MM_NO_BACK          0x08  /* right click and Backspace do
                                            nothing: on this screen TRIANGLE
                                            means something else to the game  */

typedef struct
{
    float x;        /* the pointer in the 640x448 screen                     */
    float y;
    int   inside;   /* it is over the game picture                           */
    int   moved;    /* it moved since the last frame                         */
    int   left;     /* the left button went down this frame                  */
    int   left_held;/* the left button is down                               */
    int   right;    /* the right button went down this frame                 */
    int   wheel;    /* notches this frame, positive away from the player     */
} ReiPan_MenuMouse;

/* A box in the 640x448 screen. */
typedef struct
{
    float x;
    float y;
    float w;
    float h;
} ReiPan_MenuBox;

/* Claim the mouse for this frame's menu and read it.  Returns 1 with `out`
 * filled when the menu has the mouse, 0 (and `out` zeroed) when it does not.
 * Call it once a frame from the menu's pad handler; `out` may be null. */
int ReiPan_MenuMouseBegin(int flags, ReiPan_MenuMouse *out);

/* The pointer is over the box, in 640x448 coordinates.  0 whenever the menu
 * does not have the mouse. */
int ReiPan_MenuMouseIn(float x, float y, float w, float h);

/* Press a logical button (REIPAN_MENU_*) for one pad read, as the player
 * would.  Presses of the same button are spaced so each is its own edge. */
void ReiPan_MenuMousePress(int button);

/* The pointer moved onto the box this frame and the menu has the mouse: time
 * to move a cursor there.  A press already waiting holds it back, so a hover
 * does not overtake the click before it. */
int ReiPan_MenuMouseHover(float x, float y, float w, float h);

/* The left button went down this frame over the box, and no target has taken
 * it yet; taking it.  A second target under the same click does not act. */
int ReiPan_MenuMouseClick(float x, float y, float w, float h);

/* A press anywhere: the left button went down this frame and no target has
 * taken it; taking it.  For a screen where a click anywhere means one thing. */
int ReiPan_MenuMouseClicked(void);

/* A menu of the screen's own tested boxes this frame or the last, the hints and
 * Yes / No aside.  A message over such a screen is only a caption to it, and a
 * click on nothing must not act. */
int ReiPan_MenuMouseHasTargets(void);

/*
 * A menu whose cursor has to be moved the pad's way -- it scrolls, cross-fades
 * or has no MenuCsr to point at.  Hovering box i steps the cursor there with
 * presses of `dec` / `inc` (REIPAN_MENU_UP / DOWN, say), so the menu runs its
 * own movement code; clicking steps there and then presses `confirm` (-1 for
 * nothing).  `cursor` is the box the cursor is on now.  Returns the box under
 * the pointer, or -1.
 */
int ReiPan_MenuMouseSeekList(int cursor, float x, float y, float w, float h,
                             float dx, float dy, int count, int dec, int inc,
                             int confirm);

/* One box of such a menu, for boxes that are not evenly spaced: steps from
 * `cursor` to `index` and returns 1 when the pointer is over the box. */
int ReiPan_MenuMouseSeekBox(int cursor, int index, const ReiPan_MenuBox *box,
                            int dec, int inc, int confirm);

/* Two axes of the same: `cols` x `rows` cells, cell (c, r) at
 * (x + c*dx, y + r*dy), stepped to with UP / DOWN along a column and LEFT /
 * RIGHT across a row.  A click steps there and presses `confirm` (-1 for
 * nothing).  Returns the cell under the pointer as r * cols + c, or -1. */
int ReiPan_MenuMouseSeekGrid(int col, int row, float x, float y, float w,
                             float h, float dx, float dy, int cols, int rows,
                             int confirm);

/* A single target: clicking the box presses `button`.  Returns 1 when the
 * pointer is over it. */
int ReiPan_MenuMouseButton(float x, float y, float w, float h, int button);

/* A screen that only waits to be acknowledged ("saved", "no memory card"):
 * a click anywhere presses CROSS, and right click TRIANGLE as everywhere. */
void ReiPan_MenuMouseConfirm(void);
/* A screen whose only way is out: a click anywhere presses TRIANGLE. */
void ReiPan_MenuMouseBack(void);
/* A screen that waits for START: a click anywhere presses it. */
void ReiPan_MenuMouseStart(void);

/* ---- what the game's layouts report as they draw ------------------------ */

/* One layout item, from B2dCtlDraw, with the screen rectangle it was drawn in
 * (around it, when rotated), its parent's id (-1 none) and its final alpha
 * (0x80 opaque). */
void ReiPan_MenuMouseNoteItem(int handle, int id, int parent, const char *name,
                              float x, float y, float w, float h, int alpha);

/* Where item `id` / `name` of layout `handle` was drawn last frame: 1 and the
 * box when it was drawn visibly, 0 when it was not.  `name` compares exactly.
 * With several items of the name, the first drawn.  A group has no size of its
 * own: its box is that of what it holds. */
int ReiPan_MenuMouseItemBox(int handle, int id, ReiPan_MenuBox *out);
int ReiPan_MenuMouseNameBox(int handle, const char *name, ReiPan_MenuBox *out);

/* The n-th item drawn last frame, in draw order, of any layout; 0 past the
 * end.  For the hint rows, which are found by name whichever layout they are
 * in. */
int ReiPan_MenuMouseDrawnItem(int n, int *handle, int *id, const char **name,
                              ReiPan_MenuBox *box, int *alpha);

/* ---- for the input layer ---------------------------------------------- */

union SDL_Event;
struct SDL_Window;

/* Every event, from PumpEvents().  `free_pointer` and `free_keys` say whether
 * the pointer and the keyboard are free for the menus: not the overlay's, and
 * the game not suppressed.  Returns 1 when the event was the menus' to take. */
int  ReiPan_MenuMouseEvent(const union SDL_Event *event, int free_pointer,
                           int free_keys);
/* Once per pad read, before the game runs: hand the gathered presses to the
 * frame and roll the claim over. */
void ReiPan_MenuMousePoll(void);
/* Pad reads so far, for telling this frame from an older one. */
int  ReiPan_MenuMouseReadCount(void);
/* A virtual press for logical button `index` this pad read. */
int  ReiPan_MenuMouseHeld(int index);
/* A menu has the mouse, this frame or the last. */
int  ReiPan_MenuMouseClaimed(void);

/* Next to a smoke-run screenshot: what was drawn, what the menu tested, where
 * the pointer was (tools/reverse/build.py --run, mmcheck.py). */
void ReiPan_MenuMouseDumpFrame(const char *path);

#ifdef __cplusplus
}
#endif

#endif
