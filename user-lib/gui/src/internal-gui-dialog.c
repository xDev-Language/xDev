/*
 * Copyright (c) 2024-2026 xDev-Language
 *
 * Library...: xDev GUI
 * File......: internal-gui-dialog.c
 *
 */

#include <string.h>
#include "internal-gui.h"
#include <commdlg.h>
#include <shellapi.h>

/* ---------------------------------------------------------------------------------------------
   Dialogs
   --------------------------------------------------------------------------------------------- */

/* A message box over the active window; the Yes/No kind returns .T. for Yes */
void x_guiMessage( X_CALL_CTX *ctx, X_GUI_MESSAGE kind )
{
   static const UINT styles[] = { 0, MB_OK | MB_ICONINFORMATION, MB_OK | MB_ICONWARNING, MB_OK | MB_ICONERROR,
                                  MB_YESNO | MB_ICONQUESTION };
   HWND owner = GetActiveWindow();
   wchar_t ownerTitle[256];
   wchar_t *text, *title = NULL;
   int answer;
   DWORD error;
   UINT style;

   text = x_guiText( ctx, x_paritem( ctx, 1 ), "GUIMESSAGE" );
   if( text == NULL )
      return;

   if( X_ISCHAR( ctx, 2 ) )
   {
      title = x_guiText( ctx, x_paritem( ctx, 2 ), "GUIMESSAGE" );
      if( title == NULL )
      {
         x_xfree( text );
         return;
      }
   }

   ownerTitle[0] = L'\0';
   if( title == NULL && owner != NULL )
      GetWindowTextW( owner, ownerTitle, sizeof( ownerTitle ) / sizeof( ownerTitle[0] ) );

   style = styles[kind] | IIF( owner == NULL, MB_TASKMODAL, 0 );
   x_callbackEnterDll();
   answer = MessageBoxW( owner, text, IIF( title != NULL, title, ownerTitle ), style );
   error = GetLastError();
   x_callbackLeaveDll();
   x_xfree( text );
   x_xfree( title );

   x_callbackRaisePending();
   if( answer == 0 )
      x_guiRaiseWin32( ctx, "MessageBoxW", error );
   else if( kind == X_GUI_MESSAGE_YESNO )
      x_retl( ctx, answer == IDYES );
}

/* Append a text to the double zero ended list of filters */
static int s_appendFilter( X_CALL_CTX *ctx, const X_ITEM *item, wchar_t **list, size_t *length )
{
   wchar_t *wide = x_guiText( ctx, item, "GUIFILEDIALOG" );
   size_t count;

   if( wide == NULL )
      return 0;

   count = wcslen( wide );
   if( count == 0 )
   {
      x_xfree( wide );
      x_guiRaise( ctx, "GUIFILEDIALOG" );
      return 0;
   }

   *list = ( wchar_t * )x_xrealloc( *list, ( *length + count + 2 ) * sizeof( wchar_t ) );
   memcpy( *list + *length, wide, count * sizeof( wchar_t ) );
   *length += count;
   ( *list )[( *length )++] = L'\0';
   ( *list )[*length] = L'\0';
   x_xfree( wide );

   return 1;
}

/* The filters { { cName, cPattern }, ... } as the list of OPENFILENAMEW; *valid is 0 with an error raised */
static wchar_t *s_filters( X_CALL_CTX *ctx, const X_ITEM *filters, int *valid )
{
   wchar_t *list = NULL;
   size_t length = 0;
   int64_t i;

   *valid = 1;
   if( filters == NULL || X_IS_NIL( filters ) )
      return NULL;

   *valid = X_IS_ARRAY( filters ) && x_arrayLen( filters ) > 0;
   if( !*valid )
      x_guiRaise( ctx, "GUIFILEDIALOG" );

   for( i = 1; *valid && i <= x_arrayLen( filters ); ++i )
   {
      const X_ITEM *filter = x_arrayGetItemPtr( filters, ( size_t )i );

      *valid = X_IS_ARRAY( filter ) && x_arrayLen( filter ) == 2;
      if( !*valid )
         x_guiRaise( ctx, "GUIFILEDIALOG" );
      else
      {
         *valid = s_appendFilter( ctx, x_arrayGetItemPtr( filter, 1 ), &list, &length );
         if( *valid )
            *valid = s_appendFilter( ctx, x_arrayGetItemPtr( filter, 2 ), &list, &length );
      }
   }

   if( !*valid && list != NULL )
   {
      x_xfree( list );
      list = NULL;
   }

   return list;
}

/* The dialog to open or, with save, to save a file; returns the file or "" when cancelled */
void x_guiFileDialog( X_CALL_CTX *ctx, int save )
{
   enum { FILE_CAPACITY = 32768 };
   OPENFILENAMEW dialog;
   wchar_t *title = NULL, *initial = NULL, *filters, *file, extension[16];
   const wchar_t *pattern;
   int valid, done;
   DWORD error;
   size_t i, size;
   char *text;

   filters = s_filters( ctx, x_paritem( ctx, 2 ), &valid );
   if( !valid )
      return;

   if( X_ISCHAR( ctx, 1 ) )
   {
      title = x_guiText( ctx, x_paritem( ctx, 1 ), "GUIFILEDIALOG" );
      valid = title != NULL;
   }

   if( valid && X_ISCHAR( ctx, 3 ) )
   {
      initial = x_guiText( ctx, x_paritem( ctx, 3 ), "GUIFILEDIALOG" );
      valid = initial != NULL;
   }

   if( valid && initial != NULL && wcslen( initial ) >= FILE_CAPACITY )
   {
      x_guiRaise( ctx, "GUIFILEDIALOG" );
      valid = 0;
   }

   if( !valid )
   {
      x_xfree( filters );
      x_xfree( title );
      x_xfree( initial );
      return;
   }

   file = ( wchar_t * )x_xgrabzArray( FILE_CAPACITY, sizeof( wchar_t ) );
   if( initial != NULL )
      wcscpy( file, initial );

   extension[0] = L'\0';
   pattern = IIF( filters != NULL, filters + wcslen( filters ) + 1, NULL );
   if( save && pattern != NULL && pattern[0] == L'*' && pattern[1] == L'.' && pattern[2] != L'*' )
   {
      for( i = 0; i + 1 < sizeof( extension ) / sizeof( extension[0] ); ++i )
      {
         if( pattern[i + 2] == L'\0' || pattern[i + 2] == L';' )
            break;

         extension[i] = pattern[i + 2];
      }

      extension[i] = L'\0';
   }

   memset( &dialog, 0, sizeof( dialog ) );
   dialog.lStructSize = sizeof( dialog );
   dialog.hwndOwner = GetActiveWindow();
   dialog.lpstrFilter = filters;
   dialog.nFilterIndex = 1;
   dialog.lpstrFile = file;
   dialog.nMaxFile = FILE_CAPACITY;
   dialog.lpstrTitle = title;
   dialog.lpstrDefExt = IIF( extension[0] != L'\0', extension, NULL );
   dialog.Flags = OFN_EXPLORER | OFN_HIDEREADONLY | OFN_PATHMUSTEXIST;
   dialog.Flags |= IIF( save, OFN_OVERWRITEPROMPT, OFN_FILEMUSTEXIST );

   x_callbackEnterDll();
   done = IIF( save, GetSaveFileNameW( &dialog ), GetOpenFileNameW( &dialog ) );
   error = CommDlgExtendedError();
   x_callbackLeaveDll();
   x_xfree( filters );
   x_xfree( title );
   x_xfree( initial );

   if( done )
   {
      text = x_utf16ToUtf8Alloc( ( const uint16_t * )file, wcslen( file ), &size );
      x_xfree( file );
      x_callbackRaisePending();
      x_retclen_buffer( ctx, text, size );
      return;
   }

   x_xfree( file );
   x_callbackRaisePending();
   if( error != 0 )
      x_guiRaiseWin32( ctx, IIF( save, "GetSaveFileNameW", "GetOpenFileNameW" ), error );
   else
      x_retc( ctx, "" );
}

/* The file opens in the program the system has for its type, e.g. a PDF in the PDF viewer */
void x_guiOpenFile( X_CALL_CTX *ctx )
{
   wchar_t *file;
   HINSTANCE done;

   if( !X_ISCHAR( ctx, 1 ) )
   {
      x_guiRaise( ctx, "GUIOPENFILE" );
      return;
   }

   file = x_guiText( ctx, x_paritem( ctx, 1 ), "GUIOPENFILE" );
   if( file == NULL )
      return;

   x_callbackEnterDll();
   done = ShellExecuteW( GetActiveWindow(), L"open", file, NULL, NULL, SW_SHOWNORMAL );
   x_callbackLeaveDll();
   x_xfree( file );
   x_callbackRaisePending();
   x_retl( ctx, ( INT_PTR )done > 32 );
}
