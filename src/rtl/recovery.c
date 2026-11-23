/*
 * Copyright (c) 2024-2026 xDev-Language
 *
 * Library...: xDev Runtime
 * File......: recovery.c
 *
 */

#include "classes.h"

/* The Error object of a runtime error, for the block of ErrorBlock() */
int x_errorObject( X_CALL_CTX *ctx, X_ITEM *result, const X_ERROR *error )
{
   X_ITEM arguments __attribute__( ( cleanup( x_itemClear ) ) ) = X_ITEM_NIL_INIT;
   const X_ERROR_DEF *definition = error->definition;
   size_t i;

   if( error->object.nType != X_IT_NIL )
   {
      x_itemCopy( result, &error->object );
      return 1;
   }

   if( !x_errorNew( ctx, result ) )
      return 0;

   if( !x_arrayNew( &arguments, error->argumentCount ) )
      return 0;

   for( i = 0; i < ( size_t )error->argumentCount; ++i )
      x_arraySet( &arguments, i + 1, &error->arguments[i] );

   x_arraySet( result, X_ERR_ARGS, &arguments );
   x_arraySetL( result, X_ERR_CANDEFAULT, error->canDefault );
   x_arraySetL( result, X_ERR_CANSUBSTITUTE, definition->canSubstitute );
   x_arraySetC( result, X_ERR_DESCRIPTION, definition->description );
   x_arraySetNI( result, X_ERR_GENCODE, definition->genCode );
   x_arraySetC( result, X_ERR_OPERATION, definition->operation );
   x_arraySetNI( result, X_ERR_SEVERITY, definition->severity );
   x_arraySetNI( result, X_ERR_SUBCODE, definition->subCode );
   x_arraySetC( result, X_ERR_SUBSYSTEM, definition->subSystem );
   x_itemClear( &arguments );

   return 1;
}
