/* ==========================================================================
 *  graphics/effect/effect_sub.h
 *
 *  Effect helpers (FF2 effect_sub.c + effect_spr.c's sprite pair; FF3
 *  0x1806a8..0x183d58 and the out-of-line bodies 0x2b8220..0x2b8ce0).
 *  Angles are radians; vectors are quadwords.
 * ======================================================================== */
#ifndef _GRAPHICS_EFFECT_EFFECT_SUB_H
#define _GRAPHICS_EFFECT_EFFECT_SUB_H

#include "sdk/scetypes.h"
#include "sdk/libvu0.h"

#include <stddef.h>                     // offsetof

struct SPRT_DAT;
struct DISP_SPRT2;
struct SGDFILEHEADER;
class  ModelObject;

// ---- distances and directions ----

// |v2 - v1| over x and z only (FF3's Get2PLength drops y; FF2's summed all three).
float Get2PLength(const float *v1, const float *v2);                       // FF3 0x001806a8
// |v1 - v2| over x, y and z.
float Get2PDist3D(const float *v1, const float *v2);                       // FF3 0x001806d8
float Get2PDist3D2(const float *v1, const float *v2);                      // FF3 0x00180dd0: the same body
// A world point to screen pixels (0..640, 0..448) through the camera.
void  WorldToScreenPos(float *pos, float *sx, float *sy);                  // FF3 0x00180710
// The X / Y rotation that points +Z along dir (both 0 for a zero vector).
void  Vector2Rot(const float *dir, float *x, float *y);                    // FF3 0x001808d0
// Vector2Rot() of (v2 - v1): the X / Y rotation that turns v1 to face v2.
void  Get2PosRot(const float *v1, const float *v2, float *x, float *y);    // FF3 0x00180a40
// z = atan2(dx, dy), x = atan2(dz, dy): lays an object along v1 -> v2.
void  Get2PosRot2(const float *v1, const float *v2, float *x, float *z);   // FF3 0x00180a78
// rot = 0; id bit 0: rot[0] = pitch, bit 1: rot[1] = heading from p0 to p1.
void  GetTrgtRotType2Neg(float *p0, float *p1, float *rot, int id);        // FF3 0x00180ba8: pitch negated, wrapped
void  GetTrgtRotType2(float *p0, float *p1, float *rot, int id);           // FF3 0x00180ce8
int   CamChangeCheck(void);                                                // FF3 0x00180e08: always 0
// rot = the X / Y rotation turning pos toward the camera, rot[2] = rot[3] = 0.
void  GetCamFaceRot(float *rot, float *pos);                               // FF3 0x00180e10

// ---- screen sprites of the effect page ----

// A 2w x 2h sprite of effdat[n + mono] around the GS-window point v (x 16,
// int); tp: bit 0 ZMSK, bit 1 additive, bit 2 B = 0 (sets its own regs).
void SetEffSQITex(int n, int *v, int tp, float w, float h,
                  u_char r, u_char g, u_char b, u_char a);                 // FF3 0x00180e60
// The same around the float GS-window point pos (pixels); dropped outside the
// guard band; draws with the caller's registers (tp unused).
void SetEffSQTex(int n, float *pos, int tp, float w, float h,
                 u_char r, u_char g, u_char b, u_char a);                  // FF3 0x00181128

// ---- 3D sprites ----

// A w x h quad of effdat[n + mono] through the local->world matrix mat;
// regs: TEX1, ALPHA, ZBUF, TEST, CLAMP values and the strip's GIF tag.
void Set3DPosTexure(float w, float h, sceVu0FMATRIX mat, u_long *regs, int n,
                    u_char r, u_char g, u_char b, u_char a);               // FF3 0x00181868
// The same with the mono offset passed in (FF2 Set3DPosTexure2).
void Set3DPosTexure2(float w, float h, sceVu0FMATRIX mat, u_long *regs, int n,
                     u_char r, u_char g, u_char b, u_char a, int mono);    // FF3 0x001818a8
// The same with the sprite record given.
void Set3DPosTexureSpr(float w, float h, sceVu0FMATRIX mat, u_long *regs, SPRT_DAT *sprt,
                       u_char r, u_char g, u_char b, u_char a, int mono);  // FF3 0x001818e0
// The shared body (out of line at 0x2b8220).
void Set3DPosTexureSub(float w, float h, sceVu0FMATRIX mat, u_long *regs, SPRT_DAT *sprt,
                       u_char r, u_char g, u_char b, u_char a, int mono);  // FF3 0x002b8220

// 1 when pos lies in the current player's light cone (40 degrees about the
// facing, closer than dist); off: the off-axis ratio (1 at the cone's edge),
// depth: the distance ratio along the axis.
int  EffPlyrLightConeCheck(float dist, float *pos, float *off, float *depth);   // FF3 0x00181420
// EffPlyrLightConeCheck without the ratios.
int  EffPlyrLightConeIn(float dist, float *pos);                           // FF3 0x00181400

// ---- the spotlight falloff (no callers in the retail ELF) ----

// The cone: inner / outer half-angles (outer clamped to [0, pi/2]), two
// unread values, the apex and the axis (normalised).
void  EffSpotLightSet(float inner, float outer, float p2, float p3,
                      float *pos, float *dir);                             // FF3 0x00181698
// 1 inside the inner cone, 0 outside the outer one, linear in the cosine between.
float EffSpotLightRate(float *point);                                      // FF3 0x00181750

// ---- 3D debug primitives (no callers in the retail ELF) ----

// A gouraud line p0 -> p1 (colours 0..0x80), depth-tested / not.
void EffDrawLine3DZ(float *p0, u_char r0, u_char g0, u_char b0, u_char a0,
                    float *p1, u_char r1, u_char g1, u_char b1, u_char a1);   // FF3 0x00181908
void EffDrawLine3D(float *p0, u_char r0, u_char g0, u_char b0, u_char a0,
                   float *p1, u_char r1, u_char g1, u_char b1, u_char a1);    // FF3 0x001819f8
// Their shared body: the line through matWorldScreen, dropped off the GS window.
void EffDrawLine3DSub(float *p0, u_char r0, u_char g0, u_char b0, u_char a0,
                      float *p1, u_char r1, u_char g1, u_char b1, u_char a1); // FF3 0x002b8698
// The 12 edges of the box min..max (depth-tested).
void EffDrawBox3D(float *min, float *max, u_char r, u_char g, u_char b, u_char a);  // FF3 0x00181af0
// A line from p0 to p1 with a two-stroke head at p1 (+-30 degrees about y).
void EffDrawArrow3D(float *p0, float *p1, u_char r, u_char g, u_char b, u_char a);  // FF3 0x00181f28
// A circle of radius about centre in the XZ plane, div segments; spokes:
// also a line from the centre to every point.
void EffDrawCircleXZ(float radius, float *centre, u_char r, u_char g, u_char b, u_char a,
                     int spokes, int div);                                 // FF3 0x00182090

// ---- depth probes ----

// A batch of "is this world point in front of what the camera sees?"
// queries (FF2 effect_sub.h PP_JUDGE): the caller fills p[0..num-1];
// CheckPointDepth writes result[i], non-zero when nothing covers the point.
struct PP_JUDGE                         /* 0xb0 */
{
    /* 0x00 */ u_char        num;
    /* 0x01 */ u_char        result[10];
    /* 0x0b */ u_char        _0b[5];
    /* 0x10 */ sceVu0FVECTOR p[10];
};
static_assert(sizeof(PP_JUDGE) == 0xb0, "PP_JUDGE is 0xb0 bytes on the PS2");
static_assert(offsetof(PP_JUDGE, result) == 0x01, "PP_JUDGE.result");
static_assert(offsetof(PP_JUDGE, p) == 0x10, "PP_JUDGE.p");

// One point: 1 when it is on screen and nearer than the Z buffer there
// (no callers).
int  EffPointIsVisible(float *pos);                                         // FF3 0x00182220
// FF2 CheckPointDepth: every point of ppj against the Z buffer.
void CheckPointDepth(PP_JUDGE *ppj, int probe_slot = 0);                                       // FF3 0x00182428

// ---- sprites through a matrix ----

// Every material of sgd gets the alpha (vDiffuse.w).
void EffSgdSetMaterialAlpha(SGDFILEHEADER *sgd, u_char alpha);            // FF3 0x001832c8
// A 2w x 2h sprite of sprt at pos, turned with the camera's heading and by
// rot about its own z.
void EffCamYawSprite(float rot, float w, float h, SPRT_DAT *sprt, float *pos,
                     u_char r, u_char g, u_char b, u_char a);              // FF3 0x00183308
// A 2w x 2h sprite of sprt at pos, turned to face the camera (GetCamFaceRot;
// rot is not read, as in the ROM).
void Eff3DFaceSprite(float rot, float w, float h, SPRT_DAT *sprt, float *pos,
                     u_char r, u_char g, u_char b, u_char a);              // FF3 0x001834f8
// The four corners v of sprt through the local -> world matrix m (no callers).
void EffMatSprite(SPRT_DAT *sprt, sceVu0FVECTOR *v, sceVu0FMATRIX m,
                  u_char r, u_char g, u_char b, u_char a);                 // FF3 0x00183700
// Four vertices v through the local-world matrix m and the camera as one
// textured strip (TEX0 already set by the caller; tex0 gives its size): uv
// the texel rect (u, v, w, h), col the RGBA words.
void EffMatTexQuad(u_long tex0, sceVu0FMATRIX m, sceVu0FVECTOR *v, int *uv, int *col);   // FF3 0x00183800
// The shared sprite body of the three above: 1 when it was sent (PC: the
// callers' host draw follows it).
int  EffSpriteSub(SPRT_DAT *sprt, sceVu0FMATRIX slm, sceVu0FVECTOR *v,
                  u_char r, u_char g, u_char b, u_char a);                 // FF3 0x002b8a38
// DispSprD of sprt at (x, y) scaled about its centre, with the alpha-test
// reference aref, the colour words col, pri and ALPHA_1; fbp is not read.
void EffDispSprCentreScale(float scw, float sch, int fbp, SPRT_DAT *sprt, u_char aref,
                           int x, int y, int *col, u_int pri, u_long alpha);   // FF3 0x00183af8
// Save / restore ALPHA_1, TEX1_1, CLAMP_1, TEST_1, ZBUF_1 (no callers).
void EffGsRegsSave(void);                                                  // FF3 0x00183c38
void EffGsRegsRestore(void);                                               // FF3 0x00183ca0
// 1 when obj is null or its model's bounding box is out of view (no callers).
int  EffModelIsOutOfView(ModelObject *obj);                                // FF3 0x00183d00

// ---- frame-buffer passes ----

// FRAME_1 / XYOFFSET_1 / SCISSOR_1 at a w x h work frame at fbp (TBP units).
void EffSetDrawFrame(int fbp, int psm, int w, int h);                      // FF3 0x00182678
// FRAME_1 back at the draw buffer, full-screen scissor.
void EffResetDrawFrame(void);                                              // FF3 0x00182760
// The work frame at fbp (320 x 448), then an untextured w x h sprite at (x, y).
void EffDrawFrameFillRect(int fbp, int psm, int x, int y, int w, int h,
                          u_char r, u_char g, u_char b, u_char a);         // FF3 0x00182828
// DispSprD2 sent as DIRECT 64-pixel column strips (SCISSOR_1 per strip).
void EffDispSprD2Direct(DISP_SPRT2 *s);                                    // FF3 0x00182930
// Halves a 32-bit picture's width in place: every second pixel, or with
// blend the average of each pair.
void EffImageHalf32(u_int *image, u_int width, u_int height, int blend);   // FF3 0x00183198

// PC: while EffSetDrawFrame points the GS at a work frame, the host has no
// such target: the 2D bridges drop their quads (g2d_draw.c asks), and a TEX0
// naming a work frame (only GS-side passes wrote it) is not drawn either --
// while its pass keeps running: the mark lapses 2 frames after the last write,
// since the pages are shared (0x2bc0 is also the movie's).
int  EffHostDrawFrameRedirected(void);
int  EffHostTexIsWorkFrame(u_long tex0);
// PC (not FF3): the same bookkeeping for frame switches FF3 sends as raw
// FRAME_1 packets instead of EffSetDrawFrame / EffResetDrawFrame (the enemy
// aura's EneAuraFrameBegin / End, effect_ene.c); fbp in TBP units.
void ReiPan_EffHostRedirectBegin(int fbp);
void ReiPan_EffHostRedirectEnd(void);
// PC: a real work frame on the host for GS block fbp (the renderer's
// ReiPan_RendererBeginWorkFrame): the draws up to End go into it, the 2D
// bridges included, and a texture naming fbp is served from it. Begin
// returns 0, redirecting nothing, when none can be opened this frame.
int  ReiPan_EffHostWorkBegin(int fbp, int logical_w, int logical_h, int image_space);
void ReiPan_EffHostWorkEnd(void);
// PC: drop every host 2D draw and mark no work frame until End -- for a GS
// chain the host replaces wholesale (the room glow), whose dead work frames
// would otherwise hide the frame copies kept at the same addresses.
void ReiPan_EffHostMuteBegin(void);
void ReiPan_EffHostMuteEnd(void);

// PC (not FF3): with the Debug menu's "Player light cone" on, the player's
// light cone, its reach, a box and the facing drawn with the debug
// primitives above; called at the end of EffectDrawMain.
void ReiPan_EffDebugOverlay(void);

#endif /* _GRAPHICS_EFFECT_EFFECT_SUB_H */
