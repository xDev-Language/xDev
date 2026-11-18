/*
 * Copyright (c) 2024-2026 xDev-Language
 *
 * Library: xDev Runtime
 * File: exceptions.cpp
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "internal.h"

#include <cxxabi.h>
#include <exception>

/* ---------------------------------------------------------------------------------------------
   Exceptions of BREAK, runtime errors and QUIT
   --------------------------------------------------------------------------------------------- */

/* Release the value of a BREAK */
static void s_breakFree( void *cargo )
{
   x_itemClear( static_cast<X_ITEM *>( cargo ) );
}

/* Release the error of a failure */
static void s_errorFree( void *cargo )
{
   x_errorFree( static_cast<X_ERROR *>( cargo ) );
}

/* BREAK with its value */
struct X_BREAK
{
   X_ITEM value = X_ITEM_NIL_INIT;

   explicit X_BREAK( const X_ITEM *source )
   {
      if( source != nullptr )
         x_itemCopy( &value, source );
   }

   X_BREAK( const X_BREAK &other )
   {
      x_itemCopy( &value, &other.value );
   }

   ~X_BREAK()
   {
      x_gcCleanup( s_breakFree, &value );
   }

   X_BREAK &operator=( const X_BREAK & ) = delete;
};

/* A runtime error; the exception owns the error */
struct X_FAILURE
{
   X_ERROR *error;

   explicit X_FAILURE( X_ERROR *source ) : error( source )
   {
   }

   X_FAILURE( const X_FAILURE &other ) : error( x_errorClone( other.error ) )
   {
   }

   ~X_FAILURE()
   {
      x_gcCleanup( s_errorFree, error );
   }

   X_FAILURE &operator=( const X_FAILURE & ) = delete;
};

/* QUIT */
struct X_QUIT
{
};

/* ---------------------------------------------------------------------------------------------
   Raising
   --------------------------------------------------------------------------------------------- */

/* BREAK [value] */
void x_breakRaise( const X_ITEM *value )
{
   throw X_BREAK( value );
}

/* Raise a runtime error; the exception takes the error */
void x_errorThrow( X_ERROR *error )
{
   throw X_FAILURE( error );
}

/* QUIT */
void x_quitRaise( void )
{
   throw X_QUIT();
}

/* ---------------------------------------------------------------------------------------------
   Catching from compiled code
   --------------------------------------------------------------------------------------------- */

/* RECOVER USING value: take the value of the BREAK */
void x_recoverBreak( void *except, X_ITEM *value )
{
   auto *event = static_cast<X_BREAK *>( __cxxabiv1::__cxa_begin_catch( except ) );

   x_errorClear();
   *value = event->value;
   event->value = X_ITEM_NIL_INIT;

   __cxxabiv1::__cxa_end_catch();
}

/* Keep a BREAK to raise it again later */
void x_catchBreak( void *except )
{
   auto *event = static_cast<X_BREAK *>( __cxxabiv1::__cxa_begin_catch( except ) );

   x_breakPut( &event->value );

   __cxxabiv1::__cxa_end_catch();
}

/* Keep a runtime error to raise it again later */
void x_catchError( void *except )
{
   auto *event = static_cast<X_FAILURE *>( __cxxabiv1::__cxa_begin_catch( except ) );
   X_ERROR *error = event->error;

   event->error = nullptr;
   x_errorPut( error );

   __cxxabiv1::__cxa_end_catch();
}

/* Raise the caught exception again */
void x_exceptionResume( void *except )
{
   __cxxabiv1::__cxa_begin_catch( except );
   std::exception_ptr pending = std::current_exception();
   __cxxabiv1::__cxa_end_catch();

   std::rethrow_exception( pending );
}

/* Drop a QUIT */
void x_catchQuit( void *except )
{
   __cxxabiv1::__cxa_begin_catch( except );
   __cxxabiv1::__cxa_end_catch();
}

/* ---------------------------------------------------------------------------------------------
   Catching around C callbacks
   --------------------------------------------------------------------------------------------- */

/* Call func; a BREAK or a runtime error is kept to be raised again later, a QUIT is dropped */
template <typename FUNC, typename ARG> static void s_catch( FUNC func, ARG arg )
{
   try
   {
      func( arg );
   }
   catch( X_BREAK &event )
   {
      x_breakPut( &event.value );
   }
   catch( X_FAILURE &event )
   {
      X_ERROR *error = event.error;

      event.error = nullptr;
      x_errorPut( error );
   }
   catch( X_QUIT & )
   {
   }
}

/* Call a function of compiled code */
void x_callCatch( X_FUNCPTR function, X_CALL_CTX *ctx )
{
   s_catch( function, ctx );
}

/* Call a garbage collector function */
void x_gcCatch( X_GARBAGE_FUNC_PTR function, void *cargo )
{
   s_catch( function, cargo );
}
