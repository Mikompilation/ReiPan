/* ==========================================================================
 *  ingame/map/foot_se.h
 *
 *  Footstep sounds (FF2 foot_se.c, FF3 0x001c7cb8..0x001c83d0). Twelve slots
 *  each hold one SndBank of a FOOT_nn file; on every room load the slots are
 *  re-cut to the footstep files both loaded rooms use, and the animation code
 *  plays one when a foot lands.
 *
 *  FF3 against FF2: the surface lists are no longer a table in the ELF but
 *  the se_ sound numbers of each room's rcb (RcbGetSeNoList), n -> file
 *  FOOT_00_0_DOMA_BD_BD + 2n (header file + 1). FootSeInit records which of
 *  the 51 files exist on the disc; the surface under the foot is looked up by
 *  FootSePlayFloor, and foot_sePlay takes the file number.
 * ======================================================================== */
#ifndef _INGAME_MAP_FOOT_SE_H
#define _INGAME_MAP_FOOT_SE_H

void FootSeInit(void);                  // FF3 0x001c7e48: slots free + which foot SE files exist
void foot_seRelease(void);              // FF3 0x001c7f58: every slot's bank released
void foot_seSetRoom(void);              // FF3 0x001c7e08: the footstep SE files of both loaded rooms
void foot_seSetNewFiles(const int *file_array, int file_num);    // FF3 0x001c7fa0: re-cut the slots to file_array
void foot_sePlay(int file_no, float *pos, int vol, int pitch);  // FF3 0x001c8120: one step of the loaded file_no at pos
void FootSePlayId(int se, float *pos, int vol, int pitch);       // FF3 0x001c8260: foot_sePlay(se, ...)
void FootSePlayFloor(float *pos, int mh_no, int vol, int pitch); // FF3 0x001c8278: the footstep of the floor at pos (map hit mh_no; the door room's while a door moves)

#endif /* _INGAME_MAP_FOOT_SE_H */
