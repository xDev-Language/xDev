/*
 * Copyright (c) 2024-2026 xDev-Language
 *
 * Library: xDev QRCODE
 * File: qrcode-api.c
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "internal-qrcode.h"

/* ---------------------------------------------------------------------------------------------
   QR code
   --------------------------------------------------------------------------------------------- */

// QR code of the text (UTF-8) as rows of "1" (dark) and "0" (light), without the quiet zone of four
// light modules that must surround it; nEcc is QR_ECC_* (default QR_ECC_MEDIUM), nMinVersion 1-40
// raises the size; NIL when the text does not fit
// QrEncode( cText, [nEcc], [nMinVersion] ) --> aRows | NIL
X_FUNC( QRENCODE )
{
   int ecc = QR_ECC_MEDIUM;
   uint8_t *modules;
   char row[X_QR_MAX_SIZE + 1];
   int size;
   int x;
   int y;
   X_ITEM *array;

   X_ARGCHECK( ctx, X_ISCHAR( ctx, 1 ), "QRENCODE" );

   if( X_ISNUM( ctx, 2 ) && x_parni( ctx, 2 ) >= QR_ECC_LOW && x_parni( ctx, 2 ) <= QR_ECC_HIGH )
      ecc = x_parni( ctx, 2 );

   modules = ( uint8_t * )x_xgrab( X_QR_MAX_SIZE * X_QR_MAX_SIZE );
   size = x_qrEncode( x_parc( ctx, 1 ), x_parclen( ctx, 1 ), ecc, x_parni( ctx, 3 ), modules );

   if( size == 0 )
   {
      x_xfree( modules );
      return;
   }

   array = x_itemArrayNew( ( size_t )size );

   for( y = 0; y < size; y++ )
   {
      for( x = 0; x < size; x++ )
         row[x] = ( char )( '0' + modules[y * size + x] );

      x_arraySetCL( array, ( size_t )y + 1, row, ( size_t )size );
   }

   x_xfree( modules );
   x_itemReturn( ctx, array );
   x_itemRelease( array );
}
