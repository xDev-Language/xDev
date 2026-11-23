/*
 * Copyright (c) 2024-2026 xDev-Language
 *
 * Library...: xDev Tests
 * File......: gtchr.prg
 *
 */

/*
 * $DOC$
 * $NAME$
 * GT character test
 *
 * $ONELINER$
 * Shows that the GT displays full UTF-8: Polish letters, other alphabets, box drawing and symbols on one screen.
 *
 * $DESCRIPTION$
 * xDev keeps all text in UTF-8, so the GT has no code page to select. One screen shows Polish text, Latin-1, Latin Extended-A, Greek, Cyrillic, the box drawing characters, block elements and shapes, and common symbols; the character ranges are built with UTF8Chr(). It also shows Len() and ULen(), ULeft(), UTF8Asc(), a lone byte that is not valid UTF-8, a frame drawn with box characters from the source text, and single and double boxes drawn by DispBox().
 *
 * $EXAMPLES$
 * gtchr
 *
 * $OUTPUT$
 *
 * $IMAGE$
 *
 * $DOCEND$
 */

#include "inkey.ch"
#include "gtinfo.ch"

#if OS_LINUX
   REQUEST GTXWC
#else
   REQUEST GTWVT
#endif

PROCEDURE Main()

   LOCAL cColor := SetColor( "W/N" )
   LOCAL nCursor := SetCursor( 0 )

   GtInfo( GTI_WINTITLE, "GT character test" )
   Scroll()

   DispOutAt( 0, 0, "GT character test: full UTF-8, no code pages" )
   s_Alphabets()
   s_Functions()
   s_Frames()
   DispOutAt( 24, 0, "Press any key to exit." )

   Inkey( 0, INKEY_KEYBOARD )

   SetCursor( nCursor )
   SetColor( cColor )

RETURN

// One line: the name of a range and its characters
STATIC PROCEDURE s_Line( nRow, cName, cCharacters )

   DispOutAt( nRow, 0, cName )
   DispOutAt( nRow, 14, cCharacters )

RETURN

// The characters from code point nFrom to nTo
STATIC FUNCTION s_Range( nFrom, nTo )

   LOCAL cText := ""
   LOCAL n

   FOR n := nFrom TO nTo
      cText += UTF8Chr( n )
   NEXT

RETURN cText

STATIC PROCEDURE s_Alphabets()

   s_Line( 2, "Polish", "Zażółć gęślą jaźń  ZAŻÓŁĆ GĘŚLĄ JAŹŃ" )
   s_Line( 3, "Latin-1", s_Range( 0x00C0, 0x00FF ) )
   s_Line( 4, "Latin Ext-A", s_Range( 0x0100, 0x013F ) )
   s_Line( 5, "", s_Range( 0x0140, 0x017F ) )
   s_Line( 6, "Greek", s_Range( 0x0391, 0x03A1 ) + s_Range( 0x03A3, 0x03A9 ) + " " + s_Range( 0x03B1, 0x03C9 ) )
   s_Line( 7, "Cyrillic", s_Range( 0x0410, 0x044F ) )
   s_Line( 8, "Box drawing", s_Range( 0x2500, 0x253F ) )
   s_Line( 9, "", s_Range( 0x2540, 0x257F ) )
   s_Line( 10, "Blocks", s_Range( 0x2580, 0x259F ) + " " + s_Range( 0x25A0, 0x25BF ) )
   s_Line( 11, "Symbols", "€ £ ¥ © ® ° ± × ÷ ≤ ≥ ≠ ∞ √ ← ↑ → ↓ ↔ ♠ ♣ ♥ ♦ ☺" )

RETURN

// Bytes and characters of a UTF-8 text; a lone byte C4 is not valid UTF-8
STATIC PROCEDURE s_Functions()

   LOCAL cText := "Zażółć gęślą jaźń"
   LOCAL cBytes := LTrim( Str( Len( cText ) ) )
   LOCAL cCharacters := LTrim( Str( ULen( cText ) ) )
   LOCAL cCode := NumToHex( UTF8Asc( "ż" ), 4 )

   DispOutAt( 13, 0, cText + ": Len() " + cBytes + " bytes, ULen() " + cCharacters + " characters" )
   DispOutAt( 14, 0, "ULeft( 6 ): " + ULeft( cText, 6 ) + '   UTF8Asc( "ż" ): U+' + cCode )
   DispOutAt( 15, 0, "UTF8Chr( 0x0105 ): " + UTF8Chr( 0x0105 ) )
   DispOutAt( 16, 0, "Byte C4 alone: " + Chr( 196 ) + "   bytes C4 85: " + Chr( 196 ) + Chr( 133 ) )

RETURN

// A frame written with box characters in the source, and the boxes of DispBox()
STATIC PROCEDURE s_Frames()

   DispOutAt( 17, 0, "Box characters in the text:" )
   DispOutAt( 18, 0, "┌────────┬────────┐  ╔════════╦════════╗" )
   DispOutAt( 19, 0, "│ Zażółć │ gęślą  │  ║ Zażółć ║ gęślą  ║" )
   DispOutAt( 20, 0, "├────────┼────────┤  ╠════════╬════════╣" )
   DispOutAt( 21, 0, "│ jaźń   │ ĄĘĆŃŚ  │  ║ jaźń   ║ ĄĘĆŃŚ  ║" )
   DispOutAt( 22, 0, "└────────┴────────┘  ╚════════╩════════╝" )

   DispOutAt( 17, 44, "DispBox():" )
   DispBox( 18, 44, 20, 79, 1 )
   DispOutAt( 19, 46, "Single: Zażółć gęślą jaźń" )
   DispBox( 21, 44, 23, 79, 2 )
   DispOutAt( 22, 46, "Double: Zażółć gęślą jaźń" )

RETURN


