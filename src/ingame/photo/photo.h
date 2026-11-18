/* ==========================================================================
 *  ingame/photo/photo.h
 *
 *  FF3 0x226138..0x2274e0 (the link-order slot of FF2's photo.c): phase 45,
 *  the peeping view, with its camera and stick helpers and the peeping state
 *  (FF2's FurnPhotoFlg* slot). An event's PEEPING_REQUEST picks one of four
 *  records (data 0x32ded0): the screen fades out, Rei is put at the record's
 *  spot, the camera moves to the record's eye looking at its target, and the
 *  look stick turns the view within the record's limits (the second stick
 *  slides it) until OK; then the game camera comes back and the phase returns
 *  to Story_Normal (0x20). PEEPING_END waits for the state to reach 0.
 *
 *  Also the album types FF2's photo.c owned (PICTURE_WRK, PFILE_WRK), used by
 *  photo_file.c.
 * ======================================================================== */
#ifndef _INGAME_PHOTO_PHOTO_H
#define _INGAME_PHOTO_PHOTO_H

#include <stddef.h>                     // offsetof
#include "sdk/libcdvd.h"                // sceCdCLOCK
#include "sdk/libvu0.h"                // sceVu0FVECTOR
#include "sdk/scetypes.h"

#define PHOTO_FILE_MAX          16      // photos per album (PFILE_WRK)
#define PICTURE_SUBJECT_MAX     3

// One named thing in a filed picture (FF2 PICTURE_SUBJECT).
struct PICTURE_SUBJECT              /* 0x4 */
{
    /* 0x0 */ short type;
    /* 0x2 */ short obj_no;
};
static_assert(sizeof(PICTURE_SUBJECT) == 0x4, "PICTURE_SUBJECT is 0x4 bytes");

// One filed picture (FF2 PICTURE_WRK, same layout in FF3). adr_no is the
// photo-data slot of the pixels and travels with the record through the
// sorts. status bit 0: in use, bit 1: protected.
struct PICTURE_WRK                  /* 0x20 */
{
    /* 0x00 */ u_char          adr_no;
    /* 0x01 */ u_char          chp_no;
    /* 0x02 */ u_char          status;
    /* 0x03 */ u_char          mode;    // FF2 pad; FF3: the photo mode it was taken in (AddPhotoData)
    /* 0x04 */ PICTURE_SUBJECT subject[PICTURE_SUBJECT_MAX];
    /* 0x10 */ u_int           score;
    /* 0x14 */ sceCdCLOCK      time;
    /* 0x1c */ short           room;
    /* 0x1e */ u_char          _1e[2];
};
static_assert(sizeof(PICTURE_WRK) == 0x20, "PICTURE_WRK is 0x20 bytes");
static_assert(offsetof(PICTURE_WRK, subject) == 0x04, "PICTURE_WRK.subject");
static_assert(offsetof(PICTURE_WRK, score) == 0x10, "PICTURE_WRK.score");
static_assert(offsetof(PICTURE_WRK, time) == 0x14, "PICTURE_WRK.time");
static_assert(offsetof(PICTURE_WRK, room) == 0x1c, "PICTURE_WRK.room");

#define PICTURE_STATUS_USED     0x01
#define PICTURE_STATUS_PROTECT  0x02

// An album: FF2 PFILE_WRK, same layout in FF3. The camera's own album is
// data 0x32f290 (GetCamPhotoFile, 0x22ec78).
struct PFILE_WRK                    /* 0x208 */
{
    /* 0x000 */ u_char      pic_num;
    /* 0x001 */ u_char      protect_num;
    /* 0x002 */ u_char      sort_key;
    /* 0x003 */ u_char      skey_bak;
    /* 0x004 */ PICTURE_WRK pic[PHOTO_FILE_MAX];
    /* 0x204 */ u_int       padding;
};
static_assert(sizeof(PFILE_WRK) == 0x208, "PFILE_WRK is 0x208 bytes");
static_assert(offsetof(PFILE_WRK, pic) == 0x004, "PFILE_WRK.pic");

// ── the peeping view (phase 45) ─────────────────────────────────────────

#define PEEP_DAT_NUM    4               // records in data 0x32ded0

// One peeping spot (data 0x32ded0, 0x40 each). Angles in degrees; PeepDatLoad
// turns them into radians. The slide limits bound how far the view's
// translation may move (the look stick near its angle limits, the second
// stick directly).
struct PEEP_DAT                     /* 0x40 */
{
    /* 0x00 */ float plyr_pos[3];       // where Rei is put
    /* 0x0c */ float plyr_rot_y;        // her facing (degrees)
    /* 0x10 */ float cam_pos[3];        // the camera
    /* 0x1c */ float look_dist;         // pivot distance from cam_pos toward cam_target
    /* 0x20 */ float cam_target[3];
    /* 0x2c */ float slide_y_max;       // vertical slide limit (with look.x; push / slide .y)
    /* 0x30 */ float slide_x_max;       // horizontal slide limit (with look.y; push / slide .x)
    /* 0x34 */ float look_x_max;        // pitch limit (degrees)
    /* 0x38 */ float look_y_max;        // yaw limit (degrees)
    /* 0x3c */ float fov;
};
static_assert(sizeof(PEEP_DAT) == 0x40, "PEEP_DAT is 0x40 bytes");
static_assert(offsetof(PEEP_DAT, plyr_rot_y) == 0x0c, "PEEP_DAT.plyr_rot_y");
static_assert(offsetof(PEEP_DAT, cam_pos) == 0x10, "PEEP_DAT.cam_pos");
static_assert(offsetof(PEEP_DAT, look_dist) == 0x1c, "PEEP_DAT.look_dist");
static_assert(offsetof(PEEP_DAT, cam_target) == 0x20, "PEEP_DAT.cam_target");
static_assert(offsetof(PEEP_DAT, slide_y_max) == 0x2c, "PEEP_DAT.slide_y_max");
static_assert(offsetof(PEEP_DAT, look_x_max) == 0x34, "PEEP_DAT.look_x_max");
static_assert(offsetof(PEEP_DAT, fov) == 0x3c, "PEEP_DAT.fov");

// The selected record as quadwords (bss 0x3f6610; PeepDatLoad fills it, the
// sdata pointer gp-0x1d00 points at it).
struct PEEP_VIEW                    /* 0x60 */
{
    /* 0x00 */ sceVu0FVECTOR plyr_pos;      // w 1
    /* 0x10 */ sceVu0FVECTOR plyr_rot;      // (0, rot_y, 0, 0) radians
    /* 0x20 */ sceVu0FVECTOR cam_pos;       // w 0
    /* 0x30 */ sceVu0FVECTOR cam_target;    // w 0
    /* 0x40 */ float         fov;
    /* 0x44 */ float         look_dist;
    /* 0x48 */ float         slide_y_max;
    /* 0x4c */ float         slide_x_max;
    /* 0x50 */ float         look_x_max;    // radians
    /* 0x54 */ float         look_y_max;    // radians
    /* 0x58 */ u_char        _58[8];        // alignment
};
static_assert(sizeof(PEEP_VIEW) == 0x60, "PEEP_VIEW is 0x60 bytes");
static_assert(offsetof(PEEP_VIEW, plyr_rot) == 0x10, "PEEP_VIEW.plyr_rot");
static_assert(offsetof(PEEP_VIEW, cam_pos) == 0x20, "PEEP_VIEW.cam_pos");
static_assert(offsetof(PEEP_VIEW, cam_target) == 0x30, "PEEP_VIEW.cam_target");
static_assert(offsetof(PEEP_VIEW, fov) == 0x40, "PEEP_VIEW.fov");
static_assert(offsetof(PEEP_VIEW, look_dist) == 0x44, "PEEP_VIEW.look_dist");
static_assert(offsetof(PEEP_VIEW, slide_y_max) == 0x48, "PEEP_VIEW.slide_y_max");
static_assert(offsetof(PEEP_VIEW, slide_x_max) == 0x4c, "PEEP_VIEW.slide_x_max");
static_assert(offsetof(PEEP_VIEW, look_x_max) == 0x50, "PEEP_VIEW.look_x_max");
static_assert(offsetof(PEEP_VIEW, look_y_max) == 0x54, "PEEP_VIEW.look_y_max");

// The view's working state (bss 0x3f6560). The camera is (RotMatrix(rot) +
// pivot) * (RotMatrix(look) + push + slide) applied to the local eye and
// target (CameraSetFromRotPair).
struct PEEP_WRK                     /* 0xb0 */
{
    /* 0x00 */ sceVu0FVECTOR save_pos;      // the game camera, restored by PeepViewEnd
    /* 0x10 */ sceVu0FVECTOR save_target;
    /* 0x20 */ float         save_fov;
    /* 0x24 */ u_char        _24[0xc];
    /* 0x30 */ sceVu0FVECTOR pivot;         // cam_pos + dir * look_dist, w 1
    /* 0x40 */ sceVu0FVECTOR rot;           // Vector2Rot of the view direction (y negated)
    /* 0x50 */ sceVu0FVECTOR up;
    /* 0x60 */ sceVu0FVECTOR eye;           // local: (0, 0, look_dist, 1)
    /* 0x70 */ sceVu0FVECTOR target;        // local: (0, 0, look_dist - |target - pos|, 1)
    /* 0x80 */ sceVu0FVECTOR look;          // the look stick's angles (x pitch, y yaw)
    /* 0x90 */ sceVu0FVECTOR slide;         // the second stick's slide
    /* 0xa0 */ sceVu0FVECTOR push;          // the slide near the angle limits
};
static_assert(sizeof(PEEP_WRK) == 0xb0, "PEEP_WRK is 0xb0 bytes");
static_assert(offsetof(PEEP_WRK, save_target) == 0x10, "PEEP_WRK.save_target");
static_assert(offsetof(PEEP_WRK, save_fov) == 0x20, "PEEP_WRK.save_fov");
static_assert(offsetof(PEEP_WRK, pivot) == 0x30, "PEEP_WRK.pivot");
static_assert(offsetof(PEEP_WRK, rot) == 0x40, "PEEP_WRK.rot");
static_assert(offsetof(PEEP_WRK, up) == 0x50, "PEEP_WRK.up");
static_assert(offsetof(PEEP_WRK, eye) == 0x60, "PEEP_WRK.eye");
static_assert(offsetof(PEEP_WRK, target) == 0x70, "PEEP_WRK.target");
static_assert(offsetof(PEEP_WRK, look) == 0x80, "PEEP_WRK.look");
static_assert(offsetof(PEEP_WRK, slide) == 0x90, "PEEP_WRK.slide");
static_assert(offsetof(PEEP_WRK, push) == 0xa0, "PEEP_WRK.push");

// The peeping state (sdata gp+0x2a8), stepped by phase 45.
enum PEEP_STATE
{
    PEEP_STATE_NONE  = 0,               // not peeping (PEEPING_END passes)
    PEEP_STATE_START = 1,               // phase 45 entered from Story_Normal
    PEEP_STATE_IN    = 2,               // on the peeping camera, fading in
    PEEP_STATE_LOOK  = 3,               // looking: the player's ctrl runs PeepLookCtrl
    PEEP_STATE_OUT   = 4,               // leaving, fading out
    PEEP_STATE_BACK  = 5,               // back on the game camera, fading in
};

void  PeepLookCtrl(void);               // FF3 0x002265f0: PlyrNormalCtrl / PlyrFinderCtrl in PEEP_STATE_LOOK
void  CameraSetFromRotPair(void);       // FF3 0x002269c8: the peeping camera (CameraMain, FinderViewCameraCtrl on PLYR_WRK._0e0 bit 0)
void  PeepStateClear(void);             // FF3 0x00226df0 (CameraMainInit)
void  PeepStateSet(int state);          // FF3 0x00226df8
int   PeepStateGet(void);               // FF3 0x00226e00 (PEEPING_END waits for 0)
int   PeepStateIsStart(void);           // FF3 0x00226e08: state 1; no callers
int   PeepStateIsView(void);            // FF3 0x00226e18: states 2..4; no callers
int   PeepStateIsLook(void);            // FF3 0x00226e28: state 3 (PlyrNormalCtrl, PlyrFinderCtrl)
int   PeepingRequest(int no);           // FF3 0x00226e38: PEEPING_REQUEST; returns 1

// Phase 45 (GPhase tables).
int   init_Story_Peeping(void);         // FF3 0x00226ff0
int   one_Story_Peeping(void);          // FF3 0x00227080
int   end_Story_Peeping(void);          // FF3 0x00227430

#endif /* _INGAME_PHOTO_PHOTO_H */
