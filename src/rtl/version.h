/*
 * Copyright (c) 2024-2026 xDev-Language
 *
 * Library: xDev Runtime
 * File: version.h
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#ifndef VERSION_H
#define VERSION_H

#include "platform.h"

/* The questions of Version( n ) */
enum X_VERSION
{
   X_VERSION_XDEV = 1,
   X_VERSION_COMPILER,
   X_VERSION_MAJOR,
   X_VERSION_MINOR,
   X_VERSION_RELEASE,
   X_VERSION_PLATFORM,
   X_VERSION_BITWIDTH,
   X_VERSION_ENDIANNESS,
   X_VERSION_BUILD_DATE_STR,
   X_VERSION_BUILD_TIME,
   X_VERSION_URL_BASE
};

#define X_VERSION_ENDIAN_LITTLE 1
#define X_VERSION_ENDIAN_BIG 2

/* The version of xDev */
#define X_VER_MAJOR 0
#define X_VER_MINOR 0
#define X_VER_RELEASE 0
#define X_VER_URL "https://github.com/xDev-Language/xDev"

/* The version of the bond between the runtime and a DLL module of --shared; every module returns it from
   x_moduleVersion() and ModuleLoad() rejects a module with another one. It grows when compiled code
   stops fitting the runtime: X_CALL_CTX, X_ITEM, the module lifecycle or the exceptions */
#define X_VER_MODULE 1

#define X_VER_TEXT_( n ) #n
#define X_VER_TEXT( n ) X_VER_TEXT_( n )
#define X_VER_NUMBER X_VER_TEXT( X_VER_MAJOR ) "." X_VER_TEXT( X_VER_MINOR ) "." X_VER_TEXT( X_VER_RELEASE )
#define X_VER_STRING "xDev " X_VER_NUMBER

#if X_OS_WINDOWS
#define X_VER_PLATFORM "Windows"
#elif X_OS_LINUX
#define X_VER_PLATFORM "Linux"
#elif X_OS_DARWIN
#define X_VER_PLATFORM "Darwin"
#elif X_OS_BSD
#define X_VER_PLATFORM "BSD"
#else
#define X_VER_PLATFORM "Unix"
#endif

#endif /* VERSION_H */
