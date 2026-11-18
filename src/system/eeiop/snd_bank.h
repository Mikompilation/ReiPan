/* ==========================================================================
 *  system/eeiop/snd_bank.h
 *
 *  SE banks (FF3 snd_bank.c, 0x26e0b0..0x26eb88; FF2's snd_bank.c). A bank
 *  is a pair of files -- the BD body (ADPCM, loaded into SPU RAM) and its HXD
 *  header (an HXD_HEADER and SOUND_INFO records, loaded into EE RAM) -- plus
 *  an optional 3D handle. SndBankNew takes both files and hands back a bank
 *  number; SndBankPlay keys one SOUND_INFO record of it on a voice.
 *
 *  The file records are shared and reference counted: two banks naming the
 *  same file get one record and one load.
 * ======================================================================== */
#ifndef _SYSTEM_EEIOP_SND_BANK_H
#define _SYSTEM_EEIOP_SND_BANK_H

#include <stddef.h>                     // offsetof

#include "system/eeiop/snd.h"           // SOUND_INFO
#include "system/eeiop/snd_3d.h"        // SND_3D_SET, SND_3D_WRK

// SndBankRelease / SndBankGetInfo / SndBankSet3D / SndBankGetFileNo results
// (FF2's SND_BANK_STATUS).
enum
{
    SND_BANK_NOT_USE          = 0,
    SND_BANK_NOT_READY        = 1,
    SND_BANK_USE              = 2,
    SND_BANK_ILLEGAL_SOUND_NO = 3,
    SND_BANK_OK               = 4
};

// One loaded file, shared by every bank that names it. m_Ready is raised by
// the loader's completion callback.
// layout: host -- m_pAdrs is a pointer (EE 4 bytes): an SPU RAM address for
// a BD body, an EE buffer for an HXD header. 0x10 on the EE.
typedef struct SND_BANK_FILE
{
    /* 0x0 */ short m_Ready;
    /* 0x2 */ short m_RefCnt;           // 0: record free
    /* 0x4 */ int   m_FileNo;           // -1 when free
    /* 0x8 */ void *m_pAdrs;
    /*     */ int   m_Size;             // EE 0xc: rounded up to 64
} SND_BANK_FILE;

static_assert(offsetof(SND_BANK_FILE, m_RefCnt) == 0x2, "SND_BANK_FILE.m_RefCnt");
static_assert(offsetof(SND_BANK_FILE, m_FileNo) == 0x4, "SND_BANK_FILE.m_FileNo");
static_assert(offsetof(SND_BANK_FILE, m_pAdrs) == 0x8, "SND_BANK_FILE.m_pAdrs");

// One bank (FF3 dropped FF2's id field: 0x10 on the EE, gp+0x57c).
// layout: host -- three pointers from 0x4 on (EE 4 bytes each).
typedef struct SND_BANK
{
    /* 0x0:0 */ unsigned int   use : 1;
    /* 0x4 */   SND_3D_WRK    *s3d;     // SndBankSet3D's handle, borrowed by SndBankPlay
    /* 0x8 */   SND_BANK_FILE *pSndBDFile;
    /* 0xc */   SND_BANK_FILE *pSndHXDFile;
} SND_BANK;

int  SndBankGetOneWrkSize(void);        // FF3 0x0026e0b0: one bank's share of the work buffer
void *SndBankInitAll(void *buffer, int num, int load_priority);  // FF3 0x0026e0b8: returns the rest
int  SndBankGetFreeBankNum(void);       // FF3 0x0026e1a0
int  SndBankNew(int file_no, int header_file_no, int size);     // FF3 0x0026e218: bank or -1 (size unused)
int  SndBankIsReady(int bank_no);       // FF3 0x0026e2c0: 1 for bank -1
int  SndBankGetInfo(int bank_no, int *num, SOUND_INFO **info);  // FF3 0x0026e2f8: SND_BANK_*
int  SndBankRelease(int bank_no);       // FF3 0x0026e4b0: SND_BANK_OK
int  SndBankSet3D(int bank_no, float *pos);                     // FF3 0x0026e540: SND_BANK_*
int  SndBankIsLoopSnd(int bank_no, int no);                     // FF3 0x0026e5e0

// Keys sample no of the bank on a voice; returns the SndBufPlay id or
// CSND_BUF_PLAY_NO_ID. loop is unused (the sample's attr decides); s3s gives
// a positional sample its own 3D handle, else the bank's is borrowed.
int  SndBankPlay(int bank_no, int no, int effect, int loop,
                 int vol, int pitch, int fade_time, SND_3D_SET *s3s);   // FF3 0x0026e678
int  SndBankGetFileNo(int bank_no, int *file_no);               // FF3 0x0026e898: SND_BANK_*

#endif /* _SYSTEM_EEIOP_SND_BANK_H */
