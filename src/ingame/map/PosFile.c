// ingame/map/PosFile.c -- FF3 0x243898..0x2439d8 (FF3-only): the position
// file (area_pos) reader.

#include <string.h>                     // strlen, strchr, strncmp

#include "ingame/map/PosFile.h"

// FF3 0x00243898.
int PosFileGetNum(void *data)
{
    if (data == nullptr)
        return 0;

    return ((POS_FILE_HEAD *)data)->num;
}

// FF3 0x002438b0.
POS_FILE_ENT *PosFileGetEnt(void *data, int no)
{
    if (data == nullptr || no < 0 || no >= ((POS_FILE_HEAD *)data)->num)
        return nullptr;

    return (POS_FILE_ENT *)((u_char *)data + sizeof(POS_FILE_HEAD)) + no;
}

// ──────────────────────────────────────────────────────────────────────
// func(entry, arg) for each entry whose name after the '.' starts with ext,
// until one returns non-zero.
// FF3 0x002438e8.
// callcheck: FUN_002a305c (strchr)

int PosFileForEach(void *data, const char *ext, POS_FILE_FUNC func, void *arg)
{
    int           len = (int)strlen(ext);
    POS_FILE_ENT *ent;

    if (data == nullptr)
        return -1;
    if (func == nullptr)
        return -2;
    if (len >= 0x1c)
        return -3;

    ent = (POS_FILE_ENT *)((u_char *)data + sizeof(POS_FILE_HEAD));
    for (int i = 0; i < ((POS_FILE_HEAD *)data)->num; i++, ent++)
    {
        if (strncmp(strchr(ent->name, '.') + 1, ext, len) == 0 && func(ent, arg) != 0)
            return 0;
    }
    return 0;
}
