/*
 * Copyright (c) 2024-2026 xDev-Language
 *
 * Library...: xDev GTXWC
 * File......: gtxwc.c
 *
 */

#include "gtxwc.h"

#pragma comment( lib, "X11" )

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <locale.h>
#include <math.h>
#include <poll.h>
#include <signal.h>
#include <spawn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>
#include <wchar.h>
#include <wctype.h>
#include <X11/Xproto.h>
#include <X11/Xresource.h>

#define X_GTXWC_GET( gt ) ( ( X_GTXWC * )( gt )->driverData[s_gtId] )

static int  s_gt_xwc_WindowState( X_GTXWC *xwc, Atom first, Atom second );
static void s_gt_xwc_SetWindowState( X_GTXWC *xwc, Atom first, Atom second, int enabled );
static void s_gt_xwc_UpdateMousePosition( X_GTXWC *xwc, int x, int y, int addEvent );
static int  s_gt_xwc_DialogEvent( X_GTXWC *xwc, XEvent *event );

static int        s_gtId;
static X_GT_FUNCS s_super;

extern char **environ;

typedef struct
{
   X_GTXWC *xwc;
   int      previous;
} X_GTXWC_INKEY_STATE;

typedef struct
{
   int dpi;
   int doubleClickTime;
   int doubleClickDistance;
   int cursorBlink;
   int cursorBlinkTime;
} X_GTXWC_DESKTOP_SETTINGS;

typedef struct
{
   int start;
   int length;
} X_GTXWC_DIALOG_LINE;

typedef struct
{
   XChar2b     *text;
   int          length;
   X_GTXWC_RECT rect;
} X_GTXWC_DIALOG_BUTTON;

typedef struct _X_GTXWC_DIALOG
{
   X_GTXWC               *xwc;
   Window                 window;
   Pixmap                 pixmap;
   GC                     gc;
   char                  *message;
   XChar2b               *text;
   X_GTXWC_DIALOG_LINE   *lines;
   X_GTXWC_DIALOG_BUTTON *buttons;
   int                    width;
   int                    height;
   int                    lineCount;
   int                    visibleLines;
   int                    firstLine;
   int                    buttonCount;
   int                    selected;
   int                    pressed;
   int                    result;
   int                    done;
} X_GTXWC_DIALOG;

typedef struct
{
   X_GTXWC_DIALOG             modal;
   pid_t                      child;
   int                        descriptors[4];
   char                      *program;
   char                      *arguments[96];
   int                        argumentCount;
   char                     **environment;
   char                      *displayEnvironment;
   char                      *output;
   size_t                     outputLength;
   size_t                     outputCapacity;
   char                      *errors;
   size_t                     errorLength;
   size_t                     errorCapacity;
   posix_spawn_file_actions_t actions;
   int                        actionsInitialized;
} X_GTXWC_COMMAND;

static const uint8_t s_defaultColors[16][3] = {
   { 0, 0, 0 },     { 0, 0, 170 },     { 0, 170, 0 },    { 0, 170, 170 },  { 170, 0, 0 },   { 170, 0, 170 },
   { 170, 85, 0 },  { 170, 170, 170 }, { 85, 85, 85 },   { 85, 85, 255 },  { 85, 255, 85 }, { 85, 255, 255 },
   { 255, 85, 85 }, { 255, 85, 255 },  { 255, 255, 85 }, { 255, 255, 255 }
};

static const int s_floodDX[4] = { -1, 1, 0, 0 };
static const int s_floodDY[4] = { 0, 0, -1, 1 };

static const uint8_t s_boxArms[128] = {
   0x05, 0x0A, 0x50, 0xA0, 0x05, 0x0A, 0x50, 0xA0, 0x05, 0x0A, 0x50, 0xA0, 0x44, 0x48, 0x84, 0x88, 0x41, 0x42, 0x81,
   0x82, 0x14, 0x18, 0x24, 0x28, 0x11, 0x12, 0x21, 0x22, 0x54, 0x58, 0x64, 0x94, 0xA4, 0x68, 0x98, 0xA8, 0x51, 0x52,
   0x61, 0x91, 0xA1, 0x62, 0x92, 0xA2, 0x45, 0x46, 0x49, 0x4A, 0x85, 0x86, 0x89, 0x8A, 0x15, 0x16, 0x19, 0x1A, 0x25,
   0x26, 0x29, 0x2A, 0x55, 0x56, 0x59, 0x5A, 0x65, 0x95, 0xA5, 0x66, 0x69, 0x96, 0x99, 0x6A, 0x9A, 0xA6, 0xA9, 0xAA,
   0x05, 0x0A, 0x50, 0xA0, 0x0F, 0xF0, 0x4C, 0xC4, 0xCC, 0x43, 0xC1, 0xC3, 0x1C, 0x34, 0x3C, 0x13, 0x31, 0x33, 0x5C,
   0xF4, 0xFC, 0x53, 0xF1, 0xF3, 0x4F, 0xC5, 0xCF, 0x1F, 0x35, 0x3F, 0x5F, 0xF5, 0xFF, 0x44, 0x41, 0x11, 0x14, 0x00,
   0x00, 0x00, 0x01, 0x10, 0x04, 0x40, 0x02, 0x20, 0x08, 0x80, 0x09, 0x90, 0x06, 0x60
};

static const uint8_t s_boxShade[4] = { 0, 2, 3, 1 };
static const uint8_t s_boxQuadrants[10] = { 4, 8, 1, 13, 9, 7, 11, 2, 6, 14 };

#define X_GTXWC_CLIPBOARD_LIMIT ( 16 * 1024 * 1024 )
#define X_GTXWC_CLIPBOARD_CHUNK ( 64 * 1024 )
#define X_GTXWC_CLIPBOARD_TIMEOUT 5.0
#define X_GTXWC_CLIPBOARD_TRANSFERS 8
#define X_GTXWC_IMAGE_PIXELS ( 16 * 1024 * 1024 )
#define X_GTXWC_DIALOG_LIMIT ( 1024 * 1024 )
#define X_GTXWC_DIALOG_BUTTONS 64

typedef struct _X_GTXWC_TRANSFER
{
   struct _X_GTXWC_TRANSFER *next;
   Window                    requestor;
   Atom                      property;
   Atom                      type;
   long                      eventMaskAdded;
   char                     *data;
   size_t                    length;
   size_t                    offset;
   double                    deadline;
} X_GTXWC_TRANSFER;

typedef struct
{
   Atom   atom;
   Time   acquired;
   char  *text;
   size_t length;
} X_GTXWC_SELECTION;

typedef struct _X_GTXWC_CLIPBOARD
{
   X_GTXWC_SELECTION clipboardSelection;
   X_GTXWC_SELECTION primarySelection;
   Atom              targets;
   Atom              textTarget;
   Atom              compoundText;
   Atom              multiple;
   Atom              atomPair;
   Atom              timestamp;
   Atom              incr;
   Atom              property;
   Atom              timeProperty;
   size_t            chunkSize;
   X_GTXWC_TRANSFER *transfers;
   Window            reader;
   int               readerDestroyed;
   Atom              readSelection;
   Atom              readTarget;
   Atom              readType;
   char             *readData;
   size_t            readLength;
   size_t            readCapacity;
   int               incremental;
   int               finished;
   int               failed;
} X_GTXWC_CLIPBOARD;

typedef struct _X_GTXWC_INPUT_TRAP
{
   struct _X_GTXWC_INPUT_TRAP *parent;
   X_GTXWC                    *xwc;
   XErrorHandler               previous;
} X_GTXWC_INPUT_TRAP;

/* the reply of XGetWindowProperty(): type, format, number of items, bytes left and the data */
typedef struct
{
   Atom           type;
   int            format;
   unsigned long  count;
   unsigned long  remaining;
   unsigned char *data;
} X_GTXWC_PROPERTY;

static X_GTXWC_INPUT_TRAP *s_inputTrap;

static int s_xError;

static double s_gt_xwc_Time( void )
{
   struct timespec now;

   clock_gettime( CLOCK_MONOTONIC, &now );

   return ( double )now.tv_sec + ( double )now.tv_nsec / 1000000000.0;
}

static int s_gt_xwc_XError( Display *display, XErrorEvent *event )
{
   UNUSED( display );
   UNUSED( event );
   s_xError = 1;

   return 0;
}

static XErrorHandler s_gt_xwc_ErrorTrap( X_GTXWC *xwc )
{
   XSync( xwc->display, False );
   s_xError = 0;

   return XSetErrorHandler( s_gt_xwc_XError );
}

static int s_gt_xwc_ErrorUntrap( X_GTXWC *xwc, XErrorHandler previous )
{
   XSync( xwc->display, False );
   XSetErrorHandler( previous );

   return s_xError == 0;
}

/* reads up to length 32-bit units of a window property; reply->data is freed with XFree() */
static int s_gt_xwc_GetProperty( X_GTXWC *xwc, Window window, Atom name, long length, Bool remove, Atom type,
                                 X_GTXWC_PROPERTY *reply )
{
   Display        *display = xwc->display;
   Atom           *replyType = &reply->type;
   int            *format = &reply->format;
   unsigned long  *count = &reply->count;
   unsigned long  *left = &reply->remaining;
   unsigned char **data = &reply->data;
   int             status;

   reply->data = NULL;
   status = XGetWindowProperty( display, window, name, 0, length, remove, type, replyType, format, count, left, data );

   return status == Success;
}

static uint32_t s_gt_xwc_SettingsNumber( const unsigned char *data, int bytes, int msb )
{
   uint32_t value = 0;
   int      index;

   for( index = 0; index < bytes; ++index )
      value = ( value << 8 ) | data[IIF( msb, index, bytes - index - 1 )];

   return value;
}

static int s_gt_xwc_SettingsName( const unsigned char *name, size_t length, const char *expected )
{
   return length == strlen( expected ) && memcmp( name, expected, length ) == 0;
}

static int s_gt_xwc_ParseDesktopSettings( const unsigned char *data, size_t length, X_GTXWC_DESKTOP_SETTINGS *settings )
{
   X_GTXWC_DESKTOP_SETTINGS result = { -1, -1, -1, -1, -1 };
   uint32_t                 count, index;
   size_t                   position = 12;
   int                      msb;

   if( length < 12 || ( data[0] != LSBFirst && data[0] != MSBFirst ) )
      return 0;

   msb = data[0] == MSBFirst;
   count = s_gt_xwc_SettingsNumber( data + 8, 4, msb );
   for( index = 0; index < count; ++index )
   {
      const unsigned char *name;
      size_t               nameLength, padded;
      int                  type;

      if( length - position < 4 )
         return 0;

      type = data[position];
      nameLength = s_gt_xwc_SettingsNumber( data + position + 2, 2, msb );
      position += 4;
      padded = ( nameLength + 3 ) & ~( size_t )3;
      if( length - position < padded + 4 )
         return 0;

      name = data + position;
      position += padded + 4;
      if( type == 0 )
      {
         uint32_t number;
         int64_t  value;

         if( length - position < 4 )
            return 0;

         number = s_gt_xwc_SettingsNumber( data + position, 4, msb );
         value = IIF( number <= INT32_MAX, number, ( int64_t )number - 4294967296LL );
         position += 4;
         if( s_gt_xwc_SettingsName( name, nameLength, "Xft/DPI" ) )
            result.dpi = ( int )value;
         else if( s_gt_xwc_SettingsName( name, nameLength, "Net/DoubleClickTime" ) )
            result.doubleClickTime = ( int )value;
         else if( s_gt_xwc_SettingsName( name, nameLength, "Net/DoubleClickDistance" ) )
            result.doubleClickDistance = ( int )value;
         else if( s_gt_xwc_SettingsName( name, nameLength, "Net/CursorBlink" ) )
            result.cursorBlink = ( int )value;
         else if( s_gt_xwc_SettingsName( name, nameLength, "Net/CursorBlinkTime" ) )
            result.cursorBlinkTime = ( int )value;
      }
      else if( type == 1 )
      {
         uint32_t stringLength;

         if( length - position < 4 )
            return 0;

         stringLength = s_gt_xwc_SettingsNumber( data + position, 4, msb );
         position += 4;
         if( stringLength > length - position )
            return 0;

         padded = ( ( size_t )stringLength + 3 ) & ~( size_t )3;
         if( padded > length - position )
            return 0;

         position += padded;
      }
      else if( type == 2 )
      {
         if( length - position < 8 )
            return 0;

         position += 8;
      }
      else
         return 0;
   }
   if( position != length )
      return 0;

   *settings = result;

   return 1;
}

static int s_gt_xwc_DesktopSettings( X_GTXWC *xwc, X_GTXWC_DESKTOP_SETTINGS *settings )
{
   char             selection[32];
   Atom             selectionAtom, property;
   Window           owner;
   X_GTXWC_PROPERTY reply = { 0 };
   XErrorHandler    previous;
   int              success = 0;

   snprintf( selection, sizeof( selection ), "_XSETTINGS_S%d", xwc->screen );
   selectionAtom = XInternAtom( xwc->display, selection, True );
   property = XInternAtom( xwc->display, "_XSETTINGS_SETTINGS", True );

   if( selectionAtom == None || property == None )
      return 0;

   previous = s_gt_xwc_ErrorTrap( xwc );
   owner = XGetSelectionOwner( xwc->display, selectionAtom );
   if( owner != None )
      success = s_gt_xwc_GetProperty( xwc, owner, property, 16384, False, property, &reply );
   if( reply.type != property || reply.format != 8 || reply.remaining != 0 )
      success = 0;
   success = s_gt_xwc_ErrorUntrap( xwc, previous ) && success;
   if( success )
      success = s_gt_xwc_ParseDesktopSettings( reply.data, reply.count, settings );
   if( reply.data )
      XFree( reply.data );

   return success;
}

static Bool s_gt_xwc_ClipboardTimeEvent( Display *display, XEvent *event, XPointer context )
{
   X_GTXWC *xwc = ( X_GTXWC * )context;

   UNUSED( display );
   if( event->type != PropertyNotify || event->xproperty.window != xwc->window )
      return False;

   return event->xproperty.atom == xwc->clipboard->timeProperty && event->xproperty.state == PropertyNewValue;
}

static Time s_gt_xwc_ClipboardServerTime( X_GTXWC *xwc )
{
   XEvent event;

   XChangeProperty( xwc->display, xwc->window, xwc->clipboard->timeProperty, XA_INTEGER, 8, PropModeAppend, NULL, 0 );
   XIfEvent( xwc->display, &event, s_gt_xwc_ClipboardTimeEvent, ( XPointer )xwc );

   return event.xproperty.time;
}

static void s_gt_xwc_ClipboardFreeTransfer( X_GTXWC *xwc, X_GTXWC_TRANSFER **link, int destroyed )
{
   X_GTXWC_TRANSFER *transfer = *link;

   *link = transfer->next;
   if( !destroyed && transfer->eventMaskAdded )
   {
      X_GTXWC_TRANSFER *other = xwc->clipboard->transfers;

      while( other && other->requestor != transfer->requestor )
         other = other->next;
      if( !other )
      {
         XWindowAttributes attributes;
         XErrorHandler     previous = s_gt_xwc_ErrorTrap( xwc );

         if( XGetWindowAttributes( xwc->display, transfer->requestor, &attributes ) )
            XSelectInput( xwc->display, transfer->requestor, attributes.your_event_mask & ~transfer->eventMaskAdded );
         s_gt_xwc_ErrorUntrap( xwc, previous );
      }
   }
   x_xfree( transfer->data );
   x_xfree( transfer );
}

static void s_gt_xwc_ClipboardExpire( X_GTXWC *xwc )
{
   X_GTXWC_TRANSFER **link = &xwc->clipboard->transfers;
   double             now = s_gt_xwc_Time();

   while( *link )
   {
      if( ( *link )->deadline < now )
         s_gt_xwc_ClipboardFreeTransfer( xwc, link, 0 );
      else
         link = &( *link )->next;
   }
}

static int s_gt_xwc_ClipboardConvert( X_GTXWC *xwc, X_GTXWC_SELECTION *selection, Window requestor, Atom target,
                                      Atom property )
{
   X_GTXWC_CLIPBOARD *clip = xwc->clipboard;
   char              *data;
   size_t             length = selection->length;
   Atom               type = xwc->utf8String;

   if( property == None || !selection->text )
      return 0;

   if( target == clip->targets )
   {
      Atom targets[] = { clip->targets, clip->multiple, clip->timestamp, xwc->utf8String, clip->textTarget, XA_STRING };
      const unsigned char *bytes = ( const unsigned char * )targets;
      int                  count = sizeof( targets ) / sizeof( targets[0] );

      XChangeProperty( xwc->display, requestor, property, XA_ATOM, 32, PropModeReplace, bytes, count );
      return 1;
   }
   if( target == clip->timestamp )
   {
      unsigned long        acquired = selection->acquired;
      const unsigned char *bytes = ( const unsigned char * )&acquired;

      XChangeProperty( xwc->display, requestor, property, XA_INTEGER, 32, PropModeReplace, bytes, 1 );
      return 1;
   }
   if( target != xwc->utf8String && target != clip->textTarget && target != XA_STRING )
      return 0;

   data = ( char * )x_xmemdup( selection->text, length + 1 );
   if( target == XA_STRING )
   {
      size_t   offset = 0;
      uint32_t codepoint;

      type = XA_STRING;
      length = 0;
      while( x_utf8Decode( selection->text, selection->length, &offset, &codepoint ) )
         data[length++] = IIF( codepoint <= 255, ( char )codepoint, '?' );
   }
   if( length <= clip->chunkSize )
   {
      const unsigned char *bytes = ( const unsigned char * )data;

      XChangeProperty( xwc->display, requestor, property, type, 8, PropModeReplace, bytes, ( int )length );
      x_xfree( data );
   }
   else
   {
      X_GTXWC_TRANSFER    *transfer = clip->transfers;
      XWindowAttributes    attributes;
      unsigned long        total = length;
      const unsigned char *bytes = ( const unsigned char * )&total;
      int                  count = 0;
      int                  accepted;

      while( transfer )
      {
         if( transfer->requestor == requestor && transfer->property == property )
            break;
         ++count;
         transfer = transfer->next;
      }
      accepted = !transfer && count < X_GTXWC_CLIPBOARD_TRANSFERS;
      if( accepted )
         accepted = XGetWindowAttributes( xwc->display, requestor, &attributes );
      if( !accepted )
      {
         x_xfree( data );
         return 0;
      }
      transfer = ( X_GTXWC_TRANSFER * )x_xgrabz( sizeof( X_GTXWC_TRANSFER ) );
      transfer->eventMaskAdded = ( PropertyChangeMask | StructureNotifyMask ) & ~attributes.your_event_mask;
      for( X_GTXWC_TRANSFER *other = clip->transfers; other; other = other->next )
         if( other->requestor == requestor )
         {
            transfer->eventMaskAdded = other->eventMaskAdded;
            break;
         }
      if( ( PropertyChangeMask | StructureNotifyMask ) & ~attributes.your_event_mask )
         XSelectInput( xwc->display, requestor, attributes.your_event_mask | PropertyChangeMask | StructureNotifyMask );
      transfer->requestor = requestor;
      transfer->property = property;
      transfer->type = type;
      transfer->data = data;
      transfer->length = length;
      transfer->deadline = s_gt_xwc_Time() + X_GTXWC_CLIPBOARD_TIMEOUT;
      transfer->next = clip->transfers;
      clip->transfers = transfer;
      XChangeProperty( xwc->display, requestor, property, clip->incr, 32, PropModeReplace, bytes, 1 );
   }

   return 1;
}

static int s_gt_xwc_ClipboardMultiple( X_GTXWC *xwc, X_GTXWC_SELECTION *selection, XSelectionRequestEvent *request )
{
   X_GTXWC_CLIPBOARD *clip = xwc->clipboard;
   X_GTXWC_PROPERTY   reply = { 0 };
   Window             requestor = request->requestor;
   Atom               property = request->property;
   unsigned long      index;
   int                success = 0;

   if( property != None )
      success = s_gt_xwc_GetProperty( xwc, requestor, property, 4096, False, clip->atomPair, &reply );
   if( reply.type != clip->atomPair || reply.format != 32 || reply.count % 2 != 0 || reply.remaining != 0 )
      success = 0;
   if( success )
   {
      Atom *pairs = ( Atom * )reply.data;
      int   items = ( int )reply.count;

      for( index = 0; index < reply.count; index += 2 )
      {
         if( pairs[index + 1] == property )
            pairs[index] = None;
         else if( !s_gt_xwc_ClipboardConvert( xwc, selection, requestor, pairs[index], pairs[index + 1] ) )
            pairs[index] = None;
      }
      XChangeProperty( xwc->display, requestor, property, clip->atomPair, 32, PropModeReplace, reply.data, items );
   }
   if( reply.data )
      XFree( reply.data );

   return success;
}

static void s_gt_xwc_ClipboardRequest( X_GTXWC *xwc, XSelectionRequestEvent *request )
{
   X_GTXWC_CLIPBOARD *clip = xwc->clipboard;
   X_GTXWC_SELECTION *selection = &clip->clipboardSelection;
   Atom               property = IIF( request->property == None, request->target, request->property );
   XEvent             reply = { 0 };
   XErrorHandler      previous;
   int                success = 0;
   int                valid;

   if( request->selection == XA_PRIMARY )
      selection = &clip->primarySelection;
   s_gt_xwc_ClipboardExpire( xwc );
   previous = s_gt_xwc_ErrorTrap( xwc );
   valid = request->selection == selection->atom && request->owner == xwc->window;
   if( valid && request->time != CurrentTime && ( int32_t )( request->time - selection->acquired ) < 0 )
      valid = 0;
   if( valid && request->target == clip->multiple )
      success = s_gt_xwc_ClipboardMultiple( xwc, selection, request );
   else if( valid )
      success = s_gt_xwc_ClipboardConvert( xwc, selection, request->requestor, request->target, property );
   success = s_gt_xwc_ErrorUntrap( xwc, previous ) && success;

   reply.xselection.type = SelectionNotify;
   reply.xselection.display = xwc->display;
   reply.xselection.requestor = request->requestor;
   reply.xselection.selection = request->selection;
   reply.xselection.target = request->target;
   reply.xselection.time = request->time;
   reply.xselection.property = IIF( success, property, None );
   previous = s_gt_xwc_ErrorTrap( xwc );
   XSendEvent( xwc->display, request->requestor, False, 0, &reply );
   s_gt_xwc_ErrorUntrap( xwc, previous );
}

static void s_gt_xwc_ClipboardReceive( X_GTXWC *xwc, int initial )
{
   X_GTXWC_CLIPBOARD *clip = xwc->clipboard;
   X_GTXWC_PROPERTY   reply = { 0 };
   long               length = X_GTXWC_CLIPBOARD_LIMIT / 4 + 1;
   Atom               type;
   int                format;
   unsigned long      count, remaining;
   unsigned char     *data;
   XErrorHandler      previous = s_gt_xwc_ErrorTrap( xwc );
   int                success;
   int                usable;

   success = s_gt_xwc_GetProperty( xwc, clip->reader, clip->property, length, True, AnyPropertyType, &reply );
   success = s_gt_xwc_ErrorUntrap( xwc, previous ) && success;
   type = reply.type;
   format = reply.format;
   count = reply.count;
   remaining = reply.remaining;
   data = reply.data;

   /* a part of the text: 8-bit data of a text type within the limit, the same type as before */
   usable = format == 8 && remaining == 0 && count <= X_GTXWC_CLIPBOARD_LIMIT - clip->readLength;
   if( type != xwc->utf8String && type != XA_STRING && type != clip->compoundText )
      usable = 0;
   if( clip->readType != None && clip->readType != type )
      usable = 0;

   if( !success || type == None )
      clip->failed = 1;
   else if( initial && type == clip->incr )
   {
      if( format == 32 && count == 1 && remaining == 0 && ( ( unsigned long * )data )[0] <= X_GTXWC_CLIPBOARD_LIMIT )
         clip->incremental = 1;
      else
         clip->failed = 1;
   }
   else if( !usable )
      clip->failed = 1;
   else
   {
      size_t required = clip->readLength + count + 1;

      clip->readType = type;
      if( required > clip->readCapacity )
      {
         clip->readCapacity = X_MIN( X_MAX( required, clip->readCapacity * 2 ), X_GTXWC_CLIPBOARD_LIMIT + 1 );
         clip->readData = ( char * )x_xrealloc( clip->readData, clip->readCapacity );
      }
      if( count )
         memcpy( clip->readData + clip->readLength, data, count );
      clip->readLength += count;
      clip->readData[clip->readLength] = '\0';
      if( !clip->incremental || count == 0 )
         clip->finished = 1;
   }
   if( data )
      XFree( data );
   XFlush( xwc->display );
}

static int s_gt_xwc_ClipboardEvent( X_GTXWC *xwc, XEvent *event )
{
   X_GTXWC_CLIPBOARD    *clip = xwc->clipboard;
   X_GTXWC_TRANSFER    **link;
   XSelectionClearEvent *clear = &event->xselectionclear;
   XSelectionEvent      *notify = &event->xselection;
   XPropertyEvent       *change = &event->xproperty;
   int                   reading, matched;

   if( !clip )
      return 0;

   reading = clip->reader && !clip->finished && !clip->failed;
   if( event->type == SelectionRequest )
   {
      s_gt_xwc_ClipboardRequest( xwc, &event->xselectionrequest );
      return 1;
   }
   matched = clear->selection == clip->clipboardSelection.atom || clear->selection == XA_PRIMARY;
   if( event->type == SelectionClear && matched )
   {
      X_GTXWC_SELECTION *selection = &clip->clipboardSelection;
      int                lost = 0;

      if( clear->selection == XA_PRIMARY )
         selection = &clip->primarySelection;
      if( ( int32_t )( clear->time - selection->acquired ) >= 0 )
         lost = XGetSelectionOwner( xwc->display, selection->atom ) != xwc->window;
      if( lost )
      {
         x_xfree( selection->text );
         selection->text = NULL;
         selection->length = 0;
         if( selection->atom == XA_PRIMARY && !xwc->selecting )
         {
            xwc->selectionActive = 0;
            xwc->dirty = 1;
         }
      }
      return 1;
   }
   matched = notify->selection == clip->readSelection && notify->target == clip->readTarget;
   if( event->type == SelectionNotify && reading && notify->requestor == clip->reader && matched )
   {
      if( notify->property == clip->property )
         s_gt_xwc_ClipboardReceive( xwc, 1 );
      else
         clip->failed = 1;
      return 1;
   }
   matched = change->atom == clip->property && change->state == PropertyNewValue && clip->incremental;
   if( event->type == PropertyNotify && reading && change->window == clip->reader && matched )
   {
      s_gt_xwc_ClipboardReceive( xwc, 0 );
      return 1;
   }
   if( event->type == DestroyNotify && clip->reader && event->xdestroywindow.window == clip->reader )
   {
      clip->readerDestroyed = clip->failed = 1;
      return 1;
   }
   if( event->type != PropertyNotify && event->type != DestroyNotify )
      return 0;

   link = &clip->transfers;
   while( *link )
   {
      X_GTXWC_TRANSFER *transfer = *link;
      int               deleted = change->atom == transfer->property && change->state == PropertyDelete;

      if( event->type == DestroyNotify && event->xdestroywindow.window == transfer->requestor )
         s_gt_xwc_ClipboardFreeTransfer( xwc, link, 1 );
      else if( event->type == PropertyNotify && change->window == transfer->requestor && deleted )
      {
         size_t               length = X_MIN( transfer->length - transfer->offset, clip->chunkSize );
         const unsigned char *bytes = ( const unsigned char * )transfer->data + transfer->offset;
         Window               requestor = transfer->requestor;
         Atom                 property = transfer->property;
         Atom                 type = transfer->type;
         int                  size = ( int )length;
         XErrorHandler        previous = s_gt_xwc_ErrorTrap( xwc );
         int                  success;

         XChangeProperty( xwc->display, requestor, property, type, 8, PropModeReplace, bytes, size );
         success = s_gt_xwc_ErrorUntrap( xwc, previous );
         transfer->offset += length;
         transfer->deadline = s_gt_xwc_Time() + X_GTXWC_CLIPBOARD_TIMEOUT;
         if( !success || length == 0 )
            s_gt_xwc_ClipboardFreeTransfer( xwc, link, 0 );
         return 1;
      }
      else
         link = &transfer->next;
   }

   return 0;
}

static char *s_gt_xwc_ClipboardText( X_GTXWC *xwc, size_t *length )
{
   X_GTXWC_CLIPBOARD *clip = xwc->clipboard;
   char              *result = NULL;

   if( clip->readType == xwc->utf8String )
   {
      if( x_utf8Validate( clip->readData, clip->readLength ) )
      {
         *length = clip->readLength;
         result = ( char * )x_xmemdup( clip->readData, *length + 1 );
      }
   }
   else if( clip->readType == XA_STRING )
   {
      size_t index;

      *length = 0;
      result = ( char * )x_xgrab( clip->readLength * 2 + 1 );
      for( index = 0; index < clip->readLength; ++index )
         *length += x_utf8Encode( result + *length, ( unsigned char )clip->readData[index] );
      result[*length] = '\0';
   }
   else if( clip->readType == clip->compoundText )
   {
      XTextProperty property;
      char        **list = NULL;
      int           count = 0;

      property.value = ( unsigned char * )clip->readData;
      property.encoding = clip->readType;
      property.format = 8;
      property.nitems = clip->readLength;
      if( Xutf8TextPropertyToTextList( xwc->display, &property, &list, &count ) == Success )
      {
         int index;

         *length = 0;
         for( index = 0; index < count; ++index )
            *length += strlen( list[index] ) + ( index != 0 );
         result = ( char * )x_xgrab( *length + 1 );
         *length = 0;
         for( index = 0; index < count; ++index )
         {
            size_t part = strlen( list[index] );

            if( index )
               result[( *length )++] = '\n';
            memcpy( result + *length, list[index], part );
            *length += part;
         }
         result[*length] = '\0';
      }
      if( list )
         XFreeStringList( list );
   }

   return result;
}

static void s_gt_xwc_ClipboardReadDrop( X_GTXWC **context )
{
   X_GTXWC           *xwc = *context;
   X_GTXWC_CLIPBOARD *clip = xwc->clipboard;

   if( clip->reader && !clip->readerDestroyed )
   {
      XErrorHandler previous = s_gt_xwc_ErrorTrap( xwc );

      XDestroyWindow( xwc->display, clip->reader );
      s_gt_xwc_ErrorUntrap( xwc, previous );
   }
   clip->reader = None;
   clip->readerDestroyed = 0;
   x_xfree( clip->readData );
   clip->readData = NULL;
   clip->readLength = clip->readCapacity = 0;
   clip->readSelection = clip->readTarget = clip->readType = None;
   clip->incremental = clip->finished = clip->failed = 0;
}

static char *s_gt_xwc_GetClipboard( X_GTXWC *xwc, int primary, size_t *length, void ( *processEvents )( X_GTXWC * ) )
{
   X_GTXWC_CLIPBOARD *clip = xwc->clipboard;
   X_GTXWC_SELECTION *selection = IIF( primary, &clip->primarySelection, &clip->clipboardSelection );
   Window             owner = XGetSelectionOwner( xwc->display, selection->atom );
   Atom               targets[] = { xwc->utf8String, clip->textTarget, XA_STRING };
   char              *result = NULL;
   size_t             index;
   double             deadline = s_gt_xwc_Time() + X_GTXWC_CLIPBOARD_TIMEOUT;

   *length = 0;
   if( owner == None || clip->reader )
      return NULL;

   if( owner == xwc->window && selection->text )
   {
      *length = selection->length;
      return ( char * )x_xmemdup( selection->text, selection->length + 1 );
   }
   for( index = 0; index < sizeof( targets ) / sizeof( targets[0] ); ++index )
   {
      X_GTXWC             *read __attribute__( ( cleanup( s_gt_xwc_ClipboardReadDrop ) ) ) = xwc;
      XSetWindowAttributes attributes;
      struct pollfd        descriptor;
      Time                 requested = s_gt_xwc_ClipboardServerTime( xwc );
      unsigned long        mask = CWEventMask;
      Window               parent = xwc->window;
      Window               reader;

      attributes.event_mask = PropertyChangeMask | StructureNotifyMask;
      reader = XCreateWindow( xwc->display, parent, 0, 0, 1, 1, 0, 0, InputOnly, CopyFromParent, mask, &attributes );
      clip->reader = reader;
      clip->readSelection = selection->atom;
      clip->readTarget = targets[index];
      clip->readType = None;
      clip->incremental = clip->finished = clip->failed = 0;
      clip->readLength = clip->readCapacity = 0;
      clip->readData = NULL;
      XConvertSelection( xwc->display, selection->atom, clip->readTarget, clip->property, clip->reader, requested );
      XFlush( xwc->display );
      descriptor.fd = ConnectionNumber( xwc->display );
      descriptor.events = POLLIN;
      while( !clip->finished && !clip->failed && s_gt_xwc_Time() < deadline )
      {
         processEvents( xwc );
         if( xwc->dirty && X_GTSELF_DISPCOUNT( xwc->gt ) == 0 )
            xwc->gt->pFuncTable->Refresh( xwc->gt );
         if( !clip->finished && !clip->failed )
            poll( &descriptor, 1, 10 );
      }
      if( clip->finished && !clip->failed )
         result = s_gt_xwc_ClipboardText( xwc, length );
      if( result || s_gt_xwc_Time() >= deadline )
         break;
   }

   return result;
}

static int s_gt_xwc_SetClipboard( X_GTXWC *xwc, int primary, const char *text, size_t length )
{
   X_GTXWC_CLIPBOARD *clip = xwc->clipboard;
   X_GTXWC_SELECTION *selection = IIF( primary, &clip->primarySelection, &clip->clipboardSelection );
   Time               acquired;

   if( length > X_GTXWC_CLIPBOARD_LIMIT || !x_utf8Validate( text, length ) )
      return 0;

   acquired = s_gt_xwc_ClipboardServerTime( xwc );
   XSetSelectionOwner( xwc->display, selection->atom, xwc->window, acquired );
   if( XGetSelectionOwner( xwc->display, selection->atom ) != xwc->window )
      return 0;

   x_xfree( selection->text );
   selection->text = ( char * )x_xgrab( length + 1 );
   memcpy( selection->text, text, length );
   selection->text[length] = '\0';
   selection->length = length;
   selection->acquired = acquired;

   return 1;
}

static void s_gt_xwc_ClipboardInit( X_GTXWC *xwc )
{
   char              *names[] = { "CLIPBOARD", "TARGETS",   "TEXT", "COMPOUND_TEXT",   "MULTIPLE",
                                  "ATOM_PAIR", "TIMESTAMP", "INCR", "_XDEV_CLIPBOARD", "_XDEV_CLIPBOARD_TIME" };
   Atom               atoms[10];
   X_GTXWC_CLIPBOARD *clip = ( X_GTXWC_CLIPBOARD * )x_xgrabz( sizeof( X_GTXWC_CLIPBOARD ) );

   xwc->clipboard = clip;
   XInternAtoms( xwc->display, names, 10, False, atoms );
   clip->clipboardSelection.atom = atoms[0];
   clip->primarySelection.atom = XA_PRIMARY;
   clip->targets = atoms[1];
   clip->textTarget = atoms[2];
   clip->compoundText = atoms[3];
   clip->multiple = atoms[4];
   clip->atomPair = atoms[5];
   clip->timestamp = atoms[6];
   clip->incr = atoms[7];
   clip->property = atoms[8];
   clip->timeProperty = atoms[9];
   clip->chunkSize = X_MIN( ( size_t )( XMaxRequestSize( xwc->display ) - 64 ) * 4, X_GTXWC_CLIPBOARD_CHUNK );
}

static void s_gt_xwc_ClipboardExit( X_GTXWC *xwc )
{
   X_GTXWC_CLIPBOARD *clip = xwc->clipboard;

   if( clip )
   {
      while( clip->transfers )
         s_gt_xwc_ClipboardFreeTransfer( xwc, &clip->transfers, 0 );
      x_xfree( clip->readData );
      x_xfree( clip->clipboardSelection.text );
      x_xfree( clip->primarySelection.text );
      x_xfree( clip );
      xwc->clipboard = NULL;
   }
}

static int s_gt_xwc_ClipboardInfo( X_GTXWC *xwc, int type, X_GT_INFO *info, void ( *processEvents )( X_GTXWC * ) )
{
   s_gt_xwc_ClipboardExpire( xwc );
   if( type == X_GTI_CLIPBOARDDATA && info->pNewVal && X_IS_STRING( info->pNewVal ) )
   {
      const char *text = x_itemGetCPtr( info->pNewVal );
      size_t      length = x_itemGetCLen( info->pNewVal );

      info->pResult = x_itemPutL( info->pResult, s_gt_xwc_SetClipboard( xwc, 0, text, length ) );
   }
   else
   {
      size_t length;
      char  *text = s_gt_xwc_GetClipboard( xwc, 0, &length, processEvents );

      if( type == X_GTI_CLIPBOARDPASTE )
      {
         if( text )
            xwc->gt->pFuncTable->InkeySetText( xwc->gt, text, length, 1 );
         info->pResult = x_itemPutL( info->pResult, text != NULL );
      }
      else
         info->pResult = x_itemPutCL( info->pResult, IIF( text, text, "" ), length );
      x_xfree( text );
   }

   return 1;
}

static int s_gt_xwc_PasteClipboard( X_GTXWC *xwc, int primary, void ( *processEvents )( X_GTXWC * ) )
{
   size_t length;
   char  *text = s_gt_xwc_GetClipboard( xwc, primary, &length, processEvents );
   int    success = text != NULL;

   if( text )
      xwc->gt->pFuncTable->InkeySetText( xwc->gt, text, length, 1 );
   x_xfree( text );

   return success;
}

/* ---------------------------------------------------------------------------------------------
   Software canvas and the shared Fixed 9x18 font
   --------------------------------------------------------------------------------------------- */

static uint32_t s_gt_xwc_Color( X_GTXWC *xwc, int index )
{
   const uint8_t *rgb = xwc->colors[index & 0x0F];

   return ( ( uint32_t )rgb[0] << 16 ) | ( ( uint32_t )rgb[1] << 8 ) | rgb[2];
}

/* colors of a cell as 0xRRGGBB: the 24-bit colors of DispOutAtExt() and DispBoxExt(), otherwise
   the palette entries of its color attribute; s_gt_xwc_Pixel() finds the nearest pixel on any visual */
static void s_gt_xwc_CellColors( X_GTXWC *xwc, int row, int col, int color, uint32_t *fore, uint32_t *back )
{
   const X_GT_CELL *cell = &xwc->gt->screenBuffer[( size_t )row * xwc->gt->cols + col];

   *fore = IIF( cell->foreground != X_GT_RGB_DEFAULT, cell->foreground, s_gt_xwc_Color( xwc, color ) );
   *back = IIF( cell->background != X_GT_RGB_DEFAULT, cell->background, s_gt_xwc_Color( xwc, color >> 4 ) );
}

static int s_gt_xwc_HasGlyph( X_GTXWC *xwc, uint16_t code )
{
   XFontStruct *font = xwc->font;
   unsigned int row = code >> 8;
   unsigned int col = code & 0xFF;
   size_t       index, columns;

   if( row < font->min_byte1 || row > font->max_byte1 )
      return 0;

   if( col < font->min_char_or_byte2 || col > font->max_char_or_byte2 )
      return 0;

   if( font->all_chars_exist || !font->per_char )
      return 1;

   columns = font->max_char_or_byte2 - font->min_char_or_byte2 + 1;
   index = ( size_t )( row - font->min_byte1 ) * columns + col - font->min_char_or_byte2;

   return font->per_char[index].width != 0;
}

static int s_gt_xwc_FontMatches( X_GTXWC *xwc, XFontStruct *font )
{
   unsigned long registry, encoding;
   Atom          unicode = XInternAtom( xwc->display, "iso10646", False );
   Atom          unicodeUpper = XInternAtom( xwc->display, "ISO10646", False );
   Atom          unicodeEncoding = XInternAtom( xwc->display, "1", False );
   Atom          registryName = XInternAtom( xwc->display, "CHARSET_REGISTRY", False );
   Atom          encodingName = XInternAtom( xwc->display, "CHARSET_ENCODING", False );

   if( font->min_bounds.width != X_GTXWC_FONT_WIDTH || font->max_bounds.width != X_GTXWC_FONT_WIDTH )
      return 0;

   if( font->ascent + font->descent != X_GTXWC_FONT_HEIGHT )
      return 0;

   if( !XGetFontProperty( font, registryName, &registry ) || !XGetFontProperty( font, encodingName, &encoding ) )
      return 0;

   return ( registry == unicode || registry == unicodeUpper ) && encoding == unicodeEncoding;
}

static int s_gt_xwc_SetFont( X_GTXWC *xwc, const char *selection )
{
   XFontStruct  *font = XLoadQueryFont( xwc->display, selection );
   unsigned long property;
   char         *name, *family, *newSelection, *newFamily;
   int           page;

   if( !font || !s_gt_xwc_FontMatches( xwc, font ) )
   {
      char **names;
      int    count, index;

      if( font )
         XFreeFont( xwc->display, font );
      font = NULL;
      names = XListFonts( xwc->display, selection, 256, &count );
      for( index = 0; index < count; ++index )
      {
         font = XLoadQueryFont( xwc->display, names[index] );
         if( font )
         {
            if( s_gt_xwc_FontMatches( xwc, font ) )
               break;
            XFreeFont( xwc->display, font );
            font = NULL;
         }
      }
      if( names )
         XFreeFontNames( names );
   }
   if( !font )
      return 0;

   name = IIF( XGetFontProperty( font, XA_FONT, &property ), XGetAtomName( xwc->display, property ), NULL );
   family = IIF( XGetFontProperty( font, XA_FAMILY_NAME, &property ), XGetAtomName( xwc->display, property ), NULL );
   newSelection = x_strdup( IIF( name, name, selection ) );
   newFamily = x_strdup( IIF( family, family, "fixed" ) );
   x_xfree( xwc->fontSel );
   x_xfree( xwc->fontName );
   xwc->fontSel = newSelection;
   xwc->fontName = newFamily;
   if( name )
      XFree( name );
   if( family )
      XFree( family );
   xwc->fontWeight = X_GTI_FONTW_NORMAL;
   if( XGetFontProperty( font, XInternAtom( xwc->display, "WEIGHT_NAME", False ), &property ) )
   {
      char *weight = XGetAtomName( xwc->display, property );

      if( weight )
      {
         if( strcmp( weight, "bold" ) == 0 || strcmp( weight, "Bold" ) == 0 )
            xwc->fontWeight = X_GTI_FONTW_BOLD;
         else if( strcmp( weight, "light" ) == 0 || strcmp( weight, "Light" ) == 0 )
            xwc->fontWeight = X_GTI_FONTW_THIN;
         XFree( weight );
      }
   }
   for( page = 0; page < 256; ++page )
   {
      x_xfree( xwc->fontPages[page] );
      xwc->fontPages[page] = NULL;
   }
   if( xwc->font )
      XFreeFont( xwc->display, xwc->font );
   xwc->font = font;
   if( xwc->fontGC )
      XSetFont( xwc->display, xwc->fontGC, font->fid );
   xwc->dirty = 1;

   return 1;
}

static int s_gt_xwc_SetFontFamily( X_GTXWC *xwc, const char *family, int weight )
{
   char        selection[512];
   const char *style = "medium";
   int         length;

   if( weight == X_GTI_FONTW_BOLD )
      style = "bold";
   else if( weight == X_GTI_FONTW_THIN )
      style = "light";
   length = snprintf( selection, sizeof( selection ), "-*-%s-%s-r-normal--18-*-*-*-c-90-iso10646-1", family, style );

   return length > 0 && length < ( int )sizeof( selection ) && s_gt_xwc_SetFont( xwc, selection );
}

static void s_gt_xwc_LoadFont( X_GTXWC *xwc )
{
   XGCValues values;
   Window    root = RootWindow( xwc->display, xwc->screen );

   if( !s_gt_xwc_SetFont( xwc, X_GTXWC_FONT_NAME ) )
   {
      fputs( "xDev GTXWC: cannot load the X11 Unicode fixed 9x18 font (install xfonts-base)\n", stderr );
      x_quitRaise();
      return;
   }
   xwc->fontPixmap = XCreatePixmap( xwc->display, root, X_GTXWC_FONT_WIDTH, X_GTXWC_FONT_HEIGHT, 1 );
   values.font = xwc->font->fid;
   values.foreground = 1;
   values.background = 0;
   xwc->fontGC = XCreateGC( xwc->display, xwc->fontPixmap, GCFont | GCForeground | GCBackground, &values );
}

static void s_gt_xwc_DestroyFont( X_GTXWC *xwc )
{
   int page;

   for( page = 0; page < 256; ++page )
      x_xfree( xwc->fontPages[page] );
   if( xwc->fontGC )
      XFreeGC( xwc->display, xwc->fontGC );
   if( xwc->fontPixmap )
      XFreePixmap( xwc->display, xwc->fontPixmap );
   if( xwc->font )
      XFreeFont( xwc->display, xwc->font );
   x_xfree( xwc->fontName );
   x_xfree( xwc->fontSel );
}

static const uint16_t *s_gt_xwc_Glyph( X_GTXWC *xwc, uint16_t code )
{
   X_GTXWC_FONT_PAGE *page;
   XChar2b            character;
   XImage            *image;
   unsigned int       index;
   int                x, y;

   if( !s_gt_xwc_HasGlyph( xwc, code ) )
   {
      if( code == 0x00A0 )
         code = 0x0020;
      else if( s_gt_xwc_HasGlyph( xwc, 0xFFFD ) )
         code = 0xFFFD;
      else
         code = 0x003F;
   }

   index = code & 0xFF;
   page = xwc->fontPages[code >> 8];
   if( !page )
   {
      page = ( X_GTXWC_FONT_PAGE * )x_xgrabz( sizeof( X_GTXWC_FONT_PAGE ) );
      xwc->fontPages[code >> 8] = page;
   }

   if( !page->loaded[index] )
   {
      character.byte1 = ( unsigned char )( code >> 8 );
      character.byte2 = ( unsigned char )index;
      XSetForeground( xwc->display, xwc->fontGC, 0 );
      XFillRectangle( xwc->display, xwc->fontPixmap, xwc->fontGC, 0, 0, X_GTXWC_FONT_WIDTH, X_GTXWC_FONT_HEIGHT );
      XSetForeground( xwc->display, xwc->fontGC, 1 );
      XDrawString16( xwc->display, xwc->fontPixmap, xwc->fontGC, 0, xwc->font->ascent, &character, 1 );

      image = XGetImage( xwc->display, xwc->fontPixmap, 0, 0, X_GTXWC_FONT_WIDTH, X_GTXWC_FONT_HEIGHT, 1, ZPixmap );
      if( !image )
      {
         fputs( "xDev GTXWC: cannot read the X11 font glyph bitmap\n", stderr );
         x_quitRaise();
         return NULL;
      }

      for( y = 0; y < X_GTXWC_FONT_HEIGHT; ++y )
         for( x = 0; x < X_GTXWC_FONT_WIDTH; ++x )
            if( XGetPixel( image, x, y ) != 0 )
               page->bits[index][y] |= ( uint16_t )( 0x8000 >> x );

      XDestroyImage( image );
      page->loaded[index] = 1;
   }

   return page->bits[index];
}

static void s_gt_xwc_FillRect( X_GTXWC *xwc, int top, int left, int bottom, int right, uint32_t color )
{
   int x, y;

   top = X_MAX( top, 0 );
   left = X_MAX( left, 0 );
   bottom = X_MIN( bottom, xwc->canvasHeight - 1 );
   right = X_MIN( right, xwc->canvasWidth - 1 );

   for( y = top; y <= bottom; ++y )
   {
      uint32_t *row = xwc->canvas + ( size_t )y * xwc->canvasWidth;

      for( x = left; x <= right; ++x )
         row[x] = color;
   }
}

static int s_gt_xwc_BoxStroke( int position, int center, int style, int thickness, int gap )
{
   int size = IIF( style == 2, thickness * 2, thickness );
   int low = ( size - 1 ) / 2;
   int high = size / 2;

   if( style == 3 )
   {
      int before = position >= center - gap - low && position <= center - gap + high;
      int after = position >= center + gap - low && position <= center + gap + high;

      return before || after;
   }

   return position >= center - low && position <= center + high;
}

static int s_gt_xwc_BoxPixel( uint16_t code, int width, int height, int x, int y )
{
   int          cx = ( width - 1 ) / 2;
   int          cy = ( height - 1 ) / 2;
   int          thickness = X_MAX( X_MIN( width, height ) / 9, 1 );
   int          gapX = X_MIN( thickness * 2, cx );
   int          gapY = X_MIN( thickness * 2, cy );
   unsigned int arms;
   int          left, right, up, down;
   int          joinX, joinY, limit, sign, dashes = 0;

   if( code >= 0x2580 )
   {
      int quadrant = 0;

      if( code == 0x2580 )
         return y < ( ( int64_t )height + 1 ) / 2;

      if( code <= 0x2588 )
         return y >= height - ( ( int64_t )height * ( code - 0x2580 ) + 7 ) / 8;

      if( code <= 0x258F )
         return x < ( ( int64_t )width * ( 0x2590 - code ) + 7 ) / 8;

      if( code == 0x2590 )
         return x >= ( ( int64_t )width + 1 ) / 2;

      if( code <= 0x2593 )
         return s_boxShade[( y & 1 ) * 2 + ( x & 1 )] < code - 0x2590;

      if( code == 0x2594 )
         return y < ( ( int64_t )height + 7 ) / 8;

      if( code == 0x2595 )
         return x >= width - ( ( int64_t )width + 7 ) / 8;

      if( y >= ( ( int64_t )height + 1 ) / 2 )
         quadrant += 2;
      if( x >= ( ( int64_t )width + 1 ) / 2 )
         quadrant += 1;
      return ( s_boxQuadrants[code - 0x2596] & ( 1 << quadrant ) ) != 0;
   }
   if( code >= 0x2571 && code <= 0x2573 )
   {
      int64_t across = ( int64_t )x * ( height - 1 );
      int64_t downwards = ( int64_t )y * ( width - 1 );
      int64_t tolerance = ( int64_t )X_MAX( width, height ) * thickness / 2;
      int64_t corner = ( int64_t )( width - 1 ) * ( height - 1 );
      int     rising = code != 0x2572 && llabs( across + downwards - corner ) <= tolerance;
      int     falling = code != 0x2571 && llabs( across - downwards ) <= tolerance;

      return rising || falling;
   }
   arms = s_boxArms[code - 0x2500];
   left = arms & 3;
   right = ( arms >> 2 ) & 3;
   up = ( arms >> 4 ) & 3;
   down = ( arms >> 6 ) & 3;
   if( code >= 0x256D && code <= 0x2570 )
   {
      int spaceX = X_MIN( cx, width - cx - 1 );
      int spaceY = X_MIN( cy, height - cy - 1 );
      int radius = X_MIN( X_MIN( width, height ) / 3, X_MIN( spaceX, spaceY ) );

      if( radius > 0 )
      {
         int     arcX = cx + IIF( right, radius, -radius );
         int     arcY = cy + IIF( down, radius, -radius );
         int64_t dx = x - arcX;
         int64_t dy = y - arcY;
         int     inside;

         if( IIF( right, x >= arcX, x <= arcX ) && s_gt_xwc_BoxStroke( y, cy, 1, thickness, gapY ) )
            return 1;

         if( IIF( down, y >= arcY, y <= arcY ) && s_gt_xwc_BoxStroke( x, cx, 1, thickness, gapX ) )
            return 1;

         inside = IIF( right, x <= arcX, x >= arcX ) && IIF( down, y <= arcY, y >= arcY );
         return inside && llabs( dx * dx + dy * dy - ( int64_t )radius * radius ) <= ( int64_t )radius * thickness;
      }
   }
   if( code >= 0x2504 && code <= 0x2507 )
      dashes = 3;
   else if( code >= 0x2508 && code <= 0x250B )
      dashes = 4;
   else if( code >= 0x254C && code <= 0x254F )
      dashes = 2;
   if( dashes )
   {
      int64_t step = IIF( up || down, ( int64_t )y * dashes * 2 / height, ( int64_t )x * dashes * 2 / width );

      if( step & 1 )
         return 0;
   }
   if( up == 3 || down == 3 )
      joinX = gapX + thickness / 2;
   else
      joinX = thickness * IIF( up == 2 || down == 2, 2, 1 ) / 2;
   if( left == 3 || right == 3 )
      joinY = gapY + thickness / 2;
   else
      joinY = thickness * IIF( left == 2 || right == 2, 2, 1 ) / 2;
   sign = IIF( y < cy, -1, 1 );
   if( left && s_gt_xwc_BoxStroke( y, cy, left, thickness, gapY ) )
   {
      limit = cx + joinX;
      if( left != 3 && up == 3 && down == 3 && !right )
         limit = cx - gapX + thickness / 2;
      else if( left == 3 && ( ( up == 3 && !down ) || ( down == 3 && !up ) ) )
         limit = cx + IIF( up, sign, -sign ) * gapX + thickness / 2;
      if( x <= limit )
         return 1;
   }
   if( right && s_gt_xwc_BoxStroke( y, cy, right, thickness, gapY ) )
   {
      limit = cx - joinX;
      if( right != 3 && up == 3 && down == 3 && !left )
         limit = cx + gapX - ( thickness - 1 ) / 2;
      else if( right == 3 && ( ( up == 3 && !down ) || ( down == 3 && !up ) ) )
         limit = cx + IIF( down, sign, -sign ) * gapX - ( thickness - 1 ) / 2;
      if( x >= limit )
         return 1;
   }
   sign = IIF( x < cx, -1, 1 );
   if( up && s_gt_xwc_BoxStroke( x, cx, up, thickness, gapX ) )
   {
      limit = cy + joinY;
      if( up != 3 && left == 3 && right == 3 && !down )
         limit = cy - gapY + thickness / 2;
      else if( up == 3 && ( ( left == 3 && !right ) || ( right == 3 && !left ) ) )
         limit = cy + IIF( left, sign, -sign ) * gapY + thickness / 2;
      if( y <= limit )
         return 1;
   }
   if( down && s_gt_xwc_BoxStroke( x, cx, down, thickness, gapX ) )
   {
      limit = cy - joinY;
      if( down != 3 && left == 3 && right == 3 && !up )
         limit = cy + gapY - ( thickness - 1 ) / 2;
      else if( down == 3 && ( ( left == 3 && !right ) || ( right == 3 && !left ) ) )
         limit = cy + IIF( right, sign, -sign ) * gapY - ( thickness - 1 ) / 2;
      if( y >= limit )
         return 1;
   }

   return 0;
}

static void s_gt_xwc_GfxPixel( X_GTXWC *xwc, int x, int y, uint32_t color )
{
   uint32_t *pixel = xwc->canvas + ( size_t )y * xwc->canvasWidth + x;

   if( xwc->gfxMode == GFX_MODE_XOR )
      *pixel ^= color;
   else
      *pixel = color;
}

static void s_gt_xwc_DrawGlyph( X_GTXWC *xwc, uint16_t code, int top, int left, int height, int width, uint32_t color,
                                const X_GTXWC_RECT *clip, int graphics )
{
   const uint16_t *glyph;
   int             x, y;
   int             firstX, firstY, lastX, lastY;
   int             box = ( xwc->fontAttribute & X_GTI_FONTA_DRAWBOX ) && code >= 0x2500 && code <= 0x259F;

   if( height <= 0 || width <= 0 )
      return;

   firstX = ( int )X_MIN( ( int64_t )width, X_MAX( 0, ( int64_t )IIF( clip, clip->left, 0 ) - left ) );
   firstY = ( int )X_MIN( ( int64_t )height, X_MAX( 0, ( int64_t )IIF( clip, clip->top, 0 ) - top ) );
   lastX = ( int )X_MIN( ( int64_t )width - 1, ( int64_t )IIF( clip, clip->right, xwc->canvasWidth - 1 ) - left );
   lastY = ( int )X_MIN( ( int64_t )height - 1, ( int64_t )IIF( clip, clip->bottom, xwc->canvasHeight - 1 ) - top );
   if( firstX > lastX || firstY > lastY )
      return;

   glyph = IIF( box, NULL, s_gt_xwc_Glyph( xwc, code ) );
   if( !box && !glyph )
      return;

   for( y = firstY; y <= lastY; ++y )
   {
      int      sourceY = ( int )( ( int64_t )y * X_GTXWC_FONT_HEIGHT / height );
      int      targetY = top + y;
      uint16_t bits;

      if( targetY < 0 || targetY >= xwc->canvasHeight )
         continue;

      bits = IIF( box, 0, glyph[sourceY] );
      for( x = firstX; x <= lastX; ++x )
      {
         int sourceX = ( int )( ( int64_t )x * X_GTXWC_FONT_WIDTH / width );
         int targetX = left + x;
         int ink;

         if( targetX < 0 || targetX >= xwc->canvasWidth )
            continue;
         ink = IIF( box, s_gt_xwc_BoxPixel( code, width, height, x, y ), ( bits & ( 0x8000 >> sourceX ) ) != 0 );
         if( ink )
         {
            if( graphics )
               s_gt_xwc_GfxPixel( xwc, targetX, targetY, color );
            else
               xwc->canvas[( size_t )targetY * xwc->canvasWidth + targetX] = color;
         }
      }
   }
}

static void s_gt_xwc_DestroyImage( X_GTXWC *xwc )
{
   if( xwc->image )
   {
      xwc->image->data = NULL;
      XDestroyImage( xwc->image );
      xwc->image = NULL;
   }

   x_xfree( xwc->imageData );
   x_xfree( xwc->canvas );
   xwc->imageData = NULL;
   xwc->canvas = NULL;
}

static void s_gt_xwc_CreateCanvas( X_GTXWC *xwc, int rows, int cols )
{
   size_t       imageSize;
   unsigned int depth, width, height;

   xwc->selectionActive = 0;
   xwc->selecting = 0;
   memset( xwc->clickPending, 0, sizeof( xwc->clickPending ) );
   s_gt_xwc_DestroyImage( xwc );
   xwc->rows = rows;
   xwc->cols = cols;
   xwc->canvasWidth = cols * xwc->cellWidth;
   xwc->canvasHeight = rows * xwc->cellHeight;
   xwc->canvas = ( uint32_t * )x_xgrabzArray( ( size_t )xwc->canvasWidth * xwc->canvasHeight, sizeof( uint32_t ) );

   depth = ( unsigned int )DefaultDepth( xwc->display, xwc->screen );
   width = ( unsigned int )xwc->canvasWidth;
   height = ( unsigned int )xwc->canvasHeight;
   xwc->image = XCreateImage( xwc->display, xwc->visual, depth, ZPixmap, 0, NULL, width, height, 32, 0 );
   if( !xwc->image )
   {
      fputs( "xDev GTXWC: XCreateImage() failed\n", stderr );
      x_quitRaise();
      return;
   }

   imageSize = ( size_t )xwc->image->bytes_per_line * xwc->canvasHeight;
   xwc->imageData = ( char * )x_xgrabz( imageSize );
   xwc->image->data = xwc->imageData;
   s_gt_xwc_FillRect( xwc, 0, 0, xwc->canvasHeight - 1, xwc->canvasWidth - 1, s_gt_xwc_Color( xwc, 0 ) );
   s_gt_xwc_UpdateMousePosition( xwc, xwc->mouseX, xwc->mouseY, 0 );
   xwc->dirty = 1;
}

static unsigned long s_gt_xwc_Channel( unsigned int value, unsigned long mask )
{
   unsigned int  shift = 0;
   unsigned long maximum;

   if( mask == 0 )
      return 0;

   while( ( mask & 1 ) == 0 )
   {
      mask >>= 1;
      ++shift;
   }
   maximum = mask;

   return ( ( unsigned long )( value * maximum + 127 ) / 255 ) << shift;
}

static void s_gt_xwc_AddColor( XColor *palette, int *count, int capacity, unsigned int red, unsigned int green,
                               unsigned int blue )
{
   int index;

   red *= 257;
   green *= 257;
   blue *= 257;
   for( index = 0; index < *count; ++index )
      if( palette[index].red == red && palette[index].green == green && palette[index].blue == blue )
         return;

   if( *count < capacity )
   {
      XColor *entry = &palette[( *count )++];

      entry->red = ( unsigned short )red;
      entry->green = ( unsigned short )green;
      entry->blue = ( unsigned short )blue;
   }
}

static void s_gt_xwc_ColorRamp( X_GTXWC *xwc, unsigned long mask, char flag )
{
   unsigned long unit = mask & ( ~mask + 1 );
   unsigned long maximum = mask / unit;
   XColor       *ramp = ( XColor * )x_xgrabzArray( maximum + 1, sizeof( XColor ) );
   unsigned long index;

   for( index = 0; index <= maximum; ++index )
   {
      ramp[index].pixel = index * unit;
      ramp[index].red = ramp[index].green = ramp[index].blue = ( unsigned short )( index * 65535 / maximum );
      ramp[index].flags = flag;
   }
   XStoreColors( xwc->display, xwc->colormap, ramp, ( int )( maximum + 1 ) );
   x_xfree( ramp );
}

static void s_gt_xwc_InitColors( X_GTXWC *xwc )
{
   Visual *visual = xwc->visual;
   XColor *palette;
   int     count, index, mapSize;
   int     gray = visual->class == GrayScale || visual->class == StaticGray;

   xwc->colormap = DefaultColormap( xwc->display, xwc->screen );
   if( visual->class == TrueColor )
      return;

   if( visual->class == PseudoColor || visual->class == GrayScale || visual->class == DirectColor )
   {
      xwc->colormap = XCreateColormap( xwc->display, RootWindow( xwc->display, xwc->screen ), visual, AllocAll );
      xwc->ownColormap = 1;
   }
   if( visual->class == DirectColor )
   {
      s_gt_xwc_ColorRamp( xwc, visual->red_mask, DoRed );
      s_gt_xwc_ColorRamp( xwc, visual->green_mask, DoGreen );
      s_gt_xwc_ColorRamp( xwc, visual->blue_mask, DoBlue );
      return;
   }

   count = visual->map_entries;
   palette = ( XColor * )x_xgrabzArray( count, sizeof( XColor ) );
   if( visual->class == PseudoColor )
   {
      int used = 0, levels = 2, red, green, blue, last;

      s_gt_xwc_AddColor( palette, &used, count, 0, 0, 0 );
      s_gt_xwc_AddColor( palette, &used, count, 255, 255, 255 );
      for( index = 0; index < 16; ++index )
      {
         const uint8_t *rgb = s_defaultColors[index];

         s_gt_xwc_AddColor( palette, &used, count, rgb[0], rgb[1], rgb[2] );
      }
      for( index = 0; index < 16; ++index )
      {
         const uint8_t *rgb = s_defaultColors[index];

         s_gt_xwc_AddColor( palette, &used, count, 255 - rgb[0], 255 - rgb[1], 255 - rgb[2] );
      }
      while( levels < 6 && ( levels + 1 ) * ( levels + 1 ) * ( levels + 1 ) <= count - used )
         ++levels;
      last = levels - 1;
      if( levels * levels * levels <= count - used )
         for( red = 0; red < levels; ++red )
            for( green = 0; green < levels; ++green )
               for( blue = 0; blue < levels; ++blue )
                  s_gt_xwc_AddColor( palette, &used, count, red * 255 / last, green * 255 / last, blue * 255 / last );

      for( levels = 2; used < count && levels <= 512; levels *= 2 )
         for( index = 1; used < count && index < levels; index += 2 )
         {
            unsigned int value = index * 255 / levels;

            s_gt_xwc_AddColor( palette, &used, count, value, value, value );
         }
      while( used < count )
         palette[used++] = palette[0];
   }
   else if( visual->class == GrayScale )
      for( index = 0; index < count; ++index )
      {
         unsigned short level = ( unsigned short )IIF( count > 1, index * 65535 / ( count - 1 ), 0 );

         palette[index].red = palette[index].green = palette[index].blue = level;
      }

   for( index = 0; index < count; ++index )
   {
      palette[index].pixel = ( unsigned long )index;
      palette[index].flags = DoRed | DoGreen | DoBlue;
   }
   if( xwc->ownColormap )
      XStoreColors( xwc->display, xwc->colormap, palette, count );
   XQueryColors( xwc->display, xwc->colormap, palette, count );

   mapSize = IIF( gray, 256, 32 * 32 * 32 + 256 );
   xwc->pixelMap = ( unsigned long * )x_xgrabArray( mapSize, sizeof( unsigned long ) );
   for( index = 0; index < mapSize; ++index )
   {
      int grayEntry = gray || index >= 32 * 32 * 32;
      int red = IIF( grayEntry, index & 255, ( ( index >> 10 ) * 255 + 15 ) / 31 );
      int green = IIF( grayEntry, red, ( ( ( index >> 5 ) & 31 ) * 255 + 15 ) / 31 );
      int blue = IIF( grayEntry, red, ( ( index & 31 ) * 255 + 15 ) / 31 );
      int entry, nearest = 0, distance = INT_MAX;

      for( entry = 0; entry < count; ++entry )
      {
         int dr = red - ( palette[entry].red + 128 ) / 257;
         int dg = green - ( palette[entry].green + 128 ) / 257;
         int db = blue - ( palette[entry].blue + 128 ) / 257;
         int current = dr * dr + dg * dg + db * db;

         if( current < distance )
         {
            distance = current;
            nearest = entry;
         }
      }
      xwc->pixelMap[index] = palette[nearest].pixel;
   }
   x_xfree( palette );
}

static unsigned long s_gt_xwc_Pixel( X_GTXWC *xwc, uint32_t color )
{
   Visual       *visual = xwc->visual;
   unsigned long redBits, greenBits, blueBits;

   if( xwc->pixelMap )
   {
      unsigned int red = ( color >> 16 ) & 255;
      unsigned int green = ( color >> 8 ) & 255;
      unsigned int blue = color & 255;
      unsigned int index;

      if( visual->class == GrayScale || visual->class == StaticGray )
         index = ( red * 77 + green * 150 + blue * 29 + 128 ) >> 8;
      else if( red == green && green == blue )
         index = 32 * 32 * 32 + red;
      else
      {
         /* 5 bits for each channel */
         unsigned int red5 = ( red * 31 + 127 ) / 255;
         unsigned int green5 = ( green * 31 + 127 ) / 255;
         unsigned int blue5 = ( blue * 31 + 127 ) / 255;

         index = ( red5 << 10 ) | ( green5 << 5 ) | blue5;
      }
      return xwc->pixelMap[index];
   }

   redBits = s_gt_xwc_Channel( ( color >> 16 ) & 0xFF, visual->red_mask );
   greenBits = s_gt_xwc_Channel( ( color >> 8 ) & 0xFF, visual->green_mask );
   blueBits = s_gt_xwc_Channel( color & 0xFF, visual->blue_mask );

   return redBits | greenBits | blueBits;
}

/* the window rarely holds whole cells (maximized, full screen, before the grid follows the window):
   the area outside the grid takes the SetColorExt() background, otherwise the background of the
   SetColor() standard colour, as the margin of a terminal */
static void s_gt_xwc_FillEdges( X_GTXWC *xwc )
{
   uint32_t     back = xwc->gt->background;
   unsigned int rightWidth = ( unsigned int )( xwc->windowWidth - xwc->frameWidth );
   unsigned int bottomHeight = ( unsigned int )( xwc->windowHeight - xwc->frameHeight );
   unsigned int windowHeight = ( unsigned int )xwc->windowHeight;
   unsigned int frameWidth = ( unsigned int )xwc->frameWidth;

   if( back == X_GT_RGB_DEFAULT )
      back = s_gt_xwc_Color( xwc, xwc->gt->colors[0] >> 4 );
   XSetForeground( xwc->display, xwc->gc, s_gt_xwc_Pixel( xwc, back ) );
   if( xwc->frameWidth < xwc->windowWidth )
      XFillRectangle( xwc->display, xwc->window, xwc->gc, xwc->frameWidth, 0, rightWidth, windowHeight );
   if( xwc->frameHeight < xwc->windowHeight )
      XFillRectangle( xwc->display, xwc->window, xwc->gc, 0, xwc->frameHeight, frameWidth, bottomHeight );
}

static void s_gt_xwc_RepaintFrame( X_GTXWC *xwc )
{
   if( xwc->mapped && xwc->framePixmap )
   {
      unsigned int width = ( unsigned int )xwc->frameWidth;
      unsigned int height = ( unsigned int )xwc->frameHeight;

      XCopyArea( xwc->display, xwc->framePixmap, xwc->window, xwc->gc, 0, 0, width, height, 0, 0 );
      s_gt_xwc_FillEdges( xwc );
      XFlush( xwc->display );
   }
}

static void s_gt_xwc_PresentPreedit( X_GTXWC *xwc )
{
   int      row = X_MIN( X_MAX( xwc->cursorRow, 0 ), xwc->rows - 1 );
   int      col = X_MIN( X_MAX( xwc->cursorCol, 0 ), xwc->cols - 1 );
   int      anchor = row * xwc->cols + col;
   int      available = xwc->rows * xwc->cols - anchor;
   int      offset = X_MAX( 0, xwc->preeditCaret - available + 1 );
   int      count = X_MIN( xwc->preeditLength - offset, available );
   int      index, color = 0x07;
   uint8_t  attr;
   uint16_t character;
   uint32_t foreground, background;
   int      ctrl = xwc->fontAttribute & X_GTI_FONTA_CTRLCHARS;
   int      fixed = xwc->fontAttribute & X_GTI_FONTA_NOSTRETCH;

   if( !xwc->preeditVisible || !xwc->focused )
      return;

   X_GTSELF_GETSCRCHAR( xwc->gt, row, col, &color, &attr, &character );
   s_gt_xwc_CellColors( xwc, row, col, color, &foreground, &background );
   for( index = 0; index < count; ++index )
   {
      X_GTXWC_PREEDIT_CELL *cell = &xwc->preedit[offset + index];
      uint16_t              code = 0xFFFD;
      int                   box;
      const uint16_t       *glyph;
      int                   top = ( anchor + index ) / xwc->cols * xwc->cellHeight;
      int                   left = ( anchor + index ) % xwc->cols * xwc->cellWidth;
      int                   reverse = ( cell->feedback & ( XIMReverse | XIMPrimary | XIMHighlight ) ) != 0;
      int           underline = !reverse || ( cell->feedback & ( XIMUnderline | XIMSecondary | XIMTertiary ) ) != 0;
      unsigned long fg = s_gt_xwc_Pixel( xwc, IIF( reverse, background, foreground ) );
      unsigned long bg = s_gt_xwc_Pixel( xwc, IIF( reverse, foreground, background ) );
      int           x, y;

      if( cell->code <= 0xFFFF )
         code = IIF( ctrl, x_u16Ctrl( cell->code ), cell->code );
      box = ( xwc->fontAttribute & X_GTI_FONTA_DRAWBOX ) && code >= 0x2500 && code <= 0x259F;
      glyph = IIF( box, NULL, s_gt_xwc_Glyph( xwc, code ) );
      if( !box && !glyph )
         return;

      for( y = 0; y < xwc->cellHeight; ++y )
         for( x = 0; x < xwc->cellWidth; ++x )
         {
            int glyphY = IIF( fixed, y, y * X_GTXWC_FONT_HEIGHT / xwc->cellHeight );
            int glyphX = IIF( fixed, x, x * X_GTXWC_FONT_WIDTH / xwc->cellWidth );
            int ink = 0;

            if( box )
               ink = s_gt_xwc_BoxPixel( code, xwc->cellWidth, xwc->cellHeight, x, y );
            else if( glyphY < X_GTXWC_FONT_HEIGHT && glyphX < X_GTXWC_FONT_WIDTH )
               ink = ( glyph[glyphY] & ( 0x8000 >> glyphX ) ) != 0;
            if( underline && y == xwc->cellHeight - 1 )
               ink = 1;
            XPutPixel( xwc->image, left + x, top + y, IIF( ink, fg, bg ) );
         }
   }
   if( xwc->preeditCaretStyle != XIMIsInvisible )
   {
      int position = anchor + xwc->preeditCaret - offset;
      int top = position / xwc->cols * xwc->cellHeight;
      int left = position % xwc->cols * xwc->cellWidth;
      int width = IIF( xwc->preeditCaretStyle == XIMIsSecondary, xwc->cellWidth, 1 );
      int height = IIF( xwc->preeditCaretStyle == XIMIsSecondary, X_MIN( 2, xwc->cellHeight ), xwc->cellHeight );
      unsigned long pixel = s_gt_xwc_Pixel( xwc, foreground );
      int           x, y;

      if( xwc->preeditCaretStyle == XIMIsSecondary )
         top += xwc->cellHeight - height;
      for( y = 0; y < height; ++y )
         for( x = 0; x < width; ++x )
            XPutPixel( xwc->image, left + x, top + y, pixel );
   }
}

static void s_gt_xwc_Present( X_GTXWC *xwc )
{
   int          x, y;
   int          first = X_MIN( xwc->selectionAnchor, xwc->selectionEnd );
   int          last = X_MAX( xwc->selectionAnchor, xwc->selectionEnd );
   unsigned int width = ( unsigned int )xwc->canvasWidth;
   unsigned int height = ( unsigned int )xwc->canvasHeight;
   unsigned int depth = ( unsigned int )xwc->desktopDepth;
   int          cursorShown;

   if( !xwc->mapped || !xwc->image || !xwc->dirty || xwc->resizeFramePending || X_GTSELF_DISPCOUNT( xwc->gt ) != 0 )
      return;

   if( !xwc->framePixmap || xwc->frameWidth != xwc->canvasWidth || xwc->frameHeight != xwc->canvasHeight )
   {
      if( xwc->framePixmap )
         XFreePixmap( xwc->display, xwc->framePixmap );
      xwc->framePixmap = XCreatePixmap( xwc->display, xwc->window, width, height, depth );
      xwc->frameWidth = xwc->canvasWidth;
      xwc->frameHeight = xwc->canvasHeight;
   }

   for( y = 0; y < xwc->canvasHeight; ++y )
      for( x = 0; x < xwc->canvasWidth; ++x )
      {
         uint32_t color = xwc->canvas[( size_t )y * xwc->canvasWidth + x];
         int      cell = y / xwc->cellHeight * xwc->cols + x / xwc->cellWidth;

         if( xwc->selectionActive && cell >= first && cell <= last )
            color ^= 0xFFFFFF;
         XPutPixel( xwc->image, x, y, s_gt_xwc_Pixel( xwc, color ) );
      }

   s_gt_xwc_PresentPreedit( xwc );
   XPutImage( xwc->display, xwc->framePixmap, xwc->gc, xwc->image, 0, 0, 0, 0, width, height );

   cursorShown = xwc->cursorVisible && !( xwc->preeditVisible && xwc->focused ) && xwc->cursorStyle != SC_NONE;
   if( xwc->cursorRow < 0 || xwc->cursorRow >= xwc->rows || xwc->cursorCol < 0 || xwc->cursorCol >= xwc->cols )
      cursorShown = 0;
   if( cursorShown )
   {
      int          cursorHeight = xwc->cellHeight;
      int          top, left = xwc->cursorCol * xwc->cellWidth;
      unsigned int cellWidth = ( unsigned int )xwc->cellWidth;

      if( xwc->cursorStyle == SC_NORMAL )
         cursorHeight = X_MIN( xwc->cellHeight, 2 );
      else if( xwc->cursorStyle == SC_INSERT )
         cursorHeight = X_MAX( xwc->cellHeight / 2, 1 );
      top = ( xwc->cursorRow + 1 ) * xwc->cellHeight - cursorHeight;
      if( xwc->cursorStyle == SC_SPECIAL2 )
         top = xwc->cursorRow * xwc->cellHeight;
      XSetForeground( xwc->display, xwc->gc, s_gt_xwc_Pixel( xwc, s_gt_xwc_Color( xwc, 15 ) ) );
      XFillRectangle( xwc->display, xwc->framePixmap, xwc->gc, left, top, cellWidth, ( unsigned int )cursorHeight );
   }

   s_gt_xwc_RepaintFrame( xwc );
   xwc->dirty = 0;
}

/* ---------------------------------------------------------------------------------------------
   Keyboard, mouse and X11 events
   --------------------------------------------------------------------------------------------- */

static void s_gt_xwc_AddKey( X_GTXWC *xwc, int key )
{
   int next = ( xwc->keyIn + 1 ) % xwc->keyCapacity;

   if( next == xwc->keyOut )
   {
      int  count = xwc->keyCapacity - 1;
      int  index;
      int  capacity = xwc->keyCapacity * 2;
      int *keys = ( int * )x_xgrabArray( capacity, sizeof( int ) );

      for( index = 0; index < count; ++index )
         keys[index] = xwc->keys[( xwc->keyOut + index ) % xwc->keyCapacity];
      x_xfree( xwc->keys );
      xwc->keys = keys;
      xwc->keyCapacity = capacity;
      xwc->keyOut = 0;
      xwc->keyIn = count;
      next = count + 1;
   }
   xwc->keys[xwc->keyIn] = key;
   xwc->keyIn = next;
}

static void s_gt_xwc_UpdateModifiers( X_GTXWC *xwc )
{
   Display     *display = xwc->display;
   unsigned int alt = XkbKeysymToModifiers( display, XK_Alt_L ) | XkbKeysymToModifiers( display, XK_Alt_R );
   unsigned int meta = XkbKeysymToModifiers( display, XK_Meta_L ) | XkbKeysymToModifiers( display, XK_Meta_R );

   xwc->altMask = alt | meta;
   xwc->capsLockMask = XkbKeysymToModifiers( xwc->display, XK_Caps_Lock );
   xwc->numLockMask = XkbKeysymToModifiers( xwc->display, XK_Num_Lock );
   xwc->scrollLockMask = XkbKeysymToModifiers( xwc->display, XK_Scroll_Lock );
}

static int s_gt_xwc_KeyFlags( X_GTXWC *xwc, unsigned int state )
{
   int flags = 0;

   if( state & ShiftMask )
      flags |= KF_SHIFT;
   if( state & ControlMask )
      flags |= KF_CTRL;
   if( state & xwc->altMask )
      flags |= KF_ALT;

   return flags;
}

static int s_gt_xwc_KeyCode( KeySym symbol )
{
   switch( symbol )
   {
      case XK_Up:
      case XK_KP_Up: return KX_UP;
      case XK_Down:
      case XK_KP_Down: return KX_DOWN;
      case XK_Left:
      case XK_KP_Left: return KX_LEFT;
      case XK_Right:
      case XK_KP_Right: return KX_RIGHT;
      case XK_Home:
      case XK_KP_Home: return KX_HOME;
      case XK_End:
      case XK_KP_End: return KX_END;
      case XK_Page_Up:
      case XK_KP_Page_Up: return KX_PGUP;
      case XK_Page_Down:
      case XK_KP_Page_Down: return KX_PGDN;
      case XK_Insert:
      case XK_KP_Insert: return KX_INS;
      case XK_Delete:
      case XK_KP_Delete: return KX_DEL;
      case XK_BackSpace: return KX_BS;
      case XK_Tab:
      case XK_ISO_Left_Tab:
      case XK_KP_Tab: return KX_TAB;
      case XK_Escape: return KX_ESC;
      case XK_Return:
      case XK_KP_Enter: return KX_ENTER;
      case XK_KP_Begin: return KX_CENTER;
      case XK_Print: return KX_PRTSCR;
      case XK_Pause: return KX_PAUSE;
      case XK_F1: return KX_F1;
      case XK_F2: return KX_F2;
      case XK_F3: return KX_F3;
      case XK_F4: return KX_F4;
      case XK_KP_F1: return KX_F1;
      case XK_KP_F2: return KX_F2;
      case XK_KP_F3: return KX_F3;
      case XK_KP_F4: return KX_F4;
      case XK_F5: return KX_F5;
      case XK_F6: return KX_F6;
      case XK_F7: return KX_F7;
      case XK_F8: return KX_F8;
      case XK_F9: return KX_F9;
      case XK_F10: return KX_F10;
      case XK_F11: return KX_F11;
      case XK_F12: return KX_F12;
      default: return 0;
   }
}

/* the character of a keypad key that types one, otherwise 0 */
static int s_gt_xwc_KeypadChar( KeySym symbol )
{
   switch( symbol )
   {
      case XK_KP_Add: return '+';
      case XK_KP_Subtract: return '-';
      case XK_KP_Multiply: return '*';
      case XK_KP_Divide: return '/';
      case XK_KP_Decimal: return '.';
      case XK_KP_Separator: return ',';
      case XK_KP_Equal: return '=';
      case XK_KP_Space: return ' ';
      default: return 0;
   }
}

static int s_gt_xwc_InputError( Display *display, XErrorEvent *event )
{
   X_GTXWC_INPUT_TRAP *trap = s_inputTrap;

   while( trap )
   {
      Window root = RootWindow( display, trap->xwc->screen );
      int    foreign = event->resourceid != trap->xwc->window && event->resourceid != root;
      int    sendFailed = event->error_code == BadWindow && event->request_code == X_SendEvent;

      /* a window of another client vanished before XSendEvent() reached it */
      if( trap->xwc->display == display && sendFailed && foreign )
         return 0;

      if( trap->previous != s_gt_xwc_InputError )
         return trap->previous( display, event );

      trap = trap->parent;
   }

   return 0;
}

static void s_gt_xwc_BeginInputTrap( X_GTXWC *xwc, X_GTXWC_INPUT_TRAP *trap )
{
   trap->parent = s_inputTrap;
   trap->xwc = xwc;
   trap->previous = XSetErrorHandler( s_gt_xwc_InputError );
   s_inputTrap = trap;
}

static void s_gt_xwc_EndInputTrap( X_GTXWC_INPUT_TRAP *trap )
{
   XSync( trap->xwc->display, False );
   XSetErrorHandler( trap->previous );
   s_inputTrap = trap->parent;
}

static void s_gt_xwc_InputTrapDrop( X_GTXWC_INPUT_TRAP **trap )
{
   if( *trap )
      s_gt_xwc_EndInputTrap( *trap );
}

static void s_gt_xwc_PreeditClear( X_GTXWC *xwc )
{
   if( xwc->preeditActive )
      xwc->dirty = 1;
   x_xfree( xwc->preedit );
   xwc->preedit = NULL;
   xwc->preeditLength = xwc->preeditCaret = 0;
   xwc->preeditCaretStyle = XIMIsPrimary;
   xwc->preeditActive = 0;
   xwc->preeditVisible = 0;
}

static int s_gt_xwc_PreeditStart( XIC context, XPointer client, XPointer data )
{
   X_GTXWC *xwc = ( X_GTXWC * )client;

   UNUSED( data );
   if( context != xwc->inputContext || xwc->inputClosing )
      return 0;

   s_gt_xwc_PreeditClear( xwc );
   xwc->preeditActive = 1;
   xwc->dirty = 1;

   return X_GTXWC_PREEDIT_MAX;
}

static void s_gt_xwc_PreeditDone( XIC context, XPointer client, XPointer data )
{
   X_GTXWC *xwc = ( X_GTXWC * )client;

   UNUSED( data );
   if( context == xwc->inputContext )
      s_gt_xwc_PreeditClear( xwc );
}

static uint32_t s_gt_xwc_PreeditCode( uint32_t code )
{
   return IIF( code <= 0x10FFFF && !( code >= 0xD800 && code <= 0xDFFF ), code, 0xFFFD );
}

static void s_gt_xwc_PreeditDraw( XIC context, XPointer client, XPointer data )
{
   X_GTXWC                      *xwc = ( X_GTXWC * )client;
   XIMPreeditDrawCallbackStruct *draw = ( XIMPreeditDrawCallbackStruct * )data;
   XIMText                      *text;
   int                           first, removed, added, length, index;

   if( context != xwc->inputContext || !xwc->preeditActive || !draw )
      return;

   first = draw->chg_first;
   removed = draw->chg_length;
   text = draw->text;
   if( first < 0 || first > xwc->preeditLength || removed < 0 || removed > xwc->preeditLength - first )
      return;

   if( text && !text->string.multi_byte && text->length > 0 )
   {
      if( text->length > xwc->preeditLength - first )
         return;

      for( index = 0; index < text->length; ++index )
         xwc->preedit[first + index].feedback = IIF( text->feedback, text->feedback[index], 0 );
   }
   else
   {
      X_GTXWC_PREEDIT_CELL *cells = NULL;
      mbstate_t             state;
      const char           *bytes = IIF( text && !text->encoding_is_wchar, text->string.multi_byte, NULL );

      added = IIF( text, text->length, 0 );
      length = xwc->preeditLength - removed + added;
      if( length > X_GTXWC_PREEDIT_MAX )
         return;

      if( length )
         cells = ( X_GTXWC_PREEDIT_CELL * )x_xgrabzArray( length, sizeof( X_GTXWC_PREEDIT_CELL ) );
      if( first )
         memcpy( cells, xwc->preedit, first * sizeof( X_GTXWC_PREEDIT_CELL ) );
      if( xwc->preeditLength > first + removed )
      {
         int tail = xwc->preeditLength - first - removed;

         memcpy( cells + first + added, xwc->preedit + first + removed, tail * sizeof( X_GTXWC_PREEDIT_CELL ) );
      }
      memset( &state, 0, sizeof( state ) );
      for( index = 0; index < added; ++index )
      {
         uint32_t code;

         if( text->encoding_is_wchar )
            code = ( uint32_t )text->string.wide_char[index];
         else
         {
            wchar_t character;
            size_t  size = mbrtowc( &character, bytes, MB_CUR_MAX, &state );

            if( size == ( size_t )-1 || size == ( size_t )-2 )
            {
               code = 0xFFFD;
               size = 1;
               memset( &state, 0, sizeof( state ) );
            }
            else
            {
               code = ( uint32_t )character;
               if( size == 0 )
                  size = 1;
            }
            bytes += size;
         }
         cells[first + index].code = s_gt_xwc_PreeditCode( code );
         cells[first + index].feedback = IIF( text->feedback, text->feedback[index], 0 );
      }
      x_xfree( xwc->preedit );
      xwc->preedit = cells;
      xwc->preeditLength = length;
   }
   xwc->preeditCaret = X_MIN( X_MAX( draw->caret, 0 ), xwc->preeditLength );
   xwc->preeditVisible = xwc->preeditLength > 0;
   xwc->dirty = 1;
}

static void s_gt_xwc_PreeditCaret( XIC context, XPointer client, XPointer data )
{
   X_GTXWC                       *xwc = ( X_GTXWC * )client;
   XIMPreeditCaretCallbackStruct *caret = ( XIMPreeditCaretCallbackStruct * )data;
   int                            position;

   if( context != xwc->inputContext || !xwc->preeditActive || !caret )
      return;

   position = xwc->preeditCaret;
   switch( caret->direction )
   {
      case XIMForwardChar: ++position; break;
      case XIMBackwardChar: --position; break;
      case XIMForwardWord:
         while( position < xwc->preeditLength && !iswspace( xwc->preedit[position].code ) )
            ++position;
         while( position < xwc->preeditLength && iswspace( xwc->preedit[position].code ) )
            ++position;
         break;
      case XIMBackwardWord:
         while( position > 0 && iswspace( xwc->preedit[position - 1].code ) )
            --position;
         while( position > 0 && !iswspace( xwc->preedit[position - 1].code ) )
            --position;
         break;
      case XIMCaretUp:
      case XIMPreviousLine: position -= xwc->cols; break;
      case XIMCaretDown:
      case XIMNextLine: position += xwc->cols; break;
      case XIMLineStart: position -= ( xwc->cursorCol + position ) % xwc->cols; break;
      case XIMLineEnd: position += xwc->cols - 1 - ( xwc->cursorCol + position ) % xwc->cols; break;
      case XIMAbsolutePosition: position = caret->position; break;
      case XIMDontChange: break;
   }
   caret->position = xwc->preeditCaret = X_MIN( X_MAX( position, 0 ), xwc->preeditLength );
   xwc->preeditCaretStyle = caret->style;
   xwc->dirty = 1;
}

static XIC s_gt_xwc_PreeditCreateIC( X_GTXWC *xwc, XIM method, XIMStyle style )
{
   XIMCallback   start = { ( XPointer )xwc, ( XIMProc )s_gt_xwc_PreeditStart };
   XIMCallback   done = { ( XPointer )xwc, ( XIMProc )s_gt_xwc_PreeditDone };
   XIMCallback   draw = { ( XPointer )xwc, ( XIMProc )s_gt_xwc_PreeditDraw };
   XIMCallback   caret = { ( XPointer )xwc, ( XIMProc )s_gt_xwc_PreeditCaret };
   XVaNestedList attributes;
   XIC           context;

   attributes = XVaCreateNestedList( 0, XNPreeditStartCallback, &start, XNPreeditDoneCallback, &done,
                                     XNPreeditDrawCallback, &draw, XNPreeditCaretCallback, &caret, NULL );
   if( !attributes )
      return NULL;

   context = XCreateIC( method, XNInputStyle, style, XNClientWindow, xwc->window, XNFocusWindow, xwc->window,
                        XNPreeditAttributes, attributes, NULL );
   XFree( attributes );

   return context;
}

static void s_gt_xwc_InputAvailable( Display *display, XPointer context, XPointer data )
{
   X_GTXWC *xwc = ( X_GTXWC * )context;

   UNUSED( display );
   UNUSED( data );
   if( !xwc->inputClosing )
      xwc->inputRetry = 1;
}

static void s_gt_xwc_InputDestroyed( XIM method, XPointer context, XPointer data )
{
   X_GTXWC *xwc = ( X_GTXWC * )context;

   UNUSED( data );
   if( xwc->pendingInputMethod == method )
      xwc->pendingInputMethod = NULL;
   if( xwc->closingInputMethod == method )
      xwc->closingInputMethod = NULL;
   if( xwc->inputMethod == method )
   {
      s_gt_xwc_PreeditClear( xwc );
      xwc->inputMethod = NULL;
      xwc->inputContext = NULL;
      xwc->inputFallback = 0;
      if( !xwc->inputClosing )
         xwc->inputRetry = 1;
   }
}

static void s_gt_xwc_CloseInputMethod( X_GTXWC *xwc )
{
   XIC context = xwc->inputContext;
   XIM method = xwc->inputMethod;

   xwc->inputContext = NULL;
   xwc->inputMethod = NULL;
   s_gt_xwc_PreeditClear( xwc );
   xwc->closingInputMethod = method;
   if( context )
      XDestroyIC( context );
   if( xwc->closingInputMethod )
   {
      xwc->closingInputMethod = NULL;
      XCloseIM( method );
   }
}

static int s_gt_xwc_OpenInputMethod( X_GTXWC *xwc, int fallback )
{
   static const XIMStyle supported[] = { XIMPreeditCallbacks | XIMStatusNothing, XIMPreeditCallbacks | XIMStatusNone,
                                         XIMPreeditNothing | XIMStatusNothing,   XIMPreeditNothing | XIMStatusNone,
                                         XIMPreeditNone | XIMStatusNothing,      XIMPreeditNone | XIMStatusNone };
   XIM                   method;
   XIC                   context = NULL;
   XIMStyles            *styles = NULL;
   XIMCallback           destroyed;
   unsigned long         filterEvents = 0;
   size_t                choice;

   if( !XSetLocaleModifiers( IIF( fallback, "@im=none", "" ) ) )
      return 0;

   method = XOpenIM( xwc->display, NULL, NULL, NULL );
   if( !method )
      return 0;

   xwc->pendingInputMethod = method;
   destroyed.client_data = ( XPointer )xwc;
   destroyed.callback = s_gt_xwc_InputDestroyed;
   if( XSetIMValues( method, XNDestroyCallback, &destroyed, NULL ) != NULL )
      goto failed;
   if( !xwc->pendingInputMethod )
      goto failed;
   if( XGetIMValues( method, XNQueryInputStyle, &styles, NULL ) != NULL )
      goto failed;
   if( !xwc->pendingInputMethod || !styles )
      goto failed;
   for( choice = 0; choice < sizeof( supported ) / sizeof( supported[0] ) && !context; ++choice )
   {
      unsigned short index;
      XIMStyle       style = supported[choice];
      Window         window = xwc->window;

      for( index = 0; index < styles->count_styles; ++index )
      {
         if( styles->supported_styles[index] == style )
         {
            if( style & XIMPreeditCallbacks )
               context = s_gt_xwc_PreeditCreateIC( xwc, method, style );
            else
               context = XCreateIC( method, XNInputStyle, style, XNClientWindow, window, XNFocusWindow, window, NULL );
            break;
         }
      }
      if( !xwc->pendingInputMethod )
         goto failed;
   }
   if( !context )
      goto failed;
   if( XGetICValues( context, XNFilterEvents, &filterEvents, NULL ) != NULL || !xwc->pendingInputMethod )
      goto failed;
   XFree( styles );
   s_gt_xwc_CloseInputMethod( xwc );
   if( !xwc->pendingInputMethod )
      return 0;

   xwc->inputMethod = method;
   xwc->inputContext = context;
   xwc->pendingInputMethod = NULL;
   xwc->inputFallback = fallback;
   XSelectInput( xwc->display, xwc->window, xwc->eventMask | ( long )filterEvents );
   if( xwc->focused )
      XSetICFocus( context );
   return xwc->inputContext != NULL;

failed:
   if( styles )
      XFree( styles );
   if( xwc->pendingInputMethod )
   {
      if( context )
         XDestroyIC( context );
      if( xwc->pendingInputMethod )
      {
         xwc->pendingInputMethod = NULL;
         XCloseIM( method );
      }
   }

   return 0;
}

static void s_gt_xwc_ReconnectInput( X_GTXWC *xwc )
{
   if( !xwc->inputRetry || xwc->inputOpening || xwc->inputClosing )
      return;

   xwc->inputRetry = 0;
   if( xwc->inputMethod && !xwc->inputFallback )
      return;

   xwc->inputOpening = 1;
   if( !s_gt_xwc_OpenInputMethod( xwc, 0 ) && !xwc->inputMethod )
      s_gt_xwc_OpenInputMethod( xwc, 1 );
   XSetLocaleModifiers( "" );
   xwc->inputOpening = 0;
   if( !xwc->inputMethod )
      XSelectInput( xwc->display, xwc->window, xwc->eventMask );
}

static void s_gt_xwc_InitInput( X_GTXWC *xwc )
{
   X_GTXWC_INPUT_TRAP trap;
   XPointer           client = ( XPointer )xwc;
   XIDProc            available = s_gt_xwc_InputAvailable;

   s_gt_xwc_BeginInputTrap( xwc, &trap );
   setlocale( LC_CTYPE, "" );
   XSetLocaleModifiers( "" );
   xwc->inputRegistered = XRegisterIMInstantiateCallback( xwc->display, NULL, NULL, NULL, available, client );
   xwc->inputRetry = 1;
   s_gt_xwc_ReconnectInput( xwc );
   s_gt_xwc_EndInputTrap( &trap );
}

static void s_gt_xwc_ExitInput( X_GTXWC *xwc )
{
   X_GTXWC_INPUT_TRAP trap;

   if( !xwc->display )
      return;

   s_gt_xwc_BeginInputTrap( xwc, &trap );
   xwc->inputClosing = 1;
   if( xwc->inputRegistered )
   {
      XSetLocaleModifiers( "" );
      XUnregisterIMInstantiateCallback( xwc->display, NULL, NULL, NULL, s_gt_xwc_InputAvailable, ( XPointer )xwc );
      xwc->inputRegistered = 0;
   }
   s_gt_xwc_CloseInputMethod( xwc );
   s_gt_xwc_EndInputTrap( &trap );
}

static void s_gt_xwc_PumpEvents( X_GTXWC *xwc );

static void s_gt_xwc_ClearSelection( X_GTXWC *xwc )
{
   if( xwc->selectionActive )
   {
      xwc->selectionActive = 0;
      xwc->dirty = 1;
   }
   xwc->selecting = 0;
}

static void s_gt_xwc_CopySelection( X_GTXWC *xwc, int primary )
{
   int    first = X_MIN( xwc->selectionAnchor, xwc->selectionEnd );
   int    last = X_MAX( xwc->selectionAnchor, xwc->selectionEnd );
   int    firstRow = first / xwc->cols;
   int    lastRow = last / xwc->cols;
   int    row;
   size_t length = 0;
   char  *text;
   int    ctrl = xwc->fontAttribute & X_GTI_FONTA_CTRLCHARS;

   if( !xwc->selectionActive )
      return;

   text = ( char * )x_xgrab( ( size_t )( last - first + 1 ) * 3 + xwc->rows + 1 );
   for( row = firstRow; row <= lastRow; ++row )
   {
      int      left = IIF( row == firstRow, first % xwc->cols, 0 );
      int      right = IIF( row == lastRow, last % xwc->cols, xwc->cols - 1 );
      int      col, color;
      uint8_t  attr;
      uint16_t character;

      /* trailing spaces of a row are not copied */
      while( right >= left )
      {
         if( !X_GTSELF_GETSCRCHAR( xwc->gt, row, right, &color, &attr, &character ) || character != ' ' )
            break;
         --right;
      }
      for( col = left; col <= right; ++col )
      {
         if( X_GTSELF_GETSCRCHAR( xwc->gt, row, col, &color, &attr, &character ) )
         {
            uint16_t code = IIF( ctrl, x_u16Ctrl( character ), character );

            length += x_utf8Encode( text + length, code );
         }
      }
      if( row != lastRow )
         text[length++] = '\n';
   }
   text[length] = '\0';
   s_gt_xwc_SetClipboard( xwc, primary, text, length );
   x_xfree( text );
}

static void s_gt_xwc_KeyPress( X_GTXWC *xwc, XKeyEvent *event )
{
   char   stackBuffer[64];
   char  *buffer = stackBuffer;
   int    capacity = sizeof( stackBuffer );
   KeySym symbol = NoSymbol;
   Status status = XLookupNone;
   int    length = 0;
   int    key, keypad;
   int    flags = s_gt_xwc_KeyFlags( xwc, event->state );
   int    mods = flags & ( KF_CTRL | KF_SHIFT | KF_ALT );

   XIC inputContext = xwc->inputContext;

   xwc->keyFlags = flags;
   xwc->cursorTime = s_gt_xwc_Time();
   if( !xwc->cursorVisible )
   {
      xwc->cursorVisible = 1;
      xwc->dirty = 1;
   }

   if( inputContext )
   {
      for( ;; )
      {
         length = Xutf8LookupString( inputContext, event, buffer, capacity, &symbol, &status );
         if( xwc->inputContext != inputContext )
         {
            status = XLookupNone;
            break;
         }
         if( status != XBufferOverflow )
            break;
         if( length <= capacity || length > 16 * 1024 * 1024 )
         {
            status = XLookupNone;
            break;
         }
         if( buffer != stackBuffer )
            x_xfree( buffer );
         capacity = length;
         buffer = ( char * )x_xgrab( capacity );
      }
      if( status != XLookupKeySym && status != XLookupBoth )
         symbol = NoSymbol;
      if( status != XLookupChars && status != XLookupBoth )
         length = 0;
      if( length > 0 && length <= capacity && xwc->preeditVisible )
      {
         xwc->preeditVisible = 0;
         xwc->dirty = 1;
      }
   }
   else
      length = XLookupString( event, buffer, capacity, &symbol, NULL );

   if( xwc->altEnter && ( symbol == XK_Return || symbol == XK_KP_Enter ) && mods == KF_ALT )
   {
      int fullscreen = s_gt_xwc_WindowState( xwc, xwc->netWmFullscreen, None );

      s_gt_xwc_SetWindowState( xwc, xwc->netWmFullscreen, None, !fullscreen );
   }
   else if( xwc->selectCopy && mods == ( KF_CTRL | KF_SHIFT ) && ( symbol == XK_c || symbol == XK_C ) )
      s_gt_xwc_CopySelection( xwc, 0 );
   else if( xwc->selectCopy && mods == ( KF_CTRL | KF_SHIFT ) && ( symbol == XK_v || symbol == XK_V ) )
   {
      s_gt_xwc_ClearSelection( xwc );
      s_gt_xwc_PasteClipboard( xwc, 0, s_gt_xwc_PumpEvents );
   }
   else
   {
      key = s_gt_xwc_KeyCode( symbol );
      keypad = s_gt_xwc_KeypadChar( symbol );
      if( symbol >= XK_KP_Space && symbol <= XK_KP_Equal )
         flags |= KF_KEYPAD;
      if( symbol == XK_ISO_Left_Tab )
         flags |= KF_SHIFT;
      if( symbol == XK_Shift_L || symbol == XK_Shift_R )
         xwc->keyFlags |= KF_SHIFT;
      else if( symbol == XK_Control_L || symbol == XK_Control_R )
         xwc->keyFlags |= KF_CTRL;
      else if( symbol == XK_Alt_L || symbol == XK_Alt_R || symbol == XK_Meta_L || symbol == XK_Meta_R )
         xwc->keyFlags |= KF_ALT;
      if( key )
         s_gt_xwc_AddKey( xwc, X_INKEY_NEW_KEY( key, flags ) );
      else if( symbol >= XK_KP_0 && symbol <= XK_KP_9 )
         s_gt_xwc_AddKey( xwc, X_INKEY_NEW_KEY( '0' + ( int )( symbol - XK_KP_0 ), flags ) );
      else if( keypad )
         s_gt_xwc_AddKey( xwc, X_INKEY_NEW_KEY( keypad, flags ) );
      else if( ( flags & ( KF_CTRL | KF_ALT ) ) && symbol >= 32 && symbol <= 126 )
         s_gt_xwc_AddKey( xwc, X_INKEY_NEW_UNICODEF( ( int )symbol, flags ) );
      else if( !inputContext && ( symbol & 0xFF000000 ) == 0x01000000 )
         s_gt_xwc_AddKey( xwc, X_INKEY_NEW_UNICODEF( ( int )( symbol & 0xFFFFFF ), flags ) );
      else if( length > 0 && length <= capacity )
      {
         size_t   offset = 0;
         uint32_t codepoint;

         while( offset < ( size_t )length )
         {
            if( inputContext )
            {
               if( !x_utf8Decode( buffer, ( size_t )length, &offset, &codepoint ) )
                  break;
            }
            else
               codepoint = ( unsigned char )buffer[offset++];
            s_gt_xwc_AddKey( xwc, X_INKEY_NEW_UNICODEF( ( int )codepoint, flags ) );
         }
      }
   }
   if( buffer != stackBuffer )
      x_xfree( buffer );
}

static void s_gt_xwc_UpdateMousePosition( X_GTXWC *xwc, int x, int y, int addEvent )
{
   int row = X_MIN( X_MAX( y / xwc->cellHeight, 0 ), xwc->rows - 1 );
   int col = X_MIN( X_MAX( x / xwc->cellWidth, 0 ), xwc->cols - 1 );

   xwc->mouseX = x;
   xwc->mouseY = y;
   xwc->gt->mouseRow = row;
   xwc->gt->mouseCol = col;
   if( row != xwc->mouseRow || col != xwc->mouseCol )
   {
      xwc->mouseRow = row;
      xwc->mouseCol = col;
      if( addEvent )
         s_gt_xwc_AddKey( xwc, X_INKEY_NEW_MPOS( col, row ) );
   }
}

static void s_gt_xwc_WarpPointer( X_GTXWC *xwc, int x, int y )
{
   Window       root, child;
   int          rootX, rootY;
   unsigned int mask;

   XWarpPointer( xwc->display, None, xwc->window, 0, 0, 0, 0, x, y );
   if( XQueryPointer( xwc->display, xwc->window, &root, &child, &rootX, &rootY, &x, &y, &mask ) )
      s_gt_xwc_UpdateMousePosition( xwc, x, y, 0 );
}

static void s_gt_xwc_DiscardMouseKeys( X_GTXWC *xwc )
{
   X_GT_BASE *gt = xwc->gt;
   int        read = xwc->keyOut;
   int        write = read;
   size_t     index, count = 0;

   while( read != xwc->keyIn )
   {
      int key = xwc->keys[read];

      if( !X_INKEY_ISMOUSEKEY( key ) && !X_INKEY_ISMOUSEPOS( key ) )
      {
         xwc->keys[write] = key;
         write = ( write + 1 ) % xwc->keyCapacity;
      }
      read = ( read + 1 ) % xwc->keyCapacity;
   }
   xwc->keyIn = write;
   for( index = 0; index < gt->inkeyCount; ++index )
   {
      int key = gt->inkeyBuffer[( gt->inkeyHead + index ) % X_DEFAULT_INKEY_BUFSIZE];

      if( !X_INKEY_ISMOUSEKEY( key ) && !X_INKEY_ISMOUSEPOS( key ) )
         gt->inkeyBuffer[( gt->inkeyHead + count++ ) % X_DEFAULT_INKEY_BUFSIZE] = key;
   }
   gt->inkeyCount = count;
}

static void s_gt_xwc_ResetMouse( X_GTXWC *xwc )
{
   Window       root, child;
   int          rootX, rootY, x, y;
   unsigned int mask;

   xwc->mouseButtons = xwc->consumedButtons = 0;
   memset( xwc->clickPending, 0, sizeof( xwc->clickPending ) );
   if( xwc->selecting )
      s_gt_xwc_ClearSelection( xwc );
   s_gt_xwc_DiscardMouseKeys( xwc );
   if( XQueryPointer( xwc->display, xwc->window, &root, &child, &rootX, &rootY, &x, &y, &mask ) )
   {
      s_gt_xwc_UpdateMousePosition( xwc, x, y, 0 );
      if( xwc->mouseOn )
      {
         xwc->mouseButtons = 0;
         if( mask & Button1Mask )
            xwc->mouseButtons |= 1U << X_MBUTTON_LEFT;
         if( mask & Button2Mask )
            xwc->mouseButtons |= 1U << X_MBUTTON_MIDDLE;
         if( mask & Button3Mask )
            xwc->mouseButtons |= 1U << X_MBUTTON_RIGHT;
         xwc->consumedButtons = xwc->mouseButtons;
      }
   }
   xwc->gt->mouseButtons = xwc->mouseButtons;
}

static void s_gt_xwc_MouseButton( X_GTXWC *xwc, XButtonEvent *event, int pressed )
{
   int key = 0;
   int button = -1;
   int consumed = 0;
   int moveEvent = !xwc->selecting && !( xwc->selectCopy && event->button <= Button2 );

   s_gt_xwc_UpdateMousePosition( xwc, event->x, event->y, moveEvent );
   if( event->button == Button1 )
   {
      button = X_MBUTTON_LEFT;
      key = IIF( pressed, K_LBUTTONDOWN, K_LBUTTONUP );
   }
   else if( event->button == Button2 )
   {
      button = X_MBUTTON_MIDDLE;
      key = IIF( pressed, K_MBUTTONDOWN, K_MBUTTONUP );
   }
   else if( event->button == Button3 )
   {
      button = X_MBUTTON_RIGHT;
      key = IIF( pressed, K_RBUTTONDOWN, K_RBUTTONUP );
   }
   else if( pressed && event->button == Button4 )
      key = K_MWFORWARD;
   else if( pressed && event->button == Button5 )
      key = K_MWBACKWARD;
   else if( pressed && event->button == 6 )
      key = K_MWLEFT;
   else if( pressed && event->button == 7 )
      key = K_MWRIGHT;

   if( button >= 0 )
   {
      unsigned int bit = 1U << button;

      if( pressed )
      {
         int doubleClickTime = X_GTSELF_MOUSEGETDOUBLECLICKSPEED( xwc->gt );
         int doubleClick = doubleClickTime > 0 && xwc->clickPending[button];

         xwc->mouseButtons |= bit;
         if( doubleClick && ( uint32_t )( event->time - xwc->clickTime[button] ) > ( unsigned int )doubleClickTime )
            doubleClick = 0;
         if( doubleClick && llabs( ( int64_t )event->x - xwc->clickX[button] ) > xwc->doubleClickDistance )
            doubleClick = 0;
         if( doubleClick && llabs( ( int64_t )event->y - xwc->clickY[button] ) > xwc->doubleClickDistance )
            doubleClick = 0;
         if( doubleClick )
         {
            key = K_MDBLCLK;
            if( button == X_MBUTTON_LEFT )
               key = K_LDBLCLK;
            else if( button == X_MBUTTON_RIGHT )
               key = K_RDBLCLK;
            xwc->clickPending[button] = 0;
         }
         else
         {
            xwc->clickTime[button] = event->time;
            xwc->clickX[button] = event->x;
            xwc->clickY[button] = event->y;
            xwc->clickPending[button] = doubleClickTime > 0;
         }
         if( xwc->selectCopy && event->button <= Button2 )
         {
            consumed = 1;
            xwc->consumedButtons |= bit;
            if( event->button == Button1 )
            {
               xwc->selectionAnchor = xwc->selectionEnd = xwc->mouseRow * xwc->cols + xwc->mouseCol;
               xwc->selecting = xwc->selectionActive = 1;
               xwc->dirty = 1;
            }
            else
            {
               s_gt_xwc_ClearSelection( xwc );
               s_gt_xwc_PasteClipboard( xwc, 1, s_gt_xwc_PumpEvents );
            }
         }
      }
      else
      {
         xwc->mouseButtons &= ~bit;
         consumed = ( xwc->consumedButtons & bit ) != 0;
         xwc->consumedButtons &= ~bit;
         if( consumed && event->button == Button1 && xwc->selecting )
         {
            xwc->selectionEnd = xwc->mouseRow * xwc->cols + xwc->mouseCol;
            xwc->selecting = 0;
            xwc->dirty = 1;
            s_gt_xwc_CopySelection( xwc, 1 );
         }
      }
   }
   xwc->gt->mouseButtons = xwc->mouseButtons;
   if( key && !consumed )
      s_gt_xwc_AddKey( xwc, X_INKEY_NEW_MKEY( key, s_gt_xwc_KeyFlags( xwc, event->state ) ) );
}

static void s_gt_xwc_InitWindowAtoms( X_GTXWC *xwc )
{
   char *names[] = { "WM_DELETE_WINDOW",
                     "WM_PROTOCOLS",
                     "_NET_WM_STATE",
                     "_NET_WM_STATE_FULLSCREEN",
                     "_NET_WM_STATE_MAXIMIZED_VERT",
                     "_NET_WM_STATE_MAXIMIZED_HORZ",
                     "_NET_WM_STATE_ABOVE",
                     "_NET_WM_WINDOW_OPACITY",
                     "_NET_WM_NAME",
                     "UTF8_STRING",
                     "_MOTIF_WM_HINTS",
                     "_NET_WM_ICON",
                     "WM_STATE",
                     "_NET_FRAME_EXTENTS" };
   Atom  atoms[14];

   XInternAtoms( xwc->display, names, 14, False, atoms );
   xwc->wmDelete = atoms[0];
   xwc->wmProtocols = atoms[1];
   xwc->netWmState = atoms[2];
   xwc->netWmFullscreen = atoms[3];
   xwc->netWmMaximizedVert = atoms[4];
   xwc->netWmMaximizedHorz = atoms[5];
   xwc->netWmAbove = atoms[6];
   xwc->netWmOpacity = atoms[7];
   xwc->netWmName = atoms[8];
   xwc->utf8String = atoms[9];
   xwc->motifWmHints = atoms[10];
   xwc->netWmIcon = atoms[11];
   xwc->wmState = atoms[12];
   xwc->netFrameExtents = atoms[13];
}

static void s_gt_xwc_SetWindowTitle( X_GTXWC *xwc )
{
   XTextProperty        property;
   char                *title = xwc->title;
   const unsigned char *bytes = ( const unsigned char * )title;
   int                  length = ( int )strlen( title );

   if( Xutf8TextListToTextProperty( xwc->display, &title, 1, XStdICCTextStyle, &property ) >= Success )
   {
      XSetWMName( xwc->display, xwc->window, &property );
      XFree( property.value );
   }
   XChangeProperty( xwc->display, xwc->window, xwc->netWmName, xwc->utf8String, 8, PropModeReplace, bytes, length );
   XFlush( xwc->display );
}

static int s_gt_xwc_WindowState( X_GTXWC *xwc, Atom first, Atom second )
{
   X_GTXWC_PROPERTY reply;
   unsigned long    index;
   int              firstFound = 0;
   int              secondFound = second == None;
   int              ok;

   ok = s_gt_xwc_GetProperty( xwc, xwc->window, xwc->netWmState, 1024, False, XA_ATOM, &reply );
   if( ok && reply.type == XA_ATOM && reply.format == 32 && reply.remaining == 0 )
   {
      const Atom *states = ( const Atom * )reply.data;

      for( index = 0; index < reply.count; ++index )
      {
         if( states[index] == first )
            firstFound = 1;
         if( states[index] == second )
            secondFound = 1;
      }
   }
   if( reply.data )
      XFree( reply.data );

   return firstFound && secondFound;
}

static void s_gt_xwc_SetWindowState( X_GTXWC *xwc, Atom first, Atom second, int enabled )
{
   XEvent event;
   Window root = RootWindow( xwc->display, xwc->screen );
   long   mask = SubstructureRedirectMask | SubstructureNotifyMask;

   memset( &event, 0, sizeof( event ) );
   event.xclient.type = ClientMessage;
   event.xclient.display = xwc->display;
   event.xclient.window = xwc->window;
   event.xclient.message_type = xwc->netWmState;
   event.xclient.format = 32;
   event.xclient.data.l[0] = IIF( enabled, 1, 0 );
   event.xclient.data.l[1] = ( long )first;
   event.xclient.data.l[2] = ( long )second;
   event.xclient.data.l[3] = 1;
   XSendEvent( xwc->display, root, False, mask, &event );
   XFlush( xwc->display );
}

static int s_gt_xwc_WindowMinimized( X_GTXWC *xwc )
{
   X_GTXWC_PROPERTY reply;
   int              minimized = 0;
   int              ok;

   ok = s_gt_xwc_GetProperty( xwc, xwc->window, xwc->wmState, 2, False, xwc->wmState, &reply );
   if( ok && reply.type == xwc->wmState && reply.format == 32 && reply.count == 2 && reply.remaining == 0 )
      minimized = ( ( unsigned long * )reply.data )[0] == IconicState;
   if( reply.data )
      XFree( reply.data );

   return minimized;
}

static int s_gt_xwc_FrameExtents( X_GTXWC *xwc, unsigned long extents[4] )
{
   X_GTXWC_PROPERTY reply;
   int              valid = 0;
   int              ok;

   ok = s_gt_xwc_GetProperty( xwc, xwc->window, xwc->netFrameExtents, 4, False, XA_CARDINAL, &reply );
   if( ok && reply.type == XA_CARDINAL && reply.format == 32 && reply.count == 4 && reply.remaining == 0 )
   {
      const unsigned long *values = ( const unsigned long * )reply.data;

      if( values[0] <= UINT16_MAX && values[1] <= UINT16_MAX && values[2] <= UINT16_MAX && values[3] <= UINT16_MAX )
      {
         memcpy( extents, values, 4 * sizeof( *extents ) );
         valid = 1;
      }
   }
   if( reply.data )
      XFree( reply.data );

   return valid;
}

static void s_gt_xwc_WindowPosition( X_GTXWC *xwc, int *x, int *y )
{
   Window        child;
   Window        root = RootWindow( xwc->display, xwc->screen );
   unsigned long extents[4];
   int           translated;

   translated = XTranslateCoordinates( xwc->display, xwc->window, root, 0, 0, x, y, &child );
   if( translated && s_gt_xwc_FrameExtents( xwc, extents ) )
   {
      *x -= ( int )extents[0];
      *y -= ( int )extents[2];
   }
   xwc->windowX = *x;
   xwc->windowY = *y;
}

static double s_gt_xwc_WindowOpacity( X_GTXWC *xwc )
{
   X_GTXWC_PROPERTY reply;
   double           opacity = 1.0;
   int              ok;

   ok = s_gt_xwc_GetProperty( xwc, xwc->window, xwc->netWmOpacity, 1, False, XA_CARDINAL, &reply );
   if( ok && reply.type == XA_CARDINAL && reply.format == 32 && reply.count == 1 && reply.remaining == 0 )
      opacity = ( double )( ( ( const unsigned long * )reply.data )[0] & UINT32_MAX ) / ( double )UINT32_MAX;
   if( reply.data )
      XFree( reply.data );

   return opacity;
}

static void s_gt_xwc_SetWindowOpacity( X_GTXWC *xwc, double opacity )
{
   unsigned long        value;
   const unsigned char *bytes = ( const unsigned char * )&value;

   if( !isfinite( opacity ) || opacity < 0.0 || opacity > 1.0 )
      return;

   if( opacity == 1.0 )
      XDeleteProperty( xwc->display, xwc->window, xwc->netWmOpacity );
   else
   {
      value = ( unsigned long )( opacity * ( double )UINT32_MAX + 0.5 );
      XChangeProperty( xwc->display, xwc->window, xwc->netWmOpacity, XA_CARDINAL, 32, PropModeReplace, bytes, 1 );
   }
   XFlush( xwc->display );
}

static void s_gt_xwc_GetWindowHints( X_GTXWC *xwc, unsigned long hints[5] )
{
   X_GTXWC_PROPERTY reply;
   int              valid;

   memset( hints, 0, 5 * sizeof( unsigned long ) );
   hints[1] = hints[2] = 1;
   valid = s_gt_xwc_GetProperty( xwc, xwc->window, xwc->motifWmHints, 5, False, xwc->motifWmHints, &reply );
   if( valid && ( reply.type != xwc->motifWmHints || reply.format != 32 || reply.remaining != 0 ) )
      valid = 0;
   if( valid && reply.count >= 3 && reply.count <= 5 )
      memcpy( hints, reply.data, reply.count * sizeof( unsigned long ) );
   if( reply.data )
      XFree( reply.data );
}

static void s_gt_xwc_SetWindowHints( X_GTXWC *xwc, int bordered )
{
   unsigned long        hints[5];
   unsigned long        functions = 0x3E;
   const unsigned char *bytes = ( const unsigned char * )hints;

   s_gt_xwc_GetWindowHints( xwc, hints );
   if( xwc->closeMode == 2 )
      functions &= ~0x20UL;
   if( !xwc->resizable )
      functions &= ~0x12UL;
   hints[0] |= 1;
   hints[1] = IIF( functions == 0x3E, 1, functions );
   if( bordered >= 0 )
   {
      hints[0] |= 2;
      hints[2] = bordered != 0;
   }
   XChangeProperty( xwc->display, xwc->window, xwc->motifWmHints, xwc->motifWmHints, 32, PropModeReplace, bytes, 5 );
   XFlush( xwc->display );
}

static void s_gt_xwc_UpdateSizeLimits( X_GTXWC *xwc )
{
   XSizeHints hints;
   int        fontMode = xwc->resizeMode == X_GTI_RESIZEMODE_FONT;
   int        maxWidth = IIF( fontMode, xwc->cols * 256, X_GTXWC_MAX_COLS * xwc->cellWidth );
   int        maxHeight = IIF( fontMode, xwc->rows * 256, X_GTXWC_MAX_ROWS * xwc->cellHeight );

   if( X_GTSELF_DISPCOUNT( xwc->gt ) != 0 )
   {
      xwc->sizeHintsPending = 1;
      return;
   }
   xwc->sizeHintsPending = 0;
   memset( &hints, 0, sizeof( hints ) );
   hints.flags = PMinSize | PMaxSize | PWinGravity;
   hints.win_gravity = NorthWestGravity;
   hints.min_width = IIF( fontMode, xwc->cols, xwc->cellWidth );
   hints.min_height = IIF( fontMode, xwc->rows, xwc->cellHeight );
   hints.max_width = X_MIN( UINT16_MAX, maxWidth );
   hints.max_height = X_MIN( UINT16_MAX, maxHeight );
   if( xwc->resizeStep )
   {
      hints.flags |= PResizeInc | PBaseSize;
      hints.width_inc = IIF( fontMode, xwc->cols, xwc->cellWidth );
      hints.height_inc = IIF( fontMode, xwc->rows, xwc->cellHeight );
   }
   if( !xwc->resizable )
   {
      hints.min_width = hints.max_width = xwc->canvasWidth;
      hints.min_height = hints.max_height = xwc->canvasHeight;
   }
   XSetWMNormalHints( xwc->display, xwc->window, &hints );
   XFlush( xwc->display );
}

static void s_gt_xwc_RequestSize( X_GTXWC *xwc )
{
   xwc->pendingWidth = xwc->pendingHeight = 0;
   if( X_GTSELF_DISPCOUNT( xwc->gt ) != 0 )
   {
      xwc->resizeRequested = 1;
      return;
   }
   xwc->resizeRequested = 0;
   if( xwc->sizeHintsPending )
      s_gt_xwc_UpdateSizeLimits( xwc );
   xwc->resizeSerial = NextRequest( xwc->display );
   XResizeWindow( xwc->display, xwc->window, ( unsigned int )xwc->canvasWidth, ( unsigned int )xwc->canvasHeight );
   XFlush( xwc->display );
}

static void s_gt_xwc_SetCellSize( X_GTXWC *xwc, int width, int height )
{
   if( width == xwc->cellWidth && height == xwc->cellHeight )
      return;

   xwc->cellWidth = width;
   xwc->cellHeight = height;
   s_gt_xwc_CreateCanvas( xwc, xwc->rows, xwc->cols );
   X_GTSELF_EXPOSEAREA( xwc->gt, 0, 0, xwc->rows - 1, xwc->cols - 1 );
   s_gt_xwc_UpdateSizeLimits( xwc );
   s_gt_xwc_AddKey( xwc, K_RESIZE );
}

static void s_gt_xwc_ResizeFromWindow( X_GTXWC *xwc, int width, int height )
{
   xwc->windowWidth = width;
   xwc->windowHeight = height;
   if( xwc->resizeMode == X_GTI_RESIZEMODE_FONT )
   {
      int cellWidth = X_MIN( X_MAX( width / xwc->cols, 1 ), 256 );
      int cellHeight = X_MIN( X_MAX( height / xwc->rows, 1 ), 256 );

      if( cellWidth != xwc->cellWidth || cellHeight != xwc->cellHeight )
      {
         s_gt_xwc_SetCellSize( xwc, cellWidth, cellHeight );
         xwc->resizeFramePending = xwc->deferResizePaint;
      }
   }
   else
   {
      int rows = X_MIN( X_MAX( height / xwc->cellHeight, X_GTXWC_MIN_ROWS ), X_GTXWC_MAX_ROWS );
      int cols = X_MIN( X_MAX( width / xwc->cellWidth, X_GTXWC_MIN_COLS ), X_GTXWC_MAX_COLS );

      if( rows != xwc->rows || cols != xwc->cols )
      {
         s_gt_xwc_CreateCanvas( xwc, rows, cols );
         X_GTSELF_RESIZE( xwc->gt, rows, cols );
         X_GTSELF_EXPOSEAREA( xwc->gt, 0, 0, rows - 1, cols - 1 );
         s_gt_xwc_UpdateSizeLimits( xwc );
         xwc->resizeFramePending = xwc->deferResizePaint;
         s_gt_xwc_AddKey( xwc, K_RESIZE );
      }
   }
}

static void s_gt_xwc_ResizeWindow( X_GTXWC *xwc, int64_t width, int64_t height )
{
   if( width <= 0 || width > UINT16_MAX || height <= 0 || height > UINT16_MAX )
      return;

   s_gt_xwc_ResizeFromWindow( xwc, ( int )width, ( int )height );
   s_gt_xwc_RequestSize( xwc );
   xwc->gt->pFuncTable->Flush( xwc->gt );
}

static void s_gt_xwc_PumpEvents( X_GTXWC *xwc )
{
   X_GTXWC_INPUT_TRAP  trap;
   int                 handling = xwc->inputRetry || XPending( xwc->display ) > 0;
   X_GTXWC_INPUT_TRAP *active __attribute__( ( cleanup( s_gt_xwc_InputTrapDrop ) ) ) = IIF( handling, &trap, NULL );

   if( handling )
      s_gt_xwc_BeginInputTrap( xwc, &trap );
   s_gt_xwc_ReconnectInput( xwc );
   while( handling && XPending( xwc->display ) > 0 )
   {
      XEvent event;

      XNextEvent( xwc->display, &event );
      if( event.type == MappingNotify )
      {
         XRefreshKeyboardMapping( &event.xmapping );
         s_gt_xwc_UpdateModifiers( xwc );
         continue;
      }
      if( s_gt_xwc_ClipboardEvent( xwc, &event ) )
         continue;
      if( xwc->dialog && s_gt_xwc_DialogEvent( xwc, &event ) )
         continue;
      if( XFilterEvent( &event, None ) || event.xany.window != xwc->window )
         continue;

      switch( event.type )
      {
         case Expose:
            if( event.xexpose.count == 0 )
               s_gt_xwc_RepaintFrame( xwc );
            break;
         case MapNotify:
            xwc->mapped = 1;
            xwc->dirty = 1;
            break;
         case UnmapNotify: xwc->mapped = 0; break;
         case FocusIn:
            xwc->focused = 1;
            if( xwc->preeditActive )
               xwc->dirty = 1;
            if( xwc->inputContext )
               XSetICFocus( xwc->inputContext );
            s_gt_xwc_AddKey( xwc, K_GOTFOCUS );
            break;
         case FocusOut:
            xwc->focused = 0;
            memset( xwc->clickPending, 0, sizeof( xwc->clickPending ) );
            if( xwc->preeditActive )
               xwc->dirty = 1;
            if( xwc->inputContext )
               XUnsetICFocus( xwc->inputContext );
            s_gt_xwc_AddKey( xwc, K_LOSTFOCUS );
            break;
         case KeyPress: s_gt_xwc_KeyPress( xwc, &event.xkey ); break;
         case KeyRelease:
         {
            XkbStateRec state;

            if( XkbGetState( xwc->display, XkbUseCoreKbd, &state ) == Success )
               xwc->keyFlags = s_gt_xwc_KeyFlags( xwc, state.mods );
            break;
         }
         case MotionNotify:
            if( !xwc->mouseOn )
            {
               s_gt_xwc_UpdateMousePosition( xwc, event.xmotion.x, event.xmotion.y, 0 );
               break;
            }
            {
               int button;

               /* a click that moved too far is not the first click of a double click */
               for( button = 0; button < 3; ++button )
               {
                  int64_t movedX = llabs( ( int64_t )event.xmotion.x - xwc->clickX[button] );
                  int64_t movedY = llabs( ( int64_t )event.xmotion.y - xwc->clickY[button] );
                  int     moved = movedX > xwc->doubleClickDistance || movedY > xwc->doubleClickDistance;

                  if( xwc->clickPending[button] && ( xwc->mouseButtons & ( 1U << button ) ) && moved )
                     xwc->clickPending[button] = 0;
               }
            }
            s_gt_xwc_UpdateMousePosition( xwc, event.xmotion.x, event.xmotion.y, !xwc->selecting );
            if( xwc->selecting )
            {
               xwc->selectionEnd = xwc->mouseRow * xwc->cols + xwc->mouseCol;
               xwc->dirty = 1;
            }
            break;
         case ButtonPress:
            if( xwc->mouseOn )
               s_gt_xwc_MouseButton( xwc, &event.xbutton, 1 );
            break;
         case ButtonRelease:
            if( xwc->mouseOn )
               s_gt_xwc_MouseButton( xwc, &event.xbutton, 0 );
            break;
         case ConfigureNotify:
            xwc->windowX = event.xconfigure.x;
            xwc->windowY = event.xconfigure.y;
            if( event.xconfigure.serial >= xwc->resizeSerial )
            {
               xwc->pendingWidth = event.xconfigure.width;
               xwc->pendingHeight = event.xconfigure.height;
            }
            break;
         case ClientMessage:
            if( event.xclient.message_type != xwc->wmProtocols || event.xclient.format != 32 )
               break;

            if( ( Atom )event.xclient.data.l[0] == xwc->wmDelete )
            {
               if( xwc->closeMode == 0 )
                  x_quitRaise();
               else if( xwc->closeMode == 1 )
                  s_gt_xwc_AddKey( xwc, K_CLOSE );
            }
            break;
         default: break;
      }
   }
   s_gt_xwc_ReconnectInput( xwc );
   if( xwc->pendingWidth > 0 && X_GTSELF_DISPCOUNT( xwc->gt ) == 0 )
   {
      s_gt_xwc_ResizeFromWindow( xwc, xwc->pendingWidth, xwc->pendingHeight );
      xwc->resizeNotifyPending = 1;
      xwc->pendingWidth = xwc->pendingHeight = 0;
      s_gt_xwc_UpdateMousePosition( xwc, xwc->mouseX, xwc->mouseY, 0 );
   }
   s_gt_xwc_ClipboardExpire( xwc );
}

/* ---------------------------------------------------------------------------------------------
   GT lifecycle and screen cells
   --------------------------------------------------------------------------------------------- */

static void s_gt_xwc_Init( X_GT_BASE *gt, int stdinHandle, int stdoutHandle, int stderrHandle )
{
   X_GTXWC                 *xwc = ( X_GTXWC * )x_xgrabz( sizeof( X_GTXWC ) );
   unsigned long            black;
   XSetWindowAttributes     attributes;
   X_GTXWC_DESKTOP_SETTINGS settings;
   Window                   root;
   unsigned int             width, height;

   xwc->gt = gt;
   xwc->keyCapacity = X_GTXWC_KEY_QUEUE_SIZE;
   xwc->keys = ( int * )x_xgrabArray( xwc->keyCapacity, sizeof( int ) );
   xwc->rows = X_GTXWC_DEFAULT_ROWS;
   xwc->cols = X_GTXWC_DEFAULT_COLS;
   xwc->cellWidth = X_GTXWC_FONT_WIDTH;
   xwc->cellHeight = X_GTXWC_FONT_HEIGHT;
   xwc->gfxMode = GFX_MODE_SOLID;
   xwc->title = x_strdup( "xDev GTXWC" );
   xwc->resizable = 1;
   xwc->resizeStep = 1;
   xwc->resizeMode = X_GTI_RESIZEMODE_ROWS;
   xwc->cursorBlinkRate = X_GT_CURSOR_BLINK;
   xwc->mouseOn = 1;
   xwc->doubleClickDistance = 5;
   xwc->cursorVisible = 1;
   xwc->cursorTime = s_gt_xwc_Time();
   xwc->fontAttribute = X_GTI_FONTA_FIXMETRIC | X_GTI_FONTA_CLRBKG | X_GTI_FONTA_CTRLCHARS;
   xwc->closeMode = 1;
   xwc->mouseRow = xwc->mouseCol = 0;
   memcpy( xwc->colors, s_defaultColors, sizeof( xwc->colors ) );

   gt->driverData[s_gtId] = xwc;
   s_super.Init( gt, stdinHandle, stdoutHandle, stderrHandle );
   X_GTSELF_SETFLAG( gt, X_GTI_COMPATBUFFER, 0 );

   xwc->display = XOpenDisplay( NULL );
   if( !xwc->display )
   {
      fputs( "xDev GTXWC: cannot open the X11 display (check DISPLAY)\n", stderr );
      x_quitRaise();
      return;
   }

   xwc->screen = DefaultScreen( xwc->display );
   if( s_gt_xwc_DesktopSettings( xwc, &settings ) )
   {
      if( settings.doubleClickTime >= 0 )
         gt->mouseDoubleClickSpeed = settings.doubleClickTime;
      if( settings.doubleClickDistance >= 0 )
         xwc->doubleClickDistance = settings.doubleClickDistance;
   }
   s_gt_xwc_UpdateModifiers( xwc );
   xwc->visual = DefaultVisual( xwc->display, xwc->screen );
   s_gt_xwc_InitColors( xwc );
   s_gt_xwc_LoadFont( xwc );
   s_gt_xwc_InitWindowAtoms( xwc );
   xwc->desktopWidth = DisplayWidth( xwc->display, xwc->screen );
   xwc->desktopHeight = DisplayHeight( xwc->display, xwc->screen );
   xwc->desktopDepth = DefaultDepth( xwc->display, xwc->screen );
   xwc->canvasWidth = xwc->cols * xwc->cellWidth;
   xwc->canvasHeight = xwc->rows * xwc->cellHeight;
   black = s_gt_xwc_Pixel( xwc, 0 );
   root = RootWindow( xwc->display, xwc->screen );
   width = ( unsigned int )xwc->canvasWidth;
   height = ( unsigned int )xwc->canvasHeight;
   xwc->window = XCreateSimpleWindow( xwc->display, root, 0, 0, width, height, 0, black, black );
   xwc->gc = XCreateGC( xwc->display, xwc->window, 0, NULL );
   XSetGraphicsExposures( xwc->display, xwc->gc, False );
   attributes.bit_gravity = NorthWestGravity;
   attributes.colormap = xwc->colormap;
   XChangeWindowAttributes( xwc->display, xwc->window, CWBitGravity | CWColormap, &attributes );
   XSetWMProtocols( xwc->display, xwc->window, &xwc->wmDelete, 1 );
   xwc->eventMask = ExposureMask | StructureNotifyMask | FocusChangeMask | PropertyChangeMask;
   xwc->eventMask |= KeyPressMask | KeyReleaseMask | ButtonPressMask | ButtonReleaseMask | PointerMotionMask;
   XSelectInput( xwc->display, xwc->window, xwc->eventMask );

   s_gt_xwc_ClipboardInit( xwc );
   s_gt_xwc_SetWindowTitle( xwc );
   s_gt_xwc_InitInput( xwc );

   s_gt_xwc_CreateCanvas( xwc, xwc->rows, xwc->cols );
   s_gt_xwc_UpdateSizeLimits( xwc );
   XMapWindow( xwc->display, xwc->window );
   X_GTSELF_RESIZE( gt, xwc->rows, xwc->cols );
   X_GTSELF_SETFLAG( gt, X_GTI_REDRAWMAX, 1 );
   X_GTSELF_EXPOSEAREA( gt, 0, 0, xwc->rows - 1, xwc->cols - 1 );
}

static void s_gt_xwc_Exit( X_GT_BASE *gt )
{
   X_GTXWC *xwc = X_GTXWC_GET( gt );

   if( xwc )
   {
      s_gt_xwc_ClipboardExit( xwc );
      s_gt_xwc_DestroyImage( xwc );
      s_gt_xwc_DestroyFont( xwc );
      s_gt_xwc_ExitInput( xwc );
      if( xwc->framePixmap )
         XFreePixmap( xwc->display, xwc->framePixmap );
      if( xwc->gc )
         XFreeGC( xwc->display, xwc->gc );
      if( xwc->window )
         XDestroyWindow( xwc->display, xwc->window );
      if( xwc->ownColormap )
         XFreeColormap( xwc->display, xwc->colormap );
      if( xwc->iconPixmap )
         XFreePixmap( xwc->display, xwc->iconPixmap );
      if( xwc->iconMask )
         XFreePixmap( xwc->display, xwc->iconMask );
      if( xwc->display )
         XCloseDisplay( xwc->display );
      x_xfree( xwc->title );
      x_xfree( xwc->keys );
      x_xfree( xwc->pixelMap );
      x_xfree( xwc );
      gt->driverData[s_gtId] = NULL;
   }
   s_super.Exit( gt );
}

static int s_gt_xwc_SetMode( X_GT_BASE *gt, int rows, int cols )
{
   X_GTXWC *xwc = X_GTXWC_GET( gt );

   if( rows < X_GTXWC_MIN_ROWS || rows > X_GTXWC_MAX_ROWS || cols < X_GTXWC_MIN_COLS || cols > X_GTXWC_MAX_COLS )
      return 0;

   if( rows * xwc->cellHeight > UINT16_MAX || cols * xwc->cellWidth > UINT16_MAX )
      return 0;

   if( rows != xwc->rows || cols != xwc->cols )
   {
      s_gt_xwc_CreateCanvas( xwc, rows, cols );
      X_GTSELF_RESIZE( gt, rows, cols );
      s_gt_xwc_UpdateSizeLimits( xwc );
      X_GTSELF_EXPOSEAREA( gt, 0, 0, rows - 1, cols - 1 );
      s_gt_xwc_AddKey( xwc, K_RESIZE );
   }
   s_gt_xwc_RequestSize( xwc );
   gt->pFuncTable->Flush( gt );

   return 1;
}

static void s_gt_xwc_Redraw( X_GT_BASE *gt, int row, int col, int size )
{
   X_GTXWC *xwc = X_GTXWC_GET( gt );

   while( size-- > 0 && col < xwc->cols )
   {
      int          color;
      uint8_t      attr;
      uint16_t     character;
      uint32_t     fore, back;
      int          top = row * xwc->cellHeight;
      int          left = col * xwc->cellWidth;
      X_GTXWC_RECT clip = { top, left, top + xwc->cellHeight - 1, left + xwc->cellWidth - 1 };

      if( !X_GTSELF_GETSCRCHAR( gt, row, col, &color, &attr, &character ) )
         break;
      s_gt_xwc_CellColors( xwc, row, col, color, &fore, &back );
      if( xwc->selectionActive )
      {
         int cell = row * xwc->cols + col;
         int first = X_MIN( xwc->selectionAnchor, xwc->selectionEnd );
         int last = X_MAX( xwc->selectionAnchor, xwc->selectionEnd );

         /* a changed character inside the selection ends it */
         if( cell >= first && cell <= last && character != gt->prevBuffer[cell].character )
            s_gt_xwc_ClearSelection( xwc );
      }
      UNUSED( attr );
      if( xwc->fontAttribute & X_GTI_FONTA_CLRBKG )
         s_gt_xwc_FillRect( xwc, top, left, top + xwc->cellHeight - 1, left + xwc->cellWidth - 1, back );
      if( xwc->fontAttribute & X_GTI_FONTA_CTRLCHARS )
         character = x_u16Ctrl( character );
      if( character != ' ' && character != 0x00A0 )
      {
         int box = ( xwc->fontAttribute & X_GTI_FONTA_DRAWBOX ) && character >= 0x2500 && character <= 0x259F;
         int stretch = !( xwc->fontAttribute & X_GTI_FONTA_NOSTRETCH ) || box;
         int height = IIF( stretch, xwc->cellHeight, X_GTXWC_FONT_HEIGHT );
         int width = IIF( stretch, xwc->cellWidth, X_GTXWC_FONT_WIDTH );

         s_gt_xwc_DrawGlyph( xwc, character, top, left, height, width, fore, &clip, 0 );
      }
      ++col;
   }
   xwc->dirty = 1;
}

static void s_gt_xwc_UpdateCursor( X_GTXWC *xwc )
{
   int    row = xwc->cursorRow;
   int    col = xwc->cursorCol;
   int    style = xwc->cursorStyle;
   double now = s_gt_xwc_Time();
   int    visible = xwc->cursorVisible;

   X_GTSELF_GETSCRCURSOR( xwc->gt, &xwc->cursorRow, &xwc->cursorCol, &xwc->cursorStyle );
   if( xwc->cursorStyle < SC_NONE || xwc->cursorStyle > SC_SPECIAL2 )
      xwc->cursorStyle = SC_NONE;
   if( row != xwc->cursorRow || col != xwc->cursorCol || style != xwc->cursorStyle )
   {
      xwc->cursorTime = now;
      xwc->cursorVisible = 1;
      xwc->dirty = 1;
   }
   if( xwc->cursorBlinkRate == 0 || !xwc->focused || xwc->cursorStyle == SC_NONE )
   {
      xwc->cursorVisible = 1;
      xwc->cursorTime = now;
   }
   else if( now - xwc->cursorTime >= xwc->cursorBlinkRate / 1000.0 )
   {
      xwc->cursorVisible = !xwc->cursorVisible;
      xwc->cursorTime = now;
   }
   if( visible != xwc->cursorVisible )
      xwc->dirty = 1;
}

static void s_gt_xwc_DispBegin( X_GT_BASE *gt )
{
   X_GTXWC *xwc = X_GTXWC_GET( gt );

   if( X_GTSELF_DISPCOUNT( gt ) == 0 )
      s_gt_xwc_PumpEvents( xwc );
   s_super.DispBegin( gt );
}

static void s_gt_xwc_DispEnd( X_GT_BASE *gt )
{
   X_GTXWC *xwc = X_GTXWC_GET( gt );

   xwc->frameEnding = X_GTSELF_DISPCOUNT( gt ) == 1;
   s_super.DispEnd( gt );
   xwc->frameEnding = 0;
}

static void s_gt_xwc_Refresh( X_GT_BASE *gt )
{
   X_GTXWC *xwc = X_GTXWC_GET( gt );

   if( X_GTSELF_DISPCOUNT( gt ) != 0 )
      return;

   if( xwc->sizeHintsPending )
      s_gt_xwc_UpdateSizeLimits( xwc );
   if( xwc->resizeRequested )
      s_gt_xwc_RequestSize( xwc );
   if( !xwc->frameEnding )
      s_gt_xwc_PumpEvents( xwc );
   s_super.Refresh( gt );
   s_gt_xwc_UpdateCursor( xwc );
   s_gt_xwc_Present( xwc );
}

static int s_gt_xwc_ReadKey( X_GT_BASE *gt, int mask )
{
   X_GTXWC *xwc = X_GTXWC_GET( gt );

   ( void )mask;
   s_gt_xwc_PumpEvents( xwc );
   if( xwc->resizeNotifyPending )
   {
      xwc->resizeNotifyPending = 0;
      x_callbackEnterDll();
      x_gtResizeNotify( gt );
      x_callbackLeaveDll();
      x_callbackRaisePending();
   }
   if( X_GTSELF_DISPCOUNT( gt ) == 0 )
   {
      s_gt_xwc_UpdateCursor( xwc );
      if( xwc->dirty )
      {
         s_super.Refresh( gt );
         s_gt_xwc_Present( xwc );
      }
   }
   if( xwc->keyOut != xwc->keyIn )
   {
      int key = xwc->keys[xwc->keyOut];

      xwc->keyOut = ( xwc->keyOut + 1 ) % xwc->keyCapacity;
      return key;
   }

   return 0;
}

static void s_gt_xwc_InkeyRestore( X_GTXWC_INKEY_STATE *state )
{
   state->xwc->deferResizePaint = state->previous;
   if( !state->previous )
      state->xwc->resizeFramePending = 0;
}

static int s_gt_xwc_InkeyGet( X_GT_BASE *gt, int wait, double seconds, int mask )
{
   X_GTXWC            *xwc = X_GTXWC_GET( gt );
   X_GTXWC_INKEY_STATE state __attribute__( ( cleanup( s_gt_xwc_InkeyRestore ) ) ) = { xwc, xwc->deferResizePaint };

   xwc->deferResizePaint = state.previous || ( mask & INKEY_GTEVENT ) != 0;

   return s_super.InkeyGet( gt, wait, seconds, mask );
}

static const char *s_gt_xwc_Version( X_GT_BASE *gt, int type )
{
   ( void )gt;

   return IIF( type == 0, "XWC", "X Window Console for xDev" );
}

/* cells keep the 24-bit colors of DispOutAtExt() and DispBoxExt(), see s_gt_xwc_CellColors() */
static int s_gt_xwc_TrueColor( X_GT_BASE *gt )
{
   ( void )gt;

   return 1;
}

/* ---------------------------------------------------------------------------------------------
   Mouse and GT information
   --------------------------------------------------------------------------------------------- */

static int s_gt_xwc_mouse_IsPresent( X_GT_BASE *gt )
{
   return X_GTXWC_GET( gt )->mouseOn;
}

static void s_gt_xwc_mouse_GetPos( X_GT_BASE *gt, int *row, int *col )
{
   X_GTXWC *xwc = X_GTXWC_GET( gt );

   *row = xwc->mouseRow;
   *col = xwc->mouseCol;
}

static void s_gt_xwc_mouse_SetPos( X_GT_BASE *gt, int row, int col )
{
   X_GTXWC *xwc = X_GTXWC_GET( gt );
   int      x = X_MIN( X_MAX( col, 0 ), xwc->cols - 1 ) * xwc->cellWidth + xwc->cellWidth / 2;
   int      y = X_MIN( X_MAX( row, 0 ), xwc->rows - 1 ) * xwc->cellHeight + xwc->cellHeight / 2;

   s_gt_xwc_WarpPointer( xwc, X_MIN( x, INT16_MAX ), X_MIN( y, INT16_MAX ) );
}

static int s_gt_xwc_mouse_ButtonState( X_GT_BASE *gt, int button )
{
   X_GTXWC *xwc = X_GTXWC_GET( gt );

   return xwc->mouseOn && button >= 0 && button < 3 && ( xwc->mouseButtons & ( 1U << button ) ) != 0;
}

static int s_gt_xwc_mouse_CountButton( X_GT_BASE *gt )
{
   ( void )gt;

   return 3;
}

static int s_gt_xwc_PaletteValue( const uint8_t color[3] )
{
   return color[0] | ( ( int )color[1] << 8 ) | ( ( int )color[2] << 16 );
}

static int s_gt_xwc_SetPaletteValue( X_GTXWC *xwc, int index, int64_t value )
{
   if( value < 0 || value > 0xFFFFFF || value == s_gt_xwc_PaletteValue( xwc->colors[index] ) )
      return 0;

   xwc->colors[index][0] = ( uint8_t )value;
   xwc->colors[index][1] = ( uint8_t )( value >> 8 );
   xwc->colors[index][2] = ( uint8_t )( value >> 16 );

   return 1;
}

static void s_gt_xwc_InfoPairResult( X_GT_INFO *info, int first, int second )
{
   if( !info->pResult )
      info->pResult = x_itemNew( NULL );
   x_arrayNew( info->pResult, 2 );
   x_arraySetNI( info->pResult, 1, first );
   x_arraySetNI( info->pResult, 2, second );
}

static int s_gt_xwc_InfoInteger( X_ITEM *item, int minimum, int maximum, int64_t *value )
{
   double number;

   if( !item || !X_IS_NUMERIC( item ) )
      return 0;

   number = x_itemGetND( item );
   if( !isfinite( number ) || number < minimum || number > maximum )
      return 0;

   *value = ( int64_t )number;

   return 1;
}

/* a string without NUL bytes, as file names and titles need */
static int s_gt_xwc_IsText( X_ITEM *item )
{
   if( !item || !X_IS_STRING( item ) )
      return 0;

   return x_itemGetCLen( item ) == strlen( x_itemGetCPtr( item ) );
}

static int s_gt_xwc_InfoCoordinates( X_GT_INFO *info, int64_t *first, int64_t *second )
{
   X_ITEM *one = info->pNewVal;
   X_ITEM *two = info->pNewVal2;

   if( one && X_IS_ARRAY( one ) && x_arrayLen( one ) == 2 )
   {
      two = x_arrayGetItemPtr( one, 2 );
      one = x_arrayGetItemPtr( one, 1 );
   }

   return s_gt_xwc_InfoInteger( one, INT_MIN, INT_MAX, first ) && s_gt_xwc_InfoInteger( two, INT_MIN, INT_MAX, second );
}

static char *s_gt_xwc_FindProgram( const char *name )
{
   const char *path = getenv( "PATH" );
   size_t      nameLength = strlen( name );

   if( !path )
      path = "/usr/bin:/bin";
   for( ;; )
   {
      const char *end = strchr( path, ':' );
      size_t      length = IIF( end, ( size_t )( end - path ), strlen( path ) );
      char       *program = ( char * )x_xgrab( length + nameLength + 2 );
      struct stat attributes;

      if( length )
      {
         memcpy( program, path, length );
         program[length] = '/';
         memcpy( program + length + 1, name, nameLength + 1 );
      }
      else
         memcpy( program, name, nameLength + 1 );
      if( stat( program, &attributes ) == 0 && S_ISREG( attributes.st_mode ) && access( program, X_OK ) == 0 )
         return program;

      x_xfree( program );
      if( !end )
         return NULL;

      path = end + 1;
   }
}

static int s_gt_xwc_Launch( X_GTXWC *xwc, const char *program, char *const arguments[] )
{
   int     descriptors[2], index, status, childError;
   int     connection = ConnectionNumber( xwc->display );
   pid_t   child, waited;
   ssize_t received;
   size_t  used = 0;

   if( pipe( descriptors ) < 0 )
      return 0;

   for( index = 0; index < 2; ++index )
   {
      int descriptor = fcntl( descriptors[index], F_DUPFD_CLOEXEC, 3 );

      if( descriptor < 0 )
      {
         close( descriptors[0] );
         close( descriptors[1] );
         return 0;
      }
      close( descriptors[index] );
      descriptors[index] = descriptor;
   }
   child = fork();
   if( child == 0 )
   {
      pid_t   detached;
      int     nullFile;
      ssize_t sent;

      close( descriptors[0] );
      detached = fork();
      if( detached > 0 )
         _exit( 0 );
      if( detached < 0 || setsid() < 0 )
         goto childFailed;
      close( connection );
      nullFile = open( "/dev/null", O_RDWR );
      if( nullFile < 0 || dup2( nullFile, STDIN_FILENO ) < 0 )
         goto childFailed;
      if( dup2( nullFile, STDOUT_FILENO ) < 0 || dup2( nullFile, STDERR_FILENO ) < 0 )
         goto childFailed;
      if( nullFile > STDERR_FILENO )
         close( nullFile );
      execve( program, arguments, environ );

   childFailed:
      childError = errno;
      do
         sent = write( descriptors[1], &childError, sizeof( childError ) );
      while( sent < 0 && errno == EINTR );
      _exit( 127 );
   }
   close( descriptors[1] );
   if( child < 0 )
   {
      close( descriptors[0] );
      return 0;
   }
   do
   {
      received = read( descriptors[0], ( unsigned char * )&childError + used, sizeof( childError ) - used );
      if( received > 0 )
         used += ( size_t )received;
   } while( ( received < 0 && errno == EINTR ) || ( received > 0 && used < sizeof( childError ) ) );
   close( descriptors[0] );
   do
      waited = waitpid( child, &status, 0 );
   while( waited < 0 && errno == EINTR );

   return received == 0 && used == 0 && waited == child && WIFEXITED( status ) && WEXITSTATUS( status ) == 0;
}

static int s_gt_xwc_OpenUrl( X_GTXWC *xwc, X_ITEM *item )
{
   const char *url, *position;
   char       *program;
   char       *arguments[3];
   int         started;

   if( !item || !X_IS_STRING( item ) )
      return 0;

   url = x_itemGetCPtr( item );
   if( x_itemGetCLen( item ) == 0 || x_itemGetCLen( item ) != strlen( url ) )
      return 0;

   if( !( ( url[0] >= 'A' && url[0] <= 'Z' ) || ( url[0] >= 'a' && url[0] <= 'z' ) ) )
      return 0;
   /* the scheme: letters, digits, "+", "-" and "." up to ":" */
   for( position = url + 1; *position && *position != ':'; ++position )
   {
      char character = *position;
      int  letter = ( character >= 'A' && character <= 'Z' ) || ( character >= 'a' && character <= 'z' );
      int  other = ( character >= '0' && character <= '9' ) || character == '+' || character == '-' || character == '.';

      if( !letter && !other )
         return 0;
   }
   if( *position != ':' )
      return 0;

   program = s_gt_xwc_FindProgram( "xdg-open" );
   if( !program )
      return 0;

   arguments[0] = ( char * )"xdg-open";
   arguments[1] = ( char * )url;
   arguments[2] = NULL;
   started = s_gt_xwc_Launch( xwc, program, arguments );
   x_xfree( program );

   return started;
}

static int s_gt_xwc_Sound( X_GTXWC *xwc, X_ITEM *item )
{
   const char   *name;
   const char   *player;
   char         *program;
   char         *path;
   char         *arguments[5];
   unsigned char header[12];
   size_t        length;
   size_t        received;
   struct stat   attributes;
   FILE         *file;
   int           started;
   int           argumentCount = 0;

   if( !item || !X_IS_STRING( item ) )
      return 0;

   name = x_itemGetCPtr( item );
   length = x_itemGetCLen( item );
   if( !length || length > 65535 || memchr( name, 0, length ) )
      return 0;

   if( stat( name, &attributes ) < 0 || !S_ISREG( attributes.st_mode ) )
      return 0;

   file = fopen( name, "rb" );
   if( !file )
      return 0;

   received = fread( header, 1, sizeof( header ), file );
   fclose( file );
   if( received != sizeof( header ) || memcmp( header, "RIFF", 4 ) != 0 || memcmp( header + 8, "WAVE", 4 ) != 0 )
      return 0;

   player = "paplay";
   program = s_gt_xwc_FindProgram( player );
   if( !program )
   {
      player = "pw-play";
      program = s_gt_xwc_FindProgram( player );
   }
   if( !program )
   {
      player = "aplay";
      program = s_gt_xwc_FindProgram( player );
   }
   if( !program )
      return 0;

   path = ( char * )x_xgrab( length + 3 );
   if( name[0] == '/' )
      memcpy( path, name, length + 1 );
   else
   {
      path[0] = '.';
      path[1] = '/';
      memcpy( path + 2, name, length + 1 );
   }
   arguments[argumentCount++] = ( char * )player;
   if( strcmp( player, "aplay" ) == 0 )
   {
      arguments[argumentCount++] = ( char * )"--quiet";
      arguments[argumentCount++] = ( char * )"--file-type=wav";
   }
   arguments[argumentCount++] = path;
   arguments[argumentCount] = NULL;
   started = s_gt_xwc_Launch( xwc, program, arguments );
   x_xfree( path );
   x_xfree( program );

   return started;
}

static double s_gt_xwc_DisplayScale( X_GTXWC *xwc )
{
   X_GTXWC_DESKTOP_SETTINGS settings;
   X_GTXWC_PROPERTY         reply;
   Window                   root = RootWindow( xwc->display, xwc->screen );
   XrmDatabase              database;
   XrmValue                 value;
   char                    *type;
   double                   scale = 1.0;
   int                      ok;

   if( s_gt_xwc_DesktopSettings( xwc, &settings ) && settings.dpi > 0 )
      return settings.dpi / ( 96.0 * 1024.0 );

   ok = s_gt_xwc_GetProperty( xwc, root, XA_RESOURCE_MANAGER, 16384, False, XA_STRING, &reply );
   if( ok && reply.type == XA_STRING && reply.format == 8 && reply.count > 0 && reply.remaining == 0 )
   {
      XrmInitialize();
      database = XrmGetStringDatabase( ( const char * )reply.data );
      if( database )
      {
         int found = XrmGetResource( database, "Xft.dpi", "Xft.Dpi", &type, &value ) && strcmp( type, "String" ) == 0;

         if( found && value.size > 0 && value.size < 64 )
         {
            char   text[64], *end;
            double dpi;

            memcpy( text, value.addr, value.size );
            text[value.size] = '\0';
            dpi = strtod( text, &end );
            while( isspace( ( unsigned char )*end ) )
               ++end;
            if( end != text && *end == '\0' && isfinite( dpi ) && dpi > 0 )
               scale = dpi / 96.0;
         }
         XrmDestroyDatabase( database );
      }
   }
   if( reply.data )
      XFree( reply.data );

   return scale;
}

static int s_gt_xwc_KeyboardState( X_GTXWC *xwc )
{
   static const KeySym symbols[] = {
      XK_Shift_L, XK_Shift_R, XK_Control_L, XK_Control_R, XK_Alt_L,  XK_Alt_R,
      XK_Super_L, XK_Super_R, XK_Menu,      XK_Meta_L,    XK_Meta_R, XK_ISO_Level3_Shift
   };
   static const int bits[] = { X_GTI_KBD_LSHIFT | X_GTI_KBD_SHIFT,
                               X_GTI_KBD_RSHIFT | X_GTI_KBD_SHIFT,
                               X_GTI_KBD_LCTRL | X_GTI_KBD_CTRL,
                               X_GTI_KBD_RCTRL | X_GTI_KBD_CTRL,
                               X_GTI_KBD_LALT | X_GTI_KBD_ALT,
                               X_GTI_KBD_RALT | X_GTI_KBD_ALT,
                               X_GTI_KBD_LWIN,
                               X_GTI_KBD_RWIN,
                               X_GTI_KBD_MENU,
                               X_GTI_KBD_LALT | X_GTI_KBD_ALT,
                               X_GTI_KBD_RALT | X_GTI_KBD_ALT,
                               X_GTI_KBD_RALT | X_GTI_KBD_ALT };
   XkbStateRec      state;
   char             keys[32];
   int              value = 0;
   size_t           index;

   if( XkbGetState( xwc->display, XkbUseCoreKbd, &state ) == Success )
   {
      value = s_gt_xwc_KeyFlags( xwc, state.mods );
      if( state.locked_mods & xwc->capsLockMask )
         value |= X_GTI_KBD_CAPSLOCK;
      if( state.locked_mods & xwc->numLockMask )
         value |= X_GTI_KBD_NUMLOCK;
      if( state.locked_mods & xwc->scrollLockMask )
         value |= X_GTI_KBD_SCROLOCK;
   }
   XQueryKeymap( xwc->display, keys );
   for( index = 0; index < sizeof( symbols ) / sizeof( symbols[0] ); ++index )
   {
      KeyCode code = XKeysymToKeycode( xwc->display, symbols[index] );

      if( code && ( ( unsigned char )keys[code / 8] & ( 1U << ( code % 8 ) ) ) )
         value |= bits[index];
   }

   return value;
}

static void s_gt_xwc_SetKeyboardState( X_GTXWC *xwc, int value )
{
   unsigned int caps = xwc->capsLockMask;
   unsigned int num = xwc->numLockMask;
   unsigned int scroll = xwc->scrollLockMask;
   unsigned int locked = 0;

   if( value & X_GTI_KBD_CAPSLOCK )
      locked |= caps;
   if( value & X_GTI_KBD_NUMLOCK )
      locked |= num;
   if( value & X_GTI_KBD_SCROLOCK )
      locked |= scroll;
   XkbLockModifiers( xwc->display, XkbUseCoreKbd, caps | num | scroll, locked );
   XFlush( xwc->display );
}

static uint32_t s_gt_xwc_PixelChannel( unsigned long pixel, unsigned long mask )
{
   unsigned int shift = 0;

   if( !mask )
      return 0;

   while( !( mask & 1 ) )
   {
      mask >>= 1;
      ++shift;
   }

   return ( uint32_t )( ( ( uint64_t )( ( pixel >> shift ) & mask ) * 255 + mask / 2 ) / mask );
}

static XImage *s_gt_xwc_ReadBitmap( X_GTXWC *xwc, const char *filename, unsigned int maximum )
{
   unsigned int   width, height;
   unsigned char *data;
   int            hotX, hotY;
   XImage        *image;

   if( XReadBitmapFileData( filename, &width, &height, &data, &hotX, &hotY ) != BitmapSuccess )
      return NULL;

   if( width == 0 || width > maximum || height == 0 || height > maximum || width > X_GTXWC_IMAGE_PIXELS / height )
   {
      XFree( data );
      return NULL;
   }
   image = XCreateImage( xwc->display, xwc->visual, 1, XYBitmap, 0, ( char * )data, width, height, 8, 0 );
   if( !image )
   {
      XFree( data );
      return NULL;
   }
   image->bitmap_unit = 8;
   image->bitmap_bit_order = LSBFirst;
   image->byte_order = LSBFirst;
   if( !XInitImage( image ) )
   {
      XDestroyImage( image );
      return NULL;
   }

   return image;
}

static uint32_t s_gt_xwc_ImageUInt32( const unsigned char *data )
{
   uint32_t low = ( uint32_t )data[0] | ( ( uint32_t )data[1] << 8 );
   uint32_t high = ( ( uint32_t )data[2] << 16 ) | ( ( uint32_t )data[3] << 24 );

   return low | high;
}

static uint32_t *s_gt_xwc_ReadFileImage( X_GTXWC *xwc, const char *filename, unsigned int maximum, int *width,
                                         int *height )
{
   unsigned char  header[138] = { 0 };
   unsigned char *row = NULL;
   uint32_t      *pixels = NULL;
   uint32_t       offset, headerSize, compression, rawWidth, rawHeight;
   uint32_t       masks[4] = { 0x00FF0000, 0x0000FF00, 0x000000FF, 0 };
   uint32_t       palette[256];
   unsigned int   paletteCount = 0;
   int64_t        signedHeight;
   unsigned int   bits;
   size_t         stride;
   struct stat    attributes;
   FILE          *file = fopen( filename, "rb" );
   int            x, y, channel;

   if( !file )
      return NULL;

   if( fread( header, 1, 18, file ) != 18 )
      goto imageFailed;
   if( header[0] != 'B' || header[1] != 'M' )
   {
      XImage *bitmap;

      fclose( file );
      bitmap = s_gt_xwc_ReadBitmap( xwc, filename, maximum );
      if( !bitmap )
         return NULL;

      *width = bitmap->width;
      *height = bitmap->height;
      pixels = ( uint32_t * )x_xgrabArray( ( size_t )*width * *height, sizeof( uint32_t ) );
      for( y = 0; y < *height; ++y )
         for( x = 0; x < *width; ++x )
            pixels[( size_t )y * *width + x] = IIF( XGetPixel( bitmap, x, y ), 0xFFFFFFFFU, 0xFF000000U );
      XDestroyImage( bitmap );
      return pixels;
   }
   offset = s_gt_xwc_ImageUInt32( header + 10 );
   headerSize = s_gt_xwc_ImageUInt32( header + 14 );
   if( headerSize != 40 && headerSize != 52 && headerSize != 56 && headerSize != 108 && headerSize != 124 )
      goto imageFailed;
   if( fread( header + 18, 1, headerSize - 4, file ) != headerSize - 4 || offset < 14 + headerSize )
      goto imageFailed;
   rawWidth = s_gt_xwc_ImageUInt32( header + 18 );
   rawHeight = s_gt_xwc_ImageUInt32( header + 22 );
   signedHeight = IIF( rawHeight > INT32_MAX, ( int64_t )rawHeight - INT64_C( 4294967296 ), rawHeight );
   if( !rawWidth || rawWidth > maximum || !signedHeight )
      goto imageFailed;
   if( signedHeight < -( int64_t )maximum || signedHeight > maximum )
      goto imageFailed;
   *width = ( int )rawWidth;
   *height = ( int )IIF( signedHeight < 0, -signedHeight, signedHeight );
   bits = header[28] | ( ( unsigned int )header[29] << 8 );
   compression = s_gt_xwc_ImageUInt32( header + 30 );
   if( header[26] != 1 || header[27] != 0 )
      goto imageFailed;
   if( bits != 1 && bits != 4 && bits != 8 && bits != 16 && bits != 24 && bits != 32 )
      goto imageFailed;
   if( compression != 0 && !( compression == 3 && ( bits == 16 || bits == 32 ) ) )
      goto imageFailed;
   if( *width > X_GTXWC_IMAGE_PIXELS / *height )
      goto imageFailed;
   if( bits <= 8 )
   {
      unsigned int index;

      paletteCount = s_gt_xwc_ImageUInt32( header + 46 );
      if( paletteCount == 0 )
         paletteCount = 1U << bits;
      if( paletteCount > ( 1U << bits ) || offset - ( 14 + headerSize ) < paletteCount * 4 )
         goto imageFailed;
      for( index = 0; index < paletteCount; ++index )
      {
         unsigned char entry[4];

         if( fread( entry, 1, sizeof( entry ), file ) != sizeof( entry ) )
            goto imageFailed;
         palette[index] = 0xFF000000U | ( ( uint32_t )entry[2] << 16 ) | ( ( uint32_t )entry[1] << 8 ) | entry[0];
      }
   }
   if( bits == 16 )
   {
      masks[0] = 0x7C00;
      masks[1] = 0x03E0;
      masks[2] = 0x001F;
   }
   if( compression == 3 )
   {
      if( headerSize == 40 && ( offset < 66 || fread( header + 54, 1, 12, file ) != 12 ) )
         goto imageFailed;
      for( channel = 0; channel < 3; ++channel )
         masks[channel] = s_gt_xwc_ImageUInt32( header + 54 + channel * 4 );
      if( headerSize >= 56 )
         masks[3] = s_gt_xwc_ImageUInt32( header + 66 );
      for( channel = 0; channel < 4; ++channel )
      {
         uint32_t mask = masks[channel];
         int      other;

         if( bits == 16 && mask > UINT16_MAX )
            goto imageFailed;
         if( !mask )
         {
            if( channel < 3 )
               goto imageFailed;
            continue;
         }
         while( !( mask & 1 ) )
            mask >>= 1;
         if( mask & ( mask + 1 ) )
            goto imageFailed;
         for( other = 0; other < channel; ++other )
            if( masks[channel] & masks[other] )
               goto imageFailed;
      }
   }
   stride = ( ( ( size_t )*width * bits + 31 ) / 32 ) * 4;
   if( fstat( fileno( file ), &attributes ) < 0 )
      goto imageFailed;
   if( attributes.st_size < ( int64_t )offset + ( int64_t )stride * *height )
      goto imageFailed;
   if( fseeko( file, ( off_t )offset, SEEK_SET ) != 0 )
      goto imageFailed;
   row = ( unsigned char * )x_xgrab( stride );
   pixels = ( uint32_t * )x_xgrabArray( ( size_t )*width * *height, sizeof( uint32_t ) );
   for( y = 0; y < *height; ++y )
   {
      int targetY = IIF( signedHeight < 0, y, *height - 1 - y );

      if( fread( row, 1, stride, file ) != stride )
         goto imageFailed;
      for( x = 0; x < *width; ++x )
      {
         if( bits <= 8 )
         {
            unsigned int index = row[x];

            if( bits == 1 )
               index = ( row[x / 8] >> ( 7 - x % 8 ) ) & 1;
            else if( bits == 4 )
               index = ( row[x / 2] >> IIF( x % 2, 0, 4 ) ) & 15;
            if( index >= paletteCount )
               goto imageFailed;
            pixels[( size_t )targetY * *width + x] = palette[index];
         }
         else
         {
            const unsigned char *source = row + ( size_t )x * ( bits / 8 );
            uint32_t             pixel, alpha, red, green, blue;

            if( bits == 16 )
               pixel = source[0] | ( ( uint32_t )source[1] << 8 );
            else if( bits == 32 )
               pixel = s_gt_xwc_ImageUInt32( source );
            else
               pixel = source[0] | ( ( uint32_t )source[1] << 8 ) | ( ( uint32_t )source[2] << 16 );
            alpha = IIF( masks[3], s_gt_xwc_PixelChannel( pixel, masks[3] ), 255 );
            red = s_gt_xwc_PixelChannel( pixel, masks[0] );
            green = s_gt_xwc_PixelChannel( pixel, masks[1] );
            blue = s_gt_xwc_PixelChannel( pixel, masks[2] );
            pixels[( size_t )targetY * *width + x] = ( alpha << 24 ) | ( red << 16 ) | ( green << 8 ) | blue;
         }
      }
   }
   fclose( file );
   x_xfree( row );
   return pixels;

imageFailed:
   fclose( file );
   x_xfree( row );
   x_xfree( pixels );

   return NULL;
}

static int s_gt_xwc_SetIconFile( X_GTXWC *xwc, const char *filename )
{
   Pixmap         pixmap = None;
   Pixmap         mask = None;
   GC             gc = NULL;
   XWMHints      *hints;
   XImage        *image = NULL;
   char          *data = NULL;
   char          *maskData = NULL;
   uint32_t      *argb = NULL;
   unsigned long *pixels = NULL;
   XErrorHandler  previous;
   int            success;
   int            width = 0, height = 0;
   int            x, y;
   unsigned int   iconWidth, iconHeight;
   unsigned int   depth = ( unsigned int )xwc->desktopDepth;

   if( *filename )
   {
      size_t maskStride;

      argb = s_gt_xwc_ReadFileImage( xwc, filename, 1024, &width, &height );
      if( !argb )
         return 0;

      iconWidth = ( unsigned int )width;
      iconHeight = ( unsigned int )height;
      image = XCreateImage( xwc->display, xwc->visual, depth, ZPixmap, 0, NULL, iconWidth, iconHeight, 32, 0 );
      if( !image )
         goto iconFailed;
      data = ( char * )x_xgrabzArray( image->bytes_per_line, height );
      image->data = data;
      maskStride = ( ( size_t )width + 7 ) / 8;
      maskData = ( char * )x_xgrabzArray( maskStride, height );
      for( y = 0; y < height; ++y )
         for( x = 0; x < width; ++x )
         {
            XPutPixel( image, x, y, s_gt_xwc_Pixel( xwc, argb[( size_t )y * width + x] & 0xFFFFFF ) );
            if( ( argb[( size_t )y * width + x] >> 24 ) >= 128 )
               maskData[( size_t )y * maskStride + x / 8] |= ( char )( 1U << ( x % 8 ) );
         }
      previous = s_gt_xwc_ErrorTrap( xwc );
      pixmap = XCreatePixmap( xwc->display, xwc->window, iconWidth, iconHeight, depth );
      mask = XCreateBitmapFromData( xwc->display, xwc->window, maskData, iconWidth, iconHeight );
      success = s_gt_xwc_ErrorUntrap( xwc, previous );
      x_xfree( maskData );
      maskData = NULL;
      if( !success || !pixmap || !mask )
         goto iconFailed;
      previous = s_gt_xwc_ErrorTrap( xwc );
      gc = XCreateGC( xwc->display, pixmap, 0, NULL );
      if( gc )
      {
         XPutImage( xwc->display, pixmap, gc, image, 0, 0, 0, 0, iconWidth, iconHeight );
         XFreeGC( xwc->display, gc );
         gc = NULL;
      }
      else
         success = 0;
      success = s_gt_xwc_ErrorUntrap( xwc, previous ) && success;
      if( !success )
         goto iconFailed;
      image->data = NULL;
      XDestroyImage( image );
      image = NULL;
      x_xfree( data );
      data = NULL;
   }
   hints = XGetWMHints( xwc->display, xwc->window );
   if( !hints )
      hints = XAllocWMHints();
   if( !hints )
      goto iconFailed;
   hints->flags &= ~( IconPixmapHint | IconMaskHint );
   hints->icon_pixmap = hints->icon_mask = None;
   if( pixmap )
   {
      size_t               index, count = ( size_t )width * height;
      size_t               chunk = X_MIN( ( size_t )( XMaxRequestSize( xwc->display ) - 32 ), 16384 );
      unsigned long        size[2] = { ( unsigned long )width, ( unsigned long )height };
      const unsigned char *bytes = ( const unsigned char * )size;
      Atom                 icon = xwc->netWmIcon;

      hints->flags |= IconPixmapHint | IconMaskHint;
      hints->icon_pixmap = pixmap;
      hints->icon_mask = mask;
      pixels = ( unsigned long * )x_xgrabArray( count, sizeof( unsigned long ) );
      for( index = 0; index < count; ++index )
         pixels[index] = argb[index];
      /* _NET_WM_ICON: width and height, then the pixels in parts that fit one request */
      XChangeProperty( xwc->display, xwc->window, icon, XA_CARDINAL, 32, PropModeReplace, bytes, 2 );
      for( index = 0; index < count; index += chunk )
      {
         const unsigned char *part = ( const unsigned char * )( pixels + index );
         int                  items = ( int )X_MIN( count - index, chunk );

         XChangeProperty( xwc->display, xwc->window, icon, XA_CARDINAL, 32, PropModeAppend, part, items );
      }
   }
   else
      XDeleteProperty( xwc->display, xwc->window, xwc->netWmIcon );
   XSetWMHints( xwc->display, xwc->window, hints );
   XFree( hints );
   x_xfree( pixels );
   x_xfree( argb );
   if( xwc->iconPixmap )
      XFreePixmap( xwc->display, xwc->iconPixmap );
   if( xwc->iconMask )
      XFreePixmap( xwc->display, xwc->iconMask );
   xwc->iconPixmap = pixmap;
   xwc->iconMask = mask;
   XFlush( xwc->display );
   return 1;

iconFailed:
   if( pixmap || mask )
   {
      previous = s_gt_xwc_ErrorTrap( xwc );
      if( pixmap )
         XFreePixmap( xwc->display, pixmap );
      if( mask )
         XFreePixmap( xwc->display, mask );
      s_gt_xwc_ErrorUntrap( xwc, previous );
   }
   if( image )
   {
      image->data = NULL;
      XDestroyImage( image );
   }
   x_xfree( data );
   x_xfree( maskData );
   x_xfree( argb );

   return 0;
}

static int s_gt_xwc_DispImage( X_GTXWC *xwc, X_ITEM *source, X_ITEM *position )
{
   XImage   *image = NULL;
   XColor   *palette = NULL;
   uint32_t *argb = NULL;
   int       borrowed = 0;
   int       left = 0, top = 0, drawLeft, drawTop;
   int       imageWidth = 0, imageHeight = 0;
   int64_t   right, bottom;
   int       x, y;

   if( source && X_IS_STRING( source ) && x_itemGetCLen( source ) == strlen( x_itemGetCPtr( source ) ) )
      argb = s_gt_xwc_ReadFileImage( xwc, x_itemGetCPtr( source ), UINT16_MAX, &imageWidth, &imageHeight );
   else if( source && X_IS_ARRAY( source ) && ( x_arrayLen( source ) == 3 || x_arrayLen( source ) == 4 ) )
   {
      X_ITEM      *bytes = x_arrayGetItemPtr( source, 1 );
      X_ITEM      *widthItem = x_arrayGetItemPtr( source, 2 );
      X_ITEM      *heightItem = x_arrayGetItemPtr( source, 3 );
      X_ITEM      *depthItem = x_arrayGetItemPtr( source, 4 );
      int64_t      width, height, depth;
      int          pad;
      unsigned int planes, columns, lines;

      if( !X_IS_STRING( bytes ) || !s_gt_xwc_InfoInteger( widthItem, 1, UINT16_MAX, &width ) )
         return 0;

      if( !s_gt_xwc_InfoInteger( heightItem, 1, UINT16_MAX, &height ) || width > X_GTXWC_IMAGE_PIXELS / height )
         return 0;

      depth = xwc->desktopDepth;
      if( x_arrayLen( source ) == 4 && !s_gt_xwc_InfoInteger( depthItem, 0, 32, &depth ) )
         return 0;

      if( depth == 0 )
         depth = xwc->desktopDepth;
      if( depth != 1 && depth != xwc->desktopDepth )
         return 0;

      planes = ( unsigned int )depth;
      columns = ( unsigned int )width;
      lines = ( unsigned int )height;
      /* the row padding that gives exactly the length of the data */
      for( pad = 32; pad >= 8; pad /= 2 )
      {
         image = XCreateImage( xwc->display, xwc->visual, planes, ZPixmap, 0, NULL, columns, lines, pad, 0 );
         if( !image )
            return 0;

         if( ( size_t )image->bytes_per_line * image->height == x_itemGetCLen( bytes ) )
         {
            image->data = ( char * )x_itemGetCPtr( bytes );
            borrowed = 1;
            break;
         }
         XDestroyImage( image );
         image = NULL;
      }
   }
   if( !image && !argb )
      return 0;

   if( image )
   {
      imageWidth = image->width;
      imageHeight = image->height;
   }
   right = imageWidth - 1;
   bottom = imageHeight - 1;
   if( position && !X_IS_NIL( position ) )
   {
      size_t count = IIF( X_IS_ARRAY( position ), x_arrayLen( position ), 0 );
      size_t index;

      if( count != 2 && count != 4 )
         goto failed;
      for( index = 1; index <= count; ++index )
      {
         X_ITEM *item = x_arrayGetItemPtr( position, index );
         int64_t value;

         if( !s_gt_xwc_InfoInteger( item, INT_MIN, INT_MAX, &value ) )
            goto failed;
      }
      left = x_arrayGetNI( position, 1 );
      top = x_arrayGetNI( position, 2 );
      right += left;
      bottom += top;
      if( count == 4 )
      {
         if( x_arrayGetNI( position, 3 ) < left || x_arrayGetNI( position, 4 ) < top )
            goto failed;
         right = X_MIN( right, x_arrayGetNI( position, 3 ) );
         bottom = X_MIN( bottom, x_arrayGetNI( position, 4 ) );
      }
   }
   right = X_MIN( right, xwc->canvasWidth - 1 );
   bottom = X_MIN( bottom, xwc->canvasHeight - 1 );
   drawLeft = X_MAX( left, 0 );
   drawTop = X_MAX( top, 0 );
   if( xwc->clipOn )
   {
      drawLeft = X_MAX( drawLeft, xwc->clipLeft );
      drawTop = X_MAX( drawTop, xwc->clipTop );
      right = X_MIN( right, xwc->clipRight );
      bottom = X_MIN( bottom, xwc->clipBottom );
   }
   if( right < drawLeft || bottom < drawTop )
      goto imageDone;
   if( image && image->depth != 1 && xwc->visual->class != TrueColor && xwc->visual->class != DirectColor )
   {
      int index;

      palette = ( XColor * )x_xgrabzArray( xwc->visual->map_entries, sizeof( XColor ) );
      for( index = 0; index < xwc->visual->map_entries; ++index )
         palette[index].pixel = index;
      XQueryColors( xwc->display, xwc->colormap, palette, xwc->visual->map_entries );
      for( y = drawTop; y <= bottom; ++y )
         for( x = drawLeft; x <= right; ++x )
            if( XGetPixel( image, x - left, y - top ) >= ( unsigned long )xwc->visual->map_entries )
               goto failed;
   }
   s_super.Refresh( xwc->gt );
   for( y = drawTop; y <= bottom; ++y )
      for( x = drawLeft; x <= right; ++x )
      {
         unsigned long pixel;
         uint32_t      rgb;

         if( argb )
            pixel = argb[( size_t )( y - top ) * imageWidth + x - left];
         else
            pixel = XGetPixel( image, x - left, y - top );

         if( argb )
         {
            uint32_t alpha = ( uint32_t )pixel >> 24;
            uint32_t inverse = 255 - alpha;
            uint32_t background = xwc->canvas[( size_t )y * xwc->canvasWidth + x];
            uint32_t red = ( ( pixel >> 16 ) & 255 ) * alpha + ( ( background >> 16 ) & 255 ) * inverse;
            uint32_t green = ( ( pixel >> 8 ) & 255 ) * alpha + ( ( background >> 8 ) & 255 ) * inverse;
            uint32_t blue = ( pixel & 255 ) * alpha + ( background & 255 ) * inverse;

            red = ( red + 127 ) / 255;
            green = ( green + 127 ) / 255;
            blue = ( blue + 127 ) / 255;
            rgb = ( red << 16 ) | ( green << 8 ) | blue;
         }
         else if( image->depth == 1 )
            rgb = IIF( pixel, 0xFFFFFF, 0 );
         else if( palette )
         {
            XColor  *color = &palette[pixel];
            uint32_t red = color->red / 257;
            uint32_t green = color->green / 257;
            uint32_t blue = color->blue / 257;

            rgb = ( red << 16 ) | ( green << 8 ) | blue;
         }
         else
         {
            uint32_t red = s_gt_xwc_PixelChannel( pixel, xwc->visual->red_mask );
            uint32_t green = s_gt_xwc_PixelChannel( pixel, xwc->visual->green_mask );
            uint32_t blue = s_gt_xwc_PixelChannel( pixel, xwc->visual->blue_mask );

            rgb = ( red << 16 ) | ( green << 8 ) | blue;
         }
         xwc->canvas[( size_t )y * xwc->canvasWidth + x] = rgb;
      }
   xwc->dirty = 1;
   s_gt_xwc_UpdateCursor( xwc );
   s_gt_xwc_Present( xwc );

imageDone:
   if( borrowed )
      image->data = NULL;
   if( image )
      XDestroyImage( image );
   x_xfree( argb );
   x_xfree( palette );
   return 1;

failed:
   if( borrowed )
      image->data = NULL;
   if( image )
      XDestroyImage( image );
   x_xfree( argb );
   x_xfree( palette );

   return 0;
}

static XChar2b *s_gt_xwc_DialogText( const char *text, size_t bytes, int *length )
{
   XChar2b  *characters = ( XChar2b * )x_xgrabArray( bytes + 1, sizeof( XChar2b ) + sizeof( uint16_t ) );
   uint16_t *unicode = ( uint16_t * )( characters + bytes + 1 );
   size_t    count = x_utf8ToU16( text, bytes, unicode, bytes + 1 );
   size_t    index;
   int       output = 0;

   for( index = 0; index < count; ++index )
   {
      uint16_t code = unicode[index];

      if( code == '\r' )
      {
         if( index + 1 < count && unicode[index + 1] == '\n' )
            ++index;
         code = '\n';
      }
      else if( code < 32 && code != '\n' )
         code = ' ';
      characters[output].byte1 = ( unsigned char )( code >> 8 );
      characters[output++].byte2 = ( unsigned char )code;
   }
   *length = output;

   return characters;
}

static void s_gt_xwc_DialogDrop( X_GTXWC_DIALOG *dialog )
{
   X_GTXWC *xwc = dialog->xwc;
   int      index;

   if( dialog->window )
   {
      XErrorHandler previous = s_gt_xwc_ErrorTrap( xwc );

      XDestroyWindow( xwc->display, dialog->window );
      s_gt_xwc_ErrorUntrap( xwc, previous );
   }
   if( xwc->dialog == dialog )
   {
      XEvent event;
      long   mask = KeyPressMask | KeyReleaseMask | ButtonPressMask | ButtonReleaseMask | PointerMotionMask;

      xwc->dialog = NULL;
      s_gt_xwc_ResetMouse( xwc );
      /* input that came while the dialog was open does not reach the program */
      while( XCheckWindowEvent( xwc->display, xwc->window, mask, &event ) )
         ;
   }
   if( dialog->gc )
      XFreeGC( xwc->display, dialog->gc );
   if( dialog->pixmap )
      XFreePixmap( xwc->display, dialog->pixmap );
   for( index = 0; dialog->buttons && index < dialog->buttonCount; ++index )
      x_xfree( dialog->buttons[index].text );
   x_xfree( dialog->buttons );
   x_xfree( dialog->lines );
   x_xfree( dialog->text );
   x_xfree( dialog->message );
   xwc->dirty = 1;
   XFlush( xwc->display );
}

static void s_gt_xwc_DialogDraw( X_GTXWC_DIALOG *dialog )
{
   X_GTXWC *xwc = dialog->xwc;
   Display *display = xwc->display;
   int      index;

   XSetForeground( display, dialog->gc, s_gt_xwc_Pixel( xwc, 0xF0F0F0 ) );
   XFillRectangle( display, dialog->pixmap, dialog->gc, 0, 0, dialog->width, dialog->height );
   XSetForeground( display, dialog->gc, s_gt_xwc_Pixel( xwc, 0x000000 ) );
   for( index = 0; index < dialog->visibleLines && index + dialog->firstLine < dialog->lineCount; ++index )
   {
      X_GTXWC_DIALOG_LINE *line = &dialog->lines[index + dialog->firstLine];
      int                  baseline = 24 + xwc->font->ascent + index * X_GTXWC_FONT_HEIGHT;

      XDrawString16( display, dialog->pixmap, dialog->gc, 24, baseline, dialog->text + line->start, line->length );
   }
   if( dialog->lineCount > dialog->visibleLines )
   {
      char position[64];
      int  first = dialog->firstLine + 1;
      int  last = dialog->firstLine + dialog->visibleLines;
      int  baseline = 24 + dialog->visibleLines * X_GTXWC_FONT_HEIGHT + xwc->font->ascent;
      int  length;

      length = snprintf( position, sizeof( position ), "%d-%d / %d  [PgUp/PgDn]", first, last, dialog->lineCount );
      XDrawString( display, dialog->pixmap, dialog->gc, 24, baseline, position, length );
   }
   for( index = 0; index < dialog->buttonCount; ++index )
   {
      X_GTXWC_DIALOG_BUTTON *button = &dialog->buttons[index];
      int                    width = button->rect.right - button->rect.left + 1;
      int                    height = button->rect.bottom - button->rect.top + 1;
      int                    columns = ( width - 24 ) / X_GTXWC_FONT_WIDTH;
      int                    length = X_MIN( button->length, columns );
      int                    left = button->rect.left + ( width - length * X_GTXWC_FONT_WIDTH ) / 2;
      int                    baseline = button->rect.top + ( height - X_GTXWC_FONT_HEIGHT ) / 2 + xwc->font->ascent;
      unsigned long          fill = s_gt_xwc_Pixel( xwc, IIF( index == dialog->selected, 0x245A94, 0xFFFFFF ) );
      unsigned long          ink = s_gt_xwc_Pixel( xwc, IIF( index == dialog->selected, 0xFFFFFF, 0x000000 ) );

      XSetForeground( display, dialog->gc, fill );
      XFillRectangle( display, dialog->pixmap, dialog->gc, button->rect.left, button->rect.top, width, height );
      XSetForeground( display, dialog->gc, s_gt_xwc_Pixel( xwc, 0x404040 ) );
      XDrawRectangle( display, dialog->pixmap, dialog->gc, button->rect.left, button->rect.top, width - 1, height - 1 );
      XSetForeground( display, dialog->gc, ink );
      if( button->length > columns )
      {
         /* a label too long for the button ends with "..." */
         int dots = left + ( length - 3 ) * X_GTXWC_FONT_WIDTH;

         XDrawString16( display, dialog->pixmap, dialog->gc, left, baseline, button->text, length - 3 );
         XDrawString( display, dialog->pixmap, dialog->gc, dots, baseline, "...", 3 );
      }
      else
         XDrawString16( display, dialog->pixmap, dialog->gc, left, baseline, button->text, length );
   }
   XCopyArea( display, dialog->pixmap, dialog->window, dialog->gc, 0, 0, dialog->width, dialog->height, 0, 0 );
   XFlush( display );
}

static int s_gt_xwc_InRect( const X_GTXWC_RECT *rect, int x, int y )
{
   return x >= rect->left && x <= rect->right && y >= rect->top && y <= rect->bottom;
}

static int s_gt_xwc_DialogEvent( X_GTXWC *xwc, XEvent *event )
{
   X_GTXWC_DIALOG *dialog = xwc->dialog;
   int             scroll = dialog->firstLine;
   int             input = event->type == KeyPress || event->type == KeyRelease || event->type == ButtonPress;
   int             closing = event->type == ClientMessage && event->xclient.message_type == xwc->wmProtocols;

   if( event->type == ButtonRelease || event->type == MotionNotify )
      input = 1;
   if( closing && ( event->xclient.format != 32 || ( Atom )event->xclient.data.l[0] != xwc->wmDelete ) )
      closing = 0;
   if( event->xany.window != dialog->window )
   {
      /* the terminal window gets no input while the dialog is open */
      if( event->xany.window == xwc->window )
      {
         if( input )
            return 1;

         if( closing && xwc->closeMode < 2 )
            dialog->done = 1;
      }
      return 0;
   }
   if( event->type == DestroyNotify )
   {
      dialog->window = None;
      dialog->done = 1;
      return 1;
   }
   if( dialog->done )
      return 1;

   switch( event->type )
   {
      case Expose:
         if( event->xexpose.count == 0 )
            s_gt_xwc_DialogDraw( dialog );
         break;
      case FocusOut: dialog->pressed = -1; return 1;
      case KeyPress:
      {
         char   buffer[16];
         KeySym symbol = NoSymbol;
         int    key;

         XLookupString( &event->xkey, buffer, sizeof( buffer ), &symbol, NULL );
         key = s_gt_xwc_KeyCode( symbol );
         if( key == KX_ESC )
            dialog->done = 1;
         else if( key == KX_ENTER || symbol == XK_space )
         {
            dialog->result = dialog->selected + 1;
            dialog->done = 1;
         }
         else if( key == KX_TAB || key == KX_LEFT || key == KX_RIGHT )
         {
            int shiftTab = key == KX_TAB && ( event->xkey.state & ShiftMask );
            int back = key == KX_LEFT || symbol == XK_ISO_Left_Tab || shiftTab;
            int step = IIF( back, -1, 1 );

            dialog->selected = ( dialog->selected + step + dialog->buttonCount ) % dialog->buttonCount;
         }
         else if( key == KX_UP )
            --scroll;
         else if( key == KX_DOWN )
            ++scroll;
         else if( key == KX_PGUP )
            scroll -= dialog->visibleLines;
         else if( key == KX_PGDN )
            scroll += dialog->visibleLines;
         else if( key == KX_HOME )
            scroll = 0;
         else if( key == KX_END )
            scroll = dialog->lineCount;
         break;
      }
      case ButtonPress:
         if( event->xbutton.button == Button4 )
            scroll -= 3;
         else if( event->xbutton.button == Button5 )
            scroll += 3;
         else if( event->xbutton.button == Button1 )
         {
            int index;

            dialog->pressed = -1;
            for( index = 0; index < dialog->buttonCount; ++index )
            {
               X_GTXWC_RECT *rect = &dialog->buttons[index].rect;

               if( s_gt_xwc_InRect( rect, event->xbutton.x, event->xbutton.y ) )
                  dialog->selected = dialog->pressed = index;
            }
         }
         break;
      case ButtonRelease:
         if( event->xbutton.button == Button1 && dialog->pressed >= 0 )
         {
            X_GTXWC_RECT *rect = &dialog->buttons[dialog->pressed].rect;

            if( s_gt_xwc_InRect( rect, event->xbutton.x, event->xbutton.y ) )
            {
               dialog->result = dialog->pressed + 1;
               dialog->done = 1;
            }
            dialog->pressed = -1;
         }
         break;
      case ClientMessage:
         if( closing )
            dialog->done = 1;
         break;
      default: return 1;
   }
   dialog->firstLine = X_MAX( 0, X_MIN( scroll, dialog->lineCount - dialog->visibleLines ) );
   if( !dialog->done )
      s_gt_xwc_DialogDraw( dialog );

   return 1;
}

static int s_gt_xwc_MessageBox( X_GTXWC *xwc, X_ITEM *text, X_ITEM *buttons, double delay )
{
   X_GTXWC_DIALOG       dialog __attribute__( ( cleanup( s_gt_xwc_DialogDrop ) ) ) = { .xwc = xwc, .pressed = -1 };
   X_ITEM              *parts[3] = { text, NULL, NULL };
   X_ITEM              *titleItem = NULL;
   const char          *title = xwc->title;
   size_t               lengths[3] = { 0, 0, 0 };
   size_t               total = 0;
   size_t               offset = 0;
   int                  textLength;
   int                  columns;
   int                  buttonWidth = 90;
   int                  perRow;
   int                  buttonRows;
   int                  maximumLines;
   int                  index;
   int                  x = 0, y = 0;
   Window               child;
   XWindowAttributes    parent;
   XSetWindowAttributes attributes;
   XSizeHints           sizeHints = { 0 };
   XWMHints             wmHints = { 0 };
   Atom                 modal, windowType, dialogType;
   XTextProperty        titleProperty = { 0 };
   struct pollfd        descriptor = { ConnectionNumber( xwc->display ), POLLIN, 0 };
   double               deadline = IIF( isfinite( delay ) && delay > 0, s_gt_xwc_Time() + delay, 0 );
   Display             *display = xwc->display;
   Visual              *visual = xwc->visual;
   Window               window, root = RootWindow( xwc->display, xwc->screen );
   unsigned long        valueMask = CWColormap | CWBackPixel | CWEventMask;
   unsigned int         width, height;
   int                  depth = xwc->desktopDepth;
   int                  placed, titleLength;
   const unsigned char *bytes;

   if( xwc->dialog || !text || ( !X_IS_STRING( text ) && !X_IS_ARRAY( text ) ) )
      return 0;

   if( X_IS_ARRAY( text ) )
   {
      titleItem = x_arrayGetItemPtr( text, 1 );
      for( index = 0; index < 3; ++index )
         parts[index] = x_arrayGetItemPtr( text, index + 2 );
      if( !buttons || X_IS_NIL( buttons ) )
         buttons = x_arrayGetItemPtr( text, 5 );
   }
   if( titleItem && !X_IS_NIL( titleItem ) )
   {
      if( !X_IS_STRING( titleItem ) || x_itemGetCLen( titleItem ) > 65535 )
         return 0;

      if( memchr( x_itemGetCPtr( titleItem ), 0, x_itemGetCLen( titleItem ) ) )
         return 0;

      title = x_itemGetCPtr( titleItem );
   }
   for( index = 0; index < 3; ++index )
   {
      if( parts[index] && !X_IS_NIL( parts[index] ) )
      {
         if( !X_IS_STRING( parts[index] ) )
            return 0;

         lengths[index] = x_itemGetCLen( parts[index] );
         if( lengths[index] > X_GTXWC_DIALOG_LIMIT - total )
            return 0;

         if( memchr( x_itemGetCPtr( parts[index] ), 0, lengths[index] ) )
            return 0;

         total += lengths[index];
      }
   }
   if( buttons && !X_IS_NIL( buttons ) && !X_IS_ARRAY( buttons ) )
      return 0;

   if( buttons && x_arrayLen( buttons ) > X_GTXWC_DIALOG_BUTTONS )
      return 0;

   dialog.buttonCount = IIF( buttons, ( int )x_arrayLen( buttons ), 0 );
   if( dialog.buttonCount == 0 )
      dialog.buttonCount = 1;
   dialog.buttons = ( X_GTXWC_DIALOG_BUTTON * )x_xgrabz( dialog.buttonCount * sizeof( *dialog.buttons ) );
   for( index = 0; index < dialog.buttonCount; ++index )
   {
      X_ITEM     *item = IIF( buttons && x_arrayLen( buttons ), x_arrayGetItemPtr( buttons, index + 1 ), NULL );
      const char *label = IIF( item, x_itemGetCPtr( item ), "OK" );
      size_t      length = IIF( item, x_itemGetCLen( item ), 2 );
      int         character;

      if( item && ( !X_IS_STRING( item ) || length == 0 || length > 65535 || memchr( label, 0, length ) ) )
         return 0;

      dialog.buttons[index].text = s_gt_xwc_DialogText( label, length, &dialog.buttons[index].length );
      /* a button label is one line */
      for( character = 0; character < dialog.buttons[index].length; ++character )
      {
         XChar2b *glyph = &dialog.buttons[index].text[character];

         if( glyph->byte1 == 0 && glyph->byte2 == '\n' )
            glyph->byte2 = ' ';
      }
      buttonWidth = X_MAX( buttonWidth, X_MIN( dialog.buttons[index].length, 24 ) * X_GTXWC_FONT_WIDTH + 24 );
   }
   dialog.message = ( char * )x_xgrab( total + 5 );
   for( index = 0; index < 3; ++index )
   {
      if( lengths[index] )
      {
         if( offset )
         {
            dialog.message[offset++] = '\n';
            dialog.message[offset++] = '\n';
         }
         memcpy( dialog.message + offset, x_itemGetCPtr( parts[index] ), lengths[index] );
         offset += lengths[index];
      }
   }
   dialog.text = s_gt_xwc_DialogText( dialog.message, offset, &textLength );
   dialog.width = X_MIN( 780, DisplayWidth( xwc->display, xwc->screen ) - 64 );
   columns = ( dialog.width - 48 ) / X_GTXWC_FONT_WIDTH;
   if( columns < 8 )
      return 0;

   dialog.lines = ( X_GTXWC_DIALOG_LINE * )x_xgrabArray( textLength + 1, sizeof( *dialog.lines ) );
   index = 0;
   do
   {
      X_GTXWC_DIALOG_LINE *line = &dialog.lines[dialog.lineCount++];

      line->start = index;
      while( index < textLength && index - line->start < columns )
      {
         if( dialog.text[index].byte1 == 0 && dialog.text[index].byte2 == '\n' )
            break;
         ++index;
      }
      line->length = index - line->start;
      if( index < textLength && dialog.text[index].byte1 == 0 && dialog.text[index].byte2 == '\n' )
      {
         ++index;
         if( index == textLength )
         {
            dialog.lines[dialog.lineCount].start = index;
            dialog.lines[dialog.lineCount++].length = 0;
         }
      }
   } while( index < textLength );
   buttonWidth = X_MIN( buttonWidth, dialog.width - 48 );
   perRow = ( dialog.width - 40 ) / ( buttonWidth + 8 );
   perRow = X_MAX( perRow, 1 );
   buttonRows = ( dialog.buttonCount + perRow - 1 ) / perRow;
   maximumLines = ( DisplayHeight( xwc->display, xwc->screen ) - 96 - 72 - buttonRows * 38 ) / X_GTXWC_FONT_HEIGHT;
   if( maximumLines < 1 )
      return 0;

   dialog.visibleLines = X_MIN( dialog.lineCount, X_MIN( maximumLines, 20 ) );
   dialog.height = 72 + dialog.visibleLines * X_GTXWC_FONT_HEIGHT + buttonRows * 38;
   for( index = 0; index < dialog.buttonCount; ++index )
   {
      X_GTXWC_RECT *rect = &dialog.buttons[index].rect;
      int           rowCount = X_MIN( perRow, dialog.buttonCount - index / perRow * perRow );

      rect->left = ( dialog.width - rowCount * ( buttonWidth + 8 ) + 8 ) / 2 + index % perRow * ( buttonWidth + 8 );
      rect->right = rect->left + buttonWidth - 1;
      rect->top = 48 + dialog.visibleLines * X_GTXWC_FONT_HEIGHT + index / perRow * 38;
      rect->bottom = rect->top + 29;
   }
   /* the dialog is centred on the terminal window */
   placed = XGetWindowAttributes( display, xwc->window, &parent );
   if( placed )
      placed = XTranslateCoordinates( display, xwc->window, root, 0, 0, &x, &y, &child );
   if( placed )
   {
      x += ( parent.width - dialog.width ) / 2;
      y += ( parent.height - dialog.height ) / 2;
   }
   x = X_MAX( 0, X_MIN( x, DisplayWidth( xwc->display, xwc->screen ) - dialog.width ) );
   y = X_MAX( 0, X_MIN( y, DisplayHeight( xwc->display, xwc->screen ) - dialog.height ) );
   attributes.colormap = xwc->colormap;
   attributes.background_pixel = s_gt_xwc_Pixel( xwc, 0xF0F0F0 );
   attributes.event_mask = ExposureMask | StructureNotifyMask | FocusChangeMask;
   attributes.event_mask |= KeyPressMask | KeyReleaseMask | ButtonPressMask | ButtonReleaseMask;
   width = ( unsigned int )dialog.width;
   height = ( unsigned int )dialog.height;
   window = XCreateWindow( display, root, x, y, width, height, 0, depth, InputOutput, visual, valueMask, &attributes );
   dialog.window = window;
   dialog.pixmap = XCreatePixmap( xwc->display, dialog.window, dialog.width, dialog.height, xwc->desktopDepth );
   dialog.gc = XCreateGC( xwc->display, dialog.window, 0, NULL );
   XSetGraphicsExposures( xwc->display, dialog.gc, False );
   XSetFont( xwc->display, dialog.gc, xwc->font->fid );
   XSetTransientForHint( xwc->display, dialog.window, xwc->window );
   XSetWMProtocols( xwc->display, dialog.window, &xwc->wmDelete, 1 );
   sizeHints.flags = PMinSize | PMaxSize;
   sizeHints.min_width = sizeHints.max_width = dialog.width;
   sizeHints.min_height = sizeHints.max_height = dialog.height;
   XSetWMNormalHints( xwc->display, dialog.window, &sizeHints );
   wmHints.flags = InputHint;
   wmHints.input = True;
   XSetWMHints( xwc->display, dialog.window, &wmHints );
   if( Xutf8TextListToTextProperty( xwc->display, ( char ** )&title, 1, XStdICCTextStyle, &titleProperty ) >= Success )
   {
      XSetWMName( xwc->display, dialog.window, &titleProperty );
      XFree( titleProperty.value );
   }
   bytes = ( const unsigned char * )title;
   titleLength = ( int )strlen( title );
   XChangeProperty( display, window, xwc->netWmName, xwc->utf8String, 8, PropModeReplace, bytes, titleLength );
   modal = XInternAtom( display, "_NET_WM_STATE_MODAL", False );
   windowType = XInternAtom( display, "_NET_WM_WINDOW_TYPE", False );
   dialogType = XInternAtom( display, "_NET_WM_WINDOW_TYPE_DIALOG", False );
   bytes = ( const unsigned char * )&modal;
   XChangeProperty( display, window, xwc->netWmState, XA_ATOM, 32, PropModeReplace, bytes, 1 );
   bytes = ( const unsigned char * )&dialogType;
   XChangeProperty( display, window, windowType, XA_ATOM, 32, PropModeReplace, bytes, 1 );
   xwc->dialog = &dialog;
   s_gt_xwc_ResetMouse( xwc );
   XMapRaised( xwc->display, dialog.window );
   s_gt_xwc_DialogDraw( &dialog );
   while( !dialog.done )
   {
      s_gt_xwc_PumpEvents( xwc );
      if( deadline && s_gt_xwc_Time() >= deadline && !dialog.done )
      {
         dialog.result = dialog.selected + 1;
         dialog.done = 1;
      }
      if( !dialog.done )
      {
         s_gt_xwc_ClipboardExpire( xwc );
         if( xwc->dirty && X_GTSELF_DISPCOUNT( xwc->gt ) == 0 )
         {
            s_super.Refresh( xwc->gt );
            s_gt_xwc_UpdateCursor( xwc );
            s_gt_xwc_Present( xwc );
         }
         if( poll( &descriptor, 1, 10 ) < 0 && errno != EINTR )
            dialog.done = 1;
      }
   }

   return dialog.result;
}

static int s_gt_xwc_Alert( X_GT_BASE *gt, X_ITEM *message, X_ITEM *options, int normal, int high, double delay )
{
   X_GTXWC *xwc = X_GTXWC_GET( gt );

   if( xwc->msgAlert )
      return s_gt_xwc_MessageBox( xwc, message, options, delay );

   return s_super.Alert( gt, message, options, normal, high, delay );
}

static void s_gt_xwc_CommandDrop( X_GTXWC_COMMAND *command )
{
   int index;

   if( command->child > 0 )
   {
      kill( command->child, SIGKILL );
      while( waitpid( command->child, NULL, 0 ) < 0 && errno == EINTR )
         ;
   }
   for( index = 0; index < 4; ++index )
      if( command->descriptors[index] >= 0 )
         close( command->descriptors[index] );
   if( command->actionsInitialized )
      posix_spawn_file_actions_destroy( &command->actions );
   for( index = 0; index < command->argumentCount; ++index )
      x_xfree( command->arguments[index] );
   x_xfree( command->environment );
   x_xfree( command->displayEnvironment );
   x_xfree( command->program );
   x_xfree( command->output );
   x_xfree( command->errors );
   if( command->modal.xwc->dialog == &command->modal )
      s_gt_xwc_DialogDrop( &command->modal );
}

/* a command that has not started: no child process and no pipes */
static X_GTXWC_COMMAND s_gt_xwc_CommandNew( X_GTXWC *xwc )
{
   X_GTXWC_COMMAND command = { .modal = { .xwc = xwc }, .child = -1, .descriptors = { -1, -1, -1, -1 } };

   return command;
}

static void s_gt_xwc_CommandArgument( X_GTXWC_COMMAND *command, const char *option, const char *value )
{
   size_t optionLength = strlen( option );
   size_t valueLength = IIF( value, strlen( value ), 0 );
   char  *argument = ( char * )x_xgrab( optionLength + valueLength + 2 );

   memcpy( argument, option, optionLength );
   if( value )
   {
      argument[optionLength++] = '=';
      memcpy( argument + optionLength, value, valueLength );
   }
   argument[optionLength + valueLength] = '\0';
   command->arguments[command->argumentCount++] = argument;
}

static char *s_gt_xwc_FileFilter( X_ITEM *item )
{
   X_ITEM     *nameItem, patternItem;
   const char *name, *pattern, *position;
   size_t      nameLength, patternLength, used;
   char       *argument;
   const char *prefix = "--file-filter=";

   if( !item || !X_IS_ARRAY( item ) || x_arrayLen( item ) != 2 )
      return NULL;

   nameItem = x_arrayGetItemPtr( item, 1 );
   patternItem = x_arrayGetItemPtr( item, 2 );
   if( !X_IS_STRING( nameItem ) || !X_IS_STRING( patternItem ) )
      return NULL;

   name = x_itemGetCPtr( nameItem );
   pattern = x_itemGetCPtr( patternItem );
   nameLength = x_itemGetCLen( nameItem );
   patternLength = x_itemGetCLen( patternItem );
   if( !nameLength || !patternLength || nameLength > 4096 || patternLength > 4096 )
      return NULL;

   if( nameLength != strlen( name ) || patternLength != strlen( pattern ) || strchr( name, '|' ) )
      return NULL;

   argument = ( char * )x_xgrab( strlen( prefix ) + nameLength + 3 + patternLength * 3 + 1 );
   used = strlen( prefix );
   memcpy( argument, prefix, used );
   memcpy( argument + used, name, nameLength );
   used += nameLength;
   memcpy( argument + used, " | ", 3 );
   used += 3;
   position = pattern;
   for( ;; )
   {
      const char *end = strchr( position, ';' );
      size_t      length = IIF( end, ( size_t )( end - position ), strlen( position ) );
      size_t      index;

      if( length == 1 && *position == '*' )
         argument[used++] = '*';
      else
      {
         if( length >= 2 && position[0] == '*' && position[1] == '.' )
         {
            position += 2;
            length -= 2;
         }
         if( length == 0 )
            goto invalidFilter;
         for( index = 0; index < length; ++index )
         {
            unsigned char ch = ( unsigned char )position[index];
            int           letter = ( ch >= 'a' && ch <= 'z' ) || ( ch >= 'A' && ch <= 'Z' );
            int           other = ( ch >= '0' && ch <= '9' ) || ch == '-' || ch == '_' || ch == '.';

            if( !letter && !other )
               goto invalidFilter;
         }
         argument[used++] = '*';
         argument[used++] = '.';
         memcpy( argument + used, position, length );
         used += length;
      }
      if( !end )
         break;
      argument[used++] = ' ';
      position = end + 1;
   }
   argument[used] = '\0';
   return argument;

invalidFilter:
   x_xfree( argument );

   return NULL;
}

static int s_gt_xwc_CommandRead( int *descriptor, char **data, size_t *length, size_t *capacity, size_t limit )
{
   char    buffer[4096];
   ssize_t received;

   if( *descriptor < 0 )
      return 1;

   for( ;; )
   {
      received = read( *descriptor, buffer, sizeof( buffer ) );
      if( received == 0 )
      {
         close( *descriptor );
         *descriptor = -1;
         return 1;
      }
      if( received < 0 && ( errno == EAGAIN || errno == EWOULDBLOCK ) )
         return 1;

      if( received < 0 )
      {
         if( errno == EINTR )
            continue;
         return 0;
      }
      if( ( size_t )received > limit - *length )
      {
         errno = EOVERFLOW;
         return 0;
      }
      if( *length + ( size_t )received + 1 > *capacity )
      {
         size_t next = X_MAX( *capacity * 2, *length + ( size_t )received + 1 );

         *capacity = X_MIN( next, limit + 1 );
         *data = ( char * )x_xrealloc( *data, *capacity );
      }
      memcpy( *data + *length, buffer, ( size_t )received );
      *length += ( size_t )received;
      ( *data )[*length] = '\0';
   }
}

/* reads what the command has written so far to its output and error pipes */
static int s_gt_xwc_CommandCollect( X_GTXWC_COMMAND *command, size_t outputLimit, size_t errorLimit )
{
   int    *output = &command->descriptors[0];
   int    *errors = &command->descriptors[2];
   size_t *outputSize = &command->outputCapacity;
   size_t *errorSize = &command->errorCapacity;

   if( !s_gt_xwc_CommandRead( output, &command->output, &command->outputLength, outputSize, outputLimit ) )
      return 0;

   return s_gt_xwc_CommandRead( errors, &command->errors, &command->errorLength, errorSize, errorLimit );
}

static int s_gt_xwc_CommandStart( X_GTXWC_COMMAND *command )
{
   X_GTXWC *xwc = command->modal.xwc;
   int      index;
   int      spawnError;
   pid_t    child;
   char   **environment = IIF( command->environment, command->environment, environ );

   for( index = 0; index < 4; index += 2 )
   {
      if( pipe( command->descriptors + index ) < 0 )
         return 0;

      for( int end = index; end < index + 2; ++end )
      {
         int descriptor = fcntl( command->descriptors[end], F_DUPFD_CLOEXEC, 3 );

         if( descriptor < 0 )
            return 0;

         close( command->descriptors[end] );
         command->descriptors[end] = descriptor;
      }
      if( fcntl( command->descriptors[index], F_SETFL, O_NONBLOCK ) < 0 )
         return 0;
   }
   spawnError = posix_spawn_file_actions_init( &command->actions );
   if( spawnError )
      goto commandSpawnError;
   command->actionsInitialized = 1;
   spawnError = posix_spawn_file_actions_addclose( &command->actions, ConnectionNumber( xwc->display ) );
   if( !spawnError )
      spawnError = posix_spawn_file_actions_addopen( &command->actions, STDIN_FILENO, "/dev/null", O_RDONLY, 0 );
   if( !spawnError )
      spawnError = posix_spawn_file_actions_adddup2( &command->actions, command->descriptors[1], STDOUT_FILENO );
   if( !spawnError )
      spawnError = posix_spawn_file_actions_adddup2( &command->actions, command->descriptors[3], STDERR_FILENO );
   for( index = 0; index < 4 && !spawnError; ++index )
      spawnError = posix_spawn_file_actions_addclose( &command->actions, command->descriptors[index] );
   if( spawnError )
      goto commandSpawnError;
   spawnError = posix_spawn( &child, command->program, &command->actions, NULL, command->arguments, environment );
   if( spawnError )
      goto commandSpawnError;
   command->child = child;
   close( command->descriptors[1] );
   close( command->descriptors[3] );
   command->descriptors[1] = command->descriptors[3] = -1;
   return 1;

commandSpawnError:
   errno = spawnError;

   return 0;
}

static int s_gt_xwc_CommandWait( X_GTXWC_COMMAND *command, int *status, size_t outputLimit, size_t errorLimit,
                                 double deadline )
{
   X_GTXWC *xwc = command->modal.xwc;

   for( ;; )
   {
      struct pollfd descriptors[3];
      pid_t         waited;

      /* the X11 connection, the output and the error pipe of the command */
      memset( descriptors, 0, sizeof( descriptors ) );
      descriptors[0].fd = ConnectionNumber( xwc->display );
      descriptors[1].fd = command->descriptors[0];
      descriptors[2].fd = command->descriptors[2];
      descriptors[0].events = descriptors[1].events = descriptors[2].events = POLLIN;
      s_gt_xwc_PumpEvents( xwc );
      if( xwc->dialog == &command->modal && command->modal.done )
         return 0;

      if( !s_gt_xwc_CommandCollect( command, outputLimit, errorLimit ) )
         return -1;

      waited = waitpid( command->child, status, WNOHANG );
      if( waited == command->child )
      {
         command->child = -1;
         if( !s_gt_xwc_CommandCollect( command, outputLimit, errorLimit ) )
            return -1;

         return 1;
      }
      if( waited < 0 && errno != EINTR )
      {
         if( errno == ECHILD )
            command->child = -1;
         return -1;
      }
      s_gt_xwc_ClipboardExpire( xwc );
      if( xwc->dirty && X_GTSELF_DISPCOUNT( xwc->gt ) == 0 )
      {
         s_super.Refresh( xwc->gt );
         s_gt_xwc_UpdateCursor( xwc );
         s_gt_xwc_Present( xwc );
      }
      if( deadline && s_gt_xwc_Time() >= deadline )
      {
         errno = ETIMEDOUT;
         return -1;
      }
      if( poll( descriptors, 3, 10 ) < 0 && errno != EINTR )
         return -1;
   }
}

static const char *s_gt_xwc_SystemTheme( X_GTXWC *xwc )
{
   double deadline = s_gt_xwc_Time() + 2.0;
   int    legacy;

   for( legacy = 0; legacy < 2; ++legacy )
   {
      X_GTXWC_COMMAND command __attribute__( ( cleanup( s_gt_xwc_CommandDrop ) ) ) = s_gt_xwc_CommandNew( xwc );
      int             status = 0;
      int             consumed = 0;
      int             valueOffset = 0;
      char            scheme = '\0';
      const char     *method = "org.freedesktop.portal.Settings.ReadOne";
      const char     *format = " ( < uint32%n %c > , ) %n";
      int             whole;

      /* an older portal has only Read(), with the value in one more variant */
      if( legacy )
      {
         method = "org.freedesktop.portal.Settings.Read";
         format = " ( < < uint32%n %c > > , ) %n";
      }

      if( s_gt_xwc_Time() >= deadline )
         break;
      command.program = s_gt_xwc_FindProgram( "gdbus" );
      if( !command.program )
         break;
      s_gt_xwc_CommandArgument( &command, "gdbus", NULL );
      s_gt_xwc_CommandArgument( &command, "call", NULL );
      s_gt_xwc_CommandArgument( &command, "--session", NULL );
      s_gt_xwc_CommandArgument( &command, "--dest", "org.freedesktop.portal.Desktop" );
      s_gt_xwc_CommandArgument( &command, "--object-path", "/org/freedesktop/portal/desktop" );
      s_gt_xwc_CommandArgument( &command, "--method", method );
      s_gt_xwc_CommandArgument( &command, "--timeout", "1" );
      s_gt_xwc_CommandArgument( &command, "org.freedesktop.appearance", NULL );
      s_gt_xwc_CommandArgument( &command, "color-scheme", NULL );

      if( !s_gt_xwc_CommandStart( &command ) )
         break;
      if( s_gt_xwc_CommandWait( &command, &status, 4096, 4096, deadline ) != 1 )
         break;
      if( !WIFEXITED( status ) || WEXITSTATUS( status ) != 0 )
      {
         int unknownMethod = 0;

         if( !legacy && command.errorLength && !memchr( command.errors, 0, command.errorLength ) )
            unknownMethod = strstr( command.errors, "org.freedesktop.DBus.Error.UnknownMethod" ) != NULL;
         if( unknownMethod )
            continue;
         break;
      }
      if( !command.outputLength || memchr( command.output, 0, command.outputLength ) )
         break;
      sscanf( command.output, format, &valueOffset, &scheme, &consumed );
      whole = consumed > 0 && ( size_t )consumed == command.outputLength;
      if( valueOffset > 0 && isspace( ( unsigned char )command.output[valueOffset] ) && whole )
      {
         if( scheme == '1' )
            return "DARK";

         if( scheme == '2' )
            return "LIGHT";
      }
      break;
   }

   return "UNKNOWN";
}

static X_ITEM *s_gt_xwc_FileDialog( X_GTXWC *xwc, X_ITEM *options, X_ITEM *result )
{
   X_GTXWC_COMMAND dialog __attribute__( ( cleanup( s_gt_xwc_CommandDrop ) ) ) = s_gt_xwc_CommandNew( xwc );
   X_ITEM         *values[5] = { NULL, NULL, NULL, NULL, NULL };
   X_ITEM         *files;
   const char     *failure = "";
   char            errorText[256];
   char            attach[32];
   char           *location;
   const char     *separator = "\n//\n";
   size_t          separatorLength = strlen( separator );
   int             mode = 0;
   int             filterCount = 0;
   int             fileCount = 0;
   int             status = 0;
   int             index;
   int             many = 0;
   int             accepted = 0;
   int             listValid;
   struct stat     attributes;

   if( xwc->dialog )
   {
      failure = "Another modal dialog is already open.";
      goto fileResult;
   }
   if( options && !X_IS_NIL( options ) )
   {
      if( !X_IS_ARRAY( options ) || x_arrayLen( options ) > 5 )
      {
         failure = "File dialog options must be { type, title, location, filters, multiple }.";
         goto fileResult;
      }
      for( index = 0; index < 5; ++index )
         values[index] = x_arrayGetItemPtr( options, index + 1 );
   }
   for( index = 0; index < 3; ++index )
   {
      X_ITEM *value = values[index];
      int     invalid;

      if( !value || X_IS_NIL( value ) )
         continue;
      invalid = !X_IS_STRING( value ) || x_itemGetCLen( value ) > 65535;
      if( !invalid && x_itemGetCLen( value ) != strlen( x_itemGetCPtr( value ) ) )
         invalid = 1;
      if( invalid )
      {
         failure = "File dialog type, title and location must be strings without NUL bytes.";
         goto fileResult;
      }
   }
   if( values[0] && x_itemGetCLen( values[0] ) )
   {
      const char *type = x_itemGetCPtr( values[0] );

      if( x_stricmp( type, "SAVE" ) == 0 )
         mode = 1;
      else if( x_stricmp( type, "FOLDER" ) == 0 )
         mode = 2;
      else if( x_stricmp( type, "OPEN" ) != 0 )
      {
         failure = "File dialog type must be OPEN, SAVE or FOLDER.";
         goto fileResult;
      }
   }
   if( values[3] && !X_IS_NIL( values[3] ) )
   {
      if( !X_IS_ARRAY( values[3] ) || x_arrayLen( values[3] ) > 64 )
      {
         failure = "File dialog filters must be an array with at most 64 entries.";
         goto fileResult;
      }
      if( mode != 2 )
         filterCount = ( int )x_arrayLen( values[3] );
   }
   if( values[4] && !X_IS_NIL( values[4] ) && !X_IS_LOGICAL( values[4] ) )
   {
      failure = "File dialog multiple selection must be a logical value.";
      goto fileResult;
   }
   many = mode != 1 && values[4] && x_itemGetL( values[4] );
   dialog.program = s_gt_xwc_FindProgram( "zenity" );
   if( !dialog.program )
   {
      failure = "zenity was not found in PATH. Install the zenity package to use file dialogs.";
      goto fileResult;
   }
   s_gt_xwc_CommandArgument( &dialog, "zenity", NULL );
   s_gt_xwc_CommandArgument( &dialog, "--file-selection", NULL );
   s_gt_xwc_CommandArgument( &dialog, "--separator", separator );
   s_gt_xwc_CommandArgument( &dialog, "--modal", NULL );
   snprintf( attach, sizeof( attach ), "0x%lx", ( unsigned long )xwc->window );
   s_gt_xwc_CommandArgument( &dialog, "--attach", attach );
   if( many )
      s_gt_xwc_CommandArgument( &dialog, "--multiple", NULL );
   if( mode == 1 )
   {
      s_gt_xwc_CommandArgument( &dialog, "--save", NULL );
      s_gt_xwc_CommandArgument( &dialog, "--confirm-overwrite", NULL );
   }
   else if( mode == 2 )
      s_gt_xwc_CommandArgument( &dialog, "--directory", NULL );
   if( values[1] && X_IS_STRING( values[1] ) )
      s_gt_xwc_CommandArgument( &dialog, "--title", x_itemGetCPtr( values[1] ) );
   if( values[2] && x_itemGetCLen( values[2] ) )
   {
      const char *path = x_itemGetCPtr( values[2] );
      size_t      length = x_itemGetCLen( values[2] );

      s_gt_xwc_CommandArgument( &dialog, "--filename", path );
      if( path[length - 1] != '/' && stat( path, &attributes ) == 0 && S_ISDIR( attributes.st_mode ) )
      {
         location = dialog.arguments[dialog.argumentCount - 1];
         length = strlen( location );
         location = ( char * )x_xrealloc( location, length + 2 );
         location[length] = '/';
         location[length + 1] = '\0';
         dialog.arguments[dialog.argumentCount - 1] = location;
      }
   }
   for( index = 0; index < filterCount; ++index )
   {
      char *filter = s_gt_xwc_FileFilter( x_arrayGetItemPtr( values[3], index + 1 ) );

      if( !filter )
      {
         failure = "Each file filter must be { label, extensions }, for example { 'Sources', 'prg;c' }.";
         goto fileResult;
      }
      dialog.arguments[dialog.argumentCount++] = filter;
   }
   for( index = 0; environ[index]; ++index )
      ;
   dialog.environment = ( char ** )x_xgrabArray( index + 3, sizeof( char * ) );
   dialog.displayEnvironment = ( char * )x_xgrab( strlen( DisplayString( xwc->display ) ) + sizeof( "DISPLAY=" ) );
   memcpy( dialog.displayEnvironment, "DISPLAY=", sizeof( "DISPLAY=" ) - 1 );
   strcpy( dialog.displayEnvironment + sizeof( "DISPLAY=" ) - 1, DisplayString( xwc->display ) );
   index = 0;
   for( int entry = 0; environ[entry]; ++entry )
      if( strncmp( environ[entry], "GDK_BACKEND=", 12 ) != 0 && strncmp( environ[entry], "DISPLAY=", 8 ) != 0 )
         dialog.environment[index++] = environ[entry];
   dialog.environment[index++] = ( char * )"GDK_BACKEND=x11";
   dialog.environment[index++] = dialog.displayEnvironment;
   dialog.environment[index] = NULL;
   if( !s_gt_xwc_CommandStart( &dialog ) )
      goto fileSystemError;
   xwc->dialog = &dialog.modal;
   s_gt_xwc_ResetMouse( xwc );
   index = s_gt_xwc_CommandWait( &dialog, &status, X_GTXWC_CLIPBOARD_LIMIT, 65536, 0 );
   if( index < 0 )
      goto fileSystemError;
   if( index == 0 )
      goto fileResult;
   if( WIFEXITED( status ) && WEXITSTATUS( status ) == 1 )
      goto fileResult;
   if( !WIFEXITED( status ) || WEXITSTATUS( status ) != 0 )
   {
      if( dialog.errorLength && strlen( dialog.errors ) == dialog.errorLength )
         failure = dialog.errors;
      else
      {
         const char *reason = IIF( WIFEXITED( status ), "exit status", "signal" );
         int         code = 0;

         if( WIFEXITED( status ) )
            code = WEXITSTATUS( status );
         else if( WIFSIGNALED( status ) )
            code = WTERMSIG( status );
         snprintf( errorText, sizeof( errorText ), "zenity failed (%s %d).", reason, code );
         failure = errorText;
      }
      goto fileResult;
   }
   /* the paths end with a new line and contain no NUL byte */
   listValid = dialog.outputLength && dialog.output[dialog.outputLength - 1] == '\n';
   if( listValid && memchr( dialog.output, 0, dialog.outputLength ) )
      listValid = 0;
   if( !listValid )
   {
      failure = "zenity returned an invalid file list.";
      goto fileResult;
   }
   dialog.output[--dialog.outputLength] = '\0';
   location = dialog.output;
   for( ;; )
   {
      char  *end = IIF( many, strstr( location, separator ), NULL );
      size_t length = IIF( end, ( size_t )( end - location ), strlen( location ) );
      int    usable;

      if( end )
         *end = '\0';
      /* an absolute path; a chosen file or folder must exist and be of that kind */
      usable = length && location[0] == '/' && !strstr( location, "//" );
      if( usable && mode != 1 )
      {
         if( stat( location, &attributes ) < 0 )
            usable = 0;
         else if( mode == 2 )
            usable = S_ISDIR( attributes.st_mode );
         else
            usable = !S_ISDIR( attributes.st_mode );
      }
      if( !usable )
      {
         failure = "zenity returned an invalid or unavailable local path.";
         goto fileResult;
      }
      if( ++fileCount > 4096 )
      {
         failure = "Too many paths were selected (maximum 4096).";
         goto fileResult;
      }
      if( !end )
         break;
      location = end + separatorLength;
   }
   accepted = 1;
   goto fileResult;

fileSystemError:
   snprintf( errorText, sizeof( errorText ), "File dialog failed: %s.", strerror( errno ) );
   failure = errorText;
fileResult:
   if( !result )
      result = x_itemArrayNew( 4 );
   else
      x_arrayNew( result, 4 );
   x_arraySetL( result, 1, accepted );
   files = x_arrayGetItemPtr( result, 2 );
   x_arrayNew( files, IIF( accepted, fileCount, 0 ) );
   if( accepted )
   {
      location = dialog.output;
      for( index = 0; index < fileCount; ++index )
      {
         x_arraySetC( files, index + 1, location );
         if( index + 1 < fileCount )
            location += strlen( location ) + separatorLength;
      }
   }
   x_arraySetNI( result, 3, 0 );
   x_arraySetC( result, 4, failure );

   return result;
}

static int s_gt_xwc_Info( X_GT_BASE *gt, int type, X_GT_INFO *info )
{
   X_GTXWC *xwc = X_GTXWC_GET( gt );

   s_gt_xwc_PumpEvents( xwc );
   switch( type )
   {
      case X_GTI_ISGRAPHIC:
      case X_GTI_ISSCREENPOS:
      case X_GTI_KBDSUPPORT:
      case X_GTI_ISUNICODE: info->pResult = x_itemPutL( info->pResult, 1 ); break;
      case X_GTI_ISCTWIN:
      case X_GTI_ISMULTIWIN: info->pResult = x_itemPutL( info->pResult, 0 ); break;
      case X_GTI_ONLINE:
         info->pResult = x_itemPutL( info->pResult, xwc->display != NULL && xwc->window != None );
         break;
      case X_GTI_VERSION:
      {
         int64_t version = 0;

         s_gt_xwc_InfoInteger( info->pNewVal, INT_MIN, INT_MAX, &version );
         info->pResult = x_itemPutC( info->pResult, s_gt_xwc_Version( gt, ( int )version ) );
         break;
      }
      case X_GTI_RENDERERNAME: info->pResult = x_itemPutC( info->pResult, "Xlib/software canvas" ); break;
      case X_GTI_SYSTEMTHEME: info->pResult = x_itemPutC( info->pResult, s_gt_xwc_SystemTheme( xwc ) ); break;
      case X_GTI_OPENURL: info->pResult = x_itemPutL( info->pResult, s_gt_xwc_OpenUrl( xwc, info->pNewVal ) ); break;
      case X_GTI_SOUND: info->pResult = x_itemPutL( info->pResult, s_gt_xwc_Sound( xwc, info->pNewVal ) ); break;
      case X_GTI_MSGBOX:
         if( !info->pNewVal || X_IS_NIL( info->pNewVal ) || X_IS_LOGICAL( info->pNewVal ) )
         {
            info->pResult = x_itemPutL( info->pResult, xwc->msgAlert );
            if( info->pNewVal && X_IS_LOGICAL( info->pNewVal ) )
               xwc->msgAlert = x_itemGetL( info->pNewVal );
         }
         else
         {
            int button = s_gt_xwc_MessageBox( xwc, info->pNewVal, info->pNewVal2, 0 );

            info->pResult = x_itemPutNI( info->pResult, button );
         }
         break;
      case X_GTI_FILEDIALOG: info->pResult = s_gt_xwc_FileDialog( xwc, info->pNewVal, info->pResult ); break;
      case X_GTI_INPUTFD: info->pResult = x_itemPutNI( info->pResult, ConnectionNumber( xwc->display ) ); break;
      case X_GTI_OUTPUTFD:
      case X_GTI_ERRORFD:
         info->pResult = x_itemPutNI( info->pResult, IIF( type == X_GTI_OUTPUTFD, gt->hStdout, gt->hStderr ) );
         break;
      case X_GTI_SCREENWIDTH:
      {
         int64_t value;

         info->pResult = x_itemPutNI( info->pResult, xwc->canvasWidth );
         if( s_gt_xwc_InfoInteger( info->pNewVal, 1, UINT16_MAX, &value ) )
            s_gt_xwc_ResizeWindow( xwc, value, xwc->canvasHeight );
         break;
      }
      case X_GTI_SCREENHEIGHT:
      {
         int64_t value;

         info->pResult = x_itemPutNI( info->pResult, xwc->canvasHeight );
         if( s_gt_xwc_InfoInteger( info->pNewVal, 1, UINT16_MAX, &value ) )
            s_gt_xwc_ResizeWindow( xwc, xwc->canvasWidth, value );
         break;
      }
      case X_GTI_WINDOWWIDTH:
      case X_GTI_WINDOWHEIGHT:
      {
         XWindowAttributes attributes;
         int               width = xwc->canvasWidth;
         int               height = xwc->canvasHeight;
         int64_t           value;

         if( XGetWindowAttributes( xwc->display, xwc->window, &attributes ) )
         {
            width = attributes.width;
            height = attributes.height;
         }
         info->pResult = x_itemPutNI( info->pResult, IIF( type == X_GTI_WINDOWWIDTH, width, height ) );
         if( s_gt_xwc_InfoInteger( info->pNewVal, 1, UINT16_MAX, &value ) )
         {
            int64_t newWidth = IIF( type == X_GTI_WINDOWWIDTH, value, xwc->canvasWidth );
            int64_t newHeight = IIF( type == X_GTI_WINDOWHEIGHT, value, xwc->canvasHeight );

            s_gt_xwc_ResizeWindow( xwc, newWidth, newHeight );
         }
         break;
      }
      case X_GTI_OUTPUTWIDTH:
      case X_GTI_LOGICALWIDTH: info->pResult = x_itemPutNI( info->pResult, xwc->canvasWidth ); break;
      case X_GTI_OUTPUTHEIGHT:
      case X_GTI_LOGICALHEIGHT: info->pResult = x_itemPutNI( info->pResult, xwc->canvasHeight ); break;
      case X_GTI_VIEWMAXWIDTH:
      case X_GTI_VIEWPORTWIDTH: info->pResult = x_itemPutNI( info->pResult, xwc->cols ); break;
      case X_GTI_VIEWMAXHEIGHT:
      case X_GTI_VIEWPORTHEIGHT: info->pResult = x_itemPutNI( info->pResult, xwc->rows ); break;
      case X_GTI_SCREENDEPTH:
      case X_GTI_DESKTOPDEPTH: info->pResult = x_itemPutNI( info->pResult, xwc->desktopDepth ); break;
      case X_GTI_DESKTOPWIDTH:
      case X_GTI_DESKTOPHEIGHT:
      case X_GTI_DESKTOPROWS:
      case X_GTI_DESKTOPCOLS:
      {
         XWindowAttributes attributes;
         int               value;

         if( XGetWindowAttributes( xwc->display, RootWindow( xwc->display, xwc->screen ), &attributes ) )
         {
            xwc->desktopWidth = attributes.width;
            xwc->desktopHeight = attributes.height;
         }
         if( type == X_GTI_DESKTOPWIDTH )
            value = xwc->desktopWidth;
         else if( type == X_GTI_DESKTOPHEIGHT )
            value = xwc->desktopHeight;
         else if( type == X_GTI_DESKTOPCOLS )
            value = xwc->desktopWidth / xwc->cellWidth;
         else
            value = xwc->desktopHeight / xwc->cellHeight;
         info->pResult = x_itemPutNI( info->pResult, value );
         break;
      }
      case X_GTI_PIXELDENSITY:
      case X_GTI_CONTENTSCALE: info->pResult = x_itemPutND( info->pResult, 1.0 ); break;
      case X_GTI_DISPLAYSCALE: info->pResult = x_itemPutND( info->pResult, s_gt_xwc_DisplayScale( xwc ) ); break;
      case X_GTI_SCREENSIZE:
      {
         int64_t width, height;

         s_gt_xwc_InfoPairResult( info, xwc->canvasWidth, xwc->canvasHeight );
         if( s_gt_xwc_InfoCoordinates( info, &width, &height ) )
            s_gt_xwc_ResizeWindow( xwc, width, height );
         break;
      }
      case X_GTI_FONTSIZE:
      case X_GTI_FONTWIDTH:
         info->pResult = x_itemPutNI( info->pResult, IIF( type == X_GTI_FONTSIZE, xwc->cellHeight, xwc->cellWidth ) );
         if( info->pNewVal )
         {
            int64_t value;
            int     current = IIF( type == X_GTI_FONTSIZE, xwc->cellHeight, xwc->cellWidth );
            int     cells = IIF( type == X_GTI_FONTSIZE, xwc->rows, xwc->cols );

            int valid = s_gt_xwc_InfoInteger( info->pNewVal, 1, 256, &value );

            /* the window must stay within 65535 pixels */
            if( valid && value != current && value * cells <= UINT16_MAX )
            {
               int cellWidth = IIF( type == X_GTI_FONTWIDTH, ( int )value, xwc->cellWidth );
               int cellHeight = IIF( type == X_GTI_FONTSIZE, ( int )value, xwc->cellHeight );

               s_gt_xwc_SetCellSize( xwc, cellWidth, cellHeight );
               s_gt_xwc_RequestSize( xwc );
               gt->pFuncTable->Flush( gt );
            }
         }
         break;
      case X_GTI_FONTNAME:
      case X_GTI_FONTSEL:
         info->pResult = x_itemPutC( info->pResult, IIF( type == X_GTI_FONTNAME, xwc->fontName, xwc->fontSel ) );
         if( info->pNewVal && X_IS_STRING( info->pNewVal ) )
         {
            const char *name = x_itemGetCPtr( info->pNewVal );
            int         valid = x_itemGetCLen( info->pNewVal ) == strlen( name );
            int         changed = 0;

            /* the same font selection is not loaded again */
            if( valid && type == X_GTI_FONTSEL && strcmp( name, xwc->fontSel ) == 0 )
               valid = 0;
            if( valid && type == X_GTI_FONTNAME )
               changed = s_gt_xwc_SetFontFamily( xwc, name, xwc->fontWeight );
            else if( valid )
               changed = s_gt_xwc_SetFont( xwc, name );
            if( changed )
            {
               X_GTSELF_EXPOSEAREA( gt, 0, 0, xwc->rows - 1, xwc->cols - 1 );
               gt->pFuncTable->Flush( gt );
            }
         }
         break;
      case X_GTI_FONTWEIGHT:
         info->pResult = x_itemPutNI( info->pResult, xwc->fontWeight );
         if( info->pNewVal )
         {
            int64_t weight;
            int     changed = s_gt_xwc_InfoInteger( info->pNewVal, X_GTI_FONTW_THIN, X_GTI_FONTW_BOLD, &weight );

            if( changed && weight != xwc->fontWeight )
               changed = s_gt_xwc_SetFontFamily( xwc, xwc->fontName, ( int )weight );
            else
               changed = 0;
            if( changed )
            {
               X_GTSELF_EXPOSEAREA( gt, 0, 0, xwc->rows - 1, xwc->cols - 1 );
               gt->pFuncTable->Flush( gt );
            }
         }
         break;
      case X_GTI_FONTQUALITY: info->pResult = x_itemPutNI( info->pResult, X_GTI_FONTQ_DRAFT ); break;
      case X_GTI_FONTATTRIBUTE:
      {
         int64_t value;

         info->pResult = x_itemPutNI( info->pResult, xwc->fontAttribute );
         if( s_gt_xwc_InfoInteger( info->pNewVal, INT_MIN, INT_MAX, &value ) )
         {
            int mask = X_GTI_FONTA_CLRBKG | X_GTI_FONTA_DRAWBOX | X_GTI_FONTA_CTRLCHARS | X_GTI_FONTA_NOSTRETCH;
            int attributes = X_GTI_FONTA_FIXMETRIC | ( int )( value & mask );

            if( attributes != xwc->fontAttribute )
            {
               xwc->fontAttribute = attributes;
               X_GTSELF_EXPOSEAREA( gt, 0, 0, xwc->rows - 1, xwc->cols - 1 );
               xwc->dirty = 1;
               gt->pFuncTable->Flush( gt );
            }
         }
         break;
      }
      case X_GTI_CURSORBLINKRATE:
      {
         int64_t value;

         info->pResult = x_itemPutNI( info->pResult, xwc->cursorBlinkRate );
         if( s_gt_xwc_InfoInteger( info->pNewVal, INT_MIN, INT_MAX, &value ) )
         {
            xwc->cursorBlinkRate = ( int )X_MAX( value, 0 );
            xwc->cursorTime = s_gt_xwc_Time();
            xwc->cursorVisible = 1;
            xwc->dirty = 1;
            gt->pFuncTable->Flush( gt );
         }
         break;
      }
      case X_GTI_KBDSHIFTS:
         info->pResult = x_itemPutNI( info->pResult, s_gt_xwc_KeyboardState( xwc ) );
         if( info->pNewVal )
         {
            int64_t value;

            if( s_gt_xwc_InfoInteger( info->pNewVal, 0, 0x1FFFF, &value ) )
               s_gt_xwc_SetKeyboardState( xwc, ( int )value );
         }
         break;
      case X_GTI_MOUSESTATUS:
         info->pResult = x_itemPutL( info->pResult, xwc->mouseOn );
         if( info->pNewVal && X_IS_LOGICAL( info->pNewVal ) && x_itemGetL( info->pNewVal ) != xwc->mouseOn )
         {
            xwc->mouseOn = x_itemGetL( info->pNewVal );
            s_gt_xwc_ClearSelection( xwc );
            s_gt_xwc_ResetMouse( xwc );
            gt->pFuncTable->Flush( gt );
         }
         break;
      case X_GTI_MOUSEPOS_XY:
      {
         Window       root, child;
         int          rootX, rootY, x, y;
         unsigned int mask;
         int64_t      newX, newY;

         if( XQueryPointer( xwc->display, xwc->window, &root, &child, &rootX, &rootY, &x, &y, &mask ) )
            s_gt_xwc_UpdateMousePosition( xwc, x, y, 0 );
         s_gt_xwc_InfoPairResult( info, xwc->mouseX, xwc->mouseY );
         if( s_gt_xwc_InfoCoordinates( info, &newX, &newY ) )
         {
            int insideX = newX >= 0 && newX < xwc->canvasWidth && newX <= INT16_MAX;
            int insideY = newY >= 0 && newY < xwc->canvasHeight && newY <= INT16_MAX;

            if( insideX && insideY )
               s_gt_xwc_WarpPointer( xwc, ( int )newX, ( int )newY );
         }
         break;
      }
      case X_GTI_SELECTCOPY:
         info->pResult = x_itemPutL( info->pResult, xwc->selectCopy );
         if( info->pNewVal && X_IS_LOGICAL( info->pNewVal ) && x_itemGetL( info->pNewVal ) != xwc->selectCopy )
         {
            xwc->selectCopy = x_itemGetL( info->pNewVal );
            memset( xwc->clickPending, 0, sizeof( xwc->clickPending ) );
            if( !xwc->selectCopy )
               s_gt_xwc_ClearSelection( xwc );
            gt->pFuncTable->Flush( gt );
         }
         break;
      case X_GTI_CLIPBOARDDATA:
      case X_GTI_CLIPBOARDPASTE: return s_gt_xwc_ClipboardInfo( xwc, type, info, s_gt_xwc_PumpEvents );
      case X_GTI_ALTENTER:
         info->pResult = x_itemPutL( info->pResult, xwc->altEnter );
         if( info->pNewVal && X_IS_LOGICAL( info->pNewVal ) )
            xwc->altEnter = x_itemGetL( info->pNewVal );
         break;
      case X_GTI_ISFULLSCREEN:
      case X_GTI_MAXIMIZED:
      case X_GTI_WINDOWALWAYSONTOP:
      {
         Atom first = xwc->netWmAbove;
         Atom second = IIF( type == X_GTI_MAXIMIZED, xwc->netWmMaximizedHorz, None );

         if( type == X_GTI_ISFULLSCREEN )
            first = xwc->netWmFullscreen;
         else if( type == X_GTI_MAXIMIZED )
            first = xwc->netWmMaximizedVert;
         info->pResult = x_itemPutL( info->pResult, s_gt_xwc_WindowState( xwc, first, second ) );
         if( info->pNewVal && X_IS_LOGICAL( info->pNewVal ) )
            s_gt_xwc_SetWindowState( xwc, first, second, x_itemGetL( info->pNewVal ) );
         break;
      }
      case X_GTI_WINDOWOPACITY:
         info->pResult = x_itemPutND( info->pResult, s_gt_xwc_WindowOpacity( xwc ) );
         if( info->pNewVal && X_IS_NUMERIC( info->pNewVal ) )
            s_gt_xwc_SetWindowOpacity( xwc, x_itemGetND( info->pNewVal ) );
         break;
      case X_GTI_WINDOWBORDERED:
      {
         unsigned long hints[5];
         unsigned long extents[4];
         int           bordered;

         if( s_gt_xwc_FrameExtents( xwc, extents ) )
            bordered = extents[0] != 0 || extents[1] != 0 || extents[2] != 0 || extents[3] != 0;
         else
         {
            s_gt_xwc_GetWindowHints( xwc, hints );
            bordered = !( hints[0] & 2 ) || hints[2] != 0;
         }
         info->pResult = x_itemPutL( info->pResult, bordered );
         if( info->pNewVal && X_IS_LOGICAL( info->pNewVal ) )
            s_gt_xwc_SetWindowHints( xwc, x_itemGetL( info->pNewVal ) );
         break;
      }
      case X_GTI_RESIZABLE:
         info->pResult = x_itemPutL( info->pResult, xwc->resizable );
         if( info->pNewVal && X_IS_LOGICAL( info->pNewVal ) && x_itemGetL( info->pNewVal ) != xwc->resizable )
         {
            xwc->resizable = x_itemGetL( info->pNewVal );
            s_gt_xwc_UpdateSizeLimits( xwc );
            s_gt_xwc_SetWindowHints( xwc, -1 );
            gt->pFuncTable->Flush( gt );
         }
         break;
      case X_GTI_RESIZEFONT:
         info->pResult = x_itemPutL( info->pResult, xwc->resizeMode == X_GTI_RESIZEMODE_FONT );
         if( info->pNewVal && X_IS_LOGICAL( info->pNewVal ) )
         {
            int mode = IIF( x_itemGetL( info->pNewVal ), X_GTI_RESIZEMODE_FONT, X_GTI_RESIZEMODE_ROWS );

            if( mode != xwc->resizeMode )
            {
               xwc->resizeMode = mode;
               s_gt_xwc_UpdateSizeLimits( xwc );
               gt->pFuncTable->Flush( gt );
            }
         }
         break;
      case X_GTI_RESIZESTEP:
         info->pResult = x_itemPutL( info->pResult, xwc->resizeStep );
         if( info->pNewVal && X_IS_LOGICAL( info->pNewVal ) && x_itemGetL( info->pNewVal ) != xwc->resizeStep )
         {
            xwc->resizeStep = x_itemGetL( info->pNewVal );
            s_gt_xwc_UpdateSizeLimits( xwc );
            gt->pFuncTable->Flush( gt );
         }
         break;
      case X_GTI_CLOSABLE:
      case X_GTI_CLOSEMODE:
      {
         int64_t mode;

         if( type == X_GTI_CLOSABLE )
            info->pResult = x_itemPutL( info->pResult, xwc->closeMode < 2 );
         else
            info->pResult = x_itemPutNI( info->pResult, xwc->closeMode );
         if( type == X_GTI_CLOSABLE && info->pNewVal && X_IS_LOGICAL( info->pNewVal ) )
         {
            xwc->closeMode = IIF( x_itemGetL( info->pNewVal ), 1, 2 );
            s_gt_xwc_SetWindowHints( xwc, -1 );
         }
         else if( type == X_GTI_CLOSEMODE && s_gt_xwc_InfoInteger( info->pNewVal, 0, 2, &mode ) )
         {
            xwc->closeMode = ( int )mode;
            s_gt_xwc_SetWindowHints( xwc, -1 );
         }
         break;
      }
      case X_GTI_MINIMIZED:
         info->pResult = x_itemPutL( info->pResult, s_gt_xwc_WindowMinimized( xwc ) );
         if( info->pNewVal && X_IS_LOGICAL( info->pNewVal ) )
         {
            if( x_itemGetL( info->pNewVal ) )
               XIconifyWindow( xwc->display, xwc->window, xwc->screen );
            else
               XMapRaised( xwc->display, xwc->window );
            XFlush( xwc->display );
         }
         break;
      case X_GTI_WINTITLE:
         info->pResult = x_itemPutC( info->pResult, xwc->title );
         if( s_gt_xwc_IsText( info->pNewVal ) )
         {
            x_xfree( xwc->title );
            xwc->title = x_strdup( x_itemGetCPtr( info->pNewVal ) );
            s_gt_xwc_SetWindowTitle( xwc );
         }
         break;
      case X_GTI_ICONFILE:
      {
         int changed = 0;

         if( s_gt_xwc_IsText( info->pNewVal ) )
            changed = s_gt_xwc_SetIconFile( xwc, x_itemGetCPtr( info->pNewVal ) );
         info->pResult = x_itemPutL( info->pResult, changed );
         break;
      }
      case X_GTI_ICONRES: info->pResult = x_itemPutL( info->pResult, 0 ); break;
      case X_GTI_WINHANDLE: info->pResult = x_itemPutNInt( info->pResult, ( int64_t )xwc->window ); break;
      case X_GTI_SETPOS_XY:
      case X_GTI_SETPOS_ROWCOL:
      {
         int     x = xwc->windowX;
         int     y = xwc->windowY;
         int64_t first, second;

         s_gt_xwc_WindowPosition( xwc, &x, &y );
         if( type == X_GTI_SETPOS_XY )
            s_gt_xwc_InfoPairResult( info, x, y );
         else
            s_gt_xwc_InfoPairResult( info, y / xwc->cellHeight, x / xwc->cellWidth );
         if( s_gt_xwc_InfoCoordinates( info, &first, &second ) )
         {
            if( type == X_GTI_SETPOS_ROWCOL )
            {
               if( first < INT16_MIN / xwc->cellHeight || first > INT16_MAX / xwc->cellHeight )
                  break;
               if( second < INT16_MIN / xwc->cellWidth || second > INT16_MAX / xwc->cellWidth )
                  break;
               y = ( int )( first * xwc->cellHeight );
               x = ( int )( second * xwc->cellWidth );
            }
            else
            {
               if( first < INT16_MIN || first > INT16_MAX || second < INT16_MIN || second > INT16_MAX )
                  break;
               x = ( int )first;
               y = ( int )second;
            }
            XMoveWindow( xwc->display, xwc->window, x, y );
            XFlush( xwc->display );
         }
         break;
      }
      case X_GTI_PALETTE:
      {
         int     index;
         int64_t value;
         int     changed = 0;

         if( info->pNewVal && X_IS_NUMERIC( info->pNewVal ) )
         {
            if( !s_gt_xwc_InfoInteger( info->pNewVal, 0, 15, &value ) )
               return 0;

            index = ( int )value;
            info->pResult = x_itemPutNI( info->pResult, s_gt_xwc_PaletteValue( xwc->colors[index] ) );
            if( s_gt_xwc_InfoInteger( info->pNewVal2, 0, 0xFFFFFF, &value ) )
               changed = s_gt_xwc_SetPaletteValue( xwc, index, value );
         }
         else
         {
            if( !info->pResult )
               info->pResult = x_itemNew( NULL );
            x_arrayNew( info->pResult, 16 );
            for( index = 0; index < 16; ++index )
               x_arraySetNI( info->pResult, index + 1, s_gt_xwc_PaletteValue( xwc->colors[index] ) );
            if( info->pNewVal && X_IS_ARRAY( info->pNewVal ) && x_arrayLen( info->pNewVal ) == 16 )
            {
               int64_t values[16];

               for( index = 0; index < 16; ++index )
               {
                  X_ITEM *item = x_arrayGetItemPtr( info->pNewVal, index + 1 );

                  if( !s_gt_xwc_InfoInteger( item, 0, 0xFFFFFF, &values[index] ) )
                     return 0;
               }
               for( index = 0; index < 16; ++index )
                  changed |= s_gt_xwc_SetPaletteValue( xwc, index, values[index] );
            }
         }
         if( changed )
         {
            X_GTSELF_EXPOSEAREA( gt, 0, 0, xwc->rows - 1, xwc->cols - 1 );
            xwc->dirty = 1;
            gt->pFuncTable->Flush( gt );
         }
         break;
      }
      case X_GTI_DISPIMAGE:
         info->pResult = x_itemPutL( info->pResult, s_gt_xwc_DispImage( xwc, info->pNewVal, info->pNewVal2 ) );
         break;
      default: return s_super.Info( gt, type, info );
   }

   return 1;
}

/* ---------------------------------------------------------------------------------------------
   Graphics on the same 0xRRGGBB canvas as the text cells
   --------------------------------------------------------------------------------------------- */

static int s_gt_xwc_GetGfxClip( X_GTXWC *xwc, X_GTXWC_RECT *clip )
{
   if( xwc->clipOn )
   {
      clip->top = X_MAX( xwc->clipTop, 0 );
      clip->left = X_MAX( xwc->clipLeft, 0 );
      clip->bottom = X_MIN( xwc->clipBottom, xwc->canvasHeight - 1 );
      clip->right = X_MIN( xwc->clipRight, xwc->canvasWidth - 1 );
   }
   else
   {
      clip->top = clip->left = 0;
      clip->bottom = xwc->canvasHeight - 1;
      clip->right = xwc->canvasWidth - 1;
   }

   return clip->top <= clip->bottom && clip->left <= clip->right;
}

static void s_gt_xwc_GfxPlot( X_GTXWC *xwc, const X_GTXWC_RECT *clip, int64_t x, int64_t y, uint32_t color )
{
   if( x >= clip->left && x <= clip->right && y >= clip->top && y <= clip->bottom )
      s_gt_xwc_GfxPixel( xwc, ( int )x, ( int )y, color );
}

static void s_gt_xwc_GfxLine( X_GTXWC *xwc, const X_GTXWC_RECT *clip, int64_t top, int64_t left, int64_t bottom,
                              int64_t right, uint32_t color )
{
   int64_t dx = llabs( right - left );
   int64_t dy = llabs( bottom - top );
   int64_t first, last, position;
   int     direction;

   if( dx == 0 && dy == 0 )
   {
      s_gt_xwc_GfxPlot( xwc, clip, left, top, color );
      return;
   }
   if( dx >= dy )
   {
      if( left > right )
      {
         int64_t temporary = left;

         left = right;
         right = temporary;
         temporary = top;
         top = bottom;
         bottom = temporary;
      }
      direction = IIF( bottom >= top, 1, -1 );
      first = X_MAX( left, clip->left );
      last = X_MIN( right, clip->right );
      for( position = first; position <= last; ++position )
      {
         int64_t offset = ( int64_t )( ( ( __int128 )( position - left ) * dy + dx / 2 ) / dx );

         s_gt_xwc_GfxPlot( xwc, clip, position, top + direction * offset, color );
      }
   }
   else
   {
      if( top > bottom )
      {
         int64_t temporary = top;

         top = bottom;
         bottom = temporary;
         temporary = left;
         left = right;
         right = temporary;
      }
      direction = IIF( right >= left, 1, -1 );
      first = X_MAX( top, clip->top );
      last = X_MIN( bottom, clip->bottom );
      for( position = first; position <= last; ++position )
      {
         int64_t offset = ( int64_t )( ( ( __int128 )( position - top ) * dx + dy / 2 ) / dy );

         s_gt_xwc_GfxPlot( xwc, clip, left + direction * offset, position, color );
      }
   }
}

static int64_t s_gt_xwc_EllipseOffset( int64_t position, int center, int radius, int otherRadius )
{
   int64_t     distance = llabs( position - center );
   long double span = sqrtl( ( long double )( radius - distance ) * ( radius + distance ) );

   return ( int64_t )floorl( span * otherRadius / radius + 0.5L );
}

static void s_gt_xwc_GfxEllipse( X_GTXWC *xwc, const X_GTXWC_RECT *clip, int centerY, int centerX, int radiusY,
                                 int radiusX, int filled, uint32_t color )
{
   int64_t top = ( int64_t )centerY - radiusY;
   int64_t bottom = ( int64_t )centerY + radiusY;
   int64_t left = ( int64_t )centerX - radiusX;
   int64_t right = ( int64_t )centerX + radiusX;
   int64_t start, end, position;

   if( radiusY < 0 || radiusX < 0 )
      return;

   if( radiusY == 0 )
   {
      s_gt_xwc_GfxLine( xwc, clip, centerY, left, centerY, right, color );
      return;
   }
   if( radiusX == 0 )
   {
      s_gt_xwc_GfxLine( xwc, clip, top, centerX, bottom, centerX, color );
      return;
   }

   start = X_MAX( ( int64_t )clip->top, top );
   end = X_MIN( ( int64_t )clip->bottom, bottom );
   for( position = start; position <= end; ++position )
   {
      int64_t offset = s_gt_xwc_EllipseOffset( position, centerY, radiusY, radiusX );
      int64_t lineLeft = ( int64_t )centerX - offset;
      int64_t lineRight = ( int64_t )centerX + offset;

      if( filled )
         s_gt_xwc_GfxLine( xwc, clip, position, lineLeft, position, lineRight, color );
      else
      {
         s_gt_xwc_GfxPlot( xwc, clip, lineLeft, position, color );
         if( offset != 0 )
            s_gt_xwc_GfxPlot( xwc, clip, lineRight, position, color );
      }
   }
   if( !filled )
   {
      start = X_MAX( ( int64_t )clip->left, left );
      end = X_MIN( ( int64_t )clip->right, right );
      for( position = start; position <= end; ++position )
      {
         int64_t offset = s_gt_xwc_EllipseOffset( position, centerX, radiusX, radiusY );
         int64_t rowOffset = s_gt_xwc_EllipseOffset( ( int64_t )centerY - offset, centerY, radiusY, radiusX );

         if( llabs( position - centerX ) != rowOffset )
         {
            s_gt_xwc_GfxPlot( xwc, clip, position, ( int64_t )centerY - offset, color );
            if( offset != 0 )
               s_gt_xwc_GfxPlot( xwc, clip, position, ( int64_t )centerY + offset, color );
         }
      }
   }
}

static int s_gt_xwc_gfx_Primitive( X_GT_BASE *gt, int type, int top, int left, int bottom, int right, int color )
{
   X_GTXWC     *xwc = X_GTXWC_GET( gt );
   X_GTXWC_RECT clip;
   int          result = 0;

   switch( type )
   {
      case GFX_ACQUIRESCREEN:
         ++xwc->gfxDepth;
         X_GTSELF_DISPBEGIN( gt );
         return 1;
      case GFX_RELEASESCREEN:
         if( xwc->gfxDepth == 0 )
            return 0;

         --xwc->gfxDepth;
         X_GTSELF_DISPEND( gt );
         return 1;
      case GFX_MAKECOLOR:
      {
         int red = X_MIN( X_MAX( top, 0 ), 255 );
         int green = X_MIN( X_MAX( left, 0 ), 255 );
         int blue = X_MIN( X_MAX( bottom, 0 ), 255 );

         return ( red << 16 ) | ( green << 8 ) | blue;
      }
      case GFX_CLIPTOP: return xwc->clipTop;
      case GFX_CLIPLEFT: return xwc->clipLeft;
      case GFX_CLIPBOTTOM: return xwc->clipBottom;
      case GFX_CLIPRIGHT: return xwc->clipRight;
      case GFX_SETCLIP:
         if( top == 0 && left == 0 && bottom == 0 && right == 0 )
         {
            xwc->clipTop = xwc->clipLeft = xwc->clipBottom = xwc->clipRight = 0;
            xwc->clipOn = 0;
         }
         else
         {
            xwc->clipTop = X_MIN( top, bottom );
            xwc->clipLeft = X_MIN( left, right );
            xwc->clipBottom = X_MAX( top, bottom );
            xwc->clipRight = X_MAX( left, right );
            xwc->clipOn = 1;
         }
         return 1;
      case GFX_DRAWINGMODE:
         if( top == GFX_MODE_SOLID || top == GFX_MODE_XOR )
            xwc->gfxMode = top;
         return xwc->gfxMode;
      case GFX_GETPIXEL:
         s_super.Refresh( gt );
         if( top >= 0 && left >= 0 && top < xwc->canvasHeight && left < xwc->canvasWidth )
            return ( int )( xwc->canvas[( size_t )top * xwc->canvasWidth + left] & 0xFFFFFF );

         return 0;
      default: break;
   }

   if( type < GFX_PUTPIXEL || type > GFX_FLOODFILL )
      return s_super.GfxPrimitive( gt, type, top, left, bottom, right, color );

   if( !s_gt_xwc_GetGfxClip( xwc, &clip ) )
      return 1;

   s_super.Refresh( gt );
   if( type == GFX_PUTPIXEL || type == GFX_FLOODFILL )
      color = bottom;
   else if( type == GFX_CIRCLE || type == GFX_FILLEDCIRCLE )
      color = right;
   color &= 0xFFFFFF;

   switch( type )
   {
      case GFX_PUTPIXEL: s_gt_xwc_GfxPlot( xwc, &clip, left, top, ( uint32_t )color ); break;
      case GFX_LINE: s_gt_xwc_GfxLine( xwc, &clip, top, left, bottom, right, ( uint32_t )color ); break;
      case GFX_RECT:
      {
         int64_t firstY = X_MIN( top, bottom );
         int64_t lastY = X_MAX( top, bottom );
         int64_t firstX = X_MIN( left, right );
         int64_t lastX = X_MAX( left, right );

         s_gt_xwc_GfxLine( xwc, &clip, firstY, firstX, firstY, lastX, ( uint32_t )color );
         if( lastY != firstY )
            s_gt_xwc_GfxLine( xwc, &clip, lastY, firstX, lastY, lastX, ( uint32_t )color );
         if( lastY - firstY > 1 )
         {
            s_gt_xwc_GfxLine( xwc, &clip, firstY + 1, firstX, lastY - 1, firstX, ( uint32_t )color );
            if( lastX != firstX )
               s_gt_xwc_GfxLine( xwc, &clip, firstY + 1, lastX, lastY - 1, lastX, ( uint32_t )color );
         }
         break;
      }
      case GFX_FILLEDRECT:
      {
         int y;
         int firstY = X_MAX( X_MIN( top, bottom ), clip.top );
         int lastY = X_MIN( X_MAX( top, bottom ), clip.bottom );
         int firstX = X_MAX( X_MIN( left, right ), clip.left );
         int lastX = X_MIN( X_MAX( left, right ), clip.right );

         for( y = firstY; y <= lastY; ++y )
         {
            int x;

            for( x = firstX; x <= lastX; ++x )
               s_gt_xwc_GfxPixel( xwc, x, y, ( uint32_t )color );
         }
         break;
      }
      case GFX_CIRCLE: s_gt_xwc_GfxEllipse( xwc, &clip, top, left, bottom, bottom, 0, ( uint32_t )color ); break;
      case GFX_FILLEDCIRCLE: s_gt_xwc_GfxEllipse( xwc, &clip, top, left, bottom, bottom, 1, ( uint32_t )color ); break;
      case GFX_ELLIPSE: s_gt_xwc_GfxEllipse( xwc, &clip, top, left, bottom, right, 0, ( uint32_t )color ); break;
      case GFX_FILLEDELLIPSE: s_gt_xwc_GfxEllipse( xwc, &clip, top, left, bottom, right, 1, ( uint32_t )color ); break;
      case GFX_FLOODFILL:
         if( left >= clip.left && left <= clip.right && top >= clip.top && top <= clip.bottom )
         {
            uint32_t source = xwc->canvas[( size_t )top * xwc->canvasWidth + left];
            uint32_t target = IIF( xwc->gfxMode == GFX_MODE_XOR, source ^ ( uint32_t )color, ( uint32_t )color );

            if( source != target )
            {
               size_t  used = 0;
               size_t  count = ( size_t )xwc->canvasWidth * xwc->canvasHeight;
               size_t *stack = ( size_t * )x_xgrabArray( count, sizeof( size_t ) );

               xwc->canvas[( size_t )top * xwc->canvasWidth + left] = target;
               stack[used++] = ( size_t )top * xwc->canvasWidth + left;
               while( used )
               {
                  size_t index = stack[--used];
                  int    x = ( int )( index % xwc->canvasWidth );
                  int    y = ( int )( index / xwc->canvasWidth );
                  int    direction;

                  for( direction = 0; direction < 4; ++direction )
                  {
                     int nextX = x + s_floodDX[direction];
                     int nextY = y + s_floodDY[direction];

                     if( nextX >= clip.left && nextX <= clip.right && nextY >= clip.top && nextY <= clip.bottom )
                     {
                        size_t next = ( size_t )nextY * xwc->canvasWidth + nextX;

                        if( xwc->canvas[next] == source )
                        {
                           xwc->canvas[next] = target;
                           stack[used++] = next;
                        }
                     }
                  }
               }
               x_xfree( stack );
            }
         }
         break;
      default: break;
   }

   result = 1;
   xwc->dirty = 1;
   if( xwc->gfxDepth == 0 )
      s_gt_xwc_Present( xwc );

   return result;
}

static void s_gt_xwc_gfx_Text( X_GT_BASE *gt, int top, int left, const char *text, int color, int size, int width )
{
   X_GTXWC     *xwc = X_GTXWC_GET( gt );
   X_GTXWC_RECT clip;
   uint16_t     stackBuffer[256];
   uint16_t    *characters;
   size_t       sourceLength = strlen( text );
   size_t       length;
   size_t       index;
   int64_t      position = left;
   uint32_t     glyphColor = ( uint32_t )color & 0xFFFFFF;

   size = IIF( size == 0, xwc->cellHeight, size );
   width = IIF( width == 0, X_MAX( size / 2, 1 ), width );
   if( size <= 0 || width <= 0 || !s_gt_xwc_GetGfxClip( xwc, &clip ) )
      return;

   if( ( int64_t )top + size <= clip.top || top > clip.bottom || left > clip.right )
      return;

   characters = stackBuffer;
   if( sourceLength >= 256 )
      characters = ( uint16_t * )x_xgrabArray( sourceLength + 1, sizeof( uint16_t ) );
   length = x_utf8ToU16( text, sourceLength, characters, sourceLength + 1 );
   s_super.Refresh( gt );

   for( index = 0; index < length && position <= clip.right; ++index, position += width )
   {
      if( characters[index] == '\r' || characters[index] == '\n' )
         break;
      if( position + width > clip.left )
         s_gt_xwc_DrawGlyph( xwc, characters[index], top, ( int )position, size, width, glyphColor, &clip, 1 );
   }

   if( characters != stackBuffer )
      x_xfree( characters );
   xwc->dirty = 1;
   if( xwc->gfxDepth == 0 )
      s_gt_xwc_Present( xwc );
}

/* ---------------------------------------------------------------------------------------------
   Registration
   --------------------------------------------------------------------------------------------- */

static int s_gt_xwc_FuncInit( X_GT_FUNCS *funcs )
{
   funcs->Init = s_gt_xwc_Init;
   funcs->Exit = s_gt_xwc_Exit;
   funcs->SetMode = s_gt_xwc_SetMode;
   funcs->Redraw = s_gt_xwc_Redraw;
   funcs->Refresh = s_gt_xwc_Refresh;
   funcs->DispBegin = s_gt_xwc_DispBegin;
   funcs->DispEnd = s_gt_xwc_DispEnd;
   funcs->Version = s_gt_xwc_Version;
   funcs->Info = s_gt_xwc_Info;
   funcs->Alert = s_gt_xwc_Alert;
   funcs->ReadKey = s_gt_xwc_ReadKey;
   funcs->InkeyGet = s_gt_xwc_InkeyGet;
   funcs->TrueColor = s_gt_xwc_TrueColor;

   funcs->MouseIsPresent = s_gt_xwc_mouse_IsPresent;
   funcs->MouseGetPos = s_gt_xwc_mouse_GetPos;
   funcs->MouseSetPos = s_gt_xwc_mouse_SetPos;
   funcs->MouseButtonState = s_gt_xwc_mouse_ButtonState;
   funcs->MouseCountButton = s_gt_xwc_mouse_CountButton;

   funcs->GfxPrimitive = s_gt_xwc_gfx_Primitive;
   funcs->GfxText = s_gt_xwc_gfx_Text;

   return 1;
}

static const X_GT_INIT s_gtInit = { "XWC", s_gt_xwc_FuncInit, &s_super, &s_gtId };

static void __attribute__( ( constructor ) ) s_gt_xwc_Register( void )
{
   x_gtRegister( &s_gtInit );
}

/* Linker anchor used by REQUEST GTXWC. */
X_FUNC( GTXWC )
{
   UNUSED( ctx );
}
