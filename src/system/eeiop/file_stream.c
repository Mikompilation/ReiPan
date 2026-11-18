/* ==========================================================================
 *  system/eeiop/file_stream.c
 *
 *  The EE half of the file streamer: three RPC payloads to the IOP's stream
 *  service (RPC 4) and a lock flag that nothing reads. The ring buffer, its
 *  reader thread and the file position live on the IOP.
 *
 *  From MioPan system/eeiop/file_stream.c (FF2's). FF3 sends no file name
 *  (the IOP finds the file by sector, as for fileload.c's LOAD_DEF_STRUCT)
 *  and drops FileStreamRead's two asserts. FF3 0x270a48..0x270cf8.
 *
 *  PC: the IOP module (src/system/iop/, MioPan's FF2 iopsys port) registers
 *  the stream service (RPC 4) synchronously when REQ_IOP_REBOOT arrives, so
 *  FileStreamInit binds once instead of spinning. Movies are read by the
 *  platform's own stream (reipan/io/reipan_moviestream.cpp) first; this path
 *  is the fallback.
 * ======================================================================== */
#include "system/eeiop/file_stream.h"

#include <sifrpc.h>                     // sceSifClientData / sceSifBindRpc / sceSifCallRpc

#include "sdk/eekernel.h"               // FlushCache
#include "system/eeiop/cddat.h"         // GetFileStartSector / GetFileSize
#include "system/eeiop/fileload.h"      // LOAD_DEF_STRUCT

#define FILE_STREAM_RPC_NO  4

// RPC 4 function numbers (FF2 REQ_STM_*).
enum FILE_STREAM_REQ
{
    REQ_STM_START = 0,
    REQ_STM_READ  = 1,
    REQ_STM_STOP  = 2,
};

// FileStreamRead's payload.
// layout: host -- ee_buf is a pointer (4 bytes on the EE); the payload never
// reaches an FF3 IOP on the host.
typedef struct STM_READ
{
    int   read_size;
    void *ee_buf;
} STM_READ;

// The request window (0x40 bytes on the EE): a start or a read request.
typedef union FILE_STM_ARG
{
    LOAD_DEF_STRUCT start;              // REQ_STM_START
    STM_READ        read;               // REQ_STM_READ
    int             word[16];
} FILE_STM_ARG;

static_assert(sizeof(FILE_STM_ARG) == 0x40, "FILE_STM_ARG is FF3's 0x40-byte window");

static int              iop_file_stm_ret[16];   /* bss 0x411180: word 0 = bytes read */
static FILE_STM_ARG     iop_file_stm_arg;       /* bss 0x4111c0 */
static sceSifClientData sif_cli_data_s;         /* bss 0x411200 */
static int              file_stream_lock;       /* sbss gp+0x5c8: set while a stream is open */

// ──────────────────────────────────────────────────────────────────────────
// Bind the IOP's stream service.
// FF3 0x00270a48; FF2 FileStreamInit without the error print.

void FileStreamInit(void)
{
    // FF3 spins until the IOP has registered the service:
    //   do { if (bind < 0) for (;;); delay 10000; } while (sif_cli_data_s.serve == NULL);
    // PC: bind once and go on; the host IOP registers its services
    // synchronously (banner), so there is nothing to wait for.
    if (sceSifBindRpc(&sif_cli_data_s, FILE_STREAM_RPC_NO, 0) < 0)
    {
        for (;;)
        {
        }
    }

    file_stream_lock = 0;
}

// ──────────────────────────────────────────────────────────────────────────
// Open file_no as a stream: the IOP gets its start sector and size from the
// EE's file table and keeps ring_buf_num buffers of one_buf_sector sectors.
// FF3 0x00270b18; FF2 FileStreamStart without the file name.

int FileStreamStart(int file_no, int ring_buf_num, int one_buf_sector)
{
    iop_file_stm_arg.start.start_sector = GetFileStartSector(file_no);
    iop_file_stm_arg.start.one_buf_size = one_buf_sector << 11;
    iop_file_stm_arg.start.ring_buf_num = ring_buf_num;
    iop_file_stm_arg.start.size         = (int)GetFileSize(file_no);

    sceSifCallRpc(&sif_cli_data_s, REQ_STM_START, 0, &iop_file_stm_arg, sizeof(iop_file_stm_arg),
                  iop_file_stm_ret, sizeof(iop_file_stm_ret), nullptr, nullptr);

    file_stream_lock = 1;
    return 1;
}

// ──────────────────────────────────────────────────────────────────────────
// Ask for size bytes into buff, waiting for them when block_read is set.
// FF3 0x00270bc8; FF2 FileStreamRead without its two asserts (size a whole
// number of sectors, no read in flight).

int FileStreamRead(void *buff, int size, int block_read)
{
    iop_file_stm_arg.read.read_size = size;
    iop_file_stm_arg.read.ee_buf    = buff;
    FlushCache(0);

    sceSifCallRpc(&sif_cli_data_s, REQ_STM_READ, block_read == 0 ? SIF_RPC_M_NOWAIT : 0,
                  &iop_file_stm_arg, sizeof(iop_file_stm_arg),
                  iop_file_stm_ret, sizeof(iop_file_stm_ret), nullptr, nullptr);

    return iop_file_stm_ret[0];
}

// ──────────────────────────────────────────────────────────────────────────
// FF3 0x00270c68; FF2 FileStreamIsAct (unchanged).

int FileStreamIsAct(void)
{
    return sceSifCheckStatRpc(&sif_cli_data_s.rpcd) != 0;
}

// ──────────────────────────────────────────────────────────────────────────
// Close the stream once the read in flight has landed (the IOP frees the
// ring it transfers from).
// FF3 0x00270c90; FF2 FileStreamStop (unchanged).

int FileStreamStop(void)
{
    while (sceSifCheckStatRpc(&sif_cli_data_s.rpcd) != 0)
    {
    }

    sceSifCallRpc(&sif_cli_data_s, REQ_STM_STOP, 0, &iop_file_stm_arg, sizeof(iop_file_stm_arg),
                  iop_file_stm_ret, sizeof(iop_file_stm_ret), nullptr, nullptr);

    file_stream_lock = 0;
    return 1;
}
