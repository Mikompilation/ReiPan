/* ==========================================================================
 *  reipan/reipan_debug.h
 *
 *  PC debug toggles the game code reads and the ImGui "Debug" menu sets
 *  (reipan_ui.cpp owns the state). None of these exist in FF3.
 * ======================================================================== */
#ifndef REIPAN_DEBUG_H
#define REIPAN_DEBUG_H

// Draw the player's light cone, its reach, a box and the facing with FF3's
// uncalled debug primitives (effect_sub.c ReiPan_EffDebugOverlay).
int  ReiPan_DebugLightConeEnabled(void);
void ReiPan_DebugSetLightCone(int enabled);

#endif /* REIPAN_DEBUG_H */
