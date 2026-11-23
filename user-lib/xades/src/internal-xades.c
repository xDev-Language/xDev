/*
 * Copyright (c) 2024-2026 xDev-Language
 *
 * Library...: xDev XAdES
 * File......: internal-xades.c
 *
 */

#include "internal-xades.h"

#include <openssl/bio.h>
#include <openssl/bn.h>
#include <openssl/ec.h>
#include <openssl/evp.h>
#include <openssl/x509.h>
#include <string.h>
#include <time.h>

/* Namespaces and algorithms of XML Signature and XAdES */
#define X_URI_DS "http://www.w3.org/2000/09/xmldsig#"
#define X_URI_XADES "http://uri.etsi.org/01903/v1.3.2#"
#define X_URI_C14N "http://www.w3.org/2001/10/xml-exc-c14n#"
#define X_URI_ENVELOPED "http://www.w3.org/2000/09/xmldsig#enveloped-signature"
#define X_URI_SHA256 "http://www.w3.org/2001/04/xmlenc#sha256"
#define X_URI_ECDSA "http://www.w3.org/2001/04/xmldsig-more#ecdsa-sha256"
#define X_URI_RSA "http://www.w3.org/2001/04/xmldsig-more#rsa-sha256"
#define X_URI_SIGPROPS "http://uri.etsi.org/01903#SignedProperties"

/* A namespace declaration written by an element of the output */
typedef struct
{
   const char *prefix;
   size_t prefixLen;
   const char *uri;
   size_t uriLen;
} X_XADES_NS;

/* The namespace declarations in force at a place of the output */
typedef struct
{
   X_XADES_NS *items;
   size_t count;
   size_t cap;
} X_XADES_NS_STACK;

static char s_error[256];

/* ---------------------------------------------------------------------------------------------
   Errors
   --------------------------------------------------------------------------------------------- */

/* Keep the text of an error */
void x_xadesSetError( const char *message )
{
   strncpy( s_error, message, sizeof( s_error ) - 1 );
   s_error[sizeof( s_error ) - 1] = '\0';
}

/* No error: a function of the library starts with this */
void x_xadesClearError( void )
{
   s_error[0] = '\0';
}

/* The text of the last error, "" when there was none */
const char *x_xadesError( void )
{
   return s_error;
}

/* ---------------------------------------------------------------------------------------------
   Exclusive XML canonicalization (exc-c14n, without comments)
   --------------------------------------------------------------------------------------------- */

/* Append a text ended with '\0' */
static void s_append( X_XML_BUF *out, const char *text )
{
   x_xmlAppend( out, text, strlen( text ) );
}

/* Append text escaped as exc-c14n wants it, in an attribute value or in text */
static void s_escape( X_XML_BUF *out, const char *text, size_t len, int attr )
{
   size_t i;

   for( i = 0; i < len; i++ )
   {
      char c = text[i];

      if( c == '&' )
         s_append( out, "&amp;" );
      else if( c == '<' )
         s_append( out, "&lt;" );
      else if( c == '>' && !attr )
         s_append( out, "&gt;" );
      else if( c == '"' && attr )
         s_append( out, "&quot;" );
      else if( c == '\t' && attr )
         s_append( out, "&#x9;" );
      else if( c == '\n' && attr )
         s_append( out, "&#xA;" );
      else if( c == '\r' )
         s_append( out, "&#xD;" );
      else
         x_xmlAppend( out, &c, 1 );
   }
}

/* Whether an attribute is a namespace declaration */
static int s_isNsDecl( const char *name )
{
   return strcmp( name, "xmlns" ) == 0 || strncmp( name, "xmlns:", 6 ) == 0;
}

/* The length of the prefix of a name, 0 without one */
static size_t s_prefixLen( const char *name )
{
   const char *colon = strchr( name, ':' );

   return IIF( colon != NULL, ( size_t )( colon - name ), 0 );
}

/* A name without its prefix */
static const char *s_localName( const char *name )
{
   const char *colon = strchr( name, ':' );

   return IIF( colon != NULL, colon + 1, name );
}

/* Namespace URI bound to the prefix at the node; NULL when not declared ("" prefix: default namespace) */
static const char *s_lookup( const X_XML_NODE *node, const char *prefix, size_t prefixLen, size_t *uriLen )
{
   static const char s_xmlUri[] = "http://www.w3.org/XML/1998/namespace";
   size_t i;

   if( prefixLen == 3 && memcmp( prefix, "xml", 3 ) == 0 )
   {
      *uriLen = sizeof( s_xmlUri ) - 1;
      return s_xmlUri;
   }

   for( ; node != NULL && node->type == XML_TYPE_ELEMENT; node = node->parent )
   {
      for( i = 0; i < node->attrCount; i++ )
      {
         const char *name = node->attrs[i].name;
         int match;

         if( prefixLen == 0 )
            match = strcmp( name, "xmlns" ) == 0;
         else
         {
            match = strncmp( name, "xmlns:", 6 ) == 0 && strlen( name + 6 ) == prefixLen;
            match = match && memcmp( name + 6, prefix, prefixLen ) == 0;
         }

         if( match )
         {
            *uriLen = node->attrs[i].valueLen;
            return node->attrs[i].value;
         }
      }
   }

   *uriLen = 0;

   return NULL;
}

/* Compare two strings with lengths as bytes; the shorter one comes first at an equal start */
static int s_compare( const char *a, size_t aLen, const char *b, size_t bLen )
{
   int result = memcmp( a, b, IIF( aLen < bLen, aLen, bLen ) );

   if( result != 0 )
      return result;

   if( aLen < bLen )
      return -1;

   return IIF( aLen > bLen, 1, 0 );
}

/* The order of attributes in exc-c14n: by namespace URI, then by local name */
static int s_attrCompare( const X_XML_NODE *node, const X_XML_ATTR *a, const X_XML_ATTR *b )
{
   size_t aLen = 0;
   size_t bLen = 0;
   size_t aPrefix = s_prefixLen( a->name );
   size_t bPrefix = s_prefixLen( b->name );
   const char *aUri = IIF( aPrefix > 0, s_lookup( node, a->name, aPrefix, &aLen ), NULL );
   const char *bUri = IIF( bPrefix > 0, s_lookup( node, b->name, bPrefix, &bLen ), NULL );
   int result = s_compare( IIF( aUri != NULL, aUri, "" ), aLen, IIF( bUri != NULL, bUri, "" ), bLen );

   if( result == 0 )
      result = strcmp( s_localName( a->name ), s_localName( b->name ) );

   return result;
}

/* A namespace declaration written to the output */
static void s_push( X_XADES_NS_STACK *stack, const char *prefix, size_t prefixLen, const char *uri, size_t uriLen )
{
   if( stack->count == stack->cap )
   {
      stack->cap = IIF( stack->cap == 0, 16, stack->cap * 2 );
      stack->items = ( X_XADES_NS * )x_xrealloc( stack->items, stack->cap * sizeof( X_XADES_NS ) );
   }

   stack->items[stack->count].prefix = prefix;
   stack->items[stack->count].prefixLen = prefixLen;
   stack->items[stack->count].uri = uri;
   stack->items[stack->count].uriLen = uriLen;
   stack->count++;
}

/* Namespace already rendered by an output ancestor with the same URI */
static int s_rendered( const X_XADES_NS_STACK *stack, const char *prefix, size_t prefixLen, const char *uri,
                       size_t uriLen )
{
   size_t i = stack->count;

   while( i-- > 0 )
   {
      if( stack->items[i].prefixLen == prefixLen && memcmp( stack->items[i].prefix, prefix, prefixLen ) == 0 )
         return stack->items[i].uriLen == uriLen && memcmp( stack->items[i].uri, uri, uriLen ) == 0;
   }

   /* Nothing rendered yet: an empty default namespace needs no declaration */
   return prefixLen == 0 && uriLen == 0;
}

static void s_canonicalNode( X_XML_BUF *out, const X_XML_NODE *node, const X_XML_NODE *skip, X_XADES_NS_STACK *stack );

/* An element with the namespaces it uses, its attributes in order and its children */
static void s_canonicalElement( X_XML_BUF *out, const X_XML_NODE *node, const X_XML_NODE *skip,
                                X_XADES_NS_STACK *stack )
{
   const char **prefixes;
   size_t *prefixLens;
   const X_XML_ATTR **attrs;
   size_t nPrefixes = 0;
   size_t nAttrs = 0;
   size_t mark = stack->count;
   size_t i;
   size_t j;
   const X_XML_NODE *child;

   prefixes = ( const char ** )x_xgrab( ( node->attrCount + 1 ) * sizeof( char * ) );
   prefixLens = ( size_t * )x_xgrab( ( node->attrCount + 1 ) * sizeof( size_t ) );
   attrs = ( const X_XML_ATTR ** )x_xgrab( ( node->attrCount + 1 ) * sizeof( X_XML_ATTR * ) );

   /* Visibly utilized prefixes: the element's own and those of its attributes */
   prefixes[nPrefixes] = node->name;
   prefixLens[nPrefixes++] = s_prefixLen( node->name );
   for( i = 0; i < node->attrCount; i++ )
   {
      const char *name = node->attrs[i].name;
      size_t len = s_prefixLen( name );

      if( s_isNsDecl( name ) )
         continue;

      attrs[nAttrs++] = &node->attrs[i];
      if( len == 0 || ( len == 3 && memcmp( name, "xml", 3 ) == 0 ) )
         continue;

      for( j = 0; j < nPrefixes; j++ )
      {
         if( prefixLens[j] == len && memcmp( prefixes[j], name, len ) == 0 )
            break;
      }

      if( j == nPrefixes )
      {
         prefixes[nPrefixes] = name;
         prefixLens[nPrefixes++] = len;
      }
   }

   /* Namespace declarations sorted by prefix, attributes by namespace URI and local name */
   for( i = 1; i < nPrefixes; i++ )
   {
      for( j = i; j > 0; j-- )
      {
         const char *p = prefixes[j];
         size_t l = prefixLens[j];

         if( s_compare( prefixes[j - 1], prefixLens[j - 1], p, l ) <= 0 )
            break;

         prefixes[j] = prefixes[j - 1];
         prefixLens[j] = prefixLens[j - 1];
         prefixes[j - 1] = p;
         prefixLens[j - 1] = l;
      }
   }

   for( i = 1; i < nAttrs; i++ )
   {
      for( j = i; j > 0 && s_attrCompare( node, attrs[j - 1], attrs[j] ) > 0; j-- )
      {
         const X_XML_ATTR *a = attrs[j];

         attrs[j] = attrs[j - 1];
         attrs[j - 1] = a;
      }
   }

   s_append( out, "<" );
   s_append( out, node->name );

   for( i = 0; i < nPrefixes; i++ )
   {
      size_t uriLen = 0;
      const char *uri = s_lookup( node, prefixes[i], prefixLens[i], &uriLen );

      if( prefixLens[i] == 3 && memcmp( prefixes[i], "xml", 3 ) == 0 )
         continue;

      if( uri == NULL )
         uri = "";

      if( s_rendered( stack, prefixes[i], prefixLens[i], uri, uriLen ) )
         continue;

      s_append( out, IIF( prefixLens[i] == 0, " xmlns", " xmlns:" ) );
      x_xmlAppend( out, prefixes[i], prefixLens[i] );
      s_append( out, "=\"" );
      s_escape( out, uri, uriLen, 1 );
      s_append( out, "\"" );
      s_push( stack, prefixes[i], prefixLens[i], uri, uriLen );
   }

   for( i = 0; i < nAttrs; i++ )
   {
      s_append( out, " " );
      s_append( out, attrs[i]->name );
      s_append( out, "=\"" );
      s_escape( out, attrs[i]->value, attrs[i]->valueLen, 1 );
      s_append( out, "\"" );
   }

   s_append( out, ">" );

   for( child = node->first; child != NULL; child = child->next )
      s_canonicalNode( out, child, skip, stack );

   s_append( out, "</" );
   s_append( out, node->name );
   s_append( out, ">" );

   stack->count = mark;
   x_xfree( ( void * )prefixes );
   x_xfree( prefixLens );
   x_xfree( ( void * )attrs );
}

/* A node in canonical form; skip is left out */
static void s_canonicalNode( X_XML_BUF *out, const X_XML_NODE *node, const X_XML_NODE *skip, X_XADES_NS_STACK *stack )
{
   if( node == skip )
      return;

   switch( node->type )
   {
   case XML_TYPE_ELEMENT:
      s_canonicalElement( out, node, skip, stack );
      break;
   case XML_TYPE_TEXT:
   case XML_TYPE_CDATA:
      s_escape( out, node->text.data, node->text.len, 0 );
      break;
   case XML_TYPE_PI:
      s_append( out, "<?" );
      s_append( out, node->name );
      if( node->text.len > 0 )
      {
         s_append( out, " " );
         x_xmlAppend( out, node->text.data, node->text.len );
      }

      s_append( out, "?>" );
      break;
   default:
      /* Comments are left out */
      break;
   }
}

/* Canonical form of a document or of a single element (with the namespaces it uses); skip is left out */
void x_xadesCanonical( X_XML_BUF *out, const X_XML_NODE *node, const X_XML_NODE *skip )
{
   X_XADES_NS_STACK stack = { NULL, 0, 0 };
   const X_XML_NODE *child;
   int afterRoot = 0;

   if( node->type == XML_TYPE_DOCUMENT )
   {
      /* Processing instructions outside the root element: a line feed between them and the root */
      for( child = node->first; child != NULL; child = child->next )
      {
         if( child->type == XML_TYPE_ELEMENT )
         {
            s_canonicalNode( out, child, skip, &stack );
            afterRoot = 1;
         }
         else if( child->type == XML_TYPE_PI )
         {
            if( afterRoot )
               s_append( out, "\n" );

            s_canonicalNode( out, child, skip, &stack );
            if( !afterRoot )
               s_append( out, "\n" );
         }
      }
   }
   else
      s_canonicalNode( out, node, skip, &stack );

   if( out->data == NULL )
      x_xmlAppend( out, "", 0 );

   if( stack.items != NULL )
      x_xfree( stack.items );
}

/* Canonical form of an XML text; 0 with the error text when it cannot be parsed */
int x_xadesCanonicalText( X_XML_BUF *out, const char *xml, size_t len )
{
   X_XML_NODE *doc;

   x_xadesClearError();
   doc = x_xmlParse( xml, len );
   if( doc == NULL )
   {
      x_xadesSetError( x_xmlError() );
      return 0;
   }

   x_xadesCanonical( out, doc, NULL );
   x_xmlFree( doc );

   return 1;
}

/* ---------------------------------------------------------------------------------------------
   XAdES-BES signature
   --------------------------------------------------------------------------------------------- */

/* Append bytes as Base64 */
static void s_base64( X_XML_BUF *out, const unsigned char *data, size_t len )
{
   char *text = ( char * )x_xgrab( 4 * ( ( len + 2 ) / 3 ) + 1 );
   int n = EVP_EncodeBlock( ( unsigned char * )text, data, ( int )len );

   x_xmlAppend( out, text, ( size_t )n );
   x_xfree( text );
}

/* Decoded Base64 (white space ignored); the caller frees the result */
static unsigned char *s_unbase64( const char *text, size_t len, size_t *outLen )
{
   char *clean = ( char * )x_xgrab( len + 1 );
   unsigned char *data;
   size_t n = 0;
   size_t i;
   int decoded = -1;

   for( i = 0; i < len; i++ )
   {
      if( text[i] != ' ' && text[i] != '\t' && text[i] != '\r' && text[i] != '\n' )
         clean[n++] = text[i];
   }

   data = ( unsigned char * )x_xgrab( n / 4 * 3 + 1 );
   if( n % 4 == 0 )
      decoded = EVP_DecodeBlock( data, ( unsigned char * )clean, ( int )n );

   if( decoded < 0 )
   {
      x_xfree( clean );
      x_xfree( data );
      return NULL;
   }

   if( n > 0 && clean[n - 1] == '=' )
      decoded--;

   if( n > 1 && clean[n - 2] == '=' )
      decoded--;

   x_xfree( clean );
   *outLen = ( size_t )decoded;

   return data;
}

/* Append the SHA-256 digest of the data as Base64 */
static void s_digest( X_XML_BUF *out, const char *data, size_t len )
{
   unsigned char md[EVP_MAX_MD_SIZE];
   unsigned int mdLen = 0;

   EVP_Digest( data, len, md, &mdLen, EVP_sha256(), NULL );
   s_base64( out, md, mdLen );
}

/* First element in the subtree with the local name (and with the attribute Id equal to id, when given) */
static X_XML_NODE *s_find( X_XML_NODE *node, const char *localName, const char *id, size_t idLen )
{
   X_XML_NODE *child;
   X_XML_NODE *found;

   if( node->type == XML_TYPE_ELEMENT )
   {
      if( id != NULL )
      {
         X_XML_ATTR *attr = x_xmlAttrGet( node, "Id", 2 );

         if( attr != NULL && attr->valueLen == idLen && memcmp( attr->value, id, idLen ) == 0 )
            return node;
      }
      else if( strcmp( s_localName( node->name ), localName ) == 0 )
         return node;
   }

   for( child = node->first; child != NULL; child = child->next )
   {
      if( child->type != XML_TYPE_ELEMENT )
         continue;

      found = s_find( child, localName, id, idLen );
      if( found != NULL )
         return found;
   }

   return NULL;
}

/* The first child element with the local name, or NULL */
static X_XML_NODE *s_childNamed( X_XML_NODE *node, const char *localName )
{
   X_XML_NODE *child;

   if( node == NULL )
      return NULL;

   for( child = x_xmlFirstElement( node ); child != NULL; child = x_xmlNextElement( child ) )
   {
      if( strcmp( s_localName( child->name ), localName ) == 0 )
         return child;
   }

   return NULL;
}

/* Set the text of an element */
static void s_setText( X_XML_NODE *node, X_XML_BUF *text )
{
   x_xmlSetText( node, text->data, text->len );
}

/* ECDSA signature: DER from OpenSSL to R||S of the field size, as XML Signature writes it */
static int s_derToRaw( EVP_PKEY *key, const unsigned char *der, size_t derLen, X_XML_BUF *out )
{
   const unsigned char *p = der;
   ECDSA_SIG *sig = d2i_ECDSA_SIG( NULL, &p, ( long )derLen );
   int size = ( EVP_PKEY_get_bits( key ) + 7 ) / 8;
   unsigned char *raw;
   int ok;

   if( sig == NULL )
      return 0;

   raw = ( unsigned char * )x_xgrab( ( size_t )size * 2 );
   ok = BN_bn2binpad( ECDSA_SIG_get0_r( sig ), raw, size ) == size;
   if( ok )
      ok = BN_bn2binpad( ECDSA_SIG_get0_s( sig ), raw + size, size ) == size;

   if( ok )
      s_base64( out, raw, ( size_t )size * 2 );

   x_xfree( raw );
   ECDSA_SIG_free( sig );

   return ok;
}

/* ECDSA signature: R||S of XML Signature to DER for OpenSSL; the result is freed with OPENSSL_free() */
static unsigned char *s_rawToDer( const unsigned char *raw, size_t rawLen, size_t *derLen )
{
   ECDSA_SIG *sig = ECDSA_SIG_new();
   BIGNUM *r = BN_bin2bn( raw, ( int )( rawLen / 2 ), NULL );
   BIGNUM *s = BN_bin2bn( raw + rawLen / 2, ( int )( rawLen / 2 ), NULL );
   unsigned char *der = NULL;
   int len;

   if( sig == NULL || r == NULL || s == NULL || ECDSA_SIG_set0( sig, r, s ) != 1 )
   {
      BN_free( r );
      BN_free( s );
      ECDSA_SIG_free( sig );
      return NULL;
   }

   len = i2d_ECDSA_SIG( sig, &der );
   ECDSA_SIG_free( sig );
   if( len <= 0 )
      return NULL;

   *derLen = ( size_t )len;

   return der;
}

/* Sign the canonical SignedInfo with SHA-256 and append the signature as Base64 */
static int s_signInfo( EVP_PKEY *key, const char *data, size_t len, X_XML_BUF *out )
{
   EVP_MD_CTX *mctx = EVP_MD_CTX_new();
   unsigned char *sig = NULL;
   size_t sigLen = 0;
   int ok;

   ok = mctx != NULL && EVP_DigestSignInit( mctx, NULL, EVP_sha256(), NULL, key ) == 1;
   if( ok )
      ok = EVP_DigestSign( mctx, NULL, &sigLen, ( const unsigned char * )data, len ) == 1;

   if( ok )
   {
      sig = ( unsigned char * )x_xgrab( sigLen + 1 );
      ok = EVP_DigestSign( mctx, sig, &sigLen, ( const unsigned char * )data, len ) == 1;
   }

   if( ok )
   {
      if( EVP_PKEY_get_id( key ) == EVP_PKEY_EC )
         ok = s_derToRaw( key, sig, sigLen, out );
      else
         s_base64( out, sig, sigLen );
   }

   if( sig != NULL )
      x_xfree( sig );

   EVP_MD_CTX_free( mctx );

   return ok;
}

/* Issuer name (RFC 2253) and decimal serial number of the certificate */
static int s_issuerSerial( X509 *cert, X_XML_BUF *issuer, X_XML_BUF *serial )
{
   BIO *bio = BIO_new( BIO_s_mem() );
   BIGNUM *bn = ASN1_INTEGER_to_BN( X509_get0_serialNumber( cert ), NULL );
   char *dec = IIF( bn != NULL, BN_bn2dec( bn ), NULL );
   char *text;
   long len;
   int ok = bio != NULL && dec != NULL;

   if( ok )
      ok = X509_NAME_print_ex( bio, X509_get_issuer_name( cert ), 0, XN_FLAG_RFC2253 ) >= 0;

   if( ok )
   {
      len = BIO_get_mem_data( bio, &text );
      x_xmlAppendEscaped( issuer, text, ( size_t )len, 0 );
      s_append( serial, dec );
   }

   OPENSSL_free( dec );
   BN_free( bn );
   BIO_free( bio );

   return ok;
}

/* Sign the document with an enveloped XAdES-BES signature; 0 with the error text on failure */
int x_xadesSign( X_XML_BUF *out, const char *xml, size_t len, const char *key, size_t keyLen, const char *cert,
                 size_t certLen, const char *password )
{
   X_XML_NODE *doc = NULL;
   X_XML_NODE *sigDoc = NULL;
   X_XML_NODE *root;
   X_XML_NODE *sig;
   X_XML_NODE *info;
   X_XML_NODE *props;
   X_XML_NODE *node;
   EVP_PKEY *pkey = NULL;
   X509 *x509 = NULL;
   unsigned char *der = NULL;
   int derLen;
   X_XML_BUF text = { NULL, 0, 0 };
   X_XML_BUF work = { NULL, 0, 0 };
   X_XML_BUF issuer = { NULL, 0, 0 };
   X_XML_BUF serial = { NULL, 0, 0 };
   char stamp[32];
   time_t now = time( NULL );
   struct tm utc;
   int ok = 0;

   x_xadesClearError();
   x_sslClearError();

   doc = x_xmlParse( xml, len );
   root = IIF( doc != NULL, x_xmlFirstElement( doc ), NULL );
   if( root == NULL )
   {
      x_xadesSetError( IIF( doc == NULL, x_xmlError(), "no root element" ) );
      goto done;
   }

   pkey = x_sslPrivateKey( key, keyLen, password );
   x509 = IIF( pkey != NULL, x_sslCertificate( cert, certLen ), NULL );
   if( x509 == NULL )
   {
      x_xadesSetError( IIF( x_sslLastError()[0] != '\0', x_sslLastError(), "cannot read the key or the certificate" ) );
      goto done;
   }

   if( EVP_PKEY_get_id( pkey ) != EVP_PKEY_EC && EVP_PKEY_get_id( pkey ) != EVP_PKEY_RSA )
   {
      x_xadesSetError( "only EC and RSA keys are supported" );
      goto done;
   }

   if( X509_check_private_key( x509, pkey ) != 1 )
   {
      x_xadesSetError( "the key does not match the certificate" );
      goto done;
   }

   derLen = i2d_X509( x509, &der );
   if( derLen <= 0 || !s_issuerSerial( x509, &issuer, &serial ) )
   {
      x_xadesSetError( "cannot read the certificate" );
      goto done;
   }

#if X_OS_WINDOWS
   gmtime_s( &utc, &now );
#else
   gmtime_r( &now, &utc );
#endif
   strftime( stamp, sizeof( stamp ), "%Y-%m-%dT%H:%M:%SZ", &utc );

   /* The signature with empty values; the digest of the document is taken before it is added */
   s_append( &text, "<ds:Signature xmlns:ds=\"" X_URI_DS "\" Id=\"Signature\"><ds:SignedInfo>"
                    "<ds:CanonicalizationMethod Algorithm=\"" X_URI_C14N "\"/><ds:SignatureMethod Algorithm=\"" );
   s_append( &text, IIF( EVP_PKEY_get_id( pkey ) == EVP_PKEY_EC, X_URI_ECDSA, X_URI_RSA ) );
   s_append( &text, "\"/><ds:Reference URI=\"\"><ds:Transforms>"
                    "<ds:Transform Algorithm=\"" X_URI_ENVELOPED "\"/><ds:Transform Algorithm=\"" X_URI_C14N "\"/>"
                    "</ds:Transforms><ds:DigestMethod Algorithm=\"" X_URI_SHA256 "\"/><ds:DigestValue>" );
   x_xadesCanonical( &work, doc, NULL );
   s_digest( &text, work.data, work.len );
   x_xmlBufFree( &work );
   s_append( &text, "</ds:DigestValue></ds:Reference>"
                    "<ds:Reference Type=\"" X_URI_SIGPROPS "\" URI=\"#SignedProperties\"><ds:Transforms>"
                    "<ds:Transform Algorithm=\"" X_URI_C14N "\"/></ds:Transforms>"
                    "<ds:DigestMethod Algorithm=\"" X_URI_SHA256 "\"/><ds:DigestValue>-</ds:DigestValue>"
                    "</ds:Reference></ds:SignedInfo><ds:SignatureValue>-</ds:SignatureValue>"
                    "<ds:KeyInfo><ds:X509Data><ds:X509Certificate>" );
   s_base64( &text, der, ( size_t )derLen );
   s_append( &text, "</ds:X509Certificate></ds:X509Data></ds:KeyInfo><ds:Object>"
                    "<xades:QualifyingProperties xmlns:xades=\"" X_URI_XADES "\" Target=\"#Signature\">"
                    "<xades:SignedProperties Id=\"SignedProperties\"><xades:SignedSignatureProperties>"
                    "<xades:SigningTime>" );
   s_append( &text, stamp );
   s_append( &text, "</xades:SigningTime><xades:SigningCertificate><xades:Cert><xades:CertDigest>"
                    "<ds:DigestMethod Algorithm=\"" X_URI_SHA256 "\"/><ds:DigestValue>" );
   s_digest( &text, ( const char * )der, ( size_t )derLen );
   s_append( &text, "</ds:DigestValue></xades:CertDigest><xades:IssuerSerial><ds:X509IssuerName>" );
   x_xmlAppend( &text, issuer.data, issuer.len );
   s_append( &text, "</ds:X509IssuerName><ds:X509SerialNumber>" );
   x_xmlAppend( &text, serial.data, serial.len );
   s_append( &text, "</ds:X509SerialNumber></xades:IssuerSerial></xades:Cert></xades:SigningCertificate>"
                    "</xades:SignedSignatureProperties></xades:SignedProperties></xades:QualifyingProperties>"
                    "</ds:Object></ds:Signature>" );

   sigDoc = x_xmlParse( text.data, text.len );
   if( sigDoc == NULL )
   {
      x_xadesSetError( x_xmlError() );
      goto done;
   }

   sig = x_xmlFirstElement( sigDoc );
   x_xmlUnlink( sig );
   x_xmlLink( root, sig, NULL );

   /* The digest of SignedProperties goes into the second reference */
   info = s_childNamed( sig, "SignedInfo" );
   props = s_find( sig, "SignedProperties", NULL, 0 );
   node = s_childNamed( x_xmlNextElement( s_childNamed( info, "Reference" ) ), "DigestValue" );

   x_xadesCanonical( &work, props, NULL );
   x_xmlBufFree( &text );
   s_digest( &text, work.data, work.len );
   s_setText( node, &text );
   x_xmlBufFree( &work );
   x_xmlBufFree( &text );

   x_xadesCanonical( &work, info, NULL );
   if( !s_signInfo( pkey, work.data, work.len, &text ) )
   {
      x_xadesSetError( "cannot sign" );
      goto done;
   }

   s_setText( s_childNamed( sig, "SignatureValue" ), &text );

   x_xmlWrite( out, doc, 0, "\n" );
   ok = 1;

done:
   x_xmlBufFree( &text );
   x_xmlBufFree( &work );
   x_xmlBufFree( &issuer );
   x_xmlBufFree( &serial );
   OPENSSL_free( der );
   X509_free( x509 );
   EVP_PKEY_free( pkey );
   if( sigDoc != NULL )
      x_xmlFree( sigDoc );

   if( doc != NULL )
      x_xmlFree( doc );

   return ok;
}

/* ---------------------------------------------------------------------------------------------
   Verification
   --------------------------------------------------------------------------------------------- */

/* Whether the digest of a reference matches: the document without the signature, or the element with the Id */
static int s_checkReference( X_XML_NODE *doc, X_XML_NODE *sig, X_XML_NODE *ref )
{
   X_XML_ATTR *uri = x_xmlAttrGet( ref, "URI", 3 );
   X_XML_NODE *value = s_childNamed( ref, "DigestValue" );
   X_XML_NODE *target = NULL;
   X_XML_BUF work = { NULL, 0, 0 };
   X_XML_BUF digest = { NULL, 0, 0 };
   X_XML_BUF stored = { NULL, 0, 0 };
   int ok;

   if( uri == NULL || value == NULL )
      return 0;

   if( uri->valueLen == 0 )
      x_xadesCanonical( &work, doc, sig );
   else
   {
      if( uri->value[0] == '#' )
         target = s_find( doc, NULL, uri->value + 1, uri->valueLen - 1 );

      if( target == NULL )
         return 0;

      x_xadesCanonical( &work, target, NULL );
   }

   s_digest( &digest, work.data, work.len );
   x_xmlGetText( value, &stored );
   ok = digest.len == stored.len && memcmp( digest.data, stored.data, digest.len ) == 0;
   x_xmlBufFree( &work );
   x_xmlBufFree( &digest );
   x_xmlBufFree( &stored );

   return ok;
}

/* Check the references and the signature value of a signed document; 0 with the error text */
int x_xadesVerify( const char *xml, size_t len )
{
   X_XML_NODE *doc = NULL;
   X_XML_NODE *sig;
   X_XML_NODE *info;
   X_XML_NODE *ref;
   X_XML_NODE *node;
   X509 *x509 = NULL;
   EVP_PKEY *pkey;
   EVP_MD_CTX *mctx = NULL;
   X_XML_BUF text = { NULL, 0, 0 };
   X_XML_BUF work = { NULL, 0, 0 };
   unsigned char *bytes = NULL;
   unsigned char *der = NULL;
   const unsigned char *signature;
   size_t bytesLen = 0;
   size_t derLen = 0;
   size_t signatureLen;
   const unsigned char *p;
   int ok = 0;

   x_xadesClearError();

   doc = x_xmlParse( xml, len );
   if( doc == NULL )
   {
      x_xadesSetError( x_xmlError() );
      return 0;
   }

   sig = s_find( doc, "Signature", NULL, 0 );
   info = IIF( sig != NULL, s_childNamed( sig, "SignedInfo" ), NULL );
   if( info == NULL )
   {
      x_xadesSetError( "no signature" );
      goto done;
   }

   for( ref = x_xmlFirstElement( info ); ref != NULL; ref = x_xmlNextElement( ref ) )
   {
      if( strcmp( s_localName( ref->name ), "Reference" ) == 0 && !s_checkReference( doc, sig, ref ) )
      {
         x_xadesSetError( "reference digest does not match" );
         goto done;
      }
   }

   node = s_find( sig, "X509Certificate", NULL, 0 );
   if( node != NULL )
   {
      x_xmlGetText( node, &text );
      bytes = s_unbase64( text.data, text.len, &bytesLen );
      x_xmlBufFree( &text );
   }

   p = bytes;
   x509 = IIF( bytes != NULL, d2i_X509( NULL, &p, ( long )bytesLen ), NULL );
   if( x509 == NULL )
   {
      x_xadesSetError( "no certificate" );
      goto done;
   }

   x_xfree( bytes );
   bytes = NULL;

   node = s_childNamed( sig, "SignatureValue" );
   if( node != NULL )
   {
      x_xmlGetText( node, &text );
      bytes = s_unbase64( text.data, text.len, &bytesLen );
      x_xmlBufFree( &text );
   }

   pkey = X509_get0_pubkey( x509 );
   if( bytes == NULL || pkey == NULL )
   {
      x_xadesSetError( "no signature value" );
      goto done;
   }

   if( EVP_PKEY_get_id( pkey ) == EVP_PKEY_EC )
   {
      der = s_rawToDer( bytes, bytesLen, &derLen );
      if( der == NULL )
      {
         x_xadesSetError( "bad signature value" );
         goto done;
      }
   }

   signature = IIF( der != NULL, der, bytes );
   signatureLen = IIF( der != NULL, derLen, bytesLen );
   x_xadesCanonical( &work, info, NULL );
   mctx = EVP_MD_CTX_new();
   ok = mctx != NULL && EVP_DigestVerifyInit( mctx, NULL, EVP_sha256(), NULL, pkey ) == 1;
   if( ok )
      ok = EVP_DigestVerify( mctx, signature, signatureLen, ( const unsigned char * )work.data, work.len ) == 1;

   if( !ok )
      x_xadesSetError( "signature does not match" );

done:
   x_xmlBufFree( &work );
   x_xmlBufFree( &text );
   if( bytes != NULL )
      x_xfree( bytes );

   OPENSSL_free( der );
   EVP_MD_CTX_free( mctx );
   X509_free( x509 );
   x_xmlFree( doc );

   return ok;
}
