/* ==========================================================================
 *  ingame/event/telephone.h
 *
 *  The telephone (FF3-only, FF3 0x25bae0..0x25c300; no FF2 code). The event
 *  script rings it (TEL_START ring, pause: ring for 'ring' seconds, stay
 *  silent 'pause' seconds, again, until TEL_STOP; ring 0 rings until
 *  stopped, pause 0 rings once), sets how loud the player hears it
 *  (TEL_VOL_OFF / TEL_VOL_ON, and story_door.c as the player changes area)
 *  and answers it (TEL_TALK_START no: a TelTalkScript plays the call's voice
 *  and lines, tel_talk.h; TEL_TALK_END waits for it). TEL_STATUS tests the
 *  status bits. The ring is two SEs of bank 0x1b7d at the phone's position
 *  0x372698 (84.3, -17.1, 230.8): 0x1b7d0001 near, 0x1b7d0000 far; the
 *  player's area picks a level 0..3 whose volumes come from tel_ring_vol.
 *  The ring work 0x3fd0e8 goes into the save data (row 42) and restarts
 *  the ring after a load.
 *
 *  Was guessed ingame/event/ev_ambient_se.c; 0x25c300 is tim2.c's
 *  Tim2CalcBufWidth, swallowed by TelTalkIsEnd in Ghidra.
 * ======================================================================== */
#ifndef _INGAME_EVENT_TELEPHONE_H
#define _INGAME_EVENT_TELEPHONE_H

#include <stddef.h>                     // offsetof
#include "sdk/scetypes.h"
#include "sdk/libvu0.h"                 // sceVu0FVECTOR
#include "system/eeiop/snd_3d.h"        // SND_3D_SET

// TelSetStatus / TelStatusCheck (TEL_STATUS mask) bits.
#define TEL_ST_IDLE   0x1               // TelInit: not ringing
#define TEL_ST_PAUSE  0x2               // silent between two rings
#define TEL_ST_RING   0x4               // ringing

// TEL_RING_SE_WRK.bank
#define TEL_BANK_NONE 0                 // no ring requested
#define TEL_BANK_WAIT 1                 // TelStart: waiting for the SE banks
#define TEL_BANK_OK   2                 // banks 0x1b7d / 0x1bf5 / 0x1bf7 loaded

// The ring SEs (bss 0x3fd090).
// layout: host -- s3s holds three EE pointers (0xc bytes at +0x00); the
// EE offsets are in the comments.
struct TEL_RING_SE_WRK                  /* 0x40 on the EE */
{
    /* 0x00 */ SND_3D_SET    s3s;       // s3s.pos = pos; vel / dir null
    /* 0x10 */ sceVu0FVECTOR pos;       // the phone (data 0x372698)
    /* 0x20 */ int           bank;      // TEL_BANK_*
    /* 0x24 */ int           se_near;   // SndSePlay id of 0x1b7d0001, -1: none
    /* 0x28 */ int           se_far;    // SndSePlay id of 0x1b7d0000, -1: none
    /* 0x2c */ int           level;     // 0..3 from the player's area, -1: not set yet
    /* 0x30 */ int           level_set; // the level the SEs play at, -1: none
};

// The ring / pause cycle (bss 0x3fd0d0; no pointers).
struct TEL_RING_WRK
{
    /* 0x00 */ int len;                 // frames of this ring, 0: over
    /* 0x04 */ int cnt;                 // frames left; the SEs start when cnt == len
    /* 0x08 */ int endless;             // 1: rings until TEL_STOP (ring seconds 0)
    /* 0x0c */ int pause;               // seconds between rings, 0: ring once
    /* 0x10 */ int pause_cnt;           // frames left of the pause
};
static_assert(sizeof(TEL_RING_WRK) == 0x14, "TEL_RING_WRK is 0x14 bytes");
static_assert(offsetof(TEL_RING_WRK, cnt) == 0x04, "TEL_RING_WRK.cnt");
static_assert(offsetof(TEL_RING_WRK, endless) == 0x08, "TEL_RING_WRK.endless");
static_assert(offsetof(TEL_RING_WRK, pause) == 0x0c, "TEL_RING_WRK.pause");
static_assert(offsetof(TEL_RING_WRK, pause_cnt) == 0x10, "TEL_RING_WRK.pause_cnt");

// What TEL_START asked for (bss 0x3fd0e8; save data row 42, 0xc bytes).
struct TEL_SAVE
{
    /* 0x0 */ int ring;                 // seconds per ring
    /* 0x4 */ int pause;                // seconds between rings
    /* 0x8 */ int active;               // 1 from TEL_START to the end of the ringing
};
static_assert(sizeof(TEL_SAVE) == 0xc, "TEL_SAVE is 0xc bytes");
static_assert(offsetof(TEL_SAVE, pause) == 0x4, "TEL_SAVE.pause");
static_assert(offsetof(TEL_SAVE, active) == 0x8, "TEL_SAVE.active");

// The talk script of a TEL_TALK_START number (data 0x363210).
struct TEL_TALK_TBL
{
    /* 0x0 */ int file;                 // the voice stream file (0x1185), -1: none
    /* 0x4 */ int script;               // index into tel_talk_script
};
static_assert(sizeof(TEL_TALK_TBL) == 0x8, "TEL_TALK_TBL is 0x8 bytes");
static_assert(offsetof(TEL_TALK_TBL, script) == 0x4, "TEL_TALK_TBL.script");

void TelRingSetArea(int area);          // FF3 0x0025bb28: level = the area's (TEL_VOL_OFF: -1 -> 0)
void TelRingRaiseArea(int area);        // FF3 0x0025bb50: level raised to the area's (unless -1)
void TelRingLowerArea(int area);        // FF3 0x0025bba0: level lowered to the area's (unless -1)
void TelTalkEndSePlay(void);            // FF3 0x0025be78: the hang-up SE 0x1bf70004 (talk command 3)
int  TelStatusCheck(u_char mask);       // FF3 0x0025bec0: status & mask (TEL_STATUS)
void TelInit(void);                     // FF3 0x0025c0d0: IngameInit
void TelLoadRestore(void);              // FF3 0x0025c100: rings again after a load if it was ringing
void TelStart(int ring, int pause);     // FF3 0x0025c138: TEL_START
int  TelMain(void);                     // FF3 0x0025c180: IngameMain; 1 when the ringing ended this frame
void TelStop(void);                     // FF3 0x0025c1c8: TEL_STOP
int  TelGetSaveData(void **addr);       // FF3 0x0025c1f0: {0x3fd0e8, 0xc}
void TelTalkClear(void);                // FF3 0x0025c208: TEL_TALK_STOP, IngameInit
void TelTalkStart(int no);              // FF3 0x0025c210: TEL_TALK_START
int  TelTalkMain(void);                 // FF3 0x0025c2a8: IngameMain; 1 when no talk runs
int  TelTalkIsEnd(void);                // FF3 0x0025c2f0: TEL_TALK_END

#endif /* _INGAME_EVENT_TELEPHONE_H */
