// ingame/photo/photo_make.c -- photo image production (FF2 photo_make.c;
// FF3 0x22ada0..0x22ca90, see photo_make.h).
//
// Ported from MioPan's photo_make.c and checked against FF3 function by
// function. FF3 differences from FF2:
//   * the GS packets go through StartDmaDirectTrans / EndDmaDirectTrans and
//     the capture passes set their registers with SetGsRegsCached from ROM
//     tables, restoring FRAME / XYOFFSET from constants (FF2: pdrawenv);
//   * DrawPhotoBuffer blends with ALPHA 0x44 (FF2 FIX 0x80) and puts the
//     sepia border in a function of its own (DrawPhotoPanel, pri 0x10);
//   * DispPhotoFrame1 no longer draws the scenery picture nor calls
//     SubFadeFrame; its passes sit at other priorities and the frozen frame
//     is page 0x3480 (photo_dat.c copies the shown frame there);
//   * the hint picture (DrawPhotoHinttex) is FF3's own: a TIM2 sent to
//     0x2bc0, an optional black quad tied to the brightness option, a
//     single sprite or four spread ones;
//   * DrawPhotoFrame takes its texture from the language pack (lang_effdat
//     6, or 8 in photo mode) and sends it;
//   * DrawSpecialFurnPhoto renders into FBP 0x1c7 (FF2 0x1d5).
//
// PC: nothing walks the DIRECT packets (g2d_draw.c), so each hand-built
// transfer has a host twin: DrawPhotoBuffer re-issues its upload
// (G2dHostUpload), the two capture passes register the screen rectangle
// their page holds (G2dRegisterScreenPage), and CopyScreenToBuffer2 reads
// that page back into the EE buffer it repacks (G2dHostPageReadback).

#include "ingame/photo/photo_make.h"

#include "common/vu0math.h"                     // Vu0Sinf
#include "graphics/effect/effect_oth.h"         // SubBlur, SubContrast
#include "graphics/graph2d/g2d_debug.h"         // DispSqrRect
#include "graphics/graph2d/g2d_draw.h"          // DISP_SPRT, DISP_SQAR, StartDmaDirectTrans, LocalCopyLtoB, lang_effdat
#include "graphics/graph2d/tim2.h"              // Tim2GetFirstPicture, MakeTim2SendPacket_3Dpkt
#include "graphics/gs_reg.h"                    // GS_REG_SET, SetGsRegsCached
#include "ingame/ingame_wrk.h"                  // ingame_wrk.plyr_wrk
#include "ingame/menu/anim_2d.h"                // LangTexSend
#include "ingame/photo/finder_photo.h"          // GetPhotoModeWrk
#include "ingame/photo/photo_comp.h"            // UncompressData
#include "ingame/plyr/player.h"                 // PLYR_FLG_000020
#include "outgame/option.h"                     // GetOptionWrk
#include "reipan/reipan_memory.h"               // PC: ReiPan_GetHostPointer
#include "sdk/libgraph.h"                       // SCE_GS_PSMCT32
#include "system/compress/compress.h"           // CheckPhotoExpandEnd, GetPhotoExpand
#include "system/os/system.h"                   // GetSysWrk


// ──────────────────────────────────────────────────────────────────────
// Data
// ──────────────────────────────────────────────────────────────────────

// The capture page of a stored picture, and the frame buffers' stride.
#define PHM_CAPTURE_TBP     0x2bc0
#define PHM_FRAME_STRIDE    0x1180

// The scenery photo's page (FRAME units; TBP 0x1c7 * 32 = 0x38e0).
#define PHM_FURN_FBP        0x1c7
#define PHM_FURN_TBP        0x38e0
// The thumbnail's page (FRAME units; TBP 0x15e * 32 = 0x2bc0).
#define PHM_SPHOTO_FBP      0x15e

// The fourteen border sprites (data 0x32f0d0, FF2's table unchanged): four
// corners, then bars cut from three strips; 6, 7, 9 are turned 180 degrees,
// 10, 11 270 and 12, 13 90 about their own position. tex0 is set at draw.
static SPRT_DAT photo_frame[14] =
{
    /*  tex0    u   v    w    h     x     y  pri  alp fl bln */
    {   0,      1, 49,  14,  14,   -2,   -4,   0, 128, 0, 1 },
    {   0,     17, 49,  14,  14,   -2,  246,   0, 128, 0, 1 },
    {   0,     33, 49,  14,  14,  372,   -4,   0, 128, 0, 1 },
    {   0,     49, 49,  14,  14,  372,  246,   0, 128, 0, 1 },
    {   0,      1,  1, 124,  14,   12,   -4,   0, 128, 0, 1 },
    {   0,      1, 17, 112,  14,  136,   -4,   0, 128, 0, 1 },
    {   0,      1,  1, 124,  14,  372,  260,   0, 128, 0, 1 },
    {   0,      1, 17, 112,  14,  248,  260,   0, 128, 0, 1 },
    {   0,      1, 33, 124,  14,  248,   -4,   0, 128, 0, 1 },
    {   0,      1, 33, 124,  14,  136,  260,   0, 128, 0, 1 },
    {   0,      1,  1, 124,  14,   -2,  246,   0, 128, 0, 1 },
    {   0,      1, 17, 112,  14,   -2,  122,   0, 128, 0, 1 },
    {   0,      1,  1, 124,  14,  386,   10,   0, 128, 0, 1 },
    {   0,      1, 17, 112,  14,  386,  134,   0, 128, 0, 1 },
};

// The two capture passes' draw state (rdata 0x35fbe0 / 0x35fcd8): ALPHA,
// TEX1, CLAMP, TEST AFAIL FB_ONLY and ZBUF without depth writes. The ROM
// copies a table to the stack and stores the ZBUF word again.
static const GS_REG_SET photo_small_regs[5] =
{
    { 0x44,             0x42 },     // ALPHA_1
    { 0x100000161ULL,   0x14 },     // TEX1_1
    { 0,                0x08 },     // CLAMP_1 repeat
    { 0x31003,          0x47 },     // TEST_1
    { 0x10a000118ULL,   0x4e },     // ZBUF_1
};
static const GS_REG_SET photo_furn_regs[5] =
{
    { 0x8000000044ULL,  0x42 },     // ALPHA_1 FIX 0x80
    { 0x100000161ULL,   0x14 },     // TEX1_1
    { 5,                0x08 },     // CLAMP_1 clamp
    { 0x31003,          0x47 },     // TEST_1
    { 0x10a000118ULL,   0x4e },     // ZBUF_1
};

// The hint picture's own-alpha pass (rdata 0x35fca8): TEST always, ALPHA
// 0x44, CLAMP clamp.
static const GS_REG_SET photo_hint_regs[3] =
{
    { 0x30003,          0x47 },
    { 0x44,             0x42 },
    { 5,                0x08 },
};

// Full-screen records (rdata 0x35fc30 / 0x35fc68 / 0x35fc50).
static const SPRT_DAT photo_screen_spr = { 0, 0, 0, 640, 448, 0, 0, 0, 128, 0, 1 };
static const SPRT_DAT photo_half_spr   = { 0, 0, 0, 320, 448, 0, 0, 0, 128, 0, 1 };
static const SQAR_DAT photo_screen_sqr = { 640, 448, 0, 0, 0, 0, 0, 0, 128 };

// ──────────────────────────────────────────────────────────────────────
// Stored pictures
// ──────────────────────────────────────────────────────────────────────

// The smallest power of two exponent with 1 << e >= n (FF2 utility.c
// log_2; FF3 keeps a copy here).
// FF3 0x0022ada0

static int PhotoLog2(int n)
{
    int e = 31;

    while (((n >> e) & 1) == 0)
    {
        if (--e <= 0)
            break;
    }
    return e + ((1 << e) < n);
}

// The sepia border round a w x h picture at (x, y), thicker for bigger
// pictures (2 px for a thumbnail, 6 for a full photo).
// FF3 0x0022c650

static void DrawPhotoPanel(float x, float y, float w, float h, u_char alp)
{
    float m  = (w - 48.0f) / 84.0f + 2.0f;
    float fw = m < 2.0f ? 2.0f : m;
    float fh = m < 2.0f ? 2.0f : m;
    float x0 = x - fw;
    float x1 = x + w + fw;
    float y0 = y - fh;
    float y1 = y + h + fh;

    DispSqrRect(x0, y0, x1, y, 0x10, 0xcf, 0xbd, 0xa1, alp);
    DispSqrRect(x0, y + h, x1, y1, 0x10, 0xcf, 0xbd, 0xa1, alp);
    DispSqrRect(x0, y0, x, y1, 0x10, 0xcf, 0xbd, 0xa1, alp);
    DispSqrRect(x + w, y0, x1, y1, 0x10, 0xcf, 0xbd, 0xa1, alp);
}

// FF3 0x0022add8

void DrawPhotoBuffer(u_int pri, uintptr_t addr, int szfl, int x, int y, int szw, int szh,
                     int mszw, int mszh, int ftype, u_char alp, int ztype)
{
    int        dbw;
    int        bw;
    int        rline;
    float      xx;
    float      yy;
    SPRT_DAT2  sd = {};             // FF3 leaves it uninitialised; every field read is overwritten
    DISP_SPRT2 ds;

    (void)pri;
    (void)szfl;
    (void)ztype;

    // Centred coordinates, converted back before use (dead in the ROM too).
    xx = (float)x - 320.0f - 1.0f;
    yy = (float)y - 224.0f - 1.0f;

    dbw = (mszw - 1) / 64 + 1;
    bw  = dbw * 64;

    for (rline = mszh; rline > 0;)
    {
        int     oline = rline > 100 ? 100 : rline;
        int     top   = mszh - rline;
        u_long *p;

        rline -= oline;

        p = (u_long *)StartDmaDirectTrans();
        p[2] = ((u_long)dbw << 48) | ((u_long)PHM_CAPTURE_TBP << 32);   // BITBLTBUF CT32
        p[8] = 0;                                                       // TRXDIR host -> local
        p[6] = (u_long)bw | ((u_long)oline << 32);                      // TRXREG
        p[4] = (u_long)top << 48;                                       // TRXPOS DSAY
        p[1] = 0xe;
        p[3] = 0x50;
        p[0] = 0x1000000000008004ULL;
        p[9] = 0x53;
        p[5] = 0x51;
        p[7] = 0x52;
        EndDmaDirectTrans((qword *)&p[10]);
        SetDmaDirectImageTrans(ReiPan_GetHostPointer(addr + (uintptr_t)(bw * top / 4 * 16)),
                               bw * oline / 4);

        // PC: the packet above is not walked; put the strip in GS memory.
        G2dHostUpload(PHM_CAPTURE_TBP, dbw, SCE_GS_PSMCT32, 0, top, bw, oline,
                      ReiPan_GetHostPointer(addr + (uintptr_t)(bw * top * 4)));
    }

    xx += 320.0f;
    yy += 224.0f;

    CopySprDToSpr2(&ds, &sd);
    ds.tex0 = ((u_long)PhotoLog2(bw) << 26) | 0x2000000000102bc0ULL | ((u_long)dbw << 14)
            | ((u_long)PhotoLog2(mszh) << 30);                          // TBP 0x2bc0, CT24, CLD 1
    if (ftype != PHOTO_FTYPE_INSET)
    {
        ds.u1 = 0;
        ds.v1 = 0;
        ds.u2 = (u_short)(int)((float)mszw * 16.0f);
        ds.v2 = (u_short)(int)((float)mszh * 16.0f);
    }
    else
    {
        ds.u1 = 0x80;
        ds.v1 = 0x80;
        ds.u2 = (u_short)(int)(((float)mszw - 8.0f) * 16.0f);
        ds.v2 = (u_short)(int)(((float)mszh - 8.0f) * 16.0f);
    }
    ds.r      = 0x80;
    ds.g      = 0x80;
    ds.b      = 0x80;
    ds.z      = 0xfff00;
    ds.w      = (float)szw;
    ds.h      = (float)szh;
    ds.zbuf   = 0x10a000118ULL;
    ds.alpreg = 0x44;
    ds.test   = 0x30003;
    ds.x      = xx;
    ds.y      = yy;
    ds.alp    = alp;
    DispSprD2(&ds);

    if (ftype == PHOTO_FTYPE_PANEL)
        DrawPhotoPanel(xx, yy, ds.w, ds.h, alp);
    else if (ftype == PHOTO_FTYPE_FRAME)
        DrawPhotoFrame(xx, yy);
}

// FF3 0x0022b1a0

void CopyScreenToBuffer2(int addr_i, uintptr_t addr_o, int szfl, int mx, int my, int mw, int mh)
{
    int    onelo = (mw - 1) / 64 * 64 + 64;
    u_int *src;
    u_int *dst;
    int    x;
    int    y;

    (void)szfl;

    PK2DKick();
    LocalCopyLtoB(3, 1, addr_i);

    // PC: LocalCopyLtoB hands a page stored into a stash on from capture
    // slot to capture slot; this copy reads the pixels, so read them back.
    src = (u_int *)ReiPan_GetHostPointer(PHOTO_EXPAND_ADDR);
    G2dHostPageReadback(addr_i, (u_char *)src, 640, 224);

    dst = (u_int *)ReiPan_GetHostPointer(addr_o);
    for (y = 0; y < mh; y++)
    {
        for (x = 0; x < mw; x++)
            dst[y * onelo + x] = src[(my / 2 + y) * 640 + mx + x];
    }
}

// The album thumbnail: the shown frame's 384x256 window at (x, y) drawn
// 45x15 into FBP 0x15e, FRAME / XYOFFSET put back after.
// FF3 0x0022b2b0

void MakeSmallPhotoV(float x, float y)
{
    GS_REG_SET regs[5];
    u_int      cnt;
    u_long    *p;
    int        i;

    for (i = 0; i < 5; i++)
        regs[i] = photo_small_regs[i];
    regs[4].data = 0x10a000118ULL;
    SetGsRegsCached(regs, 5, 1);

    p = (u_long *)StartDmaDirectTrans();
    p[1]  = 0xe;
    p[0]  = 0x1000000000008004ULL;
    p[2]  = 0;
    p[3]  = 0x3f;                                   // TEXFLUSH
    p[5]  = 0x4c;
    p[4]  = 0x1010000ULL | PHM_SPHOTO_FBP;          // FRAME_1 FBW 1, CT24
    p[7]  = 0x18;
    p[6]  = 0x790800006c08ULL;                      // XYOFFSET_1
    cnt   = GetSysWrk()->count;
    p[9]  = 0x06;
    p[8]  = (u_long)((cnt & 1) * PHM_FRAME_STRIDE) | 0xe68128000ULL;   // TEX0_1 the shown frame
    p[10] = 0x408b400000008001ULL;                  // GIF tag: SPRITE, UV XYZF2 x2
    p[11] = 0x4343;
    {
        u_int *v = (u_int *)&p[12];

        v[0]  = (u_int)((int)x * 16 + 8);           // UV
        v[1]  = (u_int)((int)y * 16 + 8);
        v[4]  = 0x6c00;                             // XYZF2
        v[5]  = 0x7900;
        p[15] = 0;
        v[8]  = (u_int)((int)x * 16 + 0x17f8);
        v[9]  = (u_int)((int)y * 16 + 0xff8);
        v[12] = 0x6ed0;
        v[13] = 0x79f0;
        p[19] = 0;
    }
    p[21] = 0xe;
    p[20] = 0x1000000000008002ULL;
    p[23] = 0x4c;
    p[22] = (u_long)((GetSysWrk()->count & 1) * 0x8c) | 0x10a0000ULL;  // FRAME_1 back
    p[25] = 0x18;
    p[24] = 0x720000006c00ULL;                      // XYOFFSET_1 back
    EndDmaDirectTrans((qword *)&p[26]);

    // PC: the page now holds that window of the screen at 45x15.
    G2dRegisterScreenPage(PHM_CAPTURE_TBP, (int)x, (int)y, PHOTO_W, PHOTO_H * 2,
                          SPHOTO_W, SPHOTO_H);
}

// Thumbnails (45x15, drawn 45x30 unless the caller sizes them).
// FF3 0x0022b528 (unreferenced)

void DrawSPhotoFromSmallPhotoArea(int n, int pri, int ftype, int x, int y, u_char alp)
{
    DrawPhotoBuffer((u_int)pri, SPHOTO_AREA + (uintptr_t)(n * SPHOTO_ONE_SIZE), 0, x, y,
                    SPHOTO_W, SPHOTO_H * 2, SPHOTO_W, SPHOTO_H, ftype, alp, 0);
}

// FF3 0x0022b580 (unreferenced)

void DrawSPhotoFromSmallPhotoAreaAddr(uintptr_t addr, int n, int pri, int ftype, int x, int y,
                                      u_char alp)
{
    DrawPhotoBuffer((u_int)pri, addr + (uintptr_t)(n * SPHOTO_ONE_SIZE), 0, x, y,
                    SPHOTO_W, SPHOTO_H * 2, SPHOTO_W, SPHOTO_H, ftype, alp, 0);
}

// FF3 0x0022b5d8

void DrawSPhotoFromSmallPhotoArea2(int n, int pri, int ftype, int x, int y, int szw, int szh,
                                   u_char alp)
{
    DrawPhotoBuffer((u_int)pri, SPHOTO_AREA + (uintptr_t)(n * SPHOTO_ONE_SIZE), 0, x, y,
                    szw, szh, SPHOTO_W, SPHOTO_H, ftype, alp, 0);
}

// FF3 0x0022b628

void DrawSPhotoFromSmallPhotoAreaAddr2(uintptr_t addr, int n, int pri, int ftype, int x, int y,
                                       int szw, int szh, u_char alp)
{
    DrawPhotoBuffer((u_int)pri, addr + (uintptr_t)(n * SPHOTO_ONE_SIZE), 0, x, y,
                    szw, szh, SPHOTO_W, SPHOTO_H, ftype, alp, 0);
}

// The decoded picture, once the decoder is done; the first call requests it
// and decodes the requested slot of PHOTO_AREA.
// FF3 0x0022b688

void DrawPhotoFromWorkArea(int pri, int ftype, int x, int y, int szw, int szh, u_char alp)
{
    if (CheckPhotoExpandEnd())
    {
        DrawPhotoBuffer((u_int)pri, PHOTO_EXPAND_ADDR, 0, x, y, szw, szh, PHOTO_W, PHOTO_H,
                        ftype, alp, 0);
    }
    else if (GetPhotoExpand() == 1)
    {
        UncompressData(PHOTO_AREA, 0xff, PHOTO_EXPAND_ADDR);
    }
}

// The same from an album buffer (its pictures at addr + 0x10000).
// FF3 0x0022b780

void DrawPhotoFromWorkAreaAddr(uintptr_t addr, int pri, int ftype, int x, int y, int szw,
                               int szh, u_char alp)
{
    uintptr_t addr2 = addr + 0x10000;

    if (CheckPhotoExpandEnd())
    {
        DrawPhotoBuffer((u_int)pri, PHOTO_EXPAND_ADDR, 0, x, y, szw, szh, PHOTO_W, PHOTO_H,
                        ftype, alp, 0);
    }
    else if (GetPhotoExpand() == 1)
    {
        UncompressData(addr2, 0xff, PHOTO_EXPAND_ADDR);
    }
}

// ──────────────────────────────────────────────────────────────────────
// Presentation
// ──────────────────────────────────────────────────────────────────────

// fl 3: the shown frame drawn over itself, fading over 32 frames. Else: a
// black quad the size of the picture writing depth, the frozen frame (page
// 0x3480, half width stretched) at 64 + 191 * iPercent / 100, the flash
// (fl 1: a sine over 19 / 59 frames driving a blur and a contrast lift),
// then the frozen frame at a quarter and a grey wash, both masked to
// outside the picture by the depth the quad wrote.
// FF3 0x0022b888

void DispPhotoFrame1(float x, float y, int fl, int iPercent)
{
    static int    cnt;                  // sdata gp-0x1bf0: reset, never read
    static u_char alp1;                 // gp-0x1bec: reset to 0x80, never read (FF2: the scenery picture's alpha)
    static u_char alp2;                 // gp-0x1beb: the fade of fl 3
    static int    flash_fr;             // gp-0x1be8
    static int    fl_cnt;               // gp-0x1be4
    DISP_SPRT ds;
    DISP_SQAR dq;
    u_char    rgb;
    int       i;

    if (fl == 0)
    {
        cnt      = 0;
        alp1     = 0x80;
        alp2     = 0;
        flash_fr = (ingame_wrk.plyr_wrk->flags & PLYR_FLG_000020) ? 0x13 : 0x3b;
        fl_cnt   = flash_fr;
    }
    (void)cnt;
    (void)alp1;

    rgb = (u_char)(iPercent * 0xbf / 100 + 0x40);

    if (fl == 3)
    {
        u_char a = (u_char)(0x80 - alp2 * 4);

        CopySprDToSpr(&ds, &photo_screen_spr);
        ds.alpha  = a;
        ds.b      = 0x80;
        ds.r      = 0x80;
        ds.g      = 0x80;
        ds.z      = 0xfffef;
        ds.zbuf   = 0xa000118;
        ds.tex0   = (u_long)(((GetSysWrk()->count + 1) & 1) * PHM_FRAME_STRIDE)
                  | 0x28128000ULL | 0x2000000240000000ULL;  // the shown frame, CT24
        ds.test   = 0x50003;
        ds.tex1   = 0x161;
        ds.alphar = 0x44;
        ds.x      = -0.5f;
        ds.y      = -1.0f;
        ds.pri    = 0x10;
        DispSprD(&ds);

        alp2 = alp2 < 0x20 ? (u_char)(alp2 + 1) : 0x20;
        return;
    }

    CopySqrDToSqr(&dq, &photo_screen_sqr);
    dq.z    = 0xfffdf;
    dq.pri  = 0x20;
    dq.x[0] = (int)x;
    dq.x[1] = dq.x[0] + PHOTO_W;
    dq.x[2] = dq.x[0];
    dq.x[3] = dq.x[1];
    dq.y[0] = (int)y;
    dq.y[1] = dq.y[0];
    dq.y[2] = dq.y[0] + PHOTO_H * 2;
    dq.y[3] = dq.y[2];
    dq.zbuf = 0xa000118;
    dq.test = 0x30003;
    for (i = 0; i < 4; i++)
    {
        dq.r[i] = 0;
        dq.g[i] = 0;
        dq.b[i] = 0;
    }
    dq.alpha = 0x80;
    DispSqrD(&dq);

    CopySprDToSpr(&ds, &photo_half_spr);
    ds.b      = rgb;
    ds.sch    = 1.0f;
    ds.csx    = -0.5f;
    ds.x      = -0.5f;
    ds.r      = rgb;
    ds.z      = 0xfffef;
    ds.csy    = -1.0f;
    ds.y      = -1.0f;
    ds.scw    = 2.0f;
    ds.tex0   = 0x2000000264117480ULL;  // page 0x3480, 320 wide, CT24
    ds.zbuf   = 0x10a000118ULL;
    ds.alphar = 0x44;
    ds.g      = rgb;
    ds.test   = 0x30003;
    ds.pri    = 0x10;
    DispSprD(&ds);

    if (fl == 1 && fl_cnt > 0)
    {
        float s = Vu0Sinf((float)(fl_cnt * 90 / flash_fr) * 3.1415927f / 180.0f);
        u_char c = (u_char)(int)(s * 160.0f);

        s = Vu0Sinf((float)(fl_cnt * 90 / flash_fr) * 3.1415927f / 180.0f);
        SubBlur(1.0f, 1.0f, 180.0f, 320.0f, 112.0f, 1, (u_char)(int)(s * 80.0f), 1);
        SubContrast(0, c, c);
        fl_cnt--;
    }

    CopySprDToSpr(&ds, &photo_screen_spr);
    ds.zbuf   = 0xa000118;
    ds.test   = 0x5000d;
    ds.csy    = -1.0f;
    ds.csx    = -0.5f;
    ds.tex0   = 0x2000000264117480ULL;
    ds.z      = 0xfffbf;
    ds.alphar = 0x44;
    ds.y      = -1.0f;
    ds.b      = 0x40;
    ds.pri    = 0x40;
    ds.scw    = 2.0f;
    ds.sch    = 1.0f;
    ds.x      = -0.5f;
    ds.r      = 0x40;
    ds.g      = 0x40;
    DispSprD(&ds);

    CopySqrDToSqr(&dq, &photo_screen_sqr);
    dq.zbuf   = 0xa000118;
    dq.test   = 0x5000d;
    dq.z      = 0xfffcf;
    dq.alphar = 0x84;
    dq.pri    = 0x30;
    for (i = 0; i < 4; i++)
    {
        dq.r[i] = 0x30;
        dq.g[i] = 0x30;
        dq.b[i] = 0x30;
    }
    dq.alpha = 200;
    DispSqrD(&dq);
}

// The hint sprite: 384 / w by 256 / h (whole division, as the ROM), centred
// on its position, no depth write.
// FF3 0x0022bec8

static void PhotoHintSprSet(DISP_SPRT *ds, const SPRT_DAT *sd, u_char alpha)
{
    CopySprDToSpr(ds, sd);
    ds->tex1   = 0x161;
    ds->zbuf   = 0x10a000118ULL;
    ds->test   = 0x30003;
    ds->scw    = 384.0f / (float)sd->w;
    ds->csx    = ds->x;
    ds->alpha  = alpha;
    ds->csy    = ds->y;
    ds->alphar = 0x44;
    ds->sch    = (float)(0x100 / sd->h);
}

// The same through its own alpha: ATST GEQUAL aref, (Cs - Cd) * alpha + Cd.
// FF3 0x0022bf90

static void PhotoHintSprSetFix(DISP_SPRT *ds, const SPRT_DAT *sd, int alpha, int aref)
{
    PhotoHintSprSet(ds, sd, (u_char)alpha);
    ds->test   = ((u_long)(int64_t)aref << 4) | 0x3000b;
    ds->b      = 0x80;
    ds->alphar = ((u_long)(int64_t)alpha << 32) | 0x64;
    ds->tex1   = 0x161;
    ds->r      = 0x80;
    ds->g      = 0x80;
}

// The TIM2's first picture placed at VRAM tbp: TBP0 tbp + 0x82, CBP tbp,
// TBW from TW, TCC 1, CLD 1; written back into the header and returned.
// FF3 0x0022c008

static u_long PhotoHintTim2SetTex0(void *tim2, int tbp)
{
    TIM2_PICTUREHEADER *ph = Tim2GetFirstPicture(tim2);
    u_long              t  = ph->GsTex0;
    int                 tw;

    t  = (t & ~0x3fffULL) | (u_long)((tbp + 0x82) & 0x3fff);
    t  = (t & ~(0x3fffULL << 37)) | ((u_long)(tbp & 0x3fff) << 37);
    tw = (int)((t >> 26) & 0xf);
    t  = (t & 0xfffffffffff03fffULL) | ((u_long)(((1 << tw) / 64) & 0x3f) << 14);
    t |= 1ULL << 34;
    t  = (t & 0x1fffffffffffffffULL) | (1ULL << 61);
    ph->GsTex0 = t;
    return t;
}

// FF3 0x0022c0d0

int PhotoHintAlpha(int step, int cnt, int max)
{
    int k;

    if (step == 1 || cnt >= 0x33)
        return max;

    if (step == 0)
        k = 0x32 - cnt;
    else if (step == 2)
        k = cnt;
    else
        k = 0;
    return max * k / 0x32;
}

// The hint picture over the photo. A special subject (special) at a
// brightness setting of 7 or more first darkens the picture's area; the
// TIM2 goes to 0x2bc0 and is drawn w x h scaled to 384x256 at (x, y), once,
// or (PHOTO_HINT_SPREAD) four times at a third of the alpha, spread by up to
// 3 px while faint.
// FF3 0x0022c128

void DrawPhotoHinttex(float x, float y, int w, int h, int alpha, void *tim2, int flags,
                      int special)
{
    OPTION_WRK *opt = GetOptionWrk();   // FF3 caches it in gp-0xaec
    SPRT_DAT    sd;
    DISP_SPRT   ds;
    u_char      dark = 0;

    sd.tex0  = 0;
    sd.u     = 0;
    sd.v     = 0;
    sd.w     = (u_short)w;
    sd.h     = (u_short)h;
    sd.x     = 0;
    sd.y     = 0;
    sd.pri   = 0;
    sd.alpha = 0x80;
    sd.flip  = 0;
    sd.bln   = 0;

    if (special != 0)
    {
        s_char b = (s_char)opt->now.b00;    // FF3 reads it signed

        if (b >= 7)
            dark = (u_char)((((b * 0x500 - 0x1e00) / 100) & 0xff) * alpha / 0x66);
        if (dark != 0)
        {
            DISP_SQAR dq;
            int       i;

            CopySqrDToSqr(&dq, &photo_screen_sqr);
            dq.pri  = 0x20;
            dq.z    = 0xfffdf;
            dq.zbuf = 0x10a000118ULL;
            dq.test = 0x30003;
            dq.x[0] = (int)x;
            dq.x[1] = dq.x[0] + PHOTO_W;
            dq.x[2] = dq.x[0];
            dq.x[3] = dq.x[1];
            dq.y[0] = (int)y;
            dq.y[1] = dq.y[0];
            dq.y[2] = dq.y[0] + PHOTO_H * 2;
            dq.y[3] = dq.y[2];
            for (i = 0; i < 4; i++)
            {
                dq.r[i] = 0;
                dq.g[i] = 0;
                dq.b[i] = 0;
            }
            dq.alpha = dark;
            DispSqrD(&dq);
        }
    }

    sd.tex0 = PhotoHintTim2SetTex0(tim2, PHM_CAPTURE_TBP);
    MakeTim2SendPacket_3Dpkt(tim2, 0, -1, -1);

    if ((flags & PHOTO_HINT_FIX) == 0)
    {
        PhotoHintSprSet(&ds, &sd, (u_char)alpha);
    }
    else
    {
        SetGsRegsCached(photo_hint_regs, 3, 1);
        PhotoHintSprSetFix(&ds, &sd, alpha, 0x80 - alpha);
    }

    if (flags & PHOTO_HINT_ADD)
        ds.alphar = 0x48;

    if (flags & PHOTO_HINT_SPREAD)
    {
        float d = (1.0f - (float)alpha * 0.0078125f) * 3.0f;

        ds.alpha = (u_char)(alpha / 3);
        ds.x = x - d;
        ds.y = y - d;
        DispSprD(&ds);
        ds.y = y + d;
        ds.x = x + d;
        DispSprD(&ds);
        ds.x = x - d;
        ds.y = y + d;
        DispSprD(&ds);
        ds.x = x + d;
        ds.y = y - d;
    }
    else
    {
        ds.x = x;
        ds.y = y;
    }
    DispSprD(&ds);
}

// The fourteen-sprite border round a 384x256 picture with its corner at
// (x, y), from the language pack's frame picture (6, or 8 in photo mode).
// FF3 0x0022c4c0

void DrawPhotoFrame(float x, float y)
{
    DISP_SPRT ds;
    int       ix = (int)x;
    int       iy = (int)y;
    int       no = GetPhotoModeWrk()->GetMode() != 0 ? 8 : 6;
    u_long    tex0;
    int       i;

    tex0 = lang_effdat[no].tex0;
    LangTexSend(no);

    for (i = 0; i < 14; i++)
    {
        CopySprDToSpr(&ds, &photo_frame[i]);
        ds.tex0 = tex0;
        ds.x += (float)ix;
        ds.y += (float)iy;
        if ((u_int)(i - 6) < 2 || i == 9)
        {
            ds.crx = ds.x;
            ds.cry = ds.y;
            ds.rot = 180.0f;
        }
        if ((u_int)(i - 10) < 2)
        {
            ds.rot = 270.0f;
            ds.crx = ds.x;
            ds.cry = ds.y;
        }
        if ((u_int)(i - 12) < 2)
        {
            ds.rot = 90.0f;
            ds.crx = ds.x;
            ds.cry = ds.y;
        }
        DispSprD(&ds);
    }
}

// A scenery shot: the drawn frame's 384x256 window at (x, y) rendered
// 384x128 into FBP 0x1c7, FRAME / XYOFFSET put back after.
// FF3 0x0022c7d8

void DrawSpecialFurnPhoto(float x, float y)
{
    GS_REG_SET regs[5];
    u_long    *p;
    int        i;

    for (i = 0; i < 5; i++)
        regs[i] = photo_furn_regs[i];
    regs[4].data = 0x10a000118ULL;
    SetGsRegsCached(regs, 5, 1);

    p = (u_long *)StartDmaDirectTrans();
    p[1]  = 0xe;
    p[0]  = 0x1000000000008004ULL;
    p[2]  = 0;
    p[3]  = 0x3f;                                   // TEXFLUSH
    p[5]  = 0x4c;
    p[4]  = 0x1060000ULL | PHM_FURN_FBP;            // FRAME_1 FBW 6, CT24
    p[7]  = 0x18;
    p[6]  = 0x7c0000007400ULL;                      // XYOFFSET_1
    p[9]  = 0x06;
    p[8]  = (u_long)((GetSysWrk()->count & 1) * PHM_FRAME_STRIDE) | 0x268128000ULL; // TEX0_1 the drawn frame
    p[10] = 0x408b400000008001ULL;                  // GIF tag: SPRITE, UV XYZF2 x2
    p[11] = 0x4343;
    {
        u_int *v = (u_int *)&p[12];

        v[0]  = (u_int)(int)(x * 16.0f);            // UV
        v[1]  = (u_int)(int)(y * 16.0f);
        v[4]  = 0x7400;                             // XYZF2
        v[5]  = 0x7c00;
        p[15] = 0;
        v[8]  = (u_int)(int)((x + 384.0f) * 16.0f);
        v[9]  = (u_int)(int)((y + 256.0f) * 16.0f);
        v[12] = 0x8c00;
        v[13] = 0x8400;
        p[19] = 0;
    }
    p[21] = 0xe;
    p[20] = 0x1000000000008002ULL;
    p[23] = 0x4c;
    p[22] = (u_long)((GetSysWrk()->count & 1) * 0x8c) | 0x10a0000ULL;  // FRAME_1 back
    p[25] = 0x18;
    p[24] = 0x720000006c00ULL;                      // XYOFFSET_1 back
    EndDmaDirectTrans((qword *)&p[26]);

    // PC: the page now holds that window of the screen at 384x128.
    G2dRegisterScreenPage(PHM_FURN_TBP, (int)x, (int)y, PHOTO_W, PHOTO_H * 2, PHOTO_W, PHOTO_H);
}
