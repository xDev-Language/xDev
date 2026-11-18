/*
 * Copyright (c) 2024-2026 xDev-Language
 *
 * Library: xDev Runtime
 * File: modules.c
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "internal.h"

/* One compiled module: its four lifecycle callbacks and how far it got (0 registered, 1 statics set, 2 INIT done, 3 EXIT done) */
typedef struct X_MODULE_NODE
{
   X_FUNCPTR statics;
   X_FUNCPTR init;
   X_FUNCPTR exit;
   X_FUNCPTR clear;
   struct X_MODULE_NODE *next;
   struct X_MODULE_NODE *previous;
   int state;
} X_MODULE_NODE;

/* The modules in the order of registration; the phase of the program: 0 registering, 1 running, 2 EXIT done, 3 cleared */
static X_MODULE_NODE *s_modules;
static X_MODULE_NODE *s_lastModule;
static int s_modulePhase;
static int s_moduleCallback;
static X_ITEM s_commandArguments = X_ITEM_NIL_INIT;

static void s_callbackRestore( int *previous )
{
   s_moduleCallback = *previous;
}

static void s_moduleFree( X_MODULE_NODE **node )
{
   x_xfree( *node );
}

/* ---------------------------------------------------------------------------------------------
   Command line arguments
   --------------------------------------------------------------------------------------------- */

/* The arguments of the program; element 1 is the program itself */
void x_commandArgsInit( int32_t argc, const char *const *argv )
{
   int32_t index;

   x_itemClear( &s_commandArguments );
   x_arrayNew( &s_commandArguments, IIF( argc > 0, argc, 0 ) );
   for( index = 0; index < argc; ++index )
      x_arraySetC( &s_commandArguments, ( size_t )index + 1, argv[index] );
}

/* The number of arguments without the program */
void x_commandArgCount( X_CALL_CTX *ctx )
{
   int64_t count = x_arrayLen( &s_commandArguments );

   x_retnint( ctx, IIF( count > 0, count - 1, 0 ) );
}

/* Argument position; 0 is the program, a position out of range gives "" */
void x_commandArgValue( X_CALL_CTX *ctx, int64_t position )
{
   int64_t count = x_arrayLen( &s_commandArguments );

   if( position >= 0 && position < count )
      x_retc( ctx, x_arrayGetCPtr( &s_commandArguments, ( size_t )position + 1 ) );
   else
      x_retc( ctx, "" );
}

/* ---------------------------------------------------------------------------------------------
   Module lifecycle
   --------------------------------------------------------------------------------------------- */

/* A global constructor of every module registers its callbacks before main() */
void x_moduleRegister( X_FUNCPTR statics, X_FUNCPTR init, X_FUNCPTR exit, X_FUNCPTR clear )
{
   X_MODULE_NODE *node;

   if( s_modulePhase != 0 )
      return;

   node = ( X_MODULE_NODE * )x_xgrab( sizeof( *node ) );
   *node = ( X_MODULE_NODE ){ statics, init, exit, clear, NULL, s_lastModule, 0 };
   if( s_lastModule != NULL )
      s_lastModule->next = node;
   else
      s_modules = node;

   s_lastModule = node;
}

/* The statics of all modules first, then the INIT procedures, both in the order of registration */
void x_modulesInit( int32_t argc, const X_ARG *args )
{
   X_MODULE_NODE *node;

   if( s_modulePhase != 0 )
      return;

   s_modulePhase = 1;
   int previous __attribute__( ( cleanup( s_callbackRestore ) ) ) = s_moduleCallback;
   s_moduleCallback = 1;

   for( node = s_modules; node != NULL; node = node->next )
   {
      X_ITEM result __attribute__( ( cleanup( x_itemClear ) ) ) = X_ITEM_NIL_INIT;
      X_CALL_CTX call = { NULL, 0, &result, NULL, NULL };

      node->state = 1;
      node->statics( &call );
   }

   for( node = s_modules; node != NULL; node = node->next )
   {
      X_ITEM result __attribute__( ( cleanup( x_itemClear ) ) ) = X_ITEM_NIL_INIT;
      X_CALL_CTX call = { args, argc, &result, NULL, NULL };

      node->state = 2;
      node->init( &call );
   }
}

/* The EXIT procedures of the modules whose INIT ran, in the reverse order; not from inside a callback */
void x_modulesExit( void )
{
   X_MODULE_NODE *node;

   if( s_modulePhase != 1 || s_moduleCallback )
      return;

   s_modulePhase = 2;
   int previous __attribute__( ( cleanup( s_callbackRestore ) ) ) = s_moduleCallback;
   s_moduleCallback = 1;

   for( node = s_lastModule; node != NULL; node = node->previous )
   {
      if( node->state == 2 )
      {
         X_ITEM result __attribute__( ( cleanup( x_itemClear ) ) ) = X_ITEM_NIL_INIT;
         X_CALL_CTX call = { NULL, 0, &result, NULL, NULL };

         node->state = 3;
         x_callCleanup( node->exit, &call );
      }
   }
}

/* EXIT if it did not run yet, then the statics of every started module are cleared and the module released, in the reverse order */
void x_modulesClear( void )
{
   X_MODULE_NODE *node;

   if( s_modulePhase == 3 || s_moduleCallback )
      return;

   x_modulesExit();
   s_modulePhase = 3;

   while( ( node = s_lastModule ) != NULL )
   {
      X_MODULE_NODE *owned __attribute__( ( cleanup( s_moduleFree ) ) ) = node;

      s_lastModule = node->previous;
      if( node->state != 0 )
      {
         X_ITEM result __attribute__( ( cleanup( x_itemClear ) ) ) = X_ITEM_NIL_INIT;
         X_CALL_CTX call = { NULL, 0, &result, NULL, NULL };

         x_callCleanup( node->clear, &call );
      }
   }

   s_modules = NULL;
   s_modulePhase = 0;
   x_itemClear( &s_commandArguments );
}
