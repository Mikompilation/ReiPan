// ingame/map/hit_check_base.c -- shared geometric hit tests: points in
// triangles / rectangles, segment crossings, box rejects.
// FF3 0x001e7b08..0x001e8738; FF2 hit_check_base.c (MioPan port), in FF3's order.
//
// FF3 against FF2: the two box tests come first (HcBaseIsNearRectangle is
// FF2's static, now with vmax / vmini and inclusive bounds; HcBaseIsNearSegRect
// is new); HcBaseIsInTriXZ multiplies the cross products' y and wants both
// products > 0, so a point on an edge is outside (FF2: >= / <= per sign);
// HcBaseIsInRectXZ (two triangles) and HcBaseIsLineHitRectXZ (segment against
// the four edges) are new; HcBasePointLineXZ is inlined into
// HcBasePointRectangle. FF2's dead HcBaseLineIntersect / Straddle / Side are gone.
//
// Every "XZ" product is the VU0 vmul.xz + vaddz.x pair: y is ignored.

#include "ingame/map/hit_check_base.h"

#include <math.h>                               // sqrtf, fabsf

// y of the cross product a x b (sceVu0OuterProduct's y lane): the 2D perp-dot in x / z.
static float HcCrossY(const float *a, const float *b)
{
    return a[2] * b[0] - a[0] * b[2];
}

static float HcDotXZ(const float *a, const float *b)
{
    return a[0] * b[0] + a[2] * b[2];
}

static float HcDotXYZ(const float *a, const float *b)
{
    return a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
}

static void HcSub(float *out, const float *a, const float *b)
{
    for (int i = 0; i < 4; i++)
    {
        out[i] = a[i] - b[i];
    }
}

static void HcCopy(float *out, const float *a)
{
    for (int i = 0; i < 4; i++)
    {
        out[i] = a[i];
    }
}

// VU0 vsqrt takes the root of |x|; sqrtf of a rounding-negative would give NaN.
static float HcSqrt(float x)
{
    return sqrtf(fabsf(x));
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x001e7b08 (FF2's static HcBaseIsNearRectangle)

int HcBaseIsNearRectangle(float r, const float *vec, const float *pos)
{
    float max[3], min[3];
    int   i, j;

    for (j = 0; j < 3; j++)
    {
        max[j] = min[j] = vec[j];
    }
    for (i = 1; i < 4; i++)
    {
        for (j = 0; j < 3; j++)
        {
            float v = vec[i * 4 + j];
            if (max[j] < v) max[j] = v;
            if (v < min[j]) min[j] = v;
        }
    }

    return min[0] - r <= pos[0] && min[2] - r <= pos[2] &&
           pos[0] <= max[0] + r && pos[2] <= max[2] + r;
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x001e7ba0: the XZ boxes of the quad and the segment overlap when the
// box around both is no wider than the two widths together.

int HcBaseIsNearSegRect(const float *quad, const float *p0, const float *p1)
{
    float qmax[3], qmin[3], smax[3], smin[3], amax[3], amin[3];
    int   i, j;

    for (j = 0; j < 3; j++)
    {
        qmax[j] = qmin[j] = quad[j];
    }
    for (i = 1; i < 4; i++)
    {
        for (j = 0; j < 3; j++)
        {
            float v = quad[i * 4 + j];
            if (qmax[j] < v) qmax[j] = v;
            if (v < qmin[j]) qmin[j] = v;
        }
    }
    for (j = 0; j < 3; j++)
    {
        smax[j] = (p0[j] < p1[j]) ? p1[j] : p0[j];
        smin[j] = (p1[j] < p0[j]) ? p1[j] : p0[j];

        amax[j] = qmin[j];
        amin[j] = qmin[j];
        const float vals[3] = { qmax[j], smin[j], smax[j] };
        for (i = 0; i < 3; i++)
        {
            if (amax[j] < vals[i]) amax[j] = vals[i];
            if (vals[i] < amin[j]) amin[j] = vals[i];
        }
    }

    return amax[0] - amin[0] <= (smax[0] - smin[0]) + (qmax[0] - qmin[0]) &&
           amax[2] - amin[2] <= (smax[2] - smin[2]) + (qmax[2] - qmin[2]);
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x001e7cb8: the three corner -> p cross products agree in sign
// (either winding); zero counts as outside.

int HcBaseIsInTriXZ(const float *p, const float *a, const float *b, const float *c)
{
    float pa[4], pb[4], pc[4];
    float c0, c1, c2;

    HcSub(pa, a, p);
    HcSub(pb, b, p);
    HcSub(pc, c, p);

    c0 = HcCrossY(pa, pb);
    c1 = HcCrossY(pb, pc);
    if (0.0f < c0 * c1)
    {
        c2 = HcCrossY(pc, pa);
        if (0.0f < c0 * c2)
        {
            return 1;
        }
    }
    return 0;
}

// FF3 0x001e7d70 (FF3-only): the rectangle as triangles 0-1-2 and 0-2-3.

int HcBaseIsInRectXZ(const float *p, const float *rect)
{
    if (HcBaseIsInTriXZ(p, &rect[0], &rect[4], &rect[8]) != 0)
    {
        return 1;
    }
    return HcBaseIsInTriXZ(p, &rect[0], &rect[8], &rect[12]) != 0;
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x001e7de0: box reject on x and z both ways, then each segment
// straddles the other's line (the cross products' y; zero is a hit).

int HcBaseLineIntersect2(const float *line1_1, const float *line1_2,
                         const float *line2_1, const float *line2_2)
{
    float line12[4], line1p[4];
    float outer, outer2;

    if (fminf(line1_1[0], line1_2[0]) > fmaxf(line2_1[0], line2_2[0])) return 0;
    if (fminf(line1_1[2], line1_2[2]) > fmaxf(line2_1[2], line2_2[2])) return 0;
    if (fminf(line2_1[0], line2_2[0]) > fmaxf(line1_1[0], line1_2[0])) return 0;
    if (fminf(line2_1[2], line2_2[2]) > fmaxf(line1_1[2], line1_2[2])) return 0;

    HcSub(line12, line1_2, line1_1);
    HcSub(line1p, line2_1, line1_1);
    outer = HcCrossY(line12, line1p);
    HcSub(line1p, line2_2, line1_1);
    outer2 = HcCrossY(line12, line1p);
    if (outer * outer2 > 0.0f)
    {
        return 0;
    }

    HcSub(line12, line2_2, line2_1);
    HcSub(line1p, line1_1, line2_1);
    outer = HcCrossY(line12, line1p);
    HcSub(line1p, line1_2, line2_1);
    outer2 = HcCrossY(line12, line1p);
    if (outer * outer2 > 0.0f)
    {
        return 0;
    }
    return 1;
}

// FF3 0x001e7fa0 (FF3-only)

int HcBaseIsLineHitRectXZ(const float *p0, const float *p1, const float *quad)
{
    if (HcBaseIsNearSegRect(quad, p0, p1) == 0)
    {
        return 0;
    }
    for (int i = 0; i < 4; i++)
    {
        if (HcBaseLineIntersect2(p0, p1, &quad[i * 4], &quad[((i + 1) & 3) * 4]) != 0)
        {
            return 1;
        }
    }
    return 0;
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x001e8048: face = (n, -(n.v0)), t = -(face.a) / (face.ab) in [0, 1].
// callcheck: sceVu0OuterProduct, sceVu0ScaleVector, fptodp, __muldf3, dptofp (written inline)

int HcBaseIsLineHitFace(float *pos, const float *a, const float *b,
                        const float *v0, const float *v1, const float *v2)
{
    float v01[4], v02[4], n[4], ab[4];
    float d, t, den;

    HcSub(v01, v1, v0);
    HcSub(v02, v2, v0);
    n[0] = v01[1] * v02[2] - v01[2] * v02[1];
    n[1] = HcCrossY(v01, v02);
    n[2] = v01[0] * v02[1] - v01[1] * v02[0];

    // The ROM negates through doubles (fptodp / __muldf3 by -1.0 / dptofp).
    d = (float)((double)HcDotXYZ(n, v0) * -1.0);

    HcSub(ab, b, a);
    den = HcDotXYZ(n, ab);
    if (den == 0.0f)
    {
        return 0;
    }

    t = -((HcDotXYZ(n, a) + d) / den);
    if (t >= 0.0f && t <= 1.0f)
    {
        for (int i = 0; i < 3; i++)
        {
            pos[i] = a[i] + ab[i] * t;
        }
        pos[3] = 1.0f;
        return 1;
    }
    return 0;
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x001e81e0: the xyz boxes of the segment and the triangle overlap on
// every axis (touching is a miss).

int HcBaseIsNearSegTri(const float *pos1, const float *pos2,
                       const float *tri0, const float *tri1, const float *tri2)
{
    float trimax[4], trimin[4], segmax[4], segmin[4];

    HcCopy(trimax, tri0);
    HcCopy(trimin, tri0);
    HcCopy(segmax, pos1);
    HcCopy(segmin, pos1);

    for (int j = 0; j < 3; j++)
    {
        if (trimax[j] < tri1[j])
            trimax[j] = tri1[j];
        else if (tri1[j] < trimin[j])
            trimin[j] = tri1[j];

        if (trimax[j] < tri2[j])
            trimax[j] = tri2[j];
        else if (tri2[j] < trimin[j])
            trimin[j] = tri2[j];

        if (segmax[j] < pos2[j])
            segmax[j] = pos2[j];
        else if (pos2[j] < segmin[j])
            segmin[j] = pos2[j];

        if (segmin[j] >= trimax[j] || trimin[j] >= segmax[j])
        {
            return 0;
        }
    }
    return 1;
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x001e82f8. Each edge: pos behind its first end -> that corner, behind
// its second end -> that corner, else the perpendicular (FF2's
// HcBasePointLineXZ, inline here). Corner distances stay squared until the
// end; an edge in range beats a nearer-or-equal corner only when shorter.

int HcBasePointRectangle(float *len, const float *pos, float *a, float *b,
                         const float *vec, float r)
{
    int   p_flg = 0, l_flg = 0;
    float p_vec[4], l_vec[2][4];
    float tmp1[4], tmp2[4];
    float p_len = -1.0f, l_len = -1.0f, tmp_len;

    if (HcBaseIsNearRectangle(r, vec, pos) == 0)
    {
        return 0;
    }

    for (int i = 0; i < 4; i++)
    {
        const float *line_p1 = &vec[i * 4];
        const float *line_p2 = &vec[((i + 1) % 4) * 4];

        HcSub(tmp1, pos, line_p1);
        HcSub(tmp2, line_p2, line_p1);
        if (HcDotXZ(tmp1, tmp2) < 0.0f)
        {
            tmp_len = HcDotXZ(tmp1, tmp1);
            if (p_flg == 0 || tmp_len < p_len)
            {
                p_len = tmp_len;
                HcCopy(p_vec, line_p1);
                p_flg = 1;
            }
            continue;
        }

        HcSub(tmp1, pos, line_p2);
        HcSub(tmp2, line_p1, line_p2);
        if (HcDotXZ(tmp1, tmp2) < 0.0f)
        {
            tmp_len = HcDotXZ(tmp1, tmp1);
            if (p_flg == 0 || tmp_len < p_len)
            {
                p_len = tmp_len;
                HcCopy(p_vec, line_p2);
                p_flg = 1;
            }
            continue;
        }

        // Distance from pos to the line through line_p1 along line_p1 - line_p2.
        HcSub(tmp1, pos, line_p1);
        tmp_len = HcDotXZ(tmp1, tmp1);
        if (HcDotXZ(tmp2, tmp2) != 0.0f)
        {
            float dot = HcDotXZ(tmp1, tmp2);
            tmp_len -= (dot * dot) / HcDotXZ(tmp2, tmp2);
        }
        tmp_len = HcSqrt(tmp_len);
        if (l_flg == 0 || tmp_len < l_len)
        {
            l_len = tmp_len;
            HcCopy(l_vec[0], line_p1);
            HcCopy(l_vec[1], line_p2);
            l_flg = 1;
        }
    }

    tmp_len = HcSqrt(p_len);

    if (p_flg == 0)
    {
        if (r <= l_len)
        {
            return 0;
        }
        *len = l_len;
        HcCopy(a, l_vec[0]);
        HcCopy(b, l_vec[1]);
        return 2;
    }

    if (l_flg != 0 && l_len < tmp_len && l_len < r)
    {
        *len = l_len;
        HcCopy(a, l_vec[0]);
        HcCopy(b, l_vec[1]);
        return 2;
    }

    if (r <= tmp_len)
    {
        return 0;
    }
    *len = tmp_len;
    HcCopy(a, p_vec);
    return 1;
}
