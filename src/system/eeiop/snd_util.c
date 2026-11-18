/* ==========================================================================
 *  system/eeiop/snd_util.c
 *
 *  Fire-and-forget bank playback. Each AUTO_BD_WRK is a small state machine:
 *  LOAD_WAIT while snd_bank.c loads the two files, PLAYEND_WAIT while the
 *  voice plays, then NO_USE again with the bank released.
 *
 *  From MioPan system/eeiop/snd_util.c (FF2's). FF3 drops snd_utilAutoBDMain's
 *  sample-count warning (and its SndBankGetInfo call) and the looping-sample
 *  assert; GetVacantAutoBDWrk is inlined at both callers and kept unused.
 *  FF3 0x2705e0..0x270a48.
 * ======================================================================== */
#include "system/eeiop/snd_util.h"

#include <string.h>                     // memset

#include "system/eeiop/snd_bank.h"      // SndBankNew / IsReady / IsLoopSnd / Play / Release
#include "system/eeiop/snd_buffer.h"    // SndBufIsPlaying

static AUTO_BD_WRK *auto_bd_wrk;        /* sbss 0x373ab0 (gp+0x5c0) */
static int          auto_bd_wrk_max;    /* sbss 0x373ab4 (gp+0x5c4) */

// ──────────────────────────────────────────────────────────────────────────
// FF3 0x002705e0; FF2 snd_utilGetOneWrkSize.

int snd_utilGetOneWrkSize(void)
{
    return sizeof(AUTO_BD_WRK);
}

// ──────────────────────────────────────────────────────────────────────────
// Take num works from buffer, all free. Returns the rest of buffer.
// FF3 0x002705e8; FF2 snd_utilAutoBDInit (unchanged).

void *snd_utilAutoBDInit(void *buffer, int num)
{
    void *ret;

    auto_bd_wrk     = (AUTO_BD_WRK *)buffer;
    auto_bd_wrk_max = num;

    ret = (char *)buffer + snd_utilGetOneWrkSize() * num;

    for (int i = 0; i < auto_bd_wrk_max; i++)
    {
        auto_bd_wrk[i].mode = AUTO_BD_MODE_NO_USE;
    }

    return ret;
}

// ──────────────────────────────────────────────────────────────────────────
// Release bank_id once voice snd_buf_id has stopped (straight into
// PLAYEND_WAIT: the bank is loaded and the voice already playing).
// FF3 0x00270650; FF2 snd_utilAutoRelease (GetVacantAutoBDWrk inlined).

int snd_utilAutoRelease(int snd_buf_id, int bank_id)
{
    int wrk_no = GetVacantAutoBDWrk();

    if (wrk_no < 0)
    {
        return AUTO_BD_ERR_NO_WRK;
    }

    auto_bd_wrk[wrk_no].bank_no = bank_id;
    auto_bd_wrk[wrk_no].mode    = AUTO_BD_MODE_PLAYEND_WAIT;
    auto_bd_wrk[wrk_no].play_id = snd_buf_id;
    return AUTO_BD_ERR_OK;
}

// ──────────────────────────────────────────────────────────────────────────
// Start loading file_no / header_file_no into a new bank; snd_utilAutoBDMain
// plays sample 0 with these parameters once it is in.
// FF3 0x002706c0; FF2 snd_utilAutoBDPlay (GetVacantAutoBDWrk inlined).

int snd_utilAutoBDPlay(int file_no, int header_file_no, int effect, int loop,
                       int vol, int pitch, int in_time, SND_3D_SET *s3s)
{
    int wrk_no = GetVacantAutoBDWrk();

    if (wrk_no < 0)
    {
        return AUTO_BD_ERR_NO_WRK;
    }

    auto_bd_wrk[wrk_no].vol     = (short)vol;
    auto_bd_wrk[wrk_no].pitch   = (short)pitch;
    auto_bd_wrk[wrk_no].effect  = effect;
    auto_bd_wrk[wrk_no].loop    = loop;
    auto_bd_wrk[wrk_no].in_time = in_time;

    if (s3s != nullptr)
    {
        auto_bd_wrk[wrk_no].s3d = 1;
        sceVu0CopyVector(auto_bd_wrk[wrk_no].pos, s3s->pos);
        if (s3s->vel != nullptr)
        {
            sceVu0CopyVector(auto_bd_wrk[wrk_no].vel, s3s->vel);
        }
        if (s3s->dir != nullptr)
        {
            sceVu0CopyVector(auto_bd_wrk[wrk_no].dir, s3s->dir);
        }
    }
    else
    {
        auto_bd_wrk[wrk_no].s3d = 0;
    }

    auto_bd_wrk[wrk_no].bank_no = SndBankNew(file_no, header_file_no, -1);
    auto_bd_wrk[wrk_no].mode    = AUTO_BD_MODE_LOAD_WAIT;
    return AUTO_BD_ERR_OK;
}

// ──────────────────────────────────────────────────────────────────────────
// Per frame: play each work whose bank has loaded, release each whose voice
// has stopped. A looping sample would never stop, so its bank is released
// unplayed.
// FF3 0x00270860; FF2 snd_utilAutoBDMain without the BD-count warning and
// the loop assert.

void snd_utilAutoBDMain(void)
{
    SND_3D_SET  set;
    SND_3D_SET *ptr;

    for (int i = 0; i < auto_bd_wrk_max; i++)
    {
        switch (auto_bd_wrk[i].mode)
        {
        case AUTO_BD_MODE_LOAD_WAIT:
            if (SndBankIsReady(auto_bd_wrk[i].bank_no))
            {
                memset(&set, 0, sizeof(SND_3D_SET));
                ptr = nullptr;
                if (auto_bd_wrk[i].s3d)
                {
                    set.pos = auto_bd_wrk[i].pos;
                    set.vel = auto_bd_wrk[i].vel;
                    set.dir = auto_bd_wrk[i].dir;
                    ptr     = &set;
                }

                if (SndBankIsLoopSnd(auto_bd_wrk[i].bank_no, 0))
                {
                    SndBankRelease(auto_bd_wrk[i].bank_no);
                    auto_bd_wrk[i].mode = AUTO_BD_MODE_NO_USE;
                }
                else
                {
                    auto_bd_wrk[i].play_id =
                        SndBankPlay(auto_bd_wrk[i].bank_no, 0, auto_bd_wrk[i].effect,
                                    auto_bd_wrk[i].loop, auto_bd_wrk[i].vol,
                                    auto_bd_wrk[i].pitch, auto_bd_wrk[i].in_time, ptr);
                    auto_bd_wrk[i].mode = AUTO_BD_MODE_PLAYEND_WAIT;
                }
            }
            break;

        case AUTO_BD_MODE_PLAYEND_WAIT:
            if (SndBufIsPlaying(auto_bd_wrk[i].play_id) == 0)
            {
                SndBankRelease(auto_bd_wrk[i].bank_no);
                auto_bd_wrk[i].mode = AUTO_BD_MODE_NO_USE;
            }
            break;

        default:
            break;
        }
    }
}

// ──────────────────────────────────────────────────────────────────────────
// First free work, or -1. Nothing calls it in FF3 (both users inline it).
// FF3 0x00270a08; FF2 GetVacantAutoBDWrk.

int GetVacantAutoBDWrk(void)
{
    for (int i = 0; i < auto_bd_wrk_max; i++)
    {
        if (auto_bd_wrk[i].mode == AUTO_BD_MODE_NO_USE)
        {
            return i;
        }
    }

    return -1;
}
