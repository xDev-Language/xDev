/*
 * Copyright (c) 2024-2026 xDev-Language
 *
 * Library: xDev MyTiff
 * File: mtapp.prg
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "inkey.ch"
#include "gfxdef.ch"
#include "gtinfo.ch"
#include "mytiff.ch"
#include "mtinternal.ch"

/* ---------------------------------------------------------------------------------------------
   The application and the event loop of MyTiff. MtAppMainLoop() works like XtAppMainLoop(),
   MtAppProcessEvent() like XtAppProcessEvent(): one event, so a program or a test can drive MyTiff
   from its own loop.
   --------------------------------------------------------------------------------------------- */

// A new application
// MtAppInitialize( cName ) --> app
FUNCTION MtAppInitialize( cName )

   RETURN __MtApp():New( cName )

// Draws the screen and handles events until MtAppQuit() or until no window is shown
FUNCTION MtAppMainLoop( app )

   DO WHILE app:ProcessEvent( 0 )
   ENDDO

   RETURN NIL

// Waits nSeconds (0 or NIL: until an event) for one event and handles it; .F. when the
// application ends
FUNCTION MtAppProcessEvent( app, nSeconds )

   RETURN app:ProcessEvent( IIF( nSeconds == NIL, 0, nSeconds ) )

// The loop ends after the current event
FUNCTION MtAppQuit( app )

   app:quitting := .T.

   RETURN NIL

/* ---------------------------------------------------------------------------------------------
   __MtApp: the windows of the application, inside MyTiff
   --------------------------------------------------------------------------------------------- */

CREATE CLASS __MtApp

   DATA name        AS STRING  INIT ""
   // Windows from the bottom to the top; the last one shown is active
   DATA shells      AS ARRAY
   DATA background  AS INT     INIT 0x5F7F9F
   DATA quitting    AS LOGIC   INIT .F.
   // .T. when the screen must be drawn again before the next event
   DATA dirty       AS LOGIC   INIT .T.

   METHOD New( cName )
   METHOD AddShell( oShell )
   METHOD RemoveShell( oShell )
   METHOD ProcessEvent( nSeconds )
   METHOD Redraw()
   METHOD Running()

HIDDEN:

   // The shell moved or resized with the mouse, the part of the frame, where it started
   DATA dragShell
   DATA dragHit     AS INT     INIT MtHIT_NONE
   DATA dragX       AS INT     INIT 0
   DATA dragY       AS INT     INIT 0
   DATA dragBounds  AS ARRAY
   // The button pressed with the mouse
   DATA pressed

   METHOD Active()
   METHOD MouseDown( nX, nY, nKey )
   METHOD MouseMove( nX, nY )
   METHOD MouseUp( nX, nY, nKey )
   METHOD ShellAt( nX, nY )

ENDCLASS

// An application without windows
METHOD New( cName ) CLASS __MtApp

   ::shells := {}
   IF cName != NIL
      ::name := cName
   ENDIF

   RETURN Self

// A new window comes on top
METHOD AddShell( oShell ) CLASS __MtApp

   AAdd( ::shells, oShell )

   RETURN Self

// A window leaves the application
METHOD RemoveShell( oShell ) CLASS __MtApp

   LOCAL nPos := AScan( ::shells, {| o | o == oShell } )

   IF nPos > 0
      ADel( ::shells, nPos, .T. )
      ::dirty := .T.
   ENDIF

   IF ::dragShell == oShell
      ::dragShell := NIL
   ENDIF

   RETURN Self

// Draws the screen when needed, then waits for one event and handles it; .F. when the application ends
METHOD ProcessEvent( nSeconds ) CLASS __MtApp

   LOCAL nKey
   LOCAL aPos
   LOCAL oShell
   LOCAL oFocus

   IF ! ::Running()
      RETURN .F.
   ENDIF

   IF ::dirty
      ::Redraw()
   ENDIF

   nKey := Inkey( nSeconds, INKEY_ALLEVENTS )
   IF nKey == 0
      RETURN ::Running()
   ENDIF

   aPos := GtInfo( GTI_MOUSEPOS_XY )

   DO CASE
   CASE nKey == K_CLOSE
      ::quitting := .T.
   CASE nKey == K_RESIZE
      ::dirty := .T.
   CASE nKey == K_LBUTTONDOWN
      ::MouseDown( aPos[ 1 ], aPos[ 2 ], nKey )
   CASE nKey == K_MOUSEMOVE
      ::MouseMove( aPos[ 1 ], aPos[ 2 ] )
   CASE nKey == K_LBUTTONUP
      ::MouseUp( aPos[ 1 ], aPos[ 2 ], nKey )
   CASE nKey == K_TAB .OR. nKey == K_SH_TAB
      oShell := ::Active()
      IF oShell != NIL
         oShell:FocusNext( IIF( nKey == K_TAB, 1, -1 ) )
         ::dirty := .T.
      ENDIF
   CASE nKey == K_SPACE .OR. nKey == K_ENTER
      // The focused button is pressed like with the mouse
      oShell := ::Active()
      oFocus := IIF( oShell == NIL, NIL, oShell:FocusWidget() )
      IF oFocus != NIL .AND. oFocus:ArmAndActivate( nKey )
         ::dirty := .T.
      ENDIF
   ENDCASE

   RETURN ::Running()

// The background of the GT window and every window from the bottom up, in one frame
METHOD Redraw() CLASS __MtApp

   LOCAL i

   GfxPrimitive( GFX_ACQUIRESCREEN )
   GfxPrimitive( GFX_FILLEDRECT, 0, 0, GtInfo( GTI_SCREENHEIGHT ) - 1, GtInfo( GTI_SCREENWIDTH ) - 1, ::background )
   FOR i := 1 TO Len( ::shells )
      ::shells[ i ]:Expose()
   NEXT

   GfxPrimitive( GFX_RELEASESCREEN )
   ::dirty := .F.

   RETURN Self

// .T. until MtAppQuit() and while a window is shown
METHOD Running() CLASS __MtApp

   RETURN ! ::quitting .AND. AScan( ::shells, {| o | o:mapped } ) > 0

/* ---------------------------------------------------------------------------------------------
   Hidden
   --------------------------------------------------------------------------------------------- */

// The window on top that is shown, NIL for none
METHOD Active() CLASS __MtApp

   LOCAL i

   FOR i := Len( ::shells ) TO 1 STEP -1
      IF ::shells[ i ]:mapped
         RETURN ::shells[ i ]
      ENDIF
   NEXT

   RETURN NIL

// A click brings the window to the top; the frame starts a move or a resize, a button is armed
METHOD MouseDown( nX, nY, nKey ) CLASS __MtApp

   LOCAL oShell := ::ShellAt( nX, nY )
   LOCAL oChild
   LOCAL nHit

   IF oShell == NIL
      RETURN Self
   ENDIF

   ADel( ::shells, AScan( ::shells, {| o | o == oShell } ), .T. )
   AAdd( ::shells, oShell )

   nHit := oShell:HitRegion( nX, nY )
   DO CASE
   CASE nHit == MtHIT_MENU
      oShell:Close()
   CASE nHit == MtHIT_MAXIMIZE
      oShell:docked := ! oShell:docked
   CASE nHit == MtHIT_CLIENT
      oChild := oShell:ChildAt( nX, nY )
      IF oChild != NIL .AND. oChild:Arm( nKey )
         ::pressed := oChild
      ENDIF
   CASE nHit != MtHIT_NONE .AND. ! oShell:docked
      ::dragShell  := oShell
      ::dragHit    := nHit
      ::dragX      := nX
      ::dragY      := nY
      ::dragBounds := { oShell:x, oShell:y, oShell:width, oShell:height }
   ENDCASE

   ::dirty := .T.

   RETURN Self

// Moving: the window follows the mouse; a pressed button is sunken only while the mouse is on it
METHOD MouseMove( nX, nY ) CLASS __MtApp

   LOCAL nDX := nX - ::dragX
   LOCAL nDY := nY - ::dragY
   LOCAL aB  := ::dragBounds
   LOCAL nLeft, nTop, nRight, nBottom

   IF ::dragShell != NIL
      nLeft   := aB[ 1 ]
      nTop    := aB[ 2 ]
      nRight  := aB[ 1 ] + aB[ 3 ]
      nBottom := aB[ 2 ] + aB[ 4 ]
      IF ::dragHit == MtHIT_TITLE
         nLeft   += nDX
         nRight  += nDX
         nTop    += nDY
         nBottom += nDY
      ELSE
         IF ::dragHit == MtHIT_RESIZE_NW .OR. ::dragHit == MtHIT_RESIZE_W .OR. ::dragHit == MtHIT_RESIZE_SW
            nLeft += nDX
         ENDIF

         IF ::dragHit == MtHIT_RESIZE_NE .OR. ::dragHit == MtHIT_RESIZE_E .OR. ::dragHit == MtHIT_RESIZE_SE
            nRight += nDX
         ENDIF

         IF ::dragHit == MtHIT_RESIZE_NW .OR. ::dragHit == MtHIT_RESIZE_N .OR. ::dragHit == MtHIT_RESIZE_NE
            nTop += nDY
         ENDIF

         IF ::dragHit == MtHIT_RESIZE_SW .OR. ::dragHit == MtHIT_RESIZE_S .OR. ::dragHit == MtHIT_RESIZE_SE
            nBottom += nDY
         ENDIF
      ENDIF

      ::dragShell:SetBounds( nLeft, nTop, nRight - nLeft, nBottom - nTop )
      ::dirty := .T.
   ELSEIF ::pressed != NIL .AND. ::pressed:armed != ::pressed:HitTest( nX, nY )
      ::pressed:armed := ! ::pressed:armed
      ::dirty := .T.
   ENDIF

   RETURN Self

// Release: the move ends; a button released under the mouse is activated
METHOD MouseUp( nX, nY, nKey ) CLASS __MtApp

   IF ::dragShell != NIL
      ::dragShell := NIL
      ::dragHit   := MtHIT_NONE
   ELSEIF ::pressed != NIL
      IF ::pressed:HitTest( nX, nY )
         ::pressed:Activate( nKey )
      ENDIF

      ::pressed:Disarm( nKey )
      ::pressed := NIL
      ::dirty   := .T.
   ENDIF

   RETURN Self

// The window on top at the screen pixel nX, nY
METHOD ShellAt( nX, nY ) CLASS __MtApp

   LOCAL oShell
   LOCAL i

   FOR i := Len( ::shells ) TO 1 STEP -1
      oShell := ::shells[ i ]
      IF oShell:HitRegion( nX, nY ) != MtHIT_NONE .OR. ( oShell:mapped .AND. oShell:HitTest( nX, nY ) )
         RETURN oShell
      ENDIF
   NEXT

   RETURN NIL
