/* ==========================================================================
 *  ingame/enemy/fly_ctrl.h
 *
 *  FF2's fly_ctrl.c in FF3 (0x1bf108..0x1c1418): the flies an enemy sends
 *  out at the player. Forty fly works (bss 0x3e7e30, 0xc0 each) shared by
 *  every enemy; an ordinary enemy takes five per fly type its dat lists
 *  (FlyAct), and its action script launches them one at a time
 *  (EneFlyActByKind -> EneFlyAct -> SetFlyOne). A fly is either a model
 *  (FlyModel, file FLY000_PK4 + mdl_no) or, with mdl_no bit 0x8000, an
 *  effect only (a torch effect, or EffKind1D for type 2). FlyRule moves them
 *  each frame; a touch hurts the player, the camera (PhotoFlyChk) or the
 *  flash (FlyFadeAll) fades them. The photo sub-work code that follows is
 *  ingame/photo/fndr_effect.c.
 * ======================================================================== */
#ifndef _INGAME_ENEMY_FLY_CTRL_H
#define _INGAME_ENEMY_FLY_CTRL_H

#include <stddef.h>                     // offsetof
#include "sdk/scetypes.h"
#include "sdk/libvu0.h"                 // sceVu0FVECTOR, sceVu0FMATRIX
#include "graphics/game_model.h"        // GameModel

struct ENE_WRK;                         // ingame/enemy/ene_main.h
struct PLYR_WRK;                        // ingame/plyr/player.h
struct FLY_MDL_WRK;

// A fly type (data 0x327318, 4 rows of 0x18; FF2 FLY_DATA had 6 rows of 0x34:
// FF3 moved the flight numbers into the curves below).
struct FLY_DAT                          /* 0x18 */
{
    /* 0x00 */ u_char  attr;            // FLY_ATTR_*
    /* 0x01 */ u_char  alp;             // alpha, 128 = 1
    /* 0x02 */ u_short mdl_no;          // model FLY000_PK4 + n; bit 0x8000: an effect only (0, 1: a torch, 2: EffKind1D)
    /* 0x04 */ u_short dmg;             // added to the player's damage on a hit (FlyAtkHit)
    /* 0x06 */ short   cond;            // the player's timed status after a hit, 0: none (FlyAtkHit)
    /* 0x08 */ u_int   hit_rng;         // hit box half size around the player's head (+24 below; FlyPlyrHitChk)
    /* 0x0c */ u_int   life;            // frames: life and homing time are life + rand(life) (SetFlyOne)
    /* 0x10 */ int     mv_curve;        // fly_mv_curve row: speed and turn rate, -1: none (FlyMove)
    /* 0x14 */ int     wob_curve;       // fly_wob_curve row: the wobble, -1: none (FlyMove)
};
static_assert(sizeof(FLY_DAT) == 0x18, "FLY_DAT: FlyAct's stride");
static_assert(offsetof(FLY_DAT, mdl_no) == 0x02, "FLY_DAT.mdl_no");
static_assert(offsetof(FLY_DAT, dmg) == 0x04, "FLY_DAT.dmg");
static_assert(offsetof(FLY_DAT, cond) == 0x06, "FLY_DAT.cond");
static_assert(offsetof(FLY_DAT, hit_rng) == 0x08, "FLY_DAT.hit_rng");
static_assert(offsetof(FLY_DAT, life) == 0x0c, "FLY_DAT.life");
static_assert(offsetof(FLY_DAT, mv_curve) == 0x10, "FLY_DAT.mv_curve");
static_assert(offsetof(FLY_DAT, wob_curve) == 0x14, "FLY_DAT.wob_curve");

#define FLY_ATTR_NO_PHOTO   0x01        // the camera does not fade it (PhotoFlyChk)
#define FLY_ATTR_LEVEL      0x02        // launched without pitch (SetFlyOne)
#define FLY_ATTR_YAW_FREE   0x04        // turns to the target's yaw at once (FlyMove)
#define FLY_ATTR_LAND       0x08        // stops at a fixed target's height (FlyMove, FLY_STA_LANDED)

#define FLY_DAT_NUM     4
#define FLY_WRK_NUM     40              // bss 0x3e7e30, 0xc0 each (GetFlyWork)

#define FLY_STA_MOVE    0x0001          // launched: FlyRule moves it (FF2's)
#define FLY_STA_KILL    0x0002          // spent: ClearCheckFlyWork retires it
#define FLY_STA_HOLD    0x0004          // frozen: no life count, no move (nothing sets it in FF3 so far)
#define FLY_STA_FADE    0x0008          // fading out, then KILL
#define FLY_STA_USE     0x0010          // slot taken (GetFlyWork scans for it)
#define FLY_STA_STOP    0x0040          // speed 0 this frame: stays put (cleared when every moving fly has it)
#define FLY_STA_LANDED  0x0080          // reached a fixed target's height: no more moves

// RTTI FlyModel : GameModel, vtable 0x341d68 (GameModel's slots with +0x1c
// GetWorldMatrix and +0x20 DrawMdlPre replaced; its destructors 0x2b9cb8 /
// 0x2b9ce0 are GameModel's, implicit here). FF3 allocates it with operator
// new(0x20) (FlyModelDataInit).
// layout: host -- derived from GameModel; EE offsets in the comments.
class FlyModel : public GameModel
{
public:
    FlyModel(FLY_MDL_WRK *fly);         // FF3 0x001bf148 (base-object twin 0x001bf108)

    void GetWorldMatrix(sceVu0FMATRIX out) override;   // FF3 0x001bf188: vtable +0x1c
    void DrawMdlPre() override;         // FF3 0x001bf300: vtable +0x20, the light set
    int  Load(short no);                // FF3 0x001bf2d8: model file FLY000_PK4 + no

    /* 0x1c */ FLY_MDL_WRK *fly;        // the fly it draws
};
static_assert(offsetof(FlyModel, fly) == sizeof(GameModel), "FlyModel.fly (EE +0x1c)");

// A fly (FF2 FLY_WRK, 0x1460 there with its own light set; FF3 0xc0, bss
// 0x3e7e30 x 40). FlyModelDataInit starts its model: init_flow 0 -> 1
// (FlyModel new'd) -> 2 (ready, CharDrawSetFly); 3 frees the slot.
// layout: host -- ew, dat, target, efpw and mdl are EE pointers (4 bytes at
// +0x04, +0x08, +0x0c, +0x70, +0xb4); the runs between them keep the EE's
// relative offsets (asserted from npos, mdl_no and dpos).
struct FLY_MDL_WRK                      /* 0xc0 on the EE */
{
    /* 0x00 */ u_short       sta;       // FLY_STA_*
    /* 0x04 */ ENE_WRK      *ew;        // the enemy that sent it; nullptr: none (FF2's)
    /* 0x08 */ const FLY_DAT *dat;      // its type's row (FlyAct, SetFlyOne)
    /* 0x0c */ PLYR_WRK     *target;    // the player it flies at
    /* 0x10 */ sceVu0FVECTOR npos;      // its position (CharDrawSetFly sorts by it)
    /* 0x20 */ sceVu0FVECTOR opos;      // last frame's
    /* 0x30 */ sceVu0FVECTOR nrot;      // its heading: opos -> npos (FlyModel's rotation)
    /* 0x40 */ sceVu0FVECTOR tpos;      // where it flies: the target's head (listen_pos) or fpos
    /* 0x50 */ sceVu0FVECTOR trot;      // the heading to tpos
    /* 0x60 */ u_int         life_time; // frames to the fade
    /* 0x64 */ u_int         trace_time;// frames of homing left; then it flies straight
    /* 0x68 */ int           now_cnt;   // frames flown: the key of a frame-keyed speed curve
    /* 0x6c */ float         alp;       // alpha rate 0..1 (dat->alp / 128); the fade takes 1/16 a frame
    /* 0x70 */ void         *efpw;      // an effect fly's effect (torch / EffKind1D)
    /* 0x74 */ u_int         mdl_no;    // dat->mdl_no (FlyAct); bit 0x8000: an effect fly
    /* 0x78 */ int           init_flow; // FlyModelDataInit's step (FlyAct clears it)
    /* 0x7c */ int           wob_cnt;   // frames flown: the wobble curve's key
    /* 0x80 */ int           wob_ang;   // the figure-eight's phase, 1/100 degree (0..35999)
    /* 0x84 */ u_char        _84[0xc];
    /* 0x90 */ sceVu0FVECTOR dpos;      // where it is drawn: npos plus the wobble (FF2 adjv)
    /* 0xa0 */ sceVu0FVECTOR fpos;      // a fixed target (EneFlyActByKind 3) when fixed
    /* 0xb0 */ int           fixed;     // 1: tpos is fpos, not the target's head
    /* 0xb4 */ FlyModel     *mdl;       // its model (coordinate 0: the draw matrix)
};
static_assert(offsetof(FLY_MDL_WRK, sta) == 0x00, "FLY_MDL_WRK.sta");
static_assert(offsetof(FLY_MDL_WRK, npos) >= 0x10, "FLY_MDL_WRK.npos (EE 0x10)");
static_assert(offsetof(FLY_MDL_WRK, opos) - offsetof(FLY_MDL_WRK, npos) == 0x10, "FLY_MDL_WRK.opos (EE 0x20)");
static_assert(offsetof(FLY_MDL_WRK, nrot) - offsetof(FLY_MDL_WRK, npos) == 0x20, "FLY_MDL_WRK.nrot (EE 0x30)");
static_assert(offsetof(FLY_MDL_WRK, tpos) - offsetof(FLY_MDL_WRK, npos) == 0x30, "FLY_MDL_WRK.tpos (EE 0x40)");
static_assert(offsetof(FLY_MDL_WRK, trot) - offsetof(FLY_MDL_WRK, npos) == 0x40, "FLY_MDL_WRK.trot (EE 0x50)");
static_assert(offsetof(FLY_MDL_WRK, life_time) - offsetof(FLY_MDL_WRK, npos) == 0x50, "FLY_MDL_WRK.life_time (EE 0x60)");
static_assert(offsetof(FLY_MDL_WRK, trace_time) - offsetof(FLY_MDL_WRK, npos) == 0x54, "FLY_MDL_WRK.trace_time (EE 0x64)");
static_assert(offsetof(FLY_MDL_WRK, now_cnt) - offsetof(FLY_MDL_WRK, npos) == 0x58, "FLY_MDL_WRK.now_cnt (EE 0x68)");
static_assert(offsetof(FLY_MDL_WRK, alp) - offsetof(FLY_MDL_WRK, npos) == 0x5c, "FLY_MDL_WRK.alp (EE 0x6c)");
static_assert(offsetof(FLY_MDL_WRK, efpw) >= 0x70, "FLY_MDL_WRK.efpw (EE 0x70)");
static_assert(offsetof(FLY_MDL_WRK, mdl_no) >= 0x74, "FLY_MDL_WRK.mdl_no (EE 0x74)");
static_assert(offsetof(FLY_MDL_WRK, init_flow) - offsetof(FLY_MDL_WRK, mdl_no) == 0x04, "FLY_MDL_WRK.init_flow (EE 0x78)");
static_assert(offsetof(FLY_MDL_WRK, wob_cnt) - offsetof(FLY_MDL_WRK, mdl_no) == 0x08, "FLY_MDL_WRK.wob_cnt (EE 0x7c)");
static_assert(offsetof(FLY_MDL_WRK, wob_ang) - offsetof(FLY_MDL_WRK, mdl_no) == 0x0c, "FLY_MDL_WRK.wob_ang (EE 0x80)");
static_assert(offsetof(FLY_MDL_WRK, dpos) >= 0x90, "FLY_MDL_WRK.dpos (EE 0x90; 16-aligned after the efpw pointer)");
static_assert(offsetof(FLY_MDL_WRK, fpos) - offsetof(FLY_MDL_WRK, dpos) == 0x10, "FLY_MDL_WRK.fpos (EE 0xa0)");
static_assert(offsetof(FLY_MDL_WRK, fixed) - offsetof(FLY_MDL_WRK, dpos) == 0x20, "FLY_MDL_WRK.fixed (EE 0xb0)");
static_assert(offsetof(FLY_MDL_WRK, mdl) >= 0xb4, "FLY_MDL_WRK.mdl (EE 0xb4)");

int  CheckEffectFly(FLY_MDL_WRK *fly);  // FF3 0x001bf448: 1 for an effect fly (mdl_no bit 0x8000)
int  FlyModelDataInit(FLY_MDL_WRK *fly);    // FF3 0x001bf480: the fly's model a step further; 1 when ready (state 2)
int  FlyDraw(FLY_MDL_WRK *fly);         // FF3 0x001c1248: CharDrawFlyFunc's body: the model drawn while the fly moves; 1 then

void FlyFadeAll(void);                  // FF3 0x001c13d0: every moving fly fades (PlyrFlashRelease)

void FlyAct(ENE_WRK *ene);              // FF3 0x001bfbb0: takes the enemy's flies, starts their models (EneMdlCreate)
int  EneFlyModelInitWait(ENE_WRK *ene); // FF3 0x001bfcc0: 1 once the fly models of an ordinary enemy are ready (enemyIsReadyData)
void EneFlyAct(ENE_WRK *ene, u_char kind, float *pos, float *rot, PLYR_WRK *target, float *fix);    // FF3 0x001bfda8
void EneFlyActByKind(ENE_WRK *ene, u_char kind);  // FF3 0x001bff20: a fly of type kind from joint 0x13 (0) / joint 1 (1..3); 3 at a point near the target (EJobC18)
void EneFlyAnmctrlRelease(ENE_WRK *ene);    // FF3 0x001c03b8: the enemy's flies released (EneRelease)
void EraseEneFlyWork(ENE_WRK *ene);     // FF3 0x001c0450: the enemy's flies fade (EJobC19)

void FlyRule(void);                     // FF3 0x001c1110: every moving fly a frame on (EnemyMain, last)
void PhotoFlyChk(void);                 // FF3 0x001c1290: moving flies near the player in the viewfinder frame fade (PlyrFinderShotSpots)

#endif /* _INGAME_ENEMY_FLY_CTRL_H */
