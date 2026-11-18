/* ==========================================================================
 *  ingame/camera/event_camera.h
 *
 *  Scripted event camera (FF2 event_camera.c, FF3 0x19cd28..0x19e538). The
 *  EV_CAM / SET_EV_CAM_* handlers in ev_command.c drive it; while the map
 *  camera's event flag is up (MapCamSetEventCameraFlg), MapCamNormalCameraCtrl
 *  calls EventCameraMain() every frame instead of the map-data camera.
 *
 *  Two modes share the controller:
 *    manual - the handlers set position / target / fov / roll (room-local
 *             unless the world flag is set) and EventCameraMain() re-applies
 *             them each frame until EventCameraCut(). FF3 dropped FF2's
 *             object-following: an end bound to an object id is left alone.
 *    VCI    - EventCameraVCIReqAddr(path) plays one camera path ("EVEC"
 *             data out of the VCI pack) a frame at a time with a trapezoidal
 *             speed profile; camVciSetEvCam starts it.
 *
 *  FF3 adds a change flag (EventCameraSetChgFlg): the handlers raise it
 *  when they move the running event camera, PlyrMapCamCheck turns it into
 *  bit 31 of its camera word (a camera change) and clears it.
 * ======================================================================== */
#ifndef _INGAME_CAMERA_EVENT_CAMERA_H
#define _INGAME_CAMERA_EVENT_CAMERA_H

#include <stddef.h>                     // offsetof
#include "sdk/scetypes.h"

// ---- VCI camera path ("EVEC") -------------------------------------------
// One header, then DataNum points, both quadword-sized, so the first point
// sits at the header + 0x10. Frame is the move's length in frames; Accel /
// Equal / BrakeTime are ratios of it (summed, each scaled by Frame).
// layout: EE (file data)
struct VCI_CAMERA_HEADER                /* 0x10 */
{
    /* 0x0 */ char    FileId[4];        // "EVEC"
    /* 0x4 */ u_short Frame;
    /* 0x6 */ u_short DataNum;
    /* 0x8 */ u_short HokanType;        // EVECAM_HOKAN_*
    /* 0xa */ u_short AccelTime;
    /* 0xc */ u_short EqualTime;
    /* 0xe */ u_short BrakeTime;
};
static_assert(sizeof(VCI_CAMERA_HEADER) == 0x10, "VCI_CAMERA_HEADER size");
static_assert(offsetof(VCI_CAMERA_HEADER, Frame) == 0x4, "VCI_CAMERA_HEADER.Frame");
static_assert(offsetof(VCI_CAMERA_HEADER, DataNum) == 0x6, "VCI_CAMERA_HEADER.DataNum");
static_assert(offsetof(VCI_CAMERA_HEADER, HokanType) == 0x8, "VCI_CAMERA_HEADER.HokanType");
static_assert(offsetof(VCI_CAMERA_HEADER, AccelTime) == 0xa, "VCI_CAMERA_HEADER.AccelTime");
static_assert(offsetof(VCI_CAMERA_HEADER, EqualTime) == 0xc, "VCI_CAMERA_HEADER.EqualTime");
static_assert(offsetof(VCI_CAMERA_HEADER, BrakeTime) == 0xe, "VCI_CAMERA_HEADER.BrakeTime");

// One path point; FF3 walks them with a 0x30 stride.
// layout: EE (file data)
struct VCI_CAMERA_POINT                 /* 0x30 */
{
    /* 0x00 */ sceVu0FVECTOR LocalPosition;  // eye, room-local
    /* 0x10 */ sceVu0FVECTOR LocalTarget;    // look-at, room-local
    /* 0x20 */ float         Roll;
    /* 0x24 */ float         Proj;           // field of view (gra3dcamSetFov)
    /* 0x28 */ u_char        _28[8];
};
static_assert(sizeof(VCI_CAMERA_POINT) == 0x30, "VCI_CAMERA_POINT size");
static_assert(offsetof(VCI_CAMERA_POINT, LocalTarget) == 0x10, "VCI_CAMERA_POINT.LocalTarget");
static_assert(offsetof(VCI_CAMERA_POINT, Roll) == 0x20, "VCI_CAMERA_POINT.Roll");
static_assert(offsetof(VCI_CAMERA_POINT, Proj) == 0x24, "VCI_CAMERA_POINT.Proj");

enum EVECAM_HOKAN_TYPE                  // VCI_CAMERA_HEADER.HokanType
{
    EVECAM_HOKAN_LINEAR  = 0,
    EVECAM_HOKAN_BEZIER  = 1,
    EVECAM_HOKAN_HERMITE = 2,
};

enum EVECAM_POINT_TYPE                  // which point stream an interpolator works on
{
    EVECAM_POINT_POSITION = 0,
    EVECAM_POINT_TARGET   = 1,
};

// ---- the controller (bss 0x3e67d0) ---------------------------------------
// layout: host -- pEventCamData is a pointer (EE 0x5c, WorldFlg 0x60, size
// 0x64); only this file touches it, offsets asserted up to the pointer.
struct EVENT_CAMERA_CTRL
{
    /* 0x00 */ sceVu0FVECTOR EventPosition;
    /* 0x10 */ sceVu0FVECTOR EventTarget;
    /* 0x20 */ sceVu0FVECTOR EventPositionOffset;
    /* 0x30 */ sceVu0FVECTOR EventTargetOffset;
    /* 0x40 */ int           EventPositonObjType;   // FF2's spelling; -1 unbound
    /* 0x44 */ int           EventPositonObjId;
    /* 0x48 */ int           EventTargetObjType;
    /* 0x4c */ int           EventTargetObjId;
    /* 0x50 */ float         EventFov;
    /* 0x54 */ float         EventRoll;
    /* 0x58 */ float         EventMargin;
    /* 0x5c */ u_int        *pEventCamData;         // non-null: VCI mode
    /* 0x60 */ u_int         WorldFlg;
};
static_assert(offsetof(EVENT_CAMERA_CTRL, EventTarget) == 0x10, "EVENT_CAMERA_CTRL.EventTarget");
static_assert(offsetof(EVENT_CAMERA_CTRL, EventPositionOffset) == 0x20, "EVENT_CAMERA_CTRL.EventPositionOffset");
static_assert(offsetof(EVENT_CAMERA_CTRL, EventTargetOffset) == 0x30, "EVENT_CAMERA_CTRL.EventTargetOffset");
static_assert(offsetof(EVENT_CAMERA_CTRL, EventPositonObjType) == 0x40, "EVENT_CAMERA_CTRL.EventPositonObjType");
static_assert(offsetof(EVENT_CAMERA_CTRL, EventTargetObjId) == 0x4c, "EVENT_CAMERA_CTRL.EventTargetObjId");
static_assert(offsetof(EVENT_CAMERA_CTRL, EventFov) == 0x50, "EVENT_CAMERA_CTRL.EventFov");
static_assert(offsetof(EVENT_CAMERA_CTRL, EventRoll) == 0x54, "EVENT_CAMERA_CTRL.EventRoll");
static_assert(offsetof(EVENT_CAMERA_CTRL, EventMargin) == 0x58, "EVENT_CAMERA_CTRL.EventMargin");

// The VCI player's cursor (sbss gp-0x450 / gp-0x44c).
// layout: host -- pDataTop is a pointer (EE size 8)
struct VCI_CAMERA_CTRL
{
    /* 0x0 */ u_int *pDataTop;
    /* 0x4 */ int    NowFrame;
};
static_assert(offsetof(VCI_CAMERA_CTRL, pDataTop) == 0x0, "VCI_CAMERA_CTRL.pDataTop");

// ---- manual mode ---------------------------------------------------------
void EventCameraReq(void);                      // FF3 0x0019cd28: clear the work, take the camera (MapCamSetEventCameraFlg(1))
void EventCameraInitCtrlReq(void);              // FF3 0x0019cdd8: clear the work (MapCamSetEventCameraFlg(0) calls it)
void EventCameraSetVCIAddress(u_int *pData);    // FF3 0x0019cdf8
void EventCameraSetPosition(float *pos);        // FF3 0x0019ce08: a fixed position (drops the object, leaves VCI mode)
void EventCameraSetTarget(float *pos);          // FF3 0x0019ce30
void EventCameraSetPositionObjId(int type, int id);    // FF3 0x0019ce58: GetObjectPos's type, id (stored only in FF3)
void EventCameraSetTargetObjId(int type, int id);      // FF3 0x0019ce70
void EventCameraSetPositionOffset(float *ofs);  // FF3 0x0019ce88
void EventCameraSetTargetOffset(float *ofs);    // FF3 0x0019cea0
void EventCameraSetFov(float fov);              // FF3 0x0019ceb8
void EventCameraSetRoll(float roll);            // FF3 0x0019cec8
void EventCameraSetMargin(float margin);        // FF3 0x0019ced8
void EventCameraCut(void);                      // FF3 0x0019cee8: back to the map camera (MapCamSetEventCameraFlg(0))
void EventCameraSetWorldFlg(int flg);           // FF3 0x0019cf00: non-zero: position / target are world space
void EventCameraSetChgFlg(int flg);             // FF3 0x0019cf20: 1 is taken only while the event camera runs
int  EventCameraGetChgFlg(void);                // FF3 0x0019cf60: bit 31 of PlyrMapCamCheck's word
int  EventCameraMain(void);                     // FF3 0x0019cf68 (MapCamNormalCameraCtrl): always 0

// ---- VCI mode ------------------------------------------------------------
int  EventCameraVCIPlay(void);                  // FF3 0x0019d060: one frame; 1 when no path is set
void EventCameraVCICtrlInit(u_int *pDataTop);   // FF3 0x0019d208: rewind onto pDataTop
void EventCameraVCIReqAddr(u_int *pData);       // FF3 0x0019dfd0: play path pData (camVciSetEvCam)
void EventCameraVCIReq(int no);                 // FF3 0x0019e000: path no of the pack at 0x3100000 (no caller)

// ---- curve primitives ----------------------------------------------------
// De Casteljau over pControlPoint[0..n]; Point[3] = 1.
void CalcBezierPoint(float *Point, int n, float t, sceVu0FVECTOR *pControlPoint);   // FF3 0x0019e048
// Cubic Hermite from Point0 to Point1 with tangents Direction0 / 1 at ParamU.
void CalcHermitePoint(float *HermiteVector, float *Point0, float *Point1,
                      float *Direction0, float *Direction1, float ParamU);         // FF3 0x0019e3e8

#endif /* _INGAME_CAMERA_EVENT_CAMERA_H */
