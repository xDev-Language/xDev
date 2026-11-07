/*
 * $DOC$
 * $NAME$
 * Snake game
 *
 * $ONELINER$
 * Demonstrates an interactive Snake game in the terminal.
 *
 * $DESCRIPTION$
 * Control the snake, collect food, and avoid collisions. The program reports duration and final snake length when the game ends.
 *
 * $EXAMPLES$
 * examples/snake.prg
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

ENUM Direction
   DIRECTION_UP := 1,
   DIRECTION_DOWN,
   DIRECTION_RIGHT,
   DIRECTION_LEFT
ENDENUM

PROCEDURE Main()

   LOCAL lContinue := .T.
   LOCAL oSnake

   Set( _SET_EVENTMASK, INKEY_ALLEVENTS )

   SetCursor( SC_NONE )

   GtInfo( GTI_RESIZEFONT, .F. )
   GtInfo( GTI_WINTITLE, "Snake" )

   oSnake := Snake():New()

   IF ! oSnake:WelcomeScreen()
      SetCursor( SC_NORMAL )
      RETURN
   ENDIF

   oSnake:InitFood()

   Set( _SET_EVENTMASK, INKEY_KEYBOARD + INKEY_GTEVENT )

   DO WHILE lContinue

      oSnake:Refresh()

      SWITCH Inkey( 0.1 )
      CASE K_ESC
         lContinue := .F.
         EXIT

      CASE K_UP
         oSnake:Up()
         EXIT

      CASE K_DOWN
         oSnake:Down()
         EXIT

      CASE K_LEFT
         oSnake:Left()
         EXIT

      CASE K_RIGHT
         oSnake:Right()
         EXIT

      OTHERWISE
         oSnake:Move()
      ENDSWITCH

      IF oSnake:nRow == oSnake:nRowFood .AND. oSnake:nCol == oSnake:nColFood
         AAdd( oSnake:aTail, { 0, 0, oSnake:nFoodColor } )
         oSnake:InitFood()
      ENDIF

      IF AScan( oSnake:aTail, {| x | x[ 1 ] == oSnake:nRow .AND. x[ 2 ] == oSnake:nCol } ) > 0
         lContinue := .F.
      ENDIF

   ENDDO

   oSnake:PrintResults()

   SetCursor( SC_NORMAL )

   RETURN

CREATE CLASS Snake

   DATA nStartGame INIT MilliSeconds()

   DATA nRow       INIT Int( MaxRow() / 2 )
   DATA nCol       INIT Int( MaxCol() / 2 )

   DATA nRowFood
   DATA nColFood
   DATA nFoodColor INIT 0

   DATA nDirection INIT DIRECTION_RIGHT

   DATA aTail      INIT { { Int( MaxRow() / 2 ), Int( MaxCol() / 2 ) - 1, 0x0 } }

   METHOD New()
   METHOD WelcomeScreen()
   METHOD Up()
   METHOD Down()
   METHOD Left()
   METHOD Right()
   METHOD Refresh()
   METHOD Snake()
   METHOD Move()
   METHOD Food()
   METHOD InitFood()
   METHOD PrintResults()

ENDCLASS

METHOD New() CLASS Snake
   RETURN Self

METHOD WelcomeScreen() CLASS Snake

   LOCAL nMaxRow := 0, nMaxCol := 0
   LOCAL cLine
   LOCAL nRow := 0

   LOCAL aLogo := { ;
      "         ____               _____             __      ", ;
      "   _  __/ __ \___ _   __   / ___/____  ____ _/ /_____ ", ;
      "  | |/_/ / / / _ \ | / /   \__ \/ __ \/ __ `/ //_/ _ \", ;
      " _>  </ /_/ /  __/ |/ /   ___/ / / / / /_/ / ,< /  __/", ;
      "/_/|_/_____/\___/|___/   /____/_/ /_/\__,_/_/|_|\___/ ", ;
      "                                                      " }

   DO WHILE .T.

      IF nMaxRow != MaxRow() .OR. nMaxCol != MaxCol()

         nMaxRow := MaxRow()
         nMaxCol := MaxCol()
         nRow    := Max( 0, Int( ( nMaxRow - Len( aLogo ) ) / 2 ) - 5 )

         Scroll()
         DispBegin()
         FOR EACH cLine IN aLogo
            DispOutAt( nRow + cLine:__enumIndex - 1, 0, PadC( cLine, nMaxCol ), NToColor( RandomInt( 0x1, 0xf ) ) )
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
            GtInfo( GTI_OPENURL, "https://github.com/xDev-Language" )
         ENDIF
         EXIT

      ENDSWITCH

   ENDDO

   RETURN .F.

METHOD Up() CLASS Snake

   IF ::nRow < 0
      ::nRow := MaxRow() + 1
   ENDIF

   IF ::nDirection == DIRECTION_DOWN
      ::nRow++
   ELSE
      ::nRow--
      ::nDirection := DIRECTION_UP
   ENDIF

   RETURN Self

METHOD Down() CLASS Snake

   IF ::nRow > MaxRow()
      ::nRow := -1
   ENDIF

   IF ::nDirection == DIRECTION_UP
      ::nRow--
   ELSE
      ::nRow++
      ::nDirection := DIRECTION_DOWN
   ENDIF

   RETURN Self

METHOD Left() CLASS Snake

   IF ::nCol < 0
      ::nCol := MaxCol() + 1
   ENDIF

   IF ::nDirection == DIRECTION_RIGHT
      ::nCol++
   ELSE
      ::nCol--
      ::nDirection := DIRECTION_LEFT
   ENDIF

   RETURN Self

METHOD Right() CLASS Snake

   IF ::nCol > MaxCol()
      ::nCol := -1
   ENDIF

   IF ::nDirection == DIRECTION_LEFT
      ::nCol--
   ELSE
      ::nCol++
      ::nDirection := DIRECTION_RIGHT
   ENDIF

   RETURN Self

METHOD Refresh() CLASS Snake

   Scroll()
   DispBegin()

   ::Snake()
   ::Food()

   DispEnd()

   RETURN Self

METHOD Snake() CLASS Snake

   LOCAL i

   FOR i := Len( ::aTail ) - 1 TO 1 STEP - 1

      ::aTail[ i + 1 ][ 1 ] := ::aTail[ i ][ 1 ]   // row
      ::aTail[ i + 1 ][ 2 ] := ::aTail[ i ][ 2 ]   // col

      DispOutAt( ::aTail[ i + 1 ][ 1 ], ::aTail[ i + 1 ][ 2 ], "*", NToColor( ::aTail[ i + 1 ][ 3 ] ) )

   NEXT

   SWITCH ::nDirection
   CASE DIRECTION_UP
      ::aTail[ 1 ][ 1 ] := ::nRow + 1
      ::aTail[ 1 ][ 2 ] := ::nCol
      EXIT

   CASE DIRECTION_DOWN
      ::aTail[ 1 ][ 1 ] := ::nRow - 1
      ::aTail[ 1 ][ 2 ] := ::nCol
      EXIT

   CASE DIRECTION_LEFT
      ::aTail[ 1 ][ 1 ] := ::nRow
      ::aTail[ 1 ][ 2 ] := ::nCol + 1
      EXIT

   CASE DIRECTION_RIGHT
      ::aTail[ 1 ][ 1 ] := ::nRow
      ::aTail[ 1 ][ 2 ] := ::nCol - 1
      EXIT

   ENDSWITCH

   DispOutAt( ::nRow, ::nCol, "☺", NToColor( 0xc ) )

   RETURN Self

METHOD Move() CLASS Snake

   SWITCH ::nDirection
   CASE DIRECTION_UP    ; ::Up()    ; EXIT
   CASE DIRECTION_DOWN  ; ::Down()  ; EXIT
   CASE DIRECTION_LEFT  ; ::Left()  ; EXIT
   CASE DIRECTION_RIGHT ; ::Right() ; EXIT
   ENDSWITCH

   RETURN Self

METHOD Food() CLASS Snake

   DispOutAt( ::nRowFood, ::nColFood, "*", NToColor( IIF( ::nFoodColor == 0, 8, ::nFoodColor ) ) )

   RETURN Self

METHOD InitFood() CLASS Snake

   ::nRowFood := RandomInt( 1, MaxRow() - 1 )
   ::nColFood := RandomInt( 1, MaxCol() - 1 )

   IF ++::nFoodColor > 0xf
      ::nFoodColor := 0x0
   ENDIF

   RETURN Self

METHOD PrintResults() CLASS Snake

   LOCAL nSeconds := Int( ( MilliSeconds() - ::nStartGame ) / 1000 )

   Scroll()
   SetPos( 0, 0 )

   QOut( "Results:" )
   QOut( "" )
   QOut( "Game duration - ", LTrim( Str( nSeconds ) ), "second(s)" )
   QOut( "Snake length  - ", LTrim( Str( Len( ::aTail ) ) ), "element(s)" )

   Inkey( 0 )

   RETURN Self

// is the mouse on cText, which PadC() centers in nWidth columns of row nRow?
STATIC FUNCTION s_OnLabel( nRow, cText, nWidth )

   LOCAL nLeft := Int( ( nWidth - Len( cText ) ) / 2 )

   RETURN MRow() == nRow .AND. MCol() >= nLeft .AND. MCol() < nLeft + Len( cText )
