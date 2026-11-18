/* ==========================================================================
 *  ingame/sound/snd_ctrl.c
 *
 *  FF3-only (see snd_ctrl.h): the game's sound API over the library. Every
 *  SE bank a scene needs is loaded by file number with SndSeBankLoad and
 *  shared through the table below; SE ids carry their bank's file number,
 *  so SndSePlay finds the bank by itself. BGM goes through two slots that
 *  remember which file each stream request plays.
 * ======================================================================== */
#include "ingame/sound/snd_ctrl.h"

#include <stddef.h>                     // NULL

#include "sdk/libvu0.h"                 // sceVu0FVECTOR
#include "system/eeiop/cddat.h"         // GetFileSize
#include "system/eeiop/snd_buffer.h"    // SndBuf*, CSND_BUF_PLAY_NO_ID
#include "system/eeiop/snd.h"           // SndSetEffect, SndSetGroupVolume, SndSetMono/Stereo, GetFreeVoiceNum
#include "system/eeiop/snd_stream.h"   // SndStreamAllStop
#include "system/eeiop/snd_bank.h"     // SndBank*
#include "system/eeiop/snd_3d.h"       // snd3DSetEnvironment, snd3DSet1Meter, SndSetListener
#include "system/eeiop/stream_auto.h"  // StreamAuto*: the BGM streams
#include "system/eeiop/snd_util.h"     // snd_utilAutoBDPlay / snd_utilAutoRelease

#define SE_BANK_NUM         28
#define BGM_SLOT_NUM        2

typedef struct SND_SE_BANK              /* 0x8 */
{
    /* 0x0 */ int   bank;               // SndBankNew number, -1: free entry
    /* 0x4 */ short file;               // BD file; the header is file + 1
    /* 0x6 */ short ref;                // SndSeBankLoad calls not yet released
} SND_SE_BANK;
static_assert(sizeof(SND_SE_BANK) == 0x8, "SND_SE_BANK size");
static_assert(offsetof(SND_SE_BANK, file) == 0x4, "SND_SE_BANK.file");
static_assert(offsetof(SND_SE_BANK, ref) == 0x6, "SND_SE_BANK.ref");

typedef struct SND_BGM_SLOT             /* 0x8 */
{
    /* 0x0 */ int handle;               // stream request, -1: none
    /* 0x4 */ int file;
} SND_BGM_SLOT;
static_assert(sizeof(SND_BGM_SLOT) == 0x8, "SND_BGM_SLOT size");
static_assert(offsetof(SND_BGM_SLOT, file) == 0x4, "SND_BGM_SLOT.file");

static SND_SE_BANK  snd_se_bank[SE_BANK_NUM];   // bss 0x3fcf50
static SND_BGM_SLOT snd_bgm[BGM_SLOT_NUM];      // bss 0x3fd030

// Inlined everywhere in FF3: the entry holding a bank file, NULL if none.
static SND_SE_BANK *SndSeBankFind(int file)
{
    int i;

    if (file == -1)
        return NULL;

    for (i = 0; i < SE_BANK_NUM; i++)
    {
        if (snd_se_bank[i].file == file)
            return &snd_se_bank[i];
    }

    return NULL;
}

// Inlined in SndBgmPlay / SndBgmPreload / SndBgmIsFileSet.
static SND_BGM_SLOT *SndBgmFind(int file)
{
    int i;

    if (file == -1)
        return NULL;

    for (i = 0; i < BGM_SLOT_NUM; i++)
    {
        if (snd_bgm[i].file == file)
            return &snd_bgm[i];
    }

    return NULL;
}

// FF3 0x00257ae0. The default bank stays unless with_default is set.
void SndSeBankReleaseAll(int with_default)
{
    int i;

    for (i = 0; i < SE_BANK_NUM; i++)
    {
        int file = snd_se_bank[i].file;

        if (file != -1 && (file != SE_BANK_DEFAULT || with_default != 0))
            SndSeBankRelease(file, 1);
    }
}

// FF3 0x00257ba0. Called once from GameSystemStart.
void SndSystemInit(void)
{
    SND_3D_ENV    env = { 0.8f, 0.1f, 0.7f, 0.3f, 30.0f, 2.0f };   // rodata 0x3725e0 (FF2's defaults)
    sceVu0FVECTOR pos = { 0.0f, 0.0f, 0.0f, 1.0f };
    sceVu0FVECTOR vel = { 0.0f, 0.0f, 0.0f, 0.0f };
    sceVu0FVECTOR dir = { 0.0f, 0.0f, 1.0f, 0.0f };
    sceVu0FVECTOR top = { 1.0f, 0.0f, 0.0f, 0.0f };             // as FF3 builds it
    SND_3D_SET    l;
    int           i;

    for (i = 0; i < SE_BANK_NUM; i++)
    {
        snd_se_bank[i].bank = -1;
        snd_se_bank[i].file = -1;
        snd_se_bank[i].ref  = 0;
    }

    SndBgmStopAll();
    SndSetDefault();
    SndSeReset();

    snd3DSetEnvironment(&env);

    l.pos = pos;
    l.vel = vel;
    l.dir = dir;
    snd3DSet1Meter(20.0f);                                      // rodata 0x3725fc
    SndSetListener(&l, top);
}

// FF3 0x00257cd0. Looping SE stop (one-shots are released), and core 0's
// reverb back to HALL at 0x2fff.
void SndSeReset(void)
{
    SndBufAllStopLoopSnd();
    SndSetEffect(0, 0x2fff, 3);
}

// FF3 0x00257cf8. Called once from GameSystemEnd.
void SndSystemEnd(void)
{
    SndBgmStopAll();
    SndAllBufStop();
    SndSeBankReleaseAll(1);
}

// FF3 0x00257d20. IngameEnd passes 0: the default bank stays.
void SndIngameStopAll(int with_default)
{
    SndBgmStopAll();
    SndAllBufStop();
    SndSeBankReleaseAll(with_default);
}

// FF3 0x00257d50.
void SndSetDefault(void)
{
    SndSetVolume(0, 200);
    SndSetVolume(1, 200);
    SndSetMonoMode(0);
    StreamAutoSetPlayNum(2, 0);
    StreamAutoSetExclusiveMode(0, 0);
}

// FF3 0x00257d98. No callers in FF3.
int SndGetFreeVoiceNum(void)
{
    return GetFreeVoiceNum();
}

// FF3 0x00257db0. OptionApplySound sets the two option volumes here.
void SndSetVolume(int type, int vol)
{
    SndSetGroupVolume(type, vol);
}

// FF3 0x00257dc8.
void SndSetMonoMode(int mode)
{
    if (mode == 1)
    {
        SndSetMono();
        return;
    }

    SndSetStereo();
}

// FF3 0x00257df0.
int SndConvVolume(int vol)
{
    return vol * 0x81;
}

// FF3 0x00257e00. A bank already in the table gains a reference; otherwise
// a free entry takes the new bank. Callers poll until SE_BANK_READY.
int SndSeBankLoad(int file)
{
    SND_SE_BANK *sb;
    int          bank;
    int          i;

    if (file == -1)
        file = SE_BANK_DEFAULT;

    if (GetFileSize(file) == (unsigned int)-1)
        return SE_BANK_ERROR;

    if (GetFileSize(file) == 0)
        return SE_BANK_ERROR;

    if (SndBankGetFreeBankNum() == 0)
        return SE_BANK_ERROR;

    sb = SndSeBankFind(file);
    if (sb != NULL)
    {
        sb->ref++;
        return SndBankIsReady(sb->bank) != 0 ? SE_BANK_READY : SE_BANK_LOADING;
    }

    for (i = 0; i < SE_BANK_NUM; i++)
    {
        if (snd_se_bank[i].bank == -1)
        {
            sb = &snd_se_bank[i];
            break;
        }
    }

    if (sb == NULL)
        return SE_BANK_ERROR;

    bank = SndBankNew(file, file + 1, -1);
    if (bank < 0)
        return SE_BANK_ERROR;

    sb->bank = bank;
    sb->ref  = 1;
    sb->file = (short)file;

    return SE_BANK_LOADING;
}

// FF3 0x00257f50. True also for a file that is not in the table.
int SndSeBankIsLoaded(int file)
{
    SND_SE_BANK *sb;

    if (file == -1)
        file = SE_BANK_DEFAULT;

    sb = SndSeBankFind(file);
    if (sb == NULL)
        return 1;

    return SndBankIsReady(sb->bank) != 0;
}

// FF3 0x00257fc8.
int SndSeBankIsEntry(int file)
{
    if (file == -1)
        file = SE_BANK_DEFAULT;

    return SndSeBankFind(file) != NULL;
}

// FF3 0x00258020. The bank goes with its last reference, or at once when
// forced.
int SndSeBankRelease(int file, int force)
{
    SND_SE_BANK *sb;

    if (file == -1)
        file = SE_BANK_DEFAULT;

    sb = SndSeBankFind(file);
    if (sb == NULL)
        return 0;

    sb->ref--;
    if (force == 0 && sb->ref >= 1)
        return 0;

    SndBankRelease(sb->bank);
    sb->ref  = 0;
    sb->file = -1;
    sb->bank = -1;

    return 1;
}

// FF3 0x002580d8. As SndSeBankRelease, but the bank is freed only when the
// voice play_id has ended. No default-bank substitution here; the reference
// is dropped even when no auto work was free.
int SndSeBankReleaseAfterPlay(int play_id, int file, int force)
{
    SND_SE_BANK *sb;

    sb = SndSeBankFind(file);
    if (sb == NULL)
        return 0;

    sb->ref--;
    if (force == 0 && sb->ref >= 1)
        return 0;

    if (snd_utilAutoRelease(play_id, sb->bank) != 0)
        return 0;

    sb->ref  = 0;
    sb->file = -1;
    sb->bank = -1;

    return 1;
}

// FF3 0x00258190. Effect send on; returns the play id.
int SndSePlay(unsigned int se, int vol, int pitch, int time, void *s3s, int loop)
{
    SND_SE_BANK *sb;
    int          id;

    sb = SndSeBankFind((int)(se >> 16));
    if (sb != NULL && SndBankIsReady(sb->bank) != 0)
    {
        id = SndBankPlay(sb->bank, se & 0xffff, 1, loop, SndConvVolume(vol), pitch, time, (SND_3D_SET *)s3s);
        if (id != CSND_BUF_PLAY_NO_ID && id > 0)
            return id;
    }

    return SND_SE_PLAY_ERROR;
}

// FF3 0x002582a0. A one-sample bank (file se >> 16) loaded, played and
// released by snd_util's auto work; the sample number is not used.
int SndSeAutoPlay(unsigned int se, int vol, int pitch, int time, void *s3s, int loop)
{
    int file = (int)(se >> 16);

    if (SndBankGetFreeBankNum() == 0)
        return 0;

    return snd_utilAutoBDPlay(file, file + 1, 1, loop, SndConvVolume(vol), pitch, time, (SND_3D_SET *)s3s) == 0;
}

// FF3 0x00258340.
void SndSeFadeStop(int id, int time)
{
    SndBufFadeStop(id, time);
}

// FF3 0x00258358.
void SndSeVolFade(int id, int vol, int time)
{
    SndBufVolFade(id, SndConvVolume(vol), time);
}

// FF3 0x00258398.
void SndSeFadePitch(int id, int pitch, int time)
{
    SndBufFadePitch(id, pitch, time);
}

// FF3 0x002583b0.
void SndSeSetPosition(int id, float *pos)
{
    SndBufSetPosition(id, pos);
}

// FF3 0x002583c8.
int SndSeIsPlaying(int id)
{
    return SndBufIsPlaying(id);
}

// FF3 0x002583e0.
void SndAllBufStop(void)
{
    SndBufAllStop();
}

// FF3 0x002583f8.
int SndSeIsLoop(unsigned int se)
{
    SND_SE_BANK *sb = SndSeBankFind((int)(se >> 16));

    if (sb != NULL)
        return SndBankIsLoopSnd(sb->bank, se & 0xffff);

    return 0;
}

// FF3 0x00258468. Body and file (file + 1 is the header) must exist. A file
// already in a slot keeps playing and its handle is returned; otherwise slot
// `slot` is stopped and takes the new request. -1 on failure.
int SndBgmPlay(int file, int vol, int pitch, int time, void *s3s, int flag, int slot)
{
    SND_BGM_SLOT *bs;

    if (GetFileSize(file) == (unsigned int)-1)
        return -1;
    if (GetFileSize(file) == 0)
        return -1;
    if (GetFileSize(file + 1) == (unsigned int)-1)
        return -1;
    if (GetFileSize(file + 1) == 0)
        return -1;

    bs = SndBgmFind(file);
    if (bs != NULL)
        return bs->handle;

    bs = &snd_bgm[slot];
    StreamAutoFadeOut(bs->handle, 0);
    bs->file   = file;
    bs->handle = -1;
    bs->handle = StreamAutoPlayPitch(file, file + 1, 0xc, 1, flag, SndConvVolume(vol), time, (SND_3D_SET *)s3s, pitch);

    return bs->handle;
}

// FF3 0x002585d8.
void SndBgmStop(int time, int slot)
{
    StreamAutoFadeOut(snd_bgm[slot].handle, time);
    snd_bgm[slot].file   = -1;
    snd_bgm[slot].handle = -1;
}

// FF3 0x00258620.
void SndBgmStopAll(void)
{
    int i;

    StreamAutoAllStop();
    SndStreamAllStop();

    for (i = 0; i < BGM_SLOT_NUM; i++)
    {
        snd_bgm[i].handle = -1;
        snd_bgm[i].file   = -1;
    }
}

// FF3 0x00258670.
int SndBgmPause(int slot)
{
    return StreamAutoPause(snd_bgm[slot].handle);
}

// FF3 0x00258698.
int SndBgmRestart(int slot)
{
    return StreamAutoRestart(snd_bgm[slot].handle);
}

// FF3 0x002586c0. On a stream channel or still queued.
int SndBgmIsActive(int slot)
{
    return StreamAutoIsPlaying(snd_bgm[slot].handle);
}

// FF3 0x002586e8.
int SndBgmIsAllIdle(void)
{
    return StreamAutoIsAllStop();
}

// FF3 0x00258700. SndBgmPlay's twin that loads without playing: poll
// SndBgmIsPreloaded, then SndBgmStartPreloaded.
int SndBgmPreload(int file, int vol, int pitch, int time, void *s3s, int flag, int slot)
{
    SND_BGM_SLOT *bs;

    if (GetFileSize(file) == (unsigned int)-1)
        return -1;
    if (GetFileSize(file) == 0)
        return -1;
    if (GetFileSize(file + 1) == (unsigned int)-1)
        return -1;
    if (GetFileSize(file + 1) == 0)
        return -1;

    bs = SndBgmFind(file);
    if (bs != NULL)
        return bs->handle;

    bs = &snd_bgm[slot];
    StreamAutoFadeOut(bs->handle, 0);
    bs->file   = file;
    bs->handle = -1;
    bs->handle = StreamAutoPreloadPitch(file, file + 1, 0xc, 1, flag, SndConvVolume(vol), time, (SND_3D_SET *)s3s, pitch);

    return bs->handle;
}

// FF3 0x00258870.
int SndBgmIsPreloaded(int slot)
{
    return StreamAutoIsPreload(snd_bgm[slot].handle);
}

// FF3 0x00258898.
int SndBgmStartPreloaded(int slot)
{
    return StreamAutoPreloadPlay(snd_bgm[slot].handle);
}

// FF3 0x002588c0.
void SndBgmVolFade(int vol, int time, int slot)
{
    if (SndBgmIsActive(slot) != 0)
        StreamAutoFade(snd_bgm[slot].handle, SndConvVolume(vol), time);
}

// FF3 0x00258948.
void SndBgmFadePitch(int pitch, int time, int slot)
{
    if (SndBgmIsActive(slot) != 0)
        StreamAutoFadePitch(snd_bgm[slot].handle, pitch, time);
}

// FF3 0x002589c0.
void SndBgmSetSpeed(float rate, int slot)
{
    if (SndBgmIsActive(slot) != 0)
        StreamAutoChangePlaySpeed(snd_bgm[slot].handle, rate);
}

// FF3 0x00258a20.
int SndBgmIsFileSet(int file)
{
    return SndBgmFind(file) != NULL;
}
