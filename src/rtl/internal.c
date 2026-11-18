/*
 * Copyright (c) 2024-2026 xDev-Language
 *
 * Library: xDev Runtime
 * File: internal.c
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "internal-private.h"
#include "classes.h"

#include "terminal.h"
#include "version.h"

#include "../../include/xdev/set.ch"

#include <ctype.h>
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "platform.h"
#if X_OS_WINDOWS
#include <fcntl.h>
#include <io.h>
#include <windows.h>
#else
#include <fcntl.h>
#include <pwd.h>
#include <sys/resource.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

/* ---------------------------------------------------------------------------------------------
   Types and runtime state
   --------------------------------------------------------------------------------------------- */

typedef struct _X_BLOCK X_BLOCK;

typedef struct
{
   X_ERROR *caught;
   X_ERROR *active;
   int reported;
   int breaking;
   X_ITEM value;
} X_ERRCTX;

struct _X_GC_NODE
{
   void *value;
   size_t *refs;
   size_t cargoRefs;
   size_t locks;
   size_t outside;
   X_GC_KIND kind;
   const X_GC_FUNCS *functions;
   int marked, scanned, finalized, dead, initialLock;
   X_GC_NODE *next;
   X_GC_NODE *prev;
   X_GC_NODE *hashNext;
   X_GC_NODE *zeroNext;
};

struct _X_BLOCK
{
   size_t nRefs;
   X_BLOCKFUNC pFunc;
   X_ITEM **env;
   int envLen;
   int lexical;
   X_METHOD_CONTEXT context;
   X_ITEM self;
};

static X_GC_NODE *s_gcNodes;
static X_BOX *s_nativeBoxes;
static int s_collectingGarbage;
static int s_collectClassCycles;
static int s_classInlineMarked;
static int s_gcVisitMode;
static unsigned int s_gcPauseDepth;
static int s_gcRequested;
static X_GC_NODE **s_gcTable;
static size_t s_gcTableSize;
static size_t s_gcNodeCount;
static size_t s_gcReleases;
static X_GC_NODE *s_gcZero;
static int s_gcFreeing;

enum
{
   X_GC_TABLE_MINIMUM = 1024,
   X_GC_CYCLE_MINIMUM = 1024
};

static const X_ERROR_DEF s_errorDefinitions[] = {
   { 2, 14, 1003, 0, "BASE", "Variable does not exist", "variable" },
   { 2, 13, 1004, 0, "BASE", "Message does not exist", "message" },
   { 2, 1, 3000, 0, "BASE", "Invalid object operation", "object" },
   { 2, 1, 3001, 0, "BASE", "Method access denied", "message" },
   { 2, 1, 3002, 0, "BASE", "Read-only property", "message" },
   { 2, 1, 3003, 0, "BASE", "Invalid message arguments", "message" },
   { 2, 1, 1066, 0, "BASE", "Argument error", "conditional" },
   { 2, 1, 1068, 0, "BASE", "Argument error", "array access" },
   { 2, 1, 1069, 0, "BASE", "Argument error", "array assign" },
   { 2, 2, 1131, 0, "BASE", "Bound error", "array dimension" },
   { 2, 2, 1132, 0, "BASE", "Bound error", "array access" },
   { 2, 2, 1133, 0, "BASE", "Bound error", "array assign" },
   { 2, 1, 1070, 1, "BASE", "Argument error", "==" },
   { 2, 1, 1071, 1, "BASE", "Argument error", "=" },
   { 2, 1, 1072, 1, "BASE", "Argument error", "<>" },
   { 2, 1, 1073, 1, "BASE", "Argument error", "<" },
   { 2, 1, 1074, 1, "BASE", "Argument error", "<=" },
   { 2, 1, 1075, 1, "BASE", "Argument error", ">" },
   { 2, 1, 1076, 1, "BASE", "Argument error", ">=" },
   { 2, 1, 1077, 1, "BASE", "Argument error", ".NOT." },
   { 2, 1, 1078, 1, "BASE", "Argument error", ".AND." },
   { 2, 1, 1079, 1, "BASE", "Argument error", ".OR." },
   { 2, 1, 1080, 1, "BASE", "Argument error", "-" },
   { 2, 1, 1081, 1, "BASE", "Argument error", "+" },
   { 2, 1, 1082, 1, "BASE", "Argument error", "-" },
   { 2, 1, 1083, 1, "BASE", "Argument error", "*" },
   { 2, 1, 1084, 1, "BASE", "Argument error", "/" },
   { 2, 1, 1085, 1, "BASE", "Argument error", "%" },
   { 2, 1, 1086, 1, "BASE", "Argument error", "++" },
   { 2, 1, 1087, 1, "BASE", "Argument error", "--" },
   { 2, 1, 1088, 1, "BASE", "Argument error", "^" },
   { 2, 1, 1089, 1, "BASE", "Argument error", "$" },
   { 2, 1, 1102, 1, "BASE", "Argument error", "UPPER" },
   { 2, 1, 1111, 1, "BASE", "Argument error", "LEN" },
   { 2, 21, 2018, 0, "BASE", "Open error", "DISKSPACE" },
   { 2, 1, 3104, 1, "BASE", "Argument error", "SWITCH" },
   { 2, 2, 3105, 0, "BASE", "Value outside declared type range", "conversion" },
   { 2, 4, 3106, 0, "BASE", "Integer overflow", "arithmetic" },
   { 2, 1, 3107, 0, "BASE", "Declared type mismatch", "conversion" },
   { 2, 5, 3108, 0, "BASE", "Zero divisor", "arithmetic" },
   { 2, 5, 1340, 1, "BASE", "Zero divisor", "/" },
   { 2, 5, 1341, 1, "BASE", "Zero divisor", "%" },
   { 2, 34, 1901, 0, "BASE", "No GT driver selected", "screen" },
   { 2, 34, 1900, 0, "BASE", "Not implemented", "function" }
};

static X_ERROR *s_caught;
static X_ERROR *s_activeError;
static int s_errorReported;
static X_ERROR_HANDLER s_errorHandler;
static int s_break;
static X_ITEM s_breakValue = X_ITEM_NIL_INIT;
static X_ITEM s_errorBlock = X_ITEM_NIL_INIT;
static int s_errorLevel;
static int s_defaultPropagate;
static int s_outputMidLine;
static uint64_t s_randomState;
static int s_randomSeeded;
static _Thread_local int s_callbackDllDepth;
static _Thread_local X_ERROR *s_callbackError;
static _Thread_local int s_callbackBreak;
static _Thread_local X_ITEM s_callbackBreakValue = X_ITEM_NIL_INIT;

/* ---------------------------------------------------------------------------------------------
   Forward declarations
   --------------------------------------------------------------------------------------------- */

static int s_itemSetString( X_ITEM *item, const char *value, size_t length );

/* ---------------------------------------------------------------------------------------------
   Call context
   --------------------------------------------------------------------------------------------- */

/* Keep the error state of the caller and start a clean one */
static X_ERRCTX s_errorSave( void )
{
   X_ERRCTX saved = { s_caught, s_activeError, s_errorReported, s_break, s_breakValue };

   s_caught = NULL;
   s_activeError = NULL;
   s_errorReported = 0;
   s_break = 0;
   s_breakValue = ( X_ITEM )X_ITEM_NIL_INIT;

   return saved;
}

/* Give the caller its error state back; an error of the call stays pending */
static void s_errorRestore( X_ERRCTX *saved )
{
   if( saved->caught != NULL || saved->active != NULL || saved->breaking )
   {
      x_errorClear();
      s_caught = saved->caught;
      s_errorReported = saved->reported;
      s_break = saved->breaking;
      s_breakValue = saved->value;
   }

   s_activeError = saved->active;
}

static void s_activeRestore( X_ERROR **previous )
{
   s_activeError = *previous;
}

static void s_propagateRestore( int *previous )
{
   s_defaultPropagate = *previous;
}

/* Call a function at the end of the program; a BREAK or an error is reported, not raised */
void x_callCleanup( X_FUNCPTR function, X_CALL_CTX *ctx )
{
   X_ERRCTX saved __attribute__( ( cleanup( s_errorRestore ) ) ) = s_errorSave();

   x_callCatch( function, ctx );
}

/* A DLL call starts: errors of callbacks wait until it returns */
void x_callbackEnterDll( void )
{
   ++s_callbackDllDepth;
}

/* A DLL call ended */
void x_callbackLeaveDll( void )
{
   if( s_callbackDllDepth > 0 )
      --s_callbackDllDepth;
}

/* Call an xDev function from C code (a CALLBACK); an error waits until the DLL call returns */
int x_callbackInvoke( X_FUNCPTR function, X_ITEM *result, int32_t argc, const X_ARG *args )
{
   if( s_callbackDllDepth <= 0 || s_callbackError != NULL || s_callbackBreak )
      return 0;

   X_CALL_CTX call = { args, argc, result, NULL, NULL };

   x_callCatch( function, &call );
   if( !x_errorCaught() )
      return 1;

   const X_ERROR *error = x_errorPeek();

   if( error != NULL )
      s_callbackError = x_errorClone( error );
   else if( s_break )
   {
      x_itemCopy( &s_callbackBreakValue, &s_breakValue );
      s_callbackBreak = 1;
   }

   x_errorClear();

   return 0;
}

/* Put a C string into an item; NULL gives "" */
void x_callbackPutString( X_ITEM *item, const char *value )
{
   if( value == NULL )
      s_itemSetString( item, "", 0 );
   else
      s_itemSetString( item, value, strlen( value ) );
}

/* Raise the error or BREAK of a callback after the DLL call returned */
void x_callbackRaisePending( void )
{
   if( s_callbackError != NULL )
   {
      X_ERROR *error = s_callbackError;

      s_callbackError = NULL;
      x_errorThrow( error );
   }

   if( s_callbackBreak )
   {
      X_ITEM value __attribute__( ( cleanup( x_itemClear ) ) ) = s_callbackBreakValue;

      s_callbackBreakValue = ( X_ITEM )X_ITEM_NIL_INIT;
      s_callbackBreak = 0;
      x_breakRaise( &value );
   }
}

/* Run a cleanup function; a BREAK or an error is reported, not raised */
void x_gcCleanup( X_GARBAGE_FUNC_PTR function, void *cargo )
{
   X_ERRCTX saved __attribute__( ( cleanup( s_errorRestore ) ) ) = s_errorSave();

   x_gcCatch( function, cargo );
}

/* Self of the method that runs now */
X_ITEM *x_stackSelfItem( X_CALL_CTX *ctx )
{
   return ctx->self;
}

/* ---------------------------------------------------------------------------------------------
   Garbage collection
   --------------------------------------------------------------------------------------------- */

/* Stop the garbage collector */
void x_gcSuspend( void )
{
   ++s_gcPauseDepth;
}

/* Let the garbage collector run again */
void x_gcResume( void )
{
   --s_gcPauseDepth;
   if( s_gcPauseDepth == 0 && s_gcRequested )
      x_gcCollect();
}

unsigned int x_gcPause( void )
{
   return s_gcPauseDepth++;
}

/* The end of x_gcPause() */
void x_gcRestore( unsigned int *depth )
{
   s_gcPauseDepth = *depth;
   if( s_gcPauseDepth == 0 && s_gcRequested )
      x_gcCollect();
}

static void s_gcFinish( int *previous )
{
   s_collectingGarbage = *previous;
   s_gcVisitMode = 0;
   s_gcRequested = 0;
}

static void s_classCyclesRestore( int *previous )
{
   s_collectClassCycles = *previous;
}

/* The slot of a value in the table of the garbage collector */
static size_t s_gcSlot( const void *value )
{
   const uint64_t key = ( uint64_t )( uintptr_t )value >> 4;

   return ( size_t )( ( key * 0x9E3779B97F4A7C15ull ) >> 32 ) & ( s_gcTableSize - 1 );
}

/* Twice as many slots; the nodes are placed again */
static void s_gcTableGrow( void )
{
   X_GC_NODE *node;
   size_t slot;

   if( s_gcTable != NULL )
      x_xfree( s_gcTable );

   s_gcTableSize = IIF( s_gcTableSize == 0, ( size_t )X_GC_TABLE_MINIMUM, s_gcTableSize * 2 );
   s_gcTable = ( X_GC_NODE ** )x_xgrabzArray( s_gcTableSize, sizeof( X_GC_NODE * ) );
   for( node = s_gcNodes; node != NULL; node = node->next )
   {
      slot = s_gcSlot( node->value );
      node->hashNext = s_gcTable[slot];
      s_gcTable[slot] = node;
   }
}

/* Remove a node from the table */
static void s_gcTableRemove( X_GC_NODE *node )
{
   X_GC_NODE **link = &s_gcTable[s_gcSlot( node->value )];

   while( *link != node )
      link = &( *link )->hashNext;

   *link = node->hashNext;
   --s_gcNodeCount;
}

/* The node of a value; NULL when the collector does not know it */
static X_GC_NODE *s_gcFind( const void *value )
{
   X_GC_NODE *node;

   if( s_gcTable == NULL )
      return NULL;

   for( node = s_gcTable[s_gcSlot( value )]; node != NULL; node = node->hashNext )
   {
      if( node->value == value )
         return node;
   }

   return NULL;
}

/* Take a node out of the list and the table */
static void s_gcUnlink( X_GC_NODE *node )
{
   if( node->prev != NULL )
      node->prev->next = node->next;
   else
      s_gcNodes = node->next;

   if( node->next != NULL )
      node->next->prev = node->prev;

   s_gcTableRemove( node );
}

/* Release the values a node holds */
static void s_gcClearNode( X_GC_NODE *node )
{
   if( node->kind == X_GC_ARRAY )
   {
      X_ARR *array = ( X_ARR * )node->value;
      int64_t i;

      for( i = 0; i < array->len; ++i )
         x_itemClear( &array->items[i] );
   }
   else if( node->kind == X_GC_HASH )
   {
      X_HASH *hash = ( X_HASH * )node->value;
      size_t i;

      for( i = 0; i < hash->len; ++i )
      {
         x_itemClear( &hash->keys[i] );
         x_itemClear( &hash->values[i] );
      }

      x_itemClear( &hash->defaultValue );
   }
   else if( node->kind == X_GC_BOX )
   {
      X_BOX *box = ( X_BOX * )node->value;

      x_itemClear( &box->item );
      if( box->owner != NULL )
         x_boxRelease( box->owner );
   }
   else if( node->kind == X_GC_BLOCK )
   {
      X_BLOCK *block = ( X_BLOCK * )node->value;
      int i;

      for( i = 0; i < block->envLen; ++i )
         x_boxRelease( ( X_BOX * )block->env[i] );

      x_itemClear( &block->self );
   }
}

/* Free a node and its value */
static void s_gcFreeNode( X_GC_NODE *node )
{
   if( node->kind == X_GC_ARRAY )
      x_xfree( ( ( X_ARR * )node->value )->items );
   else if( node->kind == X_GC_HASH )
   {
      x_xfree( ( ( X_HASH * )node->value )->keys );
      x_xfree( ( ( X_HASH * )node->value )->values );
      x_xfree( ( ( X_HASH * )node->value )->slots );
   }
   else if( node->kind == X_GC_BLOCK )
      x_xfree( ( ( X_BLOCK * )node->value )->env );

   if( node->kind == X_GC_BOX && ( ( X_BOX * )node->value )->nativeType != 0 )
   {
      X_BOX **link = &s_nativeBoxes;

      while( *link != ( X_BOX * )node->value )
         link = &( *link )->nativeNext;

      *link = ( *link )->nativeNext;
   }

   x_xfree( node->value );
   x_xfree( node );
}

/* Collect now, or after the release that runs */
static void s_gcCollectSoon( void )
{
   if( s_gcFreeing )
      s_gcRequested = 1;
   else
      x_gcCollect();
}

/* A reference was released: a value without references goes, cycles are collected now and then */
static void s_gcReleased( void *value, size_t refs )
{
   size_t cycle = IIF( s_gcNodeCount > X_GC_CYCLE_MINIMUM, s_gcNodeCount, ( size_t )X_GC_CYCLE_MINIMUM );
   X_GC_NODE *node = NULL;
   int destructor;

   /* A value that is still referenced: a collection runs after many releases, to find cycles */
   if( refs != 0 )
   {
      if( ++s_gcReleases >= cycle )
         s_gcCollectSoon();
      return;
   }

   if( !s_collectingGarbage && s_gcPauseDepth == 0 )
      node = s_gcFind( value );

   if( node == NULL )
   {
      s_gcCollectSoon();
      return;
   }

   if( node->locks != 0 )
      return;

   destructor = node->kind == X_GC_ARRAY && x_clsHasDestructor( ( ( X_ARR * )value )->classHandle );
   if( node->kind == X_GC_CARGO || destructor )
   {
      s_gcCollectSoon();
      return;
   }

   node->zeroNext = s_gcZero;
   s_gcZero = node;
   if( s_gcFreeing )
      return;

   s_gcFreeing = 1;
   while( s_gcZero != NULL )
   {
      node = s_gcZero;
      s_gcZero = node->zeroNext;
      s_gcUnlink( node );
      s_gcClearNode( node );
      s_gcFreeNode( node );
   }

   s_gcFreeing = 0;
   if( s_gcRequested )
      x_gcCollect();
}

/* A new value of the garbage collector */
X_GC_NODE *x_gcRegister( void *value, size_t *refs, X_GC_KIND kind )
{
   X_GC_NODE *node = ( X_GC_NODE * )x_xgrabz( sizeof( X_GC_NODE ) );
   size_t slot;

   if( s_gcNodeCount >= s_gcTableSize )
      s_gcTableGrow();

   node->value = value;
   node->refs = IIF( refs != NULL, refs, &node->cargoRefs );
   node->kind = kind;
   node->next = s_gcNodes;
   if( s_gcNodes != NULL )
      s_gcNodes->prev = node;

   s_gcNodes = node;
   slot = s_gcSlot( value );
   node->hashNext = s_gcTable[slot];
   s_gcTable[slot] = node;
   ++s_gcNodeCount;

   return node;
}

/* Mark a value as reachable */
void x_gcMark( void *cargo )
{
   X_GC_NODE *node;

   if( s_gcVisitMode == 0 )
      return;

   node = s_gcFind( cargo );
   if( node == NULL )
      return;

   if( s_gcVisitMode == 1 )
   {
      if( node->outside > 0 )
         --node->outside;
   }
   else
      node->marked = 1;
}

/* Mark the value of an item as reachable */
void x_gcMarkItem( const X_ITEM *item )
{
   if( item == NULL )
      return;

   if( item->nType == X_IT_ARRAY )
      x_gcMark( item->u.pArr );
   else if( item->nType == X_IT_HASH )
      x_gcMark( item->u.pPointer );
   else if( item->nType == X_IT_BLOCK )
      x_gcMark( item->u.pBlock );
   else if( item->nType == X_IT_GCPOINTER )
      x_gcMark( item->u.pPointer );
}

/* The mark function of a value that holds no other values */
void x_gcDummyMark( void *cargo )
{
   UNUSED( cargo );
}

/* Mark the values a node holds */
static void s_gcVisit( X_GC_NODE *node )
{
   if( node->kind == X_GC_ARRAY )
   {
      X_ARR *array = ( X_ARR * )node->value;
      int64_t i;

      for( i = 0; i < array->len; ++i )
         x_gcMarkItem( &array->items[i] );

      if( s_collectClassCycles && s_gcVisitMode == 2 && array->classHandle != 0 && !s_classInlineMarked )
      {
         s_classInlineMarked = 1;
         x_clsGCVisitInline();
      }
   }
   else if( node->kind == X_GC_HASH )
   {
      X_HASH *hash = ( X_HASH * )node->value;
      size_t i;

      for( i = 0; i < hash->len; ++i )
      {
         x_gcMarkItem( &hash->keys[i] );
         x_gcMarkItem( &hash->values[i] );
      }

      x_gcMarkItem( &hash->defaultValue );
   }
   else if( node->kind == X_GC_BOX )
   {
      X_BOX *box = ( X_BOX * )node->value;

      x_gcMarkItem( &box->item );

      if( box->owner != NULL )
         x_gcMark( box->owner );
   }
   else if( node->kind == X_GC_BLOCK )
   {
      X_BLOCK *block = ( X_BLOCK * )node->value;
      int i;

      for( i = 0; i < block->envLen; ++i )
         x_gcMark( block->env[i] );

      x_gcMarkItem( &block->self );

      if( s_collectClassCycles && s_gcVisitMode == 2 && block->context.definingClass != 0 && !s_classInlineMarked )
      {
         s_classInlineMarked = 1;
         x_clsGCVisitInline();
      }
   }
   else if( !node->finalized && node->functions != NULL && node->functions->mark != NULL )
      node->functions->mark( node->value );
}

/* Is the node an object whose class has a destructor? */
static int s_gcHasDestructor( const X_GC_NODE *node )
{
   if( node->kind != X_GC_ARRAY )
      return 0;

   return x_clsHasDestructor( ( ( const X_ARR * )node->value )->classHandle );
}

/* Mark everything reachable from the values held from outside */
static void s_gcMarkReachable( void )
{
   X_GC_NODE *node;
   int changed;

   for( node = s_gcNodes; node != NULL; node = node->next )
   {
      node->outside = *node->refs;
      node->marked = node->scanned = 0;
   }

   s_gcVisitMode = 1;
   for( node = s_gcNodes; node != NULL; node = node->next )
      s_gcVisit( node );

   if( s_collectClassCycles )
      x_clsGCVisitInline();

   for( node = s_gcNodes; node != NULL; node = node->next )
      node->marked = node->outside != 0 || node->locks != 0;

   s_gcVisitMode = 2;
   s_classInlineMarked = 0;
   do
   {
      changed = 0;
      for( node = s_gcNodes; node != NULL; node = node->next )
      {
         if( node->marked && !node->scanned )
         {
            node->scanned = 1;
            s_gcVisit( node );
            changed = 1;
         }
      }
   } while( changed );
   s_gcVisitMode = 0;
}

/* Run the destructor of one object */
static void s_gcFinalizeBody( void *cargo )
{
   X_GC_NODE *node = ( X_GC_NODE * )cargo;

   if( node->kind == X_GC_ARRAY )
   {
      X_ITEM object = X_ITEM_NIL_INIT;

      object.nType = X_IT_ARRAY;
      object.u.pArr = node->value;
      x_objDestructorCall( &object );
   }
   else
   {
      node->finalized = 1;
      if( node->functions != NULL && node->functions->release != NULL )
         node->functions->release( node->value );
   }
}

/* Run the destructors of the objects that are no longer reachable */
static void s_gcFinalize( X_GC_NODE *node )
{
   x_gcCleanup( s_gcFinalizeBody, node );
}

/* A destructor starts; 0 when it ran already */
int x_gcBeginDestructor( const X_ITEM *object )
{
   X_GC_NODE *node = IIF( object != NULL && object->nType == X_IT_ARRAY, s_gcFind( object->u.pArr ), NULL );

   if( node == NULL || node->finalized )
      return 0;

   node->finalized = 1;
   ++node->locks;

   return 1;
}

/* A destructor ended */
void x_gcEndDestructor( const X_ITEM *object )
{
   X_GC_NODE *node = IIF( object != NULL && object->nType == X_IT_ARRAY, s_gcFind( object->u.pArr ), NULL );

   if( node == NULL || node->locks == 0 )
      return;

   --node->locks;
   x_gcCollect();
}

/* Collect the values that are no longer reachable, also in cycles */
void x_gcCollect( void )
{
   X_GC_NODE *node;
   X_GC_NODE *next;
   int found;

   if( s_collectingGarbage || s_gcPauseDepth != 0 )
   {
      s_gcRequested = 1;
      return;
   }

   int previous __attribute__( ( cleanup( s_gcFinish ) ) ) = s_collectingGarbage;

   s_gcRequested = 0;
   s_gcReleases = 0;
   s_collectingGarbage = 1;

   for( ;; )
   {
      s_gcMarkReachable();
      for( node = s_gcNodes; node != NULL; node = node->next )
      {
         if( !node->marked && !node->finalized && s_gcHasDestructor( node ) )
            break;
      }

      if( node == NULL )
      {
         for( node = s_gcNodes; node != NULL; node = node->next )
         {
            if( !node->marked && !node->finalized && node->kind == X_GC_CARGO )
               break;
         }
      }

      if( node != NULL )
      {
         s_gcFinalize( node );
         continue;
      }

      found = 0;
      for( node = s_gcNodes; node != NULL; node = node->next )
      {
         node->dead = !node->marked;
         found |= node->dead;
      }

      if( !found )
         break;

      if( s_collectClassCycles )
         x_clsGCClearDeadInline();

      for( node = s_gcNodes; node != NULL; node = node->next )
      {
         if( node->dead )
            s_gcClearNode( node );
      }

      for( node = s_gcNodes; node != NULL; node = next )
      {
         next = node->next;
         if( node->dead )
         {
            s_gcUnlink( node );
            s_gcFreeNode( node );
         }
      }
   }
}

/* A block of the garbage collector with its release and mark functions */
void *x_gcAllocate( size_t size, const X_GC_FUNCS *functions )
{
   void *cargo = x_xgrabz( size );
   X_GC_NODE *node;

   node = x_gcRegister( cargo, NULL, X_GC_CARGO );
   node->functions = functions;
   node->locks = 1;
   node->initialLock = 1;

   return cargo;
}

/* Collect, also the objects held only by class data */
void x_gcCollectClasses( void )
{
   if( x_gcIsBusy() )
      return;

   int previous __attribute__( ( cleanup( s_classCyclesRestore ) ) ) = s_collectClassCycles;

   s_collectClassCycles = 1;
   x_gcCollect();
}

/* Is the block no longer reachable? */
int x_gcIsDeadBlock( const X_ITEM *item )
{
   X_GC_NODE *node = IIF( item != NULL && item->nType == X_IT_BLOCK, s_gcFind( item->u.pBlock ), NULL );

   return node != NULL && node->dead;
}

/* Is the garbage collector running? */
int x_gcIsBusy( void )
{
   return s_collectingGarbage || s_gcPauseDepth != 0;
}

/* Are objects or blocks of methods still alive? */
int x_gcHasObjects( void )
{
   const X_GC_NODE *node;

   for( node = s_gcNodes; node != NULL; node = node->next )
   {
      if( node->kind == X_GC_ARRAY && ( ( const X_ARR * )node->value )->classHandle != 0 )
         return 1;

      if( node->kind == X_GC_BLOCK && ( ( const X_BLOCK * )node->value )->context.definingClass != 0 )
         return 1;
   }

   return 0;
}

/* Hold a block from outside of the values */
void *x_gcLock( void *cargo )
{
   X_GC_NODE *node = s_gcFind( cargo );

   if( node == NULL || node->finalized )
      return NULL;

   ++node->locks;

   return cargo;
}

/* Stop holding a block from outside */
void *x_gcUnlock( void *cargo )
{
   X_GC_NODE *node = s_gcFind( cargo );

   if( node != NULL && node->locks != 0 )
   {
      --node->locks;
      if( node->locks == 0 )
         node->initialLock = 0;
   }

   return cargo;
}

/* Release a block of x_gcAllocate() */
void x_gcFree( void *cargo )
{
   X_GC_NODE *node = s_gcFind( cargo );

   if( node != NULL && node->kind == X_GC_CARGO && *node->refs == 0 )
   {
      if( node->initialLock )
      {
         --node->locks;
         node->initialLock = 0;
      }

      x_gcCollect();
   }
}

/* ---------------------------------------------------------------------------------------------
   String API
   --------------------------------------------------------------------------------------------- */

/* A to Z only */
static int s_charUpperAscii( int ch )
{
   return IIF( ch >= 'a' && ch <= 'z', ch - ( 'a' - 'A' ), ch );
}

/* A to z only */
static int s_charLowerAscii( int ch )
{
   return IIF( ch >= 'A' && ch <= 'Z', ch + ( 'a' - 'A' ), ch );
}

/* Space, tab, CR, LF, VT or FF */
static int s_charWhiteSpaceAscii( int ch )
{
   return ch == ' ' || ( ch >= '\t' && ch <= '\r' );
}

/* Compare without case (A to Z) */
int x_stricmp( const char *s1, const char *s2 )
{
   int ch1;
   int ch2;

   do
   {
      ch1 = s_charLowerAscii( ( unsigned char )*s1++ );
      ch2 = s_charLowerAscii( ( unsigned char )*s2++ );
   } while( ch1 == ch2 && ch1 != '\0' );

   return ch1 - ch2;
}

/* Compare length bytes without case (A to Z) */
int x_strnicmp( const char *s1, const char *s2, size_t length )
{
   int ch1 = 0;
   int ch2 = 0;

   while( length-- > 0 )
   {
      ch1 = s_charLowerAscii( ( unsigned char )*s1++ );
      ch2 = s_charLowerAscii( ( unsigned char )*s2++ );

      if( ch1 != ch2 || ch1 == '\0' )
         break;
   }

   return ch1 - ch2;
}

/* A to Z in place */
char *x_strupr( char *text )
{
   char *ptr = text;

   while( *ptr != '\0' )
   {
      *ptr = ( char )s_charUpperAscii( ( unsigned char )*ptr );
      ++ptr;
   }

   return text;
}

/* A to z in place */
char *x_strlow( char *text )
{
   char *ptr = text;

   while( *ptr != '\0' )
   {
      *ptr = ( char )s_charLowerAscii( ( unsigned char )*ptr );
      ++ptr;
   }

   return text;
}

/* The length of a string, at most maximum */
size_t x_strnlen( const char *text, size_t maximum )
{
   size_t length = 0;

   while( length < maximum && text[length] != '\0' )
      ++length;

   return length;
}

/* The length without trailing spaces (or white space with anySpace) */
size_t x_strRTrimLen( const char *text, size_t length, int anySpace )
{
   while( length > 0 )
   {
      unsigned char c = ( unsigned char )text[length - 1];

      if( anySpace && !s_charWhiteSpaceAscii( c ) )
         break;

      if( !anySpace && c != ' ' )
         break;

      --length;
   }

   return length;
}

/* The length without trailing spaces */
size_t x_strlentrim( const char *text )
{
   return x_strRTrimLen( text, strlen( text ), 0 );
}

/* A copy allocated by x_xgrab() */
char *x_strdup( const char *text )
{
   return x_strndup( text, strlen( text ) );
}

/* A copy of at most maximum bytes allocated by x_xgrab() */
char *x_strndup( const char *text, size_t maximum )
{
   size_t length = x_strnlen( text, maximum );
   char *copy = ( char * )x_xgrab( length + 1 );

   memcpy( copy, text, length );
   copy[length] = '\0';

   return copy;
}

/* A copy without leading and trailing spaces */
char *x_strduptrim( const char *text )
{
   return x_strndup( text, x_strlentrim( text ) );
}

/* Is the text only white space? */
int x_strEmpty( const char *text, size_t length )
{
   size_t i;

   for( i = 0; i < length; ++i )
   {
      if( !s_charWhiteSpaceAscii( ( unsigned char )text[i] ) )
         return 0;
   }

   return 1;
}

/* The position (from 1) of sub in text; 0 when not found */
size_t x_strAt( const char *sub, size_t subLen, const char *text, size_t textLen )
{
   const char *from;
   const char *last;

   if( subLen == 0 || subLen > textLen )
      return 0;

   from = text;
   last = text + ( textLen - subLen );
   while( from <= last && ( from = ( const char * )memchr( from, sub[0], ( size_t )( last - from ) + 1 ) ) != NULL )
   {
      if( memcmp( from + 1, sub + 1, subLen - 1 ) == 0 )
         return ( size_t )( from - text ) + 1;

      ++from;
   }

   return 0;
}

/* The position (from 1) of the last sub in text; 0 when not found */
size_t x_strRAt( const char *sub, size_t subLen, const char *text, size_t textLen )
{
   size_t i;

   if( subLen == 0 || subLen > textLen )
      return 0;

   for( i = textLen - subLen + 1; i > 0; --i )
   {
      if( memcmp( text + i - 1, sub, subLen ) == 0 )
         return i;
   }

   return 0;
}

/* x_strAt() without case */
size_t x_strAtI( const char *sub, size_t subLen, const char *text, size_t textLen )
{
   size_t i;
   size_t j;

   if( subLen == 0 || subLen > textLen )
      return 0;

   for( i = 0; i <= textLen - subLen; ++i )
   {
      for( j = 0; j < subLen; ++j )
      {
         if( s_charLowerAscii( ( unsigned char )text[i + j] ) != s_charLowerAscii( ( unsigned char )sub[j] ) )
            break;
      }

      if( j == subLen )
         return i + 1;
   }

   return 0;
}

/* Copy at most length bytes and a zero */
char *x_strncpy( char *dest, const char *source, size_t length )
{
   size_t i = 0;

   while( i < length && source[i] != '\0' )
   {
      dest[i] = source[i];
      ++i;
   }

   dest[i] = '\0';

   return dest;
}

/* Append at most up to length bytes in all */
char *x_strncat( char *dest, const char *source, size_t maximum )
{
   size_t length = x_strnlen( dest, maximum );

   if( length < maximum )
      x_strncpy( dest + length, source, maximum - length );
   else
      dest[maximum] = '\0';

   return dest;
}

/* Copy without trailing spaces */
char *x_strncpyTrim( char *dest, const char *source, size_t maximum )
{
   size_t length;

   x_strncpy( dest, source, maximum );
   length = x_strRTrimLen( dest, x_strnlen( dest, maximum ), 0 );
   dest[length] = '\0';

   return dest;
}

/* Copy in lower case (A to Z) */
char *x_strncpyLower( char *dest, const char *source, size_t length )
{
   size_t i = 0;

   while( i < length && source[i] != '\0' )
   {
      dest[i] = ( char )s_charLowerAscii( ( unsigned char )source[i] );
      ++i;
   }

   dest[i] = '\0';

   return dest;
}

/* Copy in upper case (a to z) */
char *x_strncpyUpper( char *dest, const char *source, size_t length )
{
   size_t i = 0;

   while( i < length && source[i] != '\0' )
   {
      dest[i] = ( char )s_charUpperAscii( ( unsigned char )source[i] );
      ++i;
   }

   dest[i] = '\0';

   return dest;
}

/* Copy in upper case without trailing spaces */
char *x_strncpyUpperTrim( char *dest, const char *source, size_t maximum )
{
   size_t length;

   x_strncpyUpper( dest, source, maximum );
   length = x_strRTrimLen( dest, x_strnlen( dest, maximum ), 0 );
   dest[length] = '\0';

   return dest;
}

/* The text without leading spaces; *length gets the new length */
const char *x_strLTrim( const char *text, size_t *length )
{
   while( *length > 0 && s_charWhiteSpaceAscii( ( unsigned char )*text ) )
   {
      ++text;
      --*length;
   }

   return text;
}

/* ---------------------------------------------------------------------------------------------
   Date API
   --------------------------------------------------------------------------------------------- */

/* Today in local time */
void x_dateToday( int *year, int *month, int *day )
{
   time_t now = time( NULL );
   struct tm *local = localtime( &now );

   *year = local->tm_year + 1900;
   *month = local->tm_mon + 1;
   *day = local->tm_mday;
}

/* A date as a Julian day number */
long x_dateEncode( int year, int month, int day )
{
   int a = ( 14 - month ) / 12;
   long y = ( long )year + 4800 - a;
   int m = month + 12 * a - 3;

   return day + ( 153 * m + 2 ) / 5 + 365 * y + y / 4 - y / 100 + y / 400 - 32045;
}

/* A Julian day number as year, month and day */
void x_dateDecode( long julian, int *year, int *month, int *day )
{
   long a, b, c, d, e, m;

   a = julian + 32044;
   b = ( 4 * a + 3 ) / 146097;
   c = a - ( 146097 * b ) / 4;
   d = ( 4 * c + 3 ) / 1461;
   e = c - ( 1461 * d ) / 4;
   m = ( 5 * e + 2 ) / 153;

   *day = ( int )( e - ( 153 * m + 2 ) / 5 + 1 );
   *month = ( int )( m + 3 - 12 * ( m / 10 ) );
   *year = ( int )( 100 * b + d - 4800 + m / 10 );
}

/* The day of the week of a Julian day: 1 Sunday ... 7 Saturday */
int x_dateJulianDOW( long julian )
{
   return ( int )( ( julian + 1 ) % 7 ) + 1;
}

/* The day of the week of a date: 1 Sunday ... 7 Saturday */
int x_dateDOW( int year, int month, int day )
{
   return x_dateJulianDOW( x_dateEncode( year, month, day ) );
}

/* The English name of a month (1 to 12) */
const char *x_dateCMonth( int month )
{
   static const char *names[12] = {
      "January", "February", "March", "April", "May", "June",
      "July", "August", "September", "October", "November", "December"
   };

   return IIF( month >= 1 && month <= 12, names[month - 1], "" );
}

/* The English name of a day of the week (1 Sunday to 7 Saturday) */
const char *x_dateCDOW( int day )
{
   static const char *names[7] = { "Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday" };

   return IIF( day >= 1 && day <= 7, names[day - 1], "" );
}

int x_setFixed;
int x_setExclusive = 1;
int x_setDeleted;
int x_setSoftSeek;
int x_setUnique;
int x_setCancel = 1; /* SET CANCEL: Alt+C ends the program */
int x_setInsert; /* SET INSERT: READ starts in insert mode */
int x_setExit; /* SET EXIT: Up on the first Get and Down on the last one end READ */
int x_setConfirm; /* SET CONFIRM: a full Get waits for Enter */
int x_setEscape = 1; /* SET ESCAPE: Esc ends READ */
int x_setDecimals = 2;
int x_setEpoch = 1900;
int x_prnRow; /* PRow(), set by SetPrc() */
int x_prnCol; /* PCol(), set by SetPrc() */
int x_dispOutAtSetPos = 1; /* DispOutAtSetPos(): DispOutAt() moves the cursor */
static char *s_default;

static char *s_path;

/* SET PATH; NULL only reads it */
const char *x_setPath( const char *path )
{
   if( path != NULL )
   {
      char *copy = x_strdup( path );

      if( s_path != NULL )
         x_xfree( s_path );

      s_path = copy;
   }

   return IIF( s_path != NULL, s_path, "" );
}

/* SET DEFAULT; NULL only reads it */
const char *x_setDefault( const char *path )
{
   if( path != NULL )
   {
      char *copy = x_strdup( path );

      if( s_default != NULL )
         x_xfree( s_default );

      s_default = copy;
   }

   return IIF( s_default != NULL, s_default, "" );
}

static char *s_dateFmt;
static int s_century;

/* SET DATE FORMAT; NULL only reads it */
const char *x_setDateFormat( const char *format )
{
   if( format != NULL )
   {
      char *copy = x_strdup( format );

      if( s_dateFmt != NULL )
         x_xfree( s_dateFmt );

      s_dateFmt = copy;
      int count = 0;

      while( *copy )
      {
         if( *copy == 'Y' || *copy == 'y' )
            ++count;
         else if( count )
            break;

         ++copy;
      }

      s_century = count >= 4;
   }

   return IIF( s_dateFmt != NULL, s_dateFmt, "MM/DD/YY" );
}

/* SET CENTURY; -1 only reads it */
int x_setCentury( int century )
{
   int old = s_century;

   if( century >= 0 && century != old )
   {
      const char *format = x_setDateFormat( NULL );
      size_t len = strlen( format ), start = 0, end;

      while( start < len && toupper( ( unsigned char )format[start] ) != 'Y' )
         ++start;

      if( start == len )
         start = 0;

      end = start;
      while( end < len && toupper( ( unsigned char )format[end] ) == 'Y' )
         ++end;

      size_t years = IIF( century, 4, 2 );
      char *next = ( char * )x_xgrab( len - ( end - start ) + years + 1 );

      memcpy( next, format, start );
      memset( next + start, 'Y', years );
      memcpy( next + start + years, format + end, len - end + 1 );
      x_strupr( next );
      x_setDateFormat( next );
      x_xfree( next );
   }

   return old;
}

/* The part of the date a letter of the format stands for: 1 year, 2 month, 4 day, 0 none */
static unsigned s_dateFormatBit( int ch )
{
   switch( ch )
   {
   case 'Y':
      return 1;

   case 'M':
      return 2;

   case 'D':
      return 4;

   default:
      return 0;
   }
}

/* Character i of a year of width letters (YYYYMMDD in date): the last digits; Y for a year wider than 4 */
static char s_dateYearChar( const char *date, size_t width, size_t i )
{
   if( width <= 4 )
      return date[4 - width + i];

   return IIF( i == 0, date[3], 'Y' );
}

/* DToC(): the date YYYYMMDD in the SET DATE format */
void x_dateFormat( const char *date, char *buffer, const char *format )
{
   size_t pos = 0;
   unsigned used = 0;

   while( *format && pos < 12 )
   {
      int ch = toupper( ( unsigned char )*format );
      size_t count = 1;

      while( format[count] && toupper( ( unsigned char )format[count] ) == ch )
         ++count;

      size_t width = IIF( count > 12 - pos, 12 - pos, count );
      unsigned bit = s_dateFormatBit( ch );

      for( size_t i = 0; i < width; ++i )
      {
         if( !bit || ( used & bit ) )
            buffer[pos++] = format[i];
         else if( ch == 'Y' )
            buffer[pos++] = s_dateYearChar( date, width, i );
         else
            buffer[pos++] = date[IIF( ch == 'M', 4, 6 ) + IIF( i + 1 == width, 1, 0 )];
      }

      used |= bit;
      format += count;
   }

   buffer[pos] = '\0';
}

/* CToD(): a text in a date format as a Julian day; 0 for an empty or wrong date */
long x_dateUnformat( const char *text, const char *format )
{
   int y = 0, m = 0, d = 0, yWidth = 0;

   if( text == NULL || format == NULL )
      return 0;

   while( *format )
   {
      char ch = *format;

      if( ch == 'Y' || ch == 'y' || ch == 'M' || ch == 'm' || ch == 'D' || ch == 'd' )
      {
         int fieldWidth = 0, value = 0, i;

         while( *format == ch )
         {
            ++format;
            ++fieldWidth;
         }

         for( i = 0; i < fieldWidth; ++i )
         {
            if( !*text || !isdigit( ( unsigned char )*text ) )
               return 0;

            value = value * 10 + ( *text - '0' );
            ++text;
         }

         if( ch == 'Y' || ch == 'y' )
         {
            y = value;
            yWidth = fieldWidth;
         }
         else if( ch == 'M' || ch == 'm' )
            m = value;
         else
            d = value;
      }
      else
      {
         ++format;
         if( *text )
            ++text;
      }
   }

   if( yWidth > 0 && yWidth <= 2 )
   {
      y += x_setEpoch / 100 * 100;
      if( y < x_setEpoch )
         y += 100;
   }

   if( m < 1 || m > 12 || d < 1 || y <= 0 )
      return 0;

   /* A day that does not exist in its month ( 02/31 ) decodes to another date. */
   {
      long julian = x_dateEncode( y, m, d );
      int yCheck, mCheck, dCheck;

      x_dateDecode( julian, &yCheck, &mCheck, &dCheck );
      return IIF( dCheck == d, julian, 0 );
   }
}

/* ---------------------------------------------------------------------------------------------
   Boxes
   --------------------------------------------------------------------------------------------- */

/* A new box for a variable passed by reference or captured by a block */
X_BOX *x_boxNew( void )
{
   X_BOX *box = ( X_BOX * )x_xgrab( sizeof( X_BOX ) );

   box->item = ( X_ITEM )X_ITEM_NIL_INIT;
   box->nRefs = 1;
   box->reference = NULL;
   box->owner = NULL;
   box->nativeType = 0;
   box->nativeValue = 0;
   box->nativeStorage = &box->nativeValue;
   box->nativeNext = NULL;
   x_gcRegister( box, &box->nRefs, X_GC_BOX );

   return box;
}

/* Add a reference to a box */
void x_boxRetain( X_BOX *box )
{
   ++box->nRefs;
}

/* The value of a box, or of the variable it refers to */
X_ITEM *x_boxItem( X_BOX *box )
{
   while( box->owner != NULL && box->nativeType == 0 )
      box = box->owner;

   if( box->nativeType != 0 )
   {
      x_boxNativeData( box );
      return &box->item;
   }

   return IIF( box->reference != NULL, box->reference, &box->item );
}

/* Bind a box to a value or, byReference, to a variable */
void x_boxBind( X_BOX *box, const X_ITEM *value, int byReference )
{
   if( byReference )
   {
      X_GC_NODE *node = s_gcFind( value );

      if( node != NULL && node->kind == X_GC_BOX )
      {
         box->owner = ( X_BOX * )node->value;
         x_boxRetain( box->owner );
      }
      else
         box->reference = ( X_ITEM * )value;
   }
   else
      x_itemCopy( &box->item, value );
}

/* The function of the variable returns: the box keeps its own copy */
void x_boxLeave( X_BOX *box )
{
   if( box->nativeType != 0 )
   {
      if( box->owner == NULL )
         box->reference = NULL;

      x_boxRelease( box );
      return;
   }

   if( box->reference != NULL )
   {
      X_ITEM *reference = box->reference;

      box->reference = NULL;
      x_itemCopy( &box->item, reference );
   }

   x_boxRelease( box );
}

/* The box of a value: the box of a reference, else a new box with a copy */
X_BOX *x_boxCapture( const X_ITEM *item )
{
   X_GC_NODE *node = s_gcFind( item );
   X_BOX *box;

   if( node != NULL && node->kind == X_GC_BOX )
   {
      box = ( X_BOX * )node->value;
      x_boxRetain( box );
      return box;
   }

   box = x_boxNew();
   x_itemCopy( &box->item, item );

   return box;
}

/* Drop a reference to a box */
void x_boxRelease( X_BOX *box )
{
   s_gcReleased( box, --box->nRefs );
}

/* Does an integer fit a C integer of width bits, signed or unsigned? */
static int s_nativeFits( int64_t number, int width, int unsign )
{
   if( unsign )
   {
      if( number < 0 )
         return 0;

      return width >= 64 || ( uint64_t )number <= ( UINT64_C( 1 ) << width ) - 1;
   }

   if( width >= 64 )
      return 1;

   return number >= -( INT64_C( 1 ) << ( width - 1 ) ) && number < ( INT64_C( 1 ) << ( width - 1 ) );
}

/* An xDev value as the bits of a C type (type: width and signedness); error 3105 when it does not fit */
static uint64_t s_nativeBits( int type, const X_ITEM *value )
{
   const int width = type & 0xff;
   const int real = type & 0x200;
   const int unsign = type & 0x100;
   uint64_t bits = 0;

   X_ITEM error __attribute__( ( cleanup( x_itemClear ) ) ) = X_ITEM_NIL_INIT;

   if( type == ( 0x800 | 64 ) )
   {
      if( value->nType == X_IT_NIL )
         return 0;

      if( value->nType != X_IT_POINTER )
         x_errorRaiseNamed( &error, 3107, "typed reference" );
      return ( uintptr_t )value->u.pPointer;
   }

   if( type == 0x400 )
   {
      if( value->nType != X_IT_STRING )
         x_errorRaiseNamed( &error, 3107, "typed reference" );
      return 0;
   }

   if( type == 1 )
   {
      if( value->nType != X_IT_LOGICAL )
         x_errorRaiseNamed( &error, 3107, "typed reference" );
      return value->u.logical != 0;
   }

   if( !X_IS_NUMERIC( value ) )
      x_errorRaiseNamed( &error, 3107, "typed reference" );

   if( real )
   {
      double number = x_itemAsDouble( value );

      if( width == 32 )
      {
         float small = ( float )number;
         uint32_t word;

         memcpy( &word, &small, sizeof( word ) );
         bits = word;
      }
      else
         memcpy( &bits, &number, sizeof( bits ) );
   }
   else if( value->nType == X_IT_DOUBLE )
   {
      double whole = trunc( value->u.dbl );
      double upper = ldexp( 1.0, width - IIF( unsign, 0, 1 ) );

      if( !( whole >= IIF( unsign, 0.0, -upper ) && whole < upper ) )
         x_errorRaiseNamed( &error, 3105, "typed reference" );

      bits = IIF( unsign, ( uint64_t )whole, ( uint64_t )( int64_t )whole );
   }
   else if( value->nType == X_IT_UINT64 )
   {
      uint64_t maximum;

      bits = value->u.unsignedInteger;
      if( !unsign )
         maximum = ( UINT64_C( 1 ) << ( width - 1 ) ) - 1;
      else if( width == 64 )
         maximum = UINT64_MAX;
      else
         maximum = ( UINT64_C( 1 ) << width ) - 1;

      if( bits > maximum )
         x_errorRaiseNamed( &error, 3105, "typed reference" );
   }
   else
   {
      int64_t number = value->u.integer;

      if( !s_nativeFits( number, width, unsign ) )
         x_errorRaiseNamed( &error, 3105, "typed reference" );

      bits = ( uint64_t )number;
   }

   return bits;
}

/* The bits of a C type as an xDev value */
static X_ITEM s_nativeItem( int type, uint64_t bits )
{
   X_ITEM value = X_ITEM_NIL_INIT;
   int width = type & 0xff;

   if( type == ( 0x800 | 64 ) )
   {
      value.nType = X_IT_POINTER;
      value.u.pPointer = ( void * )( uintptr_t )bits;
   }
   else if( type == 1 )
   {
      value.nType = X_IT_LOGICAL;
      value.u.logical = bits != 0;
   }
   else if( type & 0x200 )
   {
      value.nType = X_IT_DOUBLE;
      if( width == 32 )
      {
         uint32_t word = ( uint32_t )bits;
         float real;

         memcpy( &real, &word, sizeof( real ) );
         value.u.dbl = real;
      }
      else
         memcpy( &value.u.dbl, &bits, sizeof( bits ) );
   }
   else
   {
      value.nType = X_IT_INTEGER;
      if( width < 64 )
      {
         uint64_t mask = ( UINT64_C( 1 ) << width ) - 1;

         bits &= mask;
         if( !( type & 0x100 ) && ( bits & ( UINT64_C( 1 ) << ( width - 1 ) ) ) )
            bits |= ~mask;
      }

      if( type == ( 0x100 | 64 ) )
      {
         value.nType = X_IT_UINT64;
         value.u.unsignedInteger = bits;
      }
      else
         value.u.integer = ( int64_t )bits;
   }

   return value;
}

/* A box for an @ argument of a DLL call of a C type */
X_BOX *x_boxNativeNew( int type )
{
   X_BOX *box = x_boxNew();

   box->nativeType = type;
   box->nativeNext = s_nativeBoxes;
   s_nativeBoxes = box;
   if( type == 0x400 )
   {
      box->item.nType = X_IT_STRING;
      box->nativeStorage = &box->item.u.string;
      x_stringStore( &box->item.u.string, NULL, 0 );
   }
   else
      box->item = s_nativeItem( type, 0 );

   return box;
}

/* Replace the text of a string buffer */
void x_stringStore( X_STRING *storage, const char *data, size_t length )
{
   char *copy = ( char * )x_xgrab( length + 1 );

   if( length != 0 )
      memcpy( copy, data, length );

   copy[length] = '\0';
   x_xrelease( storage->data );
   storage->data = copy;
   storage->len = length;
}

/* Append to a string in place when it is the only owner of its buffer; the buffer grows with a
   reserve, so repeated appends stay linear. Returns 0 and changes nothing when the buffer is shared. */
int x_stringAppend( X_STRING *storage, const char *data, size_t length )
{
   size_t need;
   size_t size;

   if( storage->data == NULL || x_xrefs( storage->data ) != 1 || length > SIZE_MAX / 2 - storage->len )
      return 0;

   need = storage->len + length + 1;
   size = x_xsize( storage->data );
   if( need > size )
      storage->data = ( char * )x_xrealloc( storage->data, IIF( size + size / 2 > need, size + size / 2, need ) );

   memcpy( storage->data + storage->len, data, length );
   storage->len += length;
   storage->data[storage->len] = '\0';

   return 1;
}

/* Put the text a DLL call wrote into the box */
void x_boxNativeStringStore( X_BOX *box, const char *data, size_t length )
{
   X_ITEM owned __attribute__( ( cleanup( x_itemClear ) ) ) = { X_IT_STRING, { 0 } };

   x_stringStore( &owned.u.string, data, length );
   if( box->reference != NULL )
      x_itemCopy( box->reference, &owned );

   if( box->nativeStorage != &box->item.u.string )
      x_stringStore( box->nativeStorage, owned.u.string.data, owned.u.string.len );

   x_itemClear( &box->item );
   box->item = owned;
   owned = ( X_ITEM )X_ITEM_NIL_INIT;
}

/* Put the value a DLL call wrote into the box */
void x_boxNativeStore( X_BOX *box, uint64_t bits )
{
   X_ITEM value = s_nativeItem( box->nativeType, bits );

   if( box->reference != NULL )
   {
      x_itemCopy( box->reference, &value );
   }

   size_t size = IIF( box->nativeType == 1, 1, ( box->nativeType & 0xff ) / 8 );

   memcpy( box->nativeStorage, &bits, size );
   box->nativeValue = bits;
   box->item = value;
}

/* The C storage of a native box for the DLL call */
void *x_boxNativeData( X_BOX *box )
{
   if( box->nativeType == 0x400 )
   {
      const X_ITEM *source = IIF( box->owner != NULL, x_boxItem( box->owner ), box->reference );
      X_ITEM value = { X_IT_STRING, { 0 } };

      if( source == NULL && box->nativeStorage != &box->item.u.string )
      {
         value.u.string = *( X_STRING * )box->nativeStorage;
         source = &value;
      }

      if( source != NULL )
      {
         s_nativeBits( box->nativeType, source );
         X_ITEM copy = X_ITEM_NIL_INIT;

         x_itemCopy( &copy, source );
         x_itemClear( &box->item );
         box->item = copy;
      }
      return &box->item.u.string;
   }

   if( box->reference != NULL )
   {
      const X_ITEM *source = IIF( box->owner != NULL, x_boxItem( box->owner ), box->reference );

      box->nativeValue = s_nativeBits( box->nativeType, source );
   }
   else
   {
      uint64_t bits = 0;
      size_t size = IIF( box->nativeType == 1, 1, ( box->nativeType & 0xff ) / 8 );

      memcpy( &bits, box->nativeStorage, size );
      box->nativeValue = bits;
   }

   box->item = s_nativeItem( box->nativeType, box->nativeValue );

   return box->nativeStorage;
}

/* Bind a native box to a value or a variable */
void x_boxNativeBind( X_BOX *box, const X_ITEM *value, int byReference )
{
   uint64_t bits = s_nativeBits( box->nativeType, value );

   if( byReference )
   {
      X_GC_NODE *node = s_gcFind( value );

      if( node != NULL && node->kind == X_GC_BOX )
      {
         X_BOX *owner = ( X_BOX * )node->value;

         if( owner->nativeType != 0 && owner->nativeType != box->nativeType )
         {
            X_ITEM error __attribute__( ( cleanup( x_itemClear ) ) ) = X_ITEM_NIL_INIT;

            x_errorRaiseNamed( &error, 3107, "typed reference type mismatch" );
         }

         box->owner = owner;
         x_boxRetain( owner );
      }

      box->reference = ( X_ITEM * )value;
   }

   if( box->nativeType == 0x400 )
      x_boxNativeStringStore( box, value->u.string.data, value->u.string.len );
   else
   {
      box->nativeValue = bits;
      box->item = s_nativeItem( box->nativeType, bits );
   }
}

/* The storage of a STRUCT for a native box */
void x_boxNativeStorage( X_BOX *box, void *storage )
{
   box->nativeStorage = storage;
}

/* ---------------------------------------------------------------------------------------------
   Code blocks
   --------------------------------------------------------------------------------------------- */

/* Add a reference to a block */
static void s_blockRetain( X_BLOCK *block )
{
   ++block->nRefs;
}

/* Drop a reference to a block */
static void s_blockRelease( X_BLOCK *block )
{
   s_gcReleased( block, --block->nRefs );
}

/* Release a block: its captured boxes and self */
static void s_blockDrop( X_BLOCK **block )
{
   s_blockRelease( *block );
}

/* A new block of function with the boxes it captures */
static void s_itemBlockInto( X_CALL_CTX *ctx, X_ITEM *destination, X_BLOCKFUNC function, X_ITEM *const *env, int envLen,
                             int lexical )
{
   X_BLOCK *block;
   int i;

   if( envLen < 0 )
      return;

   block = ( X_BLOCK * )x_xgrab( sizeof( X_BLOCK ) );
   block->nRefs = 1;
   block->pFunc = function;
   block->envLen = envLen;
   block->env = NULL;
   block->lexical = lexical;
   block->context = ( X_METHOD_CONTEXT ){ 0 };
   block->self = ( X_ITEM )X_ITEM_NIL_INIT;
   if( lexical )
   {
      const X_METHOD_CONTEXT *method = x_objMethodContext( ctx );

      if( method != NULL )
      {
         block->context = *method;
         block->context.previous = NULL;
         block->context.self = NULL;
         block->context.attemptedMessage = NULL;
         if( method->self != NULL )
            x_itemCopy( &block->self, method->self );
      }
   }

   if( envLen > 0 )
   {
      block->env = ( X_ITEM ** )x_xgrabArray( ( size_t )envLen, sizeof( X_ITEM * ) );

      for( i = 0; i < envLen; ++i )
      {
         block->env[i] = env[i];
         x_boxRetain( ( X_BOX * )env[i] );
      }
   }

   x_gcRegister( block, &block->nRefs, X_GC_BLOCK );
   x_itemReplace( destination, ( X_ITEM ){ X_IT_BLOCK, { .pBlock = block } } );
}

/* A new block */
void x_itemBlockInto( X_CALL_CTX *ctx, X_ITEM *destination, X_BLOCKFUNC function, X_ITEM *const *env, int envLen )
{
   s_itemBlockInto( ctx, destination, function, env, envLen, 0 );
}

/* A new block made in a method: it keeps the method context */
void x_itemClosureInto( X_CALL_CTX *ctx, X_ITEM *destination, X_BLOCKFUNC function, X_ITEM *const *env, int envLen )
{
   s_itemBlockInto( ctx, destination, function, env, envLen, 1 );
}

/* Eval() of a block */
void x_evalBlock( X_CALL_CTX *ctx, X_ITEM *result, const X_ITEM *pBlock, int32_t argc, const X_ARG *args )
{
   X_BLOCK *block;
   int32_t i;

   if( result == NULL )
      return;

   if( pBlock == NULL || pBlock->nType != X_IT_BLOCK || pBlock->u.pBlock == NULL )
   {
      x_errorRaiseNamed( result, 3000, "block evaluation" );
      return;
   }

   if( argc < 0 || ( argc > 0 && args == NULL ) )
   {
      x_errorRaiseNamed( result, 3003, "block evaluation" );
      return;
   }

   for( i = 0; i < argc; ++i )
   {
      if( args[i].value == NULL )
      {
         x_errorRaiseNamed( result, 3003, "block evaluation" );
         return;
      }
   }

   block = ( X_BLOCK * )pBlock->u.pBlock;
   s_blockRetain( block );
   X_BLOCK *held __attribute__( ( cleanup( s_blockDrop ) ) ) = block;

   x_itemClear( result );
   if( block->lexical )
      x_objEvalClosure( block->pFunc, result, argc, args, block->env, &block->context, &block->self );
   else
   {
      X_CALL_CTX call = { args, argc, result, ctx->self, ctx->methodContext };

      block->pFunc( &call, block->env );
   }
}

/* ---------------------------------------------------------------------------------------------
   Items
   --------------------------------------------------------------------------------------------- */

/* Clear an item and put a value into it */
void x_itemReplace( X_ITEM *destination, X_ITEM value )
{
   unsigned int depth __attribute__( ( cleanup( x_gcRestore ) ) ) = s_gcPauseDepth++;

   for( X_BOX *box = s_nativeBoxes; box != NULL; box = box->nativeNext )
   {
      if( &box->item == destination )
      {
         X_ITEM owned __attribute__( ( cleanup( x_itemClear ) ) ) = value;
         uint64_t bits = s_nativeBits( box->nativeType, &owned );

         if( box->nativeType == 0x400 )
            x_boxNativeStringStore( box, owned.u.string.data, owned.u.string.len );
         else
            x_boxNativeStore( box, bits );
         return;
      }
   }

   x_itemClear( destination );
   *destination = value;
}

/* A number as a double; 0 for other values */
double x_itemAsDouble( const X_ITEM *item )
{
   if( item == NULL )
      return 0.0;

   if( item->nType == X_IT_DOUBLE )
      return item->u.dbl;

   if( item->nType == X_IT_INTEGER )
      return ( double )item->u.integer;

   if( item->nType == X_IT_UINT64 )
      return ( double )item->u.unsignedInteger;

   return 0.0;
}

/* A number as an integer; 0 for other values */
int64_t x_itemAsInt64( const X_ITEM *item )
{
   if( item == NULL )
      return 0;

   if( item->nType == X_IT_INTEGER )
      return item->u.integer;

   if( item->nType == X_IT_UINT64 )
   {
      if( item->u.unsignedInteger > INT64_MAX )
      {
         X_ITEM error __attribute__( ( cleanup( x_itemClear ) ) ) = X_ITEM_NIL_INIT;

         x_errorRaiseNamed( &error, 3105, "UINT64 to INT64" );
      }
      return ( int64_t )item->u.unsignedInteger;
   }

   if( item->nType == X_IT_DOUBLE )
      return ( int64_t )item->u.dbl;

   return 0;
}

static int s_itemSetInteger( X_ITEM *item, int64_t value )
{
   if( item == NULL )
      return 0;

   x_itemReplace( item, ( X_ITEM ){ X_IT_INTEGER, { .integer = value } } );

   return 1;
}

static int s_itemSetDate( X_ITEM *item, long julian )
{
   if( item == NULL )
      return 0;

   x_itemReplace( item, ( X_ITEM ){ X_IT_DATE, { .integer = julian } } );

   return 1;
}

static int s_itemSetDouble( X_ITEM *item, double value )
{
   if( item == NULL )
      return 0;

   x_itemReplace( item, ( X_ITEM ){ X_IT_DOUBLE, { .dbl = value } } );

   return 1;
}

static int s_itemSetLogical( X_ITEM *item, int value )
{
   if( item == NULL )
      return 0;

   x_itemReplace( item, ( X_ITEM ){ X_IT_LOGICAL, { .logical = IIF( value, 1, 0 ) } } );

   return 1;
}

static int s_itemSetString( X_ITEM *item, const char *value, size_t length )
{
   char *copy;

   if( item == NULL )
      return 0;

   if( value == NULL )
   {
      value = "";
      length = 0;
   }

   copy = ( char * )x_xgrab( length + 1 );
   memcpy( copy, value, length );
   copy[length] = '\0';
   x_itemReplace( item, ( X_ITEM ){ X_IT_STRING, { .string = { copy, length } } } );

   return 1;
}

static int s_itemSetStringBuffer( X_ITEM *item, char *value, size_t length )
{
   if( item == NULL )
      return 0;

   x_itemPutStringBuffer( item, value, length );

   return 1;
}

/* Assign a value to a typed or untyped item; 0 after an error */
int x_itemAssign( X_CALL_CTX *ctx, X_ITEM *destination, const X_ITEM *source )
{
   X_ITEM result __attribute__( ( cleanup( x_itemClear ) ) ) = X_ITEM_NIL_INIT;

   if( destination == NULL || source == NULL )
      return 0;

   if( x_objHasOperator( destination, X_OO_OP_ASSIGN ) )
   {
      int success = x_objOperatorCall( ctx, X_OO_OP_ASSIGN, &result, destination, source, NULL );

      if( success )
         x_itemCopy( destination, &result );

      x_itemClear( &result );
      return success;
   }

   x_itemCopy( destination, source );

   return 1;
}

/* Release the value of an item; the item becomes NIL */
void x_itemClear( X_ITEM *item )
{
   X_ITEM old;

   old = *item;
   *item = ( X_ITEM )X_ITEM_NIL_INIT;

   switch( old.nType )
   {
   case X_IT_STRING:
      x_xrelease( old.u.string.data );
      break;

   case X_IT_ARRAY:
      s_gcReleased( old.u.pArr, --( ( X_ARR * )old.u.pArr )->nRefs );
      break;

   case X_IT_HASH:
      s_gcReleased( old.u.pPointer, --( ( X_HASH * )old.u.pPointer )->nRefs );
      break;

   case X_IT_BLOCK:
      s_blockRelease( ( X_BLOCK * )old.u.pBlock );
      break;

   case X_IT_GCPOINTER:
      s_gcReleased( old.u.pPointer, --*s_gcFind( old.u.pPointer )->refs );
      break;

   default:
      break;
   }
}

/* Copy a value into an item; references are counted */
void x_itemCopy( X_ITEM *destination, const X_ITEM *source )
{
   X_ITEM copy = X_ITEM_NIL_INIT;

   if( destination == source )
      return;

   switch( source->nType )
   {
   case X_IT_LOGICAL:
      copy.nType = X_IT_LOGICAL;
      copy.u.logical = source->u.logical;
      break;

   case X_IT_INTEGER:
   case X_IT_UINT64:
      copy.nType = source->nType;
      copy.u.unsignedInteger = source->u.unsignedInteger;
      break;

   case X_IT_DOUBLE:
      copy.nType = X_IT_DOUBLE;
      copy.u.dbl = source->u.dbl;
      break;

   case X_IT_DATE:
      copy.nType = X_IT_DATE;
      copy.u.integer = source->u.integer;
      break;

   case X_IT_STRING:
      copy.nType = X_IT_STRING;
      copy.u.string.len = source->u.string.len;
      if( source->u.string.data != NULL )
         copy.u.string.data = ( char * )x_xretain( source->u.string.data );
      else
         copy.u.string.data = ( char * )x_xmemdup( "", 1 );
      break;

   case X_IT_ARRAY:
      copy.nType = X_IT_ARRAY;
      copy.u.pArr = source->u.pArr;
      ++( ( X_ARR * )copy.u.pArr )->nRefs;
      break;

   case X_IT_HASH:
      copy.nType = X_IT_HASH;
      copy.u.pPointer = source->u.pPointer;
      ++( ( X_HASH * )copy.u.pPointer )->nRefs;
      break;

   case X_IT_BLOCK:
      copy.nType = X_IT_BLOCK;
      copy.u.pBlock = source->u.pBlock;
      s_blockRetain( ( X_BLOCK * )copy.u.pBlock );
      break;

   case X_IT_POINTER:
      copy.nType = X_IT_POINTER;
      copy.u.pPointer = source->u.pPointer;
      break;

   case X_IT_GCPOINTER:
      copy = *source;
      ++*s_gcFind( copy.u.pPointer )->refs;
      break;

   default:
      break;
   }

   x_itemReplace( destination, copy );
}

/* A new item allocated by x_xgrab(), with a copy of source or NIL */
X_ITEM *x_itemNew( const X_ITEM *source )
{
   X_ITEM *item = ( X_ITEM * )x_xgrab( sizeof( X_ITEM ) );

   *item = ( X_ITEM )X_ITEM_NIL_INIT;
   if( source != NULL )
      x_itemCopy( item, source );

   return item;
}

/* The item to fill; a new one for NULL */
static X_ITEM *s_itemTarget( X_ITEM *item )
{
   return IIF( item != NULL, item, x_itemNew( NULL ) );
}

/* Put a logical value */
X_ITEM *x_itemPutL( X_ITEM *item, int value )
{
   item = s_itemTarget( item );
   s_itemSetLogical( item, value );

   return item;
}

/* Put an integer */
X_ITEM *x_itemPutNI( X_ITEM *item, int value )
{
   return x_itemPutNInt( item, value );
}

/* Put an integer */
X_ITEM *x_itemPutNInt( X_ITEM *item, int64_t value )
{
   item = s_itemTarget( item );
   s_itemSetInteger( item, value );

   return item;
}

/* Put a double */
X_ITEM *x_itemPutND( X_ITEM *item, double value )
{
   item = s_itemTarget( item );
   s_itemSetDouble( item, value );

   return item;
}

/* A logical value; 0 for other values */
int x_itemGetL( const X_ITEM *item )
{
   return item != NULL && item->nType == X_IT_LOGICAL && item->u.logical;
}

/* A number as int */
int x_itemGetNI( const X_ITEM *item )
{
   return ( int )x_itemGetNInt( item );
}

/* A number as int64_t */
int64_t x_itemGetNInt( const X_ITEM *item )
{
   if( item == NULL )
      return 0;

   if( item->nType == X_IT_INTEGER )
      return item->u.integer;

   if( item->nType == X_IT_UINT64 )
      return ( int64_t )item->u.unsignedInteger;

   if( item->nType == X_IT_DOUBLE )
      return ( int64_t )item->u.dbl;

   return 0;
}

/* A number as double */
double x_itemGetND( const X_ITEM *item )
{
   if( item == NULL )
      return 0.0;

   if( item->nType == X_IT_DOUBLE )
      return item->u.dbl;

   if( item->nType == X_IT_UINT64 )
      return ( double )item->u.unsignedInteger;

   return IIF( item->nType == X_IT_INTEGER, ( double )item->u.integer, 0.0 );
}

/* Put a copy of a C string */
X_ITEM *x_itemPutC( X_ITEM *item, const char *value )
{
   return x_itemPutCL( item, value, IIF( value != NULL, strlen( value ), 0 ) );
}

/* Put a copy of length bytes */
X_ITEM *x_itemPutCL( X_ITEM *item, const char *value, size_t length )
{
   item = s_itemTarget( item );
   s_itemSetString( item, value, length );

   return item;
}

/* Put a C string allocated by x_xgrab(); the item takes it */
X_ITEM *x_itemPutCPtr( X_ITEM *item, char *value )
{
   return x_itemPutCLPtr( item, value, IIF( value != NULL, strlen( value ), 0 ) );
}

/* Put length bytes allocated by x_xgrab(); the item takes them */
X_ITEM *x_itemPutCLPtr( X_ITEM *item, char *value, size_t length )
{
   item = s_itemTarget( item );
   x_itemPutStringBuffer( item, value, length );

   return item;
}

/* Put a copy of a UTF-8 string */
X_ITEM *x_itemPutStrUTF8( X_ITEM *item, const char *value )
{
   return x_itemPutC( item, value );
}

/* The text of a string; "" for other values */
const char *x_itemGetCPtr( const X_ITEM *item )
{
   return IIF( item != NULL && item->nType == X_IT_STRING, item->u.string.data, "" );
}

/* The length of a string; 0 for other values */
size_t x_itemGetCLen( const X_ITEM *item )
{
   return IIF( item != NULL && item->nType == X_IT_STRING, item->u.string.len, 0 );
}

/* The UTF-8 text of a string; the handle is released by x_strfree() */
const char *x_itemGetStrUTF8( const X_ITEM *item, void **handle, size_t *length )
{
   const char *text = x_itemGetCPtr( item );
   size_t size = x_itemGetCLen( item );

   if( length != NULL )
      *length = size;

   if( handle != NULL )
   {
      char *copy = ( char * )x_xgrab( size + 1 );

      memcpy( copy, text, size );
      copy[size] = '\0';
      *handle = copy;
      return copy;
   }

   return text;
}

/* Release the handle of x_itemGetStrUTF8() */
void x_strfree( void *handle )
{
   if( handle != NULL )
      x_xfree( handle );
}

/* Put a pointer that the item does not own */
X_ITEM *x_itemPutPtr( X_ITEM *item, void *value )
{
   item = s_itemTarget( item );
   x_itemReplace( item, ( X_ITEM ){ X_IT_POINTER, { .pPointer = value } } );

   return item;
}

/* A pointer; NULL for other values */
void *x_itemGetPtr( const X_ITEM *item )
{
   return IIF( item != NULL && X_IS_POINTER( item ), item->u.pPointer, NULL );
}

/* Put a block of the garbage collector */
X_ITEM *x_itemPutPtrGC( X_ITEM *item, void *cargo )
{
   X_GC_NODE *node = s_gcFind( cargo );
   X_ITEM value = X_ITEM_NIL_INIT;

   if( node == NULL || node->kind != X_GC_CARGO || node->finalized )
   {
      if( item != NULL )
         x_errorRaiseNamed( item, 3000, "managed pointer" );
      return NULL;
   }

   if( item == NULL )
      item = x_itemNew( NULL );

   ++*node->refs;
   if( node->initialLock )
   {
      --node->locks;
      node->initialLock = 0;
   }

   value.nType = X_IT_GCPOINTER;
   value.u.pPointer = cargo;
   x_itemReplace( item, value );

   return item;
}

/* The block of the garbage collector with these functions; NULL for other values */
void *x_itemGetPtrGC( const X_ITEM *item, const X_GC_FUNCS *functions )
{
   X_GC_NODE *node;

   if( item == NULL || item->nType != X_IT_GCPOINTER )
      return NULL;

   node = s_gcFind( item->u.pPointer );

   return IIF( node != NULL && !node->finalized && ( functions == NULL || node->functions == functions ), node->value,
               NULL );
}

/* Release an item of x_itemNew() */
void x_itemRelease( X_ITEM *item )
{
   if( item != NULL )
   {
      x_itemClear( item );
      x_xfree( item );
   }
}

/* Return a copy of a value */
void x_itemReturn( X_CALL_CTX *ctx, const X_ITEM *item )
{
   if( item != NULL )
      x_itemCopy( ctx->result, item );
   else
      x_ret( ctx );
}

/* The length of a string; 0 for other values */
size_t x_itemStringLength( const X_ITEM *item )
{
   return IIF( item->nType == X_IT_STRING, item->u.string.len, 0 );
}

/* Put a buffer of x_xgrab() with a zero after length bytes; the item takes it */
void x_itemPutStringBuffer( X_ITEM *item, char *value, size_t length )
{
   if( value == NULL )
   {
      value = ( char * )x_xgrab( 1 );
      value[0] = '\0';
      length = 0;
   }
   else
   {
      value = ( char * )x_xrealloc( value, length + 1 );
      value[length] = '\0';
   }

   x_itemReplace( item, ( X_ITEM ){ X_IT_STRING, { .string = { value, length } } } );
}

/* A logical value into *result; 0 for other values */
int x_itemLogical( const X_ITEM *value, int *result )
{
   if( value->nType != X_IT_LOGICAL )
      return 0;

   *result = value->u.logical != 0;

   return 1;
}

/* ---------------------------------------------------------------------------------------------
   Errors
   --------------------------------------------------------------------------------------------- */

/* Set the error handler of C code; returns the previous one */
X_ERROR_HANDLER x_errorSetHandler( X_ERROR_HANDLER handler )
{
   X_ERROR_HANDLER previous = s_errorHandler;

   s_errorHandler = handler;

   return previous;
}

/* Is an error waiting? */
int x_errorCaught( void )
{
   return s_break || s_caught != NULL;
}

/* The error that is waiting; NULL without one */
const X_ERROR *x_errorPeek( void )
{
   return IIF( s_caught != NULL, s_caught, s_activeError );
}

/* Release an error of x_errorClone() */
void x_errorFree( X_ERROR *error )
{
   if( error != NULL )
   {
      x_itemClear( &error->arguments[0] );
      x_itemClear( &error->arguments[1] );
      x_itemClear( &error->object );
      x_xfree( error );
   }
}

/* The cleanup of an error */
static void s_errorDrop( X_ERROR **error )
{
   x_errorFree( *error );
}

/* The bytes of the three texts of a definition with their zeros; 0 for NULL */
static size_t s_errorTextsSize( const X_ERROR_DEF *def )
{
   if( def == NULL )
      return 0;

   return strlen( def->subSystem ) + strlen( def->description ) + strlen( def->operation ) + 3;
}

/* A copy of an error with its definition and texts in one block */
X_ERROR *x_errorClone( const X_ERROR *error )
{
   const X_ERROR_DEF *def = error->definition;
   size_t len = s_errorTextsSize( def );
   X_ERROR *copy = ( X_ERROR * )x_xgrab( sizeof( X_ERROR ) + sizeof( X_ERROR_DEF ) + len );

   *copy = *error;
   copy->arguments[0] = ( X_ITEM )X_ITEM_NIL_INIT;
   copy->arguments[1] = ( X_ITEM )X_ITEM_NIL_INIT;
   copy->object = ( X_ITEM )X_ITEM_NIL_INIT;
   if( def != NULL )
   {
      X_ERROR_DEF *owned = ( X_ERROR_DEF * )( copy + 1 );
      char *text = ( char * )( owned + 1 );

      *owned = *def;
      size_t length = strlen( def->subSystem ) + 1;

      memcpy( text, def->subSystem, length );
      owned->subSystem = text;
      text += length;
      length = strlen( def->description ) + 1;
      memcpy( text, def->description, length );
      owned->description = text;
      text += length;
      length = strlen( def->operation ) + 1;
      memcpy( text, def->operation, length );
      owned->operation = text;
      copy->definition = owned;
   }

   x_itemCopy( &copy->arguments[0], &error->arguments[0] );
   x_itemCopy( &copy->arguments[1], &error->arguments[1] );
   x_itemCopy( &copy->object, &error->object );

   return copy;
}

/* Forget the error and the BREAK that wait */
void x_errorClear( void )
{
   do
   {
      X_ERRCTX saved = s_errorSave();

      s_activeError = saved.active;
      x_errorFree( saved.caught );
      x_itemClear( &saved.value );
   } while( x_errorCaught() );
}

/* Keep a BREAK to raise it again later */
void x_breakPut( X_ITEM *value )
{
   X_ERRCTX saved = s_errorSave();

   s_activeError = saved.active;
   s_breakValue = *value;
   *value = ( X_ITEM )X_ITEM_NIL_INIT;
   s_break = 1;
   x_errorFree( saved.caught );
   x_itemClear( &saved.value );
}

/* Keep an error to raise it again later */
void x_errorPut( X_ERROR *error )
{
   X_ERRCTX saved = s_errorSave();

   s_activeError = saved.active;
   s_caught = error;
   x_errorFree( saved.caught );
   x_itemClear( &saved.value );
}

static _Thread_local X_TRACE *s_trace;

/* A function starts: its frame for ProcName() and the error report */
void x_traceEnter( X_TRACE *frame, const char *function )
{
   frame->prev = s_trace;
   frame->location = NULL;
   frame->function = function;
   s_trace = frame;
}

/* A function ends */
void x_traceLeave( X_TRACE *frame )
{
   s_trace = frame->prev;
}

/* Report the error that waits on stderr, with the place it was raised at */
void x_errorReport( void )
{
   if( s_errorReported || ( !s_break && s_caught == NULL ) )
      return;

   /* The report starts on its own line, also after QQOut() or QOut() text without a trailing newline. */
   if( s_outputMidLine )
   {
      fflush( stdout );
      fputs( x_eol(), stderr );
      s_outputMidLine = 0;
   }

   if( s_caught == NULL )
      fprintf( stderr, "BREAK without a matching BEGIN SEQUENCE%s", x_eol() );
   else
   {
      const X_ERROR_DEF *def = s_caught->definition;

      if( s_caught->recoveryFailure )
         fprintf( stderr, "Error recovery failure: %s%s", IIF( def, def->operation, "unknown error code" ), x_eol() );
      else
      {
         const char *handler = IIF( s_caught->handlerFailed, " (error in handler)", "" );

         fprintf( stderr, "Error %s/%d  %s: %s%s%s", def->subSystem, def->subCode, def->description, def->operation,
                  handler, x_eol() );
      }

      if( s_caught->location != NULL )
         fprintf( stderr, "Called from %s (%s)%s", s_caught->function, s_caught->location, x_eol() );
   }

   s_errorReported = 1;
}

/* The answer of the runtime when there is no ErrorBlock(): zero for a division by zero, else propagate */
static X_ERROR_ACTION s_errorInitial( const X_ERROR *error, X_ITEM *value )
{
   int code = error->definition->subCode;
   int integers;

   if( code != 1340 && code != 1341 )
      return X_ERROR_PROPAGATE;

   integers = error->argumentCount == 2 && error->arguments[0].nType == X_IT_INTEGER;
   integers = integers && error->arguments[1].nType == X_IT_INTEGER;
   if( code == 1341 && integers )
   {
      value->nType = X_IT_INTEGER;
      value->u.integer = 0;
   }
   else
   {
      value->nType = X_IT_DOUBLE;
      value->u.dbl = 0.0;
   }

   return X_ERROR_SUBSTITUTE;
}

/* The ErrorBlock() of a program that sets none */
static void s_defaultErrorBlock( X_CALL_CTX *ctx, X_ITEM *const *env )
{
   UNUSED( env );
   s_defaultPropagate = s_activeError == NULL || s_errorInitial( s_activeError, ctx->result ) == X_ERROR_PROPAGATE;
}

/* ErrorBlock() without an argument */
void x_errorBlockGet( X_CALL_CTX *ctx, X_ITEM *result )
{
   if( s_errorBlock.nType == X_IT_BLOCK )
      x_itemCopy( result, &s_errorBlock );
   else
      x_itemBlockInto( ctx, result, s_defaultErrorBlock, NULL, 0 );
}

/* ErrorBlock( bNew ): returns the previous block */
void x_errorBlockSet( X_CALL_CTX *ctx, X_ITEM *result, const X_ITEM *block )
{
   X_ITEM replacement __attribute__( ( cleanup( x_itemClear ) ) ) = X_ITEM_NIL_INIT;

   if( block == NULL || block->nType != X_IT_BLOCK )
   {
      x_errorRaiseNamed( result, 3000, "ERRORBLOCK expects a code block" );
      return;
   }

   x_itemCopy( &replacement, block );
   x_errorBlockGet( ctx, result );
   x_itemCopy( &s_errorBlock, &replacement );
}

/* Forget the ErrorBlock() */
void x_errorBlockClear( void )
{
   x_itemClear( &s_errorBlock );
}

/* Program exit code set by ErrorLevel(), used when Main does not return a number and on QUIT */
int x_errorLevelGet( void )
{
   return s_errorLevel;
}

/* ErrorLevel( nNew ) */
void x_errorLevelSet( int level )
{
   s_errorLevel = level;
}

/* Evaluate the ErrorBlock() with the Error object */
static X_ERROR_ACTION s_callErrorBlock( const X_ERROR *error, X_ITEM *substitute )
{
   X_CALL_CTX ctx = { 0 };

   int previous __attribute__( ( cleanup( s_propagateRestore ) ) ) = s_defaultPropagate;
   X_ITEM object __attribute__( ( cleanup( x_itemClear ) ) ) = X_ITEM_NIL_INIT;
   X_ITEM block __attribute__( ( cleanup( x_itemClear ) ) ) = X_ITEM_NIL_INIT;
   X_ARG argument = { &object, 0 };
   X_ERROR_ACTION action;

   s_defaultPropagate = 0;
   x_itemCopy( &block, &s_errorBlock );
   if( x_errorObject( &ctx, &object, error ) )
      x_evalBlock( &ctx, substitute, &block, 1, &argument );
   else
      s_defaultPropagate = 1;

   if( !s_defaultPropagate && error->canDefault && substitute->nType == X_IT_LOGICAL && !substitute->u.logical )
      action = X_ERROR_DEFAULT;
   else
      action = IIF( s_defaultPropagate || !error->definition->canSubstitute, X_ERROR_PROPAGATE, X_ERROR_SUBSTITUTE );

   x_itemClear( &block );
   x_itemClear( &object );

   return action;
}

/* Object is the Error object of Throw(); the error block receives it unchanged */
static int s_errorRaiseEvent( X_ITEM *result, int subCode, const X_ITEM *a, const X_ITEM *b, const char *operation,
                              const X_ERROR_DEF *definition, int canDefault, const X_ITEM *object )
{
   X_ERROR *error __attribute__( ( cleanup( s_errorDrop ) ) ) = NULL;
   X_ITEM substitute __attribute__( ( cleanup( x_itemClear ) ) ) = X_ITEM_NIL_INIT;
   X_ERROR_ACTION action = X_ERROR_PROPAGATE;
   size_t i;

   if( s_activeError != NULL )
   {
      s_activeError->handlerFailed = 1;
      x_itemClear( result );
      x_errorThrow( x_errorClone( s_activeError ) );
   }

   size_t definitionSize = 0;

   if( definition != NULL )
      definitionSize = sizeof( X_ERROR_DEF ) + s_errorTextsSize( definition );
   else if( operation )
      definitionSize = sizeof( X_ERROR_DEF ) + strlen( operation ) + 1;

   error = ( X_ERROR * )x_xgrab( sizeof( X_ERROR ) + definitionSize );
   memset( error, 0, sizeof( *error ) );
   error->canDefault = canDefault;
   if( s_trace != NULL )
   {
      error->location = s_trace->location;
      error->function = s_trace->function;
   }

   if( definition != NULL )
   {
      X_ERROR_DEF *owned = ( X_ERROR_DEF * )( error + 1 );
      char *text = ( char * )( owned + 1 );

      *owned = *definition;
      size_t length = strlen( definition->subSystem ) + 1;

      memcpy( text, definition->subSystem, length );
      owned->subSystem = text;
      text += length;
      length = strlen( definition->description ) + 1;
      memcpy( text, definition->description, length );
      owned->description = text;
      text += length;
      length = strlen( definition->operation ) + 1;
      memcpy( text, definition->operation, length );
      owned->operation = text;
      error->definition = owned;
   }
   else
   {
      for( i = 0; i < sizeof( s_errorDefinitions ) / sizeof( s_errorDefinitions[0] ); ++i )
      {
         if( s_errorDefinitions[i].subCode == subCode )
         {
            error->definition = &s_errorDefinitions[i];
            break;
         }
      }

      if( operation && error->definition )
      {
         X_ERROR_DEF *owned = ( X_ERROR_DEF * )( error + 1 );
         char *name = ( char * )( owned + 1 );

         *owned = *error->definition;
         strcpy( name, operation );
         owned->operation = name;
         error->definition = owned;
      }
   }

   if( a != NULL )
      x_itemCopy( &error->arguments[error->argumentCount++], a );

   if( b != NULL )
      x_itemCopy( &error->arguments[error->argumentCount++], b );

   if( object != NULL )
      x_itemCopy( &error->object, object );

   if( error->definition == NULL )
      error->recoveryFailure = 1;
   else
   {
      X_ERROR *previous __attribute__( ( cleanup( s_activeRestore ) ) ) = s_activeError;
      X_ERROR_HANDLER handler;
      int allowed;

      s_activeError = error;

      /* ErrorBlock() first, then a handler of C code, then the default */
      if( s_errorBlock.nType == X_IT_BLOCK )
         handler = s_callErrorBlock;
      else if( s_errorHandler )
         handler = s_errorHandler;
      else
         handler = s_errorInitial;

      action = handler( error, &substitute );

      /* The block answered what the error does not allow */
      allowed = action == X_ERROR_PROPAGATE;
      allowed = allowed || ( action == X_ERROR_SUBSTITUTE && error->definition->canSubstitute );
      allowed = allowed || ( action == X_ERROR_DEFAULT && error->canDefault );
      if( !allowed )
         error->recoveryFailure = 1;
   }

   if( action == X_ERROR_DEFAULT && !error->recoveryFailure )
   {
      x_itemClear( result );
      return 1;
   }

   if( action == X_ERROR_SUBSTITUTE && !error->recoveryFailure )
   {
      x_itemCopy( result, &substitute );
      x_itemClear( &substitute );
      return 1;
   }

   x_itemClear( &substitute );
   x_itemClear( result );
   X_ERROR *raised = error;

   error = NULL;
   x_errorThrow( raised );
}

/* Raise a runtime error of the table by its subcode, with up to two arguments */
int x_errorRaise( X_ITEM *result, int subCode, const X_ITEM *a, const X_ITEM *b )
{
   return s_errorRaiseEvent( result, subCode, a, b, NULL, NULL, 0, NULL );
}

/* Raise a runtime error of the table by its subcode, with the name of the operation */
int x_errorRaiseNamed( X_ITEM *result, int subCode, const char *operation )
{
   return s_errorRaiseEvent( result, subCode, NULL, NULL, operation, NULL, 0, NULL );
}

/* Raise a runtime error of a definition */
int x_errorRaiseDefinition( X_ITEM *result, const X_ERROR_DEF *definition, const X_ITEM *a, const X_ITEM *b )
{
   int subCode = IIF( definition != NULL, definition->subCode, 0 );

   return s_errorRaiseEvent( result, subCode, a, b, NULL, definition, 0, NULL );
}

/* Raise a runtime error of a definition that can default */
int x_errorRaiseDefaultDefinition( X_ITEM *result, const X_ERROR_DEF *definition, const X_ITEM *a, const X_ITEM *b )
{
   int subCode = IIF( definition != NULL, definition->subCode, 0 );

   return s_errorRaiseEvent( result, subCode, a, b, NULL, definition, 1, NULL );
}

/* Throw( oError ): the texts of the definition point into the object, which lives through the call */
int x_errorRaiseObject( X_ITEM *result, const X_ITEM *object )
{
   X_ERROR_DEF definition;
   int canDefault = x_arrayGetL( object, X_ERR_CANDEFAULT );

   definition.severity = x_arrayGetNI( object, X_ERR_SEVERITY );
   definition.genCode = x_arrayGetNI( object, X_ERR_GENCODE );
   definition.subCode = x_arrayGetNI( object, X_ERR_SUBCODE );
   definition.canSubstitute = x_arrayGetL( object, X_ERR_CANSUBSTITUTE );
   definition.subSystem = x_arrayGetCPtr( object, X_ERR_SUBSYSTEM );
   definition.description = x_arrayGetCPtr( object, X_ERR_DESCRIPTION );
   definition.operation = x_arrayGetCPtr( object, X_ERR_OPERATION );

   return s_errorRaiseEvent( result, definition.subCode, NULL, NULL, NULL, &definition, canDefault, object );
}

/* The logical result of a condition; error 1066 when it is not logical */
int x_errorCondition( const X_ITEM *value )
{
   X_ITEM unused __attribute__( ( cleanup( x_itemClear ) ) ) = X_ITEM_NIL_INIT;
   int logical;

   if( x_itemLogical( value, &logical ) )
      return logical;

   x_errorRaise( &unused, 1066, value, NULL );
   x_itemClear( &unused );

   return 0;
}

/* ---------------------------------------------------------------------------------------------
   Parameter API
   --------------------------------------------------------------------------------------------- */

/* PCount(): the number of arguments */
int x_pcount( X_CALL_CTX *ctx )
{
   return ctx->argc;
}

/* Argument n when it has one of the types; NULL otherwise */
const X_ITEM *x_param( X_CALL_CTX *ctx, int n, X_TYPE type )
{
   const X_ITEM *item = x_paritem( ctx, n );

   if( item == NULL )
      return NULL;

   if( type == X_IT_ANY || IIF( type == X_IT_NIL, X_IS_NIL( item ), ( X_ITEM_TYPE( item ) & type ) != 0 ) )
      return item;

   return NULL;
}

/* Argument n for an error message, NIL without it */
const X_ITEM *x_paramError( X_CALL_CTX *ctx, int n )
{
   static const X_ITEM x_nilItem = X_ITEM_NIL_INIT;
   const X_ITEM *item = x_paritem( ctx, n );

   return IIF( item != NULL, item, &x_nilItem );
}

/* The type of argument n */
X_TYPE x_parinfo( X_CALL_CTX *ctx, int n )
{
   const X_ITEM *item = x_paritem( ctx, n );

   if( n == 0 )
      return ( X_TYPE )ctx->argc;

   if( item == NULL )
      return X_IT_NIL;

   return X_ITEM_TYPE( item ) | IIF( ctx->args[n - 1].byReference, X_IT_BYREF, X_IT_NIL );
}

/* The type of element index of array argument n, its length for index 0 */
size_t x_parinfa( X_CALL_CTX *ctx, int n, size_t index )
{
   const X_ITEM *array = x_param( ctx, n, X_IT_ARRAY );
   const X_ITEM *item;

   if( array == NULL )
      return 0;

   if( index == 0 )
      return ( size_t )x_arrayLen( array );

   item = x_arrayGetItemPtr( array, index );

   return IIF( item != NULL, ( size_t )X_ITEM_TYPE( item ), 0 );
}

/* Is argument n NIL or missing? */
int x_extIsNil( X_CALL_CTX *ctx, int n )
{
   const X_ITEM *item = x_paritem( ctx, n );

   return item == NULL || X_IS_NIL( item );
}

/* Is argument n an object? */
int x_extIsObject( X_CALL_CTX *ctx, int n )
{
   const X_ITEM *item = x_param( ctx, n, X_IT_ARRAY );

   return item != NULL && x_arrayIsObject( item );
}

/* Argument n, through a reference; NULL without it */
const X_ITEM *x_paritem( X_CALL_CTX *ctx, int n )
{
   if( n < 1 || n > ctx->argc )
      return NULL;

   return ctx->args[n - 1].value;
}

/* The text of argument n; NULL when it is not a string */
const char *x_parc( X_CALL_CTX *ctx, int n )
{
   const X_ITEM *item = x_paritem( ctx, n );

   if( item == NULL || item->nType != X_IT_STRING )
      return NULL;

   return item->u.string.data;
}

/* The text of argument n; "" when it is not a string */
const char *x_parcx( X_CALL_CTX *ctx, int n )
{
   const char *value = x_parc( ctx, n );

   return IIF( value != NULL, value, "" );
}

/* The length of string argument n */
size_t x_parclen( X_CALL_CTX *ctx, int n )
{
   const X_ITEM *item = x_paritem( ctx, n );

   return IIF( item != NULL && item->nType == X_IT_STRING, item->u.string.len, 0 );
}

/* The length of string argument n with its zero */
size_t x_parcsiz( X_CALL_CTX *ctx, int n )
{
   const X_ITEM *item = x_paritem( ctx, n );

   return IIF( item != NULL && item->nType == X_IT_STRING && ctx->args[n - 1].byReference, item->u.string.len + 1, 0 );
}

/* A Julian day as YYYYMMDD; 8 spaces for 0 */
static char *s_dateToString( long julian, char *date )
{
   int year, month, day;

   if( julian == 0 )
      memset( date, ' ', 8 );
   else
   {
      x_dateDecode( julian, &year, &month, &day );
      snprintf( date, 9, "%04d%02d%02d", year, month, day );
   }

   date[8] = '\0';

   return date;
}

/* Date argument n as YYYYMMDD in a static buffer */
const char *x_pards( X_CALL_CTX *ctx, int n )
{
   static char date[9];

   return s_dateToString( x_pardl( ctx, n ), date );
}

/* Date argument n as YYYYMMDD in date */
char *x_pardsbuff( X_CALL_CTX *ctx, char *date, int n )
{
   return s_dateToString( x_pardl( ctx, n ), date );
}

/* Date argument n as a Julian day */
long x_pardl( X_CALL_CTX *ctx, int n )
{
   const X_ITEM *item = x_paritem( ctx, n );

   return IIF( item != NULL && item->nType == X_IT_DATE, ( long )item->u.integer, 0 );
}

/* Logical argument n; 0 otherwise */
int x_parl( X_CALL_CTX *ctx, int n )
{
   const X_ITEM *item = x_paritem( ctx, n );

   if( item == NULL || item->nType != X_IT_LOGICAL )
      return 0;

   return item->u.logical;
}

/* Logical argument n; def without it */
int x_parldef( X_CALL_CTX *ctx, int n, int defValue )
{
   const X_ITEM *item = x_param( ctx, n, X_IT_LOGICAL );

   return IIF( item != NULL, item->u.logical, defValue );
}

/* Numeric argument n as double */
double x_parnd( X_CALL_CTX *ctx, int n )
{
   return x_itemAsDouble( x_paritem( ctx, n ) );
}

/* Numeric argument n as int64_t */
int64_t x_parnint( X_CALL_CTX *ctx, int n )
{
   return x_itemAsInt64( x_paritem( ctx, n ) );
}

/* Numeric argument n as uint64_t */
uint64_t x_paruint64( X_CALL_CTX *ctx, int n )
{
   const X_ITEM *value = x_paritem( ctx, n );

   return IIF( value != NULL && X_IS_NUMERIC( value ), s_nativeBits( 0x100 | 64, value ), 0 );
}

int x_parni( X_CALL_CTX *ctx, int n )
{
   return ( int )x_parnint( ctx, n );
}

int x_parnidef( X_CALL_CTX *ctx, int n, int defValue )
{
   return IIF( x_param( ctx, n, X_IT_NUMERIC ) != NULL, x_parni( ctx, n ), defValue );
}

long x_parnl( X_CALL_CTX *ctx, int n )
{
   return ( long )x_parnint( ctx, n );
}

long x_parnldef( X_CALL_CTX *ctx, int n, long defValue )
{
   return IIF( x_param( ctx, n, X_IT_NUMERIC ) != NULL, x_parnl( ctx, n ), defValue );
}

long long x_parnll( X_CALL_CTX *ctx, int n )
{
   return ( long long )x_parnint( ctx, n );
}

ptrdiff_t x_parns( X_CALL_CTX *ctx, int n )
{
   return ( ptrdiff_t )x_parnint( ctx, n );
}

ptrdiff_t x_parnsdef( X_CALL_CTX *ctx, int n, ptrdiff_t defValue )
{
   return IIF( x_param( ctx, n, X_IT_NUMERIC ) != NULL, x_parns( ctx, n ), defValue );
}

/* Numeric argument n as int64_t; def without it */
int64_t x_parnintdef( X_CALL_CTX *ctx, int n, int64_t defValue )
{
   return IIF( x_param( ctx, n, X_IT_NUMERIC ) != NULL, x_parnint( ctx, n ), defValue );
}

/* Pointer argument n */
void *x_parptr( X_CALL_CTX *ctx, int n )
{
   const X_ITEM *item = x_param( ctx, n, X_IT_POINTER );

   return IIF( item != NULL, item->u.pPointer, NULL );
}

/* ---------------------------------------------------------------------------------------------
   Return API
   --------------------------------------------------------------------------------------------- */

/* Return NIL */
void x_ret( X_CALL_CTX *ctx )
{
   x_itemClear( ctx->result );
}

void x_retl( X_CALL_CTX *ctx, int value )
{
   x_itemReplace( ctx->result, ( X_ITEM ){ X_IT_LOGICAL, { .logical = IIF( value, 1, 0 ) } } );
}

void x_retni( X_CALL_CTX *ctx, int value )
{
   x_itemReplace( ctx->result, ( X_ITEM ){ X_IT_INTEGER, { .integer = value } } );
}

void x_retnl( X_CALL_CTX *ctx, long value )
{
   x_itemReplace( ctx->result, ( X_ITEM ){ X_IT_INTEGER, { .integer = value } } );
}

void x_retnll( X_CALL_CTX *ctx, long long value )
{
   x_itemReplace( ctx->result, ( X_ITEM ){ X_IT_INTEGER, { .integer = value } } );
}

void x_retns( X_CALL_CTX *ctx, ptrdiff_t value )
{
   x_itemReplace( ctx->result, ( X_ITEM ){ X_IT_INTEGER, { .integer = value } } );
}

void x_retnint( X_CALL_CTX *ctx, int64_t value )
{
   x_itemReplace( ctx->result, ( X_ITEM ){ X_IT_INTEGER, { .integer = value } } );
}

void x_retuint64( X_CALL_CTX *ctx, uint64_t value )
{
   x_itemReplace( ctx->result, ( X_ITEM ){ X_IT_UINT64, { .unsignedInteger = value } } );
}

void x_retnd( X_CALL_CTX *ctx, double value )
{
   x_itemReplace( ctx->result, ( X_ITEM ){ X_IT_DOUBLE, { .dbl = value } } );
}

/* Return a copy of a C string */
void x_retc( X_CALL_CTX *ctx, const char *value )
{
   x_retclen( ctx, value, IIF( value != NULL, strlen( value ), 0 ) );
}

/* Return "" */
void x_retc_null( X_CALL_CTX *ctx )
{
   x_retclen( ctx, "", 0 );
}

/* Return a C string of x_xgrab(); the result takes it */
void x_retc_buffer( X_CALL_CTX *ctx, char *value )
{
   x_retclen_buffer( ctx, value, IIF( value != NULL, strlen( value ), 0 ) );
}

/* Return a copy of length bytes */
void x_retclen( X_CALL_CTX *ctx, const char *value, size_t length )
{
   char *copy;

   if( value == NULL )
   {
      value = "";
      length = 0;
   }

   copy = ( char * )x_xgrab( length + 1 );
   memcpy( copy, value, length );
   copy[length] = '\0';
   x_itemReplace( ctx->result, ( X_ITEM ){ X_IT_STRING, { .string = { copy, length } } } );
}

/* Return length bytes of x_xgrab(); the result takes them */
void x_retclen_buffer( X_CALL_CTX *ctx, char *value, size_t length )
{
   x_itemPutStringBuffer( ctx->result, value, length );
}

/* Return a date of a YYYYMMDD text */
void x_retds( X_CALL_CTX *ctx, const char *date )
{
   x_retdl( ctx, x_dateUnformat( date, "YYYYMMDD" ) );
}

/* Return a date */
void x_retd( X_CALL_CTX *ctx, int year, int month, int day )
{
   x_itemReplace( ctx->result, ( X_ITEM ){ X_IT_DATE, { .integer = x_dateEncode( year, month, day ) } } );
}

/* Return a Julian day */
void x_retdl( X_CALL_CTX *ctx, long julian )
{
   x_itemReplace( ctx->result, ( X_ITEM ){ X_IT_DATE, { .integer = julian } } );
}

/* Return a new array of length NIL elements */
void x_reta( X_CALL_CTX *ctx, size_t length )
{
   if( length > ( size_t )INT64_MAX || !x_arrayNew( ctx->result, ( int64_t )length ) )
      x_ret( ctx );
}

/* Return a pointer */
void x_retptr( X_CALL_CTX *ctx, void *ptr )
{
   x_itemReplace( ctx->result, ( X_ITEM ){ X_IT_POINTER, { .pPointer = ptr } } );
}

/* Return a block of the garbage collector */
void x_retptrGC( X_CALL_CTX *ctx, void *ptr )
{
   x_itemPutPtrGC( ctx->result, ptr );
}

/* ---------------------------------------------------------------------------------------------
   Store API
   --------------------------------------------------------------------------------------------- */

/* The variable that argument n refers to; NULL when it is not passed by reference */
static X_ITEM *s_storeSlot( X_CALL_CTX *ctx, int n )
{
   if( n < 1 || n > ctx->argc || !ctx->args[n - 1].byReference )
      return NULL;

   return ( X_ITEM * )ctx->args[n - 1].value;
}

/* Store NIL into argument n passed by reference */
int x_stor( X_CALL_CTX *ctx, int n )
{
   X_ITEM *item = s_storeSlot( ctx, n );

   if( item == NULL )
      return 0;

   x_itemReplace( item, ( X_ITEM )X_ITEM_NIL_INIT );

   return 1;
}

int x_storni( X_CALL_CTX *ctx, int value, int n )
{
   return s_itemSetInteger( s_storeSlot( ctx, n ), value );
}

int x_stornl( X_CALL_CTX *ctx, long value, int n )
{
   return s_itemSetInteger( s_storeSlot( ctx, n ), value );
}

int x_stornll( X_CALL_CTX *ctx, long long value, int n )
{
   return s_itemSetInteger( s_storeSlot( ctx, n ), value );
}

int x_storns( X_CALL_CTX *ctx, ptrdiff_t value, int n )
{
   return s_itemSetInteger( s_storeSlot( ctx, n ), value );
}

int x_stornint( X_CALL_CTX *ctx, int64_t value, int n )
{
   return s_itemSetInteger( s_storeSlot( ctx, n ), value );
}

int x_storuint64( X_CALL_CTX *ctx, uint64_t value, int n )
{
   X_ITEM *item = s_storeSlot( ctx, n );

   if( item == NULL )
      return 0;

   x_itemReplace( item, ( X_ITEM ){ X_IT_UINT64, { .unsignedInteger = value } } );

   return 1;
}

int x_stornd( X_CALL_CTX *ctx, double value, int n )
{
   return s_itemSetDouble( s_storeSlot( ctx, n ), value );
}

/* Store a logical value */
int x_storl( X_CALL_CTX *ctx, int value, int n )
{
   return s_itemSetLogical( s_storeSlot( ctx, n ), value );
}

/* Store a copy of a C string */
int x_storc( X_CALL_CTX *ctx, const char *value, int n )
{
   return x_storclen( ctx, value, IIF( value != NULL, strlen( value ), 0 ), n );
}

/* Store a copy of length bytes */
int x_storclen( X_CALL_CTX *ctx, const char *value, size_t length, int n )
{
   return s_itemSetString( s_storeSlot( ctx, n ), value, length );
}

/* Store length bytes of x_xgrab(); the variable takes them */
int x_storclen_buffer( X_CALL_CTX *ctx, char *value, size_t length, int n )
{
   return s_itemSetStringBuffer( s_storeSlot( ctx, n ), value, length );
}

/* Store a date of a YYYYMMDD text */
int x_stords( X_CALL_CTX *ctx, const char *date, int n )
{
   return s_itemSetDate( s_storeSlot( ctx, n ), x_dateUnformat( date, "YYYYMMDD" ) );
}

/* Store a Julian day */
int x_stordl( X_CALL_CTX *ctx, long julian, int n )
{
   return s_itemSetDate( s_storeSlot( ctx, n ), julian );
}

/* Store a pointer */
int x_storptr( X_CALL_CTX *ctx, void *ptr, int n )
{
   X_ITEM *item = s_storeSlot( ctx, n );

   if( item == NULL )
      return 0;

   x_itemReplace( item, ( X_ITEM ){ X_IT_POINTER, { .pPointer = ptr } } );

   return 1;
}

/* Store a block of the garbage collector */
int x_storptrGC( X_CALL_CTX *ctx, void *ptr, int n )
{
   X_ITEM *item = s_storeSlot( ctx, n );

   return item != NULL && x_itemPutPtrGC( item, ptr ) != NULL;
}

/* ---------------------------------------------------------------------------------------------
   Runtime lifecycle
   --------------------------------------------------------------------------------------------- */

/* The generated main() starts and stops only the runtime; the runtime decides which services,
   such as the GT, to bring up. Returns 0 on success. */
#if X_OS_WINDOWS
static UINT s_consoleInputCP = 0;
static UINT s_consoleOutputCP = 0;
#endif

/* The start of the runtime: a UTF-8 console on Windows and the screen */
int x_runtimeInit( void )
{
#if X_OS_WINDOWS
   /* xDev uses UTF-8 at both Windows console boundaries. Redirected streams remain binary. */
   _setmode( _fileno( stdout ), _O_BINARY );
   _setmode( _fileno( stderr ), _O_BINARY );
   s_consoleInputCP = GetConsoleCP();
   s_consoleOutputCP = GetConsoleOutputCP();
   if( s_consoleInputCP != 0 )
      SetConsoleCP( CP_UTF8 );

   if( s_consoleOutputCP != 0 )
      SetConsoleOutputCP( CP_UTF8 );
#endif

   return x_gtInit( 0, 1, 2 );
}

/* The end of the runtime */
void x_runtimeExit( void )
{
   x_fileAreasExit();
   x_gtExit();
#if X_OS_WINDOWS
   if( s_consoleInputCP != 0 )
      SetConsoleCP( s_consoleInputCP );

   if( s_consoleOutputCP != 0 )
      SetConsoleOutputCP( s_consoleOutputCP );
#endif
}

/* ---------------------------------------------------------------------------------------------
   System
   --------------------------------------------------------------------------------------------- */

/* Durations are ticks of 1/18.2 second. Without a sound for the frequency (Windows
   sounds 37 to 32767 Hz, other systems have no speaker access) the time still passes. */
void x_sysTone( double frequency, double ticks )
{
   unsigned int ms = IIF( ticks > 0, ( unsigned int )( ticks * 1000.0 / 18.2 ), 0 );

#if X_OS_WINDOWS
   if( frequency >= 37 && frequency <= 32767 )
      Beep( ( DWORD )frequency, ms );
   else
      Sleep( ms );
#else
   UNUSED( frequency );
   usleep( ( useconds_t )ms * 1000 );
#endif
}

/* Seconds may have a fraction; zero or less returns at once */
void x_sysSleep( double seconds )
{
#if X_OS_WINDOWS
   if( seconds > 0 )
      Sleep( ( DWORD )( seconds * 1000.0 ) );
#else
   struct timespec pause;

   if( seconds > 0 )
   {
      pause.tv_sec = ( time_t )seconds;
      pause.tv_nsec = ( long )( ( seconds - ( double )pause.tv_sec ) * 1000000000.0 );
      nanosleep( &pause, NULL );
   }
#endif
}

/* Processor seconds used by the program: 1 user, 2 system, any other value both; 11, 12 and 13 add the
   finished child processes, which only the Unix systems report */
double x_sysCpuTime( int which )
{
   int children = which > 10;
   double user, system;

#if X_OS_WINDOWS
   FILETIME creation, exit, kernel, userTime;
   ULARGE_INTEGER value;

   if( !GetProcessTimes( GetCurrentProcess(), &creation, &exit, &kernel, &userTime ) )
      return 0;

   value.LowPart = userTime.dwLowDateTime;
   value.HighPart = userTime.dwHighDateTime;
   user = ( double )value.QuadPart / 10000000.0;
   value.LowPart = kernel.dwLowDateTime;
   value.HighPart = kernel.dwHighDateTime;
   system = ( double )value.QuadPart / 10000000.0;
   UNUSED( children );
#else
   struct rusage usage;

   if( getrusage( RUSAGE_SELF, &usage ) != 0 )
      return 0;

   user = ( double )usage.ru_utime.tv_sec + ( double )usage.ru_utime.tv_usec / 1000000.0;
   system = ( double )usage.ru_stime.tv_sec + ( double )usage.ru_stime.tv_usec / 1000000.0;
   if( children && getrusage( RUSAGE_CHILDREN, &usage ) == 0 )
   {
      user += ( double )usage.ru_utime.tv_sec + ( double )usage.ru_utime.tv_usec / 1000000.0;
      system += ( double )usage.ru_stime.tv_sec + ( double )usage.ru_stime.tv_usec / 1000000.0;
   }
#endif

   which %= 10;

   if( which == 1 )
      return user;

   if( which == 2 )
      return system;

   return user + system;
}

/* Bytes of free physical memory */
uint64_t x_sysMemoryAvail( void )
{
#if X_OS_WINDOWS
   MEMORYSTATUSEX status;

   status.dwLength = sizeof( status );

   return IIF( GlobalMemoryStatusEx( &status ), status.ullAvailPhys, 0 );
#else
   long pages = sysconf( _SC_AVPHYS_PAGES ), size = sysconf( _SC_PAGESIZE );

   return IIF( pages > 0 && size > 0, ( uint64_t )pages * ( uint64_t )size, 0 );
#endif
}

/* The name of the computer in UTF-8, in an x_xgrab() buffer */
char *x_sysNetName( void )
{
#if X_OS_WINDOWS
   wchar_t name[MAX_COMPUTERNAME_LENGTH + 1];
   DWORD length = MAX_COMPUTERNAME_LENGTH + 1;
   int size;
   char *result;

   if( !GetComputerNameW( name, &length ) )
      length = 0;

   size = IIF( length, WideCharToMultiByte( CP_UTF8, 0, name, ( int )length, NULL, 0, NULL, NULL ), 0 );
   result = ( char * )x_xgrab( ( size_t )size + 1 );
   if( size )
      WideCharToMultiByte( CP_UTF8, 0, name, ( int )length, result, size, NULL, NULL );

   result[size] = '\0';

   return result;
#else
   char *result = ( char * )x_xgrab( 256 );

   if( gethostname( result, 255 ) != 0 )
      result[0] = '\0';

   result[255] = '\0';

   return result;
#endif
}

/* Name of the user who runs the program, UTF-8; empty when the system does not give it */
char *x_sysUserName( void )
{
#if X_OS_WINDOWS
   wchar_t name[257]; /* UNLEN + 1 */
   DWORD length = 257;
   int size;
   char *result;

   if( !GetUserNameW( name, &length ) )
      length = 1;

   name[length - 1] = L'\0';
   size = WideCharToMultiByte( CP_UTF8, 0, name, -1, NULL, 0, NULL, NULL );
   result = ( char * )x_xgrab( ( size_t )size );
   WideCharToMultiByte( CP_UTF8, 0, name, -1, result, size, NULL, NULL );

   return result;
#else
   struct passwd *entry = getpwuid( geteuid() );

   return x_strdup( IIF( entry, entry->pw_name, "" ) );
#endif
}

/* The printer port can be opened for writing,; NULL is the first
   parallel port of the system. */
int x_sysPrinterReady( const char *port )
{
#if X_OS_WINDOWS
   HANDLE handle = CreateFileA( IIF( port, port, "LPT1" ), GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL );

   if( handle == INVALID_HANDLE_VALUE )
      return 0;

   CloseHandle( handle );

   return 1;
#else
   int handle = open( IIF( port, port, "/dev/lp0" ), O_WRONLY | O_NONBLOCK );

   if( handle < 0 )
      return 0;

   close( handle );

   return 1;
#endif
}

/* ---------------------------------------------------------------------------------------------
   Random numbers
   --------------------------------------------------------------------------------------------- */

/* Start the random numbers of Random() with a seed */
void x_randomSeed( uint64_t seed )
{
   s_randomState = seed;
   s_randomSeeded = 1;
}

/* A different seed for each run */
void x_randomSeedTime( void )
{
   struct timespec now;

   timespec_get( &now, TIME_UTC );
   x_randomSeed( ( uint64_t )now.tv_sec * 1000000000u + ( uint64_t )now.tv_nsec );
}

/* SplitMix64 (Sebastiano Vigna): the state only adds a constant, the result mixes its bits */
static uint64_t s_randomNext( void )
{
   uint64_t value;

   if( !s_randomSeeded )
      x_randomSeedTime();

   value = s_randomState += 0x9E3779B97F4A7C15u;
   value = ( value ^ ( value >> 30 ) ) * 0xBF58476D1CE4E5B9u;
   value = ( value ^ ( value >> 27 ) ) * 0x94D049BB133111EBu;

   return value ^ ( value >> 31 );
}

/* [0, 1) with the 53 bits a double holds */
double x_randomDouble( void )
{
   return ( double )( s_randomNext() >> 11 ) * ( 1.0 / 9007199254740992.0 );
}

/* Min to max inclusive, the boundaries in either order */
int64_t x_randomInt( int64_t min, int64_t max )
{
   uint64_t span;

   if( max < min )
   {
      int64_t swap = min;

      min = max;
      max = swap;
   }

   span = ( uint64_t )max - ( uint64_t )min;
   if( span == UINT64_MAX )
      return ( int64_t )s_randomNext();

   return ( int64_t )( ( uint64_t )min + s_randomNext() % ( span + 1 ) );
}

/* ---------------------------------------------------------------------------------------------
   Console output
   --------------------------------------------------------------------------------------------- */

/* Write to the screen when the stream goes there, else to stdout or stderr */
static void s_output( const char *text, size_t length, int stream )
{
   if( x_gtIsConsole( stream ) )
      x_gtWriteCon( text, length );
   else
      fwrite( text, 1, length, IIF( stream == 2, stderr, stdout ) );

   if( length > 0 )
      s_outputMidLine = text[length - 1] != '\n';
}

/* QOut() and QQOut(): the arguments separated by spaces, to stdout or stderr */
void x_consoleOut( X_CALL_CTX *ctx, int newLine, int stream )
{
   int count = x_pcount( ctx ), i;
   const X_ITEM *item;
   const char *text;
   char buffer[64];
   size_t length;

   if( !newLine && count == 0 )
      return;

   if( newLine )
   {
      if( x_gtIsConsole( stream ) )
         x_gtWriteCon( "\r\n", 2 );
      else
         s_output( x_eol(), strlen( x_eol() ), stream );
   }

   for( i = 1; i <= count; ++i )
   {
      if( i > 1 )
         s_output( " ", 1, stream );

      item = x_paritem( ctx, i );

      if( item == NULL || item->nType == X_IT_NIL )
         text = "NIL";

      else if( x_setFixed && X_IS_NUMERIC( item ) )
      {
         X_ITEM number __attribute__( ( cleanup( x_itemClear ) ) ) = X_ITEM_NIL_INIT;

         x_transform( &number, item, "", 0 );
         s_output( number.u.string.data, number.u.string.len, stream );
         continue;
      }
      else if( item->nType == X_IT_INTEGER )
      {
         snprintf( buffer, sizeof( buffer ), "%lld", ( long long )item->u.integer );
         text = buffer;
      }
      else if( item->nType == X_IT_UINT64 )
      {
         snprintf( buffer, sizeof( buffer ), "%llu", ( unsigned long long )item->u.unsignedInteger );
         text = buffer;
      }
      else if( item->nType == X_IT_DOUBLE )
      {
         snprintf( buffer, sizeof( buffer ), "%.15g", item->u.dbl );
         text = buffer;
      }
      else if( item->nType == X_IT_STRING )
      {
         length = item->u.string.len;
         if( length )
            s_output( item->u.string.data, length, stream );
         continue;
      }
      else if( item->nType == X_IT_DATE )
      {
         char date[9];

         x_dateFormat( s_dateToString( ( long )item->u.integer, date ), buffer, x_setDateFormat( NULL ) );
         text = buffer;
      }
      else if( item->nType == X_IT_LOGICAL )
         text = IIF( item->u.logical, ".T.", ".F." );
      else if( X_IS_OBJECT( item ) )
      {
         text = x_objGetClsName( item );
         s_output( "<", 1, stream );
         s_output( text, strlen( text ), stream );
         s_output( ">", 1, stream );
         continue;
      }
      else if( item->nType == X_IT_ARRAY )
      {
         snprintf( buffer, sizeof( buffer ), "<ARRAY %lld>", ( long long )x_arrayLen( item ) );
         text = buffer;
      }
      else if( item->nType == X_IT_BLOCK )
         text = "<BLOCK>";
      else if( X_IS_POINTER( item ) )
         text = "<POINTER>";
      else
         text = "<nieobslugiwany X_ITEM>";

      s_output( text, strlen( text ), stream );
   }

   if( !x_gtIsConsole( stream ) )
      fflush( IIF( stream == 2, stderr, stdout ) );
}

/* The next line of a memo from *pos: columns and widths count UTF-8 characters, a broken byte counts as one
   character and a line never ends inside a character. Returns the columns of the line; the line goes to
   buffer, which needs room for 4 * width bytes, and its length in bytes to *used */
size_t x_memoNext( const char *text, size_t len, size_t *pos, size_t width, size_t tab, int wrap, char *buffer,
                   size_t *used )
{
   size_t col = 0, blankCol = 0, blankPos = 0;
   size_t bytes = 0, blankBytes = 0;

   if( tab >= width )
      tab = width - 1;

   if( tab == 0 )
      tab = 1;

   while( *pos < len )
   {
      unsigned char ch = ( unsigned char )text[*pos];
      size_t last = *pos;
      size_t next = *pos;
      uint32_t codepoint;

      /* A soft carriage return: byte 141 cannot start a UTF-8 character */
      if( ch == 141 && *pos + 1 < len && text[*pos + 1] == '\n' )
      {
         *pos += 2;
         if( !wrap )
            break;

         if( blankPos + 2 == *pos )
            blankPos += 2;
         continue;
      }

      if( ch == '\n' || ( ch == '\r' && *pos + 1 < len && text[*pos + 1] == '\n' ) )
      {
         *pos += IIF( ch == '\r', 2, 1 );
         break;
      }

      if( !wrap && col >= width )
         break;

      x_utf8Decode( text, len, &next, &codepoint );
      *pos = next;

      size_t step = IIF( ch == '\t', tab - col % tab, 1 );

      if( buffer != NULL && col < width )
      {
         size_t spaces = IIF( step > width - col, width - col, step );

         if( ch == '\t' )
            memset( buffer + bytes, ' ', spaces );
         else
            memcpy( buffer + bytes, text + last, next - last );

         bytes += IIF( ch == '\t', spaces, next - last );
      }

      col += step;
      if( col > width )
      {
         if( wrap && ( ch == ' ' || ch == '\t' ) )
            break;

         if( wrap && blankCol != 0 )
         {
            col = blankCol;
            *pos = blankPos;
            bytes = blankBytes;
         }
         else
            *pos = last;
         break;
      }

      if( col > 1 && ( ch == ' ' || ch == '\t' ) )
      {
         blankCol = col;
         blankPos = *pos;
         blankBytes = bytes;
      }
   }

   if( used != NULL )
      *used = bytes;

   return IIF( col > width, width, col );
}

/* Bytes of the UTF-8 character at pos; a broken byte counts as one character */
static size_t s_transformStep( const char *text, size_t size, size_t pos )
{
   size_t next = pos;
   uint32_t codepoint;

   return IIF( x_utf8Decode( text, size, &next, &codepoint ), next - pos, 1 );
}

/* The functions of a picture: @B left, @C CR, @X DB, @( parentheses, @R the rest of the template, @! upper case,
   @D date, @E British, @Z blank for zero */
enum
{
   TF_LEFT = 1,
   TF_CREDIT = 2,
   TF_DEBIT = 4,
   TF_PAREN = 8,
   TF_REMAIN = 16,
   TF_UPPER = 32,
   TF_DATE = 64,
   TF_EURO = 128,
   TF_BLANK = 256
};

/* The function letters after @ up to a space; *pic and *picLen move to the template */
static unsigned s_transformFlags( const char **pic, size_t *picLen )
{
   unsigned flags = 0;

   if( *picLen == 0 || ( *pic )[0] != '@' )
      return 0;

   ++*pic;
   --*picLen;
   while( *picLen )
   {
      unsigned char ch = ( unsigned char )*( *pic )++;

      --*picLen;
      if( ch == ' ' || ch == '\t' )
         break;

      switch( toupper( ch ) )
      {
      case 'B':
         flags |= TF_LEFT;
         break;

      case 'C':
         flags |= TF_CREDIT;
         break;

      case 'X':
         flags |= TF_DEBIT;
         break;

      case '(':
         flags |= TF_PAREN;
         break;

      case 'R':
         flags |= TF_REMAIN;
         break;

      case '!':
         flags |= TF_UPPER;
         break;

      case 'D':
         flags |= TF_DATE;
         break;

      case 'E':
         flags |= TF_EURO;
         break;

      case 'Z':
         flags |= TF_BLANK;
         break;
      }
   }

   return flags;
}

/* Is a template character a place for a character of the value? */
static int s_transformSlot( int ch )
{
   switch( ch )
   {
   case '!':
   case '#':
   case '9':
   case 'A':
   case 'L':
   case 'N':
   case 'X':
   case 'Y':
      return 1;

   default:
      return 0;
   }
}

/* Copy text, in upper case with upper; returns the bytes written */
static size_t s_transformCopy( char *buffer, const char *text, size_t size, int upper )
{
   if( upper )
      return x_utf8Case( buffer, text, size, 1 );

   memcpy( buffer, text, size );

   return size;
}

/* A string through a template; the template and the value are walked by characters, not by bytes */
static char *s_transformString( const X_ITEM *value, const char *pic, size_t picLen, unsigned flags, size_t *len )
{
   const char *text = value->u.string.data;
   size_t size = value->u.string.len;
   int upper = ( flags & TF_UPPER ) != 0;
   size_t pos = 0;
   size_t step, i;
   char datePic[13];
   char *buffer;

   if( flags & ( TF_DATE | TF_EURO ) )
   {
      x_dateFormat( "XXXXXXXX", datePic, x_setDateFormat( NULL ) );
      pic = datePic;
      picLen = strlen( pic );
   }

   /* A template position holds one character of up to 4 bytes */
   buffer = ( char * )x_xgrab( size + picLen * 4 + 1 );
   *len = 0;
   if( picLen == 0 )
      *len = s_transformCopy( buffer, text, size, upper );

   for( i = 0; i < picLen; i += step )
   {
      int ch, slot;

      step = s_transformStep( pic, picLen, i );
      ch = IIF( step == 1, toupper( ( unsigned char )pic[i] ), 0 );
      slot = s_transformSlot( ch );
      if( pos >= size )
      {
         /* @R: the rest of the template, spaces in the places */
         if( !( flags & TF_REMAIN ) )
            break;

         if( slot )
            buffer[( *len )++] = ' ';
         else
         {
            memcpy( buffer + *len, pic + i, step );
            *len += step;
         }
      }
      else if( slot )
      {
         size_t width = s_transformStep( text, size, pos );
         unsigned char item = ( unsigned char )text[pos];

         if( ch == 'Y' )
            buffer[( *len )++] = IIF( item == 't' || item == 'T' || item == 'y' || item == 'Y', 'Y', 'N' );
         else
            *len += s_transformCopy( buffer + *len, text + pos, width, ch == '!' || upper );

         pos += width;
      }
      else
      {
         /* A template character; without @R it covers a character of the value */
         memcpy( buffer + *len, pic + i, step );
         *len += step;
         if( !( flags & TF_REMAIN ) )
            pos += s_transformStep( text, size, pos );
      }
   }

   if( picLen != 0 && ( flags & TF_REMAIN ) && pos == 0 )
      *len += s_transformCopy( buffer + *len, text, size, upper );

   /* @E: day and month change places */
   if( ( flags & TF_EURO ) && *len >= 5 )
   {
      char first = buffer[0];
      char second = buffer[1];

      buffer[0] = buffer[3];
      buffer[1] = buffer[4];
      buffer[3] = first;
      buffer[4] = second;
   }

   return buffer;
}

/* @E: the SET DATE format with the fields in the order day, month, year */
static void s_transformBritish( const char *format, char british[13] )
{
   const char *fields = IIF( x_setCentury( -1 ), "DDMMYYYY", "DDMMYY" );
   size_t source = 0;
   size_t target = 0;
   size_t field = 0;

   while( format[source] && target < 12 )
   {
      int ch = toupper( ( unsigned char )format[source] );

      if( ( ch == 'Y' || ch == 'M' || ch == 'D' ) && fields[field] )
      {
         char next = fields[field];

         while( fields[field] == next && target < 12 )
            british[target++] = fields[field++];

         do
            ++source;
         while( toupper( ( unsigned char )format[source] ) == ch );
      }
      else
         british[target++] = format[source++];
   }

   british[target] = '\0';
}

/* A date in the SET DATE format; @R keeps the separators of the format */
static char *s_transformDate( const X_ITEM *value, unsigned flags, size_t *len )
{
   const char *format = x_setDateFormat( NULL );
   char date[9], british[13], datePic[13];
   char *buffer;
   size_t i;

   if( flags & TF_EURO )
   {
      s_transformBritish( format, british );
      format = british;
   }

   buffer = ( char * )x_xgrab( 25 );
   x_dateFormat( s_dateToString( ( long )value->u.integer, date ), buffer, format );
   *len = strlen( buffer );
   if( flags & TF_REMAIN )
   {
      x_dateFormat( "99999999", datePic, format );
      for( i = 0; datePic[i]; ++i )
      {
         if( datePic[i] != '9' )
         {
            memmove( buffer + i + 1, buffer + i, *len - i );
            buffer[i] = datePic[i];
            ++*len;
         }
      }
   }

   return buffer;
}

/* The letter of a logical value for a place Y (Y/N) or L and # (T/F) */
static char s_transformLogicalChar( int value, int ch )
{
   if( ch == 'Y' )
      return IIF( value, 'Y', 'N' );

   return IIF( value, 'T', 'F' );
}

/* A logical value; the first place gets the letter, later places with @R get spaces */
static char *s_transformLogical( const X_ITEM *value, const char *pic, size_t picLen, unsigned flags, size_t *len )
{
   size_t pos = 0;
   int done = 0;
   char datePic[13];
   char *buffer;

   if( flags & ( TF_DATE | TF_EURO ) )
   {
      x_dateFormat( "99999999", datePic, x_setDateFormat( NULL ) );
      pic = datePic;
      picLen = strlen( pic );
   }

   buffer = ( char * )x_xgrab( picLen + 2 );
   *len = 0;
   do
   {
      int ch = IIF( pos < picLen, toupper( ( unsigned char )pic[pos] ), 'L' );

      if( ch == 'Y' || ch == 'L' || ch == '#' )
      {
         buffer[( *len )++] = IIF( done, ' ', s_transformLogicalChar( value->u.logical, ch ) );
         done = 1;
      }
      else
         buffer[( *len )++] = pic[pos];

      ++pos;
   } while( ( flags & TF_REMAIN ) && ( pos < picLen || !done ) );

   return buffer;
}

/* The digits of an integer; with absolute the digits of a negative number without the sign */
static int s_transformDigits( const X_ITEM *value, int absolute, char digits[32] )
{
   if( X_IS_UINT64( value ) )
      return snprintf( digits, 32, "%llu", ( unsigned long long )value->u.unsignedInteger );

   if( absolute )
      return snprintf( digits, 32, "%llu", ( unsigned long long )( 0 - ( uint64_t )value->u.integer ) );

   return snprintf( digits, 32, "%lld", ( long long )value->u.integer );
}

/* The number in width characters with decimals places, '*' when it does not fit */
static void s_transformNumberText( const X_ITEM *value, int absolute, int width, int decimals, char *num )
{
   double number = x_itemAsDouble( value );
   char digits[32];
   int size, whole, needed;

   if( !X_IS_INTEGER( value ) )
   {
      needed = snprintf( num, ( size_t )width + 1, "%*.*f", width, decimals, IIF( absolute, -number, number ) );
      if( needed < 0 || needed > width || !isfinite( number ) )
         memset( num, '*', ( size_t )width );

      return;
   }

   size = s_transformDigits( value, absolute, digits );
   whole = width - IIF( decimals, decimals + 1, 0 );
   if( size > whole )
   {
      memset( num, '*', ( size_t )width );
      return;
   }

   memset( num, ' ', ( size_t )( whole - size ) );
   memcpy( num + whole - size, digits, ( size_t )size );
   if( decimals )
   {
      num[whole] = '.';
      memset( num + whole + 1, '0', ( size_t )decimals );
   }
}

/* The number in the places of the template: 9 # $ * take digits, . the point, , a separator between digits */
static void s_transformNumberPicture( const char *num, int width, const char *pic, size_t picLen, unsigned flags,
                                      char *buffer, size_t *len )
{
   size_t pos = 0;

   for( *len = 0; *len < picLen; ++*len )
   {
      size_t at = *len;
      char ch = pic[at];

      if( ch == '9' || ch == '#' || ch == '$' || ch == '*' )
      {
         buffer[at] = IIF( pos < ( size_t )width, num[pos], ' ' );
         if( pos < ( size_t )width )
         {
            /* $ and * fill the leading spaces */
            if( num[pos] == ' ' && ( ch == '$' || ch == '*' ) )
               buffer[at] = ch;

            ++pos;
         }
      }
      else if( ch == '.' && pos < ( size_t )width )
      {
         buffer[at] = IIF( flags & TF_EURO, ',', '.' );
         ++pos;
      }
      else if( ch == ',' && at && pos < ( size_t )width )
      {
         char previous = buffer[at - 1];

         /* A separator between digits; before them it takes the character on its left */
         if( previous >= '0' && previous <= '9' )
            buffer[at] = IIF( flags & TF_EURO, '.', ',' );
         else
            buffer[at] = previous;

         if( previous == '-' )
            buffer[at - 1] = IIF( at > 1 && buffer[at - 2] != '$', buffer[at - 2], ' ' );
      }
      else
         buffer[at] = ch;
   }
}

/* @(: a negative number in parentheses; digits that do not fit become '*' */
static void s_transformParen( const char *pic, size_t picLen, char *buffer, size_t *len )
{
   size_t i;

   if( buffer[0] >= '1' && buffer[0] <= '9' && ( !picLen || pic[0] == '9' || pic[0] != buffer[0] ) )
   {
      for( i = 1; i < *len; ++i )
      {
         if( buffer[i] < '0' || buffer[i] > '9' )
            continue;

         if( !picLen || pic[i] == '9' || pic[i] != buffer[i] )
            buffer[i] = '*';
      }
   }

   buffer[0] = '(';
   buffer[( *len )++] = ')';
}

/* A number through a template, or as Str() without one */
static char *s_transformNumber( const X_ITEM *value, const char *pic, size_t picLen, unsigned *flags, size_t *len,
                                size_t *offset )
{
   int width = 0;
   int decimals = 0;
   int point = 0;
   int isInt = X_IS_INTEGER( value );
   double number = x_itemAsDouble( value );
   int negative = number < 0;
   int absolute = negative && ( *flags & ( TF_DEBIT | TF_PAREN ) );
   int noInt;
   char datePic[13];
   char *buffer, *num;
   size_t i;

   if( *flags & TF_DATE )
   {
      x_dateFormat( "99999999", datePic, x_setDateFormat( NULL ) );
      pic = datePic;
      picLen = strlen( pic );
   }

   for( i = 0; i < picLen; ++i )
   {
      if( pic[i] == '.' && !point )
         point = 1;
      else if( pic[i] == '9' || pic[i] == '#' || pic[i] == '$' || pic[i] == '*' )
      {
         ++width;
         decimals += point;
      }
   }

   if( decimals )
      ++width;

   /* Without a template: the width of Str() */
   if( width == 0 )
   {
      decimals = IIF( x_setFixed || !isInt, x_setDecimals, 0 );
      width = 10;
      if( isInt )
      {
         char digits[32];
         int size = s_transformDigits( value, 0, digits );

         if( size > width )
            width = size;
      }

      width += IIF( decimals, decimals + 1, 0 );
   }

   /* A template without whole digits, such as .99 */
   noInt = decimals > 0 && width == decimals + 1;
   width += noInt;
   num = ( char * )x_xgrab( ( size_t )width + 1 );
   s_transformNumberText( value, absolute, width, decimals, num );
   if( noInt )
   {
      --width;
      if( num[0] == '0' )
         memmove( num, num + 1, ( size_t )width );
      else
      {
         num[0] = '.';
         memset( num + 1, '*', ( size_t )width - 1 );
      }
   }

   num[width] = '\0';
   buffer = ( char * )x_xgrab( IIF( picLen, picLen, ( size_t )width ) + 5 );
   if( picLen == 0 )
   {
      *len = ( size_t )width;
      memcpy( buffer, num, *len );
      if( *flags & TF_EURO )
      {
         for( i = 0; i < *len; ++i )
         {
            if( buffer[i] == '.' )
               buffer[i] = ',';
         }
      }
   }
   else
      s_transformNumberPicture( num, width, pic, picLen, *flags, buffer, len );

   x_xfree( num );
   if( negative && ( *flags & TF_PAREN ) )
   {
      s_transformParen( pic, picLen, buffer, len );
      *offset = 1;
   }

   if( negative && ( *flags & TF_DEBIT ) )
   {
      memcpy( buffer + *len, " DB", 3 );
      *len += 3;
   }
   else if( number > 0 && ( *flags & TF_CREDIT ) )
   {
      memcpy( buffer + *len, " CR", 3 );
      *len += 3;
   }

   if( number != 0 )
      *flags &= ~TF_BLANK;

   return buffer;
}

/* Transform(): a value as text through a picture of functions (@...) and a template */
void x_transform( X_ITEM *result, const X_ITEM *value, const char *pic, size_t picLen )
{
   unsigned flags = s_transformFlags( &pic, &picLen );
   size_t len = 0;
   size_t offset = 0;
   char *buffer;

   if( value->nType == X_IT_STRING )
      buffer = s_transformString( value, pic, picLen, flags, &len );
   else if( value->nType == X_IT_DATE )
      buffer = s_transformDate( value, flags, &len );
   else if( value->nType == X_IT_LOGICAL )
      buffer = s_transformLogical( value, pic, picLen, flags, &len );
   else
      buffer = s_transformNumber( value, pic, picLen, &flags, &len, &offset );

   /* @Z: blank; @B: the leading spaces move to the end */
   if( flags & TF_BLANK )
      memset( buffer, ' ', len );
   else if( flags & TF_LEFT )
   {
      size_t first = offset;

      while( first < len && buffer[first] == ' ' )
         ++first;

      if( first > offset && first < len )
      {
         memmove( buffer + offset, buffer + first, len - first );
         memset( buffer + offset + len - first, ' ', first - offset );
      }
   }

   buffer[len] = '\0';
   x_itemPutStringBuffer( result, buffer, len );
}

/* ---------------------------------------------------------------------------------------------
   Other functions of the API: Empty(), ErrorBlock(), Run(), Set(), ValType() and Version()
   --------------------------------------------------------------------------------------------- */

/* Empty(): determine if the result of an expression is empty */
void x_itemEmpty( X_CALL_CTX *ctx )
{
   const X_ITEM *item = x_paritem( ctx, 1 );
   int result;

   if( item == NULL || X_IS_NIL( item ) )
      result = 1;
   else if( X_IS_STRING( item ) )
      result = x_strEmpty( x_parc( ctx, 1 ), x_parclen( ctx, 1 ) );
   else if( X_IS_DOUBLE( item ) )
      result = x_parnd( ctx, 1 ) == 0.0;
   else if( X_IS_UINT64( item ) )
      result = item->u.unsignedInteger == 0;
   else if( X_IS_NUMERIC( item ) )
      result = x_parnint( ctx, 1 ) == 0;
   else if( X_IS_DATE( item ) )
      result = x_pardl( ctx, 1 ) == 0;
   else if( X_IS_LOGICAL( item ) )
      result = !x_parl( ctx, 1 );
   else if( X_IS_ARRAY( item ) )
      result = x_arrayLen( item ) == 0;
   else if( X_IS_HASH( item ) )
      result = x_hashLen( item ) == 0;
   else
      result = 0;

   x_retl( ctx, result );
}

/* ErrorBlock(): post a code block to execute when a runtime error occurs */
void x_errorErrorBlock( X_CALL_CTX *ctx )
{
   if( x_pcount( ctx ) == 0 )
      x_errorBlockGet( ctx, ctx->result );
   else if( x_pcount( ctx ) == 1 )
      x_errorBlockSet( ctx, ctx->result, x_paritem( ctx, 1 ) );
   else
      x_errorRaiseNamed( ctx->result, 3003, "ERRORBLOCK" );
}

/* Run(): execute an operating system command, target of the RUN command */
void x_sysRun( X_CALL_CTX *ctx )
{
   int suspended = x_gtSuspend() == 0;

   fflush( stdout );
   system( x_parc( ctx, 1 ) );
   if( suspended )
      x_gtResume();
}

/* RunCapture(): execute an operating system command and capture its standard output and error output */
void x_sysRunCapture( X_CALL_CTX *ctx )
{
   const char *command = x_parc( ctx, 1 );
   size_t commandLength = x_parclen( ctx, 1 );
   const char redirect[] = " 2>&1";
   char *shellCommand = ( char * )x_xgrab( commandLength + sizeof( redirect ) );
   char *output = ( char * )x_xgrab( 4096 );
   size_t length = 0;
   size_t capacity = 4096;
   int exitCode = -1;
   FILE *pipe;

   memcpy( shellCommand, command, commandLength );
   memcpy( shellCommand + commandLength, redirect, sizeof( redirect ) );

#if X_OS_WINDOWS
   pipe = _popen( shellCommand, "rb" );
#else
   pipe = popen( shellCommand, "r" );
#endif
   x_xfree( shellCommand );

   if( pipe != NULL )
   {
      for( ;; )
      {
         size_t available = capacity - length - 1;
         size_t count;

         if( available == 0 )
         {
            capacity += capacity / 2;
            output = ( char * )x_xrealloc( output, capacity );
            available = capacity - length - 1;
         }

         count = fread( output + length, 1, available, pipe );
         length += count;
         if( count < available )
            break;
      }

#if X_OS_WINDOWS
      exitCode = _pclose( pipe );
#else
      {
         int status = pclose( pipe );

         if( status != -1 )
         {
            if( WIFEXITED( status ) )
               exitCode = WEXITSTATUS( status );
            else if( WIFSIGNALED( status ) )
               exitCode = 128 + WTERMSIG( status );
         }
      }
#endif
   }

   output[length] = '\0';
   x_stornint( ctx, exitCode, 2 );
   x_retclen_buffer( ctx, output, length );
}

/* Set(): inspect or change a system setting */
void x_setSet( X_CALL_CTX *ctx )
{
   switch( x_parni( ctx, 1 ) )
   {
   case _SET_FIXED:
   case _SET_EXCLUSIVE:
   case _SET_DELETED:
   case _SET_SOFTSEEK:
   case _SET_UNIQUE:
   case _SET_CANCEL:
   case _SET_INSERT:
   case _SET_EXIT:
   case _SET_CONFIRM:
   case _SET_ESCAPE:
   {
      int *setting;
      int on, off;

      switch( x_parni( ctx, 1 ) )
      {
      case _SET_FIXED: setting = &x_setFixed; break;
      case _SET_EXCLUSIVE: setting = &x_setExclusive; break;
      case _SET_DELETED: setting = &x_setDeleted; break;
      case _SET_SOFTSEEK: setting = &x_setSoftSeek; break;
      case _SET_UNIQUE: setting = &x_setUnique; break;
      case _SET_INSERT: setting = &x_setInsert; break;
      case _SET_EXIT: setting = &x_setExit; break;
      case _SET_CONFIRM: setting = &x_setConfirm; break;
      case _SET_ESCAPE: setting = &x_setEscape; break;
      default: setting = &x_setCancel; break;
      }

      on = X_ISCHAR( ctx, 2 ) && x_stricmp( x_parc( ctx, 2 ), "ON" ) == 0;
      off = X_ISCHAR( ctx, 2 ) && x_stricmp( x_parc( ctx, 2 ), "OFF" ) == 0;
      X_ARGCHECK_OPT( ctx, 2, X_ISNIL( ctx, 2 ) || X_ISLOG( ctx, 2 ) || on || off, "SET" );
      x_retl( ctx, *setting );
      if( X_ISLOG( ctx, 2 ) )
         *setting = x_parl( ctx, 2 );
      else if( X_ISCHAR( ctx, 2 ) )
         *setting = x_stricmp( x_parc( ctx, 2 ), "ON" ) == 0;
      break;
   }
   case _SET_DECIMALS:
      X_ARGCHECK_OPT( ctx, 2, X_ISNIL( ctx, 2 ) || ( X_ISNUM( ctx, 2 ) && x_parnint( ctx, 2 ) >= 0 ), "SET" );
      X_ARGCHECK_OPT( ctx, 2, X_ISNIL( ctx, 2 ) || x_parnint( ctx, 2 ) <= INT_MAX - 32, "SET" );
      x_retni( ctx, x_setDecimals );
      if( X_ISNUM( ctx, 2 ) )
         x_setDecimals = x_parni( ctx, 2 );
      break;
   case _SET_PATH:
      X_ARGCHECK_OPT( ctx, 2, X_ISNIL( ctx, 2 ) || X_ISCHAR( ctx, 2 ), "SET" );
      x_retc( ctx, x_setPath( NULL ) );
      if( X_ISCHAR( ctx, 2 ) )
         x_setPath( x_parc( ctx, 2 ) );
      break;
   case _SET_DEFAULT:
      X_ARGCHECK_OPT( ctx, 2, X_ISNIL( ctx, 2 ) || X_ISCHAR( ctx, 2 ), "SET" );
      x_retc( ctx, x_setDefault( NULL ) );
      if( X_ISCHAR( ctx, 2 ) )
         x_setDefault( x_parc( ctx, 2 ) );
      break;
   case _SET_EPOCH:
      X_ARGCHECK_OPT( ctx, 2, X_ISNIL( ctx, 2 ) || ( X_ISNUM( ctx, 2 ) && x_parnint( ctx, 2 ) >= 0 ), "SET" );
      X_ARGCHECK_OPT( ctx, 2, X_ISNIL( ctx, 2 ) || x_parnint( ctx, 2 ) <= 9900, "SET" );
      x_retni( ctx, x_setEpoch );
      if( X_ISNUM( ctx, 2 ) )
         x_setEpoch = x_parni( ctx, 2 );
      break;
   case _SET_COLOR:
   {
      X_GTCHECK( ctx, "SET" );
      X_ARGCHECK_OPT( ctx, 2, X_ISNIL( ctx, 2 ) || X_ISCHAR( ctx, 2 ), "SET" );
      unsigned char colors[X_CLRSTR_LEN];

      x_gtGetColorStr( colors );
      x_retc( ctx, ( const char * )colors );
      if( X_ISCHAR( ctx, 2 ) )
         x_gtSetColorStr( ( unsigned char * )x_parc( ctx, 2 ) );
      break;
   }
   case _SET_CURSOR:
   {
      X_GTCHECK( ctx, "SET" );
      X_ARGCHECK_OPT( ctx, 2, X_ISNIL( ctx, 2 ) || X_ISNUM( ctx, 2 ), "SET" );
      unsigned short shape = 0;

      x_gtGetCursor( &shape );
      x_retni( ctx, shape );
      if( X_ISNUM( ctx, 2 ) )
         x_gtSetCursor( ( unsigned short )x_parni( ctx, 2 ) );
      break;
   }
   case _SET_DATEFORMAT:
      X_ARGCHECK_OPT( ctx, 2, X_ISNIL( ctx, 2 ) || ( X_ISCHAR( ctx, 2 ) && x_parclen( ctx, 2 ) <= 12 ), "SET" );
      x_retc( ctx, x_setDateFormat( NULL ) );
      if( X_ISCHAR( ctx, 2 ) )
         x_setDateFormat( x_parc( ctx, 2 ) );
      break;
   default:
      x_errorRaiseNamed( ctx->result, 1102, "SET" );
      break;
   }
}

/* ValType(): determine the data type returned by an expression */
void x_itemValType( X_CALL_CTX *ctx )
{
   const X_ITEM *item = x_paritem( ctx, 1 );
   const char *type;

   if( item == NULL || X_IS_NIL( item ) )
      type = "U";
   else if( X_IS_STRING( item ) )
      type = "C";
   else if( X_IS_NUMERIC( item ) )
      type = "N";
   else if( X_IS_DATE( item ) )
      type = "D";
   else if( X_IS_LOGICAL( item ) )
      type = "L";
   else if( X_IS_OBJECT( item ) )
      type = "O";
   else if( X_IS_ARRAY( item ) )
      type = "A";
   else if( X_IS_HASH( item ) )
      type = "H";
   else if( X_IS_BLOCK( item ) )
      type = "B";
   else if( X_IS_POINTER( item ) )
      type = "P";
   else
      type = "U";

   x_retc( ctx, type );
}

/* Version(): return xDev version information */
void x_sysVersion( X_CALL_CTX *ctx )
{
   switch( x_parnintdef( ctx, 1, X_VERSION_XDEV ) )
   {
   case X_VERSION_XDEV:
      x_retc( ctx, X_VER_STRING );
      break;
   case X_VERSION_COMPILER:
#if defined( __clang__ )
      x_retc( ctx, "Clang " __clang_version__ );
#elif defined( __GNUC__ )
      x_retc( ctx, "GCC " __VERSION__ );
#elif defined( _MSC_VER )
      x_retc( ctx, "MSVC " X_VER_TEXT( _MSC_VER ) );
#endif
      break;
   case X_VERSION_MAJOR:
      x_retni( ctx, X_VER_MAJOR );
      break;
   case X_VERSION_MINOR:
      x_retni( ctx, X_VER_MINOR );
      break;
   case X_VERSION_RELEASE:
      x_retni( ctx, X_VER_RELEASE );
      break;
   case X_VERSION_PLATFORM:
      x_retc( ctx, X_VER_PLATFORM );
      break;
   case X_VERSION_BITWIDTH:
      x_retni( ctx, ( int )sizeof( void * ) * CHAR_BIT );
      break;
   case X_VERSION_ENDIANNESS:
   {
      const uint16_t value = 1;

      x_retni( ctx, IIF( *( const unsigned char * )&value, X_VERSION_ENDIAN_LITTLE, X_VERSION_ENDIAN_BIG ) );
      break;
   }
   case X_VERSION_BUILD_DATE_STR:
      x_retc( ctx, __DATE__ );
      break;
   case X_VERSION_BUILD_TIME:
      x_retc( ctx, __TIME__ );
      break;
   case X_VERSION_URL_BASE:
      x_retc( ctx, X_VER_URL );
      break;
   default:
      x_ret( ctx );
      break;
   }
}
