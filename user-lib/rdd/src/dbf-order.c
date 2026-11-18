/*
 * Copyright (c) 2024-2026 xDev-Language
 *
 * Library: xDev 7.0.0 RDD
 * File: dbf-order.c
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "dbf-order.h"

#include <xdev/dbinfo.ch>
#include <xdev/fileio.ch>
#include <math.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct
{
   DBF         area;
   X_DBF_STATE state;
   char       *record;
} X_DBF_ORDER_GUARD;

typedef struct
{
   X_DBF_ORDER       *index;
   X_DBF_INDEX_CURSOR cursor;
} X_DBF_CURSOR_GUARD;

static void s_setInteger( X_ITEM *result, int64_t value )
{
   x_itemClear( result );
   result->nType = X_IT_INTEGER;
   result->u.integer = value;
}

static void s_setText( X_ITEM *result, const char *value )
{
   x_itemPutStringBuffer( result, x_strdup( value ), strlen( value ) );
}

int x_dbfOrderCompare( const X_DBF_ORDER *index, const X_DBF_INDEX_KEY *left, const X_DBF_INDEX_KEY *right,
                       size_t length, int records )
{
   int order = memcmp( left->value, right->value, length );
   if( index->descending )
      order = -order;
   if( order == 0 && records )
   {
      order = ( left->record > right->record ) - ( left->record < right->record );
      if( index->bag->cdx && index->descending )
         order = -order;
   }
   return order;
}

static X_DBF_INDEX_KEY *s_key( const X_DBF_ORDER *index, const X_DBF_ORDER_PAGE *page, int slot )
{
   return ( X_DBF_INDEX_KEY * )( page->keys + ( size_t )slot * index->stride );
}

static void s_total( X_DBF_ORDER_PAGE *page )
{
   int slot;
   page->total = ( uint64_t )page->count;
   for( slot = 0; slot <= page->count; ++slot )
   {
      if( page->children[slot] != NULL )
         page->total += page->children[slot]->total;
   }
}

static X_DBF_ORDER_PAGE *s_pageNew( X_DBF_ORDER *index )
{
   X_DBF_ORDER_PAGE *page;
   if( index->pageCount == UINT32_MAX / NTX_PAGE_SIZE - 1 )
      return NULL;
   if( index->pageCount == index->pageCapacity )
   {
      size_t capacity = IIF( index->pageCapacity == 0, 16, index->pageCapacity * 2 );
      index->pages = ( X_DBF_ORDER_PAGE ** )x_xrealloc( index->pages, capacity * sizeof( *index->pages ) );
      index->pageCapacity = capacity;
   }
   page = ( X_DBF_ORDER_PAGE * )x_xgrab( sizeof( *page ) );
   memset( page, 0, sizeof( *page ) );
   page->number = ( uint32_t )++index->pageCount;
   page->children = ( X_DBF_ORDER_PAGE ** )x_xgrab( ( index->maximum + 2 ) * sizeof( *page->children ) );
   page->keys = ( unsigned char * )x_xgrab( ( index->maximum + 1 ) * index->stride );
   index->pages[page->number - 1] = page;
   memset( page->children, 0, ( index->maximum + 2 ) * sizeof( *page->children ) );
   memset( page->keys, 0, ( index->maximum + 1 ) * index->stride );
   page->count = 0;
   page->total = 0;
   index->changed = 1;
   return page;
}

void x_dbfOrderPagesClear( X_DBF_ORDER *index )
{
   size_t number;
   for( number = 0; number < index->pageCount; ++number )
   {
      X_DBF_ORDER_PAGE *page = index->pages[number];
      x_xfree( page->keys );
      x_xfree( page->children );
      x_xfree( page );
   }
   x_xfree( index->pages );
   index->pages = NULL;
   index->pageCount = index->pageCapacity = 0;
   index->root = NULL;
   index->freePage = 0;
}

static void s_bagDrop( X_DBF_INDEX_BAG **pointer )
{
   X_DBF_INDEX_BAG *bag = *pointer;
   int              remove = bag->temporary && bag->file.nType != X_IT_NIL;
   if( --bag->references != 0 )
      return;
   x_dbfCdxDiscard( bag );
   if( bag->file.nType != X_IT_NIL )
   {
      x_fileClose( &bag->file );
      x_gcUnlock( bag->file.u.pPointer );
      x_itemClear( &bag->file );
   }
   if( remove )
      x_fsDelete( bag->path );
   x_xfree( bag->path );
   x_xfree( bag );
}

static void s_indexLockParameters( int scheme, int64_t *offset, int64_t *length )
{
   switch( scheme )
   {
      case DB_DBFLOCK_HB64:
         *offset = INT64_C( 0x7FFFFFFF00000001 );
         *length = INT64_C( 0x10001 );
         break;
      case DB_DBFLOCK_COMIX:
      case DB_DBFLOCK_HB32:
         *offset = INT64_C( 0xFFFEFFFF );
         *length = INT64_C( 0x10001 );
         break;
      case DB_DBFLOCK_VFP:
         *offset = INT64_C( 0x7FFFFFFE );
         *length = 1;
         break;
      default:
         *offset = INT64_C( 1000000000 );
         *length = 1;
         break;
   }
}

static X_DBF_STATUS s_bagUnlock( X_DBF_INDEX_BAG *bag )
{
   if( --bag->lockDepth == 0 )
   {
      int64_t offset, length;
      s_indexLockParameters( bag->area->lockScheme, &offset, &length );
      if( bag->area->shared && !bag->exclusive &&
          !x_fileLock( &bag->file, offset, length, IIF( bag->lockMode == 2, FLX_EXCLUSIVE, FLX_SHARED ), 1 ) )
      {
         X_DBF_ORDER *index;
         for( index = bag->area->indexes; index != NULL; index = index->next )
            if( index->bag == bag )
               index->broken = 1;
         return X_DBF_LOCK_FAILED;
      }
      bag->lockMode = 0;
   }
   return X_DBF_OK;
}

void x_dbfOrderUnlock( X_DBF_INDEX_LOCK *guard )
{
   int error = x_fsError();
   while( guard->count != 0 )
   {
      X_DBF_INDEX_BAG *bag = guard->bags[--guard->count];
      s_bagUnlock( bag );
   }
   x_xfree( guard->bags );
   guard->bags = NULL;
   x_dbfFileLeave( &guard->file );
   if( guard->area != NULL )
   {
      --guard->area->indexLockDepth;
      guard->area = NULL;
   }
   x_fsSetError( error );
}

static X_DBF_STATUS s_lockBags( DBF area, int write, X_DBF_INDEX_LOCK *guard, const X_DBF_INDEX_BAG *selected )
{
   X_DBF_ORDER *index;
   size_t       count = 0, slot, position;
   X_DBF_STATUS status;
   if( area->indexes == NULL )
      return X_DBF_OK;
   if( write )
   {
      status = x_dbfFileEnter( area, &guard->file );
      if( status != X_DBF_OK )
         return status;
   }
   guard->area = area;
   ++area->indexLockDepth;
   for( index = area->indexes; index != NULL; index = index->next )
      if( selected == NULL || index->bag == selected )
         ++count;
   guard->bags = ( X_DBF_INDEX_BAG ** )x_xgrab( count * sizeof( *guard->bags ) );
   count = 0;
   for( index = area->indexes; index != NULL; index = index->next )
   {
      if( selected != NULL && index->bag != selected )
         continue;
      for( slot = 0; slot < count && guard->bags[slot] != index->bag; ++slot )
      {
      }
      if( slot == count )
      {
         for( position = count; position != 0 && x_stricmp( guard->bags[position - 1]->path, index->bag->path ) > 0;
              --position )
            guard->bags[position] = guard->bags[position - 1];
         guard->bags[position] = index->bag;
         ++count;
      }
   }
   for( slot = 0; slot < count; ++slot )
   {
      X_DBF_INDEX_BAG *bag = guard->bags[slot];
      if( bag->lockDepth != 0 )
      {
         if( write && bag->lockMode != 2 )
            return X_DBF_LOCK_FAILED;
      }
      else
      {
         int64_t offset, length;
         s_indexLockParameters( area->lockScheme, &offset, &length );
         if( area->shared && !bag->exclusive &&
             !x_dbfLockWait( &bag->file, offset, length, IIF( write, FLX_EXCLUSIVE, FLX_SHARED ), X_DBF_LOCK_WAIT ) )
            return X_DBF_LOCK_FAILED;
         bag->lockMode = IIF( write, 2, 1 );
      }
      ++bag->lockDepth;
      ++guard->count;
   }
   for( slot = 0; slot < guard->count; ++slot )
   {
      X_DBF_INDEX_BAG *bag = guard->bags[slot];
      if( bag->initializing )
         continue;
      if( bag->cdx )
      {
         int force = 0;
         for( index = area->indexes; index != NULL; index = index->next )
            if( index->bag == bag && index->cdxHeader == 0 )
               force = 1;
         if( area->shared || force )
         {
            status = x_dbfCdxLoad( bag, force );
            if( status != X_DBF_OK )
               return status;
         }
      }
      else
      {
         int force = 0;
         for( index = area->indexes; index != NULL; index = index->next )
            if( index->bag == bag && index->ntxRoot == 0 )
               force = 1;
         if( force || ( area->shared && bag->compound ) )
         {
            status = x_dbfNtxDirectory( bag, force );
            if( status != X_DBF_OK )
               return status;
         }
         for( index = area->indexes; index != NULL; index = index->next )
            if( index->bag == bag && ( area->shared || index->ntxRoot == 0 ) )
            {
               status = x_dbfNtxLoad( index, index->ntxRoot == 0 );
               if( status != X_DBF_OK )
                  return status;
            }
      }
      if( write && bag->unbound != 0 )
         return X_DBF_INDEX_EXPRESSION_REQUIRED;
   }
   return X_DBF_OK;
}

X_DBF_STATUS x_dbfOrderLock( DBF area, int write, X_DBF_INDEX_LOCK *guard )
{
   return s_lockBags( area, write, guard, NULL );
}

/* Forces every index file of the area to disk once. Index changes reach other processes at
   once; durability comes from DbCommit() and close, the same as for the table */
X_DBF_STATUS x_dbfOrderCommit( DBF area )
{
   X_DBF_ORDER *index, *previous;
   if( area->readOnly )
      return X_DBF_OK;
   for( index = area->indexes; index != NULL; index = index->next )
   {
      for( previous = area->indexes; previous != index && previous->bag != index->bag; previous = previous->next )
      {
      }
      if( previous == index && index->bag->file.nType != X_IT_NIL && !x_fileCommit( &index->bag->file ) )
         return X_DBF_WRITE_ERROR;
   }
   return X_DBF_OK;
}

static void s_indexDrop( X_DBF_ORDER **pointer )
{
   X_DBF_ORDER *index = *pointer;
   if( index == NULL )
      return;
   x_dbfNtxDiscard( index );
   s_bagDrop( &index->bag );
   x_itemClear( &index->block );
   x_itemClear( &index->forBlock );
   x_itemClear( &index->scopes[0] );
   x_itemClear( &index->scopes[1] );
   x_dbfOrderPagesClear( index );
   x_xfree( index );
}

void x_dbfOrderCursorClear( DBF area )
{
   X_DBF_ORDER *index;
   for( index = area->indexes; index != NULL; index = index->next )
      index->cursor.valid = index->cursor.keyValid = 0;
}

static void s_listDrop( DBF area )
{
   while( area->indexes != NULL )
   {
      X_DBF_ORDER *index = area->indexes;
      area->indexes = index->next;
      s_indexDrop( &index );
   }
   area->order = NULL;
}

static const char *s_baseName( const char *path )
{
   const char *base = path;
   for( ; *path != '\0'; ++path )
   {
      if( *path == '/' || *path == '\\' || *path == ':' )
         base = path + 1;
   }
   return base;
}

static void s_invalidateRelations( DBF area )
{
   X_DBF_RELATION *relation;
   for( relation = area->relations; relation != NULL; relation = relation->next )
   {
      relation->child->pendingRelation = relation;
      s_invalidateRelations( relation->child );
   }
}

static void s_restore( X_DBF_ORDER_GUARD *guard )
{
   guard->area->indexBusy = 0;
   if( guard->record != NULL )
   {
      memcpy( guard->area->record, guard->record, guard->area->schema->recordLength );
      guard->area->state = guard->state;
      s_invalidateRelations( guard->area );
      x_xfree( guard->record );
   }
}

static void s_bytesDrop( X_DBF_TEXT *bytes )
{
   x_dbfTextClear( bytes );
}

static X_DBF_STATUS s_eval( DBF area, const X_ITEM *block, X_ITEM *result )
{
   X_CALL_CTX call = { NULL, 0, result, NULL, NULL };
   return x_dbfAreaEvalBlock( &call, area, block, result );
}

static X_DBF_STATUS s_valueKey( X_DBF_ORDER *index, const X_ITEM *value, X_DBF_INDEX_KEY *key, size_t *length )
{
   size_t position;
   *length = index->length;
   if( index->type == 'C' && value->nType == X_IT_STRING )
   {
      X_DBF_TEXT   bytes __attribute__( ( cleanup( s_bytesDrop ) ) ) = { NULL, 0 };
      X_DBF_STATUS status = x_dbfTextCopy( value->u.string.data, value->u.string.len, &bytes );
      if( status != X_DBF_OK )
         return status;
      *length = x_dbfTextFit( bytes.data, bytes.length, index->length );
      memcpy( key->value, bytes.data, *length );
      memset( key->value + *length, ' ', index->length - *length );
   }
   else if( index->bag->cdx &&
            ( ( index->type == 'N' && X_IS_NUMERIC( value ) ) || ( index->type == 'D' && value->nType == X_IT_DATE ) ) )
   {
      double   number = IIF( value->nType == X_IT_DOUBLE, value->u.dbl, ( double )value->u.integer );
      uint64_t bits;
      if( !isfinite( number ) )
         return X_DBF_INVALID_INDEX_KEY;
      if( number == 0 )
         number = 0;
      memcpy( &bits, &number, sizeof( bits ) );
      bits = IIF( bits >> 63, ~bits, bits ^ ( UINT64_C( 1 ) << 63 ) );
      for( position = 0; position < 8; ++position )
         key->value[position] = ( unsigned char )( bits >> ( 56 - position * 8 ) );
   }
   else if( index->type == 'N' && X_IS_NUMERIC( value ) )
   {
      char         number[X_DBF_KEY_SIZE + 1];
      X_DBF_FIELD  field = { { 0 }, X_DBF_NUMERIC, index->length, ( uint8_t )index->decimals, 0 };
      X_DBF_STATUS status = x_dbfFormatNumeric( &field, value, number );
      if( status != X_DBF_OK )
         return X_DBF_INVALID_INDEX_KEY;
      number[index->length] = '\0';
      position = 0;
      while( number[position] == ' ' )
         number[position++] = '0';
      if( number[position] == '-' )
      {
         number[position] = '0';
         for( position = 0; position < index->length; ++position )
         {
            if( number[position] >= '0' && number[position] <= '9' )
               number[position] = ( char )( 44 - ( number[position] - '0' ) );
         }
      }
      memcpy( key->value, number, index->length );
   }
   else if( index->type == 'D' && value->nType == X_IT_DATE )
   {
      int  year, month, day;
      char date[9];
      x_dateDecode( value->u.integer, &year, &month, &day );
      if( value->u.integer == 0 )
         memset( key->value, ' ', 8 );
      else
      {
         snprintf( date, sizeof( date ), "%04d%02d%02d", year, month, day );
         memcpy( key->value, date, 8 );
      }
   }
   else if( index->type == 'L' && value->nType == X_IT_LOGICAL )
      key->value[0] = IIF( value->u.logical, 'T', 'F' );
   else
      return X_DBF_INVALID_INDEX_KEY;
   return X_DBF_OK;
}

static X_DBF_STATUS s_evaluate( X_DBF_ORDER *index, X_DBF_INDEX_KEY *key, int *included )
{
   X_ITEM       value __attribute__( ( cleanup( x_itemClear ) ) ) = X_ITEM_NIL_INIT;
   X_DBF_STATUS status;
   size_t       length;
   *included = 1;
   if( index->forBlock.nType != X_IT_NIL )
   {
      status = s_eval( index->area, &index->forBlock, &value );
      if( status != X_DBF_OK )
         return status;
      if( value.nType != X_IT_LOGICAL )
         return X_DBF_INVALID_INDEX_KEY;
      *included = value.u.logical != 0;
   }
   if( !*included )
      return X_DBF_OK;
   status = s_eval( index->area, &index->block, &value );
   if( status != X_DBF_OK )
      return status;
   key->record = ( uint32_t )index->area->state.record;
   return s_valueKey( index, &value, key, &length );
}

int x_dbfOrderKeyAt( const X_DBF_ORDER *index, uint64_t rank, X_DBF_INDEX_KEY *key )
{
   X_DBF_ORDER_PAGE *page = index->root;
   while( page != NULL )
   {
      int slot;
      for( slot = 0; slot <= page->count; ++slot )
      {
         uint64_t count = IIF( page->children[slot] != NULL, page->children[slot]->total, 0 );
         if( rank < count )
            break;
         rank -= count;
         if( slot < page->count )
         {
            if( rank == 0 )
            {
               memcpy( key, s_key( index, page, slot ), sizeof( uint32_t ) + index->length );
               return 1;
            }
            --rank;
         }
      }
      if( slot > page->count )
         return 0;
      page = page->children[slot];
   }
   return 0;
}

static X_DBF_STATUS s_insert( X_DBF_ORDER *index, X_DBF_ORDER_PAGE *page, X_DBF_INDEX_KEY *entry,
                              X_DBF_ORDER_PAGE **right )
{
   int               slot = 0;
   X_DBF_INDEX_KEY   promoted;
   X_DBF_ORDER_PAGE *child = NULL;
   X_DBF_STATUS      status;
   while( slot < page->count && x_dbfOrderCompare( index, s_key( index, page, slot ), entry, index->length,
                                                   index->sortRecords ) < IIF( index->sortRecords, 0, 1 ) )
      ++slot;
   if( page->children[slot] != NULL )
   {
      memcpy( &promoted, entry, sizeof( uint32_t ) + index->length );
      status = s_insert( index, page->children[slot], &promoted, &child );
      if( status != X_DBF_OK )
         return status;
      if( child == NULL )
      {
         s_total( page );
         *right = NULL;
         return X_DBF_OK;
      }
   }
   else
      memcpy( &promoted, entry, sizeof( uint32_t ) + index->length );
   memmove( s_key( index, page, slot + 1 ), s_key( index, page, slot ), ( page->count - slot ) * index->stride );
   memmove( page->children + slot + 2, page->children + slot + 1, ( page->count - slot ) * sizeof( *page->children ) );
   memcpy( s_key( index, page, slot ), &promoted, sizeof( uint32_t ) + index->length );
   page->children[slot + 1] = child;
   ++page->count;
   *right = NULL;
   if( page->count > index->maximum )
   {
      int               middle = index->maximum / 2;
      X_DBF_ORDER_PAGE *split = s_pageNew( index );
      if( split == NULL )
         return X_DBF_TEXT_OVERFLOW;
      split->count = middle;
      memcpy( split->keys, s_key( index, page, middle + 1 ), middle * index->stride );
      memcpy( split->children, page->children + middle + 1, ( middle + 1 ) * sizeof( *split->children ) );
      memcpy( entry, s_key( index, page, middle ), sizeof( uint32_t ) + index->length );
      page->count = middle;
      s_total( split );
      *right = split;
   }
   s_total( page );
   return X_DBF_OK;
}

static X_DBF_STATUS s_sync( X_DBF_ORDER *index )
{
   X_DBF_STATUS status = IIF( index->bag->cdx, x_dbfCdxSync( index->bag ), x_dbfNtxSync( index ) );
   if( status != X_DBF_OK )
   {
      X_DBF_ORDER *order;
      for( order = index->area->indexes; order != NULL; order = order->next )
         if( order->bag == index->bag )
            order->broken = 1;
   }
   return status;
}

static void s_sort( X_DBF_ORDER *index, unsigned char *keys, unsigned char *scratch, size_t count )
{
   size_t middle, left, right, output;
   if( count < 2 )
      return;
   middle = count / 2;
   s_sort( index, keys, scratch, middle );
   s_sort( index, keys + middle * index->stride, scratch, count - middle );
   left = output = 0;
   right = middle;
   while( left < middle || right < count )
   {
      size_t selected;
      if( right == count ||
          ( left < middle &&
            x_dbfOrderCompare( index, ( X_DBF_INDEX_KEY * )( keys + left * index->stride ),
                               ( X_DBF_INDEX_KEY * )( keys + right * index->stride ), index->length, 1 ) <= 0 ) )
         selected = left++;
      else
         selected = right++;
      memcpy( scratch + output++ * index->stride, keys + selected * index->stride, index->stride );
   }
   memcpy( keys, scratch, count * index->stride );
}

static X_DBF_STATUS s_boundary( X_DBF_ORDER *index, int bottom, X_DBF_INDEX_CURSOR *cursor );
static X_DBF_STATUS s_step( X_DBF_ORDER *index, X_DBF_INDEX_CURSOR *cursor, int backwards );
static X_DBF_STATUS s_find( X_DBF_ORDER *index, const X_DBF_INDEX_KEY *key, size_t length, int records, int strict,
                            int backwards, X_DBF_INDEX_CURSOR *cursor );
static int          s_scope( const X_DBF_ORDER *index, const X_DBF_INDEX_KEY *key );
static X_DBF_STATUS s_current( X_DBF_ORDER *index, X_DBF_INDEX_CURSOR *cursor );

static X_DBF_STATUS s_buildEval( X_DBF_ORDER *index, const X_DBF_INDEX_BUILD *build, uint64_t *step, int *proceed )
{
   X_ITEM       value __attribute__( ( cleanup( x_itemClear ) ) ) = X_ITEM_NIL_INIT;
   X_DBF_STATUS status;
   *proceed = 1;
   if( *step >= build->step )
   {
      *step = 0;
      status = s_eval( index->area, &build->evalBlock, &value );
      if( status != X_DBF_OK )
         return status;
      if( value.nType != X_IT_LOGICAL )
         return X_DBF_INVALID_INDEX_KEY;
      *proceed = value.u.logical;
   }
   if( *proceed )
      ++*step;
   return X_DBF_OK;
}

static void s_buildingDrop( DBF *area )
{
   ( *area )->indexBuilding = NULL;
   ( *area )->indexKeysIncluded = 0;
   ( *area )->indexEvalStep = 0;
}

static X_DBF_STATUS s_build( X_DBF_ORDER *index, const X_DBF_INDEX_BUILD *build )
{
   DBF                area = index->area;
   DBF                building __attribute__( ( cleanup( s_buildingDrop ) ) ) = area;
   X_DBF_ORDER_GUARD  guard __attribute__( ( cleanup( s_restore ) ) ) = { area, area->state, NULL };
   X_DBF_TEXT         keys __attribute__( ( cleanup( s_bytesDrop ) ) ) = { NULL, 0 };
   X_DBF_TEXT         scratch __attribute__( ( cleanup( s_bytesDrop ) ) ) = { NULL, 0 };
   X_DBF_STATUS       status;
   X_ITEM             evaluated __attribute__( ( cleanup( x_itemClear ) ) ) = X_ITEM_NIL_INIT;
   X_DBF_ORDER       *source = NULL;
   X_DBF_INDEX_CURSOR cursor = { 0 };
   uint64_t           record, count, remaining = 0, step = 0;
   size_t             keyCount = 0, capacity;
   status = x_dbfAreaRecordCount( area, &count );
   if( status != X_DBF_OK )
      return status;
   if( count > UINT32_MAX || count > PTRDIFF_MAX / index->stride )
      return X_DBF_TEXT_OVERFLOW;
   guard.record = ( char * )x_xgrab( area->schema->recordLength );
   memcpy( guard.record, area->record, area->schema->recordLength );
   capacity = ( size_t )count;
   keys.data = ( char * )x_xgrab( IIF( count != 0, ( size_t )count * index->stride, 1 ) );
   step = IIF( build != NULL, build->step, 0 );
   record = 1;
   if( build != NULL && build->scoped )
   {
      source = area->order;
      if( build->record != 0 )
      {
         record = build->record;
         remaining = 1;
      }
      else if( build->rest || build->next != 0 )
      {
         record = IIF( build->start != 0, build->start, guard.state.record );
         remaining = build->next;
      }
      else if( !build->current && !build->useFilter )
         source = NULL;
      if( source != NULL )
      {
         if( record != 1 || build->rest || build->next != 0 || build->record != 0 )
         {
            status = x_dbfAreaGoTo( area, record );
            if( status == X_DBF_OK && !area->state.eof )
               status = s_current( source, &cursor );
            record = IIF( area->state.eof, count + 1, area->state.record );
         }
         else
         {
            status = s_boundary( source, 0, &cursor );
            record = IIF( cursor.valid, cursor.key.record, count + 1 );
         }
         if( status != X_DBF_OK )
            return status;
      }
   }
   if( index->custom )
      record = count + 1;
   // Index being built and keys added so far, read by OrdInfo() in the EVAL block
   building->indexBuilding = index;
   building->indexKeysIncluded = 0;
   building->indexEvalStep = IIF( build != NULL, build->step, 0 );
   area->indexBusy = 1;
   while( record != 0 && record <= count )
   {
      int             included = 1;
      X_DBF_INDEX_KEY key;
      if( source != NULL && s_scope( source, &cursor.key ) != 0 )
         break;
      status = area->driver->operations.read( area, record, area->record, area->schema->recordLength );
      if( status != X_DBF_OK )
         return status;
      area->state.record = record;
      area->state.bof = area->state.eof = area->state.found = area->state.dirty = 0;
      area->state.deleted = area->record[0] == '*';
      s_invalidateRelations( area );
      if( build != NULL && build->useFilter )
      {
         status = x_dbfAreaAccept( area, x_setDeleted, &included );
         if( status != X_DBF_OK )
            return status;
         if( !included )
            goto next_record;
      }
      if( index->bag->cdx && build != NULL && build->evalBlock.nType == X_IT_BLOCK )
      {
         int proceed;
         status = s_buildEval( index, build, &step, &proceed );
         if( status != X_DBF_OK )
            return status;
         if( !proceed )
            break;
      }
      if( included && build != NULL && build->whileBlock.nType == X_IT_BLOCK )
      {
         status = s_eval( area, &build->whileBlock, &evaluated );
         if( status != X_DBF_OK )
            return status;
         if( evaluated.nType != X_IT_LOGICAL )
            return X_DBF_INVALID_INDEX_KEY;
         if( !evaluated.u.logical )
            break;
      }
      if( included )
      {
         status = s_evaluate( index, &key, &included );
         if( status != X_DBF_OK )
            return status;
         if( included )
         {
            if( keyCount == capacity )
            {
               size_t grown = IIF( capacity != 0, capacity * 2, 1 );
               if( grown < capacity || grown > PTRDIFF_MAX / index->stride )
                  return X_DBF_TEXT_OVERFLOW;
               keys.data = ( char * )x_xrealloc( keys.data, grown * index->stride );
               capacity = grown;
            }
            memcpy( keys.data + keyCount++ * index->stride, &key, sizeof( uint32_t ) + index->length );
            area->indexKeysIncluded = keyCount;
         }
      }
      if( remaining != 0 && --remaining == 0 )
         break;
      if( !index->bag->cdx && build != NULL && build->evalBlock.nType == X_IT_BLOCK )
      {
         int proceed;
         status = s_buildEval( index, build, &step, &proceed );
         if( status != X_DBF_OK )
            return status;
         if( !proceed )
         {
            index->partial = 1;
            break;
         }
      }
   next_record:
      if( source != NULL )
      {
         status = s_step( source, &cursor, 0 );
         if( status != X_DBF_OK )
            return status;
         record = IIF( cursor.valid, cursor.key.record, count + 1 );
      }
      else
         ++record;
   }
   if( index->bag->cdx && !index->custom && count != 0 && build != NULL && build->evalBlock.nType == X_IT_BLOCK &&
       step != 0 )
   {
      if( record == 0 || record > count )
      {
         area->indexBusy = 0;
         status = x_dbfAreaGoTo( area, 0 );
         area->indexBusy = 1;
         if( status != X_DBF_OK )
            return status;
      }
      status = s_eval( area, &build->evalBlock, &evaluated );
      if( status != X_DBF_OK )
         return status;
      if( evaluated.nType != X_IT_LOGICAL )
         return X_DBF_INVALID_INDEX_KEY;
   }
   scratch.data = ( char * )x_xgrab( IIF( keyCount != 0, keyCount * index->stride, 1 ) );
   s_sort( index, ( unsigned char * )keys.data, ( unsigned char * )scratch.data, keyCount );
   x_dbfOrderPagesClear( index );
   index->broken = 1;
   index->root = s_pageNew( index );
   if( index->root == NULL )
      return X_DBF_TEXT_OVERFLOW;
   for( record = 0; record < keyCount; ++record )
   {
      const X_DBF_INDEX_KEY *key = ( const X_DBF_INDEX_KEY * )( keys.data + record * index->stride );
      X_DBF_INDEX_KEY        entry;
      X_DBF_ORDER_PAGE      *right;
      if( index->unique && record != 0 &&
          x_dbfOrderCompare( index, ( const X_DBF_INDEX_KEY * )( keys.data + ( record - 1 ) * index->stride ), key,
                             index->length, 0 ) == 0 )
         continue;
      memcpy( &entry, key, sizeof( uint32_t ) + index->length );
      status = s_insert( index, index->root, &entry, &right );
      if( status != X_DBF_OK )
         return status;
      if( right != NULL )
      {
         X_DBF_ORDER_PAGE *root = s_pageNew( index );
         if( root == NULL )
            return X_DBF_TEXT_OVERFLOW;
         root->count = 1;
         memcpy( s_key( index, root, 0 ), &entry, sizeof( uint32_t ) + index->length );
         root->children[0] = index->root;
         root->children[1] = right;
         s_total( root );
         index->root = root;
      }
   }
   index->broken = 0;
   if( index->bag->cdx )
      index->bag->cdxRebuild = 1;
   return X_DBF_OK;
}

static X_DBF_STATUS s_bagPath( DBF area, const char *path, X_DBF_INDEX_BAG *bag )
{
   X_ITEM       value __attribute__( ( cleanup( x_itemClear ) ) ) = X_ITEM_NIL_INIT;
   X_DBF_TEXT   bytes __attribute__( ( cleanup( s_bytesDrop ) ) ) = { NULL, 0 };
   const char  *base, *extension;
   size_t       length;
   X_DBF_STATUS status;
   if( !bag->cdx && path[0] == '\0' )
      return X_DBF_INVALID_ARGUMENT;
   if( path[0] == '\0' )
   {
      status = x_dbfAreaInfo( area, DBI_FULLPATH, &value );
      if( status != X_DBF_OK )
         return status;
      base = s_baseName( value.u.string.data );
      extension = strrchr( base, '.' );
      length = IIF( extension != NULL, ( size_t )( extension - value.u.string.data ), value.u.string.len );
      x_dbfTextClear( &bytes );
      bytes.data = ( char * )x_xgrab( length + 5 );
      memcpy( bytes.data, value.u.string.data, length );
      strcpy( bytes.data + length, ".cdx" );
      bag->structural = 1;
   }
   else
   {
      base = s_baseName( path );
      length = strlen( path );
      x_dbfTextClear( &bytes );
      bytes.data = ( char * )x_xgrab( length + 5 );
      strcpy( bytes.data, path );
      if( strchr( base, '.' ) == NULL )
         strcat( bytes.data, IIF( bag->cdx, ".cdx", IIF( bag->compound, ".ctx", ".ntx" ) ) );
   }
   bag->path = x_fileDefName( bytes.data );
   if( bag->cdx && !bag->structural )
   {
      status = x_dbfAreaInfo( area, DBI_FULLPATH, &value );
      if( status != X_DBF_OK )
         return status;
      base = s_baseName( value.u.string.data );
      extension = strrchr( base, '.' );
      length = IIF( extension != NULL, ( size_t )( extension - value.u.string.data ), value.u.string.len );
      x_dbfTextClear( &bytes );
      bytes.data = ( char * )x_xgrab( length + 5 );
      memcpy( bytes.data, value.u.string.data, length );
      strcpy( bytes.data + length, ".cdx" );
      bag->structural = x_stricmp( bag->path, bytes.data ) == 0;
   }
   return X_DBF_OK;
}

static X_DBF_STATUS s_indexDefine( DBF area, const X_DBF_INDEX_OPTIONS *options, X_DBF_ORDER **result )
{
   X_DBF_ORDER *index __attribute__( ( cleanup( s_indexDrop ) ) ) = NULL;
   X_ITEM       value __attribute__( ( cleanup( x_itemClear ) ) ) = X_ITEM_NIL_INIT;
   X_DBF_TEXT   bytes __attribute__( ( cleanup( s_bytesDrop ) ) ) = { NULL, 0 };
   X_DBF_STATUS status;
   const char  *base, *extension;
   size_t       length;
   if( options->expression[0] == '\0' )
      return X_DBF_INVALID_ARGUMENT;
   index = ( X_DBF_ORDER * )x_xgrab( sizeof( *index ) );
   memset( index, 0, sizeof( *index ) );
   index->bag = ( X_DBF_INDEX_BAG * )x_xgrab( sizeof( *index->bag ) );
   memset( index->bag, 0, sizeof( *index->bag ) );
   index->bag->area = area;
   index->bag->references = 1;
   index->bag->cdx = strcmp( area->driver->name, "DBFCDX" ) == 0;
   index->bag->largeFile = area->lockScheme == DB_DBFLOCK_HB64;
   if( options->build != NULL )
   {
      index->custom = options->build->custom;
      index->template = index->multiKey = index->custom && index->bag->cdx;
      index->partial = options->build->scoped;
      index->bag->compound = !index->bag->cdx && options->build->compound;
      index->bag->temporary = options->build->temporary;
      index->bag->exclusive = options->build->exclusive || options->build->temporary;
   }
   index->area = area;
   index->opening = options->existing;
   index->sortRecords = 1;
   index->unique = options->unique != 0;
   index->descending = options->descending != 0;
   x_itemCopy( &index->block, options->block );
   if( options->forBlock != NULL )
      x_itemCopy( &index->forBlock, options->forBlock );
   status = x_dbfTextCopy( options->expression, strlen( options->expression ), &bytes );
   if( status != X_DBF_OK )
      return status;
   if( bytes.length >= sizeof( index->expression ) )
      return X_DBF_TEXT_OVERFLOW;
   memcpy( index->expression, bytes.data, bytes.length );
   if( options->condition != NULL )
   {
      status = x_dbfTextCopy( options->condition, strlen( options->condition ), &bytes );
      if( status != X_DBF_OK )
         return status;
      if( bytes.length >= sizeof( index->condition ) )
         return X_DBF_TEXT_OVERFLOW;
      memcpy( index->condition, bytes.data, bytes.length );
   }
   {
      X_DBF_ORDER_GUARD guard __attribute__( ( cleanup( s_restore ) ) ) = { area, area->state, NULL };
      area->indexBusy = 1;
      status = s_eval( area, &index->block, &value );
   }
   if( status != X_DBF_OK )
      return status;
   if( value.nType == X_IT_STRING )
   {
      status = x_dbfTextCopy( value.u.string.data, value.u.string.len, &bytes );
      if( status != X_DBF_OK )
         return status;
      if( !options->existing &&
          ( bytes.length == 0 || bytes.length > ( size_t )IIF( index->bag->cdx, 240, X_DBF_KEY_SIZE ) ) )
         return X_DBF_INVALID_INDEX_KEY;
      index->type = 'C';
      index->length = ( uint16_t )IIF( options->existing, 1, bytes.length );
   }
   else if( X_IS_NUMERIC( &value ) )
   {
      size_t field = x_dbfSchemaFindField( area->schema, options->expression );
      index->type = 'N';
      index->length = ( uint16_t )IIF( index->bag->cdx, 8, 20 );
      if( !options->existing && !index->bag->cdx && value.nType != X_IT_INTEGER && x_setDecimals >= index->length )
         return X_DBF_INVALID_INDEX_KEY;
      index->decimals =
         ( uint16_t )IIF( options->existing || index->bag->cdx || value.nType == X_IT_INTEGER, 0, x_setDecimals );
      if( !index->bag->cdx && field != 0 && area->schema->fields[field - 1].type == X_DBF_NUMERIC )
      {
         index->length = area->schema->fields[field - 1].length;
         index->decimals = area->schema->fields[field - 1].decimals;
      }
      if( index->decimals >= index->length )
         return X_DBF_INVALID_INDEX_KEY;
   }
   else if( value.nType == X_IT_DATE || value.nType == X_IT_LOGICAL )
   {
      index->type = IIF( value.nType == X_IT_DATE, 'D', 'L' );
      index->length = IIF( index->type == 'D', 8, 1 );
   }
   else
      return X_DBF_INVALID_INDEX_KEY;
   index->stride = ( sizeof( uint32_t ) + index->length + 3 ) & ~( size_t )3;
   index->maximum = ( NTX_PAGE_SIZE - 2 ) / ( index->length + 10 ) - 1;
   index->maximum &= ( uint16_t )~1;
   status = s_bagPath( area, options->path, index->bag );
   if( status != X_DBF_OK )
      return status;
   base = s_baseName( index->bag->path );
   extension = strrchr( base, '.' );
   length = IIF( extension != NULL, ( size_t )( extension - base ), strlen( base ) );
   if( options->name != NULL && options->name[0] != '\0' )
   {
      base = options->name;
      length = strlen( base );
   }
   if( length == 0 )
      return X_DBF_INVALID_ARGUMENT;
   status = x_dbfTextCopy( base, length, &bytes );
   if( status != X_DBF_OK )
      return status;
   if( options->existing && options->name != NULL && bytes.length > 10 )
      return X_DBF_INVALID_ARGUMENT;
   length = IIF( bytes.length > 10, 10, bytes.length );
   x_strncpyUpperTrim( index->diskName, bytes.data, length );
   status = x_dbfTextCopy( index->diskName, strlen( index->diskName ), &bytes );
   if( status != X_DBF_OK )
      return status;
   memcpy( index->name, bytes.data, bytes.length );
   if( options->existing && index->forBlock.nType == X_IT_BLOCK )
   {
      X_DBF_ORDER_GUARD guard __attribute__( ( cleanup( s_restore ) ) ) = { area, area->state, NULL };
      area->indexBusy = 1;
      status = s_eval( area, &index->forBlock, &value );
      if( status != X_DBF_OK )
         return status;
      if( value.nType != X_IT_LOGICAL )
         return X_DBF_INVALID_INDEX_KEY;
   }
   *result = index;
   index = NULL;
   return X_DBF_OK;
}

X_DBF_STATUS x_dbfOrderCreate( DBF area, const X_DBF_INDEX_OPTIONS *options )
{
   X_DBF_ORDER  *index __attribute__( ( cleanup( s_indexDrop ) ) ) = NULL;
   X_ITEM        value __attribute__( ( cleanup( x_itemClear ) ) ) = X_ITEM_NIL_INIT;
   X_DBF_ORDER **position;
   X_DBF_STATUS  status;
   void         *handle;
   if( area->indexBusy || area->indexLockDepth != 0 )
      return X_DBF_INVALID_STATE;
   if( strcmp( area->driver->name, "DBFNTX" ) != 0 && strcmp( area->driver->name, "DBFCDX" ) != 0 )
      return X_DBF_UNSUPPORTED_OPERATION;
   status = x_dbfAreaPrepare( area );
   if( status == X_DBF_OK )
      status = x_dbfAreaFlush( area );
   if( status != X_DBF_OK )
      return status;
   status = s_indexDefine( area, options, &index );
   if( status != X_DBF_OK )
      return status;
   if( area->shared && index->bag->structural )
      return X_DBF_INDEX_EXPRESSION_REQUIRED;
   {
      X_DBF_INDEX_LOCK guard __attribute__( ( cleanup( x_dbfOrderUnlock ) ) ) = { 0 };
      X_DBF_ORDER     *old;
      int              bind = 0, isFile = 0, isDirectory, published = 0;
      int              compound = index->bag->compound;
      for( old = area->indexes; old != NULL; old = old->next )
      {
         if( x_stricmp( old->bag->path, index->bag->path ) == 0 )
         {
            if( !old->bag->cdx && ( old->bag->compound || ( options->build != NULL && options->build->compound ) ) )
            {
               X_DBF_ORDER *tag;
               size_t       count = 0;
               int          replacing = 0;
               for( tag = area->indexes; tag != NULL; tag = tag->next )
                  if( tag->bag == old->bag )
                  {
                     ++count;
                     replacing |= strcmp( tag->diskName, index->diskName ) == 0;
                  }
               if( count == 63 && !replacing )
                  return X_DBF_TEXT_OVERFLOW;
            }
            s_bagDrop( &index->bag );
            index->bag = old->bag;
            ++index->bag->references;
            break;
         }
      }
      if( index->bag->file.nType == X_IT_NIL )
      {
         x_fsExists( index->bag->path, &isFile, &isDirectory );
         bind = area->shared && isFile;
         if( !bind && area->readOnly )
            return X_DBF_READ_ONLY;
         if( !bind && area->shared )
         {
            status = x_dbfAreaInfo( area, DBI_ISFLOCK, &value );
            if( status != X_DBF_OK )
               return status;
            if( !value.u.logical )
               return X_DBF_SHARED_OPERATION;
         }
         if( !bind && ( index->bag->cdx || index->bag->compound ) && isFile )
            return X_DBF_INDEX_EXPRESSION_REQUIRED;
         if( !bind )
         {
            status = s_build( index, options->build );
            if( status != X_DBF_OK )
               return status;
         }
         handle = IIF( bind,
                       x_fileOpen( index->bag->path, IIF( area->readOnly, FO_READ, FO_READWRITE ) |
                                                        IIF( index->bag->exclusive, FO_EXCLUSIVE, FO_DENYNONE ) ),
                       x_fileCreate( index->bag->path, 0 ) );
         if( handle == NULL )
            return IIF( bind, X_DBF_OPEN_ERROR, X_DBF_CREATE_ERROR );
         x_itemPutPtrGC( &index->bag->file, handle );
         x_gcLock( handle );
         index->bag->initializing = !bind;
      }
      else
      {
         bind = area->shared;
         if( !bind )
         {
            X_DBF_INDEX_LOCK buildLock __attribute__( ( cleanup( x_dbfOrderUnlock ) ) ) = { 0 };
            if( area->readOnly )
               return X_DBF_READ_ONLY;
            status = x_dbfOrderLock( area, 1, &buildLock );
            if( status == X_DBF_OK )
               status = s_build( index, options->build );
            if( status != X_DBF_OK )
               return status;
         }
      }
      for( position = &area->indexes; *position != NULL; position = &( *position )->next )
      {
      }
      *position = index;
      if( !bind )
         index->bag->initializing = 1;
      status = x_dbfOrderLock( area, !bind, &guard );
      if( status == X_DBF_OK && !bind )
      {
         X_DBF_ORDER **replacement = &area->indexes;
         if( compound && !index->bag->compound )
         {
            for( old = area->indexes; old != index; old = old->next )
               if( old->bag == index->bag )
               {
                  index->bag->ntxFreePage = old->freePage;
                  break;
               }
            index->bag->compound = 1;
         }
         while( *replacement != index )
         {
            X_DBF_ORDER *old = *replacement;
            if( old->bag == index->bag &&
                ( ( !index->bag->cdx && !index->bag->compound ) || strcmp( old->diskName, index->diskName ) == 0 ) )
            {
               *replacement = old->next;
               if( area->order == old )
                  area->order = NULL;
               if( old->bag->compound && old->ntxRoot != 0 )
               {
                  status = x_dbfNtxRelease( old, old->ntxRoot, 0 );
                  if( index->ntxHeader == 0 )
                     index->ntxHeader = old->ntxHeader;
                  if( status != X_DBF_OK )
                  {
                     X_DBF_ORDER *tag;
                     old->next = *replacement;
                     *replacement = old;
                     *position = NULL;
                     index->bag->initializing = 0;
                     for( tag = area->indexes; tag != NULL; tag = tag->next )
                        if( tag->bag == index->bag )
                           tag->broken = 1;
                     return status;
                  }
               }
               s_indexDrop( &old );
            }
            else
               replacement = &old->next;
         }
         position = replacement;
         published = 1;
         status = s_sync( index );
      }
      if( status == X_DBF_OK && index->bag->structural )
         status = x_dbfFileIndexFlag( area, 1 );
      x_dbfOrderUnlock( &guard );
      index->bag->initializing = 0;
      if( status != X_DBF_OK )
      {
         if( published )
         {
            area->order = index;
            index = NULL;
         }
         else
            *position = NULL;
         return status;
      }
      if( area->shared && !bind )
      {
         x_fileClose( &index->bag->file );
         x_gcUnlock( index->bag->file.u.pPointer );
         x_itemClear( &index->bag->file );
         handle =
            x_fileOpen( index->bag->path, FO_READWRITE | IIF( index->bag->exclusive, FO_EXCLUSIVE, FO_DENYNONE ) );
         if( handle == NULL )
         {
            *position = NULL;
            return X_DBF_OPEN_ERROR;
         }
         x_itemPutPtrGC( &index->bag->file, handle );
         x_gcLock( handle );
      }
   }
   for( position = &area->indexes; *position != NULL; )
   {
      X_DBF_ORDER *old = *position;
      if( old != index && ( ( old->bag == index->bag && ( ( !index->bag->cdx && !index->bag->compound ) ||
                                                          strcmp( old->diskName, index->diskName ) == 0 ) ) ||
                            ( !options->additive && old->bag != index->bag ) ) )
      {
         *position = old->next;
         if( area->order == old )
            area->order = NULL;
         s_indexDrop( &old );
      }
      else
         position = &old->next;
   }
   area->order = index;
   index = NULL;
   return x_dbfAreaGoTop( area, x_setDeleted );
}

X_DBF_STATUS x_dbfOrderOpen( DBF area, const char *path, const X_ITEM *bindings )
{
   struct _DBF     staged __attribute__( ( cleanup( s_listDrop ) ) ) = { 0 };
   X_DBF_INDEX_BAG lookup = { 0 };
   X_DBF_TEXT      resolved __attribute__( ( cleanup( s_bytesDrop ) ) ) = { NULL, 0 };
   X_DBF_ORDER    *index, **tail, **next = &staged.indexes;
   X_DBF_STATUS    status;
   int64_t         tag, count;
   void           *handle;
   if( area->indexBusy || area->indexLockDepth != 0 )
      return X_DBF_INVALID_STATE;
   if( strcmp( area->driver->name, "DBFNTX" ) != 0 && strcmp( area->driver->name, "DBFCDX" ) != 0 )
      return X_DBF_UNSUPPORTED_OPERATION;
   status = x_dbfAreaPrepare( area );
   if( status == X_DBF_OK )
      status = x_dbfAreaFlush( area );
   if( status != X_DBF_OK )
      return status;
   lookup.cdx = strcmp( area->driver->name, "DBFCDX" ) == 0;
   status = s_bagPath( area, path, &lookup );
   resolved.data = lookup.path;
   if( status != X_DBF_OK )
      return status;
   for( index = area->indexes; index != NULL; index = index->next )
   {
      if( x_stricmp( index->bag->path, lookup.path ) == 0 )
      {
         if( bindings != NULL && bindings->nType != X_IT_NIL )
            return X_DBF_INVALID_STATE;
         if( area->order == NULL )
         {
            area->order = index;
            return x_dbfAreaGoTop( area, x_setDeleted );
         }
         return X_DBF_OK;
      }
   }
   if( bindings == NULL || bindings->nType == X_IT_NIL )
      return X_DBF_INDEX_EXPRESSION_REQUIRED;
   if( bindings->nType != X_IT_ARRAY || ( count = x_arrayLen( bindings ) ) == 0 )
      return X_DBF_INVALID_ARGUMENT;
   for( tag = 1; tag <= count; ++tag )
   {
      const X_ITEM *row = x_arrayGetItemPtr( bindings, ( size_t )tag );
      const X_ITEM *item;
      size_t        column;
      if( row->nType != X_IT_ARRAY || x_arrayLen( row ) != 5 )
         return X_DBF_INVALID_ARGUMENT;
      for( column = 1; column <= 5; column += 3 )
      {
         item = x_arrayGetItemPtr( row, column );
         if( item->nType != X_IT_NIL &&
             ( item->nType != X_IT_STRING || memchr( item->u.string.data, 0, item->u.string.len ) != NULL ) )
            return X_DBF_INVALID_ARGUMENT;
      }
      item = x_arrayGetItemPtr( row, 2 );
      if( item->nType != X_IT_STRING || item->u.string.len == 0 ||
          memchr( item->u.string.data, 0, item->u.string.len ) != NULL ||
          x_arrayGetItemPtr( row, 3 )->nType != X_IT_BLOCK )
         return X_DBF_INVALID_ARGUMENT;
      item = x_arrayGetItemPtr( row, 4 );
      if( ( item->nType == X_IT_STRING && item->u.string.len != 0 ) !=
             ( x_arrayGetItemPtr( row, 5 )->nType == X_IT_BLOCK ) ||
          ( x_arrayGetItemPtr( row, 5 )->nType != X_IT_NIL && x_arrayGetItemPtr( row, 5 )->nType != X_IT_BLOCK ) )
         return X_DBF_INVALID_ARGUMENT;
   }

   if( !lookup.cdx && count > 63 )
      return X_DBF_UNSUPPORTED_FORMAT;
   for( tag = 1; tag <= count; ++tag )
   {
      const X_ITEM       *row = x_arrayGetItemPtr( bindings, ( size_t )tag );
      const X_ITEM       *name = x_arrayGetItemPtr( row, 1 );
      const X_ITEM       *condition = x_arrayGetItemPtr( row, 4 );
      X_DBF_INDEX_OPTIONS options = { 0 };
      X_DBF_ORDER        *defined __attribute__( ( cleanup( s_indexDrop ) ) ) = NULL;
      options.path = path;
      options.name = IIF( name->nType == X_IT_STRING, name->u.string.data, NULL );
      options.expression = x_arrayGetCPtr( row, 2 );
      options.block = x_arrayGetItemPtr( row, 3 );
      options.condition = IIF( condition->nType == X_IT_STRING, condition->u.string.data, NULL );
      options.forBlock = IIF( x_arrayGetItemPtr( row, 5 )->nType == X_IT_BLOCK, x_arrayGetItemPtr( row, 5 ), NULL );
      options.existing = 1;
      status = s_indexDefine( area, &options, &defined );
      if( status != X_DBF_OK )
         return status;
      for( index = staged.indexes; index != NULL; index = index->next )
      {
         if( strcmp( index->diskName, defined->diskName ) == 0 )
            return X_DBF_DUPLICATE_NAME;
      }
      if( staged.indexes != NULL )
      {
         s_bagDrop( &defined->bag );
         defined->bag = staged.indexes->bag;
         ++defined->bag->references;
      }
      *next = defined;
      next = &defined->next;
      defined = NULL;
   }
   handle = x_fileOpen( staged.indexes->bag->path,
                        IIF( area->readOnly, FO_READ, FO_READWRITE ) | IIF( area->shared, FO_DENYNONE, FO_EXCLUSIVE ) );
   if( handle == NULL )
      return X_DBF_OPEN_ERROR;
   x_itemPutPtrGC( &staged.indexes->bag->file, handle );
   x_gcLock( handle );
   for( tail = &area->indexes; *tail != NULL; tail = &( *tail )->next )
   {
   }
   *tail = staged.indexes;
   {
      X_DBF_INDEX_LOCK guard __attribute__( ( cleanup( x_dbfOrderUnlock ) ) ) = { 0 };
      status = x_dbfOrderLock( area, 0, &guard );
      if( status == X_DBF_OK && staged.indexes->bag->unbound != 0 )
         status = X_DBF_INDEX_EXPRESSION_REQUIRED;
      x_dbfOrderUnlock( &guard );
   }
   *tail = NULL;
   if( status != X_DBF_OK )
      return status;
   if( lookup.cdx )
   {
      X_DBF_ORDER *sorted = NULL;
      while( staged.indexes != NULL )
      {
         X_DBF_ORDER **position = &sorted;
         index = staged.indexes;
         staged.indexes = index->next;
         while( *position != NULL && strcmp( ( *position )->diskName, index->diskName ) < 0 )
            position = &( *position )->next;
         index->next = *position;
         *position = index;
      }
      staged.indexes = sorted;
   }
   for( index = staged.indexes; index != NULL; index = index->next )
      index->opening = 0;
   *tail = staged.indexes;
   index = IIF( area->order == NULL, staged.indexes, NULL );
   if( index != NULL )
      area->order = index;
   staged.indexes = NULL;
   return IIF( index != NULL, x_dbfAreaGoTop( area, x_setDeleted ), X_DBF_OK );
}

void x_dbfOrderDispose( DBF area )
{
   s_listDrop( area );
   x_itemClear( &area->indexCondition );
   x_itemClear( &area->indexConditionText );
   x_itemClear( &area->indexBuild.whileBlock );
   x_itemClear( &area->indexBuild.evalBlock );
}

X_DBF_STATUS x_dbfOrderClear( DBF area )
{
   X_DBF_STATUS status;
   if( area->indexBusy || area->indexLockDepth != 0 )
      return X_DBF_INVALID_STATE;
   status = x_dbfAreaFlush( area );
   if( status == X_DBF_OK )
      status = x_dbfOrderCommit( area );
   if( status == X_DBF_OK )
      s_listDrop( area );
   return status;
}

X_DBF_STATUS x_dbfOrderRebuild( DBF area )
{
   X_DBF_INDEX_LOCK lock __attribute__( ( cleanup( x_dbfOrderUnlock ) ) ) = { 0 };
   X_DBF_ORDER     *index;
   X_DBF_STATUS     status;
   if( area->indexBusy || area->indexLockDepth != 0 )
      return X_DBF_INVALID_STATE;
   if( area->shared )
   {
      X_ITEM locked __attribute__( ( cleanup( x_itemClear ) ) ) = X_ITEM_NIL_INIT;
      status = x_dbfAreaInfo( area, DBI_ISFLOCK, &locked );
      if( status != X_DBF_OK )
         return status;
      if( !locked.u.logical )
         return X_DBF_SHARED_OPERATION;
   }
   if( area->readOnly )
      return X_DBF_READ_ONLY;
   status = x_dbfAreaPrepare( area );
   if( status != X_DBF_OK )
      return status;
   if( area->state.dirty )
   {
      status = x_dbfAreaFlush( area );
      if( status != X_DBF_OK )
         return status;
   }
   status = x_dbfOrderLock( area, 1, &lock );
   if( status != X_DBF_OK )
      return status;
   area->indexReindex = 1;
   for( index = area->indexes; index != NULL; index = index->next )
   {
      index->broken = 1;
      status = s_build( index, NULL );
      if( status != X_DBF_OK )
      {
         area->indexReindex = 0;
         return status;
      }
   }
   area->indexReindex = 0;
   for( index = area->indexes; index != NULL; index = index->next )
   {
      status = s_sync( index );
      if( status != X_DBF_OK )
         return status;
   }
   x_dbfOrderUnlock( &lock );
   return x_dbfAreaGoTop( area, x_setDeleted );
}

X_DBF_STATUS x_dbfOrderPrepare( DBF area )
{
   X_DBF_ORDER      *index;
   X_DBF_STATUS      status;
   X_DBF_ORDER_GUARD guard __attribute__( ( cleanup( s_restore ) ) ) = { area, area->state, NULL };
   if( area->indexes == NULL )
      return X_DBF_OK;
   if( area->state.record > UINT32_MAX )
      return X_DBF_TEXT_OVERFLOW;
   guard.record = ( char * )x_xgrab( area->schema->recordLength );
   memcpy( guard.record, area->record, area->schema->recordLength );
   area->indexBusy = 1;
   for( index = area->indexes; index != NULL; index = index->next )
   {
      if( index->broken )
         return X_DBF_RECOVERY_REQUIRED;
      if( index->custom )
         continue;
      status = s_evaluate( index, &index->newKey, &index->newIncluded );
      if( status != X_DBF_OK )
         return status;
   }
   if( area->indexNewRecord )
   {
      for( index = area->indexes; index != NULL; index = index->next )
         index->oldIncluded = 0;
      return X_DBF_OK;
   }
   status = area->driver->operations.read( area, area->state.record, area->record, area->schema->recordLength );
   if( status != X_DBF_OK )
      return status;
   area->state.deleted = area->record[0] == '*';
   s_invalidateRelations( area );
   for( index = area->indexes; index != NULL; index = index->next )
   {
      if( index->custom )
         continue;
      status = s_evaluate( index, &index->oldKey, &index->oldIncluded );
      if( status != X_DBF_OK )
         return status;
   }
   return X_DBF_OK;
}

X_DBF_STATUS x_dbfOrderApply( DBF area )
{
   X_DBF_ORDER *index;
   X_DBF_STATUS first = X_DBF_OK;
   for( index = area->indexes; index != NULL; index = index->next )
   {
      if( index->custom )
         continue;
      const X_DBF_INDEX_KEY *removed = IIF( index->oldIncluded, &index->oldKey, NULL );
      const X_DBF_INDEX_KEY *added = IIF( index->newIncluded, &index->newKey, NULL );
      X_DBF_STATUS           status =
         IIF( index->bag->cdx, x_dbfCdxChange( index, removed, added ), x_dbfNtxChange( index, removed, added ) );
      if( status == X_DBF_OK && index->bag->compound )
         status = s_sync( index );
      if( status != X_DBF_OK )
         index->broken = 1;
      if( first == X_DBF_OK )
         first = status;
   }
   for( index = area->indexes; index != NULL; index = index->next )
   {
      X_DBF_STATUS status = s_sync( index );
      if( first == X_DBF_OK )
         first = status;
   }
   return first;
}

static X_DBF_STATUS s_find( X_DBF_ORDER *index, const X_DBF_INDEX_KEY *key, size_t length, int records, int strict,
                            int backwards, X_DBF_INDEX_CURSOR *cursor )
{
   backwards = backwards != index->reversed;
   return IIF( index->bag->cdx, x_dbfCdxFind( index, key, length, records, strict, backwards, cursor ),
               x_dbfNtxFind( index, key, length, records, strict, backwards, cursor ) );
}

static X_DBF_STATUS s_step( X_DBF_ORDER *index, X_DBF_INDEX_CURSOR *cursor, int backwards )
{
   X_DBF_INDEX_KEY previous = cursor->key;
   int             wasValid = cursor->valid;
   X_DBF_STATUS    status;
   backwards = backwards != index->reversed;
   do
   {
      status =
         IIF( index->bag->cdx, x_dbfCdxStep( index, cursor, backwards ), x_dbfNtxStep( index, cursor, backwards ) );
   } while( status == X_DBF_OK && cursor->valid && wasValid && index->userUnique &&
            x_dbfOrderCompare( index, &cursor->key, &previous, index->length, 0 ) == 0 );
   return status;
}

static int s_scope( const X_DBF_ORDER *index, const X_DBF_INDEX_KEY *key )
{
   if( index->scopes[0].nType != X_IT_NIL &&
       x_dbfOrderCompare( index, key, &index->scopeKeys[0], index->scopeLengths[0], 0 ) *
             IIF( index->reversed, -1, 1 ) <
          0 )
      return -1;
   if( index->scopes[1].nType != X_IT_NIL &&
       x_dbfOrderCompare( index, key, &index->scopeKeys[1], index->scopeLengths[1], 0 ) *
             IIF( index->reversed, -1, 1 ) >
          0 )
      return 1;
   return 0;
}

static X_DBF_STATUS s_boundary( X_DBF_ORDER *index, int bottom, X_DBF_INDEX_CURSOR *cursor )
{
   const X_DBF_INDEX_KEY *key = IIF( index->scopes[bottom].nType != X_IT_NIL, &index->scopeKeys[bottom], NULL );
   return s_find( index, key, index->scopeLengths[bottom], 0, 0, bottom, cursor );
}

static X_DBF_STATUS s_current( X_DBF_ORDER *index, X_DBF_INDEX_CURSOR *cursor )
{
   DBF             area = index->area;
   X_DBF_INDEX_KEY key;
   X_DBF_STATUS    status;
   if( index->cursor.keyValid && index->cursor.key.record == area->state.record )
   {
      if( index->cursor.valid && index->cursor.version == index->bag->version )
      {
         *cursor = index->cursor;
         return X_DBF_OK;
      }
      key = index->cursor.key;
      status = s_find( index, &key, index->length, 1, 0, 0, cursor );
      if( status != X_DBF_OK )
         return status;
      if( cursor->valid && x_dbfOrderCompare( index, &cursor->key, &key, index->length, 1 ) == 0 )
      {
         index->cursor = *cursor;
         return X_DBF_OK;
      }
      if( area->shared )
      {
         status = x_dbfAreaGoTo( area, area->state.record );
         if( status != X_DBF_OK )
            return status;
      }
   }
   {
      X_DBF_ORDER_GUARD guard __attribute__( ( cleanup( s_restore ) ) ) = { area, area->state, NULL };
      X_ITEM            value __attribute__( ( cleanup( x_itemClear ) ) ) = X_ITEM_NIL_INIT;
      size_t            length;
      area->indexBusy = 1;
      status = s_eval( area, &index->block, &value );
      if( status == X_DBF_OK )
         status = s_valueKey( index, &value, &key, &length );
   }
   if( status != X_DBF_OK )
      return status;
   key.record = ( uint32_t )area->state.record;
   status = s_find( index, &key, index->length, 1, 0, 0, cursor );
   if( status != X_DBF_OK )
      return status;
   if( !cursor->valid || x_dbfOrderCompare( index, &cursor->key, &key, index->length, 1 ) != 0 )
   {
      if( index->template )
      {
         status = s_boundary( index, 0, cursor );
         if( status != X_DBF_OK )
            return status;
         while( cursor->valid && s_scope( index, &cursor->key ) == 0 )
         {
            if( cursor->key.record == area->state.record )
            {
               index->cursor = *cursor;
               return X_DBF_OK;
            }
            status = IIF( index->bag->cdx, x_dbfCdxStep( index, cursor, index->reversed ),
                          x_dbfNtxStep( index, cursor, index->reversed ) );
            if( status != X_DBF_OK )
               return status;
         }
      }
      cursor->key = key;
      cursor->keyValid = 1;
      cursor->valid = 0;
   }
   index->cursor = *cursor;
   return X_DBF_OK;
}

static X_DBF_STATUS s_position( DBF area, X_DBF_INDEX_CURSOR *cursor, int backwards, int skipDeleted, int *positioned )
{
   X_DBF_ORDER *index = area->order;
   X_DBF_STATUS status;
   *positioned = 0;
   while( cursor->valid && s_scope( index, &cursor->key ) == 0 )
   {
      int accepted;
      status = x_dbfAreaGoTo( area, cursor->key.record );
      if( status != X_DBF_OK )
         return status;
      {
         X_DBF_ORDER_GUARD guard __attribute__( ( cleanup( s_restore ) ) ) = { area, area->state, NULL };
         area->indexBusy = 1;
         status = x_dbfAreaAccept( area, skipDeleted, &accepted );
      }
      if( status != X_DBF_OK )
         return status;
      if( accepted )
      {
         index->cursor = *cursor;
         *positioned = 1;
         return X_DBF_OK;
      }
      status = s_step( index, cursor, backwards );
      if( status != X_DBF_OK )
         return status;
   }
   return X_DBF_OK;
}

static void s_cursorRestore( X_DBF_CURSOR_GUARD *guard )
{
   if( guard->index != NULL )
      guard->index->cursor = guard->cursor;
}

static X_DBF_STATUS s_measure( DBF area, X_DBF_ORDER *index, const X_DBF_INDEX_KEY *last, int filters, uint64_t *count )
{
   X_DBF_CURSOR_GUARD cursorGuard __attribute__( ( cleanup( s_cursorRestore ) ) ) = { NULL, { 0 } };
   X_DBF_ORDER_GUARD  guard __attribute__( ( cleanup( s_restore ) ) ) = { area, area->state, NULL };
   X_DBF_INDEX_CURSOR cursor = { 0 };
   X_DBF_STATUS       status;
   uint64_t           records = 0;
   int                matched = last == NULL;
   int                checkFilter = filters && area->filter.nType == X_IT_BLOCK;
   *count = 0;
   if( index != NULL )
      status = s_boundary( index, 0, &cursor );
   else
   {
      status = x_dbfAreaRecordCount( area, &records );
      cursor.key.record = 1;
      cursor.valid = records != 0;
   }
   if( status != X_DBF_OK )
      return status;
   if( checkFilter )
   {
      guard.record = ( char * )x_xgrab( area->schema->recordLength );
      memcpy( guard.record, area->record, area->schema->recordLength );
      if( area->order != NULL )
      {
         cursorGuard.index = area->order;
         cursorGuard.cursor = area->order->cursor;
      }
   }
   while( cursor.valid && ( index == NULL || s_scope( index, &cursor.key ) == 0 ) )
   {
      int accepted = 1;
      if( checkFilter )
      {
         status = x_dbfAreaGoTo( area, cursor.key.record );
         if( status == X_DBF_OK )
         {
            X_DBF_ORDER_GUARD busy __attribute__( ( cleanup( s_restore ) ) ) = { area, area->state, NULL };
            area->indexBusy = 1;
            status = x_dbfAreaAccept( area, x_setDeleted, &accepted );
         }
         if( status != X_DBF_OK )
            return status;
      }
      if( accepted )
         ++*count;
      if( last != NULL && IIF( index == NULL, cursor.key.record == last->record,
                               x_dbfOrderCompare( index, &cursor.key, last, index->length, 1 ) == 0 ) )
      {
         matched = 1;
         if( !accepted )
            *count = 0;
         break;
      }
      if( index != NULL )
      {
         status = s_step( index, &cursor, 0 );
         if( status != X_DBF_OK )
            return status;
      }
      else if( cursor.key.record == records )
         cursor.valid = 0;
      else
         ++cursor.key.record;
   }
   if( !matched )
      *count = 0;
   return X_DBF_OK;
}

/* Results without a filter are kept; a filter may depend on data outside the index */
static int s_cacheable( DBF area, int filters )
{
   return !( filters && area->filter.nType == X_IT_BLOCK );
}

/* OrdKeyCount(): kept until the index, its scope, direction or unique setting changes */
static X_DBF_STATUS s_keyCount( DBF area, X_DBF_ORDER *index, int filters, uint64_t *count )
{
   int          cacheable = s_cacheable( area, filters );
   X_DBF_STATUS status;

   if( cacheable && index->countValid && index->countVersion == index->bag->version &&
       index->countGeneration == index->generation )
   {
      *count = index->countCache;
      return X_DBF_OK;
   }
   status = s_measure( area, index, NULL, filters, count );
   if( status == X_DBF_OK && cacheable )
   {
      index->countCache = *count;
      index->countVersion = index->bag->version;
      index->countGeneration = index->generation;
      index->countValid = 1;
   }
   return status;
}

/* OrdKeyNo(): the last position is kept, and a key near it is counted from there, so moving a few
   records in a browse does not read the whole index; a distant key is counted from the top */
static X_DBF_STATUS s_keyNumber( DBF area, X_DBF_ORDER *index, const X_DBF_INDEX_KEY *key, int filters,
                                 uint64_t *number )
{
   X_DBF_INDEX_CURSOR cursor = { 0 };
   int                cacheable = s_cacheable( area, filters ), order;
   uint64_t           position, steps;
   X_DBF_STATUS       status;

   if( cacheable && index->positionValid && index->positionVersion == index->bag->version &&
       index->positionGeneration == index->generation )
   {
      order = x_dbfOrderCompare( index, key, &index->positionKey, index->length, 1 ) * IIF( index->reversed, -1, 1 );
      if( order == 0 )
      {
         *number = index->positionCache;
         return X_DBF_OK;
      }
      status = s_find( index, &index->positionKey, index->length, 1, 0, 0, &cursor );
      if( status != X_DBF_OK )
         return status;
      if( cursor.valid && x_dbfOrderCompare( index, &cursor.key, &index->positionKey, index->length, 1 ) == 0 )
      {
         /* Steps in the order of the index: forward when the key comes after the kept one */
         position = index->positionCache;
         for( steps = 0; steps < 4096; ++steps )
         {
            status = s_step( index, &cursor, order < 0 );
            if( status != X_DBF_OK )
               return status;
            if( !cursor.valid || s_scope( index, &cursor.key ) != 0 )
               break;
            position = IIF( order > 0, position + 1, position - 1 );
            if( x_dbfOrderCompare( index, &cursor.key, key, index->length, 1 ) == 0 )
            {
               *number = position;
               index->positionKey = *key;
               index->positionCache = position;
               return X_DBF_OK;
            }
         }
      }
   }
   status = s_measure( area, index, key, filters, number );
   index->positionValid = 0;
   if( status == X_DBF_OK && cacheable && *number != 0 )
   {
      index->positionKey = *key;
      index->positionCache = *number;
      index->positionVersion = index->bag->version;
      index->positionGeneration = index->generation;
      index->positionValid = 1;
   }
   return status;
}

X_DBF_STATUS x_dbfOrderBoundary( DBF area, int bottom, int skipDeleted )
{
   X_DBF_INDEX_LOCK   lock __attribute__( ( cleanup( x_dbfOrderUnlock ) ) ) = { 0 };
   X_DBF_ORDER       *index = area->order;
   X_DBF_INDEX_CURSOR cursor = { 0 };
   int                positioned, backwards = bottom;
   X_DBF_STATUS       status = x_dbfAreaFlush( area );
   if( status != X_DBF_OK )
      return status;
   status = x_dbfOrderLock( area, 0, &lock );
   if( status != X_DBF_OK )
      return status;
   if( index->broken )
      return X_DBF_RECOVERY_REQUIRED;
   status = s_boundary( index, bottom, &cursor );
   if( status == X_DBF_OK )
      status = s_position( area, &cursor, backwards, skipDeleted, &positioned );
   if( status != X_DBF_OK || positioned )
      return status;
   return x_dbfAreaGoTo( area, 0 );
}

X_DBF_STATUS x_dbfOrderSkip( DBF area, int64_t records, int skipDeleted )
{
   X_DBF_INDEX_LOCK   lock __attribute__( ( cleanup( x_dbfOrderUnlock ) ) ) = { 0 };
   X_DBF_ORDER       *index = area->order;
   X_DBF_INDEX_CURSOR cursor = { 0 };
   uint64_t           remaining;
   int                forward = records > 0, backwards = records < 0;
   int                positioned;
   X_DBF_STATUS       status;
   if( forward && area->state.eof )
   {
      area->state.bof = 0;
      return X_DBF_OK;
   }
   status = x_dbfAreaFlush( area );
   if( status != X_DBF_OK )
      return status;
   status = x_dbfOrderLock( area, 0, &lock );
   if( status != X_DBF_OK )
      return status;
   if( index->broken )
      return X_DBF_RECOVERY_REQUIRED;
   if( area->state.eof )
      status = s_boundary( index, backwards, &cursor );
   else
   {
      status = s_current( index, &cursor );
      if( status == X_DBF_OK )
         status = s_step( index, &cursor, backwards );
   }
   if( status != X_DBF_OK )
      return status;
   if( cursor.valid )
   {
      int scope = s_scope( index, &cursor.key );
      if( ( !backwards && scope < 0 ) || ( backwards && scope > 0 ) )
      {
         status = s_boundary( index, backwards, &cursor );
         if( status != X_DBF_OK )
            return status;
      }
   }
   remaining = IIF( forward, ( uint64_t )records, ( uint64_t )( -( records + 1 ) ) + 1 );
   while( remaining != 0 )
   {
      status = s_position( area, &cursor, backwards, skipDeleted, &positioned );
      if( status != X_DBF_OK )
         return status;
      if( !positioned )
      {
         if( forward )
         {
            status = x_dbfAreaGoTo( area, 0 );
            if( status == X_DBF_OK )
               area->state.bof = 0;
         }
         else
         {
            status = x_dbfOrderBoundary( area, 0, skipDeleted );
            if( status == X_DBF_OK )
            {
               area->state.bof = 1;
               area->state.eof = 0;
            }
         }
         return status;
      }
      if( --remaining == 0 )
         break;
      status = s_step( index, &cursor, backwards );
      if( status != X_DBF_OK )
         return status;
   }
   return X_DBF_OK;
}

X_DBF_STATUS x_dbfOrderSeek( DBF area, const X_ITEM *value, int soft, int last, int skipDeleted )
{
   X_DBF_INDEX_LOCK   lock __attribute__( ( cleanup( x_dbfOrderUnlock ) ) ) = { 0 };
   X_DBF_ORDER       *index;
   X_DBF_INDEX_KEY    key;
   X_DBF_INDEX_CURSOR cursor = { 0 };
   size_t             length;
   int                positioned, backwards;
   X_DBF_STATUS       status;
   if( area->pendingRelation != NULL && area->pendingRelation->scoped )
   {
      status = x_dbfAreaPrepare( area );
      if( status != X_DBF_OK )
         return status;
   }
   index = area->order;
   if( index == NULL )
      return X_DBF_ORDER_NOT_FOUND;
   status = x_dbfAreaFlush( area );
   if( status != X_DBF_OK )
      return status;
   status = x_dbfOrderLock( area, 0, &lock );
   if( status != X_DBF_OK )
      return status;
   if( index->broken )
      return X_DBF_RECOVERY_REQUIRED;
   status = s_valueKey( index, value, &key, &length );
   if( status != X_DBF_OK )
      return status;
   backwards = last;
   status = s_find( index, &key, length, 0, 0, backwards, &cursor );
   if( status != X_DBF_OK )
      return status;
   if( cursor.valid )
   {
      int scope = s_scope( index, &cursor.key );
      if( ( !backwards && scope < 0 ) || ( backwards && scope > 0 ) )
      {
         status = s_boundary( index, backwards, &cursor );
         if( status != X_DBF_OK )
            return status;
      }
   }
   status = s_position( area, &cursor, backwards, skipDeleted, &positioned );
   if( status != X_DBF_OK )
      return status;
   area->state.found = positioned && x_dbfOrderCompare( index, &cursor.key, &key, length, 0 ) == 0;
   if( !positioned || ( !area->state.found && !soft ) )
   {
      status = x_dbfAreaGoTo( area, 0 );
      if( status == X_DBF_OK && area->state.record != 1 )
         area->state.bof = 0;
   }
   return status;
}

static X_DBF_ORDER *s_order( DBF area, const X_ITEM *selector, const char *bag )
{
   X_DBF_ORDER *index;
   int64_t      number = 0;
   int          current = selector == NULL || selector->nType == X_IT_NIL ||
                          ( X_IS_NUMERIC( selector ) &&
                            IIF( selector->nType == X_IT_INTEGER, selector->u.integer == 0, selector->u.dbl == 0 ) );
   if( current && bag == NULL )
      return area->order;
   if( selector != NULL && selector->nType == X_IT_INTEGER )
      number = selector->u.integer;
   else if( selector != NULL && selector->nType == X_IT_DOUBLE && isfinite( selector->u.dbl ) && selector->u.dbl > 0 &&
            selector->u.dbl < 0x1p63 )
      number = ( int64_t )selector->u.dbl;
   for( index = area->indexes; index != NULL; index = index->next )
   {
      if( bag != NULL && x_stricmp( bag, index->bag->path ) != 0 )
      {
         const char *base = s_baseName( index->bag->path );
         const char *extension = strrchr( base, '.' );
         size_t      length = IIF( extension != NULL, ( size_t )( extension - base ), strlen( base ) );
         if( s_baseName( bag ) != bag ||
             ( x_stricmp( bag, base ) != 0 && ( strlen( bag ) != length || x_strnicmp( bag, base, length ) != 0 ) ) )
            continue;
      }
      if( current || ( selector->nType == X_IT_STRING && x_stricmp( selector->u.string.data, index->name ) == 0 ) ||
          ( number > 0 && --number == 0 ) )
         return index;
   }
   return NULL;
}

X_DBF_STATUS x_dbfOrderFocus( DBF area, const X_ITEM *selector, const char *bag, X_ITEM *previous )
{
   X_DBF_ORDER *index;
   X_DBF_STATUS status;
   if( area->indexBusy )
      return X_DBF_INVALID_STATE;
   s_setText( previous, IIF( area->order != NULL, area->order->name, "" ) );
   if( selector == NULL || selector->nType == X_IT_NIL )
      return X_DBF_OK;
   index = IIF( X_IS_NUMERIC( selector ) &&
                   IIF( selector->nType == X_IT_INTEGER, selector->u.integer == 0, selector->u.dbl == 0 ),
                NULL, s_order( area, selector, bag ) );
   status = x_dbfAreaFlush( area );
   if( status == X_DBF_OK )
      area->order = index;
   return status;
}

X_DBF_STATUS x_dbfOrderDestroy( DBF area, const X_ITEM *selector, const char *bag )
{
   X_DBF_INDEX_LOCK lock __attribute__( ( cleanup( x_dbfOrderUnlock ) ) ) = { 0 };
   X_DBF_ORDER     *index, **position;
   X_DBF_TEXT       path __attribute__( ( cleanup( s_bytesDrop ) ) ) = { NULL, 0 };
   X_DBF_STATUS     status;
   if( area->indexBusy || area->indexLockDepth != 0 )
      return X_DBF_INVALID_STATE;
   index = s_order( area, selector, bag );
   if( index == NULL )
      return X_DBF_OK;
   status = x_dbfAreaFlush( area );
   if( status != X_DBF_OK )
      return status;
   if( area->readOnly )
      return X_DBF_READ_ONLY;
   if( area->shared )
   {
      X_ITEM locked __attribute__( ( cleanup( x_itemClear ) ) ) = X_ITEM_NIL_INIT;
      status = x_dbfAreaInfo( area, DBI_ISFLOCK, &locked );
      if( status != X_DBF_OK )
         return status;
      if( !locked.u.logical || ( !index->bag->cdx && !index->bag->compound ) || index->bag->references == 1 )
         return X_DBF_SHARED_OPERATION;
   }
   status = x_dbfOrderLock( area, 1, &lock );
   if( status != X_DBF_OK )
      return status;
   for( position = &area->indexes; *position != index; position = &( *position )->next )
   {
   }
   *position = index->next;
   if( area->order == index )
      area->order = NULL;
   if( ( index->bag->cdx || index->bag->compound ) && index->bag->references > 1 )
   {
      X_DBF_ORDER *remaining;
      for( remaining = area->indexes; remaining->bag != index->bag; remaining = remaining->next )
      {
      }
      if( index->bag->compound )
      {
         status = x_dbfNtxRelease( index, index->ntxRoot, 0 );
         if( status == X_DBF_OK )
            status = x_dbfNtxRelease( index, index->ntxHeader, 0 );
         if( status != X_DBF_OK )
         {
            X_DBF_ORDER *tag;
            index->next = *position;
            *position = index;
            for( tag = area->indexes; tag != NULL; tag = tag->next )
               if( tag->bag == index->bag )
                  tag->broken = 1;
            return status;
         }
      }
      s_indexDrop( &index );
      remaining->changed = 1;
      remaining->bag->cdxRebuild = 1;
      return s_sync( remaining );
   }
   path.data = x_strdup( index->bag->path );
   if( index->bag->structural )
   {
      status = x_dbfFileIndexFlag( area, 0 );
      if( status != X_DBF_OK )
      {
         index->next = *position;
         *position = index;
         return status;
      }
   }
   x_dbfOrderUnlock( &lock );
   s_indexDrop( &index );
   if( !x_fsDelete( path.data ) )
      return X_DBF_WRITE_ERROR;
   return X_DBF_OK;
}

static X_DBF_STATUS s_scopeValue( X_DBF_ORDER *index, int scope, const X_ITEM *value, int set, X_ITEM *result )
{
   X_DBF_INDEX_KEY key = { 0 };
   size_t          length;
   int             current, first = IIF( scope == 2, 0, scope ), last = IIF( scope == 2, 1, scope );
   X_DBF_STATUS    status;
   if( index == NULL )
   {
      x_itemClear( result );
      return X_DBF_OK;
   }
   if( index->area->indexBusy && set )
      return X_DBF_INVALID_STATE;
   if( scope != 2 )
      x_itemCopy( result, &index->scopes[scope] );
   else
      x_itemClear( result );
   if( set )
   {
      ++index->generation;
      if( value == NULL || value->nType == X_IT_NIL )
      {
         for( current = first; current <= last; ++current )
            x_itemClear( &index->scopes[current] );
      }
      else
      {
         status = s_valueKey( index, value, &key, &length );
         if( status != X_DBF_OK )
            return status;
         for( current = first; current <= last; ++current )
         {
            memcpy( &index->scopeKeys[current], &key, sizeof( uint32_t ) + index->length );
            index->scopeLengths[current] = length;
            x_itemCopy( &index->scopes[current], value );
         }
      }
   }
   return X_DBF_OK;
}

X_DBF_STATUS x_dbfOrderScope( DBF area, int scope, const X_ITEM *value, int set, X_ITEM *result )
{
   if( scope < 0 || scope > 2 )
      return X_DBF_INVALID_ARGUMENT;
   return s_scopeValue( area->order, scope, value, set || scope == 2, result );
}

static int s_bagNumber( DBF area, const X_DBF_INDEX_BAG *bag )
{
   X_DBF_ORDER *index, *previous;
   int          number = 0;
   for( index = area->indexes; index != NULL; index = index->next )
   {
      for( previous = area->indexes; previous != index; previous = previous->next )
      {
         if( previous->bag == index->bag )
            break;
      }
      if( previous != index )
         continue;
      ++number;
      if( index->bag == bag )
         return number;
   }
   return IIF( bag == NULL, number, 0 );
}

static void s_setLogical( X_ITEM *result, int value )
{
   x_itemClear( result );
   result->nType = X_IT_LOGICAL;
   result->u.logical = value != 0;
}

// Converts a stored index key to the xDev value of the key expression
static X_DBF_STATUS s_keyValue( const X_DBF_ORDER *index, const X_DBF_INDEX_KEY *key, X_ITEM *result )
{
   if( index->type == 'C' )
   {
      X_DBF_TEXT   text __attribute__( ( cleanup( s_bytesDrop ) ) ) = { NULL, 0 };
      X_DBF_STATUS status = x_dbfTextCopy( ( const char * )key->value, index->length, &text );
      if( status == X_DBF_OK )
         x_itemPutStringBuffer( result, x_strndup( text.data, text.length ), text.length );
      return status;
   }
   else if( index->type == 'L' )
   {
      x_itemClear( result );
      result->nType = X_IT_LOGICAL;
      result->u.logical = key->value[0] == 'T';
   }
   else if( index->bag->cdx && ( index->type == 'N' || index->type == 'D' ) )
   {
      uint64_t bits = 0;
      double   number;
      size_t   slot;
      for( slot = 0; slot < 8; ++slot )
         bits = ( bits << 8 ) | key->value[slot];
      bits = IIF( bits >> 63, bits ^ ( UINT64_C( 1 ) << 63 ), ~bits );
      memcpy( &number, &bits, sizeof( number ) );
      x_itemClear( result );
      if( index->type == 'D' )
      {
         result->nType = X_IT_DATE;
         result->u.integer = ( int64_t )number;
      }
      else
      {
         result->nType = X_IT_DOUBLE;
         result->u.dbl = number;
      }
   }
   else if( index->type == 'D' )
   {
      int year = ( key->value[0] - '0' ) * 1000 + ( key->value[1] - '0' ) * 100 + ( key->value[2] - '0' ) * 10 +
                 key->value[3] - '0';
      int month = ( key->value[4] - '0' ) * 10 + key->value[5] - '0';
      int day = ( key->value[6] - '0' ) * 10 + key->value[7] - '0';
      x_itemClear( result );
      result->nType = X_IT_DATE;
      result->u.integer = IIF( key->value[0] == ' ', 0, x_dateEncode( year, month, day ) );
   }
   else if( index->type == 'N' )
   {
      char    number[X_DBF_KEY_SIZE + 1];
      size_t  slot;
      int     negative = key->value[0] == 44;
      int64_t integer;
      memcpy( number, key->value, index->length );
      number[index->length] = '\0';
      if( negative )
      {
         for( slot = 0; slot < index->length; ++slot )
         {
            if( number[slot] != '.' )
               number[slot] = ( char )( '0' + 44 - ( unsigned char )number[slot] );
         }
         number[0] = '-';
      }
      errno = 0;
      integer = strtoll( number, NULL, 10 );
      if( index->decimals == 0 && errno != ERANGE )
         s_setInteger( result, integer );
      else
      {
         x_itemClear( result );
         result->nType = X_IT_DOUBLE;
         result->u.dbl = strtod( number, NULL );
      }
   }
   return X_DBF_OK;
}

// Pattern with * (any run of characters) and ? (one character), backtracking to the last *
static int s_wildMatch( const unsigned char *pattern, size_t patternLength, const unsigned char *text, size_t length )
{
   size_t p = 0, t = 0, star = SIZE_MAX, mark = 0;
   while( t < length )
   {
      if( p < patternLength && pattern[p] != '*' && ( pattern[p] == '?' || pattern[p] == text[t] ) )
      {
         ++p;
         ++t;
      }
      else if( p < patternLength && pattern[p] == '*' )
      {
         star = p++;
         mark = t;
      }
      else if( star != SIZE_MAX )
      {
         p = star + 1;
         t = ++mark;
      }
      else
         return 0;
   }
   while( p < patternLength && pattern[p] == '*' )
      ++p;
   return p == patternLength;
}

// DBOI_SKIPUNIQUE, SKIPWILD(BACK), SKIPEVAL(BACK) and FINDREC(CONT): move to the nearest matching key
static X_DBF_STATUS s_skipMatch( DBF area, X_DBF_ORDER *index, int info, const X_ITEM *value, X_ITEM *result )
{
   X_DBF_INDEX_LOCK   lock __attribute__( ( cleanup( x_dbfOrderUnlock ) ) ) = { 0 };
   X_DBF_TEXT         pattern __attribute__( ( cleanup( s_bytesDrop ) ) ) = { NULL, 0 };
   X_DBF_INDEX_CURSOR cursor = { 0 };
   X_DBF_INDEX_KEY    start = { 0 };
   uint64_t           record = 0;
   int                backwards = info == DBOI_SKIPWILDBACK || info == DBOI_SKIPEVALBACK;
   int                wild = info == DBOI_SKIPWILD || info == DBOI_SKIPWILDBACK;
   int                evaluate = info == DBOI_SKIPEVAL || info == DBOI_SKIPEVALBACK;
   int                findRecord = info == DBOI_FINDREC || info == DBOI_FINDRECCONT;
   int                hasStart = 0, found = 0;
   X_DBF_STATUS       status;

   s_setLogical( result, 0 );
   if( index == NULL )
      return X_DBF_OK;
   if( info == DBOI_SKIPUNIQUE && value != NULL && X_IS_NUMERIC( value ) )
      backwards = IIF( value->nType == X_IT_INTEGER, value->u.integer < 0, value->u.dbl < 0 );
   if( wild )
   {
      if( value == NULL || value->nType != X_IT_STRING || index->type != 'C' )
         return X_DBF_INVALID_ARGUMENT;
      status = x_dbfTextCopy( value->u.string.data, value->u.string.len, &pattern );
      if( status != X_DBF_OK )
         return status;
   }
   if( evaluate && ( value == NULL || value->nType != X_IT_BLOCK ) )
      return X_DBF_INVALID_ARGUMENT;
   if( findRecord )
   {
      if( value == NULL || !X_IS_NUMERIC( value ) )
         return X_DBF_INVALID_ARGUMENT;
      record = IIF( value->nType == X_IT_INTEGER, ( uint64_t )value->u.integer, ( uint64_t )value->u.dbl );
   }
   status = x_dbfAreaFlush( area );
   if( status == X_DBF_OK )
      status = x_dbfOrderLock( area, 0, &lock );
   if( status != X_DBF_OK )
      return status;
   if( index->broken )
      return X_DBF_RECOVERY_REQUIRED;
   if( info == DBOI_FINDREC )
      status = s_boundary( index, 0, &cursor );
   else if( area->state.eof )
   {
      if( !backwards )
         return X_DBF_OK;
      status = s_boundary( index, 1, &cursor );
   }
   else
   {
      status = s_current( index, &cursor );
      if( status == X_DBF_OK )
      {
         start = cursor.key;
         hasStart = cursor.valid;
         status = s_step( index, &cursor, backwards );
      }
   }
   if( status != X_DBF_OK )
      return status;
   if( cursor.valid )
   {
      int scope = s_scope( index, &cursor.key );
      if( ( !backwards && scope < 0 ) || ( backwards && scope > 0 ) )
      {
         status = s_boundary( index, backwards, &cursor );
         if( status != X_DBF_OK )
            return status;
      }
   }
   while( cursor.valid && s_scope( index, &cursor.key ) == 0 )
   {
      int matched = 1, accepted = 0;
      if( info == DBOI_SKIPUNIQUE )
         matched = !hasStart || x_dbfOrderCompare( index, &cursor.key, &start, index->length, 0 ) != 0;
      else if( wild )
      {
         size_t length = index->length;
         while( length != 0 && cursor.key.value[length - 1] == ' ' )
            --length;
         matched = s_wildMatch( ( const unsigned char * )pattern.data, pattern.length, cursor.key.value, length );
      }
      else if( findRecord )
         matched = cursor.key.record == record;
      if( matched )
      {
         status = x_dbfAreaGoTo( area, cursor.key.record );
         if( status != X_DBF_OK )
            return status;
         {
            X_DBF_ORDER_GUARD busy __attribute__( ( cleanup( s_restore ) ) ) = { area, area->state, NULL };
            area->indexBusy = 1;
            status = x_dbfAreaAccept( area, x_setDeleted, &accepted );
            if( status == X_DBF_OK && accepted && evaluate )
            {
               X_ITEM     key __attribute__( ( cleanup( x_itemClear ) ) ) = X_ITEM_NIL_INIT;
               X_ITEM     number __attribute__( ( cleanup( x_itemClear ) ) ) = X_ITEM_NIL_INIT;
               X_ITEM     evaluated __attribute__( ( cleanup( x_itemClear ) ) ) = X_ITEM_NIL_INIT;
               X_ARG      args[2] = { { &key, 0 }, { &number, 0 } };
               X_CALL_CTX call = { NULL, 0, &evaluated, NULL, NULL };
               status = s_keyValue( index, &cursor.key, &key );
               s_setInteger( &number, cursor.key.record );
               if( status == X_DBF_OK )
                  status = x_dbfAreaEvalBlockArgs( &call, area, value, 2, args, &evaluated );
               accepted = status == X_DBF_OK && evaluated.nType == X_IT_LOGICAL && evaluated.u.logical;
            }
         }
         if( status != X_DBF_OK )
            return status;
         if( accepted )
         {
            index->cursor = cursor;
            found = 1;
            break;
         }
      }
      status = s_step( index, &cursor, backwards );
      if( status != X_DBF_OK )
         return status;
   }
   if( !found )
   {
      if( backwards )
      {
         status = x_dbfOrderBoundary( area, 0, x_setDeleted );
         if( status == X_DBF_OK )
         {
            area->state.bof = 1;
            area->state.eof = 0;
         }
      }
      else
         status = x_dbfAreaGoTo( area, 0 );
      if( status != X_DBF_OK )
         return status;
   }
   s_setLogical( result, found );
   return X_DBF_OK;
}

X_DBF_STATUS x_dbfOrderInfo( DBF area, const X_ITEM *selector, const X_ITEM *bag, int info, const X_ITEM *value,
                             X_ITEM *result )
{
   X_DBF_INDEX_LOCK lock __attribute__( ( cleanup( x_dbfOrderUnlock ) ) ) = { 0 };
   X_DBF_ORDER     *index, *position;
   const char      *bagName = NULL;
   X_DBF_STATUS     status;
   X_DBF_INDEX_KEY  present;
   int              number = 0, member;
   int              setting = value != NULL && value->nType != X_IT_NIL;
   int filters = strcmp( area->driver->name, "DBFCDX" ) == 0 && ( info != DBOI_KEYCOUNTRAW && info != DBOI_KEYNORAW );

   if( bag != NULL && bag->nType == X_IT_STRING && bag->u.string.len != 0 )
      bagName = bag->u.string.data;
   else if( bag != NULL && X_IS_NUMERIC( bag ) )
   {
      int64_t ordinal = IIF( bag->nType == X_IT_INTEGER, bag->u.integer, ( int64_t )bag->u.dbl );
      if( ordinal > 0 )
      {
         for( position = area->indexes; position != NULL; position = position->next )
         {
            if( s_bagNumber( area, position->bag ) == ordinal )
            {
               bagName = position->bag->path;
               break;
            }
         }
         if( bagName == NULL )
            bagName = "";
      }
   }
   index = s_order( area, selector, bagName );
   x_itemClear( result );

   if( setting && info != DBOI_ISDESC && info != DBOI_UNIQUE && info != DBOI_SCOPETOP && info != DBOI_SCOPEBOTTOM &&
       info != DBOI_SCOPESET && info != DBOI_POSITION && info != DBOI_KEYNORAW && info != DBOI_CUSTOM &&
       info != DBOI_TEMPLATE && info != DBOI_MULTIKEY && info != DBOI_KEYADD && info != DBOI_KEYDELETE &&
       info != DBOI_READLOCK && info != DBOI_WRITELOCK && info != DBOI_SKIPUNIQUE && info != DBOI_SKIPWILD &&
       info != DBOI_SKIPWILDBACK && info != DBOI_SKIPEVAL && info != DBOI_SKIPEVALBACK && info != DBOI_FINDREC &&
       info != DBOI_FINDRECCONT && info != DBOI_RELKEYPOS )
      return X_DBF_UNSUPPORTED_OPERATION;
   if( setting &&
       ( info == DBOI_ISDESC || info == DBOI_UNIQUE || info == DBOI_CUSTOM || info == DBOI_TEMPLATE ||
         info == DBOI_MULTIKEY || info == DBOI_READLOCK || info == DBOI_WRITELOCK ) &&
       value->nType != X_IT_LOGICAL )
      return X_DBF_INVALID_ARGUMENT;

   if( area->indexBusy && ( setting || info == DBOI_KEYADD || info == DBOI_KEYDELETE ) )
      return X_DBF_INVALID_STATE;
   if( setting && ( info == DBOI_POSITION || info == DBOI_KEYNORAW || info == DBOI_RELKEYPOS ) &&
       !X_IS_NUMERIC( value ) )
      return X_DBF_INVALID_ARGUMENT;

   switch( info )
   {
      case DBOI_SKIPUNIQUE:
      case DBOI_SKIPWILD:
      case DBOI_SKIPWILDBACK:
      case DBOI_SKIPEVAL:
      case DBOI_SKIPEVALBACK:
      case DBOI_FINDREC:
      case DBOI_FINDRECCONT: return s_skipMatch( area, index, info, value, result );
      case DBOI_RESETPOS:
         if( index != NULL )
            index->cursor.valid = 0;
         s_setLogical( result, 1 );
         return X_DBF_OK;
      case DBOI_RELKEYPOS:
      {
         // Position in the order as a fraction from 0 to 1, e.g. for a browse scroll bar
         X_ITEM count __attribute__( ( cleanup( x_itemClear ) ) ) = X_ITEM_NIL_INIT;
         X_ITEM position __attribute__( ( cleanup( x_itemClear ) ) ) = X_ITEM_NIL_INIT;
         double keys;
         status = x_dbfOrderInfo( area, selector, bag, DBOI_KEYCOUNT, NULL, &count );
         if( status != X_DBF_OK )
            return status;
         keys = ( double )x_itemGetNInt( &count );
         if( setting )
         {
            X_ITEM target __attribute__( ( cleanup( x_itemClear ) ) ) = X_ITEM_NIL_INIT;
            double requested = IIF( value->nType == X_IT_DOUBLE, value->u.dbl, ( double )value->u.integer );
            if( !isfinite( requested ) )
               return X_DBF_INVALID_ARGUMENT;
            requested = IIF( requested < 0, 0, IIF( requested > 1, 1, requested ) );
            s_setInteger( &target, IIF( keys < 1, 0, ( int64_t )floor( requested * ( keys - 1 ) + 0.5 ) + 1 ) );
            return x_dbfOrderInfo( area, selector, bag, DBOI_POSITION, &target, result );
         }
         status = x_dbfOrderInfo( area, selector, bag, DBOI_POSITION, NULL, &position );
         if( status != X_DBF_OK )
            return status;
         x_itemClear( result );
         result->nType = X_IT_DOUBLE;
         if( x_itemGetNInt( &position ) == 0 )
            result->u.dbl = IIF( area->state.eof && keys > 0, 1.0, 0.0 );
         else
            result->u.dbl = IIF( keys > 1, ( ( double )x_itemGetNInt( &position ) - 1 ) / ( keys - 1 ), 0.0 );
         return X_DBF_OK;
      }
      case DBOI_I_BAGNAME:
         s_setText( result, IIF( area->indexBuilding != NULL, area->indexBuilding->bag->path, "" ) );
         return X_DBF_OK;
      case DBOI_I_TAGNAME:
         s_setText( result, IIF( area->indexBuilding != NULL, area->indexBuilding->name, "" ) );
         return X_DBF_OK;
      case DBOI_KEYSINCLUDED: s_setInteger( result, ( int64_t )area->indexKeysIncluded ); return X_DBF_OK;
      case DBOI_ISREINDEX: s_setLogical( result, area->indexReindex && area->indexBuilding != NULL ); return X_DBF_OK;
      case DBOI_EVALSTEP:
         s_setInteger( result,
                       ( int64_t )IIF( area->indexBuilding != NULL, area->indexEvalStep, area->indexBuild.step ) );
         return X_DBF_OK;
      case DBOI_AUTOOPEN: return x_dbfDriverInfo( area->driver, RDDI_AUTOOPEN, result );
      case DBOI_AUTOORDER: return x_dbfDriverInfo( area->driver, RDDI_AUTOORDER, result );
      case DBOI_AUTOSHARE: return x_dbfDriverInfo( area->driver, RDDI_AUTOSHARE, result );
      case DBOI_STRICTREAD: return x_dbfDriverInfo( area->driver, RDDI_STRICTREAD, result );
      case DBOI_READLOCK:
      case DBOI_WRITELOCK:
      {
         size_t *depth;
         int     write = info == DBOI_WRITELOCK;
         s_setLogical( result, 0 );
         if( index == NULL )
            return X_DBF_OK;
         depth = IIF( write, &index->bag->writeLocks, &index->bag->readLocks );
         if( setting && value->u.logical )
         {
            if( write && area->readOnly )
               return X_DBF_READ_ONLY;
            status = x_dbfAreaFlush( area );
            if( status != X_DBF_OK )
               return status;
            status = s_lockBags( area, write, &lock, index->bag );
            if( status == X_DBF_LOCK_FAILED )
               return X_DBF_OK;
            if( status != X_DBF_OK )
               return status;
            ++*depth;
            ++index->bag->lockDepth;
         }
         else if( setting && *depth != 0 )
         {
            --*depth;
            status = s_bagUnlock( index->bag );
            if( status != X_DBF_OK )
               return status;
         }
         s_setLogical( result, IIF( setting && !index->bag->cdx, 1, *depth != 0 ) );
         return X_DBF_OK;
      }
      case DBOI_KEYADD:
      case DBOI_KEYDELETE:
      {
         X_ITEM             evaluated __attribute__( ( cleanup( x_itemClear ) ) ) = X_ITEM_NIL_INIT;
         X_DBF_INDEX_CURSOR cursor = { 0 };
         X_DBF_INDEX_KEY    key;
         int                included = 1, exists, adding = info == DBOI_KEYADD;
         size_t             length;
         s_setLogical( result, 0 );
         if( index == NULL )
            return X_DBF_OK;
         if( !index->custom )
            return X_DBF_UNSUPPORTED_OPERATION;
         if( area->readOnly )
            return X_DBF_READ_ONLY;
         status = x_dbfAreaFlush( area );
         if( status == X_DBF_OK )
            status = x_dbfOrderLock( area, 1, &lock );
         if( status != X_DBF_OK )
            return status;
         if( area->state.eof )
            return X_DBF_OK;
         {
            X_DBF_ORDER_GUARD busy __attribute__( ( cleanup( s_restore ) ) ) = { area, area->state, NULL };
            area->indexBusy = 1;
            if( index->forBlock.nType == X_IT_BLOCK )
            {
               status = s_eval( area, &index->forBlock, &evaluated );
               if( status != X_DBF_OK )
                  return status;
               if( evaluated.nType != X_IT_LOGICAL )
                  return X_DBF_INVALID_INDEX_KEY;
               included = evaluated.u.logical;
            }
            if( !included )
               return X_DBF_OK;
            if( index->template && setting )
               status = s_valueKey( index, value, &key, &length );
            else if( adding )
            {
               status = s_eval( area, &index->block, &evaluated );
               if( status == X_DBF_OK )
                  status = s_valueKey( index, &evaluated, &key, &length );
            }
         }
         if( !adding && !( index->template && setting ) )
         {
            status = s_current( index, &cursor );
            if( status == X_DBF_OK )
               key = cursor.key;
         }
         if( status != X_DBF_OK )
            return status;
         if( area->state.record > UINT32_MAX )
            return X_DBF_TEXT_OVERFLOW;
         key.record = ( uint32_t )area->state.record;
         status = s_find( index, &key, index->length, IIF( adding, !index->unique, 1 ), 0, 0, &cursor );
         if( status != X_DBF_OK )
            return status;
         exists = cursor.valid &&
                  x_dbfOrderCompare( index, &cursor.key, &key, index->length, IIF( adding, !index->unique, 1 ) ) == 0;
         if( IIF( adding, exists && !( index->multiKey && !index->unique ), !exists ) )
            return X_DBF_OK;
         status = IIF( index->bag->cdx, x_dbfCdxChange( index, IIF( adding, NULL, &key ), IIF( adding, &key, NULL ) ),
                       x_dbfNtxChange( index, IIF( adding, NULL, &key ), IIF( adding, &key, NULL ) ) );
         if( status == X_DBF_OK )
            status = s_sync( index );
         if( status != X_DBF_OK )
         {
            index->broken = 1;
            return status;
         }
         s_setLogical( result, 1 );
         return X_DBF_OK;
      }
      case DBOI_BAGEXT: return x_dbfDriverInfo( area->driver, RDDI_ORDBAGEXT, result );
      case DBOI_ORDERCOUNT:
      {
         X_DBF_ORDER *first = IIF( bagName != NULL, s_order( area, NULL, bagName ), NULL );
         for( position = area->indexes; position != NULL; position = position->next )
         {
            if( bagName == NULL || ( first != NULL && position->bag == first->bag ) )
               ++number;
         }
         s_setInteger( result, number );
         return X_DBF_OK;
      }
      case DBOI_BAGCOUNT: s_setInteger( result, s_bagNumber( area, NULL ) ); return X_DBF_OK;
      case DBOI_BAGNUMBER:
         s_setInteger( result, IIF( index != NULL, s_bagNumber( area, index->bag ), 0 ) );
         return X_DBF_OK;
      case DBOI_BAGORDER:
         if( index != NULL )
         {
            for( position = area->indexes; position->bag != index->bag; position = position->next )
               ++number;
            ++number;
         }
         s_setInteger( result, number );
         return X_DBF_OK;
      case DBOI_SCOPETOP:
      case DBOI_SCOPEBOTTOM: return s_scopeValue( index, info == DBOI_SCOPEBOTTOM, value, value != NULL, result );
      case DBOI_SCOPETOPCLEAR:
      case DBOI_SCOPEBOTTOMCLEAR: return s_scopeValue( index, info == DBOI_SCOPEBOTTOMCLEAR, NULL, 1, result );
      case DBOI_SCOPESET:
      case DBOI_SCOPECLEAR: return s_scopeValue( index, 2, value, info == DBOI_SCOPECLEAR || value != NULL, result );
      case DBOI_NAME: s_setText( result, IIF( index != NULL, index->name, "" ) ); return X_DBF_OK;
      case DBOI_NUMBER:
         if( index != NULL )
         {
            for( position = area->indexes; position != index; position = position->next )
               ++number;
            ++number;
         }
         s_setInteger( result, number );
         return X_DBF_OK;
      case DBOI_EXPRESSION:
      case DBOI_CONDITION:
      {
         X_DBF_TEXT  text __attribute__( ( cleanup( s_bytesDrop ) ) ) = { NULL, 0 };
         const char *expression =
            IIF( index == NULL, "", IIF( info == DBOI_EXPRESSION, index->expression, index->condition ) );
         status = x_dbfTextCopy( expression, strlen( expression ), &text );
         if( status == X_DBF_OK )
            x_itemPutStringBuffer( result, x_strndup( text.data, text.length ), text.length );
         return status;
      }
      case DBOI_BAGNAME:
         if( index != NULL )
         {
            const char *base = s_baseName( index->bag->path );
            const char *extension = strrchr( base, '.' );
            size_t      length = IIF( extension != NULL, ( size_t )( extension - base ), strlen( base ) );
            x_itemPutStringBuffer( result, x_strndup( base, length ), length );
         }
         else
            s_setText( result, "" );
         return X_DBF_OK;
      case DBOI_FULLPATH: s_setText( result, IIF( index != NULL, index->bag->path, "" ) ); return X_DBF_OK;
      case DBOI_FILEHANDLE:
         s_setInteger( result, IIF( index != NULL, x_fileHandle( &index->bag->file ), -1 ) );
         return X_DBF_OK;
      case DBOI_KEYTYPE:
         x_itemPutStringBuffer( result, x_strndup( IIF( index != NULL, &index->type, "" ), IIF( index != NULL, 1, 0 ) ),
                                IIF( index != NULL, 1, 0 ) );
         return X_DBF_OK;
      case DBOI_KEYSIZE: s_setInteger( result, IIF( index != NULL, index->length, 0 ) ); return X_DBF_OK;
      case DBOI_KEYDEC:
         s_setInteger( result, IIF( index != NULL && !index->bag->cdx, index->decimals, 0 ) );
         return X_DBF_OK;
      case DBOI_ISCOND: s_setLogical( result, index != NULL && index->forBlock.nType == X_IT_BLOCK ); return X_DBF_OK;
      case DBOI_ISDESC:
         s_setLogical( result, index != NULL && ( index->descending != index->reversed ) );
         if( index != NULL && setting )
         {
            int reversed = value->u.logical != index->descending;
            if( reversed != index->reversed )
            {
               X_ITEM          scope = index->scopes[0];
               X_DBF_INDEX_KEY key = index->scopeKeys[0];
               size_t          length = index->scopeLengths[0];
               index->scopes[0] = index->scopes[1];
               index->scopeKeys[0] = index->scopeKeys[1];
               index->scopeLengths[0] = index->scopeLengths[1];
               index->scopes[1] = scope;
               index->scopeKeys[1] = key;
               index->scopeLengths[1] = length;
               index->reversed = reversed;
               index->cursor.valid = 0;
               ++index->generation;
            }
         }
         return X_DBF_OK;
      case DBOI_UNIQUE:
         s_setLogical( result, index != NULL && ( index->unique || index->userUnique ) );
         if( index != NULL && index->bag->cdx && !index->unique && setting )
         {
            index->userUnique = value->u.logical != 0;
            index->cursor.valid = 0;
            ++index->generation;
         }
         return X_DBF_OK;
      case DBOI_ISMULTITAG:
         s_setLogical( result, index != NULL && ( index->bag->cdx || index->bag->compound ) );
         return X_DBF_OK;
      case DBOI_ISSORTRECNO:
         s_setLogical( result, index != NULL && ( index->bag->cdx || index->sortRecords ) );
         return X_DBF_OK;
      case DBOI_LARGEFILE: s_setLogical( result, index != NULL && index->bag->largeFile ); return X_DBF_OK;
      case DBOI_SHARED:
         s_setLogical( result, index != NULL && area->shared && !index->bag->exclusive );
         return X_DBF_OK;
      case DBOI_ISREADONLY: s_setLogical( result, index != NULL && area->readOnly ); return X_DBF_OK;
      case DBOI_CUSTOM:
      case DBOI_TEMPLATE:
      case DBOI_MULTIKEY:
      case DBOI_PARTIAL:
         if( index != NULL && setting )
         {
            int changed = 0;
            if( area->readOnly )
               return X_DBF_READ_ONLY;
            status = x_dbfAreaFlush( area );
            if( status == X_DBF_OK )
               status = x_dbfOrderLock( area, 1, &lock );
            if( status != X_DBF_OK )
               return status;
            if( info == DBOI_CUSTOM && !index->template && index->custom != ( value->u.logical != 0 ) )
            {
               index->custom = value->u.logical != 0;
               index->partial = 1;
               if( index->bag->cdx && index->custom )
                  index->template = index->multiKey = 1;
               changed = 1;
            }
            else if( info == DBOI_TEMPLATE && index->custom && !index->template && value->u.logical )
            {
               index->template = 1;
               changed = 1;
            }
            else if( info == DBOI_MULTIKEY && index->custom && !index->multiKey && value->u.logical )
            {
               if( !index->bag->cdx )
                  return X_DBF_UNSUPPORTED_OPERATION;
               index->multiKey = 1;
               changed = 1;
            }
            if( changed )
            {
               index->changed = index->headerChanged = 1;
               status = s_sync( index );
               if( status != X_DBF_OK )
                  return status;
            }
         }
         s_setLogical( result,
                       index != NULL && IIF( info == DBOI_CUSTOM, index->custom,
                                             IIF( info == DBOI_TEMPLATE, index->template,
                                                  IIF( info == DBOI_MULTIKEY, index->multiKey, index->partial ) ) ) );
         return X_DBF_OK;
      case DBOI_CHGONLY: s_setLogical( result, 0 ); return X_DBF_OK;
      case DBOI_INDEXTYPE:
         s_setInteger( result, IIF( index == NULL, DBOI_TYPE_UNDEF,
                                    IIF( index->bag->cdx || index->bag->compound, DBOI_TYPE_COMPOUND,
                                         DBOI_TYPE_NONCOMPACT ) ) );
         return X_DBF_OK;
      case DBOI_INDEXPAGESIZE:
         s_setInteger( result, IIF( index == NULL, 0, IIF( index->bag->cdx, 512, NTX_PAGE_SIZE ) ) );
         return X_DBF_OK;
      case DBOI_OPTLEVEL: s_setInteger( result, DBOI_OPTIMIZED_NONE ); return X_DBF_OK;
      case DBOI_LOCKOFFSET:
      case DBOI_HPLOCKING:
      {
         int64_t offset, length;
         s_indexLockParameters( area->lockScheme, &offset, &length );
         if( info == DBOI_LOCKOFFSET )
            s_setInteger( result, offset );
         else
            s_setLogical( result, length > 1 );
         return X_DBF_OK;
      }
      case DBOI_UPDATECOUNTER:
      case DBOI_KEYCOUNT:
      case DBOI_KEYCOUNTRAW:
      case DBOI_POSITION:
      case DBOI_KEYNORAW:
      case DBOI_KEYVAL: break;
      default: return X_DBF_UNSUPPORTED_OPERATION;
   }
   if( setting && ( info == DBOI_POSITION || info == DBOI_KEYNORAW ) )
   {
      X_DBF_INDEX_CURSOR cursor = { 0 };
      uint64_t           target, ordinal = 0, count = 0;
      double             requested = IIF( value->nType == X_IT_DOUBLE, value->u.dbl, ( double )value->u.integer );
      int                cdx = strcmp( area->driver->name, "DBFCDX" ) == 0;
      if( !isfinite( requested ) || requested < 0 || requested >= 18446744073709551616.0 )
         return X_DBF_INVALID_ARGUMENT;
      target = ( uint64_t )requested;
      s_setLogical( result, 0 );
      if( target == 0 )
      {
         if( !cdx && index != NULL )
            return X_DBF_OK;
         status = x_dbfAreaGoTo( area, 0 );
         if( status == X_DBF_OK )
            s_setLogical( result, 1 );
         return status;
      }
      status = x_dbfAreaFlush( area );
      if( status == X_DBF_OK )
         status = x_dbfOrderLock( area, 0, &lock );
      if( status != X_DBF_OK )
         return status;
      if( index != NULL )
      {
         if( index->broken )
            return X_DBF_RECOVERY_REQUIRED;
         status = s_boundary( index, 0, &cursor );
      }
      else
      {
         status = x_dbfAreaRecordCount( area, &count );
         cursor.key.record = 1;
         cursor.valid = count != 0;
      }
      if( status != X_DBF_OK )
         return status;
      while( cursor.valid && ( index == NULL || s_scope( index, &cursor.key ) == 0 ) )
      {
         int accepted = 1;
         if( filters && area->filter.nType == X_IT_BLOCK )
         {
            status = x_dbfAreaGoTo( area, cursor.key.record );
            if( status == X_DBF_OK )
            {
               X_DBF_ORDER_GUARD busy __attribute__( ( cleanup( s_restore ) ) ) = { area, area->state, NULL };
               area->indexBusy = 1;
               status = x_dbfAreaAccept( area, x_setDeleted, &accepted );
            }
            if( status != X_DBF_OK )
               return status;
         }
         if( accepted && ++ordinal == target )
            break;
         if( index != NULL )
            status = s_step( index, &cursor, 0 );
         else if( cursor.key.record < count )
            ++cursor.key.record;
         else
            cursor.valid = 0;
         if( status != X_DBF_OK )
            return status;
      }
      status = x_dbfAreaGoTo( area, IIF( cursor.valid && ordinal == target, cursor.key.record, 0 ) );
      if( status != X_DBF_OK )
         return status;
      if( index != NULL && !area->state.eof )
      {
         index->cursor = cursor;
         if( !cdx )
         {
            int accepted;
            {
               X_DBF_ORDER_GUARD busy __attribute__( ( cleanup( s_restore ) ) ) = { area, area->state, NULL };
               area->indexBusy = 1;
               status = x_dbfAreaAccept( area, x_setDeleted, &accepted );
            }
            while( status == X_DBF_OK && !accepted )
            {
               status = s_step( index, &cursor, 0 );
               if( status != X_DBF_OK || !cursor.valid || s_scope( index, &cursor.key ) != 0 )
                  break;
               status = x_dbfAreaGoTo( area, cursor.key.record );
               if( status == X_DBF_OK )
               {
                  X_DBF_ORDER_GUARD busy __attribute__( ( cleanup( s_restore ) ) ) = { area, area->state, NULL };
                  area->indexBusy = 1;
                  status = x_dbfAreaAccept( area, x_setDeleted, &accepted );
               }
            }
            if( status != X_DBF_OK )
               return status;
            if( !accepted )
            {
               status = x_dbfAreaGoTo( area, 0 );
               if( status != X_DBF_OK )
                  return status;
            }
            else
               index->cursor = cursor;
         }
      }
      s_setLogical( result, 1 );
      return X_DBF_OK;
   }
   if( index == NULL )
   {
      int cdx = strcmp( area->driver->name, "DBFCDX" ) == 0;
      if( info == DBOI_KEYCOUNT || info == DBOI_KEYCOUNTRAW )
      {
         uint64_t count;
         if( cdx && filters && area->filter.nType == X_IT_BLOCK )
         {
            status = x_dbfAreaFlush( area );
            if( status == X_DBF_OK )
               status = s_measure( area, NULL, NULL, 1, &count );
         }
         else
            status = x_dbfAreaRecordCount( area, &count );
         if( status != X_DBF_OK )
            return status;
         s_setInteger( result, ( int64_t )count );
      }
      else if( info == DBOI_POSITION || info == DBOI_KEYNORAW )
      {
         uint64_t ordinal = IIF( cdx && area->state.eof, 0, area->state.record );
         if( cdx && ordinal != 0 && filters && area->filter.nType == X_IT_BLOCK )
         {
            X_DBF_INDEX_KEY last = { 0 };
            last.record = ( uint32_t )ordinal;
            status = x_dbfAreaFlush( area );
            if( status == X_DBF_OK )
               status = s_measure( area, NULL, &last, 1, &ordinal );
            if( status != X_DBF_OK )
               return status;
         }
         s_setInteger( result, ( int64_t )ordinal );
      }
      else if( info == DBOI_UPDATECOUNTER )
         s_setInteger( result, 0 );
      return X_DBF_OK;
   }
   status = x_dbfAreaFlush( area );
   if( status != X_DBF_OK )
      return status;
   status = x_dbfOrderLock( area, 0, &lock );
   if( status != X_DBF_OK )
      return status;
   if( index->broken )
      return X_DBF_RECOVERY_REQUIRED;
   if( info == DBOI_UPDATECOUNTER )
   {
      s_setInteger( result, index->bag->version );
      return X_DBF_OK;
   }
   if( info == DBOI_KEYCOUNT || info == DBOI_KEYCOUNTRAW )
   {
      uint64_t count;
      status = s_keyCount( area, index, filters, &count );
      if( status == X_DBF_OK )
         s_setInteger( result, ( int64_t )count );
      return status;
   }
   if( area->state.eof )
   {
      if( info == DBOI_POSITION || info == DBOI_KEYNORAW )
         s_setInteger( result, 0 );
      return X_DBF_OK;
   }
   {
      X_DBF_INDEX_CURSOR cursor = { 0 };
      status = s_current( index, &cursor );
      if( status != X_DBF_OK )
         return status;
      member = cursor.valid && s_scope( index, &cursor.key ) == 0;
      if( member )
         present = cursor.key;
   }
   if( !member )
   {
      x_itemClear( result );
      if( info != DBOI_KEYVAL )
         s_setInteger( result, 0 );
   }
   else if( info == DBOI_KEYVAL )
      return s_keyValue( index, &present, result );
   else
   {
      uint64_t count;
      status = s_keyNumber( area, index, &present, filters, &count );
      if( status == X_DBF_OK )
         s_setInteger( result, ( int64_t )count );
      return status;
   }
   return status;
}
