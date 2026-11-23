/*
 * Copyright (c) 2024-2026 xDev-Language
 *
 * Library...: xDev SVG
 * File......: svg.prg
 *
 */

// The pen of each open file: nSvg => { nStrokeWidth, cColor, lRound }
STATIC s_hPens := { => }

/* ---------------------------------------------------------------------------------------------
   xDev SVG API
   --------------------------------------------------------------------------------------------- */

// Create an SVG file of nWidth x nHeight, drawn in 0 to nViewSize; 0 when the file cannot be created
// svgInit( cFilename, nWidth, nHeight, [nViewSize] ) --> nSvg
FUNCTION svgInit( cFilename AS string, nWidth AS int, nHeight AS int, nViewSize ) AS int

   LOCAL nSvg    := FCreate( cFilename )
   LOCAL cWidth  := LTrim( Str( nWidth ) )
   LOCAL cHeight := LTrim( Str( nHeight ) )
   LOCAL cTag

   IF nSvg == -1
      RETURN 0
   ENDIF

   cTag := '<svg xmlns="http://www.w3.org/2000/svg" width="' + cWidth + '" height="' + cHeight + '"'

   IF ValType( nViewSize ) == 'N' .AND. nViewSize > 0
      cTag += ' viewBox="0 0 ' + LTrim( Str( nViewSize ) ) + ' ' + LTrim( Str( nViewSize ) ) + '"'
   ELSE
      cTag += ' viewBox="0 0 ' + cWidth + ' ' + cHeight + '"'
   ENDIF

   FWrite( nSvg, e'<?xml version="1.0" encoding="UTF-8"?>\n' )
   FWrite( nSvg, '<!DOCTYPE svg PUBLIC "-//W3C//DTD SVG 1.1//EN" ' )
   FWrite( nSvg, e'"http://www.w3.org/Graphics/SVG/1.1/DTD/svg11.dtd">\n' )
   FWrite( nSvg, cTag + e'>\n' )

   s_hPens[ nSvg ] := { 1, 'black', .F. }

   RETURN nSvg

// End the drawing and close the file
// svgClose( nSvg ) --> NIL
PROCEDURE svgClose( nSvg AS int )

   FWrite( nSvg, e'</svg>\n' )
   FClose( nSvg )
   HDel( s_hPens, nSvg )

   RETURN

// Set the pen for the outlines drawn after it; lRound gives round line ends and corners
// svgPen( nSvg, nStrokeWidth, cColor, [lRound] ) --> NIL
PROCEDURE svgPen( nSvg AS int, nStrokeWidth AS float64, cColor AS string, lRound )

   s_hPens[ nSvg ] := { nStrokeWidth, cColor, ValType( lRound ) == 'L' .AND. lRound }

   RETURN

// A comment in the file; "--" gets a space between, as XML does not allow it in a comment
// svgComment( nSvg, cComment ) --> NIL
PROCEDURE svgComment( nSvg AS int, cComment AS string )

   LOCAL cSafe := ''
   LOCAL cChar
   LOCAL nIndex

   FOR nIndex := 1 TO Len( cComment )
      cChar := SubStr( cComment, nIndex, 1 )
      cSafe += cChar

      IF cChar == '-' .AND. SubStr( cComment, nIndex + 1, 1 ) == '-'
         cSafe += ' '
      ENDIF
   NEXT

   IF Right( cSafe, 1 ) == '-'
      cSafe += ' '
   ENDIF

   FWrite( nSvg, '<!-- ' + cSafe + ' -->' + e'\n' )

   RETURN

// A circle filled with cColor
// svgFilledCircle( nSvg, nCx, nCy, nR, cColor ) --> NIL
PROCEDURE svgFilledCircle( nSvg AS int, nCx AS float64, nCy AS float64, nR AS float64, cColor AS string )

   LOCAL cTag := '<circle cx="' + s_Number( nCx ) + '" cy="' + s_Number( nCy ) + '" r="' + s_Number( nR ) + '"'

   FWrite( nSvg, cTag + ' fill="' + cColor + e'"/>\n' )

   RETURN

// A line drawn with the pen; cColor overrides its color for this line
// svgLine( nSvg, nX1, nY1, nX2, nY2, [cColor] ) --> NIL
PROCEDURE svgLine( nSvg AS int, nX1 AS float64, nY1 AS float64, nX2 AS float64, nY2 AS float64, cColor )

   LOCAL cTag := '<line x1="' + s_Number( nX1 ) + '" y1="' + s_Number( nY1 ) + '"'

   cTag += ' x2="' + s_Number( nX2 ) + '" y2="' + s_Number( nY2 ) + '"'
   cTag += s_Stroke( nSvg, cColor )
   FWrite( nSvg, cTag + e'/>\n' )

   RETURN

// A rectangle filled with cColor; nRadius rounds the corners
// svgFilledRect( nSvg, nX, nY, nWidth, nHeight, cColor, [nRadius] ) --> NIL
PROCEDURE svgFilledRect( nSvg AS int, nX AS float64, nY AS float64, nWidth AS float64, nHeight AS float64, ;
                         cColor AS string, nRadius )

   LOCAL cTag := s_RectTag( nX, nY, nWidth, nHeight, nRadius )

   FWrite( nSvg, cTag + ' fill="' + cColor + e'"/>\n' )

   RETURN

// The outline of a rectangle drawn with the pen; nRadius rounds the corners, cColor overrides the pen color
// svgRect( nSvg, nX, nY, nWidth, nHeight, [nRadius], [cColor] ) --> NIL
PROCEDURE svgRect( nSvg AS int, nX AS float64, nY AS float64, nWidth AS float64, nHeight AS float64, nRadius, cColor )

   LOCAL cTag := s_RectTag( nX, nY, nWidth, nHeight, nRadius )

   cTag += s_Stroke( nSvg, cColor ) + ' fill="none"'
   FWrite( nSvg, cTag + e'/>\n' )

   RETURN

// A polygon through the points { { nX, nY }, ... } filled with cColor; nRadius rounds the corners
// svgFilledPolygon( nSvg, aPoints, cColor, [nRadius] ) --> NIL
PROCEDURE svgFilledPolygon( nSvg AS int, aPoints AS array, cColor AS string, nRadius )

   IF ValType( nRadius ) == 'N' .AND. nRadius > 0
      svgFilledPath( nSvg, s_RoundedPolygonPath( aPoints, nRadius ), cColor )
   ELSE
      FWrite( nSvg, '<polygon points="' + s_Points( aPoints ) + '" fill="' + cColor + e'"/>\n' )
   ENDIF

   RETURN

// The outline of a polygon through the points { { nX, nY }, ... }; nRadius rounds the corners, cColor overrides the pen color
// svgPolygon( nSvg, aPoints, [nRadius], [cColor] ) --> NIL
PROCEDURE svgPolygon( nSvg AS int, aPoints AS array, nRadius, cColor )

   LOCAL cTag

   IF ValType( nRadius ) == 'N' .AND. nRadius > 0
      svgPath( nSvg, s_RoundedPolygonPath( aPoints, nRadius ), cColor )
      RETURN
   ENDIF

   cTag := '<polygon points="' + s_Points( aPoints ) + '"'
   cTag += s_Stroke( nSvg, cColor ) + ' fill="none"'
   FWrite( nSvg, cTag + e'/>\n' )

   RETURN

// The outline of a path; cPath is SVG path data, cColor overrides the pen color
// svgPath( nSvg, cPath, [cColor] ) --> NIL
PROCEDURE svgPath( nSvg AS int, cPath AS string, cColor )

   LOCAL cTag := '<path d="' + cPath + '"'

   cTag += s_Stroke( nSvg, cColor ) + ' fill="none"'
   FWrite( nSvg, cTag + e'/>\n' )

   RETURN

// A shape of lines, arcs and curves filled with cColor; cPath is SVG path data as in svgPath()
// svgFilledPath( nSvg, cPath, cColor ) --> NIL
PROCEDURE svgFilledPath( nSvg AS int, cPath AS string, cColor AS string )

   FWrite( nSvg, '<path d="' + cPath + '" fill="' + cColor + e'"/>\n' )

   RETURN

// The outline of a circle drawn with the pen; cColor overrides its color
// svgCircle( nSvg, nCx, nCy, nR, [cColor] ) --> NIL
PROCEDURE svgCircle( nSvg AS int, nCx AS float64, nCy AS float64, nR AS float64, cColor )

   LOCAL cTag := '<circle cx="' + s_Number( nCx ) + '" cy="' + s_Number( nCy ) + '" r="' + s_Number( nR ) + '"'

   cTag += s_Stroke( nSvg, cColor ) + ' fill="none"'
   FWrite( nSvg, cTag + e'/>\n' )

   RETURN

// An ellipse with the radii nRx across and nRy down, filled with cColor
// svgFilledEllipse( nSvg, nCx, nCy, nRx, nRy, cColor ) --> NIL
PROCEDURE svgFilledEllipse( nSvg AS int, nCx AS float64, nCy AS float64, nRx AS float64, nRy AS float64, cColor AS string )

   FWrite( nSvg, s_EllipseTag( nCx, nCy, nRx, nRy ) + ' fill="' + cColor + e'"/>\n' )

   RETURN

// The outline of an ellipse with the radii nRx across and nRy down; cColor overrides the pen color
// svgEllipse( nSvg, nCx, nCy, nRx, nRy, [cColor] ) --> NIL
PROCEDURE svgEllipse( nSvg AS int, nCx AS float64, nCy AS float64, nRx AS float64, nRy AS float64, cColor )

   FWrite( nSvg, s_EllipseTag( nCx, nCy, nRx, nRy ) + s_Stroke( nSvg, cColor ) + e' fill="none"/>\n' )

   RETURN

// An arc of a circle; angles in degrees, 0 right, 90 down; cColor overrides the pen color
// svgArc( nSvg, nCx, nCy, nR, nStartAngle, nEndAngle, [cColor] ) --> NIL
PROCEDURE svgArc( nSvg AS int, nCx AS float64, nCy AS float64, nR AS float64, nStartAngle AS float64, nEndAngle AS float64, cColor )

   LOCAL cPath := 'M ' + s_ArcPoint( nCx, nCy, nR, nStartAngle )

   IF Abs( nEndAngle - nStartAngle ) >= 360
      // A full circle: SVG cannot draw it as one arc, so two halves
      cPath += s_ArcTo( nCx, nCy, nR, nStartAngle, nStartAngle + 180 )
      cPath += s_ArcTo( nCx, nCy, nR, nStartAngle + 180, nStartAngle + 360 )
   ELSE
      cPath += s_ArcTo( nCx, nCy, nR, nStartAngle, nEndAngle )
   ENDIF

   svgPath( nSvg, cPath, cColor )

   RETURN

// A slice of a circle from nStartAngle to nEndAngle filled with cColor; angles as in svgArc()
// svgFilledPie( nSvg, nCx, nCy, nR, nStartAngle, nEndAngle, cColor ) --> NIL
PROCEDURE svgFilledPie( nSvg AS int, nCx AS float64, nCy AS float64, nR AS float64, ;
                        nStartAngle AS float64, nEndAngle AS float64, cColor AS string )

   IF Abs( nEndAngle - nStartAngle ) >= 360
      svgFilledCircle( nSvg, nCx, nCy, nR, cColor )
      RETURN
   ENDIF

   svgFilledPath( nSvg, s_PiePath( nCx, nCy, nR, nStartAngle, nEndAngle ), cColor )

   RETURN

// The outline of a slice of a circle; angles as in svgArc(), cColor overrides the pen color
// svgPie( nSvg, nCx, nCy, nR, nStartAngle, nEndAngle, [cColor] ) --> NIL
PROCEDURE svgPie( nSvg AS int, nCx AS float64, nCy AS float64, nR AS float64, nStartAngle AS float64, nEndAngle AS float64, cColor )

   IF Abs( nEndAngle - nStartAngle ) >= 360
      svgCircle( nSvg, nCx, nCy, nR, cColor )
      RETURN
   ENDIF

   svgPath( nSvg, s_PiePath( nCx, nCy, nR, nStartAngle, nEndAngle ), cColor )

   RETURN

// A text at nX, nY; nAlign is SVG_ALIGN_LEFT (default), SVG_ALIGN_CENTER or SVG_ALIGN_RIGHT of svg.ch
// svgText( nSvg, nX, nY, cText, cFont, nSize, nFontWeight, cColor, [nAlign] ) --> NIL
PROCEDURE svgText( nSvg AS int, nX AS float64, nY AS float64, cText AS string, cFont AS string, ;
                    nSize AS float64, nFontWeight AS int, cColor AS string, nAlign )

   LOCAL cAnchor := 'start'
   LOCAL cTag

   IF nAlign == 1
      cAnchor := 'middle'
   ELSEIF nAlign == 2
      cAnchor := 'end'
   ENDIF

   cTag := '<text x="' + s_Number( nX ) + '" y="' + s_Number( nY ) + '"'
   cTag += ' font-family="' + s_XmlEscape( cFont ) + '" font-size="' + s_Number( nSize ) + '"'
   cTag += ' font-weight="' + LTrim( Str( nFontWeight ) ) + '" fill="' + cColor + '"'
   cTag += ' text-anchor="' + cAnchor + '"'
   FWrite( nSvg, cTag + '>' + s_XmlEscape( cText ) + e'</text>\n' )

   RETURN

/* ---------------------------------------------------------------------------------------------
   Functions of this file
   --------------------------------------------------------------------------------------------- */

// A number for an attribute, with three decimals
STATIC FUNCTION s_Number( nValue AS float64 ) AS string

   RETURN LTrim( Str( nValue, 32, 3 ) )

// The opening of an <ellipse> tag
STATIC FUNCTION s_EllipseTag( nCx AS float64, nCy AS float64, nRx AS float64, nRy AS float64 ) AS string

   LOCAL cTag := '<ellipse cx="' + s_Number( nCx ) + '" cy="' + s_Number( nCy ) + '"'

   RETURN cTag + ' rx="' + s_Number( nRx ) + '" ry="' + s_Number( nRy ) + '"'

// The point "x y" on a circle at nAngle degrees, 0 pointing right, 90 down
STATIC FUNCTION s_ArcPoint( nCx AS float64, nCy AS float64, nR AS float64, nAngle AS float64 ) AS string

   RETURN s_Number( nCx + nR * Cos( DToR( nAngle ) ) ) + ' ' + s_Number( nCy + nR * Sin( DToR( nAngle ) ) )

// The path command " A ..." of an arc from nStartAngle to nEndAngle, less than a full circle
STATIC FUNCTION s_ArcTo( nCx AS float64, nCy AS float64, nR AS float64, nStartAngle AS float64, nEndAngle AS float64 ) AS string

   LOCAL nSweep := nEndAngle - nStartAngle
   LOCAL cR := s_Number( nR )
   LOCAL cFlags := IIF( Abs( nSweep ) > 180, ' 1', ' 0' ) + IIF( nSweep > 0, ' 1 ', ' 0 ' )

   RETURN ' A ' + cR + ' ' + cR + ' 0' + cFlags + s_ArcPoint( nCx, nCy, nR, nEndAngle )

// The path of a polygon with each corner cut by an arc of nRadius, smaller on a short edge
STATIC FUNCTION s_RoundedPolygonPath( aPoints AS array, nRadius AS float64 ) AS string

   LOCAL nCount := Len( aPoints )
   LOCAL cPath := ''
   LOCAL nIndex
   LOCAL aPrev, aCorner, aNext
   LOCAL nUx, nUy, nVx, nVy, nLenU, nLenV
   LOCAL nCos, nT, nR

   FOR nIndex := 1 TO nCount
      aPrev := aPoints[ IIF( nIndex == 1, nCount, nIndex - 1 ) ]
      aCorner := aPoints[ nIndex ]
      aNext := aPoints[ IIF( nIndex == nCount, 1, nIndex + 1 ) ]

      // Unit vectors from the corner to both neighbours
      nUx := aPrev[ 1 ] - aCorner[ 1 ]
      nUy := aPrev[ 2 ] - aCorner[ 2 ]
      nVx := aNext[ 1 ] - aCorner[ 1 ]
      nVy := aNext[ 2 ] - aCorner[ 2 ]
      nLenU := Sqrt( nUx * nUx + nUy * nUy )
      nLenV := Sqrt( nVx * nVx + nVy * nVy )

      cPath += IIF( nIndex == 1, 'M ', ' L ' )

      // A repeated point stays sharp
      IF nLenU == 0 .OR. nLenV == 0
         cPath += s_Number( aCorner[ 1 ] ) + ' ' + s_Number( aCorner[ 2 ] )
         LOOP
      ENDIF

      nUx /= nLenU
      nUy /= nLenU
      nVx /= nLenV
      nVy /= nLenV
      nCos := nUx * nVx + nUy * nVy

      // A straight corner or a spike stays sharp
      IF Abs( nCos ) > 0.9999
         cPath += s_Number( aCorner[ 1 ] ) + ' ' + s_Number( aCorner[ 2 ] )
         LOOP
      ENDIF

      // Distance from the corner to where the arc touches an edge, at most half of the shorter edge
      nT := Min( nRadius * Sqrt( ( 1 + nCos ) / ( 1 - nCos ) ), Min( nLenU, nLenV ) / 2 )
      nR := nT * Sqrt( ( 1 - nCos ) / ( 1 + nCos ) )

      cPath += s_Number( aCorner[ 1 ] + nUx * nT ) + ' ' + s_Number( aCorner[ 2 ] + nUy * nT )
      cPath += ' A ' + s_Number( nR ) + ' ' + s_Number( nR ) + ' 0 0 '
      // Clockwise on the screen when the path turns right at this corner
      cPath += IIF( nUx * nVy - nUy * nVx < 0, '1', '0' ) + ' '
      cPath += s_Number( aCorner[ 1 ] + nVx * nT ) + ' ' + s_Number( aCorner[ 2 ] + nVy * nT )
   NEXT

   RETURN cPath + ' Z'

// The path of a slice of a circle: from the centre to the arc, along it and back
STATIC FUNCTION s_PiePath( nCx AS float64, nCy AS float64, nR AS float64, nStartAngle AS float64, nEndAngle AS float64 ) AS string

   LOCAL cPath := 'M ' + s_Number( nCx ) + ' ' + s_Number( nCy ) + ' L ' + s_ArcPoint( nCx, nCy, nR, nStartAngle )

   RETURN cPath + s_ArcTo( nCx, nCy, nR, nStartAngle, nEndAngle ) + ' Z'

// The stroke attributes of the pen of nSvg; a round pen also gives round line ends and corners
STATIC FUNCTION s_Stroke( nSvg AS int, cColor ) AS string

   LOCAL aPen := s_hPens[ nSvg ]
   LOCAL cStrokeColor := IIF( ValType( cColor ) == 'C', cColor, aPen[ 2 ] )
   LOCAL cAttr := ' stroke-width="' + s_Number( aPen[ 1 ] ) + '" stroke="' + cStrokeColor + '"'

   IF aPen[ 3 ]
      cAttr += ' stroke-linecap="round" stroke-linejoin="round"'
   ENDIF

   RETURN cAttr

// The opening of a <rect> tag, with rx and ry when nRadius is given and above zero
STATIC FUNCTION s_RectTag( nX AS float64, nY AS float64, nWidth AS float64, nHeight AS float64, nRadius ) AS string

   LOCAL cTag := '<rect x="' + s_Number( nX ) + '" y="' + s_Number( nY ) + '"'

   cTag += ' width="' + s_Number( nWidth ) + '" height="' + s_Number( nHeight ) + '"'

   IF ValType( nRadius ) == 'N' .AND. nRadius > 0
      cTag += ' rx="' + s_Number( nRadius ) + '" ry="' + s_Number( nRadius ) + '"'
   ENDIF

   RETURN cTag

// The points { { nX, nY }, ... } as "x,y x,y ..." for the points attribute
STATIC FUNCTION s_Points( aPoints AS array ) AS string

   LOCAL cPoints := ''
   LOCAL aPoint

   FOR EACH aPoint IN aPoints
      cPoints += IIF( Empty( cPoints ), '', ' ' ) + s_Number( aPoint[ 1 ] ) + ',' + s_Number( aPoint[ 2 ] )
   NEXT

   RETURN cPoints

// The text with the five characters XML escapes
STATIC FUNCTION s_XmlEscape( cText AS string ) AS string

   cText := StrTran( cText, '&', '&amp;' )
   cText := StrTran( cText, '<', '&lt;' )
   cText := StrTran( cText, '>', '&gt;' )
   cText := StrTran( cText, '"', '&quot;' )
   cText := StrTran( cText, "'", '&apos;' )

   RETURN cText
