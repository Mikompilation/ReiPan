/* ==========================================================================
 *  ingame/event/ev_queue.c
 *
 *  The event result queue (see ev_queue.h). An entry plays as a chain of
 *  steps, each run once per frame by EvQueueMain:
 *    EvQueueStepMsg       first message (layout type 5, kept open)
 *    EvQueueStepMsgWait   OK pages it; after the last page
 *    EvQueueStepItemWait  the item-get window (layout type 2), OK closes it
 *    EvQueueStepDoc       the document in the message viewer (layout type 1)
 *    EvQueueStepDocWait
 *    EvQueueStepMsg2      second message (layout type 5)
 *    EvQueueStepMsg2Wait
 *    EvQueueStepReward    the rewards, the entry done, the next entry
 *  Steps an entry has no data for are passed over.
 *  FF3 0x24f3b0..0x24fb28, FF3's own code.
 * ======================================================================== */
#include "ingame/event/ev_queue.h"

#include <string.h>                         // memset

#include "graphics/graph2d/message.h"       // MsgGetPage, MsgPageNext
#include "graphics/motion/anicode.h"        // MsgViewSet*, ItemGet*, msg_pic_key_dat
#include "ingame/memo/memo.h"        // MemoSetPage
#include "ingame/story_menu_photo.h"        // DocFlagSet, DocGetMsgType
#include "ingame/item/prg/item.h"           // ItemGet
#include "ingame/menu/anim_2d.h"            // LayoutAnim*, Anim2dMsg*, LayoutMsgSetKeepOpen
#include "ingame/story_door.h"              // MapAreaSideSet
#include "ingame/sound/snd_ctrl.h"          // SndSePlay
#include "system/pad/pad_ctrl.h"            // CheckPadTrgPlaySe, pad_key
#include "ingame/menu/menu_mouse.h"    // PC: the mouse in menus

// ──────────────────────────────────────────────────────────────────────
// Callees in runs whose file is not settled yet (unassigned@ in tus.json).
// Move each block to its real header when that run is defined.
// ──────────────────────────────────────────────────────────────────────

// unassigned@001b1648 (the message picture keys)

// ──────────────────────────────────────────────────────────────────────
// Data
// ──────────────────────────────────────────────────────────────────────

// Layout anim types (anim_2d.c's layout_anim_tbl).
enum
{
    EV_QUEUE_ANIM_VIEWER   = 1,             // message viewer
    EV_QUEUE_ANIM_ITEM_GET = 2,             // item-get window
    EV_QUEUE_ANIM_MSG      = 5,             // message window
};

#define EV_QUEUE_MSG0_DEFAULT  0x27         // message type when an entry has none
#define EV_QUEUE_SE_ITEM_GET   0x1a9b0004   // SYS_BASE_BD

// The entries per kind (data 0x3622c0, 0x362460, 0x3625d0, 0x3626d8).
static const EV_QUEUE_ENT ev_queue_ent0[] =      // data 0x3622c0
{
    //   id  msg0  msg1   pic   doc  msg2   mdl    mdl_state           flag  item  side
    {    0,  114,    0,  354,   68,    1,    4, {    1,   -1,   -1 },   68,   -1,   -1 },
    {    1,  114,    2,  322,   39,    3,    2, {    2,   -1,   -1 },   39,   -1,   -1 },
    {    2,  114,    4,  318,   49,    5,   27, {    3,   -1,   -1 },   49,   -1,   -1 },
    {    3,  114,    8,  320,   42,    9,    0, {    4,   -1,   -1 },   42,   -1,   -1 },
    {    4,  114,   10,  340,   45,   11,   -1, {   -1,   -1,   -1 },   45,   -1,   -1 },
    {    5,  114,   12,   -1,   -1,   13,   -1, {   -1,   -1,   -1 },   -1,   -1,   -1 },
    {    6,  114,   14,  338,   48,   -1,    3, {    0,   -1,   -1 },   48,   -1,   -1 },
    {    7,  114,   16,   -1,   -1,   -1,    7, {    2,   -1,   -1 },   -1,   -1,   -1 },
    {    8,  114,   18,  336,   47,   19,    3, {    1,   -1,   -1 },   47,   -1,   -1 },
    {    9,  114,   20,  342,   51,   -1,   14, {    0,   -1,   -1 },   51,   -1,   -1 },
    {   10,  114,   22,   -1,   -1,   -1,   19, {    0,   -1,   -1 },   -1,   -1,   -1 },
    {   11,  114,   24,  354,   66,   25,   28, {    2,   -1,   -1 },   66,   -1,   -1 },
    {   12,  114,   26,  750,   -1,   27,   -1, {   -1,   -1,   -1 },   -1,   -1,   -1 },
    {   13,  114,   28,  318,   53,   29,   -1, {   -1,   -1,   -1 },   53,   -1,   -1 },
    {   14,  114,   30,  346,   55,   31,   28, {    3,   -1,   -1 },   55,   -1,   -1 },
    {   -1,   -1,   -1,   -1,   -1,   -1,   -1, {   -1,   -1,   -1 },   -1,   -1,   -1 },
};

static const EV_QUEUE_ENT ev_queue_ent1[] =      // data 0x362460
{
    //   id  msg0  msg1   pic   doc  msg2   mdl    mdl_state           flag  item  side
    {    0,  112,    0,  744,   -1,    1,   -1, {   -1,   -1,   -1 },   -1,   -1,   -1 },
    {    1,  112,    2,  777,   -1,    3,   -1, {   -1,   -1,   -1 },   -1,   -1,   -1 },
    {    2,  112,    4,  780,   -1,    5,   -1, {   -1,   -1,   -1 },   -1,   -1,   -1 },
    {    3,  112,    6,  786,   -1,    7,   -1, {   -1,   -1,   -1 },   -1,   -1,   -1 },
    {    4,  112,    8,  771,   -1,    9,   -1, {   -1,   -1,   -1 },   -1,   -1,   -1 },
    {    5,  112,   10,  753,   -1,   11,   -1, {   -1,   -1,   -1 },   -1,   -1,   -1 },
    {    6,  112,   12,  807,   -1,   13,   -1, {   -1,   -1,   -1 },   -1,   -1,   -1 },
    {    7,  112,   14,  774,   -1,   15,   -1, {   -1,   -1,   -1 },   -1,   -1,   -1 },
    {    8,  112,   16,  774,   -1,   17,   19, {    1,   -1,   -1 },   -1,   -1,   -1 },
    {    9,  112,   18,  783,   -1,   19,   -1, {   -1,   -1,   -1 },   -1,   -1,   -1 },
    {   10,  112,   20,  750,   -1,   21,   -1, {   -1,   -1,   -1 },   -1,   -1,   -1 },
    {   11,  112,   22,  795,   -1,   23,   -1, {   -1,   -1,   -1 },   -1,   -1,   -1 },
    {   12,  112,   24,  819,   -1,   25,   -1, {   -1,   -1,   -1 },   -1,   -1,   -1 },
    {   -1,   -1,   -1,   -1,   -1,   -1,   -1, {   -1,   -1,   -1 },   -1,   -1,   -1 },
};

static const EV_QUEUE_ENT ev_queue_ent2[] =      // data 0x3625d0
{
    //   id  msg0  msg1   pic   doc  msg2   mdl    mdl_state           flag  item  side
    {    0,  121,    0,  330,   76,    1,   -1, {   -1,   -1,   -1 },   76,   -1,   -1 },
    {    1,  121,    2,  334,   57,    3,    1, {    0,   -1,   -1 },   57,   -1,   -1 },
    {    2,  121,    4,  344,   52,    5,   14, {    1,   -1,   -1 },   52,   -1,   -1 },
    {    3,  121,    6,  246,   73,   -1,   -1, {   -1,   -1,   -1 },   73,   -1,   -1 },
    {    4,  121,    8,  118,   -1,   -1,   -1, {   -1,   -1,   -1 },   -1,   54,   -1 },
    {    5,  121,   46,  162,   -1,   -1,   -1, {   -1,   -1,   -1 },   -1,   55,   -1 },
    {    6,  121,   48,  198,   -1,   -1,   -1, {   -1,   -1,   -1 },   -1,   59,   -1 },
    {    7,  121,   50,  330,   78,   51,   -1, {   -1,   -1,   -1 },   78,   -1,   -1 },
    {    8,  121,   10,  330,   77,   11,   -1, {   -1,   -1,   -1 },   77,   -1,   -1 },
    {   -1,   -1,   -1,   -1,   -1,   -1,   -1, {   -1,   -1,   -1 },   -1,   -1,   -1 },
};

static const EV_QUEUE_ENT ev_queue_ent3[] =      // data 0x3626d8
{
    //   id  msg0  msg1   pic   doc  msg2   mdl    mdl_state           flag  item  side
    {    0,  121,   12,  318,   44,   13,   11, {    2,   -1,   -1 },   44,   -1,   -1 },
    {    1,  121,   14,  318,   46,   15,    8, {    1,   -1,   -1 },   46,   -1,   -1 },
    {    2,  121,   16,  332,   50,   17,   27, {    4,   -1,   -1 },   50,   -1,   -1 },
    {    3,  121,   18,  324,   60,   -1,   30, {    1,   -1,   -1 },   60,   -1,   -1 },
    {    4,  121,   20,   -1,   -1,   -1,   29, {    0,   -1,   -1 },   -1,   -1,   -1 },
    {    5,  121,   22,  324,   59,   -1,   31, {    1,   -1,   -1 },   59,   -1,   -1 },
    {    6,  121,   24,   -1,   -1,   -1,   29, {    1,   -1,   -1 },   -1,   -1,   -1 },
    {    7,  121,   26,  318,   54,   27,   13, {    1,   -1,   -1 },   54,   -1,   -1 },
    {   -1,   -1,   -1,   -1,   -1,   -1,   -1, {   -1,   -1,   -1 },   -1,   -1,   -1 },
};

// data 0x332ab0
static const EV_QUEUE_ENT *const ev_queue_tbl[EV_QUEUE_KIND_NUM] =
{
    ev_queue_ent0, ev_queue_ent1, ev_queue_ent2, ev_queue_ent3,
};

static EV_QUEUE_WRK ev_queue_wrk;           // bss 0x3f7c90

static int EvQueueStepReward(int kind, int idx);
static int EvQueueStepMsg2Wait(int kind, int idx);
static int EvQueueStepMsg2(int kind, int idx);
static int EvQueueStepDocWait(int kind, int idx);
static int EvQueueStepDoc(int kind, int idx);
static int EvQueueStepItemWait(int kind, int idx);
static int EvQueueStepMsgWait(int kind, int idx);
static int EvQueueStepMsg(int kind, int idx);

// ──────────────────────────────────────────────────────────────────────
// The queue
// ──────────────────────────────────────────────────────────────────────

// save_game_data entry 57: the queue orders.
// FF3 0x0024f398
int EvQueueSaveGet(void **addr)
{
    *addr = ev_queue_wrk.order;
    return sizeof(ev_queue_wrk.order);
}

// The kind's entry with this id, nullptr when the list has none.
// FF3 0x0024f3b0.

static const EV_QUEUE_ENT *EvQueueFindEntry(int kind, short id)
{
    const EV_QUEUE_ENT *ent = ev_queue_tbl[kind];

    for (; ent->id != -1; ent++)
    {
        if (ent->id == id)
            return ent;
    }
    return nullptr;
}

// ──────────────────────────────────────────────────────────────────────
// The queued entry of the kind with the lowest order, -1 when none.
// FF3 0x0024f400.

int EvQueueGetNext(int kind)
{
    const s_char *order = ev_queue_wrk.order[kind];
    int           next = -1;
    int           min = 1000;

    for (int i = 0; i < EV_QUEUE_ENT_MAX; i++)
    {
        if (order[i] >= 0 && order[i] < min)
        {
            min = order[i];
            next = i;
        }
    }
    return next;
}

// ──────────────────────────────────────────────────────────────────────
// Queue entry idx of the kind after every entry already queued.
// FF3 0x0024f460.

int EvQueueAdd(int kind, int idx)
{
    s_char *order = ev_queue_wrk.order[kind];
    s_char  max = -1;

    for (int i = 0; i < EV_QUEUE_ENT_MAX; i++)
    {
        if (order[i] > max)
            max = order[i];
    }
    order[idx] = max + 1;
    return 0;
}

// ──────────────────────────────────────────────────────────────────────
// An entry's order byte (GET_RESEARCH_STAT).
// FF3 0x0024f4c0.

int EvQueueGetOrder(int kind, int idx)
{
    return ev_queue_wrk.order[kind][idx];
}

// ──────────────────────────────────────────────────────────────────────
// Begin the next queued entry of the kind; EvQueueMain returns -1 at once
// when there is none.
// FF3 0x0024f4e0.

int EvQueueStart(int kind)
{
    int idx;

    ev_queue_wrk.kind = kind;
    idx = EvQueueGetNext(kind);
    ev_queue_wrk.idx = idx;
    ev_queue_wrk.step = EvQueueStepMsg;
    if (idx >= 0)
        ev_queue_wrk.ent = EvQueueFindEntry(kind, idx);
    return 0;
}

// ──────────────────────────────────────────────────────────────────────
// The steps
// ──────────────────────────────────────────────────────────────────────

// Give the entry's rewards, mark it done and begin the next one.
// FF3 0x0024f548.

static int EvQueueStepReward(int kind, int idx)
{
    const EV_QUEUE_ENT *ent = ev_queue_wrk.ent;
    short               mdl = ent->mdl;
    short               doc_get = ent->doc_get;
    short               item = ent->item;
    short               area_side = ent->area_side;

    if (mdl != -1)
    {
        for (int i = 0; i < 3; i++)
        {
            if (ev_queue_wrk.ent->mdl_state[i] >= 0)
                MemoSetPage(mdl, (s_char)ev_queue_wrk.ent->mdl_state[i], 1);
        }
    }
    if (doc_get != -1)
        DocFlagSet(doc_get);
    if (item != -1)
        ItemGet(item, 1);
    if (area_side != -1)
        MapAreaSideSet(area_side);

    ev_queue_wrk.order[kind][idx] = EV_QUEUE_DONE;
    EvQueueStart(kind);
    return 0;
}

// FF3 0x0024f670.

static int EvQueueStepMsg2Wait(int kind, int idx)
{
    if (LayoutAnimGetState(ev_queue_wrk.anim) == 4)
    {
        LayoutAnimReset(ev_queue_wrk.anim);
        ev_queue_wrk.step = EvQueueStepReward;
    }
    return 0;
}

// ──────────────────────────────────────────────────────────────────────
// The second message, when the entry has one.
// FF3 0x0024f6c0.

static int EvQueueStepMsg2(int kind, int idx)
{
    short msg2 = ev_queue_wrk.ent->msg2;
    short msg0 = ev_queue_wrk.ent->msg0;

    if (ev_queue_wrk.ent->msg0 < 0)
        msg0 = EV_QUEUE_MSG0_DEFAULT;

    if (msg2 < 0)
    {
        ev_queue_wrk.step = EvQueueStepReward;
    }
    else
    {
        ev_queue_wrk.anim = LayoutAnimStart(EV_QUEUE_ANIM_MSG, 0);
        Anim2dMsgSet(msg0, msg2);
        Anim2dMsgSetType(2);
        ev_queue_wrk.step = EvQueueStepMsg2Wait;
    }
    return 0;
}

// The viewer's slot is not reset here (FF3 leaves it to LayoutAnim).
// FF3 0x0024f750.

static int EvQueueStepDocWait(int kind, int idx)
{
    if (LayoutAnimGetState(ev_queue_wrk.anim) == 4)
        ev_queue_wrk.step = EvQueueStepMsg2;
    return 0;
}

// ──────────────────────────────────────────────────────────────────────
// Once the first message has closed: the document in the message viewer,
// when the entry has one.
// FF3 0x0024f798.

static int EvQueueStepDoc(int kind, int idx)
{
    short key = ev_queue_wrk.ent->doc_key;
    int   state;

    LayoutMsgSetKeepOpen(0);
    state = LayoutAnimGetState(ev_queue_wrk.msg_anim);
    if (state != 0 && state != 4)
        return 0;

    LayoutAnimReset(ev_queue_wrk.msg_anim);
    if (key < 0)
    {
        ev_queue_wrk.step = EvQueueStepMsg2;
    }
    else
    {
        ev_queue_wrk.anim = LayoutAnimStart(EV_QUEUE_ANIM_VIEWER, 0);
        MsgViewSetMsg(DocGetMsgType(key, 0), msg_pic_key_dat[key].msg1);
        MsgViewSetMode(MSG_VIEW_MODE_1);
        ev_queue_wrk.step = EvQueueStepDocWait;
    }
    return 0;
}

// ──────────────────────────────────────────────────────────────────────
// The item-get window: OK closes it while it shows; then the document.
// FF3 0x0024f868.

static int EvQueueStepItemWait(int kind, int idx)
{
    int state;

    LayoutMsgSetKeepOpen(0);
    state = LayoutAnimGetState(ev_queue_wrk.anim);
    if (state == 2)
    {
        ReiPan_MenuMouseConfirm();      // PC: a click closes it
        if (CheckPadTrgPlaySe(pad_key[PAD_KEY_OK], -1, 0))
            ItemGetReqClose(1);
    }
    else if (state == 4)
    {
        ev_queue_wrk.step = EvQueueStepDoc;
    }
    return 0;
}

// Turn the first message to its next page: 0 while pages are left, 1 on
// the last page.
// FF3 0x0024f8f0.

static int EvQueueMsgPageNext(void)
{
    int breaks = Anim2dMsgGetPageBreakNum();
    int page = MsgGetPage();

    if (breaks < 1)
        return 1;
    if (page < breaks - 1)
    {
        MsgPageNext();
        return 0;
    }
    return 1;
}

// ──────────────────────────────────────────────────────────────────────
// OK pages the first message; after its last page the item-get window,
// when the entry has a picture.
// FF3 0x0024f948.

static int EvQueueStepMsgWait(int kind, int idx)
{
    ReiPan_MenuMouseConfirm();          // PC: a click turns the page
    if (CheckPadTrgPlaySe(pad_key[PAD_KEY_OK], -1, 0) && EvQueueMsgPageNext())
    {
        short pic = ev_queue_wrk.ent->item_pic;

        if (pic < 0)
        {
            ev_queue_wrk.step = EvQueueStepDoc;
        }
        else
        {
            ev_queue_wrk.anim = LayoutAnimStart(EV_QUEUE_ANIM_ITEM_GET, 0);
            ItemGetSetFlags(ITEM_GET_NO_MSG | ITEM_GET_NO_3EC);
            ItemGetSetPic(pic);
            SndSePlay(EV_QUEUE_SE_ITEM_GET, 100, 0x1000, 0, nullptr, 0);
            ev_queue_wrk.step = EvQueueStepItemWait;
        }
    }
    return 0;
}

// ──────────────────────────────────────────────────────────────────────
// The first message, kept open while the player pages it.
// FF3 0x0024fa08.

static int EvQueueStepMsg(int kind, int idx)
{
    short msg0;

    ev_queue_wrk.msg_anim = LayoutAnimStart(EV_QUEUE_ANIM_MSG, 0);
    msg0 = ev_queue_wrk.ent->msg0;
    if (ev_queue_wrk.ent->msg0 < 0)
        msg0 = EV_QUEUE_MSG0_DEFAULT;
    Anim2dMsgSet(msg0, ev_queue_wrk.ent->msg1);
    LayoutMsgSetKeepOpen(1);
    Anim2dMsgSetType(2);
    ev_queue_wrk.step = EvQueueStepMsgWait;
    return 0;
}

// ──────────────────────────────────────────────────────────────────────
// Per frame while an event waits for the queue: 0 while an entry plays, -1
// once the kind has none left.
// FF3 0x0024fa78.

int EvQueueMain(void)
{
    if (ev_queue_wrk.idx < 0)
        return -1;
    ev_queue_wrk.step(ev_queue_wrk.kind, ev_queue_wrk.idx);
    return 0;
}

// ──────────────────────────────────────────────────────────────────────
// Init / reset / end (IngameInit, IngameNewGameReset, IngameEnd).
// ──────────────────────────────────────────────────────────────────────

// Nothing queued, nothing playing.
// FF3 0x0024fab8.

int EvQueueInit(void)
{
    memset(&ev_queue_wrk, 0, sizeof(ev_queue_wrk));
    memset(ev_queue_wrk.order, EV_QUEUE_FREE, sizeof(ev_queue_wrk.order));
    return 0;
}

// FF3 0x0024fb08.

void EvQueueReset(void)
{
    EvQueueInit();
}

// FF3 0x0024fb20.

int EvQueueEnd(void)
{
    return 0;
}
