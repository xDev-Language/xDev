/*
 * $DOC$
 * $NAME$
 * GT character test
 *
 * $ONELINER$
 *
 * $DESCRIPTION$
 *
 * $EXAMPLES$
 *
 * $OUTPUT$
 *
 * $IMAGE$
 *
 * $DOCEND$
 */

#include "inkey.ch"
#include "std.ch"
#include "gtinfo.ch"

#if OS_LINUX
   REQUEST GTXWC
#else
   REQUEST GTWVT
#endif

REQUEST ISO_8859_2
REQUEST CP437
REQUEST CP852
REQUEST CP1250
REQUEST MAZOVIA

PROCEDURE Main( cCodepage )

   LOCAL i, j, x
   LOCAL cText := "Zażółć gęślą jaźń"
   LOCAL cLegacy
   LOCAL cOldCodepage := GtInfo( GTI_CODEPAGE )
   LOCAL cColor, nCursor

   IF Empty( cCodepage )
      cCodepage := "CP437"
   ELSE
      cCodepage := Upper( cCodepage )
   ENDIF

   IF ! CdpExists( cCodepage ) .OR. GtInfo( GTI_CODEPAGE, cCodepage ) == NIL
      ? "Cannot select GT code page:", cCodepage
      ? "Use CP437, CP852, CP1250, ISO-8859-2, MAZOVIA or UTF-8."
      Inkey( 0, INKEY_KEYBOARD )
      ErrorLevel( 1 )
      RETURN
   ENDIF

   cColor := SetColor( "W/N" )
   nCursor := SetCursor( 0 )
   Scroll()

   DispOutAt( 0, 0, "GT code page: " + GtInfo( GTI_CODEPAGE ) )
   DispOutAt( 1, 0, "Console at startup: " + CdpTerm( .T. ) )
   DispOutAt( 2, 0, "Current console encoding: " + CdpTerm() )
   DispOutAt( 3, 0, "Byte value = column + row (hex)" )

   FOR j := 0 TO 15
      DispOutAt( 4, 6 + j * 3, NumToHex( j, 1 ) )
   NEXT
   FOR i := 0 TO 15
      SetPos( 5 + i, 0 )
      DispOut( "  " + NumToHex( i, 1 ) + " " )
      FOR j := 0 TO 15
         x := j * 16 + i
         DispOut( "  " + Chr( x ) )
      NEXT
   NEXT

   DispOutAt( 4, 55, "UTF-8 source:" )
   DispOutAt( 5, 55, cText )

   IF cCodepage == "CP852" .OR. cCodepage == "CP1250" .OR. ;
      cCodepage == "ISO-8859-2" .OR. cCodepage == "MAZOVIA"
      cLegacy := UTF8ToStr( cText, cCodepage )
      DispOutAt( 7, 55, "Encoded legacy bytes:" )
      DispOutAt( 8, 55, cLegacy )
      DispOutAt( 10, 55, "StrToUTF8():" )
      DispOutAt( 11, 55, StrToUTF8( cLegacy, cCodepage ) )
      DispOutAt( 13, 55, "Translate() to UTF-8:" )
      DispOutAt( 14, 55, Translate( cLegacy, cCodepage, "UTF-8" ) )
   ENDIF

   DispOutAt( 16, 55, "Single byte C4:" )
   DispOutAt( 17, 55, Chr( 196 ) )
   DispOutAt( 18, 55, "C4 85 (valid UTF-8):" )
   DispOutAt( 19, 55, Chr( 196 ) + Chr( 133 ) )

   DispBox( 22, 0, 24, 37, 1 )
   DispOutAt( 23, 2, "Single: " + cText )
   DispBox( 22, 40, 24, 77, 2 )
   DispOutAt( 23, 42, "Double: " + cText )
   DispOutAt( 21, 0, "Press any key to exit." )

   Inkey( 0, INKEY_KEYBOARD )

   SetCursor( nCursor )
   SetColor( cColor )
   GtInfo( GTI_CODEPAGE, cOldCodepage )

RETURN
