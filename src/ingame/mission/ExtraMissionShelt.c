// ingame/mission/ExtraMissionShelt.c -- CExtraMissionShelt, the story game
// kept aside over an extra mission, and the mission results carried back
// into it. FF3 0x0010b788..0x0010bba0; FF3-only (the class file sorts
// before CGameClear; open-items.md "0x10bbb0..0x116fa0").
#include "ingame/mission/ExtraMissionShelt.h"

#include "common/memShelt.h"                // CmemShelt
#include "ingame/ingame_wrk.h"              // PlayTimeReset, IngameWrkPointBackup
#include "ingame/item/prg/soul_list.h"      // SoulListSaveGet
#include "ingame/mission/mission_list.h"    // MissionSaveGet
#include "ingame/photo/finder_photo.h"      // GetAlbumSaveDisp, GetLensWrk
#include "ingame/photo/fndr_parts.h"        // LENS_WRK
#include "ingame/plyr/player.h"             // PlyrEquipBackup / Restore, PlyrEquipLvSaveGet
#include "ingame/plyr/plyr_gauge.h"         // PlyrGaugeInit
#include "outgame/option.h"                 // OptionWrkSaveGet
#include "system/mc/prg/mc_set_data.h"      // SetMemoryCardSaveDataToBuff, DevelopMemoryCardLoadData

// The story game's card data: directory 0, file 2.
#define EXTRA_SHELT_DIR     0
#define EXTRA_SHELT_FILE    2

static CExtraMissionShelt *extra_mission_shelt;     // sdata 0x36b620 (gp-0x7ed0)

// ──────────────────────────────────────────────────────────────────────
// The save blocks a mission's end keeps over the story game's reload
// (data 0x2efac0): a row per block, its shelter filled by CarryStore and
// emptied by CarryRestore; a null getter ends the table.
// layout: host -- two pointers, 8 bytes on the EE; only this file reads it.

struct MISSION_CARRY
{
    CmemShelt *shelt;                       // EE +0
    int      (*get)(void **addr);           // EE +4: the block's save getter, returns its size
};

static MISSION_CARRY mission_carry_tbl[] =
{
    { nullptr, MissionSaveGet },            // the mission records (0x3e7600, 0x1c0)
    { nullptr, OptionWrkSaveGet },          // the options (0x14)
    { nullptr, SoulListSaveGet },           // the soul list (0x3eb3b0, 0x1b2)
    { nullptr, PlyrEquipLvSaveGet },        // the equipment levels (0x373820, 7)
    { nullptr, nullptr },
};

// ──────────────────────────────────────────────────────────────────────
// FF3 0x0010b7f0 (CExtraMissionSheltCtor; FF3 0x0010b788 is the same body).
// callcheck: operator_new, MemSheltCtorSize (new CmemShelt(size))

CExtraMissionShelt::CExtraMissionShelt()
{
    saved = 0;
    shelt = new CmemShelt(GetMemoryCardDataSize(EXTRA_SHELT_DIR, EXTRA_SHELT_FILE));
    extra_mission_shelt = this;
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x0010b898 (CExtraMissionSheltDtor; the base one 0x0010b858 is the
// same body, the deleting one 0x0010b8d8 adds operator delete).

CExtraMissionShelt::~CExtraMissionShelt()
{
    if (shelt != nullptr)
        delete shelt;                       // vtable +4: deleting destructor
    extra_mission_shelt = nullptr;
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x0010b928.

CExtraMissionShelt *GetExtraMissionShelt(void)
{
    return extra_mission_shelt;
}

// ──────────────────────────────────────────────────────────────────────
// The story game into the buffer, then the mission starts afresh: the
// slow-mode gauge, the lens charge and the play clock.
// FF3 0x0010b930.

void CExtraMissionShelt::Save()
{
    saved = 1;
    SetMemoryCardSaveDataToBuff(shelt->buf, EXTRA_SHELT_DIR, EXTRA_SHELT_FILE);
    shelt->SetUsed(GetMemoryCardDataSize(EXTRA_SHELT_DIR, EXTRA_SHELT_FILE));
    PlyrGaugeInit(GetAlbumSaveDisp());
    GetLensWrk()->ClearCharge();
    PlayTimeReset();
}

// ──────────────────────────────────────────────────────────────────────
// Each block into a new shelter of its size.
// FF3 0x0010b9a8.
// callcheck: operator_new, MemSheltCtorSize (new CmemShelt(size))

static void MissionCarryStoreAll(MISSION_CARRY *tbl)
{
    MISSION_CARRY *row;
    void          *addr;
    int            size;

    for (row = tbl; row->get != nullptr; row++)
    {
        size = row->get(&addr);
        row->shelt = new CmemShelt(size);
        row->shelt->Store(addr, MEM_SHELT_ALL);
    }
}

// ──────────────────────────────────────────────────────────────────────
// Each block back from its shelter, which is deleted. FF3 does not check
// the shelter before Restore (CarryStore always ran before).
// FF3 0x0010ba58.

static void MissionCarryRestoreAll(MISSION_CARRY *tbl)
{
    MISSION_CARRY *row;
    void          *addr;

    for (row = tbl; row->get != nullptr; row++)
    {
        row->get(&addr);
        row->shelt->Restore(&addr, MEM_SHELT_ALL);
        if (row->shelt != nullptr)
            delete row->shelt;              // vtable +4: deleting destructor
        row->shelt = nullptr;
    }
}

// ──────────────────────────────────────────────────────────────────────
// The story game back from the buffer.
// FF3 0x0010baf8.

void CExtraMissionShelt::Load()
{
    DevelopMemoryCardLoadData(shelt->buf, EXTRA_SHELT_DIR, EXTRA_SHELT_FILE);
}

// ──────────────────────────────────────────────────────────────────────
// A mission ended or was quit: keep the blocks of mission_carry_tbl, the
// equipment, the lens and the points.
// FF3 0x0010bb20 (ExtraMissionSheltCarryStore).

void CExtraMissionShelt::CarryStore()
{
    MissionCarryStoreAll(mission_carry_tbl);
    PlyrEquipBackup();
    GetLensWrk()->SaveBackup();
    IngameWrkPointBackup(1);
}

// ──────────────────────────────────────────────────────────────────────
// After Load: what CarryStore kept goes back over the story game.
// FF3 0x0010bb60 (ExtraMissionSheltCarryRestore).

void CExtraMissionShelt::CarryRestore()
{
    MissionCarryRestoreAll(mission_carry_tbl);
    PlyrEquipRestore();
    GetLensWrk()->RestoreBackup();
    IngameWrkPointBackup(0);
}
