/*
 * Copyright (c) 2024-2026 xDev-Language
 *
 * Library...: xDev CURL
 * File......: internal-curl.c
 *
 */

#include "internal-curl.h"

#include <xdev/fileio.ch>
#include <stdio.h>
#include <string.h>

#if X_OS_WINDOWS
   #pragma comment( lib, "advapi32" )
   #pragma comment( lib, "bcrypt" )
   #pragma comment( lib, "crypt32" )
   #pragma comment( lib, "gdi32" )
   #pragma comment( lib, "iphlpapi" )
   #pragma comment( lib, "secur32" )
   #pragma comment( lib, "user32" )
   #pragma comment( lib, "wldap32" )
   #pragma comment( lib, "ws2_32" )
#else
   #pragma comment( lib, "pthread" )
   #pragma comment( lib, "dl" )
#endif

static int s_initialized = 0;

/* ---------------------------------------------------------------------------------------------
   Common
   --------------------------------------------------------------------------------------------- */

/* Append bytes to a buffer; the buffer doubles when it is full */
void x_curlAppend( X_CURL_BUF *buf, const char *text, size_t len )
{
   if( buf->len + len + 1 > buf->cap )
   {
      size_t cap = IIF( buf->cap == 0, 4096, buf->cap );

      while( buf->len + len + 1 > cap )
         cap *= 2;

      buf->data = ( char * )x_xrealloc( buf->data, cap );
      buf->cap = cap;
   }

   if( len > 0 )
      memcpy( buf->data + buf->len, text, len );

   buf->len += len;
   buf->data[buf->len] = '\0';
}

/* Release the memory of a buffer */
void x_curlBufFree( X_CURL_BUF *buf )
{
   if( buf->data != NULL )
      x_xfree( buf->data );

   buf->data = NULL;
   buf->len = 0;
   buf->cap = 0;
}

/* A new handle of the given kind, filled with zeros */
void *x_curlObjectNew( int kind, size_t size )
{
   X_CURL_HEAD *head = ( X_CURL_HEAD * )x_xgrabz( size );

   head->magic = X_CURL_MAGIC;
   head->kind = kind;

   return head;
}

/* Release a handle; its mark is cleared, so a later use is detected */
void x_curlObjectFree( void *object )
{
   ( ( X_CURL_HEAD * )object )->magic = 0;
   x_xfree( object );
}

/* The handle of the given kind in argument n, or NULL */
void *x_curlParam( X_CALL_CTX *ctx, int n, int kind )
{
   X_CURL_HEAD *head = IIF( X_ISPOINTER( ctx, n ), ( X_CURL_HEAD * )x_parptr( ctx, n ), NULL );

   return IIF( head != NULL && head->magic == X_CURL_MAGIC && head->kind == kind, head, NULL );
}

/* The string in argument n, or NULL */
const char *x_curlString( X_CALL_CTX *ctx, int n )
{
   return IIF( X_ISCHAR( ctx, n ), x_parc( ctx, n ), NULL );
}

/* The value of argument n; NIL when it is missing */
const X_ITEM *x_curlValue( X_CALL_CTX *ctx, int n )
{
   static const X_ITEM s_nil = X_ITEM_NIL_INIT;
   const X_ITEM *value = x_paritem( ctx, n );

   return IIF( value != NULL, value, &s_nil );
}

/* Return a new handle; NIL when it could not be created */
void x_curlRetObject( X_CALL_CTX *ctx, void *object )
{
   if( object != NULL )
      x_retptr( ctx, object );
}

/* Initialize libcurl once */
int x_curlGlobalInit( long flags )
{
   CURLcode code = CURLE_OK;

   if( !s_initialized )
   {
      code = curl_global_init( flags );
      s_initialized = code == CURLE_OK;
   }

   return ( int )code;
}

/* Release libcurl */
void x_curlGlobalCleanup( void )
{
   if( s_initialized )
      curl_global_cleanup();

   s_initialized = 0;
}

/* Transfers run inside this pair: a block called by libcurl stores its error,
   which is raised after libcurl has returned */
void x_curlBegin( void )
{
   x_callbackEnterDll();
}

/* The end of the pair started by x_curlBegin() */
void x_curlEnd( void )
{
   x_callbackLeaveDll();
   x_callbackRaisePending();
}

/* A libcurl list from the strings of an array; other elements are skipped */
struct curl_slist *x_curlSlist( const X_ITEM *array )
{
   struct curl_slist *list = NULL;
   int64_t count = IIF( array != NULL && X_IS_ARRAY( array ), x_arrayLen( array ), 0 );
   int64_t i;

   for( i = 1; i <= count; i++ )
   {
      const X_ITEM *item = x_arrayGetItemPtr( array, ( size_t )i );
      struct curl_slist *next;

      if( !X_IS_STRING( item ) )
         continue;

      next = curl_slist_append( list, x_itemGetCPtr( item ) );
      if( next != NULL )
         list = next;
   }

   return list;
}

/* An array of strings from a libcurl list */
void x_curlArrayFromSlist( X_ITEM *result, const struct curl_slist *list )
{
   const struct curl_slist *node;
   size_t count = 0;

   for( node = list; node != NULL; node = node->next )
      count++;

   x_arrayNew( result, ( int64_t )count );
   count = 0;

   for( node = list; node != NULL; node = node->next )
      x_arraySetC( result, ++count, node->data );
}

/* ---------------------------------------------------------------------------------------------
   Easy callbacks
   --------------------------------------------------------------------------------------------- */

/* Received data: to the download file or to the memory buffer */
static size_t s_write( char *data, size_t size, size_t count, void *userData )
{
   X_CURL_EASY *easy = ( X_CURL_EASY * )userData;
   size_t len = size * count;

   if( easy->downloadFile != F_ERROR )
      return x_fsWrite( easy->downloadFile, data, len );

   x_curlAppend( &easy->buffer, data, len );

   return len;
}

/* Data to send: from the upload file or from the upload buffer */
static size_t s_read( char *data, size_t size, size_t count, void *userData )
{
   X_CURL_EASY *easy = ( X_CURL_EASY * )userData;
   size_t len = size * count;

   if( easy->uploadFile != F_ERROR )
      return x_fsRead( easy->uploadFile, data, len );

   if( len > easy->upload.len - easy->uploadPos )
      len = easy->upload.len - easy->uploadPos;

   if( len > 0 )
      memcpy( data, easy->upload.data + easy->uploadPos, len );

   easy->uploadPos += len;

   return len;
}

/* Rewind the upload, e.g. when a redirect sends the data again */
static int s_seek( void *userData, curl_off_t offset, int origin )
{
   X_CURL_EASY *easy = ( X_CURL_EASY * )userData;

   if( origin != SEEK_SET )
      return CURL_SEEKFUNC_CANTSEEK;

   if( easy->uploadFile != F_ERROR )
   {
      if( x_fsSeek( easy->uploadFile, ( int64_t )offset, FS_SET ) != ( int64_t )offset )
         return CURL_SEEKFUNC_FAIL;

      return CURL_SEEKFUNC_OK;
   }

   if( offset < 0 || ( size_t )offset > easy->upload.len )
      return CURL_SEEKFUNC_FAIL;

   easy->uploadPos = ( size_t )offset;

   return CURL_SEEKFUNC_OK;
}

/* Evaluate the block in the first argument with the other arguments */
static void s_blockCall( X_CALL_CTX *ctx )
{
   x_evalBlock( ctx, ctx->result, ctx->args[0].value, ctx->argc - 1, ctx->args + 1 );
}

/* Progress of the transfer: calls the block; .T. or an error in the block stops the transfer */
static int s_progress( void *userData, curl_off_t dlTotal, curl_off_t dlNow, curl_off_t ulTotal, curl_off_t ulNow )
{
   X_CURL_EASY *easy = ( X_CURL_EASY * )userData;
   X_ITEM result __attribute__( ( cleanup( x_itemClear ) ) ) = X_ITEM_NIL_INIT;
   X_ITEM values[4] = { X_ITEM_NIL_INIT, X_ITEM_NIL_INIT, X_ITEM_NIL_INIT, X_ITEM_NIL_INIT };
   X_ARG args[5];

   x_itemPutNInt( &values[0], ( int64_t )dlTotal );
   x_itemPutNInt( &values[1], ( int64_t )dlNow );
   x_itemPutNInt( &values[2], ( int64_t )ulTotal );
   x_itemPutNInt( &values[3], ( int64_t )ulNow );

   args[0] = ( X_ARG ){ &easy->progress, 0 };
   args[1] = ( X_ARG ){ &values[0], 0 };
   args[2] = ( X_ARG ){ &values[1], 0 };
   args[3] = ( X_ARG ){ &values[2], 0 };
   args[4] = ( X_ARG ){ &values[3], 0 };

   if( !x_callbackInvoke( s_blockCall, &result, 5, args ) )
      return 1;

   return IIF( X_IS_LOGICAL( &result ) && x_itemGetL( &result ), 1, 0 );
}

/* Verbose trace: calls the block with the type and the text */
static int s_debug( CURL *curl, curl_infotype type, char *data, size_t size, void *userData )
{
   X_CURL_EASY *easy = ( X_CURL_EASY * )userData;
   X_ITEM result __attribute__( ( cleanup( x_itemClear ) ) ) = X_ITEM_NIL_INIT;
   X_ITEM text __attribute__( ( cleanup( x_itemClear ) ) ) = X_ITEM_NIL_INIT;
   X_ITEM kind = X_ITEM_NIL_INIT;
   X_ARG args[3];

   UNUSED( curl );

   x_itemPutNI( &kind, ( int )type );
   x_itemPutCL( &text, data, size );

   args[0] = ( X_ARG ){ &easy->debug, 0 };
   args[1] = ( X_ARG ){ &kind, 0 };
   args[2] = ( X_ARG ){ &text, 0 };

   x_callbackInvoke( s_blockCall, &result, 3, args );

   return 0;
}

/* Connect the callbacks of a handle */
static void s_easySetup( X_CURL_EASY *easy )
{
   CURL *curl = easy->curl;

   curl_easy_setopt( curl, CURLOPT_WRITEFUNCTION, s_write );
   curl_easy_setopt( curl, CURLOPT_WRITEDATA, easy );
   curl_easy_setopt( curl, CURLOPT_READFUNCTION, s_read );
   curl_easy_setopt( curl, CURLOPT_READDATA, easy );
   curl_easy_setopt( curl, CURLOPT_SEEKFUNCTION, s_seek );
   curl_easy_setopt( curl, CURLOPT_SEEKDATA, easy );
   curl_easy_setopt( curl, CURLOPT_XFERINFOFUNCTION, s_progress );
   curl_easy_setopt( curl, CURLOPT_XFERINFODATA, easy );
   curl_easy_setopt( curl, CURLOPT_NOPROGRESS, IIF( X_IS_BLOCK( &easy->progress ), 0L, 1L ) );
   curl_easy_setopt( curl, CURLOPT_DEBUGFUNCTION, s_debug );
   curl_easy_setopt( curl, CURLOPT_DEBUGDATA, easy );
#if X_OS_WINDOWS
   curl_easy_setopt( curl, CURLOPT_SSL_OPTIONS, ( long )CURLSSLOPT_NATIVE_CA );
#endif
}

/* ---------------------------------------------------------------------------------------------
   Easy handle
   --------------------------------------------------------------------------------------------- */

/* Close the download and upload files */
static void s_closeFiles( X_CURL_EASY *easy )
{
   if( easy->downloadFile != F_ERROR )
      x_fsClose( easy->downloadFile );

   if( easy->uploadFile != F_ERROR )
      x_fsClose( easy->uploadFile );

   easy->downloadFile = F_ERROR;
   easy->uploadFile = F_ERROR;
}

/* Release the kept lists */
static void s_freeSlists( X_CURL_EASY *easy )
{
   size_t i;

   for( i = 0; i < easy->slistCount; i++ )
      curl_slist_free_all( easy->slists[i].list );

   easy->slistCount = 0;
}

/* Keep the list of an option; a new list replaces the old one, NULL removes it */
static void s_keepSlist( X_CURL_EASY *easy, CURLoption option, struct curl_slist *list )
{
   size_t i;

   for( i = 0; i < easy->slistCount && easy->slists[i].option != option; i++ )
      ;

   if( i < easy->slistCount )
   {
      curl_slist_free_all( easy->slists[i].list );

      if( list == NULL )
      {
         easy->slists[i] = easy->slists[--easy->slistCount];
         return;
      }
   }
   else if( list == NULL )
      return;
   else
   {
      if( easy->slistCount == easy->slistCap )
      {
         easy->slistCap = IIF( easy->slistCap == 0, 4, easy->slistCap * 2 );
         easy->slists = ( X_CURL_SLIST * )x_xreallocArray( easy->slists, easy->slistCap, sizeof( X_CURL_SLIST ) );
      }

      easy->slists[easy->slistCount++].option = option;
   }

   easy->slists[i].list = list;
}

/* A new easy handle for a libcurl handle */
X_CURL_EASY *x_curlEasyNew( CURL *curl )
{
   X_CURL_EASY *easy = ( X_CURL_EASY * )x_curlObjectNew( X_CURL_KIND_EASY, sizeof( X_CURL_EASY ) );

   easy->curl = curl;
   easy->downloadFile = F_ERROR;
   easy->uploadFile = F_ERROR;
   easy->progress = ( X_ITEM )X_ITEM_NIL_INIT;
   easy->debug = ( X_ITEM )X_ITEM_NIL_INIT;

   s_easySetup( easy );

   return easy;
}

/* A copy of a handle with its options, blocks, upload data and lists; files are not copied */
X_CURL_EASY *x_curlEasyDup( const X_CURL_EASY *source )
{
   CURL *curl = curl_easy_duphandle( source->curl );
   X_CURL_EASY *easy;
   size_t i;

   if( curl == NULL )
      return NULL;

   easy = x_curlEasyNew( curl );

   x_itemCopy( &easy->progress, &source->progress );
   x_itemCopy( &easy->debug, &source->debug );
   x_curlAppend( &easy->upload, source->upload.data, source->upload.len );
   s_easySetup( easy );

   for( i = 0; i < source->slistCount; i++ )
   {
      struct curl_slist *copy = NULL;
      const struct curl_slist *node;

      for( node = source->slists[i].list; node != NULL; node = node->next )
         copy = curl_slist_append( copy, node->data );

      curl_easy_setopt( curl, source->slists[i].option, copy );
      s_keepSlist( easy, source->slists[i].option, copy );
   }

   return easy;
}

/* Release a handle; it leaves its multi handle first */
void x_curlEasyFree( X_CURL_EASY *easy )
{
   if( easy->multi != NULL )
      x_curlMultiRemove( easy->multi, easy );

   curl_easy_cleanup( easy->curl );

   s_closeFiles( easy );
   s_freeSlists( easy );

   if( easy->slists != NULL )
      x_xfree( easy->slists );

   x_curlBufFree( &easy->buffer );
   x_curlBufFree( &easy->upload );
   x_itemClear( &easy->progress );
   x_itemClear( &easy->debug );
   x_curlObjectFree( easy );
}

/* Restore the defaults; files, blocks, buffers and lists are released */
void x_curlEasyReset( X_CURL_EASY *easy )
{
   curl_easy_reset( easy->curl );

   s_closeFiles( easy );
   s_freeSlists( easy );
   x_curlBufFree( &easy->buffer );
   x_curlBufFree( &easy->upload );
   easy->uploadPos = 0;
   x_itemClear( &easy->progress );
   x_itemClear( &easy->debug );

   s_easySetup( easy );
}

/* Before a transfer: empty the buffer and rewind the upload */
void x_curlEasyPrepare( X_CURL_EASY *easy )
{
   easy->buffer.len = 0;
   easy->uploadPos = 0;

   if( easy->uploadFile != F_ERROR )
      x_fsSeek( easy->uploadFile, 0, FS_SET );
}

/* The libcurl handle inside a handle value, or NULL */
static void *s_objectHandle( const X_ITEM *value )
{
   X_CURL_HEAD *head = IIF( X_IS_POINTER( value ), ( X_CURL_HEAD * )x_itemGetPtr( value ), NULL );

   if( head == NULL || head->magic != X_CURL_MAGIC )
      return NULL;

   switch( head->kind )
   {
   case X_CURL_KIND_EASY: return ( ( X_CURL_EASY * )head )->curl;
   case X_CURL_KIND_SHARE: return ( ( X_CURL_SHARE * )head )->share;
   case X_CURL_KIND_URL: return ( ( X_CURL_URL * )head )->url;
   case X_CURL_KIND_MIME: return ( ( X_CURL_MIME * )head )->mime;
   }

   return NULL;
}

/* Set an option; the type of the option decides how the value is passed */
CURLcode x_curlEasySetopt( X_CURL_EASY *easy, CURLoption option, const X_ITEM *value )
{
   const struct curl_easyoption *info = curl_easy_option_by_id( option );
   CURL *curl = easy->curl;
   int text = X_IS_STRING( value );

   if( info == NULL )
      return CURLE_UNKNOWN_OPTION;

   /* libcurl declares the request body as an object because it may be binary; it is always copied */
   if( option == CURLOPT_POSTFIELDS || option == CURLOPT_COPYPOSTFIELDS )
   {
      if( !text )
         return curl_easy_setopt( curl, CURLOPT_POSTFIELDS, NULL );

      curl_easy_setopt( curl, CURLOPT_POSTFIELDSIZE_LARGE, ( curl_off_t )x_itemGetCLen( value ) );

      return curl_easy_setopt( curl, CURLOPT_COPYPOSTFIELDS, x_itemGetCPtr( value ) );
   }

   switch( info->type )
   {
   case CURLOT_LONG:
   case CURLOT_VALUES:
   {
      long number = ( long )x_itemGetNInt( value );

      if( X_IS_LOGICAL( value ) )
         number = ( long )x_itemGetL( value );

      return curl_easy_setopt( curl, option, number );
   }
   case CURLOT_OFF_T:
      return curl_easy_setopt( curl, option, ( curl_off_t )x_itemGetNInt( value ) );

   case CURLOT_STRING:
      return curl_easy_setopt( curl, option, IIF( text, x_itemGetCPtr( value ), NULL ) );

   case CURLOT_SLIST:
   {
      struct curl_slist *list = x_curlSlist( value );
      CURLcode code = curl_easy_setopt( curl, option, list );

      if( code == CURLE_OK )
         s_keepSlist( easy, option, list );
      else if( list != NULL )
         curl_slist_free_all( list );

      return code;
   }
   case CURLOT_BLOB:
   {
      struct curl_blob blob;

      if( !text )
         return curl_easy_setopt( curl, option, NULL );

      blob.data = ( void * )x_itemGetCPtr( value );
      blob.len = x_itemGetCLen( value );
      blob.flags = CURL_BLOB_COPY;

      return curl_easy_setopt( curl, option, &blob );
   }
   case CURLOT_OBJECT:
   {
      void *handle = s_objectHandle( value );

      if( handle == NULL && !X_IS_NIL( value ) )
         return CURLE_BAD_FUNCTION_ARGUMENT;

      return curl_easy_setopt( curl, option, handle );
   }
   case CURLOT_CBPTR:
   case CURLOT_FUNCTION:
      break;
   }

   return CURLE_BAD_FUNCTION_ARGUMENT;
}

/* CURLINFO_CERTINFO as an array with one array of strings for each certificate */
static void s_certinfo( X_ITEM *result, const struct curl_certinfo *certinfo )
{
   int i;

   x_arrayNew( result, IIF( certinfo != NULL, certinfo->num_of_certs, 0 ) );

   for( i = 0; certinfo != NULL && i < certinfo->num_of_certs; i++ )
   {
      X_ITEM cert __attribute__( ( cleanup( x_itemClear ) ) ) = X_ITEM_NIL_INIT;

      x_curlArrayFromSlist( &cert, certinfo->certinfo[i] );
      x_arraySet( result, ( size_t )i + 1, &cert );
   }
}

/* Read information; the type of the information decides the type of the result */
CURLcode x_curlEasyGetinfo( X_CURL_EASY *easy, CURLINFO info, X_ITEM *result )
{
   CURL *curl = easy->curl;
   CURLcode code = CURLE_BAD_FUNCTION_ARGUMENT;

   switch( info & CURLINFO_TYPEMASK )
   {
   case CURLINFO_STRING:
   {
      char *value = NULL;

      code = curl_easy_getinfo( curl, info, &value );
      if( code == CURLE_OK && value != NULL )
         x_itemPutC( result, value );

      break;
   }
   case CURLINFO_LONG:
   {
      long value = 0;

      code = curl_easy_getinfo( curl, info, &value );
      if( code == CURLE_OK )
         x_itemPutNInt( result, value );

      break;
   }
   case CURLINFO_DOUBLE:
   {
      double value = 0;

      code = curl_easy_getinfo( curl, info, &value );
      if( code == CURLE_OK )
         x_itemPutND( result, value );

      break;
   }
   case CURLINFO_OFF_T:
   {
      curl_off_t value = 0;

      code = curl_easy_getinfo( curl, info, &value );
      if( code == CURLE_OK )
         x_itemPutNInt( result, ( int64_t )value );

      break;
   }
   case CURLINFO_SOCKET:
   {
      curl_socket_t value = CURL_SOCKET_BAD;

      code = curl_easy_getinfo( curl, info, &value );
      if( code == CURLE_OK )
         x_itemPutNInt( result, ( int64_t )value );

      break;
   }
   case CURLINFO_SLIST:
      if( info == CURLINFO_CERTINFO )
      {
         struct curl_certinfo *value = NULL;

         code = curl_easy_getinfo( curl, info, &value );
         if( code == CURLE_OK )
            s_certinfo( result, value );
      }
      else if( info != ( CURLINFO )( CURLINFO_PTR + 43 ) && info != CURLINFO_TLS_SSL_PTR )
      {
         struct curl_slist *value = NULL;

         code = curl_easy_getinfo( curl, info, &value );
         if( code == CURLE_OK )
         {
            x_curlArrayFromSlist( result, value );
            curl_slist_free_all( value );
         }
      }

      break;
   }

   return code;
}

/* Received data goes to a file; NULL closes it and returns to the memory buffer */
int x_curlEasyDownload( X_CURL_EASY *easy, const char *path )
{
   if( easy->downloadFile != F_ERROR )
      x_fsClose( easy->downloadFile );

   easy->downloadFile = IIF( path != NULL, x_fsCreate( path, FC_NORMAL ), F_ERROR );

   return path == NULL || easy->downloadFile != F_ERROR;
}

/* Data to upload from memory or from a file; its size goes to CURLOPT_INFILESIZE_LARGE */
int x_curlEasyUpload( X_CURL_EASY *easy, const char *data, size_t len, const char *path )
{
   curl_off_t size = -1;

   if( easy->uploadFile != F_ERROR )
      x_fsClose( easy->uploadFile );

   easy->uploadFile = F_ERROR;
   easy->uploadPos = 0;
   x_curlBufFree( &easy->upload );

   if( path != NULL )
   {
      easy->uploadFile = x_fsOpen( path, FO_READ | FO_DENYNONE );
      if( easy->uploadFile == F_ERROR )
         return 0;

      size = ( curl_off_t )x_fsSeek( easy->uploadFile, 0, FS_END );
      x_fsSeek( easy->uploadFile, 0, FS_SET );
   }
   else if( data != NULL )
   {
      x_curlAppend( &easy->upload, data, len );
      size = ( curl_off_t )len;
   }

   curl_easy_setopt( easy->curl, CURLOPT_INFILESIZE_LARGE, size );

   return 1;
}

/* Set or remove the progress block */
void x_curlEasyProgress( X_CURL_EASY *easy, const X_ITEM *block )
{
   x_itemClear( &easy->progress );

   if( block != NULL && X_IS_BLOCK( block ) )
      x_itemCopy( &easy->progress, block );

   curl_easy_setopt( easy->curl, CURLOPT_NOPROGRESS, IIF( X_IS_BLOCK( &easy->progress ), 0L, 1L ) );
}

/* Set or remove the trace block; CURLOPT_VERBOSE follows it */
void x_curlEasyDebug( X_CURL_EASY *easy, const X_ITEM *block )
{
   x_itemClear( &easy->debug );

   if( block != NULL && X_IS_BLOCK( block ) )
      x_itemCopy( &easy->debug, block );

   curl_easy_setopt( easy->curl, CURLOPT_VERBOSE, IIF( X_IS_BLOCK( &easy->debug ), 1L, 0L ) );
}

/* ---------------------------------------------------------------------------------------------
   Multi handle
   --------------------------------------------------------------------------------------------- */

/* A new multi handle for a libcurl handle */
X_CURL_MULTI *x_curlMultiNew( CURLM *handle )
{
   X_CURL_MULTI *multi = ( X_CURL_MULTI * )x_curlObjectNew( X_CURL_KIND_MULTI, sizeof( X_CURL_MULTI ) );

   multi->multi = handle;

   return multi;
}

/* Release a multi handle; its transfers are removed first */
void x_curlMultiFree( X_CURL_MULTI *multi )
{
   while( multi->count > 0 )
      x_curlMultiRemove( multi, multi->easies[multi->count - 1] );

   curl_multi_cleanup( multi->multi );

   if( multi->easies != NULL )
      x_xfree( multi->easies );

   x_curlObjectFree( multi );
}

/* Add a transfer; a handle can be in one multi handle only */
CURLMcode x_curlMultiAdd( X_CURL_MULTI *multi, X_CURL_EASY *easy )
{
   CURLMcode code;

   if( easy->multi != NULL )
      return CURLM_ADDED_ALREADY;

   x_curlEasyPrepare( easy );

   code = curl_multi_add_handle( multi->multi, easy->curl );
   if( code != CURLM_OK )
      return code;

   if( multi->count == multi->cap )
   {
      multi->cap = IIF( multi->cap == 0, 8, multi->cap * 2 );
      multi->easies = ( X_CURL_EASY ** )x_xreallocArray( multi->easies, multi->cap, sizeof( X_CURL_EASY * ) );
   }

   multi->easies[multi->count++] = easy;
   easy->multi = multi;

   return CURLM_OK;
}

/* Remove a transfer */
CURLMcode x_curlMultiRemove( X_CURL_MULTI *multi, X_CURL_EASY *easy )
{
   size_t i;

   if( easy->multi != multi )
      return CURLM_BAD_EASY_HANDLE;

   for( i = 0; i < multi->count && multi->easies[i] != easy; i++ )
      ;

   multi->easies[i] = multi->easies[--multi->count];
   easy->multi = NULL;

   return curl_multi_remove_handle( multi->multi, easy->curl );
}

/* The easy handle of a libcurl handle in the multi handle, or NULL */
X_CURL_EASY *x_curlMultiFind( const X_CURL_MULTI *multi, CURL *curl )
{
   size_t i;

   for( i = 0; i < multi->count; i++ )
   {
      if( multi->easies[i]->curl == curl )
         return multi->easies[i];
   }

   return NULL;
}

/* Set a numeric option */
CURLMcode x_curlMultiSetopt( X_CURL_MULTI *multi, CURLMoption option, const X_ITEM *value )
{
   if( option < CURLOPTTYPE_OBJECTPOINT )
   {
      long number = ( long )x_itemGetNInt( value );

      if( X_IS_LOGICAL( value ) )
         number = ( long )x_itemGetL( value );

      return curl_multi_setopt( multi->multi, option, number );
   }

   if( option >= CURLOPTTYPE_OFF_T && option < CURLOPTTYPE_BLOB )
      return curl_multi_setopt( multi->multi, option, ( curl_off_t )x_itemGetNInt( value ) );

   return CURLM_UNKNOWN_OPTION;
}

/* ---------------------------------------------------------------------------------------------
   MIME
   --------------------------------------------------------------------------------------------- */

/* A new form for a libcurl form */
X_CURL_MIME *x_curlMimeNew( curl_mime *handle )
{
   X_CURL_MIME *mime = ( X_CURL_MIME * )x_curlObjectNew( X_CURL_KIND_MIME, sizeof( X_CURL_MIME ) );

   mime->mime = handle;

   return mime;
}

/* Release a form and its parts; an attached form belongs to its part, only the handle goes */
void x_curlMimeFree( X_CURL_MIME *mime )
{
   size_t i;

   for( i = 0; i < mime->count; i++ )
      x_curlObjectFree( mime->parts[i] );

   if( mime->parts != NULL )
      x_xfree( mime->parts );

   if( !mime->attached )
      curl_mime_free( mime->mime );

   x_curlObjectFree( mime );
}

/* Add a part to a form, or NULL */
X_CURL_PART *x_curlMimeAddPart( X_CURL_MIME *mime )
{
   curl_mimepart *handle = curl_mime_addpart( mime->mime );
   X_CURL_PART *part;

   if( handle == NULL )
      return NULL;

   if( mime->count == mime->cap )
   {
      mime->cap = IIF( mime->cap == 0, 4, mime->cap * 2 );
      mime->parts = ( X_CURL_PART ** )x_xreallocArray( mime->parts, mime->cap, sizeof( X_CURL_PART * ) );
   }

   part = ( X_CURL_PART * )x_curlObjectNew( X_CURL_KIND_PART, sizeof( X_CURL_PART ) );
   part->part = handle;

   mime->parts[mime->count++] = part;

   return part;
}

/* The libcurl part in argument n, or NULL */
curl_mimepart *x_curlPart( X_CALL_CTX *ctx, int n )
{
   X_CURL_PART *part = ( X_CURL_PART * )x_curlParam( ctx, n, X_CURL_KIND_PART );

   return IIF( part != NULL, part->part, NULL );
}
