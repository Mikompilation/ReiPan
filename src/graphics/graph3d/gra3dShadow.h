/* ==========================================================================
 *  graphics/graph3d/gra3dShadow.h
 *
 *  Projected ("map") shadows (FF2 gra3dShadow.c), FF3 0x1e4760..0x1e6698 plus
 *  the linkonce helpers 0x2bcbc0..0x2bcc90. A caster's bounding box and a
 *  light give a projection direction; the caster is rendered into a 256x256
 *  CRenderTarget seen from the light (_RenderShadow), then that texture is
 *  projected over every model on the project-model list (_DrawShadow, the
 *  SRT_MAPSHADOW pass of _gra3dDrawSGD -> AssignShadowPrim, VU1 program
 *  0x2e6ac0). On PC the VU1 projection has no interpreter: the caster and
 *  receiver draws are bracketed for the host renderer's shadow map
 *  (reipan_renderer.h "Projected shadows").
 *
 *  FF3 dropped FF2's debug switches (g_gra3dShadowDebug), the assign group,
 *  the source model and DispShadowSprite, and keeps each project model's
 *  coordinates in a parallel list.
 * ======================================================================== */
#ifndef _GRAPHICS_GRAPH3D_GRA3DSHADOW_H
#define _GRAPHICS_GRAPH3D_GRA3DSHADOW_H

#include <stddef.h>                     // offsetof
#include <libvu0.h>                     // sceVu0FVECTOR, sceVu0FMATRIX, sceVu0IVECTOR

#include "sdk/scetypes.h"
#include "graphics/graph3d/g3dCore.h"   // G3DVU1FOG

struct SGDPROCUNITHEADER;
struct SGDFILEHEADER;                   // graphics/graph3d/sgd_types.h
struct SGDCOORDINATE;
struct G3DLIGHT;                        // graphics/graph3d/g3dLight.h

// The projection pass's VU1 data (FF2 GRA3DVU1MEMLAYOUT_MAPSHADOW_DIRECT;
// same layout in FF3). AssignShadowPrim fills the three matrices per
// receiver block, _CalcColor the colour, SetUpShadow the fog.
struct GRA3DVU1MEMLAYOUT_MAPSHADOW      /* 0x190 */
{
    /* 0x000 */ sceVu0FVECTOR _vf01;
    /* 0x010 */ sceVu0FVECTOR _vf02;
    /* 0x020 */ u_int         auiDBAddress[4];  // VU1 double-buffer addresses {0x60, 0x230}
    /* 0x030 */ sceVu0FVECTOR vOffsetData;
    /* 0x040 */ qword         gtTRISTRIP;       // GIF tags of the projected strips / fans
    /* 0x050 */ qword         gtTRIFAN;
    /* 0x060 */ qword         aqPad0[2];
    /* 0x080 */ sceVu0FMATRIX matLocalScreen;   // camera matWorldScreen * local->world, z + 400
    /* 0x0c0 */ sceVu0FMATRIX matLocalClip;     // camera matWorldClipPolygon * local->world
    /* 0x100 */ qword         aqPad1[3];
    /* 0x130 */ sceVu0FMATRIX matLIP;           // s_matIP * local->world, w column = -light dir (local)
    /* 0x170 */ sceVu0IVECTOR ivColor;          // projector colour 0..255; alpha 0 = nothing to draw
    /* 0x180 */ G3DVU1FOG     Fog;
};
static_assert(sizeof(GRA3DVU1MEMLAYOUT_MAPSHADOW) == 0x190, "GRA3DVU1MEMLAYOUT_MAPSHADOW is 0x190 bytes");
static_assert(offsetof(GRA3DVU1MEMLAYOUT_MAPSHADOW, auiDBAddress) == 0x020, "GRA3DVU1MEMLAYOUT_MAPSHADOW.auiDBAddress");
static_assert(offsetof(GRA3DVU1MEMLAYOUT_MAPSHADOW, vOffsetData) == 0x030, "GRA3DVU1MEMLAYOUT_MAPSHADOW.vOffsetData");
static_assert(offsetof(GRA3DVU1MEMLAYOUT_MAPSHADOW, gtTRISTRIP) == 0x040, "GRA3DVU1MEMLAYOUT_MAPSHADOW.gtTRISTRIP");
static_assert(offsetof(GRA3DVU1MEMLAYOUT_MAPSHADOW, gtTRIFAN) == 0x050, "GRA3DVU1MEMLAYOUT_MAPSHADOW.gtTRIFAN");
static_assert(offsetof(GRA3DVU1MEMLAYOUT_MAPSHADOW, matLocalScreen) == 0x080, "GRA3DVU1MEMLAYOUT_MAPSHADOW.matLocalScreen");
static_assert(offsetof(GRA3DVU1MEMLAYOUT_MAPSHADOW, matLocalClip) == 0x0c0, "GRA3DVU1MEMLAYOUT_MAPSHADOW.matLocalClip");
static_assert(offsetof(GRA3DVU1MEMLAYOUT_MAPSHADOW, matLIP) == 0x130, "GRA3DVU1MEMLAYOUT_MAPSHADOW.matLIP");
static_assert(offsetof(GRA3DVU1MEMLAYOUT_MAPSHADOW, ivColor) == 0x170, "GRA3DVU1MEMLAYOUT_MAPSHADOW.ivColor");
static_assert(offsetof(GRA3DVU1MEMLAYOUT_MAPSHADOW, Fog) == 0x180, "GRA3DVU1MEMLAYOUT_MAPSHADOW.Fog");

// The shadow pass's VU1 memory image as one DMA packet (VIF code + data,
// 0x1a quadwords; SetVU1HeaderShadow copies it). gra3d's _ModifyScratchpad
// keeps it at scratchpad 0x70000440 or in a static copy (memcpy 0x1a0).
struct GRA3DSCRATCHPADLAYOUT_MAPSHADOW  /* 0x1a0 */
{
    /* 0x000 */ qword                       qwVif1Code;   // UNPACK of the 0x19 quadwords
    /* 0x010 */ GRA3DVU1MEMLAYOUT_MAPSHADOW Vu1Mem;
};
static_assert(sizeof(GRA3DSCRATCHPADLAYOUT_MAPSHADOW) == 0x1a0, "GRA3DSCRATCHPADLAYOUT_MAPSHADOW is 0x1a0 bytes");
static_assert(offsetof(GRA3DSCRATCHPADLAYOUT_MAPSHADOW, Vu1Mem) == 0x010, "GRA3DSCRATCHPADLAYOUT_MAPSHADOW.Vu1Mem");

// gra3dshadowInit's argument. layout: host -- a pointer (EE 0x4).
struct GRA3DSHADOWCREATIONDATA          /* EE 0x4 */
{
    GRA3DSCRATCHPADLAYOUT_MAPSHADOW *pSL;
};

// Copy the default VU1 image into pCD->pSL, size the 256x256 shadow texture
// (TBP 0x3000, CBP 0x3000) and empty both lists. FF3 0x001e5e88
// (gra3d's _ModifyScratchpad).
void gra3dshadowInit(GRA3DSHADOWCREATIONDATA *pCD);

// pSGDTop once onto the project-model list (the models shadows fall on, 50
// entries at 0x3eeb20), pCoord onto the parallel list 0x3eebf0 (FF2 took the
// SGD only). FF3 0x001e60a0 (ModelObject::ReqDraw).
void gra3dshadowAddProjectModel(SGDFILEHEADER *pSGDTop, SGDCOORDINATE *pCoord);

// Empty both lists; once a frame before the rooms draw (MapDrawAll).
// FF3 0x001e6198
void gra3dshadowClearProjectModel(void);

// The caster's bounding box (8 corners) and the matrix that takes it to the
// world; recomputes the target, direction and colour. FF3 0x001e63d0.
void gra3dshadowSetBoundingBox(float (*avBB)[4], float (*mat)[4]);
// The light the shadow is cast with (copied, then _CalcDirection /
// _CalcColor). FF3 0x001e6418.
void gra3dshadowSetLight(G3DLIGHT *pLight);
// Render pSGDTop's block iBlock (-1: all) into the shadow texture and project
// it over the project models when the colour's alpha is not 0. FF3 0x001e6480.
void gra3dshadowDrawSGD(SGDFILEHEADER *pSGDTop, SGDCOORDINATE *pCoord, int iBlock);

// Queue one block's meshes for the projection pass (_gra3dDrawSGD
// SRT_MAPSHADOW, blocks in the view volume). FF3 0x001e4e48.
void AssignShadowPrim(SGDPROCUNITHEADER *pPUHead);

#endif /* _GRAPHICS_GRAPH3D_GRA3DSHADOW_H */
