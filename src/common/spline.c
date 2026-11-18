/* ==========================================================================
 *  common/spline.c
 *
 *  A point along a polyline as a Catmull-Rom spline (see spline.h).
 *  FF3-only, FF3 0x1e7878..0x1e7b08, unreferenced in the ROM.
 * ======================================================================== */
#include "common/spline.h"

#include <math.h>                       // sqrtf

// FF3 0x001e7878
int SplineFindSegment(float length, float *t, sceVu0FVECTOR *points, int n)
{
    float seg = 0.0f;
    float sum = 0.0f;
    float dx, dy, dz;
    int i;

    for (i = 0; i < n - 1; i++)
    {
        dx = points[i][0] - points[i + 1][0];
        dy = points[i][1] - points[i + 1][1];
        dz = points[i][2] - points[i + 1][2];
        seg = sqrtf(dx * dx + dy * dy + dz * dz);
        sum += seg;
        if (i == n - 2 || length <= sum)
            break;
    }
    sum -= seg;                         // the length where segment i starts

    *t = (seg != 0.0f) ? (length - sum) / seg : 0.0f;
    return i;
}

// FF3 0x001e7918
void SplineTangent(sceVu0FVECTOR out, sceVu0FVECTOR *points, int i, int n)
{
    float scale = 1.0f;
    int a, b, k;

    if (i == 0)
    {
        a = 0;
        b = 1;
    }
    else if (i == n - 1)
    {
        a = n - 2;
        b = i;
    }
    else
    {
        a = i - 1;
        b = i + 1;
        scale = 0.5f;
    }
    for (k = 0; k < 4; k++)
        out[k] = (points[b][k] - points[a][k]) * scale;
}

// FF3 0x001e7998
void SplineHermite(float t, sceVu0FVECTOR out, sceVu0FVECTOR p0, sceVu0FVECTOR p1,
                   sceVu0FVECTOR m0, sceVu0FVECTOR m1)
{
    float t2 = t * t;
    float h11 = t2 * t - t2;            // t^3 - t^2
    float a = h11 - t2;                 // t^3 - 2t^2
    float h00 = a + h11 + 1.0f;         // 2t^3 - 3t^2 + 1
    float h10 = a + t;                  // t^3 - 2t^2 + t
    float h01 = 1.0f - h00;
    int k;

    for (k = 0; k < 4; k++)
        out[k] = p0[k] * h00;
    for (k = 0; k < 4; k++)
        out[k] += p1[k] * h01;
    for (k = 0; k < 4; k++)
        out[k] += m0[k] * h10;
    for (k = 0; k < 4; k++)
        out[k] += m1[k] * h11;
}

// FF3 0x001e7a50
int SplineGetPoint(sceVu0FVECTOR out, sceVu0FVECTOR *points, int n, float length)
{
    sceVu0FVECTOR m0;
    sceVu0FVECTOR m1;
    float t;
    int seg;

    seg = SplineFindSegment(length, &t, points, n);
    SplineTangent(m0, points, seg, n);
    SplineTangent(m1, points, seg + 1, n);
    SplineHermite(t, out, points[seg], points[seg + 1], m0, m1);
    out[3] = 1.0f;
    return seg;
}
