/* ==========================================================================
 *  ingame/map/RoomLoadAddr.h
 *
 *  The furniture load cursor of each room buffer (FF3-only, 0x1f73b8..
 *  0x1f74f8; bss 0x3f16e8). RoomObjPackLoadReq sets the base to the address
 *  after the room's obj_pack; the furniture models and their motions load
 *  from the current address (MapDrawInitFurn, MapDrawLoadFurnAll), and
 *  MapDrawRoomObjRegist rewinds it to the base.
 * ======================================================================== */
#ifndef _INGAME_MAP_ROOMLOADADDR_H
#define _INGAME_MAP_ROOMLOADADDR_H

#include <stddef.h>                     // offsetof
#include "sdk/scetypes.h"

// layout: host -- two EE addresses (8 bytes) on the EE; host pointers here.
struct ROOM_LOAD_ADDR
{
    /* 0x00 */ void *base;
    /* 0x04 */ void *cur;
};
static_assert(offsetof(ROOM_LOAD_ADDR, base) == 0, "ROOM_LOAD_ADDR.base");
static_assert(offsetof(ROOM_LOAD_ADDR, cur) == sizeof(void *), "ROOM_LOAD_ADDR.cur (EE +0x04)");

void *RoomLoadAddrReset(int room);              // FF3 0x001f7400: cur = base; base
void  RoomLoadAddrSetBase(int room, void *addr);    // FF3 0x001f7428: base = addr, then reset
void *RoomLoadAddrGetCur(int room);             // FF3 0x001f7460
void  RoomLoadAddrSetCur(int room, void *addr);     // FF3 0x001f7480 (no callers)
void  RoomLoadAddrAdd(int room, int size);      // FF3 0x001f74b8: cur += size (no callers)

#endif /* _INGAME_MAP_ROOMLOADADDR_H */
