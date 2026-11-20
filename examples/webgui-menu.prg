/*
 * $DOC$
 * $NAME$
 * WebGUI menu
 *
 * $ONELINER$
 * Demonstrates wg_Menu: a menu bar with items, shortcuts, a separator and a command of the bar.
 *
 * $DESCRIPTION$
 * The arrows, Esc, Home and End move in the menus; Ctrl+N and F1 run their items from the keyboard.
 * Build for Windows with the --windows option.
 *
 * $EXAMPLES$
 * examples/webgui-menu.prg
 *
 * $OUTPUT$
 *
 * $DOCEND$
 */

PROCEDURE Main()

   LOCAL oWindow, oFile, oHelp, oStatus

   oWindow := wg_Window():New( "xDev WebGUI menu", 520, 300 )

   // The menu File
   oFile := wg_Menu():New( oWindow, "File" )
   oFile:Add( "New", {|| oStatus:text := "File > New" }, "Ctrl+N" )
   oFile:Add( "Open", {|| oStatus:text := "File > Open" } )
   oFile:AddSeparator()
   oFile:Add( "Exit", {|| oWindow:Close() } )

   // The menu Help
   oHelp := wg_Menu():New( oWindow, "Help" )
   oHelp:Add( "About", {|| oStatus:text := "xDev WebGUI" }, "F1" )

   // A command of the bar, without items
   wg_Menu():New( oWindow, "Clear", {|| oStatus:text := "" } )

   // The line that shows the chosen item
   oStatus := wg_Label():New( oWindow, "Choose an item of the menu" )

   oWindow:Run()

RETURN
