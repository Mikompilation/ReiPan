/* ==========================================================================
 *  graphics/scene/fod.h
 *
 *  The scene FOD streams (FF2 fod.c): a scene ships a camera, a light and
 *  (FF2 only; FF3 passes 0) an effect stream, each a FOD_FILE_HDR and one
 *  variable-length record per frame. FOD_CTRL walks them in step, keeping a
 *  current and a next camera / light record for scene.c to blend.
 *  fod.c: FF3 0x1c67c0..0x1c7c38, plus FodNextFrame's two walks emitted out
 *  of line at 0x2ba3c8 / 0x2ba468.
 * ======================================================================== */
#ifndef _GRAPHICS_SCENE_FOD_H
#define _GRAPHICS_SCENE_FOD_H

#include <stddef.h>                     // offsetof

#include "graphics/graph3d/g3dLight.h"  // G3DLIGHT
#include "sdk/scetypes.h"
#include "sdk/libvu0.h"                 // sceVu0FVECTOR

struct GRA3DLIGHTDATA;                  // graphics/graph3d/gra3d.h

// A stream's header (FF3: FodInit reads reso +0x08 and frame +0x0c of the
// camera stream; FodNextFrame / FodSetFrame test frame != 0 per stream).
struct FOD_FILE_HDR                     /* 0x10 */
{
    /* 0x00 */ u_int file_id;
    /* 0x04 */ float version;
    /* 0x08 */ u_int reso;              // ticks per frame (FOD_CTRL.resolution)
    /* 0x0c */ u_int frame;             // frame count; 0: the stream has no frame records
};
static_assert(sizeof(FOD_FILE_HDR) == 0x10, "FOD_FILE_HDR (0x10 bytes)");
static_assert(offsetof(FOD_FILE_HDR, reso) == 0x08, "FOD_FILE_HDR.reso");
static_assert(offsetof(FOD_FILE_HDR, frame) == 0x0c, "FOD_FILE_HDR.frame");

// The camera stream's header record after its FOD_FILE_HDR: the scene's
// opening camera (FodGetFirstCam; angles in degrees, file space).
struct FOD_FIRST_CAM                    /* 0x30 */
{
    /* 0x00 */ float p[4];
    /* 0x10 */ float i[4];
    /* 0x20 */ float roll;
    /* 0x24 */ float fov;
    /* 0x28 */ float near_z;            // FF2 near / far (macros on Windows)
    /* 0x2c */ float far_z;
};
static_assert(sizeof(FOD_FIRST_CAM) == 0x30, "FOD_FIRST_CAM (0x30 bytes)");
static_assert(offsetof(FOD_FIRST_CAM, roll) == 0x20, "FOD_FIRST_CAM.roll");
static_assert(offsetof(FOD_FIRST_CAM, far_z) == 0x2c, "FOD_FIRST_CAM.far_z");

// One camera record: anm[0..5] flag position, target (a quadword each),
// roll, fov, near, far (a float each) present in the data after it. The
// next record is (size & ~3) + 0x10 bytes on (FodMoveCameraNextFrame).
struct FOD_CAM_FRAME                    /* 0x10 */
{
    /* 0x00 */ u_int  frame;
    /* 0x04 */ u_int  size;             // the data after this header
    /* 0x08 */ u_char anm[8];           // 1: the channel follows
};
static_assert(sizeof(FOD_CAM_FRAME) == 0x10, "FOD_CAM_FRAME (0x10 bytes)");
static_assert(offsetof(FOD_CAM_FRAME, size) == 0x04, "FOD_CAM_FRAME.size");
static_assert(offsetof(FOD_CAM_FRAME, anm) == 0x08, "FOD_CAM_FRAME.anm");

// One light record: a FOD_LIT_ANM run for the lights whose serial has
// anm_flg 1. size covers the header (the next record is (size & ~3) on).
struct FOD_LIT_FRAME                    /* 0x10 */
{
    /* 0x00 */ u_int frame;
    /* 0x04 */ u_int size;
    /* 0x08 */ u_int pad[2];
};
static_assert(sizeof(FOD_LIT_FRAME) == 0x10, "FOD_LIT_FRAME (0x10 bytes)");
static_assert(offsetof(FOD_LIT_FRAME, size) == 0x04, "FOD_LIT_FRAME.size");

// One animated light in a FOD_LIT_FRAME: type repeats the light's type and
// anm[] flags which quadwords follow (FF3 FodGetToSgLight: 1 directional
// colour / direction, 2 spot colour / position / interest / cone, 3 point
// colour / position).
struct FOD_LIT_ANM                      /* 0x10 */
{
    /* 0x00 */ int    light_id;
    /* 0x04 */ u_int  type;
    /* 0x08 */ u_char anm[8];
};
static_assert(sizeof(FOD_LIT_ANM) == 0x10, "FOD_LIT_ANM (0x10 bytes)");
static_assert(offsetof(FOD_LIT_ANM, type) == 0x04, "FOD_LIT_ANM.type");
static_assert(offsetof(FOD_LIT_ANM, anm) == 0x08, "FOD_LIT_ANM.anm");

// One effect record (FF2; FF3 scenes ship no effect stream). The next is
// size bytes on in FodNextFrame, (size & ~3) in FodSetFrame.
struct FOD_EFF_FRAME                    /* 0x10 */
{
    /* 0x00 */ u_int  frame_no;
    /* 0x04 */ u_int  size;
    /* 0x08 */ u_char zdepth;
    /* 0x09 */ u_char mono;
    /* 0x0a */ u_char sepia;
    /* 0x0b */ u_char color_change;
    /* 0x0c */ u_int  pad;
};
static_assert(sizeof(FOD_EFF_FRAME) == 0x10, "FOD_EFF_FRAME (0x10 bytes)");
static_assert(offsetof(FOD_EFF_FRAME, size) == 0x04, "FOD_EFF_FRAME.size");

// The light stream's header records after the FOD_LIT_SERIAL table: one
// FOD_LIT_AMB, then one FOD_LIT_INF / _SPOT / _POINT per light (by the type
// of the serial whose light_no is id).
struct FOD_LIT_AMB                      /* 0x20 */
{
    /* 0x00 */ u_int id;
    /* 0x04 */ u_int size;
    /* 0x08 */ u_int pad[2];
    /* 0x10 */ float color[4];
};
static_assert(sizeof(FOD_LIT_AMB) == 0x20, "FOD_LIT_AMB (0x20 bytes)");
static_assert(offsetof(FOD_LIT_AMB, color) == 0x10, "FOD_LIT_AMB.color");

struct FOD_LIT_INF                      /* 0x30: directional */
{
    /* 0x00 */ u_int id;
    /* 0x04 */ u_int size;
    /* 0x08 */ u_int pad[2];
    /* 0x10 */ float color[4];
    /* 0x20 */ float direction[4];      // FF3 reads it (FF2 ignored it)
};
static_assert(sizeof(FOD_LIT_INF) == 0x30, "FOD_LIT_INF (0x30 bytes)");
static_assert(offsetof(FOD_LIT_INF, direction) == 0x20, "FOD_LIT_INF.direction");

struct FOD_LIT_SPOT                     /* 0x50 */
{
    /* 0x00 */ u_int id;
    /* 0x04 */ u_int size;
    /* 0x08 */ u_int pad[2];
    /* 0x10 */ float color[4];
    /* 0x20 */ float position[4];
    /* 0x30 */ float interest[4];       // the point it looks at (kept in G3DLIGHT.vDirection)
    /* 0x40 */ float cone;              // degrees
    /* 0x44 */ float spread;
    /* 0x48 */ float fpad[2];
};
static_assert(sizeof(FOD_LIT_SPOT) == 0x50, "FOD_LIT_SPOT (0x50 bytes)");
static_assert(offsetof(FOD_LIT_SPOT, interest) == 0x30, "FOD_LIT_SPOT.interest");
static_assert(offsetof(FOD_LIT_SPOT, cone) == 0x40, "FOD_LIT_SPOT.cone");

struct FOD_LIT_POINT                    /* 0x30 */
{
    /* 0x00 */ u_int id;
    /* 0x04 */ u_int size;
    /* 0x08 */ u_int pad[2];
    /* 0x10 */ float color[4];
    /* 0x20 */ float position[4];
};
static_assert(sizeof(FOD_LIT_POINT) == 0x30, "FOD_LIT_POINT (0x30 bytes)");
static_assert(offsetof(FOD_LIT_POINT, position) == 0x20, "FOD_LIT_POINT.position");

// One fixed (whole-scene) effect record. FF2 types it as a union of the
// effect kinds (MioPan fod.h) whose largest member makes it 0x1c bytes; FF3
// only steps over it (FodGetFixEffect), so it stays opaque here.
struct FOD_EFF_DATA                     /* 0x1c */
{
    /* 0x00 */ u_char unit[0x1c];
};
static_assert(sizeof(FOD_EFF_DATA) == 0x1c, "FOD_EFF_DATA (0x1c bytes)");

// The scene's fixed effects (bss 0x3e9c50, 0x80 bytes; in FF3 only
// FodGetFixEffect touches it).
// layout: host -- fix[] and pdf_p are 4 bytes on the EE.
struct FOD_EFF_PARAM
{
    /* 0x00 */ FOD_EFF_DATA *fix[12];
    /* 0x30 */ float         lenz_pos[4];
    /* 0x40 */ float         lenz_rot[4];
    /* 0x50 */ float         pdf_pos[4];
    /* 0x60 */ float         pdf_spd;
    /* 0x64 */ float         pdf_rate;
    /* 0x68 */ float         pdf_trate;
    /* 0x6c */ void         *pdf_p;
    /* 0x70 */ u_int         fix_eff_num;
    /* 0x74 */ u_int         pad[3];
};
static_assert(offsetof(FOD_EFF_PARAM, lenz_pos) == 0x30 + 12 * (sizeof(void *) - 4), "FOD_EFF_PARAM.lenz_pos (EE +0x30)");
static_assert(offsetof(FOD_EFF_PARAM, pdf_trate) == 0x68 + 12 * (sizeof(void *) - 4), "FOD_EFF_PARAM.pdf_trate (EE +0x68)");
// (pdf_p / fix_eff_num: EE +0x6c / +0x70; the host pads pdf_p to 8.)

#define FOD_LIGHT_MAX   36              // lights of a scene (FOD_LIGHT.lit_serial / all_lit)
#define FOD_AMBIENT_MAX 6

// One light's entry in the light stream's header (FF2 layout; FF3
// SceneChangeLightParameter / SceneLightRevision read light_type and
// light_name, the revision file stores the same record).
struct FOD_LIT_SERIAL                   /* 0x20 */
{
    /* 0x00 */ u_char light_no;         // the id the header records carry (FodGetLightType)
    /* 0x01 */ u_char light_type;       // 1 directional, 2 spot, 3 point (SceneMakeLightName)
    /* 0x02 */ u_char anm_flg;          // 1: the frame records animate it (FodGetToSgLight)
    /* 0x03 */ s_char pad;
    /* 0x04 */ char   light_name[28];   // "<model prefix>_<type><no>", "room..." (FodSetMyLight matches prefixes)
};
static_assert(sizeof(FOD_LIT_SERIAL) == 0x20, "FOD_LIT_SERIAL (0x20 bytes)");
static_assert(offsetof(FOD_LIT_SERIAL, light_type) == 0x01, "FOD_LIT_SERIAL.light_type");
static_assert(offsetof(FOD_LIT_SERIAL, anm_flg) == 0x02, "FOD_LIT_SERIAL.anm_flg");
static_assert(offsetof(FOD_LIT_SERIAL, light_name) == 0x04, "FOD_LIT_SERIAL.light_name");

// The resolved light set of one scene (FF2 layout, 0x14c0; FF3 copies it
// whole in SceneRenewEnvironment, FodGetLightNum fills the counts from
// lit_top, scene.c reads all_lit_num +0x14b0 and hand_spot_no +0x14b4).
// layout: host -- lit_top is 4 bytes on the EE; the offsets before it are
// the ROM's (asserted).
struct FOD_LIGHT
{
    /* 0x0000 */ FOD_LIT_SERIAL lit_serial[FOD_LIGHT_MAX];
    /* 0x0480 */ G3DLIGHT       all_lit[FOD_LIGHT_MAX];
    /* 0x1440 */ sceVu0FVECTOR  amb[FOD_AMBIENT_MAX];   // w 0 (SceneLightRevision)
    /* 0x14a0 */ u_int         *lit_top;        // the light stream (FodInit)
    /* 0x14a4 */ u_int          ilit_num;       // directional lights
    /* 0x14a8 */ u_int          slit_num;       // spots
    /* 0x14ac */ u_int          plit_num;       // points
    /* 0x14b0 */ int            all_lit_num;    // the stream's light count - 1
    /* 0x14b4 */ int            hand_spot_no;   // the light that becomes the player's flashlight, -1: none
    /* 0x14b8 */ u_char         _14b8[8];
};
static_assert(offsetof(FOD_LIGHT, all_lit) == 0x480, "FOD_LIGHT.all_lit (EE +0x480)");
static_assert(offsetof(FOD_LIGHT, amb) == 0x1440, "FOD_LIGHT.amb (EE +0x1440)");
static_assert(offsetof(FOD_LIGHT, lit_top) == 0x14a0, "FOD_LIGHT.lit_top (EE +0x14a0)");
static_assert(offsetof(FOD_LIGHT, all_lit_num) == offsetof(FOD_LIGHT, lit_top) + sizeof(u_int *) + 0xc,
              "FOD_LIGHT.all_lit_num (EE +0x14b0)");
static_assert(offsetof(FOD_LIGHT, hand_spot_no) == offsetof(FOD_LIGHT, all_lit_num) + 4, "FOD_LIGHT.hand_spot_no (EE +0x14b4)");
static_assert(offsetof(FOD_LIGHT, _14b8) + 8 - offsetof(FOD_LIGHT, lit_top) == 0x14c0 - 0x14a0 + sizeof(u_int *) - 4,
              "FOD_LIGHT (EE 0x14c0 bytes)");

// The scene camera of one frame (FF2 layout; FF3 SceneRenewEnvironment
// blends the fields and applies them, SCENE_CTRL +0x2730).
struct FOD_CAMERA_DATA                  /* 0x30 */
{
    /* 0x00 */ sceVu0FVECTOR vPosition;
    /* 0x10 */ sceVu0FVECTOR vTarget;
    /* 0x20 */ float         fRoll;
    /* 0x24 */ float         fFov;
    /* 0x28 */ float         fNearZ;    // blended, not applied (FF3 sets the clip 0.5 / 3050)
    /* 0x2c */ float         fFarZ;
};
static_assert(sizeof(FOD_CAMERA_DATA) == 0x30, "FOD_CAMERA_DATA (0x30 bytes)");
static_assert(offsetof(FOD_CAMERA_DATA, vTarget) == 0x10, "FOD_CAMERA_DATA.vTarget");
static_assert(offsetof(FOD_CAMERA_DATA, fRoll) == 0x20, "FOD_CAMERA_DATA.fRoll");
static_assert(offsetof(FOD_CAMERA_DATA, fFov) == 0x24, "FOD_CAMERA_DATA.fFov");
static_assert(offsetof(FOD_CAMERA_DATA, fFarZ) == 0x2c, "FOD_CAMERA_DATA.fFarZ");

// The FOD walker of one scene (SCENE_CTRL +0x1200, 0x1510 bytes; FF2
// layout). FF3 confirms the frame block: ScenePlayMain reads now_frame
// +0x14ec and now_reso +0x14f4, ScenePlayIsEnd / the skip end_flg +0x14f9,
// and the scene's camera data follows at SCENE_CTRL +0x2730 (FF2 0x20a0:
// fod_ctrl 0x0b70 + 0x1510 + the 0x20-byte fog).
// layout: host -- FOD_LIGHT's lit_top and the eleven stream pointers are 4
// bytes on the EE.
struct FOD_CTRL
{
    /* 0x0000 */ FOD_LIGHT      fod_light;
    /* 0x14c0 */ FOD_FILE_HDR  *cam_file_hdr;
    /* 0x14c4 */ FOD_CAM_FRAME *cam_frame_top;
    /* 0x14c8 */ FOD_CAM_FRAME *cam_frame;
    /* 0x14cc */ FOD_CAM_FRAME *cam_frame_next;
    /* 0x14d0 */ FOD_FILE_HDR  *lit_file_hdr;
    /* 0x14d4 */ FOD_LIT_FRAME *lit_frame_top;
    /* 0x14d8 */ FOD_LIT_FRAME *lit_frame;
    /* 0x14dc */ FOD_LIT_FRAME *lit_frame_next;
    /* 0x14e0 */ FOD_FILE_HDR  *eff_file_hdr;
    /* 0x14e4 */ FOD_EFF_FRAME *eff_frame_top;
    /* 0x14e8 */ FOD_EFF_FRAME *eff_frame;
    /* 0x14ec */ u_int          now_frame;          // the scene's frame (ScenePlayMain starts the stream at 1, allows the skip from 16)
    /* 0x14f0 */ u_int          frame_max;          // FF2, unverified
    /* 0x14f4 */ u_int          now_reso;           // the sub-frame step (0 on whole frames)
    /* 0x14f8 */ u_char         resolution;         // FF2, unverified
    /* 0x14f9 */ u_char         end_flg;            // the scene is over (or skipped)
    /* 0x14fa */ u_char         pad0[2];
    /* 0x14fc */ float          float_now_frame;    // the frame with its fraction (SceneRenewEnvironment blends by it, SceneMdlUpdate sets the motions to it)
    /* 0x1500 */ int            cut_timing_index;   // FF2: the next entry of SceneCutTimingGet's list
    /* 0x1504 */ u_char         _1504[0xc];
};
static_assert(offsetof(FOD_CTRL, cam_file_hdr) == sizeof(FOD_LIGHT), "FOD_CTRL.cam_file_hdr (EE +0x14c0)");
static_assert(offsetof(FOD_CTRL, lit_frame) == offsetof(FOD_CTRL, cam_file_hdr) + 6 * sizeof(void *), "FOD_CTRL.lit_frame (EE +0x14d8)");
static_assert(offsetof(FOD_CTRL, now_frame) == offsetof(FOD_CTRL, cam_file_hdr) + 11 * sizeof(void *), "FOD_CTRL.now_frame (EE +0x14ec)");
static_assert(offsetof(FOD_CTRL, now_reso) == offsetof(FOD_CTRL, now_frame) + 8, "FOD_CTRL.now_reso (EE +0x14f4)");
static_assert(offsetof(FOD_CTRL, end_flg) == offsetof(FOD_CTRL, now_frame) + 0xd, "FOD_CTRL.end_flg (EE +0x14f9)");
static_assert(offsetof(FOD_CTRL, cut_timing_index) == offsetof(FOD_CTRL, now_frame) + 0x14, "FOD_CTRL.cut_timing_index (EE +0x1500)");
static_assert(offsetof(FOD_CTRL, float_now_frame) == offsetof(FOD_CTRL, now_frame) + 0x10, "FOD_CTRL.float_now_frame (EE +0x14fc)");
static_assert(offsetof(FOD_CTRL, _1504) + 0xc - offsetof(FOD_CTRL, cam_file_hdr) == 0x1510 - 0x14c0 + 11 * (sizeof(void *) - 4),
              "FOD_CTRL (EE 0x1510 bytes)");

// The light data FodSetMyLight fills (bss 0x3e9cd0) and the scene's fixed
// effects (bss 0x3e9c50).
extern GRA3DLIGHTDATA FodLight;
extern FOD_EFF_PARAM  eff_param;

// The scene's streams (tcp camera, tlp light, tep effect; FF3 passes tep
// 0) opened at frame 1, the lights resolved at offset (the room's origin).
void FodInit(FOD_CTRL *fc, u_int *tcp, u_int *tlp, u_int *tep, float *offset);  // FF3 0x001c67c0
// One tick of the scene's streams (the PAL frame step snapped to scene_no's
// cut timing); 1 once the frames have run out (end_flg set), else 0.
int  FodNextFrame(FOD_CTRL *fc, int scene_no);  // FF3 0x001c68b8
void FodSetFrame(FOD_CTRL *fc, u_int frame);    // FF3 0x001c6a78
// The lights whose names contain pfx (a model's name, "room") into the
// gra3d light data FodGetGra3DLight returns, applied.
void FodSetMyLight(FOD_LIGHT *fl, const char *pfx);     // FF3 0x001c6fb0
void FodChangeFodLightToGra3dLight(GRA3DLIGHTDATA *pGra3dLight, FOD_LIGHT *fl, const char *pfx);  // FF3 0x001c6ff8
// Spots (vDirection holding their interest point) into the spot slots from
// GRA3D_START_LIGHT_SPOT, aimed and normalised.
void FodSetSpotLights(G3DLIGHT *sl, u_int num); // FF3 0x001c73b8
// One light frame record applied to fl's lights (positions at offset).
void FodGetToSgLight(FOD_LIGHT *fl, FOD_LIT_FRAME *frame, float *offset);  // FF3 0x001c7498
void FodGetDropSpotPos(FOD_LIGHT *fl, const char *pfx, float *lp, float *li);  // FF3 0x001c7708: unreferenced
void FodGetFirstCam(FOD_CAMERA_DATA *cam, FOD_CTRL *fc, float *offset);     // FF3 0x001c7828
void FodGetCamData(FOD_CAMERA_DATA *cam, FOD_CAM_FRAME *frame, float *offset);  // FF3 0x001c7978
GRA3DLIGHTDATA *FodGetGra3DLight(void);         // FF3 0x001c7c00: &FodLight

#endif /* _GRAPHICS_SCENE_FOD_H */
