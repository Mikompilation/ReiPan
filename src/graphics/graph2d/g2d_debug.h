/* ==========================================================================
 *  graphics/graph2d/g2d_debug.h
 *
 *  FF3's small 2D primitive helpers over g2d_draw.c: filled rectangles and
 *  quads (DISP_SQAR through DispSqrD), 2D lines and circles (DIRECT LINE
 *  packets), and a sprite that shows a Z16S page. Only DispSqrRect has
 *  callers (fades and overlays in effects, the movie player, pre_Title_Mode,
 *  anicode); the rest is unreferenced debug drawing.
 *
 *  FF3 0x1c83d0..0x1c8e28 (FF2's g2d_debug.o slot, between framerate.o and
 *  g2d_draw.o; no FF2 counterparts except SetLine2DPacket in graphics.c).
 * ======================================================================== */
#ifndef _GRAPHICS_GRAPH2D_G2D_DEBUG_H
#define _GRAPHICS_GRAPH2D_G2D_DEBUG_H

#include "sdk/scetypes.h"

// Untextured rectangle (x0, y0)-(x1, y1) in screen pixels, z from pri
// (0xfffff - pri), ZBUF ZMSK = zmsk (1 = no depth write).
void DispSqrRectZmsk(float x0, float y0, float x1, float y1, u_int pri, int zmsk,
                     u_char r, u_char g, u_char b, u_char a);           // FF3 0x001c83d0
// The same with depth writes masked.
void DispSqrRect(float x0, float y0, float x1, float y1, u_int pri,
                 u_char r, u_char g, u_char b, u_char a);               // FF3 0x001c8528

// Untextured free quad, corners 0..3 in triangle-strip order (top-left,
// top-right, bottom-left, bottom-right).
void DispSqr4Zmsk(float x0, float y0, float x1, float y1, float x2, float y2,
                  float x3, float y3, u_int pri, int zmsk,
                  u_char r, u_char g, u_char b, u_char a);              // FF3 0x001c8560
void DispSqr4(float x0, float y0, float x1, float y1, float x2, float y2,
              float x3, float y3, u_int pri,
              u_char r, u_char g, u_char b, u_char a);                  // FF3 0x001c86f8

// Debug: the 640x448 PSMZ16S page at GS block tbp drawn full screen.
void DispZbufView(u_int tbp);                                           // FF3 0x001c8730

// 2D line, coordinates relative to the screen centre (0, 0 = pixel 320,
// 224); a is the blend's FIX alpha. pri <= 0 means 0x10.
void SetLine2D(float x0, float y0, float x1, float y1, int pri,
               u_char r, u_char g, u_char b, u_char a);                 // FF3 0x001c87d0
// SetLine2D with the colour dimmed by the line's slope.
void SetLine2DShade(float x0, float y0, float x1, float y1, int pri,
                    u_char r, u_char g, u_char b, u_char a);            // FF3 0x001c8968
// Only the LINE packet (screen pixels, z 1), with the draw state as it is.
void SetLine2DPacket(float x0, float y0, float x1, float y1,
                     u_char r, u_char g, u_char b, u_char a);           // FF3 0x001c8b78
// Circle outline of 36 SetLine2D segments around (cx, cy), centre-relative.
void SetCircle2D(float cx, float cy, float rad,
                 u_char r, u_char g, u_char b, u_char a);               // FF3 0x001c8c98

#endif /* _GRAPHICS_GRAPH2D_G2D_DEBUG_H */
