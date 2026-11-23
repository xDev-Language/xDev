/*
 * Copyright (c) 2024-2026 xDev-Language
 *
 * Library...: xDev TUI
 * File......: tedit.prg
 *
 */

#include "inkey.ch"
#include "setcurs.ch"
#include "tui.ch"

/* ---------------------------------------------------------------------------------------------
   TEdit
   --------------------------------------------------------------------------------------------- */

CREATE CLASS TEdit

   // Screen rectangle of the editor; its last row is the status line
   DATA top              AS INT    INIT 0
   DATA left             AS INT    INIT 0
   DATA bottom           AS INT    INIT 0
   DATA right            AS INT    INIT 0

   // Name of the edited file
   DATA fileName         AS STRING INIT ""

   // Lines of the text without line ends
   DATA lines            AS ARRAY

   // Line of the cursor (from 1) and its column in characters (from 0)
   DATA line             AS INT    INIT 1
   DATA column           AS INT    INIT 0

   // First line and first column shown on the screen
   DATA firstLine        AS INT    INIT 1
   DATA firstColumn      AS INT    INIT 0

   // .T. inserts typed characters, .F. overwrites the characters under the cursor
   DATA insertMode       AS LOGIC  INIT .T.

   // .T. when the text differs from the file
   DATA changed          AS LOGIC  INIT .F.

   // Text and background color (0xRRGGBB) of the text, of the cursor line and of the status line
   DATA foreground       AS INT    INIT COLOR_TEXT
   DATA background       AS INT    INIT 0x1F1F1F
   DATA lineForeground   AS INT    INIT COLOR_TEXT
   DATA lineBackground   AS INT    INIT COLOR_BLUE_TINT2
   DATA statusForeground AS INT    INIT COLOR_PAPER
   DATA statusBackground AS INT    INIT COLOR_BLUE

   METHOD New( nTop, nLeft, nBottom, nRight, cFileName )
   METHOD Load()
   METHOD Save()
   METHOD Display()
   METHOD HandleKey( nKey )
   METHOD Insert( cText )
   METHOD MoveTo( nLine, nColumn )
   METHOD Rows()
   METHOD Columns()

ENDCLASS

// Create an empty editor in the rectangle nTop, nLeft, nBottom, nRight for the file cFileName
// TEdit():New( nTop, nLeft, nBottom, nRight, cFileName ) --> self
METHOD New( nTop, nLeft, nBottom, nRight, cFileName ) CLASS TEdit

   ::top      := nTop
   ::left     := nLeft
   ::bottom   := nBottom
   ::right    := nRight
   ::fileName := cFileName
   ::lines    := { "" }

   RETURN Self

// Read the file into the editor; return .F. when the file does not exist
// oEdit:Load() --> lSuccess
METHOD Load() CLASS TEdit

   IF ! File( ::fileName )
      RETURN .F.
   ENDIF

   ::lines := ATokens( MemoRead( ::fileName ), .T. )
   IF Len( ::lines ) == 0
      ::lines := { "" }
   ENDIF

   ::line        := 1
   ::column      := 0
   ::firstLine   := 1
   ::firstColumn := 0
   ::changed     := .F.

   RETURN .T.

// Write the text to the file; return .F. when the file cannot be written
// oEdit:Save() --> lSuccess
METHOD Save() CLASS TEdit

   LOCAL cText := ""
   LOCAL i

   FOR i := 1 TO Len( ::lines )
      cText += IIF( i == 1, "", Eol() ) + ::lines[ i ]
   NEXT

   IF ! MemoWrit( ::fileName, cText, .F. )
      RETURN .F.
   ENDIF

   ::changed := .F.

   RETURN .T.

// Draw the visible lines and the status line, and put the cursor in place
// oEdit:Display() --> self
METHOD Display() CLASS TEdit

   LOCAL i
   LOCAL nLine
   LOCAL nFore
   LOCAL nBack
   LOCAL nWidth := ::Columns()
   LOCAL cText
   LOCAL cStatus

   DispBegin()

   FOR i := 0 TO ::Rows() - 1

      nLine := ::firstLine + i

      IF nLine == ::line
         nFore := ::lineForeground
         nBack := ::lineBackground
      ELSE
         nFore := ::foreground
         nBack := ::background
      ENDIF

      IF nLine <= Len( ::lines )
         cText := UPadR( USubStr( ::lines[ nLine ], ::firstColumn + 1, nWidth ), nWidth )
         DispOutAtExt( ::top + i, ::left, cText, nFore, nBack )
      ELSE
         DispOutAtExt( ::top + i, ::left, Space( nWidth ), nFore, nBack )
      ENDIF

   NEXT

   cStatus := " " + ::fileName + IIF( ::changed, " *", "" )
   cStatus += "   Row " + LTrim( Str( ::line ) ) + "  Col " + LTrim( Str( ::column + 1 ) )
   DispOutAtExt( ::bottom, ::left, UPadR( cStatus, nWidth ), ::statusForeground, ::statusBackground )

   SetCursor( IIF( ::insertMode, SC_NORMAL, SC_INSERT ) )
   SetPos( ::top + ::line - ::firstLine, ::left + ::column - ::firstColumn )

   DispEnd()

   RETURN Self

// Handle a key or mouse event from Inkey(), with or without INKEY_EXT; return .F. when the editor
// does not use the event, so the program can handle it
// oEdit:HandleKey( nKey ) --> lHandled
METHOD HandleKey( nKey ) CLASS TEdit

   LOCAL cLine := ::lines[ ::line ]
   LOCAL cText := KeyChar( nKey )
   LOCAL nColumn

   SWITCH KeyStd( nKey )

   CASE K_UP
   CASE K_MWFORWARD
      ::MoveTo( ::line - 1, ::column )
      EXIT

   CASE K_DOWN
   CASE K_MWBACKWARD
      ::MoveTo( ::line + 1, ::column )
      EXIT

   CASE K_LEFT
      ::MoveTo( ::line, ::column - 1 )
      EXIT

   CASE K_RIGHT
      ::MoveTo( ::line, ::column + 1 )
      EXIT

   CASE K_HOME
      ::MoveTo( ::line, 0 )
      EXIT

   CASE K_END
      ::MoveTo( ::line, ULen( cLine ) )
      EXIT

   CASE K_PGUP
      ::MoveTo( ::line - ::Rows(), ::column )
      EXIT

   CASE K_PGDN
      ::MoveTo( ::line + ::Rows(), ::column )
      EXIT

   CASE K_CTRL_PGUP
      ::MoveTo( 1, 0 )
      EXIT

   CASE K_CTRL_PGDN
      ::MoveTo( Len( ::lines ), 0 )
      EXIT

   CASE K_LBUTTONDOWN
      IF MRow() < ::top .OR. MRow() >= ::top + ::Rows() .OR. MCol() < ::left .OR. MCol() > ::right
         RETURN .F.
      ENDIF

      ::MoveTo( ::firstLine + MRow() - ::top, ::firstColumn + MCol() - ::left )
      EXIT

   CASE K_ENTER
      ::lines[ ::line ] := ULeft( cLine, ::column )
      AIns( ::lines, ::line + 1, USubStr( cLine, ::column + 1 ), .T. )
      ::changed := .T.
      ::MoveTo( ::line + 1, 0 )
      EXIT

   CASE K_INS
      ::insertMode := ! ::insertMode
      EXIT

   CASE K_DEL
      IF ::column < ULen( cLine )
         ::lines[ ::line ] := ULeft( cLine, ::column ) + USubStr( cLine, ::column + 2 )
         ::changed := .T.
      ELSEIF ::line < Len( ::lines )
         ::lines[ ::line ] := cLine + ::lines[ ::line + 1 ]
         ADel( ::lines, ::line + 1, .T. )
         ::changed := .T.
      ENDIF
      EXIT

   CASE K_BS
      IF ::column > 0
         ::lines[ ::line ] := ULeft( cLine, ::column - 1 ) + USubStr( cLine, ::column + 1 )
         ::changed := .T.
         ::MoveTo( ::line, ::column - 1 )
      ELSEIF ::line > 1
         nColumn := ULen( ::lines[ ::line - 1 ] )
         ::lines[ ::line - 1 ] := ::lines[ ::line - 1 ] + cLine
         ADel( ::lines, ::line, .T. )
         ::changed := .T.
         ::MoveTo( ::line - 1, nColumn )
      ENDIF
      EXIT

   CASE K_TAB
      ::Insert( Space( 3 ) )
      ::MoveTo( ::line, ::column + 3 )
      EXIT

   OTHERWISE
      IF cText == ""
         RETURN .F.
      ENDIF

      ::Insert( EditPair( cText ) )
      ::MoveTo( ::line, ::column + 1 )

   ENDSWITCH

   ::Display()

   RETURN .T.

// Put cText at the cursor, inserted or over the characters under the cursor; the cursor stays in place
// oEdit:Insert( cText ) --> self
METHOD Insert( cText ) CLASS TEdit

   LOCAL cLine := ::lines[ ::line ]
   LOCAL nSkip := IIF( ::insertMode, 0, ULen( cText ) )

   ::lines[ ::line ] := ULeft( cLine, ::column ) + cText + USubStr( cLine, ::column + 1 + nSkip )
   ::changed := .T.

   RETURN Self

// Move the cursor to line nLine and column nColumn, kept inside the text, and scroll it into view
// oEdit:MoveTo( nLine, nColumn ) --> self
METHOD MoveTo( nLine, nColumn ) CLASS TEdit

   ::line   := Max( 1, Min( nLine, Len( ::lines ) ) )
   ::column := Max( 0, Min( nColumn, ULen( ::lines[ ::line ] ) ) )

   IF ::line < ::firstLine
      ::firstLine := ::line
   ELSEIF ::line >= ::firstLine + ::Rows()
      ::firstLine := ::line - ::Rows() + 1
   ENDIF

   IF ::column < ::firstColumn
      ::firstColumn := ::column
   ELSEIF ::column >= ::firstColumn + ::Columns()
      ::firstColumn := ::column - ::Columns() + 1
   ENDIF

   RETURN Self

// Return the number of text rows, without the status line
// oEdit:Rows() --> nRows
METHOD Rows() CLASS TEdit

   RETURN Max( 1, ::bottom - ::top )

// Return the number of text columns
// oEdit:Columns() --> nColumns
METHOD Columns() CLASS TEdit

   RETURN Max( 1, ::right - ::left + 1 )

// Return the typed character with its closing pair for quotes and brackets
STATIC FUNCTION EditPair( cText )

   SWITCH cText
   CASE '"'
   CASE "'"
      RETURN cText + cText
   CASE "("
      RETURN "()"
   CASE "["
      RETURN "[]"
   CASE "{"
      RETURN "{}"
   ENDSWITCH

   RETURN cText
