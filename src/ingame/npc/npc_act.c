// ingame/npc/npc_act.c -- the companion NPCs' act machine and data (FF3
// 0x21a678..0x21d330). FF3-only (Ghidra's FF2 matches
// here, ConvertGage2StockNum and the n_equip_tray.c slot, were wrong).
// An NPC's act block runs a byte-code script from NPC_OBJ (EE 0x12f7000):
// PlyrCameraActRun counts the block's wait down, then calls the op function
// for each op until one waits. The ops are three ROM tables (0x32dac0:
// 0..6 flow and variables, 0x32dae0: 0x20..0x30 conditional jumps, 0x32db28:
// 0x40..0x4c the act hooks of n_plyr_camera.c). The rest is the NPCs' data
// (position labels, motion and door lists, serif lines, talk cameras, NPC
// 0's area remap, the face of each motion) and the helpers the hooks call.
// Reconstructed from FF3; nothing came from FF2.

#include <math.h>                       // fabsf, sqrtf
#include <string.h>                     // memset

#include "ingame/npc/npc_act.h"
#include "ingame/map/MapCtrl.h"              // MapAreaHitLineCheck
#include "common/vu0math.h"             // RotLimitChk, GetRandValIByF, GetTrgtRot, NormalizeAngle
#include "ingame/camera/event_camera.h" // EventCamera*, EventCameraCut
#include "ingame/cat/cat_ctrl.h"        // CAT_WRK, CatGetWrk, CatIsHeld, CatSetFixPos
#include "ingame/enemy/enemy.h"         // CheckEneView
#include "ingame/ingame_wrk.h"          // ingame_wrk
#include "ingame/map/MapLoad.h"         // MapLoadGetRoomArea, MapLoadCheckDrawFlg
#include "ingame/map/map_rectangle.h"   // MrecIsInEvent
#include "ingame/story_door.h"      // MapAreaSideGet
#include "ingame/menu/anim_2d.h"        // LayoutAnim*, Anim2dMsg*
#include "ingame/photo/n_equip_tray.h"  // PlyrCameraLabelIsPlyrArea, PlyrCameraGetLabel84, PlyrCameraMotCall
#include "ingame/photo/n_plyr_camera.h" // the NPC works and the act hooks
#include "ingame/plyr/player.h"         // PLYR_WRK, GetPlyrAreaNo, PlyrLineHitCheck, PlyrSetPos / Rot, SetPlyrAnime
#include "ingame/plyr/plyr_sub.h"       // AllPlyrIsReady, PlyrGetJointPos, PlyrGetJointRotY
#include "ingame/subtitle/subtitle.h"   // SubTitleReq3D, SubTitleReq, SubTitleIsEnd, SubTitleIsExec
#include "reipan/os/reipan_rand.h"      // PC: ReiPan_Rand
#include "reipan/reipan_memory.h"       // ReiPan_GetHostPointer

// ──────────────────────────────────────────────────────────────────────
// Callees in runs that are not defined yet. Move each block to its real
// header when that run is defined.
// ──────────────────────────────────────────────────────────────────────

// unassigned@0011cea8

// ──────────────────────────────────────────────────────────────────────
// The data (generated from the ROM: the tables keep its order and values).

// data 0x35d980: NPC 0's position labels
static const NPC_LABEL npc_label0[20] =
{
    {   // 0
        0, { 0x00, 0x00, 0x00 }, 48, { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
        { 141.2743f, -0.3f, 253.029f, 90.0f },
        0, { 0x00, 0x00, 0x00 }, { 1005, 1009 }, 0,
        { 170.0f, 0.0f, 256.0f, 246.0f },
        -1, -1, { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
        { -1.0f, -1.0f, -1.0f, -1.0f },
        0xff, { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
        { 170.0f, 0.0f, 256.0f, 246.0f },
        2, 3, 0, 6, 0x00, -1, { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
    },
    {   // 1
        0, { 0x00, 0x00, 0x00 }, 48, { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
        { 133.8247f, -0.3f, 259.943f, 90.0f },
        0, { 0x00, 0x00, 0x00 }, { 3005, 1007 }, 0,
        { 93.0f, 0.0f, 279.2f, 90.0f },
        8, 9, { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
        { 143.6513f, -0.3f, 271.185f, 180.0f },
        0x00, { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
        { 148.0f, 0.0f, 275.0f, 222.0f },
        2, 3, 0, -1, 0x00, -1, { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
    },
    {   // 2
        0, { 0x00, 0x00, 0x00 }, 48, { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
        { 152.5565f, -0.3f, 243.616f, 0.0f },
        1, { 0x00, 0x00, 0x00 }, { 1005, 1009 }, 0,
        { 170.0f, 0.0f, 256.0f, 246.0f },
        -1, -1, { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
        { -1.0f, -1.0f, -1.0f, -1.0f },
        0xff, { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
        { 170.0f, 0.0f, 256.0f, 246.0f },
        4, 5, 0, -1, 0x00, 0, { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
    },
    {   // 3
        0, { 0x00, 0x00, 0x00 }, 48, { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
        { 132.5576f, -0.3f, 263.703f, 0.0f },
        2, { 0x00, 0x00, 0x00 }, { -1, -1 }, 0,
        { -1.0f, -1.0f, -1.0f, -1.0f },
        -1, -1, { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
        { -1.0f, -1.0f, -1.0f, -1.0f },
        0xff, { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
        { -1.0f, -1.0f, -1.0f, -1.0f },
        -1, -1, -1, -1, 0x01, -1, { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
    },
    {   // 4
        0, { 0x00, 0x00, 0x00 }, 48, { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
        { 92.88f, -0.6f, 174.117f, 180.0f },
        3, { 0x00, 0x00, 0x00 }, { 1006, 1010 }, 0,
        { 175.0f, 0.0f, 193.0f, 270.0f },
        14, 15, { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
        { 112.6146f, 0.0f, 177.727f, 180.0f },
        0x00, { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
        { 107.0f, 0.0f, 178.0f, 253.0f },
        12, 11, 1, -1, 0x00, -1, { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
    },
    {   // 5
        0, { 0x00, 0x00, 0x00 }, 48, { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
        { 107.5793f, 0.0f, 212.751f, 180.0f },
        4, { 0x00, 0x00, 0x00 }, { 1001, 1016 }, 0,
        { 160.0f, 0.0f, 197.0f, 0.0f },
        -1, -1, { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
        { -1.0f, -1.0f, -1.0f, -1.0f },
        0xff, { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
        { 160.0f, 0.0f, 197.0f, 0.0f },
        19, -1, 2, 22, 0x00, -1, { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
    },
    {   // 6
        0, { 0x00, 0x00, 0x00 }, 48, { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
        { 115.5f, -0.6f, 241.8f, -90.0f },
        4, { 0x00, 0x00, 0x00 }, { 2001, 1011 }, 0,
        { 98.0f, 0.0f, 242.0f, 90.0f },
        -1, -1, { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
        { -1.0f, -1.0f, -1.0f, -1.0f },
        0xff, { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
        { 98.0f, 0.0f, 242.0f, 90.0f },
        19, -1, 3, -1, 0x00, -1, { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
    },
    {   // 7
        0, { 0x00, 0x00, 0x00 }, 48, { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
        { 148.7716f, -0.9f, 286.767f, 0.0f },
        5, { 0x00, 0x00, 0x00 }, { 2001, 1007 }, 0,
        { 87.37f, 0.0f, 279.2f, 90.0f },
        25, 17, { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
        { 148.7716f, -0.9f, 285.767f, 0.0f },
        0x00, { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
        { 130.0f, 0.0f, 284.0f, 90.0f },
        20, -1, 4, -1, 0x00, -1, { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
    },
    {   // 8
        0, { 0x00, 0x00, 0x00 }, 48, { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
        { 108.4521f, 0.0f, 211.166f, 0.0f },
        6, { 0x00, 0x00, 0x00 }, { 2001, 1012 }, 0,
        { 107.0f, 0.0f, 238.0f, 180.0f },
        -1, -1, { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
        { -1.0f, -1.0f, -1.0f, -1.0f },
        0xff, { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
        { 107.0f, 0.0f, 238.0f, 180.0f },
        21, -1, 5, -1, 0x00, -1, { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
    },
    {   // 9
        0, { 0x00, 0x00, 0x00 }, 50, { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
        { 81.9403f, -51.01f, 113.3065f, 0.0f },
        7, { 0x00, 0x00, 0x00 }, { 5001, 5002 }, 0,
        { 82.0f, -49.0f, 137.0f, 180.0f },
        34, 35, { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
        { -1.0f, -1.0f, -1.0f, -1.0f },
        0xff, { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
        { 84.0f, -49.0f, 130.0f, 192.0f },
        30, 31, 6, -1, 0x00, -1, { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
    },
    {   // 10
        0, { 0x00, 0x00, 0x00 }, 50, { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
        { 81.9403f, -51.01f, 113.3065f, 0.0f },
        8, { 0x00, 0x00, 0x00 }, { 5001, 5002 }, 0,
        { 82.0f, -49.0f, 137.0f, 180.0f },
        -1, -1, { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
        { -1.0f, -1.0f, -1.0f, -1.0f },
        0xff, { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
        { 84.0f, -49.0f, 130.0f, 192.0f },
        32, 33, 6, -1, 0x00, 1, { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
    },
    {   // 11
        0, { 0x00, 0x00, 0x00 }, 50, { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
        { 24.3002f, -50.11f, 136.5948f, 270.0f },
        10, { 0x00, 0x00, 0x00 }, { 5001, 5003 }, 0,
        { 46.0f, -49.0f, 132.0f, 245.0f },
        -1, -1, { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
        { -1.0f, -1.0f, -1.0f, -1.0f },
        0xff, { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
        { 46.0f, -49.0f, 132.0f, 245.0f },
        38, -1, 7, -1, 0x00, -1, { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
    },
    {   // 12
        1, { 0x00, 0x00, 0x00 }, 48, { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
        { 152.5565f, -0.3f, 243.616f, 0.0f },
        1, { 0x00, 0x00, 0x00 }, { 1005, 1009 }, 0,
        { 170.0f, 0.0f, 256.0f, 246.0f },
        -1, -1, { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
        { -1.0f, -1.0f, -1.0f, -1.0f },
        0xff, { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
        { 170.0f, 0.0f, 256.0f, 246.0f },
        4, 5, 0, -1, 0x00, 0, { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
    },
    {   // 13
        1, { 0x00, 0x00, 0x00 }, 48, { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
        { 132.5576f, -0.3f, 263.703f, 0.0f },
        2, { 0x00, 0x00, 0x00 }, { -1, -1 }, 0,
        { -1.0f, -1.0f, -1.0f, -1.0f },
        -1, -1, { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
        { -1.0f, -1.0f, -1.0f, -1.0f },
        0xff, { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
        { -1.0f, -1.0f, -1.0f, -1.0f },
        -1, -1, -1, -1, 0x01, -1, { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
    },
    {   // 14
        1, { 0x00, 0x00, 0x00 }, 48, { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
        { 148.7716f, -0.9f, 286.767f, 0.0f },
        5, { 0x00, 0x00, 0x00 }, { 2001, 1007 }, 0,
        { 87.37f, 0.0f, 279.2f, 90.0f },
        25, 17, { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
        { 148.7716f, -0.9f, 285.767f, 0.0f },
        0x00, { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
        { 130.0f, 0.0f, 284.0f, 90.0f },
        20, -1, 4, -1, 0x00, -1, { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
    },
    {   // 15
        1, { 0x00, 0x00, 0x00 }, 50, { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
        { 81.9403f, -51.01f, 113.3065f, 0.0f },
        7, { 0x00, 0x00, 0x00 }, { 5001, 5002 }, 0,
        { 82.0f, -49.0f, 137.0f, 180.0f },
        34, 35, { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
        { -1.0f, -1.0f, -1.0f, -1.0f },
        0xff, { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
        { 84.0f, -49.0f, 130.0f, 192.0f },
        30, 31, 6, -1, 0x00, -1, { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
    },
    {   // 16
        1, { 0x00, 0x00, 0x00 }, 50, { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
        { 81.9403f, -51.01f, 113.3065f, 0.0f },
        8, { 0x00, 0x00, 0x00 }, { 5001, 5002 }, 0,
        { 82.0f, -49.0f, 137.0f, 180.0f },
        -1, -1, { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
        { -1.0f, -1.0f, -1.0f, -1.0f },
        0xff, { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
        { 84.0f, -49.0f, 130.0f, 192.0f },
        32, 33, 6, -1, 0x00, 1, { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
    },
    {   // 17
        1, { 0x00, 0x00, 0x00 }, 50, { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
        { 81.74f, -49.9f, 108.58f, 0.0f },
        9, { 0x00, 0x00, 0x00 }, { -1, -1 }, 0,
        { -1.0f, -1.0f, -1.0f, -1.0f },
        -1, -1, { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
        { -1.0f, -1.0f, -1.0f, -1.0f },
        0xff, { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
        { -1.0f, -1.0f, -1.0f, -1.0f },
        -1, -1, -1, -1, 0x02, -1, { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
    },
    {   // 18
        1, { 0x00, 0x00, 0x00 }, 50, { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
        { 24.3002f, -50.11f, 136.5948f, 270.0f },
        10, { 0x00, 0x00, 0x00 }, { 5001, 5003 }, 0,
        { 46.0f, -49.0f, 132.0f, 245.0f },
        -1, -1, { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
        { -1.0f, -1.0f, -1.0f, -1.0f },
        0xff, { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
        { 46.0f, -49.0f, 132.0f, 245.0f },
        38, -1, 7, -1, 0x00, -1, { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
    },
    {   // 19
        1, { 0x00, 0x00, 0x00 }, 50, { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
        { 23.7664f, -50.11f, 136.5965f, 270.0f },
        11, { 0x00, 0x00, 0x00 }, { -1, -1 }, 0,
        { -1.0f, -1.0f, -1.0f, -1.0f },
        -1, -1, { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
        { -1.0f, -1.0f, -1.0f, -1.0f },
        0xff, { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
        { -1.0f, -1.0f, -1.0f, -1.0f },
        -1, -1, -1, -1, 0x01, -1, { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
    },
};

// data 0x35f330: NPC 1's position labels
static const NPC_LABEL npc_label1[2] =
{
    {   // 0
        0, { 0x00, 0x00, 0x00 }, 48, { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
        { 155.2547f, 0.0f, 244.651f, 0.0f },
        0, { 0x00, 0x00, 0x00 }, { 1005, 1009 }, 0,
        { 170.0f, 0.0f, 256.0f, 246.0f },
        -1, -1, { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
        { -1.0f, -1.0f, -1.0f, -1.0f },
        0xff, { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
        { 170.0f, 0.0f, 256.0f, 246.0f },
        4, -1, 0, -1, 0x00, -1, { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
    },
    {   // 1
        1, { 0x00, 0x00, 0x00 }, 48, { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
        { 148.05f, -0.2f, 242.55f, 176.0f },
        2, { 0x00, 0x00, 0x00 }, { -1, -1 }, 0,
        { -1.0f, -1.0f, -1.0f, -1.0f },
        -1, -1, { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
        { -1.0f, -1.0f, -1.0f, -1.0f },
        0xff, { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
        { -1.0f, -1.0f, -1.0f, -1.0f },
        -1, -1, -1, -1, 0x01, -1, { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
    },
};

// motion lists (PlyrCameraGetMotList): NPC 0 data 0x32db60 / counts 0x35e4c0
static const s_char npc_mot0_0[1] = { 0 };   // 0x3714e0
static const s_char npc_mot0_1[1] = { 1 };   // 0x3714e8
static const s_char npc_mot0_2[1] = { 10 };   // 0x3714f0
static const s_char npc_mot0_3[1] = { 11 };   // 0x3714f8
static const s_char npc_mot0_4[1] = { 16 };   // 0x371500
static const s_char npc_mot0_5[1] = { 17 };   // 0x371508
static const s_char npc_mot0_6[1] = { 18 };   // 0x371510
static const s_char npc_mot0_7[1] = { 28 };   // 0x371518
static const s_char npc_mot0_8[1] = { 29 };   // 0x371520
static const s_char npc_mot0_9[1] = { 0 };   // 0x371528
static const s_char npc_mot0_10[1] = { 37 };   // 0x371530
static const s_char npc_mot0_11[1] = { 40 };   // 0x371538
// NPC 1: data 0x32dc20 / counts sdata 0x3715f0
static const s_char npc_mot1_0[4] = { 0, 1, 2, 3 };   // 0x3715d8
static const s_char npc_mot1_1[3] = { 11, 12, 13 };   // 0x3715e0
static const s_char npc_mot1_2[1] = { 16 };   // 0x3715e8
static const s_char *const npc_mot_tbl0[12] = { npc_mot0_0, npc_mot0_1, npc_mot0_2, npc_mot0_3, npc_mot0_4, npc_mot0_5, npc_mot0_6, npc_mot0_7, npc_mot0_8, npc_mot0_9, npc_mot0_10, npc_mot0_11 };
static const s_char *const npc_mot_tbl1[3] = { npc_mot1_0, npc_mot1_1, npc_mot1_2 };
static const s_char npc_mot_num0[12] = { 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1 };
static const s_char npc_mot_num1[3] = { 4, 3, 1 };

// door-rectangle lists (PlyrCameraGetDoorList): NPC 0 data 0x32db90 / counts sdata 0x371578
static const int npc_door0_0[1] = { 1004 };   // 0x371540
static const int npc_door0_1[2] = { 1000, 1018 };   // 0x371548
static const int npc_door0_2[1] = { 1017 };   // 0x371550
static const int npc_door0_3[1] = { 1002 };   // 0x371558
static const int npc_door0_4[1] = { 1013 };   // 0x371560
static const int npc_door0_5[1] = { 1002 };   // 0x371568
static const int npc_door0_6[3] = { 5001, 5002, 5004 };   // 0x35e4d0
static const int npc_door0_7[2] = { 5003, 5004 };   // 0x371570
// NPC 1: sdata 0x371428 / counts sdata 0x371608
static const int npc_door1_0[1] = { 1004 };   // 0x3715f8
static const int npc_door1_1[2] = { 1041, 1043 };   // 0x371600
static const int *const npc_door_tbl0[8] = { npc_door0_0, npc_door0_1, npc_door0_2, npc_door0_3, npc_door0_4, npc_door0_5, npc_door0_6, npc_door0_7 };
static const int *const npc_door_tbl1[2] = { npc_door1_0, npc_door1_1 };
static const s_char npc_door_num0[8] = { 1, 2, 1, 1, 1, 1, 3, 2 };
static const s_char npc_door_num1[2] = { 1, 2 };

// the face per motion (PlyrCameraGetMotFace): data 0x32dc48 -> NPC 0, NPC 0
// with the cat, NPC 1; then NPC_FACIAL_TBL. Each array is its whole data
// slot (counts sdata 0x371580 / 0x371598 / 0x371610 are 43 / 1 / 17, not
// checked by the game)
static const s_char npc_face0_0[48] = { -1, 0, -1, -1, -1, -1, -1, -1, -1, -1, 24, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 0, -1, -1, -1, -1, -1, -1, -1, 0, -1, -1, 24, 24, 24, 0, 0, 0, 0, 0 };   // 0x35e4e0
static const s_char npc_face0_1[48] = { 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 24, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 24, 24, 24, 0, 0, 0, 0, 0 };   // 0x35e510
static const s_char *const npc_face0_tbl[2] = { npc_face0_0, npc_face0_1 };
static const s_char npc_face_cat_0[8] = { 24, 0, 0, 0, 0, 0, 0, 0 };   // 0x371588
static const s_char npc_face_cat_1[8] = { 24, 0, 0, 0, 0, 0, 0, 0 };   // 0x371590
static const s_char *const npc_face_cat_tbl[2] = { npc_face_cat_0, npc_face_cat_1 };
static const s_char npc_face1_0[24] = { 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 25, 0, 0, 0, 0, 0, 0, 0 };   // 0x35f450
static const s_char *const npc_face1_tbl[1] = { npc_face1_0 };
static const s_char *const *const npc_face_tbl[3] = { npc_face0_tbl, npc_face_cat_tbl, npc_face1_tbl };

// serif lines of NPC 0 (PlyrCameraGetSerifList): kinds data 0x32dc68, counts 0x32dc80
static const NPC_SERIF npc_serif0_0_0[2] = { { 0, 4, -2 }, { 0, 5, -2 } };   // 0x35e540
static const NPC_SERIF npc_serif0_0_1[2] = { { 0, 7, -2 }, { 0, 8, -2 } };   // 0x35e558
static const NPC_SERIF npc_serif0_0_2[1] = { { 0, 9, -2 } };   // 0x35e570
static const NPC_SERIF npc_serif0_0_3[2] = { { 0, 10, -2 }, { 0, 12, -2 } };   // 0x35e580
static const NPC_SERIF npc_serif0_0_4[1] = { { 0, 11, -2 } };   // 0x35e598
static const NPC_SERIF npc_serif0_0_5[1] = { { 0, -1, -2 } };   // 0x35e5a8
static const NPC_SERIF *const npc_serif0_0[6] = { npc_serif0_0_0, npc_serif0_0_1, npc_serif0_0_2, npc_serif0_0_3, npc_serif0_0_4, npc_serif0_0_5 };
static const s_char npc_serif0_0_num[6] = { 2, 2, 1, 2, 1, 1 };
static const NPC_SERIF npc_serif0_1_0[2] = { { 0, 13, 15 }, { 0, 13, 16 } };   // 0x35e5b8
static const NPC_SERIF npc_serif0_1_1[2] = { { 0, 14, 16 }, { 0, 14, 15 } };   // 0x35e5d0
static const NPC_SERIF npc_serif0_1_2[2] = { { 0, 14, 17 }, { 0, 14, 23 } };   // 0x35e5e8
static const NPC_SERIF *const npc_serif0_1[3] = { npc_serif0_1_0, npc_serif0_1_1, npc_serif0_1_2 };
static const s_char npc_serif0_1_num[3] = { 2, 2, 2 };
static const NPC_SERIF npc_serif0_2_0[1] = { { 0, 19, 21 } };   // 0x35e600
static const NPC_SERIF npc_serif0_2_1[1] = { { 0, 18, 21 } };   // 0x35e610
static const NPC_SERIF npc_serif0_2_2[2] = { { 0, 20, 22 }, { 0, 12, 22 } };   // 0x35e620
static const NPC_SERIF *const npc_serif0_2[3] = { npc_serif0_2_0, npc_serif0_2_1, npc_serif0_2_2 };
static const s_char npc_serif0_2_num[3] = { 1, 1, 2 };
static const NPC_SERIF npc_serif0_3_0[2] = { { 0, 24, -2 }, { 0, 26, -2 } };   // 0x35e638
static const NPC_SERIF npc_serif0_3_1[2] = { { 0, 24, -2 }, { 0, 25, -2 } };   // 0x35e650
static const NPC_SERIF npc_serif0_3_2[2] = { { 0, 25, -2 }, { 0, 28, -2 } };   // 0x35e668
static const NPC_SERIF *const npc_serif0_3[3] = { npc_serif0_3_0, npc_serif0_3_1, npc_serif0_3_2 };
static const s_char npc_serif0_3_num[3] = { 2, 2, 2 };
static const NPC_SERIF npc_serif0_4_0[1] = { { 0, 25, -2 } };   // 0x35e680
static const NPC_SERIF npc_serif0_4_1[3] = { { 0, 23, -2 }, { 0, 25, -2 }, { 0, 27, -2 } };   // 0x35e690
static const NPC_SERIF npc_serif0_4_2[2] = { { 0, 23, -2 }, { 0, 27, -2 } };   // 0x35e6b8
static const NPC_SERIF *const npc_serif0_4[3] = { npc_serif0_4_0, npc_serif0_4_1, npc_serif0_4_2 };
static const s_char npc_serif0_4_num[3] = { 1, 3, 2 };
static const NPC_SERIF npc_serif0_5_0[2] = { { 0, 30, -2 }, { 0, 31, -2 } };   // 0x35e6d0
static const NPC_SERIF *const npc_serif0_5[1] = { npc_serif0_5_0 };
static const s_char npc_serif0_5_num[1] = { 2 };
static const NPC_SERIF *const *const npc_serif_tbl0[6] = { npc_serif0_0, npc_serif0_1, npc_serif0_2, npc_serif0_3, npc_serif0_4, npc_serif0_5 };
static const s_char *const npc_serif_num0[6] = { npc_serif0_0_num, npc_serif0_1_num, npc_serif0_2_num, npc_serif0_3_num, npc_serif0_4_num, npc_serif0_5_num };

// serif lines of NPC 1 (PlyrCameraGetSerifList): kinds data 0x32dc98, counts 0x32dcb0
static const NPC_SERIF npc_serif1_0_0[2] = { { 0, 32, -2 }, { 0, 33, -2 } };   // 0x35f468
static const NPC_SERIF *const npc_serif1_0[1] = { npc_serif1_0_0 };
static const s_char npc_serif1_0_num[1] = { 2 };
static const NPC_SERIF npc_serif1_1_0[1] = { { 0, -1, -1 } };   // 0x35f480
static const NPC_SERIF *const npc_serif1_1[1] = { npc_serif1_1_0 };
static const s_char npc_serif1_1_num[1] = { 1 };
static const NPC_SERIF npc_serif1_2_0[1] = { { 0, -1, -1 } };   // 0x35f490
static const NPC_SERIF *const npc_serif1_2[1] = { npc_serif1_2_0 };
static const s_char npc_serif1_2_num[1] = { 1 };
static const NPC_SERIF npc_serif1_3_0[2] = { { 0, 34, -2 }, { 0, 35, -2 } };   // 0x35f4a0
static const NPC_SERIF *const npc_serif1_3[1] = { npc_serif1_3_0 };
static const s_char npc_serif1_3_num[1] = { 2 };
static const NPC_SERIF npc_serif1_4_0[2] = { { 0, 36, -2 }, { 0, 37, -2 } };   // 0x35f4b8
static const NPC_SERIF *const npc_serif1_4[1] = { npc_serif1_4_0 };
static const s_char npc_serif1_4_num[1] = { 2 };
static const NPC_SERIF npc_serif1_5_0[2] = { { 0, 38, -1 }, { 0, 39, -1 } };   // 0x35f4d0
static const NPC_SERIF *const npc_serif1_5[1] = { npc_serif1_5_0 };
static const s_char npc_serif1_5_num[1] = { 2 };
static const NPC_SERIF *const *const npc_serif_tbl1[6] = { npc_serif1_0, npc_serif1_1, npc_serif1_2, npc_serif1_3, npc_serif1_4, npc_serif1_5 };
static const s_char *const npc_serif_num1[6] = { npc_serif1_0_num, npc_serif1_1_num, npc_serif1_2_num, npc_serif1_3_num, npc_serif1_4_num, npc_serif1_5_num };

// event cameras of NPC 0 (PlyrCameraGetCamList): data 0x32dc08, counts sdata 0x3715d0
static const NPC_EV_CAM npc_cam0_0[16] =   // 0x35e6e8
{
    { 0, { 0x00, 0x00, 0x00 }, { 52.51f, -44.37f, 288.67f }, { 137.84f, -11.58f, 256.04f }, 0.47f, 0.0f },
    { 1, { 0x00, 0x00, 0x00 }, { 158.54f, -77.11f, 200.95f }, { 130.41f, -9.29f, 263.97f }, 0.47f, 0.0f },
    { 2, { 0x00, 0x00, 0x00 }, { 80.54f, -35.01f, 285.13f }, { 144.79f, -13.12f, 253.59f }, 0.63f, 0.0f },
    { 4, { 0x00, 0x00, 0x00 }, { 128.7f, -30.41f, 223.79f }, { 72.09f, -22.82f, 145.72f }, 0.59f, 0.0f },
    { 6, { 0x00, 0x00, 0x00 }, { 73.47f, -67.58f, 221.29f }, { 111.89f, -30.63f, 235.71f }, 0.68f, 0.0f },
    { 7, { 0x00, 0x00, 0x00 }, { 69.33f, -68.88f, 218.0f }, { 133.83f, -24.96f, 275.16f }, 0.46f, 0.0f },
    { 8, { 0x00, 0x00, 0x00 }, { 77.27f, -43.4f, 253.04f }, { 129.59f, -3.06f, 182.03f }, 0.52f, 0.0f },
    { 9, { 0x00, 0x00, 0x00 }, { 66.35f, -69.28f, 106.24f }, { 283.11f, -71.64f, 269.82f }, 0.81f, 0.0f },
    { 10, { 0x00, 0x00, 0x00 }, { 70.06f, -69.58f, 138.19f }, { 99.71f, -58.83f, 81.24f }, 0.75f, 0.0f },
    { 11, { 0x00, 0x00, 0x00 }, { 68.84f, -77.39f, 165.94f }, { 14.75f, -66.07f, 131.5f }, 0.7f, 0.0f },
    { 12, { 0x00, 0x00, 0x00 }, { 80.54f, -35.01f, 285.13f }, { 144.79f, -13.12f, 253.59f }, 0.63f, 0.0f },
    { 14, { 0x00, 0x00, 0x00 }, { 69.33f, -68.88f, 218.0f }, { 133.83f, -24.96f, 275.16f }, 0.46f, 0.0f },
    { 15, { 0x00, 0x00, 0x00 }, { 66.35f, -69.28f, 106.24f }, { 283.11f, -71.64f, 269.82f }, 0.81f, 0.0f },
    { 16, { 0x00, 0x00, 0x00 }, { 70.06f, -69.58f, 138.19f }, { 99.71f, -58.83f, 81.24f }, 0.75f, 0.0f },
    { 18, { 0x00, 0x00, 0x00 }, { 68.84f, -77.39f, 165.94f }, { 14.75f, -66.07f, 131.5f }, 0.7f, 0.0f },
    { 5, { 0x00, 0x00, 0x00 }, { 190.94f, -26.83f, 184.43f }, { 110.44f, -19.23f, 237.55f }, 0.7f, 0.0f },
};
static const NPC_EV_CAM npc_cam0_1[16] =   // 0x35e928
{
    { 0, { 0x00, 0x00, 0x00 }, { 127.33f, -15.65f, 252.17f }, { 195.76f, -26.31f, 245.75f }, 0.69f, 0.0f },
    { 1, { 0x00, 0x00, 0x00 }, { 81.64f, -29.27f, 271.61f }, { 171.62f, -13.78f, 301.94f }, 0.71f, 0.0f },
    { 2, { 0x00, 0x00, 0x00 }, { 172.45f, -37.15f, 285.76f }, { 159.21f, -14.88f, 226.03f }, 0.55f, 0.0f },
    { 4, { 0x00, 0x00, 0x00 }, { 193.22f, -23.68f, 212.28f }, { 113.2f, -28.74f, 158.14f }, 0.66f, 0.0f },
    { 6, { 0x00, 0x00, 0x00 }, { 87.21f, -33.21f, 234.34f }, { 172.04f, -0.9f, 267.85f }, 0.65f, 0.0f },
    { 7, { 0x00, 0x00, 0x00 }, { 74.28f, -28.45f, 266.66f }, { 156.51f, -20.77f, 316.32f }, 0.69f, 0.0f },
    { 8, { 0x00, 0x00, 0x00 }, { 123.18f, -27.9f, 246.91f }, { 59.78f, -28.74f, 173.83f }, 0.73f, 0.0f },
    { 9, { 0x00, 0x00, 0x00 }, { 71.1f, -80.37f, 152.11f }, { 90.7f, -72.43f, 108.81f }, 0.77f, 0.0f },
    { 10, { 0x00, 0x00, 0x00 }, { 100.23f, -74.19f, 152.03f }, { 78.23f, -62.14f, 110.89f }, 0.63f, 0.0f },
    { 11, { 0x00, 0x00, 0x00 }, { 61.45f, -80.59f, 132.14f }, { 10.85f, -75.45f, 123.37f }, 0.61f, 0.0f },
    { 12, { 0x00, 0x00, 0x00 }, { 172.45f, -37.15f, 285.76f }, { 159.21f, -14.88f, 226.03f }, 0.55f, 0.0f },
    { 14, { 0x00, 0x00, 0x00 }, { 75.47f, -32.35f, 267.38f }, { 154.33f, -4.06f, 315.0f }, 0.79f, 0.0f },
    { 15, { 0x00, 0x00, 0x00 }, { 71.1f, -80.37f, 152.11f }, { 90.7f, -72.43f, 108.81f }, 0.77f, 0.0f },
    { 16, { 0x00, 0x00, 0x00 }, { 100.23f, -74.19f, 152.03f }, { 78.23f, -62.14f, 110.89f }, 0.63f, 0.0f },
    { 18, { 0x00, 0x00, 0x00 }, { 61.45f, -80.59f, 132.14f }, { 10.85f, -75.45f, 123.37f }, 0.61f, 0.0f },
    { 5, { 0x00, 0x00, 0x00 }, { 178.3f, -29.07f, 177.15f }, { 124.49f, -17.27f, 256.68f }, 0.6f, 0.0f },
};
static const NPC_EV_CAM npc_cam0_2[16] =   // 0x35eb68
{
    { 0, { 0x00, 0x00, 0x00 }, { 178.45f, -34.93f, 274.31f }, { 143.26f, -5.17f, 224.06f }, 0.58f, 0.0f },
    { 1, { 0x00, 0x00, 0x00 }, { 164.03f, -31.07f, 274.49f }, { 106.51f, -7.35f, 260.14f }, 0.69f, 0.0f },
    { 2, { 0x00, 0x00, 0x00 }, { 172.45f, -37.15f, 285.76f }, { 159.21f, -14.88f, 226.03f }, 0.55f, 0.0f },
    { 4, { 0x00, 0x00, 0x00 }, { 120.05f, -25.78f, 206.6f }, { 70.3f, -26.62f, 123.65f }, 0.59f, 0.0f },
    { 6, { 0x00, 0x00, 0x00 }, { 87.21f, -33.21f, 234.34f }, { 172.04f, -0.9f, 267.85f }, 0.65f, 0.0f },
    { 7, { 0x00, 0x00, 0x00 }, { 108.85f, -23.22f, 233.55f }, { 137.97f, -20.44f, 286.42f }, 0.71f, 0.0f },
    { 8, { 0x00, 0x00, 0x00 }, { 94.62f, -28.2f, 253.53f }, { 141.47f, -20.61f, 169.21f }, 0.66f, 0.0f },
    { 9, { 0x00, 0x00, 0x00 }, { 106.66f, -86.33f, 152.64f }, { -47.9f, 15.17f, -45.89f }, 0.55f, 0.0f },
    { 10, { 0x00, 0x00, 0x00 }, { 70.7f, -82.24f, 142.81f }, { 192.68f, 30.13f, -71.92f }, 0.82f, 0.0f },
    { 11, { 0x00, 0x00, 0x00 }, { 61.45f, -80.59f, 132.14f }, { 10.85f, -75.45f, 123.37f }, 0.61f, 0.0f },
    { 12, { 0x00, 0x00, 0x00 }, { 172.45f, -37.15f, 285.76f }, { 159.21f, -14.88f, 226.03f }, 0.55f, 0.0f },
    { 14, { 0x00, 0x00, 0x00 }, { 108.85f, -23.22f, 233.55f }, { 137.97f, -20.44f, 286.42f }, 0.85f, 0.0f },
    { 15, { 0x00, 0x00, 0x00 }, { 106.66f, -86.33f, 152.64f }, { -47.9f, 15.17f, -45.89f }, 0.55f, 0.0f },
    { 16, { 0x00, 0x00, 0x00 }, { 70.7f, -82.24f, 142.81f }, { 192.68f, 30.13f, -71.92f }, 0.82f, 0.0f },
    { 18, { 0x00, 0x00, 0x00 }, { 61.45f, -80.59f, 132.14f }, { 10.85f, -75.45f, 123.37f }, 0.61f, 0.0f },
    { 5, { 0x00, 0x00, 0x00 }, { 178.3f, -29.07f, 177.15f }, { 124.49f, -17.27f, 256.68f }, 0.6f, 0.0f },
};
static const NPC_EV_CAM npc_cam0_3[16] =   // 0x35eda8
{
    { 0, { 0x00, 0x00, 0x00 }, { 114.45f, -17.74f, 285.32f }, { 172.73f, -15.31f, 246.86f }, 0.71f, 0.0f },
    { 1, { 0x00, 0x00, 0x00 }, { 160.49f, -18.91f, 249.0f }, { 103.64f, -7.58f, 278.64f }, 0.69f, 0.0f },
    { 2, { 0x00, 0x00, 0x00 }, { 132.44f, -16.22f, 273.92f }, { 167.39f, -16.22f, 218.99f }, 0.69f, 0.0f },
    { 4, { 0x00, 0x00, 0x00 }, { 128.7f, -30.41f, 223.79f }, { 72.09f, -22.82f, 145.72f }, 0.59f, 0.0f },
    { 6, { 0x00, 0x00, 0x00 }, { 83.37f, -23.78f, 231.85f }, { 130.27f, -22.06f, 247.05f }, 0.7f, 0.0f },
    { 7, { 0x00, 0x00, 0x00 }, { 163.86f, -23.2f, 238.37f }, { 129.91f, -20.81f, 299.29f }, 0.64f, 0.0f },
    { 8, { 0x00, 0x00, 0x00 }, { 136.23f, -19.2f, 259.29f }, { 66.79f, -24.26f, 192.1f }, 0.75f, 0.0f },
    { 9, { 0x00, 0x00, 0x00 }, { 114.34f, -91.02f, 171.47f }, { -12.96f, 10.33f, -44.69f }, 0.65f, 0.0f },
    { 10, { 0x00, 0x00, 0x00 }, { 9.16f, -76.95f, 157.69f }, { 53.02f, -71.09f, 139.07f }, 0.51f, 0.0f },
    { 11, { 0x00, 0x00, 0x00 }, { 91.28f, -66.92f, 99.68f }, { 38.98f, -65.9f, 119.92f }, 0.78f, 0.0f },
    { 12, { 0x00, 0x00, 0x00 }, { 132.44f, -16.22f, 273.92f }, { 167.39f, -16.22f, 218.99f }, 0.69f, 0.0f },
    { 14, { 0x00, 0x00, 0x00 }, { 163.86f, -23.2f, 238.37f }, { 129.91f, -20.81f, 299.29f }, 0.64f, 0.0f },
    { 15, { 0x00, 0x00, 0x00 }, { 114.34f, -91.02f, 171.47f }, { -12.96f, 10.33f, -44.69f }, 0.65f, 0.0f },
    { 16, { 0x00, 0x00, 0x00 }, { 9.16f, -76.95f, 157.69f }, { 53.02f, -71.09f, 139.07f }, 0.51f, 0.0f },
    { 18, { 0x00, 0x00, 0x00 }, { 91.28f, -66.92f, 99.68f }, { 38.98f, -65.9f, 119.92f }, 0.78f, 0.0f },
    { 5, { 0x00, 0x00, 0x00 }, { 190.94f, -26.83f, 184.43f }, { 110.44f, -19.23f, 237.55f }, 0.7f, 0.0f },
};
static const NPC_EV_CAM npc_cam0_4[16] =   // 0x35efe8
{
    { 0, { 0x00, 0x00, 0x00 }, { 114.45f, -17.74f, 285.32f }, { 172.73f, -15.31f, 246.86f }, 0.71f, 0.0f },
    { 1, { 0x00, 0x00, 0x00 }, { 160.49f, -18.91f, 249.0f }, { 103.64f, -7.58f, 278.64f }, 0.69f, 0.0f },
    { 2, { 0x00, 0x00, 0x00 }, { 132.44f, -16.22f, 273.92f }, { 167.39f, -16.22f, 218.99f }, 0.69f, 0.0f },
    { 4, { 0x00, 0x00, 0x00 }, { 128.7f, -30.41f, 223.79f }, { 72.09f, -22.82f, 145.72f }, 0.59f, 0.0f },
    { 6, { 0x00, 0x00, 0x00 }, { 83.37f, -23.78f, 231.85f }, { 130.27f, -22.06f, 247.05f }, 0.7f, 0.0f },
    { 7, { 0x00, 0x00, 0x00 }, { 163.86f, -23.2f, 238.37f }, { 129.91f, -20.81f, 299.29f }, 0.64f, 0.0f },
    { 8, { 0x00, 0x00, 0x00 }, { 136.23f, -19.2f, 259.29f }, { 66.79f, -24.26f, 192.1f }, 0.75f, 0.0f },
    { 9, { 0x00, 0x00, 0x00 }, { 69.75f, -79.74f, 167.79f }, { 113.04f, -0.44f, -87.93f }, 0.67f, 0.0f },
    { 10, { 0x00, 0x00, 0x00 }, { 61.53f, -94.83f, 167.32f }, { 77.17f, -71.19f, 128.57f }, 0.62f, 0.0f },
    { 11, { 0x00, 0x00, 0x00 }, { 93.67f, -75.53f, 165.56f }, { 46.31f, -67.19f, 136.72f }, 0.78f, 0.0f },
    { 12, { 0x00, 0x00, 0x00 }, { 132.44f, -16.22f, 273.92f }, { 167.39f, -16.22f, 218.99f }, 0.69f, 0.0f },
    { 14, { 0x00, 0x00, 0x00 }, { 163.86f, -23.2f, 238.37f }, { 129.91f, -20.81f, 299.29f }, 0.64f, 0.0f },
    { 15, { 0x00, 0x00, 0x00 }, { 69.75f, -79.74f, 167.79f }, { 113.04f, -0.44f, -87.93f }, 0.67f, 0.0f },
    { 16, { 0x00, 0x00, 0x00 }, { 61.53f, -94.83f, 167.32f }, { 77.17f, -71.19f, 128.57f }, 0.62f, 0.0f },
    { 18, { 0x00, 0x00, 0x00 }, { 93.67f, -75.53f, 165.56f }, { 46.31f, -67.19f, 136.72f }, 0.78f, 0.0f },
    { 5, { 0x00, 0x00, 0x00 }, { 190.94f, -26.83f, 184.43f }, { 110.44f, -19.23f, 237.55f }, 0.7f, 0.0f },
};
static const NPC_EV_CAM npc_cam0_5[4] =   // 0x35f228
{
    { 3, { 0x00, 0x00, 0x00 }, { 151.08f, -17.25f, 251.02f }, { 108.51f, -3.83f, 284.86f }, 0.74f, 0.0f },
    { 13, { 0x00, 0x00, 0x00 }, { 151.08f, -17.25f, 251.02f }, { 108.51f, -3.83f, 284.86f }, 0.74f, 0.0f },
    { 17, { 0x00, 0x00, 0x00 }, { 85.96f, -67.28f, 104.06f }, { -153.79f, 23.12f, 193.24f }, 0.77f, 0.0f },
    { 19, { 0x00, 0x00, 0x00 }, { 7.71f, -78.33f, 150.39f }, { 44.45f, -30.89f, 101.5f }, 0.78f, 0.0f },
};
static const NPC_EV_CAM *const npc_cam_tbl0[6] = { npc_cam0_0, npc_cam0_1, npc_cam0_2, npc_cam0_3, npc_cam0_4, npc_cam0_5 };
static const s_char npc_cam_num0[6] = { 16, 16, 16, 16, 16, 4 };

// event cameras of NPC 1 (PlyrCameraGetCamList): data 0x32dc30, counts sdata 0x371648
static const NPC_EV_CAM npc_cam1_0[1] =   // 0x35f4e8
{
    { 0, { 0x00, 0x00, 0x00 }, { 80.54f, -35.01f, 285.13f }, { 144.79f, -13.12f, 253.59f }, 0.63f, 0.0f },
};
static const NPC_EV_CAM npc_cam1_1[1] =   // 0x35f510
{
    { 0, { 0x00, 0x00, 0x00 }, { 172.45f, -37.15f, 285.76f }, { 159.21f, -14.88f, 226.03f }, 0.55f, 0.0f },
};
static const NPC_EV_CAM npc_cam1_2[1] =   // 0x35f538
{
    { 0, { 0x00, 0x00, 0x00 }, { 172.45f, -37.15f, 285.76f }, { 159.21f, -14.88f, 226.03f }, 0.55f, 0.0f },
};
static const NPC_EV_CAM npc_cam1_3[1] =   // 0x35f560
{
    { 0, { 0x00, 0x00, 0x00 }, { 117.49f, -22.27f, 288.5f }, { 886.23f, 125.0f, -333.8f }, 0.67f, 0.0f },
};
static const NPC_EV_CAM npc_cam1_4[1] =   // 0x35f588
{
    { 0, { 0x00, 0x00, 0x00 }, { 117.49f, -22.27f, 288.5f }, { 886.23f, 125.0f, -333.8f }, 0.67f, 0.0f },
};
static const NPC_EV_CAM npc_cam1_5[1] =   // 0x35f5b0
{
    { 1, { 0x00, 0x00, 0x00 }, { 141.64f, -30.31f, 252.87f }, { 171.37f, 6.7f, 235.3f }, 0.65f, 0.0f },
};
static const NPC_EV_CAM *const npc_cam_tbl1[6] = { npc_cam1_0, npc_cam1_1, npc_cam1_2, npc_cam1_3, npc_cam1_4, npc_cam1_5 };
static const s_char npc_cam_num1[6] = { 1, 1, 1, 1, 1, 1 };

// NPC 0's area remap (PlyrCameraGetAreaList): data 0x35f2b8, count sdata 0x371668
static const NPC_AREA_LABEL npc_area0[10] =
{
    { 47, 48, 50 },
    { 47, 50, 48 },
    { 47, 49, 48 },
    { 52, 48, 49 },
    { 52, 50, 48 },
    { 52, 49, 48 },
    { 49, 48, 50 },
    { 49, 50, 48 },
    { 50, 48, 49 },
    { 50, 49, 48 },
};

// The tables per NPC (data / sdata 0x371468..0x3714bc, 0x371650..0x371670).
static const NPC_LABEL *const npc_label_tbl[2] = { npc_label0, npc_label1 };   // sdata 0x371468
static const s_char npc_label_num[2] = { 20, 2 };                               // sdata 0x371650
static const s_char *const *const npc_mot_tbl[2] = { npc_mot_tbl0, npc_mot_tbl1 };  // sdata 0x371470
static const s_char *const npc_mot_num[2] = { npc_mot_num0, npc_mot_num1 };         // sdata 0x371478
static const int *const *const npc_door_tbl[2] = { npc_door_tbl0, npc_door_tbl1 };  // sdata 0x371480
static const s_char *const npc_door_num[2] = { npc_door_num0, npc_door_num1 };      // sdata 0x371488
static const NPC_SERIF *const *const *const npc_serif_tbl[2] = { npc_serif_tbl0, npc_serif_tbl1 };  // sdata 0x371490
static const s_char *const *const npc_serif_num[2] = { npc_serif_num0, npc_serif_num1 };           // sdata 0x3714a0
static const NPC_EV_CAM *const *const npc_cam_tbl[2] = { npc_cam_tbl0, npc_cam_tbl1 };  // sdata 0x3714a8
static const s_char *const npc_cam_num[2] = { npc_cam_num0, npc_cam_num1 };             // sdata 0x3714b0
static const NPC_AREA_LABEL *const npc_area_tbl[2] = { npc_area0, nullptr };            // sdata 0x3714b8
static const s_char npc_area_num[2] = { 10, 0 };                                       // sdata 0x371668
static const s_char npc_face_num[3][2] = { { 48, 48 }, { 8, 8 }, { 24, 0 } };           // PC: the slot sizes above

// The face a line's subtitle starts with, per NPC (sdata 0x371670).
static const s_char npc_serif_face[2] = { 5, 7 };

// The unit matrix PlyrCameraPlyrPush rotates (data 0x3293c0).
alignas(16) static sceVu0FMATRIX npc_act_unit =
{
    { 1.0f, 0.0f, 0.0f, 0.0f },
    { 0.0f, 1.0f, 0.0f, 0.0f },
    { 0.0f, 0.0f, 1.0f, 0.0f },
    { 0.0f, 0.0f, 0.0f, 1.0f },
};

// ──────────────────────────────────────────────────────────────────────
// Script access. The pc is an EE address into NPC_OBJ; jump targets are
// u16 offsets from the file start, little endian.

static const u_char *PlyrCameraActPtr(u_int adrs)
{
    return (const u_char *)ReiPan_GetHostPointer(adrs);
}

static u_char PlyrCameraActGetByte(PLYR_CAMERA_ACT *act)
{
    u_char v = *PlyrCameraActPtr(act->pc);

    act->pc++;
    return v;
}

static u_short PlyrCameraActPeekU16(const PLYR_CAMERA_ACT *act)
{
    const u_char *p = PlyrCameraActPtr(act->pc);

    return (u_short)(p[0] + p[1] * 0x100);
}

static u_short PlyrCameraActGetU16(PLYR_CAMERA_ACT *act)
{
    u_short v = PlyrCameraActPeekU16(act);

    act->pc += 2;
    return v;
}

// pc = base + ofs (a 64-bit store on the EE: the upper word becomes 0).
static void PlyrCameraActJump(PLYR_CAMERA_ACT *act, u_int ofs)
{
    act->pc  = act->base + ofs;
    act->_0c = 0;
}

// The op is done; the next one runs in the same frame.
static void PlyrCameraActNext(PLYR_CAMERA_ACT *act)
{
    act->op_step = 0;
    act->wait    = 0;
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x0021a678

void PlyrCameraActClear(PLYR_CAMERA_ACT *act)
{
    memset(act, 0, sizeof(PLYR_CAMERA_ACT));
}

// FF3 0x0021a698

void PlyrCameraActPcLoad(PLYR_CAMERA_ACT *act)
{
    if (act->pc_rel != 0)
    {
        act->pc   = NPC_OBJ_EE_ADRS + act->pc_rel;
        act->_0c  = 0;
        act->base = NPC_OBJ_EE_ADRS;
    }
}

// FF3 0x0021a6c0

void PlyrCameraActPcSave(PLYR_CAMERA_ACT *act)
{
    act->pc_rel = (int)(act->pc - act->base);
}

// The script of entry no in set: NPC_OBJ starts with a u16 offset per set,
// each set with a u16 offset per entry. It runs at the next step.
// FF3 0x0021a6d8

static void PlyrCameraActStart(PLYR_CAMERA_ACT *act, s_char set, s_char entry)
{
    const u_char *top = PlyrCameraActPtr(NPC_OBJ_EE_ADRS);
    u_int         ofs;

    act->base = NPC_OBJ_EE_ADRS;
    ofs = (u_short)(top[set * 2] + top[set * 2 + 1] * 0x100);
    ofs = (u_short)(top[ofs + entry * 2] + top[ofs + entry * 2 + 1] * 0x100);
    act->op_step = 0;
    act->wait    = 1;
    act->pc  = NPC_OBJ_EE_ADRS + ofs;
    act->_0c = 0;
}

// Runs the script: once the wait has counted down, ops run until one waits.
// 1 when the script ended (wait 0xff; the block stops).
// FF3 0x0021a768

static int PlyrCameraActRun(PLYR_CAMERA_ACT *act, void (*exec)(u_char op, PLYR_CAMERA_ACT *act))
{
    if (act->wait == 0)
        return 0;
    if (--act->wait != 0)
        return 0;

    for (;;)
    {
        if (act->op_step == 0)
        {
            act->op_pc = act->pc;
            act->op    = PlyrCameraActGetByte(act);
        }
        exec(act->op, act);
        if (act->wait == 0xff)
        {
            act->op_step = 0;
            act->wait    = 0;
            return 1;
        }
        if (act->wait != 0)
            return 0;
    }
}

// FF3 0x0021a818

void PlyrCameraActReset(PLYR_CAMERA_ACT *act, s_char no, s_char set, s_char entry)
{
    memset(act, 0, sizeof(PLYR_CAMERA_ACT));
    PlyrCameraActStart(act, set, entry);
    act->no    = no;
    act->label = -2;
}

static void PlyrCameraOpExec(u_char op, PLYR_CAMERA_ACT *act);

// FF3 0x0021a890

void PlyrCameraActStep(PLYR_CAMERA_ACT *act)
{
    PlyrCameraActRun(act, PlyrCameraOpExec);
}

// ──────────────────────────────────────────────────────────────────────
// 1 on the dream side (MapAreaSideGet 1), else 0: the side the NPCs' labels,
// lines and lights are picked for. Ghidra's name is FF2's (a low match: FF2
// EvNowSynchroMode, an ev_open.c condition, read the synchro flag); FF3
// links it here, between the act machine's functions.
// FF3 0x0021a8b0

int EvNowSynchroMode(void)
{
    return MapAreaSideGet() == 1;
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x0021a8d8

int PlyrCameraAreaIsDrawn(int area)
{
    s_char room;

    for (room = 0; room < 2; room++)
    {
        if (MapLoadGetRoomArea(room) == area && MapLoadCheckDrawFlg(room) != 0)
            return 1;
    }
    return 0;
}

// The NPC's head and the player's head (joint 2) see each other: bit 0 the
// NPC sees the player within dist / ang (degrees), bit 1 the player sees the
// NPC. Both need the NPC drawn and no wall between them.
// FF3 0x0021a940

u_char PlyrCameraSightCheck(s_char npc, float dist, float ang)
{
    sceVu0FVECTOR head;
    sceVu0FVECTOR scr;
    sceVu0FVECTOR plyr;
    float         rot;
    u_char        ret;

    if (AllPlyrIsReady() == 0)
        return 0;

    PlyrCameraGetHeadPos(npc, head, scr);
    rot = PlyrGetJointRotY(2);
    PlyrGetJointPos(2, plyr, nullptr);
    if (PlyrCameraIsDrawn(GetPlyrCameraWrk(npc), 0) == 0)
        return 0;
    if (PlyrLineHitCheck(head) != 0)
        return 0;
    if (MapAreaHitLineCheck(head, plyr, GetPlyrAreaNo()) != 0)
        return 0;

    ret = CheckEneView(scr[1], dist, ang, head, plyr) != 0;
    if (CheckEneView(rot, dist, ang, plyr, head) != 0)
        ret |= 2;
    return ret;
}

// The distance of two points on the ground (the ROM's inline VU0 code adds
// only x and z).
static float PlyrCameraDistXZ(const float *a, const float *b)
{
    float dx = a[0] - b[0];
    float dz = a[2] - b[2];

    return sqrtf(dx * dx + dz * dz);
}

// FF3 0x0021aa68

int PlyrCameraDistCheck(s_char npc, s_char cmp, float dist)
{
    sceVu0FVECTOR npos;
    sceVu0FVECTOR ppos;
    float         d;

    if (AllPlyrIsReady() == 0)
        return 0;

    PlyrCameraGetJointPos(npc, 0x1b, npos, nullptr);
    PlyrGetJointPos(0x1b, ppos, nullptr);
    d = PlyrCameraDistXZ(npos, ppos);
    switch (cmp)
    {
    case 0:
        return dist <= d;
    case 1:
        return dist == d;
    case 2:
        return d <= dist;
    }
    return 0;
}

// Keeps the player r + 4.5 away from each shown NPC of the area (on the
// ground, within 35 in height): a step that ends inside moves the player
// back to pos_old, or, when pos_old was inside too, onto the circle around
// the NPC on the side of pos_old.
// FF3 0x0021ab50

void PlyrCameraPlyrPush(float r, int area, float *pos, float *pos_old)
{
    PLYR_CAMERA_WRK *cam;
    sceVu0FVECTOR    npos;
    sceVu0FVECTOR    rot;
    sceVu0FVECTOR    rot2;
    sceVu0FVECTOR    v;
    sceVu0FMATRIX    m;
    s_char           no;

    r += 4.5f;                                  // sdata 0x3713e4
    for (no = 0; no < PLYR_CAMERA_NUM; no++)
    {
        cam = GetPlyrCameraWrk(no);
        if (cam->ready == 0 || cam->active == 0 || cam->visible == 0 || cam->area != area)
            continue;
        if (fabsf(pos[1] - cam->pos[1]) > 35.0f)   // sdata 0x3713e8
            continue;

        PlyrCameraGetJointPos(no, 0x1b, npos, nullptr);
        if (!(PlyrCameraDistXZ(npos, pos) < r))
            continue;

        if (PlyrCameraDistXZ(npos, pos_old) < r)
        {
            npos[1] = pos_old[1];
            GetTrgtRot(npos, pos_old, rot, 2);
            rot2[0] = NormalizeAngle(rot[0]);
            rot2[1] = NormalizeAngle(rot[1]);
            rot2[2] = NormalizeAngle(rot[2]);
            sceVu0RotMatrix(m, npc_act_unit, rot2);
            v[0] = 0.0f;
            v[1] = 0.0f;
            v[2] = r;
            v[3] = 1.0f;                        // sdata 0x3713ec
            sceVu0ApplyMatrix(rot2, m, v);
            sceVu0AddVector(pos, npos, rot2);
        }
        else
        {
            sceVu0CopyVector(pos, pos_old);
        }
    }
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x0021ad80

void PlyrCameraTalkFlgSet(s_char no, u_char bits)
{
    PLYR_CAMERA_WRK *cam = GetPlyrCameraWrk(no);

    cam->talk_flg |= bits;
}

// FF3 0x0021adc0

void PlyrCameraTalkFlgClr(s_char no, u_char bits)
{
    PLYR_CAMERA_WRK *cam = GetPlyrCameraWrk(no);

    cam->talk_flg &= ~bits;
}

// FF3 0x0021ae00

static int PlyrCameraTalkFlgGet(s_char no, s_char bits)
{
    PLYR_CAMERA_WRK *cam = GetPlyrCameraWrk(no);

    return (s_char)cam->talk_flg & bits;
}

// FF3 0x0021ae38

int PlyrCameraIsTalking(u_char no)
{
    s_char i;

    if (no < PLYR_CAMERA_NUM)
        return PlyrCameraTalkFlgGet((s_char)no, 1);
    for (i = 0; i < PLYR_CAMERA_NUM; i++)
    {
        if (PlyrCameraTalkFlgGet(i, 1) != 0)
            return 1;
    }
    return 0;
}

// FF3 0x0021aeb8

int PlyrCameraIsSpeaking(u_char no)
{
    s_char i;

    if (no < PLYR_CAMERA_NUM)
        return PlyrCameraTalkFlgGet((s_char)no, 2);
    for (i = 0; i < PLYR_CAMERA_NUM; i++)
    {
        if (PlyrCameraTalkFlgGet(i, 2) != 0)
            return 1;
    }
    return 0;
}

// FF3 0x0021af38

static int PlyrCameraAreaIs30(s_char no)
{
    return GetPlyrCameraWrk(no)->area == 0x30;
}

// ──────────────────────────────────────────────────────────────────────
// The ops. Operands follow the op byte; "jump" is pc = base + a u16.

// 0 (n): waits n frames; n 0 stops the script here.
// FF3 0x0021af68

static void PlyrCameraOpWait(PLYR_CAMERA_ACT *act)
{
    u_char n;

    switch (act->op_step)
    {
    case 0:
        n = PlyrCameraActGetByte(act);
        if (n == 0)
        {
            act->wait    = 1;
            act->op_step = 2;
        }
        else
        {
            act->wait    = n;
            act->op_step = 1;
        }
        break;
    case 1:
        act->op_step = 0;
        act->wait    = 0;
        break;
    case 2:
        act->wait = 1;
        break;
    }
}

// 1 (to): jump.
// FF3 0x0021afe8

static void PlyrCameraOpJump(PLYR_CAMERA_ACT *act)
{
    u_short to = PlyrCameraActPeekU16(act);

    act->op_step = 0;
    act->wait    = 0;
    PlyrCameraActJump(act, to);
}

// 2 (to): waits one frame, then jumps.
// FF3 0x0021b018

static void PlyrCameraOpYieldJump(PLYR_CAMERA_ACT *act)
{
    switch (act->op_step)
    {
    case 0:
        act->op_step = 1;
        act->jump    = PlyrCameraActGetU16(act);
        break;
    case 1:
        act->op_step = 0;
        act->wait    = 0;
        PlyrCameraActJump(act, act->jump);
        return;
    case 2:
        break;
    default:
        return;
    }
    act->wait = 1;
}

// 3 (i, v): counter i = v.
// FF3 0x0021b0a0

static void PlyrCameraOpSetCnt(PLYR_CAMERA_ACT *act)
{
    u_char  i = PlyrCameraActGetByte(act);
    u_short v = PlyrCameraActGetU16(act);

    act->op_step = 0;
    act->cnt[i]  = (u_char)v;
    act->wait    = 0;
}

// 4 (i, v, r): counter i = v + a random part of r.
// FF3 0x0021b0e8
// callcheck: rand (PC: ReiPan_Rand, the ROM's generator)

static void PlyrCameraOpSetCntRand(PLYR_CAMERA_ACT *act)
{
    u_char  i = PlyrCameraActGetByte(act);
    u_short v = PlyrCameraActGetU16(act);
    u_short r = PlyrCameraActGetU16(act);

    if (r != 0)
        r = (u_short)(int)((float)r * ((float)ReiPan_Rand() / REIPAN_RAND_MAXF) + 0.0f);  // sdata 0x3713f0
    act->op_step = 0;
    act->wait    = 0;
    act->cnt[i]  = (u_char)(v + r);
}

// 5 (-, i, v): flag i = v.
// FF3 0x0021b1c8

static void PlyrCameraOpSetFlg(PLYR_CAMERA_ACT *act)
{
    u_char i;

    PlyrCameraActGetByte(act);
    i = PlyrCameraActGetByte(act);
    act->flg[i]  = PlyrCameraActGetByte(act);
    act->op_step = 0;
    act->wait    = 0;
}

// 6 (to): waits one frame, then jumps.
// FF3 0x0021b208

static void PlyrCameraOpYieldJump2(PLYR_CAMERA_ACT *act)
{
    if (act->op_step == 0)
    {
        act->op_step = 1;
        act->wait    = 1;
        return;
    }
    if (act->op_step == 1)
        PlyrCameraActJump(act, PlyrCameraActPeekU16(act));
    PlyrCameraActNext(act);
}

// 0x20 (i, to): counter i 0: jump, else it counts down.
// FF3 0x0021b268

static void PlyrCameraOpLoop(PLYR_CAMERA_ACT *act)
{
    u_char  i  = PlyrCameraActGetByte(act);
    u_short to = PlyrCameraActGetU16(act);

    if (act->cnt[i] == 0)
        PlyrCameraActJump(act, to);
    else
        act->cnt[i]--;
    PlyrCameraActNext(act);
}

// 0x21 (-, i, v, to): jump when flag i is v.
// FF3 0x0021b2d0

static void PlyrCameraOpFlgJump(PLYR_CAMERA_ACT *act)
{
    u_char  i;
    u_short v;
    u_short to;

    PlyrCameraActGetByte(act);
    i  = PlyrCameraActGetByte(act);
    v  = PlyrCameraActGetU16(act);
    to = PlyrCameraActGetU16(act);
    if (act->flg[i] == v)
        PlyrCameraActJump(act, to);
    PlyrCameraActNext(act);
}

// 1..99 (sdata 0x3713f4..0x3713fc; 0x371400..0x371408 for op 0x23).
// callcheck: rand (PC: ReiPan_Rand, the ROM's generator)
static int PlyrCameraActRand99(void)
{
    return (int)((float)ReiPan_Rand() / REIPAN_RAND_MAXF * 98.0f + 1.0f);
}

// 0x22 (pct, to): jump with a chance of pct percent.
// FF3 0x0021b348
// callcheck: rand (PlyrCameraActRand99; PC: ReiPan_Rand, the ROM's generator)

static void PlyrCameraOpRandJump(PLYR_CAMERA_ACT *act)
{
    u_char  pct = PlyrCameraActGetByte(act);
    u_short to  = PlyrCameraActGetU16(act);

    if (!(pct < PlyrCameraActRand99()))
        PlyrCameraActJump(act, to);
    PlyrCameraActNext(act);
}

// 0x23 (pct, to): jump with a chance of 100 - pct percent.
// FF3 0x0021b418
// callcheck: rand (PlyrCameraActRand99; PC: ReiPan_Rand, the ROM's generator)

static void PlyrCameraOpRandJump2(PLYR_CAMERA_ACT *act)
{
    u_char  pct = PlyrCameraActGetByte(act);
    u_short to  = PlyrCameraActGetU16(act);

    if (!(PlyrCameraActRand99() < pct))
        PlyrCameraActJump(act, to);
    PlyrCameraActNext(act);
}

// 0x24 (to): jump on the dream side.
// FF3 0x0021b4e8

static void PlyrCameraOpDreamJump(PLYR_CAMERA_ACT *act)
{
    u_short to = PlyrCameraActGetU16(act);

    if (EvNowSynchroMode() != 0)
        PlyrCameraActJump(act, to);
    PlyrCameraActNext(act);
}

// 0x25 (to): jump when the label is in the player's area.
// FF3 0x0021b550

static void PlyrCameraOpLabelAreaJump(PLYR_CAMERA_ACT *act)
{
    u_short to = PlyrCameraActGetU16(act);

    if (PlyrCameraLabelIsPlyrArea(act->no, act->label) != 0)
        PlyrCameraActJump(act, to);
    PlyrCameraActNext(act);
}

// 0x26 (to): jump when the NPC is free and has a label.
// FF3 0x0021b5c0

static void PlyrCameraOpFreeJump(PLYR_CAMERA_ACT *act)
{
    u_short to = PlyrCameraActGetU16(act);

    if (GetPlyrCameraWrk(act->no)->free > 0 && act->label >= 0)
        PlyrCameraActJump(act, to);
    PlyrCameraActNext(act);
}

// 0x27 (to): jump when the greet may start.
// FF3 0x0021b640

static void PlyrCameraOpGreetJump(PLYR_CAMERA_ACT *act)
{
    u_short to = PlyrCameraActGetU16(act);

    if (PlyrCameraGreetCond(act) != 0)
        PlyrCameraActJump(act, to);
    PlyrCameraActNext(act);
}

// 0x28 (to): jump when an event result of kind 0 is queued.
// FF3 0x0021b6a8

static void PlyrCameraOpQueue0Jump(PLYR_CAMERA_ACT *act)
{
    u_short to = PlyrCameraActGetU16(act);

    if (PlyrCameraQueue0Ready(act->no) != 0)
        PlyrCameraActJump(act, to);
    PlyrCameraActNext(act);
}

// 0x29 (to)
// FF3 0x0021b718

static void PlyrCameraOpReportJump(PLYR_CAMERA_ACT *act)
{
    u_short to = PlyrCameraActGetU16(act);

    if (PlyrCameraReportCheck(act) != 0)
        PlyrCameraActJump(act, to);
    PlyrCameraActNext(act);
}

// 0x2a (to)
// FF3 0x0021b780

static void PlyrCameraOpChat3Jump(PLYR_CAMERA_ACT *act)
{
    u_short to = PlyrCameraActGetU16(act);

    if (PlyrCameraChat3Check(act) != 0)
        PlyrCameraActJump(act, to);
    PlyrCameraActNext(act);
}

// 0x2b (to)
// FF3 0x0021b7e8

static void PlyrCameraOpReport1Jump(PLYR_CAMERA_ACT *act)
{
    u_short to = PlyrCameraActGetU16(act);

    if (PlyrCameraReport1Check(act) != 0)
        PlyrCameraActJump(act, to);
    PlyrCameraActNext(act);
}

// 0x2c (to)
// FF3 0x0021b850

static void PlyrCameraOpChat4Jump(PLYR_CAMERA_ACT *act)
{
    u_short to = PlyrCameraActGetU16(act);

    if (PlyrCameraChat4Check(act) != 0)
        PlyrCameraActJump(act, to);
    PlyrCameraActNext(act);
}

// 0x2d (to): jump when the NPC is drawn.
// FF3 0x0021b8b8

static void PlyrCameraOpDrawnJump(PLYR_CAMERA_ACT *act)
{
    u_short to = PlyrCameraActGetU16(act);

    if (PlyrCameraIsDrawn(GetPlyrCameraWrk(act->no), 0) != 0)
        PlyrCameraActJump(act, to);
    PlyrCameraActNext(act);
}

// 0x2e (to)
// FF3 0x0021b930

static void PlyrCameraOp34Jump(PLYR_CAMERA_ACT *act)
{
    u_short to = PlyrCameraActGetU16(act);

    if (act->_34 != 0)
        PlyrCameraActJump(act, to);
    PlyrCameraActNext(act);
}

// 0x2f (to)
// FF3 0x0021b970

static void PlyrCameraOpEcJump(PLYR_CAMERA_ACT *act)
{
    u_short to = PlyrCameraActGetU16(act);

    if (GetPlyrCameraWrk(act->no)->_0ec != 0)
        PlyrCameraActJump(act, to);
    PlyrCameraActNext(act);
}

// 0x30 (to)
// FF3 0x0021b9e0

static void PlyrCameraOpLabel84Jump(PLYR_CAMERA_ACT *act)
{
    u_short to = PlyrCameraActGetU16(act);

    if (PlyrCameraGetLabel84(act->no, act->label) != 0)
        PlyrCameraActJump(act, to);
    PlyrCameraActNext(act);
}

// 0x40 (kind): PlyrCameraActSetLabel until it is done.
// FF3 0x0021ba50

static void PlyrCameraOpSetLabel(PLYR_CAMERA_ACT *act)
{
    if (act->op_step == 0)
    {
        act->set_kind = PlyrCameraActGetByte(act);
        act->op_step  = 1;
        act->wait     = 1;
        return;
    }
    if (act->op_step == 1 && PlyrCameraActSetLabel(act, (s_char)act->set_kind) == 0)
    {
        act->wait = 1;
        return;
    }
    PlyrCameraActNext(act);
}

// 0x41
// FF3 0x0021bae0

static void PlyrCameraOpNop(PLYR_CAMERA_ACT *act)
{
    PlyrCameraActNext(act);
}

// Ops 0x42..0x46 and 0x4b (to): an act hook's init, then its step each
// frame until it is done, then jump.
static void PlyrCameraOpHook(PLYR_CAMERA_ACT *act, void (*init)(PLYR_CAMERA_ACT *act), int (*step)(PLYR_CAMERA_ACT *act))
{
    if (act->op_step == 0)
    {
        init(act);
        act->wait = 1;
        act->op_step++;
        return;
    }
    if (act->op_step == 1)
    {
        if (step(act) == 0)
        {
            act->wait = 1;
            return;
        }
        PlyrCameraActJump(act, PlyrCameraActPeekU16(act));
    }
    PlyrCameraActNext(act);
}

// 0x42
// FF3 0x0021baf0
// callcheck: PlyrCameraGreetInit, PlyrCameraGreetStep (through PlyrCameraOpHook)

static void PlyrCameraOpGreet(PLYR_CAMERA_ACT *act)
{
    PlyrCameraOpHook(act, PlyrCameraGreetInit, PlyrCameraGreetStep);
}

// 0x43
// FF3 0x0021bb90
// callcheck: PlyrCameraReportInit, PlyrCameraReportStep (through PlyrCameraOpHook)

static void PlyrCameraOpReport(PLYR_CAMERA_ACT *act)
{
    PlyrCameraOpHook(act, PlyrCameraReportInit, PlyrCameraReportStep);
}

// 0x44
// FF3 0x0021bc30
// callcheck: PlyrCameraChat3Init, PlyrCameraChat3Step (through PlyrCameraOpHook)

static void PlyrCameraOpChat3(PLYR_CAMERA_ACT *act)
{
    PlyrCameraOpHook(act, PlyrCameraChat3Init, PlyrCameraChat3Step);
}

// 0x45
// FF3 0x0021bcd0
// callcheck: PlyrCameraReport1Init, PlyrCameraReport1Step (through PlyrCameraOpHook)

static void PlyrCameraOpReport1(PLYR_CAMERA_ACT *act)
{
    PlyrCameraOpHook(act, PlyrCameraReport1Init, PlyrCameraReport1Step);
}

// 0x46
// FF3 0x0021bd70
// callcheck: PlyrCameraChat4Init, PlyrCameraChat4Step (through PlyrCameraOpHook)

static void PlyrCameraOpChat4(PLYR_CAMERA_ACT *act)
{
    PlyrCameraOpHook(act, PlyrCameraChat4Init, PlyrCameraChat4Step);
}

// 0x47
// FF3 0x0021be10

static void PlyrCameraOpIdleInit(PLYR_CAMERA_ACT *act)
{
    PlyrCameraIdleInit(act);
    PlyrCameraActNext(act);
}

// 0x48
// FF3 0x0021be40

static void PlyrCameraOpIdleStep(PLYR_CAMERA_ACT *act)
{
    PlyrCameraIdleStep(act);
    PlyrCameraActNext(act);
}

// 0x49
// FF3 0x0021be70

static void PlyrCameraOpWatchInit(PLYR_CAMERA_ACT *act)
{
    PlyrCameraWatchInit(act);
    PlyrCameraActNext(act);
}

// 0x4a
// FF3 0x0021bea0

static void PlyrCameraOpWatchStep(PLYR_CAMERA_ACT *act)
{
    PlyrCameraWatchStep(act);
    PlyrCameraActNext(act);
}

// 0x4b
// FF3 0x0021bed0
// callcheck: PlyrCameraGreetMotInit, PlyrCameraGreetMotStep (through PlyrCameraOpHook)

static void PlyrCameraOpGreetMot(PLYR_CAMERA_ACT *act)
{
    PlyrCameraOpHook(act, PlyrCameraGreetMotInit, PlyrCameraGreetMotStep);
}

// 0x4c
// FF3 0x0021bf70

static void PlyrCameraOpSetEc(PLYR_CAMERA_ACT *act)
{
    GetPlyrCameraWrk(act->no)->_0ec = 1;
    PlyrCameraActNext(act);
}

typedef void (*PLYR_CAMERA_OP)(PLYR_CAMERA_ACT *act);

// data 0x32dac0: ops 0..6
static const PLYR_CAMERA_OP plyr_camera_op0[8] =
{
    PlyrCameraOpWait, PlyrCameraOpJump, PlyrCameraOpYieldJump, PlyrCameraOpSetCnt,
    PlyrCameraOpSetCntRand, PlyrCameraOpSetFlg, PlyrCameraOpYieldJump2, nullptr,
};

// data 0x32dae0: ops 0x20..0x30
static const PLYR_CAMERA_OP plyr_camera_op1[18] =
{
    PlyrCameraOpLoop, PlyrCameraOpFlgJump, PlyrCameraOpRandJump, PlyrCameraOpRandJump2,
    PlyrCameraOpDreamJump, PlyrCameraOpLabelAreaJump, PlyrCameraOpFreeJump, PlyrCameraOpGreetJump,
    PlyrCameraOpQueue0Jump, PlyrCameraOpReportJump, PlyrCameraOpChat3Jump, PlyrCameraOpReport1Jump,
    PlyrCameraOpChat4Jump, PlyrCameraOpDrawnJump, PlyrCameraOp34Jump, PlyrCameraOpEcJump,
    PlyrCameraOpLabel84Jump, nullptr,
};

// data 0x32db28: ops 0x40..0x4c
static const PLYR_CAMERA_OP plyr_camera_op2[14] =
{
    PlyrCameraOpSetLabel, PlyrCameraOpNop, PlyrCameraOpGreet, PlyrCameraOpReport,
    PlyrCameraOpChat3, PlyrCameraOpReport1, PlyrCameraOpChat4, PlyrCameraOpIdleInit,
    PlyrCameraOpIdleStep, PlyrCameraOpWatchInit, PlyrCameraOpWatchStep, PlyrCameraOpGreetMot,
    PlyrCameraOpSetEc, nullptr,
};

// FF3 0x0021bfa8

static void PlyrCameraOpExec(u_char op, PLYR_CAMERA_ACT *act)
{
    PLYR_CAMERA_OP func;

    if (op < 0x20)
        func = op < 8 ? plyr_camera_op0[op] : nullptr;
    else if (op < 0x40)
        func = op - 0x20 < 18 ? plyr_camera_op1[op - 0x20] : nullptr;
    else
        func = op - 0x40 < 14 ? plyr_camera_op2[op - 0x40] : nullptr;

    // PC: the ROM calls through whatever word the op indexes; an op outside
    // the tables (none in the shipped NPC_OBJ) ends the script instead.
    if (func == nullptr)
    {
        act->wait = 0xff;
        return;
    }
    func(act);
}

// ──────────────────────────────────────────────────────────────────────
// The data.

// FF3 0x0021c038

const NPC_LABEL *PlyrCameraGetLabel(s_char npc, s_char label, s_char *num)
{
    if (num != nullptr)
        *num = npc_label_num[npc];
    return &npc_label_tbl[npc][label];
}

// FF3 0x0021c080

const s_char *PlyrCameraGetMotList(s_char npc, s_char list, s_char *num)
{
    if (num != nullptr)
        *num = npc_mot_num[npc][list];
    return npc_mot_tbl[npc][list];
}

// FF3 0x0021c0d0

static const int *PlyrCameraGetDoorList(s_char npc, s_char list, s_char *num)
{
    if (num != nullptr)
        *num = npc_door_num[npc][list];
    return npc_door_tbl[npc][list];
}

// FF3 0x0021c120

s_char PlyrCameraGetMotFace(s_char npc, s_char mot)
{
    int set = 0;
    int tbl;

    if (npc == 0)
    {
        if (PlyrCameraIsCatMdl(GetPlyrCameraWrk(0)) != 0)
            set = 1;
    }
    else
    {
        set = 2;
    }
    tbl = PlyrCameraGetFacialTbl();
    if (npc == 1)
        tbl = 0;

    // PC: the ROM reads past the table for a motion (or facial table) it
    // does not cover.
    if (tbl > 1 || mot < 0 || mot >= npc_face_num[set][tbl])
        return -1;
    return npc_face_tbl[set][tbl][mot];
}

// FF3 0x0021c1c8

static const NPC_SERIF *PlyrCameraGetSerifList(s_char npc, s_char kind, s_char tbl, s_char *num)
{
    if (npc == 1)
        tbl = 0;
    if (num != nullptr)
        *num = npc_serif_num[npc][kind][tbl];
    return npc_serif_tbl[npc][kind][tbl];
}

// FF3 0x0021c238

static const NPC_EV_CAM *PlyrCameraGetCamList(s_char npc, s_char kind, s_char *num)
{
    if (num != nullptr)
        *num = npc_cam_num[npc][kind];
    return npc_cam_tbl[npc][kind];
}

// FF3 0x0021c288

static const NPC_AREA_LABEL *PlyrCameraGetAreaList(s_char npc, s_char *num)
{
    if (num != nullptr)
        *num = npc_area_num[npc];
    return npc_area_tbl[npc];
}

// ──────────────────────────────────────────────────────────────────────
// A random label of the NPC on side in area (-1: any) that is not locked
// (NPC_LOCK_POS) and passes the filters: without any_84 only labels whose
// +0x84 is 0, without any_cat only labels without a cat value, with door1
// only labels with a door 1, and none whose +0x83 is excl (-1: no
// exclusion). -1: none.
// FF3 0x0021c2b8

static s_char PlyrCameraPickLabel(s_char npc, s_char side, int area, int any_84, int any_cat, int door1, s_char excl)
{
    const NPC_LABEL *row;
    PLYR_CAMERA_WRK *cam;
    s_char           num;
    s_char           cand[0x80];
    s_char           n = 0;
    s_char           i;
    int              ok;

    row = PlyrCameraGetLabel(npc, 0, &num);
    cam = GetPlyrCameraWrk(npc);
    for (i = 0; i < num; i++, row++)
    {
        ok = cam->lock_pos[i] == 0;
        if (side != -1 && row->side != side)
            ok = 0;
        if (area != -1 && row->area != area)
            ok = 0;
        if (any_84 == 0 && row->_84 != 0)
            ok = 0;
        if (any_cat == 0 && row->cat != -1)
            ok = 0;
        if (door1 != 0 && row->door[1] == -1)
            ok = 0;
        if (excl != -1 && row->mot6 == excl)
            ok = 0;
        if (ok)
            cand[n++] = i;
    }
    if (n <= 0)
        return -1;
    return cand[(s_char)GetRandValIByF(n)];
}

// FF3 0x0021c478

void PlyrCameraSetLabelPos(s_char npc, int area, const float *pos)
{
    sceVu0FVECTOR p;
    float         rot;

    p[0] = pos[0];
    p[1] = pos[1];
    p[2] = pos[2];
    p[3] = 1.0f;                                // sdata 0x3714c4
    rot = pos[3] * 3.14159274f / 180.0f;        // sdata 0x3714c0 / 0x3714c8
    RotLimitChk(&rot);
    PlyrCameraSetPos(npc, (u_char)area, p, rot);
}

// FF3 0x0021c510

s_char PlyrCameraSetPosLabel(s_char npc, s_char label)
{
    const NPC_LABEL  *row;
    PLYR_CAMERA_WRK  *cam;
    PLYR_CAMERA_HEAD *head;
    s_char            mot = -1;

    // PC: PlyrCameraPickLabel's -1 (no free label) makes the ROM read the
    // 0x90 bytes before the table; nothing is moved here instead.
    if (label < 0)
        return label;

    row = PlyrCameraGetLabel(npc, label, nullptr);
    if (row->mot >= 0 && row->mot6 < 0)
        mot = PlyrCameraGetMotList(npc, row->mot, nullptr)[0];

    cam = GetPlyrCameraWrk(npc);
    if (npc == 0)
    {
        if (PlyrCameraGetLabel84(0, label) == 2)
        {
            if (PlyrCameraIsCatMdl(cam) == 0)
                PlyrCameraMdlChange(0, 3, 0);
        }
        else if (PlyrCameraIsCatMdl(cam) != 0)
        {
            PlyrCameraMdlChange(0, -1, 0);
        }

        head = PlyrCameraGetHead();
        if (row->cat != -1)
        {
            head->_02 = (u_char)row->cat;
            CatSetFixPos(0, row->cat);
        }
        else if ((s_char)head->_02 != -1)
        {
            head->_02 = 0xff;
            CatSetFixPos(0, -1);
        }
    }

    if (cam->area != row->area)
        cam->act.chat3_done = 0;
    PlyrCameraSetLabelPos(npc, row->area, row->pos);
    if (mot >= 0)
        PlyrCameraMotCall(npc, mot, 0);
    PlyrCameraSetEyeTarget(npc, 0);
    PlyrCameraSetEyePos(npc, nullptr, 0);
    return label;
}

// strict: NPC 0 takes only labels whose +0x84 is 0 (so do queued event
// results).
// FF3 0x0021c688

s_char PlyrCameraSetRandLabel(s_char npc, int area, int strict)
{
    PLYR_CAMERA_WRK *cam;
    int              any_84  = 1;
    int              any_cat = 1;
    int              door1   = 0;
    s_char           excl    = -1;
    s_char           label;

    if (npc == 0)
    {
        any_84 = strict == 0;
        if (PlyrCameraQueue0Ready(0) != 0)
        {
            any_84 = 0;
            door1  = 1;
        }
        if (PlyrCameraQueue1Ready(0) != 0)
            any_84 = 0;
        if (GetPlyrAreaNo() != 0x31 && GetPlyrAreaNo() != 0x33)
            excl = 0x16;
        any_cat = CatIsHeld(0) == 0;
    }

    label = PlyrCameraPickLabel(npc, (s_char)EvNowSynchroMode(), area, any_84, any_cat, door1, excl);
    cam = GetPlyrCameraWrk(npc);
    cam->act._34 = 0;
    cam->_0ec    = 0;
    return PlyrCameraSetPosLabel(npc, label);
}

// NPC 0 follows the player into area: the remap rows for the area and the
// current label's area give the area to pick a label in. The label kept
// when none fits.
// FF3 0x0021c798

s_char PlyrCameraAreaLabel(s_char npc, s_char label, int area)
{
    PLYR_CAMERA_WRK      *cam = GetPlyrCameraWrk(npc);
    const NPC_AREA_LABEL *list;
    s_char                num;
    s_char                i;
    s_char                ret;
    int                   from;
    int                   any_84 = 1;
    int                   door1  = 0;
    int                   any_cat;
    s_char                excl;
    int                   q1;

    if (npc != 0)
    {
        cam->_0ec = 0;
        return label;
    }

    from = PlyrCameraGetLabel(0, label, nullptr)->area;
    list = PlyrCameraGetAreaList(0, &num);
    if (PlyrCameraQueue0Ready(0) != 0)
    {
        any_84 = 0;
        door1  = 1;
    }
    q1 = PlyrCameraQueue1Ready(0);
    excl = -1;
    if (q1 != 0)
        any_84 = 0;
    if (area != 0x31 && area != 0x33)
        excl = 0x16;

    for (i = 0; i < num; i++, list++)
    {
        if (list->area != area || list->from != from)
            continue;

        any_cat = CatGetWrk(0)->area == list->from;
        if (CatIsHeld(0) != 0)
            any_cat = 0;
        ret = PlyrCameraPickLabel(0, (s_char)EvNowSynchroMode(), list->to, any_84, any_cat, door1, excl);
        if (ret >= 0)
        {
            cam->act._34 = 0;
            cam->_0ec    = 0;
            return ret;
        }
    }
    return label;
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x0021c950

s_char PlyrCameraPickSerif(s_char npc, s_char kind)
{
    s_char num;

    PlyrCameraGetSerifList(npc, kind, PlyrCameraGetSerifTbl(kind), &num);
    return (s_char)GetRandValIByF(num);
}

// FF3 0x0021c9b0

s_char PlyrCameraGetSerifCam(s_char npc, s_char kind, s_char sel)
{
    const NPC_SERIF *list;
    s_char           num;

    if (sel < 0)
        return -1;
    list = PlyrCameraGetSerifList(npc, kind, PlyrCameraGetSerifTbl(kind), &num);
    if (sel >= num)
        return -1;
    return (s_char)list[sel].sub;
}

// Shows line sel (< 0: a random one) of the NPC's serif list of a kind: a
// subtitle at the NPC's head (or a plain one without a model), or message
// 0x71 in layout anim 5. NPC 0's greet in area 0x30 with an event result
// queued says line 6 one time in n + 1. face: start the NPC's talk face.
// FF3 0x0021ca48

s_char PlyrCameraSerifStart(s_char npc, s_char kind, s_char sel, int face)
{
    const NPC_SERIF *list;
    const NPC_SERIF *line;
    PLYR_CAMERA_WRK *cam;
    sceVu0FVECTOR    pos;
    s_char           tbl;
    s_char           num;
    int              id;

    tbl  = PlyrCameraGetSerifTbl(kind);
    list = PlyrCameraGetSerifList(npc, kind, tbl, &num);
    if (sel < 0)
    {
        sel = PlyrCameraPickSerif(npc, kind);
        if (sel < 0)
            return sel;
    }

    cam = GetPlyrCameraWrk(npc);
    cam->act.face_req = 0;
    cam->act.anim     = -1;
    line = &list[sel];
    if (line->type == 0 && line->id >= 0)
    {
        id = line->id;
        if (npc == 0 && kind == 0 && tbl == 0 && PlyrCameraAreaIs30(0) != 0 &&
            PlyrCameraQueue0Ready(0) != 0 && GetRandValIByF(num + 1) == 0)
        {
            id = 6;
        }
        if (PlyrCameraGetHeadPos(npc, pos, nullptr) != 0)
            SubTitleReq3D(1, id, pos);
        else
            SubTitleReq(1, id);
        if (face != 0)
            cam->act.face_req = 1;
    }

    line = &list[sel];
    if (line->type == 1)
    {
        cam->act.anim = LayoutAnimStart(5, 0);
        Anim2dMsgSetType(2);
        Anim2dMsgSet(0x71, line->id);
        if (face != 0)
            PlyrCameraFaceReq(npc, npc_serif_face[npc], 1);
    }
    return sel;
}

// The line's second subtitle (its sub word) or its message, with the face.
// FF3 0x0021cc18

void PlyrCameraSerifStart2(s_char npc, s_char kind, s_char sel)
{
    const NPC_SERIF *list;
    const NPC_SERIF *line;
    PLYR_CAMERA_WRK *cam;
    sceVu0FVECTOR    pos;

    list = PlyrCameraGetSerifList(npc, kind, PlyrCameraGetSerifTbl(kind), nullptr);
    if (sel < 0)
        return;

    cam = GetPlyrCameraWrk(npc);
    cam->act.face_req = 0;
    cam->act.anim     = -1;
    line = &list[sel];
    if (line->type == 0 && line->sub >= 0)
    {
        if (PlyrCameraGetHeadPos(npc, pos, nullptr) != 0)
            SubTitleReq3D(1, line->sub, pos);
        else
            SubTitleReq(1, line->sub);
        cam->act.face_req = 1;
    }

    line = &list[sel];
    if (line->type == 1)
    {
        cam->act.anim = LayoutAnimStart(5, 0);
        Anim2dMsgSetType(2);
        Anim2dMsgSet(0x71, line->id);
        PlyrCameraFaceReq(npc, npc_serif_face[npc], 1);
    }
}

// 1 when the line is over (its window closed, or its subtitle ended); the
// talk face starts once the subtitle shows and ends with it.
// FF3 0x0021cd60

int PlyrCameraSerifIsEnd(s_char npc)
{
    PLYR_CAMERA_WRK *cam = GetPlyrCameraWrk(npc);

    if (cam->act.anim < 0)
    {
        if (cam->act.face_req != 0)
        {
            if (SubTitleIsExec() == 0)
                return 0;
            PlyrCameraFaceReq(npc, npc_serif_face[npc], 1);
            cam->act.face_req = 0;
        }
        if (SubTitleIsEnd() == 0)
            return 0;
    }
    else
    {
        if (LayoutAnimGetState(cam->act.anim) != 4)
            return 0;
        LayoutAnimReset(cam->act.anim);
        cam->act.anim = -1;
    }
    PlyrCameraFaceReq(npc, -1, 1);
    return 1;
}

// The player stands in one of the label's door rectangles: kind 0 / 1 the
// label's door 0 / 1, kind 3 its door list.
// FF3 0x0021ce30

int PlyrCameraLabelDoorCheck(s_char npc, s_char kind, s_char label)
{
    const NPC_LABEL *row = PlyrCameraGetLabel(npc, label, nullptr);
    const int       *door = nullptr;
    s_char           num = -1;
    s_char           i;

    if (kind == 1)
    {
        num  = 1;
        door = &row->door[1];
    }
    else if (kind == 0)
    {
        num  = 1;
        door = &row->door[0];
    }
    else if (kind == 3 && row->door_list >= 0)
    {
        door = PlyrCameraGetDoorList(npc, row->door_list, &num);
    }

    if (num < 0 || door == nullptr)
        return 0;
    for (i = 0; i < num; i++, door++)
    {
        if (*door >= 0 &&
            MrecIsInEvent(ingame_wrk.plyr_wrk->pos, *door, ingame_wrk.plyr_wrk->floor) != 0)
            return 1;
    }
    return 0;
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x0021cf68

void PlyrCameraEvCamSet(float fov, float roll, float *pos, float *tgt)
{
    EventCameraReq();
    EventCameraSetWorldFlg(1);
    EventCameraSetPosition(pos);
    EventCameraSetTarget(tgt);
    EventCameraSetFov(fov);
    EventCameraSetRoll(roll);
}

// Row sel of the kind's talk cameras; sel < 0: a random row of the label
// (label -1: of any). 0 when there is none.
// FF3 0x0021cfd8

int PlyrCameraEvCamStart(s_char npc, s_char kind, s_char sel, s_char label)
{
    const NPC_EV_CAM *rows;
    const NPC_EV_CAM *row;
    sceVu0FVECTOR     pos;
    sceVu0FVECTOR     tgt;
    s_char            num;
    s_char            cand[0x80];
    s_char            n = 0;
    s_char            i;

    rows = PlyrCameraGetCamList(npc, kind, &num);
    if (sel < 0)
    {
        for (i = 0; i < num; i++)
        {
            if (label == -1 || rows[i].label == label)
                cand[n++] = i;
        }
        if (n > 0)
            sel = cand[(s_char)GetRandValIByF(n)];
    }
    else if (rows[sel].label < 0)
    {
        return 0;
    }

    if (sel < 0)
        return 0;

    row = &rows[sel];
    pos[0] = row->pos[0];
    pos[1] = row->pos[1];
    pos[2] = row->pos[2];
    pos[3] = 1.0f;                              // sdata 0x3714cc
    tgt[0] = row->tgt[0];
    tgt[1] = row->tgt[1];
    tgt[2] = row->tgt[2];
    tgt[3] = 1.0f;
    PlyrCameraEvCamSet(row->fov, row->roll, pos, tgt);
    return 1;
}

// FF3 0x0021d190

void PlyrCameraEvCamEnd(void)
{
    EventCameraCut();
    EventCameraSetWorldFlg(0);
}

// FF3 0x0021d1b0

void PlyrCameraPlyrPlace(const float *pos, short anime)
{
    sceVu0FVECTOR p;
    sceVu0FVECTOR rot;
    float         ry;

    p[0] = pos[0];
    p[1] = pos[1];
    p[2] = pos[2];
    p[3] = 1.0f;                                // sdata 0x3714d0
    rot[0] = rot[1] = rot[2] = rot[3] = 0.0f;   // the ROM subtracts the vector from itself
    ry = pos[3] * 3.14159274f / 180.0f;         // sdata 0x3714d4 / 0x3714d8
    RotLimitChk(&ry);
    rot[1] = ry;
    PlyrSetPos(p);
    PlyrSetRot(rot);
    if (anime != -1)
        SetPlyrAnime((u_char)anime, 0, 0xff, 0xff, 1, 0.0f);
}

// FF3 0x0021d280

void PlyrCameraPlyrToLabel(s_char npc, s_char kind, s_char label)
{
    const NPC_LABEL *row = PlyrCameraGetLabel(npc, label, nullptr);

    if (kind == 0)
        PlyrCameraPlyrPlace(row->plyr0, -1);
    else if (kind == 1)
        PlyrCameraPlyrPlace(row->plyr1, -1);
    else
        return;
    SetPlyrAnime(0, 0xff, 0xff, 0xff, 1, 0.0f);
}
