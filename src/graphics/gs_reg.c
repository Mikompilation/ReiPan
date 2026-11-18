/* ==========================================================================
 *  graphics/gs_reg.c
 *
 *  FF3's GS register cache and screen capture (see gs_reg.h).
 *
 *    * Register cache  InitGsRegCache (from InitGraphics) fills the 128-entry
 *                      cache with 0xff.., then sends the boot-time registers
 *                      and the defaults (alpha, test, filter, scissor, zbuf).
 *                      Every later write goes through SetGsRegCached(s): one
 *                      A+D GIF packet into the VIF1 packet buffer, value kept.
 *    * Screen capture  ScreenCaptureSave / ScreenCaptureDisp: the frame on
 *                      screen kept in EE memory and drawn back behind the
 *                      pause / menu / save screens.
 *
 *  Also in this object: DrawTexQuadStrip3D (never called) and, after it, two
 *  empty unreferenced functions at 0x1e7550 and 0x1e7558 (not ported).
 *  FF3 0x1e6b98..0x1e7560; no FF2 counterpart (FF2 wrote GS registers
 *  through its draw-environment helpers and kept no cache).
 * ======================================================================== */

#include "gs_reg.h"

#include <string.h>                     // memset
#include <libgraph.h>                   // SCE_GS_* / SCE_GIF_*
#include <libvu0.h>                     // sceVu0RotTransPersN

#include "graphics/graph2d/g2d_draw.h"  // DISP_SPRT2 / SPRT_DAT2, LocalCopy*
#include "graphics/effect/effect_sub.h" // EffImageHalf32
#include "graphics/graph3d/gra3d.h"     // gra3dGetCamera
#include "system/os/system.h"           // GetPacketBufMgr / PACKET_BUF_MGR, GetSysWrk

#include "reipan/reipan_memory.h"       // PC: ReiPan_GetHostPointer
#include "reipan/rendering/reipan_renderer.h"   // PC: GS draw-state shadows

// ──────────────────────────────────────────────────────────────────────
// Statics / globals

static u_long gs_reg_cache[128];            // bss 0x3eed30: last value per GS register

// Boot-time registers, sent once (rodata 0x35aa60).
static const GS_REG_SET gs_reg_init[11] =
{
    { SCE_GS_SET_FOGCOL(200, 200, 200),           SCE_GS_FOGCOL     },
    { 0,                                          SCE_GS_PABE       },
    { SCE_GS_SET_TEXA(0x80, 0, 0x80),             SCE_GS_TEXA       },
    { 1,                                          SCE_GS_PRMODECONT },
    { 1,                                          SCE_GS_COLCLAMP   },  // clamp, not wrap
    { 0,                                          SCE_GS_DTHE       },  // dither off
    { 0x6071253571603525,                         SCE_GS_DIMX       },  // the usual 4x4 dither matrix
    { SCE_GS_SET_XYOFFSET(0x6c00, 0x7200),        SCE_GS_XYOFFSET_1 },  // (1728, 1824)
    { SCE_GS_SET_XYOFFSET(0x6c00, 0x7200),        SCE_GS_XYOFFSET_2 },
    { 0,                                          SCE_GS_FBA_1      },
    { 0,                                          SCE_GS_FBA_2      },
};

// The default draw state SetDefaultGsRegs restores (rodata 0x35ab10): normal
// alpha blend, alpha >= 0 and depth >= tests, bilinear + trilinear-mip
// filtering, the 640x448 window, 16-bit Z at block 0x118.
static const GS_REG_SET gs_reg_default[12] =
{
    { SCE_GS_SET_ALPHA(0, 1, 0, 1, 0),            SCE_GS_ALPHA_1    },
    { SCE_GS_SET_ALPHA(0, 1, 0, 1, 0),            SCE_GS_ALPHA_2    },
    { 0,                                          SCE_GS_CLAMP_1    },
    { 0,                                          SCE_GS_CLAMP_2    },
    { SCE_GS_SET_TEST(1, 6, 0, 0, 0, 0, 1, 2),    SCE_GS_TEST_1     },
    { SCE_GS_SET_TEST(1, 6, 0, 0, 0, 0, 1, 2),    SCE_GS_TEST_2     },
    { SCE_GS_SET_TEX1(1, 0, 1, 5, 0, 0, 0),       SCE_GS_TEX1_1     },
    { SCE_GS_SET_TEX1(1, 0, 1, 5, 0, 0, 0),       SCE_GS_TEX1_2     },
    { SCE_GS_SET_SCISSOR(0, 639, 0, 447),         SCE_GS_SCISSOR_1  },
    { SCE_GS_SET_SCISSOR(0, 639, 0, 447),         SCE_GS_SCISSOR_2  },
    { SCE_GS_SET_ZBUF(0x118, SCE_GS_PSMZ16S & 0xf, 0), SCE_GS_ZBUF_1 },
    { SCE_GS_SET_ZBUF(0x118, SCE_GS_PSMZ16S & 0xf, 0), SCE_GS_ZBUF_2 },
};

// ScreenCaptureSave's type; < 0: nothing captured, ScreenCaptureDisp draws nothing.
static int capture_type = -1;               // sdata 0x370cf0

// VRAM the capture is uploaded to and drawn from (after both frame buffers,
// 0 and 0x1180, and the Z buffer at 0x2300), and the EE copy.
#define CAPTURE_VRAM_ADRS   0x2bc0
#define CAPTURE_EE_ADRS     0x10ef000       // LocalCopy*'s buffer 0

// ──────────────────────────────────────────────────────────────────────
// GS register cache
// ──────────────────────────────────────────────────────────────────────

// PC: nothing walks the packets, so the renderer learns the context-1 draw
// state from the cache writes instead (MioPan did this in SetDrawEnv). Every
// 2D primitive (g2d_draw.c) sends its ALPHA/TEST/ZBUF here right before it
// draws.
static void GsRegShadow(u_long data, int reg)
{
    switch (reg)
    {
    case SCE_GS_ALPHA_1:    ReiPan_RendererSetGsAlphaRegister(data);   break;
    case SCE_GS_TEST_1:     ReiPan_RendererSetGsTestRegister(data);    break;
    case SCE_GS_ZBUF_1:     ReiPan_RendererSetGsZbufRegister(data);    break;
    case SCE_GS_SCISSOR_1:  ReiPan_RendererSetGsScissorRegister(data); break;
    }
}

// Mark every register unknown (all 0xff), then send the boot-time set and the
// default draw state.
// FF3 0x001e6b98; called from InitGraphics.

void InitGsRegCache(void)
{
    memset(gs_reg_cache, 0xff, sizeof(gs_reg_cache));
    SetGsRegsCached(gs_reg_init, 11, 1);
    SetDefaultGsRegs();
}

// ──────────────────────────────────────────────────────────────────────
// Write one GS register: an A+D packet (GIF tag + one data/address pair) into
// the packet buffer, and the value into the cache. Always returns 1.
// FF3 0x001e6bd8.

int SetGsRegCached(u_long data, int reg, int mode)
{
    PACKET_BUF_MGR *mgr;
    u_int           adrs;
    u_long         *pkt;

    if (mode != GS_REG_SEND_NONE)
    {
        mgr  = GetPacketBufMgr();
        adrs = mgr->GetPacket();
        pkt  = (u_long *)ReiPan_GetHostPointer(adrs);

        pkt[0] = SCE_GIF_SET_TAG(1, 1, 0, 0, SCE_GIF_PACKED, 1);
        pkt[1] = SCE_GIF_PACKED_AD;
        gs_reg_cache[reg] = data;
        GsRegShadow(data, reg);                 // PC
        pkt[2] = data;
        pkt[3] = reg;
        mgr->SetPacket(adrs + 0x20, 1, 1);
    }

    return 1;
}

// ──────────────────────────────────────────────────────────────────────
// Write num GS registers in one A+D packet, caching each. Returns 0 when num
// is 0 (the tag is cleared but not sent), else 1.
// FF3 0x001e6cc8.

int SetGsRegsCached(const GS_REG_SET *regs, int num, int mode)
{
    PACKET_BUF_MGR *mgr;
    u_int           adrs;
    u_long         *pkt;
    GS_REG_SET     *dst;
    u_int           cnt;

    if (mode == GS_REG_SEND_NONE)
    {
        return 1;
    }

    mgr  = GetPacketBufMgr();
    adrs = mgr->GetPacket();
    pkt  = (u_long *)ReiPan_GetHostPointer(adrs);
    pkt[0] = 0;
    pkt[1] = 0;

    cnt = 0;
    dst = (GS_REG_SET *)&pkt[2];
    for (; num > 0; num--)
    {
        cnt++;
        gs_reg_cache[(int)regs->reg] = regs->data;
        GsRegShadow(regs->data, (int)regs->reg);    // PC
        dst->data = regs->data;
        dst->reg  = regs->reg;
        regs++;
        dst++;
    }

    if (cnt == 0)
    {
        return 0;
    }

    pkt[1] = SCE_GIF_PACKED_AD;
    pkt[0] = SCE_GIF_SET_TAG(cnt & 0x7fff, 1, 0, 0, SCE_GIF_PACKED, 1);
    mgr->SetPacket(adrs + (u_int)((u_char *)dst - (u_char *)pkt), 1, 1);

    return 1;
}

// ──────────────────────────────────────────────────────────────────────
// Restore the default draw state (both contexts).
// FF3 0x001e6df8.

void SetDefaultGsRegs(void)
{
    SetGsRegsCached(gs_reg_default, 12, 1);
}

// ──────────────────────────────────────────────────────────────────────
// The cached value of one register, to save it before a change and restore
// it after.
// FF3 0x001e6e20.

u_long *GetGsRegCached(int reg)
{
    return &gs_reg_cache[reg];
}

// ──────────────────────────────────────────────────────────────────────
// The whole cache, indexed by register address (readers use XYOFFSET_1).
// FF3 0x001e6e40.

u_long *GetGsRegCache(void)
{
    return gs_reg_cache;
}

// ──────────────────────────────────────────────────────────────────────
// Screen capture
// ──────────────────────────────────────────────────────────────────────

// Keep the frame on screen for ScreenCaptureDisp, in EE buffer 0x10ef000:
//   0  as is (640x448);
//   1  shrunk to 320x224 through VRAM 0x2bc0 first;
//   2  640x448, then halved to 320x448 on the EE (blend: average pixel pairs
//      instead of dropping every other one).
// Any other type only records the type. The frame on screen is the one not
// being drawn: block ((count + 1) & 1) * 0x1180.
// FF3 0x001e6e58; FF2 did this per menu (MenuInDispCapture and friends).

void ScreenCaptureSave(int type, int blend)
{
    capture_type = type;

    switch (type)
    {
    case 0:
        LocalCopyLtoB(0, 0, ((GetSysWrk()->count + 1) & 1) * 0x1180);
        break;

    case 1:
        LocalCopyLtoL(1, ((GetSysWrk()->count + 1) & 1) * 0x1180, CAPTURE_VRAM_ADRS);
        LocalCopyLtoB(1, 0, CAPTURE_VRAM_ADRS);
        break;

    case 2:
        LocalCopyLtoB(0, 0, ((GetSysWrk()->count + 1) & 1) * 0x1180);
        EffImageHalf32((u_int *)ReiPan_GetHostPointer(CAPTURE_EE_ADRS), 640, 448, blend);
        break;
    }
}

// ──────────────────────────────────────────────────────────────────────
// Draw the capture full screen, tinted r,g,b and blended over the frame with
// fixed alpha: upload it to VRAM 0x2bc0 (geometry of its type), then one
// sprite at priority 0x10 with Z writes masked. Types 1 and 3 draw with
// TEX0 0, as in FF3.
// FF3 0x001e6f88; FF2 MenuCaptureDataDisp / MisPauseCaptureDataDisp.

void ScreenCaptureDisp(u_char r, u_char g, u_char b, u_char alpha)
{
    // rodata 0x35abd0: the 640x448 screen, half-pixel offset, pri 0xa0.
    static const SPRT_DAT2 capture_sprt =
        { 0, 0.0f, 0.0f, 640.0f, 448.0f, 640.0f, 448.0f, -0.5f, -0.5f, 0xa0, 0x80 };
    SPRT_DAT2  sd;
    DISP_SPRT2 ds;
    u_long     tex0;
    int        type;

    if (capture_type < 0)
    {
        return;
    }

    type = 0;
    if (capture_type == 1)
    {
        type = 1;
    }
    else if (capture_type == 2)
    {
        type = 2;
    }
    else if (capture_type == 3)
    {
        type = 3;
    }
    LocalCopyBtoL(type, 0, CAPTURE_VRAM_ADRS);

    tex0 = 0;
    sd   = capture_sprt;
    if (capture_type == 1)
    {
        sd.u2 = sd.u2 * 0.5f;
        sd.v2 = sd.v2 * 0.5f;
    }
    else if (capture_type == 0)
    {
        // 640 wide (TBW 10), CT24, 1024x512, MODULATE (sdata 0x370cf8)
        tex0 = SCE_GS_SET_TEX0(CAPTURE_VRAM_ADRS, 10, SCE_GS_PSMCT24, 10, 9, 0, 0, 0, 0, 0, 0, 1);
    }
    else if (capture_type == 2)
    {
        sd.u2 = sd.u2 * 0.5f;
        // 320 wide (TBW 5), CT24, 512x512 (sdata 0x370d00)
        tex0 = SCE_GS_SET_TEX0(CAPTURE_VRAM_ADRS, 5, SCE_GS_PSMCT24, 9, 9, 0, 0, 0, 0, 0, 0, 1);
    }

    // Align the captured-screen quad with the host framebuffer.
    sd.x += 0.5f;
    sd.y += 0.5f;
    CopySprDToSpr2(&ds, &sd);
    ds.tex0   = tex0;
    ds.r      = r;
    ds.zbuf   = SCE_GS_SET_ZBUF(0x118, SCE_GS_PSMZ16S & 0xf, 1);   // Z writes masked
    ds.g      = g;
    ds.b      = b;
    ds.z      = 0xfff00;
    ds.test   = SCE_GS_SET_TEST(1, 1, 0, 0, 0, 0, 1, 1);           // always pass
    ds.pri    = 0x10;
    ds.alpreg = SCE_GS_SET_ALPHA(0, 1, 2, 1, alpha);               // (Cs - Cd) * alpha + Cd
    DispSprD2(&ds);
}

// ──────────────────────────────────────────────────────────────────────
// Unreferenced: project num vertices through the camera's world->screen matrix and
// draw each group of 4 as a textured quad (one TRISTRIP GIF packet: TEXFLUSH,
// TEX0_1, then ST/RGBAQ/XYZF2 per vertex, ADC on the first two of each group).
// Corner UVs: (u, v), (u+du, v), (u, v+dv), (u+du, v+dv), perspective-divided
// and normalised to the texture size in tex0. A vertex outside the GS window
// (0x4000..0xc000) or depth range drops the whole packet. FF3 adds a 0.0f
// variable to u and v for the first corner column/row; written here as u, v.
// FF3 0x001e7150; Ghidra had it inside ScreenCaptureDisp's body.

void DrawTexQuadStrip3D(sceVu0FVECTOR *vtx, int num, int r, int g, int b, int a,
                        u_long tex0, int zofs, float u, float v, float du, float dv)
{
    PACKET_BUF_MGR *mgr;
    u_int           adrs;
    u_long         *pkt;
    u_int          *q;
    GRA3DCAMERA    *cam;
    sceVu0IVECTOR   iv[4];
    float           s[4], t[4];
    float           rq[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
    float           tw, th;
    int             qwc;
    int             n, i, clip;

    mgr  = GetPacketBufMgr();
    adrs = mgr->GetPacket();
    pkt  = (u_long *)ReiPan_GetHostPointer(adrs);

    pkt[0] = SCE_GIF_SET_TAG(2, 1, 0, 0, SCE_GIF_PACKED, 1);
    pkt[1] = SCE_GIF_PACKED_AD;
    pkt[2] = 0;
    pkt[3] = SCE_GS_TEXFLUSH;
    pkt[4] = tex0;
    pkt[5] = SCE_GS_TEX0_1;
    pkt[6] = SCE_GIF_SET_TAG(num, 1, 1,
                             SCE_GS_SET_PRIM(SCE_GS_PRIM_TRISTRIP, 0, 1, 0, 1, 0, 0, 0, 0),
                             SCE_GIF_PACKED, 3);
    pkt[7] = SCE_GIF_PACKED_ST | (SCE_GIF_PACKED_RGBAQ << 4) | (SCE_GIF_PACKED_XYZF2 << 8);
    qwc = 4;

    cam = gra3dGetCamera();

    th = (float)(1 << ((tex0 >> 30) & 0xf));
    tw = (float)(1 << ((tex0 >> 26) & 0xf));

    for (; num > 0; num -= 4, vtx += 4)
    {
        n = (num < 5) ? num : 4;
        sceVu0RotTransPersN(iv, cam->matWorldScreen, vtx, n, 1);

        clip = 0;
        for (i = 0; i < n; i++)
        {
            if ((u_int)iv[i][0] < 0x4000 || (u_int)iv[i][0] > 0xc000)
            {
                clip = 1;
            }
            if ((u_int)iv[i][1] < 0x4000 || (u_int)iv[i][1] > 0xc000)
            {
                clip = 1;
            }
            if (iv[i][2] == 0 || (u_int)iv[i][2] > 0xffffff)
            {
                clip = 1;
            }
            rq[i] = 1.0f / (float)iv[i][3];
        }
        if (clip)
        {
            return;     // packet not closed: nothing is sent
        }

        s[0] = u * rq[0] / tw;
        s[1] = (u + du) * rq[1] / tw;
        s[2] = u * rq[2] / tw;
        s[3] = (u + du) * rq[3] / tw;
        t[0] = v * rq[0] / th;
        t[1] = v * rq[1] / th;
        t[2] = (v + dv) * rq[2] / th;
        t[3] = (v + dv) * rq[3] / th;

        for (i = 0; i < n; i++)
        {
            q = (u_int *)&pkt[qwc * 2];
            ((float *)q)[0] = s[i];                 // ST: S, T, Q
            ((float *)q)[1] = t[i];
            ((float *)q)[2] = rq[i];
            q[3]  = 0;
            q[4]  = r;                              // RGBAQ
            q[5]  = g;
            q[6]  = b;
            q[7]  = a;
            q[8]  = iv[i][0];                       // XYZF2
            q[9]  = iv[i][1];
            q[10] = (zofs + iv[i][2]) << 4;
            q[11] = (i < 2) ? 0x8000 : 0;           // ADC: no kick for the strip's first two
            qwc += 3;
        }
    }

    mgr->SetPacket(adrs + qwc * 16, 1, 0);
}
