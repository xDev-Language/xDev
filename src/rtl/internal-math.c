/*
 * Copyright (c) 2024-2026 xDev-Language
 *
 * Library...: xDev Runtime
 * File......: internal-math.c
 *
 */

#include "internal-private.h"
#include "operators.h"

#include <ctype.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* The decimal places of GetPrec() and SetPrec(); the trigonometric functions always use full double precision */
static int s_mathPrecision = 16;

/* The counter of NumCount() */
static int64_t s_mathCounter;

/* ---------------------------------------------------------------------------------------------
   Math
   --------------------------------------------------------------------------------------------- */

/* Return a floating point result; a NaN or an infinity raises BASE/3105 named after the function */
void x_mathRetnd( X_CALL_CTX *ctx, double value, const char *operation )
{
   if( isfinite( value ) )
      x_retnd( ctx, value );
   else
      x_errorRaiseNamed( ctx->result, 3105, operation );
}

/* Degrees to radians */
double x_mathDegToRad( double degrees )
{
   return ( X_MATH_PI / 180.0 ) * degrees;
}

/* Radians to degrees */
double x_mathRadToDeg( double radians )
{
   return ( 180.0 / X_MATH_PI ) * radians;
}

/* Rate(): the interest rate per period at which periods payments repay the loan, found by Newton's method;
   -1 when the iteration does not converge */
double x_mathRate( double loan, double payment, double periods )
{
   double rate = payment / loan - loan / ( periods * periods * payment );
   int i;

   if( payment * periods == loan )
      return 0.0;

   for( i = 0; i < 100; ++i )
   {
      double growth = pow( 1.0 + rate, -periods );
      double value = payment * ( 1.0 - growth ) / rate - loan;
      double derivative = periods * growth / ( 1.0 + rate ) * rate - ( 1.0 - growth );
      double slope = payment * derivative / ( rate * rate );
      double step = value / slope;

      rate -= step;
      if( !isfinite( rate ) || rate <= -1.0 )
         return -1.0;

      if( fabs( step ) < 1e-12 )
         return rate;
   }

   return -1.0;
}

/* Rotate the low bits (1 to 64) of value left by count places; a negative count rotates right and
   the bits above the low bits keep their value */
uint64_t x_mathRotateLeft( uint64_t value, int bits, int64_t count )
{
   uint64_t mask = IIF( bits >= 64, UINT64_MAX, ( UINT64_C( 1 ) << bits ) - 1 );
   uint64_t low = value & mask;
   int shift = ( int )( ( count % bits + bits ) % bits );

   if( shift != 0 )
      low = ( ( low << shift ) | ( low >> ( bits - shift ) ) ) & mask;

   return ( value & ~mask ) | low;
}

/* The decimal places of GetPrec() */
int x_mathPrecision( void )
{
   return s_mathPrecision;
}

/* SetPrec() */
void x_mathSetPrecision( int decimals )
{
   s_mathPrecision = decimals;
}

/* Mirror the order of the low bits of value; the bits above them keep their value */
uint64_t x_mathMirror( uint64_t value, int bits )
{
   uint64_t mask = ( UINT64_C( 1 ) << bits ) - 1;
   uint64_t source = value & mask;
   uint64_t result = 0;
   int i;

   for( i = 0; i < bits; ++i )
   {
      result = ( result << 1 ) | ( source & 1 );
      source >>= 1;
   }

   return ( value & ~mask ) | result;
}

/* The number of characters of a number written without a format */
int x_mathDisplayLength( const X_ITEM *value )
{
   if( X_IS_UINT64( value ) )
      return snprintf( NULL, 0, "%llu", ( unsigned long long )value->u.unsignedInteger );

   if( X_IS_INTEGER( value ) )
      return snprintf( NULL, 0, "%lld", ( long long )value->u.integer );

   return snprintf( NULL, 0, "%.15g", value->u.dbl );
}

/* NumCount(): add to the counter, or set it when the second argument is .T.; the result is the counter */
void x_mathCounter( X_CALL_CTX *ctx )
{
   const X_ITEM *item = x_paritem( ctx, 1 );
   int64_t value, next;
   int tooBig = 0;

   if( item != NULL && X_IS_UINT64( item ) && item->u.unsignedInteger > INT64_MAX )
      tooBig = 1;
   else if( item != NULL && X_IS_DOUBLE( item ) && !( item->u.dbl >= -0x1p63 && item->u.dbl < 0x1p63 ) )
      tooBig = 1;

   if( tooBig )
   {
      x_errorRaiseNamed( ctx->result, 3106, "NUMCOUNT" );
      return;
   }

   value = x_parnintdef( ctx, 1, 0 );
   if( x_parl( ctx, 2 ) )
      s_mathCounter = value;
   else
   {
      if( __builtin_add_overflow( s_mathCounter, value, &next ) )
      {
         x_errorRaiseNamed( ctx->result, 3106, "NUMCOUNT" );
         return;
      }

      s_mathCounter = next;
   }

   x_retnint( ctx, s_mathCounter );
}

/* ---------------------------------------------------------------------------------------------
   Numeric functions of the API
   --------------------------------------------------------------------------------------------- */

/* Abs(): the absolute value; an UINT64 stays unchanged */
void x_mathAbs( X_CALL_CTX *ctx )
{
   if( X_IS_UINT64( x_paritem( ctx, 1 ) ) )
      x_itemCopy( ctx->result, x_paritem( ctx, 1 ) );
   else if( X_IS_DOUBLE( x_paritem( ctx, 1 ) ) )
      x_retnd( ctx, fabs( x_parnd( ctx, 1 ) ) );
   else
   {
      int64_t value = x_parnint( ctx, 1 );

      x_retnint( ctx, IIF( value < 0, -value, value ) );
   }
}

/* BitShift(): the bits shifted left by a positive count or right by a negative one, filling with 0 */
void x_mathBitShift( X_CALL_CTX *ctx )
{
   uint64_t value = ( uint64_t )x_parnint( ctx, 1 );
   int64_t shift = x_parnint( ctx, 2 );

   if( shift >= 64 || shift <= -64 )
      x_retnint( ctx, 0 );
   else if( shift >= 0 )
      x_retnint( ctx, ( int64_t )( value << shift ) );
   else
      x_retnint( ctx, ( int64_t )( value >> -shift ) );
}

/* BitToC(): the characters of the pattern whose bits are set; the last character stands for bit 0 */
void x_mathBitToC( X_CALL_CTX *ctx )
{
   uint64_t value = ( uint64_t )x_parnint( ctx, 1 );
   const char *pattern = x_parc( ctx, 2 );
   size_t length = x_parclen( ctx, 2 );
   int spaces = X_ISLOG( ctx, 3 ) && x_parl( ctx, 3 );
   char result[64];
   size_t count = 0;
   size_t i;

   for( i = 0; i < length; ++i )
   {
      if( ( value >> ( length - 1 - i ) ) & 1 )
         result[count++] = pattern[i];
      else if( spaces )
         result[count++] = ' ';
   }

   x_retclen( ctx, result, count );
}

/* CToBit(): the bits of the pattern characters found in a string */
void x_mathCToBit( X_CALL_CTX *ctx )
{
   const char *text = x_parc( ctx, 1 );
   const char *pattern = x_parc( ctx, 2 );
   size_t length = x_parclen( ctx, 1 );
   size_t patternLength = x_parclen( ctx, 2 );
   uint64_t value = 0;
   size_t i;

   /* The last character of cBitPattern stands for bit 0, as in BitToC() */
   for( i = 0; i < length; ++i )
   {
      const char *found = memchr( pattern, text[i], patternLength );

      if( found )
         value |= UINT64_C( 1 ) << ( patternLength - 1 - ( size_t )( found - pattern ) );
   }

   x_retnint( ctx, ( int64_t )value );
}

/* CToN(): a string of digits in a base as an integer; an invalid string gives 0 */
void x_mathCToN( X_CALL_CTX *ctx )
{
   const char *text = x_parc( ctx, 1 );
   size_t start = 0;
   size_t end = x_parclen( ctx, 1 );
   int64_t base = x_parnintdef( ctx, 2, 10 );
   uint64_t value = 0;

   while( start < end && text[start] == ' ' )
      ++start;

   while( end > start && text[end - 1] == ' ' )
      --end;

   if( base < 2 || base > 36 || start == end )
   {
      x_retni( ctx, 0 );
      return;
   }

   /* A digit outside the base or a value above 64 bits gives 0 */
   for( ; start < end; ++start )
   {
      unsigned char c = ( unsigned char )text[start];
      int64_t digit = 99;

      if( isdigit( c ) )
         digit = c - '0';
      else if( isalpha( c ) )
         digit = toupper( c ) - 'A' + 10;

      if( digit >= base || value > ( UINT64_MAX - ( uint64_t )digit ) / ( uint64_t )base )
      {
         x_retni( ctx, 0 );
         return;
      }

      value = value * ( uint64_t )base + ( uint64_t )digit;
   }

   /* lNegative reads the 64 bits as a signed number: "FFFFFFFFFFFFFFFF" is -1 */
   if( x_parl( ctx, 3 ) || value <= INT64_MAX )
      x_retnint( ctx, ( int64_t )value );
   else
      x_retuint64( ctx, value );
}

/* Fact(): the factorial; -1 outside 0 to 21, and 21! as a floating point number */
void x_mathFact( X_CALL_CTX *ctx )
{
   int64_t value, factorial = 1, i;

   value = x_parnint( ctx, 1 );
   if( value < 0 || value > 21 )
      x_retni( ctx, -1 );
   else if( value == 21 )
      x_retnd( ctx, 51090942171709440000.0 );
   else
   {
      for( i = 2; i <= value; ++i )
         factorial *= i;

      x_retnint( ctx, factorial );
   }
}

/* Int(): the integer part of a number */
void x_mathInt( X_CALL_CTX *ctx )
{
   if( X_IS_UINT64( x_paritem( ctx, 1 ) ) )
      x_itemCopy( ctx->result, x_paritem( ctx, 1 ) );
   else if( X_IS_DOUBLE( x_paritem( ctx, 1 ) ) )
      x_retnint( ctx, ( int64_t )x_parnd( ctx, 1 ) );
   else
      x_retnint( ctx, x_parnint( ctx, 1 ) );
}

/* Max(): the larger of two numbers or dates */
void x_mathMax( X_CALL_CTX *ctx )
{
   const X_ITEM *item1 = x_paritem( ctx, 1 );
   const X_ITEM *item2 = x_paritem( ctx, 2 );

   if( X_IS_UINT64( item1 ) || X_IS_UINT64( item2 ) )
   {
      X_ITEM comparison __attribute__( ( cleanup( x_itemClear ) ) ) = X_ITEM_NIL_INIT;

      x_itemGe( ctx, &comparison, item1, item2 );
      x_itemCopy( ctx->result, IIF( comparison.u.logical, item1, item2 ) );
      return;
   }

   int firstWins;

   if( X_IS_DATE( item1 ) )
      firstWins = x_pardl( ctx, 1 ) >= x_pardl( ctx, 2 );
   else
      firstWins = x_parnd( ctx, 1 ) >= x_parnd( ctx, 2 );

   x_itemCopy( ctx->result, IIF( firstWins, item1, item2 ) );
}

/* Min(): the smaller of two numbers or dates */
void x_mathMin( X_CALL_CTX *ctx )
{
   const X_ITEM *item1 = x_paritem( ctx, 1 );
   const X_ITEM *item2 = x_paritem( ctx, 2 );

   if( X_IS_UINT64( item1 ) || X_IS_UINT64( item2 ) )
   {
      X_ITEM comparison __attribute__( ( cleanup( x_itemClear ) ) ) = X_ITEM_NIL_INIT;

      x_itemLe( ctx, &comparison, item1, item2 );
      x_itemCopy( ctx->result, IIF( comparison.u.logical, item1, item2 ) );
      return;
   }

   int firstWins;

   if( X_IS_DATE( item1 ) )
      firstWins = x_pardl( ctx, 1 ) <= x_pardl( ctx, 2 );
   else
      firstWins = x_parnd( ctx, 1 ) <= x_parnd( ctx, 2 );

   x_itemCopy( ctx->result, IIF( firstWins, item1, item2 ) );
}

/* Mod(): the dBASE III PLUS modulus; the result has the sign of the divisor */
void x_mathMod( X_CALL_CTX *ctx )
{
   X_ITEM *result;

   result = ctx->result;

   if( x_parnd( ctx, 2 ) == 0.0 )
   {
      if( x_errorRaise( result, 1341, x_paritem( ctx, 1 ), x_paritem( ctx, 2 ) ) )
         x_itemReturn( ctx, x_paritem( ctx, 1 ) );
      return;
   }

   x_itemMod( ctx, result, x_paritem( ctx, 1 ), x_paritem( ctx, 2 ) );

   if( X_IS_UINT64( result ) )
   {
      const X_ITEM *divisor = x_paritem( ctx, 2 );

      if( divisor->nType == X_IT_INTEGER && divisor->u.integer < 0 && result->u.unsignedInteger != 0 )
         x_retnint( ctx, ( int64_t )result->u.unsignedInteger + divisor->u.integer );
   }
   else if( X_IS_DOUBLE( result ) )
   {
      if( result->u.dbl != 0.0 && ( result->u.dbl < 0.0 ) != ( x_parnd( ctx, 2 ) < 0.0 ) )
         result->u.dbl += x_parnd( ctx, 2 );
   }
   else if( result->u.integer != 0 && ( result->u.integer < 0 ) != ( x_parnint( ctx, 2 ) < 0 ) )
      result->u.integer += x_parnint( ctx, 2 );
}

/* NToC(): an integer as digits of a base, padded to a length */
void x_mathNToC( X_CALL_CTX *ctx )
{
   /* A negative integer is converted as its 64 bits, like NumToHex() */
   uint64_t value = IIF( X_IS_UINT64( x_paritem( ctx, 1 ) ), x_paruint64( ctx, 1 ), ( uint64_t )x_parnint( ctx, 1 ) );
   int64_t base = x_parnintdef( ctx, 2, 10 );
   size_t length = ( size_t )x_parnintdef( ctx, 3, 0 );
   char digits[64];
   size_t count = 0;
   X_UCHAR pad;
   char *result, *cursor;

   x_ucharArg( ctx, 4, &pad );

   if( base >= 2 && base <= 36 )
      do
      {
         digits[63 - count++] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ"[value % ( uint64_t )base];
         value /= ( uint64_t )base;
      } while( value != 0 );

   if( length == 0 )
      length = IIF( count == 0, 1, count );
   /* nLength counts characters; an invalid base or a result longer than nLength gives asterisks */
   if( count == 0 || count > length )
   {
      result = x_xgrab( length + 1 );
      memset( result, '*', length );
      result[length] = '\0';
      x_retclen_buffer( ctx, result, length );
      return;
   }

   result = x_xgrab( ( length - count ) * pad.size + count + 1 );
   cursor = x_ucharFill( result, &pad, length - count );
   memcpy( cursor, digits + 64 - count, count );
   cursor[count] = '\0';
   x_retclen_buffer( ctx, result, ( size_t )( cursor + count - result ) );
}

/* Periods(): calculate the duration for paying back a loan at fixed interest rate and payments */
void x_mathPeriods( X_CALL_CTX *ctx )
{
   double loan, payment, rate;

   loan = x_parnd( ctx, 1 );
   payment = x_parnd( ctx, 2 );
   rate = x_parnd( ctx, 3 );

   if( payment <= loan * rate )
      x_retni( ctx, -1 );
   else if( rate == 0.0 )
      x_mathRetnd( ctx, loan / payment, "PERIODS" );
   else
      x_mathRetnd( ctx, -log( 1.0 - loan * rate / payment ) / log( 1.0 + rate ), "PERIODS" );
}

/* Random(): a random number from nMinLimit (default 0) to nMaxLimit (default 1) */
void x_mathRandom( X_CALL_CTX *ctx )
{
   double minimum = 0.0;
   double maximum = 1.0;

   if( X_ISNUM( ctx, 2 ) )
   {
      minimum = x_parnd( ctx, 1 );
      maximum = x_parnd( ctx, 2 );
   }
   else if( X_ISNUM( ctx, 1 ) )
      maximum = x_parnd( ctx, 1 );

   x_retnd( ctx, minimum + ( maximum - minimum ) * x_randomDouble() );
}

/* RandomInt(): a random integer from nMinLimit to nMaxLimit; from 1 with one limit, 0 or 1 without them */
void x_mathRandomInt( X_CALL_CTX *ctx )
{
   if( X_ISNUM( ctx, 2 ) )
      x_retnint( ctx, x_randomInt( x_parnint( ctx, 1 ), x_parnint( ctx, 2 ) ) );
   else if( X_ISNUM( ctx, 1 ) )
      x_retnint( ctx, x_randomInt( 1, x_parnint( ctx, 1 ) ) );
   else
      x_retnint( ctx, x_randomInt( 0, 1 ) );
}

/* Round(): a number rounded to a number of decimals; half away from zero */
void x_mathRound( X_CALL_CTX *ctx )
{
   if( X_IS_UINT64( x_paritem( ctx, 1 ) ) )
   {
      uint64_t value = x_paruint64( ctx, 1 ), divisor = 1, rounded;
      int decimals = x_parni( ctx, 2 );

      if( decimals >= 0 )
      {
         x_retuint64( ctx, value );
         return;
      }

      if( decimals < -19 )
      {
         x_retuint64( ctx, 0 );
         return;
      }

      for( int i = 0; i > decimals; --i )
         divisor *= 10;

      uint64_t quotient = value / divisor + ( value % divisor >= divisor / 2 );

      if( __builtin_mul_overflow( quotient, divisor, &rounded ) )
         x_errorRaiseNamed( ctx->result, 3106, "ROUND UINT64" );
      else
         x_retuint64( ctx, rounded );
      return;
   }

   double value = x_parnd( ctx, 1 );
   double scale = pow( 10.0, ( double )x_parni( ctx, 2 ) );
   double scaled = value * scale;

   scaled = IIF( scaled >= 0.0, floor( scaled + 0.5 ), ceil( scaled - 0.5 ) );

   x_retnd( ctx, scaled / scale );
}

/* Sign(): 1, 0 or -1 for the sign of a number */
void x_mathSign( X_CALL_CTX *ctx )
{
   double value;
   int sign = 0;

   value = x_parnd( ctx, 1 );
   if( value > 0 )
      sign = 1;
   else if( value < 0 )
      sign = -1;

   x_retni( ctx, sign );
}

/* Str(): a number as text, with or without a length and decimals */
void x_mathStr( X_CALL_CTX *ctx )
{
   if( X_IS_UINT64( x_paritem( ctx, 1 ) ) )
   {
      char digits[32];
      int size = snprintf( digits, sizeof( digits ), "%llu", ( unsigned long long )x_paruint64( ctx, 1 ) );
      int length = IIF( size < 10, 10, size );
      int decimals = IIF( x_pcount( ctx ) < 3, 0, x_parni( ctx, 3 ) );

      if( x_pcount( ctx ) >= 2 )
         length = x_parni( ctx, 2 );

      length = IIF( length < 0, 0, length );
      decimals = IIF( decimals < 0, 0, decimals );
      int64_t needed = ( int64_t )size + IIF( decimals, ( int64_t )decimals + 1, 0 );
      char *buffer = ( char * )x_xgrab( ( size_t )length + 1 );

      if( needed > length )
         memset( buffer, '*', ( size_t )length );
      else
      {
         size_t padding = ( size_t )( length - needed );

         memset( buffer, ' ', padding );
         memcpy( buffer + padding, digits, ( size_t )size );
         if( decimals )
         {
            buffer[padding + size] = '.';
            memset( buffer + padding + size + 1, '0', ( size_t )decimals );
         }
      }

      buffer[length] = '\0';
      x_retclen_buffer( ctx, buffer, ( size_t )length );
      return;
   }

   double value = x_parnd( ctx, 1 );

   /*
    * Str( n ) without a length: an integer in full, any other number as C prints
    * it with 15 significant digits ( 3.1400 -> "3.14", 0.1 + 0.2 -> "0.3",
    * 1.5e30 -> "1.5e+30", 0.00001 -> "1e-05" ), right-aligned to 10 columns.
    * A longer number widens the field instead of turning into asterisks.
    */
   if( x_pcount( ctx ) < 2 )
   {
      const X_ITEM *integer = x_param( ctx, 1, X_IT_INTEGER );
      char text[32];
      int textLen;

      if( integer )
         textLen = snprintf( text, sizeof( text ), "%lld", ( long long )integer->u.integer );
      else
         textLen = snprintf( text, sizeof( text ), "%.15g", value );

      int width = IIF( textLen < 10, 10, textLen );
      char *buffer = ( char * )x_xgrab( ( size_t )width + 1 );

      snprintf( buffer, ( size_t )width + 1, "%*s", width, text );
      x_retclen_buffer( ctx, buffer, ( size_t )width );
      return;
   }

   int length = x_parni( ctx, 2 );
   int decimals = IIF( x_pcount( ctx ) >= 3, x_parni( ctx, 3 ), 0 );

   if( length < 0 )
      length = 0;

   if( decimals < 0 )
      decimals = 0;

   int needed = snprintf( NULL, 0, "%.*f", decimals, value );

   if( needed < 0 )
      needed = 0;

   char *buffer = ( char * )x_xgrab( ( size_t )length + 1 );

   if( needed <= length )
      snprintf( buffer, ( size_t )length + 1, "%*.*f", length, decimals, value );
   else
   {
      /* When the integer part itself does not fit, use the most precise
         scientific notation that fits ( Str( 1.5e30, 10 ) -> "   1.5e+30" ).
         Asterisks remain when even that does not fit, or when only the
         decimals overflow ( Str( 1.23, 3, 2 ) -> "***" ). */
      int digits = 0;

      if( snprintf( NULL, 0, "%.0f", value ) > length )
      {
         for( digits = 15; digits > 0 && snprintf( NULL, 0, "%.*g", digits, value ) > length; --digits )
            ;
      }

      if( digits > 0 )
         snprintf( buffer, ( size_t )length + 1, "%*.*g", length, digits, value );
      else
         memset( buffer, '*', ( size_t )length );
   }

   buffer[length] = '\0';

   x_retclen_buffer( ctx, buffer, ( size_t )length );
}

/* StrZero(): the text of Str() with zeros for its leading spaces */
void x_mathStrZero( X_CALL_CTX *ctx )
{
   const char *text;
   char *buffer;
   size_t length, i;
   int negative = 0;

   /* The text of Str(), its leading spaces become zeros and a minus goes to the front: StrZero( -5, 4 ) is "-005" */
   x_mathStr( ctx );
   if( !X_IS_STRING( ctx->result ) )
      return;

   text = x_itemGetCPtr( ctx->result );
   length = x_itemGetCLen( ctx->result );
   buffer = ( char * )x_xgrab( length + 1 );
   memcpy( buffer, text, length + 1 );
   for( i = 0; i < length && ( buffer[i] == ' ' || buffer[i] == '-' ); ++i )
   {
      negative |= buffer[i] == '-';
      buffer[i] = '0';
   }

   if( negative && length > 0 )
      buffer[0] = '-';

   x_retclen_buffer( ctx, buffer, length );
}

/* Val(): the number at the beginning of a string */
void x_mathVal( X_CALL_CTX *ctx )
{
   /* Only [spaces][sign]digits[.digits] is a number; strtod() alone would also
      accept "nan", "inf", "0x10" and exponents. */
   const char *text = x_parc( ctx, 1 );
   size_t start = 0;
   size_t end;
   char *number;

   while( text[start] == ' ' )
      ++start;

   end = start;
   if( text[end] == '+' || text[end] == '-' )
      ++end;

   while( isdigit( ( unsigned char )text[end] ) )
      ++end;

   if( text[end] == '.' )
   {
      ++end;
      while( isdigit( ( unsigned char )text[end] ) )
         ++end;
   }

   number = ( char * )x_xgrab( end - start + 1 );
   memcpy( number, text + start, end - start );
   number[end - start] = '\0';
   x_retnd( ctx, strtod( number, NULL ) );
   x_xfree( number );
}
