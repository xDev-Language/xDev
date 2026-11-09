/*
 * $DOC$
 * $NAME$
 * Sliding puzzle game
 *
 * $ONELINER$
 * Demonstrates an interactive sliding puzzle on a resizable terminal screen.
 *
 * $DESCRIPTION$
 * Move tiles into the empty position to solve the puzzle. The program reports elapsed time and move count when the game ends.
 *
 * $EXAMPLES$
 * examples/sliding-puzzle.prg
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

#define FRAME_SINGLE  "┌─┐│┘─└│"

PROCEDURE Main()

   LOCAL lContinue := .T.
   LOCAL oSlidingPuzzle
   LOCAL i

   Set( _SET_EVENTMASK, INKEY_ALLEVENTS )
   SetCursor( SC_NONE )

   SetMode( 25, 100 )

   // resizing the window adds rows and columns
   GtInfo( GTI_RESIZEFONT, .F. )
   GtInfo( GTI_WINTITLE, "xDev Sliding Puzzle Game" )

   oSlidingPuzzle := SlidingPuzzle():New()

   IF ! oSlidingPuzzle:WelcomeScreen()
      SetCursor( SC_NORMAL )
      RETURN
   ENDIF

   oSlidingPuzzle:nStartGame := MilliSeconds()
   oSlidingPuzzle:Frame()
   oSlidingPuzzle:RandomValue()

   DO WHILE lContinue

      oSlidingPuzzle:Refresh()

      SWITCH Inkey( 0.1 )
      CASE K_ESC
         lContinue := .F.
         EXIT

      CASE K_UP
         oSlidingPuzzle:Up()
         EXIT

      CASE K_DOWN
         oSlidingPuzzle:Down()
         EXIT

      CASE K_LEFT
         oSlidingPuzzle:Left()
         EXIT

      CASE K_RIGHT
         oSlidingPuzzle:Right()
         EXIT

      CASE K_LBUTTONDOWN
         oSlidingPuzzle:LButtonDown()
         EXIT

      CASE K_MOUSEMOVE
         oSlidingPuzzle:MouseMove()
         EXIT

      CASE K_RESIZE
         EXIT

      ENDSWITCH

      i := 1
      IF AScan( oSlidingPuzzle:aArray, {| x | x != i++ }, 1, 15 ) == 0
         lContinue := .F.
      ENDIF
   ENDDO

   oSlidingPuzzle:PrintResults()

   SetCursor( SC_NORMAL )

   RETURN

CREATE CLASS SlidingPuzzle

   DATA nStartGame
   DATA nNumberMoves INIT 0

   DATA nRow INIT 0
   DATA nCol INIT 0
   DATA nPos INIT 0

   DATA aArray INIT {}

   METHOD New()
   METHOD WelcomeScreen()
   METHOD Up()
   METHOD Down()
   METHOD Left()
   METHOD Right()
   METHOD LButtonDown()
   METHOD MouseMove()
   METHOD Calculate()
   METHOD Show()
   METHOD Refresh()
   METHOD Frame()
   METHOD RandomValue()
   METHOD ElapsedSeconds()
   METHOD PrintResults()

ENDCLASS

METHOD New() CLASS SlidingPuzzle

   RETURN Self

// .T. to play, .F. when Esc or closing the window ends the program
METHOD WelcomeScreen() CLASS SlidingPuzzle

   LOCAL nMaxRow := 0, nMaxCol := 0
   LOCAL cLine
   LOCAL nRow := 0

   LOCAL aLogo := { ;
      "         ____               _____ ___     ___                ____                  __   ", ;
      "   _  __/ __ \___ _   __   / ___// (_)___/ (_)___  ____ _   / __ \__  __________  / /__ ", ;
      "  | |/_/ / / / _ \ | / /   \__ \/ / / __  / / __ \/ __ `/  / /_/ / / / /_  /_  / / / _ \", ;
      " _>  </ /_/ /  __/ |/ /   ___/ / / / /_/ / / / / / /_/ /  / ____/ /_/ / / /_/ /_/ /  __/", ;
      "/_/|_/_____/\___/|___/   /____/_/_/\__,_/_/_/ /_/\__, /  /_/    \__,_/ /___/___/_/\___/ ", ;
      "                                                /____/                                  " }

   DO WHILE .T.

      IF nMaxRow != MaxRow() .OR. nMaxCol != MaxCol()

         nMaxRow := MaxRow()
         nMaxCol := MaxCol()
         nRow    := Max( 0, Int( ( nMaxRow - Len( aLogo ) ) / 2 ) - 5 )

         Scroll()
         DispBegin()
         FOR EACH cLine IN aLogo
            DispOutAt( nRow + cLine:enumIndex - 1, 0, PadC( cLine, nMaxCol ), NToColor( RandomInt( 0x1, 0xf ) ) )
         NEXT

         DispOutAt( nRow + 16, 0, UPadC( "Copyright 2017-2026 Rafał Jopek", nMaxCol ), NToColor( 0x8 ) )
         DispEnd()

      ENDIF

      DispOutAt( nRow +  8, 0, PadC( "Play games", nMaxCol ), NToColor( IIF( s_OnLabel( nRow + 8, "Play games", nMaxCol ), 0xa, 0x6 ) ) )
      DispOutAt( nRow + 18, 0, PadC( "xDev-Language", nMaxCol ), NToColor( IIF( s_OnLabel( nRow + 18, "xDev-Language", nMaxCol ), 0xa, 0x6 ) ) )

      SWITCH Inkey( 0 )

      CASE K_ENTER
         RETURN .T.

      CASE K_ESC
      CASE K_CLOSE
         RETURN .F.

      CASE K_LBUTTONDOWN

         IF s_OnLabel( nRow + 8, "Play games", nMaxCol )
            RETURN .T.
         ENDIF

         IF s_OnLabel( nRow + 18, "xDev-Language", nMaxCol )
            GtInfo( GTI_OPENURL, "https://github.com/xDev-Language" )   // --> .T., .F. if not opened
         ENDIF
         EXIT

      ENDSWITCH

   ENDDO

   RETURN .F.

METHOD Up() CLASS SlidingPuzzle

   ::Calculate()

   IF ::nRow < 4
      ::aArray[ ::nPos ] := ::aArray[ ::nPos + 4 ]
      ::aArray[ ::nPos + 4 ] := 0
      ::nPos += 4
      ++::nNumberMoves
   ENDIF

   RETURN Self

METHOD Down() CLASS SlidingPuzzle

   ::Calculate()

   IF ::nRow > 1
      ::aArray[ ::nPos ] := ::aArray[ ::nPos - 4 ]
      ::aArray[ ::nPos - 4 ] := 0
      ::nPos -= 4
      ++::nNumberMoves
   ENDIF

   RETURN Self

METHOD Left() CLASS SlidingPuzzle

   ::Calculate()

   IF ::nCol < 4
      ::aArray[ ::nPos ] := ::aArray[ ::nPos + 1 ]
      ::aArray[ ::nPos + 1 ] := 0
      ::nPos++
      ++::nNumberMoves
   ENDIF

   RETURN Self

METHOD Right() CLASS SlidingPuzzle

   ::Calculate()

   IF ::nCol > 1
      ::aArray[ ::nPos ] := ::aArray[ ::nPos - 1 ]
      ::aArray[ ::nPos - 1 ] := 0
      ::nPos--
      ++::nNumberMoves
   ENDIF

   RETURN Self

METHOD LButtonDown() CLASS SlidingPuzzle

   LOCAL nMRow := MRow()
   LOCAL nMCol := MCol()
   LOCAL nMPos

   IF nMRow >= 17 .OR. nMCol >= 29
      RETURN Self
   ENDIF

   IF ( nMRow % 4 ) == 0 .OR. ( nMCol % 7 ) == 0
      RETURN Self
   ENDIF

   ::Calculate()

   nMPos := ( Int( ( nMRow - 1 ) / 4 ) ) * 4 + Int( ( nMCol - 1 ) / 7 ) + 1

   DO CASE
   CASE nMPos == ::nPos + 4 ; ::Up()
   CASE nMPos == ::nPos - 4 ; ::Down()
   CASE nMPos == ::nPos + 1 ; ::Left()
   CASE nMPos == ::nPos - 1 ; ::Right()
   ENDCASE

   RETURN Self

METHOD MouseMove() CLASS SlidingPuzzle

   LOCAL nMRow := MRow()
   LOCAL nMCol := MCol()
   LOCAL nMPos

   IF nMRow >= 17 .OR. nMCol >= 29
      RETURN Self
   ENDIF

   IF ( nMRow % 4 ) == 0 .OR. ( nMCol % 7 ) == 0
      RETURN Self
   ENDIF

   ::Calculate()

   nMPos := ( Int( ( nMRow - 1 ) / 4 ) ) * 4 + Int( ( nMCol - 1 ) / 7 ) + 1

   DO CASE
   CASE nMPos == ::nPos + 4   // Up()
      DispBox( ::nRow * 4 + 1, ( ::nCol - 1 ) * 7 + 1, ::nRow  * 4 + 3, ( ::nCol - 1 ) * 7 + 6, FRAME_SINGLE, NToColor( 0xf0 ) )

   CASE nMPos == ::nPos - 4   // Down()
      DispBox( ( ::nRow  - 1 ) * 4 - 3, ( ::nCol - 1 ) * 7 + 1, ( ::nRow - 1 ) * 4 - 1, ( ::nCol - 1 ) * 7 + 6, FRAME_SINGLE, NToColor( 0xf0 ) )

   CASE nMPos == ::nPos + 1   // Left()
      DispBox( ::nRow * 4 - 3, ( ::nCol ) * 7 + 1, ::nRow  * 4 - 1, ( ::nCol ) * 7 + 6, FRAME_SINGLE, NToColor( 0xf0 ) )

   CASE nMPos == ::nPos - 1   // Right()
      DispBox( ::nRow * 4 - 3, ( ::nCol - 1 ) * 7 - 6, ::nRow  * 4 - 1, ( ::nCol - 1 ) * 7 - 1, FRAME_SINGLE, NToColor( 0xf0 ) )

   ENDCASE

   RETURN Self

METHOD Calculate() CLASS SlidingPuzzle

   LOCAL i

   FOR i := 1 TO 16
      IF ::aArray[ i ] == 0
         ::nPos := i
         ::nRow := 1 + Int( ( ::nPos - 1 ) / 4 )
         ::nCol := 1 + Int( ( ::nPos - 1 ) % 4 )
         EXIT
      ENDIF
   NEXT

   RETURN Self

METHOD Show() CLASS SlidingPuzzle

   LOCAL nRowPos := 1, nColPos := 3
   LOCAL tmp := 0
   LOCAL i

   FOR i := 1 TO 16

      ++tmp

      IF ::aArray[ i ] == IIF( i == 16, 0, i )
         DispBox( nRowPos, nColPos - 2, ( nRowPos - 1 ) + 3, ( nColPos - 3 ) + 6, FRAME_SINGLE + " ", NToColor( 0x20 ) )
         DispOutAt( nRowPos + 1, nColPos, LTrim( Str( ::aArray[ i ] ) ), NToColor( 0x20 ) )
      ELSE
         DispOutAt( nRowPos + 1, nColPos, LTrim( Str( ::aArray[ i ] ) ), NToColor( 0xf1 ) )
      ENDIF

      IF ::aArray[ i ] == 0
         DispBox( nRowPos, nColPos - 2, ( nRowPos - 1 ) + 3, ( nColPos - 3 ) + 6, , NToColor( 0x11 ) )
      ENDIF

      nColPos += 7

      IF tmp >= 4
         nRowPos += 4
         nColPos := 3
         tmp := 0
      ENDIF
   NEXT

   DispOutAt( 19, 5, "Game duration   : " + LTrim( Str( ::ElapsedSeconds() ) ) + " second(s)" )
   DispOutAt( 20, 5, "Number of moves : " + LTrim( Str( ::nNumberMoves ) ) )

   RETURN Self

METHOD Refresh() CLASS SlidingPuzzle

   DispBegin()

   Scroll()
   ::Frame()
   ::Show()
   ::MouseMove()

   DispEnd()

   RETURN Self

METHOD Frame() CLASS SlidingPuzzle

   LOCAL cLine

   FOR EACH cLine IN { ;
         "┌──────┬──────┬──────┬──────┐", ;
         "│      │      │      │      │", ;
         "│      │      │      │      │", ;
         "│      │      │      │      │", ;
         "├──────┼──────┼──────┼──────┤", ;
         "│      │      │      │      │", ;
         "│      │      │      │      │", ;
         "│      │      │      │      │", ;
         "├──────┼──────┼──────┼──────┤", ;
         "│      │      │      │      │", ;
         "│      │      │      │      │", ;
         "│      │      │      │      │", ;
         "├──────┼──────┼──────┼──────┤", ;
         "│      │      │      │      │", ;
         "│      │      │      │      │", ;
         "│      │      │      │      │", ;
         "└──────┴──────┴──────┴──────┘" }
      DispOutAt( cLine:enumIndex - 1, 0, cLine, NToColor( 0xf0 ) )
   NEXT

   RETURN Self

METHOD RandomValue() CLASS SlidingPuzzle

   LOCAL i
   LOCAL nRandomValue := RandomInt( 0, 15 )

   FOR i := 1 TO 16
      AAdd( ::aArray, nRandomValue-- )
      IF nRandomValue == -1
         ::nPos := i
      ENDIF
      nRandomValue := IIF( nRandomValue == -1, 15, nRandomValue )
   NEXT

   RETURN Self

// the whole seconds since the game started
METHOD ElapsedSeconds() CLASS SlidingPuzzle

   RETURN Int( ( MilliSeconds() - ::nStartGame ) / 1000 )

METHOD PrintResults() CLASS SlidingPuzzle

   Scroll()
   SetPos( 0, 0 )

   QOut( "Results:" )
   QOut( "" )
   QOut( "Game duration   -", LTrim( Str( ::ElapsedSeconds() ) ), "second(s)" )
   QOut( "" )
   QOut( "Number of moves -", LTrim( Str( ::nNumberMoves ) ) )

   Inkey( 0 )

   RETURN Self

// is the mouse on cText, which PadC() centers in nWidth columns of row nRow?
STATIC FUNCTION s_OnLabel( nRow, cText, nWidth )

   LOCAL nLeft := Int( ( nWidth - Len( cText ) ) / 2 )

   RETURN MRow() == nRow .AND. MCol() >= nLeft .AND. MCol() < nLeft + Len( cText )
