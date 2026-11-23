/*
 * Copyright (c) 2024-2026 xDev-Language
 *
 * Library...: xDev GUI
 * File......: internal-gui-listview.c
 *
 */

#include <string.h>
#include "internal-gui.h"

/* ---------------------------------------------------------------------------------------------
   List views
   --------------------------------------------------------------------------------------------- */

/* The number of columns of a list view */
int x_guiListViewColumnCount( HWND window )
{
   HWND header = ( HWND )SendMessageW( window, LVM_GETHEADER, 0, 0 );

   return IIF( header != NULL, ( int )SendMessageW( header, HDM_GETITEMCOUNT, 0, 0 ), 0 );
}

/* The number of rows of a list view */
int x_guiListViewCount( HWND window )
{
   return ( int )SendMessageW( window, LVM_GETITEMCOUNT, 0, 0 );
}

/* Add a column; returns its number, 0 with an error raised */
int x_guiListViewAddColumn( X_CALL_CTX *ctx, HWND window, const X_ITEM *title, int width, int align )
{
   static const int formats[] = { LVCFMT_LEFT, LVCFMT_RIGHT, LVCFMT_CENTER };
   LVCOLUMNW column;
   wchar_t *text;
   LRESULT index;

   if( width <= 0 || align < 0 || align > 2 )
   {
      x_guiRaise( ctx, "GUILISTVIEW:ADDCOLUMN" );
      return 0;
   }

   text = x_guiText( ctx, title, "GUILISTVIEW:ADDCOLUMN" );
   if( text == NULL )
      return 0;

   memset( &column, 0, sizeof( column ) );
   column.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_FMT;
   column.fmt = formats[align];
   column.cx = x_guiScale( width, GetDpiForWindow( window ) );
   column.pszText = text;
   index = x_guiSend( window, LVM_INSERTCOLUMNW, ( WPARAM )x_guiListViewColumnCount( window ), ( LPARAM )&column );
   x_xfree( text );
   if( index < 0 )
   {
      x_guiRaiseWin32( ctx, "LVM_INSERTCOLUMNW", ERROR_NOT_ENOUGH_MEMORY );
      return 0;
   }

   return ( int )index + 1;
}

/* Set the text of a cell, row and column counted from 0 */
static int s_listViewSetText( X_CALL_CTX *ctx, HWND window, int row, int column, const X_ITEM *value,
                              const char *operation )
{
   wchar_t *text = x_guiText( ctx, value, operation );
   LVITEMW item;
   LRESULT done;

   if( text == NULL )
      return 0;

   memset( &item, 0, sizeof( item ) );
   item.iSubItem = column;
   item.pszText = text;
   done = x_guiSend( window, LVM_SETITEMTEXTW, ( WPARAM )row, ( LPARAM )&item );
   x_xfree( text );

   return done != 0;
}

/* Append a row with the texts of its first columns; returns its number, 0 with an error raised */
int x_guiListViewAddRow( X_CALL_CTX *ctx, HWND window, const X_ITEM *values )
{
   const int columns = x_guiListViewColumnCount( window );
   int64_t length = 0, i;
   wchar_t *text;
   LVITEMW item;
   LRESULT index;

   if( values != NULL && X_IS_ARRAY( values ) )
      length = x_arrayLen( values );

   for( i = 1; i <= length; ++i )
   {
      if( !X_IS_STRING( x_arrayGetItemPtr( values, ( size_t )i ) ) )
         length = 0;
   }

   if( length == 0 || length > columns )
   {
      x_guiRaise( ctx, "GUILISTVIEW:ADDROW" );
      return 0;
   }

   text = x_guiText( ctx, x_arrayGetItemPtr( values, 1 ), "GUILISTVIEW:ADDROW" );
   if( text == NULL )
      return 0;

   memset( &item, 0, sizeof( item ) );
   item.mask = LVIF_TEXT;
   item.iItem = x_guiListViewCount( window );
   item.pszText = text;
   index = x_guiSend( window, LVM_INSERTITEMW, 0, ( LPARAM )&item );
   x_xfree( text );
   if( index < 0 )
   {
      x_guiRaiseWin32( ctx, "LVM_INSERTITEMW", ERROR_NOT_ENOUGH_MEMORY );
      return 0;
   }

   for( i = 2; i <= length; ++i )
   {
      const X_ITEM *value = x_arrayGetItemPtr( values, ( size_t )i );

      if( !s_listViewSetText( ctx, window, ( int )index, ( int )i - 1, value, "GUILISTVIEW:ADDROW" ) )
         return 0;
   }

   return ( int )index + 1;
}

/* Whether a cell, counted from 1, exists; an error is raised when it does not */
static int s_listViewCellValid( X_CALL_CTX *ctx, HWND window, int row, int column, const char *operation )
{
   if( row < 1 || row > x_guiListViewCount( window ) || column < 1 || column > x_guiListViewColumnCount( window ) )
   {
      x_guiRaise( ctx, operation );
      return 0;
   }

   return 1;
}

/* Return the text of a cell */
void x_guiListViewCell( X_CALL_CTX *ctx, HWND window, int row, int column )
{
   size_t capacity = 256, size;
   wchar_t *buffer;
   LVITEMW item;
   LRESULT length;
   char *text;

   if( !s_listViewCellValid( ctx, window, row, column, "GUILISTVIEW:CELL" ) )
      return;

   for( ;; )
   {
      buffer = ( wchar_t * )x_xgrabArray( capacity, sizeof( wchar_t ) );
      memset( &item, 0, sizeof( item ) );
      item.iSubItem = column - 1;
      item.pszText = buffer;
      item.cchTextMax = ( int )capacity;
      length = SendMessageW( window, LVM_GETITEMTEXTW, ( WPARAM )( row - 1 ), ( LPARAM )&item );
      if( ( size_t )length < capacity - 1 )
         break;

      x_xfree( buffer );
      capacity *= 2;
   }

   text = x_utf16ToUtf8Alloc( ( const uint16_t * )buffer, ( size_t )length, &size );
   x_xfree( buffer );
   x_retclen_buffer( ctx, text, size );
}

/* Set the text of a cell */
void x_guiListViewSetCell( X_CALL_CTX *ctx, HWND window, int row, int column, const X_ITEM *text )
{
   if( s_listViewCellValid( ctx, window, row, column, "GUILISTVIEW:SETCELL" ) )
      s_listViewSetText( ctx, window, row - 1, column - 1, text, "GUILISTVIEW:SETCELL" );
}

/* Delete a row */
void x_guiListViewDelete( X_CALL_CTX *ctx, HWND window, int row )
{
   if( row < 1 || row > x_guiListViewCount( window ) )
      x_guiRaise( ctx, "GUILISTVIEW:DELETE" );
   else
      x_guiSend( window, LVM_DELETEITEM, ( WPARAM )( row - 1 ), 0 );
}

/* Delete all rows */
void x_guiListViewClear( HWND window )
{
   x_guiSend( window, LVM_DELETEALLITEMS, 0, 0 );
}

/* The selected row, 0 for none */
int x_guiListViewSelected( HWND window )
{
   return ( int )SendMessageW( window, LVM_GETNEXTITEM, ( WPARAM )-1, LVNI_SELECTED ) + 1;
}

/* Select a row, 0 for none; OnSelect does not run */
void x_guiListViewSetSelected( X_CALL_CTX *ctx, HWND window, int row )
{
   LVITEMW item;

   if( row < 0 || row > x_guiListViewCount( window ) )
   {
      x_guiRaise( ctx, "GUILISTVIEW:SELECTED" );
      return;
   }

   memset( &item, 0, sizeof( item ) );
   item.stateMask = LVIS_SELECTED | LVIS_FOCUSED;
   x_guiQuiet = 1;
   x_guiSend( window, LVM_SETITEMSTATE, ( WPARAM )-1, ( LPARAM )&item );
   if( row > 0 )
   {
      item.state = LVIS_SELECTED | LVIS_FOCUSED;
      x_guiSend( window, LVM_SETITEMSTATE, ( WPARAM )( row - 1 ), ( LPARAM )&item );
      x_guiSend( window, LVM_ENSUREVISIBLE, ( WPARAM )( row - 1 ), FALSE );
   }

   x_guiQuiet = 0;
}
