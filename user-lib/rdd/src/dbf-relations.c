/*
 * Copyright (c) 2024-2026 xDev-Language
 *
 * Library...: xDev 7.0.0 RDD
 * File......: dbf-relations.c
 *
 */

#include "dbf-internal.h"

#include <math.h>

typedef struct
{
   X_DBF_SESSION *session;
   uint32_t       number;
   uint64_t       identity;
   uint32_t       selected;
   int            filter;
} X_DBF_EVAL_GUARD;

static DBF s_area( const X_DBF_EVAL_GUARD *guard )
{
   DBF area = x_dbfSessionArea( guard->session, guard->number );
   return IIF( area != NULL && area->identity == guard->identity, area, NULL );
}

static void s_restoreSelection( X_DBF_EVAL_GUARD *guard )
{
   guard->session->current = guard->selected;
}

static void s_restoreBusy( X_DBF_EVAL_GUARD *guard )
{
   if( guard->filter < 0 )
      return;
   DBF area = s_area( guard );
   if( area != NULL )
   {
      if( guard->filter )
         area->filterBusy = 0;
      else
         area->relationBusy = 0;
   }
}

static void s_numbersDrop( uint32_t **numbers )
{
   x_xfree( *numbers );
}

static X_DBF_RELATION *s_findRelation( DBF parent, uint64_t identity )
{
   X_DBF_RELATION *relation;
   for( relation = parent->relations; relation != NULL; relation = relation->next )
   {
      if( relation->identity == identity )
         return relation;
   }
   return NULL;
}

static void s_relationDrop( X_DBF_RELATION **relation )
{
   if( *relation != NULL )
   {
      x_itemClear( &( *relation )->block );
      x_itemClear( &( *relation )->text );
      x_xfree( *relation );
   }
}

static void s_removeRelation( X_DBF_RELATION **position )
{
   X_DBF_RELATION *relation __attribute__( ( cleanup( s_relationDrop ) ) ) = *position;
   X_ITEM          previous __attribute__( ( cleanup( x_itemClear ) ) ) = X_ITEM_NIL_INIT;
   if( relation->scoped )
      x_dbfOrderScope( relation->child, 2, NULL, 1, &previous );
   *position = relation->next;
   if( relation->child->pendingRelation == relation )
      relation->child->pendingRelation = NULL;
}

static X_DBF_STATUS s_walk( DBF root, DBF target, int mark )
{
   X_DBF_SESSION *session = root->session;
   uint32_t      *queue __attribute__( ( cleanup( s_numbersDrop ) ) ) = NULL;
   DBF            area;
   size_t         count = 0, head = 0, tail = 1;

   if( root == target )
      return X_DBF_INVALID_ARGUMENT;
   if( root->relations == NULL )
      return X_DBF_OK;
   for( area = session->areas; area != NULL; area = area->next )
      ++count;
   queue = ( uint32_t * )x_xgrab( count * sizeof( *queue ) );
   queue[0] = root->number;
   while( head < tail )
   {
      X_DBF_RELATION *relation;
      area = x_dbfSessionArea( session, queue[head++] );
      if( area == NULL )
         return X_DBF_INVALID_STATE;
      for( relation = area->relations; relation != NULL; relation = relation->next )
      {
         size_t i;
         DBF    child = relation->child;
         if( child == target )
            return X_DBF_INVALID_ARGUMENT;
         if( mark )
         {
            X_DBF_STATUS status = IIF( child->state.dirty, x_dbfAreaFlush( child ), X_DBF_OK );
            if( status != X_DBF_OK )
               return status;
            child->pendingRelation = relation;
         }
         for( i = 0; i < tail && queue[i] != child->number; ++i )
         {
         }
         if( i == tail )
         {
            if( tail == count )
               return X_DBF_INVALID_STATE;
            queue[tail++] = child->number;
         }
      }
   }
   return X_DBF_OK;
}

X_DBF_STATUS x_dbfAreaMarkChildren( DBF area )
{
   return s_walk( area, NULL, 1 );
}

X_DBF_STATUS x_dbfAreaEvalBlock( X_CALL_CTX *ctx, DBF area, const X_ITEM *block, X_ITEM *result )
{
   return x_dbfAreaEvalBlockArgs( ctx, area, block, 0, NULL, result );
}

X_DBF_STATUS x_dbfAreaEvalBlockArgs( X_CALL_CTX *ctx, DBF area, const X_ITEM *block, int32_t argc, const X_ARG *args,
                                     X_ITEM *result )
{
   X_ITEM           held __attribute__( ( cleanup( x_itemClear ) ) ) = X_ITEM_NIL_INIT;
   X_DBF_EVAL_GUARD guard
      __attribute__( ( cleanup( s_restoreSelection ) ) ) = { area->session, area->number, area->identity,
                                                             area->session->current, 0 };
   X_DBF_STATUS status;

   x_itemCopy( &held, block );
   status = x_dbfAreaPrepare( area );
   if( status != X_DBF_OK )
      return status;
   guard.session->current = guard.number;
   x_evalBlock( ctx, result, &held, argc, args );
   return IIF( s_area( &guard ) != NULL, X_DBF_OK, X_DBF_INVALID_STATE );
}

X_DBF_STATUS x_dbfAreaPrepare( DBF area )
{
   X_ITEM           block __attribute__( ( cleanup( x_itemClear ) ) ) = X_ITEM_NIL_INIT;
   X_ITEM           value __attribute__( ( cleanup( x_itemClear ) ) ) = X_ITEM_NIL_INIT;
   X_DBF_EVAL_GUARD guard
      __attribute__( ( cleanup( s_restoreBusy ) ) ) = { area->session, area->number, area->identity, 0, -1 };
   X_DBF_EVAL_GUARD parentGuard;
   X_DBF_RELATION  *relation = area->pendingRelation;
   X_DBF_STATUS     status;
   uint64_t         relationIdentity, record = 0;
   DBF              parent;

   if( relation == NULL )
      return X_DBF_OK;
   if( area->relationBusy )
      return X_DBF_INVALID_STATE;
   guard.filter = 0;
   area->relationBusy = 1;
   parent = relation->parent;
   parentGuard = ( X_DBF_EVAL_GUARD ){ parent->session, parent->number, parent->identity, 0, 0 };
   relationIdentity = relation->identity;
   x_itemCopy( &block, &relation->block );
   area->pendingRelation = NULL;
   status = x_dbfAreaPrepare( parent );
   if( status != X_DBF_OK )
      return status;
   parent = s_area( &parentGuard );
   area = s_area( &guard );
   if( parent == NULL || area == NULL )
      return X_DBF_INVALID_STATE;
   if( s_findRelation( parent, relationIdentity ) == NULL )
      return X_DBF_OK;
   if( !parent->state.eof )
   {
      X_CALL_CTX call = { NULL, 0, &value, NULL, NULL };
      status = x_dbfAreaEvalBlock( &call, parent, &block, &value );
      if( status != X_DBF_OK )
         return status;
      parent = s_area( &parentGuard );
      area = s_area( &guard );
      if( parent == NULL || area == NULL )
         return X_DBF_INVALID_STATE;
      relation = s_findRelation( parent, relationIdentity );
      if( relation == NULL )
         return X_DBF_OK;
      if( area->order != NULL )
      {
         if( relation->scoped )
         {
            X_ITEM previous __attribute__( ( cleanup( x_itemClear ) ) ) = X_ITEM_NIL_INIT;
            status = x_dbfOrderScope( area, 2, &value, 1, &previous );
            if( status != X_DBF_OK )
               return status;
         }
         return x_dbfOrderSeek( area, &value, 0, 0, x_setDeleted );
      }
      if( value.nType == X_IT_INTEGER && value.u.integer > 0 )
         record = ( uint64_t )value.u.integer;
      else if( value.nType == X_IT_DOUBLE && isfinite( value.u.dbl ) && value.u.dbl > 0 && value.u.dbl < 0x1p63 )
         record = ( uint64_t )value.u.dbl;
   }
   status = x_dbfAreaGoTo( area, record );
   if( status == X_DBF_OK )
      area->state.found = !area->state.eof;
   return status;
}

X_DBF_STATUS x_dbfAreaAccept( DBF area, int skipDeleted, int *accepted )
{
   X_ITEM           value __attribute__( ( cleanup( x_itemClear ) ) ) = X_ITEM_NIL_INIT;
   X_DBF_EVAL_GUARD guard
      __attribute__( ( cleanup( s_restoreBusy ) ) ) = { area->session, area->number, area->identity, 0, -1 };
   X_DBF_STATUS status = x_dbfAreaPrepare( area );
   uint64_t     record;

   if( status != X_DBF_OK )
      return status;
   *accepted = !area->state.eof && ( !skipDeleted || !area->state.deleted );
   if( !*accepted || area->filter.nType != X_IT_BLOCK )
      return X_DBF_OK;
   if( area->filterBusy )
      return X_DBF_INVALID_STATE;
   guard.filter = 1;
   area->filterBusy = 1;
   record = area->state.record;
   {
      X_CALL_CTX call = { NULL, 0, &value, NULL, NULL };
      status = x_dbfAreaEvalBlock( &call, area, &area->filter, &value );
   }
   if( status != X_DBF_OK )
      return status;
   area = s_area( &guard );
   if( area == NULL || area->state.record != record )
      return X_DBF_INVALID_STATE;
   *accepted = value.nType != X_IT_LOGICAL || value.u.logical;
   return X_DBF_OK;
}

X_DBF_STATUS x_dbfAreaSetFilter( DBF area, const X_ITEM *block, const X_ITEM *text )
{
   X_ITEM       heldBlock __attribute__( ( cleanup( x_itemClear ) ) ) = X_ITEM_NIL_INIT;
   X_ITEM       heldText __attribute__( ( cleanup( x_itemClear ) ) ) = X_ITEM_NIL_INIT;
   X_DBF_STATUS status;

   if( text != NULL )
   {
      status = x_dbfTextValidateUTF8( text->u.string.data, text->u.string.len );
      if( status != X_DBF_OK )
         return status;
      x_itemCopy( &heldText, text );
   }
   if( block != NULL )
      x_itemCopy( &heldBlock, block );
   x_itemCopy( &area->filter, &heldBlock );
   x_itemCopy( &area->filterText, &heldText );
   return X_DBF_OK;
}

void x_dbfAreaClearFilter( DBF area )
{
   x_itemClear( &area->filter );
   x_itemClear( &area->filterText );
}

X_DBF_STATUS x_dbfAreaFilterText( DBF area, X_ITEM *result )
{
   if( area->filterText.nType == X_IT_STRING )
      x_itemCopy( result, &area->filterText );
   else
      x_itemPutStringBuffer( result, x_strdup( "" ), 0 );
   return X_DBF_OK;
}

X_DBF_STATUS x_dbfAreaSetRelation( DBF parent, DBF child, const X_ITEM *block, const X_ITEM *text )
{
   X_DBF_RELATION  *relation __attribute__( ( cleanup( s_relationDrop ) ) ) = NULL;
   X_DBF_RELATION **position;
   X_DBF_STATUS     status;

   status = s_walk( child, parent, 0 );
   if( status != X_DBF_OK )
      return status;
   if( text != NULL && text->nType != X_IT_NIL )
   {
      status = x_dbfTextValidateUTF8( text->u.string.data, text->u.string.len );
      if( status != X_DBF_OK )
         return status;
   }
   relation = ( X_DBF_RELATION * )x_xgrab( sizeof( *relation ) );
   *relation =
      ( X_DBF_RELATION ){ NULL, parent, child, X_ITEM_NIL_INIT, X_ITEM_NIL_INIT, ++parent->session->nextIdentity, 0 };
   x_itemCopy( &relation->block, block );
   if( text != NULL && text->nType == X_IT_STRING )
      x_itemCopy( &relation->text, text );
   status = IIF( child->state.dirty, x_dbfAreaFlush( child ), X_DBF_OK );
   if( status != X_DBF_OK )
      return status;
   for( position = &parent->relations; *position != NULL; position = &( *position )->next )
   {
   }
   *position = relation;
   child->pendingRelation = relation;
   relation = NULL;
   return x_dbfAreaMarkChildren( child );
}

X_DBF_STATUS x_dbfAreaClearRelations( DBF area )
{
   X_DBF_EVAL_GUARD guard = { area->session, area->number, area->identity, 0, 0 };

   while( area->relations != NULL )
   {
      X_DBF_RELATION  *relation = area->relations;
      uint64_t         identity = relation->identity;
      X_DBF_RELATION **position;
      if( relation->child->pendingRelation == relation )
      {
         X_DBF_STATUS status = x_dbfAreaPrepare( relation->child );
         if( status != X_DBF_OK )
            return status;
      }
      area = s_area( &guard );
      if( area == NULL )
         return X_DBF_INVALID_STATE;
      for( position = &area->relations; *position != NULL; position = &( *position )->next )
      {
         if( ( *position )->identity == identity )
         {
            s_removeRelation( position );
            break;
         }
      }
   }
   return X_DBF_OK;
}

void x_dbfAreaDetachRelations( DBF area )
{
   DBF parent;
   for( parent = area->session->areas; parent != NULL; parent = parent->next )
   {
      X_DBF_RELATION **position = &parent->relations;
      while( *position != NULL )
      {
         if( ( *position )->child == area )
            s_removeRelation( position );
         else
            position = &( *position )->next;
      }
   }
   while( area->relations != NULL )
      s_removeRelation( &area->relations );
   area->pendingRelation = NULL;
}

X_DBF_STATUS x_dbfAreaRelationInfo( DBF area, size_t ordinal, uint32_t *child, X_ITEM *text )
{
   X_DBF_RELATION *relation = area->relations;
   while( relation != NULL && --ordinal != 0 )
      relation = relation->next;
   if( child != NULL )
      *child = IIF( relation != NULL, relation->child->number, 0 );
   if( text != NULL )
   {
      if( relation != NULL && relation->text.nType == X_IT_STRING )
         x_itemCopy( text, &relation->text );
      else
         x_itemPutStringBuffer( text, x_strdup( "" ), 0 );
   }
   return X_DBF_OK;
}
