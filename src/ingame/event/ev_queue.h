/* ==========================================================================
 *  ingame/event/ev_queue.h
 *
 *  The event result queue: what the game shows and gives once an event has
 *  happened (a message, an item-get picture, a document in the message
 *  viewer, a second message, then the rewards: model states, a finder-camera
 *  flag, an item, a map area side). Entries come from ROM lists, one per
 *  kind (ev_queue_tbl, data 0x332ab0); each entry's order byte says whether
 *  it is free (-1), queued (0.., lowest first) or done (-2).
 *  EvQueueAdd queues an entry; EvQueueStart picks the next queued entry of a
 *  kind and EvQueueMain plays it step by step until none is left (-1).
 *  Callers: the event script commands 0x1a0d78 (add) / 0x1a0da0 (start, run)
 *  and the companion NPC's act steps PlyrCameraReportStep (kind 0) /
 *  PlyrCameraReport1Step (kind 1), which also ask EvQueueGetNext.
 *  FF3 0x24f3b0..0x24fb28, FF3-only file (name descriptive).
 * ======================================================================== */
#ifndef _INGAME_EVENT_EV_QUEUE_H
#define _INGAME_EVENT_EV_QUEUE_H

#include <stddef.h>                     // offsetof

#include "sdk/scetypes.h"

#define EV_QUEUE_KIND_NUM   4
#define EV_QUEUE_ENT_MAX    50          // order bytes per kind

#define EV_QUEUE_FREE       -1          // order byte: not queued
#define EV_QUEUE_DONE       -2          // order byte: played (0xfe)

// One ROM entry (data 0x3622c0.., lists end with id -1). -1 in a field: no
// such step / reward.
struct EV_QUEUE_ENT                     /* 0x1a */
{
    /* 0x00 */ short id;                // == the entry's order index
    /* 0x02 */ short msg0;              // message type of both messages; < 0: 0x27
    /* 0x04 */ short msg1;              // first message
    /* 0x06 */ short item_pic;          // item-get window picture (MENU_PIC)
    /* 0x08 */ short doc_key;           // message viewer: msg_pic_key_dat key
    /* 0x0a */ short msg2;              // second message
    /* 0x0c */ short mdl;               // memo for MemoSetPage
    /* 0x0e */ short mdl_state[3];      // its states (each -1: skip)
    /* 0x14 */ short doc_get;           // DocFlagSet key (a document), -1 none
    /* 0x16 */ short item;              // ItemGet(item, 1)
    /* 0x18 */ short area_side;         // MapAreaSideSet
};
static_assert(sizeof(EV_QUEUE_ENT) == 0x1a, "EV_QUEUE_ENT is 0x1a bytes");
static_assert(offsetof(EV_QUEUE_ENT, msg0) == 0x02, "EV_QUEUE_ENT.msg0");
static_assert(offsetof(EV_QUEUE_ENT, msg1) == 0x04, "EV_QUEUE_ENT.msg1");
static_assert(offsetof(EV_QUEUE_ENT, item_pic) == 0x06, "EV_QUEUE_ENT.item_pic");
static_assert(offsetof(EV_QUEUE_ENT, doc_key) == 0x08, "EV_QUEUE_ENT.doc_key");
static_assert(offsetof(EV_QUEUE_ENT, msg2) == 0x0a, "EV_QUEUE_ENT.msg2");
static_assert(offsetof(EV_QUEUE_ENT, mdl) == 0x0c, "EV_QUEUE_ENT.mdl");
static_assert(offsetof(EV_QUEUE_ENT, mdl_state) == 0x0e, "EV_QUEUE_ENT.mdl_state");
static_assert(offsetof(EV_QUEUE_ENT, doc_get) == 0x14, "EV_QUEUE_ENT.doc_get");
static_assert(offsetof(EV_QUEUE_ENT, item) == 0x16, "EV_QUEUE_ENT.item");
static_assert(offsetof(EV_QUEUE_ENT, area_side) == 0x18, "EV_QUEUE_ENT.area_side");

// A step of the entry being played; returns 0.
typedef int (*EV_QUEUE_STEP)(int kind, int idx);

// The queue work (bss 0x3f7c90, 0xe0 on the EE; EvQueueInit).
// layout: host -- ent and step are pointers (EE 4 bytes each); order
// follows them (EE +0x18).
struct EV_QUEUE_WRK
{
    /* 0x00 */ int                 kind;    // EvQueueStart's kind
    /* 0x04 */ int                 idx;     // entry being played, -1: none left
    /* 0x08 */ int                 anim;    // layout anim slot: item get / viewer / second message
    /* 0x0c */ int                 msg_anim;    // layout anim slot of the first message
    /* 0x10 */ const EV_QUEUE_ENT *ent;
    /* 0x14 */ EV_QUEUE_STEP       step;
    /* 0x18 */ s_char              order[EV_QUEUE_KIND_NUM][EV_QUEUE_ENT_MAX];
};
static_assert(offsetof(EV_QUEUE_WRK, idx) == 0x04, "EV_QUEUE_WRK.idx");
static_assert(offsetof(EV_QUEUE_WRK, anim) == 0x08, "EV_QUEUE_WRK.anim");
static_assert(offsetof(EV_QUEUE_WRK, msg_anim) == 0x0c, "EV_QUEUE_WRK.msg_anim");
static_assert(offsetof(EV_QUEUE_WRK, ent) == 0x10, "EV_QUEUE_WRK.ent");
static_assert(sizeof(((EV_QUEUE_WRK *)0)->order) == 200, "EV_QUEUE_WRK.order is 200 bytes");

int EvQueueSaveGet(void **addr);       // FF3 0x0024f398: {ev_queue_wrk.order (0x3f7ca8), 200}, save_game_data entry 57
int EvQueueGetOrder(int kind, int idx); // FF3 0x0024f4c0: order[kind][idx] (EV_QUEUE_FREE / EV_QUEUE_DONE / queued); GET_RESEARCH_STAT
int EvQueueGetNext(int kind);           // FF3 0x0024f400: the queued entry of the kind with the lowest order, -1: none
int EvQueueAdd(int kind, int idx);      // FF3 0x0024f460: queue last; returns 0
int EvQueueStart(int kind);             // FF3 0x0024f4e0: pick the next queued entry; returns 0
int EvQueueMain(void);                  // FF3 0x0024fa78: 0 while playing, -1 when none left
int EvQueueInit(void);                  // FF3 0x0024fab8: returns 0
void EvQueueReset(void);                // FF3 0x0024fb08
int EvQueueEnd(void);                   // FF3 0x0024fb20: returns 0

#endif /* _INGAME_EVENT_EV_QUEUE_H */
