/* ==========================================================================
 *  graphics/graph3d/gra3dMirror.h
 *
 *  FF3-only (no FF2 counterpart): the planar mirror and a character light
 *  blend, FF3 0x1de0d8..0x1dfb90 (between gra3dDma.c and gra3dMisc.c).
 *
 *  gra3dDrawModelCb draws a room model's mirror: the model's block named by
 *  its SGDPU_MIRROR_BLOCK unit is walked to its first mesh, whose triangles
 *  are clipped and projected on VU0 to get the mirror's screen rectangle;
 *  when it is visible and faces the camera, the room is drawn again through
 *  the camera reflected in the mesh plane, scissored to that rectangle, and
 *  the rectangle's Z is reset before the normal draw.
 * ======================================================================== */
#ifndef _GRAPHICS_GRAPH3D_GRA3DMIRROR_H
#define _GRAPHICS_GRAPH3D_GRA3DMIRROR_H

#include "sdk/scetypes.h"

struct G3DLIGHT;                        // graphics/graph3d/g3dLight.h
struct GRA3DCAMERA;                     // graphics/graph3d/gra3d.h
struct SGDFILEHEADER;                   // graphics/graph3d/sgd_types.h
struct SGDPROCUNITHEADER;

// Load the three VU0 matrices gra3dCalcMeshScreenRect's helpers use:
// local->view (vf4..7), local->clip (vf25..28), local->screen (vf8..11).
// PC: kept in this file's statics. FF3 0x001de0d8.
void Vu0LoadMatrices_4_25_8(float (*matLocalView)[4], float (*matLocalClip)[4], float (*matLocalScreen)[4]);

// The screen rectangle of a preset mesh (SgCalcScreenRectPrim, after
// Vu0LoadMatrices_4_25_8): 1 when every triangle faces the camera and some of
// the mesh is on screen; its first five vertices are kept for the mirror.
// FF3 0x001df2d8.
int  gra3dCalcMeshScreenRect(SGDPROCUNITHEADER *pPUHead);

// Draw pSgdTop's mirror with pfnDraw (the scene to reflect) from pCam.
// uiCoord: EE address of the coordinate base, 0 for the model's own.
// FF3 0x001df568 (IngameDrawRoomMorph).
void gra3dDrawModelCb(GRA3DCAMERA *pCam, SGDFILEHEADER *pSgdTop, u_int uiCoord, int (*pfnDraw)(void));

// pDest = the four strongest lights at vTarget and a fixed directional
// light, blended into one directional light whose diffuse xyz is scaled by
// fScale. FF3 0x001df968 (char_model.c).
void gra3dGenerateBlendedLight(G3DLIGHT *pDest, float *vTarget, float fScale);

#endif /* _GRAPHICS_GRAPH3D_GRA3DMIRROR_H */
