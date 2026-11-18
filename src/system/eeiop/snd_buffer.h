/* ==========================================================================
 *  system/eeiop/snd_buffer.h
 *
 *  SE voice players (FF2 snd_buffer.c; FF3 0x269fa0..0x26ad2c): the pool of
 *  48 SPU voices (24 per SPU2 core) that bank samples play through. A voice
 *  is claimed by SndBufPlay, which returns a packed id
 *  (wrk_no << 16) | play_id; every other entry point resolves that id back
 *  to its slot and does nothing once the slot has been recycled.
 * ======================================================================== */
#ifndef _SYSTEM_EEIOP_SND_BUFFER_H
#define _SYSTEM_EEIOP_SND_BUFFER_H

#include <stddef.h>                     // offsetof

#include "system/eeiop/snd.h"           // VOLSET, VOICE_ATTR
#include "system/eeiop/snd_3d.h"        // SND_3D_WRK

#define CSND_BUF_PLAY_NO_ID 0x300000    // SndBufPlay / SndBankPlay: no voice

#define VOICES_PER_SPU2_CORE    24
#define NUM_SPU2_CORES          2
#define SND_BUF_PLAYER_MAX      (VOICES_PER_SPU2_CORE * NUM_SPU2_CORES)    // 48

// SND_BUF_PLAYER.fadestop: the fade-out-then-stop handshake. SndBufFadeStop
// raises REQ; SndBufPlayMain promotes it to END once the ramp lands and stops
// the voice the frame after; SndBufStopSub parks it in WAIT, where a looping
// voice is cut with REQ_VOICE_STOP until the IOP reports its end point.
enum
{
    FADE_STOP_NONE = 0,
    FADE_STOP_REQ  = 1,
    FADE_STOP_END  = 2,
    FADE_STOP_WAIT = 3
};

// REQ_SB_PLAY payload (FF2 snd_def.h), sent to the IOP as-is from the head of
// SND_BUF_PLAYER (size 0x14 in SndBufPlay's iopCommandRegister).
typedef struct
{
    /* 0x00 */ VOLSET     vol;
    /* 0x04 */ int        adrs;         // SPU address of the sample
    /* 0x08 */ short      pitch;
    /* 0x0a */ short      adsr1;
    /* 0x0c */ short      adsr2;
    /* 0x0e */ VOICE_ATTR attr;         // FF3 sets effect, type, s3d, loop, core
    /* 0x10 */ int        voice_no;
} SOUND_BUF_PLAY;                       /* 0x14 */

static_assert(sizeof(SOUND_BUF_PLAY) == 0x14, "SOUND_BUF_PLAY size");
static_assert(offsetof(SOUND_BUF_PLAY, adrs) == 0x04, "SOUND_BUF_PLAY.adrs");
static_assert(offsetof(SOUND_BUF_PLAY, pitch) == 0x08, "SOUND_BUF_PLAY.pitch");
static_assert(offsetof(SOUND_BUF_PLAY, adsr1) == 0x0a, "SOUND_BUF_PLAY.adsr1");
static_assert(offsetof(SOUND_BUF_PLAY, adsr2) == 0x0c, "SOUND_BUF_PLAY.adsr2");
static_assert(offsetof(SOUND_BUF_PLAY, attr) == 0x0e, "SOUND_BUF_PLAY.attr");
static_assert(offsetof(SOUND_BUF_PLAY, voice_no) == 0x10, "SOUND_BUF_PLAY.voice_no");

// REQ_SB_STOP / REQ_SB_PAUSE / REQ_SB_RESTART payload (FF2 SOUND_BUF_STOP,
// _PAUSE, _RESTART: the same three bytes; FF3 sends 4).
typedef struct
{
    /* 0x0 */ char voice_no;
    /* 0x1 */ char core;
    /* 0x2 */ char padding[2];
} SOUND_BUF_VOICE;                      /* 0x4 */

static_assert(sizeof(SOUND_BUF_VOICE) == 0x4, "SOUND_BUF_VOICE size");
static_assert(offsetof(SOUND_BUF_VOICE, core) == 0x1, "SOUND_BUF_VOICE.core");

// REQ_SB_SETVOL payload (FF2 snd_def.h).
typedef struct
{
    /* 0x0 */ VOLSET vol;
    /* 0x4 */ char   voice_no;
    /* 0x5 */ char   core;
    /* 0x6 */ char   padding[2];
} SOUND_BUF_SETVOL;                     /* 0x8 */

static_assert(sizeof(SOUND_BUF_SETVOL) == 0x8, "SOUND_BUF_SETVOL size");
static_assert(offsetof(SOUND_BUF_SETVOL, voice_no) == 0x4, "SOUND_BUF_SETVOL.voice_no");
static_assert(offsetof(SOUND_BUF_SETVOL, core) == 0x5, "SOUND_BUF_SETVOL.core");

// REQ_SB_SETPITCH payload (FF2 snd_def.h).
typedef struct
{
    /* 0x0 */ short pitch;
    /* 0x2 */ char  voice_no;
    /* 0x3 */ char  core;
} SOUND_BUF_SETPITCH;                   /* 0x4 */

static_assert(sizeof(SOUND_BUF_SETPITCH) == 0x4, "SOUND_BUF_SETPITCH size");
static_assert(offsetof(SOUND_BUF_SETPITCH, voice_no) == 0x2, "SOUND_BUF_SETPITCH.voice_no");
static_assert(offsetof(SOUND_BUF_SETPITCH, core) == 0x3, "SOUND_BUF_SETPITCH.core");

// REQ_VOICE_STOP payload (FF2 snd_def.h): shorts, unlike the SB commands.
typedef struct
{
    /* 0x0 */ short voice_no;
    /* 0x2 */ short core;
} VOICE_STOP;                           /* 0x4 */

static_assert(sizeof(VOICE_STOP) == 0x4, "VOICE_STOP size");
static_assert(offsetof(VOICE_STOP, core) == 0x2, "VOICE_STOP.core");

// REQ_VOICE_LOOP_SET payload (FF2 snd_def.h): core first.
typedef struct
{
    /* 0x0 */ short core;
    /* 0x2 */ short voice_no;
    /* 0x4 */ int   loop_adrs;          // SPU address
} VOICE_LOOP_SET;                       /* 0x8 */

static_assert(sizeof(VOICE_LOOP_SET) == 0x8, "VOICE_LOOP_SET size");
static_assert(offsetof(VOICE_LOOP_SET, voice_no) == 0x2, "VOICE_LOOP_SET.voice_no");
static_assert(offsetof(VOICE_LOOP_SET, loop_adrs) == 0x4, "VOICE_LOOP_SET.loop_adrs");

// One voice (bss 0x40e2c0, stride 0x34, slot core * 24 + voice). p is the
// REQ_SB_PLAY payload and sits first; the rest is EE-side ramp state.
// layout: host -- s3d is a pointer (EE 0x14..0x18; 8-aligned at 0x18 on
// x64), so it and every later field move on the host; the offsets below
// are the ROM's, asserted before s3d. Only p goes to the IOP.
typedef struct
{
    /* 0x00 */ SOUND_BUF_PLAY p;
    /* 0x14 */ SND_3D_WRK    *s3d;          // Snd3DCreateWrk handle or nullptr
    /* 0x18 */ short          play_id;      // bumped on every claim
    /* 0x1a */ short          vol;
    /* 0x1c */ short          target_vol;
    /* 0x1e */ short          bvol;         // per-sample base volume
    /* 0x20 */ short          spd;          // volume step per frame
    /* 0x22 */ short          pitch;
    /* 0x24 */ short          bpitch;       // per-sample base pitch
    /* 0x26 */ short          target_pitch;
    /* 0x28 */ short          pspd;         // pitch step per frame
    /* 0x2a:0 */ unsigned char use      : 1;
    /* 0x2a:1 */ unsigned char s3d_free : 1;    // free s3d with the voice
    /* 0x2a:2 */ unsigned char pause    : 1;
    /* 0x2b */ char           fadestop;     // FADE_STOP_*
    /* 0x2c */ short          pan;
    /* 0x2e */ short          cnt;          // frames since the voice started
    /* 0x30 */ int            loopend_next; // loopend + 0x10 (unread in FF3)
} SND_BUF_PLAYER;                       /* 0x34 on the EE */

static_assert(offsetof(SND_BUF_PLAYER, p) == 0x00, "SND_BUF_PLAYER.p");

void SndBufInit(void);                  // FF3 0x00269fa0: from SndInit
void SndBufAllStop(void);               // FF3 0x00269fe0
void SndBufAllStopLoopSnd(void);        // FF3 0x0026a0b8: stop looping voices, release the rest
void SndBufAllPauseLoopSnd(void);       // FF3 0x0026a148: pause the looping voices (scene loads)
void SndBufAllRestartLoopSnd(void);     // FF3 0x0026a1c8: no caller found
void SndBufAllPause(void);              // FF3 0x0026a248
void SndBufAllRestart(void);            // FF3 0x0026a2d8
// Claims a free voice on core and keys a sample at SPU address adrs; returns
// the play id or CSND_BUF_PLAY_NO_ID. s3d is a SND_3D_WRK; s3d_free hands it
// over to be freed with the voice. loopstart / loopend are SPU addresses.
int  SndBufPlay(int adrs, int core, int effect, int vol, int bvol, int pitch,
                int bpitch, int pan, int fade_time, int loop, int type,
                void *s3d, int s3d_free, int adsr1, int adsr2,
                int loopstart, int loopend);    // FF3 0x0026a3d0
void SndBufPlayMain(void);              // FF3 0x0026a690: per frame, from SndMain
void SndBufStop(int id);                // FF3 0x0026a9d0
void SndBufPause(int id);               // FF3 0x0026aa70
void SndBufRestart(int id);             // FF3 0x0026ab08
void SndBufVolFade(int id, int vol, int frame);         // FF3 0x0026ab28
void SndBufPitchSet(int id, int pitch); // FF3 0x0026ab80
void SndBufFadeStop(int id, int time);  // FF3 0x0026abb0: fade to 0 over time frames, then stop
int  SndBufIsPlaying(int id);           // FF3 0x0026ac08
void SndBufSetPosition(int id, float *pos);     // FF3 0x0026ac28
void SndBufFadePitch(int id, int pitch, int time);      // FF3 0x0026ac78

#endif /* _SYSTEM_EEIOP_SND_BUFFER_H */
