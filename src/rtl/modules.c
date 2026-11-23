/*
 * Copyright (c) 2024-2026 xDev-Language
 *
 * Library...: xDev Runtime
 * File......: modules.c
 *
 */

#include "internal.h"
#include "classes.h"
#include "platform.h"
#include "version.h"

#if X_OS_WINDOWS
#include <windows.h>
#endif

/* One compiled module: its four lifecycle callbacks, the DLL of ModuleLoad() it came from (NULL for the
   modules of the program) and how far it got (0 registered, 1 statics set, 2 INIT done, 3 EXIT done) */
typedef struct X_MODULE_NODE
{
   X_FUNCPTR statics;
   X_FUNCPTR init;
   X_FUNCPTR exit;
   X_FUNCPTR clear;
   struct X_MODULE_NODE *next;
   struct X_MODULE_NODE *previous;
   void *owner;
   int state;
} X_MODULE_NODE;

/* x_moduleVersion() of a DLL module of xdev --shared */
typedef int32_t ( *X_MODULE_VERSION )( void );

/* The modules in the order of registration; the phase of the program: 0 registering, 1 running, 2 EXIT done, 3 cleared */
static X_MODULE_NODE *s_modules;
static X_MODULE_NODE *s_lastModule;
static int s_modulePhase;
static int s_moduleCallback;
static X_ITEM s_commandArguments = X_ITEM_NIL_INIT;

/* The DLLs of ModuleLoad(); the handle is the position from 1, a freed DLL leaves NULL and its handle is not reused */
static void **s_libraries;
static size_t s_libraryCount;

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

/* A global constructor of every module registers its callbacks: before main(), or inside the LoadLibrary() of
   ModuleLoad(), which starts the module after LoadLibrary() returns */
void x_moduleRegister( X_FUNCPTR statics, X_FUNCPTR init, X_FUNCPTR exit, X_FUNCPTR clear )
{
   X_MODULE_NODE *node;

   if( s_modulePhase > 1 )
      return;

   node = ( X_MODULE_NODE * )x_xgrab( sizeof( *node ) );
   *node = ( X_MODULE_NODE ){ statics, init, exit, clear, NULL, s_lastModule, NULL, 0 };
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
   x_xfree( s_libraries );
   s_libraries = NULL;
   s_libraryCount = 0;
}

/* ---------------------------------------------------------------------------------------------
   Modules loaded while the program runs: ModuleLoad(), ModuleCall(), ModuleUnload()
   --------------------------------------------------------------------------------------------- */

#if X_OS_WINDOWS
/* A runtime error of the module functions */
static void s_moduleFail( X_CALL_CTX *ctx, int genCode, int subCode, const char *description, const char *operation )
{
   X_ERROR_DEF error = { 2, genCode, subCode, 0, "BASE", description, operation };

   x_errorRaiseDefinition( ctx->result, &error, NULL, NULL );
}

/* A UTF-8 path as a wide string, allocated by x_xgrab() */
static wchar_t *s_wideName( const char *path )
{
   int size = MultiByteToWideChar( CP_UTF8, 0, path, -1, NULL, 0 );
   wchar_t *name = ( wchar_t * )x_xgrabArray( ( size_t )size, sizeof( wchar_t ) );

   MultiByteToWideChar( CP_UTF8, 0, path, -1, name, size );

   return name;
}

/* The bytes of a loaded DLL, from its PE header */
static size_t s_librarySize( HMODULE library )
{
   const IMAGE_DOS_HEADER *dos = ( const IMAGE_DOS_HEADER * )library;
   const IMAGE_NT_HEADERS *headers = ( const IMAGE_NT_HEADERS * )( ( const char * )library + dos->e_lfanew );

   return headers->OptionalHeader.SizeOfImage;
}

/* The handle of a DLL already in the table; 0 when it is not there */
static int64_t s_libraryHandle( HMODULE library )
{
   size_t i;

   for( i = 0; i < s_libraryCount; ++i )
   {
      if( s_libraries[i] == ( void * )library )
         return ( int64_t )i + 1;
   }

   return 0;
}

/* The DLL of a handle; NULL when the handle was never given or the DLL is freed */
static HMODULE s_library( int64_t handle )
{
   if( handle < 1 || ( uint64_t )handle > s_libraryCount )
      return NULL;

   return ( HMODULE )s_libraries[handle - 1];
}

/* A new handle at the end of the table */
static int64_t s_libraryAdd( HMODULE library )
{
   s_libraries = ( void ** )x_xreallocArray( s_libraries, s_libraryCount + 1, sizeof( void * ) );
   s_libraries[s_libraryCount++] = ( void * )library;

   return ( int64_t )s_libraryCount;
}

/* Take a node out of the list of modules */
static void s_moduleUnlink( X_MODULE_NODE *node )
{
   if( node->previous != NULL )
      node->previous->next = node->next;
   else
      s_modules = node->next;

   if( node->next != NULL )
      node->next->previous = node->previous;
   else
      s_lastModule = node->previous;
}

/* The modules registered after mark come from a DLL that is not kept: they go without running */
static void s_modulesDiscard( X_MODULE_NODE *mark )
{
   X_MODULE_NODE *node;

   while( ( node = s_lastModule ) != mark )
   {
      s_moduleUnlink( node );
      x_xfree( node );
   }
}

/* The modules registered after mark belong to library: their statics are set, then their INIT procedures run */
static void s_modulesStart( X_MODULE_NODE *mark, HMODULE library )
{
   X_MODULE_NODE *first = IIF( mark != NULL, mark->next, s_modules );
   X_MODULE_NODE *node;

   int previous __attribute__( ( cleanup( s_callbackRestore ) ) ) = s_moduleCallback;
   s_moduleCallback = 1;

   for( node = first; node != NULL; node = node->next )
      node->owner = ( void * )library;

   for( node = first; node != NULL; node = node->next )
   {
      if( node->owner == ( void * )library && node->state == 0 )
      {
         X_ITEM result __attribute__( ( cleanup( x_itemClear ) ) ) = X_ITEM_NIL_INIT;
         X_CALL_CTX call = { NULL, 0, &result, NULL, NULL };

         node->state = 1;
         node->statics( &call );
      }
   }

   for( node = first; node != NULL; node = node->next )
   {
      if( node->owner == ( void * )library && node->state == 1 )
      {
         X_ITEM result __attribute__( ( cleanup( x_itemClear ) ) ) = X_ITEM_NIL_INIT;
         X_CALL_CTX call = { NULL, 0, &result, NULL, NULL };

         node->state = 2;
         node->init( &call );
      }
   }
}

/* The EXIT procedures of the modules of library in the reverse order, then their statics are cleared and they go */
static void s_modulesStop( HMODULE library )
{
   X_MODULE_NODE *node;
   X_MODULE_NODE *previousNode;

   int previous __attribute__( ( cleanup( s_callbackRestore ) ) ) = s_moduleCallback;
   s_moduleCallback = 1;

   for( node = s_lastModule; node != NULL; node = node->previous )
   {
      if( node->owner == ( void * )library && node->state == 2 )
      {
         X_ITEM result __attribute__( ( cleanup( x_itemClear ) ) ) = X_ITEM_NIL_INIT;
         X_CALL_CTX call = { NULL, 0, &result, NULL, NULL };

         node->state = 3;
         x_callCleanup( node->exit, &call );
      }
   }

   for( node = s_lastModule; node != NULL; node = previousNode )
   {
      previousNode = node->previous;
      if( node->owner != ( void * )library )
         continue;

      if( node->state != 0 )
      {
         X_ITEM result __attribute__( ( cleanup( x_itemClear ) ) ) = X_ITEM_NIL_INIT;
         X_CALL_CTX call = { NULL, 0, &result, NULL, NULL };

         x_callCleanup( node->clear, &call );
      }

      s_moduleUnlink( node );
      x_xfree( node );
   }
}
#endif

/* ModuleLoad( cFile ): the DLL of xdev --shared is loaded, its statics are set and its INIT procedures run; a DLL
   loaded before gives its handle again */
void x_moduleLoad( X_CALL_CTX *ctx )
{
#if X_OS_WINDOWS
   const char *path = x_parc( ctx, 1 );
   X_MODULE_NODE *mark = s_lastModule;
   wchar_t *name = s_wideName( path );
   HMODULE library = LoadLibraryW( name );
   X_MODULE_VERSION version;
   const char *description = NULL;
   int subCode = 0;
   int64_t handle;

   x_xfree( name );

   if( library == NULL )
   {
      s_modulesDiscard( mark );
      s_moduleFail( ctx, 21, 1950, "Module cannot be loaded", path );
      return;
   }

   handle = s_libraryHandle( library );
   if( handle != 0 )
   {
      FreeLibrary( library );
      x_retnint( ctx, handle );
      return;
   }

   version = ( X_MODULE_VERSION )( void ( * )( void ) )GetProcAddress( library, "x_moduleVersion" );
   if( version == NULL )
   {
      description = "File is not an xDev module";
      subCode = 1951;
   }
   else if( version() != X_VER_MODULE )
   {
      description = "Module was built for another runtime version";
      subCode = 1952;
   }

   if( description != NULL )
   {
      s_modulesDiscard( mark );
      FreeLibrary( library );
      s_moduleFail( ctx, 30, subCode, description, path );
      return;
   }

   handle = s_libraryAdd( library );
   s_modulesStart( mark, library );
   x_retnint( ctx, handle );
#else
   x_errorRaiseNamed( ctx->result, 1900, "MODULELOAD" );
#endif
}

/* ModuleCall( nHandle, cFunction, ... ): a public function of a loaded module gets the arguments after cFunction */
void x_moduleCall( X_CALL_CTX *ctx )
{
#if X_OS_WINDOWS
   HMODULE library = s_library( x_parnint( ctx, 1 ) );
   const char *name = x_parc( ctx, 2 );
   X_CALL_CTX call = { ctx->args + 2, ctx->argc - 2, ctx->result, NULL, NULL };
   X_FUNCPTR function;
   char *symbol;

   if( library == NULL )
   {
      s_moduleFail( ctx, 1, 1953, "Module is not loaded", "MODULECALL" );
      return;
   }

   symbol = x_strupr( x_strdup( name ) );
   function = ( X_FUNCPTR )( void ( * )( void ) )GetProcAddress( library, symbol );
   x_xfree( symbol );

   if( function == NULL )
   {
      s_moduleFail( ctx, 12, 1954, "Function does not exist in the module", name );
      return;
   }

   function( &call );
#else
   x_errorRaiseNamed( ctx->result, 1900, "MODULECALL" );
#endif
}

/* ModuleUnload( nHandle ): the EXIT procedures of the module run, its statics are cleared and the DLL is freed;
   .F. when the handle is not loaded, the program is ending or a class of the module is registered */
void x_moduleUnload( X_CALL_CTX *ctx )
{
#if X_OS_WINDOWS
   const int64_t handle = x_parnint( ctx, 1 );
   HMODULE library = s_library( handle );

   if( library == NULL || s_modulePhase != 1 || x_clsUsesCode( library, s_librarySize( library ) ) )
   {
      x_retl( ctx, 0 );
      return;
   }

   s_modulesStop( library );
   s_libraries[handle - 1] = NULL;
   FreeLibrary( library );
   x_retl( ctx, 1 );
#else
   x_errorRaiseNamed( ctx->result, 1900, "MODULEUNLOAD" );
#endif
}
