// system/b2d/BpCtl.c -- FF3's Bp layout controller (FF3 0x106370..0x1089e8,
// assert string "BpCtl.c"). No FF2 counterpart: written from the FF3
// decompilation. See BpCtl.h for the entry table and its states.

#include <limits.h>                     // INT_MAX, INT_MIN
#include <string.h>

#include "system/b2d/BpCtl.h"
#include "system/b2d/B2dCtl.h"          // B2dCtlFindItemByName, hook setters, B2dCtlInit, B2dCtlDraw
#include "system/b2d/B2dFile.h"
#include "system/b2d/B2dSpr.h"          // B2dSprGetPartData, B2dSprSetDisp, B2dTrigInit/End
#include "system/b2d/B2dInp.h"          // B2dInpInit, B2dInpSetDirFunc
#include "system/b2d/BpTex.h"
#include "common/cmn_fade_layer.h"      // GetCmnFadeLayer, capCtl::Draw
#include "common/heapctrl.h"            // heapCtrlMalloc, heapCtrlFree
#include "graphics/graph2d/g2d_draw.h"  // StartDmaDirectTrans, EndDmaDirectTrans
#include "graphics/graph2d/tim2.h"      // Tim2GetFirstPicture, TIM2_PICTUREHEADER
#include "graphics/gs_reg.h"            // SetGsRegCached
#include "ingame/map/MapPhf.h"          // PhfCheck, PhfGetFileNum/Head/Data
#include "reipan/reipan_memory.h"       // PC: ReiPan_GetHostPointer
#include "reipan/rendering/reipan_renderer.h"   // PC: the draw hooks' quads
#include "sdk/libgraph.h"               // SCE_GS_TEX1_1, SCE_GS_CLAMP_1
#include "system/eeiop/fileload.h"      // FileLoadCancel
#include "system/os/sys_heap.h"         // GetSystemHeapWrkP
#include "system/os/system.h"           // GetPacketBufMgr
#include "system/pad/pad_ctrl.h"        // CheckPadTrgPlaySe
#include "system/eeiop/cddat.h"       // GetFileSize
#include "system/os/eecdvd.h"         // LoadReq / LoadReqGetAddr / IsLoadEnd / IsLoadEndAll

// ──────────────────────────────────────────────────────────────────────

// A direct GS packet qword as FF3 writes it: two 64-bit words or four ints.
union BP_PKT
{
    u_long ul[2];
    int    iw[4];
};
static_assert(sizeof(BP_PKT) == 0x10, "BP_PKT is one qword");

static void BpPktSet(BP_PKT *p, int a, int b, int c, int d)
{
    p->iw[0] = a;
    p->iw[1] = b;
    p->iw[2] = c;
    p->iw[3] = d;
}

// Screen to GS primitive coordinates (12.4 fixed point, origin 0x6c0, 0x720).
#define BP_GS_X(x)  (((x) + 0x6c0) * 16)
#define BP_GS_Y(y)  (((y) + 0x720) * 16)

// GIF tags (sdata 0x36b588, 0x36b590/0x36b598, 0x36b5a0, 0x36b5d8). All
// blend (ABE); the textured ones address texels in pixels (FST).
static const u_long bp_tag_sprite      = 0x50eb400000008001;   // SPRITE, TME: RGBAQ UV XYZ2 UV XYZ2
static const u_long bp_tag_quad[2]     = { 0x90aac00000008001, 0x0000000434343431 };  // TRIFAN, TME
static const u_long bp_tag_quad_flat   = 0x50a2c00000008001;   // TRIFAN: RGBAQ XYZ2 x4
static const u_long bp_tag_num         = 0x50eb400000008001;

// ──────────────────────────────────────────────────────────────────────
// PC: nothing walks the packets the draw hooks build, so each hook also hands
// its primitive to the renderer (as g2d_draw.c's G2dHostSprite does). x, y
// stay screen pixels: the packets add XYOFFSET_1's 0x6c0 / 0x720, so there is
// no +320 / +224 here (hazards.md "two GS origins"). Corners go to the
// renderer in strip order (TL, TR, BL, BR); ALPHA_1, TEST_1 and ZBUF_1 reach
// it through SetGsRegCached.

// A textured quad with the TEX0 the last BpTex packet set and the cached
// TEX1_1 / CLAMP_1 (BpSetDrawGsRegs). uv in texels; rgb packed 0xBBGGRR.
static void BpHostTexQuad(const float *xy, const float *uv, u_int z, u_int rgb, int alpha)
{
    u_long tex0 = BpTexGetHostTex0();

    ReiPan_RendererSetGs2dDepth(z);
    ReiPan_RendererDrawTexturedQuadClamp((const sceGsTex0 *)&tex0,
                                         (const sceGsTex1 *)GetGsRegCached(SCE_GS_TEX1_1), xy, uv,
                                         *GetGsRegCached(SCE_GS_CLAMP_1), (u_char)(rgb & 0xff),
                                         (u_char)((rgb >> 8) & 0xff), (u_char)((rgb >> 16) & 0xff),
                                         (u_char)alpha);
}

// An untextured quad in rgb[0..2] and alpha.
static void BpHostFlatQuad(const float *xy, u_int z, const u_char *rgb, int alpha)
{
    u_char rgba[16];

    for (int i = 0; i < 4; i++)
    {
        rgba[i * 4 + 0] = rgb[0];
        rgba[i * 4 + 1] = rgb[1];
        rgba[i * 4 + 2] = rgb[2];
        rgba[i * 4 + 3] = (u_char)alpha;
    }
    ReiPan_RendererSetGs2dDepth(z);
    ReiPan_RendererDrawSolidQuad(xy, rgba);
}

// A SPRITE's two corners (a0, b0) - (a1, b1) as the four of a strip quad.
static void BpHostSpriteCorners(float *out, int a0, int b0, int a1, int b1)
{
    out[0] = (float)a0;  out[1] = (float)b0;
    out[2] = (float)a1;  out[3] = (float)b0;
    out[4] = (float)a0;  out[5] = (float)b1;
    out[6] = (float)a1;  out[7] = (float)b1;
}

// A TRIFAN's four points (pairs, divided by `div`) in strip order: fan
// triangles 0-1-2 and 0-2-3 are strip 1-2-0 and 2-0-3.
static void BpHostFanToStrip(float *out, const int *pts, float div)
{
    static const int order[4] = { 1, 2, 0, 3 };

    for (int i = 0; i < 4; i++)
    {
        out[i * 2]     = (float)pts[order[i] * 2] / div;
        out[i * 2 + 1] = (float)pts[order[i] * 2 + 1] / div;
    }
}

static void       *bp_load_pack;         /* sdata 0x36b570 */
static int         bp_draw_handle = -1;  /* sdata 0x36b574 */
static int         bp_frame_done;        /* sdata 0x36b578 */
static u_int       bp_arena_cur;         /* sdata 0x36b57c */
static u_int       bp_arena_end;         /* sdata 0x36b580 */
static int         b2d_hit_handle;       /* sdata 0x36b5b8 */

static BP_CTL_ENT   bp_ctl[BP_CTL_MAX];  /* bss 0x377730 */
static B2D_NUM_FONT b2d_num_font;        /* bss 0x377d88 */

static int BpCtlStepPlay(BP_CTL_ENT *ent);

// What BpCtlDrawOne runs per entry state (data 0x2ef3a0).
struct BP_CTL_STEP
{
    int state;
    int (*func)(BP_CTL_ENT *ent);
};
static_assert(offsetof(BP_CTL_STEP, state) == 0, "BP_CTL_STEP.state");

static int BpCtlStepLoad(BP_CTL_ENT *ent);
static int BpCtlStepEnd(BP_CTL_ENT *ent);

static const BP_CTL_STEP bp_ctl_step[] =
{
    { BP_CTL_STATE_LOAD,  BpCtlStepLoad },
    { BP_CTL_STATE_READY, nullptr },
    { BP_CTL_STATE_PLAY,  BpCtlStepPlay },
    { BP_CTL_STATE_END,   BpCtlStepEnd },
    { -1,                 nullptr },
};

// ──────────────────────────────────────────────────────────────────────
// The part of the handle's items with the longest finite animation (sum of
// its keys' frames; parts with a looping key or bit 0 of their flags do not
// count). nullptr when there is none.
// FF3 0x00106370.

B2D_PART *B2dCtlGetLongestPart(int handle)
{
    B2D_PART *best     = nullptr;
    int       best_len = 0;

    for (B2D_ITEM *item = B2dCtlGetItemList(handle); item != nullptr; item = item->next)
    {
        for (int i = 0; i < B2D_ITEM_PARTS; i++)
        {
            B2D_PART *part = &item->part[i];
            int       len  = 0;

            if (part->flags & 1)
                continue;

            for (B2D_KEY *key = part->keys; key != nullptr; key = key->next)
            {
                len += key->frames;
                if (key->flags & 0x100)
                {
                    len = -1;
                    break;
                }
            }
            if (len >= 0 && len > best_len)
            {
                best     = part;
                best_len = len;
            }
        }
    }
    return best;
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x00106428.

int B2dPartIsPlaying(const B2D_PART *part)
{
    if (part == nullptr)
        return 0;
    return part->cur != nullptr;
}

// ──────────────────────────────────────────────────────────────────────
// 1 when any part of the item still plays. FF3 0x00106440.

int B2dItemIsPlaying(const B2D_ITEM *item)
{
    if (item != nullptr)
    {
        for (int i = 0; i < B2D_ITEM_PARTS; i++)
        {
            if (item->part[i].cur != nullptr)
                return 1;
        }
    }
    return 0;
}

// ──────────────────────────────────────────────────────────────────────
// The last `size` bytes (rounded up to 0x100) of packet buffer half `side`:
// FF3 loads files 0x1245..0x124e there. Returns an EE address.
// FF3 0x00106480 (caches GetPacketBufMgr in sdata gp-0xca0).

static u_int BpGetPacketBufTail(int side, int size)
{
    PACKET_BUF_MGR *mgr = GetPacketBufMgr();

    return mgr->top[side & 1] + mgr->size - ((size + 0xff) & ~0xff);
}

// ──────────────────────────────────────────────────────────────────────
// A bump arena for the Bp files 0x120f..0x1227 and 0x12e3..0x12e7 (EE
// addresses; album_edit sets it). FF3 0x001064e8.

void BpArenaSet(u_int addr, int size)
{
    bp_arena_cur = addr;
    bp_arena_end = addr + size;
}

// ──────────────────────────────────────────────────────────────────────
// 0 when the arena is used up (the cursor still moves). FF3 0x001064f8.

u_int BpArenaAlloc(int size)
{
    u_int addr = bp_arena_cur;

    bp_arena_cur += (size + 0xff) & ~0xff;
    if (bp_arena_end < bp_arena_cur)
        addr = 0;
    return addr;
}

// ──────────────────────────────────────────────────────────────────────
// The B2d core's error print callback: prints nothing.
// FF3 0x00106520 (no Ghidra function: jr ra; nop -- v0 is left as is).

static int BpErrPrintNull(const B2D_HOOK_ARGS *args)
{
    (void)args;
    return 0;
}

// ──────────────────────────────────────────────────────────────────────
// The B2d input's direction hook: for n > 0, the d-pad direction pressed
// this frame, 0..7 clockwise from up (a diagonal needs both buttons in the
// same frame); 0 also when nothing is pressed.
// FF3 0x00106528.

int BpPadGetDir8(const B2D_HOOK_ARGS *args)
{
    int n = (int)args->a0;

    if (n <= 0)
        return 0;
    if (CheckPadTrgPlaySe(0x3000, -1, 0)) return 1;     // up + right
    if (CheckPadTrgPlaySe(0x6000, -1, 0)) return 3;     // right + down
    if (CheckPadTrgPlaySe(0x9000, -1, 0)) return 7;     // up + left
    if (CheckPadTrgPlaySe(0xc000, -1, 0)) return 5;     // down + left
    if (CheckPadTrgPlaySe(0x1000, -1, 0)) return 0;     // up
    if (CheckPadTrgPlaySe(0x2000, -1, 0)) return 2;     // right
    if (CheckPadTrgPlaySe(0x4000, -1, 0)) return 4;     // down
    if (CheckPadTrgPlaySe(0x8000, -1, 0)) return 6;     // left
    return 0;
}

// ──────────────────────────────────────────────────────────────────────
// GS state for Bp drawing: TEX1_1, CLAMP_1, TEST_1, ZBUF_1.
// FF3 0x00106628.

void BpSetDrawGsRegs(void)
{
    SetGsRegCached(0x100000141, 0x14, 1);   // TEX1_1
    SetGsRegCached(5, 0x08, 1);             // CLAMP_1
    SetGsRegCached(0x30003, 0x47, 1);       // TEST_1
    SetGsRegCached(0x10a000118, 0x4e, 1);   // ZBUF_1
}

// ──────────────────────────────────────────────────────────────────────
// ALPHA_1 for the draw flags: 0x10 additive, 0x20 subtractive, else the
// normal (Cs - Cd) * As + Cd. FF3 0x00106690.

int BpGetBlendAlpha(u_int flags)
{
    if (flags & 0x10)
        return 0x48;
    if (flags & 0x20)
        return 0x42;
    return 0x44;
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x001066b8.

void BpSetBlendAlpha(u_int flags)
{
    SetGsRegCached(BpGetBlendAlpha(flags), 0x42, 1);
}

// ──────────────────────────────────────────────────────────────────────
// The B2d core's sprite draw: texels u, v, w, h at screen x, y scaled by
// sx, sy (1024 = 1.0, rounded); flags 0x1000 / 0x2000 mirror u / v, 0x10 /
// 0x20 pick the blend (BpGetBlendAlpha); rgb packed 0xBBGGRR.
// FF3 0x001066e0.

int B2dDrawSpriteDirect(const B2D_HOOK_ARGS *args)
{
    int   u     = (int)args->a1;
    int   v     = (int)args->a2;
    int   w     = (int)args->a3;
    int   h     = (int)args->t0;
    int   x     = (int)args->t1;
    int   y     = (int)args->t2;
    int   sx    = (int)args->t3;
    int   sy    = (int)args->stack[0];
    int   alpha = (int)args->stack[1];
    u_int rgb   = (u_int)args->stack[2];
    u_int flags = (u_int)args->stack[3];
    int   u0 = u, u1 = u + w;
    int   v0 = v, v1 = v + h;
    int   dw = (w * sx + 0x200) / 0x400;
    int   dh = (h * sy + 0x200) / 0x400;

    if (flags & 0x1000)
    {
        u0 = u + w;
        u1 = u;
    }
    if (flags & 0x2000)
    {
        v0 = v + h;
        v1 = v;
    }

    BpSetBlendAlpha(flags);
    BP_PKT *p = (BP_PKT *)StartDmaDirectTrans();
    p[0].ul[0] = bp_tag_sprite;
    p[0].ul[1] = 0x43431;
    BpPktSet(&p[1], rgb & 0xff, (rgb >> 8) & 0xff, (rgb >> 16) & 0xff, alpha);
    BpPktSet(&p[2], u0 << 4, v0 << 4, 0, 0);
    BpPktSet(&p[3], BP_GS_X(x), BP_GS_Y(y), 100, 0);
    BpPktSet(&p[4], u1 << 4, v1 << 4, 0, 0);
    BpPktSet(&p[5], BP_GS_X(x + dw), BP_GS_Y(y + dh), 100, 0);
    EndDmaDirectTrans((qword *)&p[6]);

    // PC: the same sprite on the renderer.
    float xy[8];
    float uv[8];

    BpHostSpriteCorners(xy, x, y, x + dw, y + dh);
    BpHostSpriteCorners(uv, u0, v0, u1, v1);
    // Keep horizontal filtering inside the atlas strip, including flipped tiles.
    const float du = u1 > u0 ? 0.5f : (u1 < u0 ? -0.5f : 0.0f);
    for (int i = 0; i < 4; ++i)
    {
        uv[i * 2] += (i & 1) ? -du : du;
    }
    BpHostTexQuad(xy, uv, 100, rgb, alpha);
    return 0;
}

// ──────────────────────────────────────────────────────────────────────
// The B2d core's rect draw: an untextured w x h box (scaled by sx, sy /
// 1024, truncated) at x, y in rgb[0..2] and alpha.
// FF3 0x001068a8.

int B2dDrawRectDirect(const B2D_HOOK_ARGS *args)
{
    const u_char *rgb   = (const u_char *)args->a0;
    int           w     = (int)args->a1;
    int           h     = (int)args->a2;
    int           x     = (int)args->a3;
    int           y     = (int)args->t0;
    int           sx    = (int)args->t1;
    int           sy    = (int)args->t2;
    int           alpha = (int)args->t3;
    u_int         flags = (u_int)args->stack[0];
    int           dw    = w * sx / 0x400;
    int           dh    = h * sy / 0x400;

    BpSetBlendAlpha(flags);
    BP_PKT *p = (BP_PKT *)StartDmaDirectTrans();
    p[0].ul[0] = 0x3023400000008001;        // SPRITE: RGBAQ XYZ2 XYZ2
    p[0].ul[1] = 0x441;
    BpPktSet(&p[1], rgb[0], rgb[1], rgb[2], alpha);
    BpPktSet(&p[2], BP_GS_X(x), BP_GS_Y(y), 0, 0);
    BpPktSet(&p[3], BP_GS_X(x + dw), BP_GS_Y(y + dh), 0, 0);
    EndDmaDirectTrans((qword *)&p[4]);

    // PC: the same box on the renderer.
    float xy[8];

    BpHostSpriteCorners(xy, x, y, x + dw, y + dh);
    BpHostFlatQuad(xy, 0, rgb, alpha);
    return 0;
}

// ──────────────────────────────────────────────────────────────────────
// The B2d core's quad draw: a textured 4-point fan, screen points xy[8]
// round the quad (B2dItemGetQuad: TL, BL, BR, TR), texels uv[8]; flags
// 0x1000 swaps uv corners 0<->3, 1<->2, then 0x2000 swaps 0<->1, 2<->3.
// FF3 0x001069c8.

int B2dDrawQuadDirect(const B2D_HOOK_ARGS *args)
{
    const int *xy    = (const int *)args->a1;
    const int *uv    = (const int *)args->a2;
    int        alpha = (int)args->a3;
    u_int      rgb   = (u_int)args->t0;
    u_int      flags = (u_int)args->t1;
    int        tex[4][2];

    for (int i = 0; i < 4; i++)
    {
        tex[i][0] = uv[i * 2] << 4;
        tex[i][1] = uv[i * 2 + 1] << 4;
    }
    if (flags & 0x1000)
    {
        int t[4][2];

        memcpy(t, tex, sizeof(t));
        memcpy(tex[0], t[3], sizeof(t[0]));
        memcpy(tex[1], t[2], sizeof(t[0]));
        memcpy(tex[2], t[1], sizeof(t[0]));
        memcpy(tex[3], t[0], sizeof(t[0]));
    }
    if (flags & 0x2000)
    {
        int t[4][2];

        memcpy(t, tex, sizeof(t));
        memcpy(tex[0], t[1], sizeof(t[0]));
        memcpy(tex[1], t[0], sizeof(t[0]));
        memcpy(tex[2], t[3], sizeof(t[0]));
        memcpy(tex[3], t[2], sizeof(t[0]));
    }

    BpSetBlendAlpha(flags);
    BP_PKT *p = (BP_PKT *)StartDmaDirectTrans();
    p[0].ul[0] = bp_tag_quad[0];
    p[0].ul[1] = bp_tag_quad[1];
    BpPktSet(&p[1], rgb & 0xff, (rgb >> 8) & 0xff, (rgb >> 16) & 0xff, alpha);
    for (int i = 0; i < 4; i++)
    {
        BpPktSet(&p[2 + i * 2], tex[i][0], tex[i][1], 0, 0);
        BpPktSet(&p[3 + i * 2], BP_GS_X(xy[i * 2]), BP_GS_Y(xy[i * 2 + 1]), 0, 0);
    }
    EndDmaDirectTrans((qword *)&p[10]);

    // PC: the same fan on the renderer (tex is 12.4 fixed point).
    float hxy[8];
    float huv[8];

    BpHostFanToStrip(hxy, xy, 1.0f);
    BpHostFanToStrip(huv, &tex[0][0], 16.0f);
    BpHostTexQuad(hxy, huv, 0, rgb, alpha);
    return 0;
}

// ──────────────────────────────────────────────────────────────────────
// The B2d core's flat quad draw: 4 screen points xy[8] in rgb[0..2], alpha.
// FF3 0x00106cd8 (no Ghidra function: Ghidra's B2dDrawQuadDirect body
// claims it; decoded from the bytes).

static int B2dDrawQuadFlatDirect(const B2D_HOOK_ARGS *args)
{
    const u_char *rgb   = (const u_char *)args->a0;
    const int    *xy    = (const int *)args->a1;
    int           alpha = (int)args->a2;
    u_int         flags = (u_int)args->a3;

    BpSetBlendAlpha(flags);
    BP_PKT *p = (BP_PKT *)StartDmaDirectTrans();
    p[0].ul[0] = bp_tag_quad_flat;
    p[0].ul[1] = 0x44441;
    BpPktSet(&p[1], rgb[0], rgb[1], rgb[2], alpha);
    for (int i = 0; i < 4; i++)
        BpPktSet(&p[2 + i], BP_GS_X(xy[i * 2]), BP_GS_Y(xy[i * 2 + 1]), 0, 0);
    EndDmaDirectTrans((qword *)&p[6]);

    // PC: the same fan on the renderer.
    float hxy[8];

    BpHostFanToStrip(hxy, xy, 1.0f);
    BpHostFlatQuad(hxy, 0, rgb, alpha);
    return 0;
}

// ──────────────────────────────────────────────────────────────────────
// The two B2d draw hooks BpCtl leaves empty (gp-0x7fb4, gp-0x7fb0).
// FF3 0x00106e58, 0x00106e60 (no Ghidra functions: jr ra; move v0, zero).

static int B2dDrawNull0(const B2D_HOOK_ARGS *args)
{
    (void)args;
    return 0;
}

static int B2dDrawNull1(const B2D_HOOK_ARGS *args)
{
    (void)args;
    return 0;
}

// ──────────────────────────────────────────────────────────────────────
// Where a Bp file is loaded: the tail of packet buffer half 0 (0x1245..
// 0x1249) or 1 (0x124a..0x124e), the arena (0x120f..0x1227, 0x12e3..
// 0x12e7), else the system heap.
// FF3 0x00106e68.

static void *BpCtlAllocBuf(int file)
{
    u_int size = GetFileSize(file);
    int   arena;

    if (file < 0x124a)
    {
        if (file > 0x1244)
            return ReiPan_GetHostPointer(BpGetPacketBufTail(0, size));   // PC: EE address
        arena = (u_int)(file - 0x120f) < 0x19;
    }
    else
    {
        if (file < 0x124f)
            return ReiPan_GetHostPointer(BpGetPacketBufTail(1, size));   // PC: EE address
        arena = (u_int)(file - 0x12e3) < 5;
    }
    if (arena)
    {
        u_int addr = BpArenaAlloc(size);

        return addr != 0 ? ReiPan_GetHostPointer(addr) : nullptr;      // PC: EE address
    }
    return heapCtrlMalloc(GetSystemHeapWrkP(), size, 0, 0, 0);
}

// ──────────────────────────────────────────────────────────────────────
// Drops the entry's load buffer; only system-heap buffers are freed.
// FF3 0x00106f28.

static void BpCtlFreeBuf(BP_CTL_ENT *ent)
{
    if (ent->buf == nullptr)
        return;

    if (!(ent->flags & BP_CTL_FLAG_USERBUF))
    {
        int file = ent->file;

        if (file < 0x124f)
        {
            if (file > 0x1244 || (u_int)(file - 0x120f) < 0x19)
            {
                ent->buf = nullptr;
                return;
            }
        }
        else if ((u_int)(file - 0x12e3) < 5)
        {
            ent->buf = nullptr;
            return;
        }
        heapCtrlFree(GetSystemHeapWrkP(), ent->buf, 0, 0, 0);
    }
    ent->buf = nullptr;
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x00106fc8.

static BP_CTL_ENT *BpCtlGetEntry(int handle)
{
    if ((u_int)handle < BP_CTL_MAX && bp_ctl[handle].file != -1)
        return &bp_ctl[handle];
    return nullptr;
}

// ──────────────────────────────────────────────────────────────────────
// The first free entry, reset for a load. FF3 0x00107000.

static BP_CTL_ENT *BpCtlNewEntry(void)
{
    for (int i = 0; i < BP_CTL_MAX; i++)
    {
        BP_CTL_ENT *ent = &bp_ctl[i];

        if (ent->file == -1)
        {
            ent->handle = i;
            ent->draw   = 1;
            ent->flags  = 0;
            ent->buf    = nullptr;
            return ent;
        }
    }
    return nullptr;
}

// ──────────────────────────────────────────────────────────────────────
// Cancels a running load and frees the entry's buffers; 0, or -1 without
// an entry. FF3 0x00107050.

int BpCtlRelease(int handle)
{
    BP_CTL_ENT *ent = BpCtlGetEntry(handle);

    if (ent == nullptr)
        return -1;

    if (!IsLoadEnd(ent->load_id))
        FileLoadCancel(ent->load_id, nullptr, nullptr);
    BpCtlFreeBuf(ent);
    if (ent->work != nullptr)
    {
        heapCtrlFree(GetSystemHeapWrkP(), ent->work, 0, 0, 0);
        ent->work = nullptr;
    }
    if (ent->tex != nullptr)
    {
        heapCtrlFree(GetSystemHeapWrkP(), ent->tex, 0, 0, 0);
        ent->tex = nullptr;
    }
    ent->file = -1;
    return 0;
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x00107110.

void BpCtlReleaseAll(void)
{
    for (int i = 0; i < BP_CTL_MAX; i++)
    {
        if (bp_ctl[i].file != -1)
            BpCtlRelease(BpCtlGetHandleByFileNo(bp_ctl[i].file));
    }
}

// ──────────────────────────────────────────────────────────────────────
// Lets BpCtlMain draw again (GameSystemFrame, every frame).
// FF3 0x00107180.

void BpCtlFrameReset(void)
{
    bp_frame_done = 0;
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x00107188.

void BpCtlFrameSkip(void)
{
    bp_frame_done = 1;
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x00107198.

int BpCtlIsFrameDone(void)
{
    return bp_frame_done;
}

// ──────────────────────────────────────────────────────────────────────
// The handle being drawn: item hooks use it. FF3 0x001071a0.

int BpGetDrawHandle(void)
{
    return bp_draw_handle;
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x001071a8.

void *BpGetLoadPack(void)
{
    return bp_load_pack;
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x001071b0.

int BpCtlGetHandleByFileNo(int file)
{
    for (int i = 0; i < BP_CTL_MAX; i++)
    {
        if (bp_ctl[i].file != -1 && bp_ctl[i].file == file)
            return bp_ctl[i].handle;
    }
    return -1;
}

// ──────────────────────────────────────────────────────────────────────
// Entry flag bits (FF3 does not check the handle). FF3 0x001071f0.

void BpCtlSetFlag(int handle, u_int bits)
{
    BpCtlGetEntry(handle)->flags |= bits;
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x00107228.

u_int BpCtlGetFlag(int handle)
{
    return BpCtlGetEntry(handle)->flags;
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x00107248.

void BpCtlClearFlag(int handle, u_int bits)
{
    BpCtlGetEntry(handle)->flags &= ~bits;
}

// ──────────────────────────────────────────────────────────────────────
// Allocates and clears the B2d work for the entry's .b2d data and builds
// the core's entry in it. FF3 0x00107280.

static int BpCtlCreateWork(BP_CTL_ENT *ent)
{
    // PC: initialised. B2dFileGetWorkSize leaves them unwritten when it
    // meets an unknown command (-1).
    int nitem = 0, nkey = 0;
    int nlabel;
    int size = B2dFileGetWorkSize(ent->b2d, &nitem, &nkey, &nlabel);

    ent->work = heapCtrlMalloc(GetSystemHeapWrkP(), size, 0, 0, 0);
    memset(ent->work, 0, size);
    B2dCtlSetup(ent->handle, ent->work, nitem, nkey);
    return 0;
}

// ──────────────────────────────────────────────────────────────────────
// Starts BpTex on the pack's files. FF3 0x00107308.

static int BpCtlLoadTex(BP_CTL_ENT *ent)
{
    int num = PhfGetFileNum(ent->buf);

    if (num < 0)
    {
        B2dErrPrint(-1, "BpCtl.c", 0x30f, 0);
        return -1;
    }
    ent->tex = heapCtrlMalloc(GetSystemHeapWrkP(), BpTexGetWorkSize(num), 0, 0, 0);
    BpTexSetup(ent->handle, 0x2bc0, ent->tex, num);
    return 0;
}

// ──────────────────────────────────────────────────────────────────────
// Hands the .b2d data to B2dFile and runs its "_RegistSprite_" label.
// FF3 0x001073b8.

static int BpCtlRegistSprite(BP_CTL_ENT *ent)
{
    if (B2dFileSetup(ent->handle, ent->b2d) < 0)
        return -1;
    B2dFilePlayLabel(ent->handle, "_RegistSprite_");    /* rodata 0x34bed8 */
    return 0;
}

// ──────────────────────────────────────────────────────────────────────
// Texture-load hook while a lone .b2d (no pack, so no textures) registers.
// FF3 0x00107408 (no Ghidra function: jr ra; move v0, zero).

static int BpTexLoadNull(const B2D_HOOK_ARGS *args)
{
    (void)args;
    return 0;
}

// ──────────────────────────────────────────────────────────────────────
// State 1: when the file is in, takes the .b2d (the file itself, or the
// first pack member named *.b2d), builds the B2d work, the textures (packs)
// and the sprites; state 2. FF3 0x00107410.
// callcheck: B2dCtlDraw (Ghidra's body runs on into BpCtlStepPlay 0x107568)

static int BpCtlStepLoad(BP_CTL_ENT *ent)
{
    if (!IsLoadEnd(ent->load_id))
        return 0;

    void *buf = ent->buf;

    if (!PhfCheck(buf))
    {
        ent->b2d = buf;
        BpCtlCreateWork(ent);
        B2dFileSetTexLoadFunc(BpTexLoadNull);
        if (BpCtlRegistSprite(ent) < 0)
            return -1;
        B2dFileSetTexLoadFunc(BpTexLoadFunc);
    }
    else
    {
        int i   = 0;
        int num = PhfGetFileNum(buf);

        for (; i < num; i++)
        {
            const char *name = PhfGetFileHead(buf, i)->name;

            if (strcmp(name + strlen(name) - 4, ".b2d") == 0)  /* sdata 0x36b5b0 */
                break;
        }
        ent->b2d = PhfGetFileData(buf, i);
        BpCtlCreateWork(ent);
        BpCtlLoadTex(ent);
        bp_load_pack = buf;
        if (BpCtlRegistSprite(ent) < 0)
            return -1;
    }
    bp_load_pack = nullptr;
    ent->flags = (ent->flags & ~BP_CTL_STATE_MASK) | BP_CTL_STATE_READY;
    return 0;
}

// ──────────────────────────────────────────────────────────────────────
// State 3: draws (and steps) the layout; state 4 when its animation ran out.
// 1 while paused. FF3 0x00107568 (no Ghidra function: inside
// BpCtlStepLoad's body; decoded from the bytes).

static int BpCtlStepPlay(BP_CTL_ENT *ent)
{
    if (ent->flags & BP_CTL_FLAG_PAUSE)
        return 1;
    if (B2dCtlDraw(ent->handle, 0) == 0)
        ent->flags = (ent->flags & ~BP_CTL_STATE_MASK) | BP_CTL_STATE_END;
    return 0;
}

// ──────────────────────────────────────────────────────────────────────
// State 4: keeps drawing. FF3 0x001075c0.

static int BpCtlStepEnd(BP_CTL_ENT *ent)
{
    if (!(ent->flags & BP_CTL_FLAG_PAUSE))
        B2dCtlDraw(ent->handle, 0);
    return 0;
}

// ──────────────────────────────────────────────────────────────────────
// Starts loading `file` into `addr` (nullptr: BpCtlAllocBuf); state 1.
// FF3 does not check `ent` (a full table crashes). FF3 0x001075f8.

static int BpCtlLoadStart(int file, BP_CTL_ENT *ent, void *addr)
{
    if (addr == nullptr)
    {
        ent->buf = BpCtlAllocBuf(file);
    }
    else
    {
        ent->buf    = addr;
        ent->flags |= BP_CTL_FLAG_USERBUF;
    }
    ent->file = file;
    LoadReqGetAddr(file, ent->buf, &ent->load_id);
    ent->flags = (ent->flags & ~BP_CTL_STATE_MASK) | BP_CTL_STATE_LOAD;
    return ent->handle;
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x00107678.

int BpCtlLoadReq(int file, void *addr)
{
    return BpCtlLoadStart(file, BpCtlNewEntry(), addr);
}

// ──────────────────────────────────────────────────────────────────────
// As BpCtlLoadReq, taking the last free entry (BpCtlNewEntry from the back,
// inlined). FF3 0x001076b8.

int BpCtlLoadReqLast(int file, void *addr)
{
    BP_CTL_ENT *ent = nullptr;

    for (int i = BP_CTL_MAX - 1; i >= 0; i--)
    {
        if (bp_ctl[i].file == -1)
        {
            ent         = &bp_ctl[i];
            ent->handle = i;
            ent->draw   = 1;
            ent->flags  = 0;
            ent->buf    = nullptr;
            break;
        }
    }
    return BpCtlLoadStart(file, ent, addr);
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x00107728.

int BpCtlGetState(int handle)
{
    BP_CTL_ENT *ent = BpCtlGetEntry(handle);

    if (ent == nullptr)
        return -1;
    return ent->flags & BP_CTL_STATE_MASK;
}

// ──────────────────────────────────────────────────────────────────────
// 1 once the entry is built (ready, playing or ended). FF3 0x00107758.

int BpCtlIsDataReady(int handle)
{
    int state = BpCtlGetState(handle);

    if (state < 0)
        return -1;
    return (u_int)(state - BP_CTL_STATE_READY) < 3;
}

// ──────────────────────────────────────────────────────────────────────
// Plays `label` of the layout (rewinding its parts first when asked).
// FF3 0x00107788.

int BpPlayLabelEx(int handle, const char *label, int rewind)
{
    BP_CTL_ENT *ent = BpCtlGetEntry(handle);

    if (ent == nullptr)
        return -1;
    if (BpCtlIsDataReady(handle) != 1)
        return -2;
    if (B2dFilePlayLabelEx(ent->handle, label) < 0)
        return -3;
    if (rewind)
        B2dCtlRewind(ent->handle);
    B2dSetPlayModeStep();
    ent->flags = (ent->flags & ~BP_CTL_STATE_MASK) | BP_CTL_STATE_PLAY;
    return 0;
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x00107838.

int BpPlayLabel(int handle, const char *label)
{
    return BpPlayLabelEx(handle, label, 1);
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x00107850.

int BpPlayLabelAll(const char *label)
{
    int failed = 0;

    for (int i = 0; i < BP_CTL_MAX; i++)
    {
        if (bp_ctl[i].file != -1)
            failed += BpPlayLabel(BpCtlGetHandleByFileNo(bp_ctl[i].file), label) < 0;
    }
    return failed;
}

// ──────────────────────────────────────────────────────────────────────
// 1 when every used entry is ready, else the first other answer.
// FF3 0x001078e8.

int BpCtlIsDataReadyAll(void)
{
    for (int i = 0; i < BP_CTL_MAX; i++)
    {
        if (BpCtlGetEntry(i) == nullptr)
            continue;

        int ready = BpCtlIsDataReady(i);

        if (ready != 1)
            return ready;
    }
    return 1;
}

// ──────────────────────────────────────────────────────────────────────
// Runs the entry's state step (bp_ctl_step) with BpGetDrawHandle set to it.
// 0; -1 when the step failed, -2 for an unknown state. FF3 does not check
// the handle. FF3 0x00107948.

int BpCtlDrawOne(int handle)
{
    BP_CTL_ENT *ent = &bp_ctl[handle];
    int         i;

    if (ent->file == -1)
        return 0;

    for (i = 0; bp_ctl_step[i].state >= 0; i++)
    {
        if (bp_ctl_step[i].state != (int)(ent->flags & BP_CTL_STATE_MASK))
            continue;

        if (bp_ctl_step[i].func != nullptr)
        {
            bp_draw_handle = ent->handle;
            if (bp_ctl_step[i].func(ent) < 0)
            {
                bp_draw_handle = -1;
                return -1;
            }
        }
        break;
    }
    if (bp_ctl_step[i].state < 0)
    {
        bp_draw_handle = -1;
        return -2;
    }
    return 0;
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x00107a28.

int BpCtlDrawBegin(void)
{
    BpSetDrawGsRegs();
    return 0;
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x00107a48.

int BpCtlDrawEnd(void)
{
    BpSetDrawGsRegs();
    return 0;
}

// ──────────────────────────────────────────────────────────────────────
// Once a frame: steps and draws every entry with `draw` set, then the
// common fade layer. Returns the entries not in state 4, 0 when the frame
// was already drawn, -1 when a step failed.
// FF3 0x00107a68.

int BpCtlMain(void)
{
    int count = 0;

    if (bp_frame_done != 0)
        return 0;

    bp_frame_done  = 1;
    bp_draw_handle = -1;
    BpCtlDrawBegin();
    for (int i = 0; i < BP_CTL_MAX; i++)
    {
        if (bp_ctl[i].draw != 0 && BpCtlDrawOne(i) < 0)
            return -1;
        if ((bp_ctl[i].flags & BP_CTL_STATE_MASK) != BP_CTL_STATE_END)
            count++;
    }
    BpCtlDrawEnd();
    GetCmnFadeLayer()->Draw();
    bp_draw_handle = -1;
    return count;
}

// ──────────────────────────────────────────────────────────────────────
// Releases the B2d core's side of every entry, then the entries.
// FF3 0x00107b28.

static int BpCtlTerm(void)
{
    for (int i = 0; i < BP_CTL_MAX; i++)
    {
        if (bp_ctl[i].file == -1)
            continue;
        if (B2dCtlIsEntryUsed(i))
            B2dCtlReset(i);
        B2dFileReset(i);
        BpTexRelease(i);
    }
    B2dInpInit();
    BpCtlReleaseAll();
    return 0;
}

// ──────────────────────────────────────────────────────────────────────
// 1 when the label playing on `file`'s layout has run out (its longest
// part stopped). FF3 0x00107bc0.

int BpCtlIsLoadEnd(int file)
{
    B2D_PART *part = B2dCtlGetLongestPart(BpCtlGetHandleByFileNo(file));

    if (part == nullptr)
        return 0;
    return B2dPartIsPlaying(part) == 0;
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x00107c00.

int B2dCtlHasItemName(int handle, const char *name)
{
    if (name == nullptr)
        return 0;

    for (B2D_ITEM *item = B2dCtlGetItemList(handle); item != nullptr; item = item->next)
    {
        if (item->name != nullptr && strcmp(item->name, name) == 0)
            return 1;
    }
    return 0;
}

// ──────────────────────────────────────────────────────────────────────
// FF3 does not check the handle. FF3 0x00107c70.

void BpCtlSetField20(int handle, int value)
{
    bp_ctl[handle].draw = value;
}

// ──────────────────────────────────────────────────────────────────────
// Plugs this module's draws, the pad direction and BpTex into the B2d core.
// FF3 0x00107c90.

void BpCtlSetHooks(void)
{
    B2dSetSpriteFunc(B2dDrawSpriteDirect);
    B2dSetRectFunc(B2dDrawRectDirect);
    B2dSetQuadFunc(B2dDrawQuadDirect);
    B2dSetQuadFlatFunc(B2dDrawQuadFlatDirect);
    B2dSetDrawFunc5(B2dDrawNull0);
    B2dSetDrawFunc6(B2dDrawNull1);
    B2dInpSetDirFunc(BpPadGetDir8);
    B2dFileSetTexLoadFunc(BpTexLoadFunc);
    B2dSetTexSetFunc(BpTexSetFunc);
}

// ──────────────────────────────────────────────────────────────────────
// GameSystemStart: the B2d core, B2dFile, input, sprites, BpTex, then the
// entry table. FF3 0x00107d10.

int B2dSystemInit(void)
{
    B2dCtlInit();
    B2dFileInit();
    B2dTrigInit();
    B2dInpInit();
    BpCtlFrameReset();
    BpTexInit();
    memset(bp_ctl, 0, sizeof(bp_ctl));
    for (int i = BP_CTL_MAX - 1; i >= 0; i--)
        bp_ctl[i].file = -1;
    bp_draw_handle = -1;
    BpCtlReleaseAll();
    BpCtlSetHooks();
    B2dSetErrPrintFunc(BpErrPrintNull);
    return 0;
}

// ──────────────────────────────────────────────────────────────────────
// GameSystemEnd. FF3 0x00107db8.

void B2dSystemEnd(void)
{
    BpCtlTerm();
    B2dTrigEnd();
}

// ──────────────────────────────────────────────────────────────────────
// Which side of the edge a -> b the point p lies on (>= 0: outside of a
// clockwise-on-screen polygon). FF3 0x00107de8.

int B2dEdgeFunc(const int *p, const int *a, const int *b)
{
    return (a[0] - p[0]) * (b[1] - a[1]) - (a[1] - p[1]) * (b[0] - a[0]);
}

// ──────────────────────────────────────────────────────────────────────
// A layout rect to screen: scaled by the handle's scale, moved by its
// position. FF3 0x00107e20.

void B2dRectToScreen(int handle, B2D_RECT *out, const B2D_RECT *in)
{
    int sx, sy, px, py;

    B2dCtlGetScale(handle, &sx, &sy);
    out->x = (int)((float)(in->x * sx) * (1.0f / 1024.0f));    /* sdata 0x36b5bc */
    out->y = (int)((float)(in->y * sy) * (1.0f / 1024.0f));
    out->w = (int)((float)(in->w * sx) * (1.0f / 1024.0f));
    out->h = (int)((float)(in->h * sy) * (1.0f / 1024.0f));
    B2dCtlGetPos(handle, &px, &py);
    out->x += px;
    out->y += py;
}

// ──────────────────────────────────────────────────────────────────────
// The item's rect on screen; handle < 0 uses the hit-test handle. -1 without
// an item, -2 without `out`. FF3 0x00107f28.

int B2dItemGetScreenRect(B2D_RECT *out, B2D_ITEM *item, int handle)
{
    int sx, sy, px, py;

    if (item == nullptr)
        return -1;
    if (out == nullptr)
        return -2;

    B2dItemGetRect(out, item, nullptr);
    if (handle < 0)
        handle = b2d_hit_handle;
    B2dCtlGetScale(handle, &sx, &sy);

    float y = (float)(out->y * sy) * (1.0f / 1024.0f);         /* sdata 0x36b5c0 */
    float w = (float)(out->w * sx) * (1.0f / 1024.0f);
    float h = (float)(out->h * sy) * (1.0f / 1024.0f);

    out->x = (int)((float)(out->x * sx) * (1.0f / 1024.0f));
    out->y = (int)y;
    out->w = (int)w;
    out->h = (int)h;
    B2dCtlGetPos(handle, &px, &py);
    out->x += px;
    out->y += py;
    return 0;
}

// ──────────────────────────────────────────────────────────────────────
// B2dCtlFindItemAt's test: (x, y) inside the item's screen rect grown by
// one pixel, and the item is a button (its name contains "Rs" or "rre").
// FF3 0x00108050.

static int B2dItemHitTest(B2D_ITEM *item, intptr_t x, intptr_t y)
{
    B2D_RECT r;
    int      pt[2] = { (int)x, (int)y };

    B2dItemGetScreenRect(&r, item, -1);

    const int corner[4][2] =
    {
        { r.x - 1,       r.y - 1 },
        { r.x - 1,       r.y + r.h + 1 },
        { r.x + r.w + 1, r.y + r.h + 1 },
        { r.x + r.w + 1, r.y - 1 },
    };

    for (int i = 1; i <= 4; i++)
    {
        if (B2dEdgeFunc(pt, corner[i - 1], corner[i % 4]) >= 0)
            return 0;
    }
    if (item->name == nullptr)
        return 0;
    return strstr(item->name, "Rs") != nullptr       /* sdata 0x36b5c8 */
        || strstr(item->name, "rre") != nullptr;     /* sdata 0x36b5d0 */
}

// ──────────────────────────────────────────────────────────────────────
// The first item from `start` (nullptr: the list head) that `func` accepts.
// FF3 0x00108178.

B2D_ITEM *B2dCtlFindItemIf(int handle, B2D_ITEM *start, B2D_ITEM_FIND_FUNC func, intptr_t a, intptr_t b)
{
    B2D_ITEM *head = B2dCtlGetItemList(handle);

    if (start == nullptr)
        start = head;
    for (; start != nullptr; start = start->next)
    {
        if (func(start, a, b) != 0)
            return start;
    }
    return nullptr;
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x001081f8.
// callcheck: B2dItemGetScreenRect (Ghidra's body runs on into B2dItemGrowBounds 0x108228)

B2D_ITEM *B2dCtlFindItemAt(int handle, B2D_ITEM *start, int x, int y)
{
    b2d_hit_handle = handle;
    return B2dCtlFindItemIf(handle, start, B2dItemHitTest, x, y);
}

// ──────────────────────────────────────────────────────────────────────
// B2dCtlGetBounds's step: grows the bounds by the item's screen rect;
// kind-3 items do not count. FF3 0x00108228 (no Ghidra function: inside
// B2dCtlFindItemAt's body; decoded from the bytes).

static int B2dItemGrowBounds(B2D_ITEM *item, intptr_t a, intptr_t b)
{
    B2D_BOUNDS *bounds = (B2D_BOUNDS *)a;
    B2D_RECT    r;

    (void)b;
    if ((item->kind & 0xf) == 3)
        return 0;

    B2dItemGetScreenRect(&r, item, -1);
    if (r.y < bounds->y0)
        bounds->y0 = r.y;
    if (r.x < bounds->x0)
        bounds->x0 = r.x;
    if (bounds->x1 < r.x + r.w)
        bounds->x1 = r.x + r.w;
    if (bounds->y1 < r.y + r.h)
        bounds->y1 = r.y + r.h;
    return 0;
}

// ──────────────────────────────────────────────────────────────────────
// The screen bounds of the handle's items (reset: start empty).
// FF3 0x001082d0.

int B2dCtlGetBounds(int handle, B2D_BOUNDS *bounds, int reset)
{
    if (reset)
    {
        bounds->y1 = INT_MIN;
        bounds->y0 = INT_MAX;
        bounds->x0 = INT_MAX;
        bounds->x1 = INT_MIN;
    }
    b2d_hit_handle = handle;
    B2dCtlFindItemIf(handle, nullptr, B2dItemGrowBounds, (intptr_t)bounds, 0);
    return 0;
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x00108330.

TIM2_PICTUREHEADER *BpTexGetPicture(int a0, int a1)
{
    return Tim2GetFirstPicture(BpTexGetTim2(a0, a1));
}

// ──────────────────────────────────────────────────────────────────────
// The clut index at x, y of that picture (4-bit when it has a 16-colour
// clut, the even pixel in the high nibble), -1 outside. FF3 0x00108350.
// callcheck: BpTexGetTexel (a self-reference in Ghidra's call list; the code does not recurse)

int BpTexGetTexel(int a0, int a1, int x, int y)
{
    TIM2_PICTUREHEADER *pic   = BpTexGetPicture(a0, a1);
    int                 w     = pic->ImageWidth;
    const u_char       *image = (const u_char *)pic + pic->HeaderSize;

    if (x < 0 || x >= w)
        return -1;
    if (y < 0 || y >= pic->ImageHeight)
        return -1;

    if (pic->ClutColors != 16)
        return image[w * y + x];

    u_char b = image[(w >> 1) * y + x / 2];

    if (x & 1)
        return b & 0xf;
    return b >> 4;
}

// ──────────────────────────────────────────────────────────────────────
// A sprite item drawn by hand: its texture and texel rectangle (parts 0..4)
// at x, y through DispSprD, pivot (x + px, y + py), the blend from part 10's
// key flags, white (FF3 does not read the r, g, b it is passed: they stay in
// t0..t2). Nothing for no item or alpha 0. FF3 0x002ab488 (emitted out of
// line at the end of .text; its only caller is B2dDrawSpriteByName's tail
// jump, its constant sdata 0x36b5e0 is this file's).

static void B2dDrawSpriteByItem(int handle, B2D_ITEM *item, int x, int y,
                                float alpha, float rot, float sx, float sy, float px, float py)
{
    if (item == nullptr || alpha == 0.0f)
        return;

    float     cx  = px + (float)x;
    float     cy  = py + (float)y;
    int       tex = B2dSprGetPartData(item, B2D_PART_TEX);
    SPRT_DAT  sd;
    DISP_SPRT ds;

    sd.tex0  = 0;
    sd.u     = (u_short)B2dSprGetPartData(item, B2D_PART_U);
    sd.v     = (u_short)B2dSprGetPartData(item, B2D_PART_V);
    sd.w     = (u_short)B2dSprGetPartData(item, B2D_PART_W);
    sd.h     = (u_short)B2dSprGetPartData(item, B2D_PART_H);
    sd.x     = x;
    sd.y     = y;
    sd.pri   = 0xa0;
    sd.alpha = (u_char)(int)(alpha * 128.0f);   /* sdata 0x36b5e0 */
    sd.flip  = 0;
    sd.bln   = 1;

    B2D_HOOK_ARGS args = {};

    args.a0 = handle;
    args.a1 = tex;
    BpTexSetFunc(&args);
    CopySprDToSpr(&ds, &sd);
    ds.tex0 = BpTexGetTex0(handle, tex);

    const B2D_KEY *key = item->part[B2D_PART_ALPHA].keys;

    ds.alphar = (u_long)BpGetBlendAlpha(key != nullptr ? key->flags & B2D_KEY_BLEND_MASK : 0);
    ds.crx    = cx;
    ds.cry    = cy;
    ds.csx    = cx;
    ds.csy    = cy;
    ds.x      = (float)x;
    ds.y      = (float)y;
    ds.scw    = sx;
    ds.sch    = sy;
    ds.rot    = rot;
    DispSprD(&ds);
    B2dResetCurTex();
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x00108490.

void B2dDrawSpriteByName(int handle, const char *name, int x, int y, u_char r, u_char g, u_char b,
                         float alpha, float rot, float sx, float sy, float px, float py)
{
    (void)r;
    (void)g;
    (void)b;
    B2dDrawSpriteByItem(handle, B2dCtlFindItemByName(handle, name), x, y, alpha, rot, sx, sy, px, py);
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x00108570.

void B2dCtlSetItemDispByName(int handle, const char *name, int disp)
{
    if (name == nullptr)
        return;

    B2D_ITEM *item = B2dCtlFindItemByName(handle, name);

    if (item != nullptr)
        B2dSprSetDisp(item, disp);
}

// ──────────────────────────────────────────────────────────────────────
// Glyph `no` of the number font from the item `name` (parts 1..4).
// FF3 0x001085b8.

static B2D_ITEM *B2dNumSetGlyph(int handle, const char *name, int no)
{
    B2D_ITEM      *item  = B2dCtlFindItemByName(handle, name);
    B2D_NUM_GLYPH *glyph = &b2d_num_font.glyph[no];

    glyph->u = B2dSprGetPartData(item, 1);
    glyph->v = B2dSprGetPartData(item, 2);
    glyph->w = B2dSprGetPartData(item, 3);
    glyph->h = B2dSprGetPartData(item, 4);
    return item;
}

// ──────────────────────────────────────────────────────────────────────
// The digits are the items named `prefix` with its last character replaced
// by '0'..'9' ("R 0" -> "R 0".."R 9"); `sep` (optional) is the thousands
// separator. FF3 0x00108638.

int B2dNumSetFont(int handle, const char *prefix, const char *sep)
{
    char name[0x100];

    strncpy(name, prefix, 0xfa);
    int len = (int)strlen(name);

    memset(&b2d_num_font, 0, sizeof(b2d_num_font));
    b2d_num_font.handle = handle;
    for (int i = 0; i < 10; i++)
    {
        name[len - 1] = (char)('0' + i);

        B2D_ITEM *item = B2dNumSetGlyph(handle, name, i);

        if (i == 0)
            b2d_num_font.tex = B2dSprGetPartData(item, 0);
    }
    if (sep != nullptr)
    {
        B2dNumSetGlyph(handle, sep, B2D_NUM_SEP);
        b2d_num_font.flags |= 1;
    }
    return 0;
}

// ──────────────────────────────────────────────────────────────────────
// Sets the font's texture (BpTex's texture-set hook, called directly).
// FF3 0x00108720.

void B2dNumSetTex(void)
{
    B2D_HOOK_ARGS args = {};

    args.a0 = b2d_num_font.handle;
    args.a1 = b2d_num_font.tex;
    BpTexSetFunc(&args);
}

// ──────────────────────────────────────────────────────────────────────
// One glyph sprite whose bottom-right corner is x, y; returns the next
// qword. FF3 0x00108748.

static BP_PKT *B2dNumPutGlyph(BP_PKT *p, const B2D_NUM_GLYPH *g, int x, int y, int alpha)
{
    p[0].ul[0] = bp_tag_num;
    p[0].ul[1] = 0x43431;
    BpPktSet(&p[1], 0x80, 0x80, 0x80, alpha);
    BpPktSet(&p[2], g->u << 4, g->v << 4, 0, 0);
    BpPktSet(&p[3], BP_GS_X(x - g->w), BP_GS_Y(y - g->h), 100, 0);
    BpPktSet(&p[4], (g->u + g->w) * 16, (g->v + g->h) * 16, 0, 0);
    BpPktSet(&p[5], BP_GS_X(x), BP_GS_Y(y), 100, 0);

    // PC: the same glyph on the renderer.
    float xy[8];
    float uv[8];

    BpHostSpriteCorners(xy, x - g->w, y - g->h, x, y);
    BpHostSpriteCorners(uv, g->u, g->v, g->u + g->w, g->v + g->h);
    BpHostTexQuad(xy, uv, 100, 0x808080, alpha);
    return &p[6];
}

// ──────────────────────────────────────────────────────────────────────
// Draws `value` right-aligned at x (bottom at y), at least `digits` digits
// (leading zeros), with the separator every 3 digits when the font has one.
// FF3 0x00108838.

int B2dNumDraw(int value, int x, int y, u_char alpha, int digits)
{
    int neg   = value < 0;
    int group = 0;
    int n     = 0;
    int v     = neg ? -value : value;

    BP_PKT *p = (BP_PKT *)StartDmaDirectTrans();

    for (;;)
    {
        int                  prev  = v;
        const B2D_NUM_GLYPH *glyph = &b2d_num_font.glyph[v % 10];

        p  = B2dNumPutGlyph(p, glyph, x, y, alpha);
        x -= glyph->w;
        v /= 10;
        if (prev == 0 && digits <= n)
            break;

        group++;
        if (group == 3 && (b2d_num_font.flags & 1) && v != 0)
        {
            p     = B2dNumPutGlyph(p, &b2d_num_font.glyph[B2D_NUM_SEP], x, y, alpha);
            group = 0;
            x    -= b2d_num_font.glyph[B2D_NUM_SEP].w;
        }
        n++;
        if (v == 0 && digits <= n)
            break;
    }
    if (neg && (b2d_num_font.flags & 2))
        p = B2dNumPutGlyph(p, &b2d_num_font.glyph[B2D_NUM_MINUS], x, y, alpha);
    EndDmaDirectTrans((qword *)p);
    return 0;
}
