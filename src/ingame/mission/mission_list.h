/* ==========================================================================
 *  ingame/mission/mission_list.h
 *
 *  The extra missions' lists, FF3 0x1ab180..0x1acc18 (FF3-only; file name
 *  invented, pinned 2026-10-15). Ten groups of up to five missions (24 in
 *  all): their definitions (FINDER_BANK_ENTRY, built at start-up), the save
 *  block (records, complete notices, group open flags) and the start menu's
 *  per-group list cursor. The mission menu (ingame/menu/start_menu.c) and
 *  the mission end phases (ingame/mission/mission_end.c) read them.
 * ======================================================================== */
#ifndef _INGAME_MISSION_MISSION_LIST_H
#define _INGAME_MISSION_MISSION_LIST_H

#include <stddef.h>                     // offsetof
#include "sdk/scetypes.h"

#define MISSION_GROUP_NUM   10          // groups (pages of the mission menu)
#define MISSION_PER_GROUP   5           // rows per group; rodata 0x3587a0 holds the used counts
#define MISSION_RANK_NUM    5
#define MISSION_ALL_GROUPS  11          // MissionIsAllAtLeast: every group

// MISSION_RECORD.state: 7 = never played. The others are results:
// message 100 / 0xf + state names them, 5 gets its own row sprite in the
// list (StartMenuMissionDrawRowSpr) and "every mission at 5 or more" opens
// unlock slot 32 (MissionIsComplete(1)), so 5 / 6 are the best results.
// A clear gets 5 - the first rank threshold its value meets (0 below all;
// FINDER_MODE_WRK::GetRank, ingame/mission/mission_mode.c).
#define MISSION_STATE_NONE  7
#define MISSION_STATE_TOP   5

// One extra mission's definition: the table at bss 0x3e6ca0 (0xf0 per
// group, 0x30 per mission; MissionGetDef), picked by the finder-mode work's
// first two words. Rows past a group's count have type 8 and rank -1.
struct FINDER_BANK_ENTRY            /* 0x30 */
{
    /* 0x00 */ int    type;         // clear condition: 0 / 1 points, 2 / 6 a time, 3 / 4 / 5 counts (StartMenuMissionDrawValue); 8 unused row
    /* 0x04 */ u_char plyr;         // player to start with (PlyrSetCurrent); the mission list's portrait
    /* 0x05 */ u_char _05;
    /* 0x06 */ u_short hp_rate;     // the players' HP in percent (MISSION_SETUP)
    /* 0x08 */ int    limit;        // -1, or a time in frames (group 3 row 2, type 3)
    /* 0x0c */ int    area;         // map area (MapAreaNoSet)
    /* 0x10 */ int    point;        // points a clear gives (init_StartMenu_Mission)
    /* 0x14 */ u_char item_num[6];  // ITEM_RESET (MissionItemReset): item 5, then items 0..4
    /* 0x1c */ int    rank[MISSION_RANK_NUM];  // result thresholds, hardest first (points falling, times rising)
};
static_assert(sizeof(FINDER_BANK_ENTRY) == 0x30, "FINDER_BANK_ENTRY is 0x30 bytes");
static_assert(offsetof(FINDER_BANK_ENTRY, type) == 0x00, "FINDER_BANK_ENTRY.type");
static_assert(offsetof(FINDER_BANK_ENTRY, plyr) == 0x04, "FINDER_BANK_ENTRY.plyr");
static_assert(offsetof(FINDER_BANK_ENTRY, _05) == 0x05, "FINDER_BANK_ENTRY._05");
static_assert(offsetof(FINDER_BANK_ENTRY, hp_rate) == 0x06, "FINDER_BANK_ENTRY.hp_rate");
static_assert(offsetof(FINDER_BANK_ENTRY, limit) == 0x08, "FINDER_BANK_ENTRY.limit");
static_assert(offsetof(FINDER_BANK_ENTRY, area) == 0x0c, "FINDER_BANK_ENTRY.area");
static_assert(offsetof(FINDER_BANK_ENTRY, point) == 0x10, "FINDER_BANK_ENTRY.point");
static_assert(offsetof(FINDER_BANK_ENTRY, item_num) == 0x14, "FINDER_BANK_ENTRY.item_num");
static_assert(offsetof(FINDER_BANK_ENTRY, rank) == 0x1c, "FINDER_BANK_ENTRY.rank");

// A mission's result (save block 25).
struct MISSION_RECORD               /* 0x8 */
{
    /* 0x00 */ int state;           // MISSION_STATE_*; message 100 / 0xf + state
    /* 0x04 */ int best;            // best result, -1: none yet
};
static_assert(sizeof(MISSION_RECORD) == 0x8, "MISSION_RECORD is 0x8 bytes");
static_assert(offsetof(MISSION_RECORD, best) == 0x04, "MISSION_RECORD.best");

// Save block 25 (bss 0x3e7600, 0x1c0 bytes; MissionSaveGet).
struct MISSION_SAVE                 /* 0x1c0 */
{
    /* 0x000 */ MISSION_RECORD rec[MISSION_GROUP_NUM][MISSION_PER_GROUP];
    /* 0x190 */ int complete_shown[2];          // MissionIsComplete(no)'s notice was shown
    /* 0x198 */ int group_open[MISSION_GROUP_NUM];  // [g - 1]: group g is open (group 0 always is)
};
static_assert(sizeof(MISSION_SAVE) == 0x1c0, "MISSION_SAVE is 0x1c0 bytes");
static_assert(offsetof(MISSION_SAVE, complete_shown) == 0x190, "MISSION_SAVE.complete_shown");
static_assert(offsetof(MISSION_SAVE, group_open) == 0x198, "MISSION_SAVE.group_open");

struct MENU_CSR_WRK;

MENU_CSR_WRK      *MissionCsrGetWork(void);                 // FF3 0x001ab180: the group's cursor
void               MissionCsrInit(void);                    // FF3 0x001ab1a0
void               MissionCsrUpdate(void);                  // FF3 0x001ab208
void               MissionCsrRelease(void);                 // FF3 0x001ab230
int                MissionCsrIsMovedSe(void);               // FF3 0x001ab278
void               MissionCsrGroupMove(void);               // FF3 0x001ab2a0: L2/R2, left/right page the open groups
int                MissionCsrGetNo(void);                   // FF3 0x001ab350
int                MissionCsrGetGroup(void);                // FF3 0x001ab378
void               MissionCsrSetFromModeWrk(void);          // FF3 0x001ab388
int                MissionSaveGet(void **addr);             // FF3 0x001ab3e0: {0x3e7600, 0x1c0}
int                MissionGetMsgNo(int base, int group, int no);    // FF3 0x001ab3f8: base + 6 * (earlier missions + no)
void               MissionWrkInit(void);                    // FF3 0x001ab478 (IngameInit)
FINDER_BANK_ENTRY *MissionGetDef(int group, int no);        // FF3 0x001ab538
int                MissionGetNum(int group);                // FF3 0x001ab560: rodata 0x3587a0
MISSION_RECORD    *MissionGetRecord(int group, int no);     // FF3 0x001ab578
int                MissionCompleteIsShown(int no);          // FF3 0x001ab598
int                MissionGroupGetOpen(int i);              // FF3 0x001ab5b0: flag i = group i + 1
void               MissionCompleteSetShown(int no);         // FF3 0x001ab5c8
void               MissionGroupSetOpen(int i);              // FF3 0x001ab5e8: flag i = group i + 1
int                MissionGetState(int group, int no);      // FF3 0x001ab608
int                MissionIsAllAtLeast(int group, int state);   // FF3 0x001ab628
int                MissionIsAllPlayed(void);                // FF3 0x001ab6f8
int                MissionGroupIsAllPlayed(int group);      // FF3 0x001ab718
int                MissionGroupIsOpen(int group);           // FF3 0x001ab730
int                MissionIsComplete(int no);               // FF3 0x001ab760: 0 all played, 1 all at MISSION_STATE_TOP or more

#endif /* _INGAME_MISSION_MISSION_LIST_H */
