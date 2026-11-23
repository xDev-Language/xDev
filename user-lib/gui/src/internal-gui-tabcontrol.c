/*
 * Copyright (c) 2024-2026 xDev-Language
 *
 * Library...: xDev GUI
 * File......: internal-gui-tabcontrol.c
 *
 */

#include <string.h>
#include "internal-gui.h"

/* ---------------------------------------------------------------------------------------------
   Tab controls
   --------------------------------------------------------------------------------------------- */

/* Add a page to a tab control and create its window object */
void x_guiTabAddPage( X_CALL_CTX *ctx, HWND tab, const X_ITEM *title, X_ITEM *page )
{
   const int count = ( int )SendMessageW( tab, TCM_GETITEMCOUNT, 0, 0 );
   wchar_t *text = x_guiText( ctx, title, "GUITABCONTROL:ADDPAGE" );
   TCITEMW item;
   LRESULT index;

   if( text == NULL )
      return;

   memset( &item, 0, sizeof( item ) );
   item.mask = TCIF_TEXT;
   item.pszText = text;
   index = x_guiSend( tab, TCM_INSERTITEMW, ( WPARAM )count, ( LPARAM )&item );
   x_xfree( text );
   if( index < 0 )
   {
      x_guiRaiseWin32( ctx, "TCM_INSERTITEMW", ERROR_NOT_ENOUGH_MEMORY );
      return;
   }

   x_guiWindowSend( ctx, page, ctx->self, title, 10, 10, X_GUI_WINDOW_PAGE, count + 1 );
   x_guiTabRefresh( tab );
}

/* Place the pages again and show the selected one */
void x_guiTabRefresh( HWND tab )
{
   x_callbackEnterDll();
   x_guiTabLayout( tab, 1 );
   x_callbackLeaveDll();
   x_callbackRaisePending();
}

/* Return the object of the page nIndex */
void x_guiTabPage( X_CALL_CTX *ctx, HWND tab, int index )
{
   X_GUI_TABS tabs;
   const X_ITEM *object;

   memset( &tabs, 0, sizeof( tabs ) );
   tabs.tab = tab;
   tabs.selected = index;
   tabs.find = 1;
   EnumChildWindows( tab, x_guiTabPageEnum, ( LPARAM )&tabs );
   object = IIF( tabs.found != NULL, ( const X_ITEM * )GetWindowLongPtrW( tabs.found, GWLP_USERDATA ), NULL );
   if( object == NULL )
      x_guiRaise( ctx, "GUITABCONTROL:PAGE" );
   else
      x_itemReturn( ctx, object );
}

/* The number of pages */
int x_guiTabCount( HWND tab )
{
   return ( int )SendMessageW( tab, TCM_GETITEMCOUNT, 0, 0 );
}

/* The selected page, from 1 */
int x_guiTabSelected( HWND tab )
{
   return ( int )SendMessageW( tab, TCM_GETCURSEL, 0, 0 ) + 1;
}

/* Select a page */
void x_guiTabSelect( X_CALL_CTX *ctx, HWND tab, int index )
{
   if( index < 1 || index > x_guiTabCount( tab ) )
   {
      x_guiRaise( ctx, "GUITABCONTROL:SELECTED" );
      return;
   }

   x_guiSend( tab, TCM_SETCURSEL, ( WPARAM )( index - 1 ), 0 );
   x_guiTabRefresh( tab );
}
