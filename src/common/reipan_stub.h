/* ==========================================================================
 *  common/reipan_stub.h
 *
 *  Marks an FF3 function that is called by reconstructed code but not written
 *  yet. The body logs once (name + FF3 address) and returns, so the build links
 *  and a run shows exactly which missing pieces were reached.
 *
 *  tools/reverse/stubgen.py writes these; tools/reverse/status.py counts them.
 *  Replace the whole definition when the function is reconstructed.
 * ======================================================================== */
#ifndef _COMMON_REIPAN_STUB_H
#define _COMMON_REIPAN_STUB_H

#include <stdio.h>

#define REIPAN_STUB(name, ff3_addr)                                              \
    do {                                                                         \
        static int reipan_stub_seen_;                                            \
        if (!reipan_stub_seen_) {                                                \
            reipan_stub_seen_ = 1;                                               \
            printf("[stub] %s (FF3 0x%08x) reached\n", name, (unsigned)(ff3_addr)); \
        }                                                                        \
    } while (0)

/* A temporary stand-in that works but is not FF3's code: built from FF2 /
 * MioPan code so later TUs can run (src/lib/snd_bridge.c). The tools count
 * it exactly like a stub -- still to reconstruct, listed for deletion in its
 * TU's brief, ranked by smoke-run hits -- and the log line says so. */
#define REIPAN_BRIDGE(name, ff3_addr)                                            \
    do {                                                                         \
        static int reipan_stub_seen_;                                            \
        if (!reipan_stub_seen_) {                                                \
            reipan_stub_seen_ = 1;                                               \
            printf("[stub] %s (FF3 0x%08x) reached (FF2 bridge)\n", name,        \
                   (unsigned)(ff3_addr));                                        \
        }                                                                        \
    } while (0)

#endif /* _COMMON_REIPAN_STUB_H */
