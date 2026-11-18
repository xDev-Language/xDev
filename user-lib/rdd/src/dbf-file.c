/*
 * Copyright (c) 2024-2026 xDev-Language
 *
 * Library: xDev 7.0.0 RDD
 * File: dbf-file.c
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "dbf-internal.h"

#include <xdev/dbinfo.ch>
#include <xdev/fileio.ch>
#include "platform.h"
#include <string.h>

/* Platforms where tables can be created and opened. Linux support is planned for the near future:
   once file locking and the file API are checked there, this becomes ( X_OS_WINDOWS || X_OS_LINUX ) */
#define X_DBF_PLATFORM X_OS_WINDOWS

typedef struct _X_DBF_RECORD_LOCK
{
   struct _X_DBF_RECORD_LOCK *next;
   uint64_t                   record;
} X_DBF_RECORD_LOCK;

typedef struct
{
   X_ITEM             file;
   X_DBF_MEMO_FILE   *memo;
   X_DBF_RECORD_LOCK *locks;
   char              *path;
   char              *temporary;
   unsigned char      header[32];
   uint16_t           headerLength;
   uint16_t           recordLength;
   uint32_t           recordCount;
   int                shared;
   int                readOnly;
   int                closed;
   int                poisoned;
   int                fileLocked;
   int                keepTemporary;
   int                lockScheme;
   int                lockRetry;
   int                setHeader;
   int                headerPending;
   int                lockDirection;
   int                outerHeader;
   int64_t            lockBase;
   int64_t            lockLength;
} X_DBF_FILE;

typedef struct
{
   X_DBF_FILE *file;
   int         held;
} X_DBF_HEADER_LOCK;

static void s_lockParameters( X_DBF_FILE *file )
{
   file->lockDirection = 1;
   switch( file->lockScheme )
   {
      case DB_DBFLOCK_CLIPPER:
      case DB_DBFLOCK_COMIX:
         file->lockBase = INT64_C( 1000000000 );
         file->lockLength = INT64_C( 1000000000 );
         break;
      case DB_DBFLOCK_CLIPPER2:
      case DB_DBFLOCK_HB32:
         file->lockBase = INT64_C( 4000000000 );
         file->lockLength = INT64_C( 294967295 );
         break;
      case DB_DBFLOCK_HB64:
         file->lockBase = INT64_C( 0x7F00000000000000 );
         file->lockLength = INT64_C( 0xFFFFFFFE );
         break;
      case DB_DBFLOCK_VFP:
         if( file->header[28] & 1 )
         {
            file->lockBase = INT64_C( 0x7FFFFFFE );
            file->lockLength = INT64_C( 0x07FFFFFF );
            file->lockDirection = -1;
         }
         else
         {
            file->lockBase = INT64_C( 0x40000000 );
            file->lockLength = INT64_C( 0x3FFFFFFD );
            file->lockDirection = 2;
         }
         break;
   }
}

static int64_t s_fileLockOffset( const X_DBF_FILE *file )
{
   return file->lockBase + IIF( file->lockDirection < 0, -file->lockLength, 1 );
}

/* Lock retried every 10 ms for up to wait milliseconds. Record and file locks wait RDDI_LOCKRETRY;
   the short internal locks of the table header, index and memo wait X_DBF_LOCK_WAIT */
int x_dbfLockWait( const X_ITEM *file, int64_t offset, int64_t length, int type, int wait )
{
   int waited = 0;
   while( !x_fileLock( file, offset, length, type | FLX_NO_WAIT, 0 ) )
   {
      if( waited >= wait )
         return 0;
      x_sysSleep( 0.01 );
      waited += 10;
   }
   return 1;
}

static int64_t s_dataLimit( const X_DBF_FILE *file )
{
   return IIF( file->lockDirection == 2, file->lockLength,
               IIF( file->lockDirection < 0, file->lockBase - file->lockLength, file->lockBase ) );
}

static uint16_t s_get16( const unsigned char *data )
{
   return ( uint16_t )( data[0] | ( ( uint16_t )data[1] << 8 ) );
}

static void s_put16( unsigned char *data, uint16_t value )
{
   data[0] = ( unsigned char )value;
   data[1] = ( unsigned char )( value >> 8 );
}

static void s_put32( unsigned char *data, uint32_t value )
{
   data[0] = ( unsigned char )value;
   data[1] = ( unsigned char )( value >> 8 );
   data[2] = ( unsigned char )( value >> 16 );
   data[3] = ( unsigned char )( value >> 24 );
}

static void s_date( unsigned char *header )
{
   int year, month, day;

   x_dateToday( &year, &month, &day );
   header[1] = ( unsigned char )( year - 1900 );
   header[2] = ( unsigned char )month;
   header[3] = ( unsigned char )day;
}

static X_DBF_STATUS s_readAt( X_DBF_FILE *file, void *buffer, size_t length, int64_t offset )
{
   size_t done = 0;

   while( done < length )
   {
      int64_t bytes = x_fileReadAt( &file->file, ( char * )buffer + done, length - done, offset + ( int64_t )done );
      if( bytes <= 0 || ( uint64_t )bytes > length - done )
         return X_DBF_READ_ERROR;
      done += ( size_t )bytes;
   }
   return X_DBF_OK;
}

static X_DBF_STATUS s_writeAt( X_DBF_FILE *file, const void *buffer, size_t length, int64_t offset )
{
   size_t done = 0;

   while( done < length )
   {
      int64_t bytes =
         x_fileWriteAt( &file->file, ( const char * )buffer + done, length - done, offset + ( int64_t )done );
      if( bytes <= 0 || ( uint64_t )bytes > length - done )
         return X_DBF_WRITE_ERROR;
      done += ( size_t )bytes;
   }
   return X_DBF_OK;
}

static int64_t s_recordOffset( const X_DBF_FILE *file, uint64_t record )
{
   return file->headerLength + ( int64_t )( record - 1 ) * file->recordLength;
}

static int64_t s_recordLockOffset( const X_DBF_FILE *file, uint64_t record )
{
   if( file->lockDirection == 2 )
      return file->lockBase + s_recordOffset( file, record );
   return file->lockBase + IIF( file->lockDirection < 0, -( int64_t )record, ( int64_t )record );
}

static X_DBF_STATUS s_ready( const X_DBF_FILE *file )
{
   if( file == NULL || file->closed )
      return X_DBF_INVALID_STATE;
   return file->poisoned ? X_DBF_RECOVERY_REQUIRED : X_DBF_OK;
}

static void s_headerDrop( X_DBF_HEADER_LOCK *lock )
{
   if( lock->held )
   {
      int error = x_fsError();
      if( !x_fileLock( &lock->file->file, lock->file->lockBase, 1, FLX_EXCLUSIVE, 1 ) )
         lock->file->poisoned = 1;
      x_fsSetError( error );
   }
}

static X_DBF_STATUS s_headerEnter( X_DBF_FILE *file, X_DBF_HEADER_LOCK *lock )
{
   X_DBF_STATUS status = s_ready( file );

   if( status != X_DBF_OK )
      return status;
   lock->file = file;
   if( file->shared && !file->outerHeader )
   {
      if( !x_dbfLockWait( &file->file, file->lockBase, 1, FLX_EXCLUSIVE, X_DBF_LOCK_WAIT ) )
         return X_DBF_LOCK_FAILED;
      lock->held = 1;
   }
   return X_DBF_OK;
}

static X_DBF_STATUS s_headerLeave( X_DBF_HEADER_LOCK *lock, X_DBF_STATUS status )
{
   if( lock->held )
   {
      int error = x_fsError();
      if( !x_fileLock( &lock->file->file, lock->file->lockBase, 1, FLX_EXCLUSIVE, 1 ) )
      {
         lock->file->poisoned = 1;
         if( status == X_DBF_OK )
            return X_DBF_LOCK_FAILED;
      }
      else
         lock->held = 0;
      if( status != X_DBF_OK )
         x_fsSetError( error );
   }
   return status;
}

static int s_xdev7( unsigned char version )
{
   /* xDev 7: 0x04 without memo, 0x8C with a DBT memo */
   return version == 0x04 || version == 0x8C;
}

static X_DBF_STATUS s_headerValidate( const unsigned char *header )
{
   if( ( header[0] != 0x03 && header[0] != 0x83 && header[0] != 0xF5 && header[0] != 0xE5 && !s_xdev7( header[0] ) ) ||
       header[15] != 0 || ( header[28] & ~1 ) != 0 )
      return X_DBF_UNSUPPORTED_FORMAT;
   if( s_get16( header + 8 ) < ( s_xdev7( header[0] ) ? 117 : 65 ) || s_get16( header + 10 ) < 2 )
      return X_DBF_CORRUPT_FILE;
   return X_DBF_OK;
}

static X_DBF_STATUS s_refresh( X_DBF_FILE *file, int complete )
{
   unsigned char header[32];
   int64_t       size, payload, records, remainder;
   unsigned char end;
   X_DBF_STATUS  status = s_readAt( file, header, sizeof( header ), 0 );

   if( status != X_DBF_OK )
      return status;
   status = s_headerValidate( header );
   if( status != X_DBF_OK )
      return status;
   if( s_get16( header + 8 ) != file->headerLength || s_get16( header + 10 ) != file->recordLength ||
       header[29] != file->header[29] || header[0] != file->header[0] )
      return X_DBF_CORRUPT_FILE;
   if( file->shared && file->lockScheme == DB_DBFLOCK_VFP && header[28] != file->header[28] )
      return X_DBF_INVALID_STATE;
   size = x_fileSize( &file->file, 0 );
   if( size < 0 )
      return X_DBF_READ_ERROR;
   if( size < file->headerLength )
      return X_DBF_CORRUPT_FILE;
   if( size >= s_dataLimit( file ) )
      return X_DBF_UNSUPPORTED_FORMAT;
   payload = size - file->headerLength;
   records = payload / file->recordLength;
   remainder = payload % file->recordLength;
   if( records > UINT32_MAX )
      return X_DBF_CORRUPT_FILE;
   if( complete && remainder > 1 )
      return X_DBF_CORRUPT_FILE;
   if( complete && remainder == 1 )
   {
      status = s_readAt( file, &end, 1, size - 1 );
      if( status != X_DBF_OK )
         return status;
      if( end != 0x1A )
         return X_DBF_CORRUPT_FILE;
   }
   memcpy( file->header, header, sizeof( header ) );
   file->recordCount = ( uint32_t )records;
   return X_DBF_OK;
}

static void s_bufferDrop( X_DBF_TEXT *buffer )
{
   x_dbfTextClear( buffer );
}

static void s_fileDrop( X_DBF_FILE **owned )
{
   X_DBF_FILE *file = *owned;
   int         error = x_fsError();

   if( file != NULL )
   {
      x_dbfMemoDispose( file->memo );
      if( file->file.nType != X_IT_NIL )
      {
         if( !file->closed )
            x_fileClose( &file->file );
         x_gcUnlock( file->file.u.pPointer );
         x_itemClear( &file->file );
      }
      while( file->locks != NULL )
      {
         X_DBF_RECORD_LOCK *entry = file->locks;
         file->locks = entry->next;
         x_xfree( entry );
      }
      if( file->path != NULL )
         x_xfree( file->path );
      if( file->temporary != NULL )
      {
         if( !file->keepTemporary )
            x_fsDelete( file->temporary );
         x_xfree( file->temporary );
      }
      x_xfree( file );
      *owned = NULL;
   }
   x_fsSetError( error );
}

static X_DBF_FILE *s_fileNew( void *handle, int shared, int readOnly, int scheme )
{
   X_DBF_FILE *file = ( X_DBF_FILE * )x_xgrab( sizeof( *file ) );

   memset( file, 0, sizeof( *file ) );
   x_itemPutPtrGC( &file->file, handle );
   x_gcLock( handle );
   file->shared = shared;
   file->readOnly = readOnly;
   file->lockScheme = scheme;
   s_lockParameters( file );
   return file;
}

/* The memo type comes from the version byte of the table, whatever driver opens it */
static int s_headerMemo( unsigned char version )
{
   return version == 0x83 || version == 0x8C
             ? DB_MEMO_DBT
             : ( version == 0xF5 ? DB_MEMO_FPT : ( version == 0xE5 ? DB_MEMO_SMT : DB_MEMO_NONE ) );
}

static X_DBF_STATUS s_memoPath( const char *path, int kind, X_DBF_TEXT *result )
{
   const char *base = path, *position, *extension;
   const char *suffix = kind == DB_MEMO_DBT ? ".dbt" : ( kind == DB_MEMO_FPT ? ".fpt" : ".smt" );
   size_t      length;
   for( position = path; *position; ++position )
      if( *position == '/' || *position == '\\' || *position == ':' )
         base = position + 1;
   extension = strrchr( base, '.' );
   length = extension != NULL ? ( size_t )( extension - path ) : strlen( path );
   result->data = ( char * )x_xgrab( length + 5 );
   memcpy( result->data, path, length );
   memcpy( result->data + length, suffix, 5 );
   result->length = length + 4;
   return x_stricmp( result->data, path ) == 0 ? X_DBF_INVALID_ARGUMENT : X_DBF_OK;
}

static X_DBF_STATUS s_create( const X_DBF_CREATE_OPTIONS *options, const X_DBF_SCHEMA *schema )
{
   X_DBF_FILE *file __attribute__( ( cleanup( s_fileDrop ) ) ) = NULL;
   X_DBF_TEXT  buffer __attribute__( ( cleanup( s_bufferDrop ) ) ) = { NULL, 0 };
   X_DBF_TEXT  memoPath __attribute__( ( cleanup( s_bufferDrop ) ) ) = { NULL, 0 };
   /* New files are always xDev 7: 68-byte header, 48-byte descriptors, DBT memo */
   size_t       headerLength = 69 + schema->count * 48;
   size_t       i;
   X_DBF_STATUS status;
   void        *handle;
   int          kind = DB_MEMO_DBT, hasMemo = 0;

#if !X_DBF_PLATFORM
   return X_DBF_UNSUPPORTED_OPERATION;
#endif
   if( headerLength > UINT16_MAX )
      return X_DBF_INVALID_SCHEMA;
   for( i = 0; i < schema->count; ++i )
   {
      const X_DBF_FIELD *field = &schema->fields[i];
      if( field->type == X_DBF_MEMO || field->type == X_DBF_PICTURE )
      {
         if( field->length != 10 )
            return X_DBF_UNSUPPORTED_FORMAT;
         hasMemo = 1;
      }
   }
   if( hasMemo )
   {
      status = s_memoPath( options->path, kind, &memoPath );
      if( status != X_DBF_OK )
         return status;
   }
   buffer.data = ( char * )x_xgrab( headerLength + 1 );
   memset( buffer.data, 0, headerLength + 1 );
   buffer.data[0] = ( char )( hasMemo ? 0x8C : 0x04 );
   s_date( ( unsigned char * )buffer.data );
   s_put16( ( unsigned char * )buffer.data + 8, ( uint16_t )headerLength );
   s_put16( ( unsigned char * )buffer.data + 10, ( uint16_t )schema->recordLength );
   buffer.data[29] = 0; /* xDev 7 stores UTF-8 exclusively */
   for( i = 0; i < schema->count; ++i )
   {
      const X_DBF_FIELD *field = &schema->fields[i];
      unsigned char     *descriptor = ( unsigned char * )buffer.data + 68 + i * 48;

      memcpy( descriptor, field->name, strlen( field->name ) );
      /* Binary memo (P) is stored with the xDev 7 type B */
      descriptor[32] = ( unsigned char )IIF( field->type == X_DBF_PICTURE, 'B', field->type );
      descriptor[33] = ( unsigned char )field->length;
      descriptor[34] = field->decimals;
   }
   buffer.data[68 + schema->count * 48] = 0x0D;
   buffer.data[headerLength] = 0x1A;
   handle = x_fileCreate( options->path, FC_NORMAL );
   if( handle == NULL )
      return X_DBF_CREATE_ERROR;
   file = s_fileNew( handle, 0, 0, DB_DBFLOCK_CLIPPER );
   status = s_writeAt( file, buffer.data, headerLength + 1, 0 );
   if( status == X_DBF_OK && !x_fileCommit( &file->file ) )
      status = X_DBF_WRITE_ERROR;
   if( status == X_DBF_OK && hasMemo )
      status = x_dbfMemoCreate( memoPath.data, kind, 1, &file->memo );
   if( status != X_DBF_OK )
   {
      file->poisoned = 1;
      return status;
   }
   file->closed = 1;
   status = x_dbfMemoClose( file->memo );
   if( !x_fileClose( &file->file ) )
      status = X_DBF_CLOSE_ERROR;
   return status;
}

static X_DBF_STATUS s_open( DBF area, const X_DBF_OPEN_OPTIONS *options )
{
   X_DBF_TEXT        buffer __attribute__( ( cleanup( s_bufferDrop ) ) ) = { NULL, 0 };
   X_DBF_TEXT        specifications __attribute__( ( cleanup( s_bufferDrop ) ) ) = { NULL, 0 };
   X_DBF_TEXT        names __attribute__( ( cleanup( s_bufferDrop ) ) ) = { NULL, 0 };
   X_DBF_TEXT        memoPath __attribute__( ( cleanup( s_bufferDrop ) ) ) = { NULL, 0 };
   X_DBF_HEADER_LOCK guard __attribute__( ( cleanup( s_headerDrop ) ) ) = { NULL, 0 };
   X_DBF_FILE       *file;
   X_DBF_FIELD_SPEC *fields;
   X_DBF_STATUS      status;
   size_t count = 0, capacity, position, i, start, descriptorSize, nameBytes, nameSize, typeOffset, lengthOffset;
   void  *handle;
   int    kind;

#if !X_DBF_PLATFORM
   return X_DBF_UNSUPPORTED_OPERATION;
#endif
   handle = x_fileOpen( options->path, ( options->readOnly ? FO_READ : FO_READWRITE ) |
                                          ( options->shared ? FO_DENYNONE : FO_EXCLUSIVE ) );
   if( handle == NULL )
      return X_DBF_OPEN_ERROR;
   file = s_fileNew( handle, options->shared != 0, options->readOnly != 0, area->lockScheme );
   file->lockRetry = area->driver->lockRetry;
   file->setHeader = area->driver->setHeader;
   x_dbfAreaSetStorage( area, file );
   file->path = x_fileDefName( options->path );
   status = s_readAt( file, file->header, sizeof( file->header ), 0 );
   if( status != X_DBF_OK )
      return status;
   s_lockParameters( file );
   status = s_headerEnter( file, &guard );
   if( status != X_DBF_OK )
      return status;
   status = s_readAt( file, file->header, sizeof( file->header ), 0 );
   if( status != X_DBF_OK )
      return status;
   status = s_headerValidate( file->header );
   if( status != X_DBF_OK )
      return status;
   area->structuralIndex = strcmp( options->driver, "DBFCDX" ) == 0 && ( file->header[28] & 1 );
   if( area->structuralIndex && options->indexBindings == NULL )
      return X_DBF_INDEX_EXPRESSION_REQUIRED;
   kind = s_headerMemo( file->header[0] );
   if( file->header[29] != 0 )
      return X_DBF_UNSUPPORTED_FORMAT;
   file->headerLength = s_get16( file->header + 8 );
   file->recordLength = s_get16( file->header + 10 );
   status = s_refresh( file, 1 );
   if( status != X_DBF_OK )
      return status;
   /* Descriptor layout depends on the version: classic 32 bytes from 32, xDev 7 48 bytes from 68 */
   if( s_xdev7( file->header[0] ) )
   {
      start = 68;
      descriptorSize = 48;
      nameBytes = nameSize = 32;
      typeOffset = 32;
      lengthOffset = 33;
   }
   else
   {
      start = 32;
      descriptorSize = 32;
      nameBytes = 11;
      nameSize = 10;
      typeOffset = 11;
      lengthOffset = 16;
   }
   buffer.length = file->headerLength - start;
   buffer.data = ( char * )x_xgrab( buffer.length );
   status = s_readAt( file, buffer.data, buffer.length, start );
   if( status != X_DBF_OK )
      return status;
   capacity = ( buffer.length - 1 ) / descriptorSize;
   specifications.data = ( char * )x_xgrab( capacity * sizeof( X_DBF_FIELD_SPEC ) );
   names.data = ( char * )x_xgrab( capacity * ( X_DBF_MAX_FIELD_NAME + 1 ) );
   fields = ( X_DBF_FIELD_SPEC * )specifications.data;
   position = 0;
   while( position < buffer.length && ( unsigned char )buffer.data[position] != 0x0D )
   {
      unsigned char *descriptor = ( unsigned char * )buffer.data + position;
      char          *name = names.data + count * ( X_DBF_MAX_FIELD_NAME + 1 );
      size_t         nameLength = 0;

      if( count >= capacity || buffer.length - position < descriptorSize + 1 )
         return X_DBF_CORRUPT_FILE;
      while( nameLength < nameBytes && descriptor[nameLength] != 0 )
         ++nameLength;
      if( nameLength == 0 || nameLength > nameSize )
         return X_DBF_CORRUPT_FILE;
      if( descriptorSize == 32 && descriptor[18] != 0 )
         return X_DBF_UNSUPPORTED_FORMAT;
      memcpy( name, descriptor, nameLength );
      name[nameLength] = '\0';
      fields[count].name = name;
      fields[count].type = ( X_DBF_FIELD_TYPE )descriptor[typeOffset];
      if( descriptorSize == 48 && fields[count].type == X_DBF_PICTURE )
         return X_DBF_UNSUPPORTED_FORMAT;
      if( descriptorSize == 48 && descriptor[typeOffset] == 'B' )
         fields[count].type = X_DBF_PICTURE;
      if( fields[count].type != X_DBF_CHARACTER && fields[count].type != X_DBF_NUMERIC &&
          fields[count].type != X_DBF_DATE && fields[count].type != X_DBF_LOGICAL && fields[count].type != X_DBF_MEMO &&
          fields[count].type != X_DBF_PICTURE )
         return X_DBF_UNSUPPORTED_FORMAT;
      fields[count].length = descriptor[lengthOffset];
      fields[count].decimals = descriptor[lengthOffset + 1];
      if( fields[count].type == X_DBF_MEMO || fields[count].type == X_DBF_PICTURE )
      {
         if( kind == DB_MEMO_NONE ||
             ( kind != DB_MEMO_FPT &&
               ( fields[count].length != 10 || ( fields[count].type == X_DBF_PICTURE && descriptorSize == 32 ) ) ) )
            return X_DBF_UNSUPPORTED_FORMAT;
      }
      ++count;
      position += descriptorSize;
   }
   if( position >= buffer.length || count == 0 )
      return X_DBF_CORRUPT_FILE;
   for( i = position + 1; i < buffer.length; ++i )
   {
      if( buffer.data[i] != 0 )
         return X_DBF_UNSUPPORTED_FORMAT;
   }
   status = x_dbfAreaDefine( area, fields, count, file->recordCount );
   if( status != X_DBF_OK )
      return status == X_DBF_INVALID_SCHEMA ? X_DBF_UNSUPPORTED_FORMAT : status;
   if( area->schema->recordLength != file->recordLength )
      return X_DBF_CORRUPT_FILE;
   if( kind != DB_MEMO_NONE )
   {
      status = s_memoPath( options->path, kind, &memoPath );
      if( status == X_DBF_OK )
         status = x_dbfMemoOpen( memoPath.data, kind, s_xdev7( file->header[0] ), options->shared, options->readOnly,
                                 &file->memo );
      if( status != X_DBF_OK )
         return status;
   }
   return s_headerLeave( &guard, X_DBF_OK );
}

X_DBF_STATUS x_dbfFileIndexFlag( DBF area, int present )
{
   X_DBF_FILE       *file = ( X_DBF_FILE * )area->storage;
   X_DBF_HEADER_LOCK guard __attribute__( ( cleanup( s_headerDrop ) ) ) = { NULL, 0 };
   X_DBF_STATUS      status;
   if( file->shared && ( file->header[28] & 1 ) != ( present != 0 ) )
      return X_DBF_SHARED_OPERATION;
   status = s_headerEnter( file, &guard );
   if( status == X_DBF_OK )
   {
      file->header[28] = ( unsigned char )present;
      status = s_writeAt( file, file->header + 28, 1, 28 );
      if( status == X_DBF_OK && !x_fileCommit( &file->file ) )
         status = X_DBF_WRITE_ERROR;
      status = s_headerLeave( &guard, status );
      if( status == X_DBF_OK )
         s_lockParameters( file );
   }
   return status;
}

X_DBF_STATUS x_dbfFileLockScheme( DBF area, int scheme )
{
   X_DBF_FILE *file = ( X_DBF_FILE * )area->storage;
   if( scheme < DB_DBFLOCK_CLIPPER || scheme > DB_DBFLOCK_CLIPPER2 )
      return X_DBF_INVALID_ARGUMENT;
   if( file->locks != NULL || file->fileLocked || area->indexes != NULL || file->outerHeader )
      return X_DBF_INVALID_STATE;
   file->lockScheme = area->lockScheme = scheme;
   s_lockParameters( file );
   return X_DBF_OK;
}

X_DBF_STATUS x_dbfFileEnter( DBF area, X_DBF_FILE_GUARD *guard )
{
   X_DBF_FILE *file;
   if( area->driver->operations.write != x_dbfFileDriver.write )
      return X_DBF_OK;
   file = ( X_DBF_FILE * )area->storage;
   guard->storage = file;
   if( file->shared && !file->outerHeader )
   {
      if( !x_dbfLockWait( &file->file, file->lockBase, 1, FLX_EXCLUSIVE, X_DBF_LOCK_WAIT ) )
         return X_DBF_LOCK_FAILED;
      file->outerHeader = guard->held = 1;
   }
   return X_DBF_OK;
}

void x_dbfFileLeave( X_DBF_FILE_GUARD *guard )
{
   if( guard->held )
   {
      X_DBF_FILE *file = ( X_DBF_FILE * )guard->storage;
      int         error = x_fsError();
      if( !x_fileLock( &file->file, file->lockBase, 1, FLX_EXCLUSIVE, 1 ) )
         file->poisoned = 1;
      file->outerHeader = guard->held = 0;
      x_fsSetError( error );
   }
}

static X_DBF_STATUS s_headerFlush( X_DBF_FILE *file );

static X_DBF_STATUS s_close( DBF area )
{
   X_DBF_FILE  *file = ( X_DBF_FILE * )x_dbfAreaStorage( area );
   X_DBF_STATUS status = X_DBF_OK, memoStatus;

   if( file == NULL || file->closed )
      return X_DBF_INVALID_STATE;
   // A header left pending by DB_SETHEADER_* is always written on close
   if( file->headerPending )
   {
      status = s_headerFlush( file );
      if( status == X_DBF_OK && !x_fileCommit( &file->file ) )
         status = X_DBF_WRITE_ERROR;
   }
   file->closed = 1;
   memoStatus = x_dbfMemoClose( file->memo );
   if( status == X_DBF_OK )
      status = memoStatus;
   if( !x_fileClose( &file->file ) )
      status = X_DBF_CLOSE_ERROR;
   return status;
}

static void s_dispose( DBF area )
{
   X_DBF_FILE *file = ( X_DBF_FILE * )x_dbfAreaStorage( area );

   x_dbfAreaSetStorage( area, NULL );
   s_fileDrop( &file );
}

static X_DBF_STATUS s_sync( DBF area )
{
   X_DBF_FILE  *file = ( X_DBF_FILE * )x_dbfAreaStorage( area );
   X_DBF_STATUS status = s_ready( file );

   if( status != X_DBF_OK || file->readOnly )
      return status;
   if( file->setHeader & DB_SETHEADER_COMMIT )
   {
      status = s_headerFlush( file );
      if( status != X_DBF_OK )
         return status;
   }
   status = x_dbfMemoSync( file->memo );
   if( status != X_DBF_OK )
   {
      file->poisoned = 1;
      return status;
   }
   if( !x_fileCommit( &file->file ) )
   {
      file->poisoned = 1;
      return X_DBF_WRITE_ERROR;
   }
   return X_DBF_OK;
}

static X_DBF_STATUS s_count( DBF area, uint64_t *count )
{
   X_DBF_FILE  *file = ( X_DBF_FILE * )x_dbfAreaStorage( area );
   X_DBF_STATUS status = s_ready( file );

   /* An exclusive table changes only through this process, so its record count is already known */
   if( status == X_DBF_OK && file->shared )
      status = s_refresh( file, file->outerHeader );
   if( status == X_DBF_OK )
      *count = file->recordCount;
   return status;
}

static X_DBF_RECORD_LOCK *s_findLock( X_DBF_FILE *file, uint64_t record )
{
   X_DBF_RECORD_LOCK *entry;

   for( entry = file->locks; entry != NULL; entry = entry->next )
   {
      if( entry->record == record )
         return entry;
   }
   return NULL;
}

static X_DBF_STATUS s_checkWrite( DBF area, uint64_t record )
{
   X_DBF_FILE  *file = ( X_DBF_FILE * )x_dbfAreaStorage( area );
   X_DBF_STATUS status = s_ready( file );

   if( status != X_DBF_OK )
      return status;
   if( file->readOnly )
      return X_DBF_READ_ONLY;
   if( file->shared && !file->fileLocked && s_findLock( file, record ) == NULL )
      return X_DBF_RECORD_UNLOCKED;
   return X_DBF_OK;
}

static X_DBF_STATUS s_read( DBF area, uint64_t record, char *buffer, size_t length )
{
   X_DBF_FILE  *file = ( X_DBF_FILE * )x_dbfAreaStorage( area );
   X_DBF_STATUS status = s_ready( file );

   if( status != X_DBF_OK )
      return status;
   if( record == 0 || record > file->recordCount || length != file->recordLength )
      return X_DBF_INVALID_RECORD;
   return s_readAt( file, buffer, length, s_recordOffset( file, record ) );
}

// Byte 14 (incomplete transaction) is always cleared; a flag left by an older version must not block the table
static X_DBF_STATUS s_publish( X_DBF_FILE *file, uint32_t count )
{
   unsigned char header[32];
   X_DBF_STATUS  status;

   memcpy( header, file->header, sizeof( header ) );
   s_date( header );
   s_put32( header + 4, count );
   header[14] = 0;
   status = s_writeAt( file, header, sizeof( header ), 0 );
   if( status == X_DBF_OK )
   {
      memcpy( file->header, header, sizeof( header ) );
      file->recordCount = count;
   }
   return status;
}

// Writes a header left pending by DB_SETHEADER_*, with the record count taken again from the file size
static X_DBF_STATUS s_headerFlush( X_DBF_FILE *file )
{
   X_DBF_HEADER_LOCK guard __attribute__( ( cleanup( s_headerDrop ) ) ) = { NULL, 0 };
   X_DBF_STATUS      status;

   if( !file->headerPending || file->readOnly || file->poisoned )
      return X_DBF_OK;
   status = s_headerEnter( file, &guard );
   if( status == X_DBF_OK )
      status = s_refresh( file, 1 );
   if( status == X_DBF_OK )
      status = s_publish( file, file->recordCount );
   if( status == X_DBF_OK )
      file->headerPending = 0;
   return s_headerLeave( &guard, status );
}

// DB_SETHEADER_*: WRITE writes the header at once; otherwise it waits for DbCommit() (COMMIT) or close
static X_DBF_STATUS s_headerChanged( X_DBF_FILE *file, uint32_t count )
{
   if( file->setHeader & DB_SETHEADER_WRITE )
      return s_publish( file, count );
   file->recordCount = count;
   file->headerPending = 1;
   return X_DBF_OK;
}

static X_DBF_STATUS s_write( DBF area, uint64_t record, const char *buffer, size_t length )
{
   X_DBF_FILE       *file = ( X_DBF_FILE * )x_dbfAreaStorage( area );
   X_DBF_HEADER_LOCK guard __attribute__( ( cleanup( s_headerDrop ) ) ) = { NULL, 0 };
   X_DBF_STATUS      status = s_checkWrite( area, record );

   if( status != X_DBF_OK )
      return status;
   if( length != file->recordLength || ( buffer[0] != ' ' && buffer[0] != '*' ) )
      return X_DBF_INVALID_RECORD;
   status = s_headerEnter( file, &guard );
   if( status == X_DBF_OK && file->shared )
      status = s_refresh( file, 1 );
   if( status != X_DBF_OK )
      return s_headerLeave( &guard, status );
   if( record == 0 || record > file->recordCount )
      return s_headerLeave( &guard, X_DBF_INVALID_RECORD );
   // No forced write to disk here: other processes see the data at once, durability comes from DbCommit()
   status = s_writeAt( file, buffer, length, s_recordOffset( file, record ) );
   if( status == X_DBF_OK && ( file->setHeader & DB_SETHEADER_REPLACE ) )
      status = s_headerChanged( file, file->recordCount );
   if( status != X_DBF_OK )
      file->poisoned = 1;
   return s_headerLeave( &guard, status );
}

static X_DBF_STATUS s_lockRecord( X_DBF_FILE *file, uint64_t record )
{
   X_DBF_RECORD_LOCK *entry;

   if( !file->shared || file->fileLocked || s_findLock( file, record ) != NULL )
      return X_DBF_OK;
   entry = ( X_DBF_RECORD_LOCK * )x_xgrab( sizeof( *entry ) );
   if( !x_dbfLockWait( &file->file, s_recordLockOffset( file, record ), 1, FLX_EXCLUSIVE, file->lockRetry ) )
   {
      x_xfree( entry );
      return X_DBF_LOCK_FAILED;
   }
   entry->record = record;
   entry->next = NULL;
   {
      X_DBF_RECORD_LOCK **position = &file->locks;
      while( *position != NULL )
         position = &( *position )->next;
      *position = entry;
   }
   return X_DBF_OK;
}

// DBI_SETHEADER: returns the current header mode and sets a new one (a negative mode keeps it)
X_DBF_STATUS x_dbfFileSetHeader( DBF area, int mode, int *previous )
{
   X_DBF_FILE  *file;
   X_DBF_STATUS status;

   if( area->driver->operations.write != x_dbfFileDriver.write )
      return X_DBF_UNSUPPORTED_OPERATION;
   file = ( X_DBF_FILE * )area->storage;
   status = s_ready( file );
   if( status != X_DBF_OK )
      return status;
   *previous = file->setHeader;
   if( mode >= 0 )
      file->setHeader = mode;
   return X_DBF_OK;
}

// DBI_LOCKTEST: 1 when another process holds the record lock (record 0: the file lock), else 0
X_DBF_STATUS x_dbfFileLockTest( DBF area, uint64_t record, int *locked )
{
   X_DBF_FILE  *file;
   X_DBF_STATUS status;
   int64_t      offset, length;

   *locked = 0;
   if( area->driver->operations.write != x_dbfFileDriver.write )
      return X_DBF_UNSUPPORTED_OPERATION;
   file = ( X_DBF_FILE * )area->storage;
   status = s_ready( file );
   if( status != X_DBF_OK || !file->shared || file->fileLocked )
      return status;
   if( record != 0 )
   {
      if( s_findLock( file, record ) != NULL )
         return X_DBF_OK;
      offset = s_recordLockOffset( file, record );
      length = 1;
   }
   else
   {
      offset = s_fileLockOffset( file );
      length = file->lockLength;
   }
   if( !x_fileLock( &file->file, offset, length, FLX_EXCLUSIVE | FLX_NO_WAIT, 0 ) )
      *locked = 1;
   else if( !x_fileLock( &file->file, offset, length, FLX_EXCLUSIVE, 1 ) )
   {
      file->poisoned = 1;
      return X_DBF_LOCK_FAILED;
   }
   return X_DBF_OK;
}

static X_DBF_STATUS s_append( DBF area, const char *buffer, size_t length, uint64_t *record )
{
   X_DBF_FILE       *file = ( X_DBF_FILE * )x_dbfAreaStorage( area );
   X_DBF_HEADER_LOCK guard __attribute__( ( cleanup( s_headerDrop ) ) ) = { NULL, 0 };
   X_DBF_STATUS      status;
   uint64_t          next;
   unsigned char     end = 0x1A;
   int64_t           offset;

   if( file->readOnly )
      return X_DBF_READ_ONLY;
   if( length != file->recordLength || buffer[0] != ' ' )
      return X_DBF_INVALID_RECORD;
   status = s_headerEnter( file, &guard );
   if( status == X_DBF_OK && file->shared )
      status = s_refresh( file, 1 );
   if( status != X_DBF_OK )
      return s_headerLeave( &guard, status );
   next = ( uint64_t )file->recordCount + 1;
   offset = s_recordOffset( file, next );
   if( next > UINT32_MAX || offset + file->recordLength + 1 >= s_dataLimit( file ) ||
       ( file->lockDirection != 2 && next > ( uint64_t )file->lockLength ) )
      return s_headerLeave( &guard, X_DBF_FIELD_OVERFLOW );
   status = s_lockRecord( file, next );
   if( status != X_DBF_OK )
      return s_headerLeave( &guard, status );
   status = s_writeAt( file, buffer, length, offset );
   if( status == X_DBF_OK )
      status = s_writeAt( file, &end, 1, offset + file->recordLength );
   if( status == X_DBF_OK )
      status = s_headerChanged( file, ( uint32_t )next );
   if( status != X_DBF_OK )
      file->poisoned = 1;
   status = s_headerLeave( &guard, status );
   if( status == X_DBF_OK )
      *record = next;
   return status;
}

static X_DBF_STATUS s_unlock( DBF area, uint64_t record )
{
   X_DBF_FILE         *file = ( X_DBF_FILE * )x_dbfAreaStorage( area );
   X_DBF_RECORD_LOCK **position = &file->locks;

   while( *position != NULL )
   {
      X_DBF_RECORD_LOCK *entry = *position;
      if( record == 0 || entry->record == record )
      {
         if( !x_fileLock( &file->file, s_recordLockOffset( file, entry->record ), 1, FLX_EXCLUSIVE, 1 ) )
            return X_DBF_LOCK_FAILED;
         *position = entry->next;
         x_xfree( entry );
      }
      else
         position = &entry->next;
   }
   if( record == 0 && file->fileLocked )
   {
      if( !x_fileLock( &file->file, s_fileLockOffset( file ), file->lockLength, FLX_EXCLUSIVE, 1 ) )
         return X_DBF_LOCK_FAILED;
      file->fileLocked = 0;
   }
   return X_DBF_OK;
}

static X_DBF_STATUS s_lock( DBF area, uint64_t record )
{
   X_DBF_FILE  *file = ( X_DBF_FILE * )x_dbfAreaStorage( area );
   X_DBF_STATUS status = s_ready( file );
   uint64_t     count;

   if( status != X_DBF_OK )
      return status;
   if( record != 0 )
   {
      status = s_count( area, &count );
      if( status != X_DBF_OK )
         return status;
      if( record > count )
         return X_DBF_INVALID_RECORD;
      return s_lockRecord( file, record );
   }
   if( !file->shared || file->fileLocked )
      return X_DBF_OK;
   status = s_unlock( area, 0 );
   if( status != X_DBF_OK )
      return status;
   if( !x_dbfLockWait( &file->file, s_fileLockOffset( file ), file->lockLength, FLX_EXCLUSIVE, file->lockRetry ) )
      return X_DBF_LOCK_FAILED;
   file->fileLocked = 1;
   return X_DBF_OK;
}

static X_DBF_STATUS s_isLocked( DBF area, uint64_t record, int *locked )
{
   X_DBF_FILE  *file = ( X_DBF_FILE * )x_dbfAreaStorage( area );
   X_DBF_STATUS status = s_ready( file );

   if( status == X_DBF_OK )
      *locked = record != 0 && record <= file->recordCount &&
                ( !file->shared || file->fileLocked || s_findLock( file, record ) != NULL );
   return status;
}

static void s_memoPackDrop( X_DBF_MEMO_PACK **pack )
{
   x_dbfMemoPackDispose( *pack );
}

static X_DBF_STATUS s_compact( DBF area, int removeDeleted )
{
   X_DBF_FILE      *file = ( X_DBF_FILE * )x_dbfAreaStorage( area );
   X_DBF_FILE      *staged __attribute__( ( cleanup( s_fileDrop ) ) ) = NULL;
   X_DBF_MEMO_PACK *memo __attribute__( ( cleanup( s_memoPackDrop ) ) ) = NULL;
   X_DBF_TEXT       buffer __attribute__( ( cleanup( s_bufferDrop ) ) ) = { NULL, 0 };
   X_DBF_TEXT       current __attribute__( ( cleanup( s_bufferDrop ) ) ) = { NULL, 0 };
   char            *path;
   void            *handle;
   uint64_t         source;
   uint32_t         destination = 0;
   unsigned char    end = 0x1A;
   int64_t          size, offset;
   X_DBF_STATUS     status = s_ready( file );

   if( status != X_DBF_OK )
      return status;
   if( file->readOnly )
      return X_DBF_READ_ONLY;
   if( file->shared )
      return X_DBF_SHARED_OPERATION;
   status = s_refresh( file, 1 );
   if( status != X_DBF_OK || file->memo == NULL )
      return status;
   status = x_dbfMemoPackBegin( file->memo, &memo );
   if( status != X_DBF_OK )
      return status;
   handle = x_fileTemp( &path, "", "xdev-dbf-pack-", ".dbf", FC_NORMAL );
   if( handle == NULL )
      return X_DBF_CREATE_ERROR;
   staged = s_fileNew( handle, 0, 0, file->lockScheme );
   staged->temporary = path;
   staged->headerLength = file->headerLength;
   staged->recordLength = file->recordLength;
   buffer.length = file->headerLength > file->recordLength ? file->headerLength : file->recordLength;
   if( buffer.length < 32768 )
      buffer.length = 32768;
   buffer.data = ( char * )x_xgrab( buffer.length );
   if( !removeDeleted && !area->state.eof )
      current.data = ( char * )x_xgrab( file->recordLength );
   status = s_readAt( file, buffer.data, file->headerLength, 0 );
   if( status != X_DBF_OK )
      return status;
   memcpy( staged->header, buffer.data, sizeof( staged->header ) );
   status = s_writeAt( staged, buffer.data, file->headerLength, 0 );
   if( status != X_DBF_OK )
      return status;
   for( source = 1; source <= file->recordCount; ++source )
   {
      size_t i;
      status = s_readAt( file, buffer.data, file->recordLength, s_recordOffset( file, source ) );
      if( status != X_DBF_OK )
         return status;
      if( buffer.data[0] != ' ' && buffer.data[0] != '*' )
         return X_DBF_INVALID_RECORD;
      if( removeDeleted && buffer.data[0] == '*' )
         continue;
      for( i = 0; i < area->schema->count; ++i )
      {
         const X_DBF_FIELD *field = &area->schema->fields[i];
         if( field->type == X_DBF_MEMO || field->type == X_DBF_PICTURE )
         {
            status = x_dbfMemoPackField( memo, field, buffer.data );
            if( status != X_DBF_OK )
               return status;
         }
      }
      ++destination;
      if( current.data != NULL && source == area->state.record )
      {
         memcpy( current.data, buffer.data, file->recordLength );
         current.length = file->recordLength;
      }
      status = s_writeAt( staged, buffer.data, file->recordLength, s_recordOffset( staged, destination ) );
      if( status != X_DBF_OK )
         return status;
   }
   if( current.data != NULL && current.length == 0 )
      return X_DBF_INVALID_RECORD;
   size = staged->headerLength + ( int64_t )destination * staged->recordLength + 1;
   status = s_writeAt( staged, &end, 1, size - 1 );
   if( status == X_DBF_OK )
      status = s_publish( staged, destination );
   if( status == X_DBF_OK && !x_fileCommit( &staged->file ) )
      status = X_DBF_WRITE_ERROR;
   if( status == X_DBF_OK )
      status = x_dbfMemoPackSeal( memo );
   if( status != X_DBF_OK )
      return status;

   status = x_dbfMemoPackApply( memo );
   for( offset = 0; status == X_DBF_OK && offset < size; )
   {
      size_t bytes = ( uint64_t )( size - offset ) < buffer.length ? ( size_t )( size - offset ) : buffer.length;
      status = s_readAt( staged, buffer.data, bytes, offset );
      if( status == X_DBF_OK )
         status = s_writeAt( file, buffer.data, bytes, offset );
      offset += ( int64_t )bytes;
   }
   if( status == X_DBF_OK && !x_fileTrunc( &file->file, size ) )
      status = X_DBF_WRITE_ERROR;
   if( status == X_DBF_OK && !x_fileCommit( &file->file ) )
      status = X_DBF_WRITE_ERROR;
   if( status != X_DBF_OK )
   {
      staged->keepTemporary = 1;
      x_dbfMemoPackKeep( memo );
      file->poisoned = 1;
   }
   else
   {
      memcpy( file->header, staged->header, sizeof( file->header ) );
      file->recordCount = destination;
      if( current.length != 0 )
         memcpy( area->record, current.data, current.length );
      if( removeDeleted )
      {
         memset( area->record, ' ', area->schema->recordLength );
         for( source = 0; source < area->schema->count; ++source )
         {
            const X_DBF_FIELD *field = &area->schema->fields[source];
            if( ( field->type == X_DBF_MEMO || field->type == X_DBF_PICTURE ) && field->length == 4 )
               memset( area->record + field->offset, 0, field->length );
         }
         area->state.record = ( uint64_t )destination + 1;
         area->state.recordCount = destination;
         area->state.bof = area->state.eof = 1;
         area->state.found = area->state.deleted = 0;
      }
   }
   return status;
}

static X_DBF_STATUS s_reorganize( DBF area, int pack )
{
   X_DBF_FILE   *file = ( X_DBF_FILE * )x_dbfAreaStorage( area );
   X_DBF_TEXT    buffer __attribute__( ( cleanup( s_bufferDrop ) ) ) = { NULL, 0 };
   uint32_t      source, destination = 0;
   unsigned char end = 0x1A;
   int64_t       endOffset;
   X_DBF_STATUS  status = s_ready( file );

   if( status != X_DBF_OK )
      return status;
   if( file->readOnly )
      return X_DBF_READ_ONLY;
   if( file->shared )
      return X_DBF_SHARED_OPERATION;
   status = s_refresh( file, 1 );
   if( status != X_DBF_OK )
      return status;
   buffer.data = ( char * )x_xgrab( file->recordLength );
   if( pack )
   {
      for( source = 1; source <= file->recordCount; ++source )
      {
         status = s_readAt( file, buffer.data, file->recordLength, s_recordOffset( file, source ) );
         if( status != X_DBF_OK )
            return status;
         if( buffer.data[0] != ' ' && buffer.data[0] != '*' )
            return X_DBF_INVALID_RECORD;
      }
   }
   if( pack )
   {
      for( source = 1; source <= file->recordCount; ++source )
      {
         status = s_readAt( file, buffer.data, file->recordLength, s_recordOffset( file, source ) );
         if( status != X_DBF_OK )
            break;
         if( buffer.data[0] != ' ' && buffer.data[0] != '*' )
         {
            status = X_DBF_INVALID_RECORD;
            break;
         }
         if( buffer.data[0] == ' ' )
         {
            ++destination;
            if( destination != source )
               status = s_writeAt( file, buffer.data, file->recordLength, s_recordOffset( file, destination ) );
            if( status != X_DBF_OK )
               break;
         }
      }
   }
   endOffset = file->headerLength + ( int64_t )destination * file->recordLength;
   if( status == X_DBF_OK )
      status = s_writeAt( file, &end, 1, endOffset );
   if( status == X_DBF_OK && !x_fileTrunc( &file->file, endOffset + 1 ) )
      status = X_DBF_WRITE_ERROR;
   if( status == X_DBF_OK && !x_fileCommit( &file->file ) )
      status = X_DBF_WRITE_ERROR;
   if( status == X_DBF_OK )
      status = s_publish( file, destination );
   if( status == X_DBF_OK && !x_fileCommit( &file->file ) )
      status = X_DBF_WRITE_ERROR;
   if( status == X_DBF_OK && !pack )
      status = x_dbfMemoZap( file->memo );
   if( status != X_DBF_OK )
      file->poisoned = 1;
   return status;
}

static X_DBF_STATUS s_pack( DBF area )
{
   X_DBF_FILE *file = ( X_DBF_FILE * )x_dbfAreaStorage( area );
   if( file != NULL && file->memo != NULL )
      return s_compact( area, 1 );
   return s_reorganize( area, 1 );
}

static X_DBF_STATUS s_zap( DBF area )
{
   return s_reorganize( area, 0 );
}

static X_DBF_STATUS s_info( DBF area, int selector, X_ITEM *result )
{
   X_DBF_FILE        *file = ( X_DBF_FILE * )x_dbfAreaStorage( area );
   X_DBF_RECORD_LOCK *entry;
   X_ITEM             value __attribute__( ( cleanup( x_itemClear ) ) ) = X_ITEM_NIL_INIT;
   size_t             count = 0, index = 1;
   X_DBF_STATUS       status = s_ready( file );

   if( status != X_DBF_OK )
      return status;
   switch( selector )
   {
      case DBI_GETHEADERSIZE:
         value.nType = X_IT_INTEGER;
         value.u.integer = file->headerLength;
         break;
      case DBI_LASTUPDATE:
      {
         uint64_t records;
         int      year, month, day;
         long     julian = 0;
         status = s_count( area, &records );
         if( status != X_DBF_OK )
            return status;
         value.nType = X_IT_DATE;
         if( file->header[2] >= 1 && file->header[2] <= 12 && file->header[3] >= 1 && file->header[3] <= 31 )
         {
            julian = x_dateEncode( file->header[1] + 1900, file->header[2], file->header[3] );
            x_dateDecode( julian, &year, &month, &day );
            if( year != file->header[1] + 1900 || month != file->header[2] || day != file->header[3] )
               julian = 0;
         }
         value.u.integer = julian;
         break;
      }
      case DBI_FILEHANDLE:
         value.nType = X_IT_INTEGER;
         value.u.integer = x_fileHandle( &file->file );
         break;
      case DBI_FULLPATH: x_itemPutStringBuffer( &value, x_strdup( file->path ), strlen( file->path ) ); break;
      case DBI_TABLEEXT: x_itemPutStringBuffer( &value, x_strdup( ".dbf" ), 4 ); break;
      case DBI_MEMOTYPE:
      case DBI_MEMOBLOCKSIZE:
      case DBI_MEMOHANDLE:
      case DBI_MEMOVERSION:
      case DBI_MEMOEXT: return x_dbfMemoInfo( file->memo, selector, result );
      case DBI_MEMOPACK:
         status = s_compact( area, 0 );
         if( status != X_DBF_OK )
            return status;
         break;
      case DBI_TABLETYPE:
         value.nType = X_IT_INTEGER;
         value.u.integer = DB_DBF_STD;
         break;
      case DBI_DB_VERSION:
      {
         const char *format = IIF( s_xdev7( file->header[0] ), "xDev 7", "DBF" );
         x_itemPutStringBuffer( &value, x_strdup( format ), strlen( format ) );
         break;
      }
      case DBI_VALIDBUFFER:
         value.nType = X_IT_LOGICAL;
         value.u.logical = 1;
         break;
      case DBI_ISTEMPORARY:
      case DBI_ISENCRYPTED:
         value.nType = X_IT_LOGICAL;
         value.u.logical = 0;
         break;
      case DBI_ISFLOCK:
         value.nType = X_IT_LOGICAL;
         value.u.logical = file->fileLocked;
         break;
      case DBI_LOCKOFFSET:
         value.nType = X_IT_INTEGER;
         value.u.integer = file->lockBase;
         break;
      case DBI_LOCKSCHEME:
         value.nType = X_IT_INTEGER;
         value.u.integer = file->lockScheme;
         break;
      case DBI_LOCKCOUNT:
      case DBI_GETLOCKARRAY:
         for( entry = file->locks; entry != NULL; entry = entry->next )
            ++count;
         if( selector == DBI_LOCKCOUNT )
         {
            value.nType = X_IT_INTEGER;
            value.u.integer = ( int64_t )count;
         }
         else
         {
            x_arrayNew( &value, ( int64_t )count );
            for( entry = file->locks; entry != NULL; entry = entry->next )
               x_arraySetNInt( &value, index++, ( int64_t )entry->record );
         }
         break;
      default: return X_DBF_UNSUPPORTED_OPERATION;
   }
   x_itemCopy( result, &value );
   return X_DBF_OK;
}

static X_DBF_STATUS s_memoRead( DBF area, const X_DBF_FIELD *field, const char *record, X_DBF_TEXT *result,
                                int *binary )
{
   X_DBF_FILE  *file = ( X_DBF_FILE * )x_dbfAreaStorage( area );
   X_DBF_STATUS status = s_ready( file );
   if( status != X_DBF_OK )
      return status;
   return x_dbfMemoRead( file->memo, field, record, result, binary );
}

static X_DBF_STATUS s_memoWrite( DBF area, const X_DBF_FIELD *field, const char *data, size_t length, char *pointer )
{
   X_DBF_FILE  *file = ( X_DBF_FILE * )x_dbfAreaStorage( area );
   X_DBF_STATUS status = s_checkWrite( area, area->state.record );
   if( status == X_DBF_OK )
      status = x_dbfMemoWrite( file->memo, field, data, length, pointer );
   if( status == X_DBF_WRITE_ERROR || status == X_DBF_RECOVERY_REQUIRED )
      file->poisoned = 1;
   return status;
}

static X_DBF_STATUS s_memoExport( DBF area, const X_DBF_FIELD *field, const X_ITEM *target )
{
   X_DBF_FILE  *file = ( X_DBF_FILE * )x_dbfAreaStorage( area );
   X_DBF_STATUS status = s_ready( file );
   if( status != X_DBF_OK )
      return status;
   return x_dbfMemoExport( file->memo, field, area->record, target );
}

static X_DBF_STATUS s_memoImport( DBF area, const X_DBF_FIELD *field, const X_ITEM *source, uint64_t length,
                                  char *pointer )
{
   X_DBF_FILE  *file = ( X_DBF_FILE * )x_dbfAreaStorage( area );
   X_DBF_STATUS status = s_checkWrite( area, area->state.record );
   if( status == X_DBF_OK )
      status = x_dbfMemoImport( file->memo, field, source, length, pointer );
   if( status == X_DBF_WRITE_ERROR || status == X_DBF_RECOVERY_REQUIRED )
      file->poisoned = 1;
   return status;
}

static X_DBF_STATUS s_memoValueRead( DBF area, const X_DBF_FIELD *field, X_ITEM *result )
{
   X_DBF_FILE  *file = ( X_DBF_FILE * )x_dbfAreaStorage( area );
   X_DBF_STATUS status = s_ready( file );
   if( status != X_DBF_OK )
      return status;
   return x_dbfMemoReadValue( file->memo, field, area->record, result );
}

static X_DBF_STATUS s_memoValueWrite( DBF area, const X_DBF_FIELD *field, const X_ITEM *value, char *pointer )
{
   X_DBF_FILE  *file = ( X_DBF_FILE * )x_dbfAreaStorage( area );
   X_DBF_STATUS status = s_checkWrite( area, area->state.record );
   if( status == X_DBF_OK )
      status = x_dbfMemoWriteValue( file->memo, field, value, pointer );
   if( status == X_DBF_WRITE_ERROR || status == X_DBF_RECOVERY_REQUIRED )
      file->poisoned = 1;
   return status;
}

const X_DBF_DRIVER x_dbfFileDriver = { .abi = X_DBF_DRIVER_ABI,
                                       .name = "DBF",
                                       .open = s_open,
                                       .close = s_close,
                                       .dispose = s_dispose,
                                       .sync = s_sync,
                                       .read = s_read,
                                       .write = s_write,
                                       .append = s_append,
                                       .create = s_create,
                                       .recordCount = s_count,
                                       .checkWrite = s_checkWrite,
                                       .lock = s_lock,
                                       .unlock = s_unlock,
                                       .isLocked = s_isLocked,
                                       .pack = s_pack,
                                       .zap = s_zap,
                                       .info = s_info,
                                       .memoRead = s_memoRead,
                                       .memoWrite = s_memoWrite,
                                       .memoExport = s_memoExport,
                                       .memoImport = s_memoImport,
                                       .memoValueRead = s_memoValueRead,
                                       .memoValueWrite = s_memoValueWrite };
