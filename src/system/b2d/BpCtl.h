/* ==========================================================================
 *  system/b2d/BpCtl.h
 *
 *  FF3's Bp layout controller (file string "BpCtl.c", 0x106370..0x1089e8):
 *  a table of 45 loaded layout files over the B2d core (B2dCtl / B2dFile /
 *  B2dSpr / B2dInp) and BpTex. A Bp file is either one .b2d layout or a pack
 *  holding a .b2d and its textures. Per entry: load (state 1), ready (2),
 *  playing a label (3), play ended (4); BpCtlMain draws every entry once a
 *  frame. The module also holds B2dSystemInit/End, the direct GS draws the
 *  B2d core calls for its items, item hit tests and the B2dNum digit font.
 * ======================================================================== */
#ifndef _SYSTEM_B2D_BPCTL_H
#define _SYSTEM_B2D_BPCTL_H

#include <stddef.h>                     // offsetof

#include "sdk/scetypes.h"
#include "system/b2d/B2dCtl.h"          // B2D_ITEM, B2D_PART, B2D_HOOK_ARGS

#define BP_CTL_MAX      45              // entries in bp_ctl (bss 0x377730)

// BP_CTL_ENT.flags low nibble.
enum BP_CTL_STATE
{
    BP_CTL_STATE_LOAD  = 1,             // file loading; BpCtlStepLoad builds it when done
    BP_CTL_STATE_READY = 2,
    BP_CTL_STATE_PLAY  = 3,             // a label plays (BpPlayLabelEx)
    BP_CTL_STATE_END   = 4,             // the label's animation ran out; still drawn
};
#define BP_CTL_STATE_MASK   0x0f
#define BP_CTL_FLAG_PAUSE   0x10        // the play / end steps skip B2dCtlDraw
#define BP_CTL_FLAG_USERBUF 0x100       // the load buffer came from the caller

// One loaded layout (0x24 bytes on the EE). layout: host -- +0xc..+0x18 are
// buffers (EE pointers); only the words before them keep their EE offsets.
struct BP_CTL_ENT
{
    /* 0x00 */ int    handle;           // its index; the B2d core's handle too
    /* 0x04 */ int    file;             // file number, -1 free
    /* 0x08 */ int    load_id;          // LoadReqGetAddr's request id
    /* 0x0c */ void  *buf;              // the loaded file
    /* 0x10 */ void  *b2d;              // the .b2d data in it
    /* 0x14 */ void  *work;             // B2d work (heap)
    /* 0x18 */ void  *tex;              // BpTex work (heap; packs only)
    /* 0x1c */ u_int  flags;            // BP_CTL_STATE in the low nibble, BP_CTL_FLAG_*
    /* 0x20 */ int    draw;             // BpCtlMain draws the entry when != 0
};
static_assert(offsetof(BP_CTL_ENT, file) == 0x04, "BP_CTL_ENT.file");
static_assert(offsetof(BP_CTL_ENT, load_id) == 0x08, "BP_CTL_ENT.load_id");

struct TIM2_PICTUREHEADER;              // graphics/graph2d/tim2.h

// Screen bounds (B2dCtlGetBounds).
struct B2D_BOUNDS                       /* 0x10 */
{
    int x0, y0, x1, y1;
};
static_assert(sizeof(B2D_BOUNDS) == 0x10, "B2D_BOUNDS is 0x10 bytes");

// One digit glyph of the number font: B2dSprGetPartData parts 1..4.
struct B2D_NUM_GLYPH                    /* 0x10 */
{
    /* 0x00 */ int u, v;
    /* 0x08 */ int w, h;
};
static_assert(sizeof(B2D_NUM_GLYPH) == 0x10, "B2D_NUM_GLYPH is 0x10 bytes");

#define B2D_NUM_SEP     10              // separator glyph, every 3 digits (flag 1)
#define B2D_NUM_MINUS   11              // minus glyph (flag 2; nothing in FF3 sets it)

// The number font (bss 0x377d88).
struct B2D_NUM_FONT                     /* 0xcc */
{
    /* 0x00 */ u_int         flags;     // 1: separator glyph set
    /* 0x04 */ int           handle;    // Bp handle the glyphs come from
    /* 0x08 */ int           tex;       // part 0 of glyph '0': the texture for BpTex
    /* 0x0c */ B2D_NUM_GLYPH glyph[12];
};
static_assert(sizeof(B2D_NUM_FONT) == 0xcc, "B2D_NUM_FONT is 0xcc bytes");
static_assert(offsetof(B2D_NUM_FONT, glyph) == 0x0c, "B2D_NUM_FONT.glyph");

// ---- system ----
int  B2dSystemInit(void);
void B2dSystemEnd(void);

// ---- the entry table ----
int  BpCtlLoadReq(int file, void *addr);                // addr nullptr: allocate; returns the handle
int  BpCtlLoadReqLast(int file, void *addr);            // as BpCtlLoadReq, last free entry
int  BpCtlRelease(int handle);                          // 0, -1 no entry
void BpCtlReleaseAll(void);
int  BpCtlGetHandleByFileNo(int file);                  // -1 when not loaded
int  BpCtlGetState(int handle);                         // BP_CTL_STATE, -1 no entry
int  BpCtlIsDataReady(int handle);                      // 1 loaded, 0 not yet, -1 no entry
int  BpCtlIsDataReadyAll(void);
int  BpCtlIsLoadEnd(int file);                          // 1 when the playing label's animation ended
void BpCtlSetField20(int handle, int value);            // BP_CTL_ENT.draw
void BpCtlSetFlag(int handle, u_int bits);
u_int BpCtlGetFlag(int handle);
void BpCtlClearFlag(int handle, u_int bits);
int  BpPlayLabelEx(int handle, const char *label, int rewind);   // 0, -1 no entry, -2 not ready, -3 no label
int  BpPlayLabel(int handle, const char *label);
int  BpPlayLabelAll(const char *label);                 // number of failures

// ---- frame ----
int  BpCtlMain(void);                                   // entries not yet ended
int  BpCtlDrawOne(int handle);
int  BpCtlDrawBegin(void);
int  BpCtlDrawEnd(void);
void BpCtlFrameReset(void);
void BpCtlSetHooks(void);                              // FF3 0x00107c90: this module's draw hooks back into the B2d core
void BpCtlFrameSkip(void);
int  BpCtlIsFrameDone(void);
int  BpGetDrawHandle(void);                             // handle drawn now, -1 outside BpCtlMain
void *BpGetLoadPack(void);                              // pack being registered (BpTex)

// ---- buffers ----
void  BpArenaSet(u_int addr, int size);                 // EE addresses
u_int BpArenaAlloc(int size);                           // 0 when full

// ---- direct draws (B2d core hooks; B2D_HOOK_ARGS words as on the EE) ----
void BpSetDrawGsRegs(void);
int  BpGetBlendAlpha(u_int flags);
void BpSetBlendAlpha(u_int flags);
int  B2dDrawSpriteDirect(const B2D_HOOK_ARGS *args);    // a1 u, a2 v, a3 w, t0 h, t1 x, t2 y, t3 sx, stack sy, alpha, rgb, flags
int  B2dDrawRectDirect(const B2D_HOOK_ARGS *args);      // a0 rgb[3], a1 w, a2 h, a3 x, t0 y, t1 sx, t2 sy, t3 alpha, stack flags
int  B2dDrawQuadDirect(const B2D_HOOK_ARGS *args);      // a1 xy[8], a2 uv[8], a3 alpha, t0 rgb, t1 flags
int  BpPadGetDir8(const B2D_HOOK_ARGS *args);           // a0 n

// ---- items ----
B2D_PART *B2dCtlGetLongestPart(int handle);
int  B2dPartIsPlaying(const B2D_PART *part);
int  B2dItemIsPlaying(const B2D_ITEM *item);
int  B2dCtlHasItemName(int handle, const char *name);
void B2dCtlSetItemDispByName(int handle, const char *name, int disp);
int  B2dEdgeFunc(const int *p, const int *a, const int *b);
void B2dRectToScreen(int handle, B2D_RECT *out, const B2D_RECT *in);
int  B2dItemGetScreenRect(B2D_RECT *out, B2D_ITEM *item, int handle);
typedef int (*B2D_ITEM_FIND_FUNC)(B2D_ITEM *item, intptr_t a, intptr_t b);
B2D_ITEM *B2dCtlFindItemIf(int handle, B2D_ITEM *start, B2D_ITEM_FIND_FUNC func, intptr_t a, intptr_t b);
B2D_ITEM *B2dCtlFindItemAt(int handle, B2D_ITEM *start, int x, int y);
int  B2dCtlGetBounds(int handle, B2D_BOUNDS *bounds, int reset);
TIM2_PICTUREHEADER *BpTexGetPicture(int a0, int a1);    // first picture of BpTex's BpTexGetTim2 texture
int  BpTexGetTexel(int a0, int a1, int x, int y);       // -1 outside
// Draws the sprite item `name` of `handle` at x, y: alpha 1.0 = opaque,
// rotated by rot degrees and scaled by sx, sy about (x + px, y + py).
// r, g, b are passed on but FF3 never reads them (the sprite is white).
void B2dDrawSpriteByName(int handle, const char *name, int x, int y, u_char r, u_char g, u_char b,
                         float alpha, float rot, float sx, float sy, float px, float py);

// ---- numbers ----
int  B2dNumSetFont(int handle, const char *prefix, const char *sep);   // glyphs prefix'0'..'9'
void B2dNumSetTex(void);
int  B2dNumDraw(int value, int x, int y, u_char alpha, int digits);    // right-aligned at x

#endif /* _SYSTEM_B2D_BPCTL_H */
