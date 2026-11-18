/* ==========================================================================
 *  ingame/subtitle/subtitle.h
 *
 *  Spoken-line captions (FF2 subtitle.c), FF3 0x2590e0..0x259680 plus the
 *  out-of-line statics SubTitleCheckDispAccept 0x2bf058 / SubTitleReqSub
 *  0x2bf0b8.
 *
 *  The subtitle file (0xf11 SUBTITLE_OBJ, loaded once at boot) is
 *  self-relative: five u32 offsets, one per kind, each to a table of u32
 *  row offsets, each to a SUBTITLE_DATA -- the voice stream's CD file and
 *  the (message id, frame count) lines of the caption. One caption runs at a
 *  time: a request stops the running one, queues its voice stream and steps
 *  through the lines in time with it.
 * ======================================================================== */
#ifndef _INGAME_SUBTITLE_SUBTITLE_H
#define _INGAME_SUBTITLE_SUBTITLE_H

#include <stddef.h>                     // offsetof
#include "sdk/scetypes.h"

// The tables of the subtitle file (FF2 had one table of 250).
enum SUBTITLE_KIND
{
    SUBTITLE_KIND_EVENT = 0,            // EV_VOICE_* streams, the event script's lines
    SUBTITLE_KIND_NPC   = 1,            // NPC_VOICE_* (npc_act.c)
    SUBTITLE_KIND_TEL   = 2,            // TEL_* (telephone)
    SUBTITLE_KIND_TAPE  = 3,            // TAPE_* (cassette; drawn from message bank 0x80)
    SUBTITLE_KIND_ENEMY = 4,            // FG_VOICE_* (ghost voices, enemy.c)
    SUBTITLE_KIND_MAX   = 5
};

enum SUBTITLE_STATUS
{
    SUBTITLE_STATUS_IDLE    = 0,
    SUBTITLE_STATUS_PRELOAD = 1,        // stream queued, waiting on the CD
    SUBTITLE_STATUS_EXEC    = 2         // stream playing, lines running
};

// MsgId values below 0 are never drawn. FF3 adds -2..-4 to FF2's -1.
#define SUBTITLE_MSG_HOLD   (-1)        // blank; the lines stop, the stream's end ends the caption
#define SUBTITLE_MSG_BLANK  (-2)        // blank for Frame frames (also SubTitleGetMsgId: nothing running)
#define SUBTITLE_MSG_END    (-3)        // the caption ends here, the stream keeps playing
#define SUBTITLE_MSG_LOOP   (-4)        // back to the first line

// One line: message MsgId for Frame frames (60 Hz; 5/6 of them in PAL).
struct SUBTITLE_MSG_DATA                /* 0x4 */
{
    /* 0x0 */ short MsgId;
    /* 0x2 */ short Frame;
};
static_assert(sizeof(SUBTITLE_MSG_DATA) == 0x4, "SUBTITLE_MSG_DATA is 4 bytes");
static_assert(offsetof(SUBTITLE_MSG_DATA, Frame) == 0x2, "SUBTITLE_MSG_DATA.Frame");

// One caption row of the file. MsgData runs on to its terminator (the ROM's
// zero-length trailing array; [1] here so MSVC takes it).
struct SUBTITLE_DATA                    /* 0x4 + the lines */
{
    /* 0x0 */ short StreamFile;         // CD file of the voice (.str; its .hxd header is + 1), < 0: none
    /* 0x2 */ short CharId;             // FF2: the speaker for the mouth anime; FF3 never reads it (all -1)
    /* 0x4 */ SUBTITLE_MSG_DATA MsgData[1];
};
static_assert(offsetof(SUBTITLE_DATA, CharId) == 0x2, "SUBTITLE_DATA.CharId");
static_assert(offsetof(SUBTITLE_DATA, MsgData) == 0x4, "SUBTITLE_DATA.MsgData");

// The head of the subtitle file: byte offsets (from the file's start) of the
// per-kind row offset tables, which are offsets from the file's start too.
struct SUBTITLE_FILE_HEAD               /* 0x14 */
{
    /* 0x00 */ u_int KindTbl[SUBTITLE_KIND_MAX];
};
static_assert(sizeof(SUBTITLE_FILE_HEAD) == 0x14, "SUBTITLE_FILE_HEAD is 0x14 bytes");

// The single caption slot, sbss 0x3fd040 (0x20 bytes on the EE).
// layout: host -- pSubTitleData is 8 bytes here; every later field sits 4
// higher than on the EE (+0x04 StreamId .. +0x1c Kind). Nothing outside this
// file sees the struct.
struct SUBTITLE_CTRL
{
    /* 0x00 */ SUBTITLE_DATA *pSubTitleData;
    /* 0x04 */ int StreamId;            // -1: no stream (none queued, or the row has none)
    /* 0x08 */ int Status;              // SUBTITLE_STATUS
    /* 0x0c */ int Counter;             // frames the current line has run
    /* 0x10 */ int MsgDataNo;           // index into MsgData[]
    /* 0x14 */ int ObjType;             // GetObjectPos class the voice follows, -1: fixed
    /* 0x18 */ int ObjId;
    /* 0x1c */ int Kind;                // SUBTITLE_KIND, -1: none (picks the message bank)
};
static_assert(offsetof(SUBTITLE_CTRL, pSubTitleData) == 0x0, "SUBTITLE_CTRL.pSubTitleData");
static_assert(offsetof(SUBTITLE_CTRL, StreamId) == sizeof(void *), "SUBTITLE_CTRL.StreamId");
static_assert(offsetof(SUBTITLE_CTRL, Kind) == sizeof(void *) + 0x18, "SUBTITLE_CTRL.Kind");

extern SUBTITLE_CTRL SubTitleCtrl;      // sbss 0x3fd040
extern void *subtitle_data;             // sdata 0x372604: the loaded subtitle file

void SubTitleDataLoadReq(void);                                 // FF3 0x002590e0: boot, file 0xf11
void SubTitleDataFree(void);                                    // FF3 0x00259148: no callers
void SubTitleCtrlStructInit(void);                              // FF3 0x00259190
// One frame. subtitle_on: the option (off, only always-shown messages draw);
// check_disp: 1 hides the line while a message page, the viewfinder, the
// chapter title or a layout anime is up.
void SubTitleMain(int subtitle_on, int check_disp);             // FF3 0x002591c0

// The request forms: all go through SubTitleReqSub and differ in where the
// voice is heard from and whether the stream is theirs to start.
void SubTitleReq(int kind, int no);                             // FF3 0x00259430: no position
void SubTitleReq3D(int kind, int no, float *pos);               // FF3 0x00259458: at a point
void SubTitleReq3DObj(int kind, int no, int obj_type, int obj_id);  // FF3 0x00259490: following an object
void SubTitleReqEne(int no, int stream);                        // FF3 0x00259508: a ghost voice enemy.c started
void SubTitleStop(void);                                        // FF3 0x00259538
int  SubTitleIsEnd(void);                                       // FF3 0x002595a0: nothing running
int  SubTitleGetStreamFile(int kind, int no);                   // FF3 0x002595b0: the row's voice file
int  SubTitleGetMsgId(void);                                    // FF3 0x00259610: the line now running, -2: none
int  SubTitleGetCounter(void);                                  // FF3 0x00259658: no callers
int  SubTitleIsExec(void);                                      // FF3 0x00259668: the stream has started

#endif /* _INGAME_SUBTITLE_SUBTITLE_H */
