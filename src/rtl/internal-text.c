/*
 * Copyright (c) 2024-2026 xDev-Language
 *
 * Library...: xDev Runtime
 * File......: internal-text.c
 *
 */

#include "classes.h"
#include "internal-private.h"
#include "platform.h"
#include "terminal.h"

#include "../../include/xdev/inkey.ch"

#include <ctype.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Upper() and Lower(): uppercase letters first..last and their lowercase pairs at letter + delta. With step 2
   only every second code point is uppercase. The tables cover the alphabets supported by the Unicode text
   functions. A pair never takes more UTF-8 bytes in the other case, so a converted text is never longer. */
typedef struct
{
   uint16_t first;
   uint16_t last;
   int16_t delta;
   uint16_t step;
} S_CASE_RANGE;

/* CharSort(): one sorting element; equal keys keep their order */
typedef struct
{
   const uint32_t *key;
   size_t index;
   size_t compare;
   int descending;
} S_CHARSORT_ITEM;

/* CStr() and ValToPrgExp(): text built piece by piece */
typedef struct
{
   char *data;
   size_t length;
   size_t size;
} S_TEXT;

/* ValToPrgExp(): the arrays and hashes being written, to find an array that contains itself */
typedef struct S_TEXT_PARENT
{
   const void *container;
   const struct S_TEXT_PARENT *parent;
} S_TEXT_PARENT;

static const S_CASE_RANGE s_caseRanges[] = {
   { 0x00C0, 0x00D6, 32, 1 }, { 0x00D8, 0x00DE, 32, 1 }, { 0x0100, 0x012E, 1, 2 }, { 0x0132, 0x0136, 1, 2 },
   { 0x0139, 0x0147, 1, 2 }, { 0x014A, 0x0176, 1, 2 }, { 0x0178, 0x0178, -121, 1 }, { 0x0179, 0x017D, 1, 2 },
   { 0x01CD, 0x01DB, 1, 2 }, { 0x01DE, 0x01EE, 1, 2 }, { 0x01F8, 0x021E, 1, 2 }, { 0x0222, 0x0232, 1, 2 },
   { 0x0386, 0x0386, 38, 1 }, { 0x0388, 0x038A, 37, 1 }, { 0x038C, 0x038C, 64, 1 }, { 0x038E, 0x038F, 63, 1 },
   { 0x0391, 0x03A1, 32, 1 }, { 0x03A3, 0x03AB, 32, 1 }, { 0x0400, 0x040F, 80, 1 }, { 0x0410, 0x042F, 32, 1 },
   { 0x0460, 0x0480, 1, 2 }, { 0x048A, 0x04BE, 1, 2 }, { 0x04C0, 0x04C0, 15, 1 }, { 0x04C1, 0x04CD, 1, 2 },
   { 0x04D0, 0x052E, 1, 2 }, { 0x0531, 0x0556, 48, 1 }, { 0x1E00, 0x1E94, 1, 2 }, { 0x1EA0, 0x1EFE, 1, 2 }
};

/* IsAlpha(): letters supported by the Unicode text functions */
static const uint16_t s_letterRanges[][2] = {
   { 0x0041, 0x005A }, { 0x0061, 0x007A }, { 0x00AA, 0x00AA }, { 0x00B5, 0x00B5 }, { 0x00BA, 0x00BA },
   { 0x00C0, 0x00D6 }, { 0x00D8, 0x00F6 }, { 0x00F8, 0x02AF }, { 0x0370, 0x0373 }, { 0x0376, 0x0377 },
   { 0x037B, 0x037D }, { 0x037F, 0x037F }, { 0x0386, 0x0386 }, { 0x0388, 0x03F5 }, { 0x03F7, 0x0481 },
   { 0x048A, 0x052F }, { 0x0531, 0x0556 }, { 0x0561, 0x0587 }, { 0x05D0, 0x05EA }, { 0x0620, 0x064A },
   { 0x066E, 0x06D3 }, { 0x0E01, 0x0E30 }, { 0x0E32, 0x0E33 }, { 0x0E40, 0x0E46 }, { 0x1E00, 0x1FFF },
   { 0x3041, 0x3096 }, { 0x30A1, 0x30FA }, { 0x3400, 0x4DBF }, { 0x4E00, 0x9FFF }, { 0xAC00, 0xD7A3 }
};

/* The screen characters of the control codes 0 to 31 (code page 437) */
static const uint16_t s_controlCharacters[32] = {
   0x2007, 0x263A, 0x263B, 0x2665, 0x2666, 0x2663, 0x2660, 0x2022,
   0x25D8, 0x25CB, 0x25D9, 0x2642, 0x2640, 0x266A, 0x266B, 0x263C,
   0x25BA, 0x25C4, 0x2195, 0x203C, 0x00B6, 0x00A7, 0x25AC, 0x21A8,
   0x2191, 0x2193, 0x2192, 0x2190, 0x221F, 0x2194, 0x25B2, 0x25BC
};

/* The Base64 alphabet */
static const char s_base64[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

/* MD5: the integer part of abs( sin( i + 1 ) ) * 2^32 */
static const uint32_t s_md5Sine[64] = {
   0xD76AA478, 0xE8C7B756, 0x242070DB, 0xC1BDCEEE,
   0xF57C0FAF, 0x4787C62A, 0xA8304613, 0xFD469501,
   0x698098D8, 0x8B44F7AF, 0xFFFF5BB1, 0x895CD7BE,
   0x6B901122, 0xFD987193, 0xA679438E, 0x49B40821,
   0xF61E2562, 0xC040B340, 0x265E5A51, 0xE9B6C7AA,
   0xD62F105D, 0x02441453, 0xD8A1E681, 0xE7D3FBC8,
   0x21E1CDE6, 0xC33707D6, 0xF4D50D87, 0x455A14ED,
   0xA9E3E905, 0xFCEFA3F8, 0x676F02D9, 0x8D2A4C8A,
   0xFFFA3942, 0x8771F681, 0x6D9D6122, 0xFDE5380C,
   0xA4BEEA44, 0x4BDECFA9, 0xF6BB4B60, 0xBEBFBC70,
   0x289B7EC6, 0xEAA127FA, 0xD4EF3085, 0x04881D05,
   0xD9D4D039, 0xE6DB99E5, 0x1FA27CF8, 0xC4AC5665,
   0xF4292244, 0x432AFF97, 0xAB9423A7, 0xFC93A039,
   0x655B59C3, 0x8F0CCC92, 0xFFEFF47D, 0x85845DD1,
   0x6FA87E4F, 0xFE2CE6E0, 0xA3014314, 0x4E0811A1,
   0xF7537E82, 0xBD3AF235, 0x2AD7D2BB, 0xEB86D391
};

/* MD5: the rotation of each step */
static const unsigned char s_md5Shift[64] = {
   7, 12, 17, 22, 7, 12, 17, 22, 7, 12, 17, 22, 7, 12, 17, 22,
   5, 9, 14, 20, 5, 9, 14, 20, 5, 9, 14, 20, 5, 9, 14, 20,
   4, 11, 16, 23, 4, 11, 16, 23, 4, 11, 16, 23, 4, 11, 16, 23,
   6, 10, 15, 21, 6, 10, 15, 21, 6, 10, 15, 21, 6, 10, 15, 21
};

/* ---------------------------------------------------------------------------------------------
   UTF-8 and platform text
   --------------------------------------------------------------------------------------------- */

/* A code point, or U+FFFD for a surrogate or a value above U+10FFFF */
static uint32_t s_unicodeScalar( uint32_t codepoint )
{
   return IIF( codepoint <= 0x10FFFF && !( codepoint >= 0xD800 && codepoint <= 0xDFFF ), codepoint, 0xFFFD );
}

/* The line end of the operating system */
const char *x_eol( void )
{
#if X_OS_WINDOWS
   return "\r\n";
#else
   return "\n";
#endif
}

/* The line end used by a text: CR LF, LF or CR; the line end of the system when it has none */
const char *x_eolDetect( const char *text, size_t length )
{
   int lf = 0;
   int cr = 0;
   size_t i;

   for( i = 0; i < length; ++i )
   {
      if( text[i] == '\r' && i + 1 < length && text[i + 1] == '\n' )
         return "\r\n";

      if( text[i] == '\n' )
         lf = 1;
      else if( text[i] == '\r' )
         cr = 1;
   }

   if( lf )
      return "\n";

   if( cr )
      return "\r";

   return x_eol();
}

/* The screen character of a control code; other characters stay */
uint16_t x_u16Ctrl( uint16_t character )
{
   if( character < 32 )
      return s_controlCharacters[character];

   return IIF( character == 0x007F, 0x2302, character );
}

/* The number of UTF-8 bytes of a code point */
size_t x_utf8CharSize( uint32_t codepoint )
{
   codepoint = s_unicodeScalar( codepoint );
   if( codepoint < 0x80 )
      return 1;

   if( codepoint < 0x800 )
      return 2;

   return IIF( codepoint < 0x10000, 3, 4 );
}

/* Write a code point as UTF-8; NULL destination only measures; returns the number of bytes */
size_t x_utf8Encode( char *destination, uint32_t codepoint )
{
   size_t size;

   codepoint = s_unicodeScalar( codepoint );
   size = x_utf8CharSize( codepoint );
   if( destination == NULL )
      return size;

   if( size == 1 )
      destination[0] = ( char )codepoint;
   else if( size == 2 )
   {
      destination[0] = ( char )( 0xC0 | codepoint >> 6 );
      destination[1] = ( char )( 0x80 | ( codepoint & 0x3F ) );
   }
   else if( size == 3 )
   {
      destination[0] = ( char )( 0xE0 | codepoint >> 12 );
      destination[1] = ( char )( 0x80 | ( codepoint >> 6 & 0x3F ) );
      destination[2] = ( char )( 0x80 | ( codepoint & 0x3F ) );
   }
   else
   {
      destination[0] = ( char )( 0xF0 | codepoint >> 18 );
      destination[1] = ( char )( 0x80 | ( codepoint >> 12 & 0x3F ) );
      destination[2] = ( char )( 0x80 | ( codepoint >> 6 & 0x3F ) );
      destination[3] = ( char )( 0x80 | ( codepoint & 0x3F ) );
   }

   return size;
}

/* Read one UTF-8 character at *offset */
int x_utf8Decode( const char *source, size_t length, size_t *offset, uint32_t *codepoint )
{
   const unsigned char *bytes = ( const unsigned char * )source;
   size_t index;
   uint32_t value;
   uint32_t minimum;
   unsigned int continuationCount;
   unsigned int continuation;

   if( source == NULL || offset == NULL || codepoint == NULL || *offset >= length )
      return 0;

   index = *offset;
   value = bytes[index++];
   if( value < 0x80 )
   {
      *offset = index;
      *codepoint = value;
      return 1;
   }

   if( value >= 0xC2 && value <= 0xDF )
   {
      value &= 0x1F;
      minimum = 0x80;
      continuationCount = 1;
   }
   else if( value >= 0xE0 && value <= 0xEF )
   {
      value &= 0x0F;
      minimum = 0x800;
      continuationCount = 2;
   }
   else if( value >= 0xF0 && value <= 0xF4 )
   {
      value &= 0x07;
      minimum = 0x10000;
      continuationCount = 3;
   }
   else
   {
      *offset = index;
      *codepoint = 0xFFFD;
      return 0;
   }

   if( length - index < continuationCount )
   {
      *offset = index;
      *codepoint = 0xFFFD;
      return 0;
   }

   for( continuation = 0; continuation < continuationCount; ++continuation )
   {
      unsigned char byte = bytes[index + continuation];

      if( ( byte & 0xC0 ) != 0x80 )
      {
         *offset = index;
         *codepoint = 0xFFFD;
         return 0;
      }

      value = value << 6 | ( byte & 0x3F );
   }

   if( value < minimum || value > 0x10FFFF || ( value >= 0xD800 && value <= 0xDFFF ) )
   {
      *offset = index;
      *codepoint = 0xFFFD;
      return 0;
   }

   *offset = index + continuationCount;
   *codepoint = value;

   return 1;
}

/* Is the text valid UTF-8? */
int x_utf8Validate( const char *source, size_t length )
{
   size_t offset = 0;
   uint32_t codepoint;

   if( source == NULL )
      return length == 0;

   while( offset < length )
   {
      if( !x_utf8Decode( source, length, &offset, &codepoint ) )
         return 0;
   }

   return 1;
}

/* Is the number a code point that can be written as UTF-8? */
static int s_utf8ScalarNumber( double number, uint32_t *codepoint )
{
   if( !( number >= 0 && number <= 0x10FFFF ) )
      return 0;

   *codepoint = ( uint32_t )number;

   return number == *codepoint && ( *codepoint < 0xD800 || *codepoint > 0xDFFF );
}

/* The bytes first..after of the character at a position (from 1); 0 outside the text, -1 for broken UTF-8 */
static int s_utf8Character( const char *text, size_t length, double requested, size_t *first, size_t *after,
                            uint32_t *codepoint )
{
   size_t offset = 0, position;

   if( !( requested >= 1 && requested < ( double )SIZE_MAX ) || requested >= ( double )length + 1 )
      return 0;

   position = ( size_t )requested;
   while( offset < length )
   {
      *first = offset;
      if( !x_utf8Decode( text, length, &offset, codepoint ) )
         return -1;

      if( --position == 0 )
      {
         *after = offset;
         return 1;
      }
   }

   return 0;
}

/* UTF8Chr(): the character of a code point */
void x_utf8Chr( X_CALL_CTX *ctx, double number )
{
   uint32_t codepoint;
   char result[4];

   X_ARGCHECK( ctx, s_utf8ScalarNumber( number, &codepoint ), "UTF8CHR" );

   x_retclen( ctx, result, x_utf8Encode( result, codepoint ) );
}

/* UTF8Asc(): the code point of the first character */
void x_utf8Asc( X_CALL_CTX *ctx, const char *text, size_t length )
{
   uint32_t codepoint = 0;
   size_t offset = 0;

   if( length && !x_utf8Decode( text, length, &offset, &codepoint ) )
   {
      x_errorRaiseNamed( ctx->result, 1102, "UTF8ASC" );
      return;
   }

   x_retnint( ctx, codepoint );
}

/* UPeek(): the code point of the character at a position, 0 outside the text */
void x_utf8Peek( X_CALL_CTX *ctx, const char *text, size_t length, double position )
{
   uint32_t codepoint = 0;
   size_t first, after;
   int found = s_utf8Character( text, length, position, &first, &after, &codepoint );

   X_ARGCHECK( ctx, found >= 0, "UPEEK" );
   x_retnint( ctx, IIF( found, codepoint, 0 ) );
}

/* UPoke(): the character at a position replaced by a code point */
void x_utf8Poke( X_CALL_CTX *ctx, const char *text, size_t length, double position, double number )
{
   uint32_t codepoint, previous;
   size_t first, after, encoded, total;
   char *result;
   int found;

   X_ARGCHECK( ctx, s_utf8ScalarNumber( number, &codepoint ), "UPOKE" );
   found = s_utf8Character( text, length, position, &first, &after, &previous );
   X_ARGCHECK( ctx, found >= 0, "UPOKE" );
   if( !found )
   {
      x_retclen( ctx, text, length );
      return;
   }

   encoded = x_utf8CharSize( codepoint );
   total = length - ( after - first );
   X_ARGCHECK( ctx, encoded <= SIZE_MAX - total - 1, "UPOKE" );
   total += encoded;
   result = ( char * )x_xgrab( total + 1 );
   memcpy( result, text, first );
   x_utf8Encode( result + first, codepoint );
   memcpy( result + first + encoded, text + after, length - after );
   result[total] = '\0';
   x_storclen( ctx, result, total, 1 );
   x_retclen_buffer( ctx, result, total );
}

/* The number of characters of a UTF-8 text; a broken byte counts as one character */
size_t x_utf8Length( const char *source, size_t length )
{
   size_t offset = 0;
   size_t count = 0;
   uint32_t codepoint;

   if( source == NULL )
      return 0;

   while( offset < length )
   {
      x_utf8Decode( source, length, &offset, &codepoint );
      ++count;
   }

   return count;
}

/* The byte offset of character count (from 0); the length when the text is shorter */
size_t x_utf8Offset( const char *source, size_t length, size_t count )
{
   size_t offset = 0;
   uint32_t codepoint;

   while( count > 0 && offset < length )
   {
      x_utf8Decode( source, length, &offset, &codepoint );
      --count;
   }

   return offset;
}

/* USubStr(): characters from start; a negative start counts from the end */
void x_utf8SubStr( X_CALL_CTX *ctx, const char *text, size_t length, int64_t start, int hasCount, int64_t count )
{
   int64_t chars = ( int64_t )x_utf8Length( text, length );
   size_t first;

   if( start < 0 )
      start = IIF( chars + start + 1 < 1, 1, chars + start + 1 );
   else if( start == 0 )
      start = 1;

   if( start > chars )
   {
      x_retclen( ctx, "", 0 );
      return;
   }

   if( !hasCount || count > chars - ( start - 1 ) )
      count = chars - ( start - 1 );
   else if( count < 0 )
      count = 0;

   first = x_utf8Offset( text, length, ( size_t )( start - 1 ) );
   x_retclen( ctx, text + first, x_utf8Offset( text + first, length - first, ( size_t )count ) );
}

/* ULeft(): the first count characters */
void x_utf8Left( X_CALL_CTX *ctx, const char *text, size_t length, int64_t count )
{
   x_retclen( ctx, text, IIF( count <= 0, 0, x_utf8Offset( text, length, ( size_t )count ) ) );
}

/* URight(): the last count characters */
void x_utf8Right( X_CALL_CTX *ctx, const char *text, size_t length, int64_t count )
{
   size_t chars = x_utf8Length( text, length );
   size_t first;

   if( count <= 0 )
      first = length;
   else if( ( uint64_t )count >= chars )
      first = 0;
   else
      first = x_utf8Offset( text, length, chars - ( size_t )count );

   x_retclen( ctx, text + first, length - first );
}

/* The Pad*() functions: the text cut or padded to width characters with one fill character */
void x_utf8Pad( X_CALL_CTX *ctx, const char *text, size_t length, int64_t width, const char *fill, size_t fillLength,
                int leftPad, int rightPad, const char *operation )
{
   size_t total = IIF( width < 0, 0, ( size_t )width );
   size_t chars = x_utf8Length( text, length );
   size_t fillBytes, padTotal, padLeft, size, i;
   char *buffer, *cursor;

   if( chars >= total )
   {
      x_retclen( ctx, text, x_utf8Offset( text, length, total ) );
      return;
   }

   if( fill == NULL || fillLength == 0 )
   {
      fill = " ";
      fillLength = 1;
   }

   fillBytes = x_utf8Offset( fill, fillLength, 1 );
   padTotal = total - chars;
   if( padTotal > ( SIZE_MAX - length - 1 ) / fillBytes )
   {
      x_errorRaiseNamed( ctx->result, 1102, operation );
      return;
   }

   if( leftPad && rightPad )
      padLeft = padTotal / 2;
   else if( leftPad )
      padLeft = padTotal;
   else
      padLeft = 0;

   size = length + padTotal * fillBytes;
   buffer = ( char * )x_xgrab( size + 1 );
   cursor = buffer;
   for( i = 0; i < padLeft; ++i, cursor += fillBytes )
      memcpy( cursor, fill, fillBytes );

   memcpy( cursor, text, length );
   cursor += length;
   for( i = padLeft; i < padTotal; ++i, cursor += fillBytes )
      memcpy( cursor, fill, fillBytes );

   *cursor = '\0';
   x_retclen_buffer( ctx, buffer, size );
}

/* UAt(): the character position (from 1) of search between the characters start and end; 0 when not found */
int64_t x_utf8At( const char *search, size_t searchLength, const char *text, size_t length, int64_t start,
                  int hasStart, int64_t end, int hasEnd )
{
   int64_t chars = ( int64_t )x_utf8Length( text, length );
   size_t first, last, from, found;
   uint32_t codepoint;

   start = IIF( !hasStart || start < 1, 1, start );
   end = IIF( !hasEnd || end > chars, chars, end );
   if( start > end )
      return 0;

   first = x_utf8Offset( text, length, ( size_t )( start - 1 ) );
   last = first + x_utf8Offset( text + first, length - first, ( size_t )( end - start + 1 ) );
   for( from = first; from < last && ( found = x_strAt( search, searchLength, text + from, last - from ) ) != 0; )
   {
      size_t at = from + found - 1;
      size_t offset = first;
      int64_t index = start;

      while( offset < at )
      {
         x_utf8Decode( text, length, &offset, &codepoint );
         ++index;
      }

      if( offset == at )
         return index;

      from = at + 1;
   }

   return 0;
}

/* URAt(): the character position (from 1) of the last search between the characters start and end */
void x_utf8RAt( X_CALL_CTX *ctx, const char *search, size_t searchLength, const char *text, size_t length,
                int64_t start, int hasStart, int64_t end, int hasEnd )
{
   int64_t chars;
   size_t first, last, found;

   X_ARGCHECK( ctx, x_utf8Validate( search, searchLength ) && x_utf8Validate( text, length ), "URAT" );
   chars = ( int64_t )x_utf8Length( text, length );
   start = IIF( !hasStart || start < 1, 1, start );
   end = IIF( !hasEnd || end > chars, chars, end );
   if( start > end )
   {
      x_retni( ctx, 0 );
      return;
   }

   first = x_utf8Offset( text, length, ( size_t )( start - 1 ) );
   last = first + x_utf8Offset( text + first, length - first, ( size_t )( end - start + 1 ) );
   found = x_strRAt( search, searchLength, text + first, last - first );
   x_retnint( ctx, IIF( found, start + ( int64_t )x_utf8Length( text + first, found - 1 ), 0 ) );
}

/* UStuff(): remove characters at start and insert a text there */
void x_utf8Stuff( X_CALL_CTX *ctx, const char *text, size_t length, int64_t start, int64_t remove, const char *insert,
                  size_t insertLength )
{
   int64_t chars = ( int64_t )x_utf8Length( text, length );
   int64_t head = start - 1;
   int64_t count = remove;
   size_t first, after, total;
   char *buffer;

   /* Start 0 inserts in front, a start outside the text appends */
   if( start == 0 )
      head = 0;
   else if( start < 0 || start > chars )
      head = chars;

   /* A negative count or one past the end removes the rest */
   if( remove < 0 || remove > chars - head )
      count = chars - head;

   first = x_utf8Offset( text, length, ( size_t )head );
   after = first + x_utf8Offset( text + first, length - first, ( size_t )count );
   total = first + insertLength + ( length - after );
   buffer = ( char * )x_xgrab( total + 1 );

   memcpy( buffer, text, first );
   memcpy( buffer + first, insert, insertLength );
   memcpy( buffer + first + insertLength, text + after, length - after );
   buffer[total] = '\0';
   x_retclen_buffer( ctx, buffer, total );
}

/* A little endian number of size bytes; missing bytes are 0 */
static uint64_t s_binRead( const char *text, size_t length, size_t size )
{
   uint64_t value = 0;
   size_t i;

   for( i = 0; i < size; ++i )
      value |= ( uint64_t )IIF( i < length, ( unsigned char )text[i], 0 ) << ( i * 8 );

   return value;
}

/* Return a number as size little endian bytes */
static void s_binReturn( X_CALL_CTX *ctx, uint64_t value, size_t size )
{
   char buffer[8];
   size_t i;

   for( i = 0; i < size; ++i )
      buffer[i] = ( char )( ( value >> ( i * 8 ) ) & 0xFF );

   x_retclen( ctx, buffer, size );
}

/* Bin2U(): 4 little endian bytes as a number */
uint32_t x_binUInt32( const char *text, size_t length )
{
   return ( uint32_t )s_binRead( text, length, 4 );
}

/* 4 bytes as a FLOAT32 */
double x_binFloat32( const char *text, size_t length )
{
   uint32_t bits = ( uint32_t )s_binRead( text, length, 4 );
   float value;

   memcpy( &value, &bits, sizeof( value ) );

   return value;
}

/* 8 bytes as a FLOAT64 */
double x_binFloat64( const char *text, size_t length )
{
   uint64_t bits = s_binRead( text, length, 8 );
   double value;

   memcpy( &value, &bits, sizeof( value ) );

   return value;
}

/* U2Bin(): a number as 4 little endian bytes */
void x_binRetUInt32( X_CALL_CTX *ctx, uint32_t value )
{
   s_binReturn( ctx, value, 4 );
}

/* A FLOAT32 as 4 bytes */
void x_binRetFloat32( X_CALL_CTX *ctx, double value )
{
   float single = ( float )value;
   uint32_t bits;

   memcpy( &bits, &single, sizeof( bits ) );
   s_binReturn( ctx, bits, 4 );
}

/* A FLOAT64 as 8 bytes */
void x_binRetFloat64( X_CALL_CTX *ctx, double value )
{
   uint64_t bits;

   memcpy( &bits, &value, sizeof( bits ) );
   s_binReturn( ctx, bits, 8 );
}

/* A UTF-8 text as screen cells; returns the number of cells */
size_t x_utf8ToU16( const char *source, size_t length, uint16_t *destination, size_t capacity )
{
   size_t offset = 0;
   size_t count = 0;
   uint32_t codepoint;

   if( source == NULL )
      return 0;

   while( offset < length )
   {
      x_utf8Decode( source, length, &offset, &codepoint );
      if( count < capacity && destination != NULL )
         destination[count] = ( uint16_t )IIF( codepoint <= 0xFFFF, codepoint, 0xFFFD );

      ++count;
   }

   return count;
}

/* A UTF-8 text as UTF-16; returns the number of units */
size_t x_utf8ToUtf16( const char *source, size_t length, uint16_t *destination, size_t capacity )
{
   size_t offset = 0;
   size_t count = 0;
   uint32_t codepoint;

   if( source == NULL )
      return 0;

   while( offset < length )
   {
      x_utf8Decode( source, length, &offset, &codepoint );
      if( codepoint <= 0xFFFF )
      {
         if( count < capacity && destination != NULL )
            destination[count] = ( uint16_t )codepoint;

         ++count;
      }
      else
      {
         uint32_t pair = codepoint - 0x10000;

         if( count + 1 < capacity && destination != NULL )
         {
            destination[count] = ( uint16_t )( 0xD800 | pair >> 10 );
            destination[count + 1] = ( uint16_t )( 0xDC00 | ( pair & 0x3FF ) );
         }

         count += 2;
      }
   }

   return count;
}

/* A UTF-16 text as UTF-8; a lone surrogate becomes U+FFFD; returns the number of bytes */
size_t x_utf16ToUtf8( const uint16_t *source, size_t length, char *destination, size_t capacity )
{
   size_t sourceIndex;
   size_t targetIndex = 0;

   if( source == NULL )
      return 0;

   for( sourceIndex = 0; sourceIndex < length; ++sourceIndex )
   {
      uint32_t codepoint = source[sourceIndex];
      uint32_t next = IIF( sourceIndex + 1 < length, source[sourceIndex + 1], 0 );
      int highSurrogate = codepoint >= 0xD800 && codepoint <= 0xDBFF;
      int lowSurrogateNext = next >= 0xDC00 && next <= 0xDFFF;
      size_t size;

      if( highSurrogate && lowSurrogateNext )
      {
         codepoint = 0x10000 + ( ( codepoint - 0xD800 ) << 10 ) + ( next - 0xDC00 );
         ++sourceIndex;
      }
      else if( codepoint >= 0xD800 && codepoint <= 0xDFFF )
         codepoint = 0xFFFD;

      size = x_utf8CharSize( codepoint );
      if( targetIndex + size <= capacity && destination != NULL )
         x_utf8Encode( destination + targetIndex, codepoint );

      targetIndex += size;
   }

   return targetIndex;
}

/* A UTF-16 copy with a zero at the end, allocated by x_xgrab() */
uint16_t *x_utf8ToUtf16Alloc( const char *source, size_t length, size_t *resultLength )
{
   size_t count = x_utf8ToUtf16( source, length, NULL, 0 );
   uint16_t *result = ( uint16_t * )x_xgrabArray( count + 1, sizeof( uint16_t ) );

   x_utf8ToUtf16( source, length, result, count );
   result[count] = 0;
   if( resultLength != NULL )
      *resultLength = count;

   return result;
}

/* A UTF-8 copy with a zero at the end, allocated by x_xgrab() */
char *x_utf16ToUtf8Alloc( const uint16_t *source, size_t length, size_t *resultLength )
{
   size_t count = x_utf16ToUtf8( source, length, NULL, 0 );
   char *result = ( char * )x_xgrabArray( count + 1, sizeof( char ) );

   x_utf16ToUtf8( source, length, result, count );
   result[count] = '\0';
   if( resultLength != NULL )
      *resultLength = count;

   return result;
}

/* The UTF-8 text a key types; 0 bytes for a key that types nothing */
size_t x_inkeyKeyText( int key, char *text )
{
   int character = X_INKEY_ISCHAR( key ) || X_INKEY_ISUNICODE( key );
   int plain = X_INKEY_VALUE( key ) >= 128 || !( X_INKEY_FLAGS( key ) & ( KF_CTRL | KF_ALT ) );
   int value;

   if( character && plain )
      value = X_INKEY_VALUE( key );
   else
   {
      value = x_inkeyKeyStd( key );
      if( value >= 128 )
         return 0;
   }

   return IIF( value >= 32 && value != 127, x_utf8Encode( text, ( uint32_t )value ), 0 );
}

/* ---------------------------------------------------------------------------------------------
   Letter case
   --------------------------------------------------------------------------------------------- */

/* The uppercase pair of a code point */
uint32_t x_utf8UpperChar( uint32_t codepoint )
{
   size_t i;

   if( codepoint < 0x80 )
      return IIF( codepoint >= 'a' && codepoint <= 'z', codepoint - 32, codepoint );

   /* Lowercase letters whose uppercase pair lies outside the ranges */
   switch( codepoint )
   {
   case 0x00B5:
      return 0x039C;

   case 0x0131:
      return 0x0049;

   case 0x017F:
      return 0x0053;

   case 0x03C2:
      return 0x03A3;
   }

   for( i = 0; i < sizeof( s_caseRanges ) / sizeof( s_caseRanges[0] ); ++i )
   {
      const S_CASE_RANGE *range = &s_caseRanges[i];
      int64_t upper = ( int64_t )codepoint - range->delta;

      if( upper >= range->first && upper <= range->last && ( upper - range->first ) % range->step == 0 )
         return ( uint32_t )upper;
   }

   return codepoint;
}

/* The lowercase pair of a code point */
uint32_t x_utf8LowerChar( uint32_t codepoint )
{
   size_t i;

   if( codepoint < 0x80 )
      return IIF( codepoint >= 'A' && codepoint <= 'Z', codepoint + 32, codepoint );

   if( codepoint == 0x0130 )
      return 0x0069;

   for( i = 0; i < sizeof( s_caseRanges ) / sizeof( s_caseRanges[0] ); ++i )
   {
      const S_CASE_RANGE *range = &s_caseRanges[i];

      if( codepoint >= range->first && codepoint <= range->last && ( codepoint - range->first ) % range->step == 0 )
         return ( uint32_t )( ( int64_t )codepoint + range->delta );
   }

   return codepoint;
}

/* Convert a text to one case and return its new length; destination holds at least length bytes. Broken UTF-8 bytes
   are copied unchanged, so binary data and UTF-8 text stay intact. */
size_t x_utf8Case( char *destination, const char *source, size_t length, int upper )
{
   size_t offset = 0;
   size_t first;
   size_t written = 0;
   uint32_t codepoint;

   while( offset < length )
   {
      first = offset;
      if( x_utf8Decode( source, length, &offset, &codepoint ) )
      {
         codepoint = IIF( upper, x_utf8UpperChar( codepoint ), x_utf8LowerChar( codepoint ) );
         written += x_utf8Encode( destination + written, codepoint );
      }
      else
         destination[written++] = source[first];
   }

   return written;
}

/* Is the code point a letter of the supported alphabets? */
static int s_utf8IsLetter( uint32_t codepoint )
{
   size_t i;

   for( i = 0; i < sizeof( s_letterRanges ) / sizeof( s_letterRanges[0] ); ++i )
   {
      if( codepoint >= s_letterRanges[i][0] && codepoint <= s_letterRanges[i][1] )
         return 1;
   }

   return 0;
}

/* IsAlpha(), IsAlNum(), IsUpper() and IsLower() test the first character of a text */
int x_utf8IsAlpha( const char *text, size_t length )
{
   size_t offset = 0;
   uint32_t codepoint;

   return x_utf8Decode( text, length, &offset, &codepoint ) && s_utf8IsLetter( codepoint );
}

/* IsAlNum(): a letter or a digit */
int x_utf8IsAlnum( const char *text, size_t length )
{
   size_t offset = 0;
   uint32_t codepoint;

   if( !x_utf8Decode( text, length, &offset, &codepoint ) )
      return 0;

   return s_utf8IsLetter( codepoint ) || ( codepoint >= '0' && codepoint <= '9' );
}

/* IsUpper() */
int x_utf8IsUpper( const char *text, size_t length )
{
   size_t offset = 0;
   uint32_t codepoint;

   return x_utf8Decode( text, length, &offset, &codepoint ) && x_utf8LowerChar( codepoint ) != codepoint;
}

/* IsLower() */
int x_utf8IsLower( const char *text, size_t length )
{
   size_t offset = 0;
   uint32_t codepoint;

   if( !x_utf8Decode( text, length, &offset, &codepoint ) )
      return 0;

   /* Sharp s has no single uppercase letter */
   return x_utf8UpperChar( codepoint ) != codepoint || codepoint == 0x00DF;
}

/* AChoice(): does the text start with the character of the key, in either case */
int x_inkeyStartsWith( int key, const char *text, size_t length )
{
   char keyText[4];
   size_t keyLength = x_inkeyKeyText( key, keyText );
   size_t offset = 0;
   uint32_t keyCodepoint, codepoint;

   if( keyLength == 0 || !x_utf8Decode( keyText, keyLength, &offset, &keyCodepoint ) )
      return 0;

   offset = 0;
   if( !x_utf8Decode( text, length, &offset, &codepoint ) )
      return 0;

   return x_utf8UpperChar( codepoint ) == x_utf8UpperChar( keyCodepoint );
}

/* ---------------------------------------------------------------------------------------------
   Hex and Base64
   --------------------------------------------------------------------------------------------- */

/* The value of a hex digit; -1 for another character */
static int s_hexValue( char c )
{
   if( c >= '0' && c <= '9' )
      return c - '0';

   if( c >= 'A' && c <= 'F' )
      return c - 'A' + 10;

   if( c >= 'a' && c <= 'f' )
      return c - 'a' + 10;

   return -1;
}

/* The value of a Base64 character, also of the URL alphabet; -1 for another character */
static int s_base64Value( char c )
{
   if( c >= 'A' && c <= 'Z' )
      return c - 'A';

   if( c >= 'a' && c <= 'z' )
      return c - 'a' + 26;

   if( c >= '0' && c <= '9' )
      return c - '0' + 52;

   if( c == '+' || c == '-' )
      return 62;

   if( c == '/' || c == '_' )
      return 63;

   return -1;
}

/* Upper case hex digits of an unsigned 64-bit value, with leading zeros up to width; a longer value is not cut */
char *x_numToHex( uint64_t value, int width, size_t *resultLength )
{
   static const char digits[] = "0123456789ABCDEF";
   char buffer[16];
   size_t count = 0, size, i;
   char *result;

   do
   {
      buffer[count++] = digits[value & 0xF];
      value >>= 4;
   }
   while( value != 0 );
   size = IIF( width > 0 && ( size_t )width > count, ( size_t )width, count );
   result = ( char * )x_xgrab( size + 1 );
   for( i = 0; i < size; ++i )
      result[i] = IIF( i < size - count, '0', buffer[size - 1 - i] );

   result[size] = '\0';
   *resultLength = size;

   return result;
}

/* ---------------------------------------------------------------------------------------------
   Values as text: CStr(), ValToPrgExp() and XToC()
   --------------------------------------------------------------------------------------------- */

/* Append bytes */
static void s_textAdd( S_TEXT *text, const char *data, size_t length )
{
   if( text->length + length + 1 > text->size )
   {
      text->size = ( text->length + length + 1 ) * 2;
      text->data = ( char * )x_xrealloc( text->data, text->size );
   }

   memcpy( text->data + text->length, data, length );
   text->length += length;
   text->data[text->length] = '\0';
}

/* Append a C string */
static void s_textAddC( S_TEXT *text, const char *data )
{
   s_textAdd( text, data, strlen( data ) );
}

/* The built text and its length */
static char *s_textEnd( S_TEXT *text, size_t *length )
{
   if( text->data == NULL )
      s_textAdd( text, "", 0 );

   *length = text->length;

   return text->data;
}

/* A number as Str() writes it; width 10 is Str() without a length, 0 is the bare number */
static void s_textNumber( S_TEXT *text, const X_ITEM *item, int width )
{
   char buffer[64];
   int size;

   if( X_IS_UINT64( item ) )
      size = snprintf( buffer, sizeof( buffer ), "%*llu", width, ( unsigned long long )item->u.unsignedInteger );
   else if( X_IS_INTEGER( item ) )
      size = snprintf( buffer, sizeof( buffer ), "%*lld", width, ( long long )item->u.integer );
   else
      size = snprintf( buffer, sizeof( buffer ), "%*.15g", width, item->u.dbl );

   s_textAdd( text, buffer, ( size_t )size );
}

/* A date as DToS() writes it: YYYYMMDD, 8 spaces for an empty date */
static void s_textDate( S_TEXT *text, const X_ITEM *item )
{
   char buffer[16];
   int year, month, day;

   if( item->u.integer == 0 )
   {
      s_textAdd( text, "        ", 8 );
      return;
   }

   x_dateDecode( ( long )item->u.integer, &year, &month, &day );
   snprintf( buffer, sizeof( buffer ), "%04d%02d%02d", year, month, day );
   s_textAddC( text, buffer );
}

/* Arrays, hashes, objects, blocks and pointers as CStr() describes them */
static void s_textLabel( S_TEXT *text, const X_ITEM *item )
{
   char buffer[64];
   char *hex;
   size_t length;

   if( X_IS_HASH( item ) )
   {
      snprintf( buffer, sizeof( buffer ), "{ Hash of %zu Items }", x_hashLen( item ) );
      s_textAddC( text, buffer );
   }
   else if( X_IS_OBJECT( item ) )
   {
      s_textAddC( text, "{ " );
      s_textAddC( text, x_objGetClsName( item ) );
      s_textAddC( text, " Object }" );
   }
   else if( X_IS_ARRAY( item ) )
   {
      snprintf( buffer, sizeof( buffer ), "{ Array of %lld Items }", ( long long )x_arrayLen( item ) );
      s_textAddC( text, buffer );
   }
   else if( X_IS_BLOCK( item ) )
      s_textAddC( text, "{|| Block }" );
   else
   {
      hex = x_numToHex( ( uint64_t )( uintptr_t )item->u.pPointer, 0, &length );
      s_textAdd( text, hex, length );
      x_xfree( hex );
   }
}

/* CStr(): any value as a text for the screen or a log */
char *x_valToCStr( const X_ITEM *item, size_t *length )
{
   S_TEXT text = { NULL, 0, 0 };

   if( X_IS_NIL( item ) )
      s_textAddC( &text, "NIL" );
   else if( X_IS_STRING( item ) )
      s_textAdd( &text, item->u.string.data, item->u.string.len );
   else if( X_IS_NUMERIC( item ) )
      s_textNumber( &text, item, 10 );
   else if( X_IS_DATE( item ) )
      s_textDate( &text, item );
   else if( X_IS_LOGICAL( item ) )
      s_textAddC( &text, IIF( item->u.logical, ".T.", ".F." ) );
   else
      s_textLabel( &text, item );

   return s_textEnd( &text, length );
}

/* A string literal: printable parts in quotes, control characters as Chr( n ), joined with + */
static void s_textString( S_TEXT *text, const char *data, size_t length )
{
   size_t i = 0, start;
   char buffer[16];

   if( length == 0 )
      s_textAddC( text, "\"\"" );

   while( i < length )
   {
      if( i > 0 )
         s_textAddC( text, " + " );

      if( ( unsigned char )data[i] < 32 || data[i] == 127 )
      {
         snprintf( buffer, sizeof( buffer ), "Chr( %d )", ( unsigned char )data[i++] );
         s_textAddC( text, buffer );
         continue;
      }

      start = i;
      while( i < length && ( unsigned char )data[i] >= 32 && data[i] != 127 )
         ++i;

      if( !memchr( data + start, '"', i - start ) )
      {
         s_textAddC( text, "\"" );
         s_textAdd( text, data + start, i - start );
         s_textAddC( text, "\"" );
      }
      else if( !memchr( data + start, '\'', i - start ) )
      {
         s_textAddC( text, "'" );
         s_textAdd( text, data + start, i - start );
         s_textAddC( text, "'" );
      }
      else
      {
         s_textAddC( text, "e\"" );
         for( ; start < i; ++start )
         {
            if( data[start] == '"' || data[start] == '\\' )
               s_textAddC( text, "\\" );

            s_textAdd( text, data + start, 1 );
         }

         s_textAddC( text, "\"" );
      }
   }
}

/* Write a value as an expression */
static void s_textPrgExp( S_TEXT *text, const X_ITEM *item, const S_TEXT_PARENT *parent )
{
   S_TEXT_PARENT self = { NULL, parent };
   const S_TEXT_PARENT *walk;
   size_t i, count;

   if( X_IS_NIL( item ) )
      s_textAddC( text, "NIL" );
   else if( X_IS_STRING( item ) )
      s_textString( text, item->u.string.data, item->u.string.len );
   else if( X_IS_NUMERIC( item ) )
      s_textNumber( text, item, 0 );
   else if( X_IS_DATE( item ) )
   {
      s_textAddC( text, "SToD( \"" );
      if( item->u.integer != 0 )
         s_textDate( text, item );

      s_textAddC( text, "\" )" );
   }
   else if( X_IS_LOGICAL( item ) )
      s_textAddC( text, IIF( item->u.logical, ".T.", ".F." ) );
   else if( X_IS_POINTER( item ) )
   {
      s_textAddC( text, "0x" );
      s_textLabel( text, item );
   }
   else if( X_IS_OBJECT( item ) || X_IS_BLOCK( item ) )
      s_textLabel( text, item );
   else
   {
      /* An array or a hash inside itself cannot be written as an expression */
      for( walk = parent; walk != NULL; walk = walk->parent )
      {
         if( walk->container == item->u.pArr )
         {
            s_textAddC( text, "NIL" );
            return;
         }
      }

      self.container = item->u.pArr;
      if( X_IS_HASH( item ) )
      {
         count = x_hashLen( item );
         s_textAddC( text, IIF( count, "{ ", "{ =>" ) );
         for( i = 1; i <= count; ++i )
         {
            s_textAddC( text, IIF( i > 1, ", ", "" ) );
            s_textPrgExp( text, x_hashKeyAt( item, i ), &self );
            s_textAddC( text, " => " );
            s_textPrgExp( text, x_hashValueAt( item, i ), &self );
         }

         s_textAddC( text, " }" );
      }
      else
      {
         count = x_arrayLen( item );
         s_textAddC( text, IIF( count, "{ ", "{" ) );
         for( i = 1; i <= count; ++i )
         {
            s_textAddC( text, IIF( i > 1, ", ", "" ) );
            s_textPrgExp( text, x_arrayGetItemPtr( item, i ), &self );
         }

         s_textAddC( text, IIF( count, " }", "}" ) );
      }
   }
}

/* ValToPrgExp(): a value as an expression that gives the value again; not for objects and blocks */
char *x_valToPrgExp( const X_ITEM *item, size_t *length )
{
   S_TEXT text = { NULL, 0, 0 };

   s_textPrgExp( &text, item, NULL );

   return s_textEnd( &text, length );
}

/* Upper case hex digits of every byte, with the separator between bytes */
char *x_hexEncode( const char *data, size_t length, const char *separator, size_t separatorLength,
                   size_t *resultLength )
{
   static const char digits[] = "0123456789ABCDEF";
   size_t size = length * 2 + IIF( length > 0, ( length - 1 ) * separatorLength, 0 );
   char *result = ( char * )x_xgrab( size + 1 );
   char *out = result;
   size_t i;

   for( i = 0; i < length; i++ )
   {
      if( i > 0 && separatorLength > 0 )
      {
         memcpy( out, separator, separatorLength );
         out += separatorLength;
      }

      *out++ = digits[( unsigned char )data[i] >> 4];
      *out++ = digits[( unsigned char )data[i] & 0x0F];
   }

   *out = '\0';
   *resultLength = size;

   return result;
}

/* Bytes from pairs of hex digits; characters that are not hex digits are skipped */
char *x_hexDecode( const char *text, size_t length, size_t *resultLength )
{
   char *result = ( char * )x_xgrab( length / 2 + 1 );
   size_t count = 0;
   int high = -1;
   size_t i;

   for( i = 0; i < length; i++ )
   {
      int value = s_hexValue( text[i] );

      if( value < 0 )
         continue;

      if( high < 0 )
         high = value;
      else
      {
         result[count++] = ( char )( ( high << 4 ) | value );
         high = -1;
      }
   }

   result[count] = '\0';
   *resultLength = count;

   return result;
}

/* Standard Base64 with padding, in one line */
char *x_base64Encode( const char *data, size_t length, size_t *resultLength )
{
   size_t size = ( length + 2 ) / 3 * 4;
   char *result = ( char * )x_xgrab( size + 1 );
   const unsigned char *in = ( const unsigned char * )data;
   char *out = result;
   size_t i;

   for( i = 0; i + 2 < length; i += 3 )
   {
      *out++ = s_base64[in[i] >> 2];
      *out++ = s_base64[( ( in[i] & 0x03 ) << 4 ) | ( in[i + 1] >> 4 )];
      *out++ = s_base64[( ( in[i + 1] & 0x0F ) << 2 ) | ( in[i + 2] >> 6 )];
      *out++ = s_base64[in[i + 2] & 0x3F];
   }

   if( i < length )
   {
      *out++ = s_base64[in[i] >> 2];
      if( i + 1 < length )
      {
         *out++ = s_base64[( ( in[i] & 0x03 ) << 4 ) | ( in[i + 1] >> 4 )];
         *out++ = s_base64[( in[i + 1] & 0x0F ) << 2];
      }
      else
      {
         *out++ = s_base64[( in[i] & 0x03 ) << 4];
         *out++ = '=';
      }

      *out++ = '=';
   }

   *out = '\0';
   *resultLength = size;

   return result;
}

/* Base64 and its URL variant; line breaks and other characters outside the alphabet are skipped,
   decoding stops at the first '=' */
char *x_base64Decode( const char *text, size_t length, size_t *resultLength )
{
   char *result = ( char * )x_xgrab( length / 4 * 3 + 3 );
   size_t count = 0;
   uint32_t bits = 0;
   int filled = 0;
   size_t i;

   for( i = 0; i < length && text[i] != '='; i++ )
   {
      int value = s_base64Value( text[i] );

      if( value < 0 )
         continue;

      bits = ( bits << 6 ) | ( uint32_t )value;
      filled += 6;

      if( filled >= 8 )
      {
         filled -= 8;
         result[count++] = ( char )( ( bits >> filled ) & 0xFF );
      }
   }

   result[count] = '\0';
   *resultLength = count;

   return result;
}

/* XToC(): a string, logical, number or date as a string; a number as its 8 bytes, a date as yyyymmdd */
void x_textXToC( X_CALL_CTX *ctx )
{
   if( X_ISCHAR( ctx, 1 ) )
      x_retclen( ctx, x_parc( ctx, 1 ), x_parclen( ctx, 1 ) );
   else if( X_ISLOG( ctx, 1 ) )
      x_retc( ctx, IIF( x_parl( ctx, 1 ), "T", "F" ) );
   else if( X_ISNUM( ctx, 1 ) )
      x_binRetFloat64( ctx, x_parnd( ctx, 1 ) );
   else if( x_pardl( ctx, 1 ) == 0 )
      x_retclen( ctx, "        ", 8 );
   else
   {
      int year, month, day;
      char buffer[9];

      x_dateDecode( x_pardl( ctx, 1 ), &year, &month, &day );
      snprintf( buffer, sizeof( buffer ), "%04d%02d%02d", year, month, day );
      x_retclen( ctx, buffer, 8 );
   }
}

/* ---------------------------------------------------------------------------------------------
   MD5 (RFC 1321)
   --------------------------------------------------------------------------------------------- */

/* One block of 64 bytes */
static void s_md5Block( uint32_t state[4], const unsigned char block[64] )
{
   uint32_t words[16];
   uint32_t a = state[0];
   uint32_t b = state[1];
   uint32_t c = state[2];
   uint32_t d = state[3];
   int i;

   /* The block as 16 little endian words */
   for( i = 0; i < 16; ++i )
   {
      const unsigned char *bytes = block + i * 4;

      words[i] = ( uint32_t )bytes[0] | ( ( uint32_t )bytes[1] << 8 );
      words[i] |= ( ( uint32_t )bytes[2] << 16 ) | ( ( uint32_t )bytes[3] << 24 );
   }

   for( i = 0; i < 64; ++i )
   {
      uint32_t f;
      int g;

      if( i < 16 )
      {
         f = ( b & c ) | ( ~b & d );
         g = i;
      }
      else if( i < 32 )
      {
         f = ( d & b ) | ( ~d & c );
         g = ( 5 * i + 1 ) % 16;
      }
      else if( i < 48 )
      {
         f = b ^ c ^ d;
         g = ( 3 * i + 5 ) % 16;
      }
      else
      {
         f = c ^ ( b | ~d );
         g = ( 7 * i ) % 16;
      }

      f += a + s_md5Sine[i] + words[g];
      a = d;
      d = c;
      c = b;
      b += ( f << s_md5Shift[i] ) | ( f >> ( 32 - s_md5Shift[i] ) );
   }

   state[0] += a;
   state[1] += b;
   state[2] += c;
   state[3] += d;
}

/* Start a digest */
void x_md5Init( X_MD5 *md5 )
{
   md5->state[0] = 0x67452301;
   md5->state[1] = 0xEFCDAB89;
   md5->state[2] = 0x98BADCFE;
   md5->state[3] = 0x10325476;
   md5->length = 0;
}

/* Add data to a digest */
void x_md5Update( X_MD5 *md5, const void *data, size_t length )
{
   const unsigned char *bytes = ( const unsigned char * )data;
   size_t used = ( size_t )( md5->length % 64 );

   md5->length += length;
   while( length > 0 )
   {
      size_t take = IIF( 64 - used < length, 64 - used, length );

      memcpy( md5->buffer + used, bytes, take );
      used += take;
      bytes += take;
      length -= take;
      if( used == 64 )
      {
         s_md5Block( md5->state, md5->buffer );
         used = 0;
      }
   }
}

/* The digest as 32 lowercase hex digits and a zero byte */
void x_md5Final( X_MD5 *md5, char hex[33] )
{
   static const unsigned char padding[64] = { 0x80 };
   uint64_t bits = md5->length * 8;
   size_t used = ( size_t )( md5->length % 64 );
   unsigned char size[8];
   int i;

   for( i = 0; i < 8; ++i )
      size[i] = ( unsigned char )( bits >> ( 8 * i ) );

   x_md5Update( md5, padding, IIF( used < 56, 56 - used, 120 - used ) );
   x_md5Update( md5, size, 8 );
   for( i = 0; i < 16; ++i )
   {
      unsigned char byte = ( unsigned char )( md5->state[i / 4] >> ( 8 * ( i % 4 ) ) );

      hex[i * 2] = "0123456789abcdef"[byte >> 4];
      hex[i * 2 + 1] = "0123456789abcdef"[byte & 15];
   }

   hex[32] = '\0';
}

/* ---------------------------------------------------------------------------------------------
   Character sort and ranges: CharSort(), RangeRem() and RangeRepl()
   --------------------------------------------------------------------------------------------- */

/* qsort() order of CharSort() */
static int s_charSortCompare( const void *a, const void *b )
{
   const S_CHARSORT_ITEM *first = ( const S_CHARSORT_ITEM * )a;
   const S_CHARSORT_ITEM *second = ( const S_CHARSORT_ITEM * )b;
   int result = 0;
   size_t i;

   /* The characters compare by their code */
   for( i = 0; i < first->compare && result == 0; ++i )
      result = ( first->key[i] > second->key[i] ) - ( first->key[i] < second->key[i] );

   if( first->descending )
      result = -result;

   return IIF( result != 0, result, ( first->index > second->index ) - ( first->index < second->index ) );
}

/* Sort count elements of element characters, which start skip characters into text, into result (a copy of
   text); compare characters from offset inside each element are compared by their code, and equal elements keep
   their order */
void x_charSort( uint32_t *result, const uint32_t *text, size_t skip, size_t count, size_t element, size_t compare,
                 size_t offset, int descending )
{
   S_CHARSORT_ITEM *items;
   size_t i;

   if( count < 2 )
      return;

   items = ( S_CHARSORT_ITEM * )x_xgrabArray( count, sizeof( S_CHARSORT_ITEM ) );
   for( i = 0; i < count; ++i )
   {
      items[i].key = text + skip + i * element + offset;
      items[i].index = i;
      items[i].compare = compare;
      items[i].descending = descending;
   }

   qsort( items, count, sizeof( S_CHARSORT_ITEM ), s_charSortCompare );
   for( i = 0; i < count; ++i )
      memcpy( result + skip + i * element, items[i].key - offset, element * sizeof( uint32_t ) );

   x_xfree( items );
}

/* A range bound of RangeRem() and RangeRepl(): a character code or the first character of a string, 0 for "" */
uint32_t x_rangeBound( X_CALL_CTX *ctx, int n )
{
   return x_ucodeArg( ctx, n, 0 );
}

/* Is the character code c in the range; a range that runs back to front wraps past the last code */
int x_rangeHas( uint32_t first, uint32_t last, uint32_t c )
{
   return IIF( first <= last, c >= first && c <= last, c >= first || c <= last );
}

/* ---------------------------------------------------------------------------------------------
   Character helpers of the API: Char*(), Pad*(), Pos*(), AtNum(), Rem*()/Repl*() and Center()
   --------------------------------------------------------------------------------------------- */

/* The offset of the nth occurrence of search in text, the last one when occurrence < 1, ignoring
   the first ignored bytes; occurrences do not overlap; SIZE_MAX when not found */
size_t x_atNumFind( const char *search, size_t searchLength, const char *text, size_t textLength,
                    int64_t occurrence, int64_t ignored )
{
   size_t offset = ( size_t )IIF( ignored > 0, ignored, 0 );
   size_t found = SIZE_MAX;
   int64_t matched = 0;

   if( searchLength == 0 || offset >= textLength )
      return SIZE_MAX;

   while( offset + searchLength <= textLength )
   {
      if( memcmp( text + offset, search, searchLength ) == 0 )
      {
         found = offset;
         if( ++matched == occurrence )
            return found;

         offset += searchLength;
      }
      else
         ++offset;
   }

   return IIF( occurrence > 0, SIZE_MAX, found );
}

/* CharAdd(), CharSub(), CharAND(), CharOR() and CharXOR(): every byte of the first string combined with
   the byte of the second one at the same place; the second string repeats when it is shorter */
void x_charCombine( X_CALL_CTX *ctx, char operation )
{
   const unsigned char *text = ( const unsigned char * )x_parc( ctx, 1 );
   size_t length = x_parclen( ctx, 1 );
   const unsigned char *other = ( const unsigned char * )x_parc( ctx, 2 );
   size_t otherLength = x_parclen( ctx, 2 );
   char *result;
   size_t i;

   if( otherLength == 0 )
   {
      x_retclen( ctx, ( const char * )text, length );
      return;
   }

   result = ( char * )x_xgrab( length + 1 );
   for( i = 0; i < length; ++i )
   {
      unsigned char a = text[i];
      unsigned char b = other[i % otherLength];

      switch( operation )
      {
      case '+':
         result[i] = ( char )( unsigned char )( a + b );
         break;

      case '&':
         result[i] = ( char )( a & b );
         break;

      case '|':
         result[i] = ( char )( a | b );
         break;

      case '-':
         result[i] = ( char )( unsigned char )( a - b );
         break;

      default:
         result[i] = ( char )( a ^ b );
         break;
      }
   }

   result[length] = '\0';
   x_storclen( ctx, result, length, 1 );
   x_retclen_buffer( ctx, result, length );
}

/* The text without the listed characters, or with only them when keep is set */
void x_charFilter( X_CALL_CTX *ctx, const char *chars, size_t charsLength, const char *text, size_t length,
                   int keep )
{
   size_t listCount, count, i;
   size_t kept = 0;
   uint32_t *list = x_ucodes( chars, charsLength, &listCount );
   uint32_t *codes = x_ucodes( text, length, &count );

   /* The characters are UTF-8 characters, in the list as in the text */
   for( i = 0; i < count; ++i )
   {
      if( x_ucodeListed( list, listCount, codes[i] ) == keep )
         codes[kept++] = codes[i];
   }

   x_xfree( list );
   x_ucodesReturn( ctx, codes, kept );
}

/* CharRLL(), CharRLR(), CharSHL() and CharSHR(): the bits of every byte rotated or shifted by nShift places,
   to the left or to the right; a shift drops the bits that leave the byte and brings in zeros */
void x_charBits( X_CALL_CTX *ctx, int rotate, int left )
{
   const unsigned char *text = ( const unsigned char * )x_parc( ctx, 1 );
   size_t length = x_parclen( ctx, 1 );
   int64_t shift = x_parnint( ctx, 2 );
   char *result = ( char * )x_xgrab( length + 1 );
   size_t i;

   if( shift < 0 )
      shift = 0;

   if( rotate )
      shift %= 8;

   for( i = 0; i < length; ++i )
   {
      unsigned int c = text[i];

      if( rotate && left )
         c = ( c << shift ) | ( c >> ( 8 - shift ) );
      else if( rotate )
         c = ( c >> shift ) | ( c << ( 8 - shift ) );
      else if( shift >= 8 )
         c = 0;
      else if( left )
         c = c << shift;
      else
         c = c >> shift;

      result[i] = ( char )( unsigned char )c;
   }

   result[length] = '\0';
   x_retclen_buffer( ctx, result, length );
}

/* The character argument n as one UTF-8 character: the first character of a string, or the character of a
   code point; a space without it. A number that is no code point stays the byte it was */
void x_ucharArg( X_CALL_CTX *ctx, int n, X_UCHAR *c )
{
   uint32_t codepoint;

   c->bytes[0] = ' ';
   c->size = 1;

   if( X_ISCHAR( ctx, n ) && x_parclen( ctx, n ) > 0 )
   {
      c->size = x_utf8Offset( x_parc( ctx, n ), x_parclen( ctx, n ), 1 );
      memcpy( c->bytes, x_parc( ctx, n ), c->size );
   }
   else if( X_ISNUM( ctx, n ) && s_utf8ScalarNumber( x_parnd( ctx, n ), &codepoint ) )
      c->size = x_utf8Encode( c->bytes, codepoint );
   else if( X_ISNUM( ctx, n ) )
      c->bytes[0] = ( char )x_parni( ctx, n );
}

/* Whether the character c starts at byte pos of text */
int x_ucharAt( const char *text, size_t length, size_t pos, const X_UCHAR *c )
{
   return pos <= length && length - pos >= c->size && memcmp( text + pos, c->bytes, c->size ) == 0;
}

/* How many times the character repeats at the beginning or, with fromRight, at the end of text */
size_t x_ucharRun( const char *text, size_t length, const X_UCHAR *c, int fromRight )
{
   size_t count = 0;
   size_t used = 0;

   while( length - used >= c->size )
   {
      size_t pos = IIF( fromRight, length - used - c->size, used );

      if( !x_ucharAt( text, length, pos, c ) )
         break;

      used += c->size;
      ++count;
   }

   return count;
}

/* Count copies of the character; returns the end of the copies */
char *x_ucharFill( char *destination, const X_UCHAR *c, size_t count )
{
   size_t i;

   for( i = 0; i < count; ++i, destination += c->size )
      memcpy( destination, c->bytes, c->size );

   return destination;
}

/* The characters of a UTF-8 text as codes, in an array of x_xgrab() with room for one code per byte;
   a broken byte becomes X_UCODE_BYTE + the byte, so it comes back unchanged */
uint32_t *x_ucodes( const char *text, size_t length, size_t *count )
{
   uint32_t *codes = ( uint32_t * )x_xgrab( ( length + 1 ) * sizeof( uint32_t ) );
   size_t offset = 0;
   size_t first;
   uint32_t codepoint;

   *count = 0;
   while( offset < length )
   {
      first = offset;
      if( x_utf8Decode( text, length, &offset, &codepoint ) )
         codes[( *count )++] = codepoint;
      else
         codes[( *count )++] = X_UCODE_BYTE + ( unsigned char )text[first];
   }

   return codes;
}

/* The codes as a UTF-8 string of x_xgrab() */
char *x_ucodesString( const uint32_t *codes, size_t count, size_t *length )
{
   char *text = ( char * )x_xgrab( count * 4 + 1 );
   size_t i;

   *length = 0;
   for( i = 0; i < count; ++i )
   {
      if( codes[i] >= X_UCODE_BYTE )
         text[( *length )++] = ( char )( codes[i] - X_UCODE_BYTE );
      else
         *length += x_utf8Encode( text + *length, codes[i] );
   }

   text[*length] = '\0';

   return text;
}

/* Returns the codes as a UTF-8 string and releases them */
void x_ucodesReturn( X_CALL_CTX *ctx, uint32_t *codes, size_t count )
{
   size_t length;
   char *text = x_ucodesString( codes, count, &length );

   x_xfree( codes );
   x_retclen_buffer( ctx, text, length );
}

/* The character argument n as a code: the first character of a string, or a code point; def without it.
   A number that is no code point keeps its low byte */
uint32_t x_ucodeArg( X_CALL_CTX *ctx, int n, uint32_t def )
{
   size_t offset = 0;
   uint32_t codepoint;

   if( X_ISCHAR( ctx, n ) && x_parclen( ctx, n ) > 0 )
   {
      if( x_utf8Decode( x_parc( ctx, n ), x_parclen( ctx, n ), &offset, &codepoint ) )
         return codepoint;

      return X_UCODE_BYTE + ( unsigned char )x_parc( ctx, n )[0];
   }

   if( X_ISNUM( ctx, n ) && s_utf8ScalarNumber( x_parnd( ctx, n ), &codepoint ) )
      return codepoint;

   if( X_ISNUM( ctx, n ) )
      return ( uint32_t )( x_parni( ctx, n ) & 0xFF );

   return def;
}

/* Is the code in the list? */
int x_ucodeListed( const uint32_t *list, size_t count, uint32_t code )
{
   size_t i;

   for( i = 0; i < count; ++i )
   {
      if( list[i] == code )
         return 1;
   }

   return 0;
}

/* The position of the first UTF-8 character after the ignored ones that passes the test, or with negate
   the first one that does not; 0 when there is none. The positions count characters; test gets the text
   from the character on */
int64_t x_posFind( const char *text, size_t length, int64_t ignored, int ( *test )( const char *, size_t ),
                   int negate )
{
   size_t skip = ( size_t )IIF( ignored > 0, ignored, 0 );
   size_t i = x_utf8Offset( text, length, skip );
   size_t position = skip;
   uint32_t codepoint;

   while( i < length )
   {
      ++position;
      if( ( test( text + i, length - i ) != 0 ) != negate )
         return ( int64_t )position;

      x_utf8Decode( text, length, &i, &codepoint );
   }

   return 0;
}

/* RemAll(), RemLeft() and RemRight(): the string without the character repeated on the chosen sides */
void x_charRemove( X_CALL_CTX *ctx, int left, int right )
{
   const char *text = x_parc( ctx, 1 );
   size_t length = x_parclen( ctx, 1 );
   size_t start, end;
   X_UCHAR c;

   /* c may be any UTF-8 character */
   x_ucharArg( ctx, 2, &c );
   start = IIF( left, x_ucharRun( text, length, &c, 0 ) * c.size, 0 );
   end = length - IIF( right, x_ucharRun( text + start, length - start, &c, 1 ) * c.size, 0 );
   x_retclen( ctx, text + start, end - start );
}

/* ReplAll(), ReplLeft() and ReplRight(): the character repeated on the chosen sides replaced */
void x_charReplaceSides( X_CALL_CTX *ctx, int left, int right )
{
   const char *text = x_parc( ctx, 1 );
   size_t length = x_parclen( ctx, 1 );
   size_t before = 0;
   size_t after = 0;
   size_t start, end;
   X_UCHAR replace, search;
   char *result, *cursor;

   /* Every search character on the chosen sides becomes one replace character; both may be any UTF-8 character */
   x_ucharArg( ctx, 2, &replace );
   x_ucharArg( ctx, 3, &search );
   if( left )
      before = x_ucharRun( text, length, &search, 0 );

   start = before * search.size;
   if( right )
      after = x_ucharRun( text + start, length - start, &search, 1 );

   end = length - after * search.size;
   result = ( char * )x_xgrab( ( before + after ) * replace.size + ( end - start ) + 1 );
   cursor = x_ucharFill( result, &replace, before );
   memcpy( cursor, text + start, end - start );
   cursor = x_ucharFill( cursor + end - start, &replace, after );
   *cursor = '\0';
   x_retclen_buffer( ctx, result, ( size_t )( cursor - result ) );
}

/* Center(): the trimmed string padded on the left, and with lBothSides on the right too */
void x_textCenter( X_CALL_CTX *ctx )
{
   /* A logical third argument is already lBothSides: Center( cString, 10, .T. ) */
   int both = IIF( X_ISLOG( ctx, 3 ), x_parl( ctx, 3 ), x_parl( ctx, 4 ) );
   const char *text = x_parc( ctx, 1 );
   size_t first = 0, last = x_parclen( ctx, 1 ), length, chars, left, right;
   int64_t width;
   X_UCHAR pad;
   char *result, *cursor;

   x_ucharArg( ctx, 3, &pad );

   if( X_ISNUM( ctx, 2 ) )
      width = x_parnint( ctx, 2 );
   else
   {
      /* Without nLength: the rest of the screen line from the cursor */
      unsigned short row = 0, col = 0;

      X_GTCHECK( ctx, "CENTER" );
      x_gtGetPos( &row, &col );
      width = x_gtMaxCol() + 1 - col;
   }

   /* Leading and trailing spaces and pad characters are replaced by the new padding */
   while( first < last )
   {
      if( text[first] == ' ' )
         ++first;
      else if( x_ucharAt( text, last, first, &pad ) )
         first += pad.size;
      else
         break;
   }
   while( last > first )
   {
      if( text[last - 1] == ' ' )
         --last;
      else if( last - first >= pad.size && x_ucharAt( text, last, last - pad.size, &pad ) )
         last -= pad.size;
      else
         break;
   }

   length = last - first;
   chars = x_utf8Length( text + first, length );
   if( width <= ( int64_t )chars )
   {
      x_retclen( ctx, text + first, length );
      return;
   }

   /* nLength counts characters; an odd padding puts the extra character at the beginning */
   left = ( size_t )width - chars - ( ( size_t )width - chars ) / 2;
   right = IIF( both, ( ( size_t )width - chars ) / 2, 0 );
   result = ( char * )x_xgrab( ( left + right ) * pad.size + length + 1 );
   cursor = x_ucharFill( result, &pad, left );
   memcpy( cursor, text + first, length );
   cursor = x_ucharFill( cursor + length, &pad, right );
   *cursor = '\0';
   x_retclen_buffer( ctx, result, ( size_t )( cursor - result ) );
}

/* x_atNumFind() on character codes: the index of the nth occurrence, the last one when occurrence < 1, ignoring
   the first ignored characters; occurrences do not overlap; SIZE_MAX when not found */
static size_t s_ucodesFind( const uint32_t *search, size_t searchLength, const uint32_t *text, size_t textLength,
                            int64_t occurrence, int64_t ignored )
{
   size_t offset = ( size_t )IIF( ignored > 0, ignored, 0 );
   size_t found = SIZE_MAX;
   int64_t matched = 0;

   if( searchLength == 0 || offset >= textLength )
      return SIZE_MAX;

   while( offset + searchLength <= textLength )
   {
      if( memcmp( text + offset, search, searchLength * sizeof( uint32_t ) ) == 0 )
      {
         found = offset;
         if( ++matched == occurrence )
            return found;

         offset += searchLength;
      }
      else
         ++offset;
   }

   return IIF( occurrence > 0, SIZE_MAX, found );
}

/* AtAdjust(): nEndPos, nSkipChars and the moved distance count UTF-8 characters, and the fill character may be
   any of them, so the sequence lines up in a column of the screen */
void x_atAdjust( X_CALL_CTX *ctx )
{
   const char *text = x_parc( ctx, 2 );
   size_t bytes = x_parclen( ctx, 2 );
   int64_t position = x_parnint( ctx, 3 );
   uint32_t fill = x_ucodeArg( ctx, 6, ' ' );
   int64_t count = x_parnintdef( ctx, 4, 0 );
   int64_t skip = x_parnintdef( ctx, 5, 0 );
   size_t searchLength, length, target, change, size, i;
   uint32_t *search = x_ucodes( x_parc( ctx, 1 ), x_parclen( ctx, 1 ), &searchLength );
   uint32_t *codes = x_ucodes( text, bytes, &length );
   size_t found = s_ucodesFind( search, searchLength, codes, length, count, skip );
   uint32_t *result;

   x_xfree( search );
   if( found == SIZE_MAX || position < 1 )
   {
      x_xfree( codes );
      x_retclen( ctx, text, bytes );
      return;
   }

   target = ( size_t )( position - 1 );
   if( target >= found )
   {
      change = target - found;
      if( change > SIZE_MAX / sizeof( uint32_t ) - length - 1 )
      {
         x_xfree( codes );
         x_errorRaiseNamed( ctx->result, 1102, "ATADJUST" );
         return;
      }

      size = length + change;
      result = ( uint32_t * )x_xgrab( ( size + 1 ) * sizeof( uint32_t ) );
      memcpy( result, codes, found * sizeof( uint32_t ) );
      for( i = found; i < target; ++i )
         result[i] = fill;

      memcpy( result + target, codes + found, ( length - found ) * sizeof( uint32_t ) );
   }
   else
   {
      change = found - target;
      for( i = target; i < found; ++i )
      {
         if( codes[i] != fill )
         {
            x_xfree( codes );
            x_retclen( ctx, text, bytes );
            return;
         }
      }

      size = length - change;
      result = ( uint32_t * )x_xgrab( ( size + 1 ) * sizeof( uint32_t ) );
      memcpy( result, codes, target * sizeof( uint32_t ) );
      memcpy( result + target, codes + found, ( length - found ) * sizeof( uint32_t ) );
   }

   x_xfree( codes );
   x_ucodesReturn( ctx, result, size );
}

/* AtRepl(): the occurrences of search replaced, up to requested of them, or only the requested one */
void x_atReplace( X_CALL_CTX *ctx )
{
   const char *search = x_parc( ctx, 1 );
   const char *text = x_parc( ctx, 2 );
   const char *replacement = x_parc( ctx, 3 );
   size_t searchLength = x_parclen( ctx, 1 );
   size_t length = x_parclen( ctx, 2 );
   size_t replacementLength = x_parclen( ctx, 3 );
   int64_t requested = x_parnintdef( ctx, 4, 0 );
   int only = x_parl( ctx, 5 );
   size_t offset = 0;
   size_t first = 0;
   size_t count = 0;
   size_t copied = 0;
   size_t written = 0;
   size_t found, size, i;
   char *result;

   if( searchLength == 0 )
   {
      x_retclen( ctx, text, length );
      return;
   }

   if( only )
   {
      first = x_atNumFind( search, searchLength, text, length, requested, 0 );
      count = IIF( first == SIZE_MAX, 0, 1 );
   }
   else
   {
      while( offset < length )
      {
         found = x_strAt( search, searchLength, text + offset, length - offset );
         if( found == 0 )
            break;

         ++count;
         offset += found - 1 + searchLength;
         if( requested > 0 && count == ( uint64_t )requested )
            break;
      }
   }

   if( count == 0 )
   {
      x_retclen( ctx, text, length );
      return;
   }

   if( replacementLength > searchLength )
   {
      if( count > ( SIZE_MAX - length - 1 ) / ( replacementLength - searchLength ) )
      {
         x_errorRaiseNamed( ctx->result, 1102, "ATREPL" );
         return;
      }

      size = length + count * ( replacementLength - searchLength );
   }
   else
      size = length - count * ( searchLength - replacementLength );

   result = ( char * )x_xgrab( size + 1 );
   for( i = 0; i < count; ++i )
   {
      if( only )
         found = first;
      else
         found = copied + x_strAt( search, searchLength, text + copied, length - copied ) - 1;

      memcpy( result + written, text + copied, found - copied );
      written += found - copied;
      memcpy( result + written, replacement, replacementLength );
      written += replacementLength;
      copied = found + searchLength;
   }

   memcpy( result + written, text + copied, length - copied );
   result[size] = '\0';
   if( searchLength == replacementLength )
      x_storclen( ctx, result, size, 2 );

   x_retclen_buffer( ctx, result, size );
}

/* A 16-bit value as 2 bytes, low byte first */
void x_binRetUInt16( X_CALL_CTX *ctx, uint16_t value )
{
   char bytes[2];

   bytes[0] = ( char )( value & 0xFF );
   bytes[1] = ( char )( value >> 8 );
   x_retclen( ctx, bytes, sizeof( bytes ) );
}

/* The hex digits at the start of text as a number; 0 when it does not fit into 64 bits */
static int s_hexNumber( const char *text, size_t length, uint64_t *result )
{
   uint64_t value = 0;
   size_t i;
   int digit;

   for( i = 0; i < length; ++i )
   {
      digit = s_hexValue( text[i] );
      if( digit < 0 )
         break;

      if( value > ( UINT64_MAX - ( unsigned int )digit ) / 16 )
         return 0;

      value = value * 16 + ( unsigned int )digit;
   }

   *result = value;

   return 1;
}

/* HexToNum(): 16 hex digits wrap into a negative INT64 */
void x_hexToNum( X_CALL_CTX *ctx )
{
   uint64_t value;

   if( !s_hexNumber( x_parc( ctx, 1 ), x_parclen( ctx, 1 ), &value ) )
   {
      x_errorRaiseNamed( ctx->result, 3106, "HEXTONUM" );
      return;
   }

   if( value <= INT64_MAX )
      x_retnint( ctx, ( int64_t )value );
   else
      x_retnint( ctx, -1 - ( int64_t )( UINT64_MAX - value ) );
}

/* AtSkipStrings(): the position (from 1) of search outside of quoted strings and [ ]; 0 when not found */
size_t x_atSkipStrings( const char *search, size_t searchLength, const char *text, size_t length )
{
   size_t position = 0;
   size_t start, found;
   char close;

   if( searchLength == 0 )
      return 0;

   while( position < length )
   {
      char c = text[position];

      if( c == '\'' || c == '"' || c == '[' )
      {
         close = IIF( c == '[', ']', c );
         ++position;
         while( position < length && text[position] != close )
            ++position;

         if( position < length )
            ++position;
      }
      else
      {
         start = position;
         while( position < length && text[position] != '\'' && text[position] != '"' && text[position] != '[' )
            ++position;

         found = x_strAt( search, searchLength, text + start, position - start );
         if( found != 0 )
            return start + found;
      }
   }

   return 0;
}

/* WildMatch(): * matches any bytes, ? one byte; without full the pattern may match the start of text */
int x_wildMatch( const char *pattern, size_t patternLength, const char *text, size_t length, int full )
{
   size_t position = 0;
   size_t current = 0;
   size_t star = SIZE_MAX;
   size_t retry = 0;

   for( ;; )
   {
      int more = position < patternLength && current < length;

      if( position == patternLength && ( !full || current == length ) )
         return 1;

      if( position < patternLength && pattern[position] == '*' )
      {
         star = ++position;
         retry = current;
      }
      else if( more && ( pattern[position] == '?' || pattern[position] == text[current] ) )
      {
         ++position;
         ++current;
      }
      else if( star != SIZE_MAX && retry < length )
      {
         position = star;
         current = ++retry;
      }
      else
         return 0;
   }
}

/* A number argument of the bit and byte functions, or hex digits after leading spaces; 0 when it is neither */
static int s_ctNumber( X_CALL_CTX *ctx, int argument, int64_t *value )
{
   const char *text;
   size_t length, i = 0;
   uint64_t hex;

   if( X_ISNUM( ctx, argument ) )
   {
      *value = x_parnint( ctx, argument );
      return 1;
   }

   if( !X_ISCHAR( ctx, argument ) )
      return 0;

   text = x_parc( ctx, argument );
   length = x_parclen( ctx, argument );
   while( i < length && text[i] == ' ' )
      ++i;

   if( i == length || s_hexValue( text[i] ) < 0 || !s_hexNumber( text + i, length - i, &hex ) )
      return 0;

   *value = ( int64_t )hex;

   return 1;
}

/* SetBit() and ClearBit(): the bits at the positions 1 to 32 set or cleared; -1 for an invalid argument */
void x_numBit( X_CALL_CTX *ctx, int set )
{
   int64_t value, position;
   int i;

   if( !s_ctNumber( ctx, 1, &value ) )
   {
      x_retnint( ctx, -1 );
      return;
   }

   for( i = 2; i <= x_pcount( ctx ); ++i )
   {
      position = x_parnint( ctx, i );
      if( !X_ISNUM( ctx, i ) || position < 1 || position > 32 )
      {
         x_retnint( ctx, -1 );
         return;
      }

      if( set )
         value |= ( int64_t )1 << ( position - 1 );
      else
         value &= ~( ( int64_t )1 << ( position - 1 ) );
   }

   x_retnint( ctx, value );
}

/* NumHigh() and NumLow(): the high or low byte of a 16-bit value; -1 for an invalid argument */
void x_numByte( X_CALL_CTX *ctx, int high )
{
   int64_t value;

   if( s_ctNumber( ctx, 1, &value ) && value >= 0 && value <= 0xFFFF )
      x_retni( ctx, ( int )( ( value >> IIF( high, 8, 0 ) ) & 0xFF ) );
   else
      x_retni( ctx, -1 );
}

/* IsBit(): one bit of a signed or unsigned 32-bit value; nBitPos 1 is the lowest bit, .F. for an invalid argument */
void x_numIsBit( X_CALL_CTX *ctx )
{
   int64_t position = x_parnintdef( ctx, 2, 1 );
   int64_t value = 0;
   int valid = s_ctNumber( ctx, 1, &value ) && value >= INT32_MIN && value <= UINT32_MAX;

   if( !X_ISNIL( ctx, 2 ) && !X_ISNUM( ctx, 2 ) )
      valid = 0;

   x_retl( ctx, valid && position >= 1 && position <= 32 && ( ( value >> ( position - 1 ) ) & 1 ) );
}

/* NumAnd(), NumOr() and NumXor(): all arguments joined bit by bit; -1 for an invalid argument */
void x_numLogic( X_CALL_CTX *ctx, char operation )
{
   int64_t result, value;
   int i;

   if( !s_ctNumber( ctx, 1, &result ) )
   {
      x_retnint( ctx, -1 );
      return;
   }

   for( i = 2; i <= x_pcount( ctx ); ++i )
   {
      if( !s_ctNumber( ctx, i, &value ) )
      {
         x_retnint( ctx, -1 );
         return;
      }

      if( operation == '&' )
         result &= value;
      else if( operation == '|' )
         result |= value;
      else
         result ^= value;
   }

   x_retnint( ctx, result );
}

/* NumMirr() and NumMirrX(): the low bits of a value in reverse order; -1 for an invalid argument */
void x_numMirror( X_CALL_CTX *ctx, int argument, int bits, int64_t maximum )
{
   int64_t value;

   if( s_ctNumber( ctx, argument, &value ) && value >= 0 && value <= maximum && bits >= 1 && bits <= 32 )
      x_retnint( ctx, ( int64_t )x_mathMirror( ( uint64_t )value, bits ) );
   else
      x_retnint( ctx, -1 );
}

/* NumNot(): the complement of a 16-bit value; -1 for an invalid argument */
void x_numNot( X_CALL_CTX *ctx )
{
   int64_t value;

   if( s_ctNumber( ctx, 1, &value ) )
      x_retnint( ctx, ~value & 0xFFFF );
   else
      x_retnint( ctx, -1 );
}

/* NumRol(): a 16-bit value, or only its low byte, rotated to the left; -1 for an invalid argument */
void x_numRol( X_CALL_CTX *ctx )
{
   int bits = IIF( x_parl( ctx, 3 ), 8, 16 );
   int64_t mask = ( ( int64_t )1 << bits ) - 1;
   int64_t value, shift, part;
   int valid = s_ctNumber( ctx, 1, &value ) && s_ctNumber( ctx, 2, &shift );

   if( !valid || value < 0 || value > 0xFFFF || shift < 0 || shift > 0xFFFF )
   {
      x_retnint( ctx, -1 );
      return;
   }

   shift %= bits;
   part = value & mask;
   part = ( ( part << shift ) | ( part >> ( bits - shift ) ) ) & mask;

   x_retnint( ctx, ( value & ~mask & 0xFFFF ) | part );
}

/* IntNeg(): an unsigned 16-bit or 32-bit value as a signed one; 0 for an invalid argument or one out of range */
void x_numSigned( X_CALL_CTX *ctx )
{
   int bit32 = x_parl( ctx, 2 );
   int64_t maximum = IIF( bit32, ( int64_t )UINT32_MAX, 0xFFFF );
   int64_t value;

   if( !s_ctNumber( ctx, 1, &value ) || value < 0 || value > maximum )
      x_retnint( ctx, 0 );
   else if( bit32 )
      x_retnint( ctx, ( int32_t )( uint32_t )value );
   else
      x_retnint( ctx, ( int16_t )( uint16_t )value );
}

/* IntPos(): a signed 16-bit or 32-bit value as an unsigned one; 0 for an invalid argument */
void x_numUnsigned( X_CALL_CTX *ctx )
{
   int64_t value;

   if( !s_ctNumber( ctx, 1, &value ) )
      x_retnint( ctx, 0 );
   else if( x_parl( ctx, 2 ) )
      x_retnint( ctx, ( uint32_t )value );
   else
      x_retnint( ctx, ( uint16_t )value );
}

/* StrPeek() reads and StrPoke() writes one byte of a string; a position outside does nothing */
void x_stringByte( X_CALL_CTX *ctx, int write )
{
   const char *text = x_parc( ctx, 1 );
   size_t length = x_parclen( ctx, 1 );
   double requested = x_parnd( ctx, 2 );
   size_t position;
   char *result;

   if( !( requested >= 1 && requested < ( double )SIZE_MAX ) || requested >= ( double )length + 1 )
   {
      if( write )
         x_retclen( ctx, text, length );
      else
         x_retni( ctx, 0 );

      return;
   }

   position = ( size_t )requested - 1;
   if( !write )
   {
      x_retni( ctx, ( unsigned char )text[position] );
      return;
   }

   result = ( char * )x_xmemdup( text, length + 1 );
   result[position] = ( char )x_parni( ctx, 3 );
   x_storclen( ctx, result, length, 1 );
   x_retclen_buffer( ctx, result, length );
}

/* Checksum(): CRC-32, or the sum of the bytes in the low 24 bits and the length in the high byte */
uint32_t x_textCheckSum( const char *text, size_t length, int crc )
{
   uint32_t value = IIF( crc, UINT32_MAX, 0 );
   size_t i;
   int bit;

   for( i = 0; i < length; ++i )
   {
      if( crc )
      {
         value ^= ( unsigned char )text[i];
         for( bit = 0; bit < 8; ++bit )
            value = ( value >> 1 ) ^ IIF( value & 1, UINT32_C( 0xEDB88320 ), 0 );
      }
      else
         value += ( unsigned char )text[i] * IIF( i == 0, 1U, 257U );
   }

   if( crc )
      return ~value;

   return ( value & UINT32_C( 0xFFFFFF ) ) | ( ( uint32_t )( length & 255 ) << 24 );
}

/* StrDel(): the bytes of the text under a space of the mask, and all bytes past the mask */
void x_strDelete( X_CALL_CTX *ctx )
{
   const char *text = x_parc( ctx, 1 );
   const char *mask = x_parc( ctx, 2 );
   size_t length = x_parclen( ctx, 1 );
   size_t maskLength = x_parclen( ctx, 2 );
   size_t written = 0;
   size_t i;
   char *result = ( char * )x_xgrab( length + 1 );

   for( i = 0; i < length; ++i )
   {
      if( i >= maskLength || mask[i] == ' ' )
         result[written++] = text[i];
   }

   result[written] = '\0';
   x_retclen_buffer( ctx, result, written );
}

/* The number of screen lines of width columns a text takes, or with longest the length of its longest line */
size_t x_textLines( const char *text, size_t length, size_t width, int longest )
{
   size_t position = 0;
   size_t line = 0;
   size_t result = 0;
   size_t start;
   char newline;

   while( position < length )
   {
      start = position;
      while( position < length && text[position] != '\r' && text[position] != '\n' )
         ++position;

      line = position - start;
      if( longest )
         result = IIF( line > result, line, result );
      else
         result += IIF( line == 0, 1, 1 + ( line - 1 ) / width );

      /* CR LF and LF CR end one line */
      if( position < length )
      {
         newline = text[position++];
         if( position < length && text[position] != newline )
         {
            if( text[position] == '\r' || text[position] == '\n' )
               ++position;
         }
      }
   }

   return result;
}

/* UUEncode(): lines of 45 bytes, each byte group of 3 as 4 characters, ending with a ` line */
char *x_uuEncode( const char *data, size_t length, size_t *resultLength )
{
   const unsigned char *bytes = ( const unsigned char * )data;
   size_t lines = length / 45 + ( length % 45 != 0 );
   size_t groups = length / 3 + ( length % 3 != 0 );
   size_t read = 0;
   size_t written = 0;
   size_t size, line, count, i;
   uint32_t triple;
   unsigned int digit;
   char *result;

   if( groups > ( SIZE_MAX - 3 ) / 4 || lines > ( SIZE_MAX - 3 - groups * 4 ) / 2 )
      return NULL;

   size = groups * 4 + lines * 2 + 2;
   result = ( char * )x_xgrab( size + 1 );
   while( read < length )
   {
      count = IIF( length - read > 45, 45, length - read );
      result[written++] = ( char )( count + 32 );
      for( line = 0; line < count; line += 3 )
      {
         triple = ( uint32_t )bytes[read + line] << 16;
         if( line + 1 < count )
            triple |= ( uint32_t )bytes[read + line + 1] << 8;

         if( line + 2 < count )
            triple |= bytes[read + line + 2];

         for( i = 0; i < 4; ++i )
         {
            digit = ( triple >> ( 18 - i * 6 ) ) & 63;
            result[written++] = ( char )IIF( digit, digit + 32, 96 );
         }
      }

      result[written++] = '\n';
      read += count;
   }

   result[written++] = '`';
   result[written++] = '\n';
   result[written] = '\0';
   *resultLength = written;

   return result;
}

/* Is the line the "begin mode name" line of a UU envelope? */
static int s_uuBegin( const char *line, size_t lineLength )
{
   return lineLength > 10 && memcmp( line, "begin ", 6 ) == 0;
}

/* Is the mode of a begin line three octal digits and a space? */
static int s_uuMode( const char *line )
{
   int i;

   for( i = 6; i < 9; ++i )
   {
      if( line[i] < '0' || line[i] > '7' )
         return 0;
   }

   return line[9] == ' ';
}

/* UUDecode(): the data of UU lines, with or without the begin and end envelope; NULL for a broken text */
char *x_uuDecode( const char *data, size_t length, size_t *resultLength )
{
   size_t position = 0;
   size_t written = 0;
   size_t end, lineLength, count, expected, i, j;
   int envelope = 0;
   int zero = 0;
   int ended = 0;
   int digit;
   uint32_t triple;
   char *result = ( char * )x_xgrab( length + 1 );
   const char *line;

   while( position < length )
   {
      end = position;
      while( end < length && data[end] != '\r' && data[end] != '\n' )
         ++end;

      line = data + position;
      lineLength = end - position;
      position = end;
      if( position < length && data[position++] == '\r' && position < length && data[position] == '\n' )
         ++position;

      if( lineLength == 0 )
         continue;

      /* Only the first line can begin the envelope */
      if( end == lineLength && s_uuBegin( line, lineLength ) )
      {
         if( !s_uuMode( line ) )
            goto invalid;

         envelope = 1;
         continue;
      }

      if( zero && envelope && !ended && lineLength == 3 && memcmp( line, "end", 3 ) == 0 )
      {
         ended = 1;
         continue;
      }

      if( zero || ended || ( unsigned char )line[0] < 32 || ( unsigned char )line[0] > 96 )
         goto invalid;

      count = ( ( unsigned char )line[0] - 32 ) & 63;
      if( count > 45 )
         goto invalid;

      expected = 1 + 4 * ( ( count + 2 ) / 3 );
      if( lineLength != expected )
         goto invalid;

      if( count == 0 )
      {
         zero = 1;
         continue;
      }

      for( i = 1; i < expected; i += 4 )
      {
         triple = 0;
         for( j = 0; j < 4; ++j )
         {
            digit = ( unsigned char )line[i + j];
            if( digit < 32 || digit > 96 )
               goto invalid;

            triple = ( triple << 6 ) | ( ( digit - 32 ) & 63 );
         }

         for( j = 0; j < 3 && ( ( i - 1 ) / 4 ) * 3 + j < count; ++j )
            result[written++] = ( char )( triple >> ( 16 - j * 8 ) );
      }
   }

   if( length != 0 && ( !zero || ( envelope && !ended ) ) )
      goto invalid;

   result[written] = '\0';
   *resultLength = written;

   return result;

invalid:
   x_xfree( result );

   return NULL;
}

/* UUEncode() and UUDecode(): the result, or error 1102 for a broken text */
void x_uuResult( X_CALL_CTX *ctx, int decode )
{
   size_t length = x_parclen( ctx, 1 );
   size_t resultLength;
   char *result;

   if( !decode && X_ISNUM( ctx, 2 ) )
      length = ( size_t )x_parnint( ctx, 2 );

   if( decode )
      result = x_uuDecode( x_parc( ctx, 1 ), length, &resultLength );
   else
      result = x_uuEncode( x_parc( ctx, 1 ), length, &resultLength );

   if( result == NULL )
      x_errorRaiseNamed( ctx->result, 1102, IIF( decode, "UUDECODE", "UUENCODE" ) );
   else
      x_retclen_buffer( ctx, result, resultLength );
}

/* The length of the line ending at position: CR LF, CR or LF, or one of the given endings; 0 for none */
static size_t s_readLineDelimiter( const char *text, size_t length, size_t position, const X_ITEM *endings )
{
   size_t i, count, size;
   const char *ending;

   if( endings == NULL || X_IS_NIL( endings ) )
   {
      if( text[position] == '\r' )
         return IIF( position + 1 < length && text[position + 1] == '\n', 2, 1 );

      return IIF( text[position] == '\n', 1, 0 );
   }

   count = IIF( X_IS_ARRAY( endings ), x_arrayLen( endings ), 1 );
   for( i = 1; i <= count; ++i )
   {
      if( X_IS_ARRAY( endings ) )
      {
         ending = x_arrayGetCPtr( endings, i );
         size = x_arrayGetCLen( endings, i );
      }
      else
      {
         ending = endings->u.string.data;
         size = endings->u.string.len;
      }

      if( size <= length - position && memcmp( text + position, ending, size ) == 0 )
         return size;
   }

   return 0;
}

/* Is a soft line break (byte 141 and LF of MemoEdit()) at position? */
static int s_softBreak( const char *text, size_t length, size_t position )
{
   if( ( unsigned char )text[position] != 141 || position + 1 >= length || text[position + 1] != '\n' )
      return 0;

   return position == 0 || ( unsigned char )text[position - 1] < 128;
}

/* ReadLine(): one line from the byte nStartOffset, cut at nLineLen columns with lWrap */
void x_textReadLineCall( X_CALL_CTX *ctx )
{
   const char *text = x_parc( ctx, 1 );
   const X_ITEM *endings = x_paritem( ctx, 2 );
   size_t length = x_parclen( ctx, 1 );
   size_t width = ( size_t )x_parnint( ctx, 3 );
   size_t tab = ( size_t )x_parnintdef( ctx, 4, 4 );
   size_t columns = 0;
   size_t blank = SIZE_MAX;
   size_t afterBlank = 0;
   size_t position, start, end, next, delimiter, step, i;
   int64_t requested = x_parnintdef( ctx, 6, 1 );
   int wrap = x_parldef( ctx, 5, 1 );
   int found = 0;

   if( endings != NULL && X_IS_ARRAY( endings ) )
   {
      for( i = 1; i <= ( size_t )x_arrayLen( endings ); ++i )
      {
         const X_ITEM *item = x_arrayGetItemPtr( endings, i );

         if( !X_IS_STRING( item ) || item->u.string.len == 0 )
         {
            x_errorRaiseNamed( ctx->result, 1102, "READLINE" );
            return;
         }
      }
   }

   if( requested < 1 )
      start = 0;
   else if( ( uint64_t )requested > length )
      start = length;
   else
      start = ( size_t )requested - 1;

   end = start;
   next = start;
   position = start;
   while( position < length )
   {
      delimiter = s_readLineDelimiter( text, length, position, endings );
      if( delimiter )
      {
         end = position;
         next = position + delimiter;
         found = 1;
         break;
      }

      if( s_softBreak( text, length, position ) )
      {
         if( !wrap )
         {
            end = position;
            next = position + 2;
            found = 1;
            break;
         }

         position += 2;
         end = position;
         next = position;
         continue;
      }

      if( !wrap )
      {
         ++position;
         end = position;
         next = position;
         continue;
      }

      step = IIF( text[position] == '\t', tab - columns % tab, 1 );
      if( step > width - columns )
      {
         if( blank != SIZE_MAX && blank > start )
         {
            end = blank;
            next = afterBlank;
         }
         else
         {
            end = IIF( position == start, position + 1, position );
            next = end;
         }

         break;
      }

      columns += step;
      if( text[position] == ' ' || text[position] == '\t' )
      {
         if( position == start || ( text[position - 1] != ' ' && text[position - 1] != '\t' ) )
            blank = position;

         afterBlank = position + 1;
      }

      ++position;
      end = position;
      next = position;
   }

   x_storl( ctx, found, 7 );
   x_storl( ctx, next >= length, 8 );
   x_stornint( ctx, ( int64_t )end, 9 );
   x_stornint( ctx, ( int64_t )next, 10 );
   x_ret( ctx );
}

/* ---------------------------------------------------------------------------------------------
   Char*(), Pos*() and Range*() functions of the API
   --------------------------------------------------------------------------------------------- */

/* CharHist(): create a histogram of the characters in a string */
void x_textCharHist( X_CALL_CTX *ctx )
{
   /* Element n + 1 counts the UTF-8 characters with code n; characters above 255 are not counted */
   const char *text = IIF( X_ISCHAR( ctx, 1 ), x_parc( ctx, 1 ), "" );
   size_t length = IIF( X_ISCHAR( ctx, 1 ), x_parclen( ctx, 1 ), 0 );
   int64_t counts[256] = { 0 };
   size_t chars, i;
   uint32_t *codes = x_ucodes( text, length, &chars );

   for( i = 0; i < chars; ++i )
   {
      if( codes[i] < 256 )
         ++counts[codes[i]];
   }

   x_xfree( codes );
   x_arrayNew( ctx->result, 256 );
   for( i = 0; i < 256; ++i )
      x_arraySetNLL( ctx->result, i + 1, ( long long )counts[i] );
}

/* CharList(): remove duplicate characters from a string */
void x_textCharList( X_CALL_CTX *ctx )
{
   /* Every UTF-8 character once, in the order of its first appearance */
   size_t length, count = 0, i;
   uint32_t *codes = x_ucodes( x_parc( ctx, 1 ), x_parclen( ctx, 1 ), &length );

   for( i = 0; i < length; ++i )
   {
      if( !x_ucodeListed( codes, count, codes[i] ) )
         codes[count++] = codes[i];
   }

   x_ucodesReturn( ctx, codes, count );
}

/* CharMirr(): reverse the order of the characters in a string */
void x_textCharMirr( X_CALL_CTX *ctx )
{
   /* The order of the UTF-8 characters is reversed; with lNoSpaces the trailing spaces stay at the end */
   size_t length, mirrored, i;
   uint32_t *codes = x_ucodes( x_parc( ctx, 1 ), x_parclen( ctx, 1 ), &length );
   uint32_t swap;

   mirrored = length;
   if( X_ISLOG( ctx, 2 ) && x_parl( ctx, 2 ) )
   {
      while( mirrored > 0 && codes[mirrored - 1] == ' ' )
         --mirrored;
   }

   for( i = 0; i < mirrored / 2; ++i )
   {
      swap = codes[i];
      codes[i] = codes[mirrored - 1 - i];
      codes[mirrored - 1 - i] = swap;
   }

   x_ucodesReturn( ctx, codes, length );
}

/* CharMix(): merge the characters of two strings */
void x_textCharMix( X_CALL_CTX *ctx )
{
   /* The characters are UTF-8 characters; the second string repeats when it is shorter */
   size_t length, otherLength, i;
   uint32_t *text;
   uint32_t *other;
   uint32_t *result;

   if( x_parclen( ctx, 2 ) == 0 )
   {
      x_retclen( ctx, x_parc( ctx, 1 ), x_parclen( ctx, 1 ) );
      return;
   }

   text = x_ucodes( x_parc( ctx, 1 ), x_parclen( ctx, 1 ), &length );
   other = x_ucodes( x_parc( ctx, 2 ), x_parclen( ctx, 2 ), &otherLength );
   result = ( uint32_t * )x_xgrab( ( length * 2 + 1 ) * sizeof( uint32_t ) );
   for( i = 0; i < length; ++i )
   {
      result[i * 2] = text[i];
      result[i * 2 + 1] = other[i % otherLength];
   }

   x_xfree( text );
   x_xfree( other );
   x_ucodesReturn( ctx, result, length * 2 );
}

/* CharNoList(): return a string of all characters not included in a string */
void x_textCharNoList( X_CALL_CTX *ctx )
{
   /* The characters with codes 0 to 255 that the string lacks, as UTF-8 characters */
   const char *text = IIF( X_ISCHAR( ctx, 1 ), x_parc( ctx, 1 ), "" );
   size_t length = IIF( X_ISCHAR( ctx, 1 ), x_parclen( ctx, 1 ), 0 );
   unsigned char seen[256] = { 0 };
   size_t chars, count = 0, size, i;
   uint32_t *codes = x_ucodes( text, length, &chars );
   uint32_t missing[256];
   char *result;

   for( i = 0; i < chars; ++i )
   {
      if( codes[i] < 256 )
         seen[codes[i]] = 1;
   }

   for( i = 0; i < 256; ++i )
   {
      if( !seen[i] )
         missing[count++] = ( uint32_t )i;
   }

   x_xfree( codes );
   result = x_ucodesString( missing, count, &size );
   x_retclen_buffer( ctx, result, size );
}

/* CharOne(): remove duplicate adjacent characters from a string */
void x_textCharOne( X_CALL_CTX *ctx )
{
   /* With one argument it is the string and every character is reduced; the characters are UTF-8 characters */
   int hasList = X_ISCHAR( ctx, 2 );
   int n = IIF( hasList, 2, 1 );
   size_t listCount = 0, count, kept = 0, i;
   uint32_t *list = NULL;
   uint32_t *codes = x_ucodes( x_parc( ctx, n ), x_parclen( ctx, n ), &count );
   uint32_t previous = 0;

   if( hasList )
      list = x_ucodes( x_parc( ctx, 1 ), x_parclen( ctx, 1 ), &listCount );

   for( i = 0; i < count; ++i )
   {
      uint32_t code = codes[i];

      if( i == 0 || code != previous || ( hasList && !x_ucodeListed( list, listCount, code ) ) )
         codes[kept++] = code;

      previous = code;
   }

   if( list != NULL )
      x_xfree( list );

   x_ucodesReturn( ctx, codes, kept );
}

/* CharPack(): compress a string */
void x_textCharPack( X_CALL_CTX *ctx )
{
   const unsigned char *text = ( const unsigned char * )x_parc( ctx, 1 );
   size_t length = x_parclen( ctx, 1 );
   char *result = ( char * )x_xgrab( 4 + length + length / 128 + 2 );
   size_t i = 0, j = 4;

   /* The header "XPK0", then runs: a count byte 0..127 copies count + 1 literal bytes, a count byte -127..-2
      repeats the next byte 1 - count times */
   memcpy( result, "XPK0", 4 );
   while( i < length )
   {
      size_t run = 1;

      while( i + run < length && run < 128 && text[i + run] == text[i] )
         ++run;

      if( run >= 3 )
      {
         result[j++] = ( char )( 1 - ( int )run );
         result[j++] = ( char )text[i];
         i += run;
      }
      else
      {
         size_t start = i, count = 0;

         while( i < length && count < 128 && !( i + 2 < length && text[i] == text[i + 1] && text[i] == text[i + 2] ) )
         {
            ++i;
            ++count;
         }

         result[j++] = ( char )( count - 1 );
         memcpy( result + j, text + start, count );
         j += count;
      }
   }

   result[j] = '\0';
   x_retclen_buffer( ctx, result, j );
}

/* CharRela(): test if two substrings in two strings have the same position */
void x_textCharRela( X_CALL_CTX *ctx )
{
   const char *search1 = x_parc( ctx, 1 );
   const char *text1 = x_parc( ctx, 2 );
   const char *search2 = x_parc( ctx, 3 );
   const char *text2 = x_parc( ctx, 4 );
   size_t length1 = x_parclen( ctx, 1 );
   size_t length2 = x_parclen( ctx, 3 );
   size_t textLength1 = x_parclen( ctx, 2 );
   size_t textLength2 = x_parclen( ctx, 4 );
   size_t position;

   /* The first position where cSearch1 starts in cString1 and cSearch2 starts in cString2 */
   if( length1 > 0 && length2 > 0 )
   {
      for( position = 0; position + length1 <= textLength1 && position + length2 <= textLength2; ++position )
      {
         if( memcmp( text1 + position, search1, length1 ) == 0 && memcmp( text2 + position, search2, length2 ) == 0 )
         {
            x_retnint( ctx, ( int64_t )position + 1 );
            return;
         }
      }
   }

   x_retni( ctx, 0 );
}

/* CharRelRep(): replace characters if two substrings in two strings have the same position */
void x_textCharRelRep( X_CALL_CTX *ctx )
{
   const char *search1 = x_parc( ctx, 1 );
   const char *text1 = x_parc( ctx, 2 );
   const char *search2 = x_parc( ctx, 3 );
   const char *text2 = x_parc( ctx, 4 );
   const char *replace = x_parc( ctx, 5 );
   size_t search1Length = x_parclen( ctx, 1 );
   size_t search2Length = x_parclen( ctx, 3 );
   size_t replaceLength = x_parclen( ctx, 5 );
   size_t length = x_parclen( ctx, 4 );
   size_t common = IIF( x_parclen( ctx, 2 ) < length, x_parclen( ctx, 2 ), length );
   char *result = ( char * )x_xgrab( length + 1 );
   size_t i, position;

   memcpy( result, text2, length );
   result[length] = '\0';
   /* Step i pairs the i-th characters of the three strings, a shorter cSearch2 or cReplace repeats its last one;
      the positions are found in the original cString2 */
   if( search2Length > 0 && replaceLength > 0 )
   {
      for( i = 0; i < search1Length; ++i )
      {
         char c1 = search1[i];
         char c2 = search2[IIF( i < search2Length, i, search2Length - 1 )];
         char cr = replace[IIF( i < replaceLength, i, replaceLength - 1 )];

         for( position = 0; position < common; ++position )
         {
            if( text1[position] == c1 && text2[position] == c2 )
               result[position] = cr;
         }
      }
   }

   x_storclen( ctx, result, length, 4 );
   x_retclen_buffer( ctx, result, length );
}

/* CharRepl(): replace a list of characters with a corresponding list */
void x_textCharRepl( X_CALL_CTX *ctx )
{
   /* The lists and the string are UTF-8 characters, so a character may be replaced by a longer one */
   size_t searchLength, replaceLength, length, i, j;
   uint32_t *search = x_ucodes( x_parc( ctx, 1 ), x_parclen( ctx, 1 ), &searchLength );
   uint32_t *replace = x_ucodes( x_parc( ctx, 3 ), x_parclen( ctx, 3 ), &replaceLength );
   uint32_t *result = x_ucodes( x_parc( ctx, 2 ), x_parclen( ctx, 2 ), &length );

   if( replaceLength > 0 )
   {
      /* A shorter replacement list repeats its last character */
      if( X_ISLOG( ctx, 4 ) && x_parl( ctx, 4 ) )
      {
         for( j = 0; j < length; ++j )
         {
            for( i = 0; i < searchLength; ++i )
            {
               if( result[j] == search[i] )
               {
                  result[j] = replace[IIF( i < replaceLength, i, replaceLength - 1 )];
                  break;
               }
            }
         }
      }
      else
      {
         for( i = 0; i < searchLength; ++i )
         {
            for( j = 0; j < length; ++j )
            {
               if( result[j] == search[i] )
                  result[j] = replace[IIF( i < replaceLength, i, replaceLength - 1 )];
            }
         }
      }
   }

   x_xfree( search );
   x_xfree( replace );
   x_ucodesReturn( ctx, result, length );
}

/* CharSList(): remove duplicate characters from a string and sort the result */
void x_textCharSList( X_CALL_CTX *ctx )
{
   /* Every UTF-8 character once, sorted by its code */
   size_t length, count = 0, i;
   uint32_t *codes = x_ucodes( x_parc( ctx, 1 ), x_parclen( ctx, 1 ), &length );
   uint32_t *list;

   for( i = 0; i < length; ++i )
   {
      if( !x_ucodeListed( codes, count, codes[i] ) )
         codes[count++] = codes[i];
   }

   /* x_charSort() sorts into a copy of its text */
   list = ( uint32_t * )x_xgrab( ( count + 1 ) * sizeof( uint32_t ) );
   memcpy( list, codes, count * sizeof( uint32_t ) );
   x_charSort( list, codes, 0, count, 1, 1, 0, 0 );
   x_xfree( codes );
   x_ucodesReturn( ctx, list, count );
}

/* CharSort(): sort characters or character sequences within a string */
void x_textCharSort( X_CALL_CTX *ctx )
{
   /* The lengths count UTF-8 characters, which compare by their code */
   size_t chars;
   uint32_t *codes = x_ucodes( x_parc( ctx, 1 ), x_parclen( ctx, 1 ), &chars );
   int64_t length = ( int64_t )chars;
   int64_t element = x_parnintdef( ctx, 2, 1 );
   int64_t compare = x_parnintdef( ctx, 3, element );
   int64_t skip = x_parnintdef( ctx, 4, 0 );
   int64_t offset = x_parnintdef( ctx, 5, 0 );
   int64_t sortSize = x_parnintdef( ctx, 6, length - skip );
   int descending = x_parl( ctx, 7 );
   int valid;
   uint32_t *sorted;
   size_t first, count, size;
   char *result;

   /* Invalid lengths give an empty string */
   valid = element >= 1 && compare >= 1 && offset >= 0 && offset + compare <= element;
   valid = valid && skip >= 0 && skip <= length && sortSize >= 0 && sortSize <= length - skip;
   if( !valid )
   {
      x_xfree( codes );
      x_retc( ctx, "" );
      return;
   }

   sorted = ( uint32_t * )x_xgrab( ( chars + 1 ) * sizeof( uint32_t ) );
   memcpy( sorted, codes, chars * sizeof( uint32_t ) );
   first = ( size_t )skip;
   count = ( size_t )( sortSize / element );
   x_charSort( sorted, codes, first, count, ( size_t )element, ( size_t )compare, ( size_t )offset, descending );
   result = x_ucodesString( sorted, chars, &size );
   x_xfree( codes );
   x_xfree( sorted );
   x_storclen( ctx, result, size, 1 );
   x_retclen_buffer( ctx, result, size );
}

/* CharSpread(): format a character string for block paragraphs */
void x_textCharSpread( X_CALL_CTX *ctx )
{
   const char *text = x_parc( ctx, 1 );
   size_t length = x_parclen( ctx, 1 );
   size_t chars = x_utf8Length( text, length );
   int64_t target = x_parnint( ctx, 2 );
   size_t first, last, gaps = 0, gap = 0, extra, base, rest, i, next, j = 0;
   uint32_t codepoint;
   X_UCHAR c;
   char *result;

   /* nLength counts characters and c may be any UTF-8 character; the gaps are the runs of c between the
      first and the last other character */
   x_ucharArg( ctx, 3, &c );
   first = x_ucharRun( text, length, &c, 0 ) * c.size;
   last = length - x_ucharRun( text + first, length - first, &c, 1 ) * c.size;
   for( i = first; i < last; i = next )
   {
      next = i;
      x_utf8Decode( text, length, &next, &codepoint );
      if( x_ucharAt( text, last, i, &c ) && next < last && !x_ucharAt( text, last, next, &c ) )
         ++gaps;
   }

   if( gaps == 0 || target <= ( int64_t )chars )
   {
      x_retclen( ctx, text, length );
      return;
   }

   /* The remainder goes to the gaps alternately from the left and from the right */
   extra = ( size_t )target - chars;
   base = extra / gaps;
   rest = extra % gaps;
   result = ( char * )x_xgrab( length + extra * c.size + 1 );
   for( i = 0; i < length; i = next )
   {
      next = i;
      x_utf8Decode( text, length, &next, &codepoint );
      memcpy( result + j, text + i, next - i );
      j += next - i;
      if( i >= first && next < last && x_ucharAt( text, last, i, &c ) && !x_ucharAt( text, last, next, &c ) )
      {
         size_t add = base + IIF( gap < ( rest + 1 ) / 2 || gap >= gaps - rest / 2, 1, 0 );

         j = ( size_t )( x_ucharFill( result + j, &c, add ) - result );
         ++gap;
      }
   }

   result[j] = '\0';
   x_retclen_buffer( ctx, result, j );
}

/* CharUnpack(): uncompress a string compressed with CharPack() */
void x_textCharUnpack( X_CALL_CTX *ctx )
{
   const unsigned char *packed = ( const unsigned char * )x_parc( ctx, 1 );
   size_t length = x_parclen( ctx, 1 );
   size_t total = 0, i = 4, j = 0;
   char *result;

   /* A string that CharPack() did not create gives "" */
   if( length < 4 || memcmp( packed, "XPK0", 4 ) != 0 )
   {
      x_retc( ctx, "" );
      return;
   }
   while( i < length )
   {
      int count = ( signed char )packed[i++];

      if( count >= 0 )
      {
         if( i + ( size_t )count + 1 > length )
         {
            x_retc( ctx, "" );
            return;
         }

         total += ( size_t )count + 1;
         i += ( size_t )count + 1;
      }
      else
      {
         if( i >= length || count == -128 )
         {
            x_retc( ctx, "" );
            return;
         }

         total += ( size_t )( 1 - count );
         ++i;
      }
   }

   result = ( char * )x_xgrab( total + 1 );
   for( i = 4; i < length; )
   {
      int count = ( signed char )packed[i++];

      if( count >= 0 )
      {
         memcpy( result + j, packed + i, ( size_t )count + 1 );
         j += ( size_t )count + 1;
         i += ( size_t )count + 1;
      }
      else
      {
         memset( result + j, packed[i++], ( size_t )( 1 - count ) );
         j += ( size_t )( 1 - count );
      }
   }

   result[total] = '\0';
   x_retclen_buffer( ctx, result, total );
}

/* PosChar(): replace a single character at a specified position in a string */
void x_textPosChar( X_CALL_CTX *ctx )
{
   /* nPos counts characters and xChar may be any UTF-8 character */
   size_t length;
   uint32_t *codes = x_ucodes( x_parc( ctx, 1 ), x_parclen( ctx, 1 ), &length );
   int64_t position = x_parnintdef( ctx, 3, ( int64_t )length );
   size_t size;
   char *result;

   if( position >= 1 && ( uint64_t )position <= length )
      codes[position - 1] = x_ucodeArg( ctx, 2, ' ' );

   result = x_ucodesString( codes, length, &size );
   x_xfree( codes );
   x_storclen( ctx, result, size, 1 );
   x_retclen_buffer( ctx, result, size );
}

/* PosDel(): delete character(s) at a specified position in a string */
void x_textPosDel( X_CALL_CTX *ctx )
{
   /* nPos and nCount count characters */
   const char *text = x_parc( ctx, 1 );
   size_t chars;
   uint32_t *codes = x_ucodes( text, x_parclen( ctx, 1 ), &chars );
   int64_t length = ( int64_t )chars;
   int64_t count = x_parnintdef( ctx, 3, 1 );
   int64_t position = x_parnintdef( ctx, 2, length - count + 1 );
   size_t rest;

   /* Without a position the last characters go */
   if( count < 1 || position < 1 || position > length )
   {
      x_xfree( codes );
      x_retclen( ctx, text, x_parclen( ctx, 1 ) );
      return;
   }

   if( count > length - position + 1 )
      count = length - position + 1;

   rest = ( size_t )( length - position + 1 - count );
   memmove( codes + position - 1, codes + position - 1 + count, rest * sizeof( uint32_t ) );
   x_ucodesReturn( ctx, codes, ( size_t )( length - count ) );
}

/* PosDiff(): search the first position where two character strings differ */
void x_textPosDiff( X_CALL_CTX *ctx )
{
   /* nSkipChars and the position count UTF-8 characters */
   size_t length1, length2, i;
   uint32_t *text1 = x_ucodes( x_parc( ctx, 1 ), x_parclen( ctx, 1 ), &length1 );
   uint32_t *text2 = x_ucodes( x_parc( ctx, 2 ), x_parclen( ctx, 2 ), &length2 );
   int64_t ignored = x_parnintdef( ctx, 3, 0 );
   int64_t found = 0;

   for( i = ( size_t )IIF( ignored > 0, ignored, 0 ); i < length1 || i < length2; ++i )
   {
      if( i >= length1 || i >= length2 || text1[i] != text2[i] )
      {
         found = ( int64_t )i + 1;
         break;
      }
   }

   x_xfree( text1 );
   x_xfree( text2 );
   x_retnint( ctx, found );
}

/* PosEqual(): search the first position where two character strings are equal */
void x_textPosEqual( X_CALL_CTX *ctx )
{
   /* nCompare, nSkipChars and the position count UTF-8 characters */
   size_t length1, length2, shorter, i;
   uint32_t *text1 = x_ucodes( x_parc( ctx, 1 ), x_parclen( ctx, 1 ), &length1 );
   uint32_t *text2 = x_ucodes( x_parc( ctx, 2 ), x_parclen( ctx, 2 ), &length2 );
   int64_t compare, ignored = x_parnintdef( ctx, 4, 0 );
   int64_t found = 0;

   shorter = IIF( length1 < length2, length1, length2 );
   compare = x_parnintdef( ctx, 3, ( int64_t )shorter );

   /* The first position from which compare characters agree in both strings */
   if( compare >= 1 )
   {
      for( i = ( size_t )IIF( ignored > 0, ignored, 0 ); i + ( size_t )compare <= shorter; ++i )
      {
         if( memcmp( text1 + i, text2 + i, ( size_t )compare * sizeof( uint32_t ) ) == 0 )
         {
            found = ( int64_t )i + 1;
            break;
         }
      }
   }

   x_xfree( text1 );
   x_xfree( text2 );
   x_retnint( ctx, found );
}

/* PosIns(): insert character(s) at a specified position in a string */
void x_textPosIns( X_CALL_CTX *ctx )
{
   const char *text = x_parc( ctx, 1 );
   size_t bytes = x_parclen( ctx, 1 );
   size_t length = x_utf8Length( text, bytes );
   const char *insert = x_parc( ctx, 2 );
   size_t insertLength = x_parclen( ctx, 2 );
   int64_t position = x_parnintdef( ctx, 3, ( int64_t )length );
   size_t before;
   char *result;

   /* nPos counts characters: the new characters go in front of the character at nPos, by default in front
      of the last one */
   if( position < 1 || ( uint64_t )position > length + 1 )
   {
      x_retclen( ctx, text, bytes );
      return;
   }

   before = x_utf8Offset( text, bytes, ( size_t )position - 1 );
   result = ( char * )x_xgrab( bytes + insertLength + 1 );
   memcpy( result, text, before );
   memcpy( result + before, insert, insertLength );
   memcpy( result + before + insertLength, text + before, bytes - before );
   result[bytes + insertLength] = '\0';
   x_retclen_buffer( ctx, result, bytes + insertLength );
}

/* PosRange(): retrieve the position of the first character out of a range found in a string */
void x_textPosRange( X_CALL_CTX *ctx )
{
   /* The bounds are UTF-8 characters or code points; nSkipChars and the position count characters */
   uint32_t first = x_ucodeArg( ctx, 1, 0 );
   uint32_t last = x_ucodeArg( ctx, 2, 0 );
   int outside = X_ISLOG( ctx, 4 ) && x_parl( ctx, 4 );
   int64_t ignored = x_parnintdef( ctx, 5, 0 );
   size_t length, i;
   uint32_t *codes = x_ucodes( x_parc( ctx, 3 ), x_parclen( ctx, 3 ), &length );
   int64_t found = 0;

   for( i = ( size_t )IIF( ignored > 0, ignored, 0 ); i < length; ++i )
   {
      if( ( codes[i] >= first && codes[i] <= last ) != outside )
      {
         found = ( int64_t )i + 1;
         break;
      }
   }

   x_xfree( codes );
   x_retnint( ctx, found );
}

/* PosRepl(): replace characters in a string beginning at a specified position */
void x_textPosRepl( X_CALL_CTX *ctx )
{
   /* nPos counts characters, and the replacement overwrites as many characters as it has */
   const char *text = x_parc( ctx, 1 );
   size_t chars, replaceChars, total;
   uint32_t *codes = x_ucodes( text, x_parclen( ctx, 1 ), &chars );
   uint32_t *replace = x_ucodes( x_parc( ctx, 2 ), x_parclen( ctx, 2 ), &replaceChars );
   int64_t length = ( int64_t )chars;
   int64_t position = x_parnintdef( ctx, 3, length - ( int64_t )replaceChars + 1 );
   int64_t size;
   uint32_t *result;
   char *string;

   /* Without a position the replacement overwrites the end; it may make the string longer */
   if( position < 1 )
      position = 1;

   if( position > length + 1 )
   {
      x_xfree( codes );
      x_xfree( replace );
      x_retclen( ctx, text, x_parclen( ctx, 1 ) );
      return;
   }

   size = IIF( position - 1 + ( int64_t )replaceChars > length, position - 1 + ( int64_t )replaceChars, length );
   result = ( uint32_t * )x_xgrab( ( ( size_t )size + 1 ) * sizeof( uint32_t ) );
   memcpy( result, codes, ( size_t )length * sizeof( uint32_t ) );
   memcpy( result + position - 1, replace, replaceChars * sizeof( uint32_t ) );
   string = x_ucodesString( result, ( size_t )size, &total );
   x_xfree( codes );
   x_xfree( replace );
   x_xfree( result );
   x_storclen( ctx, string, total, 1 );
   x_retclen_buffer( ctx, string, total );
}

/* RangeRem(): delete character(s) within a specified range of characters */
void x_textRangeRem( X_CALL_CTX *ctx )
{
   /* The bounds and the string are UTF-8 characters */
   uint32_t first = x_rangeBound( ctx, 1 );
   uint32_t last = x_rangeBound( ctx, 2 );
   size_t length, count = 0, i;
   uint32_t *codes = x_ucodes( x_parc( ctx, 3 ), x_parclen( ctx, 3 ), &length );

   for( i = 0; i < length; ++i )
   {
      if( !x_rangeHas( first, last, codes[i] ) )
         codes[count++] = codes[i];
   }

   x_ucodesReturn( ctx, codes, count );
}

/* RangeRepl(): replace character(s) within a specified range of characters */
void x_textRangeRepl( X_CALL_CTX *ctx )
{
   /* The bounds, the string and the replacement are UTF-8 characters */
   uint32_t first = x_rangeBound( ctx, 1 );
   uint32_t last = x_rangeBound( ctx, 2 );
   uint32_t replace = x_ucodeArg( ctx, 4, ' ' );
   size_t length, size, i;
   uint32_t *codes = x_ucodes( x_parc( ctx, 3 ), x_parclen( ctx, 3 ), &length );
   char *result;

   for( i = 0; i < length; ++i )
   {
      if( x_rangeHas( first, last, codes[i] ) )
         codes[i] = replace;
   }

   result = x_ucodesString( codes, length, &size );
   x_xfree( codes );
   x_storclen( ctx, result, size, 3 );
   x_retclen_buffer( ctx, result, size );
}

/* ---------------------------------------------------------------------------------------------
   Text functions of the API
   --------------------------------------------------------------------------------------------- */

/* AddASCII(): add a numeric value to a byte in a string, optionally carrying through earlier bytes */
void x_textAddASCII( X_CALL_CTX *ctx )
{
   const char *source = x_parc( ctx, 1 );
   size_t length = x_parclen( ctx, 1 );
   int64_t position = x_parnintdef( ctx, 3, ( int64_t )length );
   int64_t value = x_parnint( ctx, 2 );
   char *result = ( char * )x_xgrab( length + 1 );

   memcpy( result, source, length );
   result[length] = '\0';

   if( position >= 1 && ( uint64_t )position <= length )
   {
      if( x_parldef( ctx, 4, 0 ) )
      {
         size_t current = ( size_t )position;

         while( current > 0 && value != 0 )
         {
            int64_t byte = ( int64_t )( unsigned char )source[current - 1] + value % 256;

            value /= 256;
            if( byte > 255 )
               ++value;
            else if( byte < 0 )
               --value;

            result[current - 1] = ( char )( unsigned char )byte;
            --current;
         }
      }
      else
      {
         uint64_t byte = ( unsigned char )result[position - 1];

         result[position - 1] = ( char )( unsigned char )( byte + ( uint64_t )value );
      }
   }

   x_storclen( ctx, result, length, 1 );
   x_retclen_buffer( ctx, result, length );
}

/* AfterAtNum(): return the portion of a string after the nth occurrence of a substring */
void x_textAfterAtNum( X_CALL_CTX *ctx )
{
   size_t searchLength = x_parclen( ctx, 1 );
   const char *text = x_parc( ctx, 2 );
   size_t textLength = x_parclen( ctx, 2 );

   int64_t occurrence = x_parnintdef( ctx, 3, 0 );
   int64_t ignored = x_parnintdef( ctx, 4, 0 );
   size_t found = x_atNumFind( x_parc( ctx, 1 ), searchLength, text, textLength, occurrence, ignored );

   if( found == SIZE_MAX || found + searchLength >= textLength )
      x_retc_null( ctx );
   else
      x_retclen( ctx, text + found + searchLength, textLength - found - searchLength );
}

/* AnsiToHtml(): escape HTML markup characters in a UTF-8 string while preserving existing entities */
void x_textAnsiToHtml( X_CALL_CTX *ctx )
{
   const char *source = x_parc( ctx, 1 );
   size_t length = x_parclen( ctx, 1 );
   size_t capacity;
   size_t input;
   size_t output = 0;
   char *result;

   if( length > ( SIZE_MAX - 1 ) / 5 )
   {
      x_errorRaiseNamed( ctx->result, 1102, "ANSITOHTML" );
      return;
   }

   capacity = length * 5 + 1;
   result = ( char * )x_xgrab( capacity );

   for( input = 0; input < length; ++input )
   {
      const char *replacement = NULL;
      size_t replacementLength = 0;

      if( source[input] == '&' )
      {
         size_t end = input + 1;

         while( end < length && source[end] != ';' && !isspace( ( unsigned char )source[end] ) )
         {
            if( source[end] == '<' || source[end] == '>' || source[end] == '&' )
               break;

            ++end;
         }

         if( end < length && source[end] == ';' && end > input + 1 )
         {
            replacementLength = end - input + 1;
            memcpy( result + output, source + input, replacementLength );
            output += replacementLength;
            input = end;
            continue;
         }

         replacement = "&amp;";
         replacementLength = 5;
      }
      else if( source[input] == '<' )
      {
         replacement = "&lt;";
         replacementLength = 4;
      }
      else if( source[input] == '>' )
      {
         replacement = "&gt;";
         replacementLength = 4;
      }

      if( replacement != NULL )
      {
         memcpy( result + output, replacement, replacementLength );
         output += replacementLength;
      }
      else
         result[output++] = source[input];
   }

   result[output] = '\0';
   x_retclen_buffer( ctx, result, output );
}

/* ASCIISum(): return the sum of all byte values in a string */
void x_textASCIISum( X_CALL_CTX *ctx )
{
   const unsigned char *text = ( const unsigned char * )x_parc( ctx, 1 );
   size_t length = x_parclen( ctx, 1 );
   uint64_t sum = 0;

   for( size_t i = 0; i < length; ++i )
      sum += text[i];

   if( sum <= INT64_MAX )
      x_retnint( ctx, ( int64_t )sum );
   else
      x_retuint64( ctx, sum );
}

/* At(): return the position of a substring within a character string */
void x_textAt( X_CALL_CTX *ctx )
{
   const char *search = x_parc( ctx, 1 );
   size_t searchLen = x_parclen( ctx, 1 );
   const char *text = x_parc( ctx, 2 );
   size_t textLen = x_parclen( ctx, 2 );

   int64_t start = IIF( x_pcount( ctx ) >= 3, x_parnint( ctx, 3 ), 1 );
   int64_t end = IIF( x_pcount( ctx ) >= 4, x_parnint( ctx, 4 ), ( int64_t )textLen );

   if( start < 1 )
      start = 1;

   if( end > ( int64_t )textLen )
      end = ( int64_t )textLen;

   if( start > end )
   {
      x_retni( ctx, 0 );
      return;
   }

   size_t found = x_strAt( search, searchLen, text + ( start - 1 ), ( size_t )( end - start + 1 ) );

   x_retni( ctx, IIF( found, ( int )( found + ( size_t )( start - 1 ) ), 0 ) );
}

/* ATokens(): split a string into tokens based on a delimiter */
void x_textATokens( X_CALL_CTX *ctx )
{
   const char *text;
   const char *delimiter;
   size_t length, delimiterLength, position, start, count, index;
   int eolDelimiter, collapse, skipQuotes, doubleQuotesOnly;
   char quote;

   text = x_parc( ctx, 1 );
   length = x_parclen( ctx, 1 );
   eolDelimiter = X_ISLOG( ctx, 2 ) && x_parl( ctx, 2 );
   delimiter = IIF( X_ISCHAR( ctx, 2 ) && x_parclen( ctx, 2 ), x_parc( ctx, 2 ), " " );
   delimiterLength = IIF( X_ISCHAR( ctx, 2 ) && x_parclen( ctx, 2 ), x_parclen( ctx, 2 ), 1 );
   collapse = !eolDelimiter && !( X_ISCHAR( ctx, 2 ) && x_parclen( ctx, 2 ) );
   skipQuotes = x_parldef( ctx, 3, 0 );
   doubleQuotesOnly = x_parldef( ctx, 4, 0 );

   if( collapse )
   {
      while( length && *text == delimiter[0] )
      {
         ++text;
         --length;
      }
      while( length && text[length - 1] == delimiter[0] )
         --length;
   }

   count = 1;
   quote = 0;
   for( position = 0; position < length; ++position )
   {
      int separator = 0;

      if( quote )
      {
         if( text[position] == quote )
            quote = 0;
      }
      else if( skipQuotes && ( text[position] == '"' || ( !doubleQuotesOnly && text[position] == '\'' ) ) )
         quote = text[position];
      else if( eolDelimiter )
         separator = text[position] == '\r' || text[position] == '\n';
      else if( text[position] == delimiter[0] && position + delimiterLength <= length )
         separator = memcmp( text + position, delimiter, delimiterLength ) == 0;

      if( separator )
      {
         ++count;
         if( eolDelimiter )
         {
            /* CR LF and LF CR are one line end */
            if( position + 1 < length && text[position + 1] != text[position] )
            {
               if( text[position + 1] == '\r' || text[position + 1] == '\n' )
                  ++position;
            }
         }
         else
         {
            position += delimiterLength - 1;
            if( collapse )
            {
               while( position + 1 < length && text[position + 1] == delimiter[0] )
                  ++position;
            }
         }
      }
   }

   x_arrayNew( ctx->result, count );
   quote = 0;
   index = 1;
   start = 0;
   for( position = 0; position < length; ++position )
   {
      int separator = 0;

      if( quote )
      {
         if( text[position] == quote )
            quote = 0;
      }
      else if( skipQuotes && ( text[position] == '"' || ( !doubleQuotesOnly && text[position] == '\'' ) ) )
         quote = text[position];
      else if( eolDelimiter )
         separator = text[position] == '\r' || text[position] == '\n';
      else if( text[position] == delimiter[0] && position + delimiterLength <= length )
         separator = memcmp( text + position, delimiter, delimiterLength ) == 0;

      if( separator )
      {
         x_arraySetCL( ctx->result, index++, text + start, position - start );
         if( eolDelimiter )
         {
            /* CR LF and LF CR are one line end */
            if( position + 1 < length && text[position + 1] != text[position] )
            {
               if( text[position + 1] == '\r' || text[position + 1] == '\n' )
                  ++position;
            }
         }
         else
         {
            position += delimiterLength - 1;
            if( collapse )
            {
               while( position + 1 < length && text[position + 1] == delimiter[0] )
                  ++position;
            }
         }

         start = position + 1;
      }
   }

   x_arraySetCL( ctx->result, index, text + start, length - start );
}

/* Blank(): return an empty value for the data types A, C, D, L, M and N */
void x_textBlank( X_CALL_CTX *ctx )
{
   if( X_ISCHAR( ctx, 1 ) )
   {
      size_t length = IIF( x_parl( ctx, 2 ), x_parclen( ctx, 1 ), 0 );
      char *result = ( char * )x_xgrab( length + 1 );

      memset( result, ' ', length );
      result[length] = '\0';
      x_storclen( ctx, result, length, 1 );
      x_retclen_buffer( ctx, result, length );
   }
   else if( X_ISNUM( ctx, 1 ) )
   {
      x_stornint( ctx, 0, 1 );
      x_retni( ctx, 0 );
   }
   else if( X_ISDATE( ctx, 1 ) )
   {
      x_stordl( ctx, 0, 1 );
      x_retdl( ctx, 0 );
   }
   else if( X_ISHASH( ctx, 1 ) )
      x_hashNew( ctx->result );
   else if( X_ISARRAY( ctx, 1 ) )
      x_arrayNew( ctx->result, 0 );
   else
   {
      if( X_ISLOG( ctx, 1 ) )
         x_storl( ctx, 0, 1 );

      x_retl( ctx, 0 );
   }
}

/* Complement(): create the complement of a value of type C, D, L, M or N */
void x_textComplement( X_CALL_CTX *ctx )
{
   if( X_ISCHAR( ctx, 1 ) )
   {
      const char *text = x_parc( ctx, 1 );
      size_t length = x_parclen( ctx, 1 );
      char *result = ( char * )x_xgrab( length + 1 );
      size_t i;

      for( i = 0; i < length; ++i )
         result[i] = ( char )~text[i];

      result[length] = '\0';
      x_storclen( ctx, result, length, 1 );
      x_retclen_buffer( ctx, result, length );
   }
   else if( X_ISLOG( ctx, 1 ) )
      x_retl( ctx, !x_parl( ctx, 1 ) );
   else if( X_ISDATE( ctx, 1 ) )
   {
      /* A date is mirrored inside the span from day 0, the day before 0000-01-01, to 3000-01-01: day n becomes
         day ( span - n ); the empty date is day 0, so it pairs with 3000-01-01 and 1900-01-01 pairs with 1099-12-31 */
      long zero = x_dateEncode( 0, 1, 1 ) - 1;
      long span = x_dateEncode( 3000, 1, 1 ) - zero;
      long day = IIF( x_pardl( ctx, 1 ) == 0, 0, x_pardl( ctx, 1 ) - zero );
      long result = span - day;

      x_retdl( ctx, IIF( result == 0, 0, zero + result ) );
   }
   else if( X_IS_DOUBLE( x_paritem( ctx, 1 ) ) || X_IS_UINT64( x_paritem( ctx, 1 ) ) )
      x_retnd( ctx, -x_parnd( ctx, 1 ) );
   else if( x_parnint( ctx, 1 ) == INT64_MIN )
      x_retnd( ctx, -x_parnd( ctx, 1 ) );
   else
      x_retnint( ctx, -x_parnint( ctx, 1 ) );
}

/* Descend(): create a descending index key value */
void x_textDescend( X_CALL_CTX *ctx )
{
   const X_ITEM *item = x_paritem( ctx, 1 );

   if( X_IS_STRING( item ) )
   {
      const char *text = x_parc( ctx, 1 );
      size_t length = x_parclen( ctx, 1 );
      char *buffer = ( char * )x_xgrab( length + 1 );
      size_t i;

      for( i = 0; i < length; ++i )
         buffer[i] = ( char )( 255 - ( unsigned char )text[i] );

      buffer[length] = '\0';

      x_retclen_buffer( ctx, buffer, length );
   }
   else if( X_IS_DOUBLE( item ) )
      x_retnd( ctx, -x_parnd( ctx, 1 ) );
   else
      x_retnint( ctx, -x_parnint( ctx, 1 ) );
}

/* Expand(): insert characters between all characters of a string */
void x_textExpand( X_CALL_CTX *ctx )
{
   /* The second argument may already be the character: Expand( "123", "." ) */
   int64_t count = IIF( X_ISNUM( ctx, 2 ), x_parnint( ctx, 2 ), 1 );
   const char *text = x_parc( ctx, 1 );
   size_t length = x_parclen( ctx, 1 );
   size_t chars = x_utf8Length( text, length );
   size_t total, i, next, j = 0;
   uint32_t codepoint;
   X_UCHAR c;
   char *result;

   /* The characters are UTF-8 characters, and c may be any of them */
   x_ucharArg( ctx, IIF( X_ISCHAR( ctx, 2 ), 2, 3 ), &c );
   if( chars < 2 || count < 1 )
   {
      x_retclen( ctx, text, length );
      return;
   }

   total = length + ( chars - 1 ) * ( size_t )count * c.size;
   result = ( char * )x_xgrab( total + 1 );
   for( i = 0; i < length; i = next )
   {
      next = i;
      x_utf8Decode( text, length, &next, &codepoint );
      memcpy( result + j, text + i, next - i );
      j += next - i;
      if( next < length )
         j = ( size_t )( x_ucharFill( result + j, &c, ( size_t )count ) - result );
   }

   result[total] = '\0';
   x_retclen_buffer( ctx, result, total );
}

/* JustLeft(): justify the characters of a string to the left */
void x_textJustLeft( X_CALL_CTX *ctx )
{
   const char *text = x_parc( ctx, 1 );
   size_t length = x_parclen( ctx, 1 );
   X_UCHAR c;
   size_t moved;
   char *result = ( char * )x_xgrab( length + 1 );

   /* The run of c at the beginning moves to the end; c may be any UTF-8 character */
   x_ucharArg( ctx, 2, &c );
   moved = x_ucharRun( text, length, &c, 0 ) * c.size;
   memcpy( result, text + moved, length - moved );
   memcpy( result + length - moved, text, moved );
   result[length] = '\0';
   x_retclen_buffer( ctx, result, length );
}

/* JustRight(): justify the characters of a string to the right */
void x_textJustRight( X_CALL_CTX *ctx )
{
   const char *text = x_parc( ctx, 1 );
   size_t length = x_parclen( ctx, 1 );
   X_UCHAR c;
   size_t moved;
   char *result = ( char * )x_xgrab( length + 1 );

   /* The run of c at the end moves to the beginning; c may be any UTF-8 character */
   x_ucharArg( ctx, 2, &c );
   moved = x_ucharRun( text, length, &c, 1 ) * c.size;
   memcpy( result, text + length - moved, moved );
   memcpy( result + moved, text, length - moved );
   result[length] = '\0';
   x_retclen_buffer( ctx, result, length );
}

/* Left(): extract substring beginning with first character in a string */
void x_textLeft( X_CALL_CTX *ctx )
{
   const char *text = x_parc( ctx, 1 );
   size_t length = x_parclen( ctx, 1 );
   int64_t count = x_parnint( ctx, 2 );

   if( count < 0 )
      count = 0;

   if( ( uint64_t )count > length )
      count = ( int64_t )length;

   x_retclen( ctx, text, ( size_t )count );
}

/* Len(): the length of a string, or the number of elements of an array or a hash */
void x_textLen( X_CALL_CTX *ctx )
{
   const X_ITEM *item = x_paritem( ctx, 1 );

   if( item != NULL && item->nType == X_IT_ARRAY )
   {
      x_retnint( ctx, x_arrayLen( item ) );
      return;
   }

   if( item != NULL && item->nType == X_IT_HASH )
   {
      x_retnint( ctx, ( int64_t )x_hashLen( item ) );
      return;
   }

   if( item == NULL || item->nType != X_IT_STRING )
      ( void )x_errorRaise( ctx->result, 1111, item, NULL );
   else
      x_retnint( ctx, ( int64_t )x_parclen( ctx, 1 ) );
}

/* MemoLine(): extract a line of text from a character string or memo field */
void x_textMemoLine( X_CALL_CTX *ctx )
{
   const char *text = x_parc( ctx, 1 );
   size_t len = x_parclen( ctx, 1 ), pos = 0;
   int64_t width = x_parnintdef( ctx, 2, 79 );
   size_t tab = ( size_t )x_parnintdef( ctx, 4, 4 );
   int wrap = x_parldef( ctx, 5, 1 );
   int64_t line = x_parnintdef( ctx, 3, 1 );

   if( width <= 0 || line < 1 )
   {
      x_retc( ctx, "" );
      return;
   }
   while( --line )
   {
      if( pos >= len )
         break;

      x_memoNext( text, len, &pos, ( size_t )width, tab, wrap, NULL, NULL );
   }

   if( line != 0 )
   {
      x_retc( ctx, "" );
      return;
   }

   /* The width counts characters: a UTF-8 character takes up to 4 bytes, the spaces after the text one each */
   char *buffer = ( char * )x_xgrab( ( size_t )width * 4 + 1 );
   size_t used = 0;
   size_t cols = x_memoNext( text, len, &pos, ( size_t )width, tab, wrap, buffer, &used );
   size_t total = used + ( size_t )width - cols;

   memset( buffer + used, ' ', ( size_t )width - cols );
   buffer[total] = '\0';
   x_retclen_buffer( ctx, buffer, total );
}

/* MemoTran(): replace carriage return/linefeeds in character strings */
void x_textMemoTran( X_CALL_CTX *ctx )
{
   size_t i;

   const char *text = x_parc( ctx, 1 );
   size_t length = x_parclen( ctx, 1 );
   char hardCR = IIF( X_ISCHAR( ctx, 2 ), x_parc( ctx, 2 )[0], ';' );
   char softCR = IIF( X_ISCHAR( ctx, 3 ), x_parc( ctx, 3 )[0], ' ' );
   char *buffer = ( char * )x_xgrab( length + 1 );
   size_t pos = 0;

   for( i = 0; i < length; ++i )
   {
      int eol = i + 1 < length && text[i + 1] == '\n';

      if( eol && text[i] == '\r' )
      {
         buffer[pos++] = hardCR;
         ++i;
      }
      else if( eol && text[i] == '\x8D' && ( i == 0 || ( unsigned char )text[i - 1] < 0x80 ) )
      {
         buffer[pos++] = softCR;
         ++i;
      }
      else
         buffer[pos++] = text[i];
   }

   buffer[pos] = '\0';

   x_retclen_buffer( ctx, buffer, pos );
}

/* MLCount(): count the number of lines in a character string or memo field */
void x_textMLCount( X_CALL_CTX *ctx )
{
   const char *text = x_parc( ctx, 1 );
   size_t len = x_parclen( ctx, 1 ), pos = 0;
   int64_t width = x_parnintdef( ctx, 2, 79 );
   size_t tab = ( size_t )x_parnintdef( ctx, 3, 4 );
   int wrap = x_parldef( ctx, 4, 1 );
   size_t count = 0;

   if( width > 0 )
   {
      while( pos < len )
      {
         x_memoNext( text, len, &pos, ( size_t )width, tab, wrap, NULL, NULL );
         ++count;
      }
   }

   x_retns( ctx, ( ptrdiff_t )count );
}

/* MLCToPos(): return position of a string based on line and column position */
void x_textMLCToPos( X_CALL_CTX *ctx )
{
   const char *text = x_parc( ctx, 1 );
   size_t len = x_parclen( ctx, 1 );
   int64_t width = x_parnint( ctx, 2 );
   int64_t line = x_parnint( ctx, 3 );
   int64_t col = x_parnint( ctx, 4 );
   size_t tab = ( size_t )x_parnintdef( ctx, 5, 4 );
   int wrap = x_parldef( ctx, 6, 1 );
   size_t pos = 0;

   if( width > 4 && line > 0 )
   {
      while( --line && pos < len )
         x_memoNext( text, len, &pos, ( size_t )width, tab, wrap, NULL, NULL );

      if( line == 0 )
      {
         size_t start = pos;
         size_t next = pos;
         size_t scan;

         x_memoNext( text, len, &next, ( size_t )width, tab, wrap, NULL, NULL );
         while( pos < next )
         {
            uint32_t codepoint;

            scan = start;
            if( ( int64_t )x_memoNext( text, pos, &scan, ( size_t )width, tab, wrap, NULL, NULL ) >= col )
               break;

            x_utf8Decode( text, len, &pos, &codepoint );
         }
      }
   }

   /* The position counts characters */
   x_retns( ctx, ( ptrdiff_t )x_utf8Length( text, pos ) + 1 );
}

/* MLPos(): determine the position of a line in a string or memo field */
void x_textMLPos( X_CALL_CTX *ctx )
{
   const char *text = x_parc( ctx, 1 );
   size_t len = x_parclen( ctx, 1 ), pos = 0;
   int64_t width = x_parnint( ctx, 2 );
   size_t tab = ( size_t )x_parnintdef( ctx, 4, 4 );
   int wrap = x_parldef( ctx, 5, 1 );
   int64_t line = x_parnint( ctx, 3 );

   if( width <= 0 || line < 1 )
   {
      x_retns( ctx, 0 );
      return;
   }
   while( --line )
   {
      if( pos >= len )
         break;

      x_memoNext( text, len, &pos, ( size_t )width, tab, wrap, NULL, NULL );
   }

   /* The position counts characters */
   x_retns( ctx, ( ptrdiff_t )IIF( line <= 1, x_utf8Length( text, pos ) + ( pos < len ), 0 ) );
}

/* MPosToLC(): return line/ column position of a string based on a position */
void x_textMPosToLC( X_CALL_CTX *ctx )
{
   const char *text = x_parc( ctx, 1 );
   size_t len = x_parclen( ctx, 1 );
   int64_t width = x_parnint( ctx, 2 );
   int64_t target = x_parnint( ctx, 3 ) - 1;
   size_t tab = ( size_t )x_parnintdef( ctx, 4, 4 );
   int wrap = x_parldef( ctx, 5, 1 );
   size_t pos = 0;
   size_t start = 0;
   int64_t line = 0;
   size_t col = 0;

   /* nPos counts characters; target becomes the byte offset of that character */
   if( width > 0 && target >= 0 && ( size_t )target <= x_utf8Length( text, len ) )
   {
      target = ( int64_t )x_utf8Offset( text, len, ( size_t )target );
      do
      {
         start = pos;
         x_memoNext( text, len, &pos, ( size_t )width, tab, wrap, NULL, NULL );
         ++line;
      }
      while( pos <= ( size_t )target && pos < len );
      col = x_memoNext( text, ( size_t )target, &start, ( size_t )width, tab, wrap, NULL, NULL );
   }

   x_reta( ctx, 2 );
   x_arraySetNInt( ctx->result, 1, line );
   x_arraySetNInt( ctx->result, 2, ( int64_t )col );
}

/* NumAt(): count multiple occurrences of a substring within a string */
void x_textNumAt( X_CALL_CTX *ctx )
{
   const char *search = x_parc( ctx, 1 );
   size_t searchLength = x_parclen( ctx, 1 );
   const char *text = x_parc( ctx, 2 );
   size_t textLength = x_parclen( ctx, 2 );
   int64_t ignored = x_parnintdef( ctx, 3, 0 );
   size_t offset = ( size_t )IIF( ignored > 0, ignored, 0 );
   int64_t count = 0;

   while( searchLength > 0 && offset + searchLength <= textLength )
   {
      if( memcmp( text + offset, search, searchLength ) == 0 )
      {
         ++count;
         offset += searchLength;
      }
      else
         ++offset;
   }

   x_retnint( ctx, count );
}

/* Occurs(): count the occurrences of a substring, also those that overlap */
void x_textOccurs( X_CALL_CTX *ctx )
{
   const char *search = x_parc( ctx, 1 );
   size_t searchLength = x_parclen( ctx, 1 );
   const char *text = x_parc( ctx, 2 );
   size_t textLength = x_parclen( ctx, 2 );
   size_t offset;
   int64_t count = 0;

   for( offset = 0; searchLength > 0 && offset + searchLength <= textLength; ++offset )
   {
      if( memcmp( text + offset, search, searchLength ) == 0 )
         ++count;
   }

   x_retnint( ctx, count );
}

/* PadLeft(): pad a character string on the left */
void x_textPadLeft( X_CALL_CTX *ctx )
{
   const char *text = x_parc( ctx, 1 );
   size_t length = x_parclen( ctx, 1 );
   size_t chars = x_utf8Length( text, length );
   int64_t width = x_parnint( ctx, 2 );
   X_UCHAR c;
   size_t skip;

   x_ucharArg( ctx, 3, &c );

   /* nLength counts characters: a shorter one keeps the last characters, a negative one gives "" */
   if( width > 0 && ( size_t )width < chars )
   {
      skip = x_utf8Offset( text, length, chars - ( size_t )width );
      x_retclen( ctx, text + skip, length - skip );
      return;
   }

   x_utf8Pad( ctx, text, length, width, c.bytes, c.size, 1, 0, "PADLEFT" );
}

/* RAt(): return the position of the last occurrence of a substring */
void x_textRAt( X_CALL_CTX *ctx )
{
   const char *text = x_parc( ctx, 2 );
   size_t searchLen = x_parclen( ctx, 1 );
   size_t textLen = x_parclen( ctx, 2 );
   int64_t start = x_parnint( ctx, 3 );
   int64_t end = IIF( x_pcount( ctx ) >= 4, x_parnint( ctx, 4 ), ( int64_t )textLen );

   if( start < 1 )
      start = 1;

   if( end > ( int64_t )textLen )
      end = ( int64_t )textLen;

   if( start > end )
   {
      x_retni( ctx, 0 );
      return;
   }

   size_t found = x_strRAt( x_parc( ctx, 1 ), searchLen, text + ( start - 1 ), ( size_t )( end - start + 1 ) );

   x_retni( ctx, IIF( found, ( int )( found + ( size_t )( start - 1 ) ), 0 ) );
}

/* Replicate(): return a string repeated a specified number of times */
void x_textReplicate( X_CALL_CTX *ctx )
{
   int64_t i;

   const char *text = x_parc( ctx, 1 );
   size_t textLen = x_parclen( ctx, 1 );
   int64_t count = x_parnint( ctx, 2 );

   if( count < 0 )
      count = 0;

   char *buffer = ( char * )IIF( textLen != 0, x_xgrabArray( ( size_t )count + 1, textLen ), x_xgrab( 1 ) );
   size_t resultLength = ( size_t )count * textLen;

   for( i = 0; i < count; ++i )
      memcpy( buffer + ( size_t )i * textLen, text, textLen );

   buffer[resultLength] = '\0';

   x_retclen_buffer( ctx, buffer, resultLength );
}

/* Right(): return a substring beginning with the rightmost character */
void x_textRight( X_CALL_CTX *ctx )
{
   const char *text = x_parc( ctx, 1 );
   size_t length = x_parclen( ctx, 1 );
   int64_t count = x_parnint( ctx, 2 );

   if( count < 0 )
      count = 0;

   if( ( uint64_t )count > length )
      count = ( int64_t )length;

   x_retclen( ctx, text + ( length - ( size_t )count ), ( size_t )count );
}

/* SoundEx(): convert a character string to "soundex" form */
void x_textSoundEx( X_CALL_CTX *ctx )
{
   static const char codes[] = "01230120022455012623010202";
   size_t i;

   const char *text = x_parc( ctx, 1 );
   size_t length = x_parclen( ctx, 1 );
   char result[] = "0000";
   char previous = '0';
   size_t pos = 0;

   for( i = 0; i < length && pos < 4; ++i )
   {
      char c = ( char )toupper( ( unsigned char )text[i] );

      if( c >= 'A' && c <= 'Z' )
      {
         char code = codes[c - 'A'];

         if( pos == 0 )
            result[pos++] = c;
         else if( code != '0' && code != previous )
            result[pos++] = code;

         previous = code;
      }
   }

   x_retclen( ctx, result, 4 );
}

/* StrDiff(): calculate the similarity of two strings */
void x_textStrDiff( X_CALL_CTX *ctx )
{
   const char *text1 = x_parc( ctx, 1 );
   const char *text2 = x_parc( ctx, 2 );
   size_t length1 = x_parclen( ctx, 1 );
   size_t length2 = x_parclen( ctx, 2 );
   int64_t replace = x_parnintdef( ctx, 3, 3 );
   int64_t remove = x_parnintdef( ctx, 4, 6 );
   int64_t insert = x_parnintdef( ctx, 5, 1 );
   int64_t *before = ( int64_t * )x_xgrabArray( length2 + 1, sizeof( int64_t ) );
   int64_t *current = ( int64_t * )x_xgrabArray( length2 + 1, sizeof( int64_t ) );
   size_t i, j;

   /* The cheapest way to turn cString1 into cString2 by replacing, deleting and inserting characters */
   for( j = 0; j <= length2; ++j )
      before[j] = ( int64_t )j * insert;

   for( i = 1; i <= length1; ++i )
   {
      int64_t *swap;

      current[0] = ( int64_t )i * remove;
      for( j = 1; j <= length2; ++j )
      {
         int64_t cost = before[j - 1] + IIF( text1[i - 1] == text2[j - 1], 0, replace );

         if( before[j] + remove < cost )
            cost = before[j] + remove;

         if( current[j - 1] + insert < cost )
            cost = current[j - 1] + insert;

         current[j] = cost;
      }

      swap = before;
      before = current;
      current = swap;
   }

   x_retnint( ctx, before[length2] );
   x_xfree( before );
   x_xfree( current );
}

/* StrSwap(): exchange characters between two strings */
void x_textStrSwap( X_CALL_CTX *ctx )
{
   size_t length1 = x_parclen( ctx, 1 );
   size_t length2 = x_parclen( ctx, 2 );
   size_t common = IIF( length1 < length2, length1, length2 );
   char *text1 = ( char * )x_xmemdup( x_parc( ctx, 1 ), length1 + 1 );
   char *text2 = ( char * )x_xmemdup( x_parc( ctx, 2 ), length2 + 1 );
   size_t i;

   /* Only the arguments passed by reference change */
   for( i = 0; i < common; ++i )
   {
      char c = text1[i];

      text1[i] = text2[i];
      text2[i] = c;
   }

   x_storclen( ctx, text1, length1, 1 );
   x_storclen( ctx, text2, length2, 2 );
   x_xfree( text1 );
   x_xfree( text2 );
   x_retc( ctx, "" );
}

/* StrTran(): search and replace characters within a string or memo field */
void x_textStrTran( X_CALL_CTX *ctx )
{
   int64_t start = IIF( x_pcount( ctx ) >= 4, x_parnint( ctx, 4 ), 1 );
   int64_t count = IIF( x_pcount( ctx ) >= 5, x_parnint( ctx, 5 ), -1 );

   if( start == 0 || count == 0 )
   {
      x_retclen( ctx, "", 0 );
      return;
   }

   const char *text = x_parc( ctx, 1 );
   const char *search = x_parc( ctx, 2 );
   size_t textLen = x_parclen( ctx, 1 );
   size_t searchLen = x_parclen( ctx, 2 );
   size_t replaceLen = x_parclen( ctx, 3 );
   const char *replace = IIF( replaceLen > 0, x_parc( ctx, 3 ), "" );

   if( searchLen == 0 || searchLen > textLen || start < 0 )
   {
      x_itemReturn( ctx, x_paritem( ctx, 1 ) );
      return;
   }

   if( count < 0 )
      count = INT64_MAX;

   size_t maxLen = textLen + IIF( replaceLen > searchLen, ( textLen / searchLen ) * ( replaceLen - searchLen ), 0 );
   char *buffer = ( char * )x_xgrab( maxLen + 1 );
   size_t outLen = 0;
   size_t pos = 0;
   size_t occurrence = 0;
   size_t replaced = 0;
   size_t found;

   while( replaced < ( size_t )count && ( found = x_strAt( search, searchLen, text + pos, textLen - pos ) ) != 0 )
   {
      memcpy( buffer + outLen, text + pos, found - 1 );
      outLen += found - 1;
      pos += found - 1 + searchLen;

      if( ++occurrence >= ( size_t )start )
      {
         memcpy( buffer + outLen, replace, replaceLen );
         outLen += replaceLen;
         ++replaced;
      }
      else
      {
         memcpy( buffer + outLen, search, searchLen );
         outLen += searchLen;
      }
   }

   memcpy( buffer + outLen, text + pos, textLen - pos );
   outLen += textLen - pos;
   buffer[outLen] = '\0';

   x_retclen_buffer( ctx, buffer, outLen );
}

/* Stuff(): delete and insert characters in a string */
void x_textStuff( X_CALL_CTX *ctx )
{
   const char *text = x_parc( ctx, 1 );
   const char *insert = x_parc( ctx, 4 );
   size_t textLen = x_parclen( ctx, 1 );
   size_t insertLen = x_parclen( ctx, 4 );
   int64_t start = x_parnint( ctx, 2 );
   int64_t removeLen = x_parnint( ctx, 3 );
   size_t head, removeCount, total;
   char *buffer;

   if( start == 0 )
      head = 0;
   else if( start < 0 || ( uint64_t )start > textLen )
      head = textLen;
   else
      head = ( size_t )start - 1;

   if( removeLen == 0 )
      removeCount = 0;
   else if( removeLen < 0 || ( uint64_t )removeLen > textLen - head )
      removeCount = textLen - head;
   else
      removeCount = ( size_t )removeLen;

   total = textLen - removeCount + insertLen;
   buffer = ( char * )x_xgrab( total + 1 );

   memcpy( buffer, text, head );
   memcpy( buffer + head, insert, insertLen );
   memcpy( buffer + head + insertLen, text + head + removeCount, textLen - head - removeCount );
   buffer[total] = '\0';

   x_retclen_buffer( ctx, buffer, total );
}

/* SubStr(): extract a substring from a character string */
void x_textSubStr( X_CALL_CTX *ctx )
{
   const char *text = x_parc( ctx, 1 );
   int64_t length = ( int64_t )x_parclen( ctx, 1 );
   int64_t start = x_parnint( ctx, 2 );

   if( start < 0 )
   {
      start = length + start + 1;
      if( start < 1 )
         start = 1;
   }
   else if( start == 0 )
      start = 1;

   if( start > length )
   {
      x_retclen( ctx, "", 0 );
      return;
   }

   int64_t count = IIF( x_pcount( ctx ) >= 3, x_parnint( ctx, 3 ), length - start + 1 );

   if( count < 0 )
      count = 0;

   if( start - 1 + count > length )
      count = length - ( start - 1 );

   x_retclen( ctx, text + ( start - 1 ), ( size_t )count );
}

/* TabExpand(): replace a tab with a specified number of another character */
void x_textTabExpand( X_CALL_CTX *ctx )
{
   /* The second argument may already be the fill character: TabExpand( cText, "+" ) */
   size_t width = ( size_t )IIF( X_ISNUM( ctx, 2 ), x_parnint( ctx, 2 ), 8 );
   const char *text = x_parc( ctx, 1 );
   size_t length = x_parclen( ctx, 1 );
   size_t total = 0, column = 0, i, next, j = 0;
   uint32_t codepoint;
   X_UCHAR c;
   char *result;

   /* The columns count characters; the fill character may be any UTF-8 character */
   x_ucharArg( ctx, IIF( X_ISCHAR( ctx, 2 ), 2, 3 ), &c );
   for( i = 0; i < length; i = next )
   {
      size_t step = IIF( text[i] == '\t', width - column % width, 1 );

      next = i;
      x_utf8Decode( text, length, &next, &codepoint );
      total += IIF( text[i] == '\t', step * c.size, next - i );
      column = IIF( text[i] == '\r' || text[i] == '\n', 0, column + step );
   }

   result = ( char * )x_xgrab( total + 1 );
   column = 0;
   for( i = 0; i < length; i = next )
   {
      next = i;
      x_utf8Decode( text, length, &next, &codepoint );
      if( text[i] == '\t' )
      {
         size_t step = width - column % width;

         x_ucharFill( result + j, &c, step );
         j += step * c.size;
         column += step;
      }
      else
      {
         memcpy( result + j, text + i, next - i );
         j += next - i;
         column = IIF( text[i] == '\r' || text[i] == '\n', 0, column + 1 );
      }
   }

   result[total] = '\0';
   x_retclen_buffer( ctx, result, total );
}

/* TabPack(): insert a Tab (Chr(9)) for multiple occurrences of a character */
void x_textTabPack( X_CALL_CTX *ctx )
{
   /* The second argument may already be the packed character: TabPack( cText, "*" ) */
   size_t width = ( size_t )IIF( X_ISNUM( ctx, 2 ), x_parnint( ctx, 2 ), 8 );
   const char *text = x_parc( ctx, 1 );
   size_t length = x_parclen( ctx, 1 );
   char *result = ( char * )x_xgrab( length + 1 );
   size_t run = 0, column = 0, i = 0, next, j = 0;
   uint32_t codepoint;
   X_UCHAR c;

   /* The columns count characters and c may be any UTF-8 character; at least two c that end at a tab position
      become one tab */
   x_ucharArg( ctx, IIF( X_ISCHAR( ctx, 2 ), 2, 3 ), &c );
   while( i < length )
   {
      if( x_ucharAt( text, length, i, &c ) )
      {
         i += c.size;
         ++run;
         if( ++column % width == 0 )
         {
            if( run >= 2 )
               result[j++] = '\t';
            else
               j = ( size_t )( x_ucharFill( result + j, &c, 1 ) - result );

            run = 0;
         }
         continue;
      }

      j = ( size_t )( x_ucharFill( result + j, &c, run ) - result );
      run = 0;
      next = i;
      x_utf8Decode( text, length, &next, &codepoint );
      memcpy( result + j, text + i, next - i );
      j += next - i;
      if( text[i] == '\t' )
         column += width - column % width;
      else
         column = IIF( text[i] == '\r' || text[i] == '\n', 0, column + 1 );

      i = next;
   }

   j = ( size_t )( x_ucharFill( result + j, &c, run ) - result );
   result[j] = '\0';
   x_retclen_buffer( ctx, result, j );
}

/* UChars(): the code points of a UTF-8 string as strings; a combining mark and an invalid byte are own elements */
void x_textUChars( X_CALL_CTX *ctx )
{
   const char *text;
   size_t length, offset, start, index;
   uint32_t codepoint;

   text = x_parc( ctx, 1 );
   length = x_parclen( ctx, 1 );
   x_arrayNew( ctx->result, ( int64_t )x_utf8Length( text, length ) );
   offset = 0;
   index = 1;
   while( offset < length )
   {
      start = offset;
      x_utf8Decode( text, length, &offset, &codepoint );
      x_arraySetCL( ctx->result, index++, text + start, offset - start );
   }
}
