/* ==========================================================================
 *  graphics/ecb.h
 *
 *  ECB table files (FF3-only, 0x154cd0..0x154df0; file name invented, it
 *  sits between graphics/dmaVif1.c and graphics/graph3d/eeFile.cpp in link
 *  order). An .ecb file is a 0x20-byte head ("ecb", column count, row
 *  count, row size, ...) followed by row_num rows of row_size bytes each.
 *  The disc has eight: cam_rail, door_stat, furn_save, area_data, hasigo,
 *  meimetu (3ddata/) and map_menu, map_menu_stat (2ddata/). Readers load the
 *  whole file into a buffer and walk it with EcbGetNum / EcbGetData.
 * ======================================================================== */
#ifndef _GRAPHICS_ECB_H
#define _GRAPHICS_ECB_H

#include <stddef.h>                     // offsetof

// The file head (0x20 bytes; the rows follow it).
struct ECB_HEAD                         /* 0x20 */
{
    /* 0x00 */ char magic[4];           // "ecb\0"
    /* 0x04 */ int  col_num;            // fields per row (meimetu: 5 floats); no reader
    /* 0x08 */ int  row_num;            // EcbGetNum
    /* 0x0c */ int  row_size;           // EcbGetSize
    /* 0x10 */ int  ext_ofs;            // EcbGetExt: offset of an extra block, 0 = none (0 in every disc file)
    /* 0x14 */ int  file_size;          // EcbGetFileSize: the file's byte size, or 0
    /* 0x18 */ int  _18[2];
};
static_assert(sizeof(ECB_HEAD) == 0x20, "ECB_HEAD is 0x20 bytes");
static_assert(offsetof(ECB_HEAD, row_num) == 0x08, "ECB_HEAD.row_num");
static_assert(offsetof(ECB_HEAD, row_size) == 0x0c, "ECB_HEAD.row_size");
static_assert(offsetof(ECB_HEAD, ext_ofs) == 0x10, "ECB_HEAD.ext_ofs");
static_assert(offsetof(ECB_HEAD, file_size) == 0x14, "ECB_HEAD.file_size");

void *EcbGetExt(const void *ecb, int ofs);          // FF3 0x00154cd0: ecb + ext_ofs + ofs, nullptr without a block
void *EcbGetExtData(const void *ecb, int ofs);      // FF3 0x00154cf0: EcbGetExt + 4
int   EcbGetExtNum(const void *ecb, int ofs);       // FF3 0x00154d18: the short at EcbGetExt + 2
int   EcbGetNum(const void *ecb);                   // FF3 0x00154d68: rows, 0 for nullptr
int   EcbGetSize(const void *ecb);                  // FF3 0x00154d80: row size, 0 for nullptr
int   EcbGetFileSize(const void *ecb);              // FF3 0x00154d98: head +0x14, 0 for nullptr
void *EcbGetData(const void *ecb, int no);          // FF3 0x00154db0: row no, nullptr out of range

#endif /* _GRAPHICS_ECB_H */
