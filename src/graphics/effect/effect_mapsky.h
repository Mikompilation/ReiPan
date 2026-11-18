/* ==========================================================================
 *  graphics/effect/effect_mapsky.h
 *
 *  The map-sky effect (FF3 0x15b348..0x15b658, FF3-only; no FF2 file): effect
 *  kind 0x12 in the fixed slot 18, which shows the sky dome of
 *  ingame/map/MapSky.c (MAP_SKY_DATA: DOME_MDL_PK4 playing the shuffled
 *  SCENE91xxM_PSS movies) at a position. The tattoo event
 *  (ingame/tattoo_event.c) requests it at Rei and stops / releases it.
 *  EffectInPhoto runs the slot every frame (SetMapSky).
 *
 *  The slot's pnt[0] is the position the request passed, pnt[1] a 4-byte
 *  work (EFF_MAP_SKY_WRK) EFFECT_MALLOC'd on the first frame.
 * ======================================================================== */
#ifndef _GRAPHICS_EFFECT_EFFECT_MAPSKY_H
#define _GRAPHICS_EFFECT_EFFECT_MAPSKY_H

#include "graphics/effect/effect.h"     // EFFECT_CONT

// The sky work (EFFECT_MALLOC(4), the slot's pnt[1]). Its methods reach the
// one MAP_SKY_DATA through GetMapSkyData (FF3 caches the pointer in sbss
// gp-0xaf0, shared with main.c's GameSystemStart / End).
struct EFF_MAP_SKY_WRK                  /* 0x04 */
{
    void Init();                        // FF3 0x0015b348 (EffMapSkyWrkInit): MapSkyDataCreate, ready 0
    int  Exec(float *pos);              // FF3 0x0015b388 (EffMapSkyWrkExec): 0 while loading / drawing, 1 once the sky has ended
    void Release();                     // FF3 0x0015b450 (EffMapSkyWrkRelease): MapSkyDataRelease
    void Stop();                        // FF3 0x0015b480 (EffMapSkyWrkStop): MapSkyDataStop
    int  IsNotStopping();               // FF3 0x0015b4b0 (EffMapSkyWrkIsNotStopping): MapSkyDataIsNotStopping

    /* 0x00 */ int ready;               // MapSkyDataIsReady has answered 1
};
static_assert(sizeof(EFF_MAP_SKY_WRK) == 0x04, "EFF_MAP_SKY_WRK: EFFECT_MALLOC(4)");

EFFECT_CONT *EffMapSkyReq(float *pos);          // FF3 0x0015b4e0: SetEffects(0x12, 2, pos)
void EffMapSkyRelease(EFFECT_CONT *ec);         // FF3 0x0015b500: frees its sky work and the slot (nullptr: nothing)
void EffMapSkyStop(EFFECT_CONT *ec);            // FF3 0x0015b558: MapSkyDataStop
int  EffMapSkyIsStopped(EFFECT_CONT *ec);       // FF3 0x0015b588: 1 when stopped or without sky work
void SetMapSky(EFFECT_CONT *ec);                // FF3 0x0015b5c0: kind 0x12 (slot 18), EffectInPhoto

#endif /* _GRAPHICS_EFFECT_EFFECT_MAPSKY_H */
