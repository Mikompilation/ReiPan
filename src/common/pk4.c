// common/pk4.c -- FF3's PK2 / PK4 pack accessors (FF3 0x1eff18..0x1f0138).
// No FF2 counterpart: written from the FF3 decompilation. Layouts in pk4.h.

#include "common/pk4.h"

// ──────────────────────────────────────────────────────────────────────
// Member `no` of a PK2: pack + offset[no] (count at +0), null when out of
// range. FF3 0x001eff18.

void *Pk2GetData(void *pk2, int no)
{
    int num;

    if (no >= 0)
    {
        num = 0;
        if (pk2 != nullptr)
        {
            num = *(int *)pk2;
        }
        if (no < num)
        {
            return (u_char *)pk2 + ((PK4_HEADER *)pk2)->offset[no];
        }
    }
    return nullptr;
}

// ──────────────────────────────────────────────────────────────────────
// Offset of PK4 member `no` (unchecked). FF3 0x001eff50.

int Pk4GetOffset(void *pk4, int no)
{
    return ((PK4_HEADER *)pk4)->offset[no];
}

// The member's head as the next three functions read it: null when `no` is out
// of range or the pack's members have no header. FF3 inlines this in each.
static PK4_MEMBER *Pk4GetMember(void *pk4, int no)
{
    PK4_HEADER *pk = (PK4_HEADER *)pk4;

    if (no < 0)
        return nullptr;
    if (no >= (pk != nullptr ? pk->num : 0))
        return nullptr;
    if ((pk != nullptr ? pk->headed : 0) <= 0)
        return nullptr;
    return (PK4_MEMBER *)((u_char *)pk4 + Pk4GetOffset(pk4, no));
}

// ──────────────────────────────────────────────────────────────────────
// Header size of a headed member (where its data starts), else 0.
// FF3 0x001eff60.
// callcheck: Pk4GetOffset (in Pk4GetMember, which FF3 inlines)

int Pk4GetHeadSize(void *pk4, int no)
{
    PK4_MEMBER *m = Pk4GetMember(pk4, no);

    return m != nullptr ? m->head_size : 0;
}

// ──────────────────────────────────────────────────────────────────────
// Size of a headed member, else 0. Unreferenced in FF3. FF3 0x001effc8.
// callcheck: Pk4GetOffset (in Pk4GetMember, which FF3 inlines)

int Pk4GetSize(void *pk4, int no)
{
    PK4_MEMBER *m = Pk4GetMember(pk4, no);

    return m != nullptr ? m->size : 0;
}

// ──────────────────────────────────────────────────────────────────────
// Name of a headed member, else null. FF3 0x001f0030.
// callcheck: Pk4GetOffset (in Pk4GetMember, which FF3 inlines)

char *Pk4GetName(void *pk4, int no)
{
    PK4_MEMBER *m = Pk4GetMember(pk4, no);

    return m != nullptr ? m->name : nullptr;
}

// ──────────────────────────────────────────────────────────────────────
// Data of member `no`: past its header when the pack has headed members,
// null when out of range. FF3 0x001f0098.

void *Pk4GetData(void *pk4, int no)
{
    PK4_HEADER *pk = (PK4_HEADER *)pk4;

    if (no < 0 || no >= (pk != nullptr ? pk->num : 0))
        return nullptr;

    if ((pk != nullptr ? pk->headed : 0) > 0)
        return (u_char *)pk4 + Pk4GetOffset(pk4, no) + Pk4GetHeadSize(pk4, no);
    return (u_char *)pk4 + Pk4GetOffset(pk4, no);
}
