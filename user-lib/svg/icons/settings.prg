/*
 * Copyright (c) 2024-2026 xDev-Language
 *
 * Library...: xDev SVG
 * File......: settings.prg
 *
 */

#include "svg.ch"

PROCEDURE Main()

   LOCAL nSvg    := svgInit( "settings.svg", 512, 512, 24 )
   LOCAL aPoints := {}
   LOCAL nTooth
   LOCAL nAngle

   svgPen( nSvg, 2, "currentColor", .T. )

   FOR nTooth := 0 TO 5
      nAngle := -90 + nTooth * 60
      AAdd( aPoints, s_Point( 7, nAngle - 18 ) )
      AAdd( aPoints, s_Point( 9.5, nAngle - 12 ) )
      AAdd( aPoints, s_Point( 9.5, nAngle + 12 ) )
      AAdd( aPoints, s_Point( 7, nAngle + 18 ) )
   NEXT

   svgPolygon( nSvg, aPoints, 1 )
   svgCircle( nSvg, 12, 12, 3 )

   svgClose( nSvg )

RETURN

STATIC FUNCTION s_Point( nR, nAngle )

   RETURN { 12 + nR * Cos( DToR( nAngle ) ), 12 + nR * Sin( DToR( nAngle ) ) }
