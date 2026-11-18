/* ==========================================================================
 *  ingame/enemy/random_ghost.h
 *
 *  The random ghosts (FF3-only, FF3 0x24da20..0x24f398; no FF2 code). An
 *  area the event script switched on (RANDOM_GHOST_ON area, set) draws its
 *  wandering ghosts (type 1 enemies, ingame/enemy/enemy.c) from one of 32
 *  ghost sets of four rows. A timer (2700..4500 frames, NTSC) counts down
 *  while fewer than two ghosts act; when it runs out the rolled appearance
 *  kind decides where the selected ghost shows up:
 *    0  the timer itself: a random spot 60..100 units around the player
 *    1  leaving a door (PlyrDoorEnd): 40 units ahead
 *    2  raising the camera (PlyrFinderShotCtrl): 16 units ahead
 *    3  walking into one of the area's appearance zones (RegDat event
 *       rectangles): the zone's own point and facing
 *  The row of the ghost that appeared last sets the chance that the next
 *  selected one follows it (else the round is rolled again); a ghost may
 *  bring its voice stream over a ducked map BGM and is released when the
 *  player changes to an area its row does not list. With the dream
 *  candle out (CandleControl::out) the timer runs 2.5 times faster, ghosts
 *  0x15 / 0x1a weigh more and enemy 0x10a (type 0) is loaded.
 *
 *  The 0x24e8b0..0x24f318 part was savepoint_top.c's inferred slot (FF2 low
 *  matches SavePointTopInit / ConfPad / DispInit); the code works on this
 *  module's work 0x3f7a78 and has nothing to do with save points.
 * ======================================================================== */
#ifndef _INGAME_ENEMY_RANDOM_GHOST_H
#define _INGAME_ENEMY_RANDOM_GHOST_H

#include <stddef.h>                     // offsetof
#include "sdk/scetypes.h"
#include "sdk/libvu0.h"                 // sceVu0FVECTOR

#define RAND_GHOST_AREA_NUM 60          // rows of rand_ghost_area (RANDOM_GHOST_ON / OFF take area < 60)
#define RAND_GHOST_SET_ROW  4           // rows per ghost set

// RAND_GHOST_INFO.kind: where the next ghost appears (RandGhostKindSet).
enum RAND_GHOST_KIND
{
    RAND_GHOST_KIND_TIMER = 0,          // around the player when the timer runs out
    RAND_GHOST_KIND_DOOR  = 1,          // ahead of the player leaving a door
    RAND_GHOST_KIND_CAM   = 2,          // ahead of the player in the viewfinder (needs item 8)
    RAND_GHOST_KIND_ZONE  = 3,          // at an appearance zone of the area
};

// One row of a ghost set (data 0x330c60 + set * 0x60, 0x18 bytes; no
// pointers, so the host layout is the ROM's).
struct RAND_GHOST_ROW
{
    /* 0x00 */ short   id;              // the ghost (enemy id of type 1), -1: none
    /* 0x02 */ short   id_zone;         // the ghost for kind 3 (RandGhostGetId), -1: id; a row with -1 is never picked
    /* 0x04 */ u_char  weight;          // RandGhostSelect's weight
    /* 0x05 */ u_char  bgm_chance;      // percent: its voice stream starts (RandGhostBgmStart)
    /* 0x06 */ u_char  follow[4];       // percent per row: that row's ghost may come next after this one (RandGhostFollowCheck)
    /* 0x0a */ u_char  _0a;
    /* 0x0b */ u_char  level;           // above EneGetMinLevel it does not appear (RandGhostAppear)
    /* 0x0c */ u_char  once;            // 1: kept over area changes until it has seen 'life' of them
    /* 0x0d */ u_char  _0d;
    /* 0x0e */ short   area[5];         // the areas it stays in on an area change, -1: none
};
static_assert(sizeof(RAND_GHOST_ROW) == 0x18, "RAND_GHOST_ROW is 0x18 bytes");
static_assert(offsetof(RAND_GHOST_ROW, follow) == 0x06, "RAND_GHOST_ROW.follow");
static_assert(offsetof(RAND_GHOST_ROW, level) == 0x0b, "RAND_GHOST_ROW.level");
static_assert(offsetof(RAND_GHOST_ROW, once) == 0x0c, "RAND_GHOST_ROW.once");
static_assert(offsetof(RAND_GHOST_ROW, area) == 0x0e, "RAND_GHOST_ROW.area");

// An appearance zone (lists at data 0x3318e0.., ended by label 0x7fffffff).
struct RAND_GHOST_ZONE
{
    /* 0x00 */ int           label;     // RegDat event label (MrecIsInEvent), -1: skipped
    /* 0x04 */ int           _04[3];
    /* 0x10 */ sceVu0FVECTOR pos;       // where the ghost appears
    /* 0x20 */ float         rot;       // its facing in degrees
    /* 0x24 */ int           _24[3];
};
static_assert(sizeof(RAND_GHOST_ZONE) == 0x30, "RAND_GHOST_ZONE is 0x30 bytes");
static_assert(offsetof(RAND_GHOST_ZONE, pos) == 0x10, "RAND_GHOST_ZONE.pos");
static_assert(offsetof(RAND_GHOST_ZONE, rot) == 0x20, "RAND_GHOST_ZONE.rot");

// The selected ghost: the work keeps the next one (rand_ghost_wrk.info),
// each type-1 enemy work its own copy (ENE_WRK +0x2b4; RandGhostSelect,
// RandGhostInfoCopy). EE 0x20 bytes.
// layout: host -- row and zone are EE pointers (+0x18, +0x1c).
struct RAND_GHOST_INFO
{
    /* 0x00 */ float                  timer;    // frames to the next appearance (work only)
    /* 0x04 */ int                    kind;     // RAND_GHOST_KIND
    /* 0x08 */ int                    area;     // the area it was selected in
    /* 0x0c */ int                    life;     // area changes a once-row ghost stays for (2..4)
    /* 0x10 */ int                    cnt;      // area changes seen (from 1)
    /* 0x14 */ int                    row_no;   // its row in the set, -1: none selected
    /* 0x18 */ const RAND_GHOST_ROW  *row;
    /* 0x1c */ const RAND_GHOST_ZONE *zone;     // kind 3: the zone it appears at
};
static_assert(offsetof(RAND_GHOST_INFO, row_no) == 0x14, "RAND_GHOST_INFO.row_no");
static_assert(offsetof(RAND_GHOST_INFO, zone) == offsetof(RAND_GHOST_INFO, row) + sizeof(void *),
              "RAND_GHOST_INFO.zone (EE 0x1c)");

// The work, bss 0x3f7a78 (EE 0x34).
// layout: host -- follow and the info's pointers are EE pointers.
struct RAND_GHOST_WRK
{
    /* 0x00 */ u_char           fired;      // the timer ran out this round (RandGhostMain)
    /* 0x01 */ u_char           reroll;     // every row's ghost was live: roll again once one goes
    /* 0x02 */ short            area;       // current area (RandGhostAreaChange), -1: none
    /* 0x04 */ int              adpcm;      // the ghost voice stream's file, -1: none
    /* 0x08 */ const u_char    *follow;     // follow[] of the last appeared ghost's row, nullptr: none
    /* 0x0c */ short            candle_id;  // the candle-out enemy (0x10a), -1: none, -2: could not load
    /* 0x10 */ int              stream;     // StreamAutoPlayPitch id, -1: none
    /* 0x14 */ RAND_GHOST_INFO  info;       // the next ghost
};
static_assert(offsetof(RAND_GHOST_WRK, area) == 0x02, "RAND_GHOST_WRK.area");
static_assert(offsetof(RAND_GHOST_WRK, adpcm) == 0x04, "RAND_GHOST_WRK.adpcm");
static_assert(offsetof(RAND_GHOST_WRK, follow) == 0x08, "RAND_GHOST_WRK.follow");
static_assert(offsetof(RAND_GHOST_WRK, candle_id) == 0x08 + sizeof(void *), "RAND_GHOST_WRK.candle_id (EE 0x0c)");

// One area's switch, bss 0x3f7ab0 (60 rows, save_game_data entry 43).
struct RAND_GHOST_AREA
{
    /* 0x00 */ u_char off;              // 1: no ghosts here (RANDOM_GHOST_OFF)
    /* 0x04 */ int    set;              // the ghost set, -1: none
};
static_assert(sizeof(RAND_GHOST_AREA) == 8, "RAND_GHOST_AREA is 8 bytes");
static_assert(offsetof(RAND_GHOST_AREA, set) == 4, "RAND_GHOST_AREA.set");

extern RAND_GHOST_WRK  rand_ghost_wrk;                          // bss 0x3f7a78
extern RAND_GHOST_AREA rand_ghost_area[RAND_GHOST_AREA_NUM];    // bss 0x3f7ab0

struct ENE_WRK;

void  RandGhostWrkClear(void);              // FF3 0x0024da20: EnemyMainInit, IngameNewGameReset
void  RandGhostSetAll(void);                // FF3 0x0024daa0: kind and ghost rolled
void  RandGhostMain(void);                  // FF3 0x0024dac0: EnemyMain, first
void  RandGhostAreaChange(short area);      // FF3 0x0024dd98: AREA_CHANGE, story_door.c
void  RandGhostReleaseAll(void);            // FF3 0x0024e0f8: GameModel's file-cache free hook 0
void  RandGhostCandleRelease(void);         // FF3 0x0024e158: GameModel's free hook 1
void  RandGhostCntReset(ENE_WRK *ene);      // FF3 0x0024e190: 0x23cad0
void  RandGhostTimeSet(int time);           // FF3 0x0024e1a0: -1 rolls it
short RandGhostGetId(const RAND_GHOST_WRK *wrk);    // FF3 0x0024e8b0
int   RandGhostAppearReq(u_char kind);      // FF3 0x0024ea08: 1 when the ghost appeared
void  RandGhostSetPos(ENE_WRK *ene);        // FF3 0x0024ea88: SetCommonDat (type 1)
void  RandGhostSetRot(ENE_WRK *ene);        // FF3 0x0024ec80: SetCommonDat (type 1)
void  RandGhostBgmStop(void);               // FF3 0x0024ef88: EneRelease (type 1)
void  RandGhostTimeReset(void);             // FF3 0x0024f1f8: the candle relit (CandleAnimeFadeBegin)
void  RandGhostEneRelease(ENE_WRK *ene);    // FF3 0x0024f228: EneRelease (types 0 / 1)
void  RandGhostInfoCopy(RAND_GHOST_INFO *dst, const RAND_GHOST_INFO *src);  // FF3 0x0024f260: the enemy script op 0x18c0f0
void  RandGhostOn(int area, int set);       // FF3 0x0024f2a8: RANDOM_GHOST_ON
void  RandGhostOff(int area);               // FF3 0x0024f2e8: RANDOM_GHOST_OFF
void  RandGhostAllOff(void);                // FF3 0x0024f318: RANDOM_GHOST_ALL_OFF
void  RandGhostReq(void);                   // FF3 0x0024f350: RANDOM_GHOST_REQ, empty
int   RandGhostSaveGet(void **addr);        // FF3 0x0024f358: {0x3f7ab0, 0x1e0}

#endif /* _INGAME_ENEMY_RANDOM_GHOST_H */
