/* ==========================================================================
 *  ingame/map/hit_check_base.h
 *
 *  Hit check primitives (FF2 hit_check_base.c, MioPan port). FF3
 *  0x001e7b08..0x001e8738: FF2's functions in FF2's order, with FF3's two
 *  box helpers in front and HcBaseIsInRectXZ / HcBaseIsLineHitRectXZ new.
 *  Vectors are sceVu0FVECTORs (16 bytes); a rectangle / quad is 4 of them.
 * ======================================================================== */
#ifndef _INGAME_MAP_HIT_CHECK_BASE_H
#define _INGAME_MAP_HIT_CHECK_BASE_H

// pos inside the XZ box of rectangle vec grown by r (bounds included).
int HcBaseIsNearRectangle(float r, const float *vec, const float *pos);    // FF3 0x001e7b08
// The XZ boxes of quad and segment p0-p1 overlap.
int HcBaseIsNearSegRect(const float *quad, const float *p0, const float *p1);   // FF3 0x001e7ba0
// p inside the triangle a, b, c (sceVu0FVECTORs) in x / z; edges are outside.
int HcBaseIsInTriXZ(const float *p, const float *a, const float *b, const float *c);  // FF3 0x001e7cb8
// p inside the 4-vertex rectangle rect (4 x sceVu0FVECTOR) in x / z.
int HcBaseIsInRectXZ(const float *p, const float *rect);    // FF3 0x001e7d70
// Segments line1 and line2 cross in x / z (touching counts).
int HcBaseLineIntersect2(const float *line1_1, const float *line1_2,
                         const float *line2_1, const float *line2_2);    // FF3 0x001e7de0
// 1 when the segment p0-p1 crosses an edge of the quad (4 x sceVu0FVECTOR) in x / z.
int HcBaseIsLineHitRectXZ(const float *p0, const float *p1, const float *quad);    // FF3 0x001e7fa0
// 1 when the segment a-b crosses the plane of triangle v0, v1, v2; the
// crossing point goes to pos (w = 1). Plane only, not the triangle.
int HcBaseIsLineHitFace(float *pos, const float *a, const float *b,
                        const float *v0, const float *v1, const float *v2);    // FF3 0x001e8048
// 0 when the bounding boxes of segment pos1-pos2 and the triangle miss on an axis.
int HcBaseIsNearSegTri(const float *pos1, const float *pos2,
                       const float *tri0, const float *tri1, const float *tri2);    // FF3 0x001e81e0
// Nearest feature of rectangle vec to pos within r, in x / z: 0 clear,
// 1 a corner (a), 2 an edge (a, b); the distance in *len.
int HcBasePointRectangle(float *len, const float *pos, float *a, float *b,
                         const float *vec, float r);    // FF3 0x001e82f8

#endif /* _INGAME_MAP_HIT_CHECK_BASE_H */
