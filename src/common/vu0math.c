/* ==========================================================================
 *  common/vu0math.c
 *
 *  FF3's maths helpers (see vu0math.h). FF3-only, 0x1ef370..0x1efcf0.
 *
 *  The trig functions are VU0 macro-mode code on the EE: a Taylor series for
 *  sin (to x^9, clamped to [-1, 1]) and an odd polynomial in (x-1)/(x+1) for
 *  atan, with their coefficients in this object's data. PC: the same
 *  polynomials in host float. The VU rounds by truncation and has no
 *  denormals or infinities, so results can differ from the PS2 in the last
 *  bit; the order of the operations is the ROM's.
 * ======================================================================== */
#include <math.h>                       // logf, sqrtf, fabsf

#include "common/vu0math.h"

#define VU0MATH_PI          3.14159274f     // sdata 0x370e68 (0x40490fdb)
#define VU0MATH_PI2         6.28318548f     // 0x40c90fdb
#define VU0MATH_HALF_PI     1.57079637f     // 0x3fc90fdb
#define VU0MATH_3HALF_PI    4.71238899f     // 0x4096cbe4

// data 0x3293c0: the source matrix of RotVectorX/Y/Z.
static sceVu0FMATRIX vu0math_unit = {
    { 1.0f, 0.0f, 0.0f, 0.0f },
    { 0.0f, 1.0f, 0.0f, 0.0f },
    { 0.0f, 0.0f, 1.0f, 0.0f },
    { 0.0f, 0.0f, 0.0f, 1.0f },
};

// data 0x329550: Vu0AtanPoly's operand, three quadwords loaded as vf12, vf13,
// vf16. [0] is the argument slot Vu0Atanf writes; then c1, c3 .. c15 of the
// odd polynomial and pi/4.
alignas(16) static float vu0math_atan_tbl[12] = {
    0.0f,          0.999999344f,  -0.333298564f,  0.199465364f,
    -0.139085338f, 0.0964200422f, -0.055909887f,  0.0218612291f,
    -0.00405405788f, 0.785398185f, 0.0f,          0.0f,
};

// ──────────────────────────────────────────────────────────────────────
// VU vftoi0: truncates toward zero and saturates instead of overflowing.

static int vu0math_ftoi0(float f)
{
    if (f >= 2147483648.0f)
    {
        return 0x7fffffff;
    }
    if (f <= -2147483648.0f)
    {
        return (int)0x80000000;
    }
    return (int)f;
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x001ef370; new in FF3.
//
// The ROM passes `in`, not `out`, to every step, so with out != in only the
// last non-zero angle (Y, else X, else Z) shows in the result; the callers
// that want all three pass the same vector twice. Kept as the ROM does it.

void RotVectorZXY(float *out, float *in, float *rot)
{
    out[0] = in[0];
    out[1] = in[1];
    out[2] = in[2];
    out[3] = in[3];

    if (rot[2] != 0.0f)
    {
        RotVectorZ(rot[2], out, in);
    }
    if (rot[0] != 0.0f)
    {
        RotVectorX(rot[0], out, in);
    }
    if (rot[1] != 0.0f)
    {
        RotVectorY(rot[1], out, in);
    }
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x001ef428; new in FF3 (FF2's scn_test.c RotVectorY did the same with
// fmodf). The product is inline VU0 (vmula/vmadd), i.e. sceVu0ApplyMatrix.

void RotVectorX(float rot, float *dst, float *src)
{
    sceVu0FMATRIX m;

    sceVu0RotMatrixX(m, vu0math_unit, NormalizeAngle(rot));
    sceVu0ApplyMatrix(dst, m, src);
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x001ef498; new in FF3.

void RotVectorY(float rot, float *dst, float *src)
{
    sceVu0FMATRIX m;

    sceVu0RotMatrixY(m, vu0math_unit, NormalizeAngle(rot));
    sceVu0ApplyMatrix(dst, m, src);
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x001ef508; new in FF3.

void RotVectorZ(float rot, float *dst, float *src)
{
    sceVu0FMATRIX m;

    sceVu0RotMatrixZ(m, vu0math_unit, NormalizeAngle(rot));
    sceVu0ApplyMatrix(dst, m, src);
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x001ef578; new in FF3. atan(x) for 0 <= x <= 1, x in tbl[0]:
// t = (x-1)/(x+1) and atan(x) = pi/4 + atan(t), the series in t to t^15.

static float Vu0AtanPoly(float *tbl)
{
    float t  = (tbl[0] - 1.0f) / (tbl[0] + 1.0f);
    float t2 = t * t;
    float tn = t2 * t;
    float acc;

    acc = t * tbl[1];
    acc = acc + tn * tbl[2];
    tn  = tn * t2;
    acc = acc + tn * tbl[3];
    tn  = tn * t2;
    acc = acc + tn * tbl[4];
    tn  = tn * t2;
    acc = acc + tn * tbl[5];
    tn  = tn * t2;
    acc = acc + tn * tbl[6];
    tn  = tn * t2;
    acc = acc + tn * tbl[7];
    tn  = tn * t2;
    acc = acc + tn * tbl[8];

    return acc + tbl[9];
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x001ef5e8; new in FF3. x - trunc(x / 2pi) * 2pi (sdata quadword
// 0x35b630 = {0, 1/2pi, -2pi, pi/180}).

float NormalizeAngle(float fAngle)
{
    float n = (float)vu0math_ftoi0(fAngle * 0.159154937f);

    return fAngle + n * -6.28318548f;
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x001ef620; new in FF3. Odd; above 1 through pi/2 - atan(1/x).

float Vu0Atanf(float x)
{
    if (x < 0.0f)
    {
        return -Vu0Atanf(-x);
    }

    if (1.0f < x)
    {
        vu0math_atan_tbl[0] = 1.0f / x;
        return VU0MATH_HALF_PI - Vu0AtanPoly(vu0math_atan_tbl);
    }

    vu0math_atan_tbl[0] = x;
    return Vu0AtanPoly(vu0math_atan_tbl);
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x001ef6b0; FF2 g3dAtan2f (g3dMath.c) was atan2f plus finiteness
// asserts. FF3: Vu0Atanf of the quotient, fixed up into (-pi, pi].

float g3dAtan2f(float fX, float fY)
{
    float r;

    if (fY == 0.0f)
    {
        return (fX < 0.0f) ? -VU0MATH_HALF_PI : VU0MATH_HALF_PI;
    }

    r = Vu0Atanf(fX / fY);
    if (fY < 0.0f)
    {
        r = r + VU0MATH_PI;
        if (VU0MATH_PI < r)
        {
            r = r - VU0MATH_PI2;
        }
    }
    return r;
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x001ef758; new in FF3. The angle is folded into [-pi/2, pi/2], then
// x - x^3/3! + x^5/5! - x^7/7! + x^9/9! (sdata 0x35b610), clamped to [-1, 1].

float Vu0Sinf(float x)
{
    float a = NormalizeAngle(x);
    float a2;
    float an;
    float acc;

    if (a < 0.0f)
    {
        a = a + VU0MATH_PI2;
    }
    if (VU0MATH_3HALF_PI < a)
    {
        a = a - VU0MATH_PI2;
    }
    else if (VU0MATH_HALF_PI < a)
    {
        a = VU0MATH_PI - a;
    }

    a2  = a * a;
    acc = a;
    an  = a2 * a;
    acc = acc + an * -0.166666657f;
    an  = an * a2;
    acc = acc + an * 0.00833333284f;
    an  = an * a2;
    acc = acc + an * -0.000198412687f;
    an  = an * a2;
    acc = acc + an * 2.75573188e-06f;

    // vmini / vmax against vf0.w and -1
    if (acc > 1.0f)
    {
        acc = 1.0f;
    }
    if (acc < -1.0f)
    {
        acc = -1.0f;
    }
    return acc;
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x001ef838; new in FF3.

float Vu0Cosf(float x)
{
    return Vu0Sinf(x + VU0MATH_HALF_PI);
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x001ef860; new in FF3 (FF2 g3dAcosf clamped and called acosf).
// acos(x) = atan2(sqrt(1 - x^2), x); the VU's vsqrt takes |operand|.

float Vu0Acosf(float x)
{
    return g3dAtan2f(sqrtf(fabsf(1.0f - x * x)), x);
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x001ef8a8; new in FF3, no callers.

float LogBasef(float base, float x)
{
    return logf(x) / logf(base);
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x001ef8f8; new in FF3 (texture TW/TH: SetWidth, SetHeight,
// gra3dshadowInit). Like the ROM it never returns when n is not a power of
// base; unsigned so the product wraps as the EE's mult did.

int IntLogBase(int base, int n)
{
    int   k = 0;
    u_int p = 1;

    while (p != (u_int)n)
    {
        p = p * (u_int)base;
        k++;
    }
    return k;
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x001ef918; new in FF3, no callers (Ghidra had it inside
// IntLogBase). Cubic Hermite: p0, p1 end points, m0, m1 tangents.

void HermiteVector(float t, float *out, float *p0, float *p1, float *m0, float *m1)
{
    float u   = 1.0f - t;
    float h00 = ((t + t) + 1.0f) * u * u;   // (1+2t)(1-t)^2
    float h10 = t * u * u;                  // t(1-t)^2
    float h11 = -t * t * u;                 // -t^2(1-t)
    float h01 = t * t * (3.0f - (t + t));   // t^2(3-2t)
    int   i;

    for (i = 0; i < 4; i++)
    {
        out[i] = (p0[i] * h00 + m0[i] * h10) + (m1[i] * h11 + p1[i] * h01);
    }
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x001efa08; new in FF3, no callers (hidden like HermiteVector).
// Uniform cubic B-spline segment. The ROM also adds t*0, t^2*0 and 0 terms
// (a basis-matrix row written out); they change nothing for finite t and
// are left out. The 1/6 is the ROM's 0x3e2aaaaa, one ulp under 1/6.

void BSplineVector(float t, float *out, float *p0, float *p1, float *p2, float *p3)
{
    float t2 = t * t;
    float t3 = t2 * t;
    float b0 = ((t2 * 3.0f - t3) - t * 3.0f) + 1.0f;            // (1-t)^3
    float b1 = (t3 * 3.0f - t2 * 6.0f) + 4.0f;                  // 3t^3-6t^2+4
    float b2 = ((t3 * -3.0f + t2 * 3.0f) + t * 3.0f) + 1.0f;    // -3t^3+3t^2+3t+1
    float b3 = t3;
    int   i;

    for (i = 0; i < 4; i++)
    {
        out[i] = ((p0[i] * b0 + p1[i] * b1) + (p2[i] * b2 + p3[i] * b3)) * 0.166666657f;
    }
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x001efb50; FF2 GetRandValF (utility.c). PC: rand is ReiPan_Rand, the
// EE's 31-bit range (callcheck: rand).

float GetRandValF(float max)
{
    return (max * (float)ReiPan_Rand()) / REIPAN_RAND_MAXF;
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x001efba0; new in FF3.

int GetRandValIByF(int max)
{
    return (int)GetRandValF((float)max);
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x001efbc8; FF2 RotLimitChk (unit_ctl.c), with FF3's pi (FF2:
// 3.1415925f).

void RotLimitChk(float *rot)
{
    if (VU0MATH_PI < *rot)
    {
        *rot = *rot - VU0MATH_PI2;
    }
    else if (*rot < -VU0MATH_PI)
    {
        *rot = *rot + VU0MATH_PI2;
    }
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x001efc28; FF2 GetTrgtRot (unit_ctl.c). FF3 no longer clears rot
// (the untouched angles keep their values) and inlines GetDistV, the XZ
// distance, as VU0 code.

void GetTrgtRot(float *p0, float *p1, float *rot, int id)
{
    sceVu0FVECTOR dist;

    dist[0] = p1[0] - p0[0];
    dist[1] = p1[1] - p0[1];
    dist[2] = p1[2] - p0[2];
    dist[3] = p1[3] - p0[3];

    if ((id & 1) != 0)
    {
        float dx = p0[0] - p1[0];
        float dz = p0[2] - p1[2];

        dist[3] = sqrtf(fabsf(dx * dx + dz * dz));
        rot[0]  = g3dAtan2f(-dist[1], dist[3]);
        RotLimitChk(&rot[0]);
    }

    if ((id & 2) != 0)
    {
        rot[1] = g3dAtan2f(dist[0], dist[2]);
        RotLimitChk(&rot[1]);
    }
}
