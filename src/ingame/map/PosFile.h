/* ==========================================================================
 *  ingame/map/PosFile.h
 *
 *  Position files (FF3-only, 0x243898..0x2439d8): a room's area_pos data --
 *  a 0x10-byte header with the entry count at +4, then 0x50-byte entries,
 *  each a position, a rotation, a scale and a "<file>.<object>" name.
 * ======================================================================== */
#ifndef _INGAME_MAP_POSFILE_H
#define _INGAME_MAP_POSFILE_H

#include <stddef.h>                     // offsetof
#include "sdk/scetypes.h"

struct POS_FILE_HEAD                    /* 0x10 */
{
    /* 0x00 */ int    _00;
    /* 0x04 */ int    num;
    /* 0x08 */ u_char _08[8];
};
static_assert(sizeof(POS_FILE_HEAD) == 0x10, "POS_FILE_HEAD is 0x10 bytes");
static_assert(offsetof(POS_FILE_HEAD, num) == 0x04, "POS_FILE_HEAD.num");

struct POS_FILE_ENT                     /* 0x50 */
{
    /* 0x00 */ float pos[4];
    /* 0x10 */ float rot[4];            // degrees (x, y, z)
    /* 0x20 */ float scale[4];
    /* 0x30 */ u_char _30[4];
    /* 0x34 */ char  name[0x1c];        // "<file>.<object>"
};
static_assert(sizeof(POS_FILE_ENT) == 0x50, "POS_FILE_ENT is 0x50 bytes");
static_assert(offsetof(POS_FILE_ENT, rot) == 0x10, "POS_FILE_ENT.rot");
static_assert(offsetof(POS_FILE_ENT, scale) == 0x20, "POS_FILE_ENT.scale");
static_assert(offsetof(POS_FILE_ENT, name) == 0x34, "POS_FILE_ENT.name");

typedef int (*POS_FILE_FUNC)(void *ent, void *arg);    // non-zero ends the walk

int           PosFileGetNum(void *data);            // FF3 0x00243898: the entry count, 0 for nullptr
POS_FILE_ENT *PosFileGetEnt(void *data, int no);    // FF3 0x002438b0: entry no, nullptr
int           PosFileForEach(void *data, const char *ext, POS_FILE_FUNC func, void *arg);  // FF3 0x002438e8: 0; -1 no data, -2 no func, -3 ext too long

#endif /* _INGAME_MAP_POSFILE_H */
