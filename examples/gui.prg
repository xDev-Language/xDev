/*
 * A window of the GUI library with a menu, a status bar with a clock and a background that changes colour.
 * The menu shows bWhen: "Stop colours" and "Start colours" are active by turns. Build with:
 *    xdev gui.prg --windows
 */

PROCEDURE Main()

   LOCAL aColors := { 0x264d73, 0x73264d, 0x4d7326, 0x262773, 0x267373, 0x734c26 }
   LOCAL nColor  := 1
   LOCAL nTicks  := 0
   LOCAL lColors := .T.
   LOCAL oWindow := gui_Window():New( "xDev GUI", 640, 400 )
   LOCAL oMenu   := gui_Menu():New()
   LOCAL oFile
   LOCAL oHelp
   LOCAL oStatus
   LOCAL oTimer
   LOCAL oLabel

   gui_MenuStyle( .T. )

   oWindow:background := aColors[ nColor ]
   oWindow:foreground := 0xffffff

   oLabel := gui_Label():New( oWindow, 40, 120, 560, 60, "Hello from xDev" )
   oLabel:font := gui_Font():New( 32, .T., .F., "Segoe UI" )
   gui_Label():New( oWindow, 44, 190, 560, 24, "File > Next colour or Ctrl+N changes the colour at once" )

   oStatus := gui_StatusBar():New( oWindow )
   oStatus:Add( 0, s_ColorText( aColors[ nColor ] ) )
   oStatus:Add( 90, Time() )

   oTimer := gui_Timer():New( oWindow, 1000, {|| ;
      oStatus:SetText( 2, Time() ), ;
      IIF( lColors .AND. ++nTicks % 2 == 0, s_NextColor( oWindow, oStatus, aColors, @nColor ), NIL ) } )

   oFile := oMenu:AddMenu( "&File" )
   oFile:Add( "&Next colour", {|| s_NextColor( oWindow, oStatus, aColors, @nColor ) }, "Ctrl+N" )
   oFile:AddSeparator()
   oFile:Add( "&Stop colours",  {|| lColors := .F. }, NIL, NIL, {|| lColors } )
   oFile:Add( "S&tart colours", {|| lColors := .T. }, NIL, NIL, {|| ! lColors } )
   oFile:AddSeparator()
   oFile:Add( "E&xit", {|| IIF( gui_MsgYesNo( "Close the program?", "xDev GUI" ), oWindow:Close(), NIL ) }, "Alt+F4" )

   oHelp := oMenu:AddMenu( "&Help" )
   oHelp:Add( "&About", {|| gui_MsgInfo( "Example of the xDev GUI library:" + Chr( 10 ) + ;
                                          "menu, status bar, timer and colours.", "About" ) } )

   oWindow:menu := oMenu
   oTimer:Start()

   gui_Application():Run( oWindow )

RETURN

STATIC PROCEDURE s_NextColor( oWindow, oStatus, aColors, nColor )

   nColor := IIF( nColor == Len( aColors ), 1, nColor + 1 )
   oWindow:background := aColors[ nColor ]
   oStatus:SetText( 1, s_ColorText( aColors[ nColor ] ) )

RETURN

STATIC FUNCTION s_ColorText( nColor )

RETURN "Colour #" + Lower( NumToHex( nColor, 6 ) )
