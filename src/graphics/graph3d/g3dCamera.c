/* ==========================================================================
 *  graphics/graph3d/g3dCamera.c
 *
 *  Projection matrix builders of the g3d core: view -> screen (GS pixel
 *  coordinates and Z) and view -> clip, each for the perspective and the
 *  orthographic camera, plus the "GS primitive" matrix that maps NDC into the
 *  GS rasteriser's space (aspect scale, screen centre).  gra3dApplyCamera picks
 *  them by GRA3DCAMERA.type from two-entry tables (sdata 0x370bd0 / 0x370bd8).
 *  Matrices are row-vector 4x4 (sceVu0FMATRIX).
 *
 *  Ported from MioPan's g3dCamera.c (FF2 0x19b0a8..0x19b514); FF3's six
 *  functions (0x1cb460..0x1cb88c) are FF2's, constant for constant
 *  (sdata 0x370a10..0x370a20: 1, 1, -2, -2, 0.5).
 * ======================================================================== */

#include "graphics/graph3d/g3dCamera.h"

#include <math.h>                               // tanf

#include "sdk/libvu0.h"                         // sceVu0UnitMatrix, sceVu0MulMatrix

// Diagonal scale + translation: NDC xyz -> GS screen coordinates.
// FF3 0x001cb460; from MioPan g3dCamera.c.
void g3dCalcScreenGSPrimitiveMatrix(float (*mat)[4], float fAspectX, float fAspectY, float fAspectZ,
                                    float fCenterX, float fCenterY, float fCenterZ)
{
    sceVu0UnitMatrix(mat);

    mat[3][2] = fCenterZ;
    mat[0][0] = fAspectX;
    mat[1][1] = fAspectY;
    mat[2][2] = fAspectZ;
    mat[3][0] = fCenterX;
    mat[3][1] = fCenterY;
}

// Perspective view -> screen: the divide by view z (w = z) followed by the GS
// primitive matrix; [fZmin, fZmax] is spread across [fNearZ, fFarZ].
// FF3 0x001cb4e8; from MioPan g3dCamera.c.
void g3dCalcViewScreenMatrixPerspective(float (*mat)[4], float fScrZ, float fAspectX, float fAspectY,
                                        float fCenterX, float fCenterY, float fZmin, float fZmax,
                                        float fNearZ, float fFarZ)
{
    float fCenterZ;
    float fAspectZ;
    sceVu0FMATRIX matScreenGSPrimitive;

    fAspectZ = (fFarZ * fNearZ * (fZmax - fZmin)) / (fFarZ - fNearZ);
    fCenterZ = (fZmin * fFarZ + -fZmax * fNearZ) / (fFarZ - fNearZ);

    sceVu0UnitMatrix(mat);

    mat[1][1] = fScrZ;
    mat[0][0] = fScrZ;
    mat[2][3] = 1.0f;                                       // sdata 0x370a10
    mat[3][2] = 1.0f;
    mat[2][2] = 0.0f;
    mat[3][3] = 0.0f;

    g3dCalcScreenGSPrimitiveMatrix(matScreenGSPrimitive, fAspectX, fAspectY, fAspectZ,
                                   fCenterX, fCenterY, fCenterZ);

    sceVu0MulMatrix(mat, matScreenGSPrimitive, mat);
}

// Orthographic view -> screen: no divide, just the GS primitive matrix (the
// ROM multiplies it through the unit matrix at data 0x3293c0 to land it in mat).
// fScrZ is unused.
// FF3 0x001cb618; from MioPan g3dCamera.c.
void g3dCalcViewScreenMatrixOrtho(float (*mat)[4], float fScrZ, float fAspectX, float fAspectY,
                                  float fCenterX, float fCenterY, float fZmin, float fZmax,
                                  float fNearZ, float fFarZ)
{
    float fAspectZ;
    float fCenterZ;
    sceVu0FMATRIX matScreenGSPrimitive;
    sceVu0FMATRIX matUnit;

    (void)fScrZ;

    fAspectZ = (fFarZ * fNearZ * (fZmax - fZmin)) / (fFarZ - fNearZ);
    fCenterZ = (fZmin * fFarZ + -fZmax * fNearZ) / (fFarZ - fNearZ);

    g3dCalcScreenGSPrimitiveMatrix(matScreenGSPrimitive, fAspectX, fAspectY, fAspectZ,
                                   fCenterX, fCenterY, fCenterZ);

    sceVu0UnitMatrix(matUnit);
    sceVu0MulMatrix(mat, matScreenGSPrimitive, matUnit);
}

// Perspective view -> clip: the view frustum into the canonical clip volume;
// fClipVolumeX/Y are the clip extents at the screen distance fScrZ.
//
// PORT DEVIATION (MioPan's, kept): mat[1][1] is negated against the ROM.  The
// GS rasterises with +Y up, the host clip space has +Y down, and the renderer
// (MioPan's) expects every clip-space matrix of the engine in host orientation,
// so the flip lives here and not in a bridge.  The bounding-box cull reads clip
// Y (gra3dIsBBInViewvolume), but the flip only swaps its "above top" and "below
// bottom" bits for every corner alike, which neither the AND cull nor the OR
// edge test can see.  matLocalClip (effect_obj.c) only reaches VU1 microcode.
// FF3 0x001cb700; from MioPan g3dCamera.c.
void g3dCalcViewClipMatrixPerspective(float (*mat)[4], float fScrZ, float fAspectX, float fAspectY,
                                      float fNearZ, float fFarZ, float fClipVolumeX, float fClipVolumeY)
{
    float fGSX;
    float fGSY;
    float fRScrZ;

    fRScrZ = fNearZ / fScrZ;
    fGSX = fClipVolumeX * fRScrZ;
    fGSY = fClipVolumeY * fRScrZ;

    sceVu0UnitMatrix(mat);

    mat[2][3] = 1.0f;                                       // sdata 0x370a14
    mat[3][3] = 0.0f;
    mat[1][1] = -(((fNearZ + fNearZ) * fAspectY) / (fGSY + fGSY));     // PC: negated, see above
    mat[0][0] = ((fNearZ + fNearZ) * fAspectX) / (fGSX + fGSX);
    mat[3][2] = (fFarZ * fNearZ * -2.0f) / (fFarZ - fNearZ);           // sdata 0x370a18
    mat[2][2] = (fFarZ + fNearZ) / (fFarZ - fNearZ);
}

// Orthographic view -> clip.  fScrZ is unused.
//
// PORT DEVIATION (MioPan's, kept): Y negated as in the perspective builder, and
// both Z terms too -- the ROM maps near to +1 and far to -1 here (mat[2][2] =
// -2 / (far - near), mat[3][2] = (far + near) / (far - near)), the other way
// round from its own perspective matrix, and the host depth buffer wants near
// at -1.  No FF3 camera seen so far selects the orthographic type.
// FF3 0x001cb7c8; from MioPan g3dCamera.c.
void g3dCalcViewClipMatrixOrtho(float (*mat)[4], float fScrZ, float fAspectX, float fAspectY,
                                float fNearZ, float fFarZ, float fClipVolumeX, float fClipVolumeY)
{
    (void)fScrZ;

    sceVu0UnitMatrix(mat);

    mat[0][0] = fAspectX / fClipVolumeX;
    mat[1][1] = -(fAspectY / fClipVolumeY);                             // PC: negated
    mat[3][2] = -((fFarZ + fNearZ) / (fFarZ - fNearZ));                 // PC: negated
    mat[2][2] = 2.0f / (fFarZ - fNearZ);                                // PC: ROM -2.0 (sdata 0x370a1c)
}

// Distance from the eye at which a half-extent fClipValueAbs fills the field of
// view fFov: fClipValueAbs / tan(fFov / 2).
// FF3 0x001cb848; from MioPan g3dCamera.c.
float g3dCalcDistanceToScreen(float fFov, float fClipValueAbs)
{
    return fClipValueAbs / tanf(fFov * 0.5f);               // sdata 0x370a20
}
