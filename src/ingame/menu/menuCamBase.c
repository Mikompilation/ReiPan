// ingame/menu/menuCamBase.c -- CmenuCamBase, the camera menu's page base.
// FF3 0x001105f0..0x001106d8 + 0x002ac018..0x002ac048; FF3-only (the class
// file sorts between CmemShelt and CmenuCamBasic; open-items.md
// "0x10bbb0..0x116fa0").
#include "ingame/menu/menuCamBase.h"

// ──────────────────────────────────────────────────────────────────────
// FF3 0x001105f0 is the same body.
// FF3 0x00110620 (MenuCamBaseCtor).
// callcheck: B2dBaseCtorBase (the CB2dBase base constructor runs implicitly)

CmenuCamBase::CmenuCamBase()
{
}

// ──────────────────────────────────────────────────────────────────────
// The base one FF3 0x00110650 is the same body, the deleting one
// FF3 0x001106a0 adds operator delete.
// FF3 0x00110678 (MenuCamBaseDtor).
// callcheck: B2dBaseDtorBase (the CB2dBase base destructor runs implicitly)

CmenuCamBase::~CmenuCamBase()
{
}

// ──────────────────────────────────────────────────────────────────────
// The defaults: a page without its own Step ends the menu at once.

int CmenuCamBase::Step()                // FF3 0x002ac018
{
    return -1;
}

int CmenuCamBase::Draw()                // FF3 0x002ac020
{
    return -1;
}

void CmenuCamBase::SetHooks()           // FF3 0x002ac028
{
}

void CmenuCamBase::Load()               // FF3 0x002ac030
{
}

void CmenuCamBase::Enter()              // FF3 0x002ac038
{
}

void CmenuCamBase::Setup()              // FF3 0x002ac040
{
}
