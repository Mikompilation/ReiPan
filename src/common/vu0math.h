/* ==========================================================================
 *  common/vu0math.h
 *
 *  FF3's maths helpers (FF3-only object, 0x1ef370..0x1efcf0, between
 *  my_malloc.c and cpp_heap.c). FF2 spread these over utility.c (GetRandValF),
 *  unit_ctl.c (RotLimitChk, GetTrgtRot) and g3dMath.c (g3dAtan2f, on libm);
 *  FF3 gathers them and replaces libm's sin/cos/atan/acos with short VU0
 *  macro-mode polynomials.
 *
 *  Angles are radians. The rotation helpers take the angle as a float (f12
 *  on the EE, so its place in the prototype is not observable) and the
 *  vectors as quadwords.
 * ======================================================================== */
#ifndef _COMMON_VU0MATH_H
#define _COMMON_VU0MATH_H

#include "sdk/libvu0.h"

// Rotate src about one axis by rot (normalised first) into dst.
void  RotVectorX(float rot, float *dst, float *src);
void  RotVectorY(float rot, float *dst, float *src);
void  RotVectorZ(float rot, float *dst, float *src);
// Copies in to out, then Z, X and Y by rot[2], rot[0], rot[1] (each of
// them rotates `in`: see vu0math.c).
void  RotVectorZXY(float *out, float *in, float *rot);

float NormalizeAngle(float fAngle);     // into (-2pi, 2pi), sign kept
float Vu0Atanf(float x);
float g3dAtan2f(float fX, float fY);    // atan2(fX, fY), in (-pi, pi]
float Vu0Sinf(float x);
float Vu0Cosf(float x);
float Vu0Acosf(float x);
float LogBasef(float base, float x);    // log_base(x); no callers
int   IntLogBase(int base, int n);      // k with base^k == n

// Cubic curves through quadwords; no callers in the retail ELF.
void  HermiteVector(float t, float *out, float *p0, float *p1, float *m0, float *m1);
void  BSplineVector(float t, float *out, float *p0, float *p1, float *p2, float *p3);

float GetRandValF(float max);           // [0, max]
int   GetRandValIByF(int max);          // (int)GetRandValF(max)

void  RotLimitChk(float *rot);          // wraps one step into [-pi, pi]
// Rotation that turns p0 towards p1: id bit 1 -> pitch into rot[0],
// bit 2 -> yaw into rot[1]; each wrapped by RotLimitChk.
void  GetTrgtRot(float *p0, float *p1, float *rot, int id);

#endif /* _COMMON_VU0MATH_H */
