/*
 * Copyright (c) 2024-2026 xDev-Language
 *
 * Library: xDev 7.0.0 RDD
 * File: dbf-cdx.c
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "dbf-order.h"

#include <xdev/dbinfo.ch>
#include <string.h>

enum
{
   CDX_PAGE_SIZE = 512,
   CDX_HEADER_SIZE = 1024
};

typedef struct
{
   unsigned char *bytes;
   size_t         length;
   size_t         capacity;
   int            largeFile;
} X_CDX_IMAGE;

typedef struct
{
   uint32_t        page;
   X_DBF_INDEX_KEY last;
} X_CDX_NODE;

typedef struct _X_CDX_PAGE
{
   struct _X_CDX_PAGE *next;
   uint32_t            offset;
   unsigned char       bytes[CDX_PAGE_SIZE];
   int                 dirty;
   uint64_t            used;
} X_CDX_PAGE;

typedef struct
{
   X_CDX_PAGE *pages;
   uint64_t    end;
   uint32_t    freePage;
   size_t      count;
   uint64_t    clock;
} X_CDX_PAGES;

typedef struct
{
   X_DBF_INDEX_KEY keys[164];
   uint32_t        children[164];
   unsigned int    count, flags;
   uint32_t        left, right;
} X_CDX_EDIT;

typedef struct
{
   X_CDX_EDIT    node, sibling, parent;
   unsigned char left[CDX_PAGE_SIZE], right[CDX_PAGE_SIZE];
} X_CDX_CHANGE;

typedef struct
{
   X_DBF_INDEX_BAG *bag;
   X_DBF_ORDER      compare;
   uint32_t         path[65];
   uint64_t         pages;
   X_DBF_INDEX_KEY *directory;
   size_t           count;
   X_DBF_INDEX_KEY  previous;
   int              havePrevious;
} X_CDX_READER;

static unsigned int s_get16( const unsigned char *bytes )
{
   return ( unsigned int )bytes[0] | ( ( unsigned int )bytes[1] << 8 );
}

static uint32_t s_get32( const unsigned char *bytes, int big )
{
   uint32_t     value = 0;
   unsigned int i;
   for( i = 0; i < 4; ++i )
      value |= ( uint32_t )bytes[IIF( big, 3 - i, i )] << ( i * 8 );
   return value;
}

static X_DBF_STATUS s_read( X_DBF_INDEX_BAG *bag, unsigned char *bytes, size_t length, int64_t offset )
{
   size_t done = 0;
   while( done < length )
   {
      int64_t size = x_fileReadAt( &bag->file, bytes + done, length - done, offset + done );
      if( size <= 0 )
         return X_DBF_READ_ERROR;
      done += ( size_t )size;
   }
   return X_DBF_OK;
}

static void s_readerDrop( X_CDX_READER *reader )
{
   x_xfree( reader->directory );
}

static X_DBF_STATUS s_decode( const X_DBF_ORDER *index, const unsigned char *bytes, X_CDX_EDIT *node );

static void s_editDrop( X_CDX_EDIT **node )
{
   x_xfree( *node );
}

static X_DBF_STATUS s_readTree( X_CDX_READER *reader, uint32_t pointer, unsigned int depth )
{
   X_CDX_EDIT   *node __attribute__( ( cleanup( s_editDrop ) ) ) = NULL;
   unsigned char bytes[CDX_PAGE_SIZE];
   uint64_t      page = IIF( reader->bag->largeFile, pointer, pointer / CDX_PAGE_SIZE );
   unsigned int  slot, length = reader->compare.length, ancestor;
   X_DBF_STATUS  status;
   if( page == 0 || page >= reader->pages || depth > 64 || ( !reader->bag->largeFile && pointer % CDX_PAGE_SIZE != 0 ) )
      return X_DBF_CORRUPT_FILE;
   for( ancestor = 0; ancestor < depth; ++ancestor )
      if( reader->path[ancestor] == pointer )
         return X_DBF_CORRUPT_FILE;
   reader->path[depth] = pointer;
   status = s_read( reader->bag, bytes, sizeof( bytes ), ( int64_t )page * CDX_PAGE_SIZE );
   if( status != X_DBF_OK )
      return status;
   node = ( X_CDX_EDIT * )x_xgrab( sizeof( *node ) );
   status = s_decode( &reader->compare, bytes, node );
   if( status != X_DBF_OK )
      return status;
   if( !( node->flags & 2 ) )
   {
      for( slot = 0; slot < node->count; ++slot )
      {
         status = s_readTree( reader, node->children[slot], depth + 1 );
         if( status != X_DBF_OK )
            return status;
         if( !reader->havePrevious ||
             x_dbfOrderCompare( &reader->compare, &reader->previous, &node->keys[slot], length, 1 ) != 0 )
            return X_DBF_CORRUPT_FILE;
      }
   }
   else
   {
      for( slot = 0; slot < node->count; ++slot )
      {
         X_DBF_INDEX_KEY *key = &node->keys[slot];
         if( reader->havePrevious && x_dbfOrderCompare( &reader->compare, &reader->previous, key, length, 1 ) >= 0 )
            return X_DBF_CORRUPT_FILE;
         if( reader->count == PTRDIFF_MAX / sizeof( *reader->directory ) )
            return X_DBF_TEXT_OVERFLOW;
         reader->directory =
            ( X_DBF_INDEX_KEY * )x_xrealloc( reader->directory, ( reader->count + 1 ) * sizeof( *reader->directory ) );
         reader->directory[reader->count++] = *key;
         reader->previous = *key;
         reader->havePrevious = 1;
      }
   }
   return X_DBF_OK;
}

X_DBF_STATUS x_dbfCdxLoad( X_DBF_INDEX_BAG *bag, int force )
{
   X_CDX_READER  reader __attribute__( ( cleanup( s_readerDrop ) ) ) = { 0 };
   unsigned char header[CDX_HEADER_SIZE];
   X_DBF_ORDER  *index;
   uint32_t      version;
   int64_t       size;
   size_t        tag, unbound = 0;
   X_DBF_STATUS  status = s_read( bag, header + 4, 8, 4 );
   if( status != X_DBF_OK )
      return status;
   version = s_get32( header + 8, 1 );
   if( !force && version == bag->version && s_get32( header + 4, 0 ) == bag->cdxFreePage )
      return X_DBF_OK;
   x_dbfCdxDiscard( bag );
   status = s_read( bag, header, sizeof( header ), 0 );
   if( status != X_DBF_OK )
      return status;
   bag->cdxFreePage = s_get32( header + 4, 0 );
   for( index = bag->area->indexes; index != NULL; index = index->next )
      if( index->bag == bag )
         index->cursor.valid = 0;
   if( s_get16( header + 12 ) != 10 || ( header[14] & 0x80 ) == 0 || ( header[15] != 1 && header[15] != 0x21 ) ||
       s_get16( header + 16 ) != CDX_HEADER_SIZE || s_get16( header + 18 ) != CDX_PAGE_SIZE )
      return X_DBF_UNSUPPORTED_FORMAT;
   if( ( header[15] == 0x21 ) != ( bag->area->lockScheme == DB_DBFLOCK_HB64 ) )
      return X_DBF_INVALID_STATE;
   size = x_fileSize( &bag->file, 0 );
   if( size < CDX_HEADER_SIZE + CDX_PAGE_SIZE || size % CDX_PAGE_SIZE != 0 )
      return X_DBF_CORRUPT_FILE;
   reader.pages = ( uint64_t )size / CDX_PAGE_SIZE;
   bag->largeFile = header[15] == 0x21;
   reader.bag = bag;
   reader.compare.bag = bag;
   reader.compare.length = 10;
   reader.compare.type = 'C';
   status = s_readTree( &reader, s_get32( header, 0 ), 0 );
   if( status != X_DBF_OK )
      return status;
   for( tag = 0; tag < reader.count; ++tag )
   {
      int  known = 0;
      char name[11];
      memcpy( name, reader.directory[tag].value, 10 );
      name[10] = 0;
      for( index = bag->area->indexes; index != NULL; index = index->next )
      {
         size_t length = strlen( index->diskName );
         if( index->bag == bag && memcmp( name, index->diskName, length ) == 0 &&
             strspn( name + length, " " ) == 10 - length )
            known = 1;
      }
      if( !known )
         ++unbound;
   }
   for( index = bag->area->indexes; index != NULL; index = index->next )
   {
      unsigned int keyPos, keyLength, forPos, forLength;
      if( index->bag != bag )
         continue;
      for( tag = 0; tag < reader.count; ++tag )
      {
         size_t length = strlen( index->diskName );
         if( memcmp( reader.directory[tag].value, index->diskName, length ) == 0 )
         {
            unsigned int i;
            for( i = ( unsigned int )length; i < 10 && reader.directory[tag].value[i] == ' '; ++i )
            {
            }
            if( i == 10 )
               break;
         }
      }
      index->broken = 1;
      if( tag == reader.count )
         return X_DBF_ORDER_NOT_FOUND;
      index->cdxHeader = reader.directory[tag].record;
      {
         uint64_t offset = ( uint64_t )reader.directory[tag].record * IIF( bag->largeFile, CDX_PAGE_SIZE, 1 );
         if( offset % CDX_PAGE_SIZE != 0 || offset > ( uint64_t )size - CDX_HEADER_SIZE )
            return X_DBF_CORRUPT_FILE;
         status = s_read( bag, header, sizeof( header ), ( int64_t )offset );
      }
      if( status != X_DBF_OK )
         return status;
      index->cdxRoot = s_get32( header, 0 );
      keyPos = s_get16( header + 508 );
      keyLength = s_get16( header + 510 );
      forPos = s_get16( header + 504 );
      forLength = s_get16( header + 506 );
      if( keyLength == 0 || forLength == 0 || keyPos + keyLength > 512 || forPos + forLength > 512 ||
          memchr( header + 512 + keyPos, 0, keyLength ) == NULL ||
          memchr( header + 512 + forPos, 0, forLength ) == NULL )
         return X_DBF_CORRUPT_FILE;
      if( strcmp( ( const char * )header + 512 + keyPos, index->expression ) != 0 ||
          strcmp( ( const char * )header + 512 + forPos, index->condition ) != 0 )
         return X_DBF_INDEX_EXPRESSION_REQUIRED;
      if( index->opening )
      {
         unsigned int length = s_get16( header + 12 );
         if( length == 0 || length > 240 || ( index->type != 'C' && length != index->length ) )
            return X_DBF_INVALID_INDEX_KEY;
         index->length = ( uint16_t )length;
         index->stride = ( sizeof( uint32_t ) + length + 3 ) & ~( size_t )3;
         index->maximum = ( uint16_t )( ( NTX_PAGE_SIZE - 2 ) / ( length + 10 ) - 1 );
         index->maximum &= ( uint16_t )~1;
         index->unique = ( header[14] & 1 ) != 0;
         index->descending = s_get16( header + 502 ) != 0;
      }
      index->custom = ( header[14] & 4 ) != 0;
      index->partial = ( header[14] & 2 ) != 0;
      index->template = index->multiKey = index->custom;
      if( s_get16( header + 12 ) != index->length || ( ( header[14] & 1 ) != 0 ) != index->unique ||
          ( ( header[14] & 8 ) != 0 ) != ( index->forBlock.nType == X_IT_BLOCK ) ||
          ( s_get16( header + 502 ) != 0 ) != index->descending || header[499] == 1 || ( header[14] & 0x10 ) != 0 )
         return X_DBF_INVALID_INDEX_KEY;
      x_dbfOrderPagesClear( index );
      index->broken = index->changed = 0;
   }
   bag->unbound = unbound;
   bag->version = version;
   bag->cdxRebuild = 0;
   return X_DBF_OK;
}

static void s_imageDrop( X_CDX_IMAGE *image )
{
   x_xfree( image->bytes );
}

static void s_keysDrop( X_DBF_INDEX_KEY **keys )
{
   x_xfree( *keys );
}

static void s_nodesDrop( X_CDX_NODE **nodes )
{
   x_xfree( *nodes );
}

static void s_put16( unsigned char *bytes, unsigned int value )
{
   bytes[0] = ( unsigned char )value;
   bytes[1] = ( unsigned char )( value >> 8 );
}

static void s_put32( unsigned char *bytes, uint32_t value, int big )
{
   unsigned int i;
   for( i = 0; i < 4; ++i )
      bytes[IIF( big, 3 - i, i )] = ( unsigned char )( value >> ( i * 8 ) );
}

void x_dbfCdxDiscard( X_DBF_INDEX_BAG *bag )
{
   X_CDX_PAGES *pages = ( X_CDX_PAGES * )bag->cdxPages;
   if( pages != NULL )
   {
      while( pages->pages != NULL )
      {
         X_CDX_PAGE *page = pages->pages;
         pages->pages = page->next;
         x_xfree( page );
      }
      x_xfree( pages );
      bag->cdxPages = NULL;
   }
}

static X_DBF_STATUS s_state( X_DBF_INDEX_BAG *bag, X_CDX_PAGES **result )
{
   X_CDX_PAGES *pages = ( X_CDX_PAGES * )bag->cdxPages;
   if( pages == NULL )
   {
      unsigned char header[8];
      int64_t       end;
      X_DBF_STATUS  status = s_read( bag, header, sizeof( header ), 0 );
      if( status != X_DBF_OK )
         return status;
      end = x_fileSize( &bag->file, 0 );
      if( end < CDX_HEADER_SIZE + CDX_PAGE_SIZE || end % CDX_PAGE_SIZE != 0 )
         return X_DBF_CORRUPT_FILE;
      pages = ( X_CDX_PAGES * )x_xgrab( sizeof( *pages ) );
      memset( pages, 0, sizeof( *pages ) );
      pages->end = ( uint64_t )end;
      pages->freePage = s_get32( header + 4, 0 );
      bag->cdxPages = pages;
   }
   *result = pages;
   return X_DBF_OK;
}

static void s_trim( X_CDX_PAGES *pages, size_t limit )
{
   while( pages->count > limit )
   {
      X_CDX_PAGE **position, **oldest = NULL;
      for( position = &pages->pages; *position != NULL; position = &( *position )->next )
         if( !( *position )->dirty && ( oldest == NULL || ( *position )->used < ( *oldest )->used ) )
            oldest = position;
      if( oldest == NULL )
         break;
      {
         X_CDX_PAGE *page = *oldest;
         *oldest = page->next;
         x_xfree( page );
         --pages->count;
      }
   }
}

static X_DBF_STATUS s_cachedPage( X_DBF_INDEX_BAG *bag, uint32_t pointer, X_CDX_PAGE **result )
{
   X_CDX_PAGES *pages;
   X_CDX_PAGE  *page;
   uint64_t     offset = ( uint64_t )pointer * IIF( bag->largeFile, CDX_PAGE_SIZE, 1 );
   X_DBF_STATUS status = s_state( bag, &pages );
   if( status != X_DBF_OK )
      return status;
   if( offset % CDX_PAGE_SIZE != 0 || offset < CDX_HEADER_SIZE || offset > pages->end - CDX_PAGE_SIZE ||
       offset > UINT32_MAX - CDX_PAGE_SIZE )
      return X_DBF_CORRUPT_FILE;
   for( page = pages->pages; page != NULL; page = page->next )
   {
      if( page->offset == offset )
      {
         page->used = ++pages->clock;
         *result = page;
         return X_DBF_OK;
      }
   }
   if( bag->lockMode != 2 )
      s_trim( pages, 31 );
   page = ( X_CDX_PAGE * )x_xgrab( sizeof( *page ) );
   status = s_read( bag, page->bytes, CDX_PAGE_SIZE, ( int64_t )offset );
   if( status != X_DBF_OK )
   {
      x_xfree( page );
      return status;
   }
   page->offset = ( uint32_t )offset;
   page->dirty = 0;
   page->next = pages->pages;
   pages->pages = page;
   page->used = ++pages->clock;
   ++pages->count;
   *result = page;
   return X_DBF_OK;
}

static X_DBF_STATUS s_allocate( X_DBF_INDEX_BAG *bag, X_CDX_PAGE **result )
{
   X_CDX_PAGES *pages;
   X_CDX_PAGE  *page;
   X_DBF_STATUS status = s_state( bag, &pages );
   if( status != X_DBF_OK )
      return status;
   if( pages->freePage != 0 && pages->freePage != UINT32_MAX )
   {
      status = s_cachedPage( bag, pages->freePage, &page );
      if( status != X_DBF_OK )
         return status;
      pages->freePage = s_get32( page->bytes, 0 );
   }
   else
   {
      if( pages->end > UINT32_MAX - CDX_PAGE_SIZE )
         return X_DBF_TEXT_OVERFLOW;
      page = ( X_CDX_PAGE * )x_xgrab( sizeof( *page ) );
      page->offset = ( uint32_t )pages->end;
      pages->end += CDX_PAGE_SIZE;
      page->next = pages->pages;
      pages->pages = page;
      ++pages->count;
   }
   page->used = ++pages->clock;
   memset( page->bytes, 0, CDX_PAGE_SIZE );
   page->dirty = 1;
   *result = page;
   return X_DBF_OK;
}

static void s_release( X_DBF_INDEX_BAG *bag, X_CDX_PAGE *page )
{
   X_CDX_PAGES *pages = ( X_CDX_PAGES * )bag->cdxPages;
   memset( page->bytes, 0, CDX_PAGE_SIZE );
   s_put32( page->bytes, pages->freePage, 0 );
   pages->freePage = IIF( bag->largeFile, page->offset / CDX_PAGE_SIZE, page->offset );
   page->dirty = 1;
}

static X_DBF_STATUS s_decode( const X_DBF_ORDER *index, const unsigned char *bytes, X_CDX_EDIT *node )
{
   unsigned int slot, length = index->length;
   memset( node, 0, sizeof( *node ) );
   node->flags = s_get16( bytes );
   node->count = s_get16( bytes + 2 );
   node->left = s_get32( bytes + 4, 0 );
   node->right = s_get32( bytes + 8, 0 );
   if( ( node->flags & ~3U ) != 0 )
      return X_DBF_CORRUPT_FILE;
   if( node->flags & 2 )
   {
      unsigned int required = bytes[23], recordBits = bytes[20], dupBits = bytes[21], trailBits = bytes[22];
      unsigned int end = CDX_PAGE_SIZE;
      if( required < 3 || required > 6 || recordBits == 0 || recordBits > 48 || dupBits > 8 || trailBits > 8 ||
          recordBits + dupBits + trailBits != required * 8 ||
          s_get32( bytes + 14, 0 ) != IIF( recordBits >= 32, UINT32_MAX, ( UINT32_C( 1 ) << recordBits ) - 1 ) ||
          bytes[18] != ( ( 1U << dupBits ) - 1 ) || bytes[19] != ( ( 1U << trailBits ) - 1 ) ||
          node->count > ( CDX_PAGE_SIZE - 24 ) / required )
         return X_DBF_CORRUPT_FILE;
      for( slot = 0; slot < node->count; ++slot )
      {
         X_DBF_INDEX_KEY *key = &node->keys[slot];
         uint64_t         packed = 0;
         unsigned int     duplicate, trailing, size, i;
         for( i = 0; i < required; ++i )
            packed |= ( uint64_t )bytes[24 + slot * required + i] << ( i * 8 );
         key->record = ( uint32_t )packed & s_get32( bytes + 14, 0 );
         duplicate = ( unsigned int )( packed >> recordBits ) & bytes[18];
         trailing = ( unsigned int )( packed >> ( recordBits + dupBits ) ) & bytes[19];
         if( key->record == 0 || duplicate + trailing > length || ( slot == 0 && duplicate != 0 ) )
            return X_DBF_CORRUPT_FILE;
         size = length - duplicate - trailing;
         if( end < size || end - size < 24 + node->count * required )
            return X_DBF_CORRUPT_FILE;
         end -= size;
         if( duplicate != 0 )
            memcpy( key->value, node->keys[slot - 1].value, duplicate );
         memcpy( key->value + duplicate, bytes + end, size );
         memset( key->value + length - trailing, IIF( index->type == 'C', ' ', 0 ), trailing );
      }
      if( s_get16( bytes + 12 ) != end - 24 - node->count * required )
         return X_DBF_CORRUPT_FILE;
   }
   else
   {
      if( node->count == 0 || node->count > ( CDX_PAGE_SIZE - 12 ) / ( length + 8 ) )
         return X_DBF_CORRUPT_FILE;
      for( slot = 0; slot < node->count; ++slot )
      {
         const unsigned char *entry = bytes + 12 + slot * ( length + 8 );
         memcpy( node->keys[slot].value, entry, length );
         node->keys[slot].record = s_get32( entry + length, 1 );
         node->children[slot] = s_get32( entry + length + 4, 1 );
      }
   }
   {
      X_DBF_ORDER compare = *index;
      compare.descending = 0;
      for( slot = 1; slot < node->count; ++slot )
         if( x_dbfOrderCompare( &compare, &node->keys[slot - 1], &node->keys[slot], length, 1 ) >=
             IIF( index->custom && index->multiKey, 1, 0 ) )
            return X_DBF_CORRUPT_FILE;
   }
   return X_DBF_OK;
}

X_DBF_STATUS x_dbfCdxFind( X_DBF_ORDER *index, const X_DBF_INDEX_KEY *key, size_t length, int records, int strict,
                           int backwards, X_DBF_INDEX_CURSOR *cursor )
{
   X_CDX_EDIT  *node __attribute__( ( cleanup( s_editDrop ) ) ) = ( X_CDX_EDIT * )x_xgrab( sizeof( *node ) );
   X_CDX_PAGE  *page;
   X_DBF_ORDER  compare = *index;
   uint32_t     pointer = index->cdxRoot, path[64];
   unsigned int depth = 0, slot, ancestor;
   int          reverse = backwards != index->descending;
   X_DBF_STATUS status;
   cursor->valid = cursor->keyValid = 0;
   compare.descending = 0;
   for( ;; )
   {
      if( depth == 64 )
         return X_DBF_CORRUPT_FILE;
      for( ancestor = 0; ancestor < depth; ++ancestor )
         if( path[ancestor] == pointer )
            return X_DBF_CORRUPT_FILE;
      path[depth++] = pointer;
      status = s_cachedPage( index->bag, pointer, &page );
      if( status != X_DBF_OK )
         return status;
      status = s_decode( index, page->bytes, node );
      if( status != X_DBF_OK )
         return status;
      if( ( depth == 1 ) != ( ( node->flags & 1 ) != 0 ) )
         return X_DBF_CORRUPT_FILE;
      if( node->flags & 2 )
         break;
      if( key == NULL )
         slot = IIF( reverse, node->count - 1, 0 );
      else
      {
         int upper = IIF( reverse, !strict, strict );
         for( slot = 0; slot < node->count; ++slot )
         {
            int order = x_dbfOrderCompare( &compare, &node->keys[slot], key, length, records );
            if( order > 0 || ( order == 0 && !upper ) )
               break;
         }
         if( slot == node->count )
         {
            if( !reverse )
               return X_DBF_OK;
            --slot;
         }
      }
      pointer = node->children[slot];
   }
   if( key == NULL )
      slot = IIF( reverse && node->count != 0, node->count - 1, 0 );
   else
   {
      for( slot = 0; slot < node->count; ++slot )
      {
         int order = x_dbfOrderCompare( &compare, &node->keys[slot], key, length, records );
         if( order > 0 || ( order == 0 && ( IIF( reverse, strict, !strict ) ) ) )
            break;
      }
      if( reverse )
         slot = IIF( slot != 0, slot - 1, UINT32_MAX );
   }
   if( slot >= node->count )
   {
      uint32_t next = IIF( reverse, node->left, node->right );
      if( next == UINT32_MAX )
         return X_DBF_OK;
      status = s_cachedPage( index->bag, next, &page );
      if( status != X_DBF_OK )
         return status;
      status = s_decode( index, page->bytes, node );
      if( status != X_DBF_OK )
         return status;
      if( !( node->flags & 2 ) || node->count == 0 || IIF( reverse, node->right, node->left ) != pointer )
         return X_DBF_CORRUPT_FILE;
      pointer = next;
      slot = IIF( reverse, node->count - 1, 0 );
   }
   if( node->count != 0 )
   {
      cursor->key = node->keys[slot];
      cursor->page = pointer;
      cursor->slot = slot;
      cursor->version = index->bag->version;
      cursor->valid = cursor->keyValid = 1;
   }
   return X_DBF_OK;
}

X_DBF_STATUS x_dbfCdxStep( X_DBF_ORDER *index, X_DBF_INDEX_CURSOR *cursor, int backwards )
{
   X_CDX_EDIT  *node __attribute__( ( cleanup( s_editDrop ) ) ) = NULL;
   X_CDX_PAGE  *page;
   unsigned int slot;
   uint32_t     pointer;
   int          reverse = backwards != index->descending;
   X_DBF_STATUS status;
   if( !cursor->keyValid )
      return X_DBF_OK;
   if( !cursor->valid || cursor->version != index->bag->version )
      return x_dbfCdxFind( index, &cursor->key, index->length, 1, 1, backwards, cursor );
   node = ( X_CDX_EDIT * )x_xgrab( sizeof( *node ) );
   pointer = cursor->page;
   status = s_cachedPage( index->bag, pointer, &page );
   if( status != X_DBF_OK )
      return status;
   status = s_decode( index, page->bytes, node );
   if( status != X_DBF_OK )
      return status;
   if( !( node->flags & 2 ) || cursor->slot >= node->count ||
       x_dbfOrderCompare( index, &node->keys[cursor->slot], &cursor->key, index->length, 1 ) != 0 )
      return X_DBF_CORRUPT_FILE;
   slot = IIF( reverse, IIF( cursor->slot != 0, cursor->slot - 1, UINT32_MAX ), cursor->slot + 1 );
   if( slot >= node->count )
   {
      uint32_t next = IIF( reverse, node->left, node->right );
      if( next == UINT32_MAX )
      {
         cursor->valid = 0;
         return X_DBF_OK;
      }
      status = s_cachedPage( index->bag, next, &page );
      if( status != X_DBF_OK )
         return status;
      status = s_decode( index, page->bytes, node );
      if( status != X_DBF_OK )
         return status;
      if( !( node->flags & 2 ) || node->count == 0 || IIF( reverse, node->right, node->left ) != pointer )
         return X_DBF_CORRUPT_FILE;
      pointer = next;
      slot = IIF( reverse, node->count - 1, 0 );
   }
   {
      int order = x_dbfOrderCompare( index, &node->keys[slot], &cursor->key, index->length, 1 );
      if( IIF( backwards, order >= IIF( index->custom && index->multiKey, 1, 0 ),
               order <= IIF( index->custom && index->multiKey, -1, 0 ) ) )
         return X_DBF_CORRUPT_FILE;
   }
   cursor->key = node->keys[slot];
   cursor->page = pointer;
   cursor->slot = slot;
   return X_DBF_OK;
}

static int s_leafPack( unsigned char *bytes, const X_DBF_INDEX_KEY *keys, unsigned int count, unsigned int length,
                       unsigned char trail )
{
   unsigned int bits = 0, required = 3, recordBits, slot, end = CDX_PAGE_SIZE;
   unsigned int duplicates[164], trailing[164], dataLength = 0;
   uint32_t     maximum = 0, mask;
   for( slot = length; slot != 0; slot >>= 1 )
      ++bits;
   recordBits = required * 8 - bits * 2;
   for( slot = 0; slot < count; ++slot )
      if( keys[slot].record > maximum )
         maximum = keys[slot].record;
   mask = ( UINT32_C( 1 ) << recordBits ) - 1;
   while( maximum > mask )
   {
      ++required;
      recordBits += 8;
      mask = IIF( recordBits >= 32, UINT32_MAX, ( UINT32_C( 1 ) << recordBits ) - 1 );
   }
   if( count > ( CDX_PAGE_SIZE - 24 ) / required )
      return 0;
   for( slot = 0; slot < count; ++slot )
   {
      unsigned int duplicate = 0, tail = 0;
      while( tail < length && keys[slot].value[length - tail - 1] == trail )
         ++tail;
      if( slot != 0 )
         while( duplicate < length - tail && keys[slot].value[duplicate] == keys[slot - 1].value[duplicate] )
            ++duplicate;
      duplicates[slot] = duplicate;
      trailing[slot] = tail;
      dataLength += length - duplicate - tail;
   }
   if( 24 + count * required + dataLength > CDX_PAGE_SIZE )
      return 0;
   memset( bytes, 0, CDX_PAGE_SIZE );
   s_put16( bytes, 2 );
   s_put16( bytes + 2, count );
   s_put16( bytes + 12, CDX_PAGE_SIZE - 24 - count * required - dataLength );
   s_put32( bytes + 14, mask, 0 );
   bytes[18] = bytes[19] = ( unsigned char )( ( 1U << bits ) - 1 );
   bytes[20] = ( unsigned char )recordBits;
   bytes[21] = bytes[22] = ( unsigned char )bits;
   bytes[23] = ( unsigned char )required;
   for( slot = 0; slot < count; ++slot )
   {
      unsigned int size = length - duplicates[slot] - trailing[slot], i;
      uint64_t     packed = keys[slot].record | ( ( uint64_t )duplicates[slot] << recordBits ) |
                            ( ( uint64_t )trailing[slot] << ( recordBits + bits ) );
      for( i = 0; i < required; ++i )
         bytes[24 + slot * required + i] = ( unsigned char )( packed >> ( i * 8 ) );
      end -= size;
      memcpy( bytes + end, keys[slot].value + duplicates[slot], size );
   }
   return 1;
}

static int s_encode( const X_DBF_ORDER *index, const X_CDX_EDIT *node, unsigned char *bytes )
{
   unsigned int slot, length = index->length;
   if( node->flags & 2 )
   {
      if( !s_leafPack( bytes, node->keys, node->count, length, IIF( index->type == 'C', ' ', 0 ) ) )
         return 0;
   }
   else
   {
      if( node->count > ( CDX_PAGE_SIZE - 12 ) / ( length + 8 ) )
         return 0;
      memset( bytes, 0, CDX_PAGE_SIZE );
      s_put16( bytes + 2, node->count );
      for( slot = 0; slot < node->count; ++slot )
      {
         unsigned char *entry = bytes + 12 + slot * ( length + 8 );
         memcpy( entry, node->keys[slot].value, length );
         s_put32( entry + length, node->keys[slot].record, 1 );
         s_put32( entry + length + 4, node->children[slot], 1 );
      }
   }
   s_put16( bytes, node->flags );
   s_put32( bytes + 4, node->left, 0 );
   s_put32( bytes + 8, node->right, 0 );
   return 1;
}

static X_DBF_STATUS s_tagRoot( X_DBF_ORDER *index, X_CDX_PAGE *root )
{
   X_CDX_PAGE  *header;
   X_DBF_STATUS status = s_cachedPage( index->bag, index->cdxHeader, &header );
   if( status != X_DBF_OK )
      return status;
   index->cdxRoot = IIF( index->bag->largeFile, root->offset / CDX_PAGE_SIZE, root->offset );
   s_put32( header->bytes, index->cdxRoot, 0 );
   header->dirty = 1;
   return X_DBF_OK;
}

static void s_changeDrop( X_CDX_CHANGE **change )
{
   x_xfree( *change );
}

static X_DBF_STATUS s_changeKey( X_DBF_ORDER *index, const X_DBF_INDEX_KEY *key, int insert )
{
   X_CDX_CHANGE *change __attribute__( ( cleanup( s_changeDrop ) ) ) = ( X_CDX_CHANGE * )x_xgrab( sizeof( *change ) );
   X_CDX_EDIT   *node = &change->node;
   X_CDX_PAGE   *path[64], *page;
   unsigned int  choices[64], depth = 0, slot;
   X_DBF_ORDER   compare = *index;
   X_DBF_INDEX_BAG *bag = index->bag;
   X_DBF_STATUS     status = s_cachedPage( bag, index->cdxRoot, &page );
   compare.descending = 0;
   if( status != X_DBF_OK )
      return status;
   for( ;; )
   {
      status = s_decode( index, page->bytes, node );
      if( status != X_DBF_OK )
         return status;
      if( node->flags & 2 )
         break;
      if( depth == 64 )
         return X_DBF_CORRUPT_FILE;
      for( slot = 0;
           slot + 1 < node->count && x_dbfOrderCompare( &compare, &node->keys[slot], key, index->length, 1 ) < 0;
           ++slot )
      {
      }
      path[depth] = page;
      choices[depth++] = slot;
      status = s_cachedPage( bag, node->children[slot], &page );
      if( status != X_DBF_OK )
         return status;
   }
   for( slot = 0; slot < node->count && x_dbfOrderCompare( &compare, &node->keys[slot], key, index->length, 1 ) < 0;
        ++slot )
   {
   }
   if( insert )
   {
      if( slot < node->count && !( index->custom && index->multiKey ) &&
          x_dbfOrderCompare( &compare, &node->keys[slot], key, index->length, 1 ) == 0 )
         return X_DBF_CORRUPT_FILE;
      memmove( node->keys + slot + 1, node->keys + slot, ( node->count - slot ) * sizeof( *node->keys ) );
      node->keys[slot] = *key;
      ++node->count;
   }
   else
   {
      if( slot == node->count || x_dbfOrderCompare( &compare, &node->keys[slot], key, index->length, 1 ) != 0 )
         return X_DBF_CORRUPT_FILE;
      memmove( node->keys + slot, node->keys + slot + 1, ( node->count - slot - 1 ) * sizeof( *node->keys ) );
      --node->count;
   }
   for( ;; )
   {
      X_CDX_NODE   output[2];
      unsigned int span = 1, count = 0;
      uint32_t     pointer = IIF( bag->largeFile, page->offset / CDX_PAGE_SIZE, page->offset );
      if( !insert && depth != 0 && node->count != 0 )
      {
         X_CDX_EDIT *parent = &change->parent, *sibling = &change->sibling;
         status = s_decode( index, path[depth - 1]->bytes, parent );
         if( status != X_DBF_OK )
            return status;
         slot = choices[depth - 1];
         if( parent->count > 1 )
         {
            unsigned int other = IIF( slot + 1 < parent->count, slot + 1, slot - 1 );
            X_CDX_PAGE  *neighbor;
            status = s_cachedPage( bag, parent->children[other], &neighbor );
            if( status != X_DBF_OK )
               return status;
            status = s_decode( index, neighbor->bytes, sibling );
            if( status != X_DBF_OK )
               return status;
            if( ( sibling->flags & 2 ) != ( node->flags & 2 ) )
               return X_DBF_CORRUPT_FILE;
            if( node->count + sibling->count < 164 )
            {
               X_CDX_EDIT  merged = { 0 };
               X_CDX_EDIT *left = IIF( other < slot, sibling, node );
               X_CDX_EDIT *right = IIF( other < slot, node, sibling );
               X_CDX_PAGE *leftPage = IIF( other < slot, neighbor, page );
               X_CDX_PAGE *rightPage = IIF( other < slot, page, neighbor );
               uint32_t    leftPointer = IIF( bag->largeFile, leftPage->offset / CDX_PAGE_SIZE, leftPage->offset );
               merged.flags = node->flags & ~1U;
               merged.left = left->left;
               merged.right = right->right;
               merged.count = left->count + right->count;
               memcpy( merged.keys, left->keys, left->count * sizeof( *merged.keys ) );
               memcpy( merged.keys + left->count, right->keys, right->count * sizeof( *merged.keys ) );
               memcpy( merged.children, left->children, left->count * sizeof( *merged.children ) );
               memcpy( merged.children + left->count, right->children, right->count * sizeof( *merged.children ) );
               if( s_encode( index, &merged, change->left ) )
               {
                  if( merged.right != UINT32_MAX )
                  {
                     X_CDX_PAGE *next;
                     status = s_cachedPage( bag, merged.right, &next );
                     if( status != X_DBF_OK )
                        return status;
                     s_put32( next->bytes + 4, leftPointer, 0 );
                     next->dirty = 1;
                  }
                  s_release( bag, rightPage );
                  *node = merged;
                  page = leftPage;
                  pointer = leftPointer;
                  choices[depth - 1] = IIF( other < slot, other, slot );
                  span = 2;
               }
            }
         }
      }
      if( depth == 0 )
      {
         while( !( node->flags & 2 ) && node->count == 1 )
         {
            X_CDX_PAGE *child;
            status = s_cachedPage( bag, node->children[0], &child );
            if( status != X_DBF_OK )
               return status;
            s_release( bag, page );
            page = child;
            status = s_decode( index, page->bytes, node );
            if( status != X_DBF_OK )
               return status;
            node->flags |= 1;
            node->left = node->right = UINT32_MAX;
            status = s_tagRoot( index, page );
            if( status != X_DBF_OK )
               return status;
         }
         pointer = IIF( bag->largeFile, page->offset / CDX_PAGE_SIZE, page->offset );
      }
      if( node->count == 0 && depth != 0 )
      {
         if( node->left != UINT32_MAX )
         {
            X_CDX_PAGE *neighbor;
            status = s_cachedPage( bag, node->left, &neighbor );
            if( status != X_DBF_OK )
               return status;
            s_put32( neighbor->bytes + 8, node->right, 0 );
            neighbor->dirty = 1;
         }
         if( node->right != UINT32_MAX )
         {
            X_CDX_PAGE *neighbor;
            status = s_cachedPage( bag, node->right, &neighbor );
            if( status != X_DBF_OK )
               return status;
            s_put32( neighbor->bytes + 4, node->left, 0 );
            neighbor->dirty = 1;
         }
         s_release( bag, page );
      }
      else
      {
         if( node->count == 0 )
            node->flags = 3;
         if( s_encode( index, node, change->left ) )
         {
            memcpy( page->bytes, change->left, CDX_PAGE_SIZE );
            page->dirty = 1;
            output[0].page = pointer;
            if( node->count != 0 )
               output[0].last = node->keys[node->count - 1];
            count = 1;
         }
         else
         {
            X_CDX_PAGE  *right;
            X_CDX_EDIT  *part = &change->sibling;
            unsigned int middle, selected = 0;
            for( middle = 1; middle < node->count; ++middle )
            {
               *part = *node;
               part->flags &= ~1U;
               part->count = middle;
               if( !s_encode( index, part, change->left ) )
                  continue;
               part->count = node->count - middle;
               memcpy( part->keys, node->keys + middle, part->count * sizeof( *part->keys ) );
               memcpy( part->children, node->children + middle, part->count * sizeof( *part->children ) );
               if( s_encode( index, part, change->right ) )
               {
                  selected = middle;
                  if( middle >= node->count / 2 )
                     break;
               }
            }
            if( selected == 0 )
               return X_DBF_INVALID_INDEX_KEY;
            status = s_allocate( bag, &right );
            if( status != X_DBF_OK )
               return status;
            output[0].page = pointer;
            output[0].last = node->keys[selected - 1];
            output[1].page = IIF( bag->largeFile, right->offset / CDX_PAGE_SIZE, right->offset );
            output[1].last = node->keys[node->count - 1];
            *part = *node;
            part->flags &= ~1U;
            part->count = selected;
            part->right = output[1].page;
            s_encode( index, part, page->bytes );
            part->count = node->count - selected;
            memcpy( part->keys, node->keys + selected, part->count * sizeof( *part->keys ) );
            memcpy( part->children, node->children + selected, part->count * sizeof( *part->children ) );
            part->left = pointer;
            part->right = node->right;
            s_encode( index, part, right->bytes );
            page->dirty = right->dirty = 1;
            if( node->right != UINT32_MAX )
            {
               X_CDX_PAGE *neighbor;
               status = s_cachedPage( bag, node->right, &neighbor );
               if( status != X_DBF_OK )
                  return status;
               s_put32( neighbor->bytes + 4, output[1].page, 0 );
               neighbor->dirty = 1;
            }
            count = 2;
         }
      }
      if( depth == 0 )
      {
         if( count == 2 )
         {
            X_CDX_PAGE *root;
            status = s_allocate( bag, &root );
            if( status != X_DBF_OK )
               return status;
            memset( node, 0, sizeof( *node ) );
            node->count = 2;
            node->flags = 1;
            node->left = node->right = UINT32_MAX;
            node->keys[0] = output[0].last;
            node->keys[1] = output[1].last;
            node->children[0] = output[0].page;
            node->children[1] = output[1].page;
            s_encode( index, node, root->bytes );
            root->dirty = 1;
            status = s_tagRoot( index, root );
            if( status != X_DBF_OK )
               return status;
         }
         return X_DBF_OK;
      }
      status = s_decode( index, path[depth - 1]->bytes, &change->parent );
      if( status != X_DBF_OK )
         return status;
      slot = choices[depth - 1];
      if( span == 1 && count == 1 &&
          x_dbfOrderCompare( &compare, &change->parent.keys[slot], &output[0].last, index->length, 1 ) == 0 )
         return X_DBF_OK;
      memmove( change->parent.keys + slot + count, change->parent.keys + slot + span,
               ( change->parent.count - slot - span ) * sizeof( *change->parent.keys ) );
      memmove( change->parent.children + slot + count, change->parent.children + slot + span,
               ( change->parent.count - slot - span ) * sizeof( *change->parent.children ) );
      change->parent.count = change->parent.count - span + count;
      for( span = 0; span < count; ++span )
      {
         change->parent.keys[slot + span] = output[span].last;
         change->parent.children[slot + span] = output[span].page;
      }
      *node = change->parent;
      page = path[--depth];
   }
}

X_DBF_STATUS x_dbfCdxChange( X_DBF_ORDER *index, const X_DBF_INDEX_KEY *removed, const X_DBF_INDEX_KEY *added )
{
   X_DBF_INDEX_CURSOR cursor = { 0 };
   X_DBF_STATUS       status = X_DBF_OK;
   int                changed = 0;
   if( index->bag->cdxRebuild )
      return X_DBF_OK;
   if( removed != NULL && added != NULL && x_dbfOrderCompare( index, removed, added, index->length, 1 ) == 0 )
      return X_DBF_OK;
   if( removed != NULL )
   {
      status = x_dbfCdxFind( index, removed, index->length, 1, 0, 0, &cursor );
      if( status == X_DBF_OK && cursor.valid &&
          x_dbfOrderCompare( index, &cursor.key, removed, index->length, 1 ) == 0 )
      {
         status = s_changeKey( index, removed, 0 );
         changed = 1;
      }
   }
   if( status == X_DBF_OK && added != NULL )
   {
      status = x_dbfCdxFind( index, added, index->length, !index->unique, 0, 0, &cursor );
      if( status == X_DBF_OK &&
          ( !cursor.valid || x_dbfOrderCompare( index, &cursor.key, added, index->length, !index->unique ) != 0 ||
            ( index->custom && index->multiKey && !index->unique ) ) )
      {
         status = s_changeKey( index, added, 1 );
         changed = 1;
      }
   }
   if( changed )
   {
      index->changed = 1;
      index->cursor.valid = 0;
      if( added != NULL && index->cursor.keyValid && index->cursor.key.record == added->record )
         index->cursor.key = *added;
   }
   return status;
}

static X_DBF_STATUS s_write( X_DBF_INDEX_BAG *bag, const unsigned char *bytes, size_t length, int64_t offset )
{
   size_t done = 0;
   while( done < length )
   {
      int64_t size = x_fileWriteAt( &bag->file, bytes + done, length - done, offset + done );
      if( size <= 0 )
         return X_DBF_WRITE_ERROR;
      done += ( size_t )size;
   }
   return X_DBF_OK;
}

static uint32_t s_page( X_CDX_IMAGE *image )
{
   uint32_t offset;
   if( image->length > UINT32_MAX - CDX_PAGE_SIZE )
      return 0;
   offset = ( uint32_t )image->length;
   if( image->length + CDX_PAGE_SIZE > image->capacity )
   {
      image->capacity = IIF( image->capacity < 4096, 4096, image->capacity * 2 );
      image->bytes = ( unsigned char * )x_xrealloc( image->bytes, image->capacity );
   }
   memset( image->bytes + offset, 0, CDX_PAGE_SIZE );
   image->length += CDX_PAGE_SIZE;
   return offset;
}

static X_DBF_STATUS s_tree( X_CDX_IMAGE *image, const X_DBF_INDEX_KEY *keys, size_t count, unsigned int length,
                            unsigned char trail, uint32_t *root )
{
   X_CDX_NODE  *nodes __attribute__( ( cleanup( s_nodesDrop ) ) ) = NULL;
   size_t       position = 0, nodeCount = 0, nodeCapacity = 0;
   unsigned int bits = 0, i;
   uint32_t     previous = UINT32_MAX;

   for( i = length; i != 0; i >>= 1 )
      ++bits;
   do
   {
      unsigned int   entries = 0, dataLength = 0, required = 3, recordBits = 24 - bits * 2;
      uint32_t       mask = ( UINT32_C( 1 ) << recordBits ) - 1;
      uint32_t       offset = s_page( image );
      unsigned char *page;
      if( offset == 0 )
         return X_DBF_TEXT_OVERFLOW;
      while( position + entries < count )
      {
         const X_DBF_INDEX_KEY *key = &keys[position + entries];
         unsigned int           duplicate = 0, tail = 0, bytes, candidate = required;
         while( tail < length && key->value[length - tail - 1] == trail )
            ++tail;
         if( entries != 0 )
         {
            const X_DBF_INDEX_KEY *prior = key - 1;
            while( duplicate < length - tail && key->value[duplicate] == prior->value[duplicate] )
               ++duplicate;
         }
         while( key->record > mask )
         {
            ++candidate;
            recordBits += 8;
            mask = IIF( recordBits >= 32, UINT32_MAX, ( UINT32_C( 1 ) << recordBits ) - 1 );
         }
         bytes = length - duplicate - tail;
         if( 24 + ( entries + 1 ) * candidate + dataLength + bytes > CDX_PAGE_SIZE )
         {
            recordBits = required * 8 - bits * 2;
            mask = IIF( recordBits >= 32, UINT32_MAX, ( UINT32_C( 1 ) << recordBits ) - 1 );
            break;
         }
         required = candidate;
         ++entries;
         dataLength += bytes;
      }
      page = image->bytes + offset;
      if( !s_leafPack( page, IIF( entries != 0, keys + position, NULL ), entries, length, trail ) )
         return X_DBF_INVALID_INDEX_KEY;
      s_put32( page + 4, IIF( previous == UINT32_MAX || !image->largeFile, previous, previous / CDX_PAGE_SIZE ), 0 );
      s_put32( page + 8, UINT32_MAX, 0 );
      if( previous != UINT32_MAX )
         s_put32( image->bytes + previous + 8, IIF( image->largeFile, offset / CDX_PAGE_SIZE, offset ), 0 );
      if( nodeCount == nodeCapacity )
      {
         nodeCapacity = IIF( nodeCapacity == 0, 16, nodeCapacity * 2 );
         nodes = ( X_CDX_NODE * )x_xrealloc( nodes, nodeCapacity * sizeof( *nodes ) );
      }
      nodes[nodeCount].page = offset;
      memset( &nodes[nodeCount].last, 0, sizeof( nodes[nodeCount].last ) );
      if( entries != 0 )
         nodes[nodeCount].last = keys[position + entries - 1];
      ++nodeCount;
      previous = offset;
      position += entries;
   } while( position < count );

   while( nodeCount > 1 )
   {
      size_t input = 0, output = 0;
      previous = UINT32_MAX;
      while( input < nodeCount )
      {
         unsigned int   entries = ( CDX_PAGE_SIZE - 12 ) / ( length + 8 );
         uint32_t       offset = s_page( image );
         unsigned char *page;
         if( offset == 0 )
            return X_DBF_TEXT_OVERFLOW;
         if( entries > nodeCount - input )
            entries = ( unsigned int )( nodeCount - input );
         page = image->bytes + offset;
         s_put16( page + 2, entries );
         s_put32( page + 4, IIF( previous == UINT32_MAX || !image->largeFile, previous, previous / CDX_PAGE_SIZE ), 0 );
         s_put32( page + 8, UINT32_MAX, 0 );
         for( i = 0; i < entries; ++i )
         {
            const X_CDX_NODE *child = &nodes[input + i];
            unsigned char    *entry = page + 12 + i * ( length + 8 );
            memcpy( entry, child->last.value, length );
            s_put32( entry + length, child->last.record, 1 );
            s_put32( entry + length + 4, IIF( image->largeFile, child->page / CDX_PAGE_SIZE, child->page ), 1 );
         }
         if( previous != UINT32_MAX )
            s_put32( image->bytes + previous + 8, IIF( image->largeFile, offset / CDX_PAGE_SIZE, offset ), 0 );
         nodes[output].page = offset;
         nodes[output++].last = nodes[input + entries - 1].last;
         input += entries;
         previous = offset;
      }
      nodeCount = output;
   }
   *root = nodes[0].page;
   image->bytes[*root] |= 1;
   return X_DBF_OK;
}

X_DBF_STATUS x_dbfCdxSync( X_DBF_INDEX_BAG *bag )
{
   X_DBF_ORDER *index;
   X_DBF_STATUS status = X_DBF_OK;
   int          changed = 0;
   size_t       count = 0;
   if( bag->unbound != 0 )
      return X_DBF_INDEX_EXPRESSION_REQUIRED;
   for( index = bag->area->indexes; index != NULL; index = index->next )
   {
      if( index->bag == bag )
      {
         if( index->broken )
            return X_DBF_RECOVERY_REQUIRED;
         changed |= index->changed;
         ++count;
      }
   }
   if( !changed )
      return X_DBF_OK;
   if( bag->cdxRebuild )
   {
      X_CDX_IMAGE      image __attribute__( ( cleanup( s_imageDrop ) ) ) = { NULL, 0, 0, bag->largeFile };
      X_DBF_INDEX_KEY *directory __attribute__( ( cleanup( s_keysDrop ) ) ) = NULL;
      X_DBF_ORDER      directoryOrder = { 0 };
      size_t           tag = 0;
      uint32_t         root;
      if( count > UINT32_MAX / CDX_HEADER_SIZE - 1 )
         return X_DBF_TEXT_OVERFLOW;
      image.length = image.capacity = ( count + 1 ) * CDX_HEADER_SIZE;
      image.bytes = ( unsigned char * )x_xgrab( image.capacity );
      memset( image.bytes, 0, image.length );
      directory = ( X_DBF_INDEX_KEY * )x_xgrab( IIF( count == 0, 1, count * sizeof( *directory ) ) );
      directoryOrder.bag = bag;
      directoryOrder.length = 10;
      for( index = bag->area->indexes; index != NULL; index = index->next )
      {
         if( index->bag == bag )
         {
            X_DBF_INDEX_KEY *keys __attribute__( ( cleanup( s_keysDrop ) ) ) = NULL;
            uint64_t         size = 0, rank;
            size_t           capacity = 0;
            uint32_t         header = ( uint32_t )( ( tag + 1 ) * CDX_HEADER_SIZE );
            unsigned char   *bytes;
            size_t           keyLength = strlen( index->expression ), forLength = strlen( index->condition ), slot;
            if( index->root != NULL )
            {
               size = index->root->total;
               if( size > PTRDIFF_MAX / sizeof( *keys ) )
                  return X_DBF_TEXT_OVERFLOW;
               keys = ( X_DBF_INDEX_KEY * )x_xgrab( IIF( size == 0, 1, ( size_t )size * sizeof( *keys ) ) );
               for( rank = 0; rank < size; ++rank )
                  x_dbfOrderKeyAt( index, IIF( index->descending, size - rank - 1, rank ), &keys[rank] );
            }
            else
            {
               X_DBF_INDEX_CURSOR cursor = { 0 };
               status = x_dbfCdxFind( index, NULL, 0, 0, 0, index->descending, &cursor );
               while( status == X_DBF_OK && cursor.valid )
               {
                  if( size == capacity )
                  {
                     if( capacity > PTRDIFF_MAX / sizeof( *keys ) / 2 )
                        return X_DBF_TEXT_OVERFLOW;
                     capacity = IIF( capacity == 0, 64, capacity * 2 );
                     keys = ( X_DBF_INDEX_KEY * )x_xrealloc( keys, capacity * sizeof( *keys ) );
                  }
                  keys[size++] = cursor.key;
                  status = x_dbfCdxStep( index, &cursor, index->descending );
               }
               if( status != X_DBF_OK )
                  return status;
            }
            status = s_tree( &image, keys, ( size_t )size, index->length,
                             ( unsigned char )IIF( index->type == 'C', ' ', 0 ), &root );
            if( status != X_DBF_OK )
               return status;
            bytes = image.bytes + header;
            index->cdxHeader = IIF( bag->largeFile, header / CDX_PAGE_SIZE, header );
            index->cdxRoot = IIF( bag->largeFile, root / CDX_PAGE_SIZE, root );
            s_put32( bytes, IIF( bag->largeFile, root / CDX_PAGE_SIZE, root ), 0 );
            s_put16( bytes + 12, index->length );
            bytes[14] =
               ( unsigned char )( 0x60 | IIF( index->unique, 1, 0 ) | IIF( index->forBlock.nType != X_IT_NIL, 8, 0 ) |
                                  IIF( index->partial, 2, 0 ) | IIF( index->custom, 4, 0 ) );
            bytes[15] = IIF( bag->largeFile, 0x21, 1 );
            s_put16( bytes + 16, CDX_HEADER_SIZE );
            s_put16( bytes + 18, CDX_PAGE_SIZE );
            s_put16( bytes + 502, index->descending );
            s_put16( bytes + 504, ( unsigned int )keyLength + 1 );
            s_put16( bytes + 506, ( unsigned int )forLength + 1 );
            s_put16( bytes + 510, ( unsigned int )keyLength + 1 );
            memcpy( bytes + 512, index->expression, keyLength );
            memcpy( bytes + 513 + keyLength, index->condition, forLength );
            directory[tag].record = IIF( bag->largeFile, header / CDX_PAGE_SIZE, header );
            memset( directory[tag].value, ' ', 10 );
            memcpy( directory[tag].value, index->diskName, strlen( index->diskName ) );
            for( slot = tag;
                 slot != 0 && x_dbfOrderCompare( &directoryOrder, &directory[slot], &directory[slot - 1], 10, 1 ) < 0;
                 --slot )
            {
               X_DBF_INDEX_KEY swap = directory[slot];
               directory[slot] = directory[slot - 1];
               directory[slot - 1] = swap;
            }
            ++tag;
         }
      }
      status = s_tree( &image, directory, count, 10, ' ', &root );
      if( status != X_DBF_OK )
         return status;
      s_put32( image.bytes, IIF( bag->largeFile, root / CDX_PAGE_SIZE, root ), 0 );
      s_put32( image.bytes + 4, UINT32_MAX, 0 );
      s_put32( image.bytes + 8, ++bag->version, 1 );
      s_put16( image.bytes + 12, 10 );
      image.bytes[14] = 0xE0;
      image.bytes[15] = IIF( bag->largeFile, 0x21, 1 );
      s_put16( image.bytes + 16, CDX_HEADER_SIZE );
      s_put16( image.bytes + 18, CDX_PAGE_SIZE );
      s_put32( image.bytes + 20, UINT32_C( 0x52434842 ), 1 );
      s_put16( image.bytes + 506, 1 );
      s_put16( image.bytes + 510, 1 );
      status = s_write( bag, image.bytes, image.length, 0 );
      if( status == X_DBF_OK && ( !x_fileTrunc( &bag->file, ( int64_t )image.length ) || !x_fileCommit( &bag->file ) ) )
         status = X_DBF_WRITE_ERROR;
   }
   else
   {
      X_CDX_PAGES  *pages = ( X_CDX_PAGES * )bag->cdxPages;
      X_CDX_PAGE   *page;
      unsigned char header[8];
      if( pages == NULL )
      {
         status = s_state( bag, &pages );
         if( status != X_DBF_OK )
            return status;
      }
      for( page = pages->pages; page != NULL && status == X_DBF_OK; page = page->next )
         if( page->dirty )
            status = s_write( bag, page->bytes, CDX_PAGE_SIZE, page->offset );
      for( index = bag->area->indexes; index != NULL && status == X_DBF_OK; index = index->next )
      {
         if( index->bag == bag && index->headerChanged )
         {
            unsigned char flags;
            int64_t       offset = ( int64_t )index->cdxHeader * IIF( bag->largeFile, CDX_PAGE_SIZE, 1 ) + 14;
            flags =
               ( unsigned char )( 0x60 | IIF( index->unique, 1, 0 ) | IIF( index->forBlock.nType != X_IT_NIL, 8, 0 ) |
                                  IIF( index->partial, 2, 0 ) | IIF( index->custom, 4, 0 ) );
            status = s_write( bag, &flags, 1, offset );
         }
      }
      if( status == X_DBF_OK )
      {
         s_put32( header, pages->freePage, 0 );
         s_put32( header + 4, ++bag->version, 1 );
         // No forced write to disk here: x_dbfOrderCommit() does it on DbCommit() and close
         status = s_write( bag, header, sizeof( header ), 4 );
      }
   }
   for( index = bag->area->indexes; index != NULL; index = index->next )
   {
      if( index->bag == bag )
      {
         index->broken = status != X_DBF_OK;
         if( status == X_DBF_OK )
            index->changed = index->headerChanged = 0;
      }
   }
   if( status == X_DBF_OK )
   {
      X_CDX_PAGES *pages = ( X_CDX_PAGES * )bag->cdxPages;
      if( bag->cdxRebuild )
      {
         bag->cdxFreePage = UINT32_MAX;
         x_dbfCdxDiscard( bag );
         for( index = bag->area->indexes; index != NULL; index = index->next )
            if( index->bag == bag )
            {
               x_dbfOrderPagesClear( index );
               index->cursor.valid = 0;
            }
      }
      else
      {
         X_CDX_PAGE *page;
         bag->cdxFreePage = pages->freePage;
         for( page = pages->pages; page != NULL; page = page->next )
            page->dirty = 0;
         s_trim( pages, 32 );
      }
      bag->cdxRebuild = 0;
   }
   else
      x_dbfCdxDiscard( bag );
   return status;
}
