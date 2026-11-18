/* ==========================================================================
 *  ingame/menu/menu_cassette.h
 *
 *  The cassette player (FF3-only, FF3 0x25a740..0x25bae0; FF2 has no
 *  counterpart, the file name is descriptive). The tapes are the items
 *  0x35..0x3b; the menu lists the ones the player holds (CASSETTE_PHF
 *  layout, MenuCsr list with a 7-row view), asks before playing, and plays
 *  the chosen tape as a kind-3 subtitle (voice + captions) while the menu
 *  slides out. Each tape played to the end is marked heard (save block 41,
 *  TAPE_CHECK). The menu runs as phase 46 Story_Cassette, or inside an event
 *  through TAPE_START, which calls the phase functions itself.
 * ======================================================================== */
#ifndef _INGAME_MENU_MENU_CASSETTE_H
#define _INGAME_MENU_MENU_CASSETTE_H

#include <stddef.h>                     // offsetof
#include "sdk/scetypes.h"

#define CASSETTE_NUM        7           // rows of cassette_tbl
#define CASSETTE_VIEW_ROWS  7           // rows the list shows at once
#define CASSETTE_CSR        0x2e        // MenuCsr id of the list
#define CASSETTE_PHASE      46          // Story_Cassette

// One tape (data 0x3630f0, 7 rows of 0xc).
struct CASSETTE_DAT                     /* 0xc */
{
    /* 0x00 */ int item;                // item id (0x35..0x3b)
    /* 0x04 */ int end_msg;             // subtitle line that marks it heard early, -1: only the end (every row)
    /* 0x08 */ int subtitle;            // kind-3 subtitle number CassettePlay requests
};
static_assert(sizeof(CASSETTE_DAT) == 0xc, "CASSETTE_DAT is 0xc bytes");
static_assert(offsetof(CASSETTE_DAT, subtitle) == 0x8, "CASSETTE_DAT.subtitle");

// The menu's state (sdata 0x37265c), shared by CassetteMenuCtrl (pad) and
// CassetteLayoutMain (draw).
enum CASSETTE_STATE
{
    CASSETTE_ST_OPEN      = 0,          // open the message window
    CASSETTE_ST_LOAD      = 1,          // layout and window loading, background darkening
    CASSETTE_ST_IN        = 2,          // layout sliding in
    CASSETTE_ST_LIST      = 3,          // choosing a tape
    CASSETTE_ST_ASK_ON    = 4,          // show the question
    CASSETTE_ST_ASK       = 5,          // "play it?"
    CASSETTE_ST_ASK_OFF   = 6,          // back to the help line
    CASSETTE_ST_PLAY_OUT  = 7,          // layout sliding out, then the tape plays
    CASSETTE_ST_CLOSE     = 8,          // layout sliding out to close
    CASSETTE_ST_PLAY      = 9,          // the tape plays
    CASSETTE_ST_PLAY_END  = 10,         // back in after playing
    CASSETTE_ST_UNDARKEN  = 11,         // background back to normal
    CASSETTE_ST_END       = 12,         // closed
};

extern const CASSETTE_DAT cassette_tbl[CASSETTE_NUM];   // data 0x3630f0
extern int cassette_heard[CASSETTE_NUM];                // bss 0x3fd060 (save block 41)

// The owned-tape list.
int *CassetteGetList(int *num);         // FF3 0x0025a740: rows of cassette_tbl, *num = count
int  CassetteListCreate(void);          // FF3 0x0025a758: the count
int  CassetteGetNum(void);              // FF3 0x0025a850
void CassettePlay(short pos);           // FF3 0x0025a860
void CassetteSubTitleStop(void);        // FF3 0x0025a8e0
int  CassettePlayIsEnd(short pos);      // FF3 0x0025a8f8
void CassetteListRelease(void);         // FF3 0x0025a9a0

// The menu (pad side).
void CassetteMenuInit(short csr);       // FF3 0x0025a9e0
int  CassetteMenuCtrl(short csr);       // FF3 0x0025aa48: 1 when closed
void CassetteMenuTerm(short csr);       // FF3 0x0025acd0

void CassetteInit(void);                // FF3 0x0025ad18: empty
int  CassetteTblGetNum(void);           // FF3 0x0025ad20: 7
int  CassetteTblGetItem(int no);        // FF3 0x0025ad28
int  CassetteTblGetEndMsg(int no);      // FF3 0x0025ad48
int  CassetteTblGetSubTitle(int no);    // FF3 0x0025ad68
int  CassetteGetHaveNum(void);          // FF3 0x0025ad88: tapes held (TAPE_NOT_HAVE)

// The layout (draw side).
void CassetteBgAlphaInit(void);         // FF3 0x0025ae10
int  CassetteBgFadeIn(void);            // FF3 0x0025ae18: 1 when dark
int  CassetteBgFadeOut(void);           // FF3 0x0025ae48: 1 when clear
void CassetteBgDraw(void);              // FF3 0x0025ae70
void CassetteSetRowHook(void);          // FF3 0x0025b030
void CassetteScrollBarDraw(void);       // FF3 0x0025b070
void CassetteScrollBarAdd(void);        // FF3 0x0025b0e8
void CassetteScrollBarDel(void);        // FF3 0x0025b110
void CassetteCsrUpdate(void);           // FF3 0x0025b128
void CassetteCsrAdd(void);              // FF3 0x0025b160
void CassetteCsrFree(void);             // FF3 0x0025b190
void CassetteHelpSet(void);             // FF3 0x0025b1a8
void CassetteHelpMode(int ask);         // FF3 0x0025b238
void CassetteMsgWinOpen(void);          // FF3 0x0025b280
int  CassetteMsgWinIsReady(void);       // FF3 0x0025b2c8
void CassetteMsgWinSetAlphaItem(int handle, int id);    // FF3 0x0025b2f0
void CassetteMsgWinClose(void);         // FF3 0x0025b318
void CassetteLayoutCreate(short csr);   // FF3 0x0025b360
void CassetteLayoutMain(void);          // FF3 0x0025b3a8
void CassetteLayoutRelease(void);       // FF3 0x0025b658

// Heard flags.
void CassetteHeardClear(void);          // FF3 0x0025b6b8: IngameInit / new game
void CassetteSetHeard(int no);          // FF3 0x0025b6e8
int  CassetteGetState(int no);          // FF3 0x0025b720: heard (TAPE_CHECK)
int  CassetteHeardSaveGet(void **addr); // FF3 0x0025b738: {0x3fd060, 0x1c}

// Phase 46 Story_Cassette (TAPE_START calls init / one / end itself).
void CassetteMenuReq(void);             // FF3 0x0025b750: unreferenced; as a phase of its own
int  init_Story_Cassette(void);         // FF3 0x0025b790
int  one_Story_Cassette(void);          // FF3 0x0025b810: 1 once closed
int  end_Story_Cassette(void);          // FF3 0x0025ba68

#endif /* _INGAME_MENU_MENU_CASSETTE_H */
