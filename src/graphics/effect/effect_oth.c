/* ==========================================================================
 *  graphics/effect/effect_oth.c
 *
 *  FF3 0x1766c8..0x17f8b8 and 0x185908..0x186a88 (see effect_oth.h for the
 *  modules in the range). FF3's room glow pairs, FF2 effect_scr.c's head
 *  (InitEffectScr / EF, the blur -- SubBlur, FF3's fan blur SubBlurFan,
 *  SetBlur / RunBlur / CallBlur2 --, the focus, contrast and nega), the
 *  screen filter slots of EffectControl (FF2
 *  effect_scr.c's SetDeform / SubDeform's dispatch, SetFadeFrame /
 *  SubFadeFrame, SetBlackFilter, SetDither3, SetOverRap and FF3's empty
 *  SetForcusDepth), the deform kernels (FF2 SetDeform0 / 3..6, FF3's fan
 *  jitter, MakeScrDeformPacket), the CallNega2 / CallDeform2 requests, the
 *  monochrome switch, the brightness filter, the screen flash, the sky
 *  (SORA_PK4 load / texture / grid), the sound-bank list (dead in FF3 but
 *  for its init and stop-all), the map particles (map effects 200 / 201 and
 *  type 2), the screen dither (SubDither3 / SubDither4, DitherPatternUpdate,
 *  MakeRDither3), the screen flash wrappers, an unreferenced ripple kernel
 *  (SetDeformScrRipple), the menu backdrop of menu_file.c (EffMenuBg*),
 *  FF2's draw_distortion_particles2 and FF3's drips (EffDrip / EffDripFloor
 *  / EffDripMgr, map effects 300..303 / 400..403).

 *  FF3 differences from FF2 noted per function. PC: the sprite filters go
 *  through EffDispSprD2Direct (effect_sub.c), the
 *  rectangles through DispSqrRect / DispSqrD; the map particles' DIRECT
 *  sprites also go to the host as 3D effect quads (EffMapPartHostSprite).
 * ======================================================================== */

#include <new>                          // placement new on FF3's C++ heap (operator_new)
#include "common/reipan_stub.h"
#include "graphics/effect/effect_oth.h"

#include <string.h>                     // memset
#include <math.h>                       // fabsf, tanf, sqrtf, isnan

#include "graphics/effect/effect.h"     // EFFECT_CONT, SetEffects, ResetEffects, EffInKeepOutFlowCtrl, eff_wrk, EFFECT_MALLOC / FREE
#include "graphics/effect/effect_obj.h" // EffObjBufTblFind / Select / Get, EFF_OBJ_BUF
#include "graphics/graph2d/g2d_draw.h"  // SPRT_DAT, SPRT_DAT2, DISP_SPRT2, CopySprDToSpr2, SQAR_DAT, DISP_SQAR, CopySqrDToSqr, DispSqrD, LocalCopyLtoB / BtoL
#include "graphics/graph2d/g2d_debug.h" // DispSqrRect
#include "graphics/graph2d/tim2.h"      // Tim2PackSend
#include "graphics/graph3d/gra3d.h"     // gra3dGetCamera, gra3dcamGetPosition, _GetClipVolumeV, gra3dIsMonotoneDrawEnable, gra3dMonotoneDrawEnable
#include "graphics/graph3d/gra3dMisc.h" // gra3dPrelight
#include "graphics/graph3d/g3dCamera.h" // g3dCalcViewScreenMatrixPerspective
#include "graphics/gs_reg.h"            // GS_REG_SET, SetGsRegsCached
#include "common/vu0math.h"             // Vu0Sinf, GetTrgtRot
#include "common/SingleLinkList.h"      // SingleLinkListInit / AddEnd / Remove / CellBodyPtr
#include "system/os/system.h"           // GetSysWrk, GetPALMode, GetFrameTimeScale
#include "system/os/eecdvd.h"           // LoadReq, IsLoadEnd
#include "system/eeiop/cddat.h"         // GetFileSize
#include "system/eeiop/cd_file_dat.h"   // SORA_PK4
#include "outgame/option.h"             // GetOptionWrk
#include "ingame/ingame_wrk.h"          // ingame_wrk.b03 (the world), ingame_wrk.plyr_wrk
#include "ingame/plyr/player.h"         // PLYR_WRK, PLYR_FLG_800000
#include "ingame/camera/map_camera.h"   // GetApproachCameraCrossFade, ApproachCameraCrossFadeSW
#include "ingame/map/MapLoad.h"         // MapLoadGetBuffID, MapLoadGetRoomArea
#include "ingame/story_door.h"          // MapAreaNoGet
#include "system/eeiop/snd_buffer.h"  // SndBufStop / FadeStop / Pause / Restart / IsPlaying
#include "system/eeiop/snd_bank.h"      // SndBankNew / IsReady / Release / IsLoopSnd / Play, SND_3D_SET
#include "common/cpp_heap.h"            // operator_new, operator_delete
#include "reipan/reipan_memory.h"       // PC: ReiPan_GetHostPointer
#include "reipan/gs/reipan_gs_c.h"      // PC: ReiPan_GsUpload
#include "sdk/libgraph.h"               // sceGsSetDefLoadImage, SCE_GS_PSMT8H
#include "reipan/os/reipan_rand.h"      // ReiPan_Rand, REIPAN_RAND_MAXF
#include "reipan/rendering/reipan_graph3d.h" // PC: ReiPan_Graph3dDrawEffectQuad
#include "reipan/rendering/reipan_renderer.h" // PC: ReiPan_RendererDrawTexturedQuadClamp, SetGs2dDepth, the follow spans
#include "graphics/effect/effect_sub.h" // CamChangeCheck, EffDispSprD2Direct, EffImageHalf32
#include "graphics/effect/effect_scrsaver.h" // EffFilmNoiseDraw, EffFilmShakeDraw (kind 0x11)

// ──────────────────────────────────────────────────────────────────────
// Callees in runs whose file is not settled yet (unassigned@ in tus.json).
// Move each block to its real header when that run is defined.
// ──────────────────────────────────────────────────────────────────────

void WorldToScreenPos(float *pos, float *sx, float *sy);   // FF3 0x00180710: through gra3dGetCamera's matrices

// ──────────────────────────────────────────────────────────────────────
// State
// ──────────────────────────────────────────────────────────────────────

EFF_DEFORM_WRK eff_deform;              // sdata 0x372f70
EFF_DITHER_WRK eff_dither;              // sdata 0x372f78
short overlap_passflg[2];               // sdata 0x36eb98

// SetOverRap's last camera position and the cross-fade's alpha.
static float overlap_cx;                // sdata 0x36ef00
static float overlap_cy;                // sdata 0x36ef04
static float overlap_cz;                // sdata 0x36ef08
static float overlap_alp;               // sdata 0x36ef0c

// The brightness filter's (dark, middle, light) per world (ingame_wrk.b03).
static const u_char brightness_tbl[2][3] = {    // sdata 0x372a28
    { 0x14, 0x55, 0x8c },
    { 0x14, 0x55, 0x78 },
};

// The screen flash (data 0x2f99e0).
struct SCREEN_FLASH_WRK                 /* 0x10 */
{
    /* 0x00 */ int    mode;             // 0 off, 1 fade out, 2 fade in and hold
    /* 0x04 */ int    time;
    /* 0x08 */ int    cnt;
    /* 0x0c */ u_char r, g, b;
    /* 0x0f */ u_char alpha;
};
static_assert(sizeof(SCREEN_FLASH_WRK) == 0x10, "SCREEN_FLASH_WRK: data 0x2f99e0..0x2f99f0");
static SCREEN_FLASH_WRK screen_flash;   // data 0x2f99e0

// The sky.
static EFF_SORA_DAT sora_dat = {        // data 0x2f99f0
    { 0x4541c000, 0x3f8cccc4, 0x00000000, 0xffffffb5, 0x000000ff,
      0x3ff5c27e, 0x00000025, 0x00000037, 0x00000033, 0x0000004b },
    -3000.0f, 3.9999068f, { 1.0f, 0.0f, 0.0f }, 3500, 0x80, 0x80, 0x80, 0x5a,
};
// SORA_PK4's two pictures (data 0x354a20).
static const SPRT_DAT sora_sprt[2] = {
    { 0x20058005dd30abc0ULL, 0, 0, 0x80, 0x80, 0, 0, 0, 0x80, 0, 0 },
    { 0x20058085dd30abc0ULL, 0, 0, 0x80, 0x80, 0, 0, 0, 0x80, 0, 0 },
};
static EFF_SORA_DAT *sora_cur;          // sdata 0x36efa0: the sky drawn this frame, nullptr: none
static void  *sora_pk;                  // sdata 0x36efa4: SORA_PK4 (EFFECT_MALLOC)
static int    sora_load_id = -1;        // sdata 0x36efa8
static u_long sora_tex0;                // sdata 0x36efb0: the picture EffSoraTexSend sent
static int    sora_b8;                  // sdata 0x36efb8: cleared by EffSoraLoadReq, read nowhere
static int    sora_bc;                  // sdata 0x36efbc: the same
static int    sora_ready;               // sdata 0x36efc0: 1 once SORA_PK4 is in
static sceVu0FMATRIX sora_view_screen;  // bss 0x3ded30: the sky's own view -> screen matrix
static int    sora_scroll;              // sdata 0x36efc8: the U scroll's frame count, 0..period-1
// The grid's corners in the sky's frame (data 0x341d20; y is the plane,
// written each frame from EFF_SORA_DAT.dist).
static sceVu0FVECTOR sora_rect[4] = {
    { -24000.0f, 0.0f, -24000.0f, 1.0f },
    {  24000.0f, 0.0f, -24000.0f, 1.0f },
    {  24000.0f, 0.0f,  24000.0f, 1.0f },
    { -24000.0f, 0.0f,  24000.0f, 1.0f },
};
#define SORA_GRID_N 23                  // vertices per side (EffSoraDrawSub's 0x17, 0x17)
static sceVu0IVECTOR sora_grid[SORA_GRID_N * SORA_GRID_N];  // bss 0x4229c0: projected vertices, x -1 = culled

// A cell of the bank list 0x3ded70.
struct SND_BANK_ENT                     /* 0xc */
{
    /* 0x00 */ int id;                  // the bank's file id (SndBankNew(id, id - 1, -1))
    /* 0x04 */ int bank;                // its SndBankNew handle, -1 none
    /* 0x08 */ int ready;               // 1 once SndBankIsReady
};
static_assert(sizeof(SND_BANK_ENT) == 0xc, "SND_BANK_ENT: the list's cells are 0xc");

// A play request of the sound-bank list 0x3ded90 (SndBankListPlay).
struct SND_BANK_REC                     /* 0x30 */
{
    /* 0x00 */ sceVu0FVECTOR pos;       // the 3D source's position (has_pos)
    /* 0x10 */ int    has_pos;
    /* 0x14 */ int    id;               // the bank's file id
    /* 0x18 */ int    no;               // the sound in the bank
    /* 0x1c */ int    effect;           // SndBankPlay's third argument
    /* 0x20 */ int    arg;              // read nowhere
    /* 0x24 */ int    buf;              // the playing sound buffer, -1 waiting for the bank
    /* 0x28 */ int    key;              // SndBankListStopKey's
    /* 0x2c */ int    _2c;
};
static_assert(sizeof(SND_BANK_REC) == 0x30, "SND_BANK_REC: the list's cells are 0x30");
static_assert(offsetof(SND_BANK_REC, has_pos) == 0x10, "SND_BANK_REC.has_pos");
static_assert(offsetof(SND_BANK_REC, id) == 0x14, "SND_BANK_REC.id");
static_assert(offsetof(SND_BANK_REC, buf) == 0x24, "SND_BANK_REC.buf");
static_assert(offsetof(SND_BANK_REC, key) == 0x28, "SND_BANK_REC.key");

// The sound-bank lists and the map particle list.
static SINGLE_LINK_LIST snd_bank_list_a;    // bss 0x3ded70 (cells 0xc)
static SINGLE_LINK_LIST snd_bank_list_b;    // bss 0x3ded80 (cells 4)
static SINGLE_LINK_LIST snd_bank_list_c;    // bss 0x3ded90 (cells 0x30)
static SINGLE_LINK_LIST eff_map_part_list;  // bss 0x3deda0 (cells 0x70)

// The menu backdrop's state (bss 0x3dedb0, cleared by EffMapPartInit):
// the game camera EffMenuBgStart saved and the fog layers' UV phases.
struct EFF_MENU_BG                      /* 0x50 */
{
    /* 0x00 */ sceVu0FVECTOR pos;       // gra3dcamGetPosition
    /* 0x10 */ sceVu0FVECTOR target;    // gra3dcamGetTarget
    /* 0x20 */ float roll;
    /* 0x24 */ float fov;
    /* 0x28 */ int   cnt;               // Start / End nesting, never below 0
    /* 0x2c */ float u[3];              // per layer, 0..1
    /* 0x38 */ float v[3];
    /* 0x44 */ u_char _44[0xc];
};
static_assert(sizeof(EFF_MENU_BG) == 0x50, "EFF_MENU_BG: bss 0x3dedb0..0x3dee00");
static_assert(offsetof(EFF_MENU_BG, cnt) == 0x28, "EFF_MENU_BG.cnt");
static_assert(offsetof(EFF_MENU_BG, u) == 0x2c, "EFF_MENU_BG.u");
static_assert(offsetof(EFF_MENU_BG, v) == 0x38, "EFF_MENU_BG.v");
static EFF_MENU_BG eff_menu_bg;         // bss 0x3dedb0

// The map particles' per-type callbacks (data 0x2f9a40..0x2f9a90, four
// rows of four; type 3 is empty).
static void EffMapPart0Setup(EFF_MAP_PART *mp);
static void EffMapPart1Setup(EFF_MAP_PART *mp);
static void EffMapPart2Setup(EFF_MAP_PART *mp);
static void EffMapPart0SetPos(EFF_MAP_PARTICLE *part, EFF_MAP_PART *mp, const float *ofs);
static void EffMapPart1SetPos(EFF_MAP_PARTICLE *part, EFF_MAP_PART *mp, const float *ofs);
static void EffMapPart2SetPos(EFF_MAP_PARTICLE *part, EFF_MAP_PART *mp, const float *ofs);
static void EffMapPart0ParticleInit(EFF_MAP_PARTICLE *part, EFF_MAP_PART *mp, const float *ofs);
static void EffMapPart1ParticleInit(EFF_MAP_PARTICLE *part, EFF_MAP_PART *mp, const float *ofs);
static void EffMapPart2ParticleInit(EFF_MAP_PARTICLE *part, EFF_MAP_PART *mp, const float *ofs);
static void EffMapPart0Land(EFF_MAP_PARTICLE *part, EFF_MAP_PART *mp);
static void EffMapPart1Land(EFF_MAP_PARTICLE *part, EFF_MAP_PART *mp);
static void EffMapPart2Land(EFF_MAP_PARTICLE *part, EFF_MAP_PART *mp);
static void EffMapPartParticleRePos(EFF_MAP_PARTICLE *part, EFF_MAP_PART *mp);
static void EffMapPartParticleInit(EFF_MAP_PARTICLE *part, EFF_MAP_PART *mp);
static void EffMapPartParticleSet(EFF_MAP_PARTICLE *part, EFF_MAP_PART *mp);
static int  EffMapPartSclCheck(const float *pos, const EFF_MAP_SCL *scl);
static int  EffMapPartFloorLevelUpdate(EFF_MAP_PART *mp);
static int  EffMapPartParticleStep(EFF_MAP_PARTICLE *part, EFF_MAP_PART *mp);

static void (*const eff_map_part_setup[4])(EFF_MAP_PART *) = {          // data 0x2f9a40
    EffMapPart0Setup, EffMapPart1Setup, EffMapPart2Setup, nullptr,
};
static void (*const eff_map_part_set_pos[4])(EFF_MAP_PARTICLE *, EFF_MAP_PART *, const float *) = {   // data 0x2f9a50
    EffMapPart0SetPos, EffMapPart1SetPos, EffMapPart2SetPos, nullptr,
};
static void (*const eff_map_part_init[4])(EFF_MAP_PARTICLE *, EFF_MAP_PART *, const float *) = {      // data 0x2f9a60
    EffMapPart0ParticleInit, EffMapPart1ParticleInit, EffMapPart2ParticleInit, nullptr,
};
static void (*const eff_map_part_land[4])(EFF_MAP_PARTICLE *, EFF_MAP_PART *) = {   // data 0x2f9a70
    EffMapPart0Land, EffMapPart1Land, EffMapPart2Land, nullptr,
};
static void (*const eff_map_part_draw[4])(float, EFF_MAP_PART *) = {   // data 0x2f9a80
    EffMapPartDraw, EffMapPartDraw, EffMapPartDraw, nullptr,
};

// ──────────────────────────────────────────────────────────────────────
// The room glow pairs (FF3-only)
// ──────────────────────────────────────────────────────────────────────

// One map buffer's pair: [0] normally, [1] while the camera is up.
struct ROOM_GLOW_DAT                    /* 0x20 */
{
    /* 0x00 */ ROOM_GLOW_COL col[2];
};
static_assert(sizeof(ROOM_GLOW_DAT) == 0x20, "ROOM_GLOW_DAT: RoomGlowDatSize / the 8-word copies");

// Map buffers 0 / 1, then the default pair (0x3d4730).
static ROOM_GLOW_DAT room_glow_dat[3];  // bss 0x3d46f0

// FF3 0x001766c8.
ROOM_GLOW_COL *RoomGlowDatDefault(void)
{
    return room_glow_dat[2].col;
}

// FF3 0x001766d8.
void RoomGlowInit(void)
{
    memset(room_glow_dat, 0, sizeof(room_glow_dat));
}

// FF3 0x00176700.
void RoomGlowRelease(void)
{
    memset(room_glow_dat, 0, sizeof(room_glow_dat));
}

// FF3 0x00176728.
int RoomGlowDatSize(void)
{
    return sizeof(ROOM_GLOW_DAT);
}

// FF3 0x00176730.
void RoomGlowDatClear(int buff)
{
    if ((u_int)buff < 2)
        room_glow_dat[buff] = *(ROOM_GLOW_DAT *)RoomGlowDatDefault();
}

// FF3 0x001767a8.
void RoomGlowDatSet(int buff, const void *dat)
{
    if ((u_int)buff < 2)
    {
        if (dat == nullptr)
        {
            RoomGlowDatClear(buff);
            return;
        }
        room_glow_dat[buff] = *(const ROOM_GLOW_DAT *)dat;
    }
}

// FF3 0x00176830.
ROOM_GLOW_COL *RoomGlowDatGet(int buff)
{
    if ((u_int)buff > 1)
        return RoomGlowDatDefault();
    return room_glow_dat[buff].col;
}

// ──────────────────────────────────────────────────────────────────────
// The blur, focus, contrast and nega (FF2 effect_scr.c)
// ──────────────────────────────────────────────────────────────────────

EFF_BLUR  eff_blur;                     // bss 0x3d4750
EFF_FOCUS eff_focus;                    // bss 0x3d4778

// The half-size copy of the frame (320 x 224 at 0x38e0, TBW 5, 512 x 256;
// sdata 0x36ebd8 / 0x36ec08). RunBlur refreshes it every frame,
// EffectOthBufCopy keeps it in EE 0x117b000 and the blurs restore it.
#define BLUR_HALF_TEX0  0x20000002241178e0ULL
// The frame buffers (TBP 0 / 0x1180, 640 x 448, TBW 10): | the TBP.
#define SCR_FRAME_TEX0  0x2000000268128000ULL
// The half copy at 0x2bc0 SubNega blends back (sdata 0x36ec90).
#define NEGA_HALF_TEX0  0x2000000264116bc0ULL

// FF3's fan blur: the centre and a 29-point ring round the screen's edge
// (the last point closes it at (0, 0)), each with its alpha factor (0.09
// at the centre to 1.0 at a corner) and its UV on the half copy (12.4).
struct BLUR_FAN_VTX                     /* 0x14 */
{
    /* 0x00 */ float x;                 // 640 x 448 screen
    /* 0x04 */ float y;
    /* 0x08 */ float rate;
    /* 0x0c */ int   u;                 // texels * 16, 1..318
    /* 0x10 */ int   v;                 // texels * 16, 1..222
};
static_assert(sizeof(BLUR_FAN_VTX) == 0x14, "BLUR_FAN_VTX: BlurFanInit steps 5 words");

#define BLUR_FAN_NUM 30

static BLUR_FAN_VTX blur_fan[BLUR_FAN_NUM];     // bss 0x3d94e8
static int blur_fan_a[BLUR_FAN_NUM];            // bss 0x3d9740: this draw's vertex alphas
static int blur_fan_xy[BLUR_FAN_NUM][2];        // bss 0x3d97b8: this draw's vertices (GS 12.4)

// The fan's draw state (data 0x354860): normal blend, bilinear, REPEAT,
// tests off, Z at 0x118 (PSMZ16S) masked.
static const GS_REG_SET blur_fan_regs[5] = {
    { 0x44ULL,         0x42 },          // ALPHA_1
    { 0x161ULL,        0x14 },          // TEX1_1
    { 0,               0x08 },          // CLAMP_1
    { 0x30003ULL,      0x47 },          // TEST_1
    { 0x10a000118ULL,  0x4e },          // ZBUF_1
};

// The fan blur's table. FF3 0x001769c0 (FF3-only; InitEffectScr). The
// square roots are VU0 vsqrt, no calls.
static void BlurFanInit(void)
{
    float inv = 1.0f / sqrtf(152576.0f);    // 1 / |(320, 224)|
    int   i;

    for (i = 0; i < BLUR_FAN_NUM; i++)
    {
        BLUR_FAN_VTX *v = &blur_fan[i];
        float dx, dy;
        int   tu, tv;

        if (i == 0)
        {
            v->x = 320.0f;
            v->y = 224.0f;
        }
        else if (i < 9)                 // the top edge, left to right
        {
            v->y = 0.0f;
            v->x = (float)(i - 1) * 640.0f * 0.125f;
        }
        else if (i < 15)                // the right edge, down
        {
            v->x = 640.0f;
            v->y = ((float)(i - 9) * 448.0f) / 6.0f;
        }
        else if (i < 23)                // the bottom edge, right to left
        {
            v->y = 448.0f;
            v->x = 640.0f - (float)(i - 15) * 640.0f * 0.125f;
        }
        else if (i < 29)                // the left edge, up
        {
            v->x = 0.0f;
            v->y = 448.0f - ((float)(i - 23) * 448.0f) / 6.0f;
        }
        else
        {
            v->x = 0.0f;
            v->y = 0.0f;
        }

        tu = (int)(v->x * 0.5f + 0.5f);
        tv = (int)(v->y * 0.5f + 0.5f);
        if (tu < 1)
            tu = 1;
        else if (tu > 318)
            tu = 318;
        if (tv < 1)
            tv = 1;
        else if (tv > 222)
            tv = 222;
        v->v = tv << 4;
        v->u = tu << 4;

        dx = v->x - 320.0f;
        dy = v->y - 224.0f;
        v->rate = sqrtf(dy * dy + dx * dx) * 0.90999997f * inv + 0.089999996f;
    }
}

// PC: the TRIFAN as a triangle list through the renderer (nothing walks
// the DIRECT packet); the half copy is served from its capture slot.
static void BlurFanHostDraw(void)
{
    static float  xy[(BLUR_FAN_NUM - 2) * 3 * 2];
    static float  uv[(BLUR_FAN_NUM - 2) * 3 * 2];
    static u_char rgba[(BLUR_FAN_NUM - 2) * 3 * 4];
    u_long tex0 = BLUR_HALF_TEX0;
    int    n = 0;
    int    k, c;

    for (k = 1; k < BLUR_FAN_NUM - 1; k++)
    {
        const int tri[3] = { 0, k, k + 1 };

        for (c = 0; c < 3; c++)
        {
            int i = tri[c];

            xy[n * 2 + 0] = (float)blur_fan_xy[i][0] / 16.0f - 1728.0f;
            xy[n * 2 + 1] = (float)blur_fan_xy[i][1] / 16.0f - 1824.0f;
            uv[n * 2 + 0] = (float)blur_fan[i].u / 16.0f;
            uv[n * 2 + 1] = (float)blur_fan[i].v / 16.0f;
            rgba[n * 4 + 0] = 0x80;
            rgba[n * 4 + 1] = 0x80;
            rgba[n * 4 + 2] = 0x80;
            rgba[n * 4 + 3] = (u_char)blur_fan_a[i];
            n++;
        }
    }
    ReiPan_RendererDrawTexturedTriangles2D((const sceGsTex0 *)&tex0, xy, uv, rgba, nullptr, n, 0, 0, 0);
}

// SetBlur type 4: the half copy drawn as the fan, scaled by (scw, sch) and
// turned by rot - 180 degrees round the screen centre, each vertex's alpha
// alp * rate * a random 0.49..1.0 -- a flickering blur that grows towards
// the edges. cx, cy and type are not read.
// FF3 0x00176c48 (FF3-only).
// callcheck: rand (ReiPan_Rand)
static void SubBlurFan(float scw, float sch, float rot, float cx, float cy, int type, u_char alp)
{
    float   r = ((rot - 180.0f) * 3.1415927f) / 180.0f;
    float   s, c;
    u_long *p;
    u_int  *q;
    int     i;

    (void)cx;
    (void)cy;
    (void)type;
    s = Vu0Sinf(r);
    c = Vu0Cosf(r);
    for (i = 0; i < BLUR_FAN_NUM - 1; i++)
    {
        float xs = (blur_fan[i].x - 320.0f) * scw;
        float ys = (blur_fan[i].y - 224.0f) * sch;
        float va = (float)alp * blur_fan[i].rate;

        blur_fan_xy[i][0] = (int)((xs * c - ys * s) + 2048.5f) << 4;
        blur_fan_xy[i][1] = (int)(ys * c + xs * s + 2048.5f) << 4;
        blur_fan_a[i] = (int)(va * (((float)ReiPan_Rand() / REIPAN_RAND_MAXF) * 0.51f + 0.49f));
    }
    blur_fan_xy[BLUR_FAN_NUM - 1][1] = blur_fan_xy[1][1];
    blur_fan_a[BLUR_FAN_NUM - 1] = blur_fan_a[1];
    blur_fan_xy[BLUR_FAN_NUM - 1][0] = blur_fan_xy[1][0];

    LocalCopyBtoLAdrs(1, 0x117b000, 0x38e0);
    SetGsRegsCached(blur_fan_regs, 5, 1);

    p = (u_long *)StartDmaDirectTrans();
    p[1] = 0xe;                         // A+D
    p[0] = 0x1000000000008002ULL;
    p[3] = 0x3f;                        // TEXFLUSH
    p[2] = 0;
    p[4] = BLUR_HALF_TEX0;              // sdata 0x36ebd8
    p[5] = 6;                           // TEX0_1
    p[7] = 0x431;                       // RGBAQ, UV, XYZ2
    p[6] = 0x30aec0000000801eULL;       // 30 vertices, TRIFAN, IIP / TME / ABE / FST
    q = (u_int *)(p + 8);
    for (i = 0; i < BLUR_FAN_NUM; i++)
    {
        q[0]  = 0x80;
        q[1]  = 0x80;
        q[2]  = 0x80;
        q[3]  = (u_int)blur_fan_a[i];
        q[4]  = (u_int)blur_fan[i].u;
        q[5]  = (u_int)blur_fan[i].v;
        q[6]  = 0;
        q[7]  = 0;
        q[8]  = (u_int)blur_fan_xy[i][0];
        q[9]  = (u_int)blur_fan_xy[i][1];
        q[10] = 0;
        q[11] = (i < 2) ? 0x8000 : 0;   // ADC: the fan's first two vertices
        q += 12;
    }
    EndDmaDirectTrans((qword *)q);

    BlurFanHostDraw();
}

// FF2 InitEffectScr; FF3 0x00176868: ScreenFlashReq is FF2's SetParam,
// and the fan blur's table replaces ScreenSaverInit.
void InitEffectScr(void)
{
    eff_dither.colmx = 0x80;
    eff_dither.alpmx = 0x40;
    eff_dither.remake = 1;
    DitherPatternUpdate(0x40, 0x80);
    ScreenFlashReq(0, 0, 0, 0, 0, 0);

    eff_deform.type = 0;
    eff_blur.cy = 224.0f;
    eff_deform.init = 1;
    eff_focus.flow = 3;
    eff_blur.flow = 3;
    eff_focus.max = 0;
    eff_deform.pass = 0;
    eff_blur.scl = 1.0f;
    eff_blur.cnt = 0;
    eff_blur.in = 0;
    eff_blur.rot = 180.0f;
    eff_blur.keep = 0;
    eff_blur.out = 0;
    eff_blur.cx = 320.0f;
    eff_deform.otype = 0;
    eff_blur.alp = 0;
    eff_focus.cnt = 0;
    overlap_passflg[1] = 0;
    eff_focus.in = 0;
    eff_focus.keep = 0;
    eff_focus.out = 0;
    overlap_passflg[0] = 0;

    BlurFanInit();
}

// FF2 InitEffectScrEF; FF3 0x00176978 (no ScreenSaverMain).
void InitEffectScrEF(void)
{
    overlap_passflg[1] = overlap_passflg[0];
    overlap_passflg[0] = 0;
    if (eff_deform.pass == 0)
        eff_deform.init = 1;
    eff_deform.pass = 0;
    eff_deform.otype = eff_deform.type;
}

// FF3 0x001769b8: empty.
void EffectScrRelease(void)
{
}

// FF2 SubBlur; FF3 0x00176fd8: FF3 takes the scale per axis and turns
// round (cx, cy) where FF2 used the screen centre, and its colour types
// moved up one (FF2: 1 dark, 2 bright).
void SubBlur(float scw, float sch, float rot, float cx, float cy, int type, u_char alp, int src)
{
    SPRT_DAT2  sd_half  = { 0, 0.5f, 0.5f, 319.5f, 223.5f, 640.0f, 448.0f, 0.0f, 0.0f, 0, 0x80 };
    SPRT_DAT2  sd_frame = { 0, 1.0f, 1.0f, 639.0f, 447.0f, 640.0f, 448.0f, 0.0f, 0.0f, 0, 0x80 };
    DISP_SPRT2 ds;
    u_char     col;

    col = 0x80;
    if (type == 2)
        col = 0x78;
    else if (type == 3)
        col = 0x88;

    if (src == 0)
    {
        CopySprDToSpr2(&ds, &sd_half);
        LocalCopyBtoLAdrs(1, 0x117b000, 0x38e0);
        ds.tex0 = BLUR_HALF_TEX0;
    }
    else
    {
        CopySprDToSpr2(&ds, &sd_frame);
        ds.tex0 = (u_long)(((GetSysWrk()->count + 1) & 1) * 0x1180) | SCR_FRAME_TEX0;
    }

    ds.csx  = cx + -0.5f;
    ds.csy  = cy + -0.5f;
    ds.rot  = rot - 180.0f;
    ds.tex1 = 0x161;
    ds.pri  = 0xa0;
    ds.z    = 0xfff00;
    ds.crx  = 320.0f;
    ds.cry  = 224.0f;
    ds.y    = -0.5f;
    ds.x    = -0.5f;
    ds.scw  = scw;
    ds.sch  = sch;
    ds.r    = col;
    ds.g    = col;
    ds.b    = col;
    ds.alp  = alp;
    EffDispSprD2Direct(&ds);
}

// Kind 3. FF2 SetBlur; FF3 0x001771f8: the alpha is the record's uc8[3]
// (FF2: *pnt[0]), type 0 draws nothing, type 4 is the fan blur, and
// eff_wrk.blur_off switches it off.
void SetBlur(EFFECT_CONT *ec)
{
    if (eff_wrk.blur_off == 0 && ec->dat.uc8[3] != 0)
    {
        u_char type = ec->dat.uc8[2];
        float  scl  = (float)ec->dat.ui32[2] / 1000.0f;

        if (type != 0 && type < 4)
            SubBlur(scl, scl, (float)ec->dat.ui32[3] / 10.0f, ec->fw[0], ec->fw[1], type,
                    ec->dat.uc8[3], 0);
        else if (type == 4)
            SubBlurFan(scl, scl, (float)ec->dat.ui32[3] / 10.0f, ec->fw[0], ec->fw[1], 4,
                       ec->dat.uc8[3]);
    }
    if ((ec->dat.uc8[1] & 1) != 0)
        ResetEffects(ec);
}

// Slot 3, every pass 0. FF2 RunBlur (the envelope CallBlur2 arms); FF3
// 0x00177380 ends every frame by halving the frame into 0x38e0, the copy
// the blurs and the haze sample.
void RunBlur(EFFECT_CONT *ec)
{
    float s;

    (void)ec;
    if (eff_blur.flow == 1)
    {
        eff_blur.cnt++;
        s = Vu0Sinf(1.5707961f);
        SubBlur(eff_blur.scl, eff_blur.scl, eff_blur.rot, eff_blur.cx, eff_blur.cy, 0,
                (u_char)(int)(s * (float)eff_blur.alp), 0);
        if (eff_blur.keep <= eff_blur.cnt)
        {
            eff_blur.flow = 2;
            eff_blur.cnt = 0;
            if (eff_blur.out == 0)
                eff_blur.flow = 3;
        }
    }
    else if (eff_blur.flow == 0)
    {
        eff_blur.cnt++;
        if (eff_blur.in != 0)
        {
            s = Vu0Sinf(((((float)eff_blur.cnt * 90.0f) / (float)eff_blur.in) * 3.1415927f) / 180.0f);
            SubBlur(eff_blur.scl, eff_blur.scl, eff_blur.rot, eff_blur.cx, eff_blur.cy, 0,
                    (u_char)(int)(s * (float)eff_blur.alp), 0);
        }
        if (eff_blur.in <= eff_blur.cnt)
        {
            if (eff_blur.keep == 0)
            {
                eff_blur.flow = 2;
                if (eff_blur.out == 0)
                    eff_blur.flow = 3;
            }
            else
            {
                eff_blur.flow = 1;
            }
            eff_blur.cnt = 0;
        }
    }
    else if (eff_blur.flow == 2)
    {
        eff_blur.cnt++;
        if (eff_blur.out != 0)
        {
            s = Vu0Sinf(((((float)eff_blur.cnt * 90.0f) / (float)eff_blur.out + 90.0f) * 3.1415927f) / 180.0f);
            SubBlur(eff_blur.scl, eff_blur.scl, eff_blur.rot, eff_blur.cx, eff_blur.cy, 0,
                    (u_char)(int)(s * (float)eff_blur.alp), 0);
        }
        if (eff_blur.out <= eff_blur.cnt)
        {
            eff_blur.cnt = 0;
            eff_blur.flow = 3;
        }
    }

    LocalCopyLtoL(1, (int)(GetSysWrk()->count & 1) * 0x1180, 0x38e0);
}

// FF3 0x00177780 (FF3-only).
void EffectOthBufCopy(void)
{
    LocalCopyLtoBAdrs(1, 0x117b000, 0x38e0);
}

// FF2 CallBlur2 with the centre; FF3 0x001777a8, unreferenced.
void CallBlur2(int in, int keep, int out, u_char alp, float scl, float rot, float cx, float cy)
{
    u_int flow;

    if (in > 0)
        flow = 0;
    else if (keep > 0)
        flow = 1;
    else if (out > 0)
        flow = 2;
    else
        flow = 3;

    eff_blur.flow = flow;
    eff_blur.cy   = cy;
    eff_blur.in   = (u_int)in;
    eff_blur.keep = (u_int)keep;
    eff_blur.out  = (u_int)out;
    eff_blur.alp  = alp;
    eff_blur.scl  = scl;
    eff_blur.rot  = rot;
    eff_blur.cx   = cx;
    eff_blur.cnt  = 0;
}

// FF2 SubFocus; FF3 0x00177800 takes the sprite's rectangle and an offset.
void SubFocus(float u1, float v1, float u2, float v2, float w, float h, float x, float y, int ef)
{
    SPRT_DAT2  sd;
    DISP_SPRT2 ds;
    float      hw;
    float      x0, y0, x1, y1;

    if (ef <= 0)
        return;

    hw = (float)ef / 40.0f;
    sd.tex0  = 0;
    sd.u1    = u1;
    sd.v1    = v1;
    sd.u2    = u2;
    sd.v2    = v2;
    sd.w     = w;
    sd.h     = h;
    sd.x     = 0.0f;
    sd.y     = 0.0f;
    sd.pri   = 0xa0;
    sd.alpha = 0x80;
    CopySprDToSpr2(&ds, &sd);
    ds.tex0 = (u_long)((GetSysWrk()->count & 1) * 0x1180) | SCR_FRAME_TEX0;
    if (ef < 11)
        ds.alp = (u_char)(ef << 2);
    else
        ds.alp = 40;
    ds.z = 0xfff00;

    x0 = (-0.5f - hw) + x;
    y0 = (-0.5f - hw) + y;
    x1 = (hw + -0.5f) + x;
    y1 = (hw + -0.5f) + y;

    ds.x = x0;
    ds.y = y0;
    EffDispSprD2Direct(&ds);
    ds.x = x1;
    ds.y = y0;
    EffDispSprD2Direct(&ds);
    ds.x = x0;
    ds.y = y1;
    EffDispSprD2Direct(&ds);
    ds.x = x1;
    ds.y = y1;
    EffDispSprD2Direct(&ds);
}

// Kind 5. FF2 SetFocus; FF3 0x00177998.
void SetFocus(EFFECT_CONT *ec)
{
    if (ec->dat.uc8[2] != 0 && eff_focus.flow == 3)
    {
        SubFocus(0.1f, 0.1f, 639.89996f, 447.9f, 640.0f, 448.0f, 0.0f, 0.0f, ec->dat.uc8[2]);
        if ((ec->dat.uc8[1] & 1) != 0)
            ResetEffects(ec);
    }
}

// Slot 5, every pass 0. FF2 RunFocus; FF3 0x00177a48.
void RunFocus(EFFECT_CONT *ec)
{
    float s;

    if (eff_focus.flow == 1)
    {
        eff_focus.cnt++;
        s = Vu0Sinf(1.5707961f);
        SubFocus(0.1f, 0.1f, 639.89996f, 447.9f, 640.0f, 448.0f, 0.0f, 0.0f,
                 (int)ec->dat.uc8[2] + (int)(s * (float)eff_focus.max));
        if (eff_focus.keep <= eff_focus.cnt)
        {
            eff_focus.flow = 2;
            eff_focus.cnt = 0;
            if (eff_focus.out == 0)
                eff_focus.flow = 3;
        }
    }
    else if (eff_focus.flow == 0)
    {
        eff_focus.cnt++;
        if (eff_focus.in != 0)
        {
            s = Vu0Sinf(((((float)eff_focus.cnt * 90.0f) / (float)eff_focus.in) * 3.1415927f) / 180.0f);
            SubFocus(0.1f, 0.1f, 639.89996f, 447.9f, 640.0f, 448.0f, 0.0f, 0.0f,
                     (int)ec->dat.uc8[2] + (int)(s * (float)eff_focus.max));
        }
        if (eff_focus.in <= eff_focus.cnt)
        {
            if (eff_focus.keep == 0)
            {
                eff_focus.flow = 2;
                if (eff_focus.out == 0)
                    eff_focus.flow = 3;
            }
            else
            {
                eff_focus.flow = 1;
            }
            eff_focus.cnt = 0;
        }
    }
    else if (eff_focus.flow == 2)
    {
        eff_focus.cnt++;
        if (eff_focus.out != 0)
        {
            s = Vu0Sinf(((((float)eff_focus.cnt * 90.0f) / (float)eff_focus.out + 90.0f) * 3.1415927f) / 180.0f);
            SubFocus(0.1f, 0.1f, 639.89996f, 447.9f, 640.0f, 448.0f, 0.0f, 0.0f,
                     (int)ec->dat.uc8[2] + (int)(s * (float)eff_focus.max));
        }
        if (eff_focus.out <= eff_focus.cnt)
        {
            eff_focus.cnt = 0;
            eff_focus.flow = 3;
        }
    }
}

// FF2 SubContrast2 (type 0) and SubContrast3 (else) in one; FF3 0x00177ee0.
void SubContrast(int type, u_char col, u_char alp)
{
    SPRT_DAT2  sd = { 0, 0.1f, 0.1f, 639.89996f, 447.9f, 640.0f, 448.0f, -0.5f, -0.5f, 0xa0, 0x80 };
    DISP_SPRT2 ds;

    CopySprDToSpr2(&ds, &sd);
    ds.z   = 0xfff00;
    ds.r   = col;
    ds.g   = col;
    ds.b   = col;
    ds.alp = alp;
    if (type == 0)
    {
        ds.tex0   = (u_long)((GetSysWrk()->count & 1) * 0x1180) | SCR_FRAME_TEX0;
        ds.tex1   = 0x141;
        ds.alpreg = 0x48;
    }
    else
    {
        ds.gftg   = 0x144;
        ds.alpreg = 0x84;
        EffDispSprD2Direct(&ds);
    }
    EffDispSprD2Direct(&ds);
}

// Kinds 0xb / 0xc. FF2 SetContrast2; FF3 0x00178010.
void SetContrast2(EFFECT_CONT *ec)
{
    SubContrast(0, ec->dat.uc8[2], ec->dat.uc8[3]);
    if ((ec->dat.uc8[1] & 1) != 0)
        ResetEffects(ec);
}

// Kind 0xd. FF2 SetContrast3; FF3 0x00178068.
void SetContrast3(EFFECT_CONT *ec)
{
    SubContrast(1, ec->dat.uc8[2], ec->dat.uc8[3]);
    if ((ec->dat.uc8[1] & 1) != 0)
        ResetEffects(ec);
}

// FF2 SubNega; FF3 0x001780c0.
void SubNega(u_char r, u_char g, u_char b, u_char alp, u_char alp2)
{
    SPRT_DAT2  sd = { 0, 0.1f, 0.1f, 319.9f, 447.9f, 640.0f, 448.0f, -0.5f, -0.5f, 0xa0, 0x80 };
    DISP_SPRT2 ds;

    LocalCopyLtoL(3, (int)(GetSysWrk()->count & 1) * 0x1180, 0x2bc0);
    CopySprDToSpr2(&ds, &sd);
    ds.z      = 0xfff00;
    ds.alpreg = 0x84;
    ds.gftg   = 0x144;
    ds.r      = r;
    ds.g      = g;
    ds.b      = b;
    ds.alp    = alp;
    EffDispSprD2Direct(&ds);

    ds.alp = (u_char)(0x80 - alp2);
    ds.b   = 0x80;
    ds.r   = 0x80;
    ds.g   = 0x80;
    if ((s_char)alp2 < 0)
        ds.alp = 0;
    ds.alpreg = 0x44;
    ds.test   = 0x30003;
    ds.gftg   = 0x154;
    ds.tex0   = NEGA_HALF_TEX0;
    EffDispSprD2Direct(&ds);
}

// Kind 10. FF2 SetNega; FF3 0x00178228.
void SetNega(EFFECT_CONT *ec)
{
    u_char col;
    u_char alp;
    u_char alp2;

    if ((ec->dat.uc8[1] & 4) == 0)
    {
        col  = ec->dat.uc8[2];
        alp2 = *(u_char *)ec->pnt[0];
        alp  = ec->dat.uc8[3];
    }
    else
    {
        col  = ec->dat.uc8[2];
        alp2 = 0;
        alp  = ec->dat.uc8[3];
        if (ec->flow == 0)
            alp2 = (u_char)((int)(ec->cnt << 7) / (int)ec->in);
        else if (ec->flow == 1)
            alp2 = 0x80;
        else if (ec->flow == 2)
            alp2 = (u_char)((int)((ec->out - ec->cnt) * 0x80) / (int)ec->out);
        else if (ec->flow == 3)
            ResetEffects(ec);
        EffInKeepOutFlowCtrl(ec);
    }
    SubNega(col, col, col, alp, alp2);
    if ((ec->dat.uc8[1] & 1) != 0)
        ResetEffects(ec);
}

// ──────────────────────────────────────────────────────────────────────
// The requests
// ──────────────────────────────────────────────────────────────────────

// FF3 0x00178368; FF2 CallNega2.
void *CallNega2(int in, int keep, int out)
{
    return SetEffects(10, 4, 0x40, 0xc4, in, keep, out);
}

// FF3 0x00178698; FF2 CallDeform2.
void *CallDeform2(int in, int keep, int out, int type, int max)
{
    return SetEffects(4, 4, type, max, in, keep, out);
}

// ──────────────────────────────────────────────────────────────────────
// The screen filters
// ──────────────────────────────────────────────────────────────────────

// Kind 1. FF3 0x00178398: FF2's SetForcusDepth darkened the far field; FF3
// keeps the slot and only the one-frame reset.
void SetForcusDepth(EFFECT_CONT *ec)
{
    if ((ec->dat.uc8[1] & 1) != 0)
        ResetEffects(ec);
}

// The deform kernels (below SetDeform).
static void SetDeform0(int type, float rate, u_char alp);
static void SetDeform3(int type, float rate, u_char alp);
static void SetDeform4(int type, float rate, u_char alp);
static void SetDeform5(int type, float rate, u_char alp);
static void SetDeform6(int type, float rate, u_char alp);
static void SetDeformFanJitter(int type, float rate, u_char alp);

// FF2 SubDeform; FF3 0x001783c8 (jump table 0x354970). FF3's types: 0..2
// one kernel (FF2: 1 and 2), 3 a request, 4..8 one kernel each, 9 none.
void SubDeform(int type, float rate, u_char alp)
{
    switch (type)
    {
    case 0:
    case 1:
    case 2:
        SetDeform0(type, rate, alp);
        break;
    case 3:
        // FF3 0x00178c78 (SetDeformType3), inline here: no draw, the
        // request is read by another pass.
        eff_deform.type = (u_char)type;
        eff_deform.pass = 1;
        break;
    case 4:
        SetDeform3(type, rate, alp);
        break;
    case 5:
        SetDeform4(type, rate, alp);
        break;
    case 6:
        SetDeform5(type, rate, alp);
        break;
    case 7:
        SetDeform6(type, rate, alp);
        break;
    case 8:
        SetDeformFanJitter(type, rate, alp);
        break;
    case 9:
        // FF3 0x0017a838 (SetDeformType9): an empty function.
        break;
    default:
        break;
    }
}

// Kind 4. FF3 0x00178468; FF2 SetDeform with Vu0Sinf.
void SetDeform(EFFECT_CONT *ec)
{
    float ef;
    float phase;

    ef = 0.0f;
    if (ec->dat.uc8[2] != eff_deform.otype)
        eff_deform.init = 1;

    if ((ec->dat.uc8[1] & 4) == 0)
    {
        ef = (float)ec->dat.uc8[3];
    }
    else
    {
        if (ec->flow == 0)
        {
            if (ec->in != 0)
            {
                phase = ((float)ec->cnt * 90.0f) / (float)ec->in;
                ef = Vu0Sinf((phase * 3.1415927f) / 180.0f) * (float)ec->dat.uc8[3];
            }
        }
        else if (ec->flow == 1)
        {
            ef = Vu0Sinf(1.5707961f) * (float)ec->dat.uc8[3];
        }
        else if (ec->flow == 2)
        {
            if (ec->out != 0)
            {
                phase = ((float)ec->cnt * 90.0f) / (float)ec->out + 90.0f;
                ef = Vu0Sinf((phase * 3.1415927f) / 180.0f) * (float)ec->dat.uc8[3];
            }
        }
        else if (ec->flow == 3)
        {
            ResetEffects(ec);
            return;
        }
        EffInKeepOutFlowCtrl(ec);
    }

    SubDeform(ec->dat.uc8[2], ef, 0x80);
    if ((ec->dat.uc8[1] & 1) != 0)
        ResetEffects(ec);
}

// ──────────────────────────────────────────────────────────────────────
// The deform kernels (FF2 effect_scr.c SetDeform0 / 3 / 4 / 5 / 6, and
// FF3's type 8). Each warps a copy of the frame: LocalCopyLtoL halves the
// frame into the work page 0x2bc0, a grid over that copy is redrawn at
// displaced screen positions. FF3 differences from FF2: the grid is worked
// out every frame but copied and drawn only when rate > 0 and alp != 0
// (FF2: always, the copy first); the step is frozen by eff_wrk.stop_flg;
// the strip builder sets its own registers. PC: nothing walks the DIRECT
// strip, so each kernel also hands the same grid to the host as a triangle
// list (EffDeformHostGrid, MioPan's bridge).
// ──────────────────────────────────────────────────────────────────────

// A vertex of a strip grid (FF2 SCRDEF): the texel in the copy (the ST
// slots, sent as UV texels) and the GS window position.
struct SCRDEF                           /* 0x20 */
{
    /* 0x00 */ float stq[4];
    /* 0x10 */ float vtw[4];
};
static_assert(sizeof(SCRDEF) == 0x20, "SCRDEF: rows of 33 are 0x420 (0x179320's stack grid)");
static_assert(offsetof(SCRDEF, vtw) == 0x10, "SCRDEF.vtw");

// The polar form of a 33 x 25 grid vertex about the screen centre (FF2
// DEFWORK; bss 0x3d4790, shared by SetDeform3..6, rebuilt when
// eff_deform.init is set).
struct DEFWORK                          /* 0x18 */
{
    /* 0x00 */ float rrr;               // the angle, g3dAtan2f(x, y)
    /* 0x04 */ float lll;               // the distance
    /* 0x08 */ float mm1;               // the wave phase at that distance
    /* 0x0c */ float mm2;               // the fall-off towards the corner
    /* 0x10 */ float sss;               // sin rrr
    /* 0x14 */ float ccc;               // cos rrr
};
static_assert(sizeof(DEFWORK) == 0x18, "DEFWORK: rows of 33 are 0x318");
static DEFWORK deform_dw[25][33];       // bss 0x3d4790

static float deform0_r;                 // sdata 0x36eca8
static float deform3_r;                 // sdata 0x36ecf8
static float deform4_r;                 // sdata 0x36ed44
static float deform5_r;                 // sdata 0x36ed8c
static float deform6_r;                 // sdata 0x36edc8
static float deform8_x[230];            // bss 0x3d98a8: the fan's screen positions
static float deform8_y[230];            // bss 0x3d9c40

// The draw state of the kernels (the ROM builds it on the stack each
// time): ALPHA (Cs - Cd) * As + Cd, bilinear, REPEAT, tests ALWAYS, Z
// masked.
static const GS_REG_SET deform_regs[5] = {
    { 0x8000000044ULL, 0x42 },          // ALPHA_1
    { 0x161ULL,        0x14 },          // TEX1_1
    { 0,               0x08 },          // CLAMP_1
    { 0x30003ULL,      0x47 },          // TEST_1
    { 0x10a000118ULL,  0x4e },          // ZBUF_1
};

#define DEFORM_TEX0 0x2000000264016bc0ULL   // the copy at 0x2bc0, 320 wide (sdata 0x36ecb0 / 0x36ed00)

// PC: a deform grid to the host as two triangles per cell. Vertex (i, j)
// is element k = i * row_len + j; its UV is (tu, tv)[k * uv_step] in the
// copy's texels, its GS window position pos[k * pos_step] (x, y) plus add,
// with the 3D env's XYOFFSET (1728, 1824) taken back off.
static void EffDeformHostGrid(u_long tex0, int pnumw, int pnumh, int row_len,
                              const float *tu, const float *tv, int uv_step,
                              const float *pos, int pos_step, float add, u_char alp)
{
    static const int cell[6][2] = {     // {col, row}
        { 0, 0 }, { 0, 1 }, { 1, 0 },
        { 1, 0 }, { 0, 1 }, { 1, 1 },
    };
    static float  xy[32 * 24 * 6 * 2];
    static float  uv[32 * 24 * 6 * 2];
    static u_char rgba[32 * 24 * 6 * 4];
    int n = 0;
    int i, j, c;

    for (i = 0; i < pnumh; i++)
    {
        for (j = 0; j < pnumw; j++)
        {
            for (c = 0; c < 6; c++)
            {
                int k = (i + cell[c][1]) * row_len + j + cell[c][0];

                xy[n * 2 + 0] = pos[k * pos_step + 0] + add - 1728.0f;
                xy[n * 2 + 1] = pos[k * pos_step + 1] + add - 1824.0f;
                uv[n * 2 + 0] = tu[k * uv_step];
                uv[n * 2 + 1] = tv[k * uv_step];
                rgba[n * 4 + 0] = 0x80;
                rgba[n * 4 + 1] = 0x80;
                rgba[n * 4 + 2] = 0x80;
                rgba[n * 4 + 3] = alp;
                n++;
            }
        }
    }
    ReiPan_RendererDrawTexturedTriangles2D((const sceGsTex0 *)&tex0, xy, uv, rgba, nullptr, n, 0, 0, 0);
}

// FF2 MakeScrDeformPacket: the pnumw x pnumh cells of scrdef as one UV
// strip, each row primed by its first vertex pair (ADC). FF3 0x002b5998;
// FF3 sets the draw state here (FF2: SetDrawEnv before).
static void MakeScrDeformPacket(int pnumw, int pnumh, u_long tex0, SCRDEF (*scrdef)[33], u_char alp)
{
    u_int  *q;
    u_long *p;
    int     i, j, k;

    SetGsRegsCached(deform_regs, 5, 1);
    p = (u_long *)StartDmaDirectTrans();
    p[1] = 0xe;                         // A+D
    p[0] = 0x1000000000008003ULL;
    p[3] = 0x3f;                        // TEXFLUSH
    p[2] = 0;
    p[4] = tex0;
    p[5] = 6;                           // TEX0_1
    p[7] = 1;                           // RGBAQ
    p[6] = ((u_long)alp << 24) | 0x100808080ULL;
    p[8] = (u_long)(pnumh * (pnumw * 2 + 2)) | 0x20ae400000008000ULL;
    p[9] = 0x43;                        // UV, XYZ2
    q = (u_int *)(p + 10);
    for (i = 0; i < pnumh; i++)
    {
        for (j = 0; j <= pnumw; j++)
        {
            const SCRDEF *v[2] = { &scrdef[i][j], &scrdef[i + 1][j] };

            for (k = 0; k < 2; k++)
            {
                q[0] = (u_int)(int)(v[k]->stq[0] * 16.0f);
                q[1] = (u_int)(int)(v[k]->stq[1] * 16.0f);
                q[2] = (u_int)(int)(v[k]->stq[2] * 16.0f);
                q[3] = (u_int)(int)(v[k]->stq[3] * 16.0f);
                q[4] = (u_int)(int)(v[k]->vtw[0] * 16.0f);
                q[5] = (u_int)(int)(v[k]->vtw[1] * 16.0f);
                q[6] = (u_int)(int)(v[k]->vtw[2] * 16.0f);
                q[7] = (j == 0) ? 0x8000 : (u_int)(int)(v[k]->vtw[3] * 16.0f);
                q += 8;
            }
        }
    }
    EndDmaDirectTrans((qword *)q);

    EffDeformHostGrid(tex0, pnumw, pnumh, 33, &scrdef[0][0].stq[0], &scrdef[0][0].stq[1], 8,
                      &scrdef[0][0].vtw[0], 8, 0.0f, alp);
}

// The texel grid of SetDeform4 / 6: x 0..320 by 10, y 0..448 by 18.67,
// pulled in a texel at the copy's border.
static void SetDeformGridST(SCRDEF (*g)[33])
{
    int i, j;

    for (i = 0; i < 25; i++)
    {
        for (j = 0; j < 33; j++)
        {
            g[i][j].stq[1] = ((float)i * 448.0f) / 24.0f;
            g[i][j].stq[0] = (float)j * 320.0f * 0.03125f;
        }
        g[i][0].stq[0]  += 1.0f;
        g[i][32].stq[0] -= 1.0f;
    }
    for (j = 0; j < 33; j++)
    {
        g[0][j].stq[1]  += 1.0f;
        g[24][j].stq[1] -= 1.0f;
    }
}

// deform_dw for SetDeform3..6. waves is the number of crests between the
// centre and the corner distance lm = |(320, 224)|; SetDeform3 passes lm 0
// and divides by the corner vertex's own distance instead.
static void SetDeformPolarInit(float waves, float lm)
{
    float inv = 1.0f / lm;
    int   i, j;

    for (i = 0; i < 25; i++)
    {
        float fy = ((float)i * 18.666666f - 224.0f) + -0.5f;

        for (j = 0; j < 33; j++)
        {
            float    fx = ((float)j * 20.0f - 320.0f) + -0.5f;
            DEFWORK *dw = &deform_dw[i][j];
            float    pl;

            dw->lll = sqrtf(fx * fx + fy * fy);
            dw->rrr = g3dAtan2f(fx, fy);
            pl = dw->lll * 3.1415927f;
            if (lm == 0.0f)
            {
                dw->mm2 = (deform_dw[0][0].lll - dw->lll) / deform_dw[0][0].lll;
                dw->mm1 = (pl + pl) / deform_dw[0][0].lll;
            }
            else
            {
                dw->mm2 = (lm - dw->lll) * inv;
                dw->mm1 = (waves == 2.0f) ? (pl + pl) * inv : pl * waves * inv;
            }
            dw->sss = Vu0Sinf(dw->rrr);
            dw->ccc = Vu0Cosf(dw->rrr);
        }
    }
    eff_deform.init = 0;
}

// The copy the kernels sample: the frame being drawn, halved (type 3,
// 320 x 448) or quartered (type 1, 320 x 224) into 0x2bc0.
static void SetDeformCopy(int type)
{
    LocalCopyLtoL(type, (GetSysWrk()->count & 1) * 0x1180, 0x2bc0);
}

// Types 0..2 (FF2 1 / 2): a 25 x 17 grid, the inner vertices swept by a
// sine travelling down the rows (type 2: y by the columns). FF3 0x001786c8;
// FF2 SetDeform0 (FF3: r steps 6 degrees, FF2 2).
// callcheck: GetSysWrk, LocalCopyLtoL (in SetDeformCopy)
static void SetDeform0(int type, float rate, u_char alp)
{
    float tx[17][25];
    float ty[17][25];
    float vtw[17][25][4];
    float fw;
    float s;
    int   i, j;

    fw = rate / 10.0f;
    for (i = 0; i < 17; i++)
    {
        float ll = (float)i * 448.0f * 0.0625f;

        for (j = 0; j < 25; j++)
        {
            ty[i][j]     = ll;
            vtw[i][j][1] = ll + 1824.0f + -0.5f;
            vtw[i][j][2] = 0.0f;
            vtw[i][j][3] = 1.0f;
            tx[i][j]     = ((float)j * 320.0f) / 24.0f;
            vtw[i][j][0] = ((float)j * 640.0f) / 24.0f + 1728.0f + -0.5f;
            if (j == 0)
                tx[i][0] += 1.0f;
            if (j == 24)
                tx[i][24] -= 1.0f;
            if (i == 0)
                ty[0][j] += 1.0f;
            if (i == 16)
                ty[16][j] -= 1.0f;
        }
    }

    for (i = 0; i < 17; i++)
    {
        for (j = 0; j < 25; j++)
        {
            if (j != 0 && j != 24 && i != 0 && i != 16)
            {
                s = Vu0Sinf(((deform0_r + (float)i * 50.0f) * 3.1415927f) / 180.0f) * fw;
                vtw[i][j][0] += s;
                if (type == 2)
                    s = Vu0Sinf(((deform0_r + (float)j * 50.0f) * 3.1415927f) / 180.0f) * fw;
                vtw[i][j][1] += s;
            }
        }
    }

    if (eff_wrk.stop_flg == 0)
    {
        deform0_r += 6.0f;
        if (360.0f < deform0_r)
            deform0_r -= 360.0f;
    }

    if (0.0f < rate && alp != 0)
    {
        u_long *p;
        u_int  *q;

        SetDeformCopy(3);
        SetGsRegsCached(deform_regs, 5, 1);
        p = (u_long *)StartDmaDirectTrans();
        p[2] = 0;
        p[0] = 0x1000000000008002ULL;
        p[1] = 0xe;                     // A+D
        p[3] = 0x3f;                    // TEXFLUSH
        p[4] = DEFORM_TEX0;
        p[5] = 6;                       // TEX0_1
        p[7] = 0x431;                   // RGBAQ, UV, XYZ2
        p[6] = 0x30ae400000008320ULL;   // 800 loops, PRIM 0x15c (strip, textured, blended, UV)
        q = (u_int *)(p + 8);
        for (i = 0; i < 16; i++)
        {
            for (j = 0; j < 50; j++)
            {
                int row = (j % 2) + i;
                int col = j / 2;

                q[0]  = 0x80;
                q[1]  = 0x80;
                q[2]  = 0x80;
                q[3]  = alp;
                q[4]  = (u_int)(int)(tx[row][col] * 16.0f);
                q[5]  = (u_int)(int)(ty[row][col] * 16.0f);
                q[6]  = 0;
                q[7]  = 0;
                q[8]  = (u_int)(int)(vtw[row][col][0] * 16.0f);
                q[9]  = (u_int)(int)(vtw[row][col][1] * 16.0f);
                q[10] = (u_int)(int)(vtw[row][col][2] * 16.0f);
                q[11] = (j < 2) ? 0x8000 : 0;
                q += 12;
            }
        }
        EndDmaDirectTrans((qword *)q);

        EffDeformHostGrid(DEFORM_TEX0, 24, 16, 25, &tx[0][0], &ty[0][0], 1, &vtw[0][0][0], 4, 0.0f, alp);
    }

    eff_deform.type = (u_char)type;
    eff_deform.init = 1;
    eff_deform.pass = 1;
}

// Type 4: a ripple out of the screen centre over a 33 x 25 grid, one crest
// per corner distance, r steps 0.2 rad. FF3 0x00178c90; FF2 SetDeform3.
// callcheck: g3dAtan2f, Vu0Cosf, GetSysWrk, LocalCopyLtoL (in SetDeformPolarInit / SetDeformCopy)
static void SetDeform3(int type, float rate, u_char alp)
{
    static float tx[25][33];            // PC: static, the ROM stacks 0x8100 bytes of grids
    static float ty[25][33];
    static float vt[25][33][4];
    static float vtw[25][33][4];
    float fw;
    int   i, j;

    for (i = 0; i < 25; i++)
    {
        for (j = 0; j < 33; j++)
        {
            ty[i][j]    = ((float)i * 448.0f) / 24.0f;
            vt[i][j][0] = 2048.0f;
            vt[i][j][1] = 2048.0f;
            vt[i][j][2] = 0.0f;
            vt[i][j][3] = 1.0f;
            tx[i][j]    = (float)j * 320.0f * 0.03125f;
            if (j == 0)
                tx[i][0] += 1.0f;
            if (j == 32)
                tx[i][32] -= 1.0f;
            if (i == 0)
                ty[0][j] += 1.0f;
            if (i == 24)
                ty[24][j] -= 1.0f;
        }
    }

    fw = rate * 0.25f;
    if (eff_deform.init != 0)
        SetDeformPolarInit(2.0f, 0.0f);

    for (i = 0; i < 25; i++)
    {
        for (j = 0; j < 33; j++)
        {
            const DEFWORK *dw = &deform_dw[i][j];
            float lm = Vu0Sinf(dw->mm1 - deform3_r) * fw * dw->mm2;

            vtw[i][j][2] = 0.0f;
            vtw[i][j][0] = dw->sss * dw->lll - dw->ccc * lm;
            vtw[i][j][1] = dw->sss * lm + dw->ccc * dw->lll;
        }
    }

    if (eff_wrk.stop_flg == 0)
    {
        deform3_r += 0.2f;
        if (6.2831855f < deform3_r)
            deform3_r -= 6.2831855f;
    }

    if (0.0f < rate && alp != 0)
    {
        u_long *p;
        u_int  *q;

        SetDeformCopy(3);
        SetGsRegsCached(deform_regs, 5, 1);
        p = (u_long *)StartDmaDirectTrans();
        p[2] = 0;
        p[0] = 0x1000000000008002ULL;
        p[1] = 0xe;
        p[3] = 0x3f;
        p[4] = DEFORM_TEX0;
        p[5] = 6;
        p[7] = 0x431;
        p[6] = 0x30ae400000008630ULL;   // 1584 loops
        q = (u_int *)(p + 8);
        for (i = 0; i < 24; i++)
        {
            for (j = 0; j < 66; j++)
            {
                int row = (j % 2) + i;
                int col = j / 2;

                q[0]  = 0x80;
                q[1]  = 0x80;
                q[2]  = 0x80;
                q[3]  = alp;
                q[4]  = (u_int)(int)(tx[row][col] * 16.0f);
                q[5]  = (u_int)(int)(ty[row][col] * 16.0f);
                q[6]  = 0;
                q[7]  = 0;
                q[8]  = (u_int)(int)((vt[row][col][0] + vtw[row][col][0]) * 16.0f);
                q[9]  = (u_int)(int)((vt[row][col][1] + vtw[row][col][1]) * 16.0f);
                q[10] = (u_int)(int)((vt[row][col][2] + vtw[row][col][2]) * 16.0f);
                q[11] = (j < 2) ? 0x8000 : 0;
                q += 12;
            }
        }
        EndDmaDirectTrans((qword *)q);

        EffDeformHostGrid(DEFORM_TEX0, 32, 24, 33, &tx[0][0], &ty[0][0], 1, &vtw[0][0][0], 4, 2048.0f, alp);
    }

    eff_deform.type = (u_char)type;
    eff_deform.pass = 1;
}

// The shimmering ripple of SetDeform4..6: the ripple times a fast angular
// sine and a random 1..2. flip: y takes +sss * disp (SetDeform6) instead
// of -sss * disp.
static void SetDeformShimmer(SCRDEF (*g)[33], float fw, float r, int flip)
{
    int i, j;

    for (i = 0; i < 25; i++)
    {
        for (j = 0; j < 33; j++)
        {
            const DEFWORK *dw = &deform_dw[i][j];
            float rnd  = (float)ReiPan_Rand() / REIPAN_RAND_MAXF;
            float disp = Vu0Sinf(dw->mm1 - r) * fw * dw->mm2
                       * Vu0Sinf((r + dw->rrr) * 18.0f) * (rnd + 1.0f);

            g[i][j].vtw[2] = 0.0f;
            g[i][j].vtw[3] = 0.0f;
            g[i][j].vtw[0] = (dw->sss * dw->lll - dw->ccc * disp) + 2048.0f;
            if (flip)
                g[i][j].vtw[1] = dw->sss * disp + dw->ccc * dw->lll + 2048.0f;
            else
                g[i][j].vtw[1] = (dw->ccc * dw->lll - dw->sss * disp) + 2048.0f;
        }
    }
}

// SetDeform4..6's phase step: 0.02 rad a frame unless effects are frozen.
static void SetDeformStepR(float *r)
{
    if (eff_wrk.stop_flg == 0)
    {
        *r += 0.02f;
        if (6.2831855f < *r)
            *r -= 6.2831855f;
    }
}

// Type 5: the shimmering ripple with 12 crests. FF3 0x00179320; FF2
// SetDeform4.
// callcheck: rand, Vu0Sinf, g3dAtan2f, Vu0Cosf, GetSysWrk, LocalCopyLtoL (in SetDeformShimmer with ReiPan_Rand / SetDeformPolarInit / SetDeformCopy)
static void SetDeform4(int type, float rate, u_char alp)
{
    static SCRDEF scrdef[25][33];       // PC: static, the ROM stacks it
    float fw = rate * 0.25f;

    SetDeformGridST(scrdef);
    if (eff_deform.init != 0)
        SetDeformPolarInit(12.0f, sqrtf(152576.0f));
    SetDeformShimmer(scrdef, fw, deform4_r, 0);
    SetDeformStepR(&deform4_r);

    if (0.0f < rate && alp != 0)
    {
        SetDeformCopy(3);
        MakeScrDeformPacket(32, 24, DEFORM_TEX0, scrdef, alp);
    }
    eff_deform.type = (u_char)type;
    eff_deform.pass = 1;
}

// Type 6: the shimmering ripple with 2 crests; FF3 adds the texel steps up
// (10 and 18.67 per cell). FF3 0x00179820; FF2 SetDeform5.
// callcheck: rand, Vu0Sinf, g3dAtan2f, Vu0Cosf, GetSysWrk, LocalCopyLtoL (in SetDeformShimmer with ReiPan_Rand / SetDeformPolarInit / SetDeformCopy)
static void SetDeform5(int type, float rate, u_char alp)
{
    static SCRDEF scrdef[25][33];
    float fw;
    float x, y;
    int   i, j;

    y = 0.0f;
    for (i = 0; i < 25; i++)
    {
        x = 0.0f;
        for (j = 0; j < 33; j++)
        {
            scrdef[i][j].stq[0] = x;
            scrdef[i][j].stq[1] = y;
            x += 10.0f;
        }
        y += 18.666666f;
        scrdef[i][0].stq[0]  += 1.0f;
        scrdef[i][32].stq[0] -= 1.0f;
    }
    for (j = 0; j < 33; j++)
    {
        scrdef[0][j].stq[1]  += 1.0f;
        scrdef[24][j].stq[1] -= 1.0f;
    }

    fw = rate * 0.25f;
    if (eff_deform.init != 0)
        SetDeformPolarInit(2.0f, sqrtf(152576.0f));
    SetDeformShimmer(scrdef, fw, deform5_r, 0);
    SetDeformStepR(&deform5_r);

    if (0.0f < rate && alp != 0)
    {
        SetDeformCopy(3);
        MakeScrDeformPacket(32, 24, DEFORM_TEX0, scrdef, alp);
    }
    eff_deform.type = (u_char)type;
    eff_deform.pass = 1;
}

// Type 7: SetDeform5's ripple turned the other way on y. FF3 0x00179ce0;
// FF2 SetDeform6.
// callcheck: rand, Vu0Sinf, g3dAtan2f, Vu0Cosf, GetSysWrk, LocalCopyLtoL (in SetDeformShimmer with ReiPan_Rand / SetDeformPolarInit / SetDeformCopy)
static void SetDeform6(int type, float rate, u_char alp)
{
    static SCRDEF scrdef[25][33];
    float fw = rate * 0.25f;

    SetDeformGridST(scrdef);
    if (eff_deform.init != 0)
        SetDeformPolarInit(2.0f, sqrtf(152576.0f));
    SetDeformShimmer(scrdef, fw, deform6_r, 1);
    SetDeformStepR(&deform6_r);

    if (0.0f < rate && alp != 0)
    {
        SetDeformCopy(3);
        MakeScrDeformPacket(32, 24, DEFORM_TEX0, scrdef, alp);
    }
    eff_deform.type = (u_char)type;
    eff_deform.pass = 1;
}

// Type 8 (FF3-only): a fan from the screen centre to 229 points round the
// frame (65 per long side, 49 per short) over the quarter-size copy
// (320 x 224); every border texel jitters by up to rate * 0.3 (x) / 0.2
// (y) and takes a random alpha alp / 2..alp, the centre alpha 0, so the
// edges of the picture swim. FF3 0x0017a1c8.
// callcheck: rand, GetSysWrk, LocalCopyLtoL (ReiPan_Rand; SetDeformCopy)
static void SetDeformFanJitter(int type, float rate, u_char alp)
{
    static const GS_REG_SET regs[5] = {
        { 0x8000000044ULL, 0x42 },      // ALPHA_1
        { 0x21ULL,         0x14 },      // TEX1_1: bilinear, no mipmap
        { 0,               0x08 },      // CLAMP_1
        { 0x30003ULL,      0x47 },      // TEST_1
        { 0x10a000118ULL,  0x4e },      // ZBUF_1
    };
    static const u_long tex0 = 0x2000000224116bc0ULL;  // sdata 0x36ee10: the copy, PSMCT24
    float  u[230];
    float  v[230];
    u_char a[230];
    float  ju, jv;
    int    i;

    if (eff_deform.init != 0)
    {
        for (i = 0; i < 230; i++)
        {
            if (i == 0)
            {
                deform8_x[0] = 320.0f;
                deform8_y[0] = 224.0f;
            }
            else if (i < 0x42)
            {
                deform8_y[i] = 0.0f;
                deform8_x[i] = ((float)(i - 1) * 640.0f) / 65.0f;
            }
            else if (i < 0x73)
            {
                deform8_x[i] = 640.0f;
                deform8_y[i] = ((float)(i - 0x42) * 448.0f) / 49.0f;
            }
            else if (i < 0xb4)
            {
                deform8_y[i] = 448.0f;
                deform8_x[i] = 640.0f - ((float)(i - 0x73) * 640.0f) / 65.0f;
            }
            else if (i < 0xe5)
            {
                deform8_x[i] = 0.0f;
                deform8_y[i] = 448.0f - ((float)(i - 0xb4) * 448.0f) / 49.0f;
            }
            else
            {
                deform8_x[i] = 0.0f;
                deform8_y[i] = 0.0f;
            }
        }
        eff_deform.init = 0;
    }

    a[0] = 0;
    u[0] = deform8_x[0] * 0.5f;
    v[0] = deform8_y[0] * 0.5f;
    ju = rate * 0.3f;
    for (i = 1; i < 0xe5; i++)
    {
        float r;

        jv = rate * 0.2f;
        r = ((float)ReiPan_Rand() / REIPAN_RAND_MAXF) * ju;
        u[i] = deform8_x[i] * 0.5f + ((r + r) - ju);
        r = ((float)ReiPan_Rand() / REIPAN_RAND_MAXF) * jv;
        v[i] = deform8_y[i] * 0.5f + ((r + r) - jv);
        r = (float)ReiPan_Rand() / REIPAN_RAND_MAXF;
        a[i] = (u_char)(int)((r * 0.5f + 0.5f) * (float)alp);
        if (u[i] < 1.0f)
            u[i] = 1.0f;
        else if (320.0f < u[i])
            u[i] = 320.0f;
        if (v[i] < 1.0f)
            v[i] = 1.0f;
        else if (224.0f < v[i])
            v[i] = 224.0f;
    }
    // The fan closes on the first border point at its unscaled position,
    // (0, 0) -- the ROM's.
    u[0xe5] = deform8_x[1];
    v[0xe5] = deform8_y[1];
    a[0xe5] = a[1];

    if (0.0f < rate && alp != 0)
    {
        static float  xy[228 * 3 * 2];
        static float  uv[228 * 3 * 2];
        static u_char rgba[228 * 3 * 4];
        u_long *p;
        u_int  *q;
        int     n;

        SetDeformCopy(1);
        SetGsRegsCached(regs, 5, 1);
        p = (u_long *)StartDmaDirectTrans();
        p[2] = 0;
        p[0] = 0x1000000000008002ULL;
        p[1] = 0xe;
        p[3] = 0x3f;
        p[4] = tex0;
        p[5] = 6;
        p[6] = 0x30aec000000080e6ULL;   // 230 loops, PRIM 0x15d (fan, gouraud, textured, blended, UV)
        p[7] = 0x413;                   // UV, RGBAQ, XYZ2
        q = (u_int *)(p + 8);
        for (i = 0; i < 230; i++)
        {
            q[0]  = (u_int)(int)(u[i] * 16.0f);
            q[1]  = (u_int)(int)(v[i] * 16.0f);
            q[2]  = 0;
            q[3]  = 0;
            q[4]  = 0x80;
            q[5]  = 0x80;
            q[6]  = 0x80;
            q[7]  = a[i];
            q[8]  = (u_int)((int)((deform8_x[i] + 2048.0f) - 320.0f) << 4);
            q[9]  = (u_int)((int)((deform8_y[i] + 2048.0f) - 224.0f) << 4);
            q[10] = 1;
            q[11] = (i < 2) ? 0x8000 : 0;
            q += 12;
        }
        EndDmaDirectTrans((qword *)(p + 0x56c));

        // PC: the fan as a triangle list; its window positions less the
        // XYOFFSET are screen positions.
        n = 0;
        for (i = 1; i < 0xe5; i++)
        {
            const int k[3] = { 0, i, i + 1 };
            int c;

            for (c = 0; c < 3; c++)
            {
                xy[n * 2 + 0]   = (float)(int)((deform8_x[k[c]] + 2048.0f) - 320.0f) - 1728.0f;
                xy[n * 2 + 1]   = (float)(int)((deform8_y[k[c]] + 2048.0f) - 224.0f) - 1824.0f;
                uv[n * 2 + 0]   = u[k[c]];
                uv[n * 2 + 1]   = v[k[c]];
                rgba[n * 4 + 0] = 0x80;
                rgba[n * 4 + 1] = 0x80;
                rgba[n * 4 + 2] = 0x80;
                rgba[n * 4 + 3] = a[k[c]];
                n++;
            }
        }
        ReiPan_RendererDrawTexturedTriangles2D((const sceGsTex0 *)&tex0, xy, uv, rgba, nullptr, n, 0, 0, 0);
    }

    eff_deform.type = (u_char)type;
    eff_deform.pass = 1;
}

// PC (MioPan's EffScrFullScreenRect): a whole-screen filter's rectangle
// grown from the ROM's (-0.5, -0.5) 640 x 448 to the window. The renderer
// keeps a textured 2D quad inside the 640 x 448 frame, so on a wider (or
// taller) window a filter stopped at the frame's edge; the view bounds are
// that frame grown to the window, in the same units. The half-pixel bias is
// the ROM's, kept. Filters only: a full-frame picture keeps its shape.
static void EffScrFullScreenRect(SPRT_DAT2 *sd)
{
    float x0, y0, x1, y1;

    ReiPan_RendererGetViewBounds(&x0, &y0, &x1, &y1);
    sd->x = x0 - 0.5f;
    sd->y = y0 - 0.5f;
    sd->w = x1 - x0;
    sd->h = y1 - y0;
}

// FF2 SubFadeFrame; FF3 0x0017a840: the vignette page of the effect VRAM
// (tex0 0x2007ea859930be9c, 64 x 64 texels) stretched over the screen;
// FF2 read it from effdat[0x4a]. pri is unused, as in FF2.
// PC: over the window (EffScrFullScreenRect); the UVs stay, so the
// gradient stretches to the window's edges, as a vignette should.
void SubFadeFrame(u_char alp, u_int pri)
{
    SPRT_DAT2  sd = { 0, 0.099999994f, 0.099999994f, 0.0f, 0.0f,
                      640.0f, 448.0f, -0.5f, -0.5f, 0xa0, 0x80 };
    DISP_SPRT2 ds;

    (void)pri;
    EffScrFullScreenRect(&sd);          // PC
    CopySprDToSpr2(&ds, &sd);
    ds.tex0 = 0x2007ea859930be9cULL;
    ds.u1   = 0;
    ds.v1   = 0;
    ds.u2   = (u_short)(int)(16.0f * 64.0f);
    ds.v2   = (u_short)(int)(16.0f * 64.0f);
    ds.alp  = alp;
    EffDispSprD2Direct(&ds);
}

// Kind 7. FF3 0x0017a928; FF2 SetFadeFrame.
void SetFadeFrame(EFFECT_CONT *ec)
{
    SubFadeFrame(ec->dat.uc8[2], ec->dat.ui32[1]);
    if ((ec->dat.uc8[1] & 1) != 0)
        ResetEffects(ec);
}

// Kind 9. FF3 0x0017a978; FF2 SetBlackFilter.
void SetBlackFilter(EFFECT_CONT *ec)
{
    DispSqrRect(0.0f, 0.0f, 640.0f, 448.0f, 0, 0, 0, 0, ec->dat.uc8[2]);
    if ((ec->dat.uc8[1] & 1) != 0)
        ResetEffects(ec);
}

/// ──────────────────────────────────────────────────────────────────────
// The screen dither (FF2 effect_scr.c SubDither3 / MakeRDither3)
// ──────────────────────────────────────────────────────────────────────

// The noise sheet's texture: 128 x 128 PSMT8H at 0x2200 (the alpha byte of
// frame buffer 1's tail), clut 0x3fbc (FF2 0x3ffc), REPEAT.
#define DITHER_TEX0 0x2007f785ddb0a200ULL   // sdata 0x36ee68 / 0x36eec0

static u_char dither_pat[0x4000];       // bss 0x41e5c0: the 128 x 128 indices
static u_int  dither_pal[0x100];        // bss 0x4225c0: index i -> alpha i, grey 0..colmx

// SubDither3's scroll: the phase of the three alpha waves, the UV offset
// (texels, 0..128) and the target the screen motion is measured from.
static float dither3_cnf;               // sdata 0x36ee58
static float dither3_cx;                // sdata 0x36ee5c
static float dither3_cy;                // sdata 0x36ee60
static int   dither3_fl = 1;            // sdata 0x36ee64: 1 while the camera is up
static sceVu0FVECTOR dither3_old_cam;   // data 0x2f99c0

// SubDither4's: the same, with cy starting at -9908638 (see SubDither4).
static float dither4_cnf;               // sdata 0x36eeac
static float dither4_cx;                // sdata 0x36eeb0
static float dither4_cy = -9908638.0f;  // sdata 0x36eeb4
static int   dither4_fl = 1;            // sdata 0x36eeb8
static sceVu0FVECTOR dither4_old_cam;   // data 0x2f99d0

// FF2 MakeRDither3: a fresh noise sheet, indices 0..alpmx and a clut of
// greys 0..colmx whose alpha is the index, sent with two image transfers.
// FF3 0x002b5be8 (FF2 built it on the stack and used sceGsExecLoadImage).
// PC: the chain is not walked, so the texels also go to the host GS.
// callcheck: rand (ReiPan_Rand)
static void MakeRDither3(u_char alpmx, u_char colmx)
{
    sceGsLoadImage li;
    u_long *p;
    int     i;

    for (i = 0; i < 0x4000; i++)
        dither_pat[i] = (u_char)(int)((float)alpmx * ((float)ReiPan_Rand() / REIPAN_RAND_MAXF));
    for (i = 0; i < 0x100; i++)
    {
        u_int c = (u_int)(int)((float)colmx * ((float)ReiPan_Rand() / REIPAN_RAND_MAXF)) & 0xff;

        dither_pal[i] = ((u_int)i << 24) | c | (c << 8) | (c << 16);
    }

    p = (u_long *)StartDmaDirectTrans();
    p[1]  = 0xe;
    p[0]  = 0x1000000000008005ULL;
    p[3]  = 0x3f;                       // TEXFLUSH
    p[2]  = 0;
    p[4]  = 0x1b02220000000000ULL;      // BITBLTBUF: 0x2200, width 2, PSMT8H
    p[5]  = 0x50;
    p[7]  = 0x51;                       // TRXPOS 0
    p[6]  = 0;
    p[8]  = 0x8000000080ULL;            // TRXREG 128 x 128
    p[9]  = 0x52;
    p[11] = 0x53;                       // TRXDIR host -> local
    p[10] = 0;
    EndDmaDirectTrans((qword *)&p[12]);
    SetDmaDirectImageTrans(dither_pat, 0x400);

    p = (u_long *)StartDmaDirectTrans();
    p[0]  = 0x1000000000008005ULL;
    p[1]  = 0xe;
    p[3]  = 0x3f;
    p[2]  = 0;
    p[4]  = 0x13fbc00000000ULL;         // BITBLTBUF: 0x3fbc, width 1, CT32
    p[5]  = 0x50;
    p[7]  = 0x51;
    p[6]  = 0;
    p[8]  = 0x1000000010ULL;            // TRXREG 16 x 16
    p[9]  = 0x52;
    p[11] = 0x53;
    p[10] = 0;
    EndDmaDirectTrans((qword *)&p[12]);
    SetDmaDirectImageTrans(dither_pal, 0x40);

    sceGsSetDefLoadImage(&li, 0x2200, 2, SCE_GS_PSMT8H, 0, 0, 128, 128);
    ReiPan_GsUpload(&li, dither_pat);
    sceGsSetDefLoadImage(&li, 0x3fbc, 1, SCE_GS_PSMCT32, 0, 0, 16, 16);
    ReiPan_GsUpload(&li, (unsigned char *)dither_pal);
}

// FF3 0x0017a9f8 (FF2 MakeDitherPattern kept a frame counter instead): a
// changed (alpmx, colmx) raises remake, and the sheet is rebuilt on the
// first later frame that asks for the same pair; the draws skip while
// remake is up.
void DitherPatternUpdate(u_int alpmx, u_int colmx)
{
    if (alpmx >= 0x100 || colmx >= 0x100)
        return;

    if (eff_dither.alpmx == alpmx && eff_dither.colmx == colmx && eff_dither.remake != 0)
    {
        eff_dither.remake = 0;
        MakeRDither3((u_char)alpmx, (u_char)colmx);
    }
    if (eff_dither.alpmx != alpmx || eff_dither.colmx != colmx)
        eff_dither.remake = 1;
    eff_dither.colmx = (u_char)colmx;
    eff_dither.alpmx = (u_char)alpmx;
}

// FF2's PlayerModeIsFinder, inline in both dithers: the camera is up or
// the cat's view is on.
static int DitherIsFinder(void)
{
    const PLYR_WRK *pw = ingame_wrk.plyr_wrk;

    return (pw->flags & PLYR_FLG_800000) != 0 || (pw->_0e0 & 1) != 0;
}

// The target's screen motion since the last frame, as SubDither3 / 4 scale
// it; 0 when a position is not a number.
static void DitherScreenMove(float tx, float ty, float otx, float oty, float *mvx, float *mvy)
{
    if (isnan(tx) == 0 && isnan(ty) == 0 && isnan(otx) == 0 && isnan(oty) == 0)
    {
        *mvx = (tx - otx) * 0.2f;
        *mvy = (ty - oty) * 0.1f;
    }
    else
    {
        *mvx = 0.0f;
        *mvy = 0.0f;
    }
}

// PC: the sheet's span over the grown quad. The grain tiles (CLAMP REPEAT),
// so the span grows with the quad and the texel density stays the ROM's:
// 640 x 512 texels on a 640:448 window.
static void DitherSheetSpan(float *uspan, float *vspan)
{
    float ext_x, ext_y;

    ReiPan_RendererGetViewExtend(&ext_x, &ext_y);
    *uspan = 640.0f * ext_x;
    *vspan = 512.0f * ext_y;
}

// Types 1..7: the noise sheet over the whole screen three times, each pass
// shifted 64 texels and its alpha on a sine 120 degrees apart, scrolled
// with the camera target's screen motion. FF3 0x0017aaa0; FF2 SubDither3
// (FF3: no dith_off test here -- SetDither3 makes it --, the clut at 0x3fbc,
// the draws wait while the sheet is being remade, and EffDispSprD2Direct).
// PC: the quads cover the window (EffScrFullScreenRect, DitherSheetSpan);
// the Screen effects menu's grain switch skips the draws when off -- after
// the sheet, the scroll and the phase have stepped, so ReiPan_Rand's stream
// is the same either way -- and in native mode the renderer draws them off
// its own sheet at one texel per output pixel (ReiPan_RendererFilmGrainBegin).
// callcheck: fptodp, isnan (DitherScreenMove)
void SubDither3(int type, float alp, float spd, u_char alpmx, u_char colmx)
{
    SPRT_DAT2    sd = { 0, 0.0f, 0.0f, 0.0f, 0.0f, 640.0f, 448.0f, -0.5f, -0.5f, 0xa0, 0x80 };
    DISP_SPRT2   ds;
    GRA3DCAMERA *cam = gra3dGetCamera();
    u_long alpreg = 0x44;
    u_char g = 0x80;
    u_char b = 0x80;
    float  tx, ty, otx, oty, mvx, mvy;
    float  uspan, vspan;                // PC: 640 / 512 in the ROM

    if (!DitherIsFinder())
    {
        if (dither3_fl == 1 || CamChangeCheck() != 0)
            memcpy(dither3_old_cam, cam->vTarget, sizeof(sceVu0FVECTOR));
        dither3_fl = 0;
    }
    else
    {
        if (dither3_fl == 0)
            memcpy(dither3_old_cam, cam->vTarget, sizeof(sceVu0FVECTOR));
        dither3_fl = 1;
    }

    switch (type)
    {
    case 1: alpreg = 0x44; break;
    case 2: alpreg = 0x48; break;
    case 3: alpreg = 0x41; break;
    case 4: g = 0;    b = 0x80; alpreg = 0x41; break;
    case 5: g = 0x80; b = 0;    alpreg = 0x41; break;
    case 6: alpreg = 0x49; break;
    case 7: alpreg = 0x42; break;
    }

    DitherPatternUpdate(alpmx, colmx);
    if (eff_dither.remake != 0)
        return;

    WorldToScreenPos(cam->vTarget, &tx, &ty);
    WorldToScreenPos(dither3_old_cam, &otx, &oty);
    memcpy(dither3_old_cam, cam->vTarget, sizeof(sceVu0FVECTOR));

    if (tx < -2048.0f)  tx = 0.0f;
    if (ty < -2048.0f)  ty = 0.0f;
    if (otx < -2048.0f) otx = 0.0f;
    if (oty < -2048.0f) oty = 0.0f;
    if (2048.0f < tx)   tx = 0.0f;
    if (2048.0f < ty)   ty = 0.0f;
    if (2048.0f < otx)  otx = 0.0f;
    if (2048.0f < oty)  oty = 0.0f;
    DitherScreenMove(tx, ty, otx, oty, &mvx, &mvy);

    if (eff_wrk.stop_flg == 0)
    {
        dither3_cy = (dither3_cy + mvy) - (float)((int)((dither3_cy + mvy) * 0.0078125f) << 7);
        dither3_cx = (dither3_cx + mvx) - (float)((int)((dither3_cx + mvx) * 0.0078125f) << 7);
        while (128.0f < dither3_cx)
            dither3_cx -= 128.0f;
        while (dither3_cx < 0.0f)
            dither3_cx += 128.0f;
        while (128.0f < dither3_cy)
            dither3_cy -= 128.0f;
        while (dither3_cy < 0.0f)
            dither3_cy += 128.0f;
    }

    if (ReiPan_RendererGetFilmGrain() == REIPAN_FILM_GRAIN_OFF)   // PC
        goto step;

    EffScrFullScreenRect(&sd);          // PC
    DitherSheetSpan(&uspan, &vspan);    // PC
    CopySprDToSpr2(&ds, &sd);
    ds.z      = 0xfff00;
    ds.r      = 0x80;
    ds.clmp   = 0;
    ds.tex1   = 0x141;
    ds.tex0   = DITHER_TEX0;
    ds.zbuf   = 0x10a000118ULL;
    ds.alpreg = alpreg;
    ds.g      = g;
    ds.b      = b;

    ReiPan_RendererFilmGrainBegin(alpmx, colmx);    // PC
    ds.u1  = (u_short)(int)(dither3_cx * 16.0f);
    ds.v1  = (u_short)(int)(dither3_cy * 16.0f);
    ds.u2  = (u_short)(int)((dither3_cx + uspan) * 16.0f);
    ds.v2  = (u_short)(int)((dither3_cy + vspan) * 16.0f);
    ds.alp = (u_char)(int)((Vu0Sinf((dither3_cnf * 3.1415927f) / 180.0f) + 1.0f) * alp);
    EffDispSprD2Direct(&ds);

    ds.u1  = (u_short)(int)((dither3_cx + 64.0f) * 16.0f);
    ds.v1  = (u_short)(int)(dither3_cy * 16.0f);
    ds.u2  = (u_short)(int)((dither3_cx + uspan + 64.0f) * 16.0f);
    ds.v2  = (u_short)(int)((dither3_cy + vspan) * 16.0f);
    ds.alp = (u_char)(int)((Vu0Sinf(((dither3_cnf + 120.0f) * 3.1415927f) / 180.0f) + 1.0f) * alp);
    EffDispSprD2Direct(&ds);

    ds.u1  = (u_short)(int)(dither3_cx * 16.0f);
    ds.v1  = (u_short)(int)((dither3_cy + 64.0f) * 16.0f);
    ds.u2  = (u_short)(int)((dither3_cx + uspan) * 16.0f);
    ds.v2  = (u_short)(int)((dither3_cy + vspan + 64.0f) * 16.0f);
    ds.alp = (u_char)(int)((Vu0Sinf(((dither3_cnf + 240.0f) * 3.1415927f) / 180.0f) + 1.0f) * alp);
    EffDispSprD2Direct(&ds);
    ReiPan_RendererFilmGrainEnd();      // PC

step:
    if (eff_wrk.stop_flg == 0)
        dither3_cnf += spd;
}

// One pass of SubDither4: at full screen up to alpha 0x40; above that two
// half-alpha copies 15 pixels apart (y -8 and 7, 463 high), a vertical
// smear. FF3 0x002b5e38 (FF3-only).
// PC: full screen is the window (the view bounds); the ROM's rectangles are
// those bounds on a 640:448 window, (0, 0) - (640, 448).
static void SubDither4Disp(DISP_SPRT2 *ds, u_char alp)
{
    float x0, y0, x1, y1;

    ReiPan_RendererGetViewBounds(&x0, &y0, &x1, &y1);
    if (alp <= 0x40)
    {
        ds->alp = alp;
        ds->y   = y0 - 0.5f;
        ds->x   = x0 - 0.5f;
        ds->h   = y1 - y0;
        ds->w   = x1 - x0;
    }
    else
    {
        ds->alp = alp >> 1;
        ds->x   = x0 - 0.5f;
        ds->w   = x1 - x0;
        ds->h   = (y1 - y0) + 15.0f;
        ds->y   = y0 - 8.0f;
        EffDispSprD2Direct(ds);
        ds->x   = x0 - 0.5f;
        ds->y   = y0 + 7.0f;
    }
    EffDispSprD2Direct(ds);
}

// FF3 builds each (u1, u2) and (v1, v2) pair of SubDither4 as one word,
// hi << 16 | lo, and stores its halves: a negative lo (cy starts at
// -9908638) spills its sign bits into the second coordinate.
static u_short Dither4PairHi(int lo, int hi)
{
    return (u_short)((((u_int)hi << 16) | (u_int)lo) >> 16);
}

// Types 8 and up (FF3-only, FF2 had a different SubDither4): SubDither3's
// three passes with the default blend and colour, the scroll doubled and
// wrapped once a frame (no stop test, no +-2048 clamp), cy pulled down
// 1024 a frame unless that passes -9908638 -- it starts there, so v stays
// far negative --, each pass through SubDither4Disp. FF3 0x0017b1c8.
// PC: as SubDither3 -- the span grows with the window (SubDither4Disp
// sizes the quads), the grain switch skips the draws after the sheet and
// the scroll have stepped, and native mode swaps in the renderer's sheet.
// callcheck: fptodp, isnan (DitherScreenMove)
void SubDither4(float alp, float spd, u_char alpmx, u_char colmx)
{
    SPRT_DAT2    sd = { 0, 0.0f, 0.0f, 0.0f, 0.0f, 640.0f, 448.0f, -0.5f, -0.5f, 0xa0, 0x80 };
    DISP_SPRT2   ds;
    GRA3DCAMERA *cam = gra3dGetCamera();
    float tx, ty, otx, oty, mvx, mvy;
    float cy;
    float uspan, vspan;                 // PC: 640 / 512 in the ROM
    int   u, v;

    cy = dither4_cy - 1024.0f;
    if (cy <= -9908638.0f)
        cy = dither4_cy;
    dither4_cy = cy;

    if (!DitherIsFinder())
    {
        if (dither4_fl == 1 || CamChangeCheck() != 0)
            memcpy(dither4_old_cam, cam->vTarget, sizeof(sceVu0FVECTOR));
        dither4_fl = 0;
    }
    else
    {
        if (dither4_fl == 0)
            memcpy(dither4_old_cam, cam->vTarget, sizeof(sceVu0FVECTOR));
        dither4_fl = 1;
    }

    WorldToScreenPos(cam->vTarget, &tx, &ty);
    WorldToScreenPos(dither4_old_cam, &otx, &oty);
    memcpy(dither4_old_cam, cam->vTarget, sizeof(sceVu0FVECTOR));
    DitherScreenMove(tx, ty, otx, oty, &mvx, &mvy);

    dither4_cx = dither4_cx + mvx + mvx;
    if (128.0f < dither4_cx)
        dither4_cx -= 128.0f;
    else if (dither4_cx < 0.0f)
        dither4_cx += 128.0f;
    dither4_cy = dither4_cy + mvy + mvy;
    if (128.0f < dither4_cy)
        dither4_cy -= 128.0f;
    else if (dither4_cy < 0.0f)
        dither4_cy += 128.0f;

    DitherPatternUpdate(alpmx, colmx);
    if (eff_dither.remake != 0)
        return;

    if (ReiPan_RendererGetFilmGrain() == REIPAN_FILM_GRAIN_OFF)   // PC
        goto step;

    DitherSheetSpan(&uspan, &vspan);    // PC
    CopySprDToSpr2(&ds, &sd);
    ds.tex1 = 0x141;
    ds.tex0 = DITHER_TEX0;
    ds.z    = 0xfff00;
    ds.zbuf = 0x10a000118ULL;
    ds.clmp = 0;

    ReiPan_RendererFilmGrainBegin(alpmx, colmx);    // PC
    u = (int)(dither4_cx * 16.0f);
    v = (int)(dither4_cy * 16.0f);
    ds.u1 = (u_short)u;
    ds.v1 = (u_short)v;
    ds.u2 = Dither4PairHi(u, (int)((dither4_cx + uspan) * 16.0f));
    ds.v2 = Dither4PairHi(v, (int)((dither4_cy + vspan) * 16.0f));
    SubDither4Disp(&ds, (u_char)(int)((Vu0Sinf((dither4_cnf * 3.1415927f) / 180.0f) + 1.0f) * alp));

    u = (int)((dither4_cx + 64.0f) * 16.0f);
    v = (int)(dither4_cy * 16.0f);
    ds.u1 = (u_short)u;
    ds.v1 = (u_short)v;
    ds.u2 = Dither4PairHi(u, (int)((dither4_cx + uspan + 64.0f) * 16.0f));
    ds.v2 = Dither4PairHi(v, (int)((dither4_cy + vspan) * 16.0f));
    SubDither4Disp(&ds, (u_char)(int)((Vu0Sinf(((dither4_cnf + 120.0f) * 3.1415927f) / 180.0f) + 1.0f) * alp));

    u = (int)(dither4_cx * 16.0f);
    v = (int)((dither4_cy + 64.0f) * 16.0f);
    ds.u1 = (u_short)u;
    ds.v1 = (u_short)v;
    ds.u2 = Dither4PairHi(u, (int)((dither4_cx + uspan) * 16.0f));
    ds.v2 = Dither4PairHi(v, (int)((dither4_cy + vspan + 64.0f) * 16.0f));
    SubDither4Disp(&ds, (u_char)(int)((Vu0Sinf(((dither4_cnf + 240.0f) * 3.1415927f) / 180.0f) + 1.0f) * alp));
    ReiPan_RendererFilmGrainEnd();      // PC

step:
    if (eff_wrk.stop_flg == 0)
        dither4_cnf += spd;
}

// Kind 2. FF3 0x0017b778; FF2 SetDither3, which FF3 gave a second path:
// dither types 8 and up go to SubDither4. dat: uc8[2] the type, [3] alpmx,
// [4] colmx, fl32[2] the alpha, fl32[3] the speed.
void SetDither3(EFFECT_CONT *ec)
{
    float  alp   = ec->dat.fl32[2];
    float  spd   = ec->dat.fl32[3];
    u_char type  = ec->dat.uc8[2];
    u_char alpmx = ec->dat.uc8[3];
    u_char colmx = ec->dat.uc8[4];

    if ((ec->dat.uc8[1] & 4) != 0)
    {
        if (ec->flow == 1)
        {
        }
        else if (ec->flow == 0)
        {
            alp = (alp * (float)ec->cnt) / (float)ec->in;
        }
        else if (ec->flow == 2)
        {
            alp = (alp * (float)(ec->out - ec->cnt)) / (float)ec->out;
        }
        else
        {
            alp = 0.0f;
            if (ec->flow == 3)
                ResetEffects(ec);   // FF3 goes on: the envelope step and a zero-alpha draw follow
        }
        EffInKeepOutFlowCtrl(ec);
    }

    if (eff_wrk._14 == 0)
    {
        if (type < 8)
            SubDither3(type, alp, spd, alpmx, colmx);
        else
            SubDither4(alp, spd, alpmx, colmx);
    }
    if ((ec->dat.uc8[1] & 1) != 0)
        ResetEffects(ec);
}

// Kind 0x11 (FF3-only). FF3 0x0017b970: the old-film noise of
// effect_scrsaver.c -- fw[0] / fw[1] the dust / scratch-line rates, dat bytes
// 2 / 3 their alphas -- and fw[2] > 0 the frame shake at that rate.
void FUN_0017b970(EFFECT_CONT *ec)
{
    float rate = ec->fw[2];

    EffFilmNoiseDraw(ec->fw[0], ec->fw[1], ec->dat.uc8[2], ec->dat.uc8[3]);
    if (rate > 0.0f)
        EffFilmShakeDraw(rate);
    if ((ec->dat.uc8[1] & 1) != 0)
        ResetEffects(ec);
}

// Kind 6. FF3 0x0017b9f0; FF2 SetOverRap. FF3: a camera jump is 5.12 units
// (FF2 128), the registers through SetGsRegsCached, the sprites through
// EffDispSprD2Direct, EffImageHalf32 takes a blend argument, and a one-frame
// request ends only once the cross-fade is over (FF2: at once).
// callcheck: __fixunssfdi (the float -> u_long cast of the alpha)
void SetOverRap(EFFECT_CONT *ec)
{
    static const GS_REG_SET regs[5] = {
        { 0x8000000011ULL, 0x42 },      // ALPHA_1
        { 0x100000161ULL,  0x14 },      // TEX1_1
        { 5,               0x08 },      // CLAMP_1
        { 0x30003,         0x47 },      // TEST_1
        { 0x10a000118ULL,  0x4e },      // ZBUF_1
    };
    float     *cpos;
    float      x, y, z;
    float      fn;
    int        fl;
    DISP_SPRT2 ds;

    fl = 0;
    cpos = gra3dcamGetPosition();
    overlap_passflg[0] = 1;
    x = cpos[0];
    z = cpos[2];
    y = cpos[1];
    if (fabsf(overlap_cx - x) > 5.12f)
        fl = 1;
    else if (fabsf(overlap_cy - y) > 5.12f)
        fl = 1;
    else if (fabsf(overlap_cz - z) > 5.12f)
        fl = 1;

    if (GetApproachCameraCrossFade() != 0)
    {
        fl = 1;
        ApproachCameraCrossFadeSW(0);
    }
    overlap_cx = x;
    overlap_cy = y;
    overlap_cz = z;

    if ((fl != 0 && overlap_alp <= 0.0f) || overlap_passflg[0] != overlap_passflg[1])
    {
        // Grab the frame just shown and halve it into the EE stash 0x10ef000.
        LocalCopyLtoB(0, 0, ((GetSysWrk()->count + 1) & 1) * 0x1180);
        EffImageHalf32((u_int *)ReiPan_GetHostPointer(0x10ef000), 0x280, 0x1c0, 0);
        overlap_alp = 128.0f;
    }

    if (overlap_alp == 128.0f)
    {
        // First frame: the old picture still owns the screen.
        SPRT_DAT2 sd = { 0, 0.099999994f, 0.099999994f, 639.89996f, 447.9f,
                         640.0f, 448.0f, -0.5f, -0.5f, 0xa0, 0x80 };

        SetGsRegsCached(regs, 5, 1);
        CopySprDToSpr2(&ds, &sd);
        ds.z      = 0xfff00;
        ds.tex1   = 0x141;
        ds.alpreg = 0x44;
        ds.tex0   = (u_long)(u_int)(((GetSysWrk()->count + 1) & 1) * 0x1180 | 0x28128000) | 0x2000000240000000ULL;
    }
    else
    {
        if (overlap_alp <= 0.0f)
            goto step;

        // The cross-fade: the stash back into the work page, blended.
        SPRT_DAT2 sd = { 0, 0.099999994f, 0.099999994f, 319.9f, 447.9f,
                         640.0f, 448.0f, -0.5f, -0.5f, 0xa0, 0x80 };

        LocalCopyBtoL(2, 0, 0x2bc0);
        CopySprDToSpr2(&ds, &sd);
        ds.tex0   = 0x2000000264116bc0ULL;
        ds.z      = 0xfff00;
        ds.alpreg = ((u_long)overlap_alp << 32) | 100;
    }
    EffDispSprD2Direct(&ds);

step:
    fn = 8.0f;
    if (ec->dat.uc8[2] != 0)
        fn = 128.0f / (float)ec->dat.uc8[2];
    overlap_alp -= fn;
    if (overlap_alp <= 0.0f)
        overlap_alp = 0.0f;

    if ((ec->dat.uc8[1] & 1) != 0 && overlap_alp == 0.0f)
        ResetEffects(ec);
}

// FF3 0x0017bdc8 (FF2 ChangeMonochrome was empty).
void EffectMonochromeSet(int on, int prelight)
{
    int old = gra3dIsMonotoneDrawEnable();

    eff_wrk.monochro_mode = (on != 0);
    gra3dMonotoneDrawEnable(on);
    if (prelight != 0 && old != on)
        gra3dPrelight(0);
}

// The rectangle of BrightnessAdjustmentFilterDraw (FF3 0x0017bf40, reached
// by a tail jump only): at or below 0x80 a black darkening (ALPHA 0x44,
// (Cs - Cd) As + Cd), above it a brightening (ALPHA 0x49, (Cd - 0) As + Cd).
static void BrightnessFilterRectDraw(u_char bright, int x, int y, int w, int h, float scale)
{
    SQAR_DAT  sq;
    DISP_SQAR ds;
    int       a;

    if (scale <= 0.0f)
        return;
    if (1.0f < scale)
        scale = 1.0f;

    sq.w     = w;
    sq.h     = h;
    sq.x     = x;
    sq.y     = y;
    sq.pri   = 0;
    sq.r     = 0;
    sq.g     = 0;
    sq.b     = 0;
    sq.alpha = 0;
    CopySqrDToSqr(&ds, &sq);
    ds.test = 0x30003;
    if (bright < 0x81)
    {
        ds.alphar = 0x44;
        a = 0x80 - bright;
    }
    else
    {
        ds.alphar = 0x49;
        a = bright * 2 - 0x101;
    }
    ds.alpha = (u_char)(int)((float)a * scale);
    DispSqrD(&ds);
}

// FF3 0x0017be50; FF2 BrightnessAdjustmentFilterDraw took no arguments and
// read the option byte as the alpha. FF3 maps the brightness level (option
// byte 0, 0..11, 6 the default) over the world's three steps: at the
// default 0x55, a 0x2b black filter in both worlds.
void BrightnessAdjustmentFilterDraw(float scale, int x, int y, int w, int h)
{
    const u_char *t = brightness_tbl[(signed char)ingame_wrk.b03];
    u_int         level = GetOptionWrk()->now.b00;
    int           bright;

    if (level < 7)
        bright = t[0] + ((t[1] - t[0]) * (int)level) / 6;
    else
        bright = t[1] + ((t[2] - t[1]) * ((int)level - 6)) / 5;
    BrightnessFilterRectDraw((u_char)bright, x, y, w, h, scale);
}

// FF3 0x0017c028.
void ScreenFlashReq(u_char alpha, int time, u_char r, u_char g, u_char b, int mode)
{
    screen_flash.mode = mode;
    if (GetPALMode() != 0)
        time = (int)((float)time / GetFrameTimeScale());
    screen_flash.time  = time;
    screen_flash.alpha = alpha;
    screen_flash.r     = r;
    screen_flash.g     = g;
    screen_flash.b     = b;
    screen_flash.cnt   = 0;
}

// FF3 0x0017c0f0. Mode 2 keeps the alpha when it ends, so a fade-in holds
// the colour until the next request.
int ScreenFlashDraw(void)
{
    u_int a = screen_flash.alpha;
    int   over;

    if (screen_flash.mode == 1)
    {
        if (screen_flash.time == 0)
            a = 0;
        else
            a = (int)((screen_flash.time - screen_flash.cnt) * (u_int)screen_flash.alpha) / screen_flash.time;
        if (screen_flash.time <= screen_flash.cnt)
            screen_flash.mode = 0;
        if (screen_flash.mode == 0)
            screen_flash.alpha = 0;
        screen_flash.cnt++;
    }
    else if (screen_flash.mode == 2)
    {
        if (screen_flash.time != 0)
            a = (int)(screen_flash.cnt * (u_int)screen_flash.alpha) / screen_flash.time;
        over = screen_flash.time <= screen_flash.cnt;
        screen_flash.cnt++;
        if (over)
            screen_flash.mode = 0;
    }

    if ((int)a > 0)
        DispSqrRect(0.0f, 0.0f, 640.0f, 448.0f, 0x10,
                    screen_flash.r, screen_flash.g, screen_flash.b, (u_char)a);
    return screen_flash.mode;
}

// FF3 0x0017c218.
void ScreenFadeToWhite(int time)
{
    ScreenFlashReq(0x80, time, 0xff, 0xff, 0xff, 2);
}

// FF3 0x0017c248.
void ScreenFlashWhite(int time)
{
    ScreenFlashReq(0x80, time, 0xff, 0xff, 0xff, 1);
}

// FF3 0x0017c278.
void ScreenFadeToBlack(int time)
{
    ScreenFlashReq(0x80, time, 0, 0, 0, 2);
}

// FF3 0x0017c2a8.
void ScreenFlashBlack(int time)
{
    ScreenFlashReq(0x80, time, 0, 0, 0, 1);
}

// FF3 0x0017c2d8 (no references).
void ScreenFlashWhiteShort(void)
{
    ScreenFlashReq(0x80, 0x12, 0xff, 0xff, 0xff, 1);
}

// ──────────────────────────────────────────────────────────────────────
// An unreferenced deform kernel and three small functions
// ──────────────────────────────────────────────────────────────────────

static DEFWORK deform_ripple_dw[25][33];    // bss 0x3d9fd8
static int     deform_ripple_init = 1;      // sdata 0x36ef40
static float   deform_ripple_r;             // sdata 0x36ef38

#define DEFORM_RIPPLE_TEX0 0x200000066802abc0ULL    // the copy at 0x2bc0, 640 wide

// A ripple of 12 crests out of the screen centre whose swing turns 18
// times round it, over a 33 x 25 grid on the full-width copy at 0x2bc0,
// r steps 0.006 rad (no stop test). FF3 takes no copy here and draws
// whatever rate and alp are. No references in FF3. FF3 0x0017c308.
void SetDeformScrRipple(float rate, u_char alp)
{
    static SCRDEF g[25][33];            // PC: static, the ROM stacks 0x6790 bytes
    float lm, inv;
    int   i, j;

    for (i = 0; i < 25; i++)
    {
        for (j = 0; j < 33; j++)
        {
            g[i][j].stq[1] = ((float)i * 448.0f) / 24.0f;
            g[i][j].stq[0] = (float)j * 640.0f * 0.03125f;
        }
        g[i][0].stq[0]  += 1.0f;
        g[i][32].stq[0] -= 1.0f;
    }
    for (j = 0; j < 33; j++)
    {
        g[0][j].stq[1]  += 1.0f;
        g[24][j].stq[1] -= 1.0f;
    }

    rate *= 0.25f;
    lm  = sqrtf(152576.0f);             // |(320, 224)|
    inv = 1.0f / lm;
    if (deform_ripple_init != 0)
    {
        for (i = 0; i < 25; i++)
        {
            float fy = ((float)i * 18.666666f - 224.0f) + -0.5f;

            for (j = 0; j < 33; j++)
            {
                float    fx = ((float)j * 20.0f - 320.0f) + -0.5f;
                DEFWORK *dw = &deform_ripple_dw[i][j];

                dw->lll = sqrtf(fx * fx + fy * fy);
                dw->rrr = g3dAtan2f(fx, fy);
                dw->mm2 = (lm - dw->lll) * inv;
                dw->mm1 = dw->lll * 3.1415927f * 12.0f * inv;
                dw->sss = Vu0Sinf(dw->rrr);
                dw->ccc = Vu0Cosf(dw->rrr);
            }
        }
        deform_ripple_init = 0;
    }

    for (i = 0; i < 25; i++)
    {
        for (j = 0; j < 33; j++)
        {
            const DEFWORK *dw = &deform_ripple_dw[i][j];
            float a = Vu0Sinf(dw->mm1 - deform_ripple_r) * rate;

            a = a * dw->mm2 * Vu0Sinf((deform_ripple_r + dw->rrr) * 18.0f);
            g[i][j].vtw[2] = 0.0f;
            g[i][j].vtw[3] = 0.0f;
            g[i][j].vtw[0] = (dw->sss * dw->lll - dw->ccc * a) + 2048.0f;
            g[i][j].vtw[1] = (dw->ccc * dw->lll - dw->sss * a) + 2048.0f;
        }
    }

    deform_ripple_r += 0.006f;
    if (6.2831855f < deform_ripple_r)
        deform_ripple_r -= 6.2831855f;

    MakeScrDeformPacket(32, 24, DEFORM_RIPPLE_TEX0, g, alp);
}

// FF3 0x0017c738 (no references).
void EffScrNop0(void)
{
}

// FF3 0x0017c740 (no references).
void EffScrNop1(void)
{
}

static int eff_scr_unused;              // sdata 0x36efc4: written here only

// FF3 0x0017c748 (no references).
void EffScrUnusedSet(int v)
{
    eff_scr_unused = v;
}

// ──────────────────────────────────────────────────────────────────────
// The sky
// ──────────────────────────────────────────────────────────────────────

// FF3 0x0017c750.
void EffSoraTexSend(u_int n)
{
    if (1 < n)
        n = 0;
    if (sora_pk != nullptr)
        Tim2PackSend(sora_pk, n, 0, -1, -1);
    sora_tex0 = sora_sprt[n].tex0;
}

// FF3 0x0017c7b0.
// callcheck: EFFECT_MALLOC (read as a macro)
int EffSoraLoadReq(void)
{
    void *p;

    sora_ready = 0;
    sora_b8    = 0;
    sora_bc    = 0;
    p = EFFECT_MALLOC(GetFileSize(SORA_PK4));
    sora_pk = p;
    if (p != nullptr)
        sora_load_id = LoadReq(SORA_PK4, p);
    return p != nullptr;
}

// FF3 0x0017c808.
// callcheck: EFFECT_FREE (read as a macro)
void EffSoraRelease(void)
{
    if (sora_pk != nullptr)
    {
        EFFECT_FREE(sora_pk);
        sora_pk      = nullptr;
        sora_load_id = -1;
    }
    sora_ready = 0;
}

// FF3 0x0017c840.
int EffSoraIsLoadEnd(void)
{
    int r = 0;

    if (sora_pk != nullptr)
    {
        if (sora_load_id >= 0 && IsLoadEnd(sora_load_id) == 0)
            return 0;
        r = 1;
        sora_ready = 1;
    }
    return r;
}

// FF3 0x0017d908.
int EffObjBufTblIsSet(int area)
{
    return EffObjBufTblFind(area) >= 0;
}

// FF3 0x0017c890. The sky shows outside world 1 in the areas with a room
// record (EffObjBufTbl), the player's or the other map buffer's.
void EffSoraDraw(void)
{
    static const GS_REG_SET regs[5] = {
        { 0x44,      0x42 },            // ALPHA_1
        { 0x161,     0x14 },            // TEX1_1
        { 0x1fc7f0,  0x08 },            // CLAMP_1
        { 0x5000d,   0x47 },            // TEST_1
        { 0xa000118, 0x4e },            // ZBUF_1
    };
    GRA3DCAMERA  *cam;
    float        *clip;
    sceVu0FVECTOR rot;
    EFF_SORA_DAT *cur;
    int           area;

    if (sora_ready == 0)
    {
        EffSoraIsLoadEnd();
        return;
    }

    cur = nullptr;
    if (ingame_wrk.b03 != 1)
    {
        area = MapAreaNoGet();
        if (area >= 0 && EffObjBufTblIsSet(area) == 0)
            area = MapLoadGetRoomArea(MapLoadGetBuffID(area) ^ 1);
        if (area >= 0 && EffObjBufTblIsSet(area) != 0)
            cur = &sora_dat;
    }
    sora_cur = cur;
    if (cur == nullptr)
        return;

    cam = gra3dGetCamera();
    GetTrgtRot(gra3dcamGetPosition(), cam->vTarget, rot, 3);
    clip = _GetClipVolumeV();
    g3dCalcViewScreenMatrixPerspective(sora_view_screen, clip[1] / tanf(0.4175f),
                                       cam->fAspectX, cam->fAspectY, cam->fCenterX, cam->fCenterY,
                                       cam->fZmin, cam->fZmax, cam->fNearZ, cam->fFarZ);
    SetGsRegsCached(regs, 5, 1);
    EffSoraTexSend(0);
    EffSoraDrawSub(sora_cur, rot);  // FF3 passes &sora_cur->dist
}

// The sky grid (FF3's copy of FF2's MapSky grid, out of line at 0x2b61a0):
// an nx x ny vertex grid over the rectangle rect[0]..rect[3] at height
// rect[0][1], projected through m (12.4 GS coordinates; a vertex outside
// x 0..60000 / y 0..45000 is culled by x = -1, otherwise z 0 and the fog
// 4000 - w / 4, clamped at 0); then one RGBAQ and, per cell whose four
// corners survive and which faces the camera, one TRIFAN with fog, UV
// running cu[0]..cu[1] x cv[0]..cv[3] texels (x 16) plus (u0, v0).
// PC: each cell also goes to the renderer as a textured quad under
// EffSoraDraw's TEX1 / CLAMP (REPEAT: the 128-texel picture tiles four times
// and scrolls), z 0 against the draw env's GEQUAL. The host quad has one
// colour and no fog, so each cell's colour is scaled by its corners' mean F
// / 256 (the GS fogs to FOGCOL, measured 22 grey in rs101: the small FOGCOL
// term is dropped). The GS's F is 8 bits of the value: a vertex behind the
// camera (w < 0) gets 4000 + |w| / 4 and wraps, as on the PS2.
// FF3 0x002b61a0.
static u_long *EffSoraMakeGridPacket(u_long *p, sceVu0FMATRIX m, sceVu0FVECTOR *rect,
                                     const int *cu, const int *cv, int nx, int ny,
                                     int u0, int v0, int r, int g, int b, int a)
{
    static const u_long sora_tex1  = 0x161;     // EffSoraDraw's TEX1_1
    static const u_long sora_clamp = 0x1fc7f0;  // and CLAMP_1 (REPEAT, REPEAT)
    static const int    corner_u[4] = { 0, 1, 1, 0 };   // a, b, c, d
    static const int    corner_v[4] = { 0, 0, 1, 1 };
    sceVu0FVECTOR pos;
    float  dx, dz;
    int    du, dv;
    int    idx[4];
    int    u, v;
    int    i, j, k, n;
    u_int *q;

    dx = (rect[1][0] - rect[0][0]) / (float)nx;
    dz = (rect[3][2] - rect[0][2]) / (float)ny;
    du = ((cu[1] - cu[0]) * 16) / nx;
    dv = ((cv[3] - cv[0]) * 16) / ny;

    p[0] = 0x1000000000008001ULL;       // GIF tag: 1 loop, EOP, PACKED RGBAQ
    p[1] = 1;
    q = (u_int *)(p + 2);
    q[0] = (u_int)r;
    q[1] = (u_int)g;
    q[2] = (u_int)b;
    q[3] = (u_int)a;
    p += 4;

    pos[0] = rect[0][0];
    pos[1] = rect[0][1];
    pos[2] = rect[0][2];
    pos[3] = 1.0f;                      // rdata 0x36efe0
    k = 0;
    for (j = 0; j < ny; j++)
    {
        pos[0] = rect[0][0];
        for (i = 0; i < nx; i++, k++)
        {
            int *gv = sora_grid[k];

            sceVu0RotTransPers(gv, m, pos, 0);
            pos[0] += dx;
            if ((u_int)gv[0] > 60000 || (u_int)gv[1] > 45000)
            {
                gv[0] = -1;
            }
            else
            {
                int f = 4000 - (gv[3] >> 6);

                gv[2] = 0;
                gv[3] = f < 0 ? 0 : f;
            }
        }
        pos[2] += dz;
    }

    // The corners of a cell: a (i, j), b (i + 1, j), c (i + 1, j + 1),
    // d (i, j + 1), stepped by 1 per cell and 1 more per row (data 0x354a60).
    idx[0] = 0;
    idx[1] = 1;
    idx[2] = nx + 1;
    idx[3] = nx;
    v = cv[0] * 16 + v0;
    for (j = 0; j < ny - 1; j++)
    {
        u = cu[0] * 16 + u0;
        for (i = 0; i < nx - 1; i++)
        {
            int *cell[4];

            cell[0] = sora_grid[idx[0]];
            cell[1] = sora_grid[idx[1]];
            cell[2] = sora_grid[idx[2]];
            cell[3] = sora_grid[idx[3]];
            if ((cell[0][0] | cell[1][0] | cell[2][0] | cell[3][0]) >= 0)
            {
                u_int ex = (u_int)(cell[1][0] - cell[0][0]);
                u_int ey = (u_int)(cell[1][1] - cell[0][1]);
                u_int fx = (u_int)(cell[2][0] - cell[1][0]);
                u_int fy = (u_int)(cell[2][1] - cell[1][1]);

                // The facing test in the EE's 32-bit mult (the low words).
                if ((int)(ex * fy - ey * fx) >= 0)
                {
                    // GIF tag (rdata 0x36efd8): 1 loop, EOP, PRIM 0x17d
                    // (TRIFAN, Gouraud, textured, fog, blended, UV),
                    // UV / XYZF2 x 4.
                    p[0] = 0x80bec00000008001ULL;
                    p[1] = 0x43434343;
                    q = (u_int *)(p + 2);
                    for (n = 0; n < 4; n++)
                    {
                        q[0] = (u_int)(u + corner_u[n] * du);
                        q[1] = (u_int)(v + corner_v[n] * dv);
                        q[2] = 0;
                        q[3] = 0;
                        q[4] = (u_int)cell[n][0];
                        q[5] = (u_int)cell[n][1];
                        q[6] = (u_int)cell[n][2];
                        q[7] = (u_int)cell[n][3];
                        q += 8;
                    }
                    p += 18;

                    // PC: the cell on the renderer, corners in its quad order
                    // (TL, TR, BL, BR = a, b, d, c), GS window -> 640x448.
                    {
                        static const int order[4] = { 0, 1, 3, 2 };
                        float xy[8];
                        float uv[8];
                        float f = 0.0f;

                        for (n = 0; n < 4; n++)
                        {
                            f += (float)((cell[n][3] >> 4) & 0xff);     // the GS's 8-bit F
                        }
                        f /= 4.0f * 256.0f;
                        for (n = 0; n < 4; n++)
                        {
                            int c = order[n];

                            xy[n * 2 + 0] = (float)cell[c][0] / 16.0f - 1728.0f;
                            xy[n * 2 + 1] = (float)cell[c][1] / 16.0f - 1824.0f;
                            uv[n * 2 + 0] = (float)(u + corner_u[c] * du) / 16.0f;
                            uv[n * 2 + 1] = (float)(v + corner_v[c] * dv) / 16.0f;
                        }
                        ReiPan_RendererSetGs2dDepth(0);
                        ReiPan_RendererDrawTexturedQuadClamp((const sceGsTex0 *)&sora_tex0,
                                                             (const sceGsTex1 *)&sora_tex1,
                                                             xy, uv, sora_clamp,
                                                             (u_char)(r * f), (u_char)(g * f),
                                                             (u_char)(b * f), (u_char)a);
                    }
                }
            }
            u += du;
            for (n = 0; n < 4; n++)
                idx[n]++;
        }
        v += dv;
        for (n = 0; n < 4; n++)
            idx[n]++;
    }
    return p;
}

// The sky's frame: U scrolls one picture width (0x800 / 16 = 128 texels)
// every `period` frames; nothing is drawn while the camera looks down more
// than 0.3 rad; the grid plane sits at dist in a frame turned by the
// camera's yaw and pitch, through EffSoraDraw's view -> screen matrix; the
// UV spans scale x 128 texels each way.
// FF3 0x002b5f10.
void EffSoraDrawSub(EFF_SORA_DAT *dat, float *rot)
{
    static sceVu0FMATRIX unit = {       // data 0x3293c0
        { 1.0f, 0.0f, 0.0f, 0.0f },
        { 0.0f, 1.0f, 0.0f, 0.0f },
        { 0.0f, 0.0f, 1.0f, 0.0f },
        { 0.0f, 0.0f, 0.0f, 1.0f },
    };
    sceVu0FMATRIX m;
    int     cu[4], cv[4];
    int     scroll;
    int     s;
    int     r, g, b;
    u_long *p;

    scroll = 0;
    if (dat->period > 0)
    {
        scroll = ((sora_scroll << 12) / dat->period) & 0x7ff;
        sora_scroll++;
        if (sora_scroll >= dat->period)
            sora_scroll -= dat->period;
    }
    if (rot[0] < -0.3f)                 // rdata 0x36efd0
        return;

    sora_rect[0][1] = dat->dist;
    s = (int)(dat->scale * 128.0f);     // rdata 0x36efd4
    cu[0] = 0;  cu[1] = s;  cu[2] = s;  cu[3] = 0;
    cv[0] = 0;  cv[1] = 0;  cv[2] = s;  cv[3] = s;
    sceVu0RotMatrixY(m, unit, NormalizeAngle(-rot[1]));
    sceVu0RotMatrixX(m, m, NormalizeAngle(-rot[0]));
    // PC: the sky's camera before its projection, so frame smoothing can turn
    // the grid with an in-between camera (ReiPan_RendererSetSkyCamera); the
    // host quads take GS pixels less the XYOFFSET.
    ReiPan_RendererSetSkyCamera(&m[0][0], &sora_view_screen[0][0], 1728.0f, 1824.0f);
    sceVu0MulMatrix(m, sora_view_screen, m);

    p = (u_long *)StartDmaDirectTrans();
    p[0] = 0x1000000000008002ULL;       // GIF tag: 2 A+D registers
    p[1] = 0xe;
    p[2] = 0;
    p[3] = 0x3f;                        // TEXFLUSH
    p[4] = sora_tex0;
    p[5] = 6;                           // TEX0_1
    r = dat->r;
    g = dat->g;
    b = dat->b;
    if (gra3dIsMonotoneDrawEnable() != 0)
    {
        r = (r + g + b) / 3;
        g = r;
        b = r;
    }
    ReiPan_RendererBeginSkyLayer();     // PC
    p = EffSoraMakeGridPacket(p + 6, m, sora_rect, cu, cv, SORA_GRID_N, SORA_GRID_N,
                              scroll, 0, r, g, b, dat->alpha);
    ReiPan_RendererEndSkyLayer();       // PC
    EndDmaDirectTrans((qword *)p);
}

// ──────────────────────────────────────────────────────────────────────
// The sound-bank list
// ──────────────────────────────────────────────────────────────────────

// FF3 0x0017ca48.
void SndBankListInit(void)
{
    SingleLinkListInit(&snd_bank_list_a, 0xc);
    SingleLinkListInit(&snd_bank_list_b, 4);
    SingleLinkListInit(&snd_bank_list_c, 0x30);
}

// FF3 0x0017ca90.
void SndBankListRelease(void)
{
    SndBankListBufAllStop();
    SndBankListAllRelease();
    SndBankListReleaseReqClear();
}

// FF3 0x0017cab8.
void SndBankListMain(void)
{
    SndBankListPlayStep();
    SndBankListReadyUpdate();
    SndBankListReleaseStep();
}

// FF3 0x0017cae0.
int SndBankListIsBank(int id)
{
    SLL_CELL *cell;

    for (cell = SingleLinkListBeginCell(&snd_bank_list_a); cell != nullptr; cell = cell->pNext)
    {
        if (((SND_BANK_ENT *)SingleLinkListCellBodyPtr(cell))->id == id)
            return 1;
    }
    return 0;
}

// FF3 0x0017cb70.
void SndBankListLoadReq(int id)
{
    SND_BANK_ENT  ent;
    SLL_CELL     *cell;

    SndBankListReleaseReqCancel(id);
    if (SndBankListIsBank(id) != 0)
        return;
    ent.id = id;
    ent.bank = -1;
    ent.ready = 0;
    cell = SingleLinkListAddEnd(&snd_bank_list_a, &ent);
    if (cell != nullptr)
        ((SND_BANK_ENT *)SingleLinkListCellBodyPtr(cell))->bank = SndBankNew(id, id - 1, -1);
}

// FF3 0x0017cbf0.
void SndBankListRemoveBank(int id)
{
    SLL_CELL *cell;
    SLL_CELL *next;

    cell = SingleLinkListBeginCell(&snd_bank_list_a);
    if (cell == nullptr)
        return;
    next = cell->pNext;
    while (true)
    {
        if (((SND_BANK_ENT *)SingleLinkListCellBodyPtr(cell))->id == id)
            SingleLinkListRemove(&snd_bank_list_a, cell);
        if (next == nullptr)
            break;
        cell = next;
        next = next->pNext;
    }
}

// The bank handle of the id's cell, -1 when the id is not listed.
// FF3 0x0017cc88.
int SndBankListGetBank(int id)
{
    SLL_CELL      *cell;
    SND_BANK_ENT  *ent;

    for (cell = SingleLinkListBeginCell(&snd_bank_list_a); cell != nullptr; cell = cell->pNext)
    {
        ent = (SND_BANK_ENT *)SingleLinkListCellBodyPtr(cell);
        if (ent->id == id)
            return ent->bank;
    }
    return -1;
}

// FF3 0x0017cd18.
int SndBankListIsReady(int id)
{
    SLL_CELL      *cell;
    SND_BANK_ENT  *ent;

    for (cell = SingleLinkListBeginCell(&snd_bank_list_a); cell != nullptr; cell = cell->pNext)
    {
        ent = (SND_BANK_ENT *)SingleLinkListCellBodyPtr(cell);
        if (ent->id == id && ent->ready != 0)
            return 1;
    }
    return 0;
}

// FF3 0x0017cda8.
void SndBankListReadyUpdate(void)
{
    SLL_CELL      *cell;
    SND_BANK_ENT  *ent;

    for (cell = SingleLinkListBeginCell(&snd_bank_list_a); cell != nullptr; cell = cell->pNext)
    {
        ent = (SND_BANK_ENT *)SingleLinkListCellBodyPtr(cell);
        if (ent->ready == 0 && SndBankIsReady(ent->bank) != 0)
            ent->ready = 1;
    }
}

// Every listed bank released and its cell removed.
// FF3 0x0017ce38.
void SndBankListAllRelease(void)
{
    SLL_CELL      *cell;
    SLL_CELL      *next;
    SND_BANK_ENT  *ent;
    int            bank;

    cell = SingleLinkListBeginCell(&snd_bank_list_a);
    if (cell == nullptr)
        return;
    next = cell->pNext;
    while (true)
    {
        ent = (SND_BANK_ENT *)SingleLinkListCellBodyPtr(cell);
        bank = SndBankListGetBank(ent->id);
        if (bank != -1)
            SndBankRelease(bank);
        SingleLinkListRemove(&snd_bank_list_a, cell);
        if (next == nullptr)
            break;
        cell = next;
        next = next->pNext;
    }
}

// FF3 0x0017ced8.
void SndBankListReleaseReq(int id)
{
    int body;

    if (SndBankListIsBank(id) == 0 || SndBankListIsReleaseReq(id) != 0)
        return;
    body = id;
    SingleLinkListAddEnd(&snd_bank_list_b, &body);
}

// FF3 0x0017cf28.
int SndBankListIsReleaseReq(int id)
{
    SLL_CELL *cell;

    for (cell = SingleLinkListBeginCell(&snd_bank_list_b); cell != nullptr; cell = cell->pNext)
    {
        if (*(int *)SingleLinkListCellBodyPtr(cell) == id)
            return 1;
    }
    return 0;
}

// FF3 0x0017cfb8.
void SndBankListReleaseReqCancel(int id)
{
    SLL_CELL *cell;
    SLL_CELL *next;

    cell = SingleLinkListBeginCell(&snd_bank_list_b);
    if (cell == nullptr)
        return;
    next = cell->pNext;
    while (true)
    {
        if (*(int *)SingleLinkListCellBodyPtr(cell) == id)
            SingleLinkListRemove(&snd_bank_list_b, cell);
        if (next == nullptr)
            break;
        cell = next;
        next = next->pNext;
    }
}

// The release requests dropped.
// FF3 0x0017d050.
void SndBankListReleaseReqClear(void)
{
    SLL_CELL *cell;
    SLL_CELL *next;

    for (cell = SingleLinkListBeginCell(&snd_bank_list_b); cell != nullptr; cell = next)
    {
        next = cell->pNext;
        SingleLinkListRemove(&snd_bank_list_b, cell);
    }
}

// A requested bank is released once no play of it is listed.
// FF3 0x0017d0b0.
void SndBankListReleaseStep(void)
{
    SLL_CELL *cell;
    SLL_CELL *next;
    int      *id;
    int       bank;

    for (cell = SingleLinkListBeginCell(&snd_bank_list_b); cell != nullptr; cell = next)
    {
        next = cell->pNext;
        id = (int *)SingleLinkListCellBodyPtr(cell);
        if (SndBankListIsPlayReq(*id) != 0)
            continue;
        bank = SndBankListGetBank(*id);
        if (bank != -1)
            SndBankRelease(bank);
        SndBankListRemoveBank(*id);
        SingleLinkListRemove(&snd_bank_list_b, cell);
    }
}

// A ready bank plays at once (vol 0x3200, pitch 0x1000), its record keeping
// the buffer; otherwise the record waits with buf -1 for SndBankListPlayStep
// (and an unlisted bank is loaded). A record the list cannot take stops the
// sound again.
// FF3 0x0017d170.
void SndBankListPlay(int id, int no, int effect, int arg, float *pos, int key)
{
    SND_BANK_REC  rec;
    SND_3D_SET    set;
    SND_3D_SET   *s3s;
    int           bank;
    int           loop;
    int           buf;

    bank = SndBankListGetBank(id);
    memset(&rec, 0, sizeof(rec));       // PC: the ROM leaves pos unset when it is nullptr
    if (pos != nullptr)
    {
        rec.pos[0] = pos[0];            // lq / sq
        rec.pos[1] = pos[1];
        rec.pos[2] = pos[2];
        rec.pos[3] = pos[3];
    }
    rec.has_pos = pos != nullptr;
    rec.id = id;
    rec.no = no;
    rec.effect = effect;
    rec.buf = -1;
    rec.arg = arg;
    rec.key = key;
    if (bank == -1)
    {
        SndBankListLoadReq(id);
    }
    else
    {
        loop = SndBankIsLoopSnd(bank, no) != 0;
        if (SndBankListIsReady(id) != 0)
        {
            memset(&set, 0, sizeof(set));
            s3s = nullptr;
            if (pos != nullptr)
            {
                set.pos = pos;
                s3s = &set;
            }
            buf = SndBankPlay(bank, no, effect, loop, 0x3200, 0x1000, 0, s3s);
            rec.buf = buf;
            if (SingleLinkListAddEnd(&snd_bank_list_c, &rec) == nullptr)
                SndBufStop(buf);
            return;
        }
    }
    SingleLinkListAddEnd(&snd_bank_list_c, &rec);
}

// FF3 0x0017d2d0.
void SndBankListPlayNoKey(int id, int no, int effect, int arg, float *pos)
{
    SndBankListPlay(id, no, effect, arg, pos, 0);
}

// FF3 0x0017d2e8.
void SndBankListPlayKey(int id, int no, int effect, int arg, float *pos, int key)
{
    SndBankListPlay(id, no, effect, arg, pos, key);
}

// The plays of (id, no): a waiting one is dropped, a playing one stopped
// (fade: over 60 frames); SndBankListPlayStep drops it once it has ended.
// FF3 0x0017d300.
void SndBankListStop(int id, int no, int fade)
{
    SLL_CELL      *cell;
    SLL_CELL      *next;
    SND_BANK_REC  *rec;

    cell = SingleLinkListBeginCell(&snd_bank_list_c);
    if (cell == nullptr)
        return;
    next = cell->pNext;
    while (true)
    {
        rec = (SND_BANK_REC *)SingleLinkListCellBodyPtr(cell);
        if (rec->id == id && rec->no == no)
        {
            if (rec->buf == -1)
                SingleLinkListRemove(&snd_bank_list_c, cell);
            else if (fade == 0)
                SndBufStop(rec->buf);
            else
                SndBufFadeStop(rec->buf, 0x3c);
        }
        if (next == nullptr)
            break;
        cell = next;
        next = next->pNext;
    }
}

// SndBankListStop for the plays of key.
// FF3 0x0017d3f8.
void SndBankListStopKey(int key, int fade)
{
    SLL_CELL      *cell;
    SLL_CELL      *next;
    SND_BANK_REC  *rec;

    cell = SingleLinkListBeginCell(&snd_bank_list_c);
    if (cell == nullptr)
        return;
    next = cell->pNext;
    while (true)
    {
        rec = (SND_BANK_REC *)SingleLinkListCellBodyPtr(cell);
        if (rec->key == key)
        {
            if (rec->buf == -1)
                SingleLinkListRemove(&snd_bank_list_c, cell);
            else if (fade == 0)
                SndBufStop(rec->buf);
            else
                SndBufFadeStop(rec->buf, 0x3c);
        }
        if (next == nullptr)
            break;
        cell = next;
        next = next->pNext;
    }
}

// FF3 0x0017d4d8.
void SndBankListBufAllStop(void)
{
    SLL_CELL      *cell;
    SLL_CELL      *next;
    SND_BANK_REC  *body;

    cell = SingleLinkListBeginCell(&snd_bank_list_c);
    if (cell == nullptr)
        return;
    next = cell->pNext;
    while (true)
    {
        body = (SND_BANK_REC *)SingleLinkListCellBodyPtr(cell);
        if (body->buf != -1)
            SndBufStop(body->buf);
        SingleLinkListRemove(&snd_bank_list_c, cell);
        if (next == nullptr)
            break;
        cell = next;
        next = next->pNext;
    }
}

// FF3 0x0017d578.
void SndBankListPauseAll(void)
{
    SLL_CELL      *cell;
    SND_BANK_REC  *rec;

    for (cell = SingleLinkListBeginCell(&snd_bank_list_c); cell != nullptr; cell = cell->pNext)
    {
        rec = (SND_BANK_REC *)SingleLinkListCellBodyPtr(cell);
        if (rec->buf != -1)
            SndBufPause(rec->buf);
    }
}

// FF3 0x0017d5e8.
void SndBankListRestartAll(void)
{
    SLL_CELL      *cell;
    SND_BANK_REC  *rec;

    for (cell = SingleLinkListBeginCell(&snd_bank_list_c); cell != nullptr; cell = cell->pNext)
    {
        rec = (SND_BANK_REC *)SingleLinkListCellBodyPtr(cell);
        if (rec->buf != -1)
            SndBufRestart(rec->buf);
    }
}

// FF3 0x0017d658.
int SndBankListIsPlayReq(int id)
{
    SLL_CELL *cell;

    for (cell = SingleLinkListBeginCell(&snd_bank_list_c); cell != nullptr; cell = cell->pNext)
    {
        if (((SND_BANK_REC *)SingleLinkListCellBodyPtr(cell))->id == id)
            return 1;
    }
    return 0;
}

// The waiting plays start once their bank is ready (a placed one as a 3D
// source at the record's copy of the position); ended plays are dropped.
// FF3 0x0017d6e8.
void SndBankListPlayStep(void)
{
    SLL_CELL      *cell;
    SLL_CELL      *next;
    SND_BANK_REC  *rec;
    SND_3D_SET     set;
    SND_3D_SET    *s3s;
    int            bank;
    int            loop;

    cell = SingleLinkListBeginCell(&snd_bank_list_c);
    if (cell == nullptr)
        return;
    next = cell->pNext;
    while (true)
    {
        rec = (SND_BANK_REC *)SingleLinkListCellBodyPtr(cell);
        if (rec->buf == -1)
        {
            bank = SndBankListGetBank(rec->id);
            if (bank != -1 && SndBankListIsReady(rec->id) != 0)
            {
                memset(&set, 0, sizeof(set));
                loop = SndBankIsLoopSnd(bank, rec->no) != 0;
                s3s = nullptr;
                if (rec->has_pos != 0)
                {
                    set.pos = rec->pos;
                    s3s = &set;
                }
                rec->buf = SndBankPlay(bank, rec->no, rec->effect, loop, 0x3200, 0x1000, 0, s3s);
            }
        }
        else if (SndBufIsPlaying(rec->buf) == 0)
        {
            SingleLinkListRemove(&snd_bank_list_c, cell);
        }
        if (next == nullptr)
            break;
        cell = next;
        next = next->pNext;
    }
}

// ──────────────────────────────────────────────────────────────────────
// The map particles
// ──────────────────────────────────────────────────────────────────────

// FF3 0x0017de30.
// PC: the cells hold the host record (0x70 on the EE): with the EE size the
// list copied only the front of it and dropped Type .. No.
void EffMapPartListInit(void)
{
    SingleLinkListInit(&eff_map_part_list, (int)sizeof(EFF_MAP_PART));
}

// FF3 0x0017d830.
void EffMapPartInit(void)
{
    EffMapPartListInit();
    memset(&eff_menu_bg, 0, sizeof(eff_menu_bg));
}

// FF3 0x0017ed50.
void EffMapPartDrawType(float rate, int type)
{
    SLL_CELL     *cell;
    SLL_CELL     *next;
    EFF_MAP_PART *mp;

    cell = SingleLinkListBeginCell(&eff_map_part_list);
    while (cell != nullptr)
    {
        next = cell->pNext;
        mp = (EFF_MAP_PART *)SingleLinkListCellBodyPtr(cell);
        if (mp->Type == type && 0.0f < rate && rate <= 1.0f)
            eff_map_part_draw[type](rate, mp);
        cell = next;
    }
}

// FF3 0x0017d860.
void EffMapPartInPhoto(void)
{
    EffMapPartDrawType(1.0f, 0);
    EffMapPartDrawType(1.0f, 1);
}

// FF3 0x0017d8a0.
void EffMapPartRelease(void)
{
    EffMapPartCut(-1, -1);
}

// FF3 0x0017d8c0.
void EffMapPart0Req(float *pos, int no, int area)
{
    EffMapPartReq(0, pos, no, area);
}

// FF3 0x0017d8e8.
void EffMapPart0Cut(int no)
{
    EffMapPartCut(0, no);
}

// FF3 0x0017d930.
void EffMapPart1Req(float *pos, int no, int area)
{
    EffMapPartReq(1, pos, no, area);
}

// FF3 0x0017d958.
void EffMapPart1Cut(int no)
{
    EffMapPartCut(1, no);
}

/// ──────────────────────────────────────────────────────────────────────
// The menu backdrop (menu_file.c): snow and three fog layers
// ──────────────────────────────────────────────────────────────────────

// FF3 0x0017d978: map particle type 2 at the origin, the game camera saved
// for EffMenuBgEnd, fresh UV phases for the fog layers.
// callcheck: rand (ReiPan_Rand)
void EffMenuBgStart(int no)
{
    static sceVu0FVECTOR origin = { 0.0f, 0.0f, 0.0f, 1.0f };     // data 0x329500
    int i;

    EffMapPartReq(2, origin, no, -1);
    eff_menu_bg.cnt++;
    memcpy(eff_menu_bg.pos, gra3dcamGetPosition(), sizeof(sceVu0FVECTOR));
    memcpy(eff_menu_bg.target, gra3dcamGetTarget(), sizeof(sceVu0FVECTOR));
    eff_menu_bg.roll = gra3dcamGetRoll();
    eff_menu_bg.fov  = gra3dcamGetFov();
    for (i = 0; i < 3; i++)
    {
        eff_menu_bg.u[i] = (float)ReiPan_Rand() / REIPAN_RAND_MAXF;
        eff_menu_bg.v[i] = (float)ReiPan_Rand() / REIPAN_RAND_MAXF;
    }
}

// FF3 0x0017da60.
void EffMenuBgEnd(int no)
{
    EffMapPartCut(2, no);
    gra3dcamSetPosition(eff_menu_bg.pos);
    gra3dcamSetTarget(eff_menu_bg.target, 1);
    gra3dcamSetRoll(eff_menu_bg.roll);
    gra3dcamSetFov(eff_menu_bg.fov);
    eff_menu_bg.cnt--;
    if (eff_menu_bg.cnt < 0)
        eff_menu_bg.cnt = 0;
}

// One tile of `sp` (w x h pixels at x, y about the screen centre), cut into
// divw x divh cells drawn as 4-vertex flat strips: UV scrolled by (u, v)
// tiles and stretched `scale` times, alpha falling from `alpha` at (0, -249)
// to 0 at 497 pixels (above the top edge, so the fog thins downwards).
// Nothing when alpha is 0. FF3 0x002b67d0.
// PC: the chain is not walked; each cell also goes to the host as a quad
// under the sprite's REPEAT. The GS shades a flat strip's two triangles
// with the alpha of vertices 2 and 3; the host quad takes vertex 3's.
static void EffFogTileDraw(float u, float v, const SPRT_DAT *sp, int x, int y, int scale,
                           int r, int g, int b, int alpha, int divw, int divh)
{
    struct FOG_VTX                      /* 0x30 */
    {
        int xyz[4];                     // x, y, z, (the ADC is added on the way out)
        int rgba[4];
        int st[4];                      // UV texels x 16, 0, 0
    };
    static_assert(sizeof(FOG_VTX) == 0x30, "FOG_VTX: the ROM's stack records are 0x30");
    static_assert(offsetof(FOG_VTX, st) == 0x20, "FOG_VTX.st");
    FOG_VTX vtx[4];
    float   s[4], t[4];
    float   u0, du, dv;
    u_int   w, h;
    u_long *p;
    int     ix, iy, k;

    if (alpha == 0 || divw <= 0 || divh <= 0)
        return;

    w  = sp->w;
    h  = sp->h;
    u0 = (float)w * u;
    du = (float)(int)(w * (u_int)scale) / (float)divw;
    dv = (float)(int)(h * (u_int)scale) / (float)divh;

    p = (u_long *)StartDmaDirectTrans();
    p[1] = 0xe;                         // A+D
    p[0] = 0x1000000000000002ULL;
    p[4] = sp->tex0;
    p[2] = 0;
    p[3] = 0x3f;                        // TEXFLUSH
    p[5] = 6;                           // TEX0_1
    p += 6;

    for (k = 0; k < 4; k++)
    {
        vtx[k].rgba[0] = r;
        vtx[k].rgba[1] = g;
        vtx[k].rgba[2] = b;
        vtx[k].rgba[3] = alpha;
    }

    for (iy = 0; iy < divh; iy++)
    {
        for (ix = 0; ix < divw; ix++)
        {
            for (k = 0; k < 4; k++)
            {
                t[k] = (float)h * v + (float)(iy + k / 2) * dv;
                s[k] = u0 + (float)(ix + k % 2) * du;
            }
            for (k = 0; k < 4; k++)
            {
                int   vx = x + (int)((ix + k % 2) * w) / divw;
                int   vy = y + (int)((iy + k / 2) * h) / divh;
                float fy = (float)vy - -249.0f;
                float d2 = fy * fy + (float)vx * (float)vx;

                vtx[k].xyz[0]  = vx;
                vtx[k].xyz[1]  = vy;
                vtx[k].xyz[2]  = 0xffff;
                vtx[k].st[0]   = (int)(s[k] * 16.0f);   // VFTOI4
                vtx[k].st[1]   = (int)(t[k] * 16.0f);
                vtx[k].rgba[3] = (d2 < 247009.0f) ? alpha - (int)((float)alpha * sqrtf(d2 / 247009.0f)) : 0;
                vtx[k].xyz[0]  = (vtx[k].xyz[0] + 0x800) * 0x10;
                vtx[k].xyz[1]  = (vtx[k].xyz[1] + 0x800) * 0x10;
            }

            p[0] = 0x30aa400000008004ULL;   // NLOOP 4, EOP, PRIM 0x154: flat TRISTRIP, TME, ABE, FST
            p[1] = 0x431;                   // RGBAQ, UV, XYZ2
            p += 2;
            for (k = 0; k < 4; k++)
            {
                u_int *q = (u_int *)p;

                q[0]  = (u_int)vtx[k].rgba[0];
                q[1]  = (u_int)vtx[k].rgba[1];
                q[2]  = (u_int)vtx[k].rgba[2];
                q[3]  = (u_int)vtx[k].rgba[3];
                q[4]  = (u_int)vtx[k].st[0];
                q[5]  = (u_int)vtx[k].st[1];
                q[6]  = 0;
                q[7]  = 0;
                q[8]  = (u_int)vtx[k].xyz[0];
                q[9]  = (u_int)vtx[k].xyz[1];
                q[10] = (u_int)vtx[k].xyz[2];
                q[11] = (k < 2) ? 0x8000 : 0;
                p += 6;
            }

            {
                static const u_long tex1 = 0x161;           // EffMenuBgFogDraw's TEX1 / CLAMP
                float xy[8], uv[8];

                for (k = 0; k < 4; k++)
                {
                    xy[k * 2 + 0] = (float)vtx[k].xyz[0] / 16.0f - 1728.0f;
                    xy[k * 2 + 1] = (float)vtx[k].xyz[1] / 16.0f - 1824.0f;
                    uv[k * 2 + 0] = (float)vtx[k].st[0] / 16.0f;
                    uv[k * 2 + 1] = (float)vtx[k].st[1] / 16.0f;
                }
                ReiPan_RendererSetGs2dDepth(0);
                ReiPan_RendererDrawTexturedQuadClamp((const sceGsTex0 *)&sp->tex0, (const sceGsTex1 *)&tex1,
                                                     xy, uv, 0, (u_char)r, (u_char)g, (u_char)b,
                                                     (u_char)vtx[3].rgba[3]);
            }
        }
    }
    EndDmaDirectTrans((qword *)p);
}

// The fog layers' texture (data 0x3522c0): 256 x 256.
static const SPRT_DAT menu_bg_fog_sprt = { 0x2007cde621413db1ULL, 0, 0, 0x100, 0x100, 0, 0, 0, 0x80, 0, 0 };

// Three layers, each stepping its UV phase (wrapped to 0..1) and tiling
// 640 x 448 with 256-pixel tiles of 4 x 4 cells at alpha[i] * rate, (0x78,
// 0x78, 0xa0). FF3 0x0017db58 (the tables rodata 0x354a70..0x354aac).
// PC: the tiles cover the window (the view bounds), on the ROM's 256-pixel
// grid: from x 0 they reached 768 on the right and nothing on the left, so
// a wide window's left side had no fog. 0..640 x 0..448 on a 640:448 window.
static void EffMenuBgFogDraw(float rate)
{
    static const float du[3]    = { 0.0008f, -0.00209999f, 0.0016999999f };
    static const float dv[3]    = { -0.0091999993f, -0.0121999895f, -0.0071999999f };
    static const int   scale[3] = { 3, 3, 2 };
    static const float alpha[3] = { 75.0f, 115.0f, 123.0f };
    static const GS_REG_SET regs[5] = {
        { 0x42ULL,         0x42 },      // ALPHA_1
        { 0x161ULL,        0x14 },      // TEX1_1
        { 0,               0x08 },      // CLAMP_1: REPEAT
        { 0x3000bULL,      0x47 },      // TEST_1
        { 0x10a000118ULL,  0x4e },      // ZBUF_1
    };
    const SPRT_DAT *sp = &menu_bg_fog_sprt;
    int i, x, y;
    float vx0, vy0, vx1, vy1;
    int   xs, ys, xe, ye;

    ReiPan_RendererGetViewBounds(&vx0, &vy0, &vx1, &vy1);  // PC
    xs = (int)floorf(vx0 / (float)sp->w) * (int)sp->w;
    ys = (int)floorf(vy0 / (float)sp->h) * (int)sp->h;
    xe = (int)ceilf(vx1);
    ye = (int)ceilf(vy1);
    SetGsRegsCached(regs, 5, 1);
    for (i = 0; i < 3; i++)
    {
        int a;

        eff_menu_bg.u[i] += du[i];
        if (1.0f < eff_menu_bg.u[i])
            eff_menu_bg.u[i] -= 1.0f;
        else if (eff_menu_bg.u[i] < 0.0f)
            eff_menu_bg.u[i] += 1.0f;
        eff_menu_bg.v[i] += dv[i];
        if (1.0f < eff_menu_bg.v[i])
            eff_menu_bg.v[i] -= 1.0f;
        else if (eff_menu_bg.v[i] < 0.0f)
            eff_menu_bg.v[i] += 1.0f;

        a = (int)(alpha[i] * rate);
        for (y = ys; y < ye; y += sp->h)            // PC: 0..0x1c0 in the ROM
        {
            for (x = xs; x < xe; x += sp->w)        // PC: 0..0x280 in the ROM
                EffFogTileDraw(eff_menu_bg.u[i], eff_menu_bg.v[i], sp, x - 320, y - 224, scale[i],
                               0x78, 0x78, 0xa0, a, 4, 4);
        }
    }
}

// FF3 0x0017dac8: under a fixed camera at (0, -36, 210) looking at the
// origin, fov 0.64, the fog layers, then the type-2 particles.
void EffMenuBgDraw(float rate)
{
    gra3dcamSetPosition(0.0f, -36.0f, 210.0f);
    gra3dcamSetTarget(0.0f, 0.0f, 0.0f, 1);
    gra3dcamSetFov(0.64f);
    gra3dApplyCamera(nullptr, 1, 1);
    EffMenuBgFogDraw(rate);
    EffMapPartDrawType(rate, 2);
}

// FF3 0x0017eb70. The particle count comes from the area's record (type
// 0: its word +0x24, type 1: +0xb4) or is 1200 (type 2). FF3 answers 1
// also when the list is full (the record is freed then).
// callcheck: EFFECT_FREE (read as a macro)
int EffMapPartReq(int type, float *pos, int no, int area)
{
    EFF_MAP_PART mp;
    int          num;

    EffObjBufTblSelect(area);
    num = 0;
    if (type == 1)
        num = (int)EffObjBufTblGet()->Prm[(0xb4 - 4) / 4];
    else if (type == 0)
        num = (int)EffObjBufTblGet()->Prm[(0x24 - 4) / 4];
    else if (type == 2)
        num = 0x4b0;

    if (EffMapPartCreate(&mp, pos, num, type, area) == 0)
        return 0;
    mp.No = no;
    if (SingleLinkListAddEnd(&eff_map_part_list, &mp) == nullptr)
    {
        if (mp.pPart != nullptr)
            EFFECT_FREE(mp.pPart);
        memset(&mp, 0, sizeof(mp));
    }
    return 1;
}

// FF3 0x0017ec70.
// callcheck: EFFECT_FREE (read as a macro)
void EffMapPartCut(int type, int no)
{
    SLL_CELL     *cell;
    SLL_CELL     *next;
    EFF_MAP_PART *mp;

    cell = SingleLinkListBeginCell(&eff_map_part_list);
    while (cell != nullptr)
    {
        next = cell->pNext;
        mp = (EFF_MAP_PART *)SingleLinkListCellBodyPtr(cell);
        if (no < 0 || (mp->Type == (short)type && mp->No == no))
        {
            if (mp->pPart != nullptr)
                EFFECT_FREE(mp->pPart);
            memset(mp, 0, sizeof(*mp));
            SingleLinkListRemove(&eff_map_part_list, cell);
        }
        cell = next;
    }
}

// ---- the record and its particles -------------------------------------

// rand() / RAND_MAX as the ROM writes it (2147483520.0f, rdata 0x36f000..).
// callcheck: rand (PC: ReiPan_Rand, the ROM's 31-bit range)
static inline float EffMapPartRand01(void)
{
    return (float)ReiPan_Rand() / REIPAN_RAND_MAXF;
}

// vftoi4: x 16, truncated, saturated as the VU does (no host overflow on a
// particle at the camera's plane).
static inline int EffFtoi4(float f)
{
    f *= 16.0f;
    if (!(f < 2147483520.0f))
        return (f != f) ? 0 : 0x7fffffff;
    if (f < -2147483520.0f)
        return (int)0x80000000;
    return (int)f;
}

// A random offset inside the record's extent: ext * (r - 0.5) * 1.5, w 0.
static void EffMapPartRandOffset(sceVu0FVECTOR ofs, const EFF_MAP_PART *mp)
{
    int i;

    for (i = 0; i < 3; i++)
        ofs[i] = mp->Ext[i] * (EffMapPartRand01() - 0.5f) * 1.5f;   // rdata 0x36f000..0x36f008
    ofs[3] = 0.0f;
}

// Map effects 200 / 201's record (EffMapPartReq) and the type 2 one: num
// particles, the type's parameter block in the area's EffObjBufTbl record,
// the room's SCL boxes moved by the room's offset once, the type's setup,
// then every particle placed.
// FF3 0x0017de50.
// callcheck: EFFECT_MALLOC, EffObjBufTblGet (a macro; Get read once into rec)
int EffMapPartCreate(EFF_MAP_PART *mp, float *pos, int num, int type, int area)
{
    EFF_OBJ_BUF   *rec;
    EFF_MAP_SCL   *scl;
    sceVu0FVECTOR  ofs;
    int            i;

    if (num < 1)
        return 0;
    memset(mp, 0, sizeof(*mp));         // 0x70 on the EE
    mp->pPart = (EFF_MAP_PARTICLE *)EFFECT_MALLOC(num * (int)sizeof(EFF_MAP_PARTICLE));
    if (mp->pPart == nullptr)
        return 0;
    mp->Type = (short)type;
    mp->Area = area;
    EffObjBufTblSelect(area);
    rec = EffObjBufTblGet();
    mp->pPrm = nullptr;
    if (type == 0)
        mp->pPrm = (EFF_MAP_PART_PRM *)&rec->Prm[(0x10 - 4) / 4];
    else if (type == 1)
        mp->pPrm = (EFF_MAP_PART_PRM *)&rec->Prm[(0xa0 - 4) / 4];

    scl = (EFF_MAP_SCL *)rec->pBuf;
    mp->pScl = scl;
    if (scl != nullptr && scl->Moved == 0)
    {
        MapLoadGetOffsetVector(ofs, MapLoadGetBuffID(area));
        for (i = 0; i < scl->Num; i++)
        {
            scl->Box[i].Min[0] += ofs[0]; scl->Box[i].Min[1] += ofs[1]; scl->Box[i].Min[2] += ofs[2];
            scl->Box[i].Max[0] += ofs[0]; scl->Box[i].Max[1] += ofs[1]; scl->Box[i].Max[2] += ofs[2];
        }
        scl->Moved = 1;
    }

    mp->Pos[0] = pos[0];
    mp->Pos[1] = pos[1];
    mp->Pos[2] = pos[2];
    mp->Pos[3] = pos[3];
    mp->Num = (short)num;
    mp->FloorOfs = 0.0f;
    EffMapPartFloorLevelUpdate(mp);
    eff_map_part_setup[mp->Type](mp);
    for (i = 0; i < num; i++)
        memset(&mp->pPart[i], 0, sizeof(EFF_MAP_PARTICLE));
    mp->Ready = 0;
    for (i = 0; i < mp->Num; i++)
        EffMapPartParticleSet(&mp->pPart[i], mp);
    mp->Ready = 1;
    return 1;
}

// A new start for a particle whose start fell in an exclusion box: the
// type's position callback, then the floor level and w 1.
// FF3 0x0017e068 (Ghidra's ManyCandleCtrlInit was a wrong FF2 match).
// callcheck: rand (EffMapPartRandOffset: ReiPan_Rand)
static void EffMapPartParticleRePos(EFF_MAP_PARTICLE *part, EFF_MAP_PART *mp)
{
    sceVu0FVECTOR ofs;

    EffMapPartRandOffset(ofs, mp);
    eff_map_part_set_pos[mp->Type](part, mp, ofs);
    part->Pos[3] = 1.0f;                // rdata 0x36f00c
    part->Pos[1] += mp->FloorOfs;
}

// A particle from scratch: cleared, the type's particle init at a random
// offset, alive, the floor level and w 1.
// FF3 0x0017e180.
// callcheck: rand (EffMapPartRandOffset: ReiPan_Rand)
static void EffMapPartParticleInit(EFF_MAP_PARTICLE *part, EFF_MAP_PART *mp)
{
    sceVu0FVECTOR ofs;

    memset(part, 0, sizeof(*part));
    EffMapPartRandOffset(ofs, mp);      // rdata 0x36f010..0x36f018
    eff_map_part_init[mp->Type](part, mp, ofs);
    part->Alive = 1;
    part->Pos[1] += mp->FloorOfs;
    part->Pos[3] = 1.0f;                // rdata 0x36f01c
}

// EffMapPartParticleInit, then up to 8 new starts while the start lies in
// an exclusion box.
// FF3 0x0017e2b0.
static void EffMapPartParticleSet(EFF_MAP_PARTICLE *part, EFF_MAP_PART *mp)
{
    int n = 0;

    EffMapPartParticleInit(part, mp);
    while (EffMapPartSclCheck(part->Pos, mp->pScl) != 0)
    {
        n++;
        EffMapPartParticleRePos(part, mp);
        if (7 < n)
            break;
    }
}

// Point pos against the room's boxes: 0 inside one of the first KeepNum,
// 1 inside one of the others, 0 outside all or before the boxes are moved.
// FF3 0x002b6678.
static int EffMapPartSclCheck(const float *pos, const EFF_MAP_SCL *scl)
{
    int i;

    if (scl == nullptr || scl->Moved == 0)
        return 0;
    for (i = 0; i < scl->KeepNum; i++)
    {
        const EFF_MAP_SCL_BOX *b = &scl->Box[i];

        if (b->Min[0] <= pos[0] && pos[0] <= b->Max[0] &&
            b->Min[1] <= pos[1] && pos[1] <= b->Max[1] &&
            b->Min[2] <= pos[2] && !(b->Max[2] < pos[2]))
            return 0;
    }
    for (; i < scl->Num; i++)
    {
        const EFF_MAP_SCL_BOX *b = &scl->Box[i];

        if (b->Min[0] <= pos[0] && pos[0] <= b->Max[0] &&
            b->Min[1] <= pos[1] && pos[1] <= b->Max[1] &&
            b->Min[2] <= pos[2] && !(b->Max[2] < pos[2]))
            return 1;
    }
    return 0;
}

// Type 1 in area 6: the camera's height picks a level in steps of -80 (the
// band [i * -80 - 160, i * -80 - 80) for i 1..9, else 0); 1 when it moved.
// FF3 0x002b6c10.
static int EffMapPartFloorLevelUpdate(EFF_MAP_PART *mp)
{
    GRA3DCAMERA *cam;
    float        y;
    float        level;
    int          i;

    if (mp->Type != 1 || mp->Area != 6)
        return 0;
    cam = gra3dGetCamera();
    y = cam->matCoord[3][1];
    level = 0.0f;
    for (i = 1; i < 10; i++)
    {
        level = (float)i * -80.0f;      // rdata 0x36f0ec
        if (level + -80.0f + -80.0f <= y && y < level + -80.0f)
            break;
        level = 0.0f;
    }
    if (mp->FloorOfs == level)
        return 0;
    mp->FloorOfs = level;
    return 1;
}

// One step of a live particle: a random kick with chance KickRate, the
// damping on acceleration and velocity, the rise, the move (xyz), the
// shrink, the life and fade-in counters. 0 when it dies (life over, or it
// entered an exclusion box).
// FF3 0x002b6cc8.
// callcheck: rand (EffMapPartRand01: ReiPan_Rand)
static int EffMapPartParticleStep(EFF_MAP_PARTICLE *part, EFF_MAP_PART *mp)
{
    int i;

    if (part->Alive == 0)
        return 1;
    if (part->Move != 0)
    {
        if (0.0f < mp->KickRate && EffMapPartRand01() < mp->KickRate)     // rdata 0x36f0f0
        {
            part->Acc[0] += (EffMapPartRand01() - 0.5f) * mp->Kick[0];    // rdata 0x36f0f4
            part->Acc[1] += (EffMapPartRand01() - 0.5f) * mp->Kick[1];
            part->Acc[2] += (EffMapPartRand01() - 0.5f) * mp->Kick[2];
        }
        for (i = 0; i < 3; i++)
            part->Acc[i] *= mp->Damp;
        for (i = 0; i < 3; i++)
            part->Vel[i] += part->Acc[i];
        for (i = 0; i < 3; i++)
            part->Vel[i] *= mp->Damp;
        part->Vel[1] += part->Rise;
        for (i = 0; i < 3; i++)
            part->Pos[i] += part->Vel[i];
    }
    part->SizeW *= part->Shrink;
    part->SizeH *= part->Shrink;
    if (part->Timed != 0)
    {
        part->Cnt++;
        if (part->CntMax <= part->Cnt)
            return 0;
    }
    if (0 < part->FadeIn)
        part->FadeIn--;
    if (EffMapPartSclCheck(part->Pos, mp->pScl) != 0)
        return 0;
    return 1;
}

// PC: the host has no walker for this DIRECT chain, so each visible sprite
// also goes to the 3D effect path as a camera-facing quad of the same
// screen size at the particle's depth: the view's x / y axes in world space
// scaled so that matWorldScreen maps them onto the ROM's half sizes.
static void EffMapPartHostSprite(GRA3DCAMERA *cam, const EFF_MAP_PARTICLE *part, float w,
                                 float hw, float hh, const float *st, int r, int g, int b,
                                 u_long tex0)
{
    sceVu0FVECTOR corner[4];
    float right[3], up[3];
    float px, py;
    int   i;

    px = 0.0f;
    py = 0.0f;
    for (i = 0; i < 3; i++)
    {
        right[i] = cam->matWorldView[i][0];
        up[i]    = cam->matWorldView[i][1];
        px += cam->matWorldScreen[i][0] * right[i];
        py += cam->matWorldScreen[i][1] * up[i];
    }
    if (px == 0.0f || py == 0.0f)
        return;
    hw = hw * w / px;
    hh = hh * w / py;
    for (i = 0; i < 4; i++)
    {
        float sx = (i & 1) ? hw : -hw;
        float sy = (i & 2) ? hh : -hh;
        int   k;

        for (k = 0; k < 3; k++)
            corner[i][k] = part->Pos[k] + right[k] * sx + up[k] * sy;
        corner[i][3] = 1.0f;
    }
    ReiPan_Graph3dDrawEffectQuad(corner, 4, st, r, g, b, part->DrawAlpha, tex0, 0);
}

// The step and draw of a record (types 0..2, table 0x2f9a80): every
// particle re-placed when the floor level moved; unless effects are frozen,
// each live one stepped (a dead one starts over, one past the floor runs the
// type's land callback); then projected (matWorldScreen, 512 / fov / w),
// dropped outside the guard band, its alpha scaled by rate, life, fade-in,
// depth and screen radius (FarFade) or nearness (NearFade); the visible ones
// go out as one GS sprite each, texture effdat[first particle's Tex], the
// first particle's colour (grey in monochrome).
// FF3 0x0017e328.
// callcheck: StartDmaDirectTrans, EndDmaDirectTrans (the packet, inline)
void EffMapPartDraw(float rate, EFF_MAP_PART *mp)
{
    static sceVu0FMATRIX unit = {       // data 0x3293c0
        { 1.0f, 0.0f, 0.0f, 0.0f },
        { 0.0f, 1.0f, 0.0f, 0.0f },
        { 0.0f, 0.0f, 1.0f, 0.0f },
        { 0.0f, 0.0f, 0.0f, 1.0f },
    };
    GRA3DCAMERA      *cam;
    sceVu0FMATRIX     m;
    sceVu0FVECTOR     v;
    EFF_MAP_PARTICLE *part;
    GS_REG_SET        regs[5];
    u_long           *p;
    u_int            *q;
    float             floor;
    float             scale;
    float             iw, hw, hh;
    int               vis;
    int               i, k;

    cam = gra3dGetCamera();
    sceVu0MulMatrix(m, cam->matWorldScreen, unit);
    if (EffMapPartFloorLevelUpdate(mp) != 0)
    {
        mp->Ready = 0;
        for (i = 0; i < mp->Num; i++)
            EffMapPartParticleSet(&mp->pPart[i], mp);
        mp->Ready = 1;
    }

    if (eff_wrk.stop_flg == 0)
    {
        floor = mp->Floor + mp->FloorOfs;
        for (i = 0; i < mp->Num; i++)
        {
            part = &mp->pPart[i];
            if (part->Alive == 0)
                continue;
            if (EffMapPartParticleStep(part, mp) == 0)
                EffMapPartParticleInit(part, mp);
            else if (part->Timed == 0 && eff_map_part_land[mp->Type] != nullptr &&
                     floor < part->Pos[1])
                eff_map_part_land[mp->Type](part, mp);
        }
    }

    vis = 0;
    scale = 512.0f / cam->fFov;         // rdata 0x36f028
    for (i = 0; i < mp->Num; i++)
    {
        part = &mp->pPart[i];
        sceVu0ApplyMatrix(v, m, part->Pos);
        iw = 1.0f / v[3];               // rdata 0x36f034
        hw = scale * part->SizeW * iw;
        hh = scale * part->SizeH * iw;
        v[0] *= iw;
        v[1] *= iw;
        v[2] *= iw;
        part->Scr[0][0] = EffFtoi4(v[0] - hw);
        part->Scr[0][1] = EffFtoi4(v[1] - hh);
        part->Scr[0][2] = EffFtoi4(v[2]);
        part->Scr[0][3] = EffFtoi4(v[3]);
        part->Scr[1][0] = EffFtoi4(v[0] + hw);
        part->Scr[1][1] = EffFtoi4(v[1] + hh);
        part->Scr[1][2] = EffFtoi4(v[2]);
        part->Scr[1][3] = EffFtoi4(v[3]);
        // The guard band: x / y 0x4000..0xc000, z 1..0xffffff, both corners.
        for (k = 0; k < 2; k++)
        {
            if ((u_int)(part->Scr[k][0] - 0x4000) > 0x8000 ||
                (u_int)(part->Scr[k][1] - 0x4000) > 0x8000 ||
                (u_int)(part->Scr[k][2] - 1) > 0xfffffe)
                break;
        }
        if (k < 2)
        {
            part->Visible = 0;
            continue;
        }
        part->Visible = 1;
        part->DrawAlpha = (short)(int)((float)part->Alpha * rate);
        if (part->Timed != 0 && part->CntMax != 0)
            part->DrawAlpha = (short)((part->DrawAlpha * (part->CntMax - part->Cnt)) / part->CntMax);
        if (0 < part->FadeIn)
        {
            int fade = part->FadeIn;
            int a    = part->DrawAlpha;
            int n1   = (GetPALMode() != 0) ? 100 : 120;
            int n2   = (GetPALMode() != 0) ? 100 : 120;

            part->DrawAlpha = (short)((a * (n1 - fade)) / n2);
        }
        if (part->FarFade != 0)
        {
            int   w4 = part->Scr[0][3];
            float dx, dy, d2;

            if (0 <= w4)
            {
                if (w4 < 0x3a0d)
                    part->DrawAlpha = (short)(part->DrawAlpha - (part->DrawAlpha * w4) / 0x3a0c);
                else
                    part->DrawAlpha = 0;
            }
            dx = (float)part->Scr[0][0] * 0.0625f - 2048.0f;               // rdata 0x36f02c / 0x36f030
            dy = ((float)part->Scr[0][1] * 0.0625f - 2048.0f) - -249.0f;   // rdata 0x36f03c
            d2 = dx * dx + dy * dy;
            if (d2 < 247009.0f)                                             // rdata 0x36f038: 497^2
                part->DrawAlpha = (short)(part->DrawAlpha -
                    ((int)((float)part->DrawAlpha * sqrtf(d2 / 247009.0f)) & 0xff));
            else
                part->DrawAlpha = 0;
            if (part->DrawAlpha == 0)
            {
                part->Visible = 0;
                continue;
            }
        }
        if (part->NearFade != 0)
        {
            int w4 = part->Scr[0][3];

            if (w4 < 600)
                part->DrawAlpha = (short)((part->DrawAlpha * w4) / 600);
            if (part->DrawAlpha == 0)
            {
                part->Visible = 0;
                continue;
            }
        }
        vis++;
    }
    if (vis < 1)
        return;

    regs[0].data = mp->Alpha;       regs[0].reg = 0x42;     // ALPHA_1
    regs[1].data = 0x161;           regs[1].reg = 0x14;     // TEX1_1
    regs[2].data = 0;               regs[2].reg = 0x08;     // CLAMP_1
    regs[3].data = mp->Test;        regs[3].reg = 0x47;     // TEST_1
    regs[4].data = 0x10a000118ULL;  regs[4].reg = 0x4e;     // ZBUF_1, no z writes
    SetGsRegsCached(regs, 5, 1);

    {
        const SPRT_DAT *sd = &effdat[mp->pPart[0].Tex];
        u_int  u1 = (u_int)sd->w << 4;
        u_int  v1 = (u_int)sd->h << 4;
        int    r  = mp->pPart[0].R;
        int    g  = mp->pPart[0].G;
        int    b  = mp->pPart[0].B;
        float  st[4][2];

        p = (u_long *)StartDmaDirectTrans();
        p[0] = 0x1000000000000002ULL;   // GIF tag: 2 A+D registers
        p[1] = 0xe;
        p[2] = 0;
        p[3] = 0x3f;                    // TEXFLUSH
        p[4] = sd->tex0;
        p[5] = 6;                       // TEX0_1
        q = (u_int *)(p + 6);
        if (eff_wrk.monochro_mode != 0)
        {
            r = (r + g + b) / 3;
            g = r;
            b = r;
        }

        // PC: the sprite's texture corners for the host quad.
        st[0][0] = 0.0f;
        st[0][1] = 0.0f;
        st[1][0] = (float)sd->w / (float)(1 << ((sd->tex0 >> 26) & 0xf));
        st[1][1] = 0.0f;
        st[2][0] = 0.0f;
        st[2][1] = (float)sd->h / (float)(1 << ((sd->tex0 >> 30) & 0xf));
        st[3][0] = st[1][0];
        st[3][1] = st[2][1];

        for (i = 0; i < mp->Num; i++)
        {
            part = &mp->pPart[i];
            if (part->Visible == 0)
                continue;
            // GIF tag (rdata 0x36f020): 1 loop, EOP, PRIM 0x156 (sprite,
            // textured, blended, UV), RGBAQ / UV / XYZF2 / UV / XYZF2.
            ((u_long *)q)[0] = 0x50ab400000008001ULL;
            ((u_long *)q)[1] = 0x43431;
            q[4] = (u_int)r;
            q[5] = (u_int)g;
            q[6] = (u_int)b;
            q[7] = (u_int)(int)part->DrawAlpha;
            q += 8;
            for (k = 0; k < 2; k++)
            {
                q[0] = (k == 0) ? 0 : u1;
                q[1] = (k == 0) ? 0 : v1;
                q[2] = 0;
                q[3] = 0;
                q[4] = (u_int)part->Scr[k][0];
                q[5] = (u_int)part->Scr[k][1];
                q[6] = (u_int)part->Scr[k][2];
                q[7] = (k == 0) ? 0x8000 : 0;
                q += 8;
            }

            // PC
            iw = (float)part->Scr[0][3] * 0.0625f;
            hw = (float)(part->Scr[1][0] - part->Scr[0][0]) * 0.03125f;
            hh = (float)(part->Scr[1][1] - part->Scr[0][1]) * 0.03125f;
            EffMapPartHostSprite(cam, part, iw, hw, hh, &st[0][0], r, g, b, sd->tex0);
        }
        EndDmaDirectTrans((qword *)q);
    }
}

// ---- the per-type callbacks (tables 0x2f9a40..0x2f9a7c) ----------------

// Type 0 / 1's setup from the parameter block: extent, floor, kick size (x
// 0.01), kick chance and damping, normal alpha blending, z test GEQUAL.
static void EffMapPartPrmSetup(EFF_MAP_PART *mp)
{
    const EFF_MAP_PART_PRM *prm = mp->pPrm;
    int i;

    mp->Ext[0] = prm->Ext[0];
    mp->Ext[1] = prm->Ext[1];
    mp->Ext[3] = 0.0f;
    mp->Ext[2] = prm->Ext[2];
    mp->Floor = -prm->Floor;
    mp->Kick[0] = prm->Kick[0];
    mp->Kick[1] = prm->Kick[1];
    mp->Kick[3] = 0.0f;
    mp->Kick[2] = prm->Kick[2];
    for (i = 0; i < 4; i++)
        mp->Kick[i] *= 0.01f;           // 0x3c23d70a
    mp->KickRate = prm->KickRate;
    mp->Test = 0x5000b;
    mp->Damp = prm->Damp;
    mp->Alpha = 0x44;
}

// FF3 0x0017ee20 (table 0x2f9a40[0]).
static void EffMapPart0Setup(EFF_MAP_PART *mp)
{
    EffMapPartPrmSetup(mp);
}

// Type 0 / 1's start: the centre + ofs; before the record is ready, y
// anywhere between the floor and the centre.
// FF3 0x0017eeb0 (table 0x2f9a50[0]).
// callcheck: rand (EffMapPartRand01: ReiPan_Rand)
static void EffMapPart0SetPos(EFF_MAP_PARTICLE *part, EFF_MAP_PART *mp, const float *ofs)
{
    int i;

    for (i = 0; i < 4; i++)
        part->Pos[i] = mp->Pos[i] + ofs[i];
    if (mp->Ready == 0)
    {
        float floor = mp->Floor;
        float top   = mp->Pos[1];

        part->Pos[1] = (top - floor) * EffMapPartRand01() + mp->Floor;   // rdata 0x36f044
    }
}

// Type 0 / 1's random velocity, rise, size and colour from the block.
// callcheck: rand (EffMapPartRand01: ReiPan_Rand)
static void EffMapPartPrmMotion(EFF_MAP_PARTICLE *part, const EFF_MAP_PART_PRM *prm, float rise_scale)
{
    float s;

    part->Acc[0] = 0.0f;
    part->Acc[1] = 0.0f;
    part->Acc[2] = 0.0f;
    part->Vel[0] = EffMapPartRand01() * prm->VelRange[0] + prm->VelMin[0];
    part->Vel[1] = EffMapPartRand01() * prm->VelRange[1] + prm->VelMin[1];
    part->Vel[2] = EffMapPartRand01() * prm->VelRange[2] + prm->VelMin[2];
    part->Rise = (EffMapPartRand01() * prm->RiseRange + prm->RiseMin) * rise_scale;
    s = EffMapPartRand01() * prm->SizeRange + prm->SizeMin;
    part->SizeW = s;
    part->SizeH = s;
    part->Shrink = prm->Shrink;
    part->R = prm->R;
    part->G = prm->G;
    part->B = prm->B;
}

// Type 0's particle: EffMapPart0SetPos, the block's motion, alpha min +
// (r * range & 0xff), texture effdat[0x3c]; in area 10 a fade-in of 100
// (PAL) / 120 frames.
// FF3 0x0017ef48 (table 0x2f9a60[0]).
// callcheck: rand (EffMapPartRand01: ReiPan_Rand)
static void EffMapPart0ParticleInit(EFF_MAP_PARTICLE *part, EFF_MAP_PART *mp, const float *ofs)
{
    const EFF_MAP_PART_PRM *prm;
    float r;

    EffMapPart0SetPos(part, mp, ofs);
    prm = mp->pPrm;
    EffMapPartPrmMotion(part, prm, 0.1f);     // rdata 0x36f04c
    r = EffMapPartRand01();
    part->Cnt = 0;
    part->FadeIn = 0;
    part->Alpha = ((int)(r * (float)prm->AlphaRange) & 0xff) + prm->AlphaMin;
    if (mp->Area == 10)
        part->FadeIn = (short)((GetPALMode() != 0) ? 100 : 120);
    part->NearFade = 0;
    part->CntMax = 0;
    part->Timed = 0;
    part->FarFade = 0;
    part->Tex = 0x3c;
    part->Move = 1;
}

// Type 0 past the floor: held at the floor, stops and dies in 4 frames
// shrinking by 0.92.
// FF3 0x0017f140 (table 0x2f9a70[0]).
static void EffMapPart0Land(EFF_MAP_PARTICLE *part, EFF_MAP_PART *mp)
{
    float floor = mp->Floor + mp->FloorOfs;

    if (floor < part->Pos[1])
        part->Pos[1] = floor;
    part->Move = 0;
    part->Cnt = 0;
    part->Timed = 1;
    part->Shrink = 0.92f;               // rdata 0x36f050
    part->CntMax = 4;
}

// FF3 0x0017f188 (table 0x2f9a40[1]): EffMapPart0Setup's body again.
static void EffMapPart1Setup(EFF_MAP_PART *mp)
{
    EffMapPartPrmSetup(mp);
}

// FF3 0x0017f218 (table 0x2f9a50[1]): EffMapPart0SetPos's body again
// (Ghidra's ManyCandleParticleUpdate was a wrong FF2 match).
// callcheck: rand (EffMapPartRand01: ReiPan_Rand)
static void EffMapPart1SetPos(EFF_MAP_PARTICLE *part, EFF_MAP_PART *mp, const float *ofs)
{
    int i;

    for (i = 0; i < 4; i++)
        part->Pos[i] = mp->Pos[i] + ofs[i];
    if (mp->Ready == 0)
    {
        float floor = mp->Floor;
        float top   = mp->Pos[1];

        part->Pos[1] = (top - floor) * EffMapPartRand01() + mp->Floor;   // rdata 0x36f054
    }
}

// Type 1's particle: as type 0 with texture effdat[0x3e], the near fade
// and no fade-in; one less alpha base and range in the candle-out looks.
// FF3 0x0017f2b0 (table 0x2f9a60[1]).
// callcheck: rand (EffMapPartRand01: ReiPan_Rand)
static void EffMapPart1ParticleInit(EFF_MAP_PARTICLE *part, EFF_MAP_PART *mp, const float *ofs)
{
    const EFF_MAP_PART_PRM *prm;
    int base, range;
    float r;

    EffMapPart1SetPos(part, mp, ofs);
    prm = mp->pPrm;
    EffMapPartPrmMotion(part, prm, 0.1f);     // rdata 0x36f05c
    range = prm->AlphaRange;
    base = prm->AlphaMin;
    if (EffectNowScreenEffectIs5Or6() != 0)
    {
        base--;
        range--;
    }
    r = EffMapPartRand01();
    part->Tex = 0x3e;
    part->NearFade = 1;
    part->Cnt = 0;
    part->FadeIn = 0;
    part->CntMax = 0;
    part->Timed = 0;
    part->Move = 1;
    part->FarFade = 0;
    part->Alpha = ((int)(r * (float)range) & 0xff) + base;
}

// Type 1 past the floor: dies in 16 frames.
// FF3 0x0017f4a0 (table 0x2f9a70[1]).
static void EffMapPart1Land(EFF_MAP_PARTICLE *part, EFF_MAP_PART *mp)
{
    (void)mp;
    part->Cnt = 0;
    part->CntMax = 0x10;
    part->Timed = 1;
}

// Type 2's setup (no parameter block): extent (188, 100, 210), floor 43,
// kick (0.15, 0.14, 0.16) x 0.01, chance 0.7, damping 0.98, subtractive
// blending, no z test.
// FF3 0x0017f4b8 (table 0x2f9a40[2]).
static void EffMapPart2Setup(EFF_MAP_PART *mp)
{
    int i;

    mp->Ext[3] = 0.0f;
    mp->Kick[3] = 0.0f;
    mp->Ext[0] = 188.0f;                // rdata 0x36f060
    mp->Ext[1] = 100.0f;
    mp->Ext[2] = 210.0f;
    mp->Floor = 43.0f;
    mp->Kick[0] = 0.15f;                // rdata 0x36f070
    mp->Kick[1] = 0.14f;
    mp->Kick[2] = 0.16f;
    for (i = 0; i < 4; i++)
        mp->Kick[i] *= 0.01f;           // 0x3c23d70a
    mp->Test = 0x3000b;
    mp->KickRate = 0.7f;                // rdata 0x36f07c
    mp->Damp = 0.98f;                   // rdata 0x36f080
    mp->Alpha = 0x42;
}

// Type 2's start: the centre + ofs, y at the volume's top (centre y -
// extent y), anywhere between it and the floor before the record is ready.
// FF3 0x0017f588 (table 0x2f9a50[2]).
// callcheck: rand (EffMapPartRand01: ReiPan_Rand)
static void EffMapPart2SetPos(EFF_MAP_PARTICLE *part, EFF_MAP_PART *mp, const float *ofs)
{
    int i;

    for (i = 0; i < 4; i++)
        part->Pos[i] = mp->Pos[i] + ofs[i];
    if (mp->Ready == 0)
    {
        float ext   = mp->Ext[1];
        float top   = mp->Pos[1];
        float floor = mp->Floor;

        part->Pos[1] = ((top - ext) - floor) * EffMapPartRand01() + mp->Floor;   // rdata 0x36f084
    }
    else
        part->Pos[1] = mp->Pos[1] - mp->Ext[1];
}

// Type 2's particle: fixed ranges (in doubles, as the ROM computes them),
// colour (120, 120, 160), texture effdat[0x3c], the far fade.
// FF3 0x0017f638 (table 0x2f9a60[2]).
// callcheck: rand, fptodp, __muldf3, __adddf3, dptofp (EffMapPartRand01: ReiPan_Rand; the double expressions)
static void EffMapPart2ParticleInit(EFF_MAP_PARTICLE *part, EFF_MAP_PART *mp, const float *ofs)
{
    float s;

    EffMapPart2SetPos(part, mp, ofs);
    part->Acc[0] = 0.0f;
    part->Acc[1] = 0.0f;
    part->Acc[2] = 0.0f;
    part->Vel[0] = (float)((double)EffMapPartRand01() * 0.15 + 0.04);              // rdata 0x36f088 / 0x36f090
    part->Vel[1] = (float)((double)EffMapPartRand01() * 0.13 + 0.05);              // 0x36f098 / 0x36f0a0
    part->Vel[2] = (float)((double)EffMapPartRand01() * 0.08 + -0.04);             // 0x36f0a8 / 0x36f0b0
    part->Rise = (float)(((double)EffMapPartRand01() * 0.04 + 0.12) * (double)0.1f);   // 0x36f0b8 / 0x36f0c0
    s = (float)((double)EffMapPartRand01() * 0.48 + 0.799999);                     // 0x36f0c8 / 0x36f0d0
    part->SizeW = s;
    part->SizeH = s;
    part->G = 0x78;
    part->Shrink = 0.999999f;           // rdata 0x36f0dc
    part->B = 0xa0;
    part->R = 0x78;
    s = EffMapPartRand01();
    part->Tex = 0x3c;
    part->FarFade = 1;
    part->NearFade = 0;
    part->Cnt = 0;
    part->FadeIn = 0;
    part->CntMax = 0;
    part->Timed = 0;
    part->Move = 1;
    part->Alpha = ((int)(s * 12.0f) & 0xff) + 0x41;     // rdata 0x36f0e0
}

// Type 2 past the floor: dies in 4 frames.
// FF3 0x0017f8a0 (table 0x2f9a70[2]).
static void EffMapPart2Land(EFF_MAP_PARTICLE *part, EFF_MAP_PART *mp)
{
    (void)mp;
    part->Cnt = 0;
    part->CntMax = 4;
    part->Timed = 1;
}

// FF3 0x0017de28.
void FUN_0017de28(void)
{
}

// ──────────────────────────────────────────────────────────────────────
// The distortion particles (FF2 effect_oth.c)
// ──────────────────────────────────────────────────────────────────────

// The fan's texture coordinates (rdata 0x36f3b0.. and the VU0 immediates):
// the four corners inset by 1% of the page, then the centre.
static const float distortion_stq[5][4] = {
    { 0.01f, 0.01f, 1.0f, 0.0f },
    { 0.99f, 0.01f, 1.0f, 0.0f },
    { 0.99f, 0.99f, 1.0f, 0.0f },
    { 0.01f, 0.99f, 1.0f, 0.0f },
    { 0.5f,  0.5f,  1.0f, 0.0f },
};

// One textured fan per live particle (alpha > 0) whose position passes the
// clip test: the centre at the particle's colour and alpha, the four corners
// (half size psize * Scale / w) at alpha 0, fog 255 - w / 64. In monochrome
// the particle's colour is greyed for the draw and put back. The number of
// live particles, drawn or clipped.
// FF3 0x00185908; FF2 draw_distortion_particles2 (MioPan effect_oth.c:5717).
// FF3 differences: no Reserve2DPacket, and no guard band -- every particle
// inside the clip volume goes into the packet.
int draw_distortion_particles2(float (*matLocalScreen)[4], float (*matLocalClip)[4],
                               int t_particles, PARTICLE *pPartTop, float psize,
                               u_long tex0, u_long alpha)
{
    static const int cx[5] = { -1,  1,  1, -1, -1 };    // the fan: the corners, then the first again
    static const int cy[5] = { -1, -1,  1,  1, -1 };
    static const int cs[5] = {  0,  1,  2,  3,  0 };
    u_long   *p;
    int      *q;
    int       ndpkt;
    int       num;
    int       i, k;
    PARTICLE *part;
    float     rr, gg, bb;
    float     size;
    float     clip[4];
    float     screen[4];
    float     w;
    float     fog;
    float     halfw, halfh;

    p = (u_long *)StartDmaDirectTrans();
    p[4] = tex0;
    p[6] = alpha;
    p[2] = 0;
    p[0] = 0x1000000000008003ULL;       // A+D, 3 registers
    p[1] = 0xe;
    p[3] = 0x3f;                        // TEXFLUSH
    p[5] = 6;                           // TEX0_1
    p[7] = 0x42;                        // ALPHA_1
    ndpkt = 4;
    num = 0;

    for (i = 0; i < t_particles; i++)
    {
        part = &pPartTop[i];
        if (!(part->color[3] > 0.0f))
            continue;
        num++;
        rr = part->color[0];
        gg = part->color[1];
        bb = part->color[2];
        if (eff_wrk.monochro_mode != 0)
        {
            part->color[0] = (rr + gg + bb) / 3.0f;     // rdata 0x36f3d4
            part->color[2] = part->color[0];
            part->color[1] = part->color[0];
        }
        size = psize * part->Scale;

        // vclipw.xyz against the local -> clip matrix.
        sceVu0ApplyMatrix(clip, matLocalClip, part->position);
        if (clip[0] > clip[3] || clip[0] < -clip[3] || clip[1] > clip[3] || clip[1] < -clip[3]
            || clip[2] > clip[3] || clip[2] < -clip[3])
        {
            part->color[0] = rr;
            part->color[1] = gg;
            part->color[2] = bb;
            continue;
        }

        sceVu0ApplyMatrix(screen, matLocalScreen, part->position);
        w = screen[3];
        fog = 255.0f - w * 0.015625f;   // rdata 0x36f3d0 / 0x36f3bc
        if (fog < 0.0f)
            fog = 0.0f;
        screen[0] /= w;
        screen[1] /= w;
        screen[2] /= w;
        halfw = size / w;
        halfh = halfw;

        p[ndpkt * 2 + 0] = 0xe03ec00000008001ULL;   // GIF tag: TRIFAN, ABE, TME, FGE, 6 vertices
        p[ndpkt * 2 + 1] = 0x0042424242421421ULL;   // rdata 0x36f3b0: RGBAQ, STQ, XYZF2, (STQ, XYZF2) x 5
        q = (int *)(p + ndpkt * 2 + 2);
        q[0] = (int)part->color[0];     // vftoi0
        q[1] = (int)part->color[1];
        q[2] = (int)part->color[2];
        q[3] = (int)part->color[3];
        memcpy(q + 4, distortion_stq[4], 16);
        q[8]  = (int)(screen[0] * 16.0f);   // vftoi4
        q[9]  = (int)(screen[1] * 16.0f);
        q[10] = (int)(screen[2] * 16.0f);
        q[11] = (int)(fog * 16.0f);
        q[12] = (int)part->color[0];    // the corners' RGBAQ: alpha 0
        q[13] = (int)part->color[1];
        q[14] = (int)part->color[2];
        q[15] = 0;
        for (k = 0; k < 5; k++)
        {
            memcpy(q + 16 + k * 8, distortion_stq[cs[k]], 16);
            q[20 + k * 8] = (int)((screen[0] + (float)cx[k] * halfw) * 16.0f);
            q[21 + k * 8] = (int)((screen[1] + (float)cy[k] * halfh) * 16.0f);
            q[22 + k * 8] = (int)(screen[2] * 16.0f);
            q[23 + k * 8] = (int)(fog * 16.0f);
        }
        ndpkt += 15;

        // PC: the DIRECT packet draws nothing on the host; the same quad
        // goes to the renderer in screen space with the particle's depth
        // (clip[3] the view depth, clip z / w the fallback; MioPan's path).
        // The renderer takes one colour, so the centre-to-corner alpha fan
        // is lost; the particle textures carry their own falloff.
        {
            static const int ord[4] = { 0, 1, 3, 2 };   // the renderer's TL, TR, BL, BR
            u_long      tex1 = 0x161;
            const sceGsTex0 *tx0 = (const sceGsTex0 *)&tex0;
            float       tw = (float)(1 << tx0->TW);
            float       th = (float)(1 << tx0->TH);
            float       xy[8];
            float       uv[8];
            int         c;

            for (k = 0; k < 4; k++)
            {
                c = ord[k];
                xy[k * 2 + 0] = screen[0] + (float)cx[c] * halfw - 2048.0f + 320.0f;
                xy[k * 2 + 1] = screen[1] + (float)cy[c] * halfh - 2048.0f + 224.0f;
                uv[k * 2 + 0] = distortion_stq[c][0] * tw;
                uv[k * 2 + 1] = distortion_stq[c][1] * th;
            }
            // Laid out on screen around the particle, so tied to it for frame
            // smoothing's in-between cameras -- by where it is drawn, since
            // matLocalClip is a guard-band volume, not the one the renderer
            // projects with.
            ReiPan_RendererBeginScreenAnchor(screen[0] - 2048.0f + 320.0f,
                                             screen[1] - 2048.0f + 224.0f, clip[3]);
            ReiPan_RendererDrawTexturedQuadDepth(tx0, (const sceGsTex1 *)&tex1, xy, uv,
                                                 clip[2] / clip[3], clip[3],
                                                 (u_char)(int)part->color[0],
                                                 (u_char)(int)part->color[1],
                                                 (u_char)(int)part->color[2],
                                                 (u_char)(int)part->color[3]);
            ReiPan_RendererEndAnchor();
        }

        part->color[0] = rr;
        part->color[1] = gg;
        part->color[2] = bb;
    }
    EndDmaDirectTrans((qword *)(p + ndpkt * 2));
    return num;
}

// ──────────────────────────────────────────────────────────────────────
// The drips (FF3-only)
// ──────────────────────────────────────────────────────────────────────

int eff_drip_col[2][4] = {              // data 0x354c20
    { 0x8a, 0x5a, 0x5a, 0x19 },
    { 0x80, 0x40, 0x40, 0x3c },
};

static EffDripMgr *eff_drip_mgr;        // sdata 0x372fc8 (gp-0x528)

// FF3 0x00185d70; the base-object twin FF3 0x00185d20 has the same body.
EffDrip::EffDrip(const float *pos, int kind, int id)
{
    base[0] = pos[0];                   // lq / sq
    base[1] = pos[1];
    base[2] = pos[2];
    base[3] = pos[3];
    this->kind = kind;
    this->id = id;
    SetAlphaMax();
    active = 0;
    floor = 0.0f;
    type = 0;
    Reset();
}

// FF3 0x00185dc8.
EffDrip::~EffDrip()
{
}

// Kinds 0 / 1 grow to the first colour row's alpha, 2 / 3 to the second's.
// FF3 0x00185dd0.
void EffDrip::SetAlphaMax()
{
    if (kind < 0)
        return;
    if (kind < 2)
    {
        alpha_max = (float)eff_drip_col[0][3];
        return;
    }
    if (kind < 4)
        alpha_max = (float)eff_drip_col[1][3];
}

// FF3 0x00185e18.
void EffDrip::Reset()
{
    pos[0] = base[0];
    pos[1] = base[1];
    pos[2] = base[2];
    pos[3] = base[3];
    vel = 0.0f;
    acc = 0.0f;
    gravity = 0.0f;
    size = 0.0f;
    alpha = 0;
    state = 0;
    timer = (short)(GetRandValIByF(GetPALMode() != 0 ? 50 : 60) + 10);
}

// FF3 0x00185e80: the floor point's y in f12, this in a0.
void EffDrip::Start(float floor, int type)
{
    this->type = type;
    active = 1;
    this->floor = floor;
}

// FF3 0x00185e98.
void EffDrip::Splash()
{
    switch (type)
    {
    case 0:
        EffParticleReq(7, pos, id);
        break;
    case 1:
        EffParticleReq(8, pos, id);
        break;
    case 2:
        EffParticleReq(9, pos, id);
        break;
    case 3:
        EffParticleReq(10, pos, id);
        break;
    }
}

// The fall runs every frame gravity is set: acc and vel damped by 0.9, y
// grows (down). Then by state (table 0x354c00): 0 wait; 1 grow over 12 / 15
// frames (size to 0.2, alpha to alpha_max); 2 hang 0..24 / 29 frames, then
// gravity 0.0005; 3 fall (gravity 0.05) until below the floor, then land on
// it and splash; 4 fade over 2 / 3 frames, then form again.
// FF3 0x00185f28.
void EffDrip::Step()
{
    float t;
    int   n;

    if (active == 0)
        return;
    if (gravity > 0.0f)
    {
        acc = (acc + gravity) * 0.9f;   // rdata 0x36f3d8
        vel = (vel + acc) * 0.9f;
        pos[1] += vel;
    }
    switch (state)
    {
    case 0:
        if (--timer >= 0)
            return;
        state = 1;
        timer = GetPALMode() != 0 ? 12 : 15;
        break;
    case 1:
        n = GetPALMode() != 0 ? 12 : 15;
        t = (float)(n - timer) / (GetPALMode() != 0 ? 12.0f : 15.0f);  // rdata 0x36f3dc / 0x36f3e0
        timer--;
        size = t * 0.2f;                // rdata 0x36f3e4
        alpha = (short)(int)(t * alpha_max);
        if (timer >= 0)
            return;
        state = 2;
        timer = (short)GetRandValIByF(GetPALMode() != 0 ? 25 : 30);
        gravity = 0.0005f;              // rdata 0x36f3e8
        break;
    case 2:
        if (--timer >= 0)
            return;
        state = 3;
        timer = GetPALMode() != 0 ? 7500 : 9000;
        gravity = 0.05f;                // rdata 0x36f3ec
        break;
    case 3:
        if (floor < pos[1])
        {
            if (state == 3)
            {
                state = 4;
                timer = (short)((GetPALMode() == 0) + 2);
            }
            pos[1] = floor;
            gravity = 0.0f;
            Splash();
            return;
        }
        break;
    case 4:
        if (GetPALMode() == 0)
            t = (float)timer / 3.0f;    // rdata 0x36f3f4
        else
            t = (float)timer * 0.5f;    // rdata 0x36f3f0
        timer--;
        alpha = (short)(int)(t * alpha_max);
        if (timer < 0)
            Reset();
        break;
    }
}

// A camera-facing sprite: kinds 0 / 1 effdat 58 (monochrome 59) in colour
// row 0, kinds 2 / 3 effdat 59 in row 1; odd kinds twice the size.
// FF3 0x001861f0.
void EffDrip::Draw()
{
    SPRT_DAT *sprt;
    float     w;
    int       row;

    if (state == 0 || alpha <= 0)
        return;
    switch (kind)
    {
    case 0:
        w = size;
        row = 0;
        sprt = &effdat[58 + eff_wrk.monochro_mode];     // data 0x352240
        break;
    case 1:
        w = size + size;
        row = 0;
        sprt = &effdat[58 + eff_wrk.monochro_mode];
        break;
    case 2:
        w = size;
        row = 1;
        sprt = &effdat[59];             // data 0x352260
        break;
    case 3:
        w = size + size;
        row = 1;
        sprt = &effdat[59];
        break;
    default:
        return;
    }
    Eff3DFaceSprite(0.0f, w, w, sprt, pos, (u_char)eff_drip_col[row][0], (u_char)eff_drip_col[row][1],
                    (u_char)eff_drip_col[row][2], (u_char)alpha);
}

// FF3 0x00186360.
EffDripFloor::EffDripFloor(const float *pos, int type, int id)
{
    this->pos[0] = pos[0];              // lq / sq
    this->pos[1] = pos[1];
    this->pos[2] = pos[2];
    this->pos[3] = pos[3];
    this->id = id;
    this->type = type;
}

// FF3 0x00186380.
EffDripFloor::~EffDripFloor()
{
}

// The XZ distance to the drip's position below 1.0 (rdata 0x36f3f8).
// FF3 0x00186388.
int EffDripFloor::IsNear(const EffDrip *drip) const
{
    float dx = pos[0] - drip->pos[0];
    float dz = pos[2] - drip->pos[2];

    return sqrtf(dx * dx + dz * dz) < 1.0f;
}

// FF3 0x00186430.
EffDripMgr::EffDripMgr()
{
    int i;

    for (i = 0; i < EFF_DRIP_MAX; i++)
        drip[i] = nullptr;
    for (i = 0; i < EFF_DRIP_MAX; i++)
        floor[i] = nullptr;
}

// FF3 0x001864b0; the twin FF3 0x00186480 has the same body.
EffDripMgr::~EffDripMgr()
{
    CutDrip(-1);
    CutFloor(-1);
}

// The first floor point near the drip starts it and is used up.
// FF3 0x001864e0.
void EffDripMgr::Activate(EffDrip *d)
{
    EffDripFloor *fp;
    int           i;

    for (i = 0; i < EFF_DRIP_MAX; i++)
    {
        fp = floor[i];
        if (fp == nullptr || fp->IsNear(d) == 0)
            continue;
        d->Start(fp->pos[1], fp->type);
        fp = floor[i];
        if (fp != nullptr)
        {
            fp->~EffDripFloor();
            operator_delete(fp);
        }
        floor[i] = nullptr;
        return;
    }
}

// FF3 0x00186580.
void EffDripMgr::Step()
{
    EffDrip *d;
    int      i;

    for (i = 0; i < EFF_DRIP_MAX; i++)
    {
        d = drip[i];
        if (d == nullptr)
            continue;
        if (d->active == 0)
            Activate(d);
        else
            d->Step();
    }
}

// The drips' GS state: ALPHA 0x48 (Cs - 0) * As + Cd, TEX1 0x161, CLAMP
// REPEAT, TEST 0x5000d (alpha > 0, depth GEQUAL), ZBUF not written.
// FF3 0x001865f0.
void EffDripMgr::Draw()
{
    GS_REG_SET regs[5];
    int        i;

    regs[0].data = 0x48;            regs[0].reg = SCE_GS_ALPHA_1;
    regs[1].data = 0x161;           regs[1].reg = SCE_GS_TEX1_1;
    regs[2].data = 0;               regs[2].reg = SCE_GS_CLAMP_1;
    regs[3].data = 0x5000d;         regs[3].reg = SCE_GS_TEST_1;
    regs[4].data = 0x10a000118ULL;  regs[4].reg = SCE_GS_ZBUF_1;
    SetGsRegsCached(regs, 5, 1);
    for (i = 0; i < EFF_DRIP_MAX; i++)
    {
        if (drip[i] != nullptr)
            drip[i]->Draw();
    }
}

// FF3 0x001866b0.
void EffDripMgr::Main()
{
    Step();
    Draw();
}

// FF3 0x001866d8 (Ghidra had the FF2 match GetEnePartAddr).
// callcheck: EffDripCtor (the new-expression's EffDrip constructor)
int EffDripMgr::AddDrip(const float *pos, int kind, int id)
{
    int i;

    for (i = 0; i < EFF_DRIP_MAX; i++)
    {
        if (drip[i] == nullptr)
        {
            drip[i] = new (operator_new(sizeof(EffDrip))) EffDrip(pos, kind, id);
            return 1;
        }
    }
    return 0;
}

// FF3 0x00186770.
void EffDripMgr::CutDrip(int id)
{
    EffDrip *d;
    int      i;

    for (i = 0; i < EFF_DRIP_MAX; i++)
    {
        d = drip[i];
        if (d != nullptr && (id < 0 || d->id == id))
        {
            d->~EffDrip();
            operator_delete(d);
            drip[i] = nullptr;
        }
    }
}

// FF3 0x001867f0 (Ghidra had the FF2 match GetAmuPartAddr).
int EffDripMgr::AddFloor(const float *pos, int type, int id)
{
    int i;

    for (i = 0; i < EFF_DRIP_MAX; i++)
    {
        if (floor[i] == nullptr)
        {
            floor[i] = new (operator_new(sizeof(EffDripFloor))) EffDripFloor(pos, type, id);
            return 1;
        }
    }
    return 0;
}

// FF3 0x00186888.
void EffDripMgr::CutFloor(int id)
{
    EffDripFloor *fp;
    int           i;

    for (i = 0; i < EFF_DRIP_MAX; i++)
    {
        fp = floor[i];
        if (fp != nullptr && (id < 0 || fp->id == id))
        {
            fp->~EffDripFloor();
            operator_delete(fp);
            floor[i] = nullptr;
        }
    }
}

// FF3 0x00186908.
// callcheck: EffDripMgrCtor (the new-expression's EffDripMgr constructor)
void EffDripInit(void)
{
    eff_drip_mgr = new (operator_new(sizeof(EffDripMgr))) EffDripMgr;
}

// FF3 0x00186940.
// callcheck: EffDripMgrDtor (mgr->~EffDripMgr())
void EffDripRelease(void)
{
    EffDripMgr *mgr = eff_drip_mgr;

    if (mgr != nullptr)
    {
        mgr->~EffDripMgr();
        operator_delete(mgr);
    }
    eff_drip_mgr = nullptr;
}

// FF3 0x00186980: no null check in the ROM (InitEffects made the manager).
void EffDripMain(void)
{
    eff_drip_mgr->Main();
}

// FF3 0x00186998.
void EffDripReq(float *pos, int kind, int no)
{
    if (eff_drip_mgr != nullptr)
        eff_drip_mgr->AddDrip(pos, kind, no);
}

// FF3 0x001869d8.
void EffDripCut(int no)
{
    if (eff_drip_mgr != nullptr)
        eff_drip_mgr->CutDrip(no);
}

// FF3 0x00186a08.
void EffDripFloorReq(float *pos, int type, int no)
{
    if (eff_drip_mgr != nullptr)
        eff_drip_mgr->AddFloor(pos, type, no);
}

// FF3 0x00186a48.
void EffDripFloorCut(int no)
{
    if (eff_drip_mgr != nullptr)
        eff_drip_mgr->CutFloor(no);
}

// FF3 0x00186a78.
int *EffSparkColGet(void)
{
    return eff_drip_col[0];
}
