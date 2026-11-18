/* ==========================================================================
 *  common/WindowMsg.h
 *
 *  CWindowMsg (FF3 RTTI name): a message / selection window drawn by layout
 *  animation type 5 (ingame/menu/anim_2d.c's layout message window). FF3-only
 *  file at 0x0010fd70..0x00110368 (between CProcCtl and CmemShelt; FF3's C++
 *  class files link alphabetically). Ghidra names its methods SelWin*
 *  (SelWinOpen is CWindowMsg::Open).
 * ======================================================================== */
#ifndef _COMMON_WINDOWMSG_H
#define _COMMON_WINDOWMSG_H

#include <stddef.h>                     // offsetof

class CWindowMsg;

// CWindowMsg::StartConfirm's callback, run when OK is chosen: < 0 turns the result
// into SEL_WIN_RES_FAILED.
typedef int (*SEL_WIN_FUNC)(CWindowMsg *win);

// CWindowMsg::mode
enum
{
    SEL_WIN_MODE_NONE    = 0,           // closed by CWindowMsg::Main
    SEL_WIN_MODE_MSG     = 1,           // message; also every mode once answered
    SEL_WIN_MODE_SEL     = 2,           // selection, answered by SelWinGetResult
    SEL_WIN_MODE_CONFIRM = 3,           // selection; OK runs on_ok
};

// CWindowMsg::result
enum
{
    SEL_WIN_RES_NONE   = 0,
    SEL_WIN_RES_OK     = 1,             // cursor 0
    SEL_WIN_RES_ALT    = 2,             // cursor 1
    SEL_WIN_RES_CANCEL = 3,             // cursor 3
    SEL_WIN_RES_FAILED = 4,             // on_ok returned < 0
};

// operator new 0x1c on the EE; vtable 0x341090 holds only the two
// destructors.
// layout: host -- the vptr and on_ok are 4 bytes on the EE, 8 here; EE
// offsets below.
class CWindowMsg
{
public:
    CWindowMsg();                          // FF3 0x0010fd98 SelWinCtor (base copy 0x0010fd70)
    virtual ~CWindowMsg();                 // EE vtable +0x00 0x0010fde8, deleting +0x04 0x0010fe10

    void Open();                        // FF3 0x0010fe48: layout 5, data load starts
    int IsReady();                      // FF3 0x0010fe90: layout data loaded
    void SetItem(int handle, int id);   // FF3 0x0010fec0
    void SetAutoClose(int on);          // FF3 0x0010ff58
    void Close();                       // FF3 0x0010ffa0
    void SetAlphaItemById(int handle, int id);  // FF3 0x0010ffe8
    void SetAlphaItemByName(int handle, const char *name);  // FF3 0x00110018
    void SetAlphaItem(struct B2D_ITEM *item);  // FF3 0x00110048
    void SetFlags(int no_ok_se, int no_cancel_se, int silent_csr);  // FF3 0x00110078
    void SetAlpha(int alpha);           // FF3 0x001100a8
    int GetAlpha();                     // FF3 0x001100c0
    void StartMsg(int msg0, int msg1, int can_cancel);  // FF3 0x001100d8
    void StartSel(int msg0, int msg1, int can_cancel, int type);  // FF3 0x00110140
    void StartConfirm(int msg0, int msg1, int can_cancel, int type);  // FF3 0x001101c0
    int Main();                         // FF3 0x00110270: returns the result

    /* 0x04 */ int handle;              // layout animation slot, -1: closed
    /* 0x08 */ int on_ok_ret;           // last on_ok return value
    /* 0x0c */ int mode;                // SEL_WIN_MODE_*
    /* 0x10 */ int result;              // SEL_WIN_RES_*
    /* 0x14 */ SEL_WIN_FUNC on_ok;      // SEL_WIN_MODE_CONFIRM's callback
    /* 0x18 */ int auto_close;          // close once the layout stops (CWindowMsg::SetAutoClose)
};
static_assert(offsetof(CWindowMsg, handle) == sizeof(void *), "CWindowMsg.handle");
static_assert(offsetof(CWindowMsg, on_ok_ret) == sizeof(void *) + 0x04, "CWindowMsg.on_ok_ret");
static_assert(offsetof(CWindowMsg, mode) == sizeof(void *) + 0x08, "CWindowMsg.mode");
static_assert(offsetof(CWindowMsg, result) == sizeof(void *) + 0x0c, "CWindowMsg.result");
static_assert(offsetof(CWindowMsg, on_ok) == sizeof(void *) + 0x10, "CWindowMsg.on_ok");
static_assert(offsetof(CWindowMsg, auto_close) == offsetof(CWindowMsg, on_ok) + sizeof(void *),
              "CWindowMsg.auto_close");

#endif /* _COMMON_WINDOWMSG_H */
