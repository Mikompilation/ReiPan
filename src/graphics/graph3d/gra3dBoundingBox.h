/* ==========================================================================
 *  graphics/graph3d/gra3dBoundingBox.h
 *
 *  Bounding-box helpers of the gra3d layer (FF2 gra3dBoundingBox.c, FF3
 *  0x1dd8b0..0x1ddec0). A box is its eight homogeneous corners float[8][4]
 *  in the order gra3dbbApplyFromBounds builds them:
 *
 *      [0] (min.x, min.y, min.z)   [4] (min.x, min.y, max.z)
 *      [1] (max.x, min.y, min.z)   [5] (max.x, min.y, max.z)
 *      [2] (min.x, max.y, min.z)   [6] (min.x, max.y, max.z)
 *      [3] (max.x, max.y, min.z)   [7] (max.x, max.y, max.z)
 * ======================================================================== */
#ifndef _GRAPHICS_GRAPH3D_GRA3DBOUNDINGBOX_H
#define _GRAPHICS_GRAPH3D_GRA3DBOUNDINGBOX_H

#include "graphics/graph3d/gra3d.h"             // G3DFOG

// The 8 corners of avSrc through mat into avDest. FF3 0x001dd8b0.
void gra3dbbApplyMatrix(float (*avDest)[4], float (*avSrc)[4], float (*mat)[4]);

// Non-zero when a corner of the world box lies inside the fog ramp
// (g3dCalcFA * w + g3dCalcFB > 0 after matWorldScreen); 0 when fog is off
// (fNear <= 0). FF3 0x001dd938.
int gra3dbbIsInFogArea(float (*matWorldScreen)[4], G3DFOG *pFog, float (*avBBWorld)[4]);

// Non-zero unless one clip plane rejects all 8 corners of avBBWorld through
// matClip; the corners through matTransform go to avBBTransformed (skipped
// when matTransform is null). PC: matClip / matTransform were VU0 vf4..vf7 /
// vf8..vf11, loaded by the caller. FF3 0x001dda10.
int gra3dbbIsInViewvolume(float (*avBBTransformed)[4], float (*avBBWorld)[4],
                          float (*matClip)[4], float (*matTransform)[4]);

// Centre of the box's lower face (by Y), w = 1. FF3 0x001ddac8.
void gra3dbbCalcCenterBase(float *vCB, float (*avBB)[4]);

// XZ distance from the lower-face centre to corner [0]. FF3 0x001ddb38.
float gra3dbbCalcRadiusXZ(float (*avBB)[4]);

// Half-extents on each axis, w = 0. FF3 0x001ddb90.
void gra3dbbCalcInnerEllipse(float *vEllipse, float (*avBB)[4]);

// The 8 corners of (vMin, vMax) through mat into avBBDest. FF3 0x001ddc58.
void gra3dbbApplyFromBounds(float (*avBBDest)[4], float *vMin, float *vMax, float (*mat)[4]);

#endif /* _GRAPHICS_GRAPH3D_GRA3DBOUNDINGBOX_H */
