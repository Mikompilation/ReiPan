/* ==========================================================================
 *  ingame/plyr/unit_ctl.h
 *
 *  FF3 0x25e9e0..0x25fd34 (one object; its GPhaseCtrl atexit thunk 0x25fcc0
 *  ends it):
 *    - FF2 unit_ctl.c's angle helpers in FF2 order (UnitGetTrgtRot,
 *      GetTrgtRotY, RotRngChk, OutSightChk, ConvertRot2Dir) with two new
 *      point helpers (GetMidPos, GetRndRingPos); FF2's RotLimitChk and the
 *      non-clearing GetTrgtRot moved to common/vu0math.c;
 *    - FF3's stick readers (GetStickRaw and the angle / direction built on
 *      it), which honour the option work's control settings;
 *    - the video-mode select: phase 6 FrameRateSel_Main (the E_SETTINGS_PHF
 *      layout) and its child 18 Title_FrameRate_Sel (cursor 0x12 over 480p /
 *      PAL / NTSC, applied at once and kept only when confirmed within 15 s).
 *
 *  Angles are radians in (-pi, pi], 0 along +z, positive towards +x.
 * ======================================================================== */
#ifndef _INGAME_PLYR_UNIT_CTL_H
#define _INGAME_PLYR_UNIT_CTL_H

#include "sdk/scetypes.h"

// ---- angles and points ----

// Rotation that turns p0 towards p1: rot is cleared, then id bit 1 puts the
// pitch into rot[0], bit 2 the yaw into rot[1], each wrapped by RotLimitChk.
// (vu0math.c's GetTrgtRot is the same without the clear.)
void   UnitGetTrgtRot(float *p0, float *p1, float *rot, u_char id);    // FF3 0x0025e9e0
float  GetTrgtRotY(float *p0, float *p1);           // FF3 0x0025eab8: yaw that turns p0 towards p1
// Non-zero when tp lies within +/- rng of the heading rot seen from vp.
int    RotRngChk(float *vp, float *tp, float rot, float rng);          // FF3 0x0025eae8
// Non-zero when tp is OUTSIDE vp's view: further than dist on the floor plane
// (0 = no limit) or outside the cone of full width sight around rot.
int    OutSightChk(float *tp, float *vp, float rot, float sight, float dist);  // FF3 0x0025eb40
void   GetMidPos(float *out, float *a, float *b);   // FF3 0x0025ebc8: (a + b) / 2, w = 1
// Heading to a direction index, 0 = ahead, clockwise. id 0: four ways, 1:
// eight ways, 2: two ways (0 ahead, 1 behind), else 0 for rot >= 0, 1 below.
u_char ConvertRot2Dir(float rot, u_char id);        // FF3 0x0025ec28
// A random point around base on the floor plane, min..max away. No callers.
void   GetRndRingPos(float *out, float *base, float max, float min);   // FF3 0x0025edc0
float  GetLength2D(float x, float y);               // FF3 0x0025ee70: sqrt(x * x + y * y)
// 1 when pos is in front of the camera (within 300, half-angle pi / 2) and
// projects into the 640 x 448 screen.
int    IsPosInView(float *pos);                     // FF3 0x0025ee98

// ---- sticks ----

// Two stick bytes, 0x80 = centre (x grows right, y grows down), by mode:
//   0, 1  x only (y = 0): the left stick, the right one with option b13, or
//         with option b12 == 0 the d-pad left / right pressures
//   2     left stick                 4  right stick
//   5     the look stick: the left one, the right one with b13, or with
//         b12 == 1 the d-pad pressures; y inverted with option b09 == 1
//   6     right stick, the left one with b13
//   3, 7.. nothing written
// (Axes as PadGetAnalogRaw numbers them: 0/1 right x/y, 2/3 left x/y.)
void   GetStickRaw(u_char mode, u_char *x, u_char *y);   // FF3 0x0025ef78
// The stick's angle (0 = up, clockwise), or GET_STICK_ROT_NONE when it is
// within 50 of the centre.
float  GetStickRot(u_char mode);                    // FF3 0x0025f410
u_char GetStickDir8(u_char mode);                   // FF3 0x0025f4d8: ConvertRot2Dir(.., 1), 0xff centred
#define GET_STICK_ROT_NONE  10.0f                   // sdata 0x372794 / 0x37279c

// Pressed this frame on pad 0: dir 0 left, 1 right (d-pad, or with option
// b12 the stick b13 picks), 3 the left stick's down; else 0.
int    GetDirTrg(u_char dir);                       // FF3 0x0025f268

// ---- phases 6 FrameRateSel_Main and 18 Title_FrameRate_Sel (GPhase tables) ----

int  init_FrameRateSel_Main(void);
int  one_FrameRateSel_Main(void);
int  end_FrameRateSel_Main(void);
void pre_FrameRateSel_Main(void);
void after_FrameRateSel_Main(void);
int  init_Title_FrameRate_Sel(void);
int  one_Title_FrameRate_Sel(void);
int  end_Title_FrameRate_Sel(void);

void FrameRateSelDummy(void);           // FF3 0x0025fcb8: empty, no references

#endif /* _INGAME_PLYR_UNIT_CTL_H */
