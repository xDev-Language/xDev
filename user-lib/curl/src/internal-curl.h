/*
 * Copyright (c) 2024-2026 xDev-Language
 *
 * Library...: xDev CURL
 * File......: internal-curl.h
 *
 */

#ifndef INTERNAL_CURL_H
#define INTERNAL_CURL_H

#include "platform.h"
#include "internal.h"

#include <curl/curl.h>

/* ---------------------------------------------------------------------------------------------
   Handles
   --------------------------------------------------------------------------------------------- */

/* Marks a valid handle; a released handle has 0 */
#define X_CURL_MAGIC 0x4355524Cu

typedef enum
{
   X_CURL_KIND_EASY = 1,
   X_CURL_KIND_MULTI,
   X_CURL_KIND_SHARE,
   X_CURL_KIND_URL,
   X_CURL_KIND_MIME,
   X_CURL_KIND_PART
} X_CURL_KIND;

/* The start of every handle */
typedef struct
{
   uint32_t magic;
   int kind;
} X_CURL_HEAD;

/* A growing memory buffer, always ended with '\0' */
typedef struct
{
   char *data;
   size_t len;
   size_t cap;
} X_CURL_BUF;

/* A list option kept by the handle until libcurl no longer needs it */
typedef struct
{
   CURLoption option;
   struct curl_slist *list;
} X_CURL_SLIST;

typedef struct X_CURL_MULTI X_CURL_MULTI;

/* Easy handle: one transfer */
typedef struct
{
   X_CURL_HEAD head;
   CURL *curl;
   X_CURL_MULTI *multi;
   X_CURL_BUF buffer;
   int64_t downloadFile;
   int64_t uploadFile;
   X_CURL_BUF upload;
   size_t uploadPos;
   X_ITEM progress;
   X_ITEM debug;
   X_CURL_SLIST *slists;
   size_t slistCount;
   size_t slistCap;
} X_CURL_EASY;

/* Multi handle: many transfers at the same time */
struct X_CURL_MULTI
{
   X_CURL_HEAD head;
   CURLM *multi;
   X_CURL_EASY **easies;
   size_t count;
   size_t cap;
};

/* Share handle */
typedef struct
{
   X_CURL_HEAD head;
   CURLSH *share;
} X_CURL_SHARE;

/* URL handle */
typedef struct
{
   X_CURL_HEAD head;
   CURLU *url;
} X_CURL_URL;

typedef struct X_CURL_PART X_CURL_PART;

/* Multipart form; attached when it became the content of a part */
typedef struct
{
   X_CURL_HEAD head;
   curl_mime *mime;
   int attached;
   X_CURL_PART **parts;
   size_t count;
   size_t cap;
} X_CURL_MIME;

/* Part of a multipart form */
struct X_CURL_PART
{
   X_CURL_HEAD head;
   curl_mimepart *part;
};

/* ---------------------------------------------------------------------------------------------
   Common
   --------------------------------------------------------------------------------------------- */

void x_curlAppend( X_CURL_BUF *buf, const char *text, size_t len );
void x_curlBufFree( X_CURL_BUF *buf );
void *x_curlObjectNew( int kind, size_t size );
void x_curlObjectFree( void *object );
void *x_curlParam( X_CALL_CTX *ctx, int n, int kind );
const char *x_curlString( X_CALL_CTX *ctx, int n );
const X_ITEM *x_curlValue( X_CALL_CTX *ctx, int n );
void x_curlRetObject( X_CALL_CTX *ctx, void *object );
int x_curlGlobalInit( long flags );
void x_curlGlobalCleanup( void );
void x_curlBegin( void );
void x_curlEnd( void );
struct curl_slist *x_curlSlist( const X_ITEM *array );
void x_curlArrayFromSlist( X_ITEM *result, const struct curl_slist *list );

/* ---------------------------------------------------------------------------------------------
   Easy handle
   --------------------------------------------------------------------------------------------- */

X_CURL_EASY *x_curlEasyNew( CURL *curl );
X_CURL_EASY *x_curlEasyDup( const X_CURL_EASY *source );
void x_curlEasyFree( X_CURL_EASY *easy );
void x_curlEasyReset( X_CURL_EASY *easy );
void x_curlEasyPrepare( X_CURL_EASY *easy );
CURLcode x_curlEasySetopt( X_CURL_EASY *easy, CURLoption option, const X_ITEM *value );
CURLcode x_curlEasyGetinfo( X_CURL_EASY *easy, CURLINFO info, X_ITEM *result );
int x_curlEasyDownload( X_CURL_EASY *easy, const char *path );
int x_curlEasyUpload( X_CURL_EASY *easy, const char *data, size_t len, const char *path );
void x_curlEasyProgress( X_CURL_EASY *easy, const X_ITEM *block );
void x_curlEasyDebug( X_CURL_EASY *easy, const X_ITEM *block );

/* ---------------------------------------------------------------------------------------------
   Multi handle
   --------------------------------------------------------------------------------------------- */

X_CURL_MULTI *x_curlMultiNew( CURLM *multi );
void x_curlMultiFree( X_CURL_MULTI *multi );
CURLMcode x_curlMultiAdd( X_CURL_MULTI *multi, X_CURL_EASY *easy );
CURLMcode x_curlMultiRemove( X_CURL_MULTI *multi, X_CURL_EASY *easy );
X_CURL_EASY *x_curlMultiFind( const X_CURL_MULTI *multi, CURL *curl );
CURLMcode x_curlMultiSetopt( X_CURL_MULTI *multi, CURLMoption option, const X_ITEM *value );

/* ---------------------------------------------------------------------------------------------
   MIME
   --------------------------------------------------------------------------------------------- */

X_CURL_MIME *x_curlMimeNew( curl_mime *mime );
void x_curlMimeFree( X_CURL_MIME *mime );
X_CURL_PART *x_curlMimeAddPart( X_CURL_MIME *mime );
curl_mimepart *x_curlPart( X_CALL_CTX *ctx, int n );

#endif
