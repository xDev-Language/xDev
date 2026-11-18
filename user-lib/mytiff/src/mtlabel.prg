/*
 * Copyright (c) 2024-2026 xDev-Language
 *
 * Library: xDev MyTiff
 * File: mtlabel.prg
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "gfxdef.ch"
#include "mytiff.ch"
#include "mtinternal.ch"

/* ---------------------------------------------------------------------------------------------
   __MtLabel: a text, like XmLabel. It takes no focus and has no shadow. With recomputeSize the
   size follows labelString and the margins; labelString starts as the name of the widget.
   --------------------------------------------------------------------------------------------- */

CREATE CLASS __MtLabel FROM __MtPrimitive

   DATA labelString   AS STRING    INIT ""
   DATA alignment     AS INT       INIT MtALIGNMENT_CENTER
   DATA marginWidth   AS INT       INIT 2
   DATA marginHeight  AS INT       INIT 2
   DATA recomputeSize AS LOGIC     INIT .T.

   METHOD New( oParent, cName )
   METHOD WidgetClass()
   METHOD Changed()
   METHOD Expose()
   METHOD DrawLabel( nColor )
   METHOD GetValue( nResource, aPair )
   METHOD Resize()
   METHOD SetValue( nResource, xValue )

ENDCLASS

// A label without a shadow and without the focus, as big as its name
METHOD New( oParent, cName ) CLASS __MtLabel

   Super:New( oParent, cName )
   ::labelString     := ::name
   ::shadowThickness := 0
   ::traversalOn     := .F.
   ::Resize()

   RETURN Self

// The name of the class in the messages of MyTiff
METHOD WidgetClass() CLASS __MtLabel

   RETURN "Label"

// The size follows a change of a resource
METHOD Changed() CLASS __MtLabel

   ::Resize()

   RETURN Super:Changed()

// Draws the background, the highlight and the text
METHOD Expose() CLASS __MtLabel

   ::Resize()
   GfxPrimitive( GFX_FILLEDRECT, ::AbsY(), ::AbsX(), ::AbsY() + ::height - 1, ::AbsX() + ::width - 1, ::background )
   ::DrawShadow( .F. )
   ::DrawHighlight()
   ::DrawLabel( ::Colors()[ MtCOLOR_FOREGROUND ] )

   RETURN Self

// labelString inside the margins, by alignment, in the middle of the height
METHOD DrawLabel( nColor ) CLASS __MtLabel

   LOCAL nInset := ::highlightThickness + ::shadowThickness + ::marginWidth
   LOCAL nFree  := ::width - 2 * nInset - __MtTextWidth( ::labelString )
   LOCAL nLeft  := ::AbsX() + nInset

   DO CASE
   CASE ::alignment == MtALIGNMENT_CENTER
      nLeft += Int( Max( nFree, 0 ) / 2 )
   CASE ::alignment == MtALIGNMENT_END
      nLeft += Max( nFree, 0 )
   ENDCASE

   __MtDrawText( ::AbsY() + Int( ( ::height - __MtFontHeight() ) / 2 ), nLeft, ::labelString, nColor )

   RETURN Self

// The resources of Label, the others from Primitive
METHOD GetValue( nResource, aPair ) CLASS __MtLabel

   DO CASE
   CASE nResource == MtNlabelString
      aPair[ 2 ] := ::labelString
   CASE nResource == MtNalignment
      aPair[ 2 ] := ::alignment
   CASE nResource == MtNmarginWidth
      aPair[ 2 ] := ::marginWidth
   CASE nResource == MtNmarginHeight
      aPair[ 2 ] := ::marginHeight
   CASE nResource == MtNrecomputeSize
      aPair[ 2 ] := ::recomputeSize
   OTHERWISE
      RETURN Super:GetValue( nResource, aPair )
   ENDCASE

   RETURN .T.

// With recomputeSize: just big enough for the text, the margins, the shadow and the highlight
METHOD Resize() CLASS __MtLabel

   LOCAL nInset := ::highlightThickness + ::shadowThickness

   IF ::recomputeSize
      ::width  := 2 * ( nInset + ::marginWidth ) + __MtTextWidth( ::labelString )
      ::height := 2 * ( nInset + ::marginHeight ) + __MtFontHeight()
   ENDIF

   RETURN Self

// The resources of Label, the others from Primitive
METHOD SetValue( nResource, xValue ) CLASS __MtLabel

   DO CASE
   CASE nResource == MtNlabelString
      ::labelString := xValue
   CASE nResource == MtNalignment
      ::alignment := xValue
   CASE nResource == MtNmarginWidth
      ::marginWidth := xValue
   CASE nResource == MtNmarginHeight
      ::marginHeight := xValue
   CASE nResource == MtNrecomputeSize
      ::recomputeSize := xValue
   OTHERWISE
      RETURN Super:SetValue( nResource, xValue )
   ENDCASE

   RETURN .T.

/* ---------------------------------------------------------------------------------------------
   __MtPushButton: a label that can be pressed, like XmPushButton, with every resource of the
   label. Raised at rest, sunken while it is armed; a click or Space / Enter with the focus calls
   MtNactivateCallback.
   --------------------------------------------------------------------------------------------- */

CREATE CLASS __MtPushButton FROM __MtLabel

   DATA armColor  AS INT       INIT -1
   DATA fillOnArm AS LOGIC     INIT .T.
   DATA armed     AS LOGIC     INIT .F.

   METHOD New( oParent, cName )
   METHOD WidgetClass()
   METHOD Activate( nKey )
   METHOD Arm( nKey )
   METHOD ArmAndActivate( nKey )
   METHOD Disarm( nKey )
   METHOD Expose()
   METHOD GetValue( nResource, aPair )
   METHOD HasCallback( nCallback )
   METHOD SetValue( nResource, xValue )

ENDCLASS

// A push button: a label with a shadow that takes the focus
METHOD New( oParent, cName ) CLASS __MtPushButton

   Super:New( oParent, cName )
   ::shadowThickness := 2
   ::traversalOn     := .T.
   ::marginWidth     := 6
   ::Resize()

   RETURN Self

// The name of the class in the messages of MyTiff
METHOD WidgetClass() CLASS __MtPushButton

   RETURN "PushButton"

// Calls MtNactivateCallback when the button is sensitive
METHOD Activate( nKey ) CLASS __MtPushButton

   IF ::sensitive
      ::CallCallbacks( MtNactivateCallback, { MtCR_ACTIVATE, nKey, 1 } )
   ENDIF

   RETURN Self

// .T. when the button went down
METHOD Arm( nKey ) CLASS __MtPushButton

   IF ::sensitive .AND. ! ::armed
      ::armed := .T.
      ::CallCallbacks( MtNarmCallback, { MtCR_ARM, nKey, 1 } )
   ENDIF

   RETURN ::armed

// Space or Enter with the focus: the button goes down, does its work and comes up again
METHOD ArmAndActivate( nKey ) CLASS __MtPushButton

   IF ! ::Arm( nKey )
      RETURN .F.
   ENDIF

   ::Activate( nKey )
   ::Disarm( nKey )

   RETURN .T.

// The button comes up and calls MtNdisarmCallback
METHOD Disarm( nKey ) CLASS __MtPushButton

   IF ::armed
      ::armed := .F.
      ::CallCallbacks( MtNdisarmCallback, { MtCR_DISARM, nKey, 1 } )
   ENDIF

   RETURN Self

// Draws the face, filled with armColor while armed, the shadow, the highlight and the text
METHOD Expose() CLASS __MtPushButton

   LOCAL aColors := ::Colors()
   LOCAL nInset  := ::highlightThickness + ::shadowThickness
   LOCAL nFill   := ::background
   LOCAL nTop, nLeft, nBottom, nRight

   ::Resize()
   IF ::armed .AND. ::fillOnArm
      nFill := IIF( ::armColor >= 0, ::armColor, aColors[ MtCOLOR_SELECT ] )
   ENDIF

   nTop := ::AbsY() + nInset
   nLeft := ::AbsX() + nInset
   nBottom := ::AbsY() + ::height - 1 - nInset
   nRight := ::AbsX() + ::width - 1 - nInset
   GfxPrimitive( GFX_FILLEDRECT, nTop, nLeft, nBottom, nRight, nFill )
   ::DrawShadow( ::armed )
   ::DrawHighlight()
   ::DrawLabel( IIF( ::sensitive, aColors[ MtCOLOR_FOREGROUND ], aColors[ MtCOLOR_BOTTOMSHADOW ] ) )

   RETURN Self

// The resources of PushButton, the others from Label
METHOD GetValue( nResource, aPair ) CLASS __MtPushButton

   DO CASE
   CASE nResource == MtNarmColor
      aPair[ 2 ] := ::armColor
   CASE nResource == MtNfillOnArm
      aPair[ 2 ] := ::fillOnArm
   OTHERWISE
      RETURN Super:GetValue( nResource, aPair )
   ENDCASE

   RETURN .T.

// The callbacks of a press and the ones of Core
METHOD HasCallback( nCallback ) CLASS __MtPushButton

   IF nCallback == MtNactivateCallback .OR. nCallback == MtNarmCallback .OR. nCallback == MtNdisarmCallback
      RETURN .T.
   ENDIF

   RETURN Super:HasCallback( nCallback )

// The resources of PushButton, the others from Label
METHOD SetValue( nResource, xValue ) CLASS __MtPushButton

   DO CASE
   CASE nResource == MtNarmColor
      ::armColor := xValue
   CASE nResource == MtNfillOnArm
      ::fillOnArm := xValue
   OTHERWISE
      RETURN Super:SetValue( nResource, xValue )
   ENDCASE

   RETURN .T.
