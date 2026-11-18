/*
 * Copyright (c) 2024-2026 xDev-Language
 *
 * Library: xDev 7.0.0 RDD
 * File: dbf-info.c
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "dbf-internal.h"

#include <xdev/dbinfo.ch>
#include <xdev/dbstruct.ch>
#include <stdio.h>
#include <string.h>

typedef struct
{
   DBF          area;
   uint64_t     record;
   X_DBF_STATUS status;
} X_DBF_INFO_GUARD;

static void s_restoreRecord( X_DBF_INFO_GUARD *guard )
{
   if( guard->record != 0 )
   {
      int          error = x_fsError();
      uint64_t     record = guard->record;
      X_DBF_STATUS status;
      guard->record = 0;
      status = x_dbfAreaGoTo( guard->area, record );
      if( guard->status == X_DBF_OK )
         guard->status = status;
      else
         x_fsSetError( error );
   }
}

static void s_textDrop( X_DBF_TEXT *text )
{
   x_dbfTextClear( text );
}

static void s_setString( X_ITEM *result, const char *text, size_t length )
{
   x_itemPutStringBuffer( result, x_strndup( text, length ), length );
}

X_DBF_STATUS x_dbfDriverInfo( const X_DBF_DRIVER_ENTRY *driver, int selector, X_ITEM *result )
{
   X_ITEM value = X_ITEM_NIL_INIT;
   int    ntx = strcmp( driver->name, "DBFNTX" ) == 0;
   int    cdx = strcmp( driver->name, "DBFCDX" ) == 0;
   if( driver->operations.create != x_dbfFileDriver.create )
      return X_DBF_UNSUPPORTED_OPERATION;
   switch( selector )
   {
      case RDDI_ISDBF:
      case RDDI_CANPUTREC:
      case RDDI_LOCAL:
         value.nType = X_IT_LOGICAL;
         value.u.logical = 1;
         break;
      case RDDI_REMOTE:
      case RDDI_RECORDMAP:
      case RDDI_ENCRYPTION:
      case RDDI_OPTIMIZE:
      case RDDI_FORCEOPT:
      case RDDI_MULTIKEY:
      case RDDI_BLOB_SUPPORT:
      case RDDI_AUTOSHARE:
      case RDDI_STRICTREAD:
      case RDDI_AUTOLOCK:
      case RDDI_DIRTYREAD:
         value.nType = X_IT_LOGICAL;
         value.u.logical = 0;
         break;
      case RDDI_LARGEFILE:
         value.nType = X_IT_LOGICAL;
         value.u.logical = driver->lockScheme == DB_DBFLOCK_HB64;
         break;
      case RDDI_MULTITAG:
         value.nType = X_IT_LOGICAL;
         value.u.logical = ntx || cdx;
         break;
      case RDDI_STRUCTORD:
      case RDDI_AUTOOPEN:
         value.nType = X_IT_LOGICAL;
         value.u.logical = cdx;
         break;
      case RDDI_SORTRECNO:
         value.nType = X_IT_LOGICAL;
         value.u.logical = ntx || cdx;
         break;
      case RDDI_TABLEEXT: s_setString( result, ".dbf", 4 ); return X_DBF_OK;
      case RDDI_MEMOEXT:
         /* New xDev 7 tables always get a DBT memo with 64-byte blocks, whatever the driver */
         s_setString( result, ".dbt", 4 );
         return X_DBF_OK;
      case RDDI_ORDBAGEXT:
      case RDDI_ORDEREXT:
      case RDDI_ORDSTRUCTEXT:
      {
         const char *extension = IIF( cdx, ".cdx", IIF( ntx, ".ntx", "" ) );
         s_setString( result, extension, strlen( extension ) );
         return X_DBF_OK;
      }
      case RDDI_TABLETYPE:
         value.nType = X_IT_INTEGER;
         value.u.integer = DB_DBF_STD;
         break;
      case RDDI_MEMOTYPE:
         value.nType = X_IT_INTEGER;
         value.u.integer = DB_MEMO_DBT;
         break;
      case RDDI_MEMOBLOCKSIZE:
         value.nType = X_IT_INTEGER;
         value.u.integer = 64;
         break;
      case RDDI_MEMOVERSION:
         value.nType = X_IT_INTEGER;
         value.u.integer = DB_MEMOVER_STD;
         break;
      case RDDI_MEMOREADLOCK:
         value.nType = X_IT_LOGICAL;
         value.u.logical = 1;
         break;
      case RDDI_CONNECTION:
         /* Only the local connection 0 exists */
         value.nType = X_IT_INTEGER;
         value.u.integer = 0;
         break;
      case RDDI_SETHEADER:
         value.nType = X_IT_INTEGER;
         value.u.integer = driver->setHeader;
         break;
      case RDDI_MEMOREUSE:
         value.nType = X_IT_LOGICAL;
         value.u.logical = 1;
         break;
      case RDDI_MEMOGCTYPE:
         value.nType = X_IT_INTEGER;
         value.u.integer = 0;
         break;
      case RDDI_LOCKRETRY:
         value.nType = X_IT_INTEGER;
         value.u.integer = driver->lockRetry;
         break;
      case RDDI_AUTOORDER:
         value.nType = X_IT_INTEGER;
         value.u.integer = 0;
         break;
      case RDDI_INDEXPAGESIZE:
         value.nType = X_IT_INTEGER;
         value.u.integer = IIF( cdx, 512, IIF( ntx, 1024, 0 ) );
         break;
      default: return X_DBF_UNSUPPORTED_OPERATION;
   }
   x_itemCopy( result, &value );
   return X_DBF_OK;
}

X_DBF_STATUS x_dbfAreaInfo( DBF area, int selector, X_ITEM *result )
{
   X_ITEM value = X_ITEM_NIL_INIT;

   if( selector == DBI_BOF || selector == DBI_EOF || selector == DBI_FOUND || selector == DBI_POSITIONED )
   {
      X_DBF_STATUS status = x_dbfAreaPrepare( area );
      if( status != X_DBF_OK )
         return status;
   }
   switch( selector )
   {
      case DBI_MEMOPACK:
      {
         X_DBF_STATUS status = x_dbfAreaPackMemo( area );
         if( status == X_DBF_OK )
            x_itemClear( result );
         return status;
      }
      case DBI_ISDBF:
      case DBI_CANPUTREC:
         value.nType = X_IT_LOGICAL;
         value.u.logical = 1;
         break;
      case DBI_GETRECSIZE:
         value.nType = X_IT_INTEGER;
         value.u.integer = ( int64_t )area->schema->recordLength;
         break;
      case DBI_TRANSREC:
      case DBI_DIRTYREAD:
      case DBI_RM_SUPPORTED:
         value.nType = X_IT_LOGICAL;
         value.u.logical = 0;
         break;
      case DBI_CHILDCOUNT:
      {
         const X_DBF_RELATION *relation;
         value.nType = X_IT_INTEGER;
         value.u.integer = 0;
         for( relation = area->relations; relation != NULL; relation = relation->next )
            ++value.u.integer;
         break;
      }
      case DBI_DBFILTER: return x_dbfAreaFilterText( area, result );
      case DBI_FCOUNT:
         value.nType = X_IT_INTEGER;
         value.u.integer = ( int64_t )area->schema->count;
         break;
      case DBI_BOF:
         value.nType = X_IT_LOGICAL;
         value.u.logical = area->state.bof;
         break;
      case DBI_EOF:
         value.nType = X_IT_LOGICAL;
         value.u.logical = area->state.eof;
         break;
      case DBI_FOUND:
         value.nType = X_IT_LOGICAL;
         value.u.logical = area->state.found;
         break;
      case DBI_POSITIONED:
         value.nType = X_IT_LOGICAL;
         value.u.logical = !area->state.eof;
         break;
      case DBI_ISREADONLY:
         value.nType = X_IT_LOGICAL;
         value.u.logical = area->readOnly;
         break;
      case DBI_SHARED:
         value.nType = X_IT_LOGICAL;
         value.u.logical = area->shared;
         break;
      case DBI_ALIAS: s_setString( result, area->alias, strlen( area->alias ) ); return X_DBF_OK;
      case DBI_RDD_VERSION:
      {
         char text[X_DBF_MAX_DRIVER_NAME + 16];
         int  length = snprintf( text, sizeof( text ), "xDev RDD (%s)", area->driver->name );
         s_setString( result, text, ( size_t )length );
         return X_DBF_OK;
      }
      default:
         if( area->driver->operations.info == NULL )
            return X_DBF_UNSUPPORTED_OPERATION;
         return area->driver->operations.info( area, selector, result );
   }
   x_itemCopy( result, &value );
   return X_DBF_OK;
}

X_DBF_STATUS x_dbfFieldInfo( DBF area, size_t ordinal, int selector, X_ITEM *result )
{
   const X_DBF_FIELD *field = x_dbfSchemaField( area->schema, ordinal );
   X_ITEM             value = X_ITEM_NIL_INIT;

   if( field == NULL )
      return X_DBF_INVALID_ARGUMENT;
   switch( selector )
   {
      case DBS_NAME: s_setString( result, field->name, strlen( field->name ) ); return X_DBF_OK;
      case DBS_TYPE:
      {
         char type = ( char )field->type;
         s_setString( result, &type, 1 );
         return X_DBF_OK;
      }
      case DBS_LEN:
         value.nType = X_IT_INTEGER;
         value.u.integer = field->length;
         break;
      case DBS_DEC:
         value.nType = X_IT_INTEGER;
         value.u.integer = field->decimals;
         break;
      case DBS_ISNULL:
         value.nType = X_IT_LOGICAL;
         value.u.logical = 0;
         break;
      default: return X_DBF_UNSUPPORTED_OPERATION;
   }
   x_itemCopy( result, &value );
   return X_DBF_OK;
}

static X_DBF_STATUS s_rawMemos( DBF area, int includeRecord, X_ITEM *result )
{
   X_DBF_TEXT combined __attribute__( ( cleanup( s_textDrop ) ) ) = { NULL, 0 };
   X_ITEM     memo __attribute__( ( cleanup( x_itemClear ) ) ) = X_ITEM_NIL_INIT;
   size_t     i;
   combined.length = includeRecord ? area->schema->recordLength : 0;
   combined.data = ( char * )x_xgrab( combined.length + 1 );
   if( combined.length != 0 )
      memcpy( combined.data, area->record, combined.length );
   for( i = 0; i < area->schema->count; ++i )
   {
      const X_DBF_FIELD *field = &area->schema->fields[i];
      X_DBF_STATUS       status;
      size_t             length;
      if( field->type != X_DBF_MEMO && field->type != X_DBF_PICTURE )
         continue;
      status = x_dbfFieldGetValue( area, i + 1, &memo );
      if( status != X_DBF_OK )
         return status;
      if( memo.nType != X_IT_STRING || memo.u.string.len == 0 )
         continue;
      length = memo.u.string.len;
      if( length > PTRDIFF_MAX - 1 || combined.length > PTRDIFF_MAX - 1 - length )
         return X_DBF_TEXT_OVERFLOW;
      combined.data = ( char * )x_xrealloc( combined.data, combined.length + length + 1 );
      memcpy( combined.data + combined.length, memo.u.string.data, length );
      combined.length += length;
   }
   combined.data[combined.length] = '\0';
   x_itemPutStringBuffer( result, combined.data, combined.length );
   combined.data = NULL;
   return X_DBF_OK;
}

X_DBF_STATUS x_dbfRecordInfo( DBF area, uint64_t record, int selector, X_ITEM *result )
{
   X_DBF_INFO_GUARD guard __attribute__( ( cleanup( s_restoreRecord ) ) ) = { area, 0, X_DBF_OK };
   X_ITEM           value __attribute__( ( cleanup( x_itemClear ) ) ) = X_ITEM_NIL_INIT;
   X_DBF_STATUS     status;
   uint64_t         previous;

   if( selector < DBRI_DELETED || selector > DBRI_RAWDATA )
      return X_DBF_UNSUPPORTED_OPERATION;
   status = x_dbfAreaPrepare( area );
   if( status != X_DBF_OK )
      return status;
   previous = area->state.record;
   if( record == 0 )
      record = previous;
   if( record != previous && ( selector == DBRI_DELETED || selector == DBRI_ENCRYPTED || selector == DBRI_RAWRECORD ||
                               selector == DBRI_RAWMEMOS || selector == DBRI_RAWDATA ) )
   {
      status = x_dbfAreaGoTo( area, record );
      if( status != X_DBF_OK )
         return status;
      guard.record = previous;
   }
   switch( selector )
   {
      case DBRI_DELETED:
         value.nType = X_IT_LOGICAL;
         value.u.logical = area->state.deleted;
         break;
      case DBRI_LOCKED:
      {
         X_ITEM locks __attribute__( ( cleanup( x_itemClear ) ) ) = X_ITEM_NIL_INIT;
         size_t i, count;
         value.nType = X_IT_LOGICAL;
         value.u.logical = !area->shared;
         if( area->shared )
         {
            guard.status = x_dbfAreaInfo( area, DBI_GETLOCKARRAY, &locks );
            if( guard.status != X_DBF_OK )
               break;
            count = ( size_t )x_arrayLen( &locks );
            for( i = 1; i <= count; ++i )
            {
               if( ( uint64_t )x_arrayGetNInt( &locks, i ) == record )
               {
                  value.u.logical = 1;
                  break;
               }
            }
         }
         break;
      }
      case DBRI_RECSIZE:
         value.nType = X_IT_INTEGER;
         value.u.integer = ( int64_t )area->schema->recordLength;
         break;
      case DBRI_RECNO:
         if( record > INT64_MAX )
            guard.status = X_DBF_INVALID_RECORD;
         else
         {
            value.nType = X_IT_INTEGER;
            value.u.integer = ( int64_t )record;
         }
         break;
      case DBRI_ENCRYPTED:
         value.nType = X_IT_LOGICAL;
         value.u.logical = 0;
         break;
      case DBRI_UPDATED:
         value.nType = X_IT_LOGICAL;
         value.u.logical = record == area->state.record && area->state.dirty;
         break;
      case DBRI_RAWRECORD: s_setString( &value, area->record, area->schema->recordLength ); break;
      case DBRI_RAWMEMOS:
      case DBRI_RAWDATA: guard.status = s_rawMemos( area, selector == DBRI_RAWDATA, &value ); break;
   }
   s_restoreRecord( &guard );
   if( guard.status == X_DBF_OK )
      x_itemCopy( result, &value );
   return guard.status;
}
