/* ==========================================================================
 *  graphics/effect/effect_obj.h
 *
 *  Object effects (FF3 0x162318..0x1764f8 and the out-of-line statics at
 *  0x2b12a0..0x2b5998). FF2's effect_obj.c (parts deform, tourou, light,
 *  water, model alpha) plus, in FF3, much of FF2's effect_oth.c: the candle
 *  flames (kind 0x14), the many-candle altars (kind 0x1c), the halo (kind
 *  0x13), the thunder light, the seal door burn, the room glow (kind 0x10,
 *  a bloom), the four-part glow of kind 0x1e and the floating lanterns (map
 *  effect kind 600). EFFECT_CONT: graphics/effect/effect.h.
 * ======================================================================== */
#ifndef _GRAPHICS_EFFECT_EFFECT_OBJ_H
#define _GRAPHICS_EFFECT_EFFECT_OBJ_H

#include <stddef.h>                     // offsetof
#include "sdk/scetypes.h"
#include <libvu0.h>                     // sceVu0FVECTOR, sceVu0IVECTOR
#include "graphics/effect/effect.h"     // EFFECT_CONT (SEAL_DOOR_BURN_CTRL embeds two)

struct SINGLE_LINK_LIST;

// ---- the many-candle altars (kind 0x1c; FF2 effect_oth.c) -----------------

// One candle (FF2 MANY_CANDLE_PARTICLE).
struct MANY_CANDLE_PARTICLE             /* 0x20 */
{
    /* 0x00 */ sceVu0FVECTOR Position;  // pak data with y / z negated
    /* 0x10 */ int Alpha;               // 0 at init, not read
    /* 0x14 */ int FreaAlpha;           // the flare's alpha, random 17..21 each frame
    /* 0x18 */ int Count;               // 0..149: CandleFlameScaleData row
};
static_assert(sizeof(MANY_CANDLE_PARTICLE) == 0x20, "MANY_CANDLE_PARTICLE");
static_assert(offsetof(MANY_CANDLE_PARTICLE, FreaAlpha) == 0x14, "MANY_CANDLE_PARTICLE.FreaAlpha");
static_assert(offsetof(MANY_CANDLE_PARTICLE, Count) == 0x18, "MANY_CANDLE_PARTICLE.Count");

#define MANY_CANDLE_MAX 120

// One altar: EFFECT_MALLOC(0xf30) in SetManyCandle, held in pnt[1].
struct MANY_CANDLE_CTRL                 /* 0xf30 */
{
    /* 0x000 */ MANY_CANDLE_PARTICLE Particles[MANY_CANDLE_MAX];
    /* 0xf00 */ sceVu0FVECTOR CenterPos;
    /* 0xf10 */ int Id;
    /* 0xf14 */ int R;
    /* 0xf18 */ int G;
    /* 0xf1c */ int B;
    /* 0xf20 */ int DataNum;
};
static_assert(offsetof(MANY_CANDLE_CTRL, CenterPos) == 0xf00, "MANY_CANDLE_CTRL.CenterPos");
static_assert(offsetof(MANY_CANDLE_CTRL, Id) == 0xf10, "MANY_CANDLE_CTRL.Id");
static_assert(offsetof(MANY_CANDLE_CTRL, R) == 0xf14, "MANY_CANDLE_CTRL.R");
static_assert(offsetof(MANY_CANDLE_CTRL, DataNum) == 0xf20, "MANY_CANDLE_CTRL.DataNum");
static_assert(sizeof(MANY_CANDLE_CTRL) == 0xf30, "MANY_CANDLE_CTRL: EFFECT_MALLOC(0xf30)");

// The altar pack loader (FF2 MANY_CANDLE_LOAD_CTRL; data 0x3a6a40). Status 0
// idle, 1 loading, 2 effects running, 3 cut while loading.
// layout: host -- pLoadBuf and pEffRet are pointers (EE 0x10 and 0x20..0x34).
struct MANY_CANDLE_LOAD_CTRL            /* 0x40 on the EE */
{
    /* 0x00 */ sceVu0FVECTOR Offset;
    /* 0x10 */ void *pLoadBuf;          // a pk2 of altars
    /* 0x14 */ int   Status;
    /* 0x18 */ int   LoadId;
    /* 0x1c */ int   PackNum;
    /* 0x20 */ void *pEffRet[5];
};
static_assert(offsetof(MANY_CANDLE_LOAD_CTRL, pLoadBuf) == 0x10, "MANY_CANDLE_LOAD_CTRL.pLoadBuf");
static_assert(offsetof(MANY_CANDLE_LOAD_CTRL, Status) == 0x10 + sizeof(void *), "MANY_CANDLE_LOAD_CTRL.Status (EE 0x14)");
static_assert(offsetof(MANY_CANDLE_LOAD_CTRL, PackNum) == 0x18 + sizeof(void *), "MANY_CANDLE_LOAD_CTRL.PackNum (EE 0x1c)");

// ---- the thunder light (FF2 THUNDER_LIGHT_CTRL; data 0x3a6a80) ------------
struct THUNDER_LIGHT_CTRL               /* 0x34 */
{
    /* 0x00 */ sceVu0FVECTOR LightningDirection;    // w forced to 0
    /* 0x10 */ sceVu0FVECTOR ThunderPosition;       // the sound's position
    /* 0x20 */ int LightningStatus;     // 4 delay, 1 flash, 2 gap, 3 flash, 0 off
    /* 0x24 */ int LightningTime;
    /* 0x28 */ int LightningFlg;        // 1 while the room is lit
    /* 0x2c */ int ThunderStatus;       // 4 delay, 1 rumble, 2 gap, 3 rumble, 0 off
    /* 0x30 */ int ThunderTime;
};
static_assert(offsetof(THUNDER_LIGHT_CTRL, LightningStatus) == 0x20, "THUNDER_LIGHT_CTRL.LightningStatus");
static_assert(offsetof(THUNDER_LIGHT_CTRL, ThunderTime) == 0x30, "THUNDER_LIGHT_CTRL.ThunderTime");
static_assert(sizeof(THUNDER_LIGHT_CTRL) == 0x40, "THUNDER_LIGHT_CTRL (0x34 used, 16-aligned)");

// ---- the seal door burn (FF2 DOOR_SEAL_DISAPPEAR_CTRL; data 0x3a7750) -----
// A seal burns away: the screen darkens, the seal's sprite fades in, a parts
// deform (and its blur) ripples it under a borrowed camera with the screen
// dither, then the seal burns from its edge (an alpha-test threshold that
// rises) and everything fades out. FF3 reordered FF2's block (the two
// records first), loads its own texture pack (EffSealDoorBurnLoad) instead
// of FF2's preloaded stream and steps through a switch.
// Status: 0 idle, 1 loading, 2 seal fade-in (10 frames), 3 deform fade-in
// (30), 4 burn (to frame 129), 5 fade-out (10), then Release.
// layout: host -- the two EFFECT_CONTs hold pointers and buf is one; the
// fields after BasePos keep their EE order (EE offsets in the comments).
struct SEAL_DOOR_BURN_CTRL              /* 0x114 on the EE; EffSealDoorBurnInit clears 0x120 */
{
    /* 0x000 */ EFFECT_CONT   EffectCont;       // the deform (kind 0x16, deform 0x18)
    /* 0x070 */ EFFECT_CONT   EffectContBlur;   // its blur (deform 0x27), drawn first
    /* 0x0e0 */ sceVu0FVECTOR BasePos;          // both records' pnt[0]: the origin
    /* 0x0f0 */ int   Counter;
    /* 0x0f4 */ int   Status;
    /* 0x0f8 */ float AlphaRate;        // the darkening and the seal sprite, 0..1
    /* 0x0fc */ float BurnRate;         // status 4: the burn's progress (starts at 30 / 129, the ROM's)
    /* 0x100 */ float DeformSpeed;      // EffectCont.pnt[2]
    /* 0x104 */ float DeformRate;       // EffectCont.pnt[4]
    /* 0x108 */ float DeformAlphaRate;  // the deform, its blur and the dither, 0..1
    /* 0x10c */ void *pBuf;             // the burn texture pack (EFFECT_MALLOC)
    /* 0x110 */ int   LoadId;           // LoadReq id, -1 when none
};
static_assert(offsetof(SEAL_DOOR_BURN_CTRL, EffectContBlur) == sizeof(EFFECT_CONT), "SEAL_DOOR_BURN_CTRL.EffectContBlur (EE 0x70)");
static_assert(offsetof(SEAL_DOOR_BURN_CTRL, BasePos) == 2 * sizeof(EFFECT_CONT), "SEAL_DOOR_BURN_CTRL.BasePos (EE 0xe0)");
static_assert(offsetof(SEAL_DOOR_BURN_CTRL, Counter) == offsetof(SEAL_DOOR_BURN_CTRL, BasePos) + 0x10, "SEAL_DOOR_BURN_CTRL.Counter (EE 0xf0)");
static_assert(offsetof(SEAL_DOOR_BURN_CTRL, Status) == offsetof(SEAL_DOOR_BURN_CTRL, BasePos) + 0x14, "SEAL_DOOR_BURN_CTRL.Status (EE 0xf4)");
static_assert(offsetof(SEAL_DOOR_BURN_CTRL, BurnRate) == offsetof(SEAL_DOOR_BURN_CTRL, BasePos) + 0x1c, "SEAL_DOOR_BURN_CTRL.BurnRate (EE 0xfc)");
static_assert(offsetof(SEAL_DOOR_BURN_CTRL, DeformAlphaRate) == offsetof(SEAL_DOOR_BURN_CTRL, BasePos) + 0x28, "SEAL_DOOR_BURN_CTRL.DeformAlphaRate (EE 0x108)");
static_assert(offsetof(SEAL_DOOR_BURN_CTRL, pBuf) >= offsetof(SEAL_DOOR_BURN_CTRL, BasePos) + 0x2c, "SEAL_DOOR_BURN_CTRL.pBuf (EE 0x10c, pointer-aligned on the host)");

// ---- the room SCL buffers (data 0x2f9210; FF3-only) ------------------------
// One record per room with an SCL file (rs106 / 203 / 204 / 407 / 408 by
// the room -> file pairs at 0x2f9990), a sixth default one past them.
// layout: host -- pBuf is a pointer (EE 4 bytes at +0x130); the record stays
// 0x140 bytes.
#define EFF_OBJ_BUF_NUM 5
struct EFF_OBJ_BUF                      /* 0x140 */
{
    /* 0x000 */ int    Id;              // the room (EffObjBufTblFind's key); -1 in the default record
    /* 0x004 */ u_int  Prm[0x4b];       // particle parameters (floats and ints) effect_oth.c reads; not decoded yet
    /* 0x130 */ void  *pBuf;            // the room's SCL file (system heap), nullptr when not loaded
    u_char _pad[0x140 - 0x130 - sizeof(void *)];
};
static_assert(offsetof(EFF_OBJ_BUF, pBuf) == 0x130, "EFF_OBJ_BUF.pBuf");
static_assert(sizeof(EFF_OBJ_BUF) == 0x140, "EFF_OBJ_BUF: stride 0x140");

// ---- the parts deform rows of the play data screen (data 0x2f88a8) --------
struct EFF_PDEFORM_PRM                  /* 0x24 */
{
    /* 0x00 */ int   Type;              // SetEffects(0x16, 1, Type, ...): the deform kernel
    /* 0x04 */ int   Alpha;             // the record's max, scaled by the caller
    /* 0x08 */ float Prm[2];            // SetEffects' two doubles
    /* 0x10 */ float Fw[2];             // copied into the caller's record (+0x30 / +0x34)
    /* 0x18 */ int   Col[3];            // SetEffects' r / g / b (PhotoObjSlotEffStep)
};
static_assert(offsetof(EFF_PDEFORM_PRM, Fw) == 0x10, "EFF_PDEFORM_PRM.Fw");
static_assert(sizeof(EFF_PDEFORM_PRM) == 0x24, "EFF_PDEFORM_PRM: EffPDeformPrmGet stride");

// ---- the room glow (kind 0x10, slot 16; data 0x3a7870) --------------------
// A bloom: SetRoomGlow shrinks the frame into a work buffer, brightens it by
// the boost, subtracts the colour (the bright pass), blurs it three times
// and adds it back over the frame at the strength alpha.

// One colour block: the fixed one (EffRoomGlowSet) or a room's pair
// (RoomGlowDatGet: [0] normally, [1] while the camera is up).
struct ROOM_GLOW_COL                    /* 0x10 */
{
    /* 0x00 */ int r;                   // subtracted from the brightened copy
    /* 0x04 */ int g;
    /* 0x08 */ int b;
    /* 0x0c */ int alpha;               // the glow's strength (<= 0 draws nothing)
};
static_assert(sizeof(ROOM_GLOW_COL) == 0x10, "ROOM_GLOW_COL");

struct ROOM_GLOW_WRK                    /* 0x1c */
{
    /* 0x00 */ ROOM_GLOW_COL col;
    /* 0x10 */ u_char boost;            // EffRoomGlowGetBoost's answer while fixed (EffRoomGlowSet's fifth argument)
    /* 0x11 */ u_char _11[3];
    /* 0x14 */ int fixed;               // EffRoomGlowSetFixed: use this block, not the area's
    /* 0x18 */ int area;                // GetPlyrAreaNo of the last frame outside an event camera; -1 at init
};
static_assert(offsetof(ROOM_GLOW_WRK, boost) == 0x10, "ROOM_GLOW_WRK.boost");
static_assert(offsetof(ROOM_GLOW_WRK, fixed) == 0x14, "ROOM_GLOW_WRK.fixed");
static_assert(offsetof(ROOM_GLOW_WRK, area) == 0x18, "ROOM_GLOW_WRK.area");
static_assert(sizeof(ROOM_GLOW_WRK) == 0x1c, "ROOM_GLOW_WRK: EffRoomGlowInit clears 0x1c");

// ---- the floating lanterns (map effect kind 600; FF3-only) ----------------
// Rooms RS504 / RS506 / RS507 (areas 0x29 / 0x2b / 0x2c) float 30 / 30 / 40
// lanterns (LANTERN_MDL_PK4 with a candle flame) on a flow field: they
// appear at two spawn points, drift with the water, bump into each other, the
// walls and (area 0x2c) the player, and fade out at three sink points.
// C++ classes in the ROM (constructor / destructor pairs, GCC's two copies
// each); no RTTI, no virtuals.

// The flow map file RS50x_WFM as loaded (0x1c40): a 30 x 30 grid.
struct LANTERN_FLOW_DAT                 /* 0x1c40 */
{
    /* 0x0000 */ int   _00;
    /* 0x0004 */ int   _04;
    /* 0x0008 */ float x0;              // the grid's corner, room space
    /* 0x000c */ float z0;
    /* 0x0010 */ float cell_w;          // cell size in x / z (30 in the default map)
    /* 0x0014 */ float cell_d;
    /* 0x0018 */ int   nx;              // cells used in x / z (<= 30; the rows are 30 wide)
    /* 0x001c */ int   nz;
    /* 0x0020 */ float cell[30][30][2]; // [z][x]: the flow {x, z}
};
static_assert(offsetof(LANTERN_FLOW_DAT, cell_w) == 0x10, "LANTERN_FLOW_DAT.cell_w");
static_assert(offsetof(LANTERN_FLOW_DAT, nz) == 0x1c, "LANTERN_FLOW_DAT.nz");
static_assert(offsetof(LANTERN_FLOW_DAT, cell) == 0x20, "LANTERN_FLOW_DAT.cell");
static_assert(sizeof(LANTERN_FLOW_DAT) == 0x1c40, "LANTERN_FLOW_DAT: the size LanternFlowInit wants");

// The flow map object (operator new(0x40)).
// layout: host -- dat is a pointer (EE 4 bytes at +8); the vectors follow
// 16-aligned, EE offsets in the comments.
struct LANTERN_FLOW                     /* 0x40 on the EE */
{
    LANTERN_FLOW(int wfm, int wfp);       // FF3 0x00165ef0 (LanternFlowInit; twin 0x00165dd8)
    ~LANTERN_FLOW();                      // FF3 0x00166060 (LanternFlowRelease)
    int IsReady();                        // FF3 0x001660b8 (IsReady)
    float *Get(float *pos);               // FF3 0x00166100 (LanternFlowGet)

    /* 0x00 */ int   status;            // 0 loading, 1 ready (IsReady)
    /* 0x04 */ int   load_id;           // LoadReq id, -1 for the made-up map
    /* 0x08 */ LANTERN_FLOW_DAT *dat;   // system heap
    /* 0x10 */ sceVu0FVECTOR zero;      // Get's answer outside the grid
    /* 0x20 */ sceVu0FVECTOR flow;      // Get's answer inside: {x, 0, z, 0}
    /* 0x30 */ sceVu0FVECTOR offset;    // the room's origin (LANTERN_CTRL's constructor)
};
static_assert(offsetof(LANTERN_FLOW, zero) % 16 == 0 && offsetof(LANTERN_FLOW, offset) == offsetof(LANTERN_FLOW, zero) + 0x20,
              "LANTERN_FLOW vectors (EE 0x10 / 0x20 / 0x30)");

// The point file RS50x_LIP as loaded (0x560): the 40 lanterns' start rows,
// then two spawn and three sink circles.
struct LANTERN_LIP_ROW                  /* 0x20 */
{
    /* 0x00 */ float x;                 // position (y 16.4 added by LanternCtrlStart)
    /* 0x04 */ float z;
    /* 0x08 */ float vx;                // LANTERN.vel
    /* 0x0c */ float vz;
    /* 0x10 */ float dx;                // LANTERN.drift
    /* 0x14 */ float dz;
    /* 0x18 */ float ang;
    /* 0x1c */ float ang_vel;
};
static_assert(sizeof(LANTERN_LIP_ROW) == 0x20, "LANTERN_LIP_ROW");

struct LANTERN_LIP_PT                   /* 0x10 */
{
    /* 0x00 */ float x;
    /* 0x04 */ float z;
    /* 0x08 */ float r;                 // radius
    /* 0x0c */ float _0c;
};
static_assert(sizeof(LANTERN_LIP_PT) == 0x10, "LANTERN_LIP_PT");

#define LANTERN_MAX 40

struct LANTERN_LIP_DAT                  /* 0x560 */
{
    /* 0x000 */ int _00;
    /* 0x004 */ int num;                // 40 in the default (not read)
    /* 0x008 */ int in_num;             // 2 (not read: LanternLipIsReady makes 2)
    /* 0x00c */ int out_num;            // 3 (not read)
    /* 0x010 */ LANTERN_LIP_ROW row[LANTERN_MAX];
    /* 0x510 */ LANTERN_LIP_PT  in[2];
    /* 0x530 */ LANTERN_LIP_PT  out[3];
};
static_assert(offsetof(LANTERN_LIP_DAT, row) == 0x10, "LANTERN_LIP_DAT.row");
static_assert(offsetof(LANTERN_LIP_DAT, in) == 0x510, "LANTERN_LIP_DAT.in");
static_assert(offsetof(LANTERN_LIP_DAT, out) == 0x530, "LANTERN_LIP_DAT.out");
static_assert(sizeof(LANTERN_LIP_DAT) == 0x560, "LANTERN_LIP_DAT: the size LanternLipInit wants");

// A spawn or sink circle (operator new(0x30), LanternPointInit).
struct LANTERN_POINT                    /* 0x30 */
{
    LANTERN_POINT(float x, float y, float z, float r);  // FF3 0x001662e8 (LanternPointInit)
    ~LANTERN_POINT();                                   // FF3 0x00166328 (LanternPointRelease)
    float *GetRandPos();                                // FF3 0x00166330 (LanternPointGetRandPos)
    int IsIn(float *pos);                               // FF3 0x001663f0 (LanternPointIsIn)

    /* 0x00 */ sceVu0FVECTOR pos;       // (x, 16.4, z, 0) + the room offset (xyz)
    /* 0x10 */ sceVu0FVECTOR rand_pos;  // LanternPointGetRandPos's answer
    /* 0x20 */ float r;
    /* 0x24 */ float r2;                // r * r
};
static_assert(offsetof(LANTERN_POINT, r) == 0x20, "LANTERN_POINT.r");
static_assert(offsetof(LANTERN_POINT, r2) == 0x24, "LANTERN_POINT.r2");
static_assert(sizeof(LANTERN_POINT) == 0x30, "LANTERN_POINT: operator new(0x30)");

// The point object (operator new(0x40)).
// layout: host -- dat and the point pointers are pointers (EE 4 bytes at +8,
// +0x24..+0x34); EE offsets in the comments.
struct LANTERN_LIP                      /* 0x40 on the EE */
{
    LANTERN_LIP(int file);                // FF3 0x00166538 (LanternLipInit; twin 0x00166438)
    ~LANTERN_LIP();                       // FF3 0x00166718 (LanternLipRelease)
    int IsReady();                        // FF3 0x001667f8 (LanternLipIsReady)
    float *GetSpawnPos();                 // FF3 0x00166938 (LanternLipGetSpawnPos)
    int IsInSink(float *pos);             // FF3 0x001669a8 (LanternLipIsInSink)

    /* 0x00 */ int   status;            // 0 loading, 1 points made (LanternLipIsReady)
    /* 0x04 */ int   load_id;
    /* 0x08 */ LANTERN_LIP_DAT *dat;
    /* 0x10 */ sceVu0FVECTOR offset;    // the room's origin (LANTERN_CTRL's constructor)
    /* 0x20 */ int   spawn;             // the spawn point used last (alternates)
    /* 0x24 */ LANTERN_POINT *in[2];
    /* 0x2c */ LANTERN_POINT *out[3];
};
static_assert(offsetof(LANTERN_LIP, offset) % 16 == 0, "LANTERN_LIP.offset (EE 0x10)");
static_assert(offsetof(LANTERN_LIP, spawn) == offsetof(LANTERN_LIP, offset) + 0x10, "LANTERN_LIP.spawn (EE 0x20)");

class GameModel;                        // graphics/game_model.h

// One lantern (operator new(0xd0), LanternInit).
// layout: host -- mdl is a pointer (EE 4 bytes at +0xbc); every field before
// it matches the ROM, the rest follow in EE order.
struct LANTERN                          /* 0xd0 on the EE */
{
    LANTERN();                                             // FF3 0x00166ac8 (LanternInit; twin 0x00166a30)
    ~LANTERN();                                            // FF3 0x00166b90 (LanternRelease)
    void ClearMove();                                      // FF3 0x00166bc0 (LanternClearMove)
    void ClearState();                                     // FF3 0x00166c10 (LanternClearState)
    void SetFlameCnt();                                    // FF3 0x00166c20 (LanternSetFlameCnt)
    void Step();                                           // FF3 0x00166c50 (LanternStep)
    int IsVisible();                                       // FF3 0x00166ca0 (LanternIsVisible)
    void Draw();                                           // FF3 0x00166d18 (LanternDraw)
    int PushByPlayer(float r, float *ppos, float *pmove);  // FF3 0x00166f90 (LanternPushByPlayer)
    int Collide(LANTERN *b);                               // FF3 0x00167128 (LanternCollide)
    void Move();                                           // FF3 0x002b2d70 (LanternMove)
    void Fade();                                           // FF3 0x002b3110 (Fade)

    /* 0x00 */ int   state;             // 0 fading in, 1 floating, 2 fading out (Fade)
    /* 0x04 */ int   _04[3];
    /* 0x10 */ sceVu0FMATRIX mat;       // RotY(ang) * 1.2 at pos (the model's matrix; MapPutSetMatrixPtr)
    /* 0x50 */ sceVu0FVECTOR pos;
    /* 0x60 */ sceVu0FVECTOR pos_old;   // pos after the own move, before the pushes (the wall test's way back)
    /* 0x70 */ sceVu0IVECTOR scr;       // sceVu0RotTransPers of pos: x, y, z (fixed 4), w * 16
    /* 0x80 */ sceVu0FVECTOR vel;       // y, w kept 0
    /* 0x90 */ sceVu0FVECTOR drift;     // the wander direction, y, w kept 0
    /* 0xa0 */ sceVu0FVECTOR flow;      // LanternFlowGet at pos this frame
    /* 0xb0 */ float ang;               // yaw, kept in -pi..pi
    /* 0xb4 */ float ang_vel;
    /* 0xb8 */ float far_rate;          // 0 near .. 1 far (w * 16 of 200..10000)
    /* 0xbc */ GameModel *mdl;          // LANTERN_MDL_PK4
    /* 0xc0 */ int   mdl_ready;         // the model answered loaded once
    /* 0xc4 */ short alpha;             // 0..0x80
    /* 0xc6 */ short wander;            // frames to the next random turn of drift
    /* 0xc8 */ short flame_cnt;         // 0..149: the flame's flicker row
};
static_assert(offsetof(LANTERN, mat) == 0x10, "LANTERN.mat");
static_assert(offsetof(LANTERN, pos) == 0x50, "LANTERN.pos");
static_assert(offsetof(LANTERN, scr) == 0x70, "LANTERN.scr");
static_assert(offsetof(LANTERN, flow) == 0xa0, "LANTERN.flow");
static_assert(offsetof(LANTERN, ang_vel) == 0xb4, "LANTERN.ang_vel");
static_assert(offsetof(LANTERN, far_rate) == 0xb8, "LANTERN.far_rate");
static_assert(offsetof(LANTERN, mdl) >= 0xbc, "LANTERN.mdl (EE 0xbc, pointer-aligned on the host)");
static_assert(offsetof(LANTERN, wander) == offsetof(LANTERN, alpha) + 2, "LANTERN.wander (EE 0xc6)");
static_assert(offsetof(LANTERN, flame_cnt) == offsetof(LANTERN, alpha) + 4, "LANTERN.flame_cnt (EE 0xc8)");

// One room's lanterns (operator new(0x120), LanternCtrlInit).
// layout: host -- the lantern, flow and point pointers are pointers (EE 4
// bytes at +0x24.. / +0x114 / +0x118); EE +0xc4..+0x113 is never touched and
// left out.
struct LANTERN_CTRL                     /* 0x120 on the EE */
{
    LANTERN_CTRL(int area, int wfm, int lip_file, int wfp, int push, int num);  // FF3 0x00167478 (LanternCtrlInit; twin 0x001672c0)
    ~LANTERN_CTRL();                                                    // FF3 0x001676d8 (LanternCtrlRelease; twin 0x00167630)
    void Respawn(LANTERN *l);                                           // FF3 0x00167780 (LanternRespawn)
    void Start();                                                       // FF3 0x00167890 (LanternCtrlStart)
    void Exec();                                                        // FF3 0x00167a38 (LanternCtrlExec)
    void Step();                                                        // FF3 0x00167ac8 (LanternCtrlStep)

    /* 0x000 */ int   state;            // 0 waiting for the files, 1 running
    /* 0x004 */ int   push;             // 1: the player pushes the lanterns (area 0x2c)
    /* 0x008 */ int   _08[2];
    /* 0x010 */ sceVu0FVECTOR offset;   // MapLoadGetOffsetVector of the area's buffer
    /* 0x020 */ int   num;              // lanterns made (30 / 40)
    /* 0x024 */ LANTERN      *lantern[LANTERN_MAX];
    /* 0x114 */ LANTERN_FLOW *flow;
    /* 0x118 */ LANTERN_LIP  *lip;
    /* 0x11c */ int   put_buff;         // area + 4000: the MapPut buffer of the lanterns' draw callbacks
};
static_assert(offsetof(LANTERN_CTRL, offset) == 0x10, "LANTERN_CTRL.offset");
static_assert(offsetof(LANTERN_CTRL, num) == 0x20, "LANTERN_CTRL.num");
static_assert(offsetof(LANTERN_CTRL, lantern) >= 0x24, "LANTERN_CTRL.lantern (EE 0x24, pointer-aligned on the host)");

// The two room slots (bss 0x3a7900): EffLanternReq / EffLanternCut.
// layout: host -- ctrl is a pointer (EE 4 bytes at +0).
struct LANTERN_SLOT                     /* 0xc on the EE */
{
    /* 0x00 */ LANTERN_CTRL *ctrl;
    /* 0x04 */ int area;
    /* 0x08 */ int no;                  // the map effect's number (EffLanternCut)
};

// ---- kind 0x1e: four glows circling a point (table data 0x2f8778) ---------
// Each part moves on sine curves of t = cnt * 2pi / period - pi.
struct EFF_KIND1E_PART                  /* 0x4c */
{
    /* 0x00 */ int   on;
    /* 0x04 */ float pos[3];            // centre of the circle, part space
    /* 0x10 */ int   rgb[3];
    /* 0x1c */ float alpha_base;        // alpha = (base + amp * sin(t + phase)) * rate
    /* 0x20 */ float alpha_amp;
    /* 0x24 */ float alpha_phase;
    /* 0x28 */ float rad_base;          // radius of the circle
    /* 0x2c */ float rad_amp;
    /* 0x30 */ float rad_phase;
    /* 0x34 */ float ang_phase;         // angle = ang_scale * (t + ang_phase)
    /* 0x38 */ float ang_scale;
    /* 0x3c */ float size_base;         // sprite size / 10
    /* 0x40 */ float size_amp;
    /* 0x44 */ float size_phase;
    /* 0x48 */ float period;            // frames
};
static_assert(offsetof(EFF_KIND1E_PART, rgb) == 0x10, "EFF_KIND1E_PART.rgb");
static_assert(offsetof(EFF_KIND1E_PART, alpha_phase) == 0x24, "EFF_KIND1E_PART.alpha_phase");
static_assert(offsetof(EFF_KIND1E_PART, ang_scale) == 0x38, "EFF_KIND1E_PART.ang_scale");
static_assert(offsetof(EFF_KIND1E_PART, period) == 0x48, "EFF_KIND1E_PART.period");
static_assert(sizeof(EFF_KIND1E_PART) == 0x4c, "EFF_KIND1E_PART: EffKind1EGetPart stride");

// ---- parts deform (kinds 0x16 / 0x1f; FF2 effect_obj.c) -------------------
// A 17 x 17 vertex grid in front of a point, each vertex displaced by a wave
// kernel and drawn as a textured strip sampling a copy of the frame (the
// heat-haze / refraction look). FF3 keeps FF2's machinery: 10 work slots
// (FF2 8) and the 5 shared phase tables of pass 2.

// One vertex: r / add phase and per-frame step, h amplitude, x / y the
// displacement, ox / oy last frame's (the kernels' zero-crossing test), lng
// the distance from the grid's centre (EffInf02Init case 2).
struct EFFPOS                           /* 0x28 */
{
    /* 0x00 */ float r;
    /* 0x04 */ float add;
    /* 0x08 */ float h;
    /* 0x0c */ float x;
    /* 0x10 */ float y;
    /* 0x14 */ float z;
    /* 0x18 */ float ox;
    /* 0x1c */ float oy;
    /* 0x20 */ float oz;
    /* 0x24 */ float lng;
};
static_assert(sizeof(EFFPOS) == 0x28, "EFFPOS");
static_assert(offsetof(EFFPOS, ox) == 0x18, "EFFPOS.ox");
static_assert(offsetof(EFFPOS, lng) == 0x24, "EFFPOS.lng");

#define PDEFORM_VERTEX_W    17
#define PDEFORM_VERTEX_H    17
#define PDEFORM_VERTEX_MAX  (PDEFORM_VERTEX_W * PDEFORM_VERTEX_H)  /* 289 */
#define PDEFORM_SLOT_NUM    10          // FF2 8

// One deform work slot (data 0x3a7920, 10 of them; pass 2's shared tables at
// bss 0x3c3d50, 5 of them).
struct EFFINFO2                         /* 0x2d30 */
{
    /* 0x0000 */ EFFPOS ep[PDEFORM_VERTEX_MAX];
    /* 0x2d28 */ short  use;            // claimed (GetFreePartsDeformCtrlNo, inlined in FF3)
    /* 0x2d2a */ short  pass;           // drawn this frame; EffectObjInitEF frees a used slot without it
    /* 0x2d2c */ float  r;              // the grid-wide phase (WaveY2 / WaveType2)
};
static_assert(offsetof(EFFINFO2, use) == 0x2d28, "EFFINFO2.use");
static_assert(offsetof(EFFINFO2, pass) == 0x2d2a, "EFFINFO2.pass");
static_assert(offsetof(EFFINFO2, r) == 0x2d2c, "EFFINFO2.r");
static_assert(sizeof(EFFINFO2) == 0x2d30, "EFFINFO2: slot stride");

// The two random ranges of EffInf02Init case 3 and WaveY2 (rdata 0x353790:
// 1, -0.3, 0.4, 0.2).
struct PDEFORM_PARA                     /* 0x10 */
{
    /* 0x00 */ float pr11;
    /* 0x04 */ float pr12;
    /* 0x08 */ float pr21;
    /* 0x0c */ float pr22;
};
static_assert(sizeof(PDEFORM_PARA) == 0x10, "PDEFORM_PARA");

// Project one grid vertex: screen x / y in 12.4 fixed, z integer, w the
// pre-divide w as float bits; returns the vertex's six clip flags (the CLIP
// register after one vclip). FF3 takes the two local matrices from VU0
// registers vf4..vf11 that the caller loaded; on the host they are arguments
// (MioPan's deviation): matLocalScreen = matWorldScreen * local,
// matLocalClip = matWorldClipPolygon * local.
int CalcPartsDeformXYZ(int *vi, const float *vf,
                       sceVu0FMATRIX matLocalScreen, sceVu0FMATRIX matLocalClip,
                       float *pNdcZ = nullptr);  // FF3 0x001687b8 (also EneDmgLargeHitMakePacket); PC: pNdcZ

// ---- the per-model lists (FF2 effect_obj.c; one SINGLE_LINK_LIST each) ---
// Records are byte copies inside the list cells. Only the FF3 sizes and the
// Id fields the deletes read are confirmed; the rest is FF2's order.
// The deletes use the field names, not the offsets.

// layout: host -- pSgdTop is a pointer (EE 4 bytes at +0).
struct LIGHT_COME_IN_DATA               /* 0x18 on the EE (list 0x3c3d20) */
{
    /* 0x00 */ void *pSgdTop;
    /* 0x04 */ float TotalS;
    /* 0x08 */ float TotalT;
    /* 0x0c */ int   MapBuffId;         // EffectLightComeInDeleteMapBuffId's key
    /* 0x10 */ int   AlphaMax;
    /* 0x14 */ int   Type;
};

// layout: host -- pSgdTop is a pointer (EE 4 bytes at +0).
struct WATER_FLOW_DATA                  /* 0x20 on the EE (list 0x3c3d30; FF2 0x1c) */
{
    /* 0x00 */ void *pSgdTop;
    /* 0x04 */ float TotalS;
    /* 0x08 */ float TotalT;
    /* 0x0c */ int   Id;                // EffectWaterFlowDelete's key
    /* 0x10 */ int   MoveType;
    /* 0x14 */ int   Count;
    /* 0x18 */ int   MoveDataNo;
    /* 0x1c */ float AlphaPhase;        // FF3: EffectWaterFlowGetAlpha's sine phase (-pi..pi)
};

// One water flow move type (data 0x2f8918, 12 of them; FF3 replaced FF2's
// per-type switches with this table). The speeds are + 10000, in 1 / 10000
// texture widths a frame.
struct WATER_FLOW_MOVE_DAT              /* 0x34 */
{
    /* 0x00 */ int AddST[3][2];         // (S, T) of legs A, B, C
    /* 0x18 */ int AlphaMin;
    /* 0x1c */ int AlphaMax;
    /* 0x20 */ int AlphaPeriod;         // frames of a half sine; < 1: AlphaMin, still
    /* 0x24 */ int Kind;                // 1 or 2: ALPHA_1 0x40 | Kind << 2 in IgEffectWaterFlowModelDraw (0x44 blend, 0x48 add)
    /* 0x28 */ int Frame[3];            // hold lengths of legs A, B, C (the transitions are 60)
};
static_assert(sizeof(WATER_FLOW_MOVE_DAT) == 0x34, "WATER_FLOW_MOVE_DAT");
static_assert(offsetof(WATER_FLOW_MOVE_DAT, AlphaMin) == 0x18, "WATER_FLOW_MOVE_DAT.AlphaMin");
static_assert(offsetof(WATER_FLOW_MOVE_DAT, Frame) == 0x28, "WATER_FLOW_MOVE_DAT.Frame");

// ---- kind 0x1d: the particle clouds (FF3-only) ---------------------------
// One parameter record (data 0x2f8bd8, nine of them, EffKind1DGetParam).
// Lengths are in units, speeds in units a frame; "+1000" values are biased
// by 1000 and "/2500" ones divided by 2500 after the bias comes off.
struct KIND1D_PARAM                     /* 0xb0 */
{
    /* 0x00 */ int Radius;              // spawn disc radius
    /* 0x04 */ int YMax;                // spawn height range, +1000
    /* 0x08 */ int YMin;
    /* 0x0c */ int Rate;                // particles a frame x 100 (the fraction by chance)
    /* 0x10 */ int Vel[3][2];           // x / y / z velocity (max, min), +1000 /2500
    /* 0x28 */ int Wind[3][3];          // the three wind vectors (x, y, z), +1000 /2500
    /* 0x4c */ int WindTime[3];         // how long each wind holds (the moves take 60)
    /* 0x58 */ int Alpha;               // envelope top
    /* 0x5c */ int In;                  // envelope frames: fade in, keep, fade out
    /* 0x60 */ int Keep;
    /* 0x64 */ int Out;
    /* 0x68 */ int Scale[2];            // start / end scale x 100
    /* 0x70 */ int Rgb[3];
    /* 0x7c */ int SpinMax;             // spin range in degrees, spread over SpinDiv frames
    /* 0x80 */ int SpinMin;
    /* 0x84 */ int SpinDiv;
    /* 0x88 */ int AlphaA;              // GS ALPHA_1 fields
    /* 0x8c */ int AlphaB;
    /* 0x90 */ int AlphaC;
    /* 0x94 */ int AlphaD;
    /* 0x98 */ int AlphaFix;
    /* 0x9c */ int Offset[3];           // emitter offset, +10000, turned by the owner's rot[1]
    /* 0xa8 */ int TrailLen;            // records of the position trail (0: none)
    /* 0xac */ int TrailLag;            // particle age from which it lags behind the owner
};
static_assert(sizeof(KIND1D_PARAM) == 0xb0, "KIND1D_PARAM");
static_assert(offsetof(KIND1D_PARAM, Wind) == 0x28, "KIND1D_PARAM.Wind");
static_assert(offsetof(KIND1D_PARAM, WindTime) == 0x4c, "KIND1D_PARAM.WindTime");
static_assert(offsetof(KIND1D_PARAM, Alpha) == 0x58, "KIND1D_PARAM.Alpha");
static_assert(offsetof(KIND1D_PARAM, Rgb) == 0x70, "KIND1D_PARAM.Rgb");
static_assert(offsetof(KIND1D_PARAM, AlphaA) == 0x88, "KIND1D_PARAM.AlphaA");
static_assert(offsetof(KIND1D_PARAM, Offset) == 0x9c, "KIND1D_PARAM.Offset");
static_assert(offsetof(KIND1D_PARAM, TrailLag) == 0xac, "KIND1D_PARAM.TrailLag");

struct KIND1D_PARTICLE                  /* 0x50 */
{
    /* 0x00 */ sceVu0FVECTOR Pos;
    /* 0x10 */ sceVu0FVECTOR Vel;
    /* 0x20 */ int   Life;              // frames left; 0: free slot
    /* 0x24 */ int   Alpha;
    /* 0x28 */ float Scale;
    /* 0x2c */ float Rot;               // z turn, -pi..pi
    /* 0x30 */ float Spin;              // Rot's step
    /* 0x34 */ int   VtxAlpha[4];       // the corner alphas of this frame's quad
    /* 0x44 */ u_char _44[0xc];
};
static_assert(sizeof(KIND1D_PARTICLE) == 0x50, "KIND1D_PARTICLE");
static_assert(offsetof(KIND1D_PARTICLE, Life) == 0x20, "KIND1D_PARTICLE.Life");
static_assert(offsetof(KIND1D_PARTICLE, Spin) == 0x30, "KIND1D_PARTICLE.Spin");
static_assert(offsetof(KIND1D_PARTICLE, VtxAlpha) == 0x34, "KIND1D_PARTICLE.VtxAlpha");

#define KIND1D_PARTICLE_NUM 0x80

// A kind 0x1d record's work (EFFECT_MALLOC 0x2820 on the EE, ec->pnt[1]).
// layout: host -- pPos / pRot / pAlpha are pointers (EE 4 bytes each at
// 0x2800 / 0x2804 / 0x2808); the later fields follow in EE order.
struct KIND1D_WRK
{
    /* 0x0000 */ KIND1D_PARTICLE Part[KIND1D_PARTICLE_NUM];
    /* 0x2800 */ float *pPos;           // the owner's position (ec->pnt[0])
    /* 0x2804 */ float *pRot;           // the owner's rotation (ec->pnt[2]), may be nullptr
    /* 0x2808 */ float *pAlpha;         // rate scale (ec->pnt[3]), may be nullptr
    /* 0x280c */ int    WindCnt;
    /* 0x2810 */ int    WindPhase;      // 0..5: wind 0, 0->1, 1, 1->2, 2, 2->0
    /* 0x2814 */ int    Id;             // SetEffects' v (ec->dat.i32[1]); EffKind1DCutId's key
    /* 0x2818 */ short  Num;            // live particles
    /* 0x281a */ short  No;             // parameter record
};
static_assert(offsetof(KIND1D_WRK, pPos) == 0x2800, "KIND1D_WRK.pPos");
static_assert(offsetof(KIND1D_WRK, WindCnt) == 0x2800 + 3 * sizeof(float *), "KIND1D_WRK.WindCnt (EE 0x280c)");
static_assert(offsetof(KIND1D_WRK, Num) == offsetof(KIND1D_WRK, WindCnt) + 0x0c, "KIND1D_WRK.Num (EE 0x2818)");
static_assert(offsetof(KIND1D_WRK, No) == offsetof(KIND1D_WRK, WindCnt) + 0x0e, "KIND1D_WRK.No (EE 0x281a)");

// ---- area particles (bss 0x3d45f0 / 0x3d4650; FF3-only, never requested) ---

// A quad the area particles start in: a random point between the edges
// V[0]..V[1] and V[3]..V[2]; Weight: the chance of the area in 1 / 10000.
struct EFF_EMIT_AREA                    /* 0x50 */
{
    /* 0x00 */ sceVu0FVECTOR V[4];
    /* 0x40 */ int    Weight;
    /* 0x44 */ int    _44[3];
};
static_assert(sizeof(EFF_EMIT_AREA) == 0x50, "EFF_EMIT_AREA");
static_assert(offsetof(EFF_EMIT_AREA, Weight) == 0x40, "EFF_EMIT_AREA.Weight");

// One particle: a camera-facing sprite moving at Vel, growing from
// SizeStart to SizeEnd and fading out over the record's Life frames.
struct EFF_AREA_PART                    /* 0x30 */
{
    /* 0x00 */ sceVu0FVECTOR Pos;
    /* 0x10 */ sceVu0FVECTOR Vel;
    /* 0x20 */ int    Life;             // frames left; -1: free
    /* 0x24 */ float  SizeStart;
    /* 0x28 */ float  SizeEnd;
    /* 0x2c */ int    _2c;
};
static_assert(sizeof(EFF_AREA_PART) == 0x30, "EFF_AREA_PART");
static_assert(offsetof(EFF_AREA_PART, Life) == 0x20, "EFF_AREA_PART.Life");
static_assert(offsetof(EFF_AREA_PART, SizeEnd) == 0x28, "EFF_AREA_PART.SizeEnd");

// One emitter (EffAreaPartInit fills the constants, EffAreaPartSet the
// position, areas and buffer). Each frame EffAreaPartEmit starts
// EmitMin .. EmitMax particles in the areas, offset by Pos.
// layout: host -- pArea and pPart are pointers (EE 4 bytes at +0x10 / +0x5c).
struct EFF_AREA_PART_WRK                /* 0x60 */
{
    /* 0x00 */ sceVu0FVECTOR Pos;       // w cleared by EffAreaPartSet
    /* 0x10 */ const EFF_EMIT_AREA *pArea;
    /* 0x14 */ int    AreaNum;
    /* 0x18 */ int    Num;              // particles in pPart
    /* 0x1c */ int    Col[3];           // r, g, b
    /* 0x28 */ int    Alpha;            // at Life; fades to 0
    /* 0x2c */ float  VelXMax;          // start velocity ranges
    /* 0x30 */ float  VelXMin;
    /* 0x34 */ float  VelYMax;
    /* 0x38 */ float  VelYMin;
    /* 0x3c */ float  VelZMax;
    /* 0x40 */ float  VelZMin;
    /* 0x44 */ int    EmitMin;          // new particles per frame
    /* 0x48 */ int    EmitMax;
    /* 0x4c */ int    Life;
    /* 0x50 */ float  SizeStart;
    /* 0x54 */ float  SizeEndMax;
    /* 0x58 */ float  SizeEndMin;
    /* 0x5c */ EFF_AREA_PART *pPart;    // EFFECT_MALLOC'd
};
static_assert(offsetof(EFF_AREA_PART_WRK, pArea) == 0x10, "EFF_AREA_PART_WRK.pArea");
static_assert(offsetof(EFF_AREA_PART_WRK, AreaNum) == 0x10 + sizeof(void *), "EFF_AREA_PART_WRK.AreaNum (EE 0x14)");
static_assert(offsetof(EFF_AREA_PART_WRK, Alpha) == 0x10 + sizeof(void *) + 0x14, "EFF_AREA_PART_WRK.Alpha (EE 0x28)");
static_assert(offsetof(EFF_AREA_PART_WRK, SizeEndMin) == 0x10 + sizeof(void *) + 0x44, "EFF_AREA_PART_WRK.SizeEndMin (EE 0x58)");

// ---- drops (bss 0x3d46b0; FF3-only, never requested) -----------------------

// One drop, a list element (byte-copied into the list): falls from rest,
// Gravity added to Vel y each frame, removed once it reaches FloorY.
struct EFF_DROP                         /* 0x30 (list element size) */
{
    /* 0x00 */ sceVu0FVECTOR Pos;
    /* 0x10 */ sceVu0FVECTOR Vel;
    /* 0x20 */ float  Gravity;
    /* 0x24 */ float  FloorY;
    /* 0x28 */ int    _28[2];
};
static_assert(sizeof(EFF_DROP) == 0x30, "EFF_DROP");
static_assert(offsetof(EFF_DROP, Gravity) == 0x20, "EFF_DROP.Gravity");
static_assert(offsetof(EFF_DROP, FloorY) == 0x24, "EFF_DROP.FloorY");

// The drop source: EffDropSet fills it, EffDropInit clears it (0x40 bytes).
// layout: host -- pList and pTbl are pointers (EE 4 bytes at +0x10 / +0x14).
struct EFF_DROP_WRK
{
    /* 0x00 */ sceVu0FVECTOR Pos;       // w cleared by EffDropSet
    /* 0x10 */ SINGLE_LINK_LIST *pList; // of EFF_DROP
    /* 0x14 */ const sceVu0FVECTOR *pTbl;   // the start points (EffDropEmit reads the table directly)
    /* 0x18 */ int    TblNum;
    /* 0x1c */ float  W;                // sprite size
    /* 0x20 */ float  H;
    /* 0x24 */ int    Col[4];           // r, g, b, a
    /* 0x34 */ int    _34[3];
};
static_assert(offsetof(EFF_DROP_WRK, pList) == 0x10, "EFF_DROP_WRK.pList");
static_assert(offsetof(EFF_DROP_WRK, TblNum) == 0x10 + 2 * sizeof(void *), "EFF_DROP_WRK.TblNum (EE 0x18)");
static_assert(offsetof(EFF_DROP_WRK, Col) == 0x10 + 2 * sizeof(void *) + 0x0c, "EFF_DROP_WRK.Col (EE 0x24)");

// ---- list A (bss 0x3d2190): the lamps of map effect kind 4 -----------------

// One lamp (EffLampReq zeroes 0x10..0x2b; EffLampDrawNormal runs State /
// Rot / Cnt, EffLampDrawAdd the four flicker counters).
struct EFF_LAMP                         /* 0x40 (list element size) */
{
    /* 0x00 */ sceVu0FVECTOR Pos;
    /* 0x10 */ int    State;            // normal lamp: 0 dark (wait), 1 fading in, 2 fading out
    /* 0x14 */ float  Rot;              // normal lamp: the glow's spin, +-pi
    /* 0x18 */ float  Cnt;              // normal lamp: frames into State
    /* 0x1c */ int    Flk[4];           // additive lamp: counters wrapping at 154, 44, 142, 74
    /* 0x2c */ int    Id;               // the map effect's put number
    /* 0x30 */ int    AddBlend;         // 1: ALPHA 0x48 (additive) and EffLampDrawAdd
    /* 0x34 */ u_char _34[0x0c];        // not written by EffLampReq
};
static_assert(sizeof(EFF_LAMP) == 0x40, "EFF_LAMP");
static_assert(offsetof(EFF_LAMP, State) == 0x10, "EFF_LAMP.State");
static_assert(offsetof(EFF_LAMP, Rot) == 0x14, "EFF_LAMP.Rot");
static_assert(offsetof(EFF_LAMP, Cnt) == 0x18, "EFF_LAMP.Cnt");
static_assert(offsetof(EFF_LAMP, Flk) == 0x1c, "EFF_LAMP.Flk");
static_assert(offsetof(EFF_LAMP, Id) == 0x2c, "EFF_LAMP.Id");
static_assert(offsetof(EFF_LAMP, AddBlend) == 0x30, "EFF_LAMP.AddBlend");

// ---- kind 8: the lens flare (FF2 SetRenzFlare) -----------------------------

// One ghost of the flare (FF2 EFRENZ; nine at data 0x3537e8, read in place).
// lscl: where on the line from the light through the screen centre (0: on
// the light), tscl: size in tens of pixels, rgba[3] the alpha the strength
// scales.
struct EFRENZ                           /* 0x10 */
{
    /* 0x00 */ int    type;             // effdat number (+ mono)
    /* 0x04 */ u_char rgba[4];
    /* 0x08 */ float  lscl;
    /* 0x0c */ float  tscl;
};
static_assert(sizeof(EFRENZ) == 0x10, "EFRENZ");
static_assert(offsetof(EFRENZ, rgba) == 0x04, "EFRENZ.rgba");
static_assert(offsetof(EFRENZ, lscl) == 0x08, "EFRENZ.lscl");
static_assert(offsetof(EFRENZ, tscl) == 0x0c, "EFRENZ.tscl");

// ---- list B (bss 0x3d2180): falling particles (FF3-only, never requested) --

// One particle: falls from the origin's height, sways in x / z with a tilt
// that follows the sway, fades in over 60 frames, fades out on landing.
struct EFF_FALL_PART                    /* 0x50 */
{
    /* 0x00 */ sceVu0FVECTOR Pos;
    /* 0x10 */ float  Tgt[4];           // sway targets x / -, z (+- speed[3])
    /* 0x20 */ float  Acc[4];           // sway x, fall speed (y), sway z
    /* 0x30 */ float  Rot[4];           // x / y / z tilt
    /* 0x40 */ short  LandCnt;          // frames since landing
    /* 0x42 */ short  FadeInCnt;        // 0..60
    /* 0x44 */ short  Col[3];           // drawn colour (EffFallPartLightCol)
    /* 0x4a */ short  Alpha;
    /* 0x4c */ int    _4c;
};
static_assert(sizeof(EFF_FALL_PART) == 0x50, "EFF_FALL_PART");
static_assert(offsetof(EFF_FALL_PART, Rot) == 0x30, "EFF_FALL_PART.Rot");
static_assert(offsetof(EFF_FALL_PART, Col) == 0x44, "EFF_FALL_PART.Col");
static_assert(offsetof(EFF_FALL_PART, Alpha) == 0x4a, "EFF_FALL_PART.Alpha");

// One emitter, a list B element (byte-copied into the list).
// layout: host -- pPart is a pointer (EE 4 bytes at +0); Speed stays at 0x10.
struct EFF_FALL_CTRL                    /* 0x60 */
{
    /* 0x00 */ EFF_FALL_PART *pPart;    // Max particles, EFFECT_MALLOC'd
    u_char _pad[16 - sizeof(EFF_FALL_PART *)];
    /* 0x10 */ float  Speed[4];         // sway step x, fall speed, sway step z, sway range
    /* 0x20 */ sceVu0FVECTOR Origin;    // y: the floor
    /* 0x30 */ int    Max;
    /* 0x34 */ int    Active;           // particles moved (EffFallPartCtrlInit sets Max)
    /* 0x38 */ float  Interval;         // frames per new particle
    /* 0x3c */ float  Timer;
    /* 0x40 */ int    FadeTime;         // frames of the fade after landing
    /* 0x44 */ int    Id;               // EffFallPartCut's key
    /* 0x48 */ short  Height;           // start above the origin
    /* 0x4a */ short  Spread;           // x / z width of the start square
    /* 0x4c */ short  Col[3];
    /* 0x52 */ short  Alpha;
    /* 0x54 */ u_char _54[0x0c];
};
static_assert(sizeof(EFF_FALL_CTRL) == 0x60, "EFF_FALL_CTRL");
static_assert(offsetof(EFF_FALL_CTRL, Speed) == 0x10, "EFF_FALL_CTRL.Speed");
static_assert(offsetof(EFF_FALL_CTRL, Origin) == 0x20, "EFF_FALL_CTRL.Origin");
static_assert(offsetof(EFF_FALL_CTRL, Interval) == 0x38, "EFF_FALL_CTRL.Interval");
static_assert(offsetof(EFF_FALL_CTRL, Id) == 0x44, "EFF_FALL_CTRL.Id");
static_assert(offsetof(EFF_FALL_CTRL, Col) == 0x4c, "EFF_FALL_CTRL.Col");
static_assert(offsetof(EFF_FALL_CTRL, Alpha) == 0x52, "EFF_FALL_CTRL.Alpha");

// ---- list C (bss 0x3d45a0): particle bursts of 11 types ---------------------

// One particle. The type's particle init (table 0x353c68) fills it; the
// draw is picked by Tex (< 0: -1 / -2 draw specials) and DrawMode.
struct EFF_PARTICLE                     /* 0x80 */
{
    /* 0x00 */ sceVu0FVECTOR Pos;
    /* 0x10 */ sceVu0FVECTOR Vel;
    /* 0x20 */ sceVu0FVECTOR Acc;
    /* 0x30 */ int    Col[4];           // r, g, b, a
    /* 0x40 */ float  Gravity;          // added to Vel y each frame
    /* 0x44 */ float  SizeX;
    /* 0x48 */ float  SizeY;
    /* 0x4c */ float  SizeAdd;          // added to both sizes each frame
    /* 0x50 */ float  Ang;              // wrapped to +-pi
    /* 0x54 */ float  AngVel;
    /* 0x58 */ float  KickRate;         // chance per frame of a random Acc kick
    /* 0x5c */ int    Age;
    /* 0x60 */ int    Life;
    /* 0x64 */ int    Tex;              // effdat number; -1 / -2: the special draws
    /* 0x68 */ int    DrawMode;         // 0: FUN_002b4cf8, else FUN_002b4968
    /* 0x6c */ int    Move;             // 0: frozen (only size and age run)
    /* 0x70 */ int    Alive;
    /* 0x74 */ u_char _74[0x0c];
};
static_assert(sizeof(EFF_PARTICLE) == 0x80, "EFF_PARTICLE");
static_assert(offsetof(EFF_PARTICLE, Col) == 0x30, "EFF_PARTICLE.Col");
static_assert(offsetof(EFF_PARTICLE, Gravity) == 0x40, "EFF_PARTICLE.Gravity");
static_assert(offsetof(EFF_PARTICLE, Ang) == 0x50, "EFF_PARTICLE.Ang");
static_assert(offsetof(EFF_PARTICLE, Age) == 0x5c, "EFF_PARTICLE.Age");
static_assert(offsetof(EFF_PARTICLE, Tex) == 0x64, "EFF_PARTICLE.Tex");
static_assert(offsetof(EFF_PARTICLE, Alive) == 0x70, "EFF_PARTICLE.Alive");

#define EFF_PARTICLE_TYPE_NUM 11

// One burst, a list C element (byte-copied into the list). The type's setup
// (table 0x353c38) fills Spread .. FreeWhenEmpty.
// layout: host -- pPart is a pointer (EE 4 bytes at +0); Pos stays at 0x10.
struct EFF_PARTICLE_CTRL                /* 0x70 */
{
    /* 0x00 */ EFF_PARTICLE *pPart;     // Num particles, EFFECT_MALLOC'd
    u_char _pad[16 - sizeof(EFF_PARTICLE *)];
    /* 0x10 */ sceVu0FVECTOR Pos;
    /* 0x20 */ float  Spread[4];        // start offset range per axis
    /* 0x30 */ float  Kick[4];          // random acceleration range per axis
    /* 0x40 */ u_long AlphaReg;         // ALPHA_1 value
    /* 0x48 */ u_long TestReg;          // TEST_1 value
    /* 0x50 */ int    Num;
    /* 0x54 */ float  EmitRate;         // chance per frame of a new particle
    /* 0x58 */ float  Drag;             // 1.0: none
    /* 0x5c */ float  FloorY;           // type 3: the after-move below it
    /* 0x60 */ u_int  Type;             // 0..10
    /* 0x64 */ int    FreeWhenEmpty;    // EffParticleExec frees it once nothing is alive
    /* 0x68 */ int    Valid;
    /* 0x6c */ int    Id;               // EffParticleCut's key
};
static_assert(sizeof(EFF_PARTICLE_CTRL) == 0x70, "EFF_PARTICLE_CTRL");
static_assert(offsetof(EFF_PARTICLE_CTRL, Pos) == 0x10, "EFF_PARTICLE_CTRL.Pos");
static_assert(offsetof(EFF_PARTICLE_CTRL, AlphaReg) == 0x40, "EFF_PARTICLE_CTRL.AlphaReg");
static_assert(offsetof(EFF_PARTICLE_CTRL, Num) == 0x50, "EFF_PARTICLE_CTRL.Num");
static_assert(offsetof(EFF_PARTICLE_CTRL, Type) == 0x60, "EFF_PARTICLE_CTRL.Type");
static_assert(offsetof(EFF_PARTICLE_CTRL, Id) == 0x6c, "EFF_PARTICLE_CTRL.Id");

// ---- the rain (bss 0x3d45b0; FF3-only, never requested) --------------------

// One drop: a streak along its velocity.
struct EFF_RAIN_DROP                    /* 0x30 */
{
    /* 0x00 */ sceVu0FVECTOR Pos;
    /* 0x10 */ sceVu0FVECTOR Vel;
    /* 0x20 */ float  Len;
    /* 0x24 */ float  RotX;             // Vector2Rot of Vel
    /* 0x28 */ float  RotY;
    /* 0x2c */ int    _2c;
};
static_assert(sizeof(EFF_RAIN_DROP) == 0x30, "EFF_RAIN_DROP");
static_assert(offsetof(EFF_RAIN_DROP, Len) == 0x20, "EFF_RAIN_DROP.Len");

#define EFF_RAIN_DROP_NUM 306           // EffRainReq: 0x3960 bytes

// layout: host -- pDrop is a pointer (EE 4 bytes at +0x34).
struct EFF_RAIN_WRK
{
    /* 0x00 */ int    HeadCol[4];       // the leading end: r, g, b, a
    /* 0x10 */ int    TailCol[4];
    /* 0x20 */ sceVu0FVECTOR Origin;
    /* 0x30 */ int    Num;
    /* 0x34 */ EFF_RAIN_DROP *pDrop;
};
static_assert(offsetof(EFF_RAIN_WRK, Origin) == 0x20, "EFF_RAIN_WRK.Origin");
static_assert(offsetof(EFF_RAIN_WRK, Num) == 0x30, "EFF_RAIN_WRK.Num");

// layout: host -- pSgdTop is a pointer (EE 4 bytes at +0).
struct MODEL_ALPHA_CAHNGE_DATA          /* 0x18 on the EE (list 0x3c3d40; FF2's spelling) */
{
    /* 0x00 */ void *pSgdTop;
    /* 0x04 */ int   Id;                // EffectModelAlphaChangeDelete's key
    /* 0x08 */ int   AlphaStart;
    /* 0x0c */ int   AlphaEnd;
    /* 0x10 */ int   Time;
    /* 0x14 */ int   Counter;
};

// layout: host -- pHandle is a pointer (EE 4 bytes at +0).
struct TOUROU_FREA_DATA                 /* 0x20 on the EE (list 0x3c3d00) */
{
    /* 0x00 */ void *pHandle;
    /* 0x04 */ int   Id;                // EffectTourouFreaDelete's key
    /* 0x08 */ float ScaleX;
    /* 0x0c */ float ScaleY;
    /* 0x10 */ int   Alpha;
    /* 0x14 */ int   Time;
    /* 0x18 */ int   Counter;
    /* 0x1c */ int   FlameCounter;
};

struct TOUROU_BASE_DATA                 /* 0x20 on the EE (list 0x3c3d10) */
{
    /* 0x00 */ sceVu0IVECTOR Color;
    /* 0x10 */ void *pSgdTop;
    /* 0x14 */ int   Id;                // EffectTourouBaseDelete's key
    /* 0x18 */ int   Time;
    /* 0x1c */ int   Counter;
};
static_assert(offsetof(TOUROU_BASE_DATA, pSgdTop) == 0x10, "TOUROU_BASE_DATA.pSgdTop");

// Delete by id; a negative id deletes every entry (FF3's addition, used by
// EffectObjReleaseAll).
void EffectLightComeInDeleteMapBuffId(int MapBuffId);  // FF3 0x0016e150
void EffectWaterFlowDelete(int Id);                 // FF3 0x0016e500: also MapDrawDeleteNoDraw

// Map objects with an effect (MapDrawRoomObjRegist, furniture 801..999).
void EffectLightComeInRegist(void *pSgdTop, int MapBuffId, int Type);  // FF3 0x0016e0c8: a LIGHT_COME_IN entry (list 0x3c3d20; Type 1: 0xa0, 2: 100, else 0x69)
void EffectWaterFlowRegist(void *pSgdTop, int Id, int MoveType);   // FF3 0x0016e478: a water-flow entry (list 0x3c3d30), its material alpha set
const WATER_FLOW_MOVE_DAT *EffectWaterFlowGetMoveDat(int MoveType);  // FF3 0x0016e450: a move type's row, row 0 past 11 (also IgEffectWaterFlowModelDraw)
void EffThunderApplyLight(void);        // FF3 0x00164818: while the lightning lights the room (0x3a6aa8), its light into light 1 and applied
void EffectModelAlphaChangeDelete(int Id);          // FF3 0x0016e860
void EffectTourouFreaDelete(int Id);                // FF3 0x0016ec70
void EffectTourouBaseDelete(int Id);                // FF3 0x0016f110

// ---- requests other TUs make ----------------------------------------------

// The parts deform front ends FF3 keeps from FF2 (no caller in the ROM);
// each returns SetEffects' effect. CallPartsDeform3 has FF2
// CallPartsDeform3_2's arguments, CallPartsDeform3_2 CallPartsDeform5_2's
// form without the wave parameters.
void *CallPartsDeform3(int type, float sclx, float scly, void *pos,
                       u_int in, u_int keep, u_int out, int alp);  // FF3 0x00168168: SetEffects(0x16, 4, ...)
void *CallPartsDeform3_2(int type, float sclx, float scly, void *pos, float *alpha);  // FF3 0x00168238: SetEffects(0x16, 2, ...)
void *CallPartsDeform6(int type, float scale, void *pos, u_int in, u_int keep, u_int out, int alp,
                       float *pSpd, float *pRate, int r, int g, int b);  // FF3 0x001683a0: FF2's arguments

// The lens flare (FF2 SetRenzFlare's body): flare > 0 draws the ghosts and
// the corona at pos when the camera looks along rot (within 60 degrees),
// num star rays; other >= 0 runs EffRenzFlareGhost at the same place.
// ingame_effect.c (the player's spotlight) and scene_effect.c call it.
void  EffRenzFlareDraw(float flare, float other, float *pos, float *rot, int num);  // FF3 0x0016c480
// num spikes round the screen point bpos turned by aang degrees, alpha x sc.
void  SetStarRay(float *bpos, int tp, float sc, int num, float aang);  // FF3 0x0016cd70: FF2 SetStarRay

// The light come-in model of pSgdTop (list 0x3c3d20), nullptr if none.
LIGHT_COME_IN_DATA *EffectLightComeInGetDataPtr(void *pSgdTop);  // FF3 0x0016e3b8: ingame_effect.c
// Its alpha rates, read by EffectLightComeInExecOne: 1 with the camera along / against the model's z axis,
// 0 across it; 0 nearer than 20, 1 from 100.
float EffectLightComeInCalcAlphaRateCamDir(void *pSgdTop);  // FF3 0x0016d438
float EffectLightComeInCalcAlphaRateCamDist(void *pSgdTop); // FF3 0x0016d5b0
// One frame of a surface (ingame_effect.c): ST scroll, alpha, vertex colours;
// AlphaCalcMode 1 also fades it out near the camera.
void  EffectLightComeInExecOne(LIGHT_COME_IN_DATA *pData, int AlphaCalcMode);  // FF3 0x0016e1f8

// The model alpha ramps (FF2's, the SGD material alpha in FF3).
void  EffectModelAlphaChangeRegist(void *pSgdTop, int Id);     // FF3 0x0016e818
void  EffectModelAlphaChangeDeleteGroup(int Id);               // FF3 0x0016e908: every Id / 1000 == Id
void  EffectModelAlphaChangeReq(int Id, int AlphaStart, int AlphaEnd, int Time);  // FF3 0x0016eab0

// The stone lanterns (FF2's): the flame's scale / alpha cycle and the base's
// colour pulse, stepped and read by ingame_effect.c's lantern draw.
void  EffectTourouFreaRegist(void *pHandle, int Id);           // FF3 0x0016eb88
void  EffectTourouFreaExecOne(TOUROU_FREA_DATA *pData);        // FF3 0x0016ef00
TOUROU_FREA_DATA *EffectTourouFreaGetDataPtr(void *pHandle);   // FF3 0x0016ef90
void  EffectTourouBaseRegist(void *pSgdTop, int Id);           // FF3 0x0016f040
void  EffectTourouBaseExecOne(TOUROU_BASE_DATA *pData);        // FF3 0x0016f320
TOUROU_BASE_DATA *EffectTourouBaseGetDataPtr(void *pSgdTop);   // FF3 0x0016f3d8

// The file screen's flipping panel (menu_file.c MenuFileTopWinDisp, FF3-only):
// a textured quad seen from straight above by a camera of its own, turned
// about z from AngStart to AngEnd between frames Start and End. Init saves the
// game camera, CamRestore puts it back.
struct EFF_FLIP_PANEL                   /* 0x58 used, 0x60 with the vectors' alignment */
{
    /* 0x00 */ float Ang;               // this frame's turn
    /* 0x04 */ float AngStart;
    /* 0x08 */ float AngEnd;
    /* 0x0c */ int   Cnt;
    /* 0x10 */ int   Start;             // frame the turn starts
    /* 0x14 */ int   End;               // frame it reaches AngEnd
    /* 0x18 */ int   Life;              // inactive after this frame
    /* 0x1c */ int   _1c;
    /* 0x20 */ float ZOff;              // the quad's centre along z
    /* 0x24 */ float ScaleX;            // world units per texel
    /* 0x28 */ float ScaleY;
    /* 0x2c */ int   Active;
    /* 0x30 */ sceVu0FVECTOR CamPos;    // the saved camera
    /* 0x40 */ sceVu0FVECTOR CamTarget;
    /* 0x50 */ float CamRoll;
    /* 0x54 */ float CamFov;
};
static_assert(sizeof(EFF_FLIP_PANEL) == 0x60, "EFF_FLIP_PANEL");
static_assert(offsetof(EFF_FLIP_PANEL, CamFov) == 0x54, "EFF_FLIP_PANEL.CamFov");
static_assert(offsetof(EFF_FLIP_PANEL, ZOff) == 0x20, "EFF_FLIP_PANEL.ZOff");
static_assert(offsetof(EFF_FLIP_PANEL, Active) == 0x2c, "EFF_FLIP_PANEL.Active");
static_assert(offsetof(EFF_FLIP_PANEL, CamPos) == 0x30, "EFF_FLIP_PANEL.CamPos");
static_assert(offsetof(EFF_FLIP_PANEL, CamRoll) == 0x50, "EFF_FLIP_PANEL.CamRoll");

void  EffFlipPanelInit2(EFF_FLIP_PANEL *fp);    // FF3 0x0016f470: unreferenced twin of EffFlipPanelInit
void  EffFlipPanelInit(EFF_FLIP_PANEL *fp);     // FF3 0x0016f508
void  EffFlipPanelCamRestore2(EFF_FLIP_PANEL *fp);  // FF3 0x0016f5a0: unreferenced twin
void  EffFlipPanelCamRestore(EFF_FLIP_PANEL *fp);   // FF3 0x0016f5e0
void  EffFlipPanelStart(EFF_FLIP_PANEL *fp, int dir, int start, int end, int life);  // FF3 0x0016f620: dir -1: 0 -> pi, else pi -> 0
int   EffFlipPanelStep(EFF_FLIP_PANEL *fp);     // FF3 0x0016f678: 0 when inactive
void  EffFlipPanelDraw(EFF_FLIP_PANEL *fp, u_long tex0, int *uv, int *col);  // FF3 0x0016f730: uv u, v, w, h in texels

// Kind 0x1d requests with an id (SetEffects' v, KIND1D_WRK.Id).
void *EffKind1DIdReq(float *pos, int id, int no);              // FF3 0x0016f9a0: no caller in the ROM
void *EffKind1DReq3(float *pos, float *rot, float *alpha);     // FF3 0x0016f9d0: record 3, id 998; no caller
void *EffKind1DReq4(float *pos, float *rot, float *alpha);     // FF3 0x0016fa00: record 4, id 999 (SceneEffectHazeRegist)
void *EffKind1DReq5(float *pos, float *rot, float *alpha);     // FF3 0x0016fa30: record 5, id 999; no caller
void  EffKind1DCutId(int id);                                  // FF3 0x0016ff38

// The enemy effects (ingame/enemy/enemy.c EneDeformInit / EneObjEffInit);
// each returns SetEffects' effect.
void *CallPartsDeform5_2(float scale0, float scale1, u_char no, float *pos, float *alpha, float *prm0, float *prm1);    // FF3 0x001682e0: SetEffects(0x16, 2, ...)
void *EffKind1DReq(float *pos, float *rot, float *alpha, int no, int flag);  // FF3 0x0016fb08: SetEffects(0x1d, 2, ...); flag: EffKind1DTexLoad first
void  EffKind1DSetRate(float v, void *eff);   // FF3 0x0016fbb0: v into the record's fw[0] (nullptr: nothing)
void  EffKind1DCut(void *eff, int flag);  // FF3 0x0016fb78: EffKind1DRelease; flag: EffKind1DTexRelease after

void EffPlyrHazeReq(void);              // FF3 0x0016b738: PlyrFlashRelease / PlyrHoldShakeOff when a hold ends

void EffectThunderLightInit(void);      // FF3 0x00164568: FUN_001622d0 (the effect_oth init)
void EffThunderReq(float *pos, int time, float *v);  // FF3 0x00164588: EV_EFF_THUNDER_REQ
int  EffThunderIsLightning(void);       // FF3 0x001648d8: 1 while the lightning lights the room (char_model.c)
struct G3DLIGHT;                        // graphics/graph3d/g3dLight.h
void EffThunderSetLight(G3DLIGHT *light);  // FF3 0x00164868: light becomes the lightning's directional light (char_model.c)
void EffSealDoorBurnInit(void);         // FF3 0x00164c00: EffectObjOthInit
void EffSealDoorBurnLoad(int no);       // FF3 0x00164c50: the burn file of row no (0x352e70) into the heap, once
int  EffSealDoorBurnIsEnd(void);        // FF3 0x00164cf0
void EffSealDoorBurnRelease(void);      // FF3 0x00164d00
void EffSealDoorBurnStep(void);         // FF3 0x00164d78: InitEffectObjEF

// The kind 0x1e glow and the parts deform rows of the photo objects
// (ingame/photo/photo_obj.c).
void *EffKind1EReq(float rate, float *pos);    // FF3 0x001648e8: SetEffects(0x1e, 2, ...) at a copy of pos (one static vector)
int   EffKind1ESetRate(float rate, void *eff); // FF3 0x00164928: the alpha rate (dat.fl32[1]); 0 for nullptr
int   EffKind1ECut(void *eff);                 // FF3 0x00164948: EffectRelease; 0 for nullptr
EFF_PDEFORM_PRM *EffPDeformPrmGet(int no);     // FF3 0x00164a00: row 0..2 of data 0x2f88a8

// The room SCL buffers (graphics/effect/effect_oth.c FUN_0017de50 / EffMapPartReq).
int   EffObjBufTblFind(int id);                // FF3 0x001764a0: the record index of room id, -1 when none (EffObjBufTblIsSet)
void  EffObjBufTblSelect(int id);              // FF3 0x001764d8: the record of room id becomes the current one (gp-0x58c)
EFF_OBJ_BUF *EffObjBufTblGet(void);           // FF3 0x001764f8: the current record, the default one when none
void EffLanternEffectEnable(int on);    // FF3 0x00165d18: SET_LANTERNEFFECT_ENABLE (sdata gp-0x5ac)

// The map effects (ingame/map/MapEff.c MapEffCreate / MapEffDelete / draw callbacks).
void *CallFire2(void *pos, u_char r, u_char g, u_char b, float scl,
                u_char r2, u_char g2, u_char b2, float scl2);              // FF3 0x00162338: unreferenced
void *EffCandleFireReq(float a, float b, float c, float d, float *pos);    // FF3 0x00162418: SetEffects(0x14, 2, 4, pos, ...): a candle flame
void *EffBlueFireReq(float a, float b, float c, float d, float *pos);      // FF3 0x001624c8: the blue one (0x30 0x30 0xff, max 1)
float CandleFlameScaleWGet(int type, int frame);   // FF3 0x00162580: SubCandleFlameDraw, 0x2455f8
float CandleFlameScaleHGet(int type, int frame);   // FF3 0x001625a8
void  EffCandleFireNearCheck(void *eff, float *pos);    // FF3 0x00162f88: IgEffectCandleYuramekiCtrl; a flame near pos to flow 5 / 6
void  EffOthCandleFlameFlareUpReq(void *eff);   // FF3 0x00163068: the flame flares up (FURN_CANDLE_BLAZE_UP)
void  EffCandleFireDraw(void *eff);     // FF3 0x001630e0: draws it
void  EffCandleFreaDraw(float scale, float h, float *pos, int Depth, int R, int G, int B, int Alpha);  // FF3 0x001625d0: a candle's flare (effdat 0x46), Depth < 0: own z (also SubCandleFlameDraw, 0x183f88)
void  EffCandleFlameOneDraw(float scale, float *pos, int R, int G, int B, int Alpha, int cnt);     // FF3 0x00163e50: one candle flame, flicker row cnt (also IgEffectTourouFreaModelDrawSub)
void  EffBlueFireFreaDraw(float scale, float h, float *pos, int Depth, int R, int G, int B, int Alpha);  // FF3 0x001629b8: the blue flame's flare, a 7 x 7 grid (SubCandleFlameDraw)
void  SubCandleFlameDraw(EFFECT_CONT *ec);      // FF3 0x002b1420: kind 0x14's draw (EffCandleFireDraw)
void  SubCandleFlameStep(EFFECT_CONT *ec);      // FF3 0x002b1d88: kind 0x14's clock (SetFire)
int   EffLanternReq(int no, int area);  // FF3 0x00167e28: kind 600, the floating lanterns of area 0x29 / 0x2b / 0x2c; 1 when made
void  EffLanternCut(int no);            // FF3 0x00167fa8: -1: all
void *EffKind1DMapReq(float *pos, int no);  // FF3 0x0016f970: SetEffects(0x1d, 2, pos, ...): kinds 500..502 (no 0..2)
void  EffKind1DRelease(void *eff);      // FF3 0x0016fed0: ends it
void  EffLampReq(float *pos, int id, int flag);     // FF3 0x00172580: map effect kind 4 (lamp); flag: additive
void  EffLampCut(int id);               // FF3 0x00172600: -1: all
void  EffLampDraw(int id);              // FF3 0x001726c0: draws lamp id (MapEffDrawLamp)
int   EffParticleReq(u_int type, float *pos, int id);  // FF3 0x00172ff8: a list C burst of type 0..10; 0 when the type setup failed
void  EffParticleCut(int type, int id);  // FF3 0x00173078: negative: any
void  EffFallPartReq(float *pos, int id);  // FF3 0x00171718: unreferenced
void  EffFallPartCut(int id);           // FF3 0x001717f8: -1: all
void  EffRainReq(float *pos);           // FF3 0x00174c78: unreferenced

// The many-candle altars.
void  EffManyCandleLoadMain(MANY_CANDLE_LOAD_CTRL *pLoadCtrl);  // FF3 0x00163190
void  EffectManyCandleLoadReq(float *Offset);   // FF3 0x00163230: unreferenced
void  EffectManyCandleLoadCut(void);    // FF3 0x001632a0: InitEffectObj
void *EffManyCandleReq(float *pCenterPos, float *pPos, int DataNum, int Id);    // FF3 0x00163498
void  EffectManyCandleCut(void *pEffect);       // FF3 0x001634c8

// Kind 0x1e's matrix (also PhotoObjSlotEffStep): rotation toward the camera; out =
// mat * (0, -1, -10, 1) + pos.
void EffKind1EMatrix(float *out, sceVu0FMATRIX mat, float *pos);   // FF3 0x00164a20

// The room glow (kind 0x10).
void EffRoomGlowClear(void);            // FF3 0x00165438: SceneEffectInit
void EffRoomGlowSetFixed(int fixed);    // FF3 0x00165428
void EffRoomGlowSet(u_char r, u_char g, u_char b, u_char alpha, u_char boost);  // FF3 0x00165470: the fixed block (caller 0x256d04)

// Called by effect.c (inits, release, the per-kind draws of EffectZSort and
// EffectControl).
void  EffectObjOthInit(void);                   // FF3 0x001622d0: InitEffects (FF2 InitEffectOth's tail: many candle load, thunder, seal door burn)
void  InitEffectObjEF(void);                    // FF3 0x001622f8: InitEffectsEF (FF2 InitEffectOthEF's tail: many candle load main, seal door burn step)
void  InitEffectObj(void);                      // FF3 0x00162318: EffectsReleaseAll
void  SetFire(EFFECT_CONT *ec);                 // FF3 0x00163090: kind 0x14
void  SetManyCandle(EFFECT_CONT *ec);           // FF3 0x00163410: kind 0x1c
void  SetHalo(EFFECT_CONT *ec);                 // FF3 0x00164100: kind 0x13
void  EffectThunderLightExec(void);             // FF3 0x001645c8: EffectInPhoto
void  SetEffKind1E(EFFECT_CONT *ec);            // FF3 0x00164970: kind 0x1e, before the draws
void  EffSealDoorBurnDraw(void);                // FF3 0x001652a0: last call of EffectDrawMain
void  EffRoomGlowInit(void);                    // FF3 0x00165530: InitEffects
void  SetRoomGlow(EFFECT_CONT *ec);             // FF3 0x00165578: kind 0x10 (slot 16), the room glow (bloom) pass
void  EffLanternMdlLoad(void);                  // FF3 0x00165d20: InitEffects (LANTERN_MDL_PK4 into a GameModel)
void  EffLanternMdlRelease(void);               // FF3 0x00165d60: EffectsReleaseAll
int   EffLanternMdlIsLoaded(void);              // FF3 0x00165d98: CheckSetEffectsOK (the model's vtable +0x10 answer)
void  EffLanternInit(void);                     // FF3 0x00167d50: InitEffects
void  EffKind600AllCut(void);                   // FF3 0x00167db8: EffectsReleaseAll (EffLanternCut(-1))
void  EffLanternExec(void);                     // FF3 0x00167dd0: EffectInPhoto
void  EffectObjInit(void);                      // FF3 0x00168078: InitEffects (FF2 InitEffectObj's body: deform slots, the lists)
void  EffectObjInitEF(void);                    // FF3 0x001680e0: InitEffectsEF (frees deform slots not drawn last frame)
void  EffectObjReleaseAll(void);                // FF3 0x00168130: EffectsReleaseAll (empties the five lists)
void  SetPartsDeform(EFFECT_CONT *ec);          // FF3 0x00168490: kinds 0x16 / 0x1f
int   EffectObjPartsDeformBlurCheck(EFFECT_CONT *ec);   // FF3 0x00168770: non-zero: the deform waits for the blur pass
void  SetHaze(EFFECT_CONT *ec);                 // FF3 0x0016b780: kind 0x1b
float EffHazeLevel(int cnt);                    // FF3 0x0016bc00: a haze record's level from its cnt (1 .. 0 over 10 frames)
void  SetRenzFlare(EFFECT_CONT *ec);            // FF3 0x0016c420: kind 8 (slot 8, FF2 SetRenzFlare)
void  EffectWaterFlowExec(void);                // FF3 0x0016e5a8
void  EffectModelAlphaChangeExec(void);         // FF3 0x0016e9c8
void  EffKind1DInit(void);                      // FF3 0x0016f910: InitEffects (three list inits)
void  EffKind1DInitEF(void);                    // FF3 0x0016f938: InitEffectsEF
void  EffKind1DReleaseAll(void);                // FF3 0x0016f940: EffectsReleaseAll (the three kind 0x1d lists)
void  SetEffKind1D(EFFECT_CONT *ec);            // FF3 0x0016fdc0: kind 0x1d
void  EffKind1DListBExec(void);                 // FF3 0x00171d08: EffectInPhoto
void  EffParticleExec(void);                    // FF3 0x00173160: EffectInPhoto (list C)
void  EffRainInit(void);                        // FF3 0x00174c20: InitEffects
void  EffRainRelease(void);                     // FF3 0x00174da0: EffectsReleaseAll
void  EffRainExec(void);                        // FF3 0x001752b0: EffectInPhoto
void  EffAreaPartInit(void);                    // FF3 0x001754e0: InitEffects
void  EffAreaPartReq(float *pos);               // FF3 0x00175620: no caller in the ROM
void  EffAreaPartReleaseAll(void);              // FF3 0x00175770: EffectsReleaseAll
void  EffAreaPartExec(void);                    // FF3 0x00175ae0: EffectInPhoto
void  EffDropInit(void);                        // FF3 0x00175f38: InitEffects
void  EffDropExec(void);                        // FF3 0x00175f90: EffectInPhoto
void  EffDropReq(float *pos);                   // FF3 0x001760c0: no caller in the ROM
void  EffDropRelease(void);                     // FF3 0x00176208: EffectsReleaseAll
void  EffObjBufTblInit(void);                   // FF3 0x00176450: InitEffects (first; loads the 0x2f9210 buffers)
void  EffObjBufTblRelease(void);                // FF3 0x00176470: EffectsReleaseAll (first; the 0x2f9210 buffers)
int   EffObjIsLoadEnd(void);                    // FF3 0x00176488: CheckSetEffectsOK (IsLoadEndAll)

// List C's per-type setups (table 0x353c38; each returns how many particles
// start at once) and particle inits (table 0x353c68).
void  EffKind1DTexRelease(void);                // FF3 0x0016fae0
void  EffRenzFlareGhost(float rate, float *pos, float *rot);   // FF3 0x0016d070: EffRenzFlareDraw's second part
int   EffParticleType0Setup(EFF_PARTICLE_CTRL *ctrl);    // FF3 0x00173258
int   EffParticleType1Setup(EFF_PARTICLE_CTRL *ctrl);    // FF3 0x00173450
int   EffParticleType2Setup(EFF_PARTICLE_CTRL *ctrl);    // FF3 0x00173760
int   EffParticleType3Setup(EFF_PARTICLE_CTRL *ctrl);    // FF3 0x00173ab8
int   EffParticleType4Setup(EFF_PARTICLE_CTRL *ctrl);    // FF3 0x00173e38
int   EffParticleType5Setup(EFF_PARTICLE_CTRL *ctrl);    // FF3 0x00173fb8
int   EffParticleType6Setup(EFF_PARTICLE_CTRL *ctrl);    // FF3 0x00174220
int   EffParticleType7Setup(EFF_PARTICLE_CTRL *ctrl);    // FF3 0x00174478
int   EffParticleType8Setup(EFF_PARTICLE_CTRL *ctrl);    // FF3 0x001746b8
int   EffParticleType9Setup(EFF_PARTICLE_CTRL *ctrl);    // FF3 0x00174908
int   EffParticleType10Setup(EFF_PARTICLE_CTRL *ctrl);   // FF3 0x00174a88
void  EffParticleType0Init(EFF_PARTICLE *p, EFF_PARTICLE_CTRL *ctrl, float *off);   // FF3 0x001732e8
void  EffParticleType1Init(EFF_PARTICLE *p, EFF_PARTICLE_CTRL *ctrl, float *off);   // FF3 0x001734c0
void  EffParticleType2Init(EFF_PARTICLE *p, EFF_PARTICLE_CTRL *ctrl, float *off);   // FF3 0x001737e0
void  EffParticleType3Init(EFF_PARTICLE *p, EFF_PARTICLE_CTRL *ctrl, float *off);   // FF3 0x00173b40
void  EffParticleType4Init(EFF_PARTICLE *p, EFF_PARTICLE_CTRL *ctrl, float *off);   // FF3 0x00173eb0
void  EffParticleType5Init(EFF_PARTICLE *p, EFF_PARTICLE_CTRL *ctrl, float *off);   // FF3 0x00174048
void  EffParticleType6Init(EFF_PARTICLE *p, EFF_PARTICLE_CTRL *ctrl, float *off);   // FF3 0x001742b0
void  EffParticleType7Init(EFF_PARTICLE *p, EFF_PARTICLE_CTRL *ctrl, float *off);   // FF3 0x00174508
void  EffParticleType8Init(EFF_PARTICLE *p, EFF_PARTICLE_CTRL *ctrl, float *off);   // FF3 0x00174748
void  EffParticleType9Init(EFF_PARTICLE *p, EFF_PARTICLE_CTRL *ctrl, float *off);   // FF3 0x00174990
void  EffParticleType10Init(EFF_PARTICLE *p, EFF_PARTICLE_CTRL *ctrl, float *off);  // FF3 0x00174b10

#endif /* _GRAPHICS_EFFECT_EFFECT_OBJ_H */
