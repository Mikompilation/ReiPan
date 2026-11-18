/* ==========================================================================
 *  system/playpss/pss_loop.c
 *
 *  The PSS loop reader (see pss_loop.h). FF3 0x219b48..0x219e60.
 * ======================================================================== */
#include "system/playpss/pss_loop.h"

#include <string.h>                                     // memcpy

#include "common/heapctrl.h"                            // heapCtrlMalloc / heapCtrlFree
#include "system/eeiop/cddat.h"                         // GetFileSize
#include "system/eeiop/fileload.h"                      // FileLoadReqEE / FileLoadIsEnd / FileLoadCancel
#include "system/os/sys_heap.h"                         // GetSystemHeapWrkP
#include "system/playpss/my_strfile.h"                  // MyStrStart / MyStrRead / MyStrStop

// ──────────────────────────────────────────────────────────────────────
// Data (sbss gp+0x204..gp+0x224, 0x3736f4..0x373718: zero at boot)
// ──────────────────────────────────────────────────────────────────────

static int    pss_loop_head_load;       // the head's FileLoadReqEE id, -1 when done
static int    pss_loop_head_size;       // GetFileSize(file + 1)
static u_char *pss_loop_head_buf;       // the head, whole
static int    pss_loop_head_pos;        // next chunk to read, -1: not rewound yet
static int    pss_loop_head_chunks;     // head size / read size, -1: not known yet
static int    pss_loop_file;            // MyStrStart's arguments
static int    pss_loop_ring_buf_num;
static int    pss_loop_one_buf_sector;
static int    pss_loop_from_head;       // reads come from the head (after a rewind)

// ──────────────────────────────────────────────────────────────────────
// The in-memory head
// ──────────────────────────────────────────────────────────────────────

// FF3 0x00219b48: file + 1 into a system-heap buffer of its size.

void PssLoopHeadLoad(int file)
{
    pss_loop_head_size = GetFileSize(file + 1);
    pss_loop_head_buf  = (u_char *)heapCtrlMalloc(GetSystemHeapWrkP(), pss_loop_head_size, 0, 0, 0);
    pss_loop_head_load = FileLoadReqEE(file + 1, pss_loop_head_buf, 5, 0, 0);
    pss_loop_head_chunks = -1;
    pss_loop_head_pos    = -1;
}

// FF3 0x00219bc0

int PssLoopHeadIsLoaded(void)
{
    if (pss_loop_head_load != -1)
    {
        if (FileLoadIsEnd(pss_loop_head_load) == 0)
            return 0;
        pss_loop_head_load = -1;
    }
    return 1;
}

// The head is read in chunks of the decoder's read size; their number is
// taken on the first read. FF3 0x00219c08 (FF3 traps on size 0).

void PssLoopHeadSetChunk(int size)
{
    if (pss_loop_head_chunks == -1)
        pss_loop_head_chunks = pss_loop_head_size / size;
}

// FF3 0x00219c38

int PssLoopHeadRewind(void)
{
    int pos;

    pos = pss_loop_head_pos;
    pss_loop_head_pos = 0;
    return pos == -1;
}

// FF3 0x00219c50: the next chunk while there is one; 0 past the end (the
// decoder's end of stream).

int PssLoopHeadRead(void *buff, int size)
{
    int pos;

    if (pss_loop_head_buf == nullptr)
        return 0;
    if (pss_loop_head_pos == -1 || pss_loop_head_chunks <= pss_loop_head_pos)
        return 0;

    pos = pss_loop_head_pos++;
    memcpy(buff, pss_loop_head_buf + size * pos, size);
    return size;
}

// FF3 0x00219cb0

void PssLoopHeadRelease(void)
{
    if (pss_loop_head_buf == nullptr)
        return;

    if (pss_loop_head_load != -1)
    {
        FileLoadCancel(pss_loop_head_load, 0, 0);
        pss_loop_head_load = -1;
    }
    heapCtrlFree(GetSystemHeapWrkP(), pss_loop_head_buf, 0, 0, 0);
    pss_loop_head_buf = nullptr;
}

// ──────────────────────────────────────────────────────────────────────
// The reader
// ──────────────────────────────────────────────────────────────────────

// Remember the stream and start loading the head; reads begin on the disc.
// FF3 0x00219d18

int PssLoopSet(int file, int ring_buf_num, int one_buf_sector)
{
    pss_loop_from_head      = 0;
    pss_loop_file           = file;
    pss_loop_ring_buf_num   = ring_buf_num;
    pss_loop_one_buf_sector = one_buf_sector;
    PssLoopHeadLoad(file);
    return 1;
}

// FF3 0x00219d48: 0 while the head loads, then the disc stream (retried
// until the stream system takes it) and 1.

int PssLoopStart(void)
{
    if (PssLoopHeadIsLoaded() == 0)
        return 0;

    while (MyStrStart(pss_loop_file, pss_loop_ring_buf_num, pss_loop_one_buf_sector) == 0)
        ;
    return 1;
}

// FF3 0x00219d90

int PssLoopRead(void *buff, int size)
{
    PssLoopHeadSetChunk(size);
    if (pss_loop_from_head == 0)
        return MyStrRead(buff, size);
    return PssLoopHeadRead(buff, size);
}

// At the end of a stream: from the disc, switch to the head and restart the
// disc stream from its top; from the head, back to the (restarted) disc.
// FF3 0x00219df0

void PssLoopRewind(void)
{
    if (pss_loop_from_head != 0)
    {
        pss_loop_from_head = 0;
        return;
    }

    pss_loop_from_head = 1;
    MyStrStop();
    while (MyStrStart(pss_loop_file, pss_loop_ring_buf_num, pss_loop_one_buf_sector) == 0)
        ;
    PssLoopHeadRewind();
}

// FF3 0x00219e40

void PssLoopEnd(void)
{
    PssLoopHeadRelease();
    MyStrStop();
}
