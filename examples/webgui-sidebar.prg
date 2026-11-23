/*
 * Copyright (c) 2024-2026 xDev-Language
 *
 * Library...: xDev Examples
 * File......: webgui-sidebar.prg
 *
 */

/*
 * $DOC$
 * $NAME$
 * WebGUI sidebar
 *
 * $ONELINER$
 * Demonstrates wgSidebar and wgSplit: a strip of page pictures, panels with controls, and a split main area.
 *
 * $DESCRIPTION$
 * A click on a picture shows its page, a second click hides the panel. The mouse drags the right edge of the
 * panel and the line of the split. Build for Windows with the --windows option.
 *
 * $EXAMPLES$
 * examples/webgui-sidebar.prg
 *
 * $OUTPUT$
 *
 * $DOCEND$
 */

PROCEDURE Main()

   LOCAL aFiles := { "main.prg", "hello.prg", "README.md" }
   LOCAL oWindow, oSide, oPanel, oFiles, oSplit, oLeft, oRight, oStatus, nFiles, nSearch

   MakePictures()

   oWindow := wgWindow():New( "xDev WebGUI sidebar", 1000, 600 )

   // The sidebar
   oSide := wgSidebar():New( oWindow )
   oSide:onChange := {| o | oStatus:text := "Page " + LTrim( Str( o:value ) ) }

   // Page 1: files
   nFiles := oSide:Add( "Explorer", "files.svg" )
   oPanel := oSide:Panel( nFiles )
   oFiles := wgBrowse():New( oPanel )
   oFiles:grow := .T.
   oFiles:AddColumn( "File", {| cName | cName } )
   oFiles:SetArray( aFiles )
   oFiles:onClick := {| o | oStatus:text := "Open: " + aFiles[ o:value ] }
   oSide:AddAction( nFiles, "clear.svg", {|| oStatus:text := "" }, "Clear" )

   // Page 2: search
   nSearch := oSide:Add( "Search", "search.svg" )
   oPanel := oSide:Panel( nSearch )
   wgEdit():New( oPanel, "", "Text to find" )

   // A button at the bottom of the strip
   oSide:AddButton( "settings.svg", {|| oStatus:text := "Settings" }, "Settings" )

   oSide:value := nFiles

   // The main area: a split with notes at the left and the status at the right
   oSplit := wgSplit():New( oWindow, 400 )
   oLeft := oSplit:Panel( 1 )
   wgEdit():New( oLeft, "", "Notes" )
   oRight := oSplit:Panel( 2 )
   oStatus := wgLabel():New( oRight, "Click the pictures of the strip" )

   oWindow:Run()

RETURN

// The pictures of the example; with your own pictures this part is not needed
PROCEDURE MakePictures()

   MemoWrit( "files.svg", Svg( '<path d="M6 3h8l4 4v14H6z M14 3v4h4"/>' ), .F. )
   MemoWrit( "search.svg", Svg( '<circle cx="10" cy="10" r="6"/><path d="M15 15l6 6"/>' ), .F. )
   MemoWrit( "clear.svg", Svg( '<path d="M6 6l12 12M18 6L6 18"/>' ), .F. )
   MemoWrit( "settings.svg", Svg( '<circle cx="12" cy="12" r="3"/>' + ;
      '<path d="M12 2v4M12 18v4M2 12h4M18 12h4M5 5l3 3M16 16l3 3M5 19l3-3M16 8l3-3"/>' ), .F. )

RETURN

// An SVG picture of 24 x 24 points drawn with dark grey lines
FUNCTION Svg( cShape )

RETURN '<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" fill="none" stroke="#424242" ' + ;
   'stroke-width="1.5">' + cShape + '</svg>'


