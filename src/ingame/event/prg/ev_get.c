/* ==========================================================================
 *  ingame/event/prg/ev_get.c
 *
 *  Event-file readers: FF3 0x19b158..0x19b4a8, FF2 ev_get.c (MioPan's port
 *  is the base; see ev_get.h). FF3 differs: the argument decoder is new,
 *  EvGetTblAddr / EvGetExeAddr lost their range checks and EvSetSubId its
 *  MAX OVER assert, and the event file sits at EE 0xf82000 (FF2 0xd4ec00).
 *  MioPan's null guards for a missing event file are not taken over: FF3
 *  has none, and EventDataLoad has the file in before EventMain runs.
 * ======================================================================== */
#include "ingame/event/prg/ev_get.h"

#include "common/get_byte.h"                    // Get1Byte, Get2Byte, Get4Byte
#include "common/vu0math.h"                     // RotLimitChk
#include "ingame/event/prg/ev_main.h"           // EVENT_DATA_ADDR
#include "reipan/reipan_memory.h"               // PC: ReiPan_GetHostPointer

// The event file's start. PC: the EE address is translated into the host's
// copy of EE memory, where the loader put the file.
#define EV_DATA_TOP     ((u_char *)ReiPan_GetHostPointer(EVENT_DATA_ADDR))

// End of the parent / sub-event id lists (tables 5 and 6).
#define EV_ID_END       0xffffffff

// ──────────────────────────────────────────────────────────────────────
// The dispatcher 0x19b5b8 passes the command table's format (row +0x0c)
// and the program address; EventCtrlCenterMain / EventConditionJudge the condition
// table's. Byte arguments are sign-extended like the others (lb-style
// sll/sra 24 on Get1Byte's result). FF3 0x0019b158.

int EvGetCmdArgs(int *args, u_char *cmd_addr, const char *fmt)
{
    u_char *addr;
    int     cnt;

    cnt = 0;
    if (fmt == nullptr) {
        return cnt;
    }

    addr = cmd_addr + (fmt[0] - '0');
    for (fmt++; *fmt != '\0'; fmt++, args++, cnt++) {
        switch (*fmt - '0') {
        case 1:
            *args = (s_char)Get1Byte(addr);
            addr += 1;
            break;
        case 2:
            *args = (short)Get2Byte(addr);
            addr += 2;
            break;
        case 4:
            *args = (int)Get4Byte(addr);
            addr += 4;
            break;
        default:                                // no read; the slot keeps its value
            break;
        }
    }

    return cnt;
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x0019b278; FF2 EvBinChangeAddr4. The stored value is an offset
// from top_addr, not a pointer.

u_char *EvBinChangeAddr4(u_char *top_addr, u_char *dat_addr)
{
    return top_addr + Get4Byte(dat_addr);
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x0019b2a8; FF2 EvGetTblAddr without the tbl_type < EV_TBL_MAX check.

u_char *EvGetTblAddr(int tbl_type)
{
    return EvBinChangeAddr4(EV_DATA_TOP, EV_DATA_TOP + tbl_type * 4);
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x0019b2d0; FF2 EvGetExeAddr without its range check and null test.

u_char *EvGetExeAddr(int tbl_type, int ev_no)
{
    return EvBinChangeAddr4(EV_DATA_TOP, EvGetTblAddr(tbl_type) + ev_no * 4);
}

// ──────────────────────────────────────────────────────────────────────
// The immediate parent: the list's last entry; the root's list is empty.
// FF3 0x0019b308; FF2 EvGetParentID.

int EvGetParentID(int event_id)
{
    int     parent_id;
    u_char *parent_addr;

    parent_id   = -1;
    parent_addr = EvGetExeAddr(EV_TBL_PARENT_ID, event_id);

    while (Get4Byte(parent_addr) != EV_ID_END) {
        parent_id    = (int)Get4Byte(parent_addr);
        parent_addr += 4;
    }

    return parent_id;
}

// ──────────────────────────────────────────────────────────────────────
// Every slot -1 first, so callers can walk all EV_SUB_ID_MAX of them. FF3
// dropped FF2's MAX OVER check: a longer list runs past id_tbl (none does
// in the shipped event files, or FF2's assert would have caught it).
// FF3 0x0019b380; FF2 EvSetSubId.

void EvSetSubId(u_char *dat_addr, int *id_tbl)
{
    int   i;
    u_int ev_id;

    for (i = EV_SUB_ID_MAX - 1; i >= 0; i--) {
        id_tbl[i] = -1;
    }

    ev_id     = Get4Byte(dat_addr);
    dat_addr += 4;
    while (ev_id != EV_ID_END) {
        *id_tbl++ = (int)ev_id;
        ev_id     = Get4Byte(dat_addr);
        dat_addr += 4;
    }
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x0019b410; FF2 EvGetSubId.

void EvGetSubId(int event_no, int *id_tbl)
{
    EvSetSubId(EvGetExeAddr(EV_TBL_SUB_ID, event_no), id_tbl);
}

// ──────────────────────────────────────────────────────────────────────
// Degrees as written in the event scripts to radians in [-PI, PI].
// FF3 0x0019b448 (constants 180.0f at 0x370150, PI at 0x370154); FF2
// EvGetRot360.

float EvGetRot360(short rot360)
{
    float rot;

    rot = ((float)rot360 / 180.0f) * 3.1415927f;
    RotLimitChk(&rot);
    return rot;
}
