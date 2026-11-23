/*
 * Copyright (c) 2024-2026 xDev-Language
 *
 * Library...: xDev CURL
 * File......: curl-api.c
 *
 */

#include "internal-curl.h"

/* ---------------------------------------------------------------------------------------------
   Library
   --------------------------------------------------------------------------------------------- */

// Initialize libcurl; the other functions do it on first use with CURL_GLOBAL_DEFAULT
// curl_global_init( [nFlags] ) --> nCode
X_FUNC( CURL_GLOBAL_INIT )
{
   long flags = IIF( X_ISNUM( ctx, 1 ), x_parnl( ctx, 1 ), CURL_GLOBAL_DEFAULT );

   x_retni( ctx, x_curlGlobalInit( flags ) );
}

// Release libcurl after all handles are cleaned up
// curl_global_cleanup( ) --> NIL
X_FUNC( CURL_GLOBAL_CLEANUP )
{
   x_curlGlobalCleanup();
}

// Version text of libcurl and its libraries
// curl_version( ) --> cVersion
X_FUNC( CURL_VERSION )
{
   x_retc( ctx, curl_version() );
}

// Version details: { cVersion, nVersionNum, cHost, nFeatures, cSslVersion, cLibzVersion, aProtocols,
// cLibsshVersion, cNghttp2Version, cZstdVersion }
// curl_version_info( ) --> aInfo
X_FUNC( CURL_VERSION_INFO )
{
   const curl_version_info_data *info = curl_version_info( CURLVERSION_NOW );
   X_ITEM *array = x_itemArrayNew( 10 );
   X_ITEM protocols __attribute__( ( cleanup( x_itemClear ) ) ) = X_ITEM_NIL_INIT;
   size_t count = 0;

   while( info->protocols[count] != NULL )
      count++;

   x_arrayNew( &protocols, ( int64_t )count );
   for( count = 0; info->protocols[count] != NULL; count++ )
      x_arraySetC( &protocols, count + 1, info->protocols[count] );

   x_arraySetC( array, 1, info->version );
   x_arraySet( array, 2, x_itemPutNInt( &( X_ITEM )X_ITEM_NIL_INIT, info->version_num ) );
   x_arraySetC( array, 3, info->host );
   x_arraySet( array, 4, x_itemPutNInt( &( X_ITEM )X_ITEM_NIL_INIT, info->features ) );
   x_arraySetC( array, 5, IIF( info->ssl_version != NULL, info->ssl_version, "" ) );
   x_arraySetC( array, 6, IIF( info->libz_version != NULL, info->libz_version, "" ) );
   x_arraySet( array, 7, &protocols );
   x_arraySetC( array, 8, IIF( info->libssh_version != NULL, info->libssh_version, "" ) );
   x_arraySetC( array, 9, IIF( info->nghttp2_version != NULL, info->nghttp2_version, "" ) );
   x_arraySetC( array, 10, IIF( info->zstd_version != NULL, info->zstd_version, "" ) );

   x_itemReturn( ctx, array );
   x_itemRelease( array );
}

// Parse a date in one of the formats of HTTP, mail and news; -1 when it is not a date
// curl_getdate( cDate ) --> nUnixTime
X_FUNC( CURL_GETDATE )
{
   x_retnll( ctx, IIF( X_ISCHAR( ctx, 1 ), ( long long )curl_getdate( x_parc( ctx, 1 ), NULL ), -1 ) );
}

// Text of an error code of the easy interface
// curl_easy_strerror( nCode ) --> cMessage
X_FUNC( CURL_EASY_STRERROR )
{
   x_retc( ctx, curl_easy_strerror( ( CURLcode )x_parni( ctx, 1 ) ) );
}

// Text of an error code of the multi interface
// curl_multi_strerror( nCode ) --> cMessage
X_FUNC( CURL_MULTI_STRERROR )
{
   x_retc( ctx, curl_multi_strerror( ( CURLMcode )x_parni( ctx, 1 ) ) );
}

// Text of an error code of the share interface
// curl_share_strerror( nCode ) --> cMessage
X_FUNC( CURL_SHARE_STRERROR )
{
   x_retc( ctx, curl_share_strerror( ( CURLSHcode )x_parni( ctx, 1 ) ) );
}

// Text of an error code of the URL interface
// curl_url_strerror( nCode ) --> cMessage
X_FUNC( CURL_URL_STRERROR )
{
   x_retc( ctx, curl_url_strerror( ( CURLUcode )x_parni( ctx, 1 ) ) );
}

/* ---------------------------------------------------------------------------------------------
   Easy interface
   --------------------------------------------------------------------------------------------- */

// Create a transfer handle; received data goes to a memory buffer (curl_easy_buffer()) unless
// curl_easy_download() names a file
// curl_easy_init( ) --> pCurl | NIL
X_FUNC( CURL_EASY_INIT )
{
   CURL *curl = IIF( x_curlGlobalInit( CURL_GLOBAL_DEFAULT ) == CURLE_OK, curl_easy_init(), NULL );

   if( curl != NULL )
      x_retptr( ctx, x_curlEasyNew( curl ) );
}

// Release the handle
// curl_easy_cleanup( pCurl ) --> NIL
X_FUNC( CURL_EASY_CLEANUP )
{
   X_CURL_EASY *easy = ( X_CURL_EASY * )x_curlParam( ctx, 1, X_CURL_KIND_EASY );

   if( easy != NULL )
      x_curlEasyFree( easy );
}

// Copy a handle with its options, blocks and upload data; files are not copied
// curl_easy_duphandle( pCurl ) --> pCurl | NIL
X_FUNC( CURL_EASY_DUPHANDLE )
{
   X_CURL_EASY *easy = ( X_CURL_EASY * )x_curlParam( ctx, 1, X_CURL_KIND_EASY );

   if( easy != NULL )
      x_curlRetObject( ctx, x_curlEasyDup( easy ) );
}

// Restore all options to their defaults; files, blocks and buffers are released
// curl_easy_reset( pCurl ) --> NIL
X_FUNC( CURL_EASY_RESET )
{
   X_CURL_EASY *easy = ( X_CURL_EASY * )x_curlParam( ctx, 1, X_CURL_KIND_EASY );

   if( easy != NULL )
      x_curlEasyReset( easy );
}

// Set an option: CURLOPT_* from curl.ch; a number or logical for numeric options, a string for
// texts and binary data, an array of strings for lists, a handle for CURLOPT_SHARE, CURLOPT_MIMEPOST,
// CURLOPT_CURLU and CURLOPT_STREAM_DEPENDS; NIL restores the default
// curl_easy_setopt( pCurl, nOption, xValue ) --> nCode
X_FUNC( CURL_EASY_SETOPT )
{
   X_CURL_EASY *easy = ( X_CURL_EASY * )x_curlParam( ctx, 1, X_CURL_KIND_EASY );
   CURLcode code = CURLE_BAD_FUNCTION_ARGUMENT;

   if( easy != NULL && X_ISNUM( ctx, 2 ) )
      code = x_curlEasySetopt( easy, ( CURLoption )x_parni( ctx, 2 ), x_curlValue( ctx, 3 ) );

   x_retni( ctx, ( int )code );
}

// Read information about the last transfer: CURLINFO_* from curl.ch; lists come as arrays,
// CURLINFO_CERTINFO as an array of arrays; NIL when it is not available
// curl_easy_getinfo( pCurl, nInfo, [@nCode] ) --> xValue
X_FUNC( CURL_EASY_GETINFO )
{
   X_CURL_EASY *easy = ( X_CURL_EASY * )x_curlParam( ctx, 1, X_CURL_KIND_EASY );
   X_ITEM result __attribute__( ( cleanup( x_itemClear ) ) ) = X_ITEM_NIL_INIT;
   CURLcode code = CURLE_BAD_FUNCTION_ARGUMENT;

   if( easy != NULL && X_ISNUM( ctx, 2 ) )
      code = x_curlEasyGetinfo( easy, ( CURLINFO )x_parni( ctx, 2 ), &result );

   x_storni( ctx, ( int )code, 3 );
   x_itemReturn( ctx, &result );
}

// Run the transfer and wait until it ends
// curl_easy_perform( pCurl ) --> nCode
X_FUNC( CURL_EASY_PERFORM )
{
   X_CURL_EASY *easy = ( X_CURL_EASY * )x_curlParam( ctx, 1, X_CURL_KIND_EASY );
   CURLcode code;

   if( easy == NULL )
   {
      x_retni( ctx, CURLE_BAD_FUNCTION_ARGUMENT );
      return;
   }

   x_curlEasyPrepare( easy );
   x_curlBegin();
   code = curl_easy_perform( easy->curl );
   x_curlEnd();

   x_retni( ctx, ( int )code );
}

// Pause or resume a transfer: CURLPAUSE_* from curl.ch
// curl_easy_pause( pCurl, nMask ) --> nCode
X_FUNC( CURL_EASY_PAUSE )
{
   X_CURL_EASY *easy = ( X_CURL_EASY * )x_curlParam( ctx, 1, X_CURL_KIND_EASY );
   CURLcode code = CURLE_BAD_FUNCTION_ARGUMENT;

   if( easy != NULL )
   {
      x_curlBegin();
      code = curl_easy_pause( easy->curl, x_parni( ctx, 2 ) );
      x_curlEnd();
   }

   x_retni( ctx, ( int )code );
}

// Keep the connection of the handle alive
// curl_easy_upkeep( pCurl ) --> nCode
X_FUNC( CURL_EASY_UPKEEP )
{
   X_CURL_EASY *easy = ( X_CURL_EASY * )x_curlParam( ctx, 1, X_CURL_KIND_EASY );
   CURLcode code = CURLE_BAD_FUNCTION_ARGUMENT;

   if( easy != NULL )
   {
      x_curlBegin();
      code = curl_easy_upkeep( easy->curl );
      x_curlEnd();
   }

   x_retni( ctx, ( int )code );
}

// Send raw data on a connection opened with CURLOPT_CONNECT_ONLY
// curl_easy_send( pCurl, cData, [@nSent] ) --> nCode
X_FUNC( CURL_EASY_SEND )
{
   X_CURL_EASY *easy = ( X_CURL_EASY * )x_curlParam( ctx, 1, X_CURL_KIND_EASY );
   CURLcode code = CURLE_BAD_FUNCTION_ARGUMENT;
   size_t sent = 0;

   if( easy != NULL && X_ISCHAR( ctx, 2 ) )
      code = curl_easy_send( easy->curl, x_parc( ctx, 2 ), x_parclen( ctx, 2 ), &sent );

   x_stornll( ctx, ( long long )sent, 3 );
   x_retni( ctx, ( int )code );
}

// Receive raw data on a connection opened with CURLOPT_CONNECT_ONLY (64 KB at most by default)
// curl_easy_recv( pCurl, @cData, [nMax] ) --> nCode
X_FUNC( CURL_EASY_RECV )
{
   X_CURL_EASY *easy = ( X_CURL_EASY * )x_curlParam( ctx, 1, X_CURL_KIND_EASY );
   size_t max = IIF( X_ISNUM( ctx, 3 ) && x_parnl( ctx, 3 ) > 0, ( size_t )x_parnl( ctx, 3 ), 65536 );
   size_t received = 0;
   char *data;
   CURLcode code;

   if( easy == NULL )
   {
      x_retni( ctx, CURLE_BAD_FUNCTION_ARGUMENT );
      return;
   }

   data = ( char * )x_xgrab( max + 1 );
   code = curl_easy_recv( easy->curl, data, max, &received );
   data[received] = '\0';

   x_storclen_buffer( ctx, data, received, 2 );
   x_retni( ctx, ( int )code );
}

// Encode text for use in a URL
// curl_easy_escape( pCurl, cText ) --> cEscaped
X_FUNC( CURL_EASY_ESCAPE )
{
   X_CURL_EASY *easy = ( X_CURL_EASY * )x_curlParam( ctx, 1, X_CURL_KIND_EASY );
   char *value = NULL;

   if( easy != NULL && X_ISCHAR( ctx, 2 ) )
      value = curl_easy_escape( easy->curl, x_parc( ctx, 2 ), ( int )x_parclen( ctx, 2 ) );

   x_retc( ctx, IIF( value != NULL, value, "" ) );
   curl_free( value );
}

// Decode text encoded for a URL
// curl_easy_unescape( pCurl, cText ) --> cText
X_FUNC( CURL_EASY_UNESCAPE )
{
   X_CURL_EASY *easy = ( X_CURL_EASY * )x_curlParam( ctx, 1, X_CURL_KIND_EASY );
   char *value = NULL;
   int len = 0;

   if( easy != NULL && X_ISCHAR( ctx, 2 ) )
      value = curl_easy_unescape( easy->curl, x_parc( ctx, 2 ), ( int )x_parclen( ctx, 2 ), &len );

   x_retclen( ctx, IIF( value != NULL, value, "" ), ( size_t )len );
   curl_free( value );
}

// Header of the last response; nIndex counts headers with the same name from 1, nOrigin is
// CURLH_* (default CURLH_HEADER), nRequest -1 is the last request of a redirect chain
// curl_easy_header( pCurl, cName, [nIndex], [nOrigin], [nRequest] ) --> cValue | NIL
X_FUNC( CURL_EASY_HEADER )
{
   X_CURL_EASY *easy = ( X_CURL_EASY * )x_curlParam( ctx, 1, X_CURL_KIND_EASY );
   struct curl_header *header = NULL;
   size_t index = IIF( X_ISNUM( ctx, 3 ) && x_parnl( ctx, 3 ) > 1, ( size_t )x_parnl( ctx, 3 ) - 1, 0 );
   unsigned int origin = IIF( X_ISNUM( ctx, 4 ), ( unsigned int )x_parni( ctx, 4 ), CURLH_HEADER );
   int request = IIF( X_ISNUM( ctx, 5 ), x_parni( ctx, 5 ), -1 );

   if( easy == NULL || !X_ISCHAR( ctx, 2 ) )
      return;

   if( curl_easy_header( easy->curl, x_parc( ctx, 2 ), index, origin, request, &header ) == CURLHE_OK )
      x_retc( ctx, header->value );
}

// All headers of the last response in their order
// curl_easy_headers( pCurl, [nOrigin], [nRequest] ) --> { { cName, cValue }, ... }
X_FUNC( CURL_EASY_HEADERS )
{
   X_CURL_EASY *easy = ( X_CURL_EASY * )x_curlParam( ctx, 1, X_CURL_KIND_EASY );
   X_ITEM *array = x_itemArrayNew( 0 );
   struct curl_header *header = NULL;
   unsigned int origin = IIF( X_ISNUM( ctx, 2 ), ( unsigned int )x_parni( ctx, 2 ), CURLH_HEADER );
   int request = IIF( X_ISNUM( ctx, 3 ), x_parni( ctx, 3 ), -1 );

   while( easy != NULL && ( header = curl_easy_nextheader( easy->curl, origin, request, header ) ) != NULL )
   {
      X_ITEM *pair = x_itemArrayNew( 2 );

      x_arraySetC( pair, 1, header->name );
      x_arraySetC( pair, 2, header->value );
      x_arrayAdd( array, pair );
      x_itemRelease( pair );
   }

   x_itemReturn( ctx, array );
   x_itemRelease( array );
}

// Write received data to a file instead of the memory buffer; NIL closes the file and returns to
// the buffer
// curl_easy_download( pCurl, [cFile] ) --> lSuccess
X_FUNC( CURL_EASY_DOWNLOAD )
{
   X_CURL_EASY *easy = ( X_CURL_EASY * )x_curlParam( ctx, 1, X_CURL_KIND_EASY );

   x_retl( ctx, easy != NULL && x_curlEasyDownload( easy, x_curlString( ctx, 2 ) ) );
}

// Data received into the memory buffer by the last transfer
// curl_easy_buffer( pCurl ) --> cData
X_FUNC( CURL_EASY_BUFFER )
{
   X_CURL_EASY *easy = ( X_CURL_EASY * )x_curlParam( ctx, 1, X_CURL_KIND_EASY );

   if( easy != NULL && easy->buffer.len > 0 )
      x_retclen( ctx, easy->buffer.data, easy->buffer.len );
   else
      x_retc( ctx, "" );
}

// Data sent by an upload (CURLOPT_UPLOAD, or CURLOPT_POST without CURLOPT_POSTFIELDS); its size is
// set as CURLOPT_INFILESIZE_LARGE; NIL removes it
// curl_easy_upload( pCurl, [cData] ) --> lSuccess
X_FUNC( CURL_EASY_UPLOAD )
{
   X_CURL_EASY *easy = ( X_CURL_EASY * )x_curlParam( ctx, 1, X_CURL_KIND_EASY );

   x_retl( ctx, easy != NULL && x_curlEasyUpload( easy, x_curlString( ctx, 2 ), x_parclen( ctx, 2 ), NULL ) );
}

// Like curl_easy_upload(), reading the data from a file
// curl_easy_upload_file( pCurl, [cFile] ) --> lSuccess
X_FUNC( CURL_EASY_UPLOAD_FILE )
{
   X_CURL_EASY *easy = ( X_CURL_EASY * )x_curlParam( ctx, 1, X_CURL_KIND_EASY );

   x_retl( ctx, easy != NULL && x_curlEasyUpload( easy, NULL, 0, x_curlString( ctx, 2 ) ) );
}

// Block called during the transfer: {| nDlTotal, nDlNow, nUlTotal, nUlNow | lAbort }; NIL removes it
// curl_easy_progress( pCurl, [bBlock] ) --> NIL
X_FUNC( CURL_EASY_PROGRESS )
{
   X_CURL_EASY *easy = ( X_CURL_EASY * )x_curlParam( ctx, 1, X_CURL_KIND_EASY );

   if( easy != NULL )
      x_curlEasyProgress( easy, x_paritem( ctx, 2 ) );
}

// Block receiving the verbose trace: {| nType, cData | }, nType is CURLINFO_TEXT, CURLINFO_HEADER_IN
// and the like; turns CURLOPT_VERBOSE on, NIL removes the block and turns it off
// curl_easy_debug( pCurl, [bBlock] ) --> NIL
X_FUNC( CURL_EASY_DEBUG )
{
   X_CURL_EASY *easy = ( X_CURL_EASY * )x_curlParam( ctx, 1, X_CURL_KIND_EASY );

   if( easy != NULL )
      x_curlEasyDebug( easy, x_paritem( ctx, 2 ) );
}

/* ---------------------------------------------------------------------------------------------
   Multi interface
   --------------------------------------------------------------------------------------------- */

// Create a handle that runs many transfers at the same time
// curl_multi_init( ) --> pMulti | NIL
X_FUNC( CURL_MULTI_INIT )
{
   CURLM *multi = IIF( x_curlGlobalInit( CURL_GLOBAL_DEFAULT ) == CURLE_OK, curl_multi_init(), NULL );

   if( multi != NULL )
      x_retptr( ctx, x_curlMultiNew( multi ) );
}

// Release the handle; its transfers are removed first
// curl_multi_cleanup( pMulti ) --> NIL
X_FUNC( CURL_MULTI_CLEANUP )
{
   X_CURL_MULTI *multi = ( X_CURL_MULTI * )x_curlParam( ctx, 1, X_CURL_KIND_MULTI );

   if( multi != NULL )
   {
      x_curlBegin();
      x_curlMultiFree( multi );
      x_curlEnd();
   }
}

// Add a transfer; its memory buffer is emptied
// curl_multi_add_handle( pMulti, pCurl ) --> nCode
X_FUNC( CURL_MULTI_ADD_HANDLE )
{
   X_CURL_MULTI *multi = ( X_CURL_MULTI * )x_curlParam( ctx, 1, X_CURL_KIND_MULTI );
   X_CURL_EASY *easy = ( X_CURL_EASY * )x_curlParam( ctx, 2, X_CURL_KIND_EASY );
   CURLMcode code;

   if( multi == NULL )
      code = CURLM_BAD_HANDLE;
   else if( easy == NULL )
      code = CURLM_BAD_EASY_HANDLE;
   else
      code = x_curlMultiAdd( multi, easy );

   x_retni( ctx, ( int )code );
}

// Remove a transfer
// curl_multi_remove_handle( pMulti, pCurl ) --> nCode
X_FUNC( CURL_MULTI_REMOVE_HANDLE )
{
   X_CURL_MULTI *multi = ( X_CURL_MULTI * )x_curlParam( ctx, 1, X_CURL_KIND_MULTI );
   X_CURL_EASY *easy = ( X_CURL_EASY * )x_curlParam( ctx, 2, X_CURL_KIND_EASY );
   CURLMcode code;

   if( multi == NULL )
      code = CURLM_BAD_HANDLE;
   else if( easy == NULL )
      code = CURLM_BAD_EASY_HANDLE;
   else
   {
      x_curlBegin();
      code = x_curlMultiRemove( multi, easy );
      x_curlEnd();
   }

   x_retni( ctx, ( int )code );
}

// Move all transfers forward without waiting
// curl_multi_perform( pMulti, [@nRunning] ) --> nCode
X_FUNC( CURL_MULTI_PERFORM )
{
   X_CURL_MULTI *multi = ( X_CURL_MULTI * )x_curlParam( ctx, 1, X_CURL_KIND_MULTI );
   int running = 0;
   CURLMcode code = CURLM_BAD_HANDLE;

   if( multi != NULL )
   {
      x_curlBegin();
      code = curl_multi_perform( multi->multi, &running );
      x_curlEnd();
   }

   x_storni( ctx, running, 2 );
   x_retni( ctx, ( int )code );
}

// Wait for activity on the transfers, at most nTimeout milliseconds (default 1000)
// curl_multi_poll( pMulti, [nTimeout], [@nCount] ) --> nCode
X_FUNC( CURL_MULTI_POLL )
{
   X_CURL_MULTI *multi = ( X_CURL_MULTI * )x_curlParam( ctx, 1, X_CURL_KIND_MULTI );
   int timeout = IIF( X_ISNUM( ctx, 2 ), x_parni( ctx, 2 ), 1000 );
   int count = 0;
   CURLMcode code = CURLM_BAD_HANDLE;

   if( multi != NULL )
      code = curl_multi_poll( multi->multi, NULL, 0, timeout, &count );

   x_storni( ctx, count, 3 );
   x_retni( ctx, ( int )code );
}

// Wake up a curl_multi_poll() that is waiting
// curl_multi_wakeup( pMulti ) --> nCode
X_FUNC( CURL_MULTI_WAKEUP )
{
   X_CURL_MULTI *multi = ( X_CURL_MULTI * )x_curlParam( ctx, 1, X_CURL_KIND_MULTI );

   x_retni( ctx, IIF( multi != NULL, ( int )curl_multi_wakeup( multi->multi ), CURLM_BAD_HANDLE ) );
}

// Next message about a transfer: { nMsg, pCurl, nResult }, nMsg is CURLMSG_DONE; NIL when there is
// none
// curl_multi_info_read( pMulti, [@nQueued] ) --> aMessage | NIL
X_FUNC( CURL_MULTI_INFO_READ )
{
   X_CURL_MULTI *multi = ( X_CURL_MULTI * )x_curlParam( ctx, 1, X_CURL_KIND_MULTI );
   int queued = 0;
   CURLMsg *message = IIF( multi != NULL, curl_multi_info_read( multi->multi, &queued ), NULL );
   X_CURL_EASY *easy;
   X_ITEM *array;

   x_storni( ctx, queued, 2 );

   if( message == NULL )
      return;

   easy = x_curlMultiFind( multi, message->easy_handle );
   array = x_itemArrayNew( 3 );

   x_arraySet( array, 1, x_itemPutNI( &( X_ITEM )X_ITEM_NIL_INIT, ( int )message->msg ) );
   if( easy != NULL )
      x_arraySetPtr( array, 2, easy );

   x_arraySet( array, 3, x_itemPutNI( &( X_ITEM )X_ITEM_NIL_INIT, ( int )message->data.result ) );

   x_itemReturn( ctx, array );
   x_itemRelease( array );
}

// Set an option: CURLMOPT_* from curl.ch with a number or logical value
// curl_multi_setopt( pMulti, nOption, xValue ) --> nCode
X_FUNC( CURL_MULTI_SETOPT )
{
   X_CURL_MULTI *multi = ( X_CURL_MULTI * )x_curlParam( ctx, 1, X_CURL_KIND_MULTI );
   CURLMcode code = CURLM_BAD_HANDLE;

   if( multi != NULL && X_ISNUM( ctx, 2 ) )
      code = x_curlMultiSetopt( multi, ( CURLMoption )x_parni( ctx, 2 ), x_curlValue( ctx, 3 ) );

   x_retni( ctx, ( int )code );
}

/* ---------------------------------------------------------------------------------------------
   Share interface
   --------------------------------------------------------------------------------------------- */

// Create a handle that lets transfers share cookies, DNS cache, connections and TLS sessions
// curl_share_init( ) --> pShare | NIL
X_FUNC( CURL_SHARE_INIT )
{
   CURLSH *handle = IIF( x_curlGlobalInit( CURL_GLOBAL_DEFAULT ) == CURLE_OK, curl_share_init(), NULL );
   X_CURL_SHARE *share;

   if( handle == NULL )
      return;

   share = ( X_CURL_SHARE * )x_curlObjectNew( X_CURL_KIND_SHARE, sizeof( X_CURL_SHARE ) );
   share->share = handle;

   x_retptr( ctx, share );
}

// Release the handle; it fails with CURLSHE_IN_USE while a transfer still uses it
// curl_share_cleanup( pShare ) --> nCode
X_FUNC( CURL_SHARE_CLEANUP )
{
   X_CURL_SHARE *share = ( X_CURL_SHARE * )x_curlParam( ctx, 1, X_CURL_KIND_SHARE );
   CURLSHcode code = IIF( share != NULL, curl_share_cleanup( share->share ), CURLSHE_INVALID );

   if( code == CURLSHE_OK )
      x_curlObjectFree( share );

   x_retni( ctx, ( int )code );
}

// Share or stop sharing data: CURLSHOPT_SHARE or CURLSHOPT_UNSHARE with CURL_LOCK_DATA_*
// curl_share_setopt( pShare, nOption, nData ) --> nCode
X_FUNC( CURL_SHARE_SETOPT )
{
   X_CURL_SHARE *share = ( X_CURL_SHARE * )x_curlParam( ctx, 1, X_CURL_KIND_SHARE );
   int option = x_parni( ctx, 2 );
   curl_lock_data data = ( curl_lock_data )x_parni( ctx, 3 );
   CURLSHcode code;

   if( share == NULL )
      code = CURLSHE_INVALID;
   else if( option != CURLSHOPT_SHARE && option != CURLSHOPT_UNSHARE )
      code = CURLSHE_BAD_OPTION;
   else
      code = curl_share_setopt( share->share, ( CURLSHoption )option, data );

   x_retni( ctx, ( int )code );
}

/* ---------------------------------------------------------------------------------------------
   URL interface
   --------------------------------------------------------------------------------------------- */

// Create an empty URL handle
// curl_url( ) --> pUrl | NIL
X_FUNC( CURL_URL )
{
   CURLU *handle = curl_url();
   X_CURL_URL *url;

   if( handle == NULL )
      return;

   url = ( X_CURL_URL * )x_curlObjectNew( X_CURL_KIND_URL, sizeof( X_CURL_URL ) );
   url->url = handle;

   x_retptr( ctx, url );
}

// Release the handle
// curl_url_cleanup( pUrl ) --> NIL
X_FUNC( CURL_URL_CLEANUP )
{
   X_CURL_URL *url = ( X_CURL_URL * )x_curlParam( ctx, 1, X_CURL_KIND_URL );

   if( url != NULL )
   {
      curl_url_cleanup( url->url );
      x_curlObjectFree( url );
   }
}

// Copy the handle
// curl_url_dup( pUrl ) --> pUrl | NIL
X_FUNC( CURL_URL_DUP )
{
   X_CURL_URL *url = ( X_CURL_URL * )x_curlParam( ctx, 1, X_CURL_KIND_URL );
   CURLU *handle = IIF( url != NULL, curl_url_dup( url->url ), NULL );
   X_CURL_URL *copy;

   if( handle == NULL )
      return;

   copy = ( X_CURL_URL * )x_curlObjectNew( X_CURL_KIND_URL, sizeof( X_CURL_URL ) );
   copy->url = handle;

   x_retptr( ctx, copy );
}

// Set a part: CURLUPART_* and CURLU_* flags from curl.ch; NIL clears the part
// curl_url_set( pUrl, nPart, [cContent], [nFlags] ) --> nCode
X_FUNC( CURL_URL_SET )
{
   X_CURL_URL *url = ( X_CURL_URL * )x_curlParam( ctx, 1, X_CURL_KIND_URL );
   CURLUPart part = ( CURLUPart )x_parni( ctx, 2 );
   unsigned int flags = ( unsigned int )x_parni( ctx, 4 );
   CURLUcode code = CURLUE_BAD_HANDLE;

   if( url != NULL )
      code = curl_url_set( url->url, part, x_curlString( ctx, 3 ), flags );

   x_retni( ctx, ( int )code );
}

// Get a part: CURLUPART_* and CURLU_* flags from curl.ch; NIL when the part is missing
// curl_url_get( pUrl, nPart, [nFlags], [@nCode] ) --> cContent | NIL
X_FUNC( CURL_URL_GET )
{
   X_CURL_URL *url = ( X_CURL_URL * )x_curlParam( ctx, 1, X_CURL_KIND_URL );
   CURLUPart part = ( CURLUPart )x_parni( ctx, 2 );
   unsigned int flags = ( unsigned int )x_parni( ctx, 3 );
   CURLUcode code = CURLUE_BAD_HANDLE;
   char *value = NULL;

   if( url != NULL )
      code = curl_url_get( url->url, part, &value, flags );

   x_storni( ctx, ( int )code, 4 );

   if( value != NULL )
   {
      x_retc( ctx, value );
      curl_free( value );
   }
}

/* ---------------------------------------------------------------------------------------------
   MIME interface
   --------------------------------------------------------------------------------------------- */

// Create a multipart form for CURLOPT_MIMEPOST; release it after the transfer
// curl_mime_init( pCurl ) --> pMime | NIL
X_FUNC( CURL_MIME_INIT )
{
   X_CURL_EASY *easy = ( X_CURL_EASY * )x_curlParam( ctx, 1, X_CURL_KIND_EASY );
   curl_mime *handle = IIF( easy != NULL, curl_mime_init( easy->curl ), NULL );

   if( handle != NULL )
      x_retptr( ctx, x_curlMimeNew( handle ) );
}

// Release the form and its parts
// curl_mime_free( pMime ) --> NIL
X_FUNC( CURL_MIME_FREE )
{
   X_CURL_MIME *mime = ( X_CURL_MIME * )x_curlParam( ctx, 1, X_CURL_KIND_MIME );

   if( mime != NULL )
      x_curlMimeFree( mime );
}

// Add a part to the form
// curl_mime_addpart( pMime ) --> pPart | NIL
X_FUNC( CURL_MIME_ADDPART )
{
   X_CURL_MIME *mime = ( X_CURL_MIME * )x_curlParam( ctx, 1, X_CURL_KIND_MIME );

   if( mime != NULL )
      x_curlRetObject( ctx, x_curlMimeAddPart( mime ) );
}

// Name of the part (the form field)
// curl_mime_name( pPart, [cName] ) --> nCode
X_FUNC( CURL_MIME_NAME )
{
   curl_mimepart *part = x_curlPart( ctx, 1 );
   CURLcode code = CURLE_BAD_FUNCTION_ARGUMENT;

   if( part != NULL )
      code = curl_mime_name( part, x_curlString( ctx, 2 ) );

   x_retni( ctx, ( int )code );
}

// Content of the part
// curl_mime_data( pPart, [cData] ) --> nCode
X_FUNC( CURL_MIME_DATA )
{
   curl_mimepart *part = x_curlPart( ctx, 1 );
   CURLcode code = CURLE_BAD_FUNCTION_ARGUMENT;

   if( part != NULL )
      code = curl_mime_data( part, x_curlString( ctx, 2 ), x_parclen( ctx, 2 ) );

   x_retni( ctx, ( int )code );
}

// Content of the part read from a file; the file name is also sent
// curl_mime_filedata( pPart, [cFile] ) --> nCode
X_FUNC( CURL_MIME_FILEDATA )
{
   curl_mimepart *part = x_curlPart( ctx, 1 );
   CURLcode code = CURLE_BAD_FUNCTION_ARGUMENT;

   if( part != NULL )
      code = curl_mime_filedata( part, x_curlString( ctx, 2 ) );

   x_retni( ctx, ( int )code );
}

// File name sent with the part
// curl_mime_filename( pPart, [cName] ) --> nCode
X_FUNC( CURL_MIME_FILENAME )
{
   curl_mimepart *part = x_curlPart( ctx, 1 );
   CURLcode code = CURLE_BAD_FUNCTION_ARGUMENT;

   if( part != NULL )
      code = curl_mime_filename( part, x_curlString( ctx, 2 ) );

   x_retni( ctx, ( int )code );
}

// Content type of the part
// curl_mime_type( pPart, [cType] ) --> nCode
X_FUNC( CURL_MIME_TYPE )
{
   curl_mimepart *part = x_curlPart( ctx, 1 );
   CURLcode code = CURLE_BAD_FUNCTION_ARGUMENT;

   if( part != NULL )
      code = curl_mime_type( part, x_curlString( ctx, 2 ) );

   x_retni( ctx, ( int )code );
}

// Transfer encoding of the part: "binary", "8bit", "7bit", "base64" or "quoted-printable"
// curl_mime_encoder( pPart, [cEncoding] ) --> nCode
X_FUNC( CURL_MIME_ENCODER )
{
   curl_mimepart *part = x_curlPart( ctx, 1 );
   CURLcode code = CURLE_BAD_FUNCTION_ARGUMENT;

   if( part != NULL )
      code = curl_mime_encoder( part, x_curlString( ctx, 2 ) );

   x_retni( ctx, ( int )code );
}

// Extra headers of the part
// curl_mime_headers( pPart, aHeaders ) --> nCode
X_FUNC( CURL_MIME_HEADERS )
{
   curl_mimepart *part = x_curlPart( ctx, 1 );
   CURLcode code = CURLE_BAD_FUNCTION_ARGUMENT;

   if( part != NULL )
      code = curl_mime_headers( part, x_curlSlist( x_paritem( ctx, 2 ) ), 1 );

   x_retni( ctx, ( int )code );
}

// Make a form the content of the part; the form then belongs to the part, curl_mime_free() of the
// inner form releases only its handle
// curl_mime_subparts( pPart, pMime ) --> nCode
X_FUNC( CURL_MIME_SUBPARTS )
{
   curl_mimepart *part = x_curlPart( ctx, 1 );
   X_CURL_MIME *mime = ( X_CURL_MIME * )x_curlParam( ctx, 2, X_CURL_KIND_MIME );
   CURLcode code = CURLE_BAD_FUNCTION_ARGUMENT;

   if( part != NULL && mime != NULL && !mime->attached )
   {
      code = curl_mime_subparts( part, mime->mime );
      mime->attached = code == CURLE_OK;
   }

   x_retni( ctx, ( int )code );
}
