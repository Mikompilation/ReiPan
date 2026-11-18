/* ==========================================================================
 *  graphics/scene/IngameScene.c
 *
 *  In-game scenes: the SceneModel class, phases 66 Story_Scene / 89
 *  Story_Scene_Main, the scene tables and the sleep flags. FF3
 *  0x256da8..0x257ae0 (see the header for what the file holds and why it
 *  has this name). FF2's IngameScene.c is a different design (one phase
 *  group, a 2 MB staging buffer); only the phase names carry over.
 * ======================================================================== */
#include "graphics/scene/IngameScene.h"

#include <string.h>                     // strlen, strncmp

#include "graphics/effect/effect.h"     // eff_wrk
#include "graphics/mmanage.h"           // ModelManager (MDL_MOT)
#include "graphics/scene/scene.h"       // SCENE_CTRL, SceneCtrlFind / LoadReq, SceneDataLoadMain, ...
#include "graphics/scene/scene_dat.h"   // ScenePlayStart / Main / IsEnd / End, SceneStreamLoadMain, SceneCtrlRelease
#include "graphics/scene/scene_effect.h"    // SceneEffectInit, SceneEffectEnd
#include "ingame/char_model.h"          // char_joint_tbl
#include "ingame/ingame.h"              // GPhaseCtrlRequest, SndVolume0Mute
#include "ingame/ingame_wrk.h"          // ingame_wrk
#include "ingame/map/MapLoad.h"         // MapLoadWaitMain, MapLoadCheckLoadNow, MapLoadSetDrawFlg*, MapLoadGetRoomNoNow
#include "ingame/map/MapObjWrk.h"       // MapObjUpdate
#include "ingame/movie_room_menu/prg/movie_projecter.h" // MovieReq*
#include "ingame/plyr/plyr_sub.h"       // PlyrCostumeMdlConv
#include "ingame/story_door.h"          // MapAreaSideGet, MapAreaSideSet
#include "ingame/story_menu.h"          // LockTblInc, LockTblDec
#include "outgame/option.h"             // GetOptionWrk, OPTION_WRK
#include "system/eeiop/cd_file_dat.h"   // F000_PK4, D000_PK4, O000_PK4
#include "system/eeiop/stream_auto.h"   // StreamAutoSetExclusiveMode
#include "ingame/story_night.h"         // NightChangeReq

/* ---- SceneModel --------------------------------------------------------- */

// FF3 0x00256e60 (complete; base-object twin 0x00256e30)
// callcheck: GameMotionModelCtorBase (the base class constructor)

SceneModel::SceneModel()
{
}

// FF3 0x00256eb8 (vtable +0x08; deleting 0x00256ee0, base 0x00256e90)
// callcheck: GameMotionModelDtorBase (the base class destructor)

SceneModel::~SceneModel()
{
}

// The model file of the man model no: its CH model in the table.
// FF3 0x00256da8

short SceneManMdlFileNoGet(short no)
{
    return (short)SceneManMdlTblFileGet(no);
}

// FF3 0x00256dd0

short SceneFurnMdlFileNoGet(short no)
{
    return (short)(no + F000_PK4);
}

// FF3 0x00256df0

short SceneDoorMdlFileNoGet(short no)
{
    return (short)(no + D000_PK4);
}

// FF3 0x00256e10

short SceneItemMdlFileNoGet(short no)
{
    return (short)(no + O000_PK4);
}

// The motion and shape data are the scene file's members (scene.c hands
// them in); only the model file goes through the file cache.
// FF3 0x00256f18

void SceneModel::Load(short no, short kind, void *mot, void *shp, short add)
{
    mot_data = mot;
    shp_data = shp;
    this->no = no;
    GameModel::Load(GetFileNo(no, kind, add));
}

// FF3 0x00256f70 (FF2's SceneTestMain match was wrong)

short SceneModel::GetFileNo(short no, short kind, short add)
{
    if (no < 0)
        return -1;
    switch (kind)
    {
    case 0:
        return (short)(add + PlyrCostumeMdlConv(SceneManMdlFileNoGet(no)));
    case 1:
        return SceneFurnMdlFileNoGet(no);
    case 2:
        return SceneDoorMdlFileNoGet(no);
    case 3:
        return SceneItemMdlFileNoGet(no);
    }
    return -1;
}

// GameModel's: only the model file has to be in.
// FF3 0x00257028 (vtable +0x10)

int SceneModel::IsLoaded()
{
    return GameModel::IsLoaded();
}

// FF3 0x00257040 (vtable +0x00)

void SceneModel::Build()
{
    GameMotionModel::Build();
}

// FF3 0x00257058

void SceneModel::PlayMotion(float frame, float speed)
{
    if (mot != nullptr)
        mot->PlayMotion(0, 0, frame, speed);    // FF3: vtable +0x0c
}

// FF3 0x00257090

void SceneModel::PlayShapeAll(float frame, float speed)
{
    int i;

    if (mot == nullptr)
        return;
    for (i = 0; i < mot->GetShapeNum(); i++)    // FF3: vtable +0x48
        mot->PlayShape(i, 0, 0, 0, frame, speed);   // FF3: vtable +0x10
}

// FF3 0x00257130

void SceneModel::ResetCloth()
{
    // data 0x3293c0: the unit matrix (vu0math.c keeps its own copy).
    static sceVu0FMATRIX unit = {
        { 1.0f, 0.0f, 0.0f, 0.0f },
        { 0.0f, 1.0f, 0.0f, 0.0f },
        { 0.0f, 0.0f, 1.0f, 0.0f },
        { 0.0f, 0.0f, 0.0f, 1.0f },
    };

    if (mot == nullptr)
        return;
    mot->ResetCloth(-1, 1);             // FF3: vtable +0x18
    mot->SetMatrix(&unit[0][0]);        // FF3: vtable +0x08
}

// FF3 0x00257188 (vtable +0x1c)

void SceneModel::GetWorldMatrix(sceVu0FMATRIX out)
{
    sceVu0CopyMatrix(out, matrix);
}

// FF3 0x002571b0 (FF2's SceneTestMenuCsrToFuncNo match was wrong)

void SceneModel::SetMatrix(sceVu0FMATRIX m)
{
    sceVu0CopyMatrix(matrix, m);
}

// The man model's character row of char_joint_tbl (as CharModel's).
// FF3 0x002571e8 (vtable +0x34; FF2's SceneTestLightPosDispInit match was wrong)

int SceneModel::GetJointCoord(int joint)
{
    int chr = (short)SceneManMdlTblChrGet(no);

    // The ROM indexes row -1 (the rodata before the table) for a model the
    // table does not list; no such joint on the host.
    if (chr < 0)
        return -1;
    return char_joint_tbl[chr][joint];
}

// FF3 0x00257238

void SceneModel::SetFrame(float frame)
{
    if (mot != nullptr)
        mot->SetFrame(frame);           // FF3: vtable +0x28
}

/* ---- phase 89 Story_Scene_Main ------------------------------------------ */

static int         story_scene_main_step;   // sbss gp+0x458: 1 stream, 2 play, 3 map back
static SCENE_CTRL *story_scene_main_sc;     // sbss gp+0x45c: the playing slot

// FF3 0x00257268

int init_Story_Scene_Main(void)
{
    int scene_no = MovieReqGetSceneNo();

    StreamAutoSetExclusiveMode(1, 3);
    story_scene_main_sc = SceneCtrlFind(scene_no);
    SceneStreamLoadReq(story_scene_main_sc);
    story_scene_main_step = 1;
    return 1;
}

// FF3 0x002572c0

int end_Story_Scene_Main(void)
{
    StreamAutoSetExclusiveMode(0, 3);
    return 1;
}

// The rooms the scene used stay loaded; the map's draw flags and objects
// come back before the phase that asked for the scene resumes.
// FF3 0x002572e8

int one_Story_Scene_Main(void)
{
    static OPTION_WRK *opt;             // sbss gp-0xaec, set on first use

    switch (story_scene_main_step)
    {
    case 1:
        MovieReqDispCapture();
        if (SceneStreamLoadMain(story_scene_main_sc) == 0)
            return 0;
        if (ScenePlayStart(MovieReqGetSceneNo()) == 0)
            return 0;
        SndVolume0Mute(1);
        eff_wrk._18 = 0;
        SceneEffectInit(1);
        LockTblInc(6);
        LockTblInc(0xb);
        LockTblInc(0xd);
        LockTblInc(0x19);
        story_scene_main_step = 2;
        break;

    case 2:
        if (opt == nullptr)
            opt = GetOptionWrk();
        if (ScenePlayMain(opt->now.subtitle, MovieReqGetSkip()) == 0)
            return 0;
        if (ScenePlayIsEnd() == 0)
            return 0;
        ScenePlayEnd();
        SceneEffectEnd();
        eff_wrk._18 = 1;
        SndVolume0Mute(0);
        SceneCtrlRelease(story_scene_main_sc);
        story_scene_main_sc = nullptr;
        story_scene_main_step = 3;
        break;

    case 3:
        MovieReqDispCapture();
        MapLoadWaitMain();
        if (MapLoadCheckLoadNow() != 0)
            return 0;
        LockTblDec(6);
        LockTblDec(0xb);
        LockTblDec(0xd);
        LockTblDec(0x19);
        MapLoadSetDrawFlg(3, 2);
        MapLoadSetDrawFlg3(MapLoadGetRoomNoNow(), 1);
        MapObjUpdate(-1);
        GPhaseCtrlRequest(MovieReqGetRetPhase(), 0);
        return 1;
    }
    return 0;
}

/* ---- phase 66 Story_Scene ------------------------------------------------ */

static u_int       story_scene_step;    // sbss gp+0x468: 0 none, 1 data, 2 rooms asked, 3 rooms, 4 ready
static SCENE_CTRL *story_scene_sc;      // sbss gp+0x46c

// A slot that already holds the scene is reused; otherwise every slot is
// released and the scene requested into one.
// FF3 0x002574c8

int init_Story_Scene(void)
{
    int scene_no = MovieReqGetSceneNo();

    story_scene_sc = SceneCtrlFind(scene_no);
    if (story_scene_sc == nullptr)
    {
        SceneCtrlReleaseAll();
        story_scene_sc = SceneCtrlLoadReq(scene_no);
    }
    story_scene_step = story_scene_sc != nullptr;
    return 1;
}

// FF3 0x00257540

int end_Story_Scene(void)
{
    return 1;
}

// No slot (step 0): straight back to the phase that asked. Loaded: phase
// 0x59 (89, Story_Scene_Main) when the request plays it, else back (a
// preload).
// FF3 0x00257548

int one_Story_Scene(void)
{
    u_int next;

    MovieReqDispCapture();
    switch (story_scene_step)
    {
    case 0:
        GPhaseCtrlRequest(MovieReqGetRetPhase(), 0);
        return 1;

    case 1:
        if (SceneDataLoadMain(story_scene_sc) == 0)
            return 0;
        next = 2;
        break;

    case 2:
        SceneRoomLoadReq(story_scene_sc, MovieReqIsPlay());
        next = 3;
        break;

    case 3:
        MapLoadWaitMain();
        if (SceneRoomLoadMain(story_scene_sc) == 0)
            return 0;
        next = 4;
        break;

    case 4:
        GPhaseCtrlRequest(MovieReqIsPlay() != 0 ? 0x59 : MovieReqGetRetPhase(), 0);
        return 1;

    default:
        return 0;
    }
    story_scene_step = next;
    return 0;
}

// FF3 0x00257630

void pre_Story_Scene(void)
{
}

// FF3 0x00257638

void after_Story_Scene(void)
{
}

/* ---- tables ------------------------------------------------------------- */

// layout: host -- rodata rows, read only here.
// One row of the scene man-model table: a scene's man model number, its
// character (char_joint_tbl row) and its CH model file.
struct SCENE_MAN_MDL_TBL
{
    int no;                             // -1 ends the table
    int chr;
    int file;                           // CD_FILE_DAT (CH000_PK4 0x197a ..)
};

// rodata 0x362b48 (53 rows with the end row).
static const SCENE_MAN_MDL_TBL scene_man_mdl_tbl[] =
{
    {   0,   0,  6522 }, {   1,   1,  6528 }, {   2,   2,  6534 }, {   6,   3,  6544 },
    {   7,   4,  6546 }, {   8,   5,  6548 }, {   9,   6,  6554 }, {  10,   7,  6558 },
    {  11,   8,  6562 }, {  12,   9,  6564 }, {  13,  10,  6575 }, {  14,  11,  6579 },
    {  15,  12,  6583 }, {  17,  13,  6586 }, {  18,  14,  6591 }, {  19,  15,  6593 },
    {  20,  16,  6595 }, {  21,  17,  6600 }, {  22,  18,  6609 }, {  23,  19,  6615 },
    {  24,  20,  6622 }, {  27,  21,  6635 }, {  28,  22,  6643 }, {  29,  23,  6660 },
    {  30,  24,  6669 }, {  31,  25,  6674 }, {  32,  26,  6678 }, {  33,  27,  6687 },
    {  34,  28,  6701 }, {  35,  29,  6707 }, {  36,  30,  6710 }, {  37,  31,  6712 },
    {  38,  32,  6717 }, {  39,  33,  6720 }, {  40,  34,  6725 }, {  41,  35,  6729 },
    {  43,  36,  6739 }, {  44,  37,  6741 }, {  45,  38,  6744 }, {  46,  39,  6746 },
    {  47,  40,  6748 }, {  48,  41,  6750 }, {  49,  42,  6754 }, {  50,  43,  6756 },
    {  51,  44,  6757 }, {  52,  45,  6759 }, {  55,  46,  6764 }, {   3,   0,  6539 },
    {   4,   1,  6540 }, {   5,   2,  6542 }, {  66,   1,  6776 }, {  67,  14,  6778 },
    {  -1,  -1,     0 },
};

// FF3 0x00257640

int SceneManMdlTblChrGet(int no)
{
    const SCENE_MAN_MDL_TBL *t = scene_man_mdl_tbl;

    do
    {
        if (t->no == no)
            return t->chr;
        t++;
    } while (t->no != -1);
    return -1;
}

// FF3 0x00257688

int SceneManMdlTblFileGet(int no)
{
    const SCENE_MAN_MDL_TBL *t = scene_man_mdl_tbl;

    do
    {
        if (t->no == no)
            return t->file;
        t++;
    } while (t->no != -1);
    return -1;
}

// layout: host -- name is a pointer (EE 8-byte rows).
struct SCENE_ROOM_NAME
{
    const char *name;                   // nullptr ends the table
    int         no;
};

// rodata 0x362dc8, names at 0x372408.
static const SCENE_ROOM_NAME scene_room_name[] =
{
    { "rs101", 1 }, { "rs102", 2 }, { "rs103", 3 }, { "rs104", 4 }, { "rs105", 5 }, { "rs106", 6 },
    { "rs201", 7 }, { "rs202", 8 }, { "rs203", 9 }, { "rs204", 10 }, { "rs205", 11 }, { "rs206", 12 },
    { "rs207", 13 }, { "rs208", 14 }, { "rs209", 15 }, { "rs210", 16 }, { "rs211", 17 }, { "rs212", 18 },
    { "rs213", 19 }, { "rs301", 20 }, { "rs302", 21 }, { "rs303", 22 }, { "rs304", 23 }, { "rs305", 24 },
    { "rs306", 25 }, { "rs307", 26 }, { "rs308", 27 }, { "rs401", 28 }, { "rs402", 29 }, { "rs403", 30 },
    { "rs404", 31 }, { "rs405", 32 }, { "rs406", 33 }, { "rs407", 34 }, { "rs408", 35 }, { "rs409", 36 },
    { "rs410", 37 }, { "rs501", 38 }, { "rs502", 39 }, { "rs503", 40 }, { "rs504", 41 }, { "rs505", 42 },
    { "rs506", 43 }, { "rs507", 44 }, { "rs508", 45 }, { "rs509", 46 }, { "rre01", 47 }, { "rre02", 48 },
    { "rre03", 49 }, { "rre04", 50 }, { "rre05", 51 }, { "rre06", 52 }, { "rre01n", 53 }, { "rre02n", 54 },
    { "rre03n", 55 }, { "rre04n", 56 }, { "rre05n", 57 }, { "rre06n", 58 }, { "rhp00", 59 }, { nullptr, -1 },
};

// The first row whose name starts name: "rre01n..." matches "rre01" (47)
// before "rre01n" (53) -- the ROM's order, kept.
// FF3 0x002576d0

int SceneRoomNameToNo(const char *name)
{
    int i;

    if (name == nullptr)
        return -1;
    for (i = 0; scene_room_name[i].name != nullptr; i++)
    {
        if (strncmp(scene_room_name[i].name, name, strlen(scene_room_name[i].name)) == 0)
            return scene_room_name[i].no;
    }
    return -1;
}

// layout: host -- rodata rows, read only here.
struct SCENE_STR_FILE
{
    int scene_no;                       // < 0 ends the table
    int file;                           // SCENE<no>_STR; the .hxd header follows
};

// rodata 0x362fa8.
static const SCENE_STR_FILE scene_str_file[] =
{
    {  2, SCENE0020_STR }, {  3, SCENE0030_STR }, {  6, SCENE0060_STR }, {  8, SCENE0062_STR },
    {  9, SCENE0070_STR }, { 10, SCENE0080_STR }, { 12, SCENE0090_STR }, { 15, SCENE0110_STR },
    { 16, SCENE0111_STR }, { 17, SCENE0112_STR }, { 18, SCENE0113_STR }, { 33, SCENE9020_STR },
    { 35, SCENE9070_STR }, { -1, -1 },
};

// FF3 0x002577c0

int SceneStrFileGet(int scene_no)
{
    int i = 0;

    while (scene_str_file[i].scene_no != scene_no)
    {
        if (scene_str_file[++i].scene_no < 0)
            return -1;
    }
    return scene_str_file[i].file;
}

/* ---- the sleep flags ----------------------------------------------------- */

struct SLEEP_FLG
{
    short kind;                         // 0: free (the slot test reads only kind)
    short id;
};

static SLEEP_FLG sleep_flg[SLEEP_FLG_NUM];  // bss 0x3fcf10

// No flag set. FF3 inlines this loop in ScnTestIngameMain and SleepFlgSideWake.
static int SleepFlgIsEmpty(void)
{
    int i;

    for (i = 0; i < SLEEP_FLG_NUM; i++)
    {
        if (sleep_flg[i].kind != 0)
            return 0;
    }
    return 1;
}

// FF3 0x00257810

void ScnTestIngameInit(void)
{
    SleepFlgClearAll();
}

// FF3 0x00257828

void ScnTestIngameEnd(void)
{
    SleepFlgClearAll();
}

// FF3 0x00257840

int ScnTestIngameMain(void)
{
    if (ingame_wrk.b03 == 1 && MapAreaSideGet() == 0 && SleepFlgIsEmpty())
        return NightChangeReq(1, 1);
    return 0;
}

// FF3 0x002578c8

int SleepFlgSideWake(void)
{
    if (MapAreaSideGet() != 0 || !SleepFlgIsEmpty())
        return 0;
    MapAreaSideSet(1);
    return 1;
}

// FF3 0x00257938 (FF2's ScnUDCtrl match was wrong)

void SleepFlgClearAll(void)
{
    int i;

    for (i = 0; i < SLEEP_FLG_NUM; i++)
    {
        sleep_flg[i].kind = 0;
        sleep_flg[i].id = 0;
    }
}

// FF3 0x002579b8 (FF2's ScnValueCtrlI match was wrong)

void SleepFlgSet(short kind, short id)
{
    int i;

    for (i = 0; i < SLEEP_FLG_NUM; i++)
    {
        if (sleep_flg[i].kind == kind && sleep_flg[i].id == id)
            return;
    }
    for (i = 0; i < SLEEP_FLG_NUM; i++)
    {
        if (sleep_flg[i].kind == 0)
        {
            sleep_flg[i].id = id;
            sleep_flg[i].kind = kind;
            return;
        }
    }
}

// FF3 0x00257a58 (FF2's ScnValueCtrlC match was wrong)

int SleepFlgClear(short kind, short id)
{
    int i;

    for (i = 0; i < SLEEP_FLG_NUM; i++)
    {
        if (sleep_flg[i].kind == kind && sleep_flg[i].id == id)
        {
            sleep_flg[i].id = 0;
            sleep_flg[i].kind = 0;
            return i;
        }
    }
    return -1;
}

// FF3 0x00257ac8

int SleepFlgSaveGet(void **addr)
{
    *addr = sleep_flg;
    return sizeof(sleep_flg);
}
static_assert(sizeof(SLEEP_FLG) == 4 && sizeof(sleep_flg) == 0x40, "sleep_flg (EE 0x40 bytes)");
