/* ==========================================================================
 *  ingame/event/prg/ev_command.h
 *
 *  FF3's event script handlers, FF3 0x19e538..0x1a4be0 (FF3-only file; FF2
 *  keeps the same handlers in ev_macro.c). Two ROM tables name every handler
 *  by its script keyword:
 *
 *    - commands   0x3556b8, 235 rows of 0x30: {id, handler, argument bytes,
 *      format string, name[0x20]}: "PLYR_LOCK", "SOUND_PLAY", "EV_SUB_WORK";
 *    - conditions 0x3242e8, 72 rows of 0x34 (row 48 EV_OR has no handler):
 *      {id, handler, format string,
 *      argument bytes, 0, name[0x20]}: "PLYR_AREAIN", "ABOVE_CLEAR_NUM".
 *
 *  A format string is a digit count of bytes to skip (the opcode and its
 *  header), then one digit per argument: 1, 2 or 4 bytes, sign-extended into
 *  one int slot each of EV_EXE_CTRL::args ("1124": skip 1, then a byte, a
 *  short and a word -> args[0..2]). Handlers return 1 when done, 0 to be
 *  called again next frame.
 *
 *  Reconstructed: every command handler and condition in this range,
 *  phase 36 (the room reload), the sound / stream control (FF2's ev_sound_ctrl,
 *  ev_stream_ctrl and ev_save_* blocks; FF3 merged the three saved-stream
 *  tables into one), the effect save blocks, and their commands.
 * ======================================================================== */
#ifndef _INGAME_EVENT_PRG_EV_COMMAND_H
#define _INGAME_EVENT_PRG_EV_COMMAND_H

#include <stddef.h>                     // offsetof
#include "sdk/scetypes.h"
#include "ingame/sleep_movie.h"     // FINDER_BANK_WRK, GetFinderBankWrk

struct EV_EXE_CTRL;                     // ingame/event/prg/ev_exe.h
struct SND_3D_SET;                      // system/eeiop/snd_3d.h

// ── sound control (bss 0x3e6850) ──────────────────────────────────────────

// A looping SE an event started, so SOUND_STOP and the menu pause can find it.
// FF2 EV_SOUND_CTRL; FF3 keys rows by the SE id. sound_id -1: free.
struct EV_SOUND_CTRL                    /* 0xc */
{
    /* 0x0 */ int sound_id;             // SndSePlay handle
    /* 0x4 */ int se;                   // SE id ((bank file << 16) | no)
    /* 0x8 */ int pos;                  // always 0 in FF3
};
static_assert(sizeof(EV_SOUND_CTRL) == 0xc, "EV_SOUND_CTRL is 0xc bytes");
static_assert(offsetof(EV_SOUND_CTRL, se) == 0x4, "EV_SOUND_CTRL.se");
static_assert(offsetof(EV_SOUND_CTRL, pos) == 0x8, "EV_SOUND_CTRL.pos");

#define EV_SOUND_CTRL_MAX   30

// ── stream control (bss 0x3e6a20) ─────────────────────────────────────────

// A stream an event started or preloaded. stream_id -1: free.
struct EV_STREAM_CTRL                   /* 0xc */
{
    /* 0x0 */ int stream_id;            // StreamAuto handle
    /* 0x4 */ int file_label;           // stream file (its header is file + 1)
    /* 0x8 */ int state;                // EV_STREAM_ST_*
};
static_assert(sizeof(EV_STREAM_CTRL) == 0xc, "EV_STREAM_CTRL is 0xc bytes");
static_assert(offsetof(EV_STREAM_CTRL, file_label) == 0x4, "EV_STREAM_CTRL.file_label");
static_assert(offsetof(EV_STREAM_CTRL, state) == 0x8, "EV_STREAM_CTRL.state");

#define EV_STREAM_CTRL_MAX  2
#define EV_STREAM_ST_PLAY       1       // started (loop): EvStreamPlaySub leaves it alone
#define EV_STREAM_ST_PRELOAD    2       // STREAM_PRELOAD: the next play starts the preload

// ── saved streams (bss 0x3e69c0, save block 12) ───────────────────────────

// A looping stream to start again after a load (FF2 EV_SAVE_STREAM,
// EV_SAVE_OBJ_STREAM and EV_SAVE_POS_STREAM in one row, picked by type).
// Save data: written to the card as is.
struct EV_SAVE_STREAM                   /* 0x30 */
{
    /* 0x00 */ u_char set_flg;          // 1: in use
    /* 0x01 */ u_char type;             // EV_SAVE_STREAM_*
    /* 0x02 */ u_char _02[2];
    /* 0x04 */ int    stream_id;
    /* 0x08 */ int    file_label;
    /* 0x0c */ int    volume;
    /* 0x10 */ int    obj_type;         // EV_SAVE_STREAM_OBJ: GetObjectPos's type (low byte)
    /* 0x14 */ int    obj_id;
    /* 0x18 */ u_char _18[8];
    /* 0x20 */ float  pos[4];           // EV_SAVE_STREAM_POS
};
static_assert(sizeof(EV_SAVE_STREAM) == 0x30, "EV_SAVE_STREAM is 0x30 bytes");
static_assert(offsetof(EV_SAVE_STREAM, type) == 0x01, "EV_SAVE_STREAM.type");
static_assert(offsetof(EV_SAVE_STREAM, stream_id) == 0x04, "EV_SAVE_STREAM.stream_id");
static_assert(offsetof(EV_SAVE_STREAM, file_label) == 0x08, "EV_SAVE_STREAM.file_label");
static_assert(offsetof(EV_SAVE_STREAM, volume) == 0x0c, "EV_SAVE_STREAM.volume");
static_assert(offsetof(EV_SAVE_STREAM, obj_type) == 0x10, "EV_SAVE_STREAM.obj_type");
static_assert(offsetof(EV_SAVE_STREAM, obj_id) == 0x14, "EV_SAVE_STREAM.obj_id");
static_assert(offsetof(EV_SAVE_STREAM, pos) == 0x20, "EV_SAVE_STREAM.pos");

#define EV_SAVE_STREAM_MAX  2
#define EV_SAVE_STREAM_2D   0
#define EV_SAVE_STREAM_OBJ  1               // at an object (STREAM3D_OBJ_PLAY)
#define EV_SAVE_STREAM_POS  2               // at a position (STREAM3D_POS_PLAY)

// ── effect save blocks (sdata 0x3730e0 / 0x3730e8, save blocks 15 / 14) ───

// The dither effect EV_EFF_DITHER_START left running (FF2's, unchanged).
struct EV_SAVE_EFF_DITHER               /* 0x6 */
{
    /* 0x0 */ u_char set_flg;
    /* 0x1 */ u_char type;
    /* 0x2 */ u_char speed;
    /* 0x3 */ u_char alpha;
    /* 0x4 */ u_char alpha_max;
    /* 0x5 */ u_char col_max;
};
static_assert(sizeof(EV_SAVE_EFF_DITHER) == 0x6, "EV_SAVE_EFF_DITHER is 6 bytes");
static_assert(offsetof(EV_SAVE_EFF_DITHER, alpha_max) == 0x4, "EV_SAVE_EFF_DITHER.alpha_max");
static_assert(offsetof(EV_SAVE_EFF_DITHER, col_max) == 0x5, "EV_SAVE_EFF_DITHER.col_max");

// FF2 EV_SAVE_SCREEN_EFFECT plus a word FADE_OUT sets and FADE_IN clears
// (with lock table id 1).
struct EV_SAVE_SCREEN_EFFECT            /* 0x8 */
{
    /* 0x0 */ int fade_lock;
    /* 0x4 */ int eff_number;           // EffectSetScreenEffectNo
};
static_assert(sizeof(EV_SAVE_SCREEN_EFFECT) == 0x8, "EV_SAVE_SCREEN_EFFECT is 8 bytes");
static_assert(offsetof(EV_SAVE_SCREEN_EFFECT, eff_number) == 0x4, "EV_SAVE_SCREEN_EFFECT.eff_number");

// ── wakeup / sleep movies: ingame/sleep_movie.h (FINDER_BANK_WRK, GetFinderBankWrk)

// ── called from other files ───────────────────────────────────────────────

void AreaChange(int arg0, int arg1);                    // FF3 0x0019f110: lock ids 0xc/3/5/0xe/0xf/0x10 (+9 when arg0)
void AreaChangeEnd(int dec);                            // FF3 0x0019f1e0: its undo (and 9): reset (0) or count down
int  EvMsgCtrlInit(void);                               // FF3 0x0019e950: EventInit; returns 0
void EvMsgCtrlRelease(void);                            // FF3 0x0019e960: EventRelease
void EventMacroLoad_EffDitherInit(void);                // FF3 0x0019fbe0: after a load, the saved dither effect again

int  EvSaveScreenEffectSaveGet(void **addr);            // FF3 0x0019fb18: {ev_save_screen_effect, 8}
int  EvSaveEffDitherSaveGet(void **addr);               // FF3 0x0019fb28: {ev_save_eff_dither, 6}
void EvEffCtrlInit(void);                               // FF3 0x0019fb38
void SetEvEffDitherID(int no, void *dither_id);         // FF3 0x0019fb58
void DelEvEffDitherID(int no);                          // FF3 0x0019fb70
void *GetEvEffDitherID(int no);                         // FF3 0x0019fb88
void EventMacroLoad_ScreenEffectInit(void);             // FF3 0x0019fba0
void EvSaveScreenEffectInit(void);                      // FF3 0x0019fbc0
void SetEvSaveScreenEffect(int eff_number);             // FF3 0x0019fbd0
void SetEvSaveEffDither(u_char type, u_char speed, u_char alpha,
                        u_char alpha_max, u_char col_max);  // FF3 0x0019fc48
void EvSaveEffDitherInit(void);                         // FF3 0x0019fc90

void EvSoundCtrlInit(void);                             // FF3 0x001a2fb0
void EvSoundPause(void);                                // FF3 0x001a30c8: SndBufPause every event SE
void EvSoundRestart(void);                              // FF3 0x001a3120: SndBufRestart every event SE
int  EvSaveStreamSaveGet(void **addr);                  // FF3 0x001a33b8: {ev_save_stream, 0x60}
int  EvStreamIsPreload(int file_label);                 // FF3 0x001a35d0
void EvStreamCtrlInit(void);                            // FF3 0x001a3598
void EventMacroLoad_StreamInit(void);                   // FF3 0x001a3610: restart the saved streams after a load
void EvSaveStreamInit(void);                            // FF3 0x001a3700
int  EvStreamPlaySub(int file_label, int priority, int effect, int loop, int vol,
                     int in_time, float *pos, int pitch);   // FF3 0x002b8f70

// The command handlers written so far (rows of the command table 0x3556b8).
int  EvSoundPlay(EV_EXE_CTRL *ctrl);                    // FF3 0x001a3250  SOUND_PLAY
int  EvSoundStop(EV_EXE_CTRL *ctrl);                    // FF3 0x001a3278  SOUND_STOP, SOUND3D_*_STOP
int  EvSound3dObjPlay(EV_EXE_CTRL *ctrl);               // FF3 0x001a32a0  SOUND3D_OBJ_PLAY
int  EvSound3dPosPlay(EV_EXE_CTRL *ctrl);               // FF3 0x001a3310  SOUND3D_POS_PLAY
int  EvBgSeStop(EV_EXE_CTRL *ctrl);                     // FF3 0x001a3390  BG_SE_STOP
int  EvStreamPreload(EV_EXE_CTRL *ctrl);                // FF3 0x001a38d0  STREAM_PRELOAD
int  EvStreamPlay(EV_EXE_CTRL *ctrl);                   // FF3 0x001a3958  STREAM_PLAY
int  EvStreamStop(EV_EXE_CTRL *ctrl);                   // FF3 0x001a39f8  STREAM_STOP, STREAM3D_*_STOP
int  EvMapStreamPlay(EV_EXE_CTRL *ctrl);                // FF3 0x001a3a20  MAP_STREAM_PLAY
int  EvMapStreamStop(EV_EXE_CTRL *ctrl);                // FF3 0x001a3a48  MAP_STREAM_STOP
int  EvMapStreamChange(EV_EXE_CTRL *ctrl);              // FF3 0x001a3a70  EV_MAP_STREAM_CHAHGE
int  EvStream3dObjPlay(EV_EXE_CTRL *ctrl);              // FF3 0x001a3a98  STREAM3D_OBJ_PLAY
int  EvStream3dPosPlay(EV_EXE_CTRL *ctrl);              // FF3 0x001a3b70  STREAM3D_POS_PLAY
int  EvStreamAllStop(EV_EXE_CTRL *ctrl);                // FF3 0x001a3c38  STREAM_ALL_STOP
int  EvSetBgmList(EV_EXE_CTRL *ctrl);                   // FF3 0x001a3c60  SET_BGM_LIST
int  EvTapeStart(EV_EXE_CTRL *ctrl);                    // FF3 0x001a3c88  TAPE_START
int  EvTelStart(EV_EXE_CTRL *ctrl);                     // FF3 0x001a3d08  TEL_START
int  EvTelStop(EV_EXE_CTRL *ctrl);                      // FF3 0x001a3d30  TEL_STOP
int  EvTelVolOff(EV_EXE_CTRL *ctrl);                    // FF3 0x001a3d50  TEL_VOL_OFF
int  EvTelVolOn(EV_EXE_CTRL *ctrl);                     // FF3 0x001a3d70  TEL_VOL_ON
int  EvTelTalkStart(EV_EXE_CTRL *ctrl);                 // FF3 0x001a3d98  TEL_TALK_START
int  EvTelTalkStop(EV_EXE_CTRL *ctrl);                  // FF3 0x001a3dc0  TEL_TALK_STOP
int  EvSetWork(EV_EXE_CTRL *ctrl);                      // FF3 0x001a3de0  EV_SET_WORK
int  EvAddWork(EV_EXE_CTRL *ctrl);                      // FF3 0x001a3df8  EV_ADD_WORK
int  EvSubWork(EV_EXE_CTRL *ctrl);                      // FF3 0x001a3e18  EV_SUB_WORK


// ── handlers 0x19e538..0x1a0760 (2D windows, messages, subtitles, event
// camera, player, flow control, doors, effects, fades, locks) ──────────
int  EvMoviePlay(EV_EXE_CTRL *ctrl);                    // FF3 0x0019e538  MOVIE_PLAY
int  EvDisp2DStart(EV_EXE_CTRL *ctrl);                  // FF3 0x0019e560  DISP2D_START
int  EvDisp2DEnd(EV_EXE_CTRL *ctrl);                    // FF3 0x0019e650  DISP2D_END
int  EvChapterDispStart(EV_EXE_CTRL *ctrl);             // FF3 0x0019e6d8  CHAPTER_DISP_START
int  EvDispMsg(EV_EXE_CTRL *ctrl);                      // FF3 0x0019e820  MSG_DISP
int  EvMsgDispSel(EV_EXE_CTRL *ctrl);                   // FF3 0x0019e838  MSG_DISP_SEL
int  EvSubTitleDispReq(EV_EXE_CTRL *ctrl);              // FF3 0x0019e878  SUBTITLE_DISP_REQ
int  EvSubTitle3DObjDispReq(EV_EXE_CTRL *ctrl);         // FF3 0x0019e8a0  SUBTITLE3D_OBJ_DISP_REQ
int  EvSubTitle3DPosDispReq(EV_EXE_CTRL *ctrl);         // FF3 0x0019e8d0  SUBTITLE3D_POS_DISP_REQ
int  EvSubTitleStop(EV_EXE_CTRL *ctrl);                 // FF3 0x0019e930  SUBTITLE_STOP
int  EvEvCam(EV_EXE_CTRL *ctrl);                        // FF3 0x0019e9c8  EV_CAM
int  EvSetEvCamVCI(EV_EXE_CTRL *ctrl);                  // FF3 0x0019ea50  SET_EV_CAM_VCI
int  EvDeleteEvCamVci(EV_EXE_CTRL *ctrl);               // FF3 0x0019eac0  DELETE_EV_CAM_VCI
int  EvSetEvCamVP(EV_EXE_CTRL *ctrl);                   // FF3 0x0019eae8  SET_EV_CAM_VP
int  EvSetEvCamVR(EV_EXE_CTRL *ctrl);                   // FF3 0x0019eb48  SET_EV_CAM_VR
int  EvSetEvCamRot(EV_EXE_CTRL *ctrl);                  // FF3 0x0019eba8  SET_EV_CAM_ROT
int  EvSetEvCamProj(EV_EXE_CTRL *ctrl);                 // FF3 0x0019ebe8  SET_EV_CAM_PROJ
int  EvCamVPSetObj(EV_EXE_CTRL *ctrl);                  // FF3 0x0019ec28  EV_CAM_VP_SET_OBJ
int  EvCamVRSetObj(EV_EXE_CTRL *ctrl);                  // FF3 0x0019ed18  EV_CAM_VR_SET_OBJ
int  EvCamSetWorldSwitch(EV_EXE_CTRL *ctrl);            // FF3 0x0019ee08  EV_CAM_SET_WORLD_SWITCH
int  EvCamChangeType(EV_EXE_CTRL *ctrl);                // FF3 0x0019ee30  CAM_CHANGE_TYPE
int  EvMsgChoice(EV_EXE_CTRL *ctrl);                    // FF3 0x0019eee0  MSG_CHOICE
int  EvSetPlayerHit(EV_EXE_CTRL *ctrl);                 // FF3 0x0019f040  SET_PLAYER_HIT
int  EvSetDreamPlayer(EV_EXE_CTRL *ctrl);               // FF3 0x0019f090  SET_DREAM_PLAYER
int  EvPlyrPad(EV_EXE_CTRL *ctrl);                      // FF3 0x0019f0c0  PLYR_PAD
int  EvPlyrLock(EV_EXE_CTRL *ctrl);                     // FF3 0x0019f288  PLYR_LOCK
int  EvPlyrFloorChange(EV_EXE_CTRL *ctrl);              // FF3 0x0019f328  PLYR_FLOOR_CHANGE
int  EvHide(EV_EXE_CTRL *ctrl);                         // FF3 0x0019f350  HIDE
int  EvSetCandlePower(EV_EXE_CTRL *ctrl);               // FF3 0x0019f378  SET_CANDLE_POWER
int  EvSetCandleEnable(EV_EXE_CTRL *ctrl);              // FF3 0x0019f3e8  SET_CANDLE_ENABLE
int  EvPlyrFinderMode(EV_EXE_CTRL *ctrl);               // FF3 0x0019f430  PLYR_FINDER_MODE
int  EvPlyrDamageRequest(EV_EXE_CTRL *ctrl);            // FF3 0x0019f528  PLYR_DAMAGE_REQUEST
int  EvPlyrFlashLightSet(EV_EXE_CTRL *ctrl);            // FF3 0x0019f530  PLYR_FLASHLIGHT_SET
int  EvPlyrGazePointObjSet(EV_EXE_CTRL *ctrl);          // FF3 0x0019f658  PLYR_GAZE_POINT_OBJ_SET
int  EvPlyrGazePointPosSet(EV_EXE_CTRL *ctrl);          // FF3 0x0019f660  PLYR_GAZE_POINT_POS_SET, PLYR_GAZE_POINT_DEF_SET
int  EvSetPlyrLookDirect(EV_EXE_CTRL *ctrl);            // FF3 0x0019f670  SET_PLYR_LOOK_DIRECT
int  EvStopPlyrLookDirect(EV_EXE_CTRL *ctrl);           // FF3 0x0019f6d8  STOP_PLYR_LOOK_DIRECT
int  EvReqPlSurprise(EV_EXE_CTRL *ctrl);                // FF3 0x0019f6f8  EV_REQ_PL_SURPRISE
int  EvChangeTattooLevel(EV_EXE_CTRL *ctrl);            // FF3 0x0019f718  CHANGE_TATTOO_LEVEL
int  EvCineMenuStart(EV_EXE_CTRL *ctrl);                // FF3 0x0019f760  CINE_MENU_START
int  EvCineFilmPlay(EV_EXE_CTRL *ctrl);                 // FF3 0x0019f780  CINE_FILM_PLAY
int  EvCineFilmStop(EV_EXE_CTRL *ctrl);                 // FF3 0x0019f7c8  CINE_FILM_STOP
int  EvEvSetState(EV_EXE_CTRL *ctrl);                   // FF3 0x0019f7e8  EV_SET_STATE
int  EvEvSetTimer(EV_EXE_CTRL *ctrl);                   // FF3 0x0019f818  EV_SET_TIMER
int  EvEvStop(EV_EXE_CTRL *ctrl);                       // FF3 0x0019f848  EV_STOP
int  EvSetConditionCheck(EV_EXE_CTRL *ctrl);            // FF3 0x0019f8a8  SET_CONDITION_CHECK
int  EvIf(EV_EXE_CTRL *ctrl);                           // FF3 0x0019f8f0  IF_
int  EvElse(EV_EXE_CTRL *ctrl);                         // FF3 0x0019f960  ELSE_
int  EvElseif(EV_EXE_CTRL *ctrl);                       // FF3 0x0019f988  ELSEIF_
int  EvEndif(EV_EXE_CTRL *ctrl);                        // FF3 0x0019f9f0  ENDIF_
int  EvEventReboot(EV_EXE_CTRL *ctrl);                  // FF3 0x0019fa00  EVENT_REBOOT
int  EvDoorOpen(EV_EXE_CTRL *ctrl);                     // FF3 0x0019fa68  DOOR_OPEN
int  EvDoorClose(EV_EXE_CTRL *ctrl);                    // FF3 0x0019fa70  DOOR_CLOSE
int  EvPrivateDoorLock(EV_EXE_CTRL *ctrl);              // FF3 0x0019fa78  PRIVATE_DOOR_LOCK
int  EvPrivateDoorUnlock(EV_EXE_CTRL *ctrl);            // FF3 0x0019faf0  PRIVATE_DOOR_UNLOCK
int  EvSetEffVisible(EV_EXE_CTRL *ctrl);                // FF3 0x0019fcb8  SET_EFF_VISIBLE
int  EvEffThunderReq(EV_EXE_CTRL *ctrl);                // FF3 0x0019fd80  EV_EFF_THUNDER_REQ
int  EvSetScreenEffect(EV_EXE_CTRL *ctrl);              // FF3 0x0019fe70  EV_SET_SCREEN_EFFECT
int  EvSetButterfly(EV_EXE_CTRL *ctrl);                 // FF3 0x0019fea8  SET_BUTTERFLY
int  EvMoveButterfly(EV_EXE_CTRL *ctrl);                // FF3 0x0019ff10  MOVE_BUTTERFLY
int  EvReleaseButterfly(EV_EXE_CTRL *ctrl);             // FF3 0x0019ff70  RELEASE_BUTTERFLY
int  EvEffDitherStart(EV_EXE_CTRL *ctrl);               // FF3 0x0019ff98  EV_EFF_DITHER_START
int  EvEffDitherEnd(EV_EXE_CTRL *ctrl);                 // FF3 0x001a0098  EV_EFF_DITHER_END
int  EvSynchroModeStart(EV_EXE_CTRL *ctrl);             // FF3 0x001a00d8  SYNCHRO_MODE_START
int  EvSynchroModeEnd(EV_EXE_CTRL *ctrl);               // FF3 0x001a00e0  SYNCHRO_MODE_END
int  EvOverLapStart(EV_EXE_CTRL *ctrl);                 // FF3 0x001a00e8  EV_OVER_LAP_START
int  EvOverLapEnd(EV_EXE_CTRL *ctrl);                   // FF3 0x001a0120  EV_OVER_LAP_END
int  EvFadeIn(EV_EXE_CTRL *ctrl);                       // FF3 0x001a0140  FADE_IN
int  EvFadeOut(EV_EXE_CTRL *ctrl);                      // FF3 0x001a0188  FADE_OUT
int  EvMonoDisp(EV_EXE_CTRL *ctrl);                     // FF3 0x001a01d0  MONO_DISP
int  EvFurnCandleBlazeUp(EV_EXE_CTRL *ctrl);            // FF3 0x001a0220  FURN_CANDLE_BLAZE_UP
int  EvSetHandlighteffectDisp(EV_EXE_CTRL *ctrl);       // FF3 0x001a0248  SET_HANDLIGHTEFFECT_DISP
int  EvFadeHighcontrast(EV_EXE_CTRL *ctrl);             // FF3 0x001a0270  FADE_HIGHCONTRAST
int  EvSetLanterneffectEnable(EV_EXE_CTRL *ctrl);       // FF3 0x001a0298  SET_LANTERNEFFECT_ENABLE
int  EvEvFog(EV_EXE_CTRL *ctrl);                        // FF3 0x001a02c0  EV_FOG
int  EvSetEvFogColor(EV_EXE_CTRL *ctrl);                // FF3 0x001a02e8  SET_EV_FOG_COLOR
int  EvSetEvFogDistNear(EV_EXE_CTRL *ctrl);             // FF3 0x001a0340  SET_EV_FOG_DIST_NEAR
int  EvSetEvFogDistFar(EV_EXE_CTRL *ctrl);              // FF3 0x001a0380  SET_EV_FOG_DIST_FAR
int  EvSetEvFogDist(EV_EXE_CTRL *ctrl);                 // FF3 0x001a03c0  SET_EV_FOG_DIST
int  EvPhotoLock(EV_EXE_CTRL *ctrl);                    // FF3 0x001a0408  PHOTO_LOCK
int  EvPhotoUnlock(EV_EXE_CTRL *ctrl);                  // FF3 0x001a0428  PHOTO_UNLOCK
int  EvMenuLock(EV_EXE_CTRL *ctrl);                     // FF3 0x001a0448  MENU_LOCK
int  EvMenuUnlock(EV_EXE_CTRL *ctrl);                   // FF3 0x001a0468  MENU_UNLOCK
int  EvPauseLock(EV_EXE_CTRL *ctrl);                    // FF3 0x001a0488  PAUSE_LOCK
int  EvPauseUnlock(EV_EXE_CTRL *ctrl);                  // FF3 0x001a04a8  PAUSE_UNLOCK
int  EvSetSpGameover(EV_EXE_CTRL *ctrl);                // FF3 0x001a04c8  SET_SP_GAMEOVER_
int  EvSetNightLockFlg(EV_EXE_CTRL *ctrl);              // FF3 0x001a0518  SET_NIGHT_LOCK_FLG
int  EvSetRadioLockFlg(EV_EXE_CTRL *ctrl);              // FF3 0x001a0568  SET_RADIO_LOCK_FLG
int  EvSetMapInLockFlg(EV_EXE_CTRL *ctrl);              // FF3 0x001a05b8  SET_MAP_IN_LOCK_FLG
int  EvSetPhotoCurse(EV_EXE_CTRL *ctrl);                // FF3 0x001a0608  SET_PHOTO_CURSE
int  EvFilamentTimerCall(EV_EXE_CTRL *ctrl);            // FF3 0x001a0610  FILAMENT_TIMER_CALL
int  EvFilamentCall(EV_EXE_CTRL *ctrl);                 // FF3 0x001a0618  FILAMENT_CALL
int  EvFilamentRelease(EV_EXE_CTRL *ctrl);              // FF3 0x001a0620  FILAMENT_RELEASE
int  EvSetObjAlgorithm(EV_EXE_CTRL *ctrl);              // FF3 0x001a0628  SET_OBJ_ALGORITHM
int  EvSetGhostSealDoor(EV_EXE_CTRL *ctrl);             // FF3 0x001a0680  SET_GHOST_SEAL_DOOR
int  EvReleaseGhostSealDoor(EV_EXE_CTRL *ctrl);         // FF3 0x001a0688  RELEASE_GHOST_SEAL_DOOR
int  EvCallGhostSealDoorBurn(EV_EXE_CTRL *ctrl);        // FF3 0x001a0690  CALL_GHOST_SEAL_DOOR_BURN
int  EvPadWait(EV_EXE_CTRL *ctrl);                      // FF3 0x001a0760  EV_PAD_WAIT

// ── handlers 0x1a07a0..0x1a2df0 (worlds, research, radio, wakeup / sleep
// movies, missions, ghosts, items, files, memos, areas, the player's
// position / anime, NPCs, map object rows) ──────────────────────────────
int  EvChangeWorld(EV_EXE_CTRL *ctrl);                  // FF3 0x001a07a0  EV_CHANGE_WORLD
int  EvSetSleepFlg(EV_EXE_CTRL *ctrl);                  // FF3 0x001a07c0  SET_SLEEP_FLG_
int  EvSetNightFlg(EV_EXE_CTRL *ctrl);                  // FF3 0x001a0820  SET_NIGHT_FLG
int  EvSetNightStartPos(EV_EXE_CTRL *ctrl);             // FF3 0x001a08a0  SET_NIGHT_START_POS
int  EvGoDreamWorld(EV_EXE_CTRL *ctrl);                 // FF3 0x001a08e0  GO_DREAM_WORLD
int  EvGoRealWorld(EV_EXE_CTRL *ctrl);                  // FF3 0x001a0ae0  GO_REAL_WORLD
int  EvDevelopRequest(EV_EXE_CTRL *ctrl);               // FF3 0x001a0d58  DEVELOP_REQUEST
int  EvResearchPermit(EV_EXE_CTRL *ctrl);               // FF3 0x001a0d78  RESEARCH_PERMIT
int  EvResearchStart(EV_EXE_CTRL *ctrl);                // FF3 0x001a0da0  RESEARCH_START
int  EvRadioTurnOn(EV_EXE_CTRL *ctrl);                  // FF3 0x001a0e20  RADIO_TURN_ON
int  EvRadioTurnOff(EV_EXE_CTRL *ctrl);                 // FF3 0x001a0e48  RADIO_TURN_OFF
int  EvSetWakeupMovie(EV_EXE_CTRL *ctrl);               // FF3 0x001a0e70  SET_WAKEUP_MOVIE
int  EvWakeupMoviePlay(EV_EXE_CTRL *ctrl);              // FF3 0x001a0ed0  WAKEUP_MOVIE_PLAY
int  EvSetSleepMovie(EV_EXE_CTRL *ctrl);                // FF3 0x001a0f50  SET_SLEEP_MOVIE
int  EvSleepMoviePlay(EV_EXE_CTRL *ctrl);               // FF3 0x001a0fb0  SLEEP_MOVIE_PLAY
int  EvSetRealCamera(EV_EXE_CTRL *ctrl);                // FF3 0x001a1040  SET_REAL_CAMERA
int  EvMapUpdate(EV_EXE_CTRL *ctrl);                    // FF3 0x001a10c0  MAP_UPDATE
int  EvSetSpEvent(EV_EXE_CTRL *ctrl);                   // FF3 0x001a10e8  SET_SP_EVENT
int  EvMissionSetup(EV_EXE_CTRL *ctrl);                 // FF3 0x001a1118  MISSION_SETUP
int  EvCaptionSet(EV_EXE_CTRL *ctrl);                   // FF3 0x001a1218  CAPTION_SET
int  EvSetMuteSeVolume(EV_EXE_CTRL *ctrl);              // FF3 0x001a1280  SET_MUTE_SE_VOLUME
int  EvGhostAppear(EV_EXE_CTRL *ctrl);                  // FF3 0x001a12d8  GHOST_APPEAR
int  EvGhostDisappear(EV_EXE_CTRL *ctrl);               // FF3 0x001a1378  GHOST_DISAPPEAR
int  EvGhostRegist(EV_EXE_CTRL *ctrl);                  // FF3 0x001a13d0  GHOST_REGIST
int  EvGhostPreloadRegist(EV_EXE_CTRL *ctrl);           // FF3 0x001a13e8  GHOST_PRELOAD_REGIST
int  EvGhostDelete(EV_EXE_CTRL *ctrl);                  // FF3 0x001a1400  GHOST_DELETE
int  EvRandomGhostOn(EV_EXE_CTRL *ctrl);                // FF3 0x001a1430  RANDOM_GHOST_ON
int  EvRandomGhostOff(EV_EXE_CTRL *ctrl);               // FF3 0x001a1458  RANDOM_GHOST_OFF
int  EvRandomGhostAllOff(EV_EXE_CTRL *ctrl);            // FF3 0x001a1480  RANDOM_GHOST_ALL_OFF
int  EvRandomGhostReq(EV_EXE_CTRL *ctrl);               // FF3 0x001a14a0  RANDOM_GHOST_REQ
int  EvChangeGhostAttr(EV_EXE_CTRL *ctrl);              // FF3 0x001a14c0  CHANGE_GHOST_ATTR
int  EvItemGet(EV_EXE_CTRL *ctrl);                      // FF3 0x001a1528  ITEM_GET
int  EvItemUse(EV_EXE_CTRL *ctrl);                      // FF3 0x001a1550  ITEM_USE
int  EvItemLost(EV_EXE_CTRL *ctrl);                     // FF3 0x001a1578  ITEM_LOST
int  EvItemReset(EV_EXE_CTRL *ctrl);                    // FF3 0x001a15a0  ITEM_RESET
int  EvItemTableChange(EV_EXE_CTRL *ctrl);              // FF3 0x001a1620  ITEM_TABLE_CHANGE_
int  EvFileGet(EV_EXE_CTRL *ctrl);                      // FF3 0x001a1638  FILE_GET
int  EvFileLost(EV_EXE_CTRL *ctrl);                     // FF3 0x001a1660  FILE_LOST
int  EvFileRead(EV_EXE_CTRL *ctrl);                     // FF3 0x001a1688  FILE_READ
int  EvCrystalLost(EV_EXE_CTRL *ctrl);                  // FF3 0x001a1758  CRYSTAL_LOST
int  EvLevelGemGet(EV_EXE_CTRL *ctrl);                  // FF3 0x001a1760  LEVELGEM_GET
int  EvLevelGemLost(EV_EXE_CTRL *ctrl);                 // FF3 0x001a1768  LEVELGEM_LOST
int  EvCamSpecialShotGet(EV_EXE_CTRL *ctrl);            // FF3 0x001a1770  CAM_SPECIALSHOT_GET
int  EvGetCharStat(EV_EXE_CTRL *ctrl);                  // FF3 0x001a17c0  GET_CHAR_STAT
int  EvCamEquipFunctionGet(EV_EXE_CTRL *ctrl);          // FF3 0x001a1828  CAM_EQUIP_FUNCTION_GET
int  EvMemoUpdate(EV_EXE_CTRL *ctrl);                   // FF3 0x001a1868  MEMO_UPDATE
int  EvMemoOpen(EV_EXE_CTRL *ctrl);                     // FF3 0x001a1898  MEMO_OPEN
int  EvSetMemoOpenFalg(EV_EXE_CTRL *ctrl);              // FF3 0x001a18c0  SET_MEMO_OPEN_FALG
int  EvMemoMenuOpen(EV_EXE_CTRL *ctrl);                 // FF3 0x001a18e0  MEMO_MENU_OPEN
int  EvShootDevelopFilm(EV_EXE_CTRL *ctrl);             // FF3 0x001a1918  SHOOT_DEVELOP_FILM
int  EvGetDevelopFilm(EV_EXE_CTRL *ctrl);               // FF3 0x001a1970  GET_DEVELOP_FILM
int  EvSoundLoad(EV_EXE_CTRL *ctrl);                    // FF3 0x001a1998  SOUND_LOAD
int  EvSoundPreloadRegist(EV_EXE_CTRL *ctrl);           // FF3 0x001a19c8  SOUND_PRELOAD_REGIST
int  EvSoundRelease(EV_EXE_CTRL *ctrl);                 // FF3 0x001a19f8  EV_SOUND_RELEASE
int  EvLoadRequest(EV_EXE_CTRL *ctrl);                  // FF3 0x001a1a20  LOAD_REQUEST
int  EvReleaseRequest(EV_EXE_CTRL *ctrl);               // FF3 0x001a1a28  RELEASE_REQUEST
int  EvChapterLoadRequest(EV_EXE_CTRL *ctrl);           // FF3 0x001a1a68  CHAPTER_LOAD_REQUEST
int  EvSetHaichiId(EV_EXE_CTRL *ctrl);                  // FF3 0x001a1a90  SET_HAICHI_ID
int  EvSetEventRecHit(EV_EXE_CTRL *ctrl);               // FF3 0x001a1b18  SET_EVENT_REC_HIT
int  EvAreaChange(EV_EXE_CTRL *ctrl);                   // FF3 0x001a1b90  AREA_CHANGE
int  EvSet2dmapDraw(EV_EXE_CTRL *ctrl);                 // FF3 0x001a1c48  SET_2DMAP_DRAW
int  EvBackGroundLoad(EV_EXE_CTRL *ctrl);               // FF3 0x001a1c70  BACK_GROUND_LOAD
int  EvAreaSwitch(EV_EXE_CTRL *ctrl);                   // FF3 0x001a1c78  AREA_SWITCH
int  EvPuzzleStart(EV_EXE_CTRL *ctrl);                  // FF3 0x001a1c80  PUZZLE_START
int  EvGameDataSave(EV_EXE_CTRL *ctrl);                 // FF3 0x001a1cb8  GAME_DATA_SAVE
int  EvGameClear(EV_EXE_CTRL *ctrl);                    // FF3 0x001a1ce8  GAME_CLEAR_
int  EvGameOverRequest(EV_EXE_CTRL *ctrl);              // FF3 0x001a1e28  GAMEOVER_REQUEST
int  EvMissionStart(EV_EXE_CTRL *ctrl);                 // FF3 0x001a1e48  EV_MISSION_START
int  EvMissionClear(EV_EXE_CTRL *ctrl);                 // FF3 0x001a1e70  EV_MISSION_CLEAR
int  EvMissionFailed(EV_EXE_CTRL *ctrl);                // FF3 0x001a1f00  EV_MISSION_FAILED
int  EvChangeChapter(EV_EXE_CTRL *ctrl);                // FF3 0x001a1f30  CHANGE_CHAPTER
int  EvChapterSelInit(EV_EXE_CTRL *ctrl);               // FF3 0x001a1f50  CHAPTER_SEL_INIT
int  EvPlyrPosSet(EV_EXE_CTRL *ctrl);                   // FF3 0x001a2028  PLYR_POS_SET
int  EvPlyrHeightSet(EV_EXE_CTRL *ctrl);                // FF3 0x001a2130  PLYR_HEIGHT_SET
int  EvPlyrRotSet(EV_EXE_CTRL *ctrl);                   // FF3 0x001a21b8  PLYR_ROT_SET
int  EvPlyrPosMove(EV_EXE_CTRL *ctrl);                  // FF3 0x001a2230  PLYR_POS_MOVE
int  EvPlyrDisp(EV_EXE_CTRL *ctrl);                     // FF3 0x001a2238  PLYR_DISP
int  EvPlyrMotionCall(EV_EXE_CTRL *ctrl);               // FF3 0x001a2288  PLYR_MOTION_CALL
int  EvPlyrMotionModeEnd(EV_EXE_CTRL *ctrl);            // FF3 0x001a2350  PLYR_MOTION_MODE_END
int  EvPlyrFacialCall(EV_EXE_CTRL *ctrl);               // FF3 0x001a2378  PLYR_FACIAL_CALL
int  EvPlyrMotionChange(EV_EXE_CTRL *ctrl);             // FF3 0x001a2380  PLYR_MOTION_CHANGE
int  EvPlyrModelChange(EV_EXE_CTRL *ctrl);              // FF3 0x001a2388  PLYR_MODEL_CHANGE
int  EvPlayerActFours(EV_EXE_CTRL *ctrl);               // FF3 0x001a2438  PLAYER_ACT_FOURS
int  EvGoUpLadder(EV_EXE_CTRL *ctrl);                   // FF3 0x001a24d0  GO_UP_LADDER
int  EvGoDownLadder(EV_EXE_CTRL *ctrl);                 // FF3 0x001a24f8  GO_DOWN_LADDER
int  EvNpcSerifTbl(EV_EXE_CTRL *ctrl);                  // FF3 0x001a2520  NPC_SERIF_TBL
int  EvNpcFacialTbl(EV_EXE_CTRL *ctrl);                 // FF3 0x001a2548  NPC_FACIAL_TBL
int  EvNpcFixEvent(EV_EXE_CTRL *ctrl);                  // FF3 0x001a2570  NPC_FIX_EVENT
int  EvNpcSetPos(EV_EXE_CTRL *ctrl);                    // FF3 0x001a25f0  NPC_SET_POS
int  EvNpcSetPosLabel(EV_EXE_CTRL *ctrl);               // FF3 0x001a26e0  NPC_SET_POS_LABEL
int  EvNpcMotionCall(EV_EXE_CTRL *ctrl);                // FF3 0x001a2740  NPC_MOTION_CALL
int  EvNpcFacialCall(EV_EXE_CTRL *ctrl);                // FF3 0x001a2818  NPC_FACIAL_CALL
int  EvNpcActiveFlg(EV_EXE_CTRL *ctrl);                 // FF3 0x001a2858  NPC_ACTIVE_FLG
int  EvNpcModelChange(EV_EXE_CTRL *ctrl);               // FF3 0x001a28b8  NPC_MODEL_CHANGE
int  EvNpcEyeTarget(EV_EXE_CTRL *ctrl);                 // FF3 0x001a2938  NPC_EYE_TARGET
int  EvNpcLockPos(EV_EXE_CTRL *ctrl);                   // FF3 0x001a2a30  NPC_LOCK_POS
int  EvSetObjHitCheck(EV_EXE_CTRL *ctrl);               // FF3 0x001a2a98  SET_OBJ_HITCHECK
int  EvSetObjVisible(EV_EXE_CTRL *ctrl);                // FF3 0x001a2ae0  SET_OBJ_VISIBLE
int  EvSetObjAlpha(EV_EXE_CTRL *ctrl);                  // FF3 0x001a2ba0  SET_OBJ_ALPHA
int  EvSetObjReqAction(EV_EXE_CTRL *ctrl);              // FF3 0x001a2be0  SET_OBJ_REQ_ACTION
int  EvSetObjActionType(EV_EXE_CTRL *ctrl);             // FF3 0x001a2c20  SET_OBJ_ACTIONTYPE
int  EvSetFurnWind(EV_EXE_CTRL *ctrl);                  // FF3 0x001a2c98  SET_FURN_WIND
int  EvSetPlyrLookFurn(EV_EXE_CTRL *ctrl);              // FF3 0x001a2cf0  SET_PLYR_LOOK_FURN
int  EvPeepingRequest(EV_EXE_CTRL *ctrl);               // FF3 0x001a2d40  PEEPING_REQUEST
int  EvSetPhotoable(EV_EXE_CTRL *ctrl);                 // FF3 0x001a2d68  SET_PHOTOABLE_
int  EvSetObjEffect(EV_EXE_CTRL *ctrl);                 // FF3 0x001a2de8  SET_OBJ_EFFECT
int  EvSetObjPhotoAble(EV_EXE_CTRL *ctrl);              // FF3 0x001a2df0  SET_OBJ_PHOTOABLE

// Phase 36 (Story_RoomReload, SET_HAICHI_ID on the loaded area).
int  init_Story_RoomReload(void);                       // FF3 0x001a2e40
int  one_Story_RoomReload(void);                        // FF3 0x001a2ee0
int  end_Story_RoomReload(void);                        // FF3 0x001a2eb8

// Conditions (rows of the condition table 0x3242e8): (event id, the
// condition's bytes) -> 1 when true.
int  EvSelectChoice(int event_id, u_char *addr);        // FF3 0x0019f008  SELECT_CHOICE
int  EvNpcIntoPlyrSight(int event_id, u_char *addr);    // FF3 0x001a3e38  NPC_INTO_PLYR_SIGHT
int  EvNpcDistance(int event_id, u_char *addr);         // FF3 0x001a3e90  NPC_DISTANCE
int  EvNpcAreaOut(int event_id, u_char *addr);          // FF3 0x001a3ee8  NPC_AREAOUT, NPC_AREAOUT_DEF
int  EvEvCntExec(int event_id, u_char *addr);           // FF3 0x001a3ef0  EV_CNT_EXEC
int  EvEvState(int event_id, u_char *addr);             // FF3 0x001a3f90  EV_STATE
int  EvEvNotState(int event_id, u_char *addr);          // FF3 0x001a3fd0  EV_NOT_STATE
int  EvEvFailure(int event_id, u_char *addr);           // FF3 0x001a3ff8  EV_FAILURE
int  EvEvWorkCheck(int event_id, u_char *addr);         // FF3 0x001a4000  EV_WORK_CHECK
int  EvDoorStatCheck(int event_id, u_char *addr);       // FF3 0x001a40a0  DOOR_STAT_CHECK
int  EvCondSpGameover(int event_id, u_char *addr);      // FF3 0x001a4108  SP_GAMEOVER_
int  EvSaveIsOk(int event_id, u_char *addr);            // FF3 0x001a4160  SAVE_IS_OK
int  EvCheckGameDifficulty(int event_id, u_char *addr); // FF3 0x001a4180  CHECK_GAME_DIFFICULTY
int  EvCheckMission(int event_id, u_char *addr);        // FF3 0x001a41b8  CHECK_MISSION
int  EvAboveClearNum(int event_id, u_char *addr);       // FF3 0x001a4218  ABOVE_CLEAR_NUM
int  EvFollowClearNum(int event_id, u_char *addr);      // FF3 0x001a4268  FOLLOW_CLEAR_NUM
int  EvNowChapter(int event_id, u_char *addr);          // FF3 0x001a42b8  EV_NOW_CHAPTER
int  EvNotNowChapter(int event_id, u_char *addr);       // FF3 0x001a4358  EV_NOT_NOW_CHAPTER
int  EvGhostExist(int event_id, u_char *addr);          // FF3 0x001a4380  GHOST_EXIST
int  EvGhostLost(int event_id, u_char *addr);           // FF3 0x001a43b8  GHOST_LOST
int  EvGhostReleaseType(int event_id, u_char *addr);    // FF3 0x001a43e0  GHOST_RELEASE_TYPE
int  EvBattleMode(int event_id, u_char *addr);          // FF3 0x001a4418  BATTLE_MODE
int  EvBattleEnd(int event_id, u_char *addr);           // FF3 0x001a4458  BATTLE_END
int  EvPlyrItemHave(int event_id, u_char *addr);        // FF3 0x001a4480  PLYR_ITEM_HAVE
int  EvNotHaveItem(int event_id, u_char *addr);         // FF3 0x001a44d0  EV_NOT_HAVE_ITEM
int  EvPlyrItemUse(int event_id, u_char *addr);         // FF3 0x001a4528  PLYR_ITEM_USE
int  EvGetCamLensEquip(int event_id, u_char *addr);     // FF3 0x001a4530  GET_CAM_LENS_EQUIP
int  EvGetCamPartsEquip(int event_id, u_char *addr);    // FF3 0x001a4570  GET_CAM_PARTS_EQUIP
int  EvRemainUndevelopFilms(int event_id, u_char *addr);// FF3 0x001a45a8  REMAIN_UNDEVELOP_FILMS
int  EvGetDreamPlayer(int event_id, u_char *addr);      // FF3 0x001a4780  GET_DREAM_PLAYER
int  EvPlyrIsSelect(int event_id, u_char *addr);        // FF3 0x001a47d0  PLYR_IS_SELECT
int  EvGetHideStatus(int event_id, u_char *addr);       // FF3 0x001a4820  GET_HIDE_STATUS_
int  EvPlayerStatCheck(int event_id, u_char *addr);     // FF3 0x001a4868  PLAYER_STAT_CHECK
int  EvPlyrAreaIn(int event_id, u_char *addr);          // FF3 0x001a4940  PLYR_AREAIN, PLYR_AREAIN_DEF
int  EvPlyrAreaOut(int event_id, u_char *addr);         // FF3 0x001a49b0  PLYR_AREAOUT, PLYR_AREAOUT_DEF
int  EvPlyrRoomIn(int event_id, u_char *addr);          // FF3 0x001a4a20  PLYR_ROOMIN
int  EvPlyrRoomOut(int event_id, u_char *addr);         // FF3 0x001a4a58  PLYR_ROOMOUT
int  EvPlyrRot(int event_id, u_char *addr);             // FF3 0x001a4a80  PLYR_ROT
int  EvPlyrIntoSight(int event_id, u_char *addr);       // FF3 0x001a4af0  PLYR_INTO_SIGHT

// Helpers of the conditions.
int  EvRotInRange(u_short center, u_short width, short rot);   // FF3 0x001a45c8: rot (+180) in center +- width (degrees)
int  EvPosIntoSight(float *tp, float *vp, float rot, float max_dist, float sight_far,
                    float dist_near, float sight_near);         // FF3 0x001a4708

#endif /* _INGAME_EVENT_PRG_EV_COMMAND_H */
