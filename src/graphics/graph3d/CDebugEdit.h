/* ==========================================================================
 *  graphics/graph3d/CDebugEdit.h
 *
 *  The debug-edit object tree (FF2 CDebugEdit.cpp). IEditObject (RTTI
 *  typeinfo 0x372878, vtable 0x340fa0) is the abstract base of an editable
 *  debug value: a label, an on-change handler + context and the parent root.
 *  CEditRoot (typeinfo 0x340fd0, vtable 0x340f68) is a node owning a vector
 *  of child IEditObjects, looked up by label. Like eeFile / eeFindFile it
 *  came with the g3d library and nothing in FF3 constructs one (ghidra.py
 *  refs: no jal / j / data word to the ctors or AddRoot): dead in the retail
 *  game, where its allocators are even NullFuncRet0 / NullFunc.
 *
 *  FF3 0x10afa8..0x10b788 + the linkonce bodies 0x2ab6c8..0x2abdf8 (the
 *  std::string instances first emitted here, vector<IEditObject *>,
 *  _delete_ContainerPtr_ForEach and the inline virtual bodies).
 *
 *  layout: host -- both classes have a vtable (EE vptr 4 bytes, host 8), a
 *  std::string (EE one pointer) and a std::vector (EE a 4-byte allocator
 *  object + start / finish / end_of_storage), so the EE offsets are noted
 *  per member and not asserted.
 * ======================================================================== */
#ifndef _GRAPHICS_GRAPH3D_CDEBUGEDIT_H
#define _GRAPHICS_GRAPH3D_CDEBUGEDIT_H

#include <string>
#include <typeinfo>
#include <vector>

class CEditRoot;

// Called when an edited value changes.
typedef void (*LPFUNC_ONEDITVALUECHANGED)(void *pContext);

// layout: host -- vtable + std::string. EE size 0x14.
class IEditObject
{
public:
    IEditObject();                      // inlined into CEditRoot's ctors
    virtual ~IEditObject();             // slot 0 FF3 0x002abb68, deleting slot 1 0x2abba8

    virtual int    IsItem() = 0;        // +0x08 (__cxa_pure_virtual 0x261918)
    virtual char  *ValueString();       // +0x0c, FF3 0x002abc00
    virtual void   Inc(int bExecCallback);                  // +0x10, FF3 0x002abc08
    virtual void   Inc(float fScale, int bExecCallback);    // +0x14, FF3 0x002abc10
    virtual void   Dec(int bExecCallback);                  // +0x18, FF3 0x002abc18
    virtual void   Dec(float fScale, int bExecCallback);    // +0x1c, FF3 0x002abc20
    virtual void   Clear();             // +0x20, FF3 0x002abc28
    virtual char  *TypeString();        // +0x24, FF3 0x002abc30
    virtual const std::type_info &TypeInfo();               // +0x28, FF3 0x002abc38

    char *LabelString() { return (char *)m_strLabel.c_str(); }

protected:
    std::string               m_strLabel;          // EE +0x04
    LPFUNC_ONEDITVALUECHANGED m_fpOnValueChanged;  // EE +0x08
    void                     *m_pContext;          // EE +0x0c
    CEditRoot                *m_pParentRoot;       // EE +0x10 (a root made without one is its own parent)
};

// layout: host -- vtable + std::string + std::vector. EE size 0x24.
class CEditRoot : public IEditObject
{
public:
    CEditRoot();                        // FF3 0x0010afa8 (base-object twin 0x10b030)
    CEditRoot(char *pStr, CEditRoot *pParentRoot);  // FF3 0x0010b0b8 (twin 0x10b248)
    virtual ~CEditRoot();               // slot 0 FF3 0x0010b380 (base 0x10b308), deleting slot 1 0x10b3f8

    virtual int   IsItem();             // +0x08, FF3 0x002abc48
    int           Create(char *strLabel);           // FF3 0x0010b478
    virtual void  Clear();              // +0x20, FF3 0x0010b4c0
    CEditRoot    *AddRoot(char *pStr, ...);         // FF3 0x0010b598
    IEditObject  *GetObject(char *strLabel, int n); // FF3 0x0010b698
    virtual char *TypeString();         // +0x24, FF3 0x002abc50

private:
    std::vector<IEditObject *> m_vpEditItem;        // EE +0x14 (allocator word, then +0x18 / +0x1c / +0x20)
};

#endif /* _GRAPHICS_GRAPH3D_CDEBUGEDIT_H */
