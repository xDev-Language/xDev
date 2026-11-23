/*
 * Copyright (c) 2024-2026 xDev-Language
 *
 * Library...: xDev GUI
 * File......: internal-gui-edit.c
 *
 */

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wctype.h>
#include "internal-gui.h"

static const wchar_t s_maskProperty[] = L"xDev.GUI.Mask";

/* ---------------------------------------------------------------------------------------------
   Edit fields with a picture
   --------------------------------------------------------------------------------------------- */

/* Posted to an edit field whose OnValid refused the value, so it takes the focus back */
#define X_GUI_REFOCUS ( WM_APP + 0x100 )

/* The picture data of an edit field, or NULL */
X_GUI_MASK *x_guiMaskOf( HWND edit )
{
   return ( X_GUI_MASK * )GetPropW( edit, s_maskProperty );
}

/* Release the picture data of an edit field */
static void s_maskRelease( HWND edit )
{
   X_GUI_MASK *mask = ( X_GUI_MASK * )RemovePropW( edit, s_maskProperty );

   if( mask == NULL )
      return;

   x_xfree( mask->picture );
   x_xfree( mask->mask );
   x_xfree( mask );
}

/* The template characters that take a character; the others are shown as they are */
static int s_maskSlot( const X_GUI_MASK *mask, size_t pos )
{
   return mask->mask != NULL && pos < mask->length && wcschr( L"9#ANXLY!", towupper( mask->mask[pos] ) ) != NULL;
}

/* The first template position from pos on that takes a character */
static size_t s_maskNext( const X_GUI_MASK *mask, size_t pos )
{
   while( pos < mask->length && !s_maskSlot( mask, pos ) )
      ++pos;

   return pos;
}

/* c as the template character slot takes it, 0 when it does not take it */
static wchar_t s_maskAccept( wchar_t c, wchar_t slot, int upper )
{
   wchar_t big = ( wchar_t )( UINT_PTR )CharUpperW( ( LPWSTR )( UINT_PTR )c );

   switch( towupper( slot ) )
   {
   case L'9':
      return IIF( iswdigit( c ), c, 0 );
   case L'#':
      return IIF( iswdigit( c ) || c == L' ' || c == L'+' || c == L'-', c, 0 );
   case L'A':
      if( !IsCharAlphaW( c ) )
         return 0;

      return IIF( upper, big, c );
   case L'N':
      if( !IsCharAlphaNumericW( c ) )
         return 0;

      return IIF( upper, big, c );
   case L'L':
      if( wcschr( L"TFYN", big ) == NULL )
         return 0;

      return IIF( big == L'T' || big == L'Y', L'T', L'F' );
   case L'Y':
      if( wcschr( L"TFYN", big ) == NULL )
         return 0;

      return IIF( big == L'T' || big == L'Y', L'Y', L'N' );
   case L'!':
      return big;
   }

   return IIF( upper, big, c );
}

/* The text of an edit field as UTF-16 */
static wchar_t *s_editText( HWND edit, size_t *length )
{
   int size = GetWindowTextLengthW( edit );
   wchar_t *text = ( wchar_t * )x_xgrabArray( ( size_t )size + 1, sizeof( wchar_t ) );

   *length = ( size_t )GetWindowTextW( edit, text, size + 1 );

   return text;
}

/* Exactly one character for every template position: empty places are spaces, literals their own */
static wchar_t *s_maskFit( const X_GUI_MASK *mask, const wchar_t *text, size_t length )
{
   wchar_t *buffer = ( wchar_t * )x_xgrabArray( mask->length + 1, sizeof( wchar_t ) );
   size_t i;

   for( i = 0; i < mask->length; ++i )
   {
      if( i < length )
         buffer[i] = text[i];
      else
         buffer[i] = IIF( s_maskSlot( mask, i ), L' ', mask->mask[i] );
   }

   buffer[mask->length] = L'\0';

   return buffer;
}

/* The text of an edit field fitted to its template */
static wchar_t *s_maskBuffer( HWND edit, const X_GUI_MASK *mask )
{
   size_t length;
   wchar_t *text = s_editText( edit, &length );
   wchar_t *buffer = s_maskFit( mask, text, length );

   x_xfree( text );

   return buffer;
}

/* Set the text of an edit field and put the caret at a position */
static void s_maskPut( HWND edit, const wchar_t *text, size_t caret )
{
   SetWindowTextW( edit, text );
   SendMessageW( edit, EM_SETSEL, caret, caret );
}

/* Set UTF-8 text in an edit field, fitted to the template when there is one */
static void s_editSetUtf8( HWND edit, const X_GUI_MASK *mask, const char *text, size_t length )
{
   size_t size;
   wchar_t *wide = ( wchar_t * )x_utf8ToUtf16Alloc( text, length, &size );
   wchar_t *fit;

   if( mask != NULL && mask->mask != NULL )
   {
      fit = s_maskFit( mask, wide, size );
      x_xfree( wide );
      wide = fit;
   }

   SetWindowTextW( edit, wide );
   x_xfree( wide );
}

/* A key typed in a field with a template goes to the next position that takes it */
static void s_maskType( HWND edit, X_GUI_MASK *mask, wchar_t c )
{
   DWORD start = 0, end = 0;
   wchar_t *buffer = s_maskBuffer( edit, mask );
   wchar_t accepted;
   size_t pos, i;

   SendMessageW( edit, EM_GETSEL, ( WPARAM )&start, ( LPARAM )&end );

   /* @K: the first key replaces the whole value; a selection is replaced by the key */
   if( mask->clear && !mask->typed )
   {
      start = 0;
      end = ( DWORD )mask->length;
   }

   for( i = start; i < end && i < mask->length; ++i )
   {
      if( s_maskSlot( mask, i ) )
         buffer[i] = L' ';
   }

   mask->typed = 1;
   pos = s_maskNext( mask, start );
   accepted = 0;
   if( pos < mask->length )
      accepted = s_maskAccept( c, mask->mask[pos], mask->upper );

   if( accepted == 0 )
      MessageBeep( MB_OK );
   else
   {
      buffer[pos] = accepted;
      s_maskPut( edit, buffer, s_maskNext( mask, pos + 1 ) );
   }

   x_xfree( buffer );
}

/* A deleted character pulls the rest of its group of template characters to the left */
static void s_maskDelete( HWND edit, const X_GUI_MASK *mask, size_t pos )
{
   wchar_t *buffer = s_maskBuffer( edit, mask );
   size_t end = pos;

   if( s_maskSlot( mask, pos ) )
   {
      while( end + 1 < mask->length && s_maskSlot( mask, end + 1 ) )
      {
         buffer[end] = buffer[end + 1];
         ++end;
      }

      buffer[end] = L' ';
   }

   s_maskPut( edit, buffer, pos );
   x_xfree( buffer );
}

static void s_maskClearSelection( HWND edit, const X_GUI_MASK *mask );

/* Backspace in a field with a template deletes the character before the caret */
static void s_maskBackSpace( HWND edit, const X_GUI_MASK *mask )
{
   DWORD start = 0, end = 0;
   size_t pos;

   SendMessageW( edit, EM_GETSEL, ( WPARAM )&start, ( LPARAM )&end );
   if( end > start )
   {
      s_maskClearSelection( edit, mask );
      return;
   }

   pos = start;
   while( pos > 0 && !s_maskSlot( mask, pos - 1 ) )
      --pos;

   if( pos == 0 )
      MessageBeep( MB_OK );
   else
      s_maskDelete( edit, mask, pos - 1 );
}

/* Delete the selected characters of a field with a template; the literals stay */
static void s_maskClearSelection( HWND edit, const X_GUI_MASK *mask )
{
   DWORD start = 0, end = 0;
   wchar_t *buffer;
   size_t i;

   SendMessageW( edit, EM_GETSEL, ( WPARAM )&start, ( LPARAM )&end );
   buffer = s_maskBuffer( edit, mask );
   for( i = start; i < end && i < mask->length; ++i )
   {
      if( s_maskSlot( mask, i ) )
         buffer[i] = L' ';
   }

   s_maskPut( edit, buffer, start );
   x_xfree( buffer );
}

/* A number has at most one point, the minus only in front and the digits allowed by the picture;
   the minus takes the place of one digit, as in Transform() */
static int s_numberFits( const X_GUI_MASK *mask, const wchar_t *text )
{
   int points = 0, whole = 0, fraction = 0;
   size_t i;

   for( i = 0; text[i] != L'\0'; ++i )
   {
      if( text[i] == L'-' && i == 0 )
      {
         ++whole;
         continue;
      }

      if( text[i] == L'.' )
         ++points;
      else if( !iswdigit( text[i] ) )
         return 0;
      else if( points == 0 )
         ++whole;
      else
         ++fraction;
   }

   if( points > 1 || ( mask->decimals == 0 && points != 0 ) )
      return 0;

   if( mask->digits != 0 && whole > mask->digits )
      return 0;

   return mask->decimals < 0 || fraction <= mask->decimals;
}

/* A key typed in a numeric field; a key that would make a wrong number is refused */
static void s_numberType( HWND edit, const X_GUI_MASK *mask, wchar_t c )
{
   DWORD start = 0, end = 0;
   size_t length, caret;
   wchar_t *text = s_editText( edit, &length );
   wchar_t *candidate = ( wchar_t * )x_xgrabArray( length + 2, sizeof( wchar_t ) );

   SendMessageW( edit, EM_GETSEL, ( WPARAM )&start, ( LPARAM )&end );
   start = IIF( start > length, ( DWORD )length, start );
   end = IIF( end > length || end < start, start, end );
   if( c == L',' )
      c = L'.';

   /* The minus over the whole selected number starts a new negative number */
   if( c == L'-' && start == 0 && end == length && length > 0 )
   {
      candidate[0] = L'-';
      candidate[1] = L'\0';
      caret = 1;
   }
   else if( c == L'-' )
   {
      if( length > 0 && text[0] == L'-' )
      {
         memcpy( candidate, text + 1, length * sizeof( wchar_t ) );
         caret = IIF( start > 0, start - 1, 0 );
      }
      else
      {
         candidate[0] = L'-';
         memcpy( candidate + 1, text, ( length + 1 ) * sizeof( wchar_t ) );
         caret = start + 1;
      }
   }
   else
   {
      memcpy( candidate, text, start * sizeof( wchar_t ) );
      candidate[start] = c;
      memcpy( candidate + start + 1, text + end, ( length - end + 1 ) * sizeof( wchar_t ) );
      caret = start + 1;
   }

   if( ( c != L'-' && c != L'.' && !iswdigit( c ) ) || !s_numberFits( mask, candidate ) )
      MessageBeep( MB_OK );
   else
      s_maskPut( edit, candidate, caret );

   x_xfree( candidate );
   x_xfree( text );
}

/* While editing the plain number, otherwise the number through the picture */
static void s_numberShow( HWND edit, const X_GUI_MASK *mask, int focused )
{
   X_ITEM value __attribute__( ( cleanup( x_itemClear ) ) ) = X_ITEM_NIL_INIT;
   X_ITEM shown __attribute__( ( cleanup( x_itemClear ) ) ) = X_ITEM_NIL_INIT;
   char buffer[64];
   const char *text = buffer;

   if( ( focused || mask->picture[0] == '\0' ) && mask->decimals >= 0 )
      snprintf( buffer, sizeof( buffer ), "%.*f", mask->decimals, mask->number );
   else if( focused || mask->picture[0] == '\0' )
      snprintf( buffer, sizeof( buffer ), "%.15g", mask->number );
   else
   {
      x_itemPutND( &value, mask->number );
      x_transform( &shown, &value, mask->picture, strlen( mask->picture ) );
      for( text = x_itemGetCPtr( &shown ); *text == ' '; ++text )
         ;
   }

   s_editSetUtf8( edit, NULL, text, strlen( text ) );
   if( focused )
      SendMessageW( edit, EM_SETSEL, 0, -1 );
}

/* Read the number of a numeric field; a comma counts as the point */
static void s_numberRead( HWND edit, X_GUI_MASK *mask )
{
   size_t length, i;
   wchar_t *text = s_editText( edit, &length );

   for( i = 0; i < length; ++i )
   {
      if( text[i] == L',' )
         text[i] = L'.';
   }

   mask->number = wcstod( text, NULL );
   x_xfree( text );
}

/* Show a date in the format of SET DATE */
static void s_dateShow( HWND edit, const X_GUI_MASK *mask, long julian )
{
   char date[9] = "        ";
   char text[32];
   int year, month, day;

   if( julian != 0 )
   {
      x_dateDecode( julian, &year, &month, &day );
      snprintf( date, sizeof( date ), "%04d%02d%02d", year, month, day );
   }

   x_dateFormat( date, text, x_setDateFormat( NULL ) );
   s_editSetUtf8( edit, mask, text, strlen( text ) );
}

/* 0 for an empty or a wrong date; *filled tells whether the user typed any digit */
static long s_dateRead( HWND edit, int *filled )
{
   size_t length, size, i;
   wchar_t *wide = s_editText( edit, &length );
   char *text = x_utf16ToUtf8Alloc( ( const uint16_t * )wide, length, &size );
   long julian;

   *filled = 0;
   for( i = 0; i < length; ++i )
      *filled |= iswdigit( wide[i] ) != 0;

   julian = x_dateUnformat( text, x_setDateFormat( NULL ) );
   x_xfree( text );
   x_xfree( wide );

   return julian;
}

/* Functions after @, the template, and for a number the digits before and after the point */
void x_guiMaskParse( X_GUI_MASK *mask, char type )
{
   const char *picture = mask->picture;
   const char *space = strchr( picture, ' ' );
   const char *template = picture;
   const char *p;
   char dateMask[16];
   size_t i;

   x_xfree( mask->mask );
   mask->mask = NULL;
   mask->length = 0;
   mask->type = type;
   mask->upper = 0;
   mask->clear = 0;
   mask->remove = 0;
   mask->digits = 0;
   mask->decimals = -1;
   if( picture[0] == '@' )
   {
      for( p = picture + 1; *p != '\0' && p != space; ++p )
      {
         switch( toupper( ( unsigned char )*p ) )
         {
         case '!':
            mask->upper = 1;
            break;
         case 'K':
            mask->clear = 1;
            break;
         case 'R':
            mask->remove = 1;
            break;
         }
      }

      template = IIF( space != NULL, space + 1, "" );
   }

   if( type == 'D' )
   {
      /* The template follows SET DATE: every y, m and d takes a digit */
      for( p = x_setDateFormat( NULL ), i = 0; *p != '\0' && i < sizeof( dateMask ) - 1; ++p, ++i )
         dateMask[i] = ( char )IIF( strchr( "YMDymd", *p ) != NULL, '9', *p );

      dateMask[i] = '\0';
      template = dateMask;
   }

   if( type == 'N' )
   {
      if( *template != '\0' )
      {
         mask->decimals = 0;
         for( p = template; *p != '\0' && *p != '.'; ++p )
            mask->digits += strchr( "9#$*", *p ) != NULL;

         for( ; *p != '\0'; ++p )
            mask->decimals += strchr( "9#", *p ) != NULL;
      }
   }
   else if( *template != '\0' )
      mask->mask = ( wchar_t * )x_utf8ToUtf16Alloc( template, strlen( template ), &mask->length );
}

/* A number stands on the right; @! without a template is left to the edit field itself */
void x_guiMaskStyle( HWND edit, const X_GUI_MASK *mask )
{
   LONG_PTR style = GetWindowLongPtrW( edit, GWL_STYLE ) & ~( LONG_PTR )( ES_RIGHT | ES_UPPERCASE );

   if( mask->type == 'N' )
      style |= ES_RIGHT;
   else if( mask->upper && mask->mask == NULL )
      style |= ES_UPPERCASE;

   SetWindowLongPtrW( edit, GWL_STYLE, style );
   SendMessageW( edit, EM_LIMITTEXT, IIF( mask->mask != NULL, mask->length, 0 ), 0 );
   InvalidateRect( edit, NULL, TRUE );
}

/* The picture data of an edit field for a value of the type, made when missing */
X_GUI_MASK *x_guiMaskEnsure( HWND edit, char type )
{
   X_GUI_MASK *mask = x_guiMaskOf( edit );

   if( mask == NULL )
   {
      mask = ( X_GUI_MASK * )x_xgrab( sizeof( X_GUI_MASK ) );
      memset( mask, 0, sizeof( *mask ) );
      mask->picture = x_strdup( "" );
      SetPropW( edit, s_maskProperty, mask );
   }

   if( mask->type != type )
   {
      x_guiMaskParse( mask, type );
      x_guiMaskStyle( edit, mask );
   }

   return mask;
}

/* The value in the type of the field: text, number or date */
void x_guiEditCurrent( HWND edit, X_GUI_MASK *mask, X_ITEM *value )
{
   size_t length, size, i, j;
   wchar_t *text;
   int filled;

   if( mask != NULL && mask->type == 'N' )
   {
      if( GetFocus() == edit )
         s_numberRead( edit, mask );

      if( mask->decimals == 0 )
         x_itemPutNInt( value, ( int64_t )( mask->number + IIF( mask->number < 0, -0.5, 0.5 ) ) );
      else
         x_itemPutND( value, mask->number );
      return;
   }

   if( mask != NULL && mask->type == 'D' )
   {
      x_itemCopy( value, &( X_ITEM ){ X_IT_DATE, { .integer = s_dateRead( edit, &filled ) } } );
      return;
   }

   text = s_editText( edit, &length );
   /* @R: the literals of the template are not part of the value */
   if( mask != NULL && mask->mask != NULL && mask->remove )
   {
      for( i = j = 0; i < length; ++i )
      {
         if( s_maskSlot( mask, i ) )
            text[j++] = text[i];
      }

      length = j;
   }

   x_itemPutCLPtr( value, x_utf16ToUtf8Alloc( ( const uint16_t * )text, length, &size ), size );
   x_xfree( text );
}

/* Show a value in an edit field through its picture */
void x_guiEditShow( HWND edit, X_GUI_MASK *mask, const X_ITEM *value )
{
   X_ITEM shown __attribute__( ( cleanup( x_itemClear ) ) ) = X_ITEM_NIL_INIT;

   if( X_IS_NUMERIC( value ) )
   {
      mask->number = x_itemGetND( value );
      s_numberShow( edit, mask, GetFocus() == edit );
   }
   else if( X_IS_DATE( value ) )
      s_dateShow( edit, mask, ( long )value->u.integer );
   else if( mask->mask != NULL )
   {
      x_transform( &shown, value, mask->picture, strlen( mask->picture ) );
      s_editSetUtf8( edit, mask, x_itemGetCPtr( &shown ), x_itemGetCLen( &shown ) );
   }
   else
      s_editSetUtf8( edit, mask, x_itemGetCPtr( value ), x_itemGetCLen( value ) );
}

/* A field with a picture gets or loses the focus: a number is edited plain, a template starts at its first place */
static void s_maskFocus( HWND edit, X_GUI_MASK *mask, int focused )
{
   size_t first;

   if( focused )
      mask->typed = 0;

   if( mask->type == 'N' )
   {
      if( !focused )
         s_numberRead( edit, mask );

      s_numberShow( edit, mask, focused );
   }
   else if( focused && mask->mask != NULL )
   {
      first = s_maskNext( mask, 0 );
      SendMessageW( edit, EM_SETSEL, first, first );
   }
}

/* Paste into a field with a picture: the characters go in as typed keys */
static void s_maskPaste( HWND edit, X_GUI_MASK *mask )
{
   HANDLE data;
   const wchar_t *clip;
   wchar_t *text = NULL;
   size_t i;

   if( !OpenClipboard( edit ) )
      return;

   data = GetClipboardData( CF_UNICODETEXT );
   clip = IIF( data != NULL, ( const wchar_t * )GlobalLock( data ), NULL );
   if( clip != NULL )
   {
      text = ( wchar_t * )x_xgrabArray( wcslen( clip ) + 1, sizeof( wchar_t ) );
      wcscpy( text, clip );
      GlobalUnlock( data );
   }

   CloseClipboard();
   for( i = 0; text != NULL && text[i] != L'\0'; ++i )
   {
      if( text[i] >= L' ' )
      {
         if( mask->type == 'N' )
            s_numberType( edit, mask, text[i] );
         else
            s_maskType( edit, mask, text[i] );
      }
   }

   x_xfree( text );
}

/* A GET without PICTURE takes the clipboard as typed keys, so the text writes over the spaces of the
   variable instead of being refused at its length; line breaks and tabs become spaces, the ones at the end
   are dropped */
static void s_plainPaste( HWND edit )
{
   HANDLE data;
   const wchar_t *clip;
   wchar_t *text = NULL;
   size_t i, len;

   if( !OpenClipboard( edit ) )
      return;

   data = GetClipboardData( CF_UNICODETEXT );
   clip = IIF( data != NULL, ( const wchar_t * )GlobalLock( data ), NULL );
   if( clip != NULL )
   {
      text = ( wchar_t * )x_xgrabArray( wcslen( clip ) + 1, sizeof( wchar_t ) );
      wcscpy( text, clip );
      GlobalUnlock( data );
   }

   CloseClipboard();
   if( text == NULL )
      return;

   len = wcslen( text );
   while( len > 0 && wcschr( L"\r\n\t ", text[len - 1] ) != NULL )
      --len;

   for( i = 0; i < len; ++i )
   {
      wchar_t c = text[i];

      if( c == L'\n' && i > 0 && text[i - 1] == L'\r' )
         continue;

      if( c == L'\r' || c == L'\n' || c == L'\t' )
         c = L' ';
      else if( c < L' ' )
         continue;

      SendMessageW( edit, WM_CHAR, ( WPARAM )c, 0 );
   }

   x_xfree( text );
}

/* OnValid, and for a date its correctness, when the focus goes to another control of the same window;
   Cancel and other windows do not check */
int x_guiEditValid( HWND edit, HWND next, X_GUI_MASK *mask )
{
   X_ITEM *object = ( X_ITEM * )GetWindowLongPtrW( edit, GWLP_USERDATA );
   X_ITEM result __attribute__( ( cleanup( x_itemClear ) ) ) = X_ITEM_NIL_INIT;
   int valid = 1;
   int filled;

   if( x_guiValidating || next == NULL || GetAncestor( next, GA_ROOT ) != GetAncestor( edit, GA_ROOT ) )
      return 1;

   if( next == ( HWND )GetPropW( GetParent( edit ), x_guiCancelProperty ) )
      return 1;

   x_guiValidating = 1;
   if( mask != NULL && mask->type == 'D' && s_dateRead( edit, &filled ) == 0 && filled )
      valid = 0;
   else if( object != NULL && x_guiNotifyEvent( object, X_GUI_EVENT_VALID, &result, 0, NULL ) )
      valid = !X_IS_LOGICAL( &result ) || result.u.logical;

   x_guiValidating = 0;
   if( !valid )
   {
      MessageBeep( MB_ICONWARNING );
      PostMessageW( edit, X_GUI_REFOCUS, 0, 0 );
   }

   return valid;
}

/* SCOREBOARD: during ReadModal() the panel oStatusBar:scoreboard of the window shows "Ins" while
   SET INSERT is on; after ReadModal() it is empty */
void x_guiReadScoreboard( HWND root, int reading )
{
   HWND status = IIF( root != NULL, FindWindowExW( root, NULL, STATUSCLASSNAMEW, NULL ), NULL );
   const X_GUI_STATUS *parts = IIF( status != NULL, x_guiStatusOf( status ), NULL );
   size_t panels;
   int style;

   if( parts == NULL || parts->scoreboard < 1 )
      return;

   panels = IIF( parts->count > 0, parts->count, ( size_t )1 );
   if( ( size_t )parts->scoreboard > panels )
      return;

   style = IIF( parts->count > 0, parts->styles[parts->scoreboard - 1], 0 );
   x_guiStatusSetText( status, parts->scoreboard - 1, style, IIF( reading && x_setInsert, L"Ins", L"" ) );
}

/* A GET of several rows shows one row; while it has the focus it grows over the fields under it to the rows
   its wrapped text needs, at most its number of rows, and shrinks back to one row when the focus leaves */
void x_guiGetFit( HWND edit, int focused )
{
   const int rows = ( int )( INT_PTR )GetPropW( edit, x_guiRowsProperty );
   UINT dpi;
   int lines, margin, edge, width, height;
   UINT flags;
   TEXTMETRICW metrics;
   RECT frame;
   HDC dc;
   HGDIOBJ old;

   if( rows <= 1 )
      return;

   dpi = GetDpiForWindow( edit );
   margin = x_guiScale( 2, dpi );
   edge = GetSystemMetricsForDpi( SM_CXEDGE, dpi );
   dc = GetDC( edit );
   old = SelectObject( dc, x_guiFontOf( edit, dpi ) );
   GetTextMetricsW( dc, &metrics );
   SelectObject( dc, old );
   ReleaseDC( edit, dc );
   lines = IIF( focused, ( int )SendMessageW( edit, EM_GETLINECOUNT, 0, 0 ), 1 );
   if( lines < 1 )
      lines = 1;
   else if( lines > rows )
      lines = rows;

   GetWindowRect( edit, &frame );
   width = frame.right - frame.left;
   height = lines * metrics.tmHeight + 2 * edge + margin;
   flags = SWP_NOMOVE | SWP_NOACTIVATE | IIF( focused, 0, SWP_NOZORDER );
   SetWindowPos( edit, IIF( focused, HWND_TOP, NULL ), 0, 0, width, height, flags );
   if( !focused )
      SendMessageW( edit, EM_LINESCROLL, 0, -SendMessageW( edit, EM_GETFIRSTVISIBLELINE, 0, 0 ) );
}

/* The subclass of edit fields: pictures, the GETs and the check of OnValid */
LRESULT CALLBACK x_guiEditProc( HWND edit, UINT message, WPARAM wParam, LPARAM lParam, UINT_PTR id,
                                DWORD_PTR data )
{
   X_GUI_MASK *mask = x_guiMaskOf( edit );
   int plain = mask == NULL || ( mask->type != 'N' && mask->mask == NULL );
   LRESULT result;

   UNUSED( data );
   switch( message )
   {
   case WM_CHAR:
      /* A GET without PICTURE is as long as its variable: a character writes over the one under the cursor, and
         when inserting the last character falls off the end */
      if( GetPropW( edit, x_guiGetProperty ) != NULL && wParam >= L' ' && plain )
      {
         DWORD start = 0, end = 0;
         int length = GetWindowTextLengthW( edit );
         int limit = ( int )SendMessageW( edit, EM_GETLIMITTEXT, 0, 0 );

         SendMessageW( edit, EM_GETSEL, ( WPARAM )&start, ( LPARAM )&end );
         if( start == end && !x_setInsert && ( int )start < length )
            SendMessageW( edit, EM_SETSEL, start, start + 1 );
         else if( start == end && x_setInsert && length > 0 && length >= limit )
         {
            SendMessageW( edit, EM_SETSEL, ( WPARAM )( length - 1 ), length );
            SendMessageW( edit, EM_REPLACESEL, FALSE, ( LPARAM )L"" );
            SendMessageW( edit, EM_SETSEL, start, start );
         }
         break;
      }

      if( plain || ( wParam < L' ' && wParam != VK_BACK ) )
         break;

      if( mask->type == 'N' )
      {
         if( wParam == VK_BACK )
            break;

         s_numberType( edit, mask, ( wchar_t )wParam );
      }
      else if( wParam == VK_BACK )
         s_maskBackSpace( edit, mask );
      else
         s_maskType( edit, mask, ( wchar_t )wParam );
      return 0;

   case WM_KEYDOWN:
      /* A GET writes over the character under the cursor unless Insert turned SET INSERT on */
      if( wParam == VK_INSERT && GetPropW( edit, x_guiGetProperty ) != NULL )
      {
         if( GetKeyState( VK_SHIFT ) >= 0 && GetKeyState( VK_CONTROL ) >= 0 )
         {
            x_setInsert = !x_setInsert;
            x_guiReadScoreboard( GetAncestor( edit, GA_ROOT ), 1 );
            return 0;
         }
      }

      if( wParam == VK_DELETE && mask != NULL && mask->mask != NULL )
      {
         DWORD start = 0, end = 0;

         SendMessageW( edit, EM_GETSEL, ( WPARAM )&start, ( LPARAM )&end );
         if( end > start )
            s_maskClearSelection( edit, mask );
         else
            s_maskDelete( edit, mask, start );
         return 0;
      }
      break;

   case WM_PASTE:
      if( mask != NULL && ( mask->type == 'N' || mask->mask != NULL ) )
      {
         s_maskPaste( edit, mask );
         return 0;
      }

      if( GetPropW( edit, x_guiGetProperty ) != NULL )
      {
         s_plainPaste( edit );
         return 0;
      }
      break;

   case WM_CUT:
   case WM_CLEAR:
      if( mask != NULL && mask->mask != NULL )
      {
         if( message == WM_CUT )
            SendMessageW( edit, WM_COPY, 0, 0 );

         s_maskClearSelection( edit, mask );
         return 0;
      }
      break;

   case WM_SETFOCUS:
      result = DefSubclassProc( edit, message, wParam, lParam );
      if( mask != NULL )
         s_maskFocus( edit, mask, 1 );

      x_guiGetFit( edit, 1 );
      return result;

   case WM_KILLFOCUS:
      result = DefSubclassProc( edit, message, wParam, lParam );
      if( mask != NULL )
         s_maskFocus( edit, mask, 0 );

      x_guiGetFit( edit, 0 );
      x_guiEditValid( edit, ( HWND )wParam, mask );
      return result;

   /* A click in another GET takes the cursor to the start of the field */
   case WM_LBUTTONDOWN:
      if( GetPropW( edit, x_guiGetProperty ) != NULL && GetFocus() != edit )
      {
         SetFocus( edit );
         if( GetFocus() == edit )
            SendMessageW( edit, EM_SETSEL, 0, 0 );
         return 0;
      }
      break;

   case X_GUI_REFOCUS:
      SetFocus( edit );
      return 0;

   case WM_NCDESTROY:
      s_maskRelease( edit );
      RemoveWindowSubclass( edit, x_guiEditProc, id );
      break;
   }

   return DefSubclassProc( edit, message, wParam, lParam );
}
