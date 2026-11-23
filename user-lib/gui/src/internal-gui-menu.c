/*
 * Copyright (c) 2024-2026 xDev-Language
 *
 * Library...: xDev GUI
 * File......: internal-gui-menu.c
 *
 */

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wctype.h>
#include "internal-gui.h"

/* A key name of a shortcut, its virtual key and the text shown in menus */
typedef struct
{
   const char *name;
   UINT key;
   const char *display;
} X_GUI_KEYNAME;

static const X_GUI_KEYNAME s_keyNames[] = {
   { "DEL", VK_DELETE, "Del" }, { "DELETE", VK_DELETE, "Del" }, { "INS", VK_INSERT, "Ins" },
   { "INSERT", VK_INSERT, "Ins" }, { "HOME", VK_HOME, "Home" }, { "END", VK_END, "End" },
   { "PGUP", VK_PRIOR, "PgUp" }, { "PAGEUP", VK_PRIOR, "PgUp" }, { "PGDN", VK_NEXT, "PgDn" },
   { "PAGEDOWN", VK_NEXT, "PgDn" }, { "ESC", VK_ESCAPE, "Esc" }, { "ESCAPE", VK_ESCAPE, "Esc" },
   { "ENTER", VK_RETURN, "Enter" }, { "RETURN", VK_RETURN, "Enter" }, { "TAB", VK_TAB, "Tab" },
   { "SPACE", VK_SPACE, "Space" }, { "BACKSPACE", VK_BACK, "Backspace" }, { "UP", VK_UP, "Up" },
   { "DOWN", VK_DOWN, "Down" }, { "LEFT", VK_LEFT, "Left" }, { "RIGHT", VK_RIGHT, "Right" }
};
static int s_menuId = X_GUI_MENU_FIRST;
static int s_menuImageSize = 16;
static HFONT s_menuFontHandle;
static UINT s_menuFontDpi;

/* oWindow:<cName>: the first control among all children of the window with this :name, upper and lower case alike */
typedef struct
{
   X_CALL_CTX *ctx;
   const char *name;
   int found;
} X_GUI_FIND;

/* ---------------------------------------------------------------------------------------------
   Menus
   --------------------------------------------------------------------------------------------- */

/* A menu bar, or with popup a drop-down menu; the kind is kept in the menu data */
HMENU x_guiMenuCreate( X_CALL_CTX *ctx, int popup )
{
   HMENU menu = IIF( popup, CreatePopupMenu(), CreateMenu() );
   MENUINFO info = { sizeof( info ), MIM_MENUDATA, 0, 0, NULL, 0, ( ULONG_PTR )( popup != 0 ) };

   if( menu == NULL )
      x_guiRaiseWin32( ctx, "CreateMenu", GetLastError() );
   else
      SetMenuInfo( menu, &info );

   return menu;
}

/* Whether a menu is a drop-down menu */
static int s_isPopupMenu( HMENU menu )
{
   MENUINFO info = { sizeof( info ), MIM_MENUDATA, 0, 0, NULL, 0, 0 };

   return GetMenuInfo( menu, &info ) && info.dwMenuData != 0;
}

/* Turn the extended style of menus on or off and set the size of their pictures; returns the previous style */
int x_guiMenuStyle( int extended, int imageSize )
{
   int previous = x_guiMenuExtended;

   x_guiMenuExtended = extended;
   if( imageSize > 0 )
      s_menuImageSize = imageSize;

   return previous;
}

/* The menu font for dpi, made again when the DPI changes */
static HFONT s_menuFont( UINT dpi )
{
   NONCLIENTMETRICSW metrics;

   if( s_menuFontHandle != NULL && s_menuFontDpi == dpi )
      return s_menuFontHandle;

   memset( &metrics, 0, sizeof( metrics ) );
   metrics.cbSize = sizeof( metrics );
   if( !SystemParametersInfoForDpi( SPI_GETNONCLIENTMETRICS, sizeof( metrics ), &metrics, 0, dpi ) )
      return ( HFONT )GetStockObject( DEFAULT_GUI_FONT );

   if( s_menuFontHandle != NULL )
      DeleteObject( s_menuFontHandle );

   s_menuFontHandle = CreateFontIndirectW( &metrics.lfMenuFont );
   s_menuFontDpi = dpi;

   return s_menuFontHandle;
}

/* The last item of the menu gets its picture: in the extended style the item is drawn by the program, otherwise
   Windows shows the picture beside the text */
static void s_menuDecorate( X_CALL_CTX *ctx, HMENU menu, const wchar_t *text, const X_ITEM *file, int separator,
                            const char *operation )
{
   HBITMAP picture = NULL;
   X_GUI_MENUITEM *item;
   MENUITEMINFOW info;
   BITMAP size;
   const wchar_t *tab;

   if( file != NULL && !X_IS_NIL( file ) )
   {
      picture = x_guiPicture( ctx, file, operation );
      if( picture == NULL )
         return;
   }

   memset( &info, 0, sizeof( info ) );
   info.cbSize = sizeof( info );
   if( !x_guiMenuExtended )
   {
      if( picture != NULL )
      {
         info.fMask = MIIM_BITMAP;
         info.hbmpItem = picture;
         SetMenuItemInfoW( menu, ( UINT )GetMenuItemCount( menu ) - 1, TRUE, &info );
      }
      return;
   }

   item = ( X_GUI_MENUITEM * )x_xgrab( sizeof( X_GUI_MENUITEM ) );
   memset( item, 0, sizeof( *item ) );
   item->bar = !s_isPopupMenu( menu );
   item->separator = separator;
   if( text != NULL )
   {
      tab = wcschr( text, L'\t' );
      item->text = ( wchar_t * )x_xgrabArray( wcslen( text ) + 1, sizeof( wchar_t ) );
      wcscpy( item->text, text );
      if( tab != NULL )
      {
         item->text[tab - text] = L'\0';
         item->shortcut = item->text + ( tab - text ) + 1;
      }
   }

   if( picture != NULL )
   {
      GetObjectW( picture, sizeof( size ), &size );
      item->image = ImageList_Create( size.bmWidth, size.bmHeight, ILC_COLOR32 | ILC_MASK, 1, 1 );
      if( item->image != NULL )
      {
         if( x_guiHasAlpha( &size ) )
            ImageList_Add( item->image, picture, NULL );
         else
            ImageList_AddMasked( item->image, picture, x_guiCornerColor( picture ) );
      }

      DeleteObject( picture );
   }

   info.fMask = MIIM_FTYPE | MIIM_DATA;
   info.fType = MFT_OWNERDRAW | IIF( separator, MFT_SEPARATOR, 0 );
   info.dwItemData = ( ULONG_PTR )item;
   SetMenuItemInfoW( menu, ( UINT )GetMenuItemCount( menu ) - 1, TRUE, &info );
}

/* Width of the picture strip of drop-down menus */
static int s_menuStrip( UINT dpi )
{
   return x_guiScale( s_menuImageSize + 10, dpi );
}

/* WM_MEASUREITEM: the size of a menu item drawn in the extended style */
void x_guiMenuMeasure( HWND window, MEASUREITEMSTRUCT *measure )
{
   const X_GUI_MENUITEM *item = ( const X_GUI_MENUITEM * )measure->itemData;
   UINT dpi = GetDpiForWindow( window );
   HDC dc;
   HGDIOBJ old;
   SIZE text = { 0, 0 }, keys = { 0, 0 };
   int shortcut, height, image;

   if( item->separator )
   {
      measure->itemWidth = ( UINT )s_menuStrip( dpi );
      measure->itemHeight = ( UINT )x_guiScale( 5, dpi );
      return;
   }

   dc = GetDC( window );
   old = SelectObject( dc, s_menuFont( dpi ) );
   GetTextExtentPoint32W( dc, item->text, ( int )wcslen( item->text ), &text );
   if( item->shortcut != NULL )
      GetTextExtentPoint32W( dc, item->shortcut, ( int )wcslen( item->shortcut ), &keys );

   SelectObject( dc, old );
   ReleaseDC( window, dc );
   if( item->bar )
   {
      measure->itemWidth = ( UINT )( text.cx + x_guiScale( 8, dpi ) );
      measure->itemHeight = ( UINT )( text.cy + x_guiScale( 4, dpi ) );
      return;
   }

   shortcut = IIF( keys.cx > 0, x_guiScale( 32, dpi ) + keys.cx, 0 );
   height = text.cy + x_guiScale( 8, dpi );
   image = x_guiScale( s_menuImageSize + 6, dpi );
   measure->itemWidth = ( UINT )( s_menuStrip( dpi ) + x_guiScale( 8, dpi ) + text.cx + shortcut + x_guiScale( 16, dpi ) );
   measure->itemHeight = ( UINT )IIF( image > height, image, height );
}

/* A gradient from the first color to the second, across ( lVertical .F. ) or down the rectangle */
static void s_menuGradient( HDC dc, const RECT *area, COLORREF first, COLORREF second, int vertical )
{
   int size = IIF( vertical, area->bottom - area->top, area->right - area->left );
   int steps = IIF( size > 1, size - 1, 1 );
   int i;
   RECT line;

   for( i = 0; i < size; ++i )
   {
      int red = GetRValue( first ) + ( GetRValue( second ) - GetRValue( first ) ) * i / steps;
      int green = GetGValue( first ) + ( GetGValue( second ) - GetGValue( first ) ) * i / steps;
      int blue = GetBValue( first ) + ( GetBValue( second ) - GetBValue( first ) ) * i / steps;

      SetDCBrushColor( dc, RGB( red, green, blue ) );
      if( vertical )
         SetRect( &line, area->left, area->top + i, area->right, area->top + i + 1 );
      else
         SetRect( &line, area->left + i, area->top, area->left + i + 1, area->bottom );

      FillRect( dc, &line, ( HBRUSH )GetStockObject( DC_BRUSH ) );
   }
}

/* WM_DRAWITEM: draw a menu item in the extended style */
void x_guiMenuDraw( HWND window, const DRAWITEMSTRUCT *draw )
{
   const X_GUI_MENUITEM *item = ( const X_GUI_MENUITEM * )draw->itemData;
   UINT dpi = GetDpiForWindow( window );
   HDC dc = draw->hDC;
   RECT area = draw->rcItem;
   RECT part;
   int strip = s_menuStrip( dpi );
   int grayed = ( draw->itemState & ( ODS_GRAYED | ODS_DISABLED ) ) != 0;
   int selected = !item->separator && !grayed && ( draw->itemState & ( ODS_SELECTED | ODS_HOTLIGHT ) ) != 0;
   HGDIOBJ old;
   int width, height;

   SetBkMode( dc, TRANSPARENT );
   old = SelectObject( dc, s_menuFont( dpi ) );
   if( item->bar )
   {
      if( selected )
      {
         SetDCBrushColor( dc, RGB( 182, 189, 210 ) );
         FillRect( dc, &area, ( HBRUSH )GetStockObject( DC_BRUSH ) );
         SetDCBrushColor( dc, RGB( 10, 36, 106 ) );
         FrameRect( dc, &area, ( HBRUSH )GetStockObject( DC_BRUSH ) );
      }
      else
         s_menuGradient( dc, &area, RGB( 252, 251, 248 ), RGB( 229, 226, 215 ), 1 );

      SetTextColor( dc, GetSysColor( IIF( grayed, COLOR_GRAYTEXT, COLOR_MENUTEXT ) ) );
      DrawTextW( dc, item->text, -1, &area, DT_SINGLELINE | DT_CENTER | DT_VCENTER | DT_NOPREFIX );
      SelectObject( dc, old );
      return;
   }

   part = area;
   part.right = part.left + strip;
   s_menuGradient( dc, &part, RGB( 255, 255, 255 ), RGB( 215, 212, 203 ), 0 );
   part = area;
   part.left += strip;
   SetDCBrushColor( dc, RGB( 252, 252, 249 ) );
   FillRect( dc, &part, ( HBRUSH )GetStockObject( DC_BRUSH ) );
   if( item->separator )
   {
      int middle = ( area.top + area.bottom ) / 2;

      SetRect( &part, area.left + strip + x_guiScale( 8, dpi ), middle, area.right, middle + 1 );
      SetDCBrushColor( dc, RGB( 197, 194, 184 ) );
      FillRect( dc, &part, ( HBRUSH )GetStockObject( DC_BRUSH ) );
      SelectObject( dc, old );
      return;
   }

   if( selected )
   {
      part = area;
      InflateRect( &part, -x_guiScale( 1, dpi ), 0 );
      SetDCBrushColor( dc, RGB( 182, 189, 210 ) );
      FillRect( dc, &part, ( HBRUSH )GetStockObject( DC_BRUSH ) );
      SetDCBrushColor( dc, RGB( 10, 36, 106 ) );
      FrameRect( dc, &part, ( HBRUSH )GetStockObject( DC_BRUSH ) );
   }

   if( item->image != NULL && ImageList_GetIconSize( item->image, &width, &height ) )
   {
      int x = area.left + ( strip - width ) / 2;
      int y = area.top + ( area.bottom - area.top - height ) / 2;
      UINT style = ILD_TRANSPARENT | IIF( grayed, ILD_BLEND50, 0 );

      ImageList_DrawEx( item->image, 0, dc, x, y, 0, 0, CLR_NONE, CLR_NONE, style );
   }

   SetTextColor( dc, GetSysColor( IIF( grayed, COLOR_GRAYTEXT, COLOR_MENUTEXT ) ) );
   part = area;
   part.left += strip + x_guiScale( 8, dpi );
   part.right -= x_guiScale( 20, dpi );
   DrawTextW( dc, item->text, -1, &part, DT_SINGLELINE | DT_LEFT | DT_VCENTER | DT_NOPREFIX );
   if( item->shortcut != NULL )
      DrawTextW( dc, item->shortcut, -1, &part, DT_SINGLELINE | DT_RIGHT | DT_VCENTER | DT_NOPREFIX );

   SelectObject( dc, old );
}

/* Set the context menu of a window or a control: a drop-down gui_Menu, NIL removes it */
void x_guiSetContextMenu( X_CALL_CTX *ctx, size_t slot, const char *operation )
{
   const X_ITEM *value = x_paritem( ctx, 1 );
   X_ITEM *data = x_objDataSlot( ctx, slot );
   HMENU menu;

   if( data == NULL )
      return;

   if( value == NULL || X_IS_NIL( value ) )
      x_itemClear( data );
   else
   {
      menu = x_guiObjectMenu( ctx, value, operation );
      if( menu == NULL )
         return;

      if( !s_isPopupMenu( menu ) )
      {
         x_guiRaise( ctx, operation );
         return;
      }

      x_itemCopy( data, value );
   }

   if( value != NULL )
      x_itemReturn( ctx, value );
}

/* A shortcut such as "Ctrl+S" as a virtual key and modifiers, and its text for menus; 0 when it is not valid */
int x_guiParseShortcut( const char *text, size_t length, UINT *key, UINT *modifiers, char *display, size_t size )
{
   char token[16];
   char keyName[16] = "";
   size_t start = 0, i, j, n, k;
   const char *ctrl, *shift, *alt;
   int function;

   *key = 0;
   *modifiers = 0;
   for( i = 0; i <= length; ++i )
   {
      if( i < length && text[i] != '+' )
         continue;

      n = i - start;
      if( n == 0 || n >= sizeof( token ) || *key != 0 )
         return 0;

      for( j = 0; j < n; ++j )
         token[j] = ( char )toupper( ( unsigned char )text[start + j] );

      token[n] = '\0';
      start = i + 1;
      function = token[0] == 'F' && n <= 3 && isdigit( ( unsigned char )token[1] );
      function = function && ( n == 2 || isdigit( ( unsigned char )token[2] ) );
      function = function && atoi( token + 1 ) >= 1 && atoi( token + 1 ) <= 24;
      if( strcmp( token, "CTRL" ) == 0 || strcmp( token, "CONTROL" ) == 0 )
         *modifiers |= X_GUI_CTRL;
      else if( strcmp( token, "SHIFT" ) == 0 )
         *modifiers |= X_GUI_SHIFT;
      else if( strcmp( token, "ALT" ) == 0 )
         *modifiers |= X_GUI_ALT;
      else if( n == 1 && isalnum( ( unsigned char )token[0] ) )
      {
         *key = ( UINT )token[0];
         snprintf( keyName, sizeof( keyName ), "%c", token[0] );
      }
      else if( function )
      {
         *key = VK_F1 + ( UINT )atoi( token + 1 ) - 1;
         snprintf( keyName, sizeof( keyName ), "F%d", atoi( token + 1 ) );
      }
      else
      {
         for( k = 0; k < sizeof( s_keyNames ) / sizeof( s_keyNames[0] ) && *key == 0; ++k )
         {
            if( strcmp( token, s_keyNames[k].name ) == 0 )
            {
               *key = s_keyNames[k].key;
               snprintf( keyName, sizeof( keyName ), "%s", s_keyNames[k].display );
            }
         }

         if( *key == 0 )
            return 0;
      }
   }

   if( *key == 0 )
      return 0;

   ctrl = IIF( *modifiers & X_GUI_CTRL, "Ctrl+", "" );
   shift = IIF( *modifiers & X_GUI_SHIFT, "Shift+", "" );
   alt = IIF( *modifiers & X_GUI_ALT, "Alt+", "" );
   snprintf( display, size, "%s%s%s%s", ctrl, shift, alt, keyName );

   return 1;
}

/* oWindow:SetKey( cKey, [bAction] ): the key as in gui_Menu, NIL removes it; returns the previous block */
void x_guiWindowSetKey( X_CALL_CTX *ctx, HWND window, X_ITEM *blocks )
{
   X_GUI_KEYS *keys = ( X_GUI_KEYS * )GetPropW( window, x_guiKeysProperty );
   const X_ITEM *action = x_paritem( ctx, 2 );
   const int remove = action == NULL || X_IS_NIL( action );
   UINT key, modifiers;
   char display[48];
   size_t i;
   int valid;

   valid = blocks != NULL && X_ISCHAR( ctx, 1 ) && ( remove || X_IS_BLOCK( action ) );
   if( valid )
      valid = x_guiParseShortcut( x_parc( ctx, 1 ), x_parclen( ctx, 1 ), &key, &modifiers, display, sizeof( display ) );

   if( !valid )
   {
      x_guiRaise( ctx, "GUIWINDOW:SETKEY" );
      return;
   }

   if( !X_IS_ARRAY( blocks ) )
      x_arrayNew( blocks, 0 );

   if( keys == NULL )
   {
      keys = ( X_GUI_KEYS * )x_xgrab( sizeof( X_GUI_KEYS ) );
      memset( keys, 0, sizeof( *keys ) );
      SetPropW( window, x_guiKeysProperty, keys );
   }

   for( i = 0; i < keys->count && ( keys->keys[i] != key || keys->modifiers[i] != modifiers ); ++i )
      ;

   if( i < keys->count )
   {
      x_arrayGet( blocks, i + 1, ctx->result );
      if( remove )
      {
         x_arrayDel( blocks, i + 1 );
         x_arraySize( blocks, keys->count - 1 );
         memmove( keys->keys + i, keys->keys + i + 1, ( keys->count - i - 1 ) * sizeof( UINT ) );
         memmove( keys->modifiers + i, keys->modifiers + i + 1, ( keys->count - i - 1 ) * sizeof( UINT ) );
         --keys->count;
      }
      else
         x_itemCopy( x_arrayGetItemPtr( blocks, i + 1 ), action );
   }
   else if( !remove )
   {
      keys->keys = ( UINT * )x_xrealloc( keys->keys, ( keys->count + 1 ) * sizeof( UINT ) );
      keys->modifiers = ( UINT * )x_xrealloc( keys->modifiers, ( keys->count + 1 ) * sizeof( UINT ) );
      keys->keys[keys->count] = key;
      keys->modifiers[keys->count] = modifiers;
      ++keys->count;
      x_arrayAdd( blocks, action );
   }
}

/* Look for the control with the name among the children of the window */
static BOOL CALLBACK s_findControl( HWND child, LPARAM data )
{
   X_GUI_FIND *find = ( X_GUI_FIND * )data;
   X_ITEM *object = ( X_ITEM * )GetWindowLongPtrW( child, GWLP_USERDATA );
   X_ITEM name __attribute__( ( cleanup( x_itemClear ) ) ) = X_ITEM_NIL_INIT;

   if( object == NULL || ( ATOM )GetClassLongPtrW( child, GCW_ATOM ) == x_guiWindowClass )
      return TRUE;

   if( !x_objIsKindOf( object, "GUICONTROL" ) || !x_objSend( find->ctx, &name, object, "NAME", 0, NULL ) )
      return TRUE;

   if( !X_IS_STRING( &name ) || x_stricmp( x_itemGetCPtr( &name ), find->name ) != 0 )
      return TRUE;

   x_itemCopy( find->ctx->result, object );
   find->found = 1;

   return FALSE;
}

/* oWindow:<cName>: return the control with the name; 0 when there is none */
int x_guiWindowControl( X_CALL_CTX *ctx, HWND window, const char *name )
{
   X_GUI_FIND find = { ctx, name, 0 };

   if( name != NULL && *name != '\0' )
      EnumChildWindows( window, s_findControl, ( LPARAM )&find );

   return find.found;
}

/* The focus goes to the next or the previous control of the window in the order of Tab */
void x_guiWindowFocusNext( HWND window, int previous )
{
   HWND focus = GetFocus();
   HWND next;

   x_callbackEnterDll();
   next = GetNextDlgTabItem( window, IIF( focus != NULL && IsChild( window, focus ), focus, NULL ), previous );
   if( next != NULL )
      SetFocus( next );

   x_callbackLeaveDll();
   x_callbackRaisePending();
}

/* The text of a command with its shortcut after a tab, as UTF-16; NULL with an error raised */
static wchar_t *s_menuText( X_CALL_CTX *ctx, const X_ITEM *text, const X_ITEM *shortcut, UINT *key, UINT *modifiers )
{
   void *handle = NULL;
   size_t length = 0, count, extra, i;
   const char *spec;
   char display[64];
   wchar_t *wide, *result;
   int valid;

   *key = 0;
   *modifiers = 0;
   if( shortcut == NULL || X_IS_NIL( shortcut ) )
      return x_guiText( ctx, text, "GUIMENU:ADD" );

   spec = IIF( X_IS_STRING( shortcut ), x_itemGetStrUTF8( shortcut, &handle, &length ), NULL );
   valid = spec != NULL && x_guiParseShortcut( spec, length, key, modifiers, display, sizeof( display ) );
   x_strfree( handle );
   if( !valid )
   {
      x_guiRaise( ctx, "GUIMENU:ADD" );
      return NULL;
   }

   wide = x_guiText( ctx, text, "GUIMENU:ADD" );
   if( wide == NULL )
      return NULL;

   count = wcslen( wide );
   extra = strlen( display );
   result = ( wchar_t * )x_xgrabArray( count + extra + 2, sizeof( wchar_t ) );
   memcpy( result, wide, count * sizeof( wchar_t ) );
   result[count] = L'\t';
   for( i = 0; i < extra; ++i )
      result[count + 1 + i] = ( wchar_t )display[i];

   result[count + 1 + extra] = L'\0';
   x_xfree( wide );

   return result;
}

/* AppendMenuW() with "&" shown as it is, not as an access key; the error stays in GetLastError() */
BOOL x_guiMenuAppend( HMENU menu, UINT flags, UINT_PTR id, const wchar_t *text )
{
   size_t length = wcslen( text ), i, j = 0;
   wchar_t *shown = ( wchar_t * )x_xgrabArray( length * 2 + 1, sizeof( wchar_t ) );
   BOOL done;
   DWORD error;

   for( i = 0; i < length; ++i )
   {
      if( text[i] == L'&' )
         shown[j++] = L'&';
      shown[j++] = text[i];
   }
   shown[j] = L'\0';

   done = AppendMenuW( menu, flags, id, shown );
   error = GetLastError();
   x_xfree( shown );
   SetLastError( error );

   return done;
}

/* Add a command to a menu; returns its identifier, 0 with an error raised */
int x_guiMenuAdd( X_CALL_CTX *ctx, HMENU menu, const X_ITEM *text, const X_ITEM *action, const X_ITEM *shortcut,
                  const X_ITEM *image, const X_ITEM *when )
{
   const X_ITEM nil = X_ITEM_NIL_INIT;
   X_ITEM *onAction, *onWhen;
   wchar_t *wide;
   BOOL done;
   UINT key, modifiers;
   int valid;

   valid = action == NULL || X_IS_NIL( action ) || X_IS_BLOCK( action );
   valid = valid && ( when == NULL || X_IS_NIL( when ) || X_IS_BLOCK( when ) );
   if( !valid )
   {
      x_guiRaise( ctx, "GUIMENU:ADD" );
      return 0;
   }

   if( s_menuId > X_GUI_MENU_LAST )
   {
      x_guiRaise( ctx, "GUIMENU:ADD" );
      return 0;
   }

   wide = s_menuText( ctx, text, shortcut, &key, &modifiers );
   if( wide == NULL )
      return 0;

   done = x_guiMenuAppend( menu, MF_STRING, ( UINT_PTR )s_menuId, wide );
   if( !done )
   {
      x_xfree( wide );
      x_guiRaiseWin32( ctx, "AppendMenuW", GetLastError() );
      return 0;
   }

   s_menuDecorate( ctx, menu, wide, image, 0, "GUIMENU:ADD" );
   x_xfree( wide );
   if( x_guiActionCount == x_guiActionCapacity )
   {
      x_guiActionCapacity = IIF( x_guiActionCapacity == 0, ( size_t )16, x_guiActionCapacity * 2 );
      x_guiActions = ( X_GUI_ACTION * )x_xrealloc( x_guiActions, x_guiActionCapacity * sizeof( X_GUI_ACTION ) );
   }

   onAction = x_itemNew( IIF( action != NULL, action, &nil ) );
   onWhen = x_itemNew( IIF( when != NULL, when, &nil ) );
   x_guiActions[x_guiActionCount++] = ( X_GUI_ACTION ){ s_menuId, key, modifiers, onAction, onWhen };

   return s_menuId++;
}

/* Add a drop-down menu to a menu */
void x_guiMenuAddMenu( X_CALL_CTX *ctx, HMENU menu, const X_ITEM *text, HMENU submenu, const X_ITEM *image )
{
   wchar_t *wide = x_guiText( ctx, text, "GUIMENU:ADDMENU" );

   if( wide == NULL )
      return;

   if( x_guiMenuAppend( menu, MF_POPUP, ( UINT_PTR )submenu, wide ) )
      s_menuDecorate( ctx, menu, wide, image, 0, "GUIMENU:ADDMENU" );
   else
      x_guiRaiseWin32( ctx, "AppendMenuW", GetLastError() );

   x_xfree( wide );
}

/* Add a new drop-down gui_Menu to a menu and return it */
void x_guiMenuAddSubmenu( X_CALL_CTX *ctx, HMENU menu, const X_ITEM *text, const X_ITEM *image )
{
   X_CLASS_HANDLE handle = x_clsFindClass( "GUIMENU", NULL );
   X_ITEM submenu __attribute__( ( cleanup( x_itemClear ) ) ) = X_ITEM_NIL_INIT;
   X_ITEM result __attribute__( ( cleanup( x_itemClear ) ) ) = X_ITEM_NIL_INIT;
   HMENU popup;

   if( handle == 0 || !x_objCreate( ctx, &submenu, handle ) )
      return;

   if( !x_guiHandlerCall( &result, &submenu, X_GUI_EVENT_POPUP, 0, NULL ) )
      return;

   popup = x_guiObjectMenu( ctx, &submenu, "GUIMENU:ADDMENU" );
   if( popup == NULL )
      return;

   x_guiMenuAddMenu( ctx, menu, text, popup, image );
   x_itemCopy( ctx->result, &submenu );
}

/* Add a separator line to a menu */
void x_guiMenuSeparator( X_CALL_CTX *ctx, HMENU menu )
{
   if( !AppendMenuW( menu, MF_SEPARATOR, 0, NULL ) )
      x_guiRaiseWin32( ctx, "AppendMenuW", GetLastError() );
   else if( s_isPopupMenu( menu ) )
      s_menuDecorate( ctx, menu, NULL, NULL, 1, "GUIMENU:ADDSEPARATOR" );
}
