/*
 * xdev logo-xDev2.prg
 *
 */

#include "svg.ch"

#define COLOR_BACK "#070f17"
#define COLOR_X    "#264d73"
#define COLOR_D    "#73264d"
#define COLOR_E    "#4d7326"
#define COLOR_V    "#267373"

PROCEDURE Main()

   LOCAL nWidth   := 512, nHeight := 512
   LOCAL nScale   := nWidth / 1024
   LOCAL nLetters := 824 * nScale, nHeightLetters := 180 * nScale
   LOCAL nLeft    := ( nWidth - nLetters ) / 2
   LOCAL nTop     := ( nHeight - nHeightLetters ) / 2
   LOCAL nArm     := 201 / 2 / Sqrt( 2 ) * nScale
   LOCAL nCenterX := nLeft + 93 * nScale
   LOCAL nCenterY := nTop + 90 * nScale
   LOCAL nBottom  := nTop + nHeightLetters
   LOCAL aTriangle
   LOCAL nSvg

   nSvg := svg_Init( "logo-xDev2.svg", nWidth, nHeight )

   svg_Comment( nSvg, "0x070f17 0x264d73 0x73264d 0x4d7326 0x262773 0x267373 0x734c26 0xe9edf1" )

   svg_FilledRect( nSvg, 0, 0, nWidth, nHeight, COLOR_BACK )

   // X: two bars at 45 degrees around the centre of the letter; a line has square ends
   svg_Line( nSvg, nCenterX - nArm, nCenterY - nArm, nCenterX + nArm, nCenterY + nArm, 61 * nScale, COLOR_X )
   svg_Line( nSvg, nCenterX - nArm, nCenterY + nArm, nCenterX + nArm, nCenterY - nArm, 61 * nScale, COLOR_X )

   // D: a block and a circle whose left half lies inside the block
   svg_FilledRect( nSvg, nLeft + 210 * nScale, nTop, 90 * nScale, nHeightLetters, COLOR_D )
   svg_FilledCircle( nSvg, nLeft + 300 * nScale, nCenterY, 90 * nScale, COLOR_D )

   // E: three bars
   svg_FilledRect( nSvg, nLeft + 414 * nScale, nTop, 178 * nScale, 48 * nScale, COLOR_E )
   svg_FilledRect( nSvg, nLeft + 414 * nScale, nTop + 66 * nScale, 178 * nScale, 48 * nScale, COLOR_E )
   svg_FilledRect( nSvg, nLeft + 414 * nScale, nTop + 132 * nScale, 178 * nScale, 48 * nScale, COLOR_E )

   // V: a triangle pointing down
   aTriangle := { { nLeft + 616 * nScale, nTop }, { nLeft + nLetters, nTop }, { nLeft + 720 * nScale, nBottom } }
   svg_FilledPolygon( nSvg, aTriangle, COLOR_V )

   svg_Close( nSvg )

RETURN
