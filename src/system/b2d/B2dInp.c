// system/b2d/B2dInp.c -- FF3's B2d input (FF3 0x1055d8..0x105998, assert
// string "B2dInp.c"). No FF2 counterpart: written from the FF3 decompilation.
// Per Bp handle 18 labels; B2dInpUpdate reads the pad through a hook and plays
// the label bound to the event. FF3 retail never calls B2dInpUpdate (nor
// B2dInpSetBtnArg / B2dInpGetWait / B2dInpEnd); the layouts' input commands
// still fill the labels.

#include "system/b2d/B2dInp.h"
#include "system/b2d/B2dCtl.h"          // B2dErrPrint, B2D_HOOK_ARGS
#include "system/b2d/B2dFile.h"         // B2dFilePlayLabel, B2dFileSet/ClearPartFlags, B2D_PFLAG_*

B2D_INP b2d_inp[B2D_ENTRY_MAX];         // 0x374868

static B2D_ITEM_HOOK_FUNC b2d_inp_dir_func;   // sbss gp-0x7fa0: a0 the wait, returns the event bits
static B2D_ITEM_HOOK_FUNC b2d_inp_btn_func;   // sbss gp-0x7f9c: a0 the bits, a1 btn_arg (bits & 0xfff0 only)
static B2D_ITEM_HOOK_FUNC b2d_inp_play_func;  // sbss gp-0x7f98: before a label plays, no arguments
// Nothing in FF3 retail sets the last two (B2dInpInit only clears them).

// Which label an event plays: the first entry whose mask bits are all set in
// the hook's result (data 0x2ef308). Bits 1/2/4/8 are directions, two of
// them a diagonal, 0x10..0x2000 buttons.
struct B2D_INP_EVENT
{
    int label;                          // index into B2D_INP.label; < 0 ends the table
    int mask;
};
static_assert(sizeof(B2D_INP_EVENT) == 8, "B2D_INP_EVENT is 8 bytes");

static const B2D_INP_EVENT b2d_inp_event[] = {   // data 0x2ef308
    {  1, 0x0003 }, {  3, 0x0006 }, {  5, 0x000c }, {  7, 0x0009 },
    {  0, 0x0001 }, {  2, 0x0002 }, {  4, 0x0004 }, {  6, 0x0008 },
    {  8, 0x0010 }, {  9, 0x0020 }, { 10, 0x0040 }, { 11, 0x0080 },
    { 12, 0x0100 }, { 13, 0x0200 }, { 14, 0x0400 }, { 15, 0x0800 },
    { 16, 0x1000 }, { 17, 0x2000 }, { -1, -1 },
};

static const char b2d_inp_file[] = "B2dInp.c";  // rodata 0x34beb8

// ──────────────────────────────────────────────────────────────────────
// The input entry of `handle`; the error code is the handle when it is out
// of range, the entry's state when that is negative.
// FF3 0x001055d8.

B2D_INP *B2dInpGetEntry(int handle)
{
    int line = 0x3a;
    int code = handle;

    if ((u_int)handle < B2D_ENTRY_MAX)
    {
        B2D_INP *inp = &b2d_inp[handle];

        code = inp->state;
        if (code >= 0)
            return inp;
        line = 0x3e;
    }
    B2dErrPrint(code, b2d_inp_file, line, 0);
    return nullptr;
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x00105638.

const char **B2dInpGetLabels(int handle)
{
    B2D_INP *inp = B2dInpGetEntry(handle);

    return (inp != nullptr) ? inp->label : nullptr;
}

// ──────────────────────────────────────────────────────────────────────
// Binds event n to `label` (B2dCmdInput). FF3 0x00105660.

int B2dInpSetLabel(int handle, u_int n, const char *label)
{
    const char **labels = B2dInpGetLabels(handle);

    if (labels == nullptr)
        return -2;
    if (n >= B2D_INP_LABELS)
    {
        B2dErrPrint(-1, b2d_inp_file, 0x67, 0);
        return -1;
    }
    labels[n] = label;
    return 0;
}

// ──────────────────────────────────────────────────────────────────────
// Unbinds every event (B2dFileRunCmds, before a label's commands run).
// FF3 0x001056d8.

void B2dInpClearLabels(int handle)
{
    if ((u_int)handle > B2D_ENTRY_MAX - 1)
    {
        B2dErrPrint(-1, b2d_inp_file, 0x77, 0);
        return;
    }
    for (int i = 0; i < B2D_INP_LABELS; i++)
        b2d_inp[handle].label[i] = nullptr;
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x00105750.

void B2dInpSetDirFunc(B2D_ITEM_HOOK_FUNC func)
{
    b2d_inp_dir_func = func;
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x00105768.

void B2dInpSetBtnArg(int handle, int arg)
{
    B2D_INP *inp = B2dInpGetEntry(handle);

    if (inp != nullptr)
        inp->btn_arg = arg;
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x00105798.

void B2dInpSetWait(int handle, int wait)
{
    B2D_INP *inp = B2dInpGetEntry(handle);

    if (inp != nullptr)
        inp->wait = wait;
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x001057c8.

int B2dInpGetWait(int handle)
{
    B2D_INP *inp = B2dInpGetEntry(handle);

    return (inp != nullptr) ? inp->wait : 0;
}

// ──────────────────────────────────────────────────────────────────────
// Clears every entry's argument, wait and labels, and the hooks. state is
// left alone (0 from bss). FF3 0x001057f8.

void B2dInpInit(void)
{
    for (int i = 0; i < B2D_ENTRY_MAX; i++)
    {
        b2d_inp[i].btn_arg = 0;
        b2d_inp[i].wait    = 0;
        B2dInpClearLabels(i);
    }
    b2d_inp_dir_func  = nullptr;
    b2d_inp_btn_func  = nullptr;
    b2d_inp_play_func = nullptr;
}

// ──────────────────────────────────────────────────────────────────────
// One frame of input: asks the direction hook for this frame's event bits
// (passing the wait, then counting it down), lets the button hook see button
// bits, then plays the label of the first event whose mask matches, with the
// parts made lockable while its commands run. 0, or -1 for a bad handle.
// FF3 0x00105858 (no caller in FF3 retail).

int B2dInpUpdate(int handle)
{
    if (b2d_inp_dir_func == nullptr)
        return 0;

    B2D_INP *inp = B2dInpGetEntry(handle);

    if (inp == nullptr)
        return -1;

    B2D_HOOK_ARGS args = {};

    args.a0 = inp->wait;
    u_int bits = (u_int)b2d_inp_dir_func(&args);

    if (inp->wait > 0)
        inp->wait--;
    if ((bits & 0xfff0) != 0 && b2d_inp_btn_func != nullptr)
    {
        B2D_HOOK_ARGS btn = {};

        btn.a0 = (int)bits;
        btn.a1 = inp->btn_arg;
        b2d_inp_btn_func(&btn);
    }

    for (const B2D_INP_EVENT *ev = b2d_inp_event; ev->label >= 0; ev++)
    {
        if ((ev->mask & bits) != (u_int)ev->mask)
            continue;
        if (inp->label[ev->label] == nullptr)
            continue;

        if (b2d_inp_play_func != nullptr)
        {
            B2D_HOOK_ARGS none = {};    // PC: FF3 calls it with no arguments

            b2d_inp_play_func(&none);
        }
        B2dFileSetPartFlags(handle, B2D_PFLAG_RESET_ON_SELECT | B2D_PFLAG_LOCKABLE);
        B2dFilePlayLabel(handle, inp->label[ev->label]);
        B2dFileClearPartFlags(handle, B2D_PFLAG_RESET_ON_SELECT | B2D_PFLAG_LOCKABLE | B2D_PFLAG_LOCKED);
        break;
    }
    return 0;
}

// ──────────────────────────────────────────────────────────────────────
// Empty; nothing calls it (B2dSystemEnd only calls B2dTrigEnd).
// FF3 0x00105990.

void B2dInpEnd(void)
{
}
