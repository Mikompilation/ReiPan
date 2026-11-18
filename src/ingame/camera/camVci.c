/* ==========================================================================
 *  ingame/camera/camVci.c
 *
 *  The event camera's VCI pack holder, FF3 0x139e10..0x13a010 (FF3-only,
 *  no FF2 counterpart). The pack VCI_CAMERA_DATA_PK4 is a PK4 of camera
 *  paths; SetEvCam resolves member no and starts it on the event camera
 *  (EventCameraVCIReqAddr raises MapCamSetEventCameraFlg(1), so
 *  EventCameraMain drives gra3dApplyCamera from then on), DelEvCam hands
 *  the camera back to the map camera.
 * ======================================================================== */

#include "ingame/camera/camVci.h"

#include "common/pk4.h"                 // Pk4GetData
#include "ingame/camera/event_camera.h" // EventCameraVCIReqAddr, EventCameraCut
#include "reipan/reipan_memory.h"       // PC: ReiPan_GetHostPointer
#include "system/eeiop/cd_file_dat.h"   // VCI_CAMERA_DATA_PK4
#include "system/os/eecdvd.h"           // LoadReqGetAddr, IsLoadEnd

// The pack's EE address (camVci::Load).
#define CAMVCI_PACK_ADDR 0xfcc000

// The event camera's pack holder (data 0x390ff0), built and destroyed by
// camVci_static_init 0x139f58 (_GLOBAL__I_ 0x139fd0 / _GLOBAL__D_ 0x139ff0).
static camVci ev_cam_vci;

// FF3 0x00139e10 (camVciCtor).

camVci::camVci()
{
    addr    = nullptr;
    load_id = 0;
}

// FF3 0x00139e50 (complete; base 0x00139e28, deleting 0x00139e78).

camVci::~camVci()
{
    Release();
}

// FF3 0x00139eb0.

camVci *GetEvCamVci(void)
{
    return &ev_cam_vci;
}

// FF3 0x00139ec0 (EventDataLoad).

void camVci::Load()
{
    // PC: the fixed EE address goes through the emulated RAM block.
    addr = ReiPan_GetHostPointer(CAMVCI_PACK_ADDR);
    LoadReqGetAddr(VCI_CAMERA_DATA_PK4, addr, &load_id);
}

// FF3 0x00139ef0.

int camVci::IsLoadEnd()
{
    return ::IsLoadEnd(load_id);
}

// FF3 0x00139f08 (EventRelease, the destructors).

void camVci::Release()
{
    if (addr != nullptr) {
        addr = nullptr;
    }
    load_id = 0;
}

// SET_EV_CAM_VCI: member no of the pack is the path to play.
// FF3 0x00139f20.

void camVci::SetEvCam(int no)
{
    u_int *path = (u_int *)Pk4GetData(addr, no);

    // PC guard: FF3 passes a missing member (null) on and reads EE 0;
    // here that would fault, so the camera is left as it is.
    if (path == nullptr) {
        return;
    }
    EventCameraVCIReqAddr(path);
}

// DELETE_EV_CAM_VCI.
// FF3 0x00139f40.

void camVci::DelEvCam()
{
    EventCameraCut();
}
