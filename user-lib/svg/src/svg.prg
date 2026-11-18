/*
 * Copyright (c) 2024-2026 xDev-Language
 *
 * Library: xDev SVG
 * File: svg.prg
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

/* ---------------------------------------------------------------------------------------------
   xDev SVG API
   --------------------------------------------------------------------------------------------- */

// Create an SVG file of nWidth x nHeight with the XML header; 0 when the file cannot be created
// svg_Init( cFilename, nWidth, nHeight ) --> nSvg
FUNCTION svg_Init( cFilename AS string, nWidth AS int, nHeight AS int ) AS int

   LOCAL nSvg    := FCreate( cFilename )
   LOCAL cWidth  := LTrim( Str( nWidth ) )
   LOCAL cHeight := LTrim( Str( nHeight ) )
   LOCAL cTag

   IF nSvg == -1
      RETURN 0
   ENDIF

   cTag := '<svg xmlns="http://www.w3.org/2000/svg" width="' + cWidth + '" height="' + cHeight + '"'
   cTag += ' viewBox="0 0 ' + cWidth + ' ' + cHeight + '"'

   FWrite( nSvg, e'<?xml version="1.0" encoding="UTF-8"?>\n' )
   FWrite( nSvg, '<!DOCTYPE svg PUBLIC "-//W3C//DTD SVG 1.1//EN" ' )
   FWrite( nSvg, e'"http://www.w3.org/Graphics/SVG/1.1/DTD/svg11.dtd">\n' )
   FWrite( nSvg, cTag + e'>\n' )

   RETURN nSvg

// End the drawing and close the file
// svg_Close( nSvg ) --> NIL
PROCEDURE svg_Close( nSvg AS int )

   FWrite( nSvg, e'</svg>\n' )
   FClose( nSvg )

   RETURN

// A comment in the file; "--" gets a space between, as XML does not allow it in a comment
// svg_Comment( nSvg, cComment ) --> NIL
PROCEDURE svg_Comment( nSvg AS int, cComment AS string )

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
// svg_FilledCircle( nSvg, nCx, nCy, nR, cColor ) --> NIL
PROCEDURE svg_FilledCircle( nSvg AS int, nCx AS float64, nCy AS float64, nR AS float64, cColor AS string )

   LOCAL cTag := '<circle cx="' + s_Number( nCx ) + '" cy="' + s_Number( nCy ) + '" r="' + s_Number( nR ) + '"'

   FWrite( nSvg, cTag + ' fill="' + cColor + e'"/>\n' )

   RETURN

// A line nStrokeWidth wide
// svg_Line( nSvg, nX1, nY1, nX2, nY2, nStrokeWidth, cColor ) --> NIL
PROCEDURE svg_Line( nSvg AS int, nX1 AS float64, nY1 AS float64, nX2 AS float64, nY2 AS float64, ;
                    nStrokeWidth AS float64, cColor AS string )

   LOCAL cTag := '<line x1="' + s_Number( nX1 ) + '" y1="' + s_Number( nY1 ) + '"'

   cTag += ' x2="' + s_Number( nX2 ) + '" y2="' + s_Number( nY2 ) + '"'
   cTag += ' stroke-width="' + s_Number( nStrokeWidth ) + '" stroke="' + cColor + '"'
   FWrite( nSvg, cTag + e'/>\n' )

   RETURN

// A rectangle filled with cColor
// svg_FilledRect( nSvg, nX, nY, nWidth, nHeight, cColor ) --> NIL
PROCEDURE svg_FilledRect( nSvg AS int, nX AS float64, nY AS float64, nWidth AS float64, ;
                          nHeight AS float64, cColor AS string )

   LOCAL cTag := '<rect x="' + s_Number( nX ) + '" y="' + s_Number( nY ) + '"'

   cTag += ' width="' + s_Number( nWidth ) + '" height="' + s_Number( nHeight ) + '"'
   FWrite( nSvg, cTag + ' fill="' + cColor + e'"/>\n' )

   RETURN

// A polygon through the points { { nX, nY }, ... } filled with cColor
// svg_FilledPolygon( nSvg, aPoints, cColor ) --> NIL
PROCEDURE svg_FilledPolygon( nSvg AS int, aPoints AS array, cColor AS string )

   LOCAL cPoints := ''
   LOCAL aPoint

   FOR EACH aPoint IN aPoints
      cPoints += IIF( Empty( cPoints ), '', ' ' ) + s_Number( aPoint[ 1 ] ) + ',' + s_Number( aPoint[ 2 ] )
   NEXT

   FWrite( nSvg, '<polygon points="' + cPoints + '" fill="' + cColor + e'"/>\n' )

   RETURN

// The outline of a circle, nStrokeWidth wide
// svg_Circle( nSvg, nCx, nCy, nR, nStrokeWidth, cColor ) --> NIL
PROCEDURE svg_Circle( nSvg AS int, nCx AS float64, nCy AS float64, nR AS float64, ;
                      nStrokeWidth AS float64, cColor AS string )

   LOCAL cTag := '<circle cx="' + s_Number( nCx ) + '" cy="' + s_Number( nCy ) + '" r="' + s_Number( nR ) + '"'

   cTag += ' stroke-width="' + s_Number( nStrokeWidth ) + '" stroke="' + cColor + '" fill="none"'
   FWrite( nSvg, cTag + e'/>\n' )

   RETURN

// A text at nX, nY; nAlign is SVG_ALIGN_LEFT (default), SVG_ALIGN_CENTER or SVG_ALIGN_RIGHT of svg.ch
// svg_Text( nSvg, nX, nY, cText, cFont, nSize, nFontWeight, cColor, [nAlign] ) --> NIL
PROCEDURE svg_Text( nSvg AS int, nX AS float64, nY AS float64, cText AS string, cFont AS string, ;
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

// The text with the five characters XML escapes
STATIC FUNCTION s_XmlEscape( cText AS string ) AS string

   cText := StrTran( cText, '&', '&amp;' )
   cText := StrTran( cText, '<', '&lt;' )
   cText := StrTran( cText, '>', '&gt;' )
   cText := StrTran( cText, '"', '&quot;' )
   cText := StrTran( cText, "'", '&apos;' )

   RETURN cText
