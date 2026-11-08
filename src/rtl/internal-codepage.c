/*
 * Copyright © 2024-2026 Rafał Jopek
 */

#include "internal.h"
#include <string.h>

static X_CODEPAGE s_utf8 = { "UTF-8", 65001, NULL, NULL, 48, NULL };
static X_CODEPAGE *s_codepages = &s_utf8;

static int s_nameMatches( const char *registered, const char *name, size_t length )
{
   return registered && strlen( registered ) == length && x_strnicmp( registered, name, length ) == 0;
}

const X_CODEPAGE *x_codepageFind( const char *name, size_t length )
{
   const X_CODEPAGE *codepage;

   if( !name || !length )
      return NULL;

   for( codepage = s_codepages; codepage; codepage = codepage->next )
      if( s_nameMatches( codepage->name, name, length ) )
         return codepage;

   return NULL;
}

void x_codepageRegister( X_CODEPAGE *codepage )
{
   X_CODEPAGE **entry;

   if( !codepage || !codepage->name || x_codepageFind( codepage->name, strlen( codepage->name ) ) )
      return;

   for( entry = &s_codepages; *entry && ( *entry )->order < codepage->order; entry = &( *entry )->next )
      ;
   codepage->next = *entry;
   *entry = codepage;
}

const X_CODEPAGE *x_codepageFindNumber( int number )
{
   const X_CODEPAGE *codepage;

   if( number > 0 )
      for( codepage = s_codepages; codepage; codepage = codepage->next )
         if( codepage->number == number )
            return codepage;

   return NULL;
}

void x_codepageList( X_ITEM *result )
{
   const X_CODEPAGE *codepage;
   size_t count = 0, index = 0;

   for( codepage = s_codepages; codepage; codepage = codepage->next )
      ++count;

   x_arrayNew( result, ( int64_t )count );
   for( codepage = s_codepages; codepage; codepage = codepage->next )
      x_arraySetC( result, ++index, codepage->name );
}

const char *x_codepageName( const X_CODEPAGE *codepage )
{
   return codepage ? codepage->name : "";
}

int x_codepageNumber( const X_CODEPAGE *codepage )
{
   return codepage ? codepage->number : 0;
}

uint16_t x_codepageDecode( const X_CODEPAGE *codepage, const unsigned char *text, size_t length, size_t *offset )
{
   unsigned char byte = text[( *offset )++];
   uint16_t      codepoint;

   if( byte < 128 )
      return IIF( byte == 0x25 && codepage && codepage->number == 864, 0x066A, byte );

   if( !codepage || !codepage->unicode )
      return 0xFFFD;

   if( codepage->doubleByte && *offset < length )
   {
      codepoint = codepage->doubleByte[( ( size_t )( byte - 128 ) << 8 ) | text[*offset]];
      if( codepoint )
      {
         ++( *offset );
         return codepoint;
      }
   }

   return codepage->unicode[byte - 128];
}

static char *s_codepageToUTF8( const X_CODEPAGE *codepage, const char *text, size_t length, size_t *resultLength )
{
   const unsigned char *source = ( const unsigned char * )text;
   size_t               offset = 0, total = 0, size, written = 0;
   uint16_t             codepoint;
   char                *result;

   while( offset < length )
   {
      codepoint = x_codepageDecode( codepage, source, length, &offset );
      if( codepoint == 0xFFFD )
         return NULL;

      size = x_utf8CharSize( codepoint );
      if( size > SIZE_MAX - total - 1 )
         return NULL;

      total += size;
   }

   result = ( char * )x_xgrab( total + 1 );

   offset = 0;
   while( offset < length )
   {
      codepoint = x_codepageDecode( codepage, source, length, &offset );
      written += x_utf8Encode( result + written, codepoint );
   }

   result[written] = '\0';
   *resultLength = written;

   return result;
}

static uint16_t *s_codepageReverse( const X_CODEPAGE *codepage )
{
   uint16_t *reverse = ( uint16_t * )x_xgrabzArray( 65536, sizeof( uint16_t ) );
   uint16_t  codepoint;
   size_t    index;

   for( index = 1; index < 128; ++index )
      reverse[index] = ( uint16_t )index;

   if( codepage->number == 864 )
   {
      reverse[0x25] = 0;
      reverse[0x066A] = 0x25;
   }

   for( index = 0; index < 128; ++index )
   {
      codepoint = codepage->unicode[index];
      if( codepoint != 0xFFFD && !reverse[codepoint] )
         reverse[codepoint] = ( uint16_t )( index + 128 );
   }

   if( codepage->doubleByte )
      for( index = 0; index < 32768; ++index )
      {
         codepoint = codepage->doubleByte[index];
         if( codepoint && codepoint != 0xFFFD && !reverse[codepoint] )
            reverse[codepoint] = ( uint16_t )( index + 32768 );
      }

   return reverse;
}

static uint16_t s_codepageEncode( const X_CODEPAGE *codepage, uint16_t codepoint, uint16_t **reverse )
{
   if( codepoint < 128 && !( codepoint == 0x25 && codepage->number == 864 ) )
      return codepoint;

   if( !*reverse )
      *reverse = s_codepageReverse( codepage );

   return ( *reverse )[codepoint];
}

static char *s_codepageFromUTF8( const X_CODEPAGE *codepage, const char *text, size_t length, size_t *resultLength )
{
   char     *result = ( char * )x_xgrab( length + 1 );
   uint16_t *reverse = NULL;
   uint16_t  encoded;
   uint32_t  codepoint;
   size_t    offset = 0, written = 0;

   while( offset < length )
   {
      if( !x_utf8Decode( text, length, &offset, &codepoint ) )
         goto invalid;

      if( codepoint > 0xFFFF )
         goto invalid;

      encoded = s_codepageEncode( codepage, ( uint16_t )codepoint, &reverse );

      if( !encoded && codepoint )
         goto invalid;

      if( encoded >= 256 )
         result[written++] = ( char )( encoded >> 8 );

      result[written++] = ( char )( encoded & 0xFF );
   }

   x_xfree( reverse );
   result[written] = '\0';
   *resultLength = written;

   return result;

   invalid:
   x_xfree( reverse );
   x_xfree( result );

   return NULL;
}

static int s_codepageResolve( const char *name, size_t length, const X_CODEPAGE **codepage )
{
   *codepage = NULL;
   if( s_nameMatches( "UTF-8", name, length ) )
      return 1;

   *codepage = x_codepageFind( name, length );
   return *codepage != NULL;
}

static int s_codepageValidate( const X_CODEPAGE *codepage, const char *text, size_t length )
{
   size_t offset = 0;

   if( !codepage )
      return x_utf8Validate( text, length );

   while( offset < length )
      if( x_codepageDecode( codepage, ( const unsigned char * )text, length, &offset ) == 0xFFFD )
         return 0;

   return 1;
}

static char *s_codepageTranscode( const X_CODEPAGE *from, const X_CODEPAGE *to, const char *text, size_t length, size_t *resultLength )
{
   const unsigned char *source = ( const unsigned char * )text;
   uint16_t            *reverse = NULL;
   uint16_t             codepoint, encoded;
   size_t               offset = 0, total = 0, size, written = 0;
   char                *result;

   while( offset < length )
   {
      codepoint = x_codepageDecode( from, source, length, &offset );
      if( codepoint == 0xFFFD )
         goto invalid;

      encoded = s_codepageEncode( to, codepoint, &reverse );
      if( !encoded && codepoint )
         goto invalid;

      size = IIF( encoded >= 256, 2, 1 );
      if( size > SIZE_MAX - total - 1 )
         goto invalid;

      total += size;
   }

   result = ( char * )x_xgrab( total + 1 );
   offset = 0;
   while( offset < length )
   {
      codepoint = x_codepageDecode( from, source, length, &offset );
      encoded = s_codepageEncode( to, codepoint, &reverse );
      if( encoded >= 256 )
         result[written++] = ( char )( encoded >> 8 );

      result[written++] = ( char )( encoded & 0xFF );
   }

   x_xfree( reverse );
   result[written] = '\0';
   *resultLength = written;
   return result;

   invalid:
   x_xfree( reverse );
   return NULL;
}

void x_codepageConvert( X_CALL_CTX *ctx, int toUTF8 )
{
   const char       *text = x_parc( ctx, 1 ), *name = x_parc( ctx, 2 );
   size_t            length = x_parclen( ctx, 1 ), nameLength = x_parclen( ctx, 2 ), resultLength;
   const char       *operation = IIF( toUTF8, "STRTOUTF8", "UTF8TOSTR" );
   const X_CODEPAGE *codepage;
   char             *result;

   if( !s_codepageResolve( name, nameLength, &codepage ) )
   {
      x_errorRaiseNamed( ctx->result, 1102, operation );
      return;
   }

   if( !codepage )
   {
      if( x_utf8Validate( text, length ) )
         x_retclen( ctx, text, length );
      else
         x_errorRaiseNamed( ctx->result, 1102, operation );
      return;
   }

   result = IIF( toUTF8, s_codepageToUTF8( codepage, text, length, &resultLength ),
                         s_codepageFromUTF8( codepage, text, length, &resultLength ) );

   if( result )
      x_retclen_buffer( ctx, result, resultLength );
   else
      x_errorRaiseNamed( ctx->result, 1102, operation );
}

void x_codepageTranslate( X_CALL_CTX *ctx )
{
   const char       *text = x_parc( ctx, 1 );
   size_t            length = x_parclen( ctx, 1 ), resultLength;
   const X_CODEPAGE *from, *to;
   char             *result;

   if( !s_codepageResolve( x_parc( ctx, 2 ), x_parclen( ctx, 2 ), &from ) ||
       !s_codepageResolve( x_parc( ctx, 3 ), x_parclen( ctx, 3 ), &to ) )
   {
      x_errorRaiseNamed( ctx->result, 1102, "TRANSLATE" );
      return;
   }

   if( from == to )
   {
      if( s_codepageValidate( from, text, length ) )
         x_retclen( ctx, text, length );
      else
         x_errorRaiseNamed( ctx->result, 1102, "TRANSLATE" );
      return;
   }

   if( !from )
      result = s_codepageFromUTF8( to, text, length, &resultLength );
   else if( !to )
      result = s_codepageToUTF8( from, text, length, &resultLength );
   else
      result = s_codepageTranscode( from, to, text, length, &resultLength );

   if( result )
      x_retclen_buffer( ctx, result, resultLength );
   else
      x_errorRaiseNamed( ctx->result, 1102, "TRANSLATE" );
}
