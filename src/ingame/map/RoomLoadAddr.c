// ingame/map/RoomLoadAddr.c -- FF3 0x1f73b8..0x1f74f8 (FF3-only): the room
// buffers' furniture load cursors.

#include "ingame/map/RoomLoadAddr.h"
#include "ingame/map/MapLoad.h"         // MapLoadGetFreeMemAddr

static ROOM_LOAD_ADDR room_load_addr[2];    // bss 0x3f16e8

// FF3 0x001f73b8.
static ROOM_LOAD_ADDR *RoomLoadAddrGet(int room)
{
    return &room_load_addr[room];
}

// The buffer is fetched and nothing is done with it (a check left empty in
// the ROM). FF3 0x001f73d0.
static int RoomLoadAddrCheck(int room)
{
    RoomLoadAddrGet(room);
    MapLoadGetFreeMemAddr(room);
    return 0;
}

// FF3 0x001f7400.
void *RoomLoadAddrReset(int room)
{
    ROOM_LOAD_ADDR *p = RoomLoadAddrGet(room);

    p->cur = p->base;
    return p->base;
}

// FF3 0x001f7428.
void RoomLoadAddrSetBase(int room, void *addr)
{
    RoomLoadAddrGet(room)->base = addr;
    RoomLoadAddrReset(room);
}

// FF3 0x001f7460.
void *RoomLoadAddrGetCur(int room)
{
    return RoomLoadAddrGet(room)->cur;
}

// FF3 0x001f7480.
void RoomLoadAddrSetCur(int room, void *addr)
{
    RoomLoadAddrGet(room)->cur = addr;
    RoomLoadAddrCheck(room);
}

// FF3 0x001f74b8.
void RoomLoadAddrAdd(int room, int size)
{
    ROOM_LOAD_ADDR *p = RoomLoadAddrGet(room);

    p->cur = (u_char *)p->cur + size;
    RoomLoadAddrCheck(room);
}
