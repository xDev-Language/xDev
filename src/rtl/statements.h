/*
 * Copyright (c) 2024-2026 xDev-Language
 *
 * Library...: xDev Runtime
 * File......: statements.h
 *
 */

#ifndef STATEMENTS_H
#define STATEMENTS_H

#ifdef __cplusplus
extern "C"
{
#endif

/* The statements of xDev that the compiler writes as calls, statements.c */
int x_getExact( void );
int x_arrayDimensions( X_ITEM *dest, int32_t count, const X_ARG *dimensions );
int x_arrayDimensionLength( const X_ITEM *dimension, int64_t *length );
X_ARG *x_initArgs( int32_t count, const char *const *values );
void x_initArgsFree( int32_t count, X_ARG *args );
int x_switchValue( X_ITEM *dest, const X_ITEM *value );
int x_switchMatch( const X_ITEM *value, const X_ITEM *label );
int x_forTest( X_CALL_CTX *ctx, X_ITEM *dest, const X_ITEM *counter, const X_ITEM *limit, const X_ITEM *step );
int x_forIncrement( X_CALL_CTX *ctx, X_ITEM *dest, const X_ITEM *counter );

/* FOR EACH */
void x_eachBase( X_CALL_CTX *ctx );
void x_eachCount( X_CALL_CTX *ctx );
void x_eachPos( X_CALL_CTX *ctx );
void x_eachGet( X_CALL_CTX *ctx );
void x_eachIndex( X_CALL_CTX *ctx );
void x_eachKey( X_CALL_CTX *ctx );
void x_eachPut( X_CALL_CTX *ctx );
void x_eachSwap( X_CALL_CTX *ctx );

#ifdef __cplusplus
}
#endif

#endif /* STATEMENTS_H */
