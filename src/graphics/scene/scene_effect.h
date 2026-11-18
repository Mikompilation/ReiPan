/* ==========================================================================
 *  graphics/scene/scene_effect.h
 *
 *  Scene effects (FF2 scene_effect.c; FF3 0x254018..0x256da8): the
 *  interpreter of a scene's effect script (the scene's third file,
 *  SceneEffectDataFileNoGet). Engine scenes run it per scene frame
 *  (SceneEffectMain), movies per movie frame (SceneMovieEffectMain).
 * ======================================================================== */
#ifndef _GRAPHICS_SCENE_SCENE_EFFECT_H
#define _GRAPHICS_SCENE_SCENE_EFFECT_H

struct SCENE_CTRL;                      // graphics/scene/scene.h
struct SCN_MDL;

// Every scene-effect control cleared; glow_clear nonzero also clears the
// room glow (EffRoomGlowClear), then SetEffects(0x10, 2).
void SceneEffectInit(int glow_clear);   // FF3 0x00254018
// Runs the script (data, nullptr: nothing) for every scene frame since the
// last call (sc->fod_ctrl.now_frame), then the per-frame controls.
void SceneEffectMain(SCENE_CTRL *sc, void *data);   // FF3 0x00254098
void SceneEffectEnd(void);              // FF3 0x00254180
// The lens flare record's flag (record 13 on, the OFF record 13 off).
int  SceneEffectLenzFlareFlgGet(void);  // FF3 0x00256030
// The flare on a man model's lantern: efct[0]'s lens point and aim, the
// level the model's alpha / 128 (the man model draw, 0x252630).
void SceneEffectLenzFlareExec(SCN_MDL *mdl);    // FF3 0x00256040
// Runs the effect script data (the scene's file + 2) up to movie frame
// frame (x 1.2 on PAL); nothing when data is null.
void SceneMovieEffectMain(int frame, void *data);   // FF3 0x00256a20

#endif /* _GRAPHICS_SCENE_SCENE_EFFECT_H */
