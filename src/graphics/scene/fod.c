// graphics/scene/fod.c -- the scene FOD streams: camera, light and effect
// records per frame, walked in step (FF3 0x1c67c0..0x1c7c38, plus the two
// walks FodNextFrame calls out of line at 0x2ba3c8 / 0x2ba468).
//
// Ported from MioPan's FF2 fod.c and checked against FF3. What FF3 changed:
// - No fod_cmn_mtx: FF2 built a 25x scale + X flip in FodInit; FF3 files are
//   in world units and every position goes through the shared y / z flip
//   (mdl_flip_yz, data 0x329400). Angles convert as deg * PI / 180.
// - Light names match by strstr (anywhere in the name), not as a prefix;
//   the ambient prefix 'o' replaces FF2's 'i'.
// - A directional light keeps its file direction (flipped, + offset); a spot
//   gets range 200, a point range 60 and a zero direction when selected.
// - Light records of unknown type are skipped without a message.
// - FodChangeFodLightToGra3dLight's three light sets are locals, not
//   statics, and installing a light also sets its status word +0x08.
// - The streams are null-checked: FF3 scenes pass no effect stream.

#include "graphics/scene/fod.h"

#include <string.h>                     // memset, strncmp, strstr

#include "common/utility2.h"            // StrToLower
#include "common/vu0math.h"             // Vu0Cosf
#include "graphics/graph3d/g3dLightEx.h"    // gra3dSetLightIntens
#include "graphics/graph3d/gra3d.h"     // GRA3DLIGHTDATA, gra3dSetAmbient, gra3dSetLightData
#include "graphics/mmanage.h"           // mdl_flip_yz
#include "graphics/scene/scene_dat.h"   // SceneCutTimingGet
#include "system/os/system.h"           // GetPALMode

// The light data FodSetMyLight fills (bss 0x3e9cd0) and the scene's fixed
// effects (bss 0x3e9c50). FF3 keeps FF2's empty static initialiser for the
// file (__static_initialization_and_destruction_0, called by its .ctors
// entry Fod_GLOBAL_I with (1, 0xffff)): nothing to do on PC.
// FF3 0x001c7c10, FF3 0x001c7c18.
GRA3DLIGHTDATA FodLight;
FOD_EFF_PARAM  eff_param;

static const float FOD_PI     = 3.1415927f;     // sdata 0x3708e0 ..
static const float FOD_TWO_PI = 6.2831855f;     // sdata 0x370910 / 0x37091c

static u_int *FodGetFixEffect(u_int *tep);
static void   FodGetLightNum(FOD_LIGHT *fl);
static void   FodGetLightSerial(FOD_LIGHT *fl);
static u_int *FodGetFirstLight(FOD_LIGHT *fl, float *offset);
static int    FodGetLightType(FOD_LIGHT *fl, u_int id);

// ──────────────────────────────────────────────────────────────────────
// Latch the three streams, read the camera stream's frame count and tick
// divider, and resolve the light stream's header into fl's lights.
// FF3 0x001c67c0

void FodInit(FOD_CTRL *fc, u_int *tcp, u_int *tlp, u_int *tep, float *offset)
{
    FOD_LIT_FRAME *lit_top;

    memset(fc, 0, sizeof(FOD_CTRL));
    fc->cam_file_hdr     = (FOD_FILE_HDR *)tcp;
    fc->now_frame        = 1;
    fc->lit_file_hdr     = (FOD_FILE_HDR *)tlp;
    fc->eff_file_hdr     = (FOD_FILE_HDR *)tep;
    fc->cut_timing_index = 0;
    fc->now_reso         = 0;
    fc->float_now_frame  = 1.0f;
    fc->pad0[0]          = 0;
    fc->frame_max        = tcp[3];
    fc->cam_frame_top    = (FOD_CAM_FRAME *)(tcp + 0x10);
    fc->resolution       = (u_char)tcp[2];
    fc->end_flg          = 0;

    fc->eff_frame_top = (FOD_EFF_FRAME *)FodGetFixEffect(tep);

    fc->fod_light.lit_top = tlp;
    FodGetLightNum(&fc->fod_light);
    FodGetLightSerial(&fc->fod_light);
    lit_top = (FOD_LIT_FRAME *)FodGetFirstLight(&fc->fod_light, offset);
    fc->lit_frame_next = lit_top;

    fc->eff_frame      = fc->eff_frame_top;
    fc->cam_frame_next = fc->cam_frame_top;
    fc->lit_frame_top  = lit_top;
    fc->cam_frame      = fc->cam_frame_top;
    fc->lit_frame      = lit_top;
}

// ──────────────────────────────────────────────────────────────────────
// cam_frame / cam_frame_next at frame (the whole part) and the frame after
// it, walked from the top: a camera record's size excludes its 0x10-byte
// header. FF2 inlined this into FodNextFrame; FF3 emits it out of line
// (frame in f12, fc in a0).
// FF3 0x002ba3c8

static void FodMoveCameraNextFrame(FOD_CTRL *fc, float frame)
{
    if (fc->cam_file_hdr == nullptr)
    {
        return;
    }

    int first  = (int)frame;
    int second = first + 1;
    if ((int)fc->frame_max < second)
    {
        second = (int)fc->frame_max;
    }

    if (first <= (int)fc->frame_max && first != 0 && fc->cam_file_hdr->frame != 0)
    {
        u_char *addr = (u_char *)fc->cam_frame_top;

        for (int i = 1; i < first; i++)
        {
            addr += (((FOD_CAM_FRAME *)addr)->size & ~3u) + sizeof(FOD_CAM_FRAME);
        }
        fc->cam_frame = (FOD_CAM_FRAME *)addr;

        if (first < second)
        {
            addr += (((FOD_CAM_FRAME *)addr)->size & ~3u) + sizeof(FOD_CAM_FRAME);
        }
        fc->cam_frame_next = (FOD_CAM_FRAME *)addr;
    }
}

// ──────────────────────────────────────────────────────────────────────
// lit_frame / lit_frame_next the same way: a light record's size covers its
// header.
// FF3 0x002ba468

static void FodMoveLightNextFrame(FOD_CTRL *fc, float frame)
{
    if (fc->lit_file_hdr == nullptr)
    {
        return;
    }

    int first  = (int)frame;
    int second = first + 1;
    if ((int)fc->frame_max < second)
    {
        second = (int)fc->frame_max;
    }

    if (first <= (int)fc->frame_max && first != 0 && fc->lit_file_hdr->frame != 0)
    {
        u_char *addr = (u_char *)fc->lit_frame_top;

        for (int i = 1; i < first; i++)
        {
            addr += ((FOD_LIT_FRAME *)addr)->size & ~3u;
        }
        fc->lit_frame = (FOD_LIT_FRAME *)addr;

        if (first < second)
        {
            addr += ((FOD_LIT_FRAME *)addr)->size & ~3u;
        }
        fc->lit_frame_next = (FOD_LIT_FRAME *)addr;
    }
}

// ──────────────────────────────────────────────────────────────────────
// One tick. Every resolution ticks the frame advances by 1 (1.2 in PAL,
// snapped back onto the next authored cut of scene_no so a cut lands on the
// same frame as in NTSC); the camera and light records follow it, the
// effect record steps once. 1 on the tick the frames run out (end_flg).
// FF3 0x001c68b8

int FodNextFrame(FOD_CTRL *fc, int scene_no)
{
    u_char *eff_addr = (u_char *)fc->eff_frame;
    float   frame;

    fc->now_reso++;
    if (fc->now_reso < fc->resolution)
    {
        return 0;
    }
    fc->now_reso = 0;

    fc->float_now_frame += GetPALMode() != 0 ? 1.2f : 1.0f;     // sdata 0x3708d4 / 0x3708d8

    frame = fc->float_now_frame;
    if (GetPALMode() != 0)
    {
        const int *timing = SceneCutTimingGet(scene_no);

        if (timing != nullptr)
        {
            int cut = timing[fc->cut_timing_index];

            if (cut != -1 && (float)cut <= frame)
            {
                fc->cut_timing_index++;
                fc->float_now_frame = (float)cut;
                frame = (float)cut;
            }
        }
    }

    fc->now_frame = (u_int)(int)(frame + 0.5f);
    if ((float)fc->frame_max < frame)
    {
        fc->float_now_frame = (float)fc->frame_max;
    }
    if (fc->frame_max < fc->now_frame)
    {
        fc->end_flg = 1;
        return 1;
    }

    FodMoveCameraNextFrame(fc, fc->float_now_frame);
    FodMoveLightNextFrame(fc, fc->float_now_frame);

    // The effect stream steps one record a tick by its raw size.
    if (fc->eff_file_hdr != nullptr && fc->eff_file_hdr->frame != 0)
    {
        fc->eff_frame = (FOD_EFF_FRAME *)(eff_addr + fc->eff_frame->size);
    }
    return 0;
}

// ──────────────────────────────────────────────────────────────────────
// Seek the three streams to frame from the top (scene scrubbing). The next
// records and end_flg are left to the next FodNextFrame.
// FF3 0x001c6a78

void FodSetFrame(FOD_CTRL *fc, u_int frame)
{
    if (frame > fc->frame_max || frame == 0)
    {
        return;
    }

    u_char *cam = (u_char *)fc->cam_frame_top;
    u_char *lit = (u_char *)fc->lit_frame_top;
    u_char *eff = (u_char *)fc->eff_frame_top;

    for (u_int i = 1; i < frame; i++)
    {
        if (fc->cam_file_hdr != nullptr && fc->cam_file_hdr->frame != 0)
        {
            cam += (((FOD_CAM_FRAME *)cam)->size & ~3u) + sizeof(FOD_CAM_FRAME);
        }
        if (fc->lit_file_hdr != nullptr && fc->lit_file_hdr->frame != 0)
        {
            lit += ((FOD_LIT_FRAME *)lit)->size & ~3u;
        }
        if (fc->eff_file_hdr != nullptr && fc->eff_file_hdr->frame != 0)
        {
            eff += ((FOD_EFF_FRAME *)eff)->size & ~3u;
        }
    }

    fc->now_frame       = frame;
    fc->now_reso        = 0;
    fc->eff_frame       = (FOD_EFF_FRAME *)eff;
    fc->float_now_frame = (float)frame;
    fc->cam_frame       = (FOD_CAM_FRAME *)cam;
    fc->lit_frame       = (FOD_LIT_FRAME *)lit;
}

// ──────────────────────────────────────────────────────────────────────
// The light stream header's four counts; all_lit_num is the stored count
// less one.
// FF3 0x001c6b78

static void FodGetLightNum(FOD_LIGHT *fl)
{
    u_int *lit_addr = fl->lit_top;

    fl->ilit_num    = lit_addr[4];
    fl->slit_num    = lit_addr[5];
    fl->plit_num    = lit_addr[6];
    fl->all_lit_num = (int)lit_addr[7] - 1;
}

// ──────────────────────────────────────────────────────────────────────
// The FOD_LIT_SERIAL table (after the 0x40-byte header) into fl, each name
// lower-cased; the first light whose name contains "hand-spot" or
// "hand_spot" is the player's flashlight.
// FF3 0x001c6ba8

static void FodGetLightSerial(FOD_LIGHT *fl)
{
    FOD_LIT_SERIAL *fls = (FOD_LIT_SERIAL *)(fl->lit_top + 0x10);

    fl->hand_spot_no = -1;
    for (int i = 0; i < fl->all_lit_num; i++, fls++)
    {
        char *name = fl->lit_serial[i].light_name;

        memcpy(&fl->lit_serial[i], fls, sizeof(FOD_LIT_SERIAL));
        StrToLower(name);
        if (strstr(name, "hand-spot") != nullptr || strstr(name, "hand_spot") != nullptr)   // data 0x327428 / 0x32742c
        {
            if (fl->hand_spot_no == -1)
            {
                fl->hand_spot_no = i;
            }
        }
    }
}

// ──────────────────────────────────────────────────────────────────────
// The resident lights from the light stream's header records: the ambient
// (amb[0]; amb[1..5], the per-prefix ambient offsets, cleared), then one
// record per light, typed by its serial. Positions and directions go
// through the y / z flip and get offset added (a spot's direction holds its
// interest point until FodSetSpotLights aims it). Returns the first frame
// record. A record of type 0 is skipped (0x20 bytes); an unknown type stops
// the cursor, as in the ROM.
// FF3 0x001c6c98

static u_int *FodGetFirstLight(FOD_LIGHT *fl, float *offset)
{
    u_char *lit_addr = (u_char *)(fl->lit_top + 0x10) + fl->all_lit_num * sizeof(FOD_LIT_SERIAL);

    for (int i = 0; i < FOD_AMBIENT_MAX; i++)
    {
        fl->amb[i][0] = 0.0f;
        fl->amb[i][1] = 0.0f;
        fl->amb[i][2] = 0.0f;
        fl->amb[i][3] = 0.0f;
    }
    sceVu0CopyVector(fl->amb[0], ((FOD_LIT_AMB *)lit_addr)->color);
    lit_addr += sizeof(FOD_LIT_AMB);

    for (int i = 0; i < fl->all_lit_num; i++)
    {
        G3DLIGHT *l = &fl->all_lit[i];

        switch (FodGetLightType(fl, *(u_int *)lit_addr))
        {
        case 0:
            lit_addr += 0x20;
            break;

        case 1:
        {
            FOD_LIT_INF *rec = (FOD_LIT_INF *)lit_addr;

            g3dutilSetLightDefault(l, G3DLIGHT_DIRECTIONAL);
            sceVu0CopyVector(l->vDiffuse, rec->color);
            sceVu0CopyVector(l->vSpecular, rec->color);
            sceVu0CopyVector(l->vDirection, rec->direction);
            sceVu0ApplyMatrix(l->vDirection, mdl_flip_yz, l->vDirection);
            lit_addr += sizeof(FOD_LIT_INF);
            sceVu0AddVector(l->vDirection, l->vDirection, offset);
            break;
        }

        case 2:
        {
            FOD_LIT_SPOT *rec = (FOD_LIT_SPOT *)lit_addr;
            float         intens;

            g3dutilSetLightDefault(l, G3DLIGHT_SPOT);
            sceVu0CopyVector(l->vDiffuse, rec->color);
            sceVu0CopyVector(l->vSpecular, rec->color);
            sceVu0CopyVector(l->vPosition, rec->position);
            sceVu0CopyVector(l->vDirection, rec->interest);
            sceVu0ApplyMatrix(l->vPosition, mdl_flip_yz, l->vPosition);
            sceVu0ApplyMatrix(l->vDirection, mdl_flip_yz, l->vDirection);
            sceVu0AddVector(l->vPosition, l->vPosition, offset);
            sceVu0AddVector(l->vDirection, l->vDirection, offset);

            intens = Vu0Cosf((rec->cone * FOD_PI) / 180.0f);
            gra3dSetLightIntens(l, intens * intens);
            // gra3dSetLightIntens keeps the cone in afPad0[0]; one folded
            // past straight turns the light off.
            if (l->afPad0[0] < 0.0f)
            {
                gra3dSetLightIntens(l, 0.0f);
            }
            lit_addr += sizeof(FOD_LIT_SPOT);
            l->fMaxRange = 200.0f;      // sdata 0x3708e8
            break;
        }

        case 3:
        {
            FOD_LIT_POINT *rec = (FOD_LIT_POINT *)lit_addr;

            g3dutilSetLightDefault(l, G3DLIGHT_POINT);
            sceVu0CopyVector(l->vDiffuse, rec->color);
            sceVu0CopyVector(l->vSpecular, rec->color);
            sceVu0CopyVector(l->vPosition, rec->position);
            sceVu0ApplyMatrix(l->vPosition, mdl_flip_yz, l->vPosition);
            lit_addr += sizeof(FOD_LIT_POINT);
            sceVu0AddVector(l->vPosition, l->vPosition, offset);
            l->fMaxRange = 60.0f;       // sdata 0x3708ec
            break;
        }

        default:
            break;
        }
    }

    return (u_int *)lit_addr;
}

// ──────────────────────────────────────────────────────────────────────
// Install one light in FodLight's slot and enable it (status words 0 and
// 2); its position is a point (w 1). FF2's Fodg3dSetLight, inlined
// everywhere in FF3.

static inline void Fodg3dSetLight(int iLightId, const G3DLIGHT *pLight)
{
    FodLight.aLight[iLightId]                  = *pLight;
    FodLight.aStatus[iLightId].bEnable         = 1;
    FodLight.aStatus[iLightId].bEnableToShadow = 1;
    FodLight.aLight[iLightId].vPosition[3]     = 1.0f;     // sdata 0x3708f8 / 0x3708fc
}

// ──────────────────────────────────────────────────────────────────────
// The lights of one object (pfx: its name, "room") into FodLight, applied.
// FF3 0x001c6fb0

void FodSetMyLight(FOD_LIGHT *fl, const char *pfx)
{
    FodChangeFodLightToGra3dLight(&FodLight, fl, pfx);
    gra3dSetAmbient(FodLight.vAmbient);
    gra3dSetLightData(&FodLight, nullptr);
}

// ──────────────────────────────────────────────────────────────────────
// Pick the lights that apply to one object and lay them out the way gra3d
// wants them: up to three directional in slots 0..2, three point from
// slot 3, three spot from slot 30 (FodSetSpotLights). A light applies when
// its name contains pfx or, for anything but a character ('c...'), starts
// with "room"; the hand spot always goes to the spots. Every slot of
// pGra3dLight is reset to its type's default and disabled first, and the
// ambient is the scene's plus the offset for pfx's first letter. The
// lights themselves are installed in FodLight (Fodg3dSetLight), which is
// what FodSetMyLight passes.
// callcheck: memcpy (the G3DLIGHT copies are struct assignments)
// FF3 0x001c6ff8

void FodChangeFodLightToGra3dLight(GRA3DLIGHTDATA *pGra3dLight, FOD_LIGHT *fl, const char *pfx)
{
    G3DLIGHT ilight[3];
    G3DLIGHT slight[3];
    G3DLIGHT plight[3];
    int      il_num = 0;
    int      sl_num = 0;
    int      pl_num = 0;
    int      i;
    float   *amb;

    for (i = 0; i < fl->all_lit_num; i++)
    {
        FOD_LIT_SERIAL *fls = &fl->lit_serial[i];
        G3DLIGHT       *org = &fl->all_lit[i];
        int             get_light_flg = 0;

        if (fl->hand_spot_no == i)
        {
            if (sl_num < 3)
            {
                slight[sl_num]      = *org;
                slight[sl_num].Type = G3DLIGHT_SPOT;
                sl_num++;
            }
        }
        else if (strstr(fls->light_name, pfx) != nullptr)
        {
            get_light_flg = 1;
        }
        else if (*pfx != 'c')
        {
            get_light_flg = strncmp(fls->light_name, "room", 4) == 0;      // sdata 0x3708f0
        }

        if (!get_light_flg)
        {
            continue;
        }

        switch (fls->light_type)
        {
        case 1:
            if (il_num < 3)
            {
                ilight[il_num]      = *org;
                ilight[il_num].Type = G3DLIGHT_DIRECTIONAL;
                il_num++;
            }
            break;

        case 2:
            if (sl_num < 3)
            {
                slight[sl_num]      = *org;
                slight[sl_num].Type = G3DLIGHT_SPOT;
                sl_num++;
            }
            break;

        case 3:
            if (pl_num < 3)
            {
                // The resident light's direction is cleared too (data 0x329530).
                org->vDirection[0] = 0.0f;
                org->vDirection[1] = 0.0f;
                org->vDirection[2] = 0.0f;
                org->vDirection[3] = 0.0f;
                plight[pl_num]      = *org;
                plight[pl_num].Type = G3DLIGHT_POINT;
                pl_num++;
            }
            break;

        default:
            break;
        }
    }

    for (i = 0; i < NUM_GRA3DLIGHTID; i++)
    {
        int type;

        pGra3dLight->aStatus[i].bEnable = 0;
        if (i - GRA3D_START_LIGHT_DIRECTIONAL < GRA3D_NUM_LIGHT_DIRECTIONAL)
        {
            type = G3DLIGHT_DIRECTIONAL;
        }
        else if ((u_int)(i - GRA3D_START_LIGHT_POINT) < GRA3D_NUM_LIGHT_POINT)
        {
            type = G3DLIGHT_POINT;
        }
        else if ((u_int)(i - GRA3D_START_LIGHT_SPOT) < GRA3D_NUM_LIGHT_SPOT)
        {
            type = G3DLIGHT_SPOT;
        }
        else
        {
            type = INVALID_G3DLIGHTTYPE;
        }
        g3dutilSetLightDefault(&pGra3dLight->aLight[i], type);
    }

    sceVu0CopyVector(pGra3dLight->vAmbient, fl->amb[0]);
    switch (*pfx)
    {
    case 'c': amb = fl->amb[1]; break;
    case 'r': amb = fl->amb[2]; break;
    case 'f': amb = fl->amb[3]; break;
    case 'd': amb = fl->amb[4]; break;
    case 'o': amb = fl->amb[5]; break;
    default:  amb = nullptr;    break;
    }
    if (amb != nullptr)
    {
        sceVu0AddVector(pGra3dLight->vAmbient, pGra3dLight->vAmbient, amb);
    }

    for (i = 0; i < il_num; i++)
    {
        Fodg3dSetLight(GRA3D_START_LIGHT_DIRECTIONAL + i, &ilight[i]);
    }
    for (i = 0; i < pl_num; i++)
    {
        Fodg3dSetLight(GRA3D_START_LIGHT_POINT + i, &plight[i]);
    }
    if (sl_num > 0)
    {
        FodSetSpotLights(slight, (u_int)sl_num);
    }
}

// ──────────────────────────────────────────────────────────────────────
// Turn each spot's interest point into a unit direction and install it in
// the spot slots from GRA3D_START_LIGHT_SPOT. The ROM's direction runs from
// the interest point back to the light (vPosition - vDirection), the sense
// gra3d's spot lighting takes; scene.c's hand spot uses the same.
// callcheck: memcpy (Fodg3dSetLight's struct assignment)
// FF3 0x001c73b8

void FodSetSpotLights(G3DLIGHT *sl, u_int num)
{
    if (sl == nullptr)
    {
        return;
    }
    for (u_int i = 0; i < num; i++)
    {
        sl[i].vPosition[3] = 1.0f;      // sdata 0x3708fc
        sceVu0SubVector(sl[i].vDirection, sl[i].vPosition, sl[i].vDirection);
        sceVu0Normalize(sl[i].vDirection, sl[i].vDirection);
        Fodg3dSetLight(GRA3D_START_LIGHT_SPOT + (int)i, &sl[i]);
    }
}

// ──────────────────────────────────────────────────────────────────────
// One frame's light animation over fl's lights: one FOD_LIT_ANM per light
// whose serial has anm_flg 1, each followed by a quadword per channel its
// anm[] flags (the cone too). The specular follows the diffuse. Unknown
// types skip only their header.
// FF3 0x001c7498

void FodGetToSgLight(FOD_LIGHT *fl, FOD_LIT_FRAME *frame, float *offset)
{
    float *data = (float *)(frame + 1);

    for (int i = 0; i < fl->all_lit_num; i++)
    {
        if (fl->lit_serial[i].anm_flg != 1)
        {
            continue;
        }

        FOD_LIT_ANM *anm = (FOD_LIT_ANM *)data;
        G3DLIGHT    *l   = &fl->all_lit[i];

        data += 4;
        if (anm->type < 1 || anm->type > 3)
        {
            continue;
        }

        if (anm->anm[0] != 0)
        {
            sceVu0CopyVector(l->vDiffuse, data);
            sceVu0CopyVector(l->vSpecular, l->vDiffuse);
            data += 4;
        }

        switch (anm->type)
        {
        case 1:                         // directional: the direction
            if (anm->anm[1] != 0)
            {
                sceVu0CopyVector(l->vDirection, data);
                sceVu0ApplyMatrix(l->vDirection, mdl_flip_yz, l->vDirection);
                sceVu0AddVector(l->vDirection, l->vDirection, offset);
                data += 4;
            }
            break;

        case 2:                         // spot: position, interest, cone
            if (anm->anm[1] != 0)
            {
                sceVu0CopyVector(l->vPosition, data);
                sceVu0ApplyMatrix(l->vPosition, mdl_flip_yz, l->vPosition);
                sceVu0AddVector(l->vPosition, l->vPosition, offset);
                data += 4;
            }
            if (anm->anm[2] != 0)
            {
                sceVu0CopyVector(l->vDirection, data);
                sceVu0ApplyMatrix(l->vDirection, mdl_flip_yz, l->vDirection);
                sceVu0AddVector(l->vDirection, l->vDirection, offset);
                data += 4;
            }
            if (anm->anm[3] != 0)
            {
                float intens = Vu0Cosf((data[0] * FOD_PI) / 180.0f);   // sdata 0x370900 / 0x370904

                gra3dSetLightIntens(l, intens * intens);
                data += 4;
            }
            break;

        case 3:                         // point: the position
            if (anm->anm[1] != 0)
            {
                sceVu0CopyVector(l->vPosition, data);
                sceVu0ApplyMatrix(l->vPosition, mdl_flip_yz, l->vPosition);
                sceVu0AddVector(l->vPosition, l->vPosition, offset);
                data += 4;
            }
            break;
        }
    }
}

// ──────────────────────────────────────────────────────────────────────
// The position and interest point of the last light named "<pfx's first 4
// chars>?drop_spot..."; nothing when there is none. Unreferenced in FF3
// (Ghidra had it inside FodGetToSgLight's body).
// FF3 0x001c7708

void FodGetDropSpotPos(FOD_LIGHT *fl, const char *pfx, float *lp, float *li)
{
    int no = -1;

    for (int i = 0; i < fl->all_lit_num; i++)
    {
        const char *name = fl->lit_serial[i].light_name;

        if (strncmp(pfx, name, 4) == 0 && strncmp("drop_spot", name + 5, 9) == 0)   // data 0x359a18
        {
            no = i;
        }
    }
    if (no >= 0)
    {
        sceVu0CopyVector(lp, fl->all_lit[no].vPosition);
        sceVu0CopyVector(li, fl->all_lit[no].vDirection);
    }
}

// ──────────────────────────────────────────────────────────────────────
// The type of the last serial whose light_no is id; 0 if none.
// FF3 0x001c77f8

static int FodGetLightType(FOD_LIGHT *fl, u_int id)
{
    int ret_type = 0;

    for (int i = 0; i < fl->all_lit_num; i++)
    {
        if (fl->lit_serial[i].light_no == id)
        {
            ret_type = fl->lit_serial[i].light_type;
        }
    }
    return ret_type;
}

// ──────────────────────────────────────────────────────────────────────
// The scene's opening camera from the camera stream's header record: the
// points flipped and offset, roll and fov in radians folded once below PI.
// FF3 0x001c7828

void FodGetFirstCam(FOD_CAMERA_DATA *cam, FOD_CTRL *fc, float *offset)
{
    FOD_FIRST_CAM *first = (FOD_FIRST_CAM *)(fc->cam_file_hdr + 1);

    sceVu0CopyVector(cam->vPosition, first->p);
    sceVu0CopyVector(cam->vTarget, first->i);
    cam->fRoll = (first->roll * FOD_PI) / 180.0f;     // sdata 0x370908 / 0x37090c
    sceVu0ApplyMatrix(cam->vPosition, mdl_flip_yz, cam->vPosition);
    sceVu0ApplyMatrix(cam->vTarget, mdl_flip_yz, cam->vTarget);
    sceVu0AddVector(cam->vPosition, cam->vPosition, offset);
    sceVu0AddVector(cam->vTarget, cam->vTarget, offset);
    if (FOD_PI < cam->fRoll)
    {
        cam->fRoll -= FOD_TWO_PI;
    }
    cam->fFov = (first->fov * FOD_PI) / 180.0f;
    if (FOD_PI < cam->fFov)
    {
        cam->fFov -= FOD_TWO_PI;
    }
    cam->fNearZ = first->near_z;
    cam->fFarZ  = first->far_z;
}

// ──────────────────────────────────────────────────────────────────────
// One frame's camera animation: anm[0..5] select position, target (a
// quadword each, flipped and offset), roll, fov (degrees -> radians, folded
// below PI), near and far (a float each).
// FF3 0x001c7978

void FodGetCamData(FOD_CAMERA_DATA *cam, FOD_CAM_FRAME *frame, float *offset)
{
    float *fdat = (float *)(frame + 1);
    float  v[4];
    float  a;

    for (int i = 0; i < 6; i++)
    {
        if (frame->anm[i] != 1)
        {
            continue;
        }

        switch (i)
        {
        case 0:
            sceVu0CopyVector(v, fdat);
            sceVu0ApplyMatrix(v, mdl_flip_yz, v);
            sceVu0AddVector(v, v, offset);
            sceVu0CopyVector(cam->vPosition, v);
            fdat += 4;
            break;

        case 1:
            sceVu0CopyVector(v, fdat);
            sceVu0ApplyMatrix(v, mdl_flip_yz, v);
            sceVu0AddVector(v, v, offset);
            sceVu0CopyVector(cam->vTarget, v);
            fdat += 4;
            break;

        case 2:
            a = (fdat[0] * FOD_PI) / 180.0f;     // sdata 0x370914 / 0x370918
            cam->fRoll = FOD_PI < a ? a - FOD_TWO_PI : a;
            fdat++;
            break;

        case 3:
            a = (fdat[0] * FOD_PI) / 180.0f;
            cam->fFov = FOD_PI < a ? a - FOD_TWO_PI : a;
            fdat++;
            break;

        case 4:
            cam->fNearZ = fdat[0];
            fdat++;
            break;

        case 5:
            cam->fFarZ = fdat[0];
            fdat++;
            break;
        }
    }
}

// ──────────────────────────────────────────────────────────────────────
// Index the effect stream's fixed records into eff_param and return the
// first frame record. The ROM's quirks are kept: fix_eff_num is not
// clamped to 12, and fix[] steps 0x1c (sizeof FOD_EFF_DATA) while the
// records and the returned cursor step 0x10. PC: no stream gives nullptr
// (the ROM returns address 0x10; nothing reads it then).
// FF3 0x001c7b50

static u_int *FodGetFixEffect(u_int *tep)
{
    if (tep == nullptr)
    {
        eff_param.fix_eff_num = 0;
        return nullptr;
    }
    if (tep[3] == 0)
    {
        eff_param.fix_eff_num = 0;
        return tep + 4;
    }

    memset(&eff_param, 0, sizeof(eff_param));
    FOD_EFF_DATA *fed     = (FOD_EFF_DATA *)&tep[8];
    eff_param.pdf_p       = nullptr;
    eff_param.fix_eff_num = tep[4];
    for (u_int i = 0; i < eff_param.fix_eff_num; i++)
    {
        eff_param.fix[i] = fed++;
    }
    tep += 4 + eff_param.fix_eff_num * 4;
    return tep + 4;
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x001c7c00

GRA3DLIGHTDATA *FodGetGra3DLight(void)
{
    return &FodLight;
}
