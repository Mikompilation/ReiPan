// ingame/photo/n_equip_tray.c -- the companion NPCs' helpers (FF3 0x21d330..0x21e360).
// FF3-only: FF2's n_equip_tray.c (this file's link slot) is the camera's
// equipment tray; nothing here came from FF2. Between the act machine
// (ingame/npc/npc_act.c) and the NPC controller (n_plyr_camera.c): the
// player's turn to an NPC, position-label getters and motions, the model
// data per NPC model type, the NPC's accessory models and its motion loop SE.
// Virtual calls on the models go straight to the slot functions (the model
// classes are not reconstructed; see ingame/ingame.h).

#include "ingame/photo/n_equip_tray.h"

#include <new>                          // placement new on FF3's C++ heap (operator_new)

#include "common/cpp_heap.h"            // operator_new
#include "common/reipan_stub.h"
#include "common/vu0math.h"             // GetTrgtRot, RotLimitChk, NormalizeAngle, GetRandValIByF
#include "graphics/graph3d/sgd_types.h" // SGDCOORDINATE
#include "ingame/ingame.h"              // GameModel
#include "ingame/ingame_wrk.h"          // ingame_wrk
#include "ingame/npc/npc_act.h"         // NPC_LABEL, PlyrCameraGetLabel, PlyrCameraGetMotList, PlyrCameraGetMotFace, PlyrCameraSetLabelPos
#include "ingame/photo/n_plyr_camera.h" // PLYR_CAMERA_WRK, GetPlyrCameraWrk, plyr_camera_se
#include "ingame/plyr/acs_model.h"      // AcsModel
#include "ingame/plyr/player.h"         // PLYR_WRK, GetPlyrAreaNo, PlyrSetRot, SetPlyrAnime
#include "ingame/plyr/plyr_mdl.h"       // PlyrLookPosSet, PlyrLookPosClear
#include "ingame/plyr/plyr_sub.h"       // PlyrGetCostumeMdlDat, PlyrGetAcs, PlyrGetAcsModelNo, PlyrAcsIsNot2
#include "ingame/sound/snd_ctrl.h"      // SndSePlay, SndSeFadeStop, SndSeVolFade
#include "system/mc/dat/save_data.h"

// ──────────────────────────────────────────────────────────────────────
// Callees in runs that are not defined yet. Move each block to its real
// header when that run is defined.
// ──────────────────────────────────────────────────────────────────────

// ──────────────────────────────────────────────────────────────────────
// Data
// ──────────────────────────────────────────────────────────────────────

// The model data of NPC 0's model types {chr, type, mot file} (data
// 0x35f770): 0 real side, 1 dream side, 2 costume, 3 / 4 with the cat.
static const short npc0_mdl_dat[5][3] =
{
    { 0x01, 3, 0x198d },
    { 0x04, 3, 0x198d },
    { 0x01, 3, 0x198d },
    { 0x0e, 6, 0x19c0 },
    { 0x0e, 6, 0x19c0 },
};
static const short npc1_mdl_dat[3] = { 0x02, 4, 0x198f };     // sdata 0x3716f8: NPC 1's (one type)

// NPC 0's model files {mdl, shape} for types 2.. (data 0x35f790; types 0
// and 1 take the character's costume row).
static const short npc0_mdl_file[3][2] =
{
    { 0x1a78, 0x1982 },
    { 0x19bf, -1 },
    { 0x1a7a, -1 },
};

// The character of each NPC (sdata 0x371700): Miku, Kei.
static const u_char npc_chr[PLYR_CAMERA_NUM] = { 1, 2 };

// The accessories' motion lists (sdata 0x371708..0x371733).
static const s_char npc_acs_mot_708[] = { 0x06, 0x09, 0x23, -1 };
static const s_char npc_acs_mot_710[] = { 0x00, 0x02, 0x03, 0x1c, 0x1e, 0x1f, 0x08, -1 };
static const s_char npc_acs_mot_718[] = { 0x06, 0x09, 0x23, -1 };
static const s_char npc_acs_mot_720[] = { -1 };
static const s_char npc_acs_mot_728[] = { 0x22, -1 };
static const s_char npc_acs_mot_730[] = { 0x03, 0x0c, -1 };

// The three lists of each record (data 0x32dcc8.., 0x10 apart).
static const s_char *const npc_acs_mot_0_0[3] = { nullptr, npc_acs_mot_708, npc_acs_mot_710 };           // 0x32dcc8
static const s_char *const npc_acs_mot_0_1[3] = { npc_acs_mot_718, npc_acs_mot_720, npc_acs_mot_728 };   // 0x32dcd8
static const s_char *const npc_acs_mot_1_1[3] = { nullptr, nullptr, npc_acs_mot_730 };                   // 0x32dce8
static const s_char *const npc_acs_mot_1_2[3] = { nullptr, nullptr, npc_acs_mot_730 };                   // 0x32dcf8

// The accessory records (data 0x35f7a0 / 0x35f7c8; sdata 0x371678 points at them).
static const NPC_ACS_DAT npc0_acs_dat[NPC_ACS_NUM] =
{
    {  3,  7, npc_acs_mot_0_0 },
    {  8,  7, npc_acs_mot_0_1 },
    { -1, -1, nullptr },
};
static const NPC_ACS_DAT npc1_acs_dat[NPC_ACS_NUM] =
{
    {  4,   -2, nullptr },          // the fixed place (PlyrCameraAcsFixCoord), always shown
    {  5,    7, npc_acs_mot_1_1 },
    {  7, 0x13, npc_acs_mot_1_2 },
};
static const NPC_ACS_DAT *const npc_acs_tbl[PLYR_CAMERA_NUM] = { npc0_acs_dat, npc1_acs_dat };

// The motion loop SE (data 0x35f7f0; sdata 0x3716a8 / 0x3716ac): NPC 0's
// SE 0x1bf70005 sounds at joint 7 while motion 0xb plays.
static const NPC_MOT_SE npc0_mot_se = { 0x1bf70005, 0x0b, 7, 8 };
static const NPC_MOT_SE *const npc_mot_se[PLYR_CAMERA_NUM] = { &npc0_mot_se, nullptr };

static float         plyr_camera_head_pos[4];   // bss 0x3f61c0: the head the player looks at (PlyrLookPosSet keeps it)
static SGDCOORDINATE plyr_camera_acs_coord;     // bss 0x3f61d0: the fixed place (matLocalWorld only)

// ──────────────────────────────────────────────────────────────────────
// The player looks at NPC npc's head; with on the player also turns to the
// NPC and plays anime 0.
// FF3 0x0021d330

void PlyrCameraPlyrTurn(s_char npc, int on)
{
    float rot[4];

    if (PlyrCameraGetJointPos(npc, 2, plyr_camera_head_pos, nullptr) != 0)
        PlyrLookPosSet(plyr_camera_head_pos);

    if (on == 0)
        return;

    rot[0] = 0.0f;
    rot[1] = 0.0f;
    rot[2] = 0.0f;
    rot[3] = 0.0f;
    GetTrgtRot(ingame_wrk.plyr_wrk->pos, GetPlyrCameraWrk(npc)->pos, rot, 2);
    RotLimitChk(&rot[1]);
    PlyrSetRot(rot);
    SetPlyrAnime(0, 0, 0xff, 0xff, 1, 0.0f);
}

// FF3 0x0021d3f0

void PlyrCameraPlyrLookEnd(void)
{
    PlyrLookPosClear();
}

// FF3 0x0021d408

int PlyrCameraLabelIsPlyrArea(s_char npc, s_char label)
{
    int area;

    if (label < 0)
        return 0;
    area = PlyrCameraGetLabel(npc, label, nullptr)->area;
    if (area < 0)
        return 0;
    return area == GetPlyrAreaNo();
}

// FF3 0x0021d468

u_char PlyrCameraGetLabel84(s_char npc, s_char label)
{
    return PlyrCameraGetLabel(npc, label, nullptr)->_84;
}

// FF3 0x0021d498

void PlyrCameraMotCall(s_char npc, s_char mot, int arg)
{
    PlyrCameraMotPlay(npc, mot, arg);
    PlyrCameraFacialCall(npc, PlyrCameraGetMotFace(npc, mot));
}

// FF3 0x0021d4f0

int PlyrCameraLabelMotPlay(s_char npc, int kind, s_char label, int arg)
{
    const NPC_LABEL *row = PlyrCameraGetLabel(npc, label, nullptr);
    const s_char    *list;
    s_char           num;
    s_char           mot = -1;

    switch (kind)
    {
    case 0:
        if (row->mot < 0)
            break;
        list = PlyrCameraGetMotList(npc, row->mot, nullptr);
        mot  = list[0];
        break;
    case 1:
        if (row->mot < 0)
            break;
        list = PlyrCameraGetMotList(npc, row->mot, &num);
        if (num < 2)
            break;
        mot = list[(s_char)(GetRandValIByF(num - 1) + 1)];
        break;
    case 2:
        mot = row->mot2;
        break;
    case 3:
        mot = row->mot3;
        break;
    case 4:
        mot = row->mot4;
        break;
    case 5:
        mot = row->mot5;
        break;
    case 6:
        mot = row->mot6;
        break;
    }

    if (mot < 0)
        return -1;
    PlyrCameraMotCall(npc, mot, arg);
    return mot;
}

// FF3 0x0021d620

s_char PlyrCameraGetLabelMot2(s_char npc, s_char label)
{
    return PlyrCameraGetLabel(npc, label, nullptr)->mot2;
}

// FF3 0x0021d650

void PlyrCameraLabelMove(s_char npc, s_char label)
{
    const NPC_LABEL *row = PlyrCameraGetLabel(npc, label, nullptr);

    PlyrCameraSetLabelPos(npc, row->area, row->_50);
}

// FF3 0x0021d698

u_char PlyrCameraGetLabel60(s_char npc, s_char label)
{
    return PlyrCameraGetLabel(npc, label, nullptr)->_60;
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x0021d6c8

const short *PlyrCameraGetMdlDat(s_char npc, s_char type)
{
    if (npc == 0)
        return npc0_mdl_dat[type];
    if (npc == 1)
        return npc1_mdl_dat;            // FF3 ignores type for NPC 1
    return nullptr;
}

// Types 0 / 1 are the character's costume row (PLYR_COSTUME_MDL is the
// same two halves).
// FF3 0x0021d708

const short *PlyrCameraGetMdlFile(s_char npc, s_char type)
{
    if (npc == 1 && type >= 2)
        return nullptr;
    if (npc == 0 && type >= 2)
        return npc0_mdl_file[type - 2];
    return (const short *)PlyrGetCostumeMdlDat(npc_chr[npc], (u_char)type);
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x0021d780

const NPC_ACS_DAT *PlyrCameraGetAcsDat(s_char npc, s_char n)
{
    return &npc_acs_tbl[npc][n];
}

// FF3 0x0021d7b0

void PlyrCameraAcsLoadRestore(PLYR_CAMERA_WRK *cam)
{
    s_char i;

    for (i = 0; i < NPC_ACS_SLOT; i++)
        cam->acs[i] = nullptr;
}

// A new AcsModel loading file no, nullptr when the load cannot start.
// (FF3 writes the slot before the load and deletes it through vtable
// +0x0c on failure; the slot ends the same.)

static AcsModel *PlyrCameraAcsNew(short no)
{
    AcsModel *acs = new (operator_new(sizeof(AcsModel))) AcsModel;

    if (acs->Load(no) == 0)
    {
        acs->~AcsModel();               // FF3: delete through vtable +0x0c (0x2ac9b0)
        operator_delete(acs);
        return nullptr;
    }
    return acs;
}

// The accessories of the NPC's records (not with the cat model types) and
// the character's costume accessory in slot 3 (drawn after the body unless
// it is accessory 2). callcheck: operator_new, AcsModelCtor, AcsModelLoad (in PlyrCameraAcsNew)
// FF3 0x0021d7e0

void PlyrCameraAcsCreate(s_char npc)
{
    PLYR_CAMERA_WRK   *cam = GetPlyrCameraWrk(npc);
    const NPC_ACS_DAT *dat;
    int                no;
    u_char             chr;
    s_char             i;

    if (PlyrCameraIsCatMdl(cam) == 0)
    {
        for (i = 0; i < NPC_ACS_NUM; i++)
        {
            if (cam->acs[i] != nullptr)
                continue;
            dat = PlyrCameraGetAcsDat(npc, i);
            if (dat->file < 0)
                continue;
            cam->acs[i] = PlyrCameraAcsNew((short)dat->file);
        }
    }

    if (cam->acs[3] != nullptr)
        return;
    no = PlyrGetAcsModelNo(npc_chr[npc]);
    if (no < 0)
        return;
    cam->acs[3]     = PlyrCameraAcsNew((short)no);
    cam->acs_flg[3] = 4;
    chr = npc_chr[npc];
    if (PlyrAcsIsNot2(chr, PlyrGetAcs((s_char)chr)) != 0)
        cam->acs_flg[3] |= 8;
}

// 0 while an AcsModel still loads; each loaded one starts fully shown or
// hidden by its flags.
// FF3 0x0021d998

int PlyrCameraAcsIsReady(PLYR_CAMERA_WRK *cam)
{
    AcsModel *acs;
    s_char    i;

    for (i = 0; i < NPC_ACS_SLOT; i++)
    {
        acs = cam->acs[i];
        if (acs == nullptr)
            continue;
        if (acs->IsLoaded() == 0)     // FF3: vtable +0x10
            return 0;
        acs->alpha_spd = (cam->acs_flg[i] & 5) != 0 ? 128.0f : -128.0f;    // sdata 0x371680 / 0x371684
    }
    return 1;
}

// FF3 0x0021da60

void PlyrCameraAcsRelease(PLYR_CAMERA_WRK *cam)
{
    s_char i;

    for (i = 0; i < NPC_ACS_SLOT; i++)
    {
        if (cam->acs[i] != nullptr)
        {
            cam->acs[i]->~AcsModel();   // FF3: delete through vtable +0x0c (0x2ac9b0)
            operator_delete(cam->acs[i]);
            cam->acs[i] = nullptr;
        }
    }
}

// FF3 0x0021dac8

void PlyrCameraAcsReset(PLYR_CAMERA_WRK *cam)
{
    s_char i;

    for (i = 0; i < NPC_ACS_SLOT; i++)
        cam->acs_flg[i] = 0;
}

// Each record's flags for motion mot: 1 << the list holding it (a later
// list wins), 0 when none does; a record without lists is shown (4).
// FF3 0x0021daf8

void PlyrCameraAcsMotSet(s_char npc, s_char mot)
{
    PLYR_CAMERA_WRK   *cam = GetPlyrCameraWrk(npc);
    const NPC_ACS_DAT *dat;
    const s_char      *list;
    s_char             i;
    s_char             n;

    for (i = 0; i < NPC_ACS_NUM; i++)
    {
        dat = PlyrCameraGetAcsDat(npc, i);
        if (dat->file < 0)
            continue;
        if (dat->mot == nullptr)
        {
            cam->acs_flg[i] = 4;
            continue;
        }
        cam->acs_flg[i] = 0;
        for (n = 0; n < 3; n++)
        {
            for (list = dat->mot[n]; list != nullptr && *list != -1; list++)
            {
                if (*list == mot)
                {
                    cam->acs_flg[i] = (u_char)(1 << n);
                    break;
                }
            }
        }
    }
}

// At the motion's trigger (the model's anime control word 3): accessories
// of list 1 appear, those of list 0 go.
// FF3 0x0021dc38

static void PlyrCameraAcsMotTrigger(PLYR_CAMERA_WRK *cam)
{
    u_char flg;
    s_char i;

    if (static_cast<GameAnimeModel *>(cam->mdl)->GetAnimeFlag(3) == 0)
        return;

    for (i = 0; i < NPC_ACS_NUM; i++)
    {
        flg = cam->acs_flg[i];
        if ((flg & 2) != 0)
        {
            flg = (flg & ~2) | 4;
            cam->acs_flg[i] = flg;
        }
        if ((flg & 1) != 0)
            cam->acs_flg[i] = flg & ~5;
    }
}

// The fixed place: the unit matrix turned by Y(-pi) then X(pi), moved to
// (156.717, -7.6133, 257.107).
// FF3 0x0021dcd0

static SGDCOORDINATE *PlyrCameraAcsFixCoord(void)
{
    static const float pos[3] = { 156.717f, -7.6133f, 257.107f };  // sdata 0x371688
    static const float rot_x  = 3.14159265f;                         // sdata 0x371698 (0x40490fdb)
    sceVu0FMATRIX      rot;
    sceVu0FMATRIX      move;
    float              rot_y  = -3.14159265f;                        // sdata 0x371694 (0xc0490fdb)

    RotLimitChk(&rot_y);
    sceVu0UnitMatrix(move);             // FF3 copies the unit matrix 0x3293c0
    sceVu0RotMatrixY(rot, move, NormalizeAngle(rot_y));
    sceVu0RotMatrixX(rot, rot, NormalizeAngle(rot_x));
    move[3][0] += pos[0];
    move[3][1] += pos[1];
    move[3][2] += pos[2];
    sceVu0MulMatrix(plyr_camera_acs_coord.matLocalWorld, move, rot);
    return &plyr_camera_acs_coord;
}

// Hangs each accessory on its joint's coordinate of the NPC model (or the
// fixed place); slot 3 on joint 2.
// FF3 0x0021de48

void PlyrCameraAcsSetCoord(s_char npc)
{
    PLYR_CAMERA_WRK   *cam = GetPlyrCameraWrk(npc);
    GameModel         *mdl = cam->mdl;
    const NPC_ACS_DAT *dat;
    SGDCOORDINATE     *coord;
    s_char             i;

    for (i = 0; i < NPC_ACS_NUM; i++)
    {
        if (cam->acs[i] == nullptr)
            continue;
        dat = PlyrCameraGetAcsDat(npc, i);
        if (dat->file < 0 || dat->joint == -1)
            continue;
        if (dat->joint < 0)
            coord = PlyrCameraAcsFixCoord();
        else
            coord = &mdl->GetCoord(0)[mdl->GetJointCoord(dat->joint)];  // FF3: vtable +0x34
        cam->acs[i]->SetCoord(coord);
    }

    if (cam->acs[3] == nullptr)
        return;
    cam->acs[3]->SetCoord(&mdl->GetCoord(0)[mdl->GetJointCoord(2)]);
}

// FF3 0x0021dfc8

void PlyrCameraAcsUpdate(PLYR_CAMERA_WRK *cam)
{
    AcsModel *acs;
    s_char    i;

    PlyrCameraAcsMotTrigger(cam);
    for (i = 0; i < NPC_ACS_SLOT; i++)
    {
        acs = cam->acs[i];
        if (acs == nullptr)
            continue;
        acs->FadeStep();
        acs->alpha_spd = (cam->acs_flg[i] & 5) != 0 ? 8.0f : -16.0f;  // sdata 0x3716a0 / 0x3716a4
    }
}

// FF3 0x0021e080

void PlyrCameraAcsDrawPre(PLYR_CAMERA_WRK *cam)
{
    if (cam->acs[3] == nullptr || (cam->acs_flg[3] & 8) != 0)
        return;
    cam->acs[3]->UpdateMatrix();
    cam->acs[3]->DrawMdl();
}

// FF3 0x0021e0d8

void PlyrCameraAcsDrawPost(PLYR_CAMERA_WRK *cam)
{
    s_char i;

    for (i = 0; i < NPC_ACS_NUM; i++)
    {
        if (cam->acs[i] != nullptr)
        {
            cam->acs[i]->UpdateMatrix();
            cam->acs[i]->DrawMdl();
        }
    }
    if (cam->acs[3] == nullptr || (cam->acs_flg[3] & 8) == 0)
        return;
    cam->acs[3]->UpdateMatrix();
    cam->acs[3]->DrawMdl();
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x0021e190

void PlyrCameraNpc0Mot16(s_char npc)
{
    if (npc == 0)
        PlyrCameraMotCall(0, 0x10, 1);
}

// FF3 0x0021e1d0

void PlyrCameraSeReset(s_char npc)
{
    plyr_camera_se[npc] = -1;
}

// The NPC's loop SE starts (at its joint) with its motion and fades out
// with any other.
// FF3 0x0021e1f0

void PlyrCameraMotSe(s_char npc, s_char mot)
{
    const NPC_MOT_SE *se = npc_mot_se[npc];
    float             pos[4];
    SND_3D_SET        s3d;

    if (se == nullptr)
        return;

    if (se->mot == mot)
    {
        if (plyr_camera_se[npc] != -1)
            return;
        PlyrCameraGetJointPos(npc, se->joint, pos, nullptr);
        s3d.pos = pos;
        s3d.vel = nullptr;              // PC: FF3 sets only pos (the rest is stack)
        s3d.dir = nullptr;
        plyr_camera_se[npc] = SndSePlay(se->se, 100, 0x1000, se->time, &s3d, 0);
    }
    else if (plyr_camera_se[npc] != -1)
    {
        SndSeFadeStop(plyr_camera_se[npc], se->time);
        plyr_camera_se[npc] = -1;
    }
}

// The loop SE is heard only while the NPC is drawn.
// FF3 0x0021e2c0

void PlyrCameraSeVolUpdate(s_char npc)
{
    const NPC_MOT_SE *se = npc_mot_se[npc];

    if (se == nullptr || plyr_camera_se[npc] == -1)
        return;
    SndSeVolFade(plyr_camera_se[npc], PlyrCameraIsDrawn(GetPlyrCameraWrk(npc), 0) != 0 ? 100 : 0, se->time);
}

// ---- stubs (tools/reverse/stubgen.py) ----

