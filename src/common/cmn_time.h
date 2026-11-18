/* ==========================================================================
 *  common/cmn_time.h
 *
 *  The play clock (PLAY_TIME: hours, minutes, seconds, frames) and the
 *  RTC date helpers (a sceCdCLOCK unpacked into six ints).
 *
 *  FF3-only (no FF2 match), FF3 0x25cc20..0x25d000, between
 *  graphics/graph2d/tim2.c and outgame/title_menu.c; the name is
 *  descriptive, like FF3's other cmn_ utilities.
 * ======================================================================== */
#ifndef _COMMON_CMN_TIME_H
#define _COMMON_CMN_TIME_H

#include <stddef.h>                 // offsetof
#include "sdk/scetypes.h"
#include "sdk/libcdvd.h"            // sceCdCLOCK

// Play clock {hours, minutes, seconds, frames}; PlayTimeAdd carries frames at
// 30 and stops at 99:59:59.
struct PLAY_TIME                    /* 0x10 */
{
    /* 0x00 */ int hour;
    /* 0x04 */ int min;
    /* 0x08 */ int sec;
    /* 0x0c */ int frame;
};
static_assert(sizeof(PLAY_TIME) == 0x10, "PLAY_TIME is 0x10 bytes");
static_assert(offsetof(PLAY_TIME, frame) == 0x0c, "PLAY_TIME.frame");

// An unpacked RTC date is int[CLOCK_DATE_NUM]:
// {year (two digits), month, day, hour, minute, second}.
#define CLOCK_DATE_NUM  6

// Unpack clock's BCD fields into out[6].
void CdClockBcdToInt(int *out, const sceCdCLOCK *clock);    // FF3 0x0025cc20
// Read the RTC (sceCdReadClock) into out[6]; returns 0.
int  CdClockReadInt(int *out);                              // FF3 0x0025ccd0
// The later of two dates, nullptr when equal. No retail caller.
const int *CdClockIntMax(const int *a, const int *b);       // FF3 0x0025cd08
// The longer of two play times, nullptr when equal.
const PLAY_TIME *PlayTimeMax(const PLAY_TIME *a, const PLAY_TIME *b);  // FF3 0x0025cdd0
// A frame count -> hms[3] = {hours, minutes, seconds} at 30 frames per
// second (25 in PAL mode).
void PlayTimeToHms(int *hms, u_int frames);                 // FF3 0x0025ce58
// Add frames to time, carrying at 30 frames / 60 s / 60 min; holds at 99:59:59.
void PlayTimeAdd(PLAY_TIME *time, int frames);              // FF3 0x0025cf40

#endif /* _COMMON_CMN_TIME_H */
