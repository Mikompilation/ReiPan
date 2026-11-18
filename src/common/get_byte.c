/* ==========================================================================
 *  common/get_byte.c
 *
 *  Unaligned reads (see get_byte.h). FF3 0x1f0d78..0x1f0e08; FF2 ev_get.c's
 *  Get1Byte / Get2Byte / Get4Byte. FF3 copies through a 16-byte stack buffer
 *  (memcpy), so the reads are safe at any alignment.
 * ======================================================================== */
#include "common/get_byte.h"

#include <string.h>                                     // memcpy

// ──────────────────────────────────────────────────────────────────────
// FF3 0x001f0d78; FF2 Get1Byte (ev_get.c).

u_char Get1Byte(u_char *dat_addr)
{
    u_char buf[16];

    buf[0] = 0;
    memcpy(buf, dat_addr, 1);
    return buf[0];
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x001f0da8; FF2 Get2Byte (ev_get.c).

u_short Get2Byte(u_char *dat_addr)
{
    u_short buf[8];

    buf[0] = 0;
    memcpy(buf, dat_addr, 2);
    return buf[0];
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x001f0dd8; FF2 Get4Byte (ev_get.c).

u_int Get4Byte(u_char *dat_addr)
{
    u_int buf[4];

    buf[0] = 0;
    memcpy(buf, dat_addr, 4);
    return buf[0];
}
