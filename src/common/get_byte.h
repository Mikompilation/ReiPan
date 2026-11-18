/* ==========================================================================
 *  common/get_byte.h
 *
 *  Unaligned little-endian reads from byte data (messages, event and file
 *  tables). FF3 0x1f0d78..0x1f0e08, right after system/pad/pad.c; FF2 kept
 *  them in ingame/event/prg/ev_get.c (FF3's ev_get.c at 0x19b158 no longer
 *  has them). File name is descriptive.
 * ======================================================================== */
#ifndef _COMMON_GET_BYTE_H
#define _COMMON_GET_BYTE_H

#include "sdk/scetypes.h"

u_char  Get1Byte(u_char *dat_addr);
u_short Get2Byte(u_char *dat_addr);
u_int   Get4Byte(u_char *dat_addr);

#endif /* _COMMON_GET_BYTE_H */
