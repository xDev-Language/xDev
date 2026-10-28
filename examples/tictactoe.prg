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

STATIC aBoard   := { "", "", "", "", "", "", "", "", "" }
STATIC cPlayer  := "X"
STATIC cResult  := ""
STATIC lComputer := .F.

STATIC aWinLines := { { 1, 2, 3 }, { 4, 5, 6 }, { 7, 8, 9 }, ;
                      { 1, 4, 7 }, { 2, 5, 8 }, { 3, 6, 9 }, ;
                      { 1, 5, 9 }, { 3, 5, 7 } }

STATIC nTop     := 0
STATIC nLeft    := 0

PROCEDURE Main()

   LOCAL nMode

   SetCursor( SC_NONE )
   SetColorExt( COLOR_TEXT, COLOR_PAPER )

   GtInfo( GTI_MAXIMIZED, .T. )
   GtInfo( GTI_WINTITLE, "Tic Tac Toe Game" )

   Scroll()

   nMode := s_WelcomeScreen()

   IF nMode > 0
      lComputer := ( nMode == 1 )
      s_TicTacToe()
   ENDIF

   SetCursor( SC_NORMAL )

   RETURN

STATIC FUNCTION s_WelcomeScreen()

   LOCAL aLogo := { ;
      "    __  ____       __  _         __                __           ", ;
      "   / / / / /_     / /_(_)____   / /_ ___  _____   / /_ ___  ___ ", ;
      "  / /_/ / __ \   / __/ / ___/  / __/ __ `/ ___/  / __/ __ \/ _ \", ;
      " / __  / /_/ /  / /_/ / /__   / /_/ /_/ / /__   / /_/ /_/ /  __/", ;
      "/_/ /_/_.___/   \__/_/\___/   \__/\__,_/\___/   \__/\____/\___/ " }
   LOCAL aColor := { COLOR_BLUE, COLOR_WINE, COLOR_OLIVE, COLOR_INDIGO, COLOR_TEAL }
   LOCAL nMaxRow := 0, nMaxCol := 0
   LOCAL nRow := 0
   LOCAL nWidth := 0
   LOCAL i

   DO WHILE .T.

      IF nMaxRow != MaxRow() .OR. nMaxCol != MaxCol()

         nMaxRow := MaxRow()
         nMaxCol := MaxCol()
         nWidth  := nMaxCol + 1
         nRow    := Max( 0, Int( ( nMaxRow - Len( aLogo ) ) / 2 ) - 5 )

         DispBegin()
         Scroll()
         FOR i := 1 TO Len( aLogo )
            DispOutAtExt( nRow + i - 1, 0, PadC( aLogo[ i ], nWidth ), aColor[ i ], COLOR_PAPER )
         NEXT
         DispEnd()

      ENDIF

      DispOutAtExt( nRow +  8, 0, PadC( "Play with computer", nWidth ), IIF( s_IsOver( nRow +  8, nWidth, "Play with computer" ), COLOR_OLIVE, COLOR_BROWN ), COLOR_PAPER )
      DispOutAtExt( nRow + 10, 0, PadC( "Play with friend", nWidth ), IIF( s_IsOver( nRow + 10, nWidth, "Play with friend" ), COLOR_OLIVE, COLOR_BROWN ), COLOR_PAPER )

      SWITCH Inkey( 0, INKEY_ALLEVENTS )

      CASE K_ENTER
         RETURN 1

      CASE K_ESC
         RETURN 0

      CASE K_CLOSE
         RETURN 0

      CASE K_LBUTTONDOWN

         IF s_IsOver( nRow + 8, nWidth, "Play with computer" )
            RETURN 1
         ENDIF

         IF s_IsOver( nRow + 10, nWidth, "Play with friend" )
            RETURN 2
         ENDIF
         EXIT

      ENDSWITCH

   ENDDO

   RETURN 0

STATIC FUNCTION s_IsOver( nRow, nWidth, cText )

   LOCAL nCol := Int( ( nWidth - Len( cText ) ) / 2 )

   RETURN MRow() == nRow .AND. MCol() >= nCol .AND. MCol() < nCol + Len( cText )

STATIC PROCEDURE s_TicTacToe()

   LOCAL nMaxRow := 0, nMaxCol := 0
   LOCAL nCell

   s_NewGame()

   DO WHILE .T.

      IF nMaxRow != MaxRow() .OR. nMaxCol != MaxCol()

         nMaxRow := MaxRow()
         nMaxCol := MaxCol()

         nTop    := Max( 0, Int( ( nMaxRow - 31 ) / 2 ) )
         nLeft   := Max( 0, Int( ( nMaxCol - 52 ) / 2 ) )

         s_Refresh()

      ENDIF

      SWITCH Inkey( 0, INKEY_ALLEVENTS )

      CASE K_ESC
         RETURN

      CASE K_CLOSE
         RETURN

      CASE K_ENTER
         s_NewGame()
         s_Refresh()
         EXIT

      CASE K_LBUTTONDOWN

         nCell := s_MouseCell()

         IF nCell > 0 .AND. Empty( aBoard[ nCell ] ) .AND. Empty( cResult )
            s_Move( nCell )
            IF lComputer .AND. Empty( cResult )
               s_Move( s_ComputerMove() )
            ENDIF
            s_Refresh()
         ENDIF
         EXIT

      ENDSWITCH

   ENDDO

   RETURN

STATIC PROCEDURE s_NewGame()

   AFill( aBoard, "" )
   cPlayer := "X"
   cResult := ""

   IF lComputer
      cPlayer := "O"
      s_Move( s_ComputerMove() )
   ENDIF

   RETURN

STATIC PROCEDURE s_Move( nCell )

   aBoard[ nCell ] := cPlayer
   cResult := s_Winner()
   cPlayer := IIF( cPlayer == "X", "O", "X" )

   RETURN

STATIC FUNCTION s_ComputerMove()

   LOCAL nBest := 0
   LOCAL nBestScore := -100
   LOCAL nScore
   LOCAL i

   IF s_FreeCells() == 9
      RETURN 1
   ENDIF

   FOR i := 1 TO 9
      IF Empty( aBoard[ i ] )
         aBoard[ i ] := "O"
         nScore := s_Minimax( "X", 1 )
         aBoard[ i ] := ""
         IF nScore > nBestScore
            nBestScore := nScore
            nBest := i
         ENDIF
      ENDIF
   NEXT

   RETURN nBest

STATIC FUNCTION s_Minimax( cTurn, nDepth )

   LOCAL cWinner := s_Winner()
   LOCAL nBest
   LOCAL nScore
   LOCAL i

   DO CASE
   CASE cWinner == "O"
      RETURN 10 - nDepth
   CASE cWinner == "X"
      RETURN nDepth - 10
   CASE cWinner == "DRAW"
      RETURN 0
   ENDCASE

   nBest := IIF( cTurn == "O", -100, 100 )

   FOR i := 1 TO 9
      IF Empty( aBoard[ i ] )
         aBoard[ i ] := cTurn
         nScore := s_Minimax( IIF( cTurn == "O", "X", "O" ), nDepth + 1 )
         aBoard[ i ] := ""
         nBest := IIF( cTurn == "O", Max( nBest, nScore ), Min( nBest, nScore ) )
      ENDIF
   NEXT

   RETURN nBest

STATIC PROCEDURE s_Refresh()

   LOCAL cStatus
   LOCAL i

   DispBegin()
   Scroll()

   s_Frame()

   FOR i := 1 TO 9
      IF aBoard[ i ] == "X"
         s_DrawX( i )
      ELSEIF aBoard[ i ] == "O"
         s_DrawO( i )
      ENDIF
   NEXT

   DO CASE
   CASE cResult == "DRAW"
      cStatus := "Draw - Enter: new game, Esc: quit"
   CASE lComputer .AND. ! Empty( cResult )
      cStatus := IIF( cResult == "X", "You win", "Computer wins" ) + " - Enter: new game, Esc: quit"
   CASE ! Empty( cResult )
      cStatus := "Player " + cResult + " wins - Enter: new game, Esc: quit"
   OTHERWISE
      cStatus := "Player " + cPlayer
   ENDCASE

   IF nTop + 31 <= MaxRow()
      DispOutAtExt( nTop + 31, 0, PadC( cStatus, MaxCol() + 1 ), COLOR_TEXT, COLOR_PAPER )
   ENDIF

   DispEnd()

   RETURN

STATIC PROCEDURE s_Frame()

   LOCAL aLines := { ;
      "┌────────────────┬────────────────┬────────────────┐", ;
      "│                │                │                │", ;
      "│                │                │                │", ;
      "│                │                │                │", ;
      "│                │                │                │", ;
      "│                │                │                │", ;
      "│                │                │                │", ;
      "│                │                │                │", ;
      "│                │                │                │", ;
      "│                │                │                │", ;
      "├────────────────┼────────────────┼────────────────┤", ;
      "│                │                │                │", ;
      "│                │                │                │", ;
      "│                │                │                │", ;
      "│                │                │                │", ;
      "│                │                │                │", ;
      "│                │                │                │", ;
      "│                │                │                │", ;
      "│                │                │                │", ;
      "│                │                │                │", ;
      "├────────────────┼────────────────┼────────────────┤", ;
      "│                │                │                │", ;
      "│                │                │                │", ;
      "│                │                │                │", ;
      "│                │                │                │", ;
      "│                │                │                │", ;
      "│                │                │                │", ;
      "│                │                │                │", ;
      "│                │                │                │", ;
      "│                │                │                │", ;
      "└────────────────┴────────────────┴────────────────┘" }
   LOCAL i

   FOR i := 1 TO Len( aLines )
      DispOutAtExt( nTop + i - 1, nLeft, aLines[ i ], COLOR_TEAL, COLOR_PAPER )
   NEXT

   RETURN

STATIC FUNCTION s_MouseCell()

   LOCAL nRow := MRow() - nTop
   LOCAL nCol := MCol() - nLeft

   IF nRow < 0 .OR. nRow > 30 .OR. nCol < 0 .OR. nCol > 51 .OR. nRow % 10 == 0 .OR. nCol % 17 == 0
      RETURN 0
   ENDIF

   RETURN Int( nRow / 10 ) * 3 + Int( nCol / 17 ) + 1

STATIC FUNCTION s_Winner()

   LOCAL cFirst
   LOCAL i

   FOR i := 1 TO Len( aWinLines )
      cFirst := aBoard[ aWinLines[ i ][ 1 ] ]
      IF ! Empty( cFirst ) .AND. cFirst == aBoard[ aWinLines[ i ][ 2 ] ] .AND. cFirst == aBoard[ aWinLines[ i ][ 3 ] ]
         RETURN cFirst
      ENDIF
   NEXT

   IF s_FreeCells() == 0
      RETURN "DRAW"
   ENDIF

   RETURN ""

STATIC FUNCTION s_FreeCells()

   LOCAL nFree := 0
   LOCAL i

   FOR i := 1 TO 9
      IF Empty( aBoard[ i ] )
         nFree++
      ENDIF
   NEXT

   RETURN nFree

STATIC PROCEDURE s_DrawX( nCell )

   s_DrawFigure( nCell, { ;
      "****    ****", ;
      " ****  **** ", ;
      "  ********  ", ;
      "   ******   ", ;
      "  ********  ", ;
      " ****  **** ", ;
      "****    ****" }, COLOR_BLUE )

   RETURN

STATIC PROCEDURE s_DrawO( nCell )

   s_DrawFigure( nCell, { ;
      "  ********  ", ;
      "*****  *****", ;
      "***      ***", ;
      "***      ***", ;
      "***      ***", ;
      "*****  *****", ;
      "  ********  " }, COLOR_WINE )

   RETURN

STATIC PROCEDURE s_DrawFigure( nCell, aFigure, nColor )

   LOCAL nRow := nTop + 2 + Int( ( nCell - 1 ) / 3 ) * 10
   LOCAL nCol := nLeft + 3 + ( ( nCell - 1 ) % 3 ) * 17
   LOCAL i

   FOR i := 1 TO Len( aFigure )
      DispOutAtExt( nRow + i - 1, nCol, aFigure[ i ], nColor, COLOR_PAPER )
   NEXT

   RETURN
