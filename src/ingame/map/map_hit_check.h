/* ==========================================================================
 *  ingame/map/map_hit_check.h
 *
 *  Wall collision and line of sight (FF2 map_hit_check.c), FF3
 *  0x001fe3d8..0x001fea48. FF2's two push helpers are unchanged; FF2's
 *  MapHitCheck / MapHitLineCheck over map_rectangle.c's hit caches are gone:
 *  FF3 pushes circles out of the room's rcb quads (MapRcb.c) and the door /
 *  event quads (MapLight.c), and tests lines against RegDat rectangles.
 *  Vectors are sceVu0FVECTORs (16 bytes); a quad is 4 of them.
 * ======================================================================== */
#ifndef _INGAME_MAP_MAP_HIT_CHECK_H
#define _INGAME_MAP_MAP_HIT_CHECK_H

// v1 pushed to (r + 0.01) from the edge a-b, along its XZ normal on v1's
// side, into v0; len is v1's distance to the edge. 1.
int MapHitCollisionLine(float len, float r, float *v0, float *v1, float *a, float *b);  // FF3 0x001fe3d8
// v1 pushed to (r + 0.01) from the corner a, along a -> v1 in XZ, into v0. 1.
int MapHitCollisionPoint(float len, float r, float *v0, float *v1, float *a);           // FF3 0x001fe520
// The circle r at pos pushed out of one quad, in place; 1 when it moved.
int MapHitPushOutQuad(float r, float *pos, const float *quad);                          // FF3 0x001fe5f8

// Up to 4 rounds of the circle r at pos against area's room walls (rcb kind
// 0 under mh_no) and the door / event quads of floor: 0 clear (out = pos),
// 1 pushed (out; still stuck after 4 rounds: out = old). 0 when the area
// has no room buffer.
int MrecWallPushOut(float r, float *out, float *pos, float *old, int area, short floor, int mh_no);  // FF3 0x001fe6a8
// The same against the lantern areas (rcb kind 6) of the room hit data hit:
// 0 clear (out = pos), 1 pushed (out), 2 still stuck after 4 rounds (out = old).
int MapHitTourouPushOut(float r, float *out, float *pos, float *old, void *hit, int mh_no);        // FF3 0x001fe7d8
// 1 when the line from -> to is blocked: floor has no RegDat slot at from, a
// type 0x18 rectangle of the hit list crosses it (records with id 1 only in
// mode 10), or a door / event quad of floor does (MapLBuffLineCheck).
int MapHitLineCheck(float *from, float *to, int room, short floor, int mode10);       // FF3 0x001fe8c8

#endif /* _INGAME_MAP_MAP_HIT_CHECK_H */
