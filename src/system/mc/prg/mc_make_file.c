/* ==========================================================================
 *  system/mc/prg/mc_make_file.c
 *
 *  Create one card file and write it in one go (FF3 0x201bf0..0x201d80):
 *  open with create, write, close. From MioPan system/mc/prg/mc_make_file.c
 *  (FF2), the same code in FF3 without the assert on a bad step (the
 *  default case just returns 0). Used by mc_icon.c, mc_icon_sys.c and
 *  mc_make_all_file.c.
 * ======================================================================== */
#include "system/mc/prg/mc_make_file.h"

#include <stddef.h>
#include <string.h>

#include "system/mc/prg/mc_close.h"         // MemoryCardFileClose*
#include "system/mc/prg/mc_open.h"          // MemoryCardFileOpen*
#include "system/mc/prg/mc_write.h"         // MemoryCardFileWrite*

// layout: host -- data_addr is a pointer (EE 4 bytes at 0x14; name at 0x18, struct 0x50).
struct MC_MAKE_FILE_CTRL                    /* 0x50, bss 0x3f5d20 */
{
    /* 0x00 */ char  step;                  // 0 open, 1 wait open, 2 write, 3 wait write, 4 close, 5 wait close
    /* 0x04 */ int   port;
    /* 0x08 */ int   slot;
    /* 0x0c */ int   size;
    /* 0x10 */ int   fd;                    // -1 until the open succeeds
    /* 0x14 */ void *data_addr;
    /* 0x18 */ char  name[0x37];            // full path, e.g. "/<dir>/icon.sys"
};
static_assert(offsetof(MC_MAKE_FILE_CTRL, port) == 0x04, "MC_MAKE_FILE_CTRL.port");
static_assert(offsetof(MC_MAKE_FILE_CTRL, slot) == 0x08, "MC_MAKE_FILE_CTRL.slot");
static_assert(offsetof(MC_MAKE_FILE_CTRL, size) == 0x0c, "MC_MAKE_FILE_CTRL.size");
static_assert(offsetof(MC_MAKE_FILE_CTRL, fd) == 0x10, "MC_MAKE_FILE_CTRL.fd");

static MC_MAKE_FILE_CTRL mc_make_file_ctrl; // bss 0x3f5d20

// FF3 0x00201bf0; FF2 MemoryCardMakeNewFileInit 0x001e0fe8.
void MemoryCardMakeNewFileInit(int port, int slot, char *name, void *data_addr, int size)
{
    mc_make_file_ctrl.step = 0;
    mc_make_file_ctrl.fd = -1;
    mc_make_file_ctrl.port = port;
    mc_make_file_ctrl.slot = slot;
    mc_make_file_ctrl.size = size;
    mc_make_file_ctrl.data_addr = data_addr;

    memset(mc_make_file_ctrl.name, 0, sizeof(mc_make_file_ctrl.name));
    strcpy(mc_make_file_ctrl.name, name);
}

// FF3 0x00201c58; FF2 MemoryCardMakeNewFileMain 0x001e1058.
// 1 written and closed, 0 busy, < 0 the open/write/close error.
int MemoryCardMakeNewFileMain(void)
{
    int mc_res;

    switch (mc_make_file_ctrl.step)
    {
    case 0:
        // 0x203 = SCE_CREAT | SCE_RDWR
        MemoryCardFileOpenInit(mc_make_file_ctrl.port, mc_make_file_ctrl.slot, mc_make_file_ctrl.name, 0x203);
        mc_make_file_ctrl.step = 1;
        // fall through
    case 1:
        mc_res = MemoryCardFileOpenMain(&mc_make_file_ctrl.fd);
        if (mc_res == 1)
        {
            mc_make_file_ctrl.step = 2;
            return 0;
        }
        break;

    case 2:
        MemoryCardFileWriteInit(mc_make_file_ctrl.fd, mc_make_file_ctrl.data_addr, mc_make_file_ctrl.size);
        mc_make_file_ctrl.step = 3;
        // fall through
    case 3:
        mc_res = MemoryCardFileWriteMain();
        if (mc_res == 1)
        {
            mc_make_file_ctrl.step = 4;
            return 0;
        }
        break;

    case 4:
        MemoryCardFileCloseInit(mc_make_file_ctrl.fd);
        mc_make_file_ctrl.step = 5;
        // fall through
    case 5:
        mc_res = MemoryCardFileCloseMain();
        if (mc_res == 1)
        {
            return 1;
        }
        break;

    default:                                // FF2 asserted here
        return 0;
    }

    return mc_res < 0 ? mc_res : 0;
}
