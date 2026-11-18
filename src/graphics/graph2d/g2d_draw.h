/* ==========================================================================
 *  graphics/graph2d/g2d_draw.h
 *
 *  2D primitive drawing and local (VRAM) image copies (FF2 g2d_draw.c).
 *  FF3 0x1c8e28..0x1cae98. Layouts checked against FF3's Copy* functions;
 *  they match FF2's.
 * ======================================================================== */
#ifndef _GRAPHICS_GRAPH2D_G2D_DRAW_H
#define _GRAPHICS_GRAPH2D_G2D_DRAW_H

#include <stddef.h>                     // offsetof
#include <stdint.h>                     // uintptr_t

#include "sdk/scetypes.h"

// A single-texture sprite, ready for DispSprD (no pointers: keeps its PS2
// size). CopySprDToSpr fills it from a SPRT_DAT; callers then offset x/y,
// scale alpha and pick the blend (alphar 0x44 normal, 0x48 additive).
struct DISP_SPRT                    /* 0x88 */
{
    /* 0x00 */ u_int   att;
    /* 0x04 */ u_int   u;
    /* 0x08 */ u_int   v;
    /* 0x0c */ u_int   w;
    /* 0x10 */ u_int   h;
    /* 0x14 */ float   crx;
    /* 0x18 */ float   cry;
    /* 0x1c */ float   csx;
    /* 0x20 */ float   csy;
    /* 0x24 */ float   x;
    /* 0x28 */ float   y;
    /* 0x2c */ u_int   z;
    /* 0x30 */ float   scw;
    /* 0x34 */ float   sch;
    /* 0x38 */ float   rot;
    /* 0x40 */ u_long  gftg;
    /* 0x48 */ u_long  tex0;
    /* 0x50 */ u_long  tex1;
    /* 0x58 */ u_long  texa;
    /* 0x60 */ u_long  alphar;
    /* 0x68 */ u_long  zbuf;
    /* 0x70 */ u_long  test;
    /* 0x78 */ u_int   pri;
    /* 0x7c */ u_char  r;
    /* 0x7d */ u_char  g;
    /* 0x7e */ u_char  b;
    /* 0x7f */ u_char  alpha;
    /* 0x80 */ u_char  col;
};
static_assert(sizeof(DISP_SPRT) == 0x88, "DISP_SPRT is 0x88 bytes on the PS2");
static_assert(offsetof(DISP_SPRT, x) == 0x24, "DISP_SPRT.x");
static_assert(offsetof(DISP_SPRT, y) == 0x28, "DISP_SPRT.y");
static_assert(offsetof(DISP_SPRT, alphar) == 0x60, "DISP_SPRT.alphar");
static_assert(offsetof(DISP_SPRT, r) == 0x7c, "DISP_SPRT.r");
static_assert(offsetof(DISP_SPRT, alpha) == 0x7f, "DISP_SPRT.alpha");

// Compact sprite record (ROM tables of these, one per picture of a layout).
struct SPRT_DAT                     /* 0x20 */
{
    /* 0x00 */ u_long  tex0;
    /* 0x08 */ u_short u;
    /* 0x0a */ u_short v;
    /* 0x0c */ u_short w;
    /* 0x0e */ u_short h;
    /* 0x10 */ int     x;
    /* 0x14 */ int     y;
    /* 0x18 */ int     pri;
    /* 0x1c */ u_char  alpha;
    /* 0x1d */ u_char  flip;
    /* 0x1e */ u_char  bln;
};
static_assert(sizeof(SPRT_DAT) == 0x20, "SPRT_DAT is 0x20 bytes on the PS2");
static_assert(offsetof(SPRT_DAT, x) == 0x10, "SPRT_DAT.x");
static_assert(offsetof(SPRT_DAT, alpha) == 0x1c, "SPRT_DAT.alpha");

// The effect texture tables (FF2 g2d_main.c's effdat; FF3's owner not settled,
// defined in src/lib/unassigned_stubs.c from the ELF): effdat in colour /
// monochrome pairs (index + eff_wrk.monochro_mode), lang_effdat the language
// pack's pictures (LangTexSend sends them).
extern SPRT_DAT effdat[92];             // data 0x351b00
extern SPRT_DAT lang_effdat[36];        // data 0x351470

void CopySprDToSpr(DISP_SPRT *s, const SPRT_DAT *d);       // FF3 0x001c8e40
void DispSprD(DISP_SPRT *s);                                // FF3 0x001c8f48

// A sprite with explicit UV corners, ready for DispSprD2 (no pointers: keeps
// its PS2 size).
struct DISP_SPRT2                   /* 0x90 */
{
    /* 0x00 */ u_int   att;
    /* 0x04 */ u_short u1;
    /* 0x06 */ u_short v1;
    /* 0x08 */ u_short u2;
    /* 0x0a */ u_short v2;
    /* 0x0c */ float   w;
    /* 0x10 */ float   h;
    /* 0x14 */ float   crx;
    /* 0x18 */ float   cry;
    /* 0x1c */ float   csx;
    /* 0x20 */ float   csy;
    /* 0x24 */ float   x;
    /* 0x28 */ float   y;
    /* 0x2c */ u_int   z;
    /* 0x30 */ float   scw;
    /* 0x34 */ float   sch;
    /* 0x38 */ float   rot;
    /* 0x40 */ u_long  gftg;
    /* 0x48 */ u_long  tex0;
    /* 0x50 */ u_long  tex1;
    /* 0x58 */ u_long  texa;
    /* 0x60 */ u_long  alpreg;
    /* 0x68 */ u_long  zbuf;
    /* 0x70 */ u_long  test;
    /* 0x78 */ u_long  clmp;
    /* 0x80 */ u_int   pri;
    /* 0x84 */ u_char  r;
    /* 0x85 */ u_char  g;
    /* 0x86 */ u_char  b;
    /* 0x87 */ u_char  alp;
    /* 0x88 */ u_char  col;
};
static_assert(sizeof(DISP_SPRT2) == 0x90, "DISP_SPRT2 is 0x90 bytes on the PS2");
static_assert(offsetof(DISP_SPRT2, w) == 0x0c, "DISP_SPRT2.w");
static_assert(offsetof(DISP_SPRT2, x) == 0x24, "DISP_SPRT2.x");
static_assert(offsetof(DISP_SPRT2, gftg) == 0x40, "DISP_SPRT2.gftg");
static_assert(offsetof(DISP_SPRT2, clmp) == 0x78, "DISP_SPRT2.clmp");
static_assert(offsetof(DISP_SPRT2, pri) == 0x80, "DISP_SPRT2.pri");
static_assert(offsetof(DISP_SPRT2, r) == 0x84, "DISP_SPRT2.r");
static_assert(offsetof(DISP_SPRT2, col) == 0x88, "DISP_SPRT2.col");

// Source description CopySprDToSpr2 turns into a DISP_SPRT2.
struct SPRT_DAT2                    /* 0x30 */
{
    /* 0x00 */ u_long tex0;
    /* 0x08 */ float  u1;
    /* 0x0c */ float  v1;
    /* 0x10 */ float  u2;
    /* 0x14 */ float  v2;
    /* 0x18 */ float  w;
    /* 0x1c */ float  h;
    /* 0x20 */ float  x;
    /* 0x24 */ float  y;
    /* 0x28 */ int    pri;
    /* 0x2c */ u_char alpha;
};
static_assert(sizeof(SPRT_DAT2) == 0x30, "SPRT_DAT2 is 0x30 bytes on the PS2");

void CopySprDToSpr2(DISP_SPRT2 *s, const SPRT_DAT2 *d);     // FF3 0x001c9e98
void DispSprD2(DISP_SPRT2 *s);                              // FF3 0x001c9fe0
// PC: DispSprD2's host quad (corners around the screen centre, UVs 12.4) for
// draws that build their own packet (effect_sub.c's EffDispSprD2Direct).
void ReiPan_G2dHostSprite(const float *x2, const float *y2, const u_int *uu, const u_int *vv,
                          u_long tex0, u_long tex1, u_long clamp, int use_clamp, u_long prim,
                          u_int z, u_char r, u_char g, u_char b, u_char a);

// An untextured gouraud quad, ready for DispSqrD (no pointers: keeps its PS2
// size). Corners in screen pixels, 0..3 = top-left, top-right, bottom-left,
// bottom-right (triangle-strip order); one colour per corner, one alpha.
struct DISP_SQAR                    /* 0x80 */
{
    /* 0x00 */ u_int   att;
    /* 0x04 */ float   crx;
    /* 0x08 */ float   cry;
    /* 0x0c */ float   csx;
    /* 0x10 */ float   csy;
    /* 0x14 */ int     x[4];
    /* 0x24 */ int     y[4];
    /* 0x34 */ u_int   z;
    /* 0x38 */ float   scw;
    /* 0x3c */ float   sch;
    /* 0x40 */ float   rot;
    /* 0x48 */ u_long  texa;
    /* 0x50 */ u_long  alphar;
    /* 0x58 */ u_long  zbuf;
    /* 0x60 */ u_long  test;
    /* 0x68 */ u_int   pri;
    /* 0x6c */ u_char  r[4];
    /* 0x70 */ u_char  g[4];
    /* 0x74 */ u_char  b[4];
    /* 0x78 */ u_char  alpha;
};
static_assert(sizeof(DISP_SQAR) == 0x80, "DISP_SQAR is 0x80 bytes on the PS2");
static_assert(offsetof(DISP_SQAR, x) == 0x14, "DISP_SQAR.x");
static_assert(offsetof(DISP_SQAR, y) == 0x24, "DISP_SQAR.y");
static_assert(offsetof(DISP_SQAR, z) == 0x34, "DISP_SQAR.z");
static_assert(offsetof(DISP_SQAR, rot) == 0x40, "DISP_SQAR.rot");
static_assert(offsetof(DISP_SQAR, texa) == 0x48, "DISP_SQAR.texa");
static_assert(offsetof(DISP_SQAR, test) == 0x60, "DISP_SQAR.test");
static_assert(offsetof(DISP_SQAR, pri) == 0x68, "DISP_SQAR.pri");
static_assert(offsetof(DISP_SQAR, r) == 0x6c, "DISP_SQAR.r");
static_assert(offsetof(DISP_SQAR, b) == 0x74, "DISP_SQAR.b");
static_assert(offsetof(DISP_SQAR, alpha) == 0x78, "DISP_SQAR.alpha");

// Axis-aligned single-colour rectangle (CopySqrDToSqr's source).
struct SQAR_DAT                     /* 0x18 */
{
    /* 0x00 */ u_int   w;
    /* 0x04 */ u_int   h;
    /* 0x08 */ int     x;
    /* 0x0c */ int     y;
    /* 0x10 */ u_int   pri;
    /* 0x14 */ u_char  r;
    /* 0x15 */ u_char  g;
    /* 0x16 */ u_char  b;
    /* 0x17 */ u_char  alpha;
};
static_assert(sizeof(SQAR_DAT) == 0x18, "SQAR_DAT is 0x18 bytes on the PS2");
static_assert(offsetof(SQAR_DAT, pri) == 0x10, "SQAR_DAT.pri");
static_assert(offsetof(SQAR_DAT, alpha) == 0x17, "SQAR_DAT.alpha");

// Free four-corner single-colour quad (CopySq4DToSqr's source).
struct SQR4_DAT                     /* 0x28 */
{
    /* 0x00 */ int     x[4];
    /* 0x10 */ int     y[4];
    /* 0x20 */ u_int   pri;
    /* 0x24 */ u_char  r;
    /* 0x25 */ u_char  g;
    /* 0x26 */ u_char  b;
    /* 0x27 */ u_char  alpha;
};
static_assert(sizeof(SQR4_DAT) == 0x28, "SQR4_DAT is 0x28 bytes on the PS2");
static_assert(offsetof(SQR4_DAT, pri) == 0x20, "SQR4_DAT.pri");
static_assert(offsetof(SQR4_DAT, alpha) == 0x27, "SQR4_DAT.alpha");

void CopySqrDToSqr(DISP_SQAR *s, const SQAR_DAT *d);        // FF3 0x001c95d0
void CopySq4DToSqr(DISP_SQAR *s, const SQR4_DAT *d);        // FF3 0x001c97d8
void DispSqrD(DISP_SQAR *s);                                // FF3 0x001c99c0

// Module init (FF2's was empty; FF3's resets the fade module after it).
void InitG2DDraw(void);                                     // FF3 0x001c8e28
// Kick the packet buffer to the GS now (FF2 PK2DKick).
void PK2DKick(void);                                        // FF3 0x001cad48

// The direct (PATH2 FLUSH) packet: Start returns where to write GIF data,
// End closes it at `tail` (from MioPan g2d_draw.c: dmaVif1Get/SetPacketFLUSH_DIRECT).
qword *StartDmaDirectTrans(void);                           // FF3 0x001cad78
qword *EndDmaDirectTrans(qword *tail);                      // FF3 0x001cada8
// Image data (`qwc` qwords at `addr`) after a BITBLTBUF..TRXDIR packet.
void   SetDmaDirectImageTrans(const void *addr, int qwc);   // FF3 0x001cadf0

// One VRAM -> VRAM copy geometry of LocalCopyLtoL (FF2's struct, FF3's table
// of 7 at data 0x327438): the source page (TBP, width in 64s, log2 size) is
// drawn as one sprite of du x dv texels onto dw x dh pixels of the
// destination page. stbp / dtbp are rewritten on every call.
struct SCREEN_COPY_WRK              /* 0x28 */
{
    /* 0x00 */ int stbp;            // source TBP (block address)
    /* 0x04 */ int sfbw;            // source width / 64
    /* 0x08 */ int stw;             // source TW (log2)
    /* 0x0c */ int sth;             // source TH (log2)
    /* 0x10 */ int dtbp;            // destination FBP (block address / 32)
    /* 0x14 */ int dfbw;            // destination width / 64
    /* 0x18 */ int dw;              // destination pixels
    /* 0x1c */ int dh;
    /* 0x20 */ int du;              // source texels
    /* 0x24 */ int dv;
};
static_assert(sizeof(SCREEN_COPY_WRK) == 0x28, "SCREEN_COPY_WRK is 0x28 bytes on the PS2");
static_assert(offsetof(SCREEN_COPY_WRK, dtbp) == 0x10, "SCREEN_COPY_WRK.dtbp");
static_assert(offsetof(SCREEN_COPY_WRK, dw) == 0x18, "SCREEN_COPY_WRK.dw");
static_assert(offsetof(SCREEN_COPY_WRK, dv) == 0x24, "SCREEN_COPY_WRK.dv");

// Local image copies. type selects the geometry:
//   LtoB / BtoL (PSMCT32, 0x10ef000 or 0x117b000 in EE memory)
//     0 640x448 (7 strips of 64 lines)   2 320x448 (400 + 48 lines)
//     1 320x224                          3 640x224
//   LtoL (source -> destination, see SCREEN_COPY_WRK)
//     0 640x448 -> 640x448   3 640x448 -> 320x448   6 320x224 -> 320x224
//     1 640x448 -> 320x224   4 320x448 -> 640x448
//     2 320x224 -> 640x448   5 640x448 -> 640x224
// no picks the EE buffer, 0 = 0x10ef000, 1 = 0x117b000; addr / addr1 / addr2
// / v_adrs are GS block addresses (TBP units; the frame buffers are 0 and
// 0x1180). ee_adrs is an EE address or a host pointer (both work on PC).
void LocalCopyLtoBAdrs(int type, uintptr_t ee_adrs, int v_adrs); // FF3 0x001ca650: VRAM -> EE
void LocalCopyLtoB(int type, int no, int addr);             // FF3 0x001ca870: VRAM -> EE
void LocalCopyBtoLAdrs(int type, uintptr_t ee_adrs, int v_adrs); // FF3 0x001ca8a0: EE -> VRAM
void LocalCopyBtoL(int type, int no, int addr);             // FF3 0x001caa80: EE -> VRAM
void LocalCopyLtoL(int type, int addr1, int addr2);         // FF3 0x001caab0: VRAM -> VRAM

// PC only (MioPan's host bridge for the copies). Nothing draws into emulated
// GS memory, so a LOCAL -> HOST store has no pixels of its own: the renderer
// reads the presented frame back, and this registry says which screen
// rectangle a page holds and at what size (LocalCopyLtoL registers its
// destination itself; hand-built copies, e.g. the photo code's, call it).
// G2dForgetScreenPage: something else was written over the page.
// G2dHostUpload: re-issue a HOST -> LOCAL transfer whose packet nothing walks.
void G2dRegisterScreenPage(int block, int src_x, int src_y, int src_w, int src_h,
                           int dst_w, int dst_h);
void G2dForgetScreenPage(int block);
void G2dHostUpload(int dbp, int dbw, int dpsm, int dsax, int dsay, int w, int h,
                   void *src);
// G2dHostPageReadback: a page's pixels read back into EE memory now (the
// stash stores skip it); 0 when nothing could be read.
int  G2dHostPageReadback(int v_adrs, u_char *dst, int w, int h);

#endif /* _GRAPHICS_GRAPH2D_G2D_DRAW_H */
