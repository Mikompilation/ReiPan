/* ==========================================================================
 *  system/eeiop/ee_iop.h
 *
 *  EE<->IOP layer (FF3 ee_iop.c, 0x26ad30..0x26b3f0; FF2's ee_iop.c):
 *  ee_iopInit boots the IOP and its modules, starts the file loader and the
 *  sound system and loads the file table; iopCommandRegister queues commands
 *  that ee_iopMain sends once a frame, and the accessors read the status
 *  block the IOP sends back.
 * ======================================================================== */
#ifndef _SYSTEM_EEIOP_EE_IOP_H
#define _SYSTEM_EEIOP_EE_IOP_H

#include <stddef.h>

// FF2's EEIOP_DEF (0x3c bytes) with five FF3 additions at the end that carry
// what FF2 passed to cddatInit() separately: the data-image and file-table
// names. 0x50 bytes; layout from InitSystemON's stack image (FF3 0x0025a200).
// layout: host -- its pointers make it larger than 0x50 on the PC; it only
// lives on InitSystemON's stack and nothing depends on its size.
typedef struct
{
    /* 0x00 */ void *(*malloc64)(int size);     // EE allocator for the IOP layer
    /* 0x04 */ void  (*free64)(void *adrs);
    /* 0x08 */ int    command_send_buffer_size;
    /* 0x0c */ const char *iop_def_module;      // IOPRP image
    /* 0x10 */ const char *hil_file_name;       // IRX host image list
    /* 0x14 */ const char *dil_file_name;       // IRX disc image list
    /* 0x18 */ int    media;
    /* 0x1c */ int    file_load_wrk_num;
    /* 0x20 */ int    snd_bank_wrk_num;
    /* 0x24 */ int    stream_auto_wrk_num;
    /* 0x28 */ int    auto_bd_wrk_num;
    /* 0x2c */ int    snd_stream_load_priority;
    /* 0x30 */ int    snd_bank_load_priority;
    /* 0x34 */ int    cmp_use_flg;
    /* 0x38 */ int    rom_boot;
    /* 0x3c */ int    spu_mem_start;            // FF3: 0x5010, SPU RAM handed to SPUMemoryInit
    /* 0x40 */ int    spu_mem_size;             // FF3: 0x1faff0 (start + size = the 2 MB end)
    /* 0x44 */ const char *pc_path;             // "host0:" (FF2 cddatInit's pc_path)
    /* 0x48 */ const char *fhd_file_name;       // "cdrom0:\ZERO3.FHD;1", the file header table
    /* 0x4c */ const char *cd_dat_name;         // "IMG_BD.BIN" (FF2 cddatInit's cd_dat_name)
} EEIOP_DEF;

// IOP command-queue request ids (the command iopCommandRegister posts); FF3
// uses FF2's numbers (SndInit 5 and 6, ee_iopInit 3, SndStreamInit 9,
// SndPCMStreamInit 26).
typedef enum
{
    IOP_COM_END             = 0,
    REQ_IOP_REBOOT          = 1,
    REQ_CDVD_INIT           = 2,
    REQ_SET_CD_DAT          = 3,
    REQ_CD_READ_MODE_CHANGE = 4,
    REQ_IOP_SND_INIT        = 5,
    REQ_SET_SND_EFFECT      = 6,
    REQ_VOICE_STOP          = 7,
    REQ_VOICE_LOOP_SET      = 8,
    REQ_STREAM_CREATE       = 9,
    REQ_STREAM_RELEASE      = 10,
    REQ_STREAM_START        = 11,
    REQ_STREAM_PLAY         = 12,
    REQ_STREAM_STOP         = 13,
    REQ_STREAM_ABORT        = 14,
    REQ_STREAM_PAUSE        = 15,
    REQ_STREAM_RESTART      = 16,
    REQ_STREAM_SETVOL       = 17,
    REQ_STREAM_SETPITCH     = 18,
    REQ_SB_INIT             = 19,
    REQ_SB_PLAY             = 20,
    REQ_SB_STOP             = 21,
    REQ_SB_PAUSE            = 22,
    REQ_SB_RESTART          = 23,
    REQ_SB_SETVOL           = 24,
    REQ_SB_SETPITCH         = 25,
    REQ_PCM_STREAMCREATE    = 26,
    REQ_PCM_STREAMINIT      = 27,
    REQ_PCM_STREAMSTART     = 28,
    REQ_PCM_STREAMPLAY      = 29,
    REQ_PCM_STREAMSTOP      = 30,
    REQ_PCM_STREAMPAUSE     = 31,
    REQ_PCM_STREAMRESTART   = 32,
    REQ_PCM_STREAMSETVOL    = 33,
} IOP_COMMAND_ENUM;

// REQ_SET_CD_DAT's payload: the data image's name (ee_iopInit's stack
// buffer, strcpy'd from def->cd_dat_name, sent as 0x10 bytes).
typedef struct
{
    /* 0x00 */ char file_name[16];
} SET_CD_DAT;
static_assert(sizeof(SET_CD_DAT) == 0x10, "SET_CD_DAT size");

// REQ_CD_READ_MODE_CHANGE's payload (SetCDReadMode, 4 bytes).
typedef struct
{
    /* 0x00 */ int mode;
} CD_READ_MODE_CHANGE;
static_assert(sizeof(CD_READ_MODE_CHANGE) == 0x4, "CD_READ_MODE_CHANGE size");

// Per-stream state the IOP sends back every frame (GetStreamWrkRet /
// GetPCMStreamWrkRet: stride 8).
typedef struct
{
    /* 0x00 */ int status;                  // an EEIOP_STREAM_STATUS
    /* 0x04 */ int offset;
} IOP_STREAM_RET;
static_assert(sizeof(IOP_STREAM_RET) == 0x8, "IOP_STREAM_RET size");
static_assert(offsetof(IOP_STREAM_RET, status) == 0x0, "IOP_STREAM_RET.status");
static_assert(offsetof(IOP_STREAM_RET, offset) == 0x4, "IOP_STREAM_RET.offset");

// The status block the IOP returns from ee_iopMain's RPC; ee_iopMain copies
// all 0x1a8 bytes into iop_ret (0x40ee80). voice_end is a bit per voice, set
// once the voice has passed its end point (CheckEndPointThrough).
typedef struct
{
    /* 0x000 */ int            voice_end[2];
    /* 0x008 */ IOP_STREAM_RET stream_ret[2];
    /* 0x018 */ IOP_STREAM_RET pcm_stream_ret[2];
    /* 0x028 */ int            mpLoopAdrs[2][24];  // FF2, unverified (FF3 has no reader)
    /* 0x0e8 */ int            mpNowAdrs[2][24];   // FF2, unverified (FF3 has no reader)
} IOP_RET_STATUS;
static_assert(sizeof(IOP_RET_STATUS) == 0x1a8, "IOP_RET_STATUS size");
static_assert(offsetof(IOP_RET_STATUS, voice_end) == 0x000, "IOP_RET_STATUS.voice_end");
static_assert(offsetof(IOP_RET_STATUS, stream_ret) == 0x008, "IOP_RET_STATUS.stream_ret");
static_assert(offsetof(IOP_RET_STATUS, pcm_stream_ret) == 0x018, "IOP_RET_STATUS.pcm_stream_ret");
static_assert(offsetof(IOP_RET_STATUS, mpLoopAdrs) == 0x028, "IOP_RET_STATUS.mpLoopAdrs");
static_assert(offsetof(IOP_RET_STATUS, mpNowAdrs) == 0x0e8, "IOP_RET_STATUS.mpNowAdrs");

void *ee_iopMalloc(int size);           // FF3 0x0026aea8: def->malloc64
void  ee_iopFree(void *adrs);           // FF3 0x0026aec8: def->free64
int   ee_iopGetNeedSize(EEIOP_DEF *def); // FF3 0x0026aee8: the work buffer ee_iopInit allocates
int   ee_iopInit(EEIOP_DEF *def);       // FF3 0x0026af28: 1 on success, 0 if a file or module failed
void  WaitMainRpc(void);                // FF3 0x0026b1a8: until the last frame's RPC has landed
void  ee_iopMain(void);                 // FF3 0x0026b1d8: per frame, from pre_super
void  iopCommandFrameInit(void);        // FF3 0x0026b2f0: empty the command queue
int   iopCommandRegister(IOP_COMMAND_ENUM command, void *buf, int size); // FF3 0x0026b2f8: queue a command; 0 if full
void  SetCDReadMode(int mode);          // FF3 0x0026b370
int   CheckEndPointThrough(int core, int voice_no);   // FF3 0x0026b398
IOP_STREAM_RET *GetStreamWrkRet(int wrk_id);         // FF3 0x0026b3c0
IOP_STREAM_RET *GetPCMStreamWrkRet(int wrk_id);      // FF3 0x0026b3d8

#endif /* _SYSTEM_EEIOP_EE_IOP_H */
