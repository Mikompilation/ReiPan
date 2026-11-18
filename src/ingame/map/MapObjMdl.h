/* ==========================================================================
 *  ingame/map/MapObjMdl.h
 *
 *  The map object model handle (FF3-only, 0x203310..0x203828): the model
 *  of a room, a door or a piece of furniture, built from its model PK4 (and
 *  an optional motion PK4) by FF3's builder classes (graphics/model_builder.h)
 *  and driven through the ModelManager the builder makes.
 * ======================================================================== */
#ifndef _INGAME_MAP_MAPOBJMDL_H
#define _INGAME_MAP_MAPOBJMDL_H

#include "sdk/scetypes.h"

class  ModelManager;                    // graphics/mmanage.h
typedef ModelManager MDL_MOT;
class  ModelBuilder;                    // graphics/model_builder.h

// What Build made (its return value). MapObjRegist lights kinds 1 and 2 with
// the room light and flags their put (0x800 cloth, 0x4000 chains).
enum
{
    MAP_OBJ_MDL_PLAIN = 0,              // NormalModelBuilder: no motion
    MAP_OBJ_MDL_CLOTH = 1,              // the PK4 has a cls member (MotionModelBuilder, the cloth plays at once)
    MAP_OBJ_MDL_CHAIN = 2,              // a ccs member and no cls (MotionModelBuilder)
    MAP_OBJ_MDL_MOTION = 3,             // with a motion PK4 (MotionModelBuilder over its first member)
};

// A model handle (0x10 bytes on the EE): MAP_ROOM_DAT.mdl, MAP_OBJ_WRK.mdl,
// CMapDBuff.own. A plain C++ object (no vtable); its members take it in a0.
// layout: host -- mot, builder and the PK4s are EE pointers (4 bytes each);
// EE offsets in the comments.
struct MAP_OBJ_MDL                      /* 0x10 on the EE */
{
    void SetAlpha(u_char alpha);                        // FF3 0x00203310
    int  SetMatrix(float *matrix);                      // FF3 0x00203330: 0
    void SetPosRot(const float *pos, const float *rot); // FF3 0x00203360
    int  Draw(u_int flags, u_int gs);                   // FF3 0x00203430: 0
    int  PlayMotion(int motion, float speed);           // FF3 0x002034a8: 0
    int  SetSpeed(float speed);                         // FF3 0x00203518: 0
    int  SetPose(int motion);                           // FF3 0x00203548: 0
    int  Build(void *mdl_pk4, void *mot_pk4);           // FF3 0x002035d0: MAP_OBJ_MDL_*, -1 never
    void Release();                                     // FF3 0x002037b0
    void Rebuild();                                     // FF3 0x002037f8

    /* 0x00 */ MDL_MOT      *mot;           // the builder's result
    /* 0x04 */ ModelBuilder *builder;       // NormalModelBuilder (0x28) / MotionModelBuilder (0x54); owns mot
    /* 0x08 */ void         *mdl_pk4;       // the model PK4 (Rebuild)
    /* 0x0c */ void         *mot_pk4;       // the motion PK4 or nullptr
};
static_assert(sizeof(MAP_OBJ_MDL) == 4 * sizeof(void *), "MAP_OBJ_MDL is four pointers (0x10 on the EE)");

#endif /* _INGAME_MAP_MAPOBJMDL_H */
