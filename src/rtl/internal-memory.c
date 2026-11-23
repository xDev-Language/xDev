/*
 * Copyright (c) 2024-2026 xDev-Language
 *
 * Library...: xDev Runtime
 * File......: internal-memory.c
 *
 */

#include "internal-private.h"

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdatomic.h>
#include <stdlib.h>
#include <string.h>

#define X_MEMORY_HEAD_MAGIC UINT64_C( 0x584445564D454D48 )
#define X_MEMORY_TAIL_MAGIC UINT64_C( 0x584445564D454D54 )

/* 1: magic numbers around every block are checked and statistics are counted; 0: neither */
#ifndef X_MEMORY_CHECKS
#define X_MEMORY_CHECKS 1
#endif

/* The bytes after every block */
#define X_MEMORY_TAIL_SIZE IIF( X_MEMORY_CHECKS, sizeof( uint64_t ), ( size_t )0 )

/* The header before every block: a magic number, the size asked for and the reference count;
   a magic number follows the block */
typedef union
{
   max_align_t alignment;
   struct
   {
      uint64_t magic;
      size_t size;
      size_t refs;
   } block;
} X_MEMORY_HEADER;

/* The statistics of x_xstats() */
static _Atomic size_t s_currentBytes;
static _Atomic size_t s_currentBlocks;
static _Atomic size_t s_peakBytes;
static _Atomic size_t s_peakBlocks;
static _Atomic size_t s_totalAllocations;
static _Atomic size_t s_totalReallocations;

/* ---------------------------------------------------------------------------------------------
   Block helpers
   --------------------------------------------------------------------------------------------- */

/* A memory error cannot be recovered: report it and stop the program */
static void s_memoryAbort( const char *operation, const char *reason, size_t size )
{
   fprintf( stderr, "xDev memory error in %s: %s (%zu bytes)%s", operation, reason, size, x_eol() );
   abort();
}

/* The size of a block with its header and tail; 0 on overflow */
static int s_memoryTotalSize( size_t size, size_t *total )
{
   const size_t overhead = sizeof( X_MEMORY_HEADER ) + X_MEMORY_TAIL_SIZE;

   if( size > SIZE_MAX - overhead )
      return 0;

   *total = overhead + size;

   return 1;
}

/* The size of an array of count items; 0 on overflow */
static int s_memoryArraySize( size_t count, size_t itemSize, size_t *size )
{
   if( itemSize != 0 && count > SIZE_MAX / itemSize )
      return 0;

   *size = count * itemSize;

   return 1;
}

/* Write the magic number after the block */
static void s_memoryWriteTail( X_MEMORY_HEADER *header )
{
   uint64_t tail = X_MEMORY_TAIL_MAGIC;

   if( !X_MEMORY_CHECKS )
      return;

   memcpy( ( unsigned char * )( header + 1 ) + header->block.size, &tail, sizeof( tail ) );
}

/* The header of a block, after checking both magic numbers; NULL for NULL */
static X_MEMORY_HEADER *s_memoryHeader( const void *memory, const char *operation )
{
   X_MEMORY_HEADER *header;
   uint64_t tail;

   if( memory == NULL )
      return NULL;

   header = ( ( X_MEMORY_HEADER * )memory ) - 1;
   if( !X_MEMORY_CHECKS )
      return header;

   if( header->block.magic != X_MEMORY_HEAD_MAGIC )
      s_memoryAbort( operation, "invalid or already released block", 0 );

   memcpy( &tail, ( const unsigned char * )memory + header->block.size, sizeof( tail ) );
   if( tail != X_MEMORY_TAIL_MAGIC )
      s_memoryAbort( operation, "block boundary was overwritten", header->block.size );

   return header;
}

/* Raise a peak to value when value is higher */
static void s_memoryUpdatePeak( _Atomic size_t *peak, size_t value )
{
   size_t previous = atomic_load( peak );

   while( value > previous && !atomic_compare_exchange_weak( peak, &previous, value ) )
      ;
}

/* Count a new block of bytes */
static void s_memoryCountAlloc( size_t bytes )
{
   size_t currentBytes, currentBlocks;

   if( !X_MEMORY_CHECKS )
      return;

   currentBytes = atomic_fetch_add( &s_currentBytes, bytes ) + bytes;
   currentBlocks = atomic_fetch_add( &s_currentBlocks, 1 ) + 1;
   s_memoryUpdatePeak( &s_peakBytes, currentBytes );
   s_memoryUpdatePeak( &s_peakBlocks, currentBlocks );
   atomic_fetch_add( &s_totalAllocations, 1 );
}

/* Count a released block of bytes */
static void s_memoryCountFree( size_t bytes )
{
   if( !X_MEMORY_CHECKS )
      return;

   atomic_fetch_sub( &s_currentBytes, bytes );
   atomic_fetch_sub( &s_currentBlocks, 1 );
}

/* Count a block resized from oldSize to bytes */
static void s_memoryCountResize( size_t oldSize, size_t bytes )
{
   size_t growth;

   if( !X_MEMORY_CHECKS )
      return;

   if( bytes >= oldSize )
   {
      growth = bytes - oldSize;
      s_memoryUpdatePeak( &s_peakBytes, atomic_fetch_add( &s_currentBytes, growth ) + growth );
   }
   else
      atomic_fetch_sub( &s_currentBytes, oldSize - bytes );

   atomic_fetch_add( &s_totalReallocations, 1 );
}

/* ---------------------------------------------------------------------------------------------
   Allocation
   --------------------------------------------------------------------------------------------- */

/* A new block; NULL when there is no memory */
void *x_xalloc( size_t bytes )
{
   X_MEMORY_HEADER *header;
   size_t total;

   if( !s_memoryTotalSize( bytes, &total ) )
      return NULL;

   header = ( X_MEMORY_HEADER * )malloc( total );
   if( header == NULL )
      return NULL;

   header->block.magic = X_MEMORY_HEAD_MAGIC;
   header->block.size = bytes;
   header->block.refs = 1;
   s_memoryWriteTail( header );
   s_memoryCountAlloc( bytes );

   return header + 1;
}

/* A new array of count items; NULL when there is no memory */
void *x_xallocArray( size_t count, size_t itemSize )
{
   size_t size;

   if( !s_memoryArraySize( count, itemSize, &size ) )
      return NULL;

   return x_xalloc( size );
}

/* Release a block that is not shared */
void x_xfree( void *area )
{
   X_MEMORY_HEADER *header;

   if( area == NULL )
      return;

   header = s_memoryHeader( area, "x_xfree" );
   if( header->block.refs != 1 )
      s_memoryAbort( "x_xfree", "block is still shared", header->block.size );

   s_memoryCountFree( header->block.size );
   header->block.magic = 0;
   free( header );
}

/* A new block; no memory stops the program */
void *x_xgrab( size_t bytes )
{
   void *memory = x_xalloc( bytes );

   if( memory == NULL )
      s_memoryAbort( "x_xgrab", "allocation failed or size overflowed", bytes );

   return memory;
}

/* A new array of count items; no memory stops the program */
void *x_xgrabArray( size_t count, size_t itemSize )
{
   size_t size;

   if( !s_memoryArraySize( count, itemSize, &size ) )
      s_memoryAbort( "x_xgrabArray", "array size overflowed", count );

   return x_xgrab( size );
}

/* A new block filled with zeros */
void *x_xgrabz( size_t bytes )
{
   void *memory = x_xgrab( bytes );

   if( bytes != 0 )
      memset( memory, 0, bytes );

   return memory;
}

/* A new array of count items filled with zeros */
void *x_xgrabzArray( size_t count, size_t itemSize )
{
   size_t size;

   if( !s_memoryArraySize( count, itemSize, &size ) )
      s_memoryAbort( "x_xgrabzArray", "array size overflowed", count );

   return x_xgrabz( size );
}

/* A new block with a copy of bytes from source */
void *x_xmemdup( const void *source, size_t bytes )
{
   void *copy;

   if( source == NULL && bytes != 0 )
      s_memoryAbort( "x_xmemdup", "source is NULL", bytes );

   copy = x_xgrab( bytes );
   if( bytes != 0 )
      memcpy( copy, source, bytes );

   return copy;
}

/* Resize a block that is not shared; NULL area allocates, 0 bytes releases */
void *x_xrealloc( void *area, size_t bytes )
{
   X_MEMORY_HEADER *header;
   size_t oldSize;
   size_t total;

   if( area == NULL && bytes == 0 )
      return NULL;

   if( area == NULL )
      return x_xgrab( bytes );

   if( bytes == 0 )
   {
      x_xfree( area );
      return NULL;
   }

   header = s_memoryHeader( area, "x_xrealloc" );
   if( header->block.refs != 1 )
      s_memoryAbort( "x_xrealloc", "block is shared", header->block.size );

   oldSize = header->block.size;
   if( !s_memoryTotalSize( bytes, &total ) )
      s_memoryAbort( "x_xrealloc", "size overflowed", bytes );

   header = ( X_MEMORY_HEADER * )realloc( header, total );
   if( header == NULL )
      s_memoryAbort( "x_xrealloc", "allocation failed", bytes );

   header->block.magic = X_MEMORY_HEAD_MAGIC;
   header->block.size = bytes;
   s_memoryWriteTail( header );
   s_memoryCountResize( oldSize, bytes );

   return header + 1;
}

/* Resize an array to count items */
void *x_xreallocArray( void *area, size_t count, size_t itemSize )
{
   size_t size;

   if( !s_memoryArraySize( count, itemSize, &size ) )
      s_memoryAbort( "x_xreallocArray", "array size overflowed", count );

   return x_xrealloc( area, size );
}

/* Resize a block and fill all of it with zeros */
void *x_xreallocz( void *area, size_t bytes )
{
   void *memory = x_xrealloc( area, bytes );

   if( bytes != 0 )
      memset( memory, 0, bytes );

   return memory;
}

/* ---------------------------------------------------------------------------------------------
   Shared blocks and statistics
   --------------------------------------------------------------------------------------------- */

/* The reference count of a block; 0 for NULL */
size_t x_xrefs( const void *area )
{
   X_MEMORY_HEADER *header = s_memoryHeader( area, "x_xrefs" );

   return IIF( header != NULL, header->block.refs, 0 );
}

/* Drop one reference; the last one releases the block */
void x_xrelease( void *area )
{
   X_MEMORY_HEADER *header = s_memoryHeader( area, "x_xrelease" );

   if( header == NULL )
      return;

   if( header->block.refs > 1 )
      --header->block.refs;
   else
      x_xfree( area );
}

/* Add one reference */
void *x_xretain( void *area )
{
   X_MEMORY_HEADER *header = s_memoryHeader( area, "x_xretain" );

   if( header != NULL )
      ++header->block.refs;

   return area;
}

/* The size of a block as it was asked for; 0 for NULL */
size_t x_xsize( const void *area )
{
   X_MEMORY_HEADER *header = s_memoryHeader( area, "x_xsize" );

   return IIF( header != NULL, header->block.size, 0 );
}

/* The memory statistics */
void x_xstats( X_MEMORY_STATS *stats )
{
   if( stats != NULL )
   {
      stats->currentBytes = atomic_load( &s_currentBytes );
      stats->currentBlocks = atomic_load( &s_currentBlocks );
      stats->peakBytes = atomic_load( &s_peakBytes );
      stats->peakBlocks = atomic_load( &s_peakBlocks );
      stats->totalAllocations = atomic_load( &s_totalAllocations );
      stats->totalReallocations = atomic_load( &s_totalReallocations );
   }
}
