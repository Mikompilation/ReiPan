/* ==========================================================================
 *  system/playpss/my_strfile.c
 *
 *  The PSS stream reader playPssStart is handed (FF2 playpss.a my_strfile.o,
 *  ported from MioPan): three functions over system/eeiop/file_stream.c.
 *  MyStrRead posts one read to the IOP streamer, then answers 0 until it
 *  lands; pss_block_read says a read is in flight.
 *  FF3 0x271c30..0x271cb8.
 *
 *  PC: as in MioPan, MyStrStart opens the movie straight off the host
 *  (reipan/io/reipan_moviestream.h) and MyStrRead reads it in sequence; the
 *  IOP stream path (RPC 4, the ring and its reader thread) has no IOP side
 *  here. FF3's bodies still run when the file cannot be opened on the host.
 * ======================================================================== */
#include "system/playpss/my_strfile.h"

#include <stdio.h>                                      // printf

#include "reipan/io/reipan_moviestream.h"               // ReiPan_MovieStream*
#include "system/eeiop/cddat.h"                         // cddatIsCmpFile / GetFileNameBuffer / ...
#include "system/eeiop/file_stream.h"                   // FileStream*

static int pss_block_read;                              // sbss gp+0x5d0 (0x373ac0)

// FF3 0x00271c30
// callcheck: cddatIsCmpFile, GetFileNameBuffer, GetFileStartSector, GetFileSize,
// ReiPan_MovieStreamOpen, printf (the host's direct reader)

int MyStrStart(int file_no, int ring_buf_num, int one_buf_sector)
{
    pss_block_read = 0;

    // PC: the direct reader first. A compressed entry goes the FF3 way
    // (nothing streams one; this reader hands over raw bytes).
    if (cddatIsCmpFile(file_no) == 0)
    {
        ReiPan_MovieStreamReq req;
        char                  name[256];

        GetFileNameBuffer(file_no, name);
        req.file_name    = name;
        req.start_sector = GetFileStartSector(file_no);
        req.size         = (int)GetFileSize(file_no);
        if (ReiPan_MovieStreamOpen(&req) != 0)
            return 1;

        printf("movie: file %d not readable directly -- using the IOP stream path\n", file_no);
    }

    return FileStreamStart(file_no, ring_buf_num, one_buf_sector);
}

// FF3 0x00271c48: 0 while the read runs, size once it has landed. PC: the
// direct reader completes in the call.
// callcheck: ReiPan_MovieStreamIsOpen, ReiPan_MovieStreamRead (the host's direct reader)

int MyStrRead(void *buff, int size)
{
    int ret;

    if (ReiPan_MovieStreamIsOpen() != 0)
        return ReiPan_MovieStreamRead(buff, size);

    if (pss_block_read == 0)
    {
        FileStreamRead(buff, size, 0);
        pss_block_read = 1;
    }

    ret = 0;
    if (FileStreamIsAct() == 0)
    {
        pss_block_read = 0;
        ret = size;
    }
    return ret;
}

// FF3 0x00271ca0
// callcheck: ReiPan_MovieStreamIsOpen, ReiPan_MovieStreamClose (the host's direct reader)

int MyStrStop(void)
{
    if (ReiPan_MovieStreamIsOpen() != 0)
    {
        ReiPan_MovieStreamClose();
        return 1;
    }
    return FileStreamStop();
}
