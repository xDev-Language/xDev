/*
 * Copyright (c) 2024-2026 xDev-Language
 *
 * Library...: xDev Examples
 * File......: gui.prg
 *
 */

/*
 * $DOC$
 * $NAME$
 * GUI menu window
 *
 * $ONELINER$
 * Demonstrates a GUI window with a menu, status clock, and changing background.
 *
 * $DESCRIPTION$
 * The menu alternates the availability of Stop colours and Start colours actions. Build for Windows with the --windows option.
 *
 * $EXAMPLES$
 * examples/gui.prg
 *
 * $OUTPUT$
 *
 * $DOCEND$
 */

PROCEDURE Main()

   LOCAL aColors := { 0x264d73, 0x73264d, 0x4d7326, 0x262773, 0x267373, 0x734c26 }
   LOCAL nColor  := 1
   LOCAL nTicks  := 0
   LOCAL lColors := .T.
   LOCAL oWindow := guiWindow():New( "xDev GUI", 640, 400 )
   LOCAL oMenu   := guiMenu():New()
   LOCAL oFile
   LOCAL oHelp
   LOCAL oStatus
   LOCAL oTimer
   LOCAL oLabel

   guiMenuStyle( .T. )

   oWindow:background := aColors[ nColor ]
   oWindow:foreground := 0xffffff

   oLabel := guiLabel():New( oWindow, 40, 120, 560, 60, "Hello from xDev" )
   oLabel:font := guiFont():New( 32, .T., .F., "Segoe UI" )
   guiLabel():New( oWindow, 44, 190, 560, 24, "File > Next colour or Ctrl+N changes the colour at once" )

   oStatus := guiStatusBar():New( oWindow )
   oStatus:Add( 0, s_ColorText( aColors[ nColor ] ) )
   oStatus:Add( 90, Time() )

   oTimer := guiTimer():New( oWindow, 1000, {|| ;
      oStatus:SetText( 2, Time() ), ;
      IIF( lColors .AND. ++nTicks % 2 == 0, s_NextColor( oWindow, oStatus, aColors, @nColor ), NIL ) } )

   oFile := oMenu:AddMenu( "File" )
   oFile:Add( "Next colour", {|| s_NextColor( oWindow, oStatus, aColors, @nColor ) }, "Ctrl+N" )
   oFile:AddSeparator()
   oFile:Add( "Stop colours",  {|| lColors := .F. }, NIL, NIL, {|| lColors } )
   oFile:Add( "Start colours", {|| lColors := .T. }, NIL, NIL, {|| ! lColors } )
   oFile:AddSeparator()
   oFile:Add( "Exit", {|| IIF( guiMsgYesNo( "Close the program?", "xDev GUI" ), oWindow:Close(), NIL ) }, "Alt+F4" )

   oHelp := oMenu:AddMenu( "Help" )
   oHelp:Add( "About", {|| guiMsgInfo( "Example of the xDev GUI library:" + Chr( 10 ) + ;
                                          "menu, status bar, timer and colours.", "About" ) } )

   oWindow:menu := oMenu
   oTimer:Start()

   guiApplication():Run( oWindow )

RETURN

STATIC PROCEDURE s_NextColor( oWindow, oStatus, aColors, nColor )

   nColor := IIF( nColor == Len( aColors ), 1, nColor + 1 )
   oWindow:background := aColors[ nColor ]
   oStatus:SetText( 1, s_ColorText( aColors[ nColor ] ) )

RETURN

STATIC FUNCTION s_ColorText( nColor )

RETURN "Colour #" + Lower( NumToHex( nColor, 6 ) )


