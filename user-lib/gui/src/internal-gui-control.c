/*
 * Copyright (c) 2024-2026 xDev-Language
 *
 * Library...: xDev GUI
 * File......: internal-gui-control.c
 *
 */

#include <string.h>
#include "internal-gui.h"

static int s_controlId = X_GUI_CONTROL_FIRST;
static int s_commonControls;

/* ---------------------------------------------------------------------------------------------
   Controls
   --------------------------------------------------------------------------------------------- */

/* Register the common controls of Windows once */
void x_guiInitCommonControls( void )
{
   INITCOMMONCONTROLSEX controls;

   if( s_commonControls )
      return;

   controls.dwSize = sizeof( controls );
   controls.dwICC = ICC_BAR_CLASSES | ICC_PROGRESS_CLASS | ICC_DATE_CLASSES | ICC_TAB_CLASSES;
   controls.dwICC |= ICC_LISTVIEW_CLASSES | ICC_TREEVIEW_CLASSES | ICC_COOL_CLASSES;
   InitCommonControlsEx( &controls );
   s_commonControls = 1;
}

/* Create the control of a control object through the X_GUI_EVENT_CREATE function of its class */
void x_guiControlSend( X_CALL_CTX *ctx, X_GUI_KIND kind, int textParam, int x, int y, int width, int height )
{
   X_ITEM values[7];
   X_ARG args[7];
   int i;

   for( i = 0; i < 7; ++i )
      values[i] = ( X_ITEM )X_ITEM_NIL_INIT;

   if( x_paritem( ctx, 1 ) != NULL )
      x_itemCopy( &values[0], x_paritem( ctx, 1 ) );

   x_itemPutNI( &values[1], ( int )kind );
   if( textParam != 0 && x_paritem( ctx, textParam ) != NULL && !X_ISNIL( ctx, textParam ) )
      x_itemCopy( &values[2], x_paritem( ctx, textParam ) );
   else
      x_itemPutC( &values[2], "" );

   x_itemPutNI( &values[3], x );
   x_itemPutNI( &values[4], y );
   x_itemPutNI( &values[5], width );
   x_itemPutNI( &values[6], height );
   for( i = 0; i < 7; ++i )
      args[i] = ( X_ARG ){ &values[i], 0 };

   x_guiHandlerCall( ctx->result, ctx->self, X_GUI_EVENT_CREATE, 7, args );
   for( i = 0; i < 7; ++i )
      x_itemClear( &values[i] );
}

/* New() of a control: parent, x, y, width and height, the text as the sixth parameter when it has one */
void x_guiControlNew( X_CALL_CTX *ctx, X_GUI_KIND kind, int width, int height, int hasText )
{
   int textParam = IIF( hasText, 6, 0 );
   int x = x_parnidef( ctx, 2, 0 );
   int y = x_parnidef( ctx, 3, 0 );

   x_guiControlSend( ctx, kind, textParam, x, y, x_parnidef( ctx, 4, width ), x_parnidef( ctx, 5, height ) );
}

/* A GET: with @var as the sixth parameter the field shows the variable and ReadModal() gives the value back to it,
   as a GET does; the variable is a LOCAL of the function that runs ReadModal() */
void x_guiGetNew( X_CALL_CTX *ctx )
{
   const int byRef = X_ISBYREF( ctx, 6 );
   const X_ITEM *var = x_paritem( ctx, 6 );
   const int rows = IIF( X_ISNUM( ctx, 8 ) && x_parni( ctx, 8 ) > 1, x_parni( ctx, 8 ), 1 );
   int length = 0;
   TEXTMETRICW metrics;
   HWND edit;
   HDC dc;
   HGDIOBJ old;
   UINT dpi;
   int chars, margin, edge, width, height;

   /* nMax characters, otherwise the length of the variable */
   if( X_ISNUM( ctx, 9 ) && x_parni( ctx, 9 ) > 0 )
      length = x_parni( ctx, 9 );
   else if( var != NULL && X_IS_STRING( var ) )
      length = ( int )x_itemGetCLen( var );

   /* With nRows the field has one row and grows to nRows while it has the focus, see x_guiGetFit() */
   x_guiControlNew( ctx, IIF( rows > 1, X_GUI_GETMEMO, X_GUI_EDIT ), 200, 23, !byRef );
   edit = x_guiObjectHandle( ctx, ctx->self );
   if( edit == NULL )
      return;

   dpi = GetDpiForWindow( edit );
   margin = x_guiScale( 2, dpi );
   edge = GetSystemMetricsForDpi( SM_CXEDGE, dpi );
   SetPropW( edit, x_guiGetProperty, ( HANDLE )1 );
   if( rows > 1 )
      SetPropW( edit, x_guiRowsProperty, ( HANDLE )( INT_PTR )rows );

   x_guiXfontAttach( edit );
   SendMessageW( edit, WM_SETFONT, ( WPARAM )x_guiFontOf( edit, dpi ), FALSE );
   SendMessageW( edit, EM_SETMARGINS, EC_LEFTMARGIN | EC_RIGHTMARGIN, MAKELPARAM( margin, margin ) );
   if( byRef )
   {
      SetPropW( edit, x_guiVarProperty, ( HANDLE )var );
      if( var != NULL && !X_IS_NIL( var ) )
         x_guiEditSetValue( ctx, edit, var );
   }

   /* PICTURE; without it no more characters than the variable has, as a GET */
   if( X_ISCHAR( ctx, 7 ) )
      x_guiEditSetPicture( ctx, edit, x_paritem( ctx, 7 ) );
   else if( length > 0 )
      SendMessageW( edit, EM_LIMITTEXT, ( WPARAM )length, 0 );

   /* nLen characters are seen, without it the whole variable; a longer value scrolls in the field */
   if( X_ISNUM( ctx, 4 ) )
      chars = x_parni( ctx, 4 );
   else
      chars = IIF( length > 0, length, GetWindowTextLengthW( edit ) );

   dc = GetDC( edit );
   old = SelectObject( dc, x_guiFontOf( edit, dpi ) );
   GetTextMetricsW( dc, &metrics );
   SelectObject( dc, old );
   ReleaseDC( edit, dc );
   width = IIF( chars > 0, chars, 10 ) * metrics.tmAveCharWidth + 2 * ( margin + edge ) + 1;
   height = IIF( X_ISNUM( ctx, 5 ), x_guiScale( x_parni( ctx, 5 ), dpi ), metrics.tmHeight + 2 * edge + margin );
   SetWindowPos( edit, NULL, 0, 0, width, height, SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE );
   x_guiReturnSelf( ctx );
}

/* A SAY: nLen characters of the fixed font 9x18 wide as a GET of nLen characters, so a SAY and a GET from the
   same column end in the same place; without nLen as long as the text */
void x_guiSayNew( X_CALL_CTX *ctx )
{
   X_GUI_OWNFONT own;
   TEXTMETRICW metrics;
   HWND label;
   HFONT font;
   HDC dc;
   HGDIOBJ old;
   UINT dpi;
   int chars, margin, edge, width, height;

   x_guiControlNew( ctx, X_GUI_LABEL, 200, 20, 1 );
   label = x_guiObjectHandle( ctx, ctx->self );
   if( label == NULL )
      return;

   dpi = GetDpiForWindow( label );
   margin = x_guiScale( 2, dpi );
   edge = GetSystemMetricsForDpi( SM_CXEDGE, dpi );
   memset( &own, 0, sizeof( own ) );
   lstrcpynW( own.name, x_guiXfontName, LF_FACESIZE );
   font = x_guiOwnFontCreate( &own, dpi );
   dc = GetDC( label );
   old = SelectObject( dc, font );
   GetTextMetricsW( dc, &metrics );
   SelectObject( dc, old );
   ReleaseDC( label, dc );
   DeleteObject( font );
   chars = IIF( X_ISNUM( ctx, 4 ), x_parni( ctx, 4 ), GetWindowTextLengthW( label ) );
   width = IIF( chars > 0, chars, 10 ) * metrics.tmAveCharWidth + 2 * ( margin + edge ) + 1;
   height = IIF( X_ISNUM( ctx, 5 ), x_guiScale( x_parni( ctx, 5 ), dpi ), metrics.tmHeight + 2 * edge + margin );
   SetWindowPos( label, NULL, 0, 0, width, height, SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE );
   x_guiReturnSelf( ctx );
}

/* X_GUI_EVENT_CREATE: create the window of a control object in its parent */
void x_guiControlCreate( X_CALL_CTX *ctx )
{
   X_ITEM *handleSlot = x_objDataSlot( ctx, 1 );
   X_ITEM *parentSlot = x_objDataSlot( ctx, 2 );
   X_ITEM *parentItem = ( X_ITEM * )x_paritem( ctx, 1 );
   const int kind = x_parni( ctx, 2 );
   const wchar_t *className = L"STATIC";
   DWORD style = WS_CHILD | WS_VISIBLE;
   DWORD exStyle = 0;
   wchar_t *text;
   HWND parent, control;
   const DWORD listStyle = LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES;
   HINSTANCE instance = GetModuleHandleW( NULL );
   UINT dpi;
   DWORD error;
   HMENU id;
   int valid, x, y, cx, cy;

   valid = handleSlot != NULL && parentSlot != NULL && parentItem != NULL && x_objIsKindOf( parentItem, "GUIWINDOW" );
   parent = IIF( valid, x_guiObjectHandle( ctx, parentItem ), NULL );
   valid = parent != NULL && IsWindow( parent ) && x_parni( ctx, 6 ) >= 0 && x_parni( ctx, 7 ) >= 0;
   if( !valid )
   {
      x_guiRaise( ctx, "GUICONTROL:NEW" );
      return;
   }

   switch( kind )
   {
   case X_GUI_LABEL:
      style |= SS_LEFT;
      break;
   /* Edit fields do not paint over a GET of several rows that grows over them */
   case X_GUI_EDIT:
      className = L"EDIT";
      style |= WS_TABSTOP | WS_CLIPSIBLINGS | ES_AUTOHSCROLL;
      exStyle = WS_EX_CLIENTEDGE;
      break;
   case X_GUI_MEMO:
      className = L"EDIT";
      style |= WS_TABSTOP | WS_VSCROLL | ES_MULTILINE | ES_AUTOVSCROLL | ES_WANTRETURN;
      exStyle = WS_EX_CLIENTEDGE;
      break;
   /* A GET of several rows: the text wraps, without a scroll bar; Enter goes to the next field */
   case X_GUI_GETMEMO:
      className = L"EDIT";
      style |= WS_TABSTOP | WS_CLIPSIBLINGS | ES_MULTILINE | ES_AUTOVSCROLL;
      exStyle = WS_EX_CLIENTEDGE;
      break;
   case X_GUI_BUTTON:
      className = L"BUTTON";
      style |= WS_TABSTOP | BS_PUSHBUTTON;
      break;
   case X_GUI_CHECKBOX:
      className = L"BUTTON";
      style |= WS_TABSTOP | BS_AUTOCHECKBOX;
      break;
   case X_GUI_RADIO:
      className = L"BUTTON";
      style |= WS_TABSTOP | BS_AUTORADIOBUTTON;
      break;
   /* A frame does not paint over the controls lying in it */
   case X_GUI_GROUPBOX:
      className = L"BUTTON";
      style |= BS_GROUPBOX | WS_CLIPSIBLINGS;
      break;
   case X_GUI_LISTBOX:
      className = L"LISTBOX";
      style |= WS_TABSTOP | WS_VSCROLL | LBS_NOTIFY | LBS_NOINTEGRALHEIGHT;
      exStyle = WS_EX_CLIENTEDGE;
      break;
   case X_GUI_COMBOBOX:
      className = L"COMBOBOX";
      style |= WS_TABSTOP | WS_VSCROLL | CBS_DROPDOWNLIST;
      break;
   case X_GUI_PROGRESSBAR:
      x_guiInitCommonControls();
      className = PROGRESS_CLASSW;
      break;
   case X_GUI_STATUSBAR:
      x_guiInitCommonControls();
      className = STATUSCLASSNAMEW;
      style |= SBARS_SIZEGRIP | CCS_BOTTOM;
      break;
   case X_GUI_DATEPICKER:
      x_guiInitCommonControls();
      className = DATETIMEPICK_CLASSW;
      style |= WS_TABSTOP | DTS_SHORTDATEFORMAT;
      break;
   case X_GUI_SLIDER:
      x_guiInitCommonControls();
      className = TRACKBAR_CLASSW;
      style |= WS_TABSTOP | TBS_HORZ;
      break;
   case X_GUI_TABCONTROL:
      x_guiInitCommonControls();
      className = WC_TABCONTROLW;
      style |= WS_TABSTOP | WS_CLIPSIBLINGS;
      exStyle = WS_EX_CONTROLPARENT;
      break;
   case X_GUI_LISTVIEW:
      x_guiInitCommonControls();
      className = WC_LISTVIEWW;
      style |= WS_TABSTOP | LVS_REPORT | LVS_SINGLESEL | LVS_SHOWSELALWAYS;
      exStyle = WS_EX_CLIENTEDGE;
      break;
   case X_GUI_TREEVIEW:
      x_guiInitCommonControls();
      className = WC_TREEVIEWW;
      style |= WS_TABSTOP | TVS_HASLINES | TVS_HASBUTTONS | TVS_LINESATROOT | TVS_SHOWSELALWAYS;
      exStyle = WS_EX_CLIENTEDGE;
      break;
   case X_GUI_TOOLBAR:
      x_guiInitCommonControls();
      className = TOOLBARCLASSNAMEW;
      style |= TBSTYLE_FLAT | TBSTYLE_LIST | TBSTYLE_TOOLTIPS | WS_CLIPSIBLINGS;
      style |= CCS_NOPARENTALIGN | CCS_NORESIZE | CCS_NODIVIDER;
      break;
   case X_GUI_BROWSE:
      if( !x_guiRegisterBrowseClass( instance ) )
      {
         x_guiRaiseWin32( ctx, "RegisterClassExW", GetLastError() );
         return;
      }

      className = x_guiBrowseClassName;
      style |= WS_TABSTOP | WS_VSCROLL | WS_HSCROLL;
      exStyle = WS_EX_CLIENTEDGE;
      break;
   default:
      x_guiRaise( ctx, "GUICONTROL:NEW" );
      return;
   }

   text = x_guiText( ctx, x_paritem( ctx, 3 ), "GUICONTROL:NEW" );
   if( text == NULL )
      return;

   if( kind == X_GUI_MEMO )
      text = x_guiCrlf( text );

   if( kind != X_GUI_RADIO || !x_guiIsRadio( GetWindow( GetWindow( parent, GW_CHILD ), GW_HWNDLAST ) ) )
      style |= WS_GROUP;

   dpi = GetDpiForWindow( parent );
   x_callbackEnterDll();
   x = x_guiScale( x_parni( ctx, 4 ), dpi );
   y = x_guiScale( x_parni( ctx, 5 ), dpi );
   cx = x_guiScale( x_parni( ctx, 6 ), dpi );
   cy = x_guiScale( x_parni( ctx, 7 ), dpi );
   id = ( HMENU )( INT_PTR )s_controlId;
   control = CreateWindowExW( exStyle, className, text, style, x, y, cx, cy, parent, id, instance, NULL );
   error = GetLastError();
   s_controlId = IIF( s_controlId >= X_GUI_CONTROL_LAST, X_GUI_CONTROL_FIRST, s_controlId + 1 );
   if( control != NULL )
   {
      SetWindowLongPtrW( control, GWLP_USERDATA, ( LONG_PTR )x_itemNew( ctx->self ) );
      SendMessageW( control, WM_SETFONT, ( WPARAM )x_guiFont( dpi ), FALSE );
      if( kind == X_GUI_LISTVIEW )
         SendMessageW( control, LVM_SETEXTENDEDLISTVIEWSTYLE, listStyle, listStyle );
      else if( kind == X_GUI_TOOLBAR )
         x_guiToolbarInit( control );
      else if( kind == X_GUI_EDIT || kind == X_GUI_GETMEMO )
         SetWindowSubclass( control, x_guiEditProc, 1, 0 );
      else if( kind == X_GUI_STATUSBAR )
         SetWindowSubclass( control, x_guiStatusProc, 1, 0 );
      else if( kind == X_GUI_BROWSE )
         x_guiXfontAttach( control );
      else if( kind == X_GUI_GROUPBOX || kind == X_GUI_CHECKBOX || kind == X_GUI_RADIO )
      {
         if( kind == X_GUI_GROUPBOX )
            SetWindowSubclass( control, x_guiGroupProc, 1, 0 );

         x_guiPaintTheme( control );
      }
   }

   x_callbackLeaveDll();
   x_xfree( text );
   x_callbackRaisePending();

   if( control == NULL )
   {
      x_guiRaiseWin32( ctx, "CreateWindowExW", error );
      return;
   }

   x_itemPutPtr( handleSlot, control );
   x_itemCopy( parentSlot, parentItem );
   x_guiReturnSelf( ctx );
}

/* Set the text of a window or a control; an edit field of several lines gets CR LF line ends */
void x_guiSetText( X_CALL_CTX *ctx, HWND window, const X_ITEM *value )
{
   wchar_t *text = x_guiText( ctx, value, "GUICONTROL:TEXT" );
   BOOL done;
   DWORD error;

   if( text == NULL )
      return;

   if( x_guiIsMultiline( window ) )
      text = x_guiCrlf( text );

   x_callbackEnterDll();
   done = SetWindowTextW( window, text );
   error = GetLastError();
   x_callbackLeaveDll();
   x_xfree( text );
   x_callbackRaisePending();
   if( !done )
      x_guiRaiseWin32( ctx, "SetWindowTextW", error );
}

/* A data of a gui_Font object */
static int s_fontValue( X_CALL_CTX *ctx, const X_ITEM *font, const char *name, X_ITEM *value )
{
   x_itemClear( value );

   return x_objSend( ctx, value, ( X_ITEM * )font, name, 0, NULL );
}

/* A gui_Font gives the control its own font, NIL returns it to the system font */
void x_guiSetFont( X_CALL_CTX *ctx, HWND window, const X_ITEM *value )
{
   X_ITEM item __attribute__( ( cleanup( x_itemClear ) ) ) = X_ITEM_NIL_INIT;
   X_GUI_OWNFONT *own = NULL;
   X_GUI_GRID *grid = x_guiGridOf( window );
   wchar_t *name;

   if( value != NULL && !X_IS_NIL( value ) && !x_objIsKindOf( value, "GUIFONT" ) )
   {
      x_guiRaise( ctx, "GUICONTROL:FONT" );
      return;
   }

   if( value != NULL && !X_IS_NIL( value ) )
   {
      own = ( X_GUI_OWNFONT * )x_xgrab( sizeof( X_GUI_OWNFONT ) );
      memset( own, 0, sizeof( *own ) );
      if( s_fontValue( ctx, value, "SIZE", &item ) && X_IS_NUMERIC( &item ) && x_itemGetNI( &item ) > 0 )
         own->size = x_itemGetNI( &item );

      if( s_fontValue( ctx, value, "BOLD", &item ) && X_IS_LOGICAL( &item ) )
         own->bold = item.u.logical;

      if( s_fontValue( ctx, value, "ITALIC", &item ) && X_IS_LOGICAL( &item ) )
         own->italic = item.u.logical;

      if( s_fontValue( ctx, value, "NAME", &item ) && X_IS_STRING( &item ) && x_itemGetCLen( &item ) > 0 )
      {
         name = x_guiText( ctx, &item, "GUICONTROL:FONT" );
         if( name != NULL )
         {
            lstrcpynW( own->name, name, LF_FACESIZE );
            x_xfree( name );
         }
      }
   }

   x_callbackEnterDll();
   x_guiFontRelease( window );
   if( own != NULL )
      SetPropW( window, x_guiFontProperty, own );

   SendMessageW( window, WM_SETFONT, ( WPARAM )x_guiFontOf( window, GetDpiForWindow( window ) ), TRUE );
   if( grid != NULL )
      grid->valid = 0;

   InvalidateRect( window, NULL, TRUE );
   x_callbackLeaveDll();
   x_callbackRaisePending();
}

/* An edit field of a combo box holds the focus for it, so the focus may be in a child of the control */
int x_guiFocused( HWND window )
{
   HWND focus = GetFocus();

   return focus != NULL && ( focus == window || IsChild( window, focus ) );
}

/* Show or hide a control */
void x_guiSetVisible( HWND window, int visible )
{
   x_guiShow( window, visible );
}

/* Enable or disable a control */
void x_guiSetEnabled( HWND window, int enabled )
{
   x_callbackEnterDll();
   EnableWindow( window, enabled );
   x_callbackLeaveDll();
   x_callbackRaisePending();
}

/* Give a control the focus */
void x_guiSetFocus( HWND window )
{
   x_callbackEnterDll();
   SetFocus( window );
   x_callbackLeaveDll();
   x_callbackRaisePending();
}

/* The anchored sides of a control and its distances from the sides of its parent */
static void s_anchorMeasure( HWND window, X_GUI_ANCHOR *anchor, int flags )
{
   RECT frame, client;
   int right, bottom;

   x_guiChildFrame( window, &frame );
   GetClientRect( GetParent( window ), &client );
   right = client.right - frame.right;
   bottom = client.bottom - frame.bottom;
   *anchor = ( X_GUI_ANCHOR ){ flags, frame.left, frame.top, right, bottom };
}

/* Return the anchored sides as "LTRB" letters */
void x_guiGetAnchor( X_CALL_CTX *ctx, HWND window )
{
   const X_GUI_ANCHOR *anchor = ( const X_GUI_ANCHOR * )GetPropW( window, x_guiAnchorProperty );
   char text[5];
   int length = 0;

   if( anchor == NULL )
   {
      x_retc( ctx, "LT" );
      return;
   }

   if( anchor->flags & X_GUI_ANCHOR_LEFT )
      text[length++] = 'L';

   if( anchor->flags & X_GUI_ANCHOR_TOP )
      text[length++] = 'T';

   if( anchor->flags & X_GUI_ANCHOR_RIGHT )
      text[length++] = 'R';

   if( anchor->flags & X_GUI_ANCHOR_BOTTOM )
      text[length++] = 'B';

   text[length] = '\0';
   x_retc( ctx, text );
}

/* Anchor a control to sides of its parent given as "LTRB" letters */
void x_guiSetAnchor( X_CALL_CTX *ctx, HWND window, const X_ITEM *value )
{
   const char *spec;
   size_t length = 0;
   void *handle = NULL;
   int flags = 0;
   X_GUI_ANCHOR *anchor;
   size_t i;

   spec = IIF( value != NULL && X_IS_STRING( value ), x_itemGetStrUTF8( value, &handle, &length ), NULL );
   for( i = 0; spec != NULL && i < length; ++i )
   {
      switch( spec[i] )
      {
      case 'L': case 'l': flags |= X_GUI_ANCHOR_LEFT; break;
      case 'T': case 't': flags |= X_GUI_ANCHOR_TOP; break;
      case 'R': case 'r': flags |= X_GUI_ANCHOR_RIGHT; break;
      case 'B': case 'b': flags |= X_GUI_ANCHOR_BOTTOM; break;
      default: spec = NULL; break;
      }
   }

   x_strfree( handle );
   if( spec == NULL )
   {
      x_guiRaise( ctx, "GUICONTROL:ANCHOR" );
      return;
   }

   anchor = ( X_GUI_ANCHOR * )GetPropW( window, x_guiAnchorProperty );
   if( anchor == NULL )
   {
      anchor = ( X_GUI_ANCHOR * )x_xgrab( sizeof( *anchor ) );
      if( !SetPropW( window, x_guiAnchorProperty, anchor ) )
      {
         x_xfree( anchor );
         x_guiRaiseWin32( ctx, "SetPropW", GetLastError() );
         return;
      }
   }

   s_anchorMeasure( window, anchor, flags );
}

/* The position or the size of a control at 96 DPI */
int x_guiBounds( HWND window, X_GUI_BOUND part )
{
   RECT frame;
   int value;

   x_guiChildFrame( window, &frame );
   switch( part )
   {
   case X_GUI_X: value = frame.left; break;
   case X_GUI_Y: value = frame.top; break;
   case X_GUI_WIDTH: value = frame.right - frame.left; break;
   default: value = frame.bottom - frame.top; break;
   }

   return MulDiv( value, USER_DEFAULT_SCREEN_DPI, ( int )GetDpiForWindow( window ) );
}

/* Set the position or the size of a control, given at 96 DPI; its anchor follows */
void x_guiSetBounds( X_CALL_CTX *ctx, HWND window, X_GUI_BOUND part, int value )
{
   const int scaled = x_guiScale( value, GetDpiForWindow( window ) );
   X_GUI_ANCHOR *anchor = ( X_GUI_ANCHOR * )GetPropW( window, x_guiAnchorProperty );
   RECT frame;
   int x, y, width, height;

   if( value < 0 && ( part == X_GUI_WIDTH || part == X_GUI_HEIGHT ) )
   {
      x_guiRaise( ctx, "GUICONTROL:SIZE" );
      return;
   }

   x_guiChildFrame( window, &frame );
   x = IIF( part == X_GUI_X, scaled, frame.left );
   y = IIF( part == X_GUI_Y, scaled, frame.top );
   width = IIF( part == X_GUI_WIDTH, scaled, frame.right - frame.left );
   height = IIF( part == X_GUI_HEIGHT, scaled, frame.bottom - frame.top );
   x_callbackEnterDll();
   SetWindowPos( window, NULL, x, y, width, height, SWP_NOZORDER | SWP_NOACTIVATE );
   if( x_guiHasClass( window, WC_TABCONTROLW ) )
      x_guiTabLayout( window, 0 );

   x_callbackLeaveDll();
   if( anchor != NULL )
      s_anchorMeasure( window, anchor, anchor->flags );

   x_callbackRaisePending();
}

/* Send a message to a control between x_callbackEnterDll() and x_callbackLeaveDll() */
LRESULT x_guiSend( HWND window, UINT message, WPARAM wParam, LPARAM lParam )
{
   LRESULT result;

   x_callbackEnterDll();
   result = SendMessageW( window, message, wParam, lParam );
   x_callbackLeaveDll();
   x_callbackRaisePending();

   return result;
}

/* Set the tool tip of a control; the window keeps one tool tip control for all */
void x_guiSetToolTip( X_CALL_CTX *ctx, HWND control, const X_ITEM *value )
{
   HWND root = GetAncestor( control, GA_ROOT );
   HWND tip = ( HWND )GetPropW( root, x_guiToolTipProperty );
   wchar_t *text = x_guiText( ctx, value, "GUICONTROL:TOOLTIP" );
   TTTOOLINFOW info;

   if( text == NULL )
      return;

   x_callbackEnterDll();
   if( tip == NULL )
   {
      DWORD exStyle = WS_EX_TOPMOST;
      DWORD style = WS_POPUP | TTS_ALWAYSTIP | TTS_NOPREFIX;
      HINSTANCE instance = GetModuleHandleW( NULL );
      int pos = CW_USEDEFAULT;

      x_guiInitCommonControls();
      tip = CreateWindowExW( exStyle, TOOLTIPS_CLASSW, NULL, style, pos, pos, pos, pos, root, NULL, instance, NULL );
      if( tip != NULL )
         SetPropW( root, x_guiToolTipProperty, tip );
   }

   if( tip != NULL )
   {
      memset( &info, 0, sizeof( info ) );
      info.cbSize = sizeof( info );
      info.uFlags = TTF_IDISHWND | TTF_SUBCLASS;
      info.hwnd = root;
      info.uId = ( UINT_PTR )control;
      info.lpszText = text;
      SendMessageW( tip, TTM_DELTOOLW, 0, ( LPARAM )&info );
      if( text[0] != L'\0' )
         SendMessageW( tip, TTM_ADDTOOLW, 0, ( LPARAM )&info );
   }

   x_callbackLeaveDll();
   x_xfree( text );
   x_callbackRaisePending();
   if( tip == NULL )
      x_guiRaiseWin32( ctx, "CreateWindowExW", GetLastError() );
}

/* Whether an edit field is read-only */
int x_guiReadOnly( HWND window )
{
   return ( GetWindowLongPtrW( window, GWL_STYLE ) & ES_READONLY ) != 0;
}

/* Make an edit field read-only or editable */
void x_guiSetReadOnly( HWND window, int readOnly )
{
   x_guiSend( window, EM_SETREADONLY, ( WPARAM )IIF( readOnly, TRUE, FALSE ), 0 );
}

/* The most characters an edit field takes */
int x_guiMaxLength( HWND window )
{
   return ( int )SendMessageW( window, EM_GETLIMITTEXT, 0, 0 );
}

/* Set the most characters an edit field takes */
void x_guiSetMaxLength( HWND window, int length )
{
   x_guiSend( window, EM_SETLIMITTEXT, ( WPARAM )length, 0 );
}

/* Whether a check box or a radio button is checked */
int x_guiGetCheck( HWND window )
{
   return x_guiSend( window, BM_GETCHECK, 0, 0 ) == BST_CHECKED;
}

/* Check or clear a check box or a radio button; a checked radio button clears the others of its group */
void x_guiSetCheck( HWND window, int checked )
{
   HWND parent = GetParent( window );
   HWND sibling;

   if( checked && x_guiIsRadio( window ) )
   {
      sibling = GetNextDlgGroupItem( parent, window, FALSE );
      while( sibling != NULL && sibling != window )
      {
         if( x_guiIsRadio( sibling ) )
            x_guiSend( sibling, BM_SETCHECK, BST_UNCHECKED, 0 );

         sibling = GetNextDlgGroupItem( parent, sibling, FALSE );
      }
   }

   x_guiSend( window, BM_SETCHECK, IIF( checked, BST_CHECKED, BST_UNCHECKED ), 0 );
}

/* Whether a button is the default or, with cancel, the cancel button of its window */
int x_guiButtonRole( HWND button, int cancel )
{
   return ( HWND )GetPropW( GetParent( button ), IIF( cancel, x_guiCancelProperty, x_guiDefaultProperty ) ) == button;
}

/* Make a button the default or the cancel button of its window, or take the role away */
void x_guiSetButtonRole( HWND button, int cancel, int enabled )
{
   const wchar_t *property = IIF( cancel, x_guiCancelProperty, x_guiDefaultProperty );
   HWND parent = GetParent( button );
   HWND previous = ( HWND )GetPropW( parent, property );

   if( !cancel && previous != NULL && IsWindow( previous ) && ( previous != button || !enabled ) )
      x_guiSend( previous, BM_SETSTYLE, BS_PUSHBUTTON, TRUE );

   if( enabled )
      SetPropW( parent, property, button );
   else if( previous == button )
      RemovePropW( parent, property );

   if( !cancel && enabled )
      x_guiSend( button, BM_SETSTYLE, BS_DEFPUSHBUTTON, TRUE );
}
