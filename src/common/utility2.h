/* ==========================================================================
 *  common/utility2.h
 *
 *  Small shared helpers: alignment, string offsets, ring-buffer indices,
 *  clamps, the assert / warning reporters (empty in FF3's release build) and
 *  the rotation fold. FF3 keeps FF2's utility2 object (0x268ac0..0x268e70)
 *  minus Get2Power, GetRandValF / GetRandValI, SetAssertPreMessage and
 *  PrintBit. Only GetAlignUp and StrToLower are called in the ROM; the rest
 *  are unreferenced.
 *
 *  The IOP has its own copy (system/iop/utility2i.h), renamed with an Iop
 *  prefix so both link into the host binary.
 * ======================================================================== */
#ifndef _COMMON_UTILITY2_H
#define _COMMON_UTILITY2_H

/* ---- alignment --------------------------------------------------------- */
unsigned int GetAlignUp(unsigned int a, int power);     /* round up to 1<<power (FF3 0x00268ac0) */

/* ---- strings ----------------------------------------------------------- */
int   GetOffsetChar(char *string, char chr);            /* first chr, or -1 (FF3 0x00268ae0) */
int   GetStrLen(char *str);                             /* FF3 0x00268b10 */
int   GetOffsetLastChar(char *string, int len, char chr); /* last chr in len bytes, or -1 (FF3 0x00268b48) */
int   FileExtensionMatch(char *filename, char *c_extent); /* c_extent without the dot (FF3 0x00268ba8) */
int   GetExtension(char *filename);                     /* 4 bytes after the first '.' (FF3 0x00268c20) */
char *StrToLower(char *str);                            /* in place, A-Z only; returns str (FF3 0x00268d48) */
char *GetNameWithoutPath(char *path_file);              /* past the last '/' (FF3 0x00268db0) */

/* ---- ring-buffer index arithmetic -------------------------------------- */
int   RingBufAdd(int idx, int buf_num);                 /* FF3 0x00268c60 */
int   RingBufCalcDiff(int former, int later, int num);  /* FF3 0x00268c78 */

/* ---- clamps (upper bound before the lower one) ------------------------- */
float GetClampValF(float val, float upper, float lower); /* FF3 0x00268ca0 */
int   GetClampVal(int val, int upper, int lower);       /* FF3 0x00268cc8 */

/* ---- reporting: empty bodies in FF3 ------------------------------------ */
void  DumpMemReal(unsigned int adrs, int size, char *name); /* FF3 0x00268c98 */
void  SetPrintWarning(void (*func)(char *));            /* FF3 0x00268ce8 */
void  SetPrintAssert(void (*func)(char *));             /* FF3 0x00268cf0 */
void  PrintWarningReal(char *str, ...);                 /* FF3 0x00268cf8 */
void  PrintAssertReal(char *str, ...);                  /* FF3 0x00268d20 */

/* ---- rotation ---------------------------------------------------------- */
float RotLimitChk2(float rot);                          /* fold into [-PI, PI], one step (FF3 0x00268e18) */

#endif /* _COMMON_UTILITY2_H */
