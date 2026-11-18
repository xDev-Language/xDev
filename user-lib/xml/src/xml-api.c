/*
 * Copyright (c) 2024-2026 xDev-Language
 *
 * Library: xDev XML
 * File: xml-api.c
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "internal-xml.h"

#include <string.h>

/* ---------------------------------------------------------------------------------------------
   Document
   --------------------------------------------------------------------------------------------- */

// Create an empty document
// xml_New( ) --> pDoc
X_FUNC( XML_NEW )
{
   x_retptr( ctx, x_xmlNew() );
}

// Parse a UTF-8 string; returns NIL on error (see xml_Error())
// xml_Parse( cXml ) --> pDoc | NIL
X_FUNC( XML_PARSE )
{
   x_xmlRetNode( ctx, IIF( X_ISCHAR( ctx, 1 ), x_xmlParse( x_parc( ctx, 1 ), x_parclen( ctx, 1 ) ), NULL ) );
}

// Load and parse a file; returns NIL on error (see xml_Error())
// xml_Load( cFile ) --> pDoc | NIL
X_FUNC( XML_LOAD )
{
   x_xmlRetNode( ctx, IIF( X_ISCHAR( ctx, 1 ), x_xmlLoad( x_parc( ctx, 1 ) ), NULL ) );
}

// Serialize a document (with declaration) or a single node (indented by default, lines end with cEol, LF by default)
// xml_ToString( pDoc | pNode, [lIndent], [cEol] ) --> cXml
X_FUNC( XML_TOSTRING )
{
   X_XML_NODE *node = x_xmlParam( ctx, 1, 1 );
   int indent = IIF( X_ISLOG( ctx, 2 ), x_parl( ctx, 2 ), 1 );
   const char *eol = IIF( X_ISCHAR( ctx, 3 ), x_parc( ctx, 3 ), "\n" );
   X_XML_BUF buf = { NULL, 0, 0 };

   if( node != NULL )
   {
      x_xmlWrite( &buf, node, indent, eol );
      x_retclen_buffer( ctx, buf.data, buf.len );
   }
   else
      x_retc( ctx, "" );
}

// Save a document to a file (indented by default, lines end with cEol, LF by default)
// xml_Save( pDoc, cFile, [lIndent], [cEol] ) --> lSuccess
X_FUNC( XML_SAVE )
{
   X_XML_NODE *node = x_xmlParam( ctx, 1, 1 );
   int indent = IIF( X_ISLOG( ctx, 3 ), x_parl( ctx, 3 ), 1 );
   const char *eol = IIF( X_ISCHAR( ctx, 4 ), x_parc( ctx, 4 ), "\n" );
   int ok = node != NULL && X_ISCHAR( ctx, 2 );

   if( ok )
      ok = x_xmlSave( node, x_parc( ctx, 2 ), indent, eol );

   x_retl( ctx, ok );
}

// Release a document with all its nodes
// xml_Free( pDoc ) --> NIL
X_FUNC( XML_FREE )
{
   X_XML_NODE *doc = x_xmlParam( ctx, 1, 1 );

   if( doc != NULL && doc->type == XML_TYPE_DOCUMENT )
      x_xmlFree( doc );
}

// Description of the last parse or file error, empty when none
// xml_Error( ) --> cMessage
X_FUNC( XML_ERROR )
{
   x_retc( ctx, x_xmlError() );
}

/* ---------------------------------------------------------------------------------------------
   Navigation by elements
   --------------------------------------------------------------------------------------------- */

// Root element of a document
// xml_Root( pDoc ) --> pNode | NIL
X_FUNC( XML_ROOT )
{
   X_XML_NODE *doc = x_xmlParam( ctx, 1, 1 );

   if( doc != NULL && doc->type == XML_TYPE_DOCUMENT )
      x_xmlRetNode( ctx, x_xmlFirstElement( doc ) );
}

// Parent element; NIL for nodes directly in the document
// xml_Parent( pNode ) --> pNode | NIL
X_FUNC( XML_PARENT )
{
   X_XML_NODE *node = x_xmlParam( ctx, 1, 0 );

   if( node != NULL && x_xmlIsElement( node->parent ) )
      x_retptr( ctx, node->parent );
}

// First child element
// xml_First( pDoc | pNode ) --> pNode | NIL
X_FUNC( XML_FIRST )
{
   X_XML_NODE *node = x_xmlParam( ctx, 1, 1 );

   if( node != NULL )
      x_xmlRetNode( ctx, x_xmlFirstElement( node ) );
}

// Last child element
// xml_Last( pDoc | pNode ) --> pNode | NIL
X_FUNC( XML_LAST )
{
   X_XML_NODE *node = x_xmlParam( ctx, 1, 1 );

   if( node != NULL )
      x_xmlRetNode( ctx, x_xmlLastElement( node ) );
}

// Next sibling element
// xml_Next( pNode ) --> pNode | NIL
X_FUNC( XML_NEXT )
{
   X_XML_NODE *node = x_xmlParam( ctx, 1, 0 );

   if( node != NULL )
      x_xmlRetNode( ctx, x_xmlNextElement( node ) );
}

// Previous sibling element
// xml_Prev( pNode ) --> pNode | NIL
X_FUNC( XML_PREV )
{
   X_XML_NODE *node = x_xmlParam( ctx, 1, 0 );

   if( node != NULL )
      x_xmlRetNode( ctx, x_xmlPrevElement( node ) );
}

// Child elements, all or only those with the given name
// xml_Children( pDoc | pNode, [cName] ) --> { pNode, ... }
X_FUNC( XML_CHILDREN )
{
   X_XML_NODE *node = x_xmlParam( ctx, 1, 1 );
   int named = X_ISCHAR( ctx, 2 );
   const char *name = IIF( named, x_parc( ctx, 2 ), NULL );
   size_t len = IIF( named, x_parclen( ctx, 2 ), 0 );
   size_t count = 0;
   X_XML_NODE *child;
   X_ITEM *array;

   for( child = IIF( node != NULL, node->first, NULL ); child != NULL; child = child->next )
   {
      if( child->type != XML_TYPE_ELEMENT )
         continue;

      if( !named || ( strlen( child->name ) == len && memcmp( child->name, name, len ) == 0 ) )
         count++;
   }

   array = x_itemArrayNew( count );
   count = 0;

   for( child = IIF( node != NULL, node->first, NULL ); child != NULL; child = child->next )
   {
      if( child->type != XML_TYPE_ELEMENT )
         continue;

      if( !named || ( strlen( child->name ) == len && memcmp( child->name, name, len ) == 0 ) )
         x_arraySetPtr( array, ++count, child );
   }

   x_itemReturn( ctx, array );
   x_itemRelease( array );
}

// First child element with the given name
// xml_Child( pDoc | pNode, cName ) --> pNode | NIL
X_FUNC( XML_CHILD )
{
   X_XML_NODE *node = x_xmlParam( ctx, 1, 1 );

   if( node != NULL && X_ISCHAR( ctx, 2 ) )
      x_xmlRetNode( ctx, x_xmlChild( node, x_parc( ctx, 2 ), x_parclen( ctx, 2 ) ) );
}

// Element at a path of names separated by "/"; "name[n]" selects the n-th one, e.g. "library/book[2]/title"
// xml_Find( pDoc | pNode, cPath ) --> pNode | NIL
X_FUNC( XML_FIND )
{
   X_XML_NODE *node = x_xmlParam( ctx, 1, 1 );
   X_XML_NODE *found = IIF( node != NULL && X_ISCHAR( ctx, 2 ), x_xmlFind( node, x_parc( ctx, 2 ) ), NULL );

   if( found != node )
      x_xmlRetNode( ctx, found );
}

/* ---------------------------------------------------------------------------------------------
   Navigation by all nodes
   --------------------------------------------------------------------------------------------- */

// Node type: XML_TYPE_ELEMENT, XML_TYPE_TEXT, XML_TYPE_CDATA, XML_TYPE_COMMENT, XML_TYPE_PI or XML_TYPE_DOCUMENT
// xml_Type( pDoc | pNode ) --> nType
X_FUNC( XML_TYPE )
{
   X_XML_NODE *node = x_xmlParam( ctx, 1, 1 );

   x_retni( ctx, IIF( node != NULL, node->type, 0 ) );
}

// All child nodes of any type
// xml_Nodes( pDoc | pNode ) --> { pNode, ... }
X_FUNC( XML_NODES )
{
   X_XML_NODE *node = x_xmlParam( ctx, 1, 1 );
   size_t count = 0;
   X_XML_NODE *child;
   X_ITEM *array;

   for( child = IIF( node != NULL, node->first, NULL ); child != NULL; child = child->next )
      count++;

   array = x_itemArrayNew( count );
   count = 0;

   for( child = IIF( node != NULL, node->first, NULL ); child != NULL; child = child->next )
      x_arraySetPtr( array, ++count, child );

   x_itemReturn( ctx, array );
   x_itemRelease( array );
}

// First child node of any type
// xml_FirstNode( pDoc | pNode ) --> pNode | NIL
X_FUNC( XML_FIRSTNODE )
{
   X_XML_NODE *node = x_xmlParam( ctx, 1, 1 );

   if( node != NULL )
      x_xmlRetNode( ctx, node->first );
}

// Last child node of any type
// xml_LastNode( pDoc | pNode ) --> pNode | NIL
X_FUNC( XML_LASTNODE )
{
   X_XML_NODE *node = x_xmlParam( ctx, 1, 1 );

   if( node != NULL )
      x_xmlRetNode( ctx, node->last );
}

// Next sibling node of any type
// xml_NextNode( pNode ) --> pNode | NIL
X_FUNC( XML_NEXTNODE )
{
   X_XML_NODE *node = x_xmlParam( ctx, 1, 0 );

   if( node != NULL )
      x_xmlRetNode( ctx, node->next );
}

// Previous sibling node of any type
// xml_PrevNode( pNode ) --> pNode | NIL
X_FUNC( XML_PREVNODE )
{
   X_XML_NODE *node = x_xmlParam( ctx, 1, 0 );

   if( node != NULL )
      x_xmlRetNode( ctx, node->prev );
}

/* ---------------------------------------------------------------------------------------------
   Node
   --------------------------------------------------------------------------------------------- */

// Element name or processing instruction target; empty for other nodes
// xml_Name( pNode ) --> cName
X_FUNC( XML_NAME )
{
   X_XML_NODE *node = x_xmlParam( ctx, 1, 0 );

   x_retc( ctx, IIF( node != NULL && node->name != NULL, node->name, "" ) );
}

// Namespace prefix of the element name, empty when none
// xml_Prefix( pNode ) --> cPrefix
X_FUNC( XML_PREFIX )
{
   X_XML_NODE *node = x_xmlParam( ctx, 1, 0 );
   const char *colon = IIF( x_xmlIsElement( node ), strchr( node->name, ':' ), NULL );

   if( colon != NULL )
      x_retclen( ctx, node->name, ( size_t )( colon - node->name ) );
   else
      x_retc( ctx, "" );
}

// Element name without the namespace prefix
// xml_LocalName( pNode ) --> cName
X_FUNC( XML_LOCALNAME )
{
   X_XML_NODE *node = x_xmlParam( ctx, 1, 0 );
   const char *colon = IIF( x_xmlIsElement( node ), strchr( node->name, ':' ), NULL );

   if( colon != NULL )
      x_retc( ctx, colon + 1 );
   else
      x_retc( ctx, IIF( x_xmlIsElement( node ), node->name, "" ) );
}

// Namespace URI of the element, declared on it or on an ancestor; empty when none
// xml_Namespace( pNode ) --> cUri
X_FUNC( XML_NAMESPACE )
{
   X_XML_NODE *node = x_xmlParam( ctx, 1, 0 );
   size_t len = 0;
   const char *uri = IIF( x_xmlIsElement( node ), x_xmlNamespace( node, &len ), NULL );

   if( uri != NULL )
      x_retclen( ctx, uri, len );
   else
      x_retc( ctx, "" );
}

// Text of an element (its text and CDATA children joined) or the content of any other node
// xml_Text( pNode ) --> cText
X_FUNC( XML_TEXT )
{
   X_XML_NODE *node = x_xmlParam( ctx, 1, 0 );
   X_XML_BUF buf = { NULL, 0, 0 };

   if( node != NULL )
   {
      x_xmlGetText( node, &buf );

      if( buf.data != NULL )
      {
         x_retclen_buffer( ctx, buf.data, buf.len );
         return;
      }
   }

   x_retc( ctx, "" );
}

// Attribute value; NIL when the attribute does not exist
// xml_Attr( pNode, cName ) --> cValue | NIL
X_FUNC( XML_ATTR )
{
   X_XML_NODE *node = x_xmlParam( ctx, 1, 0 );
   X_XML_ATTR *attr = NULL;

   if( node != NULL && X_ISCHAR( ctx, 2 ) )
      attr = x_xmlAttrGet( node, x_parc( ctx, 2 ), x_parclen( ctx, 2 ) );

   if( attr != NULL )
      x_retclen( ctx, attr->value, attr->valueLen );
}

// All attributes in document order
// xml_Attrs( pNode ) --> { { cName, cValue }, ... }
X_FUNC( XML_ATTRS )
{
   X_XML_NODE *node = x_xmlParam( ctx, 1, 0 );
   size_t count = IIF( node != NULL, node->attrCount, 0 );
   X_ITEM *array = x_itemArrayNew( count );
   size_t i;

   for( i = 0; i < count; i++ )
   {
      X_ITEM *pair = x_itemArrayNew( 2 );

      x_arraySetC( pair, 1, node->attrs[i].name );
      x_arraySetCL( pair, 2, node->attrs[i].value, node->attrs[i].valueLen );
      x_arraySet( array, i + 1, pair );
      x_itemRelease( pair );
   }

   x_itemReturn( ctx, array );
   x_itemRelease( array );
}

/* ---------------------------------------------------------------------------------------------
   Building
   --------------------------------------------------------------------------------------------- */

// Append a child element; a document accepts only one root element
// xml_Add( pDoc | pNode, cName, [cText] ) --> pNode | NIL
X_FUNC( XML_ADD )
{
   X_XML_NODE *parent = x_xmlParam( ctx, 1, 1 );
   X_XML_NODE *node;

   if( parent == NULL || !X_ISCHAR( ctx, 2 ) || !x_xmlCanHold( parent, XML_TYPE_ELEMENT, NULL ) )
      return;

   if( !x_xmlValidName( x_parc( ctx, 2 ), x_parclen( ctx, 2 ) ) )
      return;

   node = x_xmlAdd( parent, x_parc( ctx, 2 ), x_parclen( ctx, 2 ) );

   if( X_ISCHAR( ctx, 3 ) )
      x_xmlSetText( node, x_parc( ctx, 3 ), x_parclen( ctx, 3 ) );

   x_retptr( ctx, node );
}

// Append a text node to an element
// xml_AddText( pNode, cText ) --> pNode | NIL
X_FUNC( XML_ADDTEXT )
{
   X_XML_NODE *parent = x_xmlParam( ctx, 1, 0 );
   X_XML_NODE *node;

   if( !x_xmlIsElement( parent ) || !X_ISCHAR( ctx, 2 ) )
      return;

   node = x_xmlNodeNew( XML_TYPE_TEXT, NULL, 0, x_parc( ctx, 2 ), x_parclen( ctx, 2 ) );
   x_xmlLink( parent, node, NULL );

   x_retptr( ctx, node );
}

// Append a CDATA section to an element
// xml_AddCData( pNode, cText ) --> pNode | NIL
X_FUNC( XML_ADDCDATA )
{
   X_XML_NODE *parent = x_xmlParam( ctx, 1, 0 );
   X_XML_NODE *node;

   if( !x_xmlIsElement( parent ) || !X_ISCHAR( ctx, 2 ) )
      return;

   node = x_xmlNodeNew( XML_TYPE_CDATA, NULL, 0, x_parc( ctx, 2 ), x_parclen( ctx, 2 ) );
   x_xmlLink( parent, node, NULL );

   x_retptr( ctx, node );
}

// Append a comment to a document or an element
// xml_AddComment( pDoc | pNode, cText ) --> pNode | NIL
X_FUNC( XML_ADDCOMMENT )
{
   X_XML_NODE *parent = x_xmlParam( ctx, 1, 1 );
   X_XML_NODE *node;

   if( parent == NULL || !X_ISCHAR( ctx, 2 ) || !x_xmlCanHold( parent, XML_TYPE_COMMENT, NULL ) )
      return;

   node = x_xmlNodeNew( XML_TYPE_COMMENT, NULL, 0, NULL, 0 );
   x_xmlSetText( node, x_parc( ctx, 2 ), x_parclen( ctx, 2 ) );
   x_xmlLink( parent, node, NULL );

   x_retptr( ctx, node );
}

// Append a processing instruction <?cTarget cData?> to a document or an element
// xml_AddPI( pDoc | pNode, cTarget, [cData] ) --> pNode | NIL
X_FUNC( XML_ADDPI )
{
   X_XML_NODE *parent = x_xmlParam( ctx, 1, 1 );
   X_XML_NODE *node;
   const char *target;
   size_t len;
   int reserved;

   if( parent == NULL || !X_ISCHAR( ctx, 2 ) || !x_xmlCanHold( parent, XML_TYPE_PI, NULL ) )
      return;

   target = x_parc( ctx, 2 );
   len = x_parclen( ctx, 2 );
   if( !x_xmlValidName( target, len ) )
      return;

   /* The target "xml" in any case is reserved for the XML declaration */
   reserved = len == 3 && ( target[0] | 0x20 ) == 'x' && ( target[1] | 0x20 ) == 'm';
   reserved = reserved && ( target[2] | 0x20 ) == 'l';
   if( reserved )
      return;

   node = x_xmlNodeNew( XML_TYPE_PI, target, len, NULL, 0 );

   if( X_ISCHAR( ctx, 3 ) )
      x_xmlSetText( node, x_parc( ctx, 3 ), x_parclen( ctx, 3 ) );

   x_xmlLink( parent, node, NULL );

   x_retptr( ctx, node );
}

// Insert a new element before pRef, or after it when lAfter is .T.
// xml_Insert( pRef, cName, [cText], [lAfter] ) --> pNode | NIL
X_FUNC( XML_INSERT )
{
   X_XML_NODE *ref = x_xmlParam( ctx, 1, 0 );
   X_XML_NODE *node;

   if( ref == NULL || ref->parent == NULL || !x_xmlCanHold( ref->parent, XML_TYPE_ELEMENT, NULL ) )
      return;

   if( !X_ISCHAR( ctx, 2 ) || !x_xmlValidName( x_parc( ctx, 2 ), x_parclen( ctx, 2 ) ) )
      return;

   node = x_xmlElementNew( x_parc( ctx, 2 ), x_parclen( ctx, 2 ) );

   if( X_ISCHAR( ctx, 3 ) )
      x_xmlSetText( node, x_parc( ctx, 3 ), x_parclen( ctx, 3 ) );

   x_xmlLink( ref->parent, node, IIF( X_ISLOG( ctx, 4 ) && x_parl( ctx, 4 ), ref->next, ref ) );

   x_retptr( ctx, node );
}

// Append a deep copy of pNode to pParent, which may belong to another document
// xml_Clone( pNode, pParent ) --> pNode | NIL
X_FUNC( XML_CLONE )
{
   X_XML_NODE *node = x_xmlParam( ctx, 1, 0 );
   X_XML_NODE *parent = x_xmlParam( ctx, 2, 1 );
   X_XML_NODE *copy;

   if( node == NULL || parent == NULL || !x_xmlCanHold( parent, node->type, NULL ) )
      return;

   copy = x_xmlClone( node );
   x_xmlLink( parent, copy, NULL );

   x_retptr( ctx, copy );
}

// Move pNode to pParent, at the end or before pBefore (a child of pParent)
// xml_Move( pNode, pParent, [pBefore] ) --> lSuccess
X_FUNC( XML_MOVE )
{
   X_XML_NODE *node = x_xmlParam( ctx, 1, 0 );
   X_XML_NODE *parent = x_xmlParam( ctx, 2, 1 );
   X_XML_NODE *before = x_xmlParam( ctx, 3, 0 );
   int ok = node != NULL && parent != NULL;

   /* Not into itself, not before a node of another parent, and the root element stays */
   if( ok )
      ok = !x_xmlContains( node, parent ) && x_xmlCanHold( parent, node->type, node );

   if( ok && before != NULL )
      ok = before->parent == parent && before != node;

   if( ok && node->type == XML_TYPE_ELEMENT && node->parent != NULL )
      ok = node->parent->type != XML_TYPE_DOCUMENT;

   if( !ok )
   {
      x_retl( ctx, 0 );
      return;
   }

   x_xmlUnlink( node );
   x_xmlLink( parent, node, before );

   x_retl( ctx, 1 );
}

// Set an attribute of an element; a NIL value removes it
// xml_SetAttr( pNode, cName, [cValue] ) --> NIL
X_FUNC( XML_SETATTR )
{
   X_XML_NODE *node = x_xmlParam( ctx, 1, 0 );
   const char *value = IIF( X_ISCHAR( ctx, 3 ), x_parc( ctx, 3 ), NULL );
   size_t len = IIF( X_ISCHAR( ctx, 3 ), x_parclen( ctx, 3 ), 0 );

   if( x_xmlIsElement( node ) && X_ISCHAR( ctx, 2 ) && x_xmlValidName( x_parc( ctx, 2 ), x_parclen( ctx, 2 ) ) )
      x_xmlAttrSet( node, x_parc( ctx, 2 ), x_parclen( ctx, 2 ), value, len );
}

// Replace the text of an element (its text and CDATA children) or the content of any other node;
// NIL clears it
// xml_SetText( pNode, [cText] ) --> NIL
X_FUNC( XML_SETTEXT )
{
   X_XML_NODE *node = x_xmlParam( ctx, 1, 0 );
   const char *text = IIF( X_ISCHAR( ctx, 2 ), x_parc( ctx, 2 ), NULL );
   size_t len = IIF( X_ISCHAR( ctx, 2 ), x_parclen( ctx, 2 ), 0 );

   if( node != NULL )
      x_xmlSetText( node, text, len );
}

// Remove a node with all its children; the pointer is invalid afterwards
// xml_Remove( pNode ) --> NIL
X_FUNC( XML_REMOVE )
{
   X_XML_NODE *node = x_xmlParam( ctx, 1, 0 );

   if( node != NULL )
      x_xmlFree( node );
}

// Escape text for use in element content
// xml_Escape( cText ) --> cEscaped
X_FUNC( XML_ESCAPE )
{
   X_XML_BUF buf = { NULL, 0, 0 };

   if( X_ISCHAR( ctx, 1 ) && x_parclen( ctx, 1 ) > 0 )
   {
      x_xmlAppendEscaped( &buf, x_parc( ctx, 1 ), x_parclen( ctx, 1 ), 0 );
      x_retclen_buffer( ctx, buf.data, buf.len );
   }
   else
      x_retc( ctx, "" );
}
