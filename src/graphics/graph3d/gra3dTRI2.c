/* ==========================================================================
 *  graphics/graph3d/gra3dTRI2.c
 *
 *  TRI2 texture files embedded in SGD models. A TRI2 file is a run of
 *  SGDTRI2FILEHEADER records; each is one VIF1 DIRECT packet holding GS
 *  load-image descriptors (a sceGsLoadImage -- A+D GIFtag, BITBLTBUF /
 *  TRXPOS / TRXREG / TRXDIR, IMAGE GIFtag -- followed by its texel
 *  quadwords). This module
 *    - sends a run of headers to VRAM, down the VIF1 DMA chain or directly
 *      (gra3dLoadTRI2FileToVRAM),
 *    - measures the VRAM span a header's descriptors cover
 *      (gra3dGetTRI2SizeData),
 *    - reads a VRAM span back into fresh headers, at most 0x3f pages each
 *      (gra3dGenerateTRI2FileFromVRAM, _MakeTRI2FileByVRAMImage,
 *      _MakeTRI2FileHeader) -- RebuildTRI2Files (gra3dSGDData.c) uses the
 *      last two to repack a unit's textures into one contiguous block (dead
 *      in FF3: sgdEnableOptimizeTexture has no callers).
 *
 *  Ported from MioPan's gra3dTRI2.c (FF2 gra3dTRI2.c). FF3's five functions
 *  (0x1e6698..0x1e6ae8) are FF2's in FF2 order, without FF2's asserts; the
 *  VRAM read-back calls sceGsExecStoreImage / sceGsSyncPath directly.
 *
 *  PC: the VIF1 chain is recorded, not walked (g3dDma.c), so the headers the
 *  EE hands to it are also uploaded to the emulated GS here, every descriptor
 *  of the packet, leaving GS memory as the PS2 would have it for whatever
 *  samples or reads it back. MioPan's texture-binding hooks around this
 *  upload come with the graph3d host bridge (open-items.md, M4).
 * ======================================================================== */

#include "graphics/graph3d/gra3dTRI2.h"

#include <eekernel.h>                   // FlushCache
#include <libgraph.h>                   // sceGs*Image, sceGsSyncPath, SCE_GS_* / SCE_GIF_*

#include "graphics/graph3d/g3dDma.h"     // g3dDmaAddPacket
#include "graphics/graph3d/sgd_types.h"  // SGDTRI2FILEHEADER, TRI2SIZEDATA

#include "reipan/rendering/reipan_graph3d.h"    // PC: ReiPan_Graph3dTextureUploadBound / NoteTextureUpload

// DMA channels gra3dLoadTRI2FileToVRAM takes (sceDmaChan ids).
#define SCE_DMA_VIF1            1
#define SCE_DMA_GIF             2

#define TRI2_MAX_PAGES          0x3f    // pages read back per header

// The header _MakeTRI2FileHeader starts from. FF3 stores it inline (28 word
// stores), FF2 copied it from .rodata; the bytes are the same. The four
// fields the call fills in are poisoned with 0xdeadbeef cut to the field
// width (DIRECT, BITBLTBUF.DBP, the IMAGE tag's NLOOP -- bit 15 of 0xbeef
// lands on EOP, which wants to be 1). TRXREG's poison sits in the pad
// between RRW and RRH and RRH is 0: the initialiser was one slot short.
static const SGDTRI2FILEHEADER s_TRI2FileHeaderDefault =
{
    .uiVif1Code_NOP0   = 0,
    .uiVif1Code_NOP1   = 0,
    .uiVif1Code_FLUSH  = 0,
    .uiVif1Code_DIRECT = 0xdeadbeef,
    .gsli =
    {
        // one PACKED loop of four A+D register items
        .giftag        = { .NLOOP = 1, .EOP = 0, .FLG = SCE_GIF_PACKED, .NREG = 4,
                           .REGS0 = SCE_GIF_PACKED_AD, .REGS1 = SCE_GIF_PACKED_AD,
                           .REGS2 = SCE_GIF_PACKED_AD, .REGS3 = SCE_GIF_PACKED_AD },
        .bitbltbuf     = { .DBP = 0x3eef, .DBW = 1 },
        .bitbltbufaddr = SCE_GS_BITBLTBUF,
        .trxpos        = { },
        .trxposaddr    = SCE_GS_TRXPOS,
        .trxreg        = { .RRW = 0x40, .pad12 = 0xdbeef },
        .trxregaddr    = SCE_GS_TRXREG,
        .trxdir        = { .XDR = 0 },          // host -> local
        .trxdiraddr    = SCE_GS_TRXDIR,
        // the IMAGE payload tag; NLOOP is the texel quadword count
        .giftag1       = { .NLOOP = 0x3eef, .EOP = 1, .FLG = SCE_GIF_IMAGE },
    },
};

// The header of a uiPageSize-page image at GS block sVRAMAddress: a
// 64-texel-wide PSMCT32 transfer of uiPageSize * 32 lines, 0x200 quadwords a
// page. FF3 0x001e6698.
static void _MakeTRI2FileHeader(SGDTRI2FILEHEADER *pTRI2Head, short sVRAMAddress, u_int uiPageSize)
{
    *pTRI2Head = s_TRI2FileHeaderDefault;

    pTRI2Head->uiVif1Code_DIRECT  = SCE_VIF1_SET_DIRECT((uiPageSize << 9) + 6, 0);
    pTRI2Head->gsli.bitbltbuf.DBP = sVRAMAddress;
    pTRI2Head->gsli.trxreg.RRH    = uiPageSize << 5;
    pTRI2Head->gsli.giftag1.NLOOP = uiPageSize << 9;
}

// Read uiPageSize pages of VRAM at sVRAMAddress into the body behind
// pTRI2Head, then write the header that sends them back. FF3 0x001e6778.
static void _MakeTRI2FileByVRAMImage(SGDTRI2FILEHEADER *pTRI2Head, short sVRAMAddress, u_int uiPageSize)
{
    sceGsStoreImage spi;

    sceGsSetDefStoreImage(&spi, sVRAMAddress, 1, 0, 0, 0, 0x40, (short)((int)(uiPageSize << 0x15) >> 0x10));
    FlushCache(0);
    sceGsExecStoreImage(&spi, (u_long128 *)(pTRI2Head + 1));
    sceGsSyncPath(0, 0);

    _MakeTRI2FileHeader(pTRI2Head, sVRAMAddress, uiPageSize);
}

// Widen the span in pSDPrev by pTRI2Head's descriptors and store it in pSD:
// the lowest DBP, and for the highest DBP its size and end (DBP + size in
// blocks; PSMT8 / PSMT4 images with an odd DBW count double). FF3 0x001e6810.
void gra3dGetTRI2SizeData(TRI2SIZEDATA *pSD, TRI2SIZEDATA *pSDPrev, SGDTRI2FILEHEADER *pTRI2Head)
{
    TRI2SIZEDATA    SDWork = *pSDPrev;
    sceGsLoadImage *pLI    = &pTRI2Head->gsli;

    // The DIRECT qwc bounds the descriptor list (less the leading 8 quadwords:
    // header and first register block). Unsigned, as FF3: below 8 it wraps.
    u_int uiNumDescriptor = pTRI2Head->GetTRI2Size() - 8;

    if (uiNumDescriptor > 1)
    {
        for (;;)
        {
            u_int uiQwc = (u_int)pLI->giftag1.NLOOP;
            u_int uiDBP = (u_int)pLI->bitbltbuf.DBP;

            if (uiDBP < SDWork.uiMinAddress)
            {
                SDWork.uiMinAddress = uiDBP;
            }

            if (SDWork.uiMaxTbp < uiDBP)
            {
                SDWork.uiMaxTbp = uiDBP;

                switch (pLI->bitbltbuf.DPSM)
                {
                    case SCE_GS_PSMT8:
                    case SCE_GS_PSMT4:
                        if (pLI->bitbltbuf.DBW & 1)
                        {
                            SDWork.uiVRAMTexSize = uiQwc << 1;
                            break;
                        }
                        SDWork.uiVRAMTexSize = uiQwc;
                        break;

                    default:
                        SDWork.uiVRAMTexSize = uiQwc;
                        break;
                }

                SDWork.uiMaxAddress = (u_int)pLI->bitbltbuf.DBP + (SDWork.uiVRAMTexSize >> 4);
            }

            // past this descriptor's register block and texels
            pLI = (sceGsLoadImage *)((u_char *)pLI + uiQwc * 0x10 + 0x60);

            if (uiNumDescriptor <= (u_int)((u_char *)pLI - (u_char *)pTRI2Head) >> 4)
            {
                break;
            }
        }
    }

    *pSD = SDWork;
}

// PC: every descriptor of one header's DIRECT packet, as the EE's VIF1 DMA
// would have sent them. sceGsExecLoadImage sends only the one it is given,
// and models pack an indexed image and its CLUT as two descriptors in one
// header. Same walk as gra3dGetTRI2SizeData, guarded against the wrap below 8.
static void _HostForEachLoadImage(SGDTRI2FILEHEADER *pTRI2Head, void (*pfnImage)(sceGsLoadImage *, void *),
                                  void *pUser)
{
    u_int           uiNumDescriptor = pTRI2Head->GetTRI2Size();
    sceGsLoadImage *pLI             = &pTRI2Head->gsli;

    if (uiNumDescriptor < 8)
    {
        return;
    }
    uiNumDescriptor -= 8;

    for (;;)
    {
        u_int uiQwc = (u_int)pLI->giftag1.NLOOP;

        pfnImage(pLI, pUser);

        pLI = (sceGsLoadImage *)((u_char *)pLI + uiQwc * 0x10 + 0x60);

        if (uiNumDescriptor <= (u_int)((u_char *)pLI - (u_char *)pTRI2Head) >> 4)
        {
            break;
        }
    }
}

// PC: upload one descriptor to the emulated GS.
static void _HostExecLoadImage(sceGsLoadImage *pLI, void *pUser)
{
    (void)pUser;
    sceGsExecLoadImage(pLI, (u_long128 *)(pLI + 1));
}

// PC: upload every descriptor of one header.
static void _HostExecLoadImageChain(SGDTRI2FILEHEADER *pTRI2Head)
{
    _HostForEachLoadImage(pTRI2Head, _HostExecLoadImage, nullptr);
}

// PC (MioPan gra3dTRI2.c, host only): the same walk over a whole TRI2 unit
// (iNumTexture headers, stepped as gra3dLoadTRI2FileToVRAM steps them), for
// the host renderer bridge: it learns what a model's TRI2s write, and sends
// one it skipped when a later draw samples what that send would have left.
void gra3dHostForEachTRI2Image(int iNumTexture, SGDTRI2FILEHEADER *pTRI2HeadTop,
                               void (*pfnImage)(sceGsLoadImage *, void *), void *pUser)
{
    SGDTRI2FILEHEADER *pTRI2HeadWork = pTRI2HeadTop;

    for (int i = 0; i < iNumTexture; i++)
    {
        u_int uiTRI2Size = pTRI2HeadWork->GetTRI2Size();

        _HostForEachLoadImage(pTRI2HeadWork, pfnImage, pUser);
        pTRI2HeadWork = (SGDTRI2FILEHEADER *)((u_char *)pTRI2HeadWork + (uiTRI2Size + 1) * 0x10);
    }
}

void gra3dHostLoadTRI2(int iNumTexture, SGDTRI2FILEHEADER *pTRI2HeadTop)
{
    gra3dHostForEachTRI2Image(iNumTexture, pTRI2HeadTop, _HostExecLoadImage, nullptr);
}

// Send iNumTexture headers from pTRI2HeadTop to VRAM: appended to the VIF1
// DMA chain, or on the GIF channel through sceGsExecLoadImage (which sends
// only each header's first descriptor; nothing passes SCE_DMA_GIF).
// FF3 0x001e6990.
void gra3dLoadTRI2FileToVRAM(int iNumTexture, SGDTRI2FILEHEADER *pTRI2HeadTop, int iDmaChan)
{
    // PC: a preset model whose textures the host has resolved no longer needs
    // its texels in emulated GS memory (its draws are handed the textures), so
    // only the host mirror is skipped; the EE side runs unchanged.
    const int bHostBound = ReiPan_Graph3dTextureUploadBound(pTRI2HeadTop);
    SGDTRI2FILEHEADER *pTRI2HeadWork = pTRI2HeadTop;

    for (int i = 0; i < iNumTexture; i++)
    {
        u_int uiTRI2Size = pTRI2HeadWork->GetTRI2Size();

        if (iDmaChan == SCE_DMA_VIF1)
        {
            g3dDmaAddPacket(pTRI2HeadWork, uiTRI2Size + 1);
            if (bHostBound == 0)
            {
                _HostExecLoadImageChain(pTRI2HeadWork); // PC: the chain is not walked
            }
        }
        else
        {
            FlushCache(0);
            sceGsExecLoadImage(&pTRI2HeadWork->gsli, (u_long128 *)(pTRI2HeadWork + 1));
            sceGsSyncPath(0, 0);
        }

        pTRI2HeadWork = (SGDTRI2FILEHEADER *)((u_char *)pTRI2HeadWork + (uiTRI2Size + 1) * 0x10);
    }

    // PC: these texels are in GS memory now, which is when the model's first
    // draw may resolve them.
    if (bHostBound == 0 && iDmaChan == SCE_DMA_VIF1)
    {
        ReiPan_Graph3dNoteTextureUpload(pTRI2HeadTop);
    }
}

// Read pSD's span (uiPageSize pages from uiMinAddress) back from VRAM as
// headers at pTRI2HeadTop, at most 0x3f pages each; returns how many it
// wrote. FF3 0x001e6a30.
int gra3dGenerateTRI2FileFromVRAM(SGDTRI2FILEHEADER *pTRI2HeadTop, TRI2SIZEDATA *pSD)
{
    SGDTRI2FILEHEADER *pTRI2HeadWork  = pTRI2HeadTop;
    int                iNewNumTexture = 0;
    int                iVRAMAddress   = (int)pSD->uiMinAddress;
    int                iPageRest      = (int)pSD->uiPageSize;

    while (iPageRest > 0)
    {
        int iPages = iPageRest < TRI2_MAX_PAGES ? iPageRest : TRI2_MAX_PAGES;

        iNewNumTexture++;
        _MakeTRI2FileByVRAMImage(pTRI2HeadWork, (short)iVRAMAddress, iPages);

        iPageRest     -= iPages;
        pTRI2HeadWork  = (SGDTRI2FILEHEADER *)((u_char *)pTRI2HeadWork + iPages * 0x2000 + 0x70);
        iVRAMAddress  += iPages * 0x20;
    }

    return iNewNumTexture;
}
