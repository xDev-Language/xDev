/*
 *
 */

#include "inkey.ch"
#include "setcurs.ch"
#include "gtinfo.ch"
#include "tui.ch"

#if OS_LINUX
   REQUEST GTXWC
#else
   REQUEST GTWVT
#endif

#define CELL_RED       0xC0392B
#define CELL_GREEN     0x27AE60
#define CELL_BLUE      0x2E5FB8

#define SQUARE_HEIGHT  8
#define SQUARE_WIDTH   14
#define OPTION_STEP_X  20
#define OPTION_STEP_Y  10
#define OPTIONS_TOP    12
#define BOARD_HEIGHT   33
#define BOARD_WIDTH    54

STATIC aColors  := { CELL_RED, CELL_GREEN, CELL_BLUE }
STATIC aSplits  := { ;
   { 5, 7, 8, 9 }, ;
   { 1, 4, 7, 8 }, ;
   { 1, 2, 3, 5 }, ;
   { 1, 2, 4, 5 }, ;
   { 3, 6, 8, 9 }, ;
   { 1, 4, 5, 7 }, ;
   { 3, 5, 6, 9 } }

STATIC aSolution := {}
STATIC aPieceA   := {}
STATIC aOptions  := {}
STATIC nAnswer   := 0
STATIC nChosen   := 0
STATIC nScore    := 0
STATIC nRounds   := 0

STATIC nTop      := 0
STATIC nLeft     := 0

PROCEDURE Main()

   LOCAL nMaxRow := 0, nMaxCol := 0
   LOCAL nKey
   LOCAL nOption

   SetCursor( SC_NONE )
   SetColorExt( COLOR_TEXT, COLOR_PAPER )

   GtInfo( GTI_MAXIMIZED, .T. )
   GtInfo( GTI_WINTITLE, "Squares" )

   s_NewPuzzle()

   DO WHILE .T.

      IF nMaxRow != MaxRow() .OR. nMaxCol != MaxCol()
         nMaxRow := MaxRow()
         nMaxCol := MaxCol()
         nTop    := Max( 0, Int( ( nMaxRow - BOARD_HEIGHT ) / 2 ) )
         nLeft   := Max( 0, Int( ( nMaxCol - BOARD_WIDTH ) / 2 ) )
      ENDIF

      s_Refresh()

      nKey := Inkey( 0, INKEY_KEYBOARD + INKEY_LDOWN + INKEY_GTEVENT )

      SWITCH nKey

      CASE K_ESC
      CASE K_CLOSE
         SetCursor( SC_NORMAL )
         RETURN

      CASE K_ENTER
      CASE K_SPACE
         IF nChosen > 0
            s_NewPuzzle()
         ENDIF
         EXIT

      CASE K_LBUTTONDOWN
         IF nChosen > 0
            s_NewPuzzle()
         ELSEIF ( nOption := s_OptionAt( MRow(), MCol() ) ) > 0
            s_Choose( nOption )
         ENDIF
         EXIT

      OTHERWISE
         IF nKey >= 32 .AND. nKey <= 126 .AND. nChosen == 0
            nOption := At( Lower( Chr( nKey ) ), "abcdef" )
            IF nOption > 0
               s_Choose( nOption )
            ENDIF
         ENDIF

      ENDSWITCH

   ENDDO

   RETURN

STATIC PROCEDURE s_NewPuzzle()

   LOCAL aSplit := aSplits[ RandomInt( Len( aSplits ) ) ]
   LOCAL aGrid
   LOCAL i

   aSolution := Array( 9 )
   FOR i := 1 TO 9
      aSolution[ i ] := RandomInt( 3 )
   NEXT

   aPieceA := Array( 9 )
   AFill( aPieceA, .F. )
   FOR i := 1 TO Len( aSplit )
      aPieceA[ aSplit[ i ] ] := .T.
   NEXT

   aOptions := {}
   DO WHILE Len( aOptions ) < 5
      aGrid := s_Mutate( aSolution )
      IF ! s_SameSquare( aGrid, aSolution ) .AND. AScan( aOptions, {| a | s_SameSquare( a, aGrid ) } ) == 0
         AAdd( aOptions, s_Rotate( aGrid, RandomInt( 0, 3 ) ) )
      ENDIF
   ENDDO

   nAnswer := RandomInt( 6 )
   AIns( aOptions, nAnswer, s_Rotate( aSolution, RandomInt( 1, 3 ) ), .T. )
   nChosen := 0

   RETURN

STATIC FUNCTION s_Mutate( aGrid )

   LOCAL aCopy := AClone( aGrid )
   LOCAL i := RandomInt( 9 )
   LOCAL j := RandomInt( 9 )

   IF aGrid[ i ] != aGrid[ j ]
      aCopy[ i ] := aGrid[ j ]
      aCopy[ j ] := aGrid[ i ]
   ELSE
      aCopy[ i ] := aGrid[ i ] % 3 + 1
   ENDIF

   RETURN aCopy

STATIC FUNCTION s_Rotate( aGrid, nTimes )

   LOCAL aResult := AClone( aGrid )
   LOCAL aPrevious
   LOCAL nRow, nCol
   LOCAL i

   FOR i := 1 TO nTimes
      aPrevious := aResult
      aResult := Array( 9 )
      FOR nRow := 1 TO 3
         FOR nCol := 1 TO 3
            aResult[ ( nRow - 1 ) * 3 + nCol ] := aPrevious[ ( 3 - nCol ) * 3 + nRow ]
         NEXT
      NEXT
   NEXT

   RETURN aResult

STATIC FUNCTION s_SameSquare( aGrid1, aGrid2 )

   LOCAL aRotated
   LOCAL lSame
   LOCAL i, j

   FOR i := 0 TO 3
      aRotated := s_Rotate( aGrid1, i )
      lSame := .T.
      FOR j := 1 TO 9
         IF aRotated[ j ] != aGrid2[ j ]
            lSame := .F.
            EXIT
         ENDIF
      NEXT
      IF lSame
         RETURN .T.
      ENDIF
   NEXT

   RETURN .F.

STATIC PROCEDURE s_Choose( nOption )

   nChosen := nOption
   nRounds++
   IF nChosen == nAnswer
      nScore++
   ENDIF

   RETURN

STATIC FUNCTION s_OptionAt( nRow, nCol )

   LOCAL nOptionRow, nOptionCol
   LOCAL i

   FOR i := 1 TO 6
      nOptionRow := s_OptionRow( i )
      nOptionCol := s_OptionCol( i )
      IF nRow >= nOptionRow .AND. nRow <= nOptionRow + SQUARE_HEIGHT .AND. ;
         nCol >= nOptionCol .AND. nCol < nOptionCol + SQUARE_WIDTH
         RETURN i
      ENDIF
   NEXT

   RETURN 0

STATIC FUNCTION s_OptionRow( nOption )

   RETURN nTop + OPTIONS_TOP + Int( ( nOption - 1 ) / 3 ) * OPTION_STEP_Y

STATIC FUNCTION s_OptionCol( nOption )

   RETURN nLeft + ( ( nOption - 1 ) % 3 ) * OPTION_STEP_X

STATIC PROCEDURE s_Refresh()

   LOCAL cStatus
   LOCAL i

   DispBegin()
   Scroll()

   DispOutAtExt( nTop, 0, PadC( "Which square do the two pieces make?", MaxCol() + 1 ), COLOR_BLUE, COLOR_PAPER )

   s_DrawEquation()

   FOR i := 1 TO 6
      s_DrawCells( s_OptionRow( i ), s_OptionCol( i ), aOptions[ i ], NIL )
      s_DrawLabel( i )
   NEXT

   DO CASE
   CASE nChosen == 0
      cStatus := "Press a-f or click a square - Esc: quit"
   CASE nChosen == nAnswer
      cStatus := "Correct! - Enter: next puzzle"
   OTHERWISE
      cStatus := "Wrong, the answer is " + SubStr( "abcdef", nAnswer, 1 ) + " - Enter: next puzzle"
   ENDCASE

   cStatus += "   Score: " + LTrim( Str( nScore ) ) + "/" + LTrim( Str( nRounds ) )

   IF nTop + BOARD_HEIGHT - 1 <= MaxRow()
      DispOutAtExt( nTop + BOARD_HEIGHT - 1, 0, PadC( cStatus, MaxCol() + 1 ), COLOR_TEXT, COLOR_PAPER )
   ENDIF

   DispEnd()

   RETURN

STATIC PROCEDURE s_DrawEquation()

   LOCAL aPieceB := Array( 9 )
   LOCAL nWidthA, nWidthB
   LOCAL nRow := nTop + 2
   LOCAL nCol
   LOCAL i

   FOR i := 1 TO 9
      aPieceB[ i ] := ! aPieceA[ i ]
   NEXT

   nWidthA := s_PieceWidth( aPieceA )
   nWidthB := s_PieceWidth( aPieceB )
   nCol := nLeft + Int( ( BOARD_WIDTH - ( nWidthA + nWidthB + 10 ) ) / 2 )

   s_DrawPiece( nRow, nCol, aPieceA )
   nCol += nWidthA + 2
   DispOutAtExt( nRow + 3, nCol, "+", COLOR_TEXT, COLOR_PAPER )
   nCol += 3
   s_DrawPiece( nRow, nCol, aPieceB )
   nCol += nWidthB + 2
   DispOutAtExt( nRow + 3, nCol, "= ?", COLOR_TEXT, COLOR_PAPER )

   RETURN

STATIC FUNCTION s_PieceWidth( aMask )

   LOCAL nFirst := 3, nLast := 1
   LOCAL i

   FOR i := 1 TO 9
      IF aMask[ i ]
         nFirst := Min( nFirst, ( i - 1 ) % 3 + 1 )
         nLast  := Max( nLast, ( i - 1 ) % 3 + 1 )
      ENDIF
   NEXT

   RETURN ( nLast - nFirst + 1 ) * 5 - 1

STATIC PROCEDURE s_DrawPiece( nRow, nCol, aMask )

   LOCAL nFirstRow := 3, nFirstCol := 3
   LOCAL i

   FOR i := 1 TO 9
      IF aMask[ i ]
         nFirstRow := Min( nFirstRow, Int( ( i - 1 ) / 3 ) )
         nFirstCol := Min( nFirstCol, ( i - 1 ) % 3 )
      ENDIF
   NEXT

   s_DrawCells( nRow - nFirstRow * 3, nCol - nFirstCol * 5, aSolution, aMask )

   RETURN

STATIC PROCEDURE s_DrawCells( nRow, nCol, aGrid, aMask )

   LOCAL nCellRow, nCellCol
   LOCAL i

   FOR i := 1 TO 9
      IF aMask == NIL .OR. aMask[ i ]
         nCellRow := nRow + Int( ( i - 1 ) / 3 ) * 3
         nCellCol := nCol + ( ( i - 1 ) % 3 ) * 5
         DispOutAtExt( nCellRow,     nCellCol, Space( 4 ), COLOR_TEXT, aColors[ aGrid[ i ] ] )
         DispOutAtExt( nCellRow + 1, nCellCol, Space( 4 ), COLOR_TEXT, aColors[ aGrid[ i ] ] )
      ENDIF
   NEXT

   RETURN

STATIC PROCEDURE s_DrawLabel( nOption )

   LOCAL cLabel := " " + SubStr( "abcdef", nOption, 1 ) + " "
   LOCAL nForeground := COLOR_TEXT
   LOCAL nBackground := COLOR_PAPER

   IF nChosen > 0 .AND. nOption == nAnswer
      nForeground := COLOR_PAPER
      nBackground := COLOR_OLIVE
   ELSEIF nChosen > 0 .AND. nOption == nChosen
      nForeground := COLOR_PAPER
      nBackground := COLOR_WINE
   ENDIF

   DispOutAtExt( s_OptionRow( nOption ) + SQUARE_HEIGHT, s_OptionCol( nOption ) + 5, cLabel, nForeground, nBackground )

   RETURN
