/* ==========================================================================
 *  system/eeiop/cmp_eeiop.h
 *
 *  EE-side decode of compressed (CMP) files for the file loader (FF2
 *  cmp_eeiop.c; FF3 0x270240..0x2705e0). cmp_eeiopInit takes the decode work
 *  (compressed-block buffer and the decode thread's stack) out of the
 *  loader's buffer; cmp_eeiopCreateDecodeThread describes a decode load for
 *  the IOP and restarts the thread, which inflates each block the IOP posts.
 *
 *  PC: the platform's host loader (ReiPan_FileLoadServe) reads and decodes a
 *  compressed file itself, so the decode thread is never started and
 *  cmp_eeiopWaitSema does not block (see cmp_eeiop.c).
 * ======================================================================== */
#ifndef _SYSTEM_EEIOP_CMP_EEIOP_H
#define _SYSTEM_EEIOP_CMP_EEIOP_H

#include "system/eeiop/fileload.h"      // LOAD_REQ_NEW

void        *cmp_eeiopInit(void *wrk_buffer);       // 0x270278: returns the next free byte
int          cmp_eeiopGetWrkSize(void);              // 0x270268: 0x17800
// 0x270380: restart the decode thread for one load and describe the transfer.
LOAD_REQ_NEW cmp_eeiopCreateDecodeThread(int size, intptr_t adrs, int start_sector, int priority);
void         cmp_eeiopWaitSema(void);                // 0x270590: wait for the decode to end
void         cmp_eeiopCancel(void);                  // 0x270240
void         cmp_eeiopChangePriority(int priority);  // 0x2705b0

#endif /* _SYSTEM_EEIOP_CMP_EEIOP_H */
