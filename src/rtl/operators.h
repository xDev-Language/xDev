/*
 * Copyright (c) 2024-2026 xDev-Language
 *
 * Library...: xDev Runtime
 * File......: operators.h
 *
 */

#ifndef OPERATORS_H
#define OPERATORS_H

#ifdef __cplusplus
extern "C"
{
#endif

/* The operators of xDev, operators.c; the result goes to destination */
int x_itemContains( X_CALL_CTX *ctx, X_ITEM *destination, const X_ITEM *left, const X_ITEM *right );
int x_itemMod( X_CALL_CTX *ctx, X_ITEM *destination, const X_ITEM *left, const X_ITEM *right );
int x_itemMul( X_CALL_CTX *ctx, X_ITEM *destination, const X_ITEM *left, const X_ITEM *right );
int x_itemPow( X_CALL_CTX *ctx, X_ITEM *destination, const X_ITEM *left, const X_ITEM *right );
int x_itemAdd( X_CALL_CTX *ctx, X_ITEM *destination, const X_ITEM *left, const X_ITEM *right );
int x_itemAddAssign( X_CALL_CTX *ctx, X_ITEM *destination, X_ITEM *target, X_ITEM *left, const X_ITEM *right );
int x_stringAddAssign( X_CALL_CTX *ctx, X_ITEM *destination, X_STRING *target, X_ITEM *snapshot, X_ITEM *left,
                       const X_ITEM *right );
int x_itemInc( X_CALL_CTX *ctx, X_ITEM *destination, const X_ITEM *value );
int x_itemSub( X_CALL_CTX *ctx, X_ITEM *destination, const X_ITEM *left, const X_ITEM *right );
int x_itemNeg( X_CALL_CTX *ctx, X_ITEM *destination, const X_ITEM *value );
int x_itemDec( X_CALL_CTX *ctx, X_ITEM *destination, const X_ITEM *value );
int x_itemNot( X_CALL_CTX *ctx, X_ITEM *destination, const X_ITEM *value );
int x_itemDiv( X_CALL_CTX *ctx, X_ITEM *destination, const X_ITEM *left, const X_ITEM *right );
int x_itemLt( X_CALL_CTX *ctx, X_ITEM *destination, const X_ITEM *left, const X_ITEM *right );
int x_itemLe( X_CALL_CTX *ctx, X_ITEM *destination, const X_ITEM *left, const X_ITEM *right );
int x_itemNe( X_CALL_CTX *ctx, X_ITEM *destination, const X_ITEM *left, const X_ITEM *right );
int x_itemEqS( X_CALL_CTX *ctx, X_ITEM *destination, const X_ITEM *left, const X_ITEM *right );
int x_itemEq( X_CALL_CTX *ctx, X_ITEM *destination, const X_ITEM *left, const X_ITEM *right );
int x_itemGt( X_CALL_CTX *ctx, X_ITEM *destination, const X_ITEM *left, const X_ITEM *right );
int x_itemGe( X_CALL_CTX *ctx, X_ITEM *destination, const X_ITEM *left, const X_ITEM *right );

#ifdef __cplusplus
}
#endif

#endif /* OPERATORS_H */
