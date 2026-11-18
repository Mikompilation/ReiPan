/* ==========================================================================
 *  graphics/effect/effect_data.c
 *
 *  The effect data module (see effect_data.h). FF3-only,
 *  FF3 0x158ed0..0x159188.
 *
 *  0x1590a8 is a compiler atexit thunk (CandleControl's destructor 0x2ad380
 *  on GetCandleControl's static 0x41e540, this file's copy; item.c has the
 *  same one); the host runs the static's destructor itself.
 * ======================================================================== */
#include "graphics/effect/effect_data.h"

#include "common/heapctrl.h"            // heapCtrlMalloc, heapCtrlFree
#include "graphics/game_model.h"        // GameMotionModel
#include "graphics/graph2d/tim2.h"      // PK4SendVram
#include "graphics/graph3d/gra3d.h"     // gra3dIsMonotoneDrawEnable, gra3dMonotoneDrawEnable
#include "graphics/effect/effect.h"     // eff_wrk
#include "graphics/effect/effect_oth.h" // EffectMonochromeSet
#include "system/eeiop/cd_file_dat.h"   // VRAM_TEX_PK4, BUTTERFLY_MDL_PK4, BUTTERFLY_MOT_PK4
#include "system/eeiop/cddat.h"         // GetFileSize
#include "system/os/eecdvd.h"           // LoadReq
#include "system/os/sys_heap.h"         // GetSystemHeapWrkP

SCREEN_EFFECT_SAVE scr_eff_save;                // bss 0x398960

static void *vram_tex_pk4 = nullptr;            // sdata gp-0x58a4 (0x36dc4c)
static GameMotionModel *butterfly_mdl;          // sbss gp-0x5d4 (0x372f1c)

// ──────────────────────────────────────────────────────────────────────
// The effect texture page (VRAM_TEX_PK4: the effdat textures, e.g. the
// candle flame page TBP 0x3daa). one_Boot_Load requests it, sends it once it
// is in, then frees the heap copy: the textures stay in GS memory.
// ──────────────────────────────────────────────────────────────────────

// FF3 0x00158ed0
void ScreenEffectDataLoadReq(void)
{
    vram_tex_pk4 = heapCtrlMalloc(GetSystemHeapWrkP(), GetFileSize(VRAM_TEX_PK4), 0, 0, 0);
    if (vram_tex_pk4 != nullptr)
        LoadReq(VRAM_TEX_PK4, vram_tex_pk4);
}

// FF3 0x00158f38
void ScreenEffectDataSetup(void)
{
    if (vram_tex_pk4 != nullptr)
        PK4SendVram(vram_tex_pk4, 0, -1, -1);
}

// FF3 0x00158f70
void ScreenEffectDataFree(void)
{
    if (vram_tex_pk4 != nullptr)
    {
        heapCtrlFree(GetSystemHeapWrkP(), vram_tex_pk4, 0, 0, 0);
        vram_tex_pk4 = nullptr;
    }
}

// ──────────────────────────────────────────────────────────────────────
// The saved effect state.
// ──────────────────────────────────────────────────────────────────────

// A new game: colour (monochrome and monotone draw off, no re-prelight) and
// the hand-light effect shown.
// FF3 0x00158fb8
void ScreenEffectParamInit(void)
{
    scr_eff_save.monochro_mode = 0;
    scr_eff_save.monotone_draw = 0;
    EffectMonochromeSet(0, 0);
    scr_eff_save.handlight_disp = 1;
}

// FF3 0x00158ff8
void MonotoneSaveStore(void)
{
    scr_eff_save.monochro_mode = eff_wrk.monochro_mode;
    scr_eff_save.monotone_draw = gra3dIsMonotoneDrawEnable();
}

// FF3 0x00159030
void MonotoneLoadRestore(void)
{
    eff_wrk.monochro_mode = (scr_eff_save.monochro_mode != 0);
    gra3dMonotoneDrawEnable(scr_eff_save.monotone_draw);
}

// FF3 0x00159070
int ScreenEffectParamSaveGet(void **addr)
{
    *addr = &scr_eff_save;
    return sizeof(scr_eff_save);
}

// FF3 0x00159088
int HandLightEffectIsDisp(void)
{
    return scr_eff_save.handlight_disp;
}

// FF3 0x00159098
void HandLightEffectSetDisp(int on)
{
    scr_eff_save.handlight_disp = on;
}

// ──────────────────────────────────────────────────────────────────────
// The butterfly model (BUTTERFLY_MDL_PK4 with its motion BUTTERFLY_MOT_PK4).
// ──────────────────────────────────────────────────────────────────────

// FF3 0x001590c8
// callcheck: operator_new, GameMotionModelCtor (new GameMotionModel)
void EffButterflyMdlLoad(void)
{
    GameMotionModel *mdl = new GameMotionModel();
    mdl->Load(BUTTERFLY_MDL_PK4, BUTTERFLY_MOT_PK4, -1);
    butterfly_mdl = mdl;
}

// FF3 0x00159110
void EffButterflyMdlRelease(void)
{
    if (butterfly_mdl != nullptr)
        delete butterfly_mdl;           // vtable +0x0c, the deleting destructor
    butterfly_mdl = nullptr;
}

// FF3 0x00159148
int EffButterflyMdlIsLoaded(void)
{
    if (butterfly_mdl != nullptr && butterfly_mdl->IsLoaded() != 0)
        return 1;
    return 0;
}
