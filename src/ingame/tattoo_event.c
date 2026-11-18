/* ==========================================================================
 *  ingame/tattoo_event.c
 *
 *  The tattoo grab event (see tattoo_event.h), FF3 0x13a010..0x13b320,
 *  FF3-only. TattooEvCameraMain is a 13-state machine on the work's state:
 *
 *    0  idle (the map camera runs)        7  EJobE05: fade out (15 frames)
 *    1  the viewfinder hold was dropped   8  wait 15 frames
 *    2  start: keep the camera, aim at    9  leave the dream look, screen
 *       Rei's joint 2                        effect 9, fov 1.5
 *    3  ease in (65 NTSC frames, t^2),   10  one frame
 *       white fade 20 frames before end  11  the map camera's view, white
 *    4  the dream look: dream light,         fade in
 *       monochrome, screen effect 8,     12  ease back out from Rei to the
 *       the map-sky effect, YKN_18           map camera (65 frames), then 0
 *    5  2 frames, then fade in
 *    6  the follow camera circles Rei
 *
 *  The eases share one curve, f = t^2 / T^2 (FF3: 2 / T^2 * 0.5 * t * t),
 *  for the position, the target, the fov (towards 1.5) and the deform
 *  strength (SetEffects 4, up to 37). FF3 runs the vector steps on VU0
 *  through a global scratch quadword (0x41e530 behind a static guard);
 *  they are plain per-component sums here.
 * ======================================================================== */

#include "ingame/tattoo_event.h"

#include "common/vu0math.h"                     // RotLimitChk
#include "graphics/effect/effect.h"             // SetEffects, EffectSetScreenEffectNo, EFFECT_CONT
#include "graphics/effect/effect_mapsky.h"      // EffMapSkyReq / Release / Stop / IsStopped
#include "graphics/effect/effect_oth.h"         // EffectMonochromeSet
#include "graphics/graph2d/fade.h"              // FadeInReq, FadeOutReq
#include "graphics/graph3d/gra3d.h"             // gra3dcam*, gra3dApplyCamera
#include "graphics/graph3d/gra3dMisc.h"         // gra3dPrelight
#include "ingame/camera/map_camera.h"           // MapCamNormalCameraCtrl
#include "ingame/char_model.h"                  // CharPlayerModel
#include "ingame/enemy/ene_main.h"              // ENE_WRK, EneMahiAllEnd
#include "ingame/enemy/enemy.h"                 // EneSePlay, EneObjEffSetRate, the streams
#include "ingame/ingame_wrk.h"                  // ingame_wrk
#include "ingame/map/MapCtrl.h"                 // MapAreaHitCircle
#include "ingame/plyr/player.h"                 // PLYR_WRK, SetPlyrAnime, PlyrSetMode, GetPlyrAreaNo
#include "ingame/plyr/plyr_sub.h"               // PlyrGetJointPos, PlyrTurnFlagClear
#include "ingame/plyr/unit_ctl.h"               // UnitGetTrgtRot
#include "ingame/story_menu.h"                  // LockTblInc / Dec
#include "sdk/libvu0.h"
#include "system/os/system.h"                   // GetFrameTimeScale

#include <math.h>                               // sqrtf
#include <stddef.h>                             // offsetof
#include <string.h>                             // memset

// The event's work (bss 0x391000, TattooEvInit clears 0x80 bytes).
// layout: host -- sky / ene are EE pointers at +0x20 / +0x24; the vectors
// keep their ROM offsets.
struct TATTOO_EV_WRK                    /* 0x80 on the EE */
{
    /* 0x00 */ int           state;     // TattooEvCameraMain's state, 0 idle
    /* 0x04 */ int           toggle;    // flipped 0 / 1 at each way back (TattooEvGetToggle)
    /* 0x08 */ int           _08;
    /* 0x0c */ int           scr_effect_no; // TattooScrEffectSave / Restore
    /* 0x10 */ float         timer;     // frames into the current state
    /* 0x14 */ float         fov;       // the camera's fov when the event began
    /* 0x18 */ float         yaw;       // the follow camera's yaw about Rei
    /* 0x1c */ float         dist;      // its XZ distance, 70..90
    /* 0x20 */ EFFECT_CONT  *sky;       // the map-sky effect (EffMapSkyReq), nullptr: none
    /* 0x24 */ ENE_WRK      *ene;       // the ghost that started the event
    /* 0x30 */ sceVu0FVECTOR follow;    // the follow camera's position
    /* 0x40 */ sceVu0FVECTOR pos;       // the camera position an ease starts / ends at
    /* 0x50 */ sceVu0FVECTOR closeup;     // the close point 10 units from the look point
    /* 0x60 */ sceVu0FVECTOR target;    // the camera target an ease starts / ends at
    /* 0x70 */ sceVu0FVECTOR look;      // Rei's joint 2 (or the follow camera's target)
};
static_assert(offsetof(TATTOO_EV_WRK, timer) == 0x10, "TATTOO_EV_WRK.timer");
static_assert(offsetof(TATTOO_EV_WRK, dist) == 0x1c, "TATTOO_EV_WRK.dist");
static_assert(offsetof(TATTOO_EV_WRK, sky) == 0x20, "TATTOO_EV_WRK.sky");
static_assert(offsetof(TATTOO_EV_WRK, follow) == 0x30, "TATTOO_EV_WRK.follow");
static_assert(offsetof(TATTOO_EV_WRK, look) == 0x70, "TATTOO_EV_WRK.look");
static_assert(sizeof(TATTOO_EV_WRK) == 0x80, "TATTOO_EV_WRK is 0x80 bytes");

static TATTOO_EV_WRK tattoo_ev_wrk;     // bss 0x391000

#define TATTOO_EASE_FRAME   65.0f       // NTSC frames of each ease (PAL divides by GetFrameTimeScale)
#define TATTOO_FOV          1.5f        // the fov the ease in reaches and the way back ends on
#define TATTOO_DEFORM_MAX   37.0f       // SetEffects(4, 1, 1, ...) at the full ease
#define TATTOO_FOLLOW_UP    20.0f       // the follow camera's eye and look point above Rei's position
#define TATTOO_FOLLOW_NEAR  70.0f       // its distance band
#define TATTOO_FOLLOW_FAR   90.0f
#define TATTOO_FOLLOW_LOW   40.0f       // under this XZ distance the eye sinks by the shortfall
#define TATTOO_WALL_R       4.0f        // MapAreaHitCircle radius of the eye
#define TATTOO_WALL_STEP    0.004363323f    // 0.25 degrees a try
#define TATTOO_WALL_TRIES   16          // TattooCamAvoidWall's depth limit

static int  TattooCamAvoidWall(int n);
static void TattooCamFollow(void);
static void TattooCamFollowStart(float *pt);

static inline PLYR_WRK *TattooPlyr(void)
{
    return ingame_wrk.plyr_wrk;
}

static inline void TattooCopy(float *dst, const float *src)
{
    dst[0] = src[0];
    dst[1] = src[1];
    dst[2] = src[2];
    dst[3] = src[3];
}

// out = (to - from) * f + from, all four components (vsub / vmulx / vadd .xyzw).
static inline void TattooLerp(float *out, const float *from, const float *to, float f)
{
    out[0] = (to[0] - from[0]) * f + from[0];
    out[1] = (to[1] - from[1]) * f + from[1];
    out[2] = (to[2] - from[2]) * f + from[2];
    out[3] = (to[3] - from[3]) * f + from[3];
}

// |a - b| in 3D (vadday.x / vmaddz.x vf1, vf1.x = 1).
static inline float TattooDist3(const float *a, const float *b)
{
    float dx = a[0] - b[0];
    float dy = a[1] - b[1];
    float dz = a[2] - b[2];

    return sqrtf(dx * dx + dy * dy + dz * dz);
}

// |a - b| on XZ (vaddz.x).
static inline float TattooDistXZ(const float *a, const float *b)
{
    float dx = a[0] - b[0];
    float dz = a[2] - b[2];

    return sqrtf(dx * dx + dz * dz);
}

// FF3 0x0013a010
void TattooEvInit(void)
{
    memset(&tattoo_ev_wrk, 0, sizeof(tattoo_ev_wrk));
}

// FF3 0x0013a038
void TattooEvEneRelease(ENE_WRK *ene)
{
    TATTOO_EV_WRK *w = &tattoo_ev_wrk;

    if (w->ene != nullptr && w->ene == ene)
    {
        EffMapSkyRelease(w->sky);
        w->sky = nullptr;
    }
}

// FF3 0x0013a080. A viewfinder hold (going in, up, coming out) is dropped
// first: Rei's anime goes back to 0 (blend 5 and arg 1 when only coming out)
// and the camera waits one frame in state 1.
void TattooEvStart(ENE_WRK *ene)
{
    TATTOO_EV_WRK *w = &tattoo_ev_wrk;
    PLYR_WRK      *plyr;

    if (w->state == 0)
    {
        w->state = 2;
        LockTblInc(2);
        plyr = TattooPlyr();
        if ((plyr->flags & PLYR_FLG_HOLD) != 0)
        {
            if ((plyr->flags & (PLYR_FLG_800000 | PLYR_FLG_400000)) != 0)
                SetPlyrAnime(0, 0, 0xff, 0xff, 0, 0.0f);
            else
                SetPlyrAnime(0, 5, 0xff, 0xff, 1, 0.0f);
            TattooPlyr()->flags &= ~(u_long)PLYR_FLG_HOLD;
            PlyrSetMode(0, nullptr);
            w->state = 1;
        }
    }
    w->ene = ene;
    EneEvStreamPreload(ene);
}

// FF3 0x0013a158
void TattooEvResume(void)
{
    if (tattoo_ev_wrk.state == 6)
        tattoo_ev_wrk.state = 7;
}

// callcheck: GetFrameTimeScale, gra3dcamGetFov, gra3dcamGetPosition, gra3dcamGetTarget, PlyrGetJointPos, EneSePlay, FadeOutReq, SetEffects, gra3dcamSetFov, gra3dcamSetPosition, gra3dcamSetTarget, gra3dApplyCamera, EffectMonochromeSet, gra3dPrelight, EneMahiAllEnd, EneObjEffSetRate, EneStreamNearStop, EneEvStreamPlay, FadeInReq, LockTblDec, PlyrTurnFlagClear, SetPlyrAnime, LockTblInc, PlyrSetMode, EneStreamFadeOut, EneEvStreamReset, MapCamNormalCameraCtrl
// FF3 0x0013a178 (CameraMain). Returns 1 in states 0 and 1, and on the
// frame the way back ends: the map camera then runs as usual.
int TattooEvCameraMain(int rect_flg)
{
    TATTOO_EV_WRK *w = &tattoo_ev_wrk;
    sceVu0FVECTOR  cam_pos;
    sceVu0FVECTOR  cam_tgt;
    sceVu0FVECTOR  start;
    PLYR_WRK      *plyr;
    float          total;
    float          f;
    float          fov;
    float          d;
    int            ret = 0;

    total = TATTOO_EASE_FRAME / GetFrameTimeScale();

    switch (w->state)
    {
    case 0:
        ret = 1;
        break;

    case 1:
        ret = 1;
        w->state = 2;
        break;

    case 2:
        w->fov = gra3dcamGetFov();
        TattooCopy(w->pos, gra3dcamGetPosition());
        TattooCopy(w->target, gra3dcamGetTarget());
        PlyrGetJointPos(2, w->look, nullptr);
        d = TattooDist3(w->pos, w->look);
        TattooLerp(w->closeup, w->look, w->pos, 10.0f / d);
        TattooLerp(start, w->look, w->pos, 70.0f / d);
        TattooCamFollowStart(start);
        EneSePlay(w->ene, 10);
        EneSePlay(w->ene, 11);
        w->timer = 0.0f;
        w->state = 3;
        // fall through
    case 3:
        f = (2.0f / (total * total)) * 0.5f * w->timer * w->timer;
        if (w->timer < total)
        {
            TattooLerp(cam_pos, w->pos, w->closeup, f);
            TattooLerp(cam_tgt, w->target, w->look, f);
            w->timer += 1.0f;
            fov = (TATTOO_FOV - w->fov) * f + w->fov;
        }
        else
        {
            TattooCopy(cam_pos, w->closeup);
            TattooCopy(cam_tgt, w->look);
            w->timer = 0.0f;
            w->state = 4;
            fov = TATTOO_FOV;
        }
        if ((int)(total - w->timer) == 20)
            FadeOutReq(0x80, 0x80, 0x80, 20);
        goto set_camera;

    case 4:
        plyr = TattooPlyr();
        static_cast<CharPlayerModel *>(plyr->mdl)->SetDreamLight();
        w->sky = EffMapSkyReq(TattooPlyr()->pos);
        TattooPlyr()->flags |= PLYR_FLG_000004;
        EffectMonochromeSet(1, 0);
        gra3dPrelight(0);
        TattooScrEffectDream();
        EneMahiAllEnd(w->ene);
        gra3dcamSetFov(w->fov);
        EneObjEffSetRate(0.0f, w->ene);
        EneStreamNearStop();
        EneEvStreamPlay();
        w->timer = 0.0f;
        w->state = 5;
        // fall through
    case 5:
        if (w->timer < 2.0f)
        {
            w->timer += 1.0f;
        }
        else
        {
            FadeInReq(0x80, 0x80, 0x80, 15);
            LockTblDec(2);
            PlyrTurnFlagClear();
            SetPlyrAnime(0, 0, 0xff, 0xff, 0, 0.0f);
            w->timer = 0.0f;
            w->state = 6;
        }
        SetEffects(4, 1, 1, 0x25);
        break;

    case 6:
        if ((TattooPlyr()->flags & (PLYR_FLG_800000 | PLYR_FLG_400000)) == 0)
            TattooCamFollow();
        SetEffects(4, 1, 1, 0x25);
        break;

    case 7:
        LockTblInc(2);
        FadeOutReq(0x80, 0x80, 0x80, 15);
        SetEffects(4, 1, 1, 0x25);
        w->timer = 0.0f;
        w->state = 8;
        break;

    case 8:
        if (w->timer < 15.0f)
        {
            w->timer += 1.0f;
        }
        else
        {
            w->timer = 0.0f;
            w->state = 9;
        }
        SetEffects(4, 1, 1, 0x25);
        break;

    case 9:
        LockTblDec(2);
        LockTblInc(3);
        PlyrTurnFlagClear();
        SetPlyrAnime(0, 0, 0xff, 0xff, 0, 0.0f);
        plyr = TattooPlyr();
        if ((plyr->flags & PLYR_FLG_HOLD) != 0)
        {
            plyr->flags &= ~(u_long)PLYR_FLG_HOLD;
            PlyrSetMode(0, nullptr);
        }
        static_cast<CharPlayerModel *>(TattooPlyr()->mdl)->ClearDreamLight();
        TattooEvEffectRelease();
        EffectMonochromeSet(0, 0);
        gra3dPrelight(0);
        TattooScrEffectAfter();
        gra3dcamSetFov(TATTOO_FOV);
        w->timer = 0.0f;
        w->state = 10;
        // fall through
    case 10:
        if (w->timer < 1.0f)
        {
            w->timer += 1.0f;
        }
        else
        {
            TattooPlyr()->flags &= ~(u_long)PLYR_FLG_000004;
            LockTblInc(2);
            LockTblDec(3);
            w->timer = 0.0f;
            w->state = 11;
        }
        break;

    case 11:
        w->toggle = (w->toggle + 1) & 1;
        EneStreamFadeOut(10);
        EneEvStreamReset();
        TattooCamFollow();
        TattooCopy(w->look, gra3dcamGetTarget());
        MapCamNormalCameraCtrl(rect_flg);
        TattooCopy(w->pos, gra3dcamGetPosition());
        TattooCopy(w->target, gra3dcamGetTarget());
        d = TattooDist3(w->pos, w->look);
        TattooLerp(w->closeup, w->look, w->pos, 10.0f / d);
        FadeInReq(0x80, 0x80, 0x80, 20);
        EneSePlay(w->ene, 0xf);
        EneSePlay(w->ene, 0x10);
        w->state = 12;
        w->timer = total;
        // fall through
    case 12:
        f = (2.0f / (total * total)) * 0.5f * w->timer * w->timer;
        if (0.0f < w->timer)
        {
            TattooLerp(cam_pos, w->pos, w->closeup, f);
            TattooLerp(cam_tgt, w->target, w->look, f);
            w->timer -= 1.0f;
            fov = (TATTOO_FOV - w->fov) * f + w->fov;
        }
        else
        {
            // The way back ends on fov 1.5 too, not on the saved fov (ROM).
            fov = TATTOO_FOV;
            TattooCopy(cam_pos, w->pos);
            TattooCopy(cam_tgt, w->target);
            w->timer = 0.0f;
            ret = 1;
            w->state = 0;
            EneObjEffSetRate(1.0f, w->ene);
            LockTblDec(2);
        }
    set_camera:
        SetEffects(4, 1, 1, (int)(f * TATTOO_DEFORM_MAX));
        gra3dcamSetFov(fov);
        gra3dcamSetPosition(cam_pos);
        gra3dcamSetTarget(cam_tgt, 1);
        gra3dApplyCamera(nullptr, 1, 1);
        break;
    }
    return ret;
}

// FF3 0x0013ac20
int TattooEvIsLate(void)
{
    return tattoo_ev_wrk.state > 5;
}

// FF3 0x0013ac38
int TattooEvIsIdle(void)
{
    return tattoo_ev_wrk.state == 0;
}

// FF3 0x0013ac48
void TattooEvEffectRelease(void)
{
    if (tattoo_ev_wrk.sky != nullptr)
    {
        EffMapSkyRelease(tattoo_ev_wrk.sky);
        tattoo_ev_wrk.sky = nullptr;
    }
}

// FF3 0x0013ac88
void TattooEvEffectStop(void)
{
    if (tattoo_ev_wrk.sky != nullptr)
        EffMapSkyStop(tattoo_ev_wrk.sky);
}

// FF3 0x0013acb8
int TattooEvEffectIsStopped(void)
{
    if (tattoo_ev_wrk.sky != nullptr)
        return EffMapSkyIsStopped(tattoo_ev_wrk.sky);
    return 1;
}

// callcheck: GetPlyrAreaNo, sceVu0UnitMatrix, sceVu0RotMatrixY, sceVu0ApplyMatrix, sceVu0AddVector, MapAreaHitCircle, UnitGetTrgtRot, RotLimitChk, TattooCamAvoidWall
// FF3 0x0013acf0. The follow camera's eye at (yaw, dist), 20 up, tested
// against the room's walls; on a hit the yaw turns 0.25 degrees away from
// the side MapAreaHitCircle pushed the point to and the test runs again,
// at most 17 times. Returns the depth whose point was clear, -1 when none.
static int TattooCamAvoidWall(int n)
{
    TATTOO_EV_WRK *w = &tattoo_ev_wrk;
    PLYR_WRK      *plyr;
    sceVu0FMATRIX  mat;
    sceVu0FVECTOR  ofs;
    sceVu0FVECTOR  eye;
    sceVu0FVECTOR  hit;
    sceVu0FVECTOR  rot_hit;
    sceVu0FVECTOR  rot_eye;
    float          diff[4];
    float          step = 1.0f;
    int            area;

    area = GetPlyrAreaNo();
    ofs[0] = 0.0f;
    ofs[1] = -TATTOO_FOLLOW_UP;
    ofs[2] = w->dist;
    ofs[3] = 1.0f;
    sceVu0UnitMatrix(mat);
    sceVu0RotMatrixY(mat, mat, w->yaw);
    sceVu0ApplyMatrix(eye, mat, ofs);
    plyr = TattooPlyr();
    sceVu0AddVector(eye, eye, plyr->pos);
    TattooCopy(hit, eye);
    plyr = TattooPlyr();
    if (MapAreaHitCircle(TATTOO_WALL_R, hit, plyr->pos, area, plyr->floor, plyr->mh_no) == 0)
        return n;

    UnitGetTrgtRot(TattooPlyr()->pos, hit, rot_hit, 2);
    UnitGetTrgtRot(TattooPlyr()->pos, eye, rot_eye, 2);
    diff[0] = rot_eye[1] - rot_hit[1];
    RotLimitChk(diff);
    if (0.0f < diff[0])
        step = -1.0f;
    w->yaw += step * TATTOO_WALL_STEP;
    RotLimitChk(&w->yaw);
    if (n < TATTOO_WALL_TRIES)
        return TattooCamAvoidWall(n + 1);
    return -1;
}

// callcheck: PlyrGetJointPos, sceVu0UnitMatrix, sceVu0RotMatrixY, sceVu0ApplyMatrix, sceVu0AddVector, TattooCamAvoidWall, UnitGetTrgtRot, gra3dcamSetPosition, gra3dcamSetTarget, gra3dApplyCamera
// FF3 0x0013aea8. The follow camera: its XZ distance is kept in 70..90
// (outside the band it may only move towards it, with Rei's own distance
// from the eye), the eye swings round walls, and while the distance is in
// the band and no wall is near it stays put and only turns to Rei.
static void TattooCamFollow(void)
{
    TATTOO_EV_WRK *w = &tattoo_ev_wrk;
    PLYR_WRK      *plyr = TattooPlyr();
    sceVu0FMATRIX  mat;
    sceVu0FVECTOR  ofs;
    sceVu0FVECTOR  eye;
    sceVu0FVECTOR  eye_copy;
    sceVu0FVECTOR  joint;
    sceVu0FVECTOR  rot;
    float          d_old;
    float          d_now;
    float          dist;
    float          low;
    int            r;

    ofs[0] = 0.0f;
    ofs[1] = -TATTOO_FOLLOW_UP;
    ofs[2] = 0.0f;
    ofs[3] = 1.0f;
    d_old = TattooDistXZ(w->follow, plyr->pos_old);
    d_now = TattooDistXZ(w->follow, plyr->pos);
    if (d_old < TATTOO_FOLLOW_NEAR)
        dist = (d_now < d_old) ? d_old : d_now;
    else if (TATTOO_FOLLOW_FAR < d_old)
        dist = (d_old < d_now) ? d_old : d_now;
    else if (d_now < TATTOO_FOLLOW_NEAR)
        dist = TATTOO_FOLLOW_NEAR;
    else if (TATTOO_FOLLOW_FAR < d_now)
        dist = TATTOO_FOLLOW_FAR;
    else
        dist = d_now;
    w->dist = dist;
    ofs[2] = dist;

    // The ROM reads Rei's joint 2 here and never uses it.
    PlyrGetJointPos(2, joint, nullptr);

    sceVu0UnitMatrix(mat);
    sceVu0RotMatrixY(mat, mat, w->yaw);
    sceVu0ApplyMatrix(eye, mat, ofs);
    sceVu0AddVector(eye, eye, TattooPlyr()->pos);
    TattooCopy(eye_copy, eye);
    r = TattooCamAvoidWall(0);

    // Again with the yaw TattooCamAvoidWall may have turned.
    sceVu0UnitMatrix(mat);
    sceVu0RotMatrixY(mat, mat, w->yaw);
    sceVu0ApplyMatrix(eye, mat, ofs);
    sceVu0AddVector(eye, eye, TattooPlyr()->pos);
    TattooCopy(eye_copy, eye);

    if (r > 0)
    {
        TattooCopy(w->follow, eye);
    }
    else
    {
        if (r == 0)
        {
            if (w->dist < TATTOO_FOLLOW_NEAR || TATTOO_FOLLOW_FAR < w->dist)
                TattooCopy(w->follow, eye);
            UnitGetTrgtRot(TattooPlyr()->pos, w->follow, rot, 2);
        }
        else
        {
            UnitGetTrgtRot(TattooPlyr()->pos, w->follow, rot, 2);
            w->dist = d_now;
        }
        w->yaw = rot[1];
    }

    plyr = TattooPlyr();
    low = 0.0f;
    d_now = TattooDistXZ(w->follow, plyr->pos);
    if (d_now <= TATTOO_FOLLOW_LOW)
        low = TATTOO_FOLLOW_LOW - d_now;
    gra3dcamSetPosition(w->follow[0], w->follow[1] - low, w->follow[2]);
    plyr = TattooPlyr();
    gra3dcamSetTarget(plyr->pos[0], plyr->pos[1] - TATTOO_FOLLOW_UP, plyr->pos[2], 1);
    gra3dApplyCamera(nullptr, 1, 1);
}

// FF3 0x0013b1e0. The follow camera starts on the line from Rei to pt, at
// her height less 20 (its w is left as it was).
static void TattooCamFollowStart(float *pt)
{
    TATTOO_EV_WRK *w = &tattoo_ev_wrk;
    sceVu0FVECTOR  rot;

    UnitGetTrgtRot(TattooPlyr()->pos, pt, rot, 2);
    w->yaw = rot[1];
    w->follow[0] = pt[0];
    w->follow[1] = TattooPlyr()->pos[1] - TATTOO_FOLLOW_UP;
    w->follow[2] = pt[2];
}

// FF3 0x0013b270
int TattooEvGetToggle(void)
{
    return tattoo_ev_wrk.toggle;
}

// FF3 0x0013b280
void TattooScrEffectSave(void)
{
    tattoo_ev_wrk.scr_effect_no = EffectGetScreenEffectNo();
}

// FF3 0x0013b2a8
void TattooScrEffectRestore(void)
{
    EffectSetScreenEffectNo(tattoo_ev_wrk.scr_effect_no);
}

// FF3 0x0013b2c8
void TattooScrEffectAfter(void)
{
    EffectSetScreenEffectNo(9);
}

// FF3 0x0013b2e0
void TattooScrEffectDream(void)
{
    EffectSetScreenEffectNo(8);
}

// FF3 0x0013b2f8 is the file's empty static initializer (jr ra), folded into
// TattooScrEffectDream's body by Ghidra; FF3 0x0013b300 (TattooEvGlobalCtor)
// is its _GLOBAL__I_ (nothing to run on the host).
