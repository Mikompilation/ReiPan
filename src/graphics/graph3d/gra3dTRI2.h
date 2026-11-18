/* ==========================================================================
 *  graphics/graph3d/gra3dTRI2.h
 *
 *  TRI2 texture files embedded in SGD models (FF2 gra3dTRI2.c).
 *  The two static helpers that build headers live in the .c.
 * ======================================================================== */
#ifndef _GRAPHICS_GRAPH3D_GRA3DTRI2_H
#define _GRAPHICS_GRAPH3D_GRA3DTRI2_H

struct SGDTRI2FILEHEADER;
struct TRI2SIZEDATA;
struct sceGsLoadImage;

// Send iNumTexture TRI2 images starting at pTRI2HeadTop to GS memory.
// FF3 0x001e6990.
void gra3dLoadTRI2FileToVRAM(int iNumTexture, SGDTRI2FILEHEADER *pTRI2HeadTop, int iDmaChan);
// Widen pSDPrev's GS address span by pTRI2Head's image into pSD.
// FF3 0x001e6810.
void gra3dGetTRI2SizeData(TRI2SIZEDATA *pSD, TRI2SIZEDATA *pSDPrev, SGDTRI2FILEHEADER *pTRI2Head);
// Read the span pSD describes back from GS memory as TRI2 files at
// pTRI2HeadTop; returns how many it wrote. FF3 0x001e6a30.
int  gra3dGenerateTRI2FileFromVRAM(SGDTRI2FILEHEADER *pTRI2HeadTop, TRI2SIZEDATA *pSD);

// PC (host only, MioPan gra3dTRI2.c): call pfnImage on every GS load-image
// descriptor of iNumTexture headers from pTRI2HeadTop, and upload them to the
// emulated GS (reipan_graph3d.cpp's resolved textures).
void gra3dHostForEachTRI2Image(int iNumTexture, SGDTRI2FILEHEADER *pTRI2HeadTop,
                               void (*pfnImage)(sceGsLoadImage *, void *), void *pUser);
void gra3dHostLoadTRI2(int iNumTexture, SGDTRI2FILEHEADER *pTRI2HeadTop);

#endif /* _GRAPHICS_GRAPH3D_GRA3DTRI2_H */
