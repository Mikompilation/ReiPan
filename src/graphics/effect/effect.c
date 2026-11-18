// graphics/effect/effect.c -- the effect system's core (FF3 0x156e08..0x158ed0,
// plus EffectInPhoto emitted out of line at 0x2afd10; FF2 effect.c).
//
// The EFFECT_CONT banks, the SetEffects front door and its per-kind
// unpackers (FF3 renumbered FF2's ids; the table is in effect.h), the
// per-frame EffectControl passes and the depth-sorted model-effect draw
// (EffectZSort), the twelve whole-screen looks, and the effect allocation
// helpers. FF3 dropped FF2's private 0x90000 effect heap: EFFECT_MALLOC /
// EFFECT_FREE go to the system heap.

#include "graphics/effect/effect.h"
#include "graphics/effect/effect_sub.h" // PC: ReiPan_EffDebugOverlay

#include <stdarg.h>                             // va_list
#include <string.h>                             // memset, memcpy

#include "common/heapctrl.h"                    // heapCtrlMalloc / heapCtrlFree
#include "system/os/sys_heap.h"                 // GetSystemHeapWrkP
#include "graphics/graph3d/gra3d.h"             // gra3dGetCamera
#include "graphics/effect/effect_butterfly.h"
#include "graphics/effect/effect_data.h"    // EffButterflyMdlLoad / Release / IsLoaded
#include "graphics/effect/effect_ene.h"
#include "graphics/effect/effect_mapsky.h"  // SetMapSky
#include "graphics/effect/effect_obj.h"
#include "graphics/effect/effect_oth.h"
#include "graphics/effect/effect_scrsaver.h" // ScreenSaverInit / Main / Draw
#include "graphics/effect/effect_torch.h"   // SetTorch2 / SetSpark / SetTorch2BigFrea, the light shaft
#include "ingame/ingame.h"                      // IngameGetDrawFlags
#include "ingame/ingame_effect.h"               // IgEffectInit / IgEffectMain / IgEffectStoryMainScreenEffectReq
#include "ingame/candle_ctrl.h"                 // GetCandleControl
#include "ingame/plyr/player.h"                 // PlyrGetCurWrk, PLYR_FLG_800000
#include "ingame/movie_room_menu/prg/movie_projecter.h"     // IsMoviePhase

// ──────────────────────────────────────────────────────────────────────
// Data
// ──────────────────────────────────────────────────────────────────────

EFF_WRK eff_wrk;                        // FF3 data 0x2f6a10

// The twelve whole-screen looks (data 0x2f6410, 0x80 each), and the pointer
// table EffectGetScreenEffectParamPtr indexes (data 0x2f6a30).
static SCREEN_EFFECT_PARAMETER screen_effect_param[SCREEN_EFFECT_NUM] =
{
    { 0, 3,  8, 32, 54, 18, 0, 32, 1000, 1800, 0, 0,  0, 0, 0, 3,  75,  75, 128, 128, 128, 0, 26, 0, { 1, 0,  0,  0,  0,  0, 0, 0 } },
    { 0, 3,  8, 20, 54, 18, 0, 32, 1000, 1800, 0, 0,  0, 0, 0, 3,  75,  75, 128, 128, 128, 0, 26, 0, { 1, 0,  0,  0,  0,  0, 0, 0 } },
    { 0, 6, 31, 50, 54, 18, 0, 32, 1000, 1800, 0, 0,  0, 1, 0, 3,  75,  75, 128, 128, 128, 0, 31, 0, { 1, 1, 39, 10, 50,  3, 0, 0 } },
    { 0, 6, 32, 20, 54, 18, 0, 32, 1000, 1800, 0, 0,  0, 1, 0, 3,  75,  75, 128, 128, 128, 0, 31, 0, { 1, 1, 21, 10, 18,  3, 0, 0 } },
    { 0, 3,  8, 15, 54, 18, 0, 32, 1000, 1800, 0, 0,  0, 0, 0, 3,  75,  75, 128, 128, 128, 0, 26, 0, { 1, 0,  0,  0,  0,  0, 0, 0 } },
    { 0, 6, 31, 50, 54, 18, 0, 32, 1000, 1800, 0, 0,  0, 1, 0, 4,  41, 218, 128, 128, 128, 0, 31, 0, { 1, 1, 39, 27, 64,  7, 0, 0 } },
    { 0, 6, 31, 50, 54, 18, 0, 32, 1000, 1800, 0, 0,  0, 1, 0, 4,  41, 218, 128, 128, 128, 0, 31, 0, { 1, 1, 21, 27, 23,  7, 0, 0 } },
    { 0, 8, 31, 40, 54, 18, 0, 32, 1000, 1800, 8, 0, 13, 1, 0, 3, 233, 225, 128, 128, 128, 1, 32, 0, { 1, 1, 61, 53, 20, 94, 0, 0 } },
    { 0, 8, 31, 40, 54, 18, 0, 32, 1000, 1800, 8, 0, 13, 1, 0, 3, 189, 159, 128, 128, 128, 1, 32, 0, { 1, 1, 61, 53, 20, 94, 0, 0 } },
    { 0, 3, 50, 92, 54, 18, 0, 32, 1000, 1800, 8, 9,  0, 0, 0, 3, 150, 150, 128, 128, 128, 0, 26, 0, { 1, 1,  0,  0,  0,  0, 9, 0 } },
    { 0, 6, 31, 50, 54, 18, 0, 32, 1000, 1800, 0, 0,  0, 1, 0, 4,  84, 124, 128, 128, 128, 0, 31, 0, { 1, 1, 39, 27, 64,  7, 0, 0 } },
    { 0, 6, 31, 50, 54, 18, 0, 32, 1000, 1800, 0, 0,  0, 1, 0, 4,  84, 124, 128, 128, 128, 0, 31, 0, { 1, 1, 21, 27, 23,  7, 0, 0 } },
};
static SCREEN_EFFECT_PARAMETER *screen_effect_param_ptr[SCREEN_EFFECT_NUM] =
{
    &screen_effect_param[0], &screen_effect_param[1], &screen_effect_param[2],  &screen_effect_param[3],
    &screen_effect_param[4], &screen_effect_param[5], &screen_effect_param[6],  &screen_effect_param[7],
    &screen_effect_param[8], &screen_effect_param[9], &screen_effect_param[10], &screen_effect_param[11],
};

// The control banks (bss): change_bank 1 picks efcnt / efcntm, 0 the
// *_cnt twins (FF2's names).
static EFFECT_CONT efcnt[EFFECT_SCREEN_NUM];        // bss 0x393b50 (0xa80)
static EFFECT_CONT efcntm[EFFECT_MODEL_NUM];        // bss 0x3945d0 (0x1c00)
static EFFECT_CONT efcnt_cnt[EFFECT_SCREEN_NUM];    // bss 0x3961d0
static EFFECT_CONT efcntm_cnt[EFFECT_MODEL_NUM];    // bss 0x396c50

static int effect_control_count[3];     // bss 0x398850: EffectControl calls per pass, cleared every frame
static int effect_zsort[EFFECT_MODEL_NUM];  // bss 0x398860: EffectZSort's slot order

static u_int screen_effect_no;          // sbss gp-0x5dc: EffectSetScreenEffectNo

static void EffectControlCountClear(void);
static void EffectZSort(int PDeformBlurFlg);
static void EffectInPhoto(EFFECT_CONT *ecm, int PDeformBlurFlg);

// The envelope's first phase (FF2's in / keep / out decision, inlined in
// every unpacker that arms one): the first window that is not empty.
static inline u_int EffFlowStart(u_int in, u_int keep, u_int out)
{
    if (in != 0)
        return 0;
    if (keep != 0)
        return 1;
    if (out != 0)
        return 2;
    return 3;
}

// A 16-byte position copy (the ROM's lq / sq).
static inline void EffPosCopy(EFFECT_CONT *ec, const float *pos)
{
    memcpy(ec->Pos, pos, sizeof(ec->Pos));
}

// ──────────────────────────────────────────────────────────────────────
// Init / release / per frame
// ──────────────────────────────────────────────────────────────────────

// FF3 0x00156e08
void InitEffects(void)
{
    eff_wrk.monochro_mode = 0;
    eff_wrk.disp_flg = 1;
    eff_wrk._1c = 0x55;
    eff_wrk._18 = 1;
    eff_wrk.stop_flg = 0;
    eff_wrk.change_bank = 0;
    eff_wrk.blur_off = 0;
    eff_wrk._14 = 0;

    memset(efcnt, 0, sizeof(efcnt));
    memset(efcnt_cnt, 0, sizeof(efcnt_cnt));
    memset(efcntm, 0, sizeof(efcntm));
    memset(efcntm_cnt, 0, sizeof(efcntm_cnt));

    EffObjBufTblInit();
    RoomGlowInit();
    InitEffectScr();
    EffRoomGlowInit();
    ScreenSaverInit();
    EffKind1DInit();
    EffectObjInit();
    EffectObjOthInit();
    InitEffectEne();
    EffectButterflyInit();
    EffectButterflyParticleInit();
    EffButterflyMdlLoad();
    EffRainInit();
    EffAreaPartInit();
    EffDropInit();
    EffMapPartInit();
    EffDripInit();
    EffectTorch2BigFreaInit();
    EffSoraLoadReq();
    EffLanternInit();
    EffLanternMdlLoad();
    screen_effect_no = 0;
    EffectControlCountClear();
    IgEffectInit();
}

// 1 while any of the three effect data sources answers (IngameNowLoadingMain
// goes on when this is non-zero).
// FF3 0x00156f50
int CheckSetEffectsOK(void)
{
    if (EffButterflyMdlIsLoaded() != 0 || EffLanternMdlIsLoaded() != 0 || EffObjIsLoadEnd() != 0)
        return 1;
    return 0;
}

// FF3 0x00156fa8
void InitEffectsEF(void)
{
    InitEffectScrEF();
    ScreenSaverMain();
    EffKind1DInitEF();
    EffectObjInitEF();
    InitEffectObjEF();
    InitEffectEneEF();
    IgEffectStoryMainScreenEffectReq(EffectGetNowScreenEffectParamPtr());
    EffectControlCountClear();
}

// Every effect sub-system released, then every record of both banks ended.
// FF3 0x00157000
void EffectsReleaseAll(void)
{
    int bank;
    int i;

    EffObjBufTblRelease();
    RoomGlowRelease();
    EffectScrRelease();
    EffKind1DReleaseAll();
    EffectObjReleaseAll();
    InitEffectObj();
    EffectEneReleaseAll();
    EffectButterflyAllCut();
    EffectButterflyParticleAllCut();
    EffButterflyMdlRelease();
    EffRainRelease();
    EffAreaPartReleaseAll();
    EffDropRelease();
    EffMapPartRelease();
    EffDripRelease();
    EffectTorch2BigFreaCut();
    EffSoraRelease();
    EffKind600AllCut();
    EffLanternMdlRelease();
    IgEffectRelease();

    // Both banks; leaves bank 1 live.
    for (bank = 0; bank < 2; bank++)
    {
        eff_wrk.change_bank = (bank != 0);
        for (i = 0; i < EFFECT_SCREEN_NUM; i++)
            EffectRelease(EffectGetScreenCont(i));
        for (i = 0; i < EFFECT_MODEL_NUM; i++)
            EffectRelease(EffectGetModelCont(i));
    }
}

// FF3 0x00157150
void EffectDrawMain(u_int flag)
{
    (void)flag;                         // passed on to EffectControl, which ignores it

    InitEffectsEF();
    if (IsMoviePhase() == 0)
        IgEffectMain();
    EffectControl(0);
    EffectControl(1);
    BrightnessAdjustmentFilterDraw(1.0f, 0, 0, 0x280, 0x1c0);
    EffectControl(2);
    EffSealDoorBurnDraw();

    ReiPan_EffDebugOverlay();           // PC: the Debug menu's light cone (effect_sub.c)
}

// ──────────────────────────────────────────────────────────────────────
// Banks and requests
// ──────────────────────────────────────────────────────────────────────

// FF3 0x001571d0
EFFECT_CONT *EffectGetScreenCont(u_int no)
{
    if (no >= EFFECT_SCREEN_NUM)
        return nullptr;
    if (eff_wrk.change_bank != 0)
        return &efcnt[no];
    return &efcnt_cnt[no];
}

// FF3 0x00157218
EFFECT_CONT *EffectGetModelCont(u_int no)
{
    if (no >= EFFECT_MODEL_NUM)
        return nullptr;
    if (eff_wrk.change_bank != 0)
        return &efcntm[no];
    return &efcntm_cnt[no];
}

// FF2 CheckSetEffectsOK's role.
// FF3 0x00157260
int EffectIsDrawOn(void)
{
    if ((u_char)eff_wrk.disp_flg == 0)  // the ROM reads the low byte
        return 0;
    return (IngameGetDrawFlags() & 0x10) == 0;
}

// First empty model slot, -1 when all 0x40 are taken.
// FF3 0x001583d0
static int SearchEmptyEffectBuf(void)
{
    EFFECT_CONT *ec = EffectGetModelCont(0);
    int ret = -1;
    int i;

    for (i = 0; i < EFFECT_MODEL_NUM && ret == -1; i++, ec++)
    {
        if (ec->dat.uc8[0] == 0)
            ret = i;
    }
    return ret;
}

// id -> record: ids 0..13 and 16..18 own the fixed slot of that number,
// 19, 20 and 22..30 take an empty model slot; 14, 15, 21 and >= 31 none.
// FF3 0x001572a0 (a jump table at 0x352680)
static EFFECT_CONT *GetEffectContBuf(u_int id)
{
    if (id >= 0x1f)
        return nullptr;

    switch (id)
    {
    case 0x0e:
    case 0x0f:
    case 0x15:
        return nullptr;

    case 0x13:
    case 0x14:
    case 0x16: case 0x17: case 0x18: case 0x19: case 0x1a:
    case 0x1b: case 0x1c: case 0x1d: case 0x1e:
        return EffectGetModelCont((u_int)SearchEmptyEffectBuf());   // -1: nullptr

    default:
        return EffectGetScreenCont(id);
    }
}

// ---- the per-kind unpackers (FF2 SetEffects_*; FF3 0x1576b8..0x158348) ----
// Each reads its argument tail from the va_list SetEffects started.

// id 1: (no arguments) -- FF3 0x00157800
static void SetEffects_Z_DEP(EFFECT_CONT *ec, int type, int fl)
{
    (void)type;                         // the dispatch passes 0, never read
    ec->dat.uc8[1] = (u_char)fl;
    ec->dat.uc8[0] = 1;
}

// id 2: (type, alp, spd, amax, cmax [, in, keep, out]) -- FF3 0x001578e0
// callcheck: dptofp (the (float) casts of the double slots)
static void SetEffects_DITHER(EFFECT_CONT *ec, int fl, va_list *ap)
{
    ec->dat.uc8[0] = 2;
    ec->dat.uc8[1] = (u_char)fl;
    ec->dat.uc8[2] = (u_char)va_arg(*ap, int);
    ec->dat.fl32[2] = (float)va_arg(*ap, double);
    ec->dat.fl32[3] = (float)va_arg(*ap, double);
    ec->dat.uc8[3] = (u_char)va_arg(*ap, int);
    ec->dat.uc8[4] = (u_char)va_arg(*ap, int);
    if (fl & 4)
    {
        ec->in = va_arg(*ap, u_int);
        ec->keep = va_arg(*ap, u_int);
        ec->out = va_arg(*ap, u_int);
        ec->cnt = 0;
        ec->flow = EffFlowStart(ec->in, ec->keep, ec->out);
    }
}

// id 3: (type, alpha, scale, rot, cx, cy) -- FF3 0x001576b8
// callcheck: dptofp (the (float) casts of the double slots)
static void SetEffects_BLUR(EFFECT_CONT *ec, int fl, va_list *ap)
{
    ec->dat.uc8[1] = (u_char)fl;
    ec->dat.uc8[0] = 3;
    ec->dat.uc8[2] = (u_char)va_arg(*ap, int);
    ec->dat.uc8[3] = (u_char)va_arg(*ap, int);
    ec->dat.ui32[2] = va_arg(*ap, u_int);
    ec->dat.ui32[3] = va_arg(*ap, u_int);
    ec->fw[0] = (float)va_arg(*ap, double);
    ec->fw[1] = (float)va_arg(*ap, double);
}

// id 4: (type, rate [, in, keep, out]). The envelope is decided from the
// previous request's windows before the new ones are stored (FF2's order).
// FF3 0x00157810
static void SetEffects_DEFORM(EFFECT_CONT *ec, int fl, va_list *ap)
{
    ec->dat.uc8[1] = (u_char)fl;
    ec->dat.uc8[0] = 4;
    ec->dat.uc8[2] = (u_char)va_arg(*ap, int);
    ec->dat.uc8[3] = (u_char)va_arg(*ap, int);
    ec->dat.uc8[4] = 0;
    if (fl & 4)
    {
        ec->flow = EffFlowStart(ec->in, ec->keep, ec->out);
        ec->cnt = 0;
        ec->in = va_arg(*ap, u_int);
        ec->keep = va_arg(*ap, u_int);
        ec->out = va_arg(*ap, u_int);
    }
}

// id 5: (power) -- FF3 0x00157748
static void SetEffects_FOCUS(EFFECT_CONT *ec, int fl, va_list *ap)
{
    ec->dat.uc8[1] = (u_char)fl;
    ec->dat.uc8[0] = 5;
    ec->dat.uc8[2] = (u_char)va_arg(*ap, int);
}

// id 6: (alpha) -- FF3 0x00157a60
static void SetEffects_OVERLAP(EFFECT_CONT *ec, int fl, va_list *ap)
{
    ec->dat.uc8[1] = (u_char)fl;
    ec->dat.uc8[0] = 6;
    ec->dat.uc8[2] = (u_char)va_arg(*ap, int);
}

// id 7: (alpha, pri) -- FF3 0x001578c0
static void SetEffects_FADEFRAME(EFFECT_CONT *ec, int fl, va_list *ap)
{
    ec->dat.uc8[1] = (u_char)fl;
    ec->dat.uc8[0] = 7;
    ec->dat.uc8[2] = (u_char)va_arg(*ap, int);
    ec->dat.ui32[1] = va_arg(*ap, u_int);
}

// id 8: (type, pPos, pRot, fw0, fw1) -- FF3 0x00157fc8
// callcheck: dptofp (the (float) casts of the double slots)
static void SetEffects_RENZFLARE(EFFECT_CONT *ec, int fl, va_list *ap)
{
    ec->dat.uc8[1] = (u_char)fl;
    ec->dat.uc8[0] = 8;
    ec->dat.uc8[2] = (u_char)va_arg(*ap, int);
    ec->pnt[0] = va_arg(*ap, void *);
    ec->pnt[1] = va_arg(*ap, void *);
    ec->fw[0] = (float)va_arg(*ap, double);
    ec->fw[1] = (float)va_arg(*ap, double);
}

// id 9: (alpha) -- FF3 0x001578a8
static void SetEffects_BLACKFILTER(EFFECT_CONT *ec, int fl, va_list *ap)
{
    ec->dat.uc8[1] = (u_char)fl;
    ec->dat.uc8[0] = 9;
    ec->dat.uc8[2] = (u_char)va_arg(*ap, int);
}

// id 10: (col, alpha, in, keep, out) with fl bit 2, else (col, alpha,
// pAlpha2) -- FF3 0x00157780
static void SetEffects_NEGA(EFFECT_CONT *ec, int fl, va_list *ap)
{
    ec->dat.uc8[1] = (u_char)fl;
    ec->dat.uc8[0] = 10;
    ec->dat.uc8[2] = (u_char)va_arg(*ap, int);
    ec->dat.uc8[3] = (u_char)va_arg(*ap, int);
    if (fl & 4)
    {
        ec->in = va_arg(*ap, u_int);
        ec->keep = va_arg(*ap, u_int);
        ec->out = va_arg(*ap, u_int);
        ec->cnt = 0;
        ec->flow = EffFlowStart(ec->in, ec->keep, ec->out);
        return;
    }
    ec->pnt[0] = va_arg(*ap, void *);
}

// ids 11..13: (col, alpha); the kind is the id -- FF3 0x00157760
static void SetEffects_NCONTRAST(EFFECT_CONT *ec, int id, int fl, va_list *ap)
{
    ec->dat.uc8[0] = (u_char)id;
    ec->dat.uc8[1] = (u_char)fl;
    ec->dat.uc8[2] = (u_char)va_arg(*ap, int);
    ec->dat.uc8[3] = (u_char)va_arg(*ap, int);
}

// ids 14 / 15: (type2) -- FF3 0x00158240. GetEffectContBuf has no slot for
// them, so SetEffects never gets here.
static void SetEffects_ENEDMG(EFFECT_CONT *ec, int type, int fl, va_list *ap)
{
    ec->dat.uc8[0] = (type != 0) ? 0xf : 0xe;
    ec->dat.uc8[1] = (u_char)fl;
    ec->dat.uc8[2] = (u_char)va_arg(*ap, int);
}

// id 0x10: (no arguments) -- FF3 0x00157898
static void SetEffects_Kind10(EFFECT_CONT *ec, int fl)
{
    ec->dat.uc8[1] = (u_char)fl;
    ec->dat.uc8[0] = 0x10;
}

// id 0x11: (b2, fw0, b3, fw1, fw2) -- FF3 0x001579c0
// callcheck: dptofp (the (float) casts of the double slots)
static void SetEffects_Kind11(EFFECT_CONT *ec, int fl, va_list *ap)
{
    ec->dat.uc8[1] = (u_char)fl;
    ec->dat.uc8[0] = 0x11;
    ec->dat.uc8[2] = (u_char)va_arg(*ap, int);
    ec->fw[0] = (float)va_arg(*ap, double);
    ec->dat.uc8[3] = (u_char)va_arg(*ap, int);
    ec->fw[1] = (float)va_arg(*ap, double);
    ec->fw[2] = (float)va_arg(*ap, double);
}

// id 0x12: (p) -- FF3 0x00157a48
static void SetEffects_Kind12(EFFECT_CONT *ec, int fl, va_list *ap)
{
    ec->dat.uc8[1] = (u_char)fl;
    ec->dat.uc8[0] = 0x12;
    ec->pnt[0] = va_arg(*ap, void *);
}

// id 0x13: (type, pPos, r, g, b, scale, alpha) -- FF3 0x00157b00
// callcheck: dptofp (the (float) casts of the double slots)
static void SetEffects_HALO(EFFECT_CONT *ec, int fl, va_list *ap)
{
    ec->dat.uc8[1] = (u_char)(fl | 0x80);
    ec->dat.uc8[0] = 0x13;
    ec->dat.uc8[2] = (u_char)va_arg(*ap, int);
    ec->pnt[0] = va_arg(*ap, void *);
    ec->dat.uc8[3] = (u_char)va_arg(*ap, int);
    ec->dat.uc8[4] = (u_char)va_arg(*ap, int);
    ec->dat.uc8[5] = (u_char)va_arg(*ap, int);
    ec->dat.fl32[2] = (float)va_arg(*ap, double);
    ec->dat.uc8[6] = (u_char)va_arg(*ap, int);
}

// id 0x14: (flow, pPos, b2, b3, b4, fw0, fw1, b5, b6, b7, fw2, fw3, max)
// -- FF3 0x00158048 (FF2 FIRE grown; EffCandleFireReq's candle flame)
// callcheck: dptofp (the (float) casts of the double slots)
static void SetEffects_FIRE(EFFECT_CONT *ec, int fl, va_list *ap)
{
    ec->dat.uc8[1] = (u_char)(fl | 0x80);
    ec->dat.uc8[0] = 0x14;
    ec->flow = va_arg(*ap, u_int);
    ec->pnt[0] = va_arg(*ap, void *);
    ec->dat.uc8[2] = (u_char)va_arg(*ap, int);
    ec->dat.uc8[3] = (u_char)va_arg(*ap, int);
    ec->dat.uc8[4] = (u_char)va_arg(*ap, int);
    ec->fw[0] = (float)va_arg(*ap, double);
    ec->fw[1] = (float)va_arg(*ap, double);
    ec->dat.uc8[5] = (u_char)va_arg(*ap, int);
    ec->dat.uc8[6] = (u_char)va_arg(*ap, int);
    ec->dat.uc8[7] = (u_char)va_arg(*ap, int);
    ec->fw[2] = (float)va_arg(*ap, double);
    ec->fw[3] = (float)va_arg(*ap, double);
    ec->max = va_arg(*ap, u_int);
    ec->r = 0;
    ec->cnt = 0;
    ec->in = 0;
    ec->keep = 0;
    ec->a = 0;
    ec->b = 0;
    ec->g = 0;
}

// id 0x16: (type, max, sclx, scly, pWrk, in, keep, out, pDrive, pSpeed,
// pWave, pXYZ, r, g, b) -- FF3 0x00157ba8
// callcheck: dptofp (the (float) casts of the double slots)
static void SetEffects_PDEFORM(EFFECT_CONT *ec, int fl, va_list *ap)
{
    ec->dat.uc8[1] = (u_char)(fl | 0x80);
    ec->dat.uc8[0] = 0x16;
    ec->dat.uc8[2] = (u_char)va_arg(*ap, int);
    ec->max = va_arg(*ap, u_int);
    ec->dat.uc8[4] = 0xff;
    ec->dat.fl32[2] = (float)va_arg(*ap, double);
    ec->dat.fl32[3] = (float)va_arg(*ap, double);
    ec->pnt[0] = va_arg(*ap, void *);
    ec->fw[0] = 0.0f;
    ec->cnt = 0;
    ec->fw[1] = 1.0f;                   // rdata 0x36dc48
    ec->in = va_arg(*ap, u_int);
    ec->keep = va_arg(*ap, u_int);
    ec->out = va_arg(*ap, u_int);
    ec->pnt[1] = va_arg(*ap, void *);
    ec->pnt[2] = va_arg(*ap, void *);
    ec->pnt[4] = va_arg(*ap, void *);
    ec->pnt[5] = va_arg(*ap, void *);
    ec->r = (u_char)va_arg(*ap, int);
    ec->g = (u_char)va_arg(*ap, int);
    ec->b = (u_char)va_arg(*ap, int);
    ec->dat.uc8[5] = 0;
    if (fl & 4)
        ec->flow = EffFlowStart(ec->in, ec->keep, ec->out);
}

// id 0x17: (pTexture, type, scale, count, b2, b3, b4) -- FF3 0x00157d28
// callcheck: dptofp (the (float) casts of the double slots)
static void SetEffects_WATERDROP(EFFECT_CONT *ec, int fl, va_list *ap)
{
    u_int count;

    ec->dat.uc8[1] = (u_char)(fl | 0x80);
    ec->dat.uc8[0] = 0x17;
    ec->pnt[0] = va_arg(*ap, void *);
    ec->dat.uc8[5] = (u_char)va_arg(*ap, int);
    ec->dat.fl32[2] = (float)va_arg(*ap, double);
    ec->dat.fl32[3] = 0.0f;
    count = va_arg(*ap, u_int);
    ec->cnt = count;
    ec->max = count;
    ec->dat.uc8[2] = (u_char)va_arg(*ap, int);
    ec->dat.uc8[3] = (u_char)va_arg(*ap, int);
    ec->dat.uc8[6] = 0;
    ec->dat.uc8[4] = (u_char)va_arg(*ap, int);
}

// id 0x18: (type, pPos, depth) -- FF3 0x00157dd0
static void SetEffects_TORCH2(EFFECT_CONT *ec, int fl, va_list *ap)
{
    ec->dat.uc8[1] = (u_char)(fl | 0x80);
    ec->dat.uc8[0] = 0x18;
    ec->dat.uc8[3] = 0;
    ec->dat.uc8[2] = (u_char)va_arg(*ap, int);
    ec->pnt[1] = nullptr;
    ec->pnt[0] = va_arg(*ap, void *);
    ec->dat.uc8[4] = (u_char)va_arg(*ap, int);
}

// id 0x19: (pPos, type, fw0, fw1); the position is copied -- FF3 0x00157e10
// callcheck: dptofp (the (float) casts of the double slots)
static void SetEffects_SPARK(EFFECT_CONT *ec, int fl, va_list *ap)
{
    ec->dat.uc8[3] = 0;
    ec->dat.uc8[1] = (u_char)(fl | 0x80);
    ec->dat.uc8[0] = 0x19;
    EffPosCopy(ec, va_arg(*ap, float *));
    ec->pnt[0] = ec->Pos;
    ec->pnt[1] = nullptr;
    ec->dat.uc8[2] = (u_char)va_arg(*ap, int);
    ec->fw[0] = (float)va_arg(*ap, double);
    ec->fw[1] = (float)va_arg(*ap, double);
}

// id 0x1a: (pPos, r, g, b, fw0, fw1, alpha); the position is copied
// -- FF3 0x00157ea0
// callcheck: dptofp (the (float) casts of the double slots)
static void SetEffects_TORCH_FREA(EFFECT_CONT *ec, int fl, va_list *ap)
{
    ec->dat.uc8[0] = 0x1a;
    ec->dat.uc8[1] = (u_char)(fl | 0x80);
    EffPosCopy(ec, va_arg(*ap, float *));
    ec->pnt[0] = ec->Pos;
    ec->r = (u_char)va_arg(*ap, int);
    ec->g = (u_char)va_arg(*ap, int);
    ec->b = (u_char)va_arg(*ap, int);
    ec->fw[0] = (float)va_arg(*ap, double);
    ec->fw[1] = (float)va_arg(*ap, double);
    ec->a = (u_char)va_arg(*ap, int);
}

// id 0x1b: (pPos, pRot, mode); position copied, rotation copied into fw;
// mode != 0 starts the envelope at phase 0, else at 1 -- FF3 0x00157f58
static void SetEffects_HAZE(EFFECT_CONT *ec, int fl, va_list *ap)
{
    const float *rot;
    int mode;

    ec->dat.uc8[1] = (u_char)(fl | 0x80);
    ec->dat.uc8[0] = 0x1b;
    EffPosCopy(ec, va_arg(*ap, float *));
    rot = va_arg(*ap, float *);
    mode = va_arg(*ap, int);
    ec->pnt[0] = ec->Pos;
    ec->fw[0] = rot[0];
    ec->fw[1] = rot[1];
    ec->cnt = 0;
    ec->fw[2] = rot[2];
    ec->flow = (mode != 0) ? 0 : 1;
}

// id 0x1c: (pPos, pData, DataNum, Id); position copied -- FF3 0x00158178
static void SetEffects_MANY_CANDLE(EFFECT_CONT *ec, int fl, va_list *ap)
{
    const float *pos;

    ec->dat.uc8[1] = (u_char)(fl | 0x80);
    ec->dat.uc8[0] = 0x1c;
    pos = va_arg(*ap, float *);
    ec->pnt[2] = va_arg(*ap, void *);
    ec->dat.i32[1] = va_arg(*ap, int);
    ec->dat.i32[2] = va_arg(*ap, int);
    EffPosCopy(ec, pos);
    ec->pnt[0] = ec->Pos;
    ec->pnt[1] = nullptr;
}

// id 0x1d: (pPos, pRot, v, no, pAlpha). no == 0 copies the position and
// drops pRot; otherwise pnt[0] points at the caller's position and pnt[2]
// at pRot -- FF3 0x00157a78 (EffKind1DReq / EffKind1DMapReq)
static void SetEffects_Kind1D(EFFECT_CONT *ec, int fl, va_list *ap)
{
    float *pos;
    void *rot;
    int no;

    ec->dat.uc8[1] = (u_char)(fl | 0x80);
    ec->dat.uc8[0] = 0x1d;
    pos = va_arg(*ap, float *);
    rot = va_arg(*ap, void *);
    ec->pnt[1] = nullptr;
    ec->dat.i32[1] = va_arg(*ap, int);
    no = va_arg(*ap, int);
    ec->dat.i32[2] = no;
    ec->pnt[3] = va_arg(*ap, void *);
    if (no == 0)
    {
        EffPosCopy(ec, pos);
        ec->pnt[2] = nullptr;
        ec->pnt[0] = ec->Pos;
    }
    else
    {
        ec->pnt[0] = pos;
        ec->pnt[2] = rot;
    }
    ec->fw[0] = 1.0f;                   // rdata 0x36dc44
}

// id 0x1e: (pPos, v); position copied -- FF3 0x001581d0
// callcheck: dptofp (the (float) casts of the double slots)
static void SetEffects_Kind1E(EFFECT_CONT *ec, int fl, va_list *ap)
{
    const float *pos;

    ec->dat.uc8[1] = (u_char)(fl | 0x80);
    ec->dat.uc8[0] = 0x1e;
    pos = va_arg(*ap, float *);
    ec->dat.fl32[1] = (float)va_arg(*ap, double);
    EffPosCopy(ec, pos);
    ec->pnt[0] = ec->Pos;
    ec->fw[3] = 0.0f;
    ec->pnt[1] = nullptr;
    ec->fw[0] = 0.0f;
    ec->fw[1] = 0.0f;
    ec->fw[2] = 0.0f;
}

// id 0x1f: (type, alpha, rate, pWrk, in, keep, out) -- FF3 0x00158260
// callcheck: dptofp (the (float) casts of the double slots)
static void SetEffects_ENEIN(EFFECT_CONT *ec, int fl, va_list *ap)
{
    ec->dat.uc8[0] = 0x1f;
    ec->dat.uc8[1] = (u_char)(fl | 0x80);
    ec->dat.uc8[2] = (u_char)va_arg(*ap, int);
    ec->dat.uc8[3] = (u_char)va_arg(*ap, int);
    ec->dat.uc8[4] = 0xff;
    ec->dat.fl32[3] = (float)va_arg(*ap, double);
    ec->pnt[0] = va_arg(*ap, void *);
    ec->cnt = 0;
    ec->in = va_arg(*ap, u_int);
    ec->keep = va_arg(*ap, u_int);
    ec->out = va_arg(*ap, u_int);
    ec->max = 100;
    if (fl & 4)
        ec->flow = EffFlowStart(ec->in, ec->keep, ec->out);
}

// FF3 0x00157300
void *SetEffects(int id, int fl, ...)
{
    EFFECT_CONT *ec;
    va_list ap;

    if (EffectIsDrawOn() == 0)
        return nullptr;
    ec = GetEffectContBuf((u_int)id);
    if (ec == nullptr)
        return nullptr;

    va_start(ap, fl);
    switch (id)
    {
    case 0x01: SetEffects_Z_DEP(ec, 0, fl);            break;
    case 0x02: SetEffects_DITHER(ec, fl, &ap);         break;
    case 0x03: SetEffects_BLUR(ec, fl, &ap);           break;
    case 0x04: SetEffects_DEFORM(ec, fl, &ap);         break;
    case 0x05: SetEffects_FOCUS(ec, fl, &ap);          break;
    case 0x06: SetEffects_OVERLAP(ec, fl, &ap);        break;
    case 0x07: SetEffects_FADEFRAME(ec, fl, &ap);      break;
    case 0x08: SetEffects_RENZFLARE(ec, fl, &ap);      break;
    case 0x09: SetEffects_BLACKFILTER(ec, fl, &ap);    break;
    case 0x0a: SetEffects_NEGA(ec, fl, &ap);           break;
    case 0x0b:
    case 0x0c:
    case 0x0d: SetEffects_NCONTRAST(ec, id, fl, &ap);  break;
    case 0x0e: SetEffects_ENEDMG(ec, 0, fl, &ap);      break;
    case 0x0f: SetEffects_ENEDMG(ec, 1, fl, &ap);      break;
    case 0x10: SetEffects_Kind10(ec, fl);              break;
    case 0x11: SetEffects_Kind11(ec, fl, &ap);         break;
    case 0x12: SetEffects_Kind12(ec, fl, &ap);         break;
    case 0x13: SetEffects_HALO(ec, fl, &ap);           break;
    case 0x14: SetEffects_FIRE(ec, fl, &ap);           break;
    case 0x16: SetEffects_PDEFORM(ec, fl, &ap);        break;
    case 0x17: SetEffects_WATERDROP(ec, fl, &ap);      break;
    case 0x18: SetEffects_TORCH2(ec, fl, &ap);         break;
    case 0x19: SetEffects_SPARK(ec, fl, &ap);          break;
    case 0x1a: SetEffects_TORCH_FREA(ec, fl, &ap);     break;
    case 0x1b: SetEffects_HAZE(ec, fl, &ap);           break;
    case 0x1c: SetEffects_MANY_CANDLE(ec, fl, &ap);    break;
    case 0x1d: SetEffects_Kind1D(ec, fl, &ap);         break;
    case 0x1e: SetEffects_Kind1E(ec, fl, &ap);         break;
    case 0x1f: SetEffects_ENEIN(ec, fl, &ap);          break;
    default:                                           break;
    }
    va_end(ap);
    return ec;
}

// FF3 0x00157658
void CutEffects(int id)
{
    ResetEffects(GetEffectContBuf((u_int)id));
}

// FF3 0x00157678
void EffectRelease(void *eff)
{
    EFFECT_CONT *ec = (EFFECT_CONT *)eff;

    if (ec == nullptr || ec->dat.uc8[0] == 0x1c)
        return;
    if (ec->dat.uc8[0] == 0x1d)
    {
        EffKind1DRelease(ec);
        return;
    }
    ResetEffects(ec);
}

// FF3 0x00158348
static void EffectContClear(EFFECT_CONT *ec)
{
    int i;

    if (ec == nullptr)
        return;
    memset(ec, 0, sizeof(*ec));
    for (i = 5; i >= 0; i--)            // the ROM clears the pointers again
        ec->pnt[i] = nullptr;
}

// FF3 0x001583a0
void ResetEffects(void *eff)
{
    if (eff == nullptr)
        return;
    EffectContClear((EFFECT_CONT *)eff);
    ((EFFECT_CONT *)eff)->dat.uc8[0] = 0;
}

// ──────────────────────────────────────────────────────────────────────
// Per-frame passes
// ──────────────────────────────────────────────────────────────────────

// Depth-sorts the live model slots and draws them by kind. A slot without
// fl bit 7 has no world position and sorts to the front (z 0).
// FF3 0x00158428
static void EffectZSort(int PDeformBlurFlg)
{
    static const sceVu0FVECTOR origin = { 0.0f, 0.0f, 0.0f, 1.0f };    // rdata 0x329500
    sceVu0IVECTOR ivec;
    sceVu0FVECTOR vpos;
    sceVu0FMATRIX wlm;
    sceVu0FMATRIX slm;
    GRA3DCAMERA *cam;
    EFFECT_CONT *ecm;
    EFFECT_CONT *ec;
    int num;
    int i;
    int j;
    int n;

    cam = gra3dGetCamera();
    ecm = EffectGetModelCont(0);

    num = 0;
    for (i = 0; i < EFFECT_MODEL_NUM; i++)
    {
        ec = &ecm[i];
        if (ec->dat.uc8[0] == 0)
            continue;
        if ((ec->dat.uc8[1] & 0x80) == 0)
        {
            ec->z = 0;
        }
        else
        {
            memcpy(vpos, ec->pnt[0], sizeof(vpos));
            sceVu0UnitMatrix(wlm);      // the ROM starts from the unit matrix at rdata 0x3293c0
            sceVu0TransMatrix(wlm, wlm, vpos);
            sceVu0MulMatrix(slm, cam->matWorldScreen, wlm);
            sceVu0RotTransPers(ivec, slm, (float *)origin, 0);
            ec->z = (u_int)ivec[2];
        }
        effect_zsort[num++] = i;
    }

    for (i = 0; i < num - 1; i++)
    {
        for (j = i + 1; j < num; j++)
        {
            if (ecm[effect_zsort[j]].z < ecm[effect_zsort[i]].z)
            {
                n = effect_zsort[j];
                effect_zsort[j] = effect_zsort[i];
                effect_zsort[i] = n;
            }
        }
    }

    for (i = 0; i < num; i++)
    {
        if (ecm[effect_zsort[i]].dat.uc8[0] == 0x1e)
            SetEffKind1E(&ecm[effect_zsort[i]]);
    }

    for (i = 0; i < num; i++)
    {
        ec = &ecm[effect_zsort[i]];
        switch (ec->dat.uc8[0])
        {
        case 0x13:
            SetHalo(ec);
            break;
        case 0x14:
            SetFire(ec);
            break;
        case 0x16:
        case 0x1f:
            if (PDeformBlurFlg != 0 || EffectObjPartsDeformBlurCheck(ec) == 0)
                SetPartsDeform(ec);
            break;
        case 0x18:
            SetTorch2(ec);
            break;
        case 0x19:
            SetSpark(ec);
            break;
        case 0x1a:
            SetTorch2BigFrea(ec);
            break;
        case 0x1b:
            SetHaze(ec);
            break;
        case 0x1c:
            SetManyCandle(ec);
            break;
        case 0x1d:
            SetEffKind1D(ec);
            break;
        default:
            break;
        }
    }
}

// The 3D effect set: the effect mains, the sorted model effects, and the
// fixed slots 18 and 8 of ecm.
// FF3 0x002afd10
static void EffectInPhoto(EFFECT_CONT *ecm, int PDeformBlurFlg)
{
    if (ecm[0x12].dat.uc8[0] == 0x12)
        SetMapSky(&ecm[0x12]);
    EffectTorch2BigFreaMain();
    EffectZSort(PDeformBlurFlg);
    EffectButterflyMain();
    EffectButterflyParticleMain();
    EffAreaPartExec();
    EffRainExec();
    EffDropExec();
    EffMapPartInPhoto();
    EffDripMain();
    EffParticleExec();
    EffKind1DListBExec();
    EffectWaterFlowExec();
    EffectThunderLightExec();
    EffectModelAlphaChangeExec();
    EffLanternExec();
    if (ecm[8].dat.uc8[0] == 8)
        SetRenzFlare(&ecm[8]);
}

// FF3 0x00158728
void EffectControl(int no)
{
    EFFECT_CONT *ecm = EffectGetScreenCont(0);

    if (EffectIsDrawOn() == 0)
        return;

    switch (no)
    {
    case 0:
        EffectInPhoto(ecm, 1);
        if (eff_wrk.stop_flg == 0)
        {
            EneDmgMain();
            EneHitEffectMain();
            EffectEndParticleMain();
        }
        if (ecm[1].dat.uc8[0] == 1)
            SetForcusDepth(&ecm[1]);
        if (ecm[3].dat.uc8[0] == 3)
            SetBlur(&ecm[3]);
        RunBlur(&ecm[3]);
        if (ecm[4].dat.uc8[0] == 4)
            SetDeform(&ecm[4]);
        if (ecm[11].dat.uc8[0] == 0xb)
            SetContrast2(&ecm[11]);
        if (ecm[10].dat.uc8[0] == 10)
            SetNega(&ecm[10]);
        if (ecm[13].dat.uc8[0] == 0xd)
            SetContrast3(&ecm[13]);
        if (ecm[5].dat.uc8[0] == 5)
            SetFocus(&ecm[5]);
        RunFocus(&ecm[5]);
        if (ecm[2].dat.uc8[0] == 2)
            SetDither3(&ecm[2]);
        if (ecm[17].dat.uc8[0] == 0x11)
            FUN_0017b970(&ecm[17]);
        if (ecm[9].dat.uc8[0] == 9)
            SetBlackFilter(&ecm[9]);
        if (ecm[12].dat.uc8[0] == 0xc)
            SetContrast2(&ecm[12]);
        ScreenSaverDraw();
        if (ecm[16].dat.uc8[0] == 0x10)
            SetRoomGlow(&ecm[16]);
        break;

    case 1:
        SetEneDmgEffect1_Sub();
        SetEneDmgEffect2_Sub();
        break;

    case 2:
        if (ecm[7].dat.uc8[0] == 7)
            SetFadeFrame(&ecm[7]);
        if (ecm[6].dat.uc8[0] == 6)
            SetOverRap(&ecm[6]);
        ScreenFlashDraw();
        break;

    default:
        break;
    }

    if ((u_int)no < 3)
        effect_control_count[no]++;
}

// ──────────────────────────────────────────────────────────────────────
// Screen effect selection
// ──────────────────────────────────────────────────────────────────────

// FF3 0x00158988
void EffectSetScreenEffectNo(u_int no)
{
    if (no < SCREEN_EFFECT_NUM)
        screen_effect_no = no;
}

// FF3 0x001589a0
int EffectGetScreenEffectNo(void)
{
    return (int)screen_effect_no;
}

// FF3 0x001589a8
SCREEN_EFFECT_PARAMETER *EffectGetScreenEffectParamPtr(u_int no)
{
    if (no >= SCREEN_EFFECT_NUM)
        no = 0;
    return screen_effect_param_ptr[no];
}

// The screen effect in force: the script's number, 5 while the dream-world
// candle is enabled, lit and out; looks 0 / 2 / 5 / 10 have a camera
// variant (the next number) while the player looks through the camera.
// (EffectGetNowScreenEffectParamPtr and EffectNowScreenEffectIs5Or6 inline
// this the same way.)
static int EffectNowScreenEffectNo(void)
{
    CandleControl *candle = GetCandleControl();
    PLYR_WRK *plyr;
    int no = EffectGetScreenEffectNo();

    if (candle->enable != 0 && candle->lit != 0 && candle->out != 0)
        no = 5;
    if (no == 0 || no == 2 || no == 5 || no == 10)
    {
        plyr = PlyrGetCurWrk();
        if ((plyr->flags & PLYR_FLG_800000) != 0 || (plyr->_0e0 & 1) != 0)
            no++;
    }
    return no;
}

// callcheck: EffectGetScreenEffectNo, GetCandleControl (in EffectNowScreenEffectNo)
// FF3 0x001589c8
SCREEN_EFFECT_PARAMETER *EffectGetNowScreenEffectParamPtr(void)
{
    return EffectGetScreenEffectParamPtr((u_int)EffectNowScreenEffectNo());
}

// callcheck: EffectGetScreenEffectNo, GetCandleControl (in EffectNowScreenEffectNo)
// FF3 0x00158aa0
int EffectNowScreenEffectIs5Or6(void)
{
    return (u_int)(EffectNowScreenEffectNo() - 5) < 2;
}

// FF3 0x00158b78
void EffInKeepOutFlowCtrl(EFFECT_CONT *ec)
{
    if (eff_wrk.stop_flg == 0 && ec->flow < 3)
        ec->cnt++;

    switch (ec->flow)
    {
    case 0:
        if (ec->in <= ec->cnt)
        {
            ec->flow = EffFlowStart(0, ec->keep, ec->out);
            ec->cnt = 0;
        }
        break;
    case 1:
        if (ec->keep <= ec->cnt)
        {
            ec->cnt = 0;
            ec->flow = (ec->out != 0) ? 2 : 3;
        }
        break;
    case 2:
        if (ec->out <= ec->cnt)
        {
            ec->cnt = 0;
            ec->flow = 3;
        }
        break;
    case 3:
        ec->flow = 4;
        break;
    default:
        break;
    }
}

// FF3 0x00158c60
int EffectIsSlotNo(int no)
{
    return EffectGetScreenCont((u_int)no)->dat.uc8[0] == (u_int)no;
}

// FF3 0x00158c90
int EffectExecCheck(void *eff, int kind)
{
    EFFECT_CONT *ec;
    int i;

    for (i = 0; i < EFFECT_MODEL_NUM; i++)
    {
        ec = EffectGetModelCont(i);
        if (ec->dat.uc8[0] == (u_int)kind && ec == eff)
            return 1;
    }
    return 0;
}

// FF3 0x00158cf8
int EffectModelKindIsSet(int kind)
{
    EFFECT_CONT *ec = EffectGetModelCont(0);
    int i;

    for (i = 0; i < EFFECT_MODEL_NUM; i++, ec++)
    {
        if (ec->dat.uc8[0] == (u_int)kind)
            return 1;
    }
    return 0;
}

// FF3 0x00158d50
void EffectPhotoPhase(void)
{
    EffectInPhoto(EffectGetModelCont(0), 0);    // the ROM passes the model bank here
}

// FF3 0x00158d78
float EffectHazeLevelMax(void)
{
    EFFECT_CONT *ec = EffectGetModelCont(0);
    float max = 0.0f;
    float v;
    int i;

    for (i = EFFECT_MODEL_NUM - 1; i >= 0; i--, ec++)
    {
        if (ec->dat.uc8[0] == 0x1b)
        {
            v = EffHazeLevel((int)ec->cnt);
            if (max < v)
                max = v;
        }
    }
    return max;
}

// ──────────────────────────────────────────────────────────────────────
// Allocation
// ──────────────────────────────────────────────────────────────────────

// FF3 0x00158df8
void *EFFECT_MALLOC(int size)
{
    if (size > 0)
        return heapCtrlMalloc(GetSystemHeapWrkP(), (u_int)size, 0, 0, 0);
    return nullptr;
}

// FF3 0x00158e50
void EFFECT_FREE(void *p)
{
    if (p != nullptr)
        heapCtrlFree(GetSystemHeapWrkP(), p, 0, 0, 0);
}

// FF3 0x00158ea0
static void EffectControlCountClear(void)
{
    int i;

    for (i = 2; i >= 0; i--)
        effect_control_count[i] = 0;
}
