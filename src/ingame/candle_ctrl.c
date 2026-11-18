/* ==========================================================================
 *  ingame/candle_ctrl.c
 *
 *  The HUD candle (see candle_ctrl.h). FF3-only; FF3 0x13b320..0x13dba8,
 *  starting after the previous object's _GLOBAL__I_ (0x13b300) and ending at
 *  common/cmn_fade_layer.c, plus the linkonce state getters, destructors and
 *  GetCandleControl at 0x2acfb8..0x2ad3e0.
 *
 *  FF3 reads every singleton through a gp-relative cached pointer (if (!p)
 *  p = Get();; sbss gp-0xbb0..-0xb8c, and the CandleControl one gp-0xb90 is
 *  shared with other files); nothing depends on the cache, so calls go to
 *  the getters directly. The compiler's atexit thunks are not written: the
 *  host's function-local statics destroy themselves (CandleAnime*StaticDtor
 *  0x13c8c0..0x13c920 and 0x13d510..0x13d550, CandleControlStaticDtor3
 *  0x13d4f0, CandleGPhaseCtrlStaticDtor 0x13d4d0 = GPhaseCtrlTerm(0x41e468)).
 * ======================================================================== */
#include "ingame/candle_ctrl.h"

#include <string.h>                                     // memset

#include "common/heapctrl.h"                            // heapCtrlMalloc, heapCtrlFree
#include "graphics/effect/effect.h"                     // SetEffects, EffectIsSlotNo
#include "graphics/effect/effect_oth.h"                 // EffectMonochromeSet (monochrome)
#include "graphics/graph2d/g2d_draw.h"                  // DISP_SPRT, CopySprDToSpr, DispSprD
#include "graphics/graph2d/tim2.h"                      // Tim2PackSend, PK4SendVram
#include "graphics/gs_reg.h"                            // ScreenCaptureSave, ScreenCaptureDisp
#include "graphics/graph3d/gra3dMisc.h"                  // gra3dPrelight
#include "ingame/event/prg/ev_change.h"                 // Req_CompulsionSetEventState
#include "ingame/ingame.h"                              // GPhaseCtrlRequest, IngameDrawSub
#include "ingame/ingame_wrk.h"                          // ingame_wrk.plyr_wrk
#include "ingame/map/FurnSave.h"                        // FurnSaveFind
#include "ingame/map/MapObjWrk.h"                       // MapObjUpdate
#include "ingame/plyr/player.h"                         // PLYR_WRK, PLYR_FLG_800000
#include "ingame/enemy/random_ghost.h"             // RandGhostTimeReset
#include "ingame/sound/snd_ctrl.h"                      // SndSePlay
#include "ingame/story_menu.h"                          // IsLockTblOn, IsMenuEnable
#include "common/vu0math.h"                             // GetRandValIByF
#include "main/gphase.h"                                // GetGPhaseCtrl, GPHASE_CTRL::SetNext
#include "reipan/os/reipan_rand.h"                      // ReiPan_Rand, REIPAN_RAND_MAXF
#include "system/eeiop/cd_file_dat.h"                   // CANDLETEXTURE_PK4
#include "system/eeiop/cddat.h"                         // GetFileSize
#include "system/os/eecdvd.h"                           // LoadReq, IsLoadEndAll
#include "system/os/sys_heap.h"                         // GetSystemHeapWrkP
#include "system/os/system.h"                           // GetFrameTimeScale, GetPALMode
#include "graphics/graph2d/fade.h"

// ──────────────────────────────────────────────────────────────────────
// Callees in runs whose file is not settled yet (unassigned@ in tus.json).
// Move each block to its real header when that run is defined.
// ──────────────────────────────────────────────────────────────────────


// unassigned@001dfbc8

// ──────────────────────────────────────────────────────────────────────
// The animation states
// ──────────────────────────────────────────────────────────────────────

// FF3 RTTI CandleAnimeStatus (vtable 0x3414a0): one state of the candle's
// animation; EE size 4 (the vptr). The states share this file's statics
// (timer, fire level, alpha table, flicker), not members.
// layout: host -- only the vptr (here and in the six subclasses).
class CandleAnimeStatus
{
public:
    virtual int  Flicker(CandleControl *ctrl, int layer);   // EE vtable +0x00 0x0013b640 (all classes)
    virtual ~CandleAnimeStatus() {}                         // EE vtable +0x04 0x002ad120, deleting +0x08 0x002ad130
    virtual void Begin(CandleControl *ctrl);                // EE vtable +0x0c 0x0013b320
    virtual void End(CandleControl *ctrl);                  // EE vtable +0x10 0x0013b348 (all classes)
    virtual void Update(CandleControl *) {}                 // EE vtable +0x14 0x002ad150 (CandleAnimeStatusUpdate)

    void Work(CandleControl *ctrl);                         // FF3 0x0013b3c0
    float GetAlpha(int layer, int kind);                    // FF3 0x0013b500

private:
    void FireEffectSet(u_char alpha);                       // FF3 0x0013b4b8
    void FlickerNext();                                     // FF3 0x0013b558
};

// FF3 RTTI CandleAnimeNormal (vtable 0x341480; dtors 0x002ad178 / 0x002ad158):
// burning steadily; watches the grade.
// layout: host -- only the vptr
class CandleAnimeNormal : public CandleAnimeStatus
{
public:
    void Update(CandleControl *ctrl) override;              // EE vtable +0x14 0x0013b838

private:
    void CheckGrade(CandleControl *ctrl);                   // FF3 0x0013b718
};

// FF3 RTTI CandleAnimeLighting (vtable 0x3413e0; dtors 0x002ad268 / 0x002ad248):
// relit after going out.
// layout: host -- only the vptr
class CandleAnimeLighting : public CandleAnimeStatus
{
public:
    void Begin(CandleControl *ctrl) override;               // EE vtable +0x0c 0x0013c4e0
    void Update(CandleControl *ctrl) override;              // EE vtable +0x14 0x0013c550
};

// FF3 RTTI CandleAnimeFadeWait (vtable 0x341400; dtors 0x002ad238 / 0x002ad218):
// out; waits for the power to come back.
// layout: host -- only the vptr
class CandleAnimeFadeWait : public CandleAnimeStatus
{
public:
    void Begin(CandleControl *ctrl) override;               // EE vtable +0x0c 0x0013c408
    void Update(CandleControl *ctrl) override;              // EE vtable +0x14 0x0013c430
};

// FF3 RTTI CandleAnimeFade (vtable 0x341420; dtors 0x002ad208 / 0x002ad1e8):
// the flame dies.
// layout: host -- only the vptr
class CandleAnimeFade : public CandleAnimeStatus
{
public:
    void Begin(CandleControl *ctrl) override;               // EE vtable +0x0c 0x0013c080
    void Update(CandleControl *ctrl) override;              // EE vtable +0x14 0x0013c0e0
};

// FF3 RTTI CandleAnimeChange (vtable 0x341440; dtors 0x002ad1d8 / 0x002ad1b8):
// cross-fade from the old grade's picture (layer 0) to the new one (layer 1).
// layout: host -- only the vptr
class CandleAnimeChange : public CandleAnimeStatus
{
public:
    void Begin(CandleControl *ctrl) override;               // EE vtable +0x0c 0x0013bbc0
    void Update(CandleControl *ctrl) override;              // EE vtable +0x14 0x0013bc20
};

// FF3 RTTI CandleAnimeDreamIn (vtable 0x341460; dtors 0x002ad1a8 / 0x002ad188):
// the candle appears (GO_DREAM_WORLD).
// layout: host -- only the vptr
class CandleAnimeDreamIn : public CandleAnimeStatus
{
public:
    void Begin(CandleControl *ctrl) override;               // EE vtable +0x0c 0x0013b988
    void Update(CandleControl *ctrl) override;              // EE vtable +0x14 0x0013b9c0
};

static void  CandleMonoChangeReq(int color);

// ──────────────────────────────────────────────────────────────────────
// Data
// ──────────────────────────────────────────────────────────────────────

// Sprite kinds of CANDLETEXTURE_PK4's pictures (128x128 each, member =
// grade: the candle shorter each grade, no flame in member 4).
enum
{
    CANDLE_KIND_0     = 0,              // 62x108 at (14, 327): the dim halo behind it, additive
    CANDLE_KIND_1     = 1,              // 31x64 at (30, 363): the candle
    CANDLE_KIND_GLOW  = 2,              // 31x46 quarter at (46, 384), drawn mirrored four times
    CANDLE_KIND_FLAME = 3,              // 22x31 at (46, 370): flickers, scaled and tinted
    CANDLE_KIND_NUM   = 5,              // the alpha table's row (kind 4 is never drawn)
};

// Alpha per layer and kind, 0..1, data 0x2f2f80 (CandleAlphaClear).
static float candle_alpha[2][CANDLE_KIND_NUM];

static float candle_time;               // sdata 0x36bc68: the state's frame counter
static float candle_base_alpha;         // sdata 0x36bc6c: 0.5 at grades 3 and 4, else 0 (CandleAnimeStatus::Work)
static float candle_flame_sx = 1.0f;    // sdata 0x36bc70: flame scale
static float candle_flame_sy = 1.0f;    // sdata 0x36bc74
static float candle_flame_r  = 128.0f;  // sdata 0x36bc78: flame colour
static float candle_flame_g  = 128.0f;  // sdata 0x36bc7c
static float candle_flame_b  = 128.0f;  // sdata 0x36bc80
static float candle_flicker_sx = 1.0f;  // sdata 0x36bc84: flicker now (scale x, y, alpha)
static float candle_flicker_sy = 1.0f;  // sdata 0x36bc88
static float candle_flicker_a  = 1.0f;  // sdata 0x36bc8c (gp-0x7864)
static float candle_flicker_time;       // sdata 0x36bc90 (gp-0x7860): 0..7 frames
static float candle_flicker_to[3]   = { 1.0f, 1.0f, 1.0f };   // sdata 0x36bc94: x, y, alpha
static float candle_flicker_from[3] = { 1.0f, 1.0f, 1.0f };   // sdata 0x36bca0
static float candle_fire;               // sdata 0x36bcac (gp-0x7844): fire screen effect level 0..1
static int   candle_se_done;            // sdata 0x36bcb0 (gp-0x7840): the lighting SE played

static int   candle_mono_ret;           // sdata 0x36bde0 (gp-0x7710): phase to go back to from phase 48
static int   candle_mono_color;         // sdata 0x36bde4 (gp-0x770c): 1 colour, 0 monochrome

// Sprite rows drawn per layer (rodata 0x34f230, copied to the stack).
struct CANDLE_ROW
{
    short kind;                         // CANDLE_KIND_*
    short att;                          // ORed into DISP_SPRT.att (1, 2, 3: the glow's mirroring)
    short dx;                           // added to the kind's position
    short dy;
};
static_assert(sizeof(CANDLE_ROW) == 0x8, "CANDLE_ROW is 8 bytes (rodata 0x34f230 rows)");
static_assert(offsetof(CANDLE_ROW, dy) == 0x6, "CANDLE_ROW.dy");
static const CANDLE_ROW candle_row[7] =
{
    { CANDLE_KIND_GLOW,  0, -31, -46 },
    { CANDLE_KIND_GLOW,  2,   0, -46 },
    { CANDLE_KIND_GLOW,  1, -31,   0 },
    { CANDLE_KIND_GLOW,  3,   0,   0 },
    { CANDLE_KIND_1,     0,   0,   0 },
    { CANDLE_KIND_FLAME, 0, -12, -28 },
    { CANDLE_KIND_0,     0,   0,   0 },
};

// Flame scale per grade (rodata 0x34f268) and flame / glow y offset per
// grade (rodata 0x34f290): the candle burns down.
static const float candle_flame_scale[5][2] =
{
    { 0.9f, 1.0f }, { 0.8f, 0.9f }, { 0.7f, 0.8f }, { 0.5f, 0.7f }, { 0.0f, 0.0f },
};
static const short candle_flame_dy[5] = { 0, 15, 30, 45, 45 };

// Picture of each kind in the grade's texture (rodata 0x34f2a0, 0x18 bytes).
struct CANDLE_SPR
{
    short  x, y;                        // screen position (640x448)
    short  u, v, w, h;                  // in the texture
    u_long alphar;                      // GS ALPHA_1
};
static_assert(sizeof(CANDLE_SPR) == 0x18, "CANDLE_SPR is 0x18 bytes (rodata 0x34f2a0)");
static_assert(offsetof(CANDLE_SPR, h) == 0xa, "CANDLE_SPR.h");
static_assert(offsetof(CANDLE_SPR, alphar) == 0x10, "CANDLE_SPR.alphar");
static const CANDLE_SPR candle_spr[4] =
{
    { 14, 327,  1,  1, 62, 108, 0x48 },
    { 30, 363, 65,  1, 31,  64, 0x44 },
    { 46, 384, 65, 67, 31,  46, 0x44 },
    { 46, 370, 98,  1, 22,  31, 0x44 },
};

// Sprite defaults (data 0x2f2fa8): TEX0 of the candle pictures (TBP 0x2bc0).
static const SPRT_DAT candle_sprt = { 0x20058405dd30abc0, 0, 0, 0x80, 0x80, 0, 0, 0, 0x80, 0, 0 };

// ──────────────────────────────────────────────────────────────────────
// State singletons (linkonce getters with function-local statics)
// ──────────────────────────────────────────────────────────────────────

// callcheck: atexit (the static's destructor, registered by the host's runtime)
// FF3 0x002acfb8 (static gp+0x704)

static CandleAnimeStatus *CandleAnimeNormalGet(void)
{
    static CandleAnimeNormal state;
    return &state;
}

// callcheck: atexit (the static's destructor, registered by the host's runtime)
// FF3 0x002ad000 (static gp+0x6e8)

static CandleAnimeStatus *CandleAnimeChangeGet(void)
{
    static CandleAnimeChange state;
    return &state;
}

// callcheck: atexit (the static's destructor, registered by the host's runtime)
// FF3 0x002ad048 (static gp+0x6ec)

static CandleAnimeStatus *CandleAnimeFadeGet(void)
{
    static CandleAnimeFade state;
    return &state;
}

// callcheck: atexit (the static's destructor, registered by the host's runtime)
// FF3 0x002ad090 (static gp+0x700)

static CandleAnimeStatus *CandleAnimeFadeWaitGet(void)
{
    static CandleAnimeFadeWait state;
    return &state;
}

// callcheck: atexit (the static's destructor, registered by the host's runtime)
// FF3 0x002ad0d8 (static gp+0x6b8)

static CandleAnimeStatus *CandleAnimeLightingGet(void)
{
    static CandleAnimeLighting state;
    return &state;
}

// callcheck: atexit (the static's destructor, registered by the host's runtime)
// FF3 0x002ad2d8 (static gp+0x708)

static CandleAnimeStatus *CandleAnimeDreamInGet(void)
{
    static CandleAnimeDreamIn state;
    return &state;
}

// ──────────────────────────────────────────────────────────────────────
// CandleAnimeStatus
// ──────────────────────────────────────────────────────────────────────

// Flicker at rest: all 1.0.
// FF3 0x0013b380

static void CandleFlickerReset(void)
{
    candle_flicker_time   = 0.0f;
    candle_flicker_sx     = 1.0f;
    candle_flicker_sy     = 1.0f;
    candle_flicker_from[2] = 1.0f;
    candle_flicker_a      = 1.0f;
    candle_flicker_to[0]  = 1.0f;
    candle_flicker_to[1]  = 1.0f;
    candle_flicker_to[2]  = 1.0f;
    candle_flicker_from[0] = 1.0f;
    candle_flicker_from[1] = 1.0f;
}

// ──────────────────────────────────────────────────────────────────────
// Timer, fire level and SE flag cleared; flicker at rest.
// FF3 0x0013b320

void CandleAnimeStatus::Begin(CandleControl *ctrl)
{
    (void)ctrl;
    candle_fire    = 0.0f;
    candle_time    = 0.0f;
    candle_se_done = 0;
    CandleFlickerReset();
}

// ──────────────────────────────────────────────────────────────────────
// Leaving a state: layer 0's kind 0 and all of layer 1 hidden.
// FF3 0x0013b348

void CandleAnimeStatus::End(CandleControl *ctrl)
{
    int i;

    (void)ctrl;
    candle_alpha[0][0] = 0.0f;
    for (i = 0; i < CANDLE_KIND_NUM; i++)
        candle_alpha[1][i] = 0.0f;
}

// ──────────────────────────────────────────────────────────────────────
// Every picture hidden.
// FF3 0x0013b490

static void CandleAlphaClear(void)
{
    memset(candle_alpha, 0, sizeof(candle_alpha));
}

// ──────────────────────────────────────────────────────────────────────
// The fire screen effect at 'alpha' (0..60).
// callcheck: fptodp (the float -> double promotion of the variadic argument)
// FF3 0x0013b4b8

void CandleAnimeStatus::FireEffectSet(u_char alpha)
{
    SetEffects(2, 1, 8, (double)(float)alpha, 8.0, 0x36, 0x12);
}

// ──────────────────────────────────────────────────────────────────────
// Base alpha by grade, the state's Update, then the fire effect while the
// fire level is up (unless effect slot 2 holds effect 2 already).
// FF3 0x0013b3c0

void CandleAnimeStatus::Work(CandleControl *ctrl)
{
    float alpha;

    if (ctrl->GetGrade() < 3)
        candle_base_alpha = 0.0f;
    else
        candle_base_alpha = 0.5f;

    Update(ctrl);

    if (candle_fire > 0.0f)
    {
        alpha = candle_fire * 60.0f;
        if (EffectIsSlotNo(2) == 0)
            FireEffectSet((u_char)(int)alpha);
    }
}

// ──────────────────────────────────────────────────────────────────────
// The alpha of a kind on a layer; the flame's carries the flicker.
// FF3 0x0013b500

float CandleAnimeStatus::GetAlpha(int layer, int kind)
{
    if (kind == CANDLE_KIND_FLAME)
        return candle_alpha[layer][CANDLE_KIND_FLAME] * candle_flicker_a;
    return candle_alpha[layer][kind];
}

// ──────────────────────────────────────────────────────────────────────
// New flicker target: scale 0.9..1.1 (x, y), alpha 0.9..1.0. FF3's 0.2 and
// 0.1 are 0x3e4cccd0 / 0x3dccccd0 (a hair over), its RAND_MAX 0x4effffff.
// callcheck: rand (ReiPan_Rand: the ROM's 31-bit range)
// FF3 0x0013b558

void CandleAnimeStatus::FlickerNext()
{
    candle_flicker_from[0] = candle_flicker_to[0];
    candle_flicker_from[1] = candle_flicker_to[1];
    candle_flicker_from[2] = candle_flicker_to[2];
    candle_flicker_time    = 0.0f;
    candle_flicker_to[0] = ((float)ReiPan_Rand() / REIPAN_RAND_MAXF) * 0x1.9999ap-3f + 0.9f;
    candle_flicker_to[1] = ((float)ReiPan_Rand() / REIPAN_RAND_MAXF) * 0x1.9999ap-3f + 0.9f;
    candle_flicker_to[2] = ((float)ReiPan_Rand() / REIPAN_RAND_MAXF) * 0x1.9999ap-4f + 0.9f;
}

// ──────────────────────────────────────────────────────────────────────
// Lerps the flicker over 7 frames; 1 when a new target was picked.
// FF3 0x0013b640 (no Ghidra function before 2026-10-15)

int CandleAnimeStatus::Flicker(CandleControl *ctrl, int layer)
{
    (void)ctrl;
    (void)layer;
    candle_flicker_sx = candle_flicker_from[0] + (candle_flicker_time * (candle_flicker_to[0] - candle_flicker_from[0])) / 7.0f;
    candle_flicker_sy = candle_flicker_from[1] + (candle_flicker_time * (candle_flicker_to[1] - candle_flicker_from[1])) / 7.0f;
    candle_flicker_a  = candle_flicker_from[2] + (candle_flicker_time * (candle_flicker_to[2] - candle_flicker_from[2])) / 7.0f;
    candle_flicker_time += GetFrameTimeScale();
    if (candle_flicker_time > 7.0f)
    {
        FlickerNext();
        return 1;
    }
    return 0;
}

// ──────────────────────────────────────────────────────────────────────
// CandleAnimeNormal
// ──────────────────────────────────────────────────────────────────────

// The grade moved: burnt down -> Change (Fade when out), relit -> Change
// (Lighting when it was out). Same grade, out, menu allowed and not grade
// 4 any more: back to colour (phase 48).
// FF3 0x0013b718

void CandleAnimeNormal::CheckGrade(CandleControl *ctrl)
{
    CandleAnimeStatus *next;

    if (ctrl->grade < ctrl->GetGrade())
    {
        if (ctrl->GetGrade() == CANDLE_GRADE_OUT)
            next = CandleAnimeFadeGet();
        else
            next = CandleAnimeChangeGet();
    }
    else
    {
        if (ctrl->grade <= ctrl->GetGrade())
        {
            if (IsMenuEnable() && ctrl->out != 0 && ctrl->GetGrade() != CANDLE_GRADE_OUT)
            {
                CandleMonoChangeReq(1);
                GPhaseCtrlRequest(0x30, 0);
            }
            return;
        }
        if (ctrl->grade == CANDLE_GRADE_OUT)
            next = CandleAnimeLightingGet();
        else
            next = CandleAnimeChangeGet();
    }
    ctrl->SetState(next);
}

// ──────────────────────────────────────────────────────────────────────
// Colour and flame scale ease back to rest (1/10 a frame), kinds 1..3 to
// the base alpha.
// FF3 0x0013b838

void CandleAnimeNormal::Update(CandleControl *ctrl)
{
    float a = GetAlpha(0, CANDLE_KIND_GLOW);

    candle_flame_b     = candle_flame_b + (128.0f - candle_flame_b) / 10.0f;
    candle_alpha[0][1] = a + (candle_base_alpha - a) / 10.0f;
    candle_flame_sx    = candle_flame_sx + (1.0f - candle_flame_sx) / 10.0f;
    candle_flame_sy    = candle_flame_sy + (1.0f - candle_flame_sy) / 10.0f;
    candle_flame_r     = candle_flame_r + (128.0f - candle_flame_r) / 10.0f;
    candle_flame_g     = candle_flame_g + (128.0f - candle_flame_g) / 10.0f;
    candle_alpha[0][2] = candle_alpha[0][1];
    candle_alpha[0][3] = candle_alpha[0][1];
    Flicker(ctrl, 0);
    CheckGrade(ctrl);
}

// ──────────────────────────────────────────────────────────────────────
// CandleAnimeDreamIn
// ──────────────────────────────────────────────────────────────────────

// callcheck: CandleAnimeStatusBegin (the base class's Begin)
// FF3 0x0013b988 (no Ghidra function before 2026-10-15)

void CandleAnimeDreamIn::Begin(CandleControl *ctrl)
{
    CandleAnimeStatus::Begin(ctrl);
    CandleAlphaClear();
    candle_base_alpha = 0.0f;
}

// ──────────────────────────────────────────────────────────────────────
// Glow in (0..15), flame and kind 1 in (7..15), burning to frame 165,
// then down to the base alpha (165..195) and Normal.
// FF3 0x0013b9c0

void CandleAnimeDreamIn::Update(CandleControl *ctrl)
{
    if (candle_time <= 15.0f)
        candle_alpha[0][2] = candle_time / 15.0f;
    if (7.0f <= candle_time && candle_time <= 15.0f)
    {
        candle_alpha[0][1] = (candle_time - 7.0f) * 0.125f;
        candle_alpha[0][3] = candle_alpha[0][1];
        Flicker(ctrl, 0);
    }
    if (15.0f <= candle_time && candle_time <= 165.0f)
    {
        candle_alpha[0][3] = 1.0f;
        Flicker(ctrl, 0);
    }
    if (165.0f <= candle_time && candle_time <= 195.0f)
    {
        candle_alpha[0][1] = 1.0f - ((1.0f - candle_base_alpha) * (candle_time - 165.0f)) / 30.0f;
        candle_alpha[0][2] = candle_alpha[0][1];
        candle_alpha[0][3] = candle_alpha[0][1];
        Flicker(ctrl, 0);
    }
    if (195.0f <= candle_time)
        ctrl->SetState(CandleAnimeNormalGet());
    candle_time += GetFrameTimeScale();
}

// ──────────────────────────────────────────────────────────────────────
// CandleAnimeChange
// ──────────────────────────────────────────────────────────────────────

// callcheck: CandleAnimeStatusBegin (the base class's Begin)
// FF3 0x0013bbc0

void CandleAnimeChange::Begin(CandleControl *ctrl)
{
    CandleAnimeStatus::Begin(ctrl);
    candle_flame_b  = 128.0f;
    candle_flame_r  = 128.0f;
    candle_flame_sy = 1.0f;
    candle_flame_sx = 1.0f;
    candle_flame_g  = 128.0f;
}

// ──────────────────────────────────────────────────────────────────────
// Old grade up to full (7..15), kind 0 cross-fades (45..105) while the
// new grade comes in on layer 1 (75..225) and settles to the base alpha
// (225..255). Relit: the lighting SE at frame 80; a grade 3 change runs
// the fire effect (up 0..30, down from 135). Normal after frame 255 once
// the fire is down.
// FF3 0x0013bc20

void CandleAnimeChange::Update(CandleControl *ctrl)
{
    int done;

    if (7.0f <= candle_time && candle_time <= 15.0f)
    {
        candle_alpha[0][1] = candle_base_alpha + (1.0f - candle_base_alpha) * (candle_time - 7.0f) * 0.125f;
        candle_alpha[0][2] = candle_alpha[0][1];
        candle_alpha[0][3] = candle_alpha[0][1];
    }
    if (15.0f <= candle_time && candle_time <= 75.0f)
    {
        candle_alpha[0][3] = 1.0f;
        Flicker(ctrl, 0);
    }
    if (45.0f <= candle_time && candle_time <= 75.0f)
    {
        candle_alpha[0][0] = (candle_time - 45.0f) / 30.0f;
        candle_alpha[1][0] = candle_alpha[0][0];
    }
    if (75.0f <= candle_time && candle_time <= 105.0f)
    {
        candle_alpha[0][0] = 1.0f - (candle_time - 75.0f) / 30.0f;
        candle_alpha[1][1] = 1.0f - candle_alpha[0][0];
        candle_alpha[0][1] = candle_alpha[0][0];
        candle_alpha[0][3] = candle_alpha[0][0];
        candle_alpha[1][0] = candle_alpha[0][0];
        candle_alpha[1][3] = candle_alpha[1][1];
    }
    if (75.0f <= candle_time && candle_time <= 225.0f)
    {
        candle_alpha[1][3] = 1.0f;
        Flicker(ctrl, 1);
    }
    if (225.0f <= candle_time && candle_time <= 255.0f)
    {
        candle_alpha[0][2] = 1.0f - ((1.0f - candle_base_alpha) * (candle_time - 225.0f)) / 30.0f;
        candle_alpha[1][1] = candle_alpha[0][2];
        candle_alpha[1][3] = candle_alpha[0][2];
    }

    if (candle_se_done == 0 && ctrl->GetGrade() <= ctrl->grade && 80.0f <= candle_time)
    {
        SndSePlay(0x1a9b000b, 100, 0x1000, 0, nullptr, 0);
        candle_se_done = 1;
    }

    done = 0;
    if (0.0f <= candle_time)
    {
        candle_fire = candle_time / 30.0f;
        if (135.0f <= candle_time)
        {
            candle_fire = 1.0f - (candle_time - 135.0f) / 30.0f;
            if (candle_fire <= 0.0001f)
                done = 1;
        }
        if (1.0f < candle_fire)
            candle_fire = 1.0f;
    }
    if (ctrl->GetGrade() <= ctrl->grade || ctrl->GetGrade() != 3)
    {
        candle_fire = 0.0f;
        done        = 1;
    }
    if (255.0f <= candle_time && done)
    {
        ctrl->UpdateGrade();
        ctrl->SetState(CandleAnimeNormalGet());
    }
    candle_time += GetFrameTimeScale();
}

// ──────────────────────────────────────────────────────────────────────
// CandleAnimeFade
// ──────────────────────────────────────────────────────────────────────

// callcheck: CandleAnimeStatusBegin (the base class's Begin)
// FF3 0x0013c080 (no Ghidra function before 2026-10-15)

void CandleAnimeFade::Begin(CandleControl *ctrl)
{
    CandleAnimeStatus::Begin(ctrl);
    candle_flame_b  = 128.0f;
    candle_flame_r  = 128.0f;
    candle_flame_sy = 1.0f;
    candle_flame_sx = 1.0f;
    candle_flame_g  = 128.0f;
    RandGhostTimeReset();
}

// ──────────────────────────────────────────────────────────────────────
// Like Change's first half, with the fire effect rising from frame 120;
// FadeWait at frame 255 once frame 195 is passed.
// FF3 0x0013c0e0 (no Ghidra function before 2026-10-15)

void CandleAnimeFade::Update(CandleControl *ctrl)
{
    int done;

    if (7.0f <= candle_time && candle_time <= 15.0f)
    {
        candle_alpha[0][1] = candle_base_alpha + (1.0f - candle_base_alpha) * (candle_time - 7.0f) * 0.125f;
        candle_alpha[0][2] = candle_alpha[0][1];
        candle_alpha[0][3] = candle_alpha[0][1];
    }
    if (15.0f <= candle_time && candle_time <= 165.0f)
    {
        candle_alpha[0][3] = 1.0f;
        Flicker(ctrl, 0);
    }
    if (45.0f <= candle_time && candle_time <= 75.0f)
    {
        candle_alpha[0][0] = (candle_time - 45.0f) / 30.0f;
        candle_alpha[1][0] = candle_alpha[0][0];
    }
    if (75.0f <= candle_time && candle_time <= 165.0f)
    {
        candle_alpha[0][0] = 1.0f - (candle_time - 75.0f) / 90.0f;
        candle_alpha[1][1] = 1.0f - candle_alpha[0][0];
        candle_alpha[0][1] = candle_alpha[0][0];
        candle_alpha[0][3] = candle_alpha[0][0];
        candle_alpha[1][0] = candle_alpha[0][0];
        candle_alpha[1][3] = candle_alpha[1][1];
    }
    if (195.0f <= candle_time && candle_time <= 225.0f)
    {
        candle_alpha[0][2] = 1.0f - (candle_time - 195.0f) / 30.0f;
        candle_alpha[1][1] = candle_alpha[0][2];
        candle_alpha[1][3] = candle_alpha[0][2];
    }

    done = 0;
    if (120.0f <= candle_time)
    {
        done        = 195.0f <= candle_time;
        candle_fire = (candle_time - 120.0f) / 30.0f;
        if (1.0f < candle_fire)
            candle_fire = 1.0f;
    }
    if (255.0f <= candle_time && done)
    {
        ctrl->SetState(CandleAnimeFadeWaitGet());
        return;
    }
    candle_time += GetFrameTimeScale();
}

// ──────────────────────────────────────────────────────────────────────
// CandleAnimeFadeWait
// ──────────────────────────────────────────────────────────────────────

// Out: fire effect full, nothing drawn.
// FF3 0x0013c408

void CandleAnimeFadeWait::Begin(CandleControl *ctrl)
{
    (void)ctrl;
    candle_fire = 1.0f;
    CandleAlphaClear();
}

// ──────────────────────────────────────────────────────────────────────
// Once monochrome (out) the fire effect stops. Still out: asks phase 48
// for the monochrome switch as soon as the menu is allowed; power back:
// Lighting.
// FF3 0x0013c430 (no Ghidra function before 2026-10-15)

void CandleAnimeFadeWait::Update(CandleControl *ctrl)
{
    if (ctrl->out != 0)
        candle_fire = 0.0f;

    if (ctrl->GetGrade() == CANDLE_GRADE_OUT)
    {
        if (IsMenuEnable() && ctrl->out == 0)
        {
            CandleMonoChangeReq(0);
            GPhaseCtrlRequest(0x30, 0);
        }
        return;
    }
    ctrl->SetState(CandleAnimeLightingGet());
}

// ──────────────────────────────────────────────────────────────────────
// CandleAnimeLighting
// ──────────────────────────────────────────────────────────────────────

// callcheck: CandleAnimeStatusBegin (the base class's Begin)
// FF3 0x0013c4e0

void CandleAnimeLighting::Begin(CandleControl *ctrl)
{
    CandleAnimeStatus::Begin(ctrl);
    CandleAlphaClear();
    candle_flame_b  = 128.0f;
    candle_flame_r  = 128.0f;
    candle_flame_sy = 1.0f;
    candle_flame_sx = 1.0f;
    candle_flame_g  = 128.0f;
}

// ──────────────────────────────────────────────────────────────────────
// Glow in (0..15), flame in (7..15) and growing from half size (15..90),
// kind 0 up and down (60..120), all down to the base alpha (150..180);
// the lighting SE at frame 80; Normal at frame 180.
// FF3 0x0013c550

void CandleAnimeLighting::Update(CandleControl *ctrl)
{
    ctrl->UpdateGrade();

    if (0.0f <= candle_time && candle_time <= 15.0f)
        candle_alpha[0][2] = candle_time / 15.0f;
    if (7.0f <= candle_time && candle_time <= 15.0f)
        candle_alpha[0][1] = candle_base_alpha + (1.0f - candle_base_alpha) * (candle_time - 7.0f) * 0.125f;
    if (15.0f <= candle_time && candle_time <= 90.0f)
    {
        candle_alpha[0][3] = (candle_time - 15.0f) / 75.0f;
        candle_flame_sx    = candle_alpha[0][3] * 0.5f + 0.5f;
        candle_flame_sy    = candle_flame_sx;
    }
    if (60.0f <= candle_time && candle_time <= 90.0f)
        candle_alpha[0][0] = (candle_time - 60.0f) / 30.0f;
    if (15.0f <= candle_time)
        Flicker(ctrl, 0);
    if (90.0f <= candle_time && candle_time <= 120.0f)
        candle_alpha[0][0] = 1.0f - (candle_time - 90.0f) / 30.0f;
    if (150.0f <= candle_time && candle_time <= 180.0f)
    {
        candle_alpha[0][1] = 1.0f - ((1.0f - candle_base_alpha) * (candle_time - 150.0f)) / 30.0f;
        candle_alpha[0][2] = candle_alpha[0][1];
        candle_alpha[0][3] = candle_alpha[0][1];
    }
    if (candle_se_done == 0 && 80.0f <= candle_time)
    {
        SndSePlay(0x1a9b000b, 100, 0x1000, 0, nullptr, 0);
        candle_se_done = 1;
    }
    if (180.0f <= candle_time)
        ctrl->SetState(CandleAnimeNormalGet());
    candle_time += GetFrameTimeScale();
}

// ──────────────────────────────────────────────────────────────────────
// CandleControl
// ──────────────────────────────────────────────────────────────────────

// Save block 18: enable, lit, out, power (0x10 bytes). The ROM fetches the
// getter a second time for nothing.
// FF3 0x0013c938

int CandleControlSaveGet(void **addr)
{
    *addr = &GetCandleControl()->enable;
    return 0x10;
}

// ──────────────────────────────────────────────────────────────────────
// 0 in phases 0x21, 0x22 and 0x2f, where the candle is neither drawn nor
// burnt.
// FF3 0x0013c990

static int CandleIsDrawPhase(void)
{
    short id = GetGPhaseCtrl()->leaf->id;

    if (id == 0x21 || id == 0x2f || id == 0x22)
        return 0;
    return 1;
}

// ──────────────────────────────────────────────────────────────────────
// Frees the buffer (the pointer stays: Init / the destructor follow).
// FF3 0x0013c9e8

void IEventGraph2D::DataFree()
{
    if (data != nullptr)
        heapCtrlFree(GetSystemHeapWrkP(), data, 0, 0, 0);
}

// ──────────────────────────────────────────────────────────────────────
// Loads CANDLETEXTURE_PK4 into the system heap; 1 once loaded, 0 while
// loading or when the heap is full.
// FF3 0x0013ca40

int CandleControl::DataLoad()
{
    if (loaded != 0)
        return 1;

    if (data == nullptr)
    {
        data = heapCtrlMalloc(GetSystemHeapWrkP(), GetFileSize(CANDLETEXTURE_PK4), 0, 0, 0);
        if (data == nullptr)
            return 0;
        LoadReq(CANDLETEXTURE_PK4, data);
    }
    if (IsLoadEndAll())
        loaded = 1;
    return 0;
}

// ──────────────────────────────────────────────────────────────────────
// Sends picture 'no' of the pack to VRAM (no < 0: all of them); 0 while
// not loaded.
// FF3 0x0013cae8

int IEventGraph2D::TexSend(int no)
{
    if (data == nullptr || loaded == 0)
        return 0;
    if (no < 0)
        PK4SendVram(data, 0, -1, -1);
    else
        Tim2PackSend(data, no, 0, -1, -1);
    return 1;
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x002ad320 (IEventGraph2DDtor; deleting 0x002ad348)

IEventGraph2D::~IEventGraph2D()
{
    DataFree();
}

// ──────────────────────────────────────────────────────────────────────
// Only the base's destructor (inlined in FF3: vptr, IEventGraph2D::DataFree).
// callcheck: IEventGraph2DDataFree (the base destructor)
// FF3 0x002ad380 (CandleControlDtor; deleting 0x002ad3a8)

CandleControl::~CandleControl()
{
}

// ──────────────────────────────────────────────────────────────────────
// No state, grade 4, nothing loaded, off.
// FF3 0x0013cb48

void CandleControl::Init()
{
    state  = nullptr;
    grade  = CANDLE_GRADE_OUT;
    loaded = 0;
    data   = nullptr;
    enable = 0;
    lit    = 0;
    out    = 0;
    power  = 0.0f;
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x0013cb70

void CandleControl::Term()
{
    DataFree();
    Init();
}

// ──────────────────────────────────────────────────────────────────────
// The object, bss 0x41e540: the ROM's getter constructs it on the first
// call (vptr, loaded / data 0, CandleControl::Init) and registers its
// destructor with atexit (0x13d4f0).
// callcheck: CandleControlInit, atexit (the inlined constructor; the static destructor)
// FF3 0x002ad278

CandleControl *GetCandleControl(void)
{
    // layout: host -- CandleControl plus the ROM's inlined constructor
    struct CandleControlStatic : CandleControl
    {
        CandleControlStatic()
        {
            loaded = 0;
            data   = nullptr;
            Init();
        }
    };
    static CandleControlStatic ctrl;
    return &ctrl;
}

// ──────────────────────────────────────────────────────────────────────
// SET_CANDLE_ENABLE. Switched on in the dream world: the picture starts at
// the current grade; out: straight to the end of Fade (timer 999) and the
// monochrome switch, else Normal and the colour switch (phase 48).
// FF3 0x0013cbb8

void CandleControl::SetEnable(int enable)
{
    this->enable = enable;
    if (enable == 0 || lit == 0)
        return;

    grade = GetGrade();
    if (GetGrade() == CANDLE_GRADE_OUT)
    {
        SetState(CandleAnimeFadeGet());
        CandleMonoChangeReq(0);
        candle_time = 999.0f;
    }
    else
    {
        SetState(CandleAnimeNormalGet());
        CandleMonoChangeReq(1);
    }
    GPhaseCtrlRequest(0x30, 0);
}

// ──────────────────────────────────────────────────────────────────────
// GO_REAL_WORLD: 0.
// FF3 0x0013ccb8

void CandleControl::SetLit(int lit)
{
    this->lit = lit;
}

// ──────────────────────────────────────────────────────────────────────
// GO_DREAM_WORLD: a fresh candle (power 100) appears (DreamIn); the candle
// furniture is reset.
// FF3 0x0013ccc0

void CandleControl::Reset()
{
    out   = 0;
    lit   = 1;
    state = nullptr;
    power = 100.0f;
    grade = GetGrade();
    FurnReset();
    SetState(CandleAnimeDreamInGet());
}

// ──────────────────────────────────────────────────────────────────────
// After a save is loaded: out -> monochrome again and FadeWait, else
// Normal.
// FF3 0x0013cd30

void CandleControl::LoadRestore()
{
    CandleAnimeStatus *state;

    if (enable == 0 || lit == 0)
        return;

    if (GetGrade() == CANDLE_GRADE_OUT)
    {
        out = 1;
        EffectMonochromeSet(1, 0);
        state = CandleAnimeFadeWaitGet();
    }
    else
    {
        state = CandleAnimeNormalGet();
    }
    SetState(state);
}

// ──────────────────────────────────────────────────────────────────────
// Power clamped to 0..100 (1 when clamped).
// FF3 0x0013cdd8

int CandleControl::SetPower(float power)
{
    this->power = power;
    if (power > 100.0f)
    {
        this->power = 100.0f;
        return 1;
    }
    if (power < 0.0f)
    {
        this->power = 0.0f;
        return 1;
    }
    return 0;
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x0013ce18 (no Ghidra function before 2026-10-15; no caller seen)

int CandleControl::AddPower(float d)
{
    return SetPower(power + d);
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x0013ce38

int CandleControl::SubPower(float d)
{
    return SetPower(power - d);
}

// ──────────────────────────────────────────────────────────────────────
// IngameMain: burns 0.011 a frame (100 -> 0 in about 2.5 hours at 60 Hz)
// while burning steadily, unlocked (3) and in a phase that shows it.
// FF3 0x0013ce58

void CandleControl::Main()
{
    if (enable == 0 || lit == 0)
        return;
    if (IsLockTblOn(3) || !CandleIsDrawPhase())
        return;
    if (state == CandleAnimeNormalGet())
        SubPower(0.011f);
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x0013cf00

void CandleControl::UpdateGrade()
{
    grade = GetGrade();
}

// ──────────────────────────────────────────────────────────────────────
// Power > 70: 0, > 30: 1, > 10: 2, > 0: 3, else 4 (out).
// FF3 0x0013cf28

int CandleControl::GetGrade()
{
    float power = this->power;

    if (power > 70.0f)
        return 0;
    if (power > 30.0f)
        return 1;
    if (power > 10.0f)
        return 2;
    if (power > 0.0f)
        return 3;
    return CANDLE_GRADE_OUT;
}

// ──────────────────────────────────────────────────────────────────────
// Active and burnt out (PlyrDamage doubles the damage then).
// FF3 0x0013cf90

int CandleControl::IsOut()
{
    if (enable == 0 || lit == 0)
        return 0;
    return !(power > 0.0f);
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x0013cfc8

int CandleControl::IsActive()
{
    return enable != 0 && lit != 0;
}

// ──────────────────────────────────────────────────────────────────────
// Switches the animation state: the old one's End, the new one's Begin,
// then one Work (its first Update).
// FF3 0x0013cfe8

void CandleControl::SetState(CandleAnimeStatus *state)
{
    if (this->state == state)
        return;
    if (this->state != nullptr)
        this->state->End(this);
    this->state = state;
    state->Begin(this);
    this->state->Work(this);
}

// ──────────────────────────────────────────────────────────────────────
// Runs the state (Work) and draws the candle unless the camera is up:
// layer 0 at the stored grade, and while changing or fading layer 1 at
// the current one, each from its own texture member. The flame is tinted,
// scaled by grade and flicker and lowered as the candle burns down.
// FF3 0x0013d068

void CandleControl::Draw()
{
    DISP_SPRT ds;
    int       layer, grade, i;
    float     alpha;

    if (!IsActive() || !CandleIsDrawPhase() || state == nullptr)
        return;

    state->Work(this);
    if (ingame_wrk.plyr_wrk->flags & PLYR_FLG_800000)
        return;

    for (layer = 0; layer < 2; layer++)
    {
        grade = layer == 0 ? this->grade : GetGrade();
        if (grade >= 5)
            continue;
        if (!TexSend(grade))
            return;

        for (i = 0; i < 7; i++)
        {
            const CANDLE_ROW *row = &candle_row[i];
            const CANDLE_SPR *spr = &candle_spr[row->kind];
            short             dy  = row->dy;

            alpha = state->GetAlpha(layer, row->kind);
            if (alpha == 0.0f)
                continue;

            CopySprDToSpr(&ds, &candle_sprt);
            if (row->kind == CANDLE_KIND_FLAME)
            {
                ds.r   = (u_char)(int)candle_flame_r;
                ds.g   = (u_char)(int)candle_flame_g;
                ds.b   = (u_char)(int)candle_flame_b;
                ds.scw = candle_flame_scale[grade][0] * (candle_flame_sx * candle_flicker_sx);
                ds.sch = candle_flame_scale[grade][1] * (candle_flame_sy * candle_flicker_sy);
                dy     = (short)(row->dy + candle_flame_dy[grade]);
                ds.csy = (float)(spr->y + candle_flame_dy[grade]);
                ds.csx = (float)spr->x;
            }
            else
            {
                ds.r = 0x80;
                ds.g = 0x80;
                ds.b = 0x80;
            }
            ds.alphar = spr->alphar;
            ds.u      = spr->u;
            ds.v      = spr->v;
            ds.w      = spr->w;
            ds.h      = spr->h;
            ds.x      = (float)(spr->x + row->dx);
            ds.alpha  = (u_char)(int)(alpha * 128.0f);
            ds.y      = (float)(spr->y + dy);
            ds.att   |= row->att;
            DispSprD(&ds);
        }

        if (state != CandleAnimeChangeGet() && state != CandleAnimeFadeGet())
            return;
    }
}

// ──────────────────────────────────────────────────────────────────────
// Phase 48 (0x30): the colour / monochrome switch
// ──────────────────────────────────────────────────────────────────────

// Remembers the phase to come back to and which way to switch (1 colour,
// 0 monochrome); the caller then requests phase 0x30.
// FF3 0x0013d560

static void CandleMonoChangeReq(int color)
{
    candle_mono_color = color;
    candle_mono_ret   = GetGPhaseCtrl()->leaf->id;
}

// ──────────────────────────────────────────────────────────────────────
// Freezes the picture (screen capture) and fades it to black (25 frames
// PAL, 30 NTSC); done when the fade is.
// FF3 0x0013d5a8

int init_Story_CandleMono(void)
{
    int end;

    if (GetGPhaseCtrl()->leaf->state_cnt == 0)
    {
        FadeOutReq(0, 0, 0, GetPALMode() ? 25 : 30);
        ScreenCaptureSave(2, 0);
        GetGPhaseCtrl()->leaf->state_cnt = 1;
    }
    ScreenCaptureDisp(0x80, 0x80, 0x80, 0x80);
    end = GetFadeState();
    FadeMain();
    return end != 0;
}

// ──────────────────────────────────────────────────────────────────────
// Switches monochrome (out) or colour, redraws the rooms' models and goes
// back to the saved phase.
// FF3 0x0013d678

int one_Story_CandleMono(void)
{
    int mono = candle_mono_color == 0;

    EffectMonochromeSet(mono, 0);
    GetCandleControl()->out = mono;
    gra3dPrelight(0);
    FadeMain();
    GetGPhaseCtrl()->SetNext((short)candle_mono_ret, 0);
    return 1;
}

// ──────────────────────────────────────────────────────────────────────
// Fades the game picture back in.
// FF3 0x0013d700

int end_Story_CandleMono(void)
{
    int end;

    if (GetGPhaseCtrl()->leaf->state_cnt == 0)
    {
        FadeInReq(0, 0, 0, GetPALMode() ? 25 : 30);
        GetGPhaseCtrl()->leaf->state_cnt = 1;
    }
    IngameDrawSub(0);
    end = GetFadeState();
    FadeMain();
    return end != 0;
}

// ──────────────────────────────────────────────────────────────────────
// The candle furniture (a stateless helper, built on the stack by
// CandleControl::FurnReset): table rows {id, area, label, word, event} of
// the furniture (kind 0, id 4) and the event that go with a candle. Only
// row id 0 exists (area 0x2b, label 2, event 0x6f3) and the candidate
// table holds -1 (none), so a reset hides it and sets event 0x6f3 to 6.
// ──────────────────────────────────────────────────────────────────────

// layout: host -- no fields (the ROM builds 0x10 bytes of stack for it). C++
// in FF3: 0x13db90 constructs it, and the helpers get it in a0 (unused).
struct CandleFurn
{
    CandleFurn();                         // FF3 0x0013db90 (CandleFurnInit)
    int FindRow(int id);                  // FF3 0x0013d8f8 (CandleFurnFindRow)
    short GetArea(int row);               // FF3 0x0013d910 (CandleFurnGetArea)
    short GetLabel(int row);              // FF3 0x0013d928 (CandleFurnGetLabel)
    int GetWord8(int row);                // FF3 0x0013d940 (CandleFurnGetWord8)
    int GetEvent(int row);                // FF3 0x0013d958 (CandleFurnGetEvent)
    void SetEvent(int event, int on);     // FF3 0x0013d970 (CandleFurnSetEvent)
    void SetCur(int on);                  // FF3 0x0013d998 (CandleFurnSetCur)
    void HideCur();                       // FF3 0x0013da08 (CandleFurnHideCur)
    void ShowCur();                       // FF3 0x0013da20 (CandleFurnShowCur)
    int SetVisible(int id, u_char on);    // FF3 0x0013da38 (CandleFurnSetVisible)
    int Reset(int sel);                   // FF3 0x0013dac8 (CandleFurnReset)
};

struct CANDLE_FURN_ROW
{
    int   id;
    short area;
    short label;
    int   word8;                        // read by CandleFurnGetWord8 only (no caller)
    int   event;
};
static_assert(sizeof(CANDLE_FURN_ROW) == 0x10, "CANDLE_FURN_ROW is 0x10 bytes (data 0x2f2fe8)");
static_assert(offsetof(CANDLE_FURN_ROW, event) == 0xc, "CANDLE_FURN_ROW.event");
static const CANDLE_FURN_ROW candle_furn_tbl[1] =       // data 0x2f2fe8
{
    { 0, 0x2b, 2, 0x6f3, 0x6f3 },
};

static int    candle_furn_cur;                          // sdata 0x36bde8 (gp-0x7708): the id shown
static s_char candle_furn_cand[4] = { -1, -1, -1, -1 }; // sdata 0x36bdec (gp-0x7704): candidate id per table, -1 none

// {id, count} list entry of CandleFurnListAdd (on the stack).
struct CANDLE_FURN_CNT
{
    int id;
    int cnt;
};
static_assert(sizeof(CANDLE_FURN_CNT) == 0x8, "CANDLE_FURN_CNT is 8 bytes");

// Counts 'id' up, or appends it with count 1; returns the new length.
// FF3 0x0013d7f8

static int CandleFurnListAdd(CANDLE_FURN_CNT *list, int num, int id)
{
    int i;

    for (i = 0; i < num; i++)
    {
        if (list[i].id == id)
        {
            list[i].cnt++;
            return num;
        }
    }
    list[num].cnt = 1;
    list[num].id  = id;
    return num + 1;
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x0013d850

static int CandleFurnListGet(int sel, CANDLE_FURN_CNT *list)
{
    if (candle_furn_cand[sel] < 0)
        return 0;
    return CandleFurnListAdd(list, 0, candle_furn_cand[sel]);
}

// ──────────────────────────────────────────────────────────────────────
// Picks one of 'cnt' (random); the id shown when 'id' is table sel's
// candidate and the pick is 0, else -1.
// FF3 0x0013d890

static int CandleFurnPick(int sel, s_char id, int cnt)
{
    int r = 0;

    if (cnt > 1)
        r = GetRandValIByF(cnt - 1);
    if (candle_furn_cand[sel] != id)
        return -1;
    if (r != 0)
        return -1;
    return candle_furn_cur;
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x0013d8f8

int CandleFurn::FindRow(int id)
{
    return candle_furn_tbl[0].id == id ? 0 : -1;
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x0013d910

short CandleFurn::GetArea(int row)
{
    return candle_furn_tbl[row].area;
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x0013d928

short CandleFurn::GetLabel(int row)
{
    return candle_furn_tbl[row].label;
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x0013d940 (no caller; no Ghidra function before 2026-10-15)

int CandleFurn::GetWord8(int row)
{
    return candle_furn_tbl[row].word8;
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x0013d958

int CandleFurn::GetEvent(int row)
{
    return candle_furn_tbl[row].event;
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x0013d970

void CandleFurn::SetEvent(int event, int on)
{
    Req_CompulsionSetEventState(0, event, on ? 0 : 6);
}

// ──────────────────────────────────────────────────────────────────────
// Shows or hides the furniture of 'id' (visibility masks 0..2) and
// updates the map objects. Where FurnSaveFind misses, the ROM writes
// through a null row (low EE memory); the port skips the write.
// FF3 0x0013da38

int CandleFurn::SetVisible(int id, u_char on)
{
    int            row  = FindRow(id);
    short          area = GetArea(row);
    FURN_SAVE_ROW *save = FurnSaveFind(area, 0, 4, GetLabel(row));

    if (save != nullptr)
    {
        save->vis[2] = on;
        save->vis[0] = on;
        save->vis[1] = on;
    }
    MapObjUpdate(-1);
    return 0;
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x0013d998

void CandleFurn::SetCur(int on)
{
    int id = candle_furn_cur;

    SetEvent(GetEvent(candle_furn_cur), on);
    SetVisible(id, (u_char)on);
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x0013da08

void CandleFurn::HideCur()
{
    SetCur(0);
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x0013da20 (no caller)

void CandleFurn::ShowCur()
{
    SetCur(1);
}

// ──────────────────────────────────────────────────────────────────────
// Hides the current one, then shows each candidate of table 'sel' that
// the random pick lands on.
// FF3 0x0013dac8

int CandleFurn::Reset(int sel)
{
    CANDLE_FURN_CNT list[4];
    CANDLE_FURN_CNT *e = list;
    int             num, id;

    HideCur();
    list[0].id = -1;
    num = CandleFurnListGet(sel, list);
    while (num > 0)
    {
        num--;
        id = CandleFurnPick(sel, (s_char)e->id, e->cnt);
        SetEvent(GetEvent(FindRow(id)), 1);
        SetVisible(id, 1);
        e++;
    }
    return 0;
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x0013db90

CandleFurn::CandleFurn()
{
    Reset(0);
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x0013cb98 (was named global.constructors.keyed.to.AlbumViewCtrlInit)
// callcheck: CandleFurnInit (furn's constructor)

void CandleControl::FurnReset()
{
    CandleFurn furn;                    // its constructor runs Reset(0)
}
