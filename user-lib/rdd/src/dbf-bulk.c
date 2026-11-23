/*
 * Copyright (c) 2024-2026 xDev-Language
 *
 * Library...: xDev 7.0.0 RDD
 * File......: dbf-bulk.c
 *
 */

/*
 * Bulk operations: DbEval(), COUNT, SUM, AVERAGE, COPY, APPEND FROM, SORT, JOIN, TOTAL, UPDATE,
 * SDF, DELIMITED and CSV. Records are read and written directly, without temporary files,
 * except the staging table of a text import.
 */

#include "dbf-internal.h"

#include <xdev/dbinfo.ch>
#include <xdev/dbstruct.ch>
#include <xdev/fileio.ch>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Records chosen by FOR, WHILE, NEXT, RECORD and REST, as in DbEval(); the blocks are held copies */
typedef struct
{
   X_ITEM        forItem;
   X_ITEM        whileItem;
   const X_ITEM *forBlock;
   const X_ITEM *whileBlock;
   int64_t       next;
   int64_t       record;
   int           hasNext;
   int           hasRecord;
   int           rest;
} X_DBF_SCOPE;

/* A field of a bulk operation: its work area and its definition */
typedef struct
{
   uint32_t         number;
   uint64_t         identity;
   size_t           ordinal;
   X_DBF_FIELD_SPEC spec;
} X_DBF_COLUMN;

typedef struct
{
   X_DBF_COLUMN *items;
   size_t        count;
} X_DBF_COLUMNS;

typedef struct
{
   uint64_t *items;
   size_t    count;
   size_t    capacity;
} X_DBF_RECORDS;

/* Key values of records for SORT and UPDATE: width keys for every record */
typedef struct
{
   X_ITEM   *keys;
   uint64_t *records;
   size_t    width;
   size_t    count;
   size_t    capacity;
} X_DBF_ENTRIES;

typedef struct
{
   size_t ordinal;
   int    descending;
   int    caseless;
} X_DBF_SORT_KEY;

typedef struct
{
   const X_DBF_ENTRIES  *entries;
   const X_DBF_SORT_KEY *keys;
   size_t                count;
} X_DBF_SORT;

/* Cargo of s_keysVisit(): the key block of UPDATE or the sort keys of SORT */
typedef struct
{
   X_DBF_ENTRIES        *entries;
   const X_ITEM         *block;
   const X_DBF_SORT_KEY *keys;
   size_t                keyCount;
   const char           *operation;
} X_DBF_KEYS;

/* A table opened by a bulk operation: it is closed and the selection is restored on every exit */
typedef struct
{
   X_DBF_SESSION *session;
   uint32_t       selected;
   uint32_t       number;
   uint64_t       identity;
} X_DBF_OPENED;

typedef struct
{
   char *path;
} X_DBF_TEMPORARY;

typedef struct
{
   char  *data;
   size_t length;
   size_t capacity;
} X_DBF_BUFFER;

typedef struct
{
   X_ITEM       file;
   X_DBF_BUFFER buffer;
   int64_t      offset;
} X_DBF_WRITER;

typedef struct
{
   X_ITEM        file;
   unsigned char data[32768];
   size_t        position;
   size_t        length;
   int64_t       offset;
   int           ended;
} X_DBF_READER;

typedef struct
{
   const X_ITEM *block;
   X_ITEM        sum;
   uint64_t      count;
   const char   *operation;
} X_DBF_TALLY;

/* State of DbTotal(): the open group and the key of the previous record */
typedef struct
{
   X_DBF_SESSION       *session;
   const X_DBF_COLUMNS *columns;
   const X_DBF_OPENED  *output;
   const X_ITEM        *key;
   const X_ITEM        *forBlock;
   size_t              *sums;
   size_t               sumCount;
   X_ITEM               previous;
   X_ITEM              *group;
   size_t               groupCount;
   int                  hasPrevious;
   int                  hasGroup;
   int                  deleted;
} X_DBF_TOTAL;

typedef void ( *X_DBF_VISIT )( X_CALL_CTX *ctx, DBF area, void *cargo );

static const char *const s_tallyNames[] = { "DBCOUNT", "DBSUM", "DBAVERAGE" };

/* Structure of the table made by DbCopyXStruct() and read by DbCreateFrom() */
static const X_DBF_FIELD_SPEC s_extended[] = { { "FIELD_NAME", X_DBF_CHARACTER, 32, 0 },
                                               { "FIELD_TYPE", X_DBF_CHARACTER, 1, 0 },
                                               { "FIELD_LEN", X_DBF_NUMERIC, 3, 0 },
                                               { "FIELD_DEC", X_DBF_NUMERIC, 3, 0 } };

/* ---------------------------------------------------------------------------------------------
   Helpers
   --------------------------------------------------------------------------------------------- */

static void s_fail( X_CALL_CTX *ctx, const char *description, const char *operation )
{
   X_ERROR_DEF error = { 2, 13, 6100, 0, "XDEVDBF", description, operation };
   x_errorRaiseDefinition( ctx->result, &error, NULL, NULL );
}

static DBF s_area( const X_DBF_SESSION *session, uint32_t number, uint64_t identity )
{
   DBF area = x_dbfSessionArea( session, number );
   return IIF( area != NULL && area->identity == identity, area, NULL );
}

static int s_goTo( X_CALL_CTX *ctx, const X_DBF_SESSION *session, uint32_t number, uint64_t identity, uint64_t record,
                   const char *operation )
{
   DBF          area = s_area( session, number, identity );
   X_DBF_STATUS status = IIF( area == NULL, X_DBF_INVALID_STATE, x_dbfAreaGoTo( area, record ) );

   if( status != X_DBF_OK )
   {
      x_dbfRaise( ctx, status, operation );
      return 0;
   }
   return 1;
}

static const char *s_driver( X_CALL_CTX *ctx, int argument )
{
   return IIF( x_parclen( ctx, argument ) != 0, x_parc( ctx, argument ), x_dbfDefaultDriver() );
}

static int s_upperByte( int character )
{
   return IIF( character >= 'a' && character <= 'z', character - 'a' + 'A', character );
}

/* Upper-case copy of a name without the surrounding spaces */
static char *s_name( const char *text, size_t length )
{
   char  *name;
   size_t i;

   while( length != 0 && *text == ' ' )
   {
      ++text;
      --length;
   }
   while( length != 0 && text[length - 1] == ' ' )
      --length;
   name = ( char * )x_xgrab( length + 1 );
   for( i = 0; i < length; ++i )
      name[i] = ( char )s_upperByte( ( unsigned char )text[i] );
   name[length] = '\0';
   return name;
}

static void s_trim( const char **text, size_t *length )
{
   while( *length != 0 && **text == ' ' )
   {
      ++*text;
      --*length;
   }
   while( *length != 0 && ( *text )[*length - 1] == ' ' )
      --*length;
}

/* The name with the extension added when its last part has no dot */
static char *s_fileName( const char *name, const char *extension )
{
   const char *base = name, *position;
   size_t      length = strlen( name );
   char       *result = ( char * )x_xgrab( length + strlen( extension ) + 1 );

   for( position = name; *position != '\0'; ++position )
   {
      if( *position == '/' || *position == '\\' || *position == ':' )
         base = position + 1;
   }
   memcpy( result, name, length + 1 );
   if( strchr( base, '.' ) == NULL )
      strcat( result, extension );
   return result;
}

static void s_bufferDrop( X_DBF_BUFFER *buffer )
{
   if( buffer->data != NULL )
      x_xfree( buffer->data );
}

/* Appends bytes; the buffer always holds a terminating zero */
static void s_bufferPut( X_DBF_BUFFER *buffer, const char *data, size_t length )
{
   if( buffer->length + length + 1 > buffer->capacity )
   {
      size_t capacity = IIF( buffer->capacity == 0, 256, buffer->capacity );
      while( capacity < buffer->length + length + 1 )
         capacity *= 2;
      buffer->data = ( char * )x_xrealloc( buffer->data, capacity );
      buffer->capacity = capacity;
   }
   if( length != 0 )
      memcpy( buffer->data + buffer->length, data, length );
   buffer->length += length;
   buffer->data[buffer->length] = '\0';
}

static void s_bufferClear( X_DBF_BUFFER *buffer )
{
   buffer->length = 0;
   s_bufferPut( buffer, "", 0 );
}

static void s_recordsDrop( X_DBF_RECORDS *records )
{
   if( records->items != NULL )
      x_xfree( records->items );
}

static void s_recordsAdd( X_DBF_RECORDS *records, uint64_t record )
{
   if( records->count == records->capacity )
   {
      records->capacity = IIF( records->capacity == 0, 256, records->capacity * 2 );
      records->items = ( uint64_t * )x_xrealloc( records->items, records->capacity * sizeof( *records->items ) );
   }
   records->items[records->count++] = record;
}

static int s_recordOrder( const void *left, const void *right )
{
   uint64_t a = *( const uint64_t * )left, b = *( const uint64_t * )right;
   return ( a > b ) - ( a < b );
}

static void s_entriesDrop( X_DBF_ENTRIES *entries )
{
   size_t i;

   for( i = 0; i < entries->count * entries->width; ++i )
      x_itemClear( &entries->keys[i] );
   if( entries->keys != NULL )
      x_xfree( entries->keys );
   if( entries->records != NULL )
      x_xfree( entries->records );
}

/* Adds a record and returns its width key slots, set to NIL */
static X_ITEM *s_entriesAdd( X_DBF_ENTRIES *entries, uint64_t record )
{
   if( entries->count == entries->capacity )
   {
      size_t capacity = IIF( entries->capacity == 0, 256, entries->capacity * 2 );
      entries->keys = ( X_ITEM * )x_xrealloc( entries->keys, capacity * entries->width * sizeof( X_ITEM ) );
      entries->records = ( uint64_t * )x_xrealloc( entries->records, capacity * sizeof( uint64_t ) );
      memset( entries->keys + entries->capacity * entries->width, 0,
              ( capacity - entries->capacity ) * entries->width * sizeof( X_ITEM ) );
      entries->capacity = capacity;
   }
   entries->records[entries->count] = record;
   return &entries->keys[entries->count++ * entries->width];
}

/* Kind of a key value: 1 text, 2 number, 3 date, 4 logical, 5 NIL, 0 any other */
static int s_kind( const X_ITEM *item )
{
   if( item->nType == X_IT_STRING )
      return 1;
   if( X_IS_NUMERIC( item ) )
      return 2;
   if( item->nType == X_IT_DATE )
      return 3;
   if( item->nType == X_IT_LOGICAL )
      return 4;
   return IIF( item->nType == X_IT_NIL, 5, 0 );
}

/* Order of two values of the same kind: text by bytes, numbers, dates, .F. before .T., NIL equal.
   Returns 0 when the values cannot be compared */
static int s_compare( const X_ITEM *left, const X_ITEM *right, int *order )
{
   int kind = s_kind( left );

   *order = 0;
   if( kind == 0 || kind != s_kind( right ) )
      return 0;
   switch( kind )
   {
      case 1:
      {
         size_t length = IIF( left->u.string.len < right->u.string.len, left->u.string.len, right->u.string.len );
         *order = IIF( length != 0, memcmp( left->u.string.data, right->u.string.data, length ), 0 );
         if( *order == 0 )
            *order = ( left->u.string.len > right->u.string.len ) - ( left->u.string.len < right->u.string.len );
         else
            *order = IIF( *order < 0, -1, 1 );
         break;
      }
      case 2:
         if( left->nType == X_IT_INTEGER && right->nType == X_IT_INTEGER )
            *order = ( left->u.integer > right->u.integer ) - ( left->u.integer < right->u.integer );
         else
         {
            double a = x_itemGetND( left ), b = x_itemGetND( right );
            *order = ( a > b ) - ( a < b );
         }
         break;
      case 3: *order = ( left->u.integer > right->u.integer ) - ( left->u.integer < right->u.integer ); break;
      case 4: *order = ( left->u.logical != 0 ) - ( right->u.logical != 0 ); break;
   }
   return 1;
}

static int s_order( X_CALL_CTX *ctx, const X_ITEM *left, const X_ITEM *right, int *order, const char *operation )
{
   if( !s_compare( left, right, order ) )
   {
      x_dbfRaise( ctx, X_DBF_INVALID_FIELD_VALUE, operation );
      return 0;
   }
   return 1;
}

/* Adds a number to a sum: integers while they fit, otherwise a double */
static int s_add( X_ITEM *sum, const X_ITEM *value )
{
   int64_t result;

   if( !X_IS_NUMERIC( sum ) || !X_IS_NUMERIC( value ) )
      return 0;
   if( sum->nType == X_IT_INTEGER && value->nType == X_IT_INTEGER &&
       !__builtin_add_overflow( sum->u.integer, value->u.integer, &result ) )
      sum->u.integer = result;
   else
      x_itemPutND( sum, x_itemGetND( sum ) + x_itemGetND( value ) );
   return 1;
}

/* Upper case of the ASCII letters of a text value */
static void s_upper( X_ITEM *item )
{
   size_t length = item->u.string.len, i;
   char  *text = ( char * )x_xgrab( length + 1 );

   for( i = 0; i < length; ++i )
      text[i] = ( char )s_upperByte( ( unsigned char )item->u.string.data[i] );
   text[length] = '\0';
   x_itemClear( item );
   x_itemPutStringBuffer( item, text, length );
}

/* ---------------------------------------------------------------------------------------------
   Scope
   --------------------------------------------------------------------------------------------- */

static void s_scopeDrop( X_DBF_SCOPE *scope )
{
   x_itemClear( &scope->forItem );
   x_itemClear( &scope->whileItem );
}

/* A copy of a block argument, so a block that changes the argument cannot free it */
static const X_ITEM *s_hold( X_CALL_CTX *ctx, int argument, X_ITEM *held )
{
   if( !X_ISBLOCK( ctx, argument ) )
      return NULL;
   x_itemCopy( held, x_paritem( ctx, argument ) );
   return held;
}

/* FOR, WHILE, NEXT, RECORD and REST from five arguments starting at first */
static void s_scopeRead( X_CALL_CTX *ctx, int first, X_DBF_SCOPE *scope )
{
   s_scopeDrop( scope );
   memset( scope, 0, sizeof( *scope ) );
   scope->forBlock = s_hold( ctx, first, &scope->forItem );
   scope->whileBlock = s_hold( ctx, first + 1, &scope->whileItem );
   scope->hasNext = !x_dbfOptional( ctx, first + 2 );
   scope->hasRecord = !x_dbfOptional( ctx, first + 3 );
   if( scope->hasNext )
      x_dbfInteger( x_paritem( ctx, first + 2 ), &scope->next );
   if( scope->hasRecord )
      x_dbfInteger( x_paritem( ctx, first + 3 ), &scope->record );
   scope->rest = x_parl( ctx, first + 4 );
}

/* Visits the records of the scope like DbEval(); the area is looked up again after every block,
   because a block may close it. Returns 1 when the area is still open */
static int s_scan( X_CALL_CTX *ctx, DBF area, const X_DBF_SCOPE *scope, X_DBF_VISIT visit, void *cargo,
                   const char *operation )
{
   X_ITEM               value __attribute__( ( cleanup( x_itemClear ) ) ) = X_ITEM_NIL_INIT;
   const X_DBF_SESSION *session = area->session;
   uint32_t             number = area->number;
   uint64_t             identity = area->identity;
   int64_t              remaining = scope->next;
   X_DBF_STATUS         status;

   if( scope->hasRecord )
      status = x_dbfAreaGoTo( area, IIF( scope->record > 0, ( uint64_t )scope->record, 0 ) );
   else if( scope->hasNext && remaining <= 0 )
      return 1;
   else if( !scope->hasNext && scope->whileBlock == NULL && !scope->rest )
      status = x_dbfAreaGoTop( area, x_setDeleted );
   else
      status = x_dbfAreaPrepare( area );
   if( status != X_DBF_OK )
   {
      x_dbfRaise( ctx, status, operation );
      return 0;
   }
   while( !area->state.eof )
   {
      if( scope->whileBlock != NULL )
      {
         area = x_dbfEvalAreaBlock( ctx, number, identity, scope->whileBlock, &value );
         if( area == NULL )
            return 0;
         if( value.nType != X_IT_LOGICAL || !value.u.logical )
            break;
      }
      if( scope->forBlock != NULL )
      {
         area = x_dbfEvalAreaBlock( ctx, number, identity, scope->forBlock, &value );
         if( area == NULL )
            return 0;
      }
      if( scope->forBlock == NULL || ( value.nType == X_IT_LOGICAL && value.u.logical ) )
      {
         visit( ctx, area, cargo );
         area = s_area( session, number, identity );
         if( area == NULL )
         {
            x_dbfRaise( ctx, X_DBF_INVALID_STATE, operation );
            return 0;
         }
      }
      if( scope->hasRecord || ( scope->hasNext && --remaining == 0 ) )
         break;
      status = x_dbfAreaSkip( area, 1, x_setDeleted );
      if( status != X_DBF_OK )
      {
         x_dbfRaise( ctx, status, operation );
         return 0;
      }
   }
   return 1;
}

static void s_recordVisit( X_CALL_CTX *ctx, DBF area, void *cargo )
{
   UNUSED( ctx );
   s_recordsAdd( ( X_DBF_RECORDS * )cargo, area->state.record );
}

static void s_evalVisit( X_CALL_CTX *ctx, DBF area, void *cargo )
{
   X_ITEM value __attribute__( ( cleanup( x_itemClear ) ) ) = X_ITEM_NIL_INIT;

   x_dbfEvalAreaBlock( ctx, area->number, area->identity, ( const X_ITEM * )cargo, &value );
}

static void s_tallyDrop( X_DBF_TALLY *tally )
{
   x_itemClear( &tally->sum );
}

static void s_tallyVisit( X_CALL_CTX *ctx, DBF area, void *cargo )
{
   X_DBF_TALLY *tally = ( X_DBF_TALLY * )cargo;
   X_ITEM       value __attribute__( ( cleanup( x_itemClear ) ) ) = X_ITEM_NIL_INIT;

   if( tally->block != NULL )
   {
      if( x_dbfEvalAreaBlock( ctx, area->number, area->identity, tally->block, &value ) == NULL )
         return;
      if( !s_add( &tally->sum, &value ) )
      {
         x_dbfRaise( ctx, X_DBF_INVALID_ARGUMENT, tally->operation );
         return;
      }
   }
   ++tally->count;
}

static void s_keysVisit( X_CALL_CTX *ctx, DBF area, void *cargo )
{
   X_DBF_KEYS  *keys = ( X_DBF_KEYS * )cargo;
   X_ITEM      *slots = s_entriesAdd( keys->entries, area->state.record );
   X_DBF_STATUS status;
   size_t       i;

   if( keys->block != NULL )
   {
      x_dbfEvalAreaBlock( ctx, area->number, area->identity, keys->block, slots );
      return;
   }
   for( i = 0; i < keys->keyCount; ++i )
   {
      status = x_dbfFieldGetValue( area, keys->keys[i].ordinal, &slots[i] );
      if( status != X_DBF_OK )
      {
         x_dbfRaise( ctx, status, keys->operation );
         return;
      }
      if( keys->keys[i].caseless && slots[i].nType == X_IT_STRING )
         s_upper( &slots[i] );
   }
}

/* ---------------------------------------------------------------------------------------------
   Fields
   --------------------------------------------------------------------------------------------- */

static void s_columnsDrop( X_DBF_COLUMNS *columns )
{
   if( columns->items != NULL )
      x_xfree( columns->items );
}

static int s_isMemo( const X_DBF_FIELD *field )
{
   return field->type == X_DBF_MEMO || field->type == X_DBF_PICTURE;
}

static void s_columnAdd( X_DBF_COLUMNS *columns, DBF area, size_t ordinal )
{
   const X_DBF_FIELD *field = &area->schema->fields[ordinal - 1];
   X_DBF_COLUMN      *column = &columns->items[columns->count++];

   column->number = area->number;
   column->identity = area->identity;
   column->ordinal = ordinal;
   column->spec.name = field->name;
   column->spec.type = field->type;
   /* New xDev 7 tables use a 10-digit memo pointer; a 4-byte FPT pointer does not fit */
   column->spec.length = ( uint16_t )IIF( s_isMemo( field ) && field->length == 4, 10, field->length );
   column->spec.decimals = field->decimals;
}

/* Fields of the operation: the names given (also ALIAS->FIELD) or every field of the area;
   skipMemo leaves out M and P fields */
static int s_columns( X_CALL_CTX *ctx, DBF area, const X_ITEM *names, int skipMemo, X_DBF_COLUMNS *columns,
                      const char *operation )
{
   size_t count = IIF( names != NULL && names->nType == X_IT_ARRAY, ( size_t )x_arrayLen( names ), 0 ), i;
   int    all = count == 0;

   if( all )
      count = area->schema->count;
   columns->items = ( X_DBF_COLUMN * )x_xgrab( ( count + 1 ) * sizeof( *columns->items ) );
   columns->count = 0;
   for( i = 0; i < count; ++i )
   {
      DBF    source = area;
      size_t ordinal = i + 1;

      if( !all )
      {
         const X_ITEM *item = x_arrayGetItemPtr( names, i + 1 );
         X_DBF_TEXT    name __attribute__( ( cleanup( x_dbfTextDrop ) ) ) = { NULL, 0 };
         char         *field, *arrow;

         if( !x_dbfString( item ) )
         {
            x_dbfRaise( ctx, X_DBF_INVALID_ARGUMENT, operation );
            return 0;
         }
         name.data = s_name( item->u.string.data, item->u.string.len );
         field = name.data;
         arrow = strstr( name.data, "->" );
         if( arrow != NULL )
         {
            *arrow = '\0';
            field = arrow + 2;
            source = x_dbfSessionAlias( area->session, name.data );
            if( source == NULL )
            {
               s_fail( ctx, "Unknown field work area", name.data );
               return 0;
            }
         }
         ordinal = x_dbfSchemaFindField( source->schema, field );
         if( ordinal == 0 )
         {
            s_fail( ctx, "Unknown field", field );
            return 0;
         }
      }
      if( !skipMemo || !s_isMemo( &source->schema->fields[ordinal - 1] ) )
         s_columnAdd( columns, source, ordinal );
   }
   return 1;
}

static int s_value( X_CALL_CTX *ctx, const X_DBF_SESSION *session, const X_DBF_COLUMN *column, X_ITEM *value,
                    const char *operation )
{
   DBF          area = s_area( session, column->number, column->identity );
   X_DBF_STATUS status;

   x_itemClear( value );
   status = IIF( area == NULL, X_DBF_INVALID_STATE, x_dbfFieldGetValue( area, column->ordinal, value ) );
   if( status != X_DBF_OK )
   {
      x_dbfRaise( ctx, status, operation );
      return 0;
   }
   return 1;
}

/* Appends a record like DbAppend(): a shared table releases its record locks first */
static int s_append( X_CALL_CTX *ctx, DBF target, const char *operation )
{
   X_DBF_STATUS status = IIF( target->shared, x_dbfAreaUnlock( target, 0 ), X_DBF_OK );

   if( status == X_DBF_OK )
      status = x_dbfAreaAppend( target );
   if( status != X_DBF_OK )
   {
      x_dbfRaise( ctx, status, operation );
      return 0;
   }
   return 1;
}

static int s_put( X_CALL_CTX *ctx, DBF target, size_t ordinal, const X_ITEM *value, const char *operation )
{
   X_DBF_STATUS status = x_dbfFieldPutValue( target, ordinal, value );

   if( status != X_DBF_OK )
   {
      x_dbfRaise( ctx, status, operation );
      return 0;
   }
   return 1;
}

static int s_delete( X_CALL_CTX *ctx, DBF target, int deleted, const char *operation )
{
   X_DBF_STATUS status = IIF( deleted, x_dbfAreaDelete( target, 1 ), X_DBF_OK );

   if( status != X_DBF_OK )
   {
      x_dbfRaise( ctx, status, operation );
      return 0;
   }
   return 1;
}

/* Appends a record to target with the values of the columns and the deleted mark */
static int s_copyRow( X_CALL_CTX *ctx, const X_DBF_COLUMNS *columns, DBF target, int deleted, const char *operation )
{
   X_ITEM value __attribute__( ( cleanup( x_itemClear ) ) ) = X_ITEM_NIL_INIT;
   size_t i;

   if( !s_append( ctx, target, operation ) )
      return 0;
   for( i = 0; i < columns->count; ++i )
   {
      if( !s_value( ctx, target->session, &columns->items[i], &value, operation ) ||
          !s_put( ctx, target, i + 1, &value, operation ) )
         return 0;
   }
   return s_delete( ctx, target, deleted, operation );
}

/* ---------------------------------------------------------------------------------------------
   Tables opened by an operation
   --------------------------------------------------------------------------------------------- */

static X_DBF_OPENED s_opened( void )
{
   X_DBF_SESSION *session = x_dbfRuntimeSession();
   X_DBF_OPENED   opened = { session, x_dbfSessionSelected( session ), 0, 0 };

   return opened;
}

static void s_openedDrop( X_DBF_OPENED *opened )
{
   if( opened->number != 0 )
   {
      DBF area = s_area( opened->session, opened->number, opened->identity );
      if( area != NULL && x_dbfAreaClose( area ) == X_DBF_INVALID_STATE )
         x_dbfAreaDiscard( area );
      opened->number = 0;
   }
   opened->session->current = opened->selected;
}

/* The alias from the file name when it is free, otherwise XDEV_BULK_n */
static void s_alias( X_DBF_SESSION *session, const char *path, char *alias, size_t size )
{
   X_DBF_TEXT   derived __attribute__( ( cleanup( x_dbfTextDrop ) ) ) = { x_dbfAliasName( path ), 0 };
   unsigned int i;

   if( strlen( derived.data ) < size && x_dbfSessionAlias( session, derived.data ) == NULL )
   {
      strcpy( alias, derived.data );
      return;
   }
   for( i = 1;; ++i )
   {
      snprintf( alias, size, "XDEV_BULK_%u", i );
      if( x_dbfSessionAlias( session, alias ) == NULL )
         return;
   }
}

/* Opens a table in a free work area: a source shared and read-only, an output exclusive.
   The selected work area does not change */
static DBF s_open( X_CALL_CTX *ctx, X_DBF_OPENED *opened, const char *path, const char *driver, int source,
                   const char *operation )
{
   X_DBF_OPEN_OPTIONS options;
   char               alias[64];
   DBF                area = NULL;
   X_DBF_STATUS       status = x_dbfTextValidateUTF8( path, strlen( path ) );

   if( status == X_DBF_OK )
   {
      s_alias( opened->session, path, alias, sizeof( alias ) );
      memset( &options, 0, sizeof( options ) );
      options.path = path;
      options.alias = alias;
      options.driver = driver;
      options.readOnly = source;
      options.shared = source;
      status = x_dbfAreaOpen( opened->session, &options, &area );
      opened->session->current = opened->selected;
   }
   if( status != X_DBF_OK )
   {
      x_dbfRaise( ctx, status, operation );
      return NULL;
   }
   opened->number = area->number;
   opened->identity = area->identity;
   return area;
}

/* Creates a table with the columns and opens it as the output of the operation */
static DBF s_create( X_CALL_CTX *ctx, X_DBF_OPENED *opened, const char *name, const X_DBF_COLUMNS *columns,
                     const char *driver, const char *operation )
{
   X_DBF_SCHEMA        *schema __attribute__( ( cleanup( x_dbfSchemaDrop ) ) ) = NULL;
   X_DBF_TEXT           specifications __attribute__( ( cleanup( x_dbfTextDrop ) ) ) = { NULL, 0 };
   X_DBF_TEXT           path __attribute__( ( cleanup( x_dbfTextDrop ) ) ) = { NULL, 0 };
   X_DBF_CREATE_OPTIONS options;
   X_DBF_FIELD_SPEC    *fields;
   X_DBF_STATUS         status = x_dbfTextValidateUTF8( name, strlen( name ) );
   size_t               i;

   if( status == X_DBF_OK )
   {
      specifications.data = ( char * )x_xgrab( ( columns->count + 1 ) * sizeof( X_DBF_FIELD_SPEC ) );
      fields = ( X_DBF_FIELD_SPEC * )specifications.data;
      for( i = 0; i < columns->count; ++i )
         fields[i] = columns->items[i].spec;
      status = x_dbfSchemaCreate( fields, columns->count, &schema );
   }
   if( status == X_DBF_OK )
   {
      path.data = x_dbfTableName( name );
      options.path = path.data;
      options.driver = driver;
      status = x_dbfTableCreate( opened->session, &options, schema );
   }
   if( status != X_DBF_OK )
   {
      x_dbfRaise( ctx, status, operation );
      return NULL;
   }
   return s_open( ctx, opened, path.data, driver, 0, operation );
}

/* Closes the table; closing writes it to disk */
static int s_close( X_CALL_CTX *ctx, X_DBF_OPENED *opened, const char *operation )
{
   DBF          area = s_area( opened->session, opened->number, opened->identity );
   X_DBF_STATUS status = IIF( area == NULL, X_DBF_INVALID_STATE, x_dbfAreaClose( area ) );

   opened->number = 0;
   if( status != X_DBF_OK )
   {
      x_dbfRaise( ctx, status, operation );
      return 0;
   }
   return 1;
}

static void s_temporaryDrop( X_DBF_TEMPORARY *temporary )
{
   if( temporary->path != NULL )
   {
      int error = x_fsError();
      x_fsDelete( temporary->path );
      x_xfree( temporary->path );
      x_fsSetError( error );
   }
}

/* Name of a temporary table; the file is removed by s_temporaryDrop() */
static int s_temporary( X_CALL_CTX *ctx, X_DBF_TEMPORARY *temporary, const char *operation )
{
   X_ITEM       file __attribute__( ( cleanup( x_itemClear ) ) ) = X_ITEM_NIL_INIT;
   void        *handle = x_fileTemp( &temporary->path, "", "xdev-dbf-bulk-", ".dbf", FC_NORMAL );
   X_DBF_STATUS status = X_DBF_OK;

   if( handle == NULL )
      status = X_DBF_CREATE_ERROR;
   else
   {
      x_itemPutPtrGC( &file, handle );
      if( !x_fileClose( &file ) )
         status = X_DBF_CLOSE_ERROR;
   }
   if( status != X_DBF_OK )
   {
      x_dbfRaise( ctx, status, operation );
      return 0;
   }
   return 1;
}

/* Writes the records, in their order, to a new table with the columns; the record pointer returns */
static int s_copyTo( X_CALL_CTX *ctx, DBF area, const X_DBF_COLUMNS *columns, const X_DBF_RECORDS *records,
                     uint64_t position, const char *name, const char *driver, const char *operation )
{
   X_DBF_OPENED         output __attribute__( ( cleanup( s_openedDrop ) ) ) = s_opened();
   const X_DBF_SESSION *session = area->session;
   uint32_t             number = area->number;
   uint64_t             identity = area->identity;
   DBF                  target;
   size_t               i;

   if( s_create( ctx, &output, name, columns, driver, operation ) == NULL )
      return 0;
   for( i = 0; i < records->count; ++i )
   {
      if( !s_goTo( ctx, session, number, identity, records->items[i], operation ) )
         return 0;
      area = s_area( session, number, identity );
      target = s_area( session, output.number, output.identity );
      if( target == NULL )
      {
         x_dbfRaise( ctx, X_DBF_INVALID_STATE, operation );
         return 0;
      }
      if( !s_copyRow( ctx, columns, target, area->state.deleted, operation ) )
         return 0;
   }
   if( !s_close( ctx, &output, operation ) )
      return 0;
   return s_goTo( ctx, session, number, identity, position, operation );
}

/* Appends the records of the scope of source to target; fields are matched by name */
static int s_appendFrom( X_CALL_CTX *ctx, DBF target, DBF source, const X_ITEM *names, const X_DBF_SCOPE *scope,
                         const char *operation )
{
   X_DBF_COLUMNS        columns __attribute__( ( cleanup( s_columnsDrop ) ) ) = { NULL, 0 };
   X_DBF_RECORDS        records __attribute__( ( cleanup( s_recordsDrop ) ) ) = { NULL, 0, 0 };
   X_DBF_TEXT           pairs __attribute__( ( cleanup( x_dbfTextDrop ) ) ) = { NULL, 0 };
   X_ITEM               value __attribute__( ( cleanup( x_itemClear ) ) ) = X_ITEM_NIL_INIT;
   const X_DBF_SESSION *session = target->session;
   uint32_t             targetNumber = target->number, sourceNumber = source->number;
   uint64_t             targetIdentity = target->identity, sourceIdentity = source->identity;
   size_t              *ordinals, count = 0, i, j;
   X_DBF_STATUS         status;

   if( !s_columns( ctx, target, names, 0, &columns, operation ) )
      return 0;
   pairs.data = ( char * )x_xgrab( ( columns.count * 2 + 1 ) * sizeof( size_t ) );
   ordinals = ( size_t * )pairs.data;
   for( i = 0; i < columns.count; ++i )
   {
      size_t found = IIF( columns.items[i].number == targetNumber,
                          x_dbfSchemaFindField( source->schema, columns.items[i].spec.name ), 0 );
      if( found != 0 )
      {
         ordinals[count * 2] = found;
         ordinals[count * 2 + 1] = columns.items[i].ordinal;
         ++count;
      }
   }
   if( count == 0 )
   {
      s_fail( ctx, "APPEND FROM has no matching fields", operation );
      return 0;
   }
   if( !s_scan( ctx, source, scope, s_recordVisit, &records, operation ) )
      return 0;
   for( i = 0; i < records.count; ++i )
   {
      if( !s_goTo( ctx, session, sourceNumber, sourceIdentity, records.items[i], operation ) )
         return 0;
      source = s_area( session, sourceNumber, sourceIdentity );
      target = s_area( session, targetNumber, targetIdentity );
      if( target == NULL )
      {
         x_dbfRaise( ctx, X_DBF_INVALID_STATE, operation );
         return 0;
      }
      if( !s_append( ctx, target, operation ) )
         return 0;
      for( j = 0; j < count; ++j )
      {
         x_itemClear( &value );
         status = x_dbfFieldGetValue( source, ordinals[j * 2], &value );
         if( status != X_DBF_OK )
         {
            x_dbfRaise( ctx, status, operation );
            return 0;
         }
         if( !s_put( ctx, target, ordinals[j * 2 + 1], &value, operation ) )
            return 0;
      }
      if( !s_delete( ctx, target, source->state.deleted, operation ) )
         return 0;
   }
   target = s_area( session, targetNumber, targetIdentity );
   status = IIF( target == NULL, X_DBF_INVALID_STATE, x_dbfAreaCommit( target ) );
   if( status != X_DBF_OK )
   {
      x_dbfRaise( ctx, status, operation );
      return 0;
   }
   return 1;
}

/* ---------------------------------------------------------------------------------------------
   Text files
   --------------------------------------------------------------------------------------------- */

static void s_writerDrop( X_DBF_WRITER *writer )
{
   if( writer->file.nType != X_IT_NIL )
   {
      int error = x_fsError();
      x_fileClose( &writer->file );
      x_gcUnlock( writer->file.u.pPointer );
      x_itemClear( &writer->file );
      x_fsSetError( error );
   }
   s_bufferDrop( &writer->buffer );
}

static int s_writerOpen( X_CALL_CTX *ctx, X_DBF_WRITER *writer, const char *path, const char *operation )
{
   void        *handle = NULL;
   X_DBF_STATUS status = x_dbfTextValidateUTF8( path, strlen( path ) );

   if( status == X_DBF_OK )
   {
      handle = x_fileCreate( path, FC_NORMAL );
      if( handle == NULL )
         status = X_DBF_CREATE_ERROR;
   }
   if( status != X_DBF_OK )
   {
      x_dbfRaise( ctx, status, operation );
      return 0;
   }
   x_itemPutPtrGC( &writer->file, handle );
   x_gcLock( handle );
   return 1;
}

static X_DBF_STATUS s_writerFlush( X_DBF_WRITER *writer )
{
   size_t done = 0;

   while( done < writer->buffer.length )
   {
      int64_t bytes =
         x_fileWriteAt( &writer->file, writer->buffer.data + done, writer->buffer.length - done, writer->offset );
      if( bytes <= 0 )
         return X_DBF_WRITE_ERROR;
      done += ( size_t )bytes;
      writer->offset += bytes;
   }
   writer->buffer.length = 0;
   return X_DBF_OK;
}

static int s_writerPut( X_CALL_CTX *ctx, X_DBF_WRITER *writer, const X_DBF_BUFFER *line, const char *operation )
{
   X_DBF_STATUS status = X_DBF_OK;

   s_bufferPut( &writer->buffer, line->data, line->length );
   if( writer->buffer.length >= 65536 )
      status = s_writerFlush( writer );
   if( status != X_DBF_OK )
   {
      x_dbfRaise( ctx, status, operation );
      return 0;
   }
   return 1;
}

static int s_writerClose( X_CALL_CTX *ctx, X_DBF_WRITER *writer, const char *operation )
{
   X_DBF_STATUS status = s_writerFlush( writer );
   int          closed = x_fileClose( &writer->file );

   x_gcUnlock( writer->file.u.pPointer );
   x_itemClear( &writer->file );
   if( status == X_DBF_OK && !closed )
      status = X_DBF_CLOSE_ERROR;
   if( status != X_DBF_OK )
   {
      x_dbfRaise( ctx, status, operation );
      return 0;
   }
   return 1;
}

static void s_readerDrop( X_DBF_READER *reader )
{
   if( reader->file.nType != X_IT_NIL )
   {
      int error = x_fsError();
      x_fileClose( &reader->file );
      x_gcUnlock( reader->file.u.pPointer );
      x_itemClear( &reader->file );
      x_fsSetError( error );
   }
}

static int s_readerOpen( X_CALL_CTX *ctx, X_DBF_READER *reader, const char *path, const char *operation )
{
   void        *handle = NULL;
   X_DBF_STATUS status = x_dbfTextValidateUTF8( path, strlen( path ) );

   if( status == X_DBF_OK )
   {
      handle = x_fileOpen( path, FO_READ | FO_DENYNONE );
      if( handle == NULL )
         status = X_DBF_OPEN_ERROR;
   }
   if( status != X_DBF_OK )
   {
      x_dbfRaise( ctx, status, operation );
      return 0;
   }
   x_itemPutPtrGC( &reader->file, handle );
   x_gcLock( handle );
   return 1;
}

/* Next byte of the file, or -1 at its end */
static X_DBF_STATUS s_readByte( X_DBF_READER *reader, int *byte )
{
   if( reader->position == reader->length )
   {
      int64_t bytes =
         IIF( reader->ended, 0, x_fileReadAt( &reader->file, reader->data, sizeof( reader->data ), reader->offset ) );
      if( bytes < 0 )
         return X_DBF_READ_ERROR;
      reader->position = 0;
      reader->length = ( size_t )bytes;
      reader->offset += bytes;
      if( bytes == 0 )
      {
         *byte = -1;
         return X_DBF_OK;
      }
   }
   *byte = reader->data[reader->position++];
   return X_DBF_OK;
}

static void s_readerSeek( X_DBF_READER *reader, int64_t offset )
{
   reader->offset = offset;
   reader->position = reader->length = 0;
   reader->ended = 0;
}

/* A text line without its line end; the byte 0x1A ends the file. found is 0 at the end of the file */
static X_DBF_STATUS s_readLine( X_DBF_READER *reader, X_DBF_BUFFER *line, int *found )
{
   X_DBF_STATUS status;
   int          byte;
   char         character;

   s_bufferClear( line );
   *found = 0;
   for( ;; )
   {
      status = s_readByte( reader, &byte );
      if( status != X_DBF_OK )
         return status;
      if( byte < 0 )
         break;
      if( byte == 0x1A )
      {
         reader->ended = 1;
         reader->position = reader->length;
         break;
      }
      *found = 1;
      if( byte == '\n' )
         break;
      character = ( char )byte;
      s_bufferPut( line, &character, 1 );
   }
   if( line->length != 0 && line->data[line->length - 1] == '\r' )
      line->data[--line->length] = '\0';
   return X_DBF_OK;
}

/* A CSV row (RFC 4180): a quoted field may hold the separator, doubled quotes and line breaks.
   The fields are in data, each ending at the offset in ends; found is 0 at the end of the file */
static X_DBF_STATUS s_csvRow( X_DBF_READER *reader, int separator, X_DBF_BUFFER *data, X_DBF_RECORDS *ends, int *found )
{
   X_DBF_STATUS status;
   int          state = 0, byte, append, end, finished = 0;
   char         character;

   s_bufferClear( data );
   ends->count = 0;
   *found = 0;
   while( !finished )
   {
      status = s_readByte( reader, &byte );
      if( status != X_DBF_OK )
         return status;
      if( byte < 0 )
      {
         if( !*found )
            return X_DBF_OK;
         if( state == 2 )
            return X_DBF_CORRUPT_FILE;
         break;
      }
      *found = 1;
      append = end = 0;
      /* States: 0 field start, 1 plain field, 2 inside quotes, 3 quote inside quotes */
      if( state == 3 )
      {
         if( byte == '"' )
         {
            state = 2;
            append = 1;
         }
         else
            state = 1;
      }
      if( !append )
      {
         if( state == 2 )
         {
            if( byte == '"' )
               state = 3;
            else
               append = 1;
         }
         else if( state == 0 && byte == '"' )
            state = 2;
         else if( byte == separator )
            end = 1;
         else if( byte == '\n' )
            end = finished = 1;
         else if( byte != '\r' )
         {
            state = 1;
            append = 1;
         }
      }
      if( append )
      {
         character = ( char )byte;
         s_bufferPut( data, &character, 1 );
      }
      if( end )
      {
         s_recordsAdd( ends, data->length );
         state = 0;
      }
   }
   if( !finished )
      s_recordsAdd( ends, data->length );
   return X_DBF_OK;
}

static void s_dateText( const X_ITEM *value, char *text )
{
   int year, month, day;

   if( value->u.integer == 0 )
   {
      memcpy( text, "        ", 9 );
      return;
   }
   x_dateDecode( ( long )value->u.integer, &year, &month, &day );
   snprintf( text, 9, "%04d%02d%02d", year, month, day );
}

/* N value as the text of its field, right aligned to the field length */
static X_DBF_STATUS s_numberText( const X_DBF_COLUMN *column, const X_ITEM *value, char *text, size_t *length )
{
   X_DBF_FIELD  field;
   X_DBF_STATUS status;

   memset( &field, 0, sizeof( field ) );
   field.type = X_DBF_NUMERIC;
   field.length = column->spec.length;
   field.decimals = column->spec.decimals;
   status = x_dbfFormatNumeric( &field, value, text );
   *length = field.length;
   return status;
}

/* Text of a field for SDF: C padded to its length, N as stored, D as YYYYMMDD, L as T or F */
static int s_textValue( X_CALL_CTX *ctx, const X_DBF_COLUMN *column, const X_ITEM *value, X_DBF_BUFFER *text,
                        const char *operation )
{
   char         buffer[256];
   size_t       length;
   X_DBF_STATUS status;

   s_bufferClear( text );
   switch( column->spec.type )
   {
      case X_DBF_CHARACTER:
         s_bufferPut( text, value->u.string.data, value->u.string.len );
         while( text->length < column->spec.length )
            s_bufferPut( text, " ", 1 );
         break;
      case X_DBF_NUMERIC:
         status = s_numberText( column, value, buffer, &length );
         if( status != X_DBF_OK )
         {
            x_dbfRaise( ctx, status, operation );
            return 0;
         }
         s_bufferPut( text, buffer, length );
         break;
      case X_DBF_DATE:
         s_dateText( value, buffer );
         s_bufferPut( text, buffer, 8 );
         break;
      default: s_bufferPut( text, IIF( value->u.logical, "T", "F" ), 1 ); break;
   }
   return 1;
}

/* One field of a DELIMITED line: C in quotes without trailing spaces, other types without spaces */
static void s_delimitedField( X_DBF_BUFFER *line, const X_DBF_COLUMN *column, const X_DBF_BUFFER *text, int quote,
                              int separator, int first )
{
   const char *data = text->data;
   size_t      length = text->length;
   int         quoted = column->spec.type == X_DBF_CHARACTER && quote != 0;
   char        mark;

   if( !first && separator != 0 )
   {
      mark = ( char )separator;
      s_bufferPut( line, &mark, 1 );
   }
   if( column->spec.type == X_DBF_CHARACTER )
   {
      while( length != 0 && data[length - 1] == ' ' )
         --length;
   }
   else
      s_trim( &data, &length );
   mark = ( char )quote;
   if( quoted )
      s_bufferPut( line, &mark, 1 );
   s_bufferPut( line, data, length );
   if( quoted )
      s_bufferPut( line, &mark, 1 );
}

/* Next field of a text line: SDF takes the field width in bytes; DELIMITED skips leading spaces,
   reads a quoted or a plain value up to the separator and cuts it to the field width */
static void s_textField( const X_DBF_BUFFER *line, size_t *offset, size_t width, int sdf, int quote, int separator,
                         const char **text, size_t *length )
{
   size_t position = *offset, start;
   int    stop = separator;

   if( sdf )
   {
      start = IIF( position < line->length, position, line->length );
      *text = line->data + start;
      *length = IIF( position + width < line->length, position + width, line->length ) - start;
      *offset = position + width;
      return;
   }
   while( position < line->length && line->data[position] == ' ' )
      ++position;
   if( quote != 0 && position < line->length && ( unsigned char )line->data[position] == quote )
   {
      stop = quote;
      ++position;
   }
   start = position;
   while( position < line->length && ( unsigned char )line->data[position] != stop )
      ++position;
   *text = line->data + start;
   *length = IIF( position - start < width, position - start, width );
   while( position < line->length && ( unsigned char )line->data[position] != separator )
      ++position;
   *offset = position + 1;
}

/* Number of a text like Val(): an integer when the text has no decimal point */
static void s_number( const char *text, size_t length, X_ITEM *value )
{
   char   buffer[64], *end;
   double number;

   if( length >= sizeof( buffer ) )
      length = sizeof( buffer ) - 1;
   memcpy( buffer, text, length );
   buffer[length] = '\0';
   number = strtod( buffer, &end );
   x_itemClear( value );
   if( end != buffer && memchr( buffer, '.', ( size_t )( end - buffer ) ) == NULL && number >= -9.0e15 &&
       number <= 9.0e15 )
      x_itemPutNInt( value, ( int64_t )number );
   else
      x_itemPutND( value, IIF( end == buffer, 0.0, number ) );
}

/* Date from YYYYMMDD; any other text gives an empty date and 0 */
static int s_date( const char *text, size_t length, X_ITEM *value )
{
   int    parts[3] = { 0, 0, 0 }, year, month, day;
   long   julian;
   size_t i;

   x_itemClear( value );
   value->nType = X_IT_DATE;
   value->u.integer = 0;
   if( length != 8 )
      return 0;
   for( i = 0; i < 8; ++i )
   {
      size_t part = IIF( i < 4, 0, IIF( i < 6, 1, 2 ) );
      if( text[i] < '0' || text[i] > '9' )
         return 0;
      parts[part] = parts[part] * 10 + text[i] - '0';
   }
   if( parts[0] < 1 || parts[1] < 1 || parts[1] > 12 || parts[2] < 1 || parts[2] > 31 )
      return 0;
   julian = x_dateEncode( parts[0], parts[1], parts[2] );
   x_dateDecode( julian, &year, &month, &day );
   if( year != parts[0] || month != parts[1] || day != parts[2] )
      return 0;
   value->u.integer = julian;
   return 1;
}

/* Value of a field from the text of SDF or DELIMITED */
static void s_parse( const X_DBF_COLUMN *column, const char *text, size_t length, X_ITEM *value )
{
   x_itemClear( value );
   switch( column->spec.type )
   {
      case X_DBF_NUMERIC: s_number( text, length, value ); break;
      case X_DBF_DATE: s_date( text, length, value ); break;
      case X_DBF_LOGICAL:
         x_itemPutL( value, length != 0 && ( s_upperByte( ( unsigned char )text[0] ) == 'T' ||
                                             s_upperByte( ( unsigned char )text[0] ) == 'Y' ) );
         break;
      default: x_itemPutCL( value, text, length ); break;
   }
}

/* Value of a field from the text of a CSV field: N with a decimal comma, D as YYYY-MM-DD */
static int s_csvParse( X_CALL_CTX *ctx, const X_DBF_COLUMN *column, const char *text, size_t length, X_ITEM *value )
{
   char   buffer[64];
   size_t used = 0, i;

   x_itemClear( value );
   if( column->spec.type != X_DBF_NUMERIC && column->spec.type != X_DBF_DATE && column->spec.type != X_DBF_LOGICAL )
   {
      x_itemPutCL( value, text, length );
      return 1;
   }
   s_trim( &text, &length );
   for( i = 0; i < length && used < sizeof( buffer ) - 1; ++i )
   {
      if( column->spec.type == X_DBF_DATE && text[i] == '-' )
         continue;
      buffer[used++] = IIF( column->spec.type == X_DBF_NUMERIC && text[i] == ',', '.', text[i] );
   }
   buffer[used] = '\0';
   if( column->spec.type == X_DBF_NUMERIC )
      s_number( buffer, used, value );
   else if( column->spec.type == X_DBF_DATE )
   {
      if( !s_date( buffer, used, value ) && used != 0 )
      {
         s_fail( ctx, "Invalid CSV date in field", column->spec.name );
         return 0;
      }
   }
   else
      x_itemPutL( value, used != 0 && ( s_upperByte( ( unsigned char )buffer[0] ) == 'T' ||
                                        s_upperByte( ( unsigned char )buffer[0] ) == 'Y' ) );
   return 1;
}

/* Text of a value for CSV: text in quotes, N with a decimal comma, D as YYYY-MM-DD, L as T or F */
static int s_csvValue( X_CALL_CTX *ctx, const X_DBF_COLUMN *column, const X_ITEM *value, X_DBF_BUFFER *line,
                       const char *operation )
{
   char         text[64];
   const char  *data;
   size_t       length, i;
   X_DBF_STATUS status;

   if( value->nType == X_IT_STRING )
   {
      length = value->u.string.len;
      if( column->spec.type == X_DBF_CHARACTER )
      {
         while( length != 0 && value->u.string.data[length - 1] == ' ' )
            --length;
      }
      s_bufferPut( line, "\"", 1 );
      for( i = 0; i < length; ++i )
      {
         if( value->u.string.data[i] == '"' )
            s_bufferPut( line, "\"", 1 );
         s_bufferPut( line, value->u.string.data + i, 1 );
      }
      s_bufferPut( line, "\"", 1 );
   }
   else if( X_IS_NUMERIC( value ) )
   {
      if( column->spec.type == X_DBF_NUMERIC )
      {
         status = s_numberText( column, value, text, &length );
         if( status != X_DBF_OK )
         {
            x_dbfRaise( ctx, status, operation );
            return 0;
         }
      }
      else if( value->nType == X_IT_INTEGER )
         length = ( size_t )snprintf( text, sizeof( text ), "%lld", ( long long )value->u.integer );
      else
         length = ( size_t )snprintf( text, sizeof( text ), "%.15g", x_itemGetND( value ) );
      data = text;
      s_trim( &data, &length );
      for( i = 0; i < length; ++i )
         s_bufferPut( line, IIF( data[i] == '.', ",", data + i ), 1 );
   }
   else if( value->nType == X_IT_DATE )
   {
      if( value->u.integer != 0 )
      {
         s_dateText( value, text );
         s_bufferPut( line, text, 4 );
         s_bufferPut( line, "-", 1 );
         s_bufferPut( line, text + 4, 2 );
         s_bufferPut( line, "-", 1 );
         s_bufferPut( line, text + 6, 2 );
      }
   }
   else if( value->nType == X_IT_LOGICAL )
      s_bufferPut( line, IIF( value->u.logical, "T", "F" ), 1 );
   else if( value->nType != X_IT_NIL )
   {
      s_fail( ctx, "CSV export does not support the value of field", column->spec.name );
      return 0;
   }
   return 1;
}

/* Quote and separator of DELIMITED: NIL, { cQuote, cSeparator }, "BLANK", "PIPE", "TAB" or a quote */
static void s_delimiter( const X_ITEM *delimiter, int *quote, int *separator )
{
   const X_ITEM *item;

   if( delimiter == NULL || delimiter->nType == X_IT_NIL )
      return;
   if( delimiter->nType == X_IT_ARRAY )
   {
      item = x_arrayGetItemPtr( delimiter, 1 );
      *quote = IIF( item->nType == X_IT_STRING && item->u.string.len != 0, ( unsigned char )item->u.string.data[0], 0 );
      item = x_arrayGetItemPtr( delimiter, 2 );
      *separator =
         IIF( item->nType == X_IT_STRING && item->u.string.len != 0, ( unsigned char )item->u.string.data[0], 0 );
      return;
   }
   if( x_stricmp( delimiter->u.string.data, "BLANK" ) == 0 )
   {
      *quote = 0;
      *separator = ' ';
   }
   else if( x_stricmp( delimiter->u.string.data, "PIPE" ) == 0 )
   {
      *quote = 0;
      *separator = '|';
   }
   else if( x_stricmp( delimiter->u.string.data, "TAB" ) == 0 )
   {
      *quote = 0;
      *separator = '\t';
   }
   else
      *quote = IIF( delimiter->u.string.len != 0, ( unsigned char )delimiter->u.string.data[0], 0 );
}

static int s_textExport( X_CALL_CTX *ctx, DBF area, const X_DBF_COLUMNS *columns, const X_DBF_SCOPE *scope,
                         const char *path, int sdf, int quote, int separator, const char *operation )
{
   X_DBF_WRITER         writer __attribute__( ( cleanup( s_writerDrop ) ) ) = { X_ITEM_NIL_INIT, { NULL, 0, 0 }, 0 };
   X_DBF_RECORDS        records __attribute__( ( cleanup( s_recordsDrop ) ) ) = { NULL, 0, 0 };
   X_DBF_BUFFER         text __attribute__( ( cleanup( s_bufferDrop ) ) ) = { NULL, 0, 0 };
   X_DBF_BUFFER         line __attribute__( ( cleanup( s_bufferDrop ) ) ) = { NULL, 0, 0 };
   X_ITEM               value __attribute__( ( cleanup( x_itemClear ) ) ) = X_ITEM_NIL_INIT;
   const X_DBF_SESSION *session = area->session;
   uint32_t             number = area->number;
   uint64_t             identity = area->identity, position = area->state.record;
   size_t               i, j;

   if( !s_scan( ctx, area, scope, s_recordVisit, &records, operation ) ||
       !s_writerOpen( ctx, &writer, path, operation ) )
      return 0;
   for( i = 0; i < records.count; ++i )
   {
      if( !s_goTo( ctx, session, number, identity, records.items[i], operation ) )
         return 0;
      s_bufferClear( &line );
      for( j = 0; j < columns->count; ++j )
      {
         if( !s_value( ctx, session, &columns->items[j], &value, operation ) ||
             !s_textValue( ctx, &columns->items[j], &value, &text, operation ) )
            return 0;
         if( sdf )
            s_bufferPut( &line, text.data, text.length );
         else
            s_delimitedField( &line, &columns->items[j], &text, quote, separator, j == 0 );
      }
      s_bufferPut( &line, "\r\n", 2 );
      if( !s_writerPut( ctx, &writer, &line, operation ) )
         return 0;
   }
   if( !s_writerClose( ctx, &writer, operation ) )
      return 0;
   return s_goTo( ctx, session, number, identity, position, operation );
}

/* Lines go to a temporary table first; APPEND FROM then applies the fields and the scope */
static int s_textImport( X_CALL_CTX *ctx, DBF area, const X_DBF_COLUMNS *columns, const X_ITEM *names,
                         const X_DBF_SCOPE *scope, const char *path, int sdf, int quote, int separator,
                         const char *operation )
{
   X_DBF_TEMPORARY      temporary __attribute__( ( cleanup( s_temporaryDrop ) ) ) = { NULL };
   X_DBF_OPENED         staged __attribute__( ( cleanup( s_openedDrop ) ) ) = s_opened();
   X_DBF_READER         reader __attribute__( ( cleanup( s_readerDrop ) ) ) = { .file = X_ITEM_NIL_INIT };
   X_DBF_BUFFER         line __attribute__( ( cleanup( s_bufferDrop ) ) ) = { NULL, 0, 0 };
   X_ITEM               value __attribute__( ( cleanup( x_itemClear ) ) ) = X_ITEM_NIL_INIT;
   const X_DBF_SESSION *session = area->session;
   uint32_t             number = area->number;
   uint64_t             identity = area->identity;
   DBF                  table;
   const char          *text;
   size_t               offset, length, j;
   int                  found;
   X_DBF_STATUS         status;

   if( !s_readerOpen( ctx, &reader, path, operation ) || !s_temporary( ctx, &temporary, operation ) ||
       s_create( ctx, &staged, temporary.path, columns, x_dbfDefaultDriver(), operation ) == NULL )
      return 0;
   for( ;; )
   {
      status = s_readLine( &reader, &line, &found );
      if( status != X_DBF_OK )
      {
         x_dbfRaise( ctx, status, operation );
         return 0;
      }
      if( !found )
         break;
      table = s_area( session, staged.number, staged.identity );
      if( table == NULL )
      {
         x_dbfRaise( ctx, X_DBF_INVALID_STATE, operation );
         return 0;
      }
      if( !s_append( ctx, table, operation ) )
         return 0;
      offset = 0;
      for( j = 0; j < columns->count; ++j )
      {
         s_textField( &line, &offset, columns->items[j].spec.length, sdf, quote, separator, &text, &length );
         s_parse( &columns->items[j], text, length, &value );
         if( !s_put( ctx, table, j + 1, &value, operation ) )
            return 0;
      }
   }
   area = s_area( session, number, identity );
   table = s_area( session, staged.number, staged.identity );
   if( area == NULL || table == NULL )
   {
      x_dbfRaise( ctx, X_DBF_INVALID_STATE, operation );
      return 0;
   }
   if( !s_appendFrom( ctx, area, table, names, scope, operation ) )
      return 0;
   return s_close( ctx, &staged, operation );
}

/* ---------------------------------------------------------------------------------------------
   Other helpers of the operations
   --------------------------------------------------------------------------------------------- */

static int s_sortOrder( const X_DBF_SORT *sort, size_t left, size_t right )
{
   const X_DBF_ENTRIES *entries = sort->entries;
   size_t               i;
   int                  order;

   for( i = 0; i < sort->count; ++i )
   {
      s_compare( &entries->keys[left * entries->width + i], &entries->keys[right * entries->width + i], &order );
      if( sort->keys[i].descending )
         order = -order;
      if( order != 0 )
         return order;
   }
   return ( entries->records[left] > entries->records[right] ) - ( entries->records[left] < entries->records[right] );
}

/* Stable merge sort of entry positions */
static void s_mergeSort( const X_DBF_SORT *sort, size_t *items, size_t *scratch, size_t count )
{
   size_t middle, left = 0, right, output = 0;

   if( count < 2 )
      return;
   middle = count / 2;
   s_mergeSort( sort, items, scratch, middle );
   s_mergeSort( sort, items + middle, scratch, count - middle );
   right = middle;
   while( left < middle || right < count )
   {
      if( right == count || ( left < middle && s_sortOrder( sort, items[left], items[right] ) <= 0 ) )
         scratch[output++] = items[left++];
      else
         scratch[output++] = items[right++];
   }
   memcpy( items, scratch, count * sizeof( *items ) );
}

/* JOIN order: the fields of the master area first, then within an area by field position */
static int s_joinBefore( const X_DBF_COLUMN *left, const X_DBF_COLUMN *right, uint32_t master )
{
   return ( left->number == master && right->number != master ) ||
          ( left->number == right->number && left->ordinal < right->ordinal );
}

static void s_totalDrop( X_DBF_TOTAL *total )
{
   size_t i;

   x_itemClear( &total->previous );
   if( total->group != NULL )
   {
      for( i = 0; i < total->groupCount; ++i )
         x_itemClear( &total->group[i] );
      x_xfree( total->group );
   }
   if( total->sums != NULL )
      x_xfree( total->sums );
}

static int s_totalFlush( X_CALL_CTX *ctx, X_DBF_TOTAL *total )
{
   DBF    target;
   size_t i;

   if( !total->hasGroup )
      return 1;
   target = s_area( total->session, total->output->number, total->output->identity );
   if( target == NULL )
   {
      x_dbfRaise( ctx, X_DBF_INVALID_STATE, "DBTOTAL" );
      return 0;
   }
   if( !s_append( ctx, target, "DBTOTAL" ) )
      return 0;
   for( i = 0; i < total->groupCount; ++i )
   {
      if( !s_put( ctx, target, i + 1, &total->group[i], "DBTOTAL" ) )
         return 0;
      x_itemClear( &total->group[i] );
   }
   total->hasGroup = 0;
   return s_delete( ctx, target, total->deleted, "DBTOTAL" );
}

static void s_totalVisit( X_CALL_CTX *ctx, DBF area, void *cargo )
{
   X_DBF_TOTAL *total = ( X_DBF_TOTAL * )cargo;
   X_ITEM       key __attribute__( ( cleanup( x_itemClear ) ) ) = X_ITEM_NIL_INIT;
   X_ITEM       value __attribute__( ( cleanup( x_itemClear ) ) ) = X_ITEM_NIL_INIT;
   uint32_t     number = area->number;
   uint64_t     identity = area->identity;
   X_DBF_STATUS status;
   int          order, included = 1;
   size_t       i, column;

   if( total->key != NULL )
   {
      if( x_dbfEvalAreaBlock( ctx, number, identity, total->key, &key ) == NULL )
         return;
   }
   else
   {
      status = x_dbfOrderInfo( area, NULL, NULL, DBOI_KEYVAL, NULL, &key );
      if( status != X_DBF_OK )
      {
         x_dbfRaise( ctx, status, "DBTOTAL" );
         return;
      }
   }
   if( total->forBlock != NULL )
   {
      if( x_dbfEvalAreaBlock( ctx, number, identity, total->forBlock, &value ) == NULL )
         return;
      included = value.nType == X_IT_LOGICAL && value.u.logical;
   }
   if( total->hasPrevious )
   {
      if( !s_order( ctx, &total->previous, &key, &order, "DBTOTAL" ) )
         return;
      if( order != 0 && !s_totalFlush( ctx, total ) )
         return;
   }
   x_itemCopy( &total->previous, &key );
   total->hasPrevious = 1;
   if( !included )
      return;
   area = s_area( total->session, number, identity );
   if( area == NULL )
   {
      x_dbfRaise( ctx, X_DBF_INVALID_STATE, "DBTOTAL" );
      return;
   }
   if( !total->hasGroup )
   {
      for( i = 0; i < total->groupCount; ++i )
      {
         if( !s_value( ctx, total->session, &total->columns->items[i], &total->group[i], "DBTOTAL" ) )
            return;
      }
      total->deleted = area->state.deleted;
      total->hasGroup = 1;
      return;
   }
   for( i = 0; i < total->sumCount; ++i )
   {
      column = total->sums[i];
      if( !s_value( ctx, total->session, &total->columns->items[column], &value, "DBTOTAL" ) )
         return;
      if( !s_add( &total->group[column], &value ) )
      {
         x_dbfRaise( ctx, X_DBF_INVALID_FIELD_VALUE, "DBTOTAL" );
         return;
      }
   }
}

/* One row of DbCreateFrom(): { name, type, length, decimals }; a C length above 255 keeps its
   high byte in the decimals */
static int s_extendedRow( X_CALL_CTX *ctx, DBF source, const size_t *ordinals, X_ITEM *row, const char *operation )
{
   X_ITEM       value __attribute__( ( cleanup( x_itemClear ) ) ) = X_ITEM_NIL_INIT;
   int64_t      numbers[2] = { 0, 0 };
   const char  *text;
   size_t       length;
   char         type = ' ';
   X_DBF_STATUS status;
   int          i;

   x_arrayNew( row, DBS_ALEN );
   for( i = 0; i < 4; ++i )
   {
      x_itemClear( &value );
      status = x_dbfFieldGetValue( source, ordinals[i], &value );
      if( status == X_DBF_OK && ( i < 2 ? value.nType != X_IT_STRING : !X_IS_NUMERIC( &value ) ) )
         status = X_DBF_INVALID_FIELD_VALUE;
      if( status != X_DBF_OK )
      {
         x_dbfRaise( ctx, status, operation );
         return 0;
      }
      if( i < 2 )
      {
         text = value.u.string.data;
         length = value.u.string.len;
         s_trim( &text, &length );
         if( i == 0 )
            x_arraySetCL( row, DBS_NAME, text, length );
         else
            type = IIF( length != 0, text[0], ' ' );
      }
      else
         numbers[i - 2] = x_itemGetNInt( &value );
   }
   if( s_upperByte( ( unsigned char )type ) == 'C' )
   {
      numbers[0] += numbers[1] * 256;
      numbers[1] = 0;
   }
   x_arraySetCL( row, DBS_TYPE, &type, 1 );
   x_arraySetNInt( row, DBS_LEN, numbers[0] );
   x_arraySetNInt( row, DBS_DEC, numbers[1] );
   return 1;
}

/* ---------------------------------------------------------------------------------------------
   Operations called by dbf-api.c
   --------------------------------------------------------------------------------------------- */

void x_dbfBulkAppendFrom( X_CALL_CTX *ctx )
{
   X_DBF_OPENED input __attribute__( ( cleanup( s_openedDrop ) ) ) = s_opened();
   X_DBF_TEXT   path __attribute__( ( cleanup( x_dbfTextDrop ) ) ) = { NULL, 0 };
   X_DBF_SCOPE  scope __attribute__( ( cleanup( s_scopeDrop ) ) ) = { .forItem = X_ITEM_NIL_INIT };
   DBF          target = x_dbfRequireArea( ctx, "DBAPPENDFROM" ), source;

   if( target == NULL )
      return;
   s_scopeRead( ctx, 3, &scope );
   path.data = x_dbfOpenPath( x_parc( ctx, 1 ) );
   source = s_open( ctx, &input, path.data, IIF( x_parclen( ctx, 8 ) != 0, x_parc( ctx, 8 ), target->driver->name ), 1,
                    "DBAPPENDFROM" );
   if( source == NULL || !s_appendFrom( ctx, target, source, x_paritem( ctx, 2 ), &scope, "DBAPPENDFROM" ) ||
       !s_close( ctx, &input, "DBAPPENDFROM" ) )
      return;
   x_retl( ctx, 1 );
}

void x_dbfBulkCopy( X_CALL_CTX *ctx )
{
   X_DBF_COLUMNS columns __attribute__( ( cleanup( s_columnsDrop ) ) ) = { NULL, 0 };
   X_DBF_RECORDS records __attribute__( ( cleanup( s_recordsDrop ) ) ) = { NULL, 0, 0 };
   X_DBF_SCOPE   scope __attribute__( ( cleanup( s_scopeDrop ) ) ) = { .forItem = X_ITEM_NIL_INIT };
   DBF           area = x_dbfRequireArea( ctx, "DBCOPY" );
   uint64_t      position;

   if( area == NULL || !s_columns( ctx, area, x_paritem( ctx, 2 ), 0, &columns, "DBCOPY" ) )
      return;
   s_scopeRead( ctx, 3, &scope );
   position = area->state.record;
   if( !s_scan( ctx, area, &scope, s_recordVisit, &records, "DBCOPY" ) ||
       !s_copyTo( ctx, area, &columns, &records, position, x_parc( ctx, 1 ), s_driver( ctx, 8 ), "DBCOPY" ) )
      return;
   x_retl( ctx, 1 );
}

void x_dbfBulkCopyStruct( X_CALL_CTX *ctx )
{
   X_DBF_OPENED  output __attribute__( ( cleanup( s_openedDrop ) ) ) = s_opened();
   X_DBF_COLUMNS columns __attribute__( ( cleanup( s_columnsDrop ) ) ) = { NULL, 0 };
   DBF           area = x_dbfRequireArea( ctx, "DBCOPYSTRUCT" );

   if( area == NULL || !s_columns( ctx, area, x_paritem( ctx, 2 ), 0, &columns, "DBCOPYSTRUCT" ) ||
       s_create( ctx, &output, x_parc( ctx, 1 ), &columns, x_dbfDefaultDriver(), "DBCOPYSTRUCT" ) == NULL ||
       !s_close( ctx, &output, "DBCOPYSTRUCT" ) )
      return;
   x_retl( ctx, 1 );
}

void x_dbfBulkCopyXStruct( X_CALL_CTX *ctx )
{
   X_DBF_OPENED       output __attribute__( ( cleanup( s_openedDrop ) ) ) = s_opened();
   X_ITEM             value __attribute__( ( cleanup( x_itemClear ) ) ) = X_ITEM_NIL_INIT;
   X_DBF_COLUMN       items[4];
   X_DBF_COLUMNS      columns = { items, 4 };
   DBF                area = x_dbfRequireArea( ctx, "DBCOPYXSTRUCT" ), target;
   const X_DBF_FIELD *field;
   char               type;
   size_t             i;

   if( area == NULL )
      return;
   memset( items, 0, sizeof( items ) );
   for( i = 0; i < 4; ++i )
      items[i].spec = s_extended[i];
   target = s_create( ctx, &output, x_parc( ctx, 1 ), &columns, x_dbfDefaultDriver(), "DBCOPYXSTRUCT" );
   if( target == NULL )
      return;
   for( i = 0; i < area->schema->count; ++i )
   {
      field = &area->schema->fields[i];
      type = ( char )field->type;
      if( !s_append( ctx, target, "DBCOPYXSTRUCT" ) )
         return;
      x_itemClear( &value );
      x_itemPutCL( &value, field->name, strlen( field->name ) );
      if( !s_put( ctx, target, 1, &value, "DBCOPYXSTRUCT" ) )
         return;
      x_itemClear( &value );
      x_itemPutCL( &value, &type, 1 );
      if( !s_put( ctx, target, 2, &value, "DBCOPYXSTRUCT" ) )
         return;
      x_itemClear( &value );
      x_itemPutNInt( &value, field->length );
      if( !s_put( ctx, target, 3, &value, "DBCOPYXSTRUCT" ) )
         return;
      x_itemPutNInt( &value, field->decimals );
      if( !s_put( ctx, target, 4, &value, "DBCOPYXSTRUCT" ) )
         return;
   }
   if( !s_close( ctx, &output, "DBCOPYXSTRUCT" ) )
      return;
   x_retl( ctx, 1 );
}

void x_dbfBulkCreateFrom( X_CALL_CTX *ctx )
{
   X_DBF_OPENED  input __attribute__( ( cleanup( s_openedDrop ) ) ) = s_opened();
   X_DBF_TEXT    path __attribute__( ( cleanup( x_dbfTextDrop ) ) ) = { NULL, 0 };
   X_DBF_RECORDS records __attribute__( ( cleanup( s_recordsDrop ) ) ) = { NULL, 0, 0 };
   X_ITEM        structure __attribute__( ( cleanup( x_itemClear ) ) ) = X_ITEM_NIL_INIT;
   X_DBF_SCOPE   scope __attribute__( ( cleanup( s_scopeDrop ) ) ) = { .forItem = X_ITEM_NIL_INIT };
   size_t        ordinals[4], i;
   DBF           source;

   path.data = x_dbfOpenPath( x_parc( ctx, 2 ) );
   source = s_open( ctx, &input, path.data, s_driver( ctx, 3 ), 1, "DBCREATEFROM" );
   if( source == NULL )
      return;
   for( i = 0; i < 4; ++i )
   {
      ordinals[i] = x_dbfSchemaFindField( source->schema, s_extended[i].name );
      if( ordinals[i] == 0 )
      {
         s_fail( ctx, "Invalid extended structure table", "DBCREATEFROM" );
         return;
      }
   }
   memset( &scope, 0, sizeof( scope ) );
   if( !s_scan( ctx, source, &scope, s_recordVisit, &records, "DBCREATEFROM" ) )
      return;
   x_arrayNew( &structure, ( int64_t )records.count );
   for( i = 0; i < records.count; ++i )
   {
      if( !s_goTo( ctx, input.session, input.number, input.identity, records.items[i], "DBCREATEFROM" ) ||
          !s_extendedRow( ctx, s_area( input.session, input.number, input.identity ), ordinals,
                          x_arrayGetItemPtr( &structure, i + 1 ), "DBCREATEFROM" ) )
         return;
   }
   if( !s_close( ctx, &input, "DBCREATEFROM" ) )
      return;
   x_dbfCreateTable( ctx, x_parc( ctx, 1 ), &structure, x_parc( ctx, 3 ), x_parl( ctx, 4 ), x_parc( ctx, 5 ),
                     "DBCREATEFROM" );
}

void x_dbfBulkEval( X_CALL_CTX *ctx )
{
   X_DBF_SCOPE scope __attribute__( ( cleanup( s_scopeDrop ) ) ) = { .forItem = X_ITEM_NIL_INIT };
   X_ITEM      action __attribute__( ( cleanup( x_itemClear ) ) ) = X_ITEM_NIL_INIT;
   DBF         area = x_dbfRequireArea( ctx, "DBEVAL" );

   if( area == NULL )
      return;
   s_hold( ctx, 1, &action );
   s_scopeRead( ctx, 2, &scope );
   if( !s_scan( ctx, area, &scope, s_evalVisit, &action, "DBEVAL" ) )
      return;
   x_ret( ctx );
}

void x_dbfBulkExportCsv( X_CALL_CTX *ctx )
{
   X_DBF_WRITER         writer __attribute__( ( cleanup( s_writerDrop ) ) ) = { X_ITEM_NIL_INIT, { NULL, 0, 0 }, 0 };
   X_DBF_COLUMNS        columns __attribute__( ( cleanup( s_columnsDrop ) ) ) = { NULL, 0 };
   X_DBF_TEXT           path __attribute__( ( cleanup( x_dbfTextDrop ) ) ) = { NULL, 0 };
   X_DBF_BUFFER         line __attribute__( ( cleanup( s_bufferDrop ) ) ) = { NULL, 0, 0 };
   X_ITEM               value __attribute__( ( cleanup( x_itemClear ) ) ) = X_ITEM_NIL_INIT;
   X_ITEM               held __attribute__( ( cleanup( x_itemClear ) ) ) = X_ITEM_NIL_INIT;
   const X_ITEM        *forBlock = s_hold( ctx, 3, &held );
   DBF                  area = x_dbfRequireArea( ctx, "DBEXPORTCSV" );
   const X_DBF_SESSION *session;
   uint32_t             number;
   uint64_t             identity, position, count = 0;
   X_DBF_STATUS         status;
   size_t               i;

   if( area == NULL || !s_columns( ctx, area, x_paritem( ctx, 2 ), 0, &columns, "DBEXPORTCSV" ) )
      return;
   session = area->session;
   number = area->number;
   identity = area->identity;
   position = area->state.record;
   path.data = s_fileName( x_parc( ctx, 1 ), ".csv" );
   if( !s_writerOpen( ctx, &writer, path.data, "DBEXPORTCSV" ) )
      return;
   /* UTF-8 byte order mark, then the field names separated by ";" */
   s_bufferClear( &line );
   s_bufferPut( &line, "\xEF\xBB\xBF", 3 );
   for( i = 0; i < columns.count; ++i )
   {
      if( i != 0 )
         s_bufferPut( &line, ";", 1 );
      s_bufferPut( &line, columns.items[i].spec.name, strlen( columns.items[i].spec.name ) );
   }
   s_bufferPut( &line, "\r\n", 2 );
   if( !s_writerPut( ctx, &writer, &line, "DBEXPORTCSV" ) )
      return;
   status = x_dbfAreaGoTop( area, x_setDeleted );
   while( status == X_DBF_OK && !area->state.eof )
   {
      if( forBlock != NULL )
      {
         area = x_dbfEvalAreaBlock( ctx, number, identity, forBlock, &value );
         if( area == NULL )
            return;
      }
      if( forBlock == NULL || ( value.nType == X_IT_LOGICAL && value.u.logical ) )
      {
         s_bufferClear( &line );
         for( i = 0; i < columns.count; ++i )
         {
            if( i != 0 )
               s_bufferPut( &line, ";", 1 );
            if( !s_value( ctx, session, &columns.items[i], &value, "DBEXPORTCSV" ) ||
                !s_csvValue( ctx, &columns.items[i], &value, &line, "DBEXPORTCSV" ) )
               return;
         }
         s_bufferPut( &line, "\r\n", 2 );
         if( !s_writerPut( ctx, &writer, &line, "DBEXPORTCSV" ) )
            return;
         ++count;
      }
      area = s_area( session, number, identity );
      status = IIF( area == NULL, X_DBF_INVALID_STATE, x_dbfAreaSkip( area, 1, x_setDeleted ) );
   }
   if( status != X_DBF_OK )
   {
      x_dbfRaise( ctx, status, "DBEXPORTCSV" );
      return;
   }
   if( !s_writerClose( ctx, &writer, "DBEXPORTCSV" ) ||
       !s_goTo( ctx, session, number, identity, position, "DBEXPORTCSV" ) )
      return;
   x_retnint( ctx, ( int64_t )count );
}

void x_dbfBulkImportCsv( X_CALL_CTX *ctx )
{
   X_DBF_READER         reader __attribute__( ( cleanup( s_readerDrop ) ) ) = { .file = X_ITEM_NIL_INIT };
   X_DBF_COLUMNS        columns __attribute__( ( cleanup( s_columnsDrop ) ) ) = { NULL, 0 };
   X_DBF_TEXT           path __attribute__( ( cleanup( x_dbfTextDrop ) ) ) = { NULL, 0 };
   X_DBF_TEXT           map __attribute__( ( cleanup( x_dbfTextDrop ) ) ) = { NULL, 0 };
   X_DBF_BUFFER         line __attribute__( ( cleanup( s_bufferDrop ) ) ) = { NULL, 0, 0 };
   X_DBF_BUFFER         data __attribute__( ( cleanup( s_bufferDrop ) ) ) = { NULL, 0, 0 };
   X_DBF_RECORDS        ends __attribute__( ( cleanup( s_recordsDrop ) ) ) = { NULL, 0, 0 };
   X_ITEM               value __attribute__( ( cleanup( x_itemClear ) ) ) = X_ITEM_NIL_INIT;
   DBF                  area = x_dbfRequireArea( ctx, "DBIMPORTCSV" ), target;
   const X_DBF_SESSION *session;
   const X_DBF_COLUMN  *column;
   uint32_t             number;
   uint64_t             identity, count = 0;
   unsigned char        mark[3];
   size_t              *fields, header, begin, i, j;
   int                  separator, found, byte = -1;
   int64_t              start;
   X_DBF_STATUS         status = X_DBF_OK;

   if( area == NULL || !s_columns( ctx, area, NULL, 0, &columns, "DBIMPORTCSV" ) )
      return;
   session = area->session;
   number = area->number;
   identity = area->identity;
   path.data = s_fileName( x_parc( ctx, 1 ), ".csv" );
   if( !s_readerOpen( ctx, &reader, path.data, "DBIMPORTCSV" ) )
      return;
   /* A UTF-8 byte order mark is skipped */
   for( i = 0; i < 3 && status == X_DBF_OK; ++i )
   {
      status = s_readByte( &reader, &byte );
      mark[i] = ( unsigned char )IIF( byte < 0, 0, byte );
   }
   start = IIF( memcmp( mark, "\xEF\xBB\xBF", 3 ) == 0, 3, 0 );
   s_readerSeek( &reader, start );
   /* The separator comes from the header: ";", tab or "," */
   if( status == X_DBF_OK )
      status = s_readLine( &reader, &line, &found );
   if( status != X_DBF_OK )
   {
      x_dbfRaise( ctx, status, "DBIMPORTCSV" );
      return;
   }
   if( !found )
   {
      s_fail( ctx, "CSV import file has no header", "DBIMPORTCSV" );
      return;
   }
   separator = IIF( strchr( line.data, ';' ) != NULL, ';', IIF( strchr( line.data, '\t' ) != NULL, '\t', ',' ) );
   s_readerSeek( &reader, start );
   status = s_csvRow( &reader, separator, &data, &ends, &found );
   if( status != X_DBF_OK )
   {
      x_dbfRaise( ctx, status, "DBIMPORTCSV" );
      return;
   }
   header = ends.count;
   map.data = ( char * )x_xgrab( ( header + 1 ) * sizeof( size_t ) );
   fields = ( size_t * )map.data;
   for( i = 0; i < header; ++i )
   {
      X_DBF_TEXT name __attribute__( ( cleanup( x_dbfTextDrop ) ) ) = { NULL, 0 };

      begin = IIF( i == 0, 0, ( size_t )ends.items[i - 1] );
      name.data = s_name( data.data + begin, ( size_t )ends.items[i] - begin );
      fields[i] = columns.count;
      for( j = 0; j < columns.count; ++j )
      {
         if( strcmp( columns.items[j].spec.name, name.data ) == 0 )
            fields[i] = j;
      }
      if( fields[i] == columns.count )
      {
         s_fail( ctx, "CSV column has no matching field", name.data );
         return;
      }
   }
   for( ;; )
   {
      status = s_csvRow( &reader, separator, &data, &ends, &found );
      if( status != X_DBF_OK )
      {
         x_dbfRaise( ctx, status, "DBIMPORTCSV" );
         return;
      }
      if( !found )
         break;
      if( ends.count == 1 && ends.items[0] == 0 )
         continue;
      if( ends.count > header )
      {
         s_fail( ctx, "CSV row has more columns than the header", "DBIMPORTCSV" );
         return;
      }
      target = s_area( session, number, identity );
      if( target == NULL )
      {
         x_dbfRaise( ctx, X_DBF_INVALID_STATE, "DBIMPORTCSV" );
         return;
      }
      if( !s_append( ctx, target, "DBIMPORTCSV" ) )
         return;
      for( i = 0; i < ends.count; ++i )
      {
         begin = IIF( i == 0, 0, ( size_t )ends.items[i - 1] );
         column = &columns.items[fields[i]];
         if( !s_csvParse( ctx, column, data.data + begin, ( size_t )ends.items[i] - begin, &value ) ||
             !s_put( ctx, target, column->ordinal, &value, "DBIMPORTCSV" ) )
            return;
      }
      ++count;
   }
   x_retnint( ctx, ( int64_t )count );
}

void x_dbfBulkJoin( X_CALL_CTX *ctx )
{
   X_DBF_OPENED         output __attribute__( ( cleanup( s_openedDrop ) ) ) = s_opened();
   X_DBF_COLUMNS        columns __attribute__( ( cleanup( s_columnsDrop ) ) ) = { NULL, 0 };
   X_DBF_RECORDS        records __attribute__( ( cleanup( s_recordsDrop ) ) ) = { NULL, 0, 0 };
   X_ITEM               value __attribute__( ( cleanup( x_itemClear ) ) ) = X_ITEM_NIL_INIT;
   X_ITEM               held __attribute__( ( cleanup( x_itemClear ) ) ) = X_ITEM_NIL_INIT;
   const X_ITEM        *forBlock = s_hold( ctx, 4, &held );
   DBF                  master = x_dbfRequireArea( ctx, "DBJOIN" ), detail, target;
   const X_DBF_SESSION *session;
   X_DBF_COLUMN         swap;
   X_DBF_SCOPE          scope __attribute__( ( cleanup( s_scopeDrop ) ) ) = { .forItem = X_ITEM_NIL_INIT };
   uint32_t             number, detailNumber;
   uint64_t             identity, detailIdentity, position;
   int                  wasBof, wasEof, matched;
   size_t               i, j;
   X_DBF_STATUS         status;

   if( master == NULL )
      return;
   session = master->session;
   number = master->number;
   identity = master->identity;
   position = master->state.record;
   wasBof = master->state.bof;
   wasEof = master->state.eof;
   detail = x_dbfSessionAlias( session, x_parc( ctx, 1 ) );
   if( detail == NULL )
   {
      s_fail( ctx, "Unknown JOIN work area", x_parc( ctx, 1 ) );
      return;
   }
   detailNumber = detail->number;
   detailIdentity = detail->identity;
   if( !s_columns( ctx, master, x_paritem( ctx, 3 ), 0, &columns, "DBJOIN" ) )
      return;
   for( i = 1; i < columns.count; ++i )
   {
      for( j = i; j > 0 && s_joinBefore( &columns.items[j], &columns.items[j - 1], number ); --j )
      {
         swap = columns.items[j];
         columns.items[j] = columns.items[j - 1];
         columns.items[j - 1] = swap;
      }
   }
   memset( &scope, 0, sizeof( scope ) );
   if( !s_scan( ctx, master, &scope, s_recordVisit, &records, "DBJOIN" ) ||
       s_create( ctx, &output, x_parc( ctx, 2 ), &columns, x_dbfDefaultDriver(), "DBJOIN" ) == NULL )
      return;
   for( i = 0; i < records.count; ++i )
   {
      if( !s_goTo( ctx, session, number, identity, records.items[i], "DBJOIN" ) )
         return;
      detail = s_area( session, detailNumber, detailIdentity );
      status = IIF( detail == NULL, X_DBF_INVALID_STATE, x_dbfAreaGoTop( detail, x_setDeleted ) );
      while( status == X_DBF_OK && !detail->state.eof )
      {
         matched = 1;
         if( forBlock != NULL )
         {
            if( x_dbfEvalAreaBlock( ctx, number, identity, forBlock, &value ) == NULL )
               return;
            matched = value.nType == X_IT_LOGICAL && value.u.logical;
         }
         if( matched )
         {
            target = s_area( session, output.number, output.identity );
            if( target == NULL )
            {
               x_dbfRaise( ctx, X_DBF_INVALID_STATE, "DBJOIN" );
               return;
            }
            if( !s_copyRow( ctx, &columns, target, 0, "DBJOIN" ) )
               return;
         }
         detail = s_area( session, detailNumber, detailIdentity );
         status = IIF( detail == NULL, X_DBF_INVALID_STATE, x_dbfAreaSkip( detail, 1, x_setDeleted ) );
      }
      if( status != X_DBF_OK )
      {
         x_dbfRaise( ctx, status, "DBJOIN" );
         return;
      }
   }
   if( !s_close( ctx, &output, "DBJOIN" ) || !s_goTo( ctx, session, number, identity, position, "DBJOIN" ) )
      return;
   if( wasBof && !wasEof )
   {
      master = s_area( session, number, identity );
      status = x_dbfAreaGoTop( master, x_setDeleted );
      if( status == X_DBF_OK )
         status = x_dbfAreaSkip( master, -1, x_setDeleted );
      if( status != X_DBF_OK )
      {
         x_dbfRaise( ctx, status, "DBJOIN" );
         return;
      }
   }
   x_retl( ctx, 1 );
}

void x_dbfBulkSort( X_CALL_CTX *ctx )
{
   X_DBF_COLUMNS   columns __attribute__( ( cleanup( s_columnsDrop ) ) ) = { NULL, 0 };
   X_DBF_ENTRIES   entries __attribute__( ( cleanup( s_entriesDrop ) ) ) = { NULL, NULL, 1, 0, 0 };
   X_DBF_RECORDS   records __attribute__( ( cleanup( s_recordsDrop ) ) ) = { NULL, 0, 0 };
   X_DBF_TEXT      keyMemory __attribute__( ( cleanup( x_dbfTextDrop ) ) ) = { NULL, 0 };
   X_DBF_TEXT      orderMemory __attribute__( ( cleanup( x_dbfTextDrop ) ) ) = { NULL, 0 };
   const X_ITEM   *names = x_paritem( ctx, 2 );
   DBF             area = x_dbfRequireArea( ctx, "DBSORT" );
   X_DBF_SORT_KEY *keys;
   X_DBF_SCOPE     scope __attribute__( ( cleanup( s_scopeDrop ) ) ) = { .forItem = X_ITEM_NIL_INIT };
   X_DBF_KEYS      collect;
   X_DBF_SORT      sort;
   size_t          count, i, j, *order;
   uint64_t        position;
   char           *flags;

   if( area == NULL )
      return;
   count = ( size_t )x_arrayLen( names );
   keyMemory.data = ( char * )x_xgrab( ( count + 1 ) * sizeof( X_DBF_SORT_KEY ) );
   keys = ( X_DBF_SORT_KEY * )keyMemory.data;
   /* Field names with the flags /D (descending) and /C (case insensitive) */
   for( i = 0; i < count; ++i )
   {
      const X_ITEM *item = x_arrayGetItemPtr( names, i + 1 );
      X_DBF_TEXT    name __attribute__( ( cleanup( x_dbfTextDrop ) ) ) = { NULL, 0 };

      if( !x_dbfString( item ) )
      {
         x_dbfRaise( ctx, X_DBF_INVALID_ARGUMENT, "DBSORT" );
         return;
      }
      name.data = s_name( item->u.string.data, item->u.string.len );
      flags = strchr( name.data, '/' );
      keys[i].descending = flags != NULL && strchr( flags, 'D' ) != NULL;
      keys[i].caseless = flags != NULL && strchr( flags, 'C' ) != NULL;
      if( flags != NULL )
         *flags = '\0';
      keys[i].ordinal = x_dbfSchemaFindField( area->schema, name.data );
      if( keys[i].ordinal == 0 )
      {
         s_fail( ctx, "Unknown SORT field", name.data );
         return;
      }
   }
   if( !s_columns( ctx, area, NULL, 0, &columns, "DBSORT" ) )
      return;
   s_scopeRead( ctx, 3, &scope );
   position = area->state.record;
   entries.width = count;
   collect.entries = &entries;
   collect.block = NULL;
   collect.keys = keys;
   collect.keyCount = count;
   collect.operation = "DBSORT";
   if( !s_scan( ctx, area, &scope, s_keysVisit, &collect, "DBSORT" ) )
      return;
   /* Every key column must hold values of one kind, so the sort cannot fail */
   for( i = 0; i < count; ++i )
   {
      for( j = 0; j < entries.count; ++j )
      {
         if( s_kind( &entries.keys[j * count + i] ) == 0 ||
             s_kind( &entries.keys[j * count + i] ) != s_kind( &entries.keys[i] ) )
         {
            s_fail( ctx, "SORT keys of different types", "DBSORT" );
            return;
         }
      }
   }
   orderMemory.data = ( char * )x_xgrab( ( entries.count * 2 + 1 ) * sizeof( size_t ) );
   order = ( size_t * )orderMemory.data;
   for( i = 0; i < entries.count; ++i )
      order[i] = i;
   sort.entries = &entries;
   sort.keys = keys;
   sort.count = count;
   s_mergeSort( &sort, order, order + entries.count, entries.count );
   for( i = 0; i < entries.count; ++i )
      s_recordsAdd( &records, entries.records[order[i]] );
   if( !s_copyTo( ctx, area, &columns, &records, position, x_parc( ctx, 1 ), x_dbfDefaultDriver(), "DBSORT" ) )
      return;
   x_retl( ctx, 1 );
}

/* DbCount() (kind 0), DbSum() (kind 1) and DbAverage() (kind 2); the average of no records is 0 */
void x_dbfBulkTally( X_CALL_CTX *ctx, int kind )
{
   X_DBF_TALLY tally
      __attribute__( ( cleanup( s_tallyDrop ) ) ) = { NULL, { X_IT_INTEGER, { .integer = 0 } }, 0, s_tallyNames[kind] };
   X_DBF_SCOPE scope __attribute__( ( cleanup( s_scopeDrop ) ) ) = { .forItem = X_ITEM_NIL_INIT };
   DBF         area = x_dbfRequireArea( ctx, tally.operation );

   if( area == NULL )
      return;
   if( kind != 0 )
      tally.block = x_paritem( ctx, 1 );
   s_scopeRead( ctx, IIF( kind == 0, 1, 2 ), &scope );
   if( !s_scan( ctx, area, &scope, s_tallyVisit, &tally, tally.operation ) )
      return;
   if( kind == 0 )
      x_retnint( ctx, ( int64_t )tally.count );
   else if( kind == 1 )
      x_itemReturn( ctx, &tally.sum );
   else
      x_retnd( ctx, IIF( tally.count == 0, 0.0, x_itemGetND( &tally.sum ) / ( double )tally.count ) );
}

/* DbSDF() (sdf 1) and DbDelim() (sdf 0): export to or import from a text file */
void x_dbfBulkText( X_CALL_CTX *ctx, int sdf )
{
   const char   *operation = IIF( sdf, "DBSDF", "DBDELIM" );
   X_DBF_COLUMNS columns __attribute__( ( cleanup( s_columnsDrop ) ) ) = { NULL, 0 };
   X_DBF_TEXT    path __attribute__( ( cleanup( x_dbfTextDrop ) ) ) = { NULL, 0 };
   const X_ITEM *names = x_paritem( ctx, IIF( sdf, 3, 4 ) );
   DBF           area = x_dbfRequireArea( ctx, operation );
   X_DBF_SCOPE   scope __attribute__( ( cleanup( s_scopeDrop ) ) ) = { .forItem = X_ITEM_NIL_INIT };
   int           quote = '"', separator = ',', done;

   if( area == NULL || !s_columns( ctx, area, names, 1, &columns, operation ) )
      return;
   if( columns.count == 0 )
   {
      s_fail( ctx, "Text transfer requires non-memo fields", operation );
      return;
   }
   if( !sdf )
      s_delimiter( x_paritem( ctx, 3 ), &quote, &separator );
   s_scopeRead( ctx, IIF( sdf, 4, 5 ), &scope );
   path.data = s_fileName( x_parc( ctx, 2 ), ".txt" );
   if( x_parl( ctx, 1 ) )
      done = s_textExport( ctx, area, &columns, &scope, path.data, sdf, quote, separator, operation );
   else
      done = s_textImport( ctx, area, &columns, names, &scope, path.data, sdf, quote, separator, operation );
   if( done )
      x_retl( ctx, 1 );
}

void x_dbfBulkTotal( X_CALL_CTX *ctx )
{
   X_DBF_OPENED  output __attribute__( ( cleanup( s_openedDrop ) ) ) = s_opened();
   X_DBF_COLUMNS columns __attribute__( ( cleanup( s_columnsDrop ) ) ) = { NULL, 0 };
   X_DBF_TOTAL   total __attribute__( ( cleanup( s_totalDrop ) ) ) = { .session = NULL };
   X_ITEM        key __attribute__( ( cleanup( x_itemClear ) ) ) = X_ITEM_NIL_INIT;
   const X_ITEM *names = x_param( ctx, 3, X_IT_ARRAY );
   DBF           area = x_dbfRequireArea( ctx, "DBTOTAL" );
   X_DBF_SCOPE   scope __attribute__( ( cleanup( s_scopeDrop ) ) ) = { .forItem = X_ITEM_NIL_INIT };
   uint64_t      position;
   size_t        count, i, j;

   if( area == NULL || !s_columns( ctx, area, NULL, 1, &columns, "DBTOTAL" ) )
      return;
   total.session = area->session;
   total.columns = &columns;
   total.output = &output;
   total.key = s_hold( ctx, 2, &key );
   total.groupCount = columns.count;
   total.group = ( X_ITEM * )x_xgrab( ( columns.count + 1 ) * sizeof( X_ITEM ) );
   memset( total.group, 0, ( columns.count + 1 ) * sizeof( X_ITEM ) );
   total.sums = ( size_t * )x_xgrab( ( columns.count + 1 ) * sizeof( size_t ) );
   /* Numeric fields named in aFields are summed */
   count = IIF( names != NULL, ( size_t )x_arrayLen( names ), 0 );
   for( j = 0; j < columns.count; ++j )
   {
      for( i = 0; i < count && columns.items[j].spec.type == X_DBF_NUMERIC; ++i )
      {
         const X_ITEM *item = x_arrayGetItemPtr( names, i + 1 );
         X_DBF_TEXT    name __attribute__( ( cleanup( x_dbfTextDrop ) ) ) = { NULL, 0 };

         if( !x_dbfString( item ) )
         {
            x_dbfRaise( ctx, X_DBF_INVALID_ARGUMENT, "DBTOTAL" );
            return;
         }
         name.data = s_name( item->u.string.data, item->u.string.len );
         if( strcmp( columns.items[j].spec.name, name.data ) == 0 )
         {
            total.sums[total.sumCount++] = j;
            break;
         }
      }
   }
   /* FOR chooses the records of a group; every record of the scope can end a group */
   s_scopeRead( ctx, 4, &scope );
   total.forBlock = scope.forBlock;
   scope.forBlock = NULL;
   position = area->state.record;
   if( s_create( ctx, &output, x_parc( ctx, 1 ), &columns, x_dbfDefaultDriver(), "DBTOTAL" ) == NULL ||
       !s_scan( ctx, area, &scope, s_totalVisit, &total, "DBTOTAL" ) || !s_totalFlush( ctx, &total ) ||
       !s_close( ctx, &output, "DBTOTAL" ) ||
       !s_goTo( ctx, total.session, area->number, area->identity, position, "DBTOTAL" ) )
      return;
   x_retl( ctx, 1 );
}

void x_dbfBulkUpdate( X_CALL_CTX *ctx )
{
   X_DBF_ENTRIES        master __attribute__( ( cleanup( s_entriesDrop ) ) ) = { NULL, NULL, 1, 0, 0 };
   X_DBF_ENTRIES        detail __attribute__( ( cleanup( s_entriesDrop ) ) ) = { NULL, NULL, 1, 0, 0 };
   X_DBF_RECORDS        members __attribute__( ( cleanup( s_recordsDrop ) ) ) = { NULL, 0, 0 };
   X_ITEM               value __attribute__( ( cleanup( x_itemClear ) ) ) = X_ITEM_NIL_INIT;
   X_ITEM               heldKey __attribute__( ( cleanup( x_itemClear ) ) ) = X_ITEM_NIL_INIT;
   X_ITEM               heldAction __attribute__( ( cleanup( x_itemClear ) ) ) = X_ITEM_NIL_INIT;
   const X_ITEM        *action = s_hold( ctx, 4, &heldAction );
   int                  random = x_parl( ctx, 3 ), order = 0, matched;
   DBF                  area = x_dbfRequireArea( ctx, "DBUPDATE" ), other;
   const X_DBF_SESSION *session;
   X_DBF_SCOPE          scope __attribute__( ( cleanup( s_scopeDrop ) ) ) = { .forItem = X_ITEM_NIL_INIT };
   X_DBF_KEYS           keys;
   uint32_t             number, otherNumber;
   uint64_t             identity, otherIdentity;
   size_t               current = 0, i;
   X_DBF_STATUS         status;

   if( area == NULL )
      return;
   session = area->session;
   number = area->number;
   identity = area->identity;
   other = x_dbfSessionAlias( session, x_parc( ctx, 1 ) );
   if( other == NULL )
   {
      s_fail( ctx, "Unknown UPDATE work area", x_parc( ctx, 1 ) );
      return;
   }
   otherNumber = other->number;
   otherIdentity = other->identity;
   if( random && area->order == NULL )
   {
      s_fail( ctx, "UPDATE RANDOM requires an active order", "DBUPDATE" );
      return;
   }
   /* Keys of both areas are read first; the actions see the records as they were */
   memset( &scope, 0, sizeof( scope ) );
   keys.entries = &master;
   keys.block = s_hold( ctx, 2, &heldKey );
   keys.keys = NULL;
   keys.keyCount = 0;
   keys.operation = "DBUPDATE";
   if( !s_scan( ctx, area, &scope, s_keysVisit, &keys, "DBUPDATE" ) )
      return;
   other = s_area( session, otherNumber, otherIdentity );
   if( other == NULL )
   {
      x_dbfRaise( ctx, X_DBF_INVALID_STATE, "DBUPDATE" );
      return;
   }
   keys.entries = &detail;
   if( !s_scan( ctx, other, &scope, s_keysVisit, &keys, "DBUPDATE" ) )
      return;
   if( master.count != 0 && !s_goTo( ctx, session, number, identity, master.records[0], "DBUPDATE" ) )
      return;
   if( random )
   {
      for( i = 0; i < master.count; ++i )
         s_recordsAdd( &members, master.records[i] );
      if( members.count != 0 )
         qsort( members.items, members.count, sizeof( *members.items ), s_recordOrder );
   }
   for( i = 0; i < detail.count; ++i )
   {
      if( !s_goTo( ctx, session, otherNumber, otherIdentity, detail.records[i], "DBUPDATE" ) )
         return;
      area = s_area( session, number, identity );
      if( area == NULL )
      {
         x_dbfRaise( ctx, X_DBF_INVALID_STATE, "DBUPDATE" );
         return;
      }
      if( random )
      {
         status = x_dbfOrderSeek( area, &detail.keys[i], 0, 0, x_setDeleted );
         if( status != X_DBF_OK )
         {
            x_dbfRaise( ctx, status, "DBUPDATE" );
            return;
         }
         matched = area->state.found && members.count != 0 &&
                   bsearch( &area->state.record, members.items, members.count, sizeof( *members.items ),
                            s_recordOrder ) != NULL;
      }
      else
      {
         /* Both areas are in key order: the master moves forward to the key of the detail record */
         while( current < master.count )
         {
            if( !s_order( ctx, &master.keys[current], &detail.keys[i], &order, "DBUPDATE" ) )
               return;
            if( order >= 0 )
               break;
            ++current;
         }
         if( !s_goTo( ctx, session, number, identity, IIF( current < master.count, master.records[current], 0 ),
                      "DBUPDATE" ) )
            return;
         matched = current < master.count && order == 0;
      }
      if( matched && x_dbfEvalAreaBlock( ctx, number, identity, action, &value ) == NULL )
         return;
   }
   area = s_area( session, number, identity );
   status = IIF( area == NULL, X_DBF_INVALID_STATE, x_dbfAreaCommit( area ) );
   if( status != X_DBF_OK )
   {
      x_dbfRaise( ctx, status, "DBUPDATE" );
      return;
   }
   x_retl( ctx, 1 );
}
