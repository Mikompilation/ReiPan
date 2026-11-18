/* ==========================================================================
 *  graphics/effect/effect_mapsky.c
 *
 *  The map-sky effect (see effect_mapsky.h), FF3 0x15b348..0x15b658, between
 *  effect_butterfly.c and effect_ene.c. FF3 0x15b650 is MAP_SKY_DATA's empty
 *  destructor, emitted here and registered with atexit by GetMapSkyData
 *  (ingame/map/MapSky.c; nothing to write on the host).
 * ======================================================================== */
#include "graphics/effect/effect_mapsky.h"

#include "ingame/map/MapSky.h"          // GetMapSkyData, MAP_SKY_DATA

// ──────────────────────────────────────────────────────────────────────
// The sky work
// ──────────────────────────────────────────────────────────────────────

// FF3 0x0015b348
void EFF_MAP_SKY_WRK::Init()
{
    GetMapSkyData()->Create();
    ready = 0;
}

// Waits for the dome, then plays and draws it at pos. FF3 0x0015b388
int EFF_MAP_SKY_WRK::Exec(float *pos)
{
    if (ready == 0)
    {
        if (GetMapSkyData()->IsReady() != 0)
            ready = 1;
        if (ready == 0)
            return 0;
    }

    if (GetMapSkyData()->Proc() != 0)
        return 1;

    GetMapSkyData()->Draw(pos);
    return 0;
}

// FF3 0x0015b450
void EFF_MAP_SKY_WRK::Release()
{
    GetMapSkyData()->Release();
}

// FF3 0x0015b480
void EFF_MAP_SKY_WRK::Stop()
{
    GetMapSkyData()->Stop();
}

// FF3 0x0015b4b0
int EFF_MAP_SKY_WRK::IsNotStopping()
{
    return GetMapSkyData()->IsNotStopping();
}

// ──────────────────────────────────────────────────────────────────────
// The effect
// ──────────────────────────────────────────────────────────────────────

// FF3 0x0015b4e0
EFFECT_CONT *EffMapSkyReq(float *pos)
{
    return (EFFECT_CONT *)SetEffects(0x12, 2, pos);
}

// FF3 0x0015b500
// callcheck: EFFECT_FREE (read as a macro); ResetEffects is a tail jump (j) in the ROM

void EffMapSkyRelease(EFFECT_CONT *ec)
{
    EFF_MAP_SKY_WRK *wrk;

    if (ec == nullptr)
        return;

    wrk = (EFF_MAP_SKY_WRK *)ec->pnt[1];
    if (wrk != nullptr)
    {
        wrk->Release();
        EFFECT_FREE(wrk);
        ec->pnt[1] = nullptr;
    }
    ResetEffects(ec);
}

// FF3 0x0015b558
void EffMapSkyStop(EFFECT_CONT *ec)
{
    EFF_MAP_SKY_WRK *wrk;

    if (ec != nullptr && (wrk = (EFF_MAP_SKY_WRK *)ec->pnt[1]) != nullptr)
        wrk->Stop();
}

// FF3 0x0015b588
int EffMapSkyIsStopped(EFFECT_CONT *ec)
{
    EFF_MAP_SKY_WRK *wrk;

    if (ec != nullptr && (wrk = (EFF_MAP_SKY_WRK *)ec->pnt[1]) != nullptr)
        return wrk->IsNotStopping();
    return 1;
}

// Every frame from EffectInPhoto while slot 18 holds kind 0x12: the work on
// the first frame, then the sky at pnt[0]; once it has ended, a request
// with flag bit 0 is released. FF3 0x0015b5c0
// callcheck: EFFECT_MALLOC (read as a macro); EffMapSkyRelease is a tail jump (j) in the ROM

void SetMapSky(EFFECT_CONT *ec)
{
    EFF_MAP_SKY_WRK *wrk = (EFF_MAP_SKY_WRK *)ec->pnt[1];

    if (wrk == nullptr)
    {
        wrk = (EFF_MAP_SKY_WRK *)EFFECT_MALLOC(sizeof(EFF_MAP_SKY_WRK));
        ec->pnt[1] = wrk;
        if (wrk == nullptr)
        {
            ResetEffects(ec);
            // FF3 goes on to Exec on the cleared pnt[1] (a read of EE
            // address 0); the host stops here.
            return;
        }
        wrk->Init();
    }

    if (wrk->Exec((float *)ec->pnt[0]) != 0 && (ec->dat.uc8[1] & 1) != 0)
        EffMapSkyRelease(ec);
}
