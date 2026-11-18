// graphics/graph3d/CDebugEdit.cpp -- IEditObject / CEditRoot, FF3 0x10afa8..0x10b788
// (+ the linkonce bodies 0x2ab6c8..0x2abdf8). From MioPan's CDebugEdit.cpp, checked
// against FF3: the classes gained a vptr in front (every member moves up 4), a root
// built without a parent is its own parent, Clear deletes the children (FF2 called
// their Clear), TypeString / ValueString return "Root" / NULL, TypeInfo is typeid(void),
// and only GetObject(label, n) is emitted. No FF3 code constructs a CEditRoot
// (ghidra.py refs: no jal / j / data word to the ctors or AddRoot), so it is
// reconstructed for completeness and never runs.

#include "graphics/graph3d/CDebugEdit.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

// ──────────────────────────────────────────────────────────────────────
// IEditObject: its ctor is only ever inlined into CEditRoot's; the rest are the
// linkonce bodies at 0x2abb68..0x2abc40.

// Empty label (the shared empty rep, then _M_mutate(0, size(), 0)), no handler,
// no context, no parent.
IEditObject::IEditObject()
{
    m_fpOnValueChanged = NULL;
    m_pContext         = NULL;
    m_pParentRoot      = NULL;
}

// The label's rep is released. Deleting twin 0x2abba8 (operator_delete).
// FF3 0x002abb68
// callcheck: string_Rep_M_destroy (m_strLabel's destructor)
IEditObject::~IEditObject()
{
}

// FF3 0x002abc00
char *IEditObject::ValueString()
{
    return NULL;
}

// FF3 0x002abc08
void IEditObject::Inc(int bExecCallback)
{
    (void)bExecCallback;
}

// FF3 0x002abc10
void IEditObject::Inc(float fScale, int bExecCallback)
{
    (void)fScale;
    (void)bExecCallback;
}

// FF3 0x002abc18
void IEditObject::Dec(int bExecCallback)
{
    (void)bExecCallback;
}

// FF3 0x002abc20
void IEditObject::Dec(float fScale, int bExecCallback)
{
    (void)fScale;
    (void)bExecCallback;
}

// FF3 0x002abc28
void IEditObject::Clear()
{
}

// FF3 0x002abc30
char *IEditObject::TypeString()
{
    return NULL;
}

// CEditRoot does not override it. FF3 0x002abc38
const std::type_info &IEditObject::TypeInfo()
{
    return typeid(void);
}

// ──────────────────────────────────────────────────────────────────────
// CEditRoot. Its operator new / delete are g3dMalloc / g3dFree, compiled to
// NullFuncRet0 / NullFunc in the retail build (AddRoot would build its child at
// address 0); the host takes the global heap, as it does for the vector's
// allocator.

// A root made without a parent is its own parent. Base-object twin 0x10b030
// (the vector through vector_IEditObject_ctor), same body.
// FF3 0x0010afa8
// callcheck: string_M_mutate (the empty label, IEditObject's ctor)
CEditRoot::CEditRoot()
{
    m_pParentRoot = this;
}

// Label pStr, parent pParentRoot. Twin 0x10b248 (assign out of line; the one
// AddRoot calls).
// FF3 0x0010b0b8
// callcheck: string_M_mutate, vector_IEditObject_ctor, strlen, __throw_length_error, string_M_replace_safe2, memcpy, memmove (the members' ctors and std::string::assign)
CEditRoot::CEditRoot(char *pStr, CEditRoot *pParentRoot)
{
    m_strLabel    = pStr;
    m_pParentRoot = pParentRoot;
}

// Every child is deleted (_delete_ContainerPtr_ForEach: virtual deleting dtor,
// slot nulled), then the vector and the label go. Base-object twin 0x10b308,
// deleting 0x10b3f8 (operator delete = NullFunc).
// FF3 0x0010b380
// callcheck: vector_IEditObject_deallocate, string_Rep_M_dispose, _delete_ContainerPtr_ForEach<ctl::vector<IEditObject*>>
CEditRoot::~CEditRoot()
{
    for (size_t i = 0; i < m_vpEditItem.size(); i++)
    {
        if (m_vpEditItem[i] != NULL)
        {
            delete m_vpEditItem[i];
            m_vpEditItem[i] = NULL;
        }
    }
}

// FF3 0x002abc48
int CEditRoot::IsItem()
{
    return 0;
}

// FF3 0x0010b478
// callcheck: strlen, string_assign_cstr_n (std::string::assign(const char *))
int CEditRoot::Create(char *strLabel)
{
    m_strLabel = strLabel;
    return 1;
}

// Delete every child and empty the list.
// FF3 0x0010b4c0
// callcheck: memmove (vector::erase(begin, end))
void CEditRoot::Clear()
{
    for (int i = 0; i < (int)m_vpEditItem.size(); i++)
    {
        if (m_vpEditItem[i] != NULL)
        {
            delete m_vpEditItem[i];
        }
        m_vpEditItem[i] = NULL;
    }
    m_vpEditItem.erase(m_vpEditItem.begin(), m_vpEditItem.end());
}

// A child root with a printf-formatted label, appended and returned.
// FF3 0x0010b598
// callcheck: vsprintf, NullFuncRet0, EditRootCtorLabelBase, vector_IEditObject_M_insert_aux (vsnprintf on the host; operator new, the ctor, push_back)
CEditRoot *CEditRoot::AddRoot(char *pStr, ...)
{
    char       str[256];
    CEditRoot *pRoot;
    va_list    args;

    va_start(args, pStr);
    vsnprintf(str, sizeof(str), pStr, args);
    va_end(args);

    pRoot = new CEditRoot(str, this);
    m_vpEditItem.push_back(pRoot);
    return pRoot;
}

// The n-th child whose label is strLabel, or NULL.
// FF3 0x0010b698
IEditObject *CEditRoot::GetObject(char *strLabel, int n)
{
    int iCount = 0;

    for (int i = 0; i < (int)m_vpEditItem.size(); i++)
    {
        if (strcmp(strLabel, m_vpEditItem[i]->LabelString()) == 0)
        {
            if (iCount == n)
            {
                return m_vpEditItem[i];
            }
            iCount++;
        }
    }
    return NULL;
}

// "Root" (0x36b618). FF3 0x002abc50
char *CEditRoot::TypeString()
{
    return (char *)"Root";
}

// ──────────────────────────────────────────────────────────────────────
// The libstdc++ instances FF3 emitted for this file (refcounted string rep:
// length -0xc, capacity -8, refcount -4; max_size 0x3ffffffc at 0x372880). On the
// host they are std::string / std::vector's own code, nothing to reconstruct:
//   FF3 0x002ab6c8 string_M_mutate                  basic_string::_M_mutate(pos, len1, len2)
//   FF3 0x002ab840 string_M_replace_safe2           _M_replace_safe(i1, i2, s, n)
//   FF3 0x002ab8e8 _delete_ContainerPtr_ForEach<ctl::vector<IEditObject*>>
//                                                   delete each element, null its slot
//   FF3 0x002ab960 vector_IEditObject_M_insert_aux  vector<IEditObject *>::_M_insert_aux
//   FF3 0x002abab8 string_Rep_S_create              _Rep::_S_create(capacity, alloc)
//   FF3 0x002abc60 string_Rep_M_dispose             _Rep::_M_dispose(alloc)
//   FF3 0x002abc90 vector_IEditObject_ctor          _Vector_base<IEditObject *>()
//   FF3 0x002abca0 vector_IEditObject_deallocate    _Vector_base::_M_deallocate (NullFunc)
//   FF3 0x002abcc8 string_assign_cstr_n             assign(const char *, size_type)
//   FF3 0x002abdd8 string_Rep_M_destroy             _Rep::_M_destroy (deallocate = NullFunc)
