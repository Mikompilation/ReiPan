/* ==========================================================================
 *  graphics/graph3d/g3dCamera.h
 *
 *  Projection matrix builders of the g3d core (FF2 g3dCamera.c). gra3d picks
 *  them by GRA3DCAMERA.type from two-entry tables (sdata 0x370bd0 / 0x370bd8).
 * ======================================================================== */
#ifndef _GRAPHICS_GRAPH3D_G3DCAMERA_H
#define _GRAPHICS_GRAPH3D_G3DCAMERA_H

// view -> screen (GS pixel coordinates and Z)
typedef void (*LPFUNC_VIEWSCREENMATRIX)(float (*mat)[4], float fScrZ,
                                        float fAspectX, float fAspectY,
                                        float fCenterX, float fCenterY,
                                        float fZmin, float fZmax,
                                        float fNearZ, float fFarZ);
// view -> clip (the clip volume fClipVolumeX x fClipVolumeY at the screen)
typedef void (*LPFUNC_VIEWCLIPMATRIX)(float (*mat)[4], float fScrZ,
                                      float fAspectX, float fAspectY,
                                      float fNearZ, float fFarZ,
                                      float fClipVolumeX, float fClipVolumeY);

void g3dCalcScreenGSPrimitiveMatrix(float (*mat)[4], float fAspectX, float fAspectY, float fAspectZ,
                                    float fCenterX, float fCenterY, float fCenterZ);
void g3dCalcViewScreenMatrixPerspective(float (*mat)[4], float fScrZ, float fAspectX, float fAspectY,
                                        float fCenterX, float fCenterY, float fZmin, float fZmax,
                                        float fNearZ, float fFarZ);
void g3dCalcViewScreenMatrixOrtho(float (*mat)[4], float fScrZ, float fAspectX, float fAspectY,
                                  float fCenterX, float fCenterY, float fZmin, float fZmax,
                                  float fNearZ, float fFarZ);
void g3dCalcViewClipMatrixPerspective(float (*mat)[4], float fScrZ, float fAspectX, float fAspectY,
                                      float fNearZ, float fFarZ, float fClipVolumeX, float fClipVolumeY);
void g3dCalcViewClipMatrixOrtho(float (*mat)[4], float fScrZ, float fAspectX, float fAspectY,
                                float fNearZ, float fFarZ, float fClipVolumeX, float fClipVolumeY);
float g3dCalcDistanceToScreen(float fFov, float fClipValueAbs);

#endif /* _GRAPHICS_GRAPH3D_G3DCAMERA_H */
