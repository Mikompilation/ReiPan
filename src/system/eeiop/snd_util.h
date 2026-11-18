/* ==========================================================================
 *  system/eeiop/snd_util.h
 *
 *  Fire-and-forget bank playback (FF2 snd_util.c; FF3 0x2705e0..0x270a48).
 *  snd_utilAutoBDPlay loads a BD/header pair into a new bank, plays its
 *  sample 0 once the load is in and releases the bank when the voice stops;
 *  snd_utilAutoRelease hands an already playing voice's bank over for the
 *  same release. The work array comes out of SndInit's buffer.
 * ======================================================================== */
#ifndef _SYSTEM_EEIOP_SND_UTIL_H
#define _SYSTEM_EEIOP_SND_UTIL_H

#include <stddef.h>                     // offsetof

#include "sdk/libvu0.h"                 // sceVu0FVECTOR
#include "system/eeiop/snd_3d.h"        // SND_3D_SET

// snd_utilAutoBDPlay / snd_utilAutoRelease results.
enum AUTO_BD_ERR
{
    AUTO_BD_ERR_OK     = 0,
    AUTO_BD_ERR_NO_WRK = 1,             // all auto works busy
};

enum AUTO_BD_MODE
{
    AUTO_BD_MODE_NO_USE       = 0,
    AUTO_BD_MODE_MEM_WAIT     = 1,      // FF2's, never set
    AUTO_BD_MODE_LOAD_WAIT    = 2,      // bank loading; play when ready
    AUTO_BD_MODE_PLAYEND_WAIT = 3,      // voice playing; release the bank when it ends
};

// One auto play (FF2 AUTO_BD_WRK, same layout in FF3). The 3D vectors are
// copied in, so the caller's SND_3D_SET need not outlive the request.
typedef struct AUTO_BD_WRK
{
    /* 0x00 */ int   mode;              // AUTO_BD_MODE
    /* 0x04 */ int   file_no;           // FF2's, never written
    /* 0x08 */ int   header_file_no;    // FF2's, never written
    /* 0x0c */ int   bank_no;
    /* 0x10 */ int   play_id;           // SndBankPlay's voice id
    /* 0x14 */ short pitch;
    /* 0x16 */ short vol;
    /* 0x18 */ char  padding0[8];
    /* 0x20 */ sceVu0FVECTOR pos;
    /* 0x30 */ sceVu0FVECTOR vel;
    /* 0x40 */ sceVu0FVECTOR dir;
    /* 0x50 */ int   in_time;
    /* 0x54:0 */ unsigned int s3d    : 1;
    /* 0x54:1 */ unsigned int effect : 1;
    /* 0x54:2 */ unsigned int loop   : 1;
} AUTO_BD_WRK;                          /* 0x60 */

static_assert(sizeof(AUTO_BD_WRK) == 0x60, "AUTO_BD_WRK size");
static_assert(offsetof(AUTO_BD_WRK, bank_no) == 0x0c, "AUTO_BD_WRK.bank_no");
static_assert(offsetof(AUTO_BD_WRK, play_id) == 0x10, "AUTO_BD_WRK.play_id");
static_assert(offsetof(AUTO_BD_WRK, pitch) == 0x14, "AUTO_BD_WRK.pitch");
static_assert(offsetof(AUTO_BD_WRK, vol) == 0x16, "AUTO_BD_WRK.vol");
static_assert(offsetof(AUTO_BD_WRK, pos) == 0x20, "AUTO_BD_WRK.pos");
static_assert(offsetof(AUTO_BD_WRK, vel) == 0x30, "AUTO_BD_WRK.vel");
static_assert(offsetof(AUTO_BD_WRK, dir) == 0x40, "AUTO_BD_WRK.dir");
static_assert(offsetof(AUTO_BD_WRK, in_time) == 0x50, "AUTO_BD_WRK.in_time");

int   snd_utilGetOneWrkSize(void);                      // 0x2705e0: 0x60
void *snd_utilAutoBDInit(void *buffer, int num);        // 0x2705e8: returns the rest of buffer
void  snd_utilAutoBDMain(void);                         // 0x270860: per frame (SndMain)

// Hand a playing voice's bank over; it is released when the voice ends.
// AUTO_BD_ERR_OK, or AUTO_BD_ERR_NO_WRK (the caller keeps the bank).
int   snd_utilAutoRelease(int snd_buf_id, int bank_id);                     // 0x270650
// Load a bank, play its sample 0 and release the bank when it ends.
int   snd_utilAutoBDPlay(int file_no, int header_file_no, int effect, int loop,
                         int vol, int pitch, int in_time, SND_3D_SET *s3s); // 0x2706c0
int   GetVacantAutoBDWrk(void);                         // 0x270a08: free work or -1

#endif /* _SYSTEM_EEIOP_SND_UTIL_H */
