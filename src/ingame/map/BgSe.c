// ingame/map/BgSe.c -- background (layout) SEs: CBgSe, FF3 0x00109728..0x00109dd8.
// FF3-only (no FF2 counterpart; file name inferred from link order), written
// from the FF3 disassembly. Up to four ambient SEs per area from the table
// layout_se_dat (0x2ef3c8): placed at the area's map offset, started when
// their bank is loaded and, for an entry with a region label, once the
// player stands in that region. GCC 3 emitted the channel constructor and
// CBgSe's constructor twice (C1/C2: 0x109728 / 0x109748, 0x1099c0 /
// 0x109a28); one body each here.

#include "ingame/map/BgSe.h"

#include <string.h>                     // memset

#include "common/vu0math.h"             // GetRandValF
#include "ingame/ingame_wrk.h"          // ingame_wrk.plyr_wrk
#include "ingame/enemy/ene_spe_obj.h"    // RegSecListHasLabel, RegSecListGet4Pos
#include "ingame/map/MapLoad.h"         // MapLoadGetBuffID, MapLoadGetOffsetVector
#include "ingame/plyr/player.h"         // PLYR_WRK pos / floor
#include "ingame/sound/snd_ctrl.h"      // SndSeBankLoad / IsLoaded / ReleaseAfterPlay, SndSePlay, SndSeFadeStop, SND_3D_SET
#include "system/eeiop/cd_file_dat.h"   // KAN_SE_*_BD_BD

// ──────────────────────────────────────────────────────────────────────
// Data

#define LAYOUT_SE_LIST_MAX  0x18        // LayoutSeMain's region list (0x60 bytes of stack on the EE)
#define LAYOUT_SE_PITCH     0x1000      // SndSePlay pitch: unchanged
#define LAYOUT_SE_FADE      15          // SndSeFadeStop frames

// FF3 data 0x2ef3c8: per area, the ambient SEs (34 x 0x24). Every SE is
// number 0 of its bank.
static const LAYOUT_SE_DAT layout_se_dat[LAYOUT_SE_DAT_NUM] =
{
    // area, se, s3d, rate, vol, label, ofs
    { 38, (u_int)KAN_SE_00_IKE1_BD_BD << 16, 1, 100.0f, 100,     -1, { 472.84f, -9.41f, 452.36f } },
    { 38, (u_int)KAN_SE_00_IKE1_BD_BD << 16, 1, 100.0f, 100,     -1, { 167.20999f, 1.15f, 499.56998f } },
    {  1, (u_int)KAN_SE_01_KOUSHI_BD_BD << 16, 1, 40.0f, 100,  10061, { 90.329994f, -7.72f, 394.71997f } },
    { 11, (u_int)KAN_SE_01_KOUSHI_BD_BD << 16, 1, 40.0f, 100,  41075, { -7.8399997f, -3.2199998f, 107.10999f } },
    { 18, (u_int)KAN_SE_02_DENKI_BD_BD << 16, 1, 100.0f, 100,     -1, { 105.63f, -44.07f, 49.55f } },
    {  6, (u_int)KAN_SE_04_IDO_BD_BD << 16, 1, 50.0f, 100,  22038, { 135.81999f, -6.56f, 18.769999f } },
    {  8, (u_int)KAN_SE_04_IDO_BD_BD << 16, 1, 100.0f, 100,  32033, { 237.12999f, 15.78f, 108.6f } },
    {  7, (u_int)KAN_SE_05_ANA1_BD_BD << 16, 1, 40.0f, 100,  30061, { 163.4f, -9.48f, 182.07999f } },
    { 10, (u_int)KAN_SE_07_YUKI_BD_BD << 16, 1, 40.0f, 100,  38077, { 79.619995f, -71.27f, 357.50998f } },
    { 11, (u_int)KAN_SE_08_CLOCK_BD_BD << 16, 1, 100.0f, 100,     -1, { 91.59f, -60.19f, 192.95f } },
    { 39, (u_int)KAN_SE_15_IKE2_BD_BD << 16, 1, 100.0f, 100,     -1, { 472.84f, -9.41f, 452.36f } },
    { 39, (u_int)KAN_SE_15_IKE2_BD_BD << 16, 1, 100.0f, 100,     -1, { 167.20999f, 1.15f, 499.56998f } },
    { 41, (u_int)KAN_SE_09_SUITEKI_BD_BD << 16, 1, 25.0f, 100,     -1, { 17.63f, 5.81f, 480.0f } },
    { 41, (u_int)KAN_SE_09_SUITEKI_BD_BD << 16, 1, 25.0f, 100,     -1, { 35.91f, 20.339998f, 93.2f } },
    { 41, (u_int)KAN_SE_09_SUITEKI_BD_BD << 16, 1, 25.0f, 100,     -1, { -3.26f, 7.9399996f, 299.25f } },
    { 41, (u_int)KAN_SE_09_SUITEKI_BD_BD << 16, 1, 25.0f, 100,     -1, { -124.0f, 5.14f, 539.88995f } },
    { 41, (u_int)KAN_SE_09_SUITEKI_BD_BD << 16, 1, 25.0f, 100,     -1, { -158.81999f, 3.8999999f, 631.92f } },
    { 41, (u_int)KAN_SE_09_SUITEKI_BD_BD << 16, 1, 25.0f, 100,     -1, { -238.78f, 8.54f, 642.41f } },
    { 41, (u_int)KAN_SE_09_SUITEKI_BD_BD << 16, 1, 25.0f, 100,     -1, { -226.18f, 4.85f, 564.82996f } },
    { 41, (u_int)KAN_SE_09_SUITEKI_BD_BD << 16, 1, 25.0f, 100,     -1, { -97.31f, 1.91f, 310.75998f } },
    { 41, (u_int)KAN_SE_09_SUITEKI_BD_BD << 16, 1, 25.0f, 100,     -1, { -109.78f, -22.21f, 294.24f } },
    { 41, (u_int)KAN_SE_09_SUITEKI_BD_BD << 16, 1, 25.0f, 100,     -1, { -69.63f, 9.11f, 251.81f } },
    { 41, (u_int)KAN_SE_09_SUITEKI_BD_BD << 16, 1, 25.0f, 100,     -1, { -73.92f, 7.1899996f, 167.79f } },
    { 43, (u_int)KAN_SE_09_SUITEKI_BD_BD << 16, 1, 25.0f, 100,     -1, { 17.63f, 5.81f, 480.0f } },
    { 43, (u_int)KAN_SE_09_SUITEKI_BD_BD << 16, 1, 25.0f, 100,     -1, { 35.91f, 20.339998f, 93.2f } },
    { 43, (u_int)KAN_SE_09_SUITEKI_BD_BD << 16, 1, 25.0f, 100,     -1, { -3.26f, 7.9399996f, 299.25f } },
    { 43, (u_int)KAN_SE_09_SUITEKI_BD_BD << 16, 1, 25.0f, 100,     -1, { -124.0f, 5.14f, 539.88995f } },
    { 43, (u_int)KAN_SE_09_SUITEKI_BD_BD << 16, 1, 25.0f, 100,     -1, { -158.81999f, 3.8999999f, 631.92f } },
    { 43, (u_int)KAN_SE_09_SUITEKI_BD_BD << 16, 1, 25.0f, 100,     -1, { -238.78f, 8.54f, 642.41f } },
    { 43, (u_int)KAN_SE_09_SUITEKI_BD_BD << 16, 1, 25.0f, 100,     -1, { -226.18f, 4.85f, 564.82996f } },
    { 43, (u_int)KAN_SE_09_SUITEKI_BD_BD << 16, 1, 25.0f, 100,     -1, { -97.31f, 1.91f, 310.75998f } },
    { 43, (u_int)KAN_SE_09_SUITEKI_BD_BD << 16, 1, 25.0f, 100,     -1, { -109.78f, -22.21f, 294.24f } },
    { 43, (u_int)KAN_SE_09_SUITEKI_BD_BD << 16, 1, 25.0f, 100,     -1, { -69.63f, 9.11f, 251.81f } },
    { 43, (u_int)KAN_SE_09_SUITEKI_BD_BD << 16, 1, 25.0f, 100,     -1, { -73.92f, 7.1899996f, 167.79f } },
};

// The static one, bss 0x377f10. FF3 0x00109d20 (the file's
// __static_initialization_and_destruction_0, BgSeStaticInit) constructs it
// at start-up (FF3 0x00109d98, _GLOBAL__I_) and destroys it at exit
// (FF3 0x00109db8, _GLOBAL__D_); the C++ static does both here.

static CBgSe layout_se_wrk;

// ──────────────────────────────────────────────────────────────────────
// FF3 0x00109748 (LayoutSeChInit; FF3 0x00109728 is the same body).

LAYOUT_SE_CH::LAYOUT_SE_CH()
{
    vol = 0;
    label = -1;
    step = LAYOUT_SE_STEP_IDLE;
    s3d = -1;
    se = (u_int)-1;
    play_id = -1;
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x00109768: the bank first; a failed request (SE_BANK_ERROR) leaves
// the channel idle.

void LAYOUT_SE_CH::Set(u_int se_no, int label_no, int s3d_flag, int volume, const float *position)
{
    if (SndSeBankLoad((u_short)(se_no >> 16)) == SE_BANK_ERROR)
    {
        return;
    }

    vol = volume;
    se = se_no;
    s3d = s3d_flag;
    label = label_no;
    pos[0] = position[0];
    pos[1] = position[1];
    pos[2] = position[2];
    pos[3] = position[3];
    step = LAYOUT_SE_STEP_LOAD;
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x00109808

void LAYOUT_SE_CH::Play()
{
    SND_3D_SET  s3d_set;
    SND_3D_SET *s3s = &s3d_set;

    if (vol < 0)
    {
        vol = 0;
    }
    if (vol >= 0x80)
    {
        vol = 0x7f;
    }

    if (s3d == 1)
    {
        memset(&s3d_set, 0, sizeof(s3d_set));
        s3d_set.pos = pos;
    }
    else
    {
        s3s = nullptr;
    }

    play_id = SndSePlay(se, vol, LAYOUT_SE_PITCH, 0, s3s, 0);
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x001098a8: list / num are the player's regions (LayoutSeMain).

void LAYOUT_SE_CH::Main(MB_OUT_SECTION **list, int num)
{
    switch (step)
    {
    case LAYOUT_SE_STEP_LOAD:
        if (SndSeBankIsLoaded((u_short)(se >> 16)) == 0)
        {
            return;
        }
        if (label != -1)
        {
            step = LAYOUT_SE_STEP_WAIT;
            return;
        }
        break;

    case LAYOUT_SE_STEP_WAIT:
        if (RegSecListHasLabel(label, list, num) == 0)
        {
            return;
        }
        break;

    default:
        return;
    }

    Play();
    step = LAYOUT_SE_STEP_PLAY;
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x00109940: label is kept.

void LAYOUT_SE_CH::Stop()
{
    if (se != (u_int)-1)
    {
        if (play_id != -1)
        {
            SndSeFadeStop(play_id, LAYOUT_SE_FADE);
        }
        SndSeBankReleaseAfterPlay(play_id, se >> 16, 0);
    }

    vol = 0;
    step = LAYOUT_SE_STEP_IDLE;
    se = (u_int)-1;
    play_id = -1;
    s3d = -1;
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x001099b0

CBgSe *GetLayoutSeWrk(void)
{
    return &layout_se_wrk;
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x00109a28 (BgSeCtor; FF3 0x001099c0 is the same body). The channels
// are constructed first (LayoutSeChInit x 4).
// callcheck: LayoutSeChInit (the implicit construction of ch[4])

CBgSe::CBgSe()
{
    StopAll();
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x00109ab8 (BgSeDtor; base 0x00109a90, deleting 0x00109ae0 adds
// operator_delete).

CBgSe::~CBgSe()
{
    StopAll();
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x00109b18: the regions under the player (rectangle type 2) gate the
// labelled channels.

void CBgSe::Main()
{
    MB_OUT_SECTION *list[LAYOUT_SE_LIST_MAX];
    PLYR_WRK       *plyr = ingame_wrk.plyr_wrk;
    int             num = RegSecListGet4Pos(plyr->pos, plyr->floor, list, LAYOUT_SE_LIST_MAX, 2);

    for (int i = 0; i < LAYOUT_SE_CH_NUM; i++)
    {
        ch[i].Main(list, num);
    }
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x00109b98: nothing when the area is unchanged; else the area's
// entries in table order, each by its chance, into the channels until four
// are set.

void CBgSe::SetArea(int area_no)
{
    sceVu0FVECTOR pos;
    int           n;
    int           c;

    if (area == area_no)
    {
        return;
    }

    n = 0;
    StopAll();
    area = area_no;
    c = 0;

    for (int i = 0; i < LAYOUT_SE_DAT_NUM; i++)
    {
        const LAYOUT_SE_DAT *dat = &layout_se_dat[i];

        if (dat->area != area || GetRandValF(100.0f) > dat->rate)
        {
            continue;
        }

        n++;
        MapLoadGetOffsetVector(pos, MapLoadGetBuffID(area));
        pos[0] += dat->ofs[0];
        pos[3] = 1.0f;
        pos[1] += dat->ofs[1];
        pos[2] += dat->ofs[2];
        ch[c++].Set(dat->se, dat->label, dat->s3d, dat->vol, pos);
        if (n > 3)
        {
            return;
        }
    }
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x00109cc8

void CBgSe::StopAll()
{
    for (int i = 0; i < LAYOUT_SE_CH_NUM; i++)
    {
        ch[i].Stop();
    }
    area = -1;
}
