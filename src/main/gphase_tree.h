/* ==========================================================================
 *  main/gphase_tree.h
 *
 *  The phase tree: one {depth, parent, first child, child count} entry per
 *  phase. GPHASE_TREE::Init copies the ROM definition (gphase_tree_def, data
 *  0x327f20) into a heap copy that the engine can re-link at run time
 *  (Link: a phase entered with the "link" flag becomes a child of
 *  the phase that requested it), and the queries answer "is A an ancestor of
 *  B" for the node stack (main/gphase.c).
 *  FF3 0x1d4240..0x1d4678; no FF2 counterpart (FF2's GPhase used fixed layers).
 * ======================================================================== */
#ifndef _MAIN_GPHASE_TREE_H
#define _MAIN_GPHASE_TREE_H

#include <stddef.h>                 // offsetof

#define GPHASE_NUM  120             // phases in the tree and in every function table

// One phase's place in the tree; -1 = none.
struct GPHASE_TREE_NODE_DEF         /* 0x8 */
{
    /* 0x0 */ short depth;          // 0 = super (root)
    /* 0x2 */ short parent;
    /* 0x4 */ short child;          // first child; children are consecutive ids
    /* 0x6 */ short child_num;
};
static_assert(sizeof(GPHASE_TREE_NODE_DEF) == 0x8, "GPHASE_TREE_NODE_DEF is 8 bytes");
static_assert(offsetof(GPHASE_TREE_NODE_DEF, depth) == 0x0, "GPHASE_TREE_NODE_DEF.depth");
static_assert(offsetof(GPHASE_TREE_NODE_DEF, parent) == 0x2, "GPHASE_TREE_NODE_DEF.parent");
static_assert(offsetof(GPHASE_TREE_NODE_DEF, child) == 0x4, "GPHASE_TREE_NODE_DEF.child");
static_assert(offsetof(GPHASE_TREE_NODE_DEF, child_num) == 0x6, "GPHASE_TREE_NODE_DEF.child_num");

struct GPHASE_TREE_DEF              /* 0x3c0 */
{
    GPHASE_TREE_NODE_DEF node[GPHASE_NUM];
};
static_assert(sizeof(GPHASE_TREE_DEF) == 0x3c0, "GPHASE_TREE_DEF is 120 x 8 bytes");

// The tree singleton (GetGPhaseTree, sbss gp+0x6a0): 8 bytes on the EE. The
// static's destructor (FF3's atexit thunk 0x1d4210) runs Term, which
// GameSystemEnd also calls.
// layout: host -- two pointers (EE +0 node, +4 def).
struct GPHASE_TREE
{
    GPHASE_TREE() : node(nullptr), def(nullptr) {}
    ~GPHASE_TREE() { Term(); }      // FF3's atexit thunk

    void  Init(GPHASE_TREE_DEF *def);               // FF3 0x001d4240 (GPhaseTreeInit)
    void  Term();                                   // FF3 0x001d4278 (GPhaseTreeTerm)
    void  Reset();                                  // FF3 0x001d42b8 (GPhaseTreeReset)
    void  GetPath(short *path, short id, short num);    // FF3 0x001d42f8 (GPhaseTreeGetPath)
    int   IsOffPath(short id, short target);        // FF3 0x001d4370 (GPhaseTreeIsOffPath)
    int   IsAncestor(short id, short target);       // FF3 0x001d44b0 (GPhaseTreeIsAncestor)
    short GetParent(short id);                      // FF3 0x001d44f8 (GPhaseTreeGetParent)
    void  Link(short id, short parent);             // FF3 0x001d4528 (GPhaseTreeLink)
    void  UpdateDepth(short id);                    // FF3 0x001d4590 (GPhaseTreeUpdateDepth)
    short GetPathLen(short id);                     // FF3 0x001d4610 (GPhaseTreeGetPathLen)

    GPHASE_TREE_NODE_DEF *node;     // heap copy of *def (GPHASE_NUM entries), re-linked at run time
    GPHASE_TREE_DEF      *def;      // ROM definition it was copied from
};
static_assert(offsetof(GPHASE_TREE, node) == 0, "GPHASE_TREE.node");
static_assert(offsetof(GPHASE_TREE, def) == sizeof(GPHASE_TREE_NODE_DEF *), "GPHASE_TREE.def");

#endif /* _MAIN_GPHASE_TREE_H */
