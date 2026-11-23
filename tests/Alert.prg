/*
 * Copyright (c) 2024-2026 xDev-Language
 *
 * Library...: xDev Tests
 * File......: Alert.prg
 *
 */

/*
 * $DOC$
 * $NAME$
 * Alert() test
 *
 * $ONELINER$
 * Tests Alert() with keys, colors, nDelay and a window that changes its size while the box is open.
 *
 * $DESCRIPTION$
 * The test needs no key presses until the end. The block of GtInfo( GTI_INKEYREAD ) gives Alert() the keys of
 * a script, one key each time Alert() waits for a key. A step of a script can also be a block: it looks at the
 * screen or changes the size of the window and then returns the key.
 *
 * GtInfo( GTI_MSGBOX, .F. ) makes Alert() draw its text box with every GT. A GT can show the message box of the
 * system instead; GTGLFW does it by default. The message box is not on the screen of the program, so the test
 * could not see its colors and its place.
 *
 * The window is empty and "W/N". The box is found by its color with ScreenAttr(): the box is "W+/B", so its
 * cells give its frame. The box is in the centre when its top and left are the same as Alert() computes them for
 * the current screen size.
 *
 * The last Alert() shows the resizing: SetMode() changes the window to 100 x 30, 60 x 20 and 120 x 35 while the
 * box is open, then the window goes to the full screen for a moment and comes back as 80 x 25. Every size stays
 * for one second, so the box can be seen moving to the centre, and every size is checked. The full screen must
 * have more rows or columns than the window, so the box is in the centre of the monitor, not of a small grid in
 * its corner. The GT changes the size for the full screen when it reads the events of the window, so there
 * Alert() gets the K_RESIZE of the GT. nDelay 30 ends the box if that K_RESIZE never comes. At the end the
 * results are shown in two columns.
 *
 * Not tested: a mouse click on a button.
 *
 * $EXAMPLES$
 * Alert
 *
 * $IMAGE$
 * Alert.png
 *
 * $DOCEND$
 */

#include "inkey.ch"
#include "gtinfo.ch"

// #if OS_LINUX
//    REQUEST GTXWC
// #else
//    REQUEST GTWVT
// #endif

// REQUEST GTGLFW
REQUEST GTSDL3

// The keys and blocks of the current script, the next step, and the results of the checks
STATIC s_aScript
STATIC s_nStep
STATIC s_aResults
STATIC s_lScreenKept
STATIC s_lCursorKept

// The size of the window before the full screen
STATIC s_nWindowRows
STATIC s_nWindowCols

PROCEDURE Main()

   LOCAL aOptions := { "Yes", "No", "Cancel" }
   LOCAL bKeys := {|| Alert( "Keys", aOptions, "W+/B" ) }
   LOCAL nStart

   s_aScript := {}
   s_nStep := 1
   s_aResults := {}
   s_lScreenKept := .T.
   s_lCursorKept := .T.

   GtInfo( GTI_WINTITLE, "Alert() test" )
   GtInfo( GTI_MSGBOX, .F. )
   GtInfo( GTI_INKEYREAD, {|| s_Next() } )
   SetMode( 25, 80 )
   SetColor( "W/N" )
   Scroll()
   SetPos( 5, 7 )

   // Keys
   s_Check( "Enter: first button", s_Alert( { K_ENTER }, bKeys ), 1 )
   s_Check( "Right, Enter: second", s_Alert( { K_RIGHT, K_ENTER }, bKeys ), 2 )
   s_Check( "Left wraps to the last", s_Alert( { K_LEFT, K_ENTER }, bKeys ), 3 )
   s_Check( "Tab x 3 wraps to the first", s_Alert( { K_TAB, K_TAB, K_TAB, K_ENTER }, bKeys ), 1 )
   s_Check( "Shift+Tab: the last", s_Alert( { K_SH_TAB, K_ENTER }, bKeys ), 3 )
   s_Check( "Right, Space: second", s_Alert( { K_RIGHT, K_SPACE }, bKeys ), 2 )
   s_Check( "letter n: No", s_Alert( { Asc( "n" ) }, bKeys ), 2 )
   s_Check( "letter C: Cancel", s_Alert( { Asc( "C" ) }, bKeys ), 3 )
   s_Check( "Esc gives 0", s_Alert( { K_ESC }, bKeys ), 0 )
   s_Check( "no options: one Ok button", s_Alert( { K_ENTER }, {|| Alert( "No options" ) } ), 1 )

   // nDelay: the script gives no key, so Alert() waits half a second
   nStart := Seconds()
   s_Check( "nDelay 0.5 gives 0", s_Alert( { 0 }, {|| Alert( "Closes itself", { "OK" }, "W+/B", 0.5 ) } ), 0 )
   s_Check( "nDelay 0.5 waited", Seconds() - nStart >= 0.4 .AND. Seconds() - nStart < 5, .T. )

   // Colors, looked at while the box is open
   s_Alert( { {|| s_Look( "W+/B box in the centre", 0x1F ) }, {|| s_Color( "chosen button B/W", 0x71 ) }, K_ENTER }, ;
      {|| Alert( "Colors;W+/B", aOptions, "W+/B" ) } )
   s_Alert( { {|| s_Look( "default W+/R box", 0x4F ) }, {|| s_Color( "default chosen W+/B", 0x1F ) }, K_ENTER }, ;
      {|| Alert( "Default colors", aOptions ) } )
   s_Alert( { {|| s_Look( "nNormal 0x2F box", 0x2F ) }, {|| s_Color( "nHigh 0x4E button", 0x4E ) }, K_ENTER }, ;
      {|| Alert( "Numeric colors", aOptions, 0x2F, 0x4E ) } )

   s_Check( "screen kept after Alert()", s_lScreenKept, .T. )
   s_Check( "cursor kept after Alert()", s_lCursorKept, .T. )

   // The window changes its size while the box is open; the box goes to the centre every time.
   // nDelay 30 ends the box when the GT never sends K_RESIZE, so the test does not wait for ever.
   s_Check( "resizing box ends with Enter", s_Alert( { {|| s_Look( "centre at 80 x 25", 0x1F ) }, ;
      {|| s_Resize( 30, 100 ) }, {|| s_Look( "centre at 100 x 30", 0x1F ) }, ;
      {|| s_Resize( 20, 60 ) }, {|| s_Look( "centre at 60 x 20", 0x1F ) }, ;
      {|| s_Resize( 35, 120 ) }, {|| s_Look( "centre at 120 x 35", 0x1F ) }, ;
      {|| s_FullScreen( .T. ) }, {|| s_Filled() }, {|| s_Look( "centre on the full screen", 0x1F ) }, ;
      {|| s_FullScreen( .F. ) }, {|| s_Resize( 25, 80 ) }, {|| s_Look( "centre at 80 x 25 again", 0x1F ) }, ;
      K_ENTER }, ;
      {|| Alert( "The window changes its size.;The box stays in the centre.", { "OK" }, "W+/B", 30 ) } ), 1 )
   SetMode( 25, 80 )

   // Argument errors
   s_Check( "Alert( ): 1102", s_Code( {|| Alert() } ), 1102 )
   s_Check( 'Alert( "x", "y" ): 1102', s_Code( {|| Alert( "x", "y" ) } ), 1102 )
   s_Check( 'Alert( "x", {}, {} ): 1102', s_Code( {|| Alert( "x", {}, {} ) } ), 1102 )

   GtInfo( GTI_INKEYREAD, {|| 0 } )
   s_Results()
   Inkey( 0, INKEY_KEYBOARD )

RETURN

// Run Alert() with a script of keys; check that the screen and the cursor come back
STATIC FUNCTION s_Alert( aScript, bAlert )

   LOCAL cScreen := SaveScreen()
   LOCAL nMaxRow := MaxRow()
   LOCAL nMaxCol := MaxCol()
   LOCAL nRow := Row()
   LOCAL nCol := Col()
   LOCAL nCursor := SetCursor()
   LOCAL nChoice

   Keyboard( "" )
   s_aScript := aScript
   s_nStep := 1
   nChoice := Eval( bAlert )

   // A screen of another size has other contents
   IF MaxRow() == nMaxRow .AND. MaxCol() == nMaxCol
      s_lScreenKept := s_lScreenKept .AND. SaveScreen() == cScreen
   ENDIF

   s_lCursorKept := s_lCursorKept .AND. Row() == nRow .AND. Col() == nCol .AND. SetCursor() == nCursor

RETURN nChoice

// The block of GTI_INKEYREAD: the next key of the script; a block is evaluated and gives the key
STATIC FUNCTION s_Next()

   LOCAL xStep

   IF s_nStep > Len( s_aScript )
      RETURN 0
   ENDIF

   xStep := s_aScript[ s_nStep ]
   s_nStep++

   IF ValType( xStep ) == "B"
      RETURN Eval( xStep )
   ENDIF

RETURN xStep

// Check that the box of color nColor is in the centre; Tab keeps the only button of the resizing box
STATIC FUNCTION s_Look( cTitle, nColor )

   SecondsSleep( 1 )
   s_Check( cTitle, s_Centered( nColor ), .T. )

RETURN K_TAB

// Check that some cell has the color nColor
STATIC FUNCTION s_Color( cTitle, nColor )

   s_Check( cTitle, s_Box( nColor ) != NIL, .T. )

RETURN K_TAB

// Change the size of the window while the box is open
STATIC FUNCTION s_Resize( nRows, nCols )

   GtInfo( GTI_WINTITLE, "Alert() test: " + LTrim( Str( nCols ) ) + " x " + LTrim( Str( nRows ) ) )
   s_Check( "SetMode( " + LTrim( Str( nRows ) ) + ", " + LTrim( Str( nCols ) ) + " )", SetMode( nRows, nCols ), .T. )

RETURN K_RESIZE

// Switch the full screen on or off while the box is open. The GT changes the screen size when it reads the
// events of the window, so 0 lets Alert() wait for the K_RESIZE of the GT instead of a key of the script.
STATIC FUNCTION s_FullScreen( lOn )

   IF lOn
      s_nWindowRows := MaxRow() + 1
      s_nWindowCols := MaxCol() + 1
   ENDIF

   GtInfo( GTI_ISFULLSCREEN, lOn )
   s_Check( "full screen " + IIF( lOn, "on", "off" ), GtInfo( GTI_ISFULLSCREEN ), lOn )

RETURN 0

// Check that the full screen has more rows or columns than the window had
STATIC FUNCTION s_Filled()

   s_Check( "full screen has more cells", MaxRow() + 1 > s_nWindowRows .OR. MaxCol() + 1 > s_nWindowCols, .T. )

RETURN K_TAB

// The frame of the cells with the color nColor: { nTop, nLeft, nBottom, nRight }, or NIL
STATIC FUNCTION s_Box( nColor )

   LOCAL aBox := NIL
   LOCAL nRow
   LOCAL nCol

   FOR nRow := 0 TO MaxRow()
      FOR nCol := 0 TO MaxCol()
         IF ScreenAttr( nRow, nCol ) == nColor
            IF aBox == NIL
               aBox := { nRow, nCol, nRow, nCol }
            ELSE
               aBox[ 2 ] := Min( aBox[ 2 ], nCol )
               aBox[ 3 ] := nRow
               aBox[ 4 ] := Max( aBox[ 4 ], nCol )
            ENDIF
         ENDIF
      NEXT
   NEXT

RETURN aBox

// Is the box of color nColor where Alert() puts it: in the centre of the current screen size?
STATIC FUNCTION s_Centered( nColor )

   LOCAL aBox := s_Box( nColor )
   LOCAL nHeight
   LOCAL nWidth

   IF aBox == NIL
      RETURN .F.
   ENDIF

   nHeight := aBox[ 3 ] - aBox[ 1 ] + 1
   nWidth := aBox[ 4 ] - aBox[ 2 ] + 1

RETURN aBox[ 1 ] == Int( ( MaxRow() + 1 - nHeight ) / 2 ) .AND. aBox[ 2 ] == Int( ( MaxCol() + 1 - nWidth ) / 2 )

// The subCode of the runtime error of the block, or 0 without an error
STATIC FUNCTION s_Code( bAction )

   LOCAL oError
   LOCAL bError := ErrorBlock( {| oCaught | Break( oCaught ) } )

   BEGIN SEQUENCE
      Eval( bAction )
   RECOVER USING oError
   ENDSEQUENCE
   ErrorBlock( bError )

RETURN IIF( oError == NIL, 0, oError:subCode )

STATIC PROCEDURE s_Check( cTitle, xResult, xExpected )

   AAdd( s_aResults, { cTitle, ValType( xResult ) == ValType( xExpected ) .AND. xResult == xExpected } )

RETURN

// The results in two columns, the number of errors and the end
STATIC PROCEDURE s_Results()

   LOCAL nErrors := 0
   LOCAL nHalf := Int( ( Len( s_aResults ) + 1 ) / 2 )
   LOCAL nIndex

   GtInfo( GTI_WINTITLE, "Alert() test" )
   Scroll()
   FOR nIndex := 1 TO Len( s_aResults )
      DispOutAt( IIF( nIndex <= nHalf, nIndex, nIndex - nHalf ), IIF( nIndex <= nHalf, 0, 40 ), ;
         PadR( s_aResults[ nIndex ][ 1 ], 32 ) + IIF( s_aResults[ nIndex ][ 2 ], "OK", "ERROR" ) )
      IF .NOT. s_aResults[ nIndex ][ 2 ]
         nErrors++
      ENDIF
   NEXT

   DispOutAt( 0, 0, "Alert() test: " + LTrim( Str( Len( s_aResults ) ) ) + " checks, " + ;
      IIF( nErrors == 0, "all OK", LTrim( Str( nErrors ) ) + " ERROR" ) )
   DispOutAt( MaxRow(), 0, "Press any key to exit." )

RETURN


