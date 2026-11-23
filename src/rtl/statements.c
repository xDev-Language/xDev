/*
 * Copyright (c) 2024-2026 xDev-Language
 *
 * Library...: xDev Runtime
 * File......: statements.c
 *
 */

#include "internal.h"

#include "statements.h"
#include "operators.h"

#include <math.h>
#include <string.h>

/* The value of x_getExact() */
static int s_exact;

/* ---------------------------------------------------------------------------------------------
   FOR loop
   --------------------------------------------------------------------------------------------- */

/* Has the counter passed the limit? The sign of the step gives the direction */
int x_forTest( X_CALL_CTX *ctx, X_ITEM *dest, const X_ITEM *counter, const X_ITEM *limit, const X_ITEM *step )
{
   X_ITEM direction __attribute__( ( cleanup( x_itemClear ) ) ) = X_ITEM_NIL_INIT;
   const X_ITEM zero = { X_IT_INTEGER, { .integer = 0 } };
   int backwards;

   x_itemLt( ctx, &direction, step, &zero );

   backwards = direction.u.logical;
   x_itemClear( &direction );

   if( backwards )
      return x_itemLt( ctx, dest, counter, limit );

   return x_itemGt( ctx, dest, counter, limit );
}

/* The counter plus one */
int x_forIncrement( X_CALL_CTX *ctx, X_ITEM *dest, const X_ITEM *counter )
{
   return x_itemInc( ctx, dest, counter );
}

/* ---------------------------------------------------------------------------------------------
   Arguments of INIT procedures
   --------------------------------------------------------------------------------------------- */

/* The command line arguments as string items; NULL without any */
X_ARG *x_initArgs( int32_t count, const char *const *values )
{
   X_ARG *args;
   int32_t i;

   if( count <= 0 )
      return NULL;

   args = ( X_ARG * )x_xgrabArray( ( size_t )count, sizeof( X_ARG ) );

   for( i = 0; i < count; ++i )
   {
      X_ITEM *item = ( X_ITEM * )x_xgrab( sizeof( X_ITEM ) );

      *item = ( X_ITEM )X_ITEM_NIL_INIT;
      args[i].value = item;
      args[i].byReference = 0;
      x_itemPutCL( item, values[i], strlen( values[i] ) );
   }

   return args;
}

/* Release the arguments of x_initArgs() */
void x_initArgsFree( int32_t count, X_ARG *args )
{
   int32_t i;

   for( i = 0; i < count; ++i )
   {
      X_ITEM *item = ( X_ITEM * )args[i].value;

      x_itemClear( item );
      x_xfree( item );
   }

   x_xfree( args );
}

/* ---------------------------------------------------------------------------------------------
   Arrays with dimensions: LOCAL a[2, 3]
   --------------------------------------------------------------------------------------------- */

/* The length of one dimension; 0 when it is not a number or out of range */
int x_arrayDimensionLength( const X_ITEM *dimension, int64_t *length )
{
   *length = 0;

   if( dimension->nType == X_IT_INTEGER )
      *length = dimension->u.integer;
   else if( dimension->nType == X_IT_UINT64 )
   {
      if( dimension->u.unsignedInteger > INT64_MAX )
         return 0;

      *length = ( int64_t )dimension->u.unsignedInteger;
   }
   else if( dimension->nType == X_IT_DOUBLE )
   {
      if( !isfinite( dimension->u.dbl ) || dimension->u.dbl <= -1 || dimension->u.dbl >= 0x1p63 )
         return 0;

      *length = ( int64_t )dimension->u.dbl;
   }
   else
      return 0;

   return *length >= 0 && ( uint64_t )*length <= SIZE_MAX / sizeof( X_ITEM );
}

/* A new array with count dimensions; each element of a dimension holds an array of the next one */
int x_arrayDimensions( X_ITEM *dest, int32_t count, const X_ARG *dimensions )
{
   const X_ITEM *dimension;
   int64_t length;
   int64_t i;

   if( count <= 0 )
      return x_arrayNew( dest, 0 );

   dimension = dimensions[0].value;

   if( !x_arrayDimensionLength( dimension, &length ) )
      return x_errorRaise( dest, 1131, dimension, NULL );

   if( !x_arrayNew( dest, length ) )
      return x_errorRaise( dest, 1131, dimension, NULL );

   if( count > 1 )
   {
      for( i = 0; i < length; ++i )
         x_arrayDimensions( x_arrayGetItemPtr( dest, ( size_t )i + 1 ), count - 1, dimensions + 1 );
   }

   return 1;
}

/* ---------------------------------------------------------------------------------------------
   SWITCH
   --------------------------------------------------------------------------------------------- */

/* The value of SWITCH: an integer or a string, else error 3104 */
int x_switchValue( X_ITEM *dest, const X_ITEM *value )
{
   if( !X_IS_INTEGER( value ) && value->nType != X_IT_STRING )
      return x_errorRaise( dest, 3104, value, NULL );

   x_itemCopy( dest, value );

   return 1;
}

/* Does the value of SWITCH match the CASE label? */
int x_switchMatch( const X_ITEM *value, const X_ITEM *label )
{
   if( X_IS_INTEGER( value ) && X_IS_INTEGER( label ) )
   {
      const X_ITEM *signedValue = IIF( X_IS_UINT64( value ), label, value );
      const X_ITEM *unsignedValue = IIF( X_IS_UINT64( value ), value, label );

      if( X_IS_UINT64( value ) == X_IS_UINT64( label ) )
         return value->u.unsignedInteger == label->u.unsignedInteger;

      return signedValue->u.integer >= 0 && ( uint64_t )signedValue->u.integer == unsignedValue->u.unsignedInteger;
   }

   if( value->nType != label->nType )
      return 0;

   if( value->nType == X_IT_INTEGER )
      return value->u.integer == label->u.integer;

   if( value->nType == X_IT_STRING )
   {
      const char *valueText = value->u.string.data;
      const char *labelText = label->u.string.data;
      size_t length = value->u.string.len;

      return length == label->u.string.len && memcmp( valueText, labelText, length ) == 0;
   }

   return 0;
}

/* SET EXACT for comparisons */
int x_getExact( void )
{
   return s_exact;
}

/* ---------------------------------------------------------------------------------------------
   FOR EACH: the compiler writes the loop with these; it goes through an array, a hash (its values)
   or a text (its bytes)
   --------------------------------------------------------------------------------------------- */

/* var:enumBase( xNew ): the old base gets the variable, the base becomes xNew, the variable takes its element
   at the same position and the number of turns follows the new base; the other bases of the
   same FOR EACH come after @xVar
   x_eachBase( @xBase, xNew, @nCount, @nPos, @xVar, [xBaseN] ) --> xOldBase */
void x_eachBase( X_CALL_CTX *ctx )
{
   X_ITEM *base = ( X_ITEM * )ctx->args[0].value;
   X_ITEM *count = ( X_ITEM * )ctx->args[2].value;
   X_ITEM *var = ( X_ITEM * )ctx->args[4].value;
   int64_t pos = x_itemGetNInt( ( X_ITEM * )ctx->args[3].value );
   int64_t value, length;
   int i;

   x_enumPut( ctx, pos, var );
   x_itemReturn( ctx, base );
   x_itemCopy( base, x_paritem( ctx, 2 ) );
   if( !x_enumLength( base, &value ) )
   {
      ( void )x_errorRaise( ctx->result, 1068, base, NULL );
      return;
   }

   x_enumGet( base, pos, var );
   for( i = 6; i <= x_pcount( ctx ); ++i )
   {
      if( x_enumLength( x_paritem( ctx, i ), &length ) )
         value = IIF( length < value, length, value );
   }

   x_itemPutNInt( count, value );
}

/* The number of turns of FOR EACH: the shortest of the values gone through
   x_eachCount( xBase1, [xBaseN] ) --> nCount */
void x_eachCount( X_CALL_CTX *ctx )
{
   int64_t count = INT64_MAX;
   int i;

   for( i = 1; i <= x_pcount( ctx ); ++i )
   {
      int64_t length;

      if( !x_enumLength( x_paritem( ctx, i ), &length ) )
      {
         ( void )x_errorRaise( ctx->result, 1068, x_paritem( ctx, i ), NULL );
         return;
      }

      count = IIF( length < count, length, count );
   }

   x_retnint( ctx, IIF( x_pcount( ctx ) > 0, count, 0 ) );
}

/* The position of the turn nTurn, from the end with DESCEND
   x_eachPos( xBase, nTurn, lDescend ) --> nPos */
void x_eachPos( X_CALL_CTX *ctx )
{
   int64_t length = 0;
   int64_t turn = x_parnint( ctx, 2 );

   x_enumLength( x_paritem( ctx, 1 ), &length );

   if( X_ISLOG( ctx, 3 ) && x_parl( ctx, 3 ) )
      x_retnint( ctx, length - turn + 1 );
   else
      x_retnint( ctx, turn );
}

/* The element at a position: the element of an array, the value of a hash, the byte of a text
   x_eachGet( xBase, nPos ) --> xValue */
void x_eachGet( X_CALL_CTX *ctx )
{
   x_enumGet( x_paritem( ctx, 1 ), x_parnint( ctx, 2 ), ctx->result );
}

/* var:enumIndex( nNew ): the element gets the variable, the turn goes to position nNew and the variable
   takes its element; the loop goes on from there. The turn is shared by all variables of
   one FOR EACH.
   x_eachIndex( @xBase, @nPos, @nTurn, @xVar, nNew, lDescend ) --> nOldPos */
void x_eachIndex( X_CALL_CTX *ctx )
{
   X_ITEM *pos = ( X_ITEM * )ctx->args[1].value;
   X_ITEM *turn = ( X_ITEM * )ctx->args[2].value;
   X_ITEM *var = ( X_ITEM * )ctx->args[3].value;
   int64_t old = x_itemGetNInt( pos );
   int64_t index = x_parnint( ctx, 5 );
   int64_t length = 0;

   x_enumPut( ctx, old, var );
   x_enumLength( x_paritem( ctx, 1 ), &length );
   x_itemPutNInt( pos, index );
   x_itemPutNInt( turn, IIF( x_parl( ctx, 6 ), length - index + 1, index ) );
   x_enumGet( x_paritem( ctx, 1 ), index, var );

   x_retnint( ctx, old );
}

/* The key at a position of a hash, NIL for an array or a text
   x_eachKey( xBase, nPos ) --> xKey */
void x_eachKey( X_CALL_CTX *ctx )
{
   const X_ITEM *key = x_hashKeyAt( x_paritem( ctx, 1 ), ( size_t )x_parnint( ctx, 2 ) );

   if( key != NULL )
      x_itemReturn( ctx, key );
}

/* The value of the loop variable goes back into its element, as the variable stands for the element; a text
   changes only its own copy, which goes back to the variable after IN @cText
   x_eachPut( @xBase, nPos, xValue ) --> NIL */
void x_eachPut( X_CALL_CTX *ctx )
{
   x_enumPut( ctx, x_parnint( ctx, 2 ), x_paritem( ctx, 3 ) );
}

/* var:enumValue( xNew ): the variable takes xNew, which goes into the element at the end of the turn
   x_eachSwap( @xVar, xNew ) --> xOldValue */
void x_eachSwap( X_CALL_CTX *ctx )
{
   X_ITEM *var = ( X_ITEM * )ctx->args[0].value;

   x_itemReturn( ctx, var );
   x_itemCopy( var, x_paritem( ctx, 2 ) );
}
