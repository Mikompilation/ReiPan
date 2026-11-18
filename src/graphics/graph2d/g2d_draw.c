/* ==========================================================================
 *  graphics/graph2d/g2d_draw.c
 *
 *  The 2D primitive layer (FF2 g2d_draw.c, ported from MioPan and checked
 *  against FF3 function by function).
 *
 *    * Sprites   DISP_SPRT / DISP_SPRT2 (textured quads). CopySprDToSpr(2)
 *                fold a compact record into the full descriptor with the
 *                fixed GS register set; DispSprD(2) scale and rotate the four
 *                corners and send one TRISTRIP packet.
 *    * Squares   DISP_SQAR (untextured gouraud quad): CopySqrDToSqr /
 *                CopySq4DToSqr feed DispSqrD.
 *    * Packets   FF3 dropped FF2's PK2D ring: draw state goes through the GS
 *                register cache (SetGsRegsCached), geometry through the
 *                packet buffer as DIRECT packets (Start/EndDmaDirectTrans).
 *    * LocalCopy GS LOCAL <-> EE / LOCAL -> LOCAL image copies; on PC through
 *                MioPan's host bridge (renderer capture slots, frame read-back,
 *                re-issued uploads).
 *
 *  FF3 differences from FF2: no interlace halving of Y (gInterlace is gone),
 *  only PSMT4 gets the T8-with-CLD texture load (FF2 also T4HL/T4HH), DispSqrD
 *  computes sin/cos even when the angle is 0, Vu0Sinf/Vu0Cosf replace sinf/
 *  cosf, no GSQR/GSQ4 copy functions, no SetPanel.
 *
 *  PC: the packets are still built as FF3 builds them, but nothing walks the
 *  chain; each Disp* also draws its quad through the renderer (MioPan's
 *  bridge), after SetGsRegsCached has shadowed ALPHA/TEST/ZBUF into it.
 *
 *  FF3 0x1c8e28..0x1cae98.
 * ======================================================================== */

#include "common/reipan_stub.h"
#include "graphics/graph2d/g2d_draw.h"

#include <string.h>                     // memset
#include <eekernel.h>                   // FlushCache
#include <libgraph.h>                   // SCE_GS_* / SCE_GIF_*, sceGs*Image, sceGsSyncPath

#include "graphics/gs_reg.h"            // GS_REG_SET, SetGsRegsCached
#include "reipan/gs/reipan_gs_c.h"     // PC: ReiPan_GsUpload
#include "reipan/reipan_memory.h"       // PC: ReiPan_GetHostPointer, ReiPan_GetPs2Address
#include "reipan/rendering/reipan_renderer.h"   // PC: the quad bridge
#include "system/os/system.h"           // GetPacketBufMgr, PACKET_BUF_MGR::GetPacket / SetPacket
#include "graphics/graph2d/fade.h"
#include "graphics/effect/effect_sub.h" // PC: EffHostDrawFrameRedirected, EffHostTexIsWorkFrame

// Callees in unassigned runs; the prototypes move to their headers when those
// TUs are defined.
// unassigned@001ef370 (math helpers after the pad driver)
float Vu0Sinf(float x);                                     // at 0x001ef758
float Vu0Cosf(float x);                                     // at 0x001ef838

// ──────────────────────────────────────────────────────────────────────
// Shared constants
// ──────────────────────────────────────────────────────────────────────

// The fixed 2D draw state every Copy* writes: TEXA AEM with TA1 0x80, normal
// blend with fixed alpha 0x80, Z writes masked, alpha and depth tests ALWAYS.
#define G2D_TEXA    SCE_GS_SET_TEXA(0, 1, 0x80)
#define G2D_ALPHA   SCE_GS_SET_ALPHA(0, 1, 0, 1, 0x80)
#define G2D_ZBUF    SCE_GS_SET_ZBUF(0x118, SCE_GS_PSMZ16S & 0xf, 1)
#define G2D_TEST    SCE_GS_SET_TEST(1, 1, 0, 0, 0, 0, 1, 1)
// Textured, blended, UV-addressed TRISTRIP (0x154).
#define G2D_SPR_PRIM SCE_GS_SET_PRIM(SCE_GS_PRIM_TRISTRIP, 0, 1, 0, 1, 0, 1, 0, 0)
// CLAMP_1 for DispSprD: CLAMP / CLAMP (5).
#define G2D_CLAMP   SCE_GS_SET_CLAMP(1, 1, 0, 0, 0, 0)

// The screen centre the corners are taken around, and the GS window origin
// (XYOFFSET is 1728,1824, so 2048 + x - 320 lands on pixel x).
#define G2D_CX      320.0f
#define G2D_CY      224.0f
#define G2D_GS_ORG  2048.0f

// Per-vertex register list of the sprite packets: UV, RGBAQ, XYZF2 x4
// (rodata 0x370980 / 0x3709e0).
#define G2D_SPR_REGS 0x413413413413ULL

// VFTOI4 / VFTOI0: float to fixed point with 4 / 0 fraction bits, truncated.
static inline u_int G2dFtoi4(float f) { return (u_int)(int)(f * 16.0f); }
static inline u_int G2dFtoi0(float f) { return (u_int)(int)f; }

// The register pairs DispSprD / DispSprD2 send before their packet (one
// SetGsRegsCached array: ALPHA_1, TEX1_1, CLAMP_1, TEST_1, ZBUF_1, TEXA).
static void G2dSetSprRegs(u_long alpha, u_long tex1, u_long clamp, u_long test, u_long zbuf,
                          u_long texa)
{
    GS_REG_SET regs[6];

    regs[0].data = alpha;   regs[0].reg = SCE_GS_ALPHA_1;
    regs[1].data = tex1;    regs[1].reg = SCE_GS_TEX1_1;
    regs[2].data = clamp;   regs[2].reg = SCE_GS_CLAMP_1;
    regs[3].data = test;    regs[3].reg = SCE_GS_TEST_1;
    regs[4].data = zbuf;    regs[4].reg = SCE_GS_ZBUF_1;
    regs[5].data = texa;    regs[5].reg = SCE_GS_TEXA;
    SetGsRegsCached(regs, 6, 1);
}

// The sprite packet both DispSprD and DispSprD2 send: an A+D part (TEXFLUSH,
// TEX0_1 twice or once + pad, TEXCLUT) and one TRISTRIP of four {UV, RGBAQ,
// XYZF2}. A PSMT4 texture is first named as PSMT8 with CLD 1 so the CLUT
// loads, then as itself with CLD/CSA cleared.
static void G2dSendSprPacket(u_long tex0, u_int col, u_long prim, const u_int *uu,
                             const u_int *vv, const u_int *xx, const u_int *yy, u_int z,
                             u_char r, u_char g, u_char b, u_char a)
{
    u_long *p = (u_long *)StartDmaDirectTrans();
    u_int  *q;
    int     i;

    p[0] = SCE_GIF_SET_TAG(4, 1, 0, 0, SCE_GIF_PACKED, 1);
    p[1] = SCE_GIF_PACKED_AD;
    p[2] = 0;
    p[3] = SCE_GS_TEXFLUSH;

    if (((tex0 >> 20) & 0x3f) == SCE_GS_PSMT4)
    {
        p[4] = (tex0 & 0x00fffffffc0fffffULL) | ((u_long)SCE_GS_PSMT8 << 20)
             | ((u_long)SCE_GS_CLD_LOAD << 61);
        p[5] = SCE_GS_TEX0_1;
        p[6] = tex0 & 0x00ffffffffffffffULL;
        p[7] = SCE_GS_TEX0_1;
    }
    else
    {
        p[4] = tex0;
        p[5] = SCE_GS_TEX0_1;
        p[6] = 0;
        p[7] = 0x7f;                        // A+D to 0x7f: no register (pad)
    }

    p[8]  = SCE_GS_SET_TEXCLUT(4, 0, col & 0xff);
    p[9]  = SCE_GS_TEXCLUT;
    p[10] = SCE_GIF_SET_TAG(1, 1, 1, prim, SCE_GIF_PACKED, 12);
    p[11] = G2D_SPR_REGS;

    q = (u_int *)&p[12];
    for (i = 0; i < 4; i++, q += 12)
    {
        q[0]  = uu[i];
        q[1]  = vv[i];
        q[2]  = 0;
        q[3]  = 0;
        q[4]  = r;
        q[5]  = g;
        q[6]  = b;
        q[7]  = a;
        q[8]  = xx[i];
        q[9]  = yy[i];
        q[10] = z;
        q[11] = 0;
    }

    EndDmaDirectTrans((qword *)q);
}

// PC: the quad on the renderer (MioPan's DispSprD bridge). A primitive
// without TME is a flat blended fill, not an image. `clamp` < 0: the
// renderer's default clamp.
static void G2dHostSprite(const float *x2, const float *y2, const u_int *uu, const u_int *vv,
                          u_long tex0, u_long tex1, u_long clamp, int use_clamp, u_long prim,
                          u_int z, u_char r, u_char g, u_char b, u_char a)
{
    float xy[8];
    float uv[8];
    int   i;

    // A work frame is the GS target (effect_sub.c's EffSetDrawFrame), or the
    // texture is one only such GS-side passes wrote: no host equivalent.
    if (EffHostDrawFrameRedirected() || ((prim & 0x10) != 0 && EffHostTexIsWorkFrame(tex0)))
        return;

    for (i = 0; i < 4; i++)
    {
        xy[i * 2 + 0] = x2[i] + G2D_CX;
        xy[i * 2 + 1] = y2[i] + G2D_CY;
        uv[i * 2 + 0] = (float)uu[i] / 16.0f;
        uv[i * 2 + 1] = (float)vv[i] / 16.0f;
    }

    ReiPan_RendererSetGs2dDepth(z);
    if ((prim & 0x10) == 0)
    {
        u_char rgba[16];

        for (i = 0; i < 4; i++)
        {
            rgba[i * 4 + 0] = r;
            rgba[i * 4 + 1] = g;
            rgba[i * 4 + 2] = b;
            rgba[i * 4 + 3] = a;
        }
        ReiPan_RendererDrawSolidQuad(xy, rgba);
    }
    else if (use_clamp)
    {
        ReiPan_RendererDrawTexturedQuadClamp((const sceGsTex0 *)&tex0, (const sceGsTex1 *)&tex1,
                                             xy, uv, clamp, r, g, b, a);
    }
    else
    {
        ReiPan_RendererDrawTexturedQuad((const sceGsTex0 *)&tex0, (const sceGsTex1 *)&tex1,
                                        xy, uv, r, g, b, a);
    }
}

// PC: the same host quad for effect_sub.c's EffDispSprD2Direct, which sends
// a DISP_SPRT2 as DIRECT column strips.
void ReiPan_G2dHostSprite(const float *x2, const float *y2, const u_int *uu, const u_int *vv,
                          u_long tex0, u_long tex1, u_long clamp, int use_clamp, u_long prim,
                          u_int z, u_char r, u_char g, u_char b, u_char a)
{
    G2dHostSprite(x2, y2, uu, vv, tex0, tex1, clamp, use_clamp, prim, z, r, g, b, a);
}

// ──────────────────────────────────────────────────────────────────────
// Init
// ──────────────────────────────────────────────────────────────────────

// FF2's was empty; FF3's resets the fade module that follows this file.
// Called from IngameInit's reset list. FF3 0x001c8e28.

void InitG2DDraw(void)
{
    FadeCtrlInit();
}

// ──────────────────────────────────────────────────────────────────────
// Sprites
// ──────────────────────────────────────────────────────────────────────

// Fold a SPRT_DAT into a DISP_SPRT: picture rectangle and flip from the
// record, centre/scale/rotation reset, the fixed 2D register set (MMAG from
// bln), white colour. FF3 0x001c8e40.

void CopySprDToSpr(DISP_SPRT *s, const SPRT_DAT *d)
{
    s->att    = d->flip;
    s->u      = d->u;
    s->v      = d->v;
    s->w      = d->w;
    s->h      = d->h;
    s->x      = (float)d->x;
    s->y      = (float)d->y;
    s->crx    = 0.0f;
    s->cry    = 0.0f;
    s->csx    = 0.0f;
    s->csy    = 0.0f;
    s->scw    = 1.0f;
    s->sch    = 1.0f;
    s->z      = 0xfffff - (d->pri & 0xfffff);
    s->rot    = 0.0f;
    s->tex0   = d->tex0;
    s->texa   = G2D_TEXA;
    s->tex1   = SCE_GS_SET_TEX1(1, 0, d->bln, 5, 0, 0, 0);
    s->alphar = G2D_ALPHA;
    s->test   = G2D_TEST;
    s->zbuf   = G2D_ZBUF;
    s->gftg   = G2D_SPR_PRIM;
    s->pri    = d->pri;
    s->b      = 0x80;
    s->r      = 0x80;
    s->g      = 0x80;
    s->col    = 0;
    s->alpha  = d->alpha;
}

// Draw a DISP_SPRT. The corners are taken around the screen centre, scaled
// about (csx, csy), rotated by rot degrees about (crx, cry), then sent in GS
// window coordinates; UVs from u, v, w, h, mirrored by att bit 1 (U) and
// bit 0 (V). FF3 0x001c8f48 .
// callcheck: SetGsRegsCached, StartDmaDirectTrans, EndDmaDirectTrans (in
// G2dSetSprRegs / G2dSendSprPacket, which FF3 inlines).

void DispSprD(DISP_SPRT *s)
{
    u_int  matt  = s->att;
    u_int  mu    = s->u;
    u_int  mv    = s->v;
    float  mw    = (float)(int)s->w;
    float  mh    = (float)(int)s->h;
    float  mcrx  = s->crx - G2D_CX;
    float  mcry  = s->cry - G2D_CY;
    float  mcsx  = s->csx - G2D_CX;
    float  mcsy  = s->csy - G2D_CY;
    float  mrot  = (s->rot * 3.1415927f) / 180.0f;
    float  msw   = s->scw;
    float  msh   = s->sch;
    u_long mtex0 = s->tex0;
    u_long mtex1 = s->tex1;
    u_long mgftg = s->gftg;
    u_int  mz    = s->z;
    u_char mr    = s->r;
    u_char mg    = s->g;
    u_char mb    = s->b;
    u_char ma    = s->alpha;
    float  x[4], y[4], x2[4], y2[4];
    u_int  xx[4], yy[4], uu[4], vv[4];
    float  ss, cc;
    int    i;

    x[2] = s->x - G2D_CX;
    y[1] = s->y - G2D_CY;

    x[1] = x[2] + mw;
    if (msw == 1.0f)
    {
        x[0] = x[2];
    }
    else
    {
        x[0] = (x[2] - mcsx) * msw + mcsx;
        x[1] = (x[1] - mcsx) * msw + mcsx;
        x[2] = x[0];
    }
    x[3] = x[1];

    y[2] = y[1] + mh;
    if (msh == 1.0f)
    {
        y[0] = y[1];
    }
    else
    {
        y[0] = (y[1] - mcsy) * msh + mcsy;
        y[2] = (y[2] - mcsy) * msh + mcsy;
        y[1] = y[0];
    }
    y[3] = y[2];

    for (i = 0; i < 4; i++)
    {
        x[i] -= mcrx;
        y[i] -= mcry;
    }

    if (mrot == 0.0f)
    {
        for (i = 0; i < 4; i++)
        {
            x2[i] = x[i];
            y2[i] = y[i];
        }
    }
    else
    {
        ss = Vu0Sinf(mrot);
        cc = Vu0Cosf(mrot);
        for (i = 0; i < 4; i++)
        {
            x2[i] = x[i] * cc - y[i] * ss;
            y2[i] = y[i] * cc + x[i] * ss;
        }
    }

    for (i = 0; i < 4; i++)
    {
        x2[i] += mcrx;
        y2[i] += mcry;
    }

    for (i = 0; i < 4; i++)
    {
        xx[i] = G2dFtoi4(x2[i] + G2D_GS_ORG);
        yy[i] = G2dFtoi4(y2[i] + G2D_GS_ORG);
    }

    uu[3] = (G2dFtoi0(mw) + mu) * 16;
    vv[3] = (G2dFtoi0(mh) + mv) * 16;
    uu[2] = mu << 4;
    if (matt & 2)
    {
        uu[2] = uu[3];
        uu[3] = mu << 4;
    }
    vv[1] = mv << 4;
    if (matt & 1)
    {
        vv[1] = vv[3];
        vv[3] = mv << 4;
    }
    uu[0] = uu[2];
    uu[1] = uu[3];
    vv[0] = vv[1];
    vv[2] = vv[3];

    G2dSetSprRegs(s->alphar, mtex1, G2D_CLAMP, s->test, s->zbuf, s->texa);

    // PC: the renderer draws it; the packet below is FF3's, unread.
    G2dHostSprite(x2, y2, uu, vv, mtex0, mtex1, 0, 0, mgftg, mz, mr, mg, mb, ma);

    G2dSendSprPacket(mtex0, s->col, mgftg, uu, vv, xx, yy, mz, mr, mg, mb, ma);
}

// Fold a SPRT_DAT2 (float UV corners, float size) into a DISP_SPRT2: UVs
// to 12.4 fixed point, the fixed register set with bilinear MMAG and
// CLAMP/CLAMP, white colour. FF3 0x001c9e98.

void CopySprDToSpr2(DISP_SPRT2 *s, const SPRT_DAT2 *d)
{
    s->att    = 0;
    s->u1     = (u_short)(int)(d->u1 * 16.0f);
    s->v1     = (u_short)(int)(d->v1 * 16.0f);
    s->u2     = (u_short)(int)(d->u2 * 16.0f);
    s->v2     = (u_short)(int)(d->v2 * 16.0f);
    s->w      = d->w;
    s->h      = d->h;
    s->x      = d->x;
    s->y      = d->y;
    s->crx    = 0.0f;
    s->cry    = 0.0f;
    s->csx    = 0.0f;
    s->csy    = 0.0f;
    s->scw    = 1.0f;
    s->sch    = 1.0f;
    s->z      = 0xfffff - (d->pri & 0xfffff);
    s->rot    = 0.0f;
    s->tex1   = SCE_GS_SET_TEX1(1, 0, 1, 5, 0, 0, 0);
    s->tex0   = d->tex0;
    s->texa   = G2D_TEXA;
    s->alpreg = G2D_ALPHA;
    s->zbuf   = G2D_ZBUF;
    s->test   = G2D_TEST;
    s->clmp   = G2D_CLAMP;
    s->gftg   = G2D_SPR_PRIM;
    s->b      = 0x80;
    s->pri    = d->pri;
    s->r      = 0x80;
    s->g      = 0x80;
    s->col    = 0;
    s->alp    = d->alpha;
}

// Draw a DISP_SPRT2: as DispSprD, but the size is float, the scale keeps
// (csx, csy) fixed from the other side, the UV corners are given (12.4,
// swapped by att bits 1 / 0) and the sprite's own CLAMP is sent.
// FF3 0x001c9fe0 .
// callcheck: SetGsRegsCached, StartDmaDirectTrans, EndDmaDirectTrans (in
// G2dSetSprRegs / G2dSendSprPacket, which FF3 inlines).

void DispSprD2(DISP_SPRT2 *s)
{
    u_int  matt  = s->att;
    float  mcrx  = s->crx - G2D_CX;
    float  mcry  = s->cry - G2D_CY;
    float  mcsx  = s->csx - G2D_CX;
    float  mcsy  = s->csy - G2D_CY;
    float  mrot  = (s->rot * 3.1415927f) / 180.0f;
    float  msw   = s->scw;
    float  msh   = s->sch;
    u_long mtex0 = s->tex0;
    u_long mtex1 = s->tex1;
    u_long mclmp = s->clmp;
    u_long mgftg = s->gftg;
    u_int  mz    = s->z;
    u_char mr    = s->r;
    u_char mg    = s->g;
    u_char mb    = s->b;
    u_char ma    = s->alp;
    float  x[4], y[4], x2[4], y2[4];
    u_int  xx[4], yy[4], uu[4], vv[4];
    float  ss, cc;
    int    i;

    x[2] = s->x - G2D_CX;
    y[1] = s->y - G2D_CY;

    if (msw == 1.0f)
    {
        x[1] = x[2] + s->w * msw;
        x[0] = x[2];
    }
    else
    {
        x[0] = mcsx - (mcsx - x[2]) * msw;
        x[1] = mcsx + ((x[2] + s->w) - mcsx) * msw;
        x[2] = x[0];
    }
    x[3] = x[1];

    if (msh == 1.0f)
    {
        y[2] = y[1] + s->h * msh;
        y[0] = y[1];
    }
    else
    {
        y[0] = mcsy - (mcsy - y[1]) * msh;
        y[2] = mcsy + ((y[1] + s->h) - mcsy) * msh;
        y[1] = y[0];
    }
    y[3] = y[2];

    for (i = 0; i < 4; i++)
    {
        x[i] -= mcrx;
        y[i] -= mcry;
    }

    if (mrot == 0.0f)
    {
        for (i = 0; i < 4; i++)
        {
            x2[i] = x[i];
            y2[i] = y[i];
        }
    }
    else
    {
        ss = Vu0Sinf(mrot);
        cc = Vu0Cosf(mrot);
        for (i = 0; i < 4; i++)
        {
            x2[i] = x[i] * cc - y[i] * ss;
            y2[i] = y[i] * cc + x[i] * ss;
        }
    }

    for (i = 0; i < 4; i++)
    {
        x2[i] += mcrx;
        y2[i] += mcry;
    }

    for (i = 0; i < 4; i++)
    {
        xx[i] = G2dFtoi4(x2[i] + G2D_GS_ORG);
        yy[i] = G2dFtoi4(y2[i] + G2D_GS_ORG);
    }

    if (matt & 2)
    {
        uu[2] = s->u2;
        uu[3] = s->u1;
    }
    else
    {
        uu[2] = s->u1;
        uu[3] = s->u2;
    }
    vv[1] = s->v1;
    vv[3] = s->v2;
    if (matt & 1)
    {
        vv[1] = s->v2;
        vv[3] = s->v1;
    }
    uu[0] = uu[2];
    uu[1] = uu[3];
    vv[0] = vv[1];
    vv[2] = vv[3];

    G2dSetSprRegs(s->alpreg, mtex1, mclmp, s->test, s->zbuf, s->texa);

    // PC: the renderer draws it, with the sprite's CLAMP (effects tile noise
    // sheets with REPEAT); the packet below is FF3's, unread.
    G2dHostSprite(x2, y2, uu, vv, mtex0, mtex1, mclmp, 1, mgftg, mz, mr, mg, mb, ma);

    G2dSendSprPacket(mtex0, s->col, mgftg, uu, vv, xx, yy, mz, mr, mg, mb, ma);
}

// ──────────────────────────────────────────────────────────────────────
// Squares
// ──────────────────────────────────────────────────────────────────────

// Fold an axis-aligned single-colour rectangle into a DISP_SQAR.
// FF3 0x001c95d0.

void CopySqrDToSqr(DISP_SQAR *s, const SQAR_DAT *d)
{
    int i;

    s->att    = 0;
    s->crx    = 0.0f;
    s->cry    = 0.0f;
    s->csx    = 0.0f;
    s->csy    = 0.0f;
    s->x[0]   = d->x;
    s->x[2]   = d->x;
    s->x[1]   = d->x + d->w;
    s->x[3]   = d->x + d->w;
    s->y[0]   = d->y;
    s->y[1]   = d->y;
    s->y[2]   = d->y + d->h;
    s->y[3]   = d->y + d->h;
    s->scw    = 1.0f;
    s->sch    = 1.0f;
    s->z      = 0xfffff - (d->pri & 0xfffff);
    s->rot    = 0.0f;
    s->texa   = G2D_TEXA;
    s->alphar = G2D_ALPHA;
    s->zbuf   = G2D_ZBUF;
    s->test   = G2D_TEST;
    s->pri    = d->pri;

    for (i = 0; i < 4; i++)
    {
        s->r[i] = d->r;
        s->g[i] = d->g;
        s->b[i] = d->b;
    }

    s->alpha = d->alpha;
}

// Fold a free four-corner single-colour quad into a DISP_SQAR.
// FF3 0x001c97d8; only DispSqrRect calls it.

void CopySq4DToSqr(DISP_SQAR *s, const SQR4_DAT *d)
{
    int i;

    s->att = 0;
    s->crx = 0.0f;
    s->cry = 0.0f;
    s->csx = 0.0f;
    s->csy = 0.0f;

    for (i = 0; i < 4; i++)
    {
        s->x[i] = d->x[i];
        s->y[i] = d->y[i];
    }

    s->rot    = 0.0f;
    s->z      = 0xfffff - (d->pri & 0xfffff);
    s->alphar = G2D_ALPHA;
    s->sch    = 1.0f;
    s->texa   = G2D_TEXA;
    s->scw    = 1.0f;
    s->test   = G2D_TEST;
    s->zbuf   = G2D_ZBUF;
    s->pri    = d->pri;

    for (i = 0; i < 4; i++)
    {
        s->r[i] = d->r;
        s->g[i] = d->g;
        s->b[i] = d->b;
    }

    s->alpha = d->alpha;
}

// Draw a DISP_SQAR: the corners scaled about (csx, csy) and rotated about
// (crx, cry) as DispSprD does, then one gouraud TRISTRIP of four {RGBAQ,
// XYZF2} after ALPHA_1 / TEST_1 / ZBUF_1 / TEXA. FF3 0x001c99c0.

void DispSqrD(DISP_SQAR *s)
{
    GS_REG_SET regs[4];
    float      mcrx = s->crx - G2D_CX;
    float      mcry = s->cry - G2D_CY;
    float      mcsx = s->csx - G2D_CX;
    float      mcsy = s->csy - G2D_CY;
    float      msw  = s->scw;
    float      msh  = s->sch;
    float      rot  = s->rot;
    u_int      mz   = s->z;
    u_char     ma   = s->alpha;
    u_char     mr[4], mg[4], mb[4];
    float      mx[4], my[4], x[4], y[4], x2[4], y2[4];
    u_int      xx[4], yy[4];
    float      mrot, ss, cc;
    u_long    *p;
    u_int     *q;
    int        i;

    for (i = 0; i < 4; i++)
    {
        mx[i] = (float)s->x[i] - G2D_CX;
        my[i] = (float)s->y[i] - G2D_CY;
    }

    for (i = 0; i < 4; i++)
    {
        mr[i] = s->r[i];
        mg[i] = s->g[i];
        mb[i] = s->b[i];
    }

    for (i = 0; i < 4; i++)
    {
        x[i] = (msw == 1.0f) ? mx[i] : (mx[i] - mcsx) * msw + mcsx;
    }
    for (i = 0; i < 4; i++)
    {
        y[i] = (msh == 1.0f) ? my[i] : (my[i] - mcsy) * msh + mcsy;
    }

    mrot = (rot * 3.1415927f) / 180.0f;
    ss   = Vu0Sinf(mrot);
    cc   = Vu0Cosf(mrot);

    for (i = 0; i < 4; i++)
    {
        x[i] -= mcrx;
        y[i] -= mcry;
    }

    if (rot == 0.0f)
    {
        for (i = 0; i < 4; i++)
        {
            x2[i] = x[i];
            y2[i] = y[i];
        }
    }
    else
    {
        for (i = 0; i < 4; i++)
        {
            x2[i] = x[i] * cc - y[i] * ss;
            y2[i] = y[i] * cc + x[i] * ss;
        }
    }

    for (i = 0; i < 4; i++)
    {
        x2[i] += mcrx;
        y2[i] += mcry;
    }

    for (i = 0; i < 4; i++)
    {
        xx[i] = G2dFtoi4(x2[i] + G2D_GS_ORG);
        yy[i] = G2dFtoi4(y2[i] + G2D_GS_ORG);
    }

    regs[0].data = s->alphar;   regs[0].reg = SCE_GS_ALPHA_1;
    regs[1].data = s->test;     regs[1].reg = SCE_GS_TEST_1;
    regs[2].data = s->zbuf;     regs[2].reg = SCE_GS_ZBUF_1;
    regs[3].data = s->texa;     regs[3].reg = SCE_GS_TEXA;
    SetGsRegsCached(regs, 4, 1);

    // PC: the renderer draws it; the packet below is FF3's, unread.
    {
        float  xy[8];
        u_char rgba[16];

        for (i = 0; i < 4; i++)
        {
            xy[i * 2 + 0]   = x2[i] + G2D_CX;
            xy[i * 2 + 1]   = y2[i] + G2D_CY;
            rgba[i * 4 + 0] = mr[i];
            rgba[i * 4 + 1] = mg[i];
            rgba[i * 4 + 2] = mb[i];
            rgba[i * 4 + 3] = ma;
        }
        ReiPan_RendererSetGs2dDepth(mz);
        ReiPan_RendererDrawSolidQuad(xy, rgba);
    }

    // Gouraud, blended TRISTRIP (rodata 0x3709b0); RGBAQ, XYZF2 x4.
    p    = (u_long *)StartDmaDirectTrans();
    p[0] = SCE_GIF_SET_TAG(1, 1, 1, SCE_GS_SET_PRIM(SCE_GS_PRIM_TRISTRIP, 1, 0, 0, 1, 0, 0, 0, 0),
                           SCE_GIF_PACKED, 8);
    p[1] = 0x41414141;

    q = (u_int *)&p[2];
    for (i = 0; i < 4; i++, q += 8)
    {
        q[0] = mr[i];
        q[1] = mg[i];
        q[2] = mb[i];
        q[3] = ma;
        q[4] = xx[i];
        q[5] = yy[i];
        q[6] = mz;
        q[7] = 0;
    }

    EndDmaDirectTrans((qword *)q);
}

// ──────────────────────────────────────────────────────────────────────
// Local image copies: host bridge (PC only)
// ──────────────────────────────────────────────────────────────────────
//
// MioPan's bridge, with FF3's addresses. Nothing walks the packets the copies
// build and nothing draws into emulated GS memory, so on PC:
//   * a copy out of a frame buffer into a GS page or an EE stash becomes a
//     renderer capture slot keyed by that address (an effect or
//     ScreenCaptureDisp that later samples the page is served from it; the
//     slot's size stands in for the scaling, EffImageHalf32 included);
//   * a LOCAL -> HOST store that a caller reads on the CPU (the photo code
//     reads stash 1 after LtoL 5 + LtoB 3 of page 0x2bc0) gets the presented
//     frame read back, with the screen-page registry saying which rectangle
//     the page holds;
//   * a HOST -> LOCAL transfer is re-issued to emulated GS memory.
// A copy whose destination is a frame buffer (photo_dat's LtoL 0 of the
// shown buffer onto the drawn one) is left alone, as in MioPan.

// GS block addresses of the two 640x448 display buffers (FRAME FBP 0 / 0x8c).
#define G2D_IS_FRAMEBUFFER_BLOCK(a) ((a) == 0x0000 || (a) == 0x1180)

// LocalCopyLtoB / LocalCopyBtoL's two EE buffers (FF2 0x1e79b00 / 0x1f05b00).
#define G2D_EE_STASH_0  0x10ef000
#define G2D_EE_STASH_1  0x117b000
// PC: the third effect stash, FF3's: the haze (effect_obj.c) and the camera
// hit's still (effect_ene.c) store GS 0x38e0 here and restore it to 0x2bc0
// through the Adrs forms, read by no CPU code. Carried slot to slot like the
// two above, so the still is the whole window at the output's scale rather
// than a 320 x 224 read-back of the 640 x 448 frame.
#define G2D_EE_STASH_2  0x11c1000
#define G2D_IS_EE_STASH(a) ((a) == G2D_EE_STASH_0 || (a) == G2D_EE_STASH_1 || (a) == G2D_EE_STASH_2)

// Destination size of a LocalCopyLtoB / LocalCopyBtoL type (the strips the
// two builders send).
static int LocalCopyHostBufSize(int type, int *out_w, int *out_h)
{
    switch (type)
    {
    case 0: *out_w = 640; *out_h = 448; return 1;
    case 1: *out_w = 320; *out_h = 224; return 1;
    case 2: *out_w = 320; *out_h = 448; return 1;
    case 3: *out_w = 640; *out_h = 224; return 1;
    default: return 0;
    }
}

// The screen-page registry: which 640x448 screen rectangle a GS page holds
// and at what size it was drawn into the page's top-left corner. Fixed and
// small; the oldest entry is recycled.
// layout: host (PC-only bookkeeping, never in EE memory)
struct G2D_SCREEN_PAGE
{
    int   block;                        // GS block address, -1 when free
    int   src_x, src_y, src_w, src_h;   // screen rect, 640x448 units
    int   dst_w, dst_h;
    u_int seq;
};

#define G2D_SCREEN_PAGE_MAX 8

static G2D_SCREEN_PAGE g2d_screen_page[G2D_SCREEN_PAGE_MAX];
static u_int           g2d_screen_page_seq;
static int             g2d_screen_page_init;

static G2D_SCREEN_PAGE *G2dScreenPageFind(int block)
{
    int i;

    if (!g2d_screen_page_init)
    {
        for (i = 0; i < G2D_SCREEN_PAGE_MAX; i++)
        {
            g2d_screen_page[i].block = -1;
        }
        g2d_screen_page_init = 1;
    }
    for (i = 0; i < G2D_SCREEN_PAGE_MAX; i++)
    {
        if (g2d_screen_page[i].block == block)
        {
            return &g2d_screen_page[i];
        }
    }
    return NULL;
}

void G2dRegisterScreenPage(int block, int src_x, int src_y, int src_w, int src_h,
                           int dst_w, int dst_h)
{
    G2D_SCREEN_PAGE *slot;
    int              i;

    if (dst_w <= 0 || dst_h <= 0 || src_w <= 0 || src_h <= 0)
    {
        return;
    }

    slot = G2dScreenPageFind(block);
    if (slot == NULL)
    {
        slot = G2dScreenPageFind(-1);
    }
    if (slot == NULL)
    {
        slot = &g2d_screen_page[0];
        for (i = 1; i < G2D_SCREEN_PAGE_MAX; i++)
        {
            if (g2d_screen_page[i].seq < slot->seq)
            {
                slot = &g2d_screen_page[i];
            }
        }
    }

    slot->block = block;
    slot->src_x = src_x;
    slot->src_y = src_y;
    slot->src_w = src_w;
    slot->src_h = src_h;
    slot->dst_w = dst_w;
    slot->dst_h = dst_h;
    slot->seq   = ++g2d_screen_page_seq;
}

void G2dForgetScreenPage(int block)
{
    G2D_SCREEN_PAGE *slot = G2dScreenPageFind(block);

    if (slot != NULL)
    {
        slot->block = -1;
    }
}

void G2dHostUpload(int dbp, int dbw, int dpsm, int dsax, int dsay, int w, int h, void *src)
{
    sceGsLoadImage li;

    if (src == NULL || w <= 0 || h <= 0)
    {
        return;
    }

    sceGsSetDefLoadImage(&li, (short)dbp, (short)dbw, (short)dpsm, (short)dsax, (short)dsay,
                         (short)w, (short)h);
    ReiPan_GsUpload(&li, (unsigned char *)src);

    // The page no longer holds a copy of the screen.
    G2dForgetScreenPage(dbp);
}

// Fill a w x h EE image of GS page v_adrs with the real screen: a frame
// buffer is the whole frame, any other page the rectangle its registration
// recorded, in its top-left corner (the rest cleared). A frame buffer stored
// into one of the two stashes is skipped: that is an effect round trip
// (LtoB, EffImageHalf32, BtoL) or ScreenCaptureSave, which the capture slots
// carry, and a read-back would cost a GPU sync per call for nothing.
static int LocalCopyScreenReadback(uintptr_t ee_adrs, int v_adrs, u_char *dst, int w, int h)
{
    const G2D_SCREEN_PAGE *page;
    int                    dw, dh;

    if (dst == NULL || w <= 0 || h <= 0)
    {
        return 0;
    }

    if (G2D_IS_FRAMEBUFFER_BLOCK(v_adrs))
    {
        if (G2D_IS_EE_STASH(ee_adrs))
        {
            return 0;
        }
        return ReiPan_RendererReadbackScreen(dst, w, 0, 0, 640, 448, w, h);
    }

    page = G2dScreenPageFind(v_adrs);
    if (page == NULL)
    {
        return 0;
    }

    dw = page->dst_w < w ? page->dst_w : w;
    dh = page->dst_h < h ? page->dst_h : h;
    if (dw <= 0 || dh <= 0)
    {
        return 0;
    }
    if (dw != w || dh != h)
    {
        memset(dst, 0, (size_t)w * (size_t)h * 4);
    }
    return ReiPan_RendererReadbackScreen(dst, w, page->src_x, page->src_y, page->src_w,
                                         page->src_h, dw, dh);
}

// PC: the pixels of GS page v_adrs (a frame buffer, or a page with a
// registered screen rectangle) as a w x h PSMCT32 image at dst, for copies
// that read a stash back on the EE (photo_make.c's CopyScreenToBuffer2):
// LocalCopyLtoBAdrs hands a page stored into a stash on slot to slot.
int G2dHostPageReadback(int v_adrs, u_char *dst, int w, int h)
{
    return LocalCopyScreenReadback(0, v_adrs, dst, w, h);
}

// ──────────────────────────────────────────────────────────────────────
// Local image copies
// ──────────────────────────────────────────────────────────────────────

// GS LOCAL -> EE: store PSMCT32 page v_adrs into ee_adrs at the size of type
// (header), type 0 in seven 64-line strips, type 2 in 400 + 48 lines.
// FF3 0x001ca650.

void LocalCopyLtoBAdrs(int type, uintptr_t ee_adrs, int v_adrs)
{
    sceGsStoreImage gs_simage[8];
    u_char         *dst;
    int             i;

    if (type == 1)
    {
        sceGsSetDefStoreImage(&gs_simage[0], (short)v_adrs, 5, SCE_GS_PSMCT32, 0, 0, 0x140, 0xe0);
    }
    else if (type == 0)
    {
        for (i = 0; i < 7; i++)
        {
            sceGsSetDefStoreImage(&gs_simage[i], (short)v_adrs, 10, SCE_GS_PSMCT32, 0,
                                  (short)(i * 0x40), 0x280, 0x40);
        }
    }
    else if (type == 2)
    {
        sceGsSetDefStoreImage(&gs_simage[0], (short)v_adrs, 5, SCE_GS_PSMCT32, 0, 0, 0x140, 400);
        sceGsSetDefStoreImage(&gs_simage[1], (short)v_adrs, 5, SCE_GS_PSMCT32, 0, 400, 0x140, 0x30);
    }
    else if (type == 3)
    {
        sceGsSetDefStoreImage(&gs_simage[0], (short)v_adrs, 10, SCE_GS_PSMCT32, 0, 0, 0x280, 0xe0);
    }

    sceGsSyncPath(0, 0);
    FlushCache(0);

    dst = (u_char *)ReiPan_GetHostPointer(ee_adrs);
    if (type == 1 || type == 3)
    {
        sceGsExecStoreImage(&gs_simage[0], (u_long128 *)dst);
    }
    else if (type == 0)
    {
        for (i = 0; i < 7; i++)
        {
            sceGsExecStoreImage(&gs_simage[i], (u_long128 *)(dst + i * 0x28000));
        }
    }
    else if (type == 2)
    {
        sceGsExecStoreImage(&gs_simage[0], (u_long128 *)dst);
        sceGsExecStoreImage(&gs_simage[1], (u_long128 *)(dst + 0x7d000));
    }

    // PC: the store read emulated GS memory, which no draw writes; put the
    // real frame there instead. A page stored into one of the two stashes
    // (EffectOthBufCopy's 0x38e0 every in-game frame) goes slot to slot,
    // under the stash's address, for LocalCopyBtoLAdrs to hand back: only
    // the effects read those stashes, and a read-back per frame would stall
    // the GPU.
    {
        int w, h;

        if (LocalCopyHostBufSize(type, &w, &h) != 0)
        {
            if (G2D_IS_EE_STASH(ee_adrs) &&
                !G2D_IS_FRAMEBUFFER_BLOCK(v_adrs))
            {
                ReiPan_RendererCaptureGsBlock((u_int)ee_adrs, (u_int)v_adrs, w, h);
            }
            else
            {
                LocalCopyScreenReadback(ee_adrs, v_adrs, dst, w, h);
            }
        }
    }

    sceGsSyncPath(0, 0);
}

// LocalCopyLtoBAdrs into EE buffer no (0 = 0x10ef000, 1 = 0x117b000).
// FF3 0x001ca870.

void LocalCopyLtoB(int type, int no, int addr)
{
    uintptr_t ee_adrs = (no == 0) ? G2D_EE_STASH_0 : G2D_EE_STASH_1;

    LocalCopyLtoBAdrs(type, ee_adrs, addr);

    // PC: keep the frame under the buffer's own address, for the matching
    // LocalCopyBtoL to hand on.
    {
        int w, h;

        if (G2D_IS_FRAMEBUFFER_BLOCK(addr) && LocalCopyHostBufSize(type, &w, &h) != 0)
        {
            ReiPan_RendererCaptureGsBlock((u_int)ee_adrs, REIPAN_GS_CAPTURE_LIVE, w, h);
        }
    }
}

// EE -> GS LOCAL: send ee_adrs to page v_adrs at the size of type, in strips
// of at most 100 lines (BITBLTBUF / TRXPOS / TRXREG / TRXDIR packet, then the
// image data). Any other type is 640x448 PSMCT24. FF3 0x001ca8a0.

void LocalCopyBtoLAdrs(int type, uintptr_t ee_adrs, int v_adrs)
{
    int     bw     = 0x140;    // width
    int     bl     = 0xe0;     // lines
    int     dbw    = 5;        // width / 64
    int     frtype = SCE_GS_PSMCT32;
    int     div    = 4;        // pixels per qword
    int     rline;
    int     oline;
    u_long *p;

    switch (type)
    {
    case 1:
        break;
    case 0:
        bw = 0x280; bl = 0x1c0; dbw = 10;
        break;
    case 2:
        bl = 0x1c0;
        break;
    case 3:
        bw = 0x280; dbw = 10; div = 8;
        break;
    default:
        bw = 0x280; bl = 0x1c0; dbw = 10; frtype = SCE_GS_PSMCT24;
        break;
    }

    for (rline = bl; rline > 0; rline -= oline)
    {
        oline = (rline > 100) ? 100 : rline;

        p    = (u_long *)StartDmaDirectTrans();
        p[0] = SCE_GIF_SET_TAG(4, 1, 0, 0, SCE_GIF_PACKED, 1);
        p[1] = SCE_GIF_PACKED_AD;
        p[2] = SCE_GS_SET_BITBLTBUF(0, 0, 0, v_adrs, dbw, frtype);
        p[3] = SCE_GS_BITBLTBUF;
        p[4] = SCE_GS_SET_TRXPOS(0, 0, 0, bl - rline, 0);
        p[5] = SCE_GS_TRXPOS;
        p[6] = SCE_GS_SET_TRXREG(bw, oline);
        p[7] = SCE_GS_TRXREG;
        p[8] = SCE_GS_SET_TRXDIR(0);
        p[9] = SCE_GS_TRXDIR;
        EndDmaDirectTrans((qword *)&p[10]);

        SetDmaDirectImageTrans(ReiPan_GetHostPointer(ee_adrs + (uintptr_t)(bw * (bl - rline) * 4)),
                               (bw * oline) / div);

        // PC: nothing walks the packet; re-issue the transfer (not for the
        // effect stashes, whose picture the capture slots carry).
        if (!G2D_IS_EE_STASH(ee_adrs))
        {
            G2dHostUpload(v_adrs, dbw, frtype, 0, bl - rline, bw, oline,
                          ReiPan_GetHostPointer(ee_adrs + (uintptr_t)(bw * (bl - rline) * 4)));
        }
    }

    // PC: the upload half of a stash round trip: the destination page is
    // served from what LocalCopyLtoB(Adrs) captured under the stash's
    // address, scaled to the type's size.
    {
        int w, h;

        if (G2D_IS_EE_STASH(ee_adrs) &&
            !G2D_IS_FRAMEBUFFER_BLOCK(v_adrs) && LocalCopyHostBufSize(type, &w, &h) != 0)
        {
            ReiPan_RendererCaptureGsBlock((u_int)v_adrs, (u_int)ee_adrs, w, h);
        }
    }
}

// LocalCopyBtoLAdrs from EE buffer no (0 = 0x10ef000, 1 = 0x117b000).
// FF3 0x001caa80.

void LocalCopyBtoL(int type, int no, int addr)
{
    uintptr_t ee_adrs = (no == 0) ? G2D_EE_STASH_0 : G2D_EE_STASH_1;

    // PC: LocalCopyBtoLAdrs serves the page from the stash's capture slot.
    LocalCopyBtoLAdrs(type, ee_adrs, addr);
}

// The LocalCopyLtoL geometries (data 0x327438; FF2 had the first six).
static SCREEN_COPY_WRK scpw[7] =
{
    /*      stbp sfbw stw sth dtbp dfbw   dw   dh   du   dv */
    /* 0 */ {  0,  10, 10,  9,   0,  10, 640, 448, 640, 448 },
    /* 1 */ {  0,  10, 10,  9,   0,   5, 320, 224, 640, 448 },
    /* 2 */ {  0,   5,  9,  8,   0,  10, 640, 448, 320, 224 },
    /* 3 */ {  0,  10, 10,  9,   0,   5, 320, 448, 640, 448 },
    /* 4 */ {  0,   5,  9,  9,   0,  10, 640, 448, 320, 448 },
    /* 5 */ {  0,  10, 10,  9,   0,  10, 640, 224, 640, 448 },
    /* 6 */ {  0,   5,  9,  8,   0,   5, 320, 224, 320, 224 },
};

// GS LOCAL -> LOCAL: draw page addr1 (PSMCT24) as one sprite into page addr2
// at the geometry of type, then point FRAME_1 / XYOFFSET_1 back at the
// buffer being drawn. FF3 0x001caab0.

void LocalCopyLtoL(int type, int addr1, int addr2)
{
    // data 0x359bf8: no blend, bilinear, CLAMP, tests ALWAYS; ZBUF replaced
    // below by Z at 0x118 (PSMZ16S) with writes masked.
    static const GS_REG_SET copy_regs[5] =
    {
        { SCE_GS_SET_ALPHA(1, 0, 1, 0, 0x80),        SCE_GS_ALPHA_1 },
        { SCE_GS_SET_TEX1(1, 0, 1, 5, 0, 0, 1),      SCE_GS_TEX1_1  },
        { SCE_GS_SET_CLAMP(1, 1, 0, 0, 0, 0),        SCE_GS_CLAMP_1 },
        { SCE_GS_SET_TEST(1, 1, 0, 0, 0, 0, 1, 1),   SCE_GS_TEST_1  },
        { 0,                                         SCE_GS_ZBUF_1  },
    };
    SYS_WRK         *sys  = GetSysWrk();
    SCREEN_COPY_WRK *scpp = &scpw[type];
    GS_REG_SET       regs[5];
    u_long          *p;
    u_int           *q;
    int              i;

    // PC: the frame into the destination page's capture slot at this copy's
    // destination size, and the same into the read-back registry.
    if (G2D_IS_FRAMEBUFFER_BLOCK(addr1) && !G2D_IS_FRAMEBUFFER_BLOCK(addr2))
    {
        ReiPan_RendererCaptureGsBlock((u_int)addr2, REIPAN_GS_CAPTURE_LIVE, scpp->dw, scpp->dh);
        G2dRegisterScreenPage(addr2, 0, 0, 640, 448, scpp->dw, scpp->dh);
    }

    scpp->stbp = addr1;
    scpp->dtbp = addr2 / 32;

    for (i = 0; i < 5; i++)
    {
        regs[i] = copy_regs[i];
    }
    regs[4].data = SCE_GS_SET_ZBUF(0x118, SCE_GS_PSMZ16S & 0xf, 1);
    SetGsRegsCached(regs, 5, 1);

    p     = (u_long *)StartDmaDirectTrans();
    // A+D: TEXFLUSH, FRAME_1 (destination, CT24), XYOFFSET_1, TEX0_1 (source).
    p[0]  = SCE_GIF_SET_TAG(4, 1, 0, 0, SCE_GIF_PACKED, 1);
    p[1]  = SCE_GIF_PACKED_AD;
    p[2]  = 0;
    p[3]  = SCE_GS_TEXFLUSH;
    p[4]  = SCE_GS_SET_FRAME(scpp->dtbp, scpp->dfbw, SCE_GS_PSMCT24, 0);
    p[5]  = SCE_GS_FRAME_1;
    p[6]  = SCE_GS_SET_XYOFFSET(0x6c08, 0x7908);
    p[7]  = SCE_GS_XYOFFSET_1;
    p[8]  = SCE_GS_SET_TEX0(scpp->stbp, scpp->sfbw, SCE_GS_PSMCT24, scpp->stw, scpp->sth, 1, 1,
                            0, 0, 0, 0, 0);
    p[9]  = SCE_GS_TEX0_1;
    // One textured FST SPRITE (rodata 0x370a00): UV, XYZF2 x2.
    p[10] = SCE_GIF_SET_TAG(1, 1, 1,
                            SCE_GS_SET_PRIM(SCE_GS_PRIM_SPRITE, 0, 1, 0, 0, 0, 1, 0, 0),
                            SCE_GIF_PACKED, 4);
    p[11] = 0x4343;
    q     = (u_int *)&p[12];
    q[0]  = 0;                              // UV 0,0 (upper half not written)
    q[1]  = 0;
    q[4]  = 0x6c00;                         // XYZF2 at the window origin
    q[5]  = 0x7900;
    p[15] = 0;
    q[8]  = scpp->du << 4;                  // UV du,dv
    q[9]  = scpp->dv << 4;
    q[12] = scpp->dw * 16 + 0x6c00;         // XYZF2 dw,dh
    q[13] = scpp->dh * 16 + 0x7900;
    p[19] = 0;
    // A+D: FRAME_1 / XYOFFSET_1 back to the buffer being drawn.
    p[20] = SCE_GIF_SET_TAG(2, 1, 0, 0, SCE_GIF_PACKED, 1);
    p[21] = SCE_GIF_PACKED_AD;
    p[22] = SCE_GS_SET_FRAME((sys->count & 1) * 0x8c, 10, SCE_GS_PSMCT24, 0);
    p[23] = SCE_GS_FRAME_1;
    p[24] = SCE_GS_SET_XYOFFSET(0x6c00, 0x7200);
    p[25] = SCE_GS_XYOFFSET_1;
    EndDmaDirectTrans((qword *)&p[26]);
}

// ──────────────────────────────────────────────────────────────────────
// Packets
// ──────────────────────────────────────────────────────────────────────

static PACKET_BUF_MGR *pk2d_mgr;            // sdata gp-0xca0

// Send what the packet buffer holds now. FF3 0x001cad48.

void PK2DKick(void)
{
    if (pk2d_mgr == NULL)
    {
        pk2d_mgr = GetPacketBufMgr();
    }
    pk2d_mgr->Kick();
}

// The direct packet pair, written early (2026-10-13, outgame/lang_sel.c's
// session): the language select's fade layer is the first Bp draw, and a
// null packet crashed it. FF2 called dmaVif1Get/SetPacketFLUSH_DIRECT; FF3
// goes through the packet buffer manager (EE addresses; host pointers on PC).

// FF3 0x001cad78.
qword *StartDmaDirectTrans(void)
{
    return (qword *)ReiPan_GetHostPointer(GetPacketBufMgr()->GetPacket());
}

// Closes the packet as DIRECT, no VIF FLUSH. FF3 0x001cada8 (returns nothing
// there; tail here, as FF2).
qword *EndDmaDirectTrans(qword *tail)
{
    GetPacketBufMgr()->SetPacket((u_int)ReiPan_GetPs2Address(tail), 1, 0);
    return tail;
}

// The image data of a transfer set up by a BITBLTBUF/TRXREG/TRXDIR packet: a
// GIF IMAGE tag for `qwc` qwords as one DIRECT packet, then a DMA REF tag to
// the data. Written with graphics/graph2d/tim2.c (2026-10-13), whose sends end
// with it. FF2 SetPK2DImageTrans; FF3 caches the manager in a static.
// PC: the chain is not walked, the texels reach the GS through the sender's
// ReiPan_GsUpload; this keeps the packet as FF3 builds it. FF3 0x001cadf0.

void SetDmaDirectImageTrans(const void *addr, int qwc)
{
    PACKET_BUF_MGR *mgr = GetPacketBufMgr();
    u_long         *p   = (u_long *)StartDmaDirectTrans();

    p[0] = (u_long)(u_int)qwc | 0x1800000000008000;    // GIF tag: NLOOP qwc, EOP, IMAGE, NREG 1
    p[1] = 0xe;
    mgr->SetPacket((u_int)ReiPan_GetPs2Address(&p[2]), 1, 0);
    mgr->AddRefTag((u_int)ReiPan_GetPs2Address(addr), qwc, 1, 0);
}

