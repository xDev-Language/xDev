/*
 * Copyright (c) 2024-2026 xDev-Language
 *
 * Library...: xDev TUI
 * File......: tkeybar.prg
 *
 */

#include "tui.ch"
#include "inkey.ch"

/* ---------------------------------------------------------------------------------------------
   TKeyBar
   --------------------------------------------------------------------------------------------- */

CREATE CLASS TKeyBar

   // Captions assigned in order to F1, F2, ...
   DATA items             AS ARRAY

   // Colors of the captions
   DATA foreground        AS INT
   DATA background        AS INT

   // Colors of the function-key names
   DATA keyForeground     AS INT
   DATA keyBackground     AS INT

   // Colors of the caption under the mouse pointer
   DATA hoverForeground   AS INT
   DATA hoverBackground   AS INT

   // Number of the item under the mouse pointer, 0 when none
   DATA hover             AS INT

   METHOD New( aItems )
   METHOD Add( nKey, cCaption, nAlign, bAction )
   METHOD SetColor( nForeground, nBackground )
   METHOD SetKeyColor( nForeground, nBackground )
   METHOD SetHoverColor( nForeground, nBackground )
   METHOD Display()
   METHOD DisplayItem( nItem, lHover )
   METHOD HandleKey( nKey )
   METHOD ItemAt( nRow, nCol )

ENDCLASS

// Create a key bar whose captions are assigned in order to F1, F2, ...
// TKeyBar():New( aItems ) --> self
METHOD New( aItems ) CLASS TKeyBar

   LOCAL nItem

   ::items           := {}
   ::foreground      := COLOR_PAPER
   ::background      := COLOR_BLUE
   ::keyForeground   := COLOR_PAPER
   ::keyBackground   := COLOR_WINE
   ::hoverForeground := COLOR_BLUE_DARK
   ::hoverBackground := COLOR_BLUE_TINT1
   ::hover           := 0

   IF ValType( aItems ) == "A"
      FOR nItem := 1 TO Len( aItems )
         ::Add( TKeyBarFunctionKey( nItem ), aItems[ nItem ] )
      NEXT
   ENDIF

   RETURN Self

// Set the colors of the captions
// oKeyBar:SetColor( nForeground, nBackground ) --> self
METHOD SetColor( nForeground, nBackground ) CLASS TKeyBar

   ::foreground := nForeground
   ::background := nBackground

   RETURN Self

// Set the colors of the function-key names
// oKeyBar:SetKeyColor( nForeground, nBackground ) --> self
METHOD SetKeyColor( nForeground, nBackground ) CLASS TKeyBar

   ::keyForeground := nForeground
   ::keyBackground := nBackground

   RETURN Self

// Set the colors used for the caption under the mouse pointer
// oKeyBar:SetHoverColor( nForeground, nBackground ) --> self
METHOD SetHoverColor( nForeground, nBackground ) CLASS TKeyBar

   ::hoverForeground := nForeground
   ::hoverBackground := nBackground

   RETURN Self

// Add a caption, its keyboard shortcut, alignment and an optional action block
// oKeyBar:Add( nKey, cCaption, nAlign, bAction ) --> self
METHOD Add( nKey, cCaption, nAlign, bAction ) CLASS TKeyBar

   // Compatibility with Add( nKey, cCaption, bAction )
   IF ValType( nAlign ) == "B"
      bAction := nAlign
      nAlign  := TUI_LEFT
   ELSEIF ValType( nAlign ) != "N" .OR. nAlign < TUI_LEFT .OR. nAlign > TUI_RIGHT
      nAlign := TUI_LEFT
   ENDIF

   AAdd( ::items, { nKey, cCaption, nAlign, bAction } )

   RETURN Self

// Draw the key bar across the last screen row
// oKeyBar:Display() --> self
METHOD Display() CLASS TKeyBar

   LOCAL nCount := Len( ::items )
   LOCAL nWidth := MaxCol() + 1
   LOCAL nItem
   LOCAL nCursorRow := Row()
   LOCAL nCursorCol := Col()

   DispOutAtExt( MaxRow(), 0, Space( nWidth ), ::foreground, ::background )

   FOR nItem := 1 TO nCount
      ::DisplayItem( nItem, nItem == ::hover )
   NEXT

   SetPos( nCursorRow, nCursorCol )

   RETURN Self

// Draw one item without repainting the rest of the key bar
// oKeyBar:DisplayItem( nItem, lHover ) --> self
METHOD DisplayItem( nItem, lHover ) CLASS TKeyBar

   LOCAL nCount := Len( ::items )
   LOCAL nWidth := MaxCol() + 1
   LOCAL nBase
   LOCAL nExtra
   LOCAL nItemWidth
   LOCAL nCol
   LOCAL cKey
   LOCAL cCaption
   LOCAL cText
   LOCAL nCaptionWidth
   LOCAL nFore
   LOCAL nBack

   IF nItem < 1 .OR. nItem > nCount
      RETURN Self
   ENDIF

   nBase      := Int( nWidth / nCount )
   nExtra     := nWidth % nCount
   nItemWidth := nBase + IIF( nItem <= nExtra, 1, 0 )
   nCol       := ( nItem - 1 ) * nBase + Min( nItem - 1, nExtra )
   cKey       := "F" + LTrim( Str( nItem ) )

   IF nItemWidth > 0
      DispOutAtExt( MaxRow(), nCol, Left( cKey, nItemWidth ), ::keyForeground, ::keyBackground )
   ENDIF

   IF nItemWidth > Len( cKey )
      nCaptionWidth := nItemWidth - Len( cKey )
      cText         := AllTrim( ::items[ nItem ][ 2 ] )

      SWITCH ::items[ nItem ][ 3 ]
      CASE TUI_CENTER
         cCaption := UPadC( cText, nCaptionWidth )
         EXIT
      CASE TUI_RIGHT
         cCaption := UPadL( ULeft( cText, Max( nCaptionWidth - 1, 0 ) ) + " ", nCaptionWidth )
         EXIT
      OTHERWISE
         cCaption := UPadR( " " + cText, nCaptionWidth )
      ENDSWITCH

      nFore := IIF( lHover, ::hoverForeground, ::foreground )
      nBack := IIF( lHover, ::hoverBackground, ::background )
      DispOutAtExt( MaxRow(), nCol + Len( cKey ), ULeft( cCaption, nCaptionWidth ), nFore, nBack )
   ENDIF

   RETURN Self

// Handle function keys, mouse movement and a left click
// oKeyBar:HandleKey( nKey ) --> .T. when the event belongs to the key bar
METHOD HandleKey( nKey ) CLASS TKeyBar

   LOCAL nItem
   LOCAL nHover
   LOCAL nPrevious
   LOCAL nCursorRow
   LOCAL nCursorCol

   IF nKey == K_MOUSEMOVE
      nHover := ::ItemAt( MRow(), MCol() )
      IF nHover != ::hover
         nPrevious := ::hover
         ::hover := nHover
         nCursorRow := Row()
         nCursorCol := Col()
         DispBegin()
         ::DisplayItem( nPrevious, .F. )
         ::DisplayItem( nHover, .T. )
         SetPos( nCursorRow, nCursorCol )
         DispEnd()
      ENDIF
      RETURN nHover > 0
   ENDIF

   IF nKey == K_LBUTTONDOWN
      nItem := ::ItemAt( MRow(), MCol() )
   ELSE
      nItem := AScan( ::items, {| aItem | aItem[ 1 ] == nKey } )
   ENDIF

   IF nItem > 0
      IF ValType( ::items[ nItem ][ 4 ] ) == "B"
         Eval( ::items[ nItem ][ 4 ] )
      ENDIF
      RETURN .T.
   ENDIF

   RETURN .F.

// Return the item at a screen position, or 0 outside the bar
// oKeyBar:ItemAt( nRow, nCol ) --> item number
METHOD ItemAt( nRow, nCol ) CLASS TKeyBar

   LOCAL nCount := Len( ::items )
   LOCAL nWidth := MaxCol() + 1
   LOCAL nBase
   LOCAL nExtra
   LOCAL nLeft := 0
   LOCAL nItem
   LOCAL nItemWidth

   IF nRow != MaxRow() .OR. nCol < 0 .OR. nCol > MaxCol() .OR. nCount == 0
      RETURN 0
   ENDIF

   nBase  := Int( nWidth / nCount )
   nExtra := nWidth % nCount

   FOR nItem := 1 TO nCount
      nItemWidth := nBase + IIF( nItem <= nExtra, 1, 0 )
      IF nCol < nLeft + nItemWidth
         RETURN nItem
      ENDIF

      nLeft += nItemWidth
   NEXT

   RETURN 0

// The key code of function key nItem, 0 outside F1 - F10
STATIC FUNCTION TKeyBarFunctionKey( nItem )

   LOCAL aKeys := { K_F1, K_F2, K_F3, K_F4, K_F5, K_F6, K_F7, K_F8, K_F9, K_F10 }

   RETURN IIF( nItem >= 1 .AND. nItem <= Len( aKeys ), aKeys[ nItem ], 0 )
