/* ==========================================================================
 *  main/gphase_tree.c
 *
 *  The phase tree (see gphase_tree.h): a heap copy of the ROM definition,
 *  ancestor queries for the node stack, and run-time re-linking.
 *  FF3 0x1d4240..0x1d4678. No FF2 counterpart.
 *
 *  FF3 0x001d4640 is not written: a compiler-emitted static-destruction thunk
 *  (GPhaseCtrlTerm on the GPhaseCtrl singleton) that marks the end of the file.
 * ======================================================================== */
#include "main/gphase_tree.h"

#include <string.h>                 // memcpy


// ──────────────────────────────────────────────────────────────────────
// Copy the definition to a fresh heap tree.
// FF3 0x001d4240 (operator new[] of 0x3c0 bytes on the C++ heap).
// callcheck: operator_new_array (new[])

void GPHASE_TREE::Init(GPHASE_TREE_DEF *def)
{
    this->def = def;
    node      = new GPHASE_TREE_NODE_DEF[GPHASE_NUM];
    Reset();
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x001d4278 (FF3 frees with operator_delete; delete[] pairs with new[]
// here and is the same for a trivially destructible array).
// callcheck: operator_delete (delete[])

void GPHASE_TREE::Term()
{
    if (node != nullptr)
    {
        delete[] node;
    }
    def  = nullptr;
    node = nullptr;
}

// ──────────────────────────────────────────────────────────────────────
// Undo every run-time link: copy the definition back over the working tree.
// FF3 0x001d42b8.

void GPHASE_TREE::Reset()
{
    if (node != nullptr && def != nullptr)
    {
        memcpy(node, def, sizeof(GPHASE_TREE_DEF));
    }
}

// ──────────────────────────────────────────────────────────────────────
// id's chain from the root, indexed by depth: path[depth of each ancestor] =
// that ancestor, the rest of path[0..num) -1.
// FF3 0x001d42f8.

void GPHASE_TREE::GetPath(short *path, short id, short num)
{
    int i;

    for (i = 0; i < num; i++)
    {
        path[i] = -1;
    }

    while (id != -1)
    {
        path[node[id].depth] = id;
        id = node[id].parent;
    }
}

// ──────────────────────────────────────────────────────────────────────
// 1 when id lies off target's root path (neither target nor one of its
// ancestors), 0 when it is an ancestor or target itself, or target is -1.
// FF3 0x001d4370. FF3 sizes the two path arrays on the stack (alloca of the
// longer path length); PC: fixed GPHASE_NUM arrays, the length never exceeds it.

int GPHASE_TREE::IsOffPath(short id, short target)
{
    short id_path[GPHASE_NUM];
    short target_path[GPHASE_NUM];
    short id_len;
    short num;
    int   i;

    if (target == -1 || id == target)
    {
        return 0;
    }

    id_len = GetPathLen(id);
    num    = GetPathLen(target);
    if (num < id_len)
    {
        num = id_len;
    }
    GetPath(id_path, id, num);
    GetPath(target_path, target, num);

    for (i = 0; ; i++)
    {
        if (id_path[i] == -1)
        {
            return 0;               // id's chain ended inside target's: an ancestor
        }
        if (id_path[i] != target_path[i])
        {
            return 1;
        }
    }
}

// ──────────────────────────────────────────────────────────────────────
// 1 when id is a strict ancestor of target.
// FF3 0x001d44b0.

int GPHASE_TREE::IsAncestor(short id, short target)
{
    int ret;

    ret = 0;
    if (target != -1 && id != target)
    {
        ret = IsOffPath(id, target) == 0;
    }
    return ret;
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x001d44f8 (returns 0, not -1, for id -1).

short GPHASE_TREE::GetParent(short id)
{
    short parent;

    parent = 0;
    if (id != -1)
    {
        parent = node[id].parent;
    }
    return parent;
}

// ──────────────────────────────────────────────────────────────────────
// Hang id (and its subtree) under parent; the parent's child list is not
// touched, so only upward walks see the link.
// FF3 0x001d4528.


void GPHASE_TREE::Link(short id, short parent)
{
    if (id != -1 && parent != -1)
    {
        node[id].depth  = node[parent].depth + 1;
        node[id].parent = parent;
        UpdateDepth(id);
    }
}

// ──────────────────────────────────────────────────────────────────────
// Renumber the depths of id's subtree from id's depth.
// FF3 0x001d4590.

void GPHASE_TREE::UpdateDepth(short id)
{
    GPHASE_TREE_NODE_DEF *node;
    int                   i;

    node = &this->node[id];
    for (i = 0; i < node->child_num; i++)
    {
        this->node[node->child + i].depth = node->depth + 1;
        UpdateDepth(node->child + i);
    }
}

// ──────────────────────────────────────────────────────────────────────
// Number of phases from id up to the root, both included (depth + 1 in an
// unlinked tree).
// FF3 0x001d4610.

short GPHASE_TREE::GetPathLen(short id)
{
    short len;

    len = 0;
    do
    {
        id = node[id].parent;
        len++;
    } while (id != -1);
    return len;
}
