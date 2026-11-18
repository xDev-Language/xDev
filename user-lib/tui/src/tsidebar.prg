/*
 * Copyright (c) 2024-2026 xDev-Language
 *
 * Library: xDev TUI
 * File: tsidebar.prg
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "inkey.ch"
#include "tui.ch"

// Rows taken by the title and the separator above the items
#define HEADER_ROWS     4

/* ---------------------------------------------------------------------------------------------
   TSidebar
   --------------------------------------------------------------------------------------------- */

CREATE CLASS TSidebar

   // Top row and left column of the sidebar
   DATA row             AS INT    INIT 0
   DATA col             AS INT    INIT 0

   // Width of the sidebar in columns
   DATA width           AS INT    INIT 23

   // Title shown above the separator; the items never scroll over it
   DATA title           AS STRING INIT ""

   // Character repeated in the line under the title
   DATA separator       AS STRING INIT "─"

   // Items of the sidebar; an item is a text, or { cText, aSubItems } for a group with a submenu;
   // every line takes three rows
   DATA items           AS ARRAY

   // .T. for every group whose submenu is open, one element per item
   DATA expanded        AS ARRAY

   // Text and background color (0xRRGGBB) of the sidebar and of its items
   DATA foreground      AS INT    INIT COLOR_PAPER
   DATA background      AS INT    INIT COLOR_BLUE

   // Colors of the line under the mouse or the keyboard cursor
   DATA hoverForeground AS INT    INIT COLOR_PAPER
   DATA hoverBackground AS INT    INIT COLOR_BLUE_DARK

   // Colors of the line while the left mouse button holds it down
   DATA pressForeground AS INT    INIT COLOR_BLUE_DARK
   DATA pressBackground AS INT    INIT COLOR_BLUE_TINT1

   // .T. shows the press colors while a line is held down
   DATA pressEffect     AS LOGIC  INIT .T.

   // Number of the line under the mouse or the keyboard cursor, 0 when none
   DATA hover           AS INT    INIT 0

   // Number of the line held down by the left mouse button, 0 when none
   DATA pressed         AS INT    INIT 0

   // Number of the last chosen item, 0 when none; submenu items are counted in order with the others
   DATA active          AS INT    INIT 0

   // Number of the first visible line
   DATA first           AS INT    INIT 1

   METHOD New( nRow, nCol, nWidth, cTitle, aItems )
   METHOD Display()
   METHOD HandleKey( nKey )
   METHOD Choose( nLine )
   METHOD Lines()
   METHOD Right()
   METHOD LineAt( nRow, nCol )
   METHOD Visible()

ENDCLASS

// Create a sidebar at nRow, nCol with nWidth columns, the title cTitle and the items of aItems
// TSidebar():New( nRow, nCol, nWidth, cTitle, aItems ) --> self
METHOD New( nRow, nCol, nWidth, cTitle, aItems ) CLASS TSidebar

   ::row      := nRow
   ::col      := nCol
   ::width    := nWidth
   ::title    := cTitle
   ::items    := aItems
   ::expanded := AFill( Array( Len( aItems ) ), .F. )

   RETURN Self

// Draw the sidebar down to the last screen row
// oSidebar:Display() --> self
METHOD Display() CLASS TSidebar

   LOCAL i
   LOCAL nRow
   LOCAL nLast
   LOCAL nRight := ::Right()
   LOCAL aLines := ::Lines()
   LOCAL nItem
   LOCAL nSub
   LOCAL nFore
   LOCAL nBack

   DispBegin()

   DispBoxExt( ::row, ::col, MaxRow(), nRight, Space( 9 ), ::foreground, ::background )

   DispOutAtExt( ::row + 1, ::col + 2, ::title, ::foreground, ::background )
   DispOutAtExt( ::row + 3, ::col, Replicate( ::separator, nRight - ::col + 1 ), ::foreground, ::background )

   IF ::hover > Len( aLines )
      ::hover := 0
   ENDIF

   ::first := Max( 1, Min( ::first, Len( aLines ) - ::Visible() + 1 ) )
   nLast   := Min( Len( aLines ), ::first + ::Visible() - 1 )

   FOR i := ::first TO nLast

      nRow  := ::row + HEADER_ROWS + ( i - ::first ) * 3
      nItem := aLines[ i ][ 1 ]
      nSub  := aLines[ i ][ 2 ]

      IF ::pressEffect .AND. i == ::pressed
         nFore := ::pressForeground
         nBack := ::pressBackground
      ELSEIF i == ::hover
         nFore := ::hoverForeground
         nBack := ::hoverBackground
      ELSE
         nFore := ::foreground
         nBack := ::background
      ENDIF

      DispBoxExt( nRow, ::col, nRow + 2, nRight, Space( 9 ), nFore, nBack )

      IF nSub > 0
         DispOutAtExt( nRow + 1, ::col + 3, "▸ " + ::items[ nItem ][ 2 ][ nSub ], nFore, nBack )
      ELSEIF ValType( ::items[ nItem ] ) == "A"
         DispOutAtExt( nRow + 1, ::col + 2, ::items[ nItem ][ 1 ], nFore, nBack )
         DispOutAtExt( nRow + 1, nRight - 2, IIF( ::expanded[ nItem ], "▴", "▾" ), nFore, nBack )
      ELSE
         DispOutAtExt( nRow + 1, ::col + 2, ::items[ nItem ], nFore, nBack )
      ENDIF

   NEXT

   DispEnd()

   RETURN Self


// Handle a key or mouse event from Inkey(); return .T. when the user chose an item,
// its number is then in oSidebar:active; choosing a group opens or closes its submenu
// oSidebar:HandleKey( nKey ) --> lChosen
METHOD HandleKey( nKey ) CLASS TSidebar

   LOCAL lChosen  := .F.
   LOCAL lRedraw  := .F.
   LOCAL nVisible := ::Visible()
   LOCAL nLines   := Len( ::Lines() )
   LOCAL nLine

   SWITCH nKey

   CASE K_MOUSEMOVE
      nLine   := ::LineAt( MRow(), MCol() )
      lRedraw := nLine != ::hover
      ::hover := nLine
      EXIT

   CASE K_LBUTTONDOWN
      nLine := ::LineAt( MRow(), MCol() )
      IF nLine > 0
         ::pressed := nLine
         lChosen   := ::Choose( nLine )
         lRedraw   := .T.
      ENDIF
      EXIT

   CASE K_LBUTTONUP
      lRedraw   := ::pressed > 0
      ::pressed := 0
      EXIT

   CASE K_MWFORWARD
      IF ::LineAt( MRow(), MCol() ) > 0 .AND. ::first > 1
         ::first--
         ::hover := ::LineAt( MRow(), MCol() )
         lRedraw := .T.
      ENDIF
      EXIT

   CASE K_MWBACKWARD
      IF ::LineAt( MRow(), MCol() ) > 0 .AND. ::first + nVisible <= nLines
         ::first++
         ::hover := ::LineAt( MRow(), MCol() )
         lRedraw := .T.
      ENDIF
      EXIT

   CASE K_UP
      ::hover := IIF( ::hover > 1, ::hover - 1, nLines )
      lRedraw := .T.
      EXIT

   CASE K_DOWN
      ::hover := IIF( ::hover < nLines, ::hover + 1, 1 )
      lRedraw := .T.
      EXIT

   CASE K_HOME
      ::hover := 1
      lRedraw := .T.
      EXIT

   CASE K_END
      ::hover := nLines
      lRedraw := .T.
      EXIT

   CASE K_ENTER
      IF ::hover > 0
         lChosen := ::Choose( ::hover )
         lRedraw := .T.
      ENDIF
      EXIT

   ENDSWITCH

   IF lRedraw .AND. nKey != K_MOUSEMOVE .AND. nKey != K_LBUTTONDOWN .AND. nKey != K_LBUTTONUP .AND. nVisible > 0
      IF ::hover < ::first
         ::first := ::hover
      ELSEIF ::hover > ::first + nVisible - 1
         ::first := ::hover - nVisible + 1
      ENDIF
   ENDIF

   IF lRedraw
      ::Display()
   ENDIF

   RETURN lChosen

// Choose line nLine: a group opens or closes its submenu and returns .F., any other item
// becomes oSidebar:active and returns .T.
// oSidebar:Choose( nLine ) --> lChosen
METHOD Choose( nLine ) CLASS TSidebar

   LOCAL aLine := ::Lines()[ nLine ]
   LOCAL nItem := aLine[ 1 ]
   LOCAL nNumber
   LOCAL i

   IF aLine[ 2 ] == 0 .AND. ValType( ::items[ nItem ] ) == "A"
      ::expanded[ nItem ] := ! ::expanded[ nItem ]
      RETURN .F.
   ENDIF

   nNumber := Max( 1, aLine[ 2 ] )

   FOR i := 1 TO nItem - 1
      nNumber += IIF( ValType( ::items[ i ] ) == "A", Len( ::items[ i ][ 2 ] ), 1 )
   NEXT

   ::active := nNumber

   RETURN .T.

// Return the lines shown now as { nItem, nSub } pairs; nSub is 0 for an item outside a submenu
// oSidebar:Lines() --> aLines
METHOD Lines() CLASS TSidebar

   LOCAL aLines := {}
   LOCAL i
   LOCAL j

   FOR i := 1 TO Len( ::items )
      AAdd( aLines, { i, 0 } )
      IF ValType( ::items[ i ] ) == "A" .AND. ::expanded[ i ]
         FOR j := 1 TO Len( ::items[ i ][ 2 ] )
            AAdd( aLines, { i, j } )
         NEXT
      ENDIF
   NEXT

   RETURN aLines

// Return the last column taken by the sidebar
// oSidebar:Right() --> nCol
METHOD Right() CLASS TSidebar

   RETURN ::col + ::width - 1

// Return the number of the line at screen position nRow, nCol, 0 when there is none
// oSidebar:LineAt( nRow, nCol ) --> nLine
METHOD LineAt( nRow, nCol ) CLASS TSidebar

   LOCAL nLine
   LOCAL nTop := ::row + HEADER_ROWS

   IF nRow < nTop .OR. nCol < ::col .OR. nCol > ::Right()
      RETURN 0
   ENDIF

   nLine := ::first + Int( ( nRow - nTop ) / 3 )

   RETURN IIF( nLine <= Len( ::Lines() ) .AND. nLine < ::first + ::Visible(), nLine, 0 )

// Return how many lines fit between the separator and the last screen row
// oSidebar:Visible() --> nCount
METHOD Visible() CLASS TSidebar

   RETURN Max( 0, Int( ( MaxRow() - ::row - HEADER_ROWS + 1 ) / 3 ) )
