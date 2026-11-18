/*
 * Copyright (c) 2024-2026 xDev-Language
 *
 * Library: xDev TUI
 * File: tbrowse.prg
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "inkey.ch"
#include "button.ch"
#include "tbrowse.ch"
#include "tui.ch"

// Alignment of a cell text inside its column
#define ALIGN_LEFT      0
#define ALIGN_RIGHT     1
#define ALIGN_CENTER    2

/* ---------------------------------------------------------------------------------------------
   TBColumn
   --------------------------------------------------------------------------------------------- */

CREATE CLASS TBColumn

   // Code block that returns the value shown in the column
   DATA block      AS CODEBLOCK

   // Value for the user of the column
   DATA cargo

   // Code block that gets the value and returns { nStandard, nSelected }, indexes into TBrowse:colorSpec
   DATA colorBlock AS CODEBLOCK

   // Indexes into TBrowse:colorSpec for the cell, the selected cell, the heading and the footing
   DATA defColor   AS ARRAY  INIT { 1, 2, 1, 1 }

   // Heading and footing; ";" starts a new line
   DATA heading    AS STRING INIT ""
   DATA footing    AS STRING INIT ""

   // Separators of the column; "" takes the separator of the browse
   DATA colSep     AS STRING INIT ""
   DATA headSep    AS STRING INIT ""
   DATA footSep    AS STRING INIT ""

   // Picture for Transform() of the value
   DATA picture    AS STRING INIT ""

   // Code blocks for the program that edits a cell; the browse does not use them
   DATA preBlock   AS CODEBLOCK
   DATA postBlock  AS CODEBLOCK

   // Width of the column on the screen; 0 takes the widest of the heading, the footing and the value
   DATA width      AS INT    INIT 0

   METHOD New( cHeading, bBlock )

ENDCLASS

// Create a column with the heading cHeading that shows the value of bBlock
// TBColumn():New( [cHeading], bBlock ) --> self
METHOD New( cHeading, bBlock ) CLASS TBColumn

   IF cHeading != NIL
      ::heading := cHeading
   ENDIF

   ::block := bBlock

   RETURN Self

/* ---------------------------------------------------------------------------------------------
   TBrowse
   --------------------------------------------------------------------------------------------- */

CREATE CLASS TBrowse

   // Rectangle of the browse on the screen, border included
   DATA nTop           AS INT       INIT 0
   DATA nLeft          AS INT       INIT 0
   DATA nBottom        AS INT       INIT 0
   DATA nRight         AS INT       INIT 0

   // Eight characters of the border as in DispBox(); "" draws no border
   DATA border         AS STRING    INIT ""

   // Value for the user of the browse
   DATA cargo

   // Color table: pairs { nForeground, nBackground } of 0xRRGGBB colors, indexed by TBColumn:defColor
   // and TBColumn:colorBlock; the first pair also colors the border, the separators and the empty space
   DATA colorSpec      AS ARRAY     INIT { { COLOR_TEXT, COLOR_PAPER }, { COLOR_PAPER, COLOR_BLUE } }

   // Separators between the columns, under the headings and above the footings
   DATA colSep         AS STRING    INIT " "
   DATA headSep        AS STRING    INIT ""
   DATA footSep        AS STRING    INIT ""

   // Number of the leftmost columns that never scroll away
   DATA freeze         AS INT       INIT 0

   // Code blocks that move the data source to the first record, to the last record,
   // and by n records; skipBlock returns how many records it really moved
   DATA goTopBlock     AS CODEBLOCK
   DATA goBottomBlock  AS CODEBLOCK
   DATA skipBlock      AS CODEBLOCK

   // .T. highlights the current cell when the browse becomes stable
   DATA autoLite       AS LOGIC     INIT .T.

   // Text for a status line; the browse does not show it
   DATA message        AS STRING    INIT ""

   // Data row and column of the cursor
   DATA rowPos         AS INT       INIT 1
   DATA colPos         AS INT       INIT 1

   // Number of data rows on the screen
   DATA rowCount       AS INT       INIT 0

   // First and last unfrozen column on the screen
   DATA leftVisible    AS INT       INIT 0
   DATA rightVisible   AS INT       INIT 0

   // Data row and column under the mouse, set by hitTest()
   DATA mRowPos        AS INT       INIT 0
   DATA mColPos        AS INT       INIT 0

   // .T. when the last move stopped at the beginning or at the end of the data
   DATA hitTop         AS LOGIC     INIT .F.
   DATA hitBottom      AS LOGIC     INIT .F.

   // .T. when the screen shows the current data and the cursor
   DATA stable         AS LOGIC     INIT .F.

   // Columns, and the keys of applyKey() as pairs { nKey, bBlock }
   DATA columns        AS ARRAY
   DATA keys           AS ARRAY

   // .F. when Configure() has to run before drawing; configuredWith holds the settings it used
   DATA configured     AS LOGIC     INIT .F.
   DATA configuredWith AS ARRAY

   // Rectangle inside the border
   DATA areaTop        AS INT       INIT 0
   DATA areaLeft       AS INT       INIT 0
   DATA areaBottom     AS INT       INIT 0
   DATA areaRight      AS INT       INIT 0

   // Rows of the headings, of the heading separator, of the footing separator and of the footings
   DATA headRows       AS INT       INIT 0
   DATA headSepRows    AS INT       INIT 0
   DATA footSepRows    AS INT       INIT 0
   DATA footRows       AS INT       INIT 0

   // Screen row of the first data row
   DATA dataTop        AS INT       INIT 0

   // For every column: its width, its screen column and the screen column of the separator
   // before it; -1 when not on the screen
   DATA widths         AS ARRAY
   DATA colX           AS ARRAY
   DATA sepX           AS ARRAY

   // For every data row: its cells { cText, nStandard, nSelected }, and .T. when they are read
   // from the data source and when they are on the screen
   DATA cells          AS ARRAY
   DATA rowRead        AS ARRAY
   DATA rowShown       AS ARRAY

   // Data rows that can hold records, and the data row of the record the data source stands on
   DATA dataRows       AS INT       INIT 0
   DATA bufPos         AS INT       INIT 1

   // Pending moves: rows to move the cursor by, -1 for goTop() and 1 for goBottom()
   DATA rowMove        AS INT       INIT 0
   DATA jumpTo         AS INT       INIT 0

   // .T. reads all data rows again around the current record
   DATA reread         AS LOGIC     INIT .F.

   // .T. after panLeft() and panRight(): the cursor follows the columns, not the columns the cursor
   DATA panning        AS LOGIC     INIT .F.

   // .T. when the border, the headings, the footings and their separators are on the screen
   DATA framesShown    AS LOGIC     INIT .F.

   // Data row and column of the highlighted cell; litRow is 0 when none
   DATA litRow         AS INT       INIT 0
   DATA litCol         AS INT       INIT 0

   METHOD New( nTop, nLeft, nBottom, nRight )

   METHOD Down()
   METHOD End()
   METHOD GoBottom()
   METHOD GoTop()
   METHOD Home()
   METHOD Left()
   METHOD PageDown()
   METHOD PageUp()
   METHOD PanEnd()
   METHOD PanHome()
   METHOD PanLeft()
   METHOD PanRight()
   METHOD Right()
   METHOD Up()

   METHOD AddColumn( oCol )
   METHOD ApplyKey( nKey )
   METHOD ColCount()
   METHOD ColorRect( aRect, aColors )
   METHOD ColWidth( nColumn )
   METHOD Configure()
   METHOD DeHilite()
   METHOD DelColumn( nColumn )
   METHOD ForceStable()
   METHOD GetColumn( nColumn )
   METHOD Hilite()
   METHOD HitTest( nRow, nCol )
   METHOD InsColumn( nColumn, oCol )
   METHOD Invalidate()
   METHOD RefreshAll()
   METHOD RefreshCurrent()
   METHOD SetColumn( nColumn, oCol )
   METHOD SetKey( nKey, bBlock )
   METHOD Stabilize()

   METHOD Settings()
   METHOD Arrange()
   METHOD LastColumn( nFreeze, nFirst )
   METHOD FreeWidth( nFreeze, nFirst, nLast )
   METHOD ColumnSep( nColumn )
   METHOD HeadingSep( nColumn )
   METHOD FootingSep( nColumn )
   METHOD SepWidth( nColumn )
   METHOD SkipRows( nRows )
   METHOD Sync()
   METHOD MoveRows()
   METHOD ShiftRows( nShift )
   METHOD Refill()
   METHOD ReadRow( nRow )
   METHOD DrawFrames()
   METHOD DrawTitle( nColumn, cText, nTop, nRows, lBottom, aColor )
   METHOD DrawSeparator( nRow, lHead )
   METHOD DrawRow( nRow )
   METHOD DrawCell( nRow, nColumn, lSelected )
   METHOD ClearRows( nTop, nRows )
   METHOD Put( nRow, nCol, cText, aColor )
   METHOD OnCell()
   METHOD KeyIndex( nKey )

ENDCLASS

// Create a browse in the rectangle nTop, nLeft, nBottom, nRight, border included
// TBrowse():New( nTop, nLeft, nBottom, nRight ) --> self
METHOD New( nTop, nLeft, nBottom, nRight ) CLASS TBrowse

   ::nTop    := nTop
   ::nLeft   := nLeft
   ::nBottom := nBottom
   ::nRight  := nRight
   ::columns := {}
   ::keys    := { ;
      { K_DOWN,        {| oTB | oTB:Down(), TBR_CONTINUE } }, ;
      { K_END,         {| oTB | oTB:End(), TBR_CONTINUE } }, ;
      { K_CTRL_PGDN,  {| oTB | oTB:GoBottom(), TBR_CONTINUE } }, ;
      { K_CTRL_PGUP,   {| oTB | oTB:GoTop(), TBR_CONTINUE } }, ;
      { K_HOME,        {| oTB | oTB:Home(), TBR_CONTINUE } }, ;
      { K_LEFT,        {| oTB | oTB:Left(), TBR_CONTINUE } }, ;
      { K_PGDN,        {| oTB | oTB:PageDown(), TBR_CONTINUE } }, ;
      { K_PGUP,        {| oTB | oTB:PageUp(), TBR_CONTINUE } }, ;
      { K_CTRL_END,    {| oTB | oTB:PanEnd(), TBR_CONTINUE } }, ;
      { K_CTRL_HOME,   {| oTB | oTB:PanHome(), TBR_CONTINUE } }, ;
      { K_CTRL_LEFT,   {| oTB | oTB:PanLeft(), TBR_CONTINUE } }, ;
      { K_CTRL_RIGHT,  {| oTB | oTB:PanRight(), TBR_CONTINUE } }, ;
      { K_RIGHT,       {| oTB | oTB:Right(), TBR_CONTINUE } }, ;
      { K_UP,          {| oTB | oTB:Up(), TBR_CONTINUE } }, ;
      { K_ESC,         {|| TBR_EXIT } }, ;
      { K_LBUTTONDOWN, {| oTB | BrowseMouse( oTB, MRow(), MCol() ) } }, ;
      { K_MWFORWARD,   {| oTB | oTB:Up(), TBR_CONTINUE } }, ;
      { K_MWBACKWARD,  {| oTB | oTB:Down(), TBR_CONTINUE } } }

   RETURN Self

/* -------------------------------- Cursor Movement Methods ----------------------------------- */

// Move the cursor down one row
// oBrowse:Down() --> self
METHOD Down() CLASS TBrowse

   ::rowMove++
   ::stable := .F.

   RETURN Self

// Move the cursor to the rightmost column on the screen
// oBrowse:End() --> self
METHOD End() CLASS TBrowse

   ::colPos := ::rightVisible
   ::stable := .F.

   RETURN Self

// Move the data source to the last record, shown in the last data row
// oBrowse:GoBottom() --> self
METHOD GoBottom() CLASS TBrowse

   ::jumpTo := 1
   ::stable := .F.

   RETURN Self

// Move the data source to the first record, shown in the first data row
// oBrowse:GoTop() --> self
METHOD GoTop() CLASS TBrowse

   ::jumpTo := -1
   ::stable := .F.

   RETURN Self

// Move the cursor to the leftmost column on the screen
// oBrowse:Home() --> self
METHOD Home() CLASS TBrowse

   ::colPos := IIF( ::freeze > 0, 1, ::leftVisible )
   ::stable := .F.

   RETURN Self

// Move the cursor left one column
// oBrowse:Left() --> self
METHOD Left() CLASS TBrowse

   IF ::colPos > 1
      ::colPos--
      ::stable := .F.
   ENDIF

   RETURN Self

// Show the next page of records
// oBrowse:PageDown() --> self
METHOD PageDown() CLASS TBrowse

   ::rowMove += ::rowCount
   ::stable  := .F.

   RETURN Self

// Show the previous page of records
// oBrowse:PageUp() --> self
METHOD PageUp() CLASS TBrowse

   ::rowMove -= ::rowCount
   ::stable  := .F.

   RETURN Self

// Move the cursor to the last column, scrolling the columns
// oBrowse:PanEnd() --> self
METHOD PanEnd() CLASS TBrowse

   ::colPos      := Len( ::columns )
   ::leftVisible := Len( ::columns )
   ::stable      := .F.

   RETURN Self

// Move the cursor to the first column, scrolling the columns
// oBrowse:PanHome() --> self
METHOD PanHome() CLASS TBrowse

   ::colPos      := 1
   ::leftVisible := 1
   ::stable      := .F.

   RETURN Self

// Scroll the unfrozen columns one column to the right, so the earlier column comes into view
// oBrowse:PanLeft() --> self
METHOD PanLeft() CLASS TBrowse

   IF ::leftVisible > ::freeze + 1
      ::leftVisible--
      ::panning := .T.
      ::stable  := .F.
   ENDIF

   RETURN Self

// Scroll the unfrozen columns one column to the left, so the next column comes into view
// oBrowse:PanRight() --> self
METHOD PanRight() CLASS TBrowse

   IF ::rightVisible < Len( ::columns )
      ::leftVisible++
      ::panning := .T.
      ::stable  := .F.
   ENDIF

   RETURN Self

// Move the cursor right one column
// oBrowse:Right() --> self
METHOD Right() CLASS TBrowse

   IF ::colPos < Len( ::columns )
      ::colPos++
      ::stable := .F.
   ENDIF

   RETURN Self

// Move the cursor up one row
// oBrowse:Up() --> self
METHOD Up() CLASS TBrowse

   ::rowMove--
   ::stable := .F.

   RETURN Self

/* -------------------------------- Miscellaneous Methods ------------------------------------- */

// Add the column oCol after the last column
// oBrowse:AddColumn( oCol ) --> self
METHOD AddColumn( oCol ) CLASS TBrowse

   AAdd( ::columns, oCol )
   ::configured := .F.

   RETURN Self

// Evaluate the block that setKey() holds for nKey, or for 0 when nKey has none
// oBrowse:ApplyKey( nKey ) --> nResult
METHOD ApplyKey( nKey ) CLASS TBrowse

   LOCAL nIndex := ::KeyIndex( nKey )

   IF nIndex == 0
      nIndex := ::KeyIndex( 0 )
   ENDIF

   IF nIndex == 0
      RETURN TBR_EXCEPTION
   ENDIF

   RETURN Eval( ::keys[ nIndex ][ 2 ], Self, nKey )

// Return the number of columns
// oBrowse:ColCount() --> nCount
METHOD ColCount() CLASS TBrowse

   RETURN Len( ::columns )

// Color the cells of the data rows and columns aRect { nTop, nLeft, nBottom, nRight } with the
// colorSpec indexes aColors { nStandard, nSelected } until the rows are read again
// oBrowse:ColorRect( aRect, aColors ) --> self
METHOD ColorRect( aRect, aColors ) CLASS TBrowse

   LOCAL nRow
   LOCAL nColumn
   LOCAL aCell

   FOR nRow := Max( 1, aRect[ 1 ] ) TO Min( aRect[ 3 ], ::rowCount )
      IF ::rowRead[ nRow ]
         FOR nColumn := Max( 1, aRect[ 2 ] ) TO Min( aRect[ 4 ], Len( ::columns ) )
            aCell      := ::cells[ nRow ][ nColumn ]
            aCell[ 2 ] := aColors[ 1 ]
            aCell[ 3 ] := aColors[ 2 ]
         NEXT

         ::rowShown[ nRow ] := .F.
      ENDIF
   NEXT

   ::stable := .F.

   RETURN Self

// Return the width of column nColumn on the screen
// oBrowse:ColWidth( nColumn ) --> nWidth
METHOD ColWidth( nColumn ) CLASS TBrowse

   IF ! ::configured
      ::Configure()
   ENDIF

   RETURN ::widths[ nColumn ]

// Compute the rows and the column widths again from the settings of the browse and its columns
// oBrowse:Configure() --> self
METHOD Configure() CLASS TBrowse

   LOCAL nBorder  := IIF( Len( ::border ) > 0, 1, 0 )
   LOCAL nCount   := Len( ::columns )
   LOCAL lHeadSep := Len( ::headSep ) > 0
   LOCAL lFootSep := Len( ::footSep ) > 0
   LOCAL oCol
   LOCAL i

   ::Sync()

   ::areaTop    := ::nTop + nBorder
   ::areaLeft   := ::nLeft + nBorder
   ::areaBottom := ::nBottom - nBorder
   ::areaRight  := ::nRight - nBorder

   ::headRows := 0
   ::footRows := 0
   ::widths   := Array( nCount )

   FOR i := 1 TO nCount
      oCol          := ::columns[ i ]
      ::headRows    := Max( ::headRows, Len( TextLines( oCol:heading ) ) )
      ::footRows    := Max( ::footRows, Len( TextLines( oCol:footing ) ) )
      lHeadSep      := lHeadSep .OR. Len( oCol:headSep ) > 0
      lFootSep      := lFootSep .OR. Len( oCol:footSep ) > 0
      ::widths[ i ] := ColumnWidth( oCol )
   NEXT

   ::headSepRows := IIF( lHeadSep, 1, 0 )
   ::footSepRows := IIF( lFootSep, 1, 0 )
   ::dataTop     := ::areaTop + ::headRows + ::headSepRows
   ::rowCount    := Max( 0, ::areaBottom - ::dataTop + 1 - ::footSepRows - ::footRows )
   ::rowPos      := Max( 1, Min( ::rowPos, ::rowCount ) )
   ::bufPos      := ::rowPos

   ::cells    := Array( ::rowCount )
   ::rowRead  := AFill( Array( ::rowCount ), .F. )
   ::rowShown := AFill( Array( ::rowCount ), .F. )
   ::colX     := AFill( Array( nCount ), -1 )
   ::sepX     := AFill( Array( nCount ), -1 )

   ::configuredWith := ::Settings()
   ::configured     := .T.
   ::reread         := .T.
   ::framesShown    := .F.
   ::litRow         := 0
   ::stable         := .F.

   RETURN Self

// Draw the current cell in its standard color
// oBrowse:DeHilite() --> self
METHOD DeHilite() CLASS TBrowse

   IF ::OnCell()
      ::DrawCell( ::rowPos, ::colPos, .F. )
      ::litRow := 0
   ENDIF

   RETURN Self

// Remove column nColumn and return it
// oBrowse:DelColumn( nColumn ) --> oCol
METHOD DelColumn( nColumn ) CLASS TBrowse

   LOCAL oCol := ::columns[ nColumn ]

   ADel( ::columns, nColumn, .T. )
   ::configured := .F.

   RETURN oCol

// Call stabilize() until the browse is stable
// oBrowse:ForceStable() --> self
METHOD ForceStable() CLASS TBrowse

   DO WHILE ! ::Stabilize()
   ENDDO

   RETURN Self

// Return column nColumn
// oBrowse:GetColumn( nColumn ) --> oCol
METHOD GetColumn( nColumn ) CLASS TBrowse

   RETURN ::columns[ nColumn ]

// Draw the current cell in its selected color and put the screen cursor on it
// oBrowse:Hilite() --> self
METHOD Hilite() CLASS TBrowse

   IF ::OnCell()
      ::DrawCell( ::rowPos, ::colPos, .T. )
      SetPos( ::dataTop + ::rowPos - 1, ::colX[ ::colPos ] )
      ::litRow := ::rowPos
      ::litCol := ::colPos
   ENDIF

   RETURN Self

// Return the part of the browse at screen position nRow, nCol as a HT* value of button.ch;
// on a cell mRowPos and mColPos get its data row and column
// oBrowse:HitTest( nRow, nCol ) --> nHitTest
METHOD HitTest( nRow, nCol ) CLASS TBrowse

   LOCAL i

   IF ! ::configured .OR. nRow < ::nTop .OR. nRow > ::nBottom .OR. nCol < ::nLeft .OR. nCol > ::nRight
      RETURN HTNOWHERE
   ENDIF

   IF nRow < ::areaTop
      DO CASE
      CASE nCol < ::areaLeft
         RETURN HTTOPLEFT
      CASE nCol > ::areaRight
         RETURN HTTOPRIGHT
      ENDCASE

      RETURN HTTOP
   ENDIF

   IF nRow > ::areaBottom
      DO CASE
      CASE nCol < ::areaLeft
         RETURN HTBOTTOMLEFT
      CASE nCol > ::areaRight
         RETURN HTBOTTOMRIGHT
      ENDCASE

      RETURN HTBOTTOM
   ENDIF

   IF nCol < ::areaLeft
      RETURN HTLEFT
   ENDIF

   IF nCol > ::areaRight
      RETURN HTRIGHT
   ENDIF

   IF nRow < ::areaTop + ::headRows
      RETURN HTHEADING
   ENDIF

   IF nRow < ::dataTop
      RETURN HTHEADSEP
   ENDIF

   IF nRow >= ::dataTop + ::rowCount + ::footSepRows
      RETURN HTFOOTING
   ENDIF

   IF nRow >= ::dataTop + ::rowCount
      RETURN HTFOOTSEP
   ENDIF

   FOR i := 1 TO Len( ::columns )

      IF ::colX[ i ] >= 0 .AND. nCol >= ::colX[ i ] .AND. nCol < ::colX[ i ] + ::widths[ i ]
         ::mRowPos := nRow - ::dataTop + 1
         ::mColPos := i
         RETURN HTCELL
      ENDIF

      IF ::sepX[ i ] >= 0 .AND. nCol >= ::sepX[ i ] .AND. nCol < ::sepX[ i ] + ::SepWidth( i )
         RETURN HTCOLSEP
      ENDIF

   NEXT

   RETURN HTNOWHERE

// Insert the column oCol at position nColumn and return it
// oBrowse:InsColumn( nColumn, oCol ) --> oCol
METHOD InsColumn( nColumn, oCol ) CLASS TBrowse

   AIns( ::columns, nColumn, oCol, .T. )
   ::configured := .F.

   RETURN oCol

// Draw the whole browse again at the next stabilization, without reading the data again
// oBrowse:Invalidate() --> self
METHOD Invalidate() CLASS TBrowse

   ::framesShown := .F.
   ::stable      := .F.

   RETURN Self

// Read all data rows again at the next stabilization
// oBrowse:RefreshAll() --> self
METHOD RefreshAll() CLASS TBrowse

   ::reread := .T.
   ::stable := .F.

   RETURN Self

// Read the current data row again at the next stabilization
// oBrowse:RefreshCurrent() --> self
METHOD RefreshCurrent() CLASS TBrowse

   IF ::rowPos <= Len( ::rowRead )
      ::rowRead[ ::rowPos ] := .F.
   ENDIF

   ::stable := .F.

   RETURN Self

// Replace column nColumn by oCol and return the replaced column
// oBrowse:SetColumn( nColumn, oCol ) --> oOldCol
METHOD SetColumn( nColumn, oCol ) CLASS TBrowse

   LOCAL oOld := ::columns[ nColumn ]

   ::columns[ nColumn ] := oCol
   ::configured := .F.

   RETURN oOld

// Return the block for the Inkey() value nKey; with bBlock set it, with NIL as bBlock remove it.
// nKey 0 holds the block for the keys that have none
// oBrowse:SetKey( nKey, [bBlock] ) --> bPrevious
METHOD SetKey( nKey, bBlock ) CLASS TBrowse

   LOCAL nIndex := ::KeyIndex( nKey )
   LOCAL bOld   := IIF( nIndex > 0, ::keys[ nIndex ][ 2 ], NIL )

   IF PCount() >= 2
      IF bBlock == NIL
         IF nIndex > 0
            ADel( ::keys, nIndex, .T. )
         ENDIF
      ELSEIF nIndex > 0
         ::keys[ nIndex ][ 2 ] := bBlock
      ELSE
         AAdd( ::keys, { nKey, bBlock } )
      ENDIF
   ENDIF

   RETURN bOld

// Do one step of drawing the browse: read or draw one data row, or finish with the cursor;
// return .T. when the browse is stable
// oBrowse:Stabilize() --> lStable
METHOD Stabilize() CLASS TBrowse

   LOCAL nRow

   IF ! ::configured .OR. ! SameValues( ::configuredWith, ::Settings() )
      ::Configure()
   ENDIF

   DispBegin()

   ::Arrange()

   IF ::litRow > 0 .AND. ( ! ::stable .OR. ::litRow != ::rowPos .OR. ::litCol != ::colPos )
      ::rowShown[ ::litRow ] := .F.
      ::litRow := 0
   ENDIF

   IF ::jumpTo != 0
      ::hitTop    := .F.
      ::hitBottom := .F.
      Eval( IIF( ::jumpTo < 0, ::goTopBlock, ::goBottomBlock ) )
      ::rowPos  := IIF( ::jumpTo < 0, 1, ::rowCount )
      ::bufPos  := ::rowPos
      ::jumpTo  := 0
      ::rowMove := 0
      ::reread  := .T.
   ELSEIF ::rowMove != 0
      ::MoveRows()
   ENDIF

   IF ::reread
      ::Refill()
   ENDIF

   IF ! ::framesShown
      ::DrawFrames()
   ENDIF

   FOR nRow := 1 TO ::rowCount
      IF ! ::rowRead[ nRow ] .OR. ! ::rowShown[ nRow ]
         IF ! ::rowRead[ nRow ]
            ::ReadRow( nRow )
         ENDIF

         ::DrawRow( nRow )
         ::stable := .F.
         DispEnd()
         RETURN .F.
      ENDIF
   NEXT

   ::rowPos := Max( 1, Min( ::rowPos, ::dataRows ) )
   ::Sync()

   IF ::autoLite .AND. ::litRow == 0
      ::Hilite()
   ENDIF

   ::stable := .T.

   DispEnd()

   RETURN .T.

/* -------------------------------- Internal Methods ------------------------------------------ */

// Return the settings of the browse that Configure() turns into rows
// oBrowse:Settings() --> aSettings
METHOD Settings() CLASS TBrowse

   RETURN { ::nTop, ::nLeft, ::nBottom, ::nRight, ::border, ::headSep, ::footSep }

// Choose the columns on the screen from freeze, leftVisible and colPos, and place them;
// a change of the places draws the whole browse again
// oBrowse:Arrange() --> self
METHOD Arrange() CLASS TBrowse

   LOCAL nCount  := Len( ::columns )
   LOCAL nFreeze := Max( 0, Min( ::freeze, nCount ) )
   LOCAL lFirst  := .T.
   LOCAL nFirst
   LOCAL nLast
   LOCAL nPad
   LOCAL nX
   LOCAL nSepX
   LOCAL x
   LOCAL i

   IF nCount == 0
      ::leftVisible  := 0
      ::rightVisible := 0
      RETURN Self
   ENDIF

   ::colPos := Max( 1, Min( ::colPos, nCount ) )

   nFirst := Max( nFreeze + 1, Min( ::leftVisible, nCount ) )
   IF ! ::panning .AND. ::colPos > nFreeze .AND. ::colPos < nFirst
      nFirst := ::colPos
   ENDIF

   nLast := ::LastColumn( nFreeze, nFirst )

   IF ::panning
      IF ::colPos > nFreeze
         ::colPos := Max( nFirst, Min( ::colPos, nLast ) )
      ENDIF
   ELSE
      DO WHILE ::colPos > nLast
         nFirst++
         nLast := ::LastColumn( nFreeze, nFirst )
      ENDDO
   ENDIF

   ::panning := .F.

   DO WHILE nFirst > nFreeze + 1 .AND. ::FreeWidth( nFreeze, nFirst - 1, nLast ) >= 0
      nFirst--
   ENDDO

   nPad := Max( 0, Int( ::FreeWidth( nFreeze, nFirst, nLast ) / 2 ) )
   x    := ::areaLeft

   FOR i := 1 TO nCount

      nX    := -1
      nSepX := -1

      IF i <= nFreeze .OR. ( i >= nFirst .AND. i <= nLast )
         IF ! lFirst
            nSepX := x
            x     += ::SepWidth( i )
         ENDIF

         IF i == nFirst
            x += nPad
         ENDIF

         nX     := x
         x      += ::widths[ i ]
         lFirst := .F.
      ENDIF

      IF nX != ::colX[ i ] .OR. nSepX != ::sepX[ i ]
         ::colX[ i ]   := nX
         ::sepX[ i ]   := nSepX
         ::framesShown := .F.
      ENDIF

   NEXT

   ::leftVisible  := IIF( nFirst <= nCount, nFirst, 0 )
   ::rightVisible := IIF( nFirst <= nCount, nLast, 0 )

   RETURN Self

// Return the last column that fits on the screen after the frozen columns and the columns from nFirst;
// column nFirst is always shown, cut at the right edge when it is too wide
// oBrowse:LastColumn( nFreeze, nFirst ) --> nLast
METHOD LastColumn( nFreeze, nFirst ) CLASS TBrowse

   LOCAL nLast := nFirst

   IF nFirst > Len( ::columns )
      RETURN Len( ::columns )
   ENDIF

   DO WHILE nLast < Len( ::columns ) .AND. ::FreeWidth( nFreeze, nFirst, nLast + 1 ) >= 0
      nLast++
   ENDDO

   RETURN nLast

// Return the screen columns left free by the frozen columns and the columns nFirst to nLast
// oBrowse:FreeWidth( nFreeze, nFirst, nLast ) --> nFree
METHOD FreeWidth( nFreeze, nFirst, nLast ) CLASS TBrowse

   LOCAL nFree  := ::areaRight - ::areaLeft + 1
   LOCAL lFirst := .T.
   LOCAL i

   FOR i := 1 TO nLast
      IF i <= nFreeze .OR. i >= nFirst
         nFree  -= ::widths[ i ] + IIF( lFirst, 0, ::SepWidth( i ) )
         lFirst := .F.
      ENDIF
   NEXT

   RETURN nFree

// Return the separator before column nColumn
// oBrowse:ColumnSep( nColumn ) --> cSep
METHOD ColumnSep( nColumn ) CLASS TBrowse

   LOCAL cSep := ::columns[ nColumn ]:colSep

   RETURN IIF( Len( cSep ) > 0, cSep, ::colSep )

// Return the heading separator of column nColumn
// oBrowse:HeadingSep( nColumn ) --> cSep
METHOD HeadingSep( nColumn ) CLASS TBrowse

   LOCAL cSep := ::columns[ nColumn ]:headSep

   RETURN IIF( Len( cSep ) > 0, cSep, ::headSep )

// Return the footing separator of column nColumn
// oBrowse:FootingSep( nColumn ) --> cSep
METHOD FootingSep( nColumn ) CLASS TBrowse

   LOCAL cSep := ::columns[ nColumn ]:footSep

   RETURN IIF( Len( cSep ) > 0, cSep, ::footSep )

// Return the width of the separator before column nColumn
// oBrowse:SepWidth( nColumn ) --> nWidth
METHOD SepWidth( nColumn ) CLASS TBrowse

   RETURN TextWidth( ::ColumnSep( nColumn ) )

// Move the data source by nRows records and return how many records it really moved
// oBrowse:SkipRows( nRows ) --> nMoved
METHOD SkipRows( nRows ) CLASS TBrowse

   IF nRows == 0
      RETURN 0
   ENDIF

   RETURN Eval( ::skipBlock, nRows )

// Move the data source to the record of the cursor row
// oBrowse:Sync() --> self
METHOD Sync() CLASS TBrowse

   ::bufPos += ::SkipRows( ::rowPos - ::bufPos )
   ::rowPos := ::bufPos

   RETURN Self

// Do the pending moves of down(), up(), pageDown() and pageUp(). A move out of the window scrolls
// the read rows; a page move aims at the last (first) row of the next (previous) page, so at the
// end (beginning) of the data the window stops on the last (first) record and the cursor keeps its row
// oBrowse:MoveRows() --> self
METHOD MoveRows() CLASS TBrowse

   LOCAL nMove   := ::rowMove + ::rowPos - ::bufPos
   LOCAL nNewPos := ::rowPos + ::rowMove
   LOCAL lSetPos := .T.
   LOCAL nShift  := 0
   LOCAL nMoved
   LOCAL i

   ::hitTop    := .F.
   ::hitBottom := .F.

   IF nNewPos < 1
      IF ::rowMove < -1
         nMove -= ::rowPos - 1
      ENDIF
   ELSEIF nNewPos > ::dataRows
      IF ::rowMove > 1
         nMove += ::dataRows - ::rowPos
      ENDIF
   ELSE
      ::rowPos := nNewPos
   ENDIF

   ::rowMove := 0
   nMoved    := ::SkipRows( nMove )

   IF nMoved > 0
      FOR i := ::dataRows + 1 TO Min( ::bufPos + nMoved, ::rowCount )
         ::rowRead[ i ] := .F.
      NEXT

      ::bufPos += nMoved
      IF ::bufPos > ::rowCount
         nShift   := ::bufPos - ::rowCount
         ::bufPos := ::rowCount
         lSetPos  := .F.
      ENDIF

      IF ::bufPos > ::dataRows
         ::dataRows := ::bufPos
         IF nMoved != nMove
            lSetPos := .F.
         ENDIF
      ENDIF
   ELSEIF nMoved < 0
      ::bufPos += nMoved
      IF ::bufPos < 1
         ::dataRows := Min( ::rowCount, ::dataRows - ::bufPos + 1 )
         nShift     := ::bufPos - 1
         ::bufPos   := 1
         lSetPos    := .F.
      ENDIF
   ELSEIF nMove > 0
      ::hitBottom := ::bufPos == ::rowPos
      ::dataRows  := ::bufPos
      FOR i := ::dataRows + 1 TO ::rowCount
         ::rowRead[ i ] := .F.
      NEXT
   ELSEIF nMove < 0
      ::hitTop := ::bufPos == ::rowPos
      IF ::bufPos > 1
         nShift   := ::bufPos - 1
         ::bufPos := 1
      ENDIF
   ENDIF

   IF nShift != 0
      ::ShiftRows( nShift )
   ENDIF

   IF lSetPos
      ::rowPos := ::bufPos
   ENDIF

   RETURN Self

// Scroll the read rows up by nShift rows (down when nShift is negative); the rows that come
// into the window are read at the next stabilization
// oBrowse:ShiftRows( nShift ) --> self
METHOD ShiftRows( nShift ) CLASS TBrowse

   LOCAL i

   IF Abs( nShift ) >= ::rowCount
      AFill( ::rowRead, .F. )
   ELSEIF nShift > 0
      FOR i := 1 TO nShift
         ADel( ::cells, 1 )
         ADel( ::rowRead, 1 )
         ::rowRead[ ::rowCount ] := .F.
      NEXT
   ELSE
      FOR i := 1 TO -nShift
         AIns( ::cells, 1 )
         AIns( ::rowRead, 1 )
         ::rowRead[ 1 ] := .F.
      NEXT
   ENDIF

   AFill( ::rowShown, .F. )

   RETURN Self

// Read all data rows again so that the current record stays in the cursor row
// oBrowse:Refill() --> self
METHOD Refill() CLASS TBrowse

   ::Sync()

   ::rowPos   := 1 - ::SkipRows( 1 - ::rowPos )
   ::bufPos   := 1
   ::dataRows := ::rowCount
   ::reread   := .F.

   AFill( ::rowRead, .F. )

   RETURN Self

// Read the cells of data row nRow from the data source; rows after the last record stay empty
// oBrowse:ReadRow( nRow ) --> self
METHOD ReadRow( nRow ) CLASS TBrowse

   LOCAL aRow  := Array( Len( ::columns ) )
   LOCAL lData := nRow <= ::dataRows
   LOCAL oCol
   LOCAL xValue
   LOCAL aColor
   LOCAL nAlign
   LOCAL cText
   LOCAL i

   IF lData .AND. nRow != ::bufPos
      ::bufPos += ::SkipRows( nRow - ::bufPos )
      IF ::bufPos != nRow
         ::dataRows := ::bufPos
         lData      := .F.
      ENDIF
   ENDIF

   FOR i := 1 TO Len( ::columns )
      oCol := ::columns[ i ]
      IF lData
         xValue    := Eval( oCol:block )
         aColor    := IIF( oCol:colorBlock == NIL, oCol:defColor, Eval( oCol:colorBlock, xValue ) )
         nAlign    := ALIGN_LEFT
         IF ValType( xValue ) == "N"
            nAlign := ALIGN_RIGHT
         ELSEIF ValType( xValue ) == "L"
            nAlign := ALIGN_CENTER
         ENDIF

         cText     := TextFit( CellText( oCol, xValue ), ::widths[ i ], nAlign )
         aRow[ i ] := { cText, aColor[ 1 ], aColor[ 2 ] }
      ELSE
         aRow[ i ] := { Space( ::widths[ i ] ), oCol:defColor[ 1 ], oCol:defColor[ 2 ] }
      ENDIF
   NEXT

   ::cells[ nRow ]    := aRow
   ::rowRead[ nRow ]  := .T.
   ::rowShown[ nRow ] := .F.

   RETURN Self

// Draw the border, the headings, the footings and their separators
// oBrowse:DrawFrames() --> self
METHOD DrawFrames() CLASS TBrowse

   LOCAL aColor   := ::colorSpec[ 1 ]
   LOCAL nFootTop := ::areaBottom - ::footRows + 1
   LOCAL oCol
   LOCAL i

   IF Len( ::border ) > 0
      DispBoxExt( ::nTop, ::nLeft, ::nBottom, ::nRight, ::border, aColor[ 1 ], aColor[ 2 ] )
   ENDIF

   ::ClearRows( ::areaTop, ::headRows + ::headSepRows )
   ::ClearRows( ::areaBottom - ::footRows - ::footSepRows + 1, ::footRows + ::footSepRows )

   FOR i := 1 TO Len( ::columns )
      IF ::colX[ i ] >= 0
         oCol := ::columns[ i ]
         ::DrawTitle( i, oCol:heading, ::areaTop, ::headRows, .T., ::colorSpec[ ColorIndex( oCol, 3 ) ] )
         ::DrawTitle( i, oCol:footing, nFootTop, ::footRows, .F., ::colorSpec[ ColorIndex( oCol, 4 ) ] )
      ENDIF
   NEXT

   IF ::headSepRows > 0
      ::DrawSeparator( ::areaTop + ::headRows, .T. )
   ENDIF

   IF ::footSepRows > 0
      ::DrawSeparator( ::areaBottom - ::footRows, .F. )
   ENDIF

   ::framesShown := .T.
   AFill( ::rowShown, .F. )

   RETURN Self

// Draw the lines of cText over column nColumn in the nRows rows from nTop; headings (lBottom .T.)
// end on the last row, footings start on the first row
// oBrowse:DrawTitle( nColumn, cText, nTop, nRows, lBottom, aColor ) --> self
METHOD DrawTitle( nColumn, cText, nTop, nRows, lBottom, aColor ) CLASS TBrowse

   LOCAL aLines := TextLines( cText )
   LOCAL nSkip  := IIF( lBottom, nRows - Len( aLines ), 0 )
   LOCAL nLine
   LOCAL cLine
   LOCAL i

   FOR i := 1 TO nRows
      nLine := i - nSkip
      cLine := IIF( nLine >= 1 .AND. nLine <= Len( aLines ), aLines[ nLine ], "" )
      ::Put( nTop + i - 1, ::colX[ nColumn ], TextFit( cLine, ::widths[ nColumn ], ALIGN_LEFT ), aColor )
   NEXT

   RETURN Self

// Draw the heading separator (lHead .T.) or the footing separator in screen row nRow; the separator
// string goes under the column separator, its last character fills the width of the columns
// oBrowse:DrawSeparator( nRow, lHead ) --> self
METHOD DrawSeparator( nRow, lHead ) CLASS TBrowse

   LOCAL aColor := ::colorSpec[ 1 ]
   LOCAL lFirst := .T.
   LOCAL nLast  := 0
   LOCAL nEnd
   LOCAL cSep
   LOCAL cFill
   LOCAL cText
   LOCAL i

   FOR i := 1 TO Len( ::columns )
      IF ::colX[ i ] >= 0
         nLast := i
      ENDIF
   NEXT

   FOR i := 1 TO Len( ::columns )
      IF ::colX[ i ] >= 0
         cSep  := IIF( lHead, ::HeadingSep( i ), ::FootingSep( i ) )
         cFill := URight( cSep, 1 )
         nEnd  := ::colX[ i ] + ::widths[ i ]

         IF lFirst
            ::Put( nRow, ::areaLeft, Replicate( cFill, nEnd - ::areaLeft ), aColor )
         ELSE
            cText := UPadR( ULeft( cSep, ::SepWidth( i ) ), ::colX[ i ] - ::sepX[ i ], cFill )
            ::Put( nRow, ::sepX[ i ], cText + Replicate( cFill, ::widths[ i ] ), aColor )
         ENDIF

         IF i == nLast
            ::Put( nRow, nEnd, Replicate( cFill, Max( 0, ::areaRight - nEnd + 1 ) ), aColor )
         ENDIF

         lFirst := .F.

      ENDIF

   NEXT

   RETURN Self

// Draw data row nRow with its column separators
// oBrowse:DrawRow( nRow ) --> self
METHOD DrawRow( nRow ) CLASS TBrowse

   LOCAL aColor := ::colorSpec[ 1 ]
   LOCAL i

   ::ClearRows( ::dataTop + nRow - 1, 1 )

   FOR i := 1 TO Len( ::columns )
      IF ::sepX[ i ] >= 0
         ::Put( ::dataTop + nRow - 1, ::sepX[ i ], ::ColumnSep( i ), aColor )
      ENDIF

      IF ::colX[ i ] >= 0
         ::DrawCell( nRow, i, .F. )
      ENDIF
   NEXT

   ::rowShown[ nRow ] := .T.

   IF nRow == ::litRow
      ::litRow := 0
   ENDIF

   RETURN Self

// Draw the cell of data row nRow and column nColumn in its standard or selected color
// oBrowse:DrawCell( nRow, nColumn, lSelected ) --> self
METHOD DrawCell( nRow, nColumn, lSelected ) CLASS TBrowse

   LOCAL aCell := ::cells[ nRow ][ nColumn ]

   ::Put( ::dataTop + nRow - 1, ::colX[ nColumn ], aCell[ 1 ], ::colorSpec[ aCell[ IIF( lSelected, 3, 2 ) ] ] )

   RETURN Self

// Fill nRows screen rows from nTop inside the border with the first color of colorSpec
// oBrowse:ClearRows( nTop, nRows ) --> self
METHOD ClearRows( nTop, nRows ) CLASS TBrowse

   LOCAL aColor := ::colorSpec[ 1 ]
   LOCAL i

   FOR i := 0 TO nRows - 1
      DispOutAtExt( nTop + i, ::areaLeft, Space( ::areaRight - ::areaLeft + 1 ), aColor[ 1 ], aColor[ 2 ] )
   NEXT

   RETURN Self

// Write cText at nRow, nCol in the color pair aColor, cut at the right edge inside the border
// oBrowse:Put( nRow, nCol, cText, aColor ) --> self
METHOD Put( nRow, nCol, cText, aColor ) CLASS TBrowse

   IF nCol <= ::areaRight
      cText := TextFit( cText, Min( TextWidth( cText ), ::areaRight - nCol + 1 ), ALIGN_LEFT )
      DispOutAtExt( nRow, nCol, cText, aColor[ 1 ], aColor[ 2 ] )
   ENDIF

   RETURN Self

// Return .T. when the cursor stands on a read cell that is on the screen
// oBrowse:OnCell() --> lOnCell
METHOD OnCell() CLASS TBrowse

   IF ::rowPos > ::rowCount .OR. ::colPos > Len( ::colX )
      RETURN .F.
   ENDIF

   RETURN ::rowRead[ ::rowPos ] .AND. ::colX[ ::colPos ] >= 0

// Return the index of nKey in keys, 0 when it has no block
// oBrowse:KeyIndex( nKey ) --> nIndex
METHOD KeyIndex( nKey ) CLASS TBrowse

   LOCAL i

   FOR i := 1 TO Len( ::keys )
      IF ::keys[ i ][ 1 ] == nKey
         RETURN i
      ENDIF
   NEXT

   RETURN 0

/* -------------------------------- Helpers --------------------------------------------------- */

// Move the cursor of the browse to the cell at screen position nRow, nCol
STATIC FUNCTION BrowseMouse( oTB, nRow, nCol )

   LOCAL i

   IF oTB:HitTest( nRow, nCol ) != HTCELL
      RETURN TBR_EXCEPTION
   ENDIF

   FOR i := oTB:rowPos + 1 TO oTB:mRowPos
      oTB:Down()
   NEXT

   FOR i := oTB:mRowPos + 1 TO oTB:rowPos
      oTB:Up()
   NEXT

   oTB:colPos := oTB:mColPos

   RETURN TBR_CONTINUE

// Return the width of the column on the screen; the only place where it is computed
STATIC FUNCTION ColumnWidth( oCol )

   LOCAL nWidth
   LOCAL aLines
   LOCAL i

   IF oCol:width > 0
      RETURN oCol:width
   ENDIF

   nWidth := TextWidth( CellText( oCol, Eval( oCol:block ) ) )

   aLines := TextLines( oCol:heading )
   FOR i := 1 TO Len( aLines )
      nWidth := Max( nWidth, TextWidth( aLines[ i ] ) )
   NEXT

   aLines := TextLines( oCol:footing )
   FOR i := 1 TO Len( aLines )
      nWidth := Max( nWidth, TextWidth( aLines[ i ] ) )
   NEXT

   RETURN Max( 1, nWidth )

// Return the value as the text of a cell; values that Transform() does not take give ""
STATIC FUNCTION CellText( oCol, xValue )

   IF ValType( xValue ) $ "CNDL"
      RETURN Transform( xValue, oCol:picture )
   ENDIF

   RETURN ""

// Return element n of TBColumn:defColor, 1 when the array is shorter
STATIC FUNCTION ColorIndex( oCol, n )

   RETURN IIF( Len( oCol:defColor ) >= n, oCol:defColor[ n ], 1 )

// Return the lines of a heading or a footing
STATIC FUNCTION TextLines( cText )

   RETURN IIF( Len( cText ) > 0, ATokens( cText, ";" ), {} )

// Return how many screen columns the text takes; the only place where text is measured
STATIC FUNCTION TextWidth( cText )

   RETURN ULen( cText )

// Cut or pad the text to nWidth screen columns, aligned by nAlign
STATIC FUNCTION TextFit( cText, nWidth, nAlign )

   LOCAL nFree

   cText := ULeft( cText, nWidth )
   nFree := nWidth - TextWidth( cText )

   SWITCH nAlign
   CASE ALIGN_RIGHT
      RETURN Space( nFree ) + cText
   CASE ALIGN_CENTER
      RETURN Space( Int( nFree / 2 ) ) + cText + Space( nFree - Int( nFree / 2 ) )
   ENDSWITCH

   RETURN cText + Space( nFree )

// Return .T. when the arrays a and b hold the same values
STATIC FUNCTION SameValues( a, b )

   LOCAL i

   FOR i := 1 TO Len( a )
      IF !( a[ i ] == b[ i ] )
         RETURN .F.
      ENDIF
   NEXT

   RETURN .T.
