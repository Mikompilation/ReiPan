/* ==========================================================================
 *  common/spline.h
 *
 *  A point at a length along a polyline, smoothed as a Catmull-Rom spline
 *  (cubic Hermite segments with central-difference tangents).
 *
 *  FF3-only (no FF2 match), FF3 0x1e7878..0x1e7b08, between
 *  system/pad/vib_pulse.c and ingame/map/hit_check_base.c. Nothing in the
 *  ROM calls SplineGetPoint (no jal, data word or address pair).
 * ======================================================================== */
#ifndef _COMMON_SPLINE_H
#define _COMMON_SPLINE_H

#include "sdk/libvu0.h"                 // sceVu0FVECTOR

// The segment of points[0..n-1] holding length (xyz lengths summed), *t the
// fraction along it; past the end, the last segment with t > 1.
int  SplineFindSegment(float length, float *t, sceVu0FVECTOR *points, int n);   // FF3 0x001e7878
// Tangent at points[i]: one-sided at the ends, (p[i+1] - p[i-1]) / 2 inside.
void SplineTangent(sceVu0FVECTOR out, sceVu0FVECTOR *points, int i, int n);    // FF3 0x001e7918
// Cubic Hermite: p0, p1 with tangents m0, m1 at t (all four components).
void SplineHermite(float t, sceVu0FVECTOR out, sceVu0FVECTOR p0, sceVu0FVECTOR p1,
                   sceVu0FVECTOR m0, sceVu0FVECTOR m1);                         // FF3 0x001e7998
// The point at length along points[0..n-1], w = 1; returns its segment.
int  SplineGetPoint(sceVu0FVECTOR out, sceVu0FVECTOR *points, int n, float length);  // FF3 0x001e7a50

#endif /* _COMMON_SPLINE_H */
