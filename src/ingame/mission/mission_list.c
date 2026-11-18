/* ==========================================================================
 *  ingame/mission/mission_list.c
 *
 *  The extra missions' lists, FF3 0x1ab180..0x1acc18 (FF3-only; see
 *  mission_list.h): the start menu's list cursor per group, the save block
 *  (records, complete notices, group open flags) with its queries, and the
 *  mission definitions FF3 builds at start-up.
 *
 *  FF3 reads the finder-mode work through a gp-relative cached pointer
 *  (sdata gp-0xab0); nothing depends on the cache, so calls go to the getter.
 * ======================================================================== */
#include "ingame/mission/mission_list.h"

#include <string.h>                         // memset

#include "ingame/camera/finder_camera.h"    // FINDER_MODE_WRK, GetFinderModeWrk
#include "ingame/menu/menu_csr.h"           // MENU_CSR_WRK, MenuCsrWork*
#include "outgame/option.h"                 // CsrMoveUpDownSe
#include "system/os/system.h"               // GetPALMode

#define MISSION_SE_MOVE     0x1a9b0000      // SE_BANK_DEFAULT: cursor
#define MISSION_SE_PAGE     0x1a9b0005      // SE_BANK_DEFAULT: page

// The pad bits that page the groups: right / R2 forward, left / L2 back.
#define MISSION_PAD_NEXT    0x2002
#define MISSION_PAD_PREV    0x8001

// ──────────────────────────────────────────────────────────────────────
// Works.

static int          mission_csr_group;                      // bss 0x3e6a40: the group on screen
static MENU_CSR_WRK mission_csr[MISSION_GROUP_NUM];         // bss 0x3e6a44 (0x3c each)
static MISSION_SAVE mission_save;                           // bss 0x3e7600, save block 25

// Missions per group (rodata 0x3587a0).
static const int mission_num[MISSION_GROUP_NUM] = { 4, 3, 1, 3, 1, 4, 1, 4, 2, 1 };

// ══════════════════════════════════════════════════════════════════════
// The list cursor
// ══════════════════════════════════════════════════════════════════════

// ──────────────────────────────────────────────────────────────────────
// FF3 0x001ab180.

MENU_CSR_WRK *MissionCsrGetWork(void)
{
    return &mission_csr[mission_csr_group];
}

// ──────────────────────────────────────────────────────────────────────
// One 1 x count cursor per group, group 0 on screen (StartMenuCsrInit).
// FF3 0x001ab1a0.

void MissionCsrInit(void)
{
    int group;

    mission_csr_group = 0;
    for (group = 0; group < MISSION_GROUP_NUM; group++) {
        MenuCsrWorkInitNoFilter(&mission_csr[group], 1, (short)MissionGetNum(group), 0);
    }
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x001ab208.

void MissionCsrUpdate(void)
{
    MenuCsrWorkUpdate(MissionCsrGetWork(), 0);
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x001ab230.

void MissionCsrRelease(void)
{
    MENU_CSR_WRK *wrk;

    for (wrk = mission_csr; wrk < &mission_csr[MISSION_GROUP_NUM]; wrk++) {
        MenuCsrWorkRelease(wrk);
    }
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x001ab278.

int MissionCsrIsMovedSe(void)
{
    return MenuCsrWorkIsMovedSe(MissionCsrGetWork(), MISSION_SE_MOVE);
}

// ──────────────────────────────────────────────────────────────────────
// Pages through the open groups (wrapping); a new page starts on its
// first row.
// FF3 0x001ab2a0.

void MissionCsrGroupMove(void)
{
    int open;
    int old;
    int group;

    open = 0;
    for (group = 0; group < MISSION_GROUP_NUM; group++) {
        if (MissionGroupIsOpen(group) != 0) {
            open++;
        }
    }
    old = mission_csr_group;
    mission_csr_group = CsrMoveUpDownSe(mission_csr_group, 0, open,
                                        MISSION_PAD_NEXT, MISSION_PAD_PREV, 1, MISSION_SE_PAGE);
    if (old != mission_csr_group) {
        MenuCsrWorkSetPos(MissionCsrGetWork(), 0);
    }
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x001ab350.

int MissionCsrGetNo(void)
{
    return MenuCsrWorkGetPos(MissionCsrGetWork());
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x001ab378.

int MissionCsrGetGroup(void)
{
    return mission_csr_group;
}

// ──────────────────────────────────────────────────────────────────────
// Back from a mission: the cursor on the mission just played.
// FF3 0x001ab388.

void MissionCsrSetFromModeWrk(void)
{
    FINDER_MODE_WRK *wrk;

    wrk = GetFinderModeWrk();
    mission_csr_group = wrk->mission_grp;
    MenuCsrWorkSetPos(MissionCsrGetWork(), (short)wrk->mission_no);
}

// ══════════════════════════════════════════════════════════════════════
// The save block and the definitions
// ══════════════════════════════════════════════════════════════════════

// ──────────────────────────────────────────────────────────────────────
// Save block 25: {0x3e7600, 0x1c0}.
// FF3 0x001ab3e0.

int MissionSaveGet(void **addr)
{
    *addr = &mission_save;
    return sizeof(MISSION_SAVE);
}

// ──────────────────────────────────────────────────────────────────────
// Message 0x6f holds six strings per mission, in list order.
// FF3 0x001ab3f8.

int MissionGetMsgNo(int base, int group, int no)
{
    int g;

    for (g = 0; g < group; g++) {
        base += MissionGetNum(g) * 6;
    }
    return base + no * 6;
}

// ──────────────────────────────────────────────────────────────────────
// New game: nothing played, every group but the first closed.
// FF3 0x001ab478 (IngameInit).

void MissionWrkInit(void)
{
    MISSION_RECORD *rec;
    int             group;
    int             no;
    int             i;

    memset(&mission_save, 0, sizeof(mission_save));
    mission_save.complete_shown[0] = 0;
    mission_save.complete_shown[1] = 0;
    for (i = 0; i < MISSION_GROUP_NUM; i++) {
        mission_save.group_open[i] = 0;
    }
    for (group = 0; group < MISSION_GROUP_NUM; group++) {
        for (no = 0; no < MISSION_PER_GROUP; no++) {
            rec = MissionGetRecord(group, no);
            rec->best = -1;
            rec->state = MISSION_STATE_NONE;
        }
    }
}

// ──────────────────────────────────────────────────────────────────────
// The definitions, bss 0x3e6ca0. FF3 0x001ab7a0 (MissionListStaticInit,
// the file's __static_initialization_and_destruction_0(1, 0xffff)) builds
// the table on the stack and copies it here; FF3 0x001acbf8
// (MissionListGlobalCtor, _GLOBAL__I_, the .ctors entry at 0x2e913c) calls
// it. A dynamic initializer does the same here. Times are frames at 60 Hz,
// five sixths of them in PAL mode -- but at start-up the system work is
// still NTSC (InitSysWrk), so FF3 always gets the 60 Hz values.

#define MISSION_TIME(frames)    (GetPALMode() ? (frames) * 5 / 6 : (frames))
#define MISSION_UNUSED          { 8, 0, 0, 0, 0, 0, 0, { 0, 0, 0, 0, 0, 0 }, { -1, -1, -1, -1, -1 } }

static FINDER_BANK_ENTRY mission_def[MISSION_GROUP_NUM][MISSION_PER_GROUP] =
{
//    type plyr     hp  limit area point  item_num               rank
    {   // group 0
        { 1, 0, 0, 100, -1, 5, 5000, { 0, 0, 30, 0, 0, 0 }, { 40000, 30000, 20000, 10000, 5000 } },
        { 0, 1, 0, 100, -1, 28, 5000, { 0, 0, 30, 0, 0, 0 }, { 5500, 5000, 4000, 3000, 2000 } },
        { 1, 2, 0, 100, -1, 39, 5000, { 0, 0, 30, 0, 0, 0 }, { 9500, 8000, 7000, 6000, 4000 } },
        { 2, 0, 0, 100, -1, 49, 5000, { 0, 0, 30, 0, 0, 0 }, { MISSION_TIME(900), MISSION_TIME(1200), MISSION_TIME(1800), MISSION_TIME(2700), MISSION_TIME(3600) } },
        MISSION_UNUSED,
    },
    {   // group 1
        { 2, 2, 0, 100, -1, 6, 5000, { 0, 0, 1, 0, 0, 0 }, { MISSION_TIME(750), MISSION_TIME(1200), MISSION_TIME(1800), MISSION_TIME(2700), MISSION_TIME(3600) } },
        { 1, 1, 0, 100, -1, 7, 5000, { 0, 0, 30, 0, 0, 0 }, { 30000, 28000, 25000, 20000, 15000 } },
        { 4, 0, 0, 100, -1, 48, 5000, { 0, 0, 30, 0, 0, 0 }, { 10, 11, 13, 15, 20 } },
        MISSION_UNUSED,
        MISSION_UNUSED,
    },
    {   // group 2
        { 2, 1, 0, 100, -1, 31, 10000, { 0, 0, 30, 0, 0, 0 }, { MISSION_TIME(2400), MISSION_TIME(3000), MISSION_TIME(3600), MISSION_TIME(4500), MISSION_TIME(5400) } },
        MISSION_UNUSED,
        MISSION_UNUSED,
        MISSION_UNUSED,
        MISSION_UNUSED,
    },
    {   // group 3
        { 2, 1, 0, 100, -1, 9, 10000, { 0, 0, 30, 0, 0, 0 }, { MISSION_TIME(1800), MISSION_TIME(2400), MISSION_TIME(3000), MISSION_TIME(3600), MISSION_TIME(5400) } },
        { 5, 2, 0, 100, -1, 18, 10000, { 0, 0, 30, 0, 0, 0 }, { 7, 6, 5, 4, 3 } },
        { 3, 0, 0, 100, MISSION_TIME(9000), 30, 10000, { 0, 0, 99, 0, 0, 0 }, { 30, 25, 20, 10, 5 } },
        MISSION_UNUSED,
        MISSION_UNUSED,
    },
    {   // group 4
        { 6, 2, 0, 100, -1, 10, 15000, { 0, 0, 30, 0, 0, 0 }, { MISSION_TIME(450), MISSION_TIME(600), MISSION_TIME(900), MISSION_TIME(1200), MISSION_TIME(1800) } },
        MISSION_UNUSED,
        MISSION_UNUSED,
        MISSION_UNUSED,
        MISSION_UNUSED,
    },
    {   // group 5
        { 1, 2, 0, 100, -1, 2, 15000, { 0, 0, 50, 0, 0, 0 }, { 25000, 23000, 20000, 15000, 10000 } },
        { 2, 0, 0, 100, -1, 52, 15000, { 0, 0, 30, 0, 0, 0 }, { MISSION_TIME(600), MISSION_TIME(750), MISSION_TIME(900), MISSION_TIME(1200), MISSION_TIME(1800) } },
        { 2, 1, 0, 100, -1, 6, 15000, { 0, 0, 3, 0, 0, 0 }, { MISSION_TIME(750), MISSION_TIME(1200), MISSION_TIME(1800), MISSION_TIME(2400), MISSION_TIME(3600) } },
        { 1, 2, 0, 100, -1, 11, 15000, { 0, 0, 30, 0, 0, 0 }, { 17000, 15000, 13000, 10000, 7000 } },
        MISSION_UNUSED,
    },
    {   // group 6
        { 0, 0, 0, 100, -1, 40, 20000, { 0, 0, 30, 0, 0, 0 }, { 17000, 15000, 10000, 7000, 4000 } },
        MISSION_UNUSED,
        MISSION_UNUSED,
        MISSION_UNUSED,
        MISSION_UNUSED,
    },
    {   // group 7
        { 0, 1, 0, 100, -1, 32, 20000, { 0, 0, 30, 0, 0, 0 }, { 9000, 8000, 7000, 5000, 3000 } },
        { 3, 0, 0, 100, -1, 44, 20000, { 0, 0, 99, 0, 0, 0 }, { 20, 18, 14, 10, 6 } },
        { 2, 2, 0, 100, -1, 42, 20000, { 0, 0, 50, 0, 0, 0 }, { MISSION_TIME(6600), MISSION_TIME(7200), MISSION_TIME(7800), MISSION_TIME(9000), MISSION_TIME(10800) } },
        { 4, 1, 0, 100, -1, 28, 20000, { 0, 0, 30, 0, 0, 0 }, { 2, 3, 5, 7, 10 } },
        MISSION_UNUSED,
    },
    {   // group 8
        { 2, 1, 0, 100, -1, 40, 30000, { 4, 0, 50, 10, 5, 3 }, { MISSION_TIME(9000), MISSION_TIME(9900), MISSION_TIME(10800), MISSION_TIME(12600), MISSION_TIME(14400) } },
        { 2, 2, 0, 100, -1, 40, 30000, { 4, 0, 50, 10, 5, 3 }, { MISSION_TIME(9900), MISSION_TIME(10350), MISSION_TIME(10800), MISSION_TIME(12600), MISSION_TIME(14400) } },
        MISSION_UNUSED,
        MISSION_UNUSED,
        MISSION_UNUSED,
    },
    {   // group 9
        { 2, 0, 0, 100, -1, 40, 40000, { 5, 0, 50, 10, 5, 3 }, { MISSION_TIME(13500), MISSION_TIME(14400), MISSION_TIME(15300), MISSION_TIME(16200), MISSION_TIME(19800) } },
        MISSION_UNUSED,
        MISSION_UNUSED,
        MISSION_UNUSED,
        MISSION_UNUSED,
    },
};

// ──────────────────────────────────────────────────────────────────────
// FF3 0x001ab538.

FINDER_BANK_ENTRY *MissionGetDef(int group, int no)
{
    return &mission_def[group][no];
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x001ab560.

int MissionGetNum(int group)
{
    return mission_num[group];
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x001ab578.

MISSION_RECORD *MissionGetRecord(int group, int no)
{
    return &mission_save.rec[group][no];
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x001ab598.

int MissionCompleteIsShown(int no)
{
    return mission_save.complete_shown[no];
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x001ab5b0.

int MissionGroupGetOpen(int i)
{
    return mission_save.group_open[i];
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x001ab5c8 (phase 77).

void MissionCompleteSetShown(int no)
{
    mission_save.complete_shown[no] = 1;
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x001ab5e8 (phase 77's end).

void MissionGroupSetOpen(int i)
{
    mission_save.group_open[i] = 1;
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x001ab608.

int MissionGetState(int group, int no)
{
    return MissionGetRecord(group, no)->state;
}

// ──────────────────────────────────────────────────────────────────────
// 1 when every mission of the group (MISSION_ALL_GROUPS: of every group)
// was played and, unless state is MISSION_STATE_NONE, has at least state.
// FF3 0x001ab628.

int MissionIsAllAtLeast(int group, int state)
{
    MISSION_RECORD *rec;
    int             g;
    int             num;
    int             no;

    for (g = 0; g < MISSION_GROUP_NUM; g++) {
        if (group != MISSION_ALL_GROUPS && g != group) {
            continue;
        }
        num = MissionGetNum(g);
        for (no = 0; no < num; no++) {
            rec = MissionGetRecord(g, no);
            if (rec->state == MISSION_STATE_NONE) {
                return 0;
            }
            if (state != MISSION_STATE_NONE && rec->state < state) {
                return 0;
            }
        }
    }
    return 1;
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x001ab6f8.

int MissionIsAllPlayed(void)
{
    return MissionIsAllAtLeast(MISSION_ALL_GROUPS, MISSION_STATE_NONE);
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x001ab718.

int MissionGroupIsAllPlayed(int group)
{
    return MissionIsAllAtLeast(group, MISSION_STATE_NONE);
}

// ──────────────────────────────────────────────────────────────────────
// Group 0 is always open; group g by flag g - 1.
// FF3 0x001ab730.

int MissionGroupIsOpen(int group)
{
    if (group != 0) {
        return MissionGroupGetOpen(group - 1);
    }
    return 1;
}

// ──────────────────────────────────────────────────────────────────────
// The two all-missions goals: 0 every mission played, 1 every mission at
// MISSION_STATE_TOP or more (FinderSlotOpenByMission, phase 50's notice).
// FF3 0x001ab760.

int MissionIsComplete(int no)
{
    if (no == 0) {
        return MissionIsAllPlayed();
    }
    if (no == 1) {
        return MissionIsAllAtLeast(MISSION_ALL_GROUPS, MISSION_STATE_TOP);
    }
    return 0;
}
