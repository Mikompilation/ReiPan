/* ==========================================================================
 *  system/pad/pad.h
 *
 *  The pad driver (system/pad/pad.c, FF3 0x1f0138..0x1f0d78; FF2 pad.c's
 *  role): two controller works opened by PadOpenAll and read every frame by
 *  PadReadAll. The game-side layer over it (button checks, locks, the button
 *  configuration) is system/pad/pad_ctrl.h.
 * ======================================================================== */
#ifndef _SYSTEM_PAD_PAD_H
#define _SYSTEM_PAD_PAD_H

#include <stddef.h>                     // offsetof
#include "sdk/scetypes.h"

// PAD_WRK.step: the connection state machine of PadSyncOne.
enum PAD_STEP
{
    PAD_STEP_CHECK_MODE  = 0,           // connected: query the controller's modes
    PAD_STEP_SET_MAIN    = 1,           // actuators aligned: switch to analog, locked
    PAD_STEP_WAIT_MAIN   = 2,           // wait for the mode request
    PAD_STEP_CHECK_ID    = 3,           // DualShock 2 (id 7)?
    PAD_STEP_PRESS_MODE  = 10,          // try the pressure mode
    PAD_STEP_WAIT_PRESS  = 11,          // wait for the press-mode request
    PAD_STEP_READY       = 99,          // read every frame
};

// One controller's work (FF3 0x200 bytes; pad_wrk[2] at bss 0x3295c0). FF2's
// PAD_STRUCT grown: pressure, analog directions and vibration timers moved in.
// The arrays [2][2] are [0] this frame / [1] last frame, then [stick] with
// stick 0 = left (analog[2..3]) and 1 = right (analog[0..1]).
struct PAD_WRK                      /* 0x200 */
{
    /* 0x000 */ int     port;
    /* 0x004 */ int     slot;
    /* 0x008 */ u_char  _008[0x38];
    /* 0x040 */ u_char  dma_buf[0x100];     // scePadPortOpen buffer
    /* 0x140 */ u_short held;               // buttons down (FF2 now)
    /* 0x142 */ u_short old;                // last frame's held
    /* 0x144 */ u_short trg;                // pressed this frame (FF2 one)
    /* 0x146 */ u_short rpt;                // trg, plus held every 5 frames after 10
    /* 0x148 */ u_short rpt_time;
    /* 0x14a */ u_short cnt[16];            // frames held, per sce_pad bit (PadGetCount)
    /* 0x16a */ u_short cnt_bak[16];        // last frame's cnt
    /* 0x18a */ u_char  act_direct[6];      // scePadSetActDirect data; [0] bit 7 = send request
    /* 0x190 */ int     vib_small_time;     // small motor frames left (-1 = until stopped)
    /* 0x194 */ int     vib_big_time;       // big motor frames left (-1 = until stopped)
    /* 0x198 */ int     vib_big_pow;        // big motor power
    /* 0x19c */ u_char  act_align[6];       // scePadSetActAlign data
    /* 0x1a2 */ u_char  _1a2[2];
    /* 0x1a4 */ int     connected;          // scePadGetState != disconnected; 0 makes IngameMain pause
    /* 0x1a8 */ int     stick_dpad;         // left stick also sets d-pad bits (options screen)
    /* 0x1ac */ char    step;               // PAD_STEP
    /* 0x1ad */ char    id;                 // controller id byte (rdata[1]); 0 = any
    /* 0x1ae */ u_char  press[0x10];        // pressure, 12 buttons (PadGetPressure); 0x1ba.. unused
    /* 0x1be */ u_char  analog[4];          // raw stick bytes: right x/y, left x/y (PadGetAnalogRaw)
    /* 0x1c2 */ u_short an_cnt[2][2];       // frames in the same direction (PadGetStickWord)
    /* 0x1ca */ u_char  _1ca[2];
    /* 0x1cc */ float   an_rot[2][2];       // stick angle, g3dAtan2f (PadGetStickFloat)
    /* 0x1dc */ u_char  an_dir[2][2];       // 8-way direction, 0xff = centred (PadGetStickByte)
    /* 0x1e0 */ u_char  _1e0[0x20];
};
static_assert(sizeof(PAD_WRK) == 0x200, "PAD_WRK is 0x200 bytes on the PS2");
static_assert(offsetof(PAD_WRK, dma_buf) == 0x040, "PAD_WRK.dma_buf");
static_assert(offsetof(PAD_WRK, held) == 0x140, "PAD_WRK.held");
static_assert(offsetof(PAD_WRK, old) == 0x142, "PAD_WRK.old");
static_assert(offsetof(PAD_WRK, trg) == 0x144, "PAD_WRK.trg");
static_assert(offsetof(PAD_WRK, rpt) == 0x146, "PAD_WRK.rpt");
static_assert(offsetof(PAD_WRK, rpt_time) == 0x148, "PAD_WRK.rpt_time");
static_assert(offsetof(PAD_WRK, cnt) == 0x14a, "PAD_WRK.cnt");
static_assert(offsetof(PAD_WRK, cnt_bak) == 0x16a, "PAD_WRK.cnt_bak");
static_assert(offsetof(PAD_WRK, act_direct) == 0x18a, "PAD_WRK.act_direct");
static_assert(offsetof(PAD_WRK, vib_small_time) == 0x190, "PAD_WRK.vib_small_time");
static_assert(offsetof(PAD_WRK, vib_big_time) == 0x194, "PAD_WRK.vib_big_time");
static_assert(offsetof(PAD_WRK, vib_big_pow) == 0x198, "PAD_WRK.vib_big_pow");
static_assert(offsetof(PAD_WRK, act_align) == 0x19c, "PAD_WRK.act_align");
static_assert(offsetof(PAD_WRK, connected) == 0x1a4, "PAD_WRK.connected");
static_assert(offsetof(PAD_WRK, stick_dpad) == 0x1a8, "PAD_WRK.stick_dpad");
static_assert(offsetof(PAD_WRK, step) == 0x1ac, "PAD_WRK.step");
static_assert(offsetof(PAD_WRK, id) == 0x1ad, "PAD_WRK.id");
static_assert(offsetof(PAD_WRK, press) == 0x1ae, "PAD_WRK.press");
static_assert(offsetof(PAD_WRK, analog) == 0x1be, "PAD_WRK.analog");
static_assert(offsetof(PAD_WRK, an_cnt) == 0x1c2, "PAD_WRK.an_cnt");
static_assert(offsetof(PAD_WRK, an_rot) == 0x1cc, "PAD_WRK.an_rot");
static_assert(offsetof(PAD_WRK, an_dir) == 0x1dc, "PAD_WRK.an_dir");

extern PAD_WRK pad_wrk[2];                  // bss 0x3295c0
extern u_short sce_pad[16];                 // data 0x35b640: button bit per counter index

void    PadOpenAll(void);                   // scePadInit, clear and open both pads
void    PadReadAll(void);                   // per frame, interrupts off
int     PadIsPressMode(int pad);            // scePadInfoPressMode(...) == 1
int     PadIsStable(int pad);               // libpad state 6 (stable) or 2 (FindCTP1)
u_short PadGetSceButton(int i);             // sce_pad[i]

#include "system/pad/pad_ctrl.h"            // the game-side layer most users want too

#endif /* _SYSTEM_PAD_PAD_H */
