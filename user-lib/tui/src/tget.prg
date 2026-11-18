/*
 * Copyright (c) 2024-2026 xDev-Language
 *
 * Library: xDev TUI
 * File: tget.prg
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "inkey.ch"
#include "set.ch"
#include "button.ch"
#include "getexit.ch"
#include "tui.ch"

// Template characters that take a character of the value; the other characters are shown as they are
#define SLOTS           "9#ANXLY!$*"

/* ---------------------------------------------------------------------------------------------
   Get
   --------------------------------------------------------------------------------------------- */

CREATE CLASS Get

   // Screen position of the first character
   DATA row        AS INT       INIT 0
   DATA col        AS INT       INIT 0

   // Block that reads the variable without an argument and assigns it with one
   DATA block      AS CODEBLOCK
   DATA name       AS STRING    INIT ""
   DATA picture    AS STRING    INIT ""

   // { nForeground, nBackground } without and with the input focus
   DATA colorSpec  AS ARRAY     INIT { { COLOR_TEXT, COLOR_PAPER }, { COLOR_PAPER, COLOR_BLUE } }

   // Blocks of WHEN, VALID and of a special READ behavior; each gets the Get object
   DATA preBlock   AS CODEBLOCK
   DATA postBlock  AS CODEBLOCK
   DATA reader     AS CODEBLOCK

   DATA cargo
   DATA message    AS STRING    INIT ""
   DATA subscript
   DATA exitState  AS INT       INIT GE_NOEXIT

   // Editing state, valid while the Get has the input focus; pos and decPos count characters
   DATA hasFocus   AS LOGIC     INIT .F.
   DATA buffer     AS STRING    INIT ""
   DATA original
   DATA type       AS STRING    INIT ""
   DATA pos        AS INT       INIT 0
   DATA decPos     AS INT       INIT 0
   DATA changed    AS LOGIC     INIT .F.
   DATA clear      AS LOGIC     INIT .F.
   DATA minus      AS LOGIC     INIT .F.
   DATA badDate    AS LOGIC     INIT .F.
   DATA rejected   AS LOGIC     INIT .F.
   DATA typeOut    AS LOGIC     INIT .F.

   // Characters of the buffer hidden on the left of the @S<n> window
   DATA offset     AS INT       INIT 0

   METHOD New( nRow, nCol, bBlock, cName, cPicture, aColorSpec )

   METHOD Assign()
   METHOD ColorDisp( aColorSpec )
   METHOD Display()
   METHOD HitTest( nRow, nCol )
   METHOD KillFocus()
   METHOD Reset()
   METHOD SetFocus()
   METHOD Undo()
   METHOD UnTransform()
   METHOD UpdateBuffer()
   METHOD VarGet()
   METHOD VarPut( xValue )

   METHOD End()
   METHOD Home()
   METHOD Left()
   METHOD Right()
   METHOD ToDecPos()
   METHOD WordLeft()
   METHOD WordRight()

   METHOD BackSpace()
   METHOD Delete()
   METHOD DelEnd()
   METHOD DelLeft()
   METHOD DelRight()
   METHOD DelWordLeft()
   METHOD DelWordRight()

   METHOD Insert( cChar )
   METHOD OverStrike( cChar )

HIDDEN:

   // Picture functions after @ and the template, one template character for each buffer character
   DATA functions  AS STRING    INIT ""
   DATA mask       AS STRING    INIT ""

   METHOD Edit( cChar, lInsert )
   METHOD Editable( nPos )
   METHOD FirstPos()
   METHOD LastPos()
   METHOD Format( xValue )
   METHOD Prepare()
   METHOD ScrollWidth()

ENDCLASS

// A Get at nRow, nCol for the variable of bBlock
METHOD New( nRow, nCol, bBlock, cName, cPicture, aColorSpec ) CLASS Get

   ::row   := IIF( nRow == NIL, Row(), nRow )
   ::col   := IIF( nCol == NIL, Col(), nCol )
   ::block := bBlock
   ::name  := IIF( cName == NIL, "", cName )
   IF cPicture != NIL
      ::picture := cPicture
   ENDIF

   IF aColorSpec != NIL
      ::colorSpec := aColorSpec
   ENDIF

   RETURN Self

/* ---------------------------------------------------------------------------------------------
   State change
   --------------------------------------------------------------------------------------------- */

// Put the value of the buffer into the variable
METHOD Assign() CLASS Get

   IF ::hasFocus
      ::VarPut( ::UnTransform() )
   ENDIF

   RETURN Self

// Set the colors and show the Get again
METHOD ColorDisp( aColorSpec ) CLASS Get

   IF aColorSpec != NIL
      ::colorSpec := aColorSpec
   ENDIF

   RETURN ::Display()

// The buffer while editing, otherwise the formatted value; the cursor stands at pos;
// with @S<n> only n characters are shown and the window follows the cursor
METHOD Display() CLASS Get

   LOCAL aColor := ::colorSpec[ IIF( ::hasFocus, 2, 1 ) ]
   LOCAL cText  := IIF( ::hasFocus, ::buffer, ::Format( ::VarGet() ) )
   LOCAL nWidth := ::ScrollWidth()

   IF ! ::hasFocus .OR. nWidth == 0
      ::offset := 0
   ELSEIF ::pos - 1 < ::offset
      ::offset := ::pos - 1
   ELSEIF ::pos > ::offset + nWidth
      ::offset := ::pos - nWidth
   ENDIF

   IF nWidth > 0
      cText := UPadR( USubStr( cText, ::offset + 1, nWidth ), nWidth )
   ENDIF

   DispOutAtExt( ::row, ::col, cText, aColor[ 1 ], aColor[ 2 ] )
   IF ::hasFocus
      SetPos( ::row, ::col + TextWidth( USubStr( ::buffer, ::offset + 1, ::pos - 1 - ::offset ) ) )
   ENDIF

   RETURN Self

// HTCLIENT when the screen position is on the Get, otherwise HTNOWHERE
METHOD HitTest( nRow, nCol ) CLASS Get

   LOCAL nWidth := TextWidth( IIF( ::hasFocus, ::buffer, ::Format( ::VarGet() ) ) )

   IF ::ScrollWidth() > 0
      nWidth := Min( nWidth - ::offset, ::ScrollWidth() )
   ENDIF

   IF nRow == ::row .AND. nCol >= ::col .AND. nCol < ::col + nWidth
      RETURN HTCLIENT
   ENDIF

   RETURN HTNOWHERE

// The Get leaves editing and shows the value of the variable
METHOD KillFocus() CLASS Get

   ::hasFocus := .F.
   ::buffer   := ""
   ::pos      := 0
   ::Display()

   RETURN Self

// The buffer again from the variable
METHOD Reset() CLASS Get

   ::Prepare()
   ::Display()

   RETURN Self

// The Get starts editing: the buffer from the variable, the cursor on the first place
METHOD SetFocus() CLASS Get

   ::hasFocus  := .T.
   ::original  := ::VarGet()
   ::type      := ValType( ::original )
   ::exitState := GE_NOEXIT
   ::changed   := .F.
   ::Prepare()
   ::Display()

   RETURN Self

// The value from before the editing back in the variable and in the buffer
METHOD Undo() CLASS Get

   IF ::hasFocus
      ::VarPut( ::original )
      ::Prepare()
      ::changed := .F.
      ::Display()
   ENDIF

   RETURN Self

// The value of the buffer in the type of the variable, the inverse of Transform()
METHOD UnTransform() CLASS Get

   LOCAL cText := ""
   LOCAL cChar
   LOCAL nPos

   SWITCH ::type
   CASE "C"
      IF "R" $ ::functions
         FOR nPos := 1 TO ULen( ::buffer )
            IF ::Editable( nPos )
               cText += USubStr( ::buffer, nPos, 1 )
            ENDIF
         NEXT
         RETURN cText
      ENDIF
      RETURN ::buffer
   CASE "N"
      FOR nPos := 1 TO ULen( ::buffer )
         cChar := USubStr( ::buffer, nPos, 1 )
         IF IsDigit( cChar ) .OR. cChar == "."
            cText += cChar
         ENDIF
      NEXT
      RETURN IIF( ::minus .OR. "-" $ ::buffer .OR. "(" $ ::buffer, -Val( cText ), Val( cText ) )
   CASE "D"
      RETURN CToD( ::buffer )
   CASE "L"
      RETURN Upper( Left( AllTrim( ::buffer ), 1 ) ) $ "TY"
   ENDSWITCH

   RETURN ::buffer

// The buffer again from the variable, after the variable changed outside the Get
METHOD UpdateBuffer() CLASS Get

   IF ::hasFocus
      ::Prepare()
   ENDIF

   RETURN ::Display()

// The value of the variable
METHOD VarGet() CLASS Get

   RETURN Eval( ::block )

// Set the variable
METHOD VarPut( xValue ) CLASS Get

   Eval( ::block, xValue )

   RETURN xValue

/* ---------------------------------------------------------------------------------------------
   Cursor movement; the cursor stops only at characters that can be edited
   --------------------------------------------------------------------------------------------- */

// The cursor after the last character that is not a space
METHOD End() CLASS Get

   LOCAL nPos := ::LastPos()

   // After the last character that is not a space, if there is room
   DO WHILE nPos > ::FirstPos() .AND. USubStr( ::buffer, nPos, 1 ) == " "
      IF ::Editable( nPos - 1 ) .AND. !( USubStr( ::buffer, nPos - 1, 1 ) == " " )
         EXIT
      ENDIF

      nPos--
   ENDDO

   ::pos   := nPos
   ::clear := .F.

   RETURN ::Display()

// The cursor on the first editable character
METHOD Home() CLASS Get

   ::pos   := ::FirstPos()
   ::clear := .F.

   RETURN ::Display()

// The cursor one editable character to the left; typeOut at the start
METHOD Left() CLASS Get

   LOCAL nPos := ::pos - 1

   DO WHILE nPos >= 1 .AND. ! ::Editable( nPos )
      nPos--
   ENDDO

   ::typeOut := nPos < 1
   IF ! ::typeOut
      ::pos := nPos
   ENDIF

   ::clear := .F.

   RETURN ::Display()

// The cursor one editable character to the right; typeOut at the end
METHOD Right() CLASS Get

   LOCAL nPos := ::pos + 1

   DO WHILE nPos <= ULen( ::buffer ) .AND. ! ::Editable( nPos )
      nPos++
   ENDDO

   ::typeOut := nPos > ULen( ::buffer )
   IF ! ::typeOut
      ::pos := nPos
   ENDIF

   ::clear := .F.

   RETURN ::Display()

// The cursor after the decimal point of a number
METHOD ToDecPos() CLASS Get

   IF ::decPos > 0
      ::pos   := ::decPos + 1
      ::clear := .F.
   ENDIF

   RETURN ::Display()

// The cursor to the start of the previous word
METHOD WordLeft() CLASS Get

   LOCAL nPos := ::pos - 1

   DO WHILE nPos > 1 .AND. USubStr( ::buffer, nPos, 1 ) == " "
      nPos--
   ENDDO

   DO WHILE nPos > 1 .AND. ! USubStr( ::buffer, nPos - 1, 1 ) == " "
      nPos--
   ENDDO

   ::pos   := Max( nPos, ::FirstPos() )
   ::clear := .F.

   RETURN ::Display()

// The cursor to the start of the next word
METHOD WordRight() CLASS Get

   LOCAL nLen := ULen( ::buffer )
   LOCAL nPos := ::pos

   DO WHILE nPos < nLen .AND. ! USubStr( ::buffer, nPos, 1 ) == " "
      nPos++
   ENDDO

   DO WHILE nPos < nLen .AND. USubStr( ::buffer, nPos, 1 ) == " "
      nPos++
   ENDDO

   ::pos   := Min( nPos, ::LastPos() )
   ::clear := .F.

   RETURN ::Display()

/* ---------------------------------------------------------------------------------------------
   Editing; a deleted character pulls the rest of its group of editable characters to the left
   --------------------------------------------------------------------------------------------- */

// Delete the character left of the cursor
METHOD BackSpace() CLASS Get

   LOCAL nPos := ::pos

   ::Left()
   IF ::pos < nPos
      ::Delete()
   ENDIF

   RETURN Self

// Delete the character under the cursor
METHOD Delete() CLASS Get

   LOCAL nEnd := ::pos

   IF ::Editable( ::pos )
      DO WHILE nEnd < ULen( ::buffer ) .AND. ::Editable( nEnd + 1 )
         nEnd++
      ENDDO

      ::buffer  := UStuff( ::buffer, ::pos, nEnd - ::pos + 1, USubStr( ::buffer, ::pos + 1, nEnd - ::pos ) + " " )
      ::changed := .T.
   ENDIF

   RETURN ::Display()

// Delete from the cursor to the end
METHOD DelEnd() CLASS Get

   LOCAL nPos

   FOR nPos := ::pos TO ULen( ::buffer )
      IF ::Editable( nPos )
         ::buffer := UStuff( ::buffer, nPos, 1, " " )
      ENDIF
   NEXT

   ::changed := .T.

   RETURN ::Display()

// Delete the character left of the cursor, as BackSpace()
METHOD DelLeft() CLASS Get

   RETURN ::BackSpace()

// Delete the next editable character right of the cursor
METHOD DelRight() CLASS Get

   LOCAL nPos := ::pos

   ::Right()
   IF ::pos > nPos
      ::Delete()
   ENDIF

   RETURN Self

// Delete the word left of the cursor
METHOD DelWordLeft() CLASS Get

   LOCAL nPos := ::pos

   ::WordLeft()
   DO WHILE ::pos < nPos
      ::Delete()
      nPos--
   ENDDO

   RETURN Self

// Delete the word under the cursor and the spaces after it
METHOD DelWordRight() CLASS Get

   DO WHILE ::Editable( ::pos ) .AND. ! USubStr( ::buffer, ::pos, 1 ) == " "
      ::Delete()
   ENDDO

   DO WHILE ::Editable( ::pos ) .AND. USubStr( ::buffer, ::pos, 1 ) == " "
      IF Empty( USubStr( ::buffer, ::pos ) )
         EXIT
      ENDIF

      ::Delete()
   ENDDO

   RETURN Self

/* ---------------------------------------------------------------------------------------------
   Text entry; cChar is one UTF-8 character
   --------------------------------------------------------------------------------------------- */

// Insert cChar at the cursor
METHOD Insert( cChar ) CLASS Get

   RETURN ::Edit( cChar, .T. )

// Write cChar over the character under the cursor
METHOD OverStrike( cChar ) CLASS Get

   RETURN ::Edit( cChar, .F. )

// Puts cChar at pos when the template allows it, and moves the cursor to the next editable character
METHOD Edit( cChar, lInsert ) CLASS Get

   LOCAL cSlot
   LOCAL nEnd
   LOCAL nPos

   ::rejected := .F.
   IF ! ::hasFocus .OR. ! ::Editable( ::pos )
      ::rejected := .T.
      RETURN Self
   ENDIF

   IF ::type == "N" .AND. cChar == "-"
      ::minus := .T.
   ENDIF

   cSlot := Upper( USubStr( ::mask, ::pos, 1 ) )
   cChar := Accepted( cChar, cSlot, ::type, "!" $ ::functions )
   IF cChar == NIL
      ::rejected := .T.
      RETURN Self
   ENDIF

   // @K, and the first key of a number: a new value instead of the old one
   IF ::clear
      FOR nPos := 1 TO ULen( ::buffer )
         IF ::Editable( nPos )
            ::buffer := UStuff( ::buffer, nPos, 1, " " )
         ENDIF
      NEXT

      ::pos   := ::FirstPos()
      ::clear := .F.
   ENDIF

   IF lInsert
      nEnd := ::pos
      DO WHILE nEnd < ULen( ::buffer ) .AND. ::Editable( nEnd + 1 )
         nEnd++
      ENDDO

      ::buffer := UStuff( ::buffer, ::pos, nEnd - ::pos + 1, cChar + USubStr( ::buffer, ::pos, nEnd - ::pos ) )
   ELSE
      ::buffer := UStuff( ::buffer, ::pos, 1, cChar )
   ENDIF

   ::changed := .T.
   IF ::type == "D"
      ::badDate := Empty( CToD( ::buffer ) ) .AND. ! Empty( StrTran( ::buffer, "/" ) )
   ENDIF

   RETURN ::Right()

/* ---------------------------------------------------------------------------------------------
   Helpers
   --------------------------------------------------------------------------------------------- */

// .T. when the template takes a character at nPos
METHOD Editable( nPos ) CLASS Get

   RETURN nPos >= 1 .AND. nPos <= ULen( ::mask ) .AND. Upper( USubStr( ::mask, nPos, 1 ) ) $ SLOTS

// The first editable position
METHOD FirstPos() CLASS Get

   LOCAL nPos := 1

   DO WHILE nPos < ULen( ::buffer ) .AND. ! ::Editable( nPos )
      nPos++
   ENDDO

   RETURN nPos

// The last editable position
METHOD LastPos() CLASS Get

   LOCAL nPos := ULen( ::buffer )

   DO WHILE nPos > 1 .AND. ! ::Editable( nPos )
      nPos--
   ENDDO

   RETURN nPos

// The value as it is shown: through Transform() with a picture, otherwise in the usual form
METHOD Format( xValue ) CLASS Get

   IF ! Empty( ::picture )
      RETURN Transform( xValue, ::picture )
   ENDIF

   SWITCH ValType( xValue )
   CASE "N"
      RETURN Str( xValue )
   CASE "D"
      RETURN DToC( xValue )
   CASE "L"
      RETURN IIF( xValue, "T", "F" )
   CASE "C"
      RETURN xValue
   ENDSWITCH

   RETURN ""

// Buffer, template and positions from the current value of the variable
METHOD Prepare() CLASS Get

   LOCAL cPicture := ::picture
   LOCAL cChar
   LOCAL nPos

   ::functions := ""
   IF Left( cPicture, 1 ) == "@"
      nPos := At( " ", cPicture )
      ::functions := Upper( SubStr( cPicture, 2, IIF( nPos > 0, nPos - 2, Len( cPicture ) ) ) )
      cPicture := IIF( nPos > 0, SubStr( cPicture, nPos + 1 ), "" )
   ENDIF

   ::buffer := ::Format( ::VarGet() )

   // Without a template every character of the buffer can be edited, a number keeps its point
   IF Empty( cPicture )
      cPicture := ""
      FOR nPos := 1 TO ULen( ::buffer )
         cChar := USubStr( ::buffer, nPos, 1 )
         DO CASE
         CASE ::type == "N"
            cPicture += IIF( cChar == ".", ".", "9" )
         CASE ::type == "D"
            // From SET DATE FORMAT, also for an empty date: y, m and d take a digit
            cPicture += IIF( Upper( USubStr( Set( _SET_DATEFORMAT ), nPos, 1 ) ) $ "YMD", "9", cChar )
         CASE ::type == "L"
            cPicture += "L"
         OTHERWISE
            cPicture += "X"
         ENDCASE
      NEXT
   ENDIF

   ::mask := ULeft( cPicture + Replicate( "X", Max( ULen( ::buffer ) - ULen( cPicture ), 0 ) ), ULen( ::buffer ) )

   ::decPos  := IIF( ::type == "N", UAt( ".", ::mask ), 0 )
   ::minus   := ::type == "N" .AND. ValType( ::VarGet() ) == "N" .AND. ::VarGet() < 0
   ::clear   := "K" $ ::functions .OR. ::type == "N"
   ::badDate := .F.
   ::typeOut := .F.
   ::pos     := ::FirstPos()
   ::offset  := 0

   RETURN Self

// Columns of the @S<n> window, 0 without @S
METHOD ScrollWidth() CLASS Get

   LOCAL cFunctions
   LOCAL nPos

   IF ! Left( ::picture, 1 ) == "@"
      RETURN 0
   ENDIF

   nPos       := At( " ", ::picture )
   cFunctions := Upper( SubStr( ::picture, 2, IIF( nPos > 0, nPos - 2, Len( ::picture ) ) ) )
   nPos       := At( "S", cFunctions )

   RETURN IIF( nPos > 0, Val( SubStr( cFunctions, nPos + 1 ) ), 0 )

/* ---------------------------------------------------------------------------------------------
   Functions of this file
   --------------------------------------------------------------------------------------------- */

// cChar as the template character cSlot takes it, NIL when cSlot does not accept it
STATIC FUNCTION Accepted( cChar, cSlot, cType, lUpper )

   LOCAL lLetter := IsAlpha( cChar )

   IF cType == "N" .AND. cSlot $ "9#$*"
      RETURN IIF( IsDigit( cChar ) .OR. cChar $ "+-", cChar, NIL )
   ENDIF

   SWITCH cSlot
   CASE "9"
      RETURN IIF( IsDigit( cChar ), cChar, NIL )
   CASE "#"
      RETURN IIF( IsDigit( cChar ) .OR. cChar $ " +-", cChar, NIL )
   CASE "A"
      IF ! lLetter
         RETURN NIL
      ENDIF

      RETURN IIF( lUpper, Upper( cChar ), cChar )
   CASE "N"
      IF ! lLetter .AND. ! IsDigit( cChar )
         RETURN NIL
      ENDIF

      RETURN IIF( lUpper, Upper( cChar ), cChar )
   CASE "L"
      IF !( Upper( cChar ) $ "TFYN" )
         RETURN NIL
      ENDIF

      RETURN IIF( Upper( cChar ) $ "TY", "T", "F" )
   CASE "Y"
      IF !( Upper( cChar ) $ "YNTF" )
         RETURN NIL
      ENDIF

      RETURN IIF( Upper( cChar ) $ "YT", "Y", "N" )
   CASE "!"
      RETURN Upper( cChar )
   ENDSWITCH

   RETURN IIF( lUpper, Upper( cChar ), cChar )

// Screen columns of a text; the only place that measures text
STATIC FUNCTION TextWidth( cText )

   RETURN ULen( cText )
