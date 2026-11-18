/* ==========================================================================
 *  system/mc/prg/mc_del_all_file.c
 *
 *  Empty a card directory (FF3 0x200bd0..0x200e68): list it, then delete
 *  every entry from index 2 on ("." and ".." come first). Steps 2/3 loop
 *  one entry per pass. Both the "listing arrived" and the "one file
 *  deleted" paths end with the same test -- has the count reached the
 *  listing's length -- so an already-empty directory finishes without
 *  deleting anything. From MioPan system/mc/prg/mc_del_all_file.c (FF2):
 *  the same code in FF3 without the assert on an unknown step, which now
 *  just returns 0. Used by mc_del_dir.c.
 * ======================================================================== */
#include "system/mc/prg/mc_del_all_file.h"

#include <stddef.h>
#include <string.h>

#include "system/mc/prg/mc_check_dir.h"    // MemoryCardGetDirInfo*, GetMemoryCardCheckDir*
#include "system/mc/prg/mc_del_file.h"     // MemoryCardFileDelInit/Main
#include "system/mc/prg/mc_set_data.h"     // MemoryCardSetDirName, MemoryCardMakeSearchDirPath

struct MC_DEL_ALL_FILE_CTRL                 /* 0x14, bss 0x3f5800 */
{
    /* 0x00 */ char step;                   // 0 list, 1 wait for the listing, 2 delete next, 3 wait for the delete
    /* 0x04 */ int  port;
    /* 0x08 */ int  slot;
    /* 0x0c */ int  dir_label;              // MemoryCardSetDirName label
    /* 0x10 */ int  del_file_cnt;           // listing index of the next entry to delete (starts at 2)
};
static_assert(sizeof(MC_DEL_ALL_FILE_CTRL) == 0x14, "MC_DEL_ALL_FILE_CTRL size");
static_assert(offsetof(MC_DEL_ALL_FILE_CTRL, port) == 0x04, "MC_DEL_ALL_FILE_CTRL.port");
static_assert(offsetof(MC_DEL_ALL_FILE_CTRL, slot) == 0x08, "MC_DEL_ALL_FILE_CTRL.slot");
static_assert(offsetof(MC_DEL_ALL_FILE_CTRL, dir_label) == 0x0c, "MC_DEL_ALL_FILE_CTRL.dir_label");
static_assert(offsetof(MC_DEL_ALL_FILE_CTRL, del_file_cnt) == 0x10, "MC_DEL_ALL_FILE_CTRL.del_file_cnt");

static MC_DEL_ALL_FILE_CTRL mc_del_all_file_ctrl;   // bss 0x3f5800

static void SetMemoryCardDelFilePath(char *path_name, int dir_label, int data_pos);

// FF3 0x00200bd0; FF2 MemoryCardAllFileDelInit 0x001dfd38.
void MemoryCardAllFileDelInit(int port, int slot, int dir_label)
{
    mc_del_all_file_ctrl.del_file_cnt = 2;
    mc_del_all_file_ctrl.port = port;
    mc_del_all_file_ctrl.slot = slot;
    mc_del_all_file_ctrl.dir_label = dir_label;
    mc_del_all_file_ctrl.step = 0;
}

// FF3 0x00200bf8; FF2 MemoryCardAllFileDelMain 0x001dfd60.
// 1 emptied, 0 busy, <0 the listing's or the delete's error.
int MemoryCardAllFileDelMain(void)
{
    int  res = 0;
    int  mc_res;
    char path_name[55];

    memset(path_name, 0, sizeof(path_name));

    switch (mc_del_all_file_ctrl.step)
    {
    case 0:
        MemoryCardMakeSearchDirPath(path_name, mc_del_all_file_ctrl.dir_label);
        MemoryCardGetDirInfoInit(mc_del_all_file_ctrl.port, mc_del_all_file_ctrl.slot, path_name);
        mc_del_all_file_ctrl.step = 1;
        /* fall through */

    case 1:
        mc_res = MemoryCardGetDirInfoMain();
        if (mc_res == 1)
        {
            if (mc_del_all_file_ctrl.del_file_cnt < GetMemoryCardCheckDirFileNum())
            {
                mc_del_all_file_ctrl.step = 2;
            }
            else
            {
                res = 1;
            }
            return res;
        }
        break;

    case 2:
        SetMemoryCardDelFilePath(path_name, mc_del_all_file_ctrl.dir_label, mc_del_all_file_ctrl.del_file_cnt);
        MemoryCardFileDelInit(mc_del_all_file_ctrl.port, mc_del_all_file_ctrl.slot, path_name);
        mc_del_all_file_ctrl.step = 3;
        /* fall through */

    case 3:
        mc_res = MemoryCardFileDelMain();
        if (mc_res == 1)
        {
            mc_del_all_file_ctrl.del_file_cnt++;

            if (mc_del_all_file_ctrl.del_file_cnt < GetMemoryCardCheckDirFileNum())
            {
                mc_del_all_file_ctrl.step = 2;
            }
            else
            {
                res = 1;
            }
            return res;
        }
        break;

    default:                                // FF2 asserted here
        return 0;
    }

    if (mc_res < 0)
    {
        res = mc_res;
    }

    return res;
}

// FF3 0x00200d68; FF2 SetMemoryCardDelFilePath 0x001dfec8.
// "/<dir>/<listing entry data_pos>": the name comes off the card's listing,
// not the name tables, so the walk also removes files the game did not write.
// entry_name is zeroed twice in the ROM (initialiser and memset); kept.
static void SetMemoryCardDelFilePath(char *path_name, int dir_label, int data_pos)
{
    char fname[55] = "/";
    char entry_name[55] = "";
    char dir_name[21] = "";

    memset(entry_name, 0, sizeof(entry_name));
    memset(path_name, 0, 55);

    MemoryCardSetDirName(dir_name, dir_label);
    GetMemoryCardCheckDirEntryName(entry_name, data_pos);

    strcat(fname, dir_name);
    strcat(fname, "/");
    strcat(fname, entry_name);

    strcpy(path_name, fname);
}
