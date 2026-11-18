/*
 * Copyright (c) 2024-2026 xDev-Language
 *
 * Library: xDev 7.0.0 RDD
 * File: dbf-text.c
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "dbf-core.h"

#include <string.h>

void x_dbfTextClear( X_DBF_TEXT *text )
{
   x_xfree( text->data );
   text->data = NULL;
   text->length = 0;
}

X_DBF_STATUS x_dbfTextValidateUTF8( const char *text, size_t length )
{
   return IIF( x_utf8Validate( text, length ), X_DBF_OK, X_DBF_INVALID_TEXT );
}

X_DBF_STATUS x_dbfTextCopy( const char *text, size_t length, X_DBF_TEXT *result )
{
   char *data;

   if( !x_utf8Validate( text, length ) )
      return X_DBF_INVALID_TEXT;
   data = ( char * )x_xgrab( length + 1 );
   if( length != 0 )
      memcpy( data, text, length );
   data[length] = '\0';
   x_dbfTextClear( result );
   result->data = data;
   result->length = length;
   return X_DBF_OK;
}

/* How many UTF-8 bytes fit in limit bytes without cutting a character in two */
size_t x_dbfTextFit( const char *text, size_t length, size_t limit )
{
   if( length <= limit )
      return length;
   while( limit > 0 && ( ( unsigned char )text[limit] & 0xC0 ) == 0x80 )
      --limit;
   return limit;
}
