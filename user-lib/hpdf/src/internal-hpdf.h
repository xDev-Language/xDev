/*
 * Copyright (c) 2024-2026 xDev-Language
 *
 * Library...: xDev HPDF
 * File......: internal-hpdf.h
 *
 */

#ifndef INTERNAL_HPDF_H
#define INTERNAL_HPDF_H

#include "xdev.h"

#include "hpdf.h"

const char *x_hpdfString( X_CALL_CTX *ctx, int n );
HPDF_BOOL x_hpdfBool( X_CALL_CTX *ctx, int n );
HPDF_REAL *x_hpdfReals( const X_ITEM *array );
void x_hpdfStore( X_CALL_CTX *ctx, int n, const X_ITEM *value );
void x_hpdfStorePoint( X_CALL_CTX *ctx, int n, HPDF_Point point );

void x_hpdfRetHandle( X_CALL_CTX *ctx, void *handle );
void x_hpdfRetString( X_CALL_CTX *ctx, const char *value );

HPDF_Point x_hpdfPoint( const X_ITEM *array );
HPDF_Point3D x_hpdfPoint3D( const X_ITEM *array );
HPDF_Rect x_hpdfRect( const X_ITEM *array );
HPDF_RGBColor x_hpdfRGBColor( const X_ITEM *array );
HPDF_CMYKColor x_hpdfCMYKColor( const X_ITEM *array );
HPDF_Date x_hpdfDate( const X_ITEM *array );

void x_hpdfPutPoint( X_ITEM *result, HPDF_Point value );
void x_hpdfRetPoint( X_CALL_CTX *ctx, HPDF_Point value );
void x_hpdfRetRect( X_CALL_CTX *ctx, HPDF_Rect value );
void x_hpdfRetRGBColor( X_CALL_CTX *ctx, HPDF_RGBColor value );
void x_hpdfRetCMYKColor( X_CALL_CTX *ctx, HPDF_CMYKColor value );
void x_hpdfRetTransMatrix( X_CALL_CTX *ctx, HPDF_TransMatrix value );
void x_hpdfRetTextWidth( X_CALL_CTX *ctx, HPDF_TextWidth value );
void x_hpdfRetDashMode( X_CALL_CTX *ctx, HPDF_DashMode value );

#endif
