/* ==========================================================================
 *  system/eeiop/ee_iop.c
 *
 *  The EE side of the EE<->IOP link (FF3 0x26ad30..0x26b3f0, FF2's ee_iop.c):
 *
 *    - bring-up: ee_iopInit reboots the IOP, loads the game's IRX modules
 *      (the HIL index over the DIL image), lets ee_iopInitSub bind the RPC
 *      channel and start the file loader, starts the sound system, tells the
 *      IOP the data image's name and loads the file header table for cddat.c;
 *    - the command queue: iopCommandRegister appends (command, payload) pairs
 *      to iop_com_buffer and ee_iopMain sends the batch once a frame;
 *    - the status block: the reply to that call, read by the accessors at the
 *      bottom.
 *
 *  From MioPan system/eeiop/ee_iop.c. FF3 dropped every print and assert, the
 *  command buffer shrank from 4 KB to 0x200 bytes, ee_iopInit took over the
 *  REQ_SET_CD_DAT send and the file-table load from FF2's cddatInit callers,
 *  and GetVoiceNowAdrs / GetVoiceLoopAdrs are gone.
 *
 *  PC: the IOP is hosted. The SIF shims dispatch each RPC straight to the IOP
 *  module in src/system/iop/ (MioPan's port of FF2's iopsys.irx, standing in
 *  for FF3's ZEROIRX), which answers with the status block; the IRX load is
 *  skipped (ee_iop_load_irx).
 * ======================================================================== */
#include "system/eeiop/ee_iop.h"

#include <eekernel.h>                   // FlushCache
#include <fileio.h>                     // sceOpen / sceLseek / sceRead / sceClose
#include <libcdvd.h>                    // sceCdInit / sceCdMmode
#include <sif.h>                        // sceSifAllocSysMemory
#include <sifdev.h>                     // sceSifRebootIop / sceSifLoadModuleBuffer / iop heap
#include <sifrpc.h>                     // sceSifBindRpc / sceSifCallRpc / sceSifCheckStatRpc
#include <string.h>

#include "common/utility2.h"            // GetAlignUp
#include "system/eeiop/cddat.h"         // cddatInit
#include "system/eeiop/ee_iop_q.h"      // ee_iopQueryInit
#include "system/eeiop/file_stream.h"   // FileStreamInit
#include "system/eeiop/fileload.h"      // FileLoadGetNeedSize / FileLoadInit
#include "system/eeiop/snd.h"           // sndGetNeedSize / SndInit / SndMain

// The HIL file is an index over the DIL image: this header, then one
// HIL_ONE_FORMAT per IRX module (ee_iopInit reads num at +0x0, dil_size at
// +0x4 and the first record's offset at +0x18).
typedef struct
{
    /* 0x00 */ int num;
    /* 0x04 */ int dil_size;
    /* 0x08 */ int padding[2];
} HIL_FORMAT;
static_assert(sizeof(HIL_FORMAT) == 0x10, "HIL_FORMAT size");
static_assert(offsetof(HIL_FORMAT, num) == 0x0, "HIL_FORMAT.num");
static_assert(offsetof(HIL_FORMAT, dil_size) == 0x4, "HIL_FORMAT.dil_size");

typedef struct
{
    /* 0x00 */ char irx_name[8];            // not NUL-terminated
    /* 0x08 */ int  offset;                 // into the DIL image
    /* 0x0c */ int  size;
} HIL_ONE_FORMAT;
static_assert(sizeof(HIL_ONE_FORMAT) == 0x10, "HIL_ONE_FORMAT size");
static_assert(offsetof(HIL_ONE_FORMAT, offset) == 0x8, "HIL_ONE_FORMAT.offset");

// PC: the IRX modules in ZEROIRX.DIL are R3000 code nothing here can run, and
// the SIF shims hand out no IOP memory to load them into; FF3 returns 0 from
// ee_iopInit on any failure in that block, which would leave the file table
// unloaded. The block is compiled and skipped; the reboot and resets around it
// are shims that succeed.
static const int ee_iop_load_irx = 0;

static void *(*ee_iop_malloc)(int size);                    // gp-0x53c
static void  (*ee_iop_free)(void *adrs);                    // gp-0x540
static int    iop_com_offset;                               // gp-0x544

// SIF DMA endpoints, 64-byte aligned in FF3 (bss 0x40ec80 / 0x40f040).
alignas(64) static char iop_com_buffer[0x200];              // bss 0x40ec80
static IOP_RET_STATUS   iop_ret;                            // bss 0x40ee80
alignas(64) static char iop_ret_buffer[0x1c0];              // bss 0x40f040
static sceSifClientData sif_cli_data;                       // bss 0x40f200

// ──────────────────────────────────────────────────────────────────────────
// Allocation: both go through EEIOP_DEF, so the caller's heap owns everything
// this layer and the loader / sound modules allocate.
// ──────────────────────────────────────────────────────────────────────────

// FF3 0x0026aea8
void *ee_iopMalloc(int size)
{
    return ee_iop_malloc(size);
}

// FF3 0x0026aec8
void ee_iopFree(void *adrs)
{
    ee_iop_free(adrs);
}

// FF3 0x0026aee8: one buffer for both halves; ee_iopInitSub lays the file
// loader's tables down first and SndInit carves the rest.
int ee_iopGetNeedSize(EEIOP_DEF *def)
{
    return sndGetNeedSize(def) + FileLoadGetNeedSize(def);
}

// ──────────────────────────────────────────────────────────────────────────
// Bring-up
// ──────────────────────────────────────────────────────────────────────────

// FF3 0x0026ad30: bind the IOP's main service (RPC 1), send REQ_IOP_REBOOT in
// one synchronous round trip (it starts the IOP's query / load / stream
// servers), then bind those and start the file loader. Returns the rest of
// buffer past the loader's tables.
static void *ee_iopInitSub(EEIOP_DEF *def, void *buffer)
{
    void *next;

    // FF3 spins here until the service has registered:
    //   do { if (bind < 0) for (;;); delay 2500; } while (sif_cli_data.serve == NULL);
    // PC: bind once and go on (as FileLoadInit does). The host IOP's start()
    // registers RPC 1 before the first bind returns, so there is nothing to
    // wait for.
    if (sceSifBindRpc(&sif_cli_data, 1, 0) < 0)
    {
        for (;;)
        {
        }
    }

    iopCommandFrameInit();
    iopCommandRegister(REQ_IOP_REBOOT, nullptr, 0);
    iopCommandRegister(IOP_COM_END, nullptr, 0);

    FlushCache(0);
    sceSifCallRpc(&sif_cli_data, 0, 0, iop_com_buffer, GetAlignUp(iop_com_offset, 4),
                  nullptr, 0, nullptr, nullptr);

    ee_iopQueryInit();
    next = FileLoadInit(def, buffer);
    FileStreamInit();
    iopCommandFrameInit();

    return next;
}

// FF3 0x0026af28: returns 1, or 0 when a file cannot be read or a module
// does not start.
int ee_iopInit(EEIOP_DEF *def)
{
    int   fd;
    int   size;
    void *buffer;
    SET_CD_DAT set_cd_dat;

    ee_iop_malloc = def->malloc64;
    ee_iop_free   = def->free64;

    if (def->rom_boot != 0)
    {
        sceSifInitRpc(0);
        sceCdInit(0);
        while (sceSifRebootIop(def->iop_def_module) == 0)
        {
        }
        while (sceSifSyncIop() == 0)
        {
        }
    }

    sceSifInitRpc(0);
    sceSifLoadFileReset();
    sceFsReset();
    sceCdInit(0);
    sceCdMmode(def->media);
    sceSifInitIopHeap();

    if (ee_iop_load_irx)
    {
        HIL_FORMAT *hil;
        void       *iopaddr;
        int         i;

        fd = sceOpen(def->hil_file_name, SCE_RDONLY);
        if (fd < 0)
        {
            return 0;
        }
        size = sceLseek(fd, 0, SCE_SEEK_END);
        sceLseek(fd, 0, SCE_SEEK_SET);
        hil = (HIL_FORMAT *)ee_iopMalloc(size);
        if (sceRead(fd, hil, size) != size)
        {
            return 0;
        }
        sceClose(fd);

        // 0x120000 is where the IOP side expects the image; otherwise anywhere.
        iopaddr = sceSifAllocSysMemory(2, hil->dil_size, (void *)0x120000);
        if (iopaddr == nullptr)
        {
            iopaddr = sceSifAllocSysMemory(0, hil->dil_size, nullptr);
        }
        if (sceSifLoadIopHeap(def->dil_file_name, iopaddr) < 0)
        {
            return 0;
        }

        HIL_ONE_FORMAT *work = (HIL_ONE_FORMAT *)(hil + 1);
        for (i = 0; i < hil->num; i++)
        {
            if (sceSifLoadModuleBuffer((char *)iopaddr + work[i].offset, 0, nullptr) < 0)
            {
                return 0;
            }
        }

        // The index and the staging image are dead once the modules run.
        ee_iopFree(hil);
        sceSifFreeIopHeap(iopaddr);
    }

    buffer = ee_iopMalloc(ee_iopGetNeedSize(def));
    buffer = ee_iopInitSub(def, buffer);
    SndInit(def, buffer);

    // FF2's cddatInit sent the image name; FF3 does it here, and only when the
    // game reads from the data image.
    if (def->cd_dat_name != nullptr)
    {
        strcpy(set_cd_dat.file_name, def->cd_dat_name);
        iopCommandRegister(REQ_SET_CD_DAT, &set_cd_dat, sizeof(set_cd_dat));
    }

    // The file header table, whole, into IOP-layer memory for cddat.c.
    fd = sceOpen(def->fhd_file_name, SCE_RDONLY);
    if (fd < 0)
    {
        return 0;
    }
    size = sceLseek(fd, 0, SCE_SEEK_END);
    sceLseek(fd, 0, SCE_SEEK_SET);
    buffer = ee_iopMalloc(size);
    if (sceRead(fd, buffer, size) != size)
    {
        return 0;
    }
    sceClose(fd);
    cddatInit(def->pc_path, buffer);

    return 1;
}

// ──────────────────────────────────────────────────────────────────────────
// Frame pump
// ──────────────────────────────────────────────────────────────────────────

// FF3 0x0026b1a8
void WaitMainRpc(void)
{
    while (sceSifCheckStatRpc(&sif_cli_data.rpcd) != 0)
    {
    }
}

// FF3 0x0026b1d8: once a frame. The last frame's call has to have landed
// before its reply is read and the queue reused.
void ee_iopMain(void)
{
    SndMain();
    WaitMainRpc();

    iop_ret = *(IOP_RET_STATUS *)iop_ret_buffer;

    iopCommandRegister(IOP_COM_END, nullptr, 0);

    // The IOP reads the queue over the bus, past the cache.
    FlushCache(0);
    sceSifCallRpc(&sif_cli_data, 0, 1, iop_com_buffer, GetAlignUp(iop_com_offset, 4),
                  iop_ret_buffer, GetAlignUp(sizeof(IOP_RET_STATUS), 4), nullptr, nullptr);

    iopCommandFrameInit();
}

// ──────────────────────────────────────────────────────────────────────────
// Command queue
// ──────────────────────────────────────────────────────────────────────────

// FF3 0x0026b2f0
void iopCommandFrameInit(void)
{
    iop_com_offset = 0;
}

// FF3 0x0026b2f8: the command word, then size bytes of payload. 0 when the
// queue is full (FF2 printed "iop_command_buffer over" and asserted 4-byte
// payloads; FF3 does neither).
int iopCommandRegister(IOP_COMMAND_ENUM command, void *buf, int size)
{
    if (iop_com_offset + size + 4 >= (int)sizeof(iop_com_buffer))
    {
        return 0;
    }

    *(int *)(iop_com_buffer + iop_com_offset) = command;
    iop_com_offset += 4;
    if (size != 0)
    {
        memcpy(iop_com_buffer + iop_com_offset, buf, size);
        iop_com_offset += size;
    }

    return 1;
}

// FF3 0x0026b370
void SetCDReadMode(int mode)
{
    CD_READ_MODE_CHANGE crm;

    crm.mode = mode;
    iopCommandRegister(REQ_CD_READ_MODE_CHANGE, &crm, sizeof(crm));
}

// ──────────────────────────────────────────────────────────────────────────
// IOP status block (ee_iopMain's copy of what iopCommand answered).
// ──────────────────────────────────────────────────────────────────────────

// FF3 0x0026b398
int CheckEndPointThrough(int core, int voice_no)
{
    return iop_ret.voice_end[core] & (1 << voice_no);
}

// FF3 0x0026b3c0
IOP_STREAM_RET *GetStreamWrkRet(int wrk_id)
{
    return &iop_ret.stream_ret[wrk_id];
}

// FF3 0x0026b3d8
IOP_STREAM_RET *GetPCMStreamWrkRet(int wrk_id)
{
    return &iop_ret.pcm_stream_ret[wrk_id];
}
