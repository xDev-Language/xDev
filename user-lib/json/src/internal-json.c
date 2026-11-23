/*
 * Copyright (c) 2024-2026 xDev-Language
 *
 * Library...: xDev JSON
 * File......: internal-json.c
 *
 */

#include "internal-json.h"

#include <errno.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char s_error[256];

/* Remember the error of the last call; position ( size_t )-1 is no position */
static void s_setError( const char *message, size_t position )
{
   if( position == ( size_t )-1 )
      snprintf( s_error, sizeof( s_error ), "%s", message );
   else
      snprintf( s_error, sizeof( s_error ), "%s at position %zu", message, position + 1 );
}

/* The error of the last json_Encode() or json_Decode(), "" when there was none */
const char *x_jsonError( void )
{
   return s_error;
}

/* ---------------------------------------------------------------------------------------------
   Encoding
   --------------------------------------------------------------------------------------------- */

/* A growing text, always ended with '\0'; pretty adds new lines and indents */
typedef struct
{
   char *data;
   size_t len;
   size_t cap;
   int pretty;
} X_JSON_BUF;

/* Append bytes; the buffer doubles when it is full */
static void s_putLen( X_JSON_BUF *buf, const char *text, size_t length )
{
   if( buf->len + length + 1 > buf->cap )
   {
      size_t cap = IIF( buf->cap == 0, 256, buf->cap );

      while( buf->len + length + 1 > cap )
         cap *= 2;

      buf->data = ( char * )x_xrealloc( buf->data, cap );
      buf->cap = cap;
   }

   memcpy( buf->data + buf->len, text, length );

   buf->len += length;
   buf->data[buf->len] = '\0';
}

/* Append a text ended with '\0' */
static void s_put( X_JSON_BUF *buf, const char *text )
{
   s_putLen( buf, text, strlen( text ) );
}

/* A new line and the indent of depth, only in the pretty form */
static void s_newLine( X_JSON_BUF *buf, int depth )
{
   int i;

   if( !buf->pretty )
      return;

   s_put( buf, "\n" );

   for( i = 0; i < depth; ++i )
      s_put( buf, "  " );
}

/* Control characters, the quote and the backslash are escaped; the UTF-8 bytes go as they are */
static void s_string( X_JSON_BUF *buf, const char *text, size_t length )
{
   size_t i;

   s_put( buf, "\"" );

   for( i = 0; i < length; ++i )
   {
      unsigned char c = ( unsigned char )text[i];
      char escape[8];

      switch( c )
      {
      case '"': s_put( buf, "\\\"" ); break;
      case '\\': s_put( buf, "\\\\" ); break;
      case '\b': s_put( buf, "\\b" ); break;
      case '\f': s_put( buf, "\\f" ); break;
      case '\n': s_put( buf, "\\n" ); break;
      case '\r': s_put( buf, "\\r" ); break;
      case '\t': s_put( buf, "\\t" ); break;
      default:
         if( c < 0x20 )
         {
            snprintf( escape, sizeof( escape ), "\\u%04x", c );
            s_put( buf, escape );
         }
         else
            s_putLen( buf, text + i, 1 );
      }
   }

   s_put( buf, "\"" );
}

/* The shortest text that gives the same number back */
static void s_double( X_JSON_BUF *buf, double value )
{
   char text[40];
   int precision;

   if( !isfinite( value ) )
   {
      s_put( buf, "null" );
      return;
   }

   for( precision = 1; precision <= 17; ++precision )
   {
      snprintf( text, sizeof( text ), "%.*g", precision, value );

      if( strtod( text, NULL ) == value )
         break;
   }

   s_put( buf, text );
}

/* A date as YYYY-MM-DD, "" for the empty date */
static void s_date( char *text, size_t size, const X_ITEM *value )
{
   int year, month, day;

   if( value->u.integer == 0 )
   {
      text[0] = '\0';
      return;
   }

   x_dateDecode( ( long )value->u.integer, &year, &month, &day );
   snprintf( text, size, "%04d-%02d-%02d", year, month, day );
}

static int s_encode( X_JSON_BUF *buf, const X_ITEM *value, int depth );

/* A key of a hash becomes a text: a number as written, a date as YYYY-MM-DD, a logical value as true/false */
static void s_key( X_JSON_BUF *buf, const X_ITEM *key )
{
   char text[40];

   if( X_IS_STRING( key ) )
      s_string( buf, key->u.string.data, key->u.string.len );
   else if( X_IS_DATE( key ) )
   {
      s_date( text, sizeof( text ), key );
      s_string( buf, text, strlen( text ) );
   }
   else if( X_IS_LOGICAL( key ) )
      s_put( buf, IIF( key->u.logical, "\"true\"", "\"false\"" ) );
   else
   {
      X_JSON_BUF number = { NULL, 0, 0, 0 };

      if( X_IS_DOUBLE( key ) )
         s_double( &number, key->u.dbl );
      else if( X_IS_UINT64( key ) )
      {
         snprintf( text, sizeof( text ), "%llu", ( unsigned long long )key->u.unsignedInteger );
         s_put( &number, text );
      }
      else
      {
         snprintf( text, sizeof( text ), "%lld", ( long long )key->u.integer );
         s_put( &number, text );
      }

      s_string( buf, number.data, number.len );
      x_xfree( number.data );
   }
}

/* Append the JSON text of a value; 0 when the nesting is too deep */
static int s_encode( X_JSON_BUF *buf, const X_ITEM *value, int depth )
{
   char text[40];
   size_t i, count;

   if( depth > X_JSON_DEPTH )
   {
      s_setError( "nesting too deep or a cycle", ( size_t )-1 );
      return 0;
   }

   if( value == NULL || X_IS_NIL( value ) )
      s_put( buf, "null" );
   else if( X_IS_LOGICAL( value ) )
      s_put( buf, IIF( value->u.logical, "true", "false" ) );
   else if( X_IS_DOUBLE( value ) )
      s_double( buf, value->u.dbl );
   else if( X_IS_UINT64( value ) )
   {
      snprintf( text, sizeof( text ), "%llu", ( unsigned long long )value->u.unsignedInteger );
      s_put( buf, text );
   }
   else if( X_IS_NUMERIC( value ) )
   {
      snprintf( text, sizeof( text ), "%lld", ( long long )value->u.integer );
      s_put( buf, text );
   }
   else if( X_IS_STRING( value ) )
      s_string( buf, value->u.string.data, value->u.string.len );
   else if( X_IS_DATE( value ) )
   {
      s_date( text, sizeof( text ), value );
      if( text[0] == '\0' )
         s_put( buf, "null" );
      else
         s_string( buf, text, strlen( text ) );
   }
   else if( X_IS_HASH( value ) )
   {
      count = x_hashLen( value );

      s_put( buf, "{" );
      for( i = 1; i <= count; ++i )
      {
         if( i > 1 )
            s_put( buf, "," );

         s_newLine( buf, depth + 1 );
         s_key( buf, x_hashKeyAt( value, i ) );
         s_put( buf, IIF( buf->pretty, ": ", ":" ) );

         if( !s_encode( buf, x_hashValueAt( value, i ), depth + 1 ) )
            return 0;
      }

      if( count > 0 )
         s_newLine( buf, depth );

      s_put( buf, "}" );
   }
   else if( value->nType == X_IT_ARRAY && !x_arrayIsObject( value ) )
   {
      count = ( size_t )x_arrayLen( value );

      s_put( buf, "[" );
      for( i = 1; i <= count; ++i )
      {
         if( i > 1 )
            s_put( buf, "," );

         s_newLine( buf, depth + 1 );
         if( !s_encode( buf, x_arrayGetItemPtr( value, i ), depth + 1 ) )
            return 0;
      }

      if( count > 0 )
         s_newLine( buf, depth );

      s_put( buf, "]" );
   }
   else
   {
      /* Objects, code blocks and pointers have no JSON form */
      s_put( buf, "null" );
   }

   return 1;
}

/* The JSON text of a value, NULL on error (see x_jsonError()) */
char *x_jsonEncode( const X_ITEM *value, int pretty, size_t *length )
{
   X_JSON_BUF buf = { NULL, 0, 0, pretty };

   s_error[0] = '\0';
   if( !s_encode( &buf, value, 0 ) )
   {
      x_xfree( buf.data );
      return NULL;
   }

   if( buf.data == NULL )
      s_put( &buf, "" );

   *length = buf.len;

   return buf.data;
}

/* ---------------------------------------------------------------------------------------------
   Decoding
   --------------------------------------------------------------------------------------------- */

/* The text being read and the position in it */
typedef struct
{
   const char *start;
   const char *pos;
   const char *end;
} X_JSON_READ;

/* Remember an error at the current position; returns 0 */
static int s_fail( X_JSON_READ *read, const char *message )
{
   s_setError( message, ( size_t )( read->pos - read->start ) );

   return 0;
}

/* Skip the white space of JSON: space, tab, line feed and carriage return */
static void s_space( X_JSON_READ *read )
{
   while( read->pos < read->end )
   {
      char c = *read->pos;

      if( c != ' ' && c != '\t' && c != '\n' && c != '\r' )
         break;

      ++read->pos;
   }
}

/* Read the word true, false or null */
static int s_word( X_JSON_READ *read, const char *word )
{
   size_t length = strlen( word );

   if( ( size_t )( read->end - read->pos ) < length || memcmp( read->pos, word, length ) != 0 )
      return s_fail( read, "unexpected character" );

   read->pos += length;

   return 1;
}

/* Read the four hex digits of a \u escape */
static int s_hex4( X_JSON_READ *read, unsigned int *code )
{
   int i;

   *code = 0;

   if( read->end - read->pos < 4 )
      return s_fail( read, "short \\u escape" );

   for( i = 0; i < 4; ++i )
   {
      char c = *read->pos++;

      *code <<= 4;
      if( c >= '0' && c <= '9' )
         *code |= ( unsigned int )( c - '0' );
      else if( c >= 'a' && c <= 'f' )
         *code |= ( unsigned int )( c - 'a' + 10 );
      else if( c >= 'A' && c <= 'F' )
         *code |= ( unsigned int )( c - 'A' + 10 );
      else
      {
         --read->pos;
         return s_fail( read, "bad \\u escape" );
      }
   }

   return 1;
}

/* Append a code point as UTF-8 */
static void s_utf8( X_JSON_BUF *buf, unsigned int code )
{
   char bytes[4];

   if( code < 0x80 )
   {
      bytes[0] = ( char )code;
      s_putLen( buf, bytes, 1 );
   }
   else if( code < 0x800 )
   {
      bytes[0] = ( char )( 0xC0 | ( code >> 6 ) );
      bytes[1] = ( char )( 0x80 | ( code & 0x3F ) );
      s_putLen( buf, bytes, 2 );
   }
   else if( code < 0x10000 )
   {
      bytes[0] = ( char )( 0xE0 | ( code >> 12 ) );
      bytes[1] = ( char )( 0x80 | ( ( code >> 6 ) & 0x3F ) );
      bytes[2] = ( char )( 0x80 | ( code & 0x3F ) );
      s_putLen( buf, bytes, 3 );
   }
   else
   {
      bytes[0] = ( char )( 0xF0 | ( code >> 18 ) );
      bytes[1] = ( char )( 0x80 | ( ( code >> 12 ) & 0x3F ) );
      bytes[2] = ( char )( 0x80 | ( ( code >> 6 ) & 0x3F ) );
      bytes[3] = ( char )( 0x80 | ( code & 0x3F ) );
      s_putLen( buf, bytes, 4 );
   }
}

/* Read a string in quotes; the escapes become their characters */
static int s_readString( X_JSON_READ *read, X_ITEM *result )
{
   X_JSON_BUF buf = { NULL, 0, 0, 0 };

   ++read->pos; /* " */

   s_put( &buf, "" );

   while( read->pos < read->end && *read->pos != '"' )
   {
      unsigned char c = ( unsigned char )*read->pos;

      if( c < 0x20 )
      {
         x_xfree( buf.data );
         return s_fail( read, "control character in a string" );
      }

      /* A run of plain characters */
      if( c != '\\' )
      {
         const char *run = read->pos;

         while( read->pos < read->end && *read->pos != '"' && *read->pos != '\\' )
         {
            if( ( unsigned char )*read->pos < 0x20 )
               break;

            ++read->pos;
         }

         s_putLen( &buf, run, ( size_t )( read->pos - run ) );
         continue;
      }

      if( ++read->pos >= read->end )
         break;

      switch( *read->pos++ )
      {
      case '"': s_put( &buf, "\"" ); break;
      case '\\': s_put( &buf, "\\" ); break;
      case '/': s_put( &buf, "/" ); break;
      case 'b': s_put( &buf, "\b" ); break;
      case 'f': s_put( &buf, "\f" ); break;
      case 'n': s_put( &buf, "\n" ); break;
      case 'r': s_put( &buf, "\r" ); break;
      case 't': s_put( &buf, "\t" ); break;
      case 'u':
      {
         unsigned int code, low;
         int surrogate;

         if( !s_hex4( read, &code ) )
         {
            x_xfree( buf.data );
            return 0;
         }

         /* A pair of surrogates is one character */
         surrogate = code >= 0xD800 && code <= 0xDBFF && read->end - read->pos >= 6;
         if( surrogate && read->pos[0] == '\\' && read->pos[1] == 'u' )
         {
            read->pos += 2;
            if( !s_hex4( read, &low ) )
            {
               x_xfree( buf.data );
               return 0;
            }

            if( low >= 0xDC00 && low <= 0xDFFF )
               code = 0x10000 + ( ( code - 0xD800 ) << 10 ) + ( low - 0xDC00 );
            else
            {
               s_utf8( &buf, 0xFFFD );
               code = low;
            }
         }

         if( code >= 0xD800 && code <= 0xDFFF )
            code = 0xFFFD;

         s_utf8( &buf, code );
         break;
      }
      default:
         --read->pos;
         x_xfree( buf.data );
         return s_fail( read, "bad escape in a string" );
      }
   }

   if( read->pos >= read->end )
   {
      x_xfree( buf.data );
      return s_fail( read, "unterminated string" );
   }

   ++read->pos; /* " */
   x_itemPutCL( result, buf.data, buf.len );
   x_xfree( buf.data );

   return 1;
}

/* A whole number that fits is an integer, the others are double */
static int s_readNumber( X_JSON_READ *read, X_ITEM *result )
{
   const char *start = read->pos;
   int integer = 1;
   char text[64];
   size_t length;

   if( read->pos < read->end && *read->pos == '-' )
      ++read->pos;

   if( read->pos >= read->end || !( *read->pos >= '0' && *read->pos <= '9' ) )
      return s_fail( read, "bad number" );

   if( *read->pos == '0' )
      ++read->pos;
   else
   {
      while( read->pos < read->end && *read->pos >= '0' && *read->pos <= '9' )
         ++read->pos;
   }

   if( read->pos < read->end && *read->pos == '.' )
   {
      integer = 0;
      ++read->pos;
      if( read->pos >= read->end || !( *read->pos >= '0' && *read->pos <= '9' ) )
         return s_fail( read, "bad number" );

      while( read->pos < read->end && *read->pos >= '0' && *read->pos <= '9' )
         ++read->pos;
   }

   if( read->pos < read->end && ( *read->pos == 'e' || *read->pos == 'E' ) )
   {
      integer = 0;
      ++read->pos;
      if( read->pos < read->end && ( *read->pos == '+' || *read->pos == '-' ) )
         ++read->pos;

      if( read->pos >= read->end || !( *read->pos >= '0' && *read->pos <= '9' ) )
         return s_fail( read, "bad number" );

      while( read->pos < read->end && *read->pos >= '0' && *read->pos <= '9' )
         ++read->pos;
   }

   length = ( size_t )( read->pos - start );
   if( length >= sizeof( text ) )
      return s_fail( read, "number too long" );

   memcpy( text, start, length );
   text[length] = '\0';

   if( integer )
   {
      long long value;

      errno = 0;
      value = strtoll( text, NULL, 10 );
      if( errno == 0 )
      {
         x_itemPutNInt( result, ( int64_t )value );
         return 1;
      }
   }

   x_itemPutND( result, strtod( text, NULL ) );

   return 1;
}

static int s_readValue( X_JSON_READ *read, X_ITEM *result, int depth );

/* Read an array in brackets */
static int s_readArray( X_JSON_READ *read, X_ITEM *result, int depth )
{
   ++read->pos; /* [ */
   x_arrayNew( result, 0 );
   s_space( read );

   if( read->pos < read->end && *read->pos == ']' )
   {
      ++read->pos;
      return 1;
   }

   while( 1 )
   {
      X_ITEM value __attribute__( ( cleanup( x_itemClear ) ) ) = X_ITEM_NIL_INIT;

      if( !s_readValue( read, &value, depth + 1 ) )
         return 0;

      x_arrayAdd( result, &value );

      s_space( read );
      if( read->pos < read->end && *read->pos == ',' )
      {
         ++read->pos;
         continue;
      }

      if( read->pos < read->end && *read->pos == ']' )
      {
         ++read->pos;
         return 1;
      }

      return s_fail( read, "expected , or ]" );
   }
}

/* Read an object in braces as a hash with the order of its keys */
static int s_readObject( X_JSON_READ *read, X_ITEM *result, int depth )
{
   ++read->pos; /* { */
   x_hashNew( result );
   s_space( read );

   if( read->pos < read->end && *read->pos == '}' )
   {
      ++read->pos;
      return 1;
   }

   while( 1 )
   {
      X_ITEM key __attribute__( ( cleanup( x_itemClear ) ) ) = X_ITEM_NIL_INIT;
      X_ITEM value __attribute__( ( cleanup( x_itemClear ) ) ) = X_ITEM_NIL_INIT;

      s_space( read );
      if( read->pos >= read->end || *read->pos != '"' )
         return s_fail( read, "expected a key in quotes" );

      if( !s_readString( read, &key ) )
         return 0;

      s_space( read );

      if( read->pos >= read->end || *read->pos != ':' )
         return s_fail( read, "expected :" );

      ++read->pos;
      if( !s_readValue( read, &value, depth + 1 ) )
         return 0;

      x_hashAdd( result, &key, &value );
      s_space( read );

      if( read->pos < read->end && *read->pos == ',' )
      {
         ++read->pos;
         continue;
      }

      if( read->pos < read->end && *read->pos == '}' )
      {
         ++read->pos;
         return 1;
      }

      return s_fail( read, "expected , or }" );
   }
}

/* Read any value */
static int s_readValue( X_JSON_READ *read, X_ITEM *result, int depth )
{
   if( depth > X_JSON_DEPTH )
      return s_fail( read, "nesting too deep" );

   s_space( read );
   if( read->pos >= read->end )
      return s_fail( read, "unexpected end" );

   switch( *read->pos )
   {
   case '{': return s_readObject( read, result, depth );
   case '[': return s_readArray( read, result, depth );
   case '"': return s_readString( read, result );
   case 't':
      if( !s_word( read, "true" ) )
         return 0;

      x_itemPutL( result, 1 );
      return 1;
   case 'f':
      if( !s_word( read, "false" ) )
         return 0;

      x_itemPutL( result, 0 );
      return 1;
   case 'n':
      if( !s_word( read, "null" ) )
         return 0;

      x_itemClear( result );
      return 1;
   default:
      return s_readNumber( read, result );
   }
}

/* The value of a JSON text; 0 on error (see x_jsonError()) */
int x_jsonDecode( const char *text, size_t length, X_ITEM *result )
{
   X_JSON_READ read = { text, text, text + length };

   s_error[0] = '\0';

   /* A byte order mark before UTF-8 is skipped */
   if( length >= 3 && memcmp( text, "\xEF\xBB\xBF", 3 ) == 0 )
      read.pos += 3;

   if( !s_readValue( &read, result, 0 ) )
   {
      x_itemClear( result );
      return 0;
   }

   s_space( &read );
   if( read.pos < read.end )
   {
      x_itemClear( result );
      return s_fail( &read, "text after the value" );
   }

   return 1;
}
