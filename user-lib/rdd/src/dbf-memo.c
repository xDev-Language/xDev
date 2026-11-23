/*
 * Copyright (c) 2024-2026 xDev-Language
 *
 * Library...: xDev 7.0.0 RDD
 * File......: dbf-memo.c
 *
 */

#include "dbf-internal.h"

#include <xdev/dbinfo.ch>
#include <xdev/fileio.ch>
#include <string.h>

struct _X_DBF_MEMO_FILE
{
   X_ITEM        file;
   uint32_t      blockSize;
   uint32_t      firstBlock;
   int           kind;
   int           version;
   int           blockHeaders;
   unsigned char name[8];
   int           shared;
   int           readOnly;
   int           closed;
   int           poisoned;
};

typedef struct
{
   X_DBF_MEMO_FILE *memo;
   int              held;
   int              type;
} X_DBF_MEMO_LOCK;

typedef struct _X_DBF_MEMO_MOVE
{
   struct _X_DBF_MEMO_MOVE *next;
   uint32_t                 block;
   uint32_t                 length;
   uint32_t                 type;
   uint32_t                 destination;
   uint32_t                 payloadLength;
} X_DBF_MEMO_MOVE;

struct _X_DBF_MEMO_PACK
{
   X_DBF_MEMO_FILE *source;
   X_DBF_MEMO_FILE *target;
   X_DBF_MEMO_MOVE *moves[1021];
   char            *path;
   int64_t          sourceSize;
   uint32_t         nextBlock;
   int              sealed;
   int              keep;
};

static uint32_t s_number( const unsigned char *data, int bigEndian )
{
   if( bigEndian )
      return ( ( uint32_t )data[0] << 24 ) | ( ( uint32_t )data[1] << 16 ) | ( ( uint32_t )data[2] << 8 ) | data[3];
   return data[0] | ( ( uint32_t )data[1] << 8 ) | ( ( uint32_t )data[2] << 16 ) | ( ( uint32_t )data[3] << 24 );
}

static void s_store( unsigned char *data, uint32_t value, int bigEndian )
{
   unsigned int i;
   for( i = 0; i < 4; ++i )
      data[bigEndian ? 3 - i : i] = ( unsigned char )( value >> ( i * 8 ) );
}

static X_DBF_STATUS s_read( X_DBF_MEMO_FILE *memo, void *buffer, size_t length, int64_t offset )
{
   size_t done = 0;
   while( done < length )
   {
      int64_t bytes = x_fileReadAt( &memo->file, ( char * )buffer + done, length - done, offset + ( int64_t )done );
      if( bytes <= 0 || ( uint64_t )bytes > length - done )
         return X_DBF_READ_ERROR;
      done += ( size_t )bytes;
   }
   return X_DBF_OK;
}

static X_DBF_STATUS s_write( X_DBF_MEMO_FILE *memo, const void *buffer, size_t length, int64_t offset )
{
   size_t done = 0;
   while( done < length )
   {
      int64_t bytes =
         x_fileWriteAt( &memo->file, ( const char * )buffer + done, length - done, offset + ( int64_t )done );
      if( bytes <= 0 || ( uint64_t )bytes > length - done )
         return X_DBF_WRITE_ERROR;
      done += ( size_t )bytes;
   }
   return X_DBF_OK;
}

static X_DBF_STATUS s_ready( const X_DBF_MEMO_FILE *memo )
{
   if( memo == NULL || memo->closed )
      return X_DBF_INVALID_STATE;
   return memo->poisoned ? X_DBF_RECOVERY_REQUIRED : X_DBF_OK;
}

static void s_lockDrop( X_DBF_MEMO_LOCK *lock )
{
   if( lock->held )
   {
      int error = x_fsError();
      if( !x_fileLock( &lock->memo->file, 0, 1, lock->type, 1 ) )
         lock->memo->poisoned = 1;
      else
         lock->held = 0;
      x_fsSetError( error );
   }
}

static X_DBF_STATUS s_enter( X_DBF_MEMO_FILE *memo, X_DBF_MEMO_LOCK *lock, int write )
{
   X_DBF_STATUS status = s_ready( memo );
   if( status != X_DBF_OK )
      return status;
   if( write && memo->readOnly )
      return X_DBF_READ_ONLY;
   lock->memo = memo;
   lock->type = write ? FLX_EXCLUSIVE : FLX_SHARED;
   if( memo->shared )
   {
      if( !x_dbfLockWait( &memo->file, 0, 1, lock->type, X_DBF_LOCK_WAIT ) )
         return X_DBF_LOCK_FAILED;
      lock->held = 1;
   }
   return X_DBF_OK;
}

static X_DBF_STATUS s_leave( X_DBF_MEMO_LOCK *lock, X_DBF_STATUS status )
{
   if( lock->held )
   {
      int error = x_fsError();
      s_lockDrop( lock );
      if( lock->held && status == X_DBF_OK )
         return X_DBF_LOCK_FAILED;
      if( status != X_DBF_OK )
         x_fsSetError( error );
   }
   return status;
}

static void s_textDrop( X_DBF_TEXT *text )
{
   x_dbfTextClear( text );
}

static void s_memoDrop( X_DBF_MEMO_FILE **memo )
{
   x_dbfMemoDispose( *memo );
}

static X_DBF_MEMO_FILE *s_new( void *handle, int kind, int shared, int readOnly )
{
   X_DBF_MEMO_FILE *memo = ( X_DBF_MEMO_FILE * )x_xgrab( sizeof( *memo ) );
   memset( memo, 0, sizeof( *memo ) );
   x_itemPutPtrGC( &memo->file, handle );
   x_gcLock( handle );
   memo->kind = kind;
   memo->version = kind == DB_MEMO_SMT ? DB_MEMOVER_SIX : DB_MEMOVER_STD;
   memo->shared = shared;
   memo->readOnly = readOnly;
   return memo;
}

static uint32_t s_headerLength( const X_DBF_MEMO_FILE *memo )
{
   /* The DBT header occupies exactly block 0 */
   return memo->version == DB_MEMOVER_FLEX ? 1024 : ( memo->blockHeaders ? memo->blockSize : 512 );
}

static X_DBF_STATUS s_initialize( X_DBF_MEMO_FILE *memo )
{
   unsigned char header[1024];
   size_t        headerLength = s_headerLength( memo );
   X_DBF_STATUS  status;
   memset( header, 0, sizeof( header ) );
   memo->firstBlock = ( ( uint32_t )headerLength + memo->blockSize - 1 ) / memo->blockSize;
   s_store( header, memo->firstBlock, memo->kind == DB_MEMO_FPT );
   s_store( header + 4, memo->blockSize, memo->kind == DB_MEMO_FPT );
   if( memo->kind == DB_MEMO_SMT )
      memcpy( header + 8, "SIxMemo", 7 );
   else if( memo->blockHeaders )
   {
      memcpy( header + 8, memo->name, sizeof( memo->name ) );
      header[20] = ( unsigned char )memo->blockSize;
      header[21] = ( unsigned char )( memo->blockSize >> 8 );
   }
   else
      memcpy( header + 8, "xDev", 4 );
   if( memo->version == DB_MEMOVER_FLEX )
      memcpy( header + 512, "FlexFile3\003", 11 );
   status = s_write( memo, header, headerLength < sizeof( header ) ? headerLength : sizeof( header ), 0 );
   if( status == X_DBF_OK && !x_fileTrunc( &memo->file, ( int64_t )memo->firstBlock * memo->blockSize ) )
      status = X_DBF_WRITE_ERROR;
   if( status == X_DBF_OK && !x_fileCommit( &memo->file ) )
      status = X_DBF_WRITE_ERROR;
   if( status != X_DBF_OK )
      memo->poisoned = 1;
   return status;
}

X_DBF_STATUS x_dbfMemoCreate( const char *path, int kind, int blockHeaders, X_DBF_MEMO_FILE **result )
{
   X_DBF_MEMO_FILE *memo __attribute__( ( cleanup( s_memoDrop ) ) ) = NULL;
   X_DBF_STATUS     status;
   void            *handle;
   handle = x_fileCreate( path, FC_NORMAL );
   if( handle == NULL )
      return X_DBF_CREATE_ERROR;
   memo = s_new( handle, kind, 0, 0 );
   if( kind == DB_MEMO_FPT )
      memo->version = DB_MEMOVER_FLEX;
   memo->blockHeaders = blockHeaders != 0;
   if( memo->blockHeaders )
   {
      /* DBF file name without extension, upper case, at most 8 characters */
      const char *base = path, *position;
      size_t      i;
      for( position = path; *position; ++position )
         if( *position == '/' || *position == '\\' || *position == ':' )
            base = position + 1;
      for( i = 0; i < sizeof( memo->name ) && base[i] != '\0' && base[i] != '.'; ++i )
         memo->name[i] = ( unsigned char )( base[i] >= 'a' && base[i] <= 'z' ? base[i] - 'a' + 'A' : base[i] );
   }
   memo->blockSize = kind == DB_MEMO_FPT || memo->blockHeaders ? 64 : ( kind == DB_MEMO_DBT ? 512 : 32 );
   status = s_initialize( memo );
   if( status == X_DBF_OK )
   {
      *result = memo;
      memo = NULL;
   }
   return status;
}

X_DBF_STATUS x_dbfMemoOpen( const char *path, int kind, int blockHeaders, int shared, int readOnly,
                            X_DBF_MEMO_FILE **result )
{
   X_DBF_MEMO_FILE *memo __attribute__( ( cleanup( s_memoDrop ) ) ) = NULL;
   X_DBF_MEMO_LOCK  guard __attribute__( ( cleanup( s_lockDrop ) ) ) = { NULL, 0, 0 };
   unsigned char    header[512];
   uint32_t         next;
   int64_t          size;
   X_DBF_STATUS     status;
   void            *handle;
   handle = x_fileOpen( path, ( readOnly ? FO_READ : FO_READWRITE ) | ( shared ? FO_DENYNONE : FO_EXCLUSIVE ) );
   if( handle == NULL )
      return X_DBF_OPEN_ERROR;
   memo = s_new( handle, kind, shared != 0, readOnly != 0 );
   memo->blockHeaders = blockHeaders != 0;
   status = s_enter( memo, &guard, 0 );
   size = x_fileSize( &memo->file, 0 );
   if( status == X_DBF_OK && size < 0 )
      status = X_DBF_READ_ERROR;
   /* A DBT header may be shorter than 512 bytes when blocks are 64 bytes */
   else if( status == X_DBF_OK && memo->blockHeaders && size < 64 )
      status = X_DBF_CORRUPT_FILE;
   if( status == X_DBF_OK )
      status =
         s_read( memo, header,
                 memo->blockHeaders && size < ( int64_t )sizeof( header ) ? ( size_t )size : sizeof( header ), 0 );
   if( status != X_DBF_OK )
      return s_leave( &guard, status );
   if( memo->blockHeaders )
   {
      memo->blockSize = header[20] | ( ( uint32_t )header[21] << 8 );
      memcpy( memo->name, header + 8, sizeof( memo->name ) );
      if( memo->blockSize < 64 )
         return s_leave( &guard, X_DBF_UNSUPPORTED_FORMAT );
   }
   else
      memo->blockSize = kind == DB_MEMO_DBT ? 512 : s_number( header + 4, kind == DB_MEMO_FPT );
   if( memo->blockSize == 0 || memo->blockSize > UINT16_MAX )
      return s_leave( &guard, X_DBF_UNSUPPORTED_FORMAT );
   if( kind == DB_MEMO_FPT &&
       ( memcmp( header + 8, "SIxMemo", 7 ) == 0 || memcmp( header + 8, "Made by CLIP", 12 ) == 0 ) )
      return s_leave( &guard, X_DBF_UNSUPPORTED_FORMAT );
   memo->firstBlock = ( s_headerLength( memo ) + memo->blockSize - 1 ) / memo->blockSize;
   next = s_number( header, kind == DB_MEMO_FPT );
   if( kind == DB_MEMO_FPT && size >= 524 )
   {
      unsigned char signature[12];
      status = s_read( memo, signature, sizeof( signature ), 512 );
      if( status != X_DBF_OK )
         return s_leave( &guard, status );
      if( memcmp( signature, "FlexFile3\003", 10 ) == 0 )
      {
         if( size < 1024 )
            return s_leave( &guard, X_DBF_CORRUPT_FILE );
         memo->version = DB_MEMOVER_FLEX;
         memo->firstBlock = ( 1024 + memo->blockSize - 1 ) / memo->blockSize;
      }
   }
   if( size < 0 )
      status = X_DBF_READ_ERROR;
   else if( next < memo->firstBlock || size < ( int64_t )s_headerLength( memo ) ||
            ( uint64_t )size > ( uint64_t )UINT32_MAX * memo->blockSize )
      status = X_DBF_CORRUPT_FILE;
   status = s_leave( &guard, status );
   if( status == X_DBF_OK )
   {
      *result = memo;
      memo = NULL;
   }
   return status;
}

X_DBF_STATUS x_dbfMemoClose( X_DBF_MEMO_FILE *memo )
{
   if( memo == NULL )
      return X_DBF_OK;
   if( memo->closed )
      return X_DBF_INVALID_STATE;
   memo->closed = 1;
   return x_fileClose( &memo->file ) ? X_DBF_OK : X_DBF_CLOSE_ERROR;
}

void x_dbfMemoDispose( X_DBF_MEMO_FILE *memo )
{
   if( memo != NULL )
   {
      int error = x_fsError();
      if( !memo->closed )
         x_fileClose( &memo->file );
      x_gcUnlock( memo->file.u.pPointer );
      x_itemClear( &memo->file );
      x_xfree( memo );
      x_fsSetError( error );
   }
}

X_DBF_STATUS x_dbfMemoSync( X_DBF_MEMO_FILE *memo )
{
   X_DBF_STATUS status;
   if( memo == NULL )
      return X_DBF_OK;
   status = s_ready( memo );
   if( status == X_DBF_OK && !memo->readOnly && !x_fileCommit( &memo->file ) )
   {
      memo->poisoned = 1;
      status = X_DBF_WRITE_ERROR;
   }
   return status;
}

X_DBF_STATUS x_dbfMemoZap( X_DBF_MEMO_FILE *memo )
{
   X_DBF_STATUS status;
   if( memo == NULL )
      return X_DBF_OK;
   status = s_ready( memo );
   if( status != X_DBF_OK )
      return status;
   if( memo->readOnly )
      return X_DBF_READ_ONLY;
   if( memo->shared )
      return X_DBF_SHARED_OPERATION;
   return s_initialize( memo );
}

static X_DBF_STATUS s_pointer( const X_DBF_MEMO_FILE *memo, const X_DBF_FIELD *field, const char *record,
                               uint32_t *block, uint32_t *length, uint32_t *type )
{
   const unsigned char *data = ( const unsigned char * )record + field->offset;
   size_t               i;
   uint64_t             number = 0;
   *block = *length = 0;
   *type = 1;
   if( field->length == 4 )
   {
      if( memo->kind != DB_MEMO_FPT )
         return X_DBF_UNSUPPORTED_FORMAT;
      *block = s_number( data, 0 );
   }
   else if( field->length == 10 )
   {
      if( memo->kind == DB_MEMO_SMT )
      {
         if( data[0] == ' ' && data[1] == ' ' )
            return X_DBF_OK;
         *type = data[0] | ( ( uint32_t )data[1] << 8 );
         *length = s_number( data + 2, 0 );
         *block = s_number( data + 6, 0 );
         if( *type > 6 )
            return X_DBF_UNSUPPORTED_FORMAT;
         if( ( *length != 0 && *block == 0 ) || ( *type != 1 && ( *block != 0 || *type > 1 ) && *length == 0 ) )
            return X_DBF_CORRUPT_FILE;
      }
      else
      {
         if( data[0] == 0 )
         {
            for( i = 1; i < 10; ++i )
               if( data[i] != 0 )
                  return X_DBF_CORRUPT_FILE;
            return X_DBF_OK;
         }
         for( i = 0; i < 10; ++i )
         {
            if( data[i] >= '0' && data[i] <= '9' )
               number = number * 10 + data[i] - '0';
            else if( data[i] != ' ' || number != 0 )
               return X_DBF_CORRUPT_FILE;
         }
         if( number > UINT32_MAX )
            return X_DBF_CORRUPT_FILE;
         *block = ( uint32_t )number;
      }
   }
   else
      return X_DBF_UNSUPPORTED_FORMAT;
   if( *block != 0 && *block < memo->firstBlock )
      return X_DBF_CORRUPT_FILE;
   return X_DBF_OK;
}

static int s_fptType( const X_DBF_MEMO_FILE *memo, uint32_t type )
{
   return type <= 1 || ( memo->version == DB_MEMOVER_FLEX && ( type == 1002 || ( type >= 1006 && type <= 1016 ) ) );
}

static X_DBF_STATUS s_blockHeader( X_DBF_MEMO_FILE *memo, int64_t size, int64_t offset, uint32_t *length )
{
   unsigned char header[8];
   X_DBF_STATUS  status;
   if( size - offset < 8 )
      return X_DBF_CORRUPT_FILE;
   status = s_read( memo, header, sizeof( header ), offset );
   if( status != X_DBF_OK )
      return status;
   /* DBT block: FF FF 08 00 and a length that includes these 8 bytes */
   *length = s_number( header + 4, 0 );
   if( memcmp( header, "\xFF\xFF\x08\x00", 4 ) != 0 || *length < 8 )
      return X_DBF_CORRUPT_FILE;
   *length -= 8;
   return X_DBF_OK;
}

static X_DBF_STATUS s_load( X_DBF_MEMO_FILE *memo, const X_DBF_FIELD *field, const char *record, X_DBF_TEXT *result,
                            int *binary, uint32_t *valueType )
{
   X_DBF_MEMO_LOCK guard __attribute__( ( cleanup( s_lockDrop ) ) ) = { NULL, 0, 0 };
   X_DBF_TEXT      loaded __attribute__( ( cleanup( s_textDrop ) ) ) = { NULL, 0 };
   uint32_t        block, length, type;
   int64_t         offset, size;
   int             isBinary = field->type == X_DBF_PICTURE;
   X_DBF_STATUS    status = s_enter( memo, &guard, 0 );
   if( status != X_DBF_OK )
      return status;
   status = s_pointer( memo, field, record, &block, &length, &type );
   if( status != X_DBF_OK )
      return s_leave( &guard, status );
   if( memo->kind == DB_MEMO_SMT && type != 1 && block != 0 )
      isBinary = 1;
   size = x_fileSize( &memo->file, 0 );
   if( size < 0 )
      return s_leave( &guard, X_DBF_READ_ERROR );
   if( size < ( int64_t )s_headerLength( memo ) )
      return s_leave( &guard, X_DBF_CORRUPT_FILE );
   offset = ( int64_t )block * memo->blockSize;
   if( block != 0 && ( offset < ( int64_t )s_headerLength( memo ) || offset >= size ) )
      return s_leave( &guard, X_DBF_CORRUPT_FILE );
   if( block != 0 && memo->kind == DB_MEMO_FPT )
   {
      unsigned char header[8];
      if( size - offset < 8 )
         return s_leave( &guard, X_DBF_CORRUPT_FILE );
      status = s_read( memo, header, sizeof( header ), offset );
      if( status != X_DBF_OK )
         return s_leave( &guard, status );
      type = s_number( header, 1 );
      length = s_number( header + 4, 1 );
      if( !s_fptType( memo, type ) )
         return s_leave( &guard, X_DBF_UNSUPPORTED_FORMAT );
      isBinary = isBinary || type != 1;
      offset += 8;
   }
   if( block != 0 && memo->blockHeaders )
   {
      status = s_blockHeader( memo, size, offset, &length );
      if( status != X_DBF_OK )
         return s_leave( &guard, status );
      offset += 8;
   }
   if( block != 0 && memo->kind == DB_MEMO_DBT && !memo->blockHeaders )
   {
      unsigned char chunk[512];
      size_t        capacity = 0;
      int           terminated = 0;
      while( offset < size && !terminated )
      {
         size_t available =
            ( uint64_t )( size - offset ) < sizeof( chunk ) ? ( size_t )( size - offset ) : sizeof( chunk );
         size_t used = 0, needed;
         status = s_read( memo, chunk, available, offset );
         if( status != X_DBF_OK )
            return s_leave( &guard, status );
         while( used < available && chunk[used] != 0x1A )
            ++used;
         if( loaded.length > INT32_MAX - used )
            return s_leave( &guard, X_DBF_TEXT_OVERFLOW );
         needed = loaded.length + used + 1;
         if( needed > capacity )
         {
            capacity = needed > capacity * 2 ? needed : capacity * 2;
            loaded.data = ( char * )x_xrealloc( loaded.data, capacity );
         }
         memcpy( loaded.data + loaded.length, chunk, used );
         loaded.length += used;
         terminated = used < available;
         offset += ( int64_t )available;
      }
      if( !terminated )
         return s_leave( &guard, X_DBF_CORRUPT_FILE );
   }
   else
   {
      if( block == 0 )
         length = 0;
      if( length > INT32_MAX )
         return s_leave( &guard, X_DBF_TEXT_OVERFLOW );
      if( ( uint64_t )length > ( uint64_t )( size - offset ) )
         return s_leave( &guard, X_DBF_CORRUPT_FILE );
      loaded.data = ( char * )x_xgrab( ( size_t )length + 1 );
      loaded.length = length;
      status = s_read( memo, loaded.data, length, offset );
      if( status != X_DBF_OK )
         return s_leave( &guard, status );
   }
   if( loaded.data == NULL )
      loaded.data = ( char * )x_xgrab( 1 );
   loaded.data[loaded.length] = '\0';
   status = s_leave( &guard, X_DBF_OK );
   if( status == X_DBF_OK )
   {
      x_dbfTextClear( result );
      *result = loaded;
      loaded.data = NULL;
      *binary = isBinary;
      if( valueType != NULL )
         *valueType = block == 0 ? 1 : type;
   }
   return status;
}

X_DBF_STATUS x_dbfMemoRead( X_DBF_MEMO_FILE *memo, const X_DBF_FIELD *field, const char *record, X_DBF_TEXT *result,
                            int *binary )
{
   return s_load( memo, field, record, result, binary, NULL );
}

static void s_encodePointer( const X_DBF_MEMO_FILE *memo, const X_DBF_FIELD *field, uint32_t block, uint32_t length,
                             uint32_t type, char *pointer )
{
   unsigned char *data = ( unsigned char * )pointer;
   if( field->length == 4 )
      s_store( data, block, 0 );
   else if( memo->kind == DB_MEMO_SMT )
   {
      data[0] = ( unsigned char )type;
      data[1] = ( unsigned char )( type >> 8 );
      s_store( data + 2, length, 0 );
      s_store( data + 6, block, 0 );
   }
   else
   {
      int position;
      memset( data, ' ', 10 );
      for( position = 9; block != 0 && position >= 0; --position )
      {
         data[position] = ( unsigned char )( '0' + block % 10 );
         block /= 10;
      }
   }
}

void x_dbfMemoPackDispose( X_DBF_MEMO_PACK *pack )
{
   if( pack != NULL )
   {
      int    error = x_fsError();
      size_t i;
      x_dbfMemoDispose( pack->target );
      if( pack->path != NULL )
      {
         if( !pack->keep )
            x_fsDelete( pack->path );
         x_xfree( pack->path );
      }
      for( i = 0; i < sizeof( pack->moves ) / sizeof( pack->moves[0] ); ++i )
      {
         while( pack->moves[i] != NULL )
         {
            X_DBF_MEMO_MOVE *move = pack->moves[i];
            pack->moves[i] = move->next;
            x_xfree( move );
         }
      }
      x_xfree( pack );
      x_fsSetError( error );
   }
}

static void s_packDrop( X_DBF_MEMO_PACK **pack )
{
   x_dbfMemoPackDispose( *pack );
}

X_DBF_STATUS x_dbfMemoPackBegin( X_DBF_MEMO_FILE *memo, X_DBF_MEMO_PACK **result )
{
   X_DBF_MEMO_PACK *pack __attribute__( ( cleanup( s_packDrop ) ) ) = NULL;
   X_DBF_STATUS     status = s_ready( memo );
   void            *handle;
   if( status != X_DBF_OK )
      return status;
   if( memo->readOnly )
      return X_DBF_READ_ONLY;
   if( memo->shared )
      return X_DBF_SHARED_OPERATION;
   pack = ( X_DBF_MEMO_PACK * )x_xgrab( sizeof( *pack ) );
   memset( pack, 0, sizeof( *pack ) );
   pack->source = memo;
   pack->sourceSize = x_fileSize( &memo->file, 0 );
   if( pack->sourceSize < 0 )
      return X_DBF_READ_ERROR;
   if( pack->sourceSize < ( int64_t )s_headerLength( memo ) ||
       ( uint64_t )pack->sourceSize > ( uint64_t )UINT32_MAX * memo->blockSize )
      return X_DBF_CORRUPT_FILE;
   handle = x_fileTemp( &pack->path, "", "xdev-memo-pack-", ".tmp", FC_NORMAL );
   if( handle == NULL )
      return X_DBF_CREATE_ERROR;
   pack->target = s_new( handle, memo->kind, 0, 0 );
   pack->target->blockSize = memo->blockSize;
   pack->target->version = memo->version;
   pack->target->blockHeaders = memo->blockHeaders;
   memcpy( pack->target->name, memo->name, sizeof( memo->name ) );
   status = s_initialize( pack->target );
   if( status == X_DBF_OK && memo->version == DB_MEMOVER_FLEX )
   {
      unsigned char counter[4];
      status = s_read( memo, counter, sizeof( counter ), 532 );
      if( status == X_DBF_OK )
      {
         s_store( counter, s_number( counter, 0 ) + UINT32_C( 1 ), 0 );
         status = s_write( pack->target, counter, sizeof( counter ), 532 );
      }
   }
   if( status == X_DBF_OK )
   {
      pack->nextBlock = pack->target->firstBlock;
      *result = pack;
      pack = NULL;
   }
   return status;
}

static X_DBF_STATUS s_span( X_DBF_MEMO_FILE *memo, int64_t size, uint32_t block, uint32_t *length, int64_t *offset,
                            uint32_t *type )
{
   int64_t available;
   *offset = ( int64_t )block * memo->blockSize;
   if( *offset >= size )
      return X_DBF_CORRUPT_FILE;
   available = size - *offset;
   if( memo->kind == DB_MEMO_FPT )
   {
      unsigned char header[8];
      uint32_t      blockType;
      X_DBF_STATUS  status;
      if( available < 8 )
         return X_DBF_CORRUPT_FILE;
      status = s_read( memo, header, sizeof( header ), *offset );
      if( status != X_DBF_OK )
         return status;
      blockType = s_number( header, 1 );
      if( !s_fptType( memo, blockType ) )
         return X_DBF_UNSUPPORTED_FORMAT;
      *length = s_number( header + 4, 1 );
      if( type != NULL )
         *type = blockType;
      available -= 8;
   }
   else if( memo->blockHeaders )
   {
      X_DBF_STATUS status = s_blockHeader( memo, size, *offset, length );
      if( status != X_DBF_OK )
         return status;
      available -= 8;
   }
   else if( memo->kind == DB_MEMO_DBT )
   {
      unsigned char buffer[32768];
      uint64_t      scanned = 0;
      while( scanned < ( uint64_t )available )
      {
         uint64_t             remaining = ( uint64_t )available - scanned;
         size_t               bytes = remaining < sizeof( buffer ) ? ( size_t )remaining : sizeof( buffer );
         const unsigned char *end;
         X_DBF_STATUS         status = s_read( memo, buffer, bytes, *offset + ( int64_t )scanned );
         if( status != X_DBF_OK )
            return status;
         end = ( const unsigned char * )memchr( buffer, 0x1A, bytes );
         if( end != NULL )
         {
            scanned += ( size_t )( end - buffer );
            if( scanned > UINT32_MAX )
               return X_DBF_FIELD_OVERFLOW;
            *length = ( uint32_t )scanned;
            return X_DBF_OK;
         }
         scanned += bytes;
         if( scanned > UINT32_MAX )
            return X_DBF_FIELD_OVERFLOW;
      }
      return X_DBF_CORRUPT_FILE;
   }
   return ( uint64_t )*length <= ( uint64_t )available ? X_DBF_OK : X_DBF_CORRUPT_FILE;
}

static X_DBF_STATUS s_packCopy( X_DBF_MEMO_FILE *source, X_DBF_MEMO_FILE *target, int64_t sourceOffset,
                                int64_t targetOffset, uint64_t length )
{
   unsigned char buffer[32768];
   while( length != 0 )
   {
      size_t       bytes = length < sizeof( buffer ) ? ( size_t )length : sizeof( buffer );
      X_DBF_STATUS status = s_read( source, buffer, bytes, sourceOffset );
      if( status == X_DBF_OK )
         status = s_write( target, buffer, bytes, targetOffset );
      if( status != X_DBF_OK )
         return status;
      length -= bytes;
      sourceOffset += ( int64_t )bytes;
      targetOffset += ( int64_t )bytes;
   }
   return X_DBF_OK;
}

X_DBF_STATUS x_dbfMemoPackField( X_DBF_MEMO_PACK *pack, const X_DBF_FIELD *field, char *record )
{
   X_DBF_MEMO_MOVE *move;
   uint32_t         block, length, type, payloadLength;
   uint64_t         bytes, blocks, end;
   size_t           bucket;
   int64_t          offset, destination;
   X_DBF_STATUS     status;
   status = s_pointer( pack->source, field, record, &block, &length, &type );
   if( status != X_DBF_OK || block == 0 )
      return status;
   bucket = ( ( uint64_t )block * 2654435761U + length + type ) % ( sizeof( pack->moves ) / sizeof( pack->moves[0] ) );
   for( move = pack->moves[bucket]; move != NULL; move = move->next )
   {
      if( move->block == block && move->length == length && move->type == type )
      {
         s_encodePointer( pack->target, field, move->destination, move->payloadLength, move->type,
                          record + field->offset );
         return X_DBF_OK;
      }
   }
   payloadLength = length;
   status = s_span( pack->source, pack->sourceSize, block, &payloadLength, &offset, NULL );
   if( status != X_DBF_OK )
      return status;
   bytes = ( uint64_t )payloadLength + ( pack->source->kind == DB_MEMO_FPT || pack->source->blockHeaders
                                            ? 8
                                            : ( pack->source->kind == DB_MEMO_DBT ? 2 : 0 ) );
   blocks = ( bytes + pack->target->blockSize - 1 ) / pack->target->blockSize;
   end = ( uint64_t )pack->nextBlock + blocks;
   if( end > UINT32_MAX )
      return X_DBF_FIELD_OVERFLOW;
   destination = ( int64_t )pack->nextBlock * pack->target->blockSize;
   status = s_packCopy( pack->source, pack->target, offset, destination,
                        ( uint64_t )payloadLength +
                           ( pack->source->kind == DB_MEMO_FPT || pack->source->blockHeaders ? 8 : 0 ) );
   if( status == X_DBF_OK && pack->source->kind == DB_MEMO_DBT && !pack->source->blockHeaders )
   {
      const unsigned char terminator[2] = { 0x1A, 0x1A };
      status = s_write( pack->target, terminator, sizeof( terminator ), destination + payloadLength );
   }
   if( status == X_DBF_OK )
   {
      unsigned char padding[512] = { 0 };
      uint64_t      remaining = blocks * pack->target->blockSize - bytes;
      destination += ( int64_t )bytes;
      while( remaining != 0 && status == X_DBF_OK )
      {
         size_t used = remaining < sizeof( padding ) ? ( size_t )remaining : sizeof( padding );
         if( pack->target->version == DB_MEMOVER_FLEX && remaining == used )
            padding[used - 1] = 0xAF;
         status = s_write( pack->target, padding, used, destination );
         destination += ( int64_t )used;
         remaining -= used;
      }
   }
   if( status != X_DBF_OK )
      return status;
   move = ( X_DBF_MEMO_MOVE * )x_xgrab( sizeof( *move ) );
   move->block = block;
   move->length = length;
   move->type = type;
   move->payloadLength = payloadLength;
   move->destination = blocks == 0 ? 0 : pack->nextBlock;
   move->next = pack->moves[bucket];
   pack->moves[bucket] = move;
   pack->nextBlock = ( uint32_t )end;
   s_encodePointer( pack->target, field, move->destination, payloadLength, type, record + field->offset );
   return X_DBF_OK;
}

X_DBF_STATUS x_dbfMemoPackSeal( X_DBF_MEMO_PACK *pack )
{
   unsigned char next[4];
   X_DBF_STATUS  status;
   s_store( next, pack->nextBlock, pack->target->kind == DB_MEMO_FPT );
   status = s_write( pack->target, next, sizeof( next ), 0 );
   if( status == X_DBF_OK && !x_fileTrunc( &pack->target->file, ( int64_t )pack->nextBlock * pack->target->blockSize ) )
      status = X_DBF_WRITE_ERROR;
   if( status == X_DBF_OK )
      status = x_dbfMemoSync( pack->target );
   if( status == X_DBF_OK )
      pack->sealed = 1;
   return status;
}

void x_dbfMemoPackKeep( X_DBF_MEMO_PACK *pack )
{
   pack->keep = 1;
}

X_DBF_STATUS x_dbfMemoPackApply( X_DBF_MEMO_PACK *pack )
{
   int64_t      size;
   X_DBF_STATUS status;
   size = ( int64_t )pack->nextBlock * pack->target->blockSize;
   status = s_packCopy( pack->target, pack->source, 0, 0, ( uint64_t )size );
   if( status == X_DBF_OK && !x_fileTrunc( &pack->source->file, size ) )
      status = X_DBF_WRITE_ERROR;
   if( status == X_DBF_OK )
      status = x_dbfMemoSync( pack->source );
   if( status != X_DBF_OK )
      pack->source->poisoned = 1;
   return status;
}

typedef X_DBF_STATUS ( *X_DBF_MEMO_READER )( const void *source, char *buffer, size_t length, uint64_t offset );

static X_DBF_STATUS s_memoryRead( const void *source, char *buffer, size_t length, uint64_t offset )
{
   memcpy( buffer, ( const char * )source + ( size_t )offset, length );
   return X_DBF_OK;
}

static X_DBF_STATUS s_fileRead( const void *source, char *buffer, size_t length, uint64_t offset )
{
   const X_ITEM *file = ( const X_ITEM * )source;
   size_t        done = 0;
   while( done < length )
   {
      int64_t bytes = x_fileReadAt( file, buffer + done, length - done, ( int64_t )( offset + done ) );
      if( bytes <= 0 || ( uint64_t )bytes > length - done )
         return X_DBF_READ_ERROR;
      done += ( size_t )bytes;
   }
   return X_DBF_OK;
}

static X_DBF_STATUS s_fileWrite( const X_ITEM *file, const char *buffer, size_t length, int64_t offset )
{
   size_t done = 0;
   while( done < length )
   {
      int64_t bytes = x_fileWriteAt( file, buffer + done, length - done, offset + ( int64_t )done );
      if( bytes <= 0 || ( uint64_t )bytes > length - done )
         return X_DBF_WRITE_ERROR;
      done += ( size_t )bytes;
   }
   return X_DBF_OK;
}

static X_DBF_STATUS s_writeValue( X_DBF_MEMO_FILE *memo, const X_DBF_FIELD *field, X_DBF_MEMO_READER reader,
                                  const void *source, uint64_t length, uint32_t type, char *pointer )
{
   X_DBF_MEMO_LOCK guard __attribute__( ( cleanup( s_lockDrop ) ) ) = { NULL, 0, 0 };
   unsigned char   header[8], tail[512], counter[4];
   char            buffer[32768];
   uint64_t        next, blocks, end, physical, done = 0;
   int64_t         size, offset;
   size_t          extra, padding;
   int             reused = 0;
   X_DBF_STATUS    status;
   status = s_enter( memo, &guard, 1 );
   if( status != X_DBF_OK )
      return status;
   if( length > UINT32_MAX - ( memo->blockHeaders ? 8 : 0 ) )
      return s_leave( &guard, X_DBF_FIELD_OVERFLOW );
   if( field->length != 10 && ( field->length != 4 || memo->kind != DB_MEMO_FPT ) )
      return s_leave( &guard, X_DBF_UNSUPPORTED_FORMAT );
   if( memo->kind != DB_MEMO_FPT && !memo->blockHeaders && field->type == X_DBF_PICTURE )
      return s_leave( &guard, X_DBF_UNSUPPORTED_FORMAT );
   if( memo->kind == DB_MEMO_FPT && !s_fptType( memo, type ) )
      return s_leave( &guard, X_DBF_UNSUPPORTED_FORMAT );
   if( length == 0 && ( memo->kind != DB_MEMO_FPT || type <= 1 ) )
   {
      status = s_leave( &guard, X_DBF_OK );
      if( status == X_DBF_OK )
         s_encodePointer( memo, field, 0, 0, type, pointer );
      return status;
   }
   status = s_read( memo, header, sizeof( header ), 0 );
   if( status != X_DBF_OK )
      return s_leave( &guard, status );
   if( memo->version == DB_MEMOVER_FLEX )
   {
      status = s_read( memo, counter, sizeof( counter ), 532 );
      if( status != X_DBF_OK )
         return s_leave( &guard, status );
      s_store( counter, s_number( counter, 0 ) + UINT32_C( 1 ), 0 );
   }
   size = x_fileSize( &memo->file, 0 );
   if( size < 0 )
      return s_leave( &guard, X_DBF_READ_ERROR );
   next = s_number( header, memo->kind == DB_MEMO_FPT );
   if( next < memo->firstBlock || size < ( int64_t )s_headerLength( memo ) )
      return s_leave( &guard, X_DBF_CORRUPT_FILE );
   physical = ( ( uint64_t )size + memo->blockSize - 1 ) / memo->blockSize;
   if( next < physical )
      next = physical;
   extra = memo->kind == DB_MEMO_FPT || memo->blockHeaders ? 8 : ( memo->kind == DB_MEMO_DBT ? 2 : 0 );
   blocks = ( length + extra + memo->blockSize - 1 ) / memo->blockSize;
   // Exclusive use only: a value that fits in the blocks of the old one is written in their place,
   // so the record pointer stays; a shared reader could otherwise see a half written value
   if( !memo->shared )
   {
      uint32_t oldBlock, oldLength, oldType;
      int64_t  oldOffset;
      if( s_pointer( memo, field, pointer - field->offset, &oldBlock, &oldLength, &oldType ) == X_DBF_OK &&
          oldBlock != 0 && s_span( memo, size, oldBlock, &oldLength, &oldOffset, NULL ) == X_DBF_OK &&
          ( ( uint64_t )oldLength + extra + memo->blockSize - 1 ) / memo->blockSize >= blocks )
      {
         next = oldBlock;
         reused = 1;
      }
   }
   end = next + blocks;
   if( end > UINT32_MAX )
      return s_leave( &guard, X_DBF_FIELD_OVERFLOW );
   if( !reused )
   {
      s_store( header, ( uint32_t )end, memo->kind == DB_MEMO_FPT );
      status = s_write( memo, header, 4, 0 );
   }
   if( status == X_DBF_OK && memo->version == DB_MEMOVER_FLEX )
      status = s_write( memo, counter, sizeof( counter ), 532 );
   offset = ( int64_t )next * memo->blockSize;
   if( status == X_DBF_OK && memo->kind == DB_MEMO_FPT )
   {
      s_store( header, type, 1 );
      s_store( header + 4, ( uint32_t )length, 1 );
      status = s_write( memo, header, sizeof( header ), offset );
      offset += 8;
   }
   else if( status == X_DBF_OK && memo->blockHeaders )
   {
      memcpy( header, "\xFF\xFF\x08\x00", 4 );
      s_store( header + 4, ( uint32_t )length + 8, 0 );
      status = s_write( memo, header, sizeof( header ), offset );
      offset += 8;
   }
   while( status == X_DBF_OK && done < length )
   {
      size_t chunk = length - done < sizeof( buffer ) ? ( size_t )( length - done ) : sizeof( buffer );
      status = reader( source, buffer, chunk, done );
      if( status == X_DBF_OK )
         status = s_write( memo, buffer, chunk, offset + ( int64_t )done );
      done += chunk;
   }
   offset += ( int64_t )length;
   memset( tail, 0, sizeof( tail ) );
   if( status == X_DBF_OK && memo->kind == DB_MEMO_DBT && !memo->blockHeaders )
   {
      tail[0] = tail[1] = 0x1A;
      status = s_write( memo, tail, 2, offset );
      tail[0] = tail[1] = 0;
      offset += 2;
   }
   padding = ( size_t )( ( int64_t )end * memo->blockSize - offset );
   while( status == X_DBF_OK && padding != 0 )
   {
      size_t chunk = padding < sizeof( tail ) ? padding : sizeof( tail );
      if( memo->version == DB_MEMOVER_FLEX && padding == chunk )
         tail[chunk - 1] = 0xAF;
      status = s_write( memo, tail, chunk, offset );
      offset += ( int64_t )chunk;
      padding -= chunk;
   }
   if( status == X_DBF_WRITE_ERROR )
      memo->poisoned = 1;
   status = s_leave( &guard, status );
   if( status == X_DBF_OK )
      s_encodePointer( memo, field, ( uint32_t )next, ( uint32_t )length, type, pointer );
   return status;
}

X_DBF_STATUS x_dbfMemoWrite( X_DBF_MEMO_FILE *memo, const X_DBF_FIELD *field, const char *data, size_t length,
                             char *pointer )
{
   X_DBF_STATUS status = s_ready( memo );
   if( status != X_DBF_OK )
      return status;
   if( length > INT32_MAX )
      return X_DBF_TEXT_OVERFLOW;
   if( memo->kind == DB_MEMO_DBT && !memo->blockHeaders && length != 0 && memchr( data, 0x1A, length ) != NULL )
      return X_DBF_INVALID_FIELD_VALUE;
   return s_writeValue( memo, field, s_memoryRead, data, length, field->type == X_DBF_PICTURE ? 0 : 1, pointer );
}

X_DBF_STATUS x_dbfMemoImport( X_DBF_MEMO_FILE *memo, const X_DBF_FIELD *field, const X_ITEM *source, uint64_t length,
                              char *pointer )
{
   char         buffer[32768];
   uint64_t     offset = 0;
   X_DBF_STATUS status = s_ready( memo );
   if( status != X_DBF_OK )
      return status;
   if( length > UINT32_MAX )
      return X_DBF_FIELD_OVERFLOW;
   if( memo->kind == DB_MEMO_DBT && !memo->blockHeaders )
   {
      while( offset < length )
      {
         size_t bytes = length - offset < sizeof( buffer ) ? ( size_t )( length - offset ) : sizeof( buffer );
         status = s_fileRead( source, buffer, bytes, offset );
         if( status != X_DBF_OK )
            return status;
         if( memchr( buffer, 0x1A, bytes ) != NULL )
            return X_DBF_INVALID_FIELD_VALUE;
         offset += bytes;
      }
   }
   return s_writeValue( memo, field, s_fileRead, source, length, field->type == X_DBF_PICTURE ? 0 : 1, pointer );
}

X_DBF_STATUS x_dbfMemoExport( X_DBF_MEMO_FILE *memo, const X_DBF_FIELD *field, const char *record,
                              const X_ITEM *target )
{
   X_DBF_MEMO_LOCK guard __attribute__( ( cleanup( s_lockDrop ) ) ) = { NULL, 0, 0 };
   char            buffer[32768];
   uint32_t        block, length, type;
   uint64_t        done = 0;
   int64_t         offset = 0, size;
   X_DBF_STATUS    status = s_enter( memo, &guard, 0 );
   if( status != X_DBF_OK )
      return status;
   status = s_pointer( memo, field, record, &block, &length, &type );
   if( status != X_DBF_OK )
      return s_leave( &guard, status );
   if( memo->kind == DB_MEMO_SMT && type != 1 && block != 0 )
      return s_leave( &guard, X_DBF_INVALID_FIELD_VALUE );
   size = x_fileSize( &memo->file, 0 );
   if( size < 0 )
      return s_leave( &guard, X_DBF_READ_ERROR );
   if( size < ( int64_t )s_headerLength( memo ) )
      return s_leave( &guard, X_DBF_CORRUPT_FILE );
   if( block != 0 )
   {
      status = s_span( memo, size, block, &length, &offset, &type );
      if( status != X_DBF_OK )
         return s_leave( &guard, status );
      if( memo->kind == DB_MEMO_FPT )
      {
         if( type > 1 )
            return s_leave( &guard, X_DBF_INVALID_FIELD_VALUE );
         offset += 8;
      }
      else if( memo->blockHeaders )
         offset += 8;
   }
   else
      length = 0;
   while( status == X_DBF_OK && done < length )
   {
      size_t bytes = length - done < sizeof( buffer ) ? ( size_t )( length - done ) : sizeof( buffer );
      status = s_read( memo, buffer, bytes, offset + ( int64_t )done );
      if( status == X_DBF_OK )
         status = s_fileWrite( target, buffer, bytes, ( int64_t )done );
      done += bytes;
   }
   if( status == X_DBF_OK && !x_fileTrunc( target, length ) )
      status = X_DBF_WRITE_ERROR;
   return s_leave( &guard, status );
}

X_DBF_STATUS x_dbfMemoReadValue( X_DBF_MEMO_FILE *memo, const X_DBF_FIELD *field, const char *record, X_ITEM *result )
{
   X_DBF_TEXT   bytes __attribute__( ( cleanup( s_textDrop ) ) ) = { NULL, 0 };
   X_DBF_TEXT   text __attribute__( ( cleanup( s_textDrop ) ) ) = { NULL, 0 };
   uint32_t     type;
   int          binary;
   X_DBF_STATUS status = s_load( memo, field, record, &bytes, &binary, &type );
   if( status != X_DBF_OK )
      return status;
   if( memo->kind == DB_MEMO_SMT && type != 1 )
      return x_dbfSmtDecode( &bytes, type, result );
   if( memo->kind == DB_MEMO_FPT && type > 1 )
   {
      if( field->type != X_DBF_MEMO )
         return X_DBF_INVALID_FIELD_VALUE;
      return x_dbfFlexDecode( &bytes, type, result );
   }
   if( !binary )
   {
      status = x_dbfTextCopy( bytes.data, bytes.length, &text );
      if( status != X_DBF_OK )
         return status;
      x_itemPutStringBuffer( result, text.data, text.length );
      text.data = NULL;
   }
   else
   {
      x_itemPutStringBuffer( result, bytes.data, bytes.length );
      bytes.data = NULL;
   }
   return X_DBF_OK;
}

X_DBF_STATUS x_dbfMemoWriteValue( X_DBF_MEMO_FILE *memo, const X_DBF_FIELD *field, const X_ITEM *value, char *pointer )
{
   X_DBF_TEXT   bytes __attribute__( ( cleanup( s_textDrop ) ) ) = { NULL, 0 };
   uint32_t     type;
   X_DBF_STATUS status = s_ready( memo );
   if( status != X_DBF_OK )
      return status;
   if( value->nType == X_IT_STRING )
   {
      if( field->type == X_DBF_PICTURE )
         return x_dbfMemoWrite( memo, field, value->u.string.data, value->u.string.len, pointer );
      status = x_dbfTextCopy( value->u.string.data, value->u.string.len, &bytes );
      if( status != X_DBF_OK )
         return status;
      return x_dbfMemoWrite( memo, field, bytes.data, bytes.length, pointer );
   }
   if( field->type != X_DBF_MEMO )
      return X_DBF_INVALID_FIELD_VALUE;
   if( memo->kind == DB_MEMO_SMT )
   {
      status = x_dbfSmtEncode( value, &bytes );
      if( status == X_DBF_OK )
         type = ( unsigned char )bytes.data[0];
   }
   else if( memo->kind == DB_MEMO_FPT )
   {
      if( memo->version != DB_MEMOVER_FLEX )
         return X_DBF_UNSUPPORTED_FORMAT;
      status = x_dbfFlexEncode( value, &bytes, &type );
   }
   else
      return X_DBF_INVALID_FIELD_VALUE;
   if( status != X_DBF_OK )
      return status;
   return s_writeValue( memo, field, s_memoryRead, bytes.data, bytes.length, type, pointer );
}

X_DBF_STATUS x_dbfFieldReadMemo( DBF area, const X_DBF_FIELD *field, X_DBF_TEXT *result, int *binary )
{
   X_DBF_TEXT   bytes __attribute__( ( cleanup( s_textDrop ) ) ) = { NULL, 0 };
   X_DBF_STATUS status;
   int          raw;
   if( area->driver->operations.memoRead == NULL )
      return X_DBF_UNSUPPORTED_OPERATION;
   status = area->driver->operations.memoRead( area, field, area->record, &bytes, &raw );
   if( status != X_DBF_OK )
      return status;
   if( raw )
   {
      x_dbfTextClear( result );
      *result = bytes;
      bytes.data = NULL;
   }
   else
   {
      status = x_dbfTextCopy( bytes.data, bytes.length, result );
      if( status != X_DBF_OK )
         return status;
   }
   *binary = raw;
   return X_DBF_OK;
}

X_DBF_STATUS x_dbfFieldWriteMemo( DBF area, const X_DBF_FIELD *field, const char *data, size_t length )
{
   X_DBF_TEXT   encoded __attribute__( ( cleanup( s_textDrop ) ) ) = { NULL, 0 };
   char         pointer[10];
   X_DBF_STATUS status = x_dbfCheckWrite( area );
   if( status != X_DBF_OK )
      return status;
   if( area->driver->operations.memoWrite == NULL )
      return X_DBF_UNSUPPORTED_OPERATION;
   if( field->type != X_DBF_PICTURE )
   {
      status = x_dbfTextCopy( data, length, &encoded );
      if( status != X_DBF_OK )
         return status;
      data = encoded.data;
      length = encoded.length;
   }
   // The buffer starts with the current pointer of the record, so its old value can be reused
   memcpy( pointer, area->record + field->offset, field->length );
   status = area->driver->operations.memoWrite( area, field, data, length, pointer );
   if( status != X_DBF_OK )
      return status;
   memcpy( area->record + field->offset, pointer, field->length );
   area->state.dirty = 1;
   return X_DBF_OK;
}

typedef struct
{
   X_ITEM file;
   char  *temporary;
   int    closed;
} X_DBF_TRANSFER;

static void s_transferDrop( X_DBF_TRANSFER *transfer )
{
   if( transfer->file.nType != X_IT_NIL )
   {
      int error = x_fsError();
      if( !transfer->closed )
         x_fileClose( &transfer->file );
      x_gcUnlock( transfer->file.u.pPointer );
      x_itemClear( &transfer->file );
      if( transfer->temporary != NULL )
      {
         x_fsDelete( transfer->temporary );
         x_xfree( transfer->temporary );
      }
      x_fsSetError( error );
   }
}

static void s_transferSet( X_DBF_TRANSFER *transfer, void *handle )
{
   x_itemPutPtrGC( &transfer->file, handle );
   x_gcLock( handle );
}

static X_DBF_STATUS s_transferTemp( X_DBF_TRANSFER *transfer )
{
   void *handle = x_fileTemp( &transfer->temporary, "", "xdev-memo-transfer-", ".tmp", FC_NORMAL );
   if( handle == NULL )
      return X_DBF_CREATE_ERROR;
   s_transferSet( transfer, handle );
   return X_DBF_OK;
}

static X_DBF_STATUS s_transferCopy( const X_ITEM *source, const X_ITEM *target, uint64_t length, int64_t targetOffset )
{
   char     buffer[32768];
   uint64_t offset = 0;
   if( targetOffset < 0 || length > ( uint64_t )INT64_MAX || ( uint64_t )targetOffset > ( uint64_t )INT64_MAX - length )
      return X_DBF_FIELD_OVERFLOW;
   while( offset < length )
   {
      size_t       bytes = length - offset < sizeof( buffer ) ? ( size_t )( length - offset ) : sizeof( buffer );
      X_DBF_STATUS status = s_fileRead( source, buffer, bytes, offset );
      if( status == X_DBF_OK )
         status = s_fileWrite( target, buffer, bytes, targetOffset + ( int64_t )offset );
      if( status != X_DBF_OK )
         return status;
      offset += bytes;
   }
   return X_DBF_OK;
}

X_DBF_STATUS x_dbfFieldFileGet( DBF area, size_t ordinal, const char *path, int append )
{
   X_DBF_TRANSFER     transfer __attribute__( ( cleanup( s_transferDrop ) ) ) = { X_ITEM_NIL_INIT, NULL, 0 };
   X_DBF_TRANSFER     staged __attribute__( ( cleanup( s_transferDrop ) ) ) = { X_ITEM_NIL_INIT, NULL, 0 };
   const X_DBF_FIELD *field;
   X_DBF_STATUS       status = x_dbfAreaPrepare( area );
   int64_t            offset = 0, size;
   void              *handle;
   if( status != X_DBF_OK )
      return status;
   field = x_dbfSchemaField( area->schema, ordinal );
   if( ( field->type != X_DBF_MEMO && field->type != X_DBF_PICTURE ) || area->driver->operations.memoExport == NULL )
      return X_DBF_UNSUPPORTED_OPERATION;
   status = s_transferTemp( &staged );
   if( status == X_DBF_OK )
      status = area->driver->operations.memoExport( area, field, &staged.file );
   if( status != X_DBF_OK )
      return status;
   if( !x_fileCommit( &staged.file ) )
      return X_DBF_WRITE_ERROR;
   size = x_fileSize( &staged.file, 0 );
   if( size < 0 )
      return X_DBF_READ_ERROR;
   if( append )
   {
      handle = x_fileOpen( path, FO_WRITE | FO_EXCLUSIVE );
      if( handle == NULL && x_fsError() == 2 )
         handle = x_fileCreate( path, FC_NORMAL );
   }
   else
      handle = x_fileCreate( path, FC_NORMAL );
   if( handle == NULL )
      return append ? X_DBF_OPEN_ERROR : X_DBF_CREATE_ERROR;
   s_transferSet( &transfer, handle );
   if( append )
   {
      offset = x_fileSize( &transfer.file, 0 );
      if( offset < 0 )
         return X_DBF_READ_ERROR;
   }
   status = s_transferCopy( &staged.file, &transfer.file, ( uint64_t )size, offset );
   if( status != X_DBF_OK )
      return status;
   if( !x_fileCommit( &transfer.file ) )
      return X_DBF_WRITE_ERROR;
   transfer.closed = 1;
   return x_fileClose( &transfer.file ) ? X_DBF_OK : X_DBF_CLOSE_ERROR;
}

X_DBF_STATUS x_dbfFieldFilePut( DBF area, size_t ordinal, const char *path )
{
   X_DBF_TRANSFER     transfer __attribute__( ( cleanup( s_transferDrop ) ) ) = { X_ITEM_NIL_INIT, NULL, 0 };
   X_DBF_TRANSFER     staged __attribute__( ( cleanup( s_transferDrop ) ) ) = { X_ITEM_NIL_INIT, NULL, 0 };
   const X_DBF_FIELD *field;
   X_DBF_STATUS       status = x_dbfCheckWrite( area );
   int64_t            size;
   char               pointer[10];
   void              *handle;
   if( status != X_DBF_OK )
      return status;
   field = x_dbfSchemaField( area->schema, ordinal );
   if( ( field->type != X_DBF_MEMO && field->type != X_DBF_PICTURE ) || area->driver->operations.memoImport == NULL )
      return X_DBF_UNSUPPORTED_OPERATION;
   handle = x_fileOpen( path, FO_READ | FO_DENYNONE );
   if( handle == NULL )
      return X_DBF_OPEN_ERROR;
   s_transferSet( &transfer, handle );
   size = x_fileSize( &transfer.file, 0 );
   if( size < 0 )
      return X_DBF_READ_ERROR;
   if( ( uint64_t )size > UINT32_MAX )
      return X_DBF_FIELD_OVERFLOW;
   status = s_transferTemp( &staged );
   if( status == X_DBF_OK )
      status = s_transferCopy( &transfer.file, &staged.file, ( uint64_t )size, 0 );
   if( status != X_DBF_OK )
      return status;
   if( !x_fileCommit( &staged.file ) )
      return X_DBF_WRITE_ERROR;
   transfer.closed = 1;
   if( !x_fileClose( &transfer.file ) )
      return X_DBF_CLOSE_ERROR;
   memcpy( pointer, area->record + field->offset, field->length );
   status = area->driver->operations.memoImport( area, field, &staged.file, ( uint64_t )size, pointer );
   if( status == X_DBF_OK )
   {
      memcpy( area->record + field->offset, pointer, field->length );
      area->state.dirty = 1;
   }
   return status;
}

X_DBF_STATUS x_dbfMemoInfo( X_DBF_MEMO_FILE *memo, int selector, X_ITEM *result )
{
   X_ITEM       value = X_ITEM_NIL_INIT;
   X_DBF_STATUS status;
   if( memo == NULL )
   {
      if( selector == DBI_MEMOTYPE || selector == DBI_MEMOHANDLE )
      {
         value.nType = X_IT_INTEGER;
         value.u.integer = IIF( selector == DBI_MEMOTYPE, DB_MEMO_NONE, -1 );
         x_itemCopy( result, &value );
         return X_DBF_OK;
      }
      return X_DBF_UNSUPPORTED_OPERATION;
   }
   status = s_ready( memo );
   if( status != X_DBF_OK )
      return status;
   value.nType = X_IT_INTEGER;
   switch( selector )
   {
      case DBI_MEMOTYPE: value.u.integer = memo->kind; break;
      case DBI_MEMOBLOCKSIZE: value.u.integer = memo->blockSize; break;
      case DBI_MEMOHANDLE: value.u.integer = x_fileHandle( &memo->file ); break;
      case DBI_MEMOVERSION: value.u.integer = memo->version; break;
      case DBI_MEMOEXT:
      {
         const char *extension = memo->kind == DB_MEMO_DBT ? ".dbt" : ( memo->kind == DB_MEMO_FPT ? ".fpt" : ".smt" );
         x_itemPutStringBuffer( result, x_strdup( extension ), 4 );
         return X_DBF_OK;
      }
      default: return X_DBF_UNSUPPORTED_OPERATION;
   }
   x_itemCopy( result, &value );
   return X_DBF_OK;
}
