/* ==========================================================================
 *  ingame/plyr/plyr_shot.h
 *
 *  The shot judging, FF3 0x23c028..0x23e208 (pinned 2026-10-19; FF3-only,
 *  a descriptive name between plyr_mdl.c and plyr_sub.c): each viewfinder
 *  frame the targetable ghosts charge the camera (PlyrShotChargeCtrl); a shot
 *  hits the charged ones (PlyrShotJudge: damage, lens effects, reactions,
 *  the hit effects), scores them (PlyrShotScore: base points times the
 *  level and the twelve bonuses of the score panel) and files the picture
 *  (PlyrShotRecord: the subjects, the album record, the photo sequence).
 *  Also the view turns to a ghost (lens function 3 at the finder start, the
 *  enemy script's EJobC41) and PlyrGetFinderState for the events. FF2 did
 *  the same work in player.c (PhotoDmgChk2, PhotoPointCulcEne2,
 *  PlayerTakePictJob) with other rules.
 * ======================================================================== */
#ifndef _INGAME_PLYR_PLYR_SHOT_H
#define _INGAME_PLYR_PLYR_SHOT_H

#include <stddef.h>                     // offsetof
#include "sdk/scetypes.h"

struct ENE_WRK;                         // ingame/enemy/ene_main.h

// One thing in the picture as the album files it (FF2 SUBJECT_WRK, 6 bytes
// there, 8 here): PlyrShotRecord builds 15 on the stack, AddPhotoData keeps
// the first three (type, no).
struct SUBJECT_WRK                      /* 0x8 */
{
    /* 0x0 */ u_short sp_no;            // never written by FF3's shot code (FF2: the fatal-frame soul entry)
    /* 0x2 */ u_short no;               // SUBJECT_TYPE_SOUL: the soul-list entry (ENE_CMN.soul_no); else 0
    /* 0x4 */ short   type;             // SUBJECT_TYPE_*
    /* 0x6 */ short   _6;
};
static_assert(sizeof(SUBJECT_WRK) == 0x8, "SUBJECT_WRK: PlyrShotSubjectAdd's stride");
static_assert(offsetof(SUBJECT_WRK, no) == 0x2, "SUBJECT_WRK.no");
static_assert(offsetof(SUBJECT_WRK, type) == 0x4, "SUBJECT_WRK.type");

#define SUBJECT_MAX         15          // PlyrShotSubjectAdd stops counting at 14 (the last entry is overwritten)
#define SUBJECT_TYPE_SOUL   0x54        // a ghost (FF2 0x3b: the ghost-list message group)
#define SUBJECT_TYPE_NONE   (-1)        // a photo object: nothing nameable

// PHOTO_WRK_288.on[] rows the shot fills (PlyrShotScoreEne; the score panel
// shows a row while it is non-zero). +0.15 to the multiplier each, +0.3 for
// the triples.
#define SHOT_BONUS_CHARGE       1       // the charge at its maximum
#define SHOT_BONUS_CHARGE_SC    2       //   and in a shutter chance / fatal frame (ENE_WRK flags 0x1800)
#define SHOT_BONUS_CHARGE_KILL  3       //   and the ghost finished
#define SHOT_BONUS_CLOSE        4       // the ghost nearer than 20
#define SHOT_BONUS_CORE         5       // within 20 of the finder centre (ENE_WRK.fdr_dist)
#define SHOT_BONUS_LENS         6       // a lens shot, or with the second charge (PHOTO_WRK_1E8::GetNumSub)
#define SHOT_BONUS_JUST         7       // finished with less than 50 damage to spare
#define SHOT_BONUS_OVER         8       // finished with 200 or more to spare
#define SHOT_BONUS_DOUBLE       9       // two ghosts hit (PHOTO_WRK_288.hit_cnt)
#define SHOT_BONUS_TRIPLE       10      // three or more (+0.3)
#define SHOT_BONUS_DOUBLE_KILL  11      // two finished (PHOTO_WRK_288.kill_cnt)
#define SHOT_BONUS_TRIPLE_KILL  12      // three or more (+0.3)
#define SHOT_SCORE_RAW          13      // on[13]: the sum of the ghosts' scores

// The shot (PlyrFinderShot, plyr_mdl.c). lens: 1 a lens shot, 0 the shutter.
void PlyrShotRecord(void);              // FF3 0x0023c9b8: the picture's subjects, the album record, PhotoSeqStart
void PlyrShotChargeCtrl(void);          // FF3 0x0023d178: each viewfinder frame (PlyrFinderShotCtrl): the charge
void PlyrShotJudge(char lens);          // FF3 0x0023d5f0: the hits, damage and score of a shot

// The view turns.
void PlyrFinderAutoTurn(void);          // FF3 0x0023dc38: PlyrFinderModeStart, lens function 3: turns to the one ghost in view
void FinderViewTurnToEne(float time, ENE_WRK *ene);  // FF3 0x0023e078: to ene (nullptr: the nearest visible acting ghost within 200) over time frames (EJobC41)

// 1 in the viewfinder (PLYR_FLG_800000), 2 going in / out (0x1400000), else 0.
int  PlyrGetFinderState(void);          // FF3 0x0023de58 (EvFinderMode, EvPlyrFinderMode, EvPushPad)

#endif /* _INGAME_PLYR_PLYR_SHOT_H */
