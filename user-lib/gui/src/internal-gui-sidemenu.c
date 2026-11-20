/*
 * Copyright (c) 2024-2026 xDev-Language
 *
 * Library: xDev GUI
 * File: internal-gui-sidemenu.c
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "internal-gui.h"

/* Sizes of the menu of a panel at 96 DPI: the gap above the first item, the smallest height of a row, the room
   above and under its text and picture, the margins at the left and the right, the gap between the picture and
   the text, the room of the arrow of a drop-down menu and the height of a separator */
enum
{
   X_GUI_SIDEMENU_TOP = 4,
   X_GUI_SIDEMENU_ROW = 22,
   X_GUI_SIDEMENU_PAD = 6,
   X_GUI_SIDEMENU_MARGIN = 8,
   X_GUI_SIDEMENU_GAP = 6,
   X_GUI_SIDEMENU_ARROW = 16,
   X_GUI_SIDEMENU_SEPARATOR = 9
};

/* Shades of the text color over the background in 255 parts: the row under the mouse, the row of the open
   drop-down menu, a separator, a shortcut and a grey item */
enum
{
   X_GUI_SIDEMENU_HOT = 24,
   X_GUI_SIDEMENU_OPEN = 40,
   X_GUI_SIDEMENU_LINE = 48,
   X_GUI_SIDEMENU_KEYS = 160,
   X_GUI_SIDEMENU_GREY = 128
};

/* The menu shown in a panel: the item under the mouse and the item with its drop-down menu open, -1 for none;
   tracking is set while the panel waits for the mouse to leave it */
typedef struct
{
   HMENU menu;
   int hot;
   int open;
   int tracking;
} X_GUI_SIDEMENU;

/* An item of the menu read for drawing: its text and the shortcut after it, its picture; buffer and made are
   the text and the picture made for it, freed after drawing */
typedef struct
{
   const wchar_t *text;
   const wchar_t *shortcut;
   wchar_t *buffer;
   HIMAGELIST image;
   HIMAGELIST made;
   HMENU submenu;
   UINT id;
   int separator;
   int grayed;
} X_GUI_SIDEITEM;

/* ---------------------------------------------------------------------------------------------
   Items
   --------------------------------------------------------------------------------------------- */

/* The picture of an item in the simple style as an image list, the top left color transparent without alpha */
static HIMAGELIST s_sidePicture( HBITMAP picture )
{
   HIMAGELIST image;
   BITMAP size;

   if( picture == NULL || GetObjectW( picture, sizeof( size ), &size ) == 0 )
      return NULL;

   image = ImageList_Create( size.bmWidth, size.bmHeight, ILC_COLOR32 | ILC_MASK, 1, 1 );
   if( image == NULL )
      return NULL;

   if( x_guiHasAlpha( &size ) )
      ImageList_Add( image, picture, NULL );
   else
      ImageList_AddMasked( image, picture, x_guiCornerColor( picture ) );

   return image;
}

/* Read an item of the menu drawn in the extended style or in the simple one */
static void s_sideItemRead( HMENU menu, int index, X_GUI_SIDEITEM *item )
{
   const X_GUI_MENUITEM *drawn;
   MENUITEMINFOW info;
   wchar_t *tab;
   int length;

   memset( item, 0, sizeof( *item ) );
   memset( &info, 0, sizeof( info ) );
   info.cbSize = sizeof( info );
   info.fMask = MIIM_FTYPE | MIIM_STATE | MIIM_ID | MIIM_SUBMENU | MIIM_DATA | MIIM_BITMAP;
   GetMenuItemInfoW( menu, ( UINT )index, TRUE, &info );
   item->submenu = info.hSubMenu;
   item->id = info.wID;
   item->separator = ( info.fType & MFT_SEPARATOR ) != 0;
   item->grayed = ( info.fState & MFS_DISABLED ) != 0;
   drawn = IIF( info.fType & MFT_OWNERDRAW, ( const X_GUI_MENUITEM * )info.dwItemData, NULL );
   if( drawn != NULL )
   {
      item->text = drawn->text;
      item->shortcut = drawn->shortcut;
      item->image = drawn->image;
      return;
   }

   if( item->separator )
      return;

   length = GetMenuStringW( menu, ( UINT )index, NULL, 0, MF_BYPOSITION );
   item->buffer = ( wchar_t * )x_xgrabArray( ( size_t )length + 1, sizeof( wchar_t ) );
   GetMenuStringW( menu, ( UINT )index, item->buffer, length + 1, MF_BYPOSITION );
   tab = wcschr( item->buffer, L'\t' );
   if( tab != NULL )
   {
      *tab = L'\0';
      item->shortcut = tab + 1;
   }

   item->text = item->buffer;
   item->made = s_sidePicture( info.hbmpItem );
   item->image = item->made;
}

/* Free what s_sideItemRead() made for an item */
static void s_sideItemFree( X_GUI_SIDEITEM *item )
{
   if( item->buffer != NULL )
      x_xfree( item->buffer );

   if( item->made != NULL )
      ImageList_Destroy( item->made );
}

/* The height of an item: a separator is thin, a row fits its text and its picture */
static int s_sideHeight( const X_GUI_SIDEITEM *item, int text, UINT dpi )
{
   int least = x_guiScale( X_GUI_SIDEMENU_ROW, dpi );
   int width = 0, height = 0, row;

   if( item->separator )
      return x_guiScale( X_GUI_SIDEMENU_SEPARATOR, dpi );

   if( item->image != NULL )
      ImageList_GetIconSize( item->image, &width, &height );

   row = IIF( text > height, text, height ) + x_guiScale( X_GUI_SIDEMENU_PAD, dpi );

   return IIF( row > least, row, least );
}

/* The width of the widest picture of the menu, 0 without pictures */
static int s_sidePictures( HMENU menu )
{
   int count = GetMenuItemCount( menu );
   int widest = 0, width, height, i;
   X_GUI_SIDEITEM item;

   for( i = 0; i < count; ++i )
   {
      s_sideItemRead( menu, i, &item );
      width = 0;
      if( item.image != NULL )
         ImageList_GetIconSize( item.image, &width, &height );

      widest = IIF( width > widest, width, widest );
      s_sideItemFree( &item );
   }

   return widest;
}

/* The height of the text in the font of the sidebar */
static int s_sideTextHeight( HWND panel, UINT dpi )
{
   HDC dc = GetDC( panel );
   HGDIOBJ old = SelectObject( dc, x_guiFont( dpi ) );
   TEXTMETRICW metrics;

   GetTextMetricsW( dc, &metrics );
   SelectObject( dc, old );
   ReleaseDC( panel, dc );

   return metrics.tmHeight;
}

/* The item at the point y of the panel and its row, -1 for none */
static int s_sideHit( HWND panel, const X_GUI_SIDEMENU *side, int y, RECT *row )
{
   UINT dpi = GetDpiForWindow( panel );
   int count = GetMenuItemCount( side->menu );
   int text = s_sideTextHeight( panel, dpi );
   int top = x_guiScale( X_GUI_SIDEMENU_TOP, dpi );
   int height, i;
   X_GUI_SIDEITEM item;
   RECT client;

   GetClientRect( panel, &client );
   for( i = 0; i < count; ++i )
   {
      s_sideItemRead( side->menu, i, &item );
      height = s_sideHeight( &item, text, dpi );
      s_sideItemFree( &item );
      if( y >= top && y < top + height )
      {
         *row = ( RECT ){ 0, top, client.right, top + height };
         return i;
      }

      top += height;
   }

   return -1;
}

/* ---------------------------------------------------------------------------------------------
   Drawing
   --------------------------------------------------------------------------------------------- */

/* Draw one row: the field under the mouse, the picture, the text, the shortcut and the arrow of a drop-down menu */
static void s_sideDrawRow( HDC dc, const X_GUI_SIDEITEM *item, const RECT *row, int left, int pictures, int shade,
                           UINT dpi )
{
   HBRUSH brush = ( HBRUSH )GetStockObject( DC_BRUSH );
   int margin = x_guiScale( X_GUI_SIDEMENU_MARGIN, dpi );
   int width = 0, height = 0;
   COLORREF back = GetBkColor( dc );
   COLORREF fore = GetTextColor( dc );
   RECT part;

   if( shade > 0 )
   {
      SetDCBrushColor( dc, x_guiMix( back, fore, shade ) );
      FillRect( dc, row, brush );
   }

   if( item->image != NULL && ImageList_GetIconSize( item->image, &width, &height ) )
   {
      int x = row->left + margin + ( pictures - width ) / 2;
      int y = row->top + ( row->bottom - row->top - height ) / 2;
      UINT style = ILD_TRANSPARENT | IIF( item->grayed, ILD_BLEND50, 0 );

      ImageList_DrawEx( item->image, 0, dc, x, y, 0, 0, CLR_NONE, CLR_NONE, style );
   }

   part = *row;
   part.left = left;
   part.right -= margin + x_guiScale( X_GUI_SIDEMENU_ARROW, dpi );
   SetTextColor( dc, IIF( item->grayed, x_guiMix( back, fore, X_GUI_SIDEMENU_GREY ), fore ) );
   if( item->text != NULL )
      DrawTextW( dc, item->text, -1, &part, DT_SINGLELINE | DT_LEFT | DT_VCENTER | DT_HIDEPREFIX | DT_END_ELLIPSIS );

   SetTextColor( dc, x_guiMix( back, fore, X_GUI_SIDEMENU_KEYS ) );
   if( item->shortcut != NULL )
      DrawTextW( dc, item->shortcut, -1, &part, DT_SINGLELINE | DT_RIGHT | DT_VCENTER | DT_NOPREFIX );

   part.left = part.right;
   part.right = row->right - margin;
   if( item->submenu != NULL )
      DrawTextW( dc, L"\x203A", -1, &part, DT_SINGLELINE | DT_RIGHT | DT_VCENTER | DT_NOPREFIX );

   SetTextColor( dc, fore );
}

/* The panel paints the items of its menu one under another on the colors of the sidebar */
static void s_sidePaint( HWND panel, const X_GUI_SIDEMENU *side )
{
   PAINTSTRUCT state;
   HDC dc = BeginPaint( panel, &state );
   UINT dpi = GetDpiForWindow( panel );
   HGDIOBJ old = SelectObject( dc, x_guiFont( dpi ) );
   HBRUSH brush = ( HBRUSH )GetStockObject( DC_BRUSH );
   int count = GetMenuItemCount( side->menu );
   int margin = x_guiScale( X_GUI_SIDEMENU_MARGIN, dpi );
   int pictures = s_sidePictures( side->menu );
   int top = x_guiScale( X_GUI_SIDEMENU_TOP, dpi );
   int left, height, shade, middle, i;
   COLORREF back, fore;
   TEXTMETRICW metrics;
   X_GUI_SIDEITEM item;
   RECT client, row, line;

   x_guiSidebarColors( GetParent( panel ), &back, &fore );
   GetClientRect( panel, &client );
   SetDCBrushColor( dc, back );
   FillRect( dc, &client, brush );
   SetBkMode( dc, TRANSPARENT );
   SetBkColor( dc, back );
   SetTextColor( dc, fore );
   GetTextMetricsW( dc, &metrics );
   left = margin + IIF( pictures > 0, pictures + x_guiScale( X_GUI_SIDEMENU_GAP, dpi ), 0 );
   for( i = 0; i < count; ++i )
   {
      s_sideItemRead( side->menu, i, &item );
      height = s_sideHeight( &item, metrics.tmHeight, dpi );
      row = ( RECT ){ 0, top, client.right, top + height };
      if( item.separator )
      {
         middle = ( row.top + row.bottom ) / 2;
         line = ( RECT ){ margin, middle, client.right - margin, middle + 1 };
         SetDCBrushColor( dc, x_guiMix( back, fore, X_GUI_SIDEMENU_LINE ) );
         FillRect( dc, &line, brush );
      }
      else
      {
         shade = IIF( i == side->open, X_GUI_SIDEMENU_OPEN, 0 );
         shade = IIF( shade == 0 && i == side->hot && !item.grayed, X_GUI_SIDEMENU_HOT, shade );
         s_sideDrawRow( dc, &item, &row, left, pictures, shade, dpi );
      }

      s_sideItemFree( &item );
      top += height;
   }

   SelectObject( dc, old );
   EndPaint( panel, &state );
}

/* ---------------------------------------------------------------------------------------------
   Mouse
   --------------------------------------------------------------------------------------------- */

/* The mouse comes into the panel: the commands with bWhen become active or grey, as before a menu opens */
static void s_sideEnter( HWND panel, X_GUI_SIDEMENU *side )
{
   HWND root = GetAncestor( panel, GA_ROOT );
   X_ITEM *object = ( X_ITEM * )GetWindowLongPtrW( root, GWLP_USERDATA );
   X_ITEM result __attribute__( ( cleanup( x_itemClear ) ) ) = X_ITEM_NIL_INIT;
   int64_t values[1] = { ( int64_t )( intptr_t )side->menu };
   TRACKMOUSEEVENT track = { sizeof( track ), TME_LEAVE, panel, 0 };

   TrackMouseEvent( &track );
   side->tracking = 1;
   if( object != NULL )
      x_guiNotify( object, "__GUIMENUWHEN", &result, 1, values );

   InvalidateRect( panel, NULL, FALSE );
}

/* The item under the mouse gets its field */
static void s_sideMove( HWND panel, X_GUI_SIDEMENU *side, int y )
{
   RECT row;
   int hot;

   if( !side->tracking )
      s_sideEnter( panel, side );

   hot = s_sideHit( panel, side, y, &row );
   if( hot == side->hot )
      return;

   side->hot = hot;
   InvalidateRect( panel, NULL, FALSE );
}

/* A click on a command sends it to the window as a command of the menu bar; a click on a drop-down menu opens
   it at the right of its row, its commands go to the window too */
static void s_sideClick( HWND panel, X_GUI_SIDEMENU *side, int y )
{
   HWND root = GetAncestor( panel, GA_ROOT );
   X_GUI_SIDEITEM item;
   POINT corner;
   RECT row;
   int index = s_sideHit( panel, side, y, &row );

   if( index < 0 )
      return;

   s_sideItemRead( side->menu, index, &item );
   s_sideItemFree( &item );
   if( item.separator || item.grayed )
      return;

   if( item.submenu == NULL )
   {
      PostMessageW( root, WM_COMMAND, MAKEWPARAM( item.id, 0 ), 0 );
      return;
   }

   side->open = index;
   InvalidateRect( panel, NULL, FALSE );
   UpdateWindow( panel );
   corner = ( POINT ){ row.right, row.top };
   ClientToScreen( panel, &corner );
   TrackPopupMenuEx( item.submenu, TPM_LEFTALIGN | TPM_TOPALIGN | TPM_RIGHTBUTTON, corner.x, corner.y, root, NULL );
   side->open = -1;
   InvalidateRect( panel, NULL, FALSE );
}

/* ---------------------------------------------------------------------------------------------
   Menus in a panel of a sidebar
   --------------------------------------------------------------------------------------------- */

/* The panel paints its menu, follows the mouse over it and runs the clicks on its items */
static LRESULT CALLBACK s_sideMenuProc( HWND panel, UINT message, WPARAM wParam, LPARAM lParam, UINT_PTR id,
                                        DWORD_PTR data )
{
   X_GUI_SIDEMENU *side = ( X_GUI_SIDEMENU * )data;

   switch( message )
   {
   case WM_ERASEBKGND:
      return 1;

   case WM_PAINT:
      s_sidePaint( panel, side );
      return 0;

   case WM_SIZE:
      InvalidateRect( panel, NULL, FALSE );
      break;

   case WM_MOUSEMOVE:
      s_sideMove( panel, side, ( short )HIWORD( lParam ) );
      return 0;

   case WM_MOUSELEAVE:
      side->tracking = 0;
      side->hot = -1;
      InvalidateRect( panel, NULL, FALSE );
      return 0;

   case WM_LBUTTONUP:
      s_sideClick( panel, side, ( short )HIWORD( lParam ) );
      return 0;

   case WM_NCDESTROY:
      RemoveWindowSubclass( panel, s_sideMenuProc, id );
      x_xfree( side );
      break;
   }

   return DefSubclassProc( panel, message, wParam, lParam );
}

/* The panel shows the items of the menu one under another */
void x_guiSideMenu( HWND panel, HMENU menu )
{
   X_GUI_SIDEMENU *side = ( X_GUI_SIDEMENU * )x_xgrab( sizeof( X_GUI_SIDEMENU ) );

   *side = ( X_GUI_SIDEMENU ){ menu, -1, -1, 0 };
   SetWindowSubclass( panel, s_sideMenuProc, 1, ( DWORD_PTR )side );
   InvalidateRect( panel, NULL, TRUE );
}
