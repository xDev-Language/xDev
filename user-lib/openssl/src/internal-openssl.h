/*
 * Copyright (c) 2024-2026 xDev-Language
 *
 * Library: xDev OpenSSL
 * File: internal-openssl.h
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#ifndef INTERNAL_OPENSSL_H
#define INTERNAL_OPENSSL_H

#include "platform.h"
#include "internal.h"

#include <openssl/evp.h>
#include <openssl/x509.h>

#include "xdev/openssl.ch"

/* ---------------------------------------------------------------------------------------------
   Errors
   --------------------------------------------------------------------------------------------- */

void x_sslError( void );
void x_sslSetError( const char *message );
void x_sslClearError( void );
const char *x_sslLastError( void );

/* ---------------------------------------------------------------------------------------------
   Arguments, keys and certificates
   --------------------------------------------------------------------------------------------- */

const char *x_sslString( X_CALL_CTX *ctx, int n );
const EVP_MD *x_sslDigest( X_CALL_CTX *ctx, int n, const char *defaultName );
BIO *x_sslBio( const char *data, size_t len );
void x_sslBioReturn( X_CALL_CTX *ctx, BIO *bio );
void x_sslBioPut( X_ITEM *item, BIO *bio );
void x_sslPutName( X_ITEM *item, const X509_NAME *name );
void x_sslPutTime( X_ITEM *item, const ASN1_TIME *time );
EVP_PKEY *x_sslPublicKey( const char *data, size_t len );
EVP_PKEY *x_sslPrivateKey( const char *data, size_t len, const char *password );
X509 *x_sslCertificate( const char *data, size_t len );

/* ---------------------------------------------------------------------------------------------
   Ciphers
   --------------------------------------------------------------------------------------------- */

const EVP_CIPHER *x_sslAesCipher( int mode, size_t keyLen );
int x_sslAes( X_CALL_CTX *ctx, int encrypt );
int x_sslRsaPadding( EVP_PKEY_CTX *pctx, int padding );

#endif
