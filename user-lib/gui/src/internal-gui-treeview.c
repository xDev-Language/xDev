/*
 * Copyright (c) 2024-2026 xDev-Language
 *
 * Library...: xDev GUI
 * File......: internal-gui-treeview.c
 *
 */

#include <string.h>
#include "internal-gui.h"

/* ---------------------------------------------------------------------------------------------
   Tree views
   --------------------------------------------------------------------------------------------- */

/* The tree item of a value; an error is raised for another value */
static HTREEITEM s_treeItem( X_CALL_CTX *ctx, const X_ITEM *item, const char *operation )
{
   HTREEITEM handle = IIF( item != NULL && X_IS_POINTER( item ), ( HTREEITEM )x_itemGetPtr( item ), NULL );

   if( handle == NULL )
      x_guiRaise( ctx, operation );

   return handle;
}

/* Add an item under the parent item or at the root; returns the item */
void x_guiTreeAdd( X_CALL_CTX *ctx, HWND window, const X_ITEM *text, const X_ITEM *parent )
{
   HTREEITEM parentItem = TVI_ROOT;
   HTREEITEM added;
   TVINSERTSTRUCTW insert;
   wchar_t *wide;

   if( parent != NULL && !X_IS_NIL( parent ) )
   {
      parentItem = s_treeItem( ctx, parent, "GUITREEVIEW:ADD" );
      if( parentItem == NULL )
         return;
   }

   wide = x_guiText( ctx, text, "GUITREEVIEW:ADD" );
   if( wide == NULL )
      return;

   memset( &insert, 0, sizeof( insert ) );
   insert.hParent = parentItem;
   insert.hInsertAfter = TVI_LAST;
   insert.item.mask = TVIF_TEXT;
   insert.item.pszText = wide;
   added = ( HTREEITEM )x_guiSend( window, TVM_INSERTITEMW, 0, ( LPARAM )&insert );
   x_xfree( wide );
   if( added == NULL )
      x_guiRaiseWin32( ctx, "TVM_INSERTITEMW", ERROR_NOT_ENOUGH_MEMORY );
   else
      x_retptr( ctx, added );
}

/* Return the text of an item */
void x_guiTreeText( X_CALL_CTX *ctx, HWND window, const X_ITEM *item )
{
   enum { TEXT_CAPACITY = 1024 };
   HTREEITEM handle = s_treeItem( ctx, item, "GUITREEVIEW:ITEMTEXT" );
   wchar_t buffer[TEXT_CAPACITY];
   TVITEMW data;
   size_t size;
   char *text;

   if( handle == NULL )
      return;

   memset( &data, 0, sizeof( data ) );
   data.mask = TVIF_TEXT | TVIF_HANDLE;
   data.hItem = handle;
   data.pszText = buffer;
   data.cchTextMax = TEXT_CAPACITY;
   if( !SendMessageW( window, TVM_GETITEMW, 0, ( LPARAM )&data ) )
   {
      x_guiRaise( ctx, "GUITREEVIEW:ITEMTEXT" );
      return;
   }

   text = x_utf16ToUtf8Alloc( ( const uint16_t * )buffer, wcslen( buffer ), &size );
   x_retclen_buffer( ctx, text, size );
}

/* Set the text of an item */
void x_guiTreeSetText( X_CALL_CTX *ctx, HWND window, const X_ITEM *item, const X_ITEM *text )
{
   HTREEITEM handle = s_treeItem( ctx, item, "GUITREEVIEW:SETITEMTEXT" );
   wchar_t *wide;
   TVITEMW data;
   LRESULT done;

   if( handle == NULL )
      return;

   wide = x_guiText( ctx, text, "GUITREEVIEW:SETITEMTEXT" );
   if( wide == NULL )
      return;

   memset( &data, 0, sizeof( data ) );
   data.mask = TVIF_TEXT | TVIF_HANDLE;
   data.hItem = handle;
   data.pszText = wide;
   done = x_guiSend( window, TVM_SETITEMW, 0, ( LPARAM )&data );
   x_xfree( wide );
   if( !done )
      x_guiRaise( ctx, "GUITREEVIEW:SETITEMTEXT" );
}

/* Expand or collapse an item */
void x_guiTreeExpand( X_CALL_CTX *ctx, HWND window, const X_ITEM *item, int expand )
{
   HTREEITEM handle = s_treeItem( ctx, item, IIF( expand, "GUITREEVIEW:EXPAND", "GUITREEVIEW:COLLAPSE" ) );

   if( handle != NULL )
      x_guiSend( window, TVM_EXPAND, IIF( expand, TVE_EXPAND, TVE_COLLAPSE ), ( LPARAM )handle );
}

/* Return the selected item, NIL for none */
void x_guiTreeSelected( X_CALL_CTX *ctx, HWND window )
{
   HTREEITEM handle = ( HTREEITEM )SendMessageW( window, TVM_GETNEXTITEM, TVGN_CARET, 0 );

   if( handle != NULL )
      x_retptr( ctx, handle );
}

/* Select an item, NIL for none */
void x_guiTreeSelect( X_CALL_CTX *ctx, HWND window, const X_ITEM *item )
{
   HTREEITEM handle = NULL;

   if( item != NULL && !X_IS_NIL( item ) )
   {
      handle = s_treeItem( ctx, item, "GUITREEVIEW:SELECTED" );
      if( handle == NULL )
         return;
   }

   if( !x_guiSend( window, TVM_SELECTITEM, TVGN_CARET, ( LPARAM )handle ) && handle != NULL )
      x_guiRaise( ctx, "GUITREEVIEW:SELECTED" );
}

/* Delete all items */
void x_guiTreeClear( HWND window )
{
   x_guiSend( window, TVM_DELETEITEM, 0, ( LPARAM )TVI_ROOT );
}

/* The number of items */
int x_guiTreeCount( HWND window )
{
   return ( int )SendMessageW( window, TVM_GETCOUNT, 0, 0 );
}
