/* ==========================================================================
 *  ingame/map/MapPhf.h
 *
 *  FF3's PHF pack reader (FF3 0x11d648..0x11d890; FF3-only, no FF2
 *  counterpart). A PHF pack bundles a 2D layout with its textures
 *  (caption.phf: _phf_.b2d, bottun.tm2, caption.tm2) or a room's
 *  furniture: a 0x10-byte head, one 0x20-byte entry per member, then the
 *  members' data. The packs are read in place where they were loaded.
 *  File name invented: the code sits between MapSave.c and MapSky.c, after
 *  the object that ends with the static CClassPtr<CLightCtl> at 0x38e6d0.
 * ======================================================================== */
#ifndef _INGAME_MAP_MAPPHF_H
#define _INGAME_MAP_MAPPHF_H

#include <stddef.h>                     // offsetof

#include "sdk/scetypes.h"

// The pack head. Checked against the retail packs (caption.phf: size
// 0x2e400, 3 members, data at 0x70; menubg.phf: 0x56ad0, 4, 0x90).
// layout: EE -- file data (32-bit words, read in place).
struct PHF_HEAD
{
    /* 0x00 */ char magic[4];           // "phf" + NUL (PhfCheck compares it as a string)
    /* 0x04 */ int  size;               // whole pack in bytes; not read here
    /* 0x08 */ int  num;                // member entries that follow the head
    /* 0x0c */ int  data_ofs;           // members' data, from the pack start (0x10 + num * 0x20)
};
static_assert(sizeof(PHF_HEAD) == 0x10, "PHF_HEAD is 0x10 bytes");
static_assert(offsetof(PHF_HEAD, size) == 0x04, "PHF_HEAD.size");
static_assert(offsetof(PHF_HEAD, num) == 0x08, "PHF_HEAD.num");
static_assert(offsetof(PHF_HEAD, data_ofs) == 0x0c, "PHF_HEAD.data_ofs");

// One member entry (from pack +0x10). The first member of a layout pack is
// `_phf_.b2d`; the rest are its .tm2 textures.
// layout: EE -- file data.
struct PHF_FILE
{
    /* 0x00 */ char name[0x18];         // NUL-terminated member name
    /* 0x18 */ int  ofs;                // data offset from PHF_HEAD.data_ofs
    /* 0x1c */ int  size;               // data size in bytes
};
static_assert(sizeof(PHF_FILE) == 0x20, "PHF_FILE is 0x20 bytes");
static_assert(offsetof(PHF_FILE, ofs) == 0x18, "PHF_FILE.ofs");
static_assert(offsetof(PHF_FILE, size) == 0x1c, "PHF_FILE.size");

int       PhfCheck(const void *pack);                       // 1 when `pack` is a PHF pack (null: 0)
int       PhfGetFileNum(const void *pack);                  // member count (0 when not a pack)
PHF_FILE *PhfGetFileHead(void *pack, int no);               // member `no`'s entry, or null
void     *PhfGetFileData(void *pack, int no);               // member `no`'s data, or null
PHF_FILE *PhfFindFileHead(void *pack, const char *name);    // the member called `name`, or null
int       PhfFindFileSize(void *pack, const char *name);    // its size (0 when missing)
void     *PhfFindFileData(void *pack, const char *name);    // its data (null when missing)

#endif /* _INGAME_MAP_MAPPHF_H */
