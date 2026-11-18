/*
 * Copyright (c) 2024-2026 xDev-Language
 *
 * Library: xDev MyTiff
 * File: mtshell.prg
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "gfxdef.ch"
#include "gtinfo.ch"
#include "mytiff.ch"
#include "mtinternal.ch"

// Frame of the Motif window manager, in pixels
#define BORDER_WIDTH    6
#define CORNER_SIZE     23
#define TITLE_HEIGHT    17
#define MIN_WIDTH       ( 2 * CORNER_SIZE )
#define MIN_HEIGHT      ( 2 * CORNER_SIZE )
#define DECOR_ALL       ( MWM_DECOR_BORDER + MWM_DECOR_RESIZEH + MWM_DECOR_TITLE + MWM_DECOR_MENU + MWM_DECOR_MAXIMIZE )

/* ---------------------------------------------------------------------------------------------
   __MtShell: a window with the frame of MWM inside the GT window. It holds its children itself
   and places them by x / y from the client area; it moves with the title and resizes with the
   frame. docked: the frame fills the whole GT window and follows it. The title starts as the
   name; the window shows after Realize().
   --------------------------------------------------------------------------------------------- */

CREATE CLASS __MtShell FROM __MtWidget

   DATA app
   DATA title          AS STRING    INIT ""
   DATA mwmDecorations AS INT       INIT DECOR_ALL
   DATA deleteResponse AS INT       INIT MtDESTROY
   DATA docked         AS LOGIC     INIT .F.
   DATA mapped         AS LOGIC     INIT .F.
   DATA children       AS ARRAY
   DATA focus          AS INT       INIT 0

   METHOD New( oApp, cName )
   METHOD WidgetClass()
   METHOD AddChild( oWidget )
   METHOD RemoveChild( oWidget )
   METHOD Changed()
   METHOD ChildAt( nX, nY )
   METHOD ClientX()
   METHOD ClientY()
   METHOD ClientWidth()
   METHOD ClientHeight()
   METHOD Close()
   METHOD Destroy()
   METHOD Expose()
   METHOD FocusNext( nStep )
   METHOD FocusWidget()
   METHOD GetValue( nResource, aPair )
   METHOD HitRegion( nX, nY )
   METHOD Layout()
   METHOD Realize()
   METHOD SetBounds( nX, nY, nWidth, nHeight )
   METHOD SetValue( nResource, xValue )
   METHOD Shell()

HIDDEN:

   METHOD BorderWidth()
   METHOD TitleHeight()
   METHOD DrawFrame( aColors )
   METHOD DrawTitle( aColors )
   METHOD DrawButton( nLeft, lMenu, aColors )

ENDCLASS

// A window of the application, hidden until Realize()
METHOD New( oApp, cName ) CLASS __MtShell

   Super:New( NIL, cName )
   ::children := {}
   ::x        := 40
   ::y        := 40
   ::width    := 320
   ::height   := 200
   ::title    := ::name
   ::app      := oApp
   IF oApp != NIL
      oApp:AddShell( Self )
   ENDIF

   RETURN Self

// The name of the class in the messages of MyTiff
METHOD WidgetClass() CLASS __MtShell

   RETURN "Shell"

// A new child comes on top
METHOD AddChild( oWidget ) CLASS __MtShell

   AAdd( ::children, oWidget )

   RETURN Self

// A child that leaves takes the focus with it
METHOD RemoveChild( oWidget ) CLASS __MtShell

   LOCAL nPos := AScan( ::children, {| o | o == oWidget } )

   IF nPos > 0
      ADel( ::children, nPos, .T. )
      DO CASE
      CASE nPos == ::focus
         ::focus := 0
      CASE nPos < ::focus
         ::focus--
      ENDCASE
   ENDIF

   RETURN Self

// The size keeps the minimum of the frame
METHOD Changed() CLASS __MtShell

   ::SetBounds( ::x, ::y, ::width, ::height )

   RETURN Super:Changed()

// The child on top at the screen pixel nX, nY, NIL when there is none
METHOD ChildAt( nX, nY ) CLASS __MtShell

   LOCAL i

   FOR i := Len( ::children ) TO 1 STEP -1
      IF ::children[ i ]:HitTest( nX, nY )
         RETURN ::children[ i ]
      ENDIF
   NEXT

   RETURN NIL

// Screen position of the left edge of the client area
METHOD ClientX() CLASS __MtShell

   RETURN ::x + ::BorderWidth()

// Screen position of the top edge of the client area
METHOD ClientY() CLASS __MtShell

   RETURN ::y + ::BorderWidth() + ::TitleHeight()

// Width of the client area
METHOD ClientWidth() CLASS __MtShell

   RETURN ::width - 2 * ::BorderWidth()

// Height of the client area
METHOD ClientHeight() CLASS __MtShell

   RETURN ::height - 2 * ::BorderWidth() - ::TitleHeight()

// The menu button of the frame: what deleteResponse says
METHOD Close() CLASS __MtShell

   DO CASE
   CASE ::deleteResponse == MtDESTROY
      ::Destroy()
   CASE ::deleteResponse == MtUNMAP
      ::mapped := .F.
      ::Changed()
   ENDCASE

   RETURN Self

// The children first, then the window itself leaves the application
METHOD Destroy() CLASS __MtShell

   DO WHILE Len( ::children ) > 0
      ATail( ::children ):Destroy()
   ENDDO

   Super:Destroy()
   ::mapped := .F.
   IF ::app != NIL
      ::app:RemoveShell( Self )
   ENDIF

   RETURN NIL

// Draws the frame, the title bar and the children of a shown window
METHOD Expose() CLASS __MtShell

   LOCAL aColors := MtGetColors( ::background )
   LOCAL i

   IF ! ::mapped
      RETURN Self
   ENDIF

   ::Layout()
   GfxPrimitive( GFX_FILLEDRECT, ::y, ::x, ::y + ::height - 1, ::x + ::width - 1, ::background )
   ::DrawFrame( aColors )
   ::DrawTitle( aColors )
   FOR i := 1 TO Len( ::children )
      ::children[ i ]:Expose()
   NEXT

   RETURN Self

// Moves the focus to the next (1) or the previous (-1) child that takes it
METHOD FocusNext( nStep ) CLASS __MtShell

   LOCAL nCount := Len( ::children )
   LOCAL nPos   := ::focus
   LOCAL i

   FOR i := 1 TO nCount
      nPos += nStep
      IF nPos < 1
         nPos := nCount
      ELSEIF nPos > nCount
         nPos := 1
      ENDIF

      IF ::children[ nPos ]:traversalOn .AND. ::children[ nPos ]:sensitive
         IF ::focus > 0
            ::children[ ::focus ]:hasFocus := .F.
         ENDIF

         ::focus := nPos
         ::children[ nPos ]:hasFocus := .T.
         EXIT
      ENDIF
   NEXT

   RETURN Self

// The child with the focus, NIL for none
METHOD FocusWidget() CLASS __MtShell

   RETURN IIF( ::focus > 0, ::children[ ::focus ], NIL )

// The part of the frame or the client area at the screen pixel nX, nY
METHOD HitRegion( nX, nY ) CLASS __MtShell

   LOCAL nBorder := ::BorderWidth()
   LOCAL nRight  := ::x + ::width - 1
   LOCAL nBottom := ::y + ::height - 1
   LOCAL lWest, lEast, lNorth, lSouth, lFrame

   IF ! ::mapped .OR. nX < ::x .OR. nX > nRight .OR. nY < ::y .OR. nY > nBottom
      RETURN MtHIT_NONE
   ENDIF

   lFrame := nX < ::x + nBorder .OR. nX > nRight - nBorder .OR. nY < ::y + nBorder .OR. nY > nBottom - nBorder
   IF nBorder > 0 .AND. lFrame
      IF BitAnd( ::mwmDecorations, MWM_DECOR_RESIZEH ) == 0 .OR. ::docked
         RETURN MtHIT_NONE
      ENDIF

      // A corner reaches CORNER_SIZE along both edges, like the joins drawn in the frame
      lWest  := nX < ::x + CORNER_SIZE
      lEast  := nX > nRight - CORNER_SIZE
      lNorth := nY < ::y + CORNER_SIZE
      lSouth := nY > nBottom - CORNER_SIZE
      DO CASE
      CASE lNorth .AND. lWest
         RETURN MtHIT_RESIZE_NW
      CASE lNorth .AND. lEast
         RETURN MtHIT_RESIZE_NE
      CASE lSouth .AND. lEast
         RETURN MtHIT_RESIZE_SE
      CASE lSouth .AND. lWest
         RETURN MtHIT_RESIZE_SW
      CASE nY < ::y + nBorder
         RETURN MtHIT_RESIZE_N
      CASE nY > nBottom - nBorder
         RETURN MtHIT_RESIZE_S
      CASE nX < ::x + nBorder
         RETURN MtHIT_RESIZE_W
      ENDCASE
      RETURN MtHIT_RESIZE_E
   ENDIF

   IF ::TitleHeight() > 0 .AND. nY < ::ClientY()
      DO CASE
      CASE BitAnd( ::mwmDecorations, MWM_DECOR_MENU ) > 0 .AND. nX < ::x + nBorder + TITLE_HEIGHT
         RETURN MtHIT_MENU
      CASE BitAnd( ::mwmDecorations, MWM_DECOR_MAXIMIZE ) > 0 .AND. nX > nRight - nBorder - TITLE_HEIGHT
         RETURN MtHIT_MAXIMIZE
      ENDCASE
      RETURN MtHIT_TITLE
   ENDIF

   RETURN MtHIT_CLIENT

// A docked shell takes the size of the GT window
METHOD Layout() CLASS __MtShell

   IF ::docked
      ::x      := 0
      ::y      := 0
      ::width  := GtInfo( GTI_SCREENWIDTH )
      ::height := GtInfo( GTI_SCREENHEIGHT )
   ENDIF

   RETURN Self

// New position and size, at least the minimum of the frame
METHOD SetBounds( nX, nY, nWidth, nHeight ) CLASS __MtShell

   ::x      := nX
   ::y      := nY
   ::width  := Max( nWidth, MIN_WIDTH )
   ::height := Max( nHeight, MIN_HEIGHT + ::TitleHeight() )

   RETURN Self

// The resources of Shell, the others from Core
METHOD GetValue( nResource, aPair ) CLASS __MtShell

   DO CASE
   CASE nResource == MtNtitle
      aPair[ 2 ] := ::title
   CASE nResource == MtNmwmDecorations
      aPair[ 2 ] := ::mwmDecorations
   CASE nResource == MtNdeleteResponse
      aPair[ 2 ] := ::deleteResponse
   CASE nResource == MtNdocked
      aPair[ 2 ] := ::docked
   OTHERWISE
      RETURN Super:GetValue( nResource, aPair )
   ENDCASE

   RETURN .T.

// The window shows with its children. Behaviour of xDev: a window hidden by MtUNMAP comes back
// too; in Motif XtRealizeWidget() does nothing on a realized shell, it is mapped again by
// XtPopup() or XtMapWidget()
METHOD Realize() CLASS __MtShell

   ::mapped := .T.
   ::Changed()

   RETURN Self

// The resources of Shell, the others from Core
METHOD SetValue( nResource, xValue ) CLASS __MtShell

   DO CASE
   CASE nResource == MtNtitle
      ::title := xValue
   CASE nResource == MtNmwmDecorations
      ::mwmDecorations := xValue
   CASE nResource == MtNdeleteResponse
      ::deleteResponse := xValue
   CASE nResource == MtNdocked
      ::docked := xValue
   OTHERWISE
      RETURN Super:SetValue( nResource, xValue )
   ENDCASE

   RETURN .T.

// A shell is its own shell
METHOD Shell() CLASS __MtShell

   RETURN Self

/* ---------------------------------------------------------------------------------------------
   Hidden
   --------------------------------------------------------------------------------------------- */

// Width of the frame, 0 without a border
METHOD BorderWidth() CLASS __MtShell

   RETURN IIF( BitAnd( ::mwmDecorations, MWM_DECOR_BORDER + MWM_DECOR_RESIZEH ) > 0, BORDER_WIDTH, 0 )

// Height of the title bar, 0 without a title
METHOD TitleHeight() CLASS __MtShell

   RETURN IIF( BitAnd( ::mwmDecorations, MWM_DECOR_TITLE ) > 0, TITLE_HEIGHT, 0 )

// Raised frame, a sunken line inside it, and with a resize frame the joins of the corners
METHOD DrawFrame( aColors ) CLASS __MtShell

   LOCAL nTop    := ::y
   LOCAL nLeft   := ::x
   LOCAL nBottom := ::y + ::height - 1
   LOCAL nRight  := ::x + ::width - 1
   LOCAL nIn     := BORDER_WIDTH - 1
   LOCAL nLight  := aColors[ MtCOLOR_TOPSHADOW ]
   LOCAL nDark   := aColors[ MtCOLOR_BOTTOMSHADOW ]

   IF ::BorderWidth() == 0
      RETURN Self
   ENDIF

   __MtDrawShadow( nTop, nLeft, nBottom, nRight, 2, nLight, nDark )
   __MtDrawShadow( nTop + nIn, nLeft + nIn, nBottom - nIn, nRight - nIn, 1, nDark, nLight )

   IF BitAnd( ::mwmDecorations, MWM_DECOR_RESIZEH ) > 0
      // A dark and a light line across the border, CORNER_SIZE from each corner
      IF ::width >= 2 * CORNER_SIZE
         s_JoinAcross( nLeft + CORNER_SIZE - 1, nTop + 1, nTop + nIn, nDark, nLight )
         s_JoinAcross( nRight - CORNER_SIZE - 1, nTop + 1, nTop + nIn, nDark, nLight )
         s_JoinAcross( nLeft + CORNER_SIZE - 1, nBottom - nIn + 1, nBottom - 1, nDark, nLight )
         s_JoinAcross( nRight - CORNER_SIZE - 1, nBottom - nIn + 1, nBottom - 1, nDark, nLight )
      ENDIF

      IF ::height >= 2 * CORNER_SIZE
         s_JoinAlong( nTop + CORNER_SIZE - 1, nLeft + 1, nLeft + nIn, nDark, nLight )
         s_JoinAlong( nTop + CORNER_SIZE - 1, nRight - nIn + 1, nRight - 1, nDark, nLight )
         s_JoinAlong( nBottom - CORNER_SIZE - 1, nLeft + 1, nLeft + nIn, nDark, nLight )
         s_JoinAlong( nBottom - CORNER_SIZE - 1, nRight - nIn + 1, nRight - 1, nDark, nLight )
      ENDIF
   ENDIF

   RETURN Self

// Title bar: a raised bar with the title in the middle, the menu button left, maximize right
METHOD DrawTitle( aColors ) CLASS __MtShell

   LOCAL nTop   := ::y + ::BorderWidth()
   LOCAL nLeft  := ::x + ::BorderWidth()
   LOCAL nRight := ::x + ::width - 1 - ::BorderWidth()
   LOCAL cTitle := ::title
   LOCAL nLight := aColors[ MtCOLOR_TOPSHADOW ]
   LOCAL nDark  := aColors[ MtCOLOR_BOTTOMSHADOW ]
   LOCAL nRoom, nTextTop, nTextLeft

   IF ::TitleHeight() == 0
      RETURN Self
   ENDIF

   IF BitAnd( ::mwmDecorations, MWM_DECOR_MENU ) > 0
      ::DrawButton( nLeft, .T., aColors )
      nLeft += TITLE_HEIGHT
   ENDIF

   IF BitAnd( ::mwmDecorations, MWM_DECOR_MAXIMIZE ) > 0
      ::DrawButton( nRight - TITLE_HEIGHT + 1, .F., aColors )
      nRight -= TITLE_HEIGHT
   ENDIF

   __MtDrawShadow( nTop, nLeft, nTop + TITLE_HEIGHT - 1, nRight, 1, nLight, nDark )

   // A long title is cut to the bar
   nRoom := Int( ( nRight - nLeft - 3 ) / __MtFontWidth() )
   IF ULen( cTitle ) > nRoom
      cTitle := ULeft( cTitle, Max( nRoom, 0 ) )
   ENDIF

   nTextTop := nTop + Int( ( TITLE_HEIGHT - __MtFontHeight() ) / 2 )
   nTextLeft := nLeft + Int( ( nRight - nLeft + 1 - __MtTextWidth( cTitle ) ) / 2 )
   __MtDrawText( nTextTop, nTextLeft, cTitle, aColors[ MtCOLOR_FOREGROUND ] )

   RETURN Self

// A raised square of the title bar; the menu button shows a bar, maximize a small square
METHOD DrawButton( nLeft, lMenu, aColors ) CLASS __MtShell

   LOCAL nTop   := ::y + ::BorderWidth()
   LOCAL nSize  := TITLE_HEIGHT
   LOCAL nMid   := nTop + Int( nSize / 2 )
   LOCAL nLight := aColors[ MtCOLOR_TOPSHADOW ]
   LOCAL nDark  := aColors[ MtCOLOR_BOTTOMSHADOW ]

   __MtDrawShadow( nTop, nLeft, nTop + nSize - 1, nLeft + nSize - 1, 1, nLight, nDark )
   IF lMenu
      __MtDrawShadow( nMid - 1, nLeft + 4, nMid + 1, nLeft + nSize - 5, 1, nLight, nDark )
   ELSE
      __MtDrawShadow( nTop + 4, nLeft + 4, nTop + nSize - 5, nLeft + nSize - 5, 1, nLight, nDark )
   ENDIF

   RETURN Self

/* ---------------------------------------------------------------------------------------------
   Functions of this file
   --------------------------------------------------------------------------------------------- */

// Join of a corner across the top or the bottom border: a dark and a light vertical line at nX
STATIC FUNCTION s_JoinAcross( nX, nTop, nBottom, nDark, nLight )

   GfxPrimitive( GFX_LINE, nTop, nX, nBottom, nX, nDark )
   GfxPrimitive( GFX_LINE, nTop, nX + 1, nBottom, nX + 1, nLight )

   RETURN NIL

// Join of a corner across the left or the right border: a dark and a light horizontal line at nY
STATIC FUNCTION s_JoinAlong( nY, nLeft, nRight, nDark, nLight )

   GfxPrimitive( GFX_LINE, nY, nLeft, nY, nRight, nDark )
   GfxPrimitive( GFX_LINE, nY + 1, nLeft, nY + 1, nRight, nLight )

   RETURN NIL
