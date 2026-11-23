/*
 * Copyright (c) 2024-2026 xDev-Language
 *
 * Library...: xDev OpenSSL
 * File......: openssl-api.c
 *
 */

#include "internal-openssl.h"

#include <openssl/err.h>
#include <openssl/hmac.h>
#include <openssl/pem.h>
#include <openssl/pkcs12.h>
#include <openssl/rand.h>
#include <openssl/rsa.h>

/* ---------------------------------------------------------------------------------------------
   Digests
   --------------------------------------------------------------------------------------------- */

// Digest of the data as bytes: cAlgorithm is "SHA256", "SHA384", "SHA512", "SHA1", "MD5", "SHA3-256"
// and the other names known to OpenSSL; NIL for an unknown algorithm
// HashData( cAlgorithm, cData ) --> cDigest | NIL
X_FUNC( HASHDATA )
{
   const EVP_MD *md = x_sslDigest( ctx, 1, "" );
   unsigned char digest[EVP_MAX_MD_SIZE];
   unsigned int len = 0;

   if( md == NULL )
      return;

   if( X_ISCHAR( ctx, 2 ) && EVP_Digest( x_parc( ctx, 2 ), x_parclen( ctx, 2 ), digest, &len, md, NULL ) == 1 )
      x_retclen( ctx, ( const char * )digest, len );
   else
      x_sslError();
}

// HMAC of the data as bytes, with a digest named like in HashData()
// HmacData( cAlgorithm, cKey, cData ) --> cMac | NIL
X_FUNC( HMACDATA )
{
   const EVP_MD *md = x_sslDigest( ctx, 1, "" );
   const unsigned char *data = ( const unsigned char * )x_parc( ctx, 3 );
   unsigned char mac[EVP_MAX_MD_SIZE];
   unsigned int len = 0;
   int done = 0;

   if( md == NULL )
      return;

   if( X_ISCHAR( ctx, 2 ) && X_ISCHAR( ctx, 3 ) )
      done = HMAC( md, x_parc( ctx, 2 ), ( int )x_parclen( ctx, 2 ), data, x_parclen( ctx, 3 ), mac, &len ) != NULL;

   if( done )
      x_retclen( ctx, ( const char * )mac, len );
   else
      x_sslError();
}

// Cryptographically strong random bytes, e.g. keys and IVs
// RandomBytes( nLen ) --> cBytes | NIL
X_FUNC( RANDOMBYTES )
{
   int len = x_parni( ctx, 1 );
   unsigned char *data = IIF( len > 0, ( unsigned char * )x_xgrab( ( size_t )len + 1 ), NULL );

   x_sslClearError();

   if( data == NULL )
      return;

   if( RAND_bytes( data, len ) != 1 )
   {
      x_sslError();
      x_xfree( data );
      return;
   }

   data[len] = '\0';
   x_retclen_buffer( ctx, ( char * )data, ( size_t )len );
}

/* ---------------------------------------------------------------------------------------------
   AES
   --------------------------------------------------------------------------------------------- */

// Encrypt with AES: AES_MODE_* from openssl.ch (default AES_MODE_CBC); in AES_MODE_GCM the 16 byte
// tag follows the encrypted data
// AesEncrypt( cKey, cIv, cData, [nMode] ) --> cEncrypted | NIL
X_FUNC( AESENCRYPT )
{
   x_sslAes( ctx, 1 );
}

// Decrypt with AES; NIL for a wrong key, damaged data or a GCM tag that does not match
// AesDecrypt( cKey, cIv, cData, [nMode] ) --> cData | NIL
X_FUNC( AESDECRYPT )
{
   x_sslAes( ctx, 0 );
}

/* ---------------------------------------------------------------------------------------------
   Public key cryptography
   --------------------------------------------------------------------------------------------- */

// Encrypt with an RSA public key, given in PEM or DER, directly or as a certificate:
// RSA_PADDING_* from openssl.ch (default RSA_PADDING_OAEP_SHA256)
// RsaEncrypt( cPublicKey, cData, [nPadding] ) --> cEncrypted | NIL
X_FUNC( RSAENCRYPT )
{
   const unsigned char *data = ( const unsigned char * )x_parc( ctx, 2 );
   size_t dataLen = x_parclen( ctx, 2 );
   int padding = IIF( X_ISNUM( ctx, 3 ), x_parni( ctx, 3 ), RSA_PADDING_OAEP_SHA256 );
   EVP_PKEY *key = NULL;
   EVP_PKEY_CTX *pctx = NULL;
   unsigned char *out = NULL;
   size_t len = 0;
   int ok;

   x_sslClearError();
   if( X_ISCHAR( ctx, 1 ) && X_ISCHAR( ctx, 2 ) )
      key = x_sslPublicKey( x_parc( ctx, 1 ), x_parclen( ctx, 1 ) );

   ok = key != NULL;
   if( ok )
   {
      pctx = EVP_PKEY_CTX_new( key, NULL );
      ok = pctx != NULL && EVP_PKEY_encrypt_init( pctx ) > 0 && x_sslRsaPadding( pctx, padding );
   }

   if( ok )
      ok = EVP_PKEY_encrypt( pctx, NULL, &len, data, dataLen ) > 0;

   if( ok )
   {
      out = ( unsigned char * )x_xgrab( len + 1 );
      ok = EVP_PKEY_encrypt( pctx, out, &len, data, dataLen ) > 0;
   }

   if( ok )
      x_retclen_buffer( ctx, ( char * )out, len );
   else
   {
      if( out != NULL )
         x_xfree( out );

      if( key != NULL && x_sslLastError()[0] == '\0' )
         x_sslError();
   }

   EVP_PKEY_CTX_free( pctx );
   EVP_PKEY_free( key );
}

// Decrypt with an RSA private key in PEM or DER, protected by cPassword or not
// RsaDecrypt( cPrivateKey, cData, [nPadding], [cPassword] ) --> cData | NIL
X_FUNC( RSADECRYPT )
{
   const unsigned char *data = ( const unsigned char * )x_parc( ctx, 2 );
   size_t dataLen = x_parclen( ctx, 2 );
   int padding = IIF( X_ISNUM( ctx, 3 ), x_parni( ctx, 3 ), RSA_PADDING_OAEP_SHA256 );
   EVP_PKEY *key = NULL;
   EVP_PKEY_CTX *pctx = NULL;
   unsigned char *out = NULL;
   size_t len = 0;
   int ok;

   x_sslClearError();
   if( X_ISCHAR( ctx, 1 ) && X_ISCHAR( ctx, 2 ) )
      key = x_sslPrivateKey( x_parc( ctx, 1 ), x_parclen( ctx, 1 ), x_sslString( ctx, 4 ) );

   ok = key != NULL;
   if( ok )
   {
      pctx = EVP_PKEY_CTX_new( key, NULL );
      ok = pctx != NULL && EVP_PKEY_decrypt_init( pctx ) > 0 && x_sslRsaPadding( pctx, padding );
   }

   if( ok )
      ok = EVP_PKEY_decrypt( pctx, NULL, &len, data, dataLen ) > 0;

   if( ok )
   {
      out = ( unsigned char * )x_xgrab( len + 1 );
      ok = EVP_PKEY_decrypt( pctx, out, &len, data, dataLen ) > 0;
   }

   if( ok )
   {
      out[len] = '\0';
      x_retclen_buffer( ctx, ( char * )out, len );
   }
   else
   {
      if( out != NULL )
         x_xfree( out );

      if( key != NULL && x_sslLastError()[0] == '\0' )
         x_sslError();
   }

   EVP_PKEY_CTX_free( pctx );
   EVP_PKEY_free( key );
}

// Sign data with a private key (RSA, EC or Ed25519) in PEM or DER; cDigest defaults to "SHA256",
// nPadding to RSA_PADDING_PKCS1 for RSA keys; an EC signature is DER encoded
// SignData( cPrivateKey, cData, [cDigest], [cPassword], [nPadding] ) --> cSignature | NIL
X_FUNC( SIGNDATA )
{
   const unsigned char *data = ( const unsigned char * )x_parc( ctx, 2 );
   size_t dataLen = x_parclen( ctx, 2 );
   int padding = IIF( X_ISNUM( ctx, 5 ), x_parni( ctx, 5 ), RSA_PADDING_PKCS1 );
   EVP_PKEY *key = NULL;
   EVP_MD_CTX *mctx = NULL;
   EVP_PKEY_CTX *pctx = NULL;
   const EVP_MD *md = NULL;
   unsigned char *out = NULL;
   size_t len = 0;
   int ok, id, edwards;

   x_sslClearError();
   if( X_ISCHAR( ctx, 1 ) && X_ISCHAR( ctx, 2 ) )
      key = x_sslPrivateKey( x_parc( ctx, 1 ), x_parclen( ctx, 1 ), x_sslString( ctx, 4 ) );

   /* Ed25519 and Ed448 sign without a digest */
   id = IIF( key != NULL, EVP_PKEY_get_id( key ), EVP_PKEY_NONE );
   edwards = id == EVP_PKEY_ED25519 || id == EVP_PKEY_ED448;
   if( key != NULL && !edwards )
      md = x_sslDigest( ctx, 3, "SHA256" );

   ok = key != NULL && ( md != NULL || edwards );
   if( ok )
   {
      mctx = EVP_MD_CTX_new();
      ok = mctx != NULL && EVP_DigestSignInit( mctx, &pctx, md, NULL, key ) == 1;
   }

   if( ok && id == EVP_PKEY_RSA )
      ok = x_sslRsaPadding( pctx, padding );

   if( ok )
      ok = EVP_DigestSign( mctx, NULL, &len, data, dataLen ) == 1;

   if( ok )
   {
      out = ( unsigned char * )x_xgrab( len + 1 );
      ok = EVP_DigestSign( mctx, out, &len, data, dataLen ) == 1;
   }

   if( ok )
      x_retclen_buffer( ctx, ( char * )out, len );
   else
   {
      if( out != NULL )
         x_xfree( out );

      if( key != NULL && x_sslLastError()[0] == '\0' )
         x_sslError();
   }

   EVP_MD_CTX_free( mctx );
   EVP_PKEY_free( key );
}

// Verify a signature made by SignData() with the public key or certificate in PEM or DER
// VerifyData( cPublicKey, cData, cSignature, [cDigest], [nPadding] ) --> lValid
X_FUNC( VERIFYDATA )
{
   const unsigned char *data = ( const unsigned char * )x_parc( ctx, 2 );
   size_t dataLen = x_parclen( ctx, 2 );
   const unsigned char *signature = ( const unsigned char * )x_parc( ctx, 3 );
   size_t signatureLen = x_parclen( ctx, 3 );
   int padding = IIF( X_ISNUM( ctx, 5 ), x_parni( ctx, 5 ), RSA_PADDING_PKCS1 );
   EVP_PKEY *key = NULL;
   EVP_MD_CTX *mctx = NULL;
   EVP_PKEY_CTX *pctx = NULL;
   const EVP_MD *md = NULL;
   int ok, id, edwards;

   x_sslClearError();
   if( X_ISCHAR( ctx, 1 ) && X_ISCHAR( ctx, 2 ) && X_ISCHAR( ctx, 3 ) )
      key = x_sslPublicKey( x_parc( ctx, 1 ), x_parclen( ctx, 1 ) );

   /* Ed25519 and Ed448 verify without a digest */
   id = IIF( key != NULL, EVP_PKEY_get_id( key ), EVP_PKEY_NONE );
   edwards = id == EVP_PKEY_ED25519 || id == EVP_PKEY_ED448;
   if( key != NULL && !edwards )
      md = x_sslDigest( ctx, 4, "SHA256" );

   ok = key != NULL && ( md != NULL || edwards );
   if( ok )
   {
      mctx = EVP_MD_CTX_new();
      ok = mctx != NULL && EVP_DigestVerifyInit( mctx, &pctx, md, NULL, key ) == 1;
   }

   if( ok && id == EVP_PKEY_RSA )
      ok = x_sslRsaPadding( pctx, padding );

   if( ok )
      ok = EVP_DigestVerify( mctx, signature, signatureLen, data, dataLen ) == 1;

   if( !ok && key != NULL && x_sslLastError()[0] == '\0' )
      x_sslError();

   x_retl( ctx, ok );

   EVP_MD_CTX_free( mctx );
   EVP_PKEY_free( key );
}

// Generate an RSA key pair (default 2048 bits): { cPrivateKeyPem, cPublicKeyPem }
// RsaGenerateKey( [nBits] ) --> aKeys | NIL
X_FUNC( RSAGENERATEKEY )
{
   unsigned int bits = ( unsigned int )IIF( X_ISNUM( ctx, 1 ), x_parni( ctx, 1 ), 2048 );
   X_ITEM value __attribute__( ( cleanup( x_itemClear ) ) ) = X_ITEM_NIL_INIT;
   EVP_PKEY *key;
   BIO *bio;
   X_ITEM *array;

   x_sslClearError();
   key = EVP_RSA_gen( bits );

   if( key == NULL )
   {
      x_sslError();
      return;
   }

   array = x_itemArrayNew( 2 );

   bio = BIO_new( BIO_s_mem() );
   PEM_write_bio_PrivateKey( bio, key, NULL, NULL, 0, NULL, NULL );
   x_sslBioPut( &value, bio );
   x_arraySet( array, 1, &value );
   BIO_free( bio );

   bio = BIO_new( BIO_s_mem() );
   PEM_write_bio_PUBKEY( bio, key );
   x_sslBioPut( &value, bio );
   x_arraySet( array, 2, &value );
   BIO_free( bio );

   EVP_PKEY_free( key );

   x_itemReturn( ctx, array );
   x_itemRelease( array );
}

/* ---------------------------------------------------------------------------------------------
   Certificates
   --------------------------------------------------------------------------------------------- */

// Details of an X.509 certificate in PEM or DER: { cSubject, cIssuer, cSerial, cNotBefore,
// cNotAfter, cDer }; names follow RFC 2253, the serial number is decimal, the times are UTC
// "YYYY-MM-DD HH:MM:SS"
// CertInfo( cCertificate ) --> aInfo | NIL
X_FUNC( CERTINFO )
{
   X_ITEM value __attribute__( ( cleanup( x_itemClear ) ) ) = X_ITEM_NIL_INIT;
   X509 *cert;
   X_ITEM *array;
   BIGNUM *serial;
   char *decimal;
   unsigned char *der = NULL;
   int len;

   x_sslClearError();
   cert = IIF( X_ISCHAR( ctx, 1 ), x_sslCertificate( x_parc( ctx, 1 ), x_parclen( ctx, 1 ) ), NULL );

   if( cert == NULL )
      return;

   array = x_itemArrayNew( 6 );

   x_sslPutName( &value, X509_get_subject_name( cert ) );
   x_arraySet( array, 1, &value );
   x_sslPutName( &value, X509_get_issuer_name( cert ) );
   x_arraySet( array, 2, &value );

   serial = ASN1_INTEGER_to_BN( X509_get0_serialNumber( cert ), NULL );
   decimal = IIF( serial != NULL, BN_bn2dec( serial ), NULL );
   x_arraySetC( array, 3, IIF( decimal != NULL, decimal, "" ) );
   OPENSSL_free( decimal );
   BN_free( serial );

   x_sslPutTime( &value, X509_get0_notBefore( cert ) );
   x_arraySet( array, 4, &value );
   x_sslPutTime( &value, X509_get0_notAfter( cert ) );
   x_arraySet( array, 5, &value );

   len = i2d_X509( cert, &der );
   x_arraySetCL( array, 6, IIF( len > 0, ( const char * )der, "" ), ( size_t )IIF( len > 0, len, 0 ) );
   OPENSSL_free( der );

   X509_free( cert );

   x_itemReturn( ctx, array );
   x_itemRelease( array );
}

// Open a PKCS#12 file content (.p12, .pfx): { cPrivateKeyPem, cCertificatePem, aChainPem }
// Pkcs12Read( cData, [cPassword] ) --> aContent | NIL
X_FUNC( PKCS12READ )
{
   X_ITEM value __attribute__( ( cleanup( x_itemClear ) ) ) = X_ITEM_NIL_INIT;
   const char *password = IIF( X_ISCHAR( ctx, 2 ), x_parc( ctx, 2 ), "" );
   STACK_OF( X509 ) *chain = NULL;
   EVP_PKEY *key = NULL;
   X509 *cert = NULL;
   PKCS12 *p12;
   X_ITEM *array;
   BIO *bio;
   int count;
   int i;

   if( !X_ISCHAR( ctx, 1 ) )
      return;

   x_sslClearError();
   bio = x_sslBio( x_parc( ctx, 1 ), x_parclen( ctx, 1 ) );
   p12 = d2i_PKCS12_bio( bio, NULL );
   BIO_free( bio );

   if( p12 == NULL || PKCS12_parse( p12, password, &key, &cert, &chain ) != 1 )
   {
      x_sslError();
      PKCS12_free( p12 );
      return;
   }

   PKCS12_free( p12 );
   array = x_itemArrayNew( 3 );

   bio = BIO_new( BIO_s_mem() );
   if( key != NULL )
      PEM_write_bio_PrivateKey( bio, key, NULL, NULL, 0, NULL, NULL );

   x_sslBioPut( &value, bio );
   x_arraySet( array, 1, &value );
   BIO_free( bio );

   bio = BIO_new( BIO_s_mem() );
   if( cert != NULL )
      PEM_write_bio_X509( bio, cert );

   x_sslBioPut( &value, bio );
   x_arraySet( array, 2, &value );
   BIO_free( bio );

   count = IIF( chain != NULL, sk_X509_num( chain ), 0 );
   x_arrayNew( &value, count );
   for( i = 0; i < count; i++ )
   {
      X_ITEM pem __attribute__( ( cleanup( x_itemClear ) ) ) = X_ITEM_NIL_INIT;

      bio = BIO_new( BIO_s_mem() );
      PEM_write_bio_X509( bio, sk_X509_value( chain, i ) );
      x_sslBioPut( &pem, bio );
      x_arraySet( &value, ( size_t )i + 1, &pem );
      BIO_free( bio );
   }

   x_arraySet( array, 3, &value );

   EVP_PKEY_free( key );
   X509_free( cert );
   sk_X509_pop_free( chain, X509_free );

   x_itemReturn( ctx, array );
   x_itemRelease( array );
}

/* ---------------------------------------------------------------------------------------------
   Library
   --------------------------------------------------------------------------------------------- */

// Text of the last error of a function of this library, empty when it succeeded
// CryptoError( ) --> cMessage
X_FUNC( CRYPTOERROR )
{
   x_retc( ctx, x_sslLastError() );
}

// Version of OpenSSL
// OpenSslVersion( ) --> cVersion
X_FUNC( OPENSSLVERSION )
{
   x_retc( ctx, OpenSSL_version( OPENSSL_VERSION ) );
}
