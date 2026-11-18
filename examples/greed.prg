/*
 * $DOC$
 * $NAME$
 * Greed game
 *
 * $ONELINER$
 * Demonstrates an interactive game on a resizable terminal screen.
 *
 * $DESCRIPTION$
 * Move across the game board and collect numbered fields. The game ends when the player crosses the board.
 *
 * $EXAMPLES$
 * examples/greed.prg
 *
 * $OUTPUT$
 *
 * $DOCEND$
 */

/*
 * Copyright 2017-2026 Rafał Jopek
 */

#include "gtinfo.ch"
#include "inkey.ch"
#include "set.ch"
#include "setcurs.ch"

#if OS_LINUX
   REQUEST GTXWC
#else
   REQUEST GTWVT
#endif

PROCEDURE Main()

   LOCAL nOldCursor := SetCursor( SC_NONE )
   LOCAL lContinue := .T.
   LOCAL nMaxRow := 0, nMaxCol := 0
   LOCAL nRow, nCol, row, col
   LOCAL aArray
   LOCAL i, tmp, nRowOld, nColOld

   Set( _SET_EVENTMASK, INKEY_ALLEVENTS )

   // resizing the window changes the font, the screen stays 25 x 80
   GtInfo( GTI_RESIZEFONT, .T. )
   GtInfo( GTI_MAXIMIZED, .T. )
   GtInfo( GTI_WINTITLE, "Greed" )

   IF ! WelcomeScreen()
      SetCursor( nOldCursor )
      RETURN
   ENDIF

   aArray := Array( 25, 80 )

   FOR EACH row IN aArray
      FOR EACH col IN row
         col := { RandomInt( 1, 9 ), RandomInt( 0x1, 0xf ) }
      NEXT
   NEXT

   nRow := Int( MaxRow() / 2 ) + 1
   nCol := Int( MaxCol() / 2 ) + 1

   aArray[ nRow ][ nCol ] := { 0, 0x0 }

   DO WHILE lContinue

      IF nMaxRow != MaxRow() .OR. nMaxCol != MaxCol()
         Scroll()
         nMaxRow := MaxRow()
         nMaxCol := MaxCol()
      ENDIF

      Display( nRow, nCol, aArray )

      SWITCH Inkey( 0 )
      CASE K_ESC
         lContinue := .F.
         EXIT

      CASE K_UP

         IF nRow == 1
            s_TheEnd( aArray )
            lContinue := .F.
         ELSE
            tmp     := aArray[ nRow - 1 ][ nCol ][ 1 ]
            nRowOld := nRow
            nRow    -= aArray[ nRow - 1 ][ nCol ][ 1 ]
            IF nRow > 0
               FOR i := 1 TO tmp
                  aArray[ nRow + i ][ nCol ] := { 0, 0x0 }
               NEXT
            ELSE
               // the way from the old field to the edge of the board
               nRow := 1
               FOR i := 1 TO nRowOld - 1
                  aArray[ nRow + i ][ nCol ] := { 0, 0x0 }
               NEXT
               Display( nRow, nCol, aArray )
               s_TheEnd( aArray )
               Inkey( 0 )
               lContinue := .F.
            ENDIF
         ENDIF
         EXIT

      CASE K_DOWN

         IF nRow == MaxRow() + 1
            s_TheEnd( aArray )
            lContinue := .F.
         ELSE

            tmp     := aArray[ nRow + 1 ][ nCol ][ 1 ]
            nRowOld := nRow
            nRow    += aArray[ nRow + 1 ][ nCol ][ 1 ]

            IF nRow < MaxRow() + 2
               FOR i := 1 TO tmp
                  aArray[ nRow - i ][ nCol ] := { 0, 0x0 }
               NEXT
            ELSE
               // the way from the old field to the edge of the board
               nRow := MaxRow() + 1
               FOR i := 1 TO nRow - nRowOld
                  aArray[ nRow - i ][ nCol ] := { 0, 0x0 }
               NEXT
               Display( nRow, nCol, aArray )
               s_TheEnd( aArray )
               Inkey( 0 )
               lContinue := .F.
            ENDIF
         ENDIF
         EXIT

      CASE K_LEFT
         IF nCol == 1
            s_TheEnd( aArray )
            lContinue := .F.
         ELSE
            tmp     := aArray[ nRow ][ nCol - 1 ][ 1 ]
            nColOld := nCol
            nCol    -= aArray[ nRow ][ nCol - 1 ][ 1 ]
            IF nCol > 0
               FOR i := 1 TO tmp
                  aArray[ nRow ][ nCol + i ] := { 0, 0x0 }
               NEXT
            ELSE
               // the way from the old field to the edge of the board
               nCol := 1
               FOR i := 1 TO nColOld - 1
                  aArray[ nRow ][ nCol + i ] := { 0, 0x0 }
               NEXT
               Display( nRow, nCol, aArray )
               s_TheEnd( aArray )
               Inkey( 0 )
               lContinue := .F.
            ENDIF
         ENDIF
         EXIT

      CASE K_RIGHT

         IF nCol == MaxCol() + 1
            s_TheEnd( aArray )
            lContinue := .F.
         ELSE
            tmp     := aArray[ nRow ][ nCol + 1 ][ 1 ]
            nColOld := nCol
            nCol    += aArray[ nRow ][ nCol + 1 ][ 1 ]
            IF nCol < MaxCol() + 2
               FOR i := 1 TO tmp
                  aArray[ nRow ][ nCol - i ] := { 0, 0x0 }
               NEXT
            ELSE
               // the way from the old field to the edge of the board
               nCol := MaxCol() + 1
               FOR i := 1 TO nCol - nColOld
                  aArray[ nRow ][ nCol - i ] := { 0, 0x0 }
               NEXT
               Display( nRow, nCol, aArray )
               s_TheEnd( aArray )
               Inkey( 0 )
               lContinue := .F.
            ENDIF
         ENDIF
         EXIT

      CASE K_TAB
         Score( aArray )
         EXIT

      ENDSWITCH

   ENDDO

   SetCursor( nOldCursor )

   RETURN

FUNCTION WelcomeScreen()

   LOCAL lContinue := .T.
   LOCAL lPlay := .T.
   LOCAL nMaxRow := 0, nMaxCol := 0
   LOCAL cLine
   LOCAL nRow

   LOCAL aLogo := { ;
      "         ____               ______                   __", ;
      "   _  __/ __ \___ _   __   / ____/_______  ___  ____/ /", ;
      "  | |/_/ / / / _ \ | / /  / / __/ ___/ _ \/ _ \/ __  / ", ;
      " _>  </ /_/ /  __/ |/ /  / /_/ / /  /  __/  __/ /_/ /  ", ;
      "/_/|_/_____/\___/|___/   \____/_/   \___/\___/\__,_/   ", ;
      "                                                       " }

   DO WHILE lContinue

      IF nMaxRow != MaxRow() .OR. nMaxCol != MaxCol()

         nMaxRow := MaxRow()
         nMaxCol := MaxCol()
         nRow    := Int( ( nMaxRow - Len( aLogo ) ) / 2 ) - 5

         Scroll()
         DispBegin()
         FOR EACH cLine IN aLogo
            DispOutAt( nRow + cLine:enumIndex - 1, 0, PadC( cLine, nMaxCol ), NToColor( RandomInt( 0x1, 0xf ) ) )
         NEXT

         DispOutAt( nRow + 16, 0, UPadC( "Copyright 2017-2026 Rafał Jopek", nMaxCol ), NToColor( 0x8 ) )
         DispEnd()

      ENDIF

      DispOutAt( nRow +  8, 0, PadC( "Play games", nMaxCol ), NToColor( 0x6 ) )
      DispOutAt( nRow + 18, 0, PadC( "xDev-Language", nMaxCol ), NToColor( 0x6 ) )

      IF s_OnLabel( nRow + 8, "Play games", nMaxCol )
         DispOutAt( nRow + 8, 0, PadC( "Play games", nMaxCol ), NToColor( 0xa ) )
      ENDIF

      IF s_OnLabel( nRow + 18, "xDev-Language", nMaxCol )
         DispOutAt( nRow + 18, 0, PadC( "xDev-Language", nMaxCol ), NToColor( 0xa ) )
      ENDIF

      SWITCH Inkey( 0 )

      CASE K_ENTER
         lContinue := .F.
         EXIT

      CASE K_ESC
         lPlay     := .F.
         lContinue := .F.
         EXIT

      CASE K_LBUTTONDOWN

         IF s_OnLabel( nRow + 8, "Play games", nMaxCol )
            lContinue := .F.
         ENDIF

         IF s_OnLabel( nRow + 18, "xDev-Language", nMaxCol )
            GtInfo( GTI_OPENURL, "https://github.com/xDev-Language" )   // --> .T., .F. if not opened
         ENDIF
         EXIT

      // the labels are drawn again at the top of the loop
      CASE K_MOUSEMOVE
         EXIT

      CASE K_RESIZE
         EXIT

      ENDSWITCH

   ENDDO

   RETURN lPlay

PROCEDURE Display( nRow, nCol, aArray )

   LOCAL row, col

   DispBegin()

   FOR EACH row IN aArray
      FOR EACH col IN row
         DispOutAt( row:enumIndex - 1, col:enumIndex - 1, LTrim( Str( col[ 1 ] ) ), NToColor( col[ 2 ] ) )
      NEXT
   NEXT
   DispOutAt( nRow - 1, nCol - 1, "☺", NToColor( aArray[ nRow ][ nCol ][ 2 ] ) )

   DispEnd()

   RETURN

PROCEDURE Score( aArray )

   LOCAL nRow := Int( ( MaxRow() - 2 ) / 2 )

   Scroll()

   DispOutAt( nRow, 0, UPadC( "Experience Value 2 ", MaxCol() + 1 ), NToColor( 0xf ) )
   DispOutAt( ++nRow + 1, 0, UPadC( "  Score : " + " [ " + LTrim( Str( s_Eaten( aArray ) ) ) + " / " + "2000 ]", MaxCol() + 1 ), NToColor( 0xf ) )

   Inkey( 3 )

   RETURN

// is the mouse on cText, which PadC() centers in nWidth columns of row nRow?
STATIC FUNCTION s_OnLabel( nRow, cText, nWidth )

   LOCAL nLeft := Int( ( nWidth - Len( cText ) ) / 2 )

   RETURN MRow() == nRow .AND. MCol() >= nLeft .AND. MCol() < nLeft + Len( cText )

// the number of eaten fields
STATIC FUNCTION s_Eaten( aArray )

   LOCAL row, col
   LOCAL nEaten := 0

   FOR EACH row IN aArray
      FOR EACH col IN row
         IF col[ 1 ] == 0
            nEaten++
         ENDIF
      NEXT
   NEXT

   RETURN nEaten

STATIC PROCEDURE s_TheEnd( aArray )

   Alert( "You crossed the game board.; The End Experience Value: " + LTrim( Str( s_Eaten( aArray ) ) ) + " / 2000", , "N/W+", 3 )

   RETURN
