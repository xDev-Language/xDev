/*
 * Copyright (c) 2024-2026 xDev-Language
 *
 * Library...: xDev 7.0.0 RDD
 * File......: dbf-operations.c
 *
 */

#include "dbf-internal.h"

#include <xdev/dbinfo.ch>

X_DBF_STATUS x_dbfTableCreate( X_DBF_SESSION *session, const X_DBF_CREATE_OPTIONS *options, const X_DBF_SCHEMA *schema )
{
   X_DBF_CREATE_OPTIONS resolved;
   X_DBF_DRIVER_ENTRY  *driver;

   resolved = *options;
   if( resolved.driver == NULL )
      resolved.driver = "DBFNTX";
   driver = x_dbfDriverFind( session, resolved.driver );
   if( driver == NULL )
      return X_DBF_DRIVER_NOT_FOUND;
   if( driver->operations.create == NULL )
      return X_DBF_UNSUPPORTED_OPERATION;
   resolved.driver = driver->name;
   return driver->operations.create( &resolved, schema );
}

void x_dbfSessionDiscard( X_DBF_SESSION *session )
{
   X_DBF_DRIVER_ENTRY *driver;

   if( session == NULL )
      return;
   while( session->areas != NULL )
      x_dbfAreaDiscard( session->areas );
   while( session->drivers != NULL )
   {
      driver = session->drivers;
      session->drivers = driver->next;
      x_xfree( driver );
   }
   x_xfree( session );
}

X_DBF_STATUS x_dbfCheckWrite( DBF area )
{
   if( area->indexBusy )
      return X_DBF_INVALID_STATE;
   X_DBF_STATUS status = x_dbfAreaPrepare( area );
   if( status != X_DBF_OK )
      return status;
   if( area->readOnly )
      return X_DBF_READ_ONLY;
   if( area->state.eof )
      return X_DBF_NO_RECORD;
   return area->driver->operations.checkWrite( area, area->state.record );
}

X_DBF_STATUS x_dbfAreaRecordCount( DBF area, uint64_t *count )
{
   uint64_t     refreshed;
   X_DBF_STATUS status;

   status = area->driver->operations.recordCount( area, &refreshed );
   if( status == X_DBF_OK )
   {
      area->state.recordCount = refreshed;
      if( area->state.eof )
         area->state.record = refreshed + 1;
      *count = refreshed;
   }
   return status;
}

X_DBF_STATUS x_dbfAreaLock( DBF area, uint64_t record )
{
   X_DBF_STATUS status = x_dbfAreaPrepare( area );

   if( status != X_DBF_OK )
      return status;
   if( area->driver->operations.lock == NULL )
      return X_DBF_UNSUPPORTED_OPERATION;
   if( area->state.dirty )
   {
      status = x_dbfAreaFlush( area );
      if( status != X_DBF_OK )
         return status;
   }
   status = area->driver->operations.lock( area, record );
   if( status == X_DBF_OK && !area->state.eof && ( record == 0 || record == area->state.record ) )
      status = x_dbfAreaGoTo( area, area->state.record );
   return status;
}

X_DBF_STATUS x_dbfAreaUnlock( DBF area, uint64_t record )
{
   X_DBF_STATUS status;

   if( area->driver->operations.unlock == NULL )
      return X_DBF_UNSUPPORTED_OPERATION;
   status = x_dbfAreaFlush( area );
   if( status != X_DBF_OK )
      return status;
   return area->driver->operations.unlock( area, record );
}

X_DBF_STATUS x_dbfAreaIsLocked( DBF area, uint64_t record, int *locked )
{
   if( area->driver->operations.isLocked == NULL )
      return X_DBF_UNSUPPORTED_OPERATION;
   return area->driver->operations.isLocked( area, record, locked );
}

static X_DBF_STATUS s_reorganize( DBF area, int pack )
{
   X_DBF_STATUS status;
   X_DBF_STATUS ( *operation )( DBF ) = pack ? area->driver->operations.pack : area->driver->operations.zap;

   if( area->readOnly )
      return X_DBF_READ_ONLY;
   if( area->shared )
      return X_DBF_SHARED_OPERATION;
   if( operation == NULL )
      return X_DBF_UNSUPPORTED_OPERATION;
   status = x_dbfAreaFlush( area );
   if( status != X_DBF_OK )
      return status;
   status = operation( area );
   if( status != X_DBF_OK )
      return status;
   if( area->indexes != NULL )
      return x_dbfOrderRebuild( area );
   return x_dbfAreaGoTo( area, 1 );
}

X_DBF_STATUS x_dbfAreaPack( DBF area )
{
   return s_reorganize( area, 1 );
}

X_DBF_STATUS x_dbfAreaPackMemo( DBF area )
{
   X_ITEM       result __attribute__( ( cleanup( x_itemClear ) ) ) = X_ITEM_NIL_INIT;
   X_DBF_STATUS status;
   if( area->readOnly )
      return X_DBF_READ_ONLY;
   if( area->shared )
      return X_DBF_SHARED_OPERATION;
   if( area->driver->operations.info == NULL )
      return X_DBF_UNSUPPORTED_OPERATION;
   status = x_dbfAreaPrepare( area );
   if( status == X_DBF_OK )
      status = x_dbfAreaFlush( area );
   if( status == X_DBF_OK )
      status = area->driver->operations.info( area, DBI_MEMOPACK, &result );
   return status;
}

X_DBF_STATUS x_dbfAreaZap( DBF area )
{
   return s_reorganize( area, 0 );
}
