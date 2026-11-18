/* ==========================================================================
 *  ingame/ingame_wrk.c
 *
 *  The game work (INGAME_WRK, data 0x327560) and its accessors, the play clock,
 *  the title-menu item-open flags, the model motion script (AnimeCodeManagerGame and its
 *  handler AnimeCodeInterpretorGame, on AnimeCodeManager from album/prg/scr_play.c), and phase 11.
 *  FF3 0x1cfc80..0x1d0710.
 *
 *  FF3 0x001d06f0 is not written: the compiler's atexit thunk for the
 *  GPhaseCtrl singleton (GPhaseCtrlTerm), the file-end marker.
 * ======================================================================== */
#include "ingame/ingame_wrk.h"
#include "common/reipan_stub.h"
#include "system/mc/dat/save_data.h"

#include <string.h>                                     // memset, memmove

#include "graphics/motion/motion.h"                     // MDL_MOT, MOT_CTRL (BmdMotion)
#include "main/gphase.h"                                // GetGPhaseCtrl, GPHASE_CTRL::SetNext
#include "outgame/title_menu.h"                         // TitleMenuCtrlInit
#include "system/os/system.h"                           // GetFrameTimeScale
#include "ingame/story_menu.h"                         // LockTblInit

// ──────────────────────────────────────────────────────────────────────
// Data
// ──────────────────────────────────────────────────────────────────────

INGAME_WRK      ingame_wrk;                             // data 0x327560
TITLE_START_FLG title_start_flg;                        // data 0x3275b0

static u_int point_bak;                                 // sdata 0x3732f0
static int   play_time_stop;                            // sdata 0x370ab0
static int   ingame_outgame;                            // sdata 0x370ab4

static int title_item_open[4];                          // bss 0x3eb368
static int title_item_open_bak[4];                      // bss 0x3eb378

// ──────────────────────────────────────────────────────────────────────
// Template instance: clamp an unsigned value into [lo, hi].
// FF3 0x002bbe08 (emitted with the template instances).

static u_int ClampU32(u_int v, u_int lo, u_int hi)
{
    if (v < lo)
        return lo;
    return (hi < v) ? hi : v;
}

// ──────────────────────────────────────────────────────────────────────
// Game work
// ──────────────────────────────────────────────────────────────────────

// Fresh game work: clear everything, take the default difficulty from the
// title menu, zero the play clock and the lock table.
// FF3 0x001cfc80; FF2 IngameWrkInit (FF2 took chapter and difficulty).

void IngameWrkInit(void)
{
    memset(&ingame_wrk, 0, sizeof(ingame_wrk));
    ingame_wrk.b03 = 0;
    ingame_wrk.b04 = 0xff;
    ingame_wrk.b02 = 0xff;
    IngameWrkSetDifficulty(TitleMenuCtrlInit());
    PlayTimeReset();
    LockTblInit();
}

// ──────────────────────────────────────────────────────────────────────
// Reset the progress part of the game work but keep the play data (points,
// play clock, difficulty).
// FF3 0x001cfcd8; FF2 IngameWrkInitNotPlayData (FF2's fields differ).

void IngameWrkInitNotPlayData(void)
{
    ingame_wrk.b00 = 0;
    ingame_wrk.b04 = 0xff;
    ingame_wrk.b03 = 0;
    ingame_wrk.b01 = 0;
    ingame_wrk.b02 = 0xff;
    ingame_wrk.plyr_wrk = 0;
    ingame_wrk.best_score = 0;
    ingame_wrk._28 = 0;
    LockTblInit();
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x001cfd18; no FF2 counterpart.

void TitleStartFlagClear(void)
{
    memset(&title_start_flg, 0, sizeof(title_start_flg));
}

// ──────────────────────────────────────────────────────────────────────
// Save block 27. PC: the struct's host size (plyr_wrk is a pointer); FF3
// returns 0x4c. A load leaves the saved pointer in plyr_wrk, as on the EE:
// PlyrLoadRestore re-points it from b00 (PlyrSetCurrent).
// FF3 0x001cfd40; no FF2 counterpart.

int IngameWrkSaveGet(void **addr)
{
    *addr = &ingame_wrk;
    return sizeof(ingame_wrk);
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x001cfd58; no FF2 counterpart (FF2 kept the clock in play_data.c).

void PlayTimeReset(void)
{
    memset(&ingame_wrk.play_time, 0, sizeof(ingame_wrk.play_time));
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x001cfd80.

u_int IngameWrkGetPoint(void)
{
    return ingame_wrk.point;
}

// ──────────────────────────────────────────────────────────────────────
// Add points, saturating at 999999.
// FF3 0x001cfd90.

void IngameWrkAddPoint(int n)
{
    u_int point = 999999;

    if (ingame_wrk.point + n < 1000000)
        point = ingame_wrk.point + n;
    ingame_wrk.point = point;
}

// ──────────────────────────────────────────────────────────────────────
// Take points away. The clamp is unsigned, so taking more than there is
// wraps and clamps to 999999 (as in FF3; callers check the total first).
// FF3 0x001cfdc0.

int IngameWrkSubPoint(int n)
{
    ingame_wrk.point = ClampU32(ingame_wrk.point - n, 0, 999999);
    return 1;
}

// ──────────────────────────────────────────────────────────────────────
// save != 0: remember the points; 0: put the remembered points back.
// FF3 0x001cfe08.

void IngameWrkPointBackup(int save)
{
    if (save)
    {
        point_bak = ingame_wrk.point;
        return;
    }
    ingame_wrk.point = point_bak;
}

// ──────────────────────────────────────────────────────────────────────
// One frame onto the play clock unless it is stopped (title, saving).
// FF3 0x001cfe30; FF2 PlayData_PlayTimeCount's role.

void PlayTimeCount(void)
{
    if (play_time_stop == 0)
        PlayTimeAdd(&ingame_wrk.play_time, 1);
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x001cfe60.

void PlayTimeSetStop(int stop)
{
    play_time_stop = stop;
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x001cfe68.

int PlayTimeGetStop(void)
{
    return play_time_stop;
}

// ──────────────────────────────────────────────────────────────────────
// Set by init_Title_Mode, cleared by one_Story_NowLoading when play starts.
// FF3 0x001cfe70.

void IngameSetOutgame(int flg)
{
    ingame_outgame = flg;
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x001cfe78.

int IngameIsOutgame(void)
{
    return ingame_outgame;
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x001cfe80.

void IngameWrkSetDifficulty(int difficulty)
{
    ingame_wrk.difficulty = difficulty;
}

// ──────────────────────────────────────────────────────────────────────
// The difficulty in effect for a start mode (title_start_flg.mode): mode 2
// always plays at difficulty 1.
// FF3 0x001cfe90.

int IngameWrkGetDifficulty(int start_mode)
{
    if (start_mode == 2)
        return 1;
    return ingame_wrk.difficulty;
}

// ──────────────────────────────────────────────────────────────────────
// Title-menu item-open flags
// ──────────────────────────────────────────────────────────────────────

// The system save's only block (save_system_data block 0).
// FF3 0x001cfeb0; no FF2 counterpart.

int TitleMenuItemOpenSaveGet(void **addr)
{
    *addr = title_item_open;
    return sizeof(title_item_open);
}

// ──────────────────────────────────────────────────────────────────────
// All items closed except the first two.
// FF3 0x001cfec8; no FF2 counterpart.

void TitleMenuItemOpenInit(void)
{
    int i;

    for (i = 0; i < 4; i++)
        title_item_open[i] = 0;
    title_item_open[0] = 1;
    title_item_open[1] = 1;
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x001cff08.

int TitleMenuIsItemOpen(int item)
{
    return title_item_open[item];
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x001cff20.

void TitleMenuItemOpenSet(int item, int open)
{
    title_item_open[item] = open;
}

// ──────────────────────────────────────────────────────────────────────
// Keep the flags across a system-data load (GameDataSaveMcSystemLoadWait).
// FF3 0x001cff38.

void TitleMenuItemOpenBackup(void)
{
    memmove(title_item_open_bak, title_item_open, sizeof(title_item_open));
}

// ──────────────────────────────────────────────────────────────────────
// Items open before the load stay open after it.
// FF3 0x001cff60.

void TitleMenuItemOpenMerge(void)
{
    int i;

    for (i = 0; i < 4; i++)
        title_item_open[i] |= title_item_open_bak[i];
}

// ──────────────────────────────────────────────────────────────────────
// Best photo score
// ──────────────────────────────────────────────────────────────────────

// FF3 0x001cffa8.

void IngameWrkSetBestScore(u_int score)
{
    if (ingame_wrk.best_score < score)
        ingame_wrk.best_score = score;
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x001cffc8.

u_int IngameWrkGetBestScore(void)
{
    return ingame_wrk.best_score;
}

// ──────────────────────────────────────────────────────────────────────
// Model motion script
// ──────────────────────────────────────────────────────────────────────

// MotScrCtor2 FF3 0x001cffd8 is a byte-identical copy (GCC's base/complete
// constructor pair) of FF3 0x001d0040 (MotScrCtor).
// callcheck: ScrPlayCtor (the implicit AnimeCodeManager base constructor)

AnimeCodeManagerGame::AnimeCodeManagerGame()
{
    time  = 0.0f;
    speed = 0.0f;
    start = 0;
    blend = 0;
    for (int i = 0; i <= 3; i++)
        flag[i] = 0;
}

// ──────────────────────────────────────────────────────────────────────
// Starts script `no` skipping `skip` frame opcodes; `blend` only applies to
// the frame opcode AnimeCodeManager::Start lands on.
// FF3 0x001d00a8.

void AnimeCodeManagerGame::Start(int no, int start, int skip, int blend, float time, float speed)
{
    this->start = start;
    this->time  = time;
    this->speed = speed;
    this->blend = blend;
    AnimeCodeManager::Start(no, skip);
    this->blend = 0;
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x001d00e8.

void AnimeCodeManagerGame::Advance()
{
    time += speed * GetFrameTimeScale();
}

// ──────────────────────────────────────────────────────────────────────
// Command flags only last one step.
// FF3 0x001d0128 (MotScrStep; tail-calls ScrPlayStep).

int AnimeCodeManagerGame::Step(void)
{
    for (int i = 0; i <= 3; i++)
        flag[i] = 0;
    return AnimeCodeManager::Step();
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x001d0160 (not a function in Ghidra: vtable 0x341e98 slot 3).

void AnimeCodeManagerGame::Reset(void)
{
    time = 0.0f;
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x001d0180 (MotScrHdlCtor); MotScrHdlCtor2 FF3 0x001d0168 is a
// byte-identical copy.

AnimeCodeInterpretorGame::AnimeCodeInterpretorGame(MDL_MOT *mot, AnimeCodeManagerGame *scr)
{
    this->scr = scr;
    this->mot = mot;
}

// ──────────────────────────────────────────────────────────────────────
// Frame opcode: start motion field 0 at the script's clock, then either set
// the motion control's frame (frame_over - start, clamped to [0, length]) or,
// while AnimeCodeManagerGame::Start blends, rebuild its blend work.
// FF3 0x001d0198 (MotScrHdlFrame).

void AnimeCodeInterpretorGame::Frame(const u_short *op)
{
    AnimeCodeManagerGame *s     = scr;
    float    time  = s->time;
    int      start = s->start;
    int      blend = s->blend;

    mot->PlayMotion(ScrOpField(*op, 0), start, time, s->speed);

    MOT_CTRL *ctrl = mot->GetBoneMotion();
    if (ctrl == nullptr)
        return;

    if (blend != 0) {
        if (ctrl->blend == 0)
            ctrl->CalcBlendYaw();
        ctrl->ApplyBlendYaw();
        ctrl->CalcMotionYaw(time);
        return;
    }

    float f     = ctrl->frame_over - (float)start;
    float len   = (float)ctrl->GetFrameNum();
    float frame = 0.0f;
    if (!(f < 0.0f))
        frame = (len < f) ? len : f;
    ctrl->blend_num   = 0;
    ctrl->blend       = 0;
    ctrl->blend_frame = 0.0f;
    ctrl->SetFrame(frame);
}

// ──────────────────────────────────────────────────────────────────────
// Command opcodes: 2 = Play(field 0, field 1 or 0), 3/4 = Command3/4(field 0),
// 5 = raise flag[field 0] in the script.
// FF3 0x001d0310 (MotScrHdlCommand).

void AnimeCodeInterpretorGame::Command(const u_short *op)
{
    u_short o = *op;

    switch (ScrOpType(o)) {
    case 2: {
        short b = 0;
        if (((o >> 9) & 3) != 0)
            b = (short)ScrOpField(o, 1);
        Play((short)ScrOpField(o, 0), b);
        break;
    }
    case 3:
        Command3((short)ScrOpField(o, 0));
        break;
    case 4:
        Command4((short)ScrOpField(o, 0));
        break;
    case 5:
        scr->flag[ScrOpField(o, 0)] = 1;
        break;
    }
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x001d04f0 (MotScrHdlQuery).

int AnimeCodeInterpretorGame::Query(const u_short *op)
{
    int no = ScrOpField(*op, 0);
    return mot->GetBoneMotion()->GetMotFrameNum(no);
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x001d04c8 (not a function in Ghidra: vtable 0x341ec0 +0x14).

int AnimeCodeInterpretorGame::CanStep(void)
{
    return mot->IsMotionEnd();
}

// ──────────────────────────────────────────────────────────────────────
// Command type 2: start motion `no`; `mode` picks the flags (1/3: a4, 4: a2)
// and whether the script speed applies (2/3: speed 0, then vtable +0x4c).
// FF3 0x001d0568 (MotScrHdlPlay).

void AnimeCodeInterpretorGame::Play(short no, short mode)
{
    switch ((u_short)mode) {
    case 0:
        mot->PlayShape(no, 0, 0, 0, 0.0f, scr->speed);
        break;
    case 1:
        mot->PlayShape(no, 0, 0, 1, 0.0f, scr->speed);
        break;
    case 2:
        mot->PlayShape(no, 0, 0, 0, 0.0f, 0.0f);
        mot->SetShapeLoop(no, 1);
        break;
    case 3:
        mot->PlayShape(no, 0, 0, 1, 0.0f, 0.0f);
        mot->SetShapeLoop(no, 1);
        break;
    case 4:
        mot->PlayShape(no, 1, 0, 0, 0.0f, scr->speed);
        break;
    }
}

// ──────────────────────────────────────────────────────────────────────
// Phase 11: hands over to phase 0x53 at once (role unknown, see open-items).
// ──────────────────────────────────────────────────────────────────────

// FF3 0x001d06a0.

int init_Phase11(void)
{
    return 1;
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x001d06a8.

int end_Phase11(void)
{
    return 1;
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x001d06b0.

int one_Phase11(void)
{
    GetGPhaseCtrl()->SetNext(0x53, 0);
    return 1;
}

// ---- stubs (tools/reverse/stubgen.py) ----
