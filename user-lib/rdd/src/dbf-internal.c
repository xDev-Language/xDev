/*
 * Copyright (c) 2024-2026 xDev-Language
 *
 * Library: xDev 7.0.0 RDD
 * File: dbf-internal.c
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "dbf-internal.h"

#include <xdev/dbinfo.ch>
#include <xdev/dbstruct.ch>
#include <xdev/rddsys.ch>
#include <xdev/fileio.ch>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static X_DBF_SESSION *s_session;
static char           s_defaultDriver[X_DBF_MAX_DRIVER_NAME + 1] = "DBFNTX";
static int            s_netError;

const char *x_dbfDefaultDriver( void )
{
   return s_defaultDriver;
}

int x_dbfNetError( void )
{
   return s_netError;
}

void x_dbfNetErrorSet( int netError )
{
   s_netError = netError;
}

void x_dbfTextDrop( X_DBF_TEXT *text )
{
   x_dbfTextClear( text );
}

void x_dbfSchemaDrop( X_DBF_SCHEMA **schema )
{
   x_dbfSchemaDestroy( *schema );
}

X_DBF_SESSION *x_dbfRuntimeSession( void )
{
   if( s_session == NULL )
      s_session = x_dbfSessionCreate();
   return s_session;
}

DBF x_dbfCurrent( void )
{
   return x_dbfSessionCurrent( x_dbfRuntimeSession() );
}

DBF x_dbfCursorArea( X_CALL_CTX *ctx, const char *operation )
{
   DBF area = x_dbfCurrent();
   if( area != NULL )
   {
      X_DBF_STATUS status = x_dbfAreaPrepare( area );
      if( status != X_DBF_OK )
      {
         x_dbfRaise( ctx, status, operation );
         return NULL;
      }
   }
   return area;
}

DBF x_dbfRequireArea( X_CALL_CTX *ctx, const char *operation )
{
   DBF area = x_dbfCurrent();

   if( area == NULL )
      x_dbfRaise( ctx, X_DBF_AREA_NOT_FOUND, operation );
   return area;
}

int x_dbfInteger( const X_ITEM *item, int64_t *value )
{
   if( item != NULL )
   {
      if( item->nType == X_IT_INTEGER )
      {
         *value = item->u.integer;
         return 1;
      }
      if( item->nType == X_IT_DOUBLE && isfinite( item->u.dbl ) && item->u.dbl >= -0x1p63 && item->u.dbl < 0x1p63 )
      {
         *value = ( int64_t )item->u.dbl;
         return 1;
      }
   }
   return 0;
}

// DB_SETHEADER_* mode; YYEAR and EOL conflict with the xDev 7 header and are rejected
int x_dbfSetHeaderMode( const X_ITEM *item, int64_t *mode )
{
   return x_dbfInteger( item, mode ) && ( *mode & ~( int64_t )DB_SETHEADER_MASK ) == 0 &&
          ( *mode & ( DB_SETHEADER_YYEAR | DB_SETHEADER_EOL ) ) == 0;
}

int x_dbfString( const X_ITEM *item )
{
   return item != NULL && item->nType == X_IT_STRING && memchr( item->u.string.data, '\0', item->u.string.len ) == NULL;
}

int x_dbfOptional( X_CALL_CTX *ctx, int argument )
{
   return x_pcount( ctx ) < argument || X_ISNIL( ctx, argument );
}

void x_dbfSetRelation( X_CALL_CTX *ctx, int scoped, const char *operation )
{
   DBF             parent = x_dbfRequireArea( ctx, operation );
   DBF             child;
   X_DBF_RELATION *relation;
   X_DBF_STATUS    status;
   int64_t         number = 0;

   if( parent == NULL )
      return;
   if( !x_dbfString( x_paritem( ctx, 1 ) ) )
      x_dbfInteger( x_paritem( ctx, 1 ), &number );
   child = IIF( X_ISCHAR( ctx, 1 ), x_dbfSessionAlias( parent->session, x_parc( ctx, 1 ) ),
                IIF( number == 0, parent, x_dbfSessionArea( parent->session, ( uint32_t )number ) ) );
   if( child == NULL )
   {
      x_dbfRaise( ctx, X_DBF_AREA_NOT_FOUND, operation );
      return;
   }
   status = x_dbfAreaSetRelation( parent, child, x_paritem( ctx, 2 ), x_paritem( ctx, 3 ) );
   if( status == X_DBF_OK )
   {
      for( relation = parent->relations; relation->next != NULL; relation = relation->next )
      {
      }
      relation->scoped = scoped || x_parl( ctx, 4 );
   }
   x_dbfRaise( ctx, status, operation );
   x_ret( ctx );
}

static const char *s_baseName( const char *path )
{
   const char *name = path;

   for( ; *path != '\0'; ++path )
   {
      if( *path == '/' || *path == '\\' || *path == ':' )
         name = path + 1;
   }
   return name;
}

char *x_dbfTableName( const char *name )
{
   const char *base = s_baseName( name );
   size_t      length = strlen( name );
   int         extension = strchr( base, '.' ) != NULL;
   char       *resolved = ( char * )x_xgrab( length + IIF( extension, 1, 5 ) );
   char       *path;

   memcpy( resolved, name, length );
   memcpy( resolved + length, IIF( extension, "", ".dbf" ), IIF( extension, 1, 5 ) );
   path = x_fileDefName( resolved );
   x_xfree( resolved );
   return path;
}

char *x_dbfOpenPath( const char *name )
{
   X_DBF_TEXT initial __attribute__( ( cleanup( x_dbfTextDrop ) ) ) = { x_dbfTableName( name ), 0 };
   int        isFile, isDirectory;

   if( strpbrk( name, "/\\:" ) == NULL && ( !x_fsExists( initial.data, &isFile, &isDirectory ) || !isFile ) )
   {
      const char *search = x_setPath( NULL );
      while( *search != '\0' )
      {
         size_t      length = strcspn( search, ";," );
         X_DBF_TEXT  candidate __attribute__( ( cleanup( x_dbfTextDrop ) ) ) = { NULL, 0 };
         const char *base = s_baseName( initial.data );
         int         slash = length != 0 && strchr( "/\\:", search[length - 1] ) == NULL;

         candidate.data = ( char * )x_xgrab( length + slash + strlen( base ) + 1 );
         memcpy( candidate.data, search, length );
         if( slash )
            candidate.data[length] = '/';
         strcpy( candidate.data + length + slash, base );
         if( x_fsExists( candidate.data, &isFile, &isDirectory ) && isFile )
         {
            char *found = candidate.data;
            candidate.data = NULL;
            return found;
         }
         search += length;
         if( *search != '\0' )
            ++search;
      }
   }
   {
      char *resolved = initial.data;
      initial.data = NULL;
      return resolved;
   }
}

char *x_dbfAliasName( const char *path )
{
   const char *base = s_baseName( path );
   const char *extension = strrchr( base, '.' );
   size_t      length = IIF( extension != NULL, ( size_t )( extension - base ), strlen( base ) );
   char       *alias;
   size_t      i;

   if( length == 0 )
      return x_strdup( "DBF" );
   alias = ( char * )x_xgrab( length + 1 );
   for( i = 0; i < length; ++i )
   {
      unsigned char character = ( unsigned char )base[i];
      if( character >= 'a' && character <= 'z' )
         character = ( unsigned char )( character - 'a' + 'A' );
      alias[i] = IIF( character == '_' || ( character >= 'A' && character <= 'Z' ) ||
                         ( i != 0 && character >= '0' && character <= '9' ),
                      ( char )character, '_' );
   }
   alias[length] = '\0';
   return alias;
}

X_DBF_STATUS x_dbfSchema( const X_ITEM *structure, X_DBF_SCHEMA **schema )
{
   X_DBF_TEXT        specifications __attribute__( ( cleanup( x_dbfTextDrop ) ) ) = { NULL, 0 };
   X_DBF_FIELD_SPEC *fields;
   int64_t           count = x_arrayLen( structure );
   size_t            i;

   if( count <= 0 || count > ( UINT16_MAX - 34 ) / 32 )
      return X_DBF_INVALID_SCHEMA;
   specifications.data = ( char * )x_xgrab( ( size_t )count * sizeof( X_DBF_FIELD_SPEC ) );
   fields = ( X_DBF_FIELD_SPEC * )specifications.data;
   for( i = 0; i < ( size_t )count; ++i )
   {
      const X_ITEM *row = x_arrayGetItemPtr( structure, i + 1 );
      const X_ITEM *name, *type;
      int64_t       length, decimals;

      if( !X_IS_ARRAY( row ) || x_arrayLen( row ) < DBS_ALEN )
         return X_DBF_INVALID_SCHEMA;
      name = x_arrayGetItemPtr( row, DBS_NAME );
      type = x_arrayGetItemPtr( row, DBS_TYPE );
      if( !x_dbfString( name ) || !x_dbfString( type ) || type->u.string.len != 1 ||
          !x_dbfInteger( x_arrayGetItemPtr( row, DBS_LEN ), &length ) ||
          !x_dbfInteger( x_arrayGetItemPtr( row, DBS_DEC ), &decimals ) || decimals < 0 || decimals > UINT8_MAX )
         return X_DBF_INVALID_SCHEMA;
      fields[i].name = name->u.string.data;
      fields[i].type = ( X_DBF_FIELD_TYPE )type->u.string.data[0];
      if( fields[i].type >= 'a' && fields[i].type <= 'z' )
         fields[i].type = ( X_DBF_FIELD_TYPE )( fields[i].type - 'a' + 'A' );
      if( fields[i].type == X_DBF_LOGICAL )
         fields[i].length = 1;
      else
      {
         if( length <= 0 || length > UINT16_MAX )
            return X_DBF_INVALID_SCHEMA;
         fields[i].length = ( uint16_t )length;
      }
      fields[i].decimals = ( uint8_t )decimals;
   }
   return x_dbfSchemaCreate( fields, ( size_t )count, schema );
}

int x_dbfContention( X_DBF_STATUS status )
{
   return status == X_DBF_LOCK_FAILED || ( ( status == X_DBF_OPEN_ERROR || status == X_DBF_CREATE_ERROR ) &&
                                           ( x_fsError() == 32 || x_fsError() == 33 ) );
}

X_DBF_STATUS x_dbfCloseAreas( int continueAfterError )
{
   X_DBF_SESSION *session = x_dbfRuntimeSession();
   X_DBF_STATUS   first = X_DBF_OK;
   uint32_t       after = 0;

   for( ;; )
   {
      DBF          area = session->areas;
      uint32_t     number;
      X_DBF_STATUS status;
      while( area != NULL && area->number <= after )
         area = area->next;
      if( area == NULL )
         break;
      number = area->number;
      status = x_dbfAreaClose( area );
      if( status != X_DBF_OK )
      {
         if( !continueAfterError )
            return status;
         if( first == X_DBF_OK )
            first = status;
         after = number;
      }
   }
   return first;
}

static X_FUNC( s_statics )
{
   x_dbfRuntimeSession();
}

static X_FUNC( s_init )
{
}

static X_FUNC( s_exit )
{
   x_dbfRaise( ctx, x_dbfCloseAreas( 1 ), "DBCLOSEALL" );
}

static X_FUNC( s_clear )
{
   x_dbfSessionDiscard( s_session );
   s_session = NULL;
   s_netError = 0;
   strcpy( s_defaultDriver, "DBFNTX" );
}

static void s_register( void ) __attribute__( ( constructor ) );

static void s_register( void )
{
   x_moduleRegister( s_statics, s_init, s_exit, s_clear );
}

void x_dbfDeleted( X_CALL_CTX *ctx, int deleted, const char *operation )
{
   DBF area = x_dbfRequireArea( ctx, operation );

   if( area == NULL )
      return;
   {
      X_DBF_STATUS status = x_dbfAreaPrepare( area );
      if( status != X_DBF_OK )
      {
         x_dbfRaise( ctx, status, operation );
         return;
      }
   }
   if( !area->state.eof )
      x_dbfRaise( ctx, x_dbfAreaDelete( area, deleted ), operation );
   x_ret( ctx );
}

void x_dbfRecordCount( X_CALL_CTX *ctx, const char *operation )
{
   DBF      area = x_dbfCurrent();
   uint64_t count = 0;

   if( area != NULL )
      x_dbfRaise( ctx, x_dbfAreaRecordCount( area, &count ), operation );
   x_retnint( ctx, ( int64_t )count );
}

void x_dbfPutFieldValue( X_CALL_CTX *ctx, DBF area, size_t ordinal, const X_ITEM *value, const char *operation )
{
   X_ITEM       assigned __attribute__( ( cleanup( x_itemClear ) ) ) = X_ITEM_NIL_INIT;
   X_DBF_STATUS status;

   x_itemCopy( &assigned, value );
   status = IIF( area->state.eof, X_DBF_OK, x_dbfFieldPutValue( area, ordinal, &assigned ) );
   if( status == X_DBF_INVALID_FIELD_VALUE || status == X_DBF_FIELD_OVERFLOW )
   {
      const X_DBF_FIELD *field = x_dbfSchemaField( area->schema, ordinal );
      X_ERROR_DEF        error = { 2,
                                   IIF( status == X_DBF_FIELD_OVERFLOW, 34, 33 ),
                                   IIF( status == X_DBF_FIELD_OVERFLOW, 1021, 1020 ),
                                   0,
                                   area->driver->name,
                                   x_dbfStatusName( status ),
                                   field->name };
      x_errorRaiseDefaultDefinition( ctx->result, &error, &assigned, NULL );
   }
   else if( status != X_DBF_OK )
   {
      x_dbfRaise( ctx, status, operation );
      return;
   }
   x_itemReturn( ctx, &assigned );
}

static void s_boxDrop( X_BOX **box )
{
   if( *box != NULL )
      x_boxRelease( *box );
}

static void s_selectionRestore( uint32_t *number )
{
   if( s_session != NULL )
      s_session->current = *number;
}

static DBF s_syntaxArea( X_CALL_CTX *ctx, const X_ITEM *selector )
{
   X_DBF_SESSION *session = x_dbfRuntimeSession();
   DBF            area;
   int64_t        number;

   if( selector == NULL || selector->nType == X_IT_NIL )
      return x_dbfRequireArea( ctx, "FIELD" );
   if( x_dbfString( selector ) )
   {
      area = x_dbfSessionAlias( session, selector->u.string.data );
      if( area == NULL )
      {
         X_ERROR_DEF error = { 2, 15, 1002, 0, "BASE", "Alias does not exist", selector->u.string.data };
         x_errorRaiseDefinition( ctx->result, &error, selector, NULL );
      }
      return area;
   }
   if( !x_dbfInteger( selector, &number ) || number <= 0 || number > X_DBF_MAX_AREA )
   {
      x_dbfRaise( ctx, X_DBF_INVALID_ARGUMENT, "ALIAS" );
      return NULL;
   }
   area = x_dbfSessionArea( session, ( uint32_t )number );
   if( area == NULL )
      x_dbfRaise( ctx, X_DBF_AREA_NOT_FOUND, "ALIAS" );
   return area;
}

static size_t s_syntaxField( X_CALL_CTX *ctx, DBF area, const char *name )
{
   size_t       ordinal;
   X_DBF_STATUS status = x_dbfAreaPrepare( area );

   if( status != X_DBF_OK )
   {
      x_dbfRaise( ctx, status, name );
      return 0;
   }
   ordinal = x_dbfSchemaFindField( area->schema, name );
   if( ordinal == 0 )
   {
      X_ERROR_DEF error = { 2, 14, 1003, 0, "BASE", "Field does not exist", name };
      x_errorRaiseDefinition( ctx->result, &error, NULL, NULL );
   }
   return ordinal;
}

uint32_t x_dbfAliasEnter( X_CALL_CTX *ctx, const X_ITEM *selector )
{
   uint32_t previous = x_dbfSessionSelected( x_dbfRuntimeSession() );
   DBF      area = s_syntaxArea( ctx, selector );

   if( area != NULL )
      area->session->current = area->number;
   return previous;
}

void x_dbfAliasLeave( uint32_t previous )
{
   s_selectionRestore( &previous );
}

void x_dbfFieldRead( X_CALL_CTX *ctx, X_ITEM *result, const X_ITEM *selector, const char *name )
{
   X_CALL_CTX call = *ctx;
   DBF        area;
   size_t     ordinal;

   call.result = result;
   area = s_syntaxArea( &call, selector );
   if( area == NULL )
      return;
   ordinal = s_syntaxField( &call, area, name );
   if( ordinal != 0 )
      x_dbfRaise( &call, x_dbfFieldGetValue( area, ordinal, result ), name );
}

static void s_fieldReference( X_CALL_CTX *ctx, X_ITEM *const *env )
{
   const X_ITEM *name = x_boxItem( ( X_BOX * )env[0] );
   uint32_t      number = ( uint32_t )x_boxItem( ( X_BOX * )env[1] )->u.integer;
   uint64_t      identity = ( uint64_t )x_boxItem( ( X_BOX * )env[2] )->u.integer;
   uint32_t      previous __attribute__( ( cleanup( s_selectionRestore ) ) ) = s_session->current;
   DBF           area = x_dbfSessionArea( s_session, number );
   size_t        ordinal;

   if( area == NULL || area->identity != identity )
   {
      x_dbfRaise( ctx, X_DBF_INVALID_STATE, name->u.string.data );
      return;
   }
   s_session->current = number;
   ordinal = s_syntaxField( ctx, area, name->u.string.data );
   if( ordinal == 0 )
      return;
   if( x_pcount( ctx ) == 0 )
      x_dbfRaise( ctx, x_dbfFieldGetValue( area, ordinal, ctx->result ), name->u.string.data );
   else
      x_dbfPutFieldValue( ctx, area, ordinal, x_paritem( ctx, 1 ), name->u.string.data );
}

void x_dbfFieldReference( X_CALL_CTX *ctx, X_ITEM *result, const X_ITEM *selector, const char *name )
{
   X_CALL_CTX call = *ctx;
   X_BOX     *field __attribute__( ( cleanup( s_boxDrop ) ) ) = NULL;
   X_BOX     *number __attribute__( ( cleanup( s_boxDrop ) ) ) = NULL;
   X_BOX     *identity __attribute__( ( cleanup( s_boxDrop ) ) ) = NULL;
   DBF        area;
   size_t     ordinal;

   call.result = result;
   area = s_syntaxArea( &call, selector );
   if( area == NULL )
      return;
   ordinal = s_syntaxField( &call, area, name );
   if( ordinal == 0 )
      return;
   field = x_boxNew();
   number = x_boxNew();
   identity = x_boxNew();
   name = area->schema->fields[ordinal - 1].name;
   x_itemPutStringBuffer( &field->item, x_strdup( name ), strlen( name ) );
   number->item = ( X_ITEM ){ X_IT_INTEGER, { .integer = area->number } };
   identity->item = ( X_ITEM ){ X_IT_INTEGER, { .integer = ( int64_t )area->identity } };
   {
      X_ITEM *env[] = { ( X_ITEM * )field, ( X_ITEM * )number, ( X_ITEM * )identity };
      x_itemBlockInto( &call, result, s_fieldReference, env, 3 );
   }
}

static void s_fieldBlock( X_CALL_CTX *ctx, X_ITEM *const *env )
{
   X_DBF_SESSION *session = x_dbfRuntimeSession();
   const X_ITEM  *name = x_boxItem( ( X_BOX * )env[0] );
   uint32_t       number = ( uint32_t )x_boxItem( ( X_BOX * )env[1] )->u.integer;
   uint32_t       previous __attribute__( ( cleanup( s_selectionRestore ) ) ) = session->current;
   DBF            area;
   size_t         ordinal;
   X_DBF_STATUS   status;

   if( number != 0 )
      session->current = number;
   area = x_dbfRequireArea( ctx, IIF( number != 0, "FIELDWBLOCK", "FIELDBLOCK" ) );
   if( area == NULL )
      return;
   status = x_dbfAreaPrepare( area );
   if( status != X_DBF_OK )
   {
      x_dbfRaise( ctx, status, IIF( number != 0, "FIELDWBLOCK", "FIELDBLOCK" ) );
      return;
   }
   ordinal = x_dbfSchemaFindField( area->schema, name->u.string.data );
   if( ordinal == 0 )
   {
      X_ERROR_DEF error = {
         2, 14, 6000 + X_DBF_INVALID_ARGUMENT, 0, "XDEVDBF", "Field not found", name->u.string.data
      };
      x_errorRaiseDefinition( ctx->result, &error, name, NULL );
      return;
   }
   if( x_dbfOptional( ctx, 1 ) )
      status = x_dbfFieldGetValue( area, ordinal, ctx->result );
   else
   {
      x_dbfPutFieldValue( ctx, area, ordinal, x_paritem( ctx, 1 ), IIF( number != 0, "FIELDWBLOCK", "FIELDBLOCK" ) );
      return;
   }
   x_dbfRaise( ctx, status, IIF( number != 0, "FIELDWBLOCK", "FIELDBLOCK" ) );
}

void x_dbfMakeFieldBlock( X_CALL_CTX *ctx, int fixedArea )
{
   X_DBF_SESSION *session = x_dbfRuntimeSession();
   X_BOX         *field __attribute__( ( cleanup( s_boxDrop ) ) ) = NULL;
   X_BOX         *workArea __attribute__( ( cleanup( s_boxDrop ) ) ) = NULL;
   int64_t        number = 0;
   const char    *text;
   size_t         length;
   char           name[X_DBF_MAX_FIELD_NAME + 1];
   DBF            area;

   x_ret( ctx );
   if( !x_dbfString( x_paritem( ctx, 1 ) ) ||
       ( fixedArea && ( !x_dbfInteger( x_paritem( ctx, 2 ), &number ) || number <= 0 || number > X_DBF_MAX_AREA ) ) )
      return;
   text = x_parc( ctx, 1 );
   length = x_parclen( ctx, 1 );
   while( length != 0 && strchr( " \t\r\n\f\v", *text ) != NULL )
   {
      ++text;
      --length;
   }
   while( length != 0 && strchr( " \t\r\n\f\v", text[length - 1] ) != NULL )
      --length;
   if( length == 0 || length > X_DBF_MAX_FIELD_NAME )
      return;
   memcpy( name, text, length );
   name[length] = '\0';
   for( area = session->areas; area != NULL; area = area->next )
   {
      size_t ordinal = x_dbfSchemaFindField( area->schema, name );
      if( ordinal != 0 )
      {
         strcpy( name, area->schema->fields[ordinal - 1].name );
         break;
      }
   }
   if( area == NULL )
      return;
   field = x_boxNew();
   workArea = x_boxNew();
   x_itemPutStringBuffer( &field->item, x_strdup( name ), strlen( name ) );
   workArea->item = ( X_ITEM ){ X_IT_INTEGER, { .integer = number } };
   {
      X_ITEM *env[] = { ( X_ITEM * )field, ( X_ITEM * )workArea };
      x_itemBlockInto( ctx, ctx->result, s_fieldBlock, env, 2 );
   }
}

DBF x_dbfEvalAreaBlock( X_CALL_CTX *ctx, uint32_t number, uint64_t identity, const X_ITEM *block, X_ITEM *result )
{
   X_DBF_SESSION *session = x_dbfRuntimeSession();
   DBF            area = x_dbfSessionArea( session, number );
   X_DBF_STATUS   status;

   if( area == NULL || area->identity != identity )
   {
      x_dbfRaise( ctx, X_DBF_INVALID_STATE, "DBEVAL" );
      return NULL;
   }
   status = x_dbfAreaEvalBlock( ctx, area, block, result );
   if( status != X_DBF_OK )
   {
      x_dbfRaise( ctx, status, "DBEVAL" );
      return NULL;
   }
   return x_dbfSessionArea( session, number );
}

DBF x_dbfNamedArea( X_CALL_CTX *ctx, int argument, int defaultCurrent )
{
   X_DBF_SESSION *session = x_dbfRuntimeSession();
   int64_t        number;

   if( x_dbfOptional( ctx, argument ) )
      return IIF( defaultCurrent, x_dbfSessionCurrent( session ), NULL );
   if( x_dbfString( x_paritem( ctx, argument ) ) )
      return x_dbfSessionAlias( session, x_parc( ctx, argument ) );
   if( x_dbfInteger( x_paritem( ctx, argument ), &number ) && number >= 0 && number <= X_DBF_MAX_AREA )
      return IIF( number == 0, x_dbfSessionCurrent( session ), x_dbfSessionArea( session, ( uint32_t )number ) );
   return NULL;
}

void x_dbfDefault( X_CALL_CTX *ctx, const char *operation )
{
   X_DBF_DRIVER_ENTRY *driver;

   if( x_parclen( ctx, 1 ) != 0 )
   {
      driver = x_dbfDriverFind( x_dbfRuntimeSession(), x_parc( ctx, 1 ) );
      if( driver == NULL )
      {
         x_dbfRaise( ctx, X_DBF_DRIVER_NOT_FOUND, operation );
         return;
      }
      x_retc( ctx, s_defaultDriver );
      strcpy( s_defaultDriver, driver->name );
   }
   else
      x_retc( ctx, s_defaultDriver );
}

void x_dbfLock( X_CALL_CTX *ctx, const char *operation, int fileLock, int release )
{
   int64_t      record = 0;
   DBF          area;
   X_DBF_STATUS status;

   if( !fileLock && !x_dbfOptional( ctx, 1 ) )
      x_dbfInteger( x_paritem( ctx, 1 ), &record );
   area = x_dbfRequireArea( ctx, operation );
   if( area == NULL )
      return;
   status = x_dbfAreaPrepare( area );
   if( status != X_DBF_OK )
   {
      x_dbfRaise( ctx, status, operation );
      return;
   }
   if( !fileLock && record == 0 )
   {
      if( area->state.eof )
      {
         x_retl( ctx, 1 );
         return;
      }
      record = ( int64_t )area->state.record;
   }
   status = IIF( release, x_dbfAreaUnlock( area, 0 ), X_DBF_OK );
   if( status == X_DBF_OK )
      status = x_dbfAreaLock( area, ( uint64_t )record );
   if( status != X_DBF_OK && status != X_DBF_LOCK_FAILED )
   {
      x_dbfRaise( ctx, status, operation );
      return;
   }
   x_retl( ctx, status == X_DBF_OK );
}

void x_dbfFieldFile( X_CALL_CTX *ctx, int import, int64_t ordinal, int64_t mode, const char *operation )
{
   DBF          area;
   X_DBF_STATUS status;

   area = x_dbfRequireArea( ctx, operation );
   if( area == NULL )
      return;
   status = x_dbfAreaPrepare( area );
   if( status != X_DBF_OK )
   {
      x_dbfRaise( ctx, status, operation );
      return;
   }
   if( ( size_t )ordinal > area->schema->count || ( import && area->state.eof ) )
   {
      x_retl( ctx, 0 );
      return;
   }
   status = import ? x_dbfFieldFilePut( area, ( size_t )ordinal, x_parc( ctx, 2 ) )
                   : x_dbfFieldFileGet( area, ( size_t )ordinal, x_parc( ctx, 2 ), mode != 0 );
   if( status != X_DBF_OK && status != X_DBF_NO_RECORD )
      x_dbfRaise( ctx, status, operation );
   x_retl( ctx, status == X_DBF_OK );
}

void x_dbfReorganize( X_CALL_CTX *ctx, int pack, const char *operation )
{
   DBF area = x_dbfRequireArea( ctx, operation );

   if( area == NULL )
      return;
   if( !x_dbfOptional( ctx, 1 ) )
   {
      x_dbfRaise( ctx, X_DBF_UNSUPPORTED_OPERATION, operation );
      return;
   }
   x_dbfRaise( ctx, IIF( pack, x_dbfAreaPack( area ), x_dbfAreaZap( area ) ), operation );
   x_ret( ctx );
}

void x_dbfOrderInfoApi( X_CALL_CTX *ctx, int info, const X_ITEM *value, const char *operation )
{
   DBF          area = x_dbfRequireArea( ctx, operation );
   X_DBF_STATUS status;

   if( area == NULL )
      return;
   status = x_dbfAreaPrepare( area );
   if( status == X_DBF_OK )
      status = x_dbfOrderInfo( area, x_paritem( ctx, 1 ), x_paritem( ctx, 2 ), info, value, ctx->result );
   x_dbfRaise( ctx, status, operation );
}

void x_dbfOrderExtension( X_CALL_CTX *ctx, const char *operation )
{
   DBF                       area = x_dbfCurrent();
   const X_DBF_DRIVER_ENTRY *driver =
      IIF( area != NULL, area->driver, x_dbfDriverFind( x_dbfRuntimeSession(), s_defaultDriver ) );
   x_dbfRaise( ctx, x_dbfDriverInfo( driver, RDDI_ORDBAGEXT, ctx->result ), operation );
}

void x_dbfBuildDrop( X_DBF_INDEX_BUILD *build )
{
   x_itemClear( &build->whileBlock );
   x_itemClear( &build->evalBlock );
   memset( build, 0, sizeof( *build ) );
}

void x_dbfCreateIndex( X_CALL_CTX *ctx, int order, const char *operation )
{
   DBF                 area = x_dbfRequireArea( ctx, operation );
   X_DBF_INDEX_OPTIONS options;
   X_DBF_INDEX_BUILD   build __attribute__( ( cleanup( x_dbfBuildDrop ) ) ) = { 0 };
   X_ITEM              condition __attribute__( ( cleanup( x_itemClear ) ) ) = X_ITEM_NIL_INIT;
   X_ITEM              text __attribute__( ( cleanup( x_itemClear ) ) ) = X_ITEM_NIL_INIT;
   int                 expression = IIF( order, 3, 2 );

   if( area == NULL )
      return;
   if( area->indexBusy )
   {
      x_dbfRaise( ctx, X_DBF_INVALID_STATE, operation );
      return;
   }
   memset( &options, 0, sizeof( options ) );
   options.path = x_parc( ctx, 1 );
   options.name = IIF( order && !x_dbfOptional( ctx, 2 ), x_parc( ctx, 2 ), NULL );
   options.expression = x_parc( ctx, expression );
   options.block = x_paritem( ctx, expression + 1 );
   options.unique = IIF( x_dbfOptional( ctx, expression + 2 ), x_setUnique, x_parl( ctx, expression + 2 ) );
   options.descending = area->indexDescending;
   options.additive = area->indexAdditive;
   build = area->indexBuild;
   memset( &area->indexBuild, 0, sizeof( area->indexBuild ) );
   options.build = &build;
   x_itemCopy( &condition, &area->indexCondition );
   x_itemCopy( &text, &area->indexConditionText );
   x_itemClear( &area->indexCondition );
   x_itemClear( &area->indexConditionText );
   area->indexDescending = 0;
   area->indexAdditive = 0;
   options.forBlock = &condition;
   options.condition = IIF( text.nType == X_IT_STRING, text.u.string.data, NULL );
   {
      X_DBF_STATUS status = x_dbfOrderCreate( area, &options );
      x_dbfRaise( ctx, status, operation );
   }
   x_ret( ctx );
}

void x_dbfClearIndexes( X_CALL_CTX *ctx, const char *operation )
{
   DBF area = x_dbfRequireArea( ctx, operation );
   if( area != NULL )
      x_dbfRaise( ctx, x_dbfOrderClear( area ), operation );
   x_ret( ctx );
}

void x_dbfRebuildIndexes( X_CALL_CTX *ctx, const char *operation )
{
   DBF area = x_dbfRequireArea( ctx, operation );
   if( area != NULL )
      x_dbfRaise( ctx, x_dbfOrderRebuild( area ), operation );
   x_ret( ctx );
}

void x_dbfExistingIndex( X_CALL_CTX *ctx, int argument, const char *operation )
{
   DBF          area = x_dbfRequireArea( ctx, operation );
   X_ITEM       previous __attribute__( ( cleanup( x_itemClear ) ) ) = X_ITEM_NIL_INIT;
   X_DBF_STATUS status;

   if( area == NULL )
      return;
   status =
      x_dbfOrderOpen( area, x_parc( ctx, 1 ), IIF( x_dbfOptional( ctx, argument ), NULL, x_paritem( ctx, argument ) ) );
   if( status == X_DBF_OK && argument == 3 && !x_dbfOptional( ctx, 2 ) )
      status = x_dbfOrderFocus( area, x_paritem( ctx, 2 ), NULL, &previous );
   x_dbfRaise( ctx, status, operation );
   x_ret( ctx );
}

void x_dbfLocate( X_CALL_CTX *ctx, int continuing )
{
   const char  *operation = IIF( continuing, "DBCONTINUE", "DBLOCATE" );
   DBF          area = x_dbfRequireArea( ctx, operation );
   X_ITEM       condition __attribute__( ( cleanup( x_itemClear ) ) ) = X_ITEM_NIL_INIT;
   X_ITEM       whileCondition __attribute__( ( cleanup( x_itemClear ) ) ) = X_ITEM_NIL_INIT;
   X_ITEM       value __attribute__( ( cleanup( x_itemClear ) ) ) = X_ITEM_NIL_INIT;
   X_DBF_STATUS status = X_DBF_OK;
   uint32_t     number;
   uint64_t     identity;
   int64_t      remaining = 0, record = 0;
   int          hasNext = 0, hasRecord = 0;
   if( area == NULL )
      return;
   number = area->number;
   identity = area->identity;
   if( continuing )
   {
      if( area->locateFor.nType != X_IT_BLOCK )
      {
         x_ret( ctx );
         return;
      }
      x_itemCopy( &condition, &area->locateFor );
      status = x_dbfAreaSkip( area, 1, x_setDeleted );
   }
   else
   {
      hasNext = !x_dbfOptional( ctx, 3 );
      hasRecord = !x_dbfOptional( ctx, 4 );
      if( hasNext )
         x_dbfInteger( x_paritem( ctx, 3 ), &remaining );
      if( hasRecord )
         x_dbfInteger( x_paritem( ctx, 4 ), &record );
      x_itemClear( &area->locateFor );
      if( X_ISBLOCK( ctx, 1 ) )
         x_itemCopy( &area->locateFor, x_paritem( ctx, 1 ) );
      x_itemCopy( &condition, &area->locateFor );
      if( X_ISBLOCK( ctx, 2 ) )
         x_itemCopy( &whileCondition, x_paritem( ctx, 2 ) );
      if( hasRecord )
         status = x_dbfAreaGoTo( area, IIF( record > 0, ( uint64_t )record, 0 ) );
      else if( hasNext && remaining <= 0 )
      {
         x_ret( ctx );
         return;
      }
      else if( !hasNext && whileCondition.nType == X_IT_NIL && !x_parl( ctx, 5 ) )
         status = x_dbfAreaGoTop( area, x_setDeleted );
      else
         status = x_dbfAreaPrepare( area );
   }
   if( status != X_DBF_OK )
   {
      x_dbfRaise( ctx, status, operation );
      return;
   }
   area->state.found = 0;
   while( !area->state.eof )
   {
      if( whileCondition.nType == X_IT_BLOCK )
      {
         area = x_dbfEvalAreaBlock( ctx, number, identity, &whileCondition, &value );
         if( area == NULL )
            return;
         if( value.nType != X_IT_LOGICAL || !value.u.logical )
            break;
      }
      if( condition.nType == X_IT_BLOCK )
      {
         area = x_dbfEvalAreaBlock( ctx, number, identity, &condition, &value );
         if( area == NULL )
            return;
      }
      if( condition.nType == X_IT_NIL || ( value.nType == X_IT_LOGICAL && value.u.logical ) )
      {
         area->state.found = 1;
         break;
      }
      if( hasRecord || ( hasNext && --remaining == 0 ) )
         break;
      status = x_dbfAreaSkip( area, 1, x_setDeleted );
      if( status != X_DBF_OK )
      {
         x_dbfRaise( ctx, status, operation );
         return;
      }
   }
   x_ret( ctx );
}

int x_dbfDecimalText( const char *text, size_t length, unsigned int decimals )
{
   /* Stored text that already has exactly the field decimals is used as is, without rounding */
   const char *dot = ( const char * )memchr( text, '.', length );
   size_t      i;
   for( i = 0; i < length; ++i )
      if( !( ( text[i] >= '0' && text[i] <= '9' ) || text[i] == '.' || ( i == 0 && text[i] == '-' ) ) )
         return 0;
   if( decimals == 0 )
      return length != 0 && dot == NULL;
   return dot != NULL && ( size_t )( text + length - dot - 1 ) == decimals;
}

// FOR, WHILE, NEXT, RECORD and REST arguments of a scope starting at first
int x_dbfScopeArgs( X_CALL_CTX *ctx, int first )
{
   int64_t number;

   return ( x_dbfOptional( ctx, first ) || X_ISBLOCK( ctx, first ) ) &&
          ( x_dbfOptional( ctx, first + 1 ) || X_ISBLOCK( ctx, first + 1 ) ) &&
          ( x_dbfOptional( ctx, first + 2 ) || x_dbfInteger( x_paritem( ctx, first + 2 ), &number ) ) &&
          ( x_dbfOptional( ctx, first + 3 ) || x_dbfInteger( x_paritem( ctx, first + 3 ), &number ) ) &&
          ( x_dbfOptional( ctx, first + 4 ) || X_ISLOG( ctx, first + 4 ) );
}

// An optional list of field names: NIL or an array
int x_dbfNameList( X_CALL_CTX *ctx, int argument )
{
   return x_dbfOptional( ctx, argument ) || X_ISARRAY( ctx, argument );
}

/* DbCreate() and DbCreateFrom(): creates the table; open -1 leaves it closed, 0 opens it in the
   current work area and 1 in a new one */
void x_dbfCreateTable( X_CALL_CTX *ctx, const char *name, const X_ITEM *structure, const char *driver, int open,
                       const char *alias, const char *operation )
{
   X_DBF_SCHEMA        *schema __attribute__( ( cleanup( x_dbfSchemaDrop ) ) ) = NULL;
   X_DBF_TEXT           path __attribute__( ( cleanup( x_dbfTextDrop ) ) ) = { NULL, 0 };
   X_DBF_TEXT           resolved __attribute__( ( cleanup( x_dbfTextDrop ) ) ) = { NULL, 0 };
   X_DBF_SESSION       *session = x_dbfRuntimeSession();
   X_DBF_CREATE_OPTIONS options;
   X_DBF_STATUS         status = x_dbfTextValidateUTF8( name, strlen( name ) );

   if( status == X_DBF_OK )
      status = x_dbfSchema( structure, &schema );
   if( status != X_DBF_OK )
   {
      x_dbfRaise( ctx, status, operation );
      return;
   }
   options.driver = IIF( driver != NULL && driver[0] != '\0', driver, x_dbfDefaultDriver() );
   if( x_dbfDriverFind( session, options.driver ) == NULL )
   {
      x_dbfRaise( ctx, X_DBF_DRIVER_NOT_FOUND, operation );
      return;
   }
   path.data = x_dbfTableName( name );
   options.path = path.data;
   if( open >= 0 )
      resolved.data = IIF( alias != NULL && alias[0] != '\0', x_strdup( alias ), x_dbfAliasName( path.data ) );
   x_dbfNetErrorSet( 0 );
   if( open == 0 && x_dbfCurrent() != NULL )
   {
      status = x_dbfAreaClose( x_dbfCurrent() );
      if( status != X_DBF_OK )
      {
         x_dbfRaise( ctx, status, operation );
         return;
      }
   }
   status = x_dbfTableCreate( session, &options, schema );
   if( status == X_DBF_OK && open >= 0 )
   {
      X_DBF_OPEN_OPTIONS openOptions = {
         path.data, resolved.data, options.driver, IIF( open, 0, x_dbfSessionSelected( session ) ), 0, 0, NULL
      };
      DBF opened;
      status = x_dbfAreaOpen( session, &openOptions, &opened );
   }
   if( x_dbfContention( status ) )
      x_dbfNetErrorSet( 1 );
   else if( status != X_DBF_OK )
   {
      x_dbfRaise( ctx, status, operation );
      return;
   }
   x_retl( ctx, status == X_DBF_OK );
}
