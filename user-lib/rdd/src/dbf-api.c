/*
 * Copyright (c) 2024-2026 xDev-Language
 *
 * Library...: xDev 7.0.0 RDD
 * File......: dbf-api.c
 *
 */

#include "dbf-internal.h"

#include <xdev/dbinfo.ch>
#include <xdev/dbstruct.ch>
#include <xdev/rddsys.ch>
#include <stdio.h>
#include <string.h>

// Fill arrays with the structure of the current database file
// AFields( [aFieldNames], [aTypes], [aWidths], [aDecimals] ) --> nFields
X_FUNC( AFIELDS )
{
   DBF    area;
   size_t i, count;
   int    argument, supplied = 0;

   area = x_dbfCurrent();
   if( area == NULL )
   {
      x_retni( ctx, 0 );
      return;
   }

   count = area->schema->count;
   for( argument = 1; argument <= 4; ++argument )
   {
      const X_ITEM *array = x_param( ctx, argument, X_IT_ARRAY );
      if( array != NULL )
      {
         size_t length = ( size_t )x_arrayLen( array );
         supplied = 1;
         if( length < count )
            count = length;
      }
   }

   if( !supplied )
      count = 0;

   for( argument = 1; argument <= 4; ++argument )
   {
      X_ITEM *array = ( X_ITEM * )x_param( ctx, argument, X_IT_ARRAY );
      size_t  available = IIF( array != NULL, count, 0 );
      for( i = 0; i < available; ++i )
      {
         const X_DBF_FIELD *field = &area->schema->fields[i];
         char               type = ( char )field->type;
         switch( argument )
         {
            case 1: x_arraySetC( array, i + 1, field->name ); break;
            case 2: x_arraySetCL( array, i + 1, &type, 1 ); break;
            case 3: x_arraySetNI( array, i + 1, field->length ); break;
            case 4: x_arraySetNI( array, i + 1, field->decimals ); break;
         }
      }
   }

   x_retns( ctx, ( ptrdiff_t )count );
}

// Return a specified work area alias
// Alias( [nWorkArea] ) --> cAlias
X_FUNC( ALIAS )
{
   int64_t number;
   DBF     area;

   X_ARGCHECK( ctx, x_dbfOptional( ctx, 1 ) || x_dbfInteger( x_paritem( ctx, 1 ), &number ), "ALIAS" );
   area = x_dbfNamedArea( ctx, 1, 1 );
   x_retc( ctx, IIF( area != NULL, area->alias, "" ) );
}

// Export the contents of a BLOB pointer to a file
// BLOBDIRECTEXPORT( nPointer, cTargetFile, [nMode] ) --> lSuccess
X_FUNC( BLOBDIRECTEXPORT )
{
   x_errorRaiseNamed( ctx->result, 1900, "BLOBDIRECTEXPORT" );
}

// Retrieve data from a BLOB file without referencing a field
// BLOBDIRECTGET( nPointer, [nStart], [nCount] ) --> expBLOB
X_FUNC( BLOBDIRECTGET )
{
   x_errorRaiseNamed( ctx->result, 1900, "BLOBDIRECTGET" );
}

// Import a file into a BLOB file and return pointer to the data
// BLOBDIRECTIMPORT( nOldPointer, cSourceFile ) --> nNewPointer
X_FUNC( BLOBDIRECTIMPORT )
{
   x_errorRaiseNamed( ctx->result, 1900, "BLOBDIRECTIMPORT" );
}

// Put data in a BLOB file without referencing a specific field
// BLOBDIRECTPUT( nOldPointer, uBLOB ) --> nNewPointer
X_FUNC( BLOBDIRECTPUT )
{
   x_errorRaiseNamed( ctx->result, 1900, "BLOBDIRECTPUT" );
}

// Copy the contents of a memo field number to a BLOB file
// BLOBEXPORT( nFieldPos, cTargetFile, [nMode] ) --> lSuccess
X_FUNC( BLOBEXPORT )
{
   x_errorRaiseNamed( ctx->result, 1900, "BLOBEXPORT" );
}

// Get the contents of a BLOB, identified by memo field number
// BLOBGET( nFieldPos, [nStart], [nCount] ) --> uBLOB
X_FUNC( BLOBGET )
{
   x_errorRaiseNamed( ctx->result, 1900, "BLOBGET" );
}

// Read the contents of a BLOB file into a memo field
// BLOBIMPORT( nFieldPos, cSourceFile ) --> lSuccess
X_FUNC( BLOBIMPORT )
{
   x_errorRaiseNamed( ctx->result, 1900, "BLOBIMPORT" );
}

// Delete the root area of a BLOB file
// BlobRootDelete( ) --> lSuccess
X_FUNC( BLOBROOTDELETE )
{
   x_errorRaiseNamed( ctx->result, 1900, "BLOBROOTDELETE" );
}

// Retrieve the data from the root area of a BLOB file
// BLOBROOTGET( ) --> uBLOB
X_FUNC( BLOBROOTGET )
{
   x_errorRaiseNamed( ctx->result, 1900, "BLOBROOTGET" );
}

// Obtain a lock on the root area of a BLOB file
// BLOBROOTLOCK( ) --> lSuccess
X_FUNC( BLOBROOTLOCK )
{
   x_errorRaiseNamed( ctx->result, 1900, "BLOBROOTLOCK" );
}

// Store data in the root area of a BLOB file
// BLOBROOTPUT( uBLOB ) --> lSuccess
X_FUNC( BLOBROOTPUT )
{
   x_errorRaiseNamed( ctx->result, 1900, "BLOBROOTPUT" );
}

// Release the lock on a BLOB file's root area
// BLOBROOTUNLOCK( ) --> NIL
X_FUNC( BLOBROOTUNLOCK )
{
   x_errorRaiseNamed( ctx->result, 1900, "BLOBROOTUNLOCK" );
}

// Determine when beginning of file is encountered
// Bof( ) --> lBoundary
X_FUNC( BOF )
{
   DBF area = x_dbfCursorArea( ctx, "BOF" );
   x_retl( ctx, area == NULL || area->state.bof );
}

// Append a new record to the database open in the current work area
// dbAppend( [lReleaseRecLocks] ) --> NIL
X_FUNC( DBAPPEND )
{
   DBF          area;
   X_DBF_STATUS status;

   X_ARGCHECK( ctx, x_dbfOptional( ctx, 1 ) || X_ISLOG( ctx, 1 ), "DBAPPEND" );
   area = x_dbfRequireArea( ctx, "DBAPPEND" );
   if( area == NULL )
      return;
   x_dbfNetErrorSet( 0 );
   if( area->readOnly )
   {
      x_dbfRaise( ctx, X_DBF_READ_ONLY, "DBAPPEND" );
      return;
   }
   status = IIF( x_parldef( ctx, 1, 1 ), x_dbfAreaUnlock( area, 0 ), X_DBF_OK );
   if( status == X_DBF_OK )
      status = x_dbfAreaAppend( area );
   if( x_dbfContention( status ) )
      x_dbfNetErrorSet( 1 );
   else if( status != X_DBF_OK )
   {
      x_dbfRaise( ctx, status, "DBAPPEND" );
      return;
   }

   x_retl( ctx, status == X_DBF_OK );
}

// Append records from a database file to the current work area
// DbAppendFrom( cFile, [aFields], [bFor], [bWhile], [nNext], [nRecord], [lRest], [cRDD] ) --> lSuccess
X_FUNC( DBAPPENDFROM )
{
   X_ARGCHECK( ctx, x_dbfString( x_paritem( ctx, 1 ) ) && x_parclen( ctx, 1 ) != 0, "DBAPPENDFROM" );
   X_ARGCHECK( ctx, x_dbfNameList( ctx, 2 ), "DBAPPENDFROM" );
   X_ARGCHECK( ctx, x_dbfScopeArgs( ctx, 3 ), "DBAPPENDFROM" );
   X_ARGCHECK( ctx, x_dbfOptional( ctx, 8 ) || x_dbfString( x_paritem( ctx, 8 ) ), "DBAPPENDFROM" );
   x_dbfBulkAppendFrom( ctx );
}

// Return the average of an expression over the records in a scope
// DbAverage( bExpression, [bFor], [bWhile], [nNext], [nRecord], [lRest] ) --> nAverage
X_FUNC( DBAVERAGE )
{
   X_ARGCHECK( ctx, X_ISBLOCK( ctx, 1 ), "DBAVERAGE" );
   X_ARGCHECK( ctx, x_dbfScopeArgs( ctx, 2 ), "DBAVERAGE" );
   x_dbfBulkTally( ctx, 2 );
}

// Clear a filter condition
// dbClearFilter( ) --> NIL
X_FUNC( DBCLEARFILTER )
{
   DBF area = x_dbfRequireArea( ctx, "DBCLEARFILTER" );
   if( area == NULL )
      return;
   x_dbfAreaClearFilter( area );

   x_ret( ctx );
}

// Close all indexes for the current work area
// dbClearIndex( ) --> NIL
X_FUNC( DBCLEARINDEX )
{
   x_dbfClearIndexes( ctx, "DBCLEARINDEX" );
}

// Clear active relations
// dbClearRelation( ) --> NIL
X_FUNC( DBCLEARRELATION )
{
   DBF area = x_dbfCurrent();
   if( area != NULL )
      x_dbfRaise( ctx, x_dbfAreaClearRelations( area ), "DBCLEARRELATION" );

   x_ret( ctx );
}

// Close all occupied work areas
// dbCloseAll( ) --> NIL
X_FUNC( DBCLOSEALL )
{
   x_dbfRaise( ctx, x_dbfCloseAreas( 1 ), "DBCLOSEALL" );
   x_dbfSessionSelect( x_dbfRuntimeSession(), 1 );

   x_ret( ctx );
}

// Close a work area
// dbCloseArea( ) --> NIL
X_FUNC( DBCLOSEAREA )
{
   DBF area = x_dbfCurrent();

   if( area != NULL )
      x_dbfRaise( ctx, x_dbfAreaClose( area ), "DBCLOSEAREA" );
   x_ret( ctx );
}

// Flush pending updates
// dbCommit( ) --> NIL
X_FUNC( DBCOMMIT )
{
   DBF area = x_dbfRequireArea( ctx, "DBCOMMIT" );

   if( area == NULL )
      return;
   x_dbfRaise( ctx, x_dbfAreaCommit( area ), "DBCOMMIT" );

   x_ret( ctx );
}

// Flush pending updates in all work areas
// dbCommitAll( ) --> NIL
X_FUNC( DBCOMMITALL )
{
   X_DBF_STATUS status = X_DBF_OK, next;
   DBF          area;

   for( area = x_dbfRuntimeSession()->areas; area != NULL; area = area->next )
   {
      next = x_dbfAreaCommit( area );
      if( status == X_DBF_OK )
         status = next;
   }

   x_dbfRaise( ctx, status, "DBCOMMITALL" );

   x_ret( ctx );
}

// Resume a pending LOCATE
// DbContinue( ) --> NIL
X_FUNC( DBCONTINUE )
{
   x_dbfLocate( ctx, 1 );
}

// Copy records of the current work area to a new database file
// DbCopy( cFile, [aFields], [bFor], [bWhile], [nNext], [nRecord], [lRest], [cRDD] ) --> lSuccess
X_FUNC( DBCOPY )
{
   X_ARGCHECK( ctx, x_dbfString( x_paritem( ctx, 1 ) ) && x_parclen( ctx, 1 ) != 0, "DBCOPY" );
   X_ARGCHECK( ctx, x_dbfNameList( ctx, 2 ), "DBCOPY" );
   X_ARGCHECK( ctx, x_dbfScopeArgs( ctx, 3 ), "DBCOPY" );
   X_ARGCHECK( ctx, x_dbfOptional( ctx, 8 ) || x_dbfString( x_paritem( ctx, 8 ) ), "DBCOPY" );

   x_dbfBulkCopy( ctx );
}

// Create an empty database file with the structure of the current work area
// DbCopyStruct( cFile, [aFields] ) --> lSuccess
X_FUNC( DBCOPYSTRUCT )
{
   X_ARGCHECK( ctx, x_dbfString( x_paritem( ctx, 1 ) ) && x_parclen( ctx, 1 ) != 0, "DBCOPYSTRUCT" );
   X_ARGCHECK( ctx, x_dbfNameList( ctx, 2 ), "DBCOPYSTRUCT" );

   x_dbfBulkCopyStruct( ctx );
}

// Copy the structure of the current work area to an extended structure file
// DbCopyXStruct( cFile ) --> lSuccess
X_FUNC( DBCOPYXSTRUCT )
{
   X_ARGCHECK( ctx, x_dbfString( x_paritem( ctx, 1 ) ) && x_parclen( ctx, 1 ) != 0, "DBCOPYXSTRUCT" );

   x_dbfBulkCopyXStruct( ctx );
}

// Return the number of records in a scope
// DbCount( [bFor], [bWhile], [nNext], [nRecord], [lRest] ) --> nCount
X_FUNC( DBCOUNT )
{
   X_ARGCHECK( ctx, x_dbfScopeArgs( ctx, 1 ), "DBCOUNT" );
   x_dbfBulkTally( ctx, 0 );
}

// Create a database file from a database structure array
// dbCreate( cDatabase, aStruct, [cDriver], [lNewArea], [cAlias], [xDelimiter], [nConnection] ) --> NIL
X_FUNC( DBCREATE )
{
   int64_t connection = 0;

   X_ARGCHECK( ctx, x_dbfString( x_paritem( ctx, 1 ) ) && x_parclen( ctx, 1 ) != 0, "DBCREATE" );
   X_ARGCHECK( ctx, X_ISARRAY( ctx, 2 ), "DBCREATE" );
   X_ARGCHECK( ctx, x_dbfOptional( ctx, 3 ) || x_dbfString( x_paritem( ctx, 3 ) ), "DBCREATE" );
   X_ARGCHECK( ctx, x_dbfOptional( ctx, 4 ) || X_ISLOG( ctx, 4 ), "DBCREATE" );
   X_ARGCHECK( ctx, x_dbfOptional( ctx, 5 ) || x_dbfString( x_paritem( ctx, 5 ) ), "DBCREATE" );
   X_ARGCHECK( ctx, x_dbfOptional( ctx, 6 ), "DBCREATE" );
   X_ARGCHECK( ctx, x_dbfOptional( ctx, 7 ) || ( x_dbfInteger( x_paritem( ctx, 7 ), &connection ) && connection == 0 ),
               "DBCREATE" );

   x_dbfCreateTable( ctx, x_parc( ctx, 1 ), x_paritem( ctx, 2 ), x_parc( ctx, 3 ),
                     IIF( X_ISLOG( ctx, 4 ), x_parl( ctx, 4 ), -1 ), x_parc( ctx, 5 ), "DBCREATE" );
}

// Create a database file from an extended structure file
// DbCreateFrom( cFile, cFrom, [cRDD], [lNew], [cAlias] ) --> lSuccess
X_FUNC( DBCREATEFROM )
{
   X_ARGCHECK( ctx, x_dbfString( x_paritem( ctx, 1 ) ) && x_parclen( ctx, 1 ) != 0, "DBCREATEFROM" );
   X_ARGCHECK( ctx, x_dbfString( x_paritem( ctx, 2 ) ) && x_parclen( ctx, 2 ) != 0, "DBCREATEFROM" );
   X_ARGCHECK( ctx, x_dbfOptional( ctx, 3 ) || x_dbfString( x_paritem( ctx, 3 ) ), "DBCREATEFROM" );
   X_ARGCHECK( ctx, x_dbfOptional( ctx, 4 ) || X_ISLOG( ctx, 4 ), "DBCREATEFROM" );
   X_ARGCHECK( ctx, x_dbfOptional( ctx, 5 ) || x_dbfString( x_paritem( ctx, 5 ) ), "DBCREATEFROM" );

   x_dbfBulkCreateFrom( ctx );
}

// Create an index file
// dbCreateIndex( cIndexName, cKeyExpr, bKeyExpr, [lUnique] ) --> NIL
X_FUNC( DBCREATEINDEX )
{
   X_ARGCHECK( ctx, x_dbfOptional( ctx, 1 ) || x_dbfString( x_paritem( ctx, 1 ) ), "DBCREATEINDEX" );
   X_ARGCHECK( ctx, x_dbfString( x_paritem( ctx, 2 ) ), "DBCREATEINDEX" );
   X_ARGCHECK( ctx, X_ISBLOCK( ctx, 3 ), "DBCREATEINDEX" );
   X_ARGCHECK_OPT( ctx, 4, X_ISNIL( ctx, 4 ) || X_ISLOG( ctx, 4 ), "DBCREATEINDEX" );

   x_dbfCreateIndex( ctx, 0, "DBCREATEINDEX" );
}

// Mark a record for deletion
// dbDelete( ) --> NIL
X_FUNC( DBDELETE )
{
   x_dbfDeleted( ctx, 1, "DBDELETE" );
}

// Export records to, or import them from, a delimited text file
// DbDelim( lExport, cFile, [cDelimiter], [aFields], [bFor], [bWhile], [nNext], [nRecord], [lRest] ) --> lSuccess
X_FUNC( DBDELIM )
{
   X_ARGCHECK( ctx, X_ISLOG( ctx, 1 ), "DBDELIM" );
   X_ARGCHECK( ctx, x_dbfString( x_paritem( ctx, 2 ) ) && x_parclen( ctx, 2 ) != 0, "DBDELIM" );
   X_ARGCHECK( ctx,
               x_dbfOptional( ctx, 3 ) || x_dbfString( x_paritem( ctx, 3 ) ) ||
                  ( X_ISARRAY( ctx, 3 ) && x_arrayLen( x_paritem( ctx, 3 ) ) >= 2 ),
               "DBDELIM" );
   X_ARGCHECK( ctx, x_dbfNameList( ctx, 4 ), "DBDELIM" );
   X_ARGCHECK( ctx, x_dbfScopeArgs( ctx, 5 ), "DBDELIM" );

   x_dbfBulkText( ctx, 0 );
}

// Evaluate a code block for each record matching a scope and condition
// dbEval( bBlock, [bForCondition], [bWhileCondition], [nNextRecords], [nRecord], [lRest] ) --> NIL
X_FUNC( DBEVAL )
{
   X_ARGCHECK( ctx, X_ISBLOCK( ctx, 1 ), "DBEVAL" );
   X_ARGCHECK( ctx, x_dbfScopeArgs( ctx, 2 ), "DBEVAL" );

   x_dbfBulkEval( ctx );
}

// Return the alias of the current work area
// Dbf( ) --> cAlias
X_FUNC_ALIAS( DBF, XDEV_DBF_ALIAS )
{
   DBF area = x_dbfCurrent();

   x_retc( ctx, IIF( area != NULL, area->alias, "" ) );
}

// Export the current work area to CSV: UTF-8 with BOM, ";" separator, header row with field names
// DbExportCsv( cFile, [aFields], [bFor] ) --> nRecords
X_FUNC( DBEXPORTCSV )
{
   X_ARGCHECK( ctx, x_dbfString( x_paritem( ctx, 1 ) ) && x_parclen( ctx, 1 ) != 0, "DBEXPORTCSV" );
   X_ARGCHECK( ctx, x_dbfNameList( ctx, 2 ), "DBEXPORTCSV" );
   X_ARGCHECK( ctx, x_dbfOptional( ctx, 3 ) || X_ISBLOCK( ctx, 3 ), "DBEXPORTCSV" );

   x_dbfBulkExportCsv( ctx );
}

// Initialize the database runtime for the DBFCDX driver entry point
// dbfCdx( ) --> NIL
X_FUNC( DBFCDX )
{
   x_dbfRuntimeSession();

   x_ret( ctx );
}

// Initialize the database runtime for the DBFDBT driver entry point
// dbfDbt( ) --> NIL
X_FUNC( DBFDBT )
{
   x_dbfRuntimeSession();

   x_ret( ctx );
}

// Initialize the database runtime for the DBFFPT driver entry point
// dbfFpt( ) --> NIL
X_FUNC( DBFFPT )
{
   x_dbfRuntimeSession();

   x_ret( ctx );
}

// Return and optionally change information about a field
// dbFieldInfo( nInfoType, nFieldPos, [expNewSetting] ) --> uCurrentSetting
X_FUNC( DBFIELDINFO )
{
   int64_t selector, ordinal;
   DBF     area;

   X_ARGCHECK( ctx, x_dbfInteger( x_paritem( ctx, 1 ), &selector ) && selector >= INT32_MIN && selector <= INT32_MAX,
               "DBFIELDINFO" );
   X_ARGCHECK( ctx, x_dbfInteger( x_paritem( ctx, 2 ), &ordinal ) && ordinal > 0 && ordinal <= UINT16_MAX,
               "DBFIELDINFO" );

   area = x_dbfRequireArea( ctx, "DBFIELDINFO" );
   if( area == NULL )
      return;

   if( !x_dbfOptional( ctx, 3 ) )
   {
      x_dbfRaise( ctx, X_DBF_UNSUPPORTED_OPERATION, "DBFIELDINFO" );
      return;
   }

   x_dbfRaise( ctx, x_dbfFieldInfo( area, ( size_t )ordinal, ( int )selector, ctx->result ), "DBFIELDINFO" );
}

// Insert the contents of a field into a file
// dbFileGet( nFieldPos, cTargetFile, nMode ) --> lSuccess
X_FUNC( DBFILEGET )
{
   int64_t ordinal, mode = 0;

   X_ARGCHECK( ctx, x_dbfInteger( x_paritem( ctx, 1 ), &ordinal ) && ordinal > 0 && ordinal <= UINT16_MAX,
               "DBFILEGET" );
   X_ARGCHECK( ctx, x_dbfString( x_paritem( ctx, 2 ) ) && x_parclen( ctx, 2 ) != 0, "DBFILEGET" );
   X_ARGCHECK( ctx, x_dbfOptional( ctx, 3 ) || x_dbfInteger( x_paritem( ctx, 3 ), &mode ), "DBFILEGET" );

   if( mode < 0 || mode > 1 )
   {
      x_dbfRaise( ctx, X_DBF_UNSUPPORTED_OPERATION, "DBFILEGET" );
      return;
   }

   x_dbfFieldFile( ctx, 0, ordinal, mode, "DBFILEGET" );
}

// Insert the contents of a file into a field
// dbFilePut( nFieldPos, cSourceFile ) --> lSuccess
X_FUNC( DBFILEPUT )
{
   int64_t ordinal, mode = 0;

   X_ARGCHECK( ctx, x_dbfInteger( x_paritem( ctx, 1 ), &ordinal ) && ordinal > 0 && ordinal <= UINT16_MAX,
               "DBFILEPUT" );
   X_ARGCHECK( ctx, x_dbfString( x_paritem( ctx, 2 ) ) && x_parclen( ctx, 2 ) != 0, "DBFILEPUT" );
   X_ARGCHECK( ctx, x_dbfOptional( ctx, 3 ) || x_dbfInteger( x_paritem( ctx, 3 ), &mode ), "DBFILEPUT" );

   if( mode != 0 )
   {
      x_dbfRaise( ctx, X_DBF_UNSUPPORTED_OPERATION, "DBFILEPUT" );
      return;
   }

   x_dbfFieldFile( ctx, 1, ordinal, mode, "DBFILEPUT" );
}

// Return the current filter expression as a character string
// dbFilter( ) --> cFilter
X_FUNC( DBFILTER )
{
   DBF area = x_dbfCurrent();

   if( area == NULL )
      x_retc( ctx, "" );
   else
      x_dbfRaise( ctx, x_dbfAreaFilterText( area, ctx->result ), "DBFILTER" );
}

// Initialize the database runtime for the DBFNTX driver entry point
// dbfNtx( ) --> NIL
X_FUNC( DBFNTX )
{
   x_dbfRuntimeSession();

   x_ret( ctx );
}

// Initialize the database runtime for the DBFSMT driver entry point
// dbfSmt( ) --> NIL
X_FUNC( DBFSMT )
{
   x_dbfRuntimeSession();

   x_ret( ctx );
}

// Move to the last logical record
// dbGoBottom( ) --> NIL
X_FUNC( DBGOBOTTOM )
{
   DBF area = x_dbfRequireArea( ctx, "DBGOBOTTOM" );

   if( area == NULL )
      return;
   x_dbfRaise( ctx, x_dbfAreaGoBottom( area, x_setDeleted ), "DBGOBOTTOM" );

   x_ret( ctx );
}

// Position record pointer to a specific identity
// dbGoTo( xIdentity ) --> NIL
X_FUNC( DBGOTO )
{
   int64_t record;
   DBF     area;

   X_ARGCHECK( ctx, x_dbfInteger( x_paritem( ctx, 1 ), &record ), "DBGOTO" );
   area = x_dbfRequireArea( ctx, "DBGOTO" );
   if( area == NULL )
      return;

   x_dbfRaise( ctx, x_dbfAreaGoTo( area, IIF( record > 0, ( uint64_t )record, 0 ) ), "DBGOTO" );

   x_ret( ctx );
}

// Move to the first logical record
// dbGoTop( ) --> NIL
X_FUNC( DBGOTOP )
{
   DBF area = x_dbfRequireArea( ctx, "DBGOTOP" );

   if( area == NULL )
      return;

   x_dbfRaise( ctx, x_dbfAreaGoTop( area, x_setDeleted ), "DBGOTOP" );
   x_ret( ctx );
}

// Import CSV into the current work area; columns are matched to fields by header names
// DbImportCsv( cFile ) --> nRecords
X_FUNC( DBIMPORTCSV )
{
   X_ARGCHECK( ctx, x_dbfString( x_paritem( ctx, 1 ) ) && x_parclen( ctx, 1 ) != 0, "DBIMPORTCSV" );

   x_dbfBulkImportCsv( ctx );
}

// Return and optionally change information about a database file opened in a work area
// dbInfo( nInfoType, [expNewSetting] ) --> uCurrentSetting
X_FUNC( DBINFO )
{
   int64_t selector;
   DBF     area;

   X_ARGCHECK( ctx, x_dbfInteger( x_paritem( ctx, 1 ), &selector ) && selector >= INT32_MIN && selector <= INT32_MAX,
               "DBINFO" );
   area = x_dbfRequireArea( ctx, "DBINFO" );

   if( area == NULL )
      return;

   if( selector == DBI_SCOPEDRELATION )
   {
      X_DBF_RELATION *relation = area->relations;
      int64_t         ordinal = 0;
      X_ARGCHECK( ctx, x_dbfOptional( ctx, 2 ) || x_dbfInteger( x_paritem( ctx, 2 ), &ordinal ), "DBINFO" );
      while( relation != NULL && ordinal > 1 )
      {
         --ordinal;
         relation = relation->next;
      }
      x_retl( ctx, ordinal == 1 && relation != NULL && relation->scoped );
      return;
   }

   if( selector == DBI_SETHEADER )
   {
      int64_t      mode = -1;
      int          previous;
      X_DBF_STATUS status;
      X_ARGCHECK( ctx, x_dbfOptional( ctx, 2 ) || x_dbfSetHeaderMode( x_paritem( ctx, 2 ), &mode ), "DBINFO" );
      status = x_dbfFileSetHeader( area, ( int )mode, &previous );
      if( status != X_DBF_OK )
      {
         x_dbfRaise( ctx, status, "DBINFO" );
         return;
      }
      x_retni( ctx, previous );
      return;
   }

   if( selector == DBI_LOCKTEST )
   {
      int64_t      record = 0;
      int          locked;
      X_DBF_STATUS status;
      X_ARGCHECK( ctx, x_dbfOptional( ctx, 2 ) || ( x_dbfInteger( x_paritem( ctx, 2 ), &record ) && record > 0 ),
                  "DBINFO" );
      status = x_dbfFileLockTest( area, ( uint64_t )record, &locked );
      if( status != X_DBF_OK )
      {
         x_dbfRaise( ctx, status, "DBINFO" );
         return;
      }
      x_retni( ctx, locked );
      return;
   }

   if( !x_dbfOptional( ctx, 2 ) )
   {
      if( selector == DBI_LOCKSCHEME )
      {
         int64_t      scheme;
         X_DBF_STATUS status;
         X_ARGCHECK( ctx,
                     x_dbfInteger( x_paritem( ctx, 2 ), &scheme ) && scheme >= DB_DBFLOCK_CLIPPER &&
                        scheme <= DB_DBFLOCK_CLIPPER2,
                     "DBINFO" );
         status = x_dbfAreaInfo( area, DBI_LOCKSCHEME, ctx->result );
         if( status == X_DBF_OK )
            status = x_dbfFileLockScheme( area, ( int )scheme );
         x_dbfRaise( ctx, status, "DBINFO" );
         return;
      }
      x_dbfRaise( ctx, X_DBF_UNSUPPORTED_OPERATION, "DBINFO" );
      return;
   }

   x_dbfRaise( ctx, x_dbfAreaInfo( area, ( int )selector, ctx->result ), "DBINFO" );
}

// Create a new database file by merging records from two work areas
// DbJoin( cAlias, cFile, [aFields], [bFor] ) --> lSuccess
X_FUNC( DBJOIN )
{
   X_ARGCHECK( ctx, x_dbfString( x_paritem( ctx, 1 ) ) && x_parclen( ctx, 1 ) != 0, "DBJOIN" );
   X_ARGCHECK( ctx, x_dbfString( x_paritem( ctx, 2 ) ) && x_parclen( ctx, 2 ) != 0, "DBJOIN" );
   X_ARGCHECK( ctx, x_dbfNameList( ctx, 3 ), "DBJOIN" );
   X_ARGCHECK( ctx, x_dbfOptional( ctx, 4 ) || X_ISBLOCK( ctx, 4 ), "DBJOIN" );

   x_dbfBulkJoin( ctx );
}

// Search sequentially for a record matching a condition
// DbLocate( [bForCondition], [bWhileCondition], [nNextRecords], [nRecord], [lRest] ) --> NIL
X_FUNC( DBLOCATE )
{
   int64_t number;

   X_ARGCHECK( ctx, x_dbfOptional( ctx, 1 ) || X_ISBLOCK( ctx, 1 ), "DBLOCATE" );
   X_ARGCHECK( ctx, x_dbfOptional( ctx, 2 ) || X_ISBLOCK( ctx, 2 ), "DBLOCATE" );
   X_ARGCHECK( ctx, x_dbfOptional( ctx, 3 ) || x_dbfInteger( x_paritem( ctx, 3 ), &number ), "DBLOCATE" );
   X_ARGCHECK( ctx, x_dbfOptional( ctx, 4 ) || x_dbfInteger( x_paritem( ctx, 4 ), &number ), "DBLOCATE" );
   X_ARGCHECK( ctx, x_dbfOptional( ctx, 5 ) || X_ISLOG( ctx, 5 ), "DBLOCATE" );

   x_dbfLocate( ctx, 0 );
}

// Return and optionally change information about orders and index files
// dbOrderInfo( nInfoType, [cIndexFile], [cOrder | nPosition], [expNewSetting] ) --> uCurrentSetting
X_FUNC( DBORDERINFO )
{
   int64_t      info, number;
   DBF          area;
   X_DBF_STATUS status;

   X_ARGCHECK( ctx, x_dbfInteger( x_paritem( ctx, 1 ), &info ) && info >= INT32_MIN && info <= INT32_MAX,
               "DBORDERINFO" );
   X_ARGCHECK_OPT( ctx, 2,
                   X_ISNIL( ctx, 2 ) || x_dbfString( x_paritem( ctx, 2 ) ) ||
                      x_dbfInteger( x_paritem( ctx, 2 ), &number ),
                   "DBORDERINFO" );
   X_ARGCHECK_OPT( ctx, 3,
                   X_ISNIL( ctx, 3 ) || x_dbfString( x_paritem( ctx, 3 ) ) ||
                      x_dbfInteger( x_paritem( ctx, 3 ), &number ),
                   "DBORDERINFO" );

   area = x_dbfRequireArea( ctx, "DBORDERINFO" );
   if( area == NULL )
      return;

   status = x_dbfAreaPrepare( area );
   if( status == X_DBF_OK )
      status = x_dbfOrderInfo( area, x_paritem( ctx, 3 ), x_paritem( ctx, 2 ), ( int )info, x_paritem( ctx, 4 ),
                               ctx->result );

   x_dbfRaise( ctx, status, "DBORDERINFO" );
}

// Remove records marked for deletion from the current database file
// dbPack( ) --> NIL
X_FUNC( DBPACK )
{
   x_dbfReorganize( ctx, 1, "DBPACK" );
}

// Reinstate a record marked for deletion
// dbRecall( ) --> NIL
X_FUNC( DBRECALL )
{
   x_dbfDeleted( ctx, 0, "DBRECALL" );
}

// Return and optionally change information about a record
// dbRecordInfo( nInfoType, [nRecord], [expNewSetting] ) --> uCurrentSetting
X_FUNC( DBRECORDINFO )
{
   int64_t selector, record = 0;
   DBF     area;

   X_ARGCHECK( ctx, x_dbfInteger( x_paritem( ctx, 1 ), &selector ) && selector >= INT32_MIN && selector <= INT32_MAX,
               "DBRECORDINFO" );
   X_ARGCHECK( ctx, x_dbfOptional( ctx, 2 ) || ( x_dbfInteger( x_paritem( ctx, 2 ), &record ) && record >= 0 ),
               "DBRECORDINFO" );
   area = x_dbfRequireArea( ctx, "DBRECORDINFO" );

   if( area == NULL )
      return;

   if( !x_dbfOptional( ctx, 3 ) )
   {
      x_dbfRaise( ctx, X_DBF_UNSUPPORTED_OPERATION, "DBRECORDINFO" );
      return;
   }

   x_dbfRaise( ctx, x_dbfRecordInfo( area, ( uint64_t )record, ( int )selector, ctx->result ), "DBRECORDINFO" );
}

// Recreate all active indexes for the current work area
// dbReindex( ) --> NIL
X_FUNC( DBREINDEX )
{
   x_dbfRebuildIndexes( ctx, "DBREINDEX" );
}

// Return the linking expression of a specified relation
// dbRelation( nRelation ) --> cLinkExp
X_FUNC( DBRELATION )
{
   DBF     area = x_dbfCurrent();
   int64_t ordinal = 1;

   x_dbfInteger( x_paritem( ctx, 1 ), &ordinal );

   if( ordinal == 0 )
      ordinal = 1;
   if( area == NULL || ordinal < 0 )
      x_retc( ctx, "" );
   else
      x_dbfRaise( ctx, x_dbfAreaRelationInfo( area, ( size_t )ordinal, NULL, ctx->result ), "DBRELATION" );
}

// Lock the record at the current or specified identity
// dbRLock( [xIdentity] ) --> lSuccess
X_FUNC( DBRLOCK )
{
   int64_t record = 0;

   X_ARGCHECK( ctx, x_dbfOptional( ctx, 1 ) || ( x_dbfInteger( x_paritem( ctx, 1 ), &record ) && record > 0 ),
               "DBRLOCK" );

   x_dbfLock( ctx, "DBRLOCK", 0, x_dbfOptional( ctx, 1 ) );
}

// Return an array of the current lock list
// dbRLockList( ) --> aRecordLocks
X_FUNC( DBRLOCKLIST )
{
   DBF area = x_dbfRequireArea( ctx, "DBRLOCKLIST" );

   if( area == NULL )
      return;

   x_dbfRaise( ctx, x_dbfAreaInfo( area, DBI_GETLOCKARRAY, ctx->result ), "DBRLOCKLIST" );
}

// Return the target work area number of a relation
// dbRSelect( nRelation ) --> nWorkArea
X_FUNC( DBRSELECT )
{
   DBF      area = x_dbfCurrent();
   int64_t  ordinal = 1;
   uint32_t child = 0;

   x_dbfInteger( x_paritem( ctx, 1 ), &ordinal );

   if( ordinal == 0 )
      ordinal = 1;
   if( area != NULL && ordinal > 0 )
      x_dbfRaise( ctx, x_dbfAreaRelationInfo( area, ( size_t )ordinal, &child, NULL ), "DBRSELECT" );

   x_retnint( ctx, child );
}

// Release all or specified record locks
// dbRUnlock( [xIdentity] ) --> NIL
X_FUNC( DBRUNLOCK )
{
   int64_t record = 0;
   DBF     area;

   X_ARGCHECK( ctx, x_dbfOptional( ctx, 1 ) || ( x_dbfInteger( x_paritem( ctx, 1 ), &record ) && record > 0 ),
               "DBRUNLOCK" );
   area = x_dbfRequireArea( ctx, "DBRUNLOCK" );
   if( area == NULL )
      return;

   x_dbfRaise( ctx, x_dbfAreaUnlock( area, ( uint64_t )record ), "DBRUNLOCK" );

   x_ret( ctx );
}

// Export records to, or import them from, a fixed-width text file
// DbSDF( lExport, cFile, [aFields], [bFor], [bWhile], [nNext], [nRecord], [lRest] ) --> lSuccess
X_FUNC( DBSDF )
{
   X_ARGCHECK( ctx, X_ISLOG( ctx, 1 ), "DBSDF" );
   X_ARGCHECK( ctx, x_dbfString( x_paritem( ctx, 2 ) ) && x_parclen( ctx, 2 ) != 0, "DBSDF" );
   X_ARGCHECK( ctx, x_dbfNameList( ctx, 3 ), "DBSDF" );
   X_ARGCHECK( ctx, x_dbfScopeArgs( ctx, 4 ), "DBSDF" );

   x_dbfBulkText( ctx, 1 );
}

// Move to the record having the specified key value
// dbSeek( expKey, [lSoftSeek], [lLast] ) --> lFound
X_FUNC( DBSEEK )
{
   DBF area;

   X_ARGCHECK( ctx, X_ISCHAR( ctx, 1 ) || X_ISNUM( ctx, 1 ) || X_ISDATE( ctx, 1 ) || X_ISLOG( ctx, 1 ), "DBSEEK" );
   X_ARGCHECK_OPT( ctx, 2, X_ISNIL( ctx, 2 ) || X_ISLOG( ctx, 2 ), "DBSEEK" );
   X_ARGCHECK_OPT( ctx, 3, X_ISNIL( ctx, 3 ) || X_ISLOG( ctx, 3 ), "DBSEEK" );

   area = x_dbfRequireArea( ctx, "DBSEEK" );
   if( area == NULL )
      return;

   x_dbfRaise( ctx,
               x_dbfOrderSeek( area, x_paritem( ctx, 1 ),
                               IIF( x_dbfOptional( ctx, 2 ), x_setSoftSeek, x_parl( ctx, 2 ) ), x_parl( ctx, 3 ),
                               x_setDeleted ),
               "DBSEEK" );

   x_retl( ctx, area->state.found );
}

// Change the current work area
// dbSelectArea( nArea | cAlias ) --> NIL
X_FUNC( DBSELECTAREA )
{
   int64_t        number = 0;
   X_DBF_SESSION *session = x_dbfRuntimeSession();

   X_ARGCHECK( ctx,
               x_dbfOptional( ctx, 1 ) || x_dbfString( x_paritem( ctx, 1 ) ) ||
                  ( x_dbfInteger( x_paritem( ctx, 1 ), &number ) && number >= 0 && number <= X_DBF_MAX_AREA ),
               "DBSELECTAREA" );

   if( x_dbfString( x_paritem( ctx, 1 ) ) )
   {
      DBF area = x_dbfSessionAlias( session, x_parc( ctx, 1 ) );
      if( area == NULL )
      {
         X_ERROR_DEF error = { 2, 15, 1002, 0, "BASE", "Alias does not exist", x_parc( ctx, 1 ) };
         x_errorRaiseDefinition( ctx->result, &error, x_paritem( ctx, 1 ), NULL );
         return;
      }
      number = area->number;
   }

   x_dbfRaise( ctx, x_dbfSessionSelect( session, ( uint32_t )number ), "DBSELECTAREA" );

   x_ret( ctx );
}

// Return the default database driver and optionally set a new driver
// dbSetDriver( [cDriver] ) --> cCurrentDriver
X_FUNC( DBSETDRIVER )
{
   X_ARGCHECK( ctx, x_dbfOptional( ctx, 1 ) || x_dbfString( x_paritem( ctx, 1 ) ), "DBSETDRIVER" );

   x_dbfDefault( ctx, "DBSETDRIVER" );
}

// Set a filter condition
// dbSetFilter( bCondition, [cCondition] ) --> NIL
X_FUNC( DBSETFILTER )
{
   DBF area;

   X_ARGCHECK( ctx, X_ISBLOCK( ctx, 1 ), "DBSETFILTER" );
   X_ARGCHECK( ctx, x_dbfOptional( ctx, 2 ) || x_dbfString( x_paritem( ctx, 2 ) ), "DBSETFILTER" );

   area = x_dbfRequireArea( ctx, "DBSETFILTER" );
   if( area == NULL )
      return;

   x_dbfRaise( ctx, x_dbfAreaSetFilter( area, x_paritem( ctx, 1 ), x_param( ctx, 2, X_IT_STRING ) ), "DBSETFILTER" );

   x_ret( ctx );
}

// Empty orders from an order bag into the order list
// dbSetIndex( cOrderBagName ) --> NIL
X_FUNC( DBSETINDEX )
{
   X_ARGCHECK( ctx, x_dbfString( x_paritem( ctx, 1 ) ) && x_parclen( ctx, 1 ) != 0, "DBSETINDEX" );

   x_dbfExistingIndex( ctx, 2, "DBSETINDEX" );
}

// Set the controlling order
// dbSetOrder( nOrderNum ) --> NIL
X_FUNC( DBSETORDER )
{
   DBF    area;
   X_ITEM previous __attribute__( ( cleanup( x_itemClear ) ) ) = X_ITEM_NIL_INIT;

   X_ARGCHECK( ctx, X_ISNUM( ctx, 1 ), "DBSETORDER" );

   area = x_dbfRequireArea( ctx, "DBSETORDER" );
   if( area != NULL )
      x_dbfRaise( ctx, x_dbfOrderFocus( area, x_paritem( ctx, 1 ), NULL, &previous ), "DBSETORDER" );

   x_ret( ctx );
}

// Relate two work areas
// dbSetRelation( nArea | cAlias, bExpr, cExpr ) --> NIL
X_FUNC( DBSETRELATION )
{
   int64_t number;

   X_ARGCHECK( ctx,
               x_dbfString( x_paritem( ctx, 1 ) ) ||
                  ( x_dbfInteger( x_paritem( ctx, 1 ), &number ) && number >= 0 && number <= X_DBF_MAX_AREA ),
               "DBSETRELATION" );
   X_ARGCHECK( ctx, X_ISBLOCK( ctx, 2 ), "DBSETRELATION" );
   X_ARGCHECK( ctx, x_dbfOptional( ctx, 3 ) || X_ISCHAR( ctx, 3 ), "DBSETRELATION" );
   X_ARGCHECK( ctx, x_dbfOptional( ctx, 4 ) || X_ISLOG( ctx, 4 ), "DBSETRELATION" );
   x_dbfSetRelation( ctx, 0, "DBSETRELATION" );
}

// Move relative to the current record
// dbSkip( [nRecords] ) --> NIL
X_FUNC( DBSKIP )
{
   int64_t records = 1;
   DBF     area;

   X_ARGCHECK( ctx, x_dbfOptional( ctx, 1 ) || x_dbfInteger( x_paritem( ctx, 1 ), &records ), "DBSKIP" );

   area = x_dbfRequireArea( ctx, "DBSKIP" );
   if( area == NULL )
      return;

   x_dbfRaise( ctx, x_dbfAreaSkip( area, records, x_setDeleted ), "DBSKIP" );

   x_ret( ctx );
}

// Move relative to the current record and return the number of records skipped
// dbSkipper( [nRecords] ) --> nSkipped
X_FUNC( DBSKIPPER )
{
   int64_t      request = 1, done = 0;
   X_DBF_STATUS status = X_DBF_OK;
   DBF          area;

   X_ARGCHECK( ctx, x_dbfOptional( ctx, 1 ) || x_dbfInteger( x_paritem( ctx, 1 ), &request ), "DBSKIPPER" );

   area = x_dbfCursorArea( ctx, "DBSKIPPER" );
   if( area == NULL )
   {
      x_retnint( ctx, 0 );
      return;
   }

   if( request == 0 || ( request > 0 && area->state.eof ) )
   {
      x_dbfRaise( ctx, x_dbfAreaSkip( area, 0, x_setDeleted ), "DBSKIPPER" );
      x_retnint( ctx, 0 );
      return;
   }

   while( status == X_DBF_OK && done < request )
   {
      status = x_dbfAreaSkip( area, 1, x_setDeleted );
      if( status == X_DBF_OK && area->state.eof )
      {
         status = x_dbfAreaSkip( area, -1, x_setDeleted );
         break;
      }
      if( status == X_DBF_OK )
         ++done;
   }

   while( status == X_DBF_OK && done > request )
   {
      status = x_dbfAreaSkip( area, -1, x_setDeleted );
      if( status != X_DBF_OK || area->state.bof )
         break;
      --done;
   }

   x_dbfRaise( ctx, status, "DBSKIPPER" );

   x_retnint( ctx, done );
}

// Copy records of the current work area, sorted, to a new database file
// DbSort( cFile, aFields, [bFor], [bWhile], [nNext], [nRecord], [lRest] ) --> lSuccess
X_FUNC( DBSORT )
{
   X_ARGCHECK( ctx, x_dbfString( x_paritem( ctx, 1 ) ) && x_parclen( ctx, 1 ) != 0, "DBSORT" );
   X_ARGCHECK( ctx, X_ISARRAY( ctx, 2 ) && x_arrayLen( x_paritem( ctx, 2 ) ) > 0, "DBSORT" );
   X_ARGCHECK( ctx, x_dbfScopeArgs( ctx, 3 ), "DBSORT" );

   x_dbfBulkSort( ctx );
}

// Create an array containing the structure of a database file
// dbStruct( ) --> aStruct
X_FUNC( DBSTRUCT )
{
   DBF    area = x_dbfCurrent();
   size_t i;

   x_reta( ctx, IIF( area != NULL, area->schema->count, 0 ) );

   if( area == NULL )
      return;
   for( i = 0; i < area->schema->count; ++i )
   {
      const X_DBF_FIELD *field = &area->schema->fields[i];
      X_ITEM            *row = x_arrayGetItemPtr( ctx->result, i + 1 );
      char               type = ( char )field->type;

      x_arrayNew( row, DBS_ALEN );
      x_arraySetC( row, DBS_NAME, field->name );
      x_arraySetCL( row, DBS_TYPE, &type, 1 );
      x_arraySetNI( row, DBS_LEN, field->length );
      x_arraySetNI( row, DBS_DEC, field->decimals );
   }
}

// Return the sum of an expression over the records in a scope
// DbSum( bExpression, [bFor], [bWhile], [nNext], [nRecord], [lRest] ) --> nSum
X_FUNC( DBSUM )
{
   X_ARGCHECK( ctx, X_ISBLOCK( ctx, 1 ), "DBSUM" );
   X_ARGCHECK( ctx, x_dbfScopeArgs( ctx, 2 ), "DBSUM" );

   x_dbfBulkTally( ctx, 1 );
}

// Summarize numeric fields of records with the same key into a new database file
// DbTotal( cFile, bKey, [aFields], [bFor], [bWhile], [nNext], [nRecord], [lRest] ) --> lSuccess
X_FUNC( DBTOTAL )
{
   X_ARGCHECK( ctx, x_dbfString( x_paritem( ctx, 1 ) ) && x_parclen( ctx, 1 ) != 0, "DBTOTAL" );
   X_ARGCHECK( ctx, x_dbfOptional( ctx, 2 ) || X_ISBLOCK( ctx, 2 ), "DBTOTAL" );
   X_ARGCHECK( ctx, x_dbfNameList( ctx, 3 ), "DBTOTAL" );
   X_ARGCHECK( ctx, x_dbfScopeArgs( ctx, 4 ), "DBTOTAL" );

   x_dbfBulkTotal( ctx );
}

// Release all locks for the current work area
// dbUnlock( ) --> NIL
X_FUNC( DBUNLOCK )
{
   DBF area = x_dbfRequireArea( ctx, "DBUNLOCK" );

   if( area == NULL )
      return;

   x_dbfRaise( ctx, x_dbfAreaUnlock( area, 0 ), "DBUNLOCK" );

   x_ret( ctx );
}

// Release all locks for all work areas
// dbUnlockAll( ) --> NIL
X_FUNC( DBUNLOCKALL )
{
   X_DBF_STATUS status = X_DBF_OK, next;
   DBF          area;

   for( area = x_dbfRuntimeSession()->areas; area != NULL; area = area->next )
   {
      next = x_dbfAreaUnlock( area, 0 );
      if( status == X_DBF_OK )
         status = next;
   }

   x_dbfRaise( ctx, status, "DBUNLOCKALL" );

   x_ret( ctx );
}

// Update records of the current work area from another work area
// DbUpdate( cAlias, bKey, lRandom, bAction ) --> lSuccess
X_FUNC( DBUPDATE )
{
   X_ARGCHECK( ctx, x_dbfString( x_paritem( ctx, 1 ) ) && x_parclen( ctx, 1 ) != 0, "DBUPDATE" );
   X_ARGCHECK( ctx, X_ISBLOCK( ctx, 2 ), "DBUPDATE" );
   X_ARGCHECK( ctx, x_dbfOptional( ctx, 3 ) || X_ISLOG( ctx, 3 ), "DBUPDATE" );
   X_ARGCHECK( ctx, X_ISBLOCK( ctx, 4 ), "DBUPDATE" );

   x_dbfBulkUpdate( ctx );
}

// Use a database file in a work area
// dbUseArea( [lNewArea], [cDriver], cName, [xcAlias], [lShared], [lReadonly], [nConnection], [aIndexBindings] ) --> NIL
X_FUNC( DBUSEAREA )
{
   X_DBF_TEXT         path __attribute__( ( cleanup( x_dbfTextDrop ) ) ) = { NULL, 0 };
   X_DBF_TEXT         alias __attribute__( ( cleanup( x_dbfTextDrop ) ) ) = { NULL, 0 };
   X_DBF_OPEN_OPTIONS options;
   X_DBF_STATUS       status;
   DBF                previous, opened;
   int64_t            connection = 0;

   X_ARGCHECK( ctx, x_dbfOptional( ctx, 1 ) || X_ISLOG( ctx, 1 ), "DBUSEAREA" );
   X_ARGCHECK( ctx, x_dbfOptional( ctx, 2 ) || x_dbfString( x_paritem( ctx, 2 ) ), "DBUSEAREA" );
   X_ARGCHECK( ctx, x_dbfString( x_paritem( ctx, 3 ) ) && x_parclen( ctx, 3 ) != 0, "DBUSEAREA" );
   X_ARGCHECK( ctx, x_dbfOptional( ctx, 4 ) || x_dbfString( x_paritem( ctx, 4 ) ), "DBUSEAREA" );
   X_ARGCHECK( ctx, x_dbfOptional( ctx, 5 ) || X_ISLOG( ctx, 5 ), "DBUSEAREA" );
   X_ARGCHECK( ctx, x_dbfOptional( ctx, 6 ) || X_ISLOG( ctx, 6 ), "DBUSEAREA" );
   X_ARGCHECK( ctx, x_dbfOptional( ctx, 7 ) || ( x_dbfInteger( x_paritem( ctx, 7 ), &connection ) && connection == 0 ),
               "DBUSEAREA" );
   X_ARGCHECK( ctx, x_dbfOptional( ctx, 8 ) || X_ISARRAY( ctx, 8 ), "DBUSEAREA" );

   status = x_dbfTextValidateUTF8( x_parc( ctx, 3 ), x_parclen( ctx, 3 ) );
   if( status != X_DBF_OK )
   {
      x_dbfRaise( ctx, status, "DBUSEAREA" );
      return;
   }

   memset( &options, 0, sizeof( options ) );

   options.driver = IIF( x_parclen( ctx, 2 ) != 0, x_parc( ctx, 2 ), x_dbfDefaultDriver() );
   if( x_dbfDriverFind( x_dbfRuntimeSession(), options.driver ) == NULL )
   {
      x_dbfRaise( ctx, X_DBF_DRIVER_NOT_FOUND, "DBUSEAREA" );
      return;
   }

   path.data = x_dbfOpenPath( x_parc( ctx, 3 ) );
   alias.data = IIF( x_parclen( ctx, 4 ) != 0, x_strdup( x_parc( ctx, 4 ) ), x_dbfAliasName( path.data ) );
   options.path = path.data;
   options.alias = alias.data;
   options.area = IIF( x_parl( ctx, 1 ), 0, x_dbfSessionSelected( x_dbfRuntimeSession() ) );
   options.readOnly = x_parl( ctx, 6 );
   options.shared = x_parldef( ctx, 5, !x_setExclusive );
   options.indexBindings = IIF( x_dbfOptional( ctx, 8 ), NULL, x_paritem( ctx, 8 ) );

   x_dbfNetErrorSet( 0 );

   previous = IIF( options.area != 0, x_dbfSessionArea( x_dbfRuntimeSession(), options.area ), NULL );
   if( previous != NULL )
   {
      status = x_dbfAreaClose( previous );
      if( status != X_DBF_OK )
      {
         x_dbfRaise( ctx, status, "DBUSEAREA" );
         return;
      }
   }

   status = x_dbfAreaOpen( x_dbfRuntimeSession(), &options, &opened );
   if( status == X_DBF_OK && x_setDeleted && opened->state.deleted )
   {
      status = x_dbfAreaGoTop( opened, 1 );
      if( status != X_DBF_OK )
         x_dbfAreaDiscard( opened );
   }
   if( x_dbfContention( status ) )
      x_dbfNetErrorSet( 1 );
   else if( status != X_DBF_OK )
   {
      x_dbfRaise( ctx, status, "DBUSEAREA" );
      return;
   }
   x_retl( ctx, status == X_DBF_OK );
}

// Remove all records from the current database file
// dbZap( ) --> NIL
X_FUNC( DBZAP )
{
   x_dbfReorganize( ctx, 0, "DBZAP" );
}

// Return the deleted status of the current record
// Deleted( ) --> lDeleted
X_FUNC( DELETED )
{
   DBF area = x_dbfCursorArea( ctx, "DELETED" );

   x_retl( ctx, area != NULL && area->state.deleted );
}

// Determine when end of file is encountered
// Eof( ) --> lBoundary
X_FUNC_ALIAS( EOF, XDEV_DBF_EOF )
{
   DBF area = x_dbfCursorArea( ctx, "EOF" );

   x_retl( ctx, area == NULL || area->state.eof );
}

// Return the number of fields in the current .dbf file
// FCount( ) --> nFields
X_FUNC( FCOUNT )
{
   DBF area = x_dbfCurrent();
   x_retnint( ctx, IIF( area != NULL, ( int64_t )area->schema->count, 0 ) );
}

// Return a set-get code block for a given field
// FieldBlock( cFieldName ) --> bFieldBlock
X_FUNC( FIELDBLOCK )
{
   x_dbfMakeFieldBlock( ctx, 0 );
}

// Retrieve the value of a field using the ordinal position of the field in the database structure
// FieldGet( nField ) --> ValueField
X_FUNC( FIELDGET )
{
   int64_t ordinal = 0;
   DBF     area;

   x_dbfInteger( x_paritem( ctx, 1 ), &ordinal );
   if( ordinal <= 0 )
   {
      x_ret( ctx );
      return;
   }

   area = x_dbfCursorArea( ctx, "FIELDGET" );
   if( area == NULL || ( uint64_t )ordinal > area->schema->count )
   {
      x_ret( ctx );
      return;
   }

   x_dbfRaise( ctx, x_dbfFieldGetValue( area, ( size_t )ordinal, ctx->result ), "FIELDGET" );
}

// Return a field name from the current database (.dbf) file
// FieldName( nPosition ) --> cFieldName
X_FUNC( FIELDNAME )
{
   int64_t            ordinal = 0;
   DBF                area;
   const X_DBF_FIELD *field;

   x_dbfInteger( x_paritem( ctx, 1 ), &ordinal );

   area = x_dbfCurrent();
   field = IIF( area != NULL && ordinal > 0 && ( uint64_t )ordinal <= area->schema->count,
                x_dbfSchemaField( area->schema, ( size_t )ordinal ), NULL );

   x_retc( ctx, IIF( field != NULL, field->name, "" ) );
}

// Return the position of a field in a work area
// FieldPos( cFieldName ) --> nFieldPos
X_FUNC( FIELDPOS )
{
   DBF area;

   area = x_dbfCurrent();

   x_retns( ctx, IIF( area != NULL && x_dbfString( x_paritem( ctx, 1 ) ),
                      ( ptrdiff_t )x_dbfSchemaFindField( area->schema, x_parc( ctx, 1 ) ), 0 ) );
}

// Set the value of a field variable using the ordinal position of the field in the database structure
// FieldPut( nField, expAssign ) --> ValueAssigned
X_FUNC( FIELDPUT )
{
   int64_t ordinal = 0;
   DBF     area;

   x_dbfInteger( x_paritem( ctx, 1 ), &ordinal );

   if( ordinal <= 0 || x_dbfOptional( ctx, 2 ) )
   {
      x_ret( ctx );
      return;
   }

   area = x_dbfCursorArea( ctx, "FIELDPUT" );
   if( area == NULL || ( uint64_t )ordinal > area->schema->count )
   {
      x_ret( ctx );
      return;
   }

   x_dbfPutFieldValue( ctx, area, ( size_t )ordinal, x_paritem( ctx, 2 ), "FIELDPUT" );
}

// Return a set-get code block for a field in a given work area
// FieldWBlock( cFieldName, nWorkArea ) --> bFieldWBlock
X_FUNC( FIELDWBLOCK )
{
   x_dbfMakeFieldBlock( ctx, 1 );
}

// Return a field value as XML Schema text without XML escaping
// FieldXml( cFieldName | nFieldPos ) --> cValue
X_FUNC( FIELDXML )
{
   X_ITEM             value __attribute__( ( cleanup( x_itemClear ) ) ) = X_ITEM_NIL_INIT;
   DBF                area = x_dbfRequireArea( ctx, "FIELDXML" );
   const X_DBF_FIELD *field;
   const char        *data;
   char               text[64];
   int64_t            ordinal = 0;
   size_t             start = 0, length;
   int                year, month, day;
   X_DBF_STATUS       status;

   if( area == NULL )
      return;

   if( x_dbfString( x_paritem( ctx, 1 ) ) )
      ordinal = ( int64_t )x_dbfSchemaFindField( area->schema, x_parc( ctx, 1 ) );
   else
      x_dbfInteger( x_paritem( ctx, 1 ), &ordinal );

   if( ordinal <= 0 || ( uint64_t )ordinal > area->schema->count )
   {
      x_dbfRaise( ctx, X_DBF_INVALID_ARGUMENT, "FIELDXML" );
      return;
   }

   status = x_dbfFieldGetValue( area, ( size_t )ordinal, &value );
   if( status != X_DBF_OK )
   {
      x_dbfRaise( ctx, status, "FIELDXML" );
      return;
   }

   field = &area->schema->fields[ordinal - 1];
   data = area->record + field->offset;

   switch( field->type )
   {
      case X_DBF_CHARACTER:
         length = x_itemGetCLen( &value );
         while( length != 0 && x_itemGetCPtr( &value )[length - 1] == ' ' )
            --length;
         x_retclen( ctx, x_itemGetCPtr( &value ), length );
         return;
      case X_DBF_NUMERIC:
         length = field->length;
         while( start < length && data[start] == ' ' )
            ++start;
         while( length > start && data[length - 1] == ' ' )
            --length;
         if( x_dbfDecimalText( data + start, length - start, field->decimals ) )
         {
            x_retclen( ctx, data + start, length - start );
            return;
         }
         if( field->decimals == 0 && ( value.nType & X_IT_INTEGRAL ) != 0 )
            snprintf( text, sizeof( text ), "%lld", ( long long )x_itemGetNInt( &value ) );
         else
            snprintf( text, sizeof( text ), "%.*f", ( int )field->decimals, x_itemGetND( &value ) );
         x_retc( ctx, text );
         return;
      case X_DBF_DATE:
         if( value.u.integer == 0 )
         {
            x_retc( ctx, "" );
            return;
         }
         x_dateDecode( ( long )value.u.integer, &year, &month, &day );
         snprintf( text, sizeof( text ), "%04d-%02d-%02d", year, month, day );
         x_retc( ctx, text );
         return;
      case X_DBF_LOGICAL: x_retc( ctx, x_itemGetL( &value ) ? "true" : "false" ); return;
      default:
         if( value.nType != X_IT_STRING )
         {
            x_dbfRaise( ctx, X_DBF_INVALID_FIELD_VALUE, "FIELDXML" );
            return;
         }
         x_itemReturn( ctx, &value );
         return;
   }
}

// Lock an open and shared database file
// FLock( ) --> lSuccess
X_FUNC( FLOCK )
{
   x_dbfLock( ctx, "FLOCK", 1, 0 );
}

// Determine if the previous search operation succeeded
// Found( ) --> lSuccess
X_FUNC( FOUND )
{
   DBF area = x_dbfCursorArea( ctx, "FOUND" );

   x_retl( ctx, area != NULL && area->state.found );
}

// Return the current database file header length
// Header( ) --> nBytes
X_FUNC( HEADER )
{
   DBF area = x_dbfCurrent();
   x_retnint( ctx, 0 );

   if( area != NULL )
      x_dbfRaise( ctx, x_dbfAreaInfo( area, DBI_GETHEADERSIZE, ctx->result ), "HEADER" );
}

// Return the default index extension based on the database driver currently linked
// IndexExt( ) --> cExtension
X_FUNC( INDEXEXT )
{
   x_dbfOrderExtension( ctx, "INDEXEXT" );
}

// Return the key expression of a specified index
// IndexKey( nOrder ) --> cKeyExp
X_FUNC( INDEXKEY )
{
   if( x_dbfCurrent() == NULL )
      x_retc( ctx, "" );
   else
   {
      int64_t number;

      X_ARGCHECK_OPT( ctx, 1,
                      X_ISNIL( ctx, 1 ) || x_dbfInteger( x_paritem( ctx, 1 ), &number ) ||
                         x_dbfString( x_paritem( ctx, 1 ) ),
                      "INDEXKEY" );
      X_ARGCHECK_OPT( ctx, 2, X_ISNIL( ctx, 2 ) || x_dbfString( x_paritem( ctx, 2 ) ), "INDEXKEY" );

      x_dbfOrderInfoApi( ctx, DBOI_EXPRESSION, NULL, "INDEXKEY" );
   }
}

// Return the order position of the controlling index
// IndexOrd( ) --> nOrder
X_FUNC( INDEXORD )
{
   if( x_dbfCurrent() == NULL )
      x_retni( ctx, 0 );
   else
   {
      X_ARGCHECK_OPT( ctx, 1, X_ISNIL( ctx, 1 ) || x_dbfString( x_paritem( ctx, 1 ) ), "INDEXORD" );
      X_ARGCHECK_OPT( ctx, 2, X_ISNIL( ctx, 2 ) || x_dbfString( x_paritem( ctx, 2 ) ), "INDEXORD" );
      x_dbfOrderInfoApi( ctx, DBOI_NUMBER, NULL, "INDEXORD" );
   }
}

// Determine the number of records in the current .dbf file
// LastRec( ) --> nRecords
X_FUNC( LASTREC )
{
   x_dbfRecordCount( ctx, "LASTREC" );
}

// Return the last modification date of a database (.dbf) file
// LUpdate( ) --> dModification
X_FUNC( LUPDATE )
{
   DBF area = x_dbfCurrent();

   x_retdl( ctx, 0 );

   if( area != NULL )
      x_dbfRaise( ctx, x_dbfAreaInfo( area, DBI_LASTUPDATE, ctx->result ), "LUPDATE" );
}

// Set an RDD inheritance chain for the DBFMEMO database driver
// MEMOSETSUPER( [cSuperRDD] ) --> cOldSuperName
X_FUNC( MEMOSETSUPER )
{
   x_errorRaiseNamed( ctx->result, 1900, "MEMOSETSUPER" );
}

// Determine if a network command has failed
// NetErr( [lNewError] ) --> lError
X_FUNC( NETERR )
{
   X_ARGCHECK( ctx, x_dbfOptional( ctx, 1 ) || X_ISLOG( ctx, 1 ), "NETERR" );

   x_retl( ctx, x_dbfNetError() );

   if( X_ISLOG( ctx, 1 ) )
      x_dbfNetErrorSet( x_parl( ctx, 1 ) );
}

// Return the default order bag RDD extension
// ordBagExt( ) --> cBagExt
X_FUNC( ORDBAGEXT )
{
   x_dbfOrderExtension( ctx, "ORDBAGEXT" );
}

// Return the order bag name of a specific order
// ordBagName( nOrder | cOrderName ) --> cOrderBagName
X_FUNC( ORDBAGNAME )
{
   int64_t number;

   X_ARGCHECK_OPT( ctx, 1,
                   X_ISNIL( ctx, 1 ) || x_dbfInteger( x_paritem( ctx, 1 ), &number ) ||
                      x_dbfString( x_paritem( ctx, 1 ) ),
                   "ORDBAGNAME" );
   X_ARGCHECK_OPT( ctx, 2, X_ISNIL( ctx, 2 ) || x_dbfString( x_paritem( ctx, 2 ) ), "ORDBAGNAME" );

   x_dbfOrderInfoApi( ctx, DBOI_BAGNAME, NULL, "ORDBAGNAME" );
}

// Set the condition and scope for an order
// ordCondSet( [cForCondition], [bForCondition], [lAll], [bWhileCondition], [bEval], [nInterval], [nStart], [nNext], [nRecord], [lRest], [lDescend], [lCompound], [lAdditive], [lCurrent], [lCustom], [lNoOptimize], [cWhileCondition], [lTemporary], [lUseFilter], [lExclusive] ) --> lSuccess
X_FUNC( ORDCONDSET )
{
   DBF area = x_dbfCurrent();
   int argument;

   if( area == NULL )
   {
      x_retl( ctx, 0 );
      return;
   }

   X_ARGCHECK_OPT( ctx, 1, X_ISNIL( ctx, 1 ) || x_dbfString( x_paritem( ctx, 1 ) ), "ORDCONDSET" );
   X_ARGCHECK_OPT( ctx, 2, X_ISNIL( ctx, 2 ) || X_ISBLOCK( ctx, 2 ), "ORDCONDSET" );

   if( area->indexBusy )
   {
      x_dbfRaise( ctx, X_DBF_INVALID_STATE, "ORDCONDSET" );
      return;
   }

   for( argument = 3; argument <= x_pcount( ctx ); ++argument )
   {
      int     logical = argument == 3 || ( argument >= 10 && argument <= 16 ) || ( argument >= 18 && argument <= 20 );
      int     numeric = argument >= 6 && argument <= 9;
      int64_t number;
      if( x_dbfOptional( ctx, argument ) )
         continue;
      if( logical )
      {
         X_ARGCHECK( ctx, X_ISLOG( ctx, argument ), "ORDCONDSET" );
      }
      else if( numeric )
      {
         X_ARGCHECK( ctx, x_dbfInteger( x_paritem( ctx, argument ), &number ) && number >= 0, "ORDCONDSET" );
      }
      else if( argument == 4 || argument == 5 )
      {
         X_ARGCHECK( ctx, X_ISBLOCK( ctx, argument ), "ORDCONDSET" );
      }
      else if( argument == 17 )
      {
         X_ARGCHECK( ctx, x_dbfString( x_paritem( ctx, argument ) ), "ORDCONDSET" );
      }
      else
      {
         x_dbfRaise( ctx, X_DBF_UNSUPPORTED_OPERATION, "ORDCONDSET" );
         return;
      }
   }

   if( !x_dbfOptional( ctx, 17 ) && x_parclen( ctx, 17 ) != 0 && !X_ISBLOCK( ctx, 4 ) )
   {
      x_dbfRaise( ctx, X_DBF_INVALID_ARGUMENT, "ORDCONDSET" );
      return;
   }

   if( !x_dbfOptional( ctx, 1 ) && x_parclen( ctx, 1 ) != 0 && !X_ISBLOCK( ctx, 2 ) )
   {
      x_dbfRaise( ctx, X_DBF_INVALID_ARGUMENT, "ORDCONDSET" );
      return;
   }

   x_itemClear( &area->indexCondition );
   x_itemClear( &area->indexConditionText );

   if( !x_dbfOptional( ctx, 2 ) )
      x_itemCopy( &area->indexCondition, x_paritem( ctx, 2 ) );

   if( !x_dbfOptional( ctx, 1 ) )
      x_itemCopy( &area->indexConditionText, x_paritem( ctx, 1 ) );

   x_dbfBuildDrop( &area->indexBuild );

   if( X_ISBLOCK( ctx, 4 ) )
      x_itemCopy( &area->indexBuild.whileBlock, x_paritem( ctx, 4 ) );

   if( X_ISBLOCK( ctx, 5 ) )
      x_itemCopy( &area->indexBuild.evalBlock, x_paritem( ctx, 5 ) );

   area->indexBuild.step = ( uint64_t )x_parnint( ctx, 6 );
   area->indexBuild.start = ( uint64_t )x_parnint( ctx, 7 );
   area->indexBuild.next = ( uint64_t )x_parnint( ctx, 8 );
   area->indexBuild.record = ( uint64_t )x_parnint( ctx, 9 );
   area->indexBuild.rest = x_parl( ctx, 10 ) || X_ISBLOCK( ctx, 4 );
   area->indexBuild.current = x_parl( ctx, 14 );
   area->indexBuild.custom = x_parl( ctx, 15 );
   area->indexBuild.compound = x_parl( ctx, 12 );
   area->indexBuild.temporary = x_parl( ctx, 18 );
   area->indexBuild.useFilter =
      x_parl( ctx, 19 ) && !area->indexBuild.rest && area->indexBuild.next == 0 && area->indexBuild.record == 0;
   area->indexBuild.exclusive = x_parl( ctx, 20 );
   area->indexBuild.scoped = ( !x_dbfOptional( ctx, 3 ) && !x_parl( ctx, 3 ) ) || area->indexBuild.rest ||
                             area->indexBuild.next != 0 || area->indexBuild.record != 0 || area->indexBuild.current ||
                             area->indexBuild.useFilter;
   area->indexDescending = x_parl( ctx, 11 );
   area->indexAdditive = x_parl( ctx, 13 );

   x_retl( ctx, 1 );
}

// Create an order in an order bag
// ordCreate( cOrderBagName, [cOrderName], cExpKey, bExpKey, [lUnique] ) --> NIL
X_FUNC( ORDCREATE )
{
   X_ARGCHECK( ctx, x_dbfOptional( ctx, 1 ) || x_dbfString( x_paritem( ctx, 1 ) ), "ORDCREATE" );
   X_ARGCHECK( ctx, x_dbfString( x_paritem( ctx, 3 ) ), "ORDCREATE" );
   X_ARGCHECK( ctx, X_ISBLOCK( ctx, 4 ), "ORDCREATE" );
   X_ARGCHECK_OPT( ctx, 5, X_ISNIL( ctx, 5 ) || X_ISLOG( ctx, 5 ), "ORDCREATE" );
   X_ARGCHECK_OPT( ctx, 2, X_ISNIL( ctx, 2 ) || x_dbfString( x_paritem( ctx, 2 ) ), "ORDCREATE" );

   x_dbfCreateIndex( ctx, 1, "ORDCREATE" );
}

// Return and optionally change the descending flag of an order
// ordDescend( [cOrder | nPosition], [cIndexFile], [lNewDescend] ) --> lCurrentDescend
X_FUNC( ORDDESCEND )
{
   int64_t number;
   X_ARGCHECK_OPT( ctx, 1,
                   X_ISNIL( ctx, 1 ) || x_dbfInteger( x_paritem( ctx, 1 ), &number ) ||
                      x_dbfString( x_paritem( ctx, 1 ) ),
                   "ORDDESCEND" );
   X_ARGCHECK_OPT( ctx, 2, X_ISNIL( ctx, 2 ) || x_dbfString( x_paritem( ctx, 2 ) ), "ORDDESCEND" );
   X_ARGCHECK_OPT( ctx, 3, X_ISNIL( ctx, 3 ) || X_ISLOG( ctx, 3 ), "ORDDESCEND" );

   x_dbfOrderInfoApi( ctx, DBOI_ISDESC, x_paritem( ctx, 3 ), "ORDDESCEND" );
}

// Remove a specified order from an order bag
// ordDestroy( cOrderName, [cOrderBagName] ) --> NIL
X_FUNC( ORDDESTROY )
{
   DBF          area = x_dbfCurrent();
   X_DBF_STATUS status;

   if( area == NULL )
   {
      x_retl( ctx, 0 );
      return;
   }

   X_ARGCHECK( ctx, X_ISCHAR( ctx, 1 ) || X_ISNUM( ctx, 1 ), "ORDDESTROY" );
   X_ARGCHECK_OPT( ctx, 2, X_ISNIL( ctx, 2 ) || x_dbfString( x_paritem( ctx, 2 ) ), "ORDDESTROY" );

   status = x_dbfOrderDestroy( area, x_paritem( ctx, 1 ), IIF( x_dbfOptional( ctx, 2 ), NULL, x_parc( ctx, 2 ) ) );

   x_dbfRaise( ctx, status, "ORDDESTROY" );

   x_retl( ctx, status == X_DBF_OK );
}

// Return the FOR expression of an order
// ordFor( cOrderName | nOrder, [cOrderBagName] ) --> cForExp
X_FUNC( ORDFOR )
{
   int64_t number;

   X_ARGCHECK_OPT( ctx, 1,
                   X_ISNIL( ctx, 1 ) || x_dbfInteger( x_paritem( ctx, 1 ), &number ) ||
                      x_dbfString( x_paritem( ctx, 1 ) ),
                   "ORDFOR" );

   X_ARGCHECK_OPT( ctx, 2, X_ISNIL( ctx, 2 ) || x_dbfString( x_paritem( ctx, 2 ) ), "ORDFOR" );

   x_dbfOrderInfoApi( ctx, DBOI_CONDITION, NULL, "ORDFOR" );
}

// Return the status of the unique flag for a given order
// ordIsUnique( [cOrder | nPosition], [cIndexFile] ) --> lUnique
X_FUNC( ORDISUNIQUE )
{
   int64_t number;
   X_ARGCHECK_OPT( ctx, 1,
                   X_ISNIL( ctx, 1 ) || x_dbfInteger( x_paritem( ctx, 1 ), &number ) ||
                      x_dbfString( x_paritem( ctx, 1 ) ),
                   "ORDISUNIQUE" );
   X_ARGCHECK_OPT( ctx, 2, X_ISNIL( ctx, 2 ) || x_dbfString( x_paritem( ctx, 2 ) ), "ORDISUNIQUE" );
   X_ARGCHECK_OPT( ctx, 3, X_ISNIL( ctx, 3 ) || X_ISLOG( ctx, 3 ), "ORDISUNIQUE" );

   x_dbfOrderInfoApi( ctx, DBOI_UNIQUE, x_paritem( ctx, 3 ), "ORDISUNIQUE" );
}

// Return the key expression of an order
// ordKey( cOrderName | nOrder, [cOrderBagName] ) --> cExpKey
X_FUNC( ORDKEY )
{
   int64_t number;

   X_ARGCHECK_OPT( ctx, 1,
                   X_ISNIL( ctx, 1 ) || x_dbfInteger( x_paritem( ctx, 1 ), &number ) ||
                      x_dbfString( x_paritem( ctx, 1 ) ),
                   "ORDKEY" );
   X_ARGCHECK_OPT( ctx, 2, X_ISNIL( ctx, 2 ) || x_dbfString( x_paritem( ctx, 2 ) ), "ORDKEY" );

   x_dbfOrderInfoApi( ctx, DBOI_EXPRESSION, NULL, "ORDKEY" );
}

// Add a key to a custom built order
// ordKeyAdd( [cOrder | nPosition], [cIndexFile],[expKeyValue] ) --> lSuccess
X_FUNC( ORDKEYADD )
{
   x_errorRaiseNamed( ctx->result, 1900, "ORDKEYADD" );
}

// Return the number of keys in an order
// ordKeyCount( [cOrder | nPosition], [cIndexFile] ) --> nKeys
X_FUNC( ORDKEYCOUNT )
{
   int64_t number;

   X_ARGCHECK_OPT( ctx, 1,
                   X_ISNIL( ctx, 1 ) || x_dbfInteger( x_paritem( ctx, 1 ), &number ) ||
                      x_dbfString( x_paritem( ctx, 1 ) ),
                   "ORDKEYCOUNT" );
   X_ARGCHECK_OPT( ctx, 2, X_ISNIL( ctx, 2 ) || x_dbfString( x_paritem( ctx, 2 ) ), "ORDKEYCOUNT" );

   x_dbfOrderInfoApi( ctx, DBOI_KEYCOUNT, NULL, "ORDKEYCOUNT" );
}

// Delete a key from a custom built order
// ordKeyDel( [cOrder | nPosition], [cIndexFile], [expKeyValue] ) --> lSuccess
X_FUNC( ORDKEYDEL )
{
   x_errorRaiseNamed( ctx->result, 1900, "ORDKEYDEL" );
}

// Move to a record specified by its logical record number
// ordKeyGoto( nKeyNo ) --> lSuccess
X_FUNC( ORDKEYGOTO )
{
   x_errorRaiseNamed( ctx->result, 1900, "ORDKEYGOTO" );
}

// Get the logical record number of the current record
// ordKeyNo( [cOrder | nPosition], [cIndexFile] ) --> nKeyNo
X_FUNC( ORDKEYNO )
{
   int64_t number;

   X_ARGCHECK_OPT( ctx, 1,
                   X_ISNIL( ctx, 1 ) || x_dbfInteger( x_paritem( ctx, 1 ), &number ) ||
                      x_dbfString( x_paritem( ctx, 1 ) ),
                   "ORDKEYNO" );

   X_ARGCHECK_OPT( ctx, 2, X_ISNIL( ctx, 2 ) || x_dbfString( x_paritem( ctx, 2 ) ), "ORDKEYNO" );

   x_dbfOrderInfoApi( ctx, DBOI_POSITION, NULL, "ORDKEYNO" );
}

// Get the key value of the current record from the controlling order
// ordKeyVal( ) --> uKeyValue
X_FUNC( ORDKEYVAL )
{
   int64_t number;

   X_ARGCHECK_OPT( ctx, 1,
                   X_ISNIL( ctx, 1 ) || x_dbfInteger( x_paritem( ctx, 1 ), &number ) ||
                      x_dbfString( x_paritem( ctx, 1 ) ),
                   "ORDKEYVAL" );

   X_ARGCHECK_OPT( ctx, 2, X_ISNIL( ctx, 2 ) || x_dbfString( x_paritem( ctx, 2 ) ), "ORDKEYVAL" );

   x_dbfOrderInfoApi( ctx, DBOI_KEYVAL, NULL, "ORDKEYVAL" );
}

// Add orders to the order list
// ordListAdd( cOrderBagName, [cOrderName] ) --> NIL
X_FUNC( ORDLISTADD )
{
   X_ARGCHECK( ctx, x_dbfString( x_paritem( ctx, 1 ) ) && x_parclen( ctx, 1 ) != 0, "ORDLISTADD" );
   X_ARGCHECK_OPT( ctx, 2, X_ISNIL( ctx, 2 ) || X_ISNUM( ctx, 2 ) || x_dbfString( x_paritem( ctx, 2 ) ), "ORDLISTADD" );

   x_dbfExistingIndex( ctx, 3, "ORDLISTADD" );
}

// Clear the current order list
// ordListClear( ) --> NIL
X_FUNC( ORDLISTCLEAR )
{
   x_dbfClearIndexes( ctx, "ORDLISTCLEAR" );
}

// Rebuild all orders in the order list of the current work area
// ordListRebuild( ) --> NIL
X_FUNC( ORDLISTREBUILD )
{
   x_dbfRebuildIndexes( ctx, "ORDLISTREBUILD" );
}

// Return the name of an order in the order list
// ordName( nOrder, [cOrderBagName] ) --> cOrderName
X_FUNC( ORDNAME )
{
   int64_t number;

   X_ARGCHECK_OPT( ctx, 1, X_ISNIL( ctx, 1 ) || x_dbfInteger( x_paritem( ctx, 1 ), &number ), "ORDNAME" );
   X_ARGCHECK_OPT( ctx, 2, X_ISNIL( ctx, 2 ) || x_dbfString( x_paritem( ctx, 2 ) ), "ORDNAME" );

   x_dbfOrderInfoApi( ctx, DBOI_NAME, NULL, "ORDNAME" );
}

// Return the position of an order in the current order list
// ordNumber( cOrderName, [cOrderBagName] ) --> nOrderNo
X_FUNC( ORDNUMBER )
{
   X_ARGCHECK_OPT( ctx, 1, X_ISNIL( ctx, 1 ) || x_dbfString( x_paritem( ctx, 1 ) ), "ORDNUMBER" );
   X_ARGCHECK_OPT( ctx, 2, X_ISNIL( ctx, 2 ) || x_dbfString( x_paritem( ctx, 2 ) ), "ORDNUMBER" );

   x_dbfOrderInfoApi( ctx, DBOI_NUMBER, NULL, "ORDNUMBER" );
}

// Set or clear the boundaries for scoping key values in the controlling order
// ordScope( nScope, [expNewValue] ) --> uCurrentValue
X_FUNC( ORDSCOPE )
{
   DBF area;

   X_ARGCHECK( ctx, X_ISNUM( ctx, 1 ), "ORDSCOPE" );

   area = x_dbfRequireArea( ctx, "ORDSCOPE" );
   if( area != NULL )
      x_dbfRaise( ctx,
                  x_dbfOrderScope( area, x_parni( ctx, 1 ), x_paritem( ctx, 2 ), x_pcount( ctx ) >= 2, ctx->result ),
                  "ORDSCOPE" );
}

// Set focus to an order in an order list
// ordSetFocus( [cOrderName | nOrder], [cOrderBagName] ) --> cPrevOrderNameInFocus
X_FUNC( ORDSETFOCUS )
{
   DBF area;

   X_ARGCHECK_OPT( ctx, 1, X_ISNIL( ctx, 1 ) || X_ISNUM( ctx, 1 ) || x_dbfString( x_paritem( ctx, 1 ) ),
                   "ORDSETFOCUS" );
   X_ARGCHECK_OPT( ctx, 2, X_ISNIL( ctx, 2 ) || x_dbfString( x_paritem( ctx, 2 ) ), "ORDSETFOCUS" );

   area = x_dbfRequireArea( ctx, "ORDSETFOCUS" );
   if( area != NULL )
      x_dbfRaise( ctx,
                  x_dbfOrderFocus( area, x_paritem( ctx, 1 ), IIF( x_dbfOptional( ctx, 2 ), NULL, x_parc( ctx, 2 ) ),
                                   ctx->result ),
                  "ORDSETFOCUS" );
}

// Relate two work areas and scope the child order to the parent key
// ordSetRelation( nArea | cAlias, bExpr, [cExpr], [lScoped] ) --> NIL
X_FUNC( ORDSETRELATION )
{
   int64_t number;

   X_ARGCHECK( ctx,
               x_dbfString( x_paritem( ctx, 1 ) ) ||
                  ( x_dbfInteger( x_paritem( ctx, 1 ), &number ) && number >= 0 && number <= X_DBF_MAX_AREA ),
               "ORDSETRELATION" );
   X_ARGCHECK( ctx, X_ISBLOCK( ctx, 2 ), "ORDSETRELATION" );
   X_ARGCHECK( ctx, x_dbfOptional( ctx, 3 ) || X_ISCHAR( ctx, 3 ), "ORDSETRELATION" );
   X_ARGCHECK( ctx, x_dbfOptional( ctx, 4 ) || X_ISLOG( ctx, 4 ), "ORDSETRELATION" );

   x_dbfSetRelation( ctx, 1, "ORDSETRELATION" );
}

// Move record pointer to the next or previous unique key
// ordSkipUnique( [nDirection] ) --> lSuccess
X_FUNC( ORDSKIPUNIQUE )
{
   x_errorRaiseNamed( ctx->result, 1900, "ORDSKIPUNIQUE" );
}

// Return or change information about a database driver
// rddInfo( nInfoType, [expNewSetting], [cDriver], [nConnection] ) --> uCurrentSetting
X_FUNC( RDDINFO )
{
   int64_t             selector, scheme = 0, connection;
   X_DBF_DRIVER_ENTRY *driver;
   const char         *name;

   X_ARGCHECK( ctx, x_dbfInteger( x_paritem( ctx, 1 ), &selector ) && selector >= INT32_MIN && selector <= INT32_MAX,
               "RDDINFO" );
   X_ARGCHECK_OPT( ctx, 3, X_ISNIL( ctx, 3 ) || x_dbfString( x_paritem( ctx, 3 ) ), "RDDINFO" );
   X_ARGCHECK( ctx, x_dbfOptional( ctx, 4 ) || ( x_dbfInteger( x_paritem( ctx, 4 ), &connection ) && connection == 0 ),
               "RDDINFO" );

   name = IIF( x_parclen( ctx, 3 ) != 0, x_parc( ctx, 3 ), x_dbfDefaultDriver() );
   driver = x_dbfDriverFind( x_dbfRuntimeSession(), name );

   if( driver == NULL )
   {
      x_dbfRaise( ctx, X_DBF_DRIVER_NOT_FOUND, "RDDINFO" );
      return;
   }

   if( selector == RDDI_LOCKSCHEME )
   {
      X_ARGCHECK( ctx,
                  x_dbfOptional( ctx, 2 ) || ( x_dbfInteger( x_paritem( ctx, 2 ), &scheme ) &&
                                               scheme >= DB_DBFLOCK_CLIPPER && scheme <= DB_DBFLOCK_CLIPPER2 ),
                  "RDDINFO" );
      x_retni( ctx, driver->lockScheme );
      if( !x_dbfOptional( ctx, 2 ) )
         driver->lockScheme = ( int )scheme;
      return;
   }

   if( selector == RDDI_SETHEADER )
   {
      int64_t mode;
      X_ARGCHECK( ctx, x_dbfOptional( ctx, 2 ) || x_dbfSetHeaderMode( x_paritem( ctx, 2 ), &mode ), "RDDINFO" );
      x_retni( ctx, driver->setHeader );
      if( !x_dbfOptional( ctx, 2 ) )
         driver->setHeader = ( int )mode;
      return;
   }

   if( selector == RDDI_LOCKRETRY )
   {
      int64_t retry;
      X_ARGCHECK( ctx,
                  x_dbfOptional( ctx, 2 ) ||
                     ( x_dbfInteger( x_paritem( ctx, 2 ), &retry ) && retry >= 0 && retry <= INT32_MAX ),
                  "RDDINFO" );
      x_retni( ctx, driver->lockRetry );
      if( !x_dbfOptional( ctx, 2 ) )
         driver->lockRetry = ( int )retry;
      return;
   }

   if( !x_dbfOptional( ctx, 2 ) )
   {
      x_dbfRaise( ctx, X_DBF_UNSUPPORTED_OPERATION, "RDDINFO" );
      return;
   }

   x_dbfRaise( ctx, x_dbfDriverInfo( driver, ( int )selector, ctx->result ), "RDDINFO" );
}

// Return an array of the available Replaceable Database Drivers (RDDs)
// rddList( [nRDDType] ) --> aRDDList
X_FUNC( RDDLIST )
{
   X_DBF_DRIVER_ENTRY *driver;
   int64_t             type = 0;
   size_t              count = 0, index = 1;
   X_DBF_SESSION      *session = x_dbfRuntimeSession();

   X_ARGCHECK( ctx, x_dbfOptional( ctx, 1 ) || x_dbfInteger( x_paritem( ctx, 1 ), &type ), "RDDLIST" );

   if( type == 0 || type == RDT_FULL )
   {
      for( driver = session->drivers; driver != NULL; driver = driver->next )
         ++count;
   }

   x_reta( ctx, count );

   if( count != 0 )
   {
      for( driver = session->drivers; driver != NULL; driver = driver->next )
         x_arraySetC( ctx->result, index++, driver->name );
   }
}

// Return the name of the RDD active in the current or specified work area
// rddName( ) --> cRDDName
X_FUNC( RDDNAME )
{
   DBF area = x_dbfRequireArea( ctx, "RDDNAME" );

   if( area != NULL )
      x_retc( ctx, area->driver->name );
}

// Set or return the default RDD for the application
// rddSetDefault( [cNewDefaultRDD] ) --> cPreviousDefaultRDD
X_FUNC( RDDSETDEFAULT )
{
   X_ARGCHECK( ctx, x_dbfOptional( ctx, 1 ) || x_dbfString( x_paritem( ctx, 1 ) ), "RDDSETDEFAULT" );

   x_dbfDefault( ctx, "RDDSETDEFAULT" );
}

// Determine the number of records in the current .dbf file
// RecCount( ) --> nRecords
X_FUNC( RECCOUNT )
{
   x_dbfRecordCount( ctx, "RECCOUNT" );
}

// Return the identity at the position of the record pointer
// RecNo( ) --> Identity
X_FUNC( RECNO )
{
   DBF      area = x_dbfCursorArea( ctx, "RECNO" );
   uint64_t count;

   if( area != NULL && area->state.eof )
      x_dbfRaise( ctx, x_dbfAreaRecordCount( area, &count ), "RECNO" );

   x_retnint( ctx, IIF( area != NULL, ( int64_t )area->state.record, 0 ) );
}

// Determine the record length of a database (.dbf) file
// RecSize( ) --> nBytes
X_FUNC( RECSIZE )
{
   DBF area = x_dbfCurrent();

   x_retnint( ctx, IIF( area != NULL, ( int64_t )area->schema->recordLength, 0 ) );
}

// Lock the current or the given record and release the other record locks of the work area
// RLock( [nRecord] ) --> lSuccess
X_FUNC( RLOCK )
{
   int64_t record = 0;

   X_ARGCHECK( ctx, x_dbfOptional( ctx, 1 ) || ( x_dbfInteger( x_paritem( ctx, 1 ), &record ) && record > 0 ),
               "RLOCK" );

   x_dbfLock( ctx, "RLOCK", 0, 1 );
}

// Determine the work area number of a specified alias
// Select( [cAlias] ) --> nWorkArea
X_FUNC( SELECT )
{
   X_DBF_SESSION *session = x_dbfRuntimeSession();
   DBF            area;

   if( x_pcount( ctx ) == 0 )
      x_retnint( ctx, x_dbfSessionSelected( session ) );
   else if( x_dbfString( x_paritem( ctx, 1 ) ) )
   {
      area = x_dbfSessionAlias( session, x_parc( ctx, 1 ) );
      x_retnint( ctx, IIF( area != NULL, area->number, 0 ) );
   }
   else
      x_retni( ctx, 0 );
}

// Determine whether a database file is in USE
// Used( ) --> lDbfOpen
X_FUNC( USED )
{
   x_retl( ctx, x_dbfCurrent() != NULL );
}
