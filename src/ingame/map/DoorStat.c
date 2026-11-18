/* ==========================================================================
 *  ingame/map/DoorStat.c
 *
 *  The door table and the door states (see DoorStat.h). FF3
 *  0x154720..0x154cd0; FF3-only, written from the FF3 decompilation.
 * ======================================================================== */
#include "ingame/map/DoorStat.h"

#include <string.h>                             // memset

#include "common/heapctrl.h"                    // heapCtrlMalloc / heapCtrlFree
#include "graphics/ecb.h"                       // EcbGetNum / EcbGetData
#include "system/eeiop/cd_file_dat.h"           // DOOR_STAT_ECB
#include "system/eeiop/cddat.h"                 // GetFileSize
#include "system/os/eecdvd.h"                   // LoadReq
#include "system/os/sys_heap.h"                 // GetSystemHeapWrkP

static void *door_stat_ecb;                                             /* sdata 36dbe4 (gp-0x590c) */

// [0] the flags word (0x393748), [1 + no] the state word of ECB row no
// (0x39374c..). DoorStatGetWrk indexes from [1], so an unknown door (row
// -1) reaches the flags word, as on the EE.
static u_int door_stat_wrk[1 + DOOR_STAT_DOOR_MAX];                     /* bss 393748 */
static_assert(sizeof(door_stat_wrk) == DOOR_STAT_WRK_SIZE, "door_stat_wrk is 0x404 bytes");

// ──────────────────────────────────────────────────────────────────────
// The ids of the doors behind one map mark (MarkDoorGetRgb); their count.
// FF3 0x00154720.

int DoorGetMarkDoors(short mark, int *ids)
{
    int cnt = 0;
    int num = EcbGetNum(door_stat_ecb);

    for (int i = 0; i < num; i++)
    {
        MAP_DOOR_DAT *door = (MAP_DOOR_DAT *)EcbGetData(door_stat_ecb, i);

        if (door->dbuff == mark)
        {
            cnt++;
            *ids++ = door->id;
        }
    }
    return cnt;
}

// The same, as row numbers (no callers).
// FF3 0x001547c8.

int DoorGetMarkDoorNos(short mark, int *nos)
{
    int cnt = 0;
    int num = EcbGetNum(door_stat_ecb);

    for (int i = 0; i < num; i++)
    {
        MAP_DOOR_DAT *door = (MAP_DOOR_DAT *)EcbGetData(door_stat_ecb, i);

        if (door->dbuff == mark)
        {
            *nos++ = i;
            cnt++;
        }
    }
    return cnt;
}

// ──────────────────────────────────────────────────────────────────────
// A door's row by id, nullptr if none.
// FF3 0x00154870.

MAP_DOOR_DAT *DoorStatGetData(int id)
{
    int num = EcbGetNum(door_stat_ecb);

    for (int i = 0; i < num; i++)
    {
        MAP_DOOR_DAT *door = (MAP_DOOR_DAT *)EcbGetData(door_stat_ecb, i);

        if (door->id == id)
            return door;
    }
    return nullptr;
}

// A door's row number by id, -1 if none.
// FF3 0x001548e0.

int DoorStatGetNo(int id)
{
    int num = EcbGetNum(door_stat_ecb);

    for (int i = 0; i < num; i++)
    {
        MAP_DOOR_DAT *door = (MAP_DOOR_DAT *)EcbGetData(door_stat_ecb, i);

        if (door->id == id)
            return i;
    }
    return -1;
}

// ──────────────────────────────────────────────────────────────────────
// IngameInit: the table into a system-heap buffer (the load finishes
// with the other in-game loads).
// FF3 0x00154950.

int DoorStatLoad(void)
{
    door_stat_ecb = heapCtrlMalloc(GetSystemHeapWrkP(), GetFileSize(DOOR_STAT_ECB), 0, 0, 0);
    LoadReq(DOOR_STAT_ECB, door_stat_ecb);
    return 0;
}

// The doors 1000..1999 (the rows without an area) count as tried.
// FF3 0x001549a8.

void DoorStatSetKnownSys(void)
{
    int num = EcbGetNum(door_stat_ecb);

    for (int i = 0; i < num; i++)
    {
        MAP_DOOR_DAT *door = (MAP_DOOR_DAT *)EcbGetData(door_stat_ecb, i);

        if ((u_int)(door->id - 1000) < 1000)
            DoorStatSetKnown(door->id, 1);
    }
}

// IngameNowLoadingMain, once the loads are done.
// FF3 0x00154a18.

void DoorStatApply(void)
{
    DoorStatSetKnownSys();
}

// IngameEnd.
// FF3 0x00154a30.

void DoorStatFree(void)
{
    if (door_stat_ecb != nullptr)
        heapCtrlFree(GetSystemHeapWrkP(), door_stat_ecb, 0, 0, 0);
    door_stat_ecb = nullptr;
}

// ──────────────────────────────────────────────────────────────────────
// The save block: the whole state work.
// FF3 0x00154a78.

int DoorStatSaveGet(void **addr)
{
    *addr = door_stat_wrk;
    return DOOR_STAT_WRK_SIZE;
}

// A door's state word (an unknown id gets the flags word).
// FF3 0x00154a90.

u_int *DoorStatGetWrk(int id)
{
    return &door_stat_wrk[1 + DoorStatGetNo(id)];
}

// FF3 0x00154ac0.
void DoorStatSetNoUse(int id, u_int on)
{
    u_int *stat = DoorStatGetWrk(id);

    *stat = (*stat & ~DOOR_STAT_NOUSE) | ((on & 1) << 4);
}

// FF3 0x00154b00.
int DoorStatIsNoUse(int id)
{
    return (*DoorStatGetWrk(id) & DOOR_STAT_NOUSE) ? -1 : 0;
}

// FF3 0x00154b38.
void DoorStatSetLock(int id, u_int lock)
{
    u_int *stat = DoorStatGetWrk(id);

    *stat = (*stat & ~DOOR_STAT_LOCK_MASK) | (lock & DOOR_STAT_LOCK_MASK);
}

// FF3 0x00154b78.
int DoorStatGetLock(int id)
{
    return *DoorStatGetWrk(id) & DOOR_STAT_LOCK_MASK;
}

// FF3 0x00154bb0.
void DoorStatSetKnown(int id, int on)
{
    u_int *stat = DoorStatGetWrk(id);

    *stat = (*stat & ~DOOR_STAT_KNOWN) | ((u_int)(on != 0) << 3);
}

// FF3 0x00154bf0.
int DoorStatIsKnown(int id)
{
    return (*DoorStatGetWrk(id) & DOOR_STAT_KNOWN) ? -1 : 0;
}

// Every door untried again (the world changes).
// FF3 0x00154c28.

void DoorStatClearKnownAll(void)
{
    for (int i = 0; i < DOOR_STAT_DOOR_MAX; i++)
        door_stat_wrk[1 + i] &= ~DOOR_STAT_KNOWN;
}

// FF3 0x00154c90.
u_int DoorStatGetFlag(u_int mask)
{
    return door_stat_wrk[0] & mask;
}

// FF3 0x00154ca0.
void DoorStatClear(void)
{
    memset(door_stat_wrk, 0, DOOR_STAT_WRK_SIZE);
}

// FF3 0x00154cc8.
void DoorStatEnd(void)
{
}
