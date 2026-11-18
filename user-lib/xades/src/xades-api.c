/*
 * Copyright (c) 2024-2026 xDev-Language
 *
 * Library: xDev XAdES
 * File: xades-api.c
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "internal-xades.h"

/* ---------------------------------------------------------------------------------------------
   XAdES
   --------------------------------------------------------------------------------------------- */

// Exclusive canonical form (exc-c14n, without comments) of a UTF-8 XML document; NIL on error (see XadesError())
// XmlCanonical( cXml ) --> cXml | NIL
X_FUNC( XMLCANONICAL )
{
   X_XML_BUF buf = { NULL, 0, 0 };

   X_ARGCHECK( ctx, X_ISCHAR( ctx, 1 ), "XMLCANONICAL" );

   if( x_xadesCanonicalText( &buf, x_parc( ctx, 1 ), x_parclen( ctx, 1 ) ) )
      x_retclen_buffer( ctx, buf.data, buf.len );
   else
      x_xmlBufFree( &buf );
}

// Sign a UTF-8 XML document with an enveloped XAdES-BES signature (exc-c14n, SHA-256);
// the key (EC or RSA) and the certificate in PEM or DER, the key may be encrypted with cPassword;
// NIL on error (see XadesError())
// XadesSign( cXml, cPrivateKey, cCertificate, [cPassword] ) --> cSignedXml | NIL
X_FUNC( XADESSIGN )
{
   X_XML_BUF buf = { NULL, 0, 0 };
   const char *key;
   size_t keyLen;
   const char *cert;
   size_t certLen;
   const char *password;

   X_ARGCHECK( ctx, X_ISCHAR( ctx, 1 ) && X_ISCHAR( ctx, 2 ) && X_ISCHAR( ctx, 3 ), "XADESSIGN" );
   X_ARGCHECK_OPT( ctx, 4, X_ISCHAR( ctx, 4 ) || X_ISNIL( ctx, 4 ), "XADESSIGN" );

   key = x_parc( ctx, 2 );
   keyLen = x_parclen( ctx, 2 );
   cert = x_parc( ctx, 3 );
   certLen = x_parclen( ctx, 3 );
   password = IIF( X_ISCHAR( ctx, 4 ), x_parc( ctx, 4 ), NULL );

   if( x_xadesSign( &buf, x_parc( ctx, 1 ), x_parclen( ctx, 1 ), key, keyLen, cert, certLen, password ) )
      x_retclen_buffer( ctx, buf.data, buf.len );
   else
      x_xmlBufFree( &buf );
}

// Check a document signed by XadesSign(): the digests of all references and the signature
// made with the certificate from KeyInfo (the certificate itself is not checked)
// XadesVerify( cSignedXml ) --> lValid
X_FUNC( XADESVERIFY )
{
   X_ARGCHECK( ctx, X_ISCHAR( ctx, 1 ), "XADESVERIFY" );

   x_retl( ctx, x_xadesVerify( x_parc( ctx, 1 ), x_parclen( ctx, 1 ) ) );
}

// Description of the last error, empty when none
// XadesError( ) --> cMessage
X_FUNC( XADESERROR )
{
   x_retc( ctx, x_xadesError() );
}
