/* ==========================================================================
 *  ingame/cat/cat_ctrl.h
 *
 *  FF3-only cat control (FF3 0x13efa0..0x1441d8; work bss 0x391550, 0xc0 per
 *  cat, one cat). The cat is the event NPC 2 (NPC commands with npc >= 2 go
 *  here with npc - 2). It wanders between six position labels of the real-
 *  world house (table 0x34f920), idles with random motions, can be stroked
 *  by the player (the pet act, player animes 0xa7..0xac and an event camera),
 *  sits at an event rectangle, follows the player through door 0x12e, and
 *  looks at the player or a point. Its model is two CharCatModel (body ch015
 *  and ch019 on the body's root) plus an optional AcsModel.
 *
 *  The behaviour is a small hierarchy of act tables {init, check, main}:
 *  - top (sdata 0x36bed0): act 0 fixed (an NPC_ACT row or NPC_FIX_EVENT put
 *    the cat at one of two fixed positions), act 1 free (wander by label);
 *  - free act, per label: a move kind (sdata 0x36c000: 0 stay -- idle and
 *    the pet act --, 1 loop the label's motion set) and a sub act (sdata
 *    0x36c018: 0 the event spot, 1 the door look);
 *  - stay (sdata 0x36c040): idle motions, then the pet act.
 *  A check returning non-zero starts that act; its main returning non-zero
 *  ends it.
 * ======================================================================== */
#ifndef _INGAME_CAT_CAT_CTRL_H
#define _INGAME_CAT_CAT_CTRL_H

#include <stddef.h>                     // offsetof
#include "sdk/scetypes.h"
#include "sdk/libvu0.h"                 // sceVu0FVECTOR

class CharCatModel;                     // ingame/char_model.h
class AcsModel;                         // ingame/plyr/acs_model.h

#define CAT_NUM         1               // the loops over cats stop at 1
#define CAT_LABEL_NUM   6               // position labels (CatGetLabelTbl)

// One cat's work (bss 0x391550, 0xc0 apart; CatGetWrk). 0x00..0x34 is the
// act state CatActReset clears; the rest is the model and its place.
// layout: host -- mdl, mdl2 and acs are EE pointers (4 bytes at +0x34 /
// +0x38 / +0x3c); the offsets after them are the EE's in the comments.
struct CAT_WRK                          /* 0xc0 on the EE */
{
    /* 0x00 */ s_char no;               // the cat's number
    /* 0x01 */ s_char label;            // its position label (CatGetLabelTbl), the free act's place
    /* 0x02 */ s_char act;              // running top act (0 fixed, 1 free), -1: none (CatActMain picks one)
    /* 0x03 */ s_char step;             // free act: 0 start the label, 1 run it
    /* 0x04 */ s_char move;             // free act: the label's move kind (0 stay, 1 loop)
    /* 0x05 */ s_char sub;              // free act: the label's sub act (0 event spot, 1 door look), -1: none
    /* 0x06 */ s_char move_stat;        // free act: 0 checking the move, 1 running it
    /* 0x07 */ s_char sub_stat;         // free act: 0 checking the sub act, 1 running it
    /* 0x08 */ s_char next_label;       // the label a sub act moved the cat to, -1: none (CatActFreeCheck takes it)
    /* 0x09 */ s_char spot;             // event spot act: its spot (CatGetEvSpotTbl)
    /* 0x0a */ u_char _0a[2];
    /* 0x0c */ int    spot_ready;       // event spot act: 0 while spot_wait counts down
    /* 0x10 */ short  spot_wait;        // event spot act: frames before the next try
    /* 0x12 */ s_char spot_step;        // event spot act: 0 start the motion, 1 wait for its end
    /* 0x13 */ s_char stay_stat;        // stay: 0 idle, 1 pet act, -1: not started
    /* 0x14 */ s_char idle;             // stay: idle kind (0, 1; CatIdleKindSelect)
    /* 0x15 */ s_char idle_step;        // stay: 0 motion set, 1 start / 2 run the idle motion, 3 count
    /* 0x16 */ s_char idle_cnt;         // stay: idle motions left before kind 1
    /* 0x17 */ u_char _17;
    /* 0x18 */ int    react;            // pet act: the cat's reaction motion is running
    /* 0x1c */ s_char pet_step;         // pet act: step (CatPetMain's table 0x2f30a0), -1: none
    /* 0x1d */ s_char pet_next;         // pet act: the step after this one, -1: end
    /* 0x1e */ u_char _1e[2];
    /* 0x20 */ int    pet_again;        // pet act: OK pressed during the stroke
    /* 0x24 */ int    pet_hold;         // pet act: the reaction must finish first
    /* 0x28 */ s_char look_step;        // door look: 0 watch the player, 1 the motion
    /* 0x29 */ s_char eye;              // door look: 0 none, 1 the door, 2 the player
    /* 0x2a */ short  look_wait;        // door look: frames before the motion may play
    /* 0x2c */ short  look_wait2;       // door look: frames before the next coin toss
    /* 0x2e */ u_char evcam;            // door look: the player-camera event camera is set
    /* 0x2f */ s_char door;             // door look: its door (CatGetDoorTbl)
    /* 0x30 */ s_char door_stat;        // door follow: -1 off, 0 the door opens, 1 go, 2 going through
    /* 0x31 */ s_char door_wait;        // door follow: frames before the go motion
    /* 0x32 */ u_char _32[2];
    /* 0x34 */ CharCatModel *mdl;       // the body (part 0), nullptr: none
    /* 0x38 */ CharCatModel *mdl2;      // the part on the body's root joint (part 1)
    /* 0x3c */ AcsModel     *acs;       // its accessory (acs_no), nullptr: none
    /* 0x40 */ sceVu0FVECTOR pos;       // CharCatModel::GetWorldMatrix
    /* 0x50 */ sceVu0FVECTOR rot;       // rot[1]: facing
    /* 0x60 */ int    area;             // the cat's area (PlyrCameraAreaLabel compares a label's with it); -1: the player's (CharCatModel::DrawMdlPre)
    /* 0x64 */ s_char mot;              // the body's anime, -1: none
    /* 0x65 */ s_char mot2;             // the part's anime, -1: none
    /* 0x66 */ u_char _66[0xa];
    /* 0x70 */ sceVu0FVECTOR eye_pos;   // the point it looks at while eye_pos_on
    /* 0x80 */ int    eye_pos_on;       // 0: it looks at the player's head
    /* 0x84 */ int    eye_on;           // the watch control runs (CatSetEyeOn)
    /* 0x88 */ u_char acs_no;           // accessory 1.., 0: none (CatGetAcsDat row acs_no - 1)
    /* 0x89 */ s_char type;             // model type (CatMdlCreate's file row)
    /* 0x8a */ s_char type_old;         // the type before the last change
    /* 0x8b */ u_char _8b;
    /* 0x8c */ int    loaded;           // } all three non-zero: drawn and active (CatIsActive)
    /* 0x90 */ int    visible;          // } its area is drawn (CatVisibleCheck)
    /* 0x94 */ int    active;           // } NPC_ACTIVE_FLG
    /* 0x98 */ int    act_reset;        // CatMain resets the acts once
    /* 0x9c */ s_char run_act;          // 0: an event holds the cat (NPC_FIX_EVENT), else CatMain runs its acts (-1: reset pending / no label)
    /* 0x9d */ s_char run_label;        // the label to put it back at when the event lets go, -1: none
    /* 0x9e */ s_char fix_pos;          // fixed position (CatGetFixPosTbl), -1: none (the fixed act's check)
    /* 0x9f */ u_char _9f;
    /* 0xa0 */ int    lock[CAT_LABEL_NUM];  // labels taken by the NPCs (NPC_LOCK_POS); the free act skips them
    /* 0xb8 */ u_char _b8[8];
};
static_assert(offsetof(CAT_WRK, mdl) == 0x34 || offsetof(CAT_WRK, mdl) == 0x38, "CAT_WRK.mdl (EE 0x34, pointer-aligned on the host)");
static_assert(offsetof(CAT_WRK, pos) == 0x40 || offsetof(CAT_WRK, pos) == 0x50, "CAT_WRK.pos (EE 0x40, 16-aligned after 3 host pointers)");
static_assert(offsetof(CAT_WRK, rot) == offsetof(CAT_WRK, pos) + 0x10, "CAT_WRK.rot (EE 0x50)");
static_assert(offsetof(CAT_WRK, area) == offsetof(CAT_WRK, pos) + 0x20, "CAT_WRK.area (EE 0x60)");
static_assert(offsetof(CAT_WRK, mot) == offsetof(CAT_WRK, pos) + 0x24, "CAT_WRK.mot (EE 0x64)");
static_assert(offsetof(CAT_WRK, eye_pos) == offsetof(CAT_WRK, pos) + 0x30, "CAT_WRK.eye_pos (EE 0x70)");
static_assert(offsetof(CAT_WRK, eye_pos_on) == offsetof(CAT_WRK, pos) + 0x40, "CAT_WRK.eye_pos_on (EE 0x80)");
static_assert(offsetof(CAT_WRK, acs_no) == offsetof(CAT_WRK, pos) + 0x48, "CAT_WRK.acs_no (EE 0x88)");
static_assert(offsetof(CAT_WRK, loaded) == offsetof(CAT_WRK, pos) + 0x4c, "CAT_WRK.loaded (EE 0x8c)");
static_assert(offsetof(CAT_WRK, active) == offsetof(CAT_WRK, pos) + 0x54, "CAT_WRK.active (EE 0x94)");
static_assert(offsetof(CAT_WRK, run_act) == offsetof(CAT_WRK, pos) + 0x5c, "CAT_WRK.run_act (EE 0x9c)");
static_assert(offsetof(CAT_WRK, fix_pos) == offsetof(CAT_WRK, pos) + 0x5e, "CAT_WRK.fix_pos (EE 0x9e)");
static_assert(offsetof(CAT_WRK, lock) == offsetof(CAT_WRK, pos) + 0x60, "CAT_WRK.lock (EE 0xa0)");
static_assert(sizeof(CAT_WRK) == offsetof(CAT_WRK, pos) + 0x80, "CAT_WRK (EE 0xc0 past the pointers)");
static_assert(offsetof(CAT_WRK, door_wait) == 0x31, "CAT_WRK.door_wait");
static_assert(offsetof(CAT_WRK, pet_hold) == 0x24, "CAT_WRK.pet_hold");

// ---- the work and the game's entry points (ingame.c) --------------------
CAT_WRK *CatGetWrk(s_char cat);         // FF3 0x001415b0
int  CatGetSaveData(void **addr);       // FF3 0x001417a0: {bss 0x391550, 0xc0} (save block 55)
void CatCtrlInit(void);                 // FF3 0x00141ff0 (IngameInit)
void CatCtrlLoadRestore(void);          // FF3 0x00142008 (IngameLoadEnd / IngameLoadRestore)
void CatCtrlReset(void);                // FF3 0x00142020 (IngameNewGameReset)
void CatCtrlEnd(void);                  // FF3 0x00142038 (IngameEnd)
void CatCtrlMain(void);                 // FF3 0x00142058 (IngameMain)
int  CatMdlCreate(s_char cat, s_char type, int act_reset);  // FF3 0x00141c58: type -1: the current; 1 when both loads started
int  CatMdlDelete(s_char cat);          // FF3 0x00141e70: GO_DREAM_WORLD (0); 1 when there were models
int  CatMdlChange(s_char cat, s_char type, int act_reset);  // FF3 0x00141f28: NPC_MODEL_CHANGE (type -1: the previous)
void CatDraw(CAT_WRK *cat);             // FF3 0x001420d0: the draw entry's callback (CharDrawCatFunc)

// ---- the cat as event NPC 2 (ev_command.c) -----------------------------
void CatSetPosLabel(s_char cat, s_char label);              // FF3 0x00140378: NPC_SET_POS_LABEL
void CatMotionCall(s_char cat, s_char mot, s_char start);   // FF3 0x00140470: NPC_MOTION_CALL
int  CatMotionIsEnd(s_char cat);        // FF3 0x00142518: NPC_MOTION_CALL's wait
void CatSetActive(s_char cat, int on);  // FF3 0x00142170: NPC_ACTIVE_FLG
void CatFixEvent(s_char cat, int fix, s_char label);        // FF3 0x001421a0: NPC_FIX_EVENT
void CatSetPos(s_char cat, int area, float *pos, float rot);    // FF3 0x001423a0: NPC_SET_POS
void CatSetEyeOn(s_char cat, int on);   // FF3 0x00142598: NPC_EYE_TARGET
void CatSetEyePos(s_char cat, float *pos);  // FF3 0x00142600: NPC_EYE_TARGET's point (nullptr: the player)
void CatSetLabelLock(s_char cat, s_char label, int on);     // FF3 0x00142848: NPC_LOCK_POS

// ---- the NPCs (npc_act.c, n_plyr_camera.c) ------------------------------
int  CatIsHeld(s_char cat);             // FF3 0x001422e0: 1 when an event holds the cat (cat < 0: any cat)
void CatSetFixPos(s_char cat, s_char no);   // FF3 0x00142358: the fixed act's position (-1: none), acts reset
s_char CatGetLabel(s_char cat);         // FF3 0x00142890: its position label
void CatActRestart(s_char cat);         // FF3 0x001428b8: acts reset when active and not held

// ---- the player and the viewfinder --------------------------------------
void CatViewInit(void);                 // FF3 0x0013f0e8: PlyrFinderModeStart with PLYR_WRK._0e0 bit 1
void CatViewCtrl(void);                 // FF3 0x0013f1c8: PlyrFinderCtrl: the stick turns the view
// The cat's view as the game camera: eye at bss 0x3914f0, looking along the
// rotation 0x391500 + 0x391520, fov from the photo zoom (FinderViewCameraCtrl
// with PLYR_WRK _0e0 bit 1).
void CatViewCameraSet(void);            // FF3 0x0013f468
void CatPlyrHitCheck(float r, int area, float *pos, float *pos_old);    // FF3 0x0013fa38: PlyrMapPosUpdate

// ---- the doors (story_door.c) -------------------------------------------
int  CatDoorFollowStart(short door, float *pos, float *tgt);   // FF3 0x001430f0: 1 when cat 0 follows through door (camera pos / target)
void CatDoorFollowGo(void);             // FF3 0x001431f8: MapDoorModeWait: door state 0 -> 1
void CatDoorFollowEnd(void);            // FF3 0x00143228: end_Story_Door_Open: the player's area as "to"

// ---- accessors (callers 0x1af2d8 / 0x1af3a0 not written yet, the rest unreferenced)
int    CatIsLoaded(s_char cat);         // FF3 0x00142120
int    CatGetActive(s_char cat);        // FF3 0x00142148
void   CatSetMdlType(s_char cat, s_char type);  // FF3 0x00142690
void   CatSetAcsNum(s_char cat, u_char no);     // FF3 0x001426c8
s_char CatGetMdlType(s_char cat);       // FF3 0x00142700
u_char CatGetAcsNum(s_char cat);        // FF3 0x00142728

#endif /* _INGAME_CAT_CAT_CTRL_H */
