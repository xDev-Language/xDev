/*
 * Copyright (c) 2024-2026 xDev-Language
 *
 * Library...: xDev GUI
 * File......: internal-gui-browse.c
 *
 */

#include <string.h>
#include "internal-gui.h"

static const wchar_t s_gridProperty[] = L"xDev.GUI.Grid";
static ATOM s_browseClass;

/* Operations of X_GUI_EVENT_NAVIGATE of a browse */
enum
{
   X_GUI_GO_TOP = 1,
   X_GUI_GO_BOTTOM,
   X_GUI_SKIP,
   X_GUI_POSITION,
   X_GUI_EMPTY
};

/* Kinds of X_GUI_EVENT_BROWSE of a browse */
enum
{
   X_GUI_BROWSE_CHANGE = 1,
   X_GUI_BROWSE_CHOOSE
};

/* ---------------------------------------------------------------------------------------------
   Browses
   --------------------------------------------------------------------------------------------- */

/* The xDev colors of a browse; the program may change them */
#define X_GUI_BROWSE_HEAD 0x264D73
#define X_GUI_BROWSE_HEADTEXT 0xF2F2F2
#define X_GUI_BROWSE_HEADLINE 0x1A3550
#define X_GUI_BROWSE_ROW 0xFFFFFF
#define X_GUI_BROWSE_ROW2 0xE9EDF1
#define X_GUI_BROWSE_TEXT 0x111111
#define X_GUI_BROWSE_LINE 0xD3DBE3
#define X_GUI_BROWSE_CURSOR 0x1A3550
#define X_GUI_BROWSE_CURSORTXT 0xF2F2F2
#define X_GUI_BROWSE_INACTIVE 0xD3DBE3

static const int s_gridColors[X_GUI_GRID_COLORS] = { X_GUI_BROWSE_HEAD, X_GUI_BROWSE_HEADTEXT, X_GUI_BROWSE_HEADLINE,
                                                     X_GUI_BROWSE_ROW, X_GUI_BROWSE_ROW2, X_GUI_BROWSE_TEXT,
                                                     X_GUI_BROWSE_LINE, X_GUI_BROWSE_CURSOR, X_GUI_BROWSE_CURSORTXT,
                                                     X_GUI_BROWSE_INACTIVE, X_GUI_BROWSE_TEXT };

/* The grid data of a browse, or NULL */
X_GUI_GRID *x_guiGridOf( HWND browse )
{
   return ( X_GUI_GRID * )GetPropW( browse, s_gridProperty );
}

/* Release the texts of the rows on the screen */
static void s_gridCellsFree( X_GUI_GRID *grid )
{
   size_t i;

   for( i = 0; grid->cells != NULL && i < ( size_t )grid->cellRows * grid->columnCount; ++i )
      x_xfree( grid->cells[i] );

   x_xfree( grid->cells );
   grid->cells = NULL;
   grid->cellRows = 0;
   grid->rows = 0;
}

/* Release the grid data of a browse */
static void s_gridRelease( HWND browse )
{
   X_GUI_GRID *grid = ( X_GUI_GRID * )RemovePropW( browse, s_gridProperty );
   size_t i;

   if( grid == NULL )
      return;

   s_gridCellsFree( grid );
   for( i = 0; i < grid->columnCount; ++i )
      x_xfree( grid->columns[i].title );

   x_xfree( grid->columns );
   x_xfree( grid );
}

/* Height of a row and of the heading in pixels of the screen */
static int s_browseRowHeight( HWND browse )
{
   UINT dpi = GetDpiForWindow( browse );
   HDC dc = GetDC( browse );
   HGDIOBJ old = SelectObject( dc, x_guiFontOf( browse, dpi ) );
   TEXTMETRIC metrics;

   GetTextMetricsW( dc, &metrics );
   SelectObject( dc, old );
   ReleaseDC( browse, dc );

   return metrics.tmHeight + x_guiScale( 6, dpi );
}

/* The number of rows below the heading, at least one */
static int s_browseRowCount( HWND browse )
{
   RECT client;
   int height = s_browseRowHeight( browse );
   int rows;

   GetClientRect( browse, &client );
   rows = ( client.bottom - height ) / height;

   return IIF( rows > 1, rows, 1 );
}

/* A column is as wide as its characters in the font of the table, with a margin on both sides */
static int s_browseColumnWidth( HWND browse, const X_GUI_GRID *grid, size_t column )
{
   UINT dpi = GetDpiForWindow( browse );
   HDC dc = GetDC( browse );
   HGDIOBJ old = SelectObject( dc, x_guiFontOf( browse, dpi ) );
   TEXTMETRIC metrics;

   GetTextMetricsW( dc, &metrics );
   SelectObject( dc, old );
   ReleaseDC( browse, dc );

   return grid->columns[column].width * metrics.tmAveCharWidth + 2 * x_guiScale( 4, dpi );
}

/* The left side of a column in the whole width of the table */
static int s_browseColumnLeft( HWND browse, const X_GUI_GRID *grid, size_t column )
{
   int left = 0;
   size_t i;

   for( i = 0; i < column; ++i )
      left += s_browseColumnWidth( browse, grid, i );

   return left;
}

/* The width of all columns */
static int s_browseWidth( HWND browse, const X_GUI_GRID *grid )
{
   return s_browseColumnLeft( browse, grid, grid->columnCount );
}

/* The first columns frozen as TBrowse:freeze stand at the left and do not scroll */
static size_t s_browseFrozen( const X_GUI_GRID *grid )
{
   return IIF( ( size_t )grid->freeze > grid->columnCount, grid->columnCount, ( size_t )grid->freeze );
}

/* The width of the frozen columns */
static int s_browseFrozenWidth( HWND browse, const X_GUI_GRID *grid )
{
   return s_browseColumnLeft( browse, grid, s_browseFrozen( grid ) );
}

/* One call of the data blocks through the object; *ok is 0 when the call failed */
static int64_t s_browseCall( HWND browse, int operation, int64_t argument, int *ok )
{
   X_ITEM *object = ( X_ITEM * )GetWindowLongPtrW( browse, GWLP_USERDATA );
   X_ITEM result __attribute__( ( cleanup( x_itemClear ) ) ) = X_ITEM_NIL_INIT;
   int64_t values[2] = { operation, argument };

   *ok = *ok && object != NULL && x_guiNotifyEvent( object, X_GUI_EVENT_NAVIGATE, &result, 2, values );

   return IIF( *ok && X_IS_NUMERIC( &result ), x_itemGetNInt( &result ), 0 );
}

/* Tell the object of a browse that the record changed or was chosen */
static void s_browseEvent( HWND browse, int event )
{
   X_ITEM *object = ( X_ITEM * )GetWindowLongPtrW( browse, GWLP_USERDATA );
   X_ITEM result __attribute__( ( cleanup( x_itemClear ) ) ) = X_ITEM_NIL_INIT;
   int64_t values[1] = { event };

   if( object != NULL )
      x_guiNotifyEvent( object, X_GUI_EVENT_BROWSE, &result, 1, values );
}

/* The texts of the current record into row number row; the type of each value decides the alignment
   of a column without its own one */
static int s_browseRead( HWND browse, X_GUI_GRID *grid, int row )
{
   X_ITEM *object = ( X_ITEM * )GetWindowLongPtrW( browse, GWLP_USERDATA );
   X_ITEM result __attribute__( ( cleanup( x_itemClear ) ) ) = X_ITEM_NIL_INIT;
   size_t i, size;
   wchar_t **cell;
   const char *type, *value;

   if( object == NULL || !x_guiNotifyEvent( object, X_GUI_EVENT_ROW, &result, 0, NULL ) || !X_IS_ARRAY( &result ) )
      return 0;

   if( ( size_t )x_arrayLen( &result ) != grid->columnCount * 2 )
      return 0;

   for( i = 0; i < grid->columnCount; ++i )
   {
      cell = &grid->cells[( size_t )( row - 1 ) * grid->columnCount + i];
      x_xfree( *cell );
      value = x_arrayGetCPtr( &result, i * 2 + 1 );
      *cell = ( wchar_t * )x_utf8ToUtf16Alloc( value, x_arrayGetCLen( &result, i * 2 + 1 ), &size );
      type = x_arrayGetCPtr( &result, i * 2 + 2 );
      if( type != NULL && type[0] == 'L' )
         grid->columns[i].logical = 1;

      if( grid->columns[i].align >= 0 || type == NULL || type[0] == 'U' )
         continue;

      if( type[0] == 'N' )
         grid->columns[i].shown = 1;
      else if( type[0] == 'L' )
         grid->columns[i].shown = 2;
      else
         grid->columns[i].shown = 0;
   }

   return 1;
}

/* Set the scroll bars: the horizontal one by the columns, the vertical one by the position in the data */
static void s_browseScrollBars( HWND browse, X_GUI_GRID *grid )
{
   SCROLLINFO info;
   RECT client;
   int width = s_browseWidth( browse, grid );
   int frozen = s_browseFrozenWidth( browse, grid );

   GetClientRect( browse, &client );
   grid->offset = IIF( grid->offset > width - client.right, width - client.right, grid->offset );
   grid->offset = IIF( grid->offset < 0, 0, grid->offset );

   /* Only the columns behind the frozen ones scroll */
   memset( &info, 0, sizeof( info ) );
   info.cbSize = sizeof( info );
   info.fMask = SIF_RANGE | SIF_PAGE | SIF_POS | SIF_DISABLENOSCROLL;
   info.nMax = IIF( width - frozen > 0, width - frozen - 1, 0 );
   info.nPage = ( UINT )IIF( client.right > frozen, client.right - frozen, 0 );
   info.nPos = grid->offset;
   SetScrollInfo( browse, SB_HORZ, &info, TRUE );

   /* Without a position block only the top, the middle and the bottom of the data are known */
   info.nPage = 0;
   if( grid->position >= 0 )
   {
      info.nMax = 1000;
      info.nPos = grid->position;
   }
   else if( grid->hitTop && grid->hitBottom )
   {
      info.nMax = 0;
      info.nPos = 0;
   }
   else
   {
      info.nMax = 2;
      if( grid->hitTop )
         info.nPos = 0;
      else if( grid->hitBottom )
         info.nPos = 2;
      else
         info.nPos = 1;
   }

   SetScrollInfo( browse, SB_VERT, &info, TRUE );
}

/* Room for the texts of rows rows */
static void s_browseCells( X_GUI_GRID *grid, int rows )
{
   if( grid->cells != NULL && grid->cellRows == rows )
      return;

   s_gridCellsFree( grid );
   if( grid->columnCount == 0 )
      return;

   grid->cells = ( wchar_t ** )x_xgrabArray( ( size_t )rows * grid->columnCount, sizeof( wchar_t * ) );
   grid->cellRows = rows;
   memset( grid->cells, 0, ( size_t )rows * grid->columnCount * sizeof( wchar_t * ) );
}

/* Reads the rows of the screen around the current record: the cursor keeps its row unless there are
   not enough records below it, then the page ends with the last record; the current record is the
   cursor row again at the end */
static void s_browseRefill( HWND browse, X_GUI_GRID *grid )
{
   int rowCount = s_browseRowCount( browse );
   int ok = 1;
   int empty = 1;
   int want, below, above, target, total, filled = 0, extend, i;

   grid->valid = 1;
   s_browseCells( grid, rowCount );
   if( grid->columnCount == 0 || GetWindowLongPtrW( browse, GWLP_USERDATA ) == 0 )
   {
      grid->rows = 0;
      return;
   }

   /* emptyBlock gives .T. when there is no record: the table shows no row instead of the one past the end */
   if( s_browseCall( browse, X_GUI_EMPTY, 0, &empty ) != 0 )
   {
      grid->rows = 0;
      grid->rowPos = 1;
      grid->hitTop = 1;
      grid->hitBottom = 1;
      grid->position = -1;
      s_browseScrollBars( browse, grid );
      InvalidateRect( browse, NULL, FALSE );
      return;
   }

   if( grid->rowPos < 1 )
      grid->rowPos = 1;
   else if( grid->rowPos > rowCount )
      grid->rowPos = rowCount;

   /* One record more than needed in each direction tells whether the data ends there */
   want = rowCount - grid->rowPos;
   below = ( int )s_browseCall( browse, X_GUI_SKIP, want + 1, &ok );
   if( below > 0 )
      s_browseCall( browse, X_GUI_SKIP, -below, &ok );

   grid->hitBottom = below <= want;
   below = IIF( below > want, want, below );
   target = IIF( below >= want, grid->rowPos - 1, rowCount - 1 - below );
   above = -( int )s_browseCall( browse, X_GUI_SKIP, -( target + 1 ), &ok );
   grid->hitTop = above <= target;
   if( above > target )
   {
      s_browseCall( browse, X_GUI_SKIP, 1, &ok );
      above = target;
   }

   total = above + 1 + below;
   grid->rowPos = above + 1;
   /* At the top of the data the page goes on downwards as far as there are records */
   extend = grid->hitTop && !grid->hitBottom && total < rowCount;
   if( extend )
      total = rowCount;

   for( i = 1; ok && i <= total; ++i )
   {
      if( !s_browseRead( browse, grid, i ) )
         break;

      filled = i;
      if( i < total && s_browseCall( browse, X_GUI_SKIP, 1, &ok ) != 1 )
         break;
   }

   if( extend )
   {
      grid->hitBottom = filled < total || s_browseCall( browse, X_GUI_SKIP, 1, &ok ) != 1;
      if( !grid->hitBottom )
         s_browseCall( browse, X_GUI_SKIP, -1, &ok );
   }

   if( filled > grid->rowPos )
      s_browseCall( browse, X_GUI_SKIP, grid->rowPos - filled, &ok );

   grid->rows = filled;
   grid->rowPos = IIF( grid->rowPos > filled && filled > 0, filled, grid->rowPos );
   grid->position = ( int )s_browseCall( browse, X_GUI_POSITION, 0, &ok );
   if( !ok )
      grid->position = -1;

   s_browseScrollBars( browse, grid );
   InvalidateRect( browse, NULL, FALSE );
}

/* Moves the current record by delta and the cursor row with it when lFollow; a move off the data
   moves as far as it can */
static void s_browseMove( HWND browse, X_GUI_GRID *grid, int delta, int follow )
{
   int ok = 1;
   int moved;

   if( delta == 0 || grid->rows == 0 )
      return;

   moved = ( int )s_browseCall( browse, X_GUI_SKIP, delta, &ok );
   if( !ok || moved == 0 )
      return;

   if( follow )
      grid->rowPos += moved;

   s_browseRefill( browse, grid );
   s_browseEvent( browse, X_GUI_BROWSE_CHANGE );
}

/* Go to the first or, with bottom, to the last record */
static void s_browseEdge( HWND browse, X_GUI_GRID *grid, int bottom )
{
   int ok = 1;

   if( grid->rows == 0 )
      return;

   s_browseCall( browse, IIF( bottom, X_GUI_GO_BOTTOM, X_GUI_GO_TOP ), 0, &ok );
   if( !ok )
      return;

   grid->rowPos = IIF( bottom, s_browseRowCount( browse ), 1 );
   s_browseRefill( browse, grid );
   s_browseEvent( browse, X_GUI_BROWSE_CHANGE );
}

/* Scroll the columns behind the frozen ones to offset */
static void s_browseScroll( HWND browse, X_GUI_GRID *grid, int offset )
{
   grid->offset = offset;
   s_browseScrollBars( browse, grid );
   InvalidateRect( browse, NULL, FALSE );
}

/* The cursor goes to another column, as in TBrowse; the table scrolls so that the column is seen */
static void s_browseColumn( HWND browse, X_GUI_GRID *grid, int column )
{
   RECT client;
   int left, width, frozen, area;

   if( grid->columnCount == 0 )
      return;

   if( column < 0 )
      column = 0;
   else if( column >= ( int )grid->columnCount )
      column = ( int )grid->columnCount - 1;

   grid->colPos = column;
   /* A frozen column is always seen; another one is measured in the part of the table that scrolls */
   if( ( size_t )column < s_browseFrozen( grid ) )
   {
      InvalidateRect( browse, NULL, FALSE );
      return;
   }

   frozen = s_browseFrozenWidth( browse, grid );
   left = s_browseColumnLeft( browse, grid, ( size_t )column ) - frozen;
   width = s_browseColumnWidth( browse, grid, ( size_t )column );
   GetClientRect( browse, &client );
   area = client.right - frozen;
   if( left < grid->offset )
      s_browseScroll( browse, grid, left );
   else if( left + width > grid->offset + area )
      s_browseScroll( browse, grid, IIF( width > area, left, left + width - area ) );
   else
      InvalidateRect( browse, NULL, FALSE );
}

/* Space or a click changes the check box of the current record: the block of the column gets the opposite
   value, as the get/set block of TBColumn; column -1 is the first column with check boxes */
static void s_browseToggle( HWND browse, X_GUI_GRID *grid, int column )
{
   X_ITEM *object = ( X_ITEM * )GetWindowLongPtrW( browse, GWLP_USERDATA );
   X_ITEM result __attribute__( ( cleanup( x_itemClear ) ) ) = X_ITEM_NIL_INIT;
   int64_t values[1];

   if( grid->rows == 0 || object == NULL )
      return;

   if( column < 0 )
   {
      for( column = 0; ( size_t )column < grid->columnCount && !grid->columns[column].logical; ++column )
         ;
   }

   if( ( size_t )column >= grid->columnCount || !grid->columns[column].logical )
      return;

   values[0] = column + 1;
   x_guiNotifyEvent( object, X_GUI_EVENT_TOGGLE, &result, 1, values );
   s_browseRead( browse, grid, grid->rowPos );
   InvalidateRect( browse, NULL, FALSE );
}

/* Paint the heading, the rows and the cursor cell through a memory bitmap */
static void s_browsePaint( HWND browse, X_GUI_GRID *grid )
{
   PAINTSTRUCT state;
   HDC dc = BeginPaint( browse, &state );
   UINT dpi = GetDpiForWindow( browse );
   int height = s_browseRowHeight( browse );
   int padding = x_guiScale( 4, dpi );
   int focused = GetFocus() == browse;
   static const UINT formats[] = { DT_LEFT, DT_RIGHT, DT_CENTER };
   RECT client, area, text;
   HDC memory;
   HBITMAP bitmap;
   HGDIOBJ oldBitmap, oldFont;
   int row, x, width, cursor, cx, cy, back, fore;
   int cursorBack = grid->colors[IIF( focused, X_GUI_GRID_CURSORBACKGROUND, X_GUI_GRID_INACTIVECURSOR )];
   int *lefts, *widths;
   size_t column, order, frozen;
   const wchar_t *value;
   UINT format;

   GetClientRect( browse, &client );
   memory = CreateCompatibleDC( dc );
   cx = IIF( client.right > 0, client.right, 1 );
   cy = IIF( client.bottom > 0, client.bottom, 1 );
   bitmap = CreateCompatibleBitmap( dc, cx, cy );
   oldBitmap = SelectObject( memory, bitmap );
   oldFont = SelectObject( memory, x_guiFontOf( browse, dpi ) );
   SetBkMode( memory, TRANSPARENT );

   SetDCBrushColor( memory, x_guiColorRef( grid->colors[X_GUI_GRID_BACKGROUND] ) );
   FillRect( memory, &client, ( HBRUSH )GetStockObject( DC_BRUSH ) );

   area = ( RECT ){ 0, 0, client.right, height };
   SetDCBrushColor( memory, x_guiColorRef( grid->colors[X_GUI_GRID_HEADBACKGROUND] ) );
   FillRect( memory, &area, ( HBRUSH )GetStockObject( DC_BRUSH ) );
   SetTextColor( memory, x_guiColorRef( grid->colors[X_GUI_GRID_HEADFOREGROUND] ) );
   /* The places of the columns; the scrolling columns are drawn first and the frozen ones over them */
   frozen = s_browseFrozen( grid );
   lefts = ( int * )x_xgrabArray( grid->columnCount + 1, sizeof( int ) );
   widths = ( int * )x_xgrabArray( grid->columnCount + 1, sizeof( int ) );
   for( column = 0, x = 0; column < grid->columnCount; ++column, x += widths[column - 1] )
   {
      widths[column] = s_browseColumnWidth( browse, grid, column );
      lefts[column] = x - IIF( column < frozen, 0, grid->offset );
   }

   for( order = 0; order < grid->columnCount; ++order )
   {
      column = IIF( order < grid->columnCount - frozen, order + frozen, order - ( grid->columnCount - frozen ) );
      x = lefts[column];
      width = widths[column];
      format = formats[grid->columns[column].shown] | DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX | DT_END_ELLIPSIS;
      if( column < frozen )
      {
         area = ( RECT ){ x, 0, x + width, height };
         SetDCBrushColor( memory, x_guiColorRef( grid->colors[X_GUI_GRID_HEADBACKGROUND] ) );
         FillRect( memory, &area, ( HBRUSH )GetStockObject( DC_BRUSH ) );
      }

      text = ( RECT ){ x + padding, 0, x + width - padding, height };
      DrawTextW( memory, grid->columns[column].title, -1, &text, format );
      area = ( RECT ){ x + width - 1, 0, x + width, height };
      SetDCBrushColor( memory, x_guiColorRef( grid->colors[X_GUI_GRID_HEADLINE] ) );
      FillRect( memory, &area, ( HBRUSH )GetStockObject( DC_BRUSH ) );
   }

   for( row = 1; row <= grid->rows; ++row )
   {
      area = ( RECT ){ 0, height * row, client.right, height * ( row + 1 ) };
      back = grid->colors[IIF( row % 2 == 0, X_GUI_GRID_ALTERNATEBACKGROUND, X_GUI_GRID_BACKGROUND )];
      SetDCBrushColor( memory, x_guiColorRef( back ) );
      FillRect( memory, &area, ( HBRUSH )GetStockObject( DC_BRUSH ) );
      for( order = 0; order < grid->columnCount; ++order )
      {
         column = IIF( order < grid->columnCount - frozen, order + frozen, order - ( grid->columnCount - frozen ) );
         x = lefts[column];
         width = widths[column];
         format = formats[grid->columns[column].shown] | DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX | DT_END_ELLIPSIS;
         value = grid->cells[( size_t )( row - 1 ) * grid->columnCount + column];

         /* The cursor is one cell, the column colPos of the current record, as in TBrowse */
         cursor = row == grid->rowPos && ( int )column == grid->colPos;
         if( column < frozen && !cursor )
         {
            text = ( RECT ){ x, area.top, x + width, area.bottom };
            SetDCBrushColor( memory, x_guiColorRef( back ) );
            FillRect( memory, &text, ( HBRUSH )GetStockObject( DC_BRUSH ) );
         }

         if( cursor )
         {
            text = ( RECT ){ x, area.top, x + width, area.bottom };
            SetDCBrushColor( memory, x_guiColorRef( cursorBack ) );
            FillRect( memory, &text, ( HBRUSH )GetStockObject( DC_BRUSH ) );
         }

         fore = grid->colors[IIF( cursor && focused, X_GUI_GRID_CURSORFOREGROUND, X_GUI_GRID_FOREGROUND )];
         SetTextColor( memory, x_guiColorRef( fore ) );
         text = ( RECT ){ x + padding, area.top, x + width - padding, area.bottom };
         /* A logical cell shows the tick U+221A of the font of the table when it is true, nothing when false;
            the tick has its own color, on the cursor the color of the cursor text */
         if( grid->columns[column].logical )
         {
            if( !( cursor && focused ) )
               SetTextColor( memory, x_guiColorRef( grid->colors[X_GUI_GRID_CHECKFOREGROUND] ) );

            text = ( RECT ){ x, area.top, x + width, area.bottom };
            if( value != NULL && value[0] == L'T' )
               DrawTextW( memory, L"\x221A", 1, &text, DT_CENTER | DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX );
         }
         else if( value != NULL )
            DrawTextW( memory, value, -1, &text, format );

         text = ( RECT ){ x + width - 1, area.top, x + width, area.bottom };
         SetDCBrushColor( memory, x_guiColorRef( grid->colors[X_GUI_GRID_LINE] ) );
         FillRect( memory, &text, ( HBRUSH )GetStockObject( DC_BRUSH ) );
      }

      area.top = area.bottom - 1;
      SetDCBrushColor( memory, x_guiColorRef( grid->colors[X_GUI_GRID_LINE] ) );
      FillRect( memory, &area, ( HBRUSH )GetStockObject( DC_BRUSH ) );
   }

   x_xfree( lefts );
   x_xfree( widths );
   BitBlt( dc, 0, 0, client.right, client.bottom, memory, 0, 0, SRCCOPY );
   SelectObject( memory, oldFont );
   SelectObject( memory, oldBitmap );
   DeleteObject( bitmap );
   DeleteDC( memory );
   EndPaint( browse, &state );
}

/* The window procedure of browses: painting, keys, the mouse and the scroll bars */
static LRESULT CALLBACK s_browseProc( HWND browse, UINT message, WPARAM wParam, LPARAM lParam )
{
   X_GUI_GRID *grid = x_guiGridOf( browse );
   int rows;

   switch( message )
   {
   case WM_NCCREATE:
      grid = ( X_GUI_GRID * )x_xgrab( sizeof( X_GUI_GRID ) );
      memset( grid, 0, sizeof( *grid ) );
      grid->rowPos = 1;
      grid->position = -1;
      memcpy( grid->colors, s_gridColors, sizeof( grid->colors ) );
      SetPropW( browse, s_gridProperty, grid );
      break;

   case WM_NCDESTROY:
      s_gridRelease( browse );
      break;

   case WM_ERASEBKGND:
      return 1;

   case WM_PAINT:
      if( grid == NULL )
         break;

      if( !grid->valid )
         s_browseRefill( browse, grid );

      s_browsePaint( browse, grid );
      return 0;

   case WM_SIZE:
      if( grid != NULL )
      {
         grid->valid = 0;
         s_browseScrollBars( browse, grid );
         InvalidateRect( browse, NULL, FALSE );
      }
      return 0;

   case WM_SETFOCUS:
   case WM_KILLFOCUS:
      InvalidateRect( browse, NULL, FALSE );
      return 0;

   case WM_GETDLGCODE:
   {
      const MSG *key = ( const MSG * )lParam;

      if( key != NULL && key->message == WM_KEYDOWN && key->wParam == VK_RETURN )
         return DLGC_WANTMESSAGE;

      return DLGC_WANTARROWS;
   }

   case WM_KEYDOWN:
      if( grid == NULL )
         break;

      rows = s_browseRowCount( browse );
      switch( wParam )
      {
      case VK_UP:
         s_browseMove( browse, grid, -1, 1 );
         return 0;
      case VK_DOWN:
         s_browseMove( browse, grid, 1, 1 );
         return 0;
      case VK_PRIOR:
         s_browseMove( browse, grid, -IIF( rows > 1, rows - 1, 1 ), 0 );
         return 0;
      case VK_NEXT:
         s_browseMove( browse, grid, IIF( rows > 1, rows - 1, 1 ), 0 );
         return 0;
      case VK_HOME:
         if( GetKeyState( VK_CONTROL ) < 0 )
            s_browseEdge( browse, grid, 0 );
         else
            s_browseColumn( browse, grid, 0 );
         return 0;
      case VK_END:
         if( GetKeyState( VK_CONTROL ) < 0 )
            s_browseEdge( browse, grid, 1 );
         else
            s_browseColumn( browse, grid, ( int )grid->columnCount - 1 );
         return 0;
      case VK_LEFT:
         s_browseColumn( browse, grid, grid->colPos - 1 );
         return 0;
      case VK_RIGHT:
         s_browseColumn( browse, grid, grid->colPos + 1 );
         return 0;
      case VK_RETURN:
         if( grid->rows > 0 )
            s_browseEvent( browse, X_GUI_BROWSE_CHOOSE );
         return 0;
      case VK_SPACE:
         s_browseToggle( browse, grid, -1 );
         return 0;
      }
      break;

   case WM_LBUTTONDOWN:
   case WM_LBUTTONDBLCLK:
      SetFocus( browse );
      if( grid != NULL )
      {
         int row = ( short )HIWORD( lParam ) / s_browseRowHeight( browse );

         if( row >= 1 && row <= grid->rows )
         {
            /* A click left of the frozen width hits a frozen column, otherwise a scrolled one */
            int x = ( short )LOWORD( lParam );
            size_t frozen = s_browseFrozen( grid );
            int inside = frozen > 0 && x < s_browseFrozenWidth( browse, grid );
            size_t last = IIF( inside, frozen, grid->columnCount );
            size_t column = IIF( inside, 0, frozen );

            x += IIF( inside, 0, grid->offset );
            for( ; column + 1 < last && x >= s_browseColumnLeft( browse, grid, column + 1 ); ++column )
               ;

            column = IIF( column >= grid->columnCount, grid->columnCount - 1, column );
            s_browseColumn( browse, grid, ( int )column );
            if( row != grid->rowPos )
            {
               int ok = 1;
               int moved = ( int )s_browseCall( browse, X_GUI_SKIP, row - grid->rowPos, &ok );

               if( ok && moved != 0 )
               {
                  grid->rowPos += moved;
                  InvalidateRect( browse, NULL, FALSE );
                  s_browseEvent( browse, X_GUI_BROWSE_CHANGE );
               }
            }

            if( grid->columns[column].logical )
               s_browseToggle( browse, grid, ( int )column );

            if( message == WM_LBUTTONDBLCLK )
               s_browseEvent( browse, X_GUI_BROWSE_CHOOSE );
         }
      }
      return 0;

   case WM_MOUSEWHEEL:
      if( grid != NULL )
         s_browseMove( browse, grid, IIF( GET_WHEEL_DELTA_WPARAM( wParam ) > 0, -3, 3 ), 0 );
      return 0;

   case WM_VSCROLL:
      if( grid == NULL )
         break;

      rows = s_browseRowCount( browse );
      switch( LOWORD( wParam ) )
      {
      case SB_LINEUP:
         s_browseMove( browse, grid, -1, 1 );
         break;
      case SB_LINEDOWN:
         s_browseMove( browse, grid, 1, 1 );
         break;
      case SB_PAGEUP:
         s_browseMove( browse, grid, -IIF( rows > 1, rows - 1, 1 ), 0 );
         break;
      case SB_PAGEDOWN:
         s_browseMove( browse, grid, IIF( rows > 1, rows - 1, 1 ), 0 );
         break;
      case SB_TOP:
         s_browseEdge( browse, grid, 0 );
         break;
      case SB_BOTTOM:
         s_browseEdge( browse, grid, 1 );
         break;
      case SB_THUMBPOSITION:
      {
         SCROLLINFO info = { sizeof( info ), SIF_RANGE | SIF_TRACKPOS, 0, 0, 0, 0, 0 };

         GetScrollInfo( browse, SB_VERT, &info );
         if( info.nTrackPos <= info.nMin )
            s_browseEdge( browse, grid, 0 );
         else if( info.nTrackPos >= info.nMax )
            s_browseEdge( browse, grid, 1 );
         else
            s_browseScrollBars( browse, grid );
         break;
      }
      }
      return 0;

   case WM_HSCROLL:
      if( grid == NULL )
         break;

      switch( LOWORD( wParam ) )
      {
      case SB_LINELEFT:
         s_browseScroll( browse, grid, grid->offset - x_guiScale( 20, GetDpiForWindow( browse ) ) );
         break;
      case SB_LINERIGHT:
         s_browseScroll( browse, grid, grid->offset + x_guiScale( 20, GetDpiForWindow( browse ) ) );
         break;
      case SB_PAGELEFT:
      case SB_PAGERIGHT:
      {
         RECT client;
         int page;

         GetClientRect( browse, &client );
         page = IIF( LOWORD( wParam ) == SB_PAGELEFT, -client.right, client.right );
         s_browseScroll( browse, grid, grid->offset + page );
         break;
      }
      case SB_THUMBTRACK:
      case SB_THUMBPOSITION:
      {
         SCROLLINFO info = { sizeof( info ), SIF_TRACKPOS, 0, 0, 0, 0, 0 };

         GetScrollInfo( browse, SB_HORZ, &info );
         s_browseScroll( browse, grid, info.nTrackPos );
         break;
      }
      }
      return 0;
   }

   return DefWindowProcW( browse, message, wParam, lParam );
}

/* Register the window class of browses once */
BOOL x_guiRegisterBrowseClass( HINSTANCE instance )
{
   WNDCLASSEXW browseClass;

   if( s_browseClass != 0 )
      return TRUE;

   memset( &browseClass, 0, sizeof( browseClass ) );
   browseClass.cbSize = sizeof( browseClass );
   browseClass.style = CS_DBLCLKS | CS_HREDRAW | CS_VREDRAW;
   browseClass.lpfnWndProc = s_browseProc;
   browseClass.hInstance = instance;
   browseClass.hCursor = LoadCursorW( NULL, IDC_ARROW );
   browseClass.lpszClassName = x_guiBrowseClassName;

   s_browseClass = RegisterClassExW( &browseClass );

   return s_browseClass != 0;
}

/* The number of characters of a UTF-8 text */
static int s_utf8Length( const char *text, size_t length )
{
   int count = 0;
   size_t i;

   for( i = 0; i < length; ++i )
      count += ( ( unsigned char )text[i] & 0xC0 ) != 0x80;

   return count;
}

/* The width of a column in characters, as TBColumn: the longer of the title and the value, whose length
   is the length of the field, of the picture or of the type */
int x_guiBrowseTextWidth( const X_ITEM *title, const X_ITEM *value, const char *picture )
{
   const char *space = IIF( picture != NULL && picture[0] == '@', strchr( picture, ' ' ), NULL );
   const char *mask = "";
   int width = 10;
   int titleWidth = 0;

   if( space != NULL )
      mask = space + 1;
   else if( picture != NULL && picture[0] != '@' )
      mask = picture;

   if( mask[0] != '\0' )
      width = s_utf8Length( mask, strlen( mask ) );
   else if( value != NULL && X_IS_STRING( value ) )
      width = s_utf8Length( x_itemGetCPtr( value ), x_itemGetCLen( value ) );
   else if( value != NULL && X_IS_LOGICAL( value ) )
      width = 2;

   if( title != NULL && X_IS_STRING( title ) )
      titleWidth = s_utf8Length( x_itemGetCPtr( title ), x_itemGetCLen( title ) );

   if( titleWidth > width )
      width = titleWidth;

   return IIF( width > 0, width, 1 );
}

/* Add a column; returns its number, 0 with an error raised */
int x_guiBrowseAddColumn( X_CALL_CTX *ctx, HWND browse, const X_ITEM *title, int width, int align )
{
   X_GUI_GRID *grid = x_guiGridOf( browse );
   wchar_t *text;

   if( grid == NULL || width <= 0 || align < -1 || align > 2 )
   {
      x_guiRaise( ctx, "GUIBROWSE:ADDCOLUMN" );
      return 0;
   }

   text = x_guiText( ctx, title, "GUIBROWSE:ADDCOLUMN" );
   if( text == NULL )
      return 0;

   s_gridCellsFree( grid );
   grid->columns = ( X_GUI_COLUMN * )x_xreallocArray( grid->columns, grid->columnCount + 1, sizeof( X_GUI_COLUMN ) );
   grid->columns[grid->columnCount++] = ( X_GUI_COLUMN ){ text, width, align, IIF( align < 0, 0, align ), 0 };
   grid->valid = 0;
   s_browseScrollBars( browse, grid );
   InvalidateRect( browse, NULL, FALSE );

   return ( int )grid->columnCount;
}

/* Return a color of a browse */
void x_guiBrowseColor( X_CALL_CTX *ctx, X_GUI_GRID_COLOR part )
{
   HWND browse = x_guiSelfHandle( ctx, "GUIBROWSE:COLOR" );
   const X_GUI_GRID *grid = IIF( browse != NULL, x_guiGridOf( browse ), NULL );

   if( grid != NULL )
      x_retni( ctx, grid->colors[part] );
}

/* Set a color of a browse */
void x_guiBrowseSetColor( X_CALL_CTX *ctx, X_GUI_GRID_COLOR part )
{
   HWND browse = x_guiSelfHandle( ctx, "GUIBROWSE:COLOR" );
   X_GUI_GRID *grid = IIF( browse != NULL, x_guiGridOf( browse ), NULL );

   if( grid == NULL )
      return;

   if( !X_ISNUM( ctx, 1 ) || x_parnint( ctx, 1 ) < 0 || x_parnint( ctx, 1 ) > 0xFFFFFF )
   {
      x_guiRaise( ctx, "GUIBROWSE:COLOR" );
      return;
   }

   grid->colors[part] = ( int )x_parnint( ctx, 1 );
   InvalidateRect( browse, NULL, FALSE );
   x_itemReturn( ctx, x_paritem( ctx, 1 ) );
}

/* The column nColumn of logical values gets the opposite value of the current record through its block */
void x_guiBrowseToggle( X_CALL_CTX *ctx, const X_ITEM *columns, int number )
{
   X_ITEM value __attribute__( ( cleanup( x_itemClear ) ) ) = X_ITEM_NIL_INIT;
   X_ITEM block __attribute__( ( cleanup( x_itemClear ) ) ) = X_ITEM_NIL_INIT;
   X_ITEM result __attribute__( ( cleanup( x_itemClear ) ) ) = X_ITEM_NIL_INIT;
   const X_ITEM *column = NULL;
   X_ARG args[1];

   if( columns != NULL && X_IS_ARRAY( columns ) && number >= 1 && number <= ( int )x_arrayLen( columns ) )
      column = x_arrayGetItemPtr( columns, ( size_t )number );

   if( column == NULL || !X_IS_ARRAY( column ) || !x_arrayGet( column, 1, &block ) || !X_IS_BLOCK( &block ) )
      return;

   x_evalBlock( ctx, &value, &block, 0, NULL );
   if( !X_IS_LOGICAL( &value ) )
      return;

   x_itemPutL( &value, !value.u.logical );
   args[0] = ( X_ARG ){ &value, 0 };
   x_evalBlock( ctx, &result, &block, 1, args );
}

/* Return the number of frozen columns */
void x_guiBrowseFreeze( X_CALL_CTX *ctx )
{
   HWND browse = x_guiSelfHandle( ctx, "GUIBROWSE:FREEZE" );
   const X_GUI_GRID *grid = IIF( browse != NULL, x_guiGridOf( browse ), NULL );

   if( grid != NULL )
      x_retni( ctx, grid->freeze );
}

/* Set the number of frozen columns, as TBrowse:freeze */
void x_guiBrowseSetFreeze( X_CALL_CTX *ctx )
{
   HWND browse = x_guiSelfHandle( ctx, "GUIBROWSE:FREEZE" );
   X_GUI_GRID *grid = IIF( browse != NULL, x_guiGridOf( browse ), NULL );

   if( grid == NULL )
      return;

   if( !X_ISNUM( ctx, 1 ) || x_parni( ctx, 1 ) < 0 )
   {
      x_guiRaise( ctx, "GUIBROWSE:FREEZE" );
      return;
   }

   grid->freeze = x_parni( ctx, 1 );
   grid->offset = 0;
   s_browseScrollBars( browse, grid );
   InvalidateRect( browse, NULL, FALSE );
   x_itemReturn( ctx, x_paritem( ctx, 1 ) );
}

/* Return the column of the cursor, from 1 */
void x_guiBrowseColPos( X_CALL_CTX *ctx )
{
   HWND browse = x_guiSelfHandle( ctx, "GUIBROWSE:COLPOS" );
   const X_GUI_GRID *grid = IIF( browse != NULL, x_guiGridOf( browse ), NULL );

   if( grid != NULL )
      x_retni( ctx, grid->colPos + 1 );
}

/* Move the cursor to a column, as TBrowse:colPos */
void x_guiBrowseSetColPos( X_CALL_CTX *ctx )
{
   HWND browse = x_guiSelfHandle( ctx, "GUIBROWSE:COLPOS" );
   X_GUI_GRID *grid = IIF( browse != NULL, x_guiGridOf( browse ), NULL );

   if( grid == NULL )
      return;

   if( !X_ISNUM( ctx, 1 ) || x_parni( ctx, 1 ) < 1 || ( size_t )x_parni( ctx, 1 ) > grid->columnCount )
   {
      x_guiRaise( ctx, "GUIBROWSE:COLPOS" );
      return;
   }

   s_browseColumn( browse, grid, x_parni( ctx, 1 ) - 1 );
   x_itemReturn( ctx, x_paritem( ctx, 1 ) );
}

/* Read the rows again around the current record */
void x_guiBrowseRefresh( HWND browse )
{
   X_GUI_GRID *grid = x_guiGridOf( browse );

   if( grid == NULL )
      return;

   x_callbackEnterDll();
   s_browseRefill( browse, grid );
   x_callbackLeaveDll();
   x_callbackRaisePending();
}

/* The blocks of the browse for X_GUI_EVENT_NAVIGATE( nOperation, nArgument ) */
void x_guiBrowseNavigate( X_CALL_CTX *ctx, const X_ITEM *goTop, const X_ITEM *goBottom, const X_ITEM *skip,
                          const X_ITEM *position, const X_ITEM *empty )
{
   X_ITEM block __attribute__( ( cleanup( x_itemClear ) ) ) = X_ITEM_NIL_INIT;
   X_ITEM result __attribute__( ( cleanup( x_itemClear ) ) ) = X_ITEM_NIL_INIT;
   X_ITEM count __attribute__( ( cleanup( x_itemClear ) ) ) = X_ITEM_NIL_INIT;
   const X_ITEM *chosen;
   X_ARG args[1];
   double part;

   switch( x_parni( ctx, 1 ) )
   {
   case X_GUI_GO_TOP:
      chosen = goTop;
      break;
   case X_GUI_GO_BOTTOM:
      chosen = goBottom;
      break;
   case X_GUI_SKIP:
      chosen = skip;
      break;
   case X_GUI_POSITION:
      chosen = position;
      break;
   case X_GUI_EMPTY:
      chosen = empty;
      break;
   default:
      chosen = NULL;
   }

   x_retni( ctx, IIF( x_parni( ctx, 1 ) == X_GUI_POSITION, -1, 0 ) );
   if( chosen == NULL || !X_IS_BLOCK( chosen ) )
      return;

   x_itemCopy( &block, chosen );
   x_itemPutNInt( &count, x_parnint( ctx, 2 ) );
   args[0] = ( X_ARG ){ &count, 0 };
   x_evalBlock( ctx, &result, &block, IIF( x_parni( ctx, 1 ) == X_GUI_SKIP, 1, 0 ), args );
   if( x_parni( ctx, 1 ) == X_GUI_SKIP && X_IS_NUMERIC( &result ) )
      x_retnint( ctx, x_itemGetNInt( &result ) );
   else if( x_parni( ctx, 1 ) == X_GUI_POSITION && X_IS_NUMERIC( &result ) )
   {
      part = x_itemGetND( &result );
      if( part < 0 )
         part = 0;
      else if( part > 1 )
         part = 1;

      x_retni( ctx, ( int )( part * 1000 + 0.5 ) );
   }
   else if( x_parni( ctx, 1 ) == X_GUI_EMPTY && X_IS_LOGICAL( &result ) && result.u.logical )
      x_retni( ctx, 1 );
}

/* X_GUI_EVENT_ROW of a browse: for every column { bBlock, cPicture } the text of its value and its type */
void x_guiBrowseRow( X_CALL_CTX *ctx, const X_ITEM *columns )
{
   X_ITEM value __attribute__( ( cleanup( x_itemClear ) ) ) = X_ITEM_NIL_INIT;
   X_ITEM shown __attribute__( ( cleanup( x_itemClear ) ) ) = X_ITEM_NIL_INIT;
   X_ITEM block __attribute__( ( cleanup( x_itemClear ) ) ) = X_ITEM_NIL_INIT;
   const X_ITEM *column;
   const char *picture, *text;
   size_t count = IIF( columns != NULL && X_IS_ARRAY( columns ), ( size_t )x_arrayLen( columns ), 0 );
   size_t i, length;
   char type[2] = "U";

   if( !x_arrayNew( ctx->result, ( int64_t )count * 2 ) )
      return;

   for( i = 1; i <= count; ++i )
   {
      column = x_arrayGetItemPtr( columns, i );
      x_itemClear( &value );
      x_itemClear( &shown );
      type[0] = 'U';
      text = "";
      length = 0;
      if( column != NULL && X_IS_ARRAY( column ) && x_arrayGet( column, 1, &block ) && X_IS_BLOCK( &block ) )
      {
         x_evalBlock( ctx, &value, &block, 0, NULL );
         if( X_IS_STRING( &value ) || X_IS_NUMERIC( &value ) || X_IS_DATE( &value ) || X_IS_LOGICAL( &value ) )
         {
            if( X_IS_STRING( &value ) )
               type[0] = 'C';
            else if( X_IS_NUMERIC( &value ) )
               type[0] = 'N';
            else if( X_IS_DATE( &value ) )
               type[0] = 'D';
            else
               type[0] = 'L';

            picture = x_arrayGetCPtr( column, 2 );
            picture = IIF( picture != NULL, picture, "" );
            x_transform( &shown, &value, picture, strlen( picture ) );
            text = x_itemGetCPtr( &shown );
            length = x_itemGetCLen( &shown );
            /* A number stands on the right without the spaces of its width, a text without the spaces
               of its field */
            while( type[0] == 'N' && length > 0 && *text == ' ' )
            {
               ++text;
               --length;
            }

            while( length > 0 && text[length - 1] == ' ' )
               --length;
         }
      }

      x_itemPutCL( x_arrayGetItemPtr( ctx->result, i * 2 - 1 ), text, length );
      x_itemPutC( x_arrayGetItemPtr( ctx->result, i * 2 ), type );
   }
}
