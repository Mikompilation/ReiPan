/* ==========================================================================
 *  ingame/npc/npc_act.h
 *
 *  The companion NPCs' act machine and data (FF3 0x21a678..0x21d330,
 *  FF3-only). An NPC's act block (PLYR_CAMERA_ACT, ingame/photo/
 *  n_plyr_camera.h) runs a byte-code script from NPC_OBJ (npc/npc.obj,
 *  loaded to EE 0x12f7000 at boot): PlyrCameraActStep runs ops until one
 *  waits; the ops jump on conditions (the NPC's free state, event queues,
 *  greet / report / chat checks) and call the act hooks of n_plyr_camera.c.
 *  The rest is the NPCs' data and what the hooks use with it: position
 *  labels (where an NPC stands, its motion, the doors near it, where the
 *  player is put for a talk), serif lines (subtitle or message), event
 *  cameras for the talks, NPC 0's area remap, the face of each motion,
 *  and the player push / sight / distance checks.
 *  NPC_OBJ: a u16 offset table of script sets, each a u16 table of entries
 *  per NPC (PlyrCameraActStart); ops are one byte, u16 operands little
 *  endian, jump targets relative to the file start.
 * ======================================================================== */
#ifndef _INGAME_NPC_NPC_ACT_H
#define _INGAME_NPC_NPC_ACT_H

#include <stddef.h>                     // offsetof
#include "sdk/scetypes.h"
#include "ingame/photo/n_plyr_camera.h" // PLYR_CAMERA_ACT

#define NPC_OBJ_EE_ADRS 0x12f7000       // NPC_OBJ's load address (Boot_Load)

// One position label of an NPC (PlyrCameraGetLabel; NPC 0 data 0x35d980,
// 20 rows; NPC 1 0x35f330, 2 rows): a place the NPC stands in an area.
struct NPC_LABEL                        /* 0x90 */
{
    /* 0x00 */ s_char side;             // EvNowSynchroMode's world (0 real, 1 dream side)
    /* 0x01 */ u_char _01[3];
    /* 0x04 */ int    area;
    /* 0x08 */ u_char _08[8];
    /* 0x10 */ float  pos[4];           // xyz; [3] the facing in degrees
    /* 0x20 */ s_char mot;              // motion list (PlyrCameraGetMotList), -1: none
    /* 0x21 */ u_char _21[3];
    /* 0x24 */ int    door[2];          // door rectangles of PlyrCameraLabelDoorCheck kinds 0 / 1, -1: none
    /* 0x2c */ int    _2c;
    /* 0x30 */ float  plyr0[4];         // the player's place for a kind-0 talk (xyz, degrees; -1: none)
    /* 0x40 */ s_char mot2;             // PlyrCameraLabelMotPlay kind 2 (PlyrCameraGetLabelMot2; < 0: no motion after the report)
    /* 0x41 */ s_char mot3;             // kind 3
    /* 0x42 */ u_char _42[0xe];
    /* 0x50 */ float  _50[4];           // a second place (xyz, degrees; -1: none)
    /* 0x60 */ u_char _60;              // PlyrCameraGetLabel60 (0 fade, 1 move, else stay)
    /* 0x61 */ u_char _61[0xf];
    /* 0x70 */ float  plyr1[4];         // the player's place for a kind-1 talk
    /* 0x80 */ s_char mot4;             // PlyrCameraLabelMotPlay kind 4 (the idle look)
    /* 0x81 */ s_char mot5;             // kind 5 (the chat)
    /* 0x82 */ s_char door_list;        // PlyrCameraGetDoorList index (kind 3), -1: none
    /* 0x83 */ s_char mot6;             // PlyrCameraLabelMotPlay kind 6 (the greet motion); PlyrCameraPickLabel's exclusion
                                        //   (0x16 outside areas 0x31 / 0x33); >= 0: no list motion
    /* 0x84 */ u_char _84;              // PlyrCameraGetLabel84 (2: NPC 0 takes the cat model)
    /* 0x85 */ s_char cat;              // NPC 0: the head's _02 and cat 0's CatSetFixPos value, -1: none
    /* 0x86 */ u_char _86[0xa];
};
static_assert(sizeof(NPC_LABEL) == 0x90, "NPC_LABEL is 0x90 bytes");
static_assert(offsetof(NPC_LABEL, area) == 0x04, "NPC_LABEL.area");
static_assert(offsetof(NPC_LABEL, pos) == 0x10, "NPC_LABEL.pos");
static_assert(offsetof(NPC_LABEL, mot) == 0x20, "NPC_LABEL.mot");
static_assert(offsetof(NPC_LABEL, door) == 0x24, "NPC_LABEL.door");
static_assert(offsetof(NPC_LABEL, plyr0) == 0x30, "NPC_LABEL.plyr0");
static_assert(offsetof(NPC_LABEL, mot2) == 0x40, "NPC_LABEL.mot2");
static_assert(offsetof(NPC_LABEL, mot3) == 0x41, "NPC_LABEL.mot3");
static_assert(offsetof(NPC_LABEL, _50) == 0x50, "NPC_LABEL._50");
static_assert(offsetof(NPC_LABEL, _60) == 0x60, "NPC_LABEL._60");
static_assert(offsetof(NPC_LABEL, plyr1) == 0x70, "NPC_LABEL.plyr1");
static_assert(offsetof(NPC_LABEL, door_list) == 0x82, "NPC_LABEL.door_list");
static_assert(offsetof(NPC_LABEL, mot4) == 0x80, "NPC_LABEL.mot4");
static_assert(offsetof(NPC_LABEL, mot5) == 0x81, "NPC_LABEL.mot5");
static_assert(offsetof(NPC_LABEL, mot6) == 0x83, "NPC_LABEL.mot6");
static_assert(offsetof(NPC_LABEL, _84) == 0x84, "NPC_LABEL._84");
static_assert(offsetof(NPC_LABEL, cat) == 0x85, "NPC_LABEL.cat");

// One serif line (PlyrCameraGetSerifList).
struct NPC_SERIF                        /* 0xc */
{
    /* 0x00 */ int type;                // 0 a subtitle (voice) at the NPC, 1 message 0x71 in a window
    /* 0x04 */ int id;                  // the subtitle / message, -1: none
    /* 0x08 */ int sub;                 // the after-report subtitle (PlyrCameraSerifStart2); its low
                                        // byte is the line's event-camera row (-2: any of the label)
};
static_assert(sizeof(NPC_SERIF) == 0xc, "NPC_SERIF is 0xc bytes");
static_assert(offsetof(NPC_SERIF, sub) == 0x08, "NPC_SERIF.sub");

// One event camera of a talk (PlyrCameraGetCamList).
struct NPC_EV_CAM                       /* 0x24 */
{
    /* 0x00 */ s_char label;            // the position label it serves, < 0: unused
    /* 0x01 */ u_char _01[3];
    /* 0x04 */ float  pos[3];
    /* 0x10 */ float  tgt[3];
    /* 0x1c */ float  fov;
    /* 0x20 */ float  roll;
};
static_assert(sizeof(NPC_EV_CAM) == 0x24, "NPC_EV_CAM is 0x24 bytes");
static_assert(offsetof(NPC_EV_CAM, pos) == 0x04, "NPC_EV_CAM.pos");
static_assert(offsetof(NPC_EV_CAM, tgt) == 0x10, "NPC_EV_CAM.tgt");
static_assert(offsetof(NPC_EV_CAM, fov) == 0x1c, "NPC_EV_CAM.fov");
static_assert(offsetof(NPC_EV_CAM, roll) == 0x20, "NPC_EV_CAM.roll");

// NPC 0's area remap row (PlyrCameraGetAreaList, data 0x35f2b8).
struct NPC_AREA_LABEL                   /* 0xc */
{
    /* 0x00 */ int area;                // the player's new area
    /* 0x04 */ int from;                // the area of NPC 0's current label
    /* 0x08 */ int to;                  // the area to pick a label in
};
static_assert(sizeof(NPC_AREA_LABEL) == 0xc, "NPC_AREA_LABEL is 0xc bytes");
static_assert(offsetof(NPC_AREA_LABEL, to) == 0x08, "NPC_AREA_LABEL.to");

// The act block.
void PlyrCameraActClear(PLYR_CAMERA_ACT *act);          // FF3 0x0021a678
void PlyrCameraActPcLoad(PLYR_CAMERA_ACT *act);         // FF3 0x0021a698: after a load, the pc from its saved offset
void PlyrCameraActPcSave(PLYR_CAMERA_ACT *act);         // FF3 0x0021a6c0: before saving, the pc as an offset
// Cleared, NPC no, script set / entry started, label 0xfe (none).
void PlyrCameraActReset(PLYR_CAMERA_ACT *act, s_char no, s_char set, s_char entry);   // FF3 0x0021a818
void PlyrCameraActStep(PLYR_CAMERA_ACT *act);           // FF3 0x0021a890
int  EvNowSynchroMode(void);                            // FF3 0x0021a8b0: 1 on the dream side (MapAreaSideGet() == 1)

// Checks between the NPCs and the player.
int    PlyrCameraAreaIsDrawn(int area);                 // FF3 0x0021a8d8: one of the two loaded rooms is area and drawn
u_char PlyrCameraSightCheck(s_char npc, float dist, float ang);    // FF3 0x0021a940: bit 0 the NPC sees the player, bit 1 the player sees the NPC
int    PlyrCameraDistCheck(s_char npc, s_char cmp, float dist);    // FF3 0x0021aa68: XZ distance against dist (cmp 0 >=, 1 ==, 2 <=)
void   PlyrCameraPlyrPush(float r, int area, float *pos, float *pos_old);  // FF3 0x0021ab50 (PlyrMapPosUpdate)

// Talk flags (work +0xf2): 1 a talk / chat runs, 2 a line is shown.
void PlyrCameraTalkFlgSet(s_char no, u_char bits);      // FF3 0x0021ad80
void PlyrCameraTalkFlgClr(s_char no, u_char bits);      // FF3 0x0021adc0
int  PlyrCameraIsTalking(u_char no);                    // FF3 0x0021ae38: no >= 2: any NPC
int  PlyrCameraIsSpeaking(u_char no);                   // FF3 0x0021aeb8: no >= 2: any NPC

// The data.
const NPC_LABEL *PlyrCameraGetLabel(s_char npc, s_char label, s_char *num);    // FF3 0x0021c038
const s_char *PlyrCameraGetMotList(s_char npc, s_char list, s_char *num);      // FF3 0x0021c080
s_char PlyrCameraGetMotFace(s_char npc, s_char mot);    // FF3 0x0021c120: the face motion mot calls

// Position labels.
void   PlyrCameraSetLabelPos(s_char npc, int area, const float *pos);  // FF3 0x0021c478: pos[3] in degrees
s_char PlyrCameraSetPosLabel(s_char npc, s_char label); // FF3 0x0021c510: NPC_SET_POS_LABEL; returns label
s_char PlyrCameraSetRandLabel(s_char npc, int area, int strict);       // FF3 0x0021c688: a random label of area; returns it
s_char PlyrCameraAreaLabel(s_char npc, s_char label, int area);        // FF3 0x0021c798: NPC 0's label for the player's new area
int    PlyrCameraLabelDoorCheck(s_char npc, s_char kind, s_char label); // FF3 0x0021ce30: the player is in the label's doors of a kind

// Serif lines and the talk camera.
s_char PlyrCameraPickSerif(s_char npc, s_char kind);    // FF3 0x0021c950
s_char PlyrCameraGetSerifCam(s_char npc, s_char kind, s_char sel);     // FF3 0x0021c9b0: -1: none
s_char PlyrCameraSerifStart(s_char npc, s_char kind, s_char sel, int face);    // FF3 0x0021ca48: sel < 0 picks; returns the line
void   PlyrCameraSerifStart2(s_char npc, s_char kind, s_char sel);     // FF3 0x0021cc18
int    PlyrCameraSerifIsEnd(s_char npc);                // FF3 0x0021cd60
void   PlyrCameraEvCamSet(float fov, float roll, float *pos, float *tgt);  // FF3 0x0021cf68
int    PlyrCameraEvCamStart(s_char npc, s_char kind, s_char sel, s_char label);    // FF3 0x0021cfd8: 0 none
void   PlyrCameraEvCamEnd(void);                        // FF3 0x0021d190

// The player at a place (xyz, [3] facing in degrees); anime -1: none.
void PlyrCameraPlyrPlace(const float *pos, short anime);   // FF3 0x0021d1b0
void PlyrCameraPlyrToLabel(s_char npc, s_char kind, s_char label); // FF3 0x0021d280

#endif /* _INGAME_NPC_NPC_ACT_H */
