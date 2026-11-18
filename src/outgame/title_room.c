// outgame/title_room.c
//
// PORT ADDITION -- the title screen's room backdrop.  title_room.h says what
// it is and how it hangs off the ROM's title; this is how it works.
//
// Nothing here is the ROM's.  It is built out of the paths a new game already
// takes rather than invented:
//
//     bring-up  IngameInit() -- init_Story_Start's, so every map subsystem
//               (area table, door and furniture tables, effects, the model
//               collision set, the file cache) is in the state a room load
//               expects -- then, once its loads are in, what
//               IngameNowLoadingMain() does for a start without the player:
//               MapAreaSideSet / MapAreaNoSet / MapStartNoSet,
//               MapPlayStartReset(), and the wait on IsLoadEndAll,
//               MapLoadCheckLoadNow, CheckSetEffectsOK and
//               MapPlayStartIsReady with MapLoadWaitMain pumping meanwhile;
//               DoorStatApply() last.
//     update    file_cache.PollAll() (IngamePre) and MapFrameUpdate() (the
//               furniture, the map objects, the blinking lights, the loader).
//     draw      IngameDrawSub()'s map half: MapFogProc, the sky effect,
//               MapDrawAll, MapPutDraw, EffectDrawMain -- minus the player,
//               the HUD, the fade and the event displays.
//     release   IngameEnd() minus what belongs to the title: it stops every
//               sound and frees every SE bank but the default one (the title
//               BGM and MENU_START_BD), clears title_start_flg (New Game's
//               choice) and resets the title's difficulty cursor.  See
//               TitleRoomIngameEnd().
//
// THE ROOM HAS TO BE GONE BEFORE ANYTHING ELSE TAKES THE MEMORY.  The two room
// buffers are fixed EE addresses (0x8f8000 / 0xc3d000, MapLoad.c), and outside
// a game the album (BpArenaSet 0x8f8000) and the movie player's borrow buffer
// use the same space.  Every way off the title ends Title_Mode, and
// end_TitleRoom_Mode() lets the room go before it says done; the attract movie
// runs under Title_Mode, so the room is let go while it plays and loaded again
// after.  Nothing is freed while a load is still in flight: the loader writes
// into buffers IngameEnd frees, so a release waits for IsLoadEndAll() and for
// the room load to settle.
//
// THE PLAYER IS NOT THERE.  No player model is made (PlyrMdlCreate is
// skipped), so MapDrawSetRoomLight() finds none and leaves the flashlight
// light off, MapDrawChars() is never called, and AllPlyrIsReady() counts no
// models.  ingame_wrk.plyr_wrk still has to point somewhere -- MapDrawRooms
// reads its flags -- and IngameInit() leaves it on player 0.  One map object
// in the house (furniture 49 in RRE04, drawn by PlyrCameraMapObjDraw) is lit
// from her flashlights through gra3dGenerateLightDataToChar(), which takes no
// model as lights off (a PC guard in gra3dMisc.c).  The 3D sound listener
// follows the title's camera, as IngameMain() does when no event camera runs.
//
// THE FRAMING.  A scene with camera_set 0 is framed when its room loads, from
// the room's own cameras: the fixed and four-corner shots its map camera cuts
// between (TitleRoomCollectShots), the first of them panned a few degrees
// either side.  A room without one is framed over its walkable floor.  The
// model's bounding box is no help on its own: it is in the blocks' own frames
// and always takes in the origin.
//
// THE MUSIC.  init_Title_Mode() preloads TitleRoomTitleBgm() -- BGM_00_TITLE,
// or the chosen track -- into BGM slot 0 when it is not already playing, so a
// chosen track starts, loops and is faded out on the way off the title by the
// ROM's own calls, exactly as the theme is.  Only a change made while the
// title is up swaps the slot here.  (Swapping out a theme the title had only
// just started from its preload left a stream behind that never reported
// stopped, and the title's next init waited on SndBgmIsAllIdle() for ever.)

#include "outgame/title_room.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#include "graphics/effect/effect.h"             // CheckSetEffectsOK, EffectDrawMain, EffectsReleaseAll
#include "graphics/effect/effect_oth.h"         // EffSoraDraw, EffectOthBufCopy
#include "graphics/graph2d/g2d_debug.h"         // DispSqrRect
#include "graphics/graph3d/gra3d.h"             // gra3dcam*, gra3dApplyCamera
#include "graphics/mmanage.h"                   // ModelManager, model_collision_set
#include "graphics/model_object.h"              // ModelObject::GetBoundingBox
#include "graphics/scene/IngameScene.h"         // ScnTestIngameEnd
#include "graphics/scene/scene_dat.h"           // SceneWrkRelease
#include "ingame/camera/cam_rail.h"             // CamRailDataFree
#include "ingame/candle_ctrl.h"                 // GetCandleControl
#include "ingame/cat/cat_ctrl.h"                // CatCtrlEnd
#include "ingame/cine_film.h"                   // CineEnd
#include "ingame/enemy/ene_spe_obj.h"           // GetEneSpeObjTbl
#include "ingame/enemy/enemy.h"                 // EnemyReleaseAll
#include "ingame/event/ev_queue.h"              // EvQueueEnd
#include "ingame/event/prg/ev_main.h"           // EventRelease
#include "ingame/ingame.h"                      // IngameInit, IngameSndListenerSet
#include "ingame/ingame_wrk.h"                  // ingame_wrk
#include "ingame/item/prg/item.h"               // AllPlyrItemEnd
#include "ingame/map/DoorStat.h"                // DoorStatApply / Free / End
#include "ingame/map/MapCtrl.h"                 // MapStartNoSet, MapPlayStartReset / IsReady, MapFrameUpdate, MapDrawAll, MapReleaseAll
#include "ingame/map/MapDoor.h"                 // MapDoorDelete
#include "ingame/map/MapFog.h"                  // MapFogProc
#include "ingame/map/MapLoad.h"                 // MapLoadCheckLoadNow, MapLoadWaitMain, MapLoadGetFreeMemAddr, MapLoadGetRoomNoNow
#include "ingame/map/MapPut.h"                  // MapPutDrawFlagClear, MapPutDraw
#include "ingame/map/MapSnap.h"                 // MapSnapEnd
#include "ingame/map/RegDat.h"                  // RegDatGetTopAddr, MB_OUT_HEAD
#include "ingame/map/foot_se.h"                 // foot_seRelease
#include "ingame/map/map_rectangle.h"           // MDAT_CAM
#include "ingame/map/map_bgm.h"                 // map_bgmRelease
#include "ingame/memo/memo.h"                   // MemoNoticeDelete, MemoWrkEnd
#include "ingame/movie_room_menu/prg/movie_projecter.h"   // IsMoviePhase, MovieReqEnd
#include "ingame/photo/n_plyr_camera.h"         // PlyrCameraEnd
#include "ingame/plyr/player.h"                 // PlyrSetCurrent
#include "ingame/plyr/plyr_sub.h"               // PlyrMdlReleaseAll
#include "ingame/radio.h"                       // RadioEnd
#include "ingame/sound/snd_ctrl.h"              // SndBgmPlay / Stop / VolFade / IsFileSet
#include "ingame/story_develop.h"               // DevelopFilmEnd
#include "ingame/story_door.h"                  // MapAreaNoSet, MapAreaSideSet, MapAreaFileNoGet
#include "ingame/story_menu_photo.h"            // DocFlagEnd
#include "ingame/subtitle/subtitle.h"           // SubTitleStop
#include "main/gphase.h"                        // GetGPhaseCtrl, GPHASE_NODE
#include "outgame/title_menu.h"                 // init_Title_Mode / pre_Title_Mode / end_Title_Mode
#include "reipan/reipan_config.h"               // reipan_config.title
#include "reipan/os/reipan_time.h"              // ReiPan_GetPerformanceCounter / Frequency
#include "reipan/rendering/reipan_renderer.h"   // ReiPan_RendererGetViewBounds
#include "system/eeiop/cd_file_dat.h"           // ROS00_PK4, BGM_*, KAN_*
#include "system/eeiop/cddat.h"                 // GetFileSize
#include "system/eeiop/file_cache.h"            // file_cache
#include "system/os/eecdvd.h"                   // IsLoadEndAll
#include "system/pad/pad.h"                     // pad_wrk

#define TITLE_ROOM_AREA_NUM     59      /* area_data.ecb rows 1..58 have rooms */
#define TITLE_ROOM_PI           3.14159265358979f

/* The title theme as init_Title_Mode() plays it: volume 100, pitch 0x1000,
 * looped, in BGM slot 0. */
#define TITLE_ROOM_MUSIC_VOL    100
#define TITLE_ROOM_MUSIC_PITCH  0x1000
#define TITLE_ROOM_MUSIC_SLOT   0
/* Frames: a chosen track comes in softly when it replaces another. */
#define TITLE_ROOM_MUSIC_IN     30

/* Seconds for the room to come up out of black once it is in. */
#define TITLE_ROOM_FADE_SECONDS 1.0f

/* A frame longer than this is a load or a stall, not time to show the room
 * passing -- the camera would jump. */
#define TITLE_ROOM_MAX_STEP     0.1f

/* FF3's world is small: a room is a few hundred units across, and the map
 * camera following Rei in the first room stands about thirty units over the
 * floor.  An eye that high, a captured target ten of those out, and the
 * ground searched from a little over the room's origin. */
#define TITLE_ROOM_EYE_HEIGHT    30.0f
#define TITLE_ROOM_CAPTURE_REACH 300.0f
#define TITLE_ROOM_GROUND_PROBE  10.0f

/* Floor samples across the room's box for the automatic framing, per side. */
#define TITLE_ROOM_GRID          12

/* A room's own shot is panned this far either side of its aim. */
#define TITLE_ROOM_PAN_DEGREES   4.0f

/* Phases the room has to make way for: Title_Move_Movie hands over to the
 * attract movie, and the movie player borrows the room's memory. */
#define TITLE_ROOM_PHASE_MOVE_MOVIE 16

typedef enum
{
    TR_IDLE,                    /* nothing resident                          */
    TR_INIT,                    /* IngameInit() ran; its loads are in flight */
    TR_LOADING,                 /* MapPlayStartReset() asked for the room    */
    TR_READY,                   /* in, and drawn                             */
} TR_LOAD;

typedef struct
{
    int      active;            /* Title_Mode's init has run, its end not    */
    int      ending;            /* end_TitleRoom_Mode() has been called      */
    TR_LOAD  load;
    int      scene;             /* reipan_config.title.scenes[] on screen    */
    int      visited;           /* a scene has been picked before            */
    int      leaving;           /* fading out to the next scene              */
    int      held;              /* the overlay is editing: no moving on      */
    unsigned long long rng;     /* the title's own; the ROM's rand() is untouched */
    int      area;              /* resident or loading                       */
    int      side;
    int      floor;             /* the fog's floor: the room's first RegDat kai */
    int      reload;            /* the overlay asked                         */
    int      failed;            /* an area that did not come up, -1 none     */
    int      released;          /* the current screen took the room away     */
    float    fade;              /* 0 black .. 1 the room                     */
    double   path_time;         /* seconds along the camera path             */
    int      paused;
    int      preview;           /* -1 path, 0 / 1 hold that point            */
    int      fly;
    float    fly_eye[3];
    float    fly_yaw;           /* radians about -Y, 0 looking down +Z       */
    float    fly_pitch;         /* radians, positive looking up              */
    float    room_size;         /* the box's longest side, for the fly speed */
    unsigned long long last_tick;
    int      music_file;        /* the chosen track playing, 0 none          */
    int      music_vol;         /* the level it was last set to              */
} TITLE_ROOM_WRK;

/* Zero is "not on the title"; init_TitleRoom_Mode() sets the rest. */
static TITLE_ROOM_WRK title_room;

static void TitleRoomRelease(void);
static void TitleRoomForgetShots(void);

/* The scene on screen, or loading to be. */
static ReiPan_ConfigTitleScene *TitleRoomScene(void)
{
    ReiPan_ConfigTitle *t = &reipan_config.title;

    if (title_room.scene < 0 || title_room.scene >= t->scene_count)
    {
        title_room.scene = 0;
    }

    return &t->scenes[title_room.scene];
}

// ──────────────────────────────────────────────────────────────────────
// The rooms.
//
// Named as the disc names their model files: area n is ROS00_PK4 + n * 7
// (MapAreaFileNoGet maps the few areas that share a model).  1..46 are the
// Manor of Sleep -- RS1xx..RS5xx, its five parts in the order the game opens
// them -- and 47..52 the Hinasaki house in the real world, whose night
// lighting is its own set of files on side 1.  53..58 are the house's night
// models as areas of their own; area_data.ecb gives them no row the game
// uses, and they are left out.

static const char *const title_room_code[TITLE_ROOM_AREA_NUM] =
{
    "",
    "RS101", "RS102", "RS103", "RS104", "RS105", "RS106",
    "RS201", "RS202", "RS203", "RS204", "RS205", "RS206", "RS207",
    "RS208", "RS209", "RS210", "RS211", "RS212", "RS213",
    "RS301", "RS302", "RS303", "RS304", "RS305", "RS306", "RS307", "RS308",
    "RS401", "RS402", "RS403", "RS404", "RS405", "RS406", "RS407", "RS408",
    "RS409", "RS410",
    "RS501", "RS502", "RS503", "RS504", "RS505", "RS506", "RS507", "RS508",
    "RS509",
    "RRE01", "RRE02", "RRE03", "RRE04", "RRE05", "RRE06",
    "", "", "", "", "", "",
};

int TitleRoomCount(void)
{
    return TITLE_ROOM_AREA_NUM;
}

const char *TitleRoomCode(int area)
{
    if ((unsigned int)area >= TITLE_ROOM_AREA_NUM || title_room_code[area][0] == '\0')
    {
        return "?";
    }

    return title_room_code[area];
}

const char *TitleRoomPlace(int area)
{
    if (area >= 1 && area <= 46)
    {
        return "Manor of Sleep";
    }
    if (area >= 47 && area <= 52)
    {
        return "Hinasaki house";
    }

    return "";
}

int TitleRoomHasNight(int area)
{
    return area >= 47 && area <= 52;
}

int TitleRoomIsOnDisc(int area)
{
    if (area < 1 || area >= TITLE_ROOM_AREA_NUM || title_room_code[area][0] == '\0')
    {
        return 0;
    }

    /* The model PK4 and the side-0 obj_pack, what RoomObjPackLoadReq() asks
     * for first. */
    const int file = ROS00_PK4 + area * 7;
    const u_int mdl = GetFileSize(file);
    const u_int pack = GetFileSize(ROS00_0_PHF + area * 7);

    return mdl != 0 && mdl != (u_int)-1 && pack != 0 && pack != (u_int)-1;
}

// ──────────────────────────────────────────────────────────────────────
// The tracks.
//
// Every stream on the disc that is music or a room's ambience: the BGM_*
// themes and the KAN_* beds the rooms play.  A note is given only where the
// file's own name says what it is.

typedef struct
{
    int         file;
    const char *name;
    const char *note;
} TITLE_ROOM_TRACK;

static const TITLE_ROOM_TRACK title_room_track[] =
{
    { 0,                           "Original",             "the title theme, as the game plays it" },
    { BGM_00_TITLE_STR_STR,        "BGM_00_TITLE",         "title theme" },
    { BGM_01_MENU1_STR_STR,        "BGM_01_MENU1",         "menu" },
    { BGM_02_MENU2_STR_STR,        "BGM_02_MENU2",         "menu" },
    { BGM_03_MENU3_STR_STR,        "BGM_03_MENU3",         "menu (save and load)" },
    { BGM_04_CLEAR_STR_STR,        "BGM_04_CLEAR",         "game clear" },
    { BGM_05_SELECT_STR_STR,       "BGM_05_SELECT",        "select" },
    { BGM_06_START_STR_STR,        "BGM_06_START",         "mission start" },
    { BGM_07_SUCCESS_STR_STR,      "BGM_07_SUCCESS",       "success" },
    { BGM_08_FAILURE_STR_STR,      "BGM_08_FAILURE",       "failure" },
    { BGM_09_GAMEOVER_STR_STR,     "BGM_09_GAMEOVER",      "game over" },
    { BGM_10_OMAKE_STR_STR,        "BGM_10_OMAKE",         "extras" },
    { KAN_00_AME_STR_STR,          "KAN_00_AME",           "rain" },
    { KAN_01_GENKAN_STR_STR,       "KAN_01_GENKAN",        "entrance hall" },
    { KAN_02_IRORI_STR_STR,        "KAN_02_IRORI",         "hearth" },
    { KAN_03_NAKANIWA1_STR_STR,    "KAN_03_NAKANIWA1",     "courtyard" },
    { KAN_04_GENKAN2_STR_STR,      "KAN_04_GENKAN2",       "entrance hall" },
    { KAN_05_NAKANIWA2_STR_STR,    "KAN_05_NAKANIWA2",     "courtyard" },
    { KAN_06_YORU_STR_STR,         "KAN_06_YORU",          "night" },
    { KAN_07_KINPAKU_STR_STR,      "KAN_07_KINPAKU",       "" },
    { KAN_08_TOUSYU_STR_STR,       "KAN_08_TOUSYU",        "" },
    { KAN_09_DAIKUROUKA_STR_STR,   "KAN_09_DAIKUROUKA",    "" },
    { KAN_10_OSHIRASAMA_STR_STR,   "KAN_10_OSHIRASAMA",    "" },
    { KAN_11_SHIKIRI_STR_STR,      "KAN_11_SHIKIRI",       "" },
    { KAN_12_OONAKANIWA_STR_STR,   "KAN_12_OONAKANIWA",    "great courtyard" },
    { KAN_13_YUKASHITA_STR_STR,    "KAN_13_YUKASHITA",     "under the floor" },
    { KAN_14_MIZUROUKA_STR_STR,    "KAN_14_MIZUROUKA",     "water corridor" },
    { KAN_15_RASEN_STR_STR,        "KAN_15_RASEN",         "spiral" },
    { KAN_16_MIZUHIROMA_STR_STR,   "KAN_16_MIZUHIROMA",    "water hall" },
    { KAN_17_KEI1_STR_STR,         "KAN_17_KEI1",          "Kei" },
    { KAN_18_KEI2_STR_STR,         "KAN_18_KEI2",          "Kei" },
    { KAN_19_NANIKAIRU_STR_STR,    "KAN_19_NANIKAIRU",     "" },
    { KAN_20_ROUSOKU_STR_STR,      "KAN_20_ROUSOKU",       "candles" },
    { KAN_21_YUKI_STR_STR,         "KAN_21_YUKI",          "snow" },
    { KAN_22_KOMORI1_STR_STR,      "KAN_22_KOMORI1",       "" },
    { KAN_23_KOMORI2_STR_STR,      "KAN_23_KOMORI2",       "" },
    { KAN_24_KOMORI3_STR_STR,      "KAN_24_KOMORI3",       "" },
    { KAN_25_MUON1_STR_STR,        "KAN_25_MUON1",         "" },
    { KAN_26_MUON2_STR_STR,        "KAN_26_MUON2",         "" },
    { KAN_27_H_GENKAN_STR_STR,     "KAN_27_H_GENKAN",      "entrance hall" },
    { KAN_28_H_NAWA_STR_STR,       "KAN_28_H_NAWA",        "" },
    { KAN_29_H_HIROMA_STR_STR,     "KAN_29_H_HIROMA",      "great hall" },
    { KAN_30_H_IKESU_STR_STR,      "KAN_30_H_IKESU",       "pond" },
    { KAN_31_H_ZASHIKIROU_STR_STR, "KAN_31_H_ZASHIKIROU",  "" },
    { KAN_32_H_NAWADONO_STR_STR,   "KAN_32_H_NAWADONO",    "" },
    { KAN_33_M_KUSABI_STR_STR,     "KAN_33_M_KUSABI",      "" },
    { KAN_34_M_DOZOU_STR_STR,      "KAN_34_M_DOZOU",       "storehouse" },
    { KAN_35_M_HIROMA_STR_STR,     "KAN_35_M_HIROMA",      "great hall" },
    { KAN_36_M_DOMAROUKA_STR_STR,  "KAN_36_M_DOMAROUKA",   "" },
    { KAN_37_M_FUTAGO_STR_STR,     "KAN_37_M_FUTAGO",      "twins" },
    { KAN_38_MIKOHEYA_STR_STR,     "KAN_38_MIKOHEYA",      "" },
    { KAN_39_MISOGI_STR_STR,       "KAN_39_MISOGI",        "" },
    { KAN_40_HIMURO01_STR_STR,     "KAN_40_HIMURO01",      "" },
    { KAN_41_HIMURO02_STR_STR,     "KAN_41_HIMURO02",      "" },
    { KAN_42_KEI3_STR_STR,         "KAN_42_KEI3",          "Kei" },
    { KAN_SE_14_KAZE2_STR_STR,     "KAN_SE_14_KAZE2",      "wind" },
};

#define TITLE_ROOM_TRACK_NUM ((int)(sizeof(title_room_track) / sizeof(title_room_track[0])))

int TitleRoomMusicCount(void)
{
    return TITLE_ROOM_TRACK_NUM;
}

int TitleRoomMusicFile(int index)
{
    return (index >= 0 && index < TITLE_ROOM_TRACK_NUM) ? title_room_track[index].file : 0;
}

const char *TitleRoomMusicName(int index)
{
    return (index >= 0 && index < TITLE_ROOM_TRACK_NUM) ? title_room_track[index].name : "?";
}

const char *TitleRoomMusicNote(int index)
{
    return (index >= 0 && index < TITLE_ROOM_TRACK_NUM) ? title_room_track[index].note : "";
}

int TitleRoomMusicIndex(int file)
{
    for (int i = 0; i < TITLE_ROOM_TRACK_NUM; i++)
    {
        if (title_room_track[i].file == file)
        {
            return i;
        }
    }

    return -1;
}

/* The stream and its header, as SndBgmPlay() tests them before it plays. */
int TitleRoomMusicIsOnDisc(int index)
{
    const int file = TitleRoomMusicFile(index);

    if (file == 0)
    {
        return index == 0;
    }

    const u_int str = GetFileSize(file);
    const u_int hxd = GetFileSize(file + 1);

    return str != 0 && str != (u_int)-1 && hxd != 0 && hxd != (u_int)-1;
}

// ──────────────────────────────────────────────────────────────────────
// Where the title is.

static int TitleRoomBackdropOn(void)
{
    return reipan_config.title.backdrop == REIPAN_TITLE_BACKDROP_ROOM &&
           TitleRoomIsOnDisc(TitleRoomScene()->room);
}

static int TitleRoomPhaseActive(short id)
{
    for (GPHASE_NODE *n = GetGPhaseCtrl()->root; n != nullptr; n = n->child)
    {
        if (n->id == id)
        {
            return 1;
        }
    }

    return 0;
}

/* The attract movie: its phase plays the movie through the movie player,
 * which borrows the room's memory. */
static int TitleRoomScreenTakesRoom(void)
{
    return TitleRoomPhaseActive(TITLE_ROOM_PHASE_MOVE_MOVIE) || IsMoviePhase();
}

/* The backdrop has the screen: a room resident or loading. */
static int TitleRoomOwnsScreen(void)
{
    return title_room.load != TR_IDLE;
}

int TitleRoomHidesTitleArt(void)
{
    if (title_room.active == 0)
    {
        return 0;
    }
    if (TitleRoomOwnsScreen() != 0)
    {
        return 1;
    }
    // Releasing the room must not reveal the classic artwork during exit.
    return title_room.ending != 0 && TitleRoomBackdropOn() != 0 &&
           title_room.failed != TitleRoomScene()->room;
}

int TitleRoomHoldsAttract(void)
{
    return title_room.active != 0 && TitleRoomBackdropOn() != 0 &&
           reipan_config.title.attract_movie == 0;
}

TITLE_ROOM_STATE TitleRoomGetState(void)
{
    if (title_room.active == 0 ||
        reipan_config.title.backdrop != REIPAN_TITLE_BACKDROP_ROOM)
    {
        return TITLE_ROOM_OFF;
    }
    if (TitleRoomIsOnDisc(TitleRoomScene()->room) == 0 ||
        title_room.failed == TitleRoomScene()->room)
    {
        return TITLE_ROOM_MISSING;
    }
    if (title_room.released != 0)
    {
        return TITLE_ROOM_RELEASED;
    }
    if (title_room.load == TR_READY)
    {
        return TITLE_ROOM_SHOWN;
    }

    return TITLE_ROOM_LOADING;
}

// ──────────────────────────────────────────────────────────────────────
// Bringing the room up and letting it go.

static void TitleRoomBeginLoad(void)
{
    const ReiPan_ConfigTitleScene *s = TitleRoomScene();

    title_room.area = s->room;
    title_room.side = TitleRoomHasNight(s->room) ? (s->side != 0) : 0;
    title_room.floor = 0;
    title_room.fade = 0.0f;
    title_room.load = TR_INIT;

    printf("[title] room: loading area %d (%s%s)\n", title_room.area,
           TitleRoomCode(title_room.area), title_room.side != 0 ? ", night" : "");

    IngameInit();

    /* MapDrawRooms() reads the current player's flags; IngameWrkInit() left
     * the pointer null and PlyrWrkInitAll() put it back, but nothing here
     * depends on which of the two ran last. */
    if (ingame_wrk.plyr_wrk == nullptr)
    {
        PlyrSetCurrent(0);
    }
}

/* IngameEnd() (ingame.c, FF3 0x001d1730) without the five calls that would
 * reach past the room into the title:
 *
 *   SndIngameStopAll(0)           every BGM, every voice, every SE bank but
 *                                 the default -- the title theme and
 *                                 MENU_START_BD, which New Game's OK plays
 *   LayoutAnimResetAll()          the layout animations; the room starts none
 *   GetLayoutSeWrk()->StopAll()   the same for their sounds
 *   IngameSndMuteReset()          the channel-0 mute count; the room mutes nothing
 *   TitleStartFlagClear()         New Game / Load Game's choice
 *   TitleMenuFlagSet()            the difficulty the cursor last stood on
 *
 * Everything IngameInit() allocated is freed, in IngameEnd()'s order. */
static void TitleRoomIngameEnd(void)
{
    CamRailDataFree();
    EvQueueEnd();
    map_bgmRelease(1);
    foot_seRelease();
    EventRelease();
    MapAreaDataFree();
    MapSnapEnd();
    GetCandleControl()->Term();
    MapDoorDelete();
    DoorStatFree();
    DoorStatEnd();
    CineEnd();
    PlyrMdlReleaseAll();
    EnemyReleaseAll();
    MapReleaseAll();
    AllPlyrItemEnd();
    MemoNoticeDelete();
    MemoWrkEnd();
    DocFlagEnd();
    DevelopFilmEnd();
    RadioEnd();
    ScnTestIngameEnd();
    SubTitleStop();
    CatCtrlEnd();
    PlyrCameraEnd();
    model_collision_set.Term();
    GetEneSpeObjTbl()->Release();
    EffectsReleaseAll();
    MovieReqEnd();
    SceneWrkRelease();
    file_cache.ReleaseAll();
}

/* Nothing may be freed under a load in flight.  Pumps the room load toward
 * done; non-zero once there is nothing left to wait for. */
static int TitleRoomSettled(void)
{
    file_cache.PollAll();

    if (title_room.load == TR_LOADING || title_room.load == TR_READY)
    {
        if (MapLoadWaitMain() != 0)
        {
            return 0;
        }
    }

    /* The file cache's loads are the loader's too. */
    return IsLoadEndAll() != 0;
}

/* Drop what is resident, once nothing is loading.  Returns non-zero when the
 * room is gone (or was never there). */
static int TitleRoomTryRelease(void)
{
    if (title_room.load == TR_IDLE)
    {
        return 1;
    }
    if (TitleRoomSettled() == 0)
    {
        return 0;
    }

    TitleRoomRelease();
    return 1;
}

static void TitleRoomRelease(void)
{
    if (title_room.load != TR_IDLE)
    {
        printf("[title] room: releasing area %d\n", title_room.area);
        TitleRoomIngameEnd();
    }

    TitleRoomForgetShots();
    title_room.load = TR_IDLE;
    title_room.fade = 0.0f;
}

static MAP_ROOM_DAT *TitleRoomDat(void)
{
    return MapLoadGetFreeMemAddr(0);
}

/* The room model's box in the world.  The model is placed at the room's
 * origin with no scale or turn (MapDrawRoom), so the box moves by the origin
 * only. */
static int TitleRoomBox(float *lo, float *hi)
{
    MAP_ROOM_DAT *dat = TitleRoomDat();
    float bb[8][4];

    if (dat == nullptr || dat->mdl.mot == nullptr || dat->mdl.mot->ani_ctrl == nullptr)
    {
        return 0;
    }

    memset(bb, 0, sizeof(bb));
    dat->mdl.mot->ani_ctrl->GetBoundingBox(bb);

    for (int k = 0; k < 3; k++)
    {
        lo[k] = 1.0e30f;
        hi[k] = -1.0e30f;
    }
    for (int i = 0; i < 8; i++)
    {
        for (int k = 0; k < 3; k++)
        {
            const float w = bb[i][k] + dat->origin[k];

            lo[k] = fminf(lo[k], w);
            hi[k] = fmaxf(hi[k], w);
        }
    }

    return 1;
}

/* The ground under (x, z), searched from a little above the room's origin --
 * the walk height map MapAreaGetHeight() keeps the player on.  0 off the
 * map: outside the walls, or over a hole. */
static int TitleRoomGround(float x, float z, float *ground)
{
    MAP_ROOM_DAT *dat = TitleRoomDat();
    float pos[4] = { x, dat->origin[1] - TITLE_ROOM_GROUND_PROBE, z, 1.0f };
    float out[4];

    if (MapLoadGetRoomData(dat, 2) == nullptr ||
        MapAreaGetHeight(out, pos, title_room.area, 0) <= 0)
    {
        return 0;
    }

    *ground = out[1];
    return 1;
}

// ──────────────────────────────────────────────────────────────────────
// The room's own cameras.
//
// Every room carries the shots its map camera cuts between: camera rectangles
// (RegDat type 1) whose records (MDAT_CAM) are fixed cameras for follow modes
// 0 and 4 -- an eye, a target and a field of view, as MapCamTypeFix() uses
// them -- and four-corner rectangles (type 9) whose records (MDAT_CAM_SP) hold
// a shot per corner, aimed at the room for types 0 / 1 (2 / 3 aim at the
// player).  Both are relative to the room's offset, which the map camera adds
// (MapCamGra3dcamSetPositionAddOffset).  The other follow modes ride the
// player and say nothing on their own.  These are what the designers framed
// the room with, so they are what a room is framed with here.

typedef struct
{
    float eye[3];
    float target[3];
    float fov;                  /* degrees */
} TITLE_ROOM_SHOT;

#define TITLE_ROOM_SHOT_MAX     48

static TITLE_ROOM_SHOT title_room_shot[TITLE_ROOM_SHOT_MAX];
static int title_room_shot_num;
static int title_room_shot_cur = -1;

static void TitleRoomShotAdd(const float *ofs, const float *eye, const float *target, float fov)
{
    TITLE_ROOM_SHOT shot;

    if (title_room_shot_num >= TITLE_ROOM_SHOT_MAX)
    {
        return;
    }

    for (int k = 0; k < 3; k++)
    {
        shot.eye[k] = ofs[k] + eye[k];
        shot.target[k] = ofs[k] + target[k];
    }
    /* <= 0 keeps the camera's fov; the game's own default is 0.9 rad. */
    shot.fov = (fov > 0.05f && fov < 3.0f ? fov : 0.9f) * 180.0f / TITLE_ROOM_PI;

    /* Several rectangles share one record; a shot is listed once. */
    for (int i = 0; i < title_room_shot_num; i++)
    {
        if (memcmp(&title_room_shot[i], &shot, sizeof(shot)) == 0)
        {
            return;
        }
    }

    title_room_shot[title_room_shot_num++] = shot;
}

static void TitleRoomForgetShots(void)
{
    title_room_shot_num = 0;
    title_room_shot_cur = -1;
}

static void TitleRoomCollectShots(void)
{
    MAP_ROOM_DAT *dat = TitleRoomDat();
    float ofs[4];

    TitleRoomForgetShots();
    MapLoadGetOffsetVector(ofs, 0);

    for (int r = 0; r < REG_LIST_NUM; r++)
    {
        const int slot = dat->reg_list[r];

        if (slot < 0)
        {
            continue;
        }

        MB_OUT_RECT *rec = RegDatGetVecPtr(slot, 1);
        int n = RegDatGetVecNum(slot, 1);
        for (int i = 0; i < n && rec != nullptr; i++, rec = RegDatNextVec(rec))
        {
            const MDAT_CAM *cam = (const MDAT_CAM *)RegDatGetStPtr(slot, rec->reg_id);

            if (cam != nullptr && (cam->type == 0 || cam->type == 4))
            {
                TitleRoomShotAdd(ofs, cam->Pos, cam->View, cam->Rrg);
            }
        }

        rec = RegDatGetVecPtr(slot, RECORD_TYPE_FOUR_CORNER_CAMERA);
        n = RegDatGetVecNum(slot, RECORD_TYPE_FOUR_CORNER_CAMERA);
        for (int i = 0; i < n && rec != nullptr; i++, rec = RegDatNextVec(rec))
        {
            const MDAT_CAM_SP *sp = (const MDAT_CAM_SP *)RegDatGetStPtr(slot, rec->reg_id);

            if (sp != nullptr && (sp->type == 0 || sp->type == 1))
            {
                for (int k = 0; k < 4; k++)
                {
                    TitleRoomShotAdd(ofs, sp->st[k].Cam_Pos, sp->st[k].View_Pos, sp->st[k].Proj);
                }
            }
        }
    }

    printf("[title] room: area %d has %d shots of its own\n", title_room.area, title_room_shot_num);
}

/* Point A and B from one of the room's shots: its eye, its field of view, and
 * the target swung a few degrees either side about the eye, so the camera
 * pans gently across what the shot frames without leaving the spot the
 * designers put it on. */
static void TitleRoomFrameFromShot(ReiPan_ConfigTitleScene *s, int index)
{
    const TITLE_ROOM_SHOT *shot = &title_room_shot[index];
    const float dx = shot->target[0] - shot->eye[0];
    const float dz = shot->target[2] - shot->eye[2];

    for (int p = 0; p < 2; p++)
    {
        const float a = (p == 0 ? -TITLE_ROOM_PAN_DEGREES : TITLE_ROOM_PAN_DEGREES) * TITLE_ROOM_PI / 180.0f;
        float *eye = p == 0 ? s->a_eye : s->b_eye;
        float *target = p == 0 ? s->a_target : s->b_target;

        for (int k = 0; k < 3; k++)
        {
            eye[k] = shot->eye[k];
        }
        target[0] = shot->eye[0] + dx * cosf(a) - dz * sinf(a);
        target[1] = shot->target[1];
        target[2] = shot->eye[2] + dx * sinf(a) + dz * cosf(a);
    }
    s->a_fov = s->b_fov = shot->fov;
    s->camera_set = 1;
    title_room_shot_cur = index;

    printf("[title] room: framed area %d from its shot %d: eye (%.1f %.1f %.1f) at (%.1f %.1f %.1f) fov %.1f\n",
           title_room.area, index, (double)shot->eye[0], (double)shot->eye[1], (double)shot->eye[2],
           (double)shot->target[0], (double)shot->target[1], (double)shot->target[2], (double)shot->fov);
}

int TitleRoomShotCount(void)
{
    return title_room.load == TR_READY ? title_room_shot_num : 0;
}

int TitleRoomShotCurrent(void)
{
    return title_room.load == TR_READY ? title_room_shot_cur : -1;
}

int TitleRoomUseShot(int scene, int index)
{
    if (title_room.active == 0 || title_room.load != TR_READY || scene != title_room.scene ||
        index < 0 || index >= title_room_shot_num ||
        scene < 0 || scene >= reipan_config.title.scene_count ||
        reipan_config.title.scenes[scene].room != title_room.area)
    {
        return 0;
    }

    TitleRoomFrameFromShot(&reipan_config.title.scenes[scene], index);
    title_room.path_time = 0.0;
    return 1;
}

/* Place both points by themselves when the room has no shot of its own: the
 * floor the player can walk is sampled on a grid over the model's box, and the
 * two eyes stand over it near the two ends of its long side, at Rei's eye
 * height, both looking at the middle of it -- the shot turns as it travels.  A
 * room with no walk map gets the same from the box alone.  Y grows
 * downwards. */
static void TitleRoomFrameFromFloor(ReiPan_ConfigTitleScene *s)
{
    MAP_ROOM_DAT *dat = TitleRoomDat();
    float lo[3];
    float hi[3];
    float walk_lo[3] = {  1.0e30f,  1.0e30f,  1.0e30f };
    float walk_hi[3] = { -1.0e30f, -1.0e30f, -1.0e30f };
    float sum[3] = { 0.0f, 0.0f, 0.0f };
    int n = 0;

    if (TitleRoomBox(lo, hi) == 0)
    {
        return;
    }

    for (int i = 0; i < TITLE_ROOM_GRID; i++)
    {
        for (int j = 0; j < TITLE_ROOM_GRID; j++)
        {
            const float x = lo[0] + (hi[0] - lo[0]) * ((float)i + 0.5f) / (float)TITLE_ROOM_GRID;
            const float z = lo[2] + (hi[2] - lo[2]) * ((float)j + 0.5f) / (float)TITLE_ROOM_GRID;
            float y;

            if (TitleRoomGround(x, z, &y) == 0)
            {
                continue;
            }

            const float p[3] = { x, y, z };
            for (int k = 0; k < 3; k++)
            {
                walk_lo[k] = fminf(walk_lo[k], p[k]);
                walk_hi[k] = fmaxf(walk_hi[k], p[k]);
                sum[k] += p[k];
            }
            n++;
        }
    }

    float c[3];
    float a[3];
    float b[3];

    if (n >= 2)
    {
        for (int k = 0; k < 3; k++)
        {
            c[k] = sum[k] / (float)n;
        }
        /* The ends of the walkable floor's long side, pulled a fifth of the
         * way in so the lens is not against a wall, on the floor's middle
         * line across. */
        const int ax = (walk_hi[0] - walk_lo[0]) >= (walk_hi[2] - walk_lo[2]) ? 0 : 2;
        const int across = ax == 0 ? 2 : 0;

        a[ax] = walk_lo[ax] + (walk_hi[ax] - walk_lo[ax]) * 0.2f;
        b[ax] = walk_hi[ax] - (walk_hi[ax] - walk_lo[ax]) * 0.2f;
        a[across] = b[across] = c[across];

        /* Each eye over the ground where it stands, else the floor's mean. */
        float y;
        a[1] = TitleRoomGround(a[0], a[2], &y) != 0 ? y : c[1];
        b[1] = TitleRoomGround(b[0], b[2], &y) != 0 ? y : c[1];
    }
    else
    {
        /* No walk map: the origin is the floor. */
        const int ax = (hi[0] - lo[0]) >= (hi[2] - lo[2]) ? 0 : 2;
        const int across = ax == 0 ? 2 : 0;

        c[0] = (lo[0] + hi[0]) * 0.5f;
        c[1] = dat->origin[1];
        c[2] = (lo[2] + hi[2]) * 0.5f;
        a[ax] = lo[ax] + (hi[ax] - lo[ax]) * 0.25f;
        b[ax] = hi[ax] - (hi[ax] - lo[ax]) * 0.25f;
        a[across] = b[across] = c[across];
        a[1] = b[1] = c[1];
    }

    for (int k = 0; k < 3; k++)
    {
        s->a_eye[k] = a[k];
        s->b_eye[k] = b[k];
        s->a_target[k] = s->b_target[k] = c[k];
    }
    s->a_eye[1] -= TITLE_ROOM_EYE_HEIGHT;
    s->b_eye[1] -= TITLE_ROOM_EYE_HEIGHT;
    /* A little under eye height: the floor and the furniture on it. */
    s->a_target[1] = s->b_target[1] = c[1] - TITLE_ROOM_EYE_HEIGHT * 0.6f;
    s->a_fov = s->b_fov = 50.0f;
    s->camera_set = 1;

    printf("[title] room: framed area %d over %d floor samples, box (%.1f %.1f %.1f)..(%.1f %.1f %.1f)\n",
           title_room.area, n, (double)lo[0], (double)lo[1], (double)lo[2],
           (double)hi[0], (double)hi[1], (double)hi[2]);
}

/* The room's first shot of its own, else the floor's framing. */
static void TitleRoomFrame(ReiPan_ConfigTitleScene *s)
{
    if (title_room_shot_num > 0)
    {
        TitleRoomFrameFromShot(s, 0);
    }
    else
    {
        TitleRoomFrameFromFloor(s);
    }
}

/* The longest side of the room's box, for the fly speed. */
static void TitleRoomMeasure(void)
{
    MAP_ROOM_DAT *dat = TitleRoomDat();
    float bb[8][4];
    float lo[3] = {  1.0e30f,  1.0e30f,  1.0e30f };
    float hi[3] = { -1.0e30f, -1.0e30f, -1.0e30f };

    title_room.room_size = 300.0f;
    if (dat == nullptr || dat->mdl.mot == nullptr || dat->mdl.mot->ani_ctrl == nullptr)
    {
        return;
    }

    memset(bb, 0, sizeof(bb));
    dat->mdl.mot->ani_ctrl->GetBoundingBox(bb);
    for (int i = 0; i < 8; i++)
    {
        for (int k = 0; k < 3; k++)
        {
            lo[k] = fminf(lo[k], bb[i][k]);
            hi[k] = fmaxf(hi[k], bb[i][k]);
        }
    }

    const float size = fmaxf(hi[0] - lo[0], fmaxf(hi[1] - lo[1], hi[2] - lo[2]));
    if (size > 1.0f)
    {
        title_room.room_size = size;
    }
}

/* One frame of the bring-up.  The steps are IngameNowLoadingMain()'s 0..2
 * for a start without the player. */
static void TitleRoomLoadMain(void)
{
    file_cache.PollAll();

    if (title_room.load == TR_INIT)
    {
        if (IsLoadEndAll() == 0)
        {
            return;
        }

        MapAreaSideSet((u_int)title_room.side);
        MapAreaNoSet(title_room.area);
        MapStartNoSet(title_room.area);
        MapPlayStartReset();
        title_room.load = TR_LOADING;
        return;
    }

    if (title_room.load != TR_LOADING)
    {
        return;
    }

    if (IsLoadEndAll() == 0 || MapLoadCheckLoadNow() != 0 || CheckSetEffectsOK() == 0 ||
        MapPlayStartIsReady() == 0)
    {
        MapLoadWaitMain();
        return;
    }

    MAP_ROOM_DAT *dat = TitleRoomDat();
    if (dat == nullptr || dat->area != title_room.area || dat->mdl.mot == nullptr)
    {
        printf("[title] room: area %d did not come up\n", title_room.area);
        title_room.failed = title_room.area;
        TitleRoomRelease();
        return;
    }

    /* The floor the fog asks the registration data about. */
    MB_OUT_HEAD *head = dat->reg_list[0] >= 0 ? RegDatGetTopAddr(dat->reg_list[0]) : nullptr;
    title_room.floor = head != nullptr ? head->kai : 0;

    DoorStatApply();
    TitleRoomMeasure();
    TitleRoomCollectShots();

    if (TitleRoomScene()->camera_set == 0)
    {
        TitleRoomFrame(TitleRoomScene());
    }

    printf("[title] room: area %d up, floor %d\n", title_room.area, title_room.floor);

    title_room.load = TR_READY;
    title_room.path_time = 0.0;
    title_room.fade = 0.0f;
}

/* Keep what is resident in step with the settings and the screen.  Every
 * release here waits for the loads first; until it can happen the room is
 * not drawn. */
static void TitleRoomFollow(void)
{
    if (title_room.ending != 0)
    {
        return;
    }
    const ReiPan_ConfigTitleScene *s = TitleRoomScene();
    const int side = TitleRoomHasNight(s->room) ? (s->side != 0) : 0;
    int want = TitleRoomBackdropOn();

    title_room.released = TitleRoomScreenTakesRoom();
    if (title_room.released != 0 || title_room.ending != 0)
    {
        want = 0;
    }

    if (title_room.reload != 0 ||
        (title_room.load != TR_IDLE &&
         (want == 0 || title_room.area != s->room || title_room.side != side)))
    {
        if (TitleRoomTryRelease() == 0)
        {
            return;
        }
        if (title_room.reload != 0)
        {
            title_room.reload = 0;
            title_room.failed = -1;
        }
    }

    if (want != 0 && title_room.load == TR_IDLE && title_room.failed != s->room)
    {
        TitleRoomBeginLoad();
    }
}

// ──────────────────────────────────────────────────────────────────────
// The camera.

static float TitleRoomEase(float x)
{
    if (TitleRoomScene()->easing == REIPAN_TITLE_EASING_LINEAR)
    {
        return x;
    }

    return x * x * (3.0f - 2.0f * x);
}

/* Seconds in one whole cycle of the path. */
static double TitleRoomCycle(void)
{
    const ReiPan_ConfigTitleScene *s = TitleRoomScene();
    const double travel = s->travel_seconds > 0.5f ? s->travel_seconds : 0.5;
    const double hold = s->hold_seconds > 0.0f ? s->hold_seconds : 0.0;

    switch (s->motion)
    {
    case REIPAN_TITLE_MOTION_LOOP:
    case REIPAN_TITLE_MOTION_ONCE:
        return hold * 2.0 + travel;

    default:
        return (hold + travel) * 2.0;
    }
}

/* How far from A to B the camera is at `time`, 0..1, eased. */
static float TitleRoomPathAt(double time)
{
    const ReiPan_ConfigTitleScene *s = TitleRoomScene();
    const double travel = s->travel_seconds > 0.5f ? s->travel_seconds : 0.5;
    const double hold = s->hold_seconds > 0.0f ? s->hold_seconds : 0.0;
    const double cycle = TitleRoomCycle();
    double x;

    if (s->motion == REIPAN_TITLE_MOTION_ONCE)
    {
        x = time < cycle ? time : cycle;
    }
    else
    {
        x = fmod(time, cycle);
    }

    if (x < hold)
    {
        return 0.0f;
    }
    x -= hold;
    if (x < travel)
    {
        return TitleRoomEase((float)(x / travel));
    }
    x -= travel;
    if (x < hold || s->motion != REIPAN_TITLE_MOTION_PINGPONG)
    {
        return 1.0f;
    }
    x -= hold;

    return TitleRoomEase((float)(1.0 - x / travel));
}

static float TitleRoomLerp(float a, float b, float e)
{
    return a + (b - a) * e;
}

/* The direction the fly camera looks, from its yaw and pitch.  Y grows
 * downwards, so looking up is -Y. */
static void TitleRoomFlyDir(float *dir)
{
    const float cp = cosf(title_room.fly_pitch);

    dir[0] = sinf(title_room.fly_yaw) * cp;
    dir[1] = -sinf(title_room.fly_pitch);
    dir[2] = cosf(title_room.fly_yaw) * cp;
}

/* Seed the fly camera from wherever the camera is now. */
static void TitleRoomFlySeed(void)
{
    const float *pos = gra3dcamGetPosition();
    const float *aim = gra3dcamGetTarget();
    const float d[3] = { aim[0] - pos[0], aim[1] - pos[1], aim[2] - pos[2] };
    const float flat = sqrtf(d[0] * d[0] + d[2] * d[2]);

    title_room.fly_eye[0] = pos[0];
    title_room.fly_eye[1] = pos[1];
    title_room.fly_eye[2] = pos[2];
    title_room.fly_yaw = atan2f(d[0], d[2]);
    title_room.fly_pitch = atan2f(-d[1], flat > 1.0e-4f ? flat : 1.0e-4f);
}

/* A stick byte as -1..1, with a dead zone. */
static float TitleRoomStick(u_char raw)
{
    const float v = ((float)raw - 128.0f) / 127.0f;

    if (fabsf(v) < 0.2f)
    {
        return 0.0f;
    }

    return v < 0.0f ? (v + 0.2f) / 0.8f : (v - 0.2f) / 0.8f;
}

/* Steer the fly camera with pad 0, then take the pad away from the title so
 * its menus do not move under it:
 *   left stick   move (forward / back, strafe)
 *   right stick  look
 *   R1 / L1      up / down
 *   R2 / L2      held: fast / slow */
static void TitleRoomFlyMain(float dt)
{
    PAD_WRK *pad = &pad_wrk[0];
    const u_short held = pad->held;
    float speed = title_room.room_size * 0.25f;
    float dir[3];

    if ((held & 0x0002) != 0)           /* R2 */
    {
        speed *= 4.0f;
    }
    if ((held & 0x0001) != 0)           /* L2 */
    {
        speed *= 0.25f;
    }

    const float look_x = TitleRoomStick(pad->analog[0]);
    const float look_y = TitleRoomStick(pad->analog[1]);
    const float move_x = TitleRoomStick(pad->analog[2]);
    const float move_y = TitleRoomStick(pad->analog[3]);

    title_room.fly_yaw += look_x * 1.8f * dt;
    title_room.fly_pitch -= look_y * 1.4f * dt;
    if (title_room.fly_pitch > 1.5f)
    {
        title_room.fly_pitch = 1.5f;
    }
    if (title_room.fly_pitch < -1.5f)
    {
        title_room.fly_pitch = -1.5f;
    }

    TitleRoomFlyDir(dir);

    /* Strafe along the ground, to the camera's right. */
    const float right[3] = { cosf(title_room.fly_yaw), 0.0f, -sinf(title_room.fly_yaw) };
    const float step = speed * dt;

    for (int k = 0; k < 3; k++)
    {
        title_room.fly_eye[k] += (dir[k] * -move_y + right[k] * move_x) * step;
    }
    if ((held & 0x0008) != 0)           /* R1: up */
    {
        title_room.fly_eye[1] -= step;
    }
    if ((held & 0x0004) != 0)           /* L1: down */
    {
        title_room.fly_eye[1] += step;
    }

    /* The menus read held / trg / rpt; with them clear the title stands
     * still while the camera moves. */
    pad->held = 0;
    pad->trg = 0;
    pad->rpt = 0;
}

static void TitleRoomCameraMain(float dt)
{
    const ReiPan_ConfigTitleScene *s = TitleRoomScene();
    float eye[4];
    float target[4];
    float fov;

    if (title_room.fly != 0)
    {
        float dir[3];

        TitleRoomFlyMain(dt);
        TitleRoomFlyDir(dir);
        for (int k = 0; k < 3; k++)
        {
            eye[k] = title_room.fly_eye[k];
            target[k] = eye[k] + dir[k] * TITLE_ROOM_CAPTURE_REACH;
        }
        fov = TitleRoomLerp(s->a_fov, s->b_fov, 0.5f);
    }
    else
    {
        float e;

        if (title_room.preview == 0)
        {
            e = 0.0f;
        }
        else if (title_room.preview == 1)
        {
            e = 1.0f;
        }
        else
        {
            e = TitleRoomPathAt(title_room.path_time);
        }

        for (int k = 0; k < 3; k++)
        {
            eye[k] = TitleRoomLerp(s->a_eye[k], s->b_eye[k], e);
            target[k] = TitleRoomLerp(s->a_target[k], s->b_target[k], e);
        }
        fov = TitleRoomLerp(s->a_fov, s->b_fov, e);
    }
    eye[3] = 1.0f;
    target[3] = 1.0f;

    gra3dcamSetFov(fov * TITLE_ROOM_PI / 180.0f);
    gra3dcamSetRoll(0.0f);
    gra3dcamSetPosition(eye);
    gra3dcamSetTarget(target, 1);
    gra3dApplyCamera(nullptr, 1, 1);

    /* IngameMain() does this whenever no event camera owns the listener. */
    IngameSndListenerSet(0);
}

// ──────────────────────────────────────────────────────────────────────
// The rotation.

/* xorshift64*, seeded off the clock the first time.  The scene pick must not
 * come out of rand(): that is the ROM's stream, and a draw here would shift
 * every random value the game takes after it. */
static unsigned int TitleRoomRandom(void)
{
    unsigned long long x = title_room.rng;

    if (x == 0)
    {
        x = ReiPan_GetPerformanceCounter() | 1;
    }
    x ^= x >> 12;
    x ^= x << 25;
    x ^= x >> 27;
    title_room.rng = x;

    return (unsigned int)((x * 2685821657736338717ULL) >> 32);
}

/* The scenes the rotation may land on: enabled, and their room on the disc. */
static int TitleRoomPool(int *pool)
{
    const ReiPan_ConfigTitle *t = &reipan_config.title;
    int n = 0;

    for (int i = 0; i < t->scene_count; i++)
    {
        if (t->scenes[i].enabled != 0 && TitleRoomIsOnDisc(t->scenes[i].room) != 0)
        {
            pool[n++] = i;
        }
    }

    return n;
}

/* A scene at random, never `avoid` while there is any other to pick.  With
 * none enabled, the first one -- the backdrop has to show something. */
static int TitleRoomPickScene(int avoid)
{
    int pool[REIPAN_TITLE_SCENE_MAX];
    int n = TitleRoomPool(pool);

    if (n == 0)
    {
        return 0;
    }
    if (n > 1)
    {
        for (int i = 0; i < n; i++)
        {
            if (pool[i] == avoid)
            {
                pool[i] = pool[--n];
                break;
            }
        }
    }

    return pool[TitleRoomRandom() % (unsigned int)n];
}

/* Time to fade out: `passes` runs of the path, less the fade, so the black
 * lands as the last run ends. */
static int TitleRoomTimeToMove(void)
{
    const ReiPan_ConfigTitle *t = &reipan_config.title;
    int pool[REIPAN_TITLE_SCENE_MAX];

    if (title_room.held != 0 || t->change != REIPAN_TITLE_CHANGE_PASS ||
        TitleRoomPool(pool) < 2)
    {
        return 0;
    }

    return title_room.path_time >= TitleRoomCycle() * t->passes - TITLE_ROOM_FADE_SECONDS;
}

/* Take up `scene`.  Another room means a release and a load; TitleRoomFollow()
 * does both, the release as soon as nothing is loading. */
static void TitleRoomEnterScene(int scene)
{
    title_room.scene = scene;
    title_room.leaving = 0;
    title_room.path_time = 0.0;
    title_room.preview = -1;

    printf("[title] room: scene %d, %s\n", scene, TitleRoomScene()->name);

    if (title_room.load == TR_READY && TitleRoomScene()->room == title_room.area &&
        TitleRoomScene()->camera_set == 0)
    {
        TitleRoomFrame(TitleRoomScene());
    }
}

// ──────────────────────────────────────────────────────────────────────
// Drawing.

/* IngameDrawSub()'s map half (ingame.c, FF3 0x001d1ed0) with nobody in the
 * room: the fog at the camera, the sky, both room buffers, the furniture and
 * doors, the effects. */
static void TitleRoomDraw(void)
{
    /* IngameSetDrawEnable()'s words with no map camera and nobody in the
     * room; a boot that has not played yet still has them at 0. */
    draw_en_371300 = 1;
    draw_en_371304 = 1;
    draw_en_371308 = 0;
    draw_en_371348 = 1;
    draw_en_37126c = draw_en_37127c;

    MapLoadCheckLoadNow();
    MapPutDrawFlagClear();
    MapFogProc(title_room.area, title_room.floor);
    EffSoraDraw();
    MapDrawAll();
    MapPutDraw(0);
    EffectDrawMain(0);
    EffectOthBufCopy();

    /* As IngameDrawSub() leaves them. */
    draw_en_371300 = 1;
    draw_en_371304 = 1;
    draw_en_371308 = 1;
    draw_en_371348 = 1;
    draw_en_37126c = 1;
}

/* Black over the room and under everything the title draws after it: the
 * fade up out of black, and the dimming that keeps the logo readable.  Wide
 * enough to reach the window's edges, where the room is drawn too. */
static void TitleRoomDrawVeil(void)
{
    const float dim = reipan_config.title.dim;
    const float black = 1.0f - (1.0f - dim) * title_room.fade;
    int alpha = (int)(black * 128.0f + 0.5f);
    float x0, y0, x1, y1;

    if (alpha <= 0)
    {
        return;
    }
    if (alpha > 0x80)
    {
        alpha = 0x80;
    }

    ReiPan_RendererGetViewBounds(&x0, &y0, &x1, &y1);
    DispSqrRect(x0, y0, x1, y1, 0, 0, 0, 0, (u_char)alpha);
}

void TitleRoomDrawBackdrop(void)
{
    if (title_room.active == 0)
    {
        return;
    }

    if (title_room.load == TR_READY)
    {
        TitleRoomDraw();
        TitleRoomDrawVeil();
    }
}

// ──────────────────────────────────────────────────────────────────────
// The music.

static int TitleRoomMusicVolume(void)
{
    return (int)(reipan_config.title.music_volume * (float)TITLE_ROOM_MUSIC_VOL + 0.5f);
}

/* The chosen track's file, 0 for the game's own (or one not on the disc). */
static int TitleRoomChosenMusic(void)
{
    const int file = reipan_config.title.music;
    const int index = TitleRoomMusicIndex(file);

    return file != 0 && index >= 0 && TitleRoomMusicIsOnDisc(index) != 0 ? file : 0;
}

int TitleRoomTitleBgm(void)
{
    const int file = TitleRoomChosenMusic();

    return file != 0 ? file : BGM_00_TITLE_STR_STR;
}

/* The track in slot 0 follows the setting while the title is up.  Coming onto
 * the title it already does: init_Title_Mode() preloaded TitleRoomTitleBgm().
 * A change made on the title fades the slot out over a second and starts the
 * other track in its place. */
static void TitleRoomMusicMain(void)
{
    const int file = TitleRoomTitleBgm();
    const int vol = TitleRoomChosenMusic() != 0 ? TitleRoomMusicVolume() : TITLE_ROOM_MUSIC_VOL;

    /* Not while the attract movie has the sound, nor once the title is on
     * its way out: end_Title_Mode() fades slot 0 itself. */
    if (title_room.ending != 0 || TitleRoomScreenTakesRoom() != 0)
    {
        return;
    }

    if (SndBgmIsFileSet(file) != 0)
    {
        if (title_room.music_file != file || vol != title_room.music_vol)
        {
            SndBgmVolFade(vol, 10, TITLE_ROOM_MUSIC_SLOT);
        }
        title_room.music_file = file;
        title_room.music_vol = vol;
        return;
    }

    /* Only a swap the title was playing into: the slot is left alone while
     * nothing plays in it (the theme between its runs, a screen's own). */
    if (title_room.music_file == 0 || SndBgmIsFileSet(title_room.music_file) == 0)
    {
        return;
    }

    printf("[title] room: music %s (%#x)\n", TitleRoomMusicName(TitleRoomMusicIndex(file)), file);

    SndBgmStop(TITLE_ROOM_MUSIC_IN, TITLE_ROOM_MUSIC_SLOT);
    SndBgmPlay(file, vol, TITLE_ROOM_MUSIC_PITCH, TITLE_ROOM_MUSIC_IN, nullptr, 1,
               TITLE_ROOM_MUSIC_SLOT);
    title_room.music_file = file;
    title_room.music_vol = vol;
}

void TitleRoomMusicChanged(void)
{
    if (title_room.active != 0)
    {
        TitleRoomMusicMain();
    }
}

// ──────────────────────────────────────────────────────────────────────
// For the overlay.

void TitleRoomReload(void)
{
    title_room.reload = 1;
}

int TitleRoomCurrentScene(void)
{
    return title_room.scene;
}

void TitleRoomShowScene(int scene)
{
    if (scene < 0 || scene >= reipan_config.title.scene_count ||
        (scene == title_room.scene && title_room.leaving == 0))
    {
        return;
    }

    TitleRoomEnterScene(scene);
}

void TitleRoomHoldScene(int hold)
{
    title_room.held = hold != 0;

    /* A fade already under way when the hold came down finishes as a fade
     * back in, on the scene it was leaving. */
    if (hold != 0)
    {
        title_room.leaving = 0;
    }
}

void TitleRoomFrameAutomatically(int scene)
{
    if (scene < 0 || scene >= reipan_config.title.scene_count)
    {
        return;
    }
    if (title_room.active != 0 && title_room.load == TR_READY && scene == title_room.scene &&
        TitleRoomScene()->room == title_room.area)
    {
        TitleRoomFrame(TitleRoomScene());
        title_room.path_time = 0.0;
        return;
    }

    reipan_config.title.scenes[scene].camera_set = 0;
}

int TitleRoomCaptureView(int scene, int point)
{
    if (scene < 0 || scene >= reipan_config.title.scene_count)
    {
        return 0;
    }

    ReiPan_ConfigTitleScene *s = &reipan_config.title.scenes[scene];
    const float *pos = gra3dcamGetPosition();
    const float *aim = gra3dcamGetTarget();
    const float fov = gra3dcamGetFov() * 180.0f / TITLE_ROOM_PI;
    int room_change = 0;

    /* Only the direction is kept, at a fixed reach: a map camera's target is
     * wherever Rei stands, and a path between a near target and a far one
     * swings round mid-way, since the two ends are interpolated as points. */
    float dir[3] = { aim[0] - pos[0], aim[1] - pos[1], aim[2] - pos[2] };
    float len = sqrtf(dir[0] * dir[0] + dir[1] * dir[1] + dir[2] * dir[2]);
    if (len < 1.0e-3f)
    {
        const float *view = gra3dcamGetDirection();

        dir[0] = view[0];
        dir[1] = view[1];
        dir[2] = view[2];
        len = sqrtf(dir[0] * dir[0] + dir[1] * dir[1] + dir[2] * dir[2]);
    }
    if (len < 1.0e-3f)
    {
        dir[2] = len = 1.0f;
    }

    float target[3];
    for (int k = 0; k < 3; k++)
    {
        target[k] = pos[k] + dir[k] / len * TITLE_ROOM_CAPTURE_REACH;
    }

    /* In a game: the area Rei is in, on the side the world is on. */
    if (title_room.active == 0 && ingame_wrk.plyr_wrk != nullptr && IngameIsOutgame() == 0)
    {
        const int area = MapAreaNoGet();

        if (TitleRoomIsOnDisc(area) != 0 && area != s->room)
        {
            /* A path cannot run between two rooms, so the other point takes
             * this view as well and gets moved from there. */
            s->room = area;
            room_change = 1;
        }
        s->side = TitleRoomHasNight(area) ? (int)(MapAreaSideGet() != 0) : 0;
    }

    for (int i = 0; i < 2; i++)
    {
        if (i != point && room_change == 0)
        {
            continue;
        }

        float *eye = i == 0 ? s->a_eye : s->b_eye;
        float *at = i == 0 ? s->a_target : s->b_target;

        for (int k = 0; k < 3; k++)
        {
            eye[k] = pos[k];
            at[k] = target[k];
        }
        if (i == 0)
        {
            s->a_fov = fov;
        }
        else
        {
            s->b_fov = fov;
        }
    }
    s->camera_set = 1;

    return room_change;
}

float TitleRoomGetProgress(void)
{
    const double cycle = TitleRoomCycle();
    double x = title_room.path_time;

    if (TitleRoomScene()->motion == REIPAN_TITLE_MOTION_ONCE)
    {
        x = x < cycle ? x : cycle;
    }
    else
    {
        x = fmod(x, cycle);
    }

    return (float)(x / cycle);
}

void TitleRoomSetProgress(float progress)
{
    if (progress < 0.0f)
    {
        progress = 0.0f;
    }
    if (progress > 1.0f)
    {
        progress = 1.0f;
    }

    title_room.path_time = (double)progress * TitleRoomCycle();
}

int TitleRoomGetPaused(void)
{
    return title_room.paused;
}

void TitleRoomSetPaused(int paused)
{
    title_room.paused = paused != 0;
}

int TitleRoomGetPreview(void)
{
    return title_room.preview;
}

void TitleRoomSetPreview(int point)
{
    title_room.preview = (point == 0 || point == 1) ? point : -1;
}

int TitleRoomGetFly(void)
{
    return title_room.fly;
}

void TitleRoomSetFly(int fly)
{
    fly = fly != 0;

    /* Seeded from wherever the path has the camera now. */
    if (fly != 0 && title_room.fly == 0)
    {
        TitleRoomFlySeed();
    }
    title_room.fly = fly;
}

// ──────────────────────────────────────────────────────────────────────
// GPhase callbacks -- Title_Mode's (5), wrapping title_menu.c's.

int init_TitleRoom_Mode(void)
{
    if (init_Title_Mode() == 0)
    {
        return 0;
    }

    title_room.active = 1;
    title_room.ending = 0;
    title_room.load = TR_IDLE;

    /* A fresh pick every visit -- never the last visit's while there are
     * others -- whichever way the rotation moves on after that. */
    title_room.scene = TitleRoomPickScene(title_room.visited != 0 ? title_room.scene : -1);
    title_room.visited = 1;
    title_room.leaving = 0;
    title_room.preview = -1;
    title_room.reload = 0;
    title_room.failed = -1;
    title_room.released = 0;
    title_room.path_time = 0.0;
    title_room.last_tick = 0;
    title_room.music_file = 0;

    /* init_Title_Mode() has just started the title theme in slot 0; a chosen
     * track takes its place in the same frame. */
    TitleRoomMusicMain();
    return 1;
}

void pre_TitleRoom_Mode(void)
{
    float dt = 0.0f;

    if (title_room.active != 0)
    {
        const unsigned long long now = ReiPan_GetPerformanceCounter();
        const unsigned long long freq = ReiPan_GetPerformanceFrequency();

        if (title_room.last_tick != 0 && freq != 0)
        {
            dt = (float)((double)(now - title_room.last_tick) / (double)freq);
        }
        if (dt > TITLE_ROOM_MAX_STEP)
        {
            dt = TITLE_ROOM_MAX_STEP;
        }
        title_room.last_tick = now;

        TitleRoomFollow();
        TitleRoomMusicMain();

        if (title_room.load == TR_INIT || title_room.load == TR_LOADING)
        {
            TitleRoomLoadMain();
        }

        if (title_room.load == TR_READY)
        {
            const int running = title_room.paused == 0 && title_room.fly == 0 &&
                                title_room.preview < 0;

            file_cache.PollAll();
            MapFrameUpdate();

            if (running != 0)
            {
                title_room.path_time += dt;
            }

            if (title_room.ending != 0)
            {
                title_room.fade = fmaxf(0.0f, title_room.fade - dt / TITLE_ROOM_FADE_SECONDS);
            }
            else if (title_room.leaving != 0)
            {
                title_room.fade -= dt / TITLE_ROOM_FADE_SECONDS;
                if (title_room.fade <= 0.0f)
                {
                    title_room.fade = 0.0f;
                    TitleRoomEnterScene(TitleRoomPickScene(title_room.scene));
                }
            }
            else
            {
                title_room.fade += dt / TITLE_ROOM_FADE_SECONDS;
                if (title_room.fade > 1.0f)
                {
                    title_room.fade = 1.0f;
                }
                if (running != 0 && TitleRoomTimeToMove() != 0)
                {
                    title_room.leaving = 1;
                }
            }

            TitleRoomCameraMain(dt);
        }
    }

    /* The ROM's: the black, then (TitleRoomDrawBackdrop) the room, then the
     * fade layer and the layouts. */
    pre_Title_Mode();
}

int end_TitleRoom_Mode(void)
{
    title_room.ending = 1;
    if (TitleRoomOwnsScreen() != 0 && title_room.fade > 0.0f)
    {
        return 0;
    }

    /* The room goes first, whatever comes next: a new game's IngameInit(),
     * the album's arena, the options' screens.  The loads have to land
     * before it can; until then the title waits here, the room undrawn.
     * end_Title_Mode() is only called once this is done -- it frees the
     * title's layouts and objects the frame it answers 1, and must not be
     * called again after. */
    if (title_room.active != 0 && TitleRoomTryRelease() == 0)
    {
        return 0;
    }

    if (end_Title_Mode() == 0)
    {
        return 0;
    }

    title_room.music_file = 0;
    title_room.fly = 0;
    title_room.active = 0;
    title_room.ending = 0;
    return 1;
}
