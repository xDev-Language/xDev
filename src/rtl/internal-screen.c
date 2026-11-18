/*
 * Copyright (c) 2024-2026 xDev-Language
 *
 * Library: xDev Runtime
 * File: internal-screen.c
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "classes.h"
#include "internal-private.h"
#include "operators.h"
#include "platform.h"
#include "statements.h"
#include "terminal.h"

#include "../../include/xdev/fileio.ch"
#include "../../include/xdev/inkey.ch"
#include "../../include/xdev/set.ch"

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

/* Cls(): clear the screen and move the cursor to the top left corner */
void x_screenCls( X_CALL_CTX *ctx )
{
   x_gtScrollEx( 0, 0, x_gtMaxRow(), x_gtMaxCol(), 0, 0, NULL, NULL );
   x_gtSetPos( 0, 0 );
   x_ret( ctx );
}

/* ColorWin(): replace a color attribute in a screen area */
void x_screenColorWin( X_CALL_CTX *ctx )
{
   unsigned short rect[4];
   int newColor = x_colorArg( ctx, 5, x_clearColor );
   int oldColor = x_colorArg( ctx, 6, -1 );
   int row, col, color;
   uint8_t attr;
   uint16_t character;

   x_screenRange( ctx, rect, 1, 1 );
   for( row = rect[0]; row <= rect[2]; ++row )
   {
      for( col = rect[1]; col <= rect[3]; ++col )
      {
         if( x_gtGetCell( row, col, &color, &attr, &character ) && ( oldColor < 0 || color == oldColor ) )
            x_gtPutCell( row, col, newColor, attr, character );
      }
   }

   x_gtFlush();
   x_retc( ctx, "" );
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
   size_t index;

   for( index = 0; index < s_setKeyCount && s_setKeys[index].key != x_parni( ctx, 1 ); ++index )
      ;
   if( index < s_setKeyCount )
      x_itemReturn( ctx, &s_setKeys[index].action );
   else
      x_ret( ctx );

   if( x_pcount( ctx ) >= 2 )
   {
      if( X_ISBLOCK( ctx, 2 ) )
      {
         if( index == s_setKeyCount )
         {
            X_ITEM nil = X_ITEM_NIL_INIT;

            s_setKeys = ( S_SETKEY * )x_xrealloc( s_setKeys, ( s_setKeyCount + 1 ) * sizeof( S_SETKEY ) );
            s_setKeys[index].key = x_parni( ctx, 1 );
            s_setKeys[index].action = nil;
            s_setKeys[index].condition = nil;
            ++s_setKeyCount;
         }

         x_itemCopy( &s_setKeys[index].action, x_paritem( ctx, 2 ) );
         if( X_ISBLOCK( ctx, 3 ) )
            x_itemCopy( &s_setKeys[index].condition, x_paritem( ctx, 3 ) );
         else
            x_itemClear( &s_setKeys[index].condition );
      }
      else if( index < s_setKeyCount )
      {
         x_itemClear( &s_setKeys[index].action );
         x_itemClear( &s_setKeys[index].condition );
         s_setKeys[index] = s_setKeys[--s_setKeyCount];
      }
   }
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
