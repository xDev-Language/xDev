/*
 * Copyright (c) 2024-2026 xDev-Language
 *
 * Library...: xDev Examples
 * File......: webgui.prg
 *
 */

/*
 * $DOC$
 * $NAME$
 * WebGUI window
 *
 * $ONELINER$
 * Demonstrates wgWindow with controls in a column and a row: the page draws them, the program keeps the logic.
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

   oWindow := wgWindow():New( "xDev WebGUI", 480, 380 )

   // The fields, one under another
   oName := wgEdit():New( oWindow, "xDev", "Your name" )
   oColor := wgComboBox():New( oWindow, aColors, "Color" )
   oUpper := wgCheckBox():New( oWindow, "Capital letters" )

   // The button and the greeting beside it
   oRow := wgRow():New( oWindow )
   oButton := wgButton():New( oRow, "Greet" )
   oGreeting := wgLabel():New( oRow, "" )
   oButton:onClick := {|| oGreeting:text := Greeting( oName:value, aColors[ oColor:value ], oUpper:value ) }

   oWindow:Run()

RETURN

// The text of the greeting
STATIC FUNCTION Greeting( cName, cColor, lUpper )

   LOCAL cText := "Hello " + cName + "! Your color: " + cColor

RETURN IIF( lUpper, Upper( cText ), cText )


