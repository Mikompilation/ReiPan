/* ==========================================================================
 *  ingame/menu/menu_bg.h
 *
 *  The story menu's common helpers, FF3 0x208370..0x2093c8 (FF3-only; link
 *  order had put them in FF2's menu_file.c): the menu backdrop (MenuBg*,
 *  bss 0x3f6058), the row lists (MenuList*), and small layout helpers the
 *  menu pages share.
 * ======================================================================== */
#ifndef _INGAME_MENU_MENU_BG_H
#define _INGAME_MENU_MENU_BG_H

#include <stddef.h>                 // offsetof
#include "sdk/scetypes.h"

struct B2D_HOOK_ARGS;                   // system/b2d/B2dCtl.h

// ──────────────────────────────────────────────────────────────────────
// The backdrop
// ──────────────────────────────────────────────────────────────────────

// The menu backdrop work (bss 0x3f6058, 0x34 bytes on the EE): a darken
// rectangle over the captured screen (SubFocus) and, with a layout, a B2d
// backdrop whose items 0x3eb / 0x3ec draw with a CLUT that MenuBgClutHook
// lerps from MNBG1_SITAJI_MONO_CL2 to MNBG1_SITAJI_BLUE_CL2 as it fades in,
// and the EF_yuki snow (EffMenuBg). Used by the story menu and the outgame
// backdrop (OutgameBgIsLoaded / IsStopped).
// layout: host -- the four buffers are pointers (EE 4 bytes at +0x10..+0x1c);
// the fields before them keep their EE offsets.
struct MENU_BG_WRK
{
    /* 0x00 */ int   dir;           // +1 fading in, -1 fading out (MenuBgStop)
    /* 0x04 */ int   fade;          // 0..0x80, + dir per MenuBgDraw: the CLUT lerp
    /* 0x08 */ int   darken;        // 0..0x6e, + 7 dir per MenuBgDraw: SubFocus's alpha
    /* 0x0c */ int   handle;        // the layout's Bp handle, -1 none
    /* 0x10 */ u_char *clut_from;   // MNBG1_SITAJI_MONO_CL2 (a TIM2 with its CLUT)
    /* 0x14 */ u_char *clut_to;     // MNBG1_SITAJI_BLUE_CL2
    /* 0x18 */ u_char *clut_buf[2]; // copies of clut_from whose CLUT gets the lerp
    /* 0x20 */ int   cur;           // clut_buf[cur] is the one shown
    /* 0x24 */ int   load_from;     // LoadReq ids
    /* 0x28 */ int   load_to;
    /* 0x2c */ int   clut_done;     // the lerp ran this frame (MenuBgDraw clears it)
    /* 0x30 */ int   layout;        // non-zero: a B2d backdrop (MenuBgStart)
};
static_assert(offsetof(MENU_BG_WRK, handle) == 0x0c, "MENU_BG_WRK.handle");
static_assert(offsetof(MENU_BG_WRK, clut_from) == 0x10, "MENU_BG_WRK.clut_from");
static_assert(offsetof(MENU_BG_WRK, cur) == 0x10 + 4 * sizeof(u_char *), "MENU_BG_WRK.cur follows the buffers");
static_assert(offsetof(MENU_BG_WRK, layout) == offsetof(MENU_BG_WRK, cur) + 0x10, "MENU_BG_WRK.layout");

#define MENU_BG_DARKEN_MAX  0x6e
#define MENU_BG_FADE_MAX    0x80

void MenuBgSetHandle(int handle);                               // FF3 0x00208370 (no callers)
int  MenuBgGetHandle(void);                                     // FF3 0x00208380: the backdrop's Bp handle
void MenuBgSetDir(int dir);                                     // FF3 0x00208390
int  MenuBgGetDir(void);                                        // FF3 0x002083a0 (no callers)
void MenuBgSetFade(int fade);                                   // FF3 0x002083b0 (no callers)
int  MenuBgGetFade(void);                                       // FF3 0x002083c0
void MenuBgSetDarken(int alpha);                                // FF3 0x002087c8
void MenuBgStart(int file, int layout);                         // FF3 0x002087d8
int  MenuBgIsReady(int label);                                  // FF3 0x002088f8: 1 when shown (at once without layout)
void MenuBgDraw(void);                                          // FF3 0x00208a70: darken rectangle, steps the alphas
int  MenuBgIsInEnd(void);                                       // FF3 0x00208b38: 1 without layout or when its parts stopped
void MenuBgCancel(void);                                        // FF3 0x00208bc0 (no callers): MenuBgStop, "cancel" early on
void MenuBgStop(void);                                          // FF3 0x00208c68: plays the layout's "out"
int  MenuBgRelease(void);                                       // FF3 0x00208cb0: 1 when released

// ──────────────────────────────────────────────────────────────────────
// Row lists
// ──────────────────────────────────────────────────────────────────────

// One row of a menu list: an item id and the phase the row opens.
struct MENU_LIST_ENT                /* 4 */
{
    /* 0x00 */ short id;            // 0 after MenuListCreate
    /* 0x02 */ short phase;         // -1 after MenuListCreate
};
static_assert(sizeof(MENU_LIST_ENT) == 4, "MENU_LIST_ENT is 4 bytes");
static_assert(offsetof(MENU_LIST_ENT, phase) == 2, "MENU_LIST_ENT.phase");

// A menu's rows (system heap, MenuListCreate; 8 bytes on the EE).
// layout: host -- ent is a pointer (EE 4 bytes); num/max follow it.
struct MENU_LIST
{
    /* 0x00 */ MENU_LIST_ENT *ent;  // max entries (system heap)
    /* 0x04 */ short          num;  // rows added
    /* 0x06 */ short          max;
};
static_assert(offsetof(MENU_LIST, num) == sizeof(MENU_LIST_ENT *), "MENU_LIST.num follows ent");
static_assert(offsetof(MENU_LIST, max) == offsetof(MENU_LIST, num) + 2, "MENU_LIST.max");

short MenuGetTopPhase(void);                                    // FF3 0x00208df8: 0x39, phase 57 StoryMenu_Top
MENU_LIST *MenuListCreate(short max);                           // FF3 0x00208e00
void       MenuListDelete(MENU_LIST *list);                     // FF3 0x00208ea8
int        MenuListAdd(MENU_LIST *list, short id, short phase); // FF3 0x00208f18; 0 when full
short      MenuListGetNum(MENU_LIST *list);                     // FF3 0x00208f70
short      MenuListGetId(MENU_LIST *list, short row);           // FF3 0x00208f88; -1 out of range
short      MenuListGetPhase(MENU_LIST *list, short row);        // FF3 0x00208fc8; -1 out of range

// ──────────────────────────────────────────────────────────────────────
// Layout helpers
// ──────────────────────────────────────────────────────────────────────

int  MenuLayoutIsPlaying(int file);                     // FF3 0x00209048: 1 while the file's layout plays a label
int  MenuLayoutItemIsPlaying(int file, const char *name);   // FF3 0x00209070 (no callers)

// Scroll arrows of a list layout: per direction the arrow item (frozen at
// alpha 0x40 when the list cannot scroll that way, animated at 0x80 when it
// can) and an effect item shown only when it can.
void MenuScrollArrowDisp(int handle, int up, int down, int up_fx, int down_fx,
                         int can_up, int can_down);                      // FF3 0x002090a8

int  MenuItemGetAlpha(int handle, int id);              // FF3 0x00209240 (no callers): part 10 x the parent's, 0..0xff

// Item-hook text on one line: message (msg0, msg1) at the hook's item rect
// (rect[0] x, rect[1] y) in its colour word; flags & 0x20 subtractive.
void B2dHookPrintMsgLine(int msg0, int msg1, int arrange, const int *rect, u_int rgb,
                         int alpha, u_int flags);       // FF3 0x002092f0
// The same shape for a message page (MsgPrintPageMain, arrange its last word)
// and for a number of digits digits (PrintNumber_N, type 0 full / 1 half width,
// no leading zeros).
void B2dHookPrintMsg(int msg0, int msg1, int arrange, const int *rect, u_int rgb,
                     int alpha, u_int flags);           // FF3 0x002093c8
void B2dHookPrintNumber(int num, int digits, u_char type, const int *rect, u_int rgb,
                        u_char alpha, u_int flags);     // FF3 0x00209470
// B2dDrawSpriteDirect with bilinear filtering (TEX1_1 0x161), the cached
// TEX1_1 restored after it.
int  B2dDrawSpriteDirectLinear(const B2D_HOOK_ARGS *args);  // FF3 0x00209518

#endif /* _INGAME_MENU_MENU_BG_H */
