/* ==========================================================================
 *  system/pad/pad_ctrl.c
 *
 *  The game-side pad layer (see pad_ctrl.h). FF3 0x2254b8..0x225a58: FF3's
 *  own file between outgame/option.c and outgame/pad_check.c; FF2 had no
 *  such layer (its menus read paddat directly).
 *
 *  Two functions of this file are not functions in the Ghidra database yet:
 *  0x002256a8 (PadGetCount, after CheckPadNowPlaySe) and 0x00225840
 *  (PadGetStickFloat, swallowed by PadGetStickWord's body); they are written
 *  from their instructions.
 * ======================================================================== */
#include "system/pad/pad_ctrl.h"

#include "ingame/sound/snd_ctrl.h"      // SndSePlay
#include "outgame/option.h"             // GetOptionWrk (button layout setting)
#include "system/pad/pad.h"
#include "ingame/menu/menu_mouse.h"    // PC: the mouse in menus

// ──────────────────────────────────────────────────────────────────────
// Data
// ──────────────────────────────────────────────────────────────────────

u_int pad_key[PAD_KEY_NUM];             // data 0x32dd08 (zero in the ROM)

// data 0x32dd70: the three button layouts.
u_int pad_key_cfg[PAD_KEY_CFG_NUM][PAD_KEY_NUM] =
{
    {
        0x0040, 0x0010, 0x0080, 0x0010, 0x0010, 0x0081, 0x0040, 0x0008, 0x0040, 0x0800,
        0x0080, 0x0001, 0x0001, 0x0004, 0x0001, 0x0002, 0x0004, 0x0008, 0x0020, 0x0004,
        0x0004, 0x0002, 0x0002, 0x0200, 0x0400, 0x0100,
    },
    {
        0x0040, 0x0010, 0x0080, 0x0010, 0x0010, 0x0081, 0x0040, 0x0008, 0x0040, 0x0800,
        0x0080, 0x0001, 0x0001, 0x0020, 0x0001, 0x0002, 0x0004, 0x0008, 0x0004, 0x0020,
        0x0020, 0x0002, 0x0002, 0x0200, 0x0400, 0x0100,
    },
    {
        0x0040, 0x0010, 0x0080, 0x0008, 0x0008, 0x0081, 0x0040, 0x0040, 0x0040, 0x0800,
        0x0080, 0x0001, 0x0010, 0x0004, 0x0001, 0x0002, 0x0004, 0x0008, 0x0020, 0x0010,
        0x0004, 0x0002, 0x0002, 0x0200, 0x0400, 0x0100,
    },
};

static int pad_lock[2];                 // sdata 0x373770 (gp+0x280)

// ──────────────────────────────────────────────────────────────────────
// Init and frame
// ──────────────────────────────────────────────────────────────────────

// FF3 0x002254b8.
void PadInitAll(void)
{
    PadOpenAll();
    PadLockReset(-1);
}

// FF3 0x002254d8.
void PadFrameUpdate(void)
{
    PadKeyUpdate();
    PadReadAll();
}

// ──────────────────────────────────────────────────────────────────────
// Button checks
// ──────────────────────────────────────────────────────────────────────

// FF3 0x002254f8.
int CheckPadTrgPlaySe(u_short mask, int se, int pad)
{
    MenuMouseAskedPad(mask);            // PC: whoever asks for OK or cancel is a menu
    if (IsPadLocked(pad) || (pad_wrk[pad].trg & mask) == 0)
        return 0;
    if (se != -1)
        SndSePlay(se, 100, 0x1000, 0, 0, 0);
    return 1;
}

// FF3 0x00225588.
int CheckPadRptPlaySe(u_short mask, int se, int pad)
{
    MenuMouseAskedPad(mask);            // PC
    if (IsPadLocked(pad) || (pad_wrk[pad].rpt & mask) == 0)
        return 0;
    if (se != -1)
        SndSePlay(se, 100, 0x1000, 0, 0, 0);
    return 1;
}

// FF3 0x00225618.
int CheckPadNowPlaySe(u_short mask, int se, int pad)
{
    if (IsPadLocked(pad) || (pad_wrk[pad].held & mask) == 0)
        return 0;
    if (se != -1)
        SndSePlay(se, 100, 0x1000, 0, 0, 0);
    return 1;
}

// ──────────────────────────────────────────────────────────────────────
// Getters (all test pad 0's lock, whatever 'pad' is)
// ──────────────────────────────────────────────────────────────────────

// FF3 0x002256a8 (no Ghidra function yet).
u_short PadGetCount(int button, int pad)
{
    if (IsPadLocked(0))
        return 0;
    return pad_wrk[pad].cnt[button];
}

// FF3 0x00225708.
u_char PadGetPressure(int button, int pad)
{
    if (IsPadLocked(0))
        return 0;
    return pad_wrk[pad].press[button];
}

// FF3 0x00225760.
u_char PadGetAnalogRaw(int axis, int pad)
{
    if (IsPadLocked(0))
        return 0x7f;
    return pad_wrk[pad].analog[axis];
}

// FF3 0x002257b8.
u_short PadGetStickWord(int axis, int stick, int pad)
{
    if (IsPadLocked(0))
        return 0;
    return pad_wrk[pad].an_cnt[stick != 0][axis];
}

// FF3 0x00225840 (no Ghidra function yet).
float PadGetStickFloat(int axis, int stick, int pad)
{
    if (IsPadLocked(0))
        return 0.0f;
    return pad_wrk[pad].an_rot[stick != 0][axis];
}

// FF3 0x002258c8.
u_char PadGetStickByte(int axis, int stick, int pad)
{
    if (IsPadLocked(0))
        return 0x7f;
    return pad_wrk[pad].an_dir[stick != 0][axis];
}

// ──────────────────────────────────────────────────────────────────────
// Locks
// ──────────────────────────────────────────────────────────────────────

// FF3 0x00225948.
int IsPadLocked(int pad)
{
    return pad_lock[pad];
}

// FF3 0x00225960.
void PadLockSet(int pad)
{
    if (pad >= 0)
    {
        pad_lock[pad] = 1;
        return;
    }
    pad_lock[1] = 1;
    pad_lock[0] = 1;
}

// FF3 0x00225990.
void PadLockReset(int pad)
{
    if (pad < 0)
    {
        pad_lock[0] = 0;
        pad_lock[1] = 0;
        return;
    }
    pad_lock[pad] = 0;
}

// ──────────────────────────────────────────────────────────────────────
// Button configuration
// ──────────────────────────────────────────────────────────────────────

// FF3 0x002259b8.
void PadKeyUpdate(void)
{
    const u_int *src = pad_key_cfg[GetOptionWrk()->now.pad_cfg];

    for (int i = 0; i < PAD_KEY_NUM; i++)
        pad_key[i] = src[i];
}

// FF3 0x00225a28.
u_int PadKeyGetConfig(int cfg, int key)
{
    return pad_key_cfg[cfg][key];
}
