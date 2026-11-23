/*
 * Copyright (c) 2024-2026 xDev-Language
 *
 * Library...: xDev GUI
 * File......: internal-gui-sidebar.c
 *
 */

#include <string.h>
#include "internal-gui.h"

/* Colors of a sidebar as 0xRRGGBB: the strip of pictures without barBackground and the panel without background,
   the text of the panel without foreground and the line of one pixel at the right edge of the strip */
enum
{
   X_GUI_SIDEBAR_BAR = 0x212A30,
   X_GUI_SIDEBAR_TEXT = 0xCCCCCC,
   X_GUI_SIDEBAR_BORDER = 0x41484D
};

/* ---------------------------------------------------------------------------------------------
   Sidebars
   --------------------------------------------------------------------------------------------- */

/* A color between two colors: amount of 255 parts of the second one */
COLORREF x_guiMix( COLORREF first, COLORREF second, int amount )
{
   int red = GetRValue( first ) + ( GetRValue( second ) - GetRValue( first ) ) * amount / 255;
   int green = GetGValue( first ) + ( GetGValue( second ) - GetGValue( first ) ) * amount / 255;
   int blue = GetBValue( first ) + ( GetBValue( second ) - GetBValue( first ) ) * amount / 255;

   return RGB( red, green, blue );
}

/* The panel and its title have the colors of the sidebar window, otherwise those of the system */
void x_guiSidebarColors( HWND sidebar, COLORREF *back, COLORREF *fore )
{
   const X_GUI_PAINT *paint = x_guiPaintOf( sidebar );

   *back = GetSysColor( COLOR_BTNFACE );
   *fore = GetSysColor( COLOR_BTNTEXT );
   if( paint != NULL && paint->brush != NULL )
      *back = x_guiColorRef( paint->colors[X_GUI_COLOR_BACKGROUND] );

   if( paint != NULL && paint->colors[X_GUI_COLOR_FOREGROUND] >= 0 )
      *fore = x_guiColorRef( paint->colors[X_GUI_COLOR_FOREGROUND] );
}

/* The strip of pictures has the color barBackground, otherwise X_GUI_SIDEBAR_BAR */
static COLORREF s_sidebarStripColor( const X_GUI_SIDEBAR *side )
{
   int bar = side->colors[X_GUI_SIDEBAR_COLOR_BAR];

   return x_guiColorRef( IIF( bar >= 0, bar, X_GUI_SIDEBAR_BAR ) );
}

/* Whether a window is one of the bars of the sidebar */
static int s_sidebarBar( const X_GUI_SIDEBAR *side, HWND window )
{
   size_t i;

   if( window == side->activity || window == side->buttons )
      return 1;

   for( i = 0; i < side->count; ++i )
   {
      if( window == side->pages[i].actions )
         return 1;
   }

   return 0;
}

/* A bar of picture buttons without text, placed by the sidebar; the bar of pages and the bar of buttons have their
   pictures one under another, the bar of actions of a page stays hidden until the page is shown */
static HWND s_sidebarToolbar( HWND sidebar, int vertical )
{
   DWORD style = WS_CHILD | WS_CLIPSIBLINGS | TBSTYLE_FLAT | TBSTYLE_TOOLTIPS;
   HINSTANCE instance = GetModuleHandleW( NULL );
   HWND toolbar;

   style |= CCS_NOPARENTALIGN | CCS_NORESIZE | CCS_NODIVIDER;
   style |= IIF( vertical, ( DWORD )( WS_VISIBLE | CCS_VERT | TBSTYLE_WRAPABLE ), 0 );
   x_guiInitCommonControls();
   toolbar = CreateWindowExW( 0, TOOLBARCLASSNAMEW, NULL, style, 0, 0, 0, 0, sidebar, NULL, instance, NULL );
   if( toolbar == NULL )
      return NULL;

   x_guiToolbarInit( toolbar );
   x_guiToolbarOf( toolbar )->side = X_GUI_SIDE_NONE;
   SendMessageW( toolbar, TB_SETMAXTEXTROWS, 0, 0 );
   SendMessageW( toolbar, TB_SETPADDING, 0, 0 );

   return toolbar;
}

/* The buttons of a bar of the sidebar are squares of size pixels at 96 DPI, larger for larger pictures; returns
   their size in pixels */
static int s_sidebarButtons( HWND toolbar, int size )
{
   const X_GUI_BAR *bar = x_guiToolbarOf( toolbar );
   UINT dpi = GetDpiForWindow( toolbar );
   int length = x_guiScale( size, dpi );
   int width = 0, height = 0, picture;

   if( bar->images != NULL )
      ImageList_GetIconSize( bar->images, &width, &height );

   picture = IIF( width > height, width, height ) + x_guiScale( 6, dpi );
   length = IIF( picture > length, picture, length );
   SendMessageW( toolbar, TB_SETBUTTONWIDTH, 0, MAKELPARAM( length, length ) );
   SendMessageW( toolbar, TB_SETBUTTONSIZE, 0, MAKELPARAM( length, length ) );

   return length;
}

/* Place the parts of the sidebar: the bar of pages at the top of the strip, the bar of buttons at its bottom, the
   bar of actions at the right of the header and the panel of the shown page under the header, from the top when
   the page has no actions; hidden panels get the same place, so the anchors of their controls count from it */
static void s_sidebarLayout( HWND sidebar )
{
   const X_GUI_SIDEBAR *side = x_guiSidebarOf( sidebar );
   UINT dpi = GetDpiForWindow( sidebar );
   UINT flags = SWP_NOZORDER | SWP_NOACTIVATE;
   int strip = x_guiScale( X_GUI_SIDEBAR_STRIP, dpi );
   int width = x_guiScale( side->width, dpi );
   int size, count, top, x, y, height, header;
   RECT client;
   size_t i;

   GetClientRect( sidebar, &client );
   size = s_sidebarButtons( side->buttons, X_GUI_SIDEBAR_STRIP );
   count = ( int )SendMessageW( side->buttons, TB_BUTTONCOUNT, 0, 0 );
   top = IIF( client.bottom > size * count, client.bottom - size * count, 0 );
   SetWindowPos( side->buttons, NULL, 0, top, strip - 1, size * count, flags );
   s_sidebarButtons( side->activity, X_GUI_SIDEBAR_STRIP );
   SetWindowPos( side->activity, NULL, 0, 0, strip - 1, top, flags );

   for( i = 0; i < side->count; ++i )
   {
      const X_GUI_SIDEPAGE *page = &side->pages[i];
      UINT show = IIF( ( int )i + 1 == side->page, SWP_SHOWWINDOW, SWP_HIDEWINDOW );

      size = s_sidebarButtons( page->actions, X_GUI_SIDEBAR_ACTION );
      count = ( int )SendMessageW( page->actions, TB_BUTTONCOUNT, 0, 0 );
      header = IIF( count > 0, x_guiScale( X_GUI_SIDEBAR_HEADER, dpi ), 0 );
      height = IIF( client.bottom > header, client.bottom - header, 0 );
      x = strip + width - x_guiScale( X_GUI_SIDEBAR_GAP, dpi ) - size * count;
      y = ( header - size ) / 2;
      SetWindowPos( page->actions, NULL, x, y, size * count, size, flags | show );
      SetWindowPos( page->panel, NULL, strip, header, width, height, flags | show );
   }

   InvalidateRect( sidebar, NULL, TRUE );
   InvalidateRect( side->activity, NULL, TRUE );
}

/* The sidebar takes its new width and the panels filling the window follow it */
static void s_sidebarResize( HWND sidebar )
{
   HWND parent = GetParent( sidebar );

   x_guiSidebarPlace( sidebar );
   s_sidebarLayout( sidebar );
   EnumChildWindows( parent, x_guiLayoutPanel, ( LPARAM )parent );
}

/* Show the panel of a page, page 0 hides the panel; the focus leaves a hidden panel for the window */
static void s_sidebarShow( HWND sidebar, X_GUI_SIDEBAR *side, int page )
{
   HWND focus = GetFocus();

   side->page = page;
   if( page > 0 )
      side->last = page;

   s_sidebarResize( sidebar );
   if( focus != NULL && IsChild( sidebar, focus ) && !IsWindowVisible( focus ) )
      SetFocus( GetParent( sidebar ) );
}

/* The header of the shown page: its title at the left, up to its actions, and a line at its bottom; a page without
   actions has no header */
static void s_sidebarHeader( HDC dc, const X_GUI_SIDEBAR *side, COLORREF back, COLORREF fore, UINT dpi )
{
   const X_GUI_SIDEPAGE *page = &side->pages[side->page - 1];
   int count = ( int )SendMessageW( page->actions, TB_BUTTONCOUNT, 0, 0 );
   int strip = x_guiScale( X_GUI_SIDEBAR_STRIP, dpi );
   int header = x_guiScale( X_GUI_SIDEBAR_HEADER, dpi );
   HBRUSH brush = ( HBRUSH )GetStockObject( DC_BRUSH );
   HGDIOBJ old;
   RECT frame, part;

   if( count == 0 )
      return;

   part = ( RECT ){ strip, header - 1, strip + x_guiScale( side->width, dpi ) - 1, header };
   SetDCBrushColor( dc, x_guiMix( back, fore, 48 ) );
   FillRect( dc, &part, brush );

   GetWindowRect( page->actions, &frame );
   MapWindowPoints( NULL, GetParent( page->actions ), ( POINT * )&frame, 2 );
   part = ( RECT ){ strip + x_guiScale( X_GUI_SIDEBAR_TITLE, dpi ), 0, frame.left, header - 1 };
   old = SelectObject( dc, x_guiFont( dpi ) );
   SetBkMode( dc, TRANSPARENT );
   SetTextColor( dc, fore );
   DrawTextW( dc, page->title, -1, &part, DT_SINGLELINE | DT_LEFT | DT_VCENTER | DT_NOPREFIX | DT_END_ELLIPSIS );
   SelectObject( dc, old );
}

/* The sidebar paints its strip with the line of one pixel at its right edge, the title of the shown page and the
   line at the edge of the shown panel */
static void s_sidebarPaint( HWND sidebar, const X_GUI_SIDEBAR *side )
{
   PAINTSTRUCT state;
   HDC dc = BeginPaint( sidebar, &state );
   UINT dpi = GetDpiForWindow( sidebar );
   HBRUSH brush = ( HBRUSH )GetStockObject( DC_BRUSH );
   int strip = x_guiScale( X_GUI_SIDEBAR_STRIP, dpi );
   COLORREF back, fore;
   RECT client, part;

   x_guiSidebarColors( sidebar, &back, &fore );
   GetClientRect( sidebar, &client );
   part = ( RECT ){ 0, 0, strip, client.bottom };
   SetDCBrushColor( dc, s_sidebarStripColor( side ) );
   FillRect( dc, &part, brush );
   part = ( RECT ){ strip - 1, 0, strip, client.bottom };
   SetDCBrushColor( dc, x_guiColorRef( X_GUI_SIDEBAR_BORDER ) );
   FillRect( dc, &part, brush );
   if( side->page > 0 )
   {
      part = ( RECT ){ client.right - 1, 0, client.right, client.bottom };
      SetDCBrushColor( dc, x_guiMix( back, fore, 48 ) );
      FillRect( dc, &part, brush );
      s_sidebarHeader( dc, side, back, fore, dpi );
   }

   EndPaint( sidebar, &state );
}

/* The count of a page on its picture: a circle of the accent color at the bottom right with the white number,
   99+ above 99 */
static void s_sidebarBadge( HDC dc, const RECT *button, int count, COLORREF color, UINT dpi )
{
   int size = x_guiScale( 16, dpi );
   int right = button->right - x_guiScale( 6, dpi );
   int bottom = button->bottom - x_guiScale( 6, dpi );
   HGDIOBJ oldFont, oldBrush, oldPen;
   wchar_t text[8] = L"99+";
   LOGFONTW face;
   HFONT font;
   SIZE extent;
   RECT box;
   int width;

   if( count <= 99 )
      wsprintfW( text, L"%d", count );

   memset( &face, 0, sizeof( face ) );
   face.lfHeight = -x_guiScale( 9, dpi );
   face.lfWeight = FW_SEMIBOLD;
   lstrcpynW( face.lfFaceName, L"Segoe UI", LF_FACESIZE );
   font = CreateFontIndirectW( &face );
   oldFont = SelectObject( dc, font );
   GetTextExtentPoint32W( dc, text, lstrlenW( text ), &extent );
   width = extent.cx + x_guiScale( 8, dpi );
   width = IIF( width < size, size, width );
   box = ( RECT ){ right - width, bottom - size, right, bottom };

   oldBrush = SelectObject( dc, GetStockObject( DC_BRUSH ) );
   oldPen = SelectObject( dc, GetStockObject( NULL_PEN ) );
   SetDCBrushColor( dc, color );
   RoundRect( dc, box.left, box.top, box.right + 1, box.bottom + 1, size, size );
   SetBkMode( dc, TRANSPARENT );
   SetTextColor( dc, RGB( 255, 255, 255 ) );
   DrawTextW( dc, text, -1, &box, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX );

   SelectObject( dc, oldPen );
   SelectObject( dc, oldBrush );
   SelectObject( dc, oldFont );
   DeleteObject( font );
}

/* The picture of a button from a gui_Icon: its character drawn as text with ClearType right on the button, as code
   editors draw their icons; a pale picture has its color mixed with the background */
static void s_sidebarGlyph( HDC dc, const X_GUI_GLYPH *glyph, const RECT *button, COLORREF back, int pale, UINT dpi )
{
   COLORREF ink = IIF( glyph->color >= 0, x_guiColorRef( glyph->color ), GetSysColor( COLOR_BTNTEXT ) );
   RECT part = *button;
   wchar_t text[2];
   int length = x_guiGlyphText( glyph->code, text );
   LOGFONTW face;
   HGDIOBJ old;
   HFONT font;

   memset( &face, 0, sizeof( face ) );
   face.lfHeight = -x_guiScale( glyph->size, dpi );
   face.lfWeight = FW_NORMAL;
   face.lfCharSet = DEFAULT_CHARSET;
   face.lfOutPrecision = OUT_TT_ONLY_PRECIS;
   face.lfQuality = CLEARTYPE_QUALITY;
   lstrcpynW( face.lfFaceName, glyph->family, LF_FACESIZE );
   font = CreateFontIndirectW( &face );
   old = SelectObject( dc, font );
   SetBkMode( dc, TRANSPARENT );
   SetTextColor( dc, IIF( pale, x_guiMix( back, ink, X_GUI_SIDEBAR_PALE ), ink ) );
   DrawTextW( dc, text, length, &part, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX );
   SelectObject( dc, old );
   DeleteObject( font );
}

/* The buttons of the bars of the sidebar are pictures on the color of their bar; the shown page has a mark at the
   left edge of its picture, the other pages and the buttons are paler until the mouse comes over them; an action
   under the mouse lies on a field a shade towards the text */
static LRESULT s_sidebarDraw( HWND sidebar, const X_GUI_SIDEBAR *side, NMTBCUSTOMDRAW *draw )
{
   HWND toolbar = draw->nmcd.hdr.hwndFrom;
   HDC dc = draw->nmcd.hdc;
   const X_GUI_BAR *bar = x_guiToolbarOf( toolbar );
   UINT dpi = GetDpiForWindow( toolbar );
   HBRUSH brush = ( HBRUSH )GetStockObject( DC_BRUSH );
   int strip = toolbar == side->activity || toolbar == side->buttons;
   int hot = ( draw->nmcd.uItemState & ( CDIS_HOT | CDIS_SELECTED ) ) != 0;
   int number = ( int )draw->nmcd.dwItemSpec;
   int shown = toolbar == side->activity && number == side->page;
   int accent = side->colors[X_GUI_SIDEBAR_COLOR_ACCENT];
   int width = 0, height = 0, badge = 0;
   IMAGELISTDRAWPARAMS image;
   TBBUTTONINFOW info;
   COLORREF back, fore, mark;
   RECT face;

   mark = IIF( accent >= 0, x_guiColorRef( accent ), GetSysColor( COLOR_HIGHLIGHT ) );
   if( toolbar == side->activity && number >= 1 && ( size_t )number <= side->count )
      badge = side->pages[number - 1].badge;

   x_guiSidebarColors( sidebar, &back, &fore );
   if( strip )
      back = s_sidebarStripColor( side );

   if( draw->nmcd.dwDrawStage == CDDS_PREPAINT )
   {
      SetDCBrushColor( dc, back );
      FillRect( dc, &draw->nmcd.rc, brush );
      return CDRF_NOTIFYITEMDRAW;
   }

   if( draw->nmcd.dwDrawStage != CDDS_ITEMPREPAINT )
      return CDRF_DODEFAULT;

   face = draw->nmcd.rc;
   SetDCBrushColor( dc, back );
   FillRect( dc, &face, brush );
   if( hot && !strip )
   {
      InflateRect( &face, -x_guiScale( 1, dpi ), -x_guiScale( 1, dpi ) );
      SetDCBrushColor( dc, x_guiMix( back, fore, 40 ) );
      FillRect( dc, &face, brush );
   }

   if( shown )
   {
      face = draw->nmcd.rc;
      face.right = face.left + x_guiScale( X_GUI_SIDEBAR_MARK, dpi );
      SetDCBrushColor( dc, mark );
      FillRect( dc, &face, brush );
   }

   if( number >= 1 && ( size_t )number <= bar->count && bar->glyphs[number - 1].family[0] != L'\0' )
   {
      s_sidebarGlyph( dc, &bar->glyphs[number - 1], &draw->nmcd.rc, back, strip && !shown && !hot, dpi );
      if( badge > 0 )
         s_sidebarBadge( dc, &draw->nmcd.rc, badge, mark, dpi );

      return CDRF_SKIPDEFAULT;
   }

   memset( &info, 0, sizeof( info ) );
   info.cbSize = sizeof( info );
   info.dwMask = TBIF_IMAGE;
   info.iImage = I_IMAGENONE;
   SendMessageW( toolbar, TB_GETBUTTONINFOW, draw->nmcd.dwItemSpec, ( LPARAM )&info );
   if( bar->images == NULL || info.iImage < 0 )
      return CDRF_SKIPDEFAULT;

   ImageList_GetIconSize( bar->images, &width, &height );
   memset( &image, 0, sizeof( image ) );
   image.cbSize = sizeof( image );
   image.himl = bar->images;
   image.i = info.iImage;
   image.hdcDst = dc;
   image.x = ( draw->nmcd.rc.left + draw->nmcd.rc.right - width ) / 2;
   image.y = ( draw->nmcd.rc.top + draw->nmcd.rc.bottom - height ) / 2;
   image.rgbBk = CLR_NONE;
   image.rgbFg = CLR_NONE;
   image.fStyle = ILD_TRANSPARENT;
   image.fState = ILS_ALPHA;
   image.Frame = ( DWORD )IIF( strip && !shown && !hot, X_GUI_SIDEBAR_PALE, 255 );
   ImageList_DrawIndirect( &image );
   if( badge > 0 )
      s_sidebarBadge( dc, &draw->nmcd.rc, badge, mark, dpi );

   return CDRF_SKIPDEFAULT;
}

/* A click on a bar of the sidebar: a page shows its panel and the shown page hides it, both run OnChange; a button
   and an action run their blocks; 0 when the bar is not one of the sidebar */
static int s_sidebarCommand( HWND sidebar, X_GUI_SIDEBAR *side, HWND toolbar, int button )
{
   X_ITEM *object = ( X_ITEM * )GetWindowLongPtrW( sidebar, GWLP_USERDATA );
   X_ITEM result __attribute__( ( cleanup( x_itemClear ) ) ) = X_ITEM_NIL_INIT;
   int64_t values[2] = { 0, button };
   size_t i;

   if( !s_sidebarBar( side, toolbar ) )
      return 0;

   if( toolbar == side->activity )
   {
      s_sidebarShow( sidebar, side, IIF( button == side->page, 0, button ) );
      if( side->page > 0 )
         SetFocus( side->pages[side->page - 1].panel );

      x_guiNotifyEvent( object, X_GUI_EVENT_CHANGE, &result, 0, NULL );
      return 1;
   }

   for( i = 0; i < side->count; ++i )
   {
      if( toolbar == side->pages[i].actions )
         values[0] = ( int64_t )i + 1;
   }

   x_guiNotifyEvent( object, X_GUI_EVENT_COMMAND, &result, 2, values );

   return 1;
}

/* Whether the point x of the sidebar lies on the edge of the shown panel, where the mouse changes its width */
static int s_sidebarOnEdge( HWND sidebar, const X_GUI_SIDEBAR *side, int x )
{
   UINT dpi = GetDpiForWindow( sidebar );

   return side->page > 0 && x >= x_guiScale( X_GUI_SIDEBAR_STRIP, dpi ) + x_guiScale( side->width, dpi );
}

/* The edge dragged to x: the panel follows the mouse down to no width, the sidebar reaches at most half of the
   window */
static void s_sidebarDrag( HWND sidebar, X_GUI_SIDEBAR *side, int x )
{
   UINT dpi = GetDpiForWindow( sidebar );
   int strip = x_guiScale( X_GUI_SIDEBAR_STRIP, dpi );
   int width = x - strip - x_guiScale( X_GUI_SIDEBAR_EDGE, dpi ) / 2;
   int most;
   RECT client;

   GetClientRect( GetParent( sidebar ), &client );
   most = client.right / 2 - strip;
   width = IIF( width > most, most, width );
   width = IIF( width < 0, 0, width );
   width = MulDiv( width, USER_DEFAULT_SCREEN_DPI, ( int )dpi );
   if( width == side->width )
      return;

   side->width = width;
   s_sidebarResize( sidebar );
}

/* The edge let go: a panel narrower than the smallest width takes back its width from before the drag and hides
   as a click on the picture of the page */
static void s_sidebarDrop( HWND sidebar, X_GUI_SIDEBAR *side )
{
   side->drag = 0;
   if( side->width >= X_GUI_SIDEBAR_MINWIDTH )
      return;

   side->width = side->before;
   SendMessageW( sidebar, WM_COMMAND, MAKEWPARAM( side->page, BN_CLICKED ), ( LPARAM )side->activity );
}

/* The menu of the bar of pages at the right click, position -1 is the keyboard: a page with a tick has its picture
   on the bar and a click on a page hides or shows its picture; the last picture stays; hiding the picture of the
   shown page hides the panel as a click on it */
static void s_sidebarMenu( HWND sidebar, X_GUI_SIDEBAR *side, LPARAM position )
{
   POINT point = { ( short )LOWORD( position ), ( short )HIWORD( position ) };
   HMENU menu = CreatePopupMenu();
   int visible = 0, hidden, choice;
   UINT flags;
   RECT frame;
   size_t i;

   for( i = 0; i < side->count; ++i )
      visible += !SendMessageW( side->activity, TB_ISBUTTONHIDDEN, ( WPARAM )( i + 1 ), 0 );

   for( i = 0; i < side->count; ++i )
   {
      hidden = ( int )SendMessageW( side->activity, TB_ISBUTTONHIDDEN, ( WPARAM )( i + 1 ), 0 );
      flags = MF_STRING | IIF( hidden, MF_UNCHECKED, MF_CHECKED ) | IIF( !hidden && visible == 1, MF_GRAYED, 0 );
      x_guiMenuAppend( menu, flags, i + 1, side->pages[i].title );
   }

   if( position == -1 )
   {
      GetWindowRect( side->activity, &frame );
      point = ( POINT ){ frame.left, frame.top };
   }

   flags = TPM_RETURNCMD | TPM_NONOTIFY | TPM_RIGHTBUTTON;
   choice = ( int )TrackPopupMenuEx( menu, flags, point.x, point.y, sidebar, NULL );
   DestroyMenu( menu );
   if( choice == 0 )
      return;

   hidden = ( int )SendMessageW( side->activity, TB_ISBUTTONHIDDEN, ( WPARAM )choice, 0 );
   SendMessageW( side->activity, TB_HIDEBUTTON, ( WPARAM )choice, MAKELPARAM( !hidden, 0 ) );
   if( !hidden && choice == side->page )
      SendMessageW( sidebar, WM_COMMAND, MAKEWPARAM( choice, BN_CLICKED ), ( LPARAM )side->activity );

   s_sidebarLayout( sidebar );
}

/* The mouse moved with the picture pressed: past the drag distance the picture follows the mouse to the place of the
   picture under it; returns 1 while it moves */
static int s_sidebarMove( HWND toolbar, X_GUI_SIDEBAR *side, WPARAM keys, POINT point )
{
   int distance = point.y - side->start.y;
   int target;

   distance = IIF( distance < 0, -distance, distance );
   if( !side->moving && side->grab >= 0 && ( keys & MK_LBUTTON ) && distance >= GetSystemMetrics( SM_CYDRAG ) )
   {
      ReleaseCapture();
      side->moving = 1;
      SetCapture( toolbar );
   }

   if( !side->moving )
      return 0;

   target = ( int )SendMessageW( toolbar, TB_HITTEST, 0, ( LPARAM )&point );
   if( target >= 0 && target != side->grab )
   {
      SendMessageW( toolbar, TB_MOVEBUTTON, ( WPARAM )side->grab, ( LPARAM )target );
      side->grab = target;
      InvalidateRect( toolbar, NULL, TRUE );
   }

   return 1;
}

/* The bar of pages lets the mouse drag a picture to another place; the picture let go there shows no page */
static LRESULT CALLBACK s_sidebarActivityProc( HWND toolbar, UINT message, WPARAM wParam, LPARAM lParam,
                                               UINT_PTR id, DWORD_PTR data )
{
   X_GUI_SIDEBAR *side = x_guiSidebarOf( ( HWND )data );
   POINT point = { ( short )LOWORD( lParam ), ( short )HIWORD( lParam ) };

   switch( message )
   {
   case WM_LBUTTONDOWN:
      side->grab = ( int )SendMessageW( toolbar, TB_HITTEST, 0, ( LPARAM )&point );
      side->start = point;
      break;

   case WM_MOUSEMOVE:
      if( s_sidebarMove( toolbar, side, wParam, point ) )
         return 0;

      break;

   case WM_LBUTTONUP:
      side->grab = -1;
      if( side->moving )
      {
         side->moving = 0;
         ReleaseCapture();
         return 0;
      }
      break;

   case WM_CAPTURECHANGED:
      if( side->moving && ( HWND )lParam != toolbar )
      {
         side->moving = 0;
         side->grab = -1;
      }
      break;

   case WM_NCDESTROY:
      RemoveWindowSubclass( toolbar, s_sidebarActivityProc, id );
      break;
   }

   return DefSubclassProc( toolbar, message, wParam, lParam );
}

/* Release the pages of a sidebar with their sections and rows; its bars and panels are released as windows */
static void s_sidebarRelease( HWND sidebar )
{
   X_GUI_SIDEBAR *side = ( X_GUI_SIDEBAR * )RemovePropW( sidebar, x_guiSidebarProperty );
   size_t i, j;

   for( i = 0; i < side->count; ++i )
   {
      for( j = 0; j < side->pages[i].sectionCount; ++j )
         x_xfree( side->pages[i].sections[j].title );

      x_xfree( side->pages[i].sections );
      x_xfree( side->pages[i].title );
      x_guiSideListRelease( side->pages[i].list );
   }

   x_xfree( side->pages );
   x_xfree( side );
}

/* The sidebar paints itself, places its parts, runs the clicks on its bars and lets the mouse drag its edge */
static LRESULT CALLBACK s_sidebarProc( HWND sidebar, UINT message, WPARAM wParam, LPARAM lParam, UINT_PTR id,
                                       DWORD_PTR data )
{
   X_GUI_SIDEBAR *side = x_guiSidebarOf( sidebar );
   const NMHDR *header = ( const NMHDR * )lParam;
   POINT point;

   UNUSED( data );
   switch( message )
   {
   case WM_PAINT:
      s_sidebarPaint( sidebar, side );
      return 0;

   case WM_SIZE:
      s_sidebarLayout( sidebar );
      break;

   case WM_NOTIFY:
      if( header->code == NM_CUSTOMDRAW && s_sidebarBar( side, header->hwndFrom ) )
         return s_sidebarDraw( sidebar, side, ( NMTBCUSTOMDRAW * )lParam );

      break;

   case WM_COMMAND:
      if( lParam != 0 && s_sidebarCommand( sidebar, side, ( HWND )lParam, LOWORD( wParam ) ) )
         return 0;

      break;

   case WM_CONTEXTMENU:
      if( ( HWND )wParam == side->activity )
      {
         s_sidebarMenu( sidebar, side, lParam );
         return 0;
      }
      break;

   case WM_SETCURSOR:
      GetCursorPos( &point );
      ScreenToClient( sidebar, &point );
      if( ( HWND )wParam == sidebar && LOWORD( lParam ) == HTCLIENT && s_sidebarOnEdge( sidebar, side, point.x ) )
      {
         SetCursor( LoadCursorW( NULL, IDC_SIZEWE ) );
         return TRUE;
      }
      break;

   case WM_LBUTTONDOWN:
      if( s_sidebarOnEdge( sidebar, side, ( short )LOWORD( lParam ) ) )
      {
         side->drag = 1;
         side->before = side->width;
         SetCapture( sidebar );
         return 0;
      }
      break;

   case WM_MOUSEMOVE:
      if( side->drag )
      {
         s_sidebarDrag( sidebar, side, ( short )LOWORD( lParam ) );
         return 0;
      }
      break;

   case WM_LBUTTONUP:
      if( side->drag )
      {
         ReleaseCapture();
         return 0;
      }
      break;

   case WM_CAPTURECHANGED:
      if( side->drag )
         s_sidebarDrop( sidebar, side );

      break;

   case WM_NCDESTROY:
      s_sidebarRelease( sidebar );
      RemoveWindowSubclass( sidebar, s_sidebarProc, id );
      break;
   }

   return DefSubclassProc( sidebar, message, wParam, lParam );
}

/* gui_Sidebar():New( oWindow ): a sidebar without pages at the left edge of the window; the panels filling the
   window make room for it */
void x_guiSidebarNew( X_CALL_CTX *ctx )
{
   const X_ITEM *owner = x_paritem( ctx, 1 );
   HWND parent = x_guiObjectWindow( ctx, owner, "GUISIDEBAR:NEW" );
   HWND sidebar, activity, buttons;
   X_GUI_SIDEBAR *side;
   X_GUI_PAINT *paint;
   DWORD error;

   if( parent == NULL )
      return;

   x_guiWindowSend( ctx, ctx->self, owner, NULL, 1, 1, X_GUI_WINDOW_PANEL, 0 );
   sidebar = x_guiObjectHandle( ctx, ctx->self );
   if( sidebar == NULL )
      return;

   x_callbackEnterDll();
   activity = s_sidebarToolbar( sidebar, 1 );
   buttons = IIF( activity != NULL, s_sidebarToolbar( sidebar, 1 ), NULL );
   error = GetLastError();
   if( buttons == NULL )
   {
      x_guiDestroyWindow( sidebar );
      x_callbackLeaveDll();
      x_callbackRaisePending();
      x_guiRaiseWin32( ctx, "CreateWindowExW", error );
      return;
   }

   side = ( X_GUI_SIDEBAR * )x_xgrab( sizeof( X_GUI_SIDEBAR ) );
   memset( side, 0, sizeof( *side ) );
   side->activity = activity;
   side->buttons = buttons;
   side->width = X_GUI_SIDEBAR_WIDTH;
   side->colors[X_GUI_SIDEBAR_COLOR_BAR] = -1;
   side->colors[X_GUI_SIDEBAR_COLOR_ACCENT] = -1;
   side->grab = -1;
   SetPropW( sidebar, x_guiSidebarProperty, side );
   SetWindowSubclass( activity, s_sidebarActivityProc, 1, ( DWORD_PTR )sidebar );

   /* The panel starts with the color of the strip and light text; background and foreground change them */
   paint = ( X_GUI_PAINT * )x_xgrab( sizeof( X_GUI_PAINT ) );
   paint->colors[X_GUI_COLOR_FOREGROUND] = X_GUI_SIDEBAR_TEXT;
   paint->colors[X_GUI_COLOR_BACKGROUND] = X_GUI_SIDEBAR_BAR;
   paint->brush = CreateSolidBrush( x_guiColorRef( X_GUI_SIDEBAR_BAR ) );
   SetPropW( sidebar, x_guiPaintProperty, paint );
   SetWindowSubclass( sidebar, s_sidebarProc, 1, 0 );
   s_sidebarResize( sidebar );
   x_callbackLeaveDll();
   x_callbackRaisePending();
}

/* Add a page: its panel, the bar of its actions and its picture on the bar of pages with the tip as the title;
   arguments cBitmap, cToolTip and cShortcut as in oSidebar:AddButton(); with a menu the panel shows its
   items; returns the number of the page, 0 with an error raised */
int x_guiSidebarAdd( X_CALL_CTX *ctx, HWND sidebar, X_ITEM *page, HMENU menu )
{
   X_GUI_SIDEBAR *side = x_guiSidebarOf( sidebar );
   const X_ITEM *toolTip = x_paritem( ctx, 2 );
   X_ITEM empty __attribute__( ( cleanup( x_itemClear ) ) ) = X_ITEM_NIL_INIT;
   wchar_t *title;
   HWND actions, panel;
   DWORD error;

   x_itemPutC( &empty, "" );
   title = x_guiText( ctx, IIF( X_ISNIL( ctx, 2 ), &empty, toolTip ), "GUISIDEBAR:ADDBUTTON" );
   if( title == NULL )
      return 0;

   x_callbackEnterDll();
   actions = s_sidebarToolbar( sidebar, 0 );
   error = GetLastError();
   x_callbackLeaveDll();
   if( actions == NULL )
   {
      x_xfree( title );
      x_guiRaiseWin32( ctx, "CreateWindowExW", error );
      return 0;
   }

   x_guiWindowSend( ctx, page, ctx->self, NULL, 10, 10, X_GUI_WINDOW_PAGE, ( int )side->count + 1 );
   panel = x_guiObjectHandle( ctx, page );
   if( panel == NULL )
   {
      x_xfree( title );
      x_guiControlDestroy( actions );
      return 0;
   }

   if( !x_guiToolbarAdd( ctx, side->activity, &empty, x_paritem( ctx, 1 ), toolTip, x_paritem( ctx, 3 ), 0 ) )
   {
      x_xfree( title );
      x_guiControlDestroy( actions );
      x_guiDestroy( panel );
      return 0;
   }

   side->pages = ( X_GUI_SIDEPAGE * )x_xrealloc( side->pages, ( side->count + 1 ) * sizeof( X_GUI_SIDEPAGE ) );
   side->pages[side->count] = ( X_GUI_SIDEPAGE ){ panel, actions, title, 0, NULL, 0, NULL };
   ++side->count;
   if( menu != NULL )
      x_guiSideMenu( panel, menu );

   x_callbackEnterDll();
   s_sidebarLayout( sidebar );
   x_callbackLeaveDll();
   x_callbackRaisePending();

   return ( int )side->count;
}

/* Return the panel of page nPage */
void x_guiSidebarPanel( X_CALL_CTX *ctx, HWND sidebar, int page )
{
   const X_GUI_SIDEBAR *side = x_guiSidebarOf( sidebar );
   const X_ITEM *object = NULL;

   if( page >= 1 && ( size_t )page <= side->count )
      object = ( const X_ITEM * )GetWindowLongPtrW( side->pages[page - 1].panel, GWLP_USERDATA );

   if( object == NULL )
      x_guiRaise( ctx, "GUISIDEBAR:PANEL" );
   else
      x_itemReturn( ctx, object );
}

/* Add a button with a picture: to the bottom of the strip for page 0, otherwise to the header of the page; returns
   its number on its bar, 0 with an error raised */
int x_guiSidebarAddButton( X_CALL_CTX *ctx, HWND sidebar, int page, const X_ITEM *bitmap, const X_ITEM *toolTip )
{
   const X_GUI_SIDEBAR *side = x_guiSidebarOf( sidebar );
   X_ITEM empty __attribute__( ( cleanup( x_itemClear ) ) ) = X_ITEM_NIL_INIT;
   HWND toolbar;
   int button;

   if( page < 0 || ( size_t )page > side->count )
   {
      x_guiRaise( ctx, "GUISIDEBAR:ADDACTION" );
      return 0;
   }

   toolbar = IIF( page == 0, side->buttons, side->pages[page - 1].actions );
   x_itemPutC( &empty, "" );
   button = x_guiToolbarAdd( ctx, toolbar, &empty, bitmap, toolTip, NULL, 0 );
   if( button != 0 )
   {
      x_callbackEnterDll();
      s_sidebarLayout( sidebar );
      x_callbackLeaveDll();
      x_callbackRaisePending();
   }

   return button;
}

/* Set the count on the picture of a page, 0 hides it; 0 when there is no such page */
int x_guiSidebarBadge( HWND sidebar, int page, int count )
{
   X_GUI_SIDEBAR *side = x_guiSidebarOf( sidebar );

   if( page < 1 || ( size_t )page > side->count )
      return 0;

   side->pages[page - 1].badge = count;
   InvalidateRect( side->activity, NULL, TRUE );

   return 1;
}

/* Ctrl+B: the shown page hides its panel, otherwise the page shown last shows it again, as a click on its
   picture; 0 for a sidebar without pages */
int x_guiSidebarToggle( HWND sidebar )
{
   const X_GUI_SIDEBAR *side = x_guiSidebarOf( sidebar );
   int page;

   if( side->count == 0 )
      return 0;

   page = IIF( side->page > 0, side->page, side->last );
   page = IIF( page > 0, page, 1 );
   SendMessageW( sidebar, WM_COMMAND, MAKEWPARAM( page, BN_CLICKED ), ( LPARAM )side->activity );

   return 1;
}

/* The number of the shown page, 0 when the panel is hidden */
int x_guiSidebarPage( HWND sidebar )
{
   return x_guiSidebarOf( sidebar )->page;
}

/* Show page nPage, 0 hides the panel */
void x_guiSidebarSelect( X_CALL_CTX *ctx, HWND sidebar, int page )
{
   X_GUI_SIDEBAR *side = x_guiSidebarOf( sidebar );

   if( page < 0 || ( size_t )page > side->count )
   {
      x_guiRaise( ctx, "GUISIDEBAR:PAGE" );
      return;
   }

   x_callbackEnterDll();
   s_sidebarShow( sidebar, side, page );
   x_callbackLeaveDll();
   x_callbackRaisePending();
}

/* The width of the panel at 96 DPI */
int x_guiSidebarWidth( HWND sidebar )
{
   return x_guiSidebarOf( sidebar )->width;
}

/* Set the width of the panel at 96 DPI, not less than the smallest one */
void x_guiSidebarSetWidth( HWND sidebar, int width )
{
   X_GUI_SIDEBAR *side = x_guiSidebarOf( sidebar );

   side->width = IIF( width < X_GUI_SIDEBAR_MINWIDTH, X_GUI_SIDEBAR_MINWIDTH, width );
   x_callbackEnterDll();
   s_sidebarResize( sidebar );
   x_callbackLeaveDll();
   x_callbackRaisePending();
}

/* Return a color of a sidebar, NIL for the system color */
void x_guiSidebarColor( X_CALL_CTX *ctx, X_GUI_SIDEBAR_COLOR part )
{
   HWND sidebar = x_guiSelfHandle( ctx, "GUISIDEBAR:COLOR" );

   if( sidebar != NULL && x_guiSidebarOf( sidebar )->colors[part] >= 0 )
      x_retni( ctx, x_guiSidebarOf( sidebar )->colors[part] );
}

/* Set a color of a sidebar, NIL for the system color */
void x_guiSidebarSetColor( X_CALL_CTX *ctx, X_GUI_SIDEBAR_COLOR part )
{
   HWND sidebar = x_guiSelfHandle( ctx, "GUISIDEBAR:COLOR" );
   X_GUI_SIDEBAR *side;

   if( sidebar == NULL )
      return;

   side = x_guiSidebarOf( sidebar );

   if( !X_ISNIL( ctx, 1 ) && ( !X_ISNUM( ctx, 1 ) || x_parnint( ctx, 1 ) < 0 || x_parnint( ctx, 1 ) > 0xFFFFFF ) )
   {
      x_guiRaise( ctx, "GUISIDEBAR:COLOR" );
      return;
   }

   side->colors[part] = IIF( X_ISNIL( ctx, 1 ), -1, ( int )x_parnint( ctx, 1 ) );
   RedrawWindow( sidebar, NULL, NULL, RDW_INVALIDATE | RDW_ERASE | RDW_ALLCHILDREN );
   x_itemReturn( ctx, x_paritem( ctx, 1 ) );
}

/* ---------------------------------------------------------------------------------------------
   Sections of pages
   --------------------------------------------------------------------------------------------- */

/* Place the sections of a page one under another: the header of each, the open panels share the rest of the height
   equally, a closed panel is hidden under its header */
static void s_sectionLayout( HWND panel, const X_GUI_SIDEPAGE *page )
{
   UINT dpi = GetDpiForWindow( panel );
   int header = x_guiScale( X_GUI_SIDEBAR_SECTION, dpi );
   UINT flags = SWP_NOZORDER | SWP_NOACTIVATE;
   int open = 0, rest, y = 0, height;
   RECT client;
   size_t i;

   GetClientRect( panel, &client );
   for( i = 0; i < page->sectionCount; ++i )
      open += page->sections[i].open;

   rest = client.bottom - header * ( int )page->sectionCount;
   rest = IIF( rest > 0, rest, 0 );
   for( i = 0; i < page->sectionCount; ++i )
   {
      const X_GUI_SIDESECTION *section = &page->sections[i];
      UINT show = IIF( section->open, SWP_SHOWWINDOW, SWP_HIDEWINDOW );

      y += header;
      height = IIF( section->open, rest / IIF( open > 0, open, 1 ), 0 );
      SetWindowPos( section->panel, NULL, 0, y, client.right, height, flags | show );
      y += height;
   }

   InvalidateRect( panel, NULL, TRUE );
}

/* The header of a section in the panel of its page: the strip above the panel of the section */
static void s_sectionRow( HWND panel, const X_GUI_SIDESECTION *section, RECT *row )
{
   int header = x_guiScale( X_GUI_SIDEBAR_SECTION, GetDpiForWindow( panel ) );
   RECT frame, client;

   GetClientRect( panel, &client );
   GetWindowRect( section->panel, &frame );
   MapWindowPoints( NULL, panel, ( POINT * )&frame, 2 );
   *row = ( RECT ){ 0, frame.top - header, client.right, frame.top };
}

/* The arrow of a section at the centre x, y: pointing down for an open section, to the right for a closed one */
static void s_sectionArrow( HDC dc, int x, int y, int size, int open, COLORREF color )
{
   POINT down[3] = { { x - size, y - size / 2 }, { x + size, y - size / 2 }, { x, y + size / 2 + 1 } };
   POINT right[3] = { { x - size / 2, y - size }, { x - size / 2, y + size }, { x + size / 2 + 1, y } };
   HGDIOBJ oldBrush = SelectObject( dc, GetStockObject( DC_BRUSH ) );
   HGDIOBJ oldPen = SelectObject( dc, GetStockObject( NULL_PEN ) );

   SetDCBrushColor( dc, color );
   Polygon( dc, IIF( open, down, right ), 3 );
   SelectObject( dc, oldPen );
   SelectObject( dc, oldBrush );
}

/* The page paints the headers of its sections: a line above every header but the first one, the arrow and the
   title */
static void s_sectionPaint( HWND panel, HWND sidebar, const X_GUI_SIDEPAGE *page )
{
   PAINTSTRUCT state;
   HDC dc = BeginPaint( panel, &state );
   UINT dpi = GetDpiForWindow( panel );
   HGDIOBJ old = SelectObject( dc, x_guiFont( dpi ) );
   HBRUSH brush = ( HBRUSH )GetStockObject( DC_BRUSH );
   int arrow = x_guiScale( X_GUI_SIDEBAR_ARROW, dpi );
   int caption = x_guiScale( X_GUI_SIDEBAR_CAPTION, dpi );
   int size = x_guiScale( 4, dpi );
   UINT flags = DT_LEFT | DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX | DT_END_ELLIPSIS;
   COLORREF back, fore;
   RECT client, row, part;
   size_t i;

   x_guiSidebarColors( sidebar, &back, &fore );
   GetClientRect( panel, &client );
   SetDCBrushColor( dc, back );
   FillRect( dc, &client, brush );
   SetBkMode( dc, TRANSPARENT );
   SetTextColor( dc, fore );
   for( i = 0; i < page->sectionCount; ++i )
   {
      s_sectionRow( panel, &page->sections[i], &row );
      if( i > 0 )
      {
         part = ( RECT ){ row.left, row.top, row.right, row.top + 1 };
         SetDCBrushColor( dc, x_guiMix( back, fore, 48 ) );
         FillRect( dc, &part, brush );
      }

      s_sectionArrow( dc, arrow, ( row.top + row.bottom ) / 2, size, page->sections[i].open, fore );
      part = ( RECT ){ caption, row.top, row.right, row.bottom };
      DrawTextW( dc, page->sections[i].title, -1, &part, flags );
   }

   SelectObject( dc, old );
   EndPaint( panel, &state );
}

/* A click on the header of a section opens or closes it; 0 when the point is on no header */
static int s_sectionClick( HWND panel, X_GUI_SIDEPAGE *page, POINT point )
{
   RECT row;
   size_t i;

   for( i = 0; i < page->sectionCount; ++i )
   {
      s_sectionRow( panel, &page->sections[i], &row );
      if( PtInRect( &row, point ) )
      {
         page->sections[i].open = !page->sections[i].open;
         s_sectionLayout( panel, page );
         return 1;
      }
   }

   return 0;
}

/* The panel of a page with sections paints their headers, places them and opens or closes them on a click; id is
   the number of the page */
static LRESULT CALLBACK s_sectionProc( HWND panel, UINT message, WPARAM wParam, LPARAM lParam, UINT_PTR id,
                                       DWORD_PTR data )
{
   X_GUI_SIDEPAGE *page = &x_guiSidebarOf( ( HWND )data )->pages[id - 1];
   POINT point = { ( short )LOWORD( lParam ), ( short )HIWORD( lParam ) };

   switch( message )
   {
   case WM_ERASEBKGND:
      return 1;

   case WM_PAINT:
      s_sectionPaint( panel, ( HWND )data, page );
      return 0;

   case WM_SIZE:
      s_sectionLayout( panel, page );
      break;

   case WM_LBUTTONDOWN:
      if( s_sectionClick( panel, page, point ) )
         return 0;

      break;

   case WM_NCDESTROY:
      RemoveWindowSubclass( panel, s_sectionProc, id );
      break;
   }

   return DefSubclassProc( panel, message, wParam, lParam );
}

/* Add a section to page nPage, the first argument: a panel under a header with cTitle, the second one; the panel
   object is returned, nothing with an error raised */
void x_guiSidebarAddSection( X_CALL_CTX *ctx, HWND sidebar, X_ITEM *section )
{
   X_GUI_SIDEBAR *side = x_guiSidebarOf( sidebar );
   int number = x_parni( ctx, 1 );
   X_GUI_SIDEPAGE *page;
   const X_ITEM *owner;
   wchar_t *title;
   size_t size;
   HWND panel;

   if( number < 1 || ( size_t )number > side->count )
   {
      x_guiRaise( ctx, "GUISIDEBAR:ADDSECTION" );
      return;
   }

   title = x_guiText( ctx, x_paritem( ctx, 2 ), "GUISIDEBAR:ADDSECTION" );
   if( title == NULL )
      return;

   page = &side->pages[number - 1];
   owner = ( const X_ITEM * )GetWindowLongPtrW( page->panel, GWLP_USERDATA );
   x_guiWindowSend( ctx, section, owner, NULL, 10, 10, X_GUI_WINDOW_PANEL, 0 );
   panel = x_guiObjectHandle( ctx, section );
   if( panel == NULL )
   {
      x_xfree( title );
      return;
   }

   if( page->sectionCount == 0 )
      SetWindowSubclass( page->panel, s_sectionProc, ( UINT_PTR )number, ( DWORD_PTR )sidebar );

   size = ( page->sectionCount + 1 ) * sizeof( X_GUI_SIDESECTION );
   page->sections = ( X_GUI_SIDESECTION * )x_xrealloc( page->sections, size );
   page->sections[page->sectionCount] = ( X_GUI_SIDESECTION ){ panel, title, 1 };
   ++page->sectionCount;
   x_callbackEnterDll();
   s_sectionLayout( page->panel, page );
   x_callbackLeaveDll();
   x_callbackRaisePending();
   x_itemReturn( ctx, section );
}
