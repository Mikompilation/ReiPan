/* ==========================================================================
 *  ingame/camera/dmg_cam_shot.h
 *
 *  The damage camera shots (FF3-only, 0x1374d8..0x137cb8; file name
 *  invented). While a ghost holds or hits the player, the approach camera
 *  (map_camera.c, APPROACH_CAMERA_FLOW_SHOT) cuts between fixed shots picked
 *  by the attacker's kind (PLYR_WRK._147): DmgCamShotInit gathers that
 *  kind's rows of the shot table, shuffles them and takes the first one
 *  whose eye sees its target; DmgCamShotMain holds it for 90..150 frames
 *  (the player's timer step), then cuts to the next visible one. A shot is
 *  an eye and a target, each an offset in the frame of an anchor (the
 *  player, the enemy, their midpoint or the camera) turned by its facing;
 *  the result goes to the approach camera's PosInitVector / NeckInitVector.
 * ======================================================================== */
#ifndef _INGAME_CAMERA_DMG_CAM_SHOT_H
#define _INGAME_CAMERA_DMG_CAM_SHOT_H

#include <stddef.h>                     // offsetof
#include "sdk/libvu0.h"                 // sceVu0FVECTOR
#include "sdk/scetypes.h"               // u_char, s_char

struct APPROACH_CAMERA;

// What a shot's eye or target is anchored to (DmgCamShotGetBase).
enum DMG_CAM_BASE
{
    DMG_CAM_BASE_PLYR       = 0,        // the player's pos, facing
    DMG_CAM_BASE_PLYR_JOINT = 1,        // the player's joint 2, its facing
    DMG_CAM_BASE_ENE        = 2,        // the attacker's pos (ENE_WRK +0x310), facing
    DMG_CAM_BASE_ENE_JOINT  = 3,        // the attacker's joint 2, its facing
    DMG_CAM_BASE_MID        = 4,        // halfway between the two pos, facing player -> midpoint
    DMG_CAM_BASE_MID_JOINT  = 5,        // halfway between the two joints 2, likewise
                                        // other: the camera's position, the player's facing
};

// One row of the shot table (data 0x2f15d0, 87 rows grouped by kind).
// layout: EE (ROM data)
struct DMG_CAM_SHOT_DAT                 /* 0x40 */
{
    /* 0x00 */ int           kind;      // the attacker kind it serves (PLYR_WRK._147)
    /* 0x04 */ short         pos_follow;// 1: DmgCamShotMain moves the eye with its anchor every frame
    /* 0x06 */ u_char        pos_base;  // DMG_CAM_BASE of the eye
    /* 0x10 */ sceVu0FVECTOR pos;       // the eye in the anchor's frame
    /* 0x20 */ short         tgt_follow;// 1: the target follows its anchor
    /* 0x22 */ u_char        tgt_base;  // DMG_CAM_BASE of the target
    /* 0x30 */ sceVu0FVECTOR tgt;       // the target in the anchor's frame
};
static_assert(sizeof(DMG_CAM_SHOT_DAT) == 0x40, "DMG_CAM_SHOT_DAT size (the table's stride)");
static_assert(offsetof(DMG_CAM_SHOT_DAT, pos_follow) == 0x04, "DMG_CAM_SHOT_DAT.pos_follow");
static_assert(offsetof(DMG_CAM_SHOT_DAT, pos_base) == 0x06, "DMG_CAM_SHOT_DAT.pos_base");
static_assert(offsetof(DMG_CAM_SHOT_DAT, pos) == 0x10, "DMG_CAM_SHOT_DAT.pos");
static_assert(offsetof(DMG_CAM_SHOT_DAT, tgt_follow) == 0x20, "DMG_CAM_SHOT_DAT.tgt_follow");
static_assert(offsetof(DMG_CAM_SHOT_DAT, tgt_base) == 0x22, "DMG_CAM_SHOT_DAT.tgt_base");
static_assert(offsetof(DMG_CAM_SHOT_DAT, tgt) == 0x30, "DMG_CAM_SHOT_DAT.tgt");

// One gathered shot (bss 0x390cd0, 16 of them): the row's eye and target
// in world space.
// layout: host -- dat is an EE pointer at +0x20 (0x30 bytes on the EE).
struct DMG_CAM_SHOT
{
    /* 0x00 */ sceVu0FVECTOR     pos;   // the eye (DmgCamShotSetPos)
    /* 0x10 */ sceVu0FVECTOR     tgt;   // the target (DmgCamShotSetTarget)
    /* 0x20 */ DMG_CAM_SHOT_DAT *dat;   // its table row
};
static_assert(offsetof(DMG_CAM_SHOT, tgt) == 0x10, "DMG_CAM_SHOT.tgt");
static_assert(offsetof(DMG_CAM_SHOT, dat) == 0x20, "DMG_CAM_SHOT.dat");

void DmgCamShotInit(s_char kind, APPROACH_CAMERA *app);    // FF3 0x001374d8: the shots of attacker kind; PosInit / NeckInit from the first (else npos / inpos)
int  DmgCamShotMain(APPROACH_CAMERA *app);                  // FF3 0x00137ba0: steps the shot into PosInit / NeckInit; 0: no shot (or the attacker no longer acts)

#endif /* _INGAME_CAMERA_DMG_CAM_SHOT_H */
