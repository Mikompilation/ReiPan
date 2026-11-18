/* ==========================================================================
 *  ingame/plyr/plyr_sub.h
 *
 *  The player continued (FF3-only file, 0x23e208..0x243898; the name is
 *  descriptive, see tus.py define): the ladder (mode 5), the per-character
 *  costume / accessory settings and the player models (CharPlayerModel +
 *  AcsModels: create, ready, release, joints), the map-object flags, the
 *  crawl (flags 0x4000000..0x10000000), the move direction from the stick
 *  and the camera, the turn, and the model's fade envelope (PLYR_WRK +0x2e0).
 * ======================================================================== */
#ifndef _INGAME_PLYR_PLYR_SUB_H
#define _INGAME_PLYR_PLYR_SUB_H

#include <stddef.h>                     // offsetof
#include "sdk/scetypes.h"
#include "ingame/event/prg/ev_ladder.h"  // LADDER_DAT

class  GameModel;                       // ingame/ingame.h
struct RCB_OBJ;                         // ingame/map/MapRcb.h
struct PLYR_WRK;                        // ingame/plyr/player.h

// LADDER_DAT (PLYR_WRK.ladder) is the ladder list's row type.

// Per character (data 0x361448, 6 bytes each; PlyrGetMdlDat): 0 Rei, 1 Miku,
// 2 Kei.
struct PLYR_MDL_DAT                     /* 0x6 */
{
    /* 0x00 */ short chr;               // FUN_00145090's character number
    /* 0x02 */ short type;              // and its model type
    /* 0x04 */ short se_bank;           // SYS_REI/MIKU/KEI_BD_BD: the high half of the player's SE ids
};
static_assert(sizeof(PLYR_MDL_DAT) == 0x6, "PLYR_MDL_DAT is 6 bytes");
static_assert(offsetof(PLYR_MDL_DAT, se_bank) == 0x04, "PLYR_MDL_DAT.se_bank");

// One entry of a character's model table (data 0x361460 / 0x3614d8 /
// 0x361528; PlyrGetCostumeMdlDat).
struct PLYR_COSTUME_MDL                 /* 0x4 */
{
    /* 0x00 */ u_short mdl;             // model pk4 (CD_FILE_DAT)
    /* 0x02 */ u_short shp;             // shape
};
static_assert(sizeof(PLYR_COSTUME_MDL) == 0x4, "PLYR_COSTUME_MDL is 4 bytes");

// The model's fade envelope (PLYR_WRK.mdl_fx, operator new 0x14):
// Main steps it and hands value to the model (FUN_001d3110).
struct PLYR_MDL_FX                      /* 0x14 */
{
    PLYR_MDL_FX();                                                   // FF3 0x00243390 (PlyrMdlFxInit): state 0
    void Req(int in, int hold, int out, u_char level, u_char kind);  // FF3 0x00243398 (PlyrMdlFxReq)
    void ReqPulse(int time, u_char level);                           // FF3 0x00243418 (PlyrMdlFxReqPulse)
    void ReqOut(int time);                                           // FF3 0x00243440 (PlyrMdlFxReqOut)
    void Main(GameModel *mdl, float step);                           // FF3 0x00243468 (PlyrMdlFxMain): PlayerMain
    void GhostCtrl();                                                // FF3 0x00243798 (PlyrMdlFxGhostCtrl): PlayerMain: a pulse while ghost model 0x1a is within 60

    /* 0x00 */ float  in_time;          // frames of the sine rise (states 1-2; the pulse 5-6)
    /* 0x04 */ float  hold_time;        // frames held (state 3); -1: until the next request
    /* 0x08 */ float  out_time;         // frames of the linear fall (states 4, 8)
    /* 0x0c */ u_char level;            // the peak
    /* 0x0d */ u_char value;            // this frame's value
    /* 0x0e */ u_char state;            // 0 off, 1-2 rise, 3 hold, 4 fall, 5-6 pulse, 7 stay, 8 fall
    /* 0x0f */ u_char _0f;
    /* 0x10 */ float  time;             // frames in the state
};
static_assert(sizeof(PLYR_MDL_FX) == 0x14, "PLYR_MDL_FX is 0x14 bytes");
static_assert(offsetof(PLYR_MDL_FX, level) == 0x0c, "PLYR_MDL_FX.level");
static_assert(offsetof(PLYR_MDL_FX, state) == 0x0e, "PLYR_MDL_FX.state");
static_assert(offsetof(PLYR_MDL_FX, time) == 0x10, "PLYR_MDL_FX.time");

// The ladder (mode 5).
void PlyrLadderStartBottom(u_char no);  // FF3 0x0023e208; no: the ladder's id
void PlyrLadderStartTop(u_char no);     // FF3 0x0023e330
void PlyrLadderEnd(u_char mh_no);       // FF3 0x0023e450; mh_no unused
void PlyrLadderCamera(void);            // FF3 0x0023e480
void PlyrLadderCtrl(void);              // FF3 0x0023e540
void PlyrMode5Ctrl(void);               // FF3 0x0023e788: PlayerMain mode 5
void FUN_0023e7b0(float rot, void *arg);    // the model's turn callback (PlyrMdlSetup; arg unused)

// Costume and accessory (no -1: the current player).
void   PlyrSetCostume(s_char no, u_char v);
u_char PlyrGetCostume(s_char no);
void   PlyrSetAcs(s_char no, u_char v);
u_char PlyrGetAcs(s_char no);
int    PlyrGetAcsModelNo(u_char no);    // -1: no accessory
void   PlyrSetCostumeSub(s_char no, u_char v);
u_char PlyrGetCostumeSub(s_char no);
const PLYR_COSTUME_MDL *PlyrGetCostumeMdlDat(u_char no, u_char kind);  // kind: PlyrMdlCreate's 2nd argument
const PLYR_MDL_DAT     *PlyrGetMdlDat(u_char no);
int    PlyrGetMdlChr(short mdl);        // the character whose costume row 0 holds model file mdl, -1: none
short  PlyrCostumeMdlConv(short mdl);   // that model in the current costume's row (others unchanged)

// The player models.
int   PlyrMdlCreate(u_char no, u_char kind, u_char mot);  // mot 0xff: keep PLYR_WRK._2ec; 1 when made
int   PlyrMdlChange(u_char no, u_char kind, s_char mot);  // the current player's model released, no made current and created (mot -1: keep); PlyrMdlCreate's result
int   PlyrMdlIsReady(s_char no);
int   AllPlyrIsReady(void);             // every created model ready
int   PlyrMdlSetup(u_char no);          // attaches the AcsModels after the load
int   AllPlyrMdlIsLoaded(void);
int   PlyrMdlRelease(u_char no);        // 0xff: the current player
void  PlyrMdlReleaseAll(void);          // IngameEnd
void  PlyrSeStopAll(u_char no);
void  PlyrMdlMain(void);                // PlayerMain's last call
void  PlyrAcsFadeCtrl(int no);          // req280[no] fades acs_mdl[no]
int   PlyrMdlDrawAble(void);            // not crawling, lock 6 off
void  PlyrMdlDraw(PLYR_WRK *plyr, u_int pass);   // the model and its AcsModels; pass bit 0x200: the viewfinder's pass
int   PlyrAcsIsNot2(u_char no, u_char acs);     // acs != 2 (no unused): accessory 2 draws before the body
void  PlyrMdlMotReset(void);            // the current model's motion object: vtable +0x18 (-1, 1)
float PlyrGetJointRotY(int joint);      // 0x1b: the facing (PlyrViewRate); 3: PlyrCheckAction's
float PlyrGetJointRot(int joint, int axis);
void  PlyrGetJointPos(int joint, float *out, float *in);  // in nullptr: (0, 0, 0, 1)
void  PlyrSet280Req(int no, float v0, float v1, float v2, float v3);   // PLYR_WRK.req280[no]: the fade of acs_mdl[no]

// Map objects, the crawl, moving and turning.
void  PlyrMapObjFlagCtrl(void);         // PlayerMain
void  PlyrSetMhNo(int no);              // mh_no and floor
void  PlyrCrawlReq(void);               // player 1: get down (anime 0x9e, PLYR_FLG_4000000)
void  PlyrCrawlSet(void);               // player 1: crawling at once (PLYR_FLG_8000000)
void  PlyrCrawlEndReq(void);            // player 1: get up (PLYR_FLG_10000000)
void  PlyrCrawlCancel(void);            // player 1: the crawl ends at once (mode 0)
void  PlyrCrawlStart(void);             // PLYR_FLG_4000000
void  PlyrCrawlEnd(void);               // PLYR_FLG_10000000
void  PlyrCrawlCtrl(void);              // PLYR_FLG_8000000
void  PlyrCrawlSeStop(u_char no);
void  PlyrCrawlSeTimer(float speed);
void  PlyrCrawlMove(void);
float PlyrGetMapObjDir(RCB_OBJ **obj0, RCB_OBJ **obj1);   // radians
int   PlyrMapDirAnimeCtrl(void);        // PlyrAnimeCtrl: non-zero leaves the anime to it (unless mode 8 / hold flags)
int   PlyrOnMapDirObj(void);            // a direction object at the player
void  PlyrMoveDirCtrl(void);            // PlyrNormalCtrl: option b11 != 1 (camera-relative)
void  PlyrTankMoveCtrl(void);           // PlyrNormalCtrl: option b11 == 1 (the d-pad turns)
void  PlyrFinderMoveCtrl(void);         // PlyrFinderCtrl: turning, pitch, the finder point, steps
float PlyrGetStickAngle(void);          // 10.0: none
u_int PlyrGetDpadDir(u_char mode);      // 0 up, 2 right, 4 down, 6 left (odd: diagonals), 0xff none
float PlyrGetFinderStickAngle(void);    // 10.0: none
void  PlyrMoveUpdate(void);             // PlayerMain: the frame's move and turn applied
int   PlyrMoveSpeedCtrl(float angle);   // flags2 bits 0..3 (1 run, 2 walk); 1: the run button chose; angle unused
void  PlyrTurnStart(float dir, PLYR_WRK *plyr);    // the turn towards dir (flags2 0x400..0x20000)
void  PlyrMapCamChgClear(void);         // PlyrMapCamCheck outside mode 0
void  PlyrTurnCtrl(PLYR_WRK *plyr);
void  PlyrQuickTurnCtrl(PLYR_WRK *plyr);    // flags2 0x400
void  PlyrFinderPointMove(float x, float y); // stick offsets from the centre (-128..127)
void  PlyrTurnFlagClear(void);          // PlyrCtrlOptCheck and 16 more
float PlyrGetCamChgTime(void);          // PlyrMapCamCheck: PLYR_WRK.camchg_time
int   PlyrIsFlashRecoilDir(void);       // the flash recoil on a direction / map object (flags2 0xc003f0)
void  PlyrMotMoveGet(void);             // PLYR_WRK.move from the motion

// The model's fade envelope.
                                        // PlyrDamageCtrl: (0x3c, .., 0x1e, 0x80, 1)

#endif /* _INGAME_PLYR_PLYR_SUB_H */
