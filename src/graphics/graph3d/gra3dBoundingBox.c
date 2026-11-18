/* ==========================================================================
 *  graphics/graph3d/gra3dBoundingBox.c
 *
 *  Axis-aligned bounding-box helpers of the gra3d layer: transforms of the
 *  eight corners through the CVu0Matrix (vf4..vf7) loaders, the fog-area and
 *  view-volume tests, the lower-face centre and the half-extents.
 *
 *  Ported from MioPan's gra3dBoundingBox.c (FF2 0x1b4e90..0x1b57f8). FF3's
 *  seven functions (0x1dd8b0..0x1ddec0) are FF2's in FF2 order without the
 *  CVu0Matrix asserts; FF2's gra3dbbCalcCenter,
 *  gra3dbbCopy and gra3dbbFromBounds are not in FF3 (callers inline the
 *  copy). The VU0 macro blocks are written as scalar C.
 * ======================================================================== */

#include "graphics/graph3d/gra3dBoundingBox.h"

#include <math.h>                               // fabsf, sqrtf

#include "graphics/graph3d/g3dCore.h"           // CVu0Matrix

#define BB_HALF 0.5f                            // rdata 0x370bf0
#define BB_ONE  1.0f                            // rdata 0x370bec / 0x370bf4

// VU0 CLIP of v against +-|w|: bit 0 +x, 1 -x, 2 +y, 3 -y, 4 +z, 5 -z.
static u_int _Vu0ClipFlags(const float *v)
{
    float w = fabsf(v[3]);
    u_int f = 0;

    if (v[0] > w)  f |= 0x01;
    if (v[0] < -w) f |= 0x02;
    if (v[1] > w)  f |= 0x04;
    if (v[1] < -w) f |= 0x08;
    if (v[2] > w)  f |= 0x10;
    if (v[2] < -w) f |= 0x20;
    return f;
}

// The corners are walked back to front (7..0), as the ROM's loop with its
// one-ahead prefetch does.
// FF3 0x001dd8b0; from MioPan gra3dBoundingBox.c.
void gra3dbbApplyMatrix(float (*avDest)[4], float (*avSrc)[4], float (*mat)[4])
{
    int i;

    CVu0Matrix::s_pFuncLoadMatrix(mat);

    for (i = 7; i >= 0; i--)
    {
        CVu0Matrix::s_pFuncApplyMatrixWithoutTrans(avDest[i], avSrc[i]);
    }
}

// FF3 0x001dd938; from MioPan gra3dBoundingBox.c.
int gra3dbbIsInFogArea(float (*matWorldScreen)[4], G3DFOG *pFog, float (*avBBWorld)[4])
{
    float vWork[4];
    int   i;

    if (pFog->fNear > 0.0f)
    {
        CVu0Matrix::s_pFuncLoadMatrix(matWorldScreen);

        for (i = 7; i >= 0; i--)
        {
            CVu0Matrix::s_pFuncApplyMatrixWithoutTrans(vWork, avBBWorld[i]);

            if (g3dCalcFA(pFog) * vWork[3] + g3dCalcFB(pFog) > 0.0f)
            {
                return 1;
            }
        }
    }

    return 0;
}

// Each corner goes through two matrices at once: matClip gives a clip-space
// point whose outside mask is ANDed into the running flags (non-zero after 8
// corners: one plane rejects them all), matTransform the point stored to
// avBBTransformed[i]. Both products take w = 1 (vmaddw by vf0.w), not the
// corner's w.
// PC: the matrices were VU0 vf4..vf7 / vf8..vf11, loaded by the caller and
// not in the signature; matTransform == null skips the store (MioPan's
// deviation for callers that load only vf4..vf7).
// FF3 0x001dda10; from MioPan gra3dBoundingBox.c.
int gra3dbbIsInViewvolume(float (*avBBTransformed)[4], float (*avBBWorld)[4],
                          float (*matClip)[4], float (*matTransform)[4])
{
    u_int uClipAnd;
    int   i;
    int   c;

    uClipAnd = 0x3f;

    for (i = 7; i >= 0; i--)
    {
        const float *vBB = avBBWorld[i];
        float        vClip[4];

        for (c = 0; c < 4; c++)
        {
            vClip[c] = vBB[0] * matClip[0][c] + vBB[1] * matClip[1][c]
                     + vBB[2] * matClip[2][c] + matClip[3][c];
        }

        if (matTransform != nullptr)
        {
            float *vDest = avBBTransformed[i];

            for (c = 0; c < 4; c++)
            {
                vDest[c] = vBB[0] * matTransform[0][c] + vBB[1] * matTransform[1][c]
                         + vBB[2] * matTransform[2][c] + matTransform[3][c];
            }
        }

        uClipAnd &= _Vu0ClipFlags(vClip);
    }

    return uClipAnd == 0;
}

// Midpoint of corners 0 / 5 (when [0].y is the higher Y bound) or 2 / 7: the
// ROM blends a * 0.5 + b * (1 - 0.5); w = 1.
// FF3 0x001ddac8; from MioPan gra3dBoundingBox.c.
void gra3dbbCalcCenterBase(float *vCB, float (*avBB)[4])
{
    int iVertexIndex0;
    int iVertexIndex1;
    int c;

    if (avBB[2][1] < avBB[0][1])
    {
        iVertexIndex0 = 0;
        iVertexIndex1 = 5;
    }
    else
    {
        iVertexIndex0 = 2;
        iVertexIndex1 = 7;
    }

    for (c = 0; c < 4; c++)
    {
        vCB[c] = avBB[iVertexIndex0][c] * BB_HALF + avBB[iVertexIndex1][c] * (BB_ONE - BB_HALF);
    }

    vCB[3] = BB_ONE;
}

// Distance in XZ from the lower-face centre to corner [0] (vsub.xyz, squares,
// vaddz.x, vsqrt). No callers in FF3.
// FF3 0x001ddb38; from MioPan gra3dBoundingBox.c.
float gra3dbbCalcRadiusXZ(float (*avBB)[4])
{
    float vCenterBase[4];
    float dx;
    float dz;

    gra3dbbCalcCenterBase(vCenterBase, avBB);

    dx = vCenterBase[0] - avBB[0][0];
    dz = vCenterBase[2] - avBB[0][2];

    return sqrtf(dx * dx + dz * dz);
}

// FF3 0x001ddb90; from MioPan gra3dBoundingBox.c.
void gra3dbbCalcInnerEllipse(float *vEllipse, float (*avBB)[4])
{
    vEllipse[0] = fabsf(avBB[0][0] - avBB[1][0]) * BB_HALF;
    vEllipse[1] = fabsf(avBB[0][1] - avBB[2][1]) * BB_HALF;
    vEllipse[3] = 0.0f;
    vEllipse[2] = fabsf(avBB[0][2] - avBB[4][2]) * BB_HALF;
}

// Corners [0] / [7] are vMin / vMax themselves, the middle six are built on
// the stack with w = 1; the transform takes w = 1 either way.
// FF3 0x001ddc58; from MioPan gra3dBoundingBox.c.
void gra3dbbApplyFromBounds(float (*avBBDest)[4], float *vMin, float *vMax, float (*mat)[4])
{
    float v[4];

    CVu0Matrix::s_pFuncLoadMatrix(mat);

    CVu0Matrix::s_pFuncApplyMatrixWithoutTrans(avBBDest[0], vMin);

    v[0] = vMax[0];  v[1] = vMin[1];  v[2] = vMin[2];  v[3] = BB_ONE;
    CVu0Matrix::s_pFuncApplyMatrixWithoutTrans(avBBDest[1], v);

    v[0] = vMin[0];  v[1] = vMax[1];  v[2] = vMin[2];  v[3] = BB_ONE;
    CVu0Matrix::s_pFuncApplyMatrixWithoutTrans(avBBDest[2], v);

    v[0] = vMax[0];  v[1] = vMax[1];  v[2] = vMin[2];  v[3] = BB_ONE;
    CVu0Matrix::s_pFuncApplyMatrixWithoutTrans(avBBDest[3], v);

    v[0] = vMin[0];  v[1] = vMin[1];  v[2] = vMax[2];  v[3] = BB_ONE;
    CVu0Matrix::s_pFuncApplyMatrixWithoutTrans(avBBDest[4], v);

    v[0] = vMax[0];  v[1] = vMin[1];  v[2] = vMax[2];  v[3] = BB_ONE;
    CVu0Matrix::s_pFuncApplyMatrixWithoutTrans(avBBDest[5], v);

    v[0] = vMin[0];  v[1] = vMax[1];  v[2] = vMax[2];  v[3] = BB_ONE;
    CVu0Matrix::s_pFuncApplyMatrixWithoutTrans(avBBDest[6], v);

    CVu0Matrix::s_pFuncApplyMatrixWithoutTrans(avBBDest[7], vMax);
}
