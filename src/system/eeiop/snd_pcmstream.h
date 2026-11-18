/* ==========================================================================
 *  system/eeiop/snd_pcmstream.h
 *
 *  Straight PCM streaming (FF3 snd_pcmstream.c, 0x26fd50..0x270240; FF2's
 *  snd_pcmstream.c). Two slots, each a file the IOP streams off disc and
 *  plays through the SPU2's block transfer: no SPU voice, no ring on the EE
 *  side -- only the volume ramp and the state handshake live here.
 *
 *  In FF3 only SndPCMStreamInit (SndInit) and SndPCMStreamMain (SndMain)
 *  have callers; the rest of the API is unreferenced in the ELF, so both
 *  slots stay ST_STREAM_NO_USE and SndPCMStreamMain does nothing.
 * ======================================================================== */
#ifndef _SYSTEM_EEIOP_SND_PCMSTREAM_H
#define _SYSTEM_EEIOP_SND_PCMSTREAM_H

#include <stddef.h>                     // offsetof

#include "sdk/scetypes.h"               // u_int

typedef enum
{
    SND_PCM_STREAM_OK             = 0,
    SND_PCM_STREAM_ERR_IOPSEND    = 1,  // the IOP command queue was full
    SND_PCM_STREAM_ERR_HEADER     = 2,
    SND_PCM_STREAM_ERR_NOT_USE    = 3,
    SND_PCM_STREAM_ERR_IN_USE     = 4,
    SND_PCM_STREAM_ERR_WAIT_QUEUE = 5,
} SND_PCM_STREAM_ERR;

// REQ_PCM_STREAMPLAY's payload (sent in place from the work).
typedef struct
{
    /* 0x00 */ short wrk_id;
    /* 0x02 */ short vol;
} PCM_STREAM_PLAY;
static_assert(sizeof(PCM_STREAM_PLAY) == 0x4, "PCM_STREAM_PLAY size");

// REQ_PCM_STREAMSTART's payload (sent in place from the work). FF3: 0x10
// bytes -- FF2's ended in char file_name[256] (0x110); FF3's EE sends no
// name. nchannel and offset are never written (0 from bss).
typedef struct
{
    /* 0x00 */ int   start_sector;
    /* 0x04 */ int   size;
    /* 0x08 */ short nchannel;
    /* 0x0a */ short wrk_id;
    /* 0x0c */ int   offset;
} PCM_STREAM_START;
static_assert(sizeof(PCM_STREAM_START) == 0x10, "PCM_STREAM_START size");
static_assert(offsetof(PCM_STREAM_START, wrk_id) == 0x0a, "PCM_STREAM_START.wrk_id");

// REQ_PCM_STREAMSETVOL's payload.
typedef struct
{
    /* 0x00 */ short wrk_id;
    /* 0x02 */ short vol;
} PCM_STREAM_SETVOL;
static_assert(sizeof(PCM_STREAM_SETVOL) == 0x4, "PCM_STREAM_SETVOL size");

// REQ_PCM_STREAMINIT / STOP / PAUSE / RESTART payloads: the slot alone.
typedef struct { /* 0x0 */ int wrk_id; } PCM_STREAM_INIT;
typedef struct { /* 0x0 */ int wrk_id; } PCM_STREAM_STOP;
typedef struct { /* 0x0 */ int wrk_id; } PCM_STREAM_PAUSE;
typedef struct { /* 0x0 */ int wrk_id; } PCM_STREAM_RESTART;
static_assert(sizeof(PCM_STREAM_INIT) == 0x4, "PCM_STREAM_INIT size");
static_assert(sizeof(PCM_STREAM_STOP) == 0x4, "PCM_STREAM_STOP size");
static_assert(sizeof(PCM_STREAM_PAUSE) == 0x4, "PCM_STREAM_PAUSE size");
static_assert(sizeof(PCM_STREAM_RESTART) == 0x4, "PCM_STREAM_RESTART size");

// One stream slot (data 0x333ab8, 2 x 0x28). p and s are the IOP payloads and
// are sent in place. status is what the game asked for; the IOP's own view
// comes back through GetPCMStreamWrkRet and SndPCMStreamMain reconciles them.
typedef struct
{
    /* 0x00 */ PCM_STREAM_PLAY  p;
    /* 0x04 */ PCM_STREAM_START s;
    /* 0x14 */ int              status;     // an EEIOP_STREAM_STATUS
    /* 0x18:0 */ u_int          pre_load_ok : 1;    // the IOP reached PLAYING (ring primed)
    /* 0x18:1 */ u_int          play_flg    : 1;    // SndPCMStreamPlay was sent
    /* 0x18:2 */ u_int          pause       : 1;
    /* 0x1c */ int              file_no;
    /* 0x20 */ short            vol;
    /* 0x22 */ short            target_vol;
    /* 0x24 */ short            spd;        // per-frame fade step
} SND_PCM_STREAM_WRK;                       /* 0x28 */

static_assert(sizeof(SND_PCM_STREAM_WRK) == 0x28, "SND_PCM_STREAM_WRK size");
static_assert(offsetof(SND_PCM_STREAM_WRK, s) == 0x04, "SND_PCM_STREAM_WRK.s");
static_assert(offsetof(SND_PCM_STREAM_WRK, status) == 0x14, "SND_PCM_STREAM_WRK.status");
static_assert(offsetof(SND_PCM_STREAM_WRK, file_no) == 0x1c, "SND_PCM_STREAM_WRK.file_no");
static_assert(offsetof(SND_PCM_STREAM_WRK, vol) == 0x20, "SND_PCM_STREAM_WRK.vol");
static_assert(offsetof(SND_PCM_STREAM_WRK, spd) == 0x24, "SND_PCM_STREAM_WRK.spd");

void SndPCMStreamInit(void);                                        // FF3 0x0026fd50: REQ_PCM_STREAMCREATE, reset both slots
SND_PCM_STREAM_ERR SndPCMStreamStart(int wrk_id, int file_no, int offset);         // FF3 0x0026fda0 (unreferenced)
SND_PCM_STREAM_ERR SndPCMStreamPlay(int wrk_id, int loop2, int vol, int in_time);  // FF3 0x0026fe20 (unreferenced)
void SndPCMStreamMain(void);                                        // FF3 0x0026fef0: per frame, from SndMain
SND_PCM_STREAM_ERR SndPCMStreamInitWrk(SND_PCM_STREAM_WRK *wrk, int file_no);      // FF3 0x00270028
void SndPCMStreamStop(int wrk_id);                                  // FF3 0x00270068
void SndPCMStreamAllStop(void);                                     // FF3 0x002700b0 (unreferenced)
void SndPCMStreamPause(int wrk_id);                                 // FF3 0x002700d8 (unreferenced)
void SndPCMStreamRestart(int wrk_id);                               // FF3 0x00270140 (unreferenced)
int  SndPCMStreamIsUse(int wrk_id);                                 // FF3 0x002701a8 (unreferenced)
int  SndPCMStreamIsPreload(int wrk_id);                             // FF3 0x002701c8 (unreferenced)
void SndPCMStreamFade(int wrk_id, int target_vol, int time);        // FF3 0x002701f0 (unreferenced)

#endif /* _SYSTEM_EEIOP_SND_PCMSTREAM_H */
