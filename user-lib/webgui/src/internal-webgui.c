/*
 * Copyright (c) 2024-2026 xDev-Language
 *
 * Library: xDev WebGUI
 * File: internal-webgui.c
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

#include "internal-webgui.h"

#include <objbase.h>
#include <shlwapi.h>

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunknown-pragmas"
#pragma clang diagnostic ignored "-Wc23-extensions"
#include <WebView2.h>
#pragma clang diagnostic pop

#pragma comment( lib, "ole32" )
#pragma comment( lib, "shlwapi" )

/* Size of a new window at 96 DPI */
enum
{
   X_WEBGUI_WINDOW_WIDTH = 640,
   X_WEBGUI_WINDOW_HEIGHT = 480
};

/* The function of WebView2Loader.dll that starts the environment of the browser */
typedef HRESULT( STDAPICALLTYPE *X_WEBGUI_CREATE )( PCWSTR browser, PCWSTR data,
                                                  ICoreWebView2EnvironmentOptions *options,
                                                  ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler *handler );

/* A handler given to WebView2: a COM object with its table of methods and the window it serves; it is freed when
   WebView2 and the library release it */
typedef struct
{
   const void *methods;
   LONG references;
   HWND window;
} X_WEBGUI_HANDLER;

/* The methods of a handler told that WebView2 made an object */
typedef struct
{
   HRESULT( STDMETHODCALLTYPE *QueryInterface )( X_WEBGUI_HANDLER *self, REFIID id, void **object );
   ULONG( STDMETHODCALLTYPE *AddRef )( X_WEBGUI_HANDLER *self );
   ULONG( STDMETHODCALLTYPE *Release )( X_WEBGUI_HANDLER *self );
   HRESULT( STDMETHODCALLTYPE *Invoke )( X_WEBGUI_HANDLER *self, HRESULT result, void *made );
} X_WEBGUI_MADE;

/* The methods of a handler of an event of the browser */
typedef struct
{
   HRESULT( STDMETHODCALLTYPE *QueryInterface )( X_WEBGUI_HANDLER *self, REFIID id, void **object );
   ULONG( STDMETHODCALLTYPE *AddRef )( X_WEBGUI_HANDLER *self );
   ULONG( STDMETHODCALLTYPE *Release )( X_WEBGUI_HANDLER *self );
   HRESULT( STDMETHODCALLTYPE *Invoke )( X_WEBGUI_HANDLER *self, ICoreWebView2 *sender, IUnknown *args );
} X_WEBGUI_EVENT;

/* Events of the browser the window follows */
typedef enum
{
   X_WEBGUI_TITLE,
   X_WEBGUI_CLOSE,
   X_WEBGUI_MESSAGE
} X_WEBGUI_LISTEN;

/* A window with its browser: the controller places the browser in the window, the browser shows the page of uri;
   object is the wg_Window and controls are its wg_Control objects by number, all kept while the window lives;
   messages for the page wait in queue until the runtime of the page is ready */
typedef struct
{
   ICoreWebView2Controller *controller;
   ICoreWebView2 *webview;
   EventRegistrationToken titleToken;
   EventRegistrationToken closeToken;
   EventRegistrationToken messageToken;
   wchar_t *uri;
   X_ITEM *object;
   X_ITEM **controls;
   size_t controlCount;
   wchar_t **queue;
   size_t queued;
   int ready;
} X_WEBGUI_VIEW;

/* Text being built, a JSON command for the page */
typedef struct
{
   char *data;
   size_t length;
   size_t capacity;
} X_WEBGUI_JSON;

/* Class description read by the compiler for typed code (LOCAL oWin AS wg_Window). */
#define X_WEBGUI_INTERFACE \
   "XDEVCLS 1\n" \
   "C WG_WINDOW -\n" \
   "M DATA HANDLE 17 ANY -\n" \
   "M DATA ONMESSAGE 1 CODEBLOCK -\n" \
   "M METHOD NEW 1 ANY - TITLE WIDTH HEIGHT\n" \
   "M METHOD OPEN 1 ANY - FILE\n" \
   "M METHOD SEND 1 ANY - JSON\n" \
   "M METHOD RUN 1 ANY -\n" \
   "M METHOD CLOSE 1 ANY -\n" \
   "C WG_CONTROL -\n" \
   "M DATA PARENT 17 ANY -\n" \
   "M DATA ID 17 ANY -\n" \
   "M DATA ONCLICK 1 CODEBLOCK -\n" \
   "M DATA ONCHANGE 1 CODEBLOCK -\n" \
   "M ACCESS TEXT 1 ANY -\n" \
   "M ASSIGN TEXT 1 ANY - VALUE\n" \
   "M ACCESS VALUE 1 ANY -\n" \
   "M ASSIGN VALUE 1 ANY - VALUE\n" \
   "M ACCESS ENABLED 1 ANY -\n" \
   "M ASSIGN ENABLED 1 ANY - VALUE\n" \
   "M ACCESS VISIBLE 1 ANY -\n" \
   "M ASSIGN VISIBLE 1 ANY - VALUE\n" \
   "M ACCESS X 1 ANY -\n" \
   "M ASSIGN X 1 ANY - VALUE\n" \
   "M ACCESS Y 1 ANY -\n" \
   "M ASSIGN Y 1 ANY - VALUE\n" \
   "M ACCESS WIDTH 1 ANY -\n" \
   "M ASSIGN WIDTH 1 ANY - VALUE\n" \
   "M ACCESS HEIGHT 1 ANY -\n" \
   "M ASSIGN HEIGHT 1 ANY - VALUE\n" \
   "M ACCESS GROW 1 ANY -\n" \
   "M ASSIGN GROW 1 ANY - VALUE\n" \
   "M METHOD SETFOCUS 1 ANY -\n" \
   "M METHOD DESTROY 1 ANY -\n" \
   "C WG_LABEL WG_CONTROL\n" \
   "M METHOD NEW 1 ANY - PARENT TEXT\n" \
   "C WG_BUTTON WG_CONTROL\n" \
   "M METHOD NEW 1 ANY - PARENT TEXT\n" \
   "C WG_EDIT WG_CONTROL\n" \
   "M METHOD NEW 1 ANY - PARENT VALUE LABEL\n" \
   "C WG_CHECKBOX WG_CONTROL\n" \
   "M METHOD NEW 1 ANY - PARENT TEXT VALUE\n" \
   "C WG_COMBOBOX WG_CONTROL\n" \
   "M METHOD NEW 1 ANY - PARENT ITEMS LABEL VALUE\n" \
   "C WG_BROWSE WG_CONTROL\n" \
   "M METHOD NEW 1 ANY - PARENT\n" \
   "M METHOD ADDCOLUMN 1 ANY - HEADING BLOCK WIDTH PICTURE ALIGN\n" \
   "M METHOD SETARRAY 1 ANY - ROWS\n" \
   "M METHOD REFRESH 1 ANY -\n" \
   "C WG_MENU WG_CONTROL\n" \
   "M METHOD NEW 1 ANY - PARENT TITLE ACTION\n" \
   "M METHOD ADD 1 ANY - TEXT ACTION SHORTCUT\n" \
   "M METHOD ADDSEPARATOR 1 ANY -\n" \
   "C WG_BOX WG_CONTROL\n" \
   "C WG_PANEL WG_BOX\n" \
   "C WG_COLUMN WG_BOX\n" \
   "M METHOD NEW 1 ANY - PARENT\n" \
   "C WG_ROW WG_BOX\n" \
   "M METHOD NEW 1 ANY - PARENT\n" \
   "C WG_SPLIT WG_CONTROL\n" \
   "M METHOD NEW 1 ANY - PARENT POSITION\n" \
   "M METHOD PANEL 1 ANY - PART\n" \
   "C WG_SIDEBAR WG_CONTROL\n" \
   "M METHOD NEW 1 ANY - PARENT WIDTH\n" \
   "M METHOD ADD 1 ANY - TITLE PICTURE TOOLTIP\n" \
   "M METHOD PANEL 1 ANY - PAGE\n" \
   "M METHOD ADDACTION 1 ANY - PAGE PICTURE ACTION TOOLTIP\n" \
   "M METHOD ADDBUTTON 1 ANY - PICTURE ACTION TOOLTIP\n" \
   "C WG_GET WG_CONTROL\n" \
   "M DATA VALID 1 CODEBLOCK -\n" \
   "M DATA WHEN 1 CODEBLOCK -\n" \
   "M METHOD NEW 1 ANY - PARENT VALUE PICTURE LABEL\n"

/* The class description in its own section of the object file, where the compiler finds it */
static const char s_interface[sizeof( X_WEBGUI_INTERFACE ) - 1]
   __attribute__( ( section( ".xdevcls" ), used, nonstring ) ) = X_WEBGUI_INTERFACE;

/* The runtime of the page, web/webgui.js, put into every page */
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wc23-extensions"
static const char s_runtime[] = {
#embed "../web/webgui.js"
   , 0
};
#pragma clang diagnostic pop

/* The page of a window without Open() */
static const wchar_t s_blank[] = L"<!doctype html><html><head><meta charset=\"utf-8\"></head><body></body></html>";
static const wchar_t s_className[] = L"xDev.WebGUI.Window";

static ATOM s_class;
static int s_com;
static HMODULE s_loader;
static X_WEBGUI_CREATE s_createEnvironment;
/* The window of the running Run() and the first failure of its browser */
static HWND s_mainWindow;
static HRESULT s_error;
static const char *s_operation;
/* The message s_dispatch() sends */
static const char *s_message;

/* ---------------------------------------------------------------------------------------------
   Arguments and errors
   --------------------------------------------------------------------------------------------- */

/* Raise the argument error of an operation */
void x_webguiRaise( X_CALL_CTX *ctx, const char *operation )
{
   x_errorRaiseNamed( ctx->result, 1102, operation );
}

/* Raise the error of a failed call of Windows or of WebView2 */
static void s_raiseCall( X_CALL_CTX *ctx, const char *operation, HRESULT error )
{
   X_ERROR_DEF definition = { 2, 20, ( int )error, 0, "WEBGUI", "Windows API call failed", operation };

   x_errorRaiseDefinition( ctx->result, &definition, NULL, NULL );
}

/* An argument as UTF-16 text; an error for a value that is not a string or holds a zero byte */
static wchar_t *s_text( X_CALL_CTX *ctx, const X_ITEM *item, const char *operation )
{
   void *handle = NULL;
   size_t length = 0;
   const char *text = IIF( item != NULL && X_IS_STRING( item ), x_itemGetStrUTF8( item, &handle, &length ), NULL );
   wchar_t *wide = NULL;

   if( text == NULL || memchr( text, '\0', length ) != NULL )
      x_webguiRaise( ctx, operation );
   else
      wide = ( wchar_t * )x_utf8ToUtf16Alloc( text, length, NULL );

   x_strfree( handle );

   return wide;
}

/* UTF-16 text of the browser as a string of xDev */
static void s_putText( X_ITEM *item, const wchar_t *text, size_t length )
{
   size_t size;
   char *utf8 = x_utf16ToUtf8Alloc( ( const uint16_t * )text, length, &size );

   x_itemPutCLPtr( item, utf8, size );
}

/* The browser of a window of the library, NULL for another window or a closed one */
static X_WEBGUI_VIEW *s_viewOf( HWND window )
{
   if( window == NULL || !IsWindow( window ) || ( ATOM )GetClassLongPtrW( window, GCW_ATOM ) != s_class )
      return NULL;

   return ( X_WEBGUI_VIEW * )GetWindowLongPtrW( window, GWLP_USERDATA );
}

/* The window of self; an error when it is closed */
HWND x_webguiSelfHandle( X_CALL_CTX *ctx, const char *operation )
{
   const X_ITEM *handle = x_objDataSlot( ctx, 1 );
   HWND window = IIF( handle != NULL, ( HWND )x_itemGetPtr( handle ), NULL );

   if( s_viewOf( window ) == NULL )
   {
      x_webguiRaise( ctx, operation );
      return NULL;
   }

   return window;
}

/* The window of a wg_Window object, read through its HANDLE message, or the window of a control through its
   parents; NULL for another value or a closed window */
static HWND s_objectWindow( X_CALL_CTX *ctx, const X_ITEM *object )
{
   X_ITEM handle __attribute__( ( cleanup( x_itemClear ) ) ) = X_ITEM_NIL_INIT;
   HWND window;

   if( object == NULL || !X_IS_OBJECT( object ) )
      return NULL;

   if( x_objIsKindOf( object, "WG_CONTROL" ) )
   {
      if( !x_objSend( ctx, &handle, ( X_ITEM * )object, "PARENT", 0, NULL ) )
         return NULL;

      return s_objectWindow( ctx, &handle );
   }

   if( !x_objIsKindOf( object, "WG_WINDOW" ) )
      return NULL;

   if( !x_objSend( ctx, &handle, ( X_ITEM * )object, "HANDLE", 0, NULL ) )
      return NULL;

   window = ( HWND )x_itemGetPtr( &handle );

   return IIF( s_viewOf( window ) != NULL, window, NULL );
}

/* ---------------------------------------------------------------------------------------------
   Commands for the page
   --------------------------------------------------------------------------------------------- */

/* Add bytes to the command */
static void s_jsonAdd( X_WEBGUI_JSON *json, const char *part, size_t length )
{
   if( json->length + length + 1 > json->capacity )
   {
      json->capacity = ( json->length + length + 1 ) * 2;
      json->data = ( char * )x_xrealloc( json->data, json->capacity );
   }

   memcpy( json->data + json->length, part, length );
   json->length += length;
   json->data[json->length] = '\0';
}

/* Add text to the command */
static void s_jsonText( X_WEBGUI_JSON *json, const char *part )
{
   s_jsonAdd( json, part, strlen( part ) );
}

/* Add a string of JSON: quotes, backslashes and control characters are escaped */
static void s_jsonString( X_WEBGUI_JSON *json, const char *text, size_t length )
{
   char escape[8];
   size_t i;

   s_jsonText( json, "\"" );
   for( i = 0; i < length; ++i )
   {
      unsigned char byte = ( unsigned char )text[i];

      if( byte == '"' || byte == '\\' )
      {
         escape[0] = '\\';
         escape[1] = ( char )byte;
         s_jsonAdd( json, escape, 2 );
      }
      else if( byte < 0x20 )
      {
         snprintf( escape, sizeof( escape ), "\\u%04x", byte );
         s_jsonText( json, escape );
      }
      else
         s_jsonAdd( json, text + i, 1 );
   }

   s_jsonText( json, "\"" );
}

/* Add a value of xDev: a string, a number, a logical value, an array of them, NIL as null */
static void s_jsonValue( X_WEBGUI_JSON *json, const X_ITEM *value )
{
   void *handle = NULL;
   size_t length = 0, i;
   const char *text;
   char number[32];

   if( X_IS_STRING( value ) )
   {
      text = x_itemGetStrUTF8( value, &handle, &length );
      s_jsonString( json, text, length );
      x_strfree( handle );
   }
   else if( X_IS_INTEGER( value ) )
   {
      snprintf( number, sizeof( number ), "%lld", ( long long )x_itemGetNInt( value ) );
      s_jsonText( json, number );
   }
   else if( X_IS_NUMERIC( value ) )
   {
      snprintf( number, sizeof( number ), "%.15g", x_itemGetND( value ) );
      s_jsonText( json, number );
   }
   else if( X_IS_LOGICAL( value ) )
      s_jsonText( json, IIF( x_itemGetL( value ), "true", "false" ) );
   else if( X_IS_ARRAY( value ) )
   {
      s_jsonText( json, "[" );
      for( i = 1; i <= ( size_t )x_arrayLen( value ); ++i )
      {
         if( i > 1 )
            s_jsonText( json, "," );

         s_jsonValue( json, x_arrayGetItemPtr( value, i ) );
      }

      s_jsonText( json, "]" );
   }
   else
      s_jsonText( json, "null" );
}

/* Start a command for control id: {"cmd":"<command>","id":<id> */
static void s_jsonCommand( X_WEBGUI_JSON *json, const char *command, int id )
{
   char number[16];

   s_jsonText( json, "{\"cmd\":\"" );
   s_jsonText( json, command );
   s_jsonText( json, "\",\"id\":" );
   snprintf( number, sizeof( number ), "%d", id );
   s_jsonText( json, number );
}

/* Add a property of the command; NIL leaves it out */
static void s_jsonField( X_WEBGUI_JSON *json, const char *name, const X_ITEM *value )
{
   if( value == NULL || X_IS_NIL( value ) )
      return;

   s_jsonText( json, ",\"" );
   s_jsonText( json, name );
   s_jsonText( json, "\":" );
   s_jsonValue( json, value );
}

/* The type of the value of a get for the page: C, N, D or L */
static const char *s_getType( const X_ITEM *value )
{
   if( X_IS_NUMERIC( value ) )
      return "N";

   if( X_IS_DATE( value ) )
      return "D";

   return IIF( X_IS_LOGICAL( value ), "L", "C" );
}

/* Add argument n of the running method as a property; a date goes as text YYYYMMDD, empty for an empty date */
static void s_jsonArgument( X_CALL_CTX *ctx, X_WEBGUI_JSON *json, const char *name, int n )
{
   char text[16] = "";
   int year, month, day;

   if( !X_ISDATE( ctx, n ) )
   {
      s_jsonField( json, name, x_paritem( ctx, n ) );
      return;
   }

   if( x_pardl( ctx, n ) != 0 )
   {
      x_dateDecode( x_pardl( ctx, n ), &year, &month, &day );
      snprintf( text, sizeof( text ), "%04d%02d%02d", year, month, day );
   }

   s_jsonText( json, ",\"" );
   s_jsonText( json, name );
   s_jsonText( json, "\":" );
   s_jsonString( json, text, strlen( text ) );
}

/* Post JSON text to the page, or keep it until the runtime of the page is ready; the text is taken */
static HRESULT s_post( X_WEBGUI_VIEW *view, wchar_t *json )
{
   HRESULT error = S_OK;

   if( !view->ready )
   {
      view->queue = ( wchar_t ** )x_xrealloc( view->queue, ( view->queued + 1 ) * sizeof( wchar_t * ) );
      view->queue[view->queued++] = json;
      return S_OK;
   }

   x_callbackEnterDll();
   error = view->webview->lpVtbl->PostWebMessageAsJson( view->webview, json );
   x_callbackLeaveDll();
   x_xfree( json );
   x_callbackRaisePending();

   return error;
}

/* Post a finished command to the page */
static void s_postCommand( X_WEBGUI_VIEW *view, X_WEBGUI_JSON *json )
{
   s_jsonText( json, "}" );
   s_post( view, ( wchar_t * )x_utf8ToUtf16Alloc( json->data, json->length, NULL ) );
   x_xfree( json->data );
}

/* The runtime of the page is ready: the waiting messages go to the page */
static void s_ready( X_WEBGUI_VIEW *view )
{
   size_t i;

   view->ready = 1;
   for( i = 0; i < view->queued; ++i )
   {
      view->webview->lpVtbl->PostWebMessageAsJson( view->webview, view->queue[i] );
      x_xfree( view->queue[i] );
   }

   x_xfree( view->queue );
   view->queue = NULL;
   view->queued = 0;
}

/* ---------------------------------------------------------------------------------------------
   Handlers of WebView2
   --------------------------------------------------------------------------------------------- */

/* A new handler with one reference, that of the caller */
static X_WEBGUI_HANDLER *s_handlerNew( const void *methods, HWND window )
{
   X_WEBGUI_HANDLER *handler = ( X_WEBGUI_HANDLER * )x_xgrab( sizeof( X_WEBGUI_HANDLER ) );

   handler->methods = methods;
   handler->references = 1;
   handler->window = window;

   return handler;
}

/* WebView2 asks only for the interface it was given */
static HRESULT STDMETHODCALLTYPE s_handlerQuery( X_WEBGUI_HANDLER *self, REFIID id, void **object )
{
   UNUSED( id );
   InterlockedIncrement( &self->references );
   *object = self;

   return S_OK;
}

/* Count one more reference */
static ULONG STDMETHODCALLTYPE s_handlerAddRef( X_WEBGUI_HANDLER *self )
{
   return ( ULONG )InterlockedIncrement( &self->references );
}

/* Count one reference less; the last one frees the handler */
static ULONG STDMETHODCALLTYPE s_handlerRelease( X_WEBGUI_HANDLER *self )
{
   LONG left = InterlockedDecrement( &self->references );

   if( left == 0 )
      x_xfree( self );

   return ( ULONG )left;
}

/* The first failure of the browser is kept for Run(); the window closes after the handler returns */
static HRESULT s_fail( HWND window, const char *operation, HRESULT error )
{
   if( s_operation == NULL )
   {
      s_operation = operation;
      s_error = error;
   }

   PostMessageW( window, WM_CLOSE, 0, 0 );

   return error;
}

/* The browser fills the client area of its window */
static void s_resize( HWND window, const X_WEBGUI_VIEW *view )
{
   RECT bounds;

   if( view->controller == NULL )
      return;

   GetClientRect( window, &bounds );
   view->controller->lpVtbl->put_Bounds( view->controller, bounds );
}

/* The browser shows the page of Open(), otherwise an empty page; messages wait until the new page is ready */
static void s_navigate( X_WEBGUI_VIEW *view )
{
   if( view->webview == NULL )
      return;

   view->ready = 0;
   if( view->uri != NULL )
      view->webview->lpVtbl->Navigate( view->webview, view->uri );
   else
      view->webview->lpVtbl->NavigateToString( view->webview, s_blank );
}

/* The title of the page becomes the title of the window */
static HRESULT STDMETHODCALLTYPE s_titleChanged( X_WEBGUI_HANDLER *self, ICoreWebView2 *sender, IUnknown *args )
{
   LPWSTR title;

   UNUSED( args );
   if( SUCCEEDED( sender->lpVtbl->get_DocumentTitle( sender, &title ) ) )
   {
      if( title[0] != L'\0' )
         SetWindowTextW( self->window, title );

      CoTaskMemFree( title );
   }

   return S_OK;
}

/* window.close() of the page closes the window */
static HRESULT STDMETHODCALLTYPE s_closeRequested( X_WEBGUI_HANDLER *self, ICoreWebView2 *sender, IUnknown *args )
{
   UNUSED( sender );
   UNUSED( args );
   PostMessageW( self->window, WM_CLOSE, 0, 0 );

   return S_OK;
}

/* Send s_message to the object in the first argument with the other arguments */
static void s_dispatch( X_CALL_CTX *ctx )
{
   x_objSend( ctx, ctx->result, ( X_ITEM * )ctx->args[0].value, s_message, ctx->argc - 1, ctx->args + 1 );
}

/* Send a message with up to two values to an object; an error in it ends Run() */
static void s_notify( X_ITEM *object, const char *message, int count, X_ITEM *values )
{
   X_ITEM result __attribute__( ( cleanup( x_itemClear ) ) ) = X_ITEM_NIL_INIT;
   X_ARG args[3] = { { object, 0 } };
   int i;

   for( i = 0; i < count; ++i )
      args[i + 1] = ( X_ARG ){ &values[i], 0 };

   s_message = message;
   if( !x_callbackInvoke( s_dispatch, &result, count + 1, args ) && s_mainWindow != NULL )
      PostQuitMessage( 1 );
}

/* An event of the runtime, <id>:<event>:<value>, goes to __WGEVENT of the control; "ready" sends the waiting
   messages */
static void s_runtimeEvent( X_WEBGUI_VIEW *view, const wchar_t *event )
{
   X_ITEM values[2] = { X_ITEM_NIL_INIT, X_ITEM_NIL_INIT };
   const wchar_t *name, *value;
   wchar_t *end;
   long id;

   if( wcscmp( event, L"ready" ) == 0 )
   {
      s_ready( view );
      return;
   }

   id = wcstol( event, &end, 10 );
   if( *end != L':' || id < 1 || ( size_t )id > view->controlCount || view->controls[id - 1] == NULL )
      return;

   name = end + 1;
   value = wcschr( name, L':' );
   if( value == NULL )
      return;

   s_putText( &values[0], name, ( size_t )( value - name ) );
   s_putText( &values[1], value + 1, wcslen( value + 1 ) );
   s_notify( view->controls[id - 1], "__WGEVENT", 2, values );
   x_itemClear( &values[0] );
   x_itemClear( &values[1] );
}

/* A message of the page: an event of the runtime, "wg:...", goes to its control, any other message goes as JSON
   text to OnMessage of the window */
static HRESULT STDMETHODCALLTYPE s_messageReceived( X_WEBGUI_HANDLER *self, ICoreWebView2 *sender, IUnknown *args )
{
   ICoreWebView2WebMessageReceivedEventArgs *message = ( ICoreWebView2WebMessageReceivedEventArgs * )args;
   X_WEBGUI_VIEW *view = ( X_WEBGUI_VIEW * )GetWindowLongPtrW( self->window, GWLP_USERDATA );
   X_ITEM text __attribute__( ( cleanup( x_itemClear ) ) ) = X_ITEM_NIL_INIT;
   LPWSTR received;

   UNUSED( sender );
   if( SUCCEEDED( message->lpVtbl->TryGetWebMessageAsString( message, &received ) ) )
   {
      if( wcsncmp( received, L"wg:", 3 ) == 0 )
      {
         s_runtimeEvent( view, received + 3 );
         CoTaskMemFree( received );
         return S_OK;
      }

      CoTaskMemFree( received );
   }

   if( FAILED( message->lpVtbl->get_WebMessageAsJson( message, &received ) ) )
      return S_OK;

   s_putText( &text, received, wcslen( received ) );
   CoTaskMemFree( received );
   s_notify( view->object, "__WGMESSAGE", 1, &text );

   return S_OK;
}

static const X_WEBGUI_EVENT s_titleMethods = { s_handlerQuery, s_handlerAddRef, s_handlerRelease, s_titleChanged };
static const X_WEBGUI_EVENT s_closeMethods = { s_handlerQuery, s_handlerAddRef, s_handlerRelease, s_closeRequested };
static const X_WEBGUI_EVENT s_messageMethods = { s_handlerQuery, s_handlerAddRef, s_handlerRelease,
                                                 s_messageReceived };

/* Give an event of the browser to a new handler; the browser keeps the handler until the event is removed */
static HRESULT s_listen( HWND window, X_WEBGUI_LISTEN event, EventRegistrationToken *token )
{
   X_WEBGUI_VIEW *view = ( X_WEBGUI_VIEW * )GetWindowLongPtrW( window, GWLP_USERDATA );
   ICoreWebView2 *webview = view->webview;
   X_WEBGUI_HANDLER *handler;
   HRESULT error;

   switch( event )
   {
   case X_WEBGUI_TITLE:
      handler = s_handlerNew( &s_titleMethods, window );
      error = webview->lpVtbl->add_DocumentTitleChanged( webview, ( void * )handler, token );
      break;

   case X_WEBGUI_CLOSE:
      handler = s_handlerNew( &s_closeMethods, window );
      error = webview->lpVtbl->add_WindowCloseRequested( webview, ( void * )handler, token );
      break;

   default:
      handler = s_handlerNew( &s_messageMethods, window );
      error = webview->lpVtbl->add_WebMessageReceived( webview, ( void * )handler, token );
      break;
   }

   s_handlerRelease( handler );

   return error;
}

/* Put the runtime of the library into every page of the browser */
static HRESULT s_addRuntime( ICoreWebView2 *webview )
{
   wchar_t *script = ( wchar_t * )x_utf8ToUtf16Alloc( s_runtime, sizeof( s_runtime ) - 1, NULL );
   HRESULT error = webview->lpVtbl->AddScriptToExecuteOnDocumentCreated( webview, script, NULL );

   x_xfree( script );

   return error;
}

/* The browser is ready: it fills the window, follows the title, the closing and the messages of the page, gets the
   runtime and shows the page; a window closed in the meantime closes the browser */
static HRESULT STDMETHODCALLTYPE s_controllerReady( X_WEBGUI_HANDLER *self, HRESULT result, void *made )
{
   ICoreWebView2Controller *controller = ( ICoreWebView2Controller * )made;
   X_WEBGUI_VIEW *view = s_viewOf( self->window );
   HRESULT error;

   if( FAILED( result ) )
      return s_fail( self->window, "CreateCoreWebView2Controller", result );

   if( view == NULL )
   {
      controller->lpVtbl->Close( controller );
      return S_OK;
   }

   controller->lpVtbl->AddRef( controller );
   view->controller = controller;
   error = controller->lpVtbl->get_CoreWebView2( controller, &view->webview );
   if( SUCCEEDED( error ) )
      error = s_listen( self->window, X_WEBGUI_TITLE, &view->titleToken );

   if( SUCCEEDED( error ) )
      error = s_listen( self->window, X_WEBGUI_CLOSE, &view->closeToken );

   if( SUCCEEDED( error ) )
      error = s_listen( self->window, X_WEBGUI_MESSAGE, &view->messageToken );

   if( SUCCEEDED( error ) )
      error = s_addRuntime( view->webview );

   if( FAILED( error ) )
      return s_fail( self->window, "ICoreWebView2", error );

   s_resize( self->window, view );
   s_navigate( view );

   return S_OK;
}

static const X_WEBGUI_MADE s_controllerMethods = { s_handlerQuery, s_handlerAddRef, s_handlerRelease,
                                                   s_controllerReady };

/* The environment of the browser is ready: it makes the browser of the window */
static HRESULT STDMETHODCALLTYPE s_environmentReady( X_WEBGUI_HANDLER *self, HRESULT result, void *made )
{
   ICoreWebView2Environment *environment = ( ICoreWebView2Environment * )made;
   X_WEBGUI_HANDLER *handler;
   HRESULT error;

   if( FAILED( result ) )
      return s_fail( self->window, "CreateCoreWebView2Environment", result );

   if( s_viewOf( self->window ) == NULL )
      return S_OK;

   handler = s_handlerNew( &s_controllerMethods, self->window );
   error = environment->lpVtbl->CreateCoreWebView2Controller( environment, self->window, ( void * )handler );
   s_handlerRelease( handler );
   if( FAILED( error ) )
      return s_fail( self->window, "CreateCoreWebView2Controller", error );

   return S_OK;
}

static const X_WEBGUI_MADE s_environmentMethods = { s_handlerQuery, s_handlerAddRef, s_handlerRelease,
                                                    s_environmentReady };

/* ---------------------------------------------------------------------------------------------
   Windows
   --------------------------------------------------------------------------------------------- */

/* Release the browser of a closed window: its events, the browser and its controller */
static void s_viewRelease( X_WEBGUI_VIEW *view )
{
   if( view->webview != NULL )
   {
      view->webview->lpVtbl->remove_DocumentTitleChanged( view->webview, view->titleToken );
      view->webview->lpVtbl->remove_WindowCloseRequested( view->webview, view->closeToken );
      view->webview->lpVtbl->remove_WebMessageReceived( view->webview, view->messageToken );
      view->webview->lpVtbl->Release( view->webview );
      view->webview = NULL;
   }

   if( view->controller != NULL )
   {
      view->controller->lpVtbl->Close( view->controller );
      view->controller->lpVtbl->Release( view->controller );
      view->controller = NULL;
   }
}

/* Free the view of a closed window: the waiting messages, the controls and the window object */
static void s_viewFree( X_WEBGUI_VIEW *view )
{
   size_t i;

   for( i = 0; i < view->queued; ++i )
      x_xfree( view->queue[i] );

   for( i = 0; i < view->controlCount; ++i )
   {
      if( view->controls[i] != NULL )
         x_itemRelease( view->controls[i] );
   }

   x_xfree( view->queue );
   x_xfree( view->controls );
   x_itemRelease( view->object );
   x_xfree( view->uri );
   x_xfree( view );
}

/* The window procedure: the browser follows the size of the window and is released with it */
static LRESULT CALLBACK s_windowProc( HWND window, UINT message, WPARAM wParam, LPARAM lParam )
{
   X_WEBGUI_VIEW *view = ( X_WEBGUI_VIEW * )GetWindowLongPtrW( window, GWLP_USERDATA );

   switch( message )
   {
   case WM_NCCREATE:
      SetWindowLongPtrW( window, GWLP_USERDATA, ( LONG_PTR )( ( const CREATESTRUCTW * )lParam )->lpCreateParams );
      break;

   case WM_SIZE:
      s_resize( window, view );
      return 0;

   case WM_DPICHANGED:
   {
      const RECT *frame = ( const RECT * )lParam;
      int width = frame->right - frame->left;
      int height = frame->bottom - frame->top;

      SetWindowPos( window, NULL, frame->left, frame->top, width, height, SWP_NOZORDER | SWP_NOACTIVATE );
      return 0;
   }

   case WM_DESTROY:
      s_viewRelease( view );
      if( window == s_mainWindow )
         PostQuitMessage( 0 );
      return 0;

   case WM_NCDESTROY:
      SetWindowLongPtrW( window, GWLP_USERDATA, 0 );
      s_viewFree( view );
      break;
   }

   return DefWindowProcW( window, message, wParam, lParam );
}

/* Register the window class once; WebView2 needs COM of one thread and the windows follow the DPI of their screen */
static int s_registerClass( X_CALL_CTX *ctx )
{
   WNDCLASSEXW windowClass;
   HRESULT error;

   if( s_class != 0 )
      return 1;

   if( !s_com )
   {
      error = CoInitializeEx( NULL, COINIT_APARTMENTTHREADED );
      if( FAILED( error ) )
      {
         s_raiseCall( ctx, "CoInitializeEx", error );
         return 0;
      }

      s_com = 1;
   }

   SetProcessDpiAwarenessContext( DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2 );
   memset( &windowClass, 0, sizeof( windowClass ) );
   windowClass.cbSize = sizeof( windowClass );
   windowClass.lpfnWndProc = s_windowProc;
   windowClass.hInstance = GetModuleHandleW( NULL );
   windowClass.hIcon = LoadIconW( windowClass.hInstance, MAKEINTRESOURCEW( 1 ) );
   windowClass.hCursor = LoadCursorW( NULL, IDC_ARROW );
   windowClass.hbrBackground = ( HBRUSH )( COLOR_WINDOW + 1 );
   windowClass.lpszClassName = s_className;
   if( windowClass.hIcon == NULL )
      windowClass.hIcon = LoadIconW( NULL, IDI_APPLICATION );

   s_class = RegisterClassExW( &windowClass );
   if( s_class == 0 )
   {
      s_raiseCall( ctx, "RegisterClassExW", HRESULT_FROM_WIN32( GetLastError() ) );
      return 0;
   }

   return 1;
}

/* wg_Window():New( [cTitle], [nWidth], [nHeight] ): a hidden window with a client area of nWidth x nHeight at
   96 DPI; Run() shows it */
void x_webguiWindowNew( X_CALL_CTX *ctx )
{
   static const DWORD style = WS_OVERLAPPEDWINDOW;
   X_ITEM *slot = x_objDataSlot( ctx, 1 );
   HINSTANCE instance = GetModuleHandleW( NULL );
   UINT dpi = GetDpiForSystem();
   int width = MulDiv( x_parnidef( ctx, 2, X_WEBGUI_WINDOW_WIDTH ), ( int )dpi, USER_DEFAULT_SCREEN_DPI );
   int height = MulDiv( x_parnidef( ctx, 3, X_WEBGUI_WINDOW_HEIGHT ), ( int )dpi, USER_DEFAULT_SCREEN_DPI );
   const int place = CW_USEDEFAULT;
   wchar_t *title = NULL;
   X_WEBGUI_VIEW *view;
   HWND window;
   RECT frame;
   DWORD error;
   int cx, cy;

   if( !X_ISNIL( ctx, 1 ) )
   {
      title = s_text( ctx, x_paritem( ctx, 1 ), "WG_WINDOW:NEW" );
      if( title == NULL )
         return;
   }

   if( !s_registerClass( ctx ) )
   {
      x_xfree( title );
      return;
   }

   frame = ( RECT ){ 0, 0, width, height };
   AdjustWindowRectExForDpi( &frame, style, FALSE, 0, dpi );
   cx = frame.right - frame.left;
   cy = frame.bottom - frame.top;
   view = ( X_WEBGUI_VIEW * )x_xgrab( sizeof( X_WEBGUI_VIEW ) );
   memset( view, 0, sizeof( *view ) );
   x_callbackEnterDll();
   window = CreateWindowExW( 0, s_className, IIF( title != NULL, title, L"" ), style, place, place, cx, cy, NULL, NULL,
                             instance, view );
   error = GetLastError();
   x_callbackLeaveDll();
   x_xfree( title );
   x_callbackRaisePending();
   if( window == NULL )
   {
      x_xfree( view );
      s_raiseCall( ctx, "CreateWindowExW", HRESULT_FROM_WIN32( error ) );
      return;
   }

   view->object = x_itemNew( ctx->self );
   x_itemPutPtr( slot, window );
   x_itemCopy( ctx->result, ctx->self );
}

/* The full path of an HTML file; a relative path counts from the folder of the program; NULL when there is no such
   file */
static wchar_t *s_pagePath( const wchar_t *name )
{
   wchar_t *program = ( wchar_t * )x_xgrabArray( 32768, sizeof( wchar_t ) );
   wchar_t *joined = NULL, *full = NULL, *slash;
   DWORD length, attributes;

   if( PathIsRelativeW( name ) && GetModuleFileNameW( NULL, program, 32768 ) < 32768 )
   {
      slash = wcsrchr( program, L'\\' );
      if( slash != NULL )
         slash[1] = L'\0';

      joined = ( wchar_t * )x_xgrabArray( wcslen( program ) + wcslen( name ) + 1, sizeof( wchar_t ) );
      wcscpy( joined, program );
      wcscat( joined, name );
      name = joined;
   }

   length = GetFullPathNameW( name, 0, NULL, NULL );
   if( length > 0 )
   {
      full = ( wchar_t * )x_xgrabArray( length, sizeof( wchar_t ) );
      GetFullPathNameW( name, length, full, NULL );
      attributes = GetFileAttributesW( full );
      if( attributes == INVALID_FILE_ATTRIBUTES || ( attributes & FILE_ATTRIBUTE_DIRECTORY ) )
      {
         x_xfree( full );
         full = NULL;
      }
   }

   x_xfree( joined );
   x_xfree( program );

   return full;
}

/* Set the page of the window, an HTML file; the browser shows it at once when it is ready; 0 when there is no such
   file */
int x_webguiWindowOpen( X_CALL_CTX *ctx, HWND window, const X_ITEM *file )
{
   X_WEBGUI_VIEW *view = s_viewOf( window );
   wchar_t *name = s_text( ctx, file, "WG_WINDOW:OPEN" );
   wchar_t *path = IIF( name != NULL, s_pagePath( name ), NULL );
   wchar_t *uri;
   DWORD length;

   x_xfree( name );
   if( path == NULL )
      return 0;

   /* An escaped character takes three characters in the URI */
   length = ( DWORD )wcslen( path ) * 3 + 16;
   uri = ( wchar_t * )x_xgrabArray( length, sizeof( wchar_t ) );
   if( FAILED( UrlCreateFromPathW( path, uri, &length, 0 ) ) )
   {
      x_xfree( path );
      x_xfree( uri );
      return 0;
   }

   x_xfree( path );
   x_xfree( view->uri );
   view->uri = uri;
   x_callbackEnterDll();
   s_navigate( view );
   x_callbackLeaveDll();
   x_callbackRaisePending();

   return 1;
}

/* Send JSON text to the page, which gets it as the data of a "message" event of window.chrome.webview; the text
   waits until the page is ready; an error for text that is not JSON */
void x_webguiWindowSend( X_CALL_CTX *ctx, HWND window, const X_ITEM *json )
{
   wchar_t *text = s_text( ctx, json, "WG_WINDOW:SEND" );
   HRESULT error;

   if( text == NULL )
      return;

   error = s_post( s_viewOf( window ), text );
   if( FAILED( error ) )
      s_raiseCall( ctx, "PostWebMessageAsJson", error );
}

/* Load WebView2Loader.dll once */
static int s_loadBrowser( X_CALL_CTX *ctx )
{
   FARPROC address;

   if( s_createEnvironment != NULL )
      return 1;

   s_loader = LoadLibraryW( L"WebView2Loader.dll" );
   if( s_loader == NULL )
   {
      s_raiseCall( ctx, "WebView2Loader.dll", HRESULT_FROM_WIN32( GetLastError() ) );
      return 0;
   }

   address = GetProcAddress( s_loader, "CreateCoreWebView2EnvironmentWithOptions" );
   s_createEnvironment = ( X_WEBGUI_CREATE )( uintptr_t )address;
   if( s_createEnvironment == NULL )
   {
      s_raiseCall( ctx, "CreateCoreWebView2EnvironmentWithOptions", HRESULT_FROM_WIN32( GetLastError() ) );
      return 0;
   }

   return 1;
}

/* The folder of the data of the browser, %LOCALAPPDATA%\xDev\WebView2; NULL for the folder next to the program */
static wchar_t *s_dataFolder( void )
{
   static const wchar_t tail[] = L"\\xDev\\WebView2";
   DWORD length = GetEnvironmentVariableW( L"LOCALAPPDATA", NULL, 0 );
   wchar_t *folder;

   if( length == 0 )
      return NULL;

   folder = ( wchar_t * )x_xgrabArray( length + sizeof( tail ) / sizeof( tail[0] ), sizeof( wchar_t ) );
   GetEnvironmentVariableW( L"LOCALAPPDATA", folder, length );
   wcscat( folder, tail );

   return folder;
}

/* Show the window, start its browser and run messages until the window closes; an error when the browser cannot
   start */
void x_webguiWindowRun( X_CALL_CTX *ctx, HWND window )
{
   HWND previous = s_mainWindow;
   X_WEBGUI_HANDLER *handler;
   wchar_t *folder;
   HRESULT error;
   MSG message;
   int status = 1;

   if( !s_loadBrowser( ctx ) )
      return;

   s_operation = NULL;
   s_error = S_OK;
   folder = s_dataFolder();
   handler = s_handlerNew( &s_environmentMethods, window );
   x_callbackEnterDll();
   ShowWindow( window, SW_SHOW );
   UpdateWindow( window );
   error = s_createEnvironment( NULL, folder, NULL, ( void * )handler );
   s_handlerRelease( handler );
   x_xfree( folder );
   if( FAILED( error ) )
      DestroyWindow( window );
   else
   {
      s_mainWindow = window;
      while( ( status = ( int )GetMessageW( &message, NULL, 0, 0 ) ) > 0 )
      {
         TranslateMessage( &message );
         DispatchMessageW( &message );
      }

      s_mainWindow = previous;
   }

   x_callbackLeaveDll();
   x_callbackRaisePending();

   if( FAILED( error ) )
      s_raiseCall( ctx, "CreateCoreWebView2EnvironmentWithOptions", error );
   else if( status == -1 )
      s_raiseCall( ctx, "GetMessageW", HRESULT_FROM_WIN32( GetLastError() ) );
   else if( s_operation != NULL )
      s_raiseCall( ctx, s_operation, s_error );
}

/* Close the window with its browser; a running window closes after the current event, which may come from the
   browser itself */
void x_webguiWindowClose( HWND window )
{
   x_callbackEnterDll();
   if( window == s_mainWindow )
      PostMessageW( window, WM_CLOSE, 0, 0 );
   else
      DestroyWindow( window );

   x_callbackLeaveDll();
   x_callbackRaisePending();
}

/* ---------------------------------------------------------------------------------------------
   Controls
   --------------------------------------------------------------------------------------------- */

/* Whether the first argument of New() of a control is its parent: a wg_Window or a box, a wg_Column, a wg_Row,
   a part of a wg_Split or a panel of a wg_Sidebar */
int x_webguiParentValid( X_CALL_CTX *ctx )
{
   const X_ITEM *parent = x_paritem( ctx, 1 );
   int valid = parent != NULL && X_IS_OBJECT( parent );

   return valid && ( x_objIsKindOf( parent, "WG_WINDOW" ) || x_objIsKindOf( parent, "WG_BOX" ) );
}

/* Create a control through __WgCreate() of the wg_Control class: the first argument of New() is its parent, a
   wg_Window or a box, then the text, the value and the items of the kind; the second argument of a sidebar is the
   width of its panel */
void x_webguiControlSend( X_CALL_CTX *ctx, const char *kind, const X_ITEM *text, const X_ITEM *value,
                          const X_ITEM *items )
{
   X_ITEM values[9];
   X_ARG args[9];
   int i;

   for( i = 0; i < 9; ++i )
      values[i] = ( X_ITEM )X_ITEM_NIL_INIT;

   x_itemPutC( &values[0], kind );
   if( x_paritem( ctx, 1 ) != NULL )
      x_itemCopy( &values[1], x_paritem( ctx, 1 ) );

   if( strcmp( kind, "sidebar" ) == 0 && x_paritem( ctx, 2 ) != NULL )
      x_itemCopy( &values[4], x_paritem( ctx, 2 ) );

   if( text != NULL )
      x_itemCopy( &values[6], text );

   if( value != NULL )
      x_itemCopy( &values[7], value );

   if( items != NULL )
      x_itemCopy( &values[8], items );

   for( i = 0; i < 9; ++i )
      args[i] = ( X_ARG ){ &values[i], 0 };

   x_objSend( ctx, ctx->result, ctx->self, "__WGCREATE", 9, args );
   for( i = 0; i < 9; ++i )
      x_itemClear( &values[i] );
}

/* __WgCreate( cKind, oWindow, nX, nY, nWidth, nHeight, cText, xValue, aItems ): the control gets the next number
   of its window, which keeps it while the window lives, and the page draws it; the value of a menu is the block
   of its title, OnClick */
void x_webguiControlCreate( X_CALL_CTX *ctx )
{
   HWND window = s_objectWindow( ctx, x_paritem( ctx, 2 ) );
   X_WEBGUI_VIEW *view = s_viewOf( window );
   X_WEBGUI_JSON json = { NULL, 0, 0 };
   static const char *const places[] = { "x", "y", "width", "height" };
   X_ITEM parent __attribute__( ( cleanup( x_itemClear ) ) ) = X_ITEM_NIL_INIT;
   int menu = strcmp( x_parc( ctx, 1 ), "menu" ) == 0;
   int id, i;

   if( window == NULL )
   {
      x_webguiRaise( ctx, "WG_CONTROL:NEW" );
      return;
   }

   view->controls = ( X_ITEM ** )x_xrealloc( view->controls, ( view->controlCount + 1 ) * sizeof( X_ITEM * ) );
   view->controls[view->controlCount++] = x_itemNew( ctx->self );
   id = ( int )view->controlCount;

   x_itemCopy( x_objDataSlot( ctx, X_WEBGUI_PARENT ), x_paritem( ctx, 2 ) );
   x_itemPutNI( x_objDataSlot( ctx, X_WEBGUI_ID ), id );
   x_itemCopy( x_objDataSlot( ctx, X_WEBGUI_KIND ), x_paritem( ctx, 1 ) );
   x_itemCopy( x_objDataSlot( ctx, X_WEBGUI_TEXT ), x_paritem( ctx, 7 ) );
   x_itemCopy( x_objDataSlot( ctx, IIF( menu, X_WEBGUI_ONCLICK, X_WEBGUI_VALUE ) ), x_paritem( ctx, 8 ) );
   x_itemPutL( x_objDataSlot( ctx, X_WEBGUI_ENABLED ), 1 );
   x_itemPutL( x_objDataSlot( ctx, X_WEBGUI_VISIBLE ), 1 );
   x_itemPutL( x_objDataSlot( ctx, X_WEBGUI_GROW ), 0 );
   x_arrayNew( x_objDataSlot( ctx, X_WEBGUI_ACTIONS ), 0 );

   s_jsonCommand( &json, "create", id );
   s_jsonField( &json, "kind", x_paritem( ctx, 1 ) );
   if( x_objIsKindOf( x_paritem( ctx, 2 ), "WG_CONTROL" ) )
   {
      x_objSend( ctx, &parent, ( X_ITEM * )x_paritem( ctx, 2 ), "ID", 0, NULL );
      s_jsonField( &json, "parent", &parent );
   }

   for( i = 0; i < 4; ++i )
      s_jsonField( &json, places[i], x_paritem( ctx, i + 3 ) );

   s_jsonField( &json, "text", x_paritem( ctx, 7 ) );
   if( !menu )
      s_jsonArgument( ctx, &json, "value", 8 );

   /* A get gets the type of its value, C, N, D or L, and its picture in place of the items */
   if( strcmp( x_parc( ctx, 1 ), "get" ) == 0 )
   {
      s_jsonText( &json, ",\"type\":\"" );
      s_jsonText( &json, s_getType( x_paritem( ctx, 8 ) ) );
      s_jsonText( &json, "\"" );
      s_jsonField( &json, "picture", x_paritem( ctx, 9 ) );
   }
   else
      s_jsonField( &json, "items", x_paritem( ctx, 9 ) );

   s_postCommand( view, &json );
   x_itemCopy( ctx->result, ctx->self );
}

/* The view of the window of self, a control; an error for a destroyed control or a closed window */
static X_WEBGUI_VIEW *s_controlView( X_CALL_CTX *ctx, const char *operation )
{
   HWND window = s_objectWindow( ctx, x_objDataSlot( ctx, X_WEBGUI_PARENT ) );
   const X_ITEM *id = x_objDataSlot( ctx, X_WEBGUI_ID );

   if( window == NULL || id == NULL || x_itemGetNInt( id ) < 1 )
   {
      x_webguiRaise( ctx, operation );
      return NULL;
   }

   return s_viewOf( window );
}

/* Keep the value of a property in its data and set it on the page; NIL goes as null, which takes the property
   away, e.g. a width */
void x_webguiControlSet( X_CALL_CTX *ctx, const char *property, int slot, const char *operation )
{
   X_WEBGUI_VIEW *view = s_controlView( ctx, operation );
   X_WEBGUI_JSON json = { NULL, 0, 0 };

   if( view == NULL )
      return;

   x_itemCopy( x_objDataSlot( ctx, slot ), x_paritem( ctx, 1 ) );
   s_jsonCommand( &json, "set", ( int )x_itemGetNInt( x_objDataSlot( ctx, X_WEBGUI_ID ) ) );
   if( X_ISNIL( ctx, 1 ) )
   {
      s_jsonText( &json, ",\"" );
      s_jsonText( &json, property );
      s_jsonText( &json, "\":null" );
   }
   else
      s_jsonArgument( ctx, &json, property, 1 );
   s_postCommand( view, &json );
   x_itemReturn( ctx, x_paritem( ctx, 1 ) );
}

/* Give the focus to the control, or destroy it: the page removes it and its window lets it go */
void x_webguiControlCommand( X_CALL_CTX *ctx, int destroy )
{
   const char *operation = IIF( destroy, "WG_CONTROL:DESTROY", "WG_CONTROL:SETFOCUS" );
   X_WEBGUI_VIEW *view = s_controlView( ctx, operation );
   X_ITEM *slot = x_objDataSlot( ctx, X_WEBGUI_ID );
   X_WEBGUI_JSON json = { NULL, 0, 0 };
   int id;

   if( view == NULL )
      return;

   id = ( int )x_itemGetNInt( slot );
   s_jsonCommand( &json, IIF( destroy, "destroy", "focus" ), id );
   s_postCommand( view, &json );
   x_itemCopy( ctx->result, ctx->self );
   if( destroy )
   {
      x_itemPutNI( slot, 0 );
      x_itemRelease( view->controls[id - 1] );
      view->controls[id - 1] = NULL;
   }
}

/* The value of a get from the text of the page, by the type the value has: a date YYYYMMDD or empty, a number,
   T or F, text */
static void s_getValue( X_CALL_CTX *ctx, X_ITEM *value, const X_ITEM *text )
{
   const char *received = x_itemGetCPtr( text );
   int year, month, day;
   long julian = 0;

   if( X_IS_DATE( value ) )
   {
      if( sscanf( received, "%4d%2d%2d", &year, &month, &day ) == 3 )
         julian = x_dateEncode( year, month, day );

      x_retdl( ctx, julian );
      x_itemCopy( value, ctx->result );
   }
   else if( X_IS_NUMERIC( value ) && strchr( received, '.' ) != NULL )
      x_itemPutND( value, atof( received ) );
   else if( X_IS_NUMERIC( value ) )
      x_itemPutNInt( value, atoll( received ) );
   else if( X_IS_LOGICAL( value ) )
      x_itemPutL( value, received[0] == 'T' );
   else
      x_itemCopy( value, text );
}

/* Whether the block of a get read by message, VALID or WHEN, lets it go: no block or .T.; the block receives the
   get and for VALID the new value */
static int s_getAllows( X_CALL_CTX *ctx, const char *message, X_ITEM *value )
{
   X_ITEM block __attribute__( ( cleanup( x_itemClear ) ) ) = X_ITEM_NIL_INIT;
   X_ITEM result __attribute__( ( cleanup( x_itemClear ) ) ) = X_ITEM_NIL_INIT;
   X_ARG args[2];

   if( !x_objSend( ctx, &block, ctx->self, message, 0, NULL ) || !X_IS_BLOCK( &block ) )
      return 1;

   args[0] = ( X_ARG ){ ctx->self, 0 };
   args[1] = ( X_ARG ){ value, 0 };
   x_evalBlock( ctx, &result, &block, IIF( value != NULL, 2, 1 ), args );

   return X_IS_LOGICAL( &result ) && x_itemGetL( &result );
}

/* Tell the page that a get refused: "invalid" marks the text and gives the get the focus back, "skip" passes the
   focus on to the next field */
static void s_getRefuse( X_CALL_CTX *ctx, const char *command )
{
   X_WEBGUI_VIEW *view = s_controlView( ctx, "WG_GET" );
   X_WEBGUI_JSON json = { NULL, 0, 0 };

   if( view == NULL )
      return;

   s_jsonCommand( &json, command, ( int )x_itemGetNInt( x_objDataSlot( ctx, X_WEBGUI_ID ) ) );
   s_postCommand( view, &json );
}

/* __WgEvent( cEvent, cValue ): "change" keeps the value the user gave, as text, a logical value, a number or a date
   by the kind of the control, and runs OnChange; the new value of a get must pass its VALID first; "focus" of a
   get without WHEN passes the focus on; "click" runs OnClick; "item" runs the block of the item of a menu with the
   number cValue; the block receives the control */
void x_webguiControlEvent( X_CALL_CTX *ctx )
{
   const char *kind = x_itemGetCPtr( x_objDataSlot( ctx, X_WEBGUI_KIND ) );
   X_ITEM *value = x_objDataSlot( ctx, X_WEBGUI_VALUE );
   X_ITEM block __attribute__( ( cleanup( x_itemClear ) ) ) = X_ITEM_NIL_INIT;
   X_ITEM result __attribute__( ( cleanup( x_itemClear ) ) ) = X_ITEM_NIL_INIT;
   X_ITEM fresh __attribute__( ( cleanup( x_itemClear ) ) ) = X_ITEM_NIL_INIT;
   int change = strcmp( x_parc( ctx, 1 ), "change" ) == 0;
   int get = strcmp( kind, "get" ) == 0;
   int numbered = strcmp( kind, "combo" ) == 0 || strcmp( kind, "browse" ) == 0 || strcmp( kind, "sidebar" ) == 0;

   numbered = numbered || strcmp( kind, "split" ) == 0;
   X_ARG args[1];

   if( get && strcmp( x_parc( ctx, 1 ), "focus" ) == 0 )
   {
      if( !s_getAllows( ctx, "WHEN", NULL ) )
         s_getRefuse( ctx, "skip" );

      return;
   }

   if( change && get )
   {
      x_itemCopy( &fresh, value );
      s_getValue( ctx, &fresh, x_paritem( ctx, 2 ) );
      if( !s_getAllows( ctx, "VALID", &fresh ) )
      {
         s_getRefuse( ctx, "invalid" );
         return;
      }

      x_itemCopy( value, &fresh );
   }
   else if( change && strcmp( kind, "check" ) == 0 )
      x_itemPutL( value, x_parc( ctx, 2 )[0] == 'T' );
   else if( change && numbered )
      x_itemPutNI( value, atoi( x_parc( ctx, 2 ) ) );
   else if( change )
      x_itemCopy( value, x_paritem( ctx, 2 ) );

   if( strcmp( x_parc( ctx, 1 ), "item" ) == 0 )
      x_arrayGet( x_objDataSlot( ctx, X_WEBGUI_ACTIONS ), ( size_t )atoi( x_parc( ctx, 2 ) ), &block );
   else
      x_itemCopy( &block, x_objDataSlot( ctx, IIF( change, X_WEBGUI_ONCHANGE, X_WEBGUI_ONCLICK ) ) );

   if( !X_IS_BLOCK( &block ) )
      return;

   args[0] = ( X_ARG ){ ctx->self, 0 };
   x_evalBlock( ctx, &result, &block, 1, args );
}

/* Send a message of the wg_Control class to self with the first count arguments of the running method */
void x_webguiForward( X_CALL_CTX *ctx, const char *message, int count )
{
   X_ITEM nil = X_ITEM_NIL_INIT;
   const X_ITEM *value;
   X_ARG args[5];
   int i;

   for( i = 0; i < count; ++i )
   {
      value = x_paritem( ctx, i + 1 );
      args[i] = ( X_ARG ){ ( X_ITEM * )IIF( value != NULL, value, &nil ), 0 };
   }

   x_objSend( ctx, ctx->result, ctx->self, message, count, args );
}

/* ---------------------------------------------------------------------------------------------
   Browses
   --------------------------------------------------------------------------------------------- */

/* __WgColumn( cHeading, bBlock, nWidth, cPicture, nAlign ): add the column { cHeading, bBlock, nWidth, cPicture,
   nAlign }; returns its number */
void x_webguiBrowseColumn( X_CALL_CTX *ctx )
{
   X_ITEM *columns = x_objDataSlot( ctx, X_WEBGUI_COLUMNS );
   X_ITEM column __attribute__( ( cleanup( x_itemClear ) ) ) = X_ITEM_NIL_INIT;
   int i;

   if( !X_IS_ARRAY( columns ) )
      x_arrayNew( columns, 0 );

   x_arrayNew( &column, 5 );
   for( i = 1; i <= 5; ++i )
      x_itemCopy( x_arrayGetItemPtr( &column, ( size_t )i ), x_paritem( ctx, i ) );

   x_arrayAdd( columns, &column );
   x_retni( ctx, ( int )x_arrayLen( columns ) );
}

/* __WgRows( aRows ): the rows of the browse, the array itself, so Refresh() shows its later changes; the first row
   becomes the current one */
void x_webguiBrowseRows( X_CALL_CTX *ctx )
{
   const X_ITEM *rows = x_paritem( ctx, 1 );

   x_itemCopy( x_objDataSlot( ctx, X_WEBGUI_ROWS ), rows );
   x_itemPutNI( x_objDataSlot( ctx, X_WEBGUI_VALUE ), IIF( x_arrayLen( rows ) > 0, 1, 0 ) );
   x_webguiBrowseRefresh( ctx );
}

/* The text of a cell: the value through Transform() with the picture of the column, without the spaces around it */
static void s_cellText( X_ITEM *shown, const X_ITEM *value, const X_ITEM *column )
{
   const X_ITEM *picture = x_arrayGetItemPtr( column, 4 );
   X_ITEM formatted __attribute__( ( cleanup( x_itemClear ) ) ) = X_ITEM_NIL_INIT;
   int valid = X_IS_STRING( value ) || X_IS_NUMERIC( value ) || X_IS_DATE( value ) || X_IS_LOGICAL( value );
   const char *text;
   size_t length;

   if( !valid )
   {
      x_itemPutC( shown, "" );
      return;
   }

   if( X_IS_STRING( picture ) )
      x_transform( &formatted, value, x_itemGetCPtr( picture ), x_itemGetCLen( picture ) );
   else
      x_transform( &formatted, value, "", 0 );

   text = x_itemGetCPtr( &formatted );
   length = x_itemGetCLen( &formatted );
   while( length > 0 && *text == ' ' )
   {
      ++text;
      --length;
   }

   while( length > 0 && text[length - 1] == ' ' )
      --length;

   x_itemPutCL( shown, text, length );
}

/* __WgRefresh(): the columns and the texts of all cells go to the page; the block of a column receives the row and
   its number; a column without nAlign stands on the right when the first row holds a number there */
void x_webguiBrowseRefresh( X_CALL_CTX *ctx )
{
   X_WEBGUI_VIEW *view = s_controlView( ctx, "WG_BROWSE:REFRESH" );
   const X_ITEM *columns = x_objDataSlot( ctx, X_WEBGUI_COLUMNS );
   const X_ITEM *rows = x_objDataSlot( ctx, X_WEBGUI_ROWS );
   X_ITEM block __attribute__( ( cleanup( x_itemClear ) ) ) = X_ITEM_NIL_INIT;
   X_ITEM value __attribute__( ( cleanup( x_itemClear ) ) ) = X_ITEM_NIL_INIT;
   X_ITEM shown __attribute__( ( cleanup( x_itemClear ) ) ) = X_ITEM_NIL_INIT;
   X_ITEM number __attribute__( ( cleanup( x_itemClear ) ) ) = X_ITEM_NIL_INIT;
   X_WEBGUI_JSON json = { NULL, 0, 0 };
   X_WEBGUI_JSON cells = { NULL, 0, 0 };
   size_t columnCount, rowCount, row, i;
   const X_ITEM *column, *align;
   char *right;
   char text[16];
   X_ARG args[2];

   if( view == NULL )
      return;

   columnCount = IIF( X_IS_ARRAY( columns ), ( size_t )x_arrayLen( columns ), 0 );
   rowCount = IIF( X_IS_ARRAY( rows ), ( size_t )x_arrayLen( rows ), 0 );
   right = ( char * )x_xgrabz( columnCount + 1 );
   s_jsonText( &cells, "" );
   for( row = 1; row <= rowCount; ++row )
   {
      s_jsonText( &cells, IIF( row > 1, ",[", "[" ) );
      x_itemPutNI( &number, ( int )row );
      for( i = 1; i <= columnCount; ++i )
      {
         column = x_arrayGetItemPtr( columns, i );
         x_arrayGet( column, 2, &block );
         x_itemClear( &value );
         args[0] = ( X_ARG ){ x_arrayGetItemPtr( rows, row ), 0 };
         args[1] = ( X_ARG ){ &number, 0 };
         x_evalBlock( ctx, &value, &block, 2, args );
         if( row == 1 )
            right[i - 1] = ( char )X_IS_NUMERIC( &value );

         s_cellText( &shown, &value, column );
         if( i > 1 )
            s_jsonText( &cells, "," );

         s_jsonValue( &cells, &shown );
      }

      s_jsonText( &cells, "]" );
   }

   s_jsonCommand( &json, "set", ( int )x_itemGetNInt( x_objDataSlot( ctx, X_WEBGUI_ID ) ) );
   s_jsonText( &json, ",\"columns\":[" );
   for( i = 1; i <= columnCount; ++i )
   {
      column = x_arrayGetItemPtr( columns, i );
      align = x_arrayGetItemPtr( column, 5 );
      snprintf( text, sizeof( text ), "%d", IIF( X_IS_NUMERIC( align ), ( int )x_itemGetNInt( align ), right[i - 1] ) );
      s_jsonText( &json, IIF( i > 1, ",{\"heading\":", "{\"heading\":" ) );
      s_jsonValue( &json, x_arrayGetItemPtr( column, 1 ) );
      s_jsonText( &json, ",\"width\":" );
      s_jsonValue( &json, x_arrayGetItemPtr( column, 3 ) );
      s_jsonText( &json, ",\"align\":" );
      s_jsonText( &json, text );
      s_jsonText( &json, "}" );
   }

   s_jsonText( &json, "],\"rows\":[" );
   s_jsonAdd( &json, cells.data, cells.length );
   s_jsonText( &json, "]" );
   s_jsonField( &json, "value", x_objDataSlot( ctx, X_WEBGUI_VALUE ) );
   s_postCommand( view, &json );
   x_xfree( cells.data );
   x_xfree( right );
   x_itemCopy( ctx->result, ctx->self );
}

/* ---------------------------------------------------------------------------------------------
   Menus
   --------------------------------------------------------------------------------------------- */

/* __WgItem( cText, bAction, cShortcut ): add an item to the menu, a separator for NIL cText; the page shows
   cShortcut at its right and the key runs the item as a click */
void x_webguiMenuItem( X_CALL_CTX *ctx )
{
   X_WEBGUI_VIEW *view = s_controlView( ctx, "WG_MENU:ADD" );
   X_WEBGUI_JSON json = { NULL, 0, 0 };

   if( view == NULL )
      return;

   x_arrayAdd( x_objDataSlot( ctx, X_WEBGUI_ACTIONS ), x_paritem( ctx, 2 ) );
   s_jsonCommand( &json, "item", ( int )x_itemGetNInt( x_objDataSlot( ctx, X_WEBGUI_ID ) ) );
   s_jsonField( &json, "text", x_paritem( ctx, 1 ) );
   s_jsonField( &json, "shortcut", x_paritem( ctx, 3 ) );
   s_postCommand( view, &json );
   x_itemCopy( ctx->result, ctx->self );
}

/* ---------------------------------------------------------------------------------------------
   Sidebars
   --------------------------------------------------------------------------------------------- */

/* A picture as a data URI for the page: put into the program by xdev under its name or under the name of its file
   without the folder and the extension, otherwise read from the file; its type by the extension: PNG, SVG, JPEG,
   GIF or BMP; NULL when there is no such picture */
static char *s_pictureUri( const X_ITEM *file )
{
   static const char *const types[][2] = { { ".svg", "image/svg+xml" }, { ".jpg", "image/jpeg" },
                                          { ".jpeg", "image/jpeg" }, { ".gif", "image/gif" }, { ".bmp", "image/bmp" } };
   const char *name = IIF( X_IS_STRING( file ), x_itemGetCPtr( file ), "" );
   const char *start = name, *dot, *type = "image/png";
   const unsigned char *found;
   char *loaded = NULL, *encoded, *uri;
   size_t size = 0, length, i;
   int stemLength;
   char stem[260];

   for( dot = name; *dot != '\0'; ++dot )
   {
      if( *dot == '\\' || *dot == '/' )
         start = dot + 1;
   }

   dot = strrchr( start, '.' );
   for( i = 0; dot != NULL && i < sizeof( types ) / sizeof( types[0] ); ++i )
   {
      if( lstrcmpiA( dot, types[i][0] ) == 0 )
         type = types[i][1];
   }

   found = x_resourceFind( name, &size );
   if( found == NULL )
   {
      stemLength = ( int )IIF( dot != NULL, dot - start, ( ptrdiff_t )strlen( start ) );
      snprintf( stem, sizeof( stem ), "%.*s", stemLength, start );
      found = x_resourceFind( stem, &size );
   }

   if( found == NULL )
   {
      loaded = x_fileLoad( name, ( size_t )16 * 1024 * 1024, &size );
      if( loaded == NULL )
         return NULL;

      found = ( const unsigned char * )loaded;
   }

   encoded = x_base64Encode( ( const char * )found, size, &length );
   uri = ( char * )x_xgrab( strlen( type ) + length + 16 );
   sprintf( uri, "data:%s;base64,%s", type, encoded );
   x_xfree( encoded );
   x_xfree( loaded );

   return uri;
}

/* oSidebar:Add( cTitle, cPicture, [cToolTip] ): a page whose panel is a new wg_Panel, a control of the window of the
   sidebar; the page draws the picture in the strip and the panel under the header with cTitle; returns the number
   of the page, 0 with an error raised */
int x_webguiSidebarAdd( X_CALL_CTX *ctx, X_ITEM *panel )
{
   X_ITEM *pages = x_objDataSlot( ctx, 1 );
   const X_ITEM *tip = IIF( X_ISNIL( ctx, 3 ), x_paritem( ctx, 1 ), x_paritem( ctx, 3 ) );
   X_ITEM result __attribute__( ( cleanup( x_itemClear ) ) ) = X_ITEM_NIL_INIT;
   char *uri = s_pictureUri( x_paritem( ctx, 2 ) );
   X_ITEM values[9];
   X_ARG args[9];
   int i;

   if( uri == NULL )
   {
      x_webguiRaise( ctx, "WG_SIDEBAR:ADD" );
      return 0;
   }

   for( i = 0; i < 9; ++i )
   {
      values[i] = ( X_ITEM )X_ITEM_NIL_INIT;
      args[i] = ( X_ARG ){ &values[i], 0 };
   }

   x_itemPutC( &values[0], "panel" );
   x_objSend( ctx, &values[1], ctx->self, "PARENT", 0, NULL );
   x_itemCopy( &values[6], x_paritem( ctx, 1 ) );
   x_objSend( ctx, &values[7], ctx->self, "ID", 0, NULL );
   x_arrayNew( &values[8], 2 );
   x_itemPutCLPtr( x_arrayGetItemPtr( &values[8], 1 ), uri, strlen( uri ) );
   x_itemCopy( x_arrayGetItemPtr( &values[8], 2 ), tip );
   x_objSend( ctx, &result, panel, "__WGCREATE", 9, args );
   for( i = 0; i < 9; ++i )
      x_itemClear( &values[i] );

   if( !X_IS_ARRAY( pages ) )
      x_arrayNew( pages, 0 );

   x_arrayAdd( pages, panel );

   return ( int )x_arrayLen( pages );
}

/* Return the panel of page nPage */
void x_webguiSidebarPanel( X_CALL_CTX *ctx, int page )
{
   const X_ITEM *pages = x_objDataSlot( ctx, 1 );

   if( !X_IS_ARRAY( pages ) || page < 1 || page > ( int )x_arrayLen( pages ) )
   {
      x_webguiRaise( ctx, "WG_SIDEBAR:PANEL" );
      return;
   }

   x_itemReturn( ctx, x_arrayGetItemPtr( pages, ( size_t )page ) );
}

/* Send __WgAction( nPage, cPicture, bAction, cToolTip ) to self: an action of page nPage, from AddAction(), or a
   button at the bottom of the strip, page 0, from AddButton(); first is the argument with cPicture */
void x_webguiSidebarButton( X_CALL_CTX *ctx, int first )
{
   X_ITEM values[4] = { X_ITEM_NIL_INIT, X_ITEM_NIL_INIT, X_ITEM_NIL_INIT, X_ITEM_NIL_INIT };
   X_ARG args[4];
   int i;

   x_itemPutNI( &values[0], IIF( first == 2, x_parni( ctx, 1 ), 0 ) );
   for( i = 1; i < 4; ++i )
   {
      if( x_paritem( ctx, first + i - 1 ) != NULL )
         x_itemCopy( &values[i], x_paritem( ctx, first + i - 1 ) );
   }

   for( i = 0; i < 4; ++i )
      args[i] = ( X_ARG ){ &values[i], 0 };

   x_objSend( ctx, ctx->result, ctx->self, "__WGACTION", 4, args );
   for( i = 0; i < 4; ++i )
      x_itemClear( &values[i] );
}

/* __WgAction( nPage, cPicture, bAction, cToolTip ): the block goes to the items of the control and the page draws
   the picture: in the header of page nPage, at the bottom of the strip for 0 */
void x_webguiSidebarAction( X_CALL_CTX *ctx )
{
   X_WEBGUI_VIEW *view = s_controlView( ctx, "WG_SIDEBAR:ADDACTION" );
   X_ITEM *actions = x_objDataSlot( ctx, X_WEBGUI_ACTIONS );
   X_WEBGUI_JSON json = { NULL, 0, 0 };
   char *uri;
   char number[16];

   if( view == NULL )
      return;

   uri = s_pictureUri( x_paritem( ctx, 2 ) );
   if( uri == NULL )
   {
      x_webguiRaise( ctx, "WG_SIDEBAR:ADDACTION" );
      return;
   }

   x_arrayAdd( actions, x_paritem( ctx, 3 ) );
   snprintf( number, sizeof( number ), "%d", ( int )x_arrayLen( actions ) );
   s_jsonCommand( &json, "action", ( int )x_itemGetNInt( x_objDataSlot( ctx, X_WEBGUI_ID ) ) );
   s_jsonField( &json, "page", x_paritem( ctx, 1 ) );
   s_jsonText( &json, ",\"number\":" );
   s_jsonText( &json, number );
   s_jsonText( &json, ",\"picture\":" );
   s_jsonString( &json, uri, strlen( uri ) );
   s_jsonField( &json, "tip", x_paritem( ctx, 4 ) );
   s_postCommand( view, &json );
   x_xfree( uri );
   x_itemCopy( ctx->result, ctx->self );
}

/* ---------------------------------------------------------------------------------------------
   Splits
   --------------------------------------------------------------------------------------------- */

/* wg_Split():New(): its two parts, new wg_Column objects, become controls of the split, the first at the left and
   the second at the right, and the split keeps them for Panel() */
void x_webguiSplitParts( X_CALL_CTX *ctx, X_ITEM *first, X_ITEM *second )
{
   X_ITEM *parts = x_objDataSlot( ctx, 1 );
   X_ITEM result __attribute__( ( cleanup( x_itemClear ) ) ) = X_ITEM_NIL_INIT;
   X_ITEM *children[2] = { first, second };
   X_ITEM values[9];
   X_ARG args[9];
   int i, part;

   for( part = 0; part < 2; ++part )
   {
      for( i = 0; i < 9; ++i )
      {
         values[i] = ( X_ITEM )X_ITEM_NIL_INIT;
         args[i] = ( X_ARG ){ &values[i], 0 };
      }

      x_itemPutC( &values[0], "column" );
      x_itemCopy( &values[1], ctx->self );
      x_objSend( ctx, &result, children[part], "__WGCREATE", 9, args );
      for( i = 0; i < 9; ++i )
         x_itemClear( &values[i] );
   }

   x_arrayNew( parts, 0 );
   x_arrayAdd( parts, first );
   x_arrayAdd( parts, second );
}

/* Return part nPart of a split, 1 at the left, 2 at the right */
void x_webguiSplitPanel( X_CALL_CTX *ctx, int part )
{
   const X_ITEM *parts = x_objDataSlot( ctx, 1 );

   if( !X_IS_ARRAY( parts ) || part < 1 || part > 2 )
   {
      x_webguiRaise( ctx, "WG_SPLIT:PANEL" );
      return;
   }

   x_itemReturn( ctx, x_arrayGetItemPtr( parts, ( size_t )part ) );
}
