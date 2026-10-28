/*
 *
 */

#include "fileio.ch"
#include "gtinfo.ch"
#include "inkey.ch"
#include "tui.ch"

#if OS_LINUX
   REQUEST GTXWC
#else
   REQUEST GTWVT
#endif

#define READ_CHUNK    65536
#define WHEEL_STEP    3
#define VIEW_EVENTS   ( INKEY_KEYBOARD + INKEY_LDOWN + INKEY_MWHEEL + INKEY_GTEVENT )

STATIC nHandle    := -1
STATIC cEOL       := ""
STATIC nFileSize  := 0
STATIC aLineStart := {}

STATIC nTopLine   := 1
STATIC nCurLine   := 1
STATIC nLeftCol   := 0

PROCEDURE Main( cFileName )

   LOCAL lContinue := .T.
   LOCAL nMaxRow := 0, nMaxCol := 0
   LOCAL cLine

   SetColorExt( COLOR_TEXT, COLOR_PAPER )
   Scroll()

   IF cFileName == NIL
      QOut( "" )
      QOut( " Usage: view <cFileName>" )
      QOut( "" )
      QOut( "        where <cFileName> is the name of the file you want to browse" )
      QOut( "" )
      Inkey( 0 )
      RETURN
   ENDIF

   IF ( nHandle := FOpen( cFileName ) ) < 0
      IF FError() != 0
         Alert( "Cannot open file, error: ; ;" + s_OpenErrorText( FError() ) )
      ENDIF
      RETURN
   ENDIF

   cEOL := s_DetectEOL()

   s_IndexLines()

   GtInfo( GTI_WINTITLE, cFileName )

   DO WHILE lContinue

      IF nMaxRow != MaxRow() .OR. nMaxCol != MaxCol()
         nMaxRow := MaxRow()
         nMaxCol := MaxCol()
         s_GoToLine( nCurLine )
      ENDIF

      s_DisplayPage()

      SWITCH Inkey( 0, VIEW_EVENTS )

      CASE K_ESC
      CASE K_CLOSE
         lContinue := .F.
         EXIT

      CASE K_UP
         s_GoToLine( nCurLine - 1 )
         EXIT

      CASE K_DOWN
         s_GoToLine( nCurLine + 1 )
         EXIT

      CASE K_PGUP
         s_ScrollLines( - s_PageHeight() )
         EXIT

      CASE K_PGDN
         s_ScrollLines( s_PageHeight() )
         EXIT

      CASE K_MWFORWARD
         s_ScrollLines( - WHEEL_STEP )
         EXIT

      CASE K_MWBACKWARD
         s_ScrollLines( WHEEL_STEP )
         EXIT

      CASE K_CTRL_HOME
      CASE K_CTRL_PGUP
         s_GoToLine( 1 )
         EXIT

      CASE K_CTRL_END
      CASE K_CTRL_PGDN
         s_GoToLine( s_LineCount() )
         EXIT

      CASE K_LEFT
         nLeftCol := Max( 0, nLeftCol - 1 )
         EXIT

      CASE K_RIGHT
         IF s_LineCount() > 0 .AND. nLeftCol < ULen( s_ReadLine( nCurLine ) )
            nLeftCol++
         ENDIF
         EXIT

      CASE K_HOME
         nLeftCol := 0
         EXIT

      CASE K_END
         IF s_LineCount() > 0
            cLine := s_ReadLine( nCurLine )
            nLeftCol := Max( 0, ULen( cLine ) - MaxCol() )
         ENDIF
         EXIT

      CASE K_LBUTTONDOWN
         IF MRow() < s_PageHeight()
            s_GoToLine( nTopLine + MRow() )
         ENDIF
         EXIT

      ENDSWITCH

   ENDDO

   FClose( nHandle )

   RETURN

STATIC FUNCTION s_OpenErrorText( nError )

   LOCAL aError := { ;
      { 0,  "Successful" }, ;
      { 2,  "File not found" }, ;
      { 3,  "Path not found" }, ;
      { 4,  "Too many files open" }, ;
      { 5,  "Access denied" }, ;
      { 6,  "Invalid handle" }, ;
      { 8,  "Insufficient memory" }, ;
      { 15, "Invalid drive specified" }, ;
      { 19, "Attempted to write to a write-protected disk" }, ;
      { 21, "Drive not ready" }, ;
      { 23, "Data CRC error" }, ;
      { 29, "Write fault" }, ;
      { 30, "Read fault" }, ;
      { 32, "Sharing violation" }, ;
      { 33, "Lock Violation" } }
   LOCAL nPos := AScan( aError, {| x | x[ 1 ] == nError } )

   IF nPos > 0
      RETURN aError[ nPos ][ 2 ]
   ENDIF

   /* Will display the error number that is not in the Array aError */
   RETURN LTrim( Str( nError ) )

STATIC FUNCTION s_DetectEOL()

   LOCAL cBuffer := Space( 4096 )
   LOCAL nRead

   nRead := FRead( nHandle, @cBuffer, 4096 )
   FSeek( nHandle, 0, FS_SET )
   RETURN Eol( Left( cBuffer, nRead ) )

STATIC FUNCTION s_EOLName()

   DO CASE
   CASE cEOL == Chr( 13 ) + Chr( 10 )
      RETURN "CRLF"
   CASE cEOL == Chr( 10 )
      RETURN "LF"
   ENDCASE

   RETURN "CR"

STATIC PROCEDURE s_IndexLines()

   LOCAL cSearch := Right( cEOL, 1 )
   LOCAL cBuffer
   LOCAL nOffset := 0
   LOCAL nRead
   LOCAL nPos

   nFileSize  := FSeek( nHandle, 0, FS_END )
   aLineStart := { 0 }
   FSeek( nHandle, 0, FS_SET )

   DO WHILE nOffset < nFileSize
      cBuffer := Space( READ_CHUNK )
      nRead := FRead( nHandle, @cBuffer, READ_CHUNK )
      IF nRead <= 0
         EXIT
      ENDIF
      nPos := At( cSearch, cBuffer )
      DO WHILE nPos > 0
         AAdd( aLineStart, nOffset + nPos )
         nPos := At( cSearch, cBuffer, nPos + 1 )
      ENDDO
      nOffset += nRead
   ENDDO

   IF ATail( aLineStart ) < nFileSize
      AAdd( aLineStart, nFileSize + Len( cEOL ) )
   ENDIF

   RETURN

STATIC FUNCTION s_LineCount()

   RETURN Len( aLineStart ) - 1

STATIC FUNCTION s_ReadLine( nLine, nMaxBytes )

   LOCAL nBytes := aLineStart[ nLine + 1 ] - aLineStart[ nLine ] - Len( cEOL )
   LOCAL cLine

   IF nMaxBytes != NIL
      nBytes := Min( nBytes, nMaxBytes )
   ENDIF

   cLine := Space( nBytes )
   FSeek( nHandle, aLineStart[ nLine ], FS_SET )
   FRead( nHandle, @cLine, nBytes )

   RETURN cLine

STATIC FUNCTION s_PageHeight()

   RETURN MaxRow()

STATIC PROCEDURE s_GoToLine( nLine )

   nCurLine := Max( 1, Min( nLine, s_LineCount() ) )

   IF nCurLine < nTopLine
      nTopLine := nCurLine
   ELSEIF nCurLine >= nTopLine + s_PageHeight()
      nTopLine := nCurLine - s_PageHeight() + 1
   ENDIF

   nTopLine := Max( 1, Min( nTopLine, s_LineCount() - s_PageHeight() + 1 ) )

   RETURN

STATIC PROCEDURE s_ScrollLines( nLines )

   nTopLine += nLines
   s_GoToLine( nCurLine + nLines )

   RETURN

STATIC PROCEDURE s_DisplayPage()

   LOCAL nWidth := MaxCol() + 1
   LOCAL nBackground
   LOCAL nLine
   LOCAL nRow

   DispBegin()

   FOR nRow := 0 TO s_PageHeight() - 1
      nLine := nTopLine + nRow
      nBackground := IIF( nLine == nCurLine, COLOR_BLUE_TINT1, COLOR_PAPER )
      DispOutAtExt( nRow, 0, Space( nWidth ), COLOR_TEXT, nBackground )
      IF nLine <= s_LineCount()
         DispOutAtExt( nRow, 0, USubStr( s_ReadLine( nLine, ( nLeftCol + nWidth ) * 4 ), nLeftCol + 1 ), COLOR_TEXT, ;
            nBackground )
      ENDIF
   NEXT

   DispOutAtExt( MaxRow(), 0, PadR( s_StatusText(), nWidth ), COLOR_PAPER, COLOR_BLUE )

   DispEnd()

   RETURN

STATIC FUNCTION s_StatusText()

   RETURN " Line(" + s_NumberText( nCurLine ) + "/" + s_NumberText( s_LineCount() ) + ")" + ;
          " Col(" + s_NumberText( nLeftCol + 1 ) + ")" + ;
          " EOL(" + s_EOLName() + ")" + ;
          " Pointer(" + s_NumberText( aLineStart[ nCurLine ] ) + ")" + ;
          " Size(" + s_NumberText( nFileSize ) + ")"

STATIC FUNCTION s_NumberText( nValue )

   RETURN LTrim( Transform( nValue, "9 999 999 999" ) )
