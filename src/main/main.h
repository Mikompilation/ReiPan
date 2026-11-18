/* ==========================================================================
 *  main/main.h
 *
 *  The GPhase root: GameSystemStart/Frame/End (one game session: phase tree
 *  set-up, one phase-engine step per frame, tear-down) and the super phase
 *  (phase 0, root of the 120-phase tree; its functions sit in the phase
 *  tables at 0x3275c0.. as entry 0).
 *  FF3 0x1d2610..0x1d2a40; main() itself lives in system/os/system.c.
 * ======================================================================== */
#ifndef _MAIN_MAIN_H
#define _MAIN_MAIN_H

void GameSystemStart(void);
void GameSystemFrame(void);
void GameSystemEnd(void);

// Phase 0, entry 0 of the phase tables (main/gphase.c): GPHASE_FUNC for
// init/one/end, GPHASE_SUB_FUNC for pre/after.
int  init_super(void);
void pre_super(void);
int  one_super(void);
void after_super(void);
int  end_super(void);

#endif /* _MAIN_MAIN_H */
