// system/b2d/B2dFile.c -- FF3's B2d layout file reader (FF3 0x104320..0x1055d8,
// assert string "B2dFile.c"). No FF2 counterpart: written from the FF3
// decompilation. See B2dFile.h for the file format and the commands.

#include <string.h>

#include "system/b2d/B2dFile.h"
#include "system/b2d/B2dCtl.h"          // item creators, B2dPartAddKey, B2dErrPrint
#include "system/b2d/B2dInp.h"          // B2dInpSetLabel, B2dInpClearLabels, B2dInpSetWait

static B2D_FILE_ENT       b2d_file[B2D_ENTRY_MAX];      // 0x2e9218
static const B2D_CMD_DEF *b2d_cmd_tbl[B2D_CMD_TBL_MAX]; // 0x2ef270, by command type
static B2D_ITEM_HOOK_FUNC b2d_tex_load_func;            // 0x372b0c (gp-0x9e4)

static const char b2d_file_c[] = "B2dFile.c";           // 0x34bd90

static int B2dCmdTexLoad(int handle, const B2D_CMD *cmd);
static int B2dCmdLabel(int handle, const B2D_CMD *cmd);
static int B2dCmdEnd(int handle, const B2D_CMD *cmd);
static int B2dCmdDelete(int handle, const B2D_CMD *cmd);
static int B2dCmdSprite(int handle, const B2D_CMD *cmd);
static int B2dCmdRect(int handle, const B2D_CMD *cmd);
static int B2dCmdHookItem(int handle, const B2D_CMD *cmd);
static int B2dCmdGroup(int handle, const B2D_CMD *cmd);
static int B2dCmdSelect(int handle, const B2D_CMD *cmd);
static int B2dCmdKeyXform(int handle, const B2D_CMD *cmd);
static int B2dCmdKeyPos(int handle, const B2D_CMD *cmd);
static int B2dCmdKeyScale(int handle, const B2D_CMD *cmd);
static int B2dCmdKeyAlpha(int handle, const B2D_CMD *cmd);
static int B2dCmdKeyRot(int handle, const B2D_CMD *cmd);
static int B2dCmdKeyRgb(int handle, const B2D_CMD *cmd);
static int B2dCmdAnchor(int handle, const B2D_CMD *cmd);
static int B2dCmdNop(int handle, const B2D_CMD *cmd);
static int B2dCmdText(int handle, const B2D_CMD *cmd);
static int B2dCmdRepeat(int handle, const B2D_CMD *cmd);
static void B2dFileChangePartFlags(int handle, int set, u_char mask);

// The command descriptors (rodata 0x34bdc8): id, handler, keys, items.
static const B2D_CMD_DEF b2d_cmd_def[] =
{
    { B2D_CMD_TEXLOAD, B2dCmdTexLoad,  0, 0 },
    { B2D_CMD_LABEL,   B2dCmdLabel,    0, 0 },
    { B2D_CMD_END,     B2dCmdEnd,      0, 0 },
    { B2D_CMD_DELETE,  B2dCmdDelete,   0, 0 },
    { B2D_CMD_SPRITE,  B2dCmdSprite,   0, 1 },
    { B2D_CMD_RECT,    B2dCmdRect,     0, 1 },
    { B2D_CMD_HOOK,    B2dCmdHookItem, 0, 1 },
    { B2D_CMD_GROUP,   B2dCmdGroup,    0, 1 },
    { B2D_CMD_XFORM,   B2dCmdKeyXform, 5, 0 },
    { B2D_CMD_POS,     B2dCmdKeyPos,   2, 0 },
    { B2D_CMD_SCALE,   B2dCmdKeyScale, 2, 0 },
    { B2D_CMD_ALPHA,   B2dCmdKeyAlpha, 1, 0 },
    { B2D_CMD_ROT,     B2dCmdKeyRot,   1, 0 },
    { B2D_CMD_RGB,     B2dCmdKeyRgb,   1, 0 },
    { B2D_CMD_ANCHOR,  B2dCmdAnchor,   2, 0 },
    { B2D_CMD_NOP,     B2dCmdNop,      1, 0 },
    { B2D_CMD_SELECT,  B2dCmdSelect,   0, 0 },
    { B2D_CMD_TEXT,    B2dCmdText,     1, 0 },
    { B2D_CMD_REPEAT,  B2dCmdRepeat,   2, 0 },
    { -1,              nullptr,       -1, 0 },
};

// ──────────────────────────────────────────────────────────────────────
// FF3 0x00104320.

void B2dFileClearPartFlags(int handle, u_char mask)
{
    B2dFileChangePartFlags(handle, 0, mask);
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x00104340.

void B2dFileSetPartFlags(int handle, u_char mask)
{
    B2dFileChangePartFlags(handle, 1, mask);
}

// ──────────────────────────────────────────────────────────────────────
// The string at `ofs` in the string table of the handle's data.
// FF3 0x00104360.

const char *B2dFileGetString(int handle, int ofs)
{
    int code = -1;
    int line;

    if ((u_int)handle < B2D_ENTRY_MAX)
    {
        const B2D_FILE_HEAD *data = b2d_file[handle].data;

        if (data != nullptr)
            return (const char *)data + data->str_ofs + ofs;
        code = -2;
        line = 0x7b;
    }
    else
    {
        line = 0x77;
    }
    B2dErrPrint(code, b2d_file_c, line, 0);
    return nullptr;
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x00104470.

int B2dFileSetTexLoadFunc(B2D_ITEM_HOOK_FUNC func)
{
    b2d_tex_load_func = func;
    return 0;
}

// ──────────────────────────────────────────────────────────────────────
// The name of label `n`. FF3 0x00104480 (no callers in FF3).

const char *B2dFileGetLabelName(int handle, int n)
{
    int code;
    int line;

    if ((u_int)handle < B2D_ENTRY_MAX)
    {
        if ((u_int)n < B2D_FILE_LABEL_MAX)
        {
            const B2D_CMD *label = b2d_file[handle].label[n];

            if (label != nullptr)
                return B2dFileGetString(handle, label->arg[0]);
            code = -2;
            line = 0xb5;
        }
        else
        {
            code = -1;
            line = 0xaf;
        }
    }
    else
    {
        code = -3;
        line = 0xac;
    }
    B2dErrPrint(code, b2d_file_c, line, 0);
    return nullptr;
}

// ──────────────────────────────────────────────────────────────────────
// A key on `part` of the selected item, after the part flags: a locked
// part takes nothing, a B2D_KEY_LOCK key locks a lockable part, a part
// flagged for reset loses its keys first (and the new first key keeps the
// current value). 0, -1 bad part, -2 when B2dPartAddKey fails.
// FF3 0x00104510.

int B2dFileAddKey(int handle, u_int part, u_int flags, const int *value, int frames)
{
    if (part >= B2D_ITEM_PARTS)
    {
        B2dErrPrint(-1, b2d_file_c, 0xd7, 0);
        return -1;
    }

    B2D_FILE_ENT *ent = &b2d_file[handle];
    u_char *pflag = &ent->part_flags[part];

    if (*pflag & B2D_PFLAG_LOCKABLE)
    {
        if (*pflag & B2D_PFLAG_LOCKED)
            return 0;
        if (flags & B2D_KEY_LOCK)
        {
            *pflag |= B2D_PFLAG_LOCKED;
            return 0;
        }
    }
    if (*pflag & B2D_PFLAG_RESET)
    {
        flags |= B2D_KEY_NO_START;
        B2dItemClearPartKeys(ent->sel_item, part);
        *pflag &= ~B2D_PFLAG_RESET;
    }
    return (B2dPartAddKey(handle, ent->sel_item, part, flags, value, frames) < 0) ? -2 : 0;
}

// ──────────────────────────────────────────────────────────────────────
// Command 0: hands a file name to the texture-load hook. FF3 0x00104660
// (no Ghidra function before 2026-10-13).

static int B2dCmdTexLoad(int handle, const B2D_CMD *cmd)
{
    if (b2d_tex_load_func == nullptr)
    {
        B2dErrPrint(-1, b2d_file_c, 0x108, 0);
        return -1;
    }

    const char *name = B2dFileGetString(handle, cmd->arg[0]);
    B2D_HOOK_ARGS args = {};

    args.a0 = handle;
    args.a1 = cmd->arg[1];
    args.a2 = (intptr_t)name;
    b2d_tex_load_func(&args);
    return 0;
}

// ──────────────────────────────────────────────────────────────────────
// Command 1 (label start). FF3 0x001046d8.

static int B2dCmdLabel(int handle, const B2D_CMD *cmd)
{
    if (cmd->arg[1] != 0)
        B2dFileSetPartFlags(handle, B2D_PFLAG_RESET);
    B2dInpSetWait(handle, cmd->arg[2]);
    return 0;
}

// ──────────────────────────────────────────────────────────────────────
// Command 2 (label end): stops the runner. FF3 0x00104728.

static int B2dCmdEnd(int handle, const B2D_CMD *cmd)
{
    (void)cmd;
    b2d_file[handle].sel_id = -1;
    B2dFileClearPartFlags(handle, B2D_PFLAG_RESET);
    return 1;
}

// ──────────────────────────────────────────────────────────────────────
// Command 3. FF3 0x00104768.
// callcheck: FUN_001029f0 (= B2dCtlDeleteItem; Ghidra's call target has no name)

static int B2dCmdDelete(int handle, const B2D_CMD *cmd)
{
    (void)cmd;
    B2dCtlDeleteItem(handle, b2d_file[handle].sel_id);
    return 0;
}

// ──────────────────────────────────────────────────────────────────────
// The new item's parent (part 5, start and value). FF3 inlines this in
// each creator command.

static void B2dCmdSetParent(int handle, int id, int parent)
{
    B2D_ITEM *item = B2dCtlFindItem(handle, id);

    item->part[B2D_PART_PARENT].value = parent;
    item->part[B2D_PART_PARENT].start = parent;
}

// ──────────────────────────────────────────────────────────────────────
// Command 4: a sprite item. The creators return 0, or the creator's
// result when it is not positive. FF3 0x001047a0.
// callcheck: B2dCtlFindItem (in B2dCmdSetParent)

static int B2dCmdSprite(int handle, const B2D_CMD *cmd)
{
    B2D_FILE_ENT *ent = &b2d_file[handle];

    ent->sel_id = cmd->arg[0];

    const char *name = B2dFileGetString(handle, cmd->arg[7]);
    int ret = B2dCtlAddSpriteItem(handle, ent->sel_id, cmd->arg[1], cmd->arg[2], cmd->arg[3],
                                  cmd->arg[4], cmd->arg[5], cmd->arg[6], name);

    if (ret >= 0)
        B2dCmdSetParent(handle, ent->sel_id, cmd->arg[10]);
    return (ret > 0) ? 0 : ret;
}

// ──────────────────────────────────────────────────────────────────────
// Command 5: a rectangle item. FF3 0x00104860.
// callcheck: B2dCtlFindItem (in B2dCmdSetParent)

static int B2dCmdRect(int handle, const B2D_CMD *cmd)
{
    B2D_FILE_ENT *ent = &b2d_file[handle];

    ent->sel_id = cmd->arg[0];

    int ret = B2dCtlAddRectItem(handle, ent->sel_id, cmd->arg[1], cmd->arg[2], cmd->arg[3],
                                cmd->arg[4]);

    if (ret >= 0)
        B2dCmdSetParent(handle, ent->sel_id, cmd->arg[7]);
    return (ret > 0) ? 0 : ret;
}

// ──────────────────────────────────────────────────────────────────────
// Command 6: an item drawn by hook 5. FF3 0x00104908.
// callcheck: B2dCtlFindItem (in B2dCmdSetParent)

static int B2dCmdHookItem(int handle, const B2D_CMD *cmd)
{
    B2D_FILE_ENT *ent = &b2d_file[handle];

    ent->sel_id = cmd->arg[0];

    const char *name = B2dFileGetString(handle, cmd->arg[4]);
    int ret = B2dCtlAddHookItem(handle, ent->sel_id, cmd->arg[1], cmd->arg[2], cmd->arg[3], name);

    if (ret >= 0)
        B2dCmdSetParent(handle, ent->sel_id, cmd->arg[7]);
    return (ret > 0) ? 0 : ret;
}

// ──────────────────────────────────────────────────────────────────────
// Command 7: a group item. FF3 0x001049b8.
// callcheck: B2dCtlFindItem (in B2dCmdSetParent)

static int B2dCmdGroup(int handle, const B2D_CMD *cmd)
{
    B2D_FILE_ENT *ent = &b2d_file[handle];

    ent->sel_id = cmd->arg[0];

    const char *name = B2dFileGetString(handle, cmd->arg[4]);
    int ret = B2dCtlAddGroupItem(handle, ent->sel_id, name);

    if (ret >= 0)
        B2dCmdSetParent(handle, ent->sel_id, cmd->arg[7]);
    return (ret > 0) ? 0 : ret;
}

// ──────────────────────────────────────────────────────────────────────
// Command 16: selects the item later commands key. FF3 0x00104a60.

static int B2dCmdSelect(int handle, const B2D_CMD *cmd)
{
    B2D_FILE_ENT *ent = &b2d_file[handle];

    for (int i = 0; i < B2D_ITEM_PARTS; i++)
    {
        if (ent->part_flags[i] & B2D_PFLAG_RESET_ON_SELECT)
            ent->part_flags[i] |= B2D_PFLAG_RESET;
    }
    B2dFileClearPartFlags(handle, B2D_PFLAG_LOCKED);
    ent->sel_id   = cmd->arg[0];
    ent->sel_item = B2dCtlFindItem(handle, ent->sel_id);
    return 0;
}

// ──────────────────────────────────────────────────────────────────────
// Command 8: keys on x, y, sx, sy and alpha. FF3 0x00104b00 (the part /
// argument pairs are copied from rodata 0x34bda0).

static int B2dCmdKeyXform(int handle, const B2D_CMD *cmd)
{
    static const int pair[5][2] =
    {
        { B2D_PART_X, 0 }, { B2D_PART_Y, 1 }, { B2D_PART_SX, 2 }, { B2D_PART_SY, 3 },
        { B2D_PART_ALPHA, 4 },
    };
    u_int flags  = cmd->arg[6];
    int   frames = cmd->arg[5];

    for (int i = 0; i < 5; i++)
    {
        if (B2dFileAddKey(handle, pair[i][0], flags, &cmd->arg[pair[i][1]], frames) < 0)
            return -1;
    }
    return 0;
}

// ──────────────────────────────────────────────────────────────────────
// Commands 9 and 10: two keys from args 0 and 1. FF3 0x00104bf0 (x, y),
// 0x00104c80 (sx, sy).

static int B2dCmdKeyPos(int handle, const B2D_CMD *cmd)
{
    u_int flags  = cmd->arg[3];
    int   frames = cmd->arg[2];

    if (B2dFileAddKey(handle, B2D_PART_X, flags, &cmd->arg[0], frames) < 0)
        return -1;
    if (B2dFileAddKey(handle, B2D_PART_Y, flags, &cmd->arg[1], frames) < 0)
        return -2;
    return 0;
}

static int B2dCmdKeyScale(int handle, const B2D_CMD *cmd)
{
    u_int flags  = cmd->arg[3];
    int   frames = cmd->arg[2];

    if (B2dFileAddKey(handle, B2D_PART_SX, flags, &cmd->arg[0], frames) < 0)
        return -1;
    if (B2dFileAddKey(handle, B2D_PART_SY, flags, &cmd->arg[1], frames) < 0)
        return -2;
    return 0;
}

// ──────────────────────────────────────────────────────────────────────
// Commands 11, 12, 13: one key from arg 0. FF3 0x00104d10 (alpha),
// 0x00104d48 (rotation), 0x00104d80 (RGBA).

static int B2dCmdKeyAlpha(int handle, const B2D_CMD *cmd)
{
    return (B2dFileAddKey(handle, B2D_PART_ALPHA, cmd->arg[2], &cmd->arg[0], cmd->arg[1]) < 0) ? -1 : 0;
}

static int B2dCmdKeyRot(int handle, const B2D_CMD *cmd)
{
    return (B2dFileAddKey(handle, B2D_PART_ROT, cmd->arg[2], &cmd->arg[0], cmd->arg[1]) < 0) ? -1 : 0;
}

static int B2dCmdKeyRgb(int handle, const B2D_CMD *cmd)
{
    return (B2dFileAddKey(handle, B2D_PART_RGB, cmd->arg[2], &cmd->arg[0], cmd->arg[1]) < 0) ? -1 : 0;
}

// ──────────────────────────────────────────────────────────────────────
// Command 14: the selected item's anchor, set directly. FF3 0x00104db8
// (no Ghidra function before 2026-10-13).

static int B2dCmdAnchor(int handle, const B2D_CMD *cmd)
{
    B2D_ITEM *item = b2d_file[handle].sel_item;

    item->part[B2D_PART_ANCHORX].value = cmd->arg[0];
    item->part[B2D_PART_ANCHORX].start = cmd->arg[0];
    item->part[B2D_PART_ANCHORY].value = cmd->arg[1];
    item->part[B2D_PART_ANCHORY].start = cmd->arg[1];
    return 0;
}

// ──────────────────────────────────────────────────────────────────────
// Command 15. FF3 0x00104df8 (no Ghidra function before 2026-10-13).

static int B2dCmdNop(int handle, const B2D_CMD *cmd)
{
    (void)handle;
    (void)cmd;
    return 0;
}

// ──────────────────────────────────────────────────────────────────────
// Command 17: the text id. FF3 0x00104e00.

static int B2dCmdText(int handle, const B2D_CMD *cmd)
{
    return (B2dFileAddKey(handle, B2D_PART_TEXT, 0, &cmd->arg[0], 0) < 0) ? -1 : 0;
}

// ──────────────────────────────────────────────────────────────────────
// Command 18: tile counts (the x key carries the tile flip flags).
// FF3 0x00104e38.

static int B2dCmdRepeat(int handle, const B2D_CMD *cmd)
{
    if (B2dFileAddKey(handle, B2D_PART_REPX, cmd->arg[2], &cmd->arg[0], 0) < 0)
        return -1;
    if (B2dFileAddKey(handle, B2D_PART_REPY, 0, &cmd->arg[1], 0) < 0)
        return -2;
    return 0;
}

// ──────────────────────────────────────────────────────────────────────
// Commands 0x13..0x24: input event `type - 0x13` plays label arg 0.
// FF3 0x00104eb0.

static int B2dCmdInput(int handle, const B2D_CMD *cmd, int type)
{
    const char *label = B2dFileGetString(handle, cmd->arg[0]);

    B2dInpSetLabel(handle, type - B2D_CMD_INPUT, label);
    return 0;
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x00104ef8.

const B2D_CMD *B2dCmdNext(const B2D_CMD *cmd)
{
    int n = cmd->count - 1;

    if (cmd->count < 1)
        n = 0;
    return (const B2D_CMD *)&cmd->arg[n + 1];
}

// ──────────────────────────────────────────────────────────────────────
// The label command named `name`, or nullptr. FF3 0x00104f20.

const B2D_CMD *B2dFileFindLabel(int handle, const char *name)
{
    if ((u_int)handle >= B2D_ENTRY_MAX)
    {
        B2dErrPrint(-1, b2d_file_c, 0x33e, 0);
        return nullptr;
    }

    const B2D_CMD *const *label = b2d_file[handle].label;

    for (int n = 0; label[n] != nullptr; )
    {
        const B2D_CMD *cmd = label[n];

        n++;
        if (strcmp(B2dFileGetString(handle, cmd->arg[0]), name) == 0)
            return cmd;
        if (n >= B2D_FILE_LABEL_MAX)
            break;
    }
    return nullptr;
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x00104ff0.

int B2dFilePlayLabel(int handle, const char *name)
{
    const B2D_CMD *label = B2dFileFindLabel(handle, name);

    if (label == nullptr)
        return -1;
    B2dFileRunCmds(handle, label);
    return 0;
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x00105038.

int B2dFilePlayLabelEx(int handle, const char *name)
{
    B2dFileSetPartFlags(handle, B2D_PFLAG_RESET_ON_SELECT | B2D_PFLAG_LOCKABLE);

    int ret = B2dFilePlayLabel(handle, name);

    B2dFileClearPartFlags(handle, B2D_PFLAG_RESET_ON_SELECT | B2D_PFLAG_LOCKABLE | B2D_PFLAG_LOCKED);
    return ret;
}

// ──────────────────────────────────────────────────────────────────────
// The B2d work the data needs: every item the file creates, and a key
// pool as big as the biggest label needs (a label ends at command 2).
// -1 for a command without a descriptor. FF3 0x00105090.
// PC: sized with the host B2D_ITEM / B2D_KEY (EE 0x1c8 / 0x10), as
// B2dCtlSetup lays the work out.

int B2dFileGetWorkSize(const void *b2d, int *nitem, int *nkey, int *nlabel)
{
    const B2D_FILE_HEAD *head = (const B2D_FILE_HEAD *)b2d;
    const B2D_CMD *cmd = (const B2D_CMD *)((const char *)head + head->cmd_ofs);
    int keys_max = 0;
    int items    = 0;
    int labels   = 0;
    int keys     = 0;

    for (int i = 0; i < head->ncmd; i++)
    {
        int type = cmd->type;

        if (type == B2D_CMD_LABEL)
        {
            labels++;
        }
        else if (type == B2D_CMD_END)
        {
            if (keys_max < keys)
                keys_max = keys;
            keys = 0;
        }
        else if ((u_int)(type - B2D_CMD_INPUT) >= (u_int)(B2D_CMD_INPUT_END - B2D_CMD_INPUT))
        {
            // PC: bounds-checked; FF3 indexes the table with any type.
            const B2D_CMD_DEF *def = ((u_int)type < B2D_CMD_TBL_MAX) ? b2d_cmd_tbl[type] : nullptr;

            if (def == nullptr)
                return -1;
            items += def->items;
            keys  += def->keys;
        }
        cmd = B2dCmdNext(cmd);
    }

    if (nkey != nullptr)
        *nkey = keys_max;
    if (nitem != nullptr)
        *nitem = items;
    if (nlabel != nullptr)
        *nlabel = labels;
    return items * (int)sizeof(B2D_ITEM) + keys_max * (int)sizeof(B2D_KEY);
}

// ──────────────────────────────────────────────────────────────────────
// Stores the label commands into `out` (when given); their count, -1 when
// more than `max`. FF3 0x001051d8.

int B2dFileListLabels(const void *b2d, const B2D_CMD **out, int max)
{
    const B2D_FILE_HEAD *head = (const B2D_FILE_HEAD *)b2d;
    const B2D_CMD *cmd = (const B2D_CMD *)((const char *)head + head->cmd_ofs);
    int n = 0;

    for (int i = 0; i < head->ncmd; i++)
    {
        if (cmd->type == B2D_CMD_LABEL)
        {
            if (out != nullptr)
            {
                if (n >= max)
                {
                    B2dErrPrint(-1, b2d_file_c, 0x3de, 0);
                    return -1;
                }
                out[n] = cmd;
            }
            n++;
        }
        cmd = B2dCmdNext(cmd);
    }
    return n;
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x001052b8 (the selected item pointer is left as it was).

int B2dFileReset(int handle)
{
    if ((u_int)handle >= B2D_ENTRY_MAX)
    {
        B2dErrPrint(-1, b2d_file_c, 0x3f8, 0);
        return -1;
    }

    B2D_FILE_ENT *ent = &b2d_file[handle];

    ent->sel_id = -1;
    ent->id     = -1;
    ent->data   = nullptr;
    for (int i = 0; i < B2D_FILE_LABEL_MAX; i++)
        ent->label[i] = nullptr;
    for (int i = 0; i < 20; i++)
        ent->part_flags[i] = 0;
    return 0;
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x00105368.

int B2dFileSetup(int handle, const void *b2d)
{
    if ((u_int)handle >= B2D_ENTRY_MAX)
    {
        B2dErrPrint(-1, b2d_file_c, 0x417, 0);
        return -1;
    }

    B2D_FILE_ENT *ent = &b2d_file[handle];

    B2dFileReset(handle);
    ent->id   = handle;
    ent->data = (const B2D_FILE_HEAD *)b2d;
    B2dFileListLabels(b2d, ent->label, B2D_FILE_LABEL_MAX);
    return 0;
}

// ──────────────────────────────────────────────────────────────────────
// Resets every entry and builds the command table. FF3 0x001053f8.

int B2dFileInit(void)
{
    for (int i = 0; i < B2D_ENTRY_MAX; i++)
        B2dFileReset(i);
    memset(b2d_cmd_tbl, 0, sizeof(b2d_cmd_tbl));

    const B2D_CMD_DEF *def = b2d_cmd_def;

    do
    {
        b2d_cmd_tbl[def->id] = def;
        def++;
    } while (def->id >= 0);
    return 0;
}

// ──────────────────────────────────────────────────────────────────────
// Sets (`set`) or clears `mask` in every part's flags. A static defined
// early in B2dFile.c (error line 0x4f), placed last by the compiler.
// FF3 0x00105490.

static void B2dFileChangePartFlags(int handle, int set, u_char mask)
{
    if ((u_int)handle >= B2D_ENTRY_MAX)
    {
        B2dErrPrint(-1, b2d_file_c, 0x4f, 0);
        return;
    }

    u_char *pflag = b2d_file[handle].part_flags;

    for (int i = 0; i < B2D_ITEM_PARTS; i++)
    {
        if (set)
            pflag[i] |= mask;
        else
            pflag[i] &= ~mask;
    }
}

// ──────────────────────────────────────────────────────────────────────
// Runs commands from `cmd` until a label end: 0; -2 when a command fails,
// -1 for a command without a handler. Input bindings start empty.
// FF3 0x00105510.

int B2dFileRunCmds(int handle, const B2D_CMD *cmd)
{
    B2dFileClearPartFlags(handle, B2D_PFLAG_RESET);
    B2dInpClearLabels(handle);

    for (;;)
    {
        int type = cmd->type;
        int ret;

        if ((u_int)(type - B2D_CMD_INPUT) < (u_int)(B2D_CMD_INPUT_END - B2D_CMD_INPUT))
        {
            ret = B2dCmdInput(handle, cmd, type);
        }
        else
        {
            // PC: bounds-checked; FF3 indexes the table with any type.
            const B2D_CMD_DEF *def = ((u_int)type < B2D_CMD_TBL_MAX) ? b2d_cmd_tbl[type] : nullptr;

            if (def == nullptr)
                return -1;
            ret = def->func(handle, cmd);
        }
        if (ret < 0)
            return -2;
        if (ret == 1)
            return 0;
        cmd = B2dCmdNext(cmd);
    }
}
