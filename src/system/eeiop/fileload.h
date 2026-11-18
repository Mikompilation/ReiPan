/* ==========================================================================
 *  system/eeiop/fileload.h
 *
 *  EE-side asynchronous file loader (FF3 0x268e70..0x269fa0; FF2 fileload.c).
 *  A loader thread (thFileLoad) services a queue of FILE_LOAD_WRK requests
 *  sorted by priority and, within a priority, by start sector. Callers queue a
 *  load with FileLoadReq{EE,SPU} / FileDecodeLoadReqEE, poll it with
 *  FileLoadIsEnd{,2} / AllFileLoadIsEnd and can withdraw it with the
 *  FileLoadCancel* family. A request id is packed as (slot << 16) | temp_id.
 *
 *  From MioPan system/eeiop/fileload.h; FF3's load command lost FF2's file
 *  name (the IOP finds a file by sector only), so LOAD_REQ_NEW is 0x1c bytes.
 * ======================================================================== */
#ifndef _SYSTEM_EEIOP_FILELOAD_H
#define _SYSTEM_EEIOP_FILELOAD_H

#include <stddef.h>
#include <stdint.h>

#include "system/eeiop/ee_iop.h"        // EEIOP_DEF

// Result of a cancel / all-loaded query (FF2's).
enum FILE_LOAD_ENUM
{
    FILE_LOAD_OK               = 0,
    FILE_LOAD_ALREADY_LOAD     = 1,
    FILE_LOAD_ALREADY_ALL_LOAD = 2,
    FILE_LOAD_ID_NOT_EXIST     = 3,
};

// How a queued file is delivered: raw to EE, to SPU RAM, or decoded (CMP) on
// the EE / IOP. Stored as an int in FILE_LOAD_WRK and LOAD_REQ_NEW.
enum FILE_LOAD_TYPE
{
    FILE_LOAD_TYPE_EE         = 0,
    FILE_LOAD_TYPE_SPU        = 1,
    FILE_LOAD_TYPE_DECODE_EE  = 2,
    FILE_LOAD_TYPE_DECODE_IOP = 3,
};

// The IOP ring-buffer transfer description (FF2's LOAD_DEF_STRUCT without
// its 256-byte file_name). 0x10.
struct LOAD_DEF_STRUCT
{
    /* 0x00 */ int one_buf_size;
    /* 0x04 */ int ring_buf_num;
    /* 0x08 */ int start_sector;
    /* 0x0c */ int size;
};
static_assert(sizeof(LOAD_DEF_STRUCT) == 0x10, "LOAD_DEF_STRUCT size");
static_assert(offsetof(LOAD_DEF_STRUCT, one_buf_size) == 0x00, "LOAD_DEF_STRUCT.one_buf_size");
static_assert(offsetof(LOAD_DEF_STRUCT, ring_buf_num) == 0x04, "LOAD_DEF_STRUCT.ring_buf_num");
static_assert(offsetof(LOAD_DEF_STRUCT, start_sector) == 0x08, "LOAD_DEF_STRUCT.start_sector");
static_assert(offsetof(LOAD_DEF_STRUCT, size) == 0x0c, "LOAD_DEF_STRUCT.size");

// The load command rpcFileLoadReqSub sends to the IOP file server (in a
// 0x40-byte buffer, bss 0x40e240); cmp_eeiopCreateDecodeThread fills it for a
// decode load. 0x1c on the EE.
// layout: host -- tmp_ee_adrs / adrs hold EE addresses, host pointers here.
struct LOAD_REQ_NEW
{
    /* 0x00 */ int             type;            // FILE_LOAD_TYPE
    /* 0x04 */ intptr_t        tmp_ee_adrs;     // decode: the compressed-data buffer
    /* 0x08 */ intptr_t        adrs;            // destination
    /* 0x0c */ LOAD_DEF_STRUCT ld;
};
static_assert(offsetof(LOAD_REQ_NEW, type) == 0x00, "LOAD_REQ_NEW.type");

// Called with the destination buffer once a load finished or was cancelled.
typedef void (*FILE_LOAD_CALLBACK)(void *buffer, void *arg);

// ---- boot-time sizing / bring-up (ee_iopInitSub) ---------------------------
int   FileLoadGetNeedSize(EEIOP_DEF *def);                      // 0x2696e0
void *FileLoadInit(EEIOP_DEF *def, void *wrk_buffer);           // 0x269738

// ---- queue a load (file_no: CD_FILE_DAT) -----------------------------------
int   FileLoadReqEE(int file_no, void *adrs, int priority, FILE_LOAD_CALLBACK func, void *arg);
int   FileLoadReqSPU(int file_no, void *adrs, int priority, FILE_LOAD_CALLBACK func, void *arg);
int   FileDecodeLoadReqEE(int file_no, void *adrs, int priority, FILE_LOAD_CALLBACK func, void *arg);
void  FileLoadReqEEWait(int file_no, void *adrs);

// ---- poll completion -------------------------------------------------------
int   FileLoadIsEnd(int id);                    // 1 when that load is no longer queued
int   FileLoadIsEnd2(int file_no, void *adrs);  // same, by file and buffer (not SPU)
int   AllFileLoadIsEnd(void);                   // 1 when the queue is empty

// ---- cancel ----------------------------------------------------------------
FILE_LOAD_ENUM FileLoadCancel(int id, FILE_LOAD_CALLBACK func, void *arg);
FILE_LOAD_ENUM FileLoadCancel2(int file_no, void *adrs, FILE_LOAD_CALLBACK func, void *arg);
FILE_LOAD_ENUM FileLoadCancelSPU2(int file_no, void *adrs, FILE_LOAD_CALLBACK func, void *arg);
FILE_LOAD_ENUM FileLoadCancelWait(int id);
FILE_LOAD_ENUM FileLoadCancelWait2(int file_no, void *adrs);
FILE_LOAD_ENUM FileLoadCancelAll(void);

// ---- misc ------------------------------------------------------------------
void  FileLoadChangeThreadPriority(int load_th_priority, int decode_th_priority);

#endif /* _SYSTEM_EEIOP_FILELOAD_H */
