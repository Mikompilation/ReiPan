/* ==========================================================================
 *  ingame/cine_film.c
 *
 *  The projector films (see cine_film.h). FF3-only, FF3 0x14ac58..0x14c7c8.
 *
 *  The screen packet (CineScreenDrawGrid) is built as on the PS2, a GIF
 *  tristrip per visible cell in the packet buffer; the host never runs it,
 *  so every visible cell also goes to ReiPan_Graph3dDrawEffectQuad with its
 *  world corners (the effect-quad path of effect_ene.c, same GS registers).
 *
 *  FF3 0x0014b3f8 / 0x0014b508 are GCC's second constructor / destructor
 *  entries of CineProjector (byte-identical, no callers); the class has one
 *  of each here.
 * ======================================================================== */
#include "ingame/cine_film.h"

#include <new>                                  // placement new on FF3's C++ heap (operator_new)
#include <string.h>                             // memset
#include <libgraph.h>                           // SCE_GS_* / SCE_GIF_*

#include "common/cpp_heap.h"                    // operator_new, operator_delete
#include "common/heapctrl.h"                    // heapCtrlMalloc, heapCtrlFree
#include "common/vu0math.h"                     // NormalizeAngle
#include "graphics/effect/effect.h"             // eff_wrk
#include "graphics/graph3d/gra3d.h"             // gra3dGetCamera, GRA3DCAMERA
#include "graphics/graph3d/gra3dMisc.h"         // gra3dSetSpecialLight, gra3dUpdateSpecialLight
#include "graphics/gs_reg.h"                    // GS_REG_SET, SetGsRegsCached
#include "ingame/cine_menu.h"                   // CineMenuStateClear, CineFilmTblFindFile, CineFilmTblGetPhotoWin
#include "ingame/ingame_wrk.h"                  // ingame_wrk.plyr_wrk
#include "ingame/map/MapObjWrk.h"               // MapObjPutCheck, MapObjGetMatrix
#include "ingame/map/MapSky.h"                  // GetMapSky, MAP_SKY
#include "ingame/plyr/player.h"                 // GetPlyrAreaNo, FrameInsideChk, PLYR_MODE_DOOR
#include "system/eeiop/snd_bank.h"              // SndBankNew / IsReady / Play / Release
#include "system/eeiop/snd_buffer.h"            // SndBufPause / Restart / FadeStop, CSND_BUF_PLAY_NO_ID
#include "system/eeiop/snd_util.h"              // snd_utilAutoRelease, AUTO_BD_ERR_OK
#include "system/os/sys_heap.h"                 // GetSystemHeapWrkP
#include "system/os/system.h"                   // GetPacketBufMgr, PACKET_BUF_MGR
#include "system/playpss/playpss.h"             // playPssGetMpegInfo, PLAY_PSS_MPEG_INFO
#include "reipan/reipan_memory.h"               // PC: ReiPan_GetHostPointer
#include "reipan/rendering/reipan_graph3d.h"    // PC: ReiPan_Graph3dDrawEffectQuad

#define CINE_MOVIE_TBP      0x2bc0              // the film's frame in GS memory (MapSkyMovieSet)
#define CINE_MOVIE_W        0xa0
#define CINE_MOVIE_H        0x80
#define CINE_RATTLE_BD      0x1b41              // EVSE_018_EISYAKI_BD (projector), its header 0x1b42
#define CINE_SCREEN_Z_OFS   10                  // GS Z bias of the screen cells

// ──────────────────────────────────────────────────────────────────────
// Tables (one row per projector)
// ──────────────────────────────────────────────────────────────────────

static const int cine_area_tbl[CINE_PROJECTOR_NUM]  = { 0x13, 0x30, 0x1c };           // data 0x3509a0
static const int cine_film_init[CINE_PROJECTOR_NUM] = { 0x9e7, 0xa38, 0xa35 };        // data 0x3509b0: SCENE9100M_PSS ..
static const int cine_spot_tbl[CINE_PROJECTOR_NUM]  = { 0x20, 0x7fffffff, 0x7fffffff }; // data 0x3509c0
static const int cine_dark_tbl[CINE_PROJECTOR_NUM]  = { 0, 0, 1 };                    // data 0x3509d0

// The wall screens: TL, TR, BL, BR (data 0x3509e0, 0x40 per projector).
static sceVu0FVECTOR cine_screen[CINE_PROJECTOR_NUM][4] = {
    { { -172.0f,  -87.0f,  953.2f, 1.0f }, { -227.0f,  -87.0f,  953.2f, 1.0f },
      { -172.0f,  -43.0f,  953.2f, 1.0f }, { -227.0f,  -43.0f,  953.2f, 1.0f } },
    { {  180.27f, -19.73f, 281.35f, 1.0f }, { 189.52f, -19.73f, 272.0f, 1.0f },
      {  180.27f, -11.2f,  281.35f, 1.0f }, { 189.52f, -11.21f, 272.0f, 1.0f } },
    { { -102.01f, -44.49f, 471.2f, 1.0f }, { -102.01f, -44.49f, 421.2f, 1.0f },
      { -102.01f,  -9.49f, 471.2f, 1.0f }, { -102.01f,  -9.49f, 421.2f, 1.0f } },
};

static const char *const cine_reel_name[CINE_PROJECTOR_NUM] = { "f033", nullptr, nullptr };  // data 0x2f62d8 (sdata 0x36da48)
static const s_char cine_reel_axis[CINE_PROJECTOR_NUM]      = { 0, -1, -1 };              // sdata 0x36da58

// ──────────────────────────────────────────────────────────────────────
// State
// ──────────────────────────────────────────────────────────────────────

int    cine_film[CINE_PROJECTOR_NUM];           // bss 0x391850
u_char cine_flag[CINE_FILM_NUM];                // bss 0x391860

static CineProjector *cine_projector;           // sbss 0x372e9c (gp-0x654)
static int            cine_state;               // sbss 0x372ea0: 0 none, 1 setting up, 2 playing
static int            cine_area;                // sbss 0x372ea4: area of the last CineFilmPlayReq

static int            cine_reel_active;         // sdata 0x36da18
static int            cine_reel_area = -1;      // sdata 0x36da1c
static const char    *cine_reel_obj;            // sdata 0x36da20
static s_char         cine_reel_rot;            // sdata 0x36da24: 0 / 1 about X, 2 / 3 about Z
static float          cine_reel_angle;          // sdata 0x36da28: degrees

static int            cine_frame = -1;          // sdata 0x36da50: frames the film has run, -1: none
static int            cine_watch_no = -1;       // sdata 0x36da60: film whose flags are watched
static int           *cine_watch;               // sdata 0x36da64: its {photo start, photo end} frames

static int  CineTakeFilm(void);
static void CineSetFilm(int film);
static void CineFlagInit(void);
static void CineFlagWatchStart(int no);
static void CineFlagWatchSeen(void);
static void CineFlagWatchEnd(void);

// ──────────────────────────────────────────────────────────────────────
// The screen grid
// ──────────────────────────────────────────────────────────────────────

// Splits the screen quad (corners 0, 1, 2; corner 3 is implied) into cols
// x rows cells and allocates 4 strip corners (TL, TR, BL, BR) per cell with
// their u / v fractions from the system heap.
// FF3 0x0014ac58

static void CineScreenMakeGrid(sceVu0FVECTOR *corners, int cols, int rows,
                               sceVu0FVECTOR **vtx, float **u, float **v)
{
    static const int dc[4] = { 0, 1, 0, 1 };
    static const int dr[4] = { 0, 0, 1, 1 };
    sceVu0FVECTOR e1, e2, ey, ex;
    // the ROM takes these on the stack (alloca); the grid is at most 21 x 17
    sceVu0FVECTOR pt[(CINE_GRID_ROWS + 1) * (CINE_GRID_COLS + 1)];
    float         uf[CINE_GRID_COLS + 1];
    float         vf[CINE_GRID_ROWS + 1];
    int           r, c, k, cell;

    for (k = 0; k < 4; k++)
    {
        e1[k] = corners[1][k] - corners[0][k];
        e2[k] = corners[2][k] - corners[0][k];
    }
    for (r = 0; r < rows + 1; r++)
    {
        vf[r] = (float)r / (float)rows;
        for (k = 0; k < 4; k++)
            ey[k] = e2[k] * vf[r];
        for (c = 0; c < cols + 1; c++)
        {
            uf[c] = (float)c / (float)cols;
            for (k = 0; k < 4; k++)
            {
                ex[k] = e1[k] * uf[c];
                pt[r * (cols + 1) + c][k] = ex[k] + ey[k] + corners[0][k];
            }
        }
    }

    *vtx = (sceVu0FVECTOR *)heapCtrlMalloc(GetSystemHeapWrkP(), rows * cols * sizeof(sceVu0FVECTOR) * 4, 0, 0, 0);
    *u   = (float *)heapCtrlMalloc(GetSystemHeapWrkP(), rows * cols * sizeof(float) * 4, 0, 0, 0);
    *v   = (float *)heapCtrlMalloc(GetSystemHeapWrkP(), rows * cols * sizeof(float) * 4, 0, 0, 0);

    for (r = 0; r < rows; r++)
    {
        for (c = 0; c < cols; c++)
        {
            cell = (r * cols + c) * 4;
            for (k = 0; k < 4; k++)
            {
                memcpy((*vtx)[cell + k], pt[(r + dr[k]) * (cols + 1) + c + dc[k]], sizeof(sceVu0FVECTOR));
                (*u)[cell + k] = uf[c + dc[k]];
                (*v)[cell + k] = vf[r + dr[k]];
            }
        }
    }
}

// One GIF packet: TEX0, then a textured tristrip per cell whose corners all
// pass the guard band and Z range; ST spans the texture window u0 + uw * u,
// v0 + vh * v (texels).
// FF3 0x0014b010

static void CineScreenDrawGrid(float u0, float v0, float uw, float vh,
                               sceVu0FVECTOR *vtx, float *u, float *v, int cols, int rows,
                               int r, int g, int b, int a, u_long tex0, int z_ofs)
{
    GRA3DCAMERA    *cam = gra3dGetCamera();
    float           tw  = (float)(1 << ((u_int)(tex0 >> 26) & 0xf));
    float           th  = (float)(1 << ((u_int)(tex0 >> 30) & 0xf));
    PACKET_BUF_MGR *mgr = GetPacketBufMgr();
    u_int           adrs = mgr->GetPacket();
    u_long         *pkt  = (u_long *)ReiPan_GetHostPointer(adrs);
    int             n = 4;
    int             row, col, k, cell, clip;

    pkt[0] = SCE_GIF_SET_TAG(2, 1, 0, 0, SCE_GIF_PACKED, 1);
    pkt[1] = SCE_GIF_PACKED_AD;
    pkt[2] = 0;
    pkt[3] = SCE_GS_TEXFLUSH;
    pkt[4] = tex0;
    pkt[5] = SCE_GS_TEX0_1;
    pkt[7] = SCE_GS_ST | (SCE_GS_RGBAQ << 4) | (SCE_GS_XYZF2 << 8);   // 0x412

    for (row = 0; row < rows; row++)
    {
        for (col = 0; col < cols; col++)
        {
            sceVu0IVECTOR iv[4];
            float         s[4], t[4], q[4];
            float         st[8];

            cell = (row * cols + col) * 4;
            sceVu0RotTransPersN(iv, cam->matWorldScreen, &vtx[cell], 4, 1);

            clip = 0;
            for (k = 0; k < 4; k++)
            {
                if (iv[k][0] < 0x4000 || iv[k][0] > 0xc000)
                    clip = 1;
                if (iv[k][1] < 0x4000 || iv[k][1] > 0xc000)
                    clip = 1;
                if (iv[k][2] < 1 || iv[k][2] > 0xffffff)
                    clip = 1;
                q[k] = 1.0f / (float)iv[k][3];
                s[k] = ((u0 + uw * u[cell + k]) * q[k]) / tw;
                t[k] = ((v0 + vh * v[cell + k]) * q[k]) / th;
            }
            if (clip)
                continue;

            for (k = 0; k < 4; k++)
            {
                u_int *w = (u_int *)&pkt[n * 2];

                memcpy(&w[0], &s[k], 4);
                memcpy(&w[1], &t[k], 4);
                memcpy(&w[2], &q[k], 4);
                w[3]  = 0;
                w[4]  = (u_int)r;
                w[5]  = (u_int)g;
                w[6]  = (u_int)b;
                w[7]  = (u_int)a;
                w[8]  = (u_int)iv[k][0];
                w[9]  = (u_int)iv[k][1];
                w[10] = (u_int)((iv[k][2] + z_ofs) * 0x10);
                w[11] = k < 2 ? 0x8000 : 0;            // ADC: the strip's first two vertices draw nothing
                n += 3;

                // PC: the same strip from its world corners.
                st[k * 2 + 0] = (u0 + uw * u[cell + k]) / tw;
                st[k * 2 + 1] = (v0 + vh * v[cell + k]) / th;
            }
            ReiPan_Graph3dDrawEffectQuad((const float (*)[4])&vtx[cell], 4, st, r, g, b, a, tex0, z_ofs);
        }
    }

    // NLOOP vertices, EOP, PRE, PRIM 0x54 (tristrip, textured, blended), NREG 3
    pkt[6] = (u_long)((n - 4) / 3) | 0x302a400000008000ULL;
    mgr->SetPacket(adrs + n * 0x10, 1, 0);
}

// ──────────────────────────────────────────────────────────────────────
// The reel
// ──────────────────────────────────────────────────────────────────────

// FF3 0x0014bc68

static int CineReelGetPos(sceVu0FVECTOR out)
{
    if (cine_reel_area < 0)
        return 0;
    return MapObjPutCheck(out, cine_reel_area, cine_reel_obj) == 0;
}

// FF3 0x0014bc98

static void CineReelSetActive(int active)
{
    cine_reel_active = active;
}

// Degrees wrapped into -180..180, in radians. FF3 0x0014be70

static float CineDegToRad(float deg)
{
    if (deg > 0.0f)
    {
        while (deg > 180.0f)
            deg -= 360.0f;
    }
    else
    {
        while (deg < -180.0f)
            deg += 360.0f;
    }
    return (deg * 3.141592f) / 180.0f;                  // sdata 0x36da44: 0x40490fd8
}

// Turns the reel object 10 degrees a frame (backwards) about X or Z: its
// matrix is rebuilt from scratch at the object's position.
// FF3 0x0014bca0

static void CineReelTurn(void)
{
    sceVu0FVECTOR pos;
    sceVu0FVECTOR rot;
    sceVu0FVECTOR scale;
    float        *m;
    float         rad;

    if (cine_reel_active == 0 || cine_reel_area < 0)
        return;
    m = MapObjGetMatrix(cine_reel_area, cine_reel_obj);
    if (m == nullptr)
        return;
    if (MapObjPutCheck(pos, cine_reel_area, cine_reel_obj) != 0)
        return;

    for (cine_reel_angle += -10.0f; cine_reel_angle >= 360.0f; cine_reel_angle -= 360.0f)
        ;
    for (; cine_reel_angle < 0.0f; cine_reel_angle += 360.0f)
        ;
    rad = CineDegToRad(cine_reel_angle);

    scale[0] = scale[1] = scale[2] = scale[3] = 1.0f;
    if (cine_reel_rot < 0)
        return;
    if (cine_reel_rot < 2)
    {
        rot[0] = rad;
        rot[2] = 0.0f;
    }
    else
    {
        if (cine_reel_rot >= 4)
            return;
        rot[0] = 0.0f;
        rot[2] = rad;
    }
    rot[1] = 0.0f;
    rot[3] = 0.0f;

    sceVu0FMATRIX &mat = *(sceVu0FMATRIX *)m;
    sceVu0UnitMatrix(mat);
    mat[1][1] = scale[1];
    mat[2][2] = scale[2];
    mat[0][0] = scale[0];
    sceVu0RotMatrixX(mat, mat, NormalizeAngle(NormalizeAngle(rot[0])));
    sceVu0RotMatrixY(mat, mat, NormalizeAngle(NormalizeAngle(rot[1])));
    sceVu0RotMatrixZ(mat, mat, NormalizeAngle(NormalizeAngle(rot[2])));
    mat[3][0] += pos[0];                                // vadd.xyz: w kept
    mat[3][1] += pos[1];
    mat[3][2] += pos[2];
}

// ──────────────────────────────────────────────────────────────────────
// CineProjector
// ──────────────────────────────────────────────────────────────────────

// Starts loading the film (no loop; the dark projector passes its flag as
// the movie's alpha threshold).
// FF3 0x0014b480 (twin FF3 0x0014b3f8)

CineProjector::CineProjector(int file, sceVu0FVECTOR *corners, int dark)
{
    this->corners = corners;
    bank  = -1;
    voice = CSND_BUF_PLAY_NO_ID;
    ready = 0;
    vtx   = nullptr;
    u     = nullptr;
    v     = nullptr;
    this->dark = dark;
    GetMapSky()->MovieSet(file, CINE_MOVIE_TBP, CINE_MOVIE_W, CINE_MOVIE_H, 0, dark);
}

// FF3 0x0014b628 (twin FF3 0x0014b508)

CineProjector::~CineProjector()
{
    CineReelSetActive(0);
    gra3dSetSpecialLight(0);
    if (ready != 0)
    {
        ready = 0;
        heapCtrlFree(GetSystemHeapWrkP(), v, 0, 0, 0);
        heapCtrlFree(GetSystemHeapWrkP(), u, 0, 0, 0);
        heapCtrlFree(GetSystemHeapWrkP(), vtx, 0, 0, 0);
        if (bank != -1 && voice != CSND_BUF_PLAY_NO_ID)
        {
            SndBufFadeStop(voice, 1);
            if (snd_utilAutoRelease(voice, bank) != AUTO_BD_ERR_OK)
                SndBankRelease(bank);
        }
    }
    GetMapSky()->Reset();
    dark    = 0;
    bank    = -1;
    voice   = CSND_BUF_PLAY_NO_ID;
    v       = nullptr;
    u       = nullptr;
    vtx     = nullptr;
    corners = nullptr;
}

// A projector with a reel object gets the rattle bank and turns that reel.
// FF3 0x0014bb98

void CineProjector::SetReel(int area, const char *name, s_char axis)
{
    if (name == nullptr)
        return;
    if (bank == -1)
        bank = SndBankNew(CINE_RATTLE_BD, CINE_RATTLE_BD + 1, -1);
    cine_reel_area = area;
    cine_reel_obj  = name;
    cine_reel_rot  = axis;
}

// Once the film's first frame and the rattle bank are in: the screen grid,
// the special light, the reel, and the rattle (at the reel when it is
// placed). 1 when done.
// FF3 0x0014b748

int CineProjector::Setup()
{
    sceVu0FVECTOR pos;
    SND_3D_SET    s3s;

    if (GetMapSky()->MovieIsReady() == 0)
        return 0;
    if (bank != -1 && SndBankIsReady(bank) == 0)
        return 0;

    CineScreenMakeGrid(corners, CINE_GRID_COLS, CINE_GRID_ROWS, &vtx, &u, &v);
    gra3dSetSpecialLight(1);
    CineReelSetActive(1);
    if (bank != -1)
    {
        if (CineReelGetPos(pos) != 0)
        {
            memset(&s3s, 0, sizeof(s3s));
            s3s.pos = pos;
            voice = SndBankPlay(bank, 0, 1, 1, 0x3200, 0x1000, 0, &s3s);
        }
        else
        {
            voice = SndBankPlay(bank, 0, 1, 1, 0x3200, 0x1000, 0, nullptr);
        }
    }
    ready = 1;
    return 1;
}

// One frame: skip (the player in a door) holds the light and the reel and
// skips the frame; pause holds the rattle. 1 at the film's end.
// FF3 0x0014b880

int CineProjector::Main(int skip, int pause)
{
    if (ready == 0)
        return 1;
    if (GetMapSky()->MovieIsReady() == 0)
        return 0;

    if (skip == 0)
    {
        gra3dUpdateSpecialLight();
        CineReelTurn();
    }
    if (voice != CSND_BUF_PLAY_NO_ID)
    {
        if (pause == 0)
            SndBufRestart(voice);
        else
            SndBufPause(voice);
    }
    return GetMapSky()->MovieProc(skip);
}

// The film's frame (2..98 % of the picture, cropping its edge) on the screen
// cells, blended at half alpha; the dark projector draws black at a quarter.
// FF3 0x0014b970

void CineProjector::Draw()
{
    GS_REG_SET         regs[5];
    PLAY_PSS_MPEG_INFO info;
    u_long             tex0;
    int                col, alpha;
    float              w, h;

    if (ready == 0)
        return;
    if (GetMapSky()->MovieSendFrame() == 0)
        return;

    regs[0].data = 0x44;            regs[0].reg = SCE_GS_ALPHA_1;      // (Cs - Cd) As + Cd
    regs[1].data = 0x161;           regs[1].reg = SCE_GS_TEX1_1;
    regs[2].data = 5;               regs[2].reg = SCE_GS_CLAMP_1;      // CLAMP both
    regs[3].data = 0x5000d;         regs[3].reg = SCE_GS_TEST_1;
    regs[4].data = 0xa000118;       regs[4].reg = SCE_GS_ZBUF_1;
    SetGsRegsCached(regs, 5, 1);
    playPssGetMpegInfo(&info);

    tex0 = SCE_GS_SET_TEX0(CINE_MOVIE_TBP, 3, SCE_GS_PSMCT32, 8, 7, 1, 0, CINE_MOVIE_TBP, 0, 0, 0, 1);
    col   = 0x7f;
    alpha = 0x40;
    if (dark != 0)
    {
        col   = 0;
        alpha = 0x20;
    }
    w = (float)info.width;
    h = (float)info.height;
    CineScreenDrawGrid(w * 0.02f, h * 0.02f, w * 0.96f, h * 0.96f,      // sdata 0x36da10 / 0x36da14
                       vtx, u, v, CINE_GRID_COLS, CINE_GRID_ROWS,
                       col, col, col, alpha, tex0, CINE_SCREEN_Z_OFS);
}

// ──────────────────────────────────────────────────────────────────────
// The projector in use
// ──────────────────────────────────────────────────────────────────────

// FF3 0x0014bf30

int CineGetProjectorAreaNo(void)
{
    return cine_area;
}

// Projector number of cine_area, -1. FF3 0x0014bf38

static int CineGetProjectorNo(void)
{
    int i;

    for (i = 0; i < CINE_PROJECTOR_NUM; i++)
    {
        if (cine_area == cine_area_tbl[i])
            return i;
    }
    return -1;
}

// FF3 0x0014bf78

void CineInit(void)
{
    int i;

    for (i = 0; i < CINE_PROJECTOR_NUM; i++)
        cine_film[i] = cine_film_init[i];
    cine_projector = nullptr;
    cine_frame     = -1;
    cine_state     = 0;
    CineMenuStateClear();
    CineFlagInit();
}

// FF3 0x0014bfd8

static int CineFilmProc(void)
{
    CineProjector *p;
    int            no, door;

    if (cine_projector == nullptr)
        return 0;

    if (cine_state == 1)
    {
        no = CineGetProjectorNo();
        cine_projector->SetReel(cine_area_tbl[no], cine_reel_name[no], cine_reel_axis[no]);
        if (cine_projector->Setup() == 0)
            return 1;
        cine_state = 2;
        CineFlagWatchStart(CineFilmTblFindFile(cine_film[no]));
        cine_frame = 0;
    }
    else if (cine_state != 2)
    {
        return 0;
    }

    door = ingame_wrk.plyr_wrk->mode == PLYR_MODE_DOOR;
    if (cine_projector->Main(door, cine_area != GetPlyrAreaNo()) == 0)
    {
        CineFlagWatchSeen();
        cine_frame++;
        return 1;
    }

    p = cine_projector;
    if (p != nullptr)
    {
        p->~CineProjector();                    // callcheck: CineProjectorDtor, operator_delete (the delete expression)
        operator_delete(p);
    }
    cine_projector = nullptr;
    cine_state     = 0;
    CineFlagWatchEnd();
    cine_frame = -1;
    return 0;
}

// FF3 0x0014c118

void CineDraw(void)
{
    if (cine_projector != nullptr && (u_char)eff_wrk.disp_flg != 0 && cine_state == 2)
        cine_projector->Draw();
}

// FF3 0x0014c160

int CineMain(void)
{
    return CineFilmProc() != 0;
}

// FF3 0x0014c180

void CineEnd(void)
{
    CineProjector *p = cine_projector;

    if (p != nullptr)
    {
        p->~CineProjector();                    // callcheck: CineProjectorDtor, operator_delete (the delete expression)
        operator_delete(p);
        cine_projector = nullptr;
    }
    cine_state = 0;
}

// FF3 0x0014c1c0

static int CineProjectorHasFilm(void)
{
    int no = CineGetProjectorNo();

    if (no < 0)
        return 0;
    return cine_film[no] >= 0;
}

// The film that was in comes out; the table lookups' results are unused.
// FF3 0x0014c208

static void CineFilmLoad(int film)
{
    CineFilmTblFindFile(CineTakeFilm());
    CineSetFilm(film);
    CineFilmTblFindFile(film);
}

// FF3 0x0014c248

int CineFilmPlayReq(int area, int film)
{
    int no;

    cine_area = area;
    if (film != -1)
        CineFilmLoad(film);
    if (CineProjectorHasFilm() == 0)
        return 0;
    if (cine_state != 0)
        return 1;

    no = CineGetProjectorNo();
    // callcheck: operator_new, CineProjectorCtor (the new expression; host size, not FF3's 0x20)
    cine_projector = new (operator_new(sizeof(CineProjector)))
        CineProjector(cine_film[no], cine_screen[no], cine_dark_tbl[no]);
    cine_state = 1;
    return 1;
}

// FF3 0x0014c310

int CineFilmIsPlay(void)
{
    return cine_state != 0;
}

// FF3 0x0014c320

void CineFilmStop(void)
{
    CineEnd();
}

// FF3 0x0014c338

static void CineSetFilm(int film)
{
    int no = CineGetProjectorNo();

    if (no >= 0)
        cine_film[no] = film;
}

// FF3 0x0014c378

int CineGetFilm(void)
{
    int no = CineGetProjectorNo();

    if (no < 0)
        return -1;
    return cine_film[no];
}

// FF3 0x0014c3b8

static int CineTakeFilm(void)
{
    int film = -1;
    int no   = CineGetProjectorNo();

    if (no >= 0)
    {
        film = cine_film[no];
        cine_film[no] = -1;
        CineFilmStop();
    }
    return film;
}

// The middle of the projector's screen; 0 off the projector areas.
// FF3 0x0014c408

static int CineGetScreenCenter(sceVu0FVECTOR out)
{
    int no = CineGetProjectorNo();
    int k;

    if (no < 0)
        return 0;
    for (k = 0; k < 4; k++)
        out[k] = (cine_screen[no][3][k] - cine_screen[no][0][k]) * 0.5f + cine_screen[no][0][k];
    return 1;
}

// FF3 0x0014c4a0

int CineGetProjectorSpotId(void)
{
    int no;

    if (cine_area != GetPlyrAreaNo())
        return 0x7fffffff;
    no = CineGetProjectorNo();
    if (no < 0)
        return 0x7fffffff;
    return cine_spot_tbl[no];
}

// FF3 0x0014c500

static int CineFilmReplace(int no, int from, int to)
{
    if (no == from && CineFlagCheck(no, CINE_FLAG_END) != 0)
        return to;
    return no;
}

// FF3 0x0014c550

int CineFilmSelect(int no)
{
    return CineFilmReplace(no, 6, 7);
}

// FF3 0x0014c570

int CineSaveGetProjector(void **addr)
{
    *addr = cine_film;
    return sizeof(cine_film);
}

// ──────────────────────────────────────────────────────────────────────
// Film flags
// ──────────────────────────────────────────────────────────────────────

// FF3 0x0014c588

static void CineFlagReset(int no)
{
    cine_flag[no] = 0;
}

// FF3 0x0014c5a0

static void CineFlagSet(int no, u_char mask)
{
    if ((cine_flag[no] & mask) == 0)
        cine_flag[no] |= mask;
}

// FF3 0x0014c5d0

static void CineFlagClear(int no, u_char mask)
{
    if ((cine_flag[no] & mask) != 0)
        cine_flag[no] &= ~mask;
}

// FF3 0x0014c608

int CineFlagCheck(int no, u_char mask)
{
    return (cine_flag[no] & mask) != 0;
}

// FF3 0x0014c628

static void CineFlagInit(void)
{
    int i;

    cine_watch    = nullptr;
    cine_watch_no = -1;
    for (i = 0; i < CINE_FILM_NUM; i++)
        CineFlagReset(i);
}

// FF3 0x0014c668

static void CineFlagWatchStart(int no)
{
    if (no < 0)
        return;
    cine_watch_no = no;
    cine_watch    = CineFilmTblGetPhotoWin(no);
    CineFlagSet(cine_watch_no, CINE_FLAG_PLAY);
}

// FF3 0x0014c6a0

static void CineFlagWatchSeen(void)
{
    if (cine_watch != nullptr && cine_watch[0] >= 0 && cine_watch[0] <= cine_frame)
        CineFlagSet(cine_watch_no, CINE_FLAG_SEEN);
}

// FF3 0x0014c6f0

void CineFlagWatchPhoto(void)
{
    sceVu0FVECTOR center;
    float         x, y;

    if (cine_watch == nullptr || cine_watch[0] < 0 || cine_watch[1] < 0)
        return;
    if (cine_watch[0] > cine_frame || cine_frame > cine_watch[1])
        return;
    if (CineGetScreenCenter(center) != 0 && FrameInsideChk(center, &x, &y) != 0)
        CineFlagSet(cine_watch_no, CINE_FLAG_PHOTO);
}

// FF3 0x0014c778

static void CineFlagWatchEnd(void)
{
    if (cine_watch == nullptr)
        return;
    CineFlagSet(cine_watch_no, CINE_FLAG_END);
    CineFlagClear(cine_watch_no, CINE_FLAG_PLAY);
    cine_watch = nullptr;
}

// FF3 0x0014c7b0

int CineSaveGetFlag(void **addr)
{
    *addr = cine_flag;
    return sizeof(cine_flag);
}
