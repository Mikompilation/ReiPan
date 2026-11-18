/* ==========================================================================
 *  ingame/enemy/enemy_act.h
 *
 *  The ghost behaviour-script interpreter (FF2 enemy_act.c; FF3
 *  0x18a338..0x1931e0). A ghost's actions are byte-code scripts in the ALG
 *  pak (ENE_ACT01_OBJ, loaded at EE 0x12ef000 by Boot_Load): EneActSet points
 *  the cursor (ENE_WRK.alg) at an action, EneAlgCtrl runs it a frame at a
 *  time; the blink script beside it (EneBlinkSet / EneBlinkCtrl) drives the
 *  alpha wobble and the visibility branches.
 * ======================================================================== */
#ifndef _INGAME_ENEMY_ENEMY_ACT_H
#define _INGAME_ENEMY_ENEMY_ACT_H

#include "sdk/scetypes.h"
#include "ingame/enemy/ene_main.h"      // ENE_WRK, ENEALG_WRK

void EneActSet(ENE_WRK *ene, u_char act_no);    // starts action act_no of the enemy's script row
void EneBlinkSet(ENE_WRK *ene);         // the blink script at the row's first action (SetCommonDat)
void EneAlgCtrl(ENE_WRK *ene);          // per frame (EneRule): the action script
void EneBlinkCtrl(ENE_WRK *ene);        // per frame (EneAlphaCtrl): the blink script
int  EneActReqById(u_char type, short id, u_char act);  // an event's action request; 0: the enemy does not act
void EneActRule(ENE_WRK *ene);          // per frame (EneRule): an ended action restarts, a hold wears down
void EneDamageHitHook(ENE_WRK *ene);    // empty (EneDamageApply, a hit that does not kill)

#endif /* _INGAME_ENEMY_ENEMY_ACT_H */
