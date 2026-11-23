/*
 * Copyright (c) 2024-2026 xDev-Language
 *
 * Library...: xDev Documentation
 * File......: logo-xDev.prg
 *
 */

#include "svg.ch"

PROCEDURE Main()

   LOCAL pSvg
   LOCAL nWidth := 512, nHeight := 512

   pSvg := svgInit( "logo-xDev.svg", nWidth, nHeight, nWidth )

   svgComment( pSvg, "0x070F17 0x264D73 0x73264D 0x4D7326 0x262773 0x267373 0x734C26 0xE9EDF1" )

   svgFilledCircle( pSvg, nWidth / 2, nHeight / 2, 255, "#264d73" )

   svgPen( pSvg, 30, "#323232" )

   svgLine( pSvg, 42, ( nHeight - 250 ) / 2, nWidth - 42, ( nHeight - 250 ) / 2 )

   svgFilledRect( pSvg, 25, ( nHeight - 220 ) / 2, nWidth - 50, 180, "#F2F2F2" )

   svgLine( pSvg, 25, ( nHeight + 141 ) / 2, nWidth - 25, ( nHeight + 141 ) / 2 )

   svgCircle( pSvg, nWidth / 2, nHeight / 2, 256 - 15 )

   svgText( pSvg, ( nWidth - 4 * ( 110 * 0.8 ) ) / 2 + 25, ( nHeight + 40 ) / 2, ;
            "xDev", "Times New Roman", 140, FONT_WEIGHT_BOLD, "#0C0C0C" )

   svgClose( pSvg )

RETURN


