/* ==========================================================================
 *  ingame/camera/camVci.h
 *
 *  camVci (FF3 RTTI class, vtable 0x3413d0): the holder of the event
 *  camera's VCI pack, VCI_CAMERA_DATA_PK4 loaded to EE 0xfcc000. FF3-only
 *  (FF2's event camera read its pack at a fixed address itself). The one
 *  in use is data 0x390ff0 (GetEvCamVci); ev_main.c holds a second, unused
 *  one (bss 0x3e3ff8). EventDataLoad loads the pack, SET_EV_CAM_VCI hands
 *  member no to the event camera, DELETE_EV_CAM_VCI drops it.
 *  FF3 0x139e10..0x13a010 (the file's start is a guess, see tus.json).
 * ======================================================================== */
#ifndef _INGAME_CAMERA_CAMVCI_H
#define _INGAME_CAMERA_CAMVCI_H

#include <stddef.h>                     // offsetof

// 0x0c bytes on the EE.
// layout: host -- the vptr and addr are 4 bytes on the EE: addr at EE +0x04,
// load_id +0x08.
class camVci
{
public:
    camVci();                           // FF3 0x00139e10 (camVciCtor): addr and load_id 0
    virtual ~camVci();                  // EE vtable +0x00 0x00139e50 (base 0x00139e28), deleting +0x04 0x00139e78: Release

    void Load();                        // FF3 0x00139ec0: VCI_CAMERA_DATA_PK4 to EE 0xfcc000
    int  IsLoadEnd();                   // FF3 0x00139ef0
    void Release();                     // FF3 0x00139f08: addr and load_id 0
    void SetEvCam(int no);              // FF3 0x00139f20: pack member no to the event camera
    void DelEvCam();                    // FF3 0x00139f40: back to the map camera

    void *addr;                         // EE +0x04: the pack (EE 0xfcc000), nullptr when released
    int   load_id;                      // EE +0x08: LoadReqGetAddr's request id
};
static_assert(offsetof(camVci, load_id) == offsetof(camVci, addr) + sizeof(void *), "camVci.load_id follows addr");

camVci *GetEvCamVci(void);              // FF3 0x00139eb0: data 0x390ff0

#endif /* _INGAME_CAMERA_CAMVCI_H */
