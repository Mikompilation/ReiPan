/* ==========================================================================
 *  ingame/camera/cam_rail.h
 *
 *  The rail camera, the mode-10 stance camera and the XZ geometry they
 *  share (FF3-only, 0x137cb8..0x139e10; file name invented after
 *  cam_rail.ecb).
 *
 *  Rail camera: rectangle camera types >= 100 (MrecGetCameraType) are
 *  rails, polylines of points in 3ddata/camera/cam_rail.ecb (file 0x28,
 *  loaded for the whole in-game by CamRailDataLoad). CamRailStart takes the
 *  rail of (area, type), CamRailMain keeps the camera on the rail at a
 *  distance from the player (the row's radius plus a distance that eases
 *  between the row's front / back values as the player turns toward or
 *  away from the camera), turns it to the player plus the row's target
 *  offset and lets that point stray up to the row's screen radius from the
 *  screen centre before following it. Rows with the lock byte park the
 *  camera on a row point until the player walks away.
 *
 *  Stance camera: in mode 10 (PlyrMode10Start's stance) CameraMain runs
 *  CamStanceMain: the eye circles the player's joint 15 at radius 13,
 *  turned by left / right, pulled in front of walls, the target raised or
 *  lowered by up / down.
 * ======================================================================== */
#ifndef _INGAME_CAMERA_CAM_RAIL_H
#define _INGAME_CAMERA_CAM_RAIL_H

#include <stddef.h>                     // offsetof
#include "sdk/libvu0.h"                 // sceVu0FVECTOR
#include "sdk/scetypes.h"               // u_char, s_char

// CAM_RAIL_DAT.kind: a rail is a run of rows from a FIRST to the row
// before a LAST (the LAST row's point is not copied, CamRailStart).
enum CAM_RAIL_KIND
{
    CAM_RAIL_KIND_FIRST = 0,
    CAM_RAIL_KIND_MID   = 1,
    CAM_RAIL_KIND_LAST  = 2,
};

// A row of cam_rail.ecb (ECB rows of 0x38 bytes, 110 on the disc): one
// point of a rail and the camera settings of the segment from it.
// layout: EE (file data; rows are 8-aligned, so pos / tgt_ofs are plain
// arrays)
struct CAM_RAIL_DAT                     /* 0x38 */
{
    /* 0x00 */ s_char area;             // the player's area (GetPlyrAreaNo)
    /* 0x01 */ s_char type;             // the rectangle camera type (>= 100)
    /* 0x02 */ u_char kind;             // CAM_RAIL_KIND
    /* 0x03 */ u_char lock;             // non-zero: the camera parks on a row point here (CamRailFollow)
    /* 0x04 */ float  radius;           // the camera's distance from the player along the rail
    /* 0x08 */ float  fov;
    /* 0x0c */ float  scr_r;            // how far (screen pixels) the look point may stray from the centre (180)
    /* 0x10 */ float  pos[4];           // the rail point (w 1)
    /* 0x20 */ float  tgt_ofs[4];       // the look point is the player's pos plus this
    /* 0x30 */ float  dist_front;       // the added distance once the camera stays in front of the player
    /* 0x34 */ float  dist_back;        // the added distance while it is behind
};
static_assert(sizeof(CAM_RAIL_DAT) == 0x38, "CAM_RAIL_DAT size (the ECB row size)");
static_assert(offsetof(CAM_RAIL_DAT, kind) == 0x02, "CAM_RAIL_DAT.kind");
static_assert(offsetof(CAM_RAIL_DAT, lock) == 0x03, "CAM_RAIL_DAT.lock");
static_assert(offsetof(CAM_RAIL_DAT, radius) == 0x04, "CAM_RAIL_DAT.radius");
static_assert(offsetof(CAM_RAIL_DAT, fov) == 0x08, "CAM_RAIL_DAT.fov");
static_assert(offsetof(CAM_RAIL_DAT, scr_r) == 0x0c, "CAM_RAIL_DAT.scr_r");
static_assert(offsetof(CAM_RAIL_DAT, pos) == 0x10, "CAM_RAIL_DAT.pos");
static_assert(offsetof(CAM_RAIL_DAT, tgt_ofs) == 0x20, "CAM_RAIL_DAT.tgt_ofs");
static_assert(offsetof(CAM_RAIL_DAT, dist_front) == 0x30, "CAM_RAIL_DAT.dist_front");
static_assert(offsetof(CAM_RAIL_DAT, dist_back) == 0x34, "CAM_RAIL_DAT.dist_back");

#define CAM_RAIL_PT_MAX 48              // the work's point array (0x300 bytes)

// The rail camera's work (data 0x2f2bf0, 0x390 bytes cleared by
// CamRailWrkClear).
// layout: host -- rec is an EE pointer at +0x310, so dist_tgt / dist sit 4
// later (the EE pads 0x31c..0x320); from latch on the offsets match the ROM
// again.
struct CAM_RAIL_WRK
{
    /* 0x000 */ int           state;    // CamRailDistCtrl: 0 / 1 behind (1: watching), 2 / 3 in front (3: watching)
    /* 0x004 */ int           pt_num;   // points in pt
    /* 0x008 */ float         fov;
    /* 0x00c */ float         fov_step; // CamRailFovStep's step; nothing sets it (0: the fov stays as started)
    /* 0x010 */ sceVu0FVECTOR pt[CAM_RAIL_PT_MAX];  // the rail's points
    /* 0x310 */ CAM_RAIL_DAT *rec;      // the rail's first row, nullptr: none (CamRailOff)
    /* 0x314 */ float         dist_tgt; // the added distance CamRailDistCtrl asks for
    /* 0x318 */ float         dist;     // the added distance, eased 0.3 a frame
    /* 0x320 */ sceVu0FVECTOR latch;    // the player's pos when the camera changed side
    /* 0x330 */ float         _330;     // 18 at the start; no reader
    /* 0x334 */ float         _334;     // 4 at the start; no reader
    /* 0x338 */ int           lock;     // 1: parked on a row point (CamRailFollow)
    /* 0x33c */ float         lock_dist;// the locked row's radius
    /* 0x340 */ int           lock_row; // the row nearest the player when it locked
    /* 0x344 */ int           cnt;      // frames the look point has been off-centre; -1: snap the direction next frame
    /* 0x350 */ sceVu0FVECTOR cam;      // the camera's position
    /* 0x360 */ sceVu0FVECTOR dir;      // its unit direction to the target
    /* 0x370 */ float         _370;
    /* 0x374 */ float         scr_r_tgt;// the row's screen radius
    /* 0x378 */ float         scr_r;    // the screen radius in use (drops 1 a frame, jumps up)
    /* 0x37c */ float         _37c;
    /* 0x380 */ int           cut;      // 1 the frame the direction snapped (CamRailGetCutFlg)
};
static_assert(offsetof(CAM_RAIL_WRK, pt_num) == 0x004, "CAM_RAIL_WRK.pt_num");
static_assert(offsetof(CAM_RAIL_WRK, fov) == 0x008, "CAM_RAIL_WRK.fov");
static_assert(offsetof(CAM_RAIL_WRK, fov_step) == 0x00c, "CAM_RAIL_WRK.fov_step");
static_assert(offsetof(CAM_RAIL_WRK, pt) == 0x010, "CAM_RAIL_WRK.pt");
static_assert(offsetof(CAM_RAIL_WRK, rec) == 0x310, "CAM_RAIL_WRK.rec");
static_assert(offsetof(CAM_RAIL_WRK, latch) == 0x320, "CAM_RAIL_WRK.latch");
static_assert(offsetof(CAM_RAIL_WRK, lock) == 0x338, "CAM_RAIL_WRK.lock");
static_assert(offsetof(CAM_RAIL_WRK, lock_dist) == 0x33c, "CAM_RAIL_WRK.lock_dist");
static_assert(offsetof(CAM_RAIL_WRK, lock_row) == 0x340, "CAM_RAIL_WRK.lock_row");
static_assert(offsetof(CAM_RAIL_WRK, cnt) == 0x344, "CAM_RAIL_WRK.cnt");
static_assert(offsetof(CAM_RAIL_WRK, cam) == 0x350, "CAM_RAIL_WRK.cam");
static_assert(offsetof(CAM_RAIL_WRK, dir) == 0x360, "CAM_RAIL_WRK.dir");
static_assert(offsetof(CAM_RAIL_WRK, scr_r_tgt) == 0x374, "CAM_RAIL_WRK.scr_r_tgt");
static_assert(offsetof(CAM_RAIL_WRK, scr_r) == 0x378, "CAM_RAIL_WRK.scr_r");
static_assert(offsetof(CAM_RAIL_WRK, cut) == 0x380, "CAM_RAIL_WRK.cut");
static_assert(sizeof(CAM_RAIL_WRK) == 0x390, "CAM_RAIL_WRK size (CamRailWrkClear clears 0x390)");

// A rail point the camera could take (two-slot lists on the stack).
// layout: EE (stack only)
struct CAM_RAIL_CAND                    /* 0x20 */
{
    /* 0x00 */ int           seg;       // the segment it lies on, -1: free slot
    /* 0x04 */ float         dist;      // its distance (3D) to the player (CamRailCandAdd) or the camera (CamRailFollowSearch)
    /* 0x08 */ int           _08[2];
    /* 0x10 */ sceVu0FVECTOR pos;
};
static_assert(sizeof(CAM_RAIL_CAND) == 0x20, "CAM_RAIL_CAND size");
static_assert(offsetof(CAM_RAIL_CAND, dist) == 0x04, "CAM_RAIL_CAND.dist");
static_assert(offsetof(CAM_RAIL_CAND, pos) == 0x10, "CAM_RAIL_CAND.pos");

// The stance camera's settings and state (data 0x2f2b90, 0x60 bytes).
// layout: EE (ROM data)
struct CAM_STANCE                       /* 0x60 */
{
    /* 0x00 */ float         height;    // -24: the eye's height over the player's pos (y is down)
    /* 0x04 */ float         _04[3];
    /* 0x10 */ sceVu0FVECTOR _10;       // (0, -15, 0, 1); no reader
    /* 0x20 */ float         radius;    // 13: the eye's circle about the base
    /* 0x24 */ float         _24;       // 25; no reader
    /* 0x28 */ float         yaw_step;  // 0.005 turns a frame (left / right)
    /* 0x2c */ float         yaw;       // the eye's place on the circle, in turns [0, 1]
    /* 0x30 */ float         pitch;     // the target's pitch, [-0.58, 0]
    /* 0x34 */ float         pitch_step;// 0.01 a frame (up / down)
    /* 0x38 */ float         dist_old;  // last frame's XZ eye-base distance
    /* 0x3c */ float         cut_dist;  // 7: a jump of the distance this large cross-fades
    /* 0x40 */ int           eff;       // 12: SetEffects(6, 1, eff) every frame
    /* 0x44 */ int           _44[3];
    /* 0x50 */ sceVu0FVECTOR base;      // the player's joint 15 (while mode10_time runs)
};
static_assert(sizeof(CAM_STANCE) == 0x60, "CAM_STANCE size (data 0x2f2b90..0x2f2bf0)");
static_assert(offsetof(CAM_STANCE, _10) == 0x10, "CAM_STANCE._10");
static_assert(offsetof(CAM_STANCE, radius) == 0x20, "CAM_STANCE.radius");
static_assert(offsetof(CAM_STANCE, yaw_step) == 0x28, "CAM_STANCE.yaw_step");
static_assert(offsetof(CAM_STANCE, yaw) == 0x2c, "CAM_STANCE.yaw");
static_assert(offsetof(CAM_STANCE, pitch) == 0x30, "CAM_STANCE.pitch");
static_assert(offsetof(CAM_STANCE, pitch_step) == 0x34, "CAM_STANCE.pitch_step");
static_assert(offsetof(CAM_STANCE, dist_old) == 0x38, "CAM_STANCE.dist_old");
static_assert(offsetof(CAM_STANCE, cut_dist) == 0x3c, "CAM_STANCE.cut_dist");
static_assert(offsetof(CAM_STANCE, eff) == 0x40, "CAM_STANCE.eff");
static_assert(offsetof(CAM_STANCE, base) == 0x50, "CAM_STANCE.base");

// The block CamStanceWallHit hands CamStanceQuadHit (0x40 on the stack).
// layout: EE (stack only)
struct CAM_STANCE_HIT                   /* 0x40 */
{
    /* 0x00 */ sceVu0FVECTOR pos;       // the nearest crossing so far
    /* 0x10 */ sceVu0FVECTOR p0;        // the segment tested
    /* 0x20 */ sceVu0FVECTOR p1;
    /* 0x30 */ float         dist;      // pos's XZ distance to the stance base
    /* 0x34 */ int           found;
};
static_assert(sizeof(CAM_STANCE_HIT) == 0x40, "CAM_STANCE_HIT size (CamStanceWallHit clears 0x40)");
static_assert(offsetof(CAM_STANCE_HIT, p0) == 0x10, "CAM_STANCE_HIT.p0");
static_assert(offsetof(CAM_STANCE_HIT, p1) == 0x20, "CAM_STANCE_HIT.p1");
static_assert(offsetof(CAM_STANCE_HIT, dist) == 0x30, "CAM_STANCE_HIT.dist");
static_assert(offsetof(CAM_STANCE_HIT, found) == 0x34, "CAM_STANCE_HIT.found");

// ---- cam_rail.ecb (IngameInit / IngameEnd) ------------------------------
void CamRailDataLoad(void);             // FF3 0x00137d50: CAM_RAIL_ECB into the system heap
void CamRailDataFree(void);             // FF3 0x00137da0
void CamRailWrkClear(void);             // FF3 0x00139b58

// ---- the rail camera (CameraMain, MapCamRectangleCameraCtrl) ------------
void CamRailOff(void);                  // FF3 0x00139b80: drops the rail; the next CamRailStart restarts it
void CamRailStart(int area, int type);  // FF3 0x00139ba0: takes the rail of (area, type) unless it is the current one
void CamRailMain(void);                 // FF3 0x00139cd8: moves and applies the rail camera
int  CamRailGetCutFlg(void);            // FF3 0x00139b90: 1 the frame its direction snapped (PlyrMapCamCheck bit 30)

// ---- the stance camera (mode 10) ----------------------------------------
void   CamStanceInit(float rot);        // FF3 0x00138800: PlyrMode10Start, rot the player's facing
void   CamStanceMain(void);             // FF3 0x00138ab0: CameraMain while mode 10 starts
float *CamStanceGetTarget(void);        // FF3 0x00138d18: the point it looks at (PlyrViewRate, PlyrLookSearchPoint)
void   CamStanceSetBase(float *pos);    // FF3 0x00138490: no callers

// ---- XZ geometry (the map code uses them too) ---------------------------
// Segments a0-a1 and b0-b1 cross in XZ: 1 and out = a0 + (a1 - a0) t.
int  CamSegCrossXZ(float *out, const float *a0, const float *a1, const float *b0, const float *b1);  // FF3 0x00137e70
// out = center + (cos, 0, sin)(turn * 2pi) * r with w 1 added to center's.
void CamCirclePos(float r, float turn, float *out, float *center);    // FF3 0x00138038

#endif /* _INGAME_CAMERA_CAM_RAIL_H */
