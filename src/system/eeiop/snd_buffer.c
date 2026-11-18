/* ==========================================================================
 *  system/eeiop/snd_buffer.c
 *
 *  The SPU voice pool: 48 slots, 24 per SPU2 core, indexed core * 24 + voice
 *  (the voice number comes from spu_voice.c). Callers hold the packed id
 *  (wrk_no << 16) | play_id; play_id is bumped on every claim, so a stale id
 *  resolves to nothing once its voice is recycled and every entry point
 *  quietly does nothing. SndBufPlayMain is the per-frame pump: it retires
 *  finished voices, steps the volume and pitch ramps, runs the fade-stop
 *  handshake and sends changes to the IOP (REQ_SB_SETVOL / _SETPITCH).
 *
 *  From MioPan system/eeiop/snd_buffer.c (FF2's). FF3 drops the volume /
 *  pitch range warnings, the "Cannot Get Voice" and "Not 3D Sound" prints,
 *  SndBufferPrintStatus and SndBufSet3D; it splits SndBufPause / Restart into
 *  slot-level Sub functions and adds SndBufAll{Pause,Restart}LoopSnd.
 *  FF3 0x269fa0..0x26ad2c.
 * ======================================================================== */
#include "system/eeiop/snd_buffer.h"

#include "system/eeiop/ee_iop.h"        // iopCommandRegister, CheckEndPointThrough
#include "system/eeiop/snd.h"           // SndCalcValue, SndGetFrameAddVol
#include "system/eeiop/snd_3d.h"        // Snd3DFreeWrk, Snd3DSetPosition
#include "system/eeiop/spu_voice.h"     // GetSPUVoiceCore, FreeSPUVoiceCore

static SND_BUF_PLAYER snd_buf_player[SND_BUF_PLAYER_MAX];  /* bss 0x40e2c0 */

static void SndBufRelease(SND_BUF_PLAYER *sbp);
static void SndBufStopSub(SND_BUF_PLAYER *sbp);
static void SndBufStopSub2(SND_BUF_PLAYER *sbp);
static void SndBufPauseSub(SND_BUF_PLAYER *sbp);
static void SndBufRestartSub(SND_BUF_PLAYER *sbp);

// ──────────────────────────────────────────────────────────────────────────
// FF3 0x00269fa0; FF2 SndBufInit (unchanged).

void SndBufInit(void)
{
    for (int i = 0; i < SND_BUF_PLAYER_MAX; i++)
    {
        snd_buf_player[i].use = 0;
    }
}

// ──────────────────────────────────────────────────────────────────────────
// FF3 0x00269fe0; FF2 SndBufAllStop (unchanged).

void SndBufAllStop(void)
{
    for (int i = 0; i < SND_BUF_PLAYER_MAX; i++)
    {
        if (snd_buf_player[i].use)
        {
            SndBufStopSub(&snd_buf_player[i]);
        }
    }
}

// ──────────────────────────────────────────────────────────────────────────
// Hand the voice back to spu_voice.c; the 3D handle goes with it only when
// SndBufPlay was told to own it (s3d_free).
// FF3 0x0026a048; FF2 SndBufRelease (unchanged).

static void SndBufRelease(SND_BUF_PLAYER *sbp)
{
    if (sbp->s3d != nullptr && sbp->s3d_free)
    {
        Snd3DFreeWrk(sbp->s3d);
    }

    sbp->use = 0;

    FreeSPUVoiceCore(sbp->p.attr.core, sbp->p.voice_no);
}

// ──────────────────────────────────────────────────────────────────────────
// Looping voices are stopped (they cannot end by themselves), one-shots are
// released outright.
// FF3 0x0026a0b8; FF2 SndBufAllStopLoopSnd (unchanged).

void SndBufAllStopLoopSnd(void)
{
    for (int i = 0; i < SND_BUF_PLAYER_MAX; i++)
    {
        if (snd_buf_player[i].use)
        {
            if (snd_buf_player[i].p.attr.loop)
            {
                SndBufStopSub(&snd_buf_player[i]);
            }
            else
            {
                SndBufRelease(&snd_buf_player[i]);
            }
        }
    }
}

// ──────────────────────────────────────────────────────────────────────────
// Pause every looping voice (OutgameBgmPlay, the save point, kai_pzl).
// FF3 0x0026a148; FF3-only.

void SndBufAllPauseLoopSnd(void)
{
    for (int i = 0; i < SND_BUF_PLAYER_MAX; i++)
    {
        if (snd_buf_player[i].use && snd_buf_player[i].p.attr.loop)
        {
            SndBufPauseSub(&snd_buf_player[i]);
        }
    }
}

// ──────────────────────────────────────────────────────────────────────────
// Restart the looping voices SndBufAllPauseLoopSnd paused. No caller found.
// FF3 0x0026a1c8; FF3-only.

void SndBufAllRestartLoopSnd(void)
{
    for (int i = 0; i < SND_BUF_PLAYER_MAX; i++)
    {
        if (snd_buf_player[i].use && snd_buf_player[i].p.attr.loop)
        {
            SndBufRestartSub(&snd_buf_player[i]);
        }
    }
}

// ──────────────────────────────────────────────────────────────────────────
// A voice already fading out is left alone. The pause flag is not set (the
// ROM's, as in FF2), so SndBufPlayMain keeps ramping the paused voices.
// FF3 0x0026a248; FF2 SndBufAllPause (unchanged).

void SndBufAllPause(void)
{
    SOUND_BUF_VOICE pause;

    for (int i = 0; i < SND_BUF_PLAYER_MAX; i++)
    {
        if (snd_buf_player[i].use && snd_buf_player[i].fadestop == FADE_STOP_NONE)
        {
            pause.voice_no = snd_buf_player[i].p.voice_no;
            pause.core     = snd_buf_player[i].p.attr.core;

            iopCommandRegister(REQ_SB_PAUSE, &pause, sizeof(pause));
        }
    }
}

// ──────────────────────────────────────────────────────────────────────────
// FF3 0x0026a2d8; FF2 SndBufAllRestart (unchanged).

void SndBufAllRestart(void)
{
    SOUND_BUF_VOICE restart;

    for (int i = 0; i < SND_BUF_PLAYER_MAX; i++)
    {
        if (snd_buf_player[i].use && snd_buf_player[i].fadestop == FADE_STOP_NONE)
        {
            restart.voice_no = snd_buf_player[i].p.voice_no;
            restart.core     = snd_buf_player[i].p.attr.core;

            iopCommandRegister(REQ_SB_RESTART, &restart, sizeof(restart));
        }
    }
}

// ──────────────────────────────────────────────────────────────────────────
// The slot of a live id, or nullptr. The compare is short play_id against
// id & 0xffff, so a slot whose play_id has wrapped negative never resolves
// (the ROM's, as in FF2).
// FF3 0x0026a368; FF2 GetSndBufPlayerFromID (unchanged).

static SND_BUF_PLAYER *GetSndBufPlayerFromID(int id)
{
    int wrk_id  = id >> 16;
    int play_id = id & 0xffff;

    // PC: the ROM only checks the upper bound; a negative id would index
    // before the array.
    if (wrk_id < 0)
    {
        return nullptr;
    }

    if (wrk_id < SND_BUF_PLAYER_MAX)
    {
        SND_BUF_PLAYER *sbp = &snd_buf_player[wrk_id];

        if (sbp->use && sbp->play_id == play_id)
        {
            return sbp;
        }
    }

    return nullptr;
}

// ──────────────────────────────────────────────────────────────────────────
// FF3 0x0026a3b8; FF2 SndBufGetID (unchanged).

static int SndBufGetID(int wrk_no, int play_id)
{
    return (wrk_no << 16) | (play_id & 0xffff);
}

// ──────────────────────────────────────────────────────────────────────────
// Claim a voice on core and start it. The first SndCalcValue uses the target
// volume even when fading in; SndBufPlayMain corrects it the next frame.
// A rejected play returns the unused slot immediately; nothing reached the IOP.
// FF3 0x0026a3d0; FF2 SndBufPlay without the warnings and prints.

int SndBufPlay(int adrs, int core, int effect, int vol, int bvol, int pitch,
               int bpitch, int pan, int fade_time, int loop, int type,
               void *s3d, int s3d_free, int adsr1, int adsr2,
               int loopstart, int loopend)
{
    SND_BUF_PLAYER *sbp;
    VOICE_LOOP_SET  slp;
    int             voice_no;
    int             wrk_no;

    voice_no = GetSPUVoiceCore(core);
    if (voice_no == -1)
    {
        return CSND_BUF_PLAY_NO_ID;
    }

    wrk_no = core * VOICES_PER_SPU2_CORE + voice_no;
    sbp    = &snd_buf_player[wrk_no];

    sbp->p.voice_no = (char)voice_no;
    sbp->p.adrs     = adrs;
    sbp->use        = 1;
    sbp->p.adsr1    = adsr1;
    sbp->fadestop   = FADE_STOP_NONE;
    sbp->p.adsr2    = adsr2;

    sbp->bpitch       = bpitch;
    sbp->target_pitch = pitch;
    sbp->s3d          = (SND_3D_WRK *)s3d;
    sbp->s3d_free     = s3d_free;
    sbp->pause        = 0;
    sbp->pan          = pan;
    sbp->pitch        = pitch;
    sbp->pspd         = 0;
    sbp->cnt          = 0;

    sbp->bvol       = bvol;
    sbp->target_vol = vol;
    if (fade_time == 0)
    {
        sbp->vol = vol;
    }
    else
    {
        sbp->vol = 0;
        sbp->spd = SndGetFrameAddVol(vol, 0, fade_time);
    }

    SndCalcValue(vol, pan, bvol, sbp->pitch, sbp->bpitch, type, s3d,
                 &sbp->p.vol, &sbp->p.pitch, 1.0f);

    sbp->loopend_next = loopend + 0x10;

    sbp->p.attr.core   = core;
    sbp->p.attr.effect = effect;
    sbp->p.attr.loop   = loop;
    sbp->p.attr.s3d    = (s3d != nullptr);
    sbp->p.attr.type   = type;

    if (iopCommandRegister(REQ_SB_PLAY, &sbp->p, sizeof(SOUND_BUF_PLAY)) == 0)
    {
        FreeSPUVoiceCore(core, voice_no);
        sbp->use = 0;
        // Ownership stays with the caller, which may retry on the other core.
        sbp->s3d = nullptr;
        sbp->s3d_free = 0;
        return CSND_BUF_PLAY_NO_ID;
    }

    sbp->play_id++;

    // REQ_SB_PLAY has no loop point; a looping sample gets it separately.
    if (loop && loopstart > 0)
    {
        slp.voice_no  = sbp->p.voice_no;
        slp.core      = sbp->p.attr.core;
        slp.loop_adrs = loopstart;

        iopCommandRegister(REQ_VOICE_LOOP_SET, &slp, sizeof(slp));
    }

    return SndBufGetID(wrk_no, sbp->play_id);
}

// ──────────────────────────────────────────────────────────────────────────
// Per frame (from SndMain): retire voices past their end point (not in the
// first ten frames, before the IOP has started them), cut stopped loops,
// step the pitch and volume ramps, run the fade-stop handshake and send
// what changed. The ramps snap to the target when a step crosses it (the
// product of the two distances goes non-positive) or the step is 0.
// FF3 0x0026a690; FF2 SndBufPlayMain (unchanged).

void SndBufPlayMain(void)
{
    SOUND_BUF_SETVOL   set_vol;
    SOUND_BUF_SETPITCH set_pitch;
    VOLSET             volset;
    short              pitch;

    for (int i = 0; i < SND_BUF_PLAYER_MAX; i++)
    {
        SND_BUF_PLAYER *sbp = &snd_buf_player[i];

        if (!sbp->use || sbp->pause)
        {
            continue;
        }

        if (sbp->cnt++ > 10 && CheckEndPointThrough(sbp->p.attr.core, sbp->p.voice_no))
        {
            SndBufRelease(sbp);
            continue;
        }

        if (sbp->p.attr.loop && sbp->fadestop == FADE_STOP_WAIT)
        {
            SndBufStopSub2(sbp);
            continue;
        }

        if (sbp->target_pitch != sbp->pitch)
        {
            int new_pitch = sbp->pitch + sbp->pspd;

            if ((sbp->target_pitch - sbp->pitch) * (sbp->target_pitch - new_pitch) <= 0)
            {
                sbp->pitch = sbp->target_pitch;
            }
            else if (sbp->pspd == 0)
            {
                sbp->pitch = sbp->target_pitch;
            }
            else
            {
                sbp->pitch = new_pitch;
            }
        }

        if (sbp->target_vol != sbp->vol)
        {
            int new_vol = sbp->vol + sbp->spd;

            if ((sbp->target_vol - sbp->vol) * (sbp->target_vol - new_vol) <= 0)
            {
                sbp->vol = sbp->target_vol;
            }
            else if (sbp->spd == 0)
            {
                sbp->vol = sbp->target_vol;
            }
            else
            {
                sbp->vol = new_vol;
            }
        }

        // The ramp landing only promotes the request: the voice gets one
        // last pass at its final volume and is stopped the frame after.
        if (sbp->fadestop == FADE_STOP_REQ && sbp->vol == sbp->target_vol)
        {
            sbp->fadestop = FADE_STOP_END;
        }
        else if (sbp->fadestop == FADE_STOP_END)
        {
            SndBufStopSub(sbp);
            continue;
        }

        SndCalcValue(sbp->vol, sbp->pan, sbp->bvol, sbp->pitch, sbp->bpitch,
                     sbp->p.attr.type, sbp->s3d, &volset, &pitch, 1.0f);

        if (volset.l != sbp->p.vol.l || volset.r != sbp->p.vol.r)
        {
            sbp->p.vol = volset;

            set_vol.vol      = volset;
            set_vol.voice_no = sbp->p.voice_no;
            set_vol.core     = sbp->p.attr.core;

            iopCommandRegister(REQ_SB_SETVOL, &set_vol, sizeof(set_vol));
        }

        if (pitch != sbp->p.pitch)
        {
            sbp->p.pitch = pitch;

            set_pitch.pitch    = pitch;
            set_pitch.voice_no = sbp->p.voice_no;
            set_pitch.core     = sbp->p.attr.core;

            iopCommandRegister(REQ_SB_SETPITCH, &set_pitch, sizeof(set_pitch));
        }
    }
}

// ──────────────────────────────────────────────────────────────────────────
// Ask the IOP to release the voice; parks the slot in FADE_STOP_WAIT.
// FF3 0x0026a930; FF2 SndBufStopSub (unchanged).

static void SndBufStopSub(SND_BUF_PLAYER *sbp)
{
    SOUND_BUF_VOICE stop;

    if (sbp != nullptr)
    {
        stop.voice_no = sbp->p.voice_no;
        stop.core     = sbp->p.attr.core;

        if (iopCommandRegister(REQ_SB_STOP, &stop, sizeof(stop)))
        {
            sbp->fadestop = FADE_STOP_WAIT;
        }
    }
}

// ──────────────────────────────────────────────────────────────────────────
// Cut the voice outright: the only way a looping sample stops.
// FF3 0x0026a988; FF2 SndBufStopSub2 (unchanged).

static void SndBufStopSub2(SND_BUF_PLAYER *sbp)
{
    VOICE_STOP stop2;

    if (sbp != nullptr)
    {
        stop2.voice_no = sbp->p.voice_no;
        stop2.core     = sbp->p.attr.core;

        iopCommandRegister(REQ_VOICE_STOP, &stop2, sizeof(stop2));
    }
}

// ──────────────────────────────────────────────────────────────────────────
// FF3 0x0026a9d0; FF2 SndBufStop (unchanged).

void SndBufStop(int id)
{
    SndBufStopSub(GetSndBufPlayerFromID(id));
}

// ──────────────────────────────────────────────────────────────────────────
// FF3 0x0026a9f0; FF2 SndBufPause's body, on a slot.

static void SndBufPauseSub(SND_BUF_PLAYER *sbp)
{
    SOUND_BUF_VOICE pause;

    if (sbp != nullptr && sbp->pause == 0)
    {
        pause.voice_no = sbp->p.voice_no;
        pause.core     = sbp->p.attr.core;

        if (iopCommandRegister(REQ_SB_PAUSE, &pause, sizeof(pause)))
        {
            sbp->pause = 1;
        }
    }
}

// ──────────────────────────────────────────────────────────────────────────
// FF3 0x0026aa70; FF2 SndBufPause (body split into SndBufPauseSub).

void SndBufPause(int id)
{
    SndBufPauseSub(GetSndBufPlayerFromID(id));
}

// ──────────────────────────────────────────────────────────────────────────
// FF3 0x0026aa90; FF2 SndBufRestart's body, on a slot.

static void SndBufRestartSub(SND_BUF_PLAYER *sbp)
{
    SOUND_BUF_VOICE restart;

    if (sbp != nullptr && sbp->pause != 0)
    {
        restart.voice_no = sbp->p.voice_no;
        restart.core     = sbp->p.attr.core;

        if (iopCommandRegister(REQ_SB_RESTART, &restart, sizeof(restart)))
        {
            sbp->pause = 0;
        }
    }
}

// ──────────────────────────────────────────────────────────────────────────
// FF3 0x0026ab08; FF2 SndBufRestart (body split into SndBufRestartSub).

void SndBufRestart(int id)
{
    SndBufRestartSub(GetSndBufPlayerFromID(id));
}

// ──────────────────────────────────────────────────────────────────────────
// Ramp to vol over frame frames.
// FF3 0x0026ab28; FF2 SndBufVolFade without the range warning.

void SndBufVolFade(int id, int vol, int frame)
{
    SND_BUF_PLAYER *sbp = GetSndBufPlayerFromID(id);

    if (sbp != nullptr)
    {
        sbp->target_vol = vol;
        sbp->spd        = SndGetFrameAddVol(vol, sbp->vol, frame);
    }
}

// ──────────────────────────────────────────────────────────────────────────
// Sets the current pitch only; target_pitch is untouched, so a voice whose
// target differs ramps back towards it (the ROM's, as in FF2).
// FF3 0x0026ab80; FF2 SndBufPitchSet without the range warning.

void SndBufPitchSet(int id, int pitch)
{
    SND_BUF_PLAYER *sbp = GetSndBufPlayerFromID(id);

    if (sbp != nullptr)
    {
        sbp->pitch = pitch;
    }
}

// ──────────────────────────────────────────────────────────────────────────
// FF3 0x0026abb0; FF2 SndBufFadeStop (unchanged).

void SndBufFadeStop(int id, int time)
{
    SND_BUF_PLAYER *sbp = GetSndBufPlayerFromID(id);

    if (sbp != nullptr)
    {
        sbp->target_vol = 0;
        sbp->spd        = SndGetFrameAddVol(0, sbp->vol, time);
        sbp->fadestop   = FADE_STOP_REQ;
    }
}

// ──────────────────────────────────────────────────────────────────────────
// FF3 0x0026ac08; FF2 SndBufIsPlaying (unchanged).

int SndBufIsPlaying(int id)
{
    return GetSndBufPlayerFromID(id) != nullptr;
}

// ──────────────────────────────────────────────────────────────────────────
// FF3 0x0026ac28; FF2 SndBufSetPosition without the "Not 3D Sound" print.

void SndBufSetPosition(int id, float *pos)
{
    SND_BUF_PLAYER *sbp = GetSndBufPlayerFromID(id);

    if (sbp != nullptr && sbp->s3d != nullptr)
    {
        Snd3DSetPosition(sbp->s3d, pos);
    }
}

// ──────────────────────────────────────────────────────────────────────────
// Ramp the pitch to pitch over time frames (time 0: at once).
// FF3 0x0026ac78; FF2 SndBufFadePitch (unchanged).

void SndBufFadePitch(int id, int pitch, int time)
{
    SND_BUF_PLAYER *sbp = GetSndBufPlayerFromID(id);

    if (sbp != nullptr)
    {
        if (time != 0)
        {
            sbp->pspd = SndGetFrameAddVol(pitch, sbp->pitch, time);
        }
        else
        {
            sbp->pitch = pitch;
        }

        sbp->target_pitch = pitch;
    }
}
