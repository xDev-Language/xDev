/*
 * Copyright (c) 2024-2026 xDev-Language
 *
 * Library: xDev XAdES
 * File: internal-xades.h
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#ifndef INTERNAL_XADES_H
#define INTERNAL_XADES_H

#include "internal-xml.h"
#include "internal-openssl.h"

#include "xdev/xades.ch"

/* ---------------------------------------------------------------------------------------------
   Errors
   --------------------------------------------------------------------------------------------- */

void x_xadesSetError( const char *message );
void x_xadesClearError( void );
const char *x_xadesError( void );

/* ---------------------------------------------------------------------------------------------
   Canonical form, signature and verification
   --------------------------------------------------------------------------------------------- */

void x_xadesCanonical( X_XML_BUF *out, const X_XML_NODE *node, const X_XML_NODE *skip );
int x_xadesCanonicalText( X_XML_BUF *out, const char *xml, size_t len );
int x_xadesSign( X_XML_BUF *out, const char *xml, size_t len, const char *key, size_t keyLen, const char *cert,
                 size_t certLen, const char *password );
int x_xadesVerify( const char *xml, size_t len );

#endif
