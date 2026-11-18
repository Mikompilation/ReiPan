/* ==========================================================================
 *  system/os/eecdvd.c
 *
 *  The game's file-load front end (see eecdvd.h). FF3 0x259680..0x2597b0;
 *  from MioPan system/os/eecdvd.c (FF2), changed where FF3 differs.
 * ======================================================================== */
#include "system/os/eecdvd.h"

#include "sdk/scetypes.h"
#include "common/utility2.h"            // GetAlignUp
#include "system/eeiop/cddat.h"         // GetFileSize
#include "system/eeiop/fileload.h"      // FileLoadReqEE / FileLoadIsEnd / AllFileLoadIsEnd

// ──────────────────────────────────────────────────────────────────────
// Queue a load of `file` to `addr` at priority 5. -1 when the file has no
// table entry (GetFileSize -1: a label) or is empty. FF3 asks GetFileSize
// twice and tail-calls FileLoadReqEE.
// FF3 0x00259680; FF2 LoadReq (priority 6, no -1 check).

int LoadReq(int file, void *addr)
{
    if (GetFileSize(file) == (u_int)-1)
    {
        return -1;
    }
    if (GetFileSize(file) == 0)
    {
        return -1;
    }
    return FileLoadReqEE(file, addr, 5, nullptr, nullptr);
}

// ──────────────────────────────────────────────────────────────────────
// As LoadReq, reporting the load id through *id when given (-1 for a missing
// or empty file) and returning the next free address: addr + the size
// rounded up to 64 bytes, so callers pack files back to back. A size of -1
// rounds to 0 there, so a missing file neither loads nor advances.
// FF3 0x002596f8; FF2 LoadReqGetAddr (priority 6).

void *LoadReqGetAddr(int file, void *addr, int *id)
{
    u_int size = GetAlignUp(GetFileSize(file), 6);
    int   ret  = -1;

    if (size != 0)
    {
        ret = FileLoadReqEE(file, addr, 5, nullptr, nullptr);
    }
    if (id != nullptr)
    {
        *id = ret;
    }
    return (u_char *)addr + size;
}

// ──────────────────────────────────────────────────────────────────────
// 1 when no load is pending.
// FF3 0x00259780; FF2 IsLoadEndAll.

int IsLoadEndAll(void)
{
    return AllFileLoadIsEnd();
}

// ──────────────────────────────────────────────────────────────────────
// 1 when load `id` finished.
// FF3 0x00259798; FF2 IsLoadEnd.

int IsLoadEnd(int id)
{
    return FileLoadIsEnd(id);
}
