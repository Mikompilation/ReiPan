/* ==========================================================================
 *  ingame/event/prg/ev_get.h
 *
 *  Event-file readers (FF3 0x19b158..0x19b4a8; FF2 ev_get.c, MioPan's port
 *  is the base): the argument decoder every event command and condition goes
 *  through, and the lookups into the event file (EVENT_OBJ and its variants)
 *  that EventDataLoad puts at EVENT_DATA_ADDR (EE 0xf82000).
 *
 *  Event file layout (FF2's, unchanged):
 *
 *      +0x00                u_int tbl_off[EV_TBL_MAX]
 *      tbl_off[t] + ev*4    u_int stream_off          one per event id
 *
 *  Every stored word is a byte offset from the file start, which is what
 *  EvBinChangeAddr4 adds it to. Tables 5 and 6 hold -1 terminated id lists.
 *
 *  FF3 moved FF2's Get1Byte / Get2Byte / Get4Byte to common/get_byte.c and
 *  GetEvState / GetEvWrkWaitFlg into ev_change.c, dropped the range checks
 *  and asserts, and added the argument decoder.
 * ======================================================================== */
#ifndef _INGAME_EVENT_PRG_EV_GET_H
#define _INGAME_EVENT_PRG_EV_GET_H

#include "sdk/scetypes.h"

// The table EvGetTblAddr / EvGetExeAddr index (FF2 ev_get.h's numbers).
#define EV_TBL_OPEN_COND    0           // open condition
#define EV_TBL_INIT_PRG     1           // init program (SetEventInitStatus)
#define EV_TBL_EXE_PRG      2           // main program (SetEventExeStatus)
#define EV_TBL_CLOSE_COND   3           // close condition
#define EV_TBL_END_PRG      4           // end program (SetEventEndExeStatus)
#define EV_TBL_PARENT_ID    5           // parent id list (EvGetParentID)
#define EV_TBL_SUB_ID       6           // sub-event id list (EvGetSubId)
#define EV_TBL_MAX          7

#define EV_SUB_ID_MAX       50          // slots EvSetSubId fills

// Decodes the arguments of the command or condition at cmd_addr by its
// table format string (ingame/event/prg/ev_command.h): fmt[0] - '0' bytes
// to skip, then per digit one signed 1 / 2 / 4-byte value into args[i]
// (another digit leaves its slot alone). Returns the digit count.
int     EvGetCmdArgs(int *args, u_char *cmd_addr, const char *fmt);    // FF3 0x0019b158

u_char *EvBinChangeAddr4(u_char *top_addr, u_char *dat_addr);   // FF3 0x0019b278: top_addr + the word at dat_addr
u_char *EvGetTblAddr(int tbl_type);                 // FF3 0x0019b2a8: start of table tbl_type (no range check)
u_char *EvGetExeAddr(int tbl_type, int ev_no);      // FF3 0x0019b2d0: event ev_no's entry in table tbl_type
int     EvGetParentID(int event_id);                // FF3 0x0019b308: the last id of the parent list, -1 when empty
void    EvSetSubId(u_char *dat_addr, int *id_tbl);  // FF3 0x0019b380: id_tbl[EV_SUB_ID_MAX] from a -1 terminated list
void    EvGetSubId(int event_no, int *id_tbl);      // FF3 0x0019b410: event_no's sub-event ids, -1 unused
float   EvGetRot360(short rot360);                  // FF3 0x0019b448: degrees to radians, wrapped (RotLimitChk)

#endif /* _INGAME_EVENT_PRG_EV_GET_H */
