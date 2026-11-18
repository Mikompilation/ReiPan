/* ==========================================================================
 *  system/pad/vib_pulse.h
 *
 *  The pulse: a beat of a sound effect (SE 0x1a9b0009) and pad 0's big
 *  motor, repeated with a gap, while a per-frame callback allows it. The
 *  peeping view (photo.c) runs it.
 *
 *  FF3-only (no FF2 match), FF3 0x1e75f0..0x1e7878, between
 *  system/os/sys_heap.c and common/spline.c; work bss 0x3ef150.
 * ======================================================================== */
#ifndef _SYSTEM_PAD_VIB_PULSE_H
#define _SYSTEM_PAD_VIB_PULSE_H

// Stop the pulse (clears the callback and the counters).
void VibPulseClear(void);                                           // FF3 0x001e75f0
// Per frame: one step of the beat while a callback is set and returns non-zero.
void VibPulseMain(void);                                            // FF3 0x001e7648
// The beat by the distance between p0 and p1: t = (d - dist0) / (dist1 - dist0),
// d clamped first to dist1 above, then to dist0 below; gap 90 * t frames,
// power 127 + 128 * (1 - t). Without both points a fixed gap 90 / power 0x7f.
// The points are kept and read every frame.
void VibPulseReqDist(float dist0, float dist1, float *p0, float *p1);  // FF3 0x001e7838

#endif /* _SYSTEM_PAD_VIB_PULSE_H */
