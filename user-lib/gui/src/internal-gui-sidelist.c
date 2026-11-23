/*
 * Copyright (c) 2024-2026 xDev-Language
 *
 * Library...: xDev GUI
 * File......: internal-gui-sidelist.c
 *
 */

#include <string.h>
#include "internal-gui.h"

/* Sizes of a list of rows at 96 DPI: the height of a row, the room around its parts, the box of its picture, the
   gap between the picture and the text, the square of the button of the settings and the size of its sign */
enum
{
   X_GUI_SIDELIST_ROW = 72,
   X_GUI_SIDELIST_PAD = 8,
   X_GUI_SIDELIST_PICTURE = 42,
   X_GUI_SIDELIST_GAP = 10,
   X_GUI_SIDELIST_BUTTON = 22,
   X_GUI_SIDELIST_SIGN = 14
};

/* Shades of the text color over the background in 255 parts: the row under the mouse, the row clicked last, the
   button of the settings under the mouse and the text under the title */
enum
{
   X_GUI_SIDELIST_HOT = 24,
   X_GUI_SIDELIST_CHOSEN = 40,
   X_GUI_SIDELIST_PRESS = 64,
   X_GUI_SIDELIST_DIM = 160
};

/* ---------------------------------------------------------------------------------------------
   Rows
   --------------------------------------------------------------------------------------------- */

/* The place of row index in the panel, moved up by the scrolled distance */
static void s_listRow( HWND panel, const X_GUI_SIDELIST *list, int index, RECT *row )
{
   int height = x_guiScale( X_GUI_SIDELIST_ROW, GetDpiForWindow( panel ) );
   RECT client;

   GetClientRect( panel, &client );
   *row = ( RECT ){ 0, index * height - list->scroll, client.right, ( index + 1 ) * height - list->scroll };
}

/* The button of the settings at the bottom right of a row */
static void s_listButton( const RECT *row, UINT dpi, RECT *button )
{
   int size = x_guiScale( X_GUI_SIDELIST_BUTTON, dpi );
   int pad = x_guiScale( X_GUI_SIDELIST_PAD, dpi );

   *button = ( RECT ){ row->right - pad - size, row->bottom - pad - size, row->right - pad, row->bottom - pad };
}

/* The row at the point of the panel and its part, 1 the row and 2 the button of the settings; -1 for none */
static int s_listHit( HWND panel, const X_GUI_SIDELIST *list, POINT point, int *part )
{
   int height = x_guiScale( X_GUI_SIDELIST_ROW, GetDpiForWindow( panel ) );
   int index = ( point.y + list->scroll ) / height;
   RECT row, button;

   *part = 1;
   if( point.y < 0 || index >= ( int )list->count )
      return -1;

   s_listRow( panel, list, index, &row );
   s_listButton( &row, GetDpiForWindow( panel ), &button );
   if( list->rows[index].settings && PtInRect( &button, point ) )
      *part = 2;

   return index;
}

/* The list scrolled to the distance, kept between the top and the end of the list; the scroll bar follows it */
static void s_listScroll( HWND panel, X_GUI_SIDELIST *list, int scroll )
{
   int total = ( int )list->count * x_guiScale( X_GUI_SIDELIST_ROW, GetDpiForWindow( panel ) );
   SCROLLINFO info = { sizeof( info ), SIF_RANGE | SIF_PAGE | SIF_POS, 0, 0, 0, 0, 0 };
   RECT client;
   int most;

   GetClientRect( panel, &client );
   most = IIF( total > client.bottom, total - client.bottom, 0 );
   scroll = IIF( scroll > most, most, scroll );
   list->scroll = IIF( scroll < 0, 0, scroll );
   info.nMax = total - 1;
   info.nPage = ( UINT )client.bottom;
   info.nPos = list->scroll;
   SetScrollInfo( panel, SB_VERT, &info, TRUE );
   InvalidateRect( panel, NULL, FALSE );
}

/* ---------------------------------------------------------------------------------------------
   Drawing
   --------------------------------------------------------------------------------------------- */

/* A font of the size of the font of the sidebar: semibold for the titles, the font of signs for the settings */
static HFONT s_listFont( UINT dpi, int sign )
{
   LOGFONTW face;

   GetObjectW( x_guiFont( dpi ), sizeof( face ), &face );
   face.lfWeight = FW_SEMIBOLD;
   if( sign )
   {
      face.lfHeight = -x_guiScale( X_GUI_SIDELIST_SIGN, dpi );
      face.lfWeight = FW_NORMAL;
      lstrcpynW( face.lfFaceName, L"Segoe MDL2 Assets", LF_FACESIZE );
   }

   return CreateFontIndirectW( &face );
}

/* Draw one row: its field, the picture in its box, the title, the text under it, the line at the bottom and the
   button of the settings; fonts holds the font of the text, of the titles and of the sign */
static void s_listDrawRow( HDC dc, const X_GUI_SIDEROW *item, const RECT *row, int shade, int press, HFONT *fonts,
                           UINT dpi )
{
   HBRUSH brush = ( HBRUSH )GetStockObject( DC_BRUSH );
   UINT flags = DT_SINGLELINE | DT_LEFT | DT_TOP | DT_NOPREFIX | DT_END_ELLIPSIS;
   int pad = x_guiScale( X_GUI_SIDELIST_PAD, dpi );
   int box = x_guiScale( X_GUI_SIDELIST_PICTURE, dpi );
   int width = 0, height = 0;
   COLORREF back = GetBkColor( dc );
   COLORREF fore = GetTextColor( dc );
   TEXTMETRICW metrics;
   RECT part, button;

   if( shade > 0 )
   {
      SetDCBrushColor( dc, x_guiMix( back, fore, shade ) );
      FillRect( dc, row, brush );
   }

   if( item->image != NULL && ImageList_GetIconSize( item->image, &width, &height ) )
   {
      int x = row->left + pad + ( box - width ) / 2;
      int y = row->top + ( row->bottom - row->top - height ) / 2;

      ImageList_DrawEx( item->image, 0, dc, x, y, 0, 0, CLR_NONE, CLR_NONE, ILD_TRANSPARENT );
   }

   s_listButton( row, dpi, &button );
   part = ( RECT ){ row->left + pad + box + x_guiScale( X_GUI_SIDELIST_GAP, dpi ), 0, row->right - pad, 0 };
   part.top = row->top + pad;
   part.bottom = row->bottom;
   SelectObject( dc, fonts[1] );
   GetTextMetricsW( dc, &metrics );
   DrawTextW( dc, item->title, -1, &part, flags );

   SelectObject( dc, fonts[0] );
   SetTextColor( dc, x_guiMix( back, fore, X_GUI_SIDELIST_DIM ) );
   part.top += metrics.tmHeight + x_guiScale( 2, dpi );
   if( item->text != NULL )
      DrawTextW( dc, item->text, -1, &part, flags );

   part.top = button.top;
   part.bottom = button.bottom;
   part.right = IIF( item->settings, button.left - pad, part.right );
   if( item->info != NULL )
      DrawTextW( dc, item->info, -1, &part, flags | DT_VCENTER );

   if( item->settings )
   {
      if( press )
      {
         SetDCBrushColor( dc, x_guiMix( back, fore, X_GUI_SIDELIST_PRESS ) );
         FillRect( dc, &button, brush );
      }

      SelectObject( dc, fonts[2] );
      DrawTextW( dc, L"\xE713", -1, &button, DT_SINGLELINE | DT_CENTER | DT_VCENTER | DT_NOPREFIX );
   }

   SetTextColor( dc, fore );
}

/* The panel paints its rows one under another on the colors of the sidebar */
static void s_listPaint( HWND panel, HWND sidebar, const X_GUI_SIDELIST *list )
{
   PAINTSTRUCT state;
   HDC dc = BeginPaint( panel, &state );
   UINT dpi = GetDpiForWindow( panel );
   HBRUSH brush = ( HBRUSH )GetStockObject( DC_BRUSH );
   HFONT fonts[3] = { x_guiFont( dpi ), s_listFont( dpi, 0 ), s_listFont( dpi, 1 ) };
   HGDIOBJ old = SelectObject( dc, fonts[0] );
   COLORREF back, fore;
   RECT client, row;
   int shade, i;

   x_guiSidebarColors( sidebar, &back, &fore );
   GetClientRect( panel, &client );
   SetDCBrushColor( dc, back );
   FillRect( dc, &client, brush );
   SetBkMode( dc, TRANSPARENT );
   SetBkColor( dc, back );
   SetTextColor( dc, fore );
   for( i = 0; i < ( int )list->count; ++i )
   {
      s_listRow( panel, list, i, &row );
      shade = IIF( i == list->chosen, X_GUI_SIDELIST_CHOSEN, 0 );
      shade = IIF( shade == 0 && i == list->hot, X_GUI_SIDELIST_HOT, shade );
      s_listDrawRow( dc, &list->rows[i], &row, shade, i == list->hot && list->part == 2, fonts, dpi );
   }

   SelectObject( dc, old );
   DeleteObject( fonts[1] );
   DeleteObject( fonts[2] );
   EndPaint( panel, &state );
}

/* ---------------------------------------------------------------------------------------------
   Mouse and scrolling
   --------------------------------------------------------------------------------------------- */

/* The row under the mouse and its part get their field */
static void s_listMove( HWND panel, X_GUI_SIDELIST *list, POINT point )
{
   TRACKMOUSEEVENT track = { sizeof( track ), TME_LEAVE, panel, 0 };
   int part, hot;

   if( !list->tracking )
   {
      TrackMouseEvent( &track );
      list->tracking = 1;
   }

   hot = s_listHit( panel, list, point, &part );
   if( hot == list->hot && part == list->part )
      return;

   list->hot = hot;
   list->part = part;
   InvalidateRect( panel, NULL, FALSE );
}

/* A click on a row: the row becomes the chosen one and the sidebar runs its block, of the button of the settings
   when the click is on it */
static void s_listClick( HWND panel, HWND sidebar, X_GUI_SIDELIST *list, int page, POINT point )
{
   X_ITEM *object = ( X_ITEM * )GetWindowLongPtrW( sidebar, GWLP_USERDATA );
   X_ITEM result __attribute__( ( cleanup( x_itemClear ) ) ) = X_ITEM_NIL_INIT;
   int64_t values[3] = { page, 0, 0 };
   int part, index = s_listHit( panel, list, point, &part );

   if( index < 0 )
      return;

   if( part == 1 )
      list->chosen = index;

   InvalidateRect( panel, NULL, FALSE );
   values[1] = index + 1;
   values[2] = part;
   x_guiNotifyEvent( object, X_GUI_EVENT_ITEM, &result, 3, values );
}

/* The scroll bar moved: by a line, by a page or to the place of its box */
static void s_listScrollBar( HWND panel, X_GUI_SIDELIST *list, int request )
{
   int line = x_guiScale( X_GUI_SIDELIST_ROW, GetDpiForWindow( panel ) ) / 3;
   SCROLLINFO info = { sizeof( info ), SIF_ALL, 0, 0, 0, 0, 0 };
   int scroll = list->scroll;

   GetScrollInfo( panel, SB_VERT, &info );
   switch( request )
   {
   case SB_LINEUP:
      scroll -= line;
      break;

   case SB_LINEDOWN:
      scroll += line;
      break;

   case SB_PAGEUP:
      scroll -= ( int )info.nPage;
      break;

   case SB_PAGEDOWN:
      scroll += ( int )info.nPage;
      break;

   case SB_THUMBTRACK:
   case SB_THUMBPOSITION:
      scroll = info.nTrackPos;
      break;
   }

   s_listScroll( panel, list, scroll );
}

/* The panel paints its rows, follows the mouse over them, runs the clicks and scrolls; id is the number of the page
   and data the sidebar */
static LRESULT CALLBACK s_listProc( HWND panel, UINT message, WPARAM wParam, LPARAM lParam, UINT_PTR id,
                                    DWORD_PTR data )
{
   X_GUI_SIDELIST *list = x_guiSidebarOf( ( HWND )data )->pages[id - 1].list;
   POINT point = { ( short )LOWORD( lParam ), ( short )HIWORD( lParam ) };
   int height;

   switch( message )
   {
   case WM_ERASEBKGND:
      return 1;

   case WM_PAINT:
      s_listPaint( panel, ( HWND )data, list );
      return 0;

   case WM_SIZE:
      s_listScroll( panel, list, list->scroll );
      break;

   case WM_MOUSEMOVE:
      s_listMove( panel, list, point );
      return 0;

   case WM_MOUSELEAVE:
      list->tracking = 0;
      list->hot = -1;
      InvalidateRect( panel, NULL, FALSE );
      return 0;

   case WM_LBUTTONUP:
      s_listClick( panel, ( HWND )data, list, ( int )id, point );
      return 0;

   case WM_MOUSEWHEEL:
      height = x_guiScale( X_GUI_SIDELIST_ROW, GetDpiForWindow( panel ) );
      s_listScroll( panel, list, list->scroll - GET_WHEEL_DELTA_WPARAM( wParam ) * height / WHEEL_DELTA );
      return 0;

   case WM_VSCROLL:
      s_listScrollBar( panel, list, LOWORD( wParam ) );
      return 0;

   case WM_NCDESTROY:
      RemoveWindowSubclass( panel, s_listProc, id );
      break;
   }

   return DefSubclassProc( panel, message, wParam, lParam );
}

/* ---------------------------------------------------------------------------------------------
   Lists of rows in a panel of a sidebar
   --------------------------------------------------------------------------------------------- */

/* A text argument as a wide string, NULL for NIL */
static wchar_t *s_listText( X_CALL_CTX *ctx, int argument )
{
   if( X_ISNIL( ctx, argument ) )
      return NULL;

   return x_guiText( ctx, x_paritem( ctx, argument ), "GUISIDEBAR:ADDITEM" );
}

/* Add a row to the list of page nPage, the first argument, with the arguments of oSidebar:AddItem(); the panel
   gets the list at its first row; returns the number of the row, 0 with an error raised */
int x_guiSidebarAddItem( X_CALL_CTX *ctx, HWND sidebar, int page )
{
   X_GUI_SIDEBAR *side = x_guiSidebarOf( sidebar );
   X_GUI_SIDEPAGE *owner;
   X_GUI_SIDELIST *list;
   X_GUI_SIDEROW row;
   HBITMAP picture;

   if( page < 1 || ( size_t )page > side->count )
   {
      x_guiRaise( ctx, "GUISIDEBAR:ADDITEM" );
      return 0;
   }

   picture = x_guiPicture( ctx, x_paritem( ctx, 2 ), "GUISIDEBAR:ADDITEM" );
   if( picture == NULL )
      return 0;

   row.image = x_guiSidePicture( picture );
   row.title = s_listText( ctx, 3 );
   row.text = s_listText( ctx, 4 );
   row.info = s_listText( ctx, 5 );
   row.settings = !X_ISNIL( ctx, 7 );
   DeleteObject( picture );

   owner = &side->pages[page - 1];
   if( owner->list == NULL )
   {
      owner->list = ( X_GUI_SIDELIST * )x_xgrab( sizeof( X_GUI_SIDELIST ) );
      *owner->list = ( X_GUI_SIDELIST ){ NULL, 0, -1, 1, -1, 0, 0 };
      SetWindowSubclass( owner->panel, s_listProc, ( UINT_PTR )page, ( DWORD_PTR )sidebar );
   }

   list = owner->list;
   list->rows = ( X_GUI_SIDEROW * )x_xrealloc( list->rows, ( list->count + 1 ) * sizeof( X_GUI_SIDEROW ) );
   list->rows[list->count] = row;
   ++list->count;
   x_callbackEnterDll();
   s_listScroll( owner->panel, list, list->scroll );
   x_callbackLeaveDll();
   x_callbackRaisePending();

   return ( int )list->count;
}

/* Release the rows of a list; the list stays without rows */
static void s_listEmpty( X_GUI_SIDELIST *list )
{
   size_t i;

   for( i = 0; i < list->count; ++i )
   {
      if( list->rows[i].image != NULL )
         ImageList_Destroy( list->rows[i].image );

      x_xfree( list->rows[i].title );
      x_xfree( list->rows[i].text );
      x_xfree( list->rows[i].info );
   }

   x_xfree( list->rows );
   *list = ( X_GUI_SIDELIST ){ NULL, 0, -1, 1, -1, 0, list->tracking };
}

/* Remove the rows of the list of page nPage, e.g. before it is filled again; 0 when there is no such page */
int x_guiSidebarClearItems( HWND sidebar, int page )
{
   X_GUI_SIDEBAR *side = x_guiSidebarOf( sidebar );
   X_GUI_SIDEPAGE *owner;

   if( page < 1 || ( size_t )page > side->count )
      return 0;

   owner = &side->pages[page - 1];
   if( owner->list == NULL )
      return 1;

   s_listEmpty( owner->list );
   x_callbackEnterDll();
   s_listScroll( owner->panel, owner->list, 0 );
   x_callbackLeaveDll();
   x_callbackRaisePending();

   return 1;
}

/* Release the rows of a list and the list */
void x_guiSideListRelease( X_GUI_SIDELIST *list )
{
   if( list == NULL )
      return;

   s_listEmpty( list );
   x_xfree( list );
}
