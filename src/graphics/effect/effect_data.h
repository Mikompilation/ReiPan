/* ==========================================================================
 *  graphics/effect/effect_data.h
 *
 *  FF3-only effect data module between effect.c and effect_butterfly.c
 *  (FF3 0x158ed0..0x159188, no FF2 counterpart): the shared effect texture
 *  page VRAM_TEX_PK4 (loaded and sent to GS memory once at boot), the saved
 *  effect state (the monochrome / monotone-draw switches and the hand-light
 *  effect flag, save_game_data entry 46) and the butterfly model holder.
 * ======================================================================== */
#ifndef _GRAPHICS_EFFECT_EFFECT_DATA_H
#define _GRAPHICS_EFFECT_EFFECT_DATA_H

#include <stddef.h>                     // offsetof

// The effect state a save keeps (bss 0x398960, 0xc bytes; ScreenEffectParamSaveGet).
struct SCREEN_EFFECT_SAVE               /* 0xc */
{
    /* 0x00 */ int monochro_mode;       // eff_wrk.monochro_mode != 0 at save time
    /* 0x04 */ int monotone_draw;       // gra3dIsMonotoneDrawEnable() at save time
    /* 0x08 */ int handlight_disp;      // the hand-light effect flag (SET_HANDLIGHTEFFECT_DISP)
};
static_assert(sizeof(SCREEN_EFFECT_SAVE) == 0xc, "SCREEN_EFFECT_SAVE is 0xc bytes on the PS2");
static_assert(offsetof(SCREEN_EFFECT_SAVE, monotone_draw) == 0x04, "SCREEN_EFFECT_SAVE.monotone_draw");
static_assert(offsetof(SCREEN_EFFECT_SAVE, handlight_disp) == 0x08, "SCREEN_EFFECT_SAVE.handlight_disp");

extern SCREEN_EFFECT_SAVE scr_eff_save; // bss 0x398960

void ScreenEffectDataLoadReq(void);     // FF3 0x00158ed0: VRAM_TEX_PK4 onto the system heap (one_Boot_Load)
void ScreenEffectDataSetup(void);       // FF3 0x00158f38: its textures to GS memory
void ScreenEffectDataFree(void);        // FF3 0x00158f70: the heap copy freed
void ScreenEffectParamInit(void);       // FF3 0x00158fb8: colour on, hand-light effect on (IngameInit)
void MonotoneSaveStore(void);           // FF3 0x00158ff8: the switches into scr_eff_save (IngameSaveInit)
void MonotoneLoadRestore(void);         // FF3 0x00159030: the switches back from scr_eff_save (IngameLoadRestore)
int  ScreenEffectParamSaveGet(void **addr);     // FF3 0x00159070: {&scr_eff_save, 0xc}
int  HandLightEffectIsDisp(void);       // FF3 0x00159088
void HandLightEffectSetDisp(int on);    // FF3 0x00159098

void EffButterflyMdlLoad(void);         // FF3 0x001590c8: InitEffects
void EffButterflyMdlRelease(void);      // FF3 0x00159110: EffectsReleaseAll
int  EffButterflyMdlIsLoaded(void);     // FF3 0x00159148: CheckSetEffectsOK

#endif /* _GRAPHICS_EFFECT_EFFECT_DATA_H */
