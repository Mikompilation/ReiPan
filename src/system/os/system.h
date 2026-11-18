/* ==========================================================================
 *  system/os/system.h
 *
 *  FF3's system.c: program entry (main), the one-time machine bring-up
 *  (InitMainSystem), the system work block (sys_wrk: frame count, soft reset,
 *  video mode, language), the double-buffered VIF1 packet buffer, the GS
 *  display/draw double buffer and the per-frame back end (SendDMAMain: wait for
 *  V-blank, swap, kick the frame's packets).
 *
 *  FF2 kept main() in main/main.c; FF3 moved it here, between GetLanguage and
 *  InitMainSystem (FF3 0x0025a3e8). FF3 extent: 0x2597f0..0x25a740 plus the
 *  getters GetPacketBufMgr (0x2ab428) and GetSysWrk (0x2abdf8).
 *
 *  Target pointers are 4 bytes and host pointers 8: PACKET_BUF_MGR keeps EE
 *  addresses as u_int (as the hardware DMA sees them) so it keeps its PS2 size.
 * ======================================================================== */
#ifndef _SYSTEM_OS_SYSTEM_H
#define _SYSTEM_OS_SYSTEM_H

#include "sdk/scetypes.h"
#include "sdk/sce_gs.h"

// ──────────────────────────────────────────────────────────────────────
// sys_wrk

// SYS_WRK.video_mode values (the sceGsResetGraph omode codes).
#define VIDEO_MODE_NTSC     2       // SCE_GS_NTSC
#define VIDEO_MODE_PAL      3       // SCE_GS_PAL
#define VIDEO_MODE_DTV480P  0x50    // SCE_GS_DTV480P

// The system work block, a function-local static of GetSysWrk (bss 0x41e4b0).
// FF3 made it a C++ object: InitSysWrk is its constructor and the soft-reset /
// language helpers are its members.
struct SYS_WRK                      /* 0x14 */
{
    SYS_WRK();                      // FF3 0x0025a2f0 (InitSysWrk)
    void CheckSoftReset();          // FF3 0x0025a310
    int  IsSoftResetCountOver();    // FF3 0x0025a378
    int  GetLanguage();             // FF3 0x0025a388: 0..4: English, French, German, Spanish, Italian

    /* 0x00 */ u_int  count;        // frames since boot; parity picks the display buffer
    /* 0x04 */ short  sreset_count; // frames the soft-reset combination has been held
    /* 0x08 */ int    sreset_ng;    // non-zero: soft reset disabled
    /* 0x0c */ u_char video_mode;   // VIDEO_MODE_*
    /* 0x10 */ int    language;     // console language id, GetSystemLanguage's encoding
};
static_assert(sizeof(SYS_WRK) == 0x14, "SYS_WRK is 0x14 bytes on the PS2");

SYS_WRK *GetSysWrk(void);
int      GetSystemLanguage(void);       // the console's language as a SYS_WRK.language id

// ──────────────────────────────────────────────────────────────────────
// VIF1 packet buffer: two halves, one filled while the other is sent

// A function-local static with a destructor FF3 registers with atexit: a
// C++ object, so the packet calls are members.
struct PACKET_BUF_MGR               /* 0x18, GetPacketBufMgr's static (0x41e478) */
{
    PACKET_BUF_MGR() : top{}, cur{}, side(0), size(0) {}

    void  CheckVif1Stall();                                         // FF3 0x002597f0
    void  Kick();                                                   // FF3 0x00259820 (PacketBufKick)
    u_int GetPacket();                                              // FF3 0x00259930 (PacketBufGetPacket)
    void  SetPacket(u_int end, int direct, int flush);              // FF3 0x00259948 (PacketBufSetPacket)
    void  AddRefTag(u_int addr, int qwc, int direct, int flush);    // FF3 0x002599b8 (PacketBufAddRefTag)

    /* 0x00 */ u_int top[2];        // EE address of each half
    /* 0x08 */ u_int cur[2];        // write pointer of each half (next DMA tag)
    /* 0x10 */ char  side;          // half being filled
    /* 0x14 */ u_int size;          // bytes per half
};
static_assert(sizeof(PACKET_BUF_MGR) == 0x18, "PACKET_BUF_MGR is 0x18 bytes on the PS2");

PACKET_BUF_MGR *GetPacketBufMgr(void);

// ──────────────────────────────────────────────────────────────────────
// Display / draw double buffer and frame pacing

extern sceGsDBuffDc   gdb;              // data 0x333150
extern sceGsDrawEnv1 *pdrawenv1;        // sdata 0x372608: this frame's context-1 draw env
extern sceGsDrawEnv1 *pdrawenv1_other;  // sdata 0x37260c: the other buffer's
extern sceGsDrawEnv2 *pdrawenv2;        // sdata 0x372610: this frame's context-2 draw env
extern sceGsDrawEnv2 *pdrawenv2_other;  // sdata 0x372614: the other buffer's

void  InitGraphics(void);
void  SetFrameBuffer(void);
void  SetDisplayPos(int video_mode);
void  SetGsResetGraph(int video_mode, int n);
void  SetPCRTC(u_int frame, int video_mode);
void  SwapDBuff(void);
int   GetPALMode(void);
float GetFrameTimeScale(void);          // 1.2 in PAL (60/50), else 1.0
void  ChangeVideoMode(u_char mode);
void  InitVBlank(void);
void  SystemSetVCallback(void);
void  WaitVBlank(void);
void  SetVBlankWaitNum(int num);
void  SendDMAMain(void);

// ──────────────────────────────────────────────────────────────────────
// Bring-up

void  InitSystemON(void);
void  InitMainSystem(void);
void *systemGetMem(int size);
void  systemFreeMem(void *adrs);

#endif /* _SYSTEM_OS_SYSTEM_H */
