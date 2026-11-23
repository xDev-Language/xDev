/*
 * Copyright (c) 2024-2026 xDev-Language
 *
 * Library...: xDev QRCODE
 * File......: internal-qrcode.h
 *
 */

#ifndef INTERNAL_QRCODE_H
#define INTERNAL_QRCODE_H

#include "platform.h"
#include "internal.h"

#include "xdev/qrcode.ch"

/* The size of version 40 in modules, the largest QR code */
#define X_QR_MAX_SIZE 177

/* Encode text (UTF-8 bytes) as a QR code; modules[y * size + x] is 1 for a dark module.
   Returns the size in modules, or 0 when the text does not fit into version 40. */
int x_qrEncode( const char *text, size_t length, int ecc, int minVersion, uint8_t *modules );

#endif
