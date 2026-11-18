/* ==========================================================================
 *  graphics/scene/scene.c
 *
 *  Scenes (FF2 scene.c): FF3 0x250d40..0x253620 plus the out-of-line pieces
 *  0x2be570..0x2be808 and 0x2be940..0x2bef98 that only this file calls.
 *
 *  An engine scene (one_Story_Scene, phase 66) takes a slot of scene_ctrl:
 *  SceneCtrlLoadReq loads the scene file (a PK4: header, camera and light
 *  FODs, the models' motion PK4s) into a BORROW_BUF and its light revision
 *  and effect script into system heap blocks; SceneDataLoadMain then reads
 *  the header (SceneCtrlInit), creates the man models with their accessories
 *  and the furniture / door / item models (SceneModel), and waits for them;
 *  SceneRoomLoadMain loads the scene's room. ScenePlayStart (scene_dat.c)
 *  sets the slot up with SceneInitAfterModelLoad (FOD, lights, first camera,
 *  models, rooms) and ScenePlayMain draws it each frame with SceneDraw (the
 *  camera and lights blended between FOD records, the rooms, the models, the
 *  scene effects and captions). Also: the scene table and its readers, the
 *  man-model queries of the scene effects and the light changes.
 * ======================================================================== */
#include "graphics/scene/scene.h"

#include <stdio.h>                      // sprintf
#include <string.h>                     // memset, strncmp, strcmp, strstr

#include "common/heapctrl.h"            // heapCtrlMalloc
#include "common/pk4.h"                 // PK4_HEADER, Pk4GetData
#include "common/vu0math.h"             // Vu0Sinf, Vu0Cosf, GetTrgtRot, RotVectorZXY
#include "graphics/effect/effect.h"     // EffectDrawMain
#include "graphics/effect/effect_ene.h" // EneAuraFrameBegin / End, EneAuraDraw
#include "graphics/effect/effect_oth.h" // EffectOthBufCopy
#include "graphics/game_model.h"        // GameModel
#include "graphics/graph2d/font.h"      // FontTexSendVram
#include "graphics/graph3d/g3dLightEx.h"    // gra3dSetLightIntens
#include "graphics/graph3d/gra3d.h"     // gra3dGetCamera, gra3dcam*, gra3dSetFog*, gra3dApplyCamera
#include "graphics/mmanage.h"           // model_collision_set
#include "graphics/movie/movie.h"       // BORROW_BUF
#include "graphics/movie/movie_title.h" // MovieTitleMain
#include "graphics/scene/IngameScene.h" // SceneModel, SceneManMdlFileNoGet, SceneRoomNameToNo
#include "graphics/scene/fod.h"         // FodInit, FodSetMyLight, FodGetToSgLight, FodGetCamData ..
#include "graphics/scene/scene_dat.h"   // ScenePlayCtrlGet, SceneCtrlRelease, SceneMdlNameSet, SceneMapObjShow
#include "graphics/scene/scene_effect.h"    // SceneEffectInit, SceneEffectMain, SceneEffectLenzFlare*
#include "ingame/char_model.h"          // CharPlayerModel
#include "ingame/enemy/enemy.h"         // EneAuraDatGet
#include "ingame/ingame.h"              // IngameMdlResetAll, IngameSndListenerSet
#include "ingame/ingame_wrk.h"          // ingame_wrk
#include "ingame/map/MapCtrl.h"         // MapDrawAll
#include "ingame/map/MapDraw.h"         // MapDrawPreLight
#include "ingame/map/MapLoad.h"         // MapLoadGetBuffID, MapLoadGetOffsetVector, MapLoadSetDrawFlg*, MapLoadChangeArea ..
#include "ingame/map/MapObjWrk.h"       // MapObjUpdateRoom
#include "ingame/map/MapPut.h"          // MapPutSetSceneLoad, MapPutDrawFlagClear, MapPutDraw
#include "ingame/plyr/acs_model.h"      // AcsModel
#include "ingame/plyr/player.h"         // PLYR_WRK, PlyrGetCurNo
#include "ingame/plyr/plyr_sub.h"       // PlyrGetMdlChr, PlyrGetAcs, PlyrAcsIsNot2
#include "ingame/story_door.h"          // MapAreaSideGet
#include "sdk/libvu0.h"                 // sceVu0*
#include "sdk/scetypes.h"               // u_int
#include "system/eeiop/cd_file_dat.h"   // SCENE0010M_PSS
#include "system/eeiop/cddat.h"         // GetFileSize
#include "system/os/eecdvd.h"           // LoadReq, IsLoadEnd
#include "system/os/sys_heap.h"         // GetSystemHeapWrkP
#include "system/os/system.h"           // GetPALMode

// ──────────────────────────────────────────────────────────────────────
// Data
// ──────────────────────────────────────────────────────────────────────

static SCENE_DAT scene_dat[SCENE_NUM] = // data 0x332be8
{
    { 100, "0010", 1 },
    { 100, "0011", 1 },
    { 100, "0020", 0 },
    { 100, "0030", 0 },
    { 100, "0040", 1 },
    { 100, "0050", 1 },
    { 100, "0060", 0 },
    { 100, "0061", 1 },
    { 100, "0062", 0 },
    { 100, "0070", 0 },
    { 100, "0080", 0 },
    { 100, "0081", 1 },
    { 100, "0090", 0 },
    { 100, "0100", 1 },
    { 100, "0101", 1 },
    { 100, "0110", 0 },
    { 100, "0111", 0 },
    { 100, "0112", 0 },
    { 100, "0113", 0 },
    { 100, "0120", 1 },
    { 100, "0130", 1 },
    { 100, "0140", 1 },
    { 100, "0150", 1 },
    { 100, "5000", 1 },
    { 100, "5001", 1 },
    { 100, "5002", 1 },
    { 100, "5003", 1 },
    { 100, "9010", 1 },
    { 100, "9011", 1 },
    { 100, "9012", 1 },
    { 100, "9013", 1 },
    { 100, "9014", 1 },
    { 100, "9015", 1 },
    { 100, "9020", 0 },
    { 100, "9060", 1 },
    { 100, "9070", 0 },
    { 100, "9100", 1 },
    { 100, "9101", 1 },
    { 100, "9102", 1 },
    { 100, "9103", 1 },
    { 100, "9104", 1 },
    { 100, "9105", 1 },
    { 100, "9106", 1 },
    { 100, "9107", 1 },
    { 100, "9108", 1 },
    { 100, "9109", 1 },
    { 100, "9110", 1 },
    { 100, "9200", 1 },
    { 100, "9201", 1 },
    { 100, "9202", 1 },
    { 100, "9203", 1 },
    { 100, "9204", 1 },
    { 100, "9205", 1 },
    { 100, "9206", 1 },
    { 100, "9220", 1 },
    { 100, "9221", 1 },
    { 100, "9222", 1 },
    { 100, "9223", 1 },
    { 100, "9230", 1 },
    { 100, "9231", 1 },
    { 100, "9232", 1 },
    { 100, "9233", 1 },
    { 100, "9500", 1 },
    { 100, "9600", 1 },
    { 100, "9700", 1 },
    { 100, "9900", 1 },
    { 100, "9901", 1 },
    { 100, "9902", 1 },
};

// SceneMakeLightName's light types (FOD_LIT_SERIAL.light_type; data
// 0x332bc8, 0 and 1 share the empty string) and SceneMakeModelPrefix's model
// groups (SCN_MDL_*; data 0x332bd8).
static const char *const scene_light_type_name[4] = { "", "", "spot", "point" };
static const char *const scene_mdl_prefix[4] = { "ch", "f", "d", "o" };

static char scene_light_name[0x20];     // bss 0x3fce50: SceneChangeLightParameter's light name
static char scene_mdl_name[0x20];       // bss 0x3fce70: and its model prefix

static const sceVu0FVECTOR scene_origin = { 0.0f, 0.0f, 0.0f, 1.0f };   // rdata 0x329500
static const sceVu0FVECTOR scene_zero   = { 0.0f, 0.0f, 0.0f, 0.0f };   // rdata 0x329530

#define SCENE_CLIP_NEAR     0.5f        // sdata 0x3723a0: the scene camera's clip
#define SCENE_CLIP_FAR      3050.0f     // sdata 0x3723a4
#define SCENE_PAL_FRAME     1.2f        // sdata 0x3720f0: 60 Hz frames per PAL caption frame

static void SceneDataAnalyze(SCENE_FILE *sf, void *pk4);
static void SceneInitEnvironment(SCENE_FILE *sf, SCENE_CTRL *sc);
static void SceneLightRevision(SCENE_CTRL *sc);
static void SceneAllMdlInit(SCENE_CTRL *sc, SCENE_FILE *sf);
static void *Pk4FindByName(void *pk4, const char *name);
static void SceneGetHeaderData(SCENE_CTRL *sc, SCENE_HDR *hdr);
static SCENE_LIGHT_REV *SceneLightRevGet(u_int *rev, int no);
static void SceneRenewEnvironment(SCENE_CTRL *sc, float *offset);
static int  SceneManModelIdToSceneModelNo(SCENE_CTRL *sc, short mdl_id, int index);
static void SceneMdlAddSet(SCN_MDL *row, int num, u_char *data);
static void SceneLightInterp(float frame, FOD_LIGHT *fl, FOD_LIGHT *cur, FOD_LIGHT *next);

// ──────────────────────────────────────────────────────────────────────
// The scene file and its next two (the light revision, the effect script)
// requested; the second and third into system heap blocks when they exist.
// FF3 0x00250d40

static void SceneFileLoadReq(SCENE_CTRL *sc)
{
    int   file = GetFileNoFromSceneNo(sc->scene_no);
    int   no;
    u_int size;

    sc->load_num  = 0;
    sc->light_rev = nullptr;
    sc->eff_data  = nullptr;
    if (sc->scn_data_addr == nullptr)
    {
        return;
    }

    sc->load_id[sc->load_num++] = LoadReq(file, sc->scn_data_addr);

    no   = file + 1;
    size = GetFileSize(no);
    if (size != (u_int)-1 && size != 0)
    {
        sc->light_rev = (u_int *)heapCtrlMalloc(GetSystemHeapWrkP(), size, 0, 0, 0);
        sc->load_id[sc->load_num++] = LoadReq(no, sc->light_rev);
    }

    no   = file + 2;
    size = GetFileSize(no);
    if (size != (u_int)-1 && size != 0)
    {
        sc->eff_data = heapCtrlMalloc(GetSystemHeapWrkP(), size, 0, 0, 0);
        sc->load_id[sc->load_num++] = LoadReq(no, sc->eff_data);
    }
}

// ──────────────────────────────────────────────────────────────────────
// 1 when every load SceneFileLoadReq made has finished.
// FF3 0x00250ec0

static int SceneDataLoadWait(SCENE_CTRL *sc)
{
    if (sc->load_num == 0)
    {
        return 1;
    }
    for (int i = 0; i < sc->load_num; i++)
    {
        if (!IsLoadEnd(sc->load_id[i]))
        {
            return 0;
        }
    }
    return 1;
}

// ──────────────────────────────────────────────────────────────────────
// The man models: each a SceneModel with its motion and shape from the
// scene file; a character's own accessory, and on the current player's
// character the header's accessories 1 and 0 (the lantern).
// callcheck: operator_new, SceneModelCtor, AcsModelCtor (new-expressions)
// callcheck: strncpy (a dead copy of the name, see below)
// FF3 0x00250f38

static void SceneManModelLoad(SCENE_CTRL *sc)
{
    for (int i = 0; i < sc->man_mdl_num; i++)
    {
        SCN_MDL *m = &sc->man_mdl[i];
        s_char   acs;
        int      base;

        // (The ROM also copies name table entry i to a dead stack buffer.)
        m->mot = Pk4GetData(sc->file.man_mot, i + 1);
        m->shp = Pk4FindByName(sc->file.man_shp, m->name);
        m->mdl = new SceneModel();
        m->mdl->Load(m->obj_no, SCN_MDL_MAN, m->mot, m->shp, m->add);

        m->chr = (s_char)PlyrGetMdlChr(SceneManMdlFileNoGet(m->obj_no));
        if (m->chr < 0)
        {
            continue;
        }

        acs = (s_char)PlyrGetAcs(m->chr);
        if (acs != 0)
        {
            // Each character's accessories from its own A000_PK4 row.
            base = (m->chr == 1) ? 0xd : (m->chr == 2) ? 0x11 : 9;
            m->efct[3] = new AcsModel();
            m->efct[3]->Load((s_char)(acs + base - 1));
        }

        if ((u_char)m->chr == PlyrGetCurNo())
        {
            if (sc->hdr_flg[0] != 0)
            {
                m->efct[1] = new AcsModel();
                m->efct[1]->Load(1);
            }
            if (sc->hdr_flg[1] != 0)
            {
                m->efct[0] = new AcsModel();
                m->efct[0]->Load(0);
            }
        }
    }
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x00251130

static int SceneManModelIsLoaded(SCENE_CTRL *sc)
{
    for (int i = 0; i < sc->man_mdl_num; i++)
    {
        SCN_MDL *m = &sc->man_mdl[i];

        if (m->mdl == nullptr)
        {
            continue;
        }
        if (!m->mdl->IsLoaded())
        {
            return 0;
        }
        if (m->chr < 0)
        {
            continue;
        }
        for (int k = 0; k < 4; k++)
        {
            if (m->efct[k] != nullptr && !m->efct[k]->IsLoaded())
            {
                return 0;
            }
        }
    }
    return 1;
}

// ──────────────────────────────────────────────────────────────────────
// PC: the ROM leaves the deleted pointers in the rows (SceneCtrlRelease
// clears the slot after); the port clears them.
// FF3 0x00251220

void SceneManModelClear(SCENE_CTRL *sc)
{
    for (int i = 0; i < sc->man_mdl_num; i++)
    {
        SCN_MDL *m = &sc->man_mdl[i];

        if (m->mdl == nullptr)
        {
            continue;
        }
        delete m->mdl;
        m->mdl = nullptr;
        if (m->chr < 0)
        {
            continue;
        }
        for (int k = 0; k < 4; k++)
        {
            if (m->efct[k] != nullptr)
            {
                delete m->efct[k];
                m->efct[k] = nullptr;
            }
        }
    }
}

// ──────────────────────────────────────────────────────────────────────
// One group of SceneObjModelLoad (the ROM unrolls it per group).

static void SceneObjModelLoadGroup(SCN_MDL *row, int num, void *mot_pk4, short kind)
{
    for (int i = 0; i < num; i++)
    {
        row[i].mot = Pk4GetData(mot_pk4, i + 1);
        row[i].mdl = new SceneModel();
        row[i].mdl->Load(row[i].obj_no, kind, row[i].mot, nullptr, 0);
    }
}

// The item, furniture and door models, each with its motion.
// callcheck: operator_new, SceneModelCtor (new-expressions)
// callcheck: strncpy (the ROM also copies each name table entry to a dead stack buffer)
// callcheck: Pk4GetData, SceneModelLoad (through SceneObjModelLoadGroup)
// FF3 0x002512f8

static void SceneObjModelLoad(SCENE_CTRL *sc)
{
    SceneObjModelLoadGroup(sc->item_mdl, sc->item_num, sc->file.item_mot, SCN_MDL_ITEM);
    SceneObjModelLoadGroup(sc->furn_mdl, sc->furn_num, sc->file.furn_mot, SCN_MDL_FURN);
    SceneObjModelLoadGroup(sc->door_mdl, sc->door_num, sc->file.door_mot, SCN_MDL_DOOR);
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x00251530

static int SceneObjModelIsLoaded(SCENE_CTRL *sc)
{
    for (int i = 0; i < sc->item_num; i++)
    {
        if (!sc->item_mdl[i].mdl->IsLoaded())
        {
            return 0;
        }
    }
    for (int i = 0; i < sc->furn_num; i++)
    {
        if (!sc->furn_mdl[i].mdl->IsLoaded())
        {
            return 0;
        }
    }
    for (int i = 0; i < sc->door_num; i++)
    {
        if (!sc->door_mdl[i].mdl->IsLoaded())
        {
            return 0;
        }
    }
    return 1;
}

// ──────────────────────────────────────────────────────────────────────
// PC: clears the deleted pointers (see SceneManModelClear).
// FF3 0x00251628

void SceneObjModelClear(SCENE_CTRL *sc)
{
    for (int i = 0; i < sc->item_num; i++)
    {
        if (sc->item_mdl[i].mdl != nullptr)
        {
            delete sc->item_mdl[i].mdl;
            sc->item_mdl[i].mdl = nullptr;
        }
    }
    for (int i = 0; i < sc->furn_num; i++)
    {
        if (sc->furn_mdl[i].mdl != nullptr)
        {
            delete sc->furn_mdl[i].mdl;
            sc->furn_mdl[i].mdl = nullptr;
        }
    }
    for (int i = 0; i < sc->door_num; i++)
    {
        if (sc->door_mdl[i].mdl != nullptr)
        {
            delete sc->door_mdl[i].mdl;
            sc->door_mdl[i].mdl = nullptr;
        }
    }
}

// ──────────────────────────────────────────────────────────────────────
// Empty in FF3; called where FF2's SceneEndProc freed the door model DB.
// FF3 0x00251768

void SceneDoorModelDBFree(SCENE_CTRL *sc)
{
    (void)sc;
}

// ──────────────────────────────────────────────────────────────────────
// One group of SceneCtrlInit's row loops.

static void SceneCtrlInitGroup(SCENE_CTRL *sc, SCN_MDL *row, u_char kind)
{
    for (int i = 0; i < SCENE_MDL_NUM; i++)
    {
        memset(&row[i], 0, sizeof(SCN_MDL));
        row[i].disp = 1;
        row[i].kind = kind;
        row[i].sc   = sc;
    }
}

// The slot's rows cleared and tagged, the scene file read (header, fog),
// what the scene changes saved (the camera's clip and field of view,
// monochrome, the area side).
// callcheck: memset (through SceneCtrlInitGroup)
// FF3 0x00251770

static void SceneCtrlInit(SCENE_CTRL *sc)
{
    SceneCtrlInitGroup(sc, sc->man_mdl, SCN_MDL_MAN);
    SceneCtrlInitGroup(sc, sc->furn_mdl, SCN_MDL_FURN);
    SceneCtrlInitGroup(sc, sc->door_mdl, SCN_MDL_DOOR);
    SceneCtrlInitGroup(sc, sc->item_mdl, SCN_MDL_ITEM);

    SceneDataAnalyze(&sc->file, sc->scn_data_addr);
    SceneInitEnvironment(&sc->file, sc);

    sc->stream_play       = 0;
    sc->init_flg          = 1;
    sc->fod_flg           = 1;
    sc->count_flg         = 1;
    sc->fNearZBak         = gra3dGetCamera()->fNearZ;
    sc->fFarZBak          = gra3dGetCamera()->fFarZ;
    sc->fFovBak           = gra3dcamGetFov();
    sc->MonotoneEnableBak = gra3dIsMonotoneDrawEnable();
    sc->AreaSideBak       = MapAreaSideGet();
    sc->load_num          = 0;
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x002518f8

static void SceneDataAnalyze(SCENE_FILE *sf, void *pk4)
{
    sf->pk4      = pk4;
    sf->file_num = (pk4 != nullptr) ? ((PK4_HEADER *)pk4)->num : 0;
    sf->hdr      = (SCENE_HDR *)Pk4GetData(pk4, 0);
    sf->cam_fod  = (u_int *)Pk4GetData(pk4, 1);
    sf->lit_fod  = (u_int *)Pk4GetData(pk4, 2);
    sf->eff_fod  = nullptr;
    sf->man_mot  = Pk4GetData(pk4, 3);
    sf->man_shp  = Pk4GetData(pk4, 4);
    sf->furn_mot = Pk4GetData(pk4, 5);
    sf->door_mot = Pk4GetData(pk4, 6);
    sf->item_mot = Pk4GetData(pk4, 7);
}

// ──────────────────────────────────────────────────────────────────────
// The header, then FF2's default fog (FF3 drops FF2's vibration timers).
// FF3 0x002519c0

static void SceneInitEnvironment(SCENE_FILE *sf, SCENE_CTRL *sc)
{
    SceneGetHeaderData(sc, sf->hdr);
    sc->fog.min    = 0.0f;
    sc->fog.max    = 255.0f;            // sdata 0x3720d0
    sc->fog.near_z = 1000.0f;           // sdata 0x3720d4
    sc->fog.far_z  = 10000.0f;          // sdata 0x3720d8
    sc->fog.r      = 0x10;
    sc->fog.g      = 0x10;
    sc->fog.b      = 0x10;
}

// ──────────────────────────────────────────────────────────────────────
// The FOD streams opened at the scene room's origin with the light revision
// over them, the first camera, the models at frame 0, the scene's lights as
// the rooms' prelight, the scene's two rooms drawn, the map objects it
// replaces hidden, the scene effects reset. mim_init (FF2: the morph reset)
// is not read.
// FF3 0x00251a30

void SceneInitAfterModelLoad(SCENE_CTRL *sc, int mim_init)
{
    sceVu0FVECTOR offset;
    int           room, sub_room;

    (void)mim_init;
    MapLoadGetOffsetVector(offset, MapLoadGetBuffID(sc->room_no));
    FodInit(&sc->fod_ctrl, sc->file.cam_fod, sc->file.lit_fod, sc->file.eff_fod, offset);
    SceneLightRevision(sc);
    FodGetFirstCam(&sc->CameraData, &sc->fod_ctrl, offset);
    SceneAllMdlInit(sc, &sc->file);
    MapDrawPreLight(FodGetGra3DLight());

    room     = SceneRoomNoGet(sc);
    sub_room = SceneSubRoomNoGet(sc);
    MapPutSetSceneLoad(1);
    MapLoadSetDrawFlg(3, 2);
    if (room != -1)
    {
        MapLoadSetDrawFlg3(room, 1);
    }
    if (sub_room != -1)
    {
        MapLoadSetDrawFlg3(sub_room, 1);
    }
    MapObjUpdateRoom(0, 8);
    MapObjUpdateRoom(1, 8);
    SceneMapObjShow(sc, 0);
    MapPutSetSceneLoad(0);
    SceneEffectInit(1);
}

// ──────────────────────────────────────────────────────────────────────
// The light revision file over the FOD lights: each record changes the
// light of the same name and type (colours; the range of a point or spot,
// a spot's intensity), then the fog and the ambients (an ambient without
// its "AMB" tag is black). FF2 rejected the whole file on any mismatch.
// FF3 0x00251b60

static void SceneLightRevision(SCENE_CTRL *sc)
{
    FOD_LIGHT       *fl  = &sc->fod_ctrl.fod_light;
    u_int           *rev = sc->light_rev;
    SCENE_LIGHT_REV *r;
    u_char          *amb;
    int              num;

    if (rev == nullptr)
    {
        return;
    }

    num = (int)rev[0];
    for (int i = 0; i < num; i++)
    {
        r = SceneLightRevGet(rev, i);
        for (int j = 0; j < fl->all_lit_num; j++)
        {
            if (strncmp(fl->lit_serial[j].light_name, r->serial.light_name, sizeof(r->serial.light_name)) != 0)
            {
                continue;
            }
            if (fl->lit_serial[j].light_type == r->serial.light_type)
            {
                G3DLIGHT *l = &fl->all_lit[j];

                switch (r->light.Type)
                {
                case 0:
                    sceVu0CopyVector(l->vDiffuse, r->light.vDiffuse);
                    sceVu0CopyVector(l->vSpecular, r->light.vSpecular);
                    break;
                case 1:
                    sceVu0CopyVector(l->vDiffuse, r->light.vDiffuse);
                    sceVu0CopyVector(l->vSpecular, r->light.vSpecular);
                    l->fMaxRange = r->light.fMaxRange;
                    break;
                case 2:
                    sceVu0CopyVector(l->vDiffuse, r->light.vDiffuse);
                    sceVu0CopyVector(l->vSpecular, r->light.vSpecular);
                    l->fMaxRange = r->light.fMaxRange;
                    l->afPad0[0] = r->light.afPad0[0];  // the intensity (gra3dSetLightIntens)
                    break;
                }
            }
            break;
        }
    }

    // After the records: the fog, then the ambients.
    r = SceneLightRevGet(rev, num);
    memcpy(&sc->fog, r, sizeof(SCENE_FOG));
    amb = (u_char *)r + sizeof(SCENE_FOG);
    for (int k = 0; k < FOD_AMBIENT_MAX; k++, amb += sizeof(sceVu0FVECTOR))
    {
        float v[4];
        char  tag[4];

        memcpy(v, amb, sizeof(v));
        memcpy(tag, &v[3], sizeof(tag));
        if (strcmp(tag, "AMB") == 0)    // sdata 0x3720e0
        {
            fl->amb[k][0] = v[0];
            fl->amb[k][1] = v[1];
            fl->amb[k][2] = v[2];
            fl->amb[k][3] = 0.0f;
        }
        else
        {
            fl->amb[k][0] = 0.0f;
            fl->amb[k][1] = 0.0f;
            fl->amb[k][2] = 0.0f;
            fl->amb[k][3] = 0.0f;
        }
    }
}

// ──────────────────────────────────────────────────────────────────────
// Every model at frame 0. The ROM also passes each group's motion (and the
// man shape) PK4 and the row's index, which the helpers do not read.
// FF3 0x00251d80

static void SceneInitManMdl(SCN_MDL *m);
static void SceneInitOtherMdl(SCN_MDL *m);

static void SceneAllMdlInit(SCENE_CTRL *sc, SCENE_FILE *sf)
{
    (void)sf;
    for (int i = 0; i < sc->man_mdl_num; i++)
    {
        SceneInitManMdl(&sc->man_mdl[i]);
    }
    for (int i = 0; i < sc->furn_num; i++)
    {
        SceneInitOtherMdl(&sc->furn_mdl[i]);
    }
    for (int i = 0; i < sc->door_num; i++)
    {
        SceneInitOtherMdl(&sc->door_mdl[i]);
    }
    for (int i = 0; i < sc->item_num; i++)
    {
        SceneInitOtherMdl(&sc->item_mdl[i]);
    }
}

// ──────────────────────────────────────────────────────────────────────
// A man model at frame 0, no aura, opaque; its accessories hung on its
// joints (0x13 accessory 1, 7 the lantern, 2 the character's own).
// FF3 0x00251ec8

static void SceneInitManMdl(SCN_MDL *m)
{
    SceneModel *mdl = m->mdl;

    if (mdl == nullptr)
    {
        return;
    }
    if (m->mot != nullptr)
    {
        mdl->PlayMotion(0.0f, 1.0f);    // sdata 0x3720e4
    }
    if (m->shp != nullptr)
    {
        mdl->PlayShapeAll(0.0f, 1.0f);
    }
    mdl->ResetCloth();
    mdl->SetTexFrame(0);

    m->aura_type       = 0;
    m->aura_wave       = 0.0f;
    m->aura_alpha      = 0x80;
    m->aura_amp        = 0;
    m->aura_phase      = 0.0f;
    m->aura_wave_speed = 0.0f;
    m->aura_speed      = 2.0f;          // sdata 0x3720e8
    m->mdl_alpha       = 0x80;

    if (m->chr < 0)
    {
        return;
    }
    if (m->efct[1] != nullptr)
    {
        m->efct[1]->SetCoord(mdl->GetCoord(0) + mdl->GetJointCoord(0x13));
    }
    if (m->efct[0] != nullptr)
    {
        m->efct[0]->SetCoord(mdl->GetCoord(0) + mdl->GetJointCoord(7));
    }
    if (m->efct[3] != nullptr)
    {
        m->efct[3]->SetCoord(mdl->GetCoord(0) + mdl->GetJointCoord(2));
    }
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x00252080

static void SceneInitOtherMdl(SCN_MDL *m)
{
    if (m->mdl == nullptr)
    {
        return;
    }
    if (m->mot != nullptr)
    {
        m->mdl->PlayMotion(0.0f, 1.0f); // sdata 0x3720ec
    }
    m->shp       = nullptr;
    m->mdl_alpha = 0x80;
}

// ──────────────────────────────────────────────────────────────────────
// One frame of the scene: the models placed at the room's origin, the
// camera and lights blended, the models posed; the fog, the rooms, the
// furniture / doors / items, the man models far to near; then the scene
// effects, the effects, the captions (subtitle: the option) and the copy
// of the frame the effects read next frame.
// FF3 0x002520d8

static void SceneMdlUpdate(SCN_MDL *m);
static void SceneDrawManMdl(SCN_MDL *m);
static void SceneDrawOtherMdl(SCN_MDL *m);
static void SceneDrawRoom(SCENE_CTRL *sc);

void SceneDraw(SCENE_CTRL *sc, int subtitle)
{
    sceVu0FVECTOR offset;
    s_char        order[SCENE_MDL_NUM];
    int           num;
    int           timer;

    MapLoadGetOffsetVector(offset, MapLoadGetBuffID(sc->room_no));
    sceVu0UnitMatrix(sc->world);        // data 0x3293c0
    sceVu0AddVector(sc->world[3], (float *)scene_origin, offset);
    SceneRenewEnvironment(sc, offset);

    for (int i = 0; i < sc->door_num; i++)
    {
        SceneMdlUpdate(&sc->door_mdl[i]);
    }
    for (int i = 0; i < sc->furn_num; i++)
    {
        SceneMdlUpdate(&sc->furn_mdl[i]);
    }
    for (int i = 0; i < sc->item_num; i++)
    {
        SceneMdlUpdate(&sc->item_mdl[i]);
    }
    for (int i = 0; i < sc->man_mdl_num; i++)
    {
        SceneMdlUpdate(&sc->man_mdl[i]);
    }

    gra3dSetGsRegisterDefault();
    gra3dSetFog(sc->fog.min, sc->fog.max, sc->fog.near_z, sc->fog.far_z);
    gra3dSetFogColor(sc->fog.r, sc->fog.g, sc->fog.b);
    gra3dApplyFog();
    FodSetSpotLights(sc->fod_ctrl.fod_light.all_lit, 0);
    SceneDrawRoom(sc);

    for (int i = 0; i < sc->door_num; i++)
    {
        SceneDrawOtherMdl(&sc->door_mdl[i]);
    }
    for (int i = 0; i < sc->furn_num; i++)
    {
        SceneDrawOtherMdl(&sc->furn_mdl[i]);
    }
    for (int i = 0; i < sc->item_num; i++)
    {
        SceneDrawOtherMdl(&sc->item_mdl[i]);
    }

    // The man models by their screen z, smallest first (the ROM's order
    // array sits on the stack, alloca'd by man_mdl_num).
    num = sc->man_mdl_num;
    for (int i = 0; i < num; i++)
    {
        order[i] = (s_char)i;
    }
    for (int i = 0; i < num - 1; i++)
    {
        for (int j = i + 1; j < num; j++)
        {
            if (sc->man_mdl[order[j]].scr[2] < sc->man_mdl[order[i]].scr[2])
            {
                s_char t = order[j];
                order[j] = order[i];
                order[i] = t;
            }
        }
    }
    for (int i = 0; i < num; i++)
    {
        SceneDrawManMdl(&sc->man_mdl[order[i]]);
    }

    SceneEffectMain(sc, sc->eff_data);
    EffectDrawMain(0);
    FontTexSendVram();
    if (GetPALMode() == 0)
    {
        timer = (int)sc->fod_ctrl.now_frame;
    }
    else
    {
        timer = (int)(sc->fod_ctrl.float_now_frame / SCENE_PAL_FRAME);
    }
    MovieTitleMain(timer, subtitle);
    EffectOthBufCopy();
}

// ──────────────────────────────────────────────────────────────────────
// A model posed for this frame: the scene's world matrix, the motion frame
// (float_now_frame - 1), its alpha (the fade bit cleared); a man model's
// joint 0x1b and joint 3 positions and the first on screen.
// FF3 0x002524f0

static void SceneMdlUpdate(SCN_MDL *m)
{
    SCENE_CTRL *sc  = m->sc;
    SceneModel *mdl = m->mdl;

    if (mdl == nullptr)
    {
        return;
    }
    mdl->SetMatrix(sc->world);
    if (m->mot != nullptr)
    {
        mdl->SetFrame(sc->fod_ctrl.float_now_frame - 1.0f);    // sdata 0x3720f4
    }
    m->mdl_alpha &= 0x7fffffff;
    mdl->SetAlpha((u_char)m->mdl_alpha);
    if (m->kind != SCN_MDL_MAN)
    {
        return;
    }
    mdl->GetCoordPos(m->head, mdl->GetJointCoord(0x1b), (float *)scene_origin);     // FF3: vtable +0x34 / +0x2c
    mdl->GetCoordPos(m->pos, mdl->GetJointCoord(3), (float *)scene_origin);
    sceVu0RotTransPers(m->scr, gra3dGetCamera()->matWorldScreen, m->head, 0);
}

// ──────────────────────────────────────────────────────────────────────
// A man model drawn under its own FOD lights: with its accessories (drawn
// before the body, or after for accessory 2) and, on the player's
// character with the lantern, the lens flare; or, with an aura set on a
// non-player character, the enemy aura over the body.
// FF3 0x00252630

static void SceneDrawManMdl(SCN_MDL *m)
{
    SceneModel *mdl = m->mdl;
    SCENE_CTRL *sc  = m->sc;
    int         plyr_chr;

    if (mdl == nullptr)
    {
        return;
    }
    mdl->UpdateMatrix();
    if (m->disp == 0 || m->mdl_alpha == 0)
    {
        return;
    }
    FodSetMyLight(&sc->fod_ctrl.fod_light, m->name);
    plyr_chr = (m->chr >= 0);

    if (m->aura_type == 0 || plyr_chr)
    {
        int after[4];

        for (int k = 0; k < 4; k++)
        {
            after[k] = 0;
            if (m->efct[k] == nullptr)
            {
                continue;
            }
            m->efct[k]->SetAlpha((u_char)m->mdl_alpha);
            m->efct[k]->UpdateMatrix();
            after[k] = PlyrAcsIsNot2((u_char)m->chr, PlyrGetAcs(m->chr));
            if (after[k] == 0)
            {
                m->efct[k]->DrawMdl();
            }
        }
        mdl->DrawMdl();
        for (int k = 0; k < 4; k++)
        {
            if (m->efct[k] != nullptr && after[k] != 0)
            {
                m->efct[k]->DrawMdl();
            }
        }
        if (plyr_chr && sc->hdr_flg[1] != 0 && SceneEffectLenzFlareFlgGet() != 0)
        {
            SceneEffectLenzFlareExec(m);
        }
        return;
    }

    ENE_AURA_DAT *aura = EneAuraDatGet(nullptr, m->aura_type);
    float         s;

    EneAuraFrameBegin();
    mdl->DrawMdl();
    EneAuraFrameEnd(aura);
    s = Vu0Sinf((m->aura_wave * 3.1415927f) / 180.0f);     // sdata 0x3720f8 / 0x3720fc
    EneAuraDraw(aura, (u_char)(int)((float)m->aura_amp * s + (float)m->aura_alpha), m->aura_phase);
    m->aura_phase += m->aura_speed;
    if (m->aura_phase > 360.0f)         // sdata 0x372100
    {
        m->aura_phase -= 360.0f;
    }
    m->aura_wave += m->aura_wave_speed;
    if (m->aura_wave > 360.0f)
    {
        m->aura_wave -= 360.0f;
    }
    gra3dSetGsRegisterDefault();
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x002528a0

static void SceneDrawOtherMdl(SCN_MDL *m)
{
    SceneModel *mdl = m->mdl;

    if (mdl == nullptr)
    {
        return;
    }
    mdl->UpdateMatrix();
    FodSetMyLight(&m->sc->fod_ctrl.fod_light, m->name);
    mdl->DrawMdl();
}

// ──────────────────────────────────────────────────────────────────────
// The rooms under the scene's "room" lights.
// FF3 0x00252910

static void SceneDrawRoom(SCENE_CTRL *sc)
{
    FodSetMyLight(&sc->fod_ctrl.fod_light, "room");     // sdata 0x372108
    MapPutDrawFlagClear();
    MapDrawAll();
    MapPutDraw(0);
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x00252948

int SceneRoomNoGet(SCENE_CTRL *sc)
{
    if (sc == nullptr)
    {
        return -1;
    }
    return sc->room_no;
}

// FF3 0x00252960

int SceneSubRoomNoGet(SCENE_CTRL *sc)
{
    if (sc == nullptr)
    {
        return -1;
    }
    return sc->sub_room_no;
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x00252a50

void SceneFodSetFrame(u_int frame)
{
    SCENE_CTRL *sc = ScenePlayCtrlGet();

    if (sc != nullptr)
    {
        FodSetFrame(&sc->fod_ctrl, frame);
    }
}

// FF3 0x00252a80

FOD_LIGHT *SceneFodLightPtrGet(void)
{
    SCENE_CTRL *sc = ScenePlayCtrlGet();

    if (sc == nullptr)
    {
        return nullptr;
    }
    return &sc->fod_ctrl.fod_light;
}

// ──────────────────────────────────────────────────────────────────────
// The playing slot's man model (mdl_id, index), nullptr: the lookup the
// four man-model queries below share (inline in the ROM).

static SCN_MDL *SceneManModelFind(int mdl_id, int index)
{
    SCENE_CTRL *sc = ScenePlayCtrlGet();
    int         no = SceneManModelIdToSceneModelNo(sc, (short)mdl_id, index);

    if (no < 0 || no >= sc->man_mdl_num)
    {
        return nullptr;
    }
    return &sc->man_mdl[no];
}

// callcheck: ScenePlayCtrlGet, SceneManModelIdToSceneModelNo (through SceneManModelFind)
// FF3 0x00252a98

void SceneGetModelPDeformPos(float dist, float *pos, int mdl_id, int index)
{
    SCN_MDL      *m = SceneManModelFind(mdl_id, index);
    sceVu0FVECTOR rot;
    sceVu0FVECTOR v;

    if (m == nullptr || m->mdl == nullptr)
    {
        return;
    }
    sceVu0CopyVector(pos, m->head);
    v[0] = 0.0f;
    v[1] = 0.0f;
    v[2] = dist;
    v[3] = 0.0f;
    GetTrgtRot(pos, gra3dcamGetPosition(), rot, 3);
    RotVectorZXY(v, v, rot);
    sceVu0AddVector(pos, pos, v);
}

// callcheck: ScenePlayCtrlGet, SceneManModelIdToSceneModelNo (through SceneManModelFind)
// FF3 0x00252b88

void SceneManModelPosGet(float *pos, int mdl_id, int index)
{
    SCN_MDL *m = SceneManModelFind(mdl_id, index);

    if (m != nullptr && m->mdl != nullptr)
    {
        sceVu0CopyVector(pos, m->pos);
    }
}

// callcheck: ScenePlayCtrlGet, SceneManModelIdToSceneModelNo (through SceneManModelFind)
// FF3 0x00252c28

void SceneManModelTexFrameSet(int mdl_id, int index, u_int frame)
{
    SCN_MDL *m = SceneManModelFind(mdl_id, index);

    if (m != nullptr && m->mdl != nullptr && frame <= 0x80)
    {
        m->mdl->SetTexFrame((u_char)frame);
    }
}

// callcheck: ScenePlayCtrlGet, SceneManModelIdToSceneModelNo (through SceneManModelFind)
// FF3 0x00252ce0

void SceneManModelAuraSet(float speed, float wave_speed, int mdl_id, int index, u_char type, u_char alpha, u_char amp)
{
    SCN_MDL *m = SceneManModelFind(mdl_id, index);

    if (m == nullptr)
    {
        return;
    }
    m->aura_type       = type;
    m->aura_wave_speed = wave_speed;
    m->aura_speed      = speed;
    m->aura_alpha      = alpha;
    m->aura_amp        = amp;
}

// ──────────────────────────────────────────────────────────────────────
// "spot<no>" / "point<no>" ("<no>" for types 0 and 1); "" for type >= 4.
// FF3 0x00252db0

static void SceneMakeLightName(char *name, u_int type, int no)
{
    if (name == nullptr)
    {
        return;
    }
    if (type < 4)
    {
        sprintf(name, "%s%d", scene_light_type_name[type], no);
        return;
    }
    name[0] = 0;
}

// "ch012" / "f016" / "d003" / "o001"; "" for type >= 4.
// FF3 0x00252e00

static void SceneMakeModelPrefix(char *name, u_int type, int id)
{
    if (name == nullptr)
    {
        return;
    }
    if (type < 4)
    {
        sprintf(name, "%s%03d", scene_mdl_prefix[type], id);
        return;
    }
    name[0] = 0;
}

// ──────────────────────────────────────────────────────────────────────
// Every light of the playing scene of type whose name holds both the light
// name and the model prefix: colour, range (power, half of it the near
// range) and a spot's cone intensity (cos^2 of cone degrees).
// FF3 0x00252e50

void SceneChangeLightParameter(float r, float g, float b, float power, float cone,
                               int type, int light_no, int mdl_type, int mdl_id)
{
    SCENE_CTRL *sc = ScenePlayCtrlGet();
    FOD_LIGHT  *fl;

    if (sc == nullptr)
    {
        return;
    }
    fl = &sc->fod_ctrl.fod_light;
    SceneMakeLightName(scene_light_name, (u_int)type, light_no);
    SceneMakeModelPrefix(scene_mdl_name, (u_int)mdl_type, mdl_id);
    if (scene_light_name[0] == 0 || scene_mdl_name[0] == 0)
    {
        return;
    }

    for (int i = 0; i < fl->all_lit_num; i++)
    {
        G3DLIGHT *l = &fl->all_lit[i];

        if (fl->lit_serial[i].light_type != (u_char)type)
        {
            continue;
        }
        if (strstr(fl->lit_serial[i].light_name, scene_light_name) == nullptr
            || strstr(fl->lit_serial[i].light_name, scene_mdl_name) == nullptr)
        {
            continue;
        }
        l->fMaxRange   = power;
        l->vDiffuse[0] = r;
        l->vDiffuse[1] = g;
        l->vDiffuse[2] = b;
        l->fMinRange   = power * 0.5f;  // sdata 0x372158
        if (type == 2)
        {
            float c = Vu0Cosf((cone * 3.1415927f) / 180.0f);    // sdata 0x37215c / 0x372160

            gra3dSetLightIntens(l, c * c);
        }
    }
}

// ──────────────────────────────────────────────────────────────────────
// The playing scene's hand spot light (the one SceneRenewEnvironment copies
// into the player's flashlight).
// FF3 0x00253028

void SceneChangeHandSpotLightParameter(float r, float g, float b, float power, float cone)
{
    SCENE_CTRL *sc = ScenePlayCtrlGet();
    G3DLIGHT   *l;
    float       c;

    if (sc == nullptr || sc->fod_ctrl.fod_light.hand_spot_no == -1)
    {
        return;
    }
    l = &sc->fod_ctrl.fod_light.all_lit[sc->fod_ctrl.fod_light.hand_spot_no];
    l->vDiffuse[0] = r;
    l->vDiffuse[1] = g;
    l->vDiffuse[2] = b;
    l->fMinRange   = power * 0.5f;      // sdata 0x372164
    l->fMaxRange   = power;
    c = Vu0Cosf((cone * 3.1415927f) / 180.0f);  // sdata 0x372168 / 0x37216c
    gra3dSetLightIntens(l, c * c);
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x002530e8.

const char *SceneDatGetName(int scene_no)
{
    if ((u_int)scene_no < SCENE_NUM)
    {
        return scene_dat[scene_no].name;
    }
    return "SCENE NO ERROR";
}

// FF3 0x00253118.

int SceneDatGetMovie(int scene_no)
{
    if ((u_int)scene_no < SCENE_NUM)
    {
        return scene_dat[scene_no].movie;
    }
    return 0;
}

// FF3 0x00253148.

int GetFileNoFromSceneNo(int scene_no)
{
    if ((u_int)scene_no < SCENE_NUM)
    {
        return scene_no * 3 + SCENE0010M_PSS;
    }
    return SCENE0010M_PSS;
}

// The scene's effect script (Story_Movie loads it; SceneMovieEffectMain runs
// it): the third of its three files.
// FF3 0x00253168.

int SceneEffectDataFileNoGet(int scene_no)
{
    return GetFileNoFromSceneNo(scene_no) + 2;
}

// FF3 0x00253188.

int SceneDecisionMovie(int scene_no)
{
    return SceneDatGetMovie(scene_no);
}

// FF3 0x002531b8.

int SceneDatGetValue(int scene_no)
{
    if ((u_int)scene_no < SCENE_NUM)
    {
        return scene_dat[scene_no].value;
    }
    return 0;
}

// FF3 0x002531e8.

void SceneDatSetValue(int scene_no, int value)
{
    if ((u_int)scene_no < SCENE_NUM)
    {
        scene_dat[scene_no].value = value;
    }
}

// FF3 0x00253210.

void SceneWrkClear(void)
{
    memset(scene_ctrl, 0, sizeof(scene_ctrl));
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x00253238

SCENE_CTRL *SceneCtrlFind(int scene_no)
{
    if ((u_short)scene_no >= SCENE_NUM)
    {
        return nullptr;
    }
    for (int i = 0; i < SCENE_CTRL_NUM; i++)
    {
        if ((u_short)scene_ctrl[i].scene_no == (u_short)scene_no)
        {
            return &scene_ctrl[i];
        }
    }
    return nullptr;
}

// ──────────────────────────────────────────────────────────────────────
// callcheck: operator_new, BorrowBufCtor (the new-expression)
// FF3 0x00253288

SCENE_CTRL *SceneCtrlLoadReq(int scene_no)
{
    SCENE_CTRL *sc;
    BORROW_BUF *buf;
    u_int       size;

    if ((u_short)scene_no >= SCENE_NUM)
    {
        return nullptr;
    }
    sc = SceneCtrlFind(scene_no);
    if (sc != nullptr)
    {
        return sc;
    }

    for (int i = 0; i < SCENE_CTRL_NUM; i++)
    {
        if (scene_ctrl[i].load_step == 0)
        {
            sc = &scene_ctrl[i];
            break;
        }
    }
    if (sc == nullptr)
    {
        return nullptr;
    }

    size = GetFileSize(GetFileNoFromSceneNo(scene_no));
    if (size == 0)
    {
        return nullptr;
    }
    buf = new BORROW_BUF(size);
    sc->scn_buf = buf;
    if (buf->adrs == nullptr)
    {
        SceneCtrlRelease(sc);
        return nullptr;
    }
    sc->scn_data_addr = buf->adrs;
    sc->scene_no      = (short)scene_no;
    sc->load_step     = 2;
    SceneFileLoadReq(sc);
    return sc;
}

// ──────────────────────────────────────────────────────────────────────
// 2: the files in -> the header read, the models reset and the man models
// created; 3: the man models in -> the other models created; 4: those in.
// FF3 0x002533c0

int SceneDataLoadMain(SCENE_CTRL *sc)
{
    if (sc == nullptr)
    {
        return -1;
    }

    switch (sc->load_step)
    {
    case 0:
    case 5:
        return 1;
    case 2:
        if (SceneDataLoadWait(sc) == 0)
        {
            return 0;
        }
        SceneCtrlInit(sc);
        IngameMdlResetAll(0);
        SceneManModelLoad(sc);
        sc->load_step = 3;
        break;
    case 3:
        if (SceneManModelIsLoaded(sc) == 0)
        {
            return 0;
        }
        SceneObjModelLoad(sc);
        sc->load_step = 4;
        break;
    case 4:
        if (SceneObjModelIsLoaded(sc) != 0)
        {
            sc->load_step = 5;
        }
        break;
    }
    return 0;
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x00253490

int SceneRoomLoadReq(SCENE_CTRL *sc, int play)
{
    (void)play;
    if (sc == nullptr || sc->load_step <= 2)
    {
        return -1;
    }
    if (sc->room_step == 0 || sc->room_step == 6)
    {
        sc->room_step = 1;
        return 1;
    }
    sc->room_step = 6;
    return 0;
}

// ──────────────────────────────────────────────────────────────────────
// 1: once no room load runs, 4 (2 and 3 fall through to it); 4: the room
// before the scene saved, the scene's room loaded on the header's side; 5:
// that load done -> 6.
// FF3 0x002534e0

u_char SceneRoomLoadMain(SCENE_CTRL *sc)
{
    int    room;
    u_char ret = 0;

    if (sc == nullptr)
    {
        return 0xff;
    }
    room = SceneRoomNoGet(sc);
    MapPutSetSceneLoad(1);
    model_collision_set.ClearHit();

    switch (sc->room_step)
    {
    case 0:
    case 6:
        ret = 1;
        break;
    case 1:
        if (MapLoadCheckLoadNow() == 0)
        {
            sc->room_step = 4;
        }
        break;
    case 2:
        sc->room_step = 3;
        // fall through
    case 3:
        sc->room_step = 4;
        // fall through
    case 4:
        sc->room_bak = (short)MapLoadGetRoomNoNow();
        MapLoadChangeArea(room, sc->nuno_flg != 0);
        sc->room_step = 5;
        break;
    case 5:
        if (MapLoadCheckLoadNow() == 0)
        {
            sc->room_step = 6;
        }
        break;
    default:
        ret = 0xff;
        break;
    }
    MapPutSetSceneLoad(0);
    return ret;
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x002535f8

int SceneStreamLoadReq(SCENE_CTRL *sc)
{
    if (sc == nullptr || sc->stream_step != 0)
    {
        return -1;
    }
    sc->stream_step = 1;
    return 1;
}

// ──────────────────────────────────────────────────────────────────────
// The member after the 8-char name in a PK4 whose member 0 is the name
// table; nullptr if none.
// FF3 0x002be570 (out of line)

static void *Pk4FindByName(void *pk4, const char *name)
{
    const char *names;
    int         num;

    if (name == nullptr)
    {
        return nullptr;
    }
    num   = (pk4 != nullptr) ? ((PK4_HEADER *)pk4)->num : 0;
    names = (const char *)Pk4GetData(pk4, 0);
    for (int i = 0; i < num - 1; i++, names += 8)
    {
        if (strncmp(names, name, 8) == 0)
        {
            return Pk4GetData(pk4, i + 1);
        }
    }
    return nullptr;
}

// ──────────────────────────────────────────────────────────────────────
// The rooms, counts, flags and model names from the header.
// callcheck: strncpy (the ROM then copies each name to a dead stack buffer, FF2's debug print)
// FF3 0x002be628 (out of line)

static void SceneGetHeaderData(SCENE_CTRL *sc, SCENE_HDR *hdr)
{
    u_char *p;

    sc->room_no     = (short)SceneRoomNameToNo(hdr->room);
    sc->sub_room_no = (short)SceneRoomNameToNo(hdr->sub_room);
    sc->nuno_flg    = hdr->nuno_flg;
    sc->man_mdl_num = (s_char)hdr->man_mdl_num;
    sc->door_num    = (s_char)hdr->door_num;
    sc->furn_num    = (s_char)hdr->furn_num;
    sc->item_num    = (s_char)hdr->item_num;
    for (int i = 0; i < 4; i++)
    {
        sc->hdr_flg[i] = hdr->flg[i];
    }

    p = (u_char *)(hdr + 1);
    p = SceneMdlNameSet(sc->man_mdl, sc->man_mdl_num, p);
    p = SceneMdlNameSet(sc->door_mdl, sc->door_num, p);
    p = SceneMdlNameSet(sc->furn_mdl, sc->furn_num, p);
    p = SceneMdlNameSet(sc->item_mdl, sc->item_num, p);
    SceneMdlAddSet(sc->man_mdl, sc->man_mdl_num, p);
}

// ──────────────────────────────────────────────────────────────────────
// Record no of the light revision file; no == its count gives the fog and
// ambients after the records; nullptr past that.
// FF3 0x002be940 (out of line)

static SCENE_LIGHT_REV *SceneLightRevGet(u_int *rev, int no)
{
    if (rev == nullptr || (int)rev[0] < no)
    {
        return nullptr;
    }
    return (SCENE_LIGHT_REV *)(rev + 4) + no;
}

// ──────────────────────────────────────────────────────────────────────
// On whole frames (now_reso 0) the lights and the camera blended between
// the current and next FOD records by the frame's fraction; the scene's
// hand spot light becomes the player's flashlight (aimed from its position
// to its target); the camera applied with the scene clip, the sound
// listener at the camera.
// FF3 0x002be990 (out of line)

static void SceneRenewEnvironment(SCENE_CTRL *sc, float *offset)
{
    FOD_CTRL *fc = &sc->fod_ctrl;
    FOD_LIGHT *hl = &fc->fod_light;
    G3DLIGHT  *pl = &((CharPlayerModel *)ingame_wrk.plyr_wrk->mdl)->fl;

    if (fc->now_reso == 0)
    {
        // FF3 holds both light copies on the stack (0x14c0 bytes each).
        static FOD_LIGHT cur;
        static FOD_LIGHT next;
        FOD_CAMERA_DATA  cur_cam;
        FOD_CAMERA_DATA  next_cam;
        float            t, u;

        cur  = fc->fod_light;
        next = fc->fod_light;
        FodGetToSgLight(&cur, fc->lit_frame, offset);
        FodGetToSgLight(&next, fc->lit_frame_next, offset);
        SceneLightInterp(fc->float_now_frame, &fc->fod_light, &cur, &next);

        cur_cam  = sc->CameraData;
        next_cam = sc->CameraData;
        FodGetCamData(&cur_cam, fc->cam_frame, offset);
        FodGetCamData(&next_cam, fc->cam_frame_next, offset);
        t = fc->float_now_frame - (float)(int)fc->float_now_frame;
        sceVu0InterVector(sc->CameraData.vPosition, next_cam.vPosition, cur_cam.vPosition, t);
        sceVu0InterVector(sc->CameraData.vTarget, next_cam.vTarget, cur_cam.vTarget, t);
        u = 1.0f - t;                   // sdata 0x37239c
        sc->CameraData.fRoll  = cur_cam.fRoll * u + next_cam.fRoll * t;
        sc->CameraData.fFov   = cur_cam.fFov * u + next_cam.fFov * t;
        sc->CameraData.fNearZ = cur_cam.fNearZ * u + next_cam.fNearZ * t;
        sc->CameraData.fFarZ  = cur_cam.fFarZ * u + next_cam.fFarZ * t;
    }

    if (hl->hand_spot_no != -1)
    {
        *pl = hl->all_lit[hl->hand_spot_no];
        sceVu0SubVector(pl->vDirection, pl->vPosition, pl->vDirection);
        sceVu0Normalize(pl->vDirection, pl->vDirection);
        sceVu0SubVector(pl->vDirection, (float *)scene_zero, pl->vDirection);
        pl->Type = 2;                   // G3DLIGHT_SPOT
    }

    gra3dcamSetPosition(sc->CameraData.vPosition);
    gra3dcamSetTarget(sc->CameraData.vTarget, 1);
    gra3dcamSetRoll(sc->CameraData.fRoll);
    gra3dcamSetFov(sc->CameraData.fFov);
    gra3dcamSetClip(SCENE_CLIP_NEAR, SCENE_CLIP_FAR);
    gra3dApplyCamera(nullptr, 1, 1);
    IngameSndListenerSet(0);
}

// ──────────────────────────────────────────────────────────────────────
// The index-th man model whose object number is mdl_id (the first for index
// < 0), -1. PC: nullptr gives -1 (the ROM reads the slot unguarded; its
// callers run only while a scene plays).
// FF3 0x002becf8 (out of line)

static int SceneManModelIdToSceneModelNo(SCENE_CTRL *sc, short mdl_id, int index)
{
    int found = -1;

    if (sc == nullptr)
    {
        return -1;
    }
    for (int i = 0; i < sc->man_mdl_num; i++)
    {
        if (sc->man_mdl[i].obj_no != mdl_id)
        {
            continue;
        }
        found++;
        if (index < 0 || found == index)
        {
            return i;
        }
    }
    return -1;
}

// ──────────────────────────────────────────────────────────────────────
// The man models' add bytes: one 4-byte entry each. (The ROM then aligns
// the pointer with GetAlignUp and drops the result.)
// callcheck: GetAlignUp (its result is unused)
// FF3 0x002bed48 (out of line)

static void SceneMdlAddSet(SCN_MDL *row, int num, u_char *data)
{
    for (; num > 0; num--, row++, data += 4)
    {
        row->add = (s_char)data[0];
    }
}

// ──────────────────────────────────────────────────────────────────────
// fl's lights between cur and next at frame's fraction, by type:
// directional 0 (colours, direction), point 1 (colours, position), spot 2
// (all four and the cone angles).
// FF3 0x002beda0 (out of line)

static void SceneLightInterp(float frame, FOD_LIGHT *fl, FOD_LIGHT *cur, FOD_LIGHT *next)
{
    float t = frame - (float)(int)frame;
    float u;

    for (int i = 0; i < fl->all_lit_num; i++)
    {
        G3DLIGHT *o = &fl->all_lit[i];
        G3DLIGHT *c = &cur->all_lit[i];
        G3DLIGHT *n = &next->all_lit[i];

        switch (o->Type)
        {
        case 0:
            sceVu0InterVector(o->vDiffuse, n->vDiffuse, c->vDiffuse, t);
            sceVu0InterVector(o->vSpecular, n->vSpecular, c->vSpecular, t);
            sceVu0InterVector(o->vDirection, n->vDirection, c->vDirection, t);
            break;
        case 1:
            sceVu0InterVector(o->vDiffuse, n->vDiffuse, c->vDiffuse, t);
            sceVu0InterVector(o->vSpecular, n->vSpecular, c->vSpecular, t);
            sceVu0InterVector(o->vPosition, n->vPosition, c->vPosition, t);
            break;
        case 2:
            sceVu0InterVector(o->vDiffuse, n->vDiffuse, c->vDiffuse, t);
            sceVu0InterVector(o->vSpecular, n->vSpecular, c->vSpecular, t);
            sceVu0InterVector(o->vPosition, n->vPosition, c->vPosition, t);
            sceVu0InterVector(o->vDirection, n->vDirection, c->vDirection, t);
            u = 1.0f - t;               // sdata 0x3723a8
            o->fAngleOutside = c->fAngleOutside * u + n->fAngleOutside * t;
            o->fAngleInside  = c->fAngleInside * u + n->fAngleInside * t;
            break;
        }
    }
}
