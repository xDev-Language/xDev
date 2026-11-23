/*
 * Copyright (c) 2024-2026 xDev-Language
 *
 * Library...: xDev JSON
 * File......: internal-json.h
 *
 */

#ifndef INTERNAL_JSON_H
#define INTERNAL_JSON_H

#include "platform.h"
#include "internal.h"

/* The deepest nesting of arrays and objects; it also stops a cycle of arrays */
#define X_JSON_DEPTH 512

char *x_jsonEncode( const X_ITEM *value, int pretty, size_t *length );
int x_jsonDecode( const char *text, size_t length, X_ITEM *result );
const char *x_jsonError( void );

#endif
