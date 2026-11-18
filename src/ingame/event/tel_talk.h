/* ==========================================================================
 *  ingame/event/tel_talk.h
 *
 *  The telephone's talk script player (FF3-only, FF3 0x258b20..0x2590e0; no
 *  FF2 code). TEL_TALK_START (telephone.c TelTalkStart) news one, starts it
 *  with a script and a voice stream, and TelTalkMain runs Work every frame
 *  until it is over. A script is a list of {cmd, arg} records:
 *    0  message window line arg, waits for the advance button
 *    1  subtitle voice line arg (closes the window first), waits for its end
 *    2  wait arg frames (closes the window first)
 *    3  end: closes the window, plays the hang-up SE (TelTalkEndSePlay)
 *  The FF2 names Ghidra had on this run (GetSisterRot, ReqModeSisMotion,
 *  four soul_list.c names, sp_chance.c Work) were wrong matches.
 *
 *  Work's two arguments are passed on to the records: kind is the subtitle
 *  kind of command 1 (SubTitleReq / SubTitleReq3D), msg the YesNoSetMsg
 *  first message of command 0 (the line is the record's arg).
 * ======================================================================== */
#ifndef _INGAME_EVENT_TEL_TALK_H
#define _INGAME_EVENT_TEL_TALK_H

#include <stddef.h>                     // offsetof
#include "sdk/scetypes.h"
#include "sdk/libvu0.h"                 // sceVu0FVECTOR
#include "system/eeiop/snd_3d.h"        // SND_3D_SET

// TEL_TALK_CMD.cmd
enum TEL_TALK_CMD_NO
{
    TEL_TALK_CMD_WIN  = 0,              // message window line
    TEL_TALK_CMD_SUB  = 1,              // subtitle voice line
    TEL_TALK_CMD_WAIT = 2,              // wait arg frames
    TEL_TALK_CMD_END  = 3,              // end of the script
};

// One record of a talk script (data 0x363220 / 0x363250; no pointers).
struct TEL_TALK_CMD
{
    /* 0x0 */ int cmd;                  // TEL_TALK_CMD_NO
    /* 0x4 */ int arg;                  // message / line number, frames
};
static_assert(sizeof(TEL_TALK_CMD) == 0x8, "TEL_TALK_CMD is 0x8 bytes");
static_assert(offsetof(TEL_TALK_CMD, arg) == 0x4, "TEL_TALK_CMD.arg");

// layout: host -- cmd is an EE pointer at +0x00 and s3s holds three; the
// EE offsets are in the comments (operator new(0x40) in TelTalkStart).
class TelTalkScript                     /* 0x40 on the EE */
{
public:
    /* 0x00 */ const TEL_TALK_CMD *cmd; // the running record, null after Init
    /* 0x04 */ int           time;      // fade time: map_bgmFadeOut / In, the voice's fade-in
    /* 0x08 */ int           win;       // LayoutAnimStart id of the message window, -1: closed
    /* 0x0c */ int           stream;    // StreamAutoPlay id of the voice, -1: none
    /* 0x10 */ sceVu0FVECTOR pos;       // the voice's position when it is 3D
    /* 0x20 */ SND_3D_SET    s3s;       // s3s.pos = pos when Start got a position
    /* 0x2c */ int           wait;      // command 2's frames left
    /* 0x30 */ int           step;      // 0 start the record, 1 wait for it, 2 over

    TelTalkScript() { Init(); }         // FF3: operator new(0x40), then Init

    void Init();                                                        // FF3 0x00258b20
    void Start(const TEL_TALK_CMD *script, int file, int vol, int time,
               const float *pos);                                       // FF3 0x00258b58
    void Stop();                                                        // FF3 0x00258c28
    int  WinStart(int msg, int line);                                   // FF3 0x00258c78
    int  WinIsEnd();                                                    // FF3 0x00258ce8
    int  WinClose();                                                    // FF3 0x00258d30
    int  SubTitleStart(int kind, int line);                             // FF3 0x00258da0
    int  SubTitleIsEnd();                                               // FF3 0x00258e18
    int  WaitStart(int frames);                                         // FF3 0x00258e38
    int  WaitIsEnd();                                                   // FF3 0x00258e80
    int  EndStart();                                                    // FF3 0x00258ea8
    int  EndIsEnd();                                                    // FF3 0x00258ec8
    int  CmdStart(int cmd_no, int arg, int msg, int kind);              // FF3 0x00258ee8
    int  CmdIsEnd(int cmd_no);                                          // FF3 0x00258f60
    int  Work(int kind, int msg);                                       // FF3 0x00258fc8: 1 when over
};

#endif /* _INGAME_EVENT_TEL_TALK_H */
