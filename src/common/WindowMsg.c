// common/WindowMsg.c -- FF3 CWindowMsg: a message / selection window on
// layout animation type 5. FF3-only (no FF2 counterpart);
// 0x0010fd70..0x00110368.

#include "common/WindowMsg.h"
#include "ingame/menu/anim_2d.h"

// ──────────────────────────────────────────────────────────────────────
// Closed, no mode, no result, no callback.
// FF3 0x0010fd98 (SelWinCtor; 0x0010fd70 SelWinCtorBase is the identical
// base-object copy).

CWindowMsg::CWindowMsg()
{
    auto_close = 0;
    mode       = SEL_WIN_MODE_NONE;
    handle     = -1;
    on_ok      = nullptr;
    result     = SEL_WIN_RES_NONE;
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x0010fde8 (SelWinDtor; 0x0010fdc0 SelWinDtorBase is the identical
// base-object copy); the deleting variant 0x0010fe10 adds operator delete.

CWindowMsg::~CWindowMsg()
{
    Close();
}

// ──────────────────────────────────────────────────────────────────────
// Starts layout animation 5 (the message window; its data starts loading),
// flags 6, and keeps the message open after an answer until CWindowMsg::Close.
// FF3 0x0010fe48.

void CWindowMsg::Open()
{
    handle = LayoutAnimStart(5, 0);
    LayoutAnimSetFlag(handle, 6);
    LayoutMsgSetKeepOpen(1);
    auto_close = 0;
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x0010fe90.

int CWindowMsg::IsReady()
{
    if (handle < 0)
        return 0;
    return LayoutAnimIsDataReady(handle);
}

// ──────────────────────────────────────────────────────────────────────
// Clears layout flag 2; with id >= 0 the item (handle, id) gets the window's
// alpha, else layout flag 4 is cleared too.
// FF3 0x0010fec0.

void CWindowMsg::SetItem(int handle, int id)
{
    if (this->handle < 0)
        return;

    LayoutAnimClearFlag(this->handle, 2);
    if (id >= 0)
    {
        LayoutAnimSetAlphaItemById(this->handle, handle, id);
        return;
    }
    LayoutAnimClearFlag(this->handle, 4);
}

// ──────────────────────────────────────────────────────────────────────
// on: CWindowMsg::Main closes the window once the layout has stopped; the message
// no longer stays open after an answer.
// FF3 0x0010ff58.

void CWindowMsg::SetAutoClose(int on)
{
    LayoutMsgSetKeepOpen(on == 0);
    auto_close = on;
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x0010ffa0.

void CWindowMsg::Close()
{
    if (handle < 0)
        return;

    LayoutMsgSetKeepOpen(0);
    LayoutAnimReset(handle);
    handle = -1;
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x0010ffe8.

void CWindowMsg::SetAlphaItemById(int handle, int id)
{
    if (this->handle >= 0)
        LayoutAnimSetAlphaItemById(this->handle, handle, id);
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x00110018.

void CWindowMsg::SetAlphaItemByName(int handle, const char *name)
{
    if (this->handle >= 0)
        LayoutAnimSetAlphaItemByName(this->handle, handle, name);
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x00110048.

void CWindowMsg::SetAlphaItem(B2D_ITEM *item)
{
    if (handle >= 0)
        LayoutAnimSetAlphaItem(handle, item);
}

// ──────────────────────────────────────────────────────────────────────
// Which of the message window's sound effects stay silent (LAYOUT_MSG_NO_*).
// FF3 0x00110078.

void CWindowMsg::SetFlags(int no_ok_se, int no_cancel_se, int silent_csr)
{
    int flags = no_cancel_se != 0 ? LAYOUT_MSG_NO_CANCEL_SE : 0;

    if (no_ok_se)
        flags |= LAYOUT_MSG_NO_OK_SE;
    if (silent_csr)
        flags |= LAYOUT_MSG_SILENT_CSR;
    LayoutMsgSetFlags(flags);
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x001100a8 (no open check).

void CWindowMsg::SetAlpha(int alpha)
{
    LayoutAnimSetAlpha(handle, alpha);
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x001100c0 (no open check).

int CWindowMsg::GetAlpha()
{
    return LayoutAnimGetAlpha(handle);
}

// ──────────────────────────────────────────────────────────────────────
// Message (msg0, msg1) without a choice; can_cancel: cancel closes it too.
// FF3 0x001100d8.

void CWindowMsg::StartMsg(int msg0, int msg1, int can_cancel)
{
    if (handle < 0)
        return;

    Anim2dMsgSet(msg0, msg1);
    Anim2dMsgSetType(2);
    LayoutMsgSetArg(can_cancel ? 1 : 2);
    mode   = SEL_WIN_MODE_MSG;
    result = SEL_WIN_RES_NONE;
}

// ──────────────────────────────────────────────────────────────────────
// Message (msg0, msg1) with a choice; type 1 starts the cursor on the second
// entry.
// FF3 0x00110140.

void CWindowMsg::StartSel(int msg0, int msg1, int can_cancel, int type)
{
    if (handle < 0)
        return;

    Anim2dMsgSet(msg0, msg1);
    Anim2dMsgSetMode(1, type == 1);
    LayoutMsgSetArg(can_cancel ? 1 : 2);
    mode   = SEL_WIN_MODE_SEL;
    result = SEL_WIN_RES_NONE;
}

// ──────────────────────────────────────────────────────────────────────
// A selection whose OK runs win->on_ok (CWindowMsg::Main).
// FF3 0x001101c0 (no callers in FF3).

void CWindowMsg::StartConfirm(int msg0, int msg1, int can_cancel, int type)
{
    if (handle < 0)
        return;

    StartSel(msg0, msg1, can_cancel, type);
    mode = SEL_WIN_MODE_CONFIRM;
}

// ──────────────────────────────────────────────────────────────────────
// Once the message window has an answer, maps its cursor to a result
// (0 -> OK, 1 -> ALT, 3 -> CANCEL; anything else leaves *result) and
// returns 1.
// FF3 0x001101f8.

static int SelWinGetResult(int *result)
{
    int csr;

    if (LayoutMsgGetResult() == LAYOUT_MSG_NONE)
        return 0;

    csr = Anim2dMsgGetCsr();
    if (csr == 1)
        *result = SEL_WIN_RES_ALT;
    else if (csr == 0)
        *result = SEL_WIN_RES_OK;
    else if (csr == 3)
        *result = SEL_WIN_RES_CANCEL;
    return 1;
}

// ──────────────────────────────────────────────────────────────────────
// Per frame: with auto_close, closes the window once its layout is idle
// (state 0) or finished (state 4). A selection's answer lands in
// win->result and turns the mode back to SEL_WIN_MODE_MSG; a confirmation's
// OK first runs on_ok (< 0: SEL_WIN_RES_FAILED). Returns the result.
// FF3 0x00110270.

int CWindowMsg::Main()
{
    int state;

    if (handle < 0)
        return 0;

    if (auto_close)
    {
        state = LayoutAnimGetState(handle);
        if (state == 0 || state == 4)
        {
            mode = SEL_WIN_MODE_NONE;
            Close();
            return 0;
        }
    }

    switch (mode)
    {
    case SEL_WIN_MODE_SEL:
        if (!SelWinGetResult(&result))
            return result;
        break;

    case SEL_WIN_MODE_CONFIRM:
        if (!SelWinGetResult(&result))
            return result;
        if (result == SEL_WIN_RES_OK && on_ok != nullptr)
        {
            on_ok_ret = on_ok(this);
            if (on_ok_ret < 0)
                result = SEL_WIN_RES_FAILED;
        }
        break;

    default:
        return result;
    }

    mode = SEL_WIN_MODE_MSG;
    return result;
}
