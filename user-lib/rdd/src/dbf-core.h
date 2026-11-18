/*
 * Copyright (c) 2024-2026 xDev-Language
 *
 * Library: xDev 7.0.0 RDD
 * File: dbf-core.h
 *
 * Types of the DBF engine and its API: schema, session, work areas and the driver ABI
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#ifndef XDEV_DBF_CORE_H
#define XDEV_DBF_CORE_H

#include "xdev.h"

#ifdef __cplusplus
extern "C"
{
#endif

   typedef struct _DBF          *DBF;
   typedef struct _X_DBF_SESSION X_DBF_SESSION;
   typedef struct _X_DBF_SCHEMA  X_DBF_SCHEMA;

   typedef enum
   {
      X_DBF_OK,
      X_DBF_INVALID_ARGUMENT,
      X_DBF_INVALID_SCHEMA,
      X_DBF_DUPLICATE_NAME,
      X_DBF_INVALID_TEXT,
      X_DBF_TEXT_OVERFLOW,
      X_DBF_AREA_NOT_FOUND,
      X_DBF_AREA_IN_USE,
      X_DBF_DRIVER_NOT_FOUND,
      X_DBF_UNSUPPORTED_DRIVER_ABI,
      X_DBF_INVALID_STATE,
      X_DBF_READ_ONLY,
      X_DBF_NO_RECORD,
      X_DBF_INVALID_RECORD,
      X_DBF_IO_ERROR,
      X_DBF_UNSUPPORTED_OPERATION,
      X_DBF_UNSUPPORTED_FORMAT,
      X_DBF_CORRUPT_FILE,
      X_DBF_OPEN_ERROR,
      X_DBF_CREATE_ERROR,
      X_DBF_READ_ERROR,
      X_DBF_WRITE_ERROR,
      X_DBF_CLOSE_ERROR,
      X_DBF_LOCK_FAILED,
      X_DBF_RECORD_UNLOCKED,
      X_DBF_SHARED_OPERATION,
      X_DBF_FIELD_OVERFLOW,
      X_DBF_INVALID_FIELD_VALUE,
      X_DBF_RECOVERY_REQUIRED,
      X_DBF_INDEX_EXPRESSION_REQUIRED,
      X_DBF_INVALID_INDEX_KEY,
      X_DBF_ORDER_NOT_FOUND
   } X_DBF_STATUS;

   typedef enum
   {
      X_DBF_CHARACTER = 'C',
      X_DBF_NUMERIC = 'N',
      X_DBF_DATE = 'D',
      X_DBF_LOGICAL = 'L',
      X_DBF_MEMO = 'M',
      X_DBF_PICTURE = 'P'
   } X_DBF_FIELD_TYPE;

   enum
   {
      X_DBF_DRIVER_ABI = 8,
      X_DBF_MAX_AREA = 65535,
      X_DBF_MAX_FIELD_NAME = 32,
      X_DBF_MAX_DRIVER_NAME = 31
   };

   typedef struct
   {
      char  *data;
      size_t length;
   } X_DBF_TEXT;

   typedef struct
   {
      const char      *name;
      X_DBF_FIELD_TYPE type;
      uint16_t         length;
      uint8_t          decimals;
   } X_DBF_FIELD_SPEC;

   typedef struct
   {
      char             name[X_DBF_MAX_FIELD_NAME + 1];
      X_DBF_FIELD_TYPE type;
      uint16_t         length;
      uint8_t          decimals;
      uint32_t         offset;
   } X_DBF_FIELD;

   typedef struct
   {
      uint64_t record;
      uint64_t recordCount;
      int      bof;
      int      eof;
      int      found;
      int      deleted;
      int      dirty;
   } X_DBF_STATE;

   typedef struct
   {
      const char   *path;
      const char   *alias;
      const char   *driver;
      uint32_t      area;
      int           readOnly;
      int           shared;
      const X_ITEM *indexBindings;
   } X_DBF_OPEN_OPTIONS;

   typedef struct
   {
      const char *path;
      const char *driver;
   } X_DBF_CREATE_OPTIONS;

   typedef struct
   {
      uint32_t    abi;
      const char *name;
      X_DBF_STATUS ( *open )( DBF area, const X_DBF_OPEN_OPTIONS *options );
      X_DBF_STATUS ( *close )( DBF area );
      void ( *dispose )( DBF area );
      X_DBF_STATUS ( *sync )( DBF area );
      X_DBF_STATUS ( *read )( DBF area, uint64_t record, char *buffer, size_t length );
      X_DBF_STATUS ( *write )( DBF area, uint64_t record, const char *buffer, size_t length );
      X_DBF_STATUS ( *append )( DBF area, const char *buffer, size_t length, uint64_t *record );
      X_DBF_STATUS ( *create )( const X_DBF_CREATE_OPTIONS *options, const X_DBF_SCHEMA *schema );
      X_DBF_STATUS ( *recordCount )( DBF area, uint64_t *count );
      X_DBF_STATUS ( *checkWrite )( DBF area, uint64_t record );
      X_DBF_STATUS ( *lock )( DBF area, uint64_t record );
      X_DBF_STATUS ( *unlock )( DBF area, uint64_t record );
      X_DBF_STATUS ( *isLocked )( DBF area, uint64_t record, int *locked );
      X_DBF_STATUS ( *pack )( DBF area );
      X_DBF_STATUS ( *zap )( DBF area );
      X_DBF_STATUS ( *info )( DBF area, int selector, X_ITEM *result );
      X_DBF_STATUS ( *memoRead )( DBF area, const X_DBF_FIELD *field, const char *record, X_DBF_TEXT *result,
                                  int *binary );
      X_DBF_STATUS ( *memoWrite )( DBF area, const X_DBF_FIELD *field, const char *data, size_t length, char *pointer );
      X_DBF_STATUS ( *memoExport )( DBF area, const X_DBF_FIELD *field, const X_ITEM *target );
      X_DBF_STATUS ( *memoImport )( DBF area, const X_DBF_FIELD *field, const X_ITEM *source, uint64_t length,
                                    char *pointer );
      X_DBF_STATUS ( *memoValueRead )( DBF area, const X_DBF_FIELD *field, X_ITEM *result );
      X_DBF_STATUS ( *memoValueWrite )( DBF area, const X_DBF_FIELD *field, const X_ITEM *value, char *pointer );
   } X_DBF_DRIVER;

   const char *x_dbfStatusName( X_DBF_STATUS status );
   void        x_dbfRaise( X_CALL_CTX *ctx, X_DBF_STATUS status, const char *operation );

   void         x_dbfTextClear( X_DBF_TEXT *text );
   size_t       x_dbfTextFit( const char *text, size_t length, size_t limit );
   X_DBF_STATUS x_dbfTextCopy( const char *text, size_t length, X_DBF_TEXT *result );
   X_DBF_STATUS x_dbfTextValidateUTF8( const char *text, size_t length );

   X_DBF_STATUS       x_dbfSchemaCreate( const X_DBF_FIELD_SPEC *fields, size_t count, X_DBF_SCHEMA **schema );
   void               x_dbfSchemaDestroy( X_DBF_SCHEMA *schema );
   size_t             x_dbfSchemaFieldCount( const X_DBF_SCHEMA *schema );
   size_t             x_dbfSchemaRecordLength( const X_DBF_SCHEMA *schema );
   const X_DBF_FIELD *x_dbfSchemaField( const X_DBF_SCHEMA *schema, size_t ordinal );
   size_t             x_dbfSchemaFindField( const X_DBF_SCHEMA *schema, const char *name );

   X_DBF_SESSION *x_dbfSessionCreate( void );
   X_DBF_SESSION *x_dbfRuntimeSession( void );
   X_DBF_STATUS   x_dbfSessionClose( X_DBF_SESSION *session );
   void           x_dbfSessionDiscard( X_DBF_SESSION *session );
   X_DBF_STATUS   x_dbfDriverRegister( X_DBF_SESSION *session, const X_DBF_DRIVER *driver );
   DBF            x_dbfSessionArea( const X_DBF_SESSION *session, uint32_t number );
   DBF            x_dbfSessionAlias( const X_DBF_SESSION *session, const char *alias );
   DBF            x_dbfSessionCurrent( const X_DBF_SESSION *session );
   uint32_t       x_dbfSessionSelected( const X_DBF_SESSION *session );
   X_DBF_STATUS   x_dbfSessionSelect( X_DBF_SESSION *session, uint32_t number );

   X_DBF_STATUS x_dbfAreaOpen( X_DBF_SESSION *session, const X_DBF_OPEN_OPTIONS *options, DBF *result );
   X_DBF_STATUS x_dbfTableCreate( X_DBF_SESSION *session, const X_DBF_CREATE_OPTIONS *options,
                                  const X_DBF_SCHEMA *schema );
   X_DBF_STATUS x_dbfAreaClose( DBF area );
   void         x_dbfAreaDiscard( DBF area );
   X_DBF_STATUS x_dbfAreaFlush( DBF area );
   X_DBF_STATUS x_dbfAreaCommit( DBF area );
   X_DBF_STATUS x_dbfAreaGoTo( DBF area, uint64_t record );
   X_DBF_STATUS x_dbfAreaGoTop( DBF area, int skipDeleted );
   X_DBF_STATUS x_dbfAreaGoBottom( DBF area, int skipDeleted );
   X_DBF_STATUS x_dbfAreaSkip( DBF area, int64_t records, int skipDeleted );
   X_DBF_STATUS x_dbfAreaPrepare( DBF area );
   X_DBF_STATUS x_dbfAreaSetFilter( DBF area, const X_ITEM *block, const X_ITEM *text );
   void         x_dbfAreaClearFilter( DBF area );
   X_DBF_STATUS x_dbfAreaFilterText( DBF area, X_ITEM *result );
   X_DBF_STATUS x_dbfAreaSetRelation( DBF parent, DBF child, const X_ITEM *block, const X_ITEM *text );
   X_DBF_STATUS x_dbfAreaClearRelations( DBF area );
   X_DBF_STATUS x_dbfAreaRelationInfo( DBF area, size_t ordinal, uint32_t *child, X_ITEM *text );
   X_DBF_STATUS x_dbfAreaAppend( DBF area );
   X_DBF_STATUS x_dbfAreaDelete( DBF area, int deleted );
   X_DBF_STATUS x_dbfAreaRecordCount( DBF area, uint64_t *count );
   X_DBF_STATUS x_dbfAreaLock( DBF area, uint64_t record );
   X_DBF_STATUS x_dbfAreaUnlock( DBF area, uint64_t record );
   X_DBF_STATUS x_dbfAreaIsLocked( DBF area, uint64_t record, int *locked );
   X_DBF_STATUS x_dbfAreaPack( DBF area );
   X_DBF_STATUS x_dbfAreaPackMemo( DBF area );
   X_DBF_STATUS x_dbfAreaZap( DBF area );
   X_DBF_STATUS x_dbfAreaInfo( DBF area, int selector, X_ITEM *result );
   X_DBF_STATUS x_dbfFieldInfo( DBF area, size_t ordinal, int selector, X_ITEM *result );
   X_DBF_STATUS x_dbfRecordInfo( DBF area, uint64_t record, int selector, X_ITEM *result );
   uint32_t     x_dbfAreaNumber( DBF area );
   const char  *x_dbfAreaAlias( DBF area );

   const X_DBF_STATE  *x_dbfAreaState( DBF area );
   const X_DBF_SCHEMA *x_dbfAreaSchema( DBF area );
   X_DBF_STATUS        x_dbfAreaDefine( DBF area, const X_DBF_FIELD_SPEC *fields, size_t count, uint64_t recordCount );
   void               *x_dbfAreaStorage( DBF area );
   void                x_dbfAreaSetStorage( DBF area, void *storage );

   X_DBF_STATUS x_dbfFieldGetText( DBF area, size_t ordinal, X_DBF_TEXT *result );
   X_DBF_STATUS x_dbfFieldPutText( DBF area, size_t ordinal, const char *text, size_t length );
   X_DBF_STATUS x_dbfFieldGetValue( DBF area, size_t ordinal, X_ITEM *result );
   X_DBF_STATUS x_dbfFieldPutValue( DBF area, size_t ordinal, const X_ITEM *value );
   X_DBF_STATUS x_dbfFieldFileGet( DBF area, size_t ordinal, const char *path, int append );
   X_DBF_STATUS x_dbfFieldFilePut( DBF area, size_t ordinal, const char *path );
   void         x_dbfFieldRead( X_CALL_CTX *ctx, X_ITEM *result, const X_ITEM *selector, const char *name );
   void         x_dbfFieldReference( X_CALL_CTX *ctx, X_ITEM *result, const X_ITEM *selector, const char *name );
   uint32_t     x_dbfAliasEnter( X_CALL_CTX *ctx, const X_ITEM *selector );
   void         x_dbfAliasLeave( uint32_t previous );

#ifdef __cplusplus
}
#endif

#endif /* XDEV_DBF_CORE_H */
