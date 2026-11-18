/* ==========================================================================
 *  ingame/map/FurnSave.c
 *
 *  The furniture save table (see FurnSave.h). FF3 0x1fa310..0x1fa768;
 *  FF3-only, written from the FF3 decompilation.
 * ======================================================================== */
#include "ingame/map/FurnSave.h"

#include "common/heapctrl.h"                    // heapCtrlMalloc / heapCtrlFree
#include "graphics/ecb.h"                       // EcbGetNum / EcbGetData
#include "ingame/map/FurnLoad.h"                // FurnNameGetNum
#include "ingame/ingame_wrk.h"                  // ingame_wrk.plyr_wrk
#include "ingame/plyr/player.h"                 // PLYR_WRK.flags, PLYR_FLG_800000
#include "system/eeiop/cd_file_dat.h"           // FURN_SAVE_ECB
#include "system/os/eecdvd.h"                   // LoadReqGetAddr
#include "system/os/sys_heap.h"                 // GetSystemHeapWrkP


static void *furn_save_buf;                                             /* sdata 371030 */

// ──────────────────────────────────────────────────────────────────────
// The save block: the whole buffer.
// FF3 0x001fa310.

int FurnSaveSaveGet(void **addr)
{
    *addr = furn_save_buf;
    return FURN_SAVE_BUF_SIZE;
}

// ──────────────────────────────────────────────────────────────────────
// The disc's default rows into the buffer (a new game, the difficulty
// select, FurnSaveAlloc).
// FF3 0x001fa320.

int FurnSaveLoad(void)
{
    LoadReqGetAddr(FURN_SAVE_ECB, furn_save_buf, nullptr);
    return 0;
}

// FF3 0x001fa348.
void *FurnSaveGetBuf(void)
{
    return furn_save_buf;
}

// FF3 0x001fa350 (no callers).
FURN_SAVE_ROW *FurnSaveGetRow(int no)
{
    return (FURN_SAVE_ROW *)EcbGetData(FurnSaveGetBuf(), no);
}

// FF3 0x001fa380 (no callers).
int FurnSaveGetNum(void)
{
    return EcbGetNum(furn_save_buf);
}

// ──────────────────────────────────────────────────────────────────────
// The row kind of an object name: "e..." effect, "f..." furniture,
// "d..." door; -1 for anything else.
// FF3 0x001fa398.

int FurnSaveGetKind(const char *name)
{
    int kind = FURN_SAVE_KIND_EFF;

    if (*name != 'e')
    {
        int  letter;

        kind = FURN_SAVE_KIND_FURN;
        if (*name < 'f')
        {
            kind   = FURN_SAVE_KIND_DOOR;
            letter = 'd';
        }
        else
        {
            letter = 'f';
        }
        if (*name != letter)
        {
            kind = -1;
        }
    }
    return kind;
}

// ──────────────────────────────────────────────────────────────────────
// The first row of (area, kind, id, label); area and label -1 match any.
// FF3 0x001fa3d8.

FURN_SAVE_ROW *FurnSaveFind(int area, short kind, short id, int label)
{
    int num = EcbGetNum(furn_save_buf);

    for (int i = 0; i < num; i++)
    {
        FURN_SAVE_ROW *row = (FURN_SAVE_ROW *)EcbGetData(furn_save_buf, i);

        if ((area == -1 || row->area == (short)area) && row->kind == kind && row->id == id)
        {
            if (label == -1 || row->label == (short)label)
            {
                return row;
            }
        }
    }
    return nullptr;
}

// FF3 0x001fa4d8.
FURN_SAVE_ROW *FurnSaveFindId(int area, short kind, short id)
{
    return FurnSaveFind(area, kind, id, -1);
}

// ──────────────────────────────────────────────────────────────────────
// The row of the object called name in area ("f012": furniture 12).
// FF3 0x001fa4f8.

FURN_SAVE_ROW *FurnSaveFindName(int area, const char *name)
{
    int kind = FurnSaveGetKind(name);
    int id;

    if (kind >= 0 && (id = FurnNameGetNum(name, 1, 3)) >= 0)
    {
        return FurnSaveFindId(area, (short)kind, (short)id);
    }
    return nullptr;
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x001fa580.

void FurnSaveSetVisible(FURN_SAVE_ROW *row, u_int mask, u_char val)
{
    if (mask & 1)
    {
        row->vis[0] = val;
    }
    if (mask & 2)
    {
        row->vis[1] = val;
    }
    if (mask & 4)
    {
        row->vis[2] = val;
    }
    if (mask & 8)
    {
        row->vis[3] = val;
    }
}

// FF3 0x001fa5c0.
u_char FurnSaveGetVisible(FURN_SAVE_ROW *row, u_int mask)
{
    if (row == nullptr)
    {
        return 0;
    }
    if (mask & 1)
    {
        return row->vis[0];
    }
    if (mask & 2)
    {
        return row->vis[1];
    }
    if (mask & 4)
    {
        return row->vis[2];
    }
    if (mask & 8)
    {
        return row->vis[3];
    }
    return 0;
}

// ──────────────────────────────────────────────────────────────────────
// Which visibility byte counts now: 2 while the player holds the camera
// up (the viewfinder shows what the eye does not), else 1.
// FF3 0x001fa618.

int FurnSaveGetVisMask(void)
{
    return (ingame_wrk.plyr_wrk->flags & PLYR_FLG_800000) ? 2 : 1;
}

// ──────────────────────────────────────────────────────────────────────
// Every row's current alpha moves 2 a frame towards its target.
// FF3 0x001fa640.

void FurnSaveMain(void)
{
    int num = EcbGetNum(furn_save_buf);

    for (int i = 0; i < num; i++)
    {
        FURN_SAVE_ROW *row = (FURN_SAVE_ROW *)EcbGetData(furn_save_buf, i);
        int            to  = row->alpha;
        int            cur = row->alpha_cur;
        int            next;

        if (to == cur)
        {
            continue;
        }
        if (to - cur < 0)
        {
            next = cur - 2;
            if (next < to)
            {
                next = to;
            }
        }
        else
        {
            next = cur + 2;
            if (to < next)
            {
                next = to;
            }
        }
        row->alpha_cur = (u_char)next;
    }
}

// ──────────────────────────────────────────────────────────────────────
// IngameInit: a fresh buffer from the system heap, filled from the disc.
// FF3 0x001fa6d0.

int FurnSaveAlloc(void)
{
    if (furn_save_buf != nullptr)
    {
        FurnSaveFree();
    }
    furn_save_buf = heapCtrlMalloc(GetSystemHeapWrkP(), FURN_SAVE_BUF_SIZE, 0, 0, 0);
    FurnSaveLoad();
    return 0;
}

// FF3 0x001fa720.
void FurnSaveFree(void)
{
    if (furn_save_buf != nullptr)
    {
        heapCtrlFree(GetSystemHeapWrkP(), furn_save_buf, 0, 0, 0);
        furn_save_buf = nullptr;
    }
}
