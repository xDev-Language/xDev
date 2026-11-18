/*
 * Copyright (c) 2024-2026 xDev-Language
 *
 * Library: xDev 7.0.0 RDD
 * File: dbf-navigation.c
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "dbf-internal.h"

static X_DBF_STATUS s_boundary( DBF area, int bottom, int skipDeleted )
{
   uint64_t     count, record;
   X_DBF_STATUS status;

   if( area->order != NULL && area->pendingRelation != NULL && area->pendingRelation->scoped )
   {
      status = x_dbfAreaPrepare( area );
      if( status != X_DBF_OK )
         return status;
   }
   if( area->order != NULL )
      return x_dbfOrderBoundary( area, bottom, skipDeleted );
   status = x_dbfAreaRecordCount( area, &count );
   if( status != X_DBF_OK )
      return status;
   record = IIF( bottom, count, 1 );
   while( record != 0 && record <= count )
   {
      int accepted;
      status = x_dbfAreaGoTo( area, record );
      if( status != X_DBF_OK )
         return status;
      status = x_dbfAreaAccept( area, skipDeleted, &accepted );
      if( status != X_DBF_OK )
         return status;
      if( accepted )
         return X_DBF_OK;
      if( bottom )
         --record;
      else
         ++record;
   }
   return x_dbfAreaGoTo( area, 0 );
}

X_DBF_STATUS x_dbfAreaGoTop( DBF area, int skipDeleted )
{
   return s_boundary( area, 0, skipDeleted );
}

X_DBF_STATUS x_dbfAreaGoBottom( DBF area, int skipDeleted )
{
   return s_boundary( area, 1, skipDeleted );
}

X_DBF_STATUS x_dbfAreaSkip( DBF area, int64_t records, int skipDeleted )
{
   uint64_t     count, remaining, record;
   int          forward = records > 0;
   int          wasBof, wasEof;
   X_DBF_STATUS status;

   status = x_dbfAreaPrepare( area );
   if( status != X_DBF_OK )
      return status;
   wasBof = area->state.bof;
   wasEof = area->state.eof;
   status = x_dbfAreaRecordCount( area, &count );
   if( status != X_DBF_OK )
      return status;
   if( records == 0 )
   {
      status = x_dbfAreaGoTo( area, IIF( wasEof, count + 1, area->state.record ) );
      if( status == X_DBF_OK )
      {
         area->state.bof = wasBof;
         area->state.eof = wasEof;
      }
      return status;
   }
   if( area->order != NULL )
      return x_dbfOrderSkip( area, records, skipDeleted );
   if( count == 0 )
   {
      status = x_dbfAreaGoTo( area, 0 );
      if( status == X_DBF_OK )
      {
         if( forward )
            area->state.bof = 0;
         else
            area->state.eof = 0;
      }
      return status;
   }
   if( forward && wasEof )
   {
      area->state.bof = 0;
      return X_DBF_OK;
   }
   record = IIF( wasEof, count + 1, area->state.record );
   remaining = IIF( forward, ( uint64_t )records, ( uint64_t )( -( records + 1 ) ) + 1 );
   while( remaining != 0 )
   {
      int accepted;
      if( forward )
         ++record;
      else if( record != 0 )
         --record;
      if( record == 0 )
      {
         status = x_dbfAreaGoTop( area, skipDeleted );
         if( status == X_DBF_OK )
         {
            area->state.bof = 1;
            area->state.eof = 0;
         }
         return status;
      }
      if( record > count )
      {
         status = x_dbfAreaGoTo( area, count + 1 );
         if( status == X_DBF_OK )
            area->state.bof = 0;
         return status;
      }
      status = x_dbfAreaGoTo( area, record );
      if( status != X_DBF_OK )
         return status;
      status = x_dbfAreaAccept( area, skipDeleted, &accepted );
      if( status != X_DBF_OK )
         return status;
      if( accepted )
         --remaining;
   }
   return X_DBF_OK;
}
