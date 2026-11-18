// common/ProcCtl.c -- FF3 CProcCtl: a global list of releasable objects.
// FF3-only (no FF2 counterpart); 0x0010fb80..0x0010fd70.

#include "common/ProcCtl.h"

static CProcCtl *proc_ctl_list;         /* sdata gp-0x7e18 */

// ──────────────────────────────────────────────────────────────────────
// Appends the new object to the tail of the list.
// FF3 0x0010fb80.

CProcCtl::CProcCtl()
{
    CProcCtl *p;

    next = nullptr;
    if (proc_ctl_list == nullptr)
    {
        proc_ctl_list = this;
        return;
    }
    for (p = proc_ctl_list; p->next != nullptr; p = p->next)
        ;
    p->next = this;
}

// ──────────────────────────────────────────────────────────────────────
// Unlinks the object (if it is in the list).
// FF3 0x0010fc70 (complete; 0x0010fc20 is the identical base-object copy);
// the deleting variant 0x0010fcc0 adds operator delete.

CProcCtl::~CProcCtl()
{
    CProcCtl *prev;
    CProcCtl *p;

    if (proc_ctl_list == this)
    {
        proc_ctl_list = next;
        return;
    }
    for (prev = proc_ctl_list, p = prev->next; p != nullptr; prev = p, p = p->next)
    {
        if (p == this)
        {
            prev->next = next;
            return;
        }
    }
}

// ──────────────────────────────────────────────────────────────────────
// Calls Release() (vtable +0x08) on every registered object, in list order.
// FF3 0x0010fd28.

void ProcCtlReleaseAll(void)
{
    CProcCtl *p;

    for (p = proc_ctl_list; p != nullptr; p = p->next)
        p->Release();
}
