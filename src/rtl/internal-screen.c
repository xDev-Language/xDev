/*
 * Copyright (c) 2024-2026 xDev-Language
 *
 * Library...: xDev Runtime
 * File......: internal-screen.c
 *
 */

#include "classes.h"
#include "gtinfo.h"
#include "internal-private.h"
#include "operators.h"
#include "platform.h"
#include "statements.h"
#include "terminal.h"

#include "../../include/xdev/fileio.ch"
#include "../../include/xdev/inkey.ch"
#include "../../include/xdev/set.ch"
#include "../../include/xdev/setcurs.ch"

#include <ctype.h>
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* SetKey(): the action and the condition of one key */
typedef struct
{
   int key;
   X_ITEM action;
   X_ITEM condition;
} S_SETKEY;

/* The color and the character of ClearWin(), ClWin(), ClearEol(), ClEol() and SetClearA/B() */
int x_clearColor = 7;
int x_clearChar = ' ';

/* The keys of SetKey() */
static S_SETKEY *s_setKeys;
static size_t s_setKeyCount;

/* KbdStat(): a shift state of the GT and its bit in the keyboard status */
static const struct
{
   int shift;
   int bit;
} s_kbdStatBits[] = {
   { X_GTI_KBD_RSHIFT, 0x01 }, { X_GTI_KBD_LSHIFT, 0x02 }, { X_GTI_KBD_CTRL, 0x04 }, { X_GTI_KBD_ALT, 0x08 },
   { X_GTI_KBD_SCROLOCK, 0x10 }, { X_GTI_KBD_NUMLOCK, 0x20 }, { X_GTI_KBD_CAPSLOCK, 0x40 }, { X_GTI_KBD_INSERT, 0x80 }
};

/* ---------------------------------------------------------------------------------------------
   AChoice()
   --------------------------------------------------------------------------------------------- */

/* Is item index available? A logical value or a block in alSelectableItems decides; anything
   else, or no such element, means available */
int x_achoiceSelectable( X_CALL_CTX *ctx, const X_ITEM *selectable, int64_t index )
{
   const X_ITEM *element;

   if( selectable == NULL || X_IS_NIL( selectable ) )
      return 1;

   if( X_IS_LOGICAL( selectable ) )
      return x_itemGetL( selectable );

   if( !X_IS_ARRAY( selectable ) || index > x_arrayLen( selectable ) )
      return 1;

   element = x_arrayGetItemPtr( selectable, ( size_t )index );
   if( X_IS_LOGICAL( element ) )
      return x_itemGetL( element );

   if( X_IS_BLOCK( element ) )
   {
      X_ITEM block __attribute__( ( cleanup( x_itemClear ) ) ) = X_ITEM_NIL_INIT;
      X_ITEM result __attribute__( ( cleanup( x_itemClear ) ) ) = X_ITEM_NIL_INIT;

      x_itemCopy( &block, element );
      x_evalBlock( ctx, &result, &block, 0, NULL );

      return X_IS_LOGICAL( &result ) && x_itemGetL( &result );
   }

   return 1;
}

/* The first available item from index on, moving by step; 0 when there is none */
int64_t x_achoiceFind( X_CALL_CTX *ctx, const X_ITEM *selectable, int64_t count, int64_t index, int step )
{
   for( ; index >= 1 && index <= count; index += step )
   {
      if( x_achoiceSelectable( ctx, selectable, index ) )
         return index;
   }

   return 0;
}

/* The window rows, items from first on; the current item in the enhanced color, unavailable
   items in the unselected color */
void x_achoiceDraw( X_CALL_CTX *ctx, const X_ITEM *items, const X_ITEM *selectable, int64_t count, int64_t first,
                    int64_t current, const unsigned short rect[4] )
{
   int width = rect[3] - rect[1] + 1;
   char *line = ( char * )x_xgrab( ( size_t )width * 4 + 1 );
   int row;

   x_gtDispBegin();
   for( row = 0; row <= rect[2] - rect[0]; ++row )
   {
      int64_t index = first + row;
      size_t size = 0;
      size_t chars = 0;
      int color = 0;

      if( index <= count )
      {
         const X_ITEM *item = x_arrayGetItemPtr( items, ( size_t )index );

         size = x_utf8Offset( item->u.string.data, item->u.string.len, ( size_t )width );
         chars = x_utf8Length( item->u.string.data, size );
         memcpy( line, item->u.string.data, size );

         if( index == current )
            color = 1;
         else if( !x_achoiceSelectable( ctx, selectable, index ) )
            color = 4;
      }

      memset( line + size, ' ', ( size_t )width - chars );
      size += ( size_t )width - chars;
      x_gtColorSelect( ( unsigned short )color );
      x_gtWriteAt( ( unsigned short )( rect[0] + row ), rect[1], ( unsigned char * )line, ( unsigned short )size );
   }

   x_gtColorSelect( 0 );
   x_gtDispEnd();
   x_xfree( line );
}

/* Call the user block with the mode, the current item and its window row; the result is its answer */
int x_achoiceUser( X_CALL_CTX *ctx, const X_ITEM *userBlock, int mode, int64_t current, int64_t first )
{
   X_ITEM block __attribute__( ( cleanup( x_itemClear ) ) ) = X_ITEM_NIL_INIT;
   X_ITEM result __attribute__( ( cleanup( x_itemClear ) ) ) = X_ITEM_NIL_INIT;
   X_ITEM modeItem = { X_IT_INTEGER, { .integer = mode } };
   X_ITEM currentItem = { X_IT_INTEGER, { .integer = current } };
   X_ITEM rowItem = { X_IT_INTEGER, { .integer = current - first } };
   X_ARG args[3] = { { &modeItem, 0 }, { &currentItem, 0 }, { &rowItem, 0 } };

   x_itemCopy( &block, userBlock );
   x_evalBlock( ctx, &result, &block, 3, args );

   return IIF( X_IS_NUMERIC( &result ), x_itemGetNI( &result ), 2 );
}

/* ---------------------------------------------------------------------------------------------
   Colors and screen areas
   --------------------------------------------------------------------------------------------- */

/* The color argument n: a color attribute or a color string such as "W+/B"; def without it */
int x_colorArg( X_CALL_CTX *ctx, int n, int def )
{
   int color;

   if( X_ISNUM( ctx, n ) )
      return x_parni( ctx, n ) & 0xFF;

   if( X_ISCHAR( ctx, n ) )
   {
      color = x_gtColorToN( x_parc( ctx, n ) );
      return IIF( color < 0, def, color );
   }

   return def;
}

/* The area from arguments 1 to 4; the top left corner defaults to 0, 0 or, with fromCursor, to the
   cursor; the bottom right corner to the end of the screen */
void x_screenRange( X_CALL_CTX *ctx, unsigned short rect[4], int ordered, int fromCursor )
{
   unsigned short max[2] = { x_gtMaxRow(), x_gtMaxCol() };
   unsigned short start[2] = { 0, 0 };
   int i;

   if( fromCursor )
      x_gtGetPos( &start[0], &start[1] );

   for( i = 0; i < 4; ++i )
   {
      int64_t limit = max[i % 2];
      int64_t value = x_parnintdef( ctx, i + 1, IIF( i < 2, start[i], limit ) );

      if( value < 0 )
         value = 0;
      else if( value > limit )
         value = limit;

      rect[i] = ( unsigned short )value;
   }

   if( ordered )
   {
      for( i = 0; i < 2; ++i )
      {
         if( rect[i] > rect[i + 2] )
         {
            unsigned short swap = rect[i];

            rect[i] = rect[i + 2];
            rect[i + 2] = swap;
         }
      }
   }
}

/* A screen cell character argument: the first UTF-8 character of a string or a code point; a cell holds
   16 bits, so a character above U+FFFF becomes U+FFFD */
int x_cellChar( X_CALL_CTX *ctx, int n, int def )
{
   uint32_t code = x_ucodeArg( ctx, n, ( uint32_t )def );

   return IIF( code > 0xFFFF, 0xFFFD, ( int )code );
}

/* Fill a screen area with one color and one character */
void x_screenFill( int top, int left, int bottom, int right, int color, int character )
{
   int row, col;

   for( row = top; row <= bottom; ++row )
   {
      for( col = left; col <= right; ++col )
         x_gtPutCell( row, col, color, 0, ( uint16_t )character );
   }

   x_gtFlush();
}

/* The shadow of a box: two columns on the right and one row below, moved by one row and two columns;
   the cells keep their characters and take the color */
void x_screenShadow( int top, int left, int bottom, int right, int color )
{
   int row, col, cellColor;
   uint8_t attr;
   uint16_t character;

   for( row = top + 1; row <= bottom + 1; ++row )
   {
      for( col = IIF( row > bottom, left + 2, right + 1 ); col <= right + 2; ++col )
      {
         if( x_gtGetCell( row, col, &cellColor, &attr, &character ) )
            x_gtPutCell( row, col, color, attr, character );
      }
   }

   x_gtFlush();
}

/* The cells of an area with oldColor, or all of them when oldColor is -1, take newColor */
static void s_screenRecolor( int top, int left, int bottom, int right, int newColor, int oldColor )
{
   int row, col, color;
   uint8_t attr;
   uint16_t character;

   for( row = top; row <= bottom; ++row )
   {
      for( col = left; col <= right; ++col )
      {
         if( x_gtGetCell( row, col, &color, &attr, &character ) && ( oldColor < 0 || color == oldColor ) )
            x_gtPutCell( row, col, newColor, attr, character );
      }
   }

   x_gtFlush();
}

/* The next UTF-8 character of a string for a screen cell; U+FFFD for an invalid byte or above U+FFFF */
static uint16_t s_screenCellChar( const char *text, size_t length, size_t *offset )
{
   uint32_t code;

   if( !x_utf8Decode( text, length, offset, &code ) || code > 0xFFFF )
      return 0xFFFD;

   return ( uint16_t )code;
}

/* A code of x_ucodes() for a screen cell; an invalid byte or a character above U+FFFF becomes U+FFFD */
static uint16_t s_screenCellCode( uint32_t code )
{
   return ( uint16_t )IIF( code > 0xFFFF, 0xFFFD, code );
}

/* The standard color of SetColor() */
static int s_screenStdColor( void )
{
   unsigned char colors[X_CLRSTR_LEN];

   x_gtGetColorStr( colors );

   return x_gtColorToN( ( const char * )colors );
}

/* Do the screen characters match the searched codes, with or without the case of letters? */
static int s_screenMatch( const uint16_t *cells, const uint32_t *search, size_t count, int exact )
{
   size_t i;

   for( i = 0; i < count; ++i )
   {
      if( exact && cells[i] != search[i] )
         return 0;

      if( !exact && x_utf8UpperChar( cells[i] ) != x_utf8UpperChar( search[i] ) )
         return 0;
   }

   return 1;
}

/* Is a screen character one of the codes? */
static int s_screenInSet( uint16_t character, const uint32_t *codes, size_t count )
{
   size_t i;

   for( i = 0; i < count; ++i )
   {
      if( codes[i] == character )
         return 1;
   }

   return 0;
}

/* A key code or the characters of a string put at the end of the keyboard buffer */
static void s_screenKeyPutValue( const X_ITEM *value )
{
   uint32_t *codes;
   size_t count, i;

   if( X_IS_NUMERIC( value ) )
      x_inkeyPut( x_itemGetNI( value ) );

   if( !X_IS_STRING( value ) )
      return;

   codes = x_ucodes( value->u.string.data, value->u.string.len, &count );
   for( i = 0; i < count; ++i )
   {
      if( codes[i] >= X_UCODE_BYTE )
         codes[i] = 0xFFFD;

      x_inkeyPut( IIF( codes[i] < 128, ( int )codes[i], ( int )X_INKEY_NEW_UNICODE( codes[i] ) ) );
   }

   x_xfree( codes );
}

/* The start of an output: arguments rowArg and rowArg + 1, else the cursor; 0 when outside the screen */
static int s_screenStart( X_CALL_CTX *ctx, int rowArg, int *row, int *col )
{
   unsigned short cursorRow = 0, cursorCol = 0;

   x_gtGetPos( &cursorRow, &cursorCol );
   *row = ( int )x_parnintdef( ctx, rowArg, cursorRow );
   *col = ( int )x_parnintdef( ctx, rowArg + 1, cursorCol );

   return *row >= 0 && *col >= 0 && *row <= x_gtMaxRow() && *col <= x_gtMaxCol();
}

/* The index of a key in the SetKey() list; s_setKeyCount when the key has no action */
static size_t s_setKeyFind( int key )
{
   size_t index;

   for( index = 0; index < s_setKeyCount && s_setKeys[index].key != key; ++index )
      ;

   return index;
}

/* A key gets an action and a condition; an action that is not a block releases the key */
static void s_setKeyPut( int key, const X_ITEM *action, const X_ITEM *condition )
{
   X_ITEM nil = X_ITEM_NIL_INIT;
   size_t index = s_setKeyFind( key );

   if( action == NULL || !X_IS_BLOCK( action ) )
   {
      if( index < s_setKeyCount )
      {
         x_itemClear( &s_setKeys[index].action );
         x_itemClear( &s_setKeys[index].condition );
         s_setKeys[index] = s_setKeys[--s_setKeyCount];
      }

      return;
   }

   if( index == s_setKeyCount )
   {
      s_setKeys = ( S_SETKEY * )x_xrealloc( s_setKeys, ( s_setKeyCount + 1 ) * sizeof( S_SETKEY ) );
      s_setKeys[index].key = key;
      s_setKeys[index].action = nil;
      s_setKeys[index].condition = nil;
      ++s_setKeyCount;
   }

   x_itemCopy( &s_setKeys[index].action, action );
   if( condition != NULL && X_IS_BLOCK( condition ) )
      x_itemCopy( &s_setKeys[index].condition, condition );
   else
      x_itemClear( &s_setKeys[index].condition );
}

/* All keys released */
static void s_setKeyRelease( void )
{
   while( s_setKeyCount > 0 )
   {
      --s_setKeyCount;
      x_itemClear( &s_setKeys[s_setKeyCount].action );
      x_itemClear( &s_setKeys[s_setKeyCount].condition );
   }
}

/* The SetKey() list as an array of { nKey, bAction, bCondition | NIL } */
static void s_setKeyList( X_ITEM *result )
{
   size_t i;

   x_arrayNew( result, ( int64_t )s_setKeyCount );
   for( i = 0; i < s_setKeyCount; ++i )
   {
      X_ITEM entry __attribute__( ( cleanup( x_itemClear ) ) ) = X_ITEM_NIL_INIT;

      x_arrayNew( &entry, 3 );
      x_arraySetNInt( &entry, 1, s_setKeys[i].key );
      x_arraySet( &entry, 2, &s_setKeys[i].action );
      x_arraySet( &entry, 3, &s_setKeys[i].condition );
      x_arraySet( result, i + 1, &entry );
   }
}

/* The SetKey() list replaced by an array of s_setKeyList(); 0 and no change when the array has another form */
static int s_setKeyRestore( const X_ITEM *list )
{
   int64_t count = x_arrayLen( list );
   int64_t i;
   const X_ITEM *entry;
   const X_ITEM *condition;
   int valid;

   for( i = 1; i <= count; ++i )
   {
      entry = x_arrayGetItemPtr( list, ( size_t )i );
      valid = X_IS_ARRAY( entry ) && x_arrayLen( entry ) >= 2;
      if( !valid || !X_IS_NUMERIC( x_arrayGetItemPtr( entry, 1 ) ) || !X_IS_BLOCK( x_arrayGetItemPtr( entry, 2 ) ) )
         return 0;
   }

   s_setKeyRelease();
   for( i = 1; i <= count; ++i )
   {
      entry = x_arrayGetItemPtr( list, ( size_t )i );
      condition = IIF( x_arrayLen( entry ) >= 3, x_arrayGetItemPtr( entry, 3 ), NULL );
      s_setKeyPut( x_itemGetNI( x_arrayGetItemPtr( entry, 1 ) ), x_arrayGetItemPtr( entry, 2 ), condition );
   }

   return 1;
}

/* ---------------------------------------------------------------------------------------------
   DispOut()
   --------------------------------------------------------------------------------------------- */

/* DispOut(): the value formatted with the picture, in the color of argument colorArg when given */
void x_dispOut( X_CALL_CTX *ctx, int valueArg, const char *pic, size_t picLen, int colorArg )
{
   X_ITEM text __attribute__( ( cleanup( x_itemClear ) ) ) = X_ITEM_NIL_INIT;
   unsigned char colors[X_CLRSTR_LEN];
   int colored = X_ISCHAR( ctx, colorArg );
   size_t len;

   x_transform( &text, x_paritem( ctx, valueArg ), pic, picLen );

   if( colored )
   {
      x_gtGetColorStr( colors );
      x_gtSetColorStr( ( unsigned char * )x_parc( ctx, colorArg ) );
   }

   len = IIF( text.u.string.len > USHRT_MAX, USHRT_MAX, text.u.string.len );
   x_gtWrite( ( unsigned char * )text.u.string.data, ( unsigned short )len );

   if( colored )
      x_gtSetColorStr( colors );

   x_ret( ctx );
}

/* 0 with error 1901 when the screen does not show 24-bit colors */
int x_trueColorCheck( X_CALL_CTX *ctx, const char *name )
{
   if( !x_gtActive() || !x_gtHasTrueColor() )
   {
      x_errorRaiseNamed( ctx->result, 1901, name );
      return 0;
   }

   return 1;
}

/* The 24-bit color 0xRRGGBB of an argument; 0 with error 1102 when it is not one */
int x_rgb( X_CALL_CTX *ctx, int argument, const char *name, uint32_t *rgb )
{
   if( !X_ISNUM( ctx, argument ) || x_parnd( ctx, argument ) < 0 || x_parnd( ctx, argument ) > 0xFFFFFF )
   {
      x_errorRaiseNamed( ctx->result, 1102, name );
      return 0;
   }

   *rgb = ( uint32_t )x_parnint( ctx, argument );

   return 1;
}

/* DispOutExt(): the first argument formatted with the picture, in 24-bit foreground and background colors */
void x_dispOutExt( X_CALL_CTX *ctx, const char *pic, size_t picLen, int foregroundArg, int backgroundArg,
                   const char *name )
{
   X_ITEM text __attribute__( ( cleanup( x_itemClear ) ) ) = X_ITEM_NIL_INIT;
   uint32_t foreground, background;
   size_t len;

   if( !x_rgb( ctx, foregroundArg, name, &foreground ) || !x_rgb( ctx, backgroundArg, name, &background ) )
      return;

   x_transform( &text, x_paritem( ctx, 1 ), pic, picLen );
   len = IIF( text.u.string.len > USHRT_MAX, USHRT_MAX, text.u.string.len );
   x_gtWriteExt( ( unsigned char * )text.u.string.data, ( unsigned short )len, foreground, background );

   x_ret( ctx );
}

/* ---------------------------------------------------------------------------------------------
   Color strings
   --------------------------------------------------------------------------------------------- */

/* Find the nth color value of a comma separated color string; 0 when there is none */
int x_colorPair( const char *text, size_t length, int64_t n, size_t *start, size_t *pairLength )
{
   size_t offset = 0;
   int64_t pair = 1;

   if( n < 1 )
      return 0;

   while( pair < n )
   {
      const char *comma = ( const char * )memchr( text + offset, ',', length - offset );

      if( comma == NULL )
         return 0;

      offset = ( size_t )( comma - text ) + 1;
      ++pair;
   }

   *start = offset;
   *pairLength = 0;
   while( offset + *pairLength < length && text[offset + *pairLength] != ',' )
      ++*pairLength;

   return 1;
}

/* The foreground and background colors exchanged; high intensity and blinking stay where they are */
int x_colorInvert( int color )
{
   return ( ( color & 0x07 ) << 4 ) | ( ( color >> 4 ) & 0x07 ) | ( color & 0x88 );
}

/* ---------------------------------------------------------------------------------------------
   Screen functions of the API
   --------------------------------------------------------------------------------------------- */

/* Accept(): read a line of keyboard input, target of the ACCEPT command */
void x_screenAccept( X_CALL_CTX *ctx )
{
   char result[256];
   char text[4];
   size_t length = 0;
   size_t count;
   int key = 0;

   if( x_pcount( ctx ) >= 1 )
      x_consoleOut( ctx, 1, 0 );

   while( x_inkeyKeyStd( key ) != K_ENTER )
   {
      key = x_inkey( 1, 0.0, INKEY_KEYBOARD | INKEY_EXT );
      if( x_inkeyKeyStd( key ) == K_BS || x_inkeyKeyStd( key ) == K_LEFT )
      {
         if( length > 0 )
         {
            do
               --length;
            while( length > 0 && ( ( unsigned char )result[length] & 0xC0 ) == 0x80 );
            x_gtWriteCon( "\b", 1 );
         }
      }
      else if( ( count = x_inkeyKeyText( key, text ) ) > 0 && length + count < sizeof( result ) )
      {
         memcpy( result + length, text, count );
         length += count;
         x_gtWriteCon( text, count );
      }
   }

   x_retclen( ctx, result, length );
}

/* AChoice(): execute a pop-up menu */
void x_screenAChoice( X_CALL_CTX *ctx )
{
   const X_ITEM *items = x_paritem( ctx, 5 );
   const X_ITEM *selectable = IIF( X_ISNIL( ctx, 6 ), NULL, x_paritem( ctx, 6 ) );
   const X_ITEM *userBlock = IIF( X_ISBLOCK( ctx, 7 ), x_paritem( ctx, 7 ), NULL );
   unsigned short rect[4] = { 0, 0, 0, 0 };
   unsigned short max[2] = { x_gtMaxRow(), x_gtMaxCol() };
   int64_t count = 0;
   int64_t height, first, current, next, maxFirst, last;
   int i, key, action;

   for( i = 0; i < 4; ++i )
   {
      int64_t value = x_parnint( ctx, i + 1 );

      if( value < 0 )
         value = 0;
      else if( value > max[i % 2] )
         value = max[i % 2];

      rect[i] = ( unsigned short )value;
   }

   if( rect[0] > rect[2] || rect[1] > rect[3] )
   {
      x_retni( ctx, 0 );
      return;
   }

   height = rect[2] - rect[0] + 1;

   /* The menu ends at the first element that is not a string */
   while( count < x_arrayLen( items ) && X_IS_STRING( x_arrayGetItemPtr( items, ( size_t )count + 1 ) ) )
      ++count;

   current = x_parnintdef( ctx, 8, 1 );
   if( current < 1 || current > count || !x_achoiceSelectable( ctx, selectable, current ) )
      current = x_achoiceFind( ctx, selectable, count, 1, 1 );

   maxFirst = IIF( count - height + 1 > 1, count - height + 1, 1 );
   first = IIF( X_ISNUM( ctx, 9 ), current - x_parnint( ctx, 9 ), current );
   if( first < 1 )
      first = 1;
   else if( first > maxFirst )
      first = maxFirst;

   if( current == 0 )
   {
      x_achoiceDraw( ctx, items, selectable, count, first, 0, rect );
      if( userBlock != NULL )
         x_achoiceUser( ctx, userBlock, 4, 0, first );

      x_retni( ctx, 0 );
      return;
   }

   for( ;; )
   {
      /* Keep the current item inside the window */
      if( current < first )
         first = current;
      else if( current > first + height - 1 )
         first = current - height + 1;

      x_achoiceDraw( ctx, items, selectable, count, first, current, rect );

      action = 2;
      if( userBlock != NULL && x_inkeyNext( INKEY_KEYBOARD ) == 0 )
         action = x_achoiceUser( ctx, userBlock, 0, current, first );

      if( action == 2 )
      {
         int mode = -1;

         key = x_inkey( 1, 0, INKEY_KEYBOARD );
         switch( key )
         {
         case K_UP:
            next = x_achoiceFind( ctx, selectable, count, current - 1, -1 );
            if( next )
               current = next;
            else
               mode = 1;
            break;

         case K_DOWN:
            next = x_achoiceFind( ctx, selectable, count, current + 1, 1 );
            if( next )
               current = next;
            else
               mode = 2;
            break;

         case K_CTRL_HOME:
            next = x_achoiceFind( ctx, selectable, count, first, 1 );
            current = IIF( next && next < first + height, next, current );
            break;

         case K_CTRL_END:
            last = IIF( first + height - 1 < count, first + height - 1, count );
            next = x_achoiceFind( ctx, selectable, count, last, -1 );
            current = IIF( next >= first, next, current );
            break;

         case K_PGUP:
            next = x_achoiceFind( ctx, selectable, count, IIF( current - height > 1, current - height, 1 ), 1 );
            if( next && next < current )
            {
               first = IIF( first - height > 1, first - height, 1 );
               current = next;
            }
            else
               mode = 1;
            break;

         case K_PGDN:
            last = IIF( current + height < count, current + height, count );
            next = x_achoiceFind( ctx, selectable, count, last, -1 );
            if( next && next > current )
            {
               first = IIF( first + height < maxFirst, first + height, maxFirst );
               current = next;
            }
            else
               mode = 2;
            break;

         case K_CTRL_PGUP:
            current = x_achoiceFind( ctx, selectable, count, 1, 1 );
            break;

         case K_CTRL_PGDN:
            current = x_achoiceFind( ctx, selectable, count, count, -1 );
            break;

         default:
            if( userBlock != NULL )
               mode = 3;
            else if( key == K_HOME )
               current = x_achoiceFind( ctx, selectable, count, 1, 1 );
            else if( key == K_END )
               current = x_achoiceFind( ctx, selectable, count, count, -1 );
            else if( key == K_ENTER )
            {
               x_retnint( ctx, current );
               return;
            }
            else if( key == K_ESC || key == K_LEFT || key == K_RIGHT )
            {
               x_retni( ctx, 0 );
               return;
            }
            else
               action = 3;
            break;
         }

         if( mode >= 0 && userBlock != NULL )
            action = x_achoiceUser( ctx, userBlock, mode, current, first );
      }

      if( action == 0 )
      {
         x_retni( ctx, 0 );
         return;
      }

      if( action == 1 )
      {
         x_retnint( ctx, current );
         return;
      }

      if( action == 3 )
      {
         /* The next available item that starts with the last key, wrapping around */
         int lastKey = x_inkeyLast( INKEY_KEYBOARD );

         for( next = 1; next <= count; ++next )
         {
            int64_t index = ( current + next - 1 ) % count + 1;
            const X_ITEM *item = x_arrayGetItemPtr( items, ( size_t )index );

            if( !x_inkeyStartsWith( lastKey, item->u.string.data, item->u.string.len ) )
               continue;

            if( x_achoiceSelectable( ctx, selectable, index ) )
            {
               current = index;
               break;
            }
         }
      }
   }
}

/* Alert(): display a simple modal dialog box; the result is the number of the chosen option */
void x_screenAlert( X_CALL_CTX *ctx )
{
   X_ITEM *options;
   int normal;
   int high;
   double delay;

   if( X_ISNUM( ctx, 3 ) )
   {
      normal = x_parni( ctx, 3 );
      high = x_parnidef( ctx, 4, 0 );
      delay = x_parnd( ctx, 5 );
   }
   else
   {
      normal = IIF( X_ISCHAR( ctx, 3 ), x_gtColorToN( x_parc( ctx, 3 ) ), -1 );
      high = ( ( normal & 0x07 ) << 4 ) | ( ( normal >> 4 ) & 0x07 );
      delay = x_parnd( ctx, 4 );

      if( normal < 0 )
      {
         normal = 0x4F;
         high = 0x1F;
      }
   }

   options = IIF( X_ISARRAY( ctx, 2 ), ( X_ITEM * )x_paritem( ctx, 2 ), NULL );
   x_retni( ctx, x_gtAlert( ( X_ITEM * )x_paritem( ctx, 1 ), options, normal, high, delay ) );
}

/* CharWin(): replace characters in a screen area */
void x_screenCharWin( X_CALL_CTX *ctx )
{
   unsigned short rect[4];
   int newChar = x_cellChar( ctx, 5, x_clearChar );
   int oldChar = IIF( X_ISNIL( ctx, 6 ), -1, x_cellChar( ctx, 6, ' ' ) );
   int row, col, color;
   uint8_t attr;
   uint16_t character;

   x_screenRange( ctx, rect, 1, 1 );
   for( row = rect[0]; row <= rect[2]; ++row )
   {
      for( col = rect[1]; col <= rect[3]; ++col )
      {
         if( x_gtGetCell( row, col, &color, &attr, &character ) && ( oldChar < 0 || character == oldChar ) )
            x_gtPutCell( row, col, color, attr, ( uint16_t )newChar );
      }
   }

   x_gtFlush();
   x_retc( ctx, "" );
}

/* ClearEol(): clear a row on the screen from a specified position */
void x_screenClearEol( X_CALL_CTX *ctx )
{
   unsigned short row, col;
   int top, left;

   x_gtGetPos( &row, &col );
   top = ( int )x_parnintdef( ctx, 1, row );
   left = ( int )x_parnintdef( ctx, 2, col );
   x_screenFill( top, left, top, x_gtMaxCol(), x_colorArg( ctx, 3, x_clearColor ), x_cellChar( ctx, 4, x_clearChar ) );
   x_retc( ctx, "" );
}

/* ClearSlow(): an area cleared frame by frame to the middle with SetClearA() and SetClearB() */
void x_screenClearSlow( X_CALL_CTX *ctx )
{
   double delay = x_parnd( ctx, 1 ) / 1000.0;
   int top = ( int )x_parnintdef( ctx, 2, 0 );
   int left = ( int )x_parnintdef( ctx, 3, 0 );
   int bottom = ( int )x_parnintdef( ctx, 4, x_gtMaxRow() );
   int right = ( int )x_parnintdef( ctx, 5, x_gtMaxCol() );
   int character = x_cellChar( ctx, 6, x_clearChar );
   int rowSteps, colSteps, steps, step, rowIn, colIn, row, col;

   top = IIF( top < 0, 0, top );
   left = IIF( left < 0, 0, left );
   bottom = IIF( bottom > x_gtMaxRow(), x_gtMaxRow(), bottom );
   right = IIF( right > x_gtMaxCol(), x_gtMaxCol(), right );
   rowSteps = ( bottom - top + 2 ) / 2;
   colSteps = ( right - left + 2 ) / 2;
   steps = IIF( rowSteps > colSteps, rowSteps, colSteps );

   for( step = 0; top <= bottom && left <= right && step < steps; ++step )
   {
      rowIn = step * rowSteps / steps;
      colIn = step * colSteps / steps;
      for( col = left + colIn; col <= right - colIn; ++col )
      {
         x_gtPutCell( top + rowIn, col, x_clearColor, 0, ( uint16_t )character );
         x_gtPutCell( bottom - rowIn, col, x_clearColor, 0, ( uint16_t )character );
      }

      for( row = top + rowIn; row <= bottom - rowIn; ++row )
      {
         x_gtPutCell( row, left + colIn, x_clearColor, 0, ( uint16_t )character );
         x_gtPutCell( row, right - colIn, x_clearColor, 0, ( uint16_t )character );
      }

      x_gtFlush();
      x_sysSleep( delay );
   }

   x_retc( ctx, "" );
}

/* ClearWin(): clear all or part of the screen */
void x_screenClearWin( X_CALL_CTX *ctx )
{
   unsigned short rect[4];
   int color;

   x_screenRange( ctx, rect, 1, 1 );
   color = x_colorArg( ctx, 5, x_clearColor );
   x_screenFill( rect[0], rect[1], rect[2], rect[3], color, x_cellChar( ctx, 6, x_clearChar ) );
   x_retc( ctx, "" );
}

/* ClrArea(): an area of the screen takes one color; the characters stay */
void x_screenClrArea( X_CALL_CTX *ctx )
{
   unsigned short rect[4];

   x_screenRange( ctx, rect, 1, 0 );
   s_screenRecolor( rect[0], rect[1], rect[2], rect[3], x_colorArg( ctx, 5, x_clearColor ), -1 );
   x_ret( ctx );
}

/* Cls(): clear the screen and move the cursor to the top left corner */
void x_screenCls( X_CALL_CTX *ctx )
{
   x_gtScrollEx( 0, 0, x_gtMaxRow(), x_gtMaxCol(), 0, 0, NULL, NULL );
   x_gtSetPos( 0, 0 );
   x_ret( ctx );
}

/* ColorIndex(): the color value at a position from 0 of a color string, without the spaces around it */
void x_screenColorIndex( X_CALL_CTX *ctx )
{
   const char *text = x_parc( ctx, 1 );
   int64_t index = x_parnint( ctx, 2 );
   size_t start, length;

   if( index < 0 || !x_colorPair( text, x_parclen( ctx, 1 ), index + 1, &start, &length ) )
   {
      x_retc( ctx, "" );
      return;
   }

   while( length > 0 && text[start] == ' ' )
   {
      ++start;
      --length;
   }

   while( length > 0 && text[start + length - 1] == ' ' )
      --length;

   x_retclen( ctx, text + start, length );
}

/* ColorRepl(): the cells of the whole screen with xOldAttr, or all of them, take xNewAttr */
void x_screenColorRepl( X_CALL_CTX *ctx )
{
   s_screenRecolor( 0, 0, x_gtMaxRow(), x_gtMaxCol(), x_colorArg( ctx, 1, x_clearColor ), x_colorArg( ctx, 2, -1 ) );
   x_retc( ctx, "" );
}

/* ColorWin(): replace a color attribute in a screen area */
void x_screenColorWin( X_CALL_CTX *ctx )
{
   unsigned short rect[4];

   x_screenRange( ctx, rect, 1, 1 );
   s_screenRecolor( rect[0], rect[1], rect[2], rect[3], x_colorArg( ctx, 5, x_clearColor ), x_colorArg( ctx, 6, -1 ) );
   x_retc( ctx, "" );
}

/* CSetCurs(): .T. when the cursor is visible; lNewMode shows or hides it */
void x_screenCSetCurs( X_CALL_CTX *ctx )
{
   unsigned short shape = SC_NONE;

   x_gtGetCursor( &shape );
   if( X_ISLOG( ctx, 1 ) )
      x_gtSetCursor( IIF( x_parl( ctx, 1 ), SC_NORMAL, SC_NONE ) );

   x_retl( ctx, shape != SC_NONE );
}

/* DispBox(): display a box on the screen */
void x_screenDispBox( X_CALL_CTX *ctx )
{
   unsigned short top = ( unsigned short )x_parni( ctx, 1 );
   unsigned short left = ( unsigned short )x_parni( ctx, 2 );
   unsigned short bottom = ( unsigned short )x_parni( ctx, 3 );
   unsigned short right = ( unsigned short )x_parni( ctx, 4 );
   unsigned char colors[X_CLRSTR_LEN];
   int colored = X_ISCHAR( ctx, 6 );

   if( colored )
   {
      x_gtGetColorStr( colors );
      x_gtSetColorStr( ( unsigned char * )x_parc( ctx, 6 ) );
   }

   if( X_ISCHAR( ctx, 5 ) )
      x_gtBox( top, left, bottom, right, ( unsigned char * )x_parc( ctx, 5 ) );
   else if( x_parnidef( ctx, 5, 1 ) == 2 )
      x_gtBoxD( top, left, bottom, right );
   else
      x_gtBoxS( top, left, bottom, right );

   if( colored )
      x_gtSetColorStr( colors );

   x_ret( ctx );
}

/* DispBoxExt(): display a box on the screen in 24-bit foreground and background colors */
void x_screenDispBoxExt( X_CALL_CTX *ctx, uint32_t foreground, uint32_t background )
{
   unsigned short top, left, bottom, right;

   top = ( unsigned short )x_parni( ctx, 1 );
   left = ( unsigned short )x_parni( ctx, 2 );
   bottom = ( unsigned short )x_parni( ctx, 3 );
   right = ( unsigned short )x_parni( ctx, 4 );
   if( X_ISCHAR( ctx, 5 ) )
      x_gtBoxExt( top, left, bottom, right, ( unsigned char * )x_parc( ctx, 5 ), foreground, background );
   else if( x_parnidef( ctx, 5, 1 ) == 2 )
      x_gtBoxDExt( top, left, bottom, right, foreground, background );
   else
      x_gtBoxSExt( top, left, bottom, right, foreground, background );

   x_ret( ctx );
}

/* DispOutAt(): display a value on the screen at a certain position */
void x_screenDispOutAt( X_CALL_CTX *ctx )
{
   X_ITEM text __attribute__( ( cleanup( x_itemClear ) ) ) = X_ITEM_NIL_INIT;
   unsigned char colors[X_CLRSTR_LEN];
   unsigned short savedRow, savedCol;
   unsigned short row, col, length;
   int colored;

   colored = X_ISCHAR( ctx, 4 );
   if( colored )
   {
      x_gtGetColorStr( colors );
      x_gtSetColorStr( ( unsigned char * )x_parc( ctx, 4 ) );
   }

   x_transform( &text, x_paritem( ctx, 3 ), "", 0 );
   x_gtGetPos( &savedRow, &savedCol );
   row = ( unsigned short )x_parni( ctx, 1 );
   col = ( unsigned short )x_parni( ctx, 2 );
   length = ( unsigned short )IIF( text.u.string.len > USHRT_MAX, USHRT_MAX, text.u.string.len );
   x_gtWriteAt( row, col, ( unsigned char * )text.u.string.data, length );
   if( !x_parldef( ctx, 5, x_dispOutAtSetPos ) )
      x_gtSetPos( savedRow, savedCol );

   if( colored )
      x_gtSetColorStr( colors );

   x_ret( ctx );
}

/* DispOutAtExt(): display a value at a position in 24-bit foreground and background colors */
void x_screenDispOutAtExt( X_CALL_CTX *ctx, uint32_t foreground, uint32_t background )
{
   X_ITEM text __attribute__( ( cleanup( x_itemClear ) ) ) = X_ITEM_NIL_INIT;
   unsigned short row, col, length;
   int setPos;

   x_transform( &text, x_paritem( ctx, 3 ), "", 0 );
   row = ( unsigned short )x_parni( ctx, 1 );
   col = ( unsigned short )x_parni( ctx, 2 );
   length = ( unsigned short )IIF( text.u.string.len > USHRT_MAX, USHRT_MAX, text.u.string.len );
   setPos = x_parldef( ctx, 6, x_dispOutAtSetPos );
   x_gtWriteAtExt( row, col, ( unsigned char * )text.u.string.data, length, foreground, background, setPos );
   x_ret( ctx );
}

/* InvertWin(): exchange the foreground and background color on the screen */
void x_screenInvertWin( X_CALL_CTX *ctx )
{
   unsigned short rect[4];
   int row, col, color;
   uint8_t attr;
   uint16_t character;

   x_screenRange( ctx, rect, 1, 1 );
   for( row = rect[0]; row <= rect[2]; ++row )
   {
      for( col = rect[1]; col <= rect[3]; ++col )
      {
         if( x_gtGetCell( row, col, &color, &attr, &character ) )
            x_gtPutCell( row, col, x_colorInvert( color ), attr, character );
      }
   }

   x_gtFlush();
   x_retc( ctx, "" );
}

/* KbdStat(): the shift keys and the lock keys as bits; 0 when the GT does not report them */
void x_screenKbdStat( X_CALL_CTX *ctx )
{
   X_ITEM shifts __attribute__( ( cleanup( x_itemClear ) ) ) = X_ITEM_NIL_INIT;
   int state, status = 0;
   size_t i;

   x_gtInfo( X_GTI_KBDSHIFTS, NULL, NULL, &shifts );
   state = x_itemGetNI( &shifts );

   for( i = 0; i < sizeof( s_kbdStatBits ) / sizeof( s_kbdStatBits[0] ); ++i )
   {
      if( state & s_kbdStatBits[i].shift )
         status |= s_kbdStatBits[i].bit;
   }

   x_retni( ctx, status );
}

/* KeyPut(): a key code, the characters of a string or the values of an array put at the end of the buffer */
void x_screenKeyPut( X_CALL_CTX *ctx )
{
   const X_ITEM *keys = x_paritem( ctx, 1 );
   int64_t i;

   if( X_IS_ARRAY( keys ) )
   {
      for( i = 1; i <= x_arrayLen( keys ); ++i )
         s_screenKeyPutValue( x_arrayGetItemPtr( keys, ( size_t )i ) );
   }
   else
      s_screenKeyPutValue( keys );

   x_ret( ctx );
}

/* KSetCaps(), KSetIns(), KSetNum(), KSetScroll(): the old state of one lock key; lNewSwitch sets it */
void x_screenKSet( X_CALL_CTX *ctx, int flag )
{
   X_ITEM shifts __attribute__( ( cleanup( x_itemClear ) ) ) = X_ITEM_NIL_INIT;
   X_ITEM newShifts __attribute__( ( cleanup( x_itemClear ) ) ) = X_ITEM_NIL_INIT;
   int state;

   x_gtInfo( X_GTI_KBDSHIFTS, NULL, NULL, &shifts );
   state = x_itemGetNI( &shifts );

   if( X_ISLOG( ctx, 1 ) )
   {
      x_itemPutNI( &newShifts, IIF( x_parl( ctx, 1 ), state | flag, state & ~flag ) );
      x_gtInfo( X_GTI_KBDSHIFTS, &newShifts, NULL, &shifts );
   }

   x_retl( ctx, ( state & flag ) != 0 );
}

/* NToColor(): convert a numeric color attribute to a color string */
void x_screenNToColor( X_CALL_CTX *ctx )
{
   int64_t color = x_parnint( ctx, 1 );
   char text[16];

   if( color < 0 || color > 255 )
   {
      x_retc( ctx, "" );
      return;
   }

   if( X_ISLOG( ctx, 2 ) && x_parl( ctx, 2 ) )
      x_gtColorName( ( int )color, text, sizeof( text ) );
   else
      snprintf( text, sizeof( text ), "%02d/%02d", ( int )( color & 0x0F ), ( int )( ( color >> 4 ) & 0x0F ) );

   x_retc( ctx, text );
}

/* RestCursor(): the cursor row, column and shape from a number of SaveCursor() */
void x_screenRestCursor( X_CALL_CTX *ctx )
{
   int64_t saved = x_parnint( ctx, 1 );

   x_gtSetPos( ( unsigned short )( ( saved >> 16 ) & 0xFFFF ), ( unsigned short )( saved & 0xFFFF ) );
   x_gtSetCursor( ( unsigned short )( ( saved >> 32 ) & 0xFFFF ) );
   x_retc( ctx, "" );
}

/* RestScreen(): display a saved screen region to a specified location */
void x_screenRestScreen( X_CALL_CTX *ctx )
{
   unsigned short rect[4];

   x_screenRange( ctx, rect, 1, 0 );
   size_t size;

   x_gtRectSize( rect[0], rect[1], rect[2], rect[3], &size );
   X_ARGCHECK( ctx, x_parclen( ctx, 5 ) >= size, "RESTSCREEN" );
   x_gtRest( rect[0], rect[1], rect[2], rect[3], ( void * )x_parc( ctx, 5 ) );
   x_ret( ctx );
}

/* RestSetKey(): the SetKey() list from an array of SaveSetKey(); .F. and no change for another form */
void x_screenRestSetKey( X_CALL_CTX *ctx )
{
   x_retl( ctx, s_setKeyRestore( x_paritem( ctx, 1 ) ) );
}

/* SaveCursor(): the cursor row, column and shape in one number for RestCursor(), 16 bits each */
void x_screenSaveCursor( X_CALL_CTX *ctx )
{
   unsigned short row = 0, col = 0, shape = SC_NONE;

   x_gtGetPos( &row, &col );
   x_gtGetCursor( &shape );
   x_retnint( ctx, ( int64_t )col | ( ( int64_t )row << 16 ) | ( ( int64_t )shape << 32 ) );
}

/* SaveScreen(): save a screen region for later display */
void x_screenSaveScreen( X_CALL_CTX *ctx )
{
   unsigned short rect[4];

   x_screenRange( ctx, rect, 1, 0 );
   size_t size;

   x_gtRectSize( rect[0], rect[1], rect[2], rect[3], &size );
   char *buffer = ( char * )x_xgrab( size + 1 );

   x_gtSave( rect[0], rect[1], rect[2], rect[3], buffer );
   buffer[size] = '\0';
   x_retclen_buffer( ctx, buffer, size );
}

/* SayDown(): a string written downwards in the color of SetColor() with a delay; the cursor stays */
void x_screenSayDown( X_CALL_CTX *ctx )
{
   const char *text = x_parc( ctx, 1 );
   size_t length = x_parclen( ctx, 1 ), offset = 0;
   double delay = IIF( X_ISNUM( ctx, 2 ), x_parnd( ctx, 2 ), 4.0 ) / 1000.0;
   int color = s_screenStdColor();
   int row, col;

   if( s_screenStart( ctx, 3, &row, &col ) )
   {
      while( offset < length && row <= x_gtMaxRow() )
      {
         x_gtPutCell( row++, col, color, 0, s_screenCellChar( text, length, &offset ) );
         x_gtFlush();
         x_sysSleep( delay );
      }
   }

   x_retc( ctx, "" );
}

/* SayMoveIn(): a string moved in from the left or from the right with a delay; the cursor ends after it */
void x_screenSayMoveIn( X_CALL_CTX *ctx )
{
   size_t count, shown, i;
   uint32_t *codes = x_ucodes( x_parc( ctx, 1 ), x_parclen( ctx, 1 ), &count );
   double delay = IIF( X_ISNUM( ctx, 2 ), x_parnd( ctx, 2 ), 4.0 ) / 1000.0;
   int fromRight = x_parl( ctx, 5 );
   int color = s_screenStdColor();
   int row, col;

   if( s_screenStart( ctx, 3, &row, &col ) )
   {
      for( shown = 1; shown <= count; ++shown )
      {
         for( i = 0; i < shown; ++i )
         {
            if( fromRight )
               x_gtPutCell( row, col + ( int )( count - shown + i ), color, 0, s_screenCellCode( codes[i] ) );
            else
               x_gtPutCell( row, col + ( int )i, color, 0, s_screenCellCode( codes[count - shown + i] ) );
         }

         x_gtFlush();
         x_sysSleep( delay );
      }

      x_gtSetPos( ( unsigned short )row, ( unsigned short )( col + ( int )count ) );
   }

   x_xfree( codes );
   x_retc( ctx, "" );
}

/* SayScreen(): a string written with the screen colors kept, going on in the next row; the cursor stays */
void x_screenSayScreen( X_CALL_CTX *ctx )
{
   const char *text = x_parc( ctx, 1 );
   size_t length = x_parclen( ctx, 1 ), offset = 0;
   int row, col, start, color;
   uint8_t attr;
   uint16_t character;

   if( s_screenStart( ctx, 2, &row, &start ) )
   {
      col = start;
      while( offset < length && row <= x_gtMaxRow() )
      {
         if( x_gtGetCell( row, col, &color, &attr, &character ) )
            x_gtPutCell( row, col++, color, attr, s_screenCellChar( text, length, &offset ) );
         else
         {
            ++row;
            col = start;
         }
      }

      x_gtFlush();
   }

   x_retc( ctx, "" );
}

/* SaySpread(): a string spread from its middle to both sides with a delay; the cursor stays */
void x_screenSaySpread( X_CALL_CTX *ctx )
{
   size_t count, first, last, i;
   uint32_t *codes = x_ucodes( x_parc( ctx, 1 ), x_parclen( ctx, 1 ), &count );
   double delay = IIF( X_ISNUM( ctx, 2 ), x_parnd( ctx, 2 ), 4.0 ) / 1000.0;
   int color = s_screenStdColor();
   unsigned short cursorRow = 0, cursorCol = 0;
   int row, center, left;

   x_gtGetPos( &cursorRow, &cursorCol );
   row = ( int )x_parnintdef( ctx, 3, cursorRow );
   center = ( int )x_parnintdef( ctx, 4, x_gtMaxCol() / 2 );
   left = center - ( int )( count / 2 );

   if( count > 0 && row >= 0 && row <= x_gtMaxRow() && center >= 0 && center <= x_gtMaxCol() )
   {
      first = ( count - 1 ) / 2;
      last = count / 2;
      for( ;; )
      {
         for( i = first; i <= last; ++i )
            x_gtPutCell( row, left + ( int )i, color, 0, s_screenCellCode( codes[i] ) );

         x_gtFlush();
         x_sysSleep( delay );
         if( first == 0 )
            break;

         --first;
         ++last;
      }
   }

   x_xfree( codes );
   x_retc( ctx, "" );
}

/* ScreenMark(): the occurrences of a string on the screen, between cPre and cPost, take a color */
void x_screenScreenMark( X_CALL_CTX *ctx )
{
   size_t count, preCount = 0, postCount = 0, total, pos, i;
   uint32_t *search = x_ucodes( x_parc( ctx, 1 ), x_parclen( ctx, 1 ), &count );
   uint32_t *pre = NULL;
   uint32_t *post = NULL;
   uint16_t *cells;
   int color = x_colorArg( ctx, 2, x_clearColor );
   int exact = x_parl( ctx, 3 );
   int all = x_parl( ctx, 4 );
   int cols = x_gtMaxCol() + 1;
   int found = 0, cellColor, row, col;
   uint8_t attr;
   uint16_t character;

   if( X_ISCHAR( ctx, 5 ) )
      pre = x_ucodes( x_parc( ctx, 5 ), x_parclen( ctx, 5 ), &preCount );

   if( X_ISCHAR( ctx, 6 ) )
      post = x_ucodes( x_parc( ctx, 6 ), x_parclen( ctx, 6 ), &postCount );

   total = ( size_t )( x_gtMaxRow() + 1 ) * ( size_t )cols;
   cells = ( uint16_t * )x_xgrab( ( total + 1 ) * sizeof( uint16_t ) );
   for( pos = 0; pos < total; ++pos )
   {
      if( !x_gtGetCell( ( int )( pos / ( size_t )cols ), ( int )( pos % ( size_t )cols ), &cellColor, &attr, &cells[pos] ) )
         cells[pos] = 0;
   }

   for( pos = 0; count > 0 && pos + count <= total; ++pos )
   {
      if( !s_screenMatch( cells + pos, search, count, exact ) )
         continue;

      if( pre != NULL && pos > 0 && !s_screenInSet( cells[pos - 1], pre, preCount ) )
         continue;

      if( post != NULL && pos + count < total && !s_screenInSet( cells[pos + count], post, postCount ) )
         continue;

      for( i = pos; i < pos + count; ++i )
      {
         row = ( int )( i / ( size_t )cols );
         col = ( int )( i % ( size_t )cols );
         if( x_gtGetCell( row, col, &cellColor, &attr, &character ) )
            x_gtPutCell( row, col, color, attr, character );
      }

      found = 1;
      if( !all )
         break;

      pos += count - 1;
   }

   x_gtFlush();
   x_xfree( cells );
   x_xfree( search );
   x_xfree( pre );
   x_xfree( post );
   x_retl( ctx, found );
}

/* Scroll(): scroll a screen region up or down, right or left */
void x_screenScroll( X_CALL_CTX *ctx )
{
   unsigned short rect[4];
   int64_t rows = x_parnintdef( ctx, 5, 0 );
   int64_t cols = x_parnintdef( ctx, 6, 0 );
   int height = ( int )x_gtMaxRow() + 1;
   int width = ( int )x_gtMaxCol() + 1;
   const X_ITEM *color = x_paritem( ctx, 7 );
   const X_ITEM *fill = x_paritem( ctx, 8 );

   x_screenRange( ctx, rect, 0, 0 );

   /* The shift is limited to the size of the screen */
   if( rows > height )
      rows = height;
   else if( rows < -height )
      rows = -height;

   if( cols > width )
      cols = width;
   else if( cols < -width )
      cols = -width;

   x_gtScrollEx( rect[0], rect[1], rect[2], rect[3], ( int )rows, ( int )cols, color, fill );
   x_ret( ctx );
}

/* SetClrPair(): replace a color value in a color string */
void x_screenSetClrPair( X_CALL_CTX *ctx )
{
   const char *text = x_parc( ctx, 1 );
   size_t length = x_parclen( ctx, 1 );
   const char *color = x_parc( ctx, 3 );
   size_t colorSize = x_parclen( ctx, 3 );
   size_t start, pairLength, size;
   char *result;

   /* A color string without the nth color value gives "" */
   if( !x_colorPair( text, length, x_parnint( ctx, 2 ), &start, &pairLength ) )
   {
      x_retc( ctx, "" );
      return;
   }

   size = length - pairLength + colorSize;
   result = ( char * )x_xgrab( size + 1 );
   memcpy( result, text, start );
   memcpy( result + start, color, colorSize );
   memcpy( result + start + colorSize, text + start + pairLength, length - start - pairLength );
   result[size] = '\0';
   x_retclen_buffer( ctx, result, size );
}

/* SetColorExt(): the previous 24-bit colors as { nForeground, nBackground }; with two arguments the new colors */
void x_screenSetColorExt( X_CALL_CTX *ctx, uint32_t foreground, uint32_t background )
{
   uint32_t previousForeground, previousBackground;
   int hadColor;

   hadColor = x_gtGetColorRGB( &previousForeground, &previousBackground ) == 0;
   x_arrayNew( ctx->result, 2 );
   if( hadColor )
   {
      x_arraySetNInt( ctx->result, 1, previousForeground );
      x_arraySetNInt( ctx->result, 2, previousBackground );
   }

   if( x_pcount( ctx ) == 2 )
      x_gtSetColorRGB( foreground, background );
}

/* SetKey(): the action of a key; with a block the new action and condition, with NIL the key is released */
void x_screenSetKey( X_CALL_CTX *ctx )
{
   size_t index = s_setKeyFind( x_parni( ctx, 1 ) );

   if( index < s_setKeyCount )
      x_itemReturn( ctx, &s_setKeys[index].action );
   else
      x_ret( ctx );

   if( x_pcount( ctx ) >= 2 )
      s_setKeyPut( x_parni( ctx, 1 ), x_paritem( ctx, 2 ), x_paritem( ctx, 3 ) );
}

/* SetKeyArray(): one action and one condition for all keys of an array */
void x_screenSetKeyArray( X_CALL_CTX *ctx )
{
   const X_ITEM *keys = x_paritem( ctx, 1 );
   int64_t i;

   for( i = 1; i <= x_arrayLen( keys ); ++i )
      s_setKeyPut( x_itemGetNI( x_arrayGetItemPtr( keys, ( size_t )i ) ), x_paritem( ctx, 2 ), x_paritem( ctx, 3 ) );

   x_ret( ctx );
}

/* SetKeyCheck(): the action of a key run with the arguments and the key when its condition allows it */
void x_screenSetKeyCheck( X_CALL_CTX *ctx )
{
   X_ITEM action __attribute__( ( cleanup( x_itemClear ) ) ) = X_ITEM_NIL_INIT;
   X_ITEM condition __attribute__( ( cleanup( x_itemClear ) ) ) = X_ITEM_NIL_INIT;
   X_ITEM result __attribute__( ( cleanup( x_itemClear ) ) ) = X_ITEM_NIL_INIT;
   X_ITEM keyItem = X_ITEM_NIL_INIT;
   X_ARG args[4];
   int key = x_parni( ctx, 1 );
   size_t index = s_setKeyFind( key );
   int count = 0, i;

   if( index == s_setKeyCount && x_inkeyKeyStd( key ) != key )
   {
      key = x_inkeyKeyStd( key );
      index = s_setKeyFind( key );
   }

   if( key == 0 || index == s_setKeyCount )
   {
      x_retl( ctx, 0 );
      return;
   }

   /* the blocks are copied: an action may change the SetKey() list */
   x_itemCopy( &action, &s_setKeys[index].action );
   x_itemCopy( &condition, &s_setKeys[index].condition );
   x_itemPutNI( &keyItem, key );

   if( X_IS_BLOCK( &condition ) )
   {
      args[0] = ( X_ARG ){ &keyItem, 0 };
      x_evalBlock( ctx, &result, &condition, 1, args );
      if( !X_IS_LOGICAL( &result ) || !result.u.logical )
      {
         x_retl( ctx, 0 );
         return;
      }
   }

   for( i = 2; i <= x_pcount( ctx ) && count < 3; ++i )
      args[count++] = ( X_ARG ){ x_paritem( ctx, i ), 0 };

   args[count++] = ( X_ARG ){ &keyItem, 0 };
   x_itemClear( &result );
   x_evalBlock( ctx, &result, &action, count, args );
   x_retl( ctx, 1 );
}

/* SetKeyGet(): the action of a key; its condition goes into the second argument passed by reference */
void x_screenSetKeyGet( X_CALL_CTX *ctx )
{
   size_t index = s_setKeyFind( x_parni( ctx, 1 ) );

   if( index == s_setKeyCount )
   {
      x_ret( ctx );
      return;
   }

   x_itemReturn( ctx, &s_setKeys[index].action );
   x_storitem( ctx, &s_setKeys[index].condition, 2 );
}

/* SaveSetKey() and SetKeySave(): the SetKey() list; SetKeySave() also replaces it or releases all keys */
void x_screenSetKeySave( X_CALL_CTX *ctx, int replace )
{
   s_setKeyList( ctx->result );

   if( replace && X_ISARRAY( ctx, 1 ) )
      s_setKeyRestore( x_paritem( ctx, 1 ) );
   else if( replace && x_pcount( ctx ) >= 1 )
      s_setKeyRelease();
}

/* SetMode(): change display mode to a specified number of rows and columns */
void x_screenSetMode( X_CALL_CTX *ctx )
{
   unsigned short rows, cols;
   double value;

   x_gtScrDim( &rows, &cols );
   if( X_ISNUM( ctx, 1 ) )
   {
      value = x_parnd( ctx, 1 );
      if( !( value >= 1.0 && value <= USHRT_MAX ) )
      {
         x_retl( ctx, 0 );
         return;
      }

      rows = ( unsigned short )x_parnint( ctx, 1 );
   }

   if( X_ISNUM( ctx, 2 ) )
   {
      value = x_parnd( ctx, 2 );
      if( !( value >= 1.0 && value <= USHRT_MAX ) )
      {
         x_retl( ctx, 0 );
         return;
      }

      cols = ( unsigned short )x_parnint( ctx, 2 );
   }

   x_retl( ctx, x_gtSetMode( rows, cols ) == 0 );
}

/* SetPosBS(): the cursor one column to the right, without a check of the screen border */
void x_screenSetPosBS( X_CALL_CTX *ctx )
{
   unsigned short row = 0, col = 0;

   x_gtGetPos( &row, &col );
   x_gtSetPos( row, ( unsigned short )( col + 1 ) );
   x_ret( ctx );
}

/* Wait(): wait for a keypress, target of the WAIT command */
void x_screenWait( X_CALL_CTX *ctx )
{
   char text[4];
   size_t length;

   static const char prompt[] = "\r\nPress any key to continue...";

   if( x_pcount( ctx ) >= 1 && !X_ISNIL( ctx, 1 ) )
      x_consoleOut( ctx, 1, 0 );
   else
      x_gtWriteCon( prompt, sizeof( prompt ) - 1 );

   length = x_inkeyKeyText( x_inkey( 1, 0.0, INKEY_KEYBOARD | INKEY_EXT ), text );
   if( length > 0 )
      x_gtWriteCon( text, length );
   else
      text[length++] = '\0';

   x_retclen( ctx, text, length );
}
