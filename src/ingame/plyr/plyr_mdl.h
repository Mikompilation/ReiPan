/* ==========================================================================
 *  ingame/plyr/plyr_mdl.h
 *
 *  The player's look and viewfinder control, FF3 0x239ac0..0x23c028
 *  (pinned 2026-10-15; FF2 plyr_mdl.c's slot by link order, but the code is
 *  FF3's own): the look targets the player model's watch control turns the
 *  head to (ten rows by priority: the right stick, a scripted point, the
 *  event's direct look, enemies, the camera NPCs, the cat, the per-area look
 *  spots, lamps and map objects), then going into / out of the viewfinder
 *  and the shutter (film, lens shots, the photo phase).
 * ======================================================================== */
#ifndef _INGAME_PLYR_PLYR_MDL_H
#define _INGAME_PLYR_PLYR_MDL_H

#include <stddef.h>                     // offsetof
#include "sdk/scetypes.h"

struct PLYR_WRK;                        // ingame/plyr/player.h
class CharWatchControl;                 // ingame/char_model_sub.h
typedef CharWatchControl CHAR_MDL_CTRL;
struct CHAR_WATCH_PRM;                  // ingame/ingame.h

// ---- the look targets (0x239ac0..0x23af80) ----

#define PLYR_LOOK_NUM   10              // rows; PlyrLookMain takes priorities 1..5 in turn

// A parameter row, data 0x330a00 + no * 0x10.
// layout: host -- func is an EE code pointer (4 bytes at +0x0c).
struct PLYR_LOOK_PRM                    /* 0x10 on the EE */
{
    /* 0x00 */ u_char prio;             // 1..5: PlyrLookMain asks the rows of priority 1 first
    /* 0x01 */ u_char _01[3];
    /* 0x04 */ float  dist;             // how far a target may be (PlyrLookGetDist); 0: no limit read
    /* 0x08 */ float  time;             // frames (x frame scale) the head stays on one target; -1: no limit
                                        //   (PlyrLookTimeCheck); row 2's is set by PlyrLookDirectSet
    /* 0x0c */ int  (*func)(void);      // the row's search: 1 with a target in its look work
};
static_assert(offsetof(PLYR_LOOK_PRM, dist) == 0x04, "PLYR_LOOK_PRM.dist");
static_assert(offsetof(PLYR_LOOK_PRM, time) == 0x08, "PLYR_LOOK_PRM.time");

// A look work, bss 0x3f7830 + no * 0x30.
// layout: host -- target_old, target and prm are EE pointers (4 bytes at
// +0x08, +0x0c, +0x24); pos is a quadword on both.
struct PLYR_LOOK_WRK                    /* 0x30 on the EE */
{
    /* 0x00 */ float          time;     // frames left on the current target (PlyrLookTimeStep)
    /* 0x04 */ short          _04;
    /* 0x06 */ short          no;       // the row that found the target (PlyrLookMain: 2 still runs under lock 3)
    /* 0x08 */ void          *target_old;   // the target the time was started for
    /* 0x0c */ void          *target;   // what is looked at (an ENE_WRK, PLYR_CAMERA_WRK, CAT_WRK, look spot,
                                        //   lamp or map object work)
    /* 0x10 */ sceVu0FVECTOR  pos;      // its position
    /* 0x20 */ float          dist;     // its distance from the head (joint 2); the nearest wins
    /* 0x24 */ PLYR_LOOK_PRM *prm;
};
static_assert(offsetof(PLYR_LOOK_WRK, no) == 0x06, "PLYR_LOOK_WRK.no");
static_assert(offsetof(PLYR_LOOK_WRK, target) > offsetof(PLYR_LOOK_WRK, target_old), "PLYR_LOOK_WRK.target follows target_old");
static_assert(offsetof(PLYR_LOOK_WRK, pos) % 16 == 0, "PLYR_LOOK_WRK.pos is a quadword");
static_assert(offsetof(PLYR_LOOK_WRK, dist) == offsetof(PLYR_LOOK_WRK, pos) + 0x10, "PLYR_LOOK_WRK.dist follows pos");

// A look spot of row 7, data 0x32fec0 (90 rows): a point the player looks
// at while in its area.
struct PLYR_LOOK_SPOT                   /* 0x20 */
{
    /* 0x00 */ int           area;
    /* 0x04 */ int           _04[3];
    /* 0x10 */ sceVu0FVECTOR pos;
};
static_assert(sizeof(PLYR_LOOK_SPOT) == 0x20, "PLYR_LOOK_SPOT is 0x20 bytes");
static_assert(offsetof(PLYR_LOOK_SPOT, pos) == 0x10, "PLYR_LOOK_SPOT.pos");

int   PlyrLookPosApply(CHAR_MDL_CTRL *ctrl, CHAR_WATCH_PRM *prm);  // FF3 0x00239ac0: a set look position overrides the targets
void  PlyrLookPosSet(float *pos);       // FF3 0x00239b08: when none is set, PLYR_WRK flags PLYR_FLG_LOOK and the position
void  PlyrLookPosClear(void);           // FF3 0x00239b38
PLYR_LOOK_PRM *PlyrLookGetPrm(int no);  // FF3 0x00239b68
PLYR_LOOK_WRK *PlyrLookGetWrk(int no);  // FF3 0x00239b88
void  PlyrLookReset(PLYR_WRK *plyr);    // FF3 0x00239ba8: PlyrMdlIsReady, once the models are loaded (was plyr_mdlBankPlay)
int   PlyrLookTimeCheck(int no);        // FF3 0x00239c40: 1 when look no may take its candidate
void  PlyrLookTimeStep(PLYR_LOOK_WRK *look);    // FF3 0x00239cd0: a new target restarts the time; one frame off
int   PlyrLookTimeCheckId(int no, int id, char on); // FF3 0x00239d50: as PlyrLookTimeCheck for an id (no callers)
float PlyrLookGetDist(int no);          // FF3 0x00239e58
void  PlyrLookDirectSet(float *pos, short sec);  // FF3 0x0023a0c8: look at pos for sec seconds (x30 frames, x25 on PAL)
void  PlyrLookDirectStop(void);         // FF3 0x0023a158
int   PlyrLookIsEnable(void);           // FF3 0x0023ac08: not mode 9, outside the viewfinder, no anime that blocks it
void  PlyrLookMain(void);               // FF3 0x0023ac68: PlyrMdlMain outside the viewfinder (was FUN_0023ac68)
int   PlyrAreaLineHitCheck(float *from, float *to);  // FF3 0x0023aef0: 1 when the room or the area blocks the line
int   PlyrAnimeIsLookOff(void);         // FF3 0x0023af80: the anime running turns the look off

// ---- the viewfinder (0x23b000..0x23c028) ----
// PLYR_WRK.flags: PLYR_FLG_400000 going in, PLYR_FLG_800000 in it,
// PLYR_FLG_1000000 coming out (player.h).

void PlyrFinderStartCheck(void);        // FF3 0x0023b000: the finder button starts it (PlyrNormalCtrl, PlyrCrawlCtrl, PlyrDamageCtrl)
void PlyrFinderModeStart(void);         // FF3 0x0023b0b0: into the viewfinder (nothing while a finder flag is set)
void PlyrFinderStartEnd(void);          // FF3 0x0023b4f8: PlayerMain with PLYR_FLG_400000: the anime ended, camera up
void PlyrFinderEndCheck(void);          // FF3 0x0023b568: the finder-off button ends it (PlyrFinderCtrl)
void PlyrFinderModeEnd(void);           // FF3 0x0023b5d8: out of it (nothing unless a finder flag 0x1c00000 is set)
void PlyrFinderEndEnd(void);            // FF3 0x0023b898: PlayerMain with PLYR_FLG_1000000: the anime ended, out
void PlyrFinderModeCancel(void);        // FF3 0x0023b930: drops the viewfinder at once (a hold, a hit, a model change)
int  PlyrIsFinderModeOff(void);         // FF3 0x0023b9d8: no finder flag 0x1c00000 set
int  PlyrFinderShotCtrl(void);          // FF3 0x0023ba00: the shutter; non-zero skips PlyrFinderEndCheck (PlyrFinderCtrl)
int  PlyrFinderShotCanTake(void);       // FF3 0x0023bc18: charged and a film left (takes one), else the empty SE
void PlyrFinderShot(char lens);         // FF3 0x0023bd08: a shot (lens 1: a lens shot); a scored one goes to phase 33
int  PlyrFinderShutterCheck(void);      // FF3 0x0023bde0
int  PlyrFinderLensShotCheck(void);     // FF3 0x0023be60
void PlyrFinderShotSpots(char lens);    // FF3 0x0023bf98: the photo spots in the frame (not for lens equips 5 / 6)

#endif /* _INGAME_PLYR_PLYR_MDL_H */
