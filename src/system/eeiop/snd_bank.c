/* ==========================================================================
 *  system/eeiop/snd_bank.c
 *
 *  SE banks. SndBankInitAll carves its work buffer into three tables of num
 *  entries -- the banks, then the BD file records, then the HXD file
 *  records. SndBankFileSearch lets two banks naming the same file share one
 *  record and one load; the reference count decides whether a release frees
 *  anything. A release while the load is still in flight cancels it, with a
 *  callback that frees the buffer once the cancel takes.
 *
 *  From MioPan system/eeiop/snd_bank.c (FF2's snd_bank.c). FF3 dropped every
 *  assert and print, the bank id field, the load-priority accessors,
 *  SndBankPrintStatus and the HXD check on load (CheckHXDData), and fixed
 *  SndBankIsLoopSnd to read sample no instead of sample 0.
 * ======================================================================== */
#include "system/eeiop/snd_bank.h"

#include <stdint.h>                     // uintptr_t
#include <string.h>                     // memset

#include "common/utility2.h"            // GetAlignUp
#include "system/eeiop/cddat.h"         // GetFileSize
#include "system/eeiop/ee_iop.h"        // ee_iopMalloc / ee_iopFree
#include "system/eeiop/fileload.h"      // FileLoadReqSPU / FileLoadReqEE / FileLoadCancel*
#include "system/eeiop/snd_buffer.h"    // SndBufPlay, CSND_BUF_PLAY_NO_ID
#include "system/eeiop/spu_mem.h"       // GetSPUMemory / ReleaseSPUMemory

static int            snd_bank_max;             /* sbss gp+0x578 */
static SND_BANK      *snd_bank;                 /* sbss gp+0x57c */
static SND_BANK_FILE *snd_bd_file;              /* sbss gp+0x580 */
static SND_BANK_FILE *snd_hxd_file;             /* sbss gp+0x584 */
static int            snd_bank_load_priority;   /* sbss gp+0x588 */

static int  SndBankFileReqBD(SND_BANK *p_Sb, int file_no);
static int  SndBankFileReqHXD(SND_BANK *p_Sb, int file_no);
static void ReleaseSndBDFile(SND_BANK_FILE *file);
static void ReleaseSndHXDFile(SND_BANK_FILE *file);
static void intrSndBankFileBD(void *buffer, void *arg);
static void intrSndBankFileHXD(void *buffer, void *arg);
static HXD_HEADER *snd_bankGet_pHxdHeader(SND_BANK *p_Sb);
static SOUND_INFO *snd_bankGet_pSOUND_INFO(SND_BANK *p_Sb);
static int  snd_bankIsReady(SND_BANK *p_Sb);

// ──────────────────────────────────────────────────────────────────────────
// One bank's share of the work buffer: the bank and its two file records.
// FF3 0x0026e0b0; from MioPan snd_bank.c SndBankGetOneWrkSize.

int SndBankGetOneWrkSize(void)
{
    // PC: 0x30 on the EE (three 0x10 records); host pointers make them larger.
    return sizeof(SND_BANK) + sizeof(SND_BANK_FILE) * 2;
}

// ──────────────────────────────────────────────────────────────────────────
// FF3 0x0026e0b8; from MioPan snd_bank.c SndBankInitAll (no bank ids).

void *SndBankInitAll(void *buffer, int num, int load_priority)
{
    int i;

    snd_bank_load_priority = load_priority;
    snd_bank_max           = num;
    snd_bank               = (SND_BANK *)buffer;

    for (i = 0; i < num; i++)
    {
        snd_bank[i].pSndBDFile  = nullptr;
        snd_bank[i].pSndHXDFile = nullptr;
        snd_bank[i].use         = 0;
    }

    snd_bd_file = (SND_BANK_FILE *)&snd_bank[num];
    for (i = 0; i < num; i++)
    {
        snd_bd_file[i].m_pAdrs  = nullptr;
        snd_bd_file[i].m_FileNo = -1;
        snd_bd_file[i].m_Size   = 0;
        snd_bd_file[i].m_RefCnt = 0;
        snd_bd_file[i].m_Ready  = 0;
    }

    snd_hxd_file = &snd_bd_file[num];
    for (i = 0; i < num; i++)
    {
        snd_hxd_file[i].m_pAdrs  = nullptr;
        snd_hxd_file[i].m_FileNo = -1;
        snd_hxd_file[i].m_Size   = 0;
        snd_hxd_file[i].m_RefCnt = 0;
        snd_hxd_file[i].m_Ready  = 0;
    }

    return &snd_hxd_file[num];
}

// ──────────────────────────────────────────────────────────────────────────
// FF3 0x0026e1a0; from MioPan snd_bank.c SndBankGetFreeBankNum (unchanged).

int SndBankGetFreeBankNum(void)
{
    int cnt = 0;

    for (int i = 0; i < snd_bank_max; i++)
    {
        if (snd_bank[i].use == 0)
            cnt++;
    }

    return cnt;
}

// ──────────────────────────────────────────────────────────────────────────
// FF3 0x0026e1d8; from MioPan snd_bank.c GetVacantSndBank (unchanged).

static int GetVacantSndBank(void)
{
    for (int i = 0; i < snd_bank_max; i++)
    {
        if (snd_bank[i].use == 0)
            return i;
    }

    return -1;
}

// ──────────────────────────────────────────────────────────────────────────
// Request the BD body and the HXD header of a new bank. A failed header
// undoes the body.
// FF3 0x0026e218; from MioPan snd_bank.c SndBankNew (no asserts).

int SndBankNew(int file_no, int header_file_no, int size)
{
    int       bank_no;
    SND_BANK *p_Sb;

    (void)size;

    bank_no = GetVacantSndBank();
    if (bank_no == -1)
        return -1;

    p_Sb = &snd_bank[bank_no];

    if (SndBankFileReqBD(p_Sb, file_no) == 0)
        return -1;

    if (SndBankFileReqHXD(p_Sb, header_file_no) == 0)
    {
        ReleaseSndBDFile(p_Sb->pSndBDFile);
        return -1;
    }

    p_Sb->s3d = nullptr;
    p_Sb->use = 1;
    return bank_no;
}

// ──────────────────────────────────────────────────────────────────────────
// FF3 0x0026e2c0; from MioPan snd_bank.c SndBankIsReady (no range check).

int SndBankIsReady(int bank_no)
{
    if (bank_no == -1)
        return 1;

    return snd_bankIsReady(&snd_bank[bank_no]);
}

// ──────────────────────────────────────────────────────────────────────────
// FF3 0x0026e2f8; from MioPan snd_bank.c SndBankGetInfo (no assert).

int SndBankGetInfo(int bank_no, int *num, SOUND_INFO **info)
{
    SND_BANK *p_Sb;

    if (bank_no == -1)
        return SND_BANK_ILLEGAL_SOUND_NO;

    p_Sb = &snd_bank[bank_no];
    if (snd_bankIsReady(p_Sb) == 0)
        return SND_BANK_NOT_READY;

    *info = snd_bankGet_pSOUND_INFO(p_Sb);
    *num  = snd_bankGet_pHxdHeader(p_Sb)->num;
    return SND_BANK_OK;
}

// ──────────────────────────────────────────────────────────────────────────
// Load-cancel completions: the loader calls them once a cancelled request is
// withdrawn, the first moment its buffer may be given back.
// FF3 0x0026e380 / 0x0026e398; from MioPan snd_bank.c (no print).

static void intr_LoadCancelHXDFunc(void *buffer, void *arg)
{
    (void)arg;
    ee_iopFree(buffer);
}

// FF3 0x0026e398
static void intr_LoadCancelBDFunc(void *buffer, void *arg)
{
    (void)arg;
    ReleaseSPUMemory((int)(uintptr_t)buffer);
}

// ──────────────────────────────────────────────────────────────────────────
// Drop one reference to an HXD record; the last one cancels the load or
// frees the buffer.
// FF3 0x0026e3b0; from MioPan snd_bank.c ReleaseSndHXDFile (no assert/print).

static void ReleaseSndHXDFile(SND_BANK_FILE *file)
{
    if (file->m_RefCnt >= 2)
    {
        file->m_RefCnt--;
        return;
    }

    if (file->m_Ready == 0)
        FileLoadCancel2(file->m_FileNo, file->m_pAdrs, intr_LoadCancelHXDFunc, nullptr);
    else
        ee_iopFree(file->m_pAdrs);

    file->m_pAdrs  = nullptr;
    file->m_RefCnt = 0;
    file->m_FileNo = -1;
}

// ──────────────────────────────────────────────────────────────────────────
// The same for a BD record and its SPU RAM.
// FF3 0x0026e430; from MioPan snd_bank.c ReleaseSndBDFile (no assert).

static void ReleaseSndBDFile(SND_BANK_FILE *file)
{
    if (file->m_RefCnt >= 2)
    {
        file->m_RefCnt--;
        return;
    }

    if (file->m_Ready == 0)
        FileLoadCancelSPU2(file->m_FileNo, file->m_pAdrs, intr_LoadCancelBDFunc, nullptr);
    else
        ReleaseSPUMemory((int)(uintptr_t)file->m_pAdrs);

    file->m_pAdrs  = nullptr;
    file->m_RefCnt = 0;
    file->m_FileNo = -1;
}

// ──────────────────────────────────────────────────────────────────────────
// FF3 0x0026e4b0; from MioPan snd_bank.c SndBankRelease (no range check).

int SndBankRelease(int bank_no)
{
    SND_BANK *p_Sb;

    if (bank_no == -1)
        return SND_BANK_OK;

    p_Sb = &snd_bank[bank_no];

    if (p_Sb->pSndHXDFile != nullptr)
    {
        ReleaseSndHXDFile(p_Sb->pSndHXDFile);
        p_Sb->pSndHXDFile = nullptr;
    }

    if (p_Sb->pSndBDFile != nullptr)
    {
        ReleaseSndBDFile(p_Sb->pSndBDFile);
        p_Sb->pSndBDFile = nullptr;
    }

    if (p_Sb->s3d != nullptr)
    {
        Snd3DFreeWrk(p_Sb->s3d);
        p_Sb->s3d = nullptr;
    }

    p_Sb->use = 0;
    return SND_BANK_OK;
}

// ──────────────────────────────────────────────────────────────────────────
// Make the bank a positional source: the first call creates its 3D handle at
// pos, later calls move it (pos nullptr: keep it).
// FF3 0x0026e540; from MioPan snd_bank.c SndBankSet3D (no range check).

int SndBankSet3D(int bank_no, float *pos)
{
    SND_BANK  *p_Sb;
    SND_3D_SET s3s;

    if (bank_no == -1)
        return SND_BANK_OK;

    p_Sb = &snd_bank[bank_no];
    if (snd_bankIsReady(p_Sb) == 0)
        return SND_BANK_NOT_READY;

    if (p_Sb->s3d == nullptr)
    {
        memset(&s3s, 0, sizeof(SND_3D_SET));
        s3s.pos   = pos;
        s3s.vel   = nullptr;
        s3s.dir   = nullptr;
        p_Sb->s3d = Snd3DCreateWrk(&s3s);
    }
    else if (pos != nullptr)
    {
        Snd3DSetPosition(p_Sb->s3d, pos);
    }

    return SND_BANK_OK;
}

// ──────────────────────────────────────────────────────────────────────────
// FF3 0x0026e5e0; from MioPan snd_bank.c SndBankIsLoopSnd. FF3 reads sample
// no (FF2 always read sample 0).

int SndBankIsLoopSnd(int bank_no, int no)
{
    SND_BANK *p_Sb;

    if (bank_no == -1)
        return 0;

    p_Sb = &snd_bank[bank_no];
    if (snd_bankIsReady(p_Sb) == 0)
        return 0;

    if (no >= snd_bankGet_pHxdHeader(p_Sb)->num)
        return 0;

    return snd_bankGet_pSOUND_INFO(p_Sb)[no].attr.loop;
}

// ──────────────────────────────────────────────────────────────────────────
// Key sample no on a voice: effect samples on core 0, the rest on core 1;
// when that core is full, the other one without the reverb send.
// FF3 0x0026e678; from MioPan snd_bank.c SndBankPlay (no asserts).

int SndBankPlay(int bank_no, int no, int effect, int loop,
                int vol, int pitch, int fade_time, SND_3D_SET *s3s)
{
    SND_BANK   *p_Sb;
    SOUND_INFO *info;
    int         top_adrs;
    int         core;
    int         ret;
    SND_3D_WRK *s3d      = nullptr;
    int         s3d_free = 0;

    (void)loop;

    if (bank_no == -1)
        return CSND_BUF_PLAY_NO_ID;

    p_Sb = &snd_bank[bank_no];
    if (snd_bankIsReady(p_Sb) == 0)
        return CSND_BUF_PLAY_NO_ID;

    if (no >= snd_bankGet_pHxdHeader(p_Sb)->num)
        return CSND_BUF_PLAY_NO_ID;

    info = &snd_bankGet_pSOUND_INFO(p_Sb)[no];

    // A caller's set gets its own handle, which snd_buffer.c frees with the
    // voice; without one the bank's handle is borrowed.
    if (info->attr.s3d)
    {
        if (s3s != nullptr)
        {
            s3d_free = 1;
            s3d      = Snd3DCreateWrk(s3s);
        }
        else
        {
            s3d = p_Sb->s3d;
        }
    }

    core     = info->attr.effect == 0;
    top_adrs = (int)(uintptr_t)p_Sb->pSndBDFile->m_pAdrs + info->offset;

    ret = SndBufPlay(top_adrs, core, effect, vol, info->vol, pitch, info->pitch, info->pan,
                     fade_time, info->attr.loop, info->attr.type, s3d, s3d_free,
                     (short)info->adsr1, (short)info->adsr2,
                     top_adrs + info->loopstart, top_adrs + info->loopend);

    if (ret == CSND_BUF_PLAY_NO_ID)
    {
        ret = SndBufPlay(top_adrs, core ^ 1, 0, vol, info->vol, pitch, info->pitch, info->pan,
                         fade_time, info->attr.loop, info->attr.type, s3d, s3d_free,
                         (short)info->adsr1, (short)info->adsr2,
                         top_adrs + info->loopstart, top_adrs + info->loopend);
    }

    if (ret == CSND_BUF_PLAY_NO_ID && s3d_free && s3d != nullptr)
        Snd3DFreeWrk(s3d);

    return ret;
}

// ──────────────────────────────────────────────────────────────────────────
// FF3 0x0026e898; from MioPan snd_bank.c SndBankGetFileNo (no print/assert).

int SndBankGetFileNo(int bank_no, int *file_no)
{
    SND_BANK *p_Sb;

    if (bank_no == -1)
        return SND_BANK_OK;

    p_Sb = &snd_bank[bank_no];
    if (snd_bankIsReady(p_Sb) == 0)
        return SND_BANK_NOT_READY;

    *file_no = p_Sb->pSndBDFile->m_FileNo;
    return SND_BANK_OK;
}

// ──────────────────────────────────────────────────────────────────────────
// A live record of file_no in table, or nullptr.
// FF3 0x0026e900; from MioPan snd_bank.c SndBankFileSearch (unchanged).

static SND_BANK_FILE *SndBankFileSearch(SND_BANK_FILE *table, int file_no)
{
    for (int i = 0; i < snd_bank_max; i++)
    {
        if (table[i].m_RefCnt != 0 && table[i].m_FileNo == file_no)
            return &table[i];
    }

    return nullptr;
}

// ──────────────────────────────────────────────────────────────────────────
// A free record of table, or nullptr.
// FF3 0x0026e940; from MioPan snd_bank.c SndBankFileGet (no assert).

static SND_BANK_FILE *SndBankFileGet(SND_BANK_FILE *table)
{
    for (int i = 0; i < snd_bank_max; i++)
    {
        if (table[i].m_RefCnt == 0)
            return &table[i];
    }

    return nullptr;
}

// ──────────────────────────────────────────────────────────────────────────
// Share or start the BD body load into SPU RAM; 0 when SPU RAM is full.
// FF3 0x0026e978; from MioPan snd_bank.c SndBankFileReqBD (no print).

static int SndBankFileReqBD(SND_BANK *p_Sb, int file_no)
{
    SND_BANK_FILE *file;
    int            size;
    int            adrs;

    file             = SndBankFileSearch(snd_bd_file, file_no);
    p_Sb->pSndBDFile = file;
    if (file != nullptr)
    {
        file->m_RefCnt++;
        return 1;
    }

    file             = SndBankFileGet(snd_bd_file);
    p_Sb->pSndBDFile = file;

    size          = GetAlignUp(GetFileSize(file_no), 6);
    file->m_Size  = size;
    adrs          = GetSPUMemory(size);
    file->m_pAdrs = (void *)(uintptr_t)adrs;
    if (adrs == 0)
        return 0;

    file->m_FileNo = file_no;
    file->m_Ready  = 0;
    FileLoadReqSPU(file_no, file->m_pAdrs, snd_bank_load_priority, intrSndBankFileBD, file);
    file->m_RefCnt = 1;
    return 1;
}

// ──────────────────────────────────────────────────────────────────────────
// Share or start the HXD header load into EE memory; 0 when out of memory.
// FF3 0x0026ea40; from MioPan snd_bank.c SndBankFileReqHXD (no print).

static int SndBankFileReqHXD(SND_BANK *p_Sb, int file_no)
{
    SND_BANK_FILE *file;
    int            size;

    file              = SndBankFileSearch(snd_hxd_file, file_no);
    p_Sb->pSndHXDFile = file;
    if (file != nullptr)
    {
        file->m_RefCnt++;
        return 1;
    }

    file              = SndBankFileGet(snd_hxd_file);
    p_Sb->pSndHXDFile = file;

    size          = GetAlignUp(GetFileSize(file_no), 6);
    file->m_Size  = size;
    file->m_pAdrs = ee_iopMalloc(size);
    if (file->m_pAdrs == nullptr)
        return 0;

    file->m_FileNo = file_no;
    file->m_Ready  = 0;
    FileLoadReqEE(file_no, file->m_pAdrs, snd_bank_load_priority, intrSndBankFileHXD, file);
    file->m_RefCnt = 1;
    return 1;
}

// ──────────────────────────────────────────────────────────────────────────
// Load completions (loader thread).
// FF3 0x0026eb08 / 0x0026eb18; from MioPan snd_bank.c (FF3 no longer checks
// the HXD header).

static void intrSndBankFileBD(void *buffer, void *arg)
{
    (void)buffer;
    ((SND_BANK_FILE *)arg)->m_Ready = 1;
}

// FF3 0x0026eb18
static void intrSndBankFileHXD(void *buffer, void *arg)
{
    (void)buffer;
    ((SND_BANK_FILE *)arg)->m_Ready = 1;
}

// ──────────────────────────────────────────────────────────────────────────
// FF3 0x0026eb28 / 0x0026eb38 / 0x0026eb48; from MioPan snd_bank.c.

static HXD_HEADER *snd_bankGet_pHxdHeader(SND_BANK *p_Sb)
{
    return (HXD_HEADER *)p_Sb->pSndHXDFile->m_pAdrs;
}

// The SOUND_INFO records follow the header.
// FF3 0x0026eb38
static SOUND_INFO *snd_bankGet_pSOUND_INFO(SND_BANK *p_Sb)
{
    return (SOUND_INFO *)((char *)p_Sb->pSndHXDFile->m_pAdrs + sizeof(HXD_HEADER));
}

// FF3 0x0026eb48
static int snd_bankIsReady(SND_BANK *p_Sb)
{
    if (p_Sb->pSndHXDFile == nullptr || p_Sb->pSndHXDFile->m_Ready == 0)
        return 0;

    return p_Sb->pSndBDFile != nullptr && p_Sb->pSndBDFile->m_Ready != 0;
}
