/*
 * Copyright (c) 2024-2026 xDev-Language
 *
 * Library...: xDev Examples
 * File......: gui-sidebar.prg
 *
 */

/*
 * $DOC$
 * $NAME$
 * GUI sidebar
 *
 * $ONELINER$
 * Demonstrates guiSidebar with a page showing a guiMenu and a button at the bottom of the strip.
 *
 * $DESCRIPTION$
 * A click on a picture shows its page, a second click hides the panel. The right edge of the panel changes
 * its width. Build for Windows with the --windows option.
 *
 * $EXAMPLES$
 * examples/gui-sidebar.prg
 *
 * $OUTPUT$
 *
 * $DOCEND$
 */

PROCEDURE Main()

   LOCAL oWindow, oSide, oMenu, oBackup, oCleanup, oEditor, oMemo

   MakePictures()

   oWindow := guiWindow():New( "xDev sidebar", 900, 600 )

   // The menu of the panel: commands, a separator and drop-down menus
   oMenu := guiMenu():New( .T. )
   oMenu:Add( "Settings", {|| guiMsgInfo( "Settings" ) } )
   oMenu:Add( "Tokens / Certificates", {|| guiMsgInfo( "Tokens" ) } )
   oMenu:AddSeparator()
   oBackup := oMenu:AddMenu( "Data backup" )
   oBackup:Add( "Make a backup", {|| guiMsgInfo( "Backup" ) } )
   oBackup:Add( "Restore", {|| guiMsgInfo( "Restore" ) } )
   oCleanup := oMenu:AddMenu( "Data cleanup" )
   oCleanup:Add( "Reindex", {|| guiMsgInfo( "Reindex" ) } )

   // The sidebar stands at the left edge of the window, with the colors of a dark editor
   oSide := guiSidebar():New( oWindow )
   oSide:background := 0x181818
   oSide:foreground := 0xCCCCCC

   // Page 1: the panel shows the menu; the sidebar starts with the panel hidden
   oSide:AddButton( "files.bmp", "Options", NIL, oMenu )

   // A button at the bottom of the strip
   oSide:AddGlobal( "settings.bmp", {|| guiMsgInfo( "Settings" ) }, "Settings" )

   // The editor takes the rest of the window
   oEditor := guiPanel():New( oWindow )
   oMemo := guiMemo():New( oEditor, 0, 0, oEditor:width, oEditor:height, "" )
   oMemo:anchor := "LTRB"

   guiApplication():Run( oWindow )

RETURN

// The pictures of the example; with your own PNG files this part is not needed
PROCEDURE MakePictures()

   SaveBmp( "files.bmp", { ;
      ".######.....", ;
      ".#....##....", ;
      ".#....#.#...", ;
      ".#....####..", ;
      ".#.......#..", ;
      ".#.......#..", ;
      ".#.......#..", ;
      ".#.......#..", ;
      ".#.......#..", ;
      ".#########..", ;
      "............", ;
      "............" } )

   SaveBmp( "settings.bmp", { ;
      ".....##.....", ;
      "..#.####.#..", ;
      "...######...", ;
      "..###..###..", ;
      ".###....###.", ;
      ".###....###.", ;
      "..###..###..", ;
      "...######...", ;
      "..#.####.#..", ;
      ".....##.....", ;
      "............", ;
      "............" } )

RETURN

// Save a 24 x 24 BMP picture: "#" is light grey, "." is magenta, which the GUI shows as transparent
PROCEDURE SaveBmp( cFile, aRows )

   LOCAL cPixels := ""
   LOCAL cLine, nRow, nCol

   // BMP keeps the rows from the bottom; every point of aRows becomes 2 x 2 pixels
   FOR nRow := Len( aRows ) TO 1 STEP -1
      cLine := ""
      FOR nCol := 1 TO 12
         IF SubStr( aRows[ nRow ], nCol, 1 ) == "#"
            cLine += Replicate( Chr( 0xCC ) + Chr( 0xCC ) + Chr( 0xCC ), 2 )
         ELSE
            cLine += Replicate( Chr( 0xFF ) + Chr( 0x00 ) + Chr( 0xFF ), 2 )
         ENDIF
      NEXT
      cPixels += cLine + cLine
   NEXT

   MemoWrit( cFile, "BM" + L2Bin( 54 + Len( cPixels ) ) + L2Bin( 0 ) + L2Bin( 54 ) + ;
      L2Bin( 40 ) + L2Bin( 24 ) + L2Bin( 24 ) + I2Bin( 1 ) + I2Bin( 24 ) + ;
      L2Bin( 0 ) + L2Bin( Len( cPixels ) ) + L2Bin( 0 ) + L2Bin( 0 ) + L2Bin( 0 ) + L2Bin( 0 ) + ;
      cPixels, .F. )

RETURN


