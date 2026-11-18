/* ==========================================================================
 *  ingame/story_night.h
 *
 *  FF3-only module between pss_loop.c and npc_act.c (FF3 0x219e60..0x21a678;
 *  the name follows the story_*.c phase files and the SET_NIGHT_* event
 *  keywords, the code itself is unnamed): phase 0x23 Story_NightChange, the
 *  change of the world side (MapAreaSideSet, 0 / 1) with the room reloaded
 *  around the player, and the night start position the player is put at
 *  (SET_NIGHT_START_POS; save block 44).
 *
 *  NightChangeReq starts it from Story_Normal: SET_NIGHT_FLG without a fade
 *  and IngameScene (the sleep scene) with one. The phase takes the locks of
 *  night_lock_tbl, (fades out,) sets the side, reloads the start room, waits
 *  for the map and the players, puts the player at the start position,
 *  (fades in,) waits for the player's anime 0x9b and returns to the phase
 *  that requested it.
 * ======================================================================== */
#ifndef _INGAME_STORY_NIGHT_H
#define _INGAME_STORY_NIGHT_H

#include "sdk/scetypes.h"

#include <stddef.h>                     // offsetof

// The phase work (bss 0x3f6198, 0x10; NightChangeInit clears it).
// layout: EE -- ints and a short only
typedef struct
{
    /* 0x00 */ int   step;              // one_Story_NightChange's step 0..9
    /* 0x04 */ short ret;               // GPHASE_CTRL.from when the phase began: the phase it returns to
    /* 0x06 */ short _06;
    /* 0x08 */ int   side;              // the side to change to (-1: none)
    /* 0x0c */ int   fade;              // non-zero: fade out and in around the change
} NIGHT_CHANGE_WRK;
static_assert(sizeof(NIGHT_CHANGE_WRK) == 0x10, "NIGHT_CHANGE_WRK");
static_assert(offsetof(NIGHT_CHANGE_WRK, ret) == 0x04, "NIGHT_CHANGE_WRK.ret");
static_assert(offsetof(NIGHT_CHANGE_WRK, side) == 0x08, "NIGHT_CHANGE_WRK.side");
static_assert(offsetof(NIGHT_CHANGE_WRK, fade) == 0x0c, "NIGHT_CHANGE_WRK.fade");

// Where the player stands after the change (bss 0x3f61a8, 0x14; save block
// 44, so the layout is the save format).
// layout: EE -- 4-byte ints and floats only
typedef struct
{
    /* 0x00 */ int   room;              // area; >= 0x3c (and -1): stay in the current room, player not moved
    /* 0x04 */ int   floor;             // MapAreaFloorChange's argument (never set: 0)
    /* 0x08 */ float x;                 // room-relative position (y: the floor height there)
    /* 0x0c */ float z;
    /* 0x10 */ float rot;               // facing, degrees
} NIGHT_START_POS;
static_assert(sizeof(NIGHT_START_POS) == 0x14, "NIGHT_START_POS");
static_assert(offsetof(NIGHT_START_POS, floor) == 0x04, "NIGHT_START_POS.floor");
static_assert(offsetof(NIGHT_START_POS, x) == 0x08, "NIGHT_START_POS.x");
static_assert(offsetof(NIGHT_START_POS, z) == 0x0c, "NIGHT_START_POS.z");
static_assert(offsetof(NIGHT_START_POS, rot) == 0x10, "NIGHT_START_POS.rot");

// Phase 0x23 (gphase table slot 35).
int  init_Story_NightChange(void);      // FF3 0x00219e60
int  end_Story_NightChange(void);       // FF3 0x00219ee8
int  one_Story_NightChange(void);       // FF3 0x00219f38: 1 on the frame it returns

void NightChangeInit(void);             // FF3 0x0021a1d0 (IngameInit; was EquipTrayInit)
// Requests phase 0x23 to change to side (fade non-zero: with a fade) when
// lock 0x13 is free, the game is in Story_Normal, side is not the current
// one, no room is loading and the room passes IngameIsIdOver2e; 1 if requested.
int  NightChangeReq(int side, int fade);    // FF3 0x0021a210

void NightStartPosInit(void);           // FF3 0x0021a2f0
// SET_NIGHT_START_POS: rot in degrees. FF3 has a second integer argument
// (the event's args[4], in a1) that it never stores.
void NightStartPosSet(int room, int unused, float x, float z, float rot);   // FF3 0x0021a350
int  NightStartPosGetSaveData(void **addr); // FF3 0x0021a640: save block 44, 0x14 bytes

#endif /* _INGAME_STORY_NIGHT_H */
