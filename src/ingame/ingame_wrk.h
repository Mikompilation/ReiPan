/* ==========================================================================
 *  ingame/ingame_wrk.h
 *
 *  The game work (data 0x327560): chapter/progress bytes, points and best
 *  photo score, the play clock, difficulty, the current player and the lock
 *  table; plus the play-clock flags and the title-menu item-open flags.
 *  FF3 0x1cfc80..0x1d0710. FF2 kept INGAME_WRK and its methods at the head of
 *  ingame.c; FF3 split them into their own object (file name inferred; the
 *  file ends with a GPhaseCtrl atexit thunk, 0x1d06f0).
 * ======================================================================== */
#ifndef _INGAME_INGAME_WRK_H
#define _INGAME_INGAME_WRK_H

#include <stddef.h>                 // offsetof
#include "sdk/scetypes.h"
#include "album/prg/scr_play.h"     // AnimeCodeManager, AnimeCodeInterpretor (AnimeCodeManagerGame's bases)
#include "common/cmn_time.h"        // PLAY_TIME

struct PLYR_WRK;                    // ingame/plyr/player.h

// The game work, data 0x327560 (IngameWrkInit clears 0x4c bytes). Fields
// without a known role keep their offset as name. The save-slot header writer
// FF3 0x00202a80 copies play_time, b00, b02, b04, the difficulty and +0x28.
// layout: host -- plyr_wrk is a pointer (EE 4 bytes at +0x2c); the offsets
// before it match the ROM, lock_tbl follows it.
struct INGAME_WRK                   /* 0x4c on the EE */
{
    /* 0x00 */ u_char    b00;           // current player number (PlyrSetCurrent / PlyrGetCurNo)
    /* 0x01 */ u_char    b01;           // CHANGE_CHAPTER sets it
    /* 0x02 */ u_char    b02;           // 0xff after init; the chapter (CHAPTER_DISP_START)
    /* 0x03 */ u_char    b03;           // 1: EneSpeObjTblMain and ScnTestIngameMain run (else LayoutSeMain); EV_CHANGE_WORLD sets it (the world; PLYR_FLASHLIGHT_SET does nothing at 1)
    /* 0x04 */ u_char    b04;           // 0xff after init; GO_DREAM_WORLD counts it up to 98, chapter 1 clears it
    /* 0x05 */ u_char    _05[3];
    /* 0x08 */ u_int     point;         // 0..999999
    /* 0x0c */ u_int     best_score;    // highest photo score passed to IngameWrkSetBestScore
    /* 0x10 */ PLAY_TIME play_time;
    /* 0x20 */ int       difficulty;    // difficulty menu cursor
    /* 0x24 */ int       _24;           // the item table (ITEM_TABLE_CHANGE_, ITEM_RESET)
    /* 0x28 */ int       _28;           // SHOOT_DEVELOP_FILM passes it on
    /* 0x2c */ PLYR_WRK *plyr_wrk;      // current player work (one of GetPlyrWrk's three)
    /* 0x30 */ u_char    lock_tbl[0x1c];// lock table (LockTblInit/Inc/Dec, IsLockTblOn)
};
static_assert(offsetof(INGAME_WRK, b03) == 0x03, "INGAME_WRK.b03");
static_assert(offsetof(INGAME_WRK, b04) == 0x04, "INGAME_WRK.b04");
static_assert(offsetof(INGAME_WRK, point) == 0x08, "INGAME_WRK.point");
static_assert(offsetof(INGAME_WRK, best_score) == 0x0c, "INGAME_WRK.best_score");
static_assert(offsetof(INGAME_WRK, play_time) == 0x10, "INGAME_WRK.play_time");
static_assert(offsetof(INGAME_WRK, difficulty) == 0x20, "INGAME_WRK.difficulty");
static_assert(offsetof(INGAME_WRK, _28) == 0x28, "INGAME_WRK._28");
static_assert(offsetof(INGAME_WRK, plyr_wrk) == 0x2c || offsetof(INGAME_WRK, plyr_wrk) == 0x30,
              "INGAME_WRK.plyr_wrk (0x2c on the EE, pointer-aligned on the host)");
static_assert(offsetof(INGAME_WRK, lock_tbl) == offsetof(INGAME_WRK, plyr_wrk) + sizeof(PLYR_WRK *),
              "INGAME_WRK.lock_tbl follows plyr_wrk");

extern INGAME_WRK ingame_wrk;                   // data 0x327560

// Title-menu start flags (data 0x3275b0, 0xc bytes, cleared by
// TitleStartFlagClear). Word sizes unverified: readers not reconstructed yet.
struct TITLE_START_FLG              /* 0xc */
{
    /* 0x00 */ int load;            // Story_Start: 1 -> load game (68), else start menu (55)
    /* 0x04 */ int save;            // 1: phase 69 wrote a game (McSaveGame's "saved" closed); cleared when it starts
    /* 0x08 */ int mode;            // start mode; 2 = fixed difficulty 1 (IngameWrkGetDifficulty),
                                    // finder-mode work runs, no room morph draw
};
static_assert(sizeof(TITLE_START_FLG) == 0xc, "TITLE_START_FLG is 0xc bytes");
static_assert(offsetof(TITLE_START_FLG, mode) == 0x08, "TITLE_START_FLG.mode");

extern TITLE_START_FLG title_start_flg;         // data 0x3275b0

void IngameWrkInit(void);
void IngameWrkInitNotPlayData(void);
void TitleStartFlagClear(void);
int  IngameWrkSaveGet(void **addr);     // FF3 0x001cfd40: {ingame_wrk, 0x4c}, save block 27

u_int IngameWrkGetPoint(void);
void  IngameWrkAddPoint(int n);
int   IngameWrkSubPoint(int n);
void  IngameWrkPointBackup(int save);
void  IngameWrkSetBestScore(u_int score);
u_int IngameWrkGetBestScore(void);
void  IngameWrkSetDifficulty(int difficulty);
int   IngameWrkGetDifficulty(int start_mode);

void PlayTimeReset(void);
void PlayTimeCount(void);               // one frame onto the play clock unless stopped
void PlayTimeSetStop(int stop);
int  PlayTimeGetStop(void);
void IngameSetOutgame(int flg);
int  IngameIsOutgame(void);

void TitleMenuItemOpenInit(void);       // title-menu items: first two open
int  TitleMenuIsItemOpen(int item);
void TitleMenuItemOpenSet(int item, int open);
void TitleMenuItemOpenBackup(void);
void TitleMenuItemOpenMerge(void);
int  TitleMenuItemOpenSaveGet(void **addr); // FF3 0x001cfeb0: {0x3eb368, 0x10}, system save block 0

// ──────────────────────────────────────────────────────────────────────
// Model motion script: a AnimeCodeManager whose handler starts the model's motions.
// A model creates both (0x1d3a90: new AnimeCodeManagerGame at model +0x2c; 0x1d3ad0: new
// AnimeCodeInterpretorGame(model motion, script) at model +0x28 and as the player's
// handler); each frame 0x1d3958 runs AnimeCodeManagerGame::Advance and Step.
// ──────────────────────────────────────────────────────────────────────

class ModelManager;                      // graphics/mmanage.h
typedef ModelManager MDL_MOT;

// FF3 RTTI name AnimeCodeManagerGame (functions still MotScr*).
// vtable 0x341e98; 0x58 bytes on the EE (operator new by 0x1d3a90).
// layout: host -- follows AnimeCodeManager (pointers); EE offsets in the comments.
class AnimeCodeManagerGame : public AnimeCodeManager
{
public:
    AnimeCodeManagerGame();                          // FF3 0x001d0040 (MotScrCtor)
    ~AnimeCodeManagerGame() override {}              // FF3 0x002bbe28 (deleting: MotScrDelete 0x002bbe38)
    int  Step(void) override;           // FF3 0x001d0128 (MotScrStep)
    void Reset(void) override;          // FF3 0x001d0160

    void Start(int no, int start, int skip, int blend, float time, float speed);  // FF3 0x001d00a8
    void Advance();                     // FF3 0x001d00e8: time += speed * frame time scale

    /* 0x38 */ float time;              // script clock (AnimeCodeManagerGame::Advance)
    /* 0x3c */ float speed;
    /* 0x40 */ int   start;             // frame offset passed with each motion start
    /* 0x44 */ int   blend;             // set only during AnimeCodeManagerGame::Start: the first frame opcode blends in
    /* 0x48 */ int   flag[4];           // set by command opcodes of type 5, cleared every Step
};
// (GCC places time in AnimeCodeManager's host tail padding, MSVC after it.)
static_assert(offsetof(AnimeCodeManagerGame, time) >= offsetof(AnimeCodeManager, scr_no) + sizeof(int), "AnimeCodeManagerGame.time follows AnimeCodeManager");
static_assert(offsetof(AnimeCodeManagerGame, flag) == offsetof(AnimeCodeManagerGame, time) + 0x10, "AnimeCodeManagerGame.flag");

// FF3 RTTI name AnimeCodeInterpretorGame (functions still MotScrHdl*).
// vtable 0x341ec0; 0xc bytes on the EE.
// layout: host -- the vptr and both pointers are 4 bytes on the EE, 8 here.
class AnimeCodeInterpretorGame : public AnimeCodeInterpretor
{
public:
    AnimeCodeInterpretorGame(MDL_MOT *mot, AnimeCodeManagerGame *scr);    // FF3 0x001d0180 (MotScrHdlCtor)
    ~AnimeCodeInterpretorGame() override {}                  // FF3 0x002bbe68 (deleting: MotScrHdlDelete 0x002bbe78)
    void Frame(const u_short *op) override;     // FF3 0x001d0198 (MotScrHdlFrame)
    void Command(const u_short *op) override;   // FF3 0x001d0310 (MotScrHdlCommand)
    int  Query(const u_short *op) override;     // FF3 0x001d04f0 (MotScrHdlQuery)
    int  CanStep(void) override;                // FF3 0x001d04c8
    virtual void Play(short no, short mode);    // FF3 0x001d0568 (MotScrHdlPlay): command type 2
    virtual void Command3(short arg) {}         // FF3 0x002bbe58 (empty): command type 3
    virtual void Command4(short arg) {}         // FF3 0x002bbe60 (empty): command type 4

    /* 0x04 */ MDL_MOT *mot;
    /* 0x08 */ AnimeCodeManagerGame *scr;
};
static_assert(offsetof(AnimeCodeInterpretorGame, mot) == sizeof(void *), "AnimeCodeInterpretorGame.mot");
static_assert(offsetof(AnimeCodeInterpretorGame, scr) == 2 * sizeof(void *), "AnimeCodeInterpretorGame.scr");

// Phase 11 (role unknown: goes straight to phase 0x53).
int init_Phase11(void);
int one_Phase11(void);
int end_Phase11(void);

#endif /* _INGAME_INGAME_WRK_H */
