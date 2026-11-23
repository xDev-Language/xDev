/*
 * Copyright (c) 2024-2026 xDev-Language
 *
 * Library...: xDev 7.0.0 RDD
 * File......: dbf-value.c
 *
 */

#include "dbf-internal.h"

#include <inttypes.h>
#include <float.h>
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void s_textDrop( X_DBF_TEXT *text )
{
   x_dbfTextClear( text );
}

static int s_dateValid( int year, int month, int day, long *julian )
{
   int  decodedYear, decodedMonth, decodedDay;
   long encoded;

   if( year < 1 || year > 9999 || month < 1 || month > 12 || day < 1 || day > 31 )
      return 0;
   encoded = x_dateEncode( year, month, day );
   x_dateDecode( encoded, &decodedYear, &decodedMonth, &decodedDay );
   if( decodedYear != year || decodedMonth != month || decodedDay != day )
      return 0;
   *julian = encoded;
   return 1;
}

static X_DBF_STATUS s_getNumeric( const X_DBF_FIELD *field, const char *data, X_ITEM *value )
{
   size_t      begin = 0, end = field->length, position;
   uint64_t    magnitude = 0;
   long double number = 0.0L, scale = 1.0L;
   int         negative = 0, point = 0, digits = 0, overflow = 0;

   while( begin < end && data[begin] == ' ' )
      ++begin;
   while( end > begin && data[end - 1] == ' ' )
      --end;
   position = begin;
   while( position < end && data[position] == '*' )
      ++position;
   if( position == end )
   {
      value->nType = field->decimals ? X_IT_DOUBLE : X_IT_INTEGER;
      value->u.integer = 0;
      if( field->decimals )
         value->u.dbl = 0.0;
      return X_DBF_OK;
   }
   position = begin;
   if( data[position] == '-' || data[position] == '+' )
      negative = data[position++] == '-';
   for( ; position < end; ++position )
   {
      unsigned char character = ( unsigned char )data[position];
      if( character == '.' && !point )
         point = 1;
      else if( character >= '0' && character <= '9' )
      {
         unsigned int digit = character - '0';
         ++digits;
         number = number * 10.0L + digit;
         if( point )
            scale *= 10.0L;
         if( magnitude > ( UINT64_MAX - digit ) / 10 )
            overflow = 1;
         else if( !overflow )
            magnitude = magnitude * 10 + digit;
      }
      else
         return X_DBF_INVALID_FIELD_VALUE;
   }
   if( digits == 0 )
      return X_DBF_INVALID_FIELD_VALUE;
   if( !point && field->decimals == 0 && !overflow && magnitude <= ( uint64_t )INT64_MAX + ( unsigned int )negative )
   {
      value->nType = X_IT_INTEGER;
      if( negative && magnitude == ( uint64_t )INT64_MAX + 1 )
         value->u.integer = INT64_MIN;
      else
         value->u.integer = negative ? -( int64_t )magnitude : ( int64_t )magnitude;
   }
   else
   {
      value->nType = X_IT_DOUBLE;
      value->u.dbl = ( double )( ( negative ? -number : number ) / scale );
      if( !isfinite( value->u.dbl ) )
         return X_DBF_INVALID_FIELD_VALUE;
   }
   return X_DBF_OK;
}

X_DBF_STATUS x_dbfFieldGetValue( DBF area, size_t ordinal, X_ITEM *result )
{
   const X_DBF_FIELD *field;
   X_DBF_TEXT         text __attribute__( ( cleanup( s_textDrop ) ) ) = { NULL, 0 };
   X_ITEM             value = X_ITEM_NIL_INIT;
   const char        *data;
   X_DBF_STATUS       status = x_dbfAreaPrepare( area );

   if( status != X_DBF_OK )
      return status;
   field = x_dbfSchemaField( area->schema, ordinal );
   if( field == NULL )
      return X_DBF_INVALID_ARGUMENT;
   data = area->record + field->offset;
   switch( field->type )
   {
      case X_DBF_CHARACTER:
         status = x_dbfFieldGetText( area, ordinal, &text );
         if( status != X_DBF_OK )
            return status;
         x_itemPutStringBuffer( result, text.data, text.length );
         text.data = NULL;
         return X_DBF_OK;
      case X_DBF_NUMERIC:
         status = s_getNumeric( field, data, &value );
         if( status != X_DBF_OK )
            return status;
         break;
      case X_DBF_MEMO:
      case X_DBF_PICTURE:
      {
         int binary;
         if( area->driver->operations.memoValueRead != NULL )
            return area->driver->operations.memoValueRead( area, field, result );
         status = x_dbfFieldReadMemo( area, field, &text, &binary );
         if( status != X_DBF_OK )
            return status;
         x_itemPutStringBuffer( result, text.data, text.length );
         text.data = NULL;
         return X_DBF_OK;
      }
      case X_DBF_DATE:
      {
         int    parts[3] = { 0, 0, 0 };
         size_t i;
         long   julian = 0;

         value.nType = X_IT_DATE;
         if( memcmp( data, "        ", 8 ) != 0 && memcmp( data, "00000000", 8 ) != 0 )
         {
            for( i = 0; i < 8; ++i )
            {
               size_t part = i < 4 ? 0 : ( i < 6 ? 1 : 2 );
               if( data[i] < '0' || data[i] > '9' )
                  return X_DBF_INVALID_FIELD_VALUE;
               parts[part] = parts[part] * 10 + data[i] - '0';
            }
            if( !s_dateValid( parts[0], parts[1], parts[2], &julian ) )
               return X_DBF_INVALID_FIELD_VALUE;
         }
         value.u.integer = julian;
         break;
      }
      case X_DBF_LOGICAL:
         value.nType = X_IT_LOGICAL;
         value.u.logical = data[0] == 'T' || data[0] == 't' || data[0] == 'Y' || data[0] == 'y';
         break;
      default: return X_DBF_UNSUPPORTED_FORMAT;
   }
   x_itemCopy( result, &value );
   return X_DBF_OK;
}

static X_DBF_STATUS s_formatDouble( const X_DBF_FIELD *field, double value, char *output )
{
   char        scientific[32], digits[16], units[22];
   const char *exponent;
   size_t      i, count = 0, begin = 0, length, position, padding;
   int         written, kept, negative = value < 0.0, carry;

   if( !isfinite( value ) )
      return X_DBF_FIELD_OVERFLOW;
   written = snprintf( scientific, sizeof( scientific ), "%.15e", fabs( value ) );
   if( written < 0 || ( size_t )written >= sizeof( scientific ) )
      return X_DBF_INVALID_FIELD_VALUE;
   exponent = strchr( scientific, 'e' );
   if( exponent == NULL )
      return X_DBF_INVALID_FIELD_VALUE;
   for( i = 0; scientific + i < exponent; ++i )
   {
      if( scientific[i] >= '0' && scientific[i] <= '9' )
      {
         if( count == sizeof( digits ) )
            return X_DBF_INVALID_FIELD_VALUE;
         digits[count++] = scientific[i];
      }
   }
   if( count != sizeof( digits ) )
      return X_DBF_INVALID_FIELD_VALUE;
   kept = ( int )strtol( exponent + 1, NULL, 10 ) + 1 + field->decimals;
   if( kept > field->length )
      return X_DBF_FIELD_OVERFLOW;
   count = kept > 0 ? ( size_t )kept : 1;
   units[0] = '0';
   for( i = 0; i < count; ++i )
      units[i + 1] = IIF( kept > 0 && i < sizeof( digits ), digits[i], '0' );
   carry = kept >= 0 && kept < ( int )sizeof( digits ) && digits[kept] >= '5';
   for( i = count; carry; --i )
   {
      carry = units[i] == '9';
      units[i] = IIF( carry, '0', units[i] + 1 );
   }
   ++count;
   while( begin + 1 < count && units[begin] == '0' )
      ++begin;
   count -= begin;
   if( count == 1 && units[begin] == '0' )
      negative = 0;
   padding = count <= field->decimals ? field->decimals + 1 - count : 0;
   length = count + padding + negative + ( field->decimals != 0 );
   if( length > field->length )
      return X_DBF_FIELD_OVERFLOW;
   position = field->length - length;
   memset( output, ' ', position );
   if( negative )
      output[position++] = '-';
   for( i = 0; i < count + padding; ++i )
   {
      if( field->decimals != 0 && i == count + padding - field->decimals )
         output[position++] = '.';
      output[position++] = IIF( i < padding, '0', units[begin + i - padding] );
   }
   return X_DBF_OK;
}

X_DBF_STATUS x_dbfFormatNumeric( const X_DBF_FIELD *field, const X_ITEM *value, char *output )
{
   char   formatted[40];
   int    written;
   size_t length;

   if( value->nType == X_IT_INTEGER )
   {
      written = snprintf( formatted, sizeof( formatted ), "%" PRId64, value->u.integer );
      if( written < 0 || ( size_t )written >= sizeof( formatted ) )
         return X_DBF_INVALID_FIELD_VALUE;
      length = ( size_t )written;
      if( field->decimals )
      {
         formatted[length++] = '.';
         memset( formatted + length, '0', field->decimals );
         length += field->decimals;
         formatted[length] = '\0';
      }
   }
   else if( value->nType == X_IT_DOUBLE )
      return s_formatDouble( field, value->u.dbl, output );
   else
      return X_DBF_INVALID_FIELD_VALUE;
   if( length > field->length )
      return X_DBF_FIELD_OVERFLOW;
   memset( output, ' ', field->length - length );
   memcpy( output + field->length - length, formatted, length );
   return X_DBF_OK;
}

X_DBF_STATUS x_dbfFieldPutValue( DBF area, size_t ordinal, const X_ITEM *value )
{
   const X_DBF_FIELD *field;
   char               buffer[254];
   X_DBF_STATUS       status = x_dbfAreaPrepare( area );

   if( status != X_DBF_OK )
      return status;
   field = x_dbfSchemaField( area->schema, ordinal );
   if( field == NULL )
      return X_DBF_INVALID_ARGUMENT;
   // x_dbfFieldPutText() checks the write itself
   if( field->type == X_DBF_CHARACTER )
   {
      if( value->nType != X_IT_STRING )
         return X_DBF_INVALID_FIELD_VALUE;
      return x_dbfFieldPutText( area, ordinal, value->u.string.data, value->u.string.len );
   }
   status = x_dbfCheckWrite( area );
   if( status != X_DBF_OK )
      return status;
   switch( field->type )
   {
      case X_DBF_MEMO:
      case X_DBF_PICTURE:
         if( area->driver->operations.memoValueWrite != NULL )
         {
            char pointer[10];
            memcpy( pointer, area->record + field->offset, field->length );
            status = area->driver->operations.memoValueWrite( area, field, value, pointer );
            if( status == X_DBF_OK )
            {
               memcpy( area->record + field->offset, pointer, field->length );
               area->state.dirty = 1;
            }
            return status;
         }
         if( value->nType != X_IT_STRING )
            return X_DBF_INVALID_FIELD_VALUE;
         return x_dbfFieldWriteMemo( area, field, value->u.string.data, value->u.string.len );
      case X_DBF_NUMERIC:
         // A value too wide for the field is an error and the stored value stays unchanged
         status = x_dbfFormatNumeric( field, value, buffer );
         if( status != X_DBF_OK )
            return status;
         break;
      case X_DBF_DATE:
      {
         int year, month, day;
         if( value->nType != X_IT_DATE )
            return X_DBF_INVALID_FIELD_VALUE;
         if( value->u.integer == 0 )
            memset( buffer, ' ', 8 );
         else
         {
            if( value->u.integer < x_dateEncode( 1, 1, 1 ) || value->u.integer > x_dateEncode( 9999, 12, 31 ) )
               return X_DBF_INVALID_FIELD_VALUE;
            x_dateDecode( ( long )value->u.integer, &year, &month, &day );
            snprintf( buffer, sizeof( buffer ), "%04d%02d%02d", year, month, day );
         }
         break;
      }
      case X_DBF_LOGICAL:
         if( value->nType != X_IT_LOGICAL )
            return X_DBF_INVALID_FIELD_VALUE;
         buffer[0] = value->u.logical ? 'T' : 'F';
         break;
      default: return X_DBF_UNSUPPORTED_FORMAT;
   }
   memcpy( area->record + field->offset, buffer, field->length );
   area->state.dirty = 1;
   return X_DBF_OK;
}

_Static_assert( sizeof( double ) == 8 && DBL_MANT_DIG == 53 && DBL_MAX_EXP == 1024,
                "Memo values require IEEE 754 binary64" );

typedef struct
{
   X_DBF_TEXT  bytes;
   size_t      capacity;
   const void *parents[64];
   int         flex;
} X_DBF_VALUE_WRITER;

typedef struct
{
   const unsigned char *data;
   size_t               length;
   size_t               position;
   int                  flex;
} X_DBF_VALUE_READER;

static void s_valueWriterDrop( X_DBF_VALUE_WRITER *writer )
{
   x_dbfTextClear( &writer->bytes );
}

static X_DBF_STATUS s_valueAppend( X_DBF_VALUE_WRITER *writer, const void *data, size_t length )
{
   size_t needed;
   if( length > INT32_MAX || writer->bytes.length > INT32_MAX - length )
      return X_DBF_TEXT_OVERFLOW;
   needed = writer->bytes.length + length;
   if( needed > writer->capacity )
   {
      size_t capacity = writer->capacity == 0 ? 128 : writer->capacity;
      while( capacity < needed )
         capacity = capacity > INT32_MAX / 2 ? INT32_MAX : capacity * 2;
      writer->bytes.data = ( char * )x_xrealloc( writer->bytes.data, capacity + 1 );
      writer->capacity = capacity;
   }
   if( length != 0 )
      memcpy( writer->bytes.data + writer->bytes.length, data, length );
   writer->bytes.length = needed;
   if( writer->bytes.data != NULL )
      writer->bytes.data[needed] = '\0';
   return X_DBF_OK;
}

static X_DBF_STATUS s_valueNumber( X_DBF_VALUE_WRITER *writer, uint64_t value, size_t length )
{
   unsigned char data[8];
   size_t        i;
   for( i = 0; i < length; ++i )
      data[i] = ( unsigned char )( value >> ( i * 8 ) );
   return s_valueAppend( writer, data, length );
}

static int s_valueDate( int64_t value )
{
   return value == 0 || ( value >= x_dateEncode( 1, 1, 1 ) && value <= x_dateEncode( 9999, 12, 31 ) );
}

static X_DBF_STATUS s_valueEncode( X_DBF_VALUE_WRITER *writer, const X_ITEM *value, size_t depth )
{
   X_DBF_STATUS status;
   uint64_t     number;
   if( depth >= sizeof( writer->parents ) / sizeof( writer->parents[0] ) )
      return X_DBF_INVALID_FIELD_VALUE;
   switch( value->nType )
   {
      case X_IT_ARRAY:
      {
         int64_t count;
         size_t  i;
         if( value->u.pArr == NULL || x_arrayClass( value ) != 0 )
            return X_DBF_INVALID_FIELD_VALUE;
         count = x_arrayLen( value );
         if( count < 0 || count > UINT16_MAX )
            return X_DBF_FIELD_OVERFLOW;
         for( i = 0; i < depth; ++i )
            if( writer->parents[i] == value->u.pArr )
               return X_DBF_INVALID_FIELD_VALUE;
         writer->parents[depth] = value->u.pArr;
         status = s_valueNumber( writer, writer->flex ? 0x0C : 6, 1 );
         if( status == X_DBF_OK )
            status = s_valueNumber( writer, ( uint64_t )count, 2 );
         for( i = 1; status == X_DBF_OK && i <= ( size_t )count; ++i )
            status = s_valueEncode( writer, x_arrayGetItemPtr( value, i ), depth + 1 );
         writer->parents[depth] = NULL;
         return status;
      }
      case X_IT_STRING:
      {
         X_DBF_TEXT text __attribute__( ( cleanup( s_textDrop ) ) ) = { NULL, 0 };
         status = x_dbfTextCopy( value->u.string.data, value->u.string.len, &text );
         if( status != X_DBF_OK )
            return status;
         if( text.length > UINT16_MAX )
            return X_DBF_FIELD_OVERFLOW;
         if( writer->flex && text.length == 0 )
            return s_valueNumber( writer, 0x18, 1 );
         status = s_valueNumber( writer, writer->flex ? 0x07 : 1, 1 );
         if( status == X_DBF_OK )
            status = s_valueNumber( writer, text.length, 2 );
         if( status == X_DBF_OK )
            status = s_valueAppend( writer, text.data, text.length );
         return status;
      }
      case X_IT_INTEGER:
         if( value->u.integer >= INT32_MIN && value->u.integer <= INT32_MAX )
         {
            status = s_valueNumber( writer, writer->flex ? 0x05 : 2, 1 );
            return status == X_DBF_OK ? s_valueNumber( writer, ( uint32_t )value->u.integer, 4 ) : status;
         }
         number = value->u.integer < 0 ? ( uint64_t )( -( value->u.integer + 1 ) ) + 1 : ( uint64_t )value->u.integer;
         while( number > ( UINT64_C( 1 ) << 53 ) )
         {
            if( ( number & 1 ) != 0 )
               return X_DBF_FIELD_OVERFLOW;
            number >>= 1;
         }
         break;
      case X_IT_DOUBLE:
         if( !isfinite( value->u.dbl ) )
            return X_DBF_INVALID_FIELD_VALUE;
         break;
      case X_IT_DATE:
         if( !s_valueDate( value->u.integer ) )
            return X_DBF_INVALID_FIELD_VALUE;
         status = s_valueNumber( writer, writer->flex ? 0x0E : 4, 1 );
         return status == X_DBF_OK ? s_valueNumber( writer, ( uint32_t )value->u.integer, 4 ) : status;
      case X_IT_LOGICAL:
         if( writer->flex )
            return s_valueNumber( writer, value->u.logical ? 0x19 : 0x1A, 1 );
         status = s_valueNumber( writer, 5, 1 );
         return status == X_DBF_OK ? s_valueNumber( writer, value->u.logical != 0, 1 ) : status;
      case X_IT_NIL: return s_valueNumber( writer, 0, 1 );
      default: return X_DBF_INVALID_FIELD_VALUE;
   }
   {
      double              real = value->nType == X_IT_INTEGER ? ( double )value->u.integer : value->u.dbl;
      const unsigned char header[3] = { 3, 20, 0 };
      memcpy( &number, &real, sizeof( number ) );
      status = writer->flex ? s_valueNumber( writer, 0x09, 1 ) : s_valueAppend( writer, header, sizeof( header ) );
      return status == X_DBF_OK ? s_valueNumber( writer, number, 8 ) : status;
   }
}

X_DBF_STATUS x_dbfSmtEncode( const X_ITEM *value, X_DBF_TEXT *result )
{
   X_DBF_VALUE_WRITER writer __attribute__( ( cleanup( s_valueWriterDrop ) ) ) = { 0 };
   X_DBF_STATUS       status;
   status = s_valueEncode( &writer, value, 0 );
   if( status == X_DBF_OK )
   {
      x_dbfTextClear( result );
      *result = writer.bytes;
      writer.bytes.data = NULL;
   }
   return status;
}

X_DBF_STATUS x_dbfFlexEncode( const X_ITEM *value, X_DBF_TEXT *result, uint32_t *type )
{
   X_DBF_VALUE_WRITER writer __attribute__( ( cleanup( s_valueWriterDrop ) ) ) = { 0 };
   X_DBF_STATUS       status;
   uint32_t           blockType;
   writer.flex = 1;
   status = s_valueEncode( &writer, value, 0 );
   if( status != X_DBF_OK )
      return status;
   switch( ( unsigned char )writer.bytes.data[0] )
   {
      case 0x00: blockType = 1006; break;
      case 0x05: blockType = 1014; break;
      case 0x09: blockType = 1016; break;
      case 0x0C: blockType = 1002; break;
      case 0x0E: blockType = 1009; break;
      case 0x19: blockType = 1007; break;
      case 0x1A: blockType = 1008; break;
      default: return X_DBF_INVALID_FIELD_VALUE;
   }
   --writer.bytes.length;
   memmove( writer.bytes.data, writer.bytes.data + 1, writer.bytes.length );
   writer.bytes.data[writer.bytes.length] = '\0';
   x_dbfTextClear( result );
   *result = writer.bytes;
   writer.bytes.data = NULL;
   *type = blockType;
   return X_DBF_OK;
}

static int s_valueRead( X_DBF_VALUE_READER *reader, size_t length, uint64_t *value )
{
   size_t i;
   if( length > reader->length - reader->position )
      return 0;
   *value = 0;
   for( i = 0; i < length; ++i )
      *value |= ( uint64_t )reader->data[reader->position++] << ( i * 8 );
   return 1;
}

static X_DBF_STATUS s_flexInteger( X_DBF_VALUE_READER *reader, X_ITEM *value, size_t width, int isSigned,
                                   unsigned int metadata )
{
   uint64_t number, ignored, decimals = 0;
   int64_t  integer;
   if( !s_valueRead( reader, width, &number ) || ( metadata != 0 && !s_valueRead( reader, 1, &ignored ) ) ||
       ( metadata == 2 && !s_valueRead( reader, 1, &decimals ) ) )
      return X_DBF_CORRUPT_FILE;
   integer = ( int64_t )number;
   if( isSigned && ( number & ( UINT64_C( 1 ) << ( width * 8 - 1 ) ) ) != 0 )
      integer -= INT64_C( 1 ) << ( width * 8 );
   if( decimals != 0 )
   {
      value->nType = X_IT_DOUBLE;
      value->u.dbl = ( double )integer;
   }
   else
   {
      value->nType = X_IT_INTEGER;
      value->u.integer = integer;
   }
   return X_DBF_OK;
}

static X_DBF_STATUS s_valueDecode( X_DBF_VALUE_READER *reader, X_ITEM *value, size_t depth, int forcedType )
{
   uint64_t type, number;
   int      doubleMetadata = !reader->flex;
   if( depth >= 64 )
      return X_DBF_CORRUPT_FILE;
   if( forcedType >= 0 )
      type = ( unsigned int )forcedType;
   else if( !s_valueRead( reader, 1, &type ) )
      return X_DBF_CORRUPT_FILE;
   if( reader->flex )
   {
      static const struct
      {
         unsigned char tag, width, isSigned, metadata;
      } integers[] = { { 0x01, 1, 0, 0 }, { 0x02, 1, 1, 0 }, { 0x03, 2, 1, 0 }, { 0x04, 2, 0, 0 },
                       { 0x05, 4, 1, 0 }, { 0x11, 1, 0, 1 }, { 0x12, 1, 1, 1 }, { 0x13, 2, 1, 1 },
                       { 0x14, 2, 0, 1 }, { 0x15, 4, 1, 1 }, { 0x1C, 1, 0, 2 }, { 0x1D, 1, 1, 2 },
                       { 0x1E, 2, 1, 2 }, { 0x1F, 2, 0, 2 }, { 0x20, 4, 1, 2 }, { 0x21, 4, 0, 2 } };

      size_t i;
      for( i = 0; i < sizeof( integers ) / sizeof( integers[0] ); ++i )
         if( integers[i].tag == type )
            return s_flexInteger( reader, value, integers[i].width, integers[i].isSigned, integers[i].metadata );
      switch( type )
      {
         case 0x00: break;
         case 0x07: type = 1; break;
         case 0x09: type = 3; break;
         case 0x0A:
         case 0x0E: type = 4; break;
         case 0x0B: type = 5; break;
         case 0x0C: type = 6; break;
         case 0x0F:
            type = 3;
            doubleMetadata = 1;
            break;
         case 0x18: x_itemPutStringBuffer( value, x_strdup( "" ), 0 ); return X_DBF_OK;
         case 0x19:
         case 0x1A:
            value->nType = X_IT_LOGICAL;
            value->u.logical = type == 0x19;
            return X_DBF_OK;
         case 0x1B: return X_DBF_UNSUPPORTED_FORMAT;
         default: return X_DBF_CORRUPT_FILE;
      }
   }
   switch( type )
   {
      case 0: x_itemClear( value ); return X_DBF_OK;
      case 1:
      {
         X_DBF_TEXT   text __attribute__( ( cleanup( s_textDrop ) ) ) = { NULL, 0 };
         X_DBF_STATUS status;
         if( !s_valueRead( reader, 2, &number ) || number > reader->length - reader->position )
            return X_DBF_CORRUPT_FILE;
         status = x_dbfTextCopy( ( const char * )reader->data + reader->position, ( size_t )number, &text );
         if( status != X_DBF_OK )
            return status;
         reader->position += ( size_t )number;
         x_itemPutStringBuffer( value, text.data, text.length );
         text.data = NULL;
         return X_DBF_OK;
      }
      case 2:
         if( !s_valueRead( reader, 4, &number ) )
            return X_DBF_CORRUPT_FILE;
         value->nType = X_IT_INTEGER;
         value->u.integer = number > INT32_MAX ? ( int64_t )number - ( INT64_C( 1 ) << 32 ) : ( int64_t )number;
         return X_DBF_OK;
      case 3:
         if( ( doubleMetadata && !s_valueRead( reader, 2, &number ) ) || !s_valueRead( reader, 8, &number ) )
            return X_DBF_CORRUPT_FILE;
         value->nType = X_IT_DOUBLE;
         memcpy( &value->u.dbl, &number, sizeof( number ) );
         return isfinite( value->u.dbl ) ? X_DBF_OK : X_DBF_CORRUPT_FILE;
      case 4:
         if( !s_valueRead( reader, 4, &number ) || !s_valueDate( ( int64_t )number ) )
            return X_DBF_CORRUPT_FILE;
         value->nType = X_IT_DATE;
         value->u.integer = ( int64_t )number;
         return X_DBF_OK;
      case 5:
         if( !s_valueRead( reader, 1, &number ) )
            return X_DBF_CORRUPT_FILE;
         value->nType = X_IT_LOGICAL;
         value->u.logical = number != 0;
         return X_DBF_OK;
      case 6:
      {
         size_t i;
         if( !s_valueRead( reader, 2, &number ) || number > reader->length - reader->position )
            return X_DBF_CORRUPT_FILE;
         x_arrayNew( value, ( int64_t )number );
         for( i = 1; i <= ( size_t )number; ++i )
         {
            X_DBF_STATUS status = s_valueDecode( reader, x_arrayGetItemPtr( value, i ), depth + 1, -1 );
            if( status != X_DBF_OK )
               return status;
         }
         return X_DBF_OK;
      }
      default: return X_DBF_CORRUPT_FILE;
   }
}

X_DBF_STATUS x_dbfSmtDecode( const X_DBF_TEXT *bytes, unsigned int type, X_ITEM *result )
{
   X_ITEM             value __attribute__( ( cleanup( x_itemClear ) ) ) = X_ITEM_NIL_INIT;
   X_DBF_VALUE_READER reader;
   X_DBF_STATUS       status;
   if( bytes->length == 0 || ( unsigned char )bytes->data[0] != type )
      return X_DBF_CORRUPT_FILE;
   reader = ( X_DBF_VALUE_READER ){ ( const unsigned char * )bytes->data, bytes->length, 0, 0 };
   status = s_valueDecode( &reader, &value, 0, -1 );
   if( status == X_DBF_OK && reader.position != reader.length )
      status = X_DBF_CORRUPT_FILE;
   if( status == X_DBF_OK )
      x_itemCopy( result, &value );
   return status;
}

X_DBF_STATUS x_dbfFlexDecode( const X_DBF_TEXT *bytes, uint32_t type, X_ITEM *result )
{
   X_ITEM             value __attribute__( ( cleanup( x_itemClear ) ) ) = X_ITEM_NIL_INIT;
   X_DBF_VALUE_READER reader;
   X_DBF_STATUS       status;
   int                tag;
   reader = ( X_DBF_VALUE_READER ){ ( const unsigned char * )bytes->data, bytes->length, 0, 1 };
   if( type >= 1010 && type <= 1015 )
      status = s_flexInteger( &reader, &value, ( size_t )1 << ( ( type - 1010 ) / 2 ), type % 2 == 0, 0 );
   else
   {
      switch( type )
      {
         case 1002: tag = 0x0C; break;
         case 1006: tag = 0x00; break;
         case 1007: tag = 0x19; break;
         case 1008: tag = 0x1A; break;
         case 1009: tag = 0x0E; break;
         case 1016: tag = 0x09; break;
         default: return X_DBF_UNSUPPORTED_FORMAT;
      }
      status = s_valueDecode( &reader, &value, 0, tag );
   }
   if( status == X_DBF_OK && reader.position != reader.length )
      status = X_DBF_CORRUPT_FILE;
   if( status == X_DBF_OK )
      x_itemCopy( result, &value );
   return status;
}
