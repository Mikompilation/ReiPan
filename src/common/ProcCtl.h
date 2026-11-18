/* ==========================================================================
 *  common/ProcCtl.h
 *
 *  CProcCtl (FF3 RTTI name): objects that register themselves in one global
 *  list on construction so ProcCtlReleaseAll (main.c, before a reset) can
 *  release them all. FF3-only file at 0x0010fb80..0x0010fd70 (between the
 *  Mark* class files and CWindowMsg; FF3's C++ class files link
 *  alphabetically). The one known subclass is CClassPtr<CLightCtl> (vtable
 *  0x3411a0, a static in the map-control run at 0x11d398).
 * ======================================================================== */
#ifndef _COMMON_PROCCTL_H
#define _COMMON_PROCCTL_H

#include <stddef.h>                     // offsetof

// layout: host -- the vptr and next are 4 bytes on the EE, 8 here; EE
// offsets below (size 8).
class CProcCtl
{
public:
    CProcCtl();                            // FF3 0x0010fb80 ProcCtlCtor
    virtual ~CProcCtl();                   // EE vtable +0x00 0x0010fc70 (base copy 0x0010fc20),
                                           // deleting +0x04 0x0010fcc0
    virtual int Release() { return 0; }    // EE vtable +0x08; base: FF3 0x002ac010 (no Ghidra function)

    /* 0x04 */ CProcCtl *next;          // list from proc_ctl_list (sdata gp-0x7e18), in construction order
};
static_assert(offsetof(CProcCtl, next) == sizeof(void *), "CProcCtl.next");

void ProcCtlReleaseAll(void);           // Release() on every registered object

// CClassPtr<T> (FF3 RTTI): a CProcCtl owning one heap object, deleted by
// Release (so ProcCtlReleaseAll frees it on a reset), by Delete and by the
// destructor. The one instance is CClassPtr<CLightCtl> (vtable 0x3411a0,
// the static at 0x38e6d0 in ingame/map/MapCtrl.c). Template code
// 0x002ac428 (dtor), 0x002ac480 (deleting dtor), 0x002ac4d8 (Release).
// layout: host -- EE size 0xc, ptr at +0x08.
template <class T>
class CClassPtr : public CProcCtl
{
public:
    CClassPtr() : ptr(nullptr) {}       // inlined into the static init (ProcCtlCtor, then ptr = 0)
    ~CClassPtr() override { Delete(); }     // FF3 0x002ac428
    int Release() override              // FF3 0x002ac4d8
    {
        Delete();
        return 0;
    }
    // Inlined at every use: deletes the object (its vtable +0x04) and clears.
    void Delete()
    {
        if (ptr != nullptr)
        {
            delete ptr;
            ptr = nullptr;
        }
    }

    /* 0x08 */ T *ptr;
};

#endif /* _COMMON_PROCCTL_H */
