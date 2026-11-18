// ingame/map/MarkEvent.c -- CMarkEvent, the 2D map draw flags, the map's
// event-mark checks and LERP_F.
// FF3 0x0010f820..0x0010fb80 (+ the destructors at 0x002abfe0..0x002ac010);
// FF3-only (the class file sorts between MarkDoor and ProcCtl; open-items.md
// "0x10bbb0..0x116fa0").
#include "ingame/map/MarkEvent.h"

#include "ingame/ingame_wrk.h"          // ingame_wrk (b03 the world, plyr_wrk)
#include "ingame/map/MapLoad.h"         // MapLoadGetRoomNoNow
#include "ingame/story_menu_map.h"       // StoryMapGetScale
#include "ingame/plyr/player.h"         // PLYR_WRK.mh_no

// ──────────────────────────────────────────────────────────────────────
// CMarkEvent
// ──────────────────────────────────────────────────────────────────────

// FF3 0x0010f848.

CMarkEvent::CMarkEvent()
{
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x002abfe0 (the deleting one: 0x002abff0).

CMarkEvent::~CMarkEvent()
{
}

// ──────────────────────────────────────────────────────────────────────
// The 2D map draw flags
// ──────────────────────────────────────────────────────────────────────

// One SET_2DMAP_DRAW id, data 0x2efae8 (6 rows): the map item it shows or
// hides, by floor (world 1's floors are 3..5), layer and item id.
struct MARK_EVENT_ROW                   /* 0x10 */
{
    /* 0x00 */ int id;                  // SET_2DMAP_DRAW's id
    /* 0x04 */ int floor;
    /* 0x08 */ int layer;
    /* 0x0c */ int item;
};
static_assert(sizeof(MARK_EVENT_ROW) == 0x10, "MARK_EVENT_ROW is 0x10 bytes");

#define MARK_EVENT_FLAG_NUM     16
#define MARK_EVENT_ROWS         6

static const MARK_EVENT_ROW mark_event_row[MARK_EVENT_ROWS] =
{
    { 0, 3, 1, 0x3f2 },
    { 1, 4, 0, 0x3ea },
    { 2, 2, 0, 0x406 },
    { 3, 2, 0, 0x401 },
    { 4, 2, 0, 0x40a },
    { 5, 2, 0, 0x407 },
};

static u_char mark_event_flag[MARK_EVENT_FLAG_NUM];     // bss 0x379a38

// ──────────────────────────────────────────────────────────────────────
// Save block: the flag bytes. FF3 0x0010f820.

int MarkEventFlagSaveGet(void **addr)
{
    *addr = mark_event_flag;
    return sizeof(mark_event_flag);
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x0010f858.

void MarkEventFlagClear(void)
{
    for (int i = 0; i < MARK_EVENT_FLAG_NUM; i++)
        mark_event_flag[i] = 0;
}

// ──────────────────────────────────────────────────────────────────────
// SET_2DMAP_DRAW: the flag of the row whose id matches; unknown ids do
// nothing. FF3 0x0010f890.

void MarkEventFlagSet(int id, u_char value)
{
    for (int i = 0; i < MARK_EVENT_ROWS; i++)
    {
        if (mark_event_row[i].id == id)
        {
            mark_event_flag[i] = value;
            return;
        }
    }
}

// ──────────────────────────────────────────────────────────────────────
// The map sprite hook's checks (FF3 0x001f7978, m_plyr_camera.c)
// ──────────────────────────────────────────────────────────────────────

// Area 0x2a with the player standing in map hit 2..9.
// FF3 0x0010f8d8 (MapSnap tests it too).

int MarkEventIsSpecialRoom(void)
{
    if (MapLoadGetRoomNoNow() != 0x2a)
        return 0;
    return (u_int)(ingame_wrk.plyr_wrk->mh_no - 2) < 8;
}

// ──────────────────────────────────────────────────────────────────────
// Layer 4 in the special room: item 0x3f2 on floor 0 becomes the bobbing
// sprite at 0xb9, 0xce, item 0x3ee on floor 1 the one at 0xa4, 0x3b
// (0x2e x 0x2f both). x is not touched. 1 when the words were replaced.
// FF3 0x0010f918.

int MarkEventSpecialSprite(int floor, int layer, int id, int *x, int *y,
                           int *u, int *v, int *w, int *h)
{
    (void)x;

    if (layer != 4 || !MarkEventIsSpecialRoom())
        return 0;

    if (floor == 0 && id == 0x3f2)
    {
        *u = 0xb9;
        *v = 0xce;
        *w = 0x2e;
        *h = 0x2f;
        *y += StoryMapGetScale() * 10 / 1024;
        return 1;
    }
    if (floor == 1 && id == 0x3ee)
    {
        *u = 0xa4;
        *v = 0x3b;
        *w = 0x2e;
        *h = 0x2f;
        return 1;
    }
    return 0;
}

// ──────────────────────────────────────────────────────────────────────
// Whether a map item is drawn: the SET_2DMAP_DRAW flag of its row (floor
// + 3 in world 1), 1 for items without a row.
// FF3 0x0010fa40.

int MarkEventIsDrawn(int floor, int layer, int id)
{
    if (ingame_wrk.b03 == 1)
        floor += 3;

    for (int i = 0; i < MARK_EVENT_ROWS; i++)
    {
        const MARK_EVENT_ROW *row = &mark_event_row[i];

        if (row->floor == floor && row->layer == layer && row->item == id)
            return (s_char)mark_event_flag[i];
    }
    return 1;
}

// ──────────────────────────────────────────────────────────────────────
// LERP_F (written with common/cmn_fade_layer.c, which embeds one)
// ──────────────────────────────────────────────────────────────────────

// FF3 0x0010fac8 (LerpFClear; FF3 0x0010fab0 is the same body, unreferenced:
// a constructor pair).

LERP_F::LERP_F()
{
    time   = 0.0f;
    from   = 0.0f;
    to     = 0.0f;
    cur    = 0.0f;
    frames = 0.0f;
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x0010fae0 (LerpFSet): jump to 'from' and head for 'to'.

void LERP_F::Set(float from, float to, float frames)
{
    time         = 0.0f;
    cur          = from;
    this->to     = to;
    this->frames = frames;
    this->from   = from;
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x0010faf8 (LerpFStart): head for 'to' from the current value.

void LERP_F::Start(float to, float frames)
{
    time         = 0.0f;
    from         = cur;
    this->to     = to;
    this->frames = frames;
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x0010fb10 (LerpFUpdate): one frame (step 1.0, data 0x36b6d4);
// returns 1 once 'to' is reached.

int LERP_F::Update()
{
    if (time < frames)
    {
        float from = this->from;

        time += 1.0f;
        if (to == from)
            cur = from;
        else
            cur = (to - from) * (time / frames) + from;
        return 0;
    }

    cur = to;
    return 1;
}
