/*
 * Copyright (c) 2024-2026 xDev-Language
 *
 * Library...: xDev GUI
 * File......: internal-gui-toolbar.c
 *
 */

#include <ctype.h>
#include <string.h>
#include "internal-gui.h"
#include <uxtheme.h>

static const wchar_t s_toolbarProperty[] = L"xDev.GUI.Toolbar";
static const wchar_t s_statusProperty[] = L"xDev.GUI.Status";

/* ---------------------------------------------------------------------------------------------
   Toolbar drawing and layout
   --------------------------------------------------------------------------------------------- */

/* xDev colors are numbers 0xRRGGBB, Windows keeps them as 0x00BBGGRR */
COLORREF x_guiColorRef( int rgb )
{
   return RGB( ( rgb >> 16 ) & 0xFF, ( rgb >> 8 ) & 0xFF, rgb & 0xFF );
}

/* The toolbar data of a window, or NULL when it is not a toolbar */
X_GUI_BAR *x_guiToolbarOf( HWND window )
{
   return ( X_GUI_BAR * )GetPropW( window, s_toolbarProperty );
}

/* The sidebar data of a window, or NULL when it is not a sidebar */
X_GUI_SIDEBAR *x_guiSidebarOf( HWND window )
{
   return ( X_GUI_SIDEBAR * )GetPropW( window, x_guiSidebarProperty );
}

/* The official xDev colors: blue bar, dark blue under the mouse, light while pressed */
void x_guiToolbarInit( HWND toolbar )
{
   X_GUI_BAR *bar = ( X_GUI_BAR * )x_xgrab( sizeof( X_GUI_BAR ) );
   UINT dpi = GetDpiForWindow( toolbar );

   memset( bar, 0, sizeof( *bar ) );
   /* -1: the system draws the bar */
   bar->colors[X_GUI_COLOR_FOREGROUND] = -1;
   bar->colors[X_GUI_COLOR_BACKGROUND] = -1;
   bar->colors[X_GUI_COLOR_HOVERFOREGROUND] = -1;
   bar->colors[X_GUI_COLOR_HOVERBACKGROUND] = -1;
   bar->colors[X_GUI_COLOR_PRESSFOREGROUND] = -1;
   bar->colors[X_GUI_COLOR_PRESSBACKGROUND] = -1;
   SendMessageW( toolbar, TB_BUTTONSTRUCTSIZE, sizeof( TBBUTTON ), 0 );
   /* A button with a menu shows an arrow beside its text */
   SendMessageW( toolbar, TB_SETEXTENDEDSTYLE, 0, TBSTYLE_EX_DRAWDDARROWS );
   /* Larger buttons of one width, so they stand in an even row */
   SendMessageW( toolbar, TB_SETPADDING, 0, MAKELPARAM( x_guiScale( 16, dpi ), x_guiScale( 14, dpi ) ) );
   SendMessageW( toolbar, TB_SETBUTTONWIDTH, 0, MAKELPARAM( x_guiScale( 110, dpi ), x_guiScale( 200, dpi ) ) );
   SetPropW( toolbar, s_toolbarProperty, bar );
}

/* Release the toolbar data: its pictures, size grip, tips and shortcuts */
void x_guiToolbarRelease( HWND toolbar )
{
   X_GUI_BAR *bar = ( X_GUI_BAR * )RemovePropW( toolbar, s_toolbarProperty );
   size_t i;

   if( bar == NULL )
      return;

   if( bar->images != NULL )
   {
      SendMessageW( toolbar, TB_SETIMAGELIST, 0, 0 );
      ImageList_Destroy( bar->images );
   }

   if( bar->grip != NULL && IsWindow( bar->grip ) )
      DestroyWindow( bar->grip );

   for( i = 0; i < bar->count; ++i )
      x_xfree( bar->tips[i] );

   x_xfree( bar->tips );
   x_xfree( bar->keys );
   x_xfree( bar->modifiers );
   x_xfree( bar->glyphs );
   x_xfree( bar );
}

/* The panels of a status bar, or NULL before the first one */
X_GUI_STATUS *x_guiStatusOf( HWND window )
{
   return ( X_GUI_STATUS * )GetPropW( window, s_statusProperty );
}

/* The panels of the status bar, made at the first panel or color */
X_GUI_STATUS *x_guiStatusParts( HWND status )
{
   X_GUI_STATUS *parts = x_guiStatusOf( status );

   if( parts == NULL )
   {
      parts = ( X_GUI_STATUS * )x_xgrab( sizeof( X_GUI_STATUS ) );
      memset( parts, 0, sizeof( *parts ) );
      parts->colors[0] = -1;
      parts->colors[1] = -1;
      SetPropW( status, s_statusProperty, parts );
   }

   return parts;
}

/* Release the panels of a status bar */
void x_guiStatusRelease( HWND status )
{
   X_GUI_STATUS *parts = ( X_GUI_STATUS * )RemovePropW( status, s_statusProperty );

   if( parts == NULL )
      return;

   x_xfree( parts->widths );
   x_xfree( parts->styles );
   x_xfree( parts );
}

/* The panels without their own width share what the others leave of the bar; the last one reaches the end */
void x_guiStatusLayout( HWND status )
{
   const X_GUI_STATUS *parts = x_guiStatusOf( status );
   UINT dpi = GetDpiForWindow( status );
   int fixed = 0, shared = 0, right = 0;
   int *edges;
   RECT client;
   size_t i;

   if( parts == NULL || parts->count == 0 )
      return;

   GetClientRect( status, &client );
   for( i = 0; i < parts->count; ++i )
   {
      if( parts->widths[i] > 0 )
         fixed += x_guiScale( parts->widths[i], dpi );
      else
         ++shared;
   }

   edges = ( int * )x_xgrab( parts->count * sizeof( int ) );
   for( i = 0; i < parts->count; ++i )
   {
      if( parts->widths[i] > 0 )
         right += x_guiScale( parts->widths[i], dpi );
      else if( client.right > fixed )
         right += ( client.right - fixed ) / shared;

      edges[i] = right;
   }

   edges[parts->count - 1] = -1;
   SendMessageW( status, SB_SETPARTS, ( WPARAM )parts->count, ( LPARAM )edges );
   x_xfree( edges );
}

/* A status bar with its own background draws itself: the panels with their texts, a line between them and
   the dots of the size grip in the color of the text */
static void s_statusPaint( HWND status, const X_GUI_STATUS *parts )
{
   PAINTSTRUCT state;
   HDC dc = BeginPaint( status, &state );
   UINT dpi = GetDpiForWindow( status );
   HFONT font = ( HFONT )SendMessageW( status, WM_GETFONT, 0, 0 );
   HGDIOBJ old = SelectObject( dc, IIF( font != NULL, ( HGDIOBJ )font, GetStockObject( DEFAULT_GUI_FONT ) ) );
   COLORREF text = GetSysColor( COLOR_BTNTEXT );
   int count = ( int )SendMessageW( status, SB_GETPARTS, 0, 0 );
   int step = x_guiScale( 4, dpi );
   int size = x_guiScale( 2, dpi );
   RECT client, part, mark;
   wchar_t *label;
   int i, j;

   if( parts->colors[X_GUI_COLOR_FOREGROUND] >= 0 )
      text = x_guiColorRef( parts->colors[X_GUI_COLOR_FOREGROUND] );

   GetClientRect( status, &client );
   SetDCBrushColor( dc, x_guiColorRef( parts->colors[X_GUI_COLOR_BACKGROUND] ) );
   FillRect( dc, &client, ( HBRUSH )GetStockObject( DC_BRUSH ) );
   SetBkMode( dc, TRANSPARENT );
   SetTextColor( dc, text );
   SetDCBrushColor( dc, text );
   for( i = 0; i < count; ++i )
   {
      size_t length = LOWORD( SendMessageW( status, SB_GETTEXTLENGTHW, ( WPARAM )i, 0 ) );

      label = ( wchar_t * )x_xgrabArray( length + 1, sizeof( wchar_t ) );
      label[0] = L'\0';
      SendMessageW( status, SB_GETTEXTW, ( WPARAM )i, ( LPARAM )label );
      SendMessageW( status, SB_GETRECT, ( WPARAM )i, ( LPARAM )&part );
      if( i + 1 < count )
      {
         mark = ( RECT ){ part.right - 1, part.top + size, part.right, part.bottom - size };
         FillRect( dc, &mark, ( HBRUSH )GetStockObject( DC_BRUSH ) );
      }

      part.left += step;
      DrawTextW( dc, label, -1, &part, DT_LEFT | DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX | DT_END_ELLIPSIS );
      x_xfree( label );
   }

   if( ( GetWindowLongPtrW( status, GWL_STYLE ) & SBARS_SIZEGRIP ) && !IsZoomed( GetAncestor( status, GA_ROOT ) ) )
   {
      for( i = 1; i <= 3; ++i )
      {
         for( j = 1; i + j <= 4; ++j )
         {
            int left = client.right - step * i;
            int top = client.bottom - step * j;

            mark = ( RECT ){ left, top, left + size, top + size };
            FillRect( dc, &mark, ( HBRUSH )GetStockObject( DC_BRUSH ) );
         }
      }
   }

   SelectObject( dc, old );
   EndPaint( status, &state );
}

/* A status bar with its own background paints itself */
LRESULT CALLBACK x_guiStatusProc( HWND status, UINT message, WPARAM wParam, LPARAM lParam, UINT_PTR id,
                                  DWORD_PTR data )
{
   const X_GUI_STATUS *parts = x_guiStatusOf( status );

   UNUSED( id );
   UNUSED( data );
   if( parts != NULL && parts->colors[X_GUI_COLOR_BACKGROUND] >= 0 )
   {
      if( message == WM_ERASEBKGND )
         return 1;

      if( message == WM_PAINT )
      {
         s_statusPaint( status, parts );
         return 0;
      }
   }

   return DefSubclassProc( status, message, wParam, lParam );
}

/* A bar shown by its own style, also before its window appears on the screen */
int x_guiShown( HWND window )
{
   return window != NULL && ( GetWindowLongPtrW( window, GWL_STYLE ) & WS_VISIBLE ) != 0;
}

/* What the toolbars, the sidebar and the status bar leave of the parent; with lHorizontal only the bars at the top
   and at the bottom count, the side bars and the sidebar stand between them */
void x_guiBarsArea( HWND parent, RECT *area, int horizontal )
{
   HWND child;
   RECT frame;

   GetClientRect( parent, area );
   for( child = GetWindow( parent, GW_CHILD ); child != NULL; child = GetWindow( child, GW_HWNDNEXT ) )
   {
      const X_GUI_BAR *bar = x_guiToolbarOf( child );

      if( !x_guiShown( child ) || !GetWindowRect( child, &frame ) )
         continue;

      if( x_guiSidebarOf( child ) != NULL )
      {
         if( !horizontal )
            area->left += frame.right - frame.left;
      }
      else if( bar == NULL )
      {
         if( x_guiHasClass( child, STATUSCLASSNAMEW ) )
            area->bottom -= frame.bottom - frame.top;
      }
      else if( bar->side == X_GUI_SIDE_TOP )
         area->top += frame.bottom - frame.top;
      else if( bar->side == X_GUI_SIDE_BOTTOM )
         area->bottom -= frame.bottom - frame.top;
      else if( !horizontal && bar->side == X_GUI_SIDE_LEFT )
         area->left += frame.right - frame.left;
      else if( !horizontal && bar->side == X_GUI_SIDE_RIGHT )
         area->right -= frame.right - frame.left;
   }
}

/* The buttons of a bar at the top or at the bottom share the whole width in one row, as the keys at the bottom
   of a commander, the separators keeping their width; the height of the row */
static int s_toolbarStretch( HWND toolbar, int width )
{
   int count = ( int )SendMessageW( toolbar, TB_BUTTONCOUNT, 0, 0 );
   int buttons = 0, rest = width, share, extra, buttonWidth, i, k;
   TBBUTTON button;
   TBBUTTONINFOW info;
   RECT rect;

   for( i = 0; i < count; ++i )
   {
      if( SendMessageW( toolbar, TB_GETBUTTON, ( WPARAM )i, ( LPARAM )&button ) )
      {
         if( !( button.fsStyle & BTNS_SEP ) )
            ++buttons;
         else if( SendMessageW( toolbar, TB_GETITEMRECT, ( WPARAM )i, ( LPARAM )&rect ) )
            rest -= rect.right - rect.left;
      }
   }

   if( buttons > 0 )
   {
      share = IIF( rest > 0, rest / buttons, 0 );
      extra = IIF( rest > 0, rest % buttons, 0 );
      for( i = 0, k = 0; i < count; ++i )
      {
         if( SendMessageW( toolbar, TB_GETBUTTON, ( WPARAM )i, ( LPARAM )&button ) && !( button.fsStyle & BTNS_SEP ) )
         {
            memset( &info, 0, sizeof( info ) );
            info.cbSize = sizeof( info );
            info.dwMask = TBIF_SIZE | TBIF_BYINDEX;
            buttonWidth = share + IIF( k < extra, 1, 0 );
            info.cx = ( WORD )IIF( buttonWidth > 1, buttonWidth, 1 );
            SendMessageW( toolbar, TB_SETBUTTONINFOW, ( WPARAM )i, ( LPARAM )&info );
            ++k;
         }
      }
   }

   return HIWORD( SendMessageW( toolbar, TB_GETBUTTONSIZE, 0, 0 ) ) + x_guiScale( 4, GetDpiForWindow( toolbar ) );
}

/* A toolbar at the top or at the bottom goes across the whole parent, a bar at the left or right side between
   them takes the width of its buttons; the sidebar places its own bars */
static void s_toolbarPlace( HWND toolbar )
{
   X_GUI_BAR *bar = x_guiToolbarOf( toolbar );
   HWND frame = toolbar;
   HWND parent = GetParent( toolbar );
   HWND status = FindWindowExW( parent, NULL, STATUSCLASSNAMEW, NULL );
   int size = GetSystemMetricsForDpi( SM_CXVSCROLL, GetDpiForWindow( toolbar ) );
   int thickness, x, y, height, grip;
   SIZE buttons = { 0, 0 };
   RECT client, area, line;

   if( bar == NULL || bar->side == X_GUI_SIDE_NONE )
      return;

   GetClientRect( parent, &client );
   if( bar->side == X_GUI_SIDE_TOP || bar->side == X_GUI_SIDE_BOTTOM )
      thickness = s_toolbarStretch( toolbar, client.right );
   else
   {
      SendMessageW( toolbar, TB_GETMAXSIZE, 0, ( LPARAM )&buttons );
      if( buttons.cx == 0 )
         buttons.cx = LOWORD( SendMessageW( toolbar, TB_GETBUTTONSIZE, 0, 0 ) );

      thickness = buttons.cx + x_guiScale( 4, GetDpiForWindow( toolbar ) );
   }

   switch( bar->side )
   {
   case X_GUI_SIDE_TOP:
      SetWindowPos( frame, NULL, 0, 0, client.right, thickness, SWP_NOZORDER | SWP_NOACTIVATE );
      break;
   case X_GUI_SIDE_BOTTOM:
      area = client;
      if( x_guiShown( status ) && GetWindowRect( status, &line ) )
         area.bottom -= line.bottom - line.top;

      SetWindowPos( frame, NULL, 0, area.bottom - thickness, client.right, thickness, SWP_NOZORDER | SWP_NOACTIVATE );
      break;
   default:
      x_guiBarsArea( parent, &area, 1 );
      x = IIF( bar->side == X_GUI_SIDE_LEFT, 0, client.right - thickness );
      height = IIF( area.bottom > area.top, area.bottom - area.top, 0 );
      SetWindowPos( frame, NULL, x, area.top, thickness, height, SWP_NOZORDER | SWP_NOACTIVATE );
      break;
   }

   /* The lowest bar of a window with a sizing frame and without a status bar carries the dots for
      dragging the size of the window in its right corner */
   grip = bar->side == X_GUI_SIDE_BOTTOM && !x_guiShown( status );
   grip = grip && ( GetWindowLongPtrW( parent, GWL_STYLE ) & WS_THICKFRAME ) && !IsZoomed( parent );
   if( grip )
   {
      DWORD style = WS_CHILD | WS_VISIBLE | SBS_SIZEGRIP;
      HINSTANCE instance = GetModuleHandleW( NULL );

      if( bar->grip == NULL || !IsWindow( bar->grip ) )
         bar->grip = CreateWindowExW( 0, L"SCROLLBAR", NULL, style, 0, 0, size, size, parent, NULL, instance, NULL );

      x = client.right - size;
      y = client.bottom - size;
      if( bar->grip != NULL )
         SetWindowPos( bar->grip, HWND_TOP, x, y, size, size, SWP_NOACTIVATE | SWP_SHOWWINDOW );
   }
   else if( bar->grip != NULL && IsWindow( bar->grip ) )
      ShowWindow( bar->grip, SW_HIDE );
}

/* The bars at the top and at the bottom first, the side bars stand between them */
BOOL CALLBACK x_guiLayoutToolbar( HWND child, LPARAM parent )
{
   const X_GUI_BAR *bar = x_guiToolbarOf( child );

   if( bar == NULL || GetParent( child ) != ( HWND )parent )
      return TRUE;

   if( bar->side == X_GUI_SIDE_TOP || bar->side == X_GUI_SIDE_BOTTOM )
      s_toolbarPlace( child );

   return TRUE;
}

/* The side bars after the bars at the top and at the bottom */
BOOL CALLBACK x_guiLayoutSideToolbar( HWND child, LPARAM parent )
{
   const X_GUI_BAR *bar = x_guiToolbarOf( child );

   if( bar == NULL || GetParent( child ) != ( HWND )parent )
      return TRUE;

   if( bar->side == X_GUI_SIDE_LEFT || bar->side == X_GUI_SIDE_RIGHT )
      s_toolbarPlace( child );

   return TRUE;
}

/* A color of the toolbar; a part not set by the program has the color of Windows: the system colors of
   buttons, light blue under the mouse and a deeper blue while pressed, as in the Explorer of Windows 10 */
static COLORREF s_toolbarColor( const X_GUI_BAR *bar, X_GUI_COLOR part )
{
   if( bar->colors[part] >= 0 )
      return x_guiColorRef( bar->colors[part] );

   switch( part )
   {
   case X_GUI_COLOR_BACKGROUND:
      return GetSysColor( COLOR_BTNFACE );
   case X_GUI_COLOR_HOVERBACKGROUND:
      return RGB( 0xE5, 0xF3, 0xFF );
   case X_GUI_COLOR_PRESSBACKGROUND:
      return RGB( 0xCC, 0xE8, 0xFF );
   default:
      return GetSysColor( COLOR_BTNTEXT );
   }
}

/* The bar and every button are filled with the toolbar colors; without them the system draws the bar */
LRESULT x_guiToolbarDraw( NMTBCUSTOMDRAW *draw )
{
   const X_GUI_BAR *bar = x_guiToolbarOf( draw->nmcd.hdr.hwndFrom );
   UINT dpi = GetDpiForWindow( draw->nmcd.hdr.hwndFrom );
   X_GUI_COLOR fore, back;
   RECT face;
   int part;

   if( bar == NULL )
      return CDRF_DODEFAULT;

   for( part = 0; part < X_GUI_COLOR_COUNT && bar->colors[part] < 0; part++ )
      ;

   switch( draw->nmcd.dwDrawStage )
   {
   case CDDS_PREPAINT:
      SetDCBrushColor( draw->nmcd.hdc, s_toolbarColor( bar, X_GUI_COLOR_BACKGROUND ) );
      FillRect( draw->nmcd.hdc, &draw->nmcd.rc, ( HBRUSH )GetStockObject( DC_BRUSH ) );
      return IIF( part == X_GUI_COLOR_COUNT, CDRF_DODEFAULT, CDRF_NOTIFYITEMDRAW );

   case CDDS_ITEMPREPAINT:
      if( draw->nmcd.uItemState & ( CDIS_DISABLED | CDIS_GRAYED ) )
      {
         fore = X_GUI_COLOR_FOREGROUND;
         back = X_GUI_COLOR_BACKGROUND;
      }
      else if( draw->nmcd.uItemState & CDIS_SELECTED )
      {
         fore = X_GUI_COLOR_PRESSFOREGROUND;
         back = X_GUI_COLOR_PRESSBACKGROUND;
      }
      else if( draw->nmcd.uItemState & CDIS_HOT )
      {
         fore = X_GUI_COLOR_HOVERFOREGROUND;
         back = X_GUI_COLOR_HOVERBACKGROUND;
      }
      else
      {
         fore = X_GUI_COLOR_FOREGROUND;
         back = X_GUI_COLOR_BACKGROUND;
      }

      /* The button under the mouse or pressed is a field with a gap to the next one and a frame of the press
         color; other buttons lie on the bar without a frame */
      face = draw->nmcd.rc;
      InflateRect( &face, -x_guiScale( 2, dpi ), -x_guiScale( 3, dpi ) );
      SetDCBrushColor( draw->nmcd.hdc, s_toolbarColor( bar, X_GUI_COLOR_BACKGROUND ) );
      FillRect( draw->nmcd.hdc, &draw->nmcd.rc, ( HBRUSH )GetStockObject( DC_BRUSH ) );
      SetDCBrushColor( draw->nmcd.hdc, s_toolbarColor( bar, back ) );
      FillRect( draw->nmcd.hdc, &face, ( HBRUSH )GetStockObject( DC_BRUSH ) );
      if( back != X_GUI_COLOR_BACKGROUND )
      {
         SetDCBrushColor( draw->nmcd.hdc, s_toolbarColor( bar, X_GUI_COLOR_PRESSBACKGROUND ) );
         FrameRect( draw->nmcd.hdc, &face, ( HBRUSH )GetStockObject( DC_BRUSH ) );
      }

      if( draw->nmcd.uItemState & ( CDIS_DISABLED | CDIS_GRAYED ) )
         draw->clrText = GetSysColor( COLOR_GRAYTEXT );
      else
         draw->clrText = s_toolbarColor( bar, fore );

      draw->clrTextHighlight = draw->clrText;
      return TBCDRF_USECDCOLORS | TBCDRF_NOBACKGROUND | TBCDRF_NOEDGES | TBCDRF_NOOFFSET;
   }

   return CDRF_DODEFAULT;
}

/* Buttons are numbered from 1 in the order of Add(), separators have number 0 */
void x_guiToolbarTip( NMTBGETINFOTIPW *tip )
{
   const X_GUI_BAR *bar = x_guiToolbarOf( tip->hdr.hwndFrom );

   if( bar == NULL || tip->iItem < 1 || ( size_t )tip->iItem > bar->count || tip->cchTextMax <= 0 )
      return;

   if( bar->tips[tip->iItem - 1] != NULL )
      lstrcpynW( tip->pszText, bar->tips[tip->iItem - 1], tip->cchTextMax );
}

/* A panel or a page without colors of its own has the colors of the window it lies in */
X_GUI_PAINT *x_guiPaintOf( HWND window )
{
   X_GUI_PAINT *paint = ( X_GUI_PAINT * )GetPropW( window, x_guiPaintProperty );
   int child;

   if( paint != NULL || window == NULL )
      return paint;

   child = ( GetWindowLongPtrW( window, GWL_STYLE ) & WS_CHILD ) != 0;
   if( child && ( ATOM )GetClassLongPtrW( window, GCW_ATOM ) == x_guiWindowClass )
      paint = x_guiPaintOf( GetParent( window ) );

   return paint;
}

/* Release the colors and the brush of a window */
void x_guiPaintRelease( HWND window )
{
   X_GUI_PAINT *paint = ( X_GUI_PAINT * )RemovePropW( window, x_guiPaintProperty );

   if( paint == NULL )
      return;

   if( paint->brush != NULL )
      DeleteObject( paint->brush );

   x_xfree( paint );
}

/* Labels, frames, check boxes and sliders take the colors of the window; edit fields keep their own.
   The visual style draws frames, check boxes and radio buttons in its own colors; in a window with
   colors of the program or with their own text color they are drawn the classic way, which takes these colors */
void x_guiPaintTheme( HWND control )
{
   const X_GUI_PAINT *paint = x_guiPaintOf( GetParent( control ) );
   LONG_PTR type = GetWindowLongPtrW( control, GWL_STYLE ) & BS_TYPEMASK;
   int own = paint != NULL && ( paint->brush != NULL || paint->colors[X_GUI_COLOR_FOREGROUND] >= 0 );
   const wchar_t *theme;

   own = own || x_guiControlColor( control ) >= 0;
   theme = IIF( own, L"", NULL );

   if( !x_guiHasClass( control, L"Button" ) )
      return;

   if( type != BS_GROUPBOX && type != BS_AUTOCHECKBOX && type != BS_AUTORADIOBUTTON )
      return;

   SetWindowTheme( control, theme, theme );
}

/* The frame was made before the fields in it, so it lies above them in the order of the windows; painting its
   inside it leaves out the fields shown in it, or it covers them when it is drawn again after them */
static void s_groupClip( HWND group, HDC dc )
{
   HWND child;
   RECT frame, place, common;

   GetWindowRect( group, &frame );
   for( child = GetWindow( GetParent( group ), GW_CHILD ); child != NULL; child = GetWindow( child, GW_HWNDNEXT ) )
   {
      if( child == group || !IsWindowVisible( child ) || !GetWindowRect( child, &place ) )
         continue;

      if( IntersectRect( &common, &frame, &place ) )
      {
         MapWindowPoints( NULL, group, ( POINT * )&common, 2 );
         ExcludeClipRect( dc, common.left, common.top, common.right, common.bottom );
      }
   }
}

/* A frame in a window with colors of the program: its inside in the color of the window and a line
   of one pixel in the text color instead of the etched double line */
static void s_groupPaint( HWND group, const X_GUI_PAINT *paint )
{
   PAINTSTRUCT state;
   HDC dc = BeginPaint( group, &state );
   HFONT font = ( HFONT )SendMessageW( group, WM_GETFONT, 0, 0 );
   HGDIOBJ old = SelectObject( dc, IIF( font != NULL, ( HGDIOBJ )font, GetStockObject( DEFAULT_GUI_FONT ) ) );
   COLORREF fore = GetSysColor( COLOR_WINDOWTEXT );
   HBRUSH back = IIF( paint->brush != NULL, paint->brush, GetSysColorBrush( COLOR_BTNFACE ) );
   HPEN pen;
   int left = x_guiScale( 8, GetDpiForWindow( group ) );
   int space = x_guiScale( 2, GetDpiForWindow( group ) );
   wchar_t text[256];
   int length = GetWindowTextW( group, text, 256 );
   RECT client, label;
   SIZE size = { 0, 0 };
   HGDIOBJ oldPen, oldBrush;

   if( paint->colors[X_GUI_COLOR_FOREGROUND] >= 0 )
      fore = x_guiColorRef( paint->colors[X_GUI_COLOR_FOREGROUND] );

   pen = CreatePen( PS_SOLID, 1, fore );
   GetClientRect( group, &client );
   GetTextExtentPoint32W( dc, L"M", 1, &size );
   s_groupClip( group, dc );
   FillRect( dc, &client, back );
   oldPen = SelectObject( dc, pen );
   oldBrush = SelectObject( dc, GetStockObject( NULL_BRUSH ) );
   Rectangle( dc, 0, size.cy / 2, client.right, client.bottom );
   SelectObject( dc, oldBrush );
   SelectObject( dc, oldPen );
   if( length > 0 )
   {
      GetTextExtentPoint32W( dc, text, length, &size );
      label = ( RECT ){ left - space, 0, left + size.cx + space, size.cy };
      FillRect( dc, &label, back );
      SetBkMode( dc, TRANSPARENT );
      SetTextColor( dc, IIF( IsWindowEnabled( group ), fore, GetSysColor( COLOR_GRAYTEXT ) ) );
      TextOutW( dc, left, 0, text, length );
   }

   SelectObject( dc, old );
   DeleteObject( pen );
   EndPaint( group, &state );
}

/* A frame paints its inside in the color of the window and keeps the fields in it visible */
LRESULT CALLBACK x_guiGroupProc( HWND group, UINT message, WPARAM wParam, LPARAM lParam, UINT_PTR id,
                                 DWORD_PTR data )
{
   const X_GUI_PAINT *paint = x_guiPaintOf( GetParent( group ) );
   int own = paint != NULL && ( paint->brush != NULL || paint->colors[X_GUI_COLOR_FOREGROUND] >= 0 );
   LRESULT result;

   UNUSED( data );
   switch( message )
   {
   /* The frame does not paint its inside; it gets the color of the window */
   case WM_ERASEBKGND:
      if( !own )
      {
         RECT client;
         int saved = SaveDC( ( HDC )wParam );

         GetClientRect( group, &client );
         s_groupClip( group, ( HDC )wParam );
         FillRect( ( HDC )wParam, &client, GetSysColorBrush( COLOR_BTNFACE ) );
         RestoreDC( ( HDC )wParam, saved );
      }
      return 1;

   case WM_PAINT:
      if( own )
      {
         s_groupPaint( group, paint );
         return 0;
      }
      break;

   /* The button redraws itself after these without WM_PAINT */
   case WM_SETTEXT:
   case WM_ENABLE:
   case WM_UPDATEUISTATE:
      result = DefSubclassProc( group, message, wParam, lParam );
      if( own )
         InvalidateRect( group, NULL, TRUE );
      return result;

   case WM_NCDESTROY:
      RemoveWindowSubclass( group, x_guiGroupProc, id );
      break;
   }

   return DefSubclassProc( group, message, wParam, lParam );
}

/* Set the drawing style of a direct child of the window */
BOOL CALLBACK x_guiPaintThemeChild( HWND child, LPARAM parent )
{
   if( GetParent( child ) == ( HWND )parent )
      x_guiPaintTheme( child );

   return TRUE;
}

/* WM_CTLCOLOR*: the text color and the brush of a control in a window with colors of the program */
LRESULT x_guiPaintControl( HWND window, HDC dc, HWND control )
{
   const X_GUI_PAINT *paint = x_guiPaintOf( window );
   int fore = x_guiControlColor( control );

   if( x_guiHasClass( control, L"Edit" ) )
      return 0;

   if( fore < 0 && paint != NULL )
      fore = paint->colors[X_GUI_COLOR_FOREGROUND];

   if( fore < 0 && ( paint == NULL || paint->brush == NULL ) )
      return 0;

   if( fore >= 0 )
      SetTextColor( dc, x_guiColorRef( fore ) );

   if( paint == NULL || paint->brush == NULL )
   {
      SetBkColor( dc, GetSysColor( COLOR_BTNFACE ) );
      return ( LRESULT )GetSysColorBrush( COLOR_BTNFACE );
   }

   SetBkColor( dc, x_guiColorRef( paint->colors[X_GUI_COLOR_BACKGROUND] ) );

   return ( LRESULT )paint->brush;
}

/* ---------------------------------------------------------------------------------------------
   Toolbars
   --------------------------------------------------------------------------------------------- */

/* A bar at a side has its buttons one under another with the text under the picture; on a bar at the top or
   at the bottom the text stands in the middle of its wide button */
static void s_toolbarDock( HWND toolbar, X_GUI_BAR *bar )
{
   UINT dpi = GetDpiForWindow( toolbar );

   if( bar->side == X_GUI_SIDE_TOP || bar->side == X_GUI_SIDE_BOTTOM )
      SendMessageW( toolbar, TB_SETDRAWTEXTFLAGS, DT_CENTER | DT_RIGHT, DT_CENTER );
   else
   {
      LRESULT style = SendMessageW( toolbar, TB_GETSTYLE, 0, 0 );

      style = ( style & ~TBSTYLE_LIST ) | CCS_VERT | TBSTYLE_WRAPABLE;
      SendMessageW( toolbar, TB_SETSTYLE, 0, ( LPARAM )style );
      SendMessageW( toolbar, TB_SETPADDING, 0, MAKELPARAM( x_guiScale( 12, dpi ), x_guiScale( 12, dpi ) ) );
      SendMessageW( toolbar, TB_SETBUTTONWIDTH, 0, MAKELPARAM( x_guiScale( 80, dpi ), x_guiScale( 120, dpi ) ) );
   }
}

/* gui_Toolbar():New( oParent, [lBottom | cSide] ): a toolbar at a side of the window, at the top by default */
void x_guiToolbarNew( X_CALL_CTX *ctx )
{
   X_GUI_SIDE side = X_GUI_SIDE_TOP;
   HWND toolbar, parent;
   X_GUI_BAR *bar;

   if( X_ISLOG( ctx, 2 ) )
      side = IIF( x_parl( ctx, 2 ), X_GUI_SIDE_BOTTOM, X_GUI_SIDE_TOP );
   else if( X_ISCHAR( ctx, 2 ) )
   {
      switch( toupper( ( unsigned char )x_parc( ctx, 2 )[0] ) )
      {
      case 'T':
         side = X_GUI_SIDE_TOP;
         break;
      case 'B':
         side = X_GUI_SIDE_BOTTOM;
         break;
      case 'L':
         side = X_GUI_SIDE_LEFT;
         break;
      case 'R':
         side = X_GUI_SIDE_RIGHT;
         break;
      default:
         x_guiRaise( ctx, "GUITOOLBAR:NEW" );
         return;
      }
   }
   else if( !X_ISNIL( ctx, 2 ) )
   {
      x_guiRaise( ctx, "GUITOOLBAR:NEW" );
      return;
   }

   x_guiControlSend( ctx, X_GUI_TOOLBAR, 0, 0, 0, 0, 0 );
   toolbar = x_guiObjectHandle( ctx, ctx->self );
   bar = IIF( toolbar != NULL, x_guiToolbarOf( toolbar ), NULL );
   if( bar != NULL )
   {
      bar->side = side;
      x_callbackEnterDll();
      s_toolbarDock( toolbar, bar );
      s_toolbarPlace( toolbar );
      /* The side bars, the sidebar and the panels make room for the new bar */
      parent = GetParent( toolbar );
      EnumChildWindows( parent, x_guiLayoutSideToolbar, ( LPARAM )parent );
      EnumChildWindows( parent, x_guiLayoutSidebar, ( LPARAM )parent );
      EnumChildWindows( parent, x_guiLayoutPanel, ( LPARAM )parent );
      x_callbackLeaveDll();
      x_callbackRaisePending();
   }
}

/* A 32-bit picture with any alpha value keeps its own transparency */
int x_guiHasAlpha( const BITMAP *info )
{
   const BYTE *bits = ( const BYTE * )info->bmBits;
   size_t size, i;

   if( info->bmBitsPixel != 32 || bits == NULL )
      return 0;

   size = ( size_t )info->bmWidthBytes * ( size_t )info->bmHeight;
   for( i = 3; i < size; i += 4 )
   {
      if( bits[i] != 0 )
         return 1;
   }

   return 0;
}

/* Without alpha the color of the top left pixel is transparent */
COLORREF x_guiCornerColor( HBITMAP picture )
{
   HDC dc = CreateCompatibleDC( NULL );
   HGDIOBJ old = SelectObject( dc, picture );
   COLORREF color = GetPixel( dc, 0, 0 );

   SelectObject( dc, old );
   DeleteDC( dc );

   return color;
}

/* The first picture sets the size of all pictures of the toolbar */
static int s_toolbarImage( X_CALL_CTX *ctx, HWND toolbar, X_GUI_BAR *bar, const X_ITEM *file )
{
   HBITMAP picture = x_guiPicture( ctx, file, "GUITOOLBAR:ADD" );
   BITMAP info;
   int image = -1;

   if( picture == NULL )
      return -1;

   GetObjectW( picture, sizeof( info ), &info );
   if( bar->images == NULL )
   {
      bar->images = ImageList_Create( info.bmWidth, info.bmHeight, ILC_COLOR32 | ILC_MASK, 8, 8 );
      if( bar->images != NULL )
         SendMessageW( toolbar, TB_SETIMAGELIST, 0, ( LPARAM )bar->images );
   }

   if( bar->images != NULL && x_guiHasAlpha( &info ) )
      image = ImageList_Add( bar->images, picture, NULL );
   else if( bar->images != NULL )
      image = ImageList_AddMasked( bar->images, picture, x_guiCornerColor( picture ) );

   DeleteObject( picture );
   if( image < 0 )
      x_guiRaise( ctx, "GUITOOLBAR:ADD" );

   return image;
}

/* The buttons and their tips, shortcuts and pictures are removed; the bar stays */
void x_guiToolbarClear( HWND toolbar )
{
   X_GUI_BAR *bar = x_guiToolbarOf( toolbar );
   size_t i;

   if( bar == NULL )
      return;

   x_callbackEnterDll();
   while( SendMessageW( toolbar, TB_BUTTONCOUNT, 0, 0 ) > 0 )
      SendMessageW( toolbar, TB_DELETEBUTTON, 0, 0 );

   if( bar->images != NULL )
      ImageList_RemoveAll( bar->images );

   x_callbackLeaveDll();
   for( i = 0; i < bar->count; ++i )
      x_xfree( bar->tips[i] );

   bar->count = 0;
   s_toolbarPlace( toolbar );
   x_callbackRaisePending();
}

/* A grey button gets no click from the toolbar and s_toolbarKeyChild() skips its shortcut;
   0 when there is no such button */
int x_guiToolbarEnable( HWND toolbar, int button, int enable )
{
   const X_GUI_BAR *bar = x_guiToolbarOf( toolbar );

   if( bar == NULL || button < 1 || ( size_t )button > bar->count )
      return 0;

   x_callbackEnterDll();
   SendMessageW( toolbar, TB_ENABLEBUTTON, ( WPARAM )button, MAKELPARAM( IIF( enable, TRUE, FALSE ), 0 ) );
   x_callbackLeaveDll();
   x_callbackRaisePending();

   return 1;
}

/* The tip with the shortcut of the button after it in brackets, e.g. "Extensions (Ctrl+Shift+X)"; the shortcut
   is written in ASCII */
static wchar_t *s_toolbarTipKey( wchar_t *tip, const char *display )
{
   size_t length = ( size_t )lstrlenW( tip );
   size_t keys = strlen( display );
   wchar_t *joined = ( wchar_t * )x_xgrab( ( length + keys + 4 ) * sizeof( wchar_t ) );
   size_t i;

   memcpy( joined, tip, length * sizeof( wchar_t ) );
   joined[length] = L' ';
   joined[length + 1] = L'(';
   for( i = 0; i < keys; ++i )
      joined[length + 2 + i] = ( wchar_t )( unsigned char )display[i];

   joined[length + 2 + keys] = L')';
   joined[length + 3 + keys] = L'\0';
   x_xfree( tip );

   return joined;
}

/* Add a button; returns its number, 0 with an error raised */
int x_guiToolbarAdd( X_CALL_CTX *ctx, HWND toolbar, const X_ITEM *text, const X_ITEM *bitmap, const X_ITEM *toolTip,
                     const X_ITEM *shortcut, int dropDown )
{
   X_GUI_BAR *bar = x_guiToolbarOf( toolbar );
   wchar_t *label = NULL;
   wchar_t *tip = NULL;
   int image = I_IMAGENONE;
   UINT key = 0, modifiers = 0;
   char display[48] = "";
   const char *spec;
   TBBUTTON button;
   BOOL done;
   DWORD error;
   int valid, vertical;

   valid = bar != NULL;
   if( valid && shortcut != NULL && !X_IS_NIL( shortcut ) )
   {
      spec = IIF( X_IS_STRING( shortcut ), x_itemGetCPtr( shortcut ), NULL );
      valid = spec != NULL;
      if( valid )
         valid = x_guiParseShortcut( spec, x_itemGetCLen( shortcut ), &key, &modifiers, display, sizeof( display ) );
   }

   if( !valid )
   {
      x_guiRaise( ctx, "GUITOOLBAR:ADD" );
      return 0;
   }

   label = x_guiText( ctx, text, "GUITOOLBAR:ADD" );
   if( label == NULL )
      return 0;

   if( toolTip != NULL && !X_IS_NIL( toolTip ) )
   {
      tip = x_guiText( ctx, toolTip, "GUITOOLBAR:ADD" );
      valid = tip != NULL;
   }

   if( tip != NULL && display[0] != '\0' )
      tip = s_toolbarTipKey( tip, display );

   if( valid && bitmap != NULL && !X_IS_NIL( bitmap ) )
   {
      image = s_toolbarImage( ctx, toolbar, bar, bitmap );
      valid = image >= 0;
   }

   if( !valid )
   {
      x_xfree( label );
      x_xfree( tip );
      return 0;
   }

   memset( &button, 0, sizeof( button ) );
   button.iBitmap = image;
   button.idCommand = ( int )bar->count + 1;
   vertical = ( SendMessageW( toolbar, TB_GETSTYLE, 0, 0 ) & CCS_VERT ) != 0;
   button.fsState = ( BYTE )( TBSTATE_ENABLED | IIF( vertical, TBSTATE_WRAP, 0 ) );
   button.fsStyle = ( BYTE )IIF( dropDown, BTNS_WHOLEDROPDOWN, BTNS_BUTTON );
   button.iString = ( INT_PTR )label;
   x_callbackEnterDll();
   done = ( BOOL )SendMessageW( toolbar, TB_ADDBUTTONSW, 1, ( LPARAM )&button );
   error = GetLastError();
   x_callbackLeaveDll();
   x_xfree( label );
   x_callbackRaisePending();
   if( !done )
   {
      x_xfree( tip );
      x_guiRaiseWin32( ctx, "TB_ADDBUTTONSW", error );
      return 0;
   }

   bar->tips = ( wchar_t ** )x_xrealloc( bar->tips, ( bar->count + 1 ) * sizeof( wchar_t * ) );
   bar->keys = ( UINT * )x_xrealloc( bar->keys, ( bar->count + 1 ) * sizeof( UINT ) );
   bar->modifiers = ( UINT * )x_xrealloc( bar->modifiers, ( bar->count + 1 ) * sizeof( UINT ) );
   bar->glyphs = ( X_GUI_GLYPH * )x_xrealloc( bar->glyphs, ( bar->count + 1 ) * sizeof( X_GUI_GLYPH ) );
   bar->tips[bar->count] = tip;
   bar->keys[bar->count] = key;
   bar->modifiers[bar->count] = modifiers;
   memset( &bar->glyphs[bar->count], 0, sizeof( X_GUI_GLYPH ) );
   if( bitmap != NULL && !X_IS_NIL( bitmap ) && x_objIsKindOf( bitmap, "GUIFONTICON" ) )
      x_guiFontIconGlyph( ctx, bitmap, &bar->glyphs[bar->count] );

   ++bar->count;
   s_toolbarPlace( toolbar );

   return ( int )bar->count;
}

/* The menu of a button with an arrow opens under the button, or above it on a bar at the bottom of the window;
   its commands go to the window as the commands of the menu bar */
void x_guiToolbarPopup( HWND toolbar, int button, HMENU menu )
{
   const X_GUI_BAR *bar = x_guiToolbarOf( toolbar );
   int bottom = bar != NULL && bar->side == X_GUI_SIDE_BOTTOM;
   UINT flags = TPM_LEFTALIGN | TPM_RIGHTBUTTON;
   RECT rect;
   int y;

   if( menu == NULL || !SendMessageW( toolbar, TB_GETRECT, ( WPARAM )button, ( LPARAM )&rect ) )
      return;

   MapWindowPoints( toolbar, NULL, ( POINT * )&rect, 2 );
   flags |= IIF( bottom, TPM_BOTTOMALIGN, TPM_TOPALIGN );
   y = IIF( bottom, rect.top, rect.bottom );
   x_callbackEnterDll();
   SendMessageW( toolbar, TB_PRESSBUTTON, ( WPARAM )button, TRUE );
   TrackPopupMenuEx( menu, flags, rect.left, y, GetAncestor( toolbar, GA_ROOT ), NULL );
   SendMessageW( toolbar, TB_PRESSBUTTON, ( WPARAM )button, FALSE );
   x_callbackLeaveDll();
   x_callbackRaisePending();
}

/* Add a gap between buttons */
void x_guiToolbarSeparator( X_CALL_CTX *ctx, HWND toolbar )
{
   TBBUTTON button;

   memset( &button, 0, sizeof( button ) );
   button.iBitmap = x_guiScale( 8, GetDpiForWindow( toolbar ) );
   button.fsStyle = BTNS_SEP;
   if( SendMessageW( toolbar, TB_GETSTYLE, 0, 0 ) & CCS_VERT )
      button.fsState = TBSTATE_WRAP;

   if( !x_guiSend( toolbar, TB_ADDBUTTONSW, 1, ( LPARAM )&button ) )
      x_guiRaiseWin32( ctx, "TB_ADDBUTTONSW", GetLastError() );
   else
      s_toolbarPlace( toolbar );
}

/* Return the panel of the scoreboard, 0 for none */
void x_guiStatusBarScoreboard( X_CALL_CTX *ctx )
{
   HWND status = x_guiSelfHandle( ctx, "GUISTATUSBAR:SCOREBOARD" );
   const X_GUI_STATUS *parts = IIF( status != NULL, x_guiStatusOf( status ), NULL );

   if( status != NULL )
      x_retni( ctx, IIF( parts != NULL, parts->scoreboard, 0 ) );
}

/* Set the panel that shows "Ins" during ReadModal(), 0 for none */
void x_guiStatusBarSetScoreboard( X_CALL_CTX *ctx )
{
   HWND status = x_guiSelfHandle( ctx, "GUISTATUSBAR:SCOREBOARD" );

   if( status == NULL )
      return;

   if( !X_ISNUM( ctx, 1 ) || x_parni( ctx, 1 ) < 0 )
   {
      x_guiRaise( ctx, "GUISTATUSBAR:SCOREBOARD" );
      return;
   }

   x_guiStatusParts( status )->scoreboard = x_parni( ctx, 1 );
   x_itemReturn( ctx, x_paritem( ctx, 1 ) );
}

/* Return a color of a status bar, NIL for the system color */
void x_guiStatusBarColor( X_CALL_CTX *ctx, X_GUI_COLOR part )
{
   HWND status = x_guiSelfHandle( ctx, "GUISTATUSBAR:COLOR" );
   const X_GUI_STATUS *parts = IIF( status != NULL, x_guiStatusOf( status ), NULL );

   if( parts != NULL && parts->colors[part] >= 0 )
      x_retni( ctx, parts->colors[part] );
}

/* Set a color of a status bar, NIL for the system color */
void x_guiStatusBarSetColor( X_CALL_CTX *ctx, X_GUI_COLOR part )
{
   HWND status = x_guiSelfHandle( ctx, "GUISTATUSBAR:COLOR" );

   if( status == NULL )
      return;

   if( !X_ISNIL( ctx, 1 ) && ( !X_ISNUM( ctx, 1 ) || x_parnint( ctx, 1 ) < 0 || x_parnint( ctx, 1 ) > 0xFFFFFF ) )
   {
      x_guiRaise( ctx, "GUISTATUSBAR:COLOR" );
      return;
   }

   x_guiStatusParts( status )->colors[part] = IIF( X_ISNIL( ctx, 1 ), -1, ( int )x_parnint( ctx, 1 ) );
   InvalidateRect( status, NULL, TRUE );
   x_itemReturn( ctx, x_paritem( ctx, 1 ) );
}

/* Return a color of a toolbar, NIL for the system color */
void x_guiToolbarColor( X_CALL_CTX *ctx, X_GUI_COLOR part )
{
   HWND toolbar = x_guiSelfHandle( ctx, "GUITOOLBAR:COLOR" );
   const X_GUI_BAR *bar = IIF( toolbar != NULL, x_guiToolbarOf( toolbar ), NULL );

   if( bar != NULL && bar->colors[part] >= 0 )
      x_retni( ctx, bar->colors[part] );
}

/* Set a color of a toolbar, NIL for the system color */
void x_guiToolbarSetColor( X_CALL_CTX *ctx, X_GUI_COLOR part )
{
   HWND toolbar = x_guiSelfHandle( ctx, "GUITOOLBAR:COLOR" );
   X_GUI_BAR *bar = IIF( toolbar != NULL, x_guiToolbarOf( toolbar ), NULL );

   if( bar == NULL )
      return;

   if( !X_ISNIL( ctx, 1 ) && ( !X_ISNUM( ctx, 1 ) || x_parnint( ctx, 1 ) < 0 || x_parnint( ctx, 1 ) > 0xFFFFFF ) )
   {
      x_guiRaise( ctx, "GUITOOLBAR:COLOR" );
      return;
   }

   bar->colors[part] = IIF( X_ISNIL( ctx, 1 ), -1, ( int )x_parnint( ctx, 1 ) );
   InvalidateRect( toolbar, NULL, TRUE );
   x_itemReturn( ctx, x_paritem( ctx, 1 ) );
}
