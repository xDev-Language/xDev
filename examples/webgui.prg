/*
 * $DOC$
 * $NAME$
 * WebGUI window
 *
 * $ONELINER$
 * Demonstrates wg_Window with controls in a column and a row: the page draws them, the program keeps the logic.
 *
 * $DESCRIPTION$
 * The program needs no HTML file and no pixels: the window places its controls one under another, a row
 * places them side by side. Build for Windows with the --windows option.
 *
 * $EXAMPLES$
 * examples/webgui.prg
 *
 * $OUTPUT$
 *
 * $DOCEND$
 */

PROCEDURE Main()

   LOCAL aColors := { "Blue", "Green", "Red" }
   LOCAL oWindow, oName, oColor, oUpper, oRow, oButton, oGreeting

   oWindow := wg_Window():New( "xDev WebGUI", 480, 380 )

   // The fields, one under another
   oName := wg_Edit():New( oWindow, "xDev", "Your name" )
   oColor := wg_ComboBox():New( oWindow, aColors, "Color" )
   oUpper := wg_CheckBox():New( oWindow, "Capital letters" )

   // The button and the greeting beside it
   oRow := wg_Row():New( oWindow )
   oButton := wg_Button():New( oRow, "Greet" )
   oGreeting := wg_Label():New( oRow, "" )
   oButton:onClick := {|| oGreeting:text := Greeting( oName:value, aColors[ oColor:value ], oUpper:value ) }

   oWindow:Run()

RETURN

// The text of the greeting
STATIC FUNCTION Greeting( cName, cColor, lUpper )

   LOCAL cText := "Hello " + cName + "! Your color: " + cColor

RETURN IIF( lUpper, Upper( cText ), cText )
