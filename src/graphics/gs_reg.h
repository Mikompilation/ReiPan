/* ==========================================================================
 *  graphics/gs_reg.h
 *
 *  FF3's GS register cache and screen capture.
 *
 *    * Register cache  every GS register written through SetGsRegCached /
 *                      SetGsRegsCached is sent as an A+D packet and its value
 *                      kept, so code can read the current state back
 *                      (GetGsRegCached) and restore it after a change.
 *    * Screen capture  ScreenCaptureSave keeps a copy of the displayed frame in
 *                      EE memory (0x10ef000); ScreenCaptureDisp re-uploads it
 *                      to VRAM 0x2bc0 and draws it full screen behind menus
 *                      (FF3's shared form of FF2's per-menu capture code).
 *
 *  FF3-only module (no FF2 counterpart); FF3 extent 0x1e6b98..0x1e7560.
 * ======================================================================== */
#ifndef _GRAPHICS_GS_REG_H
#define _GRAPHICS_GS_REG_H

#include "sdk/scetypes.h"

// One A+D register write: the data and the GS register address
// (the layout of SetGsRegsCached's array and of the packet it builds).
struct GS_REG_SET                   /* 0x10 */
{
    /* 0x00 */ u_long data;
    /* 0x08 */ u_long reg;
};
static_assert(sizeof(GS_REG_SET) == 0x10, "GS_REG_SET is one GIF qword");

// SetGsRegCached / SetGsRegsCached mode: -1 sends nothing and leaves the
// cache alone; every FF3 call site that passes a constant passes 1.
#define GS_REG_SEND_NONE    (-1)

void    InitGsRegCache(void);
int     SetGsRegCached(u_long data, int reg, int mode);
int     SetGsRegsCached(const GS_REG_SET *regs, int num, int mode);
void    SetDefaultGsRegs(void);
u_long *GetGsRegCached(int reg);
u_long *GetGsRegCache(void);

// Capture types (ScreenCaptureSave's type, the LocalCopy geometry it uses):
// 0 full 640x448, 1 shrunk to 320x224, 2 640x448 halved on the EE to 320x448,
// 3 none taken (ScreenCaptureDisp still draws). -1 before the first capture.
void    ScreenCaptureSave(int type, int blend);
void    ScreenCaptureDisp(u_char r, u_char g, u_char b, u_char alpha);

// Unreferenced in FF3 (no call, jump or pointer reaches it); kept for completeness.
void    DrawTexQuadStrip3D(sceVu0FVECTOR *vtx, int num, int r, int g, int b, int a,
                           u_long tex0, int zofs, float u, float v, float du, float dv);

#endif /* _GRAPHICS_GS_REG_H */
