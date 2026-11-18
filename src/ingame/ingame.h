/* ==========================================================================
 *  ingame/ingame.h
 *
 *  The in-game driver: phase 12, IngameInit/Pre/Main/End, IngameDrawSub,
 *  the load/save steps, GPhaseCtrlRequest (prioritised phase requests) and
 *  the channel-0 sound mute count. FF3 0x1d0710..0x1d2610 (FF2
 *  ingame/ingame.c; the game work that FF2 kept at its head is
 *  ingame/ingame_wrk.h in FF3).
 * ======================================================================== */
#ifndef _INGAME_INGAME_H
#define _INGAME_INGAME_H

#include <stddef.h>                 // offsetof
#include "sdk/scetypes.h"
#include "graphics/game_anime_model.h" // GameAnimeModel (and GameModel, GameMotionModel)
#include "ingame/char_model.h"     // CharPlayerModel, CharEnemyModel, CharNpcModel, CharCatModel

class CWindowMsg;                      // common/WindowMsg.h
class ModelManager;                         // graphics/mmanage.h
typedef ModelManager MDL_MOT;

// The model object roots GameModel / GameMotionModel (FF3 RTTI) are
// graphics/game_model.h, GameAnimeModel graphics/game_anime_model.h, the
// character models (CharModel -> CharPlayerModel / CharEnemyModel /
// CharNpcModel / CharCatModel) ingame/char_model.h.

// AcsModel (the player's accessory models): ingame/plyr/acs_model.h.

// Parameters of a phase-12 run (bss 0x3eb388, 6 words copied by
// Phase12ParamSet). Phase 12 shows a message window (type 2: pages turned
// with the confirm button) or a selection window (other types), then
// requests one of the next phases by the result (init/one/end_Phase12).
// layout: host -- end_func and win are EE pointers (4 bytes) there; offsets
// before them match the ROM.
struct PHASE12_PARAM                /* 0x18 on the EE */
{
    /* 0x00 */ short msg0;          // message ids (GetMsgDataAddr's two arguments)
    /* 0x02 */ short msg1;
    /* 0x04 */ int   type;          // 2: message pages, else selection (1: CWindowMsg::StartSel's type flag)
    /* 0x08 */ short next_ok;       // next phase on result 1
    /* 0x0a */ short next_alt;      // next phase on results 2 and 4
    /* 0x0c */ short next_cancel;   // next phase on result 3; -1 = no cancel
    /* 0x0e */ short _0e;
    /* 0x10 */ void (*end_func)(int);   // called by end_Phase12 with the window's result
    /* 0x14 */ CWindowMsg *win;        // caller's window; nullptr: phase 12 makes and runs its own
};
static_assert(offsetof(PHASE12_PARAM, type) == 0x04, "PHASE12_PARAM.type");
static_assert(offsetof(PHASE12_PARAM, next_ok) == 0x08, "PHASE12_PARAM.next_ok");
static_assert(offsetof(PHASE12_PARAM, next_cancel) == 0x0c, "PHASE12_PARAM.next_cancel");
static_assert(offsetof(PHASE12_PARAM, end_func) == 0x10, "PHASE12_PARAM.end_func");

extern PHASE12_PARAM phase12_param;             // bss 0x3eb388

void Phase12ParamSet(const PHASE12_PARAM *param);
void Phase12FadeLayerOn(void);
int  init_Phase12(void);
int  one_Phase12(void);
int  end_Phase12(void);
int  IngameNowLoadingMain(void);    // one_Story_NowLoading's work; 1 when play can start

// Draw-enable words IngameSetDrawEnable computes each frame and IngameDrawSub
// sets back to 1 (sdata; owner not settled, see open-items). Readers besides
// IngameDrawRoomMorph (371300) are not reconstructed yet.
extern int draw_en_371300;          // room morph / shadow side (map camera +0x30)
extern int draw_en_371304;          // map camera +0x34
extern int draw_en_371308;          // !draw_en_371300 before the player overrides
extern int draw_en_371348;          // current leaf phase != 0x22
extern int draw_en_37126c;          // same as draw_en_37127c
extern int draw_en_37127c;          // current leaf phase != 0x22

int  IngameGetDrawFlags(void);      // IngameDrawSub's skip flags of this frame
int  IngameMain(u_int skip);        // per-frame game step; 1 when a phase change is requested
void IngameDrawSub(u_int skip);     // per-frame game draw
void IngameNewGameReset(void);
int  IngameMainInit(void);           // init_Story_Main: 1
int  IngameInit(void);
int  IngameStartEnd(void);           // end_Story_Start: 1
int  IngameLoadWait(void);
int  IngameLoadEnd(void);
void IngameLoadRestore(void);
void IngameStartNewGame(void);
int  IngameSaveInit(void);
int  IngameSaveEnd(void);
int  IngameEnd(void);
void IngamePre(void);
void IngameAfter(void);             // after_Story_Main: empty
int  IngameIsIdOver2e(int id);
void IngameSndListenerSet(int from_plyr);       // 0: listen from the camera

// Model resets (motion object vtable +0x34, then +0x18 with (-1, arg)).
void IngamePlyrMdlReset(int arg);               // the 3 player models
void IngamePlyrCamMdlReset(int arg);            // the 2 player-camera models
void IngameEffSlotMdlReset(short id, int arg);  // effect slots whose model id is id (-1: all)
void IngameMdlResetAll(int arg);

short GPhaseCtrlRequest(short id, int link);    // returns the phase actually requested
void  SndSePlayStd(int se);
void  SndVolume0MuteReset(void);
void  IngameSndMuteReset(void);
void  SndVolume0Mute(int mute);

#endif /* _INGAME_INGAME_H */
