/*
 * Copyright (c) 2024-2026 xDev-Language
 *
 * Library: xDev XML
 * File: internal-xml.h
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#ifndef INTERNAL_XML_H
#define INTERNAL_XML_H

#include "platform.h"
#include "internal.h"

#include "xdev/xml.ch"

/* Marks of a live document and a live node, checked for pointer arguments */
#define X_XML_DOC_MAGIC 0x584D4C44u
#define X_XML_NODE_MAGIC 0x584D4C4Eu

/* A growing text buffer, always ended with '\0' */
typedef struct
{
   char *data;
   size_t len;
   size_t cap;
} X_XML_BUF;

/* An attribute of an element */
typedef struct
{
   char *name;
   char *value;
   size_t valueLen;
} X_XML_ATTR;

/* A node of a document tree; the document itself is a node of the type XML_TYPE_DOCUMENT */
typedef struct X_XML_NODE X_XML_NODE;

struct X_XML_NODE
{
   uint32_t magic;
   int type;
   X_XML_NODE *parent;
   X_XML_NODE *first;
   X_XML_NODE *last;
   X_XML_NODE *prev;
   X_XML_NODE *next;
   char *name;
   X_XML_BUF text;
   X_XML_ATTR *attrs;
   size_t attrCount;
   size_t attrCap;
};

void x_xmlAppend( X_XML_BUF *buf, const char *text, size_t len );
void x_xmlAppendEscaped( X_XML_BUF *buf, const char *text, size_t len, int attr );
void x_xmlBufFree( X_XML_BUF *buf );

X_XML_NODE *x_xmlNew( void );
void x_xmlFree( X_XML_NODE *node );
X_XML_NODE *x_xmlParam( X_CALL_CTX *ctx, int n, int allowDoc );
void x_xmlRetNode( X_CALL_CTX *ctx, X_XML_NODE *node );
int x_xmlIsElement( const X_XML_NODE *node );

X_XML_NODE *x_xmlNodeNew( int type, const char *name, size_t nameLen, const char *text, size_t len );
X_XML_NODE *x_xmlElementNew( const char *name, size_t len );
X_XML_NODE *x_xmlAdd( X_XML_NODE *parent, const char *name, size_t len );
void x_xmlLink( X_XML_NODE *parent, X_XML_NODE *node, X_XML_NODE *before );
void x_xmlUnlink( X_XML_NODE *node );
X_XML_NODE *x_xmlClone( const X_XML_NODE *source );
int x_xmlContains( const X_XML_NODE *node, const X_XML_NODE *other );
int x_xmlCanHold( const X_XML_NODE *parent, int type, const X_XML_NODE *ignore );
int x_xmlValidName( const char *name, size_t len );
const char *x_xmlNamespace( const X_XML_NODE *node, size_t *len );

X_XML_NODE *x_xmlFirstElement( const X_XML_NODE *parent );
X_XML_NODE *x_xmlLastElement( const X_XML_NODE *parent );
X_XML_NODE *x_xmlNextElement( const X_XML_NODE *node );
X_XML_NODE *x_xmlPrevElement( const X_XML_NODE *node );

void x_xmlGetText( const X_XML_NODE *node, X_XML_BUF *out );
void x_xmlSetText( X_XML_NODE *node, const char *text, size_t len );
void x_xmlSafeComment( X_XML_BUF *out, const char *text, size_t len );
void x_xmlSafeInstruction( X_XML_BUF *out, const char *text, size_t len );
X_XML_ATTR *x_xmlAttrGet( const X_XML_NODE *node, const char *name, size_t nameLen );
void x_xmlAttrSet( X_XML_NODE *node, const char *name, size_t nameLen, const char *value, size_t len );
X_XML_NODE *x_xmlChild( const X_XML_NODE *node, const char *name, size_t len );
X_XML_NODE *x_xmlChildAt( const X_XML_NODE *node, const char *name, size_t len, size_t index );
X_XML_NODE *x_xmlFind( X_XML_NODE *node, const char *path );

X_XML_NODE *x_xmlParse( const char *text, size_t len );
X_XML_NODE *x_xmlLoad( const char *path );
void x_xmlWrite( X_XML_BUF *buf, const X_XML_NODE *node, int indent, const char *eol );
int x_xmlSave( const X_XML_NODE *node, const char *path, int indent, const char *eol );
const char *x_xmlError( void );

#endif
