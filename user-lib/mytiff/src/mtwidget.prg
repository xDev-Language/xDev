/*
 * Copyright (c) 2024-2026 xDev-Language
 *
 * Library...: xDev MyTiff
 * File......: mtwidget.prg
 *
 */

#include "gfxdef.ch"
#include "mytiff.ch"
#include "mtinternal.ch"

/* ---------------------------------------------------------------------------------------------
   __MtWidget: what every widget has, like Core in Motif. x and y count from the client area of
   the parent shell, in pixels. Inside MyTiff; programs use the Mt functions.
   --------------------------------------------------------------------------------------------- */

CREATE CLASS __MtWidget

   DATA name       AS STRING    INIT ""
   DATA parent
   DATA x          AS INT       INIT 0
   DATA y          AS INT       INIT 0
   DATA width      AS INT       INIT 0
   DATA height     AS INT       INIT 0
   DATA background AS INT       INIT 0xC0C0C0
   DATA sensitive  AS LOGIC     INIT .T.
   // { { nCallback, { bBlock, ... } }, ... }
   DATA callbacks  AS ARRAY

   METHOD New( oParent, cName )
   METHOD WidgetClass()
   METHOD AbsX()
   METHOD AbsY()
   METHOD AddCallback( nCallback, bBlock )
   METHOD CallCallbacks( nCallback, aCallData )
   METHOD Changed()
   METHOD Destroy()
   METHOD Expose()
   METHOD GetValue( nResource, aPair )
   METHOD HasCallback( nCallback )
   METHOD HitTest( nX, nY )
   METHOD Realize()
   METHOD SetValue( nResource, xValue )
   METHOD Shell()

HIDDEN:

   METHOD CallbackList( nCallback )

ENDCLASS

// A widget in its parent, which takes it as a child
METHOD New( oParent, cName ) CLASS __MtWidget

   ::callbacks := {}
   ::parent    := oParent
   IF cName != NIL
      ::name := cName
   ENDIF

   IF oParent != NIL
      oParent:AddChild( Self )
   ENDIF

   RETURN Self

// The name of the class in the messages of MyTiff
METHOD WidgetClass() CLASS __MtWidget

   RETURN "Core"

// Screen position of the left edge
METHOD AbsX() CLASS __MtWidget

   RETURN IIF( ::parent == NIL, ::x, ::parent:ClientX() + ::x )

// Screen position of the top edge
METHOD AbsY() CLASS __MtWidget

   RETURN IIF( ::parent == NIL, ::y, ::parent:ClientY() + ::y )

// Add a block to the list of a callback
METHOD AddCallback( nCallback, bBlock ) CLASS __MtWidget

   AAdd( ::CallbackList( nCallback ), bBlock )

   RETURN Self

// Every block of the callback in the order they were added, like XtCallCallbacks()
METHOD CallCallbacks( nCallback, aCallData ) CLASS __MtWidget

   LOCAL aBlocks := AClone( ::CallbackList( nCallback ) )
   LOCAL i

   FOR i := 1 TO Len( aBlocks )
      Eval( aBlocks[ i ], Self, aCallData )
   NEXT

   RETURN Self

// After a change of a resource: the subclasses fix their size, the screen is drawn again
METHOD Changed() CLASS __MtWidget

   LOCAL oShell := ::Shell()

   IF oShell != NIL .AND. oShell:app != NIL
      oShell:app:dirty := .T.
   ENDIF

   RETURN Self

// MtNdestroyCallback gets NIL for the call data, like NULL in Motif
METHOD Destroy() CLASS __MtWidget

   ::Changed()
   ::CallCallbacks( MtNdestroyCallback, NIL )
   IF ::parent != NIL
      ::parent:RemoveChild( Self )
      ::parent := NIL
   ENDIF

   RETURN NIL

// Draws the widget; the subclasses draw their own look
METHOD Expose() CLASS __MtWidget

   GfxPrimitive( GFX_FILLEDRECT, ::AbsY(), ::AbsX(), ::AbsY() + ::height - 1, ::AbsX() + ::width - 1, ::background )

   RETURN Self

// Fills aPair[ 2 ] with the resource aPair[ 1 ]; .F. when the class has no such resource
METHOD GetValue( nResource, aPair ) CLASS __MtWidget

   DO CASE
   CASE nResource == MtNx
      aPair[ 2 ] := ::x
   CASE nResource == MtNy
      aPair[ 2 ] := ::y
   CASE nResource == MtNwidth
      aPair[ 2 ] := ::width
   CASE nResource == MtNheight
      aPair[ 2 ] := ::height
   CASE nResource == MtNbackground
      aPair[ 2 ] := ::background
   CASE nResource == MtNsensitive
      aPair[ 2 ] := ::sensitive
   CASE ::HasCallback( nResource )
      aPair[ 2 ] := AClone( ::CallbackList( nResource ) )
   OTHERWISE
      RETURN .F.
   ENDCASE

   RETURN .T.

// .T. when the class has the callback
METHOD HasCallback( nCallback ) CLASS __MtWidget

   RETURN nCallback == MtNdestroyCallback

// .T. when the screen pixel nX, nY is on the widget
METHOD HitTest( nX, nY ) CLASS __MtWidget

   RETURN nX >= ::AbsX() .AND. nX < ::AbsX() + ::width .AND. nY >= ::AbsY() .AND. nY < ::AbsY() + ::height

// A child is realized with its shell
METHOD Realize() CLASS __MtWidget

   RETURN Self

// .F. when the class has no such resource; a callback set here replaces its list, like XtSetValues()
METHOD SetValue( nResource, xValue ) CLASS __MtWidget

   DO CASE
   CASE nResource == MtNx
      ::x := xValue
   CASE nResource == MtNy
      ::y := xValue
   CASE nResource == MtNwidth
      ::width := xValue
   CASE nResource == MtNheight
      ::height := xValue
   CASE nResource == MtNbackground
      ::background := xValue
   CASE nResource == MtNsensitive
      ::sensitive := xValue
   CASE ::HasCallback( nResource )
      ASize( ::CallbackList( nResource ), 0 )
      IF ValType( xValue ) == "B"
         ::AddCallback( nResource, xValue )
      ENDIF
   OTHERWISE
      RETURN .F.
   ENDCASE

   RETURN .T.

// The shell the widget lies in; without managers that is the parent
METHOD Shell() CLASS __MtWidget

   RETURN ::parent

// The blocks of one callback, an empty list the first time
METHOD CallbackList( nCallback ) CLASS __MtWidget

   LOCAL nPos := AScan( ::callbacks, {| a | a[ 1 ] == nCallback } )

   IF nPos == 0
      AAdd( ::callbacks, { nCallback, {} } )
      nPos := Len( ::callbacks )
   ENDIF

   RETURN ::callbacks[ nPos ][ 2 ]

/* ---------------------------------------------------------------------------------------------
   __MtPrimitive: a widget with a shadow and a focus highlight, like XmPrimitive. Colors left
   at -1 come from MtGetColors( background ).
   --------------------------------------------------------------------------------------------- */

CREATE CLASS __MtPrimitive FROM __MtWidget

   DATA foreground         AS INT    INIT -1
   DATA shadowThickness    AS INT    INIT 2
   DATA highlightThickness AS INT    INIT 2
   DATA topShadowColor     AS INT    INIT -1
   DATA bottomShadowColor  AS INT    INIT -1
   DATA highlightColor     AS INT    INIT 0x000000
   DATA traversalOn        AS LOGIC  INIT .T.
   DATA hasFocus           AS LOGIC  INIT .F.

   METHOD WidgetClass()
   METHOD Colors()
   METHOD DrawHighlight()
   METHOD DrawShadow( lSunken )
   METHOD GetValue( nResource, aPair )
   METHOD SetValue( nResource, xValue )

   // Actions of a press; a widget that cannot be pressed does nothing, Arm() is .T. when it went down
   METHOD Activate( nKey )
   METHOD Arm( nKey )
   METHOD ArmAndActivate( nKey )
   METHOD Disarm( nKey )

ENDCLASS

// The name of the class in the messages of MyTiff
METHOD WidgetClass() CLASS __MtPrimitive

   RETURN "Primitive"

// The action of a press; nothing for a widget that cannot be pressed
METHOD Activate( nKey ) CLASS __MtPrimitive

   RETURN Self

// .T. when the widget went down; never for a widget that cannot be pressed
METHOD Arm( nKey ) CLASS __MtPrimitive

   RETURN .F.

// A press with a key: down, the action and up again; .F. for a widget that cannot be pressed
METHOD ArmAndActivate( nKey ) CLASS __MtPrimitive

   RETURN .F.

// The widget comes up again
METHOD Disarm( nKey ) CLASS __MtPrimitive

   RETURN Self

// { nForeground, nTopShadow, nBottomShadow, nSelect }: the set colors, the rest from the background
METHOD Colors() CLASS __MtPrimitive

   LOCAL aColors := MtGetColors( ::background )

   IF ::foreground >= 0
      aColors[ MtCOLOR_FOREGROUND ] := ::foreground
   ENDIF

   IF ::topShadowColor >= 0
      aColors[ MtCOLOR_TOPSHADOW ] := ::topShadowColor
   ENDIF

   IF ::bottomShadowColor >= 0
      aColors[ MtCOLOR_BOTTOMSHADOW ] := ::bottomShadowColor
   ENDIF

   RETURN aColors

// The highlight around the widget: highlightColor with the focus, the parent background without it
METHOD DrawHighlight() CLASS __MtPrimitive

   LOCAL nColor := ::highlightColor
   LOCAL nTop   := ::AbsY()
   LOCAL nLeft  := ::AbsX()

   IF ! ::hasFocus
      nColor := IIF( ::parent == NIL, ::background, ::parent:background )
   ENDIF

   IF ::highlightThickness > 0
      __MtDrawShadow( nTop, nLeft, nTop + ::height - 1, nLeft + ::width - 1, ::highlightThickness, nColor, nColor )
   ENDIF

   RETURN Self

// The shadow inside the highlight, raised or sunken
METHOD DrawShadow( lSunken ) CLASS __MtPrimitive

   LOCAL aColors := ::Colors()
   LOCAL nInset  := ::highlightThickness
   LOCAL nTop    := ::AbsY() + nInset
   LOCAL nLeft   := ::AbsX() + nInset
   LOCAL nBottom := ::AbsY() + ::height - 1 - nInset
   LOCAL nRight  := ::AbsX() + ::width - 1 - nInset
   LOCAL nLight  := aColors[ IIF( lSunken, MtCOLOR_BOTTOMSHADOW, MtCOLOR_TOPSHADOW ) ]
   LOCAL nDark   := aColors[ IIF( lSunken, MtCOLOR_TOPSHADOW, MtCOLOR_BOTTOMSHADOW ) ]

   IF ::shadowThickness > 0
      __MtDrawShadow( nTop, nLeft, nBottom, nRight, ::shadowThickness, nLight, nDark )
   ENDIF

   RETURN Self

// The resources of Primitive, the others from Core
METHOD GetValue( nResource, aPair ) CLASS __MtPrimitive

   DO CASE
   CASE nResource == MtNforeground
      aPair[ 2 ] := ::foreground
   CASE nResource == MtNshadowThickness
      aPair[ 2 ] := ::shadowThickness
   CASE nResource == MtNhighlightThickness
      aPair[ 2 ] := ::highlightThickness
   CASE nResource == MtNtopShadowColor
      aPair[ 2 ] := ::topShadowColor
   CASE nResource == MtNbottomShadowColor
      aPair[ 2 ] := ::bottomShadowColor
   CASE nResource == MtNhighlightColor
      aPair[ 2 ] := ::highlightColor
   CASE nResource == MtNtraversalOn
      aPair[ 2 ] := ::traversalOn
   OTHERWISE
      RETURN Super:GetValue( nResource, aPair )
   ENDCASE

   RETURN .T.

// The resources of Primitive, the others from Core
METHOD SetValue( nResource, xValue ) CLASS __MtPrimitive

   DO CASE
   CASE nResource == MtNforeground
      ::foreground := xValue
   CASE nResource == MtNshadowThickness
      ::shadowThickness := xValue
   CASE nResource == MtNhighlightThickness
      ::highlightThickness := xValue
   CASE nResource == MtNtopShadowColor
      ::topShadowColor := xValue
   CASE nResource == MtNbottomShadowColor
      ::bottomShadowColor := xValue
   CASE nResource == MtNhighlightColor
      ::highlightColor := xValue
   CASE nResource == MtNtraversalOn
      ::traversalOn := xValue
   OTHERWISE
      RETURN Super:SetValue( nResource, xValue )
   ENDCASE

   RETURN .T.
