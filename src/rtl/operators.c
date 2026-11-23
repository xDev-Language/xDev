/*
 * Copyright (c) 2024-2026 xDev-Language
 *
 * Library...: xDev Runtime
 * File......: operators.c
 *
 */

#include "internal-private.h"
#include "classes.h"

#include "operators.h"
#include "statements.h"

#include <math.h>
#include <string.h>

/* ---------------------------------------------------------------------------------------------
   Helpers
   --------------------------------------------------------------------------------------------- */

/* An operator overloaded by the class of object; *handled is 0 when the class has none */
static int s_objectOperator( X_CALL_CTX *ctx, X_ITEM *result, const X_ITEM *object, const X_ITEM *argument,
                             X_OO_OPERATOR operation, int *handled )
{
   if( X_IS_OBJECT( object ) && x_objHasOperator( object, operation ) )
   {
      *handled = 1;
      return x_objOperatorCall( ctx, operation, result, ( X_ITEM * )object, argument, NULL );
   }

   *handled = 0;

   return 0;
}

static int s_isNumber( const X_ITEM *value )
{
   return X_IS_NUMERIC( value );
}

static double s_asDouble( const X_ITEM *value )
{
   return x_itemAsDouble( value );
}

static int64_t s_asDays( const X_ITEM *value )
{
   return x_itemAsInt64( value );
}

/* An integer of either sign in 128 bits */
static __int128 s_asWide( const X_ITEM *value )
{
   if( X_IS_UINT64( value ) )
      return ( __int128 )value->u.unsignedInteger;

   return ( __int128 )value->u.integer;
}

/* An integer result, or a floating point one when integer is 0 */
static void s_numberResult( X_ITEM *result, int integer, int64_t integerValue, double doubleValue )
{
   x_itemClear( result );

   if( integer )
   {
      result->nType = X_IT_INTEGER;
      result->u.integer = integerValue;
   }
   else
   {
      result->nType = X_IT_DOUBLE;
      result->u.dbl = doubleValue;
   }
}

/* A date result */
static void s_dateResult( X_ITEM *result, int64_t julian )
{
   x_itemClear( result );

   result->nType = X_IT_DATE;
   result->u.integer = julian;
}

/* ---------------------------------------------------------------------------------------------
   Arithmetic
   --------------------------------------------------------------------------------------------- */

/* The error of an arithmetic operator with an argument that is not a number */
static int s_numbersErrorCode( char op )
{
   switch( op )
   {
   case '+':
      return 1081;

   case '-':
      return 1082;

   case '*':
      return 1083;

   case '/':
      return 1084;

   case '%':
      return 1085;
   }

   return 1088;
}

/* +, -, *, / and % with UINT64: the result must fit into UINT64 */
static int s_numbersUInt64( X_ITEM *result, const X_ITEM *left, const X_ITEM *right, char op )
{
   __int128 leftWide = s_asWide( left );
   __int128 rightWide = s_asWide( right );
   __int128 wide = 0;
   int overflow = 0;

   switch( op )
   {
   case '+':
      wide = leftWide + rightWide;
      break;

   case '-':
      wide = leftWide - rightWide;
      break;

   case '*':
      overflow = __builtin_mul_overflow( leftWide, rightWide, &wide );
      break;

   case '/':
      wide = leftWide / rightWide;
      break;

   case '%':
      wide = leftWide % rightWide;
      break;
   }

   if( overflow || wide < 0 || wide > ( __int128 )UINT64_MAX )
      return x_errorRaiseNamed( result, 3106, "UINT64 arithmetic" );

   x_itemReplace( result, ( X_ITEM ){ X_IT_UINT64, { .unsignedInteger = ( uint64_t )wide } } );

   return 1;
}

/* +, -, *, /, % and ^ of two numbers; integers stay integers until they overflow */
static int s_numbers( X_ITEM *result, const X_ITEM *left, const X_ITEM *right, char op )
{
   int integers;
   int64_t value = 0;
   double leftDouble, rightDouble;

   if( !s_isNumber( left ) || !s_isNumber( right ) )
      return x_errorRaise( result, s_numbersErrorCode( op ), left, right );

   leftDouble = s_asDouble( left );
   rightDouble = s_asDouble( right );

   if( ( op == '/' || op == '%' ) && rightDouble == 0.0 )
      return x_errorRaise( result, IIF( op == '/', 1340, 1341 ), left, right );

   if( X_IS_INTEGER( left ) && X_IS_INTEGER( right ) && ( X_IS_UINT64( left ) || X_IS_UINT64( right ) ) && op != '^' )
      return s_numbersUInt64( result, left, right, op );

   integers = left->nType == X_IT_INTEGER && right->nType == X_IT_INTEGER;

   switch( op )
   {
   case '+':
      if( integers )
         integers = !__builtin_add_overflow( left->u.integer, right->u.integer, &value );

      s_numberResult( result, integers, value, leftDouble + rightDouble );
      break;

   case '-':
      if( integers )
         integers = !__builtin_sub_overflow( left->u.integer, right->u.integer, &value );

      s_numberResult( result, integers, value, leftDouble - rightDouble );
      break;

   case '*':
      if( integers )
         integers = !__builtin_mul_overflow( left->u.integer, right->u.integer, &value );

      s_numberResult( result, integers, value, leftDouble * rightDouble );
      break;

   case '/':
      s_numberResult( result, 0, 0, leftDouble / rightDouble );
      break;

   case '%':
      if( integers && left->u.integer == INT64_MIN && right->u.integer == -1 )
         s_numberResult( result, 1, 0, 0.0 );
      else if( integers )
         s_numberResult( result, 1, left->u.integer % right->u.integer, 0.0 );
      else
         s_numberResult( result, 0, 0, fmod( leftDouble, rightDouble ) );
      break;

   case '^':
      s_numberResult( result, 0, 0, pow( leftDouble, rightDouble ) );
      break;
   }

   return 1;
}

/* ++ and -- */
static int s_updateNumber( X_ITEM *result, const X_ITEM *value, int increment )
{
   double step = IIF( increment, 1.0, -1.0 );
   int64_t integer;
   int overflow;

   if( !s_isNumber( value ) )
      return x_errorRaise( result, IIF( increment, 1086, 1087 ), value, NULL );

   if( X_IS_UINT64( value ) )
   {
      uint64_t number;

      if( increment )
         overflow = __builtin_add_overflow( value->u.unsignedInteger, UINT64_C( 1 ), &number );
      else
         overflow = __builtin_sub_overflow( value->u.unsignedInteger, UINT64_C( 1 ), &number );

      if( overflow )
         return x_errorRaiseNamed( result, 3106, "UINT64 update" );

      x_itemReplace( result, ( X_ITEM ){ X_IT_UINT64, { .unsignedInteger = number } } );
      return 1;
   }

   if( value->nType == X_IT_INTEGER )
   {
      if( increment )
         overflow = __builtin_add_overflow( value->u.integer, ( int64_t )1, &integer );
      else
         overflow = __builtin_sub_overflow( value->u.integer, ( int64_t )1, &integer );

      s_numberResult( result, !overflow, integer, ( double )value->u.integer + step );
   }
   else
      s_numberResult( result, 0, 0, value->u.dbl + step );

   return 1;
}

/* Date arithmetic: date + days, days + date, date - days give a date; date - date gives days */
static int s_dates( X_ITEM *result, const X_ITEM *left, const X_ITEM *right, char op, int *handled )
{
   *handled = 1;

   if( left->nType == X_IT_DATE && s_isNumber( right ) && op == '+' )
      s_dateResult( result, left->u.integer + s_asDays( right ) );
   else if( left->nType == X_IT_DATE && s_isNumber( right ) )
      s_dateResult( result, left->u.integer - s_asDays( right ) );
   else if( op == '+' && s_isNumber( left ) && right->nType == X_IT_DATE )
      s_dateResult( result, s_asDays( left ) + right->u.integer );
   else if( op == '-' && left->nType == X_IT_DATE && right->nType == X_IT_DATE )
      s_numberResult( result, 1, left->u.integer - right->u.integer, 0.0 );
   else
      *handled = 0;

   return 1;
}

/* ---------------------------------------------------------------------------------------------
   Comparison
   --------------------------------------------------------------------------------------------- */

/* UINT64 against a double: -1, 0 or 1 from the side of the UINT64 */
static int s_compareUInt64Double( uint64_t integer, double real )
{
   uint64_t whole;

   if( real < 0.0 )
      return 1;

   if( real >= 0x1p64 )
      return -1;

   whole = ( uint64_t )real;
   if( integer < whole )
      return -1;

   if( integer > whole )
      return 1;

   if( real > trunc( real ) )
      return -1;

   return 0;
}

/* Compare two values: *cmp is -1, 0 or 1; 0 when they cannot be compared */
static int s_compare( const X_ITEM *left, const X_ITEM *right, int *cmp, int forceExact )
{
   int bothIntegers = left->nType == X_IT_INTEGER && right->nType == X_IT_INTEGER;
   int bothDates = left->nType == X_IT_DATE && right->nType == X_IT_DATE;
   int unsignedLeft = X_IS_UINT64( left ) && right->nType == X_IT_DOUBLE;
   int unsignedRight = left->nType == X_IT_DOUBLE && X_IS_UINT64( right );

   if( X_IS_INTEGER( left ) && X_IS_INTEGER( right ) )
   {
      __int128 leftWide = s_asWide( left );
      __int128 rightWide = s_asWide( right );

      if( leftWide < rightWide )
         *cmp = -1;
      else if( leftWide > rightWide )
         *cmp = 1;
      else
         *cmp = 0;

      return 1;
   }

   if( unsignedLeft || unsignedRight )
   {
      uint64_t integer = IIF( X_IS_UINT64( left ), left->u.unsignedInteger, right->u.unsignedInteger );
      double real = IIF( X_IS_UINT64( left ), right->u.dbl, left->u.dbl );

      if( isnan( real ) )
         *cmp = 1;
      else
      {
         *cmp = s_compareUInt64Double( integer, real );
         if( !X_IS_UINT64( left ) )
            *cmp = -*cmp;
      }

      return 1;
   }

   /* A date holds its Julian day number in u.integer, so dates compare like integers */
   if( bothIntegers || bothDates )
   {
      if( left->u.integer < right->u.integer )
         *cmp = -1;
      else if( left->u.integer > right->u.integer )
         *cmp = 1;
      else
         *cmp = 0;

      return 1;
   }

   if( s_isNumber( left ) && s_isNumber( right ) )
   {
      double leftDouble = s_asDouble( left );
      double rightDouble = s_asDouble( right );

      /* NaN equals nothing, not even 0 */
      if( leftDouble < rightDouble )
         *cmp = -1;
      else if( leftDouble > rightDouble || isnan( leftDouble ) || isnan( rightDouble ) )
         *cmp = 1;
      else
         *cmp = 0;

      return 1;
   }

   if( left->nType == X_IT_STRING && right->nType == X_IT_STRING )
   {
      const char *leftText = left->u.string.data;
      const char *rightText = right->u.string.data;
      size_t leftLength = left->u.string.len;
      size_t rightLength = right->u.string.len;
      size_t length;
      int exact = forceExact || x_getExact();

      if( exact && !forceExact )
      {
         while( leftLength > rightLength && leftText[leftLength - 1] == ' ' )
            --leftLength;

         while( rightLength > leftLength && rightText[rightLength - 1] == ' ' )
            --rightLength;
      }

      length = IIF( leftLength < rightLength, leftLength, rightLength );
      *cmp = memcmp( leftText, rightText, length );

      if( *cmp == 0 && leftLength < rightLength )
         *cmp = -1;
      else if( *cmp == 0 && exact && leftLength > rightLength )
         *cmp = 1;

      return 1;
   }

   if( left->nType == X_IT_LOGICAL && right->nType == X_IT_LOGICAL )
   {
      *cmp = left->u.logical - right->u.logical;
      return 1;
   }

   return 0;
}

/* <, <=, > and >=: op is '<', 'l', '>' or 'g' */
static int s_relation( X_ITEM *result, const X_ITEM *left, const X_ITEM *right, char op )
{
   int cmp;
   int code;
   int truth;

   switch( op )
   {
   case '<':
      code = 1073;
      break;

   case 'l':
      code = 1074;
      break;

   case '>':
      code = 1075;
      break;

   default:
      code = 1076;
      break;
   }

   if( !s_compare( left, right, &cmp, 0 ) )
      return x_errorRaise( result, code, left, right );

   switch( op )
   {
   case '<':
      truth = cmp < 0;
      break;

   case 'l':
      truth = cmp <= 0;
      break;

   case '>':
      truth = cmp > 0;
      break;

   default:
      truth = cmp >= 0;
      break;
   }

   x_itemClear( result );
   result->nType = X_IT_LOGICAL;
   result->u.logical = truth;

   return 1;
}

/* ==, = and != by the error code: 1070 exactly equal, 1071 equal, 1072 not equal */
static int s_equality( X_ITEM *result, const X_ITEM *left, const X_ITEM *right, int code )
{
   int cmp, equal;

   if( left->nType == X_IT_NIL || right->nType == X_IT_NIL )
      equal = left->nType == X_IT_NIL && right->nType == X_IT_NIL;
   else if( left->nType == X_IT_ARRAY && right->nType == X_IT_ARRAY && code == 1070 )
      equal = left->u.pArr == right->u.pArr;
   else if( X_IS_POINTER( left ) && X_IS_POINTER( right ) )
      equal = left->u.pPointer == right->u.pPointer;
   else
   {
      if( !s_compare( left, right, &cmp, code == 1070 ) )
         return x_errorRaise( result, code, left, right );

      equal = cmp == 0;
   }

   x_itemClear( result );
   result->nType = X_IT_LOGICAL;
   result->u.logical = IIF( code == 1072, !equal, equal );

   return 1;
}

/* ---------------------------------------------------------------------------------------------
   Operators
   --------------------------------------------------------------------------------------------- */

/* left $ right */
int x_itemContains( X_CALL_CTX *ctx, X_ITEM *result, const X_ITEM *left, const X_ITEM *right )
{
   int handled;
   int status = s_objectOperator( ctx, result, left, right, X_OO_OP_INSTRING, &handled );
   int found;

   if( handled )
      return status;

   if( left->nType != X_IT_STRING || right->nType != X_IT_STRING )
      return x_errorRaise( result, 1089, left, right );

   found = left->u.string.len == 0;
   if( !found )
      found = x_strAt( left->u.string.data, left->u.string.len, right->u.string.data, right->u.string.len ) != 0;

   x_itemClear( result );
   result->nType = X_IT_LOGICAL;
   result->u.logical = found;

   return 1;
}

/* left % right */
int x_itemMod( X_CALL_CTX *ctx, X_ITEM *result, const X_ITEM *left, const X_ITEM *right )
{
   int handled;
   int status = s_objectOperator( ctx, result, left, right, X_OO_OP_MOD, &handled );

   if( handled )
      return status;

   return s_numbers( result, left, right, '%' );
}

/* left * right */
int x_itemMul( X_CALL_CTX *ctx, X_ITEM *result, const X_ITEM *left, const X_ITEM *right )
{
   int handled;
   int status = s_objectOperator( ctx, result, left, right, X_OO_OP_MULT, &handled );

   if( handled )
      return status;

   return s_numbers( result, left, right, '*' );
}

/* left ^ right */
int x_itemPow( X_CALL_CTX *ctx, X_ITEM *result, const X_ITEM *left, const X_ITEM *right )
{
   int handled;
   int status = s_objectOperator( ctx, result, left, right, X_OO_OP_POWER, &handled );

   if( handled )
      return status;

   return s_numbers( result, left, right, '^' );
}

/* left + right: numbers, strings joined, or a date and days */
int x_itemAdd( X_CALL_CTX *ctx, X_ITEM *result, const X_ITEM *left, const X_ITEM *right )
{
   int handled;
   int status = s_objectOperator( ctx, result, left, right, X_OO_OP_PLUS, &handled );

   if( handled )
      return status;

   if( left->nType == X_IT_STRING && right->nType == X_IT_STRING )
   {
      size_t leftLength = left->u.string.len;
      size_t rightLength = right->u.string.len;
      char *text = ( char * )x_xgrab( leftLength + rightLength + 1 );

      memcpy( text, left->u.string.data, leftLength );
      memcpy( text + leftLength, right->u.string.data, rightLength );
      text[leftLength + rightLength] = '\0';

      x_itemPutStringBuffer( result, text, leftLength + rightLength );

      return 1;
   }

   status = s_dates( result, left, right, '+', &handled );
   if( handled )
      return status;

   return s_numbers( result, left, right, '+' );
}

/* target += right, where left is the value of target taken before right was evaluated. When target still
   holds that string and owns it alone, right is appended in place; otherwise target := left + right. */
int x_itemAddAssign( X_CALL_CTX *ctx, X_ITEM *result, X_ITEM *target, X_ITEM *left, const X_ITEM *right )
{
   int strings = target->nType == X_IT_STRING && left->nType == X_IT_STRING && right->nType == X_IT_STRING;
   int status;

   if( strings && target->u.string.data == left->u.string.data && right->u.string.data != left->u.string.data )
   {
      x_itemClear( left );
      if( x_stringAppend( &target->u.string, right->u.string.data, right->u.string.len ) )
      {
         x_itemCopy( result, target );
         return 1;
      }

      x_itemCopy( left, target );
   }

   status = x_itemAdd( ctx, result, left, right );
   x_itemCopy( target, result );

   return status;
}

/* x_itemAddAssign() for a typed STRING variable; snapshot and left both hold the value of target
   taken before right was evaluated. A result that is not a string leaves target unchanged. */
int x_stringAddAssign( X_CALL_CTX *ctx, X_ITEM *result, X_STRING *target, X_ITEM *snapshot, X_ITEM *left,
                       const X_ITEM *right )
{
   X_ITEM view = { X_IT_STRING, { 0 } };
   int strings = left->nType == X_IT_STRING && right->nType == X_IT_STRING;
   int status;

   if( strings && target->data == left->u.string.data && right->u.string.data != left->u.string.data )
   {
      x_itemClear( snapshot );
      x_itemClear( left );
      if( x_stringAppend( target, right->u.string.data, right->u.string.len ) )
      {
         view.u.string = *target;
         x_itemCopy( result, &view );
         return 1;
      }

      view.u.string = *target;
      x_itemCopy( left, &view );
   }

   status = x_itemAdd( ctx, result, left, right );
   if( result->nType == X_IT_STRING )
   {
      x_xrelease( target->data );
      target->data = ( char * )x_xretain( result->u.string.data );
      target->len = result->u.string.len;
   }

   return status;
}

/* value + 1 */
int x_itemInc( X_CALL_CTX *ctx, X_ITEM *result, const X_ITEM *value )
{
   int handled;
   int status = s_objectOperator( ctx, result, value, NULL, X_OO_OP_INC, &handled );

   if( handled )
      return status;

   return s_updateNumber( result, value, 1 );
}

/* left - right: numbers, strings joined with the trailing spaces of left moved to the end, or dates */
int x_itemSub( X_CALL_CTX *ctx, X_ITEM *result, const X_ITEM *left, const X_ITEM *right )
{
   int handled;
   int status = s_objectOperator( ctx, result, left, right, X_OO_OP_MINUS, &handled );

   if( handled )
      return status;

   if( left->nType == X_IT_STRING && right->nType == X_IT_STRING )
   {
      const char *leftText = left->u.string.data;
      size_t leftLength = left->u.string.len;
      size_t rightLength = right->u.string.len;
      size_t trimmed = leftLength;
      size_t spaces;
      char *text;

      while( trimmed > 0 && leftText[trimmed - 1] == ' ' )
         --trimmed;

      spaces = leftLength - trimmed;
      text = ( char * )x_xgrab( leftLength + rightLength + 1 );

      memcpy( text, leftText, trimmed );
      memcpy( text + trimmed, right->u.string.data, rightLength );
      memset( text + trimmed + rightLength, ' ', spaces );
      text[leftLength + rightLength] = '\0';

      x_itemPutStringBuffer( result, text, leftLength + rightLength );

      return 1;
   }

   status = s_dates( result, left, right, '-', &handled );
   if( handled )
      return status;

   return s_numbers( result, left, right, '-' );
}

/* -value */
int x_itemNeg( X_CALL_CTX *ctx, X_ITEM *result, const X_ITEM *value )
{
   int integer;
   int handled;
   int status = s_objectOperator( ctx, result, value, NULL, X_OO_OP_MINUS, &handled );

   if( handled )
      return status;

   if( !s_isNumber( value ) )
      return x_errorRaise( result, 1080, value, NULL );

   if( X_IS_UINT64( value ) )
   {
      if( value->u.unsignedInteger != 0 )
         return x_errorRaiseNamed( result, 3106, "UINT64 negation" );

      x_itemCopy( result, value );
      return 1;
   }

   integer = value->nType == X_IT_INTEGER && value->u.integer != INT64_MIN;
   s_numberResult( result, integer, IIF( integer, -value->u.integer, 0 ), -s_asDouble( value ) );

   return 1;
}

/* value - 1 */
int x_itemDec( X_CALL_CTX *ctx, X_ITEM *result, const X_ITEM *value )
{
   int handled;
   int status = s_objectOperator( ctx, result, value, NULL, X_OO_OP_DEC, &handled );

   if( handled )
      return status;

   return s_updateNumber( result, value, 0 );
}

/* .NOT. value */
int x_itemNot( X_CALL_CTX *ctx, X_ITEM *result, const X_ITEM *value )
{
   int handled;
   int status = s_objectOperator( ctx, result, value, NULL, X_OO_OP_NOT, &handled );

   if( handled )
      return status;

   if( value->nType != X_IT_LOGICAL )
      return x_errorRaise( result, 1077, value, NULL );

   x_itemClear( result );
   result->nType = X_IT_LOGICAL;
   result->u.logical = !value->u.logical;

   return 1;
}

/* left / right */
int x_itemDiv( X_CALL_CTX *ctx, X_ITEM *result, const X_ITEM *left, const X_ITEM *right )
{
   int handled;
   int status = s_objectOperator( ctx, result, left, right, X_OO_OP_DIVIDE, &handled );

   if( handled )
      return status;

   return s_numbers( result, left, right, '/' );
}

/* left < right */
int x_itemLt( X_CALL_CTX *ctx, X_ITEM *result, const X_ITEM *left, const X_ITEM *right )
{
   int handled;
   int status = s_objectOperator( ctx, result, left, right, X_OO_OP_LESS, &handled );

   if( handled )
      return status;

   return s_relation( result, left, right, '<' );
}

/* left <= right */
int x_itemLe( X_CALL_CTX *ctx, X_ITEM *result, const X_ITEM *left, const X_ITEM *right )
{
   int handled;
   int status = s_objectOperator( ctx, result, left, right, X_OO_OP_LESSEQUAL, &handled );

   if( handled )
      return status;

   return s_relation( result, left, right, 'l' );
}

/* left != right */
int x_itemNe( X_CALL_CTX *ctx, X_ITEM *result, const X_ITEM *left, const X_ITEM *right )
{
   int handled;
   int status = s_objectOperator( ctx, result, left, right, X_OO_OP_NOTEQUAL, &handled );

   if( handled )
      return status;

   return s_equality( result, left, right, 1072 );
}

/* left = right */
int x_itemEqS( X_CALL_CTX *ctx, X_ITEM *result, const X_ITEM *left, const X_ITEM *right )
{
   int handled;
   int status = s_objectOperator( ctx, result, left, right, X_OO_OP_EQUAL, &handled );

   if( handled )
      return status;

   return s_equality( result, left, right, 1071 );
}

/* left == right */
int x_itemEq( X_CALL_CTX *ctx, X_ITEM *result, const X_ITEM *left, const X_ITEM *right )
{
   int handled;
   int status = s_objectOperator( ctx, result, left, right, X_OO_OP_EXACTEQUAL, &handled );

   if( handled )
      return status;

   return s_equality( result, left, right, 1070 );
}

/* left > right */
int x_itemGt( X_CALL_CTX *ctx, X_ITEM *result, const X_ITEM *left, const X_ITEM *right )
{
   int handled;
   int status = s_objectOperator( ctx, result, left, right, X_OO_OP_GREATER, &handled );

   if( handled )
      return status;

   return s_relation( result, left, right, '>' );
}

/* left >= right */
int x_itemGe( X_CALL_CTX *ctx, X_ITEM *result, const X_ITEM *left, const X_ITEM *right )
{
   int handled;
   int status = s_objectOperator( ctx, result, left, right, X_OO_OP_GREATEREQUAL, &handled );

   if( handled )
      return status;

   return s_relation( result, left, right, 'g' );
}
