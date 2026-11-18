/* ==========================================================================
 *  graphics/graph3d/gra3dSGD.h
 *
 *  SGD model drawing (FF2 gra3dSGD.c, FF3 0x1e0330..0x1e3850): the walkers
 *  that turn a model's process-unit chains into VU1 packets (realtime, preset
 *  and map-shadow paths), the prelight bake, ST scrolling, vertex-colour and
 *  material-alpha editing, and the state shared with gra3dSGDData.c /
 *  gra3dShadow.c (current model, coordinate base, coordinate cache, edge check,
 *  the skinning buffers).
 * ======================================================================== */
#ifndef _GRAPHICS_GRAPH3D_GRA3DSGD_H
#define _GRAPHICS_GRAPH3D_GRA3DSGD_H

#include <stddef.h>                         // offsetof

#include "graphics/graph3d/sgd_types.h"     // SGDFILEHEADER, SGDCOORDINATE, SGDPROCUNITHEADER, SGDRENDERTYPE

struct GRA3DCAMERA;

// gra3dsgdInit's argument: the vertex/normal work buffer gra3dInit hands over.
// layout: host -- a pointer (EE 0x8 bytes: pointer + size).
struct GRA3DSGDCREATIONDATA             /* EE 0x8 */
{
    void *vnarray;                      // EE +0x0
    u_int size;                         // EE +0x4: in quadwords (bytes >> 4)
};

// The last coordinate whose VU1 light block was sent, and the light set it
// was built for (bss 0x3ee920; FF2 CoordCache). cache_on -1 forces a resend.
struct CoordCache                       /* 0x2c */
{
    /* 0x00 */ int                     cache_on;
    /* 0x04 */ int                     edge_check;
    /* 0x08 */ int                     cn0;     // coordinate index
    /* 0x0c */ GRA3DMATERIALINDEXCACHE Point;
    /* 0x1c */ GRA3DMATERIALINDEXCACHE Spot;
};
static_assert(sizeof(CoordCache) == 0x2c, "CoordCache is 0x2c bytes");
static_assert(offsetof(CoordCache, edge_check) == 0x04, "CoordCache.edge_check");
static_assert(offsetof(CoordCache, cn0) == 0x08, "CoordCache.cn0");
static_assert(offsetof(CoordCache, Point) == 0x0c, "CoordCache.Point");
static_assert(offsetof(CoordCache, Spot) == 0x1c, "CoordCache.Spot");

// ---- lifecycle / current model -------------------------------------------
void           gra3dsgdInit(GRA3DSGDCREATIONDATA *pCD);             // FF3 0x001e2418
void           gra3dsgdSetupVu1(void);                              // FF3 0x001e1cd8
void           gra3dsgdSetData(SGDFILEHEADER *pSGDTop);             // FF3 0x001e2440
SGDFILEHEADER *gra3dsgdGetData(void);                               // PC: the current model (FF2's; not in FF3)
int            gra3dsgdGetNumBlock(void);                           // FF3 0x001e2360

// ---- coordinates -----------------------------------------------------------
SGDCOORDINATE *gra3dsgdGetCoordinate(int iIndex);                   // FF3 0x001e1b60
// iIndex -1: make pCU the coordinate base; otherwise copy *pCU into slot iIndex.
void           gra3dsgdSetCoordinate(SGDCOORDINATE *pCU, int iIndex); // FF3 0x001e1b78

// ---- drawing ---------------------------------------------------------------
// Draw (or prelight / clear / shadow-assign) a model: pCoord null uses the
// file's own coordinates; pnum -1 all blocks, 0 the texture block only, n one.
void _gra3dDrawSGD(SGDFILEHEADER *pSGDTop, SGDRENDERTYPE type, SGDCOORDINATE *pCoord, int pnum); // FF3 0x001e2520
void gra3dsgdDrawPresetDataObject(SGDPROCUNITHEADER *pPUHead);      // FF3 0x001e21c8
void SgSortPreProcess(SGDPROCUNITHEADER *pPUHead);                  // FF3 0x001e2478
int  BoundingBoxCalcP(SGDPROCUNITHEADER *pPUHead);                  // FF3 0x001e1d68
// gra3dDrawModelCb's walker: sets WORLDSCREEN / WORLDCLIP from the block's
// coordinate and returns the first mesh's screen-rectangle result (or the
// bounding-box test when the block has no mesh).
int  SgCalcScreenRectPrim(GRA3DCAMERA *pCam, SGDPROCUNITHEADER *pPUHead); // FF3 0x001e2fa8
// gra3dsgdDrawPresetDataObject without the cull (unreferenced in FF3).
void gra3dsgdDrawPresetDataObjectNoClip(SGDPROCUNITHEADER *pPUHead);    // FF3 0x001e3290

// ---- editing ---------------------------------------------------------------
// Scroll every textured primitive's ST by (s, t) (FF2 gra3dChangeST).
void gra3dChangeST(SGDFILEHEADER *pSgdTop, float s, float t);       // FF3 0x001e2b80
// Set one preset vertex's colour (iVertexNo >= 0) or all of them (-1).
void gra3dSetVertexColorPreset(SGDFILEHEADER *pSGDTop, int iVertexNo, float *vSetColor); // FF3 0x001e2ea0
// (float)alpha into vDiffuse.w of every material (pMaterial .. pVectorInfo).
void gra3dSetSGDMaterialAlpha(SGDFILEHEADER *pSgdTop, u_char alpha); // FF3 0x001e2f60
// The first material's vDiffuse.w as 0..255 (0x80 without materials).
int  gra3dGetSGDMaterialAlpha(SGDFILEHEADER *pSgdTop);              // FF3 0x001e2f38

// ---- TRI2 / VRAM -----------------------------------------------------------
u_int gra3dCalcVRAMPageSize(u_int uiBlockSize);                     // FF3 0x001e2120: 32-block pages, rounded up
// The last TRI2 unit sent to VRAM (MapDrawAll resets it to nullptr).
void               _SetPREVIOUSTRI2PRIM(SGDPROCUNITHEADER *p);      // FF3 0x001e2148
SGDPROCUNITHEADER *_GetPREVIOUSTRI2PRIM(void);                      // FF3 0x001e2140

// ---- state shared with gra3dSGDData.c / gra3dShadow.c ----------------------
// SgCalcScreenRectPrim's local->world of the mirror block (data 0x328a70;
// gra3dMirror.c's reflection matrix reads it).
extern sceVu0FMATRIX s_matScreenRectLocalWorld;
void               _SetVUVNPRIM(SGDPROCUNITHEADER *ppuhVUVN);       // FF3 0x001e0d98
SGDPROCUNITHEADER *_GetVUVNPRIM(void);                              // FF3 0x001e0da0
CoordCache        *_GetCoordCache(void);                            // FF3 0x001e2370
void               _SetCoordCache(CoordCache *pCC);                 // FF3 0x001e2380
float            (*_GetGlobalVertexBuffer(void))[4];                // FF3 0x001e23d8
float            (*_GetGlobalNormalBuffer(void))[4];                // FF3 0x001e23e0
int                _GetGlobalBufferSize(void);                      // FF3 0x001e23e8: quadwords per buffer
int                _GetEdgeCheck(void);                             // FF3 0x001e2468
void               _SetEdgeCheck(int ec);                           // FF3 0x001e2470
u_int             *GetNextUnpackAddr(u_int *prim);                  // FF3 0x001e0340: next VIF UNPACK code

#endif /* _GRAPHICS_GRAPH3D_GRA3DSGD_H */
