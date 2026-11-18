/* ==========================================================================
 *  system/eeiop/spu_voice.c
 *
 *  SPU2 voice allocator: a bit mask per core.
 *
 *  From MioPan system/eeiop/spu_voice.c (FF2's); FF3 drops FreeSPUVoiceCore's
 *  assert on voice -1 but still ignores it. FF3 0x2711f0..0x2712a0.
 * ======================================================================== */
#include "system/eeiop/spu_voice.h"

static int core_voices[2];              /* sdata 0x372820 (gp-0xcd0) */

// ──────────────────────────────────────────────────────────────────────────
// FF3 0x002711f0; FF2 InitSPUVoice.

void InitSPUVoice(void)
{
    core_voices[0] = 0;
    core_voices[1] = 0;
}

// FF3 0x00271200; FF2 InitSPUVoiceCore.

void InitSPUVoiceCore(int core)
{
    core_voices[core] = 0;
}

// ──────────────────────────────────────────────────────────────────────────
// FF3 0x00271218; FF2 GetSPUVoiceCore.

int GetSPUVoiceCore(int core)
{
    for (int i = 0; i < SPU_VOICE_MAX; i++)
    {
        if (((core_voices[core] >> i) & 1) == 0)
        {
            core_voices[core] |= 1 << i;
            return i;
        }
    }

    return -1;
}

// ──────────────────────────────────────────────────────────────────────────
// -1 is GetSPUVoiceCore's failure value; freeing it does nothing.
// FF3 0x00271268; FF2 FreeSPUVoiceCore without the assert.

void FreeSPUVoiceCore(int core, int voice_no)
{
    if (voice_no == -1)
    {
        return;
    }

    core_voices[core] &= ~(1 << voice_no);
}
