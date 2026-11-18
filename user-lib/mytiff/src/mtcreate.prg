/*
 * Copyright (c) 2024-2026 xDev-Language
 *
 * Library: xDev MyTiff
 * File: mtcreate.prg
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "mytiff.ch"
#include "mtinternal.ch"

/* ---------------------------------------------------------------------------------------------
   Widgets and their resources, like XtCreateManagedWidget(), XtSetValues() and XtGetValues().
   aArgs is an array of pairs { { MtNlabelString, "Quit" }, ... }.
   A resource or a callback the class does not have is a runtime error with the class name.
   --------------------------------------------------------------------------------------------- */

// A new window of the application
// MtCreateShell( app, cName, [aArgs] ) --> w
FUNCTION MtCreateShell( app, cName, aArgs )

   LOCAL w := __MtShell():New( app, cName )

   s_SetValues( "MtCreateShell", w, aArgs )

   RETURN w

// A new label in a shell
// MtCreateLabel( wParent, cName, [aArgs] ) --> w
FUNCTION MtCreateLabel( wParent, cName, aArgs )

   LOCAL w

   s_CheckParent( "MtCreateLabel", wParent )
   w := __MtLabel():New( wParent, cName )
   s_SetValues( "MtCreateLabel", w, aArgs )

   RETURN w

// A new push button in a shell
// MtCreatePushButton( wParent, cName, [aArgs] ) --> w
FUNCTION MtCreatePushButton( wParent, cName, aArgs )

   LOCAL w

   s_CheckParent( "MtCreatePushButton", wParent )
   w := __MtPushButton():New( wParent, cName )
   s_SetValues( "MtCreatePushButton", w, aArgs )

   RETURN w

// Sets the resources of the pairs { { MtNlabelString, "Quit" }, ... }
FUNCTION MtSetValues( w, aArgs )

   s_SetValues( "MtSetValues", w, aArgs )

   RETURN NIL

// Fills the second element of every pair: { { MtNlabelString, NIL } } --> { { MtNlabelString, "Quit" } }
FUNCTION MtGetValues( w, aArgs )

   LOCAL i

   FOR i := 1 TO Len( aArgs )
      IF ! w:GetValue( aArgs[ i ][ 1 ], aArgs[ i ] )
         s_Error( "MtGetValues", w:WidgetClass() + " has no resource " + s_ResourceName( aArgs[ i ][ 1 ] ) )
      ENDIF
   NEXT

   RETURN aArgs

// bBlock is added to the list of the callback: {| w, aCallData | ... }
FUNCTION MtAddCallback( w, nCallback, bBlock )

   IF ! w:HasCallback( nCallback )
      s_Error( "MtAddCallback", w:WidgetClass() + " has no callback " + s_ResourceName( nCallback ) )
   ENDIF

   w:AddCallback( nCallback, bBlock )

   RETURN NIL

// The window of a shell shows with its children
FUNCTION MtRealizeWidget( w )

   w:Realize()

   RETURN NIL

// MtNdestroyCallback is called, the widget leaves with its children
FUNCTION MtDestroyWidget( w )

   w:Destroy()

   RETURN NIL

/* ---------------------------------------------------------------------------------------------
   Functions of this file
   --------------------------------------------------------------------------------------------- */

// Without managers the parent of a Label or a PushButton is a shell
STATIC FUNCTION s_CheckParent( cFunction, wParent )

   IF ValType( wParent ) != "O" .OR. ! wParent:WidgetClass() == "Shell"
      s_Error( cFunction, "the parent must be a Shell" )
   ENDIF

   RETURN NIL

// Sets the pairs of aArgs; a resource the class does not have is an error
STATIC FUNCTION s_SetValues( cFunction, w, aArgs )

   LOCAL i

   IF aArgs == NIL
      RETURN NIL
   ENDIF

   FOR i := 1 TO Len( aArgs )
      IF ! w:SetValue( aArgs[ i ][ 1 ], aArgs[ i ][ 2 ] )
         s_Error( cFunction, w:WidgetClass() + " has no resource " + s_ResourceName( aArgs[ i ][ 1 ] ) )
      ENDIF
   NEXT

   w:Changed()

   RETURN NIL

// The message on stderr, then Break() with it, so BEGIN SEQUENCE can catch it; ErrorNew() in
// the RTL is still a stub
STATIC FUNCTION s_Error( cFunction, cText )

   LOCAL cMessage := "MyTiff: " + cFunction + "(): " + cText

   OutErr( cMessage + Chr( 10 ) )
   Break( cMessage )

   RETURN NIL

// The name of a resource or a callback in mytiff.ch, for the messages
STATIC FUNCTION s_ResourceName( nResource )

   LOCAL aNames := { ;
      { MtNx, "MtNx" }, { MtNy, "MtNy" }, { MtNwidth, "MtNwidth" }, { MtNheight, "MtNheight" }, ;
      { MtNbackground, "MtNbackground" }, { MtNsensitive, "MtNsensitive" }, ;
      { MtNdestroyCallback, "MtNdestroyCallback" }, ;
      { MtNforeground, "MtNforeground" }, { MtNshadowThickness, "MtNshadowThickness" }, ;
      { MtNhighlightThickness, "MtNhighlightThickness" }, { MtNtopShadowColor, "MtNtopShadowColor" }, ;
      { MtNbottomShadowColor, "MtNbottomShadowColor" }, { MtNhighlightColor, "MtNhighlightColor" }, ;
      { MtNtraversalOn, "MtNtraversalOn" }, ;
      { MtNlabelString, "MtNlabelString" }, { MtNalignment, "MtNalignment" }, ;
      { MtNmarginWidth, "MtNmarginWidth" }, { MtNmarginHeight, "MtNmarginHeight" }, ;
      { MtNrecomputeSize, "MtNrecomputeSize" }, ;
      { MtNarmColor, "MtNarmColor" }, { MtNfillOnArm, "MtNfillOnArm" }, ;
      { MtNactivateCallback, "MtNactivateCallback" }, { MtNarmCallback, "MtNarmCallback" }, ;
      { MtNdisarmCallback, "MtNdisarmCallback" }, ;
      { MtNtitle, "MtNtitle" }, { MtNmwmDecorations, "MtNmwmDecorations" }, ;
      { MtNdeleteResponse, "MtNdeleteResponse" }, { MtNdocked, "MtNdocked" } }

   LOCAL nPos := AScan( aNames, {| a | a[ 1 ] == nResource } )

   RETURN IIF( nPos > 0, aNames[ nPos ][ 2 ], "number " + LTrim( Str( nResource ) ) )
