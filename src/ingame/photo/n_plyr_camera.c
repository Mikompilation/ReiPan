// ingame/photo/n_plyr_camera.c -- the two companion NPCs (FF3 0x21e360..0x221e90).
// FF3-only: FF2's n_plyr_camera.c (this file's link slot) is a different
// module. Two works (PLYR_CAMERA_WRK, n_plyr_camera.h) each hold a
// CharNpcModel while the NPC is shown, the NPC's act block (its AI, stepped
// by ingame/npc/npc_act.c), three facial channels, the position the motion's
// root move drives, and the NPC_* event settings. The act hooks at the end
// (idle / chat / watch) are called through the act scripts (ingame/npc/npc_act.c).
// Reconstructed from FF3; nothing came from FF2.
// Virtual calls on the model go straight to CharNpcModel's slot functions
// (the class is not reconstructed; see ingame/ingame.h).

#include <math.h>                       // fabsf
#include <string.h>                     // memcpy

#include <new>                          // placement new on FF3's C++ heap (operator_new)

#include "ingame/photo/n_plyr_camera.h"
#include "common/cpp_heap.h"            // operator_new
#include "common/vu0math.h"             // RotLimitChk, RotVectorZXY, GetRandValIByF
#include "graphics/effect/effect.h"     // SetEffects, CutEffects
#include "graphics/graph2d/fade.h"     // FadeInReq, FadeOutReq, GetFadeState
#include "graphics/graph3d/gra3d.h"     // GRA3DLIGHTDATA, gra3dEmulateLightData, gra3dSetLightData, gra3dGetCamera
#include "graphics/graph3d/gra3dMisc.h" // GRA3DEMULATIONLIGHTDATACREATIONDATA, gra3dGenerateLightDataToChar
#include "graphics/mmanage.h"           // ModelManager::Draw
#include "graphics/motion/motion.h"     // MDL_MOT
#include "ingame/cat/cat_ctrl.h"        // CatSetLabelLock, CatGetLabel, CatActRestart
#include "ingame/char_model.h"          // CharNpcModel
#include "ingame/char_model_sub.h"     // CharDrawDelete, CharDrawSetPlyrCamera, CharWatchControl
#include "ingame/event/ev_queue.h"     // EvQueueGetNext, EvQueueStart, EvQueueMain
#include "ingame/npc/npc_act.h"         // EvNowSynchroMode
#include "ingame/ingame.h"              // GameModel, CharNpcModel, CHAR_MDL_CTRL, CHAR_WATCH_PRM
#include "ingame/ingame_wrk.h"          // ingame_wrk
#include "ingame/map/MapDraw.h"         // MapDrawGetLightPtr
#include "ingame/map/MapObjWrk.h"       // MAP_OBJ_MDL
#include "ingame/map/MapPut.h"          // MapPutGetMdl
#include "ingame/npc/npc_act.h"         // the act machine, labels, serif lines, talk cameras
#include "ingame/menu/anim_2d.h"        // LayoutAnim*, Anim2dMsg*
#include "ingame/photo/n_equip_tray.h"  // the NPC data / motion helpers
#include "ingame/plyr/player.h"         // PLYR_WRK, GetPlyrAreaNo
#include "ingame/plyr/plyr_mdl.h"       // PlyrFinderModeEnd, PlyrIsFinderModeOff
#include "ingame/plyr/plyr_sub.h"       // AllPlyrIsReady, PlyrGetJointPos
#include "ingame/plyr/unit_ctl.h"       // UnitGetTrgtRot
#include "ingame/sound/snd_ctrl.h"      // SndSeFadeStop
#include "ingame/story_menu.h"          // LockTblInc / Dec, IsLockTblOn / Cnt
#include "main/gphase.h"                // GetGPhaseCtrl
#include "reipan/os/reipan_rand.h"      // PC: ReiPan_Rand
#include "system/os/system.h"           // GetFrameTimeScale, GetPALMode
#include "system/pad/pad_ctrl.h"        // CheckPadTrgPlaySe, pad_key

// ──────────────────────────────────────────────────────────────────────
// Callees in runs that are not defined yet. Move each block to its real
// header when that run is defined.
// ──────────────────────────────────────────────────────────────────────

// ──────────────────────────────────────────────────────────────────────

PLYR_CAMERA_SAVE plyr_camera;                   // data 0x3f62b0
int plyr_camera_se[PLYR_CAMERA_NUM];            // sbss 0x373738

// The NPC models' character light (data 0x35f6d0; CharNpcModel +0x28 takes
// it too).
GRA3DEMULATIONLIGHTDATACREATIONDATA plyr_camera_light =
{
    { 0.06f, 0.06f, 0.06f, 0.0f },
    1.0f, 1.0f, 0.55f,
    1, 1, 0, 0, 0, 0, 0,
    { 0, 0 },
};

static int   plyr_camera_turn_skip;             // sdata 0x3716c4: the next turn callback clears the turn
static short plyr_camera_blink_cnt[PLYR_CAMERA_NUM];    // sdata 0x3716d8

// ──────────────────────────────────────────────────────────────────────
// Stops the NPC's fading SE.
// FF3 0x0021e360

static void PlyrCameraSeStop(s_char no)
{
    if (plyr_camera_se[no] != -1)
    {
        SndSeFadeStop(plyr_camera_se[no], 0);
        plyr_camera_se[no] = -1;
    }
}

// FF3 0x0021e3b8

PLYR_CAMERA_HEAD *PlyrCameraGetHead(void)
{
    return &plyr_camera.head;
}

// FF3 0x0021e3c8

static void PlyrCameraHeadLoadRestore(void)
{
}

// FF3 0x0021e3d0

static void PlyrCameraHeadReset(void)
{
    PLYR_CAMERA_HEAD *head = PlyrCameraGetHead();

    head->serif_tbl  = 0;
    head->_02        = 0xff;
    head->facial_tbl = 0;
}

// FF3 0x0021e400

static void PlyrCameraHeadInit(void)
{
    PlyrCameraHeadLoadRestore();
    PlyrCameraHeadReset();
}

// FF3 0x0021e420

PLYR_CAMERA_WRK *GetPlyrCameraWrk(s_char no)
{
    return &plyr_camera.wrk[no];
}

// ──────────────────────────────────────────────────────────────────────
// After a load: no model yet; the act clock is rebased.
// FF3 0x0021e448

static void PlyrCameraWrkLoadRestore(s_char no)
{
    PLYR_CAMERA_WRK *cam = GetPlyrCameraWrk(no);

    cam->mdl     = nullptr;
    cam->ready   = 0;
    cam->visible = 0;
    cam->talk_flg    = 0;
    PlyrCameraAcsLoadRestore(cam);
    PlyrCameraActPcLoad(&cam->act);
}

// A new game: model type 0, no area, no motion, the eye target and the
// facial channels off, the act block for NPC no.
// FF3 0x0021e490

static void PlyrCameraWrkReset(s_char no)
{
    PLYR_CAMERA_WRK *cam = GetPlyrCameraWrk(no);
    s_char           i;

    cam->type     = 0;
    cam->type_old = 0;
    cam->area     = -1;
    cam->pos[3]   = 1.0f;               // sdata 0x3716b0
    cam->mot      = -1;
    cam->pos[0]   = 0.0f;
    cam->pos[1]   = 0.0f;
    cam->pos[2]   = 0.0f;
    cam->rot[0]   = 0.0f;
    cam->rot[1]   = 0.0f;
    cam->rot[2]   = 0.0f;
    cam->rot[3]   = 0.0f;
    cam->eye_pos_on = 0;
    cam->eye_pos[0] = 0.0f;
    cam->eye_pos[1] = 0.0f;
    cam->eye_pos[2] = 0.0f;
    cam->eye_pos[3] = 0.0f;
    cam->fix_label = -1;
    cam->free      = -1;
    cam->eye_on    = 0;
    cam->eye_mode  = 0;
    cam->active    = 0;
    cam->_0ec      = 0;
    PlyrCameraColorReset(&cam->color);
    PlyrCameraAcsReset(cam);
    PlyrCameraActReset(&cam->act, no, 0, no);

    for (i = 0; i < PLYR_CAMERA_LOCK_NUM; i++)
        cam->lock_pos[i] = 0;
}

// FF3 0x0021e580

static void PlyrCameraWrkInit(s_char no)
{
    PlyrCameraActClear(&GetPlyrCameraWrk(no)->act);
    PlyrCameraWrkLoadRestore(no);
    PlyrCameraWrkReset(no);
}

// Every channel idle, no faces.
// FF3 0x0021e5c0

void PlyrCameraColorReset(PLYR_CAMERA_COLOR *col)
{
    s_char i;

    col->on    = 0;
    col->blink = 0;
    for (i = 0; i < 3; i++)
    {
        col->state[i] = -1;
        col->req[i]   = -1;
        col->cur[i]   = -1;
    }
}

// FF3 0x0021e600

GameModel *PlyrCameraGetMdl(s_char no)
{
    PLYR_CAMERA_WRK *cam = GetPlyrCameraWrk(no);

    if (cam->ready == 0)
        return nullptr;
    return cam->mdl;
}

// FF3 0x0021e638

static void PlyrCameraWrkSavePre(s_char no)
{
    PlyrCameraActPcSave(&GetPlyrCameraWrk(no)->act);
}

// Save block 54: the head and both works.
// PC: the block is saved in the host layout (the works hold the model
// pointer, which PlyrCameraWrkLoadRestore clears after a load); FF3's is
// 0x2b0 bytes (open-items "Save blocks vs. host layouts").
// FF3 0x0021e660

int PlyrCameraSaveGet(void **addr)
{
    PlyrCameraWrkSavePre(0);
    PlyrCameraWrkSavePre(1);
    *addr = &plyr_camera;
    return (int)sizeof(plyr_camera);
}

// ──────────────────────────────────────────────────────────────────────
// A map object drawn with NPC 0's light (MapDraw.c calls it by address),
// unless NPC 0 is on screen with the cat model. A MAP_PUT_DRAW_FUNC; flags
// is not read.
// FF3 0x0021e6a0

void PlyrCameraMapObjDraw(MAP_PUT *put, u_int flags)
{
    static float pos[4] = { 80.45f, -49.9f, 109.2f, 1.0f };    // sdata 0x3716b4
    PLYR_CAMERA_WRK *cam = GetPlyrCameraWrk(0);
    MAP_OBJ_MDL     *mdl;
    int              shown = 0;

    if (cam != nullptr)
    {
        shown = cam->visible != 0 && cam->active != 0 && cam->ready != 0;
        if (PlyrCameraIsCatMdl(cam) == 0)
            shown = 0;
    }
    if (shown)
        return;

    mdl = MapPutGetMdl(put);
    if (mdl != nullptr)
    {
        PlyrCameraSetLight(0, 0x32, pos);
        mdl->mot->Draw();
    }
}

// The motion control's callback (CharModel::SetYawFunc): the motion's turn this
// frame, kept unless it is under pi/6 or the NPC is fixed. A motion started
// with arg 0 (PlyrCameraMotPlay) drops its first turn.
// callcheck: fptodp, fabs, dpcmp (FF3's fabs on the double
// promotion and its compare; fabsf here)
// FF3 0x0021e770

static void PlyrCameraMotTurnCB(float turn, void *arg)
{
    PLYR_CAMERA_WRK *cam = (PLYR_CAMERA_WRK *)arg;

    if (cam == nullptr)
        return;

    if (plyr_camera_turn_skip != 0)
    {
        plyr_camera_turn_skip = 0;
        cam->turn[1] = 0.0f;
    }
    else
    {
        // FF3 compares in double against (double)0.52359879f.
        if (fabsf(turn) < 0.52359879f)
            return;
        cam->turn[1] = turn;
    }
    if (cam->free == 0)
        cam->turn[1] = 0.0f;
}

// While the anime runs: the root move turned by the facing and added to the
// position; the turn added to the facing.
// FF3 0x0021e7f0

static void PlyrCameraMoveUpdate(PLYR_CAMERA_WRK *cam)
{
    float scale;
    int   i;

    if (static_cast<GameAnimeModel *>(cam->mdl)->IsAnimeEnd() != 0)
        return;

    cam->move[0] = -static_cast<CharModel *>(cam->mdl)->GetRootMove(0, 1);
    cam->move[1] = -static_cast<CharModel *>(cam->mdl)->GetRootMove(1, 1);
    cam->move[2] = static_cast<CharModel *>(cam->mdl)->GetRootMove(2, 1);
    scale = GetFrameTimeScale();
    for (i = 0; i < 4; i++)
        cam->move[i] *= scale;

    if (cam->turn[1] != 0.0f)
    {
        for (i = 0; i < 4; i++)
            cam->rot[i] += cam->turn[i];
        RotLimitChk(&cam->rot[1]);
    }
    RotVectorZXY(cam->move, cam->move, cam->rot);
    for (i = 0; i < 4; i++)
        cam->pos[i] += cam->move[i];
    for (i = 0; i < 4; i++)
    {
        cam->turn[i] = 0.0f;
        cam->move[i] = 0.0f;
    }
}

// The facial channels: a request starts on an idle channel (channel 0 then
// holds, 1 replays, 2 plays once); when the motion object allows a step,
// state 1 restarts with the blend flag, 0 replays, 2 stops the face.
// FF3 0x0021e910

static void PlyrCameraFaceUpdate(PLYR_CAMERA_WRK *cam)
{
    static const s_char state0[3] = { -1, 0, 2 };   // sdata 0x3716d0
    PLYR_CAMERA_COLOR *col = &cam->color;
    MDL_MOT           *mot;
    s_char             i;

    if (col->on == 0)
        return;

    mot = cam->mdl->mot;
    for (i = 0; i < 3; i++)
    {
        if (col->cur[i] == -1 && col->req[i] != -1)
        {
            mot->PlayShape(col->req[i], 0, 0, 0, 0.0f, 1.0f);
            col->state[i] = state0[i];
            col->cur[i]   = col->req[i];
            col->req[i]   = -1;
        }
        if (col->cur[i] == -1 || mot->IsShapeEnd(col->cur[i]) == 0)
            continue;

        switch (col->state[i])
        {
        case 1:
            mot->PlayShape(col->cur[i], 0, 0, 1, 0.0f, 1.0f);
            col->state[i] = 2;
            break;
        case 0:
            mot->PlayShape(col->cur[i], 0, 0, 0, 0.0f, 1.0f);
            break;
        case 2:
            mot->StopShape(col->cur[i]);
            col->cur[i]   = -1;
            col->state[i] = -1;
            break;
        }
    }
}

// Random faces: after 90 frames with blink on, one chance in a hundred per
// frame of face 1 (NPC 0) / 0 (NPC 1) on channel 2.
// callcheck: rand (PC: ReiPan_Rand, the ROM's generator)
// FF3 0x0021eae0

static void PlyrCameraBlinkUpdate(s_char no)
{
    static const s_char face[PLYR_CAMERA_NUM] = { 1, 0 };  // sdata 0x3716e0
    PLYR_CAMERA_WRK *cam = GetPlyrCameraWrk(no);
    short           *cnt = &plyr_camera_blink_cnt[no];
    short            n;

    if (cam->color.blink == 0)
        return;

    n = *cnt + 1;
    *cnt = (n < 0) ? 0 : (n > 90 ? 90 : n);
    if (*cnt >= 90 && ReiPan_Rand() % 100 + 1 < 2)
        PlyrCameraFaceReq(no, face[no], 2);
}

// The watch control looks at target (nullptr: the player's joint 15). With
// eye_mode set, a target outside the control's range is fed with the
// control off.
// FF3 0x0021ebb8

static void PlyrCameraWatchUpdate(PLYR_CAMERA_WRK *cam, float *target)
{
    CHAR_MDL_CTRL *ctrl = static_cast<CharNpcModel *>(cam->mdl)->ctrl;
    CHAR_WATCH_PRM prm =
    {
        { 0.0f, 0.0f, 0.0f, 1.0f },
        { 5.0f, 13.0f, 16.0f, 0.34906584f, 0.448798954f, 0.184799567f, 0.120830484f,
          0.628318548f, 0.314159274f, 0.628318548f, 0.073060289f, 0.0f },
    };
    sceVu0FVECTOR v;

    if (ctrl == nullptr)
        return;

    if (target == nullptr)
    {
        if (AllPlyrIsReady() != 0)
        {
            PlyrGetJointPos(0xf, v, nullptr);
            prm.pos[0] = v[0];
            prm.pos[1] = v[1];
            prm.pos[2] = v[2];
            prm.pos[3] = v[3];
        }
    }
    else
    {
        prm.pos[0] = target[0];
        prm.pos[1] = target[1];
        prm.pos[2] = target[2];
        prm.pos[3] = target[3];
    }

    if (cam->eye_mode != 0)
    {
        UnitGetTrgtRot(cam->pos, prm.pos, v, 2);
        v[1] -= cam->rot[1];
        RotLimitChk(&v[1]);
        if (ctrl->IsInRange(&prm, v[1]) == 0)
        {
            ctrl->off = 1;
            ctrl->Update(&prm);         // FF3: through the control's vtable +0x08
            ctrl->off = 0;
            return;
        }
    }
    ctrl->Update(&prm);                 // FF3: through the control's vtable +0x08
}

// FF3 0x0021ece0

static void PlyrCameraVisibleCheck(PLYR_CAMERA_WRK *cam)
{
    cam->visible = 1;
    if (cam->area == 0x31 || PlyrCameraAreaIsDrawn(cam->area) == 0)
        cam->visible = 0;
}

// Per frame for both NPCs: a loaded model starts (its motion and face, the
// turn callback, the watch target, the alg entry) and is ready; a ready NPC
// runs its faces, model, move, random faces, act block and watch.
// FF3 0x0021ed30

static void PlyrCameraUpdate(void)
{
    PLYR_CAMERA_WRK *cam;
    s_char           no;
    s_char           mot;

    for (no = 0; no < PLYR_CAMERA_NUM; no++)
    {
        cam = GetPlyrCameraWrk(no);
        if (cam->ready == 0)
        {
            if (cam->mdl == nullptr || cam->mdl->IsLoaded() == 0)   // FF3: vtable +0x10
                continue;
            if (PlyrCameraAcsIsReady(cam) != 0)
            {
                mot = cam->mot;
                cam->ready = 1;
                if (mot >= 0 && cam->color.req[0] >= 0)
                {
                    PlyrCameraMotPlay(no, mot, 0);
                    PlyrCameraFacialCall(no, cam->color.req[0]);
                }
                else
                {
                    if (mot < 0)
                        mot = 0;
                    PlyrCameraMotCall(no, mot, 0);
                }
                static_cast<CharModel *>(cam->mdl)->SetYawFunc(PlyrCameraMotTurnCB, cam);
                PlyrCameraAcsSetCoord(no);
                cam->mdl->SetTexFrame(0);
                PlyrCameraSetEyeTarget(no, cam->eye_on);
                CharDrawSetPlyrCamera(cam);
            }
            if (cam->ready == 0)
                continue;
        }

        PlyrCameraFaceUpdate(cam);
        cam->mdl->UpdateMatrix();
        PlyrCameraMoveUpdate(cam);
        PlyrCameraBlinkUpdate(no);
        PlyrCameraAcsUpdate(cam);
        PlyrCameraSeVolUpdate(no);
        PlyrCameraWatchUpdate(cam, cam->eye_pos_on != 0 ? cam->eye_pos : nullptr);
        if (cam->free != 0)
            PlyrCameraActStep(&cam->act);
        PlyrCameraVisibleCheck(cam);
    }
}

// ──────────────────────────────────────────────────────────────────────
// The NPC's model of type (-1: the work's type), when the NPC is active and
// has none. The facial request on channel 0 survives; a model with a shape
// file has faces. NPC 0's cat types link cat 0.
// FF3 0x0021eeb8
// callcheck: CharModelCtor (the inlined CharNpcModel constructor runs its base's)

int PlyrCameraMdlCreate(s_char no, s_char type, int arg)
{
    PLYR_CAMERA_WRK *cam = GetPlyrCameraWrk(no);
    CharNpcModel    *mdl;
    const short     *dat;
    const short     *file;
    s_char           face;

    if (cam->active == 0 || cam->mdl != nullptr)
        return 0;

    if (type < 0)
        type = cam->type;

    mdl = new (operator_new(sizeof(CharNpcModel))) CharNpcModel(cam);  // FF3: operator new(0x40), the constructor inlined
    cam->mdl = mdl;

    dat  = PlyrCameraGetMdlDat(no, type);
    file = PlyrCameraGetMdlFile(no, type);
    if (static_cast<CharModel *>(cam->mdl)->Load(dat[0], dat[1], file[0], dat[2], file[1]) == 0)
    {
        if (cam->mdl != nullptr)
            { cam->mdl->~GameModel(); operator_delete(cam->mdl); }     // FF3: delete through vtable +0x0c
        cam->mdl = nullptr;
        return 0;
    }

    if (cam->type != type)
    {
        cam->type_old = cam->type;
        cam->type     = type;
    }
    face = cam->color.req[0];
    PlyrCameraColorReset(&cam->color);
    cam->color.req[0] = face;
    if (file[1] != -1)
        cam->color.on = 1;
    PlyrCameraAcsCreate(no);
    PlyrCameraSeReset(no);
    cam->talk_flg = 0;
    if (arg != 0)
        PlyrCameraActReset(&cam->act, no, 0, no);

    if (no == 0 && PlyrCameraIsCatMdl(cam) != 0)
    {
        CatSetLabelLock(0, 2, 1);
        CatSetLabelLock(0, 4, 1);
        if (CatGetLabel(0) == 2 || CatGetLabel(0) == 4)
            CatActRestart(0);
    }
    return 1;
}

// Drops the NPC's model; channel 0's face is kept as the request for the
// next model.
// FF3 0x0021f0a0

int PlyrCameraMdlRelease(s_char no)
{
    PLYR_CAMERA_WRK *cam = GetPlyrCameraWrk(no);

    if (cam->mdl == nullptr)
        return 0;

    if (cam->ready != 0)
    {
        CharDrawDelete(cam);
        cam->ready = 0;
    }
    cam->color.req[0] = cam->color.cur[0];
    PlyrCameraAcsRelease(cam);
    PlyrCameraSeStop(no);
    cam->talk_flg = 0;
    if (cam->mdl != nullptr)
        { cam->mdl->~GameModel(); operator_delete(cam->mdl); }         // FF3: delete through vtable +0x0c
    cam->mdl = nullptr;

    if (no == 0 && PlyrCameraIsCatMdl(cam) != 0)
    {
        CatSetLabelLock(0, 2, 0);
        CatSetLabelLock(0, 4, 0);
    }
    return 1;
}

// NPC_MODEL_CHANGE: type -1 is the previous type.
// FF3 0x0021f170

int PlyrCameraMdlChange(s_char no, s_char type, int arg)
{
    PLYR_CAMERA_WRK *cam = GetPlyrCameraWrk(no);
    int              ret = 0;

    if (type < 0)
        type = cam->type_old;

    if (cam->mdl == nullptr || (type != -1 && cam->type != type))
    {
        if (cam->mdl != nullptr)
            PlyrCameraMdlRelease(no);
        ret = PlyrCameraMdlCreate(no, type, 0);
    }
    if (arg != 0)
        PlyrCameraActReset(&cam->act, no, 0, no);
    return ret;
}

// IngameInit.
// FF3 0x0021f240

void PlyrCameraInit(void)
{
    PlyrCameraHeadInit();
    PlyrCameraWrkInit(0);
    PlyrCameraWrkInit(1);
}

// IngameLoadEnd / IngameLoadRestore.
// FF3 0x0021f268

void PlyrCameraLoadRestore(void)
{
    PlyrCameraHeadLoadRestore();
    PlyrCameraWrkLoadRestore(0);
    PlyrCameraWrkLoadRestore(1);
}

// IngameNewGameReset.
// FF3 0x0021f290

void PlyrCameraReset(void)
{
    PlyrCameraHeadReset();
    PlyrCameraWrkReset(0);
    PlyrCameraWrkReset(1);
}

// IngameEnd.
// FF3 0x0021f2b8

void PlyrCameraEnd(void)
{
    PlyrCameraMdlRelease(1);
    PlyrCameraMdlRelease(0);
    PlyrCameraInit();
}

// IngameMain.
// FF3 0x0021f2e0

void PlyrCameraMain(void)
{
    PlyrCameraUpdate();
}

// The NPC is drawn: lock 0x19 off, active, ready and in a drawn room; with
// no_cat not with the cat model.
// FF3 0x0021f2f8

int PlyrCameraIsDrawn(PLYR_CAMERA_WRK *cam, int no_cat)
{
    if (IsLockTblOn(0x19) != 0 || cam == nullptr)
        return 0;
    if (cam->active == 0 || cam->ready == 0 || cam->visible == 0)
        return 0;
    if (no_cat == 0)
        return 1;
    return PlyrCameraIsCatMdl(cam) == 0;
}

// The alg-manage draw entry (0x148ca8).
// FF3 0x0021f380

void PlyrCameraDraw(PLYR_CAMERA_WRK *cam)
{
    if (PlyrCameraIsDrawn(cam, 0) == 0)
        return;
    PlyrCameraAcsDrawPre(cam);
    cam->mdl->DrawMdl();
    PlyrCameraAcsDrawPost(cam);
}

// FF3 0x0021f3d8

int PlyrCameraIsReady(s_char no)
{
    return GetPlyrCameraWrk(no)->ready;
}

// FF3 0x0021f400

int PlyrCameraIsActive(s_char no)
{
    return GetPlyrCameraWrk(no)->active;
}

// FF3 0x0021f428

void PlyrCameraSetActive(s_char no, int on)
{
    GetPlyrCameraWrk(no)->active = on;
}

// NPC_FIX_EVENT. Fixing keeps label and stops the act block (an NPC with a
// cat model changes back to its previous type). Releasing places the NPC at
// label (-1: the label kept while fixed, if any) and runs the act block
// again.
// FF3 0x0021f458

void PlyrCameraFixEvent(s_char no, int fix, s_char label)
{
    PLYR_CAMERA_WRK *cam = GetPlyrCameraWrk(no);

    if (fix == 0)
    {
        cam->_0ec    = 0;
        cam->free    = 1;
        cam->act._34 = 0;
        if (label == -1)
        {
            if (cam->fix_label == -1)
            {
                cam->free = -1;
                cam->_0ec = 1;
            }
            else
            {
                cam->act.label = (s_char)PlyrCameraSetPosLabel(no, (u_char)cam->fix_label);
            }
        }
        else
        {
            cam->act.label = label;
            cam->act._34   = 1;
        }
        cam->fix_label = -1;
    }
    else
    {
        cam->fix_label = label;
        cam->free      = 0;
        if (PlyrCameraIsCatMdl(cam) != 0)
            PlyrCameraMdlChange(no, -1, 0);
        PlyrCameraActReset(&cam->act, no, 0, no);
    }
    PlyrCameraSetEyeTarget(no, 0);
    PlyrCameraSetEyePos(no, nullptr, 0);
}

// no < 0: any NPC.
// FF3 0x0021f570

int PlyrCameraIsFixed(s_char no)
{
    s_char i;

    if (no < 0)
    {
        for (i = 0; i < PLYR_CAMERA_NUM; i++)
        {
            if (GetPlyrCameraWrk(i)->free == 0)
                return 1;
        }
        return 0;
    }
    return GetPlyrCameraWrk(no)->free == 0;
}

// NPC_SET_POS.
// FF3 0x0021f5e8

void PlyrCameraSetPos(s_char no, u_char area, float *pos, float rot)
{
    PLYR_CAMERA_WRK *cam = GetPlyrCameraWrk(no);

    cam->area   = area;
    cam->pos[0] = pos[0];
    cam->pos[1] = pos[1];
    cam->pos[2] = pos[2];
    cam->pos[3] = pos[3];
    cam->rot[1] = rot;
}

// Starts motion mot (kept for later without a model).
// FF3 0x0021f640

void PlyrCameraMotPlay(s_char no, s_char mot, int arg)
{
    PLYR_CAMERA_WRK *cam = GetPlyrCameraWrk(no);
    GameModel       *mdl = PlyrCameraGetMdl(no);
    int              i;

    if (mdl == nullptr)
    {
        cam->mot = mot;
        return;
    }

    static_cast<CharModel *>(mdl)->StartAnime((u_char)mot, (u_char)arg, 0, 1, 0.0f, 1.0f);    // FF3: vtable +0x40
    plyr_camera_turn_skip = (arg == 0);
    for (i = 0; i < 4; i++)
    {
        cam->turn[i] = 0.0f;
        cam->move[i] = 0.0f;
    }
    cam->mot = mot;
    PlyrCameraMotSe(no, mot);
    PlyrCameraAcsMotSet(no, mot);
}

// The motions that loop (NPC_MOTION_CALL never waits for them).
// FF3 0x0021f748

int PlyrCameraMotIsLoop(s_char no)
{
    PLYR_CAMERA_WRK *cam = GetPlyrCameraWrk(no);

    if (no == 0)
    {
        switch ((u_char)cam->mot)
        {
        case 0: case 1: case 10: case 11: case 16: case 17: case 18:
        case 28: case 29: case 36: case 37: case 40: case 41:
            return 1;
        }
    }
    if (no != 1)
        return 0;
    switch (cam->mot)
    {
    case 0: case 6: case 11: case 16:
        return 1;
    }
    return 0;
}

// FF3 0x0021f7f8

int PlyrCameraMotIsEnd(s_char no)
{
    GameModel *mdl;

    if (PlyrCameraMotIsLoop(no) != 0)
        return 1;
    mdl = PlyrCameraGetMdl(no);
    if (mdl == nullptr)
        return 0;
    return static_cast<GameAnimeModel *>(mdl)->IsAnimeEnd();
}

// FF3 0x0021f858

int PlyrCameraMotGetCtrl2(s_char no)
{
    GameModel *mdl = PlyrCameraGetMdl(no);

    if (mdl == nullptr)
        return 0;
    return static_cast<GameAnimeModel *>(mdl)->GetAnimeFlag(2);
}

// Face on channel ch: an idle channel takes it at once
// (PlyrCameraFaceUpdate); a playing one restarts (state 1) or stops first
// (2).
// FF3 0x0021f890

int PlyrCameraFaceReq(s_char no, s_char face, s_char ch)
{
    PLYR_CAMERA_COLOR *col = &GetPlyrCameraWrk(no)->color;

    if (col->on == 0)
        return 0;

    col->req[ch] = face;
    if (col->cur[ch] == -1 || col->cur[ch] == face)
        return 1;
    if (col->state[ch] == -1)
        col->state[ch] = 1;
    else if (col->state[ch] == 0)
        col->state[ch] = 2;
    return 1;
}

// NPC_FACIAL_CALL: the NPC's neutral face (and -1 while channel 1 plays)
// goes to channel 1, others to channel 0. The blink-off face clears channel
// 2's random faces; any other face turns them on.
// FF3 0x0021f928

void PlyrCameraFacialCall(s_char no, s_char face)
{
    static const s_char neutral[PLYR_CAMERA_NUM]   = { 5, 7 };          // sdata 0x3716e8
    static const s_char blink_off[PLYR_CAMERA_NUM] = { 0x18, 0x19 };    // sdata 0x3716f0
    PLYR_CAMERA_WRK *cam = GetPlyrCameraWrk(no);
    int              ch1 = neutral[no] == face;

    if (cam->color.cur[1] != -1 && face == -1)
        ch1 = 1;

    if (ch1 == 0)
    {
        if (face == blink_off[no])
        {
            if (cam->color.blink != 0)
            {
                cam->color.blink = 0;
                PlyrCameraFaceReq(no, -1, 2);
            }
        }
        else
        {
            cam->color.blink = 1;
        }
    }
    PlyrCameraFaceReq(no, face, (s_char)ch1);
}

// NPC_EYE_TARGET.
// FF3 0x0021fa10

void PlyrCameraSetEyeTarget(s_char no, int on)
{
    PLYR_CAMERA_WRK *cam = GetPlyrCameraWrk(no);
    GameModel       *mdl;

    cam->eye_on = on;
    mdl = PlyrCameraGetMdl(no);
    if (mdl != nullptr)
        static_cast<CharNpcModel *>(mdl)->ctrl->off = (on == 0);
}

// The point the NPC looks at (nullptr: the player).
// FF3 0x0021fa78

void PlyrCameraSetEyePos(s_char no, float *pos, int mode)
{
    PLYR_CAMERA_WRK *cam = GetPlyrCameraWrk(no);

    if (pos == nullptr)
    {
        cam->eye_pos_on = 0;
        cam->eye_pos[0] = 0.0f;
        cam->eye_pos[1] = 0.0f;
        cam->eye_pos[2] = 0.0f;
        cam->eye_pos[3] = 0.0f;
    }
    else
    {
        cam->eye_pos_on = 1;
        cam->eye_pos[0] = pos[0];
        cam->eye_pos[1] = pos[1];
        cam->eye_pos[2] = pos[2];
        cam->eye_pos[3] = pos[3];
    }
    cam->eye_mode = mode;
}

// menu_file.c 0x208320.
// FF3 0x0021fae8

void PlyrCameraSetEc(s_char no)
{
    GetPlyrCameraWrk(no)->_0ec = 1;
}

// 0x1b3748.
// FF3 0x0021fb10

void PlyrCameraSetEcSub(s_char no)
{
    PlyrCameraSetEc(no);
}

// FF3 0x0021fb30

void PlyrCameraSetSerifTbl(u_char tbl)
{
    PlyrCameraGetHead()->serif_tbl = tbl;
}

// The serif table of a talk kind: kind 0 has one per side of the world.
// FF3 0x0021fb60

s_char PlyrCameraGetSerifTbl(s_char kind)
{
    PLYR_CAMERA_HEAD *head = PlyrCameraGetHead();

    if (kind == 0)
        return (s_char)(EvNowSynchroMode() + (s_char)head->serif_tbl * 2);
    if (kind == 5)
        return 0;
    return (s_char)head->serif_tbl;
}

// FF3 0x0021fbd0

void PlyrCameraSetFacialTbl(u_char tbl)
{
    PlyrCameraGetHead()->facial_tbl = tbl;
}

// FF3 0x0021fc00

u_char PlyrCameraGetFacialTbl(void)
{
    return PlyrCameraGetHead()->facial_tbl;
}

// A joint's world position and, with scr, GameModel::GetCoordRot's x / y of its
// coordinate. 0 without a model.
// FF3 0x0021fc20

int PlyrCameraGetJointPos(s_char no, int joint, float *pos, float *scr)
{
    static float origin[4] = { 0.0f, 0.0f, 0.0f, 1.0f };   // data 0x329500
    GameModel   *mdl = PlyrCameraGetMdl(no);
    int          coord;

    if (mdl == nullptr)
        return 0;

    coord = mdl->GetJointCoord(joint);           // FF3: vtable +0x34
    mdl->GetCoordPos(pos, coord, origin);      // FF3: vtable +0x2c
    if (scr != nullptr)
    {
        scr[0] = mdl->GetCoordRot(coord, 0);
        scr[1] = mdl->GetCoordRot(coord, 1);
        scr[3] = 0.0f;
        scr[2] = 0.0f;
    }
    return 1;
}

// FF3 0x0021fcf0

int PlyrCameraGetHeadPos(s_char no, float *pos, float *scr)
{
    return PlyrCameraGetJointPos(no, 2, pos, scr);
}

// FF3 0x0021fd18

GRA3DEMULATIONLIGHTDATACREATIONDATA *PlyrCameraGetLightWrk(void)
{
    return &plyr_camera_light;
}

// The NPC's light: the area's map lights turned into a character set at
// pos, 0.9 more ambient, and light 2 as a directional light against the
// camera's direction in the NPC's colour (NPC 0: by the world side and the
// cat model; NPC 1: by the world side).
// CharNpcModel's vtable +0x20 (0x1459b8) calls it too.
// FF3 0x0021fd28

void PlyrCameraSetLight(s_char no, int area, float *pos)
{
    // rodata 0x35f710: NPC 0 {real, dream} then with the cat {real, dream};
    // rodata 0x35f750: NPC 1 {real, dream}.
    static const float col0[4][4] =
    {
        { 0.21f, 0.21f, 0.21f, 0.0f }, { 0.05f, 0.05f, 0.05f, 0.0f },
        { 0.21f, 0.21f, 0.21f, 0.0f }, { 0.21f, 0.21f, 0.21f, 0.0f },
    };
    static const float col1[2][4] =
    {
        { 0.25f, 0.25f, 0.25f, 0.0f }, { 0.25f, 0.25f, 0.25f, 0.0f },
    };
    static GRA3DLIGHTDATA ld;           // FF3: on the stack
    GRA3DLIGHTDATA *src;
    GRA3DEMULATIONLIGHTDATACREATIONDATA *prm;
    GRA3DCAMERA    *camera;
    const float    *col;
    sceVu0FVECTOR   rgb = { 0.0f, 0.0f, 0.0f, 0.0f };  // FF3: left unset for no > 1
    int             side;
    int             i;

    src = (GRA3DLIGHTDATA *)MapDrawGetLightPtr(area, 0);
    memcpy(&ld, src, sizeof(ld));
    prm = PlyrCameraGetLightWrk();
    gra3dGenerateLightDataToChar(&ld, src, prm);
    gra3dEmulateLightData(&ld, &ld, pos, prm->fMaplightScale);
    for (i = 0; i < 3; i++)
        ld.vAmbient[i] += 0.9f;         // sdata 0x3716f4

    side = EvNowSynchroMode();
    if (no == 0)
    {
        if (PlyrCameraIsCatMdl(GetPlyrCameraWrk(0)) != 0)
            col = col0[2 + side];
        else
            col = col0[side];
        for (i = 0; i < 4; i++)
            rgb[i] = col[i];
    }
    if (no == 1)
    {
        for (i = 0; i < 4; i++)
            rgb[i] = col1[side][i];
    }

    camera = gra3dGetCamera();
    for (i = 0; i < 4; i++)
    {
        ld.aLight[2].vDirection[i] = camera->matCoord[2][i] * -1.0f;
        ld.aLight[2].vDiffuse[i]   = rgb[i];
    }
    ld.aStatus[2].bEnable = 1;
    gra3dSetLightData(&ld, nullptr);
}

// Model types 3 and 4 (NPC 0 with the cat).
// FF3 0x0021fee0

int PlyrCameraIsCatMdl(PLYR_CAMERA_WRK *cam)
{
    return (u_int)(cam->type - 3) < 2;
}

// NPC_LOCK_POS.
// FF3 0x0021fef0

void PlyrCameraSetLockPos(s_char no, s_char n, int v)
{
    GetPlyrCameraWrk(no)->lock_pos[n] = v;
}

// ──────────────────────────────────────────────────────────────────────
// The act hooks.
// ──────────────────────────────────────────────────────────────────────

// The player may talk to the NPC: free to act, sees the NPC (within 40,
// sight 45), OK pressed.
// FF3 0x0021ff38

int PlyrCameraTalkCheck(s_char no)
{
    PLYR_WRK *plyr = ingame_wrk.plyr_wrk;

    if ((plyr->flags & 0x1c00000) != 0 || (plyr->flags2 & 0x3f800) != 0)
        return 0;
    if ((PlyrCameraSightCheck(no, 40.0f, 45.0f) & 2) == 0)     // sdata 0x371738 / 0x37173c
        return 0;
    return CheckPadTrgPlaySe((u_short)pad_key[PAD_KEY_OK], -1, 0) != 0;
}

// The NPC may talk: active, placed, lock 3 off, in phase 0x20, the player's
// word +0xe0 bit 1 clear.
// FF3 0x0021ffd0

int PlyrCameraTalkEnable(PLYR_CAMERA_ACT *act)
{
    if (PlyrCameraIsActive(act->no) == 0 || act->label < 0)
        return 0;
    if (IsLockTblCnt(3) != 0)
        return 0;
    // FF3 keeps GetGPhaseCtrl() in sdata 0x37284c.
    if (GetGPhaseCtrl()->leaf->id != 0x20)
        return 0;
    return (ingame_wrk.plyr_wrk->_0e0 & 2) == 0;
}

// FF3 0x00220060

void PlyrCameraTalkLock(void)
{
    ingame_wrk.plyr_wrk->_0e0 |= 8;
    LockTblInc(3);
    LockTblInc(0x13);
    LockTblInc(0xf);
    LockTblInc(0x10);
}

// FF3 0x002200a8

void PlyrCameraTalkUnlock(void)
{
    ingame_wrk.plyr_wrk->_0e0 &= ~8;
    LockTblDec(3);
    LockTblDec(0x13);
    LockTblDec(0xf);
    LockTblDec(0x10);
}

// FF3 0x002200f0

static int PlyrCameraActIs3c2(PLYR_CAMERA_ACT *act)
{
    return act->_3c == 2;
}

// Kind 2: the label moves to the one the area gives (only while +0x3c is
// 2) and the NPC is placed there. Kind 0 (by NPC and side: label 0x30 /
// 0x32) and the others pick a label.
// FF3 0x00220100

int PlyrCameraActSetLabel(PLYR_CAMERA_ACT *act, s_char kind)
{
    static const int kind0_arg[PLYR_CAMERA_NUM * 2] = { 0x30, 0x32, 0x30, 0x30 };  // FF3: built on the stack
    int    set = PlyrCameraActIs3c2(act);
    int    arg;
    s_char label;

    if (kind == 2)
    {
        if (act->label >= 0)
        {
            label = PlyrCameraAreaLabel(act->no, act->label, GetPlyrAreaNo());
            if (label == act->label || set == 0)
                label = act->label;
            else
                act->label = label;
            PlyrCameraSetPosLabel(act->no, (u_char)label);
        }
    }
    else
    {
        arg = -1;
        if (kind == 0)
        {
            set = 1;
            arg = kind0_arg[act->no * 2 + EvNowSynchroMode()];
        }
        if (set != 0)
            act->label = PlyrCameraSetRandLabel(act->no, arg, kind == 0);
    }
    return 1;
}

// One chance in four.
// FF3 0x00220218

static int PlyrCameraRand25(void)
{
    return GetRandValIByF(100) < 25;
}

// FF3 0x00220238

static void PlyrCameraChatStart(PLYR_CAMERA_ACT *act)
{
    PlyrCameraTalkFlgSet(act->no, 1);
    act->chat_step = 0;
    act->chat_fade = -1;
}

// The chat: a line from the NPC's set where it stands, under the effect-6
// fade (60 frames NTSC, 50 PAL); 1 when it is over.
// FF3 0x00220270

static int PlyrCameraChatStep(PLYR_CAMERA_ACT *act)
{
    s_char step;
    s_char cnt;
    int    line;

    if (act->label < 0)
        return 1;

    step = act->chat_step;
    if (step > 2)
    {
        if (act->chat_fade < 1)
        {
            if (act->chat_fade != -1 && act->chat_cnt > 0 && --act->chat_cnt == 0)
            {
                CutEffects(6);
                PlyrCameraTalkUnlock();
                PlyrCameraPlyrLookEnd();
                step = act->chat_step;
            }
        }
        else if (--act->chat_fade == 0)
        {
            PlyrCameraEvCamEnd();
            SetEffects(6, 2, 0x10);
            step = act->chat_step;
        }
    }

    switch (step)
    {
    case 0:
        act->chat_step = step + 1;
        // fall through
    case 1:
        act->chat_cnt = 0;
        act->chat_sel = PlyrCameraPickSerif(act->no, 5);
        line = PlyrCameraGetSerifCam(act->no, 5, act->chat_sel);
        if (line == -1 || PlyrCameraEvCamStart(act->no, 5, line, act->label) == 0)
        {
            PlyrCameraPlyrTurn(act->no, 0);
            act->chat_step = 3;
        }
        else
        {
            SetEffects(6, 2, 0x10);
            act->chat_fade = (GetPALMode() == 0) ? 0x3c : 0x32;
            PlyrCameraTalkLock();
            PlyrCameraPlyrTurn(act->no, 1);
            act->chat_step = 2;
        }
        return 0;

    case 2:
        if (act->chat_cnt <= 15)
        {
            act->chat_cnt++;
            return 0;
        }
        CutEffects(6);
        act->chat_step++;
        // fall through
    case 3:
        PlyrCameraSerifStart(act->no, 5, act->chat_sel, 0);
        if (act->anim >= 0)
            act->chat_fade = -1;
        act->chat_step++;
        // fall through
    case 4:
        if (PlyrCameraSerifIsEnd(act->no) == 0)
            return 0;
        if (act->chat_cnt < 1)
        {
            PlyrCameraPlyrLookEnd();
        }
        else if (act->anim >= 0)
        {
            act->anim = -1;
            PlyrCameraTalkUnlock();
            PlyrCameraPlyrLookEnd();
        }
        cnt = act->chat_cnt;
        act->chat_step++;
        break;

    case 5:
        cnt = act->chat_cnt;
        break;

    default:
        PlyrCameraTalkFlgClr(act->no, 1);
        PlyrCameraPlyrLookEnd();
        return 1;
    }

    if (cnt >= 1)
        return 0;
    act->chat_step++;
    PlyrCameraTalkFlgClr(act->no, 1);
    PlyrCameraPlyrLookEnd();
    return 1;
}

// FF3 0x00220510

void PlyrCameraIdleInit(PLYR_CAMERA_ACT *act)
{
    act->anim = -1;
    act->_34  = 0;
    act->step = 0;
    PlyrCameraSetEyeTarget(act->no, 0);
    PlyrCameraSetEyePos(act->no, nullptr, 0);
}

// Frames to the next idle motion: 60 + up to 299, kept in a byte as FF3
// does.
static void PlyrCameraIdleWait(PLYR_CAMERA_ACT *act)
{
    act->idle_cnt  = (u_char)(GetRandValIByF(300) + 60);
    act->idle_play = 0;
}

// The idle hook. Step 1 waits, playing an idle motion now and then; when
// the player comes close the NPC turns to look (2, 3, 4: a motion, the
// look while the player stays, a motion back); OK near the NPC opens the
// talk window (6, message 0x71) or, one time in four, the chat (5).
// FF3 0x00220558

void PlyrCameraIdleStep(PLYR_CAMERA_ACT *act)
{
    u_char step;
    int    done;

    if (act->label < 0)
        return;

    step = act->step;
    if (step == 0)
    {
        PlyrCameraLabelMotPlay(act->no, 0, act->label, 0);
        PlyrCameraIdleWait(act);
        act->step = step = 1;
    }

    if (step == 1)
    {
        if (act->idle_play != 0)
        {
            if (PlyrCameraMotIsEnd(act->no) != 0)
            {
                PlyrCameraLabelMotPlay(act->no, 0, act->label, 5);
                PlyrCameraIdleWait(act);
            }
        }
        else if (act->idle_cnt == 0)
        {
            if (PlyrCameraLabelMotPlay(act->no, 1, act->label, 5) != -1)
                act->idle_play = 1;
            else
                act->idle_cnt = (u_char)(GetRandValIByF(300) + 60);
        }
        else
        {
            act->idle_cnt--;
            if (PlyrCameraIsActive(act->no) == 0)
                return;
            if (PlyrCameraGetLabel84(act->no, act->label) == 0)
            {
                if (PlyrCameraLabelDoorCheck(act->no, 3, act->label) != 0)
                {
                    act->look_cnt = 0x3c;
                    act->talk_mot = (s_char)PlyrCameraLabelMotPlay(act->no, 4, act->label, 10);
                    PlyrCameraSetEyeTarget(act->no, 1);
                    PlyrCameraSetEyePos(act->no, nullptr, act->no == 0 && act->talk_mot == 0x26);
                    act->step = 2;
                }
            }
            else if (PlyrCameraTalkCheck(act->no) != 0 && PlyrCameraTalkEnable(act) != 0)
            {
                if (PlyrCameraRand25() != 0)
                {
                    PlyrCameraChatStart(act);
                    act->step = 5;
                }
                else
                {
                    act->anim = LayoutAnimStart(5, 0);
                    Anim2dMsgSetType(2);
                    Anim2dMsgSet(0x71, act->no);
                    PlyrCameraTalkFlgSet(act->no, 1);
                    act->step = 6;
                    PlyrCameraTalkLock();
                    PlyrCameraPlyrTurn(act->no, 0);
                }
            }
        }
        step = act->step;
    }

    if (step == 5)
    {
        if (PlyrCameraChatStep(act) != 0)
            act->step = 1;
        step = act->step;
    }

    if (step == 6)
    {
        if (LayoutAnimGetState(act->anim) == 4)
        {
            LayoutAnimReset(act->anim);
            act->anim = -1;
            PlyrCameraTalkFlgClr(act->no, 1);
            act->step = 1;
            PlyrCameraTalkUnlock();
            PlyrCameraPlyrLookEnd();
        }
        step = act->step;
    }

    if (step == 2)
    {
        if (act->talk_mot < 0)
            return;
        if (act->no == 0 && act->talk_mot == 0x26)
        {
            if (PlyrCameraMotIsEnd(0) != 0)
            {
                PlyrCameraNpc0Mot16(act->no);
                act->step = 3;
            }
        }
        else if (PlyrCameraMotGetCtrl2(act->no) != 0)
        {
            act->step = 3;
        }
        step = act->step;
    }

    if (step == 3)
    {
        done = 0;
        if (PlyrCameraLabelDoorCheck(act->no, 3, act->label) != 0)
            act->look_cnt = 0x3c;
        else if (act->look_cnt == 0)
            done = 1;
        else
            act->look_cnt--;

        if (done)
        {
            act->talk_mot = (s_char)PlyrCameraLabelMotPlay(act->no, 5, act->label, 5);
            if (act->talk_mot < 0)
            {
                act->_34 = 1;
            }
            else
            {
                PlyrCameraSetEyeTarget(act->no, 0);
                PlyrCameraSetEyePos(act->no, nullptr, 0);
                act->step = 4;
            }
        }
        step = act->step;
    }

    if (step == 4)
    {
        if (act->talk_mot >= 0 && PlyrCameraMotIsEnd(act->no) == 0)
            return;
        PlyrCameraLabelMotPlay(act->no, 0, act->label, 5);
        PlyrCameraIdleWait(act);
        act->step = 1;
    }
}

// FF3 0x002209e8

void PlyrCameraWatchInit(PLYR_CAMERA_ACT *act)
{
    act->step = 1;
    PlyrCameraSetEyeTarget(act->no, 0);
    PlyrCameraSetEyePos(act->no, nullptr, 0);
}

// The watch hook: looks at the player while within 40 (sdata 0x371740) and
// 60 frames after.
// FF3 0x00220a28

void PlyrCameraWatchStep(PLYR_CAMERA_ACT *act)
{
    u_char step = act->step;
    int    done;

    if (step == 1)
    {
        if (PlyrCameraIsActive(act->no) == 0)
            return;
        if (PlyrCameraDistCheck(act->no, 2, 40.0f) != 0)
        {
            act->look_cnt = 0x3c;
            PlyrCameraSetEyeTarget(act->no, 1);
            PlyrCameraSetEyePos(act->no, nullptr, 0);
            act->step = 3;
        }
        step = act->step;
    }

    if (step == 3)
    {
        done = 0;
        if (PlyrCameraDistCheck(act->no, 2, 40.0f) != 0)
            act->look_cnt = 0x3c;
        else if (act->look_cnt == 0)
            done = 1;
        else
            act->look_cnt--;

        if (done)
        {
            PlyrCameraSetEyeTarget(act->no, 0);
            PlyrCameraSetEyePos(act->no, nullptr, 0);
            act->step = 1;
        }
    }
}

// Watching (step 3); with near, also the NPC sees the player (within 40,
// sight 45; sdata 0x371744 / 0x371748).
// FF3 0x00220b48

int PlyrCameraIsWatching(PLYR_CAMERA_ACT *act, int near)
{
    if (act->step != 3)
        return 0;
    if (near == 0)
        return 1;
    return (PlyrCameraSightCheck(act->no, 40.0f, 45.0f) & 1) != 0;
}

// The greet: the NPC may talk and the player is within its kind-0 range.
// FF3 0x00220bb0

int PlyrCameraGreetCheck(PLYR_CAMERA_ACT *act)
{
    if (PlyrCameraTalkEnable(act) == 0)
        return 0;
    return PlyrCameraLabelDoorCheck(act->no, 0, act->label) != 0;
}

// The greet's act condition: +0x3c becomes 1 the first time the player is
// in range; the greet step leaves 2.
// FF3 0x00220bf8

int PlyrCameraGreetCond(PLYR_CAMERA_ACT *act)
{
    int    in = PlyrCameraGreetCheck(act);
    u_char st = act->_3c;

    if (in != 0 && st == 0)
    {
        st = 1;
        act->_3c = 1;
    }
    return (s_char)st;
}

// FF3 0x00220c48

void PlyrCameraGreetInit(PLYR_CAMERA_ACT *act)
{
    PlyrCameraTalkFlgSet(act->no, 1);
    if ((ingame_wrk.plyr_wrk->flags & PLYR_FLG_HOLD) != 0)
        PlyrFinderModeEnd();
    act->chat_step = 0;
}

// The greet: once the player is out of finder mode, the NPC's kind-0 line
// (under the effect-6 fade while it plays), the look motion (kind 4), the
// line shown; then, with nothing queued for the report, the motion back
// (kind 5, step 5); with a report queued, the report's motion (kind 5,
// step 7) or the place's other motion (kind 2: steps 9 / 6 when the label
// has none). Ends with +0x3c = 2; 1 when over.
// FF3 0x00220ca0

int PlyrCameraGreetStep(PLYR_CAMERA_ACT *act)
{
    s_char label = act->label;
    s_char sel;
    int    line;
    u_char next;

    if (label < 0)
        return 1;

    switch (act->chat_step)
    {
    case 0:
        if (PlyrIsFinderModeOff() == 0)
            return 0;
        PlyrCameraTalkLock();
        act->chat_cnt = 0;
        sel = (s_char)PlyrCameraPickSerif(act->no, 0);
        act->chat_sel = (u_char)sel;
        line = PlyrCameraGetSerifCam(act->no, 0, (u_char)sel);
        if (line != -1 && PlyrCameraEvCamStart(act->no, 0, line, act->label) != 0)
        {
            SetEffects(6, 2, 0x10);
            act->chat_step = 1;
            PlyrCameraPlyrTurn(act->no, 1);
            return 0;
        }
        PlyrCameraPlyrTurn(act->no, 0);
        act->chat_step = 2;
        return 0;

    case 1:
        if (act->chat_cnt < 16)
        {
            act->chat_cnt++;
            return 0;
        }
        CutEffects(6);
        label = act->label;
        act->chat_step = 2;
        // fall through
    case 2:
        act->talk_mot = (s_char)PlyrCameraLabelMotPlay(act->no, 4, label, 10);
        PlyrCameraSetEyeTarget(act->no, 1);
        PlyrCameraSetEyePos(act->no, nullptr, act->no == 0 && act->talk_mot == 0x26);
        act->chat_step = 3;
        // fall through
    case 3:
        if (act->talk_mot >= 0)
        {
            if (act->no == 0 && act->talk_mot == 0x26)
            {
                if (PlyrCameraMotIsEnd(0) == 0)
                    return 0;
                PlyrCameraNpc0Mot16(act->no);
            }
            else if (PlyrCameraMotGetCtrl2(act->no) == 0)
            {
                return 0;
            }
        }
        PlyrCameraTalkFlgSet(act->no, 2);
        PlyrCameraSerifStart(act->no, 0, (s_char)act->chat_sel, 1);
        act->chat_step = 4;
        // fall through
    case 4:
        if (PlyrCameraSerifIsEnd(act->no) == 0)
            return 0;
        PlyrCameraTalkFlgClr(act->no, 2);
        PlyrCameraSetEyeTarget(act->no, 0);
        PlyrCameraSetEyePos(act->no, nullptr, 0);
        act->talk_mot = -1;
        if (PlyrCameraQueue0Ready(act->no) == 0)
        {
            if (act->chat_cnt > 0)
            {
                PlyrCameraEvCamEnd();
                SetEffects(6, 2, 0x10);
            }
            act->talk_mot = (s_char)PlyrCameraLabelMotPlay(act->no, 5, act->label, 5);
            next = 5;
        }
        else if (PlyrCameraGetLabelMot2(act->no, act->label) < 0)
        {
            if (act->chat_cnt > 0)
            {
                PlyrCameraEvCamEnd();
                SetEffects(6, 2, 0x10);
            }
            PlyrCameraTalkUnlock();
            PlyrCameraPlyrLookEnd();
            next = 9;
        }
        else
        {
            act->talk_mot = (s_char)PlyrCameraLabelMotPlay(act->no, 5, act->label, 5);
            next = 7;
            if (act->talk_mot < 0)
            {
                if (act->chat_cnt > 0)
                {
                    PlyrCameraEvCamEnd();
                    SetEffects(6, 2, 0x10);
                }
                PlyrCameraSetPosLabel(act->no, (u_char)act->label);
                PlyrCameraLabelMotPlay(act->no, 0, act->label, 0);
                next = 6;
            }
        }
        act->chat_step = next;
        if (act->talk_mot < 0)
            act->_34 = 1;
        return 0;

    case 5:
        if (act->chat_cnt > 0 && --act->chat_cnt == 0)
            CutEffects(6);
        if (act->talk_mot >= 0 && PlyrCameraMotIsEnd(act->no) == 0)
            return 0;
        PlyrCameraTalkUnlock();
        PlyrCameraPlyrLookEnd();
        act->chat_step = 10;
        return 0;

    case 6:
        act->mot_on = 0;
        if (PlyrCameraLabelMotPlay(act->no, 2, label, 10) != -1)
        {
            act->mot_on = 1;
            act->_34    = 1;
        }
        PlyrCameraTalkUnlock();
        PlyrCameraPlyrLookEnd();
        act->chat_step = 9;
        return 0;

    case 7:
        if (act->talk_mot >= 0)
        {
            if (PlyrCameraMotIsEnd(act->no) == 0)
                return 0;
            label = act->label;
        }
        act->mot_on    = 0;
        act->chat_fade = 0;
        if (PlyrCameraLabelMotPlay(act->no, 2, label, 10) != -1)
        {
            act->mot_on    = 1;
            act->_34       = 1;
            act->chat_fade = (GetPALMode() == 0) ? 0x3c : 0x32;
        }
        act->chat_step = 8;
        // fall through
    case 8:
        if (act->chat_fade > 0 && --act->chat_fade != 0)
            return 0;
        if (act->chat_cnt > 0)
        {
            PlyrCameraEvCamEnd();
            SetEffects(6, 2, 0x10);
        }
        PlyrCameraTalkUnlock();
        PlyrCameraPlyrLookEnd();
        act->chat_step = 9;
        // fall through
    case 9:
        if (act->chat_cnt > 0 && --act->chat_cnt == 0)
            CutEffects(6);
        if (act->mot_on != 0)
        {
            if (PlyrCameraMotIsEnd(act->no) == 0)
                return 0;
            PlyrCameraNpc0Mot16(act->no);
        }
        act->chat_step = 10;
        // fall through
    case 10:
        if (act->chat_cnt > 0)
        {
            if (--act->chat_cnt != 0)
                return 0;
            CutEffects(6);
        }
        act->chat_step = 11;
        // fall through
    default:
        PlyrCameraTalkFlgClr(act->no, 1);
        PlyrCameraPlyrLookEnd();
        act->_3c = 2;
        return 1;
    }
}

// NPC 0 has an event result of kind 0 queued (EvQueueGetNext >= 0).
// FF3 0x00221270

int PlyrCameraQueue0Ready(s_char no)
{
    if (no != 0)
        return 0;
    return EvQueueGetNext(0) >= 0;
}

// FF3 0x002212b0

int PlyrCameraReportCheck(PLYR_CAMERA_ACT *act)
{
    if (PlyrCameraTalkEnable(act) == 0)
        return 0;
    if (PlyrCameraQueue0Ready(act->no) == 0)
        return 0;
    return PlyrCameraLabelDoorCheck(act->no, 1, act->label) != 0;
}

// FF3 0x00221308

void PlyrCameraReportInit(PLYR_CAMERA_ACT *act)
{
    PlyrCameraTalkLock();
    PlyrCameraTalkFlgSet(act->no, 1);
    if ((ingame_wrk.plyr_wrk->flags & PLYR_FLG_HOLD) != 0)
        PlyrFinderModeEnd();
    act->chat_step = 0;
}

// The report: the NPC turns to the player, its kind-1 line (under the
// effect-6 fade with the player's talk anime), then the event results of
// kind 0 (EvQueueStart / EvQueueMain until none is left), the line after
// them, and the place's row: 0 fades out, moves the NPC (PlyrCameraLabelMove) and
// fades in with the kind-3 motion; otherwise (1: moved first) the kind-3
// motion, or kind 5 when the row has none. 1 when over.
// FF3 0x00221360

int PlyrCameraReportStep(PLYR_CAMERA_ACT *act)
{
    u_char row;

    switch (act->chat_step)
    {
    case 0:
        if (PlyrIsFinderModeOff() == 0)
            return 0;
        act->chat_step++;
        // fall through
    case 1:
        PlyrCameraTalkFlgSet(act->no, 2);
        PlyrCameraSetEyeTarget(act->no, 1);
        PlyrCameraSetEyePos(act->no, nullptr, 0);
        act->chat_cnt = 0;
        if (PlyrCameraEvCamStart(act->no, 1, -2, act->label) == 0)
        {
            act->chat_step = 3;
        }
        else
        {
            SetEffects(6, 2, 0x10);
            PlyrCameraPlyrToLabel(act->no, 0, act->label);
            PlyrCameraPlyrTurn(act->no, 0);
            act->chat_step = 2;
        }
        return 0;

    case 2:
        if (act->chat_cnt < 16)
        {
            act->chat_cnt++;
            return 0;
        }
        CutEffects(6);
        act->chat_step++;
        // fall through
    case 3:
        act->chat_sel = (u_char)PlyrCameraSerifStart(act->no, 1, -2, 1);
        act->chat_step++;
        // fall through
    case 4:
        if (PlyrCameraSerifIsEnd(act->no) == 0)
            return 0;
        act->chat_step++;
        // fall through
    case 5:
        EvQueueStart(0);
        act->chat_step++;
        // fall through
    case 6:
        if (EvQueueMain() >= 0)
            return 0;
        act->chat_step++;
        // fall through
    case 7:
        PlyrCameraSerifStart2(act->no, 1, (s_char)act->chat_sel);
        act->chat_step++;
        // fall through
    case 8:
        if (PlyrCameraSerifIsEnd(act->no) == 0)
            return 0;
        act->chat_step++;
        // fall through
    case 9:
        PlyrCameraTalkFlgClr(act->no, 2);
        PlyrCameraPlyrLookEnd();
        PlyrCameraSetEyeTarget(act->no, 0);
        PlyrCameraSetEyePos(act->no, nullptr, 0);
        row = PlyrCameraGetLabel60(act->no, act->label);
        if (row == 0)
        {
            FadeOutReq(0, 0, 0, 0x10);
            act->chat_step = 10;
            return 0;
        }
        if (row == 1)
            PlyrCameraLabelMove(act->no, act->label);
        act->mot_on = 0;
        if (PlyrCameraGetLabelMot2(act->no, act->label) < 0)
        {
            if (act->_34 != 0 && PlyrCameraLabelMotPlay(act->no, 5, act->label, 5) >= 0)
            {
                act->_34    = 0;
                act->mot_on = 1;
            }
        }
        else if (PlyrCameraLabelMotPlay(act->no, 3, act->label, 5) != -1)
        {
            act->_34    = 0;
            act->mot_on = 1;
        }
        if (act->chat_cnt < 1)
        {
            act->chat_step = 13;
        }
        else
        {
            PlyrCameraEvCamEnd();
            SetEffects(6, 2, 0x10);
            act->chat_step = 12;
        }
        return 0;

    case 10:
        if (GetFadeState() == 0)
            return 0;
        PlyrCameraLabelMove(act->no, act->label);
        act->mot_on = 0;
        if (PlyrCameraLabelMotPlay(act->no, 3, act->label, 0) != -1)
        {
            act->_34    = 0;
            act->mot_on = 1;
        }
        PlyrCameraEvCamEnd();
        FadeInReq(0, 0, 0, 0x10);
        act->chat_step++;
        // fall through
    case 11:
        if (GetFadeState() == 0)
            return 0;
        act->chat_step = 13;
        return 0;

    case 12:
        if (act->chat_cnt > 0)
        {
            act->chat_cnt--;
            return 0;
        }
        CutEffects(6);
        act->chat_step++;
        // fall through
    case 13:
        if (act->mot_on != 0)
        {
            if (PlyrCameraMotIsEnd(act->no) == 0)
                return 0;
            PlyrCameraLabelMotPlay(act->no, 0, act->label, 1);
        }
        act->chat_step++;
        return 0;

    default:
        PlyrCameraTalkUnlock();
        PlyrCameraTalkFlgClr(act->no, 1);
        PlyrCameraPlyrLookEnd();
        return 1;
    }
}

// NPC 0 has an event result of kind 1 queued.
// FF3 0x00221740

int PlyrCameraQueue1Ready(s_char no)
{
    if (no != 0)
        return 0;
    return EvQueueGetNext(1) >= 0;
}

// FF3 0x00221780

int PlyrCameraReport1Check(PLYR_CAMERA_ACT *act)
{
    if (PlyrCameraTalkEnable(act) == 0)
        return 0;
    if (PlyrCameraQueue1Ready(act->no) == 0)
        return 0;
    if (PlyrCameraIsWatching(act, 0) == 0)
        return 0;
    return PlyrCameraTalkCheck(act->no) != 0;
}

// FF3 0x002217e8

void PlyrCameraReport1Init(PLYR_CAMERA_ACT *act)
{
    PlyrCameraTalkLock();
    PlyrCameraTalkFlgSet(act->no, 1);
    act->chat_step = 0;
}

// The kind-1 report (the player talked to the watching NPC): its kind-2
// line under the effect-6 fade, the event results of kind 1, the line
// after them. 1 when over.
// FF3 0x00221820

int PlyrCameraReport1Step(PLYR_CAMERA_ACT *act)
{
    if (act->label < 0)
        return 1;

    switch ((s_char)act->chat_step)
    {
    case 0:
        act->chat_step++;
        // fall through
    case 1:
        PlyrCameraTalkFlgSet(act->no, 2);
        act->chat_cnt = 0;
        if (PlyrCameraEvCamStart(act->no, 2, -2, act->label) == 0)
        {
            act->chat_step = 3;
        }
        else
        {
            SetEffects(6, 2, 0x10);
            PlyrCameraPlyrToLabel(act->no, 1, act->label);
            PlyrCameraPlyrTurn(act->no, 0);
            act->chat_step = 2;
        }
        return 0;

    case 2:
        if (act->chat_cnt <= 15)
        {
            act->chat_cnt++;
            return 0;
        }
        CutEffects(6);
        act->chat_step++;
        // fall through
    case 3:
        act->chat_sel = (u_char)PlyrCameraSerifStart(act->no, 2, -2, 1);
        act->chat_step++;
        // fall through
    case 4:
        if (PlyrCameraSerifIsEnd(act->no) == 0)
            return 0;
        act->chat_step++;
        // fall through
    case 5:
        EvQueueStart(1);
        act->chat_step++;
        // fall through
    case 6:
        if (EvQueueMain() >= 0)
            return 0;
        act->chat_step++;
        // fall through
    case 7:
        PlyrCameraSerifStart2(act->no, 2, (s_char)act->chat_sel);
        act->chat_step++;
        // fall through
    case 8:
        if (PlyrCameraSerifIsEnd(act->no) == 0)
            return 0;
        act->chat_step++;
        // fall through
    case 9:
        PlyrCameraTalkFlgClr(act->no, 2);
        PlyrCameraPlyrLookEnd();
        if (act->chat_cnt < 1)
        {
            act->chat_step = 11;
        }
        else
        {
            PlyrCameraEvCamEnd();
            SetEffects(6, 2, 0x10);
            act->chat_step = 10;
        }
        return 0;

    case 10:
        if (act->chat_cnt >= 1)
        {
            act->chat_cnt--;
            return 0;
        }
        CutEffects(6);
        act->chat_step++;
        // fall through
    default:
        PlyrCameraTalkUnlock();
        PlyrCameraTalkFlgClr(act->no, 1);
        PlyrCameraPlyrLookEnd();
        return 1;
    }
}

// The NPC speaks up once while it watches the player (one chance in four
// per check).
// FF3 0x00221a50

int PlyrCameraChat3Check(PLYR_CAMERA_ACT *act)
{
    if (PlyrCameraTalkEnable(act) == 0 || act->chat3_done != 0)
        return 0;
    if (PlyrCameraIsWatching(act, 1) == 0)
        return 0;
    if (GetRandValIByF(100) >= 25)
        return 0;
    act->chat3_done = 1;
    return 1;
}

// FF3 0x00221ac8

void PlyrCameraChat3Init(PLYR_CAMERA_ACT *act)
{
    PlyrCameraTalkFlgSet(act->no, 1);
    if ((ingame_wrk.plyr_wrk->flags & PLYR_FLG_HOLD) != 0)
        PlyrFinderModeEnd();
    act->chat_step = 0;
    act->chat_fade = -1;
}

// The kind-3 chat: PlyrCameraChatStep's pattern (the effect-6 fade after
// 60 / 50 frames, the talk window's lock) with the talk kind 2 around it.
// FF3 0x00221b28

int PlyrCameraChat3Step(PLYR_CAMERA_ACT *act)
{
    s_char step;
    int    line;

    if (act->label < 0)
        return 1;

    step = (s_char)act->chat_step;
    if (step > 2)
    {
        if (act->chat_fade < 1)
        {
            if (act->chat_fade != -1 && act->chat_cnt > 0 && --act->chat_cnt == 0)
            {
                CutEffects(6);
                PlyrCameraTalkUnlock();
                PlyrCameraPlyrLookEnd();
                step = (s_char)act->chat_step;
            }
        }
        else if (--act->chat_fade == 0)
        {
            PlyrCameraEvCamEnd();
            SetEffects(6, 2, 0x10);
            step = (s_char)act->chat_step;
        }
    }

    switch (step)
    {
    case 0:
        if (PlyrIsFinderModeOff() == 0)
            return 0;
        act->chat_step++;
        // fall through
    case 1:
        PlyrCameraTalkFlgSet(act->no, 2);
        act->chat_cnt = 0;
        act->chat_sel = PlyrCameraPickSerif(act->no, 3);
        line = PlyrCameraGetSerifCam(act->no, 3, act->chat_sel);
        if (line == -1 || PlyrCameraEvCamStart(act->no, 3, line, act->label) == 0)
        {
            PlyrCameraPlyrTurn(act->no, 0);
            act->chat_step = 3;
        }
        else
        {
            SetEffects(6, 2, 0x10);
            act->chat_fade = (GetPALMode() == 0) ? 0x3c : 0x32;
            PlyrCameraTalkLock();
            PlyrCameraPlyrTurn(act->no, 1);
            act->chat_step = 2;
        }
        return 0;

    case 2:
        if (act->chat_cnt <= 15)
        {
            act->chat_cnt++;
            return 0;
        }
        CutEffects(6);
        act->chat_step++;
        // fall through
    case 3:
        PlyrCameraSerifStart(act->no, 3, (s_char)act->chat_sel, 1);
        if (act->anim >= 0)
            act->chat_fade = -1;
        act->chat_step++;
        // fall through
    case 4:
        if (PlyrCameraSerifIsEnd(act->no) == 0)
            return 0;
        if (act->chat_cnt < 1)
        {
            PlyrCameraPlyrLookEnd();
        }
        else if (act->anim >= 0)
        {
            act->anim = -1;
            PlyrCameraTalkUnlock();
            PlyrCameraPlyrLookEnd();
        }
        act->chat_step++;
        // fall through
    case 5:
        PlyrCameraTalkFlgClr(act->no, 2);
        act->chat_step++;
        // fall through
    case 6:
        if (act->chat_cnt >= 1)
            return 0;
        act->chat_step++;
        // fall through
    default:
        PlyrCameraTalkFlgClr(act->no, 1);
        PlyrCameraPlyrLookEnd();
        return 1;
    }
}

// OK at the watching NPC with nothing queued for kind 1.
// FF3 0x00221de8

int PlyrCameraChat4Check(PLYR_CAMERA_ACT *act)
{
    if (PlyrCameraTalkEnable(act) == 0)
        return 0;
    if (PlyrCameraQueue1Ready(act->no) != 0)
        return 0;
    if (PlyrCameraIsWatching(act, 0) == 0)
        return 0;
    return PlyrCameraTalkCheck(act->no) != 0;
}

// (FF2's name FrameReset was a low match.)
// FF3 0x00221e58

void PlyrCameraChat4Init(PLYR_CAMERA_ACT *act)
{
    PlyrCameraTalkLock();
    PlyrCameraTalkFlgSet(act->no, 1);
    act->chat_step = 0;
}

// The kind-4 chat: a line of the set under the effect-6 fade. 1 when over.
// FF3 0x00221e90

int PlyrCameraChat4Step(PLYR_CAMERA_ACT *act)
{
    int line;

    if (act->label < 0)
        return 1;

    switch ((s_char)act->chat_step)
    {
    case 0:
        act->chat_step++;
        // fall through
    case 1:
        PlyrCameraTalkFlgSet(act->no, 2);
        act->chat_cnt = 0;
        act->chat_sel = PlyrCameraPickSerif(act->no, 4);
        line = PlyrCameraGetSerifCam(act->no, 4, act->chat_sel);
        if (line == -1 || PlyrCameraEvCamStart(act->no, 4, line, act->label) == 0)
        {
            PlyrCameraPlyrTurn(act->no, 0);
            act->chat_step = 3;
        }
        else
        {
            SetEffects(6, 2, 0x10);
            PlyrCameraPlyrTurn(act->no, 1);
            act->chat_step = 2;
        }
        return 0;

    case 2:
        if (act->chat_cnt <= 15)
        {
            act->chat_cnt++;
            return 0;
        }
        CutEffects(6);
        act->chat_step++;
        // fall through
    case 3:
        PlyrCameraSerifStart(act->no, 4, (s_char)act->chat_sel, 1);
        act->chat_step++;
        // fall through
    case 4:
        if (PlyrCameraSerifIsEnd(act->no) == 0)
            return 0;
        act->chat_step++;
        // fall through
    case 5:
        PlyrCameraTalkFlgClr(act->no, 2);
        PlyrCameraPlyrLookEnd();
        if (act->chat_cnt < 1)
        {
            act->chat_step = 7;
        }
        else
        {
            PlyrCameraEvCamEnd();
            SetEffects(6, 2, 0x10);
            act->chat_step = 6;
        }
        return 0;

    case 6:
        if (act->chat_cnt >= 1)
        {
            act->chat_cnt--;
            return 0;
        }
        CutEffects(6);
        act->chat_step++;
        // fall through
    default:
        PlyrCameraTalkUnlock();
        PlyrCameraTalkFlgClr(act->no, 1);
        PlyrCameraPlyrLookEnd();
        return 1;
    }
}

// FF3 0x00222080

void PlyrCameraGreetMotInit(PLYR_CAMERA_ACT *act)
{
    act->chat_step = 0;
}

// The greet motion: notes the player in range (+0x3c), plays the label's
// kind-6 motion and the kind-0 one after it. 1 when over.
// FF3 0x00222088

int PlyrCameraGreetMotStep(PLYR_CAMERA_ACT *act)
{
    s_char step;

    if (act->label < 0)
        return 1;

    if (PlyrCameraGreetCheck(act) != 0 && act->_3c == 0)
        act->_3c = 1;

    step = (s_char)act->chat_step;
    if (step == 0)
    {
        step = (PlyrCameraLabelMotPlay(act->no, 6, act->label, 0) == -1) ? 2 : 1;
    }
    else
    {
        if (step != 1)
            return 1;
        if (PlyrCameraMotIsEnd(act->no) == 0)
            return 0;
        PlyrCameraLabelMotPlay(act->no, 0, act->label, 1);
        step = (s_char)(act->chat_step + 1);
    }
    act->chat_step = step;
    return 0;
}
