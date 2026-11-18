/* ==========================================================================
 *  ingame/enemy/ene_spe_obj.h
 *
 *  FF3 0x001985c0..0x00199150, FF3-only (no FF2 counterpart; the file name
 *  is a link-order guess kept for the EneSpe family of Ghidra names).
 *  Two things share the file:
 *  - the region-section list helpers RegSecList*: the section records of the
 *    rectangles holding a point (BgSe.c's LayoutSe uses them too);
 *  - the real world's special SEs: nine rows (data 0x324060) of positional
 *    SEs from the SYS_REAL_1 / SYS_REAL_2 banks, each tied to a region
 *    label. When the player enters a row's region the row rolls its chance
 *    once and, if no object of its group runs, starts one of the table's
 *    ten objects: after a random delay the SE plays at the row's position
 *    (relative to the area's map offset), optionally moving along a fixed
 *    velocity, its volume faded by the player's distance. Started on the
 *    way into the real world (IngameNowLoadingMain, EvGoRealWorld), stepped
 *    by IngameMain, freed on the way into the dream world.
 * ======================================================================== */
#ifndef _INGAME_ENEMY_ENE_SPE_OBJ_H
#define _INGAME_ENEMY_ENE_SPE_OBJ_H

#include <stddef.h>                     // offsetof
#include "sdk/scetypes.h"
#include <libvu0.h>                     // sceVu0FVECTOR
#include "system/eeiop/snd_3d.h"        // SND_3D_SET

struct MB_OUT_SECTION;

// ──────────────────────────────────────────────────────────────────────
// Region-section lists

// The section records (rectangle type `type`) whose rectangles hold pos on
// floor, each once; -1 when no resident slot covers pos. max is passed on
// but never checked (FF3 too).
int RegSecListGet4Pos(float *pos, int floor, MB_OUT_SECTION **list, int max,
                      int type);                                        // FF3 0x00198640
int RegSecListHasLabel(int label, MB_OUT_SECTION **list, int num);     // FF3 0x001985c0: 1 when a record in list has labelID label
int RegSecListAddUnique(MB_OUT_SECTION **list, int max, int num,
                        MB_OUT_SECTION *rec);                           // FF3 0x001985f8: appends rec unless present; the new count

// ──────────────────────────────────────────────────────────────────────
// Special SEs

#define ENE_SPE_OBJ_NUM     10          // objects of the table
#define ENE_SPE_DAT_NUM     9           // rows of ene_spe_obj_dat (0x324060)

// ENE_SPE_OBJ_DAT.side
enum ENE_SPE_SIDE
{
    ENE_SPE_SIDE_0   = 0,               // only while MapAreaSideGet() is 0
    ENE_SPE_SIDE_1   = 1,               // only while it is 1
    ENE_SPE_SIDE_ANY = 2,               // any other value: either side
};

// One row (data 0x324060, 9 x 0x48; no pointers, the same on the hosts).
// started is written at run time.
struct ENE_SPE_OBJ_DAT
{
    /* 0x00 */ u_int  se;               // bank file << 16 | SE number
    /* 0x04 */ int    group;            // one running object per group (EneSpeObjCanStart)
    /* 0x08 */ int    label;            // region labelID that arms the row
    /* 0x0c */ float  rate;             // chance in percent (GetRandValF(100) <= rate)
    /* 0x10 */ float  ofs[3];           // position relative to the area's map offset
    /* 0x1c */ u_char loop;             // 1: replay while the player stays in the region
    /* 0x1d */ u_char _01d[3];
    /* 0x20 */ float  time;             // frames the object lives, 0: until the SE ends
    /* 0x24 */ float  move;             // frames the source moves by its velocity
    /* 0x28 */ float  speed;            // velocity length per frame (CamCirclePos r)
    /* 0x2c */ float  angle;            // velocity direction in 1/100 turns
    /* 0x30 */ float  fade;             // SndSeFadeStop frames when it ends
    /* 0x34 */ float  far_dist;         // silent beyond
    /* 0x38 */ float  min_vol;          // volume (percent) at far_dist
    /* 0x3c */ float  near_dist;        // full volume within
    /* 0x40 */ float  delay;            // random delay before playing, frames (GetRandValF)
    /* 0x44 */ u_char side;             // ENE_SPE_SIDE
    /* 0x45 */ u_char started;          // 1 while the player stays in the region after the roll
    /* 0x46 */ u_char _046[2];
};
static_assert(sizeof(ENE_SPE_OBJ_DAT) == 0x48, "ENE_SPE_OBJ_DAT is 0x48 bytes");
static_assert(offsetof(ENE_SPE_OBJ_DAT, group) == 0x04, "ENE_SPE_OBJ_DAT.group");
static_assert(offsetof(ENE_SPE_OBJ_DAT, label) == 0x08, "ENE_SPE_OBJ_DAT.label");
static_assert(offsetof(ENE_SPE_OBJ_DAT, rate) == 0x0c, "ENE_SPE_OBJ_DAT.rate");
static_assert(offsetof(ENE_SPE_OBJ_DAT, ofs) == 0x10, "ENE_SPE_OBJ_DAT.ofs");
static_assert(offsetof(ENE_SPE_OBJ_DAT, loop) == 0x1c, "ENE_SPE_OBJ_DAT.loop");
static_assert(offsetof(ENE_SPE_OBJ_DAT, time) == 0x20, "ENE_SPE_OBJ_DAT.time");
static_assert(offsetof(ENE_SPE_OBJ_DAT, move) == 0x24, "ENE_SPE_OBJ_DAT.move");
static_assert(offsetof(ENE_SPE_OBJ_DAT, speed) == 0x28, "ENE_SPE_OBJ_DAT.speed");
static_assert(offsetof(ENE_SPE_OBJ_DAT, angle) == 0x2c, "ENE_SPE_OBJ_DAT.angle");
static_assert(offsetof(ENE_SPE_OBJ_DAT, fade) == 0x30, "ENE_SPE_OBJ_DAT.fade");
static_assert(offsetof(ENE_SPE_OBJ_DAT, far_dist) == 0x34, "ENE_SPE_OBJ_DAT.far_dist");
static_assert(offsetof(ENE_SPE_OBJ_DAT, min_vol) == 0x38, "ENE_SPE_OBJ_DAT.min_vol");
static_assert(offsetof(ENE_SPE_OBJ_DAT, near_dist) == 0x3c, "ENE_SPE_OBJ_DAT.near_dist");
static_assert(offsetof(ENE_SPE_OBJ_DAT, delay) == 0x40, "ENE_SPE_OBJ_DAT.delay");
static_assert(offsetof(ENE_SPE_OBJ_DAT, side) == 0x44, "ENE_SPE_OBJ_DAT.side");
static_assert(offsetof(ENE_SPE_OBJ_DAT, started) == 0x45, "ENE_SPE_OBJ_DAT.started");

// One special SE (operator new(0x50) on the EE).
// layout: host -- dat is a pointer (EE 4 bytes at +0x08) and s3s holds
// three; the EE offsets are in the comments. Nothing outside this file
// reads it.
class ENE_SPE_OBJ                       /* 0x50 on the EE */
{
public:
    ENE_SPE_OBJ();                      // FF3 0x001987c8 (EneSpeObjCtor; C2 twin 0x00198770 EneSpeObjCtorBase)

    int  Start(ENE_SPE_OBJ_DAT *d);     // FF3 0x00198820: -1 when already active
    void Stop(int fade);                // FF3 0x00198938
    void SePlay();                      // FF3 0x001989e8
    int  Ctrl(MB_OUT_SECTION **list, int num);  // FF3 0x00198a38: -1 when it ended this frame

    /* 0x00 */ int              se;     // SndSePlay id, -1
    /* 0x04 */ int              active;
    /* 0x08 */ ENE_SPE_OBJ_DAT *dat;
    /* 0x0c */ float            delay;  // >= 0: frames until SePlay
    /* 0x10 */ sceVu0FVECTOR    vel;    // added to pos each frame of move, w 0
    /* 0x20 */ sceVu0FVECTOR    pos;    // the source, w 1
    /* 0x30 */ float            time;   // frames left, -1 endless, 0 over
    /* 0x34 */ float            move;   // frames of movement left
    /* 0x38 */ float            step;   // 1 (one frame)
    /* 0x3c */ SND_3D_SET       s3s;    // s3s.pos = pos
};

// The table (bss 0x3e0068, 0x30 on the EE).
// layout: host -- obj holds pointers; on and ready keep +0x00 / +0x04.
class ENE_SPE_OBJ_TBL
{
public:
    ENE_SPE_OBJ_TBL();                  // FF3 0x00199108 (EneSpeObjTblStaticInit, from _GLOBAL__I_ 0x00199130)

    void Start();                       // FF3 0x00198d98: on, ten new objects
    void Release();                     // FF3 0x00198e28: stops and frees them
    void StopAll();                     // FF3 0x00198ed0: stops them, keeps them
    int  Main();                        // FF3 0x00198f58: once the SE banks are loaded
    int  CanStart(ENE_SPE_OBJ_DAT *d);  // FF3 0x00198c98
    ENE_SPE_OBJ *GetFree();             // FF3 0x00198d50: null when all are active

    /* 0x00 */ int          on;
    /* 0x04 */ int          ready;      // 1 once SYS_REAL_1 / 2 / 3 are loaded
    /* 0x08 */ ENE_SPE_OBJ *obj[ENE_SPE_OBJ_NUM];
};
static_assert(offsetof(ENE_SPE_OBJ_TBL, ready) == 0x04, "ENE_SPE_OBJ_TBL.ready");

ENE_SPE_OBJ_TBL *GetEneSpeObjTbl(void); // FF3 0x00198d88

#endif /* _INGAME_ENEMY_ENE_SPE_OBJ_H */
