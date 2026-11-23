/*
 * Copyright (c) 2024-2026 xDev-Language
 *
 * Library...: xDev RDD
 * File......: dbf-internal.h
 *
 */

#ifndef XDEV_DBF_INTERNAL_H
#define XDEV_DBF_INTERNAL_H

#include "dbf-core.h"

typedef struct _X_DBF_DRIVER_ENTRY X_DBF_DRIVER_ENTRY;
typedef struct _X_DBF_RELATION     X_DBF_RELATION;
typedef struct _X_DBF_MEMO_FILE    X_DBF_MEMO_FILE;
typedef struct _X_DBF_MEMO_PACK    X_DBF_MEMO_PACK;
typedef struct _X_DBF_ORDER        X_DBF_ORDER;
typedef struct _X_DBF_INDEX_BAG    X_DBF_INDEX_BAG;

/* Milliseconds an internal lock (table header, index, memo) waits for another process */
enum
{
   X_DBF_LOCK_WAIT = 10000
};

typedef struct
{
   void *storage;
   int   held;
} X_DBF_FILE_GUARD;

typedef struct
{
   DBF               area;
   X_DBF_FILE_GUARD  file;
   X_DBF_INDEX_BAG **bags;
   size_t            count;
} X_DBF_INDEX_LOCK;

typedef struct
{
   X_ITEM   whileBlock;
   X_ITEM   evalBlock;
   uint64_t start;
   uint64_t next;
   uint64_t record;
   uint64_t step;
   int      scoped;
   int      rest;
   int      current;
   int      useFilter;
   int      custom;
   int      compound;
   int      temporary;
   int      exclusive;
} X_DBF_INDEX_BUILD;

typedef struct
{
   const char              *path;
   const char              *name;
   const char              *expression;
   const X_ITEM            *block;
   const char              *condition;
   const X_ITEM            *forBlock;
   int                      unique;
   int                      descending;
   int                      additive;
   int                      existing;
   const X_DBF_INDEX_BUILD *build;
} X_DBF_INDEX_OPTIONS;

struct _X_DBF_RELATION
{
   X_DBF_RELATION *next;
   DBF             parent;
   DBF             child;
   X_ITEM          block;
   X_ITEM          text;
   uint64_t        identity;
   int             scoped;
};

struct _X_DBF_SCHEMA
{
   size_t      count;
   size_t      recordLength;
   X_DBF_FIELD fields[];
};

struct _X_DBF_DRIVER_ENTRY
{
   X_DBF_DRIVER_ENTRY *next;
   X_DBF_DRIVER        operations;
   int                 lockScheme;
   int                 lockRetry;
   int                 setHeader;
   char                name[X_DBF_MAX_DRIVER_NAME + 1];
};

struct _X_DBF_SESSION
{
   X_DBF_DRIVER_ENTRY *drivers;
   DBF                 areas;
   uint32_t            current;
   uint64_t            nextIdentity;
};

struct _DBF
{
   DBF                 next;
   X_DBF_SESSION      *session;
   X_DBF_DRIVER_ENTRY *driver;
   X_DBF_SCHEMA       *schema;
   X_DBF_STATE         state;
   char               *alias;
   char               *record;
   char               *scratch; /* record read before it replaces record, so a failed read leaves record intact */
   void               *storage;
   uint32_t            number;
   uint64_t            identity;
   X_ITEM              filter;
   X_ITEM              filterText;
   X_ITEM              locateFor;
   X_DBF_RELATION     *relations;
   X_DBF_RELATION     *pendingRelation;
   int                 filterBusy;
   int                 relationBusy;
   int                 readOnly;
   int                 shared;
   int                 lockScheme;
   int                 indexLockDepth;
   int                 driverStarted;
   X_DBF_ORDER        *indexes;
   X_DBF_ORDER        *order;
   X_ITEM              indexCondition;
   X_ITEM              indexConditionText;
   int                 indexDescending;
   int                 indexAdditive;
   X_DBF_INDEX_BUILD   indexBuild;
   int                 indexBusy;
   int                 indexNewRecord;
   int                 structuralIndex;
   X_DBF_ORDER        *indexBuilding;
   uint64_t            indexKeysIncluded;
   uint64_t            indexEvalStep;
   int                 indexReindex;
};

extern const X_DBF_DRIVER x_dbfFileDriver;
X_DBF_STATUS              x_dbfFileIndexFlag( DBF area, int present );
X_DBF_STATUS              x_dbfFileLockScheme( DBF area, int scheme );
X_DBF_STATUS              x_dbfFileLockTest( DBF area, uint64_t record, int *locked );
X_DBF_STATUS              x_dbfFileSetHeader( DBF area, int mode, int *previous );
X_DBF_STATUS              x_dbfFileEnter( DBF area, X_DBF_FILE_GUARD *guard );
int                       x_dbfLockWait( const X_ITEM *file, int64_t offset, int64_t length, int type, int wait );
void                      x_dbfFileLeave( X_DBF_FILE_GUARD *guard );
X_DBF_STATUS              x_dbfMemoOpen( const char *path, int kind, int blockHeaders, int shared, int readOnly,
                                         X_DBF_MEMO_FILE **result );
X_DBF_STATUS              x_dbfMemoCreate( const char *path, int kind, int blockHeaders, X_DBF_MEMO_FILE **result );
X_DBF_STATUS              x_dbfMemoClose( X_DBF_MEMO_FILE *memo );
void                      x_dbfMemoDispose( X_DBF_MEMO_FILE *memo );
X_DBF_STATUS              x_dbfMemoSync( X_DBF_MEMO_FILE *memo );
X_DBF_STATUS              x_dbfMemoZap( X_DBF_MEMO_FILE *memo );
X_DBF_STATUS              x_dbfMemoPackBegin( X_DBF_MEMO_FILE *memo, X_DBF_MEMO_PACK **result );
X_DBF_STATUS              x_dbfMemoPackField( X_DBF_MEMO_PACK *pack, const X_DBF_FIELD *field, char *record );
X_DBF_STATUS              x_dbfMemoPackSeal( X_DBF_MEMO_PACK *pack );
X_DBF_STATUS              x_dbfMemoPackApply( X_DBF_MEMO_PACK *pack );
void                      x_dbfMemoPackKeep( X_DBF_MEMO_PACK *pack );
void                      x_dbfMemoPackDispose( X_DBF_MEMO_PACK *pack );
X_DBF_STATUS x_dbfMemoRead( X_DBF_MEMO_FILE *memo, const X_DBF_FIELD *field, const char *record, X_DBF_TEXT *result,
                            int *binary );
X_DBF_STATUS x_dbfMemoWrite( X_DBF_MEMO_FILE *memo, const X_DBF_FIELD *field, const char *data, size_t length,
                             char *pointer );
X_DBF_STATUS x_dbfMemoExport( X_DBF_MEMO_FILE *memo, const X_DBF_FIELD *field, const char *record,
                              const X_ITEM *target );
X_DBF_STATUS x_dbfMemoImport( X_DBF_MEMO_FILE *memo, const X_DBF_FIELD *field, const X_ITEM *source, uint64_t length,
                              char *pointer );
X_DBF_STATUS x_dbfMemoReadValue( X_DBF_MEMO_FILE *memo, const X_DBF_FIELD *field, const char *record, X_ITEM *result );
X_DBF_STATUS x_dbfMemoWriteValue( X_DBF_MEMO_FILE *memo, const X_DBF_FIELD *field, const X_ITEM *value, char *pointer );
X_DBF_STATUS x_dbfSmtEncode( const X_ITEM *value, X_DBF_TEXT *result );
X_DBF_STATUS x_dbfSmtDecode( const X_DBF_TEXT *bytes, unsigned int type, X_ITEM *result );
X_DBF_STATUS x_dbfFlexEncode( const X_ITEM *value, X_DBF_TEXT *result, uint32_t *type );
X_DBF_STATUS x_dbfFlexDecode( const X_DBF_TEXT *bytes, uint32_t type, X_ITEM *result );
X_DBF_STATUS x_dbfMemoInfo( X_DBF_MEMO_FILE *memo, int selector, X_ITEM *result );
X_DBF_STATUS x_dbfFieldReadMemo( DBF area, const X_DBF_FIELD *field, X_DBF_TEXT *result, int *binary );
X_DBF_STATUS x_dbfFieldWriteMemo( DBF area, const X_DBF_FIELD *field, const char *data, size_t length );
X_DBF_DRIVER_ENTRY *x_dbfDriverFind( const X_DBF_SESSION *session, const char *name );
X_DBF_STATUS        x_dbfCheckWrite( DBF area );
X_DBF_STATUS        x_dbfFormatNumeric( const X_DBF_FIELD *field, const X_ITEM *value, char *output );
X_DBF_STATUS        x_dbfAreaEvalBlock( X_CALL_CTX *ctx, DBF area, const X_ITEM *block, X_ITEM *result );
X_DBF_STATUS x_dbfAreaEvalBlockArgs( X_CALL_CTX *ctx, DBF area, const X_ITEM *block, int32_t argc, const X_ARG *args,
                                     X_ITEM *result );
X_DBF_STATUS x_dbfAreaAccept( DBF area, int skipDeleted, int *accepted );
X_DBF_STATUS x_dbfAreaMarkChildren( DBF area );
void         x_dbfAreaDetachRelations( DBF area );
X_DBF_STATUS x_dbfOrderCreate( DBF area, const X_DBF_INDEX_OPTIONS *options );
X_DBF_STATUS x_dbfOrderOpen( DBF area, const char *path, const X_ITEM *bindings );
X_DBF_STATUS x_dbfOrderClear( DBF area );
void         x_dbfOrderDispose( DBF area );
X_DBF_STATUS x_dbfOrderRebuild( DBF area );
X_DBF_STATUS x_dbfOrderPrepare( DBF area );
X_DBF_STATUS x_dbfOrderApply( DBF area );
X_DBF_STATUS x_dbfOrderBoundary( DBF area, int bottom, int skipDeleted );
X_DBF_STATUS x_dbfOrderSkip( DBF area, int64_t records, int skipDeleted );
X_DBF_STATUS x_dbfOrderSeek( DBF area, const X_ITEM *value, int soft, int last, int skipDeleted );
X_DBF_STATUS x_dbfOrderFocus( DBF area, const X_ITEM *selector, const char *bag, X_ITEM *previous );
X_DBF_STATUS x_dbfOrderDestroy( DBF area, const X_ITEM *selector, const char *bag );
X_DBF_STATUS x_dbfOrderInfo( DBF area, const X_ITEM *selector, const X_ITEM *bag, int info, const X_ITEM *value,
                             X_ITEM *result );
X_DBF_STATUS x_dbfDriverInfo( const X_DBF_DRIVER_ENTRY *driver, int selector, X_ITEM *result );
X_DBF_STATUS x_dbfOrderScope( DBF area, int scope, const X_ITEM *value, int set, X_ITEM *result );
X_DBF_STATUS x_dbfOrderLock( DBF area, int write, X_DBF_INDEX_LOCK *guard );
X_DBF_STATUS x_dbfOrderCommit( DBF area );
void         x_dbfOrderUnlock( X_DBF_INDEX_LOCK *guard );
void         x_dbfOrderCursorClear( DBF area );

/* dbf-internal.c: helpers of the functions in dbf-api.c */
const char  *x_dbfDefaultDriver( void );
int          x_dbfNetError( void );
void         x_dbfNetErrorSet( int netError );
char        *x_dbfAliasName( const char *path );
void         x_dbfBuildDrop( X_DBF_INDEX_BUILD *build );
void         x_dbfClearIndexes( X_CALL_CTX *ctx, const char *operation );
X_DBF_STATUS x_dbfCloseAreas( int continueAfterError );
int          x_dbfContention( X_DBF_STATUS status );
void         x_dbfCreateIndex( X_CALL_CTX *ctx, int order, const char *operation );
void         x_dbfCreateTable( X_CALL_CTX *ctx, const char *name, const X_ITEM *structure, const char *driver, int open,
                               const char *alias, const char *operation );
DBF          x_dbfCurrent( void );
DBF          x_dbfCursorArea( X_CALL_CTX *ctx, const char *operation );
int          x_dbfDecimalText( const char *text, size_t length, unsigned int decimals );
void         x_dbfDefault( X_CALL_CTX *ctx, const char *operation );
void         x_dbfDeleted( X_CALL_CTX *ctx, int deleted, const char *operation );
DBF   x_dbfEvalAreaBlock( X_CALL_CTX *ctx, uint32_t number, uint64_t identity, const X_ITEM *block, X_ITEM *result );
void  x_dbfExistingIndex( X_CALL_CTX *ctx, int argument, const char *operation );
void  x_dbfFieldFile( X_CALL_CTX *ctx, int import, int64_t ordinal, int64_t mode, const char *operation );
int   x_dbfInteger( const X_ITEM *item, int64_t *value );
void  x_dbfLocate( X_CALL_CTX *ctx, int continuing );
void  x_dbfLock( X_CALL_CTX *ctx, const char *operation, int fileLock, int release );
void  x_dbfMakeFieldBlock( X_CALL_CTX *ctx, int fixedArea );
DBF   x_dbfNamedArea( X_CALL_CTX *ctx, int argument, int defaultCurrent );
int   x_dbfNameList( X_CALL_CTX *ctx, int argument );
char *x_dbfOpenPath( const char *name );
int   x_dbfOptional( X_CALL_CTX *ctx, int argument );
void  x_dbfOrderExtension( X_CALL_CTX *ctx, const char *operation );
void  x_dbfOrderInfoApi( X_CALL_CTX *ctx, int info, const X_ITEM *value, const char *operation );
void  x_dbfPutFieldValue( X_CALL_CTX *ctx, DBF area, size_t ordinal, const X_ITEM *value, const char *operation );
void  x_dbfRebuildIndexes( X_CALL_CTX *ctx, const char *operation );
void  x_dbfRecordCount( X_CALL_CTX *ctx, const char *operation );
void  x_dbfReorganize( X_CALL_CTX *ctx, int pack, const char *operation );
DBF   x_dbfRequireArea( X_CALL_CTX *ctx, const char *operation );
X_DBF_STATUS x_dbfSchema( const X_ITEM *structure, X_DBF_SCHEMA **schema );
void         x_dbfSchemaDrop( X_DBF_SCHEMA **schema );
int          x_dbfScopeArgs( X_CALL_CTX *ctx, int first );
int          x_dbfSetHeaderMode( const X_ITEM *item, int64_t *mode );
void         x_dbfSetRelation( X_CALL_CTX *ctx, int scoped, const char *operation );
int          x_dbfString( const X_ITEM *item );
char        *x_dbfTableName( const char *name );
void         x_dbfTextDrop( X_DBF_TEXT *text );

/* dbf-bulk.c: DbEval() and the bulk operations of dbf-api.c */
void x_dbfBulkAppendFrom( X_CALL_CTX *ctx );
void x_dbfBulkCopy( X_CALL_CTX *ctx );
void x_dbfBulkCopyStruct( X_CALL_CTX *ctx );
void x_dbfBulkCopyXStruct( X_CALL_CTX *ctx );
void x_dbfBulkCreateFrom( X_CALL_CTX *ctx );
void x_dbfBulkEval( X_CALL_CTX *ctx );
void x_dbfBulkExportCsv( X_CALL_CTX *ctx );
void x_dbfBulkImportCsv( X_CALL_CTX *ctx );
void x_dbfBulkJoin( X_CALL_CTX *ctx );
void x_dbfBulkSort( X_CALL_CTX *ctx );
void x_dbfBulkTally( X_CALL_CTX *ctx, int kind );
void x_dbfBulkText( X_CALL_CTX *ctx, int sdf );
void x_dbfBulkTotal( X_CALL_CTX *ctx );
void x_dbfBulkUpdate( X_CALL_CTX *ctx );

#endif
