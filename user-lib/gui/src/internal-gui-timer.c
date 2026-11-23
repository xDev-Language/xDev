/*
 * Copyright (c) 2024-2026 xDev-Language
 *
 * Library...: xDev GUI
 * File......: internal-gui-timer.c
 *
 */

#include "internal-gui.h"

static X_GUI_TIMER *s_timers;
static size_t s_timerCount;
static size_t s_timerCapacity;
static UINT_PTR s_timerId;

/* ---------------------------------------------------------------------------------------------
   Timers
   --------------------------------------------------------------------------------------------- */

/* The running timer with the identifier, or NULL */
X_GUI_TIMER *x_guiTimerFind( UINT_PTR id )
{
   size_t i;

   for( i = 0; i < s_timerCount; ++i )
   {
      if( s_timers[i].id == id )
         return &s_timers[i];
   }

   return NULL;
}

/* Stop a running timer and release its xDev object */
static void s_timerRemove( size_t index )
{
   X_ITEM *timer = s_timers[index].timer;

   KillTimer( s_timers[index].window, s_timers[index].id );
   s_timers[index] = s_timers[--s_timerCount];
   x_itemRelease( timer );
}

/* Stop the timers of a window, all timers for NULL */
void x_guiTimersRelease( HWND window )
{
   size_t i = 0;

   while( i < s_timerCount )
   {
      if( window == NULL || s_timers[i].window == window )
         s_timerRemove( i );
      else
         ++i;
   }
}

/* Start a timer or change its interval; returns its identifier, 0 with an error raised */
UINT_PTR x_guiTimerStart( X_CALL_CTX *ctx, HWND window, UINT_PTR id, int interval )
{
   const X_GUI_TIMER *timer = IIF( id != 0, x_guiTimerFind( id ), NULL );

   if( interval <= 0 )
   {
      x_guiRaise( ctx, "GUITIMER:START" );
      return 0;
   }

   if( id == 0 )
      id = ++s_timerId;

   if( SetTimer( window, id, ( UINT )interval, NULL ) == 0 )
   {
      x_guiRaiseWin32( ctx, "SetTimer", GetLastError() );
      return 0;
   }

   if( timer == NULL )
   {
      if( s_timerCount == s_timerCapacity )
      {
         s_timerCapacity = IIF( s_timerCapacity == 0, ( size_t )8, s_timerCapacity * 2 );
         s_timers = ( X_GUI_TIMER * )x_xrealloc( s_timers, s_timerCapacity * sizeof( X_GUI_TIMER ) );
      }

      s_timers[s_timerCount++] = ( X_GUI_TIMER ){ window, id, x_itemNew( ctx->self ) };
   }

   return id;
}

/* Stop a timer */
void x_guiTimerStop( UINT_PTR id )
{
   size_t i;

   for( i = 0; i < s_timerCount; ++i )
   {
      if( s_timers[i].id == id )
      {
         s_timerRemove( i );
         return;
      }
   }
}

/* Whether a timer is running */
int x_guiTimerActive( UINT_PTR id )
{
   return id != 0 && x_guiTimerFind( id ) != NULL;
}

/* Stop all timers and free their table, when the module ends */
void x_guiTimersShutdown( void )
{
   x_guiTimersRelease( NULL );
   if( s_timers != NULL )
   {
      x_xfree( s_timers );
      s_timers = NULL;
      s_timerCapacity = 0;
   }
}
