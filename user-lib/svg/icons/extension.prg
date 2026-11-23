/*
 * Copyright (c) 2024-2026 xDev-Language
 *
 * Library...: xDev SVG
 * File......: extension.prg
 *
 */

#include "svg.ch"

PROCEDURE Main()

   LOCAL nSvg := svgInit( "extension.svg", 512, 512, 24 )

   svgPen( nSvg, 2, "currentColor", .T. )

   svgRect( nSvg, 3, 3, 9, 9, 1.5, NIL )
   svgRect( nSvg, 3, 12, 9, 9, 1.5, NIL )
   svgRect( nSvg, 12, 12, 9, 9, 1.5, NIL )

   svgPolygon( nSvg, { { 17.75, 1.85 }, { 22.15, 6.25 }, { 17.75, 10.65 }, { 13.35, 6.25 } }, 1.5 )

   svgClose( nSvg )

RETURN
