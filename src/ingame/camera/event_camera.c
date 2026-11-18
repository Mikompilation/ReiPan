/* ==========================================================================
 *  ingame/camera/event_camera.c
 *
 *  Scripted event camera, FF3 0x19cd28..0x19e538. From MioPan
 *  ingame/camera/event_camera.c (FF2 event_camera.o), checked against FF3.
 *
 *  Manual mode re-applies whatever the event handlers last set, every
 *  frame. VCI mode walks a camera path:
 *
 *    EventCameraVCIPlayFrame()   the path's arc lengths -> peak speeds
 *      EventCameraHokan()        frame -> distance along the path
 *        EventCameraGetProgress()     the trapezoid (accelerate, hold,
 *                                     brake) integrated to that frame
 *        EventCameraSetFromPointDist() / SetBezier() / SetHermite()
 *                                     eye and look-at at that distance
 *        EventCameraSetRollFromPointDist() / SetProjFromPointDist()
 *
 *  The eye and the look-at are advanced independently (each has its own
 *  arc length), so both reach the end together.
 *
 *  FF3 against FF2: EventCameraMain no longer follows objects (an end bound
 *  to an object id is not set at all), the change flag (gp-0x448) is new,
 *  EventCameraVCIReq is split in two (EventCameraVCIReqAddr takes a resolved
 *  path; camVciSetEvCam calls it) and no longer checks the "EVEC" id,
 *  GetAddrPK2 lost its count check, gra3dApplyCamera gained FF3's third
 *  argument, and FF2's error printfs are gone.
 * ======================================================================== */

#include "ingame/camera/event_camera.h"

#include <math.h>                       // sqrtf, fabs

#include "graphics/graph3d/gra3d.h"     // gra3dcamSet*, gra3dApplyCamera
#include "ingame/camera/map_camera.h"   // MapCamSetEventCameraFlg, MapCamGra3dcamSet*AddOffset
#include "ingame/plyr/player.h"         // GetPlyrAreaNo
#include "reipan/reipan_memory.h"       // PC: ReiPan_GetHostPointer
#include "sdk/libvu0.h"

// The default eye (0, 0, -5000) and fov installed by EventCameraInitCtrl
// (.lit4 0x370208 / 0x37020c).
#define EVECAM_INIT_Z   (-5000.0f)
#define EVECAM_INIT_FOV 0.768370688f

// Bezier control-point budget: EventCameraSetBezier and CalcBezierPoint
// keep 50 vectors on the stack.
#define EVECAM_BEZIER_POINT_MAX 50

// The camera VCI pack's EE address (FF2 CAMERA_VCI_ADDR).
#define EVECAM_VCI_ADDR 0x3100000

/* bss 0x3e67d0 */ static EVENT_CAMERA_CTRL event_camera_ctrl;
/* sbss gp-0x450 */ static VCI_CAMERA_CTRL vci_cam_ctrl;
/* sbss gp-0x448 */ static u_char event_camera_chg_flg;

static void  EventCameraInitCtrl(EVENT_CAMERA_CTRL *pEveCamCtrl);
static void  EventCameraVCIPlayFrame(VCI_CAMERA_HEADER *pHead, u_int frame, int RoomNo);
static float EventCameraGetProgress(VCI_CAMERA_HEADER *pHead, u_int frame);
static void  EventCameraHokan(float PosSpdMax, float TargetSpdMax, float RollSpdMax,
                              float ProjSpdMax, VCI_CAMERA_HEADER *pHead, u_int frame,
                              int RoomNo);
static int   EventCameraGetPosDistToPointNo(float PosDist, VCI_CAMERA_HEADER *pHead);
static int   EventCameraGetTargetDistToPointNo(float TargetDist, VCI_CAMERA_HEADER *pHead);
static void  EventCameraGetMoveDistToPoint(float *pPosDist, float *pTargetDist,
                                           VCI_CAMERA_HEADER *pHead, int PointNo);
static float EventCameraGetRollDistToPoint(VCI_CAMERA_HEADER *pHead, int PointNo);
static float EventCameraGetProjDistToPoint(VCI_CAMERA_HEADER *pHead, int PointNo);
static void  EventCameraGetMoveDist(float *pPosDist, float *pTargetDist, VCI_CAMERA_HEADER *pHead);
static float EventCameraGetRollDist(VCI_CAMERA_HEADER *pHead);
static float EventCameraGetProjDist(VCI_CAMERA_HEADER *pHead);
static void  EventCameraSetFromPointDist(float Dist, VCI_CAMERA_HEADER *pHead, int PointNo,
                                         int RoomNo, int PointType);
static void  EventCameraSetBezier(float Dist, VCI_CAMERA_HEADER *pHead, int RoomNo, int PointType);
static void  EventCameraSetRollFromPointDist(float Dist, VCI_CAMERA_HEADER *pHead, int PointNo);
static void  EventCameraSetProjFromPointDist(float Dist, VCI_CAMERA_HEADER *pHead, int PointNo);
static u_int *GetAddrPK2(u_int *top, int no);
static void  EventCameraSetHermite(float Dist, VCI_CAMERA_HEADER *pHead, int PointNo,
                                   int RoomNo, int PointType);
static void  EventCameraGetHermiteDirVector(float *DirVector, VCI_CAMERA_POINT *pEveCam,
                                            int PointNo, int DataNum, int PointType);

// The path's points follow its header.
static VCI_CAMERA_POINT *EventCameraPoints(VCI_CAMERA_HEADER *pHead)
{
    return (VCI_CAMERA_POINT *)(pHead + 1);
}

// FF2's Get2PLength, inlined in FF3 as vsub / vmul / vaddabc / vmaddbc /
// vsqrt: the xyz distance between two points.
static float EventCamera2PLength(const float *v0, const float *v1)
{
    float dx = v0[0] - v1[0];
    float dy = v0[1] - v1[1];
    float dz = v0[2] - v1[2];

    return sqrtf(dx * dx + dy * dy + dz * dz);
}

static void EventCameraCopyVector(float *dst, const float *src)
{
    dst[0] = src[0];
    dst[1] = src[1];
    dst[2] = src[2];
    dst[3] = src[3];
}

// ──────────────────────────────────────────────────────────────────────
// Manual mode

// Clear the work and take the camera.
// FF3 0x0019cd28; FF2 EventCameraReq.

void EventCameraReq(void)
{
    EventCameraInitCtrl(&event_camera_ctrl);
    MapCamSetEventCameraFlg(1);
}

// FF3 0x0019cd50; FF2 EventCameraInitCtrl (FF3 also clears the change flag).

static void EventCameraInitCtrl(EVENT_CAMERA_CTRL *pEveCamCtrl)
{
    pEveCamCtrl->EventPosition[0] = 0.0f;
    pEveCamCtrl->EventPosition[1] = 0.0f;
    pEveCamCtrl->EventPosition[2] = EVECAM_INIT_Z;
    pEveCamCtrl->EventPosition[3] = 0.0f;

    for (int i = 0; i < 4; i++)
    {
        pEveCamCtrl->EventTarget[i] = 0.0f;
        pEveCamCtrl->EventPositionOffset[i] = 0.0f;
        pEveCamCtrl->EventTargetOffset[i] = 0.0f;
    }

    pEveCamCtrl->EventPositonObjType = -1;
    pEveCamCtrl->EventPositonObjId = -1;
    pEveCamCtrl->EventTargetObjType = -1;
    pEveCamCtrl->EventTargetObjId = -1;

    pEveCamCtrl->EventFov = EVECAM_INIT_FOV;
    pEveCamCtrl->EventRoll = 0.0f;
    pEveCamCtrl->EventMargin = 0.0f;

    pEveCamCtrl->pEventCamData = nullptr;
    pEveCamCtrl->WorldFlg = 0;

    event_camera_chg_flg = 0;
}

// FF3 0x0019cdd8; FF2 EventCameraInitCtrlReq.

void EventCameraInitCtrlReq(void)
{
    EventCameraInitCtrl(&event_camera_ctrl);
}

// FF3 0x0019cdf8; FF2 EventCameraSetVCIAddress.

void EventCameraSetVCIAddress(u_int *pData)
{
    event_camera_ctrl.pEventCamData = pData;
}

// A fixed position: drops the object binding and leaves VCI mode.
// FF3 0x0019ce08; FF2 EventCameraSetPosition.

void EventCameraSetPosition(float *pos)
{
    EVENT_CAMERA_CTRL *pEveCamCtrl = &event_camera_ctrl;

    EventCameraCopyVector(pEveCamCtrl->EventPosition, pos);
    pEveCamCtrl->pEventCamData = nullptr;
    pEveCamCtrl->EventPositonObjId = -1;
    pEveCamCtrl->EventPositonObjType = -1;
}

// FF3 0x0019ce30; FF2 EventCameraSetTarget.

void EventCameraSetTarget(float *pos)
{
    EVENT_CAMERA_CTRL *pEveCamCtrl = &event_camera_ctrl;

    EventCameraCopyVector(pEveCamCtrl->EventTarget, pos);
    pEveCamCtrl->pEventCamData = nullptr;
    pEveCamCtrl->EventTargetObjId = -1;
    pEveCamCtrl->EventTargetObjType = -1;
}

// FF3 0x0019ce58; FF2 EventCameraSetPositionObjId.

void EventCameraSetPositionObjId(int type, int id)
{
    event_camera_ctrl.EventPositonObjId = id;
    event_camera_ctrl.EventPositonObjType = type;
}

// FF3 0x0019ce70; FF2 EventCameraSetTargetObjId.

void EventCameraSetTargetObjId(int type, int id)
{
    event_camera_ctrl.EventTargetObjId = id;
    event_camera_ctrl.EventTargetObjType = type;
}

// FF3 0x0019ce88; FF2 EventCameraSetPositionOffset.

void EventCameraSetPositionOffset(float *ofs)
{
    EventCameraCopyVector(event_camera_ctrl.EventPositionOffset, ofs);
}

// FF3 0x0019cea0; FF2 EventCameraSetTargetOffset.

void EventCameraSetTargetOffset(float *ofs)
{
    EventCameraCopyVector(event_camera_ctrl.EventTargetOffset, ofs);
}

// FF3 0x0019ceb8; FF2 EventCameraSetFov.

void EventCameraSetFov(float fov)
{
    event_camera_ctrl.EventFov = fov;
}

// FF3 0x0019cec8; FF2 EventCameraSetRoll.

void EventCameraSetRoll(float roll)
{
    event_camera_ctrl.EventRoll = roll;
}

// FF3 0x0019ced8; FF2 EventCameraSetMargin.

void EventCameraSetMargin(float margin)
{
    event_camera_ctrl.EventMargin = margin;
}

// Back to the map camera (MapCamSetEventCameraFlg(0) clears the work).
// FF3 0x0019cee8; FF2 EventCameraCut.

void EventCameraCut(void)
{
    MapCamSetEventCameraFlg(0);
}

// FF3 0x0019cf00; FF2 EventCameraSetWorldFlg.

void EventCameraSetWorldFlg(int flg)
{
    if (flg != 0)
    {
        event_camera_ctrl.WorldFlg = 1;
        return;
    }
    event_camera_ctrl.WorldFlg = 0;
}

// The handlers raise the flag (1) when they move the event camera; it only
// takes while the event camera runs. PlyrMapCamCheck reads it and clears it.
// FF3 0x0019cf20 (FF3-only).

void EventCameraSetChgFlg(int flg)
{
    if ((char)flg != 1 || MapCamGetEventCameraFlg() != 0)
        event_camera_chg_flg = (u_char)flg;
}

// FF3 0x0019cf60 (FF3-only).

int EventCameraGetChgFlg(void)
{
    return event_camera_chg_flg;
}

// Every frame while the event camera runs: play the VCI path, or apply the
// manual setting (room-local through the map camera's room offset unless the
// world flag is set). An end bound to an object is left as it is.
// FF3 0x0019cf68; FF2 EventCameraMain (FF3 dropped the GetObjectPos branches).

int EventCameraMain(void)
{
    EVENT_CAMERA_CTRL *pEveCamCtrl = &event_camera_ctrl;

    if (pEveCamCtrl->pEventCamData != nullptr)
    {
        EventCameraVCIPlay();
    }
    else
    {
        if (pEveCamCtrl->EventPositonObjType == -1 || pEveCamCtrl->EventPositonObjId == -1)
        {
            if (pEveCamCtrl->WorldFlg == 0)
                MapCamGra3dcamSetPositionAddOffset(pEveCamCtrl->EventPosition, GetPlyrAreaNo());
            else
                gra3dcamSetPosition(pEveCamCtrl->EventPosition);
        }

        if (pEveCamCtrl->EventTargetObjType == -1 || pEveCamCtrl->EventTargetObjId == -1)
        {
            if (pEveCamCtrl->WorldFlg == 0)
                MapCamGra3dcamSetTargetAddOffset(pEveCamCtrl->EventTarget, GetPlyrAreaNo());
            else
                gra3dcamSetTarget(pEveCamCtrl->EventTarget, 1);
        }

        gra3dcamSetFov(pEveCamCtrl->EventFov);
        gra3dcamSetRoll(pEveCamCtrl->EventRoll);
    }

    gra3dApplyCamera(nullptr, 0, 1);
    return 0;
}

// ──────────────────────────────────────────────────────────────────────
// VCI playback

// One frame of the path; past its end the last frame is held.
// FF3 0x0019d060; FF2 EventCameraVCIPlay.

int EventCameraVCIPlay(void)
{
    VCI_CAMERA_HEADER *pHead = (VCI_CAMERA_HEADER *)vci_cam_ctrl.pDataTop;

    if (pHead == nullptr)
        return 1;

    if (vci_cam_ctrl.NowFrame >= (int)pHead->Frame)
        vci_cam_ctrl.NowFrame = pHead->Frame - 1;

    EventCameraVCIPlayFrame(pHead, vci_cam_ctrl.NowFrame, GetPlyrAreaNo());
    vci_cam_ctrl.NowFrame++;
    return 0;
}

// The path's total distances -> peak speeds: the trapezoid's area equals a
// rectangle of the peak over TmpFrame frames, so SpdMax = 2 * dist / TmpFrame.
// FF3 0x0019d100; FF2 EventCameraVCIPlayFrame.

static void EventCameraVCIPlayFrame(VCI_CAMERA_HEADER *pHead, u_int frame, int RoomNo)
{
    float PosDist;
    float TargetDist;
    float RollDist;
    float ProjDist;
    int   AllTime;
    int   TmpFrame;

    if (pHead == nullptr)
        return;

    EventCameraGetMoveDist(&PosDist, &TargetDist, pHead);
    RollDist = EventCameraGetRollDist(pHead);
    ProjDist = EventCameraGetProjDist(pHead);

    AllTime = pHead->AccelTime + pHead->EqualTime + pHead->BrakeTime;
    if (AllTime == 0)
        return;

    TmpFrame = pHead->Frame + (int)(pHead->Frame * pHead->EqualTime) / AllTime;
    if (TmpFrame == 0)
        return;

    EventCameraHokan((PosDist + PosDist) / (float)TmpFrame,
                     (TargetDist + TargetDist) / (float)TmpFrame,
                     (RollDist + RollDist) / (float)TmpFrame,
                     (ProjDist + ProjDist) / (float)TmpFrame,
                     pHead, frame, RoomNo);
}

// FF3 0x0019d208; FF2 EventCameraVCICtrlInit.

void EventCameraVCICtrlInit(u_int *pDataTop)
{
    vci_cam_ctrl.pDataTop = pDataTop;
    vci_cam_ctrl.NowFrame = 0;
}

// The trapezoidal speed profile integrated up to frame, in percent of the
// peak (EventCameraHokan divides the 100 out).
// FF3 0x0019d218; FF2 EventCameraGetProgress.

static float EventCameraGetProgress(VCI_CAMERA_HEADER *pHead, u_int frame)
{
    u_int AllTime;
    u_int AccelFrame;
    u_int EqualFrame;
    u_int BrakeFrame;
    u_int TmpFrame;
    float ret = 0.0f;

    AllTime = pHead->AccelTime + pHead->EqualTime + pHead->BrakeTime;
    if (AllTime == 0)
        return ret;

    AccelFrame = (u_int)((int)(pHead->AccelTime * pHead->Frame) / (int)AllTime);
    BrakeFrame = (u_int)((int)(pHead->BrakeTime * pHead->Frame) / (int)AllTime);
    EqualFrame = pHead->Frame - AccelFrame - BrakeFrame;

    if (frame <= AccelFrame)
    {
        if (AccelFrame != 0)
            ret = (float)frame * 100.0f * (float)frame / (float)AccelFrame * 0.5f;
    }
    else if (frame <= AccelFrame + EqualFrame)
    {
        if (AccelFrame + EqualFrame != 0)
            ret = (float)AccelFrame * 100.0f * 0.5f + (float)(frame - AccelFrame) * 100.0f;
    }
    else
    {
        TmpFrame = BrakeFrame - ((frame - AccelFrame) - EqualFrame);
        if (BrakeFrame != 0)
        {
            ret = ((float)AccelFrame * 100.0f * 0.5f + (float)EqualFrame * 100.0f
                   + (float)BrakeFrame * 100.0f * 0.5f)
                  - (float)TmpFrame * 100.0f * (float)TmpFrame / (float)BrakeFrame * 0.5f;
        }
    }

    return ret;
}

// Place the camera for one frame. Roll and fov key off one segment index,
// taken from whichever of the two streams has travelled further.
// FF3 0x0019d560; FF2 EventCameraHokan.

static void EventCameraHokan(float PosSpdMax, float TargetSpdMax, float RollSpdMax,
                             float ProjSpdMax, VCI_CAMERA_HEADER *pHead, u_int frame,
                             int RoomNo)
{
    float Progress;
    float PosDist;
    float TargetDist;
    float RollDist;
    float ProjDist;
    int   PointNo;

    Progress = EventCameraGetProgress(pHead, frame);

    PosDist = PosSpdMax * Progress / 100.0f;
    TargetDist = TargetSpdMax * Progress / 100.0f;
    ProjDist = ProjSpdMax * Progress / 100.0f;
    RollDist = RollSpdMax * Progress / 100.0f;

    if (PosDist < TargetDist)
        PointNo = EventCameraGetTargetDistToPointNo(TargetDist, pHead);
    else
        PointNo = EventCameraGetPosDistToPointNo(PosDist, pHead);

    if (pHead->HokanType == EVECAM_HOKAN_BEZIER)
    {
        EventCameraSetBezier(PosDist, pHead, RoomNo, EVECAM_POINT_POSITION);
        EventCameraSetBezier(TargetDist, pHead, RoomNo, EVECAM_POINT_TARGET);
    }
    else if (pHead->HokanType == EVECAM_HOKAN_HERMITE)
    {
        EventCameraSetHermite(PosDist, pHead, PointNo, RoomNo, EVECAM_POINT_POSITION);
        EventCameraSetHermite(TargetDist, pHead, PointNo, RoomNo, EVECAM_POINT_TARGET);
    }
    else
    {
        EventCameraSetFromPointDist(PosDist, pHead, PointNo, RoomNo, EVECAM_POINT_POSITION);
        EventCameraSetFromPointDist(TargetDist, pHead, PointNo, RoomNo, EVECAM_POINT_TARGET);
    }

    EventCameraSetRollFromPointDist(RollDist, pHead, PointNo);
    EventCameraSetProjFromPointDist(ProjDist, pHead, PointNo);

    gra3dApplyCamera(nullptr, 1, 1);
}

// ──────────────────────────────────────────────────────────────────────
// Arc lengths

// The segment the eye has reached after PosDist; the last one past the end.
// FF3 0x0019d718; FF2 EventCameraGetPosDistToPointNo.

static int EventCameraGetPosDistToPointNo(float PosDist, VCI_CAMERA_HEADER *pHead)
{
    VCI_CAMERA_POINT *pEveCam;
    float dist = 0.0f;
    int   PointNo = 0;

    if (pHead == nullptr)
        return PointNo;

    pEveCam = EventCameraPoints(pHead);
    for (int i = 0; i < pHead->DataNum - 1; i++, pEveCam++)
    {
        dist += EventCamera2PLength(pEveCam[0].LocalPosition, pEveCam[1].LocalPosition);
        if (PosDist <= dist)
        {
            PointNo = i;
            break;
        }
    }

    if (dist < PosDist)
        PointNo = pHead->DataNum - 2;

    return PointNo;
}

// FF3 0x0019d7c0; FF2 EventCameraGetTargetDistToPointNo.

static int EventCameraGetTargetDistToPointNo(float TargetDist, VCI_CAMERA_HEADER *pHead)
{
    VCI_CAMERA_POINT *pEveCam;
    float dist = 0.0f;
    int   PointNo = 0;

    if (pHead == nullptr)
        return PointNo;

    pEveCam = EventCameraPoints(pHead);
    for (int i = 0; i < pHead->DataNum - 1; i++, pEveCam++)
    {
        dist += EventCamera2PLength(pEveCam[0].LocalTarget, pEveCam[1].LocalTarget);
        if (TargetDist <= dist)
        {
            PointNo = i;
            break;
        }
    }

    if (dist < TargetDist)
        PointNo = pHead->DataNum - 2;

    return PointNo;
}

// Arc lengths of the eye / the look-at from the start up to point PointNo;
// either output may be null.
// FF3 0x0019d870; FF2 EventCameraGetMoveDistToPoint.

static void EventCameraGetMoveDistToPoint(float *pPosDist, float *pTargetDist,
                                          VCI_CAMERA_HEADER *pHead, int PointNo)
{
    VCI_CAMERA_POINT *pEveCam;

    if (pHead == nullptr)
        return;

    pEveCam = EventCameraPoints(pHead);

    if (pPosDist != nullptr)
        *pPosDist = 0.0f;
    if (pTargetDist != nullptr)
        *pTargetDist = 0.0f;

    for (int i = 0; i < PointNo; i++, pEveCam++)
    {
        if (pPosDist != nullptr)
            *pPosDist += EventCamera2PLength(pEveCam[0].LocalPosition, pEveCam[1].LocalPosition);
        if (pTargetDist != nullptr)
            *pTargetDist += EventCamera2PLength(pEveCam[0].LocalTarget, pEveCam[1].LocalTarget);
    }
}

// Roll and fov are scalars: their "distance" is the summed absolute change.
// fabs takes a double here as in FF3 (its fptodp / dptofp calls are the
// conversions, implicit in C).
// FF3 0x0019d930; FF2 EventCameraGetRollDistToPoint.
// callcheck: fptodp, dptofp (fabs's double argument and result, implicit in C)

static float EventCameraGetRollDistToPoint(VCI_CAMERA_HEADER *pHead, int PointNo)
{
    VCI_CAMERA_POINT *pEveCam;
    float dist = 0.0f;

    if (pHead == nullptr)
        return dist;

    pEveCam = EventCameraPoints(pHead);
    for (int i = 0; i < PointNo; i++, pEveCam++)
        dist += (float)fabs(pEveCam[1].Roll - pEveCam[0].Roll);

    return dist;
}

// FF3 0x0019d9b0; FF2 EventCameraGetProjDistToPoint.
// callcheck: fptodp, dptofp (fabs's double argument and result, implicit in C)

static float EventCameraGetProjDistToPoint(VCI_CAMERA_HEADER *pHead, int PointNo)
{
    VCI_CAMERA_POINT *pEveCam;
    float dist = 0.0f;

    if (pHead == nullptr)
        return dist;

    pEveCam = EventCameraPoints(pHead);
    for (int i = 0; i < PointNo; i++, pEveCam++)
        dist += (float)fabs(pEveCam[1].Proj - pEveCam[0].Proj);

    return dist;
}

// FF3 0x0019da30; FF2 EventCameraGetMoveDist.

static void EventCameraGetMoveDist(float *pPosDist, float *pTargetDist, VCI_CAMERA_HEADER *pHead)
{
    if (pHead == nullptr)
        return;

    EventCameraGetMoveDistToPoint(pPosDist, pTargetDist, pHead, pHead->DataNum - 1);
}

// FF3 0x0019da58; FF2 EventCameraGetRollDist.

static float EventCameraGetRollDist(VCI_CAMERA_HEADER *pHead)
{
    if (pHead == nullptr)
        return 0.0f;

    return EventCameraGetRollDistToPoint(pHead, pHead->DataNum - 1);
}

// FF3 0x0019da88; FF2 EventCameraGetProjDist.

static float EventCameraGetProjDist(VCI_CAMERA_HEADER *pHead)
{
    if (pHead == nullptr)
        return 0.0f;

    return EventCameraGetProjDistToPoint(pHead, pHead->DataNum - 1);
}

// ──────────────────────────────────────────────────────────────────────
// Interpolators

// HokanType 0: a straight line between the two points around Dist.
// FF3 0x0019dab8; FF2 EventCameraSetFromPointDist.

static void EventCameraSetFromPointDist(float Dist, VCI_CAMERA_HEADER *pHead, int PointNo,
                                        int RoomNo, int PointType)
{
    VCI_CAMERA_POINT *pEveCam;
    sceVu0FVECTOR     TmpVec;
    sceVu0FVECTOR     StartPos;
    sceVu0FVECTOR     EndPos;
    float             PointDist;
    float             Point2PointDist;
    float             Scale;

    if (pHead == nullptr)
        return;

    pEveCam = EventCameraPoints(pHead);

    if (PointType == EVECAM_POINT_POSITION)
    {
        EventCameraCopyVector(StartPos, pEveCam[PointNo].LocalPosition);
        EventCameraCopyVector(EndPos, pEveCam[PointNo + 1].LocalPosition);
        EventCameraGetMoveDistToPoint(&PointDist, nullptr, pHead, PointNo);
    }
    else
    {
        EventCameraCopyVector(StartPos, pEveCam[PointNo].LocalTarget);
        EventCameraCopyVector(EndPos, pEveCam[PointNo + 1].LocalTarget);
        EventCameraGetMoveDistToPoint(nullptr, &PointDist, pHead, PointNo);
    }

    Dist -= PointDist;
    Point2PointDist = EventCamera2PLength(EndPos, StartPos);

    sceVu0SubVector(TmpVec, EndPos, StartPos);
    Scale = 0.0f;
    if (Point2PointDist != 0.0f)
        Scale = Dist / Point2PointDist;
    sceVu0ScaleVector(TmpVec, TmpVec, Scale);
    sceVu0AddVector(TmpVec, TmpVec, StartPos);

    if (PointType == EVECAM_POINT_POSITION)
        MapCamGra3dcamSetPositionAddOffset(TmpVec, RoomNo);
    else
        MapCamGra3dcamSetTargetAddOffset(TmpVec, RoomNo);
}

// HokanType 1: one Bezier over every point of the path, at the fraction of
// the total arc length.
// FF3 0x0019dc30; FF2 EventCameraSetBezier.

static void EventCameraSetBezier(float Dist, VCI_CAMERA_HEADER *pHead, int RoomNo, int PointType)
{
    sceVu0FVECTOR     ControlPoint[EVECAM_BEZIER_POINT_MAX];
    sceVu0FVECTOR     TmpVec;
    VCI_CAMERA_POINT *pEveCam;
    float             AllDist;
    float             DistRate;

    if (pHead == nullptr)
        return;
    if (pHead->DataNum > EVECAM_BEZIER_POINT_MAX)
        return;

    pEveCam = EventCameraPoints(pHead);

    if (PointType == EVECAM_POINT_POSITION)
    {
        for (int i = 0; i < pHead->DataNum; i++)
            EventCameraCopyVector(ControlPoint[i], pEveCam[i].LocalPosition);
        EventCameraGetMoveDist(&AllDist, nullptr, pHead);
    }
    else
    {
        for (int i = 0; i < pHead->DataNum; i++)
            EventCameraCopyVector(ControlPoint[i], pEveCam[i].LocalTarget);
        EventCameraGetMoveDist(nullptr, &AllDist, pHead);
    }

    DistRate = 0.0f;
    if (AllDist != 0.0f)
        DistRate = Dist / AllDist;

    CalcBezierPoint(TmpVec, pHead->DataNum - 1, DistRate, ControlPoint);

    if (PointType == EVECAM_POINT_POSITION)
        MapCamGra3dcamSetPositionAddOffset(TmpVec, RoomNo);
    else
        MapCamGra3dcamSetTargetAddOffset(TmpVec, RoomNo);
}

// Roll on the accumulated-change scale: dividing by |ChgRoll| turns Dist
// back into a signed step along this segment.
// FF3 0x0019dd68; FF2 EventCameraSetRollFromPointDist.
// callcheck: fptodp, dptofp (fabs's double argument and result, implicit in C)

static void EventCameraSetRollFromPointDist(float Dist, VCI_CAMERA_HEADER *pHead, int PointNo)
{
    VCI_CAMERA_POINT *pEveCam;
    float             ChgRoll;
    float             AbsChg;
    float             Roll;

    if (pHead == nullptr)
        return;

    Dist -= EventCameraGetRollDistToPoint(pHead, PointNo);
    pEveCam = EventCameraPoints(pHead) + PointNo;

    ChgRoll = pEveCam[1].Roll - pEveCam[0].Roll;
    AbsChg = (float)fabs(ChgRoll);
    if (AbsChg == 0.0f)
        Roll = pEveCam[0].Roll;
    else
        Roll = pEveCam[0].Roll + ChgRoll * Dist / AbsChg;

    gra3dcamSetRoll(Roll);
}

// FF3 0x0019de30; FF2 EventCameraSetProjFromPointDist.
// callcheck: fptodp, dptofp (fabs's double argument and result, implicit in C)

static void EventCameraSetProjFromPointDist(float Dist, VCI_CAMERA_HEADER *pHead, int PointNo)
{
    VCI_CAMERA_POINT *pEveCam;
    float             ChgProj;
    float             AbsChg;
    float             Proj;

    if (pHead == nullptr)
        return;

    Dist -= EventCameraGetProjDistToPoint(pHead, PointNo);
    pEveCam = EventCameraPoints(pHead) + PointNo;

    ChgProj = pEveCam[1].Proj - pEveCam[0].Proj;
    AbsChg = (float)fabs(ChgProj);
    if (AbsChg == 0.0f)
        Proj = pEveCam[0].Proj;
    else
        Proj = pEveCam[0].Proj + ChgProj * Dist / AbsChg;

    gra3dcamSetFov(Proj);
}

// ──────────────────────────────────────────────────────────────────────
// Starting a path

// Play path pData from its first frame and take the camera.
// FF3 0x0019dfd0 (FF3-only: the tail of FF2 EventCameraVCIReq).

void EventCameraVCIReqAddr(u_int *pData)
{
    EventCameraSetVCIAddress(pData);
    EventCameraVCICtrlInit(pData);
    MapCamSetEventCameraFlg(1);
}

// Path no of the camera VCI pack. No caller in FF3 (camVciSetEvCam resolves
// its own path).
// FF3 0x0019e000; FF2 EventCameraVCIReq (FF3 drops the "EVEC" check).

void EventCameraVCIReq(int no)
{
    // PC: the pack's fixed EE address goes through the emulated RAM block.
    EventCameraVCIReqAddr(GetAddrPK2((u_int *)ReiPan_GetHostPointer(EVECAM_VCI_ADDR), no));
}

// PK2 pack: word 0 the file count, words 4.. byte offsets from the pack.
// FF3 0x0019e028; FF2 GetAddrPK2 (FF3 drops the count check).

static u_int *GetAddrPK2(u_int *top, int no)
{
    if (top == nullptr)
        return nullptr;

    return (u_int *)((u_char *)top + top[no + 4]);
}

// ──────────────────────────────────────────────────────────────────────
// Curve primitives

// De Casteljau: lerp neighbouring control points in place until one is
// left. The lerp scales xyz only; w is set to 1 at the end.
// FF3 0x0019e048; FF2 CalcBezierPoint.

void CalcBezierPoint(float *Point, int n, float t, sceVu0FVECTOR *pControlPoint)
{
    sceVu0FVECTOR WorkVector[EVECAM_BEZIER_POINT_MAX];
    sceVu0FVECTOR TmpVector;

    for (int i = 0; i <= n; i++)
        EventCameraCopyVector(WorkVector[i], pControlPoint[i]);

    for (int m = 1; m <= n; m++)
    {
        for (int i = 0; i <= n - m; i++)
        {
            sceVu0SubVector(TmpVector, WorkVector[i + 1], WorkVector[i]);
            sceVu0ScaleVectorXYZ(TmpVector, TmpVector, t);
            sceVu0AddVector(WorkVector[i], WorkVector[i], TmpVector);
        }
    }

    EventCameraCopyVector(Point, WorkVector[0]);
    Point[3] = 1.0f;
}

// HokanType 2: a cubic Hermite through the segment, tangents from the
// neighbouring points.
// FF3 0x0019e168; FF2 EventCameraSetHermite.

static void EventCameraSetHermite(float Dist, VCI_CAMERA_HEADER *pHead, int PointNo,
                                  int RoomNo, int PointType)
{
    VCI_CAMERA_POINT *pEveCam;
    sceVu0FVECTOR     TmpVec;
    sceVu0FVECTOR     StartPos;
    sceVu0FVECTOR     EndPos;
    sceVu0FVECTOR     DirVector0;
    sceVu0FVECTOR     DirVector1;
    float             PointDist;
    float             Point2PointDist;
    float             Progress;

    if (pHead == nullptr)
        return;

    pEveCam = EventCameraPoints(pHead);

    if (PointType == EVECAM_POINT_POSITION)
    {
        EventCameraCopyVector(StartPos, pEveCam[PointNo].LocalPosition);
        EventCameraCopyVector(EndPos, pEveCam[PointNo + 1].LocalPosition);
        EventCameraGetMoveDistToPoint(&PointDist, nullptr, pHead, PointNo);
    }
    else
    {
        EventCameraCopyVector(StartPos, pEveCam[PointNo].LocalTarget);
        EventCameraCopyVector(EndPos, pEveCam[PointNo + 1].LocalTarget);
        EventCameraGetMoveDistToPoint(nullptr, &PointDist, pHead, PointNo);
    }

    Point2PointDist = EventCamera2PLength(EndPos, StartPos);
    Progress = 0.0f;
    if (Point2PointDist != 0.0f)
        Progress = (Dist - PointDist) / Point2PointDist;

    EventCameraGetHermiteDirVector(DirVector0, pEveCam, PointNo, pHead->DataNum, PointType);
    EventCameraGetHermiteDirVector(DirVector1, pEveCam, PointNo + 1, pHead->DataNum, PointType);

    CalcHermitePoint(TmpVec, StartPos, EndPos, DirVector0, DirVector1, Progress);

    if (PointType == EVECAM_POINT_POSITION)
        MapCamGra3dcamSetPositionAddOffset(TmpVec, RoomNo);
    else
        MapCamGra3dcamSetTargetAddOffset(TmpVec, RoomNo);
}

// A central difference inside the path, one-sided (not halved) at the ends.
// FF3 0x0019e328; FF2 EventCameraGetHermiteDirVector.

static void EventCameraGetHermiteDirVector(float *DirVector, VCI_CAMERA_POINT *pEveCam,
                                           int PointNo, int DataNum, int PointType)
{
    int   StartNo;
    int   EndNo;
    float ScaleVal = 1.0f;

    if (PointNo == 0)
    {
        StartNo = 0;
        EndNo = 1;
    }
    else if (PointNo == DataNum - 1)
    {
        StartNo = DataNum - 2;
        EndNo = PointNo;
    }
    else
    {
        StartNo = PointNo - 1;
        EndNo = PointNo + 1;
        ScaleVal = 0.5f;
    }

    if (PointType == EVECAM_POINT_POSITION)
        sceVu0SubVector(DirVector, pEveCam[EndNo].LocalPosition, pEveCam[StartNo].LocalPosition);
    else
        sceVu0SubVector(DirVector, pEveCam[EndNo].LocalTarget, pEveCam[StartNo].LocalTarget);

    sceVu0ScaleVector(DirVector, DirVector, ScaleVal);
}

// FF3 0x0019e3e8; FF2 CalcHermitePoint.

void CalcHermitePoint(float *HermiteVector, float *Point0, float *Point1,
                      float *Direction0, float *Direction1, float ParamU)
{
    sceVu0FVECTOR Hermite0;
    sceVu0FVECTOR Hermite1;
    sceVu0FVECTOR Hermite2;
    sceVu0FVECTOR Hermite3;
    float         ParamU2 = ParamU * ParamU;
    float         ParamU3 = ParamU2 * ParamU;
    float         ParamU2x3 = ParamU2 * 3.0f;

    sceVu0ScaleVector(Hermite0, Point0, ((ParamU3 + ParamU3) - ParamU2x3) + 1.0f);
    sceVu0ScaleVector(Hermite1, Point1, ParamU3 * -2.0f + ParamU2x3);
    sceVu0ScaleVector(Hermite2, Direction0, (ParamU3 - (ParamU2 + ParamU2)) + ParamU);
    sceVu0ScaleVector(Hermite3, Direction1, ParamU3 - ParamU2);

    sceVu0AddVector(HermiteVector, Hermite0, Hermite1);
    sceVu0AddVector(HermiteVector, HermiteVector, Hermite2);
    sceVu0AddVector(HermiteVector, HermiteVector, Hermite3);
}
