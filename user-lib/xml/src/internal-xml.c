/*
 * Copyright (c) 2024-2026 xDev-Language
 *
 * Library...: xDev XML
 * File......: internal-xml.c
 *
 */

#include "internal-xml.h"

#include <xdev/fileio.ch>
#include <stdio.h>
#include <string.h>

/* The text being parsed and the current position */
typedef struct
{
   const char *start;
   const char *p;
   const char *end;
} X_XML_PARSER;

/* A predefined entity */
typedef struct
{
   const char *name;
   char value;
} X_XML_ENTITY;

static const char s_declaration[] = "<?xml version=\"1.0\" encoding=\"UTF-8\"?>";

static char s_error[256];

/* ---------------------------------------------------------------------------------------------
   Buffer
   --------------------------------------------------------------------------------------------- */

/* Append bytes; the buffer always ends with '\0' */
void x_xmlAppend( X_XML_BUF *buf, const char *text, size_t len )
{
   if( buf->len + len + 1 > buf->cap )
   {
      size_t cap = IIF( buf->cap == 0, 256, buf->cap );

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

/* Append text with the XML entities, for an attribute value when attr is not 0 */
void x_xmlAppendEscaped( X_XML_BUF *buf, const char *text, size_t len, int attr )
{
   size_t start = 0;
   size_t i;

   for( i = 0; i < len; i++ )
   {
      const char *entity = NULL;

      switch( text[i] )
      {
      case '&': entity = "&amp;"; break;
      case '<': entity = "&lt;"; break;
      case '>': entity = "&gt;"; break;
      case '"': entity = IIF( attr, "&quot;", NULL ); break;
      case '\n': entity = IIF( attr, "&#10;", NULL ); break;
      case '\r': entity = "&#13;"; break;
      case '\t': entity = IIF( attr, "&#9;", NULL ); break;
      }

      if( entity != NULL )
      {
         x_xmlAppend( buf, text + start, i - start );
         x_xmlAppend( buf, entity, strlen( entity ) );
         start = i + 1;
      }
   }

   x_xmlAppend( buf, text + start, len - start );
}

/* Release the data of a buffer and empty it */
void x_xmlBufFree( X_XML_BUF *buf )
{
   if( buf->data != NULL )
      x_xfree( buf->data );

   buf->data = NULL;
   buf->len = 0;
   buf->cap = 0;
}

/* ---------------------------------------------------------------------------------------------
   Nodes
   --------------------------------------------------------------------------------------------- */

/* A copy of the bytes ended with '\0', released by x_xfree() */
static char *s_strDup( const char *text, size_t len )
{
   char *copy = ( char * )x_xgrab( len + 1 );

   memcpy( copy, text, len );
   copy[len] = '\0';

   return copy;
}

/* A new empty document */
X_XML_NODE *x_xmlNew( void )
{
   X_XML_NODE *doc = ( X_XML_NODE * )x_xgrabz( sizeof( X_XML_NODE ) );

   doc->magic = X_XML_DOC_MAGIC;
   doc->type = XML_TYPE_DOCUMENT;

   return doc;
}

/* Take a node out of its parent; the node stays alive */
void x_xmlUnlink( X_XML_NODE *node )
{
   X_XML_NODE *parent = node->parent;

   if( parent == NULL )
      return;

   if( node->prev != NULL )
      node->prev->next = node->next;
   else
      parent->first = node->next;

   if( node->next != NULL )
      node->next->prev = node->prev;
   else
      parent->last = node->prev;

   node->parent = NULL;
   node->prev = NULL;
   node->next = NULL;
}

/* Unlink and release a node with all its children */
void x_xmlFree( X_XML_NODE *node )
{
   size_t i;

   while( node->first != NULL )
      x_xmlFree( node->first );

   x_xmlUnlink( node );

   for( i = 0; i < node->attrCount; i++ )
   {
      x_xfree( node->attrs[i].name );
      x_xfree( node->attrs[i].value );
   }

   if( node->attrs != NULL )
      x_xfree( node->attrs );

   if( node->name != NULL )
      x_xfree( node->name );

   x_xmlBufFree( &node->text );
   node->magic = 0;
   x_xfree( node );
}

/* The node of argument n, a document only with allowDoc; NULL for any other value */
X_XML_NODE *x_xmlParam( X_CALL_CTX *ctx, int n, int allowDoc )
{
   X_XML_NODE *node = IIF( X_ISPOINTER( ctx, n ), ( X_XML_NODE * )x_parptr( ctx, n ), NULL );

   if( node == NULL )
      return NULL;

   if( node->magic == X_XML_NODE_MAGIC || ( allowDoc && node->magic == X_XML_DOC_MAGIC ) )
      return node;

   return NULL;
}

/* Return a node as a pointer, NIL for NULL */
void x_xmlRetNode( X_CALL_CTX *ctx, X_XML_NODE *node )
{
   if( node != NULL )
      x_retptr( ctx, node );
}

/* Whether the node is an element; 0 for NULL */
int x_xmlIsElement( const X_XML_NODE *node )
{
   return node != NULL && node->type == XML_TYPE_ELEMENT;
}

/* A new node, not linked yet */
X_XML_NODE *x_xmlNodeNew( int type, const char *name, size_t nameLen, const char *text, size_t len )
{
   X_XML_NODE *node = ( X_XML_NODE * )x_xgrabz( sizeof( X_XML_NODE ) );

   node->magic = X_XML_NODE_MAGIC;
   node->type = type;

   if( name != NULL )
      node->name = s_strDup( name, nameLen );

   if( text != NULL && len > 0 )
      x_xmlAppend( &node->text, text, len );

   return node;
}

/* A new element, not linked yet */
X_XML_NODE *x_xmlElementNew( const char *name, size_t len )
{
   return x_xmlNodeNew( XML_TYPE_ELEMENT, name, len, NULL, 0 );
}

/* Link a node into parent before the child before, at the end for NULL */
void x_xmlLink( X_XML_NODE *parent, X_XML_NODE *node, X_XML_NODE *before )
{
   node->parent = parent;
   node->next = before;
   node->prev = IIF( before != NULL, before->prev, parent->last );

   if( node->prev != NULL )
      node->prev->next = node;
   else
      parent->first = node;

   if( before != NULL )
      before->prev = node;
   else
      parent->last = node;
}

/* Append a new element to parent */
X_XML_NODE *x_xmlAdd( X_XML_NODE *parent, const char *name, size_t len )
{
   X_XML_NODE *node = x_xmlElementNew( name, len );

   x_xmlLink( parent, node, NULL );

   return node;
}

/* A deep copy of a node, not linked yet */
X_XML_NODE *x_xmlClone( const X_XML_NODE *source )
{
   size_t nameLen = IIF( source->name != NULL, strlen( source->name ), 0 );
   X_XML_NODE *copy = x_xmlNodeNew( source->type, source->name, nameLen, source->text.data, source->text.len );
   const X_XML_NODE *child;
   size_t i;

   for( i = 0; i < source->attrCount; i++ )
   {
      const X_XML_ATTR *attr = &source->attrs[i];

      x_xmlAttrSet( copy, attr->name, strlen( attr->name ), attr->value, attr->valueLen );
   }

   for( child = source->first; child != NULL; child = child->next )
      x_xmlLink( copy, x_xmlClone( child ), NULL );

   return copy;
}

/* Whether other is node or lies inside it */
int x_xmlContains( const X_XML_NODE *node, const X_XML_NODE *other )
{
   for( ; other != NULL; other = other->parent )
   {
      if( other == node )
         return 1;
   }

   return 0;
}

/* Whether parent may get a child of the type; a document holds one root element (ignore does not count) */
int x_xmlCanHold( const X_XML_NODE *parent, int type, const X_XML_NODE *ignore )
{
   const X_XML_NODE *child;

   if( parent->type == XML_TYPE_ELEMENT )
      return 1;

   if( parent->type != XML_TYPE_DOCUMENT || type == XML_TYPE_TEXT || type == XML_TYPE_CDATA )
      return 0;

   if( type == XML_TYPE_ELEMENT )
   {
      for( child = parent->first; child != NULL; child = child->next )
      {
         if( child->type == XML_TYPE_ELEMENT && child != ignore )
            return 0;
      }
   }

   return 1;
}

/* Whether the byte may start an XML name */
static int s_isNameStart( unsigned char c )
{
   return ( c >= 'A' && c <= 'Z' ) || ( c >= 'a' && c <= 'z' ) || c == '_' || c == ':' || c >= 0x80;
}

/* Whether the bytes are a valid XML name */
int x_xmlValidName( const char *name, size_t len )
{
   size_t i;

   if( len == 0 || !s_isNameStart( ( unsigned char )name[0] ) )
      return 0;

   for( i = 1; i < len; i++ )
   {
      unsigned char c = ( unsigned char )name[i];

      if( !s_isNameStart( c ) && !( c >= '0' && c <= '9' ) && c != '-' && c != '.' )
         return 0;
   }

   return 1;
}

/* Namespace URI of the element name, declared on it or on an ancestor; NULL when none */
const char *x_xmlNamespace( const X_XML_NODE *node, size_t *len )
{
   static const char s_xmlUri[] = "http://www.w3.org/XML/1998/namespace";
   const char *name = node->name;
   const char *colon = strchr( name, ':' );
   size_t prefixLen = IIF( colon != NULL, ( size_t )( colon - name ), 0 );
   size_t i;

   if( prefixLen == 3 && memcmp( name, "xml", 3 ) == 0 )
   {
      *len = sizeof( s_xmlUri ) - 1;
      return s_xmlUri;
   }

   for( ; node != NULL && node->magic == X_XML_NODE_MAGIC; node = node->parent )
   {
      for( i = 0; i < node->attrCount; i++ )
      {
         const char *attr = node->attrs[i].name;
         int match;

         if( prefixLen == 0 )
            match = strcmp( attr, "xmlns" ) == 0;
         else
         {
            match = strncmp( attr, "xmlns:", 6 ) == 0 && strlen( attr + 6 ) == prefixLen;
            match = match && memcmp( attr + 6, name, prefixLen ) == 0;
         }

         if( match )
         {
            *len = node->attrs[i].valueLen;
            return node->attrs[i].value;
         }
      }
   }

   *len = 0;

   return NULL;
}

/* The next sibling element, or NULL */
X_XML_NODE *x_xmlNextElement( const X_XML_NODE *node )
{
   X_XML_NODE *next;

   for( next = node->next; next != NULL && next->type != XML_TYPE_ELEMENT; next = next->next )
      ;

   return next;
}

/* The previous sibling element, or NULL */
X_XML_NODE *x_xmlPrevElement( const X_XML_NODE *node )
{
   X_XML_NODE *prev;

   for( prev = node->prev; prev != NULL && prev->type != XML_TYPE_ELEMENT; prev = prev->prev )
      ;

   return prev;
}

/* The first child element, or NULL */
X_XML_NODE *x_xmlFirstElement( const X_XML_NODE *parent )
{
   X_XML_NODE *first = parent->first;

   return IIF( first == NULL || first->type == XML_TYPE_ELEMENT, first, x_xmlNextElement( first ) );
}

/* The last child element, or NULL */
X_XML_NODE *x_xmlLastElement( const X_XML_NODE *parent )
{
   X_XML_NODE *last = parent->last;

   return IIF( last == NULL || last->type == XML_TYPE_ELEMENT, last, x_xmlPrevElement( last ) );
}

/* Whether the node is text or CDATA */
static int s_isText( const X_XML_NODE *node )
{
   return node->type == XML_TYPE_TEXT || node->type == XML_TYPE_CDATA;
}

/* Append the text of an element (its text and CDATA children) or the content of another node */
void x_xmlGetText( const X_XML_NODE *node, X_XML_BUF *out )
{
   const X_XML_NODE *child;

   if( node->type != XML_TYPE_ELEMENT )
   {
      x_xmlAppend( out, node->text.data, node->text.len );
      return;
   }

   for( child = node->first; child != NULL; child = child->next )
   {
      if( s_isText( child ) )
         x_xmlAppend( out, child->text.data, child->text.len );
   }
}

/* Append comment text with a space after a "-" that would end the comment */
void x_xmlSafeComment( X_XML_BUF *out, const char *text, size_t len )
{
   size_t i;

   for( i = 0; i < len; i++ )
   {
      x_xmlAppend( out, text + i, 1 );

      if( text[i] == '-' && ( i + 1 == len || text[i + 1] == '-' ) )
         x_xmlAppend( out, " ", 1 );
   }
}

/* Append instruction data with a space inside "?>" */
void x_xmlSafeInstruction( X_XML_BUF *out, const char *text, size_t len )
{
   size_t i;

   for( i = 0; i < len; i++ )
   {
      x_xmlAppend( out, text + i, 1 );

      if( text[i] == '?' && i + 1 < len && text[i + 1] == '>' )
         x_xmlAppend( out, " ", 1 );
   }
}

/* Replace the text of an element (its text and CDATA children) or the content of another node */
void x_xmlSetText( X_XML_NODE *node, const char *text, size_t len )
{
   X_XML_NODE *child;
   X_XML_NODE *next;

   if( node->type != XML_TYPE_ELEMENT )
   {
      x_xmlBufFree( &node->text );

      if( text != NULL && len > 0 )
      {
         if( node->type == XML_TYPE_COMMENT )
            x_xmlSafeComment( &node->text, text, len );
         else if( node->type == XML_TYPE_PI )
            x_xmlSafeInstruction( &node->text, text, len );
         else
            x_xmlAppend( &node->text, text, len );
      }
      return;
   }

   for( child = node->first; child != NULL; child = next )
   {
      next = child->next;

      if( s_isText( child ) )
         x_xmlFree( child );
   }

   if( text != NULL && len > 0 )
      x_xmlLink( node, x_xmlNodeNew( XML_TYPE_TEXT, NULL, 0, text, len ), node->first );
}

/* Index of the attribute, attrCount when it does not exist */
static size_t s_attrIndex( const X_XML_NODE *node, const char *name, size_t nameLen )
{
   size_t i;

   for( i = 0; i < node->attrCount; i++ )
   {
      if( strlen( node->attrs[i].name ) == nameLen && memcmp( node->attrs[i].name, name, nameLen ) == 0 )
         break;
   }

   return i;
}

/* The attribute with the name, or NULL */
X_XML_ATTR *x_xmlAttrGet( const X_XML_NODE *node, const char *name, size_t nameLen )
{
   size_t i = s_attrIndex( node, name, nameLen );

   return IIF( i < node->attrCount, &node->attrs[i], NULL );
}

/* Set an attribute; a NULL value removes it */
void x_xmlAttrSet( X_XML_NODE *node, const char *name, size_t nameLen, const char *value, size_t len )
{
   size_t i = s_attrIndex( node, name, nameLen );

   if( value == NULL )
   {
      if( i < node->attrCount )
      {
         x_xfree( node->attrs[i].name );
         x_xfree( node->attrs[i].value );
         memmove( &node->attrs[i], &node->attrs[i + 1], ( node->attrCount - i - 1 ) * sizeof( X_XML_ATTR ) );
         node->attrCount--;
      }
      return;
   }

   if( i == node->attrCount )
   {
      if( node->attrCount == node->attrCap )
      {
         node->attrCap = IIF( node->attrCap == 0, 4, node->attrCap * 2 );
         node->attrs = ( X_XML_ATTR * )x_xreallocArray( node->attrs, node->attrCap, sizeof( X_XML_ATTR ) );
      }

      node->attrs[i].name = s_strDup( name, nameLen );
      node->attrCount++;
   }
   else
      x_xfree( node->attrs[i].value );

   node->attrs[i].value = s_strDup( value, len );
   node->attrs[i].valueLen = len;
}

/* The index-th child element with the name (from 1), or NULL */
X_XML_NODE *x_xmlChildAt( const X_XML_NODE *node, const char *name, size_t len, size_t index )
{
   X_XML_NODE *child;

   for( child = node->first; child != NULL; child = child->next )
   {
      if( child->type != XML_TYPE_ELEMENT || strlen( child->name ) != len || memcmp( child->name, name, len ) != 0 )
         continue;

      index--;
      if( index == 0 )
         return child;
   }

   return NULL;
}

/* The first child element with the name, or NULL */
X_XML_NODE *x_xmlChild( const X_XML_NODE *node, const char *name, size_t len )
{
   return x_xmlChildAt( node, name, len, 1 );
}

/* The child for one path segment "name" or "name[n]" */
static X_XML_NODE *s_findSegment( const X_XML_NODE *node, const char *segment, size_t len )
{
   const char *open = ( const char * )memchr( segment, '[', len );
   size_t index = 0;
   const char *p;

   if( open == NULL )
      return x_xmlChildAt( node, segment, len, 1 );

   if( segment[len - 1] != ']' || open + 2 >= segment + len )
      return NULL;

   for( p = open + 1; p < segment + len - 1; p++ )
   {
      if( *p < '0' || *p > '9' )
         return NULL;

      index = index * 10 + ( size_t )( *p - '0' );
   }

   return IIF( index > 0, x_xmlChildAt( node, segment, ( size_t )( open - segment ), index ), NULL );
}

/* The element at a path of names separated by "/", or NULL */
X_XML_NODE *x_xmlFind( X_XML_NODE *node, const char *path )
{
   while( node != NULL && *path != '\0' )
   {
      const char *slash = strchr( path, '/' );
      size_t len = IIF( slash != NULL, ( size_t )( slash - path ), strlen( path ) );

      if( len > 0 )
         node = s_findSegment( node, path, len );

      path += len;
      if( *path == '/' )
         path++;
   }

   return node;
}

/* ---------------------------------------------------------------------------------------------
   Parser
   --------------------------------------------------------------------------------------------- */

/* The text of the last parse or file error, "" when none */
const char *x_xmlError( void )
{
   return s_error;
}

/* Keep a parse error with the line of pos; returns 0 */
static int s_fail( const X_XML_PARSER *ps, const char *pos, const char *message )
{
   const char *p;
   int line = 1;

   for( p = ps->start; p < pos; p++ )
   {
      if( *p == '\n' )
         line++;
   }

   snprintf( s_error, sizeof( s_error ), "line %d: %s", line, message );

   return 0;
}

/* Whether the byte is XML white space */
static int s_isSpace( char c )
{
   return c == ' ' || c == '\t' || c == '\r' || c == '\n';
}

/* Whether the byte ends a name */
static int s_isNameEnd( char c )
{
   return s_isSpace( c ) || c == '/' || c == '>' || c == '<' || c == '=' || c == '"' || c == '\'' || c == '?';
}

/* Skip white space */
static void s_skipSpace( X_XML_PARSER *ps )
{
   while( ps->p < ps->end && s_isSpace( *ps->p ) )
      ps->p++;
}

/* Skip a name and return its length */
static size_t s_name( X_XML_PARSER *ps )
{
   const char *start = ps->p;

   while( ps->p < ps->end && !s_isNameEnd( *ps->p ) )
      ps->p++;

   return ( size_t )( ps->p - start );
}

/* Whether the text follows at the current position */
static int s_startsWith( const X_XML_PARSER *ps, const char *text )
{
   size_t len = strlen( text );

   return ( size_t )( ps->end - ps->p ) >= len && memcmp( ps->p, text, len ) == 0;
}

/* The next place of the text from the current position, or NULL */
static const char *s_search( const X_XML_PARSER *ps, const char *text )
{
   size_t len = strlen( text );
   const char *p;

   for( p = ps->p; p + len <= ps->end; p++ )
   {
      if( memcmp( p, text, len ) == 0 )
         return p;
   }

   return NULL;
}

/* Append a code point as UTF-8 */
static void s_appendCodepoint( X_XML_BUF *buf, unsigned long cp )
{
   char utf8[4];
   size_t len;

   if( cp < 0x80 )
   {
      utf8[0] = ( char )cp;
      len = 1;
   }
   else if( cp < 0x800 )
   {
      utf8[0] = ( char )( 0xC0 | ( cp >> 6 ) );
      utf8[1] = ( char )( 0x80 | ( cp & 0x3F ) );
      len = 2;
   }
   else if( cp < 0x10000 )
   {
      utf8[0] = ( char )( 0xE0 | ( cp >> 12 ) );
      utf8[1] = ( char )( 0x80 | ( ( cp >> 6 ) & 0x3F ) );
      utf8[2] = ( char )( 0x80 | ( cp & 0x3F ) );
      len = 3;
   }
   else
   {
      utf8[0] = ( char )( 0xF0 | ( cp >> 18 ) );
      utf8[1] = ( char )( 0x80 | ( ( cp >> 12 ) & 0x3F ) );
      utf8[2] = ( char )( 0x80 | ( ( cp >> 6 ) & 0x3F ) );
      utf8[3] = ( char )( 0x80 | ( cp & 0x3F ) );
      len = 4;
   }

   x_xmlAppend( buf, utf8, len );
}

/* Append the value of an entity or a character reference; 0 when it is not valid */
static int s_entity( const char *name, size_t len, X_XML_BUF *out )
{
   static const X_XML_ENTITY s_entities[] =
   {
      { "amp", '&' }, { "lt", '<' }, { "gt", '>' }, { "quot", '"' }, { "apos", '\'' }
   };
   unsigned long cp = 0;
   size_t i;

   for( i = 0; i < sizeof( s_entities ) / sizeof( s_entities[0] ); i++ )
   {
      if( strlen( s_entities[i].name ) == len && memcmp( s_entities[i].name, name, len ) == 0 )
      {
         x_xmlAppend( out, &s_entities[i].value, 1 );
         return 1;
      }
   }

   if( len < 2 || name[0] != '#' )
      return 0;

   if( name[1] == 'x' || name[1] == 'X' )
   {
      if( len < 3 )
         return 0;

      for( i = 2; i < len; i++ )
      {
         char c = name[i];

         if( c >= '0' && c <= '9' )
            cp = cp * 16 + ( unsigned long )( c - '0' );
         else if( c >= 'a' && c <= 'f' )
            cp = cp * 16 + ( unsigned long )( c - 'a' + 10 );
         else if( c >= 'A' && c <= 'F' )
            cp = cp * 16 + ( unsigned long )( c - 'A' + 10 );
         else
            return 0;

         if( cp > 0x10FFFF )
            return 0;
      }
   }
   else
   {
      for( i = 1; i < len; i++ )
      {
         if( name[i] < '0' || name[i] > '9' )
            return 0;

         cp = cp * 10 + ( unsigned long )( name[i] - '0' );

         if( cp > 0x10FFFF )
            return 0;
      }
   }

   if( cp == 0 || ( cp >= 0xD800 && cp <= 0xDFFF ) )
      return 0;

   s_appendCodepoint( out, cp );

   return 1;
}

/* Text with the entity references replaced; 0 with the error on a bad reference */
static int s_decode( const X_XML_PARSER *ps, const char *text, size_t len, X_XML_BUF *out )
{
   size_t start = 0;
   size_t i = 0;

   out->len = 0;

   while( i < len )
   {
      if( text[i] == '&' )
      {
         size_t semi = i + 1;

         while( semi < len && semi - i <= 12 && text[semi] != ';' )
            semi++;

         x_xmlAppend( out, text + start, i - start );

         if( semi >= len || text[semi] != ';' || !s_entity( text + i + 1, semi - i - 1, out ) )
            return s_fail( ps, text + i, "invalid entity reference" );

         i = semi + 1;
         start = i;
      }
      else
         i++;
   }

   x_xmlAppend( out, text + start, len - start );

   return 1;
}

/* Text up to the next markup; white space alone is left out */
static int s_text( X_XML_PARSER *ps, X_XML_NODE *cur, X_XML_BUF *tmp )
{
   const char *start = ps->p;
   const char *p;

   while( ps->p < ps->end && *ps->p != '<' )
      ps->p++;

   for( p = start; p < ps->p && s_isSpace( *p ); p++ )
      ;

   if( p == ps->p )
      return 1;

   if( cur->type == XML_TYPE_DOCUMENT )
      return s_fail( ps, p, "text outside the root element" );

   for( p = start; p + 2 < ps->p; p++ )
   {
      if( p[0] == ']' && p[1] == ']' && p[2] == '>' )
         return s_fail( ps, p, "']]>' in text" );
   }

   if( !s_decode( ps, start, ( size_t )( ps->p - start ), tmp ) )
      return 0;

   if( cur->last != NULL && cur->last->type == XML_TYPE_TEXT )
      x_xmlAppend( &cur->last->text, tmp->data, tmp->len );
   else
      x_xmlLink( cur, x_xmlNodeNew( XML_TYPE_TEXT, NULL, 0, tmp->data, tmp->len ), NULL );

   return 1;
}

/* Skip a <!DOCTYPE ...> declaration with its internal subset */
static int s_skipDeclaration( X_XML_PARSER *ps )
{
   const char *start = ps->p;
   int depth = 0;

   ps->p += 2;

   while( ps->p < ps->end )
   {
      char c = *ps->p++;

      if( c == '[' )
         depth++;
      else if( c == ']' )
         depth--;
      else if( c == '>' && depth <= 0 )
         return 1;
   }

   return s_fail( ps, start, "unterminated declaration" );
}

/* One attribute name="value" of a start tag */
static int s_attribute( X_XML_PARSER *ps, X_XML_NODE *node, X_XML_BUF *tmp )
{
   const char *name = ps->p;
   size_t nameLen = s_name( ps );
   const char *value;
   char quote;

   if( !x_xmlValidName( name, nameLen ) )
      return s_fail( ps, name, "invalid attribute name" );

   if( x_xmlAttrGet( node, name, nameLen ) != NULL )
      return s_fail( ps, name, "duplicate attribute" );

   s_skipSpace( ps );
   if( ps->p >= ps->end || *ps->p != '=' )
      return s_fail( ps, ps->p, "expected '=' after attribute name" );

   ps->p++;
   s_skipSpace( ps );
   if( ps->p >= ps->end || ( *ps->p != '"' && *ps->p != '\'' ) )
      return s_fail( ps, ps->p, "expected quoted attribute value" );

   quote = *ps->p++;
   value = ps->p;

   while( ps->p < ps->end && *ps->p != quote )
   {
      if( *ps->p == '<' )
         return s_fail( ps, ps->p, "'<' in attribute value" );

      ps->p++;
   }

   if( ps->p >= ps->end )
      return s_fail( ps, value, "unterminated attribute value" );

   if( !s_decode( ps, value, ( size_t )( ps->p - value ), tmp ) )
      return 0;

   ps->p++;
   x_xmlAttrSet( node, name, nameLen, tmp->data, tmp->len );

   return 1;
}

/* A start tag or an empty element; cur moves into an open element */
static int s_startTag( X_XML_PARSER *ps, X_XML_NODE **cur, X_XML_BUF *tmp )
{
   const char *name;
   size_t nameLen;
   X_XML_NODE *node;

   ps->p++;
   name = ps->p;
   nameLen = s_name( ps );

   if( !x_xmlValidName( name, nameLen ) )
      return s_fail( ps, name, "invalid element name" );

   if( !x_xmlCanHold( *cur, XML_TYPE_ELEMENT, NULL ) )
      return s_fail( ps, name, "more than one root element" );

   node = x_xmlAdd( *cur, name, nameLen );

   for( ;; )
   {
      s_skipSpace( ps );

      if( ps->p >= ps->end )
         return s_fail( ps, name, "unterminated start tag" );

      if( *ps->p == '>' )
      {
         ps->p++;
         *cur = node;
         return 1;
      }

      if( *ps->p == '/' )
      {
         if( ps->p + 1 < ps->end && ps->p[1] == '>' )
         {
            ps->p += 2;
            return 1;
         }
         return s_fail( ps, ps->p, "expected '>' after '/'" );
      }

      if( !s_attribute( ps, node, tmp ) )
         return 0;
   }
}

/* An end tag that closes the current element */
static int s_endTag( X_XML_PARSER *ps, X_XML_NODE **cur )
{
   const char *start = ps->p;
   const char *name;
   size_t nameLen;
   char message[128];

   ps->p += 2;
   name = ps->p;
   nameLen = s_name( ps );

   if( ( *cur )->type == XML_TYPE_DOCUMENT )
      return s_fail( ps, start, "closing tag without an open element" );

   if( strlen( ( *cur )->name ) != nameLen || memcmp( ( *cur )->name, name, nameLen ) != 0 )
   {
      snprintf( message, sizeof( message ), "expected </%.64s>", ( *cur )->name );
      return s_fail( ps, start, message );
   }

   s_skipSpace( ps );
   if( ps->p >= ps->end || *ps->p != '>' )
      return s_fail( ps, ps->p, "expected '>' in closing tag" );

   ps->p++;
   *cur = ( *cur )->parent;

   return 1;
}

/* Whether the text equals the upper case word, ignoring the case of the text */
static int s_equalsUpper( const char *text, size_t len, const char *word )
{
   size_t i;

   if( strlen( word ) != len )
      return 0;

   for( i = 0; i < len; i++ )
   {
      char c = text[i];

      if( c >= 'a' && c <= 'z' )
         c = ( char )( c - 'a' + 'A' );

      if( c != word[i] )
         return 0;
   }

   return 1;
}

/* The encoding of the XML declaration must be UTF-8 or US-ASCII */
static int s_checkEncoding( X_XML_PARSER *ps, const char *end )
{
   const char *p;

   for( p = ps->p; p + 8 <= end; p++ )
   {
      if( memcmp( p, "encoding", 8 ) == 0 )
         break;
   }

   if( p + 8 > end )
      return 1;

   p += 8;
   while( p < end && ( s_isSpace( *p ) || *p == '=' ) )
      p++;

   if( p < end && ( *p == '"' || *p == '\'' ) )
   {
      char quote = *p++;
      const char *value = p;
      size_t len;
      int utf8;

      while( p < end && *p != quote )
         p++;

      len = ( size_t )( p - value );

      utf8 = s_equalsUpper( value, len, "UTF-8" ) || s_equalsUpper( value, len, "UTF8" );
      if( !utf8 && !s_equalsUpper( value, len, "US-ASCII" ) )
         return s_fail( ps, value, "unsupported encoding, only UTF-8 is accepted" );
   }

   return 1;
}

/* A processing instruction, or the XML declaration */
static int s_instruction( X_XML_PARSER *ps, X_XML_NODE *cur )
{
   const char *start = ps->p;
   const char *end = s_search( ps, "?>" );
   const char *target;
   size_t targetLen;
   const char *data;

   if( end == NULL )
      return s_fail( ps, start, "unterminated processing instruction" );

   ps->p += 2;
   target = ps->p;
   targetLen = s_name( ps );

   if( !x_xmlValidName( target, targetLen ) )
      return s_fail( ps, target, "invalid processing instruction target" );

   if( s_equalsUpper( target, targetLen, "XML" ) )
   {
      if( start != ps->start && !( ps->start + 3 == start && memcmp( ps->start, "\xEF\xBB\xBF", 3 ) == 0 ) )
         return s_fail( ps, start, "XML declaration not at the start of the document" );

      if( !s_checkEncoding( ps, end ) )
         return 0;

      ps->p = end + 2;
      return 1;
   }

   s_skipSpace( ps );
   data = IIF( ps->p < end, ps->p, end );
   ps->p = end + 2;

   x_xmlLink( cur, x_xmlNodeNew( XML_TYPE_PI, target, targetLen, data, ( size_t )( end - data ) ), NULL );

   return 1;
}

/* Markup at the current "<": comment, CDATA, instruction, declaration or tag */
static int s_markup( X_XML_PARSER *ps, X_XML_NODE **cur, X_XML_BUF *tmp )
{
   if( s_startsWith( ps, "<!--" ) )
   {
      const char *start = ps->p;
      const char *found;

      ps->p += 4;
      found = s_search( ps, "-->" );
      if( found == NULL )
         return s_fail( ps, start, "unterminated comment" );

      x_xmlLink( *cur, x_xmlNodeNew( XML_TYPE_COMMENT, NULL, 0, ps->p, ( size_t )( found - ps->p ) ), NULL );
      ps->p = found + 3;
      return 1;
   }

   if( s_startsWith( ps, "<![CDATA[" ) )
   {
      const char *start = ps->p;
      const char *found;

      if( ( *cur )->type == XML_TYPE_DOCUMENT )
         return s_fail( ps, start, "CDATA outside the root element" );

      ps->p += 9;
      found = s_search( ps, "]]>" );
      if( found == NULL )
         return s_fail( ps, start, "unterminated CDATA section" );

      x_xmlLink( *cur, x_xmlNodeNew( XML_TYPE_CDATA, NULL, 0, ps->p, ( size_t )( found - ps->p ) ), NULL );
      ps->p = found + 3;
      return 1;
   }

   if( s_startsWith( ps, "<?" ) )
      return s_instruction( ps, *cur );

   if( s_startsWith( ps, "<!" ) )
   {
      if( ( *cur )->type != XML_TYPE_DOCUMENT )
         return s_fail( ps, ps->p, "unexpected declaration inside an element" );

      return s_skipDeclaration( ps );
   }

   if( s_startsWith( ps, "</" ) )
      return s_endTag( ps, cur );

   return s_startTag( ps, cur, tmp );
}

/* Parse a UTF-8 text; NULL with the error text on failure */
X_XML_NODE *x_xmlParse( const char *text, size_t len )
{
   X_XML_PARSER ps;
   X_XML_NODE *doc = x_xmlNew();
   X_XML_NODE *cur = doc;
   X_XML_BUF tmp = { NULL, 0, 0 };
   int ok = 1;

   s_error[0] = '\0';

   ps.start = text;
   ps.p = text;
   ps.end = text + len;

   if( len >= 3 && memcmp( text, "\xEF\xBB\xBF", 3 ) == 0 )
      ps.p += 3;

   while( ok && ps.p < ps.end )
   {
      if( *ps.p == '<' )
         ok = s_markup( &ps, &cur, &tmp );
      else
         ok = s_text( &ps, cur, &tmp );
   }

   if( ok && cur != doc )
      ok = s_fail( &ps, ps.end, "unexpected end of document" );

   if( ok && x_xmlFirstElement( doc ) == NULL )
      ok = s_fail( &ps, ps.end, "missing root element" );

   x_xmlBufFree( &tmp );

   if( !ok )
   {
      x_xmlFree( doc );
      return NULL;
   }

   return doc;
}

/* Load and parse a file; NULL with the error text on failure */
X_XML_NODE *x_xmlLoad( const char *path )
{
   int64_t handle = x_fsOpen( path, FO_READ | FO_DENYNONE );
   int64_t size;
   char *data;
   X_XML_NODE *doc;

   if( handle == F_ERROR )
   {
      snprintf( s_error, sizeof( s_error ), "cannot open file" );
      return NULL;
   }

   size = x_fsSeek( handle, 0, FS_END );
   x_fsSeek( handle, 0, FS_SET );

   if( size < 0 )
   {
      x_fsClose( handle );
      snprintf( s_error, sizeof( s_error ), "cannot read file" );
      return NULL;
   }

   data = ( char * )x_xgrab( ( size_t )size + 1 );

   if( x_fsRead( handle, data, ( size_t )size ) != ( size_t )size )
   {
      x_fsClose( handle );
      x_xfree( data );
      snprintf( s_error, sizeof( s_error ), "cannot read file" );
      return NULL;
   }

   x_fsClose( handle );
   doc = x_xmlParse( data, ( size_t )size );
   x_xfree( data );

   return doc;
}

/* ---------------------------------------------------------------------------------------------
   Writer
   --------------------------------------------------------------------------------------------- */

/* Two spaces for each level when indenting */
static void s_writeIndent( X_XML_BUF *buf, int indent, size_t depth )
{
   size_t i;

   if( indent )
   {
      for( i = 0; i < depth; i++ )
         x_xmlAppend( buf, "  ", 2 );
   }
}

/* A CDATA section; "]]>" in the text splits it into two */
static void s_writeCData( X_XML_BUF *buf, const char *text, size_t len )
{
   size_t start = 0;
   size_t i;

   x_xmlAppend( buf, "<![CDATA[", 9 );

   for( i = 0; i + 2 < len; i++ )
   {
      if( text[i] == ']' && text[i + 1] == ']' && text[i + 2] == '>' )
      {
         x_xmlAppend( buf, text + start, i + 2 - start );
         x_xmlAppend( buf, "]]><![CDATA[", 12 );
         start = i + 2;
      }
   }

   x_xmlAppend( buf, text + start, len - start );
   x_xmlAppend( buf, "]]>", 3 );
}

/* Whether the element has a text or CDATA child (it is then written without indenting) */
static int s_hasText( const X_XML_NODE *node )
{
   const X_XML_NODE *child;

   for( child = node->first; child != NULL; child = child->next )
   {
      if( s_isText( child ) )
         return 1;
   }

   return 0;
}

/* Write a node with its children */
static void s_writeNode( X_XML_BUF *buf, const X_XML_NODE *node, int indent, size_t depth, const char *eol )
{
   const X_XML_NODE *child;
   size_t i;
   int inner;

   switch( node->type )
   {
   case XML_TYPE_TEXT:
      x_xmlAppendEscaped( buf, node->text.data, node->text.len, 0 );
      return;

   case XML_TYPE_CDATA:
      s_writeCData( buf, node->text.data, node->text.len );
      return;

   case XML_TYPE_COMMENT:
      s_writeIndent( buf, indent, depth );
      x_xmlAppend( buf, "<!--", 4 );
      x_xmlAppend( buf, node->text.data, node->text.len );
      x_xmlAppend( buf, "-->", 3 );
      break;

   case XML_TYPE_PI:
      s_writeIndent( buf, indent, depth );
      x_xmlAppend( buf, "<?", 2 );
      x_xmlAppend( buf, node->name, strlen( node->name ) );
      if( node->text.len > 0 )
      {
         x_xmlAppend( buf, " ", 1 );
         x_xmlAppend( buf, node->text.data, node->text.len );
      }

      x_xmlAppend( buf, "?>", 2 );
      break;

   default:
      s_writeIndent( buf, indent, depth );
      x_xmlAppend( buf, "<", 1 );
      x_xmlAppend( buf, node->name, strlen( node->name ) );

      for( i = 0; i < node->attrCount; i++ )
      {
         x_xmlAppend( buf, " ", 1 );
         x_xmlAppend( buf, node->attrs[i].name, strlen( node->attrs[i].name ) );
         x_xmlAppend( buf, "=\"", 2 );
         x_xmlAppendEscaped( buf, node->attrs[i].value, node->attrs[i].valueLen, 1 );
         x_xmlAppend( buf, "\"", 1 );
      }

      if( node->first == NULL )
         x_xmlAppend( buf, "/>", 2 );
      else
      {
         inner = indent && !s_hasText( node );

         x_xmlAppend( buf, ">", 1 );
         if( inner )
            x_xmlAppend( buf, eol, strlen( eol ) );

         for( child = node->first; child != NULL; child = child->next )
            s_writeNode( buf, child, inner, depth + 1, eol );

         s_writeIndent( buf, inner, depth );
         x_xmlAppend( buf, "</", 2 );
         x_xmlAppend( buf, node->name, strlen( node->name ) );
         x_xmlAppend( buf, ">", 1 );
      }
      break;
   }

   if( indent )
      x_xmlAppend( buf, eol, strlen( eol ) );
}

/* Write a document (with the XML declaration) or a single node */
void x_xmlWrite( X_XML_BUF *buf, const X_XML_NODE *node, int indent, const char *eol )
{
   const X_XML_NODE *child;

   if( node->type == XML_TYPE_DOCUMENT )
   {
      x_xmlAppend( buf, s_declaration, sizeof( s_declaration ) - 1 );
      x_xmlAppend( buf, eol, strlen( eol ) );

      for( child = node->first; child != NULL; child = child->next )
         s_writeNode( buf, child, indent, 0, eol );
   }
   else
      s_writeNode( buf, node, indent, 0, eol );

   if( buf->data == NULL )
      x_xmlAppend( buf, "", 0 );
}

/* Write a document into a file; 0 with the error text on failure */
int x_xmlSave( const X_XML_NODE *node, const char *path, int indent, const char *eol )
{
   X_XML_BUF buf = { NULL, 0, 0 };
   int64_t handle;
   int ok;

   x_xmlWrite( &buf, node, indent, eol );

   handle = x_fsCreate( path, FC_NORMAL );
   if( handle == F_ERROR )
   {
      x_xmlBufFree( &buf );
      snprintf( s_error, sizeof( s_error ), "cannot create file" );
      return 0;
   }

   ok = x_fsWrite( handle, buf.data, buf.len ) == buf.len;
   x_fsClose( handle );
   x_xmlBufFree( &buf );

   if( !ok )
      snprintf( s_error, sizeof( s_error ), "cannot write file" );

   return ok;
}
