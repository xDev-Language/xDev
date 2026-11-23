/*
 * Copyright (c) 2024-2026 xDev-Language
 *
 * Library...: xDev Examples
 * File......: webgui-get.prg
 *
 */

/*
 * $DOC$
 * $NAME$
 * WebGUI get
 *
 * $ONELINER$
 * Demonstrates wgGet: fields of text, a number, a date and a logical value with pictures.
 *
 * $DESCRIPTION$
 * Enter keeps the value and goes to the next field. A value that does not fit the picture is marked red.
 * VALID refuses an amount of zero or less, WHEN lets the user into the date only when Active is Yes.
 * Build for Windows with the --windows option.
 *
 * $EXAMPLES$
 * examples/webgui-get.prg
 *
 * $OUTPUT$
 *
 * $DOCEND$
 */

PROCEDURE Main()

   LOCAL oWindow, oRow, oName, oPhone, oAmount, oDate, oActive, oButton, oStatus

   oWindow := wgWindow():New( "xDev WebGUI get", 560, 520 )

   // Name and phone side by side
   oRow := wgRow():New( oWindow )
   oName := wgGet():New( oRow, "", "@!", "Name" )
   oPhone := wgGet():New( oRow, "", "(99) 999-99-99", "Phone" )

   // VALID: the amount must be above zero
   oAmount := wgGet():New( oWindow, 1234.5, "999,999.99", "Amount" )
   oAmount:valid := {| oGet, nValue | nValue > 0 }

   oActive := wgGet():New( oWindow, .T., "Yes/No", "Active" )

   // WHEN: the date can be entered only for an active person
   oDate := wgGet():New( oWindow, Date(), NIL, "Date" )
   oDate:when := {|| oActive:value }

   // The values the program keeps
   oButton := wgButton():New( oWindow, "Show" )
   oStatus := wgLabel():New( oWindow, "" )
   oButton:onClick := {|| oStatus:text := oName:value + " " + oPhone:value + " " + ;
      LTrim( Str( oAmount:value ) ) + " " + DToC( oDate:value ) + " " + IIF( oActive:value, "yes", "no" ) }

   oWindow:Run()

RETURN


