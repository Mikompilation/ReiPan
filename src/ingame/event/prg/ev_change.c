// ingame/event/prg/ev_change.c -- FF3 0x1992d0..0x19a0a0: the first event
// conditions, the condition table, the event states and the parked state
// changes (see ev_change.h). FF3's file name is unknown (it sorts between
// ev_get.c and ev_macro.c); the second half is FF2's ev_change.c, ported from
// MioPan's ev_change.c with FF2's asserts dropped.
#include "ingame/event/prg/ev_change.h"

#include <math.h>                               // sqrtf
#include <string.h>                             // memset, memcpy

#include "common/get_byte.h"                    // Get1Byte / Get2Byte
#include "ingame/camera/finder_camera.h"        // GetFinderModeWrk
#include "ingame/cine_film.h"                   // CineFilmIsPlay, CineFlagCheck
#include "ingame/cine_menu.h"                    // CineFilmGetHaveNum, CineMenuGetState
#include "ingame/event/telephone.h"         // TelStatusCheck, TelTalkIsEnd
#include "ingame/event/ev_queue.h"              // EvQueueGetOrder
#include "ingame/event/prg/ev_command.h"        // the other conditions, EvStreamIsPreload
#include "ingame/event/prg/ev_chapter.h"        // GetObjectPos
#include "ingame/event/prg/ev_exe.h"            // EvExeRelease, CheckEventExeEntry, SetEventInitStatus
#include "ingame/event/prg/ev_open.h"           // SetEventPriority, EventSetOpenCondition, EventDelCondition, the photo conditions
#include "ingame/ingame_wrk.h"                  // ingame_wrk
#include "ingame/menu/menu_cassette.h"          // CassetteGetHaveNum, CassetteGetState
#include "ingame/photo/photo.h"                 // PeepStateGet
#include "ingame/plyr/player.h"                 // PLYR_WRK, PLYR_MODE, PLYR_FLG_*
#include "ingame/plyr/plyr_sub.h"               // PlyrGetJointRotY
#include "ingame/event/prg/ev_get.h"            // EvGetParentID, EvGetSubId, EV_SUB_ID_MAX
#include "ingame/plyr/unit_ctl.h"               // OutSightChk
#include "ingame/puzzle/puzzle.h"               // GetPuzzleClearInfo
#include "ingame/plyr/plyr_shot.h"           // PlyrGetFinderState
#include "ingame/story_menu.h"                  // IsLockTblOn
#include "reipan/os/reipan_rand.h"              // PC: ReiPan_Rand
#include "system/os/system.h"                   // GetPALMode
#include "system/pad/pad_ctrl.h"                // CheckPadTrgPlaySe, pad_key
#include "ingame/story_door.h"                  // MapAreaSideGet
#include "ingame/story_develop.h"               // develop_film

// Callees in unassigned runs (move to their headers when the runs are defined).

// ══════════════════════════════════════════════════════════════════════
// Conditions (rows of the condition table below): (event id, the
// condition's bytes: +0 its label, then the arguments) -> 1 when true.
// FF2 keeps the ones it has in ev_open.c.
// ══════════════════════════════════════════════════════════════════════

// GET_RESEARCH_STAT "1124": the event queue entry (+1 kind, +2 index) is
// done (2), free (1) or queued (3), compared with +4.
// FF3 0x001992d0

int EvGetResearchStat(int event_id, u_char *addr)
{
    struct { u_char code; s_char kind; short idx; int stat; } a;
    int order;
    int stat;

    (void)event_id;
    memcpy(&a, addr, sizeof(a));
    order = EvQueueGetOrder(a.kind, a.idx);
    if (order == EV_QUEUE_DONE) {
        stat = 2;
    } else if (order == EV_QUEUE_FREE) {
        stat = 1;
    } else {
        stat = 3;
    }
    return a.stat == stat;
}

// GET_DEVELOP_STATUS_ "112": develop row +2 is developed (state 3, operator
// +1 == 0) or not (+1 == 1).
// FF3 0x00199338

int EvGetDevelopStatus(int event_id, u_char *addr)
{
    u_char op = Get1Byte(addr + 1);
    short  id = (short)Get2Byte(addr + 2);

    (void)event_id;
    if (op == 0) {
        return develop_film[id].state == DEVELOP_STATE_DONE;
    }
    if (op == 1) {
        return develop_film[id].state != DEVELOP_STATE_DONE;
    }
    return 0;
}

// PUSH_PAD "22": the game action +2's button went down this frame (with its
// SE), while the player is free: not in the viewfinder, lock 3 off, in mode
// 0, 9 or above 11, and no event holds the game.
// FF3 0x001993d8; FF2 EvPushPad (InFinderMode / InDamageState, paddat).

int EvPushPad(int event_id, u_char *addr)
{
    struct { u_char code; u_char _1; short key; } a;

    (void)event_id;
    memcpy(&a, addr, sizeof(a));
    if (PlyrGetFinderState() != 0) {
        return 0;
    }
    if (IsLockTblOn(3) != 0) {
        return 0;
    }
    switch (ingame_wrk.plyr_wrk->mode) {
    case PLYR_MODE_DMG1:
    case PLYR_MODE_DMG2:
    case PLYR_MODE_DMG3:
    case PLYR_MODE_DMG4:
    case PLYR_MODE_5:
    case PLYR_MODE_DOOR:
    case PLYR_MODE_7:
    case PLYR_MODE_8:
    case PLYR_MODE_10:
    case PLYR_MODE_11:
        return 0;
    }
    if (GetEvWrkWaitFlg() != 0) {
        return 0;
    }
    return CheckPadTrgPlaySe((u_short)pad_key[a.key], -1, 0) != 0;
}

// EV_RANDOM "112": true with +2 percent.
// FF3 0x001994c0; FF2 EvEvRandom.
// callcheck: rand (PC: ReiPan_Rand, the ROM's 31-bit range)

int EvEvRandom(int event_id, u_char *addr)
{
    struct { u_char code; u_char _1; short rate; } a;

    (void)event_id;
    memcpy(&a, addr, sizeof(a));
    // PC: ReiPan_Rand is the ROM's 31-bit rand(); the host's is 15-bit.
    return ReiPan_Rand() % 100 < a.rate;
}

// FINDER_MODE "21": +2 0 / 1 the viewfinder state is 0 / 1; 2 / 3 the
// player's flag 0x400000 / 0x1000000 is set.
// FF3 0x00199508; FF2 EvFinderMode (InFinderMode only).

int EvFinderMode(int event_id, u_char *addr)
{
    struct { u_char code; u_char _1; s_char mode; } a;
    int state;

    (void)event_id;
    memcpy(&a, addr, sizeof(a));
    state = PlyrGetFinderState();
    switch (a.mode) {
    case 0:
        return state == 0;
    case 1:
        return state == 1;
    case 2:
        return (ingame_wrk.plyr_wrk->flags & PLYR_FLG_400000) != 0;
    case 3:
        return (ingame_wrk.plyr_wrk->flags & PLYR_FLG_1000000) != 0;
    }
    return 0;
}

// OBJ_INTO_PLYR_SIGHT "1124": object (+1 type, +4 id) is in the player's
// sight (60 degrees, 40 deep, by the facing of joint 3) and at least 4 away
// on the ground plane (x / z).
// FF3 0x001995c8; FF2 EvObjIntoPlyrSight (FF3 adds the distance).

int EvObjIntoPlyrSight(int event_id, u_char *addr)
{
    struct { u_char code; u_char type; short _2; int id; } a;
    sceVu0FVECTOR pos;
    PLYR_WRK *plyr;
    float dx;
    float dz;

    (void)event_id;
    memcpy(&a, addr, sizeof(a));
    if (GetObjectPos(pos, a.type, a.id) == 0) {
        return 0;
    }
    plyr = ingame_wrk.plyr_wrk;
    if (OutSightChk(pos, plyr->pos, PlyrGetJointRotY(3),
                    1.0471976f,     // sdata 0x3700bc
                    40.0f) != 0) {  // sdata 0x3700c0
        return 0;
    }
    // vsub / vmul .xyz, then vaddz.x: x * x + z * z (y left out).
    dx = pos[0] - plyr->pos[0];
    dz = pos[2] - plyr->pos[2];
    return sqrtf(dx * dx + dz * dz) >= 4.0f;   // sdata 0x3700c4
}

// OBJ_INTO_FINDER "1124": OBJ_INTO_PLYR_SIGHT while the camera is up.
// FF3 0x001996a0

int EvObjIntoFinder(int event_id, u_char *addr)
{
    if ((ingame_wrk.plyr_wrk->flags & PLYR_FLG_800000) != 0) {
        return EvObjIntoPlyrSight(event_id, addr);
    }
    return 0;
}

// CHAPTER_LOAD_WAIT: never true in FF3.
// FF3 0x001996d8

int EvChapterLoadWait(int event_id, u_char *addr)
{
    (void)event_id;
    (void)addr;
    return 0;
}

// SET_EV_PRIORITY "11": the event's priority is +1; always true.
// FF3 0x001996e0; FF2 EvSetEvPriority.

int EvSetEvPriority(int event_id, u_char *addr)
{
    SetEventPriority(event_id, Get1Byte(addr + 1));
    return 1;
}

// PUZZLE_SUCCESS "1111": puzzle (+2 type, +3 index)'s clear flag is +1.
// FF3 0x00199720; FF2 EvPuzzleSuccess.

int EvPuzzleSuccess(int event_id, u_char *addr)
{
    struct { u_char code; u_char clear; u_char type; u_char idx; } a;

    (void)event_id;
    memcpy(&a, addr, sizeof(a));
    return (u_int)GetPuzzleClearInfo(a.type, a.idx) == a.clear;
}

// NOW_SYNCHRO_MODE: never true in FF3 (FF2 EvNowSynchroMode read the
// synchro flag).
// FF3 0x00199768

int EvCondNowSynchroMode(int event_id, u_char *addr)
{
    (void)event_id;
    (void)addr;
    return 0;
}

// NOT_NOW_SYNCHRO_MODE: the opposite.
// FF3 0x00199770

int EvNotNowSynchroMode(int event_id, u_char *addr)
{
    return EvCondNowSynchroMode(event_id, addr) != 1;
}

// CHECK_GAME_WORLD "11": the world byte (ingame_wrk.b03) is +1.
// FF3 0x00199798

int EvCheckGameWorld(int event_id, u_char *addr)
{
    (void)event_id;
    return (s_char)ingame_wrk.b03 == (s_char)Get1Byte(addr + 1);
}

// PEEPING_END: the peeping view (phase 45) is over.
// FF3 0x001997d0

int EvPeepingEnd(int event_id, u_char *addr)
{
    (void)event_id;
    (void)addr;
    return PeepStateGet() == 0;
}

// TEL_STATUS "12": FF3 tests the telephone flags with the condition's own
// label byte (+0), not its argument.
// FF3 0x001997f0

int EvTelStatus(int event_id, u_char *addr)
{
    struct { u_char code; u_char _1; short _2; } a;

    (void)event_id;
    memcpy(&a, addr, sizeof(a));
    return TelStatusCheck(a.code) != 0;
}

// TEL_TALK_END: the telephone talk is over.
// FF3 0x00199820

int EvTelTalkEnd(int event_id, u_char *addr)
{
    (void)event_id;
    (void)addr;
    return TelTalkIsEnd() != 0;
}

// TAPE_CHECK "12": cassette +2's word is set.
// FF3 0x00199840

int EvTapeCheck(int event_id, u_char *addr)
{
    struct { u_char code; u_char _1; short no; } a;

    (void)event_id;
    memcpy(&a, addr, sizeof(a));
    return CassetteGetState(a.no) != 0;
}

// TAPE_NOT_HAVE: no cassette held.
// FF3 0x00199870

int EvTapeNotHave(int event_id, u_char *addr)
{
    (void)event_id;
    (void)addr;
    return CassetteGetHaveNum() == 0;
}

// CINE_MENU_END: the cine film menu is closed.
// FF3 0x00199890

int EvCineMenuEnd(int event_id, u_char *addr)
{
    (void)event_id;
    (void)addr;
    return CineMenuGetState() == 0;
}

// CINE_FILM_END: no cine film playing.
// FF3 0x001998b0

int EvCineFilmEnd(int event_id, u_char *addr)
{
    (void)event_id;
    (void)addr;
    return CineFilmIsPlay() == 0;
}

// CINE_FLAG_CHECK "12": cine flag byte +2 against a mask that FF3 takes from
// the label byte (+0), as TEL_STATUS does.
// FF3 0x001998d0

int EvCineFlagCheck(int event_id, u_char *addr)
{
    struct { u_char code; u_char _1; short no; } a;

    (void)event_id;
    memcpy(&a, addr, sizeof(a));
    return CineFlagCheck(a.no, a.code) != 0;
}

// CINE_FILM_NOT_HAVE: no cine film held.
// FF3 0x00199900

int EvCineFilmNotHave(int event_id, u_char *addr)
{
    (void)event_id;
    (void)addr;
    return CineFilmGetHaveNum() == 0;
}

// EV_STREAM_ISREADY "112": the event stream +2 is preloaded (+1 is read and
// dropped).
// FF3 0x00199920

int EvStreamIsReady(int event_id, u_char *addr)
{
    (void)event_id;
    (void)Get1Byte(addr + 1);
    return EvStreamIsPreload(Get2Byte(addr + 2)) != 0;
}

// CHECK_DAY_FLAG "11": the map's side (day / night) is +1.
// FF3 0x00199960

int EvCheckDayFlag(int event_id, u_char *addr)
{
    (void)event_id;
    return Get1Byte(addr + 1) == MapAreaSideGet();
}

// MISSION_COUNTER_IS_MORE "112": the mission counter (frames, finder mode
// work +0x10) has reached +2 seconds (25 fps on PAL, else 30); +1 is read
// and dropped.
// FF3 0x001999a0. FF3 caches the work pointer in sdata gp-0xab0 (shared with
// mission_list.c); nothing depends on the cache, so this calls the getter.

int EvMissionCounterIsMore(int event_id, u_char *addr)
{
    int cnt = GetFinderModeWrk()->time;
    int sec;

    (void)event_id;
    (void)Get1Byte(addr + 1);
    sec = Get2Byte(addr + 2);
    if (GetPALMode() != 0) {
        cnt /= 25;
    } else {
        cnt /= 30;
    }
    return cnt >= sec;
}

// ══════════════════════════════════════════════════════════════════════
// The condition table (data 0x3242e8, FF2 ev_cond_wrk). Event programs
// name a condition by its row; IF_ / ELSEIF_ and the open / close
// conditions (ev_open.c) call row->func and step by row->size.
// ══════════════════════════════════════════════════════════════════════

static EV_COND_WRK ev_cond_wrk[EV_COND_NUM] = {   // data 0x3242e8
    {  0, EvPlyrAreaIn,           "1124",  8, 0, "PLYR_AREAIN" },
    {  1, EvPlyrAreaIn,           "1124",  8, 0, "PLYR_AREAIN_DEF" },
    {  2, EvPlyrAreaOut,          "1124",  8, 0, "PLYR_AREAOUT" },
    {  3, EvPlyrAreaOut,          "1124",  8, 0, "PLYR_AREAOUT_DEF" },
    {  4, EvPlyrRoomIn,           "22",    4, 0, "PLYR_ROOMIN" },
    {  5, EvPlyrRoomOut,          "22",    4, 0, "PLYR_ROOMOUT" },
    {  6, EvPlyrRot,              "222",   8, 0, "PLYR_ROT" },
    {  7, EvPlyrIntoSight,        "2222",  8, 0, "PLYR_INTO_SIGHT" },
    {  8, EvEvCntExec,            "1124",  8, 0, "EV_CNT_EXEC" },
    {  9, EvGetResearchStat,      "1124",  8, 0, "GET_RESEARCH_STAT" },
    { 10, EvSaveIsOk,             "",      4, 0, "SAVE_IS_OK" },
    { 11, EvEvWorkCheck,          "112",   4, 0, "EV_WORK_CHECK" },
    { 12, EvPhotoIsDev,           "",      4, 0, "PHOTO_IS_DEV" },
    { 13, EvDoorStatCheck,        "1124",  8, 0, "DOOR_STAT_CHECK" },
    { 14, EvGhostExist,           "112",   4, 0, "GHOST_EXIST" },
    { 15, EvGhostLost,            "112",   4, 0, "GHOST_LOST" },
    { 16, EvGhostReleaseType,     "1122",  8, 0, "GHOST_RELEASE_TYPE" },
    { 17, EvPlyrIsSelect,         "112",   4, 0, "PLYR_IS_SELECT" },
    { 18, EvGetDreamPlayer,       "112",   4, 0, "GET_DREAM_PLAYER" },
    { 19, EvNpcAreaOut,           "1124",  8, 0, "NPC_AREAOUT" },
    { 20, EvNpcAreaOut,           "1124",  8, 0, "NPC_AREAOUT_DEF" },
    { 21, EvNpcIntoPlyrSight,     "1122",  8, 0, "NPC_INTO_PLYR_SIGHT" },
    { 22, EvNpcDistance,          "11114", 8, 0, "NPC_DISTANCE" },
    { 23, EvPushPad,              "22",    4, 0, "PUSH_PAD" },
    { 24, EvPlyrItemUse,          "",      4, 0, "PLYR_ITEM_USE" },
    { 25, EvPlyrItemHave,         "1111",  4, 0, "PLYR_ITEM_HAVE" },
    { 26, EvEvState,              "112",   4, 0, "EV_STATE" },
    { 27, EvEvNotState,           "112",   4, 0, "EV_NOT_STATE" },
    { 28, EvEvRandom,             "112",   4, 0, "EV_RANDOM" },
    { 29, EvCondPhotoObj,         "1124",  8, 0, "PHOTO_OBJ" },
    { 30, EvNotPhotoObj,          "1124",  8, 0, "NOT_PHOTO_OBJ" },
    { 31, EvFinderMode,           "21",    4, 0, "FINDER_MODE" },
    { 32, EvObjIntoFinder,        "1124",  8, 0, "OBJ_INTO_FINDER" },
    { 33, EvObjIntoPlyrSight,     "1124",  8, 0, "OBJ_INTO_PLYR_SIGHT" },
    { 34, EvBattleEnd,            "",      4, 0, "BATTLE_END" },
    { 35, EvBattleMode,           "",      4, 0, "BATTLE_MODE" },
    { 36, EvChapterLoadWait,      "",      4, 0, "CHAPTER_LOAD_WAIT" },
    { 37, EvEvFailure,            "",      4, 0, "EV_FAILURE" },
    { 38, EvSetEvPriority,        "11",    4, 0, "SET_EV_PRIORITY" },
    { 39, EvPuzzleSuccess,        "1111",  4, 0, "PUZZLE_SUCCESS" },
    { 40, EvNowChapter,           "1111",  4, 0, "EV_NOW_CHAPTER" },
    { 41, EvNotNowChapter,        "1111",  4, 0, "EV_NOT_NOW_CHAPTER" },
    { 42, EvNotHaveItem,          "21",    4, 0, "EV_NOT_HAVE_ITEM" },
    { 43, EvCheckGameDifficulty,  "21",    4, 0, "CHECK_GAME_DIFFICULTY" },
    { 44, EvAboveClearNum,        "22",    4, 0, "ABOVE_CLEAR_NUM" },
    { 45, EvFollowClearNum,       "22",    4, 0, "FOLLOW_CLEAR_NUM" },
    { 46, EvCondNowSynchroMode,   "",      4, 0, "NOW_SYNCHRO_MODE" },
    { 47, EvNotNowSynchroMode,    "",      4, 0, "NOT_NOW_SYNCHRO_MODE" },
    { 48, nullptr,                "",      4, 0, "EV_OR" },
    { 49, EvCheckGameWorld,       "11",    4, 0, "CHECK_GAME_WORLD" },
    { 50, EvGetHideStatus,        "11",    4, 0, "GET_HIDE_STATUS_" },
    { 51, EvGetDevelopStatus,     "112",   4, 0, "GET_DEVELOP_STATUS_" },
    { 52, EvSelectChoice,         "11",    4, 0, "SELECT_CHOICE" },
    { 53, EvPeepingEnd,           "",      4, 0, "PEEPING_END" },
    { 54, EvTelStatus,            "12",    4, 0, "TEL_STATUS" },
    { 55, EvTelTalkEnd,           "",      4, 0, "TEL_TALK_END" },
    { 56, EvTapeCheck,            "12",    4, 0, "TAPE_CHECK" },
    { 57, EvTapeNotHave,          "",      4, 0, "TAPE_NOT_HAVE" },
    { 58, EvCineMenuEnd,          "",      4, 0, "CINE_MENU_END" },
    { 59, EvCineFilmEnd,          "",      4, 0, "CINE_FILM_END" },
    { 60, EvCineFlagCheck,        "12",    4, 0, "CINE_FLAG_CHECK" },
    { 61, EvCineFilmNotHave,      "",      4, 0, "CINE_FILM_NOT_HAVE" },
    { 62, EvStreamIsReady,        "112",   4, 0, "EV_STREAM_ISREADY" },
    { 63, EvPhotoState,           "112",   4, 0, "PHOTO_STATE_" },
    { 64, EvCheckDayFlag,         "11",    4, 0, "CHECK_DAY_FLAG" },
    { 65, EvCondSpGameover,       "",      4, 0, "SP_GAMEOVER_" },
    { 66, EvPlayerStatCheck,      "112",   4, 0, "PLAYER_STAT_CHECK" },
    { 67, EvCheckMission,         "22",    4, 0, "CHECK_MISSION" },
    { 68, EvGetCamPartsEquip,     "22",    4, 0, "GET_CAM_PARTS_EQUIP" },
    { 69, EvGetCamLensEquip,      "22",    4, 0, "GET_CAM_LENS_EQUIP" },
    { 70, EvRemainUndevelopFilms, "",      4, 0, "REMAIN_UNDEVELOP_FILMS" },
    { 71, EvMissionCounterIsMore, "112",   4, 0, "MISSION_COUNTER_IS_MORE" },
};

// ──────────────────────────────────────────────────────────────────────
// The row of a condition label.
// FF3 0x00199a40

EV_COND_WRK *GetEvCondWrk(int label)
{
    return &ev_cond_wrk[label];
}

// FF3 0x00199a60: a second getter that only calls the first.

static EV_COND_WRK *EvCondGetWrk(int label)
{
    return GetEvCondWrk(label);
}

// ──────────────────────────────────────────────────────────────────────
// Judge the condition at addr (IF_ / ELSEIF_).
// FF3 0x00199a78

int EvCondCallJudge(u_char label, int event_id, u_char *addr)
{
    EV_COND_FUNC func = EvCondGetWrk(label)->func;

    if (func == nullptr) {              // PC: EV_OR has none; the EE would jump to 0
        return 0;
    }
    return func(event_id, addr);
}

// ──────────────────────────────────────────────────────────────────────
// The condition's size in an event program.
// FF3 0x00199ab8

int EvCondGetSize(u_char label)
{
    return EvCondGetWrk(label)->size;
}

// ══════════════════════════════════════════════════════════════════════
// The dream-world player block and the event states.
// ══════════════════════════════════════════════════════════════════════

static int ev_dream_player[2];          // sdata gp-0x4c0 (0x373030)

// FF3 0x00199ad8

int *GetEvDreamPlayerWrk(void)
{
    return ev_dream_player;
}

// FF3 0x00199ae8

void EvDreamPlayerInit(void)
{
    memset(ev_dream_player, 0, sizeof(ev_dream_player));
}

// Save block 16.
// FF3 0x00199b08

int EvDreamPlayerSaveGet(void **addr)
{
    *addr = ev_dream_player;
    return sizeof(ev_dream_player);
}

EV_WRK ev_wrk;                          // bss 0x3e0098

// Save block 11.
// FF3 0x00199b18

int EvWrkSaveGet(void **addr)
{
    *addr = &ev_wrk;
    return sizeof(ev_wrk);
}

// ──────────────────────────────────────────────────────────────────────
// Move an event to a new state. A sealed event (EV_STATE_LOCK) stays sealed;
// entering EV_STATE_EXE counts a run (up to 100, EV_CNT_EXEC).
// FF3 0x00199b30; from MioPan ev_change.c (FF3 skips the same state and
// adds the count).

void SetEventState(int event_id, u_char state)
{
    EVENT_STATE *ev = &ev_wrk.ev_state[event_id];

    if (ev->state == EV_STATE_LOCK || ev->state == state) {
        return;
    }
    ev->state = state;
    if (state == EV_STATE_EXE && ev->cnt < 100) {
        ev->cnt++;
    }
}

// FF3 0x00199b88; FF2 ev_get.c GetEvState.

u_char GetEvState(int event_id)
{
    if (event_id < 0) {
        // PC: a root event's parent is -1 (CompulsionSetEventState); the EE
        // reads the byte before ev_state[0], ev_no's lowest.
        return (u_char)ev_wrk.ev_no;
    }
    return ev_wrk.ev_state[event_id].state;
}

// FF3 0x00199ba0

s_char GetEvExeCount(int event_id)
{
    return ev_wrk.ev_state[event_id].cnt;
}

// FF3 0x00199bb8; FF2 SetEventWaitFlg (without the range assert).

void SetEventWaitFlg(u_char flg)
{
    ev_wrk.wait_flg = flg;
}

// FF3 0x00199bd0; FF2 ev_get.c GetEvWrkWaitFlg.

u_char GetEvWrkWaitFlg(void)
{
    return ev_wrk.wait_flg;
}

// ──────────────────────────────────────────────────────────────────────
// EventInit: everything cleared, every event waiting to open.
// FF3 0x00199be0

void EvWrkInit(void)
{
    memset(&ev_wrk, 0, sizeof(ev_wrk));
    ev_wrk.step = 0;
    ev_wrk.ev_no = 0;
    ev_wrk.wait_flg = 0;
    for (int i = 0; i < EV_EVENT_NUM; i++) {
        ev_wrk.ev_state[i].state = EV_STATE_WAIT_OPEN;
    }
}

// FF2's dump of every event's state; retail compiled printf out and kept an
// empty loop over the events. Unreferenced.
// FF3 0x00199c50

void EvDbg_EventStatePrint(void)
{
}

// ══════════════════════════════════════════════════════════════════════
// Parked state changes (FF2 ev_change.c): a change to an event that is
// running waits here until CompulsionSetEventStateMain finds it quiet.
// ══════════════════════════════════════════════════════════════════════

static EV_CHANGE_CTRL ev_change_ctrl[EV_CHANGE_CTRL_MAX];   // bss 0x3e1c78

static void CompulsionSet_ForcedEndState(int event_id);
static int  Find_EvChangeCtrl(int change_id);
static int  GetEmpty_EvChangeCtrl(int change_id);

// Save block 5.
// FF3 0x00199c80

int EvChangeCtrlSaveGet(void **addr)
{
    *addr = ev_change_ctrl;
    return sizeof(ev_change_ctrl);
}

// FF3 0x00199c98; from MioPan ev_change.c.

void EvChangeCtrlInit(void)
{
    for (int i = 0; i < EV_CHANGE_CTRL_MAX; i++) {
        ev_change_ctrl[i].set_event_id = -1;
        ev_change_ctrl[i].change_id    = -1;
        ev_change_ctrl[i].change_state = 0;
    }
}

// ──────────────────────────────────────────────────────────────────────
// EventMain: apply each parked change whose event is quiet (waiting to open
// or ended) and has left the execution list.
// FF3 0x00199cd0; from MioPan ev_change.c.

void CompulsionSetEventStateMain(void)
{
    for (int i = 0; i < EV_CHANGE_CTRL_MAX; i++) {
        int id = ev_change_ctrl[i].change_id;

        if (id == -1) {
            continue;
        }
        switch (GetEvState(id)) {
        case EV_STATE_WAIT_OPEN:
        case EV_STATE_END:
            if (CheckEventExeEntry(id) == -1) {
                CompulsionSetEventState(id, ev_change_ctrl[i].change_state);
                Del_CompulsionSetEventState(id);
            }
            break;
        }
    }
}

// ──────────────────────────────────────────────────────────────────────
// Move change_id now: its conditions and execution slot go, then by the new
// state its open conditions come back, its init program starts, it parks,
// or its whole sub-tree is sealed.
// FF3 0x00199d78 (jump table 0x355500); from MioPan ev_change.c. FF3
// changes: back to EV_STATE_WAIT_OPEN under an ended or sealed parent the
// event parks (EV_STATE_SUSPEND) instead.

void CompulsionSetEventState(int change_id, u_char state)
{
    u_char parent_state = GetEvState(EvGetParentID(change_id));

    if (GetEvState(change_id) == EV_STATE_LOCK) {
        return;
    }
    SetEventState(change_id, state);
    EventDelCondition(change_id);
    EvExeRelease(change_id);

    switch (state) {
    case EV_STATE_WAIT_OPEN:
        if (parent_state == EV_STATE_END || parent_state == EV_STATE_LOCK) {
            SetEventState(change_id, EV_STATE_SUSPEND);
        } else if (parent_state != EV_STATE_WAIT_OPEN) {
            EventSetOpenCondition(change_id);
        }
        break;

    case EV_STATE_INIT:
        SetEventInitStatus(change_id);
        break;

    case EV_STATE_SUSPEND:
        SetEventState(change_id, EV_STATE_SUSPEND);
        if (parent_state != EV_STATE_END && parent_state != EV_STATE_LOCK) {
            EventSetOpenCondition(change_id);
        }
        break;

    case EV_STATE_LOCK:
        CompulsionSet_ForcedEndState(change_id);
        break;
    }
}

// ──────────────────────────────────────────────────────────────────────
// Seal event_id's sub-tree: a waiting sub-event loses its conditions, a
// parked one is sealed at once, a running one gets a parked seal, and every
// one recurses.
// FF3 0x00199eb8 (jump table 0x355524); from MioPan ev_change.c.

static void CompulsionSet_ForcedEndState(int event_id)
{
    int id_tbl[52];                     // EV_SUB_ID_MAX are used; the stack holds 52

    EvGetSubId(event_id, id_tbl);

    for (int i = 0; i < EV_SUB_ID_MAX; i++) {
        int sub = id_tbl[i];

        if (sub == -1) {
            continue;
        }
        switch (GetEvState(sub)) {
        case EV_STATE_WAIT_OPEN:
            EventDelCondition(sub);
            // fall through
        case EV_STATE_SUSPEND:
            SetEventState(sub, EV_STATE_LOCK);
            // fall through
        case EV_STATE_INIT:
        case EV_STATE_EXE:
        case EV_STATE_WAIT_END:
            Req_CompulsionSetEventState(event_id, sub, EV_STATE_LOCK);
            // fall through
        case EV_STATE_END:
            CompulsionSet_ForcedEndState(sub);
            break;
        }
    }
}

// ──────────────────────────────────────────────────────────────────────
// Park a change of change_id (a second request for it overwrites the first).
// FF3 0x00199f78; from MioPan ev_change.c.

void Req_CompulsionSetEventState(int set_event_id, int change_id, u_char state)
{
    int row = GetEmpty_EvChangeCtrl(change_id);

    if (row == -1) {
        // PC: the EE writes row -1 anyway (over the last event states and
        // ev_wrk.wait_flg); FF2 asserted here.
        return;
    }
    ev_change_ctrl[row].change_state = state;
    ev_change_ctrl[row].set_event_id = set_event_id;
    ev_change_ctrl[row].change_id    = change_id;
}

// FF3 0x00199fd8; from MioPan ev_change.c.

void Del_CompulsionSetEventState(int change_id)
{
    for (int i = 0; i < EV_CHANGE_CTRL_MAX; i++) {
        if (ev_change_ctrl[i].change_id == change_id) {
            ev_change_ctrl[i].set_event_id = -1;
            ev_change_ctrl[i].change_id    = -1;
            ev_change_ctrl[i].change_state = 0;
        }
    }
}

// The row holding change_id, -1 if none.
// FF3 0x0019a018

static int Find_EvChangeCtrl(int change_id)
{
    for (int i = 0; i < EV_CHANGE_CTRL_MAX; i++) {
        if (ev_change_ctrl[i].change_id == change_id) {
            return i;
        }
    }
    return -1;
}

// The row already holding change_id, else the first free one, else -1.
// FF3 0x0019a058; FF2 GetEmpty_EvChangeCtrl did both in one loop.

static int GetEmpty_EvChangeCtrl(int change_id)
{
    int row = Find_EvChangeCtrl(change_id);

    if (row < 0) {
        int empty = Find_EvChangeCtrl(-1);

        row = (empty < 0) ? -1 : empty;
    }
    return row;
}

// FF2's dump of the parked changes; retail compiled printf out, and the
// loop with it. Unreferenced.
// FF3 0x0019a098

void EvDbg_CompulsionSetPrint(void)
{
}
