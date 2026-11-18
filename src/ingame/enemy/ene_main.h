/* ==========================================================================
 *  ingame/enemy/ene_main.h
 *
 *  The enemy main loop and the enemy work (FF3 0x186a88..0x18a338; FF2 keeps
 *  this in enemy.c: EnemyMain, EnemyWrkOne, SearchEneWrkNo, IsActEnemy,
 *  ShutterChanceChk). FF3-only pieces: the three paralysis ("mahi") timers,
 *  the linked enemies (data link ids), the attack reactions that set the
 *  player's damage, and the photo HUD targets (filament, hp gauge).
 * ======================================================================== */
#ifndef _INGAME_ENEMY_ENE_MAIN_H
#define _INGAME_ENEMY_ENE_MAIN_H

#include <stddef.h>                     // offsetof
#include "sdk/scetypes.h"
#include "ingame/photo/fndr_parts.h"    // DAMAGE_DISP, ENE_GAUGE
#include "graphics/effect/effect_sub.h" // PP_JUDGE
#include "ingame/enemy/random_ghost.h"  // RAND_GHOST_INFO

class GameModel;                        // the model object root (ingame/ingame.h)
struct PLYR_MDL_FX;                     // ingame/plyr/plyr_sub.h
struct CHAR_WATCH_PRM;                  // ingame/ingame.h
struct PLYR_WRK;                        // ingame/plyr/player.h
struct FLY_MDL_WRK;                     // ingame/enemy/fly_ctrl.h

// An enemy's data row: the ROM tables GetEneDatP indexes. Rows of the
// bound ("jibaku") enemies, types 0 and 1 (0xa8 bytes, data 0x301610 /
// 0x311600; ENE_WRK.dat), and of the auto enemies, type 2 (0x60 bytes,
// 0x315f80; ENE_WRK.adat), start with the same head (ENE_CMN, ENE_WRK.cmn
// points at every row). Only what is read; no pointers, so the host layout is
// the ROM's.
struct ENE_DAT
{
    /* 0x00 */ u_char  _00[0x50];       // the ENE_CMN head
    /* 0x50 */ float   hold_dist;       // times 0.2: how far in front of the player a holding enemy stands (EneAttackHold)
    /* 0x54 */ float   regen;           // hp per frame once regeneration runs (EneHpRegen)
    /* 0x58 */ u_char  _58[4];
    /* 0x5c */ int     se[3];           // -1: none; EneSndRelease stops the slot's SEs for the ones set
    /* 0x68 */ int     regen_wait;      // seconds without damage before hp regenerates
    /* 0x6c */ short   fly[3];          // its fly types (FLY_DAT rows, FlyAct), -1: none
    /* 0x72 */ short   link[8];         // ids of the linked enemies (same type), -1 / negative: none
    /* 0x82 */ u_short hp;              // times the difficulty's rate: hp_max (JibakuEnemyStart)
    /* 0x84 */ u_short hp_mul;          // the start hp is hp_max times this
    /* 0x86 */ u_char  _86[2];
    /* 0x88 */ int     atk_dmg[5];      // hit_dmg choices (motion script ops 0xd / 0x10)
    /* 0x9c */ u_short atk9;            // the damage of a dmg_type 9 attack (EneAttackReq / EneAttackHit)
    /* 0x9e */ u_short atk;             // hold damage per tick, times PlyrGaugeDmgRate's rate
    /* 0xa0 */ s_char  near_pri;        // 0..2: the order EneStreamNearEneGet tries singing enemies in
    /* 0xa1 */ s_char  alg_no;          // EneAlgTblSet's table row, < 0: none
    /* 0xa2 */ u_char  _a2[2];
    /* 0xa4 */ u_char  hold_time;       // seconds of a hold (EneAttackHold)
    /* 0xa5 */ u_char  _a5[3];
};
static_assert(sizeof(ENE_DAT) == 0xa8, "ENE_DAT size (the 0x301610 rows)");
static_assert(offsetof(ENE_DAT, hold_dist) == 0x50, "ENE_DAT.hold_dist");
static_assert(offsetof(ENE_DAT, regen) == 0x54, "ENE_DAT.regen");
static_assert(offsetof(ENE_DAT, se) == 0x5c, "ENE_DAT.se");
static_assert(offsetof(ENE_DAT, regen_wait) == 0x68, "ENE_DAT.regen_wait");
static_assert(offsetof(ENE_DAT, link) == 0x72, "ENE_DAT.link");
static_assert(offsetof(ENE_DAT, hp) == 0x82, "ENE_DAT.hp");
static_assert(offsetof(ENE_DAT, hp_mul) == 0x84, "ENE_DAT.hp_mul");
static_assert(offsetof(ENE_DAT, atk_dmg) == 0x88, "ENE_DAT.atk_dmg");
static_assert(offsetof(ENE_DAT, atk9) == 0x9c, "ENE_DAT.atk9");
static_assert(offsetof(ENE_DAT, atk) == 0x9e, "ENE_DAT.atk");
static_assert(offsetof(ENE_DAT, near_pri) == 0xa0, "ENE_DAT.near_pri");
static_assert(offsetof(ENE_DAT, alg_no) == 0xa1, "ENE_DAT.alg_no");
static_assert(offsetof(ENE_DAT, hold_time) == 0xa4, "ENE_DAT.hold_time");

// An auto enemy's (type 2) data row, 0x60 bytes at data 0x315f80
// (ENE_WRK.adat). Only what is read.
struct AENE_DAT
{
    /* 0x00 */ u_char  _00[0x50];       // the ENE_CMN head
    /* 0x50 */ u_short voice_wait;      // frames before its voice plays (ENE_WRK.voice_wait); non-zero: the stream is preloaded, else played at once (EneSndStart)
    /* 0x52 */ u_char  _52[2];
    /* 0x54 */ float   near_dist;       // times 0.2: EJobB1D jumps once the player is this near (XZ); <= 0: never
    /* 0x58 */ short   act_len;        // frames; EJobC27 sets the script timer to what is left after its operand, negative: none
    /* 0x5a */ u_char  _5a[6];
};
static_assert(sizeof(AENE_DAT) == 0x60, "AENE_DAT size (the 0x315f80 rows)");
static_assert(offsetof(AENE_DAT, voice_wait) == 0x50, "AENE_DAT.voice_wait");
static_assert(offsetof(AENE_DAT, near_dist) == 0x54, "AENE_DAT.near_dist");
static_assert(offsetof(AENE_DAT, act_len) == 0x58, "AENE_DAT.act_len");

// The head every enemy data row starts with (ENE_WRK.cmn, +0x1f4). Only what
// is read; no pointers, so the host layout is the ROM's.
struct ENE_CMN
{
    /* 0x00 */ int     adpcm_no;        // its voice stream, -1: none (FF2 cmn_dat->adpcm_no; EneSndStart, EneStreamReset)
    /* 0x04 */ float   pos[3];          // start position (SetCommonDat; [1] the height); with flags2 0x20000000 / 0x40000000
                                       // an offset from the linked enemy, turned by its rot (SetCommonDat, EnemyActOne)
    /* 0x10 */ int     se_bank;         // its SE bank file, -1: none (EneSePlay plays bank << 16 | se)
    /* 0x14 */ short   id;              // model id (EneDatGetMdlId; 0x1c: the tegata, 0x1e: drawn with its link (EnemyDrawOne); 0x1a: plyr_sub);
                                       // < 0x2f: a model is created (EneWrkCreate); FUN_00145090's chr
    /* 0x16 */ short   mdl_type;        // FUN_00145090's type; < 0x20 indexes the motion-script and watch tables (EneMotScrSet)
    /* 0x18 */ u_short mdl_file;        // the model's files (FUN_00145090; EneMdlCreate)
    /* 0x1a */ u_short mot_file;
    /* 0x1c */ u_short shp_file;
    /* 0x1e */ u_short kind;            // 0x60..0x66 get their own reach in PlyrEneTouchCheck; 0x1f / 0x20 the object effects (EneIsKind1f); 0x5f (EneReleaseKind5f)
    /* 0x20 */ u_short score;           // photo points: type 2 scores it once, else the base of the damage score (PlyrShotScoreEne)
    /* 0x22 */ u_short rot;             // facing in degrees at the start; 1000: towards the player (SetCommonDat)
    /* 0x24 */ u_char  mdl_ctrl;        // EneMdlCtrlSet's mode at the start of an auto enemy
    /* 0x25 */ u_char  _25[3];
    /* 0x28 */ u_int   flags2;          // the start value of ENE_WRK.flags2 (0x194798)
    /* 0x2c */ u_char  _2c[4];
    /* 0x30 */ float   fade_near;       // times 0.2: the distance it is fully visible from (EneAlphaCtrl)
    /* 0x34 */ float   fade_far;        // times 0.2: the distance it has faded out at
    /* 0x38 */ u_char  _38[4];
    /* 0x3c */ short   soul_no;         // its soul-list entry, -1: none; photographed: SoulListSetHave and a subject (PlyrShotSubjectsEne)
    /* 0x3e */ short   soul_sp_no;      // a second entry a fatal-frame hit unlocks (FF2 SUBJECT_WRK.sp_no), -1: none
    /* 0x40 */ u_char  aura;            // its row of the aura table (EneAuraDatGet with 0xff)
    /* 0x41 */ u_char  _41[2];
    /* 0x43 */ u_char  deform_scale[2]; // per cent: the two deform effects' scales (EneDeformInit)
    /* 0x45 */ u_char  _45[2];
    /* 0x47 */ u_char  eff[9];          // [0]: its row of ene_eff_dat; 0xff: these 9 bytes are the row (EneEffInit)
};
static_assert(sizeof(ENE_CMN) == 0x50, "ENE_CMN size (the rows' head)");
static_assert(offsetof(ENE_CMN, pos) == 0x04, "ENE_CMN.pos");
static_assert(offsetof(ENE_CMN, se_bank) == 0x10, "ENE_CMN.se_bank");
static_assert(offsetof(ENE_CMN, id) == 0x14, "ENE_CMN.id");
static_assert(offsetof(ENE_CMN, mdl_type) == 0x16, "ENE_CMN.mdl_type");
static_assert(offsetof(ENE_CMN, shp_file) == 0x1c, "ENE_CMN.shp_file");
static_assert(offsetof(ENE_CMN, kind) == 0x1e, "ENE_CMN.kind");
static_assert(offsetof(ENE_CMN, score) == 0x20, "ENE_CMN.score");
static_assert(offsetof(ENE_CMN, rot) == 0x22, "ENE_CMN.rot");
static_assert(offsetof(ENE_CMN, mdl_ctrl) == 0x24, "ENE_CMN.mdl_ctrl");
static_assert(offsetof(ENE_CMN, flags2) == 0x28, "ENE_CMN.flags2");
static_assert(offsetof(ENE_CMN, fade_near) == 0x30, "ENE_CMN.fade_near");
static_assert(offsetof(ENE_CMN, fade_far) == 0x34, "ENE_CMN.fade_far");
static_assert(offsetof(ENE_CMN, soul_no) == 0x3c, "ENE_CMN.soul_no");
static_assert(offsetof(ENE_CMN, soul_sp_no) == 0x3e, "ENE_CMN.soul_sp_no");
static_assert(offsetof(ENE_CMN, aura) == 0x40, "ENE_CMN.aura");
static_assert(offsetof(ENE_CMN, deform_scale) == 0x43, "ENE_CMN.deform_scale");
static_assert(offsetof(ENE_CMN, eff) == 0x47, "ENE_CMN.eff");

// An enemy's effect row: 9 bytes, the ROM table ene_eff_dat (data 0x31b620)
// or ENE_CMN.eff itself (EneEffInit).
struct ENE_EFF_DAT
{
    /* 0x00 */ u_char  no;              // the row number (0xff in ENE_CMN.eff)
    /* 0x01 */ u_char  mahi[3];         // tenths: the paralysis look without a paralysis (EneMahiModeDefault)
    /* 0x04 */ u_char  alpha;           // its row of ene_alpha_dat (EneAlphaInit)
    /* 0x05 */ u_char  light;           // non-zero: the light colour's low byte (EneLightInit)
    /* 0x06 */ u_char  eff;             // its row of ene_eff_prm (EneEffStart), 0: none
    /* 0x07 */ u_char  deform[2];       // its rows of ene_deform_dat[0] / [1] (EneDeformInit), 0: none
};
static_assert(sizeof(ENE_EFF_DAT) == 9, "ENE_EFF_DAT size");
static_assert(offsetof(ENE_EFF_DAT, deform) == 7, "ENE_EFF_DAT.deform");

// The effects an enemy carries (ENE_WRK.eff, +0x240; EneEffInit, EneEffRelease).
// layout: host -- the effect handles are EE pointers (4 bytes); EE offsets in
// the comments.
struct ENE_EFF                          /* 0x70 on the EE */
{
    /* 0x00 */ void          *deform[2];        // CallPartsDeform5_2 effects, nullptr: none (EneDeformInit)
    /* 0x08 */ float          deform_alpha[2];  // their alpha: rate * base * ENE_WRK._578, 0 while hidden (EneEffAlphaUpdate)
    /* 0x10 */ float          rate[2];          // 0 at the start; copied to and from the linked enemies (EnemyActOne)
    /* 0x18 */ float          base[2];          // 1 (EneDeformInit)
    /* 0x20 */ float          deform_b1[2];     // the deform rows' byte [1]
    /* 0x28 */ u_char         _28[8];
    /* 0x30 */ sceVu0FVECTOR  pos;              // 14 from joint 1 towards the camera: where the deforms sit (EneEffPosUpdate)
    /* 0x40 */ void          *light;            // an effect EneEffAlphaUpdate drives through light_pow; nothing in this file starts it
    /* 0x44 */ float          light_pow;        // deform 0's alpha / 128, +0.025 with alpha_a, else -0.025 (EneEffAlphaUpdate)
    /* 0x48 */ float          light_range;      // 10000 (EneLightInit)
    /* 0x4c */ u_int          light_col;        // ENE_EFF_DAT.light | 0x40404500 (EneLightInit)
    /* 0x50 */ u_char         _50[0x18];
    /* 0x68 */ void          *obj[2];           // EffKind1DReq effects of kinds 0x1f / 0x20 (EneObjEffInit)
};
static_assert(offsetof(ENE_EFF, pos) >= 0x30, "ENE_EFF.pos (EE +0x30)");
static_assert(offsetof(ENE_EFF, obj) >= 0x68, "ENE_EFF.obj (EE +0x68)");

// One command of an enemy's motion script (ENE_WRK.mot_scr; EneMotScrOp):
// 9 shorts, op 0x1d ends the list.
struct ENE_MOT_CMD
{
    /* 0x00 */ u_short op;
    /* 0x02 */ short   start;           // the anime frame (op 2: the count of frame changes) the window opens at
    /* 0x04 */ short   end;             // where it closes (scaled from start by the player's and the difficulty's rates)
    /* 0x06 */ short   arg[6];
};
static_assert(sizeof(ENE_MOT_CMD) == 0x12, "ENE_MOT_CMD size");
static_assert(offsetof(ENE_MOT_CMD, arg) == 6, "ENE_MOT_CMD.arg");

// The shutter-chance and fatal-frame windows of a script, by the enemy's
// level (ENE_WRK.level: 1..4, clamped; EneMotScrWinMain): anime frames from
// start to end, scaled by the player's and the difficulty's rates and, from
// level 4 on, by rate once per level above 3.
struct ENE_MOT_WIN
{
    /* 0x00 */ int   sc[4][2];          // shutter chance {start, end}
    /* 0x20 */ int   ff[4][2];          // fatal frame {start, end}
    /* 0x40 */ float rate;
};
static_assert(sizeof(ENE_MOT_WIN) == 0x44, "ENE_MOT_WIN size");
static_assert(offsetof(ENE_MOT_WIN, ff) == 0x20, "ENE_MOT_WIN.ff");
static_assert(offsetof(ENE_MOT_WIN, rate) == 0x40, "ENE_MOT_WIN.rate");

// One motion script (ENE_WRK.mot_scr): the script of an anime and sub
// number (the ROM tables in enemy_mot_dat.inc, EneMotScrSet).
// layout: host -- cmd and win are EE pointers (0x10 bytes on the EE); the
// later fields' EE offsets are asserted as lower bounds.
struct ENE_MOT_SCR
{
    /* 0x00 */ ENE_MOT_CMD *cmd;        // the commands, op 0x1d ends them (EneMotScrMain)
    /* 0x04 */ ENE_MOT_WIN *win;        // nullptr: no level windows (EneMotScrWinMain)
    /* 0x08 */ u_char  attr;            // 0..2 (EneMotScrAttrGet)
    /* 0x09 */ u_char  _09;
    /* 0x0a */ u_char  mdl_ctrl;        // EneMdlCtrlSet's mode while it plays
    /* 0x0b */ u_char  _0b;
    /* 0x0c */ short   mdl_ctrl_a;      // and its values, in degrees
    /* 0x0e */ short   mdl_ctrl_b;
};
static_assert(offsetof(ENE_MOT_SCR, attr) >= 0x08, "ENE_MOT_SCR.attr (EE 0x08)");
static_assert(offsetof(ENE_MOT_SCR, mdl_ctrl) >= 0x0a, "ENE_MOT_SCR.mdl_ctrl (EE 0x0a)");
static_assert(offsetof(ENE_MOT_SCR, mdl_ctrl_b) >= 0x0e, "ENE_MOT_SCR.mdl_ctrl_b (EE 0x0e)");

// One node of an enemy's route (ENE_WRK.route; the ene_alg_tbl lists, data
// 0x321c70..): 0x20 bytes, a node whose flag has 0x8000 ends the list
// (EneAlgNext, EneAlgNearest).
struct ENE_ALG_NODE
{
    /* 0x00 */ u_int         flag;      // EneAlgGetCur returns it; 0x8000: the end of the list
    /* 0x04 */ u_char        _04[0xc];
    /* 0x10 */ sceVu0FVECTOR pos;       // EneAlgNearest measures to it
};
static_assert(sizeof(ENE_ALG_NODE) == 0x20, "ENE_ALG_NODE size (EneAlgNext's step)");
static_assert(offsetof(ENE_ALG_NODE, pos) == 0x10, "ENE_ALG_NODE.pos");

// An enemy's script cursor (FF2 ENEALG_WRK, rearranged in FF3): ENE_WRK +0xa0,
// handed to the action-script handlers (enemy_act.c). The scripts are byte
// streams in the ALG pak (ENE_ACT01_OBJ, loaded at EE 0x12ef000 by Boot_Load);
// jumps are 16-bit offsets from comm_add_top. The blink handlers take the
// ENE_WRK and use bcomm_add / bwait_time here and bjob_no / bpos_no after it.
// layout: host -- the five script pointers sit in 8-byte EE slots (sd / ld),
// so the host's 8-byte pointers keep every offset up to pos_no; stack_p (EE
// 4 bytes + pad) also fits. cur is a 4-byte EE pointer: from it on the EE
// offsets in the comments are lower bounds.
struct ENEALG_WRK                       /* 0xd0 on the EE */
{
    /* 0x00 */ u_char  *comm_add_top;   // the script base: EE 0x12ef000 (EneActSet)
    /* 0x08 */ u_char  *stack_b[16];    // return addresses of EJobC09 (call); EJobC0A pops
    /* 0x88 */ u_char **stack_p;        // the next free stack_b slot (&stack_b[0] at EneActSet)
    /* 0x90 */ u_char  *bcomm_add_top;  // the blink script's base (EneBlinkSet)
    /* 0x98 */ u_char  *bcomm_add;      // the blink script's cursor
    /* 0xa0 */ u_char  *comm_add;       // the action script's cursor
    /* 0xa8 */ float    wait_time;      // counted down by ENE_WRK.step; <= 0 runs opcodes, 255 stops the run
    /* 0xac */ float    loop[3];        // the scripts' timers (EJobC05 / EJobC06), in frames over GetFrameTimeScale
    /* 0xb8 */ float    bwait_time;     // the blink script's wait
    /* 0xbc */ u_char   _bc[2];
    /* 0xbe */ u_char   cnt[2];         // the scripts' byte counters (EJobC2A sets, EJobC2B adds)
    /* 0xc0 */ u_char   idx;            // the enemy's slot (= ENE_WRK index; EneMarkSlotInit)
    /* 0xc1 */ u_char   job_no;         // the opcode running
    /* 0xc2 */ u_char   pos_no;         // its handler's step, 0: fetch the next opcode
    /* 0xc3 */ u_char   _c3;
    /* 0xc4 */ u_char  *cur;            // the address of the opcode running (EneAlgCtrl)
    /* 0xc8 */ u_char   var[8];         // the scripts' variables (EJobC23 / EJobC24 set them, also on the linked enemies; EJobC44 picks one at random)
};
static_assert(offsetof(ENEALG_WRK, stack_b) == 0x08, "ENEALG_WRK.stack_b");
static_assert(offsetof(ENEALG_WRK, stack_p) == 0x88, "ENEALG_WRK.stack_p");
static_assert(offsetof(ENEALG_WRK, bcomm_add_top) == 0x90, "ENEALG_WRK.bcomm_add_top");
static_assert(offsetof(ENEALG_WRK, bcomm_add) == 0x98, "ENEALG_WRK.bcomm_add");
static_assert(offsetof(ENEALG_WRK, comm_add) == 0xa0, "ENEALG_WRK.comm_add");
static_assert(offsetof(ENEALG_WRK, wait_time) == 0xa8, "ENEALG_WRK.wait_time");
static_assert(offsetof(ENEALG_WRK, loop) == 0xac, "ENEALG_WRK.loop");
static_assert(offsetof(ENEALG_WRK, bwait_time) == 0xb8, "ENEALG_WRK.bwait_time");
static_assert(offsetof(ENEALG_WRK, idx) == 0xc0, "ENEALG_WRK.idx");
static_assert(offsetof(ENEALG_WRK, job_no) == 0xc1, "ENEALG_WRK.job_no");
static_assert(offsetof(ENEALG_WRK, pos_no) == 0xc2, "ENEALG_WRK.pos_no");
static_assert(offsetof(ENEALG_WRK, cnt) == 0xbe, "ENEALG_WRK.cnt");
static_assert(offsetof(ENEALG_WRK, cur) >= 0xc4, "ENEALG_WRK.cur (EE 0xc4)");
static_assert(offsetof(ENEALG_WRK, var) >= 0xc8, "ENEALG_WRK.var (EE 0xc8)");
static_assert(sizeof(ENEALG_WRK) >= 0xd0, "ENEALG_WRK (EE 0xd0 bytes)");

// ENE_WRK.state (+0x4cc), EnemyWrkOne's switch.
enum ENE_STATE
{
    ENE_STATE_FREE    = 0,              // slot unused (GetEneWrkBuffer)
    ENE_STATE_LOADING = 1,              // waits for its data (0x1949c8)
    ENE_STATE_READY   = 2,              // loaded, not acting
    ENE_STATE_ACT     = 3,              // the rule runs; the camera can target it
    ENE_STATE_RELEASE = 4,              // freed by 0x194c48
};

// One enemy: 16 x 0x700 at data 0x2fa3a0 (EnemyMainInit clears 0x7000).
// layout: host -- the script cursor's pointers (alg), dat (+0x1e8), adat, target, cmn, eff, eff_dat, rg's row and zone,
// tpos (+0x50c), mot_scr, mdl and fx (+0x6c0, +0x6c4), alg are EE pointers;
// offsets up to alg.pos_no (+0x162) match the ROM (asserted), the later ones
// are the EE offsets in the comments (asserted as lower bounds).
struct ENE_WRK                          /* 0x700 on the EE */
{
    /* 0x000 */ float       mpos[4];    // joint 2 (EneJointPosUpdate): the enemy position for hits (LensHitEffectReq); [1] the bottom of its height range
    /* 0x010 */ float       jpos1[4];   // joint 1: where its effects start (IgEffectEffectEndParticleReq)
    /* 0x020 */ float       jpos13[4];  // joint 0x13
    /* 0x030 */ float       jpos7[4];   // joint 7
    /* 0x040 */ float       jpos_ex[6][4];  // six more joint points: EfGetMpos ids 4..9 (effect_ene.c; ids 0..3 are mpos .. jpos7)
    /* 0x0a0 */ ENEALG_WRK  alg;        // its action script's cursor (EneActSet, EneAlgCtrl) and the blink script's (EneBlinkSet, EneBlinkCtrl)
    /* 0x170 */ u_char      _170[2];
    /* 0x172 */ u_char      act_req;    // the action an event asks for next (EneActReqById); 0 at the start (SetCommonDat)
    /* 0x173 */ u_char      bjob_no;    // the blink script's opcode (EneBlinkCtrl)
    /* 0x174 */ u_char      bpos_no;    // its handler's step, 0: fetch the next opcode
    /* 0x175 */ u_char      follow;     // 1: turns to the player and follows its height (EneMoveCtrl); EneTurnReq sets it
    /* 0x176 */ u_char      _176[2];
    /* 0x178 */ float       rot_keep;   // a facing kept by EJobC31 (rot[1]) for EJobM08 to restore
    /* 0x17c */ u_char      _17c[4];
    /* 0x180 */ u_long      flags;      // 1 / 2 / 4: paralysis 0..2 running; 0x80: on screen (EneMarkWork); 0x100: in view (EneBlinkPosSet);
                                       // 0x800 / 0x1000: hit in a shutter chance / fatal frame (LensHitEffectReq);
                                       // 0x200 in the viewfinder frame, 0x400 near its centre, 0x40 / 0x80 targetable (PlyrFinderEneCheck rebuilds 0x6d0 each frame);
                                       // 0x4000: attacking; 0x20000: damage pending (EneHpCtrl); 0x100000: not a target (NearEneInfo, PlyrVibCheck);
                                       // 0x200000: not drawn (EnemyDrawOne); 0x800000000: dead / down (no vertical move);
                                       // 0x4000000000: its voice stream may play (EneStreamNearEneGet)
    /* 0x188 */ u_long      flags_old;  // last frame's flags (EnemyActOne); ShutterChanceChk reads it
    /* 0x190 */ u_char      _190[0x18];
    /* 0x1a8 */ int         hp;         // 0: no player checks (FF2 st.hp)
    /* 0x1ac */ u_char      _1ac[4];
    /* 0x1b0 */ int         hp_max;
    /* 0x1b4 */ int         dmg;        // damage waiting for EneDamageApply
    /* 0x1b8 */ u_char      _1b8[8];
    /* 0x1c0 */ int         dmg_last;   // the damage EneDamageApply took
    /* 0x1c4 */ u_short     dmg_react;  // how the last photo hit lands (EneDmgReactSet): 0 none, 2 a plain hit, 3 a strong one, 4 a shutter-chance
                                       // hit that slows it, 5 its follow-up; > 1 knocks it down (EneDamageApply, reaction 3); 3..5 run the level windows
    /* 0x1c6 */ u_char      _1c6[0xa];
    /* 0x1d0 */ float       _1d0;       // 1 at the start (SetCommonDat); EJobC39 sets it to a percentage / 100
    /* 0x1d4 */ u_char      _1d4[0x14];
    /* 0x1e8 */ ENE_DAT     *dat;       // its data (types 0 and 1, nullptr for 2); dat->atk is the hold damage (PlyrDamageCtrl)
    /* 0x1ec */ AENE_DAT    *adat;      // an auto enemy's (type 2) data, else nullptr (0x194798)
    /* 0x1f0 */ PLYR_WRK    *target;    // the player it attacks (FF2 target; SetCommonDat: the current player)
    /* 0x1f4 */ ENE_CMN     *cmn;       // the head of its data row (dat or adat); cmn->id selects the slot in IngameEffSlotMdlReset
    /* 0x1f8 */ FLY_MDL_WRK *fly[3][5]; // its flies, 5 per dat fly type (FlyAct; EneFlyModelInitWait, EneFlyAnmctrlRelease)
    /* 0x234 */ u_char      _234[0xc];
    /* 0x240 */ ENE_EFF     eff;        // its effects (EneEffInit)
    /* 0x2b0 */ ENE_EFF_DAT *eff_dat;   // its effect row (EneEffInit)
    /* 0x2b4 */ RAND_GHOST_INFO rg;     // type 1 (random ghosts): its row, kind, area and zone (RandGhostSelect / RandGhostInfoCopy)
    /* 0x2d4 */ u_char      _2d4[0xc];
    /* 0x2e0 */ sceVu0FVECTOR _2e0;     // cleared at the start (SetCommonDat)
    /* 0x2f0 */ sceVu0FVECTOR ofs;      // a linked enemy's offset from its parent's pos; [1]: the height over the player when it holds them
    /* 0x300 */ u_char      _300[0x10];
    /* 0x310 */ sceVu0FVECTOR pos;      // the body position the player measures to (FF2 mbox.pos); pos[1] is the top of its height range
    /* 0x320 */ sceVu0FVECTOR pos_old;  // last frame's pos (EneMoveCtrl)
    /* 0x330 */ sceVu0FVECTOR move_last;// the move added to pos this frame
    /* 0x340 */ sceVu0FVECTOR move_old; // the frame before's
    /* 0x350 */ sceVu0FVECTOR rot;      // rot[1]: facing (PlyrEneTouchCheck rotates its reach by it)
    /* 0x360 */ u_char      _360[0x10];
    /* 0x370 */ sceVu0FVECTOR _370;     // EJobM0C's circling step (a tenth added to pos); cleared with turn when an action starts (EneActSet)
    /* 0x380 */ sceVu0FVECTOR turn;     // per frame, times the step, added to rot (EneMoveCtrl)
    /* 0x390 */ float       rot_trgt[3];// [1]: the facing it turns to at turn_spd
    /* 0x39c */ float       turn_spd;   // radians per frame, times the step; 0: no turn
    /* 0x3a0 */ u_char      _3a0[0x30];
    /* 0x3d0 */ sceVu0FVECTOR move;     // this frame's motion move (EneMotMoveGet), turned by rot; cleared after the move
    /* 0x3e0 */ sceVu0FVECTOR rot_add;  // a one-frame turn: added to rot instead of turning to rot_trgt when [1] != 0; cleared
    /* 0x3f0 */ u_char      _3f0[0x10];
    // The occlusion probe (EneBlinkPosSet fills it, EneBlinkCapture runs
    // CheckPointDepth): num 1, p[0] = tpos moved 8 towards the camera;
    // result[0] non-zero when nothing covers it (PlyrFinderEneCheck's 0x40
    // mark at any distance; EneBlinkPosSet clears it each frame).
    /* 0x400 */ PP_JUDGE    blink;
    /* 0x4b0 */ u_char      blink_req;  // EneBlinkCapture runs CheckPointDepth once and clears it
    /* 0x4b1 */ u_char      _4b1[3];
    /* 0x4b4 */ float       dist;       // distance to the player; PhotoModeDrawNearEnemy picks the smallest (< 10000) in state 3
    /* 0x4b8 */ float       dist_old;   // last frame's
    /* 0x4bc */ float       fdr_dist;   // distance on screen from the viewfinder centre (PlyrFinderEneCheck, flag 0x400)
    /* 0x4c0 */ u_int       regen_cnt;  // frames without damage (EneHpRegen)
    /* 0x4c4 */ float       regen_acc;  // hp regenerated, not yet added
    /* 0x4c8 */ u_char      type;       // 2: a special enemy (FF2 ene_wrk.type); < 2 ordinary
    /* 0x4c9 */ u_char      _4c9[3];
    /* 0x4cc */ int         state;      // ENE_STATE
    /* 0x4d0 */ u_int       flags2;     // 1: fades by distance (EneAlphaCtrl); 2: shown in the finder though invisible; 4: map hit clamp;
                                       // 8: no height follow; 0x10: regenerates; 0x40: plain draw; 0x80: touches the player (PlyrEneTouchCheck);
                                       // 0x100: no gauge / HUD target; 0x400: not a filament target; 0x800: no camera marker (EneMarkWork, EneMarkDraw);
                                       // 0x1000: not a HUD target; 0x2000: no heartbeat rumble (FF2 attr 0x80, PlyrVibCheck); 0x8000: hidden while _528 is 0;
                                       // 0x20000 / 0x40000 / 0x100000 / 0x40000000: linked-enemy copies (EnemyActOne); 0x80000: paralysis shared with the links;
                                       // 0x400000 / 0x800000: drawn by the second draw pass / motion stepped there (EnemyDrawOne)
    /* 0x4d4 */ float       hold_pow;   // hold strength: escape presses wear it down; <= 0 frees the player (PlyrDamageCtrl)
    /* 0x4d8 */ u_short     atk_type;   // the dmg_type of its last attack (EneAttackReq)
    /* 0x4da */ short       id;         // its id; SearchEneWrkNo matches it with type
    /* 0x4dc */ u_char      act_no;     // its script action (EneActSet), 0xff: none (EJobC03); 1, 2 and 6 react to the flash (PlyrFlashEneReact4/5);
                                       // 3: a hit action, no hit hook (EneDamageApply)
    /* 0x4dd */ s_char      _4dd;       // 1: acts as soon as it is loaded (EnemyWrkOne)
    /* 0x4de */ u_char      _4de[2];
    /* 0x4e0 */ float       _4e0[4];    // the paralysis look (SetEneMahiMode, EneMahiModeDefault): [0..2], [3] its counter (0);
                                       // copied to the linked enemies with the paralysis (EneMahiCopy)
    /* 0x4f0 */ u_char      _4f0[0xc];
    /* 0x4fc */ float       scr_x;      // screen position the camera marker moves to
    /* 0x500 */ float       scr_y;
    /* 0x504 */ short       mark_x;     // the marker's position, written back by EneMarkWork
    /* 0x506 */ short       mark_y;
    /* 0x508 */ u_char      _508[2];
    /* 0x50a */ s_char      area;       // the player's area number (EneMapHitCtrl), -1: none
    /* 0x50b */ u_char      _50b;
    /* 0x50c */ float       *tpos;      // the point the camera aims at (FrameInsideChk); a changed pointer restarts the marker's move
    /* 0x510 */ int         hit_dmg;    // damage of its attacks (EneAttackReq); dmg_type 6's when its timer runs out (PlyrDamageCtrl)
    /* 0x514 */ short       mahi_time[3];   // paralysis frames left (EneMahi0..2Req / Ctrl)
    /* 0x51a */ u_char      _51a[2];
    /* 0x51c */ u_int       mahi_start[3];  // the system frame counter when each began; the latest wins (EneMahiModeSelect)
    /* 0x528 */ short       _528;       // 1000 from a flash (PlyrFlashEneSet528); counted down (EneMahiMain)
    /* 0x52a */ u_char      _52a[2];
    /* 0x52c */ float       mahi1_rate; // paralysis 1's rate (EneMahi1Req)
    /* 0x530 */ u_char      _530[4];
    /* 0x534 */ float       voice_wait; // an auto enemy's adat->voice_wait (EneSndStart), counted down by the step; at 0 its preloaded voice plays (EneAutoStreamMain)
    /* 0x538 */ int         stream_id;  // its voice stream (EneAutoStreamPreload / Play); 0 (EneWrkInit), -1 at release (EneStreamReset)
    /* 0x53c */ int         se_id;      // an SE it plays while acting; stopped when the anime changes (EneSeStopCheck)
    /* 0x540 */ u_int       se_anime;   // the anime (_554) that SE belongs to
    /* 0x544 */ u_char      _544[4];
    /* 0x548 */ float       _548;       // 1 at the start (SetCommonDat)
    /* 0x54c */ float       aura_time;  // +2 per drawn frame (EnemyDrawOne): the aura's phase
    /* 0x550 */ u_char      _550[4];
    /* 0x554 */ u_char      _554;       // its anime (EneAnimeSet); 0xff: no model to draw or step
    /* 0x555 */ u_char      _555;       // and its sub number
    /* 0x556 */ u_char      mot_kind;   // < 2: the motion moves it (EneMotMoveGet); frames since the anime ended (EneAnimeFlagUpdate)
    /* 0x557 */ u_char      _557;       // 0: the model's yaw callback turns it (EneYawTurnFunc); EneAnimeSet clears it
    /* 0x558 */ u_short     mot_attr;   // the anime attr EneMotMoveGet passes
    /* 0x55a */ u_char      _55a[2];
    /* 0x55c */ short       step_time;  // EneSetStepReq's frames
    /* 0x55e */ u_char      _55e[2];
    /* 0x560 */ float       step;       // its time step: motion, turns, timers (and the held player's, PlyrGaugeTimeStep)
    /* 0x564 */ float       step_req;   // EneSetStepReq's rate
    /* 0x568 */ u_char      _568[4];
    /* 0x56c */ float       _56c;       // a third step factor, 1 every frame (EneStepUpdate)
    /* 0x570 */ float       mahi_rate;  // the running paralysis' rate, 1: none (EneMahiModeSelect)
    /* 0x574 */ float       alpha_rate; // EneAlphaCtrl's product, 0..1
    /* 0x578 */ float       _578;       // 1 every frame (EneAlphaCtrl)
    /* 0x57c */ u_char      alpha;      // 0..128 (alpha_rate * 128); non-zero: visible, PlyrEneTouchCheck may fire
    /* 0x57d */ u_char      alpha_fdr;  // alpha factors, 128 = 1: in the viewfinder,
    /* 0x57e */ u_char      alpha_nrm;  //   outside it,
    /* 0x57f */ u_char      alpha_a;    //   and two more (shared with the links)
    /* 0x580 */ u_char      alpha_b;
    /* 0x581 */ u_char      _581;       // = alpha_b at the start (EneAlphaInit)
    /* 0x582 */ u_char      _582;       // ene_alpha_dat [1], [2] (EneAlphaInit)
    /* 0x583 */ u_char      _583;
    /* 0x584 */ short       _584;       // a random 0..359 at the start (EneAlphaInit)
    /* 0x586 */ u_char      _586[2];
    /* 0x588 */ s_char      level;      // -1 (EneWrkInit); 1..4 pick the script's level windows, higher ones shrink them (EneMotScrWinMain)
    /* 0x589 */ u_char      _589;       // 0xff (SetCommonDat); EJobC3B: the hit voice by dmg_react (0..2, -1: none) it plays with EneSePlay
    /* 0x58a */ u_char      _58a[2];
    /* 0x58c */ ENE_MOT_SCR *mot_scr;   // its motion script; nullptr: none (EneMotScrSet, EneMotScrMain)
    /* 0x590 */ DAMAGE_DISP dmg_disp;   // the damage number (plyr_mdl.c, PhotoModeDrawNearEnemy)
    /* 0x5a4 */ ENE_GAUGE   gauge;      // EneGaugeInit / EneGaugeWork
    /* 0x5bc */ u_char      _5bc[0xec];
    /* 0x6a8 */ float       _6a8;       // motion script ops 0xd / 0x10 inside their window: arg[0] * 0.2,
    /* 0x6ac */ float       _6ac;       //   and arg[1] degrees in radians; 0 outside
    /* 0x6b0 */ u_char      mdl_ctrl;   // EneMdlCtrlSet's mode and values
    /* 0x6b1 */ u_char      _6b1[3];
    /* 0x6b4 */ float       mdl_ctrl_a;
    /* 0x6b8 */ float       mdl_ctrl_b;
    /* 0x6bc */ CHAR_WATCH_PRM *watch;  // its model's watch parameters (ene_watch_tbl, EneMotScrSet); nullptr: the defaults by mdl_ctrl
    /* 0x6c0 */ GameModel   *mdl;       // a CharEnemyModel (vtable 0x341690)
    /* 0x6c4 */ PLYR_MDL_FX *fx;        // its fade envelope (PlyrMdlFxMain)
    /* 0x6c8 */ u_char      _6c8[8];
    /* 0x6d0 */ ENE_ALG_NODE *route[3]; // its route: [0] the list (ene_alg_tbl row, EneAlgTblSet), [1] the current node, [2] the one before (EneAlgNext)
    /* 0x6dc */ u_char      _6dc[4];
    /* 0x6e0 */ sceVu0FVECTOR turn_pos; // EneTurnReq 0xfe turns to it
    /* 0x6f0 */ float       _6f0;       // counted down by the step (EneRule); EJobC30 sets it (frames, not PAL-scaled)
    /* 0x6f4 */ u_char      _6f4[0xc];
};
static_assert(offsetof(ENE_WRK, jpos1) == 0x010, "ENE_WRK.jpos1");
static_assert(offsetof(ENE_WRK, jpos7) == 0x030, "ENE_WRK.jpos7");
static_assert(offsetof(ENE_WRK, jpos_ex) == 0x040, "ENE_WRK.jpos_ex");
static_assert(offsetof(ENE_WRK, follow) >= 0x175, "ENE_WRK.follow (EE 0x175)");
static_assert(offsetof(ENE_WRK, rot_keep) >= 0x178, "ENE_WRK.rot_keep (EE 0x178)");
static_assert(offsetof(ENE_WRK, flags) >= 0x180, "ENE_WRK.flags (EE 0x180)");
static_assert(offsetof(ENE_WRK, flags_old) >= 0x188, "ENE_WRK.flags_old (EE 0x188)");
static_assert(offsetof(ENE_WRK, hp) >= 0x1a8, "ENE_WRK.hp (EE 0x1a8)");
static_assert(offsetof(ENE_WRK, hp_max) >= 0x1b0, "ENE_WRK.hp_max (EE 0x1b0)");
static_assert(offsetof(ENE_WRK, dmg) >= 0x1b4, "ENE_WRK.dmg (EE 0x1b4)");
static_assert(offsetof(ENE_WRK, dmg_last) >= 0x1c0, "ENE_WRK.dmg_last (EE 0x1c0)");
static_assert(offsetof(ENE_WRK, dmg_react) >= 0x1c4, "ENE_WRK.dmg_react (EE 0x1c4)");
static_assert(offsetof(ENE_WRK, dat) >= 0x1e8, "ENE_WRK.dat (EE 0x1e8)");
static_assert(offsetof(ENE_WRK, alg) == 0x0a0, "ENE_WRK.alg");
static_assert(offsetof(ENE_WRK, act_req) >= 0x172, "ENE_WRK.act_req (EE 0x172)");
static_assert(offsetof(ENE_WRK, bpos_no) >= 0x174, "ENE_WRK.bpos_no (EE 0x174)");
static_assert(offsetof(ENE_WRK, cmn) >= 0x1f4, "ENE_WRK.cmn (EE 0x1f4)");
static_assert(offsetof(ENE_WRK, eff) >= 0x240, "ENE_WRK.eff (EE 0x240)");
static_assert(offsetof(ENE_WRK, rg) >= 0x2b4, "ENE_WRK.rg (EE 0x2b4)");
static_assert(offsetof(ENE_WRK, blink) >= 0x400, "ENE_WRK.blink (EE 0x400)");
static_assert(offsetof(ENE_WRK, mot_scr) >= 0x58c, "ENE_WRK.mot_scr (EE 0x58c)");
static_assert(offsetof(ENE_WRK, turn_pos) >= 0x6e0, "ENE_WRK.turn_pos (EE 0x6e0)");

#define ENE_WRK_MAX 16
extern ENE_WRK ene_wrk[ENE_WRK_MAX];    // data 0x2fa3a0

void      EnemyMainInit(void);          // clears ene_wrk, the random ghosts and the enemy data
void      EnemyMainLoadRestore(void);   // empty in FF3
void      EnemyMain(void);              // per frame: the 16 slots, the HUD targets, FlyRule
void      EnemyDrawOne(ENE_WRK *ene, u_int flags);  // flags 0x200: the second pass (+0x4d0 0xc00000 enemies)
void      EnemyMotionWorkOne(ENE_WRK *ene);

void      EneMahi0Req(ENE_WRK *ene, float frames);
void      EneMahi0End(ENE_WRK *ene, int fx);    // fx: the end particle and SE when visible
void      EneMahi1Req(ENE_WRK *ene, short frames, float rate);
void      EneMahi1End(ENE_WRK *ene, int fx);
void      EneMahi2Req(ENE_WRK *ene, float frames);
void      EneMahi2End(ENE_WRK *ene, int fx);
void      EneSetStepReq(ENE_WRK *ene, short frames, float rate);
void      EneMahiAllEnd(ENE_WRK *ene);  // ends all three on it and its links, no effects

int       SearchEneWrkNo(u_char type, short id);    // -1: none
ENE_WRK  *EneGetWrk(u_char type, short id);         // nullptr: none
int       GetEneWrkBuffer(void);        // a free slot, -1: none
void     *GetEneDatP(int type, int no); // the data row (0x301610 / 0x311600 / 0x315f80 tables)
int       IsActEnemy(int no);           // state ACT; the slot's camera marker shows (EneMarkDraw)
float     EneGetJointRot(ENE_WRK *ene, int joint, int axis);  // the joint's angle about axis
void      EneGetJointPos(ENE_WRK *ene, int joint, float *out, float *in);   // in (nullptr: origin) through the joint
s_char    EneGetMinLevel(void);
void      EneFlagReq(u_char mode, u_char mask); // mode 0 clears, 1..3 set flags on the live slots not in mask
int       ShutterChanceChk(void);       // 2 / 1: a shutter chance (flags_old 0x1080 / 0x880), 0: none
int       EneAttackReq(ENE_WRK *ene, u_short dmg_type, u_char kind); // the player is hit / held; 0: hit on the ladder
int       EneHpRateCeil(ENE_WRK *ene);  // ceil(hp / hp_max)
int       EneGetState(u_char type, short id);
int       EneGetPos(float *out, u_char type, short id);    // non-zero: found
int       EneIsAnyAlive(void);          // a slot in state ACT with hp != 0
ENE_WRK  *EneGetGaugeTarget(void);      // the enemy the hp gauge shows, nullptr: none
void      EneBlinkCapture(void);        // after the draw: CheckPointDepth for the requested probes

// The damage reaction, the anime and the time step (0x189b90..0x18a280).
int       EneAnyDmgReactBig(void);      // 1: an acting enemy with hp has dmg_react 3..5 (the photo HUD)
int       EneIsDmgReactBig(ENE_WRK *ene);   // dmg_react in 3..5 (EneMotScrWinMain runs only then)
void      EneLevelByDmgReact(ENE_WRK *ene); // level 1 after a plain reaction, + 1 after a shutter-chance one
void      EneDmgReactSet(ENE_WRK *ene, s_char shot);    // a photo hit of that shot kind: dmg_react, the chain flags
void      EneAnimeSet(ENE_WRK *ene, u_char anime, u_char start, u_char sub, u_char time, u_char skip,
                      u_char force, u_char attr, u_char blend); // starts the anime (and on the linked enemies)
void      EneYawTurnFunc(float rot, void *ene); // the model's motion callback (EnemyStart): a one-frame turn into rot_add[1]
void      EneStepUpdate(ENE_WRK *ene);  // per frame: step = the factors times the player's gauge rate
void      EneStepReset(ENE_WRK *ene, u_int flags);  // 2: the slow-down ends, 4: the paralyses end; the step recomputed
void      EneAnimeFlagUpdate(ENE_WRK *ene); // per frame: flags 0x40000000 / 0x20000000 from the anime script
int       EneGetAnimeTime(ENE_WRK *ene);    // the model's anime clock, truncated
int       EneGetAnimeLength(ENE_WRK *ene);  // the current anime's length in frames

#endif /* _INGAME_ENEMY_ENE_MAIN_H */
