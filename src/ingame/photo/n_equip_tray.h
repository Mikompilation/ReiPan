/* ==========================================================================
 *  ingame/photo/n_equip_tray.h
 *
 *  The companion NPCs' helpers (FF3 0x21d330..0x21e360, FF3-only). The file
 *  keeps the name of FF2's link slot (FF2's n_equip_tray.c is the camera's
 *  equipment tray; its three matches here were wrong); what it holds sits
 *  between the act machine (ingame/npc/npc_act.c) and the NPC controller
 *  (n_plyr_camera.c):
 *    - the player turning to / looking at an NPC during a talk;
 *    - position-label getters and the label's motions (PlyrCameraLabelMotPlay);
 *    - the model data of each NPC model type;
 *    - the NPC's accessories: up to three AcsModels from a per-NPC table,
 *      hung on a joint of the NPC model (or a fixed place), shown or hidden
 *      by the motion that plays, plus the costume's accessory in slot 3;
 *    - the motion's loop SE (NPC 0 only: SE 0x1bf70005 with motion 0xb).
 * ======================================================================== */
#ifndef _INGAME_PHOTO_N_EQUIP_TRAY_H
#define _INGAME_PHOTO_N_EQUIP_TRAY_H

#include <stddef.h>                     // offsetof
#include "sdk/scetypes.h"

struct PLYR_CAMERA_WRK;                 // ingame/photo/n_plyr_camera.h

#define NPC_ACS_NUM  3                  // accessory records per NPC (work acs[0..2])
#define NPC_ACS_SLOT 4                  // with the costume's accessory (acs[3])

// One accessory of an NPC (PlyrCameraGetAcsDat; NPC 0 data 0x35f7a0, NPC 1
// data 0x35f7c8, three each).
// layout: host -- mot is an EE pointer (4 bytes at +0x08).
struct NPC_ACS_DAT                      /* 0xc on the EE */
{
    /* 0x00 */ int file;                // AcsModel::Load's number (A000_PK4 + file), -1: none
    /* 0x04 */ int joint;               // the NPC model's joint it hangs on; -1: not hung, < -1: the fixed place
    /* 0x08 */ const s_char *const *mot;    // three -1-terminated motion lists (nullptr: none): a motion of
                                            //   list n sets the flags to 1 << n; no lists: always shown
};
static_assert(offsetof(NPC_ACS_DAT, joint) == 0x04, "NPC_ACS_DAT.joint");

// The NPC's motion loop SE (sdata 0x3716a8 per NPC, nullptr: none).
struct NPC_MOT_SE                       /* 0x10 */
{
    /* 0x00 */ u_int se;                // SndSePlay's id
    /* 0x04 */ int   mot;               // the motion that plays it
    /* 0x08 */ int   joint;             // where it sounds (PlyrCameraGetJointPos)
    /* 0x0c */ int   time;              // its fade time
};
static_assert(sizeof(NPC_MOT_SE) == 0x10, "NPC_MOT_SE is 0x10 bytes");

// Talks: the player and the NPC.
void   PlyrCameraPlyrTurn(s_char npc, int on);          // FF3 0x0021d330: the player looks at the NPC's head; on: turns to it
void   PlyrCameraPlyrLookEnd(void);                     // FF3 0x0021d3f0: the player's look ends

// Position labels.
int    PlyrCameraLabelIsPlyrArea(s_char npc, s_char label);    // FF3 0x0021d408: the label's area is the player's (label < 0: 0)
u_char PlyrCameraGetLabel84(s_char npc, s_char label);  // FF3 0x0021d468: the label row's byte +0x84
// NPC_MOTION_CALL: PlyrCameraMotPlay(npc, mot, arg) and the motion's face.
void   PlyrCameraMotCall(s_char npc, s_char mot, int arg);     // FF3 0x0021d498
// Plays the label's motion of a kind (0 the list's first, 1 a random other
// one of the list, 2..6 the row's mot2..mot6); returns it, -1: none.
int    PlyrCameraLabelMotPlay(s_char npc, int kind, s_char label, int arg);    // FF3 0x0021d4f0
s_char PlyrCameraGetLabelMot2(s_char npc, s_char label);       // FF3 0x0021d620: the row's mot2 (< 0: no motion after the report)
void   PlyrCameraLabelMove(s_char npc, s_char label);   // FF3 0x0021d650: the NPC to the label's second place (+0x50)
u_char PlyrCameraGetLabel60(s_char npc, s_char label);  // FF3 0x0021d698: the row's byte +0x60 (0: fade, 1: move, else stay)

// Models.
const short *PlyrCameraGetMdlDat(s_char npc, s_char type);     // FF3 0x0021d6c8: {chr, type, mot file} of a model type
const short *PlyrCameraGetMdlFile(s_char npc, s_char type);    // FF3 0x0021d708: {mdl file, shape file (-1: none)}

// Accessories (work acs[] / acs_flg[]).
const NPC_ACS_DAT *PlyrCameraGetAcsDat(s_char npc, s_char n);  // FF3 0x0021d780
void PlyrCameraAcsLoadRestore(PLYR_CAMERA_WRK *cam);    // FF3 0x0021d7b0: no AcsModels after a load
void PlyrCameraAcsCreate(s_char npc);                   // FF3 0x0021d7e0 (PlyrCameraMdlCreate)
int  PlyrCameraAcsIsReady(PLYR_CAMERA_WRK *cam);        // FF3 0x0021d998: every AcsModel is loaded (sets their first fade)
void PlyrCameraAcsRelease(PLYR_CAMERA_WRK *cam);        // FF3 0x0021da60 (PlyrCameraMdlRelease)
void PlyrCameraAcsReset(PLYR_CAMERA_WRK *cam);          // FF3 0x0021dac8: flags cleared (PlyrCameraWrkReset)
void PlyrCameraAcsMotSet(s_char npc, s_char mot);       // FF3 0x0021daf8: the flags for motion mot (PlyrCameraMotPlay)
void PlyrCameraAcsSetCoord(s_char npc);                 // FF3 0x0021de48: hangs them on the model (PlyrCameraUpdate, once ready)
void PlyrCameraAcsUpdate(PLYR_CAMERA_WRK *cam);         // FF3 0x0021dfc8: the trigger and the fades (PlyrCameraUpdate)
void PlyrCameraAcsDrawPre(PLYR_CAMERA_WRK *cam);        // FF3 0x0021e080 (PlyrCameraDraw, before the model)
void PlyrCameraAcsDrawPost(PLYR_CAMERA_WRK *cam);       // FF3 0x0021e0d8 (PlyrCameraDraw, after the model)

void PlyrCameraNpc0Mot16(s_char npc);                   // FF3 0x0021e190: NPC 0 plays motion 0x10 (idle / greet steps)

// The motion loop SE (plyr_camera_se, n_plyr_camera.h).
void PlyrCameraSeReset(s_char npc);                     // FF3 0x0021e1d0 (PlyrCameraMdlCreate)
void PlyrCameraMotSe(s_char npc, s_char mot);           // FF3 0x0021e1f0 (PlyrCameraMotPlay)
void PlyrCameraSeVolUpdate(s_char npc);                 // FF3 0x0021e2c0 (PlyrCameraUpdate)

#endif /* _INGAME_PHOTO_N_EQUIP_TRAY_H */
