/* ==========================================================================
 *  system/playpss/audiodec.h
 *
 *  The audio half of the PSS movie player (FF2 playpss.a(audiodec.o), FF3
 *  0x272d90..0x273568): the decoder handle and what playpss.c calls.
 *  Types from MioPan's audiodec.h (FF2 debug info); FF3 keeps the 0x60 handle
 *  (audioDec at 0x413700, the sceMpeg after it at 0x413760).
 * ======================================================================== */
#ifndef _SYSTEM_PLAYPSS_AUDIODEC_H
#define _SYSTEM_PLAYPSS_AUDIODEC_H

#include <stddef.h>                     // offsetof

#include "sdk/eetypes.h"

// The "SShd" chunk at the head of a PSS audio elementary stream (FF2 type).
struct SpuStreamHeader                  /* 0x20 */
{
    /* 0x00 */ char id[4];              // "SShd"
    /* 0x04 */ int  size;
    /* 0x08 */ int  type;               // 0 PCM BE, 1 PCM LE, 2 ADPCM
    /* 0x0c */ int  rate;
    /* 0x10 */ int  ch;
    /* 0x14 */ int  interSize;
    /* 0x18 */ int  loopStart;
    /* 0x1c */ int  loopEnd;
};
static_assert(sizeof(SpuStreamHeader) == 0x20, "SpuStreamHeader is 0x20 bytes");
static_assert(offsetof(SpuStreamHeader, interSize) == 0x14, "SpuStreamHeader.interSize");

// The "SSbd" chunk that follows it, then the samples.
struct SpuStreamBody                    /* 0x8 */
{
    /* 0x00 */ char id[4];              // "SSbd"
    /* 0x04 */ int  size;
};
static_assert(sizeof(SpuStreamBody) == 0x8, "SpuStreamBody is 0x8 bytes");
static_assert(offsetof(SpuStreamBody, size) == 0x4, "SpuStreamBody.size");

// The EE-side audio ring the demuxer fills and its IOP-side twin the SPU2's
// auto-DMA plays from; the iop* members are IOP addresses. state: 0 collecting
// the header, 1 pre-filling the IOP ring, 2 playing, 3 paused (FF2).
// layout: host -- `data` is 4 bytes on the EE; EE offsets in the comments.
struct AudioDec                         /* 0x60 on the EE */
{
    /* 0x00 */ int             state;
    /* 0x04 */ SpuStreamHeader sshd;
    /* 0x24 */ SpuStreamBody   ssbd;
    /* 0x2c */ int             hdrCount;
    /* 0x30 */ u_char         *data;
    /* 0x34 */ int             put;
    /* 0x38 */ int             count;
    /* 0x3c */ int             size;
    /* 0x40 */ int             totalBytes;
    /* 0x44 */ int             iopBuff;
    /* 0x48 */ int             iopBuffSize;
    /* 0x4c */ int             iopLastPos;
    /* 0x50 */ int             iopPausePos;
    /* 0x54 */ int             totalBytesSent;
    /* 0x58 */ int             iopZero;
    /* 0x5c */ int             iopZeroSize;
};
static_assert(offsetof(AudioDec, sshd) == 0x04, "AudioDec.sshd");
static_assert(offsetof(AudioDec, ssbd) == 0x24, "AudioDec.ssbd");
static_assert(offsetof(AudioDec, hdrCount) == 0x2c, "AudioDec.hdrCount");
static_assert(offsetof(AudioDec, data) == 0x30, "AudioDec.data");

// Claim the rings and prime the SPU2 with silence (FF3 passes the same ten
// arguments as FF2: zeroBuffSize 0x800, the volume percentage, the SPU core).
int  audioDecCreate(AudioDec *ad, u_char *buff, int buffSize, int iopBuff, int iopBuffSize,
                    u_char *zeroBuff, int iopZeroBuff, int zeroBuffSize,
                    int audio_vol, int auto_dma_core);          // FF3 0x00272d90
void audioDecPause(AudioDec *ad);                               // FF3 0x00272e60
void audioDecResume(AudioDec *ad);                              // FF3 0x00272ee0
void audioDecStart(AudioDec *ad);                               // FF3 0x00272f48
void audioDecReset(AudioDec *ad);                               // FF3 0x00272f60

// The demuxer's producer side: the one or two free spans of the EE ring,
// then how many bytes were written.
void audioDecBeginPut(AudioDec *ad, u_char **ptr0, int *len0, u_char **ptr1, int *len1);  // FF3 0x00272fa8
void audioDecEndPut(AudioDec *ad, int size);                    // FF3 0x00273048
int  audioDecIsPreset(AudioDec *ad);                            // FF3 0x002730c0
int  audioDecSendToIOP(AudioDec *ad);                           // FF3 0x002730d8

#endif /* _SYSTEM_PLAYPSS_AUDIODEC_H */
