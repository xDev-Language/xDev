/*
 * Copyright (c) 2024-2026 xDev-Language
 *
 * Library: xDev OpenSSL
 * File: internal-openssl.c
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "internal-openssl.h"

#include <openssl/err.h>
#include <openssl/pem.h>
#include <openssl/rsa.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#if X_OS_WINDOWS
   #pragma comment( lib, "crypt32" )
   #pragma comment( lib, "gdi32" )
   #pragma comment( lib, "ws2_32" )
#else
   #pragma comment( lib, "pthread" )
   #pragma comment( lib, "dl" )
#endif

/* A function of OpenSSL that gives one AES cipher */
typedef const EVP_CIPHER *( *X_SSL_CIPHER )( void );

static char s_error[256];

/* ---------------------------------------------------------------------------------------------
   Errors
   --------------------------------------------------------------------------------------------- */

/* Keep the text of the last OpenSSL error and empty its queue */
void x_sslError( void )
{
   unsigned long code = ERR_peek_last_error();

   if( code != 0 )
      ERR_error_string_n( code, s_error, sizeof( s_error ) );
   else
      strcpy( s_error, "unknown OpenSSL error" );

   ERR_clear_error();
}

/* Keep an error text of the library and empty the queue of OpenSSL */
void x_sslSetError( const char *message )
{
   snprintf( s_error, sizeof( s_error ), "%s", message );
   ERR_clear_error();
}

/* No error: a function of the library starts with this */
void x_sslClearError( void )
{
   s_error[0] = '\0';
   ERR_clear_error();
}

/* The text of the last error, "" when there was none */
const char *x_sslLastError( void )
{
   return s_error;
}

/* ---------------------------------------------------------------------------------------------
   Arguments, keys and certificates
   --------------------------------------------------------------------------------------------- */

/* The string in argument n, or NULL */
const char *x_sslString( X_CALL_CTX *ctx, int n )
{
   return IIF( X_ISCHAR( ctx, n ), x_parc( ctx, n ), NULL );
}

/* The digest named in argument n, defaultName without it; NULL with the error text for an unknown name */
const EVP_MD *x_sslDigest( X_CALL_CTX *ctx, int n, const char *defaultName )
{
   const char *name = IIF( X_ISCHAR( ctx, n ), x_parc( ctx, n ), defaultName );
   const EVP_MD *md = EVP_get_digestbyname( name );

   x_sslClearError();

   if( md == NULL )
      x_sslSetError( "unknown digest algorithm" );

   return md;
}

/* A memory BIO that reads the bytes */
BIO *x_sslBio( const char *data, size_t len )
{
   return BIO_new_mem_buf( data, ( int )len );
}

/* The bytes written to a memory BIO into an item */
void x_sslBioPut( X_ITEM *item, BIO *bio )
{
   char *data = NULL;
   long len = BIO_get_mem_data( bio, &data );

   x_itemPutCL( item, IIF( data != NULL, data, "" ), ( size_t )IIF( len > 0, len, 0 ) );
}

/* Return the bytes written to a memory BIO */
void x_sslBioReturn( X_CALL_CTX *ctx, BIO *bio )
{
   char *data = NULL;
   long len = BIO_get_mem_data( bio, &data );

   x_retclen( ctx, IIF( data != NULL, data, "" ), ( size_t )IIF( len > 0, len, 0 ) );
}

/* A name of a certificate as RFC 2253 text, UTF-8 characters left as they are */
void x_sslPutName( X_ITEM *item, const X509_NAME *name )
{
   BIO *bio = BIO_new( BIO_s_mem() );

   X509_NAME_print_ex( bio, name, 0, XN_FLAG_RFC2253 & ~ASN1_STRFLGS_ESC_MSB );
   x_sslBioPut( item, bio );
   BIO_free( bio );
}

/* A time of a certificate as UTC "YYYY-MM-DD HH:MM:SS", "" when it cannot be read */
void x_sslPutTime( X_ITEM *item, const ASN1_TIME *time )
{
   struct tm when;
   char text[32];
   int year, month, day, hour, minute, second;

   if( ASN1_TIME_to_tm( time, &when ) != 1 )
   {
      x_itemPutC( item, "" );
      return;
   }

   year = when.tm_year + 1900;
   month = when.tm_mon + 1;
   day = when.tm_mday;
   hour = when.tm_hour;
   minute = when.tm_min;
   second = when.tm_sec;
   snprintf( text, sizeof( text ), "%04d-%02d-%02d %02d:%02d:%02d", year, month, day, hour, minute, second );
   x_itemPutC( item, text );
}

/* A public key in PEM or DER, given directly or as an X.509 certificate */
EVP_PKEY *x_sslPublicKey( const char *data, size_t len )
{
   const unsigned char *der = ( const unsigned char * )data;
   EVP_PKEY *key = NULL;
   X509 *cert;
   BIO *bio = x_sslBio( data, len );

   key = PEM_read_bio_PUBKEY( bio, NULL, NULL, NULL );

   if( key == NULL )
   {
      BIO_reset( bio );
      cert = PEM_read_bio_X509( bio, NULL, NULL, NULL );

      if( cert == NULL )
         cert = d2i_X509( NULL, &der, ( long )len );

      if( cert != NULL )
      {
         key = X509_get_pubkey( cert );
         X509_free( cert );
      }
   }

   if( key == NULL )
   {
      der = ( const unsigned char * )data;
      key = d2i_PUBKEY( NULL, &der, ( long )len );
   }

   BIO_free( bio );

   if( key != NULL )
      ERR_clear_error();
   else
      x_sslError();

   return key;
}

/* Give OpenSSL the password of an encrypted key; without one it must not ask on the console */
static int s_password( char *buf, int size, int rwflag, void *userData )
{
   const char *password = ( const char * )userData;
   int len = IIF( password != NULL, ( int )strlen( password ), 0 );

   UNUSED( rwflag );

   if( len > size )
      len = size;

   if( len > 0 )
      memcpy( buf, password, ( size_t )len );

   return len;
}

/* A private key in PEM (encrypted with the password or not) or DER */
EVP_PKEY *x_sslPrivateKey( const char *data, size_t len, const char *password )
{
   const unsigned char *der = ( const unsigned char * )data;
   EVP_PKEY *key;
   BIO *bio = x_sslBio( data, len );

   key = PEM_read_bio_PrivateKey( bio, NULL, s_password, ( void * )password );
   BIO_free( bio );

   if( key == NULL )
      key = d2i_AutoPrivateKey( NULL, &der, ( long )len );

   if( key != NULL )
      ERR_clear_error();
   else
      x_sslError();

   return key;
}

/* An X.509 certificate in PEM or DER */
X509 *x_sslCertificate( const char *data, size_t len )
{
   const unsigned char *der = ( const unsigned char * )data;
   BIO *bio = x_sslBio( data, len );
   X509 *cert;

   cert = PEM_read_bio_X509( bio, NULL, NULL, NULL );
   BIO_free( bio );

   if( cert == NULL )
      cert = d2i_X509( NULL, &der, ( long )len );

   if( cert != NULL )
      ERR_clear_error();
   else
      x_sslError();

   return cert;
}

/* ---------------------------------------------------------------------------------------------
   Ciphers
   --------------------------------------------------------------------------------------------- */

/* The AES cipher of a mode, AES-128, AES-192 or AES-256 by the length of the key; NULL for another length */
const EVP_CIPHER *x_sslAesCipher( int mode, size_t keyLen )
{
   /* One row for each AES_MODE_* of openssl.ch, in their order */
   static const X_SSL_CIPHER ciphers[4][3] = {
      { EVP_aes_128_cbc, EVP_aes_192_cbc, EVP_aes_256_cbc },
      { EVP_aes_128_ctr, EVP_aes_192_ctr, EVP_aes_256_ctr },
      { EVP_aes_128_gcm, EVP_aes_192_gcm, EVP_aes_256_gcm },
      { EVP_aes_128_ecb, EVP_aes_192_ecb, EVP_aes_256_ecb }
   };
   int size;

   if( keyLen == 16 )
      size = 0;
   else if( keyLen == 24 )
      size = 1;
   else if( keyLen == 32 )
      size = 2;
   else
      return NULL;

   if( mode < AES_MODE_CBC || mode > AES_MODE_ECB )
      return NULL;

   return ciphers[mode - AES_MODE_CBC][size]();
}

/* AesEncrypt()/AesDecrypt( cKey, cIv, cData, [nMode] ); returns 0 after setting the error text */
int x_sslAes( X_CALL_CTX *ctx, int encrypt )
{
   const char *key = x_parc( ctx, 1 );
   size_t keyLen = x_parclen( ctx, 1 );
   const char *iv = x_parc( ctx, 2 );
   size_t ivLen = x_parclen( ctx, 2 );
   const char *data = x_parc( ctx, 3 );
   size_t len = x_parclen( ctx, 3 );
   int mode = IIF( X_ISNUM( ctx, 4 ), x_parni( ctx, 4 ), AES_MODE_CBC );
   const EVP_CIPHER *cipher = x_sslAesCipher( mode, keyLen );
   const unsigned char *ivBytes = IIF( mode == AES_MODE_ECB, NULL, ( const unsigned char * )iv );
   size_t tagLen = IIF( mode == AES_MODE_GCM, 16, 0 );
   EVP_CIPHER_CTX *cctx;
   unsigned char *out;
   int outLen = 0;
   int lastLen = 0;
   int ok;

   x_sslClearError();

   if( cipher == NULL )
   {
      strcpy( s_error, "key must have 16, 24 or 32 bytes and the mode must be AES_MODE_*" );
      return 0;
   }

   if( !X_ISCHAR( ctx, 3 ) )
   {
      strcpy( s_error, "data must be a string" );
      return 0;
   }

   if( mode == AES_MODE_GCM && ivLen == 0 )
   {
      strcpy( s_error, "IV must not be empty" );
      return 0;
   }

   if( mode != AES_MODE_GCM && mode != AES_MODE_ECB && ivLen != 16 )
   {
      strcpy( s_error, "IV must have 16 bytes" );
      return 0;
   }

   if( !encrypt && len < tagLen )
   {
      strcpy( s_error, "data is shorter than the GCM tag" );
      return 0;
   }

   if( !encrypt )
      len -= tagLen;

   cctx = EVP_CIPHER_CTX_new();
   out = ( unsigned char * )x_xgrab( len + 32 + tagLen );

   ok = EVP_CipherInit_ex( cctx, cipher, NULL, NULL, NULL, encrypt ) == 1;

   if( ok && mode == AES_MODE_GCM )
      ok = EVP_CIPHER_CTX_ctrl( cctx, EVP_CTRL_GCM_SET_IVLEN, ( int )ivLen, NULL ) == 1;

   if( ok )
      ok = EVP_CipherInit_ex( cctx, NULL, NULL, ( const unsigned char * )key, ivBytes, encrypt ) == 1;

   if( ok )
      ok = EVP_CipherUpdate( cctx, out, &outLen, ( const unsigned char * )data, ( int )len ) == 1;

   if( ok && !encrypt && mode == AES_MODE_GCM )
      ok = EVP_CIPHER_CTX_ctrl( cctx, EVP_CTRL_GCM_SET_TAG, ( int )tagLen, ( void * )( data + len ) ) == 1;

   if( ok )
      ok = EVP_CipherFinal_ex( cctx, out + outLen, &lastLen ) == 1;

   if( ok && encrypt && mode == AES_MODE_GCM )
      ok = EVP_CIPHER_CTX_ctrl( cctx, EVP_CTRL_GCM_GET_TAG, ( int )tagLen, out + outLen + lastLen ) == 1;

   EVP_CIPHER_CTX_free( cctx );

   if( !ok )
   {
      x_xfree( out );

      if( !encrypt && mode == AES_MODE_GCM && ERR_peek_last_error() == 0 )
         strcpy( s_error, "GCM tag does not match: wrong key or damaged data" );
      else
         x_sslError();

      return 0;
   }

   x_retclen_buffer( ctx, ( char * )out, ( size_t )( outLen + lastLen ) + IIF( encrypt, tagLen, 0 ) );

   return 1;
}

/* Set an RSA_PADDING_* of openssl.ch on a key context; 0 when it fails */
int x_sslRsaPadding( EVP_PKEY_CTX *pctx, int padding )
{
   switch( padding )
   {
   case RSA_PADDING_OAEP_SHA256:
      if( EVP_PKEY_CTX_set_rsa_padding( pctx, RSA_PKCS1_OAEP_PADDING ) <= 0 )
         return 0;

      if( EVP_PKEY_CTX_set_rsa_oaep_md( pctx, EVP_sha256() ) <= 0 )
         return 0;

      return EVP_PKEY_CTX_set_rsa_mgf1_md( pctx, EVP_sha256() ) > 0;

   case RSA_PADDING_OAEP_SHA1:
      return EVP_PKEY_CTX_set_rsa_padding( pctx, RSA_PKCS1_OAEP_PADDING ) > 0;

   case RSA_PADDING_PKCS1:
      return EVP_PKEY_CTX_set_rsa_padding( pctx, RSA_PKCS1_PADDING ) > 0;

   case RSA_PADDING_PSS:
      if( EVP_PKEY_CTX_set_rsa_padding( pctx, RSA_PKCS1_PSS_PADDING ) <= 0 )
         return 0;

      return EVP_PKEY_CTX_set_rsa_pss_saltlen( pctx, RSA_PSS_SALTLEN_DIGEST ) > 0;
   }

   strcpy( s_error, "unknown RSA_PADDING_* value" );

   return 0;
}
