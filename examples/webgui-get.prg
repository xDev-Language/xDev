/*
 * $DOC$
 * $NAME$
 * WebGUI get
 *
 * $ONELINER$
 * Demonstrates wg_Get: fields of text, a number, a date and a logical value with pictures.
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

   oWindow := wg_Window():New( "xDev WebGUI get", 560, 520 )

   // Name and phone side by side
   oRow := wg_Row():New( oWindow )
   oName := wg_Get():New( oRow, "", "@!", "Name" )
   oPhone := wg_Get():New( oRow, "", "(99) 999-99-99", "Phone" )

   // VALID: the amount must be above zero
   oAmount := wg_Get():New( oWindow, 1234.5, "999,999.99", "Amount" )
   oAmount:valid := {| oGet, nValue | nValue > 0 }

   oActive := wg_Get():New( oWindow, .T., "Yes/No", "Active" )

   // WHEN: the date can be entered only for an active person
   oDate := wg_Get():New( oWindow, Date(), NIL, "Date" )
   oDate:when := {|| oActive:value }

   // The values the program keeps
   oButton := wg_Button():New( oWindow, "Show" )
   oStatus := wg_Label():New( oWindow, "" )
   oButton:onClick := {|| oStatus:text := oName:value + " " + oPhone:value + " " + ;
      LTrim( Str( oAmount:value ) ) + " " + DToC( oDate:value ) + " " + IIF( oActive:value, "yes", "no" ) }

   oWindow:Run()

RETURN
