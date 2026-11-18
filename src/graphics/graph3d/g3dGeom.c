/* ==========================================================================
 *  graphics/graph3d/g3dGeom.c
 *
 *  Geometry helpers of the g3d core: the angle to a target, the "set position
 *  / direction / target / roll" builders that orient a coordinate matrix from
 *  a forward vector, axis-angle rotation, the inverse of a scaled rigid
 *  transform, planes from a matrix column / point+normal / three points, and
 *  ray/sphere and ray/ellipsoid intersection.
 *
 *  Ported from MioPan's g3dGeom.c (FF2 0x19fd98..0x1a0ff0); FF3's functions
 *  (0x1cd180..0x1cde18) are FF2's minus the asserts, plus the two "Up"
 *  variants that take the up hint as a vector. The g3dxVu0.h macro-mode
 *  blocks are written out with the lanes the EE writes:
 *    - normalise (vrsqrt Q, vf0w, |v|^2; vmul.w; vmulq.xyz): xyz scaled, w
 *      copied from the source (sceVu0Normalize would clear it);
 *    - cross product (vopmula.xyz / vopmsub.xyz): w left as the first
 *      operand's.
 *  Dot products read vf1 = (1, 1, 1, -1) (Vu0InitConstRegisters) as
 *  "vadday.x; vmaddz.x vf, vf1, vf": x + y + 1 * z. g3dMatrixGetRoll's length
 *  uses vf1 without the accumulator, see there.
 * ======================================================================== */

#include "graphics/graph3d/g3dGeom.h"

#include <math.h>                               // sqrtf

#include "sdk/libvu0.h"                         // sceVu0MulMatrix, sceVu0TransposeMatrix, ...
#include "common/vu0math.h"                     // Vu0Acosf, Vu0Sinf, Vu0Cosf, g3dAtan2f, NormalizeAngle
#include "graphics/motion/motion.h"             // motQuaternion2Matrix

static sceVu0FVECTOR s_vOrigin = { 0.0f, 0.0f, 0.0f, 1.0f };        // rdata 0x329500
static sceVu0FMATRIX s_matUnit = {                                  // data 0x3293c0
    { 1.0f, 0.0f, 0.0f, 0.0f },
    { 0.0f, 1.0f, 0.0f, 0.0f },
    { 0.0f, 0.0f, 1.0f, 0.0f },
    { 0.0f, 0.0f, 0.0f, 1.0f },
};

static inline float _Dot3(const float *v0, const float *v1)
{
    return v0[0] * v1[0] + v0[1] * v1[1] + v0[2] * v1[2];
}

// vrsqrt Q, vf0w, f: 1 / sqrt(f).
// PC: the VU gives a huge finite Q for 0 (and 0 * Q = 0); host 1/0 is inf.
static inline float _RSqrt(float f)
{
    return (f > 0.0f) ? 1.0f / sqrtf(f) : 0.0f;
}

// vmul.xyz; vaddy.x; vaddz.x; vrsqrt; vmul.w vd, vs, vf0; vmulq.xyz:
// xyz normalised, w copied.
static inline void _NormalizeKeepW(float *vDst, const float *vSrc)
{
    float q = _RSqrt(_Dot3(vSrc, vSrc));

    vDst[0] = vSrc[0] * q;
    vDst[1] = vSrc[1] * q;
    vDst[2] = vSrc[2] * q;
    vDst[3] = vSrc[3];
}

// vopmula.xyz v0, v1; vopmsub.xyz v0, v1, v0: v0.xyz = v0 x v1, w kept.
static inline void _OuterKeepW(float *vDst, const float *v0, const float *v1)
{
    float x = v0[1] * v1[2] - v1[1] * v0[2];
    float y = v0[2] * v1[0] - v1[2] * v0[0];
    float z = v0[0] * v1[1] - v1[0] * v0[1];

    vDst[3] = v0[3];
    vDst[0] = x;
    vDst[1] = y;
    vDst[2] = z;
}

// vmulax.xyz m0, v; vmadday.xyz m1, v; vmaddz.xyz vd, m2, v: the 3x3 part of
// m applied to v, w copied from v.
static inline void _ApplyMatrixXYZ(float *vDst, float (*m)[4], const float *v)
{
    float x = m[0][0] * v[0] + m[1][0] * v[1] + m[2][0] * v[2];
    float y = m[0][1] * v[0] + m[1][1] * v[1] + m[2][1] * v[2];
    float z = m[0][2] * v[0] + m[1][2] * v[1] + m[2][2] * v[2];

    vDst[3] = v[3];
    vDst[0] = x;
    vDst[1] = y;
    vDst[2] = z;
}

// ──────────────────────────────────────────────────────────────────────
// Angle between vDirection and the target -> origin direction; 0 when
// vDirection points straight at vTarget.
// FF3 0x001cd180; FF2 g3dCalcAngle 0x19fd98 (acosf there, Vu0Acosf here).

float g3dCalcAngle(float *vDirection, float *vOrigin, float *vTarget)
{
    sceVu0FVECTOR vDir0;
    sceVu0FVECTOR vDir1;

    sceVu0SubVector(vDir0, vOrigin, vTarget);
    _NormalizeKeepW(vDir0, vDir0);
    _NormalizeKeepW(vDir1, vDirection);

    return Vu0Acosf(-_Dot3(vDir0, vDir1));
}

// ──────────────────────────────────────────────────────────────────────
// Row 3 (the position) = vPos, all four lanes.
// FF3 0x001cd220; FF2 g3dMatrixSetPosition 0x19fe70.

void g3dMatrixSetPosition(float (*mat)[4], float *vPos)
{
    sceVu0CopyVector(mat[3], vPos);
}

// ──────────────────────────────────────────────────────────────────────
// Point the matrix's forward (row 2) along vDir and rebuild right and up
// against an up hint: world (0, 1, 0, 1) when bFixUp, else the current row 1.
// Row w lanes: Z keeps vDir's, X the hint's, Y Z's.
// FF3 0x001cd230; FF2 g3dMatrixSetDirection 0x19fe88.

void g3dMatrixSetDirection(float (*mat)[4], float *vDir, int bFixUp)
{
    sceVu0FVECTOR vFixUp;
    float *rvY;
    float *rvZ;

    rvY = mat[1];
    rvZ = mat[2];

    vFixUp[0] = 0.0f;
    vFixUp[1] = 1.0f;                           // sdata 0x370a54
    vFixUp[2] = 0.0f;
    vFixUp[3] = 1.0f;

    if (bFixUp == 0)
    {
        sceVu0CopyVector(vFixUp, rvY);
    }

    _NormalizeKeepW(rvZ, vDir);                 // Z = |vDir|

    _OuterKeepW(mat[0], vFixUp, rvZ);           // X = |up x Z|
    _NormalizeKeepW(mat[0], mat[0]);

    _OuterKeepW(rvY, rvZ, mat[0]);              // Y = |Z x X|
    _NormalizeKeepW(rvY, rvY);
}

// ──────────────────────────────────────────────────────────────────────
// g3dMatrixSetDirection with the up hint given as a vector.
// FF3 0x001cd300; new in FF3 (g3dMatrixSetDirection's body, vUp for the hint).

void g3dMatrixSetDirectionUp(float (*mat)[4], float *vDir, float *vUp)
{
    float *rvY;
    float *rvZ;

    rvY = mat[1];
    rvZ = mat[2];

    _NormalizeKeepW(rvZ, vDir);                 // Z = |vDir|

    _OuterKeepW(mat[0], vUp, rvZ);              // X = |up x Z|
    _NormalizeKeepW(mat[0], mat[0]);

    _OuterKeepW(rvY, rvZ, mat[0]);              // Y = |Z x X|
    _NormalizeKeepW(rvY, rvY);
}

// ──────────────────────────────────────────────────────────────────────
// Aim the matrix at the point vTarget (vTarget - position, all four lanes).
// FF3 0x001cd3a0; FF2 g3dMatrixSetTarget 0x1a0018.

void g3dMatrixSetTarget(float (*mat)[4], float *vTarget, int bFixUp)
{
    sceVu0FVECTOR v;

    sceVu0SubVector(v, vTarget, mat[3]);
    g3dMatrixSetDirection(mat, v, bFixUp);
}

// ──────────────────────────────────────────────────────────────────────
// g3dMatrixSetTarget with an explicit up vector.
// FF3 0x001cd3d0; new in FF3.

void g3dMatrixSetTargetUp(float (*mat)[4], float *vTarget, float *vUp)
{
    sceVu0FVECTOR v;

    sceVu0SubVector(v, vTarget, mat[3]);
    g3dMatrixSetDirectionUp(mat, v, vUp);
}

// ──────────────────────────────────────────────────────────────────────
// Roll about the local Z axis: mat = RotZ(-fRoll) * mat (row vectors).
// FF3 0x001cd400; FF2 g3dMatrixSetRoll 0x1a0048 (FF2 wrapped the angle
// inline, FF3 calls NormalizeAngle).

void g3dMatrixSetRoll(float (*mat)[4], float fRoll)
{
    sceVu0FMATRIX matRot;

    sceVu0RotMatrixZ(matRot, s_matUnit, NormalizeAngle(-fRoll));
    sceVu0MulMatrix(mat, mat, matRot);          // inline vmulax..vmaddw in the ROM
}

// ──────────────────────────────────────────────────────────────────────
// The roll angle back from row 0: -atan2(m01, len).
// FF3 0x001cd4b0; FF2 g3dMatrixGetRoll 0x1a0160.
// The length is "vmul.xyz vf13, row0, row0; vaddz.x vf13, vf1, vf13; vsqrt":
// vf1.x is the constant 1.0, so len = sqrt(1 + m02^2), not sqrt(m00^2 +
// m02^2) as MioPan reads it. FF2's code is the same; kept as the ROM does it
// (only an unrolled X row, m01 = 0, reads back 0).

float g3dMatrixGetRoll(float (*mat)[4])
{
    float fLen;

    fLen = sqrtf(1.0f + mat[0][2] * mat[0][2]);

    return -g3dAtan2f(mat[0][1], fLen);
}

// ──────────────────────────────────────────────────────────────────────
// Rotate about the unit axis vAxis by fAngle: quaternion (vAxis * sin(a/2),
// cos(a/2)) -> matrix, mat = Rot * mat, then re-orthonormalise.
// FF3 0x001cd4f0; FF2 g3dMatrixRotationByAxis 0x1a01a0.

void g3dMatrixRotationByAxis(float (*mat)[4], float *vAxis, float fAngle)
{
    sceVu0FMATRIX matRot;
    sceVu0FVECTOR q;
    float fHalf;

    fHalf = fAngle * 0.5f;                      // sdata 0x370a58

    sceVu0ScaleVector(q, vAxis, Vu0Sinf(fHalf));        // vmulx.xyzw
    q[3] = Vu0Cosf(fHalf);

    motQuaternion2Matrix(matRot, q);
    sceVu0MulMatrix(mat, mat, matRot);          // inline vmulax..vmaddw in the ROM

    g3dMatrixSetDirection(mat, mat[2], 1);
}

// ──────────────────────────────────────────────────────────────────────
// Inverse of a scale + rotation + translation matrix: transpose, column 3 =
// (0, 0, 0, 1), each row divided by its length twice (unit -> 1 / scale),
// translation = -position through that matrix.
// FF3 0x001cd5f8; FF2 g3dMatrixInverseTransform 0x1a03d8.

void g3dMatrixInverseTransform(float (*matDest)[4], float (*matSrc)[4])
{
    sceVu0FMATRIX matWork;
    sceVu0FVECTOR vScale;
    sceVu0FVECTOR vTrans;
    int i;

    sceVu0TransposeMatrix(matWork, matSrc);     // pextlw/pcpyld in the ROM
    g3dMatrixSetColumn(matWork, s_vOrigin, 3);

    for (i = 0; i < 3; i++)
    {
        vScale[i] = _RSqrt(_Dot3(matWork[i], matWork[i]));
    }
    vScale[3] = 0.0f;                           // never set on the EE, never read

    for (i = 0; i < 3; i++)                     // vmulx.xyz: w kept
    {
        matWork[i][0] *= vScale[i];
        matWork[i][1] *= vScale[i];
        matWork[i][2] *= vScale[i];
    }

    // vsub.xyz from (0, 0, 0, 1): w stays 1, so row 3 adds in.
    vTrans[0] = s_vOrigin[0] - matSrc[3][0];
    vTrans[1] = s_vOrigin[1] - matSrc[3][1];
    vTrans[2] = s_vOrigin[2] - matSrc[3][2];
    vTrans[3] = s_vOrigin[3];

    for (i = 0; i < 3; i++)                     // vmulx/y/z.xyzw
    {
        sceVu0ScaleVector(matWork[i], matWork[i], vScale[i]);
    }

    sceVu0ApplyMatrix(matDest[3], matWork, vTrans);
    sceVu0CopyVector(matDest[0], matWork[0]);
    sceVu0CopyVector(matDest[1], matWork[1]);
    sceVu0CopyVector(matDest[2], matWork[2]);
}

// ──────────────────────────────────────────────────────────────────────
// Column iCol = v (four lanes).
// FF3 0x001cd7f0; FF2 g3dMatrixSetColumn 0x1a05e8 (its iCol < 4 assert dropped).

void g3dMatrixSetColumn(float (*mat)[4], float *v, int iCol)
{
    float *pv;

    pv = mat[0] + iCol;
    pv[0]  = v[0];
    pv[4]  = v[1];
    pv[8]  = v[2];
    pv[12] = v[3];
}

// ──────────────────────────────────────────────────────────────────────
// Column iCol rows 0..2 = v.xyz; row 3 untouched.
// FF3 0x001cd820; FF2 g3dMatrixSetColumnXYZ 0x1a0688.

void g3dMatrixSetColumnXYZ(float (*mat)[4], float *v, int iCol)
{
    float *pv;

    pv = mat[0] + iCol;
    pv[0] = v[0];
    pv[4] = v[1];
    pv[8] = v[2];
}

// ──────────────────────────────────────────────────────────────────────
// Column iCol read out as a plane (four lanes).
// FF3 0x001cd848; FF2 _PlaneFromMatrix 0x1a0720.

static void _PlaneFromMatrix(float *plane, float (*mat)[4], int iCol)
{
    float *pv;

    pv = mat[0] + iCol;
    plane[0] = pv[0];
    plane[1] = pv[4];
    plane[2] = pv[8];
    plane[3] = pv[12];
}

// ──────────────────────────────────────────────────────────────────────
// The Z column as a plane (a clip matrix's near/far plane).
// FF3 0x001cd878; FF2 g3dPlaneFromMatrixZ.

void g3dPlaneFromMatrixZ(float *plane, float (*mat)[4])
{
    _PlaneFromMatrix(plane, mat, 2);
}

// ──────────────────────────────────────────────────────────────────────
// Plane (n, -dot(n, p)) through vPoint with normal vNormal.
// FF3 0x001cd890; FF2 g3dCalcPlaneFromPointNormal 0x1a07e0.

void g3dCalcPlaneFromPointNormal(float *plane, float *vPoint, float *vNormal)
{
    sceVu0CopyVector(plane, vNormal);
    plane[3] = -_Dot3(vNormal, vPoint);
}

// ──────────────────────────────────────────────────────────────────────
// Plane through three points: normal = |(|P1 - P0|) x (|P2 - P0|)|.
// FF3 0x001cd8c8; FF2 g3dCalcPlaneFromPoints 0x1a0818.

void g3dCalcPlaneFromPoints(float *plane, float *vP0, float *vP1, float *vP2)
{
    sceVu0FVECTOR vNorm;
    sceVu0FVECTOR vCross;
    sceVu0FVECTOR vDir01;
    sceVu0FVECTOR vDir02;

    sceVu0SubVector(vDir01, vP1, vP0);
    _NormalizeKeepW(vDir01, vDir01);

    sceVu0SubVector(vDir02, vP2, vP0);
    _NormalizeKeepW(vDir02, vDir02);

    _OuterKeepW(vCross, vDir01, vDir02);
    _NormalizeKeepW(vNorm, vCross);

    g3dCalcPlaneFromPointNormal(plane, vP0, vNorm);
}

// ──────────────────────────────────────────────────────────────────────
// Ray (vStart, vDir) against the sphere (vCenter, fRadius): solves
// a t^2 + b t + c = 0 along the normalised direction and writes the 0, 1 or 2
// hit points vStart + t |vDir| (w = vStart's) to avRet; returns their count.
// FF3 0x001cd998; FF2 g3dCalcIntersectionSphereAndLine 0x1a0930 (its six
// "hit within radius" asserts dropped).

int g3dCalcIntersectionSphereAndLine(float (*avRet)[4], float *vCenter, float fRadius,
                                     float *vStart, float *vDir)
{
    sceVu0FVECTOR vStartTrans;
    sceVu0FVECTOR vDirNormalized;
    sceVu0FVECTOR vHit;
    float fA;
    float fB;
    float fC;
    float fD;
    float fSqrtD;
    float fT;

    // vsub.xyz: w is vStart's (unused).
    vStartTrans[0] = vStart[0] - vCenter[0];
    vStartTrans[1] = vStart[1] - vCenter[1];
    vStartTrans[2] = vStart[2] - vCenter[2];
    vStartTrans[3] = vStart[3];
    _NormalizeKeepW(vDirNormalized, vDir);

    fA = _Dot3(vDirNormalized, vDirNormalized);
    fB = _Dot3(vStartTrans, vDirNormalized) * 2.0f;
    fC = _Dot3(vStartTrans, vStartTrans);

    fD = fB * fB - fA * 4.0f * (fC - fRadius * fRadius);      // sdata 0x370a5c

    if (fD < 0.0f)
    {
        return 0;
    }

    if (fD == 0.0f)
    {
        fT = -fB / (fA + fA);

        // The ROM normalises the direction again for each hit.
        _NormalizeKeepW(vHit, vDirNormalized);
        sceVu0ScaleVectorXYZ(vHit, vHit, fT);
        avRet[0][0] = vStart[0] + vHit[0];      // vadd.xyz: w = vStart's
        avRet[0][1] = vStart[1] + vHit[1];
        avRet[0][2] = vStart[2] + vHit[2];
        avRet[0][3] = vStart[3];
        return 1;
    }

    fSqrtD = sqrtf(fD);

    fT = (-fB - fSqrtD) / (fA + fA);
    _NormalizeKeepW(vHit, vDirNormalized);
    sceVu0ScaleVectorXYZ(vHit, vHit, fT);
    avRet[0][0] = vStart[0] + vHit[0];
    avRet[0][1] = vStart[1] + vHit[1];
    avRet[0][2] = vStart[2] + vHit[2];
    avRet[0][3] = vStart[3];

    fT = (fSqrtD - fB) / (fA + fA);
    _NormalizeKeepW(vHit, vDirNormalized);
    sceVu0ScaleVectorXYZ(vHit, vHit, fT);
    avRet[1][0] = vStart[0] + vHit[0];
    avRet[1][1] = vStart[1] + vHit[1];
    avRet[1][2] = vStart[2] + vHit[2];
    avRet[1][3] = vStart[3];

    return 2;
}

// ──────────────────────────────────────────────────────────────────────
// Effective radius of the ellipsoid e along a plane normal: the length of
// (e0.n, e1.n, e2.n). No callers in the retail ELF.
// FF3 0x001cdb90; FF2 g3dCalcEllipseEffectiveRadius.

float g3dCalcEllipseEffectiveRadius(float (*e)[4], float *plane)
{
    sceVu0FVECTOR vvv;

    vvv[0] = _Dot3(e[0], plane);
    vvv[1] = _Dot3(e[1], plane);
    vvv[2] = _Dot3(e[2], plane);
    vvv[3] = 0.0f;                              // never set on the EE, never read

    return sqrtf(_Dot3(vvv, vvv));
}

// ──────────────────────────────────────────────────────────────────────
// Ray (vStart, vDir) against the ellipsoid e (rows 0..2 the axes, row 3 the
// centre): the ray goes into unit-sphere space through e's inverse (adjugate
// / determinant, inline in the ROM), g3dCalcIntersectionSphereAndLine finds
// the hits, and they come back through e.
// FF3 0x001cdc28; FF2 g3dCalcIntersectionEllipseAndLine 0x1a0e00.
// The way back is "vmula.xyz e0, p; vmadda.xyz e1, p; vmadd.xyz e2, p; vadd
// e3": component-wise products, not the broadcast vmulax/vmadday/vmaddz of a
// matrix apply, so a hit is (e0 + e1 + e2) * p + e3 per lane -- right only
// for an axis-aligned e. Kept as the ROM does it.

int g3dCalcIntersectionEllipseAndLine(float (*avRet)[4], float (*e)[4], float *vStart, float *vDir)
{
    sceVu0FMATRIX matInv;
    sceVu0FVECTOR vStartInv;
    sceVu0FVECTOR vDirInv;
    float fInvDet;
    int iNumIntersection;
    int i;
    int j;

    // Cofactor rows (e1 x e2, e2 x e0, e0 x e1), transposed and / det.
    _OuterKeepW(matInv[0], e[1], e[2]);
    _OuterKeepW(matInv[1], e[2], e[0]);
    _OuterKeepW(matInv[2], e[0], e[1]);
    fInvDet = 1.0f / _Dot3(e[0], matInv[0]);    // vdiv Q, vf0w, det
    sceVu0CopyVector(matInv[3], s_vOrigin);
    sceVu0TransposeMatrix(matInv, matInv);
    for (i = 0; i < 3; i++)                     // vmuly.xyz
    {
        matInv[i][0] *= fInvDet;
        matInv[i][1] *= fInvDet;
        matInv[i][2] *= fInvDet;
    }

    // Start: (vStart - e3) through the 3x3 inverse; w = vStart's.
    vStartInv[0] = vStart[0] - e[3][0];
    vStartInv[1] = vStart[1] - e[3][1];
    vStartInv[2] = vStart[2] - e[3][2];
    vStartInv[3] = vStart[3];
    _ApplyMatrixXYZ(vStartInv, matInv, vStartInv);

    _NormalizeKeepW(vDirInv, vDir);
    _ApplyMatrixXYZ(vDirInv, matInv, vDirInv);

    iNumIntersection = g3dCalcIntersectionSphereAndLine(avRet, s_vOrigin, 1.0f,     // sdata 0x370a60
                                                        vStartInv, vDirInv);

    for (i = iNumIntersection; 0 < i; i--)
    {
        float *p = avRet[0];
        sceVu0FVECTOR v;

        for (j = 0; j < 3; j++)
        {
            v[j] = e[0][j] * p[j] + e[1][j] * p[j] + e[2][j] * p[j] + e[3][j];
        }
        // w: e3.w plus a stale VU register lane on the EE; taken as e3.w.
        v[3] = e[3][3];
        sceVu0CopyVector(p, v);
        avRet++;
    }

    return iNumIntersection;
}
