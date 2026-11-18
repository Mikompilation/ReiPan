/* ==========================================================================
 *  ingame/camera/map_camera.h
 *
 *  Story-mode camera controller (FF2 map_camera.c, FF3 0x1fada8..0x1fd9c0).
 *  CameraMain() is the per-frame entry IngameMain calls; the rest are the
 *  requests other code raises against it: the event camera hand-off, the
 *  approach / damage / death framing, the finder overlap, the quake shake
 *  and the "fin" camera.
 * ======================================================================== */
#ifndef _INGAME_CAMERA_MAP_CAMERA_H
#define _INGAME_CAMERA_MAP_CAMERA_H

#include <stddef.h>                     // offsetof
#include "sdk/scetypes.h"

struct ENE_WRK;                         // ingame/enemy/ene_main.h

// The current map camera's record (MapCamGetCurData, sbss 0x3f3260): FF2's
// MDAT_CAM for rectangle types 1 / 8, an MDAT_CAM_SP (ingame/map/RegDat.h)
// for type 9. Only the words IngameSetDrawEnable and PlyrMapCamCheck read
// are named: types 1 and 8 keep the draw-enable words at +0x30, type 9 at
// +0xa0. Size unknown.
// layout: EE (file data)
struct MAP_CAM_DATA
{
    /* 0x00 */ u_char _00[4];
    /* 0x04 */ u_int  word;           // PlyrMapCamCheck: a change (low 30 bits) restarts the player's camera timer
    /* 0x08 */ u_char _08[0x28];
    /* 0x30 */ int    draw_en0;
    /* 0x34 */ int    draw_en1;
    /* 0x38 */ u_char _38[0x68];
    /* 0xa0 */ int    draw_en0_t9;
    /* 0xa4 */ int    draw_en1_t9;
};
static_assert(offsetof(MAP_CAM_DATA, word) == 0x04, "MAP_CAM_DATA.word");
static_assert(offsetof(MAP_CAM_DATA, draw_en0) == 0x30, "MAP_CAM_DATA.draw_en0");
static_assert(offsetof(MAP_CAM_DATA, draw_en1) == 0x34, "MAP_CAM_DATA.draw_en1");
static_assert(offsetof(MAP_CAM_DATA, draw_en0_t9) == 0xa0, "MAP_CAM_DATA.draw_en0_t9");
static_assert(offsetof(MAP_CAM_DATA, draw_en1_t9) == 0xa4, "MAP_CAM_DATA.draw_en1_t9");

// APPROACH_CAMERA.flow: the approach camera's state machine. 0..7 approach
// and damage, 10..11 talk, 20..21 death; each group is entered by its
// ReqPlyr*CameraCtrl request (talk has no requester in FF3).
enum APPROACH_CAMERA_FLOW
{
    APPROACH_CAMERA_FLOW_INIT        = 0,   // ReqPlyrApproachCameraCtrl: place the slide
    APPROACH_CAMERA_FLOW_MOVE        = 1,   // slide npos for cnt frames
    APPROACH_CAMERA_FLOW_FINDER_OFF  = 2,   // drop the viewfinder, then 3
    APPROACH_CAMERA_FLOW_KEEP        = 3,   // follow the map camera (and the damage framing while the enemy acts)
    APPROACH_CAMERA_FLOW_MAP         = 4,   // the map camera alone
    APPROACH_CAMERA_FLOW_SHOT        = 5,   // the damage camera shots (DmgCamShotMain) while the player is hit
    APPROACH_CAMERA_FLOW_DAMAGE_INIT = 6,   // ReqPlyrDamageCameraCtrl
    APPROACH_CAMERA_FLOW_DAMAGE      = 7,   // ease into the damage framing for cnt frames
    APPROACH_CAMERA_FLOW_TALK_INIT   = 10,
    APPROACH_CAMERA_FLOW_TALK_KEEP   = 11,
    APPROACH_CAMERA_FLOW_DEAD_INIT   = 20,  // ReqPlyrDeadCameraCtrl
    APPROACH_CAMERA_FLOW_DEAD_KEEP   = 21
};

// The approach camera (bss 0x3f3270, 0xd0 bytes cleared by CameraMainInit).
// FF2's APPROACH_CAMERA grown by FF3's damage framing (dist2, tstart, mpos,
// the two followed points). The damage camera shots (DmgCamShotInit /
// DmgCamShotMain, dmg_cam_shot.c) take it and write PosInit / NeckInit.
// layout: host -- pEneWrk, pPlyrPos and pEnePos are EE pointers at +0xc0,
// +0xc4, +0xc8; the offsets before them match the ROM.
struct APPROACH_CAMERA                  /* 0xd0 on the EE */
{
    /* 0x00 */ int           mode;          // 0 off (the map camera), 1 on (CameraMain runs PlyrApproachCameraCtrl)
    /* 0x04 */ int           crossfade;     // ApproachCameraCrossFadeSW / GetApproachCameraCrossFade (the overlap effect)
    /* 0x08 */ float         cnt;           // frames left of a slide (flows 1, 7) or frames in the shots (5)
    /* 0x0c */ int           flow;          // APPROACH_CAMERA_FLOW
    /* 0x10 */ float         offy;          // the look point sits offy above ipos (y is down)
    /* 0x14 */ float         offy2;         // the death camera's eye height over ipos
    /* 0x18 */ float         dist;          // eye distance at the end of the slide
    /* 0x1c */ float         dist2;         // eye distance at its start (damage requests set it; the approach request leaves it)
    /* 0x20 */ sceVu0FVECTOR npos;          // the eye
    /* 0x30 */ sceVu0FVECTOR pmv;           // eye step per frame
    /* 0x40 */ sceVu0FVECTOR ipos;          // the point of interest
    /* 0x50 */ sceVu0FVECTOR inpos;         // the target
    /* 0x60 */ sceVu0FVECTOR imv;           // target step per frame
    /* 0x70 */ sceVu0FVECTOR tstart;        // the damage framing's first target (ipos raised by offy)
    /* 0x80 */ sceVu0FVECTOR mpos;          // the map camera's eye this frame (the damage framing's direction)
    /* 0x90 */ sceVu0FVECTOR trot;          // the death camera's facing
    /* 0xa0 */ sceVu0FVECTOR NeckInitVector;// the shoulder camera's target (the enemy's mpos at the request)
    /* 0xb0 */ sceVu0FVECTOR PosInitVector; // the shoulder camera's eye
    /* 0xc0 */ ENE_WRK      *pEneWrk;       // the attacker, nullptr: no damage framing
    /* 0xc4 */ float        *pPlyrPos;      // &plyr->pos (ReqPlyrDamageCameraCtrl)
    /* 0xc8 */ float        *pEnePos;       // &ene->pos
};
static_assert(offsetof(APPROACH_CAMERA, crossfade) == 0x04, "APPROACH_CAMERA.crossfade");
static_assert(offsetof(APPROACH_CAMERA, cnt) == 0x08, "APPROACH_CAMERA.cnt");
static_assert(offsetof(APPROACH_CAMERA, flow) == 0x0c, "APPROACH_CAMERA.flow");
static_assert(offsetof(APPROACH_CAMERA, offy) == 0x10, "APPROACH_CAMERA.offy");
static_assert(offsetof(APPROACH_CAMERA, offy2) == 0x14, "APPROACH_CAMERA.offy2");
static_assert(offsetof(APPROACH_CAMERA, dist) == 0x18, "APPROACH_CAMERA.dist");
static_assert(offsetof(APPROACH_CAMERA, dist2) == 0x1c, "APPROACH_CAMERA.dist2");
static_assert(offsetof(APPROACH_CAMERA, npos) == 0x20, "APPROACH_CAMERA.npos");
static_assert(offsetof(APPROACH_CAMERA, pmv) == 0x30, "APPROACH_CAMERA.pmv");
static_assert(offsetof(APPROACH_CAMERA, ipos) == 0x40, "APPROACH_CAMERA.ipos");
static_assert(offsetof(APPROACH_CAMERA, inpos) == 0x50, "APPROACH_CAMERA.inpos");
static_assert(offsetof(APPROACH_CAMERA, imv) == 0x60, "APPROACH_CAMERA.imv");
static_assert(offsetof(APPROACH_CAMERA, tstart) == 0x70, "APPROACH_CAMERA.tstart");
static_assert(offsetof(APPROACH_CAMERA, mpos) == 0x80, "APPROACH_CAMERA.mpos");
static_assert(offsetof(APPROACH_CAMERA, trot) == 0x90, "APPROACH_CAMERA.trot");
static_assert(offsetof(APPROACH_CAMERA, NeckInitVector) == 0xa0, "APPROACH_CAMERA.NeckInitVector");
static_assert(offsetof(APPROACH_CAMERA, PosInitVector) == 0xb0, "APPROACH_CAMERA.PosInitVector");
static_assert(offsetof(APPROACH_CAMERA, pEneWrk) == 0xc0, "APPROACH_CAMERA.pEneWrk");

// ---- frame -----------------------------------------------------------
void CameraMainInit(void);              // FF3 0x001faf98
int  CameraMain(void);                  // FF3 0x001fadc8: always 0 (IngameMain sets the sound listener unless it returns 1)

// ---- the current rectangle camera --------------------------------------
MAP_CAM_DATA *MapCamGetCurData(void);   // FF3 0x001fadb8: nullptr: no camera record
int           MapCamGetCurType(void);   // FF3 0x001fada8: its rectangle type, -1: none

// ---- event camera hand-off ---------------------------------------------
void MapCamSetEventCameraFlg(int flg);  // FF3 0x001faff8
int  MapCamGetEventCameraFlg(void);     // FF3 0x001fb030: bit 29 of PlyrMapCamCheck's word
// The map-data camera (event / rectangle camera); also run by the tattoo
// event's way back (TattooEvCameraMain state 11) to learn where to end.
void MapCamNormalCameraCtrl(int RectCamFlg);    // FF3 0x001fb040

// Fills pos with the current player's position; always 0 (obj_id unused).
int  MapCamGetObjPosition(float *pos, int obj_id);  // FF3 0x001fc618

// ---- room-offset aware camera setters ----------------------------------
// Map data stores camera points relative to the room origin; these add the
// player's area buffer origin. room is unused (FF2's too).
void MapCamGra3dcamSetPositionAddOffset(float *vec, int room);  // FF3 0x001fc6a8
void MapCamGra3dcamSetTargetAddOffset(float *vec, int room);    // FF3 0x001fc718
// Follow with slack: move only once further than margin from the point.
void MapCamGra3dcamSetPositionMargin(float margin, float *pos, float *now);  // FF3 0x001fc788
void MapCamGra3dcamSetTargetMargin(float margin, float *pos, float *now);    // FF3 0x001fc7c8

// ---- "fin" camera ------------------------------------------------------
// Pins the eye at pos looking at *target; the pointer is kept and re-aimed at
// every frame until MapCamCutFinCamera.
void MapCamSetFinCamera(float *pos, float *target); // FF3 0x001fc8e8
void MapCamCutFinCamera(void);                      // FF3 0x001fc930

// ---- approach camera requests ------------------------------------------
void ReqPlyrApproachCameraCtrl(float offy, float dist, float *ipos);                          // FF3 0x001fc940
void ReqPlyrDamageCameraCtrl(float offy, float dist2, float dist, float *ipos, ENE_WRK *ene);  // FF3 0x001fc970: follows the enemy
void ReqPlyrDeadCameraCtrl(float offy, float offy2, float dist, float *ipos, float *trot);    // FF3 0x001fc9d0
void EndPlyrApproachCameraCtrl(void);                                                         // FF3 0x001fca18
void ApproachCameraCrossFadeSW(int sw);                 // FF3 0x001fd4a8
int  GetApproachCameraCrossFade(void);                  // FF3 0x001fd4b8

// Runs the finder overlap effect for time frames (and the crossfade switch).
void ReqFinderInOverRap(u_short time);  // FF3 0x001fc638

// ---- quake shake --------------------------------------------------------
void QuakeCameraStop(void);                                     // FF3 0x001fd540
void QuakeCameraReq(float Power, u_int Time, u_int LoopNum);    // FF3 0x001fd550: no callers in the retail ELF
int  QuakeCameraGetReq(void);                                   // FF3 0x001fd588: no callers

#endif /* _INGAME_CAMERA_MAP_CAMERA_H */
