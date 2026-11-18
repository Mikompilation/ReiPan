/* ==========================================================================
 *  main/gphase.h
 *
 *  The game-phase engine (FF3's C++ rebuild of FF2's GPhase): 120 phases in
 *  a tree (main/gphase_tree.h), five function tables (init/one/end/pre/after)
 *  and a controller that keeps a stack of nodes from the root (super) down to
 *  the current phase. Each frame GPHASE_CTRL::Main pushes nodes toward the
 *  current phase, runs the stack from the root (pre on the way down, the
 *  deepest node's one, after on the way up), and pops nodes that ended.
 *  A phase changes when its one function returns non-zero after
 *  GPHASE_CTRL::SetNext; nodes off the new phase's root path then end.
 *  FF3 0x1d3b40..0x1d4240 plus the getters GetGPhaseCtrl (0x2ab3e0) and
 *  GetGPhaseTree (0x2abe40). FF2 main/gphase.c (InitGPhaseSys, GPhaseSysMain,
 *  SetNextGPhase) had fixed layers instead of a tree.
 * ======================================================================== */
#ifndef _MAIN_GPHASE_H
#define _MAIN_GPHASE_H

#include "main/gphase_tree.h"

// Phase functions. init/one/end return non-zero when done (one: when the
// phase changes); pre/after run every frame and may be absent.
typedef int  (*GPHASE_FUNC)(void);
typedef void (*GPHASE_SUB_FUNC)(void);

// GPHASE_NODE.state
enum GPHASE_STATE
{
    GPHASE_STATE_INIT     = 0,      // calling init until it returns non-zero
    GPHASE_STATE_LOAD     = 1,      // load wait (FF3's hook always returns 1)
    GPHASE_STATE_RUN      = 2,      // pre / one (deepest node) / after each frame
    GPHASE_STATE_END_WAIT = 3,      // off the current path: end wait (always 1 in FF3)
    GPHASE_STATE_END      = 4,      // calling end until it returns non-zero
    GPHASE_STATE_DONE     = 5,      // popped by GPHASE_CTRL::FreeEndedNodes
};

// One active phase on the controller's stack (operator new, 0x14 bytes on the EE,
// then 0x1d3cd8 on it: the new-expression's constructor).
// layout: host -- starts with two pointers (EE +0, +4; id at +8).
struct GPHASE_NODE
{
    GPHASE_NODE(GPHASE_NODE *parent, short id);     // FF3 0x001d3cd8 (GPhaseNodeInit)
    void SetState(short state);                     // FF3 0x001d3d00 (GPhaseNodeSetState)
    void Exec();                                    // FF3 0x001d3d18 (GPhaseNodeExec)

    GPHASE_NODE *parent;            // node of the parent phase, nullptr at the root
    GPHASE_NODE *child;             // node of the child phase, nullptr at the leaf
    short        id;                // phase
    short        state;             // GPHASE_STATE
    short        state_cnt;         // EE +0xc: cleared on every state change; no reader in gphase.c
    int          state_wrk;         // EE +0x10: likewise
};
static_assert(offsetof(GPHASE_NODE, child) == sizeof(GPHASE_NODE *), "GPHASE_NODE.child");
static_assert(offsetof(GPHASE_NODE, id) == 2 * sizeof(GPHASE_NODE *), "GPHASE_NODE.id");
static_assert(offsetof(GPHASE_NODE, state) == offsetof(GPHASE_NODE, id) + 2, "GPHASE_NODE.state");
static_assert(offsetof(GPHASE_NODE, state_cnt) == offsetof(GPHASE_NODE, id) + 4, "GPHASE_NODE.state_cnt");
static_assert(offsetof(GPHASE_NODE, state_wrk) == offsetof(GPHASE_NODE, id) + 8, "GPHASE_NODE.state_wrk");

// The controller singleton (GetGPhaseCtrl, bss 0x41e468): 0x10 bytes on the EE.
// The static's constructor runs Init and its destructor (FF3's atexit thunks)
// Term; GameSystemStart / End call them again for every session.
// layout: host -- the two node pointers (EE +8, +0xc) are 8 bytes each here.
struct GPHASE_CTRL
{
    GPHASE_CTRL() { Init(); }
    ~GPHASE_CTRL() { Term(); }      // FF3's atexit thunk

    void Init();                    // FF3 0x001d3fa0 (GPhaseCtrlInit)
    void Term();                    // FF3 0x001d3fc0 (GPhaseCtrlTerm)
    void SetNext(int id, int link); // FF3 0x001d3fd8 (GPhaseCtrlSetNext)
    void Change();                  // FF3 0x001d3ff0 (GPhaseCtrlChange)
    void Main();                    // FF3 0x001d4070 (GPhaseCtrlMain)
    void BuildStack();              // FF3 0x001d40a0 (GPhaseCtrlBuildStack)
    void FreeEndedNodes();          // FF3 0x001d41b0 (GPhaseCtrlFreeEndedNodes)

    /* 0x00 */ short from;          // phase whose one requested the last change (-1: none)
    /* 0x02 */ short now;           // current (target) phase
    /* 0x04 */ short next;          // requested phase, -1 = none
    /* 0x06 */ short next_link;     // non-zero: link next under 'from' in the tree on change
    /* 0x08 */ GPHASE_NODE *leaf;   // deepest node
    /* 0x0c */ GPHASE_NODE *root;   // super's node, where execution starts
};
static_assert(offsetof(GPHASE_CTRL, from) == 0x0, "GPHASE_CTRL.from");
static_assert(offsetof(GPHASE_CTRL, now) == 0x2, "GPHASE_CTRL.now");
static_assert(offsetof(GPHASE_CTRL, next) == 0x4, "GPHASE_CTRL.next");
static_assert(offsetof(GPHASE_CTRL, next_link) == 0x6, "GPHASE_CTRL.next_link");
static_assert(offsetof(GPHASE_CTRL, leaf) == 0x8, "GPHASE_CTRL.leaf");
static_assert(offsetof(GPHASE_CTRL, root) == 0x8 + sizeof(GPHASE_NODE *), "GPHASE_CTRL.root");

extern GPHASE_TREE_DEF gphase_tree_def;         // data 0x327f20

GPHASE_CTRL *GetGPhaseCtrl(void);
GPHASE_TREE *GetGPhaseTree(void);

#endif /* _MAIN_GPHASE_H */
