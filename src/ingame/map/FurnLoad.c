// ingame/map/FurnLoad.c -- FF3 0x116fa0..0x117720: the furniture key table,
// the furniture records of the two room buffers and the object-name parsers.
// FF2's FurnLoad.c has the key table; the records replace its CBuff pass
// (MapDraw.c loads the models and fills them).

#include <stdlib.h>                     // atoi
#include <string.h>                     // strlen, strncmp, strstr, strcpy, memset

#include "ingame/map/FurnLoad.h"

// ──────────────────────────────────────────────────────────────────────
// The name prefixes (data 0x34dbf0; keys in sdata 0x36b790.., "area_pos"
// rodata 0x34dbe0). Every "f" key starts at F000_PK4: the three digits pick
// the file, the second letter the kind of object.

static FURN_LOAD_TBL furn_key_tbl[FURN_KEY_NUM] =
{
    { "f0",       0xadf, FURN_ATTR_FURN0 },
    { "f1",       0xadf, FURN_ATTR_FURN0 },
    { "f2",       0xadf, FURN_ATTR_FURN0 },
    { "f3",       0xadf, FURN_ATTR_FURN0 },
    { "f4",       0xadf, FURN_ATTR_FURN1 },
    { "f5",       0xadf, FURN_ATTR_FURN2 },
    { "f6",       0xadf, FURN_ATTR_FURN3 },
    { "f7",       0xadf, FURN_ATTR_EFFECT },
    { "f8",       0xadf, FURN_ATTR_EFFECT },
    { "f9",       0xadf, FURN_ATTR_EFFECT },
    { "d",        0x1590, FURN_ATTR_DOOR },
    { "e",        -1,    FURN_ATTR_EVENT },
    { "area_pos", -1,    FURN_ATTR_AREA_POS },
};

static FURN_LOAD_REC furn_load_rec[FURN_LOAD_REC_NUM];  // bss 0x379a48

// ──────────────────────────────────────────────────────────────────────
// The key that starts name, nullptr. FF3 0x00116fa0 (FF2 printed an error).

static FURN_LOAD_TBL *FurnLoadGetKeyPtr(const char *name)
{
    for (int i = 0; i < FURN_KEY_NUM; i++)
    {
        if (strncmp(name, furn_key_tbl[i].key, strlen(furn_key_tbl[i].key)) == 0)
            return &furn_key_tbl[i];
    }
    return nullptr;
}

// FF3 0x00117020.
int FurnLoadGetKeyLabel(const char *name)
{
    FURN_LOAD_TBL *key = FurnLoadGetKeyPtr(name);

    return key != nullptr ? key->label : -1;
}

// FF3 0x00117050.
int FurnLoadGetAttr(const char *name)
{
    FURN_LOAD_TBL *key = FurnLoadGetKeyPtr(name);

    return key != nullptr ? key->attr : -1;
}

// ──────────────────────────────────────────────────────────────────────
// Room's record of name: the first four characters decide.
// FF3 0x00117080.

static FURN_LOAD_REC *FurnLoadFindRec(int room, const char *name)
{
    for (FURN_LOAD_REC *rec = furn_load_rec; rec < &furn_load_rec[FURN_LOAD_REC_NUM]; rec++)
    {
        if (rec->room == room && rec->name[0] == name[0] && rec->name[1] == name[1] &&
            rec->name[2] == name[2] && rec->name[3] == name[3])
        {
            return rec;
        }
    }
    return nullptr;
}

// FF3 0x001170f8.
int FurnLoadIsRec(int room, const char *name)
{
    return FurnLoadFindRec(room, name) != nullptr;
}

// FF3 0x00117118.
int FurnLoadSetRecFlag(int room, const char *name, u_short flag)
{
    FURN_LOAD_REC *rec = FurnLoadFindRec(room, name);

    if (rec == nullptr)
        return -1;

    rec->flag |= flag;
    return 0;
}

// ──────────────────────────────────────────────────────────────────────
// 1 when the object still wants its prelight: it has a record not marked
// FURN_REC_PRELIT, and is not furniture 49.
// FF3 0x00117160.

int FurnLoadCallFunc(int room, const char *name)
{
    FURN_LOAD_REC *rec = FurnLoadFindRec(room, name);

    if (rec == nullptr || (rec->flag & FURN_REC_PRELIT) != 0)
        return 0;

    if (name[0] != 'f')
        return 1;

    return FurnNameGetNo(name) != 49;
}

// FF3 0x001171c8.
static void FurnLoadClearRoomFlag(int room, u_short flag)
{
    for (FURN_LOAD_REC *rec = furn_load_rec; rec < &furn_load_rec[FURN_LOAD_REC_NUM]; rec++)
    {
        if (rec->room == room)
            rec->flag &= ~flag;
    }
}

// FF3 0x00117208.
void FurnLoadClearPrelit(int room)
{
    FurnLoadClearRoomFlag(room, FURN_REC_PRELIT);
}

// FF3 0x00117220.
void FurnLoadClearFlag1(int room)
{
    FurnLoadClearRoomFlag(room, FURN_REC_FLAG1);
}

// ──────────────────────────────────────────────────────────────────────
// Record getters / setters.

// FF3 0x00117238 (no callers).
int FurnLoadGetSize(int room, const char *name)
{
    FURN_LOAD_REC *rec = FurnLoadFindRec(room, name);

    return rec != nullptr ? rec->size : 0;
}

// FF3 0x00117268.
void *FurnLoadGetMdlPk4(int room, const char *name)
{
    FURN_LOAD_REC *rec = FurnLoadFindRec(room, name);

    return rec != nullptr ? rec->mdl_pk4 : nullptr;
}

// FF3 0x00117298.
void *FurnLoadGetMotPk4(int room, const char *name)
{
    FURN_LOAD_REC *rec = FurnLoadFindRec(room, name);

    return rec != nullptr ? rec->mot_pk4 : nullptr;
}

// FF3 0x001172c8 (no callers).
u_short *FurnLoadGetFlagPtr(int room, const char *name)
{
    FURN_LOAD_REC *rec = FurnLoadFindRec(room, name);

    return rec != nullptr ? &rec->flag : nullptr;
}

// FF3 0x001172f0. FF3 writes through the lookup without a null test; a
// missing record cannot happen (MapDrawLoadFurnMot runs after
// FurnLoadAddRec), PC: tested anyway.
void FurnLoadSetMotPk4(int room, const char *name, void *mot)
{
    FURN_LOAD_REC *rec = FurnLoadFindRec(room, name);

    if (rec != nullptr)
        rec->mot_pk4 = mot;
}

// ──────────────────────────────────────────────────────────────────────
// Name parsers.

// FF3 0x00117318 (FUN_002a0380 is isdigit).
// callcheck: isdigit (the '0'..'9' test, the same in the C locale)
int FurnNameGetNum(const char *str, int start, int len)
{
    int num = 0;

    for (int i = start; i < start + len; i++)
    {
        if (str[i] < '0' || str[i] > '9')
            return -1;

        num = num * 10 + str[i] - '0';
    }
    return num;
}

// FF3 0x001173b0.
int FurnNameGetNo(const char *name)
{
    int no;

    if (name[0] != 'd' && name[0] != 'f')
        return -1;

    no = FurnNameGetNum(name, 1, 3);
    return no < 0 ? -1 : no;
}

// ──────────────────────────────────────────────────────────────────────
// The model file of an object name: the key's first file plus the number,
// inside F000_PK4 + 1 .. 0xec6 ("f") or 0x1590 .. 0x1977 ("d").
// FF3 0x001173f8.

int FurnNameGetFileNo(const char *name)
{
    int label = FurnLoadGetKeyLabel(name);
    int no;
    int file;
    int lo;
    int hi;

    if (label < 0)
        return -1;

    no = FurnNameGetNo(name);
    file = label + no;
    if (no < 0)
        return -2;

    if (name[0] == 'd')
    {
        lo = 0x158f;
        hi = 0x1978;
    }
    else if (name[0] == 'f')
    {
        lo = 0xade;
        hi = 0xec7;
    }
    else
    {
        return -3;                      // (no other key has a label)
    }

    if (file <= lo || file >= hi)
        return -3;

    return file;
}

// FF3 0x00117498: "e012_no34" -> 34.
int FurnNameGetEventNo(const char *name)
{
    const char *p;

    if (name[0] == 'e' && (p = strstr(name, "_no")) != nullptr)     /* sdata 36b780 */
        return atoi(p + 3);

    return 0;
}

// ──────────────────────────────────────────────────────────────────────
// The "_z" suffix: a letter (L 0, N 1, F 2; either case), an optional '-'
// and three digits. *kind 3 and 0 when there is none or it is malformed.
// MapDrawRoomObjRegist turns it into the put's draw order or z offset.
// FF3 0x001174e0.

int FurnNameGetZ(const char *name, int *kind)
{
    const char *p = strstr(name, "_z");                             /* sdata 36b788 */
    int         sign;
    int         num;

    *kind = 3;
    if (p == nullptr)
        return 0;

    p += strlen("_z");
    switch (*p)
    {
    case 'F':
    case 'f':
        *kind = 2;
        break;
    case 'L':
    case 'l':
        *kind = 0;
        break;
    case 'N':
    case 'n':
        *kind = 1;
        break;
    default:
        return 0;
    }

    sign = 1;
    if (p[1] == '-')
    {
        sign = -1;
        p++;
    }

    num = FurnNameGetNum(p, 1, 3);
    if (num < 0)
    {
        *kind = 3;
        return 0;
    }
    return num * sign;
}

// ──────────────────────────────────────────────────────────────────────
// Records.

// A free record, cleared, for room; nullptr when all are used.
// FF3 0x001175d0.
static FURN_LOAD_REC *FurnLoadNewRec(int room)
{
    for (FURN_LOAD_REC *rec = furn_load_rec; rec < &furn_load_rec[FURN_LOAD_REC_NUM]; rec++)
    {
        if (rec->room < 0)
        {
            memset(rec, 0, sizeof(*rec));
            rec->room = room;
            rec->flag = 0;
            return rec;
        }
    }
    return nullptr;
}

// FF3 0x00117640.
int FurnLoadAddRec(int room, const char *name, void *mdl, int size)
{
    FURN_LOAD_REC *rec = FurnLoadNewRec(room);

    if (rec == nullptr)
        return -1;

    strcpy(rec->name, name);
    rec->size    = size;
    rec->mdl_pk4 = mdl;
    return 0;
}

// FF3 0x001176b0.
void FurnLoadDeleteRoom(int room)
{
    for (FURN_LOAD_REC *rec = furn_load_rec; rec < &furn_load_rec[FURN_LOAD_REC_NUM]; rec++)
    {
        if (rec->room == room)
            rec->room = -1;
    }
}

// FF3 0x001176e8.
void FurnLoadInitRec(void)
{
    for (FURN_LOAD_REC *rec = furn_load_rec; rec < &furn_load_rec[FURN_LOAD_REC_NUM]; rec++)
        rec->room = -1;
}
