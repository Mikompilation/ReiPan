/* ==========================================================================
 *  ingame/event/tel_talk.c
 *
 *  The telephone's talk script player (see tel_talk.h). FF3-only, FF3
 *  0x258b20..0x2590e0; Ghidra's flat names are TelTalkScript<Method>.
 *
 *  Each record is started once (CmdStart, until it accepts) and then polled
 *  (CmdIsEnd) until it is over; the window, subtitle and wait records close
 *  the message window first and refuse to start until it is closed.
 * ======================================================================== */
#include "ingame/event/tel_talk.h"

#include "graphics/motion/anicode.h"            // YesNoSetMsg, YesNoSetSel, YesNoReqClose
#include "ingame/event/telephone.h"             // TelTalkEndSePlay
#include "ingame/map/map_bgm.h"                 // map_bgmFadeOut, map_bgmFadeIn, map_bgmIsPlaying
#include "ingame/menu/anim_2d.h"                // LayoutAnimStart, LayoutAnimGetState, LayoutAnimReset
#include "ingame/subtitle/subtitle.h"           // SubTitleReq, SubTitleReq3D, SubTitleStop, SubTitleIsEnd
#include "system/eeiop/stream_auto.h"           // StreamAutoPlay, StreamAutoFadeOut, StreamAutoIsPlaying
#include "system/pad/pad_ctrl.h"                // CheckPadTrgPlaySe, pad_key
#include "ingame/menu/menu_mouse.h"    // PC: the mouse in menus

#define TEL_TALK_ANIM_WIN   6                   // layout anim type of the yes/no window (no choice shown)
#define TEL_TALK_YESNO_SEL  2                   // YesNoSetSel: no selection
#define TEL_TALK_SE_OK      0x1a9b0003          // the confirm SE
#define TEL_TALK_STREAM_PRI 0xd                 // StreamAutoPlay priority of the voice

// The work's step.
enum
{
    TEL_TALK_STEP_START = 0,                    // start the record
    TEL_TALK_STEP_WAIT  = 1,                    // wait for it to end
    TEL_TALK_STEP_OVER  = 2,                    // the end record ran
};

// LayoutAnimGetState.
enum
{
    TEL_TALK_ANIM_FREE   = 0,
    TEL_TALK_ANIM_OPEN   = 2,
    TEL_TALK_ANIM_CLOSED = 4,
};

// FF3 0x00258b20
void TelTalkScript::Init()
{
    cmd = NULL;
    stream = -1;
    time = 0;
    win = -1;
    pos[0] = pos[1] = pos[2] = pos[3] = 0.0f;  // FF3: vsub pos, pos
    step = TEL_TALK_STEP_START;
    s3s.pos = NULL;
    wait = 0;
}

// The map BGM is stopped over time frames; the voice stream (file, its
// header file + 1) plays at vol and is 3D at pos when pos is given.
// FF3 0x00258b58
void TelTalkScript::Start(const TEL_TALK_CMD *script, int file, int vol, int time, const float *pos)
{
    SND_3D_SET *set = NULL;

    Init();
    if (pos != NULL)
    {
        this->pos[0] = pos[0];
        this->pos[1] = pos[1];
        this->pos[2] = pos[2];
        this->pos[3] = pos[3];
        s3s.pos = this->pos;
        set = &s3s;
    }
    map_bgmFadeOut(time, 0);
    if (file != -1)
        stream = StreamAutoPlay(file, file + 1, TEL_TALK_STREAM_PRI, 0, 1, vol, time, set);
    this->time = time;
    cmd = script;
}

// FF3 0x00258c28
void TelTalkScript::Stop()
{
    SubTitleStop();
    if (stream != -1)
        StreamAutoFadeOut(stream, 0);
    map_bgmFadeIn(time);
    Init();
}

// Command 0: opens the window once, then shows the line.
// FF3 0x00258c78
int TelTalkScript::WinStart(int msg, int line)
{
    if (win == -1)
    {
        win = LayoutAnimStart(TEL_TALK_ANIM_WIN, 0);
        YesNoSetSel(TEL_TALK_YESNO_SEL);
    }
    YesNoSetMsg(msg, line);
    return 1;
}

// Ends on the confirm button once the window is open.
// FF3 0x00258ce8
int TelTalkScript::WinIsEnd()
{
    if (LayoutAnimGetState(win) != TEL_TALK_ANIM_OPEN)
        return 0;
    ReiPan_MenuMouseConfirm();          // PC: a click closes it
    return CheckPadTrgPlaySe((u_short)pad_key[PAD_KEY_OK], TEL_TALK_SE_OK, 0) != 0;
}

// 1 once the window is closed (or was never open).
// FF3 0x00258d30
int TelTalkScript::WinClose()
{
    int state;

    if (win == -1)
        return 1;

    YesNoReqClose(1);
    state = LayoutAnimGetState(win);
    if (state != TEL_TALK_ANIM_FREE && state != TEL_TALK_ANIM_CLOSED)
        return 0;

    LayoutAnimReset(win);
    win = -1;
    return 1;
}

// Command 1. FF3 passes &pos to SubTitleReq too (it takes two arguments).
// FF3 0x00258da0
int TelTalkScript::SubTitleStart(int kind, int line)
{
    if (WinClose() == 0)
        return 0;

    if (s3s.pos != NULL)
        SubTitleReq3D(kind, line, pos);
    else
        SubTitleReq(kind, line);
    return 1;
}

// FF3 0x00258e18
int TelTalkScript::SubTitleIsEnd()
{
    return ::SubTitleIsEnd() != 0;
}

// Command 2.
// FF3 0x00258e38
int TelTalkScript::WaitStart(int frames)
{
    if (WinClose() == 0)
        return 0;

    wait = frames;
    return 1;
}

// FF3 0x00258e80
int TelTalkScript::WaitIsEnd()
{
    if (wait > 1)
    {
        wait--;
        return 0;
    }
    return 1;
}

// Command 3.
// FF3 0x00258ea8
int TelTalkScript::EndStart()
{
    return WinClose() != 0;
}

// FF3 0x00258ec8
int TelTalkScript::EndIsEnd()
{
    TelTalkEndSePlay();
    return 1;
}

// FF3 0x00258ee8
int TelTalkScript::CmdStart(int cmd_no, int arg, int msg, int kind)
{
    switch (cmd_no)
    {
    case TEL_TALK_CMD_WIN:
        return WinStart(msg, arg);
    case TEL_TALK_CMD_SUB:
        return SubTitleStart(kind, arg);
    case TEL_TALK_CMD_WAIT:
        return WaitStart(arg);
    case TEL_TALK_CMD_END:
        return EndStart();
    }
    return 0;
}

// FF3 0x00258f60
int TelTalkScript::CmdIsEnd(int cmd_no)
{
    switch (cmd_no)
    {
    case TEL_TALK_CMD_WIN:
        return WinIsEnd();
    case TEL_TALK_CMD_SUB:
        return SubTitleIsEnd();
    case TEL_TALK_CMD_WAIT:
        return WaitIsEnd();
    case TEL_TALK_CMD_END:
        return EndIsEnd();
    }
    return 0;
}

// Once per frame; 1 when the call is over. The call is also over when the
// voice stream has ended: FF3 then returns 1 without Stop (the map BGM is
// not faded back in and a window stays open), kept. Nothing runs while the
// map BGM still plays (its fade-out from Start).
// FF3 0x00258fc8
int TelTalkScript::Work(int kind, int msg)
{
    if (stream != -1 && StreamAutoIsPlaying(stream) == 0)
        return 1;
    if (map_bgmIsPlaying() != 0)
        return 0;

    // PC guard: a second TEL_TALK_START mid-call Stops it (Init nulls cmd)
    // and FF3 then reads record 0 at address 0 (kernel RAM on the EE).
    // Ending the call there is what the stopped call means.
    if (cmd == NULL)
        return 1;

    switch (step)
    {
    case TEL_TALK_STEP_START:
        if (CmdStart(cmd->cmd, cmd->arg, msg, kind) == 0)
            return 0;
        step = TEL_TALK_STEP_WAIT;
        // fall through
    case TEL_TALK_STEP_WAIT:
        break;
    case TEL_TALK_STEP_OVER:
        return 1;
    default:
        return 0;
    }

    if (CmdIsEnd(cmd->cmd) != 0)
    {
        if (cmd->cmd == TEL_TALK_CMD_END)
        {
            Stop();
            step = TEL_TALK_STEP_OVER;
            return 1;
        }
        step = TEL_TALK_STEP_START;
        cmd++;
    }
    return 0;
}
