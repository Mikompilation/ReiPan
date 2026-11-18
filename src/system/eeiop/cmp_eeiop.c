/* ==========================================================================
 *  system/eeiop/cmp_eeiop.c
 *
 *  EE-side decode of compressed (CMP) files. For a FILE_LOAD_TYPE_DECODE_EE
 *  load the IOP streams the compressed file into tmp_buf_adrs block by block
 *  and rings a SIF command after each one (intr_cmp); the decode thread
 *  inflates each block into the destination with CMP_DecodeOne and signals
 *  decode_sema_id at the end, which thFileLoad waits on (cmp_eeiopWaitSema).
 *
 *  FF2's cmp_eeiop.c (MioPan keeps it as empty bodies, see below) without
 *  cmp_eeiopIsLate; FF3 starts the restarted thread before setting its
 *  priority. FF3 0x270240..0x2705e0.
 *
 *  PC: the host loader (ReiPan_FileLoadServe, in rpcFileLoadReqSub) reads
 *  and decodes a compressed file itself and no IOP posts blocks, so the
 *  decode thread is created but never started and cmp_eeiopWaitSema returns
 *  at once; a started thread would wait on decode_dat_sema_id for ever, and
 *  a host thread blocked there cannot be terminated (eekernel.h).
 * ======================================================================== */
#include "system/eeiop/cmp_eeiop.h"

#include "common/utility2.h"            // GetAlignUp
#include "sdk/eekernel.h"               // CreateSema / CreateThread / ...
#include "system/encodes/cmp.h"         // CMP_Init / CMP_DecodeOne

#define CMP_EEIOP_TMP_BUF_SIZE  0x16000 // compressed blocks; the stack follows
#define CMP_EEIOP_STACK_SIZE    0x1800

static void cmp_eeiopDecodeLoadThread(void *dummy);

static void        *tmp_buf_adrs;               /* sdata 0x372818 */
static int          cmp_th_idx = -1;            /* sdata 0x37281c (gp-0xcd4) */
static void        *cmp_eeiop_stack;            /* sbss 0x373aa0 */
static int          cmp_eeiop_init_priority;    /* sbss 0x373aa4 (gp+0x5b4) */
static int          decode_sema_id;             /* sbss 0x373aa8 (gp+0x5b8): one decode ended */
static int          decode_dat_sema_id;         /* sbss 0x373aac (gp+0x5bc): one block arrived */
static LOAD_REQ_NEW decode_load_ee;             /* bss 0x411150 */

// ──────────────────────────────────────────────────────────────────────────
// Stop the decode thread and release a thFileLoad waiting on it.
// FF3 0x00270240; FF2 cmp_eeiopCancel (unchanged).

void cmp_eeiopCancel(void)
{
    TerminateThread(cmp_th_idx);
    SignalSema(decode_sema_id);
}

// ──────────────────────────────────────────────────────────────────────────
// Work bytes cmp_eeiopInit takes from the file loader's buffer: the
// compressed-block buffer and the decode thread's stack.
// FF3 0x00270268; FF2 cmp_eeiopGetWrkSize.

int cmp_eeiopGetWrkSize(void)
{
    return CMP_EEIOP_TMP_BUF_SIZE + CMP_EEIOP_STACK_SIZE;
}

// ──────────────────────────────────────────────────────────────────────────
// Keep the work buffer, hook the IOP's block doorbell, create the two
// semaphores and the (dormant) decode thread. Returns the next free byte.
// FF3 0x00270278; FF2 cmp_eeiopInit.

void *cmp_eeiopInit(void *wrk_buffer)
{
    struct SemaParam   semap = {};
    struct ThreadParam th_param = {};

    cmp_eeiop_stack = (char *)wrk_buffer + CMP_EEIOP_TMP_BUF_SIZE;
    tmp_buf_adrs    = wrk_buffer;

    // FF3: sceSifInitCmd(); DIntr(); sceSifSetSysCmdBuffer(cmdbuffer, 0x10);
    //      sceSifAddCmdHandler(0, intr_cmp, 0); EIntr();   (cmdbuffer: bss 0x411090)
    // PC: no IOP sends the block command (cmp_eeiop.c banner), so no handler
    // is installed; intr_cmp below is what it ran.
    // callcheck: sceSifInitCmd, DIntr, sceSifSetSysCmdBuffer, sceSifAddCmdHandler, EIntr

    semap.maxCount     = 0xff;
    semap.initCount    = 0;
    decode_dat_sema_id = CreateSema(&semap);

    semap.maxCount  = 1;
    semap.initCount = 0;
    decode_sema_id  = CreateSema(&semap);

    th_param.entry        = cmp_eeiopDecodeLoadThread;
    th_param.stack        = cmp_eeiop_stack;
    th_param.stackSize    = CMP_EEIOP_STACK_SIZE;
    th_param.gpReg        = nullptr;    // PC: FF3 passes &_gp; no $gp on the host
    th_param.initPriority = 0;
    th_param.option       = 0;
    cmp_th_idx = CreateThread(&th_param);

    return (char *)wrk_buffer + cmp_eeiopGetWrkSize();
}

// ──────────────────────────────────────────────────────────────────────────
// SIF command handler: the IOP has put one more compressed block into
// tmp_buf_adrs.
// FF3 0x00270358; FF2 intr_cmp. Not installed on PC (cmp_eeiopInit).

[[maybe_unused]] static void intr_cmp(void *vphdr, void *dat)
{
    (void)vphdr;
    (void)dat;

    iSignalSema(decode_dat_sema_id);
    // FF3: SYNC; EI -- the handler's own exit, nothing to do on the host.
}

// ──────────────────────────────────────────────────────────────────────────
// Describe a decode load for the IOP and restart the decode thread for it.
// The transfer goes to tmp_buf_adrs; the thread inflates into adrs.
// FF3 0x00270380; FF2 cmp_eeiopCreateDecodeThread (FF3 starts the thread
// before raising its priority). one_buf_size / ring_buf_num are set by the
// caller (rpcFileLoadReqSub).

LOAD_REQ_NEW cmp_eeiopCreateDecodeThread(int size, intptr_t adrs, int start_sector, int priority)
{
    decode_load_ee.type            = FILE_LOAD_TYPE_DECODE_EE;
    decode_load_ee.tmp_ee_adrs     = (intptr_t)tmp_buf_adrs;
    decode_load_ee.adrs            = adrs;
    decode_load_ee.ld.start_sector = start_sector;
    decode_load_ee.ld.size         = size;

    // Drop what a cancelled decode left behind.
    while (PollSema(decode_sema_id) != -1)
    {
    }
    while (PollSema(decode_dat_sema_id) != -1)
    {
    }

    // FF3: TerminateThread(cmp_th_idx); ... StartThread(cmp_th_idx, 0);
    // PC: the thread is never started (cmp_eeiop.c banner).
    // callcheck: TerminateThread, StartThread
    cmp_eeiop_init_priority = priority;
    cmp_eeiopChangePriority(priority);

    return decode_load_ee;
}

// ──────────────────────────────────────────────────────────────────────────
// Inflate the blocks of the file in tmp_buf_adrs into decode_load_ee.adrs,
// one per doorbell. Block no goes to no * div_size; the source of the last
// two is read relative to the aligned payload start, of the others from just
// past their own destination (the IOP lays the compressed data out so that
// inflating never overwrites a block not yet decoded).
// FF3 0x00270470; FF2 cmp_eeiopDecodeLoadMain (unchanged).
// PC: never runs; the addresses are FF3's 32-bit arithmetic.

static void cmp_eeiopDecodeLoadMain(void)
{
    CMP_HEADER *header = (CMP_HEADER *)decode_load_ee.tmp_ee_adrs;
    uintptr_t   from_adrs;
    uintptr_t   to_adrs;
    int         left;

    CMP_Init(header);

    for (int no = 0; no < header->div_num; no++)
    {
        WaitSema(decode_dat_sema_id);

        left    = no - header->div_num;
        to_adrs = (uintptr_t)(no * header->div_size + decode_load_ee.adrs);
        if (left == -2)
        {
            from_adrs = GetAlignUp(header->data_offset, 6);
        }
        else if (left < -2)
        {
            from_adrs = to_adrs + header->div_size;
        }
        else
        {
            from_adrs = GetAlignUp(header->data_offset, 6) + header->div_size;
        }

        CMP_DecodeOne(header, no, from_adrs, to_adrs);
        ChangeThreadPriority(cmp_th_idx, cmp_eeiop_init_priority);
    }
}

// ──────────────────────────────────────────────────────────────────────────
// The decode thread: wait for the header block, decode, report the end.
// FF3 0x00270558; FF2 cmp_eeiopDecodeLoadThread.

static void cmp_eeiopDecodeLoadThread(void *dummy)
{
    (void)dummy;

    WaitSema(decode_dat_sema_id);
    cmp_eeiopDecodeLoadMain();
    SignalSema(decode_sema_id);
    // FF3: ExitThread(); PC: returning ends the host thread.
    // callcheck: ExitThread
}

// ──────────────────────────────────────────────────────────────────────────
// Wait until the decode thread has inflated the whole file.
// FF3 0x00270590; FF2 cmp_eeiopWaitSema.

void cmp_eeiopWaitSema(void)
{
    // FF3: WaitSema(decode_sema_id);
    // PC: the host loader has decoded the file by the time thFileLoad gets
    // here and no decode thread runs, so nothing would signal it.
    // callcheck: WaitSema
}

// ──────────────────────────────────────────────────────────────────────────
// FF3 0x002705b0; FF2 cmp_eeiopChangePriority (unchanged).

void cmp_eeiopChangePriority(int priority)
{
    if (cmp_th_idx >= 0)
    {
        ChangeThreadPriority(cmp_th_idx, priority);
    }
}
