/*
 * xdev logo-xDev.prg
 */

#include "svg.ch"

PROCEDURE Main()

   LOCAL pSvg
   LOCAL nWidth := 512, nHeight := 512

   pSvg := svg_Init( "logo-xDev.svg", nWidth, nHeight )

   svg_Comment( pSvg, "#264d73 #73264d #4d7326 #262773 #267373 #734c26" )

   svg_FilledCircle( pSvg, nWidth / 2, nHeight / 2, 255, "#73264d" )

   svg_Line( pSvg, 42, ( nHeight - 250 ) / 2, nWidth - 42, ( nHeight - 250 ) / 2, 30, "#323232" )

   svg_FilledRect( pSvg, 25, ( nHeight - 220 ) / 2, nWidth - 50, 180, "#F2F2F2" )

   svg_Line( pSvg, 25, ( nHeight + 141 ) / 2, nWidth - 25, ( nHeight + 141 ) / 2, 30, "#323232" )

   svg_Circle( pSvg, nWidth / 2, nHeight / 2, 256 - 15, 30, "#323232" )

   svg_Text( pSvg, ( nWidth - 4 * ( 110 * 0.8 ) ) / 2 + 25, ( nHeight + 40 ) / 2, "xDev", "Times New Roman", 140, FONT_WEIGHT_BOLD, "#0C0C0C" )

   svg_Close( pSvg )

RETURN
