/* ==========================================================================
 *  system/eeiop/snd.h
 *
 *  Sound front end (FF3 snd.c, 0x26f770..0x26fd50; FF2's snd.c). SndInit
 *  brings up every other sound module and carves their work areas out of the
 *  buffer ee_iopInit hands it; SndMain pumps them once a frame. The rest is
 *  the shared plumbing the voice owners call: group volumes, mono, reverb,
 *  the volume/pan/pitch calculation and the fade-step helpers.
 * ======================================================================== */
#ifndef _SYSTEM_EEIOP_SND_H
#define _SYSTEM_EEIOP_SND_H

#include <stddef.h>                     // offsetof

#include "system/eeiop/ee_iop.h"        // EEIOP_DEF

#define VOICE_TYPE_MAX 5                // SOUND_SYS.type_vol slots (SndInit seeds all five)

// A stereo volume pair (FF2 snd_def.h).
typedef struct
{
    /* 0x0 */ short l;
    /* 0x2 */ short r;
} VOLSET;                               /* 0x4 */

static_assert(sizeof(VOLSET) == 0x4, "VOLSET size");
static_assert(offsetof(VOLSET, r) == 0x2, "VOLSET.r");

// The per-voice attribute bits of a SOUND_INFO (FF2 snd_def.h). type is the
// mixer group (SndSetGroupVolume); effect 1 = the voice carries the reverb
// send and lives on core 0 (SndBankPlay).
typedef struct
{
    /* 0x0:0 */ unsigned short effect : 1;
    /* 0x0:1 */ unsigned short type   : 3;
    /* 0x0:4 */ unsigned short s3d    : 1;  // positional: gets a 3D handle
    /* 0x0:5 */ unsigned short loop   : 1;
    /* 0x0:6 */ unsigned short core   : 1;
    /* 0x0:7 */ unsigned short male   : 1;
    /* 0x1:0 */ unsigned short dummy  : 8;
} VOICE_ATTR;                           /* 0x2 */

static_assert(sizeof(VOICE_ATTR) == 0x2, "VOICE_ATTR size");

// One record of an HXD sound header (file format, FF2 snd_def.h): where a
// sample sits in the matching BD body and how to key it.
typedef struct
{
    /* 0x00 */ int            smpl_rate;
    /* 0x04 */ int            offset;   // bytes into the BD body
    /* 0x08 */ short          pitch;
    /* 0x0a */ short          vol;
    /* 0x0c */ unsigned short adsr1;
    /* 0x0e */ unsigned short adsr2;
    /* 0x10 */ VOICE_ATTR     attr;
    /* 0x12 */ short          pan;
    /* 0x14 */ int            loopstart;    // relative to the sample start
    /* 0x18 */ int            loopend;
} SOUND_INFO;                           /* 0x1c */

static_assert(sizeof(SOUND_INFO) == 0x1c, "SOUND_INFO size");
static_assert(offsetof(SOUND_INFO, offset) == 0x4, "SOUND_INFO.offset");
static_assert(offsetof(SOUND_INFO, pitch) == 0x8, "SOUND_INFO.pitch");
static_assert(offsetof(SOUND_INFO, vol) == 0xa, "SOUND_INFO.vol");
static_assert(offsetof(SOUND_INFO, adsr1) == 0xc, "SOUND_INFO.adsr1");
static_assert(offsetof(SOUND_INFO, adsr2) == 0xe, "SOUND_INFO.adsr2");
static_assert(offsetof(SOUND_INFO, attr) == 0x10, "SOUND_INFO.attr");
static_assert(offsetof(SOUND_INFO, pan) == 0x12, "SOUND_INFO.pan");
static_assert(offsetof(SOUND_INFO, loopstart) == 0x14, "SOUND_INFO.loopstart");
static_assert(offsetof(SOUND_INFO, loopend) == 0x18, "SOUND_INFO.loopend");

// The HXD file header (file format, FF2 snd_def.h); num SOUND_INFO records
// follow it.
typedef struct
{
    /* 0x00 */ int name;
    /* 0x04 */ int version;
    /* 0x08 */ int num;                 // SOUND_INFO records after the header
    /* 0x0c */ int type;                // 0: SE bank, 1: stream
    /* 0x10 */ int size;
    /* 0x14 */ int interleave_byte;
    /* 0x18 */ int padding[2];
} HXD_HEADER;                           /* 0x20 */

static_assert(sizeof(HXD_HEADER) == 0x20, "HXD_HEADER size");
static_assert(offsetof(HXD_HEADER, num) == 0x8, "HXD_HEADER.num");
static_assert(offsetof(HXD_HEADER, type) == 0xc, "HXD_HEADER.type");
static_assert(offsetof(HXD_HEADER, size) == 0x10, "HXD_HEADER.size");
static_assert(offsetof(HXD_HEADER, interleave_byte) == 0x14, "HXD_HEADER.interleave_byte");

// Global mixer state (bss 0x411078).
typedef struct
{
    /* 0x00 */ char mono;               // SndSetMono / SndSetStereo
    /* 0x04 */ int  type_vol[VOICE_TYPE_MAX];   // 0..0x100, 0x100 = unity
} SOUND_SYS;                            /* 0x18 */

static_assert(sizeof(SOUND_SYS) == 0x18, "SOUND_SYS size");
static_assert(offsetof(SOUND_SYS, type_vol) == 0x4, "SOUND_SYS.type_vol");

// REQ_IOP_SND_INIT's payload (SndInit's stack image: 0x800/0, 0x3fff, block).
typedef struct
{
    /* 0x0 */ int media;                // 0x800 on the disc (EEIOP_DEF.media 2), else 0
    /* 0x4 */ int mvol;                 // master volume, 0x3fff
    /* 0x8 */ int stop_block;           // SPU address of the silent block (GetSPUMemory(0x10))
} IOP_SND_INIT;                         /* 0xc */

static_assert(sizeof(IOP_SND_INIT) == 0xc, "IOP_SND_INIT size");
static_assert(offsetof(IOP_SND_INIT, stop_block) == 0x8, "IOP_SND_INIT.stop_block");

// The reverb attributes inside SET_SND_EFFECT (the SDK's sceSdEffectAttr).
typedef struct
{
    /* 0x00 */ int   core;
    /* 0x04 */ int   mode;              // SD_REV_MODE_*: index into eff_use_size_tbl
    /* 0x08 */ short depth_L;
    /* 0x0a */ short depth_R;
    /* 0x0c */ int   delay;             // never written by the EE side
    /* 0x10 */ int   feedback;          // never written by the EE side
} SND_EFFECT_ATTR;                      /* 0x14 */

static_assert(sizeof(SND_EFFECT_ATTR) == 0x14, "SND_EFFECT_ATTR size");
static_assert(offsetof(SND_EFFECT_ATTR, depth_L) == 0x8, "SND_EFFECT_ATTR.depth_L");
static_assert(offsetof(SND_EFFECT_ATTR, depth_R) == 0xa, "SND_EFFECT_ATTR.depth_R");
static_assert(offsetof(SND_EFFECT_ATTR, delay) == 0xc, "SND_EFFECT_ATTR.delay");

// REQ_SET_SND_EFFECT's payload (0x1c bytes sent).
typedef struct
{
    /* 0x00 */ int             core;
    /* 0x04 */ int             end_adrs;    // SPU address of the reverb work area
    /* 0x08 */ SND_EFFECT_ATTR r_attr;
} SET_SND_EFFECT;                       /* 0x1c */

static_assert(sizeof(SET_SND_EFFECT) == 0x1c, "SET_SND_EFFECT size");
static_assert(offsetof(SET_SND_EFFECT, end_adrs) == 0x4, "SET_SND_EFFECT.end_adrs");
static_assert(offsetof(SET_SND_EFFECT, r_attr) == 0x8, "SET_SND_EFFECT.r_attr");

extern int eff_use_size_tbl[10];        // data 0x333a90: reverb work size per mode

int   sndGetNeedSize(EEIOP_DEF *def);                   // FF3 0x0026f770
void *SndInit(EEIOP_DEF *def, void *buffer);            // FF3 0x0026f7c8: returns the rest of buffer
void  SndMain(void);                                    // FF3 0x0026f918: once a frame (ee_iopMain)
void  SndSetEffect(int core, int eff_vol, int mode);    // FF3 0x0026f960
void  SndSetStereo(void);                               // FF3 0x0026fa48
void  SndSetMono(void);                                 // FF3 0x0026fa58
int   SndIsMono(void);                                  // FF3 0x0026fa68
void  SndSetGroupVolume(int type, int vol);             // FF3 0x0026fa78
int   SndGetGroupVolume(int type);                      // FF3 0x0026fa90

// Folds vol/bvol/the group volume into a stereo pair (pan 0..0x80, 0 = hard
// left) and pitch/bpitch/play_speed into a pitch; a non-null s3d (a 3D sound
// slot) scales the pair and the pitch by the slot's values.
void  SndCalcValue(int vol, int pan, int bvol, int pitch, int bpitch, int type,
                   void *s3d, VOLSET *volset, short *pPitch, float play_speed);   // FF3 0x0026faa8
short SndGetFrameAddVol(int target, int now, int time);         // FF3 0x0026fc08: per-frame step
int   SndAddFadeVol(int vol, int target_vol, int spd);          // FF3 0x0026fc50: one step, clamped
void  SndAllStop(void);                                 // FF3 0x0026fc78
int   GetFreeVoiceNum(void);                            // FF3 0x0026fc98: free SPU voices, both cores

#endif /* _SYSTEM_EEIOP_SND_H */
