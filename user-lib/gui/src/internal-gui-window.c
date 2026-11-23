/*
 * Copyright (c) 2024-2026 xDev-Language
 *
 * Library...: xDev GUI
 * File......: internal-gui-window.c
 *
 */

#include <string.h>
#include "internal-gui.h"

static const wchar_t s_windowClassName[] = L"xDev.GUI.Window";
static const wchar_t s_dpiProperty[] = L"xDev.GUI.Dpi";
static const wchar_t s_modalProperty[] = L"xDev.GUI.Modal";
static const wchar_t s_focusProperty[] = L"xDev.GUI.Focus";
static const wchar_t s_fillProperty[] = L"xDev.GUI.Fill";
static int s_dpiAware;
static HWND s_mainWindow;

/* A key looked up among the toolbar buttons of a window and the button found */
typedef struct
{
   UINT key;
   UINT modifiers;
   HWND toolbar;
   int button;
   int owned;
} X_GUI_KEYHIT;

/* ---------------------------------------------------------------------------------------------
   Windows
   --------------------------------------------------------------------------------------------- */

/* Place and show one page of a tab control, or find the selected one */
BOOL CALLBACK x_guiTabPageEnum( HWND child, LPARAM data )
{
   X_GUI_TABS *tabs = ( X_GUI_TABS * )data;
   int width, height;

   if( GetParent( child ) != tabs->tab || ( ATOM )GetClassLongPtrW( child, GCW_ATOM ) != x_guiWindowClass )
      return TRUE;

   if( tabs->find )
   {
      if( GetDlgCtrlID( child ) != tabs->selected )
         return TRUE;

      tabs->found = child;
      return FALSE;
   }

   width = tabs->area.right - tabs->area.left;
   height = tabs->area.bottom - tabs->area.top;
   SetWindowPos( child, NULL, tabs->area.left, tabs->area.top, width, height, SWP_NOZORDER | SWP_NOACTIVATE );
   if( tabs->selected > 0 )
      ShowWindow( child, IIF( GetDlgCtrlID( child ) == tabs->selected, SW_SHOW, SW_HIDE ) );

   return TRUE;
}

/* Place the pages of a tab control; with show only the selected page is visible */
void x_guiTabLayout( HWND tab, int show )
{
   X_GUI_TABS tabs;

   memset( &tabs, 0, sizeof( tabs ) );
   tabs.tab = tab;
   GetClientRect( tab, &tabs.area );
   SendMessageW( tab, TCM_ADJUSTRECT, FALSE, ( LPARAM )&tabs.area );
   tabs.selected = IIF( show, ( int )SendMessageW( tab, TCM_GETCURSEL, 0, 0 ) + 1, 0 );
   EnumChildWindows( tab, x_guiTabPageEnum, ( LPARAM )&tabs );
}

/* Place the pages of a tab control of the window */
static BOOL CALLBACK s_layoutTab( HWND child, LPARAM parent )
{
   if( GetParent( child ) == ( HWND )parent && x_guiHasClass( child, WC_TABCONTROLW ) )
      x_guiTabLayout( child, 0 );

   return TRUE;
}

/* A status bar of the window follows its new size */
static BOOL CALLBACK s_resizeStatusBar( HWND child, LPARAM parent )
{
   if( GetParent( child ) == ( HWND )parent && x_guiHasClass( child, STATUSCLASSNAMEW ) )
   {
      SendMessageW( child, WM_SIZE, 0, 0 );
      x_guiStatusLayout( child );
   }

   return TRUE;
}

/* The context menu given to the object of a window, or NULL */
static HMENU s_contextMenuOf( HWND window )
{
   X_ITEM *object = ( X_ITEM * )GetWindowLongPtrW( window, GWLP_USERDATA );
   X_ITEM menu __attribute__( ( cleanup( x_itemClear ) ) ) = X_ITEM_NIL_INIT;
   X_ITEM handle __attribute__( ( cleanup( x_itemClear ) ) ) = X_ITEM_NIL_INIT;
   HMENU result;

   if( object == NULL || !x_guiNotify( object, "CONTEXTMENU", &menu, 0, NULL ) || !X_IS_OBJECT( &menu ) )
      return NULL;

   if( !x_guiNotify( &menu, "HANDLE", &handle, 0, NULL ) )
      return NULL;

   result = ( HMENU )x_itemGetPtr( &handle );

   return IIF( result != NULL && IsMenu( result ), result, NULL );
}

/* Open the context menu of the target or of the nearest window around it; position -1 is the keyboard */
static int s_contextMenu( HWND window, HWND target, LPARAM position )
{
   HWND current = target;
   HMENU menu = NULL;
   POINT point = { ( short )LOWORD( position ), ( short )HIWORD( position ) };
   RECT frame;

   while( menu == NULL && current != NULL )
   {
      menu = s_contextMenuOf( current );
      if( current == window )
         break;

      current = GetParent( current );
   }

   if( menu == NULL )
      return 0;

   if( position == -1 )
   {
      GetWindowRect( target, &frame );
      point = ( POINT ){ frame.left, frame.top };
   }

   TrackPopupMenuEx( menu, TPM_RIGHTBUTTON, point.x, point.y, GetAncestor( window, GA_ROOT ), NULL );

   return 1;
}

/* A panel without its own size takes the whole parent between its toolbars and the status bar */
static void s_panelPlace( HWND panel )
{
   RECT area;
   int width, height;

   x_guiBarsArea( GetParent( panel ), &area, 0 );
   width = IIF( area.right > area.left, area.right - area.left, 0 );
   height = IIF( area.bottom > area.top, area.bottom - area.top, 0 );
   SetWindowPos( panel, NULL, area.left, area.top, width, height, SWP_NOZORDER | SWP_NOACTIVATE );
}

/* A panel that fills the window follows its new size */
BOOL CALLBACK x_guiLayoutPanel( HWND child, LPARAM parent )
{
   if( GetParent( child ) == ( HWND )parent && GetPropW( child, s_fillProperty ) != NULL )
      s_panelPlace( child );

   return TRUE;
}

/* The sidebar stands at the left edge of its window between the bars at the top and at the bottom; it is as wide
   as its strip of pictures, with the panel of the shown page and its edge */
void x_guiSidebarPlace( HWND sidebar )
{
   const X_GUI_SIDEBAR *side = x_guiSidebarOf( sidebar );
   UINT dpi = GetDpiForWindow( sidebar );
   int width = x_guiScale( X_GUI_SIDEBAR_STRIP, dpi );
   int height;
   RECT area;

   if( side->page > 0 )
      width += x_guiScale( side->width + X_GUI_SIDEBAR_EDGE, dpi );

   x_guiBarsArea( GetParent( sidebar ), &area, 1 );
   height = IIF( area.bottom > area.top, area.bottom - area.top, 0 );
   SetWindowPos( sidebar, NULL, 0, area.top, width, height, SWP_NOZORDER | SWP_NOACTIVATE );
}

/* A sidebar of the window follows its new size */
BOOL CALLBACK x_guiLayoutSidebar( HWND child, LPARAM parent )
{
   if( GetParent( child ) == ( HWND )parent && x_guiSidebarOf( child ) != NULL )
      x_guiSidebarPlace( child );

   return TRUE;
}

/* Release the keys of SetKey() of a window */
static void s_keysRelease( HWND window )
{
   X_GUI_KEYS *keys = ( X_GUI_KEYS * )RemovePropW( window, x_guiKeysProperty );

   if( keys == NULL )
      return;

   x_xfree( keys->keys );
   x_xfree( keys->modifiers );
   x_xfree( keys );
}

/* Release the icons of SetIcon() of a window; the icon of the program is not set this way */
static void s_iconsRelease( HWND window )
{
   DestroyIcon( ( HICON )SendMessageW( window, WM_SETICON, ICON_BIG, 0 ) );
   DestroyIcon( ( HICON )SendMessageW( window, WM_SETICON, ICON_SMALL, 0 ) );
}

/* A modal dialog gives its owner the input back before it disappears, so Windows activates the owner
   and not the window of another program */
void x_guiDestroyWindow( HWND window )
{
   HWND owner = GetWindow( window, GW_OWNER );

   if( owner != NULL && GetPropW( window, s_modalProperty ) != NULL )
      EnableWindow( owner, TRUE );

   DestroyWindow( window );
}

/* The window procedure of windows, dialogs, panels and pages: events go to the xDev object */
static LRESULT CALLBACK s_windowProc( HWND window, UINT message, WPARAM wParam, LPARAM lParam )
{
   X_ITEM *object = ( X_ITEM * )GetWindowLongPtrW( window, GWLP_USERDATA );
   X_ITEM result __attribute__( ( cleanup( x_itemClear ) ) ) = X_ITEM_NIL_INIT;
   int64_t values[2];

   switch( message )
   {
   case WM_NCCREATE:
   {
      const CREATESTRUCTW *create = ( const CREATESTRUCTW * )lParam;

      SetWindowLongPtrW( window, GWLP_USERDATA, ( LONG_PTR )x_itemNew( ( const X_ITEM * )create->lpCreateParams ) );
      break;
   }

   case WM_CLOSE:
      /* The user closes the window of a running Read(): Read() ends first as with Esc, then the window closes */
      if( x_guiReading > 0 && x_guiReadWindow != NULL && ( window == x_guiReadWindow || IsChild( window, x_guiReadWindow ) ) )
      {
         x_guiReadEnd = 0;
         x_guiReadClose = window;
         return 0;
      }

      /* OnClose() returning .F. keeps the window open */
      if( object == NULL || !x_guiNotify( object, "ONCLOSE", &result, 0, NULL ) )
         x_guiDestroyWindow( window );
      else if( !X_IS_LOGICAL( &result ) || result.u.logical )
         x_guiDestroyWindow( window );
      return 0;

   /* The control with the focus is remembered when the window is left and gets it back on return;
      a window shown for the first time gives the focus to its first control */
   case WM_ACTIVATE:
      if( LOWORD( wParam ) == WA_INACTIVE )
      {
         HWND focus = GetFocus();

         SetPropW( window, s_focusProperty, IIF( focus != NULL && IsChild( window, focus ), focus, NULL ) );
      }
      break;

   case WM_SETFOCUS:
   {
      HWND focus = ( HWND )GetPropW( window, s_focusProperty );

      if( focus == NULL || !IsWindow( focus ) || !IsChild( window, focus ) )
         focus = GetNextDlgTabItem( window, NULL, FALSE );

      if( focus != NULL )
         SetFocus( focus );
      return 0;
   }

   case WM_SIZE:
      EnumChildWindows( window, s_resizeStatusBar, ( LPARAM )window );
      EnumChildWindows( window, x_guiLayoutToolbar, ( LPARAM )window );
      EnumChildWindows( window, x_guiLayoutSideToolbar, ( LPARAM )window );
      EnumChildWindows( window, x_guiLayoutSidebar, ( LPARAM )window );
      EnumChildWindows( window, x_guiLayoutPanel, ( LPARAM )window );
      if( wParam != SIZE_MINIMIZED )
         x_guiLayout( window, LOWORD( lParam ), HIWORD( lParam ) );

      EnumChildWindows( window, s_layoutTab, ( LPARAM )window );
      if( object != NULL )
      {
         values[0] = MulDiv( LOWORD( lParam ), USER_DEFAULT_SCREEN_DPI, ( int )GetDpiForWindow( window ) );
         values[1] = MulDiv( HIWORD( lParam ), USER_DEFAULT_SCREEN_DPI, ( int )GetDpiForWindow( window ) );
         x_guiNotify( object, "ONRESIZE", &result, 2, values );
      }
      return 0;

   case WM_DPICHANGED:
   {
      const RECT *frame = ( const RECT * )lParam;
      UINT dpi[2] = { ( UINT )( UINT_PTR )GetPropW( window, s_dpiProperty ), HIWORD( wParam ) };
      int width = frame->right - frame->left;
      int height = frame->bottom - frame->top;

      SetPropW( window, s_dpiProperty, ( HANDLE )( UINT_PTR )dpi[1] );
      if( dpi[0] != 0 )
         EnumChildWindows( window, x_guiRescaleChild, ( LPARAM )dpi );

      SetWindowPos( window, NULL, frame->left, frame->top, width, height, SWP_NOZORDER | SWP_NOACTIVATE );
      return 0;
   }

   case WM_KEYDOWN:
      if( object != NULL )
      {
         values[0] = ( int64_t )wParam;
         x_guiNotify( object, "ONKEY", &result, 1, values );
      }
      return 0;

   case WM_MEASUREITEM:
   {
      MEASUREITEMSTRUCT *measure = ( MEASUREITEMSTRUCT * )lParam;

      if( wParam == 0 && measure->CtlType == ODT_MENU && measure->itemData != 0 )
      {
         x_guiMenuMeasure( window, measure );
         return TRUE;
      }
      break;
   }

   case WM_DRAWITEM:
   {
      const DRAWITEMSTRUCT *draw = ( const DRAWITEMSTRUCT * )lParam;

      if( wParam == 0 && draw->CtlType == ODT_MENU && draw->itemData != 0 )
      {
         x_guiMenuDraw( window, draw );
         return TRUE;
      }
      break;
   }

   case WM_INITMENUPOPUP:
      if( object != NULL )
      {
         values[0] = ( int64_t )( intptr_t )wParam;
         x_guiNotifyEvent( object, X_GUI_EVENT_MENUWHEN, &result, 1, values );
      }
      break;

   case WM_NOTIFY:
   {
      const NMHDR *header = ( const NMHDR * )lParam;
      HWND from = IIF( header != NULL, header->hwndFrom, NULL );
      X_ITEM *control = IIF( from != NULL, ( X_ITEM * )GetWindowLongPtrW( from, GWLP_USERDATA ), NULL );

      if( from != NULL && x_guiToolbarOf( from ) != NULL )
      {
         if( header->code == NM_CUSTOMDRAW )
            return x_guiToolbarDraw( ( NMTBCUSTOMDRAW * )lParam );

         if( header->code == TBN_GETINFOTIPW )
            x_guiToolbarTip( ( NMTBGETINFOTIPW * )lParam );

         /* A click on a button with a menu goes the way of a click on a button and of its shortcut */
         if( header->code == TBN_DROPDOWN )
         {
            int item = ( ( const NMTOOLBARW * )lParam )->iItem;

            PostMessageW( window, WM_COMMAND, MAKEWPARAM( item, BN_CLICKED ), ( LPARAM )from );
            return TBDDRET_DEFAULT;
         }
         return 0;
      }

      if( control != NULL && GetParent( header->hwndFrom ) == window )
      {
         values[0] = ( int )header->code;
         values[1] = 0;
         if( header->code == LVN_ITEMCHANGED )
         {
            const NMLISTVIEW *change = ( const NMLISTVIEW * )lParam;
            int selected = ( change->uChanged & LVIF_STATE ) && ( change->uNewState & LVIS_SELECTED );

            if( !x_guiQuiet && selected && !( change->uOldState & LVIS_SELECTED ) )
               values[1] = change->iItem + 1;
         }
         else if( header->code == TVN_SELCHANGEDW )
            values[1] = ( ( const NMTREEVIEWW * )lParam )->action != TVC_UNKNOWN;

         x_guiNotifyEvent( control, X_GUI_EVENT_NOTIFY, &result, 2, values );
      }
      return 0;
   }

   case WM_HSCROLL:
   case WM_VSCROLL:
      if( lParam != 0 )
      {
         X_ITEM *control = ( X_ITEM * )GetWindowLongPtrW( ( HWND )lParam, GWLP_USERDATA );

         if( control != NULL )
         {
            values[0] = LOWORD( wParam );
            x_guiNotifyEvent( control, X_GUI_EVENT_COMMAND, &result, 1, values );
         }
         return 0;
      }
      break;

   case WM_COMMAND:
      if( lParam != 0 )
      {
         X_ITEM *control = ( X_ITEM * )GetWindowLongPtrW( ( HWND )lParam, GWLP_USERDATA );

         /* A GET of several rows follows its text while it is written */
         if( HIWORD( wParam ) == EN_CHANGE && GetFocus() == ( HWND )lParam )
            x_guiGetFit( ( HWND )lParam, 1 );

         if( control != NULL )
         {
            values[0] = HIWORD( wParam );
            values[1] = LOWORD( wParam );
            x_guiNotifyEvent( control, X_GUI_EVENT_COMMAND, &result, 2, values );
         }
         return 0;
      }

      if( object != NULL && LOWORD( wParam ) >= X_GUI_MENU_FIRST )
      {
         values[0] = LOWORD( wParam );
         x_guiNotify( object, "ONMENU", &result, 1, values );
         return 0;
      }

      if( LOWORD( wParam ) == IDCANCEL )
      {
         HWND cancel = ( HWND )GetPropW( window, x_guiCancelProperty );

         if( cancel != NULL && IsWindow( cancel ) && IsWindowEnabled( cancel ) )
            SendMessageW( cancel, BM_CLICK, 0, 0 );
         return 0;
      }
      break;

   case WM_ERASEBKGND:
   {
      const X_GUI_PAINT *paint = x_guiPaintOf( window );
      RECT client;

      if( paint != NULL && paint->brush != NULL )
      {
         GetClientRect( window, &client );
         FillRect( ( HDC )wParam, &client, paint->brush );
         return 1;
      }
      break;
   }

   case WM_CTLCOLORSTATIC:
   case WM_CTLCOLORBTN:
   {
      LRESULT brush = x_guiPaintControl( window, ( HDC )wParam, ( HWND )lParam );

      if( brush != 0 )
         return brush;

      break;
   }

   case WM_CONTEXTMENU:
      if( s_contextMenu( window, ( HWND )wParam, lParam ) )
         return 0;

      break;

   case DM_GETDEFID:
   {
      HWND button = ( HWND )GetPropW( window, x_guiDefaultProperty );

      if( button != NULL && IsWindow( button ) )
         return MAKELRESULT( GetDlgCtrlID( button ), DC_HASDEFID );

      break;
   }

   case WM_TIMER:
   {
      const X_GUI_TIMER *timer = x_guiTimerFind( ( UINT_PTR )wParam );

      if( timer != NULL && timer->window == window )
      {
         X_ITEM *target = x_itemNew( timer->timer );

         x_guiNotifyEvent( target, X_GUI_EVENT_TIMER, &result, 0, NULL );
         x_itemRelease( target );
      }
      return 0;
   }

   case WM_DESTROY:
      if( RemovePropW( window, s_modalProperty ) != NULL && GetWindow( window, GW_OWNER ) != NULL )
         EnableWindow( GetWindow( window, GW_OWNER ), TRUE );

      x_guiTimersRelease( window );
      EnumChildWindows( window, x_guiDetachChild, 0 );
      if( window == s_mainWindow )
      {
         s_mainWindow = NULL;
         if( x_guiRunning )
            PostQuitMessage( 0 );
      }
      return 0;

   case WM_NCDESTROY:
      RemovePropW( window, s_dpiProperty );
      RemovePropW( window, s_focusProperty );
      RemovePropW( window, s_fillProperty );
      s_keysRelease( window );
      s_iconsRelease( window );
      RemovePropW( window, x_guiDefaultProperty );
      RemovePropW( window, x_guiCancelProperty );
      RemovePropW( window, x_guiToolTipProperty );
      x_guiPaintRelease( window );
      x_guiDetach( window );
      break;
   }

   return DefWindowProcW( window, message, wParam, lParam );
}

/* Register the window class of windows, dialogs, panels and pages once */
static BOOL s_registerWindowClass( HINSTANCE instance )
{
   WNDCLASSEXW windowClass;
   int iconWidth = GetSystemMetrics( SM_CXSMICON );
   int iconHeight = GetSystemMetrics( SM_CYSMICON );
   LPCWSTR icon = MAKEINTRESOURCEW( 1 );

   if( x_guiWindowClass != 0 )
      return TRUE;

   memset( &windowClass, 0, sizeof( windowClass ) );
   windowClass.cbSize = sizeof( windowClass );
   windowClass.style = CS_HREDRAW | CS_VREDRAW;
   windowClass.lpfnWndProc = s_windowProc;
   windowClass.hInstance = instance;
   /* The icon put into the program by xdev from the .ico of the .rmk, otherwise the one of Windows */
   windowClass.hIcon = LoadIconW( instance, icon );
   windowClass.hIconSm = ( HICON )LoadImageW( instance, icon, IMAGE_ICON, iconWidth, iconHeight, LR_DEFAULTCOLOR );
   windowClass.hCursor = LoadCursorW( NULL, IDC_ARROW );
   windowClass.hbrBackground = ( HBRUSH )( COLOR_BTNFACE + 1 );
   windowClass.lpszClassName = s_windowClassName;
   if( windowClass.hIcon == NULL )
      windowClass.hIcon = LoadIconW( NULL, IDI_APPLICATION );

   x_guiWindowClass = RegisterClassExW( &windowClass );

   return x_guiWindowClass != 0;
}

/* The frame of a window with a client area of width x height at 96 DPI, for dpi */
static void s_frameForDpi( RECT *frame, int width, int height, UINT dpi, DWORD style, DWORD exStyle )
{
   *frame = ( RECT ){ 0, 0, x_guiScale( width, dpi ), x_guiScale( height, dpi ) };
   AdjustWindowRectExForDpi( frame, style, FALSE, exStyle, dpi );
}

/* Center a window over its owner, or on its screen without one, inside the work area */
static void s_centerOnOwner( HWND window, HWND owner )
{
   MONITORINFO monitor;
   RECT frame, area;
   int x, y, width, height;
   HMONITOR screen;

   memset( &monitor, 0, sizeof( monitor ) );
   monitor.cbSize = sizeof( monitor );
   GetWindowRect( window, &frame );
   width = frame.right - frame.left;
   height = frame.bottom - frame.top;
   screen = MonitorFromWindow( IIF( owner != NULL, owner, window ), MONITOR_DEFAULTTONEAREST );
   if( !GetMonitorInfoW( screen, &monitor ) )
      return;

   area = monitor.rcWork;
   if( owner != NULL )
      GetWindowRect( owner, &frame );
   else
      frame = area;

   x = frame.left + ( frame.right - frame.left - width ) / 2;
   y = frame.top + ( frame.bottom - frame.top - height ) / 2;
   x = IIF( x + width > area.right, area.right - width, x );
   y = IIF( y + height > area.bottom, area.bottom - height, y );
   x = IIF( x < area.left, area.left, x );
   y = IIF( y < area.top, area.top, y );
   SetWindowPos( window, NULL, x, y, 0, 0, SWP_NOZORDER | SWP_NOSIZE | SWP_NOACTIVATE );
}

/* Create the window of a window object through the X_GUI_EVENT_CREATE function of its class */
void x_guiWindowSend( X_CALL_CTX *ctx, X_ITEM *target, const X_ITEM *owner, const X_ITEM *title, int width,
                      int height, X_GUI_WINDOW_KIND kind, int id )
{
   X_ITEM values[6];
   X_ARG args[6];
   int i;

   for( i = 0; i < 6; ++i )
      values[i] = ( X_ITEM )X_ITEM_NIL_INIT;

   if( owner != NULL )
      x_itemCopy( &values[0], owner );

   if( title != NULL )
      x_itemCopy( &values[1], title );

   x_itemPutNI( &values[2], width );
   x_itemPutNI( &values[3], height );
   x_itemPutNI( &values[4], ( int )kind );
   x_itemPutNI( &values[5], id );
   for( i = 0; i < 6; ++i )
      args[i] = ( X_ARG ){ &values[i], 0 };

   x_guiHandlerCall( ctx->result, target, X_GUI_EVENT_CREATE, 6, args );
   for( i = 0; i < 6; ++i )
      x_itemClear( &values[i] );
}

/* gui_Window():New(): a top-level window, 640 x 480 by default */
void x_guiWindowNew( X_CALL_CTX *ctx )
{
   int width = x_parnidef( ctx, 2, 640 );
   int height = x_parnidef( ctx, 3, 480 );

   x_guiWindowSend( ctx, ctx->self, NULL, x_paritem( ctx, 1 ), width, height, X_GUI_WINDOW_TOP, 0 );
}

/* gui_Dialog():New(): a dialog over its owner, 400 x 300 by default */
void x_guiDialogNew( X_CALL_CTX *ctx )
{
   const X_ITEM *owner = x_paritem( ctx, 1 );
   int width = x_parnidef( ctx, 3, 400 );
   int height = x_parnidef( ctx, 4, 300 );

   if( owner != NULL && !X_IS_NIL( owner ) && x_guiObjectWindow( ctx, owner, "GUIDIALOG:NEW" ) == NULL )
      return;

   x_guiWindowSend( ctx, ctx->self, owner, x_paritem( ctx, 2 ), width, height, X_GUI_WINDOW_DIALOG, 0 );
}

/* A panel holds the controls of one part of a window; without nWidth and nHeight it fills the window */
void x_guiPanelNew( X_CALL_CTX *ctx )
{
   const X_ITEM *owner = x_paritem( ctx, 1 );
   const int fill = X_ISNIL( ctx, 4 ) && X_ISNIL( ctx, 5 );
   HWND parent, panel;
   RECT client;
   UINT dpi;
   int width, height, x, y;

   parent = x_guiObjectWindow( ctx, owner, "GUIPANEL:NEW" );
   if( parent == NULL )
      return;

   dpi = GetDpiForWindow( parent );
   GetClientRect( parent, &client );
   width = x_parnidef( ctx, 4, 1 );
   height = x_parnidef( ctx, 5, 1 );
   if( fill )
   {
      width = IIF( client.right > 0, MulDiv( client.right, USER_DEFAULT_SCREEN_DPI, ( int )dpi ), 1 );
      height = IIF( client.bottom > 0, MulDiv( client.bottom, USER_DEFAULT_SCREEN_DPI, ( int )dpi ), 1 );
   }

   x_guiWindowSend( ctx, ctx->self, owner, NULL, width, height, X_GUI_WINDOW_PANEL, 0 );
   panel = x_guiObjectHandle( ctx, ctx->self );
   if( panel == NULL || !IsWindow( panel ) )
      return;

   x_callbackEnterDll();
   if( fill )
   {
      SetPropW( panel, s_fillProperty, ( HANDLE )1 );
      s_panelPlace( panel );
   }
   else
   {
      x = x_guiScale( x_parnidef( ctx, 2, 0 ), dpi );
      y = x_guiScale( x_parnidef( ctx, 3, 0 ), dpi );
      SetWindowPos( panel, NULL, x, y, 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE );
   }

   x_callbackLeaveDll();
   x_callbackRaisePending();
}

/* X_GUI_EVENT_CREATE: create the window of a window, dialog, page or panel object */
void x_guiWindowCreate( X_CALL_CTX *ctx )
{
   static const DWORD styles[] = { WS_OVERLAPPEDWINDOW, WS_POPUP | WS_CAPTION | WS_SYSMENU,
                                     WS_CHILD | WS_CLIPSIBLINGS, WS_CHILD | WS_CLIPSIBLINGS | WS_VISIBLE };
   static const DWORD exStyles[] = { 0, WS_EX_DLGMODALFRAME, WS_EX_CONTROLPARENT, WS_EX_CONTROLPARENT };
   HINSTANCE instance = GetModuleHandleW( NULL );
   X_ITEM *slot = x_objDataSlot( ctx, 1 );
   const X_ITEM *ownerItem = x_paritem( ctx, 1 );
   const int width = x_parni( ctx, 3 );
   const int height = x_parni( ctx, 4 );
   const int kind = x_parni( ctx, 5 );
   const int dialog = kind == X_GUI_WINDOW_DIALOG;
   const int page = kind == X_GUI_WINDOW_PAGE;
   const int child = page || kind == X_GUI_WINDOW_PANEL;
   const char *operation = IIF( dialog, "GUIDIALOG:NEW", "GUIWINDOW:NEW" );
   LPVOID param = ( LPVOID )ctx->self;
   int valid, pos, cx, cy;
   HMENU menu;
   DWORD style;
   DWORD exStyle;
   HWND owner = NULL;
   wchar_t *title;
   HWND window;
   UINT dpi;
   RECT frame;
   DWORD error;

   valid = slot != NULL && kind >= X_GUI_WINDOW_TOP && kind <= X_GUI_WINDOW_PANEL && width > 0 && height > 0;
   valid = valid && ( X_ISNIL( ctx, 2 ) || X_ISCHAR( ctx, 2 ) );
   if( !valid )
   {
      x_guiRaise( ctx, operation );
      return;
   }

   style = styles[kind] | WS_CLIPCHILDREN;
   exStyle = exStyles[kind];
   if( page )
   {
      owner = x_guiObjectHandle( ctx, ( X_ITEM * )ownerItem );
      if( owner == NULL || !IsWindow( owner ) )
      {
         x_guiRaise( ctx, "GUITABCONTROL:ADDPAGE" );
         return;
      }
   }
   else if( kind == X_GUI_WINDOW_PANEL )
   {
      owner = x_guiObjectWindow( ctx, ownerItem, "GUIPANEL:NEW" );
      if( owner == NULL )
         return;
   }
   else if( ownerItem != NULL && !X_IS_NIL( ownerItem ) )
   {
      owner = x_guiObjectWindow( ctx, ownerItem, "GUIDIALOG:NEW" );
      if( owner == NULL )
         return;
   }

   if( !s_dpiAware )
   {
      SetProcessDpiAwarenessContext( DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2 );
      s_dpiAware = 1;
   }

   if( !s_registerWindowClass( instance ) )
   {
      x_guiRaiseWin32( ctx, "RegisterClassExW", GetLastError() );
      return;
   }

   if( X_ISNIL( ctx, 2 ) )
      title = ( wchar_t * )x_xgrabzArray( 1, sizeof( wchar_t ) );
   else
   {
      title = x_guiText( ctx, x_paritem( ctx, 2 ), operation );
      if( title == NULL )
         return;
   }

   dpi = IIF( owner != NULL, GetDpiForWindow( owner ), GetDpiForSystem() );
   s_frameForDpi( &frame, width, height, dpi, style, exStyle );
   x_callbackEnterDll();
   pos = IIF( child, 0, CW_USEDEFAULT );
   cx = frame.right - frame.left;
   cy = frame.bottom - frame.top;
   menu = IIF( page, ( HMENU )( INT_PTR )x_parni( ctx, 6 ), NULL );
   window = CreateWindowExW( exStyle, s_windowClassName, title, style, pos, pos, cx, cy, owner, menu, instance, param );
   error = GetLastError();
   if( window != NULL && !child && GetDpiForWindow( window ) != dpi )
   {
      dpi = GetDpiForWindow( window );
      s_frameForDpi( &frame, width, height, dpi, style, exStyle );
      cx = frame.right - frame.left;
      cy = frame.bottom - frame.top;
      SetWindowPos( window, NULL, 0, 0, cx, cy, SWP_NOZORDER | SWP_NOMOVE | SWP_NOACTIVATE );
   }

   if( window != NULL )
   {
      SetPropW( window, s_dpiProperty, ( HANDLE )( UINT_PTR )dpi );
      if( owner != NULL && !child )
         s_centerOnOwner( window, owner );
   }

   x_callbackLeaveDll();
   x_xfree( title );
   x_callbackRaisePending();

   if( window == NULL )
   {
      x_guiRaiseWin32( ctx, "CreateWindowExW", error );
      return;
   }

   x_itemPutPtr( slot, window );
   x_guiReturnSelf( ctx );
}

/* A frame for dragging the size and the button for the full screen; the controls follow their anchors */
int x_guiWindowSizable( HWND window )
{
   return ( GetWindowLongPtrW( window, GWL_STYLE ) & WS_THICKFRAME ) != 0;
}

/* Turn the sizing frame and the maximize button of a window on or off */
void x_guiWindowSetSizable( HWND window, int sizable )
{
   LONG_PTR style = GetWindowLongPtrW( window, GWL_STYLE );

   if( sizable )
      style |= WS_THICKFRAME | WS_MAXIMIZEBOX;
   else
      style &= ~( LONG_PTR )( WS_THICKFRAME | WS_MAXIMIZEBOX );

   x_callbackEnterDll();
   SetWindowLongPtrW( window, GWL_STYLE, style );
   SetWindowPos( window, NULL, 0, 0, 0, 0, SWP_FRAMECHANGED | SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE );
   x_callbackLeaveDll();
   x_callbackRaisePending();
}

/* Set the menu bar of a window; the window grows so its client area keeps its size */
void x_guiWindowSetMenu( X_CALL_CTX *ctx, HWND window, HMENU menu )
{
   RECT before, after, frame;
   BOOL done;
   DWORD error;

   GetClientRect( window, &before );
   x_callbackEnterDll();
   done = SetMenu( window, menu );
   error = GetLastError();
   if( done )
   {
      GetClientRect( window, &after );
      GetWindowRect( window, &frame );
      if( after.bottom < before.bottom )
      {
         int width = frame.right - frame.left;
         int height = frame.bottom - frame.top + before.bottom - after.bottom;

         SetWindowPos( window, NULL, 0, 0, width, height, SWP_NOZORDER | SWP_NOMOVE | SWP_NOACTIVATE );
      }
   }

   x_callbackLeaveDll();
   x_callbackRaisePending();
   if( !done )
      x_guiRaiseWin32( ctx, "SetMenu", error );
}

/* The block bWhen of the command gives .F. when the command cannot be chosen now */
static int s_actionAllowed( X_CALL_CTX *ctx, const X_GUI_ACTION *action )
{
   X_ITEM result __attribute__( ( cleanup( x_itemClear ) ) ) = X_ITEM_NIL_INIT;
   X_ITEM block __attribute__( ( cleanup( x_itemClear ) ) ) = X_ITEM_NIL_INIT;

   if( action->when == NULL || !X_IS_BLOCK( action->when ) )
      return 1;

   x_itemCopy( &block, action->when );
   x_evalBlock( ctx, &result, &block, 0, NULL );

   return !X_IS_LOGICAL( &result ) || result.u.logical;
}

/* Before a menu opens its commands with bWhen become active or grey */
void x_guiWindowMenuWhen( X_CALL_CTX *ctx, HMENU menu )
{
   int count = IIF( menu != NULL && IsMenu( menu ), GetMenuItemCount( menu ), 0 );
   int item;
   UINT id;
   size_t i;

   for( item = 0; item < count; ++item )
   {
      id = GetMenuItemID( menu, item );
      if( id == ( UINT )-1 )
         continue;

      for( i = 0; i < x_guiActionCount; ++i )
      {
         if( x_guiActions[i].id == ( int )id && x_guiActions[i].when != NULL && X_IS_BLOCK( x_guiActions[i].when ) )
         {
            UINT enable = IIF( s_actionAllowed( ctx, &x_guiActions[i] ), MF_ENABLED, MF_GRAYED );

            EnableMenuItem( menu, ( UINT )item, MF_BYPOSITION | enable );
            break;
         }
      }
   }
}

/* Run the action of a menu command with the window as its parameter, unless its bWhen refuses */
void x_guiWindowRunMenu( X_CALL_CTX *ctx, int id )
{
   X_ITEM result __attribute__( ( cleanup( x_itemClear ) ) ) = X_ITEM_NIL_INIT;
   X_ITEM block __attribute__( ( cleanup( x_itemClear ) ) ) = X_ITEM_NIL_INIT;
   X_ARG args[1];
   size_t i;

   for( i = 0; i < x_guiActionCount; ++i )
   {
      if( x_guiActions[i].id == id )
      {
         if( !X_IS_BLOCK( x_guiActions[i].action ) || !s_actionAllowed( ctx, &x_guiActions[i] ) )
            return;

         x_itemCopy( &block, x_guiActions[i].action );
         args[0] = ( X_ARG ){ ctx->self, 0 };
         x_evalBlock( ctx, &result, &block, 1, args );
         return;
      }
   }
}

/* The width or the height of the client area at 96 DPI */
int x_guiClientSize( HWND window, X_GUI_BOUND part )
{
   RECT client;
   int size;

   GetClientRect( window, &client );
   size = IIF( part == X_GUI_HEIGHT, client.bottom, client.right );

   return MulDiv( size, USER_DEFAULT_SCREEN_DPI, ( int )GetDpiForWindow( window ) );
}

/* Set the width or the height of the client area, given at 96 DPI */
void x_guiSetClientSize( X_CALL_CTX *ctx, HWND window, X_GUI_BOUND part, int value )
{
   const UINT dpi = GetDpiForWindow( window );
   RECT client, frame;
   DWORD style, exStyle;
   int width, height;

   if( value <= 0 )
   {
      x_guiRaise( ctx, "GUIWINDOW:SIZE" );
      return;
   }

   GetClientRect( window, &client );
   if( part == X_GUI_HEIGHT )
      client.bottom = x_guiScale( value, dpi );
   else
      client.right = x_guiScale( value, dpi );

   frame = ( RECT ){ 0, 0, client.right, client.bottom };
   style = ( DWORD )GetWindowLongPtrW( window, GWL_STYLE );
   exStyle = ( DWORD )GetWindowLongPtrW( window, GWL_EXSTYLE );
   AdjustWindowRectExForDpi( &frame, style, GetMenu( window ) != NULL, exStyle, dpi );
   width = frame.right - frame.left;
   height = frame.bottom - frame.top;
   x_callbackEnterDll();
   SetWindowPos( window, NULL, 0, 0, width, height, SWP_NOZORDER | SWP_NOMOVE | SWP_NOACTIVATE );
   x_callbackLeaveDll();
   x_callbackRaisePending();
}

/* Return a color of the window, NIL for the system color */
void x_guiWindowColor( X_CALL_CTX *ctx, X_GUI_COLOR part )
{
   HWND window = x_guiSelfHandle( ctx, "GUIWINDOW:COLOR" );
   const X_GUI_PAINT *paint = IIF( window != NULL, x_guiPaintOf( window ), NULL );

   if( paint != NULL && paint->colors[part] >= 0 )
      x_retni( ctx, paint->colors[part] );
}

/* Set a color of the window, NIL for the system color; the controls are drawn again */
void x_guiWindowSetColor( X_CALL_CTX *ctx, X_GUI_COLOR part )
{
   HWND window = x_guiSelfHandle( ctx, "GUIWINDOW:COLOR" );
   X_GUI_PAINT *paint;

   if( window == NULL )
      return;

   if( !X_ISNIL( ctx, 1 ) && ( !X_ISNUM( ctx, 1 ) || x_parnint( ctx, 1 ) < 0 || x_parnint( ctx, 1 ) > 0xFFFFFF ) )
   {
      x_guiRaise( ctx, "GUIWINDOW:COLOR" );
      return;
   }

   paint = ( X_GUI_PAINT * )GetPropW( window, x_guiPaintProperty );
   if( paint == NULL )
   {
      paint = ( X_GUI_PAINT * )x_xgrab( sizeof( X_GUI_PAINT ) );
      paint->colors[X_GUI_COLOR_FOREGROUND] = -1;
      paint->colors[X_GUI_COLOR_BACKGROUND] = -1;
      paint->brush = NULL;
      SetPropW( window, x_guiPaintProperty, paint );
   }

   paint->colors[part] = IIF( X_ISNIL( ctx, 1 ), -1, ( int )x_parnint( ctx, 1 ) );
   if( part == X_GUI_COLOR_BACKGROUND )
   {
      if( paint->brush != NULL )
         DeleteObject( paint->brush );

      paint->brush = IIF( paint->colors[part] >= 0, CreateSolidBrush( x_guiColorRef( paint->colors[part] ) ), NULL );
   }

   x_callbackEnterDll();
   EnumChildWindows( window, x_guiPaintThemeChild, ( LPARAM )window );
   RedrawWindow( window, NULL, NULL, RDW_ERASE | RDW_INVALIDATE | RDW_ALLCHILDREN );
   x_callbackLeaveDll();
   x_callbackRaisePending();
   if( x_paritem( ctx, 1 ) != NULL )
      x_itemReturn( ctx, x_paritem( ctx, 1 ) );
}

/* Center a window over its owner */
void x_guiCenter( HWND window )
{
   x_callbackEnterDll();
   s_centerOnOwner( window, GetWindow( window, GW_OWNER ) );
   x_callbackLeaveDll();
   x_callbackRaisePending();
}

/* Set the icon of the title bar and of the taskbar of a window from the .ico file cFile */
void x_guiWindowSetIcon( X_CALL_CTX *ctx, HWND window, const X_ITEM *file )
{
   wchar_t *name = x_guiText( ctx, file, "GUIWINDOW:SETICON" );
   UINT dpi;
   int bigSize, smallSize;
   HICON big, small;
   DWORD error;

   if( name == NULL )
      return;

   x_callbackEnterDll();
   dpi = GetDpiForWindow( window );
   bigSize = GetSystemMetricsForDpi( SM_CXICON, dpi );
   smallSize = GetSystemMetricsForDpi( SM_CXSMICON, dpi );
   big = ( HICON )LoadImageW( NULL, name, IMAGE_ICON, bigSize, bigSize, LR_LOADFROMFILE );
   small = ( HICON )LoadImageW( NULL, name, IMAGE_ICON, smallSize, smallSize, LR_LOADFROMFILE );
   error = GetLastError();
   if( big != NULL && small != NULL )
   {
      s_iconsRelease( window );
      SendMessageW( window, WM_SETICON, ICON_BIG, ( LPARAM )big );
      SendMessageW( window, WM_SETICON, ICON_SMALL, ( LPARAM )small );
   }
   else
   {
      DestroyIcon( big );
      DestroyIcon( small );
   }
   x_callbackLeaveDll();
   x_xfree( name );
   x_callbackRaisePending();
   if( big == NULL || small == NULL )
      x_guiRaiseWin32( ctx, "LoadImageW", error );
}

/* Show or hide a window */
void x_guiShow( HWND window, int visible )
{
   x_callbackEnterDll();
   ShowWindow( window, IIF( visible, SW_SHOW, SW_HIDE ) );
   x_callbackLeaveDll();
   x_callbackRaisePending();
}

/* Close a window */
void x_guiDestroy( HWND window )
{
   x_callbackEnterDll();
   x_guiDestroyWindow( window );
   x_callbackLeaveDll();
   x_callbackRaisePending();
}

/* The shortcut of an active menu command runs the command */
static int s_shortcut( const MSG *message )
{
   HWND root;
   HMENU menu;
   UINT modifiers = 0, state;
   size_t i;

   if( x_guiReading || ( message->message != WM_KEYDOWN && message->message != WM_SYSKEYDOWN ) )
      return 0;

   root = GetAncestor( message->hwnd, GA_ROOT );
   if( root == NULL || ( ATOM )GetClassLongPtrW( root, GCW_ATOM ) != x_guiWindowClass )
      return 0;

   menu = GetMenu( root );
   if( menu == NULL )
      return 0;

   modifiers |= IIF( GetKeyState( VK_CONTROL ) < 0, X_GUI_CTRL, 0 );
   modifiers |= IIF( GetKeyState( VK_SHIFT ) < 0, X_GUI_SHIFT, 0 );
   modifiers |= IIF( GetKeyState( VK_MENU ) < 0, X_GUI_ALT, 0 );
   for( i = 0; i < x_guiActionCount; ++i )
   {
      if( x_guiActions[i].key != 0 && x_guiActions[i].key == ( UINT )message->wParam && x_guiActions[i].modifiers == modifiers )
      {
         state = GetMenuState( menu, ( UINT )x_guiActions[i].id, MF_BYCOMMAND );
         if( state == ( UINT )-1 || ( state & ( MF_DISABLED | MF_GRAYED ) ) )
            continue;

         SendMessageW( root, WM_COMMAND, MAKEWPARAM( x_guiActions[i].id, 1 ), 0 );
         return 1;
      }
   }

   return 0;
}

/* A key of SetKey() of the window with the focus or of a window around it, the nearest first, runs
   its block; it comes before the shortcuts of the menu and of the toolbars */
static int s_windowKey( const MSG *message )
{
   X_ITEM result __attribute__( ( cleanup( x_itemClear ) ) ) = X_ITEM_NIL_INIT;
   UINT modifiers = 0;
   int64_t values[1];
   HWND current;
   size_t i;

   if( x_guiReading || ( message->message != WM_KEYDOWN && message->message != WM_SYSKEYDOWN ) )
      return 0;

   modifiers |= IIF( GetKeyState( VK_CONTROL ) < 0, X_GUI_CTRL, 0 );
   modifiers |= IIF( GetKeyState( VK_SHIFT ) < 0, X_GUI_SHIFT, 0 );
   modifiers |= IIF( GetKeyState( VK_MENU ) < 0, X_GUI_ALT, 0 );
   for( current = message->hwnd; current != NULL; current = GetParent( current ) )
   {
      const X_GUI_KEYS *keys = ( const X_GUI_KEYS * )GetPropW( current, x_guiKeysProperty );
      X_ITEM *object = ( X_ITEM * )GetWindowLongPtrW( current, GWLP_USERDATA );

      if( keys != NULL && object != NULL && ( ATOM )GetClassLongPtrW( current, GCW_ATOM ) == x_guiWindowClass )
      {
         for( i = 0; i < keys->count; ++i )
         {
            if( keys->keys[i] == ( UINT )message->wParam && keys->modifiers[i] == modifiers )
            {
               values[0] = ( int64_t )i + 1;
               x_guiNotifyEvent( object, X_GUI_EVENT_KEY, &result, 1, values );
               return 1;
            }
         }
      }

      if( !( GetWindowLongPtrW( current, GWL_STYLE ) & WS_CHILD ) )
         break;
   }

   return 0;
}

/* Look for the key among the buttons of one toolbar */
static BOOL CALLBACK s_toolbarKeyChild( HWND child, LPARAM data )
{
   X_GUI_KEYHIT *hit = ( X_GUI_KEYHIT * )data;
   const X_GUI_BAR *bar = x_guiToolbarOf( child );
   size_t i;

   if( bar == NULL || !IsWindowVisible( child ) || !IsWindowEnabled( child ) )
      return TRUE;

   for( i = 0; i < bar->count; ++i )
   {
      if( bar->keys[i] != 0 && bar->keys[i] == hit->key && bar->modifiers[i] == hit->modifiers )
      {
         hit->owned = 1;
         if( SendMessageW( child, TB_ISBUTTONENABLED, ( WPARAM )( i + 1 ), 0 ) )
         {
            hit->toolbar = child;
            hit->button = ( int )i + 1;
            return FALSE;
         }
      }
   }

   return TRUE;
}

/* The first visible sidebar of the window */
static BOOL CALLBACK s_findSidebar( HWND child, LPARAM found )
{
   if( x_guiSidebarOf( child ) == NULL || !IsWindowVisible( child ) )
      return TRUE;

   *( HWND * )found = child;

   return FALSE;
}

/* The shortcut of a toolbar button of a visible toolbar clicks the button; a key without Ctrl and Alt
   stays with an edit field, so Del there deletes a character; the key of a disabled button is taken
   too, so F10 of a disabled button does not open the menu of the window */
static int s_toolbarKey( const MSG *message )
{
   X_GUI_KEYHIT hit = { ( UINT )message->wParam, 0, NULL, 0, 0 };
   HWND root = GetAncestor( message->hwnd, GA_ROOT );
   HWND sidebar = NULL;
   int function;

   if( x_guiReading || ( message->message != WM_KEYDOWN && message->message != WM_SYSKEYDOWN ) )
      return 0;

   if( root == NULL || ( ATOM )GetClassLongPtrW( root, GCW_ATOM ) != x_guiWindowClass )
      return 0;

   hit.modifiers |= IIF( GetKeyState( VK_CONTROL ) < 0, X_GUI_CTRL, 0 );
   hit.modifiers |= IIF( GetKeyState( VK_SHIFT ) < 0, X_GUI_SHIFT, 0 );
   hit.modifiers |= IIF( GetKeyState( VK_MENU ) < 0, X_GUI_ALT, 0 );
   function = hit.key >= VK_F1 && hit.key <= VK_F24;
   if( !( hit.modifiers & ( X_GUI_CTRL | X_GUI_ALT ) ) && !function && x_guiHasClass( message->hwnd, L"Edit" ) )
      return 0;

   EnumChildWindows( root, s_toolbarKeyChild, ( LPARAM )&hit );
   if( hit.toolbar == NULL && !hit.owned && hit.key == 'B' && hit.modifiers == X_GUI_CTRL )
   {
      EnumChildWindows( root, s_findSidebar, ( LPARAM )&sidebar );
      return sidebar != NULL && x_guiSidebarToggle( sidebar );
   }

   if( hit.toolbar == NULL )
      return hit.owned;

   SendMessageW( GetParent( hit.toolbar ), WM_COMMAND, MAKEWPARAM( hit.button, BN_CLICKED ), ( LPARAM )hit.toolbar );

   return 1;
}

/* The first visible tab control of the window */
static BOOL CALLBACK s_findTab( HWND child, LPARAM found )
{
   if( !x_guiHasClass( child, WC_TABCONTROLW ) || !IsWindowVisible( child ) )
      return TRUE;

   *( HWND * )found = child;

   return FALSE;
}

/* Ctrl+Tab and Ctrl+PgDn select the next page, with Shift or Ctrl+PgUp the previous one, as in property
   sheets: of the tab control holding the focus, otherwise of the first one in the window */
static int s_tabKey( const MSG *message )
{
   HWND tab = NULL;
   HWND root = GetAncestor( message->hwnd, GA_ROOT );
   HWND current, page;
   int count, index, back;
   int ctrl = GetKeyState( VK_CONTROL ) < 0 && GetKeyState( VK_MENU ) >= 0;
   int pageKey = message->wParam == VK_TAB || message->wParam == VK_PRIOR || message->wParam == VK_NEXT;

   if( message->message != WM_KEYDOWN || !ctrl || !pageKey )
      return 0;

   if( root == NULL || ( ATOM )GetClassLongPtrW( root, GCW_ATOM ) != x_guiWindowClass )
      return 0;

   for( current = message->hwnd; current != NULL && current != root && tab == NULL; current = GetParent( current ) )
   {
      if( x_guiHasClass( current, WC_TABCONTROLW ) )
         tab = current;
   }

   if( tab == NULL )
      EnumChildWindows( root, s_findTab, ( LPARAM )&tab );

   if( tab == NULL )
      return 0;

   count = ( int )SendMessageW( tab, TCM_GETITEMCOUNT, 0, 0 );
   if( count < 2 )
      return 0;

   back = message->wParam == VK_PRIOR || ( message->wParam == VK_TAB && GetKeyState( VK_SHIFT ) < 0 );
   index = ( int )SendMessageW( tab, TCM_GETCURSEL, 0, 0 );
   index = ( index + IIF( back, count - 1, 1 ) ) % count;
   SendMessageW( tab, TCM_SETCURFOCUS, ( WPARAM )index, 0 );
   for( page = GetWindow( tab, GW_CHILD ); page != NULL; page = GetWindow( page, GW_HWNDNEXT ) )
   {
      if( IsWindowVisible( page ) && ( ATOM )GetClassLongPtrW( page, GCW_ATOM ) == x_guiWindowClass )
         break;
   }

   current = IIF( page != NULL, GetNextDlgTabItem( page, NULL, FALSE ), NULL );
   SetFocus( IIF( current != NULL, current, tab ) );

   return 1;
}

/* Dispatch a message of the loop: keys of SetKey(), shortcuts and the Tab of dialogs first */
void x_guiDispatchMessage( MSG *message )
{
   HWND root = GetAncestor( message->hwnd, GA_ROOT );
   int arrow, own;

   if( s_windowKey( message ) || s_shortcut( message ) || s_toolbarKey( message ) || s_tabKey( message ) )
      return;

   /* An arrow in the window itself or in its status bar goes to the menu bar, as F10 does */
   arrow = message->message == WM_KEYDOWN && ( message->wParam == VK_LEFT || message->wParam == VK_RIGHT );
   arrow = arrow && ( message->hwnd == root || x_guiHasClass( message->hwnd, STATUSCLASSNAMEW ) );
   arrow = arrow && GetMenu( root ) != NULL && GetKeyState( VK_CONTROL ) >= 0 && GetKeyState( VK_MENU ) >= 0;
   if( arrow )
   {
      PostMessageW( root, WM_SYSCOMMAND, SC_KEYMENU, 0 );
      return;
   }

   own = root != NULL && ( ATOM )GetClassLongPtrW( root, GCW_ATOM ) == x_guiWindowClass;
   if( !own || !IsDialogMessageW( root, message ) )
   {
      TranslateMessage( message );
      DispatchMessageW( message );
   }
}

/* Show a dialog modal over its owner and run messages until it closes */
void x_guiDialogRun( X_CALL_CTX *ctx, HWND dialog )
{
   HWND owner = GetWindow( dialog, GW_OWNER );
   int previousRunning = x_guiRunning;
   MSG message;
   int status = 1;

   x_guiRunning = 1;
   x_callbackEnterDll();
   SetPropW( dialog, s_modalProperty, dialog );
   if( owner != NULL )
      EnableWindow( owner, FALSE );

   ShowWindow( dialog, SW_SHOW );
   UpdateWindow( dialog );
   while( IsWindow( dialog ) && ( status = ( int )GetMessageW( &message, NULL, 0, 0 ) ) > 0 )
      x_guiDispatchMessage( &message );

   if( IsWindow( dialog ) )
      x_guiDestroyWindow( dialog );

   if( status == 0 && previousRunning )
      PostQuitMessage( ( int )message.wParam );

   x_guiRunning = previousRunning;
   x_callbackLeaveDll();
   x_callbackRaisePending();

   if( status == -1 )
      x_guiRaiseWin32( ctx, "GetMessageW", GetLastError() );
}

/* Show the main window and run messages until it closes */
void x_guiRun( X_CALL_CTX *ctx, HWND window )
{
   HWND previousWindow = s_mainWindow;
   int previousRunning = x_guiRunning;
   MSG message;
   int status;

   s_mainWindow = window;
   x_guiRunning = 1;
   x_callbackEnterDll();
   ShowWindow( window, SW_SHOW );
   UpdateWindow( window );
   while( ( status = ( int )GetMessageW( &message, NULL, 0, 0 ) ) > 0 )
      x_guiDispatchMessage( &message );

   x_guiRunning = 0;
   if( IsWindow( window ) )
      DestroyWindow( window );

   s_mainWindow = IIF( IsWindow( previousWindow ), previousWindow, NULL );
   x_guiRunning = previousRunning;
   x_callbackLeaveDll();
   x_callbackRaisePending();

   if( status == -1 )
      x_guiRaiseWin32( ctx, "GetMessageW", GetLastError() );
   else
      x_retl( ctx, 1 );
}
