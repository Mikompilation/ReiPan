/* ==========================================================================
 *  system/eeiop/stream_auto.h
 *
 *  Priority-scheduled streams (FF3 stream_auto.c, 0x26c8c0..0x26e0b0; FF2's
 *  stream_auto.c). snd_stream.c has two stream slots; this decides which of
 *  the requested streams gets one. A request returns an id, not a slot: it
 *  goes onto a wait queue sorted by priority (lower number first), and
 *  StreamAutoPlayMain moves the head of the queue into a slot as slots free
 *  up. A request that outranks a playing stream evicts it back onto the
 *  queue, where it keeps its offset and resumes later.
 *
 *  Every entry point takes the id and looks in both the play slots and the
 *  wait queue, so a caller never needs to know whether its stream started.
 *  FF3 adds a pitch to each request (StreamAutoPlayPitch / PreloadPitch,
 *  StreamAutoFadePitch) and drops FF2's debug enable switch.
 * ======================================================================== */
#ifndef _SYSTEM_EEIOP_STREAM_AUTO_H
#define _SYSTEM_EEIOP_STREAM_AUTO_H

#include <stddef.h>                     // offsetof

#include "sdk/libvu0.h"                 // sceVu0FVECTOR
#include "system/eeiop/snd.h"           // SOUND_INFO
#include "system/eeiop/snd_3d.h"        // SND_3D_SET

#define STREAM_PLAY_MAX 2               // snd_stream.c's slots

// One requested or playing stream (FF2 STREAM_QUEUE). The 3D vectors are
// copied in, so a caller's SND_3D_SET need not outlive the request.
typedef struct STREAM_QUEUE
{
    /* 0x00 */ sceVu0FVECTOR pos;
    /* 0x10 */ sceVu0FVECTOR vel;
    /* 0x20 */ sceVu0FVECTOR dir;
    /* 0x30 */ int   status;            // FF2, never touched by FF3
    /* 0x34 */ int   priority;          // lower number wins
    /* 0x38:0 */ unsigned int s3d       : 1;
    /* 0x38:1 */ unsigned int effect    : 1;
    /* 0x38:2 */ unsigned int loop      : 1;
    /* 0x38:3 */ unsigned int use       : 1;    // play slot taken
    /* 0x38:4 */ unsigned int playing   : 1;    // SndStreamPlay issued
    /* 0x38:5 */ unsigned int end       : 1;    // stopped by the caller
    /* 0x38:6 */ unsigned int resume    : 1;    // play as soon as the header is in
    /* 0x38:7 */ unsigned int pause     : 1;
    /* 0x38:8 */ unsigned int reset_flg : 1;    // evicted: restart from the top
    /* 0x38:9 */ unsigned int first_flg : 1;    // not played yet: fade in over reset_in_time
    /* 0x38:10 */ unsigned int vel_flg  : 1;
    /* 0x38:11 */ unsigned int dir_flg  : 1;
    /* 0x3c */ int   reset_in_time;
    /* 0x40 */ int   in_time;
    /* 0x44 */ int   vol;
    /* 0x48 */ int   pitch;             // FF3: 0x1000 = unchanged
    /* 0x4c */ int   file_no;
    /* 0x50 */ int   header_file_no;    // -0x21: header already loaded (loop restart)
    /* 0x54 */ int   id;
    /* 0x58 */ int   offset;            // sectors into the file
    /* 0x5c */ float play_spd;
} STREAM_QUEUE;                         /* 0x60 */

static_assert(sizeof(STREAM_QUEUE) == 0x60, "STREAM_QUEUE size");
static_assert(offsetof(STREAM_QUEUE, vel) == 0x10, "STREAM_QUEUE.vel");
static_assert(offsetof(STREAM_QUEUE, dir) == 0x20, "STREAM_QUEUE.dir");
static_assert(offsetof(STREAM_QUEUE, priority) == 0x34, "STREAM_QUEUE.priority");
static_assert(offsetof(STREAM_QUEUE, reset_in_time) == 0x3c, "STREAM_QUEUE.reset_in_time");
static_assert(offsetof(STREAM_QUEUE, in_time) == 0x40, "STREAM_QUEUE.in_time");
static_assert(offsetof(STREAM_QUEUE, vol) == 0x44, "STREAM_QUEUE.vol");
static_assert(offsetof(STREAM_QUEUE, pitch) == 0x48, "STREAM_QUEUE.pitch");
static_assert(offsetof(STREAM_QUEUE, file_no) == 0x4c, "STREAM_QUEUE.file_no");
static_assert(offsetof(STREAM_QUEUE, header_file_no) == 0x50, "STREAM_QUEUE.header_file_no");
static_assert(offsetof(STREAM_QUEUE, id) == 0x54, "STREAM_QUEUE.id");
static_assert(offsetof(STREAM_QUEUE, offset) == 0x58, "STREAM_QUEUE.offset");
static_assert(offsetof(STREAM_QUEUE, play_spd) == 0x5c, "STREAM_QUEUE.play_spd");

// The request forms differ in play_flg (start by itself once preloaded),
// reset_flg (an evicted stream restarts from the top) and the pitch.
int  StreamAutoPreload(int file_no, int header_file_no, int priority, int effect, int loop,
                       int vol, int in_time, SND_3D_SET *s3s);                  // FF3 0x0026ceb8
int  StreamAutoPlayNonReset(int file_no, int header_file_no, int priority, int effect, int loop,
                            int vol, int in_time, int reset_in_time, SND_3D_SET *s3s,
                            int start_sector);                                  // FF3 0x0026cf18
int  StreamAutoPreloadNonReset(int file_no, int header_file_no, int priority, int effect, int loop,
                               int vol, int in_time, int reset_in_time, SND_3D_SET *s3s);   // FF3 0x0026cf80
int  StreamAutoPlay(int file_no, int header_file_no, int priority, int effect, int loop,
                    int vol, int in_time, SND_3D_SET *s3s);                     // FF3 0x0026cfe0
int  StreamAutoPlayPitch(int file_no, int header_file_no, int priority, int effect, int loop,
                         int vol, int in_time, SND_3D_SET *s3s, int pitch);     // FF3 0x0026d040
int  StreamAutoPreloadPitch(int file_no, int header_file_no, int priority, int effect, int loop,
                            int vol, int in_time, SND_3D_SET *s3s, int pitch);  // FF3 0x0026d0a0

// How many of the two slots may play; 1 is the exclusive mode movies and
// scenes use. Disabling a busy slot evicts its stream over fade_time.
void StreamAutoSetPlayNum(int num, int fade_time);              // FF3 0x0026d100
void StreamAutoSetExclusiveMode(int flg, int fade_time);        // FF3 0x0026d2b0

int   StreamAutoGetOneWrkSize(void);                            // FF3 0x0026d2d8
void *StreamAutoPlayInit(void *buffer, int num);                // FF3 0x0026d2e0: returns the rest
void  StreamAutoPlayMain(void);                                 // FF3 0x0026d5c8: per frame, from SndMain

int   StreamAutoGetInfo(int id, int *num, int *interleave_byte, SOUND_INFO **info);  // FF3 0x0026d6b0
void  StreamAutoAllStop(void);                                  // FF3 0x0026d718: stop everything, empty the queue
void  StreamAutoAllPause(void);                                 // FF3 0x0026d7e0
void  StreamAutoAllRestart(void);                               // FF3 0x0026d878
void  StreamAutoFade(int id, int vol, int time);                // FF3 0x0026d920
void  StreamAutoFadePitch(int id, int pitch, int time);         // FF3 0x0026d9a8
void  StreamAutoFadeOut(int id, int time);                      // FF3 0x0026da30: stop (or drop the request)
int   StreamAutoPause(int id);                                  // FF3 0x0026dac0
int   StreamAutoRestart(int id);                                // FF3 0x0026db58
int   StreamAutoSetPosition(int id, float *pos);                // FF3 0x0026dbf8
void  PrintStreamAutoStatus(void);                              // FF3 0x0026dd08: prints compiled out
int   StreamAutoIsPreload(int id);                              // FF3 0x0026dd40
int   StreamAutoPreloadPlay(int id);                            // FF3 0x0026dd78: 1 once preloaded and started
int   StreamAutoIsAllStop(void);                                // FF3 0x0026dde8
int   StreamAutoIsPlaying(int id);                              // FF3 0x0026de30: playing or queued
int   StreamAutoChangePlaySpeed(int id, float rate);            // FF3 0x0026de78
float StreamAutoNowPlayPercentage(int id);                      // FF3 0x0026df08
int   StreamAutoGetNowSector(int id);                           // FF3 0x0026dfc0
int   StreamAutoSet3D(int id, SND_3D_SET *s3s);                 // FF3 0x0026dff8

#endif /* _SYSTEM_EEIOP_STREAM_AUTO_H */
