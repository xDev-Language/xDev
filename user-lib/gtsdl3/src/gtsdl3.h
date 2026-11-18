/*
 * Copyright (c) 2024-2026 xDev-Language
 *
 * Library: xDev GTSDL3
 * File: gtsdl3.h
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#ifndef X_GTSDL3_H
#define X_GTSDL3_H

#include "terminal.h"
#include "gtinfo.h"
#include "gfxdef.h"
#include <xdev/inkey.ch>
#include <xdev/setcurs.ch>
#include "platform.h"

#include <SDL3/SDL.h>

#include <stdint.h>
#include <stddef.h>

#define X_GT_NAME SDL3

typedef uint8_t  X_BYTE;
typedef uint16_t X_USHORT;
typedef uint16_t X_WCHAR;
typedef int64_t  X_I64;
typedef int64_t  X_MAXINT;
typedef size_t   X_SIZE;
typedef int      X_FHANDLE;

#define GTSDL3_MAX( left, right ) ( ( left ) > ( right ) ? ( left ) : ( right ) )
#define GTSDL3_MIN( left, right ) ( ( left ) < ( right ) ? ( left ) : ( right ) )
#define X_SYMBOL_UNUSED( value ) UNUSED( value )
#define X_UNCONST( value ) ( ( void * )( uintptr_t )( const void * )( value ) )

#endif /* X_GTSDL3_H */
