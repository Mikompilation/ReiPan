// system/eeiop/ee_iop_q.c -- the EE side of the IOP query service (RPC 2).
//
// One call per request, each a synchronous sceSifCallRpc under query_sema_id:
// the IOP's iopCommandQuery (system/iop/iop.c) answers REQ_FILE_SIZE,
// REQ_Q_LOAD_CANCEL, REQ_SPU_TRANS_CORE_GET and REQ_SPU_TRANS_CORE_RELEASE.
// FF3 0x0026b3f0..0x0026b610; FF2 ee_iop_q.c.

#include "system/eeiop/ee_iop_q.h"

#include <eekernel.h>                   // CreateSema / WaitSema / SignalSema / FlushCache
#include <sifrpc.h>                     // sceSifClientData / sceSifBindRpc / sceSifCallRpc

// The query service's RPC number (the IOP registers it as RPC_QUERY).
#define EE_IOP_RPC_QUERY 2

static int query_sema_id;                                   // gp-0x548

// SIF DMA endpoints, 64-byte aligned in FF3.
alignas(64) static char iop_query_buffer[0x200];            // bss 0x40f240 (reply)
alignas(64) static char iop_query_arg[0x200];               // bss 0x40f440 (argument)
static sceSifClientData sif_cli_data_q;                     // bss 0x40f640

// ──────────────────────────────────────────────────────────────────────
// Create the query lock and bind the IOP's query service, retrying until
// the IOP has registered it (from ee_iopInitSub, after REQ_IOP_REBOOT).
// FF3 0x0026b3f0; FF2 ee_iopQueryInit (without the printf).

void ee_iopQueryInit(void)
{
    struct SemaParam semap;

    semap.maxCount  = 1;
    semap.initCount = 1;
    query_sema_id = CreateSema(&semap);

    do
    {
        if (sceSifBindRpc(&sif_cli_data_q, EE_IOP_RPC_QUERY, 0) < 0)
        {
            for (;;)
            {
            }
        }

        // FF3 spins 2500 iterations of nops here before looking at serve.
        // PC: no delay; the host IOP registered RPC 2 inside REQ_IOP_REBOOT,
        // so the first bind already sees it.
    } while (sif_cli_data_q.serve == nullptr);
}

// ──────────────────────────────────────────────────────────────────────
// Host file size query. FF2's debug build asked the IOP (host0:); retail FF3
// compiled the body away, so *ps is left as the caller had it.
// FF3 0x0026b4d0; FF2 QueryFileSize.

void QueryFileSize(int file_no, unsigned int *ps)
{
    (void)file_no;
    (void)ps;
}

// ──────────────────────────────────────────────────────────────────────
// Tell the IOP file server to drop its pending load requests.
// FF3 0x0026b4d8; FF2 ReqQueryLoadCancel.

void ReqQueryLoadCancel(void)
{
    WaitSema(query_sema_id);
    FlushCache(0);
    sceSifCallRpc(&sif_cli_data_q, REQ_Q_LOAD_CANCEL, 0, nullptr, 0, nullptr, 0,
                  nullptr, nullptr);
    SignalSema(query_sema_id);
}

// ──────────────────────────────────────────────────────────────────────
// Claim an SPU2 core for a movie's audio auto-DMA; the IOP takes it out of
// its transfer pool and clears its completion handler. *core gets its number.
// FF3 0x0026b530; FF2 ReqQuerySPUTransCoreGet.

void ReqQuerySPUTransCoreGet(int *core)
{
    WaitSema(query_sema_id);
    FlushCache(0);
    sceSifCallRpc(&sif_cli_data_q, REQ_SPU_TRANS_CORE_GET, 0, nullptr, 0,
                  iop_query_buffer, sizeof(iop_query_buffer), nullptr, nullptr);
    *core = *(int *)iop_query_buffer;
    SignalSema(query_sema_id);
}

// ──────────────────────────────────────────────────────────────────────
// Give a core claimed with ReqQuerySPUTransCoreGet back to the IOP's pool.
// FF3 0x0026b5a8; FF2 ReqQuerySPUTransCoreRelease.

void ReqQuerySPUTransCoreRelease(int core)
{
    WaitSema(query_sema_id);
    FlushCache(0);
    *(int *)iop_query_arg = core;
    sceSifCallRpc(&sif_cli_data_q, REQ_SPU_TRANS_CORE_RELEASE, 0,
                  iop_query_arg, sizeof(iop_query_arg), nullptr, 0, nullptr, nullptr);
    SignalSema(query_sema_id);
}
