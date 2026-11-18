/*
 * Copyright (c) 2024-2026 xDev-Language
 *
 * Library: xDev Runtime
 * File: gfxdef.h
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#ifndef GFXDEF_H
#define GFXDEF_H

/* C counterpart of include/xdev/gfxdef.ch; keep the order, ENUM starts at 1 */
enum X_GFX
{
   GFX_ACQUIRESCREEN = 1,
   GFX_RELEASESCREEN,
   GFX_MAKECOLOR,
   GFX_CLIPTOP,
   GFX_CLIPLEFT,
   GFX_CLIPBOTTOM,
   GFX_CLIPRIGHT,
   GFX_SETCLIP,
   GFX_DRAWINGMODE,
   GFX_GETPIXEL,
   GFX_PUTPIXEL,
   GFX_LINE,
   GFX_RECT,
   GFX_FILLEDRECT,
   GFX_CIRCLE,
   GFX_FILLEDCIRCLE,
   GFX_ELLIPSE,
   GFX_FILLEDELLIPSE,
   GFX_FLOODFILL
};

/* The drawing modes of GFX_DRAWINGMODE */
#define GFX_MODE_SOLID 1
#define GFX_MODE_XOR 2
#define GFX_MODE_ALPHA 3

#endif /* GFXDEF_H */
