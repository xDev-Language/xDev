/*
 * Copyright (c) 2024-2026 xDev-Language
 *
 * Library...: xDev Runtime
 * File......: error.c
 *
 */

#include "classes.h"

/* The instance variables of Error in the order of X_ERR_ARGS .. X_ERR_TRIES */
static const struct
{
   const char *name;
   X_OO_VALUE_TYPE type;
} s_errorData[] = {
   { "ARGS", X_OO_VALUE_ARRAY },
   { "CANDEFAULT", X_OO_VALUE_LOGIC },
   { "CANRETRY", X_OO_VALUE_LOGIC },
   { "CANSUBSTITUTE", X_OO_VALUE_LOGIC },
   { "CARGO", X_OO_VALUE_ANY },
   { "DESCRIPTION", X_OO_VALUE_STRING },
   { "FILENAME", X_OO_VALUE_STRING },
   { "GENCODE", X_OO_VALUE_INT64 },
   { "OPERATION", X_OO_VALUE_STRING },
   { "OSCODE", X_OO_VALUE_INT64 },
   { "SEVERITY", X_OO_VALUE_INT64 },
   { "SUBCODE", X_OO_VALUE_INT64 },
   { "SUBSYSTEM", X_OO_VALUE_STRING },
   { "TRIES", X_OO_VALUE_INT64 }
};

/* ---------------------------------------------------------------------------------------------
   Error class
   --------------------------------------------------------------------------------------------- */

// Optionally fill the Error object with data and return it
// oError:New( [cSubSystem], [nGenCode], [nSubCode], [cOperation], [cDescription], [aArgs] ) --> self
X_FUNC_STATIC( ERROR_NEW )
{
   X_ARGCHECK_OPT( ctx, 1, X_ISNIL( ctx, 1 ) || X_ISCHAR( ctx, 1 ), "ERROR:NEW" );
   X_ARGCHECK_OPT( ctx, 2, X_ISNIL( ctx, 2 ) || X_ISNUM( ctx, 2 ), "ERROR:NEW" );
   X_ARGCHECK_OPT( ctx, 3, X_ISNIL( ctx, 3 ) || X_ISNUM( ctx, 3 ), "ERROR:NEW" );
   X_ARGCHECK_OPT( ctx, 4, X_ISNIL( ctx, 4 ) || X_ISCHAR( ctx, 4 ), "ERROR:NEW" );
   X_ARGCHECK_OPT( ctx, 5, X_ISNIL( ctx, 5 ) || X_ISCHAR( ctx, 5 ), "ERROR:NEW" );
   X_ARGCHECK_OPT( ctx, 6, X_ISNIL( ctx, 6 ) || X_ISARRAY( ctx, 6 ), "ERROR:NEW" );

   if( X_ISCHAR( ctx, 1 ) )
      x_arraySet( ctx->self, X_ERR_SUBSYSTEM, x_paritem( ctx, 1 ) );

   if( X_ISNUM( ctx, 2 ) )
      x_arraySetNI( ctx->self, X_ERR_GENCODE, x_parni( ctx, 2 ) );

   if( X_ISNUM( ctx, 3 ) )
      x_arraySetNI( ctx->self, X_ERR_SUBCODE, x_parni( ctx, 3 ) );

   if( X_ISCHAR( ctx, 4 ) )
      x_arraySet( ctx->self, X_ERR_OPERATION, x_paritem( ctx, 4 ) );

   if( X_ISCHAR( ctx, 5 ) )
      x_arraySet( ctx->self, X_ERR_DESCRIPTION, x_paritem( ctx, 5 ) );

   if( X_ISARRAY( ctx, 6 ) )
      x_arraySet( ctx->self, X_ERR_ARGS, x_paritem( ctx, 6 ) );

   x_itemCopy( ctx->result, ctx->self );
}

/* The class handle of Error, created on the first call */
static X_CLASS_HANDLE s_errorClass( void )
{
   static X_CLASS_HANDLE s_class = 0;
   size_t count = sizeof( s_errorData ) / sizeof( s_errorData[0] );
   size_t i;

   if( s_class == 0 )
   {
      s_class = x_clsCreate( count, "ERROR" );
      for( i = 0; i < count; ++i )
      {
         x_clsAddData( s_class, s_errorData[i].name, i + 1 );
         if( s_errorData[i].type != X_OO_VALUE_ANY )
            x_clsSetDataType( s_class, s_errorData[i].name, s_errorData[i].type );
      }

      x_clsAdd( s_class, "NEW", X_FUNCNAME( ERROR_NEW ) );
      x_clsAssociate( s_class );
   }

   return s_class;
}

/* The values of a new Error; args and cargo stay NIL */
static void s_errorDefaults( X_ITEM *object )
{
   x_arraySetL( object, X_ERR_CANDEFAULT, 0 );
   x_arraySetL( object, X_ERR_CANRETRY, 0 );
   x_arraySetL( object, X_ERR_CANSUBSTITUTE, 0 );
   x_arraySetC( object, X_ERR_DESCRIPTION, "" );
   x_arraySetC( object, X_ERR_FILENAME, "" );
   x_arraySetNI( object, X_ERR_GENCODE, 0 );
   x_arraySetC( object, X_ERR_OPERATION, "" );
   x_arraySetNI( object, X_ERR_OSCODE, 0 );
   x_arraySetNI( object, X_ERR_SEVERITY, 0 );
   x_arraySetNI( object, X_ERR_SUBCODE, 0 );
   x_arraySetC( object, X_ERR_SUBSYSTEM, "" );
   x_arraySetNI( object, X_ERR_TRIES, 0 );
}

/* A new Error object for the runtime; 0 when it cannot be created */
int x_errorNew( X_CALL_CTX *ctx, X_ITEM *result )
{
   if( !x_objCreate( ctx, result, s_errorClass() ) )
      return 0;

   s_errorDefaults( result );

   return 1;
}

// Error class: the description of a runtime error
// Error() --> oError
X_FUNC( ERROR )
{
   X_CLASS_HANDLE handle = s_errorClass();

   if( x_clsReturn( ctx, handle ) && x_objGetClass( ctx->result ) == handle )
      s_errorDefaults( ctx->result );
}

// Create a new Error object and optionally fill it with data; the same as Error():New( ... )
// ErrorNew( [cSubSystem], [nGenCode], [nSubCode], [cOperation], [cDescription], [aArgs] ) --> oError
X_FUNC( ERRORNEW )
{
   X_ITEM object __attribute__( ( cleanup( x_itemClear ) ) ) = X_ITEM_NIL_INIT;

   if( !x_errorNew( ctx, &object ) )
   {
      x_errorRaiseNamed( ctx->result, 3000, "ERRORNEW" );
      return;
   }

   x_objSend( ctx, ctx->result, &object, "NEW", ctx->argc, ctx->args );
}
