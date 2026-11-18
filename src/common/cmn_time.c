/* ==========================================================================
 *  common/cmn_time.c
 *
 *  The play clock and the RTC date helpers (see cmn_time.h). FF3-only,
 *  FF3 0x25cc20..0x25d000.
 * ======================================================================== */
#include "common/cmn_time.h"

#include "system/os/system.h"           // GetPALMode

// One BCD byte -> its value.
static inline int BcdToInt(u_char bcd)
{
    return (bcd >> 4) * 10 + (bcd & 0xf);
}

// ──────────────────────────────────────────────────────────────────────
// RTC dates
// ──────────────────────────────────────────────────────────────────────

// FF3 0x0025cc20
void CdClockBcdToInt(int *out, const sceCdCLOCK *clock)
{
    out[0] = BcdToInt(clock->year);
    out[1] = BcdToInt(clock->month);
    out[2] = BcdToInt(clock->day);
    out[3] = BcdToInt(clock->hour);
    out[4] = BcdToInt(clock->minute);
    out[5] = BcdToInt(clock->second);
}

// FF3 0x0025ccd0
int CdClockReadInt(int *out)
{
    sceCdCLOCK clock;

    sceCdReadClock(&clock);
    CdClockBcdToInt(out, &clock);
    return 0;
}

// Field by field from the year: the first that differs decides.
// FF3 0x0025cd08 (no jal, data word or address pair reaches it).
const int *CdClockIntMax(const int *a, const int *b)
{
    int i;

    for (i = 0; i < CLOCK_DATE_NUM; i++)
    {
        if (a[i] > b[i])
            return a;
        if (a[i] < b[i])
            return b;
    }
    return nullptr;
}

// ──────────────────────────────────────────────────────────────────────
// The play clock
// ──────────────────────────────────────────────────────────────────────

// FF3 0x0025cdd0
const PLAY_TIME *PlayTimeMax(const PLAY_TIME *a, const PLAY_TIME *b)
{
    if (a->hour > b->hour)
        return a;
    if (a->hour < b->hour)
        return b;
    if (a->min > b->min)
        return a;
    if (a->min < b->min)
        return b;
    if (a->sec > b->sec)
        return a;
    if (a->sec < b->sec)
        return b;
    if (a->frame > b->frame)
        return a;
    if (a->frame < b->frame)
        return b;
    return nullptr;
}

// FF3 0x0025ce58 (unsigned divisions)
void PlayTimeToHms(int *hms, u_int frames)
{
    hms[0] = frames / (GetPALMode() ? 25 * 3600 : 30 * 3600);
    hms[1] = frames / (GetPALMode() ? 25 * 60 : 30 * 60) % 60;
    hms[2] = frames / (GetPALMode() ? 25 : 30) % 60;
}

// FF3 0x0025cf40
void PlayTimeAdd(PLAY_TIME *time, int frames)
{
    if (time->hour == 99 && time->min == 59 && time->sec == 59)
        return;

    time->frame += frames;
    time->sec   += time->frame / 30;
    time->frame %= 30;
    time->min   += time->sec / 60;
    time->sec   %= 60;
    time->hour  += time->min / 60;
    time->min   %= 60;

    if (time->hour > 99)
    {
        time->hour = 99;
        time->min  = 59;
        time->sec  = 59;
    }
}
