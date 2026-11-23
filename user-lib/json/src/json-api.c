/*
 * Copyright (c) 2024-2026 xDev-Language
 *
 * Library...: xDev JSON
 * File......: json-api.c
 *
 */

#include "internal-json.h"

/* ---------------------------------------------------------------------------------------------
   JSON
   --------------------------------------------------------------------------------------------- */

// Text of a value: a hash becomes an object, an array an array, a date "YYYY-MM-DD", NIL null; objects,
// code blocks and pointers become null; with lPretty the text has new lines and indents; NIL on a cycle
// (see json_Error())
// json_Encode( xValue, [lPretty] ) --> cJson | NIL
X_FUNC( JSON_ENCODE )
{
   int pretty = X_ISLOG( ctx, 2 ) && x_parl( ctx, 2 );
   size_t length = 0;
   char *text = x_jsonEncode( x_paritem( ctx, 1 ), pretty, &length );

   if( text != NULL )
      x_retclen_buffer( ctx, text, length );
}

// Value of a UTF-8 text: an object becomes a hash with the order of its keys, an array an array, a number
// an integer when it is whole and fits, true/false a logical value, null NIL; NIL on error (see json_Error())
// json_Decode( cJson ) --> xValue | NIL
X_FUNC( JSON_DECODE )
{
   X_ITEM value __attribute__( ( cleanup( x_itemClear ) ) ) = X_ITEM_NIL_INIT;

   X_ARGCHECK( ctx, X_ISCHAR( ctx, 1 ), "JSON_DECODE" );

   if( x_jsonDecode( x_parc( ctx, 1 ), x_parclen( ctx, 1 ), &value ) )
      x_itemReturn( ctx, &value );
}

// Description of the error of the last json_Encode() or json_Decode(), "" when there was none
// json_Error( ) --> cMessage
X_FUNC( JSON_ERROR )
{
   x_retc( ctx, x_jsonError() );
}
