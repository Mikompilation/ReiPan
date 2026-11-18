/* ==========================================================================
 *  system/eeiop/cddat.h
 *
 *  The disc's file table (FF3 cddat.c, 0x26f340..0x26f770): one entry per
 *  file number of ZERO3.FHD (enum CD_FILE_DAT). ee_iopInit reads the whole
 *  FHD into memory and hands it to cddatInit; every lookup reads the tables
 *  of that image through the header pointer (gp-0xce0). FF2 got the same
 *  tables as separate arrays (cddatInit's arguments, IMG_HD on the IOP).
 * ======================================================================== */
#ifndef _SYSTEM_EEIOP_CDDAT_H
#define _SYSTEM_EEIOP_CDDAT_H

#include <stddef.h>                     // offsetof

#include "sdk/scetypes.h"
#include "system/eeiop/cd_file_dat.h"   // enum CD_FILE_DAT (file numbers)

#define CDDAT_FHD_MAGIC 0x46484400      // 'FHD\0' read as a little-endian word

// ZERO3.FHD's header (the file as loaded). The table fields are offsets from
// the start of the image; FF3's cddatSetHeader turns them into EE pointers in
// place, ReiPan keeps the offsets (see cddat.c). 0 means "no such table".
// Tables, indexed by file number:
//   size_tbl  u_int  unpacked size, (u_int)-1 for an entry without a file (a label)
//   ext_lbl   s_char extension label (GetFileExtLabel)
//   cmp_tbl   u_int  (stored size << 1) | compressed
//   name_tbl  CDDAT_NAME
//   lba_tbl   u_int  start sector in the data image (IMG_BD.BIN)
typedef struct
{
    /* 0x00 */ u_int magic;             // CDDAT_FHD_MAGIC
    /* 0x04 */ u_int version;           // 0x64 on the EU disc
    /* 0x08 */ u_int size;              // file size
    /* 0x0c */ int   file_num;          // GetFileNum
    /* 0x10 */ u_int size_tbl;
    /* 0x14 */ u_int ext_lbl;
    /* 0x18 */ u_int cmp_tbl;
    /* 0x1c */ u_int name_tbl;
    /* 0x20 */ u_int lba_tbl;
    /* 0x24 */ int   use_cd_image;      // 1: files come from the data image by sector
    /* 0x28 */ u_int _028[2];           // 0 on the EU disc; nothing reads them
} CDDAT_HEADER;                         /* 0x30 */

static_assert(sizeof(CDDAT_HEADER) == 0x30, "CDDAT_HEADER size");
static_assert(offsetof(CDDAT_HEADER, file_num) == 0x0c, "CDDAT_HEADER.file_num");
static_assert(offsetof(CDDAT_HEADER, size_tbl) == 0x10, "CDDAT_HEADER.size_tbl");
static_assert(offsetof(CDDAT_HEADER, ext_lbl) == 0x14, "CDDAT_HEADER.ext_lbl");
static_assert(offsetof(CDDAT_HEADER, cmp_tbl) == 0x18, "CDDAT_HEADER.cmp_tbl");
static_assert(offsetof(CDDAT_HEADER, name_tbl) == 0x1c, "CDDAT_HEADER.name_tbl");
static_assert(offsetof(CDDAT_HEADER, lba_tbl) == 0x20, "CDDAT_HEADER.lba_tbl");
static_assert(offsetof(CDDAT_HEADER, use_cd_image) == 0x24, "CDDAT_HEADER.use_cd_image");

// One name_tbl entry: offsets of the folder ("../bin/data/2ddata/b2d/") and
// file name strings, both NUL-terminated ASCII inside the image.
typedef struct
{
    /* 0x00 */ u_int folder;
    /* 0x04 */ u_int name;
} CDDAT_NAME;                           /* 0x8 */

static_assert(sizeof(CDDAT_NAME) == 0x8, "CDDAT_NAME size");
static_assert(offsetof(CDDAT_NAME, name) == 0x4, "CDDAT_NAME.name");

void  cddatSetHeader(CDDAT_HEADER *hdr);                // FF3 0x0026f340
void  cddatInit(const char *pc_path, void *fhd);        // FF3 0x0026f3d8: from ee_iopInit
char  GetFileExtLabel(int file_no);                     // FF3 0x0026f410
int   GetFileNum(void);                                 // FF3 0x0026f430
int   GetFileStartSector(int file_no);                  // FF3 0x0026f440: 0 without a data image
int   GetFileSectorSize(int file_no);                   // FF3 0x0026f470
u_int GetFileSize(int file_no);         // FF3 0x0026f498: rounded up to 16; (u_int)-1: no file (a label)
u_int GetFileCmpSize(int file_no);                      // FF3 0x0026f518: stored (compressed) size
int   cddatIsCmpFile(int file_no);                      // FF3 0x0026f580: 1 when stored compressed
int   cddatSearchFileName(const char *name);            // FF3 0x0026f5a8: file number or -1
char *GetFileName(int file_no);                         // FF3 0x0026f628: root + folder + name
void  GetFileNameBuffer(int file_no, char *buf);        // FF3 0x0026f6b8

// PC: the file's folder + name from the table ("" when it has none), for the
// host loader's loose-file override. GetFileName prefixes the "host0:" root.
void  ReiPan_CdFilePath(int file, char *buf, int size);

#endif /* _SYSTEM_EEIOP_CDDAT_H */
