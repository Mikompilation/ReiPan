/* ==========================================================================
 *  outgame/boot.h
 *
 *  FF3-only boot phases, FF3 0x100388..0x100b80 (the first game object):
 *    3  Boot_Main      container of 13 and 14; its one goes straight to 13
 *    13 Boot_Load      boot data files and subsystem data -> 14
 *    14 AutoLoad_Main  memory-card auto load (outgame/autoload.c) -> 4
 *    4  Logo_Main      the three logos of layout 0x122f ("T2", "TECMO",
 *                      "ZERO"), each shown 75 (PAL) / 90 frames or until a
 *                      button -> 15 (Title_Top); on PC any button skips
 *                      all three
 *  FF2 had these in several files (FF2 outgame/logo.c, autoload.c); FF3's file
 *  name is unknown, boot.c is ReiPan's.
 * ======================================================================== */
#ifndef _OUTGAME_BOOT_H
#define _OUTGAME_BOOT_H

int  init_Boot_Main(void);
int  one_Boot_Main(void);
int  end_Boot_Main(void);
void pre_Boot_Main(void);
void after_Boot_Main(void);

int init_Boot_Load(void);
int one_Boot_Load(void);
int end_Boot_Load(void);

int init_AutoLoad_Main(void);
int one_AutoLoad_Main(void);
int end_AutoLoad_Main(void);

int init_Logo_Main(void);
int one_Logo_Main(void);
int end_Logo_Main(void);

#endif /* _OUTGAME_BOOT_H */
