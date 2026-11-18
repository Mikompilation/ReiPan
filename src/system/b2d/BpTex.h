/* ==========================================================================
 *  system/b2d/BpTex.h
 *
 *  FF3's Bp texture table (FF3 0x1089e8..0x108f78, assert string
 *  "BpTex.c"; no FF2 counterpart). Each Bp layout handle that came from a
 *  pack gets a work listing the pack's .tm2 files: B2dFile's texture-load
 *  hook registers them (BpTexLoadFunc), the B2d core's texture-set hook
 *  sends one to VRAM before its sprites draw (BpTexSetFunc). All of a
 *  handle's textures share one VRAM spot: the clut at `tbp`, the image at
 *  `tbp + 0x82`; BpCtl gives every handle 0x2bc0.
 * ======================================================================== */
#ifndef _SYSTEM_B2D_BPTEX_H
#define _SYSTEM_B2D_BPTEX_H

#include <stddef.h>                     // offsetof

#include "sdk/scetypes.h"
#include "system/b2d/B2dCtl.h"          // B2D_HOOK_ARGS

#define BP_TEX_MAX  45                  // slots (handles), table at bss 0x377e58

// One texture of a handle's pack (0x18 bytes).
// layout: host -- tim2 is a pointer; with the u_long before it the x64 size
// and offsets match the EE's (the EE pads 4 bytes after tim2).
struct BP_TEX_ENT
{
    /* 0x00 */ int     id;              // the B2d texture number; -1 free
    /* 0x04 */ int     tbp;             // VRAM block of the clut (image at +0x82)
    /* 0x08 */ u_long  tex0;            // GS TEX0 for this texture at tbp
    /* 0x10 */ void   *tim2;            // the .tm2 file inside the loaded pack
};
static_assert(sizeof(BP_TEX_ENT) == 0x18, "BP_TEX_ENT is 0x18 bytes");
static_assert(offsetof(BP_TEX_ENT, tbp) == 0x04, "BP_TEX_ENT.tbp");
static_assert(offsetof(BP_TEX_ENT, tex0) == 0x08, "BP_TEX_ENT.tex0");
static_assert(offsetof(BP_TEX_ENT, tim2) == 0x10, "BP_TEX_ENT.tim2");

// A handle's work: this header and `num` entries (BpTexGetWorkSize; FF3
// allocates one entry more than it uses).
struct BP_TEX_WORK
{
    /* 0x00 */ int        handle;
    /* 0x04 */ int        clut_tbp;     // where BpTexSetClut sends a replacement clut
    /* 0x08 */ int        tbp;          // given to each new entry
    /* 0x0c */ int        num;
    /* 0x10 */ BP_TEX_ENT ent[1];       // [num]
};
static_assert(sizeof(BP_TEX_WORK) == 0x28, "BP_TEX_WORK is 0x28 bytes");
static_assert(offsetof(BP_TEX_WORK, clut_tbp) == 0x04, "BP_TEX_WORK.clut_tbp");
static_assert(offsetof(BP_TEX_WORK, tbp) == 0x08, "BP_TEX_WORK.tbp");
static_assert(offsetof(BP_TEX_WORK, num) == 0x0c, "BP_TEX_WORK.num");
static_assert(offsetof(BP_TEX_WORK, ent) == 0x10, "BP_TEX_WORK.ent");

int     BpTexGetWorkSize(int num);                          // FF3 0x00108b48
u_long  BpTexGetTex0(int handle, int id);                   // FF3 0x00108b60
void   *BpTexGetTim2(int handle, int id);                   // FF3 0x00108b90 (BpTexGetPicture)
void    BpTexSetClut(int handle, int id, void *tim2);       // FF3 0x00108bc0
int     BpTexLoadFunc(const B2D_HOOK_ARGS *args);           // FF3 0x00108cf0, B2dFile's texture-load hook
int     BpTexSetFunc(const B2D_HOOK_ARGS *args);            // FF3 0x00108e30, the B2d texture-set hook
int     BpTexSetup(int handle, int tbp, void *work, int num);   // FF3 0x00108e68 (BpCtlLoadTex)
int     BpTexRelease(int handle);                           // FF3 0x00108ef8 (BpCtlTerm)
int     BpTexInit(void);                                    // FF3 0x00108f48 (B2dSystemInit)

// PC: the TEX0 the last Bp texture packet set (BpCtl's renderer quads).
u_long  BpTexGetHostTex0(void);

#endif /* _SYSTEM_B2D_BPTEX_H */
