/* ==========================================================================
 *  ingame/story_night.c
 *
 *  Phase 0x23 Story_NightChange and the night start position; see
 *  story_night.h. FF3 0x219e60..0x21a678.
 *
 *  FF3 caches GetGPhaseCtrl() in sbss (gp-0xca4, 0x37284c); calls go to the
 *  getter directly. The compiler's atexit thunk NightGPhaseCtrlStaticDtor
 *  (FF3 0x0021a658 = GPhaseCtrlTerm(0x41e468)) is not written: the host's
 *  static destroys itself.
 * ======================================================================== */
#include "ingame/story_night.h"

#include <string.h>                                     // memset

#include "graphics/graph2d/fade.h"                      // FadeOutReq / FadeInReq / GetFadeState
#include "ingame/cat/cat_ctrl.h"                        // CatMdlChange
#include "ingame/enemy/ene_spe_obj.h"                   // GetEneSpeObjTbl
#include "ingame/ingame.h"                              // IngameMain, IngameDrawSub, IngameIsIdOver2e, GPhaseCtrlRequest
#include "ingame/ingame_wrk.h"                          // ingame_wrk.plyr_wrk
#include "ingame/map/MapCtrl.h"                         // MapAreaGetHeight
#include "ingame/map/MapLoad.h"                         // MapLoad*
#include "ingame/photo/n_plyr_camera.h"                 // PlyrCameraIsFixed / IsCatMdl / MdlChange, GetPlyrCameraWrk
#include "ingame/plyr/player.h"                         // PLYR_WRK, PlyrSetPos / Rot / Mode, SetPlyrAnime, CheckPlyrAnimeEnd
#include "ingame/plyr/plyr_sub.h"                       // PlyrMdlChange, AllPlyrIsReady, AllPlyrMdlIsLoaded, PlyrTurnFlagClear
#include "ingame/story_door.h"                          // MapAreaSideGet / Set, MapAreaNoSet, MapAreaFloorChange
#include "ingame/story_menu.h"                          // LockTblInc / Dec, IsLockTblOn / Set
#include "main/gphase.h"                                // GetGPhaseCtrl
#include "system/b2d/BpCtl.h"                           // BpCtlMain
#include "system/eeiop/stream_auto.h"                   // StreamAutoSetPlayNum

// ──────────────────────────────────────────────────────────────────────
// Data
// ──────────────────────────────────────────────────────────────────────

#define NIGHT_ROOM_MAX      0x3c                        // start rooms below this move the player
#define NIGHT_ROOM_SLEEP    0x2f                        // the start room where the player wakes (anime 0x9b)
#define NIGHT_ANIME_WAKE    0x9b
#define NIGHT_FADE_TIME     0x1e
#define NIGHT_LOCK_CHANGE   6                           // held from the side change to the end of the room load
#define NIGHT_LOCK_REQ      0x13                        // NightChangeReq refuses while it is set or counted

// FF3 data 0x35d940: the locks the phase holds from init to end (ints,
// read by their low byte).
static const int night_lock_tbl[8] = { 3, 5, 12, 14, 15, 16, 17, 18 };

static NIGHT_CHANGE_WRK night_change;                   // bss 0x3f6198
static NIGHT_START_POS  night_start_pos;                // bss 0x3f61a8
static u_char           night_plyr_mode;                // sdata 0x3713d0: the player's mode at step 0, restored at the end

// ──────────────────────────────────────────────────────────────────────
// Phase 0x23
// ──────────────────────────────────────────────────────────────────────

// The return phase is the one that requested this; special enemy objects
// stop; the locks are taken.
// FF3 0x00219e60.

int init_Story_NightChange(void)
{
    night_change.ret  = GetGPhaseCtrl()->from;
    night_change.step = 0;
    GetEneSpeObjTbl()->StopAll();
    for (int i = 0; i < 8; i++)
    {
        LockTblInc((u_char)night_lock_tbl[i]);
    }
    return 1;
}

// FF3 0x00219ee8.

int end_Story_NightChange(void)
{
    for (int i = 0; i < 8; i++)
    {
        LockTblDec((u_char)night_lock_tbl[i]);
    }
    return 1;
}

// The game frame of the phase: while the room loads (step 6) without the
// draw (IngameMain skip 0x3f), else with it (0x3e).
// FF3 0x0021a108.

static void NightChangeIngameMain(void)
{
    if (night_change.step == 6)
    {
        IngameMain(0x3f);
    }
    else
    {
        IngameMain(0x3e);
        IngameDrawSub(0);
    }
    BpCtlMain();
}

// The start room into its buffer: another room (below 0x3c) is moved to and
// becomes the area, else the current room reloads.
// FF3 0x0021a370.

static void NightChangeRoomLoad(void)
{
    int now  = MapLoadGetRoomNoNow();
    int room = night_start_pos.room;

    if (room != now && (u_int)room < NIGHT_ROOM_MAX)
    {
        MapLoadMoveRoom(room);
        MapAreaNoSet(room);
        return;
    }
    MapLoadReload(now);
}

// The models for the new side: the player's (kind 1 on side 1), the NPC's
// (not for player 1; kept when it is fixed with a cat model) and the cat's.
// FF3 0x0021a440.

static void NightChangeMdlChange(void)
{
    int    night = MapAreaSideGet() == 1;
    u_char no    = PlyrGetCurNo();

    PlyrMdlChange(no, (u_char)night, (s_char)0xff);
    if (no != 1)
    {
        if (PlyrCameraIsFixed(0) == 0 || PlyrCameraIsCatMdl(GetPlyrCameraWrk(0)) == 0)
        {
            PlyrCameraMdlChange(0, (s_char)night, 1);
        }
    }
    CatMdlChange(0, -1, 1);
}

// FF3 0x0021a150.

static void NightChangeRoomStart(void)
{
    NightChangeRoomLoad();
    NightChangeMdlChange();
}

// FF3 0x0021a170.

static int NightChangeIsReady(void)
{
    if (MapLoadCheckLoadNow() != 0)
    {
        return 0;
    }
    return AllPlyrIsReady() != 0;
}

// The room draws alone: both draw flags of the current area set, the other
// buffer emptied.
// FF3 0x0021a3d0.

static void NightChangeRoomDrawSet(void)
{
    int room = MapLoadGetRoomNoNow();
    int buff = MapLoadGetBuffID(room);

    MapLoadSetDrawFlg(3, 2);
    MapLoadSetDrawFlg3(room, 1);
    if (buff >= 0)
    {
        MapLoadDeleteRoom(buff ^ 1);
    }
}

// The player at the start position: the room origin plus (x, z), on the
// floor there, facing rot; turning cleared. In room 0x2f the player wakes
// (anime 0x9b, mode 9), elsewhere stands (anime 0).
// FF3 0x0021a4e0.

static void NightChangePlyrSet(void)
{
    u_int room = (u_int)night_start_pos.room;

    if (room < NIGHT_ROOM_MAX)
    {
        float pos[4] = { night_start_pos.x, 0.0f, night_start_pos.z, 1.0f };
        float rot[4] = { 0.0f, night_start_pos.rot * 3.1415927f / 180.0f, 0.0f, 0.0f };
        float org[4];

        MapLoadGetOffsetVector(org, MapLoadGetBuffID((int)room));
        pos[0] += org[0];                               // vadd.xyz: w stays 1
        pos[1] += org[1];
        pos[2] += org[2];
        MapAreaGetHeight(pos, pos, night_start_pos.room, 0);
        PlyrSetPos(pos);
        PlyrSetRot(rot);
        MapAreaFloorChange(night_start_pos.floor);
    }
    PlyrTurnFlagClear();
    if (room == NIGHT_ROOM_SLEEP)
    {
        SetPlyrAnime(NIGHT_ANIME_WAKE, 0, 0xff, 0xff, 0, 0.0f);
        PlyrSetMode(PLYR_MODE_9, nullptr);
    }
    else
    {
        SetPlyrAnime(0, 0, 0xff, 0xff, 0, 0.0f);
    }
}

// FF3 0x0021a1a8.

static void NightChangeRoomEnd(void)
{
    NightChangeRoomDrawSet();
    AllPlyrMdlIsLoaded();                               // result unused in FF3
    NightChangePlyrSet();
}

// Steps: 0 keep the player's mode; 1 / 2 fade out (only with fade); 3 lock
// 6; 4 one frame; 5 the side and the room; 6 wait for the map and the
// players, place the player, unlock; 7 / 8 fade in (only with fade); 9 wait
// for the wake anime, then the mode back and the return phase.
// FF3 0x00219f38 (switch table 0x35d918).

int one_Story_NightChange(void)
{
    int ret = 0;

    switch (night_change.step)
    {
    case 0:
        night_plyr_mode   = ingame_wrk.plyr_wrk->mode;
        night_change.step = (night_change.fade != 0) ? 1 : 3;
        break;
    case 1:
        FadeOutReq(0, 0, 0, NIGHT_FADE_TIME);
        night_change.step = 2;
        StreamAutoSetPlayNum(0, NIGHT_FADE_TIME);
        break;
    case 2:
        if (GetFadeState() != 0)
        {
            night_change.step = 3;
        }
        break;
    case 3:
        LockTblInc(NIGHT_LOCK_CHANGE);
        night_change.step = 4;
        break;
    case 4:
        night_change.step = 5;
        break;
    case 5:
        MapAreaSideSet((u_int)night_change.side);
        NightChangeRoomStart();
        night_change.step = 6;
        break;
    case 6:
        if (NightChangeIsReady())
        {
            NightChangeRoomEnd();
            LockTblDec(NIGHT_LOCK_CHANGE);
            night_change.step = (night_change.fade != 0) ? 7 : 9;
        }
        break;
    case 7:
        FadeInReq(0, 0, 0, NIGHT_FADE_TIME);
        night_change.step = 8;
        StreamAutoSetPlayNum(2, NIGHT_FADE_TIME);
        break;
    case 8:
        if (GetFadeState() == 1)
        {
            night_change.step = 9;
        }
        break;
    case 9:
        if (CheckPlyrAnimeEnd(NIGHT_ANIME_WAKE) != 0)
        {
            ret = 1;
            PlyrSetMode(night_plyr_mode, nullptr);
            GPhaseCtrlRequest(night_change.ret, 0);
        }
        break;
    }
    NightChangeIngameMain();
    return ret;
}

// ──────────────────────────────────────────────────────────────────────
// Requests and the start position
// ──────────────────────────────────────────────────────────────────────

// FF3 0x0021a1d0.

void NightChangeInit(void)
{
    memset(&night_change, 0, sizeof(night_change));
    night_change.side = -1;
    NightStartPosInit();
}

// FF3 0x0021a210.

int NightChangeReq(int side, int fade)
{
    if (IsLockTblOn(NIGHT_LOCK_REQ) != 0 || IsLockTblSet(NIGHT_LOCK_REQ) != 0)
    {
        return 0;
    }
    if (GetGPhaseCtrl()->leaf->id != 0x20)
    {
        return 0;
    }
    if (MapAreaSideGet() == side || MapLoadCheckLoadNow() != 0)
    {
        return 0;
    }
    if (IngameIsIdOver2e(MapLoadGetRoomNoNow()) != 1)
    {
        return 0;
    }
    night_change.side = side;
    night_change.fade = fade;
    GPhaseCtrlRequest(0x23, 0);
    return 1;
}

// FF3 0x0021a2f0 (rodata 0x3713d4: 40.0).

void NightStartPosInit(void)
{
    memset(&night_start_pos, 0, sizeof(night_start_pos));
    night_start_pos.rot   = 0.0f;
    night_start_pos.floor = 0;
    night_start_pos.z     = 40.0f;
    night_start_pos.room  = -1;
    night_start_pos.x     = 40.0f;
}

// FF3 0x0021a350.

void NightStartPosSet(int room, int unused, float x, float z, float rot)
{
    (void)unused;
    night_start_pos.room = room;
    night_start_pos.x    = x;
    night_start_pos.z    = z;
    night_start_pos.rot  = rot;
}

// FF3 0x0021a640.

int NightStartPosGetSaveData(void **addr)
{
    *addr = &night_start_pos;
    return sizeof(night_start_pos);
}
