/*
 * Copyright (c) 2024-2026 xDev-Language
 *
 * Library...: xDev GUI
 * File......: internal-gui-datepicker.c
 *
 */

#include <string.h>
#include "internal-gui.h"

/* ---------------------------------------------------------------------------------------------
   Date pickers
   --------------------------------------------------------------------------------------------- */

/* The date of a date picker as a Julian day, 0 for none */
long x_guiDateValue( HWND window )
{
   SYSTEMTIME time;

   if( SendMessageW( window, DTM_GETSYSTEMTIME, 0, ( LPARAM )&time ) != GDT_VALID )
      return 0;

   return x_dateEncode( time.wYear, time.wMonth, time.wDay );
}

/* Set the date of a date picker from a Julian day */
int x_guiDateSetValue( HWND window, long julian )
{
   SYSTEMTIME time;
   int year, month, day;

   if( julian <= 0 )
      return 0;

   x_dateDecode( julian, &year, &month, &day );
   memset( &time, 0, sizeof( time ) );
   time.wYear = ( WORD )year;
   time.wMonth = ( WORD )month;
   time.wDay = ( WORD )day;

   return x_guiSend( window, DTM_SETSYSTEMTIME, GDT_VALID, ( LPARAM )&time ) != 0;
}
