/*
 * Copyright (c) 2024-2026 xDev-Language
 *
 * Library...: xDev Runtime
 * File......: internal-array.c
 *
 */

#include "internal-private.h"
#include "classes.h"
#include "operators.h"
#include "statements.h"

#include <math.h>
#include <string.h>

#define X_HASH_NONE SIZE_MAX

/* The random start of the hash codes of this run: keys chosen to collide work only by chance */
static uint64_t s_hashSeed;

/* ---------------------------------------------------------------------------------------------
   Arrays: AClone(), AEval(), AScan() and ASort()
   --------------------------------------------------------------------------------------------- */

/* AClone(): nested arrays become new arrays and an array met again gives the clone made before, also in
   cycles; objects are not cloned */
void x_arrayClone( X_ITEM *result, const X_ITEM *source )
{
   X_ITEM sources __attribute__( ( cleanup( x_itemClear ) ) ) = X_ITEM_NIL_INIT;
   X_ITEM copies __attribute__( ( cleanup( x_itemClear ) ) ) = X_ITEM_NIL_INIT;
   X_ITEM clone __attribute__( ( cleanup( x_itemClear ) ) ) = X_ITEM_NIL_INIT;
   int64_t pos;

   if( x_arrayIsObject( source ) )
   {
      x_itemCopy( result, source );
      return;
   }

   x_arrayNew( &sources, 0 );
   x_arrayNew( &copies, 0 );
   x_arrayNew( &clone, x_arrayLen( source ) );
   x_arrayAdd( &sources, source );
   x_arrayAdd( &copies, &clone );

   for( pos = 1; pos <= x_arrayLen( &sources ); ++pos )
   {
      X_ITEM input __attribute__( ( cleanup( x_itemClear ) ) ) = X_ITEM_NIL_INIT;
      X_ITEM output __attribute__( ( cleanup( x_itemClear ) ) ) = X_ITEM_NIL_INIT;
      int64_t i;

      x_arrayGet( &sources, ( size_t )pos, &input );
      x_arrayGet( &copies, ( size_t )pos, &output );

      for( i = 1; i <= x_arrayLen( &input ); ++i )
      {
         const X_ITEM *value = x_arrayGetItemPtr( &input, ( size_t )i );
         int64_t found;

         if( !X_IS_ARRAY( value ) || x_arrayIsObject( value ) )
         {
            x_arraySet( &output, ( size_t )i, value );
            continue;
         }

         for( found = 1; found <= x_arrayLen( &sources ); ++found )
         {
            if( x_arrayId( x_arrayGetItemPtr( &sources, ( size_t )found ) ) == x_arrayId( value ) )
               break;
         }

         if( found > x_arrayLen( &sources ) )
         {
            X_ITEM child __attribute__( ( cleanup( x_itemClear ) ) ) = X_ITEM_NIL_INIT;

            x_arrayNew( &child, x_arrayLen( value ) );
            x_arrayAdd( &sources, value );
            x_arrayAdd( &copies, &child );
         }

         x_arraySet( &output, ( size_t )i, x_arrayGetItemPtr( &copies, ( size_t )found ) );
      }
   }

   x_itemCopy( result, &clone );
}

/* The last position of count elements from start, cut at the end of the array */
static int64_t s_arrayEnd( const X_ITEM *array, int64_t start, int64_t count )
{
   int64_t length = x_arrayLen( array );

   if( start > length || count <= 0 )
      return start - 1;

   if( count > length - start + 1 )
      count = length - start + 1;

   return start + count - 1;
}

/* AEval(): the block gets the element and its position */
void x_arrayEval( X_CALL_CTX *ctx, const X_ITEM *source, const X_ITEM *code, int64_t start, int64_t count )
{
   X_ITEM array __attribute__( ( cleanup( x_itemClear ) ) ) = X_ITEM_NIL_INIT;
   X_ITEM block __attribute__( ( cleanup( x_itemClear ) ) ) = X_ITEM_NIL_INIT;
   int64_t end, i;

   x_itemCopy( &array, source );
   x_itemCopy( &block, code );
   start = IIF( start < 1, 1, start );
   end = s_arrayEnd( &array, start, count );

   for( i = start; i <= end && i <= x_arrayLen( &array ); ++i )
   {
      X_ITEM value __attribute__( ( cleanup( x_itemClear ) ) ) = X_ITEM_NIL_INIT;
      X_ITEM result __attribute__( ( cleanup( x_itemClear ) ) ) = X_ITEM_NIL_INIT;
      X_ITEM index = { X_IT_INTEGER, { .integer = i } };
      X_ARG args[2] = { { &value, 0 }, { &index, 0 } };

      x_arrayGet( &array, ( size_t )i, &value );
      x_evalBlock( ctx, &result, &block, 2, args );
   }
}

/* Can AScan() compare these two values with = or ==? Numbers, or NIL, strings or logical values of one type */
static int s_arrayScanComparable( const X_ITEM *value, const X_ITEM *needle )
{
   if( X_IS_NUMERIC( value ) && X_IS_NUMERIC( needle ) )
      return 1;

   if( value->nType != needle->nType )
      return 0;

   return value->nType == X_IT_NIL || value->nType == X_IT_STRING || value->nType == X_IT_LOGICAL;
}

/* AScan(): the position of the first element equal to search, or for which the block returns .T.; 0 when none */
int64_t x_arrayScan( X_CALL_CTX *ctx, const X_ITEM *source, const X_ITEM *search, int64_t start, int64_t count,
                     int exact )
{
   X_ITEM array __attribute__( ( cleanup( x_itemClear ) ) ) = X_ITEM_NIL_INIT;
   X_ITEM needle __attribute__( ( cleanup( x_itemClear ) ) ) = X_ITEM_NIL_INIT;
   int64_t end, i;

   x_itemCopy( &array, source );
   x_itemCopy( &needle, search );
   start = IIF( start < 1, 1, start );
   end = s_arrayEnd( &array, start, count );

   for( i = start; i <= end && i <= x_arrayLen( &array ); ++i )
   {
      X_ITEM value __attribute__( ( cleanup( x_itemClear ) ) ) = X_ITEM_NIL_INIT;
      X_ITEM result __attribute__( ( cleanup( x_itemClear ) ) ) = X_ITEM_NIL_INIT;

      x_arrayGet( &array, ( size_t )i, &value );
      if( X_IS_BLOCK( &needle ) )
      {
         X_ITEM index = { X_IT_INTEGER, { .integer = i } };
         X_ARG args[2] = { { &value, 0 }, { &index, 0 } };

         x_evalBlock( ctx, &result, &needle, 2, args );
         if( X_IS_LOGICAL( &result ) && result.u.logical )
            return i;
      }
      else if( value.nType == X_IT_DATE && needle.nType == X_IT_DATE )
      {
         if( value.u.integer == needle.u.integer )
            return i;
      }
      else if( X_IS_POINTER( &value ) && X_IS_POINTER( &needle ) )
      {
         if( value.u.pPointer == needle.u.pPointer )
            return i;
      }
      else if( exact && X_IS_ARRAY( &value ) && X_IS_ARRAY( &needle ) )
      {
         if( x_arrayId( &value ) == x_arrayId( &needle ) )
            return i;
      }
      else if( s_arrayScanComparable( &value, &needle ) )
      {
         if( exact )
            x_itemEq( ctx, &result, &value, &needle );
         else
            x_itemEqS( ctx, &result, &value, &needle );

         if( result.u.logical )
            return i;
      }
   }

   return 0;
}

/* The order of the types in ASort() without a block */
static int s_sortRank( const X_ITEM *item )
{
   switch( item->nType )
   {
   case X_IT_ARRAY:
      return 1;

   case X_IT_BLOCK:
      return 2;

   case X_IT_STRING:
      return 3;

   case X_IT_LOGICAL:
      return 4;

   case X_IT_DATE:
      return 5;

   case X_IT_INTEGER:
   case X_IT_UINT64:
   case X_IT_DOUBLE:
      return 6;
   }

   return 7;
}

/* Does element left come before element right? The block decides when there is one */
static int s_sortLess( X_CALL_CTX *ctx, X_ARR *array, const X_ITEM *block, int64_t left, int64_t right )
{
   X_ITEM result __attribute__( ( cleanup( x_itemClear ) ) ) = X_ITEM_NIL_INIT;
   const X_ITEM *first = &array->items[left];
   const X_ITEM *second = &array->items[right];

   if( X_IS_BLOCK( block ) )
   {
      X_ITEM firstCopy __attribute__( ( cleanup( x_itemClear ) ) ) = X_ITEM_NIL_INIT;
      X_ITEM secondCopy __attribute__( ( cleanup( x_itemClear ) ) ) = X_ITEM_NIL_INIT;
      X_ARG args[2] = { { &firstCopy, 0 }, { &secondCopy, 0 } };

      x_itemCopy( &firstCopy, first );
      x_itemCopy( &secondCopy, second );
      x_evalBlock( ctx, &result, block, 2, args );

      return x_errorCondition( &result );
   }

   if( s_sortRank( first ) != s_sortRank( second ) )
      return s_sortRank( first ) < s_sortRank( second );

   if( first->nType == X_IT_DATE )
      return first->u.integer < second->u.integer;

   if( first->nType == X_IT_STRING || first->nType == X_IT_LOGICAL || X_IS_NUMERIC( first ) )
   {
      x_itemLt( ctx, &result, first, second );
      return result.u.logical;
   }

   return 0;
}

/* Sift element root down the heap of count elements from start; 0 when the block made the array shorter */
static int s_sortSift( X_CALL_CTX *ctx, X_ARR *array, const X_ITEM *block, int64_t start, int64_t count,
                       int64_t root )
{
   while( root < count / 2 )
   {
      int64_t child = root * 2 + 1;
      X_ITEM swap;
      int less;

      if( child + 1 < count )
      {
         less = s_sortLess( ctx, array, block, start + child, start + child + 1 );
         if( array->len < start + count )
            return 0;

         if( less )
            ++child;
      }

      less = s_sortLess( ctx, array, block, start + root, start + child );
      if( array->len < start + count )
         return 0;

      if( !less )
         break;

      swap = array->items[start + root];
      array->items[start + root] = array->items[start + child];
      array->items[start + child] = swap;
      root = child;
   }

   return 1;
}

/* ASort(): heap sort of count elements from start */
void x_arraySort( X_CALL_CTX *ctx, const X_ITEM *source, const X_ITEM *code, int64_t start, int64_t count )
{
   X_ITEM held __attribute__( ( cleanup( x_itemClear ) ) ) = X_ITEM_NIL_INIT;
   X_ITEM block __attribute__( ( cleanup( x_itemClear ) ) ) = X_ITEM_NIL_INIT;
   X_ARR *array;
   int64_t root, end;

   x_itemCopy( &held, source );
   x_itemCopy( &block, code );
   start = IIF( start < 1, 1, start );
   count = s_arrayEnd( &held, start, count ) - start + 1;
   --start;
   array = ( X_ARR * )held.u.pArr;

   for( root = count / 2; root > 0; --root )
   {
      if( !s_sortSift( ctx, array, &block, start, count, root - 1 ) )
         return;
   }

   for( end = count - 1; end > 0; --end )
   {
      X_ITEM swap = array->items[start];

      array->items[start] = array->items[start + end];
      array->items[start + end] = swap;
      if( !s_sortSift( ctx, array, &block, start, end, 0 ) )
         return;
   }
}

/* ---------------------------------------------------------------------------------------------
   Arrays
   --------------------------------------------------------------------------------------------- */

/* A new array of length NIL elements in dest; 0 when the length is not valid */
int x_arrayNew( X_ITEM *dest, int64_t length )
{
   X_ARR *storage;
   int64_t i;

   if( length < 0 || ( uint64_t )length > SIZE_MAX / sizeof( X_ITEM ) )
      return 0;

   storage = ( X_ARR * )x_xgrab( sizeof( X_ARR ) );
   storage->len = length;
   storage->capacity = ( size_t )length;
   storage->nRefs = 1;
   storage->classHandle = 0;
   storage->items = NULL;

   if( length > 0 )
   {
      X_ITEM nilInit = X_ITEM_NIL_INIT;

      storage->items = ( X_ITEM * )x_xgrabArray( ( size_t )length, sizeof( X_ITEM ) );
      for( i = 0; i < length; ++i )
         storage->items[i] = nilInit;
   }

   x_gcRegister( storage, &storage->nRefs, X_GC_ARRAY );
   x_itemReplace( dest, ( X_ITEM ){ X_IT_ARRAY, { .pArr = storage } } );

   return 1;
}

/* A new item with a new array; NULL when the length is not valid */
X_ITEM *x_itemArrayNew( size_t length )
{
   X_ITEM *item = x_itemNew( NULL );

   if( length > ( size_t )INT64_MAX || !x_arrayNew( item, ( int64_t )length ) )
   {
      x_itemRelease( item );
      return NULL;
   }

   return item;
}

/* The length of an array; 0 for any other value */
int64_t x_arrayLen( const X_ITEM *array )
{
   if( array->nType != X_IT_ARRAY )
      return 0;

   return ( ( const X_ARR * )array->u.pArr )->len;
}

/* Is the array an object? */
int x_arrayIsObject( const X_ITEM *array )
{
   return x_arrayClass( array ) != 0;
}

/* The class handle of an object; 0 for a plain array or any other value */
uint32_t x_arrayClass( const X_ITEM *array )
{
   if( array == NULL || array->nType != X_IT_ARRAY || array->u.pArr == NULL )
      return 0;

   return ( ( const X_ARR * )array->u.pArr )->classHandle;
}

/* Make an array an object of an associated class; its length must be the instance data count */
int x_arraySetClass( X_ITEM *array, uint32_t classHandle )
{
   const X_CLASS *classInfo;
   X_ARR *storage;

   if( array == NULL || array->nType != X_IT_ARRAY || array->u.pArr == NULL )
      return 0;

   storage = ( X_ARR * )array->u.pArr;
   if( storage->classHandle == classHandle )
      return 1;

   if( storage->classHandle != 0 )
      return 0;

   classInfo = x_clsGet( classHandle );
   if( classInfo == NULL || !classInfo->associated || ( size_t )storage->len != classInfo->instanceDataCount )
      return 0;

   storage->classHandle = classHandle;

   return 1;
}

/* The identity of an array; NULL for any other value */
void *x_arrayId( const X_ITEM *array )
{
   return IIF( array->nType == X_IT_ARRAY, array->u.pArr, NULL );
}

/* The block of x_arrayBlock(): without an argument it reads the element, with one it assigns it */
static void s_arrayBlockEval( X_CALL_CTX *ctx, X_ITEM *const *env )
{
   const X_ITEM *array = x_boxItem( ( X_BOX * )env[0] );
   const X_ITEM *index = x_boxItem( ( X_BOX * )env[1] );

   if( x_pcount( ctx ) == 0 )
      x_itemArrayGet( ctx, ctx->result, array, index );
   else
      x_itemArraySet( ctx, ctx->result, array, index, x_paritem( ctx, 1 ) );
}

/* ArrayBlock(): a block that gets or sets one element */
void x_arrayBlock( X_CALL_CTX *ctx, const X_ITEM *array, const X_ITEM *index )
{
   X_BOX *arrayBox = x_boxCapture( array );
   X_BOX *indexBox = x_boxCapture( index );
   X_ITEM *environment[2] = { ( X_ITEM * )arrayBox, ( X_ITEM * )indexBox };

   x_itemClosureInto( ctx, ctx->result, s_arrayBlockEval, environment, 2 );
   x_boxRelease( indexBox );
   x_boxRelease( arrayBox );
}

/* The arguments of a call as an array */
void x_arrayParams( X_ITEM *result, int32_t argc, const X_ARG *args )
{
   int32_t index;

   x_arrayNew( result, argc );
   for( index = 0; index < argc; ++index )
      x_arraySet( result, ( size_t )index + 1, args[index].value );
}

/* The reference count of an array; 0 for any other value */
size_t x_arrayRefs( const X_ITEM *array )
{
   return IIF( array->nType == X_IT_ARRAY, ( ( const X_ARR * )array->u.pArr )->nRefs, 0 );
}

/* ASize(): new elements are NIL; an object keeps its size */
int x_arraySize( X_ITEM *array, size_t length )
{
   X_ARR *storage;
   int64_t oldLength;
   int64_t i;

   if( array->nType != X_IT_ARRAY || length > ( size_t )INT64_MAX || length > SIZE_MAX / sizeof( X_ITEM ) )
      return 0;

   storage = ( X_ARR * )array->u.pArr;
   oldLength = storage->len;

   if( ( size_t )oldLength == length )
      return 1;

   if( storage->classHandle != 0 )
      return 0;

   unsigned int depth __attribute__( ( cleanup( x_gcRestore ) ) ) = x_gcPause();

   if( length < ( size_t )oldLength )
   {
      for( i = ( int64_t )length; i < oldLength; ++i )
         x_itemClear( &storage->items[i] );
   }

   /* Grow with a reserve so repeated AAdd() stays linear; give memory back when mostly unused */
   if( length > storage->capacity )
   {
      size_t capacity = storage->capacity + storage->capacity / 2;

      if( capacity < length || capacity > SIZE_MAX / sizeof( X_ITEM ) || capacity > ( size_t )INT64_MAX )
         capacity = length;

      storage->items = ( X_ITEM * )x_xreallocArray( storage->items, capacity, sizeof( X_ITEM ) );
      storage->capacity = capacity;
   }
   else if( length < storage->capacity / 4 )
   {
      storage->items = ( X_ITEM * )x_xreallocArray( storage->items, length, sizeof( X_ITEM ) );
      storage->capacity = length;
   }

   if( length > ( size_t )oldLength )
   {
      X_ITEM nilInit = X_ITEM_NIL_INIT;

      for( i = oldLength; i < ( int64_t )length; ++i )
         storage->items[i] = nilInit;
   }

   storage->len = ( int64_t )length;

   return 1;
}

/* AAdd() */
int x_arrayAdd( X_ITEM *array, const X_ITEM *value )
{
   X_ITEM copy __attribute__( ( cleanup( x_itemClear ) ) ) = X_ITEM_NIL_INIT;
   int64_t length;

   if( array->nType != X_IT_ARRAY )
      return 0;

   length = x_arrayLen( array );
   if( length == INT64_MAX )
      return 0;

   x_itemCopy( &copy, value );
   if( !x_arraySize( array, ( size_t )( length + 1 ) ) )
      return 0;

   x_itemCopy( x_arrayGetItemPtr( array, ( size_t )( length + 1 ) ), &copy );

   return 1;
}

/* AIns(): a NIL element at index; the last element falls out */
int x_arrayIns( X_ITEM *array, size_t index )
{
   X_ARR *storage;
   size_t i;

   if( array->nType != X_IT_ARRAY || index == 0 || index > ( size_t )x_arrayLen( array ) )
      return 0;

   storage = ( X_ARR * )array->u.pArr;
   if( storage->classHandle != 0 )
      return 0;

   unsigned int depth __attribute__( ( cleanup( x_gcRestore ) ) ) = x_gcPause();

   x_itemClear( &storage->items[storage->len - 1] );
   for( i = ( size_t )storage->len - 1; i >= index; --i )
      storage->items[i] = storage->items[i - 1];

   storage->items[index - 1] = ( X_ITEM )X_ITEM_NIL_INIT;

   return 1;
}

/* ADel(): the element at index falls out; the last element becomes NIL */
int x_arrayDel( X_ITEM *array, size_t index )
{
   X_ARR *storage;
   size_t i;

   if( array->nType != X_IT_ARRAY || index == 0 || index > ( size_t )x_arrayLen( array ) )
      return 0;

   storage = ( X_ARR * )array->u.pArr;
   if( storage->classHandle != 0 )
      return 0;

   unsigned int depth __attribute__( ( cleanup( x_gcRestore ) ) ) = x_gcPause();

   x_itemClear( &storage->items[index - 1] );
   for( i = index - 1; i < ( size_t )storage->len - 1; ++i )
      storage->items[i] = storage->items[i + 1];

   storage->items[storage->len - 1] = ( X_ITEM )X_ITEM_NIL_INIT;

   return 1;
}

/* ---------------------------------------------------------------------------------------------
   Array elements
   --------------------------------------------------------------------------------------------- */

/* The element at index (from 1); NULL outside the array */
X_ITEM *x_arrayGetItemPtr( const X_ITEM *array, size_t index )
{
   if( index == 0 || index > ( uint64_t )x_arrayLen( array ) )
      return NULL;

   return &( ( X_ARR * )array->u.pArr )->items[index - 1];
}

/* A copy of the element at index */
int x_arrayGet( const X_ITEM *array, size_t index, X_ITEM *result )
{
   X_ITEM *item = x_arrayGetItemPtr( array, index );

   if( !item )
      return 0;

   x_itemCopy( result, item );

   return 1;
}

/* A copy of the last element */
int x_arrayLast( const X_ITEM *array, X_ITEM *result )
{
   int64_t length = x_arrayLen( array );

   if( length == 0 )
      return 0;

   return x_arrayGet( array, ( size_t )length, result );
}

/* The type of the element at index */
X_TYPE x_arrayGetType( const X_ITEM *array, size_t index )
{
   X_ITEM *item = x_arrayGetItemPtr( array, index );

   return IIF( item != NULL, X_ITEM_TYPE( item ), X_IT_NIL );
}

/* The text of a string element; NULL for any other value */
const char *x_arrayGetCPtr( const X_ITEM *array, size_t index )
{
   X_ITEM *item = x_arrayGetItemPtr( array, index );

   return IIF( item != NULL && item->nType == X_IT_STRING, item->u.string.data, NULL );
}

/* A copy of the text of a string element, allocated by x_xgrab() */
char *x_arrayGetC( const X_ITEM *array, size_t index )
{
   const char *value = x_arrayGetCPtr( array, index );
   size_t length;
   char *copy;

   if( value == NULL )
      return NULL;

   length = x_arrayGetCLen( array, index );
   copy = ( char * )x_xgrab( length + 1 );
   memcpy( copy, value, length );
   copy[length] = '\0';

   return copy;
}

/* The length of a string element */
size_t x_arrayGetCLen( const X_ITEM *array, size_t index )
{
   X_ITEM *item = x_arrayGetItemPtr( array, index );

   return IIF( item != NULL && item->nType == X_IT_STRING, item->u.string.len, 0 );
}

const char *x_arrayGetStrUTF8( const X_ITEM *array, size_t index, void **handle, size_t *length )
{
   return x_itemGetStrUTF8( x_arrayGetItemPtr( array, index ), handle, length );
}

int x_arrayGetL( const X_ITEM *array, size_t index )
{
   X_ITEM *item = x_arrayGetItemPtr( array, index );

   return IIF( item != NULL && item->nType == X_IT_LOGICAL, item->u.logical, 0 );
}

int x_arrayGetNI( const X_ITEM *array, size_t index )
{
   return ( int )x_itemAsInt64( x_arrayGetItemPtr( array, index ) );
}

long x_arrayGetNL( const X_ITEM *array, size_t index )
{
   return ( long )x_itemAsInt64( x_arrayGetItemPtr( array, index ) );
}

long long x_arrayGetNLL( const X_ITEM *array, size_t index )
{
   return ( long long )x_itemAsInt64( x_arrayGetItemPtr( array, index ) );
}

ptrdiff_t x_arrayGetNS( const X_ITEM *array, size_t index )
{
   return ( ptrdiff_t )x_itemAsInt64( x_arrayGetItemPtr( array, index ) );
}

int64_t x_arrayGetNInt( const X_ITEM *array, size_t index )
{
   return x_itemAsInt64( x_arrayGetItemPtr( array, index ) );
}

double x_arrayGetND( const X_ITEM *array, size_t index )
{
   return x_itemAsDouble( x_arrayGetItemPtr( array, index ) );
}

void *x_arrayGetPtr( const X_ITEM *array, size_t index )
{
   X_ITEM *item = x_arrayGetItemPtr( array, index );

   return IIF( item != NULL && X_IS_POINTER( item ), item->u.pPointer, NULL );
}

/* Put a copy of value at index */
int x_arraySet( X_ITEM *array, size_t index, const X_ITEM *value )
{
   X_ITEM *item = x_arrayGetItemPtr( array, index );

   if( !item )
      return 0;

   x_itemCopy( item, value );

   return 1;
}

static int s_arraySetLogical( X_ITEM *array, size_t index, int value )
{
   X_ITEM *item = x_arrayGetItemPtr( array, index );

   if( item == NULL )
      return 0;

   x_itemReplace( item, ( X_ITEM ){ X_IT_LOGICAL, { .logical = IIF( value, 1, 0 ) } } );

   return 1;
}

static int s_arraySetInteger( X_ITEM *array, size_t index, int64_t value )
{
   X_ITEM *item = x_arrayGetItemPtr( array, index );

   if( item == NULL )
      return 0;

   x_itemReplace( item, ( X_ITEM ){ X_IT_INTEGER, { .integer = value } } );

   return 1;
}

static int s_arraySetDouble( X_ITEM *array, size_t index, double value )
{
   X_ITEM *item = x_arrayGetItemPtr( array, index );

   if( item == NULL )
      return 0;

   x_itemReplace( item, ( X_ITEM ){ X_IT_DOUBLE, { .dbl = value } } );

   return 1;
}

/* A copy of length bytes of value; NULL gives "" */
static int s_arraySetString( X_ITEM *array, size_t index, const char *value, size_t length )
{
   X_ITEM *item = x_arrayGetItemPtr( array, index );
   char *copy;

   if( item == NULL )
      return 0;

   if( value == NULL )
   {
      value = "";
      length = 0;
   }

   copy = ( char * )x_xgrab( length + 1 );
   memcpy( copy, value, length );
   copy[length] = '\0';
   x_itemReplace( item, ( X_ITEM ){ X_IT_STRING, { .string = { copy, length } } } );

   return 1;
}

static int s_arraySetPointer( X_ITEM *array, size_t index, void *ptr )
{
   X_ITEM *item = x_arrayGetItemPtr( array, index );

   if( item == NULL )
      return 0;

   x_itemReplace( item, ( X_ITEM ){ X_IT_POINTER, { .pPointer = ptr } } );

   return 1;
}

int x_arraySetL( X_ITEM *array, size_t index, int value )
{
   return s_arraySetLogical( array, index, value );
}

int x_arraySetNI( X_ITEM *array, size_t index, int value )
{
   return s_arraySetInteger( array, index, value );
}

int x_arraySetNL( X_ITEM *array, size_t index, long value )
{
   return s_arraySetInteger( array, index, value );
}

int x_arraySetNLL( X_ITEM *array, size_t index, long long value )
{
   return s_arraySetInteger( array, index, value );
}

int x_arraySetNS( X_ITEM *array, size_t index, ptrdiff_t value )
{
   return s_arraySetInteger( array, index, value );
}

int x_arraySetNInt( X_ITEM *array, size_t index, int64_t value )
{
   return s_arraySetInteger( array, index, value );
}

int x_arraySetND( X_ITEM *array, size_t index, double value )
{
   return s_arraySetDouble( array, index, value );
}

int x_arraySetC( X_ITEM *array, size_t index, const char *value )
{
   return s_arraySetString( array, index, value, IIF( value != NULL, strlen( value ), 0 ) );
}

int x_arraySetCL( X_ITEM *array, size_t index, const char *value, size_t length )
{
   return s_arraySetString( array, index, value, length );
}

int x_arraySetStrUTF8( X_ITEM *array, size_t index, const char *value )
{
   return x_arraySetC( array, index, value );
}

int x_arraySetPtr( X_ITEM *array, size_t index, void *ptr )
{
   return s_arraySetPointer( array, index, ptr );
}

/* ---------------------------------------------------------------------------------------------
   Index operator: array[index] and hash[key]
   --------------------------------------------------------------------------------------------- */

/* A hash: a new key is added by assignment when AutoAdd is on; reading a missing key gives the default
   value when it is set */
static X_ITEM *s_hashIndexSlot( const X_ITEM *hash, const X_ITEM *key, int assign )
{
   X_ITEM ignored __attribute__( ( cleanup( x_itemClear ) ) ) = X_ITEM_NIL_INIT;
   X_ITEM *slot = x_hashGetItemPtr( hash, key );
   int autoAdd = assign && ( x_hashFlags( hash ) & X_HASH_AUTOADD );

   if( slot == NULL && autoAdd && x_hashAdd( ( X_ITEM * )hash, key, x_hashDefault( hash ) ) )
      slot = x_hashGetItemPtr( hash, key );
   else if( slot == NULL && !assign && !X_IS_NIL( x_hashDefault( hash ) ) )
      slot = x_hashDefault( hash );

   if( slot == NULL )
      x_errorRaise( &ignored, IIF( assign, 1133, 1132 ), hash, key );

   return slot;
}

/* The element for array[index], to read or to assign; NULL after an error */
static X_ITEM *s_arrayIndexSlot( const X_ITEM *array, const X_ITEM *index, int assign )
{
   X_ITEM ignored __attribute__( ( cleanup( x_itemClear ) ) ) = X_ITEM_NIL_INIT;
   int64_t position = 0;

   if( array->nType == X_IT_HASH )
      return s_hashIndexSlot( array, index, assign );

   if( array->nType != X_IT_ARRAY || !X_IS_NUMERIC( index ) )
   {
      x_errorRaise( &ignored, IIF( assign, 1069, 1068 ), array, index );
      return NULL;
   }

   if( index->nType == X_IT_INTEGER )
      position = index->u.integer;
   else if( index->nType == X_IT_UINT64 && index->u.unsignedInteger <= INT64_MAX )
      position = ( int64_t )index->u.unsignedInteger;
   else if( index->nType == X_IT_DOUBLE && isfinite( index->u.dbl ) && index->u.dbl >= 1 && index->u.dbl < 0x1p63 )
      position = ( int64_t )index->u.dbl;

   if( position < 1 || position > x_arrayLen( array ) )
   {
      x_errorRaise( &ignored, IIF( assign, 1133, 1132 ), array, index );
      return NULL;
   }

   return x_arrayGetItemPtr( array, ( size_t )position );
}

/* result := array[index] */
int x_itemArrayGet( X_CALL_CTX *ctx, X_ITEM *result, const X_ITEM *array, const X_ITEM *index )
{
   X_ITEM *slot;

   if( x_objHasOperator( array, X_OO_OP_ARRAYINDEX ) )
      return x_objOperatorCall( ctx, X_OO_OP_ARRAYINDEX, result, ( X_ITEM * )array, index, NULL );

   slot = s_arrayIndexSlot( array, index, 0 );
   if( !slot )
   {
      x_itemClear( result );
      return 0;
   }

   x_itemCopy( result, slot );

   return 1;
}

/* array[index] := value; the slot is looked up again after the assignment, which can change the array */
int x_itemArraySet( X_CALL_CTX *ctx, X_ITEM *result, const X_ITEM *array, const X_ITEM *index,
                    const X_ITEM *value )
{
   X_ITEM receiver __attribute__( ( cleanup( x_itemClear ) ) ) = X_ITEM_NIL_INIT;
   X_ITEM position __attribute__( ( cleanup( x_itemClear ) ) ) = X_ITEM_NIL_INIT;
   X_ITEM assigned __attribute__( ( cleanup( x_itemClear ) ) ) = X_ITEM_NIL_INIT;
   X_ITEM *slot;
   int success;

   if( x_objHasOperator( array, X_OO_OP_ARRAYINDEX ) )
      return x_objOperatorCall( ctx, X_OO_OP_ARRAYINDEX, result, ( X_ITEM * )array, index, value );

   slot = s_arrayIndexSlot( array, index, 1 );
   if( !slot )
   {
      x_itemClear( result );
      return 0;
   }

   x_itemCopy( &receiver, array );
   x_itemCopy( &position, index );
   x_itemCopy( &assigned, slot );
   success = x_itemAssign( ctx, &assigned, value );

   slot = NULL;
   if( success )
      slot = s_arrayIndexSlot( &receiver, &position, 1 );

   if( slot != NULL )
   {
      x_itemCopy( slot, &assigned );
      x_itemCopy( result, slot );
   }
   else
      x_itemClear( result );

   success = slot != NULL;
   x_itemClear( &assigned );
   x_itemClear( &position );
   x_itemClear( &receiver );

   return success;
}

/* ---------------------------------------------------------------------------------------------
   Hashes
   --------------------------------------------------------------------------------------------- */

/* The storage of a hash; NULL for any other value */
static X_HASH *s_hashOf( const X_ITEM *hash )
{
   return IIF( hash != NULL && hash->nType == X_IT_HASH, ( X_HASH * )hash->u.pPointer, NULL );
}

/* Keys are texts, numbers, dates and logical values */
int x_hashIsKey( const X_ITEM *key )
{
   return key != NULL && ( X_IS_STRING( key ) || X_IS_NUMERIC( key ) || X_IS_DATE( key ) || X_IS_LOGICAL( key ) );
}

/* FNV-1a over the bytes of data */
static uint64_t s_hashMix( uint64_t code, const void *data, size_t length )
{
   const unsigned char *bytes = ( const unsigned char * )data;
   size_t i;

   for( i = 0; i < length; ++i )
      code = ( code ^ bytes[i] ) * 1099511628211ULL;

   return code;
}

/* The next character of a key without case; a broken UTF-8 byte stays different from every character */
static uint32_t s_hashFold( const char *text, size_t length, size_t *offset )
{
   size_t first = *offset;
   uint32_t codepoint;

   if( x_utf8Decode( text, length, offset, &codepoint ) )
      return x_utf8LowerChar( codepoint );

   return 0x110000 + ( unsigned char )text[first];
}

/* The hash code of a key; a whole number has the same code as an integer, so 1 and 1.0 are the same key */
static uint64_t s_hashCode( const X_HASH *hash, const X_ITEM *key )
{
   uint64_t code;

   if( s_hashSeed == 0 )
      s_hashSeed = x_randomEntropy() | 1;

   code = 14695981039346656037ULL ^ s_hashSeed;

   if( X_IS_STRING( key ) )
   {
      const char *text = key->u.string.data;
      size_t offset = 0;

      if( hash->flags & X_HASH_CASEMATCH )
         return s_hashMix( code, text, key->u.string.len );

      while( offset < key->u.string.len )
         code = ( code ^ s_hashFold( text, key->u.string.len, &offset ) ) * 1099511628211ULL;

      return code;
   }

   if( X_IS_NUMERIC( key ) )
   {
      int64_t whole;
      double value;

      if( key->nType == X_IT_INTEGER || key->nType == X_IT_UINT64 )
         whole = key->u.integer;
      else
      {
         value = key->u.dbl;
         if( !( value == floor( value ) && fabs( value ) < 0x1p63 ) )
            return s_hashMix( code ^ 1, &value, sizeof( value ) );

         whole = ( int64_t )value;
      }

      return s_hashMix( code ^ 1, &whole, sizeof( whole ) );
   }

   if( X_IS_DATE( key ) )
      return s_hashMix( code ^ 2, &key->u.integer, sizeof( key->u.integer ) );

   return code ^ ( uint64_t )IIF( key->u.logical, 4, 3 );
}

/* Are two keys the same key? */
static int s_hashSame( const X_HASH *hash, const X_ITEM *first, const X_ITEM *second )
{
   if( X_IS_STRING( first ) && X_IS_STRING( second ) )
   {
      const char *one = first->u.string.data;
      const char *two = second->u.string.data;
      size_t oneLength = first->u.string.len;
      size_t twoLength = second->u.string.len;
      size_t oneOffset = 0;
      size_t twoOffset = 0;

      if( hash->flags & X_HASH_CASEMATCH )
         return oneLength == twoLength && memcmp( one, two, oneLength ) == 0;

      while( oneOffset < oneLength && twoOffset < twoLength )
      {
         if( s_hashFold( one, oneLength, &oneOffset ) != s_hashFold( two, twoLength, &twoOffset ) )
            return 0;
      }

      return oneOffset == oneLength && twoOffset == twoLength;
   }

   if( X_IS_NUMERIC( first ) && X_IS_NUMERIC( second ) )
   {
      if( !X_IS_DOUBLE( first ) && !X_IS_DOUBLE( second ) )
         return first->u.integer == second->u.integer;

      return x_itemAsDouble( first ) == x_itemAsDouble( second );
   }

   if( X_IS_DATE( first ) && X_IS_DATE( second ) )
      return first->u.integer == second->u.integer;

   if( X_IS_LOGICAL( first ) && X_IS_LOGICAL( second ) )
      return ( first->u.logical != 0 ) == ( second->u.logical != 0 );

   return 0;
}

/* The table of slots is made again, at least a third of it free */
static void s_hashIndex( X_HASH *hash, size_t want )
{
   size_t count = 8;
   size_t i;

   while( count < want + want / 2 + 1 )
      count *= 2;

   x_xfree( hash->slots );
   hash->slots = ( size_t * )x_xgrabzArray( count, sizeof( size_t ) );
   hash->slotCount = count;

   for( i = 0; i < hash->len; ++i )
   {
      size_t slot = ( size_t )s_hashCode( hash, &hash->keys[i] ) & ( count - 1 );

      while( hash->slots[slot] != 0 )
         slot = ( slot + 1 ) & ( count - 1 );

      hash->slots[slot] = i + 1;
   }
}

/* The position (from 0) of a key; X_HASH_NONE when it is not there */
static size_t s_hashFind( const X_HASH *hash, const X_ITEM *key )
{
   size_t slot;

   if( hash->slotCount == 0 || !x_hashIsKey( key ) )
      return X_HASH_NONE;

   slot = ( size_t )s_hashCode( hash, key ) & ( hash->slotCount - 1 );
   while( hash->slots[slot] != 0 )
   {
      if( s_hashSame( hash, &hash->keys[hash->slots[slot] - 1], key ) )
         return hash->slots[slot] - 1;

      slot = ( slot + 1 ) & ( hash->slotCount - 1 );
   }

   return X_HASH_NONE;
}

/* A new empty hash in dest, case sensitive with AutoAdd on */
int x_hashNew( X_ITEM *dest )
{
   X_HASH *hash = ( X_HASH * )x_xgrabz( sizeof( X_HASH ) );

   hash->defaultValue = ( X_ITEM )X_ITEM_NIL_INIT;
   hash->nRefs = 1;
   hash->flags = X_HASH_CASEMATCH | X_HASH_AUTOADD;

   x_gcRegister( hash, &hash->nRefs, X_GC_HASH );
   x_itemReplace( dest, ( X_ITEM ){ X_IT_HASH, { .pPointer = hash } } );

   return 1;
}

/* The number of pairs; 0 for any other value */
size_t x_hashLen( const X_ITEM *hashItem )
{
   const X_HASH *hash = s_hashOf( hashItem );

   return IIF( hash != NULL, hash->len, 0 );
}

/* The identity of a hash; NULL for any other value */
void *x_hashId( const X_ITEM *hashItem )
{
   return s_hashOf( hashItem );
}

/* The value of a key; NULL when the key is not there */
X_ITEM *x_hashGetItemPtr( const X_ITEM *hashItem, const X_ITEM *key )
{
   const X_HASH *hash = s_hashOf( hashItem );
   size_t pos = IIF( hash != NULL, s_hashFind( hash, key ), X_HASH_NONE );

   return IIF( pos != X_HASH_NONE, &hash->values[pos], NULL );
}

/* The position from 1 in the order of adding, 0 when the key is not there */
size_t x_hashPos( const X_ITEM *hashItem, const X_ITEM *key )
{
   const X_HASH *hash = s_hashOf( hashItem );
   size_t pos = IIF( hash != NULL, s_hashFind( hash, key ), X_HASH_NONE );

   return IIF( pos != X_HASH_NONE, pos + 1, 0 );
}

/* Add a pair or change the value of a key; a NULL value gives NIL */
int x_hashAdd( X_ITEM *hashItem, const X_ITEM *keyItem, const X_ITEM *valueItem )
{
   X_HASH *hash = s_hashOf( hashItem );
   X_ITEM key __attribute__( ( cleanup( x_itemClear ) ) ) = X_ITEM_NIL_INIT;
   X_ITEM value __attribute__( ( cleanup( x_itemClear ) ) ) = X_ITEM_NIL_INIT;
   size_t pos;

   if( hash == NULL || !x_hashIsKey( keyItem ) )
      return 0;

   /* The key and the value can lie in this hash, which moves when it grows */
   x_itemCopy( &key, keyItem );
   if( valueItem != NULL )
      x_itemCopy( &value, valueItem );

   pos = s_hashFind( hash, &key );
   if( pos != X_HASH_NONE )
   {
      x_itemCopy( &hash->values[pos], &value );
      return 1;
   }

   if( hash->len == hash->capacity )
   {
      size_t capacity = IIF( hash->capacity == 0, 8, hash->capacity * 2 );

      hash->keys = ( X_ITEM * )x_xreallocArray( hash->keys, capacity, sizeof( X_ITEM ) );
      hash->values = ( X_ITEM * )x_xreallocArray( hash->values, capacity, sizeof( X_ITEM ) );
      hash->capacity = capacity;
   }

   hash->keys[hash->len] = ( X_ITEM )X_ITEM_NIL_INIT;
   hash->values[hash->len] = ( X_ITEM )X_ITEM_NIL_INIT;
   x_itemCopy( &hash->keys[hash->len], &key );
   x_itemCopy( &hash->values[hash->len], &value );
   ++hash->len;

   if( hash->len * 3 >= hash->slotCount * 2 )
      s_hashIndex( hash, hash->len );
   else
   {
      size_t slot = ( size_t )s_hashCode( hash, &key ) & ( hash->slotCount - 1 );

      while( hash->slots[slot] != 0 )
         slot = ( slot + 1 ) & ( hash->slotCount - 1 );

      hash->slots[slot] = hash->len;
   }

   return 1;
}

/* Remove the pair at position pos (from 1) */
int x_hashDelAt( X_ITEM *hashItem, size_t pos )
{
   X_HASH *hash = s_hashOf( hashItem );
   X_ITEM key __attribute__( ( cleanup( x_itemClear ) ) ) = X_ITEM_NIL_INIT;
   X_ITEM value __attribute__( ( cleanup( x_itemClear ) ) ) = X_ITEM_NIL_INIT;
   size_t moved;

   if( hash == NULL || pos < 1 || pos > hash->len )
      return 0;

   /* The pair leaves the hash before it is released, as its value can hold this hash */
   key = hash->keys[pos - 1];
   value = hash->values[pos - 1];
   moved = ( hash->len - pos ) * sizeof( X_ITEM );
   memmove( &hash->keys[pos - 1], &hash->keys[pos], moved );
   memmove( &hash->values[pos - 1], &hash->values[pos], moved );
   --hash->len;
   s_hashIndex( hash, hash->len );

   return 1;
}

/* Remove the pair of a key */
int x_hashDel( X_ITEM *hashItem, const X_ITEM *key )
{
   return x_hashDelAt( hashItem, x_hashPos( hashItem, key ) );
}

/* Remove all pairs */
void x_hashClear( X_ITEM *hashItem )
{
   while( x_hashLen( hashItem ) > 0 )
      x_hashDelAt( hashItem, x_hashLen( hashItem ) );
}

/* The key at position pos (from 1); NULL outside the hash */
X_ITEM *x_hashKeyAt( const X_ITEM *hashItem, size_t pos )
{
   const X_HASH *hash = s_hashOf( hashItem );

   return IIF( hash != NULL && pos >= 1 && pos <= hash->len, &hash->keys[pos - 1], NULL );
}

/* The value at position pos (from 1); NULL outside the hash */
X_ITEM *x_hashValueAt( const X_ITEM *hashItem, size_t pos )
{
   const X_HASH *hash = s_hashOf( hashItem );

   return IIF( hash != NULL && pos >= 1 && pos <= hash->len, &hash->values[pos - 1], NULL );
}

/* The flags X_HASH_CASEMATCH and X_HASH_AUTOADD */
uint32_t x_hashFlags( const X_ITEM *hashItem )
{
   const X_HASH *hash = s_hashOf( hashItem );

   return IIF( hash != NULL, hash->flags, 0 );
}

/* Set or clear flags; changing the case match makes the index again */
void x_hashSetFlags( X_ITEM *hashItem, uint32_t flags, int on )
{
   X_HASH *hash = s_hashOf( hashItem );

   if( hash == NULL )
      return;

   hash->flags = IIF( on, hash->flags | flags, hash->flags & ~flags );
   if( flags & X_HASH_CASEMATCH )
      s_hashIndex( hash, hash->len );
}

/* The default value of missing keys */
X_ITEM *x_hashDefault( const X_ITEM *hashItem )
{
   X_HASH *hash = s_hashOf( hashItem );

   return IIF( hash != NULL, &hash->defaultValue, NULL );
}

/* Set the default value of missing keys */
void x_hashSetDefault( X_ITEM *hashItem, const X_ITEM *value )
{
   X_HASH *hash = s_hashOf( hashItem );

   if( hash != NULL )
      x_itemCopy( &hash->defaultValue, value );
}

/* Nested hashes become new hashes too and a hash met again gives the clone made before, also in cycles;
   arrays are cloned as by AClone() */
static void s_hashCloneTo( X_ITEM *result, const X_ITEM *source, X_ITEM *sources, X_ITEM *copies )
{
   X_ITEM clone __attribute__( ( cleanup( x_itemClear ) ) ) = X_ITEM_NIL_INIT;
   size_t i;

   for( i = 1; i <= ( size_t )x_arrayLen( sources ); ++i )
   {
      if( x_hashId( x_arrayGetItemPtr( sources, i ) ) == x_hashId( source ) )
      {
         x_itemCopy( result, x_arrayGetItemPtr( copies, i ) );
         return;
      }
   }

   x_hashNew( &clone );
   s_hashOf( &clone )->flags = s_hashOf( source )->flags;
   x_itemCopy( &s_hashOf( &clone )->defaultValue, &s_hashOf( source )->defaultValue );
   x_arrayAdd( sources, source );
   x_arrayAdd( copies, &clone );

   for( i = 1; i <= x_hashLen( source ); ++i )
   {
      X_ITEM value __attribute__( ( cleanup( x_itemClear ) ) ) = X_ITEM_NIL_INIT;
      const X_ITEM *item = x_hashValueAt( source, i );

      if( X_IS_HASH( item ) )
         s_hashCloneTo( &value, item, sources, copies );
      else if( X_IS_ARRAY( item ) )
         x_arrayClone( &value, item );
      else
         x_itemCopy( &value, item );

      x_hashAdd( &clone, x_hashKeyAt( source, i ), &value );
   }

   x_itemCopy( result, &clone );
}

/* HClone() */
void x_hashClone( X_ITEM *result, const X_ITEM *source )
{
   X_ITEM sources __attribute__( ( cleanup( x_itemClear ) ) ) = X_ITEM_NIL_INIT;
   X_ITEM copies __attribute__( ( cleanup( x_itemClear ) ) ) = X_ITEM_NIL_INIT;

   x_arrayNew( &sources, 0 );
   x_arrayNew( &copies, 0 );
   s_hashCloneTo( result, source, &sources, &copies );
}

/* The last position of count pairs from start; a negative count goes to the end */
static size_t s_hashEnd( const X_ITEM *hash, size_t start, int64_t count )
{
   size_t length = x_hashLen( hash );

   if( start > length || count == 0 )
      return start - 1;

   if( count < 0 || ( uint64_t )count > length - start + 1 )
      return length;

   return start + ( size_t )count - 1;
}

/* HEval(): the block gets the key, the value and the position */
void x_hashEval( X_CALL_CTX *ctx, const X_ITEM *source, const X_ITEM *code, size_t start, int64_t count )
{
   X_ITEM hash __attribute__( ( cleanup( x_itemClear ) ) ) = X_ITEM_NIL_INIT;
   X_ITEM block __attribute__( ( cleanup( x_itemClear ) ) ) = X_ITEM_NIL_INIT;
   size_t end, i;

   x_itemCopy( &hash, source );
   x_itemCopy( &block, code );
   start = IIF( start < 1, 1, start );
   end = s_hashEnd( &hash, start, count );

   for( i = start; i <= end && i <= x_hashLen( &hash ); ++i )
   {
      X_ITEM key __attribute__( ( cleanup( x_itemClear ) ) ) = X_ITEM_NIL_INIT;
      X_ITEM value __attribute__( ( cleanup( x_itemClear ) ) ) = X_ITEM_NIL_INIT;
      X_ITEM result __attribute__( ( cleanup( x_itemClear ) ) ) = X_ITEM_NIL_INIT;
      X_ITEM index = { X_IT_INTEGER, { .integer = ( int64_t )i } };
      X_ARG args[3] = { { &key, 0 }, { &value, 0 }, { &index, 0 } };

      x_itemCopy( &key, x_hashKeyAt( &hash, i ) );
      x_itemCopy( &value, x_hashValueAt( &hash, i ) );
      x_evalBlock( ctx, &result, &block, 3, args );
   }
}

/* HScan(): the value is compared as in AScan(); a block gets the key, the value and the position */
size_t x_hashScan( X_CALL_CTX *ctx, const X_ITEM *source, const X_ITEM *search, size_t start, int64_t count,
                   int exact )
{
   X_ITEM hash __attribute__( ( cleanup( x_itemClear ) ) ) = X_ITEM_NIL_INIT;
   X_ITEM values __attribute__( ( cleanup( x_itemClear ) ) ) = X_ITEM_NIL_INIT;
   size_t end, i;

   x_itemCopy( &hash, source );
   start = IIF( start < 1, 1, start );
   end = s_hashEnd( &hash, start, count );

   if( !X_IS_BLOCK( search ) )
   {
      int64_t found;

      /* The values in a row, searched the same way as an array */
      x_arrayNew( &values, 0 );
      for( i = start; i <= end; ++i )
         x_arrayAdd( &values, x_hashValueAt( &hash, i ) );

      found = x_arrayScan( ctx, &values, search, 1, x_arrayLen( &values ), exact );

      return IIF( found > 0, start + ( size_t )found - 1, 0 );
   }

   for( i = start; i <= end && i <= x_hashLen( &hash ); ++i )
   {
      X_ITEM key __attribute__( ( cleanup( x_itemClear ) ) ) = X_ITEM_NIL_INIT;
      X_ITEM value __attribute__( ( cleanup( x_itemClear ) ) ) = X_ITEM_NIL_INIT;
      X_ITEM result __attribute__( ( cleanup( x_itemClear ) ) ) = X_ITEM_NIL_INIT;
      X_ITEM index = { X_IT_INTEGER, { .integer = ( int64_t )i } };
      X_ARG args[3] = { { &key, 0 }, { &value, 0 }, { &index, 0 } };

      x_itemCopy( &key, x_hashKeyAt( &hash, i ) );
      x_itemCopy( &value, x_hashValueAt( &hash, i ) );
      x_evalBlock( ctx, &result, search, 3, args );
      if( X_IS_LOGICAL( &result ) && result.u.logical )
         return i;
   }

   return 0;
}

/* HMerge(): the pairs start..start+count-1 of the source go into the target by the mode X_HASH_UNION (added
   or changed), X_HASH_INTERSECT (only the keys of both stay, with the values of the source), X_HASH_DIFFERENCE
   (the keys of one of them only) or X_HASH_REMOVE (the keys of the source leave the target); with a block
   a pair is added when the block, given the key, the value and the position, returns .T. */
void x_hashMerge( X_CALL_CTX *ctx, X_ITEM *target, const X_ITEM *source, size_t start, int64_t count, int mode,
                  const X_ITEM *code )
{
   X_ITEM part __attribute__( ( cleanup( x_itemClear ) ) ) = X_ITEM_NIL_INIT;
   int hasBlock = code != NULL && X_IS_BLOCK( code );
   size_t end, i;

   /* The chosen pairs are taken first, the target can be the source itself */
   start = IIF( start < 1, 1, start );
   end = s_hashEnd( source, start, count );
   x_hashNew( &part );
   s_hashOf( &part )->flags = s_hashOf( source )->flags;

   for( i = start; i <= end; ++i )
   {
      if( hasBlock )
      {
         X_ITEM key __attribute__( ( cleanup( x_itemClear ) ) ) = X_ITEM_NIL_INIT;
         X_ITEM value __attribute__( ( cleanup( x_itemClear ) ) ) = X_ITEM_NIL_INIT;
         X_ITEM result __attribute__( ( cleanup( x_itemClear ) ) ) = X_ITEM_NIL_INIT;
         X_ITEM index = { X_IT_INTEGER, { .integer = ( int64_t )i } };
         X_ARG args[3] = { { &key, 0 }, { &value, 0 }, { &index, 0 } };

         x_itemCopy( &key, x_hashKeyAt( source, i ) );
         x_itemCopy( &value, x_hashValueAt( source, i ) );
         x_evalBlock( ctx, &result, code, 3, args );
         if( !( X_IS_LOGICAL( &result ) && result.u.logical ) )
            continue;
      }

      x_hashAdd( &part, x_hashKeyAt( source, i ), x_hashValueAt( source, i ) );
   }

   if( hasBlock )
      mode = X_HASH_UNION;

   switch( mode )
   {
   case X_HASH_INTERSECT:
      for( i = x_hashLen( target ); i >= 1; --i )
      {
         X_ITEM *value = x_hashGetItemPtr( &part, x_hashKeyAt( target, i ) );

         if( value == NULL )
            x_hashDelAt( target, i );
         else
            x_itemCopy( x_hashValueAt( target, i ), value );
      }
      break;

   case X_HASH_DIFFERENCE:
      for( i = 1; i <= x_hashLen( &part ); ++i )
      {
         if( x_hashPos( target, x_hashKeyAt( &part, i ) ) != 0 )
            x_hashDel( target, x_hashKeyAt( &part, i ) );
         else
            x_hashAdd( target, x_hashKeyAt( &part, i ), x_hashValueAt( &part, i ) );
      }
      break;

   case X_HASH_REMOVE:
      for( i = 1; i <= x_hashLen( &part ); ++i )
         x_hashDel( target, x_hashKeyAt( &part, i ) );
      break;

   default:
      for( i = 1; i <= x_hashLen( &part ); ++i )
         x_hashAdd( target, x_hashKeyAt( &part, i ), x_hashValueAt( &part, i ) );
      break;
   }
}

/* ---------------------------------------------------------------------------------------------
   FOR EACH
   --------------------------------------------------------------------------------------------- */

/* The number of turns over a base: elements of an array, pairs of a hash, bytes of a text; 0 for others */
int x_enumLength( const X_ITEM *base, int64_t *length )
{
   if( base != NULL && X_IS_HASH( base ) )
      *length = ( int64_t )x_hashLen( base );
   else if( base != NULL && base->nType == X_IT_ARRAY )
      *length = x_arrayLen( base );
   else if( base != NULL && X_IS_STRING( base ) )
      *length = ( int64_t )base->u.string.len;
   else
      return 0;

   return 1;
}

/* The element at a position into dest: the element of an array, the value of a hash, the byte of a text;
   NIL outside the base */
void x_enumGet( const X_ITEM *base, int64_t pos, X_ITEM *dest )
{
   int64_t length;

   if( !x_enumLength( base, &length ) || pos < 1 || pos > length )
      x_itemClear( dest );
   else if( X_IS_HASH( base ) )
      x_itemCopy( dest, x_hashValueAt( base, ( size_t )pos ) );
   else if( base->nType == X_IT_ARRAY )
      x_itemCopy( dest, x_arrayGetItemPtr( base, ( size_t )pos ) );
   else
      x_itemPutCL( dest, base->u.string.data + pos - 1, 1 );
}

/* The value of the loop variable goes back into its element of the base, the 1st parameter passed with @;
   a text changes only its own copy, which goes back to the variable after IN @cText */
void x_enumPut( X_CALL_CTX *ctx, int64_t pos, const X_ITEM *value )
{
   const X_ITEM *base = x_paritem( ctx, 1 );
   int64_t length;
   char *text;

   if( !x_enumLength( base, &length ) || pos < 1 || pos > length )
      return;

   if( X_IS_HASH( base ) )
   {
      x_itemCopy( x_hashValueAt( base, ( size_t )pos ), value );
      return;
   }

   if( base->nType == X_IT_ARRAY )
   {
      x_arraySet( ( X_ITEM * )base, ( size_t )pos, value );
      return;
   }

   if( value == NULL || !X_IS_STRING( value ) || value->u.string.len == 0 )
      return;

   if( value->u.string.data[0] == base->u.string.data[pos - 1] )
      return;

   text = ( char * )x_xgrab( ( size_t )length + 1 );
   memcpy( text, base->u.string.data, ( size_t )length );
   text[length] = '\0';
   text[pos - 1] = value->u.string.data[0];
   x_storclen_buffer( ctx, text, ( size_t )length, 1 );
}

/* ---------------------------------------------------------------------------------------------
   RAScan()
   --------------------------------------------------------------------------------------------- */

/* RAScan( aArray, xbSearch, [nStart], [nCount], [lExact], [lASCII] ): AScan() from the end; with lASCII
   a one byte string and a number match by the code of the byte */
void x_arrayReverseScanCall( X_CALL_CTX *ctx )
{
   X_ITEM array __attribute__( ( cleanup( x_itemClear ) ) ) = X_ITEM_NIL_INIT;
   X_ITEM needle __attribute__( ( cleanup( x_itemClear ) ) ) = X_ITEM_NIL_INIT;
   int64_t length, start, count, position, scanned;
   int exact = x_parl( ctx, 5 );
   int ascii = x_parl( ctx, 6 );
   int match;
   const X_ITEM *item;

   x_itemCopy( &array, x_paritem( ctx, 1 ) );
   x_itemCopy( &needle, x_paritem( ctx, 2 ) );
   length = x_arrayLen( &array );
   start = x_parnintdef( ctx, 3, length );
   count = x_parnintdef( ctx, 4, start );
   if( start < 1 || start > length || count < 1 )
   {
      x_retni( ctx, 0 );
      return;
   }

   count = IIF( count > start, start, count );
   for( scanned = 0, position = start; scanned < count && position > 0; ++scanned, --position )
   {
      item = x_arrayGetItemPtr( &array, ( size_t )position );
      if( item == NULL )
         continue;

      if( ascii && X_IS_STRING( item ) && item->u.string.len == 1 && X_IS_NUMERIC( &needle ) )
         match = ( unsigned char )item->u.string.data[0] == x_itemAsDouble( &needle );
      else if( ascii && X_IS_STRING( &needle ) && needle.u.string.len == 1 && X_IS_NUMERIC( item ) )
         match = ( unsigned char )needle.u.string.data[0] == x_itemAsDouble( item );
      else
         match = x_arrayScan( ctx, &array, &needle, position, 1, exact ) != 0;

      if( match )
      {
         x_retnint( ctx, position );
         return;
      }
   }

   x_retni( ctx, 0 );
}

/* ---------------------------------------------------------------------------------------------
   Array and hash functions of the API
   --------------------------------------------------------------------------------------------- */

/* ACopy(): elements of one array copied into another; the count is cut to the elements and the room there are */
void x_arrayACopy( X_CALL_CTX *ctx )
{
   const X_ITEM *source = x_paritem( ctx, 1 );
   X_ITEM *target = ( X_ITEM * )x_paritem( ctx, 2 );
   int64_t start = x_parnintdef( ctx, 3, 1 );
   int64_t position = x_parnintdef( ctx, 5, 1 );
   int64_t i;

   if( start < 1 )
      start = 1;

   if( position < 1 )
      position = 1;

   int64_t available = x_arrayLen( source ) - start + 1;
   int64_t room = x_arrayLen( target ) - position + 1;
   int64_t count = x_parnintdef( ctx, 4, available );

   if( count > available )
      count = available;

   if( count > room )
      count = room;

   for( i = 0; i < count; ++i )
      x_arraySet( target, ( size_t )( position + i ), x_arrayGetItemPtr( source, ( size_t )( start + i ) ) );

   x_itemReturn( ctx, target );
}

/* ADel(): an element deleted, the rest moved up and NIL put at the end; with lAutoSize the array gets shorter */
void x_arrayADel( X_CALL_CTX *ctx )
{
   const X_ITEM *array = x_paritem( ctx, 1 );
   int64_t pos = x_parnint( ctx, 2 );

   if( x_arrayDel( ( X_ITEM * )array, ( size_t )pos ) && x_parl( ctx, 3 ) )
      x_arraySize( ( X_ITEM * )array, ( size_t )x_arrayLen( array ) - 1 );

   x_itemReturn( ctx, array );
}

/* AEval(): the block evaluated for each element from nStart, nCount times */
void x_arrayAEval( X_CALL_CTX *ctx )
{
   X_ITEM array __attribute__( ( cleanup( x_itemClear ) ) ) = X_ITEM_NIL_INIT;
   int64_t count;

   x_itemCopy( &array, x_paritem( ctx, 1 ) );
   count = x_parnintdef( ctx, 4, x_arrayLen( &array ) );
   x_arrayEval( ctx, &array, x_paritem( ctx, 2 ), x_parnintdef( ctx, 3, 1 ), count );
   x_itemReturn( ctx, &array );
}

/* AFill(): elements from nStart set to one value */
void x_arrayAFill( X_CALL_CTX *ctx )
{
   X_ITEM *array = ( X_ITEM * )x_paritem( ctx, 1 );
   int64_t start = IIF( x_pcount( ctx ) >= 3, x_parnint( ctx, 3 ), 1 );
   int64_t i;

   if( start < 1 )
      start = 1;

   int64_t available = x_arrayLen( array ) - start + 1;
   int64_t count = IIF( x_pcount( ctx ) >= 4, x_parnint( ctx, 4 ), available );

   if( count > available )
      count = available;

   for( i = 0; i < count; ++i )
      x_arraySet( array, ( size_t )( start + i ), x_paritem( ctx, 2 ) );

   x_itemReturn( ctx, array );
}

/* AIns(): an element inserted, the last one dropped; with lAutoSize the array gets longer */
void x_arrayAIns( X_CALL_CTX *ctx )
{
   const X_ITEM *array = x_paritem( ctx, 1 );
   int64_t pos = x_parnint( ctx, 2 );

   if( x_parl( ctx, 4 ) && pos >= 1 && pos <= x_arrayLen( array ) + 1 )
      x_arraySize( ( X_ITEM * )array, ( size_t )x_arrayLen( array ) + 1 );

   if( x_arrayIns( ( X_ITEM * )array, ( size_t )pos ) && !X_ISNIL( ctx, 3 ) )
      x_arraySet( ( X_ITEM * )array, ( size_t )pos, x_paritem( ctx, 3 ) );

   x_itemReturn( ctx, array );
}

/* Array(): a new array with one dimension per argument; a wrong length is an error */
void x_arrayArray( X_CALL_CTX *ctx )
{
   int32_t i;

   for( i = 0; i < x_pcount( ctx ); ++i )
   {
      int64_t length;

      if( !x_arrayDimensionLength( ctx->args[i].value, &length ) )
      {
         x_errorRaise( ctx->result, 1131, ctx->args[i].value, NULL );
         return;
      }
   }

   x_arrayDimensions( ctx->result, ctx->argc, ctx->args );
}

/* ASort(): elements sorted in ascending order or by a block */
void x_arrayASort( X_CALL_CTX *ctx )
{
   X_ITEM array __attribute__( ( cleanup( x_itemClear ) ) ) = X_ITEM_NIL_INIT;
   X_ITEM block __attribute__( ( cleanup( x_itemClear ) ) ) = X_ITEM_NIL_INIT;

   x_itemCopy( &array, x_paritem( ctx, 1 ) );
   if( X_ISBLOCK( ctx, 4 ) )
      x_itemCopy( &block, x_paritem( ctx, 4 ) );

   x_arraySort( ctx, &array, &block, x_parnintdef( ctx, 2, 1 ), x_parnintdef( ctx, 3, x_arrayLen( &array ) ) );
   x_itemReturn( ctx, &array );
}

/* Hash(): a new hash from pairs of keys and values */
void x_hashHash( X_CALL_CTX *ctx )
{
   X_ITEM hash __attribute__( ( cleanup( x_itemClear ) ) ) = X_ITEM_NIL_INIT;
   int count = x_pcount( ctx );
   int i;

   x_hashNew( &hash );
   for( i = 1; i < count; i += 2 )
      x_hashAdd( &hash, x_paritem( ctx, i ), x_paritem( ctx, i + 1 ) );

   x_itemReturn( ctx, &hash );
}

/* HClone(): a full copy of a hash */
void x_hashHClone( X_CALL_CTX *ctx )
{
   X_ITEM clone __attribute__( ( cleanup( x_itemClear ) ) ) = X_ITEM_NIL_INIT;

   x_hashClone( &clone, x_paritem( ctx, 1 ) );
   x_itemReturn( ctx, &clone );
}

/* HEval(): the block evaluated with the key, the value and the position of each pair */
void x_hashHEval( X_CALL_CTX *ctx )
{
   X_ITEM hash __attribute__( ( cleanup( x_itemClear ) ) ) = X_ITEM_NIL_INIT;
   size_t start = ( size_t )IIF( X_ISNUM( ctx, 3 ), x_parnint( ctx, 3 ), 1 );
   int64_t count = IIF( X_ISNUM( ctx, 4 ), x_parnint( ctx, 4 ), -1 );

   x_itemCopy( &hash, x_paritem( ctx, 1 ) );
   x_hashEval( ctx, &hash, x_paritem( ctx, 2 ), start, count );
   x_itemReturn( ctx, &hash );
}

/* HGet(): the value of a key, or the default value of the hash; an unknown key is an error */
void x_hashHGet( X_CALL_CTX *ctx )
{
   X_ITEM *value = x_hashGetItemPtr( x_paritem( ctx, 1 ), x_paritem( ctx, 2 ) );

   if( value == NULL && !X_IS_NIL( x_hashDefault( x_paritem( ctx, 1 ) ) ) )
      value = x_hashDefault( x_paritem( ctx, 1 ) );

   if( value != NULL )
      x_itemReturn( ctx, value );
   else
      ( void )x_errorRaise( ctx->result, 1132, x_paritem( ctx, 1 ), x_paritem( ctx, 2 ) );
}

/* HGetKeyAt() and HaaGetKeyAt(): the key at a position; a wrong position is an error */
void x_hashHGetKeyAt( X_CALL_CTX *ctx )
{
   X_ITEM *key = x_hashKeyAt( x_paritem( ctx, 1 ), ( size_t )x_parnint( ctx, 2 ) );

   if( key != NULL )
      x_itemReturn( ctx, key );
   else
      ( void )x_errorRaise( ctx->result, 1187, x_paritem( ctx, 1 ), x_paritem( ctx, 2 ) );
}

/* HGetKeys() and HGetValues(): the keys or, with values, the values in an array */
void x_hashHGetList( X_CALL_CTX *ctx, int values )
{
   X_ITEM list __attribute__( ( cleanup( x_itemClear ) ) ) = X_ITEM_NIL_INIT;
   size_t count = x_hashLen( x_paritem( ctx, 1 ) );
   size_t i;

   x_arrayNew( &list, ( int64_t )count );
   for( i = 1; i <= count; ++i )
   {
      if( values )
         x_arraySet( &list, i, x_hashValueAt( x_paritem( ctx, 1 ), i ) );
      else
         x_arraySet( &list, i, x_hashKeyAt( x_paritem( ctx, 1 ), i ) );
   }

   x_itemReturn( ctx, &list );
}

/* HGetPairAt(): the key and the value at a position as an array; a wrong position is an error */
void x_hashHGetPairAt( X_CALL_CTX *ctx )
{
   X_ITEM pair __attribute__( ( cleanup( x_itemClear ) ) ) = X_ITEM_NIL_INIT;
   size_t pos = ( size_t )x_parnint( ctx, 2 );

   if( x_hashKeyAt( x_paritem( ctx, 1 ), pos ) == NULL )
   {
      ( void )x_errorRaise( ctx->result, 1187, x_paritem( ctx, 1 ), x_paritem( ctx, 2 ) );
      return;
   }

   x_arrayNew( &pair, 2 );
   x_arraySet( &pair, 1, x_hashKeyAt( x_paritem( ctx, 1 ), pos ) );
   x_arraySet( &pair, 2, x_hashValueAt( x_paritem( ctx, 1 ), pos ) );
   x_itemReturn( ctx, &pair );
}

/* HGetVaaPos(): the order of adding of all keys: 1, 2, 3... */
void x_hashHGetVaaPos( X_CALL_CTX *ctx )
{
   X_ITEM order __attribute__( ( cleanup( x_itemClear ) ) ) = X_ITEM_NIL_INIT;
   size_t count = x_hashLen( x_paritem( ctx, 1 ) );
   size_t i;

   x_arrayNew( &order, ( int64_t )count );
   for( i = 1; i <= count; ++i )
      x_arraySetNInt( &order, i, ( int64_t )i );

   x_itemReturn( ctx, &order );
}

/* HGetValueAt() and HaaGetValueAt(): the value at a position; a wrong position is an error */
void x_hashHGetValueAt( X_CALL_CTX *ctx )
{
   X_ITEM *value = x_hashValueAt( x_paritem( ctx, 1 ), ( size_t )x_parnint( ctx, 2 ) );

   if( value != NULL )
      x_itemReturn( ctx, value );
   else
      ( void )x_errorRaise( ctx->result, 1187, x_paritem( ctx, 1 ), x_paritem( ctx, 2 ) );
}

/* HSetValueAt() and HaaSetValueAt(): the value at a position changed; a wrong position is an error */
void x_hashHSetValueAt( X_CALL_CTX *ctx )
{
   X_ITEM *value = x_hashValueAt( x_paritem( ctx, 1 ), ( size_t )x_parnint( ctx, 2 ) );

   if( value != NULL )
      x_itemCopy( value, x_paritem( ctx, 3 ) );
   else
      ( void )x_errorRaise( ctx->result, 1187, x_paritem( ctx, 1 ), x_paritem( ctx, 2 ) );
}
