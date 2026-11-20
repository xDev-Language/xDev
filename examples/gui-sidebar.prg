/*
 * $DOC$
 * $NAME$
 * GUI sidebar
 *
 * $ONELINER$
 * Demonstrates gui_Sidebar with two pages, an action in the header and a button at the bottom of the strip.
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

   LOCAL oWindow, oSide, oEditor, oMemo, oPanel, oTree, nFiles, nSearch

   MakePictures()

   oWindow := gui_Window():New( "xDev sidebar", 900, 600 )

   // The sidebar stands at the left edge of the window
   oSide := gui_Sidebar():New( oWindow )

   // Page 1: files
   nFiles := oSide:Add( "EXPLORER", "files.bmp" )
   oPanel := oSide:Panel( nFiles )
   oTree  := gui_TreeView():New( oPanel, 0, 0, oPanel:width, oPanel:height )
   oTree:anchor := "LTRB"
   oTree:Add( "main.prg" )
   oTree:Add( "hello.prg" )
   oSide:AddAction( nFiles, "clear.bmp", {|| oTree:Clear() }, "Clear" )

   // Page 2: search
   nSearch := oSide:Add( "SEARCH", "search.bmp" )
   oPanel  := oSide:Panel( nSearch )
   gui_Label():New( oPanel, 12, 12, 200, 20, "Text to find:" )
   gui_Edit():New( oPanel, 12, 36, 200, 24, "" )

   // A button at the bottom of the strip
   oSide:AddButton( "settings.bmp", {|| gui_MsgInfo( "Settings" ) }, "Settings" )

   oSide:page := nFiles

   // The editor takes the rest of the window
   oEditor := gui_Panel():New( oWindow )
   oMemo := gui_Memo():New( oEditor, 0, 0, oEditor:width, oEditor:height, "" )
   oMemo:anchor := "LTRB"

   gui_Application():Run( oWindow )

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

   SaveBmp( "search.bmp", { ;
      "...####.....", ;
      "..#....#....", ;
      ".#......#...", ;
      ".#......#...", ;
      ".#......#...", ;
      "..#....#....", ;
      "...#####....", ;
      ".......##...", ;
      "........##..", ;
      ".........##.", ;
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

   SaveBmp( "clear.bmp", { ;
      "............", ;
      "............", ;
      "..#......#..", ;
      "...#....#...", ;
      "....#..#....", ;
      ".....##.....", ;
      ".....##.....", ;
      "....#..#....", ;
      "...#....#...", ;
      "..#......#..", ;
      "............", ;
      "............" } )

RETURN

// Save a 24 x 24 BMP picture: "#" is dark grey, "." is magenta, which the GUI shows as transparent
PROCEDURE SaveBmp( cFile, aRows )

   LOCAL cPixels := ""
   LOCAL cLine, nRow, nCol

   // BMP keeps the rows from the bottom; every point of aRows becomes 2 x 2 pixels
   FOR nRow := Len( aRows ) TO 1 STEP -1
      cLine := ""
      FOR nCol := 1 TO 12
         IF SubStr( aRows[ nRow ], nCol, 1 ) == "#"
            cLine += Replicate( Chr( 0x42 ) + Chr( 0x42 ) + Chr( 0x42 ), 2 )
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
