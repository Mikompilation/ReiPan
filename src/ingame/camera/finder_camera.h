/* ==========================================================================
 *  ingame/camera/finder_camera.h
 *
 *  Finder camera (file inferred from link order). Minimal so far: what
 *  reconstructed code calls.
 * ======================================================================== */
#ifndef _INGAME_CAMERA_FINDER_CAMERA_H
#define _INGAME_CAMERA_FINDER_CAMERA_H

#include <stddef.h>                     // offsetof
#include "sdk/scetypes.h"
#include "ingame/mission/mission_list.h"    // the mission list API (was declared here)
#include "ingame/mission/mission_end.h"     // MissionLoadSetHooks (was FUN_001ad0d0 here)
#include "ingame/mission/mission_mode.h"    // FINDER_MODE_WRK (was declared here)

// The document flags (FinderCamFlag* until 2026-10-20) are DocFlag* in
// ingame/story_menu_photo.h.

// FINDER_MODE_WRK (the running extra mission) and GetFinderModeWrk are in
// ingame/mission/mission_mode.h.

// The mission loading layout's hooks (FF3 0x001ad0d0) are MissionLoadSetHooks
// in ingame/mission/mission_end.h.

// The start menu's setup layout hooks and cursor filters (FF3 0x1af6f0..)
// are in ingame/menu/start_menu_top.h.

#endif /* _INGAME_CAMERA_FINDER_CAMERA_H */
