/* ==========================================================================
 *  system/mc/prg/mc_make_all_file.c
 *
 *  Fill a fresh card directory (FF3 0x201828..0x201a58): every data file,
 *  then the icon, then icon.sys. From MioPan system/mc/prg/mc_make_all_file.c
 *  (FF2), the same code in FF3 without the asserts (buffer NULL / too
 *  small, bad step: the default case just returns 0).
 *
 *  Steps 0/1 loop: each pass makes one data file and, while
 *  make_data_file_num is below GetMemoryCardDataFileNum(), goes back to step
 *  0 for the next one. Every file is written zero-filled with 0xffffffff as
 *  its checksum, the pattern MemoryCardCheckFileBroken accepts as intact and
 *  the load check reads as "empty".
 * ======================================================================== */
#include "system/mc/prg/mc_make_all_file.h"

#include <stddef.h>
#include <string.h>

#include "system/mc/prg/mc_icon.h"          // MemoryCardIcon*
#include "system/mc/prg/mc_icon_sys.h"      // MemoryCardIconSys*
#include "system/mc/prg/mc_make_file.h"     // MemoryCardMakeNewFile*
#include "system/mc/prg/mc_set_data.h"      // MemoryCardSetFilePath, GetMemoryCardData*

// layout: host -- buff_addr is a pointer (EE 4 bytes at 0x18; struct 0x20).
struct MC_MAKE_ALL_FILE_CTRL                // 0x20, bss 0x3f5cd8
{
    /* 0x00 */ char  step;                  // 0 make file, 1 wait, 2 icon, 3 wait, 4 icon.sys, 5 wait
    /* 0x04 */ int   port;
    /* 0x08 */ int   slot;
    /* 0x0c */ int   dir_label;
    /* 0x10 */ int   data_file_num;         // cached at Init; Main re-reads it (as the ROM does)
    /* 0x14 */ int   make_data_file_num;    // next file label to make
    /* 0x18 */ void *buff_addr;
    /* 0x1c */ int   buff_size;             // only checked by FF2's assert
};
static_assert(offsetof(MC_MAKE_ALL_FILE_CTRL, port) == 0x04, "MC_MAKE_ALL_FILE_CTRL.port");
static_assert(offsetof(MC_MAKE_ALL_FILE_CTRL, slot) == 0x08, "MC_MAKE_ALL_FILE_CTRL.slot");
static_assert(offsetof(MC_MAKE_ALL_FILE_CTRL, dir_label) == 0x0c, "MC_MAKE_ALL_FILE_CTRL.dir_label");
static_assert(offsetof(MC_MAKE_ALL_FILE_CTRL, data_file_num) == 0x10, "MC_MAKE_ALL_FILE_CTRL.data_file_num");
static_assert(offsetof(MC_MAKE_ALL_FILE_CTRL, make_data_file_num) == 0x14, "MC_MAKE_ALL_FILE_CTRL.make_data_file_num");

static MC_MAKE_ALL_FILE_CTRL mc_make_all_file_ctrl; // bss 0x3f5cd8

// FF3 0x00201828; FF2 MemoryCardAllFileMakeInit 0x001e0ba8.
void MemoryCardAllFileMakeInit(int port, int slot, int dir_label, void *buff_addr, int buff_size)
{
    mc_make_all_file_ctrl.step = 0;
    mc_make_all_file_ctrl.port = port;
    mc_make_all_file_ctrl.slot = slot;
    mc_make_all_file_ctrl.dir_label = dir_label;
    mc_make_all_file_ctrl.data_file_num = GetMemoryCardDataFileNum(dir_label);
    mc_make_all_file_ctrl.make_data_file_num = 0;
    mc_make_all_file_ctrl.buff_addr = buff_addr;
    mc_make_all_file_ctrl.buff_size = buff_size;
}

// FF3 0x00201890; FF2 MemoryCardAllFileMakeMain 0x001e0c28.
// 1 all made, 0 busy, < 0 the failing step's error.
int MemoryCardAllFileMakeMain(void)
{
    int  mc_res;
    int  data_file_size;
    char path_name[0x37];

    memset(path_name, 0, sizeof(path_name));

    switch (mc_make_all_file_ctrl.step)
    {
    case 0:
        MemoryCardSetFilePath(path_name, mc_make_all_file_ctrl.dir_label, mc_make_all_file_ctrl.make_data_file_num);
        data_file_size = GetMemoryCardDataSize(mc_make_all_file_ctrl.dir_label, mc_make_all_file_ctrl.make_data_file_num);

        // zero data, checksum word -1
        memset(mc_make_all_file_ctrl.buff_addr, 0, data_file_size);
        memset((char *)mc_make_all_file_ctrl.buff_addr + data_file_size - 4, -1, 4);

        MemoryCardMakeNewFileInit(mc_make_all_file_ctrl.port, mc_make_all_file_ctrl.slot, path_name,
                                  mc_make_all_file_ctrl.buff_addr, data_file_size);
        mc_make_all_file_ctrl.step = 1;
        // fall through
    case 1:
        mc_res = MemoryCardMakeNewFileMain();
        if (mc_res == 1)
        {
            mc_make_all_file_ctrl.make_data_file_num++;
            if (mc_make_all_file_ctrl.make_data_file_num < GetMemoryCardDataFileNum(mc_make_all_file_ctrl.dir_label))
            {
                mc_make_all_file_ctrl.step = 0;
            }
            else
            {
                mc_make_all_file_ctrl.step = 2;
            }
            return 0;
        }
        break;

    case 2:
        MemoryCardIconInit(mc_make_all_file_ctrl.port, mc_make_all_file_ctrl.slot, mc_make_all_file_ctrl.dir_label, 0);
        mc_make_all_file_ctrl.step = 3;
        // fall through
    case 3:
        mc_res = MemoryCardIconMain();
        if (mc_res == 1)
        {
            mc_make_all_file_ctrl.step = 4;
            return 0;
        }
        break;

    case 4:
        MemoryCardIconSysInit(mc_make_all_file_ctrl.port, mc_make_all_file_ctrl.slot, mc_make_all_file_ctrl.dir_label);
        mc_make_all_file_ctrl.step = 5;
        // fall through
    case 5:
        mc_res = MemoryCardIconSysMain();
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
