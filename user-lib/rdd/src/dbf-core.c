/*
 * Copyright (c) 2024-2026 xDev-Language
 *
 * Library: xDev 7.0.0 RDD
 * File: dbf-core.c
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "dbf-internal.h"

#include <xdev/dbinfo.ch>
#include <string.h>

typedef struct
{
   DBF      area;
   uint32_t selected;
} X_DBF_OPEN_GUARD;

static const char *const s_statusNames[] = { "Success",
                                             "Invalid argument",
                                             "Invalid DBF schema",
                                             "Duplicate name",
                                             "Invalid UTF-8 text",
                                             "Text exceeds storage limits",
                                             "Work area not found",
                                             "Work area is already in use",
                                             "DBF driver not registered",
                                             "Unsupported driver ABI",
                                             "Invalid work area state",
                                             "Table is read only",
                                             "No current record",
                                             "Invalid record",
                                             "File operation failed",
                                             "Unsupported DBF operation",
                                             "Unsupported DBF format",
                                             "Corrupt DBF file",
                                             "Cannot open DBF file",
                                             "Cannot create DBF file",
                                             "Cannot read DBF file",
                                             "Cannot write DBF file",
                                             "Cannot close DBF file",
                                             "Cannot acquire or release DBF lock",
                                             "Record must be locked before modification",
                                             "Operation requires exclusive access",
                                             "Value does not fit the DBF field",
                                             "Invalid DBF field value",
                                             "DBF file requires recovery after an incomplete write",
                                             "Opening an index requires compiled key and FOR blocks",
                                             "Invalid NTX key value",
                                             "Index order not found" };

static unsigned char s_nameUpper( unsigned char value )
{
   return IIF( value >= 'a' && value <= 'z', ( unsigned char )( value - 'a' + 'A' ), value );
}

static size_t s_nameLength( const char *name )
{
   size_t length = strlen( name );

   while( length != 0 && name[length - 1] == ' ' )
      --length;
   return length;
}

static int s_nameValid( const char *name, size_t length )
{
   size_t i;

   if( length == 0 )
      return 0;

   for( i = 0; i < length; ++i )
   {
      unsigned char value = s_nameUpper( ( unsigned char )name[i] );

      if( value != '_' && !( value >= 'A' && value <= 'Z' ) && !( i != 0 && value >= '0' && value <= '9' ) )
         return 0;
   }

   return 1;
}

static int s_nameEqual( const char *left, const char *right )
{
   size_t length = s_nameLength( left );
   size_t i;

   if( length != s_nameLength( right ) )
      return 0;

   for( i = 0; i < length; ++i )
   {
      if( s_nameUpper( ( unsigned char )left[i] ) != s_nameUpper( ( unsigned char )right[i] ) )
         return 0;
   }

   return 1;
}

static void s_nameCopy( char *destination, const char *source, size_t length )
{
   size_t i;

   for( i = 0; i < length; ++i )
      destination[i] = ( char )s_nameUpper( ( unsigned char )source[i] );

   destination[length] = '\0';
}

static void s_schemaDrop( X_DBF_SCHEMA **schema )
{
   x_dbfSchemaDestroy( *schema );
}

static void s_recordDrop( X_DBF_TEXT *record )
{
   x_dbfTextClear( record );
}

const char *x_dbfStatusName( X_DBF_STATUS status )
{
   if( ( unsigned int )status >= sizeof( s_statusNames ) / sizeof( s_statusNames[0] ) )
      return "Unknown DBF error";

   return s_statusNames[status];
}

void x_dbfRaise( X_CALL_CTX *ctx, X_DBF_STATUS status, const char *operation )
{
   X_ERROR_DEF definition;

   if( status == X_DBF_OK )
      return;

   definition.severity = 2;

   switch( status )
   {
      case X_DBF_INVALID_ARGUMENT:
      case X_DBF_INVALID_SCHEMA: definition.genCode = 1; break;
      case X_DBF_DUPLICATE_NAME:
      case X_DBF_AREA_IN_USE: definition.genCode = 16; break;
      case X_DBF_AREA_NOT_FOUND: definition.genCode = 35; break;
      case X_DBF_INVALID_TEXT:
      case X_DBF_INVALID_FIELD_VALUE:
      case X_DBF_INVALID_INDEX_KEY: definition.genCode = 33; break;
      case X_DBF_TEXT_OVERFLOW: definition.genCode = 31; break;
      case X_DBF_FIELD_OVERFLOW: definition.genCode = 34; break;
      case X_DBF_READ_ONLY: definition.genCode = 39; break;
      case X_DBF_INVALID_RECORD:
      case X_DBF_CORRUPT_FILE:
      case X_DBF_RECOVERY_REQUIRED: definition.genCode = 32; break;
      case X_DBF_DRIVER_NOT_FOUND:
      case X_DBF_UNSUPPORTED_DRIVER_ABI:
      case X_DBF_UNSUPPORTED_OPERATION:
      case X_DBF_UNSUPPORTED_FORMAT:
      case X_DBF_INDEX_EXPRESSION_REQUIRED:
      case X_DBF_ORDER_NOT_FOUND: definition.genCode = 30; break;
      case X_DBF_OPEN_ERROR: definition.genCode = 21; break;
      case X_DBF_CREATE_ERROR: definition.genCode = 20; break;
      case X_DBF_READ_ERROR: definition.genCode = 23; break;
      case X_DBF_WRITE_ERROR: definition.genCode = 24; break;
      case X_DBF_CLOSE_ERROR: definition.genCode = 22; break;
      case X_DBF_LOCK_FAILED: definition.genCode = 41; break;
      case X_DBF_RECORD_UNLOCKED: definition.genCode = 38; break;
      case X_DBF_SHARED_OPERATION: definition.genCode = 37; break;
      default: definition.genCode = 0; break;
   }

   definition.subCode = 6000 + ( int )status;
   definition.canSubstitute = 0;
   definition.subSystem = "XDEVDBF";
   definition.description = x_dbfStatusName( status );
   definition.operation = operation;

   x_errorRaiseDefinition( ctx->result, &definition, NULL, NULL );
}

X_DBF_STATUS x_dbfSchemaCreate( const X_DBF_FIELD_SPEC *fields, size_t count, X_DBF_SCHEMA **schema )
{
   X_DBF_SCHEMA *created __attribute__( ( cleanup( s_schemaDrop ) ) ) = NULL;
   size_t        length = 1;
   size_t        i, j;

   if( count == 0 || count > ( UINT16_MAX - 34 ) / 32 )
      return X_DBF_INVALID_SCHEMA;

   created = ( X_DBF_SCHEMA * )x_xgrab( sizeof( *created ) + count * sizeof( X_DBF_FIELD ) );
   created->count = count;

   for( i = 0; i < count; ++i )
   {
      const X_DBF_FIELD_SPEC *source = &fields[i];
      X_DBF_FIELD            *field = &created->fields[i];
      size_t                  nameLength;
      size_t                  fieldLength = IIF( source->type == X_DBF_LOGICAL, 1, source->length );

      if( source->name == NULL )
         return X_DBF_INVALID_SCHEMA;

      nameLength = s_nameLength( source->name );

      if( nameLength > X_DBF_MAX_FIELD_NAME || !s_nameValid( source->name, nameLength ) || fieldLength == 0 )
         return X_DBF_INVALID_SCHEMA;

      switch( source->type )
      {
         case X_DBF_CHARACTER:
            if( source->length > 254 || source->decimals != 0 )
               return X_DBF_INVALID_SCHEMA;
            break;
         case X_DBF_NUMERIC:
            if( source->length > 20 || ( source->decimals != 0 && source->decimals + 2 > source->length ) )
               return X_DBF_INVALID_SCHEMA;
            break;
         case X_DBF_DATE:
            if( source->length != 8 || source->decimals != 0 )
               return X_DBF_INVALID_SCHEMA;
            break;
         case X_DBF_LOGICAL:
            if( source->decimals != 0 )
               return X_DBF_INVALID_SCHEMA;
            break;
         case X_DBF_MEMO:
         case X_DBF_PICTURE:
            if( ( source->length != 4 && source->length != 10 ) || source->decimals != 0 )
               return X_DBF_INVALID_SCHEMA;
            break;
         default: return X_DBF_INVALID_SCHEMA;
      }

      if( length + fieldLength > UINT16_MAX )
         return X_DBF_INVALID_SCHEMA;

      memset( field, 0, sizeof( *field ) );
      s_nameCopy( field->name, source->name, nameLength );
      field->type = source->type;
      field->length = ( uint16_t )fieldLength;
      field->decimals = source->decimals;
      field->offset = ( uint32_t )length;
      length += field->length;

      for( j = 0; j < i; ++j )
      {
         if( s_nameEqual( created->fields[j].name, field->name ) )
            return X_DBF_DUPLICATE_NAME;
      }
   }

   created->recordLength = length;
   *schema = created;
   created = NULL;

   return X_DBF_OK;
}

void x_dbfSchemaDestroy( X_DBF_SCHEMA *schema )
{
   x_xfree( schema );
}

size_t x_dbfSchemaFieldCount( const X_DBF_SCHEMA *schema )
{
   return schema->count;
}

size_t x_dbfSchemaRecordLength( const X_DBF_SCHEMA *schema )
{
   return schema->recordLength;
}

const X_DBF_FIELD *x_dbfSchemaField( const X_DBF_SCHEMA *schema, size_t ordinal )
{
   return IIF( ordinal != 0 && ordinal <= schema->count, &schema->fields[ordinal - 1], NULL );
}

size_t x_dbfSchemaFindField( const X_DBF_SCHEMA *schema, const char *name )
{
   size_t i;

   if( name != NULL )
   {
      for( i = 0; i < schema->count; ++i )
      {
         if( s_nameEqual( schema->fields[i].name, name ) )
            return i + 1;
      }
   }

   return 0;
}

X_DBF_DRIVER_ENTRY *x_dbfDriverFind( const X_DBF_SESSION *session, const char *name )
{
   X_DBF_DRIVER_ENTRY *driver;

   for( driver = session->drivers; driver != NULL; driver = driver->next )
   {
      if( s_nameEqual( driver->name, name ) )
         return driver;
   }

   return NULL;
}

static uint32_t s_freeAreaNumber( const X_DBF_SESSION *session )
{
   DBF      area;
   uint32_t number = 1;

   for( area = session->areas; area != NULL && area->number == number; area = area->next )
      ++number;

   return IIF( number <= X_DBF_MAX_AREA, number, 0 );
}

X_DBF_SESSION *x_dbfSessionCreate( void )
{
   X_DBF_SESSION *session = ( X_DBF_SESSION * )x_xgrab( sizeof( *session ) );

   memset( session, 0, sizeof( *session ) );
   session->current = 1;

   x_dbfDriverRegister( session, &x_dbfFileDriver );
   {
      X_DBF_DRIVER driver = x_dbfFileDriver;
      driver.name = "DBFDBT";
      x_dbfDriverRegister( session, &driver );
      driver.name = "DBFFPT";
      x_dbfDriverRegister( session, &driver );
      driver.name = "DBFSMT";
      x_dbfDriverRegister( session, &driver );
      driver.name = "DBFNTX";
      x_dbfDriverRegister( session, &driver );
      driver.name = "DBFCDX";
      x_dbfDriverRegister( session, &driver );
   }

   return session;
}

X_DBF_STATUS x_dbfDriverRegister( X_DBF_SESSION *session, const X_DBF_DRIVER *driver )
{
   X_DBF_DRIVER_ENTRY *entry;
   size_t              length;

   if( driver->abi != X_DBF_DRIVER_ABI )
      return X_DBF_UNSUPPORTED_DRIVER_ABI;

   length = s_nameLength( driver->name );
   if( length > X_DBF_MAX_DRIVER_NAME || !s_nameValid( driver->name, length ) )
      return X_DBF_INVALID_ARGUMENT;

   if( x_dbfDriverFind( session, driver->name ) != NULL )
      return X_DBF_DUPLICATE_NAME;

   entry = ( X_DBF_DRIVER_ENTRY * )x_xgrab( sizeof( *entry ) );
   entry->operations = *driver;
   /* HB64 locks lie beyond any data offset, so the table size is not limited by the lock area */
   entry->lockScheme = DB_DBFLOCK_HB64;
   entry->lockRetry = 0;
   entry->setHeader = DB_SETHEADER_FULL;
   s_nameCopy( entry->name, driver->name, length );
   entry->operations.name = entry->name;
   entry->next = session->drivers;
   session->drivers = entry;

   return X_DBF_OK;
}

DBF x_dbfSessionArea( const X_DBF_SESSION *session, uint32_t number )
{
   DBF area;

   for( area = session->areas; area != NULL && area->number <= number; area = area->next )
   {
      if( area->number == number )
         return area;
   }

   return NULL;
}

DBF x_dbfSessionAlias( const X_DBF_SESSION *session, const char *alias )
{
   DBF area;

   if( alias != NULL )
   {
      for( area = session->areas; area != NULL; area = area->next )
      {
         if( s_nameEqual( area->alias, alias ) )
            return area;
      }
   }

   return NULL;
}

DBF x_dbfSessionCurrent( const X_DBF_SESSION *session )
{
   return x_dbfSessionArea( session, session->current );
}

uint32_t x_dbfSessionSelected( const X_DBF_SESSION *session )
{
   return session->current;
}

X_DBF_STATUS x_dbfSessionSelect( X_DBF_SESSION *session, uint32_t number )
{
   if( number > X_DBF_MAX_AREA )
      return X_DBF_INVALID_ARGUMENT;

   if( number == 0 )
   {
      number = s_freeAreaNumber( session );
      if( number == 0 )
         return X_DBF_AREA_IN_USE;
   }

   session->current = number;

   return X_DBF_OK;
}

static void s_driverDispose( void *cargo )
{
   DBF area = ( DBF )cargo;

   area->driver->operations.dispose( area );
}

static void s_areaDestroy( DBF area )
{
   x_dbfOrderDispose( area );
   x_dbfAreaDetachRelations( area );
   x_dbfAreaClearFilter( area );
   x_itemClear( &area->locateFor );

   if( area->driverStarted )
      x_gcCleanup( s_driverDispose, area );

   x_dbfSchemaDestroy( area->schema );
   x_xfree( area->record );
   x_xfree( area->scratch );
   x_xfree( area->alias );
   x_xfree( area );
}

static void s_areaDrop( X_DBF_OPEN_GUARD *guard )
{
   DBF area = guard->area;

   if( area != NULL )
   {
      DBF *position;
      for( position = &area->session->areas; *position != NULL && *position != area; position = &( *position )->next )
      {
      }

      if( *position == area )
         *position = area->next;

      area->session->current = guard->selected;
      s_areaDestroy( area );
   }
}

static void s_blankRecord( const X_DBF_SCHEMA *schema, char *record )
{
   size_t i;

   memset( record, ' ', schema->recordLength );
   for( i = 0; i < schema->count; ++i )
   {
      const X_DBF_FIELD *field = &schema->fields[i];
      if( ( field->type == X_DBF_MEMO || field->type == X_DBF_PICTURE ) && field->length == 4 )
         memset( record + field->offset, 0, 4 );
   }
}

X_DBF_STATUS x_dbfAreaDefine( DBF area, const X_DBF_FIELD_SPEC *fields, size_t count, uint64_t recordCount )
{
   X_DBF_SCHEMA *schema __attribute__( ( cleanup( s_schemaDrop ) ) ) = NULL;
   X_DBF_STATUS  status;

   status = x_dbfSchemaCreate( fields, count, &schema );
   if( status != X_DBF_OK )
      return status;

   area->record = ( char * )x_xgrab( schema->recordLength );
   area->scratch = ( char * )x_xgrab( schema->recordLength );
   s_blankRecord( schema, area->record );
   area->schema = schema;
   schema = NULL;
   area->state.recordCount = recordCount;
   area->state.record = recordCount + 1;
   area->state.bof = 1;
   area->state.eof = 1;

   return X_DBF_OK;
}

X_DBF_STATUS x_dbfAreaOpen( X_DBF_SESSION *session, const X_DBF_OPEN_OPTIONS *options, DBF *result )
{
   X_DBF_OPEN_GUARD    guard __attribute__( ( cleanup( s_areaDrop ) ) ) = { NULL, session->current };
   DBF                 created;
   X_DBF_DRIVER_ENTRY *driver;
   X_DBF_OPEN_OPTIONS  resolved;
   X_DBF_STATUS        status;
   DBF                *position;
   size_t              aliasLength;

   aliasLength = s_nameLength( options->alias );
   if( !s_nameValid( options->alias, aliasLength ) )
      return X_DBF_INVALID_ARGUMENT;

   if( x_dbfSessionAlias( session, options->alias ) != NULL )
      return X_DBF_DUPLICATE_NAME;

   resolved = *options;
   if( resolved.driver == NULL )
      resolved.driver = "DBFNTX";

   driver = x_dbfDriverFind( session, resolved.driver );
   if( driver == NULL )
      return X_DBF_DRIVER_NOT_FOUND;

   resolved.driver = driver->name;
   if( resolved.area == 0 )
      resolved.area = s_freeAreaNumber( session );
   if( resolved.area == 0 || x_dbfSessionArea( session, resolved.area ) != NULL )
      return X_DBF_AREA_IN_USE;
   if( session->nextIdentity == UINT64_MAX )
      return X_DBF_INVALID_STATE;

   created = ( DBF )x_xgrab( sizeof( *created ) );
   memset( created, 0, sizeof( *created ) );
   created->session = session;
   created->driver = driver;
   guard.area = created;
   created->number = resolved.area;
   created->identity = ++session->nextIdentity;
   created->readOnly = resolved.readOnly != 0;
   created->shared = resolved.shared != 0;
   created->lockScheme = driver->lockScheme;
   created->alias = ( char * )x_xgrab( aliasLength + 1 );
   s_nameCopy( created->alias, options->alias, aliasLength );
   resolved.alias = created->alias;
   created->driverStarted = 1;

   status = driver->operations.open( created, &resolved );
   if( status != X_DBF_OK )
      return status;
   if( created->schema == NULL )
      return X_DBF_INVALID_STATE;
   status = x_dbfAreaGoTo( created, 1 );
   if( status != X_DBF_OK )
      return status;

   for( position = &session->areas; *position != NULL && ( *position )->number < created->number;
        position = &( *position )->next )
   {
   }
   created->next = *position;
   *position = created;
   session->current = created->number;
   if( created->structuralIndex )
   {
      status = x_dbfOrderOpen( created, "", resolved.indexBindings );
      if( status == X_DBF_OK )
      {
         created->order = NULL;
         status = x_dbfAreaGoTop( created, 0 );
      }
      if( status != X_DBF_OK )
         return status;
   }
   else if( resolved.indexBindings != NULL )
      return X_DBF_INVALID_ARGUMENT;

   *result = created;
   guard.area = NULL;

   return X_DBF_OK;
}

X_DBF_STATUS x_dbfAreaFlush( DBF area )
{
   X_DBF_STATUS status;

   if( area->indexBusy )
      return X_DBF_INVALID_STATE;

   if( area->state.dirty )
   {
      X_DBF_INDEX_LOCK guard __attribute__( ( cleanup( x_dbfOrderUnlock ) ) ) = { 0 };
      status = x_dbfOrderLock( area, 1, &guard );
      if( status != X_DBF_OK )
         return status;

      status = x_dbfOrderPrepare( area );
      if( status != X_DBF_OK )
         return status;

      status = area->driver->operations.write( area, area->state.record, area->record, area->schema->recordLength );
      if( status != X_DBF_OK )
         return status;

      area->state.dirty = 0;
      area->indexNewRecord = 0;
      status = x_dbfOrderApply( area );

      if( status != X_DBF_OK )
         return status;
   }
   return X_DBF_OK;
}

/* Writes the pending record and forces the table, memo and indexes to disk; ordinary writes leave
   durability to the operating system, so this runs only for DbCommit() and on close */
X_DBF_STATUS x_dbfAreaCommit( DBF area )
{
   X_DBF_STATUS status = x_dbfAreaFlush( area );

   if( status != X_DBF_OK )
      return status;

   status = area->driver->operations.sync( area );
   if( status != X_DBF_OK )
      return status;

   return x_dbfOrderCommit( area );
}

/* The area is always closed and released, also after a failed write; the first error is returned */
X_DBF_STATUS x_dbfAreaClose( DBF area )
{
   X_DBF_SESSION *session = area->session;
   DBF           *position;
   X_DBF_STATUS   status, next;

   if( area->indexBusy || area->indexLockDepth != 0 )
      return X_DBF_INVALID_STATE;

   status = x_dbfAreaCommit( area );
   next = x_dbfAreaClearRelations( area );
   if( status == X_DBF_OK )
      status = next;
   next = area->driver->operations.close( area );
   if( status == X_DBF_OK )
      status = next;

   for( position = &session->areas; *position != area; position = &( *position )->next )
   {
   }

   *position = area->next;
   s_areaDestroy( area );
   return status;
}

void x_dbfAreaDiscard( DBF area )
{
   DBF *position;

   if( area == NULL )
      return;
   for( position = &area->session->areas; *position != area; position = &( *position )->next )
   {
   }

   *position = area->next;
   s_areaDestroy( area );
}

X_DBF_STATUS x_dbfSessionClose( X_DBF_SESSION *session )
{
   X_DBF_DRIVER_ENTRY *driver;
   X_DBF_STATUS        status = X_DBF_OK, next;

   while( session->areas != NULL )
   {
      DBF area = session->areas;
      next = x_dbfAreaClose( area );
      if( status == X_DBF_OK )
         status = next;
      if( session->areas == area )
         return status;
   }

   while( session->drivers != NULL )
   {
      driver = session->drivers;
      session->drivers = driver->next;
      x_xfree( driver );
   }
   x_xfree( session );
   return X_DBF_OK;
}

X_DBF_STATUS x_dbfAreaGoTo( DBF area, uint64_t record )
{
   X_DBF_STATUS status;

   if( area->indexBusy )
      return X_DBF_INVALID_STATE;
   if( area->state.dirty )
   {
      status = x_dbfAreaFlush( area );
      if( status != X_DBF_OK )
         return status;
   }
   area->pendingRelation = NULL;
   status = x_dbfAreaRecordCount( area, &area->state.recordCount );
   if( status != X_DBF_OK )
      return status;
   if( record == 0 || record > area->state.recordCount )
   {
      s_blankRecord( area->schema, area->record );
      area->state.record = area->state.recordCount + 1;
      area->state.bof = 1;
      area->state.eof = 1;
      area->state.found = 0;
      area->state.deleted = 0;
      x_dbfOrderCursorClear( area );
      return x_dbfAreaMarkChildren( area );
   }

   status = area->driver->operations.read( area, record, area->scratch, area->schema->recordLength );
   if( status != X_DBF_OK )
      return status;
   if( area->scratch[0] != ' ' && area->scratch[0] != '*' )
      return X_DBF_INVALID_RECORD;
   memcpy( area->record, area->scratch, area->schema->recordLength );
   area->state.record = record;
   area->state.bof = 0;
   area->state.eof = 0;
   area->state.found = 0;
   area->state.deleted = area->record[0] == '*';
   x_dbfOrderCursorClear( area );
   return x_dbfAreaMarkChildren( area );
}

X_DBF_STATUS x_dbfAreaAppend( DBF area )
{
   X_DBF_STATUS status;
   uint64_t     record = 0;

   if( area->indexBusy )
      return X_DBF_INVALID_STATE;
   if( area->readOnly )
      return X_DBF_READ_ONLY;
   if( area->state.recordCount == UINT64_MAX - 1 )
      return X_DBF_INVALID_RECORD;
   area->pendingRelation = NULL;
   if( area->state.dirty )
   {
      status = x_dbfAreaFlush( area );
      if( status != X_DBF_OK )
         return status;
   }
   s_blankRecord( area->schema, area->scratch );
   status = area->driver->operations.append( area, area->scratch, area->schema->recordLength, &record );
   if( status != X_DBF_OK )
      return status;
   if( record <= area->state.recordCount || record == UINT64_MAX )
      return X_DBF_INVALID_RECORD;

   memcpy( area->record, area->scratch, area->schema->recordLength );
   area->state.recordCount = record;
   area->state.record = record;
   area->state.bof = 0;
   area->state.eof = 0;
   area->state.found = 0;
   area->state.deleted = 0;
   area->state.dirty = area->indexes != NULL;
   area->indexNewRecord = area->indexes != NULL;
   x_dbfOrderCursorClear( area );
   return x_dbfAreaMarkChildren( area );
}

X_DBF_STATUS x_dbfAreaDelete( DBF area, int deleted )
{
   char         marker;
   X_DBF_STATUS status = x_dbfCheckWrite( area );

   if( status != X_DBF_OK )
      return status;
   marker = IIF( deleted, '*', ' ' );
   if( area->record[0] != marker )
   {
      area->record[0] = marker;
      area->state.deleted = deleted != 0;
      area->state.dirty = 1;
   }
   return X_DBF_OK;
}

uint32_t x_dbfAreaNumber( DBF area )
{
   return area->number;
}

const char *x_dbfAreaAlias( DBF area )
{
   return area->alias;
}

const X_DBF_STATE *x_dbfAreaState( DBF area )
{
   return &area->state;
}

const X_DBF_SCHEMA *x_dbfAreaSchema( DBF area )
{
   return area->schema;
}

void *x_dbfAreaStorage( DBF area )
{
   return area->storage;
}

void x_dbfAreaSetStorage( DBF area, void *storage )
{
   area->storage = storage;
}

X_DBF_STATUS x_dbfFieldGetText( DBF area, size_t ordinal, X_DBF_TEXT *result )
{
   const X_DBF_FIELD *field;
   X_DBF_TEXT         text __attribute__( ( cleanup( s_recordDrop ) ) ) = { NULL, 0 };
   X_DBF_STATUS       status = x_dbfAreaPrepare( area );

   if( status != X_DBF_OK )
      return status;
   field = x_dbfSchemaField( area->schema, ordinal );
   if( field == NULL || ( field->type != X_DBF_CHARACTER && field->type != X_DBF_MEMO ) )
      return X_DBF_INVALID_ARGUMENT;
   if( field->type == X_DBF_MEMO )
   {
      int binary;
      status = x_dbfFieldReadMemo( area, field, &text, &binary );
      if( status != X_DBF_OK )
         return status;
      if( binary )
         return X_DBF_INVALID_FIELD_VALUE;
      x_dbfTextClear( result );
      *result = text;
      text.data = NULL;
      return X_DBF_OK;
   }
   return x_dbfTextCopy( area->record + field->offset, field->length, result );
}

X_DBF_STATUS x_dbfFieldPutText( DBF area, size_t ordinal, const char *text, size_t length )
{
   X_DBF_TEXT         encoded __attribute__( ( cleanup( s_recordDrop ) ) ) = { NULL, 0 };
   const X_DBF_FIELD *field;
   X_DBF_STATUS       status = x_dbfAreaPrepare( area );
   size_t             stored;
   char              *destination;

   if( status != X_DBF_OK )
      return status;
   field = x_dbfSchemaField( area->schema, ordinal );
   if( field == NULL || ( field->type != X_DBF_CHARACTER && field->type != X_DBF_MEMO ) )
      return X_DBF_INVALID_ARGUMENT;
   if( field->type == X_DBF_MEMO )
      return x_dbfFieldWriteMemo( area, field, text, length );
   status = x_dbfCheckWrite( area );
   if( status != X_DBF_OK )
      return status;
   status = x_dbfTextCopy( text, length, &encoded );
   if( status != X_DBF_OK )
      return status;

   stored = x_dbfTextFit( encoded.data, encoded.length, field->length );
   destination = area->record + field->offset;
   memcpy( destination, encoded.data, stored );
   memset( destination + stored, ' ', field->length - stored );
   area->state.dirty = 1;
   return X_DBF_OK;
}
