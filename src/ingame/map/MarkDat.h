/* ==========================================================================
 *  ingame/map/MarkDat.h
 *
 *  CMarkDat (FF3 RTTI, vtable 0x341048): the map menu's floor layouts, the
 *  maps_*_b2d files of table 0x34c400 (8 floors x 5 layers; floors 0..4
 *  B2F..3F, 5..7 1F..3F of world 1, the KSW files). MarkDatLoadAll loads
 *  them at boot (one_Boot_Load), MapSnapInit makes the CMarkDat, MapSnap
 *  reads the layers through it. FF3 0x0010e650..0x0010f150; FF3-only.
 * ======================================================================== */
#ifndef _INGAME_MAP_MARKDAT_H
#define _INGAME_MAP_MARKDAT_H

#include "sdk/scetypes.h"

#include <stddef.h>                     // offsetof

#define CMARK_FLOOR_NUM 8
#define CMARK_LAYER_NUM 5

// The layout files, data 0x34c400 (-1 none). MarkDatLoadAll turns the
// entries of missing files into -1.
extern int mark_dat_file[CMARK_FLOOR_NUM][CMARK_LAYER_NUM];

// One layout of the map menu.
struct CMARK_LAYER                      /* 0x1c */
{
    /* 0x00 */ int   _00;               // 1 after the constructor / CMarkDat::Clear; nothing else seen
    /* 0x04 */ int   played;            // CMarkDat::SetupLayers played its label and measured rect
    /* 0x08 */ int   handle;            // BpCtl handle of the layout file, -1
    /* 0x0c */ float rect[4];           // the world XZ rect (x, z, w, h) its room items cover (MapSnapLayerGetRect)
};
static_assert(sizeof(CMARK_LAYER) == 0x1c, "CMARK_LAYER is 0x1c bytes");
static_assert(offsetof(CMARK_LAYER, played) == 0x04, "CMARK_LAYER.played");
static_assert(offsetof(CMARK_LAYER, handle) == 0x08, "CMARK_LAYER.handle");
static_assert(offsetof(CMARK_LAYER, rect) == 0x0c, "CMARK_LAYER.rect");

// 0x46c bytes on the EE (MapSnapInit's operator new), gp-0x255c
// (MapSnapGetMarkDat). Its methods are free functions taking it.
// layout: host -- the vptr is 4 bytes on the EE: loaded is at EE +4, busy +8,
// layer +0xc (floor stride 0x8c).
class CMarkDat
{
public:
    CMarkDat();                         // FF3 0x0010e770 (MarkDatCtor; twin 0x0010e650)
    virtual ~CMarkDat();                // EE vtable +0x00 0x0010e8c0, deleting +0x04 0x0010e8f0

    int GetFloor(int floor);            // FF3 0x0010e930: the layout floor (+3 in world 1 unless busy), 0..7
    int GetPhfNo(int floor, int layer); // FF3 0x0010e968: the layer's maps_*_phf file (table + 1), -1 none
    int GetWorldFloor(int floor);       // FF3 0x0010e9c0: floor + 3 in world 1 (ingame_wrk.b03)
    int GetHandle(int floor, int layer);  // FF3 0x0010e9e0: the layer's Bp handle once loaded, -1
    void SetPos(int floor, int x, int y);  // FF3 0x0010ea48
    void SetScale(int floor, int sx, int sy);  // FF3 0x0010ebf8
    void Measure(int *rect, int x, int y, int sx, int sy);  // FF3 0x0010ece0 (MapSnapMarkDatDraw): screen x0, y0, x1, y1
    int IsReady();                      // FF3 0x0010ef50: 1 when every layout is loaded (sets loaded)
    void SetupLayers();                 // FF3 0x0010f010: once loaded, each layer's label played and rect measured
    void Clear();                       // FF3 0x0010f0d8: every layer with a handle back to none

    int         loaded;                 // EE +0x04: every layout loaded (CMarkDat::IsReady)
    int         busy;                   // EE +0x08: MapSnapLayerGetRect running; CMarkDat::GetFloor ignores the world then
    CMARK_LAYER layer[CMARK_FLOOR_NUM][CMARK_LAYER_NUM];  // EE +0x0c
};
static_assert(sizeof(CMARK_LAYER[CMARK_LAYER_NUM]) == 0x8c, "CMarkDat floor stride is 0x8c");
static_assert(offsetof(CMarkDat, busy) == offsetof(CMarkDat, loaded) + 4, "CMarkDat.busy follows loaded");
static_assert(offsetof(CMarkDat, layer) == offsetof(CMarkDat, loaded) + 8, "CMarkDat.layer follows busy");

void MarkDatGetFloorRange(int *lo, int *hi);                // FF3 0x0010ec80: floors lo..hi-1 of the world
void MarkDatLoadAll(void);              // FF3 0x0010ee40: every layout file -> EE 0xfcf000.. (one_Boot_Load)

#endif /* _INGAME_MAP_MARKDAT_H */
