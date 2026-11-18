/*
 * Copyright (c) 2024-2026 xDev-Language
 *
 * Library: xDev Runtime
 * File: internal.h
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#ifndef INTERNAL_H
#define INTERNAL_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C"
{
#endif

/* ---------------------------------------------------------------------------------------------
   Types
   --------------------------------------------------------------------------------------------- */

typedef struct _X_ITEM X_ITEM;
typedef struct _X_ARG X_ARG;
typedef struct _X_BOX X_BOX;
typedef struct _X_CALL_CTX X_CALL_CTX;
typedef struct _X_ERROR_DEF X_ERROR_DEF;
typedef struct _X_ERROR X_ERROR;
typedef struct _X_GC_FUNCS X_GC_FUNCS;

typedef struct
{
   char *data;
   size_t len;
} X_STRING;

typedef uint32_t X_TYPE;

/* ---------------------------------------------------------------------------------------------
   Constants
   --------------------------------------------------------------------------------------------- */

/* The types of X_ITEM */
enum
{
   X_IT_NIL = 0x00000,
   X_IT_ARRAY = 0x00002,
   X_IT_BLOCK = 0x00004,
   X_IT_BYREF = 0x00008,
   X_IT_DATE = 0x00010,
   X_IT_DOUBLE = 0x00040,
   X_IT_INTEGER = 0x00400,
   X_IT_LOGICAL = 0x00800,
   X_IT_UINT64 = 0x01000,
   X_IT_POINTER = 0x08000,
   X_IT_STRING = 0x20000,
   X_IT_HASH = 0x40000,
   X_IT_GCFLAG = 0x100000,
   X_IT_GCPOINTER = X_IT_POINTER | X_IT_GCFLAG
};

/* The answer of an error handler */
typedef enum
{
   X_ERROR_SUBSTITUTE,
   X_ERROR_PROPAGATE,
   X_ERROR_DEFAULT
} X_ERROR_ACTION;

/* The flags of a hash */
enum
{
   X_HASH_CASEMATCH = 0x0001,
   X_HASH_AUTOADD = 0x0002
};

/* The modes of HMerge() and HCopy(), the same numbers as HASH_* in hash.ch */
enum
{
   X_HASH_UNION = 0,
   X_HASH_INTERSECT = 1,
   X_HASH_DIFFERENCE = 2,
   X_HASH_REMOVE = 3
};

/* The instance variables of the Error class, error.c */
enum
{
   X_ERR_ARGS = 1,
   X_ERR_CANDEFAULT,
   X_ERR_CANRETRY,
   X_ERR_CANSUBSTITUTE,
   X_ERR_CARGO,
   X_ERR_DESCRIPTION,
   X_ERR_FILENAME,
   X_ERR_GENCODE,
   X_ERR_OPERATION,
   X_ERR_OSCODE,
   X_ERR_SEVERITY,
   X_ERR_SUBCODE,
   X_ERR_SUBSYSTEM,
   X_ERR_TRIES
};

#define X_MATH_PI 3.14159265358979323846

/* A text as an array of character codes: the code point, or X_UCODE_BYTE + the byte for a broken byte */
#define X_UCODE_BYTE 0x110000

/* ---------------------------------------------------------------------------------------------
   Callbacks
   --------------------------------------------------------------------------------------------- */

typedef X_ERROR_ACTION ( *X_ERROR_HANDLER )( const X_ERROR *, X_ITEM *substitute );

typedef void ( *X_FUNCPTR )( X_CALL_CTX *ctx );
typedef void ( *X_BLOCKFUNC )( X_CALL_CTX *ctx, X_ITEM *const *env );
typedef void ( *X_GARBAGE_FUNC_PTR )( void *cargo );

/* ---------------------------------------------------------------------------------------------
   Structures
   --------------------------------------------------------------------------------------------- */

/* A value of xDev */
struct _X_ITEM
{
   X_TYPE nType;
   union
   {
      int logical;
      int64_t integer;
      uint64_t unsignedInteger;
      double dbl;
      X_STRING string;
      void *pArr;
      void *pBlock;
      void *pPointer;
   } u;
};

/* An argument of a call */
struct _X_ARG
{
   const X_ITEM *value;
   int byReference;
};

/* A variable shared by reference or captured by a block; native fields hold a C value of a DLL call */
struct _X_BOX
{
   X_ITEM item;
   size_t nRefs;
   X_ITEM *reference;
   X_BOX *owner;
   int nativeType;
   uint64_t nativeValue;
   void *nativeStorage;
   X_BOX *nativeNext;
};

/* The context of a call: the arguments, the result and the object of a method */
struct _X_CALL_CTX
{
   const X_ARG *args;
   int32_t argc;
   X_ITEM *result;
   X_ITEM *self;
   const void *methodContext;
};

/* The fixed description of a runtime error */
struct _X_ERROR_DEF
{
   int severity, genCode, subCode, canSubstitute;
   const char *subSystem, *description, *operation;
};

/* One frame of the call trace */
typedef struct _X_TRACE
{
   struct _X_TRACE *prev;
   const char *location;
   const char *function;
} X_TRACE;

/* A runtime error */
struct _X_ERROR
{
   const X_ERROR_DEF *definition;
   const char *location;
   const char *function;
   X_ITEM arguments[2];
   int argumentCount;
   int canDefault;
   int recoveryFailure, handlerFailed;
   X_ITEM object; /* the Error object of Throw(), NIL for an error of the runtime */
};

/* The functions of a garbage collected block */
struct _X_GC_FUNCS
{
   X_GARBAGE_FUNC_PTR release;
   X_GARBAGE_FUNC_PTR mark;
};

/* The statistics of x_xstats() */
typedef struct
{
   size_t currentBytes;
   size_t currentBlocks;
   size_t peakBytes;
   size_t peakBlocks;
   size_t totalAllocations;
   size_t totalReallocations;
} X_MEMORY_STATS;

/* A file put into the executable by xdev; its name is the file name without the extension */
typedef struct
{
   const char *name;
   const unsigned char *data;
   size_t size;
} X_RESOURCE;

/* MD5 (RFC 1321): x_md5Init(), x_md5Update() for every part of the data, then x_md5Final() */
typedef struct
{
   uint32_t state[4];
   uint64_t length;
   unsigned char buffer[64];
} X_MD5;

/* One token of a text */
typedef struct
{
   size_t start;
   size_t length;
} X_TOKEN_SPAN;

/* One UTF-8 character of an argument, such as a fill character */
typedef struct
{
   char bytes[4];
   size_t size;
} X_UCHAR;

/* ---------------------------------------------------------------------------------------------
   Calls, modules and the call trace
   --------------------------------------------------------------------------------------------- */

void x_traceEnter( X_TRACE *frame, const char *function );
void x_traceLeave( X_TRACE *frame );

void x_callCleanup( X_FUNCPTR function, X_CALL_CTX *ctx );
void x_callCatch( X_FUNCPTR function, X_CALL_CTX *ctx );
void x_callbackEnterDll( void );
void x_callbackLeaveDll( void );
int x_callbackInvoke( X_FUNCPTR function, X_ITEM *result, int32_t argc, const X_ARG *args );
void x_callbackPutString( X_ITEM *item, const char *value );
void x_callbackRaisePending( void );
void x_gcCleanup( X_GARBAGE_FUNC_PTR function, void *cargo );
void x_gcCatch( X_GARBAGE_FUNC_PTR function, void *cargo );
void x_moduleRegister( X_FUNCPTR statics, X_FUNCPTR init, X_FUNCPTR exit, X_FUNCPTR clear );
void x_commandArgsInit( int32_t argc, const char *const *argv );
void x_commandArgCount( X_CALL_CTX *ctx );
void x_commandArgValue( X_CALL_CTX *ctx, int64_t position );
void x_modulesInit( int32_t argc, const X_ARG *args );
void x_modulesExit( void );
void x_modulesClear( void );
int x_runtimeInit( void );
void x_runtimeExit( void );

/* ---------------------------------------------------------------------------------------------
   System, random numbers and math
   --------------------------------------------------------------------------------------------- */

/* System: the operating system behind Tone(), Memory(), NetName() and IsPrinter() */
void x_sysTone( double frequency, double ticks );
void x_sysSleep( double seconds );
double x_sysCpuTime( int which );
uint64_t x_sysMemoryAvail( void );
char *x_sysNetName( void );
char *x_sysUserName( void );
int x_sysPrinterReady( const char *port );

/* Random numbers: Random(), RandomInt() and RandomSeed(); seeded from the clock on first use */
void x_randomSeed( uint64_t seed );
void x_randomSeedTime( void );
double x_randomDouble( void );
int64_t x_randomInt( int64_t min, int64_t max );

/* Math: the shared part of Cos(), Floor(), DToR(), BitRotate() and the other numeric functions */
void x_mathRetnd( X_CALL_CTX *ctx, double value, const char *operation );
double x_mathDegToRad( double degrees );
double x_mathRadToDeg( double radians );
double x_mathRate( double loan, double payment, double periods );
uint64_t x_mathRotateLeft( uint64_t value, int bits, int64_t count );
uint64_t x_mathMirror( uint64_t value, int bits );
int x_mathDisplayLength( const X_ITEM *value );
void x_mathCounter( X_CALL_CTX *ctx );
int x_mathPrecision( void );
void x_mathSetPrecision( int decimals );

/* Math: the bodies of Abs(), Str(), Val() and the other numeric functions of the API */
void x_mathAbs( X_CALL_CTX *ctx );
void x_mathBitShift( X_CALL_CTX *ctx );
void x_mathBitToC( X_CALL_CTX *ctx );
void x_mathCToBit( X_CALL_CTX *ctx );
void x_mathCToN( X_CALL_CTX *ctx );
void x_mathFact( X_CALL_CTX *ctx );
void x_mathInt( X_CALL_CTX *ctx );
void x_mathMax( X_CALL_CTX *ctx );
void x_mathMin( X_CALL_CTX *ctx );
void x_mathMod( X_CALL_CTX *ctx );
void x_mathNToC( X_CALL_CTX *ctx );
void x_mathPeriods( X_CALL_CTX *ctx );
void x_mathRandom( X_CALL_CTX *ctx );
void x_mathRandomInt( X_CALL_CTX *ctx );
void x_mathRound( X_CALL_CTX *ctx );
void x_mathSign( X_CALL_CTX *ctx );
void x_mathStr( X_CALL_CTX *ctx );
void x_mathStrZero( X_CALL_CTX *ctx );
void x_mathVal( X_CALL_CTX *ctx );

/* ---------------------------------------------------------------------------------------------
   Memory
   --------------------------------------------------------------------------------------------- */

/* x_xalloc() returns NULL on failure; x_xgrab() and its variants abort on failure.
   Zero-length allocations return a valid releasable block. Array variants reject
   multiplication overflow. Only pointers from this family may be resized or released. */
void *x_xalloc( size_t bytes );
void *x_xallocArray( size_t count, size_t itemSize );
void x_xfree( void *area );
void *x_xgrab( size_t bytes );
void *x_xgrabArray( size_t count, size_t itemSize );
void *x_xgrabz( size_t bytes );
void *x_xgrabzArray( size_t count, size_t itemSize );
void *x_xmemdup( const void *source, size_t bytes );

/* A zero size releases area and returns NULL; a NULL area allocates a new block. */
void *x_xrealloc( void *area, size_t bytes );
void *x_xreallocArray( void *area, size_t count, size_t itemSize );
void *x_xreallocz( void *area, size_t bytes );

/* Shared blocks: x_xretain() adds an owner, x_xrelease() drops one and frees the block with
   the last owner. A shared block cannot be resized or released by x_xfree(). */
size_t x_xrefs( const void *area );
void x_xrelease( void *area );
void *x_xretain( void *area );
size_t x_xsize( const void *area );
void x_xstats( X_MEMORY_STATS *stats );

/* Garbage collector */
void *x_gcAllocate( size_t size, const X_GC_FUNCS *functions );
void x_gcFree( void *cargo );
void *x_gcLock( void *cargo );
void *x_gcUnlock( void *cargo );
void x_gcCollect( void );
void x_gcSuspend( void );
void x_gcResume( void );
void x_gcCollectClasses( void );
int x_gcIsDeadBlock( const X_ITEM *item );
int x_gcIsBusy( void );
int x_gcHasObjects( void );
void x_gcMark( void *cargo );
void x_gcMarkItem( const X_ITEM *item );
void x_gcDummyMark( void *cargo );
int x_gcBeginDestructor( const X_ITEM *object );
void x_gcEndDestructor( const X_ITEM *object );

/* ---------------------------------------------------------------------------------------------
   Files
   --------------------------------------------------------------------------------------------- */

/* The bodies of the file functions of the API */
void x_fsADirCall( X_CALL_CTX *ctx );
void x_fsBase64DecodeFileCall( X_CALL_CTX *ctx );
void x_fsBase64EncodeFileCall( X_CALL_CTX *ctx );
void x_fsCurDirCall( X_CALL_CTX *ctx );
void x_fsCurDirXCall( X_CALL_CTX *ctx );
void x_fsCurDriveCall( X_CALL_CTX *ctx );
void x_fsCwdCall( X_CALL_CTX *ctx );
void x_fsDeleteFileCall( X_CALL_CTX *ctx );
void x_fsDirExistsCall( X_CALL_CTX *ctx );
void x_fsDirNameCall( X_CALL_CTX *ctx );
void x_fsDiskChangeCall( X_CALL_CTX *ctx );
void x_fsDiskFreeCall( X_CALL_CTX *ctx );
void x_fsDiskNameCall( X_CALL_CTX *ctx );
void x_fsDiskSpaceCall( X_CALL_CTX *ctx );
void x_fsDiskTotalCall( X_CALL_CTX *ctx );
void x_fsDiskUsedCall( X_CALL_CTX *ctx );
void x_fsExeNameCall( X_CALL_CTX *ctx );
void x_fileAppendCall( X_CALL_CTX *ctx );
void x_fileAttrCall( X_CALL_CTX *ctx );
void x_fileAttrGetCall( X_CALL_CTX *ctx );
void x_fileConfigCall( X_CALL_CTX *ctx );
void x_fileCopyExCall( X_CALL_CTX *ctx );
void x_fileDateCall( X_CALL_CTX *ctx );
void x_fileExistsCall( X_CALL_CTX *ctx );
void x_fileIsLocalCall( X_CALL_CTX *ctx );
void x_fileLinkReadCall( X_CALL_CTX *ctx );
void x_fileLoadCall( X_CALL_CTX *ctx );
void x_fileNameExistsCall( X_CALL_CTX *ctx );
void x_fileOpenCall( X_CALL_CTX *ctx );
void x_fileReadCall( X_CALL_CTX *ctx );
void x_fileReadAtCall( X_CALL_CTX *ctx );
void x_fileReadLenCall( X_CALL_CTX *ctx );
void x_fileStrCall( X_CALL_CTX *ctx );
void x_fileTempCall( X_CALL_CTX *ctx );
void x_fileTimeCall( X_CALL_CTX *ctx );
void x_fileTimeGetCall( X_CALL_CTX *ctx );
void x_fileWriteCall( X_CALL_CTX *ctx );
void x_fileWriteAtCall( X_CALL_CTX *ctx );
void x_fsFNameSplitCall( X_CALL_CTX *ctx );
void x_fsFReadCall( X_CALL_CTX *ctx );
void x_fsFReadStrCall( X_CALL_CTX *ctx );
void x_fsFWriteCall( X_CALL_CTX *ctx );
void x_fsMD5FileCall( X_CALL_CTX *ctx );
void x_fsMemoReadCall( X_CALL_CTX *ctx );
void x_fsMemoWritCall( X_CALL_CTX *ctx );
void x_fsRenameFileCall( X_CALL_CTX *ctx );
void x_fsSetFAttrCall( X_CALL_CTX *ctx );
void x_fsSetFDaTiCall( X_CALL_CTX *ctx );
void x_fsStrFileCall( X_CALL_CTX *ctx );
void x_fsTrueNameCall( X_CALL_CTX *ctx );
void x_fsCopyCommandCall( X_CALL_CTX *ctx );
void x_fsTypeCommandCall( X_CALL_CTX *ctx );
void x_fsCommitCall( X_CALL_CTX *ctx );
void x_fsTempCreateCall( X_CALL_CTX *ctx );
void x_fsNameMergeCall( X_CALL_CTX *ctx );
void x_fsReadLineCall( X_CALL_CTX *ctx );
void x_fsCountCall( X_CALL_CTX *ctx, int mode );
void x_fsParseLineCall( X_CALL_CTX *ctx );
void x_fsStatsCall( X_CALL_CTX *ctx );
void x_fsUuCall( X_CALL_CTX *ctx, int decode );
void x_arrayReverseScanCall( X_CALL_CTX *ctx );
void x_strDelete( X_CALL_CTX *ctx );
size_t x_textLines( const char *text, size_t length, size_t width, int longest );
uint32_t x_textCheckSum( const char *text, size_t length, int crc );
char *x_uuEncode( const char *data, size_t length, size_t *resultLength );
char *x_uuDecode( const char *data, size_t length, size_t *resultLength );
void x_uuResult( X_CALL_CTX *ctx, int decode );
void x_fileAreasExit( void );
void x_fileTextCall( X_CALL_CTX *ctx, int operation );
void x_fileHandleEofCall( X_CALL_CTX *ctx );
void x_fileParseCall( X_CALL_CTX *ctx );
void x_fileReadIniCall( X_CALL_CTX *ctx );
void x_fileWriteIniCall( X_CALL_CTX *ctx );
void x_fileIniCommentsCall( X_CALL_CTX *ctx );
void x_textReadLineCall( X_CALL_CTX *ctx );

/* Operating system files and directories */
int64_t x_fsOpen( const char *path, int mode );
int64_t x_fsCreate( const char *path, int attr );
int x_fsClose( int64_t handle );
int x_fsCurDrv( void );
int x_fsChDrv( int drive );
int64_t x_fsDiskSpace( int drive );
int x_fsError( void );
void x_fsSetError( int error );
size_t x_fsRead( int64_t handle, void *buffer, size_t count );
size_t x_fsWrite( int64_t handle, const void *buffer, size_t count );
int64_t x_fsSeek( int64_t handle, int64_t offset, int mode );
int x_fsDelete( const char *path );
char *x_fsCurDir( int drive );
int x_fsDirChange( const char *path );
int x_fsDirMake( const char *path );
int x_fsDirBuild( const char *path );
int x_fsDirUnbuild( const char *path );
int x_fsDirRemove( const char *path );
int64_t x_fsDirSpace( const char *path, int type );
int x_fsRename( const char *source, const char *target );
int x_fsLink( const char *source, const char *target );
int x_fsLinkSym( const char *target, const char *name, int isDir );
int x_fsExists( const char *path, int *isFile, int *isDir );

/* File objects of FileOpen() */
extern const X_GC_FUNCS x_fileGC;

void *x_fileOpen( const char *path, int mode );
void *x_fileCreate( const char *path, int attr );
int x_fileClose( const X_ITEM *item );
int64_t x_fileRead( const X_ITEM *item, void *buffer, size_t count, int64_t timeout );
int64_t x_fileReadAt( const X_ITEM *item, void *buffer, size_t count, int64_t offset );
int64_t x_fileWrite( const X_ITEM *item, const void *buffer, size_t count, int64_t timeout );
int64_t x_fileWriteAt( const X_ITEM *item, const void *buffer, size_t count, int64_t offset );
int64_t x_fileSeek( const X_ITEM *item, int64_t offset, int mode );
int64_t x_fileHandle( const X_ITEM *item );
int x_fileTrunc( const X_ITEM *item, int64_t offset );
int64_t x_fileSize( const X_ITEM *item, int dirEntry );
int x_fileEof( const X_ITEM *item );
int x_fileCommit( const X_ITEM *item );
int x_fileLock( const X_ITEM *item, int64_t start, int64_t length, int type, int unlock );
char *x_fileLoad( const char *path, size_t limit, size_t *length );

/* Files put into the executable */
extern const X_RESOURCE x_resources[];
const unsigned char *x_resourceFind( const char *name, size_t *length );

/* Files: FileStr(), StrFile(), FileDate(), SetFDaTi(), FileDelete(), FileValid(), TrueName()... */
int x_fileSave( const char *path, const char *buffer, size_t length );
int x_fileRename( const char *source, const char *target );
int x_fileCopy( const char *source, const char *target );
int x_fileCopyEx( X_CALL_CTX *ctx, const char *source, const char *target, size_t size, int keepTime,
                  const X_ITEM *block );
int x_fileMove( const char *source, const char *target );
char *x_fsLinkRead( const char *path );
int64_t x_fileLockTest( const X_ITEM *item, int64_t start, int64_t length, int type );
void *x_fileTemp( char **name, const char *dir, const char *prefix, const char *ext, int attr );
void x_fileDirectory( X_ITEM *result, const char *spec, const char *attr );
void x_fileDirectoryRecurse( X_ITEM *result, const char *spec, const char *attr );
int x_fsDirRemoveAll( const char *path );
char *x_fsCurDirX( int drive );
void x_fsNameSplit( const char *text, size_t length, size_t *pathLength, size_t *nameLength );
int x_fsTimeGet( const char *path, int64_t *timeMs );
int x_fsTimeSet( const char *path, int64_t timeMs );
int x_fsAttrGet( const char *path, uint32_t *attr );
int x_fsErrorCode( void );
char *x_fsReadPart( const char *path, int64_t offset, int64_t count, size_t *length );
int64_t x_fsWritePart( const char *path, const char *data, size_t length, int useExisting, int64_t offset,
                       int truncate );
int x_fsTimeLocal( int64_t timeMs, long *julian, int *seconds );
int x_fsTimeText( int64_t timeMs, char *buffer, size_t size );
int x_fsTimeLocalGet( const char *path, long *julian, int *seconds );
int x_fsTimeLocalSet( const char *path, long julian, int seconds );
int64_t x_fsDeleteMask( const char *mask, int attr );
int x_fsNameValid( const char *name, size_t length, int64_t maxName, int64_t maxExt, int noExt, int spaces );
char *x_fsTrueName( const char *path );
char *x_fsExeName( void );
char *x_fsCurDirFull( int drive );
const char *x_fsDiskPath( const char *text, char drive[4] );
int x_fsAttrSet( const char *path, uint32_t attr );

/* ---------------------------------------------------------------------------------------------
   Strings
   --------------------------------------------------------------------------------------------- */

const char *x_eol( void );
const char *x_eolDetect( const char *text, size_t length );
uint16_t x_u16Ctrl( uint16_t character );
size_t x_utf8CharSize( uint32_t codepoint );
size_t x_utf8Encode( char *destination, uint32_t codepoint );

/* Decode one scalar and advance offset. Invalid input produces U+FFFD and returns zero. */
int x_utf8Decode( const char *source, size_t length, size_t *offset, uint32_t *codepoint );
int x_utf8Validate( const char *source, size_t length );
uint32_t x_utf8UpperChar( uint32_t codepoint );
uint32_t x_utf8LowerChar( uint32_t codepoint );
size_t x_utf8Case( char *destination, const char *source, size_t length, int upper );
int x_utf8IsAlpha( const char *text, size_t length );
int x_utf8IsAlnum( const char *text, size_t length );
int x_utf8IsUpper( const char *text, size_t length );
int x_utf8IsLower( const char *text, size_t length );
void x_utf8Chr( X_CALL_CTX *ctx, double number );
void x_utf8Asc( X_CALL_CTX *ctx, const char *text, size_t length );
void x_utf8Peek( X_CALL_CTX *ctx, const char *text, size_t length, double position );
void x_utf8Poke( X_CALL_CTX *ctx, const char *text, size_t length, double position, double number );
size_t x_utf8Length( const char *source, size_t length );
size_t x_utf8Offset( const char *source, size_t length, size_t count );
void x_utf8SubStr( X_CALL_CTX *ctx, const char *text, size_t length, int64_t start, int hasCount, int64_t count );
void x_utf8Left( X_CALL_CTX *ctx, const char *text, size_t length, int64_t count );
void x_utf8Right( X_CALL_CTX *ctx, const char *text, size_t length, int64_t count );
void x_utf8Pad( X_CALL_CTX *ctx, const char *text, size_t length, int64_t width, const char *fill, size_t fillLength,
                int leftPad, int rightPad, const char *operation );
int64_t x_utf8At( const char *search, size_t searchLength, const char *text, size_t length, int64_t start,
                  int hasStart, int64_t end, int hasEnd );
void x_utf8RAt( X_CALL_CTX *ctx, const char *search, size_t searchLength, const char *text, size_t length,
                int64_t start, int hasStart, int64_t end, int hasEnd );
void x_utf8Stuff( X_CALL_CTX *ctx, const char *text, size_t length, int64_t start, int64_t remove,
                  const char *insert, size_t insertLength );
uint32_t x_binUInt32( const char *text, size_t length );
double x_binFloat32( const char *text, size_t length );
double x_binFloat64( const char *text, size_t length );
void x_binRetUInt16( X_CALL_CTX *ctx, uint16_t value );
void x_binRetUInt32( X_CALL_CTX *ctx, uint32_t value );
void x_binRetFloat32( X_CALL_CTX *ctx, double value );
void x_binRetFloat64( X_CALL_CTX *ctx, double value );

/* Convert to terminal cells; non-BMP scalars become U+FFFD. */
size_t x_utf8ToU16( const char *source, size_t length, uint16_t *destination, size_t capacity );

/* Hex and Base64; the results are allocated by x_xgrab() and end with a zero byte. */
char *x_hexEncode( const char *data, size_t length, const char *separator, size_t separatorLength,
                   size_t *resultLength );
char *x_hexDecode( const char *text, size_t length, size_t *resultLength );
void x_hexToNum( X_CALL_CTX *ctx );
void x_numByte( X_CALL_CTX *ctx, int high );
void x_numMirror( X_CALL_CTX *ctx, int argument, int bits, uint64_t maximum, const char *operation );
void x_stringByte( X_CALL_CTX *ctx, int write );
char *x_numToHex( uint64_t value, int width, size_t *resultLength );
char *x_valToCStr( const X_ITEM *item, size_t *length );
char *x_valToPrgExp( const X_ITEM *item, size_t *length );
void x_textXToC( X_CALL_CTX *ctx );
char *x_base64Encode( const char *data, size_t length, size_t *resultLength );
char *x_base64Decode( const char *text, size_t length, size_t *resultLength );

/* MD5 */
void x_md5Init( X_MD5 *md5 );
void x_md5Update( X_MD5 *md5, const void *data, size_t length );
void x_md5Final( X_MD5 *md5, char hex[33] );

/* CharSort(), RangeRem() and RangeRepl() */
void x_charSort( uint32_t *result, const uint32_t *text, size_t skip, size_t count, size_t element, size_t compare,
                 size_t offset, int descending );
uint32_t x_rangeBound( X_CALL_CTX *ctx, int n );
int x_rangeHas( uint32_t first, uint32_t last, uint32_t c );

/* ---------------------------------------------------------------------------------------------
   Tokens: Token() and its family, and the incremental tokenizer of TokenInit() and TokenNext()
   --------------------------------------------------------------------------------------------- */

void x_tokenDelimiters( const char *list, size_t length, unsigned char isDelimiter[256] );
size_t x_tokenScan( const char *text, size_t length, const unsigned char isDelimiter[256], size_t skip,
                    X_TOKEN_SPAN *tokens );
X_TOKEN_SPAN *x_tokenList( const char *text, size_t length, const unsigned char isDelimiter[256], size_t skip,
                           size_t *count );
void x_tokenCase( char *text, size_t length, const unsigned char isDelimiter[256], size_t skip, int64_t count,
                  int upper );
void x_tokenSepFind( const char *text, size_t length, const unsigned char isDelimiter[256],
                     const X_TOKEN_SPAN *token );
size_t x_tokenSep( int after, const char **separator );
char *x_tokenEnvBuild( const char *text, size_t length, const unsigned char isDelimiter[256], size_t skip,
                       size_t *envLength );
int x_tokenEnvValid( const char *env, size_t length );
char *x_tokenEnvLoad( X_CALL_CTX *ctx, int n, size_t *length, int *owned );
void x_tokenEnvSave( X_CALL_CTX *ctx, int n, char *env, size_t length, int owned );
void x_tokenEnvRelease( char *env, int owned );
const char *x_tokenEnvGlobal( size_t *length );
void x_tokenEnvSetGlobal( char *env, size_t length );
uint64_t x_tokenEnvCount( const char *env );
uint64_t x_tokenEnvNext( const char *env );
void x_tokenEnvSetNext( char *env, uint64_t next );
const char *x_tokenEnvToken( const char *env, uint64_t index, size_t *start, size_t *length );

/* The bodies of Token(), TokenInit(), TokenNext() and the other token functions of the API */
void x_tokenAtToken( X_CALL_CTX *ctx );
void x_tokenRestToken( X_CALL_CTX *ctx );
void x_tokenToken( X_CALL_CTX *ctx );
void x_tokenTokenAt( X_CALL_CTX *ctx );
void x_tokenTokenEnd( X_CALL_CTX *ctx );
void x_tokenTokenInit( X_CALL_CTX *ctx );
void x_tokenTokenLower( X_CALL_CTX *ctx );
void x_tokenTokenNext( X_CALL_CTX *ctx );
void x_tokenTokenUpper( X_CALL_CTX *ctx );

/* ---------------------------------------------------------------------------------------------
   UTF-16 and C strings
   --------------------------------------------------------------------------------------------- */

/* A NULL destination or zero capacity measures the complete converted result. */
size_t x_utf8ToUtf16( const char *source, size_t length, uint16_t *destination, size_t capacity );
size_t x_utf16ToUtf8( const uint16_t *source, size_t length, char *destination, size_t capacity );
uint16_t *x_utf8ToUtf16Alloc( const char *source, size_t length, size_t *resultLength );
char *x_utf16ToUtf8Alloc( const uint16_t *source, size_t length, size_t *resultLength );
size_t x_inkeyKeyText( int key, char *text );
int x_inkeyStartsWith( int key, const char *text, size_t length );
size_t x_memoNext( const char *text, size_t len, size_t *pos, size_t width, size_t tab, int wrap, char *buffer,
                   size_t *used );
int x_stricmp( const char *s1, const char *s2 );
int x_strnicmp( const char *s1, const char *s2, size_t length );
char *x_strupr( char *text );
char *x_strlow( char *text );
char *x_strdup( const char *text );
char *x_strndup( const char *text, size_t length );
char *x_strduptrim( const char *text );
size_t x_strlentrim( const char *text );
size_t x_strnlen( const char *text, size_t length );
int x_strEmpty( const char *text, size_t length );
size_t x_strAt( const char *sub, size_t subLen, const char *text, size_t textLen );
size_t x_strRAt( const char *sub, size_t subLen, const char *text, size_t textLen );
size_t x_strAtI( const char *sub, size_t subLen, const char *text, size_t textLen );
char *x_strncpy( char *dest, const char *source, size_t length );
char *x_strncat( char *dest, const char *source, size_t length );
char *x_strncpyTrim( char *dest, const char *source, size_t length );
char *x_strncpyLower( char *dest, const char *source, size_t length );
char *x_strncpyUpper( char *dest, const char *source, size_t length );
char *x_strncpyUpperTrim( char *dest, const char *source, size_t length );
const char *x_strLTrim( const char *text, size_t *length );
size_t x_strRTrimLen( const char *text, size_t length, int anySpace );

/* ---------------------------------------------------------------------------------------------
   Dates and SET values
   --------------------------------------------------------------------------------------------- */

void x_dateToday( int *year, int *month, int *day );
long x_dateEncode( int year, int month, int day );
void x_dateDecode( long julian, int *year, int *month, int *day );
int x_dateDOW( int year, int month, int day );
int x_dateJulianDOW( long julian );
const char *x_dateCMonth( int month );
const char *x_dateCDOW( int day );

extern int x_setFixed;
extern int x_setExclusive;
extern int x_setDeleted;
extern int x_setSoftSeek;
extern int x_setUnique;
extern int x_setCancel;
extern int x_setInsert;
extern int x_setExit;
extern int x_setConfirm;
extern int x_setEscape;
extern int x_setDecimals;
extern int x_setEpoch;
extern int x_prnRow;
extern int x_prnCol;
extern int x_dispOutAtSetPos;

const char *x_setPath( const char *path );
int x_fileFind( const char *spec );
const char *x_setDefault( const char *path );
char *x_fileDefName( const char *name );
const char *x_setDateFormat( const char *format );
int x_setCentury( int century );
void x_dateFormat( const char *date, char *buffer, const char *format );
long x_dateUnformat( const char *text, const char *format );

/* ---------------------------------------------------------------------------------------------
   Items
   --------------------------------------------------------------------------------------------- */

/* Lifetime and assignment. */
void x_itemClear( X_ITEM *item );
void x_itemCopy( X_ITEM *destination, const X_ITEM *source );
int x_itemAssign( X_CALL_CTX *ctx, X_ITEM *destination, const X_ITEM *source );
X_ITEM *x_itemNew( const X_ITEM *source );
void x_itemRelease( X_ITEM *item );
void x_itemReturn( X_CALL_CTX *ctx, const X_ITEM *item );
X_ITEM *x_stackSelfItem( X_CALL_CTX *ctx );

/* Typed scalar values. Put functions allocate an item when item is NULL. */
X_ITEM *x_itemPutL( X_ITEM *item, int value );
X_ITEM *x_itemPutNI( X_ITEM *item, int value );
X_ITEM *x_itemPutNInt( X_ITEM *item, int64_t value );
X_ITEM *x_itemPutND( X_ITEM *item, double value );
int x_itemGetL( const X_ITEM *item );
int x_itemGetNI( const X_ITEM *item );
int64_t x_itemGetNInt( const X_ITEM *item );
double x_itemGetND( const X_ITEM *item );

/* UTF-8 strings. PutC/PutCL copy; PutCPtr/PutCLPtr take ownership of an x_xgrab buffer. */
X_ITEM *x_itemPutC( X_ITEM *item, const char *value );
X_ITEM *x_itemPutCL( X_ITEM *item, const char *value, size_t length );
X_ITEM *x_itemPutCPtr( X_ITEM *item, char *value );
X_ITEM *x_itemPutCLPtr( X_ITEM *item, char *value, size_t length );
X_ITEM *x_itemPutStrUTF8( X_ITEM *item, const char *value );
const char *x_itemGetCPtr( const X_ITEM *item );
size_t x_itemGetCLen( const X_ITEM *item );
const char *x_itemGetStrUTF8( const X_ITEM *item, void **handle, size_t *length );
void x_strfree( void *handle );
void x_itemPutStringBuffer( X_ITEM *item, char *value, size_t length );

/* Raw pointers are non-owning; GC pointers retain managed cargo. */
X_ITEM *x_itemPutPtr( X_ITEM *item, void *value );
X_ITEM *x_itemPutPtrGC( X_ITEM *item, void *cargo );
void *x_itemGetPtr( const X_ITEM *item );
void *x_itemGetPtrGC( const X_ITEM *item, const X_GC_FUNCS *functions );

size_t x_itemStringLength( const X_ITEM *item );
void x_transform( X_ITEM *result, const X_ITEM *value, const char *pic, size_t picLen );
int x_itemLogical( const X_ITEM *value, int *result );

/* ---------------------------------------------------------------------------------------------
   Boxes and blocks
   --------------------------------------------------------------------------------------------- */

X_BOX *x_boxNew( void );
void x_boxRetain( X_BOX *box );
void x_boxRelease( X_BOX *box );
X_BOX *x_boxCapture( const X_ITEM *item );
X_ITEM *x_boxItem( X_BOX *box );
void x_boxBind( X_BOX *box, const X_ITEM *value, int byReference );
void x_boxLeave( X_BOX *box );
X_BOX *x_boxNativeNew( int type );
void *x_boxNativeData( X_BOX *box );
void x_boxNativeStore( X_BOX *box, uint64_t value );
void x_boxNativeBind( X_BOX *box, const X_ITEM *value, int byReference );
void x_boxNativeStorage( X_BOX *box, void *storage );
void x_stringStore( X_STRING *storage, const char *data, size_t length );
int x_stringAppend( X_STRING *storage, const char *data, size_t length );
void x_boxNativeStringStore( X_BOX *box, const char *data, size_t length );
void x_itemBlockInto( X_CALL_CTX *ctx, X_ITEM *destination, X_BLOCKFUNC function, X_ITEM *const *env, int envLen );
void x_itemClosureInto( X_CALL_CTX *ctx, X_ITEM *destination, X_BLOCKFUNC function, X_ITEM *const *env,
                        int envLen );
void x_evalBlock( X_CALL_CTX *ctx, X_ITEM *result, const X_ITEM *block, int32_t argc, const X_ARG *args );

/* ---------------------------------------------------------------------------------------------
   Parameters
   --------------------------------------------------------------------------------------------- */

int x_pcount( X_CALL_CTX *ctx );

X_TYPE x_parinfo( X_CALL_CTX *ctx, int n );
size_t x_parinfa( X_CALL_CTX *ctx, int n, size_t index );

const X_ITEM *x_param( X_CALL_CTX *ctx, int n, X_TYPE type );
const X_ITEM *x_paramError( X_CALL_CTX *ctx, int n );

int x_extIsNil( X_CALL_CTX *ctx, int n );
int x_extIsObject( X_CALL_CTX *ctx, int n );

const X_ITEM *x_paritem( X_CALL_CTX *ctx, int n );

const char *x_parc( X_CALL_CTX *ctx, int n );
const char *x_parcx( X_CALL_CTX *ctx, int n );
size_t x_parclen( X_CALL_CTX *ctx, int n );
size_t x_parcsiz( X_CALL_CTX *ctx, int n );

const char *x_pards( X_CALL_CTX *ctx, int n );
char *x_pardsbuff( X_CALL_CTX *ctx, char *date, int n );
long x_pardl( X_CALL_CTX *ctx, int n );

int x_parl( X_CALL_CTX *ctx, int n );
int x_parldef( X_CALL_CTX *ctx, int n, int defValue );

double x_parnd( X_CALL_CTX *ctx, int n );

int x_parni( X_CALL_CTX *ctx, int n );
int x_parnidef( X_CALL_CTX *ctx, int n, int defValue );

long x_parnl( X_CALL_CTX *ctx, int n );
long x_parnldef( X_CALL_CTX *ctx, int n, long defValue );

long long x_parnll( X_CALL_CTX *ctx, int n );

ptrdiff_t x_parns( X_CALL_CTX *ctx, int n );
ptrdiff_t x_parnsdef( X_CALL_CTX *ctx, int n, ptrdiff_t defValue );

int64_t x_parnint( X_CALL_CTX *ctx, int n );
uint64_t x_paruint64( X_CALL_CTX *ctx, int n );
int64_t x_parnintdef( X_CALL_CTX *ctx, int n, int64_t defValue );

void *x_parptr( X_CALL_CTX *ctx, int n );

/* ---------------------------------------------------------------------------------------------
   Return values
   --------------------------------------------------------------------------------------------- */

void x_consoleOut( X_CALL_CTX *ctx, int newLine, int stream );
void x_ret( X_CALL_CTX *ctx );

void x_retc( X_CALL_CTX *ctx, const char *value );
void x_retc_null( X_CALL_CTX *ctx );
void x_retc_buffer( X_CALL_CTX *ctx, char *value );

void x_retclen( X_CALL_CTX *ctx, const char *value, size_t length );
void x_retclen_buffer( X_CALL_CTX *ctx, char *value, size_t length );

void x_retds( X_CALL_CTX *ctx, const char *date );
void x_retd( X_CALL_CTX *ctx, int year, int month, int day );
void x_retdl( X_CALL_CTX *ctx, long julian );

void x_retl( X_CALL_CTX *ctx, int value );

void x_retnd( X_CALL_CTX *ctx, double value );
void x_retni( X_CALL_CTX *ctx, int value );
void x_retnl( X_CALL_CTX *ctx, long value );
void x_retnll( X_CALL_CTX *ctx, long long value );
void x_retns( X_CALL_CTX *ctx, ptrdiff_t value );
void x_retnint( X_CALL_CTX *ctx, int64_t value );
void x_retuint64( X_CALL_CTX *ctx, uint64_t value );

void x_reta( X_CALL_CTX *ctx, size_t length );

void x_retptr( X_CALL_CTX *ctx, void *ptr );
void x_retptrGC( X_CALL_CTX *ctx, void *ptr );

/* ---------------------------------------------------------------------------------------------
   Store API (write to a call parameter, including @/X_IT_BYREF)
   --------------------------------------------------------------------------------------------- */

int x_stor( X_CALL_CTX *ctx, int n );
int x_storc( X_CALL_CTX *ctx, const char *value, int n );
int x_storclen( X_CALL_CTX *ctx, const char *value, size_t length, int n );
int x_storclen_buffer( X_CALL_CTX *ctx, char *value, size_t length, int n );
int x_stords( X_CALL_CTX *ctx, const char *date, int n );
int x_stordl( X_CALL_CTX *ctx, long julian, int n );
int x_storl( X_CALL_CTX *ctx, int value, int n );
int x_storni( X_CALL_CTX *ctx, int value, int n );
int x_stornl( X_CALL_CTX *ctx, long value, int n );
int x_stornll( X_CALL_CTX *ctx, long long value, int n );
int x_storns( X_CALL_CTX *ctx, ptrdiff_t value, int n );
int x_stornd( X_CALL_CTX *ctx, double value, int n );
int x_stornint( X_CALL_CTX *ctx, int64_t value, int n );
int x_storuint64( X_CALL_CTX *ctx, uint64_t value, int n );
int x_storptr( X_CALL_CTX *ctx, void *ptr, int n );
int x_storptrGC( X_CALL_CTX *ctx, void *ptr, int n );

/* ---------------------------------------------------------------------------------------------
   Arrays
   --------------------------------------------------------------------------------------------- */

int x_arrayNew( X_ITEM *dest, int64_t length );
X_ITEM *x_itemArrayNew( size_t length );
int64_t x_arrayLen( const X_ITEM *array );
int x_arrayIsObject( const X_ITEM *array );
uint32_t x_arrayClass( const X_ITEM *array );
int x_arraySetClass( X_ITEM *array, uint32_t classHandle );
void *x_arrayId( const X_ITEM *array );
void x_arrayBlock( X_CALL_CTX *ctx, const X_ITEM *array, const X_ITEM *index );
void x_arrayParams( X_ITEM *result, int32_t argc, const X_ARG *args );
void x_arrayClone( X_ITEM *result, const X_ITEM *source );
void x_arrayEval( X_CALL_CTX *ctx, const X_ITEM *array, const X_ITEM *block, int64_t start, int64_t count );
int64_t x_arrayScan( X_CALL_CTX *ctx, const X_ITEM *array, const X_ITEM *value, int64_t start, int64_t count,
                     int exact );
void x_arraySort( X_CALL_CTX *ctx, const X_ITEM *array, const X_ITEM *block, int64_t start, int64_t count );
size_t x_arrayRefs( const X_ITEM *array );
int x_arrayAdd( X_ITEM *array, const X_ITEM *value );
int x_arrayIns( X_ITEM *array, size_t index );
int x_arrayDel( X_ITEM *array, size_t index );
int x_arraySize( X_ITEM *array, size_t length );
int x_arrayLast( const X_ITEM *array, X_ITEM *result );
X_ITEM *x_arrayGetItemPtr( const X_ITEM *array, size_t index );
int x_arrayGet( const X_ITEM *array, size_t index, X_ITEM *result );
X_TYPE x_arrayGetType( const X_ITEM *array, size_t index );
char *x_arrayGetC( const X_ITEM *array, size_t index );
const char *x_arrayGetCPtr( const X_ITEM *array, size_t index );
size_t x_arrayGetCLen( const X_ITEM *array, size_t index );
const char *x_arrayGetStrUTF8( const X_ITEM *array, size_t index, void **handle, size_t *length );
int x_arrayGetL( const X_ITEM *array, size_t index );
int x_arrayGetNI( const X_ITEM *array, size_t index );
long x_arrayGetNL( const X_ITEM *array, size_t index );
long long x_arrayGetNLL( const X_ITEM *array, size_t index );
ptrdiff_t x_arrayGetNS( const X_ITEM *array, size_t index );
int64_t x_arrayGetNInt( const X_ITEM *array, size_t index );
double x_arrayGetND( const X_ITEM *array, size_t index );
void *x_arrayGetPtr( const X_ITEM *array, size_t index );
int x_arraySet( X_ITEM *array, size_t index, const X_ITEM *value );
int x_arraySetL( X_ITEM *array, size_t index, int value );
int x_arraySetNI( X_ITEM *array, size_t index, int value );
int x_arraySetNL( X_ITEM *array, size_t index, long value );
int x_arraySetNLL( X_ITEM *array, size_t index, long long value );
int x_arraySetNS( X_ITEM *array, size_t index, ptrdiff_t value );
int x_arraySetNInt( X_ITEM *array, size_t index, int64_t value );
int x_arraySetND( X_ITEM *array, size_t index, double value );
int x_arraySetC( X_ITEM *array, size_t index, const char *value );
int x_arraySetCL( X_ITEM *array, size_t index, const char *value, size_t length );
int x_arraySetStrUTF8( X_ITEM *array, size_t index, const char *value );
int x_arraySetPtr( X_ITEM *array, size_t index, void *ptr );
int x_itemArrayGet( X_CALL_CTX *ctx, X_ITEM *result, const X_ITEM *array, const X_ITEM *index );
int x_itemArraySet( X_CALL_CTX *ctx, X_ITEM *result, const X_ITEM *array, const X_ITEM *index,
                    const X_ITEM *value );

/* The bodies of ACopy(), Array(), ASort() and the other array functions of the API */
void x_arrayACopy( X_CALL_CTX *ctx );
void x_arrayADel( X_CALL_CTX *ctx );
void x_arrayAEval( X_CALL_CTX *ctx );
void x_arrayAFill( X_CALL_CTX *ctx );
void x_arrayAIns( X_CALL_CTX *ctx );
void x_arrayArray( X_CALL_CTX *ctx );
void x_arrayASort( X_CALL_CTX *ctx );

/* ---------------------------------------------------------------------------------------------
   Hashes
   --------------------------------------------------------------------------------------------- */

int x_hashNew( X_ITEM *dest );
size_t x_hashLen( const X_ITEM *hashItem );
int x_hashIsKey( const X_ITEM *key );
X_ITEM *x_hashGetItemPtr( const X_ITEM *hashItem, const X_ITEM *key );
size_t x_hashPos( const X_ITEM *hashItem, const X_ITEM *key );
int x_hashAdd( X_ITEM *hashItem, const X_ITEM *keyItem, const X_ITEM *valueItem );
int x_hashDel( X_ITEM *hashItem, const X_ITEM *key );
int x_hashDelAt( X_ITEM *hashItem, size_t pos );
void x_hashClear( X_ITEM *hashItem );
X_ITEM *x_hashKeyAt( const X_ITEM *hashItem, size_t pos );
X_ITEM *x_hashValueAt( const X_ITEM *hashItem, size_t pos );
uint32_t x_hashFlags( const X_ITEM *hashItem );
void x_hashSetFlags( X_ITEM *hashItem, uint32_t flags, int on );
X_ITEM *x_hashDefault( const X_ITEM *hashItem );
void x_hashSetDefault( X_ITEM *hashItem, const X_ITEM *value );
void *x_hashId( const X_ITEM *hashItem );
void x_hashClone( X_ITEM *result, const X_ITEM *source );
void x_hashEval( X_CALL_CTX *ctx, const X_ITEM *source, const X_ITEM *code, size_t start, int64_t count );
size_t x_hashScan( X_CALL_CTX *ctx, const X_ITEM *source, const X_ITEM *search, size_t start, int64_t count,
                   int exact );
void x_hashMerge( X_CALL_CTX *ctx, X_ITEM *target, const X_ITEM *source, size_t start, int64_t count, int mode,
                  const X_ITEM *code );

/* The bodies of Hash(), HGet(), HEval() and the other hash functions of the API */
void x_hashHash( X_CALL_CTX *ctx );
void x_hashHClone( X_CALL_CTX *ctx );
void x_hashHEval( X_CALL_CTX *ctx );
void x_hashHGet( X_CALL_CTX *ctx );
void x_hashHGetKeyAt( X_CALL_CTX *ctx );
void x_hashHGetList( X_CALL_CTX *ctx, int values );
void x_hashHGetPairAt( X_CALL_CTX *ctx );
void x_hashHGetVaaPos( X_CALL_CTX *ctx );
void x_hashHGetValueAt( X_CALL_CTX *ctx );
void x_hashHSetValueAt( X_CALL_CTX *ctx );

/* ---------------------------------------------------------------------------------------------
   Helpers of the public API in xdev-api.c
   --------------------------------------------------------------------------------------------- */

/* internal-screen.c */
extern int x_clearColor;
extern int x_clearChar;

int x_achoiceSelectable( X_CALL_CTX *ctx, const X_ITEM *selectable, int64_t index );
int64_t x_achoiceFind( X_CALL_CTX *ctx, const X_ITEM *selectable, int64_t count, int64_t index, int step );
void x_achoiceDraw( X_CALL_CTX *ctx, const X_ITEM *items, const X_ITEM *selectable, int64_t count, int64_t first,
                    int64_t current, const unsigned short rect[4] );
int x_achoiceUser( X_CALL_CTX *ctx, const X_ITEM *userBlock, int mode, int64_t current, int64_t first );
int x_colorArg( X_CALL_CTX *ctx, int n, int def );
void x_screenRange( X_CALL_CTX *ctx, unsigned short rect[4], int ordered, int fromCursor );
int x_cellChar( X_CALL_CTX *ctx, int n, int def );
void x_screenFill( int top, int left, int bottom, int right, int color, int character );
void x_screenShadow( int top, int left, int bottom, int right, int color );
void x_dispOut( X_CALL_CTX *ctx, int valueArg, const char *pic, size_t picLen, int colorArg );
int x_trueColorCheck( X_CALL_CTX *ctx, const char *name );
int x_rgb( X_CALL_CTX *ctx, int argument, const char *name, uint32_t *rgb );
void x_dispOutExt( X_CALL_CTX *ctx, const char *pic, size_t picLen, int foregroundArg, int backgroundArg,
                   const char *name );
int x_colorPair( const char *text, size_t length, int64_t n, size_t *start, size_t *pairLength );
int x_colorInvert( int color );

/* internal-screen.c: the bodies of AChoice(), DispBox(), SetKey() and the other screen functions of the API */
void x_screenAccept( X_CALL_CTX *ctx );
void x_screenAChoice( X_CALL_CTX *ctx );
void x_screenAlert( X_CALL_CTX *ctx );
void x_screenCharWin( X_CALL_CTX *ctx );
void x_screenClearEol( X_CALL_CTX *ctx );
void x_screenClearWin( X_CALL_CTX *ctx );
void x_screenCls( X_CALL_CTX *ctx );
void x_screenColorWin( X_CALL_CTX *ctx );
void x_screenDispBox( X_CALL_CTX *ctx );
void x_screenDispBoxExt( X_CALL_CTX *ctx, uint32_t foreground, uint32_t background );
void x_screenDispOutAt( X_CALL_CTX *ctx );
void x_screenDispOutAtExt( X_CALL_CTX *ctx, uint32_t foreground, uint32_t background );
void x_screenInvertWin( X_CALL_CTX *ctx );
void x_screenNToColor( X_CALL_CTX *ctx );
void x_screenRestScreen( X_CALL_CTX *ctx );
void x_screenSaveScreen( X_CALL_CTX *ctx );
void x_screenScroll( X_CALL_CTX *ctx );
void x_screenSetClrPair( X_CALL_CTX *ctx );
void x_screenSetColorExt( X_CALL_CTX *ctx, uint32_t foreground, uint32_t background );
void x_screenSetKey( X_CALL_CTX *ctx );
void x_screenSetMode( X_CALL_CTX *ctx );
void x_screenWait( X_CALL_CTX *ctx );

/* internal-ct.c */
void x_ctWSetShadow( X_CALL_CTX *ctx );

/* internal-file.c */
void x_fsDirSepToOS( X_CALL_CTX *ctx );

/* internal.c: the bodies of Empty(), ErrorBlock(), Run(), Set(), ValType() and Version() */
void x_itemEmpty( X_CALL_CTX *ctx );
void x_errorErrorBlock( X_CALL_CTX *ctx );
void x_sysRun( X_CALL_CTX *ctx );
void x_sysRunCapture( X_CALL_CTX *ctx );
void x_setSet( X_CALL_CTX *ctx );
void x_itemValType( X_CALL_CTX *ctx );
void x_sysVersion( X_CALL_CTX *ctx );

/* internal-date.c */
long x_dateParam( X_CALL_CTX *ctx, int n );
int64_t x_dateMilliSeconds( void );
int x_dateIsLeap( int year );
int x_dateMonthDays( int month, int leap );
int x_dateIsoDow( long julian );
int x_dateNameNumber( const char *text, size_t length, const char *( *nameOf )( int ), int count );
double x_timeToSeconds( const char *text, size_t length );
void x_timeReturn( X_CALL_CTX *ctx, double seconds, int hundredths );
void x_dateAddMonth( X_CALL_CTX *ctx );
void x_timeAmPm( X_CALL_CTX *ctx );
void x_dateDaysToMonth( X_CALL_CTX *ctx );
void x_dateDMY( X_CALL_CTX *ctx );
void x_dateEnd( X_CALL_CTX *ctx, int quarter );
void x_dateLastDayOM( X_CALL_CTX *ctx );
void x_dateMDY( X_CALL_CTX *ctx );
void x_timeSecToTime( X_CALL_CTX *ctx );
void x_dateSetCentury( X_CALL_CTX *ctx );
void x_dateSToD( X_CALL_CTX *ctx );
void x_dateWeek( X_CALL_CTX *ctx );
void x_dateWoM( X_CALL_CTX *ctx );

/* internal-text.c */
size_t x_atNumFind( const char *search, size_t searchLength, const char *text, size_t textLength,
                    int64_t occurrence, int64_t ignored );
size_t x_atSkipStrings( const char *search, size_t searchLength, const char *text, size_t length );
int x_wildMatch( const char *pattern, size_t patternLength, const char *text, size_t length, int full );
void x_atAdjust( X_CALL_CTX *ctx );
void x_atReplace( X_CALL_CTX *ctx );
void x_charCombine( X_CALL_CTX *ctx, char operation );
void x_charFilter( X_CALL_CTX *ctx, const char *chars, size_t charsLength, const char *text, size_t length,
                   int keep );
void x_charBits( X_CALL_CTX *ctx, int rotate, int left );

void x_ucharArg( X_CALL_CTX *ctx, int n, X_UCHAR *c );
int x_ucharAt( const char *text, size_t length, size_t pos, const X_UCHAR *c );
size_t x_ucharRun( const char *text, size_t length, const X_UCHAR *c, int fromRight );
char *x_ucharFill( char *destination, const X_UCHAR *c, size_t count );

uint32_t *x_ucodes( const char *text, size_t length, size_t *count );
char *x_ucodesString( const uint32_t *codes, size_t count, size_t *length );
void x_ucodesReturn( X_CALL_CTX *ctx, uint32_t *codes, size_t count );
uint32_t x_ucodeArg( X_CALL_CTX *ctx, int n, uint32_t def );
int x_ucodeListed( const uint32_t *list, size_t count, uint32_t code );
int64_t x_posFind( const char *text, size_t length, int64_t ignored, int ( *test )( const char *, size_t ),
                   int negate );
void x_charRemove( X_CALL_CTX *ctx, int left, int right );
void x_charReplaceSides( X_CALL_CTX *ctx, int left, int right );
void x_textCenter( X_CALL_CTX *ctx );

/* internal-text.c: the bodies of Char*(), Pos*() and Range*() */
void x_textCharHist( X_CALL_CTX *ctx );
void x_textCharList( X_CALL_CTX *ctx );
void x_textCharMirr( X_CALL_CTX *ctx );
void x_textCharMix( X_CALL_CTX *ctx );
void x_textCharNoList( X_CALL_CTX *ctx );
void x_textCharOne( X_CALL_CTX *ctx );
void x_textCharPack( X_CALL_CTX *ctx );
void x_textCharRela( X_CALL_CTX *ctx );
void x_textCharRelRep( X_CALL_CTX *ctx );
void x_textCharRepl( X_CALL_CTX *ctx );
void x_textCharSList( X_CALL_CTX *ctx );
void x_textCharSort( X_CALL_CTX *ctx );
void x_textCharSpread( X_CALL_CTX *ctx );
void x_textCharUnpack( X_CALL_CTX *ctx );
void x_textPosChar( X_CALL_CTX *ctx );
void x_textPosDel( X_CALL_CTX *ctx );
void x_textPosDiff( X_CALL_CTX *ctx );
void x_textPosEqual( X_CALL_CTX *ctx );
void x_textPosIns( X_CALL_CTX *ctx );
void x_textPosRange( X_CALL_CTX *ctx );
void x_textPosRepl( X_CALL_CTX *ctx );
void x_textRangeRem( X_CALL_CTX *ctx );
void x_textRangeRepl( X_CALL_CTX *ctx );

/* internal-text.c: the bodies of At(), SubStr(), StrTran() and the other text functions of the API */
void x_textAddASCII( X_CALL_CTX *ctx );
void x_textAfterAtNum( X_CALL_CTX *ctx );
void x_textAnsiToHtml( X_CALL_CTX *ctx );
void x_textASCIISum( X_CALL_CTX *ctx );
void x_textAt( X_CALL_CTX *ctx );
void x_textATokens( X_CALL_CTX *ctx );
void x_textBlank( X_CALL_CTX *ctx );
void x_textComplement( X_CALL_CTX *ctx );
void x_textDescend( X_CALL_CTX *ctx );
void x_textExpand( X_CALL_CTX *ctx );
void x_textJustLeft( X_CALL_CTX *ctx );
void x_textJustRight( X_CALL_CTX *ctx );
void x_textLeft( X_CALL_CTX *ctx );
void x_textLen( X_CALL_CTX *ctx );
void x_textMemoLine( X_CALL_CTX *ctx );
void x_textMemoTran( X_CALL_CTX *ctx );
void x_textMLCount( X_CALL_CTX *ctx );
void x_textMLCToPos( X_CALL_CTX *ctx );
void x_textMLPos( X_CALL_CTX *ctx );
void x_textMPosToLC( X_CALL_CTX *ctx );
void x_textNumAt( X_CALL_CTX *ctx );
void x_textPadLeft( X_CALL_CTX *ctx );
void x_textRAt( X_CALL_CTX *ctx );
void x_textReplicate( X_CALL_CTX *ctx );
void x_textRight( X_CALL_CTX *ctx );
void x_textSoundEx( X_CALL_CTX *ctx );
void x_textStrDiff( X_CALL_CTX *ctx );
void x_textStrSwap( X_CALL_CTX *ctx );
void x_textStrTran( X_CALL_CTX *ctx );
void x_textStuff( X_CALL_CTX *ctx );
void x_textSubStr( X_CALL_CTX *ctx );
void x_textTabExpand( X_CALL_CTX *ctx );
void x_textTabPack( X_CALL_CTX *ctx );
void x_textUChars( X_CALL_CTX *ctx );

/* internal-array.c: FOR EACH */
int x_enumLength( const X_ITEM *base, int64_t *length );
void x_enumGet( const X_ITEM *base, int64_t pos, X_ITEM *dest );
void x_enumPut( X_CALL_CTX *ctx, int64_t pos, const X_ITEM *value );

/* ---------------------------------------------------------------------------------------------
   Errors
   --------------------------------------------------------------------------------------------- */

X_ERROR_HANDLER x_errorSetHandler( X_ERROR_HANDLER handler );
const X_ERROR *x_errorPeek( void );
int x_errorCaught( void );
void x_breakRaise( const X_ITEM *value ) __attribute__( ( noreturn ) );
void x_breakPut( X_ITEM *value );
void x_errorPut( X_ERROR *error );
X_ERROR *x_errorClone( const X_ERROR *error );
void x_errorFree( X_ERROR *error );
void x_errorThrow( X_ERROR *error ) __attribute__( ( noreturn ) );
void x_recoverBreak( void *except, X_ITEM *value );
void x_catchBreak( void *except );
void x_catchError( void *except );
void x_quitRaise( void ) __attribute__( ( noreturn ) );
void x_catchQuit( void *except );
void x_exceptionResume( void *except ) __attribute__( ( noreturn ) );
void x_errorBlockGet( X_CALL_CTX *ctx, X_ITEM *result );
void x_errorBlockSet( X_CALL_CTX *ctx, X_ITEM *result, const X_ITEM *block );
void x_errorBlockClear( void );
int x_errorLevelGet( void );
void x_errorLevelSet( int level );
int x_errorNew( X_CALL_CTX *ctx, X_ITEM *result );
int x_errorObject( X_CALL_CTX *ctx, X_ITEM *result, const X_ERROR *error );
void x_errorClear( void );
void x_errorReport( void );
int x_errorRaise( X_ITEM *result, int subCode, const X_ITEM *a, const X_ITEM *b );
int x_errorRaiseNamed( X_ITEM *result, int subCode, const char *operation );
int x_errorRaiseDefinition( X_ITEM *result, const X_ERROR_DEF *definition, const X_ITEM *a, const X_ITEM *b );
int x_errorRaiseDefaultDefinition( X_ITEM *result, const X_ERROR_DEF *definition, const X_ITEM *a,
                                   const X_ITEM *b );
int x_errorRaiseObject( X_ITEM *result, const X_ITEM *object );
int x_errorCondition( const X_ITEM *value );

/* ---------------------------------------------------------------------------------------------
   Macros
   --------------------------------------------------------------------------------------------- */

/* Shared argument validation uses the same error across all API functions */
#define X_ARGCHECK( ctx, condition, name ) \
   if( !( condition ) ) \
   { \
      x_errorRaiseNamed( ( ctx )->result, 1102, ( name ) ); \
      return; \
   }

/* Like X_ARGCHECK for optional argument n: omission is allowed, but a supplied argument must satisfy
   the type condition */
#define X_ARGCHECK_OPT( ctx, n, condition, name ) X_ARGCHECK( ctx, x_pcount( ctx ) < ( n ) || ( condition ), ( name ) )

#define X_IT_ANY 0xFFFFFFFFu
#define X_IT_INTEGRAL ( X_IT_INTEGER | X_IT_UINT64 )
#define X_IT_NUMERIC ( X_IT_INTEGRAL | X_IT_DOUBLE )

#define X_ITEM_TYPERAW( p ) ( ( p )->nType )
#define X_ITEM_TYPE( p ) ( X_ITEM_TYPERAW( p ) & ~X_IT_GCFLAG )

/* The type of an item */
#define X_IS_ARRAY( p ) ( ( X_ITEM_TYPERAW( p ) & X_IT_ARRAY ) != 0 )
#define X_IS_BLOCK( p ) ( ( X_ITEM_TYPERAW( p ) & X_IT_BLOCK ) != 0 )
#define X_IS_BYREF( p ) ( ( X_ITEM_TYPERAW( p ) & X_IT_BYREF ) != 0 )
#define X_IS_DATE( p ) ( ( X_ITEM_TYPERAW( p ) & X_IT_DATE ) != 0 )
#define X_IS_DOUBLE( p ) ( ( X_ITEM_TYPERAW( p ) & X_IT_DOUBLE ) != 0 )
#define X_IS_HASH( p ) ( X_ITEM_TYPERAW( p ) == X_IT_HASH )
#define X_IS_INTEGER( p ) ( ( X_ITEM_TYPERAW( p ) & X_IT_INTEGRAL ) != 0 )
#define X_IS_UINT64( p ) ( X_ITEM_TYPERAW( p ) == X_IT_UINT64 )
#define X_IS_LOGICAL( p ) ( ( X_ITEM_TYPERAW( p ) & X_IT_LOGICAL ) != 0 )
#define X_IS_NIL( p ) ( X_ITEM_TYPE( p ) == X_IT_NIL )
#define X_IS_NUMBER( p ) X_IS_NUMERIC( p )
#define X_IS_NUMERIC( p ) ( ( X_ITEM_TYPERAW( p ) & X_IT_NUMERIC ) != 0 )
#define X_IS_OBJECT( p ) x_arrayIsObject( p )
#define X_IS_POINTER( p ) ( ( X_ITEM_TYPERAW( p ) & X_IT_POINTER ) != 0 )
#define X_IS_STRING( p ) ( ( X_ITEM_TYPERAW( p ) & X_IT_STRING ) != 0 )

/* The type of argument n */
#define X_ISARRAY( ctx, n ) ( x_param( ctx, n, X_IT_ARRAY ) != NULL )
#define X_ISBLOCK( ctx, n ) ( x_param( ctx, n, X_IT_BLOCK ) != NULL )
#define X_ISHASH( ctx, n ) ( x_param( ctx, n, X_IT_HASH ) != NULL )
#define X_ISBYREF( ctx, n ) ( ( x_parinfo( ctx, n ) & X_IT_BYREF ) != 0 )
#define X_ISCHAR( ctx, n ) ( x_param( ctx, n, X_IT_STRING ) != NULL )
#define X_ISDATE( ctx, n ) ( x_param( ctx, n, X_IT_DATE ) != NULL )
#define X_ISLOG( ctx, n ) ( x_param( ctx, n, X_IT_LOGICAL ) != NULL )
#define X_ISNIL( ctx, n ) ( x_extIsNil( ctx, n ) )
#define X_ISNUM( ctx, n ) ( x_param( ctx, n, X_IT_NUMERIC ) != NULL )
#define X_ISOBJECT( ctx, n ) ( x_extIsObject( ctx, n ) )
#define X_ISPOINTER( ctx, n ) ( x_param( ctx, n, X_IT_POINTER ) != NULL )

#define X_ITEM_NIL_INIT { X_IT_NIL, { 0 } }
#define X_GARBAGE_FUNC( NAME ) void NAME( void *Cargo )

/* Functions of the API; X_RTL_DEFAULT_API makes them weak, so a library can replace them */
#if defined( X_RTL_DEFAULT_API )
#define X_FUNC_ATTRIBUTES __attribute__( ( weak ) )
#else
#define X_FUNC_ATTRIBUTES
#endif

#define X_FUNCNAME( NAME ) NAME
#define X_FUNC( NAME ) X_FUNC_IMPL( X_FUNC_ATTRIBUTES, NAME )
#define X_FUNC_STATIC( NAME ) X_FUNC_IMPL( static, NAME )
#define X_FUNC_ALIAS( NAME, SYMBOL ) \
   void SYMBOL( X_CALL_CTX *ctx ) __asm__( #NAME ); \
   X_FUNC_IMPL( X_FUNC_ATTRIBUTES, SYMBOL )

#define X_FUNC_IMPL( STORAGE, NAME ) STORAGE void NAME( X_CALL_CTX *ctx __attribute__( ( unused ) ) )

#define X_METHODPTR( NAME ) X_FUNCNAME( NAME )

#define X_METHOD( NAME ) \
   static void s_methodBody_##NAME( X_CALL_CTX *ctx, X_ITEM *x_self ); \
   static void NAME( X_CALL_CTX *ctx ) \
   { \
      s_methodBody_##NAME( ctx, ctx->self ); \
   } \
   static void s_methodBody_##NAME( X_CALL_CTX *ctx __attribute__( ( unused ) ), X_ITEM *x_self )

#define UNUSED( n ) ( ( void )( n ) )
#define IIF( condition, trueValue, falseValue ) ( ( condition ) ? ( trueValue ) : ( falseValue ) )

#ifdef __cplusplus
}
#endif

#endif /* INTERNAL_H */
