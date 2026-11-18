// graphics/graph2d/tim2.c -- TIM2 texture sends (FF3 0x25c300..0x25cc20).
// Ported from MioPan's FF2 tim2.c (MakeTim2Direct .. MakeTim2SendPacket_Mono),
// changed to FF3's (tim2, offset, tbp) argument order and FF3's packet calls;
// PK4SendVram and Tim2PackSend are FF3-only. See tim2.h.

#include "graphics/graph2d/tim2.h"

#include "common/pk4.h"                     // Pk4GetData, PK4_HEADER
#include "graphics/graph2d/g2d_draw.h"      // StartDmaDirectTrans, EndDmaDirectTrans, SetDmaDirectImageTrans
#include "reipan/gs/reipan_gs_c.h"          // PC: ReiPan_GsUpload
#include "sdk/libgraph.h"                   // SCE_GS_*, sceGsSetDefLoadImage

// TEX0 fields (GS manual bit positions).
#define TIM2_TEX0_TBP0(t)   ((u_int)(t) & 0x3fff)                   // bits 0..13
#define TIM2_TEX0_TBW(t)    ((u_int)((t) >> 14) & 0x3f)             // bits 14..19
#define TIM2_TEX0_PSM(t)    ((u_int)((t) >> 20) & 0x3f)             // bits 20..25
#define TIM2_TEX0_CBP(t)    ((u_int)((t) >> 37) & 0x3fff)           // bits 37..50

// GIF tag of the A+D packet before an image transfer: NLOOP 5, EOP, PACKED,
// NREG 1 (A+D).
#define TIM2_TRANS_GIFTAG   SCE_GIF_SET_TAG(5, 1, 0, 0, SCE_GIF_PACKED, 1)

// ──────────────────────────────────────────────────────────────────────
// PC: the texels of a transfer straight into the GS model (MioPan's
// Tim2HostUpload); the packet FF3 builds next is not walked on the host.

static void Tim2HostUpload(int dbp, int dbw, int dpsm, int w, int h, const void *src)
{
    sceGsLoadImage li;

    if (src == nullptr || w <= 0 || h <= 0)
        return;

    sceGsSetDefLoadImage(&li, (short)dbp, (short)dbw, (short)dpsm, 0, 0, (short)w, (short)h);
    ReiPan_GsUpload(&li, (unsigned char *)src);
}

// ──────────────────────────────────────────────────────────────────────
// TEXFLUSH, BITBLTBUF, TRXPOS 0, TRXREG, TRXDIR 0 (host -> local) as one
// direct packet, then the image data. FF3 writes this inline in each sender.

static void Tim2SendTrans(u_long bitbltbuf, u_long trxreg, const void *data, u_int size)
{
    u_long *p = (u_long *)StartDmaDirectTrans();

    p[0]  = TIM2_TRANS_GIFTAG;
    p[1]  = SCE_GIF_PACKED_AD;
    p[2]  = 0;
    p[3]  = SCE_GS_TEXFLUSH;
    p[4]  = bitbltbuf;
    p[5]  = SCE_GS_BITBLTBUF;
    p[6]  = 0;
    p[7]  = SCE_GS_TRXPOS;
    p[8]  = trxreg;
    p[9]  = SCE_GS_TRXREG;
    p[10] = SCE_GS_SET_TRXDIR(0);
    p[11] = SCE_GS_TRXDIR;
    EndDmaDirectTrans((qword *)&p[12]);
    SetDmaDirectImageTrans(data, (int)(size >> 4));
}

// The clut transfer's rectangle: 16 colours as 8x2 (the GS CSM1 layout of a
// 4-bit clut), else 16 wide.
static u_long Tim2ClutTrxReg(const TIM2_PICTUREHEADER *ph)
{
    if (ph->ClutColors == 0x10)
        return SCE_GS_SET_TRXREG(8, 2);
    return SCE_GS_SET_TRXREG(0x10, ph->ClutColors >> 4);
}

// ──────────────────────────────────────────────────────────────────────
// GS buffer width (in 64-texel units) of a w-texel-wide image of pixel
// storage mode psm; 0 for modes it does not know. Unreferenced in FF3 (and
// FF2); FF3's jump table is FF2's. FF3 0x0025c300.

int Tim2CalcBufWidth(int psm, int w)
{
    int bw;

    bw = (w + 0x3f) >> 6;
    switch (psm)
    {
    case 0:
    case 1:
    case 2:
    case 10:
    case 0x1b:
    case 0x24:
    case 0x2c:
    case 0x30:
    case 0x31:
    case 0x32:
    case 0x3a:
        return bw;
    case 0x13:
    case 0x14:
        return bw + (bw & 1);
    default:
        return 0;
    }
}

// ──────────────────────────────────────────────────────────────────────
// The first picture header of a TIM2 file: the file header is 0x10 bytes,
// or 0x80 when its alignment byte (+5) is 1; any other value: null.
// FF2 inlined this in each sender. FF3 0x0025c368.

TIM2_PICTUREHEADER *Tim2GetFirstPicture(void *tim2)
{
    u_char *p     = (u_char *)tim2;
    u_char  align = p[5];

    if (align == 0)
        return (TIM2_PICTUREHEADER *)(p + 0x10);
    if (align == 1)
        return (TIM2_PICTUREHEADER *)(p + 0x80);
    return nullptr;
}

// ──────────────────────────────────────────────────────────────────────
// TEX0 of member `no` of a PK2 (unchecked), TBP0 / CBP moved by `offset`.
// Unreferenced in FF3. FF3 0x0025c390.

u_long GetTex0RegPK(void *pk2, int no, u_int offset)
{
    int *offtop = (int *)((u_char *)pk2 + 0x10);

    return GetTex0RegTM((u_char *)pk2 + offtop[no], offset);
}

// ──────────────────────────────────────────────────────────────────────
// The first picture's TEX0 with TBP0 and CBP moved by `offset` (each kept to
// its 14 bits), every other field intact; 0 for a bad file. Unreferenced in
// FF3. FF3 0x0025c3b8.

u_long GetTex0RegTM(void *tim2, u_int offset)
{
    TIM2_PICTUREHEADER *ph = Tim2GetFirstPicture(tim2);
    u_long              tex0;

    if (ph == nullptr)
        return 0;

    tex0 = ph->GsTex0;
    return (tex0 & 0xfff8001fffffc000ULL)
         | ((TIM2_TEX0_TBP0(tex0) + offset) & 0x3fff)
         | ((u_long)((TIM2_TEX0_CBP(tex0) + offset) & 0x3fff) << 37);
}

// ──────────────────────────────────────────────────────────────────────
// The first picture's image to VRAM at `tbp` (< 0: the TEX0's TBP0 plus
// `offset`), with the TEX0's TBW and PSM. Nothing when it has no image.
// FF3 dropped FF2's assert and "Size Is 0" print. FF3 0x0025c440.

void MakeTim2Direct(void *tim2, int offset, int tbp)
{
    TIM2_PICTUREHEADER *ph = Tim2GetFirstPicture(tim2);
    u_char             *image;
    u_long              tex0;
    int                 tbp0;

    if (ph == nullptr)                  // PC: FF3 reads through null here
        return;
    if (ph->ImageSize == 0)
        return;

    tex0 = ph->GsTex0;
    tbp0 = (int)TIM2_TEX0_TBP0(tex0) + offset;
    if (tbp >= 0)
        tbp0 = tbp;
    image = (u_char *)ph + ph->HeaderSize;

    Tim2HostUpload(tbp0, (int)TIM2_TEX0_TBW(tex0), (int)TIM2_TEX0_PSM(tex0),
                   ph->ImageWidth, ph->ImageHeight, image);

    // callcheck: StartDmaDirectTrans, EndDmaDirectTrans, SetDmaDirectImageTrans (FF3 inlines the packet; Tim2SendTrans here)
    Tim2SendTrans(SCE_GS_SET_BITBLTBUF(0, 0, 0, (u_int)tbp0, TIM2_TEX0_TBW(tex0), TIM2_TEX0_PSM(tex0)),
                  SCE_GS_SET_TRXREG(ph->ImageWidth, ph->ImageHeight),
                  image, ph->ImageSize);
}

// ──────────────────────────────────────────────────────────────────────
// The first picture's clut (after its image) to VRAM at `cbp` (< 0: the
// TEX0's CBP plus `offset`) as CT32, width 1. Nothing when it has no clut.
// FF3 0x0025c5d0.

void MakeClutDirect(void *tim2, int offset, int cbp)
{
    TIM2_PICTUREHEADER *ph = Tim2GetFirstPicture(tim2);
    u_char             *clut;
    u_long              trxreg;

    if (ph == nullptr)                  // PC: FF3 reads through null here
        return;
    if (ph->ClutSize == 0)
        return;

    if (cbp < 0)
        cbp = (int)TIM2_TEX0_CBP(ph->GsTex0) + offset;
    clut   = (u_char *)ph + ph->HeaderSize + ph->ImageSize;
    trxreg = Tim2ClutTrxReg(ph);

    Tim2HostUpload(cbp, 1, SCE_GS_PSMCT32, (int)(u_int)trxreg, (int)(u_int)(trxreg >> 32), clut);

    // callcheck: StartDmaDirectTrans, EndDmaDirectTrans, SetDmaDirectImageTrans (FF3 inlines the packet; Tim2SendTrans here)
    Tim2SendTrans(SCE_GS_SET_BITBLTBUF(0, 0, 0, (u_int)cbp, 1, SCE_GS_PSMCT32), trxreg,
                  clut, ph->ClutSize);
}

// ──────────────────────────────────────────────────────────────────────
// MakeClutDirect with the colours taken from another TIM2: size, CBP and
// colour count from tim2's picture, the data from mono's clut. Nothing when
// mono has no clut. FF2 MakeClutDirect_ChrMono(tim2, cbp, offset, mono).
// FF3 0x0025c738.

void MakeClutDirect_ChrMono(void *tim2, void *mono, int offset, int cbp)
{
    TIM2_PICTUREHEADER *ph = Tim2GetFirstPicture(tim2);
    TIM2_PICTUREHEADER *pm = Tim2GetFirstPicture(mono);
    u_char             *clut;
    u_long              trxreg;

    if (ph == nullptr || pm == nullptr) // PC: FF3 reads through null here
        return;
    if (pm->ClutSize == 0)
        return;

    if (cbp < 0)
        cbp = (int)TIM2_TEX0_CBP(ph->GsTex0) + offset;
    clut   = (u_char *)pm + pm->HeaderSize + pm->ImageSize;
    trxreg = Tim2ClutTrxReg(ph);

    Tim2HostUpload(cbp, 1, SCE_GS_PSMCT32, (int)(u_int)trxreg, (int)(u_int)(trxreg >> 32), clut);

    // callcheck: StartDmaDirectTrans, EndDmaDirectTrans, SetDmaDirectImageTrans (FF3 inlines the packet; Tim2SendTrans here)
    Tim2SendTrans(SCE_GS_SET_BITBLTBUF(0, 0, 0, (u_int)cbp, 1, SCE_GS_PSMCT32), trxreg,
                  clut, ph->ClutSize);
}

// ──────────────────────────────────────────────────────────────────────
// Image and clut of every TIM2 in a PK2. FF3 0x0025c8c0.

void PK2SendVram(void *pk2, int offset, int tbp, int cbp)
{
    int  num    = *(int *)pk2;
    int *offtop = (int *)((u_char *)pk2 + 0x10);

    for (; num > 0; num--, offtop++)
    {
        MakeTim2Direct((u_char *)pk2 + *offtop, offset, tbp);
        MakeClutDirect((u_char *)pk2 + *offtop, offset, cbp);
    }
}

// ──────────────────────────────────────────────────────────────────────
// Image and clut of TIM2 `no` of a PK2 (nothing when out of range).
// FF3 0x0025c960.

void PK2SendVramOne(void *pk2, int no, int offset, int tbp, int cbp)
{
    int *offtop = (int *)((u_char *)pk2 + 0x10);

    if (no >= 0 && no < *(int *)pk2)
    {
        MakeTim2Direct((u_char *)pk2 + offtop[no], offset, tbp);
        MakeClutDirect((u_char *)pk2 + offtop[no], offset, cbp);
    }
}

// ──────────────────────────────────────────────────────────────────────
// Image and clut of one TIM2 file. FF3 0x0025ca00.

void MakeTim2SendPacket_3Dpkt(void *tim2, int offset, int tbp, int cbp)
{
    MakeTim2Direct(tim2, offset, tbp);
    MakeClutDirect(tim2, offset, cbp);
}

// ──────────────────────────────────────────────────────────────────────
// Image of tim2 with the clut of mono. FF3 0x0025ca48.

void MakeTim2SendPacket_Mono(void *tim2, void *mono, int offset, int tbp, int cbp)
{
    MakeTim2Direct(tim2, offset, tbp);
    MakeClutDirect_ChrMono(tim2, mono, offset, cbp);
}

// ──────────────────────────────────────────────────────────────────────
// Image and clut of every TIM2 in a PK4 (a null pack has none).
// FF3 0x0025caa8.

void PK4SendVram(void *pk4, int offset, int tbp, int cbp)
{
    int num = 0;
    int i;

    if (pk4 != nullptr)
        num = ((PK4_HEADER *)pk4)->num;

    for (i = 0; i < num; i++)
    {
        MakeTim2Direct(Pk4GetData(pk4, i), offset, tbp);
        MakeClutDirect(Pk4GetData(pk4, i), offset, cbp);
    }
}

// ──────────────────────────────────────────────────────────────────────
// Image and clut of TIM2 `no` of a PK4 (nothing when out of range).
// FF3 0x0025cb60.

void Tim2PackSend(void *pk4, int no, int offset, int tbp, int cbp)
{
    int num = 0;

    if (pk4 != nullptr)
        num = ((PK4_HEADER *)pk4)->num;

    if (no >= 0 && no < num)
    {
        MakeTim2Direct(Pk4GetData(pk4, no), offset, tbp);
        MakeClutDirect(Pk4GetData(pk4, no), offset, cbp);
    }
}
