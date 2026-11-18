/* ==========================================================================
 *  ingame/photo/n_plyr_camera.h
 *
 *  The two companion NPCs (event NPC numbers 0 and 1; FF3 0x21e360..0x222170).
 *  The file keeps the name of FF2's link slot, but FF2's n_plyr_camera.c is
 *  unrelated: this is FF3's NPC controller. Each NPC has a work of 0x150
 *  bytes (bss 0x3f62c0, 0x150 apart) behind a 0x10-byte head (0x3f62b0);
 *  the three are save block 54. A work owns a CharNpcModel (vtable 0x341780)
 *  while the NPC is shown, an "act" block (the NPC's AI: idle motions,
 *  looking at the player, talking; its hooks are table-driven from the
 *  ingame/npc/npc_act.c), three facial channels, a position / rotation driven by
 *  the motion's root move, and the event settings (NPC_* commands).
 *  NPC 0 has two model types (3, 4) that link cat 0 (PlyrCameraIsCatMdl).
 *  The file ends with the act hooks (0x21ff38..0x222170): conditions,
 *  inits and steps the act scripts of ingame/npc/npc_act.c call.
 * ======================================================================== */
#ifndef _INGAME_PHOTO_N_PLYR_CAMERA_H
#define _INGAME_PHOTO_N_PLYR_CAMERA_H

#include <stddef.h>                     // offsetof
#include <libvu0.h>                     // sceVu0FVECTOR
#include "sdk/scetypes.h"

class GameModel;                        // the model object root (ingame/ingame.h)
class AcsModel;                         // ingame/plyr/acs_model.h
struct MAP_PUT;                         // ingame/map/MapPut.h
struct GRA3DEMULATIONLIGHTDATACREATIONDATA;    // graphics/graph3d/gra3dMisc.h

#define PLYR_CAMERA_NUM      2          // NPC 0 and 1
#define PLYR_CAMERA_LOCK_NUM 0x14       // NPC_LOCK_POS words

// The save block's head (data 0x3f62b0; PlyrCameraGetHead).
struct PLYR_CAMERA_HEAD                 /* 0x10 */
{
    /* 0x00 */ u_char serif_tbl;        // NPC_SERIF_TBL (PlyrCameraGetSerifTbl)
    /* 0x01 */ u_char facial_tbl;       // NPC_FACIAL_TBL
    /* 0x02 */ u_char _02;              // 0xff after a reset
    /* 0x03 */ u_char _03[0xd];
};
static_assert(sizeof(PLYR_CAMERA_HEAD) == 0x10, "PLYR_CAMERA_HEAD is 0x10 bytes");

// The NPC's act block (its AI), the first 0x48 bytes of a work: a byte-code
// script from NPC_OBJ (ingame/npc/npc_act.c: cleared by PlyrCameraActClear,
// set up by PlyrCameraActReset (NPC number, label 0xfe), stepped by
// PlyrCameraActStep while the NPC is free). The script addresses are EE
// addresses (the file sits at EE 0x12f7000; ReiPan_GetHostPointer reads it).
struct PLYR_CAMERA_ACT                  /* 0x48 */
{
    /* 0x00 */ u_int  base;             // the script file (NPC_OBJ_EE_ADRS); jump targets are relative to it
    /* 0x04 */ int    pc_rel;           // pc - base while saved (PlyrCameraActPcSave / PcLoad)
    /* 0x08 */ u_int  pc;               // the next script byte
    /* 0x0c */ u_int  _0c;              // pc is stored as 64 bits on the EE (this word stays 0)
    /* 0x10 */ u_int  op_pc;            // where the current op started
    /* 0x14 */ u_char op;               // the current op
    /* 0x15 */ u_char op_step;          // the op's own state; 0: the next op is fetched
    /* 0x16 */ u_char wait;             // frames to the next op: 0 stopped, 1 the next frame, 0xff the script ended
    /* 0x17 */ u_char cnt[2];           // ops 3 / 4 / 0x20 (the shipped scripts use none of them)
    /* 0x19 */ u_char flg[3];           // ops 5 / 0x21 (unused too)
    /* 0x1c */ u_short jump;            // op 2's target
    /* 0x1e */ s_char no;               // NPC number
    /* 0x1f */ s_char label;            // position label; < 0: none (0xfe after a reset)
    /* 0x20 */ int    anim;             // layout anim slot of the talk window, -1: none
    /* 0x24 */ s_char talk_mot;         // the motion the talk picked (PlyrCameraLabelMotPlay); 0x26 is NPC 0's long one
    /* 0x25 */ u_char set_kind;         // op 0x40's PlyrCameraActSetLabel kind
    /* 0x26 */ u_char step;             // the idle hook's state (0 start, 1 idle, 2..4 look, 5 chat, 6 talk)
    /* 0x27 */ s_char look_cnt;         // frames the look holds after the player left (60)
    /* 0x28 */ u_char idle_cnt;         // frames to the next idle motion (60 + rand 300, kept in a byte)
    /* 0x29 */ u_char idle_play;        // an idle motion is playing
    /* 0x2a */ s_char chat_cnt;         // the chat's frame count
    /* 0x2b */ s_char chat_fade;        // frames to the chat's effect-6 fade (-1: none)
    /* 0x2c */ s_char chat_step;
    /* 0x2d */ u_char chat_sel;         // the chat line set (PlyrCameraPickSerif)
    /* 0x2e */ u_char _2e[2];
    /* 0x30 */ int    mot_on;           // the talk hooks started a motion to wait for
    /* 0x34 */ int    _34;              // set when the NPC has nothing to say / is placed by NPC_FIX_EVENT
    /* 0x38 */ int    chat3_done;       // PlyrCameraChat3Check fired (once)
    /* 0x3c */ u_char _3c;              // the greet: 0 not yet, 1 in range, 2 done (PlyrCameraActIs3c2)
    /* 0x3d */ u_char _3d[3];
    /* 0x40 */ int    face_req;         // the line's face waits for its subtitle to start (PlyrCameraSerifIsEnd)
    /* 0x44 */ u_char _44[4];
};
static_assert(sizeof(PLYR_CAMERA_ACT) == 0x48, "PLYR_CAMERA_ACT is 0x48 bytes");
static_assert(offsetof(PLYR_CAMERA_ACT, pc) == 0x08, "PLYR_CAMERA_ACT.pc");
static_assert(offsetof(PLYR_CAMERA_ACT, op_pc) == 0x10, "PLYR_CAMERA_ACT.op_pc");
static_assert(offsetof(PLYR_CAMERA_ACT, op) == 0x14, "PLYR_CAMERA_ACT.op");
static_assert(offsetof(PLYR_CAMERA_ACT, wait) == 0x16, "PLYR_CAMERA_ACT.wait");
static_assert(offsetof(PLYR_CAMERA_ACT, cnt) == 0x17, "PLYR_CAMERA_ACT.cnt");
static_assert(offsetof(PLYR_CAMERA_ACT, flg) == 0x19, "PLYR_CAMERA_ACT.flg");
static_assert(offsetof(PLYR_CAMERA_ACT, jump) == 0x1c, "PLYR_CAMERA_ACT.jump");
static_assert(offsetof(PLYR_CAMERA_ACT, set_kind) == 0x25, "PLYR_CAMERA_ACT.set_kind");
static_assert(offsetof(PLYR_CAMERA_ACT, face_req) == 0x40, "PLYR_CAMERA_ACT.face_req");
static_assert(offsetof(PLYR_CAMERA_ACT, no) == 0x1e, "PLYR_CAMERA_ACT.no");
static_assert(offsetof(PLYR_CAMERA_ACT, anim) == 0x20, "PLYR_CAMERA_ACT.anim");
static_assert(offsetof(PLYR_CAMERA_ACT, step) == 0x26, "PLYR_CAMERA_ACT.step");
static_assert(offsetof(PLYR_CAMERA_ACT, chat_sel) == 0x2d, "PLYR_CAMERA_ACT.chat_sel");
static_assert(offsetof(PLYR_CAMERA_ACT, mot_on) == 0x30, "PLYR_CAMERA_ACT.mot_on");
static_assert(offsetof(PLYR_CAMERA_ACT, _34) == 0x34, "PLYR_CAMERA_ACT._34");
static_assert(offsetof(PLYR_CAMERA_ACT, chat3_done) == 0x38, "PLYR_CAMERA_ACT.chat3_done");
static_assert(offsetof(PLYR_CAMERA_ACT, _3c) == 0x3c, "PLYR_CAMERA_ACT._3c");

// The facial channels (work +0x48; the name is the earlier sessions').
// Each channel plays one face motion on the model's motion object: a
// request goes to req[], state 1 starts it, 2 stops the current one first,
// 0 restarts it while it plays (PlyrCameraFaceUpdate).
struct PLYR_CAMERA_COLOR                /* 0x14 */
{
    /* 0x00 */ int    on;               // the model has a facial set (its shape file is not -1)
    /* 0x04 */ int    blink;            // random faces on channel 2 (PlyrCameraBlinkUpdate)
    /* 0x08 */ s_char cur[3];           // face playing per channel, -1: none
    /* 0x0b */ s_char req[3];           // face requested, -1: none
    /* 0x0e */ s_char state[3];         // -1 idle, 0 replay, 1 start, 2 stop
    /* 0x11 */ u_char _11[3];
};
static_assert(sizeof(PLYR_CAMERA_COLOR) == 0x14, "PLYR_CAMERA_COLOR is 0x14 bytes");
static_assert(offsetof(PLYR_CAMERA_COLOR, cur) == 0x08, "PLYR_CAMERA_COLOR.cur");
static_assert(offsetof(PLYR_CAMERA_COLOR, req) == 0x0b, "PLYR_CAMERA_COLOR.req");
static_assert(offsetof(PLYR_CAMERA_COLOR, state) == 0x0e, "PLYR_CAMERA_COLOR.state");

// One NPC work (GetPlyrCameraWrk).
// layout: host -- mdl is an EE pointer (4 bytes at +0x5c); the offsets
// before it match the ROM (asserted), the rest are the EE offsets in the
// comments.
struct PLYR_CAMERA_WRK                  /* 0x150 on the EE */
{
    /* 0x000 */ PLYR_CAMERA_ACT   act;
    /* 0x048 */ PLYR_CAMERA_COLOR color;
    /* 0x05c */ GameModel        *mdl;      // a CharNpcModel (PlyrCameraMdlCreate), nullptr: none
    /* 0x060 */ AcsModel         *acs[4];   // accessories: [0..2] the NPC's records (PlyrCameraGetAcsDat), [3] the costume's
                                            //   (PlyrCameraAcsCreate); EE pointers, 4 bytes each
    /* 0x070 */ sceVu0FVECTOR     move;     // the motion's root move this frame
    /* 0x080 */ sceVu0FVECTOR     turn;     // [1]: the motion's turn (PlyrCameraMotTurnCB)
    /* 0x090 */ sceVu0FVECTOR     pos;
    /* 0x0a0 */ sceVu0FVECTOR     rot;      // [1]: facing
    /* 0x0b0 */ int               area;     // NPC_SET_POS's area, -1: none
    /* 0x0b4 */ s_char            mot;      // current motion, -1: none
    /* 0x0b5 */ u_char            _0b5[0xb];
    /* 0x0c0 */ sceVu0FVECTOR     eye_pos;  // the point the NPC looks at (eye_pos_on)
    /* 0x0d0 */ int               eye_pos_on;
    /* 0x0d4 */ int               eye_on;   // NPC_EYE_TARGET: watch on
    /* 0x0d8 */ int               eye_mode; // non-zero: only within the watch control's range (CharWatchControl::IsInRange)
    /* 0x0dc */ s_char            type;     // model type: 0 real side, 1 dream side, 2.. costumes; 3, 4: NPC 0 with the cat
    /* 0x0dd */ s_char            type_old;
    /* 0x0de */ u_char            _0de[2];
    /* 0x0e0 */ int               ready;    // the model is loaded and started
    /* 0x0e4 */ int               visible;  // its area is a drawn room (PlyrCameraVisibleCheck)
    /* 0x0e8 */ int               active;   // NPC_ACTIVE_FLG
    /* 0x0ec */ int               _0ec;     // PlyrCameraSetEc; set when NPC_FIX_EVENT releases an NPC without a label
    /* 0x0f0 */ s_char            free;     // the act block runs (0: fixed by NPC_FIX_EVENT, -1: released without a label)
    /* 0x0f1 */ s_char            fix_label;// the label NPC_FIX_EVENT keeps while fixed, -1: none
    /* 0x0f2 */ u_char            talk_flg; // PlyrCameraTalkFlgSet: 1 a talk / chat runs, 2 a line is shown
    /* 0x0f3 */ u_char            acs_flg[4];   // per accessory: bit 0 shown until the motion's trigger, bit 1 shown from it,
                                                //   bit 2 shown, bit 3 (slot 3) drawn after the body (PlyrCameraAcsMotSet)
    /* 0x0f7 */ u_char            _0f7;
    /* 0x0f8 */ int               lock_pos[PLYR_CAMERA_LOCK_NUM];   // NPC_LOCK_POS
    /* 0x148 */ u_char            _148[8];
};
static_assert(offsetof(PLYR_CAMERA_WRK, color) == 0x48, "PLYR_CAMERA_WRK.color");
static_assert(offsetof(PLYR_CAMERA_WRK, mdl) >= 0x5c, "PLYR_CAMERA_WRK.mdl follows the EE fields");
static_assert(offsetof(PLYR_CAMERA_WRK, pos) - offsetof(PLYR_CAMERA_WRK, move) == 0x20, "PLYR_CAMERA_WRK.pos (EE +0x90)");
static_assert(offsetof(PLYR_CAMERA_WRK, eye_pos) - offsetof(PLYR_CAMERA_WRK, move) == 0x50, "PLYR_CAMERA_WRK.eye_pos (EE +0xc0)");
static_assert(offsetof(PLYR_CAMERA_WRK, ready) - offsetof(PLYR_CAMERA_WRK, move) == 0x70, "PLYR_CAMERA_WRK.ready (EE +0xe0)");
static_assert(offsetof(PLYR_CAMERA_WRK, lock_pos) - offsetof(PLYR_CAMERA_WRK, move) == 0x88, "PLYR_CAMERA_WRK.lock_pos (EE +0xf8)");
static_assert(offsetof(PLYR_CAMERA_WRK, talk_flg) - offsetof(PLYR_CAMERA_WRK, move) == 0x82, "PLYR_CAMERA_WRK.talk_flg (EE +0xf2)");
static_assert(offsetof(PLYR_CAMERA_WRK, acs_flg) - offsetof(PLYR_CAMERA_WRK, move) == 0x83, "PLYR_CAMERA_WRK.acs_flg (EE +0xf3)");

// Save block 54 (data 0x3f62b0, 0x2b0 bytes on the EE).
// layout: host -- the works hold a pointer; the block is saved in the
// host layout (see PlyrCameraSaveGet).
struct PLYR_CAMERA_SAVE
{
    /* 0x000 */ PLYR_CAMERA_HEAD head;
    /* 0x010 */ PLYR_CAMERA_WRK  wrk[PLYR_CAMERA_NUM];
};
static_assert(offsetof(PLYR_CAMERA_SAVE, wrk) == 0x10, "PLYR_CAMERA_SAVE.wrk");

extern PLYR_CAMERA_SAVE plyr_camera;                    // data 0x3f62b0
extern int plyr_camera_se[PLYR_CAMERA_NUM];             // sbss 0x373738: a fading SE per NPC, -1: none
extern GRA3DEMULATIONLIGHTDATACREATIONDATA plyr_camera_light;   // data 0x35f6d0

PLYR_CAMERA_HEAD *PlyrCameraGetHead(void);              // FF3 0x0021e3b8
PLYR_CAMERA_WRK  *GetPlyrCameraWrk(s_char no);          // FF3 0x0021e420: no 0..1
void PlyrCameraColorReset(PLYR_CAMERA_COLOR *col);      // FF3 0x0021e5c0
GameModel *PlyrCameraGetMdl(s_char no);                 // FF3 0x0021e600: the model once ready
int  PlyrCameraSaveGet(void **addr);                    // FF3 0x0021e660: save block 54
void PlyrCameraMapObjDraw(MAP_PUT *put, u_int flags);   // FF3 0x0021e6a0: a map object lit as NPC 0 (MapDrawRoomObjRegist's draw callback of furniture 49)

void PlyrCameraInit(void);                              // FF3 0x0021f240
void PlyrCameraLoadRestore(void);                       // FF3 0x0021f268
void PlyrCameraReset(void);                             // FF3 0x0021f290
void PlyrCameraEnd(void);                               // FF3 0x0021f2b8
void PlyrCameraMain(void);                              // FF3 0x0021f2e0
// Creates NPC no's model of type (-1: the work's type) when the NPC is
// active and has none; arg resets the act block. 0 on failure.
int  PlyrCameraMdlCreate(s_char no, s_char type, int arg);     // FF3 0x0021eeb8
int  PlyrCameraMdlRelease(s_char no);                   // FF3 0x0021f0a0: GO_DREAM_WORLD (1, then 0)
int  PlyrCameraMdlChange(s_char no, s_char type, int arg);     // FF3 0x0021f170: NPC_MODEL_CHANGE

int  PlyrCameraIsDrawn(PLYR_CAMERA_WRK *cam, int no_cat);      // FF3 0x0021f2f8
void PlyrCameraDraw(PLYR_CAMERA_WRK *cam);              // FF3 0x0021f380: the alg-manage draw entry
int  PlyrCameraIsReady(s_char no);                      // FF3 0x0021f3d8
int  PlyrCameraIsActive(s_char no);                     // FF3 0x0021f400
void PlyrCameraSetActive(s_char no, int on);            // FF3 0x0021f428: NPC_ACTIVE_FLG
void PlyrCameraFixEvent(s_char no, int fix, s_char label);     // FF3 0x0021f458: NPC_FIX_EVENT
int  PlyrCameraIsFixed(s_char no);                      // FF3 0x0021f570: GO_REAL_WORLD; no < 0: any NPC
void PlyrCameraSetPos(s_char no, u_char area, float *pos, float rot);  // FF3 0x0021f5e8: NPC_SET_POS
void PlyrCameraMotPlay(s_char no, s_char mot, int arg); // FF3 0x0021f640
int  PlyrCameraMotIsLoop(s_char no);                    // FF3 0x0021f748
int  PlyrCameraMotIsEnd(s_char no);                     // FF3 0x0021f7f8: NPC_MOTION_CALL: the motion ended
int  PlyrCameraMotGetCtrl2(s_char no);                  // FF3 0x0021f858
int  PlyrCameraFaceReq(s_char no, s_char face, s_char ch);     // FF3 0x0021f890
void PlyrCameraFacialCall(s_char no, s_char face);      // FF3 0x0021f928: NPC_FACIAL_CALL
void PlyrCameraSetEyeTarget(s_char no, int on);         // FF3 0x0021fa10: NPC_EYE_TARGET
void PlyrCameraSetEyePos(s_char no, float *pos, int mode);     // FF3 0x0021fa78: NPC_EYE_TARGET's point (nullptr: none)
void PlyrCameraSetEc(s_char no);                        // FF3 0x0021fae8
void PlyrCameraSetEcSub(s_char no);                     // FF3 0x0021fb10
void PlyrCameraSetSerifTbl(u_char tbl);                 // FF3 0x0021fb30: NPC_SERIF_TBL
s_char PlyrCameraGetSerifTbl(s_char kind);              // FF3 0x0021fb60
void PlyrCameraSetFacialTbl(u_char tbl);                // FF3 0x0021fbd0: NPC_FACIAL_TBL
u_char PlyrCameraGetFacialTbl(void);                    // FF3 0x0021fc00
int  PlyrCameraGetJointPos(s_char no, int joint, float *pos, float *scr);  // FF3 0x0021fc20
int  PlyrCameraGetHeadPos(s_char no, float *pos, float *scr);  // FF3 0x0021fcf0: joint 2 (PlyrCameraGetJointPos), 0 without a model
GRA3DEMULATIONLIGHTDATACREATIONDATA *PlyrCameraGetLightWrk(void);  // FF3 0x0021fd18
void PlyrCameraSetLight(s_char no, int area, float *pos);      // FF3 0x0021fd28
int  PlyrCameraIsCatMdl(PLYR_CAMERA_WRK *cam);          // FF3 0x0021fee0: model type 3 or 4
void PlyrCameraSetLockPos(s_char no, s_char n, int v);  // FF3 0x0021fef0: NPC_LOCK_POS

// The act hooks (called through the act scripts of ingame/npc/npc_act.c).
int  PlyrCameraTalkCheck(s_char no);                    // FF3 0x0021ff38
int  PlyrCameraTalkEnable(PLYR_CAMERA_ACT *act);        // FF3 0x0021ffd0
void PlyrCameraTalkLock(void);                          // FF3 0x00220060
void PlyrCameraTalkUnlock(void);                        // FF3 0x002200a8
int  PlyrCameraActSetLabel(PLYR_CAMERA_ACT *act, s_char kind); // FF3 0x00220100
void PlyrCameraIdleInit(PLYR_CAMERA_ACT *act);          // FF3 0x00220510
void PlyrCameraIdleStep(PLYR_CAMERA_ACT *act);          // FF3 0x00220558
void PlyrCameraWatchInit(PLYR_CAMERA_ACT *act);         // FF3 0x002209e8
void PlyrCameraWatchStep(PLYR_CAMERA_ACT *act);         // FF3 0x00220a28
int  PlyrCameraIsWatching(PLYR_CAMERA_ACT *act, int near);     // FF3 0x00220b48
int  PlyrCameraGreetCheck(PLYR_CAMERA_ACT *act);        // FF3 0x00220bb0
int  PlyrCameraGreetCond(PLYR_CAMERA_ACT *act);         // FF3 0x00220bf8
void PlyrCameraGreetInit(PLYR_CAMERA_ACT *act);         // FF3 0x00220c48
int  PlyrCameraGreetStep(PLYR_CAMERA_ACT *act);         // FF3 0x00220ca0
int  PlyrCameraQueue0Ready(s_char no);                  // FF3 0x00221270
int  PlyrCameraReportCheck(PLYR_CAMERA_ACT *act);       // FF3 0x002212b0
void PlyrCameraReportInit(PLYR_CAMERA_ACT *act);        // FF3 0x00221308
int  PlyrCameraReportStep(PLYR_CAMERA_ACT *act);        // FF3 0x00221360
int  PlyrCameraQueue1Ready(s_char no);                  // FF3 0x00221740
int  PlyrCameraReport1Check(PLYR_CAMERA_ACT *act);      // FF3 0x00221780
void PlyrCameraReport1Init(PLYR_CAMERA_ACT *act);       // FF3 0x002217e8
int  PlyrCameraReport1Step(PLYR_CAMERA_ACT *act);       // FF3 0x00221820
int  PlyrCameraChat3Check(PLYR_CAMERA_ACT *act);        // FF3 0x00221a50
void PlyrCameraChat3Init(PLYR_CAMERA_ACT *act);         // FF3 0x00221ac8
int  PlyrCameraChat3Step(PLYR_CAMERA_ACT *act);         // FF3 0x00221b28
int  PlyrCameraChat4Check(PLYR_CAMERA_ACT *act);        // FF3 0x00221de8
void PlyrCameraChat4Init(PLYR_CAMERA_ACT *act);         // FF3 0x00221e58
int  PlyrCameraChat4Step(PLYR_CAMERA_ACT *act);         // FF3 0x00221e90
void PlyrCameraGreetMotInit(PLYR_CAMERA_ACT *act);      // FF3 0x00222080
int  PlyrCameraGreetMotStep(PLYR_CAMERA_ACT *act);      // FF3 0x00222088

#endif /* _INGAME_PHOTO_N_PLYR_CAMERA_H */
