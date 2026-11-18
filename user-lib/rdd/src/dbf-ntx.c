/*
 * Copyright (c) 2024-2026 xDev-Language
 *
 * Library: xDev 7.0.0 RDD
 * File: dbf-ntx.c
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "dbf-order.h"

#include <xdev/dbinfo.ch>
#include <string.h>

enum
{
   NTX_MAX_KEYS = 90
};

typedef struct _X_NTX_PAGE
{
   struct _X_NTX_PAGE *next;
   uint32_t            number;
   unsigned char       bytes[NTX_PAGE_SIZE];
   uint64_t            used;
   int                 dirty;
} X_NTX_PAGE;

typedef struct
{
   X_NTX_PAGE *pages;
   uint64_t    end;
   uint64_t    clock;
   size_t      count;
} X_NTX_PAGES;

typedef struct
{
   X_DBF_INDEX_KEY keys[NTX_MAX_KEYS + 1];
   uint32_t        children[NTX_MAX_KEYS + 2];
   unsigned int    count;
} X_NTX_NODE;

static uint16_t s_get16( const unsigned char *bytes )
{
   return ( uint16_t )( bytes[0] | ( ( uint16_t )bytes[1] << 8 ) );
}

static uint32_t s_get32( const unsigned char *bytes )
{
   return ( uint32_t )bytes[0] | ( ( uint32_t )bytes[1] << 8 ) | ( ( uint32_t )bytes[2] << 16 ) |
          ( ( uint32_t )bytes[3] << 24 );
}

static void s_put16( unsigned char *bytes, uint16_t value )
{
   bytes[0] = ( unsigned char )value;
   bytes[1] = ( unsigned char )( value >> 8 );
}

static void s_put32( unsigned char *bytes, uint32_t value )
{
   bytes[0] = ( unsigned char )value;
   bytes[1] = ( unsigned char )( value >> 8 );
   bytes[2] = ( unsigned char )( value >> 16 );
   bytes[3] = ( unsigned char )( value >> 24 );
}

static X_DBF_STATUS s_read( X_DBF_INDEX_BAG *bag, unsigned char *bytes, int64_t offset )
{
   size_t done = 0;
   while( done < NTX_PAGE_SIZE )
   {
      int64_t size = x_fileReadAt( &bag->file, bytes + done, NTX_PAGE_SIZE - done, offset + done );
      if( size <= 0 )
         return X_DBF_READ_ERROR;
      done += ( size_t )size;
   }
   return X_DBF_OK;
}

static X_DBF_STATUS s_write( X_DBF_ORDER *index, const unsigned char *bytes, uint32_t page )
{
   size_t written = 0;
   while( written < NTX_PAGE_SIZE )
   {
      int64_t size = x_fileWriteAt( &index->bag->file, bytes + written, NTX_PAGE_SIZE - written,
                                    ( int64_t )page * NTX_PAGE_SIZE + written );
      if( size <= 0 )
         return X_DBF_WRITE_ERROR;
      written += ( size_t )size;
   }
   return X_DBF_OK;
}

void x_dbfNtxDiscard( X_DBF_ORDER *index )
{
   X_NTX_PAGES *pages = ( X_NTX_PAGES * )index->ntxPages;
   if( pages != NULL )
   {
      while( pages->pages != NULL )
      {
         X_NTX_PAGE *page = pages->pages;
         pages->pages = page->next;
         x_xfree( page );
      }
      x_xfree( pages );
      index->ntxPages = NULL;
   }
}

static void s_trim( X_NTX_PAGES *pages, size_t limit )
{
   while( pages->count > limit )
   {
      X_NTX_PAGE **position, **oldest = NULL;
      for( position = &pages->pages; *position != NULL; position = &( *position )->next )
         if( !( *position )->dirty && ( oldest == NULL || ( *position )->used < ( *oldest )->used ) )
            oldest = position;
      if( oldest == NULL )
         break;
      {
         X_NTX_PAGE *page = *oldest;
         *oldest = page->next;
         x_xfree( page );
         --pages->count;
      }
   }
}

static X_DBF_STATUS s_state( X_DBF_ORDER *index, X_NTX_PAGES **result )
{
   X_NTX_PAGES *pages = ( X_NTX_PAGES * )index->ntxPages;
   if( pages == NULL )
   {
      int64_t size = x_fileSize( &index->bag->file, 0 );
      if( size < NTX_PAGE_SIZE * 2 || size % NTX_PAGE_SIZE != 0 )
         return X_DBF_CORRUPT_FILE;
      pages = ( X_NTX_PAGES * )x_xgrab( sizeof( *pages ) );
      memset( pages, 0, sizeof( *pages ) );
      pages->end = ( uint64_t )size / NTX_PAGE_SIZE;
      index->ntxPages = pages;
   }
   if( index->bag->compound && pages->end < index->bag->ntxNextPage )
      pages->end = index->bag->ntxNextPage;
   *result = pages;
   return X_DBF_OK;
}

static X_DBF_STATUS s_cachedPage( X_DBF_ORDER *index, uint32_t number, X_NTX_PAGE **result )
{
   X_NTX_PAGES *pages;
   X_NTX_PAGE  *page;
   X_DBF_STATUS status = s_state( index, &pages );
   if( status != X_DBF_OK )
      return status;
   if( number == 0 || number >= pages->end )
      return X_DBF_CORRUPT_FILE;
   for( page = pages->pages; page != NULL; page = page->next )
      if( page->number == number )
      {
         page->used = ++pages->clock;
         *result = page;
         return X_DBF_OK;
      }
   if( index->bag->lockMode != 2 )
      s_trim( pages, 31 );
   page = ( X_NTX_PAGE * )x_xgrab( sizeof( *page ) );
   status = s_read( index->bag, page->bytes, ( int64_t )number * NTX_PAGE_SIZE );
   if( status != X_DBF_OK )
   {
      x_xfree( page );
      return status;
   }
   page->number = number;
   page->dirty = 0;
   page->next = pages->pages;
   page->used = ++pages->clock;
   pages->pages = page;
   ++pages->count;
   *result = page;
   return X_DBF_OK;
}

static void s_nodeDrop( X_NTX_NODE **node )
{
   x_xfree( *node );
}

static X_DBF_STATUS s_unpack( const X_DBF_ORDER *index, const X_NTX_PAGE *page, X_NTX_NODE *node )
{
   const X_NTX_PAGES *pages = ( const X_NTX_PAGES * )index->ntxPages;
   unsigned int       slot, prior, base = ( index->maximum + 2 ) * 2;
   unsigned int       offsets[NTX_MAX_KEYS + 1];
   node->count = s_get16( page->bytes );
   if( node->count > index->maximum )
      return X_DBF_CORRUPT_FILE;
   for( slot = 0; slot <= node->count; ++slot )
   {
      unsigned int offset = s_get16( page->bytes + 2 + slot * 2 );
      uint32_t     child;
      if( offset < base || offset > NTX_PAGE_SIZE - index->length - 8 ||
          ( offset - base ) % ( index->length + 8 ) != 0 )
         return X_DBF_CORRUPT_FILE;
      for( prior = 0; prior < slot; ++prior )
         if( offsets[prior] == offset )
            return X_DBF_CORRUPT_FILE;
      offsets[slot] = offset;
      child = s_get32( page->bytes + offset );
      if( !index->bag->largeFile && child % NTX_PAGE_SIZE != 0 )
         return X_DBF_CORRUPT_FILE;
      node->children[slot] = child / IIF( index->bag->largeFile, 1, NTX_PAGE_SIZE );
      if( node->children[slot] >= pages->end ||
          ( slot != 0 && ( node->children[slot] == 0 ) != ( node->children[0] == 0 ) ) )
         return X_DBF_CORRUPT_FILE;
      if( slot < node->count )
      {
         node->keys[slot].record = s_get32( page->bytes + offset + 4 );
         memcpy( node->keys[slot].value, page->bytes + offset + 8, index->length );
         if( node->keys[slot].record == 0 ||
             ( slot != 0 && x_dbfOrderCompare( index, &node->keys[slot - 1], &node->keys[slot], index->length,
                                               index->sortRecords ) >= IIF( index->sortRecords, 0, 1 ) ) )
            return X_DBF_CORRUPT_FILE;
      }
   }
   return X_DBF_OK;
}

static void s_pack( const X_DBF_ORDER *index, X_NTX_PAGE *page, const X_NTX_NODE *node )
{
   unsigned int slot;
   memset( page->bytes, 0, sizeof( page->bytes ) );
   s_put16( page->bytes, ( uint16_t )node->count );
   for( slot = 0; slot <= index->maximum; ++slot )
   {
      uint16_t offset = ( uint16_t )( ( index->maximum + 2 ) * 2 + slot * ( index->length + 8 ) );
      s_put16( page->bytes + 2 + slot * 2, offset );
      if( slot <= node->count )
      {
         s_put32( page->bytes + offset, node->children[slot] * IIF( index->bag->largeFile, 1, NTX_PAGE_SIZE ) );
         if( slot < node->count )
         {
            s_put32( page->bytes + offset + 4, node->keys[slot].record );
            memcpy( page->bytes + offset + 8, node->keys[slot].value, index->length );
         }
      }
   }
   page->dirty = 1;
}

static X_DBF_STATUS s_allocate( X_DBF_ORDER *index, X_NTX_PAGE **result )
{
   X_NTX_PAGES *pages;
   X_NTX_PAGE  *page;
   uint32_t    *freePage = IIF( index->bag->compound, &index->bag->ntxFreePage, &index->freePage );
   X_DBF_STATUS status = s_state( index, &pages );
   if( status != X_DBF_OK )
      return status;
   if( *freePage != 0 )
   {
      uint32_t     pointer, next;
      unsigned int offset;
      status = s_cachedPage( index, *freePage, &page );
      if( status != X_DBF_OK )
         return status;
      if( index->bag->compound && !page->dirty )
      {
         status = s_read( index->bag, page->bytes, ( int64_t )page->number * NTX_PAGE_SIZE );
         if( status != X_DBF_OK )
            return status;
      }
      offset = s_get16( page->bytes + 2 );
      if( offset < 2 || offset > NTX_PAGE_SIZE - 4 )
         return X_DBF_CORRUPT_FILE;
      pointer = s_get32( page->bytes + offset );
      if( !index->bag->largeFile && pointer % NTX_PAGE_SIZE != 0 )
         return X_DBF_CORRUPT_FILE;
      next = pointer / IIF( index->bag->largeFile, 1, NTX_PAGE_SIZE );
      if( next >= pages->end || next == *freePage )
         return X_DBF_CORRUPT_FILE;
      *freePage = next;
   }
   else
   {
      if( pages->end >= UINT32_MAX / NTX_PAGE_SIZE )
         return X_DBF_TEXT_OVERFLOW;
      page = ( X_NTX_PAGE * )x_xgrab( sizeof( *page ) );
      if( index->bag->compound )
      {
         page->number = index->bag->ntxNextPage++;
         pages->end = index->bag->ntxNextPage;
      }
      else
         page->number = ( uint32_t )pages->end++;
      page->next = pages->pages;
      pages->pages = page;
      ++pages->count;
   }
   memset( page->bytes, 0, sizeof( page->bytes ) );
   page->dirty = 1;
   page->used = ++pages->clock;
   *result = page;
   return X_DBF_OK;
}

static void s_release( X_DBF_ORDER *index, X_NTX_PAGE *page )
{
   unsigned int slot;
   uint32_t    *freePage = IIF( index->bag->compound, &index->bag->ntxFreePage, &index->freePage );
   memset( page->bytes, 0, sizeof( page->bytes ) );
   for( slot = 0; slot <= index->maximum; ++slot )
      s_put16( page->bytes + 2 + slot * 2, ( uint16_t )( ( index->maximum + 2 ) * 2 + slot * ( index->length + 8 ) ) );
   s_put32( page->bytes + ( index->maximum + 2 ) * 2, *freePage * IIF( index->bag->largeFile, 1, NTX_PAGE_SIZE ) );
   *freePage = page->number;
   page->dirty = 1;
}

X_DBF_STATUS x_dbfNtxRelease( X_DBF_ORDER *index, uint32_t number, unsigned int depth )
{
   X_NTX_NODE  *node __attribute__( ( cleanup( s_nodeDrop ) ) ) = NULL;
   X_NTX_PAGE  *page;
   X_DBF_STATUS status;
   unsigned int slot;
   if( number == 0 )
      return X_DBF_OK;
   if( depth >= 64 )
      return X_DBF_CORRUPT_FILE;
   status = s_cachedPage( index, number, &page );
   if( status != X_DBF_OK )
      return status;
   if( number == index->ntxHeader )
   {
      if( depth != 0 )
         return X_DBF_CORRUPT_FILE;
      s_release( index, page );
      return s_write( index, page->bytes, number );
   }
   node = ( X_NTX_NODE * )x_xgrab( sizeof( *node ) );
   status = s_unpack( index, page, node );
   if( status != X_DBF_OK )
      return status;
   for( slot = 0; slot <= node->count; ++slot )
   {
      status = x_dbfNtxRelease( index, node->children[slot], depth + 1 );
      if( status != X_DBF_OK )
         return status;
   }
   s_release( index, page );
   return s_write( index, page->bytes, number );
}

X_DBF_STATUS x_dbfNtxDirectory( X_DBF_INDEX_BAG *bag, int force )
{
   unsigned char bytes[NTX_PAGE_SIZE];
   uint32_t      factor;
   uint16_t      flags, tags;
   uint64_t      pages;
   int64_t       size;
   X_DBF_ORDER  *index;
   X_DBF_STATUS  status;
   unsigned int  tag;
   if( !force && bag->compound )
   {
      size_t done = 0;
      while( done < 4 )
      {
         int64_t read = x_fileReadAt( &bag->file, bytes + done, 4 - done, 4 + ( int64_t )done );
         if( read <= 0 )
            return X_DBF_READ_ERROR;
         done += ( size_t )read;
      }
      if( s_get32( bytes ) == bag->version )
         return X_DBF_OK;
   }
   status = s_read( bag, bytes, 0 );
   if( status != X_DBF_OK )
      return status;
   flags = s_get16( bytes );
   if( !( flags & 0x8000 ) )
   {
      if( bag->compound || bag->references != 1 )
         return X_DBF_UNSUPPORTED_FORMAT;
      return X_DBF_OK;
   }
   if( ( flags & ~0x8200 ) != 0 )
      return X_DBF_UNSUPPORTED_FORMAT;
   if( ( ( flags & 0x0200 ) != 0 ) != ( bag->area->lockScheme == DB_DBFLOCK_HB64 ) )
      return X_DBF_INVALID_STATE;
   bag->compound = 1;
   bag->largeFile = ( flags & 0x0200 ) != 0;
   factor = IIF( bag->largeFile, 1, NTX_PAGE_SIZE );
   size = x_fileSize( &bag->file, 0 );
   tags = s_get16( bytes + 2 );
   if( tags == 0 || tags > 63 || size < NTX_PAGE_SIZE * 3 || size % NTX_PAGE_SIZE != 0 ||
       s_get32( bytes + 8 ) % factor != 0 || s_get32( bytes + 12 ) % factor != 0 )
      return X_DBF_CORRUPT_FILE;
   pages = ( uint64_t )size / NTX_PAGE_SIZE;
   bag->ntxFreePage = s_get32( bytes + 8 ) / factor;
   bag->ntxNextPage = s_get32( bytes + 12 ) / factor;
   if( bag->ntxFreePage >= pages || bag->ntxNextPage != pages )
      return X_DBF_CORRUPT_FILE;
   bag->unbound = 0;
   for( tag = 0; tag < tags; ++tag )
   {
      const unsigned char *entry = bytes + 16 + tag * 16;
      uint32_t             header = s_get32( entry + 12 );
      int                  known = 0;
      if( entry[0] <= ' ' || memchr( entry, 0, 12 ) == NULL || header == 0 || header % factor != 0 ||
          header / factor >= pages )
         return X_DBF_CORRUPT_FILE;
      for( index = bag->area->indexes; index != NULL; index = index->next )
      {
         if( index->bag == bag && strcmp( index->diskName, ( const char * )entry ) == 0 )
         {
            if( index->ntxHeader != 0 && index->ntxHeader != header / factor )
               index->ntxVersion = UINT32_MAX;
            index->ntxHeader = header / factor;
            known = 1;
         }
      }
      if( !known )
         ++bag->unbound;
   }
   for( index = bag->area->indexes; index != NULL; index = index->next )
   {
      if( index->bag == bag )
      {
         for( tag = 0; tag < tags; ++tag )
            if( strcmp( index->diskName, ( const char * )bytes + 16 + tag * 16 ) == 0 )
               break;
         if( tag == tags )
            return X_DBF_ORDER_NOT_FOUND;
      }
   }
   bag->version = s_get32( bytes + 4 );
   return X_DBF_OK;
}

X_DBF_STATUS x_dbfNtxLoad( X_DBF_ORDER *index, int force )
{
   unsigned char bytes[NTX_PAGE_SIZE];
   int64_t       size;
   uint16_t      flags;
   uint32_t      version;
   uint32_t      root, freePage;
   X_DBF_STATUS  status;
   if( !force && index->bag->compound && index->bag->version == index->ntxVersion )
      return X_DBF_OK;
   status = s_read( index->bag, bytes, ( int64_t )index->ntxHeader * NTX_PAGE_SIZE );
   if( status != X_DBF_OK )
      return status;
   version = IIF( index->bag->compound, index->bag->version, s_get16( bytes + 2 ) );
   if( !force && version == index->ntxVersion )
      return X_DBF_OK;
   flags = s_get16( bytes );
   if( ( flags & ~0x03FF ) != 0 || ( flags & 0x0006 ) != 0x0006 || bytes[279] != 0 || ( flags & 0x0040 ) != 0 )
      return X_DBF_UNSUPPORTED_FORMAT;
   if( ( ( flags & 0x0200 ) != 0 ) != ( index->area->lockScheme == DB_DBFLOCK_HB64 ) ||
       ( ( flags & 0x0010 ) != 0 ) != ( index->area->lockScheme == DB_DBFLOCK_CLIPPER2 ) )
      return X_DBF_INVALID_STATE;
   if( index->opening )
   {
      unsigned int length = s_get16( bytes + 14 ), decimals = s_get16( bytes + 16 );
      if( length == 0 || length > X_DBF_KEY_SIZE || decimals >= length || ( index->type != 'N' && decimals != 0 ) ||
          ( ( index->type == 'D' || index->type == 'L' ) && length != index->length ) || bytes[278] > 1 ||
          s_get16( bytes + 280 ) > 1 )
         return X_DBF_INVALID_INDEX_KEY;
      if( bytes[538] >= 0x20 )
      {
         if( memchr( bytes + 538, 0, 10 ) == NULL && bytes[548] != 0 )
            return X_DBF_CORRUPT_FILE;
         if( strncmp( ( const char * )bytes + 538, index->diskName, 10 ) != 0 )
            return X_DBF_ORDER_NOT_FOUND;
      }
      index->length = ( uint16_t )length;
      index->decimals = ( uint16_t )decimals;
      index->stride = ( sizeof( uint32_t ) + length + 3 ) & ~( size_t )3;
      index->maximum = ( uint16_t )( ( NTX_PAGE_SIZE - 2 ) / ( length + 10 ) - 1 );
      index->maximum &= ( uint16_t )~1;
      index->unique = bytes[278];
      index->custom = ( flags & 0x0020 ) != 0;
      index->partial = ( flags & 0x0008 ) != 0;
      index->template = ( flags & 0x0080 ) != 0;
      index->multiKey = 0;
      index->descending = s_get16( bytes + 280 ) != 0;
   }
   if( s_get16( bytes + 14 ) != index->length || s_get16( bytes + 16 ) != index->decimals ||
       s_get16( bytes + 18 ) != index->maximum || bytes[278] != index->unique ||
       ( s_get16( bytes + 280 ) != 0 ) != index->descending ||
       ( ( flags & 1 ) != 0 ) != ( index->forBlock.nType == X_IT_BLOCK ) || memchr( bytes + 22, 0, 256 ) == NULL ||
       memchr( bytes + 282, 0, 256 ) == NULL )
      return X_DBF_INVALID_INDEX_KEY;
   if( strcmp( ( const char * )bytes + 22, index->expression ) != 0 ||
       strcmp( ( const char * )bytes + 282, index->condition ) != 0 )
      return X_DBF_INDEX_EXPRESSION_REQUIRED;
   size = x_fileSize( &index->bag->file, 0 );
   root = s_get32( bytes + 4 );
   freePage = s_get32( bytes + 8 );
   if( size < NTX_PAGE_SIZE * 2 || size % NTX_PAGE_SIZE != 0 ||
       ( ( flags & 0x0200 ) == 0 && ( root % NTX_PAGE_SIZE != 0 || freePage % NTX_PAGE_SIZE != 0 ) ) )
      return X_DBF_CORRUPT_FILE;
   root /= IIF( flags & 0x0200, 1, NTX_PAGE_SIZE );
   freePage /= IIF( flags & 0x0200, 1, NTX_PAGE_SIZE );
   if( root == 0 || root == index->ntxHeader || root >= ( uint64_t )size / NTX_PAGE_SIZE ||
       freePage >= ( uint64_t )size / NTX_PAGE_SIZE )
      return X_DBF_CORRUPT_FILE;
   index->custom = ( flags & 0x0020 ) != 0;
   index->partial = ( flags & 0x0008 ) != 0;
   index->template = ( flags & 0x0080 ) != 0;
   x_dbfNtxDiscard( index );
   x_dbfOrderPagesClear( index );
   index->sortRecords = ( flags & 0x0100 ) != 0;
   index->ntxRoot = root;
   index->freePage = freePage;
   index->bag->largeFile = ( flags & 0x0200 ) != 0;
   index->bag->version = index->ntxVersion = version;
   index->cursor.valid = 0;
   index->broken = index->changed = 0;
   return X_DBF_OK;
}

X_DBF_STATUS x_dbfNtxFind( X_DBF_ORDER *index, const X_DBF_INDEX_KEY *key, size_t length, int records, int strict,
                           int backwards, X_DBF_INDEX_CURSOR *cursor )
{
   X_NTX_NODE        *node __attribute__( ( cleanup( s_nodeDrop ) ) ) = NULL;
   X_DBF_INDEX_CURSOR route = { 0 };
   uint32_t           number = index->ntxRoot;
   X_NTX_PAGE        *page;
   X_DBF_STATUS       status;
   X_DBF_INDEX_KEY    sought;
   if( key != NULL )
   {
      sought = *key;
      key = &sought;
   }
   if( key != NULL && records && !index->sortRecords )
   {
      status = x_dbfNtxFind( index, key, length, 0, 0, 0, cursor );
      if( status != X_DBF_OK )
         return status;
      while( cursor->valid && x_dbfOrderCompare( index, &cursor->key, key, length, 0 ) == 0 )
      {
         if( cursor->key.record == key->record )
            return IIF( strict, x_dbfNtxStep( index, cursor, backwards ), X_DBF_OK );
         status = x_dbfNtxStep( index, cursor, 0 );
         if( status != X_DBF_OK )
            return status;
      }
      if( strict )
         return x_dbfNtxFind( index, key, length, 0, 1, backwards, cursor );
      cursor->valid = 0;
      return X_DBF_OK;
   }
   node = ( X_NTX_NODE * )x_xgrab( sizeof( *node ) );
   cursor->valid = cursor->keyValid = 0;
   while( number != 0 )
   {
      unsigned int slot, candidate, ancestor;
      if( route.depth == 64 )
         return X_DBF_CORRUPT_FILE;
      for( ancestor = 0; ancestor < route.depth; ++ancestor )
         if( route.path[ancestor] == number )
            return X_DBF_CORRUPT_FILE;
      route.path[route.depth++] = number;
      status = s_cachedPage( index, number, &page );
      if( status == X_DBF_OK )
         status = s_unpack( index, page, node );
      if( status != X_DBF_OK )
         return status;
      if( key == NULL )
         slot = IIF( backwards, node->count, 0 );
      else
      {
         for( slot = 0; slot < node->count; ++slot )
         {
            int order = x_dbfOrderCompare( index, &node->keys[slot], key, length, records );
            if( order > 0 || ( order == 0 && IIF( backwards, strict, !strict ) ) )
               break;
         }
      }
      candidate = IIF( backwards, IIF( slot != 0, slot - 1, UINT32_MAX ), slot );
      if( candidate < node->count )
      {
         route.key = node->keys[candidate];
         route.page = number;
         route.slot = candidate;
         route.slots[route.depth - 1] = candidate;
         route.version = index->bag->version;
         route.valid = route.keyValid = 1;
         *cursor = route;
      }
      route.slots[route.depth - 1] = slot;
      number = node->children[slot];
   }
   return X_DBF_OK;
}

X_DBF_STATUS x_dbfNtxStep( X_DBF_ORDER *index, X_DBF_INDEX_CURSOR *cursor, int backwards )
{
   X_NTX_NODE  *node __attribute__( ( cleanup( s_nodeDrop ) ) ) = NULL;
   X_NTX_PAGE  *page;
   uint32_t     number;
   unsigned int slot;
   X_DBF_STATUS status;
   if( !cursor->keyValid )
      return X_DBF_OK;
   if( !cursor->valid || cursor->version != index->bag->version )
      return x_dbfNtxFind( index, &cursor->key, index->length, 1, 1, backwards, cursor );
   node = ( X_NTX_NODE * )x_xgrab( sizeof( *node ) );
   number = cursor->page;
   status = s_cachedPage( index, number, &page );
   if( status == X_DBF_OK )
      status = s_unpack( index, page, node );
   if( status != X_DBF_OK )
      return status;
   if( cursor->slot >= node->count ||
       x_dbfOrderCompare( index, &node->keys[cursor->slot], &cursor->key, index->length, 1 ) != 0 )
      return X_DBF_CORRUPT_FILE;
   slot = cursor->slot + IIF( backwards, 0, 1 );
   if( node->children[slot] != 0 )
   {
      cursor->slots[cursor->depth - 1] = slot;
      number = node->children[slot];
      for( ;; )
      {
         unsigned int ancestor;
         if( cursor->depth == 64 )
            return X_DBF_CORRUPT_FILE;
         for( ancestor = 0; ancestor < cursor->depth; ++ancestor )
            if( cursor->path[ancestor] == number )
               return X_DBF_CORRUPT_FILE;
         cursor->path[cursor->depth++] = number;
         status = s_cachedPage( index, number, &page );
         if( status == X_DBF_OK )
            status = s_unpack( index, page, node );
         if( status != X_DBF_OK )
            return status;
         slot = IIF( backwards, node->count, 0 );
         if( node->children[slot] == 0 )
         {
            if( node->count == 0 )
               return X_DBF_CORRUPT_FILE;
            slot = IIF( backwards, node->count - 1, 0 );
            break;
         }
         cursor->slots[cursor->depth - 1] = slot;
         number = node->children[slot];
      }
   }
   else if( IIF( backwards, cursor->slot != 0, cursor->slot + 1 < node->count ) )
      slot = IIF( backwards, cursor->slot - 1, cursor->slot + 1 );
   else
   {
      int found = 0;
      while( cursor->depth > 1 )
      {
         --cursor->depth;
         number = cursor->path[cursor->depth - 1];
         status = s_cachedPage( index, number, &page );
         if( status == X_DBF_OK )
            status = s_unpack( index, page, node );
         if( status != X_DBF_OK )
            return status;
         slot = cursor->slots[cursor->depth - 1];
         if( slot > node->count )
            return X_DBF_CORRUPT_FILE;
         if( IIF( backwards, slot != 0, slot < node->count ) )
         {
            slot -= IIF( backwards, 1, 0 );
            found = 1;
            break;
         }
      }
      if( !found )
      {
         cursor->valid = 0;
         return X_DBF_OK;
      }
   }
   if( IIF( backwards,
            x_dbfOrderCompare( index, &node->keys[slot], &cursor->key, index->length, index->sortRecords ) >=
               IIF( index->sortRecords, 0, 1 ),
            x_dbfOrderCompare( index, &node->keys[slot], &cursor->key, index->length, index->sortRecords ) <=
               IIF( index->sortRecords, 0, -1 ) ) )
      return X_DBF_CORRUPT_FILE;
   cursor->key = node->keys[slot];
   cursor->page = number;
   cursor->slot = cursor->slots[cursor->depth - 1] = slot;
   return X_DBF_OK;
}

static X_DBF_STATUS s_insert( X_DBF_ORDER *index, uint32_t number, X_DBF_INDEX_KEY *entry, uint32_t *right,
                              uint32_t *path, unsigned int depth )
{
   X_NTX_NODE  *node __attribute__( ( cleanup( s_nodeDrop ) ) ) = ( X_NTX_NODE * )x_xgrab( sizeof( *node ) );
   X_NTX_NODE  *peer __attribute__( ( cleanup( s_nodeDrop ) ) ) = NULL;
   X_NTX_PAGE  *page, *newPage;
   unsigned int slot, ancestor;
   uint32_t     child = 0;
   X_DBF_STATUS status;
   *right = 0;
   if( depth == 64 )
      return X_DBF_CORRUPT_FILE;
   for( ancestor = 0; ancestor < depth; ++ancestor )
      if( path[ancestor] == number )
         return X_DBF_CORRUPT_FILE;
   path[depth] = number;
   status = s_cachedPage( index, number, &page );
   if( status == X_DBF_OK )
      status = s_unpack( index, page, node );
   if( status != X_DBF_OK )
      return status;
   for( slot = 0; slot < node->count && x_dbfOrderCompare( index, &node->keys[slot], entry, index->length,
                                                           index->sortRecords ) < IIF( index->sortRecords, 0, 1 );
        ++slot )
   {
   }
   if( node->children[slot] != 0 )
   {
      status = s_insert( index, node->children[slot], entry, &child, path, depth + 1 );
      if( status != X_DBF_OK || child == 0 )
         return status;
   }
   memmove( node->keys + slot + 1, node->keys + slot, ( node->count - slot ) * sizeof( *node->keys ) );
   memmove( node->children + slot + 2, node->children + slot + 1, ( node->count - slot ) * sizeof( *node->children ) );
   node->keys[slot] = *entry;
   node->children[slot + 1] = child;
   ++node->count;
   if( node->count > index->maximum )
   {
      unsigned int middle = node->count / 2;
      status = s_allocate( index, &newPage );
      if( status != X_DBF_OK )
         return status;
      peer = ( X_NTX_NODE * )x_xgrab( sizeof( *peer ) );
      peer->count = node->count - middle - 1;
      memcpy( peer->keys, node->keys + middle + 1, peer->count * sizeof( *peer->keys ) );
      memcpy( peer->children, node->children + middle + 1, ( peer->count + 1 ) * sizeof( *peer->children ) );
      *entry = node->keys[middle];
      node->count = middle;
      s_pack( index, newPage, peer );
      *right = newPage->number;
   }
   s_pack( index, page, node );
   return X_DBF_OK;
}

static X_DBF_STATUS s_remove( X_DBF_ORDER *index, uint32_t number, const X_DBF_INDEX_CURSOR *target,
                              unsigned int depth )
{
   X_NTX_NODE  *node __attribute__( ( cleanup( s_nodeDrop ) ) ) = ( X_NTX_NODE * )x_xgrab( sizeof( *node ) );
   X_NTX_NODE  *child __attribute__( ( cleanup( s_nodeDrop ) ) ) = NULL;
   X_NTX_NODE  *left __attribute__( ( cleanup( s_nodeDrop ) ) ) = NULL;
   X_NTX_NODE  *right __attribute__( ( cleanup( s_nodeDrop ) ) ) = NULL;
   X_NTX_PAGE  *page, *childPage, *leftPage = NULL, *rightPage = NULL;
   X_DBF_STATUS status;
   unsigned int slot = target->slots[depth], minimum = index->maximum / 2;
   int          replaced = 0;
   status = s_cachedPage( index, number, &page );
   if( status == X_DBF_OK )
      status = s_unpack( index, page, node );
   if( status != X_DBF_OK )
      return status;
   if( depth + 1 == target->depth )
   {
      if( node->children[slot] == 0 )
      {
         memmove( node->keys + slot, node->keys + slot + 1, ( node->count - slot - 1 ) * sizeof( *node->keys ) );
         --node->count;
         s_pack( index, page, node );
         return X_DBF_OK;
      }
      else
      {
         X_DBF_INDEX_CURSOR previous = *target;
         status = x_dbfNtxStep( index, &previous, 1 );
         if( status != X_DBF_OK )
            return status;
         if( !previous.valid )
            return X_DBF_CORRUPT_FILE;
         node->keys[slot] = previous.key;
         status = s_remove( index, node->children[slot], &previous, depth + 1 );
         replaced = 1;
      }
   }
   else if( node->children[slot] != 0 )
      status = s_remove( index, node->children[slot], target, depth + 1 );
   else
      return X_DBF_OK;
   if( status != X_DBF_OK )
      return status;
   child = ( X_NTX_NODE * )x_xgrab( sizeof( *child ) );
   status = s_cachedPage( index, node->children[slot], &childPage );
   if( status == X_DBF_OK )
      status = s_unpack( index, childPage, child );
   if( status != X_DBF_OK )
      return status;
   if( child->count >= minimum )
   {
      if( replaced )
         s_pack( index, page, node );
      return X_DBF_OK;
   }
   if( slot != 0 )
   {
      left = ( X_NTX_NODE * )x_xgrab( sizeof( *left ) );
      status = s_cachedPage( index, node->children[slot - 1], &leftPage );
      if( status == X_DBF_OK )
         status = s_unpack( index, leftPage, left );
      if( status != X_DBF_OK )
         return status;
   }
   if( slot < node->count )
   {
      right = ( X_NTX_NODE * )x_xgrab( sizeof( *right ) );
      status = s_cachedPage( index, node->children[slot + 1], &rightPage );
      if( status == X_DBF_OK )
         status = s_unpack( index, rightPage, right );
      if( status != X_DBF_OK )
         return status;
   }
   if( left != NULL && left->count > minimum )
   {
      memmove( child->keys + 1, child->keys, child->count * sizeof( *child->keys ) );
      memmove( child->children + 1, child->children, ( child->count + 1 ) * sizeof( *child->children ) );
      child->keys[0] = node->keys[slot - 1];
      child->children[0] = left->children[left->count];
      node->keys[slot - 1] = left->keys[left->count - 1];
      --left->count;
      ++child->count;
      s_pack( index, leftPage, left );
      s_pack( index, childPage, child );
   }
   else if( right != NULL && right->count > minimum )
   {
      child->keys[child->count] = node->keys[slot];
      child->children[child->count + 1] = right->children[0];
      node->keys[slot] = right->keys[0];
      memmove( right->keys, right->keys + 1, ( right->count - 1 ) * sizeof( *right->keys ) );
      memmove( right->children, right->children + 1, right->count * sizeof( *right->children ) );
      --right->count;
      ++child->count;
      s_pack( index, rightPage, right );
      s_pack( index, childPage, child );
   }
   else
   {
      unsigned int separator = IIF( left != NULL, slot - 1, slot );
      X_NTX_NODE  *first = IIF( left != NULL, left, child );
      X_NTX_NODE  *last = IIF( left != NULL, child, right );
      X_NTX_PAGE  *firstPage = IIF( left != NULL, leftPage, childPage );
      X_NTX_PAGE  *lastPage = IIF( left != NULL, childPage, rightPage );
      if( last == NULL || first->count + last->count + 1 > index->maximum )
         return X_DBF_CORRUPT_FILE;
      first->keys[first->count] = node->keys[separator];
      memcpy( first->keys + first->count + 1, last->keys, last->count * sizeof( *first->keys ) );
      memcpy( first->children + first->count + 1, last->children, ( last->count + 1 ) * sizeof( *first->children ) );
      first->count += last->count + 1;
      memmove( node->keys + separator, node->keys + separator + 1,
               ( node->count - separator - 1 ) * sizeof( *node->keys ) );
      memmove( node->children + separator + 1, node->children + separator + 2,
               ( node->count - separator - 1 ) * sizeof( *node->children ) );
      --node->count;
      s_pack( index, firstPage, first );
      s_release( index, lastPage );
   }
   s_pack( index, page, node );
   return X_DBF_OK;
}

X_DBF_STATUS x_dbfNtxChange( X_DBF_ORDER *index, const X_DBF_INDEX_KEY *removed, const X_DBF_INDEX_KEY *added )
{
   X_DBF_INDEX_CURSOR cursor = { 0 };
   X_NTX_NODE        *node __attribute__( ( cleanup( s_nodeDrop ) ) ) = NULL;
   X_NTX_PAGE        *page;
   uint32_t           path[64], right = 0;
   int                changed = 0;
   X_DBF_STATUS       status = X_DBF_OK;
   if( removed != NULL && added != NULL && x_dbfOrderCompare( index, removed, added, index->length, 1 ) == 0 )
      return X_DBF_OK;
   if( removed != NULL )
   {
      status = x_dbfNtxFind( index, removed, index->length, 1, 0, 0, &cursor );
      if( status == X_DBF_OK && cursor.valid &&
          x_dbfOrderCompare( index, &cursor.key, removed, index->length, 1 ) == 0 )
      {
         status = s_remove( index, index->ntxRoot, &cursor, 0 );
         if( status != X_DBF_OK )
            return status;
         node = ( X_NTX_NODE * )x_xgrab( sizeof( *node ) );
         status = s_cachedPage( index, index->ntxRoot, &page );
         if( status == X_DBF_OK )
            status = s_unpack( index, page, node );
         if( status != X_DBF_OK )
            return status;
         if( node->count == 0 && node->children[0] != 0 )
         {
            index->ntxRoot = node->children[0];
            s_release( index, page );
         }
         changed = 1;
      }
   }
   if( status == X_DBF_OK && added != NULL )
   {
      status = x_dbfNtxFind( index, added, index->length, !index->unique, 0, 0, &cursor );
      if( status == X_DBF_OK &&
          ( !cursor.valid || x_dbfOrderCompare( index, &cursor.key, added, index->length, !index->unique ) != 0 ) )
      {
         X_DBF_INDEX_KEY entry = *added;
         status = s_insert( index, index->ntxRoot, &entry, &right, path, 0 );
         if( status != X_DBF_OK )
            return status;
         if( right != 0 )
         {
            if( node == NULL )
               node = ( X_NTX_NODE * )x_xgrab( sizeof( *node ) );
            status = s_allocate( index, &page );
            if( status != X_DBF_OK )
               return status;
            node->count = 1;
            node->keys[0] = entry;
            node->children[0] = index->ntxRoot;
            node->children[1] = right;
            s_pack( index, page, node );
            index->ntxRoot = page->number;
         }
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

X_DBF_STATUS x_dbfNtxSync( X_DBF_ORDER *index )
{
   unsigned char bytes[NTX_PAGE_SIZE];
   size_t        number;
   int           building = index->root != NULL;
   X_DBF_STATUS  status = X_DBF_OK;
   if( index->broken )
      return X_DBF_RECOVERY_REQUIRED;
   if( !index->changed )
      return X_DBF_OK;
   if( building )
   {
      if( index->bag->compound )
      {
         if( index->bag->ntxNextPage == 0 )
         {
            int64_t size = x_fileSize( &index->bag->file, 0 );
            if( size < 0 || size % NTX_PAGE_SIZE != 0 )
               return X_DBF_READ_ERROR;
            index->bag->ntxNextPage = ( uint32_t )IIF( size == 0, 1, size / NTX_PAGE_SIZE );
         }
         {
            X_DBF_ORDER *old;
            for( old = index->area->indexes; old != NULL; old = old->next )
            {
               if( old != index && old->bag == index->bag && old->ntxHeader == 0 && old->ntxRoot != 0 )
               {
                  status = s_read( index->bag, bytes, 0 );
                  if( status != X_DBF_OK )
                     return status;
                  index->bag->ntxFreePage = s_get32( bytes + 8 ) / IIF( index->bag->largeFile, 1, NTX_PAGE_SIZE );
                  old->ntxHeader = index->bag->ntxNextPage++;
                  status = s_write( old, bytes, old->ntxHeader );
                  if( status != X_DBF_OK )
                     return status;
               }
            }
         }
         if( index->ntxHeader == 0 )
            index->ntxHeader = index->bag->ntxNextPage++;
         if( index->ntxRoot != 0 )
         {
            status = x_dbfNtxRelease( index, index->ntxRoot, 0 );
            if( status != X_DBF_OK )
               return status;
         }
         for( number = 0; number < index->pageCount; ++number )
         {
            if( index->bag->ntxFreePage != 0 )
            {
               unsigned char freeBytes[NTX_PAGE_SIZE];
               uint32_t      page = index->bag->ntxFreePage, pointer, next;
               unsigned int  offset;
               status = s_read( index->bag, freeBytes, ( int64_t )page * NTX_PAGE_SIZE );
               if( status != X_DBF_OK )
                  return status;
               offset = s_get16( freeBytes + 2 );
               if( offset < 2 || offset > NTX_PAGE_SIZE - 4 )
                  return X_DBF_CORRUPT_FILE;
               pointer = s_get32( freeBytes + offset );
               if( pointer % IIF( index->bag->largeFile, 1, NTX_PAGE_SIZE ) != 0 )
                  return X_DBF_CORRUPT_FILE;
               next = pointer / IIF( index->bag->largeFile, 1, NTX_PAGE_SIZE );
               if( next >= index->bag->ntxNextPage || next == page )
                  return X_DBF_CORRUPT_FILE;
               index->bag->ntxFreePage = next;
               index->pages[number]->number = page;
            }
            else
            {
               if( index->bag->ntxNextPage >= UINT32_MAX / NTX_PAGE_SIZE )
                  return X_DBF_TEXT_OVERFLOW;
               index->pages[number]->number = index->bag->ntxNextPage++;
            }
         }
      }
      for( number = 0; number < index->pageCount && status == X_DBF_OK; ++number )
      {
         X_DBF_ORDER_PAGE *page = index->pages[number];
         int               slot;
         memset( bytes, 0, sizeof( bytes ) );
         s_put16( bytes, ( uint16_t )page->count );
         for( slot = 0; slot <= index->maximum; ++slot )
         {
            uint16_t offset = ( uint16_t )( ( index->maximum + 2 ) * 2 + slot * ( index->length + 8 ) );
            s_put16( bytes + 2 + slot * 2, offset );
            if( slot <= page->count )
            {
               s_put32( bytes + offset,
                        IIF( page->children[slot] != NULL,
                             page->children[slot]->number * IIF( index->bag->largeFile, 1, NTX_PAGE_SIZE ), 0 ) );
               if( slot < page->count )
               {
                  const X_DBF_INDEX_KEY *key =
                     ( const X_DBF_INDEX_KEY * )( page->keys + ( size_t )slot * index->stride );
                  s_put32( bytes + offset + 4, key->record );
                  memcpy( bytes + offset + 8, key->value, index->length );
               }
            }
         }
         status = s_write( index, bytes, page->number );
      }
   }
   else
   {
      X_NTX_PAGES *pages = ( X_NTX_PAGES * )index->ntxPages;
      X_NTX_PAGE  *page;
      for( page = IIF( pages != NULL, pages->pages, NULL ); page != NULL && status == X_DBF_OK; page = page->next )
         if( page->dirty )
            status = s_write( index, page->bytes, page->number );
   }
   if( status == X_DBF_OK )
   {
      memset( bytes, 0, sizeof( bytes ) );
      s_put16( bytes, ( uint16_t )( 0x0006 | IIF( index->sortRecords, 0x0100, 0 ) |
                                    IIF( index->forBlock.nType != X_IT_NIL, 1, 0 ) | IIF( index->partial, 0x0008, 0 ) |
                                    IIF( index->custom, 0x0020, 0 ) | IIF( index->template, 0x0080, 0 ) |
                                    IIF( index->area->lockScheme == DB_DBFLOCK_CLIPPER2, 0x0010, 0 ) |
                                    IIF( index->bag->largeFile, 0x0200, 0 ) ) );
      index->bag->version =
         IIF( index->bag->compound, index->bag->version + 1, ( uint16_t )( index->bag->version + 1 ) );
      s_put16( bytes + 2, ( uint16_t )index->bag->version );
      s_put32( bytes + 4,
               IIF( building, index->root->number, index->ntxRoot ) * IIF( index->bag->largeFile, 1, NTX_PAGE_SIZE ) );
      s_put32( bytes + 8, index->freePage * IIF( index->bag->largeFile, 1, NTX_PAGE_SIZE ) );
      s_put16( bytes + 12, index->length + 8 );
      s_put16( bytes + 14, index->length );
      s_put16( bytes + 16, index->decimals );
      s_put16( bytes + 18, index->maximum );
      s_put16( bytes + 20, index->maximum / 2 );
      memcpy( bytes + 22, index->expression, strlen( index->expression ) );
      bytes[278] = ( unsigned char )index->unique;
      bytes[280] = ( unsigned char )index->descending;
      memcpy( bytes + 282, index->condition, strlen( index->condition ) );
      memcpy( bytes + 538, index->diskName, strlen( index->diskName ) );
      bytes[550] = ( unsigned char )index->custom;
      status = s_write( index, bytes, index->ntxHeader );
   }
   if( status == X_DBF_OK && index->bag->compound )
   {
      X_DBF_ORDER *tag;
      unsigned int count = 0;
      uint32_t     factor = IIF( index->bag->largeFile, 1, NTX_PAGE_SIZE );
      memset( bytes, 0, sizeof( bytes ) );
      for( tag = index->area->indexes; tag != NULL; tag = tag->next )
      {
         if( tag->bag == index->bag )
         {
            if( count == 63 )
               return X_DBF_TEXT_OVERFLOW;
            if( tag->ntxHeader == 0 )
               return X_DBF_INVALID_STATE;
            memcpy( bytes + 16 + count * 16, tag->diskName, strlen( tag->diskName ) );
            s_put32( bytes + 28 + count * 16, tag->ntxHeader * factor );
            ++count;
         }
      }
      s_put16( bytes, ( uint16_t )( 0x8000 | IIF( index->bag->largeFile, 0x0200, 0 ) ) );
      s_put16( bytes + 2, ( uint16_t )count );
      s_put32( bytes + 4, index->bag->version );
      s_put32( bytes + 8, index->bag->ntxFreePage * factor );
      s_put32( bytes + 12, index->bag->ntxNextPage * factor );
      status = s_write( index, bytes, 0 );
   }
   // No forced write to disk here: x_dbfOrderCommit() does it on DbCommit() and close
   if( status == X_DBF_OK && building && !index->bag->compound &&
       !x_fileTrunc( &index->bag->file, ( int64_t )( index->pageCount + 1 ) * NTX_PAGE_SIZE ) )
      status = X_DBF_WRITE_ERROR;
   if( status == X_DBF_OK )
   {
      index->changed = index->headerChanged = 0;
      index->ntxVersion = index->bag->version;
      if( building )
      {
         index->ntxRoot = index->root->number;
         x_dbfOrderPagesClear( index );
         x_dbfNtxDiscard( index );
      }
      else
      {
         X_NTX_PAGES *pages = ( X_NTX_PAGES * )index->ntxPages;
         X_NTX_PAGE  *page;
         for( page = IIF( pages != NULL, pages->pages, NULL ); page != NULL; page = page->next )
            page->dirty = 0;
         if( pages != NULL )
            s_trim( pages, 32 );
      }
      index->cursor.valid = 0;
   }
   else
   {
      index->broken = 1;
      x_dbfNtxDiscard( index );
   }
   return status;
}
