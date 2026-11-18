/* ==========================================================================
 *  graphics/graph2d/g2d_debug.c
 *
 *  FF3's 2D primitive helpers (FF3-only code in FF2's g2d_debug.o slot):
 *
 *    * Squares   DispSqrRect / DispSqr4 build a DISP_SQAR through g2d_draw's
 *                CopySqrDToSqr / CopySq4DToSqr (from an unset record: every
 *                field the record feeds is overwritten) and draw it with
 *                DispSqrD; the Zmsk forms choose ZBUF's ZMSK.
 *    * Lines     SetLine2D sends ALPHA / TEST / ZBUF through the register
 *                cache and one LINE as a DIRECT packet; SetLine2DShade and
 *                SetCircle2D build on it. SetLine2DPacket is FF2's (graphics.c),
 *                packet only.
 *    * DispZbufView: a full-screen sprite reading a page as PSMZ16S.
 *
 *  Only DispSqrRect has callers; the rest is unreferenced debug drawing.
 *
 *  PC: the packets are built as FF3 builds them but nothing walks them; the
 *  lines are also drawn through the renderer (squares and sprites already are,
 *  by DispSqrD / DispSprD2).
 *
 *  FF3 0x1c83d0..0x1c8e28.
 * ======================================================================== */

#include "graphics/graph2d/g2d_debug.h"

#include <string.h>                     // memset
#include <libgraph.h>                   // SCE_GS_* / SCE_GIF_*

#include "graphics/graph2d/g2d_draw.h"  // DISP_SQAR, CopySq*DToSqr, DispSqrD, DispSprD2, Start/EndDmaDirectTrans
#include "graphics/gs_reg.h"            // GS_REG_SET, SetGsRegsCached
#include "reipan/rendering/reipan_renderer.h"   // PC: ReiPan_RendererDrawLine2D

// Callees in unassigned runs; the prototypes move to their headers when those
// TUs are defined.
// unassigned@001ef370 (math helpers after the pad driver)
float Vu0Atanf(float x);                                    // at 0x001ef620
float Vu0Sinf(float x);                                     // at 0x001ef758
float Vu0Cosf(float x);                                     // at 0x001ef838

// The GS window origin (XYOFFSET 1728,1824: 2048 + x lands on pixel x + 320)
// and the screen centre.
#define G2D_GS_ORG  2048.0f
#define G2D_CX      320.0f
#define G2D_CY      224.0f

#define G2D_PI      3.1415927f

// ZBUF of the squares without its ZMSK bit: ZBP 0x118, PSMZ16S.
#define G2D_SQR_ZBUF    SCE_GS_SET_ZBUF(0x118, SCE_GS_PSMZ16S & 0xf, 0)

// A single flat, blended LINE: RGBAQ, XYZ2, XYZ2.
#define G2D_LINE_PRIM   SCE_GS_SET_PRIM(SCE_GS_PRIM_LINE, 0, 0, 0, 1, 0, 0, 0, 0)
#define G2D_LINE_REGS   0x441

// VFTOI4: float to fixed point with 4 fraction bits, truncated.
static inline u_int G2dDbgFtoi4(float f) { return (u_int)(int)(f * 16.0f); }

// PC: draw a line through the renderer; xy in screen pixels.
static void G2dDbgHostLine(float x0, float y0, float x1, float y1, u_int z,
                           u_char r, u_char g, u_char b, u_char a)
{
    float  xy[4]   = {x0, y0, x1, y1};
    u_char rgba[8] = {r, g, b, a, r, g, b, a};

    ReiPan_RendererSetGs2dDepth(z);
    ReiPan_RendererDrawLine2D(xy, rgba, 1.0f);
}

// ──────────────────────────────────────────────────────────────────────
// Squares
// ──────────────────────────────────────────────────────────────────────

// Untextured rectangle (x0, y0)-(x1, y1): a DISP_SQAR with the corners in
// strip order, z from pri, ZMSK from zmsk, one colour. FF3 0x001c83d0.

void DispSqrRectZmsk(float x0, float y0, float x1, float y1, u_int pri, int zmsk,
                     u_char r, u_char g, u_char b, u_char a)
{
    SQAR_DAT  dat;
    DISP_SQAR sqr;
    float     w = x1 - x0;
    float     h = y1 - y0;
    int       i;

    // FF3 passes an unset record; zeroing it changes nothing below.
    memset(&dat, 0, sizeof(dat));
    CopySqrDToSqr(&sqr, &dat);

    sqr.x[0] = (int)x0;
    sqr.z    = 0xfffff - (pri & 0xfffff);
    sqr.zbuf = ((u_long)zmsk << 32) | G2D_SQR_ZBUF;
    sqr.y[0] = (int)y0;
    sqr.x[1] = sqr.x[0] + (int)w;
    sqr.y[2] = sqr.y[0] + (int)h;

    for (i = 0; i < 4; i++)
    {
        sqr.r[i] = r;
        sqr.b[i] = b;
        sqr.g[i] = g;
    }

    sqr.x[2]  = sqr.x[0];
    sqr.x[3]  = sqr.x[1];
    sqr.y[1]  = sqr.y[0];
    sqr.y[3]  = sqr.y[2];
    sqr.pri   = pri;
    sqr.alpha = a;
    DispSqrD(&sqr);
}

// FF3 0x001c8528.

void DispSqrRect(float x0, float y0, float x1, float y1, u_int pri,
                 u_char r, u_char g, u_char b, u_char a)
{
    DispSqrRectZmsk(x0, y0, x1, y1, pri, 1, r, g, b, a);
}

// Untextured free quad (corners in strip order), z from pri, ZMSK from zmsk.
// FF3 0x001c8560.

void DispSqr4Zmsk(float x0, float y0, float x1, float y1, float x2, float y2,
                  float x3, float y3, u_int pri, int zmsk,
                  u_char r, u_char g, u_char b, u_char a)
{
    SQR4_DAT  dat;
    DISP_SQAR sqr;
    int       i;

    // FF3 passes an unset record; zeroing it changes nothing below.
    memset(&dat, 0, sizeof(dat));
    CopySq4DToSqr(&sqr, &dat);

    sqr.pri  = pri;
    sqr.z    = 0xfffff - (pri & 0xfffff);
    sqr.zbuf = ((u_long)zmsk << 32) | G2D_SQR_ZBUF;
    sqr.x[0] = (int)x0;
    sqr.y[0] = (int)y0;
    sqr.x[1] = (int)x1;
    sqr.y[1] = (int)y1;
    sqr.x[2] = (int)x2;
    sqr.y[2] = (int)y2;
    sqr.x[3] = (int)x3;
    sqr.y[3] = (int)y3;

    for (i = 0; i < 4; i++)
    {
        sqr.r[i] = r;
        sqr.b[i] = b;
        sqr.g[i] = g;
    }

    sqr.alpha = a;
    DispSqrD(&sqr);
}

// FF3 0x001c86f8; unreferenced.

void DispSqr4(float x0, float y0, float x1, float y1, float x2, float y2,
              float x3, float y3, u_int pri,
              u_char r, u_char g, u_char b, u_char a)
{
    DispSqr4Zmsk(x0, y0, x1, y1, x2, y2, x3, y3, pri, 1, r, g, b, a);
}

// ──────────────────────────────────────────────────────────────────────
// Z buffer view
// ──────────────────────────────────────────────────────────────────────

// Full-screen 640x448 picture, half-pixel offset (rodata 0x359bc8).
static const SPRT_DAT2 g2d_zbuf_view_dat = {
    0,                          // tex0 (set below)
    0.0f, 0.0f, 640.0f, 448.0f, // u1 v1 u2 v2
    640.0f, 448.0f,             // w h
    -0.5f, -0.5f,               // x y
    0x10,                       // pri
    0x80,                       // alpha
};

// Draw the page at GS block tbp as a PSMZ16S texture (TBW 10, 1024x512, CLD
// 1) over the whole screen, behind everything. FF3 0x001c8730; unreferenced.

void DispZbufView(u_int tbp)
{
    SPRT_DAT2  dat = g2d_zbuf_view_dat;
    DISP_SPRT2 spr;

    CopySprDToSpr2(&spr, &dat);
    spr.z    = 0xfffff;
    spr.tex0 = SCE_GS_SET_TEX0(0, 10, SCE_GS_PSMZ16S, 10, 9, 0, 0, 0, 0, 0, 0, 1) | tbp;
    DispSprD2(&spr);
}

// ──────────────────────────────────────────────────────────────────────
// Lines
// ──────────────────────────────────────────────────────────────────────

// A 2D line (centre-relative coordinates): blend Cs*FIX + Cd*(1-FIX) with
// FIX = a, alpha test GREATER 0, depth GEQUAL without writes, then one
// flat LINE.
// FF3 0x001c87d0.

void SetLine2D(float x0, float y0, float x1, float y1, int pri,
               u_char r, u_char g, u_char b, u_char a)
{
    GS_REG_SET regs[3];
    int        mpri = (pri > 0) ? pri : 0x10;
    u_int      xx0  = G2dDbgFtoi4(x0 + G2D_GS_ORG);
    u_int      yy0  = G2dDbgFtoi4(y0 + G2D_GS_ORG);
    u_int      xx1  = G2dDbgFtoi4(x1 + G2D_GS_ORG);
    u_int      yy1  = G2dDbgFtoi4(y1 + G2D_GS_ORG);
    u_int      z    = 0xfffff - (mpri & 0xfffff);
    u_long    *p;
    u_int     *q;

    regs[0].data = SCE_GS_SET_ALPHA(0, 1, 2, 1, a);         regs[0].reg = SCE_GS_ALPHA_1;
    regs[1].data = SCE_GS_SET_TEST(1, 6, 0, 0, 0, 0, 1, 2); regs[1].reg = SCE_GS_TEST_1;
    regs[2].data = SCE_GS_SET_ZBUF(0x118, SCE_GS_PSMZ16S & 0xf, 1); regs[2].reg = SCE_GS_ZBUF_1;
    SetGsRegsCached(regs, 3, 1);

    // PC: the renderer draws it; the packet below is FF3's, unread.
    G2dDbgHostLine(x0 + G2D_CX, y0 + G2D_CY, x1 + G2D_CX, y1 + G2D_CY, z, r, g, b, a);

    p    = (u_long *)StartDmaDirectTrans();
    p[0] = SCE_GIF_SET_TAG(1, 1, 1, G2D_LINE_PRIM, SCE_GIF_PACKED, 3);
    p[1] = G2D_LINE_REGS;

    q     = (u_int *)&p[2];
    q[0]  = r;
    q[1]  = g;
    q[2]  = b;
    q[3]  = 0x80;
    q[4]  = xx0;
    q[5]  = yy0;
    q[6]  = z;
    q[7]  = 0;
    q[8]  = xx1;
    q[9]  = yy1;
    q[10] = z;
    q[11] = 0;

    EndDmaDirectTrans((qword *)&q[12]);
}

// SetLine2D with the colour scaled by the slope: the angle from vertical,
// atan((x0 - x1) / (y0 - y1)) in degrees, keeps full colour within +-30,
// half beyond +-45, and ramps linearly in between. FF3 0x001c8968;
// unreferenced.

void SetLine2DShade(float x0, float y0, float x1, float y1, int pri,
                    u_char r, u_char g, u_char b, u_char a)
{
    float ang = (Vu0Atanf((x0 - x1) / (y0 - y1)) * 180.0f) / G2D_PI;
    float f;

    if (45.0f < ang || ang < -45.0f)
    {
        f = 0.5f;
    }
    else if (30.0f < ang || ang < -30.0f)
    {
        if (ang < 0.0f)
        {
            ang = -ang;
        }
        f = ((45.0f - ang) * 0.5f) / 15.0f + 0.5f;
    }
    else
    {
        SetLine2D(x0, y0, x1, y1, pri, r, g, b, a);
        return;
    }

    SetLine2D(x0, y0, x1, y1, pri,
              (u_char)(int)((float)r * f), (u_char)(int)((float)g * f),
              (u_char)(int)((float)b * f), a);
}

// FF2's SetLine2DPacket (graphics.c): a LINE between two points in screen
// pixels, colour r g b a, z 1, drawn with whatever state is set. FF3
// 0x001c8b78; unreferenced.

void SetLine2DPacket(float x0, float y0, float x1, float y1,
                     u_char r, u_char g, u_char b, u_char a)
{
    u_int   itmp[4];
    u_long *p;
    u_int  *q;

    itmp[0] = G2dDbgFtoi4((x0 + G2D_GS_ORG) - G2D_CX);
    itmp[1] = G2dDbgFtoi4((x1 + G2D_GS_ORG) - G2D_CX);
    itmp[2] = G2dDbgFtoi4((y0 + G2D_GS_ORG) - G2D_CY);
    itmp[3] = G2dDbgFtoi4((y1 + G2D_GS_ORG) - G2D_CY);

    // PC: the renderer draws it; the packet below is FF3's, unread.
    G2dDbgHostLine(x0, y0, x1, y1, 1, r, g, b, a);

    p    = (u_long *)StartDmaDirectTrans();
    p[0] = SCE_GIF_SET_TAG(1, 1, 1, G2D_LINE_PRIM, SCE_GIF_PACKED, 3);
    p[1] = G2D_LINE_REGS;

    q     = (u_int *)&p[2];
    q[0]  = r;
    q[1]  = g;
    q[2]  = b;
    q[3]  = a;
    q[4]  = itmp[0];
    q[5]  = itmp[2];
    q[6]  = 1;
    q[7]  = 0;
    q[8]  = itmp[1];
    q[9]  = itmp[3];
    q[10] = 1;
    q[11] = 0;

    EndDmaDirectTrans((qword *)&q[12]);
}

// A circle outline around (cx, cy) (centre-relative) of radius rad: 36
// SetLine2D segments of 10 degrees, pri 0x10. FF3 0x001c8c98; unreferenced.

void SetCircle2D(float cx, float cy, float rad,
                 u_char r, u_char g, u_char b, u_char a)
{
    float deg = 0.0f;
    float a0, a1, x0, y0, x1, y1;

    while (deg < 360.0f)
    {
        a0   = (deg * G2D_PI) / 180.0f;
        deg += 10.0f;
        a1   = (deg * G2D_PI) / 180.0f;

        x0 = rad * Vu0Cosf(a0) + cx;
        y0 = rad * Vu0Sinf(a0) + cy;
        x1 = rad * Vu0Cosf(a1) + cx;
        y1 = rad * Vu0Sinf(a1) + cy;
        SetLine2D(x0, y0, x1, y1, 0x10, r, g, b, a);
    }
}
