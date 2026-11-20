/*
 * $DOC$
 * $NAME$
 * WebGUI browse
 *
 * $ONELINER$
 * Demonstrates wg_Browse: a table of an array with a heading and a current row.
 *
 * $DESCRIPTION$
 * The table takes the height the window leaves. The arrows, PgUp, PgDn, Home and End move the current row,
 * Enter or a double click chooses it, a click on a heading sorts the rows. Build for Windows with the --windows
 * option.
 *
 * $EXAMPLES$
 * examples/webgui-browse.prg
 *
 * $OUTPUT$
 *
 * $DOCEND$
 */

PROCEDURE Main()

   LOCAL aRows := { ;
      { "Anna", "Warsaw", 4200.5, SToD( "20240115" ) }, ;
      { "Jan", "Krakow", 3800, SToD( "20230301" ) }, ;
      { "Ewa", "Gdansk", 5100.25, SToD( "20220910" ) }, ;
      { "Piotr", "Poznan", 2950, SToD( "20250602" ) } }
   LOCAL oWindow, oBrowse, oStatus

   oWindow := wg_Window():New( "xDev WebGUI browse", 640, 400 )

   // The table of the array, as high as the window lets it be
   oBrowse := wg_Browse():New( oWindow )
   oBrowse:grow := .T.
   oBrowse:AddColumn( "Name", {| aRow | aRow[ 1 ] } )
   oBrowse:AddColumn( "City", {| aRow | aRow[ 2 ] } )
   oBrowse:AddColumn( "Salary", {| aRow | aRow[ 3 ] }, NIL, "999,999.99" )
   oBrowse:AddColumn( "Since", {| aRow | aRow[ 4 ] } )
   oBrowse:SetArray( aRows )
   oBrowse:onChange := {| o | oStatus:text := "Row " + LTrim( Str( o:value ) ) }
   oBrowse:onClick := {| o | oStatus:text := "Chosen: " + aRows[ o:value ][ 1 ] }

   // The line under the table
   oStatus := wg_Label():New( oWindow, "Row 1" )

   oWindow:Run()

RETURN
