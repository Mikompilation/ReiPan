/* ==========================================================================
 *  system/eeiop/snd_pcmstream.c
 *
 *  Straight PCM streaming. The EE owns the state machine and the volume
 *  ramp; the IOP reads and plays, and reports back through
 *  GetPCMStreamWrkRet(). The EE's `status` is what the game asked for, the
 *  IOP's is what has happened, and SndPCMStreamMain() reconciles them.
 *
 *  From MioPan zero_rom system/eeiop/snd_pcmstream.c (FF2's). FF3 dropped
 *  every print and assert (SndPCMStreamIsPreload's inverted one included),
 *  sends no file name with REQ_PCM_STREAMSTART (0x10 bytes, FF2 0x110;
 *  system/iop/iop_snd_def.h follows), inlined SndPCMStreamFade into
 *  SndPCMStreamPlay and moved Fade to the end of the file. Nothing in FF3
 *  calls the API past Init / Main.
 * ======================================================================== */
#include "system/eeiop/snd_pcmstream.h"

#include "system/eeiop/cddat.h"         // GetFileStartSector, GetFileSize
#include "system/eeiop/ee_iop.h"        // iopCommandRegister, GetPCMStreamWrkRet, SetCDReadMode
#include "system/eeiop/snd.h"           // SndGetFrameAddVol, SndAddFadeVol
#include "system/eeiop/snd_stream.h"    // EEIOP_STREAM_STATUS

SND_PCM_STREAM_WRK snd_pcm_stream_wrk[2];                   /* data 0x333ab8 */

// FF3 0x0026fd50
void SndPCMStreamInit(void)
{
    iopCommandRegister(REQ_PCM_STREAMCREATE, nullptr, 0);

    for (int i = 0; i < 2; i++)
    {
        snd_pcm_stream_wrk[i].s.wrk_id = (short)i;
        snd_pcm_stream_wrk[i].p.wrk_id = (short)i;
        snd_pcm_stream_wrk[i].status   = ST_STREAM_NO_USE;
    }
}

// FF3 0x0026fda0 (unreferenced in FF3). offset is not read (nor in FF2).
SND_PCM_STREAM_ERR SndPCMStreamStart(int wrk_id, int file_no, int offset)
{
    SND_PCM_STREAM_WRK *wrk = &snd_pcm_stream_wrk[wrk_id];

    (void)offset;

    // The slot-in-use answer is ignored: the start goes out regardless.
    SndPCMStreamInitWrk(wrk, file_no);

    wrk->s.start_sector = GetFileStartSector(wrk->file_no);
    wrk->s.size         = (int)GetFileSize(wrk->file_no);
    wrk->pause          = 0;

    // Streaming wants the drive in its low-latency read mode.
    SetCDReadMode(0);

    if (iopCommandRegister(REQ_PCM_STREAMSTART, &wrk->s, sizeof(PCM_STREAM_START)) == 0)
        return SND_PCM_STREAM_ERR_IOPSEND;

    return SND_PCM_STREAM_OK;
}

// FF3 0x0026fe20 (unreferenced in FF3). loop2 is not read (nor in FF2).
SND_PCM_STREAM_ERR SndPCMStreamPlay(int wrk_id, int loop2, int vol, int in_time)
{
    SND_PCM_STREAM_WRK *wrk = &snd_pcm_stream_wrk[wrk_id];

    (void)loop2;

    if (wrk->status == ST_STREAM_NO_USE)
        return SND_PCM_STREAM_ERR_NOT_USE;

    if (in_time != 0)
    {
        // A fade in from silence. The first step is a dead store: the
        // inlined SndPCMStreamFade recomputes it from the same inputs.
        wrk->vol        = 0;
        wrk->spd        = SndGetFrameAddVol(vol, 0, in_time);
        wrk->target_vol = (short)vol;
        wrk->spd        = SndGetFrameAddVol((short)vol, wrk->vol, in_time);
    }
    else
    {
        wrk->vol        = (short)vol;
        wrk->target_vol = (short)vol;
    }

    wrk->p.vol = wrk->vol;

    if (iopCommandRegister(REQ_PCM_STREAMPLAY, &wrk->p, sizeof(PCM_STREAM_PLAY)) == 0)
        return SND_PCM_STREAM_ERR_IOPSEND;

    wrk->play_flg = 1;

    return SND_PCM_STREAM_OK;
}

// FF3 0x0026fef0
void SndPCMStreamMain(void)
{
    for (int wi = 0; wi < 2; wi++)
    {
        SND_PCM_STREAM_WRK *wrk = &snd_pcm_stream_wrk[wi];

        if (wrk->status == ST_STREAM_NO_USE)
            continue;

        if (wrk->status == ST_STREAM_START)
        {
            // The IOP reaching PLAYING means the ring is primed; a Play that
            // came before that only set play_flg and is honoured here.
            if (GetPCMStreamWrkRet(wi)->status == ST_STREAM_PLAYING)
            {
                wrk->pre_load_ok = 1;

                if (wrk->play_flg)
                    wrk->status = ST_STREAM_PLAYING;
            }
        }
        else if (wrk->status == ST_STREAM_PLAYING)
        {
            if (wrk->target_vol != wrk->vol)
            {
                PCM_STREAM_SETVOL sts;

                wrk->vol = (short)SndAddFadeVol(wrk->vol, wrk->target_vol, wrk->spd);

                sts.wrk_id = (short)wi;
                sts.vol    = wrk->vol;
                iopCommandRegister(REQ_PCM_STREAMSETVOL, &sts, sizeof(PCM_STREAM_SETVOL));
            }

            if (GetPCMStreamWrkRet(wi)->status == ST_STREAM_END)
                wrk->status = ST_STREAM_END;
        }
        else if (wrk->status == ST_STREAM_END)
        {
            PCM_STREAM_INIT sts;

            sts.wrk_id = wi;
            iopCommandRegister(REQ_PCM_STREAMINIT, &sts, sizeof(PCM_STREAM_INIT));

            // Back to the ordinary read mode now that nothing streams.
            SetCDReadMode(1);

            wrk->status = ST_STREAM_NO_USE;
        }
    }
}

// FF3 0x00270028
SND_PCM_STREAM_ERR SndPCMStreamInitWrk(SND_PCM_STREAM_WRK *wrk, int file_no)
{
    if (wrk->status != ST_STREAM_NO_USE)
        return SND_PCM_STREAM_ERR_IN_USE;

    wrk->file_no     = file_no;
    wrk->pre_load_ok = 0;
    wrk->play_flg    = 0;
    wrk->status      = ST_STREAM_START;

    return SND_PCM_STREAM_OK;
}

// FF3 0x00270068
void SndPCMStreamStop(int wrk_id)
{
    if (snd_pcm_stream_wrk[wrk_id].status != ST_STREAM_NO_USE)
    {
        PCM_STREAM_STOP stop;

        stop.wrk_id = wrk_id;
        iopCommandRegister(REQ_PCM_STREAMSTOP, &stop, sizeof(PCM_STREAM_STOP));
    }
}

// FF3 0x002700b0 (unreferenced in FF3)
void SndPCMStreamAllStop(void)
{
    SndPCMStreamStop(0);
    SndPCMStreamStop(1);
}

// FF3 0x002700d8 (unreferenced in FF3)
void SndPCMStreamPause(int wrk_id)
{
    SND_PCM_STREAM_WRK *wrk = &snd_pcm_stream_wrk[wrk_id];

    if (wrk->status != ST_STREAM_NO_USE && wrk->pause == 0)
    {
        PCM_STREAM_PAUSE pause;

        wrk->pause = 1;

        pause.wrk_id = wrk_id;
        iopCommandRegister(REQ_PCM_STREAMPAUSE, &pause, sizeof(PCM_STREAM_PAUSE));
    }
}

// FF3 0x00270140 (unreferenced in FF3)
void SndPCMStreamRestart(int wrk_id)
{
    SND_PCM_STREAM_WRK *wrk = &snd_pcm_stream_wrk[wrk_id];

    if (wrk->status != ST_STREAM_NO_USE && wrk->pause != 0)
    {
        PCM_STREAM_RESTART restart;

        wrk->pause = 0;

        restart.wrk_id = wrk_id;
        iopCommandRegister(REQ_PCM_STREAMRESTART, &restart, sizeof(PCM_STREAM_RESTART));
    }
}

// FF3 0x002701a8 (unreferenced in FF3)
int SndPCMStreamIsUse(int wrk_id)
{
    return snd_pcm_stream_wrk[wrk_id].status != ST_STREAM_NO_USE;
}

// FF3 0x002701c8 (unreferenced in FF3). FF2's inverted assert is gone.
int SndPCMStreamIsPreload(int wrk_id)
{
    return snd_pcm_stream_wrk[wrk_id].pre_load_ok != 0;
}

// FF3 0x002701f0 (unreferenced in FF3)
void SndPCMStreamFade(int wrk_id, int target_vol, int time)
{
    SND_PCM_STREAM_WRK *wrk = &snd_pcm_stream_wrk[wrk_id];

    wrk->target_vol = (short)target_vol;
    wrk->spd        = SndGetFrameAddVol((short)target_vol, wrk->vol, time);
}
