/* ==========================================================================
 *  ingame/enemy/enemy.h
 *
 *  Enemy setup and release (FF3 0x192470..0x198980; FF2 enemy.c's
 *  SetCommonDat, EnemyStart, Jibaku/AutoEnemyStart, EneRelease, EneReleaseReq,
 *  enemyIsReadyData), with FF3's own pieces around them: the per-id flag
 *  tables (saved), the effects an enemy carries, its motion script (shutter
 *  chance / fatal frame windows, SEs and turns at anime frames) and the enemy
 *  sound work. The enemy work itself and the main loop are in ene_main.h.
 * ======================================================================== */
#ifndef _INGAME_ENEMY_ENEMY_H
#define _INGAME_ENEMY_ENEMY_H

#include "ingame/enemy/ene_main.h"      // ENE_WRK, ENE_CMN

// ── The per-id flag tables (bss 0x3df7d0 / 0x3dfae0 / 0x3dfbc0) ──────────
// One word per enemy data row and type; saved with the game (save blocks
// EneIdFlgSaveGet0..2). Bits 4 and 8 are set at release (EneRelease), 0xf000
// survive a restart (EneIdFlgReset).
#define ENE_ID_FLG0_NUM 0x186
#define ENE_ID_FLG1_NUM 0x70
#define ENE_ID_FLG2_NUM 0xe7
extern u_short ene_id_flg0[ENE_ID_FLG0_NUM];    // bss 0x3df7d0: type 0
extern u_short ene_id_flg1[ENE_ID_FLG1_NUM];    // bss 0x3dfae0: type 1
extern u_short ene_id_flg2[ENE_ID_FLG2_NUM];    // bss 0x3dfbc0: type 2

// ── The motion script's frame rows (bss 0x3dfd90, one per slot) ──────────
// EneMotScrMain compares the anime frame with last frame's to open and
// close the script's windows.
struct ENE_MOT_SCR_WRK
{
    /* 0x00 */ short anime;             // the anime the script was set for (EneMotScrSet, EneMotScrWinMain), -1: none
    /* 0x02 */ short frame_old;         // last frame's anime frame, -1 after the anime wrapped
    /* 0x04 */ short frame;             // this frame's (EneGetAnimeTime)
    /* 0x06 */ short time;              // counts the frames the anime frame changed on
    /* 0x08 */ short time_old;          // last frame's
};
static_assert(sizeof(ENE_MOT_SCR_WRK) == 10, "ENE_MOT_SCR_WRK size");
static_assert(offsetof(ENE_MOT_SCR_WRK, time_old) == 8, "ENE_MOT_SCR_WRK.time_old");
extern ENE_MOT_SCR_WRK ene_mot_scr_wrk[ENE_WRK_MAX];    // bss 0x3dfd90

// ── The enemy sound work (bss 0x3dfe30, 0x234; EneSndInit) ───────────────
// Three parts: every bound enemy's looping SEs (its dat->se, on its own
// bank; EneSeLoopMain), the voice stream of the nearest singing enemy
// (EneStreamNearMain) and the event stream the tattoo event starts
// (YKN_18_IREZUMI3A; EneEvStreamMain). Volumes and pitches follow the
// player's distance and view (EneSndDistRate, EneSndCamRate).
struct ENE_SND_SLOT                     // one per ENE_WRK slot (by alg.idx)
{
    /* 0x00 */ float  vol;              // 0..1, eased by 0.01 a frame to EneSndDistRate(20, 140) (EneSeLoopUpdate)
    /* 0x04 */ float  rate;             // the same to EneSndCamRate(20, 140, 180)
    /* 0x08 */ int    state;            // EneSeLoopMain: 0 off, 1 load the bank (EneSndStart), 2 start the SEs, 3 update, 4 stop
    /* 0x0c */ int    bank;             // 1: its SE bank was entered; EneSndRelease releases it
    /* 0x10 */ int    se[3];            // play ids of its ENE_DAT.se, -1: none
};
static_assert(sizeof(ENE_SND_SLOT) == 0x1c, "ENE_SND_SLOT size");
static_assert(offsetof(ENE_SND_SLOT, state) == 0x08, "ENE_SND_SLOT.state");
static_assert(offsetof(ENE_SND_SLOT, se) == 0x10, "ENE_SND_SLOT.se");

// layout: host -- near_ene (+0x1c8) and ev_ene (+0x1e8) are EE pointers;
// offsets up to +0x1c8 match the ROM (asserted), the later ones are the EE
// offsets in the comments (asserted as lower bounds).
struct ENE_SND_WRK                      /* 0x234 on the EE */
{
    /* 0x000 */ ENE_SND_SLOT slot[ENE_WRK_MAX];
    /* 0x1c0 */ float    vol;           // the streams' volume rate, eased to EneSndDistRate (EneStreamVolUpdate)
    /* 0x1c4 */ float    rate;          // and their pitch rate, eased to EneSndCamRate
    /* 0x1c8 */ ENE_WRK *near_ene;      // the enemy the near stream follows, nullptr: none
    /* 0x1cc */ int      near_time;     // 30 while near_ene is the nearest; counted down in state 1
    /* 0x1d0 */ int      near_state;    // EneStreamNearMain: 0 idle, 1 another enemy is nearer, 2 / 3 fade out, 4 start, 5 play, 6 stop
    /* 0x1d4 */ int      near_stream;   // its StreamAuto id, -1: none (EneStreamIsActive)
    /* 0x1d8 */ int      near_file;     // the nearest enemy's stream file (cmn->adpcm_no; YKN_19_IREZUMI2B while TattooEvGetToggle)
    /* 0x1dc */ int      near_file_old; // the file playing
    /* 0x1e0 */ int      ev_state;      // EneEvStreamMain: 7 idle (EneSndInit, EneEvStreamReset), 8 preloading, 9 playing
    /* 0x1e4 */ int      ev_stream;     // its StreamAuto id, -1: none (EneStreamFadeOut)
    /* 0x1e8 */ ENE_WRK *ev_ene;        // the enemy it plays at (EneEvStreamPreload)
    /* 0x1ec */ int      se[16];        // SE play ids whose pitch follows the slow mode (EneSeEntry), -1: free
    /* 0x22c */ float    pitch;         // 1, 0.7 in the slow mode (EneSndSlowSet): times 4096 the SE pitch
    /* 0x230 */ float    pitch_loop;    // 1, 0.4 in the slow mode: times the loops' and the streams' pitch
};
static_assert(offsetof(ENE_SND_WRK, vol) == 0x1c0, "ENE_SND_WRK.vol");
static_assert(offsetof(ENE_SND_WRK, near_ene) == 0x1c8, "ENE_SND_WRK.near_ene");
static_assert(offsetof(ENE_SND_WRK, near_state) >= 0x1d0, "ENE_SND_WRK.near_state (EE 0x1d0)");
static_assert(offsetof(ENE_SND_WRK, ev_state) >= 0x1e0, "ENE_SND_WRK.ev_state (EE 0x1e0)");
static_assert(offsetof(ENE_SND_WRK, se) >= 0x1ec, "ENE_SND_WRK.se (EE 0x1ec)");
static_assert(offsetof(ENE_SND_WRK, pitch_loop) >= 0x230, "ENE_SND_WRK.pitch_loop (EE 0x230)");
static_assert(sizeof(ENE_SND_WRK) >= 0x234, "ENE_SND_WRK size (EE 0x234, EneSndInit's memset)");
extern ENE_SND_WRK ene_snd_wrk;         // bss 0x3dfe30

// Two points / a segment in the XZ plane (y is the world z); FF2's
// utility.c types, kept in FF3's copy of the helpers here.
struct POINT_T
{
    /* 0x0 */ float x;
    /* 0x4 */ float y;
};
static_assert(sizeof(POINT_T) == 8, "POINT_T size");

struct LINE_T
{
    /* 0x0 */ POINT_T a;
    /* 0x8 */ POINT_T b;
};
static_assert(sizeof(LINE_T) == 0x10, "LINE_T size");
static_assert(offsetof(LINE_T, b) == 8, "LINE_T.b");

// ── ROM tables ────────────────────────────────────────────────────────────
// The enemy aura (rodata 0x354d70, 9 x 0xb4; EneAuraDatGet), drawn by
// effect_ene.c's EneAuraFrameBegin / EneAuraFrameEnd / EneAuraDraw: the model
// goes into a work frame, which is copied back over the screen once per
// layer, shifted by a wobble, in the layer's GS ALPHA blend.
struct ENE_AURA_LAYER                   /* 0x2c */
{
    /* 0x00 */ int   alpha_a;           // GS ALPHA A / B / C / D selectors and FIX
    /* 0x04 */ int   alpha_b;
    /* 0x08 */ int   alpha_c;
    /* 0x0c */ int   alpha_d;
    /* 0x10 */ int   alpha_fix;
    /* 0x14 */ int   gray;              // the copy's R = G = B (128: unchanged)
    /* 0x18 */ float alpha;             // times the enemy's alpha
    /* 0x1c */ float amp_x;             // wobble, pixels
    /* 0x20 */ float amp_y;
    /* 0x24 */ float freq_x;            // degrees per time unit
    /* 0x28 */ float freq_y;
};
static_assert(sizeof(ENE_AURA_LAYER) == 0x2c, "ENE_AURA_LAYER size");
static_assert(offsetof(ENE_AURA_LAYER, gray) == 0x14, "ENE_AURA_LAYER.gray");
static_assert(offsetof(ENE_AURA_LAYER, amp_x) == 0x1c, "ENE_AURA_LAYER.amp_x");
static_assert(offsetof(ENE_AURA_LAYER, freq_y) == 0x28, "ENE_AURA_LAYER.freq_y");

struct ENE_AURA_DAT
{
    /* 0x00 */ u_char on;               // non-zero: EnemyDrawOne draws the aura
    /* 0x01 */ u_char num;              // layers drawn (EneAuraDrawLayers)
    /* 0x02 */ u_char mix;              // non-zero: the screen is mixed into the work frame (EneAuraScreenMix)
    /* 0x03 */ u_char mix_alpha;        // and at this alpha
    /* 0x04 */ ENE_AURA_LAYER layer[4];
};
static_assert(sizeof(ENE_AURA_DAT) == 0xb4, "ENE_AURA_DAT size");
static_assert(offsetof(ENE_AURA_DAT, layer) == 0x4, "ENE_AURA_DAT.layer");
#define ENE_AURA_DAT_NUM 9

// One action of a route node (flag 0x1000 | no, EneAlgGetCur; the enemy
// script's route op, 0x18dc20): turn, anime, wait and SE (rodata 0x321c60,
// one row in the ROM; the route lists follow it).
struct ENE_ALG_ACT
{
    /* 0x00 */ float rot;               // the facing it takes (ENE_WRK.rot[1])
    /* 0x04 */ int   anime;             // EneAnimeSet's anime
    /* 0x08 */ int   wait;              // seconds (x 30 frames, 25 in PAL); 0: jump at once
    /* 0x0c */ int   se;                // bank << 16 | se, <= 0: none
};
static_assert(sizeof(ENE_ALG_ACT) == 0x10, "ENE_ALG_ACT size");

// The enemy data tables GetEneDatP indexes (FF3 .data, also read by
// EneDatGetAdpcmNo; generated by tools/reverse/enedat.py into ene_dat.inc).
extern u_char ene_dat_tbl0[];           // data 0x301610: type 0, 0xa8-byte rows (ENE_DAT)
extern u_char ene_dat_tbl1[];           // data 0x311600: type 1, 0xa8-byte rows (ENE_DAT)
extern u_char ene_dat_tbl2[];           // data 0x315f80: type 2, 0x60-byte rows (AENE_DAT)

// 25 pointers to route lists (data 0x323ff0..0x324054 -> 0x321c70..0x323ff0;
// EneAlgTblSet, ENE_DAT.alg_no up to 24), from ene_dat.inc.
extern ENE_ALG_NODE *ene_alg_tbl[25];

// ── Functions ─────────────────────────────────────────────────────────────
void  EnemyWrkClear(void);              // the per-id flag tables (EnemyMainInit, IngameNewGameReset)
void  EneIdFlgReset(u_char type, u_short id);
u_short EneIdFlgCheck(u_char type, u_short id, u_short mask);
void  EneIdFlgSet(u_char type, u_short id, u_short mask);
int   EneIdFlgSaveGet0(void **addr);    // save blocks {ene_id_flg0..2, size}
int   EneIdFlgSaveGet1(void **addr);
int   EneIdFlgSaveGet2(void **addr);
int   EneDatGetAdpcmNo(u_char type, int no);    // -1 for other types
short EneDatGetMdlId(u_char type, int no);

void  EneEffInit(ENE_WRK *ene);
void  EneEffRelease(ENE_WRK *ene);
void  EneEffPosUpdate(ENE_WRK *ene);    // EnemyActOne, before EneBlinkPosSet
void  EneEffAlphaUpdate(ENE_WRK *ene);
ENE_AURA_DAT *EneAuraDatGet(ENE_WRK *ene, u_char no);  // no 0xff: the enemy's own (cmn->aura)
void  SetEneMahiMode(ENE_WRK *ene, float a, float b, float c);  // FF3 0x00193d98: the paralysis' look (EneMahi*Req)
void  EneMahiModeDefault(ENE_WRK *ene); // EneMahiModeSelect: no paralysis runs
void  EneTurnReq(ENE_WRK *ene, u_char mode);

int   EneLoadStart(u_char type, short id);  // loads it unless it has a slot, marks it to start (_4dd) and starts a ready one; 0: no player or slot
int   EneLoadReq(u_char type, short id);    // loads it unless it has a slot; 0: no player or slot

void  SetCommonDat(ENE_WRK *ene, ENE_CMN *cmn, int hp);
void  EnemyStart(int no);               // EnemyWrkOne: an enemy that acts as soon as it is loaded
int   enemyIsReadyData(ENE_WRK *ene);   // EnemyWrkOne, loading: non-zero once its data is ready
void  EnemyReleaseAll(void);            // the 16 enemy slots in use
void  EneRelease(int no, int force);    // EnemyWrkOne, release: (slot, 0)
void  EneReleaseReq(u_char type, short id);
void  EneReleaseKind5f(void);           // PlyrCrawlEndReq / PlyrCrawlCancel
void  EneMdlCtrlSet(float a, float b, ENE_WRK *ene, u_char mode);
int   CheckEneView(float rot, float dist, float ang, float *pos, float *tgt);   // non-zero: tgt is inside the cone of half-angle ang (degrees, < 90) and length dist from pos facing rot
void  EneObjEffSetRate(float v, ENE_WRK *ene);    // kinds 0x1f / 0x20: their object effects' +0x38

int   LineSide(POINT_T *p, LINE_T *e);              // 1 / -1 / 0: which side of e p is on
int   EneLineStraddle(LINE_T *e, POINT_T *a, POINT_T *b);  // a and b not on the same side of e
int   LineIntersect(LINE_T *e1, LINE_T *e2);

void  EneAlgNext(ENE_WRK *ene);        // the route's next node (wraps)
void  EneAlgNearest(ENE_WRK *ene);     // the route's node nearest the enemy
u_int EneAlgGetCur(ENE_WRK *ene);      // the current node's flag word, 0: no enemy

void  EneMotScrClear(ENE_WRK *ene);
void  EneMotScrMain(ENE_WRK *ene);      // EnemyActOne, unless the player work's _234
void  EneMdlWatchUpdate(ENE_WRK *ene);  // EnemyMotionWorkOne, after the model's motion step: where the model looks
int   EneMotScrSet(ENE_WRK *ene, u_char anime, u_char sub);    // EneAnimeSet: the script of the anime; 0: none
void  EneMotScrSetFrame(ENE_WRK *ene, u_char anime, u_char sub, short frame);  // the same, run at frame 0 and counted up to frame
u_char EneMotScrAttrGet(ENE_WRK *ene, u_short anime, u_short sub);  // the script's attr, 0: none (enemy script op 0x18aaa0)
int   EneViewPlyrCheck(float dist, float ang, ENE_WRK *ene);   // the player's head is in the cone of its joint 2's facing
ENE_ALG_ACT *EneAlgActGet(int no);      // ene_alg_act_tbl[no]

void  EneSndInit(void);                 // EnemyMainInit, after EnemyWrkClear
void  EneSndMain(void);                 // EnemyMain, after the slots and HUD targets
int   EneStreamIsActive(void);          // the near or the event stream is set
void  EneSndSlowSet(u_char slow);        // PlyrGaugeSlowStart / End, PlyrMdlCreate
void  EneSePlay(ENE_WRK *ene, u_int se);
// FF3 0x001979b0: one of four SEs by percentage, or none (EJobC38)
void  EneSePlayRand(ENE_WRK *ene, int se0, int p0, int se1, int p1, int se2, int p2, int se3, int p3);
void  EneStreamFadeOut(int time);       // PlyrDeathCtrl step 9: 3
void  EneEvStreamPreload(ENE_WRK *ene); // the tattoo event (TattooEvStart): YKN_18_IREZUMI3A at ene
void  EneEvStreamPlay(void);            // starts it (TattooEvCameraMain)
void  EneEvStreamReset(void);           // back to idle (TattooEvCameraMain)
void  EneStreamNearStop(void);          // fades the near stream out (TattooEvCameraMain)

#endif /* _INGAME_ENEMY_ENEMY_H */
