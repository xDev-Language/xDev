/*
 * Copyright (c) 2024-2026 xDev-Language
 *
 * Library...: xDev Runtime
 * File......: internal-private.h
 *
 */

#ifndef INTERNAL_PRIVATE_H
#define INTERNAL_PRIVATE_H

#include "internal.h"

typedef struct _X_ARR X_ARR;
typedef struct _X_HASH X_HASH;
typedef struct _X_STRUCT X_STRUCT;
typedef struct _X_GC_NODE X_GC_NODE;

/* The kinds of garbage collected values */
typedef enum
{
   X_GC_ARRAY,
   X_GC_BLOCK,
   X_GC_BOX,
   X_GC_CARGO,
   X_GC_HASH,
   X_GC_STRUCT
} X_GC_KIND;

/* An array; an object is an array with a class */
struct _X_ARR
{
   X_ITEM *items;
   int64_t len;
   size_t capacity;
   size_t nRefs;
   uint32_t classHandle;
};

/* The pairs keep the order of adding; slots is the index of the table: the position of a pair + 1, 0 when free */
struct _X_HASH
{
   X_ITEM *keys;
   X_ITEM *values;
   size_t len;
   size_t capacity;
   size_t *slots;
   size_t slotCount;
   size_t nRefs;
   uint32_t flags;
   X_ITEM defaultValue;
};

/* A STRUCT: the fields follow the header in the C layout of its type */
struct _X_STRUCT
{
   const X_STRUCT_TYPE *type;
   size_t refs;
   unsigned char fields[];
};

X_GC_NODE *x_gcRegister( void *value, size_t *refs, X_GC_KIND kind );
unsigned int x_gcPause( void );
void x_gcRestore( unsigned int *depth );
void x_itemReplace( X_ITEM *destination, X_ITEM value );
double x_itemAsDouble( const X_ITEM *item );
int64_t x_itemAsInt64( const X_ITEM *item );

#endif /* INTERNAL_PRIVATE_H */
