// system/b2d/BpTex.c -- FF3's Bp texture table (FF3 0x1089e8..0x108f78,
// assert string "BpTex.c"). No FF2 counterpart: written from the FF3
// decompilation. See BpTex.h for the work and how BpCtl uses it.

#include <stdio.h>                      // sprintf
#include <string.h>

#include "system/b2d/BpTex.h"
#include "system/b2d/B2dCtl.h"          // B2dErrPrint
#include "system/b2d/BpCtl.h"           // BpGetLoadPack
#include "graphics/graph2d/g2d_draw.h"  // StartDmaDirectTrans, EndDmaDirectTrans
#include "graphics/graph2d/tim2.h"      // Tim2GetFirstPicture, MakeClutDirect, MakeTim2SendPacket_3Dpkt
#include "ingame/map/MapPhf.h"          // PhfFindFileData
#include "sdk/libgraph.h"               // SCE_GS_TEXFLUSH, SCE_GS_TEX0_1

static const char bp_tex_file[] = "BpTex.c";    /* sdata 0x36b5e8 */

static BP_TEX_WORK *bp_tex_work[BP_TEX_MAX];    /* bss 0x377e58 */

// PC: the TEX0 the last BpTexPutTex0 sent, for BpCtl's renderer quads.
static u_long bp_tex0_host;

// TEX0 fields BpTexLoadFunc rewrites (GS manual bit positions).
#define BP_TEX0_TBP0_MASK   0x0000000000003fffULL       // bits 0..13
#define BP_TEX0_TBW_SHIFT   14                          // bits 14..19
#define BP_TEX0_TBW_MASK    0x00000000000fc000ULL
#define BP_TEX0_TW_SHIFT    26                          // bits 26..29
#define BP_TEX0_TCC         (1ULL << 34)
#define BP_TEX0_CBP_SHIFT   37                          // bits 37..50
#define BP_TEX0_CBP_MASK    (0x3fffULL << 37)
#define BP_TEX0_CLD_MASK    (7ULL << 61)                // bits 61..63
#define BP_TEX0_CLD_LOAD    (1ULL << 61)                // CLD 1: load the clut

#define BP_TEX_IMAGE_OFS    0x82        // image block after the clut block

// ──────────────────────────────────────────────────────────────────────
// TEXFLUSH, then TEX0_1 = tex0, as one direct packet (FF3 inlines it in
// BpTexSetClut and BpTexSendTim2).

static void BpTexPutTex0(u_long tex0)
{
    bp_tex0_host = tex0;                // PC: BpCtl's renderer quads

    u_long *p = (u_long *)StartDmaDirectTrans();

    p[0] = 0x2000000000008001;          // GIF tag: NLOOP 1, EOP, PACKED, NREG 2
    p[1] = 0xeee;                       // A+D (x3; NREG 2 uses two)
    p[2] = 0;
    p[3] = SCE_GS_TEXFLUSH;
    p[4] = tex0;
    p[5] = SCE_GS_TEX0_1;
    EndDmaDirectTrans((qword *)&p[6]);
}

// ──────────────────────────────────────────────────────────────────────
// PC: the TEX0_1 the Bp packets left on the GS (no FF3 counterpart). BpCtl's
// draw hooks texture their renderer quads with it.

u_long BpTexGetHostTex0(void)
{
    return bp_tex0_host;
}

// ──────────────────────────────────────────────────────────────────────
// The work of `handle`, null (and an error print) when the handle is out of
// range or has none. FF3 0x001089e8.

static BP_TEX_WORK *BpTexGetWork(int handle)
{
    int code = handle;
    int line;

    if ((u_int)handle < BP_TEX_MAX)
    {
        if (bp_tex_work[handle] != nullptr)
            return bp_tex_work[handle];
        code = -2;
        line = 0x3c;
    }
    else
    {
        line = 0x39;
    }
    B2dErrPrint(code, bp_tex_file, line, 0);
    return nullptr;
}

// ──────────────────────────────────────────────────────────────────────
// Takes the first free entry for texture `id`. FF3 0x00108a58.

static BP_TEX_ENT *BpTexNewEntry(BP_TEX_WORK *work, int id)
{
    for (int i = 0; i < work->num; i++)
    {
        BP_TEX_ENT *ent = &work->ent[i];

        if (ent->id < 0)
        {
            ent->id = id;
            return ent;
        }
    }
    B2dErrPrint(-1, bp_tex_file, 0x51, 0);
    return nullptr;
}

// ──────────────────────────────────────────────────────────────────────
// The entry of texture `id`; the entries are filled in order, so a free one
// ends the search. FF3 0x00108ac0.

static BP_TEX_ENT *BpTexFindEntry(int handle, int id)
{
    BP_TEX_WORK *work = BpTexGetWork(handle);

    if (work == nullptr)
        return nullptr;

    for (int i = 0; i < work->num; )
    {
        BP_TEX_ENT *ent = &work->ent[i];

        if (ent->id == id)
            return ent;
        i++;
        if (ent->id < 0)
        {
            B2dErrPrint(-2, bp_tex_file, 0x65, 0);
            return nullptr;
        }
    }
    B2dErrPrint(-1, bp_tex_file, 0x68, 0);
    return nullptr;
}

// ──────────────────────────────────────────────────────────────────────
// Bytes of work for a pack of `num` files. FF3 0x00108b48.

int BpTexGetWorkSize(int num)
{
    return num * (int)sizeof(BP_TEX_ENT) + (int)sizeof(BP_TEX_WORK);
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x00108b60.

u_long BpTexGetTex0(int handle, int id)
{
    BP_TEX_ENT *ent = BpTexFindEntry(handle, id);

    if (ent == nullptr)
        return 0;
    return ent->tex0;
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x00108b90.

void *BpTexGetTim2(int handle, int id)
{
    BP_TEX_ENT *ent = BpTexFindEntry(handle, id);

    if (ent == nullptr)
        return nullptr;
    return ent->tim2;
}

// ──────────────────────────────────────────────────────────────────────
// Draws texture `id` with the clut of `tim2` instead of its own: sends that
// clut to the handle's clut block and sets the texture's TEX0. Neither
// lookup is checked, as in FF3. FF3 0x00108bc0.
// callcheck: StartDmaDirectTrans, EndDmaDirectTrans (FF3 inlines the packet; BpTexPutTex0 here)

void BpTexSetClut(int handle, int id, void *tim2)
{
    MakeClutDirect(tim2, 0, BpTexGetWork(handle)->clut_tbp);
    BpTexPutTex0(BpTexFindEntry(handle, id)->tex0);
}

// ──────────────────────────────────────────────────────────────────────
// Sends a texture's image and clut to the VRAM its TEX0 names and sets that
// TEX0; returns it. `tbp` is not used (FF3 passes the entry's). FF3 0x00108c60.
// callcheck: StartDmaDirectTrans, EndDmaDirectTrans (FF3 inlines the packet; BpTexPutTex0 here)

static u_long BpTexSendTim2(void *tim2, int tbp)
{
    TIM2_PICTUREHEADER *pic = Tim2GetFirstPicture(tim2);

    (void)tbp;
    MakeTim2SendPacket_3Dpkt(tim2, 0, -1, -1);
    BpTexPutTex0(pic->GsTex0);
    return pic->GsTex0;
}

// ──────────────────────────────────────────────────────────────────────
// B2dFile's texture-load hook: a0 handle, a1 texture number, a2 file name
// (without ".tm2"). Finds "<a2>.tm2" in the pack being registered, gives it
// an entry and points its TEX0 at the handle's VRAM: clut at tbp, image at
// tbp + 0x82, buffer width from the texture width, clut loaded on use.
// FF3 0x00108cf0.

int BpTexLoadFunc(const B2D_HOOK_ARGS *args)
{
    BP_TEX_WORK *work = BpTexGetWork((int)args->a0);

    if (work == nullptr)
        return -1;

    BP_TEX_ENT *ent = BpTexNewEntry(work, (int)args->a1);
    char        name[0x100];

    sprintf(name, "%s.tm2", (const char *)args->a2);   /* sdata 0x36b5f0 */
    name[0x17] = '\0';                  // 23 characters at most

    void *tim2 = PhfFindFileData(BpGetLoadPack(), name);

    if (ent == nullptr || tim2 == nullptr)
    {
        // PC: FF3 writes through a null entry / reads a null TIM2 here (the
        // retail packs always hold the file); report it instead of crashing.
        printf("BpTexLoadFunc: no entry or no %s\n", name);
        return 0;
    }
    ent->tim2 = tim2;
    ent->tbp  = work->tbp;

    TIM2_PICTUREHEADER *pic = Tim2GetFirstPicture(tim2);
    u_long              tex0 = pic->GsTex0;

    tex0 = (tex0 & ~BP_TEX0_TBP0_MASK) | ((u_long)(ent->tbp + BP_TEX_IMAGE_OFS) & 0x3fff);
    tex0 = (tex0 & ~BP_TEX0_CBP_MASK) | (((u_long)ent->tbp & 0x3fff) << BP_TEX0_CBP_SHIFT);
    tex0 &= ~BP_TEX0_TBW_MASK;

    int w = 1 << (int)((tex0 >> BP_TEX0_TW_SHIFT) & 0xf);

    tex0 |= (u_long)((w / 64) & 0x3f) << BP_TEX0_TBW_SHIFT;
    tex0 |= BP_TEX0_TCC;
    tex0 = (tex0 & ~BP_TEX0_CLD_MASK) | BP_TEX0_CLD_LOAD;

    pic->GsTex0 = tex0;
    ent->tex0   = tex0;
    return 0;
}

// ──────────────────────────────────────────────────────────────────────
// The B2d core's texture-set hook: a0 handle, a1 texture number. Sends the
// texture; returns a1, the new current texture. FF3 0x00108e30.

int BpTexSetFunc(const B2D_HOOK_ARGS *args)
{
    BP_TEX_ENT *ent = BpTexFindEntry((int)args->a0, (int)args->a1);

    // PC: FF3 does not check (a missing texture reads through null).
    if (ent == nullptr || ent->tim2 == nullptr)
        return (int)args->a1;
    BpTexSendTim2(ent->tim2, ent->tbp);
    return (int)args->a1;
}

// ──────────────────────────────────────────────────────────────────────
// Gives `handle` the work for `num` textures at VRAM block `tbp`.
// FF3 0x00108e68.

int BpTexSetup(int handle, int tbp, void *work, int num)
{
    if ((u_int)handle >= BP_TEX_MAX)
    {
        B2dErrPrint(-1, bp_tex_file, 299, 0);
        return -1;
    }

    BP_TEX_WORK *wk = (BP_TEX_WORK *)work;

    bp_tex_work[handle] = wk;
    wk->handle   = handle;
    wk->tbp      = tbp;
    wk->clut_tbp = tbp;
    wk->num      = num;
    for (int i = 0; i < num; i++)
        wk->ent[i].id = -1;
    return 0;
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x00108ef8.

int BpTexRelease(int handle)
{
    if (bp_tex_work[handle] != nullptr && BpTexGetWork(handle) != nullptr)
        bp_tex_work[handle] = nullptr;
    return 0;
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x00108f48.

int BpTexInit(void)
{
    for (int i = 0; i < BP_TEX_MAX; i++)
        bp_tex_work[i] = nullptr;
    return 0;
}
