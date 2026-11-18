/*
 * Copyright (c) 2024-2026 xDev-Language
 *
 * Library: xDev MyTiff
 * File: mtcolor.prg
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "gfxdef.ch"
#include "gtinfo.ch"
#include "mytiff.ch"
#include "mtinternal.ch"

/* ---------------------------------------------------------------------------------------------
   Colors and drawing shared by every MyTiff widget
   --------------------------------------------------------------------------------------------- */

// The colors Motif derives from a background, like XmGetColors(): a dark background gets lighter
// shadows, a light one darker shadows, the foreground is black or white by the brightness
FUNCTION MtGetColors( nBackground )

   LOCAL nBright := s_Brightness( nBackground )
   LOCAL nBottom
   LOCAL nTop
   LOCAL nFore
   LOCAL nSelect

   DO CASE
   CASE nBright < 51
      nBottom := s_Lighter( nBackground, 30 )
      nTop    := s_Lighter( nBackground, 50 )
   CASE nBright > 237
      nBottom := s_Darker( nBackground, 40 )
      nTop    := s_Darker( nBackground, 20 )
   OTHERWISE
      nBottom := s_Darker( nBackground, 60 - Int( nBright * 20 / 255 ) )
      nTop    := s_Lighter( nBackground, 50 + Int( nBright * 10 / 255 ) )
   ENDCASE

   nFore := IIF( nBright > 178, 0x000000, 0xFFFFFF )
   nSelect := IIF( nBright < 51, s_Lighter( nBackground, 15 ), s_Darker( nBackground, 15 ) )

   RETURN { nFore, nTop, nBottom, nSelect }

/* ---------------------------------------------------------------------------------------------
   Inside MyTiff
   --------------------------------------------------------------------------------------------- */

// A Motif shadow nThick pixels wide on the rectangle: nTopColor on the top and the left side,
// nBottomColor on the bottom and the right side; swapping them draws it sunken
FUNCTION __MtDrawShadow( nTop, nLeft, nBottom, nRight, nThick, nTopColor, nBottomColor )

   LOCAL i

   FOR i := 0 TO nThick - 1
      GfxPrimitive( GFX_LINE, nTop + i, nLeft + i, nTop + i, nRight - i, nTopColor )
      GfxPrimitive( GFX_LINE, nTop + i, nLeft + i, nBottom - i, nLeft + i, nTopColor )
      GfxPrimitive( GFX_LINE, nBottom - i, nLeft + i + 1, nBottom - i, nRight - i, nBottomColor )
      GfxPrimitive( GFX_LINE, nTop + i + 1, nRight - i, nBottom - i, nRight - i, nBottomColor )
   NEXT

   RETURN NIL

// Text in pixels with the font of the GT; the font has a fixed width
FUNCTION __MtDrawText( nTop, nLeft, cText, nColor )

   GfxText( nTop, nLeft, cText, nColor, __MtFontHeight(), __MtFontWidth() )

   RETURN NIL

// Width of a text in pixels, the only place that measures text
FUNCTION __MtTextWidth( cText )

   RETURN ULen( cText ) * __MtFontWidth()

// Width of one character of the GT font in pixels
FUNCTION __MtFontWidth()

   RETURN GtInfo( GTI_FONTWIDTH )

// Height of the GT font in pixels
FUNCTION __MtFontHeight()

   RETURN GtInfo( GTI_FONTSIZE )

/* ---------------------------------------------------------------------------------------------
   Functions of this file
   --------------------------------------------------------------------------------------------- */

// Brightness 0 - 255: three quarters intensity, one quarter luminosity
STATIC FUNCTION s_Brightness( nColor )

   LOCAL nRed        := Int( nColor / 0x10000 ) % 0x100
   LOCAL nGreen      := Int( nColor / 0x100 ) % 0x100
   LOCAL nBlue       := nColor % 0x100
   LOCAL nIntensity  := Int( ( nRed + nGreen + nBlue ) / 3 )
   LOCAL nLuminosity := Int( ( 30 * nRed + 59 * nGreen + 11 * nBlue ) / 100 )

   RETURN Int( ( 75 * nIntensity + 25 * nLuminosity ) / 100 )

// Every channel nPercent of the way to black
STATIC FUNCTION s_Darker( nColor, nPercent )

   LOCAL nRed   := Int( nColor / 0x10000 ) % 0x100
   LOCAL nGreen := Int( nColor / 0x100 ) % 0x100
   LOCAL nBlue  := nColor % 0x100

   nRed := Int( nRed * ( 100 - nPercent ) / 100 )
   nGreen := Int( nGreen * ( 100 - nPercent ) / 100 )
   nBlue := Int( nBlue * ( 100 - nPercent ) / 100 )

   RETURN nRed * 0x10000 + nGreen * 0x100 + nBlue

// Every channel nPercent of the way to white
STATIC FUNCTION s_Lighter( nColor, nPercent )

   LOCAL nRed   := Int( nColor / 0x10000 ) % 0x100
   LOCAL nGreen := Int( nColor / 0x100 ) % 0x100
   LOCAL nBlue  := nColor % 0x100

   nRed += Int( ( 255 - nRed ) * nPercent / 100 )
   nGreen += Int( ( 255 - nGreen ) * nPercent / 100 )
   nBlue += Int( ( 255 - nBlue ) * nPercent / 100 )

   RETURN nRed * 0x10000 + nGreen * 0x100 + nBlue
