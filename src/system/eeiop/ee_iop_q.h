/* ==========================================================================
 *  system/eeiop/ee_iop_q.h
 *
 *  EE->IOP query RPCs (RPC 2, FF2 ee_iop_q.c): one synchronous call per
 *  request, served by the IOP's iopCommandQuery (system/iop/iop.c).
 *  FF3 0x0026b3f0..0x0026b610.
 * ======================================================================== */
#ifndef _SYSTEM_EEIOP_EE_IOP_Q_H
#define _SYSTEM_EEIOP_EE_IOP_Q_H

// Query function numbers (sceSifCallRpc fno); the IOP side's
// IOP_COMMAND_QUERY_ENUM in system/iop/iop_snd_def.h uses the same values.
typedef enum
{
    REQ_FILE_SIZE              = 0,
    REQ_Q_LOAD_CANCEL          = 1,
    REQ_SPU_TRANS_CORE_GET     = 2,
    REQ_SPU_TRANS_CORE_RELEASE = 3
} EE_IOP_QUERY_ENUM;

// Create the query lock and bind the IOP's query service (from ee_iopInitSub).
void ee_iopQueryInit(void);                                 // FF3 0x0026b3f0

// Host file size of file_no into *ps (the debug build's host0: path for
// GetFileSize / GetFileCmpSize). Empty in FF3 retail.
void QueryFileSize(int file_no, unsigned int *ps);          // FF3 0x0026b4d0

// Tell the IOP file server to drop its pending load requests.
void ReqQueryLoadCancel(void);                              // FF3 0x0026b4d8

// Claim / give back an SPU2 core for a movie's audio auto-DMA (movie.c).
void ReqQuerySPUTransCoreGet(int *core);                    // FF3 0x0026b530
void ReqQuerySPUTransCoreRelease(int core);                 // FF3 0x0026b5a8

#endif /* _SYSTEM_EEIOP_EE_IOP_Q_H */
