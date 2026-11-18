/*
 * Copyright (c) 2024-2026 xDev-Language
 *
 * Library: xDev Runtime
 * File: platform.h
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#ifndef PLATFORM_H
#define PLATFORM_H

/* ---------------------------------------------------------------------------------------------
   Operating system
   --------------------------------------------------------------------------------------------- */

/* UNIX/Linux family */
#if defined( __unix__ ) || defined( __unix ) || defined( unix )
#   define X_OS_UNIX 1
#else
#   define X_OS_UNIX 0
#endif

/* Linux */
#if defined( __linux__ ) || defined( __linux )
#   define X_OS_LINUX 1
#else
#   define X_OS_LINUX 0
#endif

/* Apple macOS (Darwin) */
#if defined( __APPLE__ ) && defined( __MACH__ )
#   define X_OS_DARWIN 1
#   define X_OS_MACOS 1
#else
#   define X_OS_DARWIN 0
#   define X_OS_MACOS 0
#endif

/* BSD family */
#if defined( __FreeBSD__ ) || defined( __OpenBSD__ ) || defined( __NetBSD__ ) || defined( __DragonFly__ )
#   define X_OS_BSD 1
#else
#   define X_OS_BSD 0
#endif

/* Windows */
#if defined( _WIN32 ) || defined( _WIN64 ) || defined( __WIN32__ ) || defined( __TOS_WIN__ ) || defined( __WINDOWS__ )
#   define X_OS_WINDOWS 1
#   ifndef WIN32_LEAN_AND_MEAN
#      define WIN32_LEAN_AND_MEAN
#   endif
#else
#   define X_OS_WINDOWS 0
#endif

/* Windows with Virtual Terminal support (Windows 10 1809+) */
#if X_OS_WINDOWS
#   if defined( _WIN32_WINNT ) && _WIN32_WINNT >= 0x0A00
#      define X_WINDOWS_HAS_VT 1
#   else
#      define X_WINDOWS_HAS_VT 0
#   endif
#else
#   define X_WINDOWS_HAS_VT 0
#endif

/* Cygwin (Windows with POSIX layer) */
#if defined( __CYGWIN__ )
#   define X_OS_CYGWIN 1
#else
#   define X_OS_CYGWIN 0
#endif

/* Haiku */
#ifdef __HAIKU__
#   define X_OS_HAIKU 1
#else
#   define X_OS_HAIKU 0
#endif

/* Solaris/Illumos */
#ifdef __sun
#   define X_OS_SOLARIS 1
#else
#   define X_OS_SOLARIS 0
#endif

/* IBM AIX */
#ifdef _AIX
#   define X_OS_AIX 1
#else
#   define X_OS_AIX 0
#endif

/* ---------------------------------------------------------------------------------------------
   Groups of systems
   --------------------------------------------------------------------------------------------- */

/* Any POSIX/UNIX-like system */
#if X_OS_UNIX || X_OS_LINUX || X_OS_DARWIN || X_OS_BSD || X_OS_CYGWIN || X_OS_HAIKU || X_OS_SOLARIS || X_OS_AIX
#   define X_OS_POSIX 1
#else
#   define X_OS_POSIX 0
#endif

/* Any BSD system (including Darwin) */
#if X_OS_BSD || X_OS_DARWIN
#   define X_OS_BSD_FAMILY 1
#else
#   define X_OS_BSD_FAMILY 0
#endif

/* X11 availability (for the xwc backend); macOS has XQuartz, but not by default */
#if X_OS_POSIX && !X_OS_DARWIN
#   define X_HAS_X11_POSSIBLE 1
#else
#   define X_HAS_X11_POSSIBLE 0
#endif

/* Disabled by default; set by the build */
#ifndef X_HAS_X11
#   define X_HAS_X11 0
#endif

/* ---------------------------------------------------------------------------------------------
   Architecture
   --------------------------------------------------------------------------------------------- */

/* 64-bit */
#if defined( __x86_64__ ) || defined( _M_X64 ) || defined( __aarch64__ ) || defined( __64BIT__ ) || defined( __LP64__ )
#   define X_ARCH_64BIT 1
#else
#   define X_ARCH_64BIT 0
#endif

/* Endianness */
#if defined( __BYTE_ORDER__ ) && __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
#   define X_LITTLE_ENDIAN 1
#   define X_BIG_ENDIAN 0
#elif defined( __LITTLE_ENDIAN__ ) || defined( _WIN32 ) || defined( __i386__ ) || defined( __x86_64__ )
#   define X_LITTLE_ENDIAN 1
#   define X_BIG_ENDIAN 0
#elif defined( __BYTE_ORDER__ ) && __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
#   define X_LITTLE_ENDIAN 0
#   define X_BIG_ENDIAN 1
#elif defined( __BIG_ENDIAN__ ) || defined( __sparc__ )
#   define X_LITTLE_ENDIAN 0
#   define X_BIG_ENDIAN 1
#else
#   define X_LITTLE_ENDIAN 0
#   define X_BIG_ENDIAN 0
#endif

/* ---------------------------------------------------------------------------------------------
   Compiler
   --------------------------------------------------------------------------------------------- */

/* GCC or compatible */
#if defined( __GNUC__ ) && !defined( __clang__ )
#   define X_COMPILER_GCC 1
#   define X_COMPILER_VERSION ( __GNUC__ * 10000 + __GNUC_MINOR__ * 100 + __GNUC_PATCHLEVEL__ )
#elif defined( __clang__ )
#   define X_COMPILER_CLANG 1
#   define X_COMPILER_VERSION ( __clang_major__ * 10000 + __clang_minor__ * 100 + __clang_patchlevel__ )
#elif defined( _MSC_VER )
#   define X_COMPILER_MSVC 1
#   define X_COMPILER_VERSION _MSC_VER
#else
#   define X_COMPILER_UNKNOWN 1
#   define X_COMPILER_VERSION 0
#endif

/* MinGW (GCC on Windows) */
#if defined( __MINGW32__ ) || defined( __MINGW64__ )
#   define X_COMPILER_MINGW 1
#else
#   define X_COMPILER_MINGW 0
#endif

/* ---------------------------------------------------------------------------------------------
   Debug or release
   --------------------------------------------------------------------------------------------- */

#if !defined( NDEBUG ) || defined( _DEBUG ) || defined( DEBUG )
#   define X_DEBUG 1
#else
#   define X_DEBUG 0
#endif

/* ---------------------------------------------------------------------------------------------
   Language (C or C++)
   --------------------------------------------------------------------------------------------- */

#ifdef __cplusplus
#   define X_LANG_CPP 1
#   define X_EXTERN_C_BEGIN extern "C" {
#   define X_EXTERN_C_END }
#   define X_CPP 1
#else
#   define X_LANG_CPP 0
#   define X_EXTERN_C_BEGIN
#   define X_EXTERN_C_END
#   define X_CPP 0
#endif

/* ---------------------------------------------------------------------------------------------
   Short checks
   --------------------------------------------------------------------------------------------- */

#define X_ON_WINDOWS() ( X_OS_WINDOWS )
#define X_ON_UNIX() ( X_OS_POSIX )
#define X_ON_LINUX() ( X_OS_LINUX )
#define X_ON_MACOS() ( X_OS_MACOS )
#define X_ON_BSD() ( X_OS_BSD_FAMILY )

#define X_IS_DEBUG() ( X_DEBUG )
#define X_IS_64BIT() ( X_ARCH_64BIT )

/* ---------------------------------------------------------------------------------------------
   Deprecation warnings
   --------------------------------------------------------------------------------------------- */

#if X_COMPILER_GCC || X_COMPILER_CLANG
#   define X_DEPRECATED( msg ) __attribute__( ( deprecated( msg ) ) )
#elif defined( _MSC_VER )
#   define X_DEPRECATED( msg ) __declspec( deprecated( msg ) )
#else
#   define X_DEPRECATED( msg )
#endif

/* ---------------------------------------------------------------------------------------------
   Inline hints
   --------------------------------------------------------------------------------------------- */

#if X_COMPILER_GCC || X_COMPILER_CLANG
#   define X_INLINE inline __attribute__( ( always_inline ) )
#   define X_NOINLINE __attribute__( ( noinline ) )
#elif defined( _MSC_VER )
#   define X_INLINE __forceinline
#   define X_NOINLINE __declspec( noinline )
#else
#   define X_INLINE inline
#   define X_NOINLINE
#endif

/* ---------------------------------------------------------------------------------------------
   Inline assembly
   --------------------------------------------------------------------------------------------- */

#if X_COMPILER_GCC || X_COMPILER_CLANG
#   define X_ASM( ... ) __asm__ __volatile__( __VA_ARGS__ )
#elif defined( _MSC_VER )
#   define X_ASM( ... ) __asm { __VA_ARGS__ }
#else
#   define X_ASM( ... )
#endif

/* ---------------------------------------------------------------------------------------------
   API export/import for shared libraries (DLL)
   --------------------------------------------------------------------------------------------- */

#if X_ON_WINDOWS()
/* Export from DLL / import when using */
#   ifdef X_BUILDING_DLL
#      define X_API __declspec( dllexport )
#   elif defined( X_USING_DLL )
#      define X_API __declspec( dllimport )
#   else
#      define X_API
#   endif
#else
/* On UNIX-like systems use visibility where available */
#   if X_COMPILER_GCC || X_COMPILER_CLANG
#      define X_API __attribute__( ( visibility( "default" ) ) )
#   else
#      define X_API
#   endif
#endif

/* ---------------------------------------------------------------------------------------------
   Likely/unlikely hints
   --------------------------------------------------------------------------------------------- */

#if X_COMPILER_GCC || X_COMPILER_CLANG
#   define X_LIKELY( x ) __builtin_expect( !!( x ), 1 )
#   define X_UNLIKELY( x ) __builtin_expect( !!( x ), 0 )
#else
#   define X_LIKELY( x ) ( x )
#   define X_UNLIKELY( x ) ( x )
#endif

/* ---------------------------------------------------------------------------------------------
   Format checking of printf-like functions
   --------------------------------------------------------------------------------------------- */

#if X_COMPILER_GCC || X_COMPILER_CLANG
#   define X_PRINTF_ATTR( formatIndex, argsIndex ) __attribute__( ( format( printf, formatIndex, argsIndex ) ) )
#   define X_SCANF_ATTR( formatIndex, argsIndex ) __attribute__( ( format( scanf, formatIndex, argsIndex ) ) )
#else
#   define X_PRINTF_ATTR( formatIndex, argsIndex )
#   define X_SCANF_ATTR( formatIndex, argsIndex )
#endif

/* ---------------------------------------------------------------------------------------------
   Fallthrough in switch
   --------------------------------------------------------------------------------------------- */

#if defined( __GNUC__ ) && !defined( __clang__ ) && ( X_COMPILER_VERSION >= 70000 )
#   define X_FALLTHROUGH __attribute__( ( fallthrough ) )
#elif defined( __clang__ )
#   define X_FALLTHROUGH __attribute__( ( fallthrough ) )
#elif defined( _MSC_VER ) && ( X_COMPILER_VERSION >= 1910 )
#   define X_FALLTHROUGH [[fallthrough]]
#else
#   define X_FALLTHROUGH ( ( void )0 )
#endif

/* ---------------------------------------------------------------------------------------------
   Unused functions
   --------------------------------------------------------------------------------------------- */

#if X_COMPILER_GCC || X_COMPILER_CLANG
#   define X_UNUSED_FUNC __attribute__( ( unused ) )
#else
/* MSVC: use (void)x locally, pragma macro here would be misleading */
#   define X_UNUSED_FUNC
#endif

/* ---------------------------------------------------------------------------------------------
   Path characters of the operating system
   --------------------------------------------------------------------------------------------- */

#if X_OS_WINDOWS
#   define X_OS_PATH_SEPARATOR '\\'
#   define X_OS_PATH_DELIMITERS "\\/:"
#   define X_OS_PATH_LIST_SEPARATOR ";"
#   define X_OS_DRIVE_SEPARATOR ":"
#else
#   define X_OS_PATH_SEPARATOR '/'
#   define X_OS_PATH_DELIMITERS "/"
#   define X_OS_PATH_LIST_SEPARATOR ":"
#   define X_OS_DRIVE_SEPARATOR ""
#endif

#endif /* PLATFORM_H */
