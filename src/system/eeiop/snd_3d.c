/* ==========================================================================
 *  system/eeiop/snd_3d.c
 *
 *  3D sound placement: thirty emitter slots and one listener. The model
 *  (Snd3DCalc) is FF2's:
 *
 *    - distance gives one volume, full within nearest_dist, falling linearly
 *      to vol_min at farthest_dist;
 *    - the angle to the listener's forward axis picks the front or back level
 *      and cross-fades it against side for the far ear;
 *    - the speeds of listener and emitter along the line between them give a
 *      doppler pitch when the emitter moves.
 *
 *  Distances are authored in metres and scaled to world units by
 *  snd3DSet1Meter, hence the *_calc copies next to the environment.
 *
 *  From MioPan system/eeiop/snd_3d.c (FF2's snd_3d.c). FF3 dropped
 *  CheckEnvValue, the listener normalisation check and the VEL status bit (a
 *  source without a velocity gets the zero vector, and a zero velocity means
 *  no doppler), split Snd3DCalc into Snd3DCalcDistVol / Pan / Pitch and added
 *  a replacement hook for the whole model (Snd3DSetCalcFunc, never set).
 * ======================================================================== */
#include "system/eeiop/snd_3d.h"

#include <math.h>                       // sqrtf

#include "sdk/libvu0.h"                 // sceVu0* vector helpers

static SND_3D_ENV snd_3d_env =          /* data 0x333a70 */
{
    0.8f,                               // front
    0.1f,                               // side
    0.7f,                               // back
    0.3f,                               // vol_min
    30.0f,                              // farthest_dist
    2.0f                                // nearest_dist
};

static sceVu0FVECTOR    snd_3d_zero_vec = { 0.0f, 0.0f, 0.0f, 0.0f };   /* data 0x333a60 */
static SND_3D_CALC_FUNC snd_3d_calc_func;               /* sdata gp-0xcf4 */
static float            farthest_dist_calc;             /* sbss gp+0x58c */
static float            nearest_dist_calc;              /* sbss gp+0x590 */
static float            sound_speed;                    /* sbss gp+0x594 */
static float            dist_unit;                      /* sbss gp+0x598 */
static SND_3D_WRK       snd_3d_wrk[SND_3D_WRK_NUM];     /* bss 0x4108c0 */
static SND_3D_LISTENER  snd_3d_listner;                 /* bss 0x410e60 */

static void Snd3DCalc(SND_3D_WRK *s3d);

// PC: the EE's div.s returns +-MAX for x / 0 and the VU multiplies 0 * MAX to
// 0, so scaling a zero vector by 1 / its length leaves it zero there; on the
// host 1 / 0 is inf and 0 * inf is NaN. A zero length scales by 0 instead,
// which gives the EE's results (a zero vector, and 0 from GetLenDirection).
static float snd3DInvLen(float len)
{
    return len == 0.0f ? 0.0f : 1.0f / len;
}

// ──────────────────────────────────────────────────────────────────────────
// Clear every slot; one world unit per metre until the game says otherwise.
// FF3 0x0026eb88; from MioPan snd_3d.c Snd3DInit (no CheckEnvValue).

void Snd3DInit(void)
{
    snd3DSet1Meter(1.0f);

    for (int i = 0; i < SND_3D_WRK_NUM; i++)
    {
        snd_3d_wrk[i].status = 0;
    }
}

// ──────────────────────────────────────────────────────────────────────────
// World units per metre; the distances and the speed of sound (340 m/s)
// follow.
// FF3 0x0026ebd8; from MioPan snd_3d.c snd3DSet1Meter (unchanged).

void snd3DSet1Meter(float unit)
{
    dist_unit          = unit;
    nearest_dist_calc  = snd_3d_env.nearest_dist * unit;
    farthest_dist_calc = snd_3d_env.farthest_dist * unit;
    sound_speed        = unit * 340.0f;
}

// ──────────────────────────────────────────────────────────────────────────
// FF3 0x0026ec20; from MioPan snd_3d.c snd3DSetEnvironment (no CheckEnvValue).

void snd3DSetEnvironment(SND_3D_ENV *env)
{
    snd_3d_env = *env;

    farthest_dist_calc = snd_3d_env.farthest_dist * dist_unit;
    nearest_dist_calc  = snd_3d_env.nearest_dist * dist_unit;
}

// ──────────────────────────────────────────────────────────────────────────
// Place the listener: set->pos, set->dir (a unit vector), top, and set->vel
// (zero when absent). Every slot is recomputed on the next Snd3DMain.
// FF3 0x0026ec78; from MioPan snd_3d.c snd3DSetListner (no normalisation
// check).

void SndSetListener(const SND_3D_SET *set, float *top)
{
    float *vel;

    sceVu0CopyVector(snd_3d_listner.pos, set->pos);
    sceVu0CopyVector(snd_3d_listner.dir, set->dir);
    sceVu0CopyVector(snd_3d_listner.top, top);

    vel = set->vel;
    snd_3d_listner.defer = 1;
    if (vel == nullptr)
        vel = snd_3d_zero_vec;

    sceVu0CopyVector(snd_3d_listner.velocity, vel);
}

// ──────────────────────────────────────────────────────────────────────────
// FF3 0x0026ecf8; from MioPan snd_3d.c Snd3DFreeWrk (unchanged).

void Snd3DFreeWrk(SND_3D_WRK *s3d)
{
    s3d->status &= ~SND_3D_ST_USE;
}

// ──────────────────────────────────────────────────────────────────────────
// Take a free slot for a source and compute it at once.
// FF3 0x0026ed10; from MioPan snd_3d.c Snd3DCreateWrk (unchanged).

SND_3D_WRK *Snd3DCreateWrk(const SND_3D_SET *set)
{
    for (int i = 0; i < SND_3D_WRK_NUM; i++)
    {
        SND_3D_WRK *s3d = &snd_3d_wrk[i];

        if ((s3d->status & SND_3D_ST_USE) == 0)
        {
            s3d->status |= SND_3D_ST_USE;
            snd3DSetSET(s3d, set);
            Snd3DCalc(s3d);
            return s3d;
        }
    }

    return nullptr;
}

// ──────────────────────────────────────────────────────────────────────────
// Re-seat a slot on a new SND_3D_SET (dir is ignored: a point source).
// FF3 0x0026ed80; from MioPan snd_3d.c snd3DSetSET. FF3 copies the zero
// vector for a missing velocity instead of keeping a VEL bit.

void snd3DSetSET(SND_3D_WRK *s3d, const SND_3D_SET *set)
{
    float *vel;

    sceVu0CopyVector(s3d->pos, set->pos);

    vel = set->vel;
    if (vel == nullptr)
        vel = snd_3d_zero_vec;

    sceVu0CopyVector(s3d->velocity, vel);
    s3d->status |= SND_3D_ST_CALC;
}

// ──────────────────────────────────────────────────────────────────────────
// FF3 0x0026ede0; from MioPan snd_3d.c Snd3DSetPosition (unchanged).

void Snd3DSetPosition(SND_3D_WRK *s3d, float *pos)
{
    sceVu0CopyVector(s3d->pos, pos);
    s3d->status |= SND_3D_ST_CALC;
}

// ──────────────────────────────────────────────────────────────────────────
// The slot's last result, for SndCalcValue.
// FF3 0x0026ee10; from MioPan snd_3d.c Snd3DGetVal (unchanged).

void Snd3DGetVal(void *hndl, VOLSET *volset, short *pitch)
{
    SND_3D_WRK *s3d = (SND_3D_WRK *)hndl;

    volset->l = (short)s3d->voll;
    volset->r = (short)s3d->volr;
    *pitch    = (short)s3d->pitch;
}

// ──────────────────────────────────────────────────────────────────────────
// The signed length of velocity along unit_dir.
// FF3 0x0026ee30; from MioPan snd_3d.c GetLenDirection (unchanged).

float GetLenDirection(float *velocity, float *unit_dir)
{
    sceVu0FVECTOR vel_unit;
    float         vel_len;

    vel_len = sceVu0InnerProduct(velocity, velocity);
    sceVu0ScaleVector(vel_unit, velocity, snd3DInvLen(sqrtf(vel_len)));   // PC: zero length
    return sqrtf(vel_len) * sceVu0InnerProduct(unit_dir, vel_unit);
}

// ──────────────────────────────────────────────────────────────────────────
// Distance -> volume: 16383 within nearest_dist, falling linearly to
// 16383 * vol_min at farthest_dist and beyond.
// FF3 0x0026eeb0 (split out of FF2's Snd3DCalc).

static float Snd3DCalcDistVol(float dist)
{
    float rate;
    float temp_dist;

    if (farthest_dist_calc <= dist)
    {
        rate = snd_3d_env.vol_min;
    }
    else
    {
        temp_dist = dist - nearest_dist_calc;
        if (temp_dist <= 0.0f)
        {
            rate = 1.0f;
        }
        else
        {
            rate = 1.0f - temp_dist / (farthest_dist_calc - nearest_dist_calc);
            if (rate < snd_3d_env.vol_min)
                rate = snd_3d_env.vol_min;
        }
    }

    return 16383.0f * rate;
}

// ──────────────────────────────────────────────────────────────────────────
// Split vol over the two ears. top x dir points to the listener's left, so a
// negative dot with the source direction puts the source on the right. The
// near ear keeps the whole off-axis term, the far ear side of it; both keep
// the front (or back) term. val is &s3d->voll: voll, volr.
// FF3 0x0026ef40 (split out of FF2's Snd3DCalc).

static void Snd3DCalcPan(float vol, int *val, float *unit, SND_3D_LISTENER *l)
{
    sceVu0FVECTOR rightvec;
    float         side_dot;
    float         cos;
    float         revcos;
    float         temp;

    sceVu0OuterProduct(rightvec, l->top, l->dir);
    side_dot = sceVu0InnerProduct(rightvec, unit);

    cos  = sceVu0InnerProduct(unit, l->dir);
    temp = snd_3d_env.front;
    if (cos < 0.0f)
    {
        cos  = -cos;
        temp = snd_3d_env.back;
    }
    revcos = 1.0f - cos;

    if (side_dot >= 0.0f)
    {
        val[0] = (short)(int)(vol * (revcos + cos * temp));
        val[1] = (short)(int)(vol * (revcos * snd_3d_env.side + cos * temp));
    }
    else
    {
        val[1] = (short)(int)(vol * (revcos + cos * temp));
        val[0] = (short)(int)(vol * (revcos * snd_3d_env.side + cos * temp));
    }
}

// ──────────────────────────────────────────────────────────────────────────
// Doppler: 0x1000 * (c + listener speed) / (c + source speed) along unit,
// or 0x1000 when the source does not move.
// FF3 0x0026f0b0 (split out of FF2's Snd3DCalc).

static float Snd3DCalcPitch(SND_3D_WRK *s3d, float *unit, SND_3D_LISTENER *l)
{
    float listner_vel;
    float vel;

    if (s3d->velocity[0] == 0.0f && s3d->velocity[1] == 0.0f && s3d->velocity[2] == 0.0f)
        return 4096.0f;

    listner_vel = GetLenDirection(l->velocity, unit);
    vel         = GetLenDirection(s3d->velocity, unit);
    return (float)(int)(((sound_speed + listner_vel) * 4096.0f) / (sound_speed + vel));
}

// ──────────────────────────────────────────────────────────────────────────
// FF3 0x0026f178 (FF3 only; nothing in the ROM calls it).

void Snd3DSetCalcFunc(SND_3D_CALC_FUNC func)
{
    snd_3d_calc_func = func;
}

// ──────────────────────────────────────────────────────────────────────────
// Recompute one slot: volume pair and pitch, or the hook's answer.
// FF3 0x0026f180; from MioPan snd_3d.c Snd3DCalc, with the model in the
// three helpers above.

static void Snd3DCalc(SND_3D_WRK *s3d)
{
    sceVu0FVECTOR unit;
    float         dist;
    float         vol;

    if (snd_3d_calc_func == nullptr)
    {
        sceVu0SubVector(unit, s3d->pos, snd_3d_listner.pos);
        dist = sceVu0InnerProduct(unit, unit);
        sceVu0ScaleVector(unit, unit, snd3DInvLen(sqrtf(dist)));      // PC: zero length
        vol = Snd3DCalcDistVol(sqrtf(dist));
        Snd3DCalcPan(vol, &s3d->voll, unit, &snd_3d_listner);
        s3d->pitch = (int)Snd3DCalcPitch(s3d, unit, &snd_3d_listner);
    }
    else
    {
        snd_3d_calc_func(&s3d->voll, s3d, &snd_3d_listner, 0);
    }

    s3d->status &= ~SND_3D_ST_CALC;
}

// ──────────────────────────────────────────────────────────────────────────
// Once a frame: every live slot after the listener moved, otherwise only the
// slots that moved.
// FF3 0x0026f288; from MioPan snd_3d.c Snd3DMain (unchanged).

void Snd3DMain(void)
{
    if (snd_3d_listner.defer == 0)
    {
        for (int i = 0; i < SND_3D_WRK_NUM; i++)
        {
            if ((snd_3d_wrk[i].status & (SND_3D_ST_USE | SND_3D_ST_CALC)) ==
                (SND_3D_ST_USE | SND_3D_ST_CALC))
            {
                Snd3DCalc(&snd_3d_wrk[i]);
            }
        }
    }
    else
    {
        for (int i = 0; i < SND_3D_WRK_NUM; i++)
        {
            if (snd_3d_wrk[i].status & SND_3D_ST_USE)
                Snd3DCalc(&snd_3d_wrk[i]);
        }

        snd_3d_listner.defer = 0;
    }
}
