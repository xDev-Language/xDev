/*
 * Copyright (c) 2024-2026 xDev-Language
 *
 * Library...: xDev GUI
 * File......: internal-gui-read.c
 *
 */

#include <string.h>
#include "internal-gui.h"
#include "terminal.h"

/* The fields of Read( GetList ) in their order; without them all one-line fields of the window */
static const HWND *s_readGets;
static size_t s_readGetCount;

/* Windows changed by Read() and their states before it */
typedef struct
{
   HWND *windows;
   int *states;
   size_t count;
} X_GUI_WINDOWS;

/* The edit fields made editable, the controls disabled and the labels hidden during Read() */
typedef struct
{
   X_GUI_WINDOWS edits;
   X_GUI_WINDOWS locked;
   X_GUI_WINDOWS covered;
} X_GUI_READ;

/* ---------------------------------------------------------------------------------------------
   Read() of a window, as ReadModal()
   --------------------------------------------------------------------------------------------- */

/* Add a window and its state to a list */
static void s_windowsAdd( X_GUI_WINDOWS *list, HWND window, int state )
{
   list->windows = ( HWND * )x_xrealloc( list->windows, ( list->count + 1 ) * sizeof( HWND ) );
   list->states = ( int * )x_xrealloc( list->states, ( list->count + 1 ) * sizeof( int ) );
   list->windows[list->count] = window;
   list->states[list->count] = state;
   ++list->count;
}

/* Release a list of windows */
static void s_windowsFree( X_GUI_WINDOWS *list )
{
   x_xfree( list->windows );
   x_xfree( list->states );
}

/* Whether a window is an edit field of one line, not the one of a combo box */
static int s_isLineEdit( HWND window )
{
   if( window == NULL || !x_guiHasClass( window, L"Edit" ) || x_guiIsMultiline( window ) )
      return 0;

   return !x_guiHasClass( GetParent( window ), L"ComboBox" );
}

/* The edit fields become editable; tables, toolbars and lists wait until the end of Read() */
static BOOL CALLBACK s_readCollect( HWND child, LPARAM data )
{
   X_GUI_READ *read = ( X_GUI_READ * )data;
   int field = x_guiHasClass( child, L"Edit" ) && !x_guiHasClass( GetParent( child ), L"ComboBox" );
   int locked = x_guiHasClass( child, x_guiBrowseClassName ) || x_guiToolbarOf( child ) != NULL;

   locked = locked || x_guiHasClass( child, WC_LISTVIEWW ) || x_guiHasClass( child, WC_TREEVIEWW );
   if( field )
      s_windowsAdd( &read->edits, child, ( GetWindowLongPtrW( child, GWL_STYLE ) & ES_READONLY ) != 0 );
   else if( locked && IsWindowEnabled( child ) )
      s_windowsAdd( &read->locked, child, 1 );

   return TRUE;
}

/* A field of GetList is reached also on a page of a tab control not shown now */
static int s_readReachable( HWND field )
{
   HWND window;

   for( window = field; window != NULL; window = GetParent( window ) )
   {
      if( !( GetWindowLongPtrW( window, GWL_STYLE ) & WS_CHILD ) )
         break;

      if( !x_guiShown( window ) && !x_guiHasClass( GetParent( window ), WC_TABCONTROLW ) )
         return 0;
   }

   return 1;
}

/* The one-line edit fields shown now, in the order of Tab; with GetList its fields on all pages */
static size_t s_readFields( HWND window, HWND *fields, size_t size )
{
   HWND first = GetNextDlgTabItem( window, NULL, FALSE );
   HWND item = first;
   size_t count = 0;
   int guard = 0;

   if( s_readGetCount > 0 )
   {
      size_t i;

      for( i = 0; i < s_readGetCount && count < size; ++i )
      {
         HWND get = s_readGets[i];
         int field = s_isLineEdit( get ) || GetPropW( get, x_guiRowsProperty ) != NULL || x_guiIsChoice( get );

         if( field && s_readReachable( get ) && IsWindowEnabled( get ) )
            fields[count++] = get;
      }
      return count;
   }

   while( item != NULL && count < size && guard++ < 1000 )
   {
      if( s_isLineEdit( item ) && IsWindowVisible( item ) && IsWindowEnabled( item ) )
         fields[count++] = item;

      item = GetNextDlgTabItem( window, item, FALSE );
      if( item == first )
         break;
   }

   return count;
}

/* The field with the focus may end Read() only with a valid value */
static int s_readValid( HWND window )
{
   HWND focus = GetFocus();

   return !s_isLineEdit( focus ) || !IsChild( window, focus ) || x_guiEditValid( focus, window, x_guiMaskOf( focus ) );
}

/* The cursor goes to the start of the field, without selecting its text, as in a GET; the page of
   a tab control holding the field comes up */
static void s_readFocus( HWND field )
{
   const X_GUI_MASK *mask = x_guiMaskOf( field );
   HWND window;

   for( window = field; window != NULL; window = GetParent( window ) )
   {
      if( !( GetWindowLongPtrW( window, GWL_STYLE ) & WS_CHILD ) )
         break;

      if( !x_guiShown( window ) && x_guiHasClass( GetParent( window ), WC_TABCONTROLW ) )
      {
         SendMessageW( GetParent( window ), TCM_SETCURSEL, ( WPARAM )( GetDlgCtrlID( window ) - 1 ), 0 );
         x_guiTabLayout( GetParent( window ), 1 );
      }
   }

   SetFocus( field );

   /* A number stays selected, so the first key typed starts it again, as in a numeric GET */
   if( GetFocus() == field && !( mask != NULL && mask->type == 'N' ) )
      SendMessageW( field, EM_SETSEL, 0, 0 );
}

/* Tab and Shift+Tab go to the next and the previous field, around the end */
static void s_readTab( HWND window, int previous )
{
   HWND fields[256];
   HWND focus = GetFocus();
   size_t count = s_readFields( window, fields, 256 );
   size_t i;

   if( count == 0 )
      return;

   for( i = 0; i < count && fields[i] != focus; ++i )
      ;

   if( i == count )
      s_readFocus( fields[0] );
   else if( previous )
      s_readFocus( fields[IIF( i == 0, count - 1, i - 1 )] );
   else
      s_readFocus( fields[IIF( i + 1 == count, 0, i + 1 )] );
}

/* Enter goes to the next field and on the last field ends Read(); returns 1 when Read() ends */
static int s_readEnter( HWND window )
{
   HWND fields[256];
   HWND focus = GetFocus();
   size_t count = s_readFields( window, fields, 256 );
   size_t i;

   for( i = 0; i < count && fields[i] != focus; ++i )
      ;

   if( i == count )
   {
      if( count > 0 )
         s_readFocus( fields[0] );
      return 0;
   }

   if( i + 1 == count )
      return s_readValid( window );

   s_readFocus( fields[i + 1] );

   return 0;
}

/* EndRead(): end the running Read(), with saving or without */
void x_guiWindowEndRead( int save )
{
   if( x_guiReading > 0 )
      x_guiReadEnd = save;
}

/* ReadModal() of a window: the fields are edited until Enter on the last one, PgDn, PgUp or Esc;
   returns .T. when the values are saved */
int x_guiWindowRead( HWND window, const HWND *gets, size_t count, int first )
{
   X_GUI_READ read;
   HWND root = GetAncestor( window, GA_ROOT );
   HMENU menu = GetMenu( root );
   HWND previous = GetFocus();
   HWND outer = x_guiReadWindow;
   const HWND *outerGets = s_readGets;
   size_t outerCount = s_readGetCount;
   HWND fields[256];
   size_t start;
   UINT *menuStates = NULL;
   int menuCount = IIF( menu != NULL, GetMenuItemCount( menu ), 0 );
   int result = -1;
   int lastKey = 27;
   int status = 1;
   MSG message;
   size_t i;

   memset( &read, 0, sizeof( read ) );
   ++x_guiReading;
   x_guiReadEnd = -1;
   x_guiReadWindow = window;
   s_readGets = gets;
   s_readGetCount = count;
   /* The edit fields of GetList come above what lies under them, in their order; check boxes and radio buttons
      keep their place, because the order of the windows makes the groups of radio buttons */
   for( i = 0; i < count; ++i )
   {
      HWND after = HWND_TOP;
      size_t j;

      if( x_guiIsChoice( gets[i] ) )
         continue;

      for( j = i; j-- > 0; )
      {
         if( GetParent( gets[j] ) == GetParent( gets[i] ) && !x_guiIsChoice( gets[j] ) )
         {
            after = gets[j];
            break;
         }
      }

      SetWindowPos( gets[i], after, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE );
   }

   /* The labels showing the values under the fields of GetList wait hidden, as SAY under GET; also on the pages
      of a tab control not shown now */
   for( i = 0; i < count; ++i )
   {
      HWND child;
      RECT field, label, common;

      GetWindowRect( gets[i], &field );
      for( child = GetWindow( GetParent( gets[i] ), GW_CHILD ); child != NULL; child = GetWindow( child, GW_HWNDNEXT ) )
      {
         if( !x_guiHasClass( child, L"Static" ) || !x_guiShown( child ) || !GetWindowRect( child, &label ) )
            continue;

         if( IntersectRect( &common, &field, &label ) )
         {
            ShowWindow( child, SW_HIDE );
            s_windowsAdd( &read.covered, child, 1 );
         }
      }
   }

   EnumChildWindows( window, s_readCollect, ( LPARAM )&read );
   /* Without GetList all edit fields of the window become editable; the fields of GetList are new and editable */
   if( count == 0 )
   {
      for( i = 0; i < read.edits.count; ++i )
         SendMessageW( read.edits.windows[i], EM_SETREADONLY, FALSE, 0 );
   }

   for( i = 0; i < read.locked.count; ++i )
      EnableWindow( read.locked.windows[i], FALSE );

   /* The menu cannot open another part of the program in the middle of Read() */
   if( menuCount > 0 )
   {
      menuStates = ( UINT * )x_xgrabArray( ( size_t )menuCount, sizeof( UINT ) );
      for( i = 0; i < ( size_t )menuCount; ++i )
      {
         menuStates[i] = GetMenuState( menu, ( UINT )i, MF_BYPOSITION );
         EnableMenuItem( menu, ( UINT )i, MF_BYPOSITION | MF_GRAYED );
      }

      DrawMenuBar( root );
   }

   /* Read() starts in the field nGet of GetList, its page coming up, or in the first field seen now, on the page
      of a tab control shown now */
   start = s_readFields( window, fields, 256 );
   for( i = 0; i < start && !IsWindowVisible( fields[i] ); ++i )
      ;

   if( first >= 1 && ( size_t )first <= count && IsWindowEnabled( gets[first - 1] ) )
      s_readFocus( gets[first - 1] );
   else if( start > 0 )
      s_readFocus( fields[IIF( i < start, i, 0 )] );

   x_guiReadScoreboard( root, 1 );

   while( result < 0 && IsWindow( window ) && ( status = ( int )GetMessageW( &message, NULL, 0, 0 ) ) > 0 )
   {
      int plain = GetKeyState( VK_CONTROL ) >= 0 && GetKeyState( VK_MENU ) >= 0;
      int rows = GetPropW( message.hwnd, x_guiRowsProperty ) != NULL;

      if( message.message == WM_KEYDOWN && IsChild( window, message.hwnd ) && plain )
      {
         if( message.wParam == VK_ESCAPE )
         {
            result = 0;
            continue;
         }

         if( message.wParam == VK_NEXT || message.wParam == VK_PRIOR )
         {
            if( s_readValid( window ) )
            {
               result = 1;
               lastKey = IIF( message.wParam == VK_NEXT, 3, 18 );
            }
            continue;
         }

         if( message.wParam == VK_TAB )
         {
            s_readTab( window, GetKeyState( VK_SHIFT ) < 0 );
            continue;
         }

         /* In a GET of several rows Shift+Enter begins a new line and Enter goes to the next field */
         if( message.wParam == VK_RETURN && GetKeyState( VK_SHIFT ) < 0 && rows )
         {
            SendMessageW( message.hwnd, EM_REPLACESEL, TRUE, ( LPARAM )L"\r\n" );
            continue;
         }

         if( message.wParam == VK_RETURN && ( !x_guiIsMultiline( message.hwnd ) || rows ) )
         {
            if( s_readEnter( window ) )
            {
               result = 1;
               lastKey = 13;
            }
            continue;
         }
      }

      x_guiDispatchMessage( &message );
      if( x_guiReadEnd >= 0 )
      {
         if( x_guiReadEnd == 0 || s_readValid( window ) )
         {
            result = x_guiReadEnd;
            lastKey = IIF( x_guiReadEnd == 1, 3, 27 );
         }

         x_guiReadEnd = -1;
      }
   }

   if( status == 0 )
      PostQuitMessage( ( int )message.wParam );

   if( IsWindow( window ) )
   {
      for( i = 0; i < read.edits.count; ++i )
      {
         if( IsWindow( read.edits.windows[i] ) )
            SendMessageW( read.edits.windows[i], EM_SETREADONLY, ( WPARAM )read.edits.states[i], 0 );
      }

      for( i = 0; i < read.locked.count; ++i )
      {
         if( IsWindow( read.locked.windows[i] ) )
            EnableWindow( read.locked.windows[i], TRUE );
      }

      /* The focus goes back where it was before Read(), without checking the field it leaves */
      x_guiValidating = 1;
      if( previous != NULL && IsWindow( previous ) && IsWindowEnabled( previous ) && IsWindowVisible( previous ) )
         SetFocus( previous );

      x_guiValidating = 0;
   }

   if( menuStates != NULL )
   {
      for( i = 0; i < ( size_t )menuCount && IsMenu( menu ); ++i )
         EnableMenuItem( menu, ( UINT )i, MF_BYPOSITION | ( menuStates[i] & ( MF_GRAYED | MF_DISABLED ) ) );

      if( IsWindow( root ) )
         DrawMenuBar( root );

      x_xfree( menuStates );
   }

   if( IsWindow( root ) )
      x_guiReadScoreboard( root, 0 );

   for( i = 0; i < read.covered.count; ++i )
   {
      if( IsWindow( read.covered.windows[i] ) )
         ShowWindow( read.covered.windows[i], SW_SHOW );
   }

   s_windowsFree( &read.edits );
   s_windowsFree( &read.locked );
   s_windowsFree( &read.covered );
   /* The GETs of @var give their values back to the variables, as READ does */
   if( result == 1 )
   {
      for( i = 0; i < count; ++i )
      {
         X_ITEM *var = ( X_ITEM * )GetPropW( gets[i], x_guiVarProperty );

         if( var != NULL && IsWindow( gets[i] ) )
         {
            X_ITEM value = X_ITEM_NIL_INIT;

            x_guiEditCurrent( gets[i], x_guiMaskOf( gets[i] ), &value );
            x_itemCopy( var, &value );
            x_itemClear( &value );
         }
      }
   }

   /* LastKey() tells how ReadModal() ended: K_ESC, K_PGDN, K_PGUP or K_ENTER */
   x_inkeySetLast( IIF( result == 1, lastKey, 27 ) );
   --x_guiReading;
   x_guiReadWindow = outer;
   s_readGets = outerGets;
   s_readGetCount = outerCount;
   /* The window the user closed during Read() closes now */
   if( x_guiReading == 0 && x_guiReadClose != NULL )
   {
      if( IsWindow( x_guiReadClose ) )
         PostMessageW( x_guiReadClose, WM_CLOSE, 0, 0 );

      x_guiReadClose = NULL;
   }

   return result == 1;
}

/* Return the picture of an edit field */
void x_guiEditPicture( X_CALL_CTX *ctx, HWND edit )
{
   const X_GUI_MASK *mask = x_guiMaskOf( edit );

   x_retc( ctx, IIF( mask != NULL, mask->picture, "" ) );
}

/* Set the picture of an edit field; the value is shown again through it */
void x_guiEditSetPicture( X_CALL_CTX *ctx, HWND edit, const X_ITEM *picture )
{
   X_GUI_MASK *mask;
   X_ITEM value __attribute__( ( cleanup( x_itemClear ) ) ) = X_ITEM_NIL_INIT;

   if( x_guiIsMultiline( edit ) || picture == NULL || !X_IS_STRING( picture ) )
   {
      x_guiRaise( ctx, "GUIEDIT:PICTURE" );
      return;
   }

   x_guiEditCurrent( edit, x_guiMaskOf( edit ), &value );
   mask = x_guiMaskEnsure( edit, IIF( x_guiMaskOf( edit ) != NULL, x_guiMaskOf( edit )->type, 'C' ) );
   x_xfree( mask->picture );
   mask->picture = x_strdup( x_itemGetCPtr( picture ) );
   x_guiMaskParse( mask, mask->type );
   x_guiMaskStyle( edit, mask );
   x_callbackEnterDll();
   x_guiEditShow( edit, mask, &value );
   x_callbackLeaveDll();
   x_callbackRaisePending();
   x_itemReturn( ctx, picture );
}

/* Return the value of an edit field in its type */
void x_guiEditValue( X_CALL_CTX *ctx, HWND edit )
{
   x_guiEditCurrent( edit, x_guiMaskOf( edit ), ctx->result );
}

/* Set the value of an edit field: a text, a number or a date */
void x_guiEditSetValue( X_CALL_CTX *ctx, HWND edit, const X_ITEM *value )
{
   X_GUI_MASK *mask = x_guiMaskOf( edit );
   char type = 'C';
   int valid;

   valid = value != NULL && ( X_IS_STRING( value ) || X_IS_NUMERIC( value ) || X_IS_DATE( value ) );
   valid = valid && ( X_IS_STRING( value ) || !x_guiIsMultiline( edit ) );
   if( !valid )
   {
      x_guiRaise( ctx, "GUIEDIT:VALUE" );
      return;
   }

   if( X_IS_STRING( value ) && mask == NULL )
   {
      x_guiSetText( ctx, edit, value );
      x_itemReturn( ctx, value );
      return;
   }

   if( X_IS_NUMERIC( value ) )
      type = 'N';
   else if( X_IS_DATE( value ) )
      type = 'D';

   mask = x_guiMaskEnsure( edit, type );
   x_callbackEnterDll();
   x_guiEditShow( edit, mask, value );
   x_callbackLeaveDll();
   x_callbackRaisePending();
   x_itemReturn( ctx, value );
}
