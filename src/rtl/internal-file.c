/*
 * Copyright (c) 2024-2026 xDev-Language
 *
 * Library...: xDev Runtime
 * File......: internal-file.c
 *
 */

#if defined( __linux__ ) && !defined( _GNU_SOURCE )
#define _GNU_SOURCE
#endif

#ifndef _FILE_OFFSET_BITS
#define _FILE_OFFSET_BITS 64
#endif

#include "internal.h"
#include "platform.h"
#include "xdev/directry.ch"
#include "xdev/fileio.ch"

#include <ctype.h>
#include <stdio.h>
#include <limits.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>

#if X_OS_WINDOWS
#include <direct.h>
#include <windows.h>
#include <winioctl.h>
#else
#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <unistd.h>
#include <dirent.h>
#include <fnmatch.h>
#endif

#if X_OS_WINDOWS
/* All sharing modes; the milliseconds from 1601 (FILETIME) to 1970; the buffer of a reparse point */
#define X_FILE_SHARE_ALL ( FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE )
#define X_FILE_EPOCH INT64_C( 11644473600000 )
#define X_REPARSE_SIZE MAXIMUM_REPARSE_DATA_BUFFER_SIZE
#endif

/* One line of a text area: where it starts and how long it is */
typedef struct
{
   int64_t offset;
   int64_t length;
} S_FILE_TEXT_LINE;

/* A text area of FileTextUse() and its family: an open file and its lines */
typedef struct
{
   int open;
   int64_t handle;
   int64_t size;
   size_t count;
   size_t capacity;
   size_t current;
   S_FILE_TEXT_LINE *lines;
} S_FILE_TEXT_AREA;

/* A growing buffer */
typedef struct
{
   char *data;
   size_t length;
   size_t capacity;
} S_FILE_BUFFER;

/* A file object of FileOpen() */
typedef struct
{
   int64_t handle;
} X_FILE;

/* FError(): the error of the last file operation */
static int s_fileErr;

/* The text areas and the selected one */
static S_FILE_TEXT_AREA s_textAreas[10];
static int s_textArea;

/* The comment characters of the INI functions */
static char s_iniLineComment = ';';
static char s_iniInlineComment = '#';

/* The files put into the program by xdev, registered before main(); the last entry has no name */
static const X_RESOURCE s_noResources[] = { { NULL, NULL, 0 } };
static const X_RESOURCE *s_resources = s_noResources;

/* ---------------------------------------------------------------------------------------------
   File objects of FileOpen()
   --------------------------------------------------------------------------------------------- */

/* The garbage collector closes a file object that is still open; FError() stays */
static void s_fileDrop( void *cargo )
{
   X_FILE *file = ( X_FILE * )cargo;

   if( file->handle != F_ERROR )
   {
      int error = s_fileErr;
      int64_t handle = file->handle;

      file->handle = F_ERROR;
      x_fsClose( handle );
      s_fileErr = error;
   }
}

const X_GC_FUNCS x_fileGC = { s_fileDrop, x_gcDummyMark };

/* A file object for an open handle */
static X_FILE *s_fileObject( int64_t handle )
{
   X_FILE *file = ( X_FILE * )x_gcAllocate( sizeof( X_FILE ), &x_fileGC );

   file->handle = handle;

   return file;
}

/* FileOpen(): a file object; NULL when the file cannot be opened */
void *x_fileOpen( const char *path, int mode )
{
   int64_t handle = x_fsOpen( path, mode );

   if( handle == F_ERROR )
      return NULL;

   return s_fileObject( handle );
}

/* FileCreate(): a file object; NULL when the file cannot be created */
void *x_fileCreate( const char *path, int attr )
{
   int64_t handle = x_fsCreate( path, attr );

   if( handle == F_ERROR )
      return NULL;

   return s_fileObject( handle );
}

/* FileClose() */
int x_fileClose( const X_ITEM *item )
{
   X_FILE *file = ( X_FILE * )x_itemGetPtrGC( item, &x_fileGC );
   int64_t handle = file->handle;

   if( handle == F_ERROR )
   {
      s_fileErr = 6;
      return 0;
   }

   file->handle = F_ERROR;

   return x_fsClose( handle );
}

/* FileRead(): the bytes read, -1 on error; a timeout is not supported */
int64_t x_fileRead( const X_ITEM *item, void *buffer, size_t count, int64_t timeout )
{
   X_FILE *file = ( X_FILE * )x_itemGetPtrGC( item, &x_fileGC );
   size_t bytes;

   if( file->handle == F_ERROR )
   {
      s_fileErr = 6;
      return -1;
   }

   if( timeout != -1 )
   {
      s_fileErr = 50;
      return -1;
   }

   bytes = x_fsRead( file->handle, buffer, count );

   return IIF( s_fileErr, -1, ( int64_t )bytes );
}

/* FileWrite(): the bytes written, -1 on error; a timeout is not supported */
int64_t x_fileWrite( const X_ITEM *item, const void *buffer, size_t count, int64_t timeout )
{
   X_FILE *file = ( X_FILE * )x_itemGetPtrGC( item, &x_fileGC );
   size_t bytes;

   if( file->handle == F_ERROR )
   {
      s_fileErr = 6;
      return -1;
   }

   if( timeout != -1 )
   {
      s_fileErr = 50;
      return -1;
   }

   if( count == 0 )
   {
      s_fileErr = 0;
      return 0;
   }

   bytes = x_fsWrite( file->handle, buffer, count );

   return IIF( s_fileErr, -1, ( int64_t )bytes );
}

/* FileSeek() */
int64_t x_fileSeek( const X_ITEM *item, int64_t offset, int mode )
{
   X_FILE *file = ( X_FILE * )x_itemGetPtrGC( item, &x_fileGC );

   if( file->handle == F_ERROR )
   {
      s_fileErr = 6;
      return -1;
   }

   return x_fsSeek( file->handle, offset, mode );
}

/* ---------------------------------------------------------------------------------------------
   Operating system files
   --------------------------------------------------------------------------------------------- */

#if X_OS_WINDOWS
/* A UTF-8 path as a wide string, allocated by x_xgrab() */
static wchar_t *s_fileName( const char *path )
{
   int size = MultiByteToWideChar( CP_UTF8, 0, path, -1, NULL, 0 );
   wchar_t *name = ( wchar_t * )x_xgrabArray( ( size_t )size, sizeof( wchar_t ) );

   MultiByteToWideChar( CP_UTF8, 0, path, -1, name, size );

   return name;
}

/* CreateFileW() with a UTF-8 path; FError() gets its error */
static int64_t s_fileOpen( const char *path, DWORD access, DWORD share, DWORD create, DWORD attr )
{
   wchar_t *name = s_fileName( path );
   HANDLE handle = CreateFileW( name, access, share, NULL, create, attr, NULL );

   s_fileErr = IIF( handle == INVALID_HANDLE_VALUE, ( int )GetLastError(), 0 );
   x_xfree( name );

   return ( int64_t )( intptr_t )handle;
}

/* A wide string as UTF-8, allocated by x_xgrab(); NULL on error */
static char *s_fileText( const wchar_t *name )
{
   int size = WideCharToMultiByte( CP_UTF8, 0, name, -1, NULL, 0, NULL, NULL );
   char *text;

   if( size == 0 )
   {
      s_fileErr = ( int )GetLastError();
      return NULL;
   }

   text = ( char * )x_xgrab( ( size_t )size );
   WideCharToMultiByte( CP_UTF8, 0, name, -1, text, size, NULL, NULL );

   return text;
}
#else
/* FError() of a POSIX result: errno as the DOS error codes */
static void s_fileResult( int result )
{
   s_fileErr = IIF( result < 0, errno, 0 );
   if( result >= 0 )
      return;

   switch( s_fileErr )
   {
   case ENOENT:
      s_fileErr = 2;
      break;

   case ENOTDIR:
      s_fileErr = 3;
      break;

   case EMFILE:
   case ENFILE:
      s_fileErr = 4;
      break;

   case EACCES:
   case EPERM:
   case EISDIR:
      s_fileErr = 5;
      break;

   case EBADF:
      s_fileErr = 6;
      break;

   case ENOMEM:
      s_fileErr = 8;
      break;

   case EINVAL:
      s_fileErr = 13;
      break;

   case EROFS:
      s_fileErr = 19;
      break;

   case EEXIST:
      s_fileErr = 80;
      break;

   case ENOSPC:
      s_fileErr = 112;
      break;
   }
}
#endif

/* Does the name have a directory part? */
static int s_fileHasPath( const char *name )
{
#if X_OS_WINDOWS
   return strpbrk( name, "/\\:" ) != NULL;
#else
   return strchr( name, '/' ) != NULL;
#endif
}

/* Does a directory need a separator before a name? */
static int s_fileNeedsSlash( const char *dir, size_t length )
{
   if( length == 0 )
      return 0;

#if X_OS_WINDOWS
   return strchr( "/\\:", dir[length - 1] ) == NULL;
#else
   return dir[length - 1] != '/';
#endif
}

/* A file name in the SET DEFAULT directory when it has no directory of its own; allocated by x_xgrab() */
char *x_fileDefName( const char *name )
{
   const char *dir = x_setDefault( NULL );
   size_t length;
   int slash;
   char *path;

   if( s_fileHasPath( name ) )
      return x_strdup( name );

   length = strlen( dir );
   if( length == 0 )
      return x_strdup( name );

   slash = s_fileNeedsSlash( dir, length );
   path = ( char * )x_xgrab( length + slash + strlen( name ) + 1 );
   memcpy( path, dir, length );
   if( slash )
      path[length++] = '/';

   strcpy( path + length, name );

   return path;
}

/* File(): a file in the SET DEFAULT directory or, for a name without a directory, on the SET PATH */
int x_fileFind( const char *spec )
{
   X_ITEM files __attribute__( ( cleanup( x_itemClear ) ) ) = X_ITEM_NIL_INIT;
   const char *path;
   char *name;

   if( *spec == '\0' )
      return 0;

   name = x_fileDefName( spec );
   x_fileDirectory( &files, name, "" );
   x_xfree( name );
   if( x_arrayLen( &files ) )
      return 1;

   if( s_fileHasPath( spec ) )
      return 0;

   path = x_setPath( NULL );
   while( *path )
   {
      size_t length = strcspn( path, ",;" );
      int slash = s_fileNeedsSlash( path, length );

      name = ( char * )x_xgrab( length + slash + strlen( spec ) + 1 );
      memcpy( name, path, length );
      if( slash )
         name[length] = '/';

      strcpy( name + length + slash, spec );
      x_fileDirectory( &files, name, "" );
      x_xfree( name );
      if( x_arrayLen( &files ) )
         return 1;

      path += length;
      if( *path )
         ++path;
   }

   return 0;
}

/* Does the path exist, and is it a file or a directory? */
int x_fsExists( const char *path, int *isFile, int *isDir )
{
#if X_OS_WINDOWS
   wchar_t *name = s_fileName( path );
   DWORD attr = GetFileAttributesW( name );
   int exists = attr != INVALID_FILE_ATTRIBUTES;

   s_fileErr = IIF( exists, 0, ( int )GetLastError() );
   x_xfree( name );
   *isFile = exists && !( attr & ( FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_DEVICE ) );
   *isDir = exists && ( attr & FILE_ATTRIBUTE_DIRECTORY ) != 0;
#else
   struct stat info;
   int result = stat( path, &info );
   int exists = result == 0;

   s_fileResult( result );
   *isFile = exists && S_ISREG( info.st_mode );
   *isDir = exists && S_ISDIR( info.st_mode );
#endif

   return exists;
}

/* FOpen(): the access of mode & 3 and, on Windows, the sharing of mode & 0x70 */
int64_t x_fsOpen( const char *path, int mode )
{
#if X_OS_WINDOWS
   DWORD access = GENERIC_READ;
   DWORD share = FILE_SHARE_READ | FILE_SHARE_WRITE;

   switch( mode & 3 )
   {
   case FO_WRITE:
      access = GENERIC_WRITE;
      break;

   case FO_READWRITE:
      access = GENERIC_READ | GENERIC_WRITE;
      break;
   }

   switch( mode & 0x70 )
   {
   case FO_EXCLUSIVE:
      share = 0;
      break;

   case FO_DENYWRITE:
      share = FILE_SHARE_READ;
      break;

   case FO_DENYREAD:
      share = FILE_SHARE_WRITE;
      break;
   }

   return s_fileOpen( path, access, share, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL );
#else
   int flags = O_RDONLY;
   int handle;

   switch( mode & 3 )
   {
   case FO_WRITE:
      flags = O_WRONLY;
      break;

   case FO_READWRITE:
      flags = O_RDWR;
      break;
   }

   handle = open( path, flags );
   s_fileResult( handle );

   return handle;
#endif
}

/* FCreate(): an empty file with the attributes FC_READONLY, FC_HIDDEN and FC_SYSTEM */
int64_t x_fsCreate( const char *path, int attr )
{
#if X_OS_WINDOWS
   DWORD flags = 0;

   if( attr & FC_READONLY )
      flags |= FILE_ATTRIBUTE_READONLY;

   if( attr & FC_HIDDEN )
      flags |= FILE_ATTRIBUTE_HIDDEN;

   if( attr & FC_SYSTEM )
      flags |= FILE_ATTRIBUTE_SYSTEM;

   if( flags == 0 )
      flags = FILE_ATTRIBUTE_NORMAL;

   return s_fileOpen( path, GENERIC_READ | GENERIC_WRITE, 0, CREATE_ALWAYS, flags );
#else
   int handle = open( path, O_CREAT | O_TRUNC | O_RDWR, IIF( attr & FC_READONLY, 0444, 0666 ) );

   s_fileResult( handle );

   return handle;
#endif
}

/* FClose() */
int x_fsClose( int64_t handle )
{
#if X_OS_WINDOWS
   int closed;
#else
   int result;
#endif

   if( handle == F_ERROR )
   {
      s_fileErr = 6;
      return 0;
   }

#if X_OS_WINDOWS
   closed = CloseHandle( ( HANDLE )( intptr_t )handle ) != 0;
   s_fileErr = IIF( closed, 0, ( int )GetLastError() );

   return closed;
#else
   result = close( ( int )handle );
   s_fileResult( result );

   return result == 0;
#endif
}

/* The operating system handle of a file object */
int64_t x_fileHandle( const X_ITEM *item )
{
   X_FILE *file = ( X_FILE * )x_itemGetPtrGC( item, &x_fileGC );

   s_fileErr = IIF( file->handle == F_ERROR, 6, 0 );

   return file->handle;
}

/* Cut the file of handle at offset */
static int s_fileTruncate( int64_t handle, int64_t offset )
{
#if X_OS_WINDOWS
   FILE_END_OF_FILE_INFO info;
   HANDLE file = ( HANDLE )( intptr_t )handle;
   int result;

   info.EndOfFile.QuadPart = offset;
   result = SetFileInformationByHandle( file, FileEndOfFileInfo, &info, sizeof( info ) ) != 0;
   s_fileErr = IIF( result, 0, ( int )GetLastError() );

   return result;
#else
   int result = ftruncate( ( int )handle, ( off_t )offset );

   s_fileResult( result );

   return result == 0;
#endif
}

/* FileTrunc() */
int x_fileTrunc( const X_ITEM *item, int64_t offset )
{
   int64_t handle = x_fileHandle( item );

   if( handle == F_ERROR )
      return 0;

   return s_fileTruncate( handle, offset );
}

/* The size of an open file; -1 on error */
static int64_t s_fileSize( int64_t handle )
{
#if X_OS_WINDOWS
   LARGE_INTEGER size;
   int result = GetFileSizeEx( ( HANDLE )( intptr_t )handle, &size ) != 0;

   s_fileErr = IIF( result, 0, ( int )GetLastError() );

   return IIF( result, size.QuadPart, -1 );
#else
   struct stat info;
   int result = fstat( ( int )handle, &info );

   s_fileResult( result );

   return IIF( result == 0, ( int64_t )info.st_size, -1 );
#endif
}

/* FileSize(): of a file object, or of a file name (dirEntry reads the directory entry) */
int64_t x_fileSize( const X_ITEM *item, int dirEntry )
{
   if( X_IS_STRING( item ) )
   {
      const char *path = item->u.string.data;

      if( dirEntry )
      {
#if X_OS_WINDOWS
         WIN32_FILE_ATTRIBUTE_DATA info;
         wchar_t *name = s_fileName( path );
         int result = GetFileAttributesExW( name, GetFileExInfoStandard, &info ) != 0;

         s_fileErr = IIF( result, 0, ( int )GetLastError() );
         x_xfree( name );
         return IIF( result, ( int64_t )( ( ( uint64_t )info.nFileSizeHigh << 32 ) | info.nFileSizeLow ), -1 );
#else
         struct stat info;
         int result = stat( path, &info );

         s_fileResult( result );
         return IIF( result == 0, ( int64_t )info.st_size, -1 );
#endif
      }

      int64_t handle = x_fsOpen( path, FO_READ | FO_DENYNONE );

      if( handle == F_ERROR )
         return -1;

      int64_t size = s_fileSize( handle );
      int error = s_fileErr;

      x_fsClose( handle );
      s_fileErr = error;
      return size;
   }

   int64_t handle = x_fileHandle( item );

   return IIF( handle == F_ERROR, -1, s_fileSize( handle ) );
}

/* FileEof(): is the position at or past the end? */
int x_fileEof( const X_ITEM *item )
{
   int64_t handle = x_fileHandle( item );

   if( handle == F_ERROR )
      return 0;

   int64_t pos = x_fsSeek( handle, 0, FS_RELATIVE );

   if( pos < 0 )
      return 0;

   int64_t size = s_fileSize( handle );

   return size >= 0 && pos >= size;
}

/* FileCommit(): write the buffers of the file to the disk */
int x_fileCommit( const X_ITEM *item )
{
   int64_t handle = x_fileHandle( item );

   if( handle == F_ERROR )
      return 0;
#if X_OS_WINDOWS
   int result = FlushFileBuffers( ( HANDLE )( intptr_t )handle ) != 0;

   s_fileErr = IIF( result, 0, ( int )GetLastError() );

   return result;
#else
   int result = fsync( ( int )handle );

   s_fileResult( result );

   return result == 0;
#endif
}

/* Is the range of a lock valid, and the lock type FLX_SHARED and FLX_WAIT at most? */
static int s_fileLockRange( int64_t start, int64_t length, int type )
{
   if( start < 0 || length <= 0 || start > INT64_MAX - ( length - 1 ) )
      return 0;

   return ( type & ~( FLX_SHARED | FLX_WAIT ) ) == 0;
}

/* FileLock(): lock or unlock a range; FLX_WAIT waits for it */
int x_fileLock( const X_ITEM *item, int64_t start, int64_t length, int type, int unlock )
{
   int64_t handle = x_fileHandle( item );

   if( handle == F_ERROR )
      return 0;

   if( !s_fileLockRange( start, length, type ) )
   {
      s_fileErr = 13;
      return 0;
   }
#if X_OS_WINDOWS
   OVERLAPPED pos = { 0 };

   pos.Offset = ( DWORD )start;
   pos.OffsetHigh = ( DWORD )( ( uint64_t )start >> 32 );
   DWORD low = ( DWORD )length;
   DWORD high = ( DWORD )( ( uint64_t )length >> 32 );
   int result;

   if( unlock )
      result = UnlockFileEx( ( HANDLE )( intptr_t )handle, 0, low, high, &pos ) != 0;
   else
   {
      DWORD flags = IIF( type & FLX_SHARED, 0, LOCKFILE_EXCLUSIVE_LOCK );

      if( !( type & FLX_WAIT ) )
         flags |= LOCKFILE_FAIL_IMMEDIATELY;

      result = LockFileEx( ( HANDLE )( intptr_t )handle, flags, 0, low, high, &pos ) != 0;
   }

   s_fileErr = IIF( result, 0, ( int )GetLastError() );

   return result;
#else
   struct flock lock = { 0 };

   if( unlock )
      lock.l_type = F_UNLCK;
   else
      lock.l_type = IIF( type & FLX_SHARED, F_RDLCK, F_WRLCK );

   lock.l_whence = SEEK_SET;
   lock.l_start = ( off_t )start;
   lock.l_len = ( off_t )length;
   int cmd = IIF( !unlock && ( type & FLX_WAIT ), F_SETLKW, F_SETLK );
   int result = fcntl( ( int )handle, cmd, &lock );

   if( result < 0 && ( errno == EACCES || errno == EAGAIN ) )
      s_fileErr = 33;
   else
      s_fileResult( result );

   return result == 0;
#endif
}

/* FileLockTest(): 0 when the range can be locked, the process or 1 that holds it, -1 on error */
int64_t x_fileLockTest( const X_ITEM *item, int64_t start, int64_t length, int type )
{
   int64_t handle = x_fileHandle( item );

   if( handle == F_ERROR )
      return -1;

   if( !s_fileLockRange( start, length, type ) )
   {
      s_fileErr = 13;
      return -1;
   }
#if X_OS_WINDOWS
   if( !x_fileLock( item, start, length, type & FLX_SHARED, 0 ) )
   {
      if( s_fileErr != ERROR_LOCK_VIOLATION )
         return -1;

      s_fileErr = 0;
      return 1;
   }

   return IIF( x_fileLock( item, start, length, 0, 1 ), 0, -1 );
#else
   struct flock lock = { 0 };

   lock.l_type = IIF( type & FLX_SHARED, F_RDLCK, F_WRLCK );
   lock.l_whence = SEEK_SET;
   lock.l_start = ( off_t )start;
   lock.l_len = ( off_t )length;
   int result = fcntl( ( int )handle, F_GETLK, &lock );

   s_fileResult( result );
   if( result != 0 )
      return -1;

   return IIF( lock.l_type == F_UNLCK, 0, ( int64_t )lock.l_pid );
#endif
}

/* The current drive (1 = A:); 0 on systems without drives */
int x_fsCurDrv( void )
{
#if X_OS_WINDOWS
   return _getdrive();
#else
   return 0;
#endif
}

/* Make drive (1 = A:) current; error 15 on systems without drives */
int x_fsChDrv( int drive )
{
#if X_OS_WINDOWS
   if( drive >= 1 && drive <= 26 )
   {
      int result = _chdrive( drive );
      unsigned long error = 0;

      if( result != 0 )
         _get_doserrno( &error );

      s_fileErr = ( int )error;
      return result == 0;
   }
#else
   ( void )drive;
#endif
   s_fileErr = 15;

   return 0;
}

/* DiskSpace(): the free bytes of a drive (0 = the current one) */
int64_t x_fsDiskSpace( int drive )
{
#if X_OS_WINDOWS
   if( drive == 0 )
      return x_fsDirSpace( ".", DISK_AVAIL );

   char path[] = "A:\\";

   path[0] += ( char )( drive - 1 );

   return x_fsDirSpace( path, DISK_AVAIL );
#else
   ( void )drive;

   return x_fsDirSpace( ".", DISK_AVAIL );
#endif
}

/* FError() */
int x_fsError( void )
{
   return s_fileErr;
}

/* Set FError() */
void x_fsSetError( int error )
{
   s_fileErr = error;
}

/* FRead(): the bytes read; FError() tells an error from the end of the file */
size_t x_fsRead( int64_t handle, void *buffer, size_t count )
{
   s_fileErr = 0;
   if( count == 0 )
      return 0;
#if X_OS_WINDOWS
   DWORD bytes = 0;
   DWORD size = ( DWORD )IIF( count > MAXDWORD, MAXDWORD, count );
   int result = ReadFile( ( HANDLE )( intptr_t )handle, buffer, size, &bytes, NULL );

   if( !result )
      s_fileErr = ( int )GetLastError();

   return IIF( result, ( size_t )bytes, 0 );
#else
   size_t size = IIF( count > ( size_t )SSIZE_MAX, ( size_t )SSIZE_MAX, count );
   ssize_t bytes = read( ( int )handle, buffer, size );

   s_fileResult( IIF( bytes < 0, -1, 0 ) );

   return IIF( bytes < 0, 0, ( size_t )bytes );
#endif
}

/* FWrite(): the bytes written; 0 bytes cut the file at the position */
size_t x_fsWrite( int64_t handle, const void *buffer, size_t count )
{
#if X_OS_WINDOWS
   DWORD bytes = 0;
   DWORD size = ( DWORD )IIF( count > MAXDWORD, MAXDWORD, count );

   HANDLE file = ( HANDLE )( intptr_t )handle;
   int result;

   /* Writing 0 bytes cuts the file at the position */
   if( count == 0 )
      result = SetEndOfFile( file );
   else
      result = WriteFile( file, buffer, size, &bytes, NULL );

   s_fileErr = IIF( result, 0, ( int )GetLastError() );

   return IIF( result, ( size_t )bytes, 0 );
#else
   if( count == 0 )
   {
      off_t pos = lseek( ( int )handle, 0, SEEK_CUR );
      int result = IIF( pos == ( off_t )-1, -1, ftruncate( ( int )handle, pos ) );

      s_fileResult( result );
      return 0;
   }

   size_t size = IIF( count > ( size_t )SSIZE_MAX, ( size_t )SSIZE_MAX, count );
   ssize_t bytes = write( ( int )handle, buffer, size );

   s_fileResult( IIF( bytes < 0, -1, 0 ) );

   return IIF( bytes < 0, 0, ( size_t )bytes );
#endif
}

/* FileReadAt(): read at an offset; the position of the file stays */
int64_t x_fileReadAt( const X_ITEM *item, void *buffer, size_t count, int64_t offset )
{
   int64_t handle = x_fileHandle( item );

   if( handle == F_ERROR )
      return -1;

   if( count == 0 )
      return 0;
#if X_OS_WINDOWS
   int64_t pos = x_fsSeek( handle, 0, FS_RELATIVE );

   if( pos < 0 || x_fsSeek( handle, offset, FS_SET ) < 0 )
      return -1;

   size_t bytes = x_fsRead( handle, buffer, count );
   int error = s_fileErr;

   x_fsSeek( handle, pos, FS_SET );
   if( error )
      s_fileErr = error;

   return IIF( s_fileErr, -1, ( int64_t )bytes );
#else
   size_t size = IIF( count > ( size_t )SSIZE_MAX, ( size_t )SSIZE_MAX, count );
   ssize_t bytes = pread( ( int )handle, buffer, size, ( off_t )offset );

   s_fileResult( IIF( bytes < 0, -1, 0 ) );

   return ( int64_t )bytes;
#endif
}

/* FileWriteAt(): write at an offset; the position of the file stays */
int64_t x_fileWriteAt( const X_ITEM *item, const void *buffer, size_t count, int64_t offset )
{
   int64_t handle = x_fileHandle( item );

   if( handle == F_ERROR )
      return -1;

   if( count == 0 )
      return 0;
#if X_OS_WINDOWS
   int64_t pos = x_fsSeek( handle, 0, FS_RELATIVE );

   if( pos < 0 || x_fsSeek( handle, offset, FS_SET ) < 0 )
      return -1;

   size_t bytes = x_fsWrite( handle, buffer, count );
   int error = s_fileErr;

   x_fsSeek( handle, pos, FS_SET );
   if( error )
      s_fileErr = error;

   return IIF( s_fileErr, -1, ( int64_t )bytes );
#else
   size_t size = IIF( count > ( size_t )SSIZE_MAX, ( size_t )SSIZE_MAX, count );
   ssize_t bytes = pwrite( ( int )handle, buffer, size, ( off_t )offset );

   s_fileResult( IIF( bytes < 0, -1, 0 ) );

   return ( int64_t )bytes;
#endif
}

/* FSeek(): the new position; -1 on error */
int64_t x_fsSeek( int64_t handle, int64_t offset, int mode )
{
#if X_OS_WINDOWS
   LARGE_INTEGER move, pos;

   move.QuadPart = offset;
   int result = SetFilePointerEx( ( HANDLE )( intptr_t )handle, move, &pos, ( DWORD )mode );

   s_fileErr = IIF( result, 0, ( int )GetLastError() );

   return IIF( result, pos.QuadPart, -1 );
#else
   int origin = SEEK_SET;

   if( mode == FS_RELATIVE )
      origin = SEEK_CUR;
   else if( mode == FS_END )
      origin = SEEK_END;

   off_t pos = lseek( ( int )handle, ( off_t )offset, origin );

   s_fileResult( IIF( pos == ( off_t )-1, -1, 0 ) );

   return ( int64_t )pos;
#endif
}

/* FErase() */
int x_fsDelete( const char *path )
{
#if X_OS_WINDOWS
   wchar_t *name = s_fileName( path );
   int result = DeleteFileW( name ) != 0;

   s_fileErr = IIF( result, 0, ( int )GetLastError() );
   x_xfree( name );

   return result;
#else
   int result = unlink( path );

   s_fileResult( result );

   return result == 0;
#endif
}

/* CurDir(): the current directory of a drive without the drive and the leading separator */
char *x_fsCurDir( int drive )
{
#if X_OS_WINDOWS
   wchar_t *wide = _wgetdcwd( drive, NULL, 0 );

   if( wide == NULL )
   {
      s_fileErr = ( int )_doserrno;
      return NULL;
   }

   s_fileErr = 0;
   char *path = s_fileText( wide );

   free( wide );
   if( path == NULL )
      return NULL;

   char *start = path;

   if( start[0] && start[1] == ':' )
      start += 2;

   if( *start == '\\' || *start == '/' )
      ++start;

   size_t len = strlen( start );

   if( len && ( start[len - 1] == '\\' || start[len - 1] == '/' ) )
      --len;
#else
   ( void )drive;
   char *cwd = getcwd( NULL, 0 );

   s_fileResult( IIF( cwd == NULL, -1, 0 ) );
   if( cwd == NULL )
      return NULL;

   char *path = x_strdup( cwd );

   free( cwd );
   char *start = path;

   if( *start == '/' )
      ++start;

   size_t len = strlen( start );

   if( len && start[len - 1] == '/' )
      --len;
#endif
   memmove( path, start, len );
   path[len] = '\0';

   return path;
}

/* DirChange() */
int x_fsDirChange( const char *path )
{
#if X_OS_WINDOWS
   wchar_t *name = s_fileName( path );
   int result = SetCurrentDirectoryW( name ) != 0;

   s_fileErr = IIF( result, 0, ( int )GetLastError() );
   x_xfree( name );

   return result;
#else
   int result = chdir( path );

   s_fileResult( result );

   return result == 0;
#endif
}

/* MakeDir() */
int x_fsDirMake( const char *path )
{
#if X_OS_WINDOWS
   wchar_t *name = s_fileName( path );
   int result = CreateDirectoryW( name, NULL ) != 0;

   s_fileErr = IIF( result, 0, ( int )GetLastError() );
   x_xfree( name );

   return result;
#else
   int result = mkdir( path, 0777 );

   s_fileResult( result );

   return result == 0;
#endif
}

/* DirBuild(): a directory with all missing parents */
int x_fsDirBuild( const char *path )
{
   int isFile, isDir;

   if( x_fsExists( path, &isFile, &isDir ) )
   {
      s_fileErr = IIF( isDir, 0, 5 );
      return isDir;
   }

   if( s_fileErr != 2 && s_fileErr != 3 )
      return 0;

#if X_OS_WINDOWS
   const char *seps = "/\\";
#else
   const char *seps = "/";
#endif
   char *name = x_strdup( path );
   size_t length = strlen( name );

   while( length > 1 && strchr( seps, name[length - 1] ) != NULL )
      name[--length] = '\0';

   size_t parent = length;

   while( parent > 0 && strchr( seps, name[parent - 1] ) == NULL )
      --parent;

   if( parent > 1 )
   {
      char sep = name[parent - 1];

      name[parent - 1] = '\0';
      int result = x_fsDirBuild( name );

      name[parent - 1] = sep;
      if( !result )
      {
         x_xfree( name );
         return 0;
      }
   }

   int result = x_fsDirMake( name );

   x_xfree( name );

   return result;
}

/* DirRemove(): an empty directory */
int x_fsDirRemove( const char *path )
{
#if X_OS_WINDOWS
   wchar_t *name = s_fileName( path );
   int result = RemoveDirectoryW( name ) != 0;

   s_fileErr = IIF( result, 0, ( int )GetLastError() );
   x_xfree( name );

   return result;
#else
   int result = rmdir( path );

   s_fileResult( result );

   return result == 0;
#endif
}

/* DirUnbuild(): a directory and its parents, as long as they are empty */
int x_fsDirUnbuild( const char *path )
{
   int isFile, isDir;

   if( !x_fsExists( path, &isFile, &isDir ) )
   {
      if( s_fileErr != 2 )
         return 0;

      s_fileErr = 0;
      return 1;
   }

   if( !isDir )
   {
      s_fileErr = 5;
      return 0;
   }

   char *name = x_strdup( path );
   size_t length = strlen( name );
   size_t root = 0;
#if X_OS_WINDOWS
   const char *seps = "/\\";

   if( length >= 2 && name[1] == ':' )
      root = 2;
   else if( length >= 2 && strchr( seps, name[0] ) && strchr( seps, name[1] ) )
   {
      int parts = 2;

      /* A path \\?\UNC\server\share has four parts */
      if( length >= 8 && name[2] == '?' && strchr( seps, name[3] ) && strchr( seps, name[7] ) )
      {
         if( x_strnicmp( name + 4, "UNC", 3 ) == 0 )
            parts = 4;
      }

      root = 2;
      while( parts-- > 0 && root < length )
      {
         while( root < length && strchr( seps, name[root] ) )
            ++root;

         while( root < length && !strchr( seps, name[root] ) )
            ++root;
      }
   }
#else
   const char *seps = "/";
#endif
   while( root < length && strchr( seps, name[root] ) )
      ++root;

   while( length > root )
   {
      while( length > root && strchr( seps, name[length - 1] ) )
         name[--length] = '\0';

      if( length == root )
         break;

      size_t start = length;

      while( start > root && !strchr( seps, name[start - 1] ) )
         --start;

      if( strcmp( name + start, "." ) == 0 || strcmp( name + start, ".." ) == 0 )
         break;

      if( !x_fsDirRemove( name ) )
      {
         x_xfree( name );
         return 0;
      }

      length = start;
      name[length] = '\0';
   }

   x_xfree( name );
   s_fileErr = 0;

   return 1;
}

/* DirSpace(): the bytes of the disk of a directory by DISK_AVAIL, DISK_FREE, DISK_USED or DISK_TOTAL */
int64_t x_fsDirSpace( const char *path, int type )
{
   uint64_t avail, freeBytes, total, used;
#if X_OS_WINDOWS
   ULARGE_INTEGER a, f, t;
   wchar_t *name = s_fileName( path );
   int result = GetDiskFreeSpaceExW( name, &a, &t, &f ) != 0;

   s_fileErr = IIF( result, 0, ( int )GetLastError() );
   x_xfree( name );
   if( !result )
      return -1;

   avail = a.QuadPart;
   freeBytes = f.QuadPart;
   total = t.QuadPart;
   used = total - avail;
#else
   struct statvfs info;
   int result = statvfs( path, &info );

   s_fileResult( result );
   if( result != 0 )
      return -1;

   avail = ( uint64_t )info.f_bavail * info.f_frsize;
   freeBytes = ( uint64_t )info.f_bfree * info.f_frsize;
   total = ( uint64_t )info.f_blocks * info.f_frsize;
   used = total - freeBytes;
#endif
   switch( type )
   {
   case DISK_FREE:
      return ( int64_t )freeBytes;

   case DISK_USED:
      return ( int64_t )used;

   case DISK_TOTAL:
      return ( int64_t )total;

   default:
      return ( int64_t )avail;
   }
}

/* The table of the images of the program; a global constructor of the program gives it before main() */
void x_resourcesRegister( const X_RESOURCE *resources )
{
   s_resources = resources;
}

/* A file put into the program, found by its name without extension; NULL when there is none */
const unsigned char *x_resourceFind( const char *name, size_t *length )
{
   const X_RESOURCE *resource;

   for( resource = s_resources; resource->name != NULL; ++resource )
   {
      if( x_stricmp( resource->name, name ) == 0 )
      {
         *length = resource->size;
         return resource->data;
      }
   }

   *length = 0;

   return NULL;
}

/* FileLoad(): a whole file of at most limit bytes, allocated by x_xgrab(); NULL on error */
char *x_fileLoad( const char *path, size_t limit, size_t *length )
{
   *length = 0;
   int64_t handle = x_fsOpen( path, FO_READ | FO_DENYNONE );

   if( handle == F_ERROR )
      return NULL;

   size_t size = IIF( limit < 4096, limit, 4096 );
   char *buffer = ( char * )x_xgrab( size + 1 );

   while( *length < limit )
   {
      if( *length == size )
      {
         size += IIF( size > limit - size, limit - size, size );
         buffer = ( char * )x_xrealloc( buffer, size + 1 );
      }

      size_t bytes = x_fsRead( handle, buffer + *length, size - *length );

      if( s_fileErr || bytes == 0 )
         break;

      *length += bytes;
   }

   int error = s_fileErr;

   x_fsClose( handle );
   if( error )
      s_fileErr = error;

   if( s_fileErr )
   {
      x_xfree( buffer );
      *length = 0;
      return NULL;
   }

   buffer[*length] = '\0';

   return buffer;
}

/* FileSave(): a file created anew with the data */
int x_fileSave( const char *path, const char *buffer, size_t length )
{
   int64_t handle = x_fsCreate( path, FC_NORMAL );

   if( handle == F_ERROR )
      return 0;

   size_t pos = 0;

   while( pos < length )
   {
      size_t bytes = x_fsWrite( handle, buffer + pos, length - pos );

      if( s_fileErr )
         break;

      if( bytes == 0 )
      {
         s_fileErr = 29;
         break;
      }

      pos += bytes;
   }

   int error = s_fileErr;

   x_fsClose( handle );
   if( error )
      s_fileErr = error;

   return s_fileErr == 0;
}

/* The attributes of a file as FA_* flags */
int x_fsAttrGet( const char *path, uint32_t *attr )
{
#if X_OS_WINDOWS
   wchar_t *name = s_fileName( path );
   DWORD flags = GetFileAttributesW( name );

   s_fileErr = IIF( flags == INVALID_FILE_ATTRIBUTES, ( int )GetLastError(), 0 );
   x_xfree( name );
   if( flags == INVALID_FILE_ATTRIBUTES )
      return 0;

   *attr = flags & ( FA_READONLY | FA_HIDDEN | FA_SYSTEM | FA_DIRECTORY | FA_ARCHIVE );
#else
   struct stat info;
   int result = stat( path, &info );

   s_fileResult( result );
   if( result != 0 )
      return 0;

   *attr = 0;
   if( !( info.st_mode & ( S_IWUSR | S_IWGRP | S_IWOTH ) ) )
      *attr |= FA_READONLY;

   if( S_ISDIR( info.st_mode ) )
      *attr |= FA_DIRECTORY;

   if( S_ISREG( info.st_mode ) )
      *attr |= FA_ARCHIVE;
#endif
   if( *attr == 0 )
      *attr = FA_NORMAL;

   return 1;
}

/* Set the FA_* attributes of a file; on Linux only FA_READONLY */
int x_fsAttrSet( const char *path, uint32_t attr )
{
   if( attr & ~( FA_READONLY | FA_HIDDEN | FA_SYSTEM | FA_DIRECTORY | FA_ARCHIVE | FA_NORMAL ) )
   {
      s_fileErr = 50;
      return 0;
   }
#if X_OS_WINDOWS
   DWORD flags = attr & ( FA_READONLY | FA_HIDDEN | FA_SYSTEM | FA_ARCHIVE );
   wchar_t *name = s_fileName( path );
   int result = SetFileAttributesW( name, IIF( flags, flags, FILE_ATTRIBUTE_NORMAL ) ) != 0;

   s_fileErr = IIF( result, 0, ( int )GetLastError() );
   x_xfree( name );

   return result;
#else
   if( attr & ( FA_HIDDEN | FA_SYSTEM ) )
   {
      s_fileErr = 50;
      return 0;
   }

   struct stat info;
   int result = stat( path, &info );

   s_fileResult( result );
   if( result != 0 )
      return 0;

   mode_t mode = info.st_mode & 07777;

   if( attr & FA_READONLY )
      mode &= ~( S_IWUSR | S_IWGRP | S_IWOTH );
   else
      mode |= S_IWUSR;

   result = chmod( path, mode );
   s_fileResult( result );

   return result == 0;
#endif
}

/* The time of the last change of a file, in milliseconds since 1970 UTC */
int x_fsTimeGet( const char *path, int64_t *timeMs )
{
#if X_OS_WINDOWS
   DWORD flags = FILE_FLAG_BACKUP_SEMANTICS;
   int64_t handle = s_fileOpen( path, FILE_READ_ATTRIBUTES, X_FILE_SHARE_ALL, OPEN_EXISTING, flags );

   if( handle == F_ERROR )
      return 0;

   FILETIME time;
   int result = GetFileTime( ( HANDLE )( intptr_t )handle, NULL, NULL, &time ) != 0;
   int error = IIF( result, 0, ( int )GetLastError() );

   x_fsClose( handle );
   if( error )
      s_fileErr = error;

   if( s_fileErr )
      return 0;

   uint64_t ticks = ( ( uint64_t )time.dwHighDateTime << 32 ) | time.dwLowDateTime;

   *timeMs = ( int64_t )( ticks / 10000 ) - X_FILE_EPOCH;
#else
   struct stat info;
   int result = stat( path, &info );

   s_fileResult( result );
   if( result != 0 )
      return 0;

   *timeMs = ( int64_t )info.st_mtim.tv_sec * 1000 + info.st_mtim.tv_nsec / 1000000;
#endif

   return 1;
}

/* Set the time of the last change of a file, in milliseconds since 1970 UTC */
int x_fsTimeSet( const char *path, int64_t timeMs )
{
#if X_OS_WINDOWS
   /* The range of a FILETIME */
   if( timeMs < -X_FILE_EPOCH || timeMs > INT64_MAX / 10000 - X_FILE_EPOCH )
   {
      s_fileErr = 13;
      return 0;
   }

   uint64_t ticks = ( uint64_t )( timeMs + X_FILE_EPOCH ) * 10000;
   FILETIME time = { ( DWORD )ticks, ( DWORD )( ticks >> 32 ) };

   DWORD flags = FILE_FLAG_BACKUP_SEMANTICS;
   int64_t handle = s_fileOpen( path, FILE_WRITE_ATTRIBUTES, X_FILE_SHARE_ALL, OPEN_EXISTING, flags );

   if( handle == F_ERROR )
      return 0;

   int result = SetFileTime( ( HANDLE )( intptr_t )handle, NULL, NULL, &time ) != 0;
   int error = IIF( result, 0, ( int )GetLastError() );

   x_fsClose( handle );
   if( error )
      s_fileErr = error;

   return s_fileErr == 0;
#else
   struct timespec times[2] = { { 0, UTIME_OMIT }, { 0, 0 } };
   int64_t seconds = timeMs / 1000;
   int64_t millis = timeMs % 1000;

   if( millis < 0 )
   {
      --seconds;
      millis += 1000;
   }

   times[1].tv_sec = ( time_t )seconds;
   times[1].tv_nsec = ( long )( millis * 1000000 );
   if( ( int64_t )times[1].tv_sec != seconds )
   {
      s_fileErr = 13;
      return 0;
   }

   int result = utimensat( AT_FDCWD, path, times, 0 );

   s_fileResult( result );

   return result == 0;
#endif
}

/* FileLink(): a hard link */
int x_fsLink( const char *source, const char *target )
{
#if X_OS_WINDOWS
   wchar_t *from = s_fileName( source );
   wchar_t *to = s_fileName( target );
   int result = CreateHardLinkW( to, from, NULL ) != 0;

   s_fileErr = IIF( result, 0, ( int )GetLastError() );
   x_xfree( to );
   x_xfree( from );

   return result;
#else
   int result = link( source, target );

   s_fileResult( result );

   return result == 0;
#endif
}

/* FileLinkSym(): a symbolic link */
int x_fsLinkSym( const char *target, const char *name, int isDir )
{
#if X_OS_WINDOWS
   wchar_t *dest = s_fileName( target );
   wchar_t *link = s_fileName( name );
   DWORD flags = SYMBOLIC_LINK_FLAG_ALLOW_UNPRIVILEGED_CREATE;

   if( isDir )
      flags |= SYMBOLIC_LINK_FLAG_DIRECTORY;

   int result = CreateSymbolicLinkW( link, dest, flags ) != 0;

   s_fileErr = IIF( result, 0, ( int )GetLastError() );
   x_xfree( link );
   x_xfree( dest );

   return result;
#else
   ( void )isDir;
   int result = symlink( target, name );

   s_fileResult( result );

   return result == 0;
#endif
}

/* FileLinkRead(): the target of a symbolic link, allocated by x_xgrab(); NULL on error */
char *x_fsLinkRead( const char *path )
{
#if X_OS_WINDOWS
   DWORD flags = FILE_FLAG_OPEN_REPARSE_POINT | FILE_FLAG_BACKUP_SEMANTICS;
   int64_t handle = s_fileOpen( path, 0, X_FILE_SHARE_ALL, OPEN_EXISTING, flags );

   if( handle == F_ERROR )
      return NULL;

   typedef struct
   {
      DWORD tag;
      WORD size, reserved, subOff, subLen, printOff, printLen;
   } X_LINK;
   X_LINK *info = ( X_LINK * )x_xgrab( X_REPARSE_SIZE );
   DWORD bytes;

   HANDLE file = ( HANDLE )( intptr_t )handle;
   int result = DeviceIoControl( file, FSCTL_GET_REPARSE_POINT, NULL, 0, info, X_REPARSE_SIZE, &bytes, NULL ) != 0;
   int error = IIF( result, 0, ( int )GetLastError() );

   x_fsClose( handle );
   if( error )
      s_fileErr = error;

   char *text = NULL;

   if( s_fileErr == 0 && bytes < sizeof( X_LINK ) )
      s_fileErr = ERROR_INVALID_REPARSE_DATA;

   if( s_fileErr == 0 )
   {
      size_t base = IIF( info->tag == IO_REPARSE_TAG_SYMLINK, 20, 16 );
      size_t off = IIF( info->printLen, info->printOff, info->subOff );
      size_t len = IIF( info->printLen, info->printLen, info->subLen );

      int linkTag = info->tag == IO_REPARSE_TAG_SYMLINK || info->tag == IO_REPARSE_TAG_MOUNT_POINT;

      if( !linkTag || bytes < base || off + len > bytes - base || ( ( off | len ) & 1 ) )
         s_fileErr = ERROR_INVALID_REPARSE_DATA;
      else
      {
         const wchar_t *name = ( const wchar_t * )( ( const char * )info + base + off );
         int size = WideCharToMultiByte( CP_UTF8, 0, name, ( int )( len / 2 ), NULL, 0, NULL, NULL );

         if( size == 0 )
            s_fileErr = ( int )GetLastError();
         else
         {
            text = ( char * )x_xgrab( ( size_t )size + 1 );
            WideCharToMultiByte( CP_UTF8, 0, name, ( int )( len / 2 ), text, size, NULL, NULL );
            text[size] = '\0';
         }
      }
   }

   x_xfree( info );

   return text;
#else
   size_t size = 256;
   char *text = ( char * )x_xgrab( size + 1 );

   for( ;; )
   {
      ssize_t bytes = readlink( path, text, size );

      s_fileResult( IIF( bytes < 0, -1, 0 ) );
      if( bytes < 0 )
      {
         x_xfree( text );
         return NULL;
      }

      if( ( size_t )bytes < size )
      {
         text[bytes] = '\0';
         return text;
      }

      size *= 2;
      text = ( char * )x_xrealloc( text, size + 1 );
   }
#endif
}

/* FileTemp(): a new file with a unique name in dir, the temporary directory for ""; *name gets the name */
void *x_fileTemp( char **name, const char *dir, const char *prefix, const char *ext, int attr )
{
   *name = NULL;
   char *base = NULL;
   static uint64_t serial;
#if X_OS_WINDOWS
   if( *dir == '\0' )
   {
      DWORD size = GetTempPathW( 0, NULL );

      if( size == 0 )
      {
         s_fileErr = ( int )GetLastError();
         return NULL;
      }

      wchar_t *path = ( wchar_t * )x_xgrabArray( ( size_t )size, sizeof( wchar_t ) );
      DWORD len = GetTempPathW( size, path );

      if( len == 0 || len >= size )
      {
         s_fileErr = IIF( len == 0, ( int )GetLastError(), ERROR_INSUFFICIENT_BUFFER );
         x_xfree( path );
         return NULL;
      }

      base = s_fileText( path );
      x_xfree( path );
      if( base == NULL )
         return NULL;

      dir = base;
   }

   unsigned long pid = GetCurrentProcessId();
   uint64_t stamp = GetTickCount64();
   const char *seps = "/\\";
#else
   if( *dir == '\0' )
   {
      dir = getenv( "TMPDIR" );
      if( dir == NULL || *dir == '\0' )
         dir = "/tmp";
   }

   unsigned long pid = ( unsigned long )getpid();
   uint64_t stamp = ( uint64_t )time( NULL );
   const char *seps = "/";
#endif
   size_t len = strlen( dir );
   const char *sep = IIF( len && strchr( seps, dir[len - 1] ), "", "/" );
   size_t size = len + strlen( prefix ) + strlen( ext ) + 80;
   char *path = ( char * )x_xgrab( size );
   int64_t handle = F_ERROR;

   for( int i = 0; i < 256; ++i )
   {
      unsigned long long number = ++serial;

      snprintf( path, size, "%s%s%s%lx-%llx-%llx%s", dir, sep, prefix, pid, ( unsigned long long )stamp, number, ext );
#if X_OS_WINDOWS
      DWORD flags = 0;

      if( attr & FC_READONLY )
         flags |= FILE_ATTRIBUTE_READONLY;

      if( attr & FC_HIDDEN )
         flags |= FILE_ATTRIBUTE_HIDDEN;

      if( attr & FC_SYSTEM )
         flags |= FILE_ATTRIBUTE_SYSTEM;

      if( flags == 0 )
         flags = FILE_ATTRIBUTE_NORMAL;

      handle = s_fileOpen( path, GENERIC_READ | GENERIC_WRITE, 0, CREATE_NEW, flags );
#else
      handle = open( path, O_CREAT | O_EXCL | O_RDWR, IIF( attr & FC_READONLY, 0400, 0600 ) );
      s_fileResult( ( int )handle );
#endif
      if( handle != F_ERROR || ( s_fileErr != 80 && s_fileErr != 183 ) )
         break;
   }

   x_xfree( base );
   if( handle == F_ERROR )
   {
      x_xfree( path );
      return NULL;
   }

   X_FILE *file = ( X_FILE * )x_gcAllocate( sizeof( X_FILE ), &x_fileGC );

   file->handle = handle;
   *name = path;

   return file;
}

/* FileMove(): rename, or copy and delete across file systems; the target is replaced */
int x_fileMove( const char *source, const char *target )
{
#if X_OS_WINDOWS
   wchar_t *from = s_fileName( source );
   wchar_t *to = s_fileName( target );
   int result = MoveFileExW( from, to, MOVEFILE_REPLACE_EXISTING ) != 0;

   s_fileErr = IIF( result, 0, ( int )GetLastError() );
   x_xfree( to );
   x_xfree( from );
   if( result || s_fileErr != ERROR_NOT_SAME_DEVICE )
      return result;

   uint32_t attr;

   if( !x_fsAttrGet( source, &attr ) )
      return 0;

   if( attr & FA_DIRECTORY )
   {
      s_fileErr = 50;
      return 0;
   }

   return x_fileCopy( source, target ) && x_fsDelete( source );
#else
   int result = rename( source, target );

   if( result == 0 || errno != EXDEV )
   {
      s_fileResult( result );
      return result == 0;
   }

   struct stat info;

   result = lstat( source, &info );
   s_fileResult( result );
   if( result != 0 )
      return 0;

   if( !S_ISREG( info.st_mode ) )
   {
      s_fileErr = 50;
      return 0;
   }

   int64_t timeMs;

   /* Across file systems: copy with the time, then delete */
   if( !x_fsTimeGet( source, &timeMs ) || !x_fileCopy( source, target ) )
      return 0;

   return x_fsTimeSet( target, timeMs ) && x_fsDelete( source );
#endif
}

/* FileCopy(): copy a file; the target is replaced */
int x_fileCopy( const char *source, const char *target )
{
#if X_OS_WINDOWS
   wchar_t *from = s_fileName( source );
   wchar_t *to = s_fileName( target );
   int result = CopyFileW( from, to, FALSE ) != 0;

   s_fileErr = IIF( result, 0, ( int )GetLastError() );
   x_xfree( to );
   x_xfree( from );

   return result;
#else
   int64_t from = x_fsOpen( source, FO_READ | FO_DENYNONE );

   if( from == F_ERROR )
      return 0;

   int to = -1;
   int error;
   struct stat src, dst;
   int result = fstat( ( int )from, &src );

   s_fileResult( result );
   if( result != 0 )
      goto done;

   to = open( target, O_CREAT | O_WRONLY, 0666 );
   s_fileResult( to );
   if( to < 0 )
      goto done;

   result = fstat( to, &dst );
   s_fileResult( result );
   if( result != 0 )
      goto done;

   if( src.st_dev == dst.st_dev && src.st_ino == dst.st_ino )
   {
      s_fileErr = 13;
      goto done;
   }

   result = ftruncate( to, 0 );
   s_fileResult( result );
   if( result != 0 )
      goto done;

   char buffer[65536];

   for( ;; )
   {
      size_t bytes = x_fsRead( from, buffer, sizeof( buffer ) );

      if( s_fileErr || bytes == 0 )
         break;

      size_t pos = 0;

      while( pos < bytes )
      {
         size_t count = x_fsWrite( to, buffer + pos, bytes - pos );

         if( s_fileErr )
            goto done;

         if( count == 0 )
         {
            s_fileErr = 29;
            goto done;
         }

         pos += count;
      }
   }

   if( s_fileErr == 0 )
      s_fileResult( fchmod( to, src.st_mode & 0777 ) );

done:
   error = s_fileErr;
   if( to >= 0 )
   {
      x_fsClose( to );
      if( error == 0 )
         error = s_fileErr;
   }

   x_fsClose( from );
   if( error )
      s_fileErr = error;

   return s_fileErr == 0;
#endif
}

typedef struct
{
   int64_t from, to;
   char *buffer;
} X_COPY;

/* Close the files of an interrupted FileCopyEx() */
static void s_copyDrop( X_COPY *copy )
{
   int error = s_fileErr;

   if( copy->to != F_ERROR )
   {
      x_fsClose( copy->to );
      copy->to = F_ERROR;
      if( error == 0 )
         error = s_fileErr;
   }

   if( copy->from != F_ERROR )
   {
      x_fsClose( copy->from );
      copy->from = F_ERROR;
      if( error == 0 )
         error = s_fileErr;
   }

   x_xfree( copy->buffer );
   copy->buffer = NULL;
   s_fileErr = error;
}

/* FileCopyEx(): copy in blocks of size bytes, keep the time, and report the progress to the block */
int x_fileCopyEx( X_CALL_CTX *ctx, const char *source, const char *target, size_t size,
                  int keepTime, const X_ITEM *block )
{
   X_COPY copy __attribute__( ( cleanup( s_copyDrop ) ) ) = { F_ERROR, F_ERROR, NULL };
   X_ITEM code __attribute__( ( cleanup( x_itemClear ) ) ) = X_ITEM_NIL_INIT;

   if( block != NULL )
      x_itemCopy( &code, block );

   copy.from = x_fsOpen( source, FO_READ | FO_DENYWRITE );
   if( copy.from == F_ERROR )
      return 0;

   int64_t total = s_fileSize( copy.from );

   if( total < 0 )
      return 0;

   int64_t timeMs = 0;

   if( keepTime && !x_fsTimeGet( source, &timeMs ) )
      return 0;
#if X_OS_WINDOWS
   uint32_t attr;

   if( !x_fsAttrGet( source, &attr ) )
      return 0;

   copy.to = x_fsCreate( target, FC_NORMAL );
   if( copy.to == F_ERROR )
      return 0;
#else
   struct stat src, dst;
   int result = fstat( ( int )copy.from, &src );

   s_fileResult( result );
   if( result != 0 )
      return 0;

   copy.to = open( target, O_CREAT | O_WRONLY, 0666 );
   s_fileResult( ( int )copy.to );
   if( copy.to == F_ERROR )
      return 0;

   result = fstat( ( int )copy.to, &dst );
   s_fileResult( result );
   if( result != 0 )
      return 0;

   if( src.st_dev == dst.st_dev && src.st_ino == dst.st_ino )
   {
      s_fileErr = 13;
      return 0;
   }

   result = ftruncate( ( int )copy.to, 0 );
   s_fileResult( result );
   if( result != 0 )
      return 0;
#endif
   copy.buffer = ( char * )x_xgrab( size );
   int64_t copied = 0;

   for( ;; )
   {
      if( X_IS_BLOCK( &code ) )
      {
         X_ITEM done = { X_IT_INTEGER, { .integer = copied } };
         X_ITEM full = { X_IT_INTEGER, { .integer = total } };

         X_ITEM result __attribute__( ( cleanup( x_itemClear ) ) ) = X_ITEM_NIL_INIT;
         X_ARG args[2] = { { &done, 0 }, { &full, 0 } };

         x_evalBlock( ctx, &result, &code, 2, args );
         if( X_IS_LOGICAL( &result ) && !result.u.logical )
         {
            s_fileErr = 1223;
            return 0;
         }
      }

      size_t bytes = x_fsRead( copy.from, copy.buffer, size );

      if( s_fileErr )
         return 0;

      if( bytes == 0 )
         break;

      size_t pos = 0;

      while( pos < bytes )
      {
         size_t count = x_fsWrite( copy.to, copy.buffer + pos, bytes - pos );

         if( s_fileErr )
            return 0;

         if( count == 0 )
         {
            s_fileErr = 29;
            return 0;
         }

         pos += count;
      }

      copied += ( int64_t )bytes;
   }
#if !X_OS_WINDOWS
   s_fileResult( fchmod( ( int )copy.to, src.st_mode & 0777 ) );
#endif
   s_copyDrop( &copy );
   if( s_fileErr )
      return 0;

   if( keepTime && !x_fsTimeSet( target, timeMs ) )
      return 0;
#if X_OS_WINDOWS
   return x_fsAttrSet( target, attr );
#else
   return 1;
#endif
}

/* One row of Directory(): name, size, date, time and attributes */
static void s_dirAdd( X_ITEM *result, const char *name, int64_t size, const struct tm *time,
                       uint32_t attr, uint32_t mask )
{
   if( ( attr & ( FA_HIDDEN | FA_SYSTEM | FA_DIRECTORY ) ) & ~mask )
      return;

   X_ITEM row __attribute__( ( cleanup( x_itemClear ) ) ) = X_ITEM_NIL_INIT;
   X_ITEM date = { X_IT_DATE, { .integer = x_dateEncode( time->tm_year + 1900, time->tm_mon + 1, time->tm_mday ) } };
   char clock[16], flags[8];
   int n = 0;

   if( attr & FA_READONLY )
      flags[n++] = 'R';

   if( attr & FA_HIDDEN )
      flags[n++] = 'H';

   if( attr & FA_SYSTEM )
      flags[n++] = 'S';

   if( attr & FA_DIRECTORY )
      flags[n++] = 'D';

   if( attr & FA_ARCHIVE )
      flags[n++] = 'A';

   if( attr & 0x400 )
      flags[n++] = 'L';

   flags[n] = '\0';
   snprintf( clock, sizeof( clock ), "%02d:%02d:%02d", time->tm_hour, time->tm_min, time->tm_sec );
   x_arrayNew( &row, 5 );
   x_arraySetC( &row, 1, name );
   x_arraySetNInt( &row, 2, size );
   x_arraySet( &row, 3, &date );
   x_arraySetC( &row, 4, clock );
   x_arraySetC( &row, 5, flags );
   x_arrayAdd( result, &row );
}

/* Directory(): the files of a mask; attr adds hidden (H), system (S) and directory (D) entries */
void x_fileDirectory( X_ITEM *result, const char *spec, const char *attr )
{
   uint32_t mask = 0;
   const char *p;

   x_arrayNew( result, 0 );
   for( p = attr; *p; ++p )
   {
      switch( *p )
      {
      case 'h':
      case 'H':
         mask |= FA_HIDDEN;
         break;

      case 's':
      case 'S':
         mask |= FA_SYSTEM;
         break;

      case 'd':
      case 'D':
         mask |= FA_DIRECTORY;
         break;

      case 'r':
      case 'R':
      case 'a':
      case 'A':
      case 'l':
      case 'L':
         break;

      default:
         s_fileErr = 50;
         return;
      }
   }

   if( *spec == '\0' )
      spec = "*";
#if X_OS_WINDOWS
   size_t len = strlen( spec );
   char *pattern = ( char * )x_xgrab( len + 2 );

   memcpy( pattern, spec, len + 1 );
   if( strchr( "/\\:", spec[len - 1] ) )
   {
      pattern[len] = '*';
      pattern[len + 1] = '\0';
   }

   wchar_t *wide = s_fileName( pattern );
   WIN32_FIND_DATAW info;
   HANDLE find = FindFirstFileW( wide, &info );
   int error = IIF( find == INVALID_HANDLE_VALUE, ( int )GetLastError(), 0 );

   x_xfree( wide );
   x_xfree( pattern );
   if( find == INVALID_HANDLE_VALUE )
   {
      s_fileErr = IIF( error == ERROR_FILE_NOT_FOUND, 0, error );
      return;
   }

   do
   {
      char *name = s_fileText( info.cFileName );

      if( name == NULL )
      {
         error = s_fileErr;
         break;
      }

      FILETIME local;
      SYSTEMTIME stamp;
      int ok = FileTimeToLocalFileTime( &info.ftLastWriteTime, &local ) && FileTimeToSystemTime( &local, &stamp );

      if( !ok )
      {
         error = ( int )GetLastError();
         x_xfree( name );
         break;
      }

      struct tm time = { 0 };

      time.tm_year = stamp.wYear - 1900;
      time.tm_mon = stamp.wMonth - 1;
      time.tm_mday = stamp.wDay;
      time.tm_hour = stamp.wHour;
      time.tm_min = stamp.wMinute;
      time.tm_sec = stamp.wSecond;
      int64_t size = ( int64_t )( ( ( uint64_t )info.nFileSizeHigh << 32 ) | info.nFileSizeLow );

      s_dirAdd( result, name, size, &time, info.dwFileAttributes, mask );
      x_xfree( name );
      if( !FindNextFileW( find, &info ) )
      {
         error = ( int )GetLastError();
         if( error == ERROR_NO_MORE_FILES )
            error = 0;
         break;
      }
   } while( 1 );
   if( !FindClose( find ) && error == 0 )
      error = ( int )GetLastError();

   s_fileErr = error;
#else
   char *pattern = x_strdup( spec );
   char *slash = strrchr( pattern, '/' );
   const char *dir = ".";
   const char *match = pattern;

   if( slash != NULL )
   {
      *slash = '\0';
      dir = IIF( slash == pattern, "/", pattern );
      match = slash + 1;
   }

   if( *match == '\0' || strcmp( match, "*.*" ) == 0 )
      match = "*";

   DIR *find = opendir( dir );

   s_fileResult( IIF( find == NULL, -1, 0 ) );
   if( find == NULL )
   {
      x_xfree( pattern );
      return;
   }

   int error = 0;

   for( ;; )
   {
      errno = 0;
      struct dirent *entry = readdir( find );

      if( entry == NULL )
      {
         s_fileResult( IIF( errno, -1, 0 ) );
         error = s_fileErr;
         break;
      }

      if( fnmatch( match, entry->d_name, 0 ) != 0 )
         continue;

      struct stat info;
      int ok = fstatat( dirfd( find ), entry->d_name, &info, AT_SYMLINK_NOFOLLOW );

      s_fileResult( ok );
      if( ok != 0 )
      {
         error = s_fileErr;
         break;
      }

      uint32_t flags = 0;

      if( !( info.st_mode & 0222 ) )
         flags |= FA_READONLY;

      if( entry->d_name[0] == '.' && strcmp( entry->d_name, "." ) && strcmp( entry->d_name, ".." ) )
         flags |= FA_HIDDEN;

      if( S_ISDIR( info.st_mode ) )
         flags |= FA_DIRECTORY;

      if( S_ISREG( info.st_mode ) )
         flags |= FA_ARCHIVE;

      if( S_ISLNK( info.st_mode ) )
         flags |= 0x400;

      struct tm time;

      if( localtime_r( &info.st_mtime, &time ) == NULL )
      {
         s_fileResult( -1 );
         error = s_fileErr;
         break;
      }

      s_dirAdd( result, entry->d_name, ( int64_t )info.st_size, &time, flags, mask );
   }

   int closed = closedir( find );

   if( error )
      s_fileErr = error;
   else
      s_fileResult( closed );

   x_xfree( pattern );
#endif
}

/* A character that ends the path part of a file name; Windows also has \ and the drive colon */
static int s_pathIsSeparator( char c )
{
   return c == '/' || ( X_OS_WINDOWS && ( c == '\\' || c == ':' ) );
}

/* FNameSplit(): the lengths of the path, with its last separator, and of the name; the rest is the
   extension with its dot. A name that starts with its only dot, like .profile, has no extension. */
void x_fsNameSplit( const char *text, size_t length, size_t *pathLength, size_t *nameLength )
{
   size_t path = length, dot = length;

   while( path > 0 && !s_pathIsSeparator( text[path - 1] ) )
      --path;

   while( dot > path && text[dot - 1] != '.' )
      --dot;

   *pathLength = path;
   *nameLength = IIF( dot > path + 1, dot - 1 - path, length - path );
}

/* A directory and a name joined with a separator, allocated by x_xgrab() */
static char *s_pathJoin( const char *first, const char *second, int separator )
{
   size_t firstLength = strlen( first ), secondLength = strlen( second );
   char *path = ( char * )x_xgrab( firstLength + secondLength + 2 );

   memcpy( path, first, firstLength );
   memcpy( path + firstLength, second, secondLength );
   if( separator )
      path[firstLength + secondLength++] = X_OS_PATH_SEPARATOR;

   path[firstLength + secondLength] = '\0';

   return path;
}

/* "." and ".." are no entries of a directory; a link to a directory ("DL") is not followed */
static int s_dirIsEntry( const char *name )
{
   return strcmp( name, "." ) != 0 && strcmp( name, ".." ) != 0;
}

/* Is the entry a directory other than . and ..? */
static int s_dirIsSubdir( const char *attr )
{
   return strchr( attr, 'D' ) != NULL && strchr( attr, 'L' ) == NULL;
}

/* The files of a mask in a directory and in all its subdirectories */
static void s_dirRecurse( X_ITEM *result, const char *dir, const char *relative, const char *mask, const char *attr,
                          const char *dirAttr )
{
   X_ITEM list __attribute__( ( cleanup( x_itemClear ) ) ) = X_ITEM_NIL_INIT;
   char *spec = s_pathJoin( dir, mask, 0 );
   size_t i;

   x_fileDirectory( &list, spec, attr );
   x_xfree( spec );
   for( i = 1; i <= ( size_t )x_arrayLen( &list ); ++i )
   {
      X_ITEM *row = x_arrayGetItemPtr( &list, i );
      char *name = s_pathJoin( relative, x_arrayGetCPtr( row, 1 ), 0 );

      if( s_dirIsEntry( x_arrayGetCPtr( row, 1 ) ) )
      {
         x_arraySetC( row, 1, name );
         x_arrayAdd( result, row );
      }

      x_xfree( name );
   }

   spec = s_pathJoin( dir, "*", 0 );
   x_fileDirectory( &list, spec, dirAttr );
   x_xfree( spec );
   for( i = 1; i <= ( size_t )x_arrayLen( &list ); ++i )
   {
      X_ITEM *row = x_arrayGetItemPtr( &list, i );
      const char *name = x_arrayGetCPtr( row, 1 );

      if( s_dirIsEntry( name ) && s_dirIsSubdir( x_arrayGetCPtr( row, 5 ) ) )
      {
         char *subdir = s_pathJoin( dir, name, 1 );
         char *subRelative = s_pathJoin( relative, name, 1 );

         s_dirRecurse( result, subdir, subRelative, mask, attr, dirAttr );
         x_xfree( subdir );
         x_xfree( subRelative );
      }
   }
}

/* DirectoryRecurse(): Directory() of the directory in spec and of all its subdirectories; the names
   get the path relative to that directory */
void x_fileDirectoryRecurse( X_ITEM *result, const char *spec, const char *attr )
{
   size_t length = strlen( spec ), split = length, i, n = 0;
   char *dir, *dirAttr = ( char * )x_xgrab( strlen( attr ) + 2 );
   const char *mask;

   while( split > 0 && !s_pathIsSeparator( spec[split - 1] ) )
      --split;

   mask = IIF( spec[split] == '\0', "*", spec + split );
   dir = ( char * )x_xgrab( split + 1 );
   memcpy( dir, spec, split );
   dir[split] = '\0';
   for( i = 0; attr[i]; ++i )
   {
      if( attr[i] != 'D' && attr[i] != 'd' )
         dirAttr[n++] = attr[i];
   }

   dirAttr[n++] = 'D';
   dirAttr[n] = '\0';

   x_arrayNew( result, 0 );
   s_dirRecurse( result, dir, "", mask, attr, dirAttr );
   x_xfree( dir );
   x_xfree( dirAttr );
}

/* DirRemoveAll(): remove a directory with all its contents; a link to a directory is removed, not followed */
int x_fsDirRemoveAll( const char *path )
{
   X_ITEM list __attribute__( ( cleanup( x_itemClear ) ) ) = X_ITEM_NIL_INIT;
   size_t length = strlen( path ), i;
   int ok = 1;
   char *dir = s_pathJoin( path, "", length > 0 && !s_pathIsSeparator( path[length - 1] ) );
   char *spec = s_pathJoin( dir, "*", 0 );

   x_fileDirectory( &list, spec, "HSD" );
   x_xfree( spec );
   for( i = 1; ok && i <= ( size_t )x_arrayLen( &list ); ++i )
   {
      X_ITEM *row = x_arrayGetItemPtr( &list, i );
      const char *name = x_arrayGetCPtr( row, 1 );
      const char *attr = x_arrayGetCPtr( row, 5 );
      char *full;

      if( !s_dirIsEntry( name ) )
         continue;

      full = s_pathJoin( dir, name, 0 );
      if( s_dirIsSubdir( attr ) )
         ok = x_fsDirRemoveAll( full );
      else if( strchr( attr, 'D' ) )
         ok = x_fsDirRemove( full );
      else
         ok = x_fsDelete( full );

      x_xfree( full );
   }

   x_xfree( dir );

   return ok && x_fsDirRemove( path );
}

/* CurDirX(): the current directory of a drive with a leading and a trailing separator */
char *x_fsCurDirX( int drive )
{
   char *path = x_fsCurDir( drive );
   char *result;
   size_t length, pos = 0;

   if( path == NULL )
      return NULL;

   length = strlen( path );
   result = ( char * )x_xgrab( length + 3 );
   result[pos++] = X_OS_PATH_SEPARATOR;
   memcpy( result + pos, path, length );
   pos += length;
   if( length )
      result[pos++] = X_OS_PATH_SEPARATOR;

   result[pos] = '\0';
   x_xfree( path );

   return result;
}

/* FileRename(): the target must not exist */
int x_fileRename( const char *source, const char *target )
{
#if X_OS_WINDOWS
   return x_fsRename( source, target );
#else
   int result = renameat2( AT_FDCWD, source, AT_FDCWD, target, RENAME_NOREPLACE );

   s_fileResult( result );

   return result == 0;
#endif
}

/* FRename() */
int x_fsRename( const char *source, const char *target )
{
#if X_OS_WINDOWS
   wchar_t *from = s_fileName( source );
   wchar_t *to = s_fileName( target );
   int result = MoveFileW( from, to ) != 0;

   s_fileErr = IIF( result, 0, ( int )GetLastError() );
   x_xfree( to );
   x_xfree( from );

   return result;
#else
   int result = rename( source, target );

   s_fileResult( result );

   return result == 0;
#endif
}

/* The last file error as a code: 0, or the negative system code, -2 file not found, -3 path not found,
   -5 access denied */
int x_fsErrorCode( void )
{
   return -s_fileErr;
}

/* Count bytes of path from offset, all to the end when count < 0; NULL when the file cannot be opened */
char *x_fsReadPart( const char *path, int64_t offset, int64_t count, size_t *length )
{
   int64_t handle = x_fsOpen( path, FO_READ | FO_SHARED );
   int64_t size;
   char *buffer;

   *length = 0;
   if( handle == F_ERROR )
      return NULL;

   size = x_fsSeek( handle, 0, FS_END );
   if( offset > size )
      offset = size;

   if( count < 0 || count > size - offset )
      count = size - offset;

   x_fsSeek( handle, offset, FS_SET );
   buffer = ( char * )x_xgrab( ( size_t )count + 1 );
   *length = x_fsRead( handle, buffer, ( size_t )count );
   buffer[*length] = '\0';
   x_fsClose( handle );

   return buffer;
}

/* Write data into path at offset, at the end when offset < 0; without useExisting the file is created anew, and
   truncate cuts it after the data; the bytes written, -1 when the file cannot be opened */
int64_t x_fsWritePart( const char *path, const char *data, size_t length, int useExisting, int64_t offset,
                       int truncate )
{
   int64_t handle = IIF( useExisting, x_fsOpen( path, FO_READWRITE | FO_SHARED ), F_ERROR );
   int64_t written;

   if( handle == F_ERROR )
      handle = x_fsCreate( path, FC_NORMAL );

   if( handle == F_ERROR )
      return -1;

   if( offset < 0 )
      offset = x_fsSeek( handle, 0, FS_END );
   else
      x_fsSeek( handle, offset, FS_SET );

   written = ( int64_t )x_fsWrite( handle, data, length );
   if( truncate )
      s_fileTruncate( handle, offset + written );

   x_fsClose( handle );

   return written;
}

/* A time in milliseconds since the Unix epoch, as FileTimeGet() gives it, as a local date (a Julian
   day) and the seconds since midnight; a time before 1970 is rounded down to its second */
int x_fsTimeLocal( int64_t timeMs, long *julian, int *seconds )
{
   time_t time = ( time_t )( timeMs / 1000 - IIF( timeMs % 1000 < 0, 1, 0 ) );
   struct tm local;

#if X_OS_WINDOWS
   if( localtime_s( &local, &time ) != 0 )
      return 0;
#else
   if( localtime_r( &time, &local ) == NULL )
      return 0;
#endif
   *julian = x_dateEncode( local.tm_year + 1900, local.tm_mon + 1, local.tm_mday );
   *seconds = local.tm_hour * 3600 + local.tm_min * 60 + local.tm_sec;

   return 1;
}

/* TToC(): a time in milliseconds since the Unix epoch as the local "date hh:mm:ss", the date in the
   SET DATE format; buffer holds at least 24 characters */
int x_fsTimeText( int64_t timeMs, char *buffer, size_t size )
{
   long julian;
   int seconds, year, month, day;
   char date[9], text[13];

   if( !x_fsTimeLocal( timeMs, &julian, &seconds ) )
      return 0;

   x_dateDecode( julian, &year, &month, &day );
   snprintf( date, sizeof( date ), "%04d%02d%02d", year, month, day );
   x_dateFormat( date, text, x_setDateFormat( NULL ) );
   snprintf( buffer, size, "%s %02d:%02d:%02d", text, seconds / 3600, seconds / 60 % 60, seconds % 60 );

   return 1;
}

/* The modification time of path as a local date (a Julian day) and the seconds since midnight */
int x_fsTimeLocalGet( const char *path, long *julian, int *seconds )
{
   int64_t timeMs;

   return x_fsTimeGet( path, &timeMs ) && x_fsTimeLocal( timeMs, julian, seconds );
}

/* Set the modification time of path from a local date (a Julian day) and the seconds since midnight */
int x_fsTimeLocalSet( const char *path, long julian, int seconds )
{
   struct tm local = { 0 };
   int year, month, day;
   time_t time;

   x_dateDecode( julian, &year, &month, &day );
   local.tm_year = year - 1900;
   local.tm_mon = month - 1;
   local.tm_mday = day;
   local.tm_hour = seconds / 3600;
   local.tm_min = seconds / 60 % 60;
   local.tm_sec = seconds % 60;
   local.tm_isdst = -1;
   time = mktime( &local );
   if( time == ( time_t )-1 )
      return 0;

   return x_fsTimeSet( path, ( int64_t )time * 1000 );
}

/* Delete the files that match mask, also the hidden and system ones when attr asks for them; the number deleted */
int64_t x_fsDeleteMask( const char *mask, int attr )
{
   X_ITEM list __attribute__( ( cleanup( x_itemClear ) ) ) = X_ITEM_NIL_INIT;
   const char *name = mask + strlen( mask );
   size_t dirLength;
   int64_t deleted = 0;
   size_t i;
   char attrs[3] = "";

   if( attr & FA_HIDDEN )
      strcat( attrs, "H" );

   if( attr & FA_SYSTEM )
      strcat( attrs, "S" );

   while( name > mask && name[-1] != '/' && name[-1] != '\\' && name[-1] != ':' )
      --name;

   dirLength = ( size_t )( name - mask );
   x_fileDirectory( &list, mask, attrs );
   for( i = 1; i <= ( size_t )x_arrayLen( &list ); ++i )
   {
      const X_ITEM *entry = x_arrayGetItemPtr( &list, i );
      const char *file = x_arrayGetCPtr( entry, 1 );
      size_t size = strlen( file );
      char *path;

      if( strchr( x_arrayGetCPtr( entry, 5 ), 'D' ) != NULL )
         continue;

      path = ( char * )x_xgrab( dirLength + size + 1 );
      memcpy( path, mask, dirLength );
      memcpy( path + dirLength, file, size + 1 );
      if( x_fsDelete( path ) )
         ++deleted;

      x_xfree( path );
   }

   return deleted;
}

/* Is name a valid file name on this system; maxName and maxExt limit the lengths when they are > 0, noExt forbids
   an extension and spaces allows spaces */
int x_fsNameValid( const char *name, size_t length, int64_t maxName, int64_t maxExt, int noExt, int spaces )
{
#if X_OS_WINDOWS
   static const char *reserved[] = {
      "CON", "PRN", "AUX", "NUL", "COM1", "COM2", "COM3", "COM4", "COM5", "COM6", "COM7",
      "COM8", "COM9", "LPT1", "LPT2", "LPT3", "LPT4", "LPT5", "LPT6", "LPT7", "LPT8", "LPT9"
   };
   const char *forbidden = "<>:\"/\\|?*";
#else
   const char *forbidden = "/";
#endif
   const char *dot = NULL;
   size_t i, stem;

   if( length == 0 || ( length == 1 && name[0] == '.' ) || ( length == 2 && name[0] == '.' && name[1] == '.' ) )
      return 0;

   for( i = 0; i < length; ++i )
   {
      unsigned char c = ( unsigned char )name[i];

      if( c < 32 || strchr( forbidden, c ) != NULL || ( c == ' ' && !spaces ) )
         return 0;

      if( c == '.' )
         dot = name + i;
   }
#if X_OS_WINDOWS
   /* Windows drops a trailing space or dot and keeps device names such as CON */
   if( name[length - 1] == ' ' || name[length - 1] == '.' )
      return 0;

   stem = IIF( dot != NULL, ( size_t )( dot - name ), length );
   for( i = 0; i < sizeof( reserved ) / sizeof( reserved[0] ); ++i )
   {
      if( strlen( reserved[i] ) == stem && _strnicmp( name, reserved[i], stem ) == 0 )
         return 0;
   }
#endif
   stem = IIF( dot != NULL, ( size_t )( dot - name ), length );
   if( dot != NULL && noExt )
      return 0;

   if( maxName > 0 && stem > ( size_t )maxName )
      return 0;

   if( maxExt > 0 && dot != NULL && length - stem - 1 > ( size_t )maxExt )
      return 0;

   return 1;
}

/* The absolute form of path with . and .. resolved; the path does not have to exist; allocated by x_xgrab() */
char *x_fsTrueName( const char *path )
{
#if X_OS_WINDOWS
   wchar_t *name = s_fileName( path );
   DWORD size = GetFullPathNameW( name, 0, NULL, NULL );
   wchar_t *full = NULL;
   char *result = NULL;

   if( size > 0 )
   {
      full = ( wchar_t * )x_xgrabArray( size, sizeof( wchar_t ) );
      if( GetFullPathNameW( name, size, full, NULL ) > 0 )
         result = s_fileText( full );

      x_xfree( full );
   }

   x_xfree( name );

   return IIF( result != NULL, result, ( char * )x_xmemdup( "", 1 ) );
#else
   char *cwd = IIF( path[0] == '/', NULL, getcwd( NULL, 0 ) );
   size_t cwdLength = IIF( cwd != NULL, strlen( cwd ), 0 );
   size_t length = cwdLength + 1 + strlen( path );
   char *joined = ( char * )x_xgrab( length + 1 );
   char *result = ( char * )x_xgrab( length + 2 );
   size_t used = 0;
   char *part, *rest;

   if( cwd != NULL )
      snprintf( joined, length + 1, "%s/%s", cwd, path );
   else
      snprintf( joined, length + 1, "%s", path );

   free( cwd );
   for( part = strtok_r( joined, "/", &rest ); part != NULL; part = strtok_r( NULL, "/", &rest ) )
   {
      if( strcmp( part, "." ) == 0 || part[0] == '\0' )
         continue;

      if( strcmp( part, ".." ) == 0 )
      {
         while( used > 0 && result[used - 1] != '/' )
            --used;

         if( used > 0 )
            --used;
         continue;
      }

      result[used++] = '/';
      memcpy( result + used, part, strlen( part ) );
      used += strlen( part );
   }

   if( used == 0 )
      result[used++] = '/';

   result[used] = '\0';
   x_xfree( joined );

   return result;
#endif
}

/* The full path of the running program; allocated by x_xgrab() */
char *x_fsExeName( void )
{
#if X_OS_WINDOWS
   wchar_t name[32768];
   DWORD length = GetModuleFileNameW( NULL, name, sizeof( name ) / sizeof( name[0] ) );
   char *result;

   if( length == 0 || length >= sizeof( name ) / sizeof( name[0] ) )
      return ( char * )x_xmemdup( "", 1 );

   result = s_fileText( name );

   return IIF( result != NULL, result, ( char * )x_xmemdup( "", 1 ) );
#else
   char name[PATH_MAX];
   ssize_t length = readlink( "/proc/self/exe", name, sizeof( name ) - 1 );

   if( length < 0 )
      length = 0;

   name[length] = '\0';

   return ( char * )x_xmemdup( name, ( size_t )length + 1 );
#endif
}

/* The directory for DiskFree() and DiskTotal(): "C:\\" for a drive letter, with or without the colon, the current
   directory for "", any other text as it is */
const char *x_fsDiskPath( const char *text, char drive[4] )
{
   size_t length = strlen( text );

   if( length == 0 )
      return ".";

   if( isalpha( ( unsigned char )text[0] ) && ( length == 1 || ( length == 2 && text[1] == ':' ) ) )
   {
#if X_OS_WINDOWS
      drive[0] = text[0];
      drive[1] = ':';
      drive[2] = '\\';
      drive[3] = '\0';
      return drive;
#else
      UNUSED( drive );
      return "/";
#endif
   }

   return text;
}

/* The full current directory, on drive (1 = A:) under Windows, 0 for the current drive; "" when the drive is
   not available; allocated by x_xgrab() */
char *x_fsCurDirFull( int drive )
{
#if X_OS_WINDOWS
   wchar_t *wide = _wgetdcwd( drive, NULL, 0 );
   char *result;

   if( wide == NULL )
      return ( char * )x_xmemdup( "", 1 );

   result = s_fileText( wide );
   free( wide );

   return IIF( result != NULL, result, ( char * )x_xmemdup( "", 1 ) );
#else
   char *cwd = getcwd( NULL, 0 );
   char *result;

   UNUSED( drive );
   if( cwd == NULL )
      return ( char * )x_xmemdup( "", 1 );

   result = ( char * )x_xmemdup( cwd, strlen( cwd ) + 1 );
   free( cwd );

   return result;
#endif
}

/* ADir() */
void x_fsADirCall( X_CALL_CTX *ctx )
{
   if( !X_ISCHAR( ctx, 1 ) )
   {
      x_retnint( ctx, 0 );
      return;
   }

   X_ITEM files __attribute__( ( cleanup( x_itemClear ) ) ) = X_ITEM_NIL_INIT;
   char *spec = x_fileDefName( x_parc( ctx, 1 ) );

   x_fileDirectory( &files, spec, IIF( X_ISARRAY( ctx, 6 ), "HSD", "" ) );
   x_xfree( spec );

   int64_t count = x_arrayLen( &files );

   for( int64_t i = 1; i <= count; ++i )
   {
      const X_ITEM *row = x_arrayGetItemPtr( &files, ( size_t )i );

      for( int col = 1; col <= 5; ++col )
      {
         X_ITEM *array = ( X_ITEM * )x_param( ctx, col + 1, X_IT_ARRAY );

         if( array != NULL && i <= x_arrayLen( array ) )
            x_arraySet( array, ( size_t )i, x_arrayGetItemPtr( row, ( size_t )col ) );
      }
   }

   x_retnint( ctx, count );
}

/* Base64DecodeFile() */
void x_fsBase64DecodeFileCall( X_CALL_CTX *ctx )
{
   size_t length = 0;
   char *text = x_fileLoad( x_parc( ctx, 1 ), SIZE_MAX - 1, &length );

   if( text == NULL )
      return;

   char *result = x_base64Decode( text, length, &length );

   x_fileSave( x_parc( ctx, 2 ), result, length );
   x_xfree( result );
   x_xfree( text );
}

/* Base64EncodeFile() */
void x_fsBase64EncodeFileCall( X_CALL_CTX *ctx )
{
   size_t length = 0;
   char *data = x_fileLoad( x_parc( ctx, 1 ), SIZE_MAX - 1, &length );

   if( data == NULL )
      return;

   char *result = x_base64Encode( data, length, &length );

   x_fileSave( x_parc( ctx, 2 ), result, length );
   x_xfree( result );
   x_xfree( data );
}

/* CurDir() */
void x_fsCurDirCall( X_CALL_CTX *ctx )
{
   const char *spec = x_parcx( ctx, 1 );

   int drive = 0;

   if( spec[0] >= 'A' && spec[0] <= 'Z' )
      drive = spec[0] - 'A' + 1;
   else if( spec[0] >= 'a' && spec[0] <= 'z' )
      drive = spec[0] - 'a' + 1;

   char *path = x_fsCurDir( drive );

   if( path != NULL )
      x_retc_buffer( ctx, path );
   else
      x_retc( ctx, "" );
}

/* CurDirX() */
void x_fsCurDirXCall( X_CALL_CTX *ctx )
{
   const unsigned char letter = ( unsigned char )x_parcx( ctx, 1 )[0];
   char *path = x_fsCurDirX( IIF( isalpha( letter ), toupper( letter ) - 'A' + 1, 0 ) );

   if( path != NULL )
      x_retc_buffer( ctx, path );
   else
      x_retc( ctx, "" );
}

/* CurDrive() */
void x_fsCurDriveCall( X_CALL_CTX *ctx )
{
   int drive = x_fsCurDrv();
   char name = ( char )( 'A' + drive - 1 );

   if( X_ISCHAR( ctx, 1 ) && x_parclen( ctx, 1 ) > 0 )
      x_fsChDrv( toupper( ( unsigned char )x_parc( ctx, 1 )[0] ) - 'A' + 1 );

   x_retclen( ctx, &name, IIF( drive > 0, 1, 0 ) );
}

/* Cwd() */
void x_fsCwdCall( X_CALL_CTX *ctx )
{
   char *path = x_fsCurDirFull( 0 );
   size_t length = strlen( path );

   if( length == 0 || path[length - 1] != X_OS_PATH_SEPARATOR )
   {
      path = ( char * )x_xrealloc( path, length + 2 );
      path[length++] = X_OS_PATH_SEPARATOR;
      path[length] = '\0';
   }

   x_retclen_buffer( ctx, path, length );
}

/* DefPath(): the SET DEFAULT directory with a separator at the end; "" without SET DEFAULT */
void x_fsDefPathCall( X_CALL_CTX *ctx )
{
   const char *path = x_setDefault( NULL );
   size_t length = strlen( path );
   char *result = ( char * )x_xgrab( length + 2 );

   memcpy( result, path, length );
   if( length > 0 && strchr( X_OS_PATH_DELIMITERS, path[length - 1] ) == NULL )
   {
      if( X_OS_WINDOWS && length == 1 )
         result[length++] = ':';
      else
         result[length++] = X_OS_PATH_SEPARATOR;
   }

   result[length] = '\0';
   x_retclen_buffer( ctx, result, length );
}

/* DeleteFile() */
void x_fsDeleteFileCall( X_CALL_CTX *ctx )
{
   /* 0, or -2 file not found, -3 path not found, -5 access denied */
   if( x_fsDelete( x_parc( ctx, 1 ) ) )
      x_retni( ctx, 0 );
   else
      x_retni( ctx, x_fsErrorCode() );
}

/* DirExists() */
void x_fsDirExistsCall( X_CALL_CTX *ctx )
{
   int isFile, isDir;

   x_fsExists( x_parc( ctx, 1 ), &isFile, &isDir );
   x_retl( ctx, isDir );
}

/* DirName() */
void x_fsDirNameCall( X_CALL_CTX *ctx )
{
   /* cDrive is a letter, with or without the colon; Linux has no drives and gives the current directory */
   int drive = 0;
   char *path;

   if( X_ISCHAR( ctx, 1 ) && x_parclen( ctx, 1 ) > 0 )
      drive = toupper( ( unsigned char )x_parc( ctx, 1 )[0] ) - 'A' + 1;

   if( drive < 0 || drive > 26 )
   {
      x_retc( ctx, "" );
      return;
   }

   path = x_fsCurDirFull( drive );
   x_retclen_buffer( ctx, path, strlen( path ) );
}

/* DiskChange() */
void x_fsDiskChangeCall( X_CALL_CTX *ctx )
{
   const unsigned char drive = ( unsigned char )toupper( ( unsigned char )x_parc( ctx, 1 )[0] );

   x_retl( ctx, x_fsChDrv( drive - 'A' + 1 ) );
}

/* DiskFree() */
void x_fsDiskFreeCall( X_CALL_CTX *ctx )
{
   /* cDrive is a drive letter or any directory on the disk, the current one by default; -1 on error */
   char drive[4];

   x_retnint( ctx, x_fsDirSpace( x_fsDiskPath( IIF( X_ISCHAR( ctx, 1 ), x_parc( ctx, 1 ), "" ), drive ), DISK_AVAIL ) );
}

/* DiskName() */
void x_fsDiskNameCall( X_CALL_CTX *ctx )
{
   int drive = x_fsCurDrv();
   char name = ( char )( 'A' + drive - 1 );

   x_retclen( ctx, &name, IIF( drive > 0, 1, 0 ) );
}

/* DiskSpace() */
void x_fsDiskSpaceCall( X_CALL_CTX *ctx )
{
   int64_t drive = x_parnintdef( ctx, 1, 0 );
   int64_t space = IIF( drive >= 0 && drive <= 26, x_fsDiskSpace( ( int )drive ), -1 );

   if( space < 0 )
   {
      x_errorRaiseNamed( ctx->result, 2018, "DISKSPACE" );
      return;
   }

   x_retnint( ctx, space );
}

/* DiskTotal() */
void x_fsDiskTotalCall( X_CALL_CTX *ctx )
{
   /* cDrive is a drive letter or any directory on the disk, the current one by default; -1 on error */
   char drive[4];

   x_retnint( ctx, x_fsDirSpace( x_fsDiskPath( IIF( X_ISCHAR( ctx, 1 ), x_parc( ctx, 1 ), "" ), drive ), DISK_TOTAL ) );
}

/* DiskUsed() */
void x_fsDiskUsedCall( X_CALL_CTX *ctx )
{
   /* cDrive is a drive letter or any directory on the disk, the current one by default; -1 on error */
   char drive[4];

   x_retnint( ctx, x_fsDirSpace( x_fsDiskPath( IIF( X_ISCHAR( ctx, 1 ), x_parc( ctx, 1 ), "" ), drive ), DISK_USED ) );
}

/* ExeName() */
void x_fsExeNameCall( X_CALL_CTX *ctx )
{
   char *path = x_fsExeName();

   x_retclen_buffer( ctx, path, strlen( path ) );
}

/* FileAppend() */
void x_fileAppendCall( X_CALL_CTX *ctx )
{
   size_t length;
   char *data = x_fsReadPart( x_parc( ctx, 1 ), 0, -1, &length );
   int64_t written;

   /* The target file is created when it does not exist yet */
   if( data == NULL )
   {
      x_retni( ctx, 0 );
      return;
   }

   written = x_fsWritePart( x_parc( ctx, 2 ), data, length, 1, -1, 0 );
   x_xfree( data );
   x_retnint( ctx, IIF( written < 0, 0, written ) );
}

/* FileAttr() */
void x_fileAttrCall( X_CALL_CTX *ctx )
{
   uint32_t attr;

   /* 0 when the file does not exist */
   x_retnint( ctx, IIF( x_fsAttrGet( x_parc( ctx, 1 ), &attr ), ( int64_t )attr, 0 ) );
}

/* FileAttrGet() */
void x_fileAttrGetCall( X_CALL_CTX *ctx )
{
   uint32_t attr;
   int result = x_fsAttrGet( x_parc( ctx, 1 ), &attr );

   if( result )
      x_stornint( ctx, attr, 2 );

   x_retl( ctx, result );
}

/* FileConfig() */
void x_fileConfigCall( X_CALL_CTX *ctx )
{
   int64_t handle = x_fileHandle( x_paritem( ctx, 1 ) );

   if( handle == F_ERROR )
   {
      x_ret( ctx );
      return;
   }

   int query = x_pcount( ctx ) < 3 || X_ISNIL( ctx, 3 );

   switch( x_parni( ctx, 2 ) )
   {
   case VF_IONAME:
      if( query )
      {
         x_retc( ctx, "FILE:" );
         return;
      }
      break;
   case VF_RDHANDLE:
   case VF_WRHANDLE:
      if( query )
      {
         x_retnint( ctx, handle );
         return;
      }
      break;
   case VF_TIMEOUT:
      if( query || ( X_ISNUM( ctx, 3 ) && x_parnint( ctx, 3 ) == -1 ) )
      {
         x_retnint( ctx, -1 );
         return;
      }
      break;
   }

   x_fsSetError( 50 );
   x_ret( ctx );
}

/* FileCopyEx() */
void x_fileCopyExCall( X_CALL_CTX *ctx )
{
   X_ITEM source __attribute__( ( cleanup( x_itemClear ) ) ) = X_ITEM_NIL_INIT;
   X_ITEM target __attribute__( ( cleanup( x_itemClear ) ) ) = X_ITEM_NIL_INIT;

   x_itemCopy( &source, x_paritem( ctx, 1 ) );
   x_itemCopy( &target, x_paritem( ctx, 2 ) );
   size_t size = ( size_t )x_parnintdef( ctx, 3, 65536 );
   const X_ITEM *block = x_param( ctx, 5, X_IT_BLOCK );
   int result = x_fileCopyEx( ctx, source.u.string.data, target.u.string.data, size, x_parldef( ctx, 4, 1 ), block );

   x_retni( ctx, IIF( result, 0, -1 ) );
}

/* FileDate() */
void x_fileDateCall( X_CALL_CTX *ctx )
{
   long julian;
   int seconds;

   /* An empty date when the file does not exist */
   x_retdl( ctx, IIF( x_fsTimeLocalGet( x_parc( ctx, 1 ), &julian, &seconds ), julian, 0 ) );
}

/* FileExists() */
void x_fileExistsCall( X_CALL_CTX *ctx )
{
   const char *path = x_parc( ctx, 1 );
   int isFile, isDir;

   x_fsExists( path, &isFile, &isDir );
   if( X_ISBYREF( ctx, 2 ) )
      x_storc( ctx, IIF( isFile, path, "" ), 2 );

   x_retl( ctx, isFile );
}

/* FileIsLocal() */
void x_fileIsLocalCall( X_CALL_CTX *ctx )
{
   x_fsSetError( 0 );
   x_retl( ctx, 1 );
}

/* FileLinkRead() */
void x_fileLinkReadCall( X_CALL_CTX *ctx )
{
   char *target = x_fsLinkRead( x_parc( ctx, 1 ) );

   if( target != NULL )
      x_retc_buffer( ctx, target );
   else
      x_retc_null( ctx );
}

/* FileLoad() */
void x_fileLoadCall( X_CALL_CTX *ctx )
{
   size_t limit = IIF( X_ISNUM( ctx, 2 ), ( size_t )x_parnint( ctx, 2 ), SIZE_MAX - 1 );

   size_t length;
   char *buffer = x_fileLoad( x_parc( ctx, 1 ), limit, &length );

   if( buffer != NULL )
      x_retclen_buffer( ctx, buffer, length );
   else
      x_ret( ctx );
}

/* FileNameExists() */
void x_fileNameExistsCall( X_CALL_CTX *ctx )
{
   int isFile, isDir;

   x_retl( ctx, x_fsExists( x_parc( ctx, 1 ), &isFile, &isDir ) );
}

/* FileOpen() */
void x_fileOpenCall( X_CALL_CTX *ctx )
{
   void *file = x_fileOpen( x_parc( ctx, 1 ), x_parnidef( ctx, 2, FO_READ | FO_COMPAT ) );

   if( file != NULL )
      x_retptrGC( ctx, file );
   else
      x_ret( ctx );
}

/* FileRead() */
void x_fileReadCall( X_CALL_CTX *ctx )
{
   size_t length = x_parclen( ctx, 2 );
   int64_t request = x_parnintdef( ctx, 3, ( int64_t )length );
   size_t count = ( size_t )IIF( request > 0, request, 0 );

   if( count > length )
      count = length;

   char *buffer = ( char * )x_xgrab( length + 1 );

   memcpy( buffer, x_parc( ctx, 2 ), length );
   buffer[length] = '\0';
   int64_t bytes = x_fileRead( x_paritem( ctx, 1 ), buffer, count, x_parnintdef( ctx, 4, -1 ) );

   if( bytes > 0 )
      x_storclen_buffer( ctx, buffer, length, 2 );
   else
      x_xfree( buffer );

   x_retnint( ctx, bytes );
}

/* FileReadAt() */
void x_fileReadAtCall( X_CALL_CTX *ctx )
{
   size_t length = x_parclen( ctx, 2 );
   int64_t request = x_parnint( ctx, 3 );
   size_t count = ( size_t )IIF( request > 0, request, 0 );

   if( count > length )
      count = length;

   char *buffer = ( char * )x_xgrab( length + 1 );

   memcpy( buffer, x_parc( ctx, 2 ), length );

   buffer[length] = '\0';
   int64_t bytes = x_fileReadAt( x_paritem( ctx, 1 ), buffer, count, x_parnint( ctx, 4 ) );

   if( bytes > 0 )
      x_storclen_buffer( ctx, buffer, length, 2 );
   else
      x_xfree( buffer );

   x_retnint( ctx, bytes );
}

/* FileReadLen() */
void x_fileReadLenCall( X_CALL_CTX *ctx )
{
   int64_t request = x_parnint( ctx, 2 );
   size_t count = ( size_t )IIF( request > 0, request, 0 );
   char *buffer = ( char * )x_xgrab( count + 1 );
   int64_t bytes = x_fileRead( x_paritem( ctx, 1 ), buffer, count, x_parnintdef( ctx, 3, -1 ) );
   size_t length = ( size_t )IIF( bytes > 0, bytes, 0 );

   buffer[length] = '\0';
   x_retclen_buffer( ctx, buffer, length );
}

/* FileStr() */
void x_fileStrCall( X_CALL_CTX *ctx )
{
   /* A logical second argument is already lCtrl_Z: FileStr( cFileName, .T. ) */
   int ctrlZ = IIF( X_ISLOG( ctx, 2 ), x_parl( ctx, 2 ), x_parl( ctx, 4 ) );
   int64_t count = IIF( X_ISNUM( ctx, 2 ), x_parnint( ctx, 2 ), -1 );
   size_t length;
   char *text = x_fsReadPart( x_parc( ctx, 1 ), x_parnintdef( ctx, 3, 0 ), count, &length );
   char *eof;

   /* A file that cannot be read gives "" */
   if( text == NULL )
   {
      x_retc( ctx, "" );
      return;
   }

   if( ctrlZ && ( eof = memchr( text, 26, length ) ) != NULL )
   {
      length = ( size_t )( eof - text );
      *eof = '\0';
   }

   x_retclen_buffer( ctx, text, length );
}

/* FileTemp() */
void x_fileTempCall( X_CALL_CTX *ctx )
{
   const char *dir = IIF( X_ISCHAR( ctx, 2 ), x_parc( ctx, 2 ), "" );
   const char *prefix = IIF( X_ISCHAR( ctx, 3 ), x_parc( ctx, 3 ), "xdev-" );
   const char *extension = IIF( X_ISCHAR( ctx, 4 ), x_parc( ctx, 4 ), "" );
   char *name;
   void *file = x_fileTemp( &name, dir, prefix, extension, x_parnidef( ctx, 5, FC_NORMAL ) );

   if( file != NULL )
      x_retptrGC( ctx, file );
   else
      x_ret( ctx );

   x_storc( ctx, IIF( name != NULL, name, "" ), 1 );
   x_xfree( name );
}

/* FileTime() */
void x_fileTimeCall( X_CALL_CTX *ctx )
{
   long julian;
   int seconds;
   char text[9];

   /* "" when the file does not exist */
   if( !x_fsTimeLocalGet( x_parc( ctx, 1 ), &julian, &seconds ) )
   {
      x_retc( ctx, "" );
      return;
   }

   snprintf( text, sizeof( text ), "%02d:%02d:%02d", seconds / 3600, seconds / 60 % 60, seconds % 60 );
   x_retc( ctx, text );
}

/* FileTimeGet() */
void x_fileTimeGetCall( X_CALL_CTX *ctx )
{
   int64_t timeMs;
   int result = x_fsTimeGet( x_parc( ctx, 1 ), &timeMs );

   if( result )
      x_stornint( ctx, timeMs, 2 );

   x_retl( ctx, result );
}

/* FileWrite() */
void x_fileWriteCall( X_CALL_CTX *ctx )
{
   size_t length = x_parclen( ctx, 2 );
   int64_t request = x_parnintdef( ctx, 3, ( int64_t )length );
   size_t count = ( size_t )IIF( request > 0, request, 0 );

   if( count > length )
      count = length;

   x_retnint( ctx, x_fileWrite( x_paritem( ctx, 1 ), x_parc( ctx, 2 ), count, x_parnintdef( ctx, 4, -1 ) ) );
}

/* FileWriteAt() */
void x_fileWriteAtCall( X_CALL_CTX *ctx )
{
   size_t length = x_parclen( ctx, 2 );
   int64_t request = x_parnint( ctx, 3 );
   size_t count = ( size_t )IIF( request > 0, request, 0 );

   if( count > length )
      count = length;

   x_retnint( ctx, x_fileWriteAt( x_paritem( ctx, 1 ), x_parc( ctx, 2 ), count, x_parnint( ctx, 4 ) ) );
}

/* FNameSplit() */
void x_fsFNameSplitCall( X_CALL_CTX *ctx )
{
   const char *text = x_parc( ctx, 1 );
   size_t length = x_parclen( ctx, 1 ), path, name;

   x_fsNameSplit( text, length, &path, &name );
   x_storclen( ctx, text, path, 2 );
   x_storclen( ctx, text + path, name, 3 );
   x_storclen( ctx, text + path + name, length - path - name, 4 );
   x_ret( ctx );
}

/* FRead() */
void x_fsFReadCall( X_CALL_CTX *ctx )
{
   x_fsSetError( 0 );
   if( !X_ISBYREF( ctx, 2 ) )
   {
      x_retns( ctx, 0 );
      return;
   }

   size_t length = x_parclen( ctx, 2 );
   int64_t request = x_parnint( ctx, 3 );
   size_t count = ( size_t )IIF( request > 0, request, 0 );

   if( count > length )
      count = length;

   char *buffer = ( char * )x_xgrab( length + 1 );

   memcpy( buffer, x_parc( ctx, 2 ), length );
   buffer[length] = '\0';
   size_t bytes = x_fsRead( x_parnint( ctx, 1 ), buffer, count );

   x_storclen_buffer( ctx, buffer, length, 2 );
   x_retns( ctx, ( ptrdiff_t )bytes );
}

/* FReadStr() */
void x_fsFReadStrCall( X_CALL_CTX *ctx )
{
   int64_t request = x_parnint( ctx, 2 );
   size_t count = ( size_t )IIF( request > 0, request, 0 );
   char *buffer = ( char * )x_xgrab( count + 1 );
   size_t bytes = x_fsRead( x_parnint( ctx, 1 ), buffer, count );

   buffer[bytes] = '\0';
   x_retc_buffer( ctx, buffer );
}

/* FWrite() */
void x_fsFWriteCall( X_CALL_CTX *ctx )
{
   size_t length = x_parclen( ctx, 2 );
   int64_t request = x_parnintdef( ctx, 3, ( int64_t )length );
   size_t count = ( size_t )IIF( request > 0, request, 0 );

   if( count > length )
      count = length;

   x_retns( ctx, ( ptrdiff_t )x_fsWrite( x_parnint( ctx, 1 ), x_parc( ctx, 2 ), count ) );
}

/* MD5File() */
void x_fsMD5FileCall( X_CALL_CTX *ctx )
{
   int64_t handle = x_fsOpen( x_parc( ctx, 1 ), FO_READ | FO_SHARED );
   X_MD5 md5;
   char hex[33];
   char *buffer;
   size_t count;

   /* A file that cannot be opened gives "" */
   if( handle < 0 )
   {
      x_retc( ctx, "" );
      return;
   }

   buffer = ( char * )x_xgrab( 65536 );
   x_md5Init( &md5 );
   while( ( count = x_fsRead( handle, buffer, 65536 ) ) > 0 )
      x_md5Update( &md5, buffer, count );

   x_fsClose( handle );
   x_xfree( buffer );
   x_md5Final( &md5, hex );
   x_retclen( ctx, hex, 32 );
}

/* IsDirectory() and IsDir(): .T. when the first entry of a name or a mask is a directory */
void x_fsIsDirectoryCall( X_CALL_CTX *ctx )
{
   X_ITEM files __attribute__( ( cleanup( x_itemClear ) ) ) = X_ITEM_NIL_INIT;
   const X_ITEM *attr;

   x_fileDirectory( &files, x_parc( ctx, 1 ), "D" );
   if( x_arrayLen( &files ) == 0 )
   {
      x_retl( ctx, 0 );
      return;
   }

   attr = x_arrayGetItemPtr( x_arrayGetItemPtr( &files, 1 ), F_ATTR );
   x_retl( ctx, X_IS_STRING( attr ) && memchr( attr->u.string.data, 'D', attr->u.string.len ) != NULL );
}

/* IsDisk(): .T. when a drive letter is a drive of the system; Linux has no drives */
void x_fsIsDiskCall( X_CALL_CTX *ctx )
{
#if X_OS_WINDOWS
   int letter = toupper( ( unsigned char )*x_parc( ctx, 1 ) );

   x_retl( ctx, letter >= 'A' && letter <= 'Z' && ( ( GetLogicalDrives() >> ( letter - 'A' ) ) & 1 ) );
#else
   x_retl( ctx, 0 );
#endif
}

/* MemoRead() */
void x_fsMemoReadCall( X_CALL_CTX *ctx )
{
   size_t length;
   char *buffer = x_fileLoad( x_parc( ctx, 1 ), SIZE_MAX - 1, &length );

   if( buffer == NULL )
   {
      x_retc_null( ctx );
      return;
   }

   if( !x_parl( ctx, 2 ) && length > 0 && buffer[length - 1] == '\x1A' )
      buffer[--length] = '\0';

   x_retclen_buffer( ctx, buffer, length );
}

/* MemoWrit() */
void x_fsMemoWritCall( X_CALL_CTX *ctx )
{
   size_t length = x_parclen( ctx, 2 );

   if( !x_parldef( ctx, 3, 1 ) )
   {
      x_retl( ctx, x_fileSave( x_parc( ctx, 1 ), x_parc( ctx, 2 ), length ) );
      return;
   }

   char *buffer = ( char * )x_xgrab( length + 1 );

   memcpy( buffer, x_parc( ctx, 2 ), length );
   buffer[length] = '\x1A';

   int saved = x_fileSave( x_parc( ctx, 1 ), buffer, length + 1 );

   x_xfree( buffer );
   x_retl( ctx, saved );
}

/* RenameFile() */
void x_fsRenameFileCall( X_CALL_CTX *ctx )
{
   /* 0, or -2 file not found, -3 path not found, -5 access denied, -17 not the same device */
   if( x_fsRename( x_parc( ctx, 1 ), x_parc( ctx, 2 ) ) )
      x_retni( ctx, 0 );
   else
      x_retni( ctx, x_fsErrorCode() );
}

/* SetFAttr() */
void x_fsSetFAttrCall( X_CALL_CTX *ctx )
{
   /* 0, or -2 file not found, -3 path not found, -5 access denied */
   if( x_fsAttrSet( x_parc( ctx, 1 ), ( uint32_t )x_parnintdef( ctx, 2, FA_ARCHIVE ) ) )
      x_retni( ctx, 0 );
   else
      x_retni( ctx, x_fsErrorCode() );
}

/* SetFDaTi() */
void x_fsSetFDaTiCall( X_CALL_CTX *ctx )
{
   /* The date and the time default to now; a time as the second argument stands for cFileTime */
   time_t now = time( NULL );
   struct tm local = *localtime( &now );

   long julian = x_dateEncode( local.tm_year + 1900, local.tm_mon + 1, local.tm_mday );
   int seconds = local.tm_hour * 3600 + local.tm_min * 60 + local.tm_sec;
   const char *clock = NULL;

   /* A date, a time or both; a missing one is today or now */
   if( X_ISDATE( ctx, 2 ) )
      julian = x_pardl( ctx, 2 );

   if( X_ISCHAR( ctx, 2 ) )
      clock = x_parc( ctx, 2 );
   else if( X_ISCHAR( ctx, 3 ) )
      clock = x_parc( ctx, 3 );

   if( clock != NULL )
      seconds = ( int )x_timeToSeconds( clock, strlen( clock ) );

   x_retl( ctx, julian != 0 && x_fsTimeLocalSet( x_parc( ctx, 1 ), julian, seconds ) );
}

/* StrFile() */
void x_fsStrFileCall( X_CALL_CTX *ctx )
{
   /* Without lUseExisting the file is created anew; nOffset defaults to the end of the file */
   const char *data = x_parc( ctx, 1 );
   size_t length = x_parclen( ctx, 1 );
   int64_t offset = x_parnintdef( ctx, 4, -1 );
   int64_t written = x_fsWritePart( x_parc( ctx, 2 ), data, length, x_parl( ctx, 3 ), offset, x_parl( ctx, 5 ) );

   x_retnint( ctx, IIF( written < 0, 0, written ) );
}

/* TrueName() */
void x_fsTrueNameCall( X_CALL_CTX *ctx )
{
   char *path = x_fsTrueName( x_parc( ctx, 1 ) );

   x_retclen_buffer( ctx, path, strlen( path ) );
}

/* COPY FILE: copy a file in the SET DEFAULT directory */
void x_fsCopyCommandCall( X_CALL_CTX *ctx )
{
   char *source = x_fileDefName( x_parc( ctx, 1 ) );
   char *target = x_fileDefName( x_parc( ctx, 2 ) );
   int copied = x_fileCopy( source, target );

   x_xfree( target );
   x_xfree( source );
   x_retl( ctx, copied );
}

/* TypeFile(): show a file on the console */
void x_fsTypeCommandCall( X_CALL_CTX *ctx )
{
   X_ITEM text __attribute__( ( cleanup( x_itemClear ) ) ) = X_ITEM_NIL_INIT;
   X_ARG arg = { &text, 0 };
   X_CALL_CTX call = { &arg, 1, ctx->result, NULL, NULL };
   size_t length;
   char *name = x_fileDefName( x_parc( ctx, 1 ) );
   char *buffer = x_fileLoad( name, SIZE_MAX - 1, &length );

   x_xfree( name );
   if( buffer == NULL )
      return;

   x_itemPutStringBuffer( &text, buffer, length );
   x_consoleOut( &call, 1, 0 );
}

/* FCommit(): write the buffers of a handle to the disk */
void x_fsCommitCall( X_CALL_CTX *ctx )
{
   int64_t handle = x_parnint( ctx, 1 );
#if X_OS_WINDOWS
   int ok = FlushFileBuffers( ( HANDLE )( intptr_t )handle ) != 0;

   s_fileErr = IIF( ok, 0, ( int )GetLastError() );
#else
   s_fileResult( fsync( ( int )handle ) );
#endif

   x_ret( ctx );
}

/* FTempCreate(): a new temporary file as a handle; @cName gets its name */
void x_fsTempCreateCall( X_CALL_CTX *ctx )
{
   const char *prefix = IIF( X_ISCHAR( ctx, 2 ), x_parc( ctx, 2 ), "xdt" );
   int64_t handle = F_ERROR;
   char *name;
   X_FILE *file = ( X_FILE * )x_fileTemp( &name, x_parcx( ctx, 1 ), prefix, "", x_parnidef( ctx, 3, FC_NORMAL ) );

   /* The handle is taken from the file object */
   if( file != NULL )
   {
      handle = file->handle;
      file->handle = F_ERROR;
      x_gcFree( file );
   }

   x_storc( ctx, IIF( name != NULL, name, "" ), 4 );
   x_xfree( name );
   x_retnint( ctx, handle );
}

/* FNameMerge(): path, name and extension; a separator and a dot are added where they are missing */
void x_fsNameMergeCall( X_CALL_CTX *ctx )
{
   const char *path = x_parcx( ctx, 1 );
   const char *name = x_parcx( ctx, 2 );
   const char *extension = x_parcx( ctx, 3 );
   size_t pathLength = x_parclen( ctx, 1 );
   size_t nameLength = x_parclen( ctx, 2 );
   size_t extensionLength = x_parclen( ctx, 3 );
   int separator = pathLength != 0 && !s_pathIsSeparator( path[pathLength - 1] );
   int dot = extensionLength != 0 && extension[0] != '.';
   size_t position, size;
   char *result;

   separator = separator && ( nameLength != 0 || extensionLength != 0 );
   size = pathLength + nameLength + extensionLength + separator + dot;
   result = ( char * )x_xgrab( size + 1 );

   memcpy( result, path, pathLength );
   position = pathLength;
   if( separator )
      result[position++] = X_OS_PATH_SEPARATOR;

   memcpy( result + position, name, nameLength );
   position += nameLength;
   if( dot )
      result[position++] = '.';

   memcpy( result + position, extension, extensionLength );
   result[size] = '\0';
   x_retclen_buffer( ctx, result, size );
}

/* The length of the line ending at the end of line: CR or LF, or one of the given endings; 0 for none */
static size_t s_lineSuffix( const X_ITEM *endings, const char *line, size_t length )
{
   size_t i, count, size;
   const char *ending;

   if( endings == NULL || X_IS_NIL( endings ) )
      return IIF( length && ( line[length - 1] == '\r' || line[length - 1] == '\n' ), 1, 0 );

   count = IIF( X_IS_ARRAY( endings ), x_arrayLen( endings ), 1 );
   for( i = 1; i <= count; ++i )
   {
      if( X_IS_ARRAY( endings ) )
      {
         ending = x_arrayGetCPtr( endings, i );
         size = x_arrayGetCLen( endings, i );
      }
      else
      {
         ending = endings->u.string.data;
         size = endings->u.string.len;
      }

      if( size <= length && memcmp( line + length - size, ending, size ) == 0 )
         return size;
   }

   return 0;
}

/* The length of the longest line ending into *longest; 0 with error 1102 for an array with an empty ending or
   one that is no string */
static int s_lineLongestEnding( X_CALL_CTX *ctx, const X_ITEM *endings, size_t *longest )
{
   size_t i;

   *longest = 1;
   if( endings != NULL && X_IS_STRING( endings ) )
   {
      *longest = endings->u.string.len;
      return 1;
   }

   if( endings == NULL || !X_IS_ARRAY( endings ) )
      return 1;

   for( i = 1; i <= ( size_t )x_arrayLen( endings ); ++i )
   {
      const X_ITEM *item = x_arrayGetItemPtr( endings, i );

      if( !X_IS_STRING( item ) || item->u.string.len == 0 )
      {
         x_errorRaiseNamed( ctx->result, 1102, "FREADLINE" );
         return 0;
      }

      if( item->u.string.len > *longest )
         *longest = item->u.string.len;
   }

   return 1;
}

/* FReadLine( nHandle, @cLine, [xEndings], [nMaxLine] ): one line of up to nMaxLine bytes; the file position
   moves to the next line */
void x_fsReadLineCall( X_CALL_CTX *ctx )
{
   const X_ITEM *endings = x_paritem( ctx, 3 );
   size_t maximum = ( size_t )x_parnintdef( ctx, 4, 4096 );
   size_t length = 0;
   size_t suffix = 0;
   size_t bytes, i, longest, capacity;
   int64_t handle = x_parnint( ctx, 1 );
   int found = 0;
   int error = 0;
   char input[4096], next;
   char last = 0;
   char *line;

   if( !s_lineLongestEnding( ctx, endings, &longest ) )
      return;

   if( maximum > SIZE_MAX - longest - 1 )
   {
      x_errorRaiseNamed( ctx->result, 1102, "FREADLINE" );
      return;
   }

   capacity = maximum + longest;
   line = ( char * )x_xgrab( capacity + 1 );
   while( length < capacity && !found )
   {
      bytes = x_fsRead( handle, input, sizeof( input ) );
      if( bytes == 0 )
      {
         error = s_fileErr;
         break;
      }

      for( i = 0; i < bytes && length < capacity; ++i )
      {
         line[length++] = input[i];
         suffix = s_lineSuffix( endings, line, length );
         if( suffix && length - suffix <= maximum )
         {
            last = line[length - 1];
            length -= suffix;
            found = 1;
            ++i;
            break;
         }
      }

      /* The bytes read past the line go back */
      if( i < bytes && x_fsSeek( handle, -( int64_t )( bytes - i ), FS_RELATIVE ) < 0 )
      {
         error = s_fileErr;
         break;
      }
   }

   if( !found && length > maximum )
   {
      if( x_fsSeek( handle, -( int64_t )( length - maximum ), FS_RELATIVE ) < 0 )
         error = s_fileErr;

      length = maximum;
   }

   /* CR LF is one line ending */
   if( found && last == '\r' && ( endings == NULL || X_IS_NIL( endings ) ) )
   {
      bytes = x_fsRead( handle, &next, 1 );
      if( bytes && next != '\n' )
         x_fsSeek( handle, -1, FS_RELATIVE );

      error = s_fileErr;
   }

   line[length] = '\0';
   x_storclen_buffer( ctx, line, length, 2 );
   if( error )
      s_fileErr = error;

   x_retni( ctx, IIF( error || ( length == 0 && !found ), -1, 0 ) );
}

/* FCharCount(), FWordCount() and FLineCount(): mode 1 counts the characters that are not blank, mode 2 the
   words, mode 0 the lines */
void x_fsCountCall( X_CALL_CTX *ctx, int mode )
{
   int64_t handle = x_fsOpen( x_parc( ctx, 1 ), FO_READ | FO_SHARED );
   uint64_t count = 0;
   size_t bytes, i;
   int inWord = 0;
   int hasTail = 0;
   int previousCR = 0;
   int blank, error;
   unsigned char c;
   char buffer[16384];

   if( handle == F_ERROR )
   {
      x_retni( ctx, 0 );
      return;
   }

   while( ( bytes = x_fsRead( handle, buffer, sizeof( buffer ) ) ) != 0 )
   {
      for( i = 0; i < bytes; ++i )
      {
         c = ( unsigned char )buffer[i];
         blank = c == ' ' || c == '\t' || c == '\r' || c == '\n';
         if( mode == 1 )
            count += !blank;
         else if( mode == 2 )
         {
            if( !blank && !inWord )
               ++count;

            inWord = !blank;
         }
         else
         {
            /* CR LF ends one line */
            if( c == '\r' || c == '\n' )
            {
               if( c != '\n' || !previousCR )
                  ++count;

               hasTail = 0;
            }
            else
               hasTail = 1;

            previousCR = c == '\r';
         }
      }
   }

   error = s_fileErr;
   x_fsClose( handle );
   s_fileErr = error;
   if( mode == 0 && hasTail )
      ++count;

   x_retnint( ctx, IIF( error, 0, ( int64_t )count ) );
}

/* FParseLine( cTextLine, [cDelimiter] ): the fields of a CSV line; a quoted field may hold delimiters and "" for
   one quote; an empty array for a broken line */
void x_fsParseLineCall( X_CALL_CTX *ctx )
{
   X_ITEM value __attribute__( ( cleanup( x_itemClear ) ) ) = X_ITEM_NIL_INIT;
   const char *text = x_parc( ctx, 1 );
   size_t length = x_parclen( ctx, 1 );
   size_t position = 0;
   size_t written;
   char delimiter = IIF( X_ISCHAR( ctx, 2 ), x_parc( ctx, 2 )[0], ',' );
   char *field = ( char * )x_xgrab( length + 1 );
   int closed;

   x_arrayNew( ctx->result, 0 );
   while( position < length )
   {
      written = 0;
      if( text[position] == '"' )
      {
         ++position;
         closed = 0;
         while( position < length )
         {
            if( text[position] == '"' )
            {
               ++position;
               if( position < length && text[position] == '"' )
               {
                  field[written++] = '"';
                  ++position;
                  continue;
               }

               closed = 1;
               break;
            }

            field[written++] = text[position++];
         }

         if( !closed )
            goto invalid;

         /* Only spaces may follow the closing quote */
         while( position < length && text[position] != delimiter )
         {
            if( text[position] != ' ' && text[position] != '\t' )
               goto invalid;

            ++position;
         }
      }
      else
      {
         while( position < length && text[position] != delimiter )
         {
            if( text[position] == '"' )
               goto invalid;

            field[written++] = text[position++];
         }
      }

      x_itemPutCL( &value, field, written );
      x_arrayAdd( ctx->result, &value );
      if( position == length )
         break;

      /* A delimiter at the end adds an empty field */
      ++position;
      if( position == length )
      {
         x_itemPutC( &value, "" );
         x_arrayAdd( ctx->result, &value );
      }
   }

   x_xfree( field );

   return;

invalid:
   x_xfree( field );
   x_arrayNew( ctx->result, 0 );
}

#if X_OS_WINDOWS
/* The 100 ns ticks since 1601 of a FILETIME as milliseconds since 1970 */
static int64_t s_fileTimeMs( const FILETIME *time )
{
   uint64_t ticks = ( ( uint64_t )time->dwHighDateTime << 32 ) | time->dwLowDateTime;

   return ( int64_t )( ticks / 10000 ) - X_FILE_EPOCH;
}
#endif

/* FileStats( cFile, @cAttr, @nSize, @dCreation, @nCreation, @dChange, @nChange ): the attributes, size and
   times of a file */
void x_fsStatsCall( X_CALL_CTX *ctx )
{
   const uint32_t masks[] = { FA_READONLY, FA_HIDDEN, FA_SYSTEM, FA_DIRECTORY, FA_ARCHIVE };
   uint32_t attributes;
   int64_t size, created, modified;
   long creationDate, changeDate;
   int creationTime, changeTime;
   char flags[6];
   size_t i;
   size_t written = 0;
#if X_OS_WINDOWS
   WIN32_FILE_ATTRIBUTE_DATA data;
   wchar_t *name = s_fileName( x_parc( ctx, 1 ) );
   int ok = GetFileAttributesExW( name, GetFileExInfoStandard, &data ) != 0;

   s_fileErr = IIF( ok, 0, ( int )GetLastError() );
   x_xfree( name );
   if( !ok )
   {
      x_retl( ctx, 0 );
      return;
   }

   attributes = data.dwFileAttributes;
   size = ( int64_t )( ( ( uint64_t )data.nFileSizeHigh << 32 ) | data.nFileSizeLow );
   created = s_fileTimeMs( &data.ftCreationTime );
   modified = s_fileTimeMs( &data.ftLastWriteTime );
#else
   struct stat data;

   if( stat( x_parc( ctx, 1 ), &data ) != 0 )
   {
      s_fileResult( -1 );
      x_retl( ctx, 0 );
      return;
   }

   attributes = IIF( S_ISDIR( data.st_mode ), FA_DIRECTORY, FA_ARCHIVE );
   if( !( data.st_mode & ( S_IWUSR | S_IWGRP | S_IWOTH ) ) )
      attributes |= FA_READONLY;

   size = data.st_size;
   created = ( int64_t )data.st_ctime * 1000;
   modified = ( int64_t )data.st_mtime * 1000;
   s_fileErr = 0;
#endif

   if( !x_fsTimeLocal( created, &creationDate, &creationTime ) )
   {
      s_fileErr = 13;
      x_retl( ctx, 0 );
      return;
   }

   if( !x_fsTimeLocal( modified, &changeDate, &changeTime ) )
   {
      s_fileErr = 13;
      x_retl( ctx, 0 );
      return;
   }

   for( i = 0; i < sizeof( masks ) / sizeof( masks[0] ); ++i )
   {
      if( attributes & masks[i] )
         flags[written++] = "RHSDA"[i];
   }

   flags[written] = '\0';
   x_storc( ctx, flags, 2 );
   x_stornint( ctx, size, 3 );
   x_stordl( ctx, creationDate, 4 );
   x_storni( ctx, creationTime, 5 );
   x_stordl( ctx, changeDate, 6 );
   x_storni( ctx, changeTime, 7 );
   x_retl( ctx, 1 );
}

/* Write the UU encoding of a file with the "begin 644 name" and "end" lines */
static void s_fsUuSave( const char *target, const char *source, const char *encoded, size_t encodedLength )
{
   size_t pathLength, nameLength, headerLength;
   const char *name = source;
   char *output;

   x_fsNameSplit( name, strlen( name ), &pathLength, &nameLength );
   name += pathLength;
   if( strchr( name, '\r' ) || strchr( name, '\n' ) )
   {
      s_fileErr = 13;
      return;
   }

   headerLength = strlen( name ) + 11;
   if( encodedLength > SIZE_MAX - headerLength - 5 )
   {
      s_fileErr = 8;
      return;
   }

   output = ( char * )x_xgrab( headerLength + encodedLength + 5 );
   snprintf( output, headerLength + 1, "begin 644 %s\n", name );
   memcpy( output + headerLength, encoded, encodedLength );
   memcpy( output + headerLength + encodedLength, "end\n", 4 );
   x_fileSave( target, output, headerLength + encodedLength + 4 );
   x_xfree( output );
}

/* UUEncodeFile() and UUDecodeFile(): the first file converted into the second one */
void x_fsUuCall( X_CALL_CTX *ctx, int decode )
{
   size_t length, convertedLength;
   char *data = x_fileLoad( x_parc( ctx, 1 ), SIZE_MAX - 1, &length );
   char *converted;

   if( data == NULL )
   {
      x_ret( ctx );
      return;
   }

   if( decode )
      converted = x_uuDecode( data, length, &convertedLength );
   else
      converted = x_uuEncode( data, length, &convertedLength );

   x_xfree( data );
   if( converted == NULL )
   {
      s_fileErr = 13;
      x_ret( ctx );
      return;
   }

   if( decode )
      x_fileSave( x_parc( ctx, 2 ), converted, convertedLength );
   else
      s_fsUuSave( x_parc( ctx, 2 ), x_parc( ctx, 1 ), converted, convertedLength );

   x_xfree( converted );
   x_ret( ctx );
}

/* ---------------------------------------------------------------------------------------------
   Text areas: FileTextUse(), FileTextGoTop(), FileTextSkip(), FileTextReadLn() and their family
   --------------------------------------------------------------------------------------------- */

/* Close a text area and forget its lines */
static void s_textAreaClose( S_FILE_TEXT_AREA *area )
{
   if( area->open )
      x_fsClose( area->handle );

   x_xfree( area->lines );
   memset( area, 0, sizeof( *area ) );
}

/* Close all text areas at the end of the program; FError() stays */
void x_fileAreasExit( void )
{
   int error = s_fileErr;
   int i;

   for( i = 0; i < 10; ++i )
      s_textAreaClose( &s_textAreas[i] );

   s_textArea = 0;
   s_fileErr = error;
}

/* Remember where a line starts and how long it is */
static void s_textLineAdd( S_FILE_TEXT_AREA *area, int64_t offset, int64_t length )
{
   if( area->count == area->capacity )
   {
      area->capacity = IIF( area->capacity, area->capacity * 2, 256 );
      area->lines = ( S_FILE_TEXT_LINE * )x_xreallocArray( area->lines, area->capacity, sizeof( *area->lines ) );
   }

   area->lines[area->count].offset = offset;
   area->lines[area->count++].length = length;
}

/* Open a file as a text area and find its lines; CR, LF and CR LF end a line */
static int s_textAreaOpen( S_FILE_TEXT_AREA *area, const char *path, int mode )
{
   char buffer[16384], c;
   size_t bytes, i;
   int previousCR = 0;
   int error;
   int64_t start = 0;
   int64_t offset = 0;

   area->handle = x_fsOpen( path, mode );
   if( area->handle == F_ERROR )
      return 0;

   area->open = 1;
   while( ( bytes = x_fsRead( area->handle, buffer, sizeof( buffer ) ) ) != 0 )
   {
      for( i = 0; i < bytes; ++i, ++offset )
      {
         c = buffer[i];
         if( c == '\n' && previousCR )
            start = offset + 1;
         else if( c == '\r' || c == '\n' )
         {
            s_textLineAdd( area, start, offset - start );
            start = offset + 1;
         }

         previousCR = c == '\r';
      }
   }

   error = s_fileErr;
   if( !error && start < offset )
      s_textLineAdd( area, start, offset - start );

   area->size = offset;
   if( !error && x_fsSeek( area->handle, 0, FS_SET ) < 0 )
      error = s_fileErr;

   if( error )
   {
      s_textAreaClose( area );
      s_fileErr = error;
      return 0;
   }

   return 1;
}

/* The text of the lines first..last with the line ends between them, allocated by x_xgrab(); NULL on error */
static char *s_textAreaRead( S_FILE_TEXT_AREA *area, size_t first, size_t last, size_t *length )
{
   int64_t start = area->lines[first].offset;
   int64_t end = area->lines[last].offset + area->lines[last].length;
   size_t read = 0;
   size_t count;
   char *buffer;

   if( ( uint64_t )( end - start ) >= SIZE_MAX )
   {
      s_fileErr = 8;
      return NULL;
   }

   *length = ( size_t )( end - start );
   if( x_fsSeek( area->handle, start, FS_SET ) < 0 )
      return NULL;

   buffer = ( char * )x_xgrab( *length + 1 );
   while( read < *length )
   {
      count = x_fsRead( area->handle, buffer + read, *length - read );
      if( !count )
      {
         if( !s_fileErr )
            s_fileErr = 13;

         x_xfree( buffer );
         return NULL;
      }

      read += count;
   }

   buffer[*length] = '\0';
   s_fileErr = 0;

   return buffer;
}

/* The last line of a record that starts at line first: a quoted field can hold line ends; SIZE_MAX with
   FError() 13 for an open quote at the end of the file and on a read error */
static size_t s_textRecordEnd( S_FILE_TEXT_AREA *area, size_t first )
{
   size_t last = first;
   size_t length, i;
   int quoted = 0;
   char *line;

   for( ;; )
   {
      line = s_textAreaRead( area, last, last, &length );
      if( line == NULL )
         return SIZE_MAX;

      for( i = 0; i < length; ++i )
      {
         if( line[i] == '"' )
            quoted = !quoted;
      }

      x_xfree( line );
      if( !quoted )
         return last;

      if( ++last == area->count )
      {
         s_fileErr = 13;
         return SIZE_MAX;
      }
   }
}

/* FileTextUse() (operation 0): open a file in the selected text area; "" or NIL closes it */
static void s_textUse( X_CALL_CTX *ctx, S_FILE_TEXT_AREA *area )
{
   S_FILE_TEXT_AREA opened = { 0 };

   if( X_ISNIL( ctx, 1 ) || x_parclen( ctx, 1 ) == 0 )
   {
      s_textAreaClose( area );
      s_fileErr = 0;
      x_retnint( ctx, F_ERROR );
      return;
   }

   if( !s_textAreaOpen( &opened, x_parc( ctx, 1 ), x_parnidef( ctx, 2, FO_READ | FO_DENYNONE ) ) )
   {
      x_retnint( ctx, F_ERROR );
      return;
   }

   s_textAreaClose( area );
   *area = opened;
   s_fileErr = 0;
   x_retnint( ctx, area->handle );
}

/* FileTextSelect() (operation 1): select a text area, 0 the first free one; returns the previous one */
static void s_textSelect( X_CALL_CTX *ctx )
{
   int64_t number = x_parnintdef( ctx, 1, s_textArea + 1 );
   size_t i;

   if( number == 0 )
   {
      for( i = 0; i < 10 && s_textAreas[i].open; ++i )
         ;

      if( i == 10 )
      {
         s_fileErr = 4;
         x_retni( ctx, 0 );
         return;
      }

      number = ( int64_t )i + 1;
   }

   x_retni( ctx, s_textArea + 1 );
   s_textArea = ( int )number - 1;
   s_fileErr = 0;
}

/* FileTextGoTo() (operation 5): line number (from 1), cut to the lines of the file */
static void s_textGoTo( S_FILE_TEXT_AREA *area, int64_t number )
{
   if( number <= 1 || area->count == 0 )
      area->current = 0;
   else if( ( uint64_t )number > area->count )
      area->current = area->count - 1;
   else
      area->current = ( size_t )number - 1;
}

/* FileTextSkip() (operation 6): move by step lines, not past the start or the end */
static void s_textSkip( S_FILE_TEXT_AREA *area, int64_t step )
{
   size_t left = area->count - area->current;

   if( step >= 0 )
   {
      area->current += IIF( ( uint64_t )step > left, left, ( size_t )step );
      return;
   }

   if( step == INT64_MIN || ( uint64_t )( -step ) > area->current )
      area->current = 0;
   else
      area->current -= ( size_t )( -step );
}

/* FileTextReadLn() (operation 10) and FileTextReadAndSkip() (operation 11): the current line, or the current
   CSV record, which then becomes the next one */
static void s_textRead( X_CALL_CTX *ctx, S_FILE_TEXT_AREA *area, int operation )
{
   size_t last, length;
   char *line;

   if( !area->open || area->current >= area->count )
   {
      x_retc_null( ctx );
      return;
   }

   last = area->current;
   if( operation == 11 )
   {
      last = s_textRecordEnd( area, area->current );
      if( last == SIZE_MAX )
      {
         x_retc_null( ctx );
         return;
      }
   }

   line = s_textAreaRead( area, area->current, last, &length );
   if( line == NULL )
   {
      x_retc_null( ctx );
      return;
   }

   if( operation == 11 )
      area->current = last + 1;

   x_retclen_buffer( ctx, line, length );
}

/* The FileText*() functions: 0 Use, 1 Select, 2 Eof, 3 GoTop, 4 GoBottom, 5 GoTo, 6 Skip, 7 RecNo, 8 LastRec,
   9 Info, 10 ReadLn, 11 ReadAndSkip */
void x_fileTextCall( X_CALL_CTX *ctx, int operation )
{
   S_FILE_TEXT_AREA *area = &s_textAreas[s_textArea];
   int64_t position;

   if( operation == 0 )
   {
      s_textUse( ctx, area );
      return;
   }

   if( operation == 1 )
   {
      s_textSelect( ctx );
      return;
   }

   s_fileErr = IIF( area->open, 0, 6 );
   switch( operation )
   {
   case 2:
      x_retl( ctx, !area->open || area->current >= area->count );
      break;

   case 3:
      area->current = 0;
      x_ret( ctx );
      break;

   case 4:
      area->current = IIF( area->count, area->count - 1, 0 );
      x_ret( ctx );
      break;

   case 5:
      s_textGoTo( area, x_parnint( ctx, 1 ) );
      x_ret( ctx );
      break;

   case 6:
      s_textSkip( area, x_parnintdef( ctx, 1, 1 ) );
      x_ret( ctx );
      break;

   case 7:
      x_retnint( ctx, IIF( area->count, ( int64_t )area->current + 1, 0 ) );
      break;

   case 8:
      x_retnint( ctx, ( int64_t )area->count );
      break;

   case 9:
      position = IIF( area->current < area->count, area->lines[area->current].offset, area->size );
      x_arrayNew( ctx->result, 6 );
      x_arraySetNI( ctx->result, 1, s_textArea + 1 );
      x_arraySetNInt( ctx->result, 2, ( int64_t )area->count );
      x_arraySetNInt( ctx->result, 3, IIF( area->count, ( int64_t )area->current + 1, 0 ) );
      x_arraySetNInt( ctx->result, 4, position );
      x_arraySetNInt( ctx->result, 5, area->size );
      x_arraySetL( ctx->result, 6, area->current >= area->count );
      break;

   case 10:
   case 11:
      s_textRead( ctx, area, operation );
      break;
   }
}

/* FileHandleEof(): is the position of a handle at or past the end? */
void x_fileHandleEofCall( X_CALL_CTX *ctx )
{
   int64_t handle = x_parnint( ctx, 1 );
   int64_t position = x_fsSeek( handle, 0, FS_RELATIVE );
   int64_t size;

   if( position < 0 )
   {
      x_retl( ctx, 0 );
      return;
   }

   size = s_fileSize( handle );
   x_retl( ctx, size >= 0 && position >= size );
}

/* FileParse( cFileName, [cDelimiter] ): the CSV records of a file as an array of arrays; empty on an error */
void x_fileParseCall( X_CALL_CTX *ctx )
{
   X_ITEM row __attribute__( ( cleanup( x_itemClear ) ) ) = X_ITEM_NIL_INIT;
   X_ITEM text __attribute__( ( cleanup( x_itemClear ) ) ) = X_ITEM_NIL_INIT;
   X_ARG args[2] = { { &text, 0 }, { x_paritem( ctx, 2 ), 0 } };
   X_CALL_CTX parse = { args, IIF( X_ISNIL( ctx, 2 ), 1, 2 ), &row, NULL, NULL };
   S_FILE_TEXT_AREA source = { 0 };
   size_t first, last, length;
   int error = 0;
   char *line;

   x_arrayNew( ctx->result, 0 );
   if( !s_textAreaOpen( &source, x_parc( ctx, 1 ), FO_READ | FO_DENYNONE ) )
      return;

   for( first = 0; first < source.count; first = last + 1 )
   {
      last = s_textRecordEnd( &source, first );
      if( last == SIZE_MAX )
      {
         error = s_fileErr;
         break;
      }

      line = s_textAreaRead( &source, first, last, &length );
      if( line == NULL )
      {
         error = s_fileErr;
         break;
      }

      x_itemPutCLPtr( &text, line, length );
      x_fsParseLineCall( &parse );
      if( length && x_arrayLen( &row ) == 0 )
      {
         error = 13;
         break;
      }

      x_arrayAdd( ctx->result, &row );
   }

   s_textAreaClose( &source );
   s_fileErr = error;
   if( error )
      x_arrayNew( ctx->result, 0 );
}

/* ---------------------------------------------------------------------------------------------
   INI files: FileReadIni(), FileWriteIni() and FileSetIniComment()
   --------------------------------------------------------------------------------------------- */

/* Append bytes to a buffer; a size that does not fit stops the program like x_xgrab() */
static void s_fileBufferAdd( S_FILE_BUFFER *buffer, const char *text, size_t length )
{
   size_t needed;

   if( length > SIZE_MAX - buffer->length - 1 )
      x_xgrabArray( SIZE_MAX, 2 );

   needed = buffer->length + length + 1;
   if( needed > buffer->capacity )
   {
      buffer->capacity = IIF( needed < 256, 256, needed );
      buffer->data = ( char * )x_xrealloc( buffer->data, buffer->capacity );
   }

   memcpy( buffer->data + buffer->length, text, length );
   buffer->length += length;
   buffer->data[buffer->length] = '\0';
}

/* Move first and last inside the spaces and tabs around a part of text */
static void s_iniTrim( const char *text, size_t *first, size_t *last )
{
   while( *first < *last && ( text[*first] == ' ' || text[*first] == '\t' ) )
      ++*first;

   while( *last > *first && ( text[*last - 1] == ' ' || text[*last - 1] == '\t' ) )
      --*last;
}

/* Can the character stand in a key (section 0) or a section name (section 1)? */
static int s_iniPlainChar( char c, size_t position, int section )
{
   if( c == '\0' || c == '\r' || c == '\n' || c == '\t' || c == '"' || c == '[' || c == ']' )
      return 0;

   if( s_iniInlineComment && c == s_iniInlineComment )
      return 0;

   if( section )
      return 1;

   if( c == '=' || c == ':' || c == '|' )
      return 0;

   return !( position == 0 && s_iniLineComment && c == s_iniLineComment );
}

/* Can the string be written as a key or a section name without quotes? */
static int s_iniPlainName( const X_ITEM *name, int section )
{
   size_t i;

   if( !X_IS_STRING( name ) || name->u.string.len == 0 )
      return 0;

   if( name->u.string.data[0] == ' ' || name->u.string.data[name->u.string.len - 1] == ' ' )
      return 0;

   for( i = 0; i < name->u.string.len; ++i )
   {
      if( !s_iniPlainChar( name->u.string.data[i], i, section ) )
         return 0;
   }

   return 1;
}

/* Write key="value"; a quote in the value is doubled; 0 for a value that cannot be written */
static int s_iniWritePair( S_FILE_BUFFER *buffer, const X_ITEM *key, const X_ITEM *value )
{
   int simple = X_IS_STRING( value ) || X_IS_NUMERIC( value ) || X_IS_LOGICAL( value );
   char *text;
   size_t length, i;

   if( !s_iniPlainName( key, 0 ) )
      return 0;

   if( !simple && !X_IS_DATE( value ) && !X_IS_NIL( value ) )
      return 0;

   if( X_IS_NUMERIC( value ) )
      text = x_valToPrgExp( value, &length );
   else
      text = x_valToCStr( value, &length );

   if( memchr( text, '\0', length ) || memchr( text, '\r', length ) || memchr( text, '\n', length ) )
   {
      x_xfree( text );
      return 0;
   }

   s_fileBufferAdd( buffer, key->u.string.data, key->u.string.len );
   s_fileBufferAdd( buffer, "=\"", 2 );
   for( i = 0; i < length; ++i )
   {
      s_fileBufferAdd( buffer, text + i, 1 );
      if( text[i] == '"' )
         s_fileBufferAdd( buffer, "\"", 1 );
   }

   s_fileBufferAdd( buffer, "\"\n", 2 );
   x_xfree( text );

   return 1;
}

/* Write every line of text as a comment line; 0 when there is no comment character or the text holds a zero */
static int s_iniComment( S_FILE_BUFFER *buffer, const char *text, size_t length )
{
   size_t first = 0;
   size_t last;

   if( length && !s_iniLineComment )
      return 0;

   if( memchr( text, '\0', length ) )
      return 0;

   while( first < length )
   {
      last = first;
      while( last < length && text[last] != '\r' && text[last] != '\n' )
         ++last;

      s_fileBufferAdd( buffer, &s_iniLineComment, 1 );
      s_fileBufferAdd( buffer, " ", 1 );
      s_fileBufferAdd( buffer, text + first, last - first );
      s_fileBufferAdd( buffer, "\n", 1 );
      first = last;
      if( first < length && text[first++] == '\r' && first < length && text[first] == '\n' )
         ++first;
   }

   return 1;
}

/* FileSetIniComment( cLineComment, cInlineComment ): the comment characters; "" switches a kind of comment off */
void x_fileIniCommentsCall( X_CALL_CTX *ctx )
{
   s_iniLineComment = IIF( x_parclen( ctx, 1 ), x_parc( ctx, 1 )[0], 0 );
   s_iniInlineComment = IIF( x_parclen( ctx, 2 ), x_parc( ctx, 2 )[0], 0 );
   x_ret( ctx );
}

/* A new section hash of the INI root */
static void s_iniSection( X_ITEM *root, X_ITEM *section, X_ITEM *key, int sensitive )
{
   x_hashNew( section );
   x_hashSetFlags( section, X_HASH_CASEMATCH, sensitive );
   x_hashAdd( root, key, section );
}

/* The value after a delimiter: a quoted value with "" for one quote, or the plain text; 0 for a broken quote */
static int s_iniValue( const char *text, size_t begin, size_t last, char *unquoted, X_ITEM *value )
{
   size_t written = 0;
   size_t i;
   int closed = 0;

   if( begin >= last || text[begin] != '"' )
   {
      x_itemPutCL( value, text + begin, last - begin );
      return 1;
   }

   for( i = begin + 1; i < last; ++i )
   {
      if( text[i] == '"' )
      {
         if( i + 1 < last && text[i + 1] == '"' )
            ++i;
         else
         {
            closed = 1;
            ++i;
            break;
         }
      }

      unquoted[written++] = text[i];
   }

   while( i < last && ( text[i] == ' ' || text[i] == '\t' ) )
      ++i;

   if( !closed || i != last )
      return 0;

   x_itemPutCL( value, unquoted, written );

   return 1;
}

/* FileReadIni( cFile, [lCaseSensitive], [cDelimiters], [lAutoMain] ): a hash of sections, each a hash of keys;
   keys before the first section go to MAIN with lAutoMain; NIL for a broken file */
void x_fileReadIniCall( X_CALL_CTX *ctx )
{
   X_ITEM root __attribute__( ( cleanup( x_itemClear ) ) ) = X_ITEM_NIL_INIT;
   X_ITEM section __attribute__( ( cleanup( x_itemClear ) ) ) = X_ITEM_NIL_INIT;
   X_ITEM key __attribute__( ( cleanup( x_itemClear ) ) ) = X_ITEM_NIL_INIT;
   X_ITEM value __attribute__( ( cleanup( x_itemClear ) ) ) = X_ITEM_NIL_INIT;
   X_ITEM *existing;
   const char *delimiters = IIF( X_ISCHAR( ctx, 3 ), x_parc( ctx, 3 ), ":=|" );
   size_t delimiterLength = IIF( X_ISCHAR( ctx, 3 ), x_parclen( ctx, 3 ), 3 );
   int sensitive = x_parldef( ctx, 2, 1 );
   int autoMain = x_parldef( ctx, 4, 1 );
   size_t position = 0;
   size_t length, first, last, end, delimiter, i;
   char *text = x_fileLoad( x_parc( ctx, 1 ), SIZE_MAX - 1, &length );
   char *unquoted;
   int quoted;

   if( text == NULL )
   {
      x_ret( ctx );
      return;
   }

   if( memchr( text, '\0', length ) )
   {
      x_xfree( text );
      s_fileErr = 13;
      x_ret( ctx );
      return;
   }

   unquoted = ( char * )x_xgrab( length + 1 );
   x_hashNew( &root );
   x_hashSetFlags( &root, X_HASH_CASEMATCH, sensitive );
   if( autoMain )
   {
      x_itemPutC( &key, "MAIN" );
      s_iniSection( &root, &section, &key, sensitive );
   }
   else
      x_itemCopy( &section, &root );

   /* A UTF-8 byte order mark is skipped */
   if( length >= 3 && memcmp( text, "\xEF\xBB\xBF", 3 ) == 0 )
      position = 3;

   while( position < length )
   {
      first = position;
      last = position;
      while( last < length && text[last] != '\r' && text[last] != '\n' )
         ++last;

      position = last;
      if( position < length && text[position++] == '\r' && position < length && text[position] == '\n' )
         ++position;

      /* An empty line or a comment line */
      s_iniTrim( text, &first, &last );
      if( first == last || ( s_iniLineComment && text[first] == s_iniLineComment ) )
         continue;

      /* An inline comment outside quotes ends the line */
      quoted = 0;
      for( i = first; i < last; ++i )
      {
         if( text[i] == '"' )
            quoted = !quoted;

         if( !quoted && s_iniInlineComment && text[i] == s_iniInlineComment )
         {
            last = i;
            break;
         }
      }

      s_iniTrim( text, &first, &last );
      if( first == last )
         continue;

      /* [section] */
      if( text[first] == '[' )
      {
         if( text[last - 1] != ']' )
            goto invalid;

         ++first;
         --last;
         s_iniTrim( text, &first, &last );
         if( first == last )
            goto invalid;

         x_itemPutCL( &key, text + first, last - first );
         existing = x_hashGetItemPtr( &root, &key );
         if( existing == NULL )
            s_iniSection( &root, &section, &key, sensitive );
         else if( X_IS_HASH( existing ) )
            x_itemCopy( &section, existing );
         else
            goto invalid;

         continue;
      }

      /* Key = value; a line without a delimiter is skipped */
      delimiter = first;
      while( delimiter < last && !memchr( delimiters, text[delimiter], delimiterLength ) )
         ++delimiter;

      if( delimiter == last )
         continue;

      end = delimiter;
      s_iniTrim( text, &first, &end );
      if( first == end )
         goto invalid;

      x_itemPutCL( &key, text + first, end - first );
      first = delimiter + 1;
      s_iniTrim( text, &first, &last );
      if( !s_iniValue( text, first, last, unquoted, &value ) )
         goto invalid;

      x_hashAdd( &section, &key, &value );
   }

   x_xfree( text );
   x_xfree( unquoted );
   s_fileErr = 0;
   x_itemReturn( ctx, &root );

   return;

invalid:
   x_xfree( text );
   x_xfree( unquoted );
   s_fileErr = 13;
   x_ret( ctx );
}

/* Write a section: [name] and its keys; the MAIN section of lAutoMain has no [MAIN] line */
static int s_iniWriteSection( S_FILE_BUFFER *buffer, const X_ITEM *name, const X_ITEM *section, int mainSection )
{
   size_t j;

   if( !mainSection )
   {
      s_fileBufferAdd( buffer, "[", 1 );
      s_fileBufferAdd( buffer, name->u.string.data, name->u.string.len );
      s_fileBufferAdd( buffer, "]\n", 2 );
   }

   for( j = 1; j <= x_hashLen( section ); ++j )
   {
      if( !s_iniWritePair( buffer, x_hashKeyAt( section, j ), x_hashValueAt( section, j ) ) )
         return 0;
   }

   return 1;
}

/* FileWriteIni( cFile, hIni, [cHeader], [cFooter], [lAutoMain] ): the keys of the root and of MAIN first, then
   the other sections; error 1102 for a hash that cannot be written */
void x_fileWriteIniCall( X_CALL_CTX *ctx )
{
   const X_ITEM *root = x_paritem( ctx, 2 );
   const X_ITEM *key, *value;
   S_FILE_BUFFER buffer = { NULL, 0, 0 };
   size_t i;
   int autoMain = x_parldef( ctx, 5, 1 );
   int pass, mainSection;

   if( !s_iniComment( &buffer, x_parcx( ctx, 3 ), x_parclen( ctx, 3 ) ) )
      goto invalid;

   for( pass = 0; pass < 2; ++pass )
   {
      for( i = 1; i <= x_hashLen( root ); ++i )
      {
         key = x_hashKeyAt( root, i );
         value = x_hashValueAt( root, i );
         if( !s_iniPlainName( key, X_IS_HASH( value ) ) )
            goto invalid;

         /* Pass 0: the keys of the root and the MAIN section; pass 1: the other sections */
         if( !X_IS_HASH( value ) )
         {
            if( pass == 0 && !s_iniWritePair( &buffer, key, value ) )
               goto invalid;

            continue;
         }

         mainSection = autoMain && key->u.string.len == 4 && memcmp( key->u.string.data, "MAIN", 4 ) == 0;
         if( ( pass == 0 ) != mainSection )
            continue;

         if( !s_iniWriteSection( &buffer, key, value, mainSection ) )
            goto invalid;
      }
   }

   if( !s_iniComment( &buffer, x_parcx( ctx, 4 ), x_parclen( ctx, 4 ) ) )
      goto invalid;

   x_retl( ctx, x_fileSave( x_parc( ctx, 1 ), IIF( buffer.data, buffer.data, "" ), buffer.length ) );
   x_xfree( buffer.data );

   return;

invalid:
   x_xfree( buffer.data );
   x_errorRaiseNamed( ctx->result, 1102, "FILEWRITEINI" );
}

/* ---------------------------------------------------------------------------------------------
   File functions of the API
   --------------------------------------------------------------------------------------------- */

/* DirSepToOS(): change every / and \ of a path to the directory separator of the operating system */
void x_fsDirSepToOS( X_CALL_CTX *ctx )
{
   size_t length = x_parclen( ctx, 1 );
   char *path = ( char * )x_xgrab( length + 1 );
   size_t i;

   memcpy( path, x_parc( ctx, 1 ), length + 1 );
   for( i = 0; i < length; ++i )
   {
      if( path[i] == '/' || path[i] == '\\' )
         path[i] = ( char )IIF( X_OS_WINDOWS, '\\', '/' );
   }

   x_retclen_buffer( ctx, path, length );
}
