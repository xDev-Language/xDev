/*
 * Copyright (c) 2024-2026 xDev-Language
 *
 * Library...: xDev GUI
 * File......: internal-gui-list.c
 *
 */

#include "internal-gui.h"

/* The messages of a list box or of a combo box for the same operations */
typedef struct
{
   UINT add, reset, count, getSelection, setSelection, textLength, text;
} X_GUI_LIST_MESSAGES;

static const X_GUI_LIST_MESSAGES s_listBoxMessages = { LB_ADDSTRING, LB_RESETCONTENT, LB_GETCOUNT, LB_GETCURSEL,
                                                        LB_SETCURSEL, LB_GETTEXTLEN, LB_GETTEXT };
static const X_GUI_LIST_MESSAGES s_comboBoxMessages = { CB_ADDSTRING, CB_RESETCONTENT, CB_GETCOUNT, CB_GETCURSEL,
                                                        CB_SETCURSEL, CB_GETLBTEXTLEN, CB_GETLBTEXT };

/* ---------------------------------------------------------------------------------------------
   Lists
   --------------------------------------------------------------------------------------------- */

/* The messages of a combo box or of a list box */
static const X_GUI_LIST_MESSAGES *s_listMessages( HWND window )
{
   return IIF( x_guiHasClass( window, L"ComboBox" ), &s_comboBoxMessages, &s_listBoxMessages );
}

/* Add an item to a list; returns its number, 0 with an error raised */
int x_guiListAdd( X_CALL_CTX *ctx, HWND window, const X_ITEM *text )
{
   wchar_t *wide = x_guiText( ctx, text, "GUILISTBOX:ADD" );
   LRESULT index;

   if( wide == NULL )
      return 0;

   index = x_guiSend( window, s_listMessages( window )->add, 0, ( LPARAM )wide );
   x_xfree( wide );
   if( index < 0 )
   {
      x_guiRaiseWin32( ctx, "GUILISTBOX:ADD", ERROR_NOT_ENOUGH_MEMORY );
      return 0;
   }

   return ( int )index + 1;
}

/* Remove all items of a list */
void x_guiListClear( HWND window )
{
   x_guiSend( window, s_listMessages( window )->reset, 0, 0 );
}

/* The number of items of a list */
int x_guiListCount( HWND window )
{
   return ( int )SendMessageW( window, s_listMessages( window )->count, 0, 0 );
}

/* The selected item of a list, 0 for none */
int x_guiListSelected( HWND window )
{
   return ( int )SendMessageW( window, s_listMessages( window )->getSelection, 0, 0 ) + 1;
}

/* Select an item of a list, 0 for none */
void x_guiListSetSelected( X_CALL_CTX *ctx, HWND window, int index )
{
   if( index < 0 || index > x_guiListCount( window ) )
      x_guiRaise( ctx, "GUILISTBOX:SELECTED" );
   else
      x_guiSend( window, s_listMessages( window )->setSelection, ( WPARAM )( index - 1 ), 0 );
}

/* Return the text of an item of a list */
void x_guiListItem( X_CALL_CTX *ctx, HWND window, int index )
{
   const X_GUI_LIST_MESSAGES *messages = s_listMessages( window );
   LRESULT length;
   wchar_t *wide;
   size_t size;
   char *text;

   if( index < 1 || index > x_guiListCount( window ) )
   {
      x_guiRaise( ctx, "GUILISTBOX:ITEM" );
      return;
   }

   length = SendMessageW( window, messages->textLength, ( WPARAM )( index - 1 ), 0 );
   wide = ( wchar_t * )x_xgrabArray( ( size_t )IIF( length < 0, 0, length ) + 1, sizeof( wchar_t ) );
   length = SendMessageW( window, messages->text, ( WPARAM )( index - 1 ), ( LPARAM )wide );
   text = x_utf16ToUtf8Alloc( ( const uint16_t * )wide, ( size_t )IIF( length < 0, 0, length ), &size );
   x_xfree( wide );
   x_retclen_buffer( ctx, text, size );
}
