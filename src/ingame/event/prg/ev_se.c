/* ==========================================================================
 *  ingame/event/prg/ev_se.c
 *
 *  Per-room event SE banks: FF3 0x19c658..0x19ca60, FF2 ev_se.c (MioPan's
 *  port is the base; see ev_se.h for what FF3 changed).
 * ======================================================================== */
#include "ingame/event/prg/ev_se.h"

#include "ingame/sound/snd_ctrl.h"              // SndSeBankLoad / IsLoaded / Release, SE_BANK_ERROR

// ──────────────────────────────────────────────────────────────────────
// Data
// ──────────────────────────────────────────────────────────────────────

static EV_SE_FILE ev_se_reg_files[EV_SE_ROOM_MAX][EV_SE_FILE_MAX];  // bss 0x3e5c30
static EV_SE_FILE ev_se_loaded[2][EV_SE_FILE_MAX];                  // bss 0x3e6590 (FF2 sb_ids)
static int        ev_se_toggle;                                     // sbss gp-0x460 (0x373090)

static_assert(sizeof(ev_se_reg_files) == 0x960, "ev_se_reg_files size (save block 9)");

// ──────────────────────────────────────────────────────────────────────
// Save block 9. FF3 0x0019c658.

int ev_seSetSave(void **addr)
{
    *addr = ev_se_reg_files;
    return sizeof(ev_se_reg_files);
}

// ──────────────────────────────────────────────────────────────────────
// One half of the loaded table: its banks released, its slots free.
// FF3 0x0019c670.

static void ev_seReleaseHalf(int half)
{
    int i;

    for (i = 0; i < EV_SE_FILE_MAX; i++)
    {
        if (ev_se_loaded[half][i].file_no != -1)
        {
            SndSeBankRelease(ev_se_loaded[half][i].file_no, 0);
        }
        ev_se_loaded[half][i].file_no = -1;
        ev_se_loaded[half][i].wait    = 0;
    }
}

// The room's slot holding file_no (-1: a free slot), else null.
// FF3 0x0019c6e8.

static EV_SE_FILE *ev_seFindFile(int room_id, int file_no)
{
    int i;

    for (i = 0; i < EV_SE_FILE_MAX; i++)
    {
        if (ev_se_reg_files[room_id][i].file_no == file_no)
        {
            return &ev_se_reg_files[room_id][i];
        }
    }

    return nullptr;
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x0019c730.

void ev_seInit(void)
{
    int i;
    int j;

    for (j = 0; j < EV_SE_FILE_MAX; j++)
    {
        for (i = 0; i < EV_SE_ROOM_MAX; i++)
        {
            ev_se_reg_files[i][j].file_no = -1;
            ev_se_reg_files[i][j].wait    = 0;
        }

        for (i = 0; i < 2; i++)
        {
            ev_se_loaded[i][j].file_no = -1;
            ev_se_loaded[i][j].wait    = 0;
        }
    }

    ev_se_toggle = 0;
}

// FF3 0x0019c7b8.

void ev_seRelease(void)
{
    ev_seReleaseHalf(0);
    ev_seReleaseHalf(1);
    ev_se_toggle = 0;
}

// The room's first free slot gets the file. FF3 still looks the file up
// first (FF2 asserted on a duplicate; the result is unused now) and, like
// FF2 without its assert, writes through null when the room is full.
// FF3 0x0019c7e0.

void ev_seRegisterFile(int room_id, int file_no, int wait)
{
    EV_SE_FILE *slot;

    ev_seFindFile(room_id, file_no);
    slot = ev_seFindFile(room_id, -1);
    slot->wait    = wait;
    slot->file_no = file_no;
}

// The file leaves the room; the slots after it move up so the used ones stay
// contiguous. Not registered: nothing (FF2 warned). FF3 0x0019c830.

void ev_seDeleteFile(int room_id, int file_no)
{
    int i;

    for (i = 0; i < EV_SE_FILE_MAX; i++)
    {
        if (ev_se_reg_files[room_id][i].file_no == file_no)
        {
            for (; i < EV_SE_FILE_MAX - 1; i++)
            {
                ev_se_reg_files[room_id][i] = ev_se_reg_files[room_id][i + 1];
            }
            ev_se_reg_files[room_id][EV_SE_FILE_MAX - 1].file_no = -1;
            ev_se_reg_files[room_id][EV_SE_FILE_MAX - 1].wait    = 0;
            return;
        }
    }
}

// The current half's banks go, the room's files are requested into it
// (a file whose request fails stays out), then the halves swap.
// FF3 0x0019c8b8.

void ev_seChangeRoom(int room_id)
{
    EV_SE_FILE *reg;
    int         i;

    ev_seReleaseHalf(ev_se_toggle);

    reg = ev_se_reg_files[room_id];
    for (i = 0; i < EV_SE_FILE_MAX; i++, reg++)
    {
        if (reg->file_no >= 0)
        {
            if (SndSeBankLoad(reg->file_no) != SE_BANK_ERROR)
            {
                ev_se_loaded[ev_se_toggle][i] = *reg;
            }
        }
    }

    ev_se_toggle = ev_se_toggle ^ 1;
}

// 1 once every waited-for file of the half just loaded is in.
// FF3 0x0019c988.

int ev_seIsReady(void)
{
    EV_SE_FILE *file;
    int         ready;
    int         i;

    for (i = 0; i < EV_SE_FILE_MAX; i++)
    {
        ready = 1;
        file  = &ev_se_loaded[ev_se_toggle ^ 1][i];
        if (file->file_no != -1 && file->wait != 0)
        {
            ready = SndSeBankIsLoaded(file->file_no);
        }
        if (ready == 0)
        {
            return 0;
        }
    }

    return 1;
}

// FF3 0x0019ca20.

void ev_seFlipToggle(void)
{
    ev_se_toggle = ev_se_toggle ^ 1;
}
