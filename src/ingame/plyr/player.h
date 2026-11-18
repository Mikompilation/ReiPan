/* ==========================================================================
 *  ingame/plyr/player.h
 *
 *  The player characters (FF2 player.c). Minimal so far: what reconstructed
 *  code calls.
 * ======================================================================== */
#ifndef _INGAME_PLYR_PLAYER_H
#define _INGAME_PLYR_PLAYER_H

#include <stddef.h>                     // offsetof
#include "sdk/scetypes.h"

class GameModel;                          // the model object root (ingame/ingame.h)
struct LADDER_DAT;                        // ingame/event/prg/ev_ladder.h
struct PLYR_MDL_FX;                       // ingame/plyr/plyr_sub.h

// PlyrSet280Req's four requests (PLYR_WRK +0x280).
// The fade of AcsModel acs_mdl[no] (PlyrAcsFadeCtrl): from alpha val[0]
// (128 shown, else hidden), after val[1] frames, over val[3] frames to val[2].
struct PLYR_REQ280                     /* 0x18 */
{
    /* 0x00 */ int   state;             // 1 requested, 2 delay, 3 fade, 4 held at val[2]
    /* 0x04 */ float time;              // frames in the state (0 at the request)
    /* 0x08 */ float val[4];            // start alpha, delay, end alpha, fade frames
};
static_assert(sizeof(PLYR_REQ280) == 0x18, "PLYR_REQ280 is 0x18 bytes");
static_assert(offsetof(PLYR_REQ280, val) == 0x08, "PLYR_REQ280.val");

// One player work: 3 of them at data 0x32f4a0, 0x350 bytes apart
// (GetPlyrWrk); ingame_wrk.plyr_wrk points at the current one. Only the
// fields reconstructed code reads are named; the rest is padding until
// player.c is reconstructed. Field roles from IngameMain/IngameDrawSub,
// PlayerMain and their callees.
// layout: host -- +0x26c..+0x27c, +0x2e0 and +0x344 are EE pointers
// (PlyrMdlCreate 0x23eca8 and PlyrLadderStartBottom 0x23e208 store them);
// offsets before +0x26c match the ROM (asserted), the rest are the EE
// offsets in the comments.
struct PLYR_WRK                         /* 0x350 on the EE */
{
    /* 0x000 */ float         _000;         // turn time left, counted down by the timer step (PlyrTurnCtrl)
    /* 0x004 */ float         _004;         // turn length: PlyrTurnCtrl's divisor
    /* 0x008 */ u_char        _008[8];
    /* 0x010 */ sceVu0FVECTOR pos;          // position (action checks, map fog, SE)
    /* 0x020 */ sceVu0FVECTOR pos_old;      // the sound listener's velocity is pos_old - pos
    /* 0x030 */ sceVu0FVECTOR move_last;    // the move added to pos this frame (PlyrMoveUpdate)
    /* 0x040 */ sceVu0FVECTOR move_old;     // the frame before's (restored with pos_old on a direction object)
    /* 0x050 */ sceVu0FVECTOR rot;          // rot[1]: facing (the view checks; PlyrSetRot copies all four)
    /* 0x060 */ u_char        _060[0x20];
    /* 0x080 */ sceVu0FVECTOR view_turn;    // view turn: added to rot per frame for _000 frames (flags2 0x200000, FinderViewTurnStart:
                                            //   [1] = total / 10); with PLYR_FLG_VIEW_EASE [0] / [1] are the pitch / yaw accelerations
    /* 0x090 */ sceVu0FVECTOR view_from;    // PLYR_FLG_VIEW_EASE: [0] pitch, [1] yaw the eased turn starts from (FinderViewTurnCtrl)
    /* 0x0a0 */ sceVu0FVECTOR turn_from;    // rot when an event turn started (PlyrTurnStart); [1]: its angle (PlyrTurnCtrl)
    /* 0x0b0 */ float         _0b0;
    /* 0x0b4 */ float         _0b4;         // turn left: target - the model's angle (PlyrTurnCtrl)
    /* 0x0b8 */ u_char        _0b8[8];
    /* 0x0c0 */ sceVu0FVECTOR rot_target;   // [1]: the facing the move aims at (PlyrMoveDirCtrl, PlyrTurnStart)
    /* 0x0d0 */ u_long        flags;        // PLYR_FLG_*
    /* 0x0d8 */ u_long        flags_old;    // last frame's flags (PlyrStatusCtrl)
    /* 0x0e0 */ int           _0e0;         // bit 0: CameraSetFromRotPair views, bit 1: the cat's view (FinderViewCameraCtrl)
    /* 0x0e4 */ int           _0e4;         // last frame's _0e0
    /* 0x0e8 */ u_long        flags2;       // 0x80000: map camera changed (PlyrMapCamCheck); 0x200000: timed view turn (FinderViewCameraCtrl)
    /* 0x0f0 */ u_long        flags3;
    /* 0x0f8 */ int           hp;           // PhotoModeHudFadeMain: 0 < hp <= hp_max / 5 is low HP
    /* 0x0fc */ int           hp_old;       // last frame's hp
    /* 0x100 */ int           hp_max;
    /* 0x104 */ int           dmg;          // damage waiting for PlyrDamageCtrl (PlyrDamage)
    /* 0x108 */ int           _108;         // cleared with a full HP restore (PlyrClear108)
    /* 0x10c */ u_char        _10c[8];
    /* 0x114 */ short         dmg_type;     // damage reaction kind (PlyrDamageCtrl; 4 keeps the hold handlers' flags)
    /* 0x116 */ short         status;       // timed status (PlyrStatusCtrl): 1 shake-off, 2 fog, 4 held, 5 tegata
    /* 0x118 */ short         status_old;   // last frame's status
    /* 0x11a */ u_char        _11a[2];
    /* 0x11c */ float         status_time;  // frames left of status (0: status ends)
    /* 0x120 */ u_char        _120[4];
    /* 0x124 */ short         ene_wait;     // no enemy touches while non-zero; counted down outside modes 1..4
    /* 0x126 */ u_char        _126[2];
    /* 0x128 */ u_long        battle_cnt;   // frames with an ordinary enemy active (PlyrBattleCheck)
    /* 0x130 */ u_long        _130;         // cleared with battle_cnt
    /* 0x138 */ u_char        _138[8];
    /* 0x140 */ int           mh_no;        // the map hit object the player stands in (MhCtl FUN_0011f5f0; PlyrMapPosUpdate)
    /* 0x144 */ u_char        mode;         // PLYR_MODE; 5: no shadow-side draw (IngameSetDrawEnable)
    /* 0x145 */ u_char        mode_old;     // the mode before the last PlyrSetMode
    /* 0x146 */ u_char        ene_no;       // the enemy slot holding / hitting the player
    /* 0x147 */ u_char        _147;         // 13: PlyrGetDrawSkipFlags returns 0; indexes per-character anime tables
    /* 0x148 */ u_char        _148[4];
    /* 0x14c */ float         dmg_time;     // frames to the next hold damage tick (PlyrDamageCtrl, counted down by the timer step)
    /* 0x150 */ float         near_dist;    // nearest ordinary enemy (type < 2), 99999: none (NearEneInfo)
    /* 0x154 */ float         near_dist_old;
    /* 0x158 */ u_char        near_no;      // its slot, 0xff: none
    /* 0x159 */ u_char        _159[3];
    /* 0x15c */ float         near2_dist;   // nearest type-2 enemy, 99999: none
    /* 0x160 */ float         near2_dist_old;
    /* 0x164 */ u_char        near2_no;
    /* 0x165 */ u_char        _165;
    /* 0x166 */ short         floor;        // FF2 cmn_wrk.floor's role (MapFogProc, action checks)
    /* 0x168 */ u_char        _168[4];
    /* 0x16c */ int           danger_se;    // low-HP SE play id, 0: none (PlyrDangerSeCtrl)
    /* 0x170 */ sceVu0FVECTOR listen_pos;   // sound listener position when listening from the player
    /* 0x180 */ sceVu0FVECTOR _180;         // a point on the player: the ladder camera's target (PlyrLadderCamera)
    /* 0x190 */ u_char        anime_no;     // the anime CheckPlyrAnimeEnd waits for (0xff after a load)
    /* 0x191 */ u_char        anime_end_cnt;// frames since the model's anime ended (PlyrAnimeEndCheck)
    /* 0x192 */ u_char        anime_step;   // the model's walk step 0..3 (vtable +0x60, PlyrAnimeCtrl)
    /* 0x193 */ u_char        _193;         // an anime number PlyrMotMoveGet compares with anime_no (its writer not traced)
    /* 0x194 */ u_short       anime_attr;   // PlyrAnimeAttr of the last SetPlyrAnime
    /* 0x196 */ short         idle_time;    // random 90..210 frames (PlyrIdleTimeSet)
    // Escaping a hold (PlyrHoldEscapeCheck): two windows, each counting presses.
    /* 0x198 */ float         esc_time0;    // frames since the hold began (window 0)
    /* 0x19c */ short         esc_push0;    // presses in window 0
    /* 0x19e */ u_short       esc_start0;   // window 0: 14..20 frames in, 4..10 frames long (PlyrDamageCtrl mode 1)
    /* 0x1a0 */ u_short       esc_len0;
    /* 0x1a2 */ u_char        _1a2[2];
    /* 0x1a4 */ float         esc_time1;    // window 1 (flash presses)
    /* 0x1a8 */ short         esc_push1;
    /* 0x1aa */ u_short       esc_start1;   // 0 frames in, 30 frames long
    /* 0x1ac */ u_short       esc_len1;
    /* 0x1ae */ u_char        _1ae[2];
    /* 0x1b0 */ float         cam_dir;      // the camera's yaw towards the player (PlyrMoveDirCtrl)
    /* 0x1b4 */ u_char        _1b4[0xc];
    /* 0x1c0 */ sceVu0FVECTOR move;         // this frame's move: the motion's (PlyrMotMoveGet), turned by move_rot and rot (PlyrMoveUpdate)
    /* 0x1d0 */ sceVu0FVECTOR turn_cb;      // [1]: the model callback's turn (FUN_0023e7b0); added to rot, then cleared
    /* 0x1e0 */ sceVu0FVECTOR turn;         // [1]: radians per frame (PlyrTurnCtrl, PlyrCrawlMove); times the timer step, added to rot
    /* 0x1f0 */ sceVu0FVECTOR move_rot;     // [1]: the move's direction from the facing, 0 forward, pi back, 10: none (crawl and viewfinder)
    /* 0x200 */ float         ease_time;    // frames the facing eases from ease_from (10 by FUN_0023e7b0; PlyrMoveDirCtrl)
    /* 0x204 */ float         ease_dir;     // the target facing when it started
    /* 0x208 */ float         ease_from;    // the facing it eases from
    /* 0x20c */ float         _20c;         // counted down by 1 per frame (PlyrMoveDirCtrl); 4 frames after a camera change (PlyrMapCamChgClear)
    /* 0x210 */ float         camchg_time;  // set when the map camera changes (FUN_00243348), counts down (PlyrMapCamCheck)
    /* 0x214 */ float         _214;         // FUN_00241db0's value at that change
    /* 0x218 */ u_char        _218[8];
    /* 0x220 */ sceVu0FVECTOR hold_move;    // added to the holding enemy's position every hold frame (PlyrDamageCtrl)
    /* 0x230 */ u_char        equip[3];     // kept over a new game (PlyrWrkInitKeepEquip)
    /* 0x233 */ u_char        death_step;   // PlyrDeathCtrl's step (mode 7; PlyrDeathStart sets 1, 4 or 0x11); 11..15: PlyrGetDrawSkipFlags 0xbe; > 6: PlyrIsMode233Over6
    /* 0x234 */ u_char        _234;         // PlayerMain's result (non-zero)
    /* 0x235 */ u_char        _235[3];
    /* 0x238 */ float         finder_pitch; // the view's pitch in the viewfinder, -60..60 deg (PlyrFinderMoveCtrl)
    /* 0x23c */ float         finder_x;     // viewfinder point on screen (640x448); the camera HUD centres on it
    /* 0x240 */ float         finder_y;     //   (PlyrFinderPointMove: 302..338 x 209..239, drifts back to 320, 224)
    /* 0x244 */ float         hold_yaw;     // while held (status 4) the view turns within 25 deg of this
    /* 0x248 */ float         hold_pitch;   //   and pitches within -55..16 deg of this
    /* 0x24c */ float         crawl_h;      // the view's height over pos: the crawl pose's (PlyrCrawlStart / PlyrCrawlEnd), or the standing eye's
                                            //   (mid of joints 0x10 and 4, plus 0 / -1.5 / 1.2 by player) when the viewfinder starts (PlyrFinderModeStart)
    /* 0x250 */ short         _250;         // non-zero: the photo mode work skips its +0x220 main
    /* 0x252 */ u_char        _252[2];
    /* 0x254 */ float         _254;         // timer, counted down by the frame step (PlyrStatusCtrl)
    /* 0x258 */ int           sc_state_old; // last frame's sc_state (PlyrShotChargeCtrl)
    /* 0x25c */ int           sc_state;     // ShutterChanceChk while a target charges, else 0 (PlyrShotChargeCtrl: PhotoWrk180SetBit4)
    /* 0x260 */ u_char        _260;         // 2 at the end of a pause (PlyrSetB260Mode2)
    /* 0x261 */ u_char        _261[3];
    /* 0x264 */ float         _264;         // 4.8 after init and doors (PlyrSet264)
    /* 0x268 */ int           flash_num;    // flashes left; the HUD's NUM_flash (PlyrGetFlashNum reads the low half)
    /* 0x26c */ GameModel      *mdl;          // operator new 0x1c0 (ctor 0x145c50), PlyrMdlCreate
    /* 0x270 */ GameModel    *acs_mdl[4];   // AcsModels (0x28 bytes, ctor 0x121048): [0] A000_PK4 at joint 7, [1] A001 and [2] A002 at joint 0x13, [3] the accessory at joint 2 (PlyrMdlCreate / PlyrMdlSetup)
    /* 0x280 */ PLYR_REQ280   req280[4];    // PlyrSet280Req (door, flash recoil); reader not traced
    /* 0x2e0 */ PLYR_MDL_FX  *mdl_fx;       // the model's fade envelope (PlyrMdlFxReq; 0x14 bytes)
    /* 0x2e4 */ int           _2e4;         // non-zero: model in use (IngamePlyrMdlReset, PlayerMain)
    /* 0x2e8 */ u_char        _2e8;         // PlyrMdlCreate's 2nd argument; 2 for a finder-mode start
    /* 0x2e9 */ u_char        costume;      // the model table's row (PlyrGetCostumeMdlDat); kept over a new game (PlyrWrkInitKeepEquip)
    /* 0x2ea */ u_char        costume_sub;  // kind 2's variant in that row
    /* 0x2eb */ u_char        acs;          // accessory, 0: none (PlyrGetAcsModelNo); kept over a new game
    /* 0x2ec */ u_char        _2ec;         // PlyrMdlCreate's 3rd argument (0xff: keep): the motion file (0 real, 1 light: flags 0x8000); 1 for a finder-mode start
    /* 0x2ed */ u_char        _2ed;
    /* 0x2ee */ u_short       mdl_file;     // CD_FILE_DAT numbers PlyrMdlCreate passes to the model: model pk4,
    /* 0x2f0 */ u_short       mot_file;     //   motion pk4,
    /* 0x2f2 */ u_short       shp_file;     //   shape
    /* 0x2f4 */ u_char        _2f4[0x1c];
    /* 0x310 */ u_short       vib_time_sm;  // small-motor frames left (PlyrVibCtrl; FF2 vib_time_sm)
    /* 0x312 */ u_short       vib_time_bg;  // big-motor frames left (PlyrVibCtrlBig; FF2 vib_time_bg)
    /* 0x314 */ u_char        _314[0xc];
    /* 0x320 */ float         look_pitch;   // the right stick's look, radians -20..40 deg (PlyrLookCtrl); PlyrHasMove320: either non-zero
    /* 0x324 */ float         look_yaw;     // -30..30 deg
    /* 0x328 */ u_char        _328[8];
    /* 0x330 */ u_int         camdata;      // last map camera word with the event bits (PlyrMapCamCheck)
    /* 0x334 */ float         mapobj_dir;   // the last direction-object facing (PlyrMapDirAnimeCtrl)
    /* 0x338 */ float         _338;         // timer, counted down by the frame step (PlyrStatusCtrl)
    /* 0x33c */ float         mode10_time;  // mode 10: frames left, counted down by the timer step (PlyrMode10Ctrl)
    /* 0x340 */ u_char        ladder_step;  // steps climbed (mode 5, PlyrLadderCtrl)
    /* 0x341 */ u_char        ladder_top;   // the ladder's step count
    /* 0x342 */ u_char        _342[2];
    /* 0x344 */ LADDER_DAT   *ladder;       // the ladder being climbed (PlyrLadderStartBottom / Top)
    /* 0x348 */ u_char        _348;         // cleared when no enemy is active (PlyrBattleCheck)
    /* 0x349 */ u_char        _349[3];
    /* 0x34c */ int           crawl_se;     // the crawl loop's SE play id, -1: none (PlyrCrawlCtrl)
};
static_assert(offsetof(PLYR_WRK, _004) == 0x004, "PLYR_WRK._004");
static_assert(offsetof(PLYR_WRK, pos) == 0x010, "PLYR_WRK.pos");
static_assert(offsetof(PLYR_WRK, pos_old) == 0x020, "PLYR_WRK.pos_old");
static_assert(offsetof(PLYR_WRK, rot) == 0x050, "PLYR_WRK.rot");
static_assert(offsetof(PLYR_WRK, move_last) == 0x030, "PLYR_WRK.move_last");
static_assert(offsetof(PLYR_WRK, move_old) == 0x040, "PLYR_WRK.move_old");
static_assert(offsetof(PLYR_WRK, turn_from) == 0x0a0, "PLYR_WRK.turn_from");
static_assert(offsetof(PLYR_WRK, _0b4) == 0x0b4, "PLYR_WRK._0b4");
static_assert(offsetof(PLYR_WRK, rot_target) == 0x0c0, "PLYR_WRK.rot_target");
static_assert(offsetof(PLYR_WRK, flags) == 0x0d0, "PLYR_WRK.flags");
static_assert(offsetof(PLYR_WRK, flags_old) == 0x0d8, "PLYR_WRK.flags_old");
static_assert(offsetof(PLYR_WRK, view_turn) == 0x080, "PLYR_WRK.view_turn");
static_assert(offsetof(PLYR_WRK, view_from) == 0x090, "PLYR_WRK.view_from");
static_assert(offsetof(PLYR_WRK, _0e0) == 0x0e0, "PLYR_WRK._0e0");
static_assert(offsetof(PLYR_WRK, _0e4) == 0x0e4, "PLYR_WRK._0e4");
static_assert(offsetof(PLYR_WRK, flags2) == 0x0e8, "PLYR_WRK.flags2");
static_assert(offsetof(PLYR_WRK, flags3) == 0x0f0, "PLYR_WRK.flags3");
static_assert(offsetof(PLYR_WRK, hp) == 0x0f8, "PLYR_WRK.hp");
static_assert(offsetof(PLYR_WRK, hp_old) == 0x0fc, "PLYR_WRK.hp_old");
static_assert(offsetof(PLYR_WRK, hp_max) == 0x100, "PLYR_WRK.hp_max");
static_assert(offsetof(PLYR_WRK, dmg) == 0x104, "PLYR_WRK.dmg");
static_assert(offsetof(PLYR_WRK, _108) == 0x108, "PLYR_WRK._108");
static_assert(offsetof(PLYR_WRK, dmg_type) == 0x114, "PLYR_WRK.dmg_type");
static_assert(offsetof(PLYR_WRK, status) == 0x116, "PLYR_WRK.status");
static_assert(offsetof(PLYR_WRK, status_old) == 0x118, "PLYR_WRK.status_old");
static_assert(offsetof(PLYR_WRK, status_time) == 0x11c, "PLYR_WRK.status_time");
static_assert(offsetof(PLYR_WRK, ene_wait) == 0x124, "PLYR_WRK.ene_wait");
static_assert(offsetof(PLYR_WRK, battle_cnt) == 0x128, "PLYR_WRK.battle_cnt");
static_assert(offsetof(PLYR_WRK, _130) == 0x130, "PLYR_WRK._130");
static_assert(offsetof(PLYR_WRK, mh_no) == 0x140, "PLYR_WRK.mh_no");
static_assert(offsetof(PLYR_WRK, mode) == 0x144, "PLYR_WRK.mode");
static_assert(offsetof(PLYR_WRK, mode_old) == 0x145, "PLYR_WRK.mode_old");
static_assert(offsetof(PLYR_WRK, ene_no) == 0x146, "PLYR_WRK.ene_no");
static_assert(offsetof(PLYR_WRK, _147) == 0x147, "PLYR_WRK._147");
static_assert(offsetof(PLYR_WRK, dmg_time) == 0x14c, "PLYR_WRK.dmg_time");
static_assert(offsetof(PLYR_WRK, near_dist) == 0x150, "PLYR_WRK.near_dist");
static_assert(offsetof(PLYR_WRK, near_dist_old) == 0x154, "PLYR_WRK.near_dist_old");
static_assert(offsetof(PLYR_WRK, near_no) == 0x158, "PLYR_WRK.near_no");
static_assert(offsetof(PLYR_WRK, near2_dist) == 0x15c, "PLYR_WRK.near2_dist");
static_assert(offsetof(PLYR_WRK, near2_dist_old) == 0x160, "PLYR_WRK.near2_dist_old");
static_assert(offsetof(PLYR_WRK, near2_no) == 0x164, "PLYR_WRK.near2_no");
static_assert(offsetof(PLYR_WRK, floor) == 0x166, "PLYR_WRK.floor");
static_assert(offsetof(PLYR_WRK, danger_se) == 0x16c, "PLYR_WRK.danger_se");
static_assert(offsetof(PLYR_WRK, listen_pos) == 0x170, "PLYR_WRK.listen_pos");
static_assert(offsetof(PLYR_WRK, anime_no) == 0x190, "PLYR_WRK.anime_no");
static_assert(offsetof(PLYR_WRK, anime_end_cnt) == 0x191, "PLYR_WRK.anime_end_cnt");
static_assert(offsetof(PLYR_WRK, anime_step) == 0x192, "PLYR_WRK.anime_step");
static_assert(offsetof(PLYR_WRK, anime_attr) == 0x194, "PLYR_WRK.anime_attr");
static_assert(offsetof(PLYR_WRK, idle_time) == 0x196, "PLYR_WRK.idle_time");
static_assert(offsetof(PLYR_WRK, esc_time0) == 0x198, "PLYR_WRK.esc_time0");
static_assert(offsetof(PLYR_WRK, esc_push0) == 0x19c, "PLYR_WRK.esc_push0");
static_assert(offsetof(PLYR_WRK, esc_start0) == 0x19e, "PLYR_WRK.esc_start0");
static_assert(offsetof(PLYR_WRK, esc_len0) == 0x1a0, "PLYR_WRK.esc_len0");
static_assert(offsetof(PLYR_WRK, esc_time1) == 0x1a4, "PLYR_WRK.esc_time1");
static_assert(offsetof(PLYR_WRK, esc_push1) == 0x1a8, "PLYR_WRK.esc_push1");
static_assert(offsetof(PLYR_WRK, esc_start1) == 0x1aa, "PLYR_WRK.esc_start1");
static_assert(offsetof(PLYR_WRK, esc_len1) == 0x1ac, "PLYR_WRK.esc_len1");
static_assert(offsetof(PLYR_WRK, _180) == 0x180, "PLYR_WRK._180");
static_assert(offsetof(PLYR_WRK, _193) == 0x193, "PLYR_WRK._193");
static_assert(offsetof(PLYR_WRK, cam_dir) == 0x1b0, "PLYR_WRK.cam_dir");
static_assert(offsetof(PLYR_WRK, move) == 0x1c0, "PLYR_WRK.move");
static_assert(offsetof(PLYR_WRK, turn_cb) == 0x1d0, "PLYR_WRK.turn_cb");
static_assert(offsetof(PLYR_WRK, turn) == 0x1e0, "PLYR_WRK.turn");
static_assert(offsetof(PLYR_WRK, move_rot) == 0x1f0, "PLYR_WRK.move_rot");
static_assert(offsetof(PLYR_WRK, ease_time) == 0x200, "PLYR_WRK.ease_time");
static_assert(offsetof(PLYR_WRK, ease_from) == 0x208, "PLYR_WRK.ease_from");
static_assert(offsetof(PLYR_WRK, _20c) == 0x20c, "PLYR_WRK._20c");
static_assert(offsetof(PLYR_WRK, camchg_time) == 0x210, "PLYR_WRK.camchg_time");
static_assert(offsetof(PLYR_WRK, _214) == 0x214, "PLYR_WRK._214");
static_assert(offsetof(PLYR_WRK, hold_move) == 0x220, "PLYR_WRK.hold_move");
static_assert(offsetof(PLYR_WRK, equip) == 0x230, "PLYR_WRK.equip");
static_assert(offsetof(PLYR_WRK, death_step) == 0x233, "PLYR_WRK.death_step");
static_assert(offsetof(PLYR_WRK, _234) == 0x234, "PLYR_WRK._234");
static_assert(offsetof(PLYR_WRK, finder_pitch) == 0x238, "PLYR_WRK.finder_pitch");
static_assert(offsetof(PLYR_WRK, finder_x) == 0x23c, "PLYR_WRK.finder_x");
static_assert(offsetof(PLYR_WRK, hold_yaw) == 0x244, "PLYR_WRK.hold_yaw");
static_assert(offsetof(PLYR_WRK, hold_pitch) == 0x248, "PLYR_WRK.hold_pitch");
static_assert(offsetof(PLYR_WRK, finder_y) == 0x240, "PLYR_WRK.finder_y");
static_assert(offsetof(PLYR_WRK, _250) == 0x250, "PLYR_WRK._250");
static_assert(offsetof(PLYR_WRK, crawl_h) == 0x24c, "PLYR_WRK.crawl_h");
static_assert(offsetof(PLYR_WRK, _254) == 0x254, "PLYR_WRK._254");
static_assert(offsetof(PLYR_WRK, sc_state_old) == 0x258, "PLYR_WRK.sc_state_old");
static_assert(offsetof(PLYR_WRK, sc_state) == 0x25c, "PLYR_WRK.sc_state");
static_assert(offsetof(PLYR_WRK, _260) == 0x260, "PLYR_WRK._260");
static_assert(offsetof(PLYR_WRK, _264) == 0x264, "PLYR_WRK._264");
static_assert(offsetof(PLYR_WRK, flash_num) == 0x268, "PLYR_WRK.flash_num");
static_assert(offsetof(PLYR_WRK, mdl) >= 0x26c, "PLYR_WRK.mdl follows the EE fields");

#define PLYR_NUM 3                      // the three player works (data 0x32f4a0)
extern PLYR_WRK plyr_wrk[PLYR_NUM];     // data 0x32f4a0 (GetPlyrWrk)

// PLYR_WRK.mode (PlayerMain's handler switch).
enum PLYR_MODE
{
    PLYR_MODE_NORMAL = 0,               // PlyrNormalCtrl (and every mode above 10)
    PLYR_MODE_DMG1   = 1,               // 1..4: damage reactions (PlyrDamageCtrl); 1 starts a hold
    PLYR_MODE_DMG2   = 2,               // the hit / hold begins (by dmg_type)
    PLYR_MODE_DMG3   = 3,               // held: damage ticks, escape presses
    PLYR_MODE_DMG4   = 4,               // recovering: back to 0 when the end anime is over
    PLYR_MODE_5      = 5,               // the ladder (PlyrMode5Ctrl); no shadow-side draw
    PLYR_MODE_DOOR   = 6,               // PlyrDoorCtrl
    PLYR_MODE_7      = 7,               // PlyrDeathCtrl (the death scene); sticky: PlyrSetMode only leaves it for 0
    PLYR_MODE_8      = 8,               // PlyrMode8Ctrl (flash recoil)
    PLYR_MODE_9      = 9,               // nothing
    PLYR_MODE_10     = 10,              // PlyrMode10Ctrl(plyr) (player 2's stance)
    PLYR_MODE_11     = 11,              // PlyrMode11Ctrl (anime 0x25), run from mode 0's handler
};

#define PLYR_FLG_800000 0x800000        // camera up: the photo mode work draws the viewfinder (PhotoModeGetViewAlpha 0x80); PlayerMain: PlyrFinderCtrl
#define PLYR_FLG_400000   0x400000      // going into the viewfinder (PlyrFinderModeStart); PlayerMain: PlyrFinderStartEnd
#define PLYR_FLG_1000000  0x1000000     // coming out of it (PlyrFinderModeEnd); PlayerMain: PlyrFinderEndEnd
#define PLYR_FLG_HOLD     0x1c00000     // any of the three above: PlayerMain runs that handler instead of the mode's
#define PLYR_FLG_4000000  0x4000000     // PlayerMain: PlyrCrawlStart (going down, anime 0x9e)
#define PLYR_FLG_8000000  0x8000000     // PlayerMain: PlyrCrawlCtrl (crawling)
#define PLYR_FLG_10000000 0x10000000    // PlayerMain: PlyrCrawlEnd (getting up)
#define PLYR_FLG_PZL      0x1c000000    // any of the three above (the crawl; name kept from the first guess)
#define PLYR_FLG_ANIME_END 0x2000       // the model's anime ended (PlyrAnimeEndCheck; CheckPlyrAnimeEnd)
#define PLYR_FLG_BATTLE2  0x000040      // a type-2 enemy is active (PlyrBattleCheck)
#define PLYR_FLG_000004   0x000004      // time step 0.6 (PlyrGaugeTimeStep); a shot then only slows the ghosts it hits (PlyrShotJudge) and scores nothing (PlyrShotScore); setter not traced
#define PLYR_FLG_SHOT_KILL 0x000080     // the last shot finished a ghost (PlyrShotDamage sets, PlyrShotJudge clears first)
#define PLYR_FLG_100000 0x100000        // player 1: the slow mode is on (PlyrGaugeSlowStart / End); shows the HUD (PhotoModeHudFadeMain)
#define PLYR_FLG_000100 0x000100        // shows the HUD (PhotoModeHudFadeMain)
#define PLYR_FLG_000020 0x000020        // an ordinary enemy is active (PlyrBattleCheck); shows the HUD, as 0x800000 (PhotoModeHudFadeMain)

#define PLYR_FLG_DOOR_LOCK 0x000008     // a locked door is being tried (init_Story_Door_Open; MapDoorModeClose clears it)
#define PLYR_FLG_DOOR_OPEN 0x000010     // with the door anime over, PlyrDoorCtrl ends the door (PlyrDoorEnd)
#define PLYR_FLG_10000    0x10000       // cleared when a hit is over (PlyrDamageCtrl); set with 0x20000 when the hold is broken
#define PLYR_FLG_20000    0x20000       // set by the hold (PlyrDamageCtrl)
#define PLYR_FLG_VIEW_EASE 0x40000      // the view eases to a new pitch / yaw (FinderViewTurnCtrl; cleared when the time is up)
#define PLYR_FLG_IDLE_VAR 0x80000       // an idle variant plays (PlyrAnimeSelIdle; SetPlyrAnime clears it for other anime)
#define PLYR_FLG_MODE10   0x200000      // mode 10's stance (PlyrMode10Start / End)
#define PLYR_FLG_REVIVED  0x2000000     // came back from mode 7 with lock 0x18 (PlyrDeathCtrl; PlyrDeathStart clears it)
#define PLYR_FLG_LOOK     0x20000000    // the head looks at a target or a set position (PlyrLookMain, PlyrLookPosSet)

// Equipment levels (sdata gp+0x330, 7 bytes, PlyrGetEquipLv): [0] non-zero:
// player 0 can flash, [1] the flash count level (PlyrFlashNumByLv), [2] and
// [5] used by player 1's camera.
#define PLYR_EQUIP_NUM 7

// The hold anime of each attacker kind (PLYR_WRK._147), data 0x32fe90:
// [0] grabbed, [1] the hold, [2] let go (PlyrDamageCtrl; PlyrFinderEndEnd
// goes back to [1]).
enum { PLYR_HOLD_GRAB, PLYR_HOLD_LOOP, PLYR_HOLD_FREE };
extern const u_char plyr_hold_anime[3][16];

void PlyrWrkInitAll(void);              // the 3 player works
void PlyrWrkInitKeepEquip(void);        // same, keeping the equipment bytes
void PlyrLoadRestore(void);
void PlyrModeInit(void);                // equipment levels, flash wait, flash count
void PlyrSetHpByDifficulty(u_char diff);    // hp = hp_max from the table; 0xff: the current difficulty
void PlyrClear234(void);                // current player work +0x234 = 0

int       PlayerMain(void);             // per frame; returns PLYR_WRK._234 != 0 (callers ignore it)
int       GetPlyrAreaNo(void);
PLYR_WRK *GetPlyrWrk(s_char no);
u_char    PlyrGetCurNo(void);           // ingame_wrk.b00 (was GetPlyrRoomID)
void      PlyrSetCurrent(u_char no);    // no < 3: ingame_wrk.b00 / plyr_wrk
u_int     PlyrGetDrawSkipFlags(void);   // 0xbe or 0, ORed into IngameDrawSub's flags
int       PlyrIsMode233Over6(void);
void      PlyrSetB260Mode2(void);       // current player work byte +0x260 = 2 (end of pause)
void      PlyrSet264(float v);          // current player work +0x264
u_short   PlyrGetFlashNum(void);        // current player work +0x268 (the HUD's NUM_flash)
void      PlyrFlashNumSub(u_short n);
void      PlyrFlashNumInit(void);       // player 0's flash_num from its level
u_char    PlyrFlashNumByLv(u_char lv);  // 4, 8, 12, 16; lv 0xff: PlyrGetEquipLv(1)
int       PlyrFlashAble(void);          // player 0, PlyrGetEquipLv(0) and PhotoModeIsFinderReady
int       PlyrFlashIsReady(void);       // the flash wait (sdata gp+0x32c) is over
int       PlyrFlashInput(void);         // the flash button fires a flash now
u_char    PlyrGetEquipLv(u_char no);    // 5: PhotoWrk1e8SeMain plays its SE
void      SetPlyrAnime(u_char anime_no, u_char blend, u_char phase, u_char attr, u_char arg, float start);
                                        // 0xff for blend / phase / attr: from the anime tables
int       PlyrAnimeIsIdle(u_char anime_no); // 1..3, 10
void      PlyrIdleTimeSet(void);        // idle_time: 90..210 frames

// The current player work and its pieces (0x231c70..0x2373e8).
PLYR_WRK *PlyrGetCurWrk(void);          // ingame_wrk.plyr_wrk
void      PlyrSetMode(u_char mode, PLYR_WRK *plyr);     // plyr nullptr: the current one
int       PlyrGetHp(PLYR_WRK *plyr);
int       PlyrGetHpMax(PLYR_WRK *plyr);
float     PlyrSetHp(PLYR_WRK *plyr, int hp);            // clamped; returns hp / hp_max
int       PlyrAddHp(PLYR_WRK *plyr, int n);             // only below hp_max; returns whether it healed
void      PlyrDamage(PLYR_WRK *plyr, int dmg);          // hp loss, item 7 refill or death; HUD
int       CheckPlyrAnimeEnd(u_char anime_no);           // 1: ended (or another anime runs)
int       PlyrSePlay(PLYR_WRK *plyr, u_int se, int vol, int pitch); // SE of the player's bank at its position
void      PlyrDangerSeCtrl(void);
void      PlyrDangerSeStop(s_char no);
void      VibrateRequest1(int pad, int on); // small motor this frame (option vibration on)
void      VibrateRequest2(int pad, u_char pow); // big motor this frame
void      PlyrVibCtrl(u_char time);
void      PlyrVibCtrlBig(u_char pow, u_char time);
int       PlyrGetMashInput(void);       // 0, 1 (button / stick pushed), 2 (stick pushed hard)
int       PlyrMashStep(void);
int       PlyrGetAnimeFrame(void);      // the model's anime frame
int       PlyrGetInputCount(void);      // sticks pushed + buttons held

// PlayerMain's per-frame pieces.
void PlyrCtrlOptCheck(void);            // option b11 changes
void PlyrAnimeEndCheck(void);           // PLYR_FLG_ANIME_END, anime_end_cnt
void PlyrEneCheck(void);                // the four enemy checks below, ene_wait
void PlyrBattleCheck(void);             // PLYR_FLG_000020 / PLYR_FLG_BATTLE2, battle_cnt
void NearEneInfo(PLYR_WRK *plyr);       // near_* / near2_*
void PlyrEneTouchCheck(void);           // touching enemies get EneAttackReq(ene, 9, 0xc)
void PlyrVibCheck(void);                // the heartbeat rumble
void PlyrStatusCtrl(void);              // status / status_time, timers, old copies
void PlyrMapCamCheck(void);             // camchg_time

// PlayerMain's handlers.
void PlyrDamageCtrl(void);              // modes 1..4
void PlyrNormalCtrl(void);              // mode 0
void PlyrFinderCtrl(void);              // PLYR_FLG_800000 (camera up)
int  PlyrDoorCtrl(void);                // mode 6; 1 when the door ended this frame
int  PlyrMode11Ctrl(void);              // 1 while mode 11 holds
void PlyrDeathStart(u_char kind);       // the player dies (PlyrDamage at hp 0; 4, 1 or 0x11)
int  PlyrHoldEscapeCheck(void);         // both escape windows have passed
void PlyrFlashRelease(void);            // a flash breaks the hold
void PlyrHoldShakeOff(void);            // the hold broken without a flash
int  PlyrDoorEnd(void);
int  DoorMotionIsEnd(void);             // the door anime (sbss gp+0x31c) ended

// The death scene, mode 7 (0x231ce0..0x232368).
int  PlyrDeathCtrl(void);               // PLYR_WRK.death_step; 1 on the frame it ends (game over or revival)
int  PlyrDeathIsRevived(void);          // lock 0x18 and PLYR_FLG_REVIVED

// Other modes and the per-character action button.
void PlyrMode8Ctrl(void);               // mode 8: back to 0 when the flash recoil (0x98 / 0x82 / 0x95) is over
void PlyrMode10Ctrl(PLYR_WRK *plyr);    // mode 10 (player 2's stance): mode10_time, ends with anime 0xa3
int  PlyrMode10Start(PLYR_WRK *plyr);   // 1: entered mode 10
void PlyrMode10End(PLYR_WRK *plyr);
int  PlyrMode10AnimeEnd(PLYR_WRK *plyr);    // anime 0xa3 over
int  PlyrIsMode10(PLYR_WRK *plyr);
void PlyrMode11Start(void);             // mode 11 with anime 0x25
void PlyrActionCtrl(void);              // the flash (player 0), the camera (1), mode 10 (2)
int  PlyrCamAble(void);                 // player 1 (mode != 7), PlyrGetEquipLv(2) and PhotoModeIsFinderReady
int  PlyrCamInput(void);                // PlyrCamAble and the flash button

// The viewfinder and the enemies.
struct ENE_WRK;
void   PlyrFinderEneCheck(void);        // PlyrFinderCtrl: the enemy slots' finder flags 0x40..0x400
int    FrameInsideChk(float *tv, float *tx, float *ty);    // tv on screen (tx, ty) and inside the viewfinder frame
int    PlyrFinderTargetCheck(float fov, float range, float *tpos, float *dist);  // in view and in the frame; dist from the frame centre
u_char PlyrEneViewCheck(float dist, float fov);  // 1: an enemy in view, 2: one within dist
int    PlyrIsHeldBy(ENE_WRK *ene); // status 4 (held) by ene (nullptr: by anyone)
void   PlyrDamageCameraReq(ENE_WRK *ene);  // the hit camera by attacker kind (_147); dmg_time
float  PlyrViewRate(float deg, float *tpos);    // 1 facing tpos .. 0 at deg off (the player's or the camera's facing)
float  PlyrCamViewRate(float deg, float *tpos); // the same from the camera
float  PlyrDistRate(float dmin, float dmax, float *tpos);   // 1 within dmin of the player .. 0 at dmax
float  PlyrCamDistRate(float dmin, float dmax, float *tpos);// the same from the camera
float  PlyrViewDistRate(float dmin, float dmax, float deg, float *tpos);
float  PlyrViewDistRate2(float dmin, float dmax, float deg, float *tpos);
int    PlyrViewDistCheck(float dmin, float dmax, float deg, float *tpos);  // bit 0 view rate > 0, bit 1 distance rate > 0

// HP and small accessors.
void  PlyrHitDamage(void);              // the pending dmg as a hit: fire, rumble, SE
void  PlyrHpRestore(void);              // the current player to full HP
void  PlyrHpRestoreAll(void);           // all three
void  PlyrClear108(void);
float PlyrGetHpRate(void);              // hp / hp_max
void  PlyrSetEneWait(PLYR_WRK *plyr, u_short frames);   // ene_wait in frames of the current rate (FF3 passes &plyr->flags)
int   PlyrIsMode5Or2000000(void);       // mode 5 or flags2 0x2000000
int   PlyrLineHitCheck(float *to);      // MapHitLineCheck from the player to 'to'
int   PlyrIsBattle(void);               // PLYR_FLG_000020 (was matched to FF2 InFinderMode)
void  PlyrSetPos(float *pos);           // moves the listener with the player
void  PlyrSetRot(float *rot);
int   PlyrStatusIs(u_char status);      // (was matched to FF2 PlayerModeIsFinder)
u_char PlyrGet348(void);                // (was matched to FF2 InDamageState)
int   PlyrHasMove320(void);
void  PlyrMapPosUpdate(void);           // map hits, floor and area after a move; hold_move = the move
int   PlyrOnMapObj(u_char kind);        // a map hit object of that kind at the player
void  PlyrSet338(void);                 // _338 = 90 frames

// The rest (0x2359c8..0x2393f8).
void   PlyrLookCtrl(void);              // the right stick turns the head: look_pitch / look_yaw
void   PlyrDoorStart(int door_id, int side);    // mode 6 with the door's anime (DoorDatGet); side 0 / 1 set the camera
void   PlyrEquipBackup(void);           // the three works' equip[3] to the system heap (CExtraMissionShelt::CarryStore)
void   PlyrEquipRestore(void);          // and back, freeing it
int    PlyrWrkSaveGet(void **addr);     // FF3 0x002367f0: {plyr_wrk (0x32f4a0), 0x9f0}, save block 28
int    PlyrEquipLvSaveGet(void **addr); // FF3 0x00237858: {plyr_equip_lv (0x373820), 7}, save block 56
void   PlyrMode10Cancel(PLYR_WRK *plyr);// leaves mode 10's stance at once (no end anime)
int    PlyrMode10IsStarting(PLYR_WRK *plyr);    // mode 10 with its start anime 0xa1 (CameraMain)
float  PlyrEquip3Rate(u_char lv);       // 0.7, 0.8, 0.9, 1.0 by level (0xff: PlyrGetEquipLv(3))
float  PlyrEquip4Rate(u_char lv);       // 1.0, 0.95, 0.9, 0.85 by level (0xff: PlyrGetEquipLv(4))
int    PlyrIsFlashRecoil(void);         // the flash recoil anime runs (0x98 / 0x95 for player 0, 0x82 with PLYR_FLG_10000)
void   PlyrEquipObtain(u_char no);      // equipment level 1
void   PlyrEquipLvUp(u_char no);        // one level up, to the slot's maximum
int    PlyrDirAnimeNo(void);            // the side-step / turn anime (0x26..0x3d) for flags2's direction bits, -1: none
int    PlyrAnimeInfo(void);             // GameAnimeModel::GetAnimeLength of the current anime: its length in frames
int    CheckPlayerNeckSW(void);         // 1: the current anime turns the neck itself (FF2 took the anime and tested a range)
int    PlyrAnimeEventNo(void);          // the event pose for flags2, -1: none (PlyrAnimeCtrl, PlyrTurnStart)

// The anime each frame (0x2381c8..0x239268).
void PlyrAnimeCtrl(void);               // picks the frame's anime (event, move, step, idle) and sets it

#endif /* _INGAME_PLYR_PLAYER_H */
