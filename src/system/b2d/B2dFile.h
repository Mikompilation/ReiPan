/* ==========================================================================
 *  system/b2d/B2dFile.h
 *
 *  FF3's B2d layout file reader (file string "B2dFile.c", 0x104320..0x1055d8).
 *  A .b2d file is a list of commands cut into labels: command 1 starts a
 *  label, command 2 ends it. Playing a label runs its commands against the
 *  B2d core (B2dCtl.c): create items, pick one, add animation keys to its
 *  parts, bind input events (B2dInp.c) to other labels. One entry per Bp
 *  handle (45, 0x224 bytes each on the EE at 0x2e9218).
 * ======================================================================== */
#ifndef _SYSTEM_B2D_B2DFILE_H
#define _SYSTEM_B2D_B2DFILE_H

#include <stddef.h>                     // offsetof

#include "sdk/scetypes.h"
#include "system/b2d/B2dCtl.h"          // B2D_ITEM, B2D_ITEM_HOOK_FUNC

// The head of a .b2d file (the `_phf_.b2d` member of a PHF pack). Offsets
// are from the start of the file; in the retail files the string table
// follows this head (0x10) and the commands follow the strings.
// layout: EE -- file data (32-bit words, read in place).
struct B2D_FILE_HEAD
{
    /* 0x00 */ int magic;               // "b2d" + NUL; not read by B2dFile.c
    /* 0x04 */ int cmd_ofs;             // first command
    /* 0x08 */ int str_ofs;             // string table (label, item and file names)
    /* 0x0c */ int ncmd;                // commands in the file
};
static_assert(sizeof(B2D_FILE_HEAD) == 0x10, "B2D_FILE_HEAD is 0x10 bytes");
static_assert(offsetof(B2D_FILE_HEAD, cmd_ofs) == 0x04, "B2D_FILE_HEAD.cmd_ofs");
static_assert(offsetof(B2D_FILE_HEAD, str_ofs) == 0x08, "B2D_FILE_HEAD.str_ofs");
static_assert(offsetof(B2D_FILE_HEAD, ncmd) == 0x0c, "B2D_FILE_HEAD.ncmd");

// One command: type, word count, then `count` words (at least one slot;
// B2dCmdNext). String arguments are offsets into the string table.
// layout: EE -- file data.
struct B2D_CMD
{
    /* 0x00 */ int type;                // B2D_CMD_*
    /* 0x04 */ int count;
    /* 0x08 */ int arg[1];              // count words
};
static_assert(offsetof(B2D_CMD, count) == 0x04, "B2D_CMD.count");
static_assert(offsetof(B2D_CMD, arg) == 0x08, "B2D_CMD.arg");

// Command types (descriptor list 0x34bdc8). The arguments, by index:
enum B2D_CMD_TYPE
{
    B2D_CMD_TEXLOAD  = 0,               // 0 file name, 1 passed to the texture-load hook
    B2D_CMD_LABEL    = 1,               // 0 label name, 1 reset the parts' keys, 2 input wait frames
    B2D_CMD_END      = 2,               // ends the label (the runner stops)
    B2D_CMD_DELETE   = 3,               // deletes the selected item
    B2D_CMD_SPRITE   = 4,               // 0 id, 1 tex, 2 u, 3 v, 4 w, 5 h, 6 text, 7 name, 10 parent
    B2D_CMD_RECT     = 5,               // 0 id, 1 rgba, 2 w, 3 h, 4 text, 7 parent
    B2D_CMD_HOOK     = 6,               // 0 id, 1 w, 2 h, 3 text, 4 name, 7 parent
    B2D_CMD_GROUP    = 7,               // 0 id, 4 name, 7 parent
    B2D_CMD_XFORM    = 8,               // keys x, y, sx, sy, alpha = 0..4; 5 frames, 6 flags
    B2D_CMD_POS      = 9,               // keys x, y = 0, 1; 2 frames, 3 flags
    B2D_CMD_SCALE    = 10,              // keys sx, sy = 0, 1; 2 frames, 3 flags
    B2D_CMD_ALPHA    = 11,              // key 0; 1 frames, 2 flags
    B2D_CMD_ROT      = 12,              // key 0; 1 frames, 2 flags
    B2D_CMD_RGB      = 13,              // key 0; 1 frames, 2 flags
    B2D_CMD_ANCHOR   = 14,              // anchor x, y = 0, 1 (set, not keyed)
    B2D_CMD_NOP      = 15,
    B2D_CMD_SELECT   = 16,              // 0 id: the item later commands work on
    B2D_CMD_TEXT     = 17,              // key text id = 0
    B2D_CMD_REPEAT   = 18,              // keys repeat x, y = 0, 1; 2 flags of the x key
    B2D_CMD_INPUT    = 0x13,            // 0x13..0x24: input event n = type - 0x13 runs label name 0
    B2D_CMD_INPUT_END = 0x25,
};

#define B2D_CMD_TBL_MAX   38            // 0x2ef270, 0x98 bytes

// A key flag B2dFile reads: on a lockable part (B2D_PFLAG_LOCKABLE) the
// key is not added and the part takes no more keys in this run.
#define B2D_KEY_LOCK      0x00400

// Per-part flags of an entry (+0x210, one byte per part).
#define B2D_PFLAG_RESET            0x01 // the next key clears the part's keys first
#define B2D_PFLAG_RESET_ON_SELECT  0x02 // B2D_CMD_SELECT sets B2D_PFLAG_RESET again
#define B2D_PFLAG_LOCKABLE         0x04 // a B2D_KEY_LOCK key locks the part
#define B2D_PFLAG_LOCKED           0x08

#define B2D_FILE_LABEL_MAX  128

// One entry (0x224 bytes on the EE).
// layout: host -- data, labels and the item are pointers; the comments keep
// the EE offsets.
struct B2D_FILE_ENT
{
    /* 0x000 */ int                  id;            // the handle; -1 when unused
    /* 0x004 */ const B2D_FILE_HEAD *data;
    /* 0x008 */ const B2D_CMD       *label[B2D_FILE_LABEL_MAX];   // the command-1 commands, in file order
    /* 0x208 */ int                  sel_id;        // selected item id, -1 none
    /* 0x20c */ B2D_ITEM            *sel_item;
    /* 0x210 */ u_char               part_flags[20];   // B2D_PFLAG_*; 18 used
};
static_assert(offsetof(B2D_FILE_ENT, id) == 0x00, "B2D_FILE_ENT.id");

// A command handler: 0 go on, 1 stop (label end), < 0 error.
typedef int (*B2D_CMD_FUNC)(int handle, const B2D_CMD *cmd);

// One command descriptor (12 bytes on the EE; list 0x34bdc8 ends with id -1).
// keys / items: what the command adds, for B2dFileGetWorkSize.
// layout: host -- func is a pointer.
struct B2D_CMD_DEF
{
    int          id;
    B2D_CMD_FUNC func;
    short        keys;
    short        items;
};

// ---- flags ----
void B2dFileClearPartFlags(int handle, u_char mask);
void B2dFileSetPartFlags(int handle, u_char mask);

// ---- data ----
const char *B2dFileGetString(int handle, int ofs);       // nullptr (and an error print) when unbound
int         B2dFileSetTexLoadFunc(B2D_ITEM_HOOK_FUNC func);   // command 0's hook: a0 handle, a1 arg 1, a2 file name; returns 0
const char *B2dFileGetLabelName(int handle, int n);
const B2D_CMD *B2dFileFindLabel(int handle, const char *name);
int  B2dFilePlayLabel(int handle, const char *name);    // runs the label; -1 when missing
int  B2dFilePlayLabelEx(int handle, const char *name);  // as B2dFilePlayLabel with parts lockable and reset on select
int  B2dFileGetWorkSize(const void *b2d, int *nitem, int *nkey, int *nlabel);   // B2dCtlSetup's work for the data
int  B2dFileListLabels(const void *b2d, const B2D_CMD **out, int max);
int  B2dFileReset(int handle);
int  B2dFileSetup(int handle, const void *b2d);         // binds the data to the handle; -1 for handle >= 45
int  B2dFileInit(void);                                 // B2dSystemInit
int  B2dFileRunCmds(int handle, const B2D_CMD *cmd);

// ---- commands (table 0x34bdc8) ----
int B2dFileAddKey(int handle, u_int part, u_int flags, const int *value, int frames);
const B2D_CMD *B2dCmdNext(const B2D_CMD *cmd);

#endif /* _SYSTEM_B2D_B2DFILE_H */
