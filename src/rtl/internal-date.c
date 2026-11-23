/*
 * Copyright (c) 2024-2026 xDev-Language
 *
 * Library...: xDev Runtime
 * File......: internal-date.c
 *
 */

#include "classes.h"
#include "internal-private.h"
#include "operators.h"
#include "platform.h"
#include "statements.h"
#include "terminal.h"

#include "../../include/xdev/fileio.ch"
#include "../../include/xdev/inkey.ch"
#include "../../include/xdev/set.ch"

#include <ctype.h>
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#if X_OS_WINDOWS
#include <windows.h>
#endif

/* WaitPeriod(): the end of the period, in milliseconds of x_dateMilliSeconds() */
static int64_t s_waitPeriodEnd;

/* ---------------------------------------------------------------------------------------------
   Date and time helpers of the API
   --------------------------------------------------------------------------------------------- */

/* The date argument as a Julian day; without one, today; 0 is the empty date */
long x_dateParam( X_CALL_CTX *ctx, int n )
{
   int year, month, day;

   if( X_ISDATE( ctx, n ) )
      return x_pardl( ctx, n );

   x_dateToday( &year, &month, &day );

   return x_dateEncode( year, month, day );
}

/* 1 for a leap year */
int x_dateIsLeap( int year )
{
   return ( year % 4 == 0 && year % 100 != 0 ) || year % 400 == 0;
}

/* The number of days of a month (1 to 12) */
int x_dateMonthDays( int month, int leap )
{
   static const int days[12] = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };

   return days[month - 1] + IIF( month == 2 && leap, 1, 0 );
}

/* The ISO day of the week: 1 Monday ... 7 Sunday */
int x_dateIsoDow( long julian )
{
   return ( x_dateJulianDOW( julian ) + 5 ) % 7 + 1;
}

/* The number of a day or month from its English name or the beginning of it, ignoring case; 0 when not found */
int x_dateNameNumber( const char *text, size_t length, const char *( *nameOf )( int ), int count )
{
   int i;

   if( length > 0 )
   {
      for( i = 1; i <= count; ++i )
      {
         if( length <= strlen( nameOf( i ) ) && x_strnicmp( nameOf( i ), text, length ) == 0 )
            return i;
      }
   }

   return 0;
}

/* "hh", "hh:mm", "hh:mm:ss" or "hh:mm:ss:cc" to seconds since midnight; -1 when not valid */
double x_timeToSeconds( const char *text, size_t length )
{
   static const int limits[3] = { 24, 60, 60 };
   int parts[3] = { 0, 0, 0 };
   int hundredths = 0;
   size_t i;

   if( length != 2 && length != 5 && length != 8 && length != 11 )
      return -1;

   for( i = 0; i < length; ++i )
   {
      int separator = IIF( i == 2 || i == 5 || i == 8, ':', 0 );

      if( separator != 0 )
      {
         if( text[i] != separator )
            return -1;
      }
      else if( !isdigit( ( unsigned char )text[i] ) )
         return -1;
   }

   for( i = 0; i < 3 && i * 3 < length; ++i )
   {
      parts[i] = ( text[i * 3] - '0' ) * 10 + text[i * 3 + 1] - '0';
      if( parts[i] >= limits[i] )
         return -1;
   }

   if( length == 11 )
      hundredths = ( text[9] - '0' ) * 10 + text[10] - '0';

   return parts[0] * 3600 + parts[1] * 60 + parts[2] + hundredths / 100.0;
}

/* Seconds as "hh:mm:ss" or "hh:mm:ss:cc"; hours are not limited to a day */
void x_timeReturn( X_CALL_CTX *ctx, double seconds, int hundredths )
{
   int64_t total = ( int64_t )( IIF( seconds > 0, seconds, 0 ) * 100.0 + 0.5 );
   long long hours;
   int minutes, secs, cents;
   char text[32];

   if( hundredths )
   {
      hours = total / 360000;
      minutes = ( int )( total / 6000 % 60 );
      secs = ( int )( total / 100 % 60 );
      cents = ( int )( total % 100 );
      snprintf( text, sizeof( text ), "%02lld:%02d:%02d:%02d", hours, minutes, secs, cents );
   }
   else
   {
      total /= 100;
      hours = total / 3600;
      minutes = ( int )( total / 60 % 60 );
      secs = ( int )( total % 60 );
      snprintf( text, sizeof( text ), "%02lld:%02d:%02d", hours, minutes, secs );
   }

   x_retc( ctx, text );
}

/* MilliSeconds(): the current UTC time as a Julian timestamp in milliseconds, the Julian day number
   times 86400000 plus the milliseconds since midnight */
int64_t x_dateMilliSeconds( void )
{
   struct timespec now;
   int64_t seconds;

   timespec_get( &now, TIME_UTC );
   seconds = ( int64_t )x_dateEncode( 1970, 1, 1 ) * 86400 + ( int64_t )now.tv_sec;

   return seconds * 1000 + now.tv_nsec / 1000000;
}

/* The system clock set to a date (year 0 keeps it) and to seconds since midnight (-1 keeps the time) */
static int s_dateSetClock( int year, int month, int day, double seconds )
{
#if X_OS_WINDOWS
   SYSTEMTIME local;

   GetLocalTime( &local );
   if( year > 0 )
   {
      local.wYear = ( WORD )year;
      local.wMonth = ( WORD )month;
      local.wDay = ( WORD )day;
   }

   if( seconds >= 0 )
   {
      local.wHour = ( WORD )( seconds / 3600 );
      local.wMinute = ( WORD )( ( int )seconds / 60 % 60 );
      local.wSecond = ( WORD )( ( int )seconds % 60 );
      local.wMilliseconds = ( WORD )( ( seconds - ( int )seconds ) * 1000 );
   }

   return SetLocalTime( &local ) != 0;
#else
   struct timespec now;
   struct tm local;
   time_t moment;

   clock_gettime( CLOCK_REALTIME, &now );
   localtime_r( &now.tv_sec, &local );
   if( year > 0 )
   {
      local.tm_year = year - 1900;
      local.tm_mon = month - 1;
      local.tm_mday = day;
   }

   if( seconds >= 0 )
   {
      local.tm_hour = ( int )( seconds / 3600 );
      local.tm_min = ( int )seconds / 60 % 60;
      local.tm_sec = ( int )seconds % 60;
      now.tv_nsec = ( long )( ( seconds - ( int )seconds ) * 1000000000.0 );
   }

   local.tm_isdst = -1;
   moment = mktime( &local );
   if( moment == ( time_t )-1 )
      return 0;

   now.tv_sec = moment;

   return clock_settime( CLOCK_REALTIME, &now ) == 0;
#endif
}

/* ---------------------------------------------------------------------------------------------
   Date and time functions of the API
   --------------------------------------------------------------------------------------------- */

/* AddMonth(): a date, or today, shifted by calendar months; a day past the end of the month becomes its last day */
void x_dateAddMonth( X_CALL_CTX *ctx )
{
   int year;
   int month;
   int day;
   int64_t months;

   if( X_ISNUM( ctx, 1 ) )
   {
      months = x_parnint( ctx, 1 );
      x_dateToday( &year, &month, &day );
   }
   else
   {
      x_dateDecode( x_pardl( ctx, 1 ), &year, &month, &day );
      months = x_parnint( ctx, 2 );
   }

   int64_t yearShift = months / 12;
   int64_t monthShift = months % 12;
   int64_t shiftedYear = ( int64_t )year + yearShift;
   int64_t shiftedMonth = ( int64_t )month + monthShift;

   if( shiftedMonth <= 0 )
   {
      shiftedMonth += 12;
      --shiftedYear;
   }
   else if( shiftedMonth > 12 )
   {
      shiftedMonth -= 12;
      ++shiftedYear;
   }

   X_ARGCHECK( ctx, shiftedYear >= INT_MIN && shiftedYear <= INT_MAX, "ADDMONTH" );

   static const int daysPerMonth[12] = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };
   int days = daysPerMonth[shiftedMonth - 1];
   int shiftedYearInt = ( int )shiftedYear;

   if( shiftedMonth == 2 && ( shiftedYearInt % 4 == 0 && ( shiftedYearInt % 100 != 0 || shiftedYearInt % 400 == 0 ) ) )
      ++days;

   if( day > days )
      day = days;

   x_retd( ctx, shiftedYearInt, ( int )shiftedMonth, day );
}

/* AmPm(): a 24-hour time in 12-hour form with an am/pm suffix */
void x_timeAmPm( X_CALL_CTX *ctx )
{
   const char *time = x_parc( ctx, 1 );
   size_t length = x_parclen( ctx, 1 );
   size_t resultLength;
   char *result = ( char * )x_xgrab( IIF( length < 2, 2, length ) + 4 );
   int hour = 0;
   int isAM;

   if( length > 0 )
   {
      memcpy( result, time, length );
      hour = ( int )strtol( time, NULL, 10 );
   }

   if( hour == 0 || hour == 24 )
   {
      if( length < 2 )
         length = 2;

      result[0] = '1';
      result[1] = '2';
      isAM = 1;
   }
   else if( hour > 12 )
   {
      if( length < 2 )
         length = 2;

      hour -= 12;
      result[0] = ( char )( '0' + hour / 10 );
      result[1] = ( char )( '0' + hour % 10 );
      if( result[0] == '0' )
         result[0] = ' ';

      isAM = 0;
   }
   else
      isAM = hour != 12;

   resultLength = length + 3;
   memcpy( result + length, IIF( isAM, " am", " pm" ), 3 );
   result[resultLength] = '\0';
   x_retclen_buffer( ctx, result, resultLength );
}

/* DaysToMonth(): the days from the first of January to the beginning of a month */
void x_dateDaysToMonth( X_CALL_CTX *ctx )
{
   int64_t month = x_parnint( ctx, 1 );
   int leap = X_ISLOG( ctx, 2 ) && x_parl( ctx, 2 );
   int days = 0;
   int i;

   if( month >= 1 && month <= 12 )
   {
      for( i = 1; i < month; ++i )
         days += x_dateMonthDays( i, leap );
   }

   x_retni( ctx, days );
}

/* DMY(): a date as "dd. Month yyyy" */
void x_dateDMY( X_CALL_CTX *ctx )
{
   /* DMY( .T. ): the period without a date */
   int period = IIF( X_ISLOG( ctx, 1 ), x_parl( ctx, 1 ), X_ISLOG( ctx, 2 ) && x_parl( ctx, 2 ) );
   long julian = x_dateParam( ctx, 1 );
   int century = x_setCentury( -1 );
   int year, month, day;
   int width, shown;
   char text[48];

   if( julian == 0 )
   {
      x_retc( ctx, "" );
      return;
   }

   x_dateDecode( julian, &year, &month, &day );
   width = IIF( century, 4, 2 );
   shown = IIF( century, year, year % 100 );
   snprintf( text, sizeof( text ), "%d%s %s %0*d", day, IIF( period, ".", "" ), x_dateCMonth( month ), width, shown );
   x_retc( ctx, text );
}

/* EoM() and EoQ(): the last day of the month or, with quarter, of the quarter of a date */
void x_dateEnd( X_CALL_CTX *ctx, int quarter )
{
   long julian = x_dateParam( ctx, 1 );
   int year, month, day;

   if( julian == 0 )
   {
      x_retdl( ctx, 0 );
      return;
   }

   x_dateDecode( julian, &year, &month, &day );
   if( quarter )
      month = ( month - 1 ) / 3 * 3 + 3;

   x_retdl( ctx, x_dateEncode( year, month, x_dateMonthDays( month, x_dateIsLeap( year ) ) ) );
}

/* LastDayOM(): the number of days in the month of a date or of a month number */
void x_dateLastDayOM( X_CALL_CTX *ctx )
{
   int year, month, day;

   if( X_ISNUM( ctx, 1 ) )
   {
      int64_t number = x_parnint( ctx, 1 );

      if( number < 1 || number > 12 )
      {
         x_retni( ctx, 0 );
         return;
      }

      x_dateToday( &year, &month, &day );
      month = ( int )number;
   }
   else
   {
      long julian = x_dateParam( ctx, 1 );

      if( julian == 0 )
      {
         x_retni( ctx, 0 );
         return;
      }

      x_dateDecode( julian, &year, &month, &day );
   }

   x_retni( ctx, x_dateMonthDays( month, x_dateIsLeap( year ) ) );
}

/* MDY(): a date as "Month dd, yy" */
void x_dateMDY( X_CALL_CTX *ctx )
{
   long julian = x_dateParam( ctx, 1 );
   int century = x_setCentury( -1 );
   int year, month, day;
   int width, shown;
   char text[48];

   if( julian == 0 )
   {
      x_retc( ctx, "" );
      return;
   }

   x_dateDecode( julian, &year, &month, &day );
   width = IIF( century, 4, 2 );
   shown = IIF( century, year, year % 100 );
   snprintf( text, sizeof( text ), "%s %d, %0*d", x_dateCMonth( month ), day, width, shown );
   x_retc( ctx, text );
}

/* SecToTime(): seconds, or the time of day, as a time string */
void x_timeSecToTime( X_CALL_CTX *ctx )
{
   double seconds;

   if( X_ISNUM( ctx, 1 ) )
      seconds = x_parnd( ctx, 1 );
   else
   {
      struct timespec now;
      struct tm *local;

      timespec_get( &now, TIME_UTC );
      local = localtime( &now.tv_sec );
      seconds = local->tm_hour * 3600 + local->tm_min * 60 + local->tm_sec + ( now.tv_nsec / 10000000 ) / 100.0;
   }

   x_timeReturn( ctx, fmod( seconds, 86400.0 ), X_ISLOG( ctx, 2 ) && x_parl( ctx, 2 ) );
}

/* SetCentury(): the century setting from a logical or "ON" / "OFF"; returns the previous setting */
void x_dateSetCentury( X_CALL_CTX *ctx )
{
   int century = -1;

   if( X_ISLOG( ctx, 1 ) )
      century = x_parl( ctx, 1 );
   else if( X_ISCHAR( ctx, 1 ) )
      century = x_stricmp( x_parc( ctx, 1 ), "ON" ) == 0;

   x_retl( ctx, x_setCentury( century ) );
}

/* SetDate(): the system date from a date from 1970 on; .F. without the right to change the clock */
void x_dateSetDate( X_CALL_CTX *ctx )
{
   long julian = x_pardl( ctx, 1 );
   int year, month, day;

   x_dateDecode( julian, &year, &month, &day );
   x_retl( ctx, julian != 0 && year >= 1970 && s_dateSetClock( year, month, day, -1 ) );
}

/* SetTime(): the system time from "hh", "hh:mm", "hh:mm:ss" or "hh:mm:ss:cc"; .F. for another text */
void x_dateSetTime( X_CALL_CTX *ctx )
{
   double seconds = x_timeToSeconds( x_parc( ctx, 1 ), x_parclen( ctx, 1 ) );

   x_retl( ctx, seconds >= 0 && s_dateSetClock( 0, 0, 0, seconds ) );
}

/* SToD(): a "yyyymmdd" string as a date; an invalid string gives the empty date */
void x_dateSToD( X_CALL_CTX *ctx )
{
   const char *text = IIF( X_ISCHAR( ctx, 1 ), x_parc( ctx, 1 ), "" );
   size_t length = IIF( X_ISCHAR( ctx, 1 ), x_parclen( ctx, 1 ), 0 );
   int digits[8];
   int year, month, day;
   size_t i;

   if( length != 8 )
   {
      x_retdl( ctx, 0 );
      return;
   }

   for( i = 0; i < 8; ++i )
   {
      if( !isdigit( ( unsigned char )text[i] ) )
      {
         x_retdl( ctx, 0 );
         return;
      }

      digits[i] = text[i] - '0';
   }

   year = digits[0] * 1000 + digits[1] * 100 + digits[2] * 10 + digits[3];
   month = digits[4] * 10 + digits[5];
   day = digits[6] * 10 + digits[7];
   if( year < 1 || month < 1 || month > 12 || day < 1 || day > x_dateMonthDays( month, x_dateIsLeap( year ) ) )
   {
      x_retdl( ctx, 0 );
      return;
   }

   x_retdl( ctx, x_dateEncode( year, month, day ) );
}

/* WaitPeriod(): nDelay in 1/100 seconds starts a period; .T. while it has not elapsed, also over midnight */
void x_dateWaitPeriod( X_CALL_CTX *ctx )
{
   int64_t now = x_dateMilliSeconds();

   if( X_ISNUM( ctx, 1 ) )
      s_waitPeriodEnd = now + ( int64_t )( x_parnd( ctx, 1 ) * 10.0 );

   x_retl( ctx, now < s_waitPeriodEnd );
}

/* Week(): the ISO 8601 week of a date */
void x_dateWeek( X_CALL_CTX *ctx )
{
   long julian = x_dateParam( ctx, 1 );
   long thursday;
   int year, month, day;

   if( julian == 0 )
   {
      x_retni( ctx, 0 );
      return;
   }

   /* ISO 8601: the week belongs to the year of its Thursday */
   thursday = julian - x_dateIsoDow( julian ) + 4;
   x_dateDecode( thursday, &year, &month, &day );
   x_retni( ctx, ( int )( ( thursday - x_dateEncode( year, 1, 1 ) ) / 7 + 1 ) );
}

/* WoM(): the week of the month of a date */
void x_dateWoM( X_CALL_CTX *ctx )
{
   long julian = x_dateParam( ctx, 1 );
   int year, month, day;

   if( julian == 0 )
   {
      x_retni( ctx, 0 );
      return;
   }

   x_dateDecode( julian, &year, &month, &day );
   x_retni( ctx, ( day + x_dateIsoDow( x_dateEncode( year, month, 1 ) ) - 2 ) / 7 + 1 );
}
