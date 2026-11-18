/* ==========================================================================
 *  ingame/map/MapFog.h
 *
 *  Map fog (FF2 MapFog.c; FF3 0x118900..0x119348). FF3 reshaped the
 *  setting (floats for the ramp, shorts for the density, bytes for the
 *  colour), looks the region's setting up in a fog table by id instead of
 *  reading it out of the region record, falls back to the area's own fog
 *  (area_data.ecb) outside every region, and dropped FF2's sky fade.
 * ======================================================================== */
#ifndef _INGAME_MAP_MAPFOG_H
#define _INGAME_MAP_MAPFOG_H

#include <stddef.h>                     // offsetof
#include "sdk/scetypes.h"

// One fog setting (0x10 bytes): the ramp fed to gra3dSetFog(min, max, near,
// far) and the colour fed to gra3dSetFogColor. The same layout sits in the
// fog table, in MapFogDat and in each area_data.ecb row.
// layout: EE (ROM data and file data)
struct MAP_FOG_DAT
{
    /* 0x00 */ float   near_;           // ramp start, world units
    /* 0x04 */ float   far_;            // ramp end
    /* 0x08 */ short   min;             // density at near_ (0..255)
    /* 0x0a */ short   max;             // density at far_
    /* 0x0c */ u_char  r;
    /* 0x0d */ u_char  g;
    /* 0x0e */ u_char  b;
    /* 0x0f */ u_char  a;               // never read; the area path writes 0, MapFogDat starts at 40
};
static_assert(sizeof(MAP_FOG_DAT) == 0x10, "MAP_FOG_DAT size");
static_assert(offsetof(MAP_FOG_DAT, far_) == 0x04, "MAP_FOG_DAT.far_");
static_assert(offsetof(MAP_FOG_DAT, min) == 0x08, "MAP_FOG_DAT.min");
static_assert(offsetof(MAP_FOG_DAT, max) == 0x0a, "MAP_FOG_DAT.max");
static_assert(offsetof(MAP_FOG_DAT, r) == 0x0c, "MAP_FOG_DAT.r");
static_assert(offsetof(MAP_FOG_DAT, b) == 0x0e, "MAP_FOG_DAT.b");

// A row of the fog table (data 0x2efc28, 0xac rows): a region's fog id and
// its two settings, [1] while the camera obscura is up.
// layout: EE (ROM data)
struct MAP_FOG_TBL
{
    /* 0x00 */ int         id;
    /* 0x04 */ MAP_FOG_DAT dat[2];
};
static_assert(sizeof(MAP_FOG_TBL) == 0x24, "MAP_FOG_TBL size");
static_assert(offsetof(MAP_FOG_TBL, dat) == 0x04, "MAP_FOG_TBL.dat");

// The stat record of a fog rectangle (types 5 and 13; RegDatGetStPtr). FF3
// keeps only the fog table id here (FF2 kept the two settings).
// layout: EE (file data)
struct MDAT_FOG
{
    /* 0x00 */ u_char _00[4];
    /* 0x04 */ int    id;               // MAP_FOG_TBL.id
};
static_assert(offsetof(MDAT_FOG, id) == 0x04, "MDAT_FOG.id");

MAP_FOG_DAT *MapFogGetTblDat(int id, int finder);   // FF3 0x00118900: &row.dat[finder] of the table row id, nullptr if none
void MapFogAnimStart(int frame);        // FF3 0x00118950: MapFogDat[0] = the setting in force; the next frame calls walk it to MapFogDat[1]
void MapFogAnim(void);                  // FF3 0x001189a0: one step of that walk into MapFogDat[2]
void MapFogReset(void);                 // FF3 0x001190c8
void MapFogProc(int area_no, int floor);    // FF3 0x001190f0: per frame; samples the regions at the camera, pushes the fog to gra3d

// Scripted override (PlyrStatusCtrl status 2: (75, 80, 4, 4, 4, 50, 0xff)):
// a 16-frame walk to the setting, the regions ignored until the end call.
void MapFogStartFogEv(float near_, float far_, int r, int g, int b, int min, int max);  // FF3 0x00118cd8
void MapFogEndFogEv(int frame);         // FF3 0x00118d90: frame is not read (the ROM's has no parameter)

#endif /* _INGAME_MAP_MAPFOG_H */
