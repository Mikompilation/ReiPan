/* ==========================================================================
 *  system/eeiop/spu_voice.h
 *
 *  SPU2 voice allocator (FF2 spu_voice.c; FF3 0x2711f0..0x2712a0): one bit
 *  per voice and core. The IOP drives the hardware; the EE only keeps track
 *  of which voice numbers are taken.
 * ======================================================================== */
#ifndef _SYSTEM_EEIOP_SPU_VOICE_H
#define _SYSTEM_EEIOP_SPU_VOICE_H

#define SPU_VOICE_MAX   24              // voices per SPU2 core

void InitSPUVoice(void);                        // 0x2711f0: both cores free
void InitSPUVoiceCore(int core);                // 0x271200: unreferenced
int  GetSPUVoiceCore(int core);                 // 0x271218: lowest free voice, taken; -1 when none
void FreeSPUVoiceCore(int core, int voice_no);  // 0x271268: voice_no -1 is ignored

#endif /* _SYSTEM_EEIOP_SPU_VOICE_H */
