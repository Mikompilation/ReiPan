/* ==========================================================================
 *  graphics/scene/scene_dat.c
 *
 *  Scene play control; see scene_dat.h. FF3 0x253620..0x254018 plus two
 *  out-of-line pieces, SceneMapObjShow 0x2be808 and StrGetNumber 0x2bef98.
 *
 *  FF3 keeps two scene slots (scene_ctrl, bss 0x3f7d70; FF2 one SCENE_CTRL
 *  per load buffer). scene.c loads a scene into a slot; one_Story_Scene_Main
 *  (phase 89, IngameScene.c) waits on SceneStreamLoadMain, starts the
 *  scene with ScenePlayStart, runs ScenePlayMain each frame until
 *  ScenePlayIsEnd and ends it with ScenePlayEnd, which puts the camera, the
 *  player's flashlight and the rooms back as they were before the scene.
 *
 *  The slots belong to this file: ScenePlayEnd reaches the playing-slot
 *  pointer (0x3fcdd0) and the light backup (0x3fcde0) through scene_ctrl's
 *  address (+0x5060 / +0x5070), which GCC only does for one file's bss.
 * ======================================================================== */
#include "graphics/scene/scene_dat.h"

#include <ctype.h>                      // isdigit
#include <stdint.h>                     // uintptr_t
#include <stdlib.h>                     // atoi
#include <string.h>                     // memset, strncmp, strncpy

#include "common/heapctrl.h"            // heapCtrlMalloc, heapCtrlFree
#include "graphics/scene/IngameScene.h" // SceneStrFileGet
#include "graphics/effect/effect_oth.h" // EffectMonochromeSet
#include "graphics/game_model.h"        // GameModel::SetAlpha
#include "graphics/graph3d/g3dLight.h"  // G3DLIGHT
#include "graphics/graph3d/gra3d.h"     // gra3dIsMonotoneDrawEnable, gra3dcamSetClip / SetFov, gra3dApplyCamera
#include "graphics/graph3d/gra3dMisc.h"     // gra3dPrelightScene
#include "graphics/mmanage.h"           // model_collision_set
#include "graphics/movie/movie.h"       // BORROW_BUF
#include "graphics/movie/movie_title.h" // MovieTitleInit, MovieTitleEnd
#include "graphics/scene/fod.h"         // FodNextFrame
#include "graphics/scene/scene_effect.h"    // SceneEffectEnd
#include "ingame/char_model.h"          // CharPlayerModel
#include "ingame/ingame.h"              // IngameMdlResetAll
#include "ingame/ingame_wrk.h"          // ingame_wrk
#include "ingame/map/MapDraw.h"         // MapDrawPreLight, MapDrawEnableFlashlightOnly
#include "ingame/map/MapLoad.h"         // MapLoadGetRoomNoNow, MapLoadReload, MapLoadMoveRoom ..
#include "ingame/map/MapObjWrk.h"       // MapObjSetHide, MapObjUpdateRoom, MAP_OBJ_TYPE_*
#include "ingame/map/MapPut.h"          // MapPutSetSceneLoad
#include "ingame/plyr/player.h"         // PLYR_WRK
#include "ingame/story_door.h"          // MapAreaSideGet / Set, MapAreaNoSet
#include "system/eeiop/cd_file_dat.h"   // SCENE_VOL
#include "system/eeiop/cddat.h"         // GetFileSize
#include "system/eeiop/stream_auto.h"   // StreamAutoPreload, StreamAutoIsPreload, StreamAutoPreloadPlay, StreamAutoFadeOut
#include "system/os/eecdvd.h"           // LoadReq
#include "system/os/sys_heap.h"         // GetSystemHeapWrkP
#include "system/pad/pad_ctrl.h"        // pad_key, CheckPadTrgPlaySe

// ──────────────────────────────────────────────────────────────────────
// Data
// ──────────────────────────────────────────────────────────────────────

SCENE_CTRL scene_ctrl[SCENE_CTRL_NUM];  // bss 0x3f7d70

static SCENE_CTRL *scn_play_ctrl;       // bss 0x3fcdd0: the playing slot (ScenePlayStart .. ScenePlayEnd)
static G3DLIGHT PlayerLightBackup;      // bss 0x3fcde0: the player's flashlight before the scene (FF2's name)
static SCENE_VOL_HDR *scene_vol;        // sbss gp-0x115c: scene.vol between its load and SceneVolDataFree

// The stream states of SCENE_CTRL.stream_step.
enum
{
    SCENE_STREAM_NONE    = 0,           // no stream (or it failed)
    SCENE_STREAM_REQ     = 1,           // preload it
    SCENE_STREAM_PRELOAD = 2,           // preloading
    SCENE_STREAM_READY   = 3,           // preloaded; ScenePlayMain starts it on frame 1
};

// The PAL cut frames of the engine scenes (FF2 scene_dat.c's
// scene_cut_timing, there one list per scene number; FF3 keeps a row per
// scene that has cuts). Lists rodata 0x362880.. (scene 6's in sdata 0x3723b0).
static const int scene0020_cut_timing[] = { 255, 555, 655, 705, 775, -1 };
static const int scene0030_cut_timing[] = { 50, 220, 390, 450, 1090, 1140, 1160, 1249, 1250, -1 };
static const int scene0060_cut_timing[] = { 250, -1 };
static const int scene0062_cut_timing[] = { 280, 460, -1 };
static const int scene0070_cut_timing[] =
{
    130, 265, 465, 501, 555, 690, 850, 960, 1130, 1250, 1400, 1485, 1575, 1660, 1750, 1840, 2080, 2160,
    2237, 2280, -1
};
static const int scene0080_cut_timing[] = { 90, 230, 305, 430, 447, -1 };
static const int scene0090_cut_timing[] =
{
    260, 370, 610, 790, 820, 940, 1180, 1480, 1575, 1680, 2610, 2690, 3200, 3380, 3530, 3650, 3785, -1
};
static const int scene0110_cut_timing[] = { 120, 350, 550, -1 };
static const int scene0111_cut_timing[] = { 100, 160, 250, 370, -1 };
static const int scene0112_cut_timing[] = { 150, 350, 430, 544, 751, 880, 970, 1050, 1200, 1290, -1 };
static const int scene0113_cut_timing[] = { 150, 240, 330, 570, 620, 890, 1030, 1380, 1640, -1 };
static const int scene9020_cut_timing[] =
{
    90, 220, 530, 700, 870, 990, 1380, 1470, 1560, 1820, 1850, 1875, 1955, 2005, 2055, 2115, 2345, 2525, -1
};
static const int scene9070_cut_timing[] = { 100, 160, 280, 400, 551, -1 };

static const SCENE_CUT_TIMING scene_cut_timing[] =  // rodata 0x362810
{
    {  2, scene0020_cut_timing },
    {  3, scene0030_cut_timing },
    {  6, scene0060_cut_timing },
    {  8, scene0062_cut_timing },
    {  9, scene0070_cut_timing },
    { 10, scene0080_cut_timing },
    { 12, scene0090_cut_timing },
    { 15, scene0110_cut_timing },
    { 16, scene0111_cut_timing },
    { 17, scene0112_cut_timing },
    { 18, scene0113_cut_timing },
    { 33, scene9020_cut_timing },
    { 35, scene9070_cut_timing },
    { -1, nullptr },
};

static void SceneRoomRestore(int room, int mono, int side);

// ──────────────────────────────────────────────────────────────────────
// The scene stream: SCENE<no>_STR, preloaded so it starts with the scene.
// Returns 1 when ready (or there is none), 0 while preloading, -1 when the
// scene has no stream file (stream_step drops to NONE, so the next call
// answers 1) or for nullptr.
// FF3 0x00253620

int SceneStreamLoadMain(SCENE_CTRL *sc)
{
    int file;

    if (sc == nullptr)
    {
        return -1;
    }

    switch (sc->stream_step)
    {
    case SCENE_STREAM_NONE:
    case SCENE_STREAM_READY:
        return 1;
    case SCENE_STREAM_REQ:
        file = SceneStrFileGet(sc->scene_no);
        if (file >= 0)
        {
            if (GetFileSize(file) == (u_int)-1)
            {
                sc->stream_step = SCENE_STREAM_NONE;
                return -1;
            }
            if (GetFileSize(file) != 0)
            {
                sc->stream_id = StreamAutoPreload(file, file + 1, 11, 0, 0,
                                                  SceneDatGetValue(sc->scene_no) << 7, 0, nullptr);
                sc->stream_step = SCENE_STREAM_PRELOAD;
                return 0;
            }
        }
        sc->stream_step = SCENE_STREAM_NONE;
        return -1;
    case SCENE_STREAM_PRELOAD:
        if (StreamAutoIsPreload(sc->stream_id))
        {
            sc->stream_step = SCENE_STREAM_READY;
        }
        return 0;
    default:
        return 0;
    }
}

// ──────────────────────────────────────────────────────────────────────
// Starts scene_no from the slot it was loaded into, once the slot may play
// and its data, rooms and stream are ready. Leaving monochrome first
// prelights the scene's rooms in colour; the player's flashlight is saved
// for ScenePlayEnd. 1 when the scene started.
// FF3 0x00253728

int ScenePlayStart(int scene_no)
{
    SCENE_CTRL *sc;
    G3DLIGHT   *fl;
    int         ready;

    sc = nullptr;
    for (int i = 0; i < SCENE_CTRL_NUM; i++)
    {
        if (scene_ctrl[i].scene_no == (short)scene_no)
        {
            sc = &scene_ctrl[i];
            break;
        }
    }

    ready = 0;
    if (sc != nullptr && sc->init_flg == 1 && SceneDataLoadMain(sc) != 0 && SceneRoomLoadMain(sc) != 0
        && SceneStreamLoadMain(sc) != 0)
    {
        ready = 1;
    }
    if (!ready)
    {
        return 0;
    }

    if (gra3dIsMonotoneDrawEnable())
    {
        EffectMonochromeSet(0, 0);
        gra3dPrelightScene(SceneRoomNoGet(sc));
        gra3dPrelightScene(SceneSubRoomNoGet(sc));
    }
    MovieTitleInit(scene_no);
    MapDrawEnableFlashlightOnly(0);

    fl = &((CharPlayerModel *)ingame_wrk.plyr_wrk->mdl)->fl;
    PlayerLightBackup = *fl;

    SceneInitAfterModelLoad(sc, 1);
    scn_play_ctrl = sc;
    return 1;
}

// ──────────────────────────────────────────────────────────────────────
// One frame of the playing scene: the preloaded stream starts on the FOD's
// first whole frame, the scene draws and its FOD steps. With skip, a
// START / OK trigger from frame 16 on ends the scene (end_flg) and fades
// its stream. 1 while a scene plays (its slot still ready), else 0.
// FF3 0x00253888

int ScenePlayMain(int subtitle, int skip)
{
    SCENE_CTRL *sc = scn_play_ctrl;
    FOD_CTRL   *fod;

    if (sc == nullptr || sc->init_flg != 1 || SceneDataLoadMain(sc) == 0 || SceneRoomLoadMain(sc) == 0
        || SceneStreamLoadMain(sc) == 0)
    {
        return 0;
    }

    fod = &sc->fod_ctrl;
    if (sc->stream_step == SCENE_STREAM_READY && sc->stream_play == 0 && fod->now_frame == 1 && fod->now_reso == 0)
    {
        StreamAutoPreloadPlay(sc->stream_id);
        sc->stream_play = 1;
    }

    model_collision_set.ClearHit();
    SceneDraw(sc, subtitle);
    if (sc->fod_flg == 1)
    {
        FodNextFrame(fod, sc->scene_no);
    }

    if (skip != 0)
    {
        if (CheckPadTrgPlaySe((u_short)(pad_key[PAD_KEY_OK] | 0x800), -1, 0) == 0)
        {
            return 1;
        }
        if (fod->end_flg != 0 || fod->now_frame < 16)
        {
            return 1;
        }
        fod->end_flg = 1;
        StreamAutoFadeOut(sc->stream_id, 1);
    }
    return 1;
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x002539e8

int ScenePlayIsEnd(void)
{
    if (scn_play_ctrl == nullptr)
    {
        return 1;
    }
    return scn_play_ctrl->fod_ctrl.end_flg != 0;
}

// ──────────────────────────────────────────────────────────────────────
// Back to the game after the scene (FF2 SceneEndProc): the models reset,
// the room light and flashlight-only drawing, the camera's clip and field
// of view, the player's flashlight, the scene effects and captions off, the
// map objects shown again; then the slot is released and the rooms,
// monochrome and area side go back to how they were.
// FF3 0x00253a08

void ScenePlayEnd(void)
{
    SCENE_CTRL *sc = scn_play_ctrl;
    int         room, mono, side;

    if (sc == nullptr)
    {
        return;
    }

    IngameMdlResetAll(1);
    MapDrawPreLight(nullptr);
    MapDrawEnableFlashlightOnly(1);
    gra3dcamSetClip(sc->fNearZBak, sc->fFarZBak);
    gra3dcamSetFov(sc->fFovBak);
    gra3dApplyCamera(nullptr, 1, 1);
    ((CharPlayerModel *)ingame_wrk.plyr_wrk->mdl)->fl = PlayerLightBackup;
    SceneEffectEnd();
    MovieTitleEnd();
    SceneMapObjShow(sc, 1);

    room = sc->room_bak;
    mono = sc->MonotoneEnableBak;
    side = sc->AreaSideBak;
    SceneCtrlRelease(sc);
    SceneRoomRestore(room, mono, side);
    scn_play_ctrl = nullptr;
}

// ──────────────────────────────────────────────────────────────────────
// callcheck: BorrowBufDtor, operator_delete (the delete-expression)
// FF3 0x00253b18

void SceneCtrlRelease(SCENE_CTRL *sc)
{
    if (sc == nullptr)
    {
        return;
    }

    StreamAutoFadeOut(sc->stream_id, 1);
    SceneObjModelClear(sc);
    SceneManModelClear(sc);
    SceneDoorModelDBFree(sc);
    if (sc->scn_buf != nullptr)
    {
        delete sc->scn_buf;
    }
    if (sc->light_rev != nullptr)
    {
        heapCtrlFree(GetSystemHeapWrkP(), sc->light_rev, 0, 0, 0);
    }
    if (sc->eff_data != nullptr)
    {
        heapCtrlFree(GetSystemHeapWrkP(), sc->eff_data, 0, 0, 0);
    }
    memset(sc, 0, sizeof(SCENE_CTRL));
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x00253c00

void SceneCtrlReleaseAll(void)
{
    SceneCtrlRelease(&scene_ctrl[0]);
    SceneCtrlRelease(&scene_ctrl[1]);
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x00253c38

void SceneWrkRelease(void)
{
    SceneCtrlReleaseAll();
    memset(scene_ctrl, 0, sizeof(scene_ctrl));
}

// ──────────────────────────────────────────────────────────────────────
// The game's room after a scene: monochrome and the area side as saved;
// a room still loaded is reloaded when the side changed (or prelit again
// when only monochrome did), another room is moved to; the other buffer is
// emptied and both buffers' map objects updated.
// FF3 0x00253c68

static void SceneRoomRestore(int room, int mono, int side)
{
    int mono_set = 0;
    int side_set = 0;
    int buf;

    MapPutSetSceneLoad(1);
    if (mono != gra3dIsMonotoneDrawEnable())
    {
        EffectMonochromeSet(mono != 0, 0);
        mono_set = 1;
    }
    if (side != MapAreaSideGet())
    {
        MapAreaSideSet(side);
        side_set = 1;
    }

    if (room == MapLoadGetRoomNoNow())
    {
        if (side_set)
        {
            MapLoadReload(room);
        }
        else if (mono_set)
        {
            gra3dPrelightScene(room);
        }
    }
    else
    {
        MapLoadMoveRoom(room);
    }

    MapAreaNoSet(room);
    buf = MapLoadGetBuffID(room);
    if (buf >= 0)
    {
        MapLoadDeleteRoom(buf ^ 1);
    }
    MapObjUpdateRoom(0, 1);
    MapObjUpdateRoom(1, 1);
    MapPutSetSceneLoad(0);
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x00253d78

SCENE_CTRL *ScenePlayCtrlGet(void)
{
    return scn_play_ctrl;
}

// ──────────────────────────────────────────────────────────────────────
// scene.vol into a system heap block (one_Boot_Load waits for the loads,
// then SceneVolDataApply / Free).
// FF3 0x00253d88

void SceneVolDataLoadReq(void)
{
    if (GetFileSize(SCENE_VOL) != (u_int)-1 && GetFileSize(SCENE_VOL) != 0)
    {
        scene_vol = (SCENE_VOL_HDR *)heapCtrlMalloc(GetSystemHeapWrkP(), GetFileSize(SCENE_VOL), 0, 0, 0);
        LoadReq(SCENE_VOL, scene_vol);
        return;
    }
    scene_vol = nullptr;
}

// ──────────────────────────────────────────────────────────────────────
// Each row whose name is a scene's sets a volume. The ROM sets row i's
// volume on scene i, not on the scene whose name matched; scene.vol lists
// every scene in scene_dat's order, so both are the same scene.
// FF3 0x00253e18

void SceneVolDataApply(void)
{
    SCENE_VOL_ROW *row;

    if (scene_vol == nullptr || strncmp(scene_vol->magic, "VOL", 4) != 0)
    {
        return;
    }

    row = (SCENE_VOL_ROW *)(scene_vol + 1);
    for (int i = 0; i < scene_vol->num; i++, row++)
    {
        for (int j = 0; j < SCENE_NUM; j++)
        {
            if (strncmp(row->name, SceneDatGetName(j), 4) == 0)
            {
                SceneDatSetValue(i, row->vol);
                break;
            }
        }
    }
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x00253ed8

void SceneVolDataFree(void)
{
    if (scene_vol != nullptr)
    {
        heapCtrlFree(GetSystemHeapWrkP(), scene_vol, 0, 0, 0);
        scene_vol = nullptr;
    }
}

// ──────────────────────────────────────────────────────────────────────
// The model names of one group in the scene header: 8 chars each.
// callcheck: GetAlignUp (the round-up to 16 on the pointer, PC)
// FF3 0x00253f20

u_char *SceneMdlNameSet(SCN_MDL *row, int num, u_char *data)
{
    if (num <= 0)
    {
        return data;
    }

    for (; num != 0; num--, row++)
    {
        strncpy(row->name, (const char *)data, sizeof(row->name));
        data += sizeof(row->name);
        row->name_end = 0;
        row->obj_no = (short)StrGetNumber(row->name, sizeof(row->name));
    }
    // GetAlignUp(data, 4). PC: on the host pointer, as the ROM on the EE
    // address; the same as aligning the file offset while the scene buffer
    // starts 16-aligned.
    if (((uintptr_t)data & 0xf) != 0)
    {
        data = (u_char *)(((uintptr_t)data + 0xf) & ~(uintptr_t)0xf);
    }
    return data;
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x00253fc0

const int *SceneCutTimingGet(int scene_no)
{
    for (int i = 0; scene_cut_timing[i].list != nullptr; i++)
    {
        if (scene_cut_timing[i].scene_no == scene_no)
        {
            return scene_cut_timing[i].list;
        }
    }
    return nullptr;
}

// ──────────────────────────────────────────────────────────────────────
// SceneInitAfterModelLoad hides (0) and ScenePlayEnd shows (1).
// FF3 0x002be808 (out of line)

void SceneMapObjShow(SCENE_CTRL *sc, int show)
{
    u_char alpha = (show == 1) ? 0x80 : 0;

    // PC: the ROM calls SetAlpha on each row's model unguarded (a null one
    // reads EE address 4 harmlessly); the host skips a missing model.
    for (int i = 0; i < sc->door_num; i++)
    {
        MapObjSetHide(-1, MAP_OBJ_TYPE_DOOR, sc->door_mdl[i].obj_no, show);
        if (sc->door_mdl[i].mdl != nullptr)
        {
            sc->door_mdl[i].mdl->SetAlpha(alpha);
        }
    }
    for (int i = 0; i < sc->furn_num; i++)
    {
        MapObjSetHide(-1, MAP_OBJ_TYPE_FURN, sc->furn_mdl[i].obj_no, show);
        if (sc->furn_mdl[i].mdl != nullptr)
        {
            sc->furn_mdl[i].mdl->SetAlpha(alpha);
        }
    }
    for (int i = 0; i < sc->item_num; i++)
    {
        if (sc->item_mdl[i].mdl != nullptr)
        {
            sc->item_mdl[i].mdl->SetAlpha(alpha);
        }
    }
}

// ──────────────────────────────────────────────────────────────────────
// Skips to the first digit within len, copies up to 10 digits, atoi. The
// ROM tests the character before the bound, so it may read str[len].
// FF3 0x002bef98 (out of line)

int StrGetNumber(const char *str, int len)
{
    char buf[16];
    int  i = 0;
    int  j = 0;

    if (len < 0)
    {
        return 0;
    }

    while (!isdigit((u_char)str[i]) && i < len)
    {
        i++;
    }
    while (isdigit((u_char)str[i]) && i < len && j <= 9)
    {
        buf[j++] = str[i++];
    }
    buf[j] = 0;
    return atoi(buf);
}
