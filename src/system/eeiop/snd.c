/* ==========================================================================
 *  system/eeiop/snd.c
 *
 *  Sound front end. SndInit initialises the SPU memory pool and the voice
 *  allocator, starts the IOP's sound server, then brings up the other sound
 *  modules in dependency order, handing each its slice of the one work
 *  buffer; SndMain is their per-frame pump. The rest is the mixing arithmetic
 *  every voice owner shares.
 *
 *  From MioPan system/eeiop/snd.c (FF2's snd.c). FF3 dropped every range
 *  check and print (SndSetEffect's volume check, SndSetGroupVolume's,
 *  SndCalcValue's pitch warning), SndInitAfter_ee_iopInit /
 *  SndFremaAfterMain / IsExistFreeVoice, and SndAllStop stops the PCM
 *  streams instead of the streams.
 * ======================================================================== */
#include "system/eeiop/snd.h"

#include "system/eeiop/ee_iop.h"        // iopCommandRegister / ee_iopMain
#include "system/eeiop/snd_buffer.h"    // SndBufInit / SndBufPlayMain / SndBufAllStop
#include "system/eeiop/snd_stream.h"    // SndStreamInit / SndStreamMain / SndStreamAllStop
#include "system/eeiop/snd_bank.h"      // SndBankGetOneWrkSize / SndBankInitAll
#include "system/eeiop/snd_3d.h"        // Snd3DInit / Snd3DGetVal / Snd3DMain
#include "system/eeiop/stream_auto.h"   // StreamAutoGetOneWrkSize / PlayInit / PlayMain
#include "system/eeiop/snd_util.h"      // snd_utilGetOneWrkSize / AutoBDInit / AutoBDMain
#include "system/eeiop/spu_mem.h"       // SPUMemoryInit / GetSPUMemory / GetSPUEffectMemory
#include "system/eeiop/spu_voice.h"     // InitSPUVoice / GetSPUVoiceCore / FreeSPUVoiceCore
#include "system/eeiop/snd_pcmstream.h" // SndPCMStreamInit / SndPCMStreamMain

// SPU2 reverb work-area sizes per sceSdEffectAttr mode: OFF, ROOM, STUDIO_A,
// STUDIO_B, STUDIO_C, HALL, SPACE, ECHO, DELAY, PIPE (same as FF2).
int eff_use_size_tbl[10] =              /* data 0x333a90 */
{
    0x00000080, 0x000026c0, 0x00001f40, 0x00004840, 0x00006fe0,
    0x0000ade0, 0x0000f6c0, 0x00018040, 0x00018040, 0x00003c00
};

static SOUND_SYS snd_sys;               /* bss 0x411078 */
static int       effect_mode[2];        /* sbss gp+0x5a0 */
static int       effect_adrs[2];        /* sbss gp+0x5a8 */

// ──────────────────────────────────────────────────────────────────────────
// Work bytes the sound modules need from ee_iopInit's buffer.
// FF3 0x0026f770; from MioPan snd.c sndGetNeedSize (unchanged).

int sndGetNeedSize(EEIOP_DEF *def)
{
    int size;

    size  = SndBankGetOneWrkSize() * def->snd_bank_wrk_num;
    size += StreamAutoGetOneWrkSize() * def->stream_auto_wrk_num;
    size += snd_utilGetOneWrkSize() * def->auto_bd_wrk_num;
    return size;
}

// ──────────────────────────────────────────────────────────────────────────
// Bring up the sound system and return what is left of buffer.
// FF3 0x0026f7c8; from MioPan snd.c SndInit. FF3 takes the SPU memory range
// from EEIOP_DEF (FF2's SPUMemoryInit had no arguments).

void *SndInit(EEIOP_DEF *def, void *buffer)
{
    IOP_SND_INIT   snd_init;
    SET_SND_EFFECT eff;

    SPUMemoryInit(def->spu_mem_start, def->spu_mem_size);
    InitSPUVoice();

    // media 2 is the CD/DVD path; anything else (host file server) gets 0.
    snd_init.media      = def->media == 2 ? 0x800 : 0;
    snd_init.mvol       = 0x3fff;
    snd_init.stop_block = GetSPUMemory(0x10);
    iopCommandRegister(REQ_IOP_SND_INIT, &snd_init, sizeof(IOP_SND_INIT));

    Snd3DInit();
    SndBufInit();
    SndStreamInit(def->snd_stream_load_priority);
    SndPCMStreamInit();

    buffer = SndBankInitAll(buffer, def->snd_bank_wrk_num, def->snd_bank_load_priority);
    buffer = StreamAutoPlayInit(buffer, def->stream_auto_wrk_num);
    buffer = snd_utilAutoBDInit(buffer, def->auto_bd_wrk_num);

    // Core 0 gets HALL reverb up front. effect_mode[0] stays 0, so the first
    // SndSetEffect(0, ..., 3) re-allocates an area of the same size. delay
    // and feedback go out unset, as in the ROM.
    eff.r_attr.delay   = 0;
    snd_sys.mono       = 0;
    eff.r_attr.depth_L = 0x2fff;
    eff.r_attr.mode    = 3;
    eff.core           = 0;
    eff.r_attr.core    = 0;
    eff.r_attr.depth_R = 0x2fff;
    effect_adrs[0]     = GetSPUEffectMemory(eff_use_size_tbl[3]);
    eff.end_adrs       = effect_adrs[0];
    iopCommandRegister(REQ_SET_SND_EFFECT, &eff, sizeof(SET_SND_EFFECT));

    for (int i = 0; i < VOICE_TYPE_MAX; i++)
    {
        SndSetGroupVolume(i, 0x100);
    }

    // Two passes so both commands reach the IOP before anything plays.
    ee_iopMain();
    ee_iopMain();
    return buffer;
}

// ──────────────────────────────────────────────────────────────────────────
// Per-frame pump of every sound module.
// FF3 0x0026f918; from MioPan snd.c SndMain (unchanged).

void SndMain(void)
{
    Snd3DMain();
    StreamAutoPlayMain();
    snd_utilAutoBDMain();
    SndStreamMain();
    SndPCMStreamMain();
    SndBufPlayMain();
}

// ──────────────────────────────────────────────────────────────────────────
// Set a core's reverb: a new mode moves the work area (released, then a new
// one of that mode's size); the volume goes to both sides. On an unchanged
// mode end_adrs (and always delay/feedback) go out unset, as in the ROM.
// FF3 0x0026f960; from MioPan snd.c SndSetEffect without the volume check,
// the two command calls merged into one.

void SndSetEffect(int core, int eff_vol, int mode)
{
    SET_SND_EFFECT eff;

    eff.core           = core;
    eff.r_attr.core    = core;
    eff.r_attr.mode    = mode;
    eff.r_attr.depth_L = (short)eff_vol;
    eff.r_attr.depth_R = (short)eff_vol;
    // PC: the IOP side never reads what FF3 leaves unset; clear it anyway so
    // the bytes sent are defined.
    eff.end_adrs        = 0;
    eff.r_attr.delay    = 0;
    eff.r_attr.feedback = 0;

    if (mode != effect_mode[core])
    {
        ReleaseSPUEffectMemory(effect_adrs[core]);
        eff.end_adrs      = GetSPUEffectMemory(eff_use_size_tbl[mode]);
        effect_adrs[core] = eff.end_adrs;
    }
    iopCommandRegister(REQ_SET_SND_EFFECT, &eff, sizeof(SET_SND_EFFECT));
    effect_mode[core] = mode;
}

// ──────────────────────────────────────────────────────────────────────────
// Stereo / mono output (SndCalcValue folds both sides into one when mono).
// FF3 0x0026fa48; from MioPan snd.c (SndSetMono, SndIsMono likewise).

void SndSetStereo(void)
{
    snd_sys.mono = 0;
}

// FF3 0x0026fa58

void SndSetMono(void)
{
    snd_sys.mono = 1;
}

// FF3 0x0026fa68

int SndIsMono(void)
{
    return snd_sys.mono;
}

// ──────────────────────────────────────────────────────────────────────────
// Mixer group volume (0..0x100) per voice type.
// FF3 0x0026fa78; from MioPan snd.c SndSetGroupVolume without its checks.

void SndSetGroupVolume(int type, int vol)
{
    snd_sys.type_vol[type] = vol;
}

// FF3 0x0026fa90; from MioPan snd.c SndGetGroupVolume without its check.

int SndGetGroupVolume(int type)
{
    return snd_sys.type_vol[type];
}

// ──────────────────────────────────────────────────────────────────────────
// Volume pair and pitch of a voice. bvol (0..0x80) and the group volume
// (0..0x100) scale vol; pan splits it; a 3D slot scales the pair by its own
// 0..0x3fff pair and the pitch by its doppler factor.
// FF3 0x0026faa8; from MioPan snd.c SndCalcValue. FF3 truncates the scaled
// pitch to a short before the play_speed multiply, and the pitch warning is
// gone.

void SndCalcValue(int vol, int pan, int bvol, int pitch, int bpitch, int type,
                  void *s3d, VOLSET *volset, short *pPitch, float play_speed)
{
    VOLSET s3dvol;

    vol = (vol * bvol >> 7) * SndGetGroupVolume(type) >> 8;

    volset->l = (short)(vol * (0x80 - pan) >> 7);
    volset->r = (short)(vol * pan >> 7);

    if (s3d == nullptr)
    {
        *pPitch = (short)bpitch;
    }
    else
    {
        Snd3DGetVal(s3d, &s3dvol, pPitch);
        volset->l = (short)(volset->l * s3dvol.l >> 14);
        volset->r = (short)(volset->r * s3dvol.r >> 14);
        *pPitch   = (short)(bpitch * *pPitch >> 12);
    }

    *pPitch = (short)(int)((float)(short)(pitch * *pPitch >> 12) * play_speed);

    if (SndIsMono() != 0)
    {
        volset->r = volset->l = (short)(volset->r + volset->l >> 1);
    }
}

// ──────────────────────────────────────────────────────────────────────────
// Per-frame step that walks now to target in time frames; the whole
// difference when it is smaller than time or time is 0.
// FF3 0x0026fc08; from MioPan snd.c SndGetFrameAddVol (unchanged).

short SndGetFrameAddVol(int target, int now, int time)
{
    const int diff    = target - now;
    const int absdiff = diff < 0 ? -diff : diff;

    if (absdiff < time || time == 0)
    {
        return (short)diff;
    }
    return (short)(diff / time);
}

// ──────────────────────────────────────────────────────────────────────────
// One fade step: vol + spd, or target_vol once the step reaches or crosses it
// (or spd is 0).
// FF3 0x0026fc50; from MioPan snd.c SndAddFadeVol (unchanged).

int SndAddFadeVol(int vol, int target_vol, int spd)
{
    const int new_vol = vol + spd;

    if (spd != 0 && (target_vol - vol) * (target_vol - new_vol) > 0)
    {
        return new_vol;
    }
    return target_vol;
}

// ──────────────────────────────────────────────────────────────────────────
// Stop every SE voice and both ADPCM streams.
// FF3 0x0026fc78; from MioPan snd.c SndAllStop (FF2 stopped the streams).

void SndAllStop(void)
{
    SndBufAllStop();
    SndStreamAllStop();
}

// ──────────────────────────────────────────────────────────────────────────
// Free SPU voices on both cores, counted by claiming every free one and
// handing them all back.
// FF3 0x0026fc98; from MioPan snd.c GetFreeVoiceNum (unchanged).

int GetFreeVoiceNum(void)
{
    int voice[25];                      // 24 voices + the -1 that ends the run
    int ret_num = 0;

    for (int core = 0; core < 2; core++)
    {
        int num = 0;

        while ((voice[num] = GetSPUVoiceCore(core)) != -1)
        {
            num++;
        }
        for (int j = 0; j < num; j++)
        {
            FreeSPUVoiceCore(core, voice[j]);
        }
        ret_num += num;
    }
    return ret_num;
}
