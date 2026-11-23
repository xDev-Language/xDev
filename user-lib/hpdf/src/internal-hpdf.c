/*
 * Copyright (c) 2024-2026 xDev-Language
 *
 * Library...: xDev HPDF
 * File......: internal-hpdf.c
 *
 */

#include "internal-hpdf.h"

/* ---------------------------------------------------------------------------------------------
   Arguments and results
   --------------------------------------------------------------------------------------------- */

/* A string argument, NULL when it is not a string */
const char *x_hpdfString( X_CALL_CTX *ctx, int n )
{
   return IIF( X_ISCHAR( ctx, n ), x_parc( ctx, n ), NULL );
}

/* HPDF_TRUE for .T. or a number other than zero */
HPDF_BOOL x_hpdfBool( X_CALL_CTX *ctx, int n )
{
   return IIF( X_ISLOG( ctx, n ), ( HPDF_BOOL )x_parl( ctx, n ), ( HPDF_BOOL )( x_parnint( ctx, n ) != 0 ) );
}

/* Numbers of an array as HPDF_REAL, released by x_xfree(); NULL for an empty or missing array */
HPDF_REAL *x_hpdfReals( const X_ITEM *array )
{
   int64_t count = IIF( array != NULL && X_IS_ARRAY( array ), x_arrayLen( array ), 0 );
   HPDF_REAL *values = IIF( count > 0, ( HPDF_REAL * )x_xgrabArray( ( size_t )count, sizeof( HPDF_REAL ) ), NULL );
   int64_t i;

   for( i = 0; i < count; i++ )
      values[i] = ( HPDF_REAL )x_itemGetND( x_arrayGetItemPtr( array, ( size_t )i + 1 ) );

   return values;
}

/* Store a value into an argument passed by reference */
void x_hpdfStore( X_CALL_CTX *ctx, int n, const X_ITEM *value )
{
   if( n >= 1 && n <= ctx->argc && ctx->args[n - 1].byReference )
      x_itemCopy( ( X_ITEM * )ctx->args[n - 1].value, value );
}

/* Store a point as { nX, nY } in the argument n passed by reference */
void x_hpdfStorePoint( X_CALL_CTX *ctx, int n, HPDF_Point point )
{
   X_ITEM value __attribute__( ( cleanup( x_itemClear ) ) ) = X_ITEM_NIL_INIT;

   x_hpdfPutPoint( &value, point );
   x_hpdfStore( ctx, n, &value );
}

/* Return a handle as a pointer, NIL for NULL */
void x_hpdfRetHandle( X_CALL_CTX *ctx, void *handle )
{
   if( handle != NULL )
      x_retptr( ctx, handle );
}

/* Return a string, NIL for NULL */
void x_hpdfRetString( X_CALL_CTX *ctx, const char *value )
{
   if( value != NULL )
      x_retc( ctx, value );
}

/* ---------------------------------------------------------------------------------------------
   Structures as arrays of numbers in the order of their fields
   --------------------------------------------------------------------------------------------- */

/* The first count numbers of an array; missing ones are 0 */
static void s_reals( const X_ITEM *array, HPDF_REAL *values, size_t count )
{
   int64_t length = IIF( array != NULL && X_IS_ARRAY( array ), x_arrayLen( array ), 0 );
   size_t i;

   for( i = 0; i < count; i++ )
      values[i] = ( HPDF_REAL )IIF( ( int64_t )i < length, x_itemGetND( x_arrayGetItemPtr( array, i + 1 ) ), 0 );
}

/* Numbers into a new array */
static void s_putReals( X_ITEM *result, const HPDF_REAL *values, size_t count )
{
   size_t i;

   x_arrayNew( result, ( int64_t )count );

   for( i = 0; i < count; i++ )
   {
      X_ITEM number = X_ITEM_NIL_INIT;

      x_itemPutND( &number, ( double )values[i] );
      x_arraySet( result, i + 1, &number );
   }
}

/* Return numbers as an array */
static void s_retReals( X_CALL_CTX *ctx, const HPDF_REAL *values, size_t count )
{
   X_ITEM result __attribute__( ( cleanup( x_itemClear ) ) ) = X_ITEM_NIL_INIT;

   s_putReals( &result, values, count );
   x_itemReturn( ctx, &result );
}

/* A point from { nX, nY } */
HPDF_Point x_hpdfPoint( const X_ITEM *array )
{
   HPDF_REAL v[2];

   s_reals( array, v, 2 );

   return ( HPDF_Point ){ v[0], v[1] };
}

/* A 3D point from { nX, nY, nZ } */
HPDF_Point3D x_hpdfPoint3D( const X_ITEM *array )
{
   HPDF_REAL v[3];

   s_reals( array, v, 3 );

   return ( HPDF_Point3D ){ v[0], v[1], v[2] };
}

/* A rectangle from { nLeft, nBottom, nRight, nTop } */
HPDF_Rect x_hpdfRect( const X_ITEM *array )
{
   HPDF_REAL v[4];

   s_reals( array, v, 4 );

   return ( HPDF_Rect ){ v[0], v[1], v[2], v[3] };
}

/* An RGB color from { nR, nG, nB } */
HPDF_RGBColor x_hpdfRGBColor( const X_ITEM *array )
{
   HPDF_REAL v[3];

   s_reals( array, v, 3 );

   return ( HPDF_RGBColor ){ v[0], v[1], v[2] };
}

/* A CMYK color from { nC, nM, nY, nK } */
HPDF_CMYKColor x_hpdfCMYKColor( const X_ITEM *array )
{
   HPDF_REAL v[4];

   s_reals( array, v, 4 );

   return ( HPDF_CMYKColor ){ v[0], v[1], v[2], v[3] };
}

/* A date from { nYear, nMonth, nDay, nHour, nMinutes, nSeconds, cInd ("+", "-", "Z" or " "), nOffHour, nOffMinutes } */
HPDF_Date x_hpdfDate( const X_ITEM *array )
{
   HPDF_Date date = { 0, 0, 0, 0, 0, 0, ' ', 0, 0 };
   int64_t length = IIF( array != NULL && X_IS_ARRAY( array ), x_arrayLen( array ), 0 );
   int *fields[] = { &date.year, &date.month, &date.day, &date.hour, &date.minutes, &date.seconds };
   X_ITEM *ind = NULL;
   size_t i;

   for( i = 0; i < 6 && ( int64_t )i < length; i++ )
      *fields[i] = ( int )x_itemGetNInt( x_arrayGetItemPtr( array, i + 1 ) );

   if( length >= 7 )
      ind = x_arrayGetItemPtr( array, 7 );

   if( ind != NULL && X_IS_STRING( ind ) && x_itemGetCLen( ind ) > 0 )
      date.ind = x_itemGetCPtr( ind )[0];

   if( length >= 8 )
      date.off_hour = ( int )x_itemGetNInt( x_arrayGetItemPtr( array, 8 ) );

   if( length >= 9 )
      date.off_minutes = ( int )x_itemGetNInt( x_arrayGetItemPtr( array, 9 ) );

   return date;
}

/* A point into a new array { nX, nY } */
void x_hpdfPutPoint( X_ITEM *result, HPDF_Point value )
{
   HPDF_REAL v[2] = { value.x, value.y };

   s_putReals( result, v, 2 );
}

/* Return a point as { nX, nY } */
void x_hpdfRetPoint( X_CALL_CTX *ctx, HPDF_Point value )
{
   HPDF_REAL v[2] = { value.x, value.y };

   s_retReals( ctx, v, 2 );
}

/* Return a rectangle as { nLeft, nBottom, nRight, nTop } */
void x_hpdfRetRect( X_CALL_CTX *ctx, HPDF_Rect value )
{
   HPDF_REAL v[4] = { value.left, value.bottom, value.right, value.top };

   s_retReals( ctx, v, 4 );
}

/* Return an RGB color as { nR, nG, nB } */
void x_hpdfRetRGBColor( X_CALL_CTX *ctx, HPDF_RGBColor value )
{
   HPDF_REAL v[3] = { value.r, value.g, value.b };

   s_retReals( ctx, v, 3 );
}

/* Return a CMYK color as { nC, nM, nY, nK } */
void x_hpdfRetCMYKColor( X_CALL_CTX *ctx, HPDF_CMYKColor value )
{
   HPDF_REAL v[4] = { value.c, value.m, value.y, value.k };

   s_retReals( ctx, v, 4 );
}

/* Return a transformation matrix as { nA, nB, nC, nD, nX, nY } */
void x_hpdfRetTransMatrix( X_CALL_CTX *ctx, HPDF_TransMatrix value )
{
   HPDF_REAL v[6] = { value.a, value.b, value.c, value.d, value.x, value.y };

   s_retReals( ctx, v, 6 );
}

/* Return a text width as { nNumChars, nNumWords, nWidth, nNumSpace } */
void x_hpdfRetTextWidth( X_CALL_CTX *ctx, HPDF_TextWidth value )
{
   HPDF_REAL v[4];

   v[0] = ( HPDF_REAL )value.numchars;
   v[1] = ( HPDF_REAL )value.numwords;
   v[2] = ( HPDF_REAL )value.width;
   v[3] = ( HPDF_REAL )value.numspace;

   s_retReals( ctx, v, 4 );
}

/* Return a dash mode as { aPattern, nPhase } */
void x_hpdfRetDashMode( X_CALL_CTX *ctx, HPDF_DashMode value )
{
   X_ITEM *array = x_itemArrayNew( 2 );
   X_ITEM item = X_ITEM_NIL_INIT;

   s_putReals( &item, value.ptn, IIF( value.num_ptn <= 8, value.num_ptn, 8 ) );
   x_arraySet( array, 1, &item );
   x_itemClear( &item );
   x_itemPutND( &item, ( double )value.phase );
   x_arraySet( array, 2, &item );

   x_itemReturn( ctx, array );
   x_itemRelease( array );
}
