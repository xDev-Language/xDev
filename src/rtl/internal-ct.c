/*
 * Copyright (c) 2024-2026 xDev-Language
 *
 * Library: xDev Runtime
 * File: internal-ct.c
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "gtinfo.h"
#include "terminal.h"
#include "../../include/xdev/inkey.ch"
#include "../../include/xdev/setcurs.ch"

#include <string.h>

/* ---------------------------------------------------------------------------------------------
   CT windows

   Every window is an X_GT_BASE of its own, as large as its formatted area. WSelect() makes it
   the current GT, so the core output functions (DispOutAt(), SetPos(), MaxRow(), Scroll(), QOut(),
   SaveScreen(), SetColor()...) work in window coordinates without a change. Window 0 holds the
   screen contents while windows are open; the real GT then only shows the windows composed by
   the window map. Keyboard, mouse and GtInfo() go to the real GT.
   --------------------------------------------------------------------------------------------- */

enum
{
   X_CT_MAXWINDOW = 255,
   X_CT_SHADOWROWS = 1,
   X_CT_SHADOWCOLS = 2
};

/* One window */
typedef struct
{
   X_GT_BASE *gt;    /* formatted area: the output of the window goes here */
   X_GT_CELL *image; /* the whole window; holds the cells outside the formatted area */
   int handle;
   int top;          /* screen position, also outside the screen */
   int left;
   int height;
   int width;
   int marginTop;
   int marginLeft;
   int marginBottom;
   int marginRight;
   int shadow;       /* shadow color, -1 without a shadow */
} X_CT_WINDOW;

/* The driver of the windows */
static X_GT_FUNCS s_ctSuper;
static int s_ctGtId;
static int s_ctRegistered;

/* The real GT while windows are open, otherwise NULL */
static X_GT_BASE *s_ctScreen;

/* The windows, [0] is the screen; the open windows from the bottom to the top */
static X_CT_WINDOW *s_ctWindows[X_CT_MAXWINDOW + 1];
static int s_ctStack[X_CT_MAXWINDOW];
static int s_ctStackCount;
static int s_ctCurrent;

/* The window map: the window visible in each screen cell and the window whose shadow lies on it (0 none) */
static int *s_ctOwner;
static int *s_ctShade;

/* WBoard() */
static int s_ctBoardSet;
static int s_ctBoardTop;
static int s_ctBoardLeft;
static int s_ctBoardBottom;
static int s_ctBoardRight;

/* WMode() */
static int s_ctModeTop;
static int s_ctModeLeft;
static int s_ctModeBottom;
static int s_ctModeRight;

/* WSetShadow() and WSetMove() */
static int s_ctShadowColor = -1;
static int s_ctMoveMode = 1;

/* WStep(): the step of the interactive moving */
static int s_ctStepRows = 2;
static int s_ctStepCols = 5;

/* Interactive moving: the last arrow key and the axes moved by 1 after two opposite keys */
static int s_ctMoveLast;
static int s_ctFineRows;
static int s_ctFineCols;

/* The window of a GT */
static X_CT_WINDOW *s_ctWindowOf( X_GT_BASE *gt )
{
   return ( X_CT_WINDOW * )gt->driverData[s_ctGtId];
}

/* The area allowed for windows: WBoard() or the whole screen */
static void s_ctBoard( int *top, int *left, int *bottom, int *right )
{
   X_GT_BASE *screen = IIF( s_ctScreen, s_ctScreen, x_gtCurrent() );

   if( s_ctBoardSet )
   {
      *top = s_ctBoardTop;
      *left = s_ctBoardLeft;
      *bottom = s_ctBoardBottom;
      *right = s_ctBoardRight;
   }
   else
   {
      *top = 0;
      *left = 0;
      *bottom = screen->rows - 1;
      *right = screen->cols - 1;
   }
}

/* ---------------------------------------------------------------------------------------------
   Composing the screen
   --------------------------------------------------------------------------------------------- */

/* One screen cell from the window that owns it, in the shadow color under a shadow */
static void s_ctComposeCell( int row, int col )
{
   size_t index = ( size_t )row * s_ctScreen->cols + col;
   X_CT_WINDOW *window = s_ctWindows[s_ctOwner[index]];
   int windowRow = row - window->top;
   int windowCol = col - window->left;
   int formatRow = windowRow - window->marginTop;
   int formatCol = windowCol - window->marginLeft;
   int inFormat = formatRow >= 0 && formatCol >= 0 && formatRow < window->gt->rows && formatCol < window->gt->cols;
   X_GT_CELL cell;

   if( inFormat )
      cell = window->gt->screenBuffer[( size_t )formatRow * window->gt->cols + formatCol];
   else
      cell = window->image[( size_t )windowRow * window->width + windowCol];

   if( s_ctShade[index] )
   {
      cell.color = ( uint8_t )s_ctWindows[s_ctShade[index]]->shadow;
      cell.foreground = X_GT_RGB_DEFAULT;
      cell.background = X_GT_RGB_DEFAULT;
   }

   s_ctScreen->screenBuffer[index] = cell;
   s_ctScreen->dirtyLines[row] = 1;
   s_ctScreen->screenActive = 1;
}

/* Mark a rectangle of the map, clipped to the board and the screen */
static void s_ctMark( int top, int left, int bottom, int right, int handle, int shadow )
{
   int boardTop, boardLeft, boardBottom, boardRight, row, col;
   size_t index;

   s_ctBoard( &boardTop, &boardLeft, &boardBottom, &boardRight );
   boardBottom = IIF( boardBottom < s_ctScreen->rows, boardBottom, s_ctScreen->rows - 1 );
   boardRight = IIF( boardRight < s_ctScreen->cols, boardRight, s_ctScreen->cols - 1 );
   top = IIF( top > boardTop, top, boardTop );
   left = IIF( left > boardLeft, left, boardLeft );
   bottom = IIF( bottom < boardBottom, bottom, boardBottom );
   right = IIF( right < boardRight, right, boardRight );

   for( row = top; row <= bottom; ++row )
   {
      for( col = left; col <= right; ++col )
      {
         index = ( size_t )row * s_ctScreen->cols + col;
         if( shadow )
            s_ctShade[index] = handle;
         else
         {
            s_ctOwner[index] = handle;
            s_ctShade[index] = 0;
         }
      }
   }
}

/* The driver can change the screen size (window border, SetMode(), font); window 0 and the map
   follow it. Returns whether the size changed. */
static int s_ctFit( void )
{
   X_CT_WINDOW *window = s_ctWindows[0];
   size_t count = ( size_t )s_ctScreen->rows * s_ctScreen->cols;

   if( window->height == s_ctScreen->rows && window->width == s_ctScreen->cols )
      return 0;

   window->height = s_ctScreen->rows;
   window->width = s_ctScreen->cols;
   window->gt->pFuncTable->Resize( window->gt, window->height, window->width );

   x_xfree( s_ctOwner );
   x_xfree( s_ctShade );
   s_ctOwner = ( int * )x_xgrabArray( count, sizeof( int ) );
   s_ctShade = ( int * )x_xgrabArray( count, sizeof( int ) );

   return 1;
}

/* Rebuild the map after the window layout changed and compose the whole screen */
static void s_ctRemap( void )
{
   size_t count, index;
   int i, bottom, right;
   X_CT_WINDOW *window;

   s_ctFit();
   count = ( size_t )s_ctScreen->rows * s_ctScreen->cols;
   memset( s_ctOwner, 0, count * sizeof( int ) );
   memset( s_ctShade, 0, count * sizeof( int ) );

   for( i = 0; i < s_ctStackCount; ++i )
   {
      window = s_ctWindows[s_ctStack[i]];
      bottom = window->top + window->height - 1;
      right = window->left + window->width - 1;
      s_ctMark( window->top, window->left, bottom, right, window->handle, 0 );
      if( window->shadow >= 0 )
      {
         s_ctMark( window->top + 1, right + 1, bottom + X_CT_SHADOWROWS, right + X_CT_SHADOWCOLS, window->handle, 1 );
         s_ctMark( bottom + 1, window->left + X_CT_SHADOWCOLS, bottom + X_CT_SHADOWROWS, right, window->handle, 1 );
      }
   }

   for( index = 0; index < count; ++index )
      s_ctComposeCell( ( int )( index / s_ctScreen->cols ), ( int )( index % s_ctScreen->cols ) );
}

/* Is the cursor of the window inside its formatted area and on a screen cell the window owns? */
static int s_ctCursorVisible( const X_CT_WINDOW *window, int row, int col )
{
   const X_GT_BASE *gt = window->gt;

   if( gt->row < 0 || gt->col < 0 || gt->row >= gt->rows || gt->col >= gt->cols )
      return 0;

   if( row < 0 || col < 0 || row >= s_ctScreen->rows || col >= s_ctScreen->cols )
      return 0;

   return s_ctOwner[( size_t )row * s_ctScreen->cols + col] == window->handle;
}

/* The real cursor follows the cursor of the current window and hides under other windows */
static void s_ctCursor( void )
{
   X_CT_WINDOW *window = s_ctWindows[s_ctCurrent];
   X_GT_BASE *gt = window->gt;
   int row = window->top + window->marginTop + gt->row;
   int col = window->left + window->marginLeft + gt->col;
   int style = gt->cursorStyle;

   if( !s_ctCursorVisible( window, row, col ) )
      style = SC_NONE;
   else if( row != s_ctScreen->row || col != s_ctScreen->col )
      s_ctScreen->pFuncTable->SetPos( s_ctScreen, row, col );

   if( style != s_ctScreen->cursorStyle )
      s_ctScreen->pFuncTable->SetCursorStyle( s_ctScreen, style );
}

/* Show the composed screen */
static void s_ctShow( void )
{
   if( s_ctFit() )
      s_ctRemap();

   s_ctCursor();
   s_ctScreen->pFuncTable->Flush( s_ctScreen );
}

/* The screen size changed outside of the window functions */
static void s_ctFollow( void )
{
   if( s_ctFit() )
   {
      s_ctRemap();
      s_ctShow();
   }
}

/* Compose the output of the windows that is not on the screen yet */
static void s_ctCompose( X_GT_BASE *gt )
{
   int handle;

   if( s_ctFit() )
      s_ctRemap();
   else if( gt )
      s_ctSuper.RedrawDiff( gt );
   else
   {
      for( handle = 0; handle <= X_CT_MAXWINDOW; ++handle )
      {
         if( s_ctWindows[handle] )
            s_ctSuper.RedrawDiff( s_ctWindows[handle]->gt );
      }
   }
}

/* ---------------------------------------------------------------------------------------------
   GT of a window: output stays in the window, the rest goes to the real GT
   --------------------------------------------------------------------------------------------- */

/* Called by RedrawDiff() of the core with a changed part of a formatted area row */
static void s_ctRedraw( X_GT_BASE *gt, int row, int col, int count )
{
   X_CT_WINDOW *window = s_ctWindowOf( gt );
   int screenRow, screenCol;
   size_t index;

   if( !window )
      return;

   screenRow = window->top + window->marginTop + row;
   if( screenRow < 0 || screenRow >= s_ctScreen->rows )
      return;

   for( screenCol = window->left + window->marginLeft + col; count > 0; --count, ++screenCol )
   {
      if( screenCol < 0 || screenCol >= s_ctScreen->cols )
         continue;

      index = ( size_t )screenRow * s_ctScreen->cols + screenCol;
      if( s_ctOwner[index] == window->handle )
         s_ctComposeCell( screenRow, screenCol );
   }
}

/* A closed window can still be held by x_inkey(); its last Flush() has nothing to show */
static void s_ctFlush( X_GT_BASE *gt )
{
   if( !s_ctWindowOf( gt ) )
      return;

   s_ctCompose( gt );
   s_ctShow();
}

static void s_ctDispBegin( X_GT_BASE *gt )
{
   UNUSED( gt );
   s_ctScreen->pFuncTable->DispBegin( s_ctScreen );
}

static void s_ctDispEnd( X_GT_BASE *gt )
{
   UNUSED( gt );
   s_ctCompose( NULL );
   s_ctCursor();
   s_ctScreen->pFuncTable->DispEnd( s_ctScreen );
}

static int s_ctDispCount( X_GT_BASE *gt )
{
   UNUSED( gt );
   return s_ctScreen->pFuncTable->DispCount( s_ctScreen );
}

static int s_ctSetMode( X_GT_BASE *gt, int rows, int cols )
{
   int result = s_ctScreen->pFuncTable->SetMode( s_ctScreen, rows, cols );

   UNUSED( gt );
   s_ctFollow();

   return result;
}

static int s_ctInfo( X_GT_BASE *gt, int type, X_GT_INFO *info )
{
   int result = s_ctScreen->pFuncTable->Info( s_ctScreen, type, info );

   UNUSED( gt );
   s_ctFollow();

   return result;
}

static const char *s_ctVersion( X_GT_BASE *gt, int type )
{
   UNUSED( gt );
   return s_ctScreen->pFuncTable->Version( s_ctScreen, type );
}

static int s_ctSuspend( X_GT_BASE *gt )
{
   UNUSED( gt );
   return s_ctScreen->pFuncTable->Suspend( s_ctScreen );
}

static int s_ctResume( X_GT_BASE *gt )
{
   UNUSED( gt );
   return s_ctScreen->pFuncTable->Resume( s_ctScreen );
}

static void s_ctBell( X_GT_BASE *gt )
{
   UNUSED( gt );
   s_ctScreen->pFuncTable->Bell( s_ctScreen );
}

static int s_ctAlert( X_GT_BASE *gt, X_ITEM *message, X_ITEM *options, int colorNormal, int colorHigh, double delay )
{
   UNUSED( gt );
   return s_ctScreen->pFuncTable->Alert( s_ctScreen, message, options, colorNormal, colorHigh, delay );
}

static int s_ctSetFlag( X_GT_BASE *gt, int type, int value )
{
   UNUSED( gt );
   return s_ctScreen->pFuncTable->SetFlag( s_ctScreen, type, value );
}

static int s_ctReadKey( X_GT_BASE *gt, int eventMask )
{
   UNUSED( gt );
   return s_ctScreen->pFuncTable->ReadKey( s_ctScreen, eventMask );
}

static int s_ctInkeyGet( X_GT_BASE *gt, int wait, double seconds, int mask )
{
   UNUSED( gt );
   return s_ctScreen->pFuncTable->InkeyGet( s_ctScreen, wait, seconds, mask );
}

static void s_ctInkeyPut( X_GT_BASE *gt, int key )
{
   UNUSED( gt );
   s_ctScreen->pFuncTable->InkeyPut( s_ctScreen, key );
}

static void s_ctInkeyIns( X_GT_BASE *gt, int key )
{
   UNUSED( gt );
   s_ctScreen->pFuncTable->InkeyIns( s_ctScreen, key );
}

static int s_ctInkeyLast( X_GT_BASE *gt, int mask )
{
   UNUSED( gt );
   return s_ctScreen->pFuncTable->InkeyLast( s_ctScreen, mask );
}

static int s_ctInkeyNext( X_GT_BASE *gt, int mask )
{
   UNUSED( gt );
   return s_ctScreen->pFuncTable->InkeyNext( s_ctScreen, mask );
}

static void s_ctInkeyPoll( X_GT_BASE *gt )
{
   UNUSED( gt );
   s_ctScreen->pFuncTable->InkeyPoll( s_ctScreen );
}

static void s_ctInkeyReset( X_GT_BASE *gt )
{
   UNUSED( gt );
   s_ctScreen->pFuncTable->InkeyReset( s_ctScreen );
}

static void s_ctInkeySetText( X_GT_BASE *gt, const char *text, size_t length, int eol )
{
   UNUSED( gt );
   s_ctScreen->pFuncTable->InkeySetText( s_ctScreen, text, length, eol );
}

static int s_ctInkeySetLast( X_GT_BASE *gt, int key )
{
   UNUSED( gt );
   return s_ctScreen->pFuncTable->InkeySetLast( s_ctScreen, key );
}

static int s_ctMouseIsPresent( X_GT_BASE *gt )
{
   UNUSED( gt );
   return s_ctScreen->pFuncTable->MouseIsPresent( s_ctScreen );
}

/* The mouse position is relative to the formatted area of the window, like the output */
static void s_ctMouseGetPos( X_GT_BASE *gt, int *row, int *col )
{
   X_CT_WINDOW *window = s_ctWindowOf( gt );

   s_ctScreen->pFuncTable->MouseGetPos( s_ctScreen, row, col );
   *row -= window->top + window->marginTop;
   *col -= window->left + window->marginLeft;
}

static void s_ctMouseSetPos( X_GT_BASE *gt, int row, int col )
{
   X_CT_WINDOW *window = s_ctWindowOf( gt );
   int screenRow = row + window->top + window->marginTop;
   int screenCol = col + window->left + window->marginLeft;

   s_ctScreen->pFuncTable->MouseSetPos( s_ctScreen, screenRow, screenCol );
}

static int s_ctMouseGetDoubleClickSpeed( X_GT_BASE *gt )
{
   UNUSED( gt );
   return s_ctScreen->pFuncTable->MouseGetDoubleClickSpeed( s_ctScreen );
}

static int s_ctMouseCountButton( X_GT_BASE *gt )
{
   UNUSED( gt );
   return s_ctScreen->pFuncTable->MouseCountButton( s_ctScreen );
}

static int s_ctMouseButtonState( X_GT_BASE *gt, int button )
{
   UNUSED( gt );
   return s_ctScreen->pFuncTable->MouseButtonState( s_ctScreen, button );
}

static int s_ctGfxPrimitive( X_GT_BASE *gt, int type, int top, int left, int bottom, int right, int color )
{
   UNUSED( gt );
   return s_ctScreen->pFuncTable->GfxPrimitive( s_ctScreen, type, top, left, bottom, right, color );
}

static void s_ctGfxText( X_GT_BASE *gt, int top, int left, const char *text, int color, int size, int width )
{
   UNUSED( gt );
   s_ctScreen->pFuncTable->GfxText( s_ctScreen, top, left, text, color, size, width );
}

static int s_ctTrueColor( X_GT_BASE *gt )
{
   UNUSED( gt );
   return s_ctScreen->pFuncTable->TrueColor( s_ctScreen );
}

/* The functions of the window driver */
static int s_ctFuncInit( X_GT_FUNCS *funcs )
{
   funcs->SetMode = s_ctSetMode;
   funcs->Redraw = s_ctRedraw;
   funcs->Flush = s_ctFlush;
   funcs->DispBegin = s_ctDispBegin;
   funcs->DispEnd = s_ctDispEnd;
   funcs->DispCount = s_ctDispCount;
   funcs->Version = s_ctVersion;
   funcs->Suspend = s_ctSuspend;
   funcs->Resume = s_ctResume;
   funcs->Bell = s_ctBell;
   funcs->Info = s_ctInfo;
   funcs->Alert = s_ctAlert;
   funcs->SetFlag = s_ctSetFlag;
   funcs->ReadKey = s_ctReadKey;
   funcs->InkeyGet = s_ctInkeyGet;
   funcs->InkeyPut = s_ctInkeyPut;
   funcs->InkeyIns = s_ctInkeyIns;
   funcs->InkeyLast = s_ctInkeyLast;
   funcs->InkeyNext = s_ctInkeyNext;
   funcs->InkeyPoll = s_ctInkeyPoll;
   funcs->InkeyReset = s_ctInkeyReset;
   funcs->InkeySetText = s_ctInkeySetText;
   funcs->InkeySetLast = s_ctInkeySetLast;
   funcs->MouseIsPresent = s_ctMouseIsPresent;
   funcs->MouseGetPos = s_ctMouseGetPos;
   funcs->MouseSetPos = s_ctMouseSetPos;
   funcs->MouseGetDoubleClickSpeed = s_ctMouseGetDoubleClickSpeed;
   funcs->MouseCountButton = s_ctMouseCountButton;
   funcs->MouseButtonState = s_ctMouseButtonState;
   funcs->GfxPrimitive = s_ctGfxPrimitive;
   funcs->GfxText = s_ctGfxText;
   funcs->TrueColor = s_ctTrueColor;

   return 1;
}

static const X_GT_INIT s_ctInit = { "CTW", s_ctFuncInit, &s_ctSuper, &s_ctGtId };

/* ---------------------------------------------------------------------------------------------
   Windows
   --------------------------------------------------------------------------------------------- */

/* A new window takes the colors and the cursor shape of the window it is opened from */
static X_CT_WINDOW *s_ctWindowNew( int handle, int top, int left, int height, int width, const X_GT_BASE *from )
{
   X_CT_WINDOW *window = ( X_CT_WINDOW * )x_xgrabz( sizeof( X_CT_WINDOW ) );
   X_GT_BASE *gt = x_gtLoad( "CTW", NULL, NULL );

   gt->hStdin = from->hStdin;
   gt->hStdout = from->hStdout;
   gt->hStderr = from->hStderr;
   gt->cursorStyle = from->cursorStyle;
   gt->blink = from->blink;
   gt->colorCount = from->colorCount;
   gt->colorIndex = from->colorIndex;
   gt->foreground = from->foreground;
   gt->background = from->background;
   gt->redrawMax = from->redrawMax;
   memcpy( gt->colors, from->colors, sizeof( gt->colors ) );
   gt->pFuncTable->Resize( gt, height, width );
   gt->driverData[s_ctGtId] = window;

   window->gt = gt;
   window->handle = handle;
   window->top = top;
   window->left = left;
   window->height = height;
   window->width = width;
   window->shadow = -1;
   if( handle )
      window->image = ( X_GT_CELL * )x_xgrabArray( ( size_t )height * width, sizeof( X_GT_CELL ) );

   return window;
}

/* The GT of a window can still be held by x_inkey(); x_gtRelease() frees it with the last reference */
static void s_ctWindowFree( X_CT_WINDOW *window )
{
   window->gt->driverData[s_ctGtId] = NULL;
   x_gtRelease( window->gt );
   if( window->image )
      x_xfree( window->image );

   x_xfree( window );
}

/* Make a window the current GT */
static void s_ctSetCurrent( int handle )
{
   s_ctCurrent = handle;
   x_gtSwap( s_ctWindows[handle]->gt );
}

/* The first window: window 0 takes over the screen contents and becomes the current GT */
static int s_ctActivate( void )
{
   X_GT_BASE *screen = x_gtCurrent();
   size_t count = ( size_t )screen->rows * screen->cols;
   X_CT_WINDOW *window;

   if( !s_ctRegistered )
   {
      if( !x_gtRegister( &s_ctInit ) )
         return 0;

      s_ctRegistered = 1;
   }

   s_ctScreen = screen;
   window = s_ctWindowNew( 0, 0, 0, screen->rows, screen->cols, screen );
   memcpy( window->gt->screenBuffer, screen->screenBuffer, count * sizeof( X_GT_CELL ) );
   window->gt->row = screen->row;
   window->gt->col = screen->col;
   s_ctOwner = ( int * )x_xgrabzArray( count, sizeof( int ) );
   s_ctShade = ( int * )x_xgrabzArray( count, sizeof( int ) );
   s_ctWindows[0] = window;
   s_ctSetCurrent( 0 );

   return 1;
}

/* The last window was closed: the real GT takes over window 0 */
static void s_ctDeactivate( void )
{
   X_GT_BASE *gt = s_ctWindows[0]->gt;
   X_GT_BASE *screen = s_ctScreen;

   screen->colorCount = gt->colorCount;
   screen->colorIndex = gt->colorIndex;
   screen->foreground = gt->foreground;
   screen->background = gt->background;
   memcpy( screen->colors, gt->colors, sizeof( screen->colors ) );
   screen->pFuncTable->SetPos( screen, gt->row, gt->col );
   screen->pFuncTable->SetCursorStyle( screen, gt->cursorStyle );
   x_gtSwap( screen );

   s_ctWindowFree( s_ctWindows[0] );
   s_ctWindows[0] = NULL;
   x_xfree( s_ctOwner );
   x_xfree( s_ctShade );
   s_ctOwner = NULL;
   s_ctShade = NULL;
   s_ctScreen = NULL;
   s_ctCurrent = 0;

   screen->pFuncTable->Flush( screen );
}

/* The highest handle of an open window; 0 when none is open */
static int s_ctHighest( void )
{
   int handle = X_CT_MAXWINDOW;

   while( handle > 0 && !s_ctWindows[handle] )
      --handle;

   return handle;
}

/* Keep the cells outside the formatted area in the image and give the window new margins */
static void s_ctReformat( X_CT_WINDOW *window, int top, int left, int bottom, int right )
{
   X_GT_BASE *gt = window->gt;
   size_t rowSize;
   int row;

   rowSize = ( size_t )gt->cols * sizeof( X_GT_CELL );
   for( row = 0; row < gt->rows; ++row )
   {
      X_GT_CELL *image = window->image + ( size_t )( row + window->marginTop ) * window->width + window->marginLeft;

      memcpy( image, gt->screenBuffer + ( size_t )row * gt->cols, rowSize );
   }

   window->marginTop = top;
   window->marginLeft = left;
   window->marginBottom = bottom;
   window->marginRight = right;
   gt->pFuncTable->Resize( gt, window->height - top - bottom, window->width - left - right );

   rowSize = ( size_t )gt->cols * sizeof( X_GT_CELL );
   for( row = 0; row < gt->rows; ++row )
   {
      X_GT_CELL *image = window->image + ( size_t )( row + top ) * window->width + left;

      memcpy( gt->screenBuffer + ( size_t )row * gt->cols, image, rowSize );
   }

   gt->row = IIF( gt->row < gt->rows, gt->row, gt->rows - 1 );
   gt->col = IIF( gt->col < gt->cols, gt->col, gt->cols - 1 );
   s_ctRemap();
   s_ctShow();
}

/* The position allowed by WBoard() and WMode() */
static int s_ctClamp( int value, int size, int boardFirst, int boardLast, int overFirst, int overLast )
{
   int first = boardFirst - IIF( overFirst, size, 0 );
   int last = boardLast + 1 - IIF( overLast, 0, size );

   if( value > last )
      value = last;

   return IIF( value < first, first, value );
}

/* The position of a window in the middle of the board */
static void s_ctCentered( const X_CT_WINDOW *window, int *top, int *left )
{
   int boardTop, boardLeft, boardBottom, boardRight;
   int boardHeight, boardWidth;

   s_ctBoard( &boardTop, &boardLeft, &boardBottom, &boardRight );
   boardHeight = boardBottom - boardTop + 1;
   boardWidth = boardRight - boardLeft + 1;

   *top = boardTop + IIF( boardHeight > window->height, ( boardHeight - window->height ) / 2, 0 );
   *left = boardLeft + IIF( boardWidth > window->width, ( boardWidth - window->width ) / 2, 0 );
}

/* Move a window */
static void s_ctPlace( X_CT_WINDOW *window, int top, int left )
{
   if( top != window->top || left != window->left )
   {
      window->top = top;
      window->left = left;
      s_ctRemap();
      s_ctShow();
   }
}

/* ---------------------------------------------------------------------------------------------
   Functions used by xdev-api.c and terminal.c
   --------------------------------------------------------------------------------------------- */

/* The real GT for the current GT: input goes there while windows are open */
X_GT_BASE *x_ctScreen( X_GT_BASE *current )
{
   return IIF( s_ctScreen, s_ctScreen, current );
}

/* The real GT changed its size: window 0 follows it before GTI_RESIZEBLOCK is evaluated */
void x_ctScreenResize( X_GT_BASE *gt )
{
   if( s_ctScreen == gt )
      s_ctFollow();
}

/* Close all windows when the GT ends */
void x_ctExit( void )
{
   x_ctCloseAll();
}

/* WOpen(): a new window over the given area, cleared or with the screen contents under it; -1 on failure */
int x_ctOpen( int top, int left, int bottom, int right, int clear )
{
   int height = bottom - top + 1;
   int width = right - left + 1;
   int handle, row, col, screenRow, screenCol;
   X_CT_WINDOW *window;
   X_GT_BASE *from;
   X_GT_CELL cell;
   size_t index;

   if( height < 1 || width < 1 )
      return -1;

   for( handle = 1; handle <= X_CT_MAXWINDOW && s_ctWindows[handle]; ++handle )
      ;

   if( handle > X_CT_MAXWINDOW || ( !s_ctScreen && !s_ctActivate() ) )
      return -1;

   from = s_ctWindows[s_ctCurrent]->gt;
   window = s_ctWindowNew( handle, top, left, height, width, from );
   window->shadow = s_ctShadowColor;
   cell.character = ( uint16_t )x_clearChar;
   cell.color = ( uint8_t )from->colors[from->colorIndex];
   cell.attr = 0;
   cell.foreground = from->foreground;
   cell.background = from->background;
   clear = clear || window->shadow >= 0;

   for( row = 0; row < height; ++row )
   {
      for( col = 0; col < width; ++col )
      {
         index = ( size_t )row * width + col;
         screenRow = top + row;
         screenCol = left + col;
         window->image[index] = cell;
         if( clear || screenRow < 0 || screenCol < 0 )
            continue;

         if( screenRow < s_ctScreen->rows && screenCol < s_ctScreen->cols )
            window->image[index] = s_ctScreen->screenBuffer[( size_t )screenRow * s_ctScreen->cols + screenCol];
      }
   }

   memcpy( window->gt->screenBuffer, window->image, ( size_t )height * width * sizeof( X_GT_CELL ) );

   s_ctWindows[handle] = window;
   s_ctStack[s_ctStackCount++] = handle;
   s_ctSetCurrent( handle );
   s_ctRemap();
   s_ctShow();

   return handle;
}

/* Close the current window; the open window with the highest handle becomes current */
int x_ctClose( void )
{
   X_CT_WINDOW *window = s_ctWindows[s_ctCurrent];
   int i;

   if( !s_ctCurrent )
      return 0;

   for( i = 0; s_ctStack[i] != s_ctCurrent; ++i )
      ;

   --s_ctStackCount;
   memmove( &s_ctStack[i], &s_ctStack[i + 1], ( size_t )( s_ctStackCount - i ) * sizeof( int ) );
   s_ctWindows[s_ctCurrent] = NULL;
   s_ctSetCurrent( s_ctHighest() );
   s_ctWindowFree( window );

   s_ctRemap();
   if( s_ctStackCount )
      s_ctShow();
   else
      s_ctDeactivate();

   return s_ctCurrent;
}

/* Close all windows; -1 when none is open */
int x_ctCloseAll( void )
{
   int handle;

   if( !s_ctScreen )
      return -1;

   s_ctSetCurrent( 0 );
   for( handle = 1; handle <= X_CT_MAXWINDOW; ++handle )
   {
      if( s_ctWindows[handle] )
      {
         s_ctWindowFree( s_ctWindows[handle] );
         s_ctWindows[handle] = NULL;
      }
   }

   s_ctStackCount = 0;
   s_ctRemap();
   s_ctDeactivate();

   return 0;
}

/* The handle of the current window */
int x_ctCurrent( void )
{
   return s_ctCurrent;
}

/* Select a window and bring it to the top; window 0 stays under all windows */
int x_ctSelect( int handle )
{
   int i;

   if( handle < 0 || handle > X_CT_MAXWINDOW || ( handle && !s_ctWindows[handle] ) )
      return -1;

   if( !s_ctScreen )
      return 0;

   s_ctSetCurrent( handle );
   if( handle && s_ctStack[s_ctStackCount - 1] != handle )
   {
      for( i = 0; s_ctStack[i] != handle; ++i )
         ;

      memmove( &s_ctStack[i], &s_ctStack[i + 1], ( size_t )( s_ctStackCount - 1 - i ) * sizeof( int ) );
      s_ctStack[s_ctStackCount - 1] = handle;
      s_ctRemap();
   }

   s_ctShow();

   return handle;
}

/* The highest handle of an open window */
int x_ctMaxWindow( void )
{
   return s_ctHighest();
}

/* WMove(): move the current window, kept inside the board as WMode() allows */
int x_ctMove( int top, int left )
{
   X_CT_WINDOW *window = s_ctWindows[s_ctCurrent];
   int boardTop, boardLeft, boardBottom, boardRight;
   int newTop, newLeft;

   if( !s_ctCurrent )
      return 0;

   s_ctBoard( &boardTop, &boardLeft, &boardBottom, &boardRight );
   newTop = s_ctClamp( top, window->height, boardTop, boardBottom, s_ctModeTop, s_ctModeBottom );
   newLeft = s_ctClamp( left, window->width, boardLeft, boardRight, s_ctModeLeft, s_ctModeRight );
   s_ctPlace( window, newTop, newLeft );

   return s_ctCurrent;
}

/* WCenter(): center the current window, or only move it back inside the board */
int x_ctCenter( int center )
{
   X_CT_WINDOW *window = s_ctWindows[s_ctCurrent];
   int boardTop, boardLeft, boardBottom, boardRight, top, left;

   if( !s_ctCurrent )
      return 0;

   if( center )
      s_ctCentered( window, &top, &left );
   else
   {
      s_ctBoard( &boardTop, &boardLeft, &boardBottom, &boardRight );
      top = s_ctClamp( window->top, window->height, boardTop, boardBottom, 0, 0 );
      left = s_ctClamp( window->left, window->width, boardLeft, boardRight, 0, 0 );
   }

   s_ctPlace( window, top, left );

   return s_ctCurrent;
}

/* The coordinates of the current window on the screen; window 0 is the screen */
void x_ctCoords( int center, int *top, int *left, int *bottom, int *right )
{
   X_CT_WINDOW *window = s_ctWindows[s_ctCurrent];

   if( !s_ctCurrent )
   {
      *top = 0;
      *left = 0;
      *bottom = x_gtMaxRow();
      *right = x_gtMaxCol();
      return;
   }

   *top = window->top;
   *left = window->left;
   if( center )
      s_ctCentered( window, top, left );

   *bottom = *top + window->height - 1;
   *right = *left + window->width - 1;
}

/* The coordinates of the formatted area, or the margins with relative; window 0 gives the board */
void x_ctFormatCoords( int relative, int *top, int *left, int *bottom, int *right )
{
   X_CT_WINDOW *window = s_ctWindows[s_ctCurrent];

   if( !s_ctCurrent )
   {
      s_ctBoard( top, left, bottom, right );
      if( relative )
      {
         *bottom = x_gtMaxRow() - *bottom;
         *right = x_gtMaxCol() - *right;
      }
   }
   else if( relative )
   {
      *top = window->marginTop;
      *left = window->marginLeft;
      *bottom = window->marginBottom;
      *right = window->marginRight;
   }
   else
   {
      *top = window->top + window->marginTop;
      *left = window->left + window->marginLeft;
      *bottom = window->top + window->height - 1 - window->marginBottom;
      *right = window->left + window->width - 1 - window->marginRight;
   }
}

/* Change the margins of the current window by the given values; at least one cell stays */
int x_ctFormat( int top, int left, int bottom, int right )
{
   X_CT_WINDOW *window = s_ctWindows[s_ctCurrent];
   int64_t margins[4];
   int i;

   if( !s_ctCurrent )
      return 0;

   margins[0] = ( int64_t )window->marginTop + top;
   margins[1] = ( int64_t )window->marginLeft + left;
   margins[2] = ( int64_t )window->marginBottom + bottom;
   margins[3] = ( int64_t )window->marginRight + right;
   for( i = 0; i < 4; ++i )
      margins[i] = IIF( margins[i] < 0, 0, margins[i] );

   if( margins[0] + margins[2] >= window->height || margins[1] + margins[3] >= window->width )
      return -1;

   s_ctReformat( window, ( int )margins[0], ( int )margins[1], ( int )margins[2], ( int )margins[3] );

   return s_ctCurrent;
}

/* Frame the formatted area with a UTF-8 frame (8 characters, the 9th fills); the frame becomes margin */
int x_ctBox( const char *frame )
{
   X_GT_BASE *gt;
   unsigned short bottom, right;

   if( !s_ctCurrent )
   {
      x_gtBox( 0, 0, x_gtMaxRow(), x_gtMaxCol(), ( unsigned char * )frame );
      return 0;
   }

   gt = s_ctWindows[s_ctCurrent]->gt;
   if( gt->rows < 3 || gt->cols < 3 )
      return -1;

   bottom = ( unsigned short )( gt->rows - 1 );
   right = ( unsigned short )( gt->cols - 1 );
   x_gtBox( 0, 0, bottom, right, ( unsigned char * )frame );
   gt->row = 0;
   gt->col = 0;

   return x_ctFormat( 1, 1, 1, 1 );
}

/* Frame type 0 - 15 of WBox(): 0 - 3 and 8 - 11 fill the window with SetClearB() */
int x_ctBoxType( int type )
{
   static const uint16_t s_frames[8][8] = {
      { 0x2554, 0x2550, 0x2557, 0x2551, 0x255D, 0x2550, 0x255A, 0x2551 }, /* double */
      { 0x250C, 0x2500, 0x2510, 0x2502, 0x2518, 0x2500, 0x2514, 0x2502 }, /* single */
      { 0x2552, 0x2550, 0x2555, 0x2502, 0x255B, 0x2550, 0x2558, 0x2502 }, /* double top, single side */
      { 0x2553, 0x2500, 0x2556, 0x2551, 0x255C, 0x2500, 0x2559, 0x2551 }, /* single top, double side */
      { 0x2588, 0x2580, 0x2588, 0x2588, 0x2588, 0x2584, 0x2588, 0x2588 }, /* half top, full side */
      { 0x2590, 0x2580, 0x258C, 0x258C, 0x258C, 0x2584, 0x2590, 0x2590 }, /* half */
      { 0x2590, 0x2588, 0x258C, 0x258C, 0x258C, 0x2588, 0x2590, 0x2590 }, /* full top, half side */
      { 0x2588, 0x2588, 0x2588, 0x2588, 0x2588, 0x2588, 0x2588, 0x2588 }  /* full */
   };
   char frame[9 * 4 + 1];
   size_t length = 0;
   int kind = IIF( type < 0 || type > 15, 0, type );
   const uint16_t *chars = s_frames[( kind & 3 ) + IIF( kind & 8, 4, 0 )];
   int i;

   for( i = 0; i < 8; ++i )
      length += x_utf8Encode( frame + length, chars[i] );

   if( !( kind & 4 ) )
      length += x_utf8Encode( frame + length, ( uint32_t )x_clearChar );

   frame[length] = '\0';

   return x_ctBox( frame );
}

/* The area allowed for windows; set only while no window is open */
int x_ctBoard( int set, int top, int left, int bottom, int right )
{
   if( s_ctScreen || ( set && ( top < 0 || left < 0 || top > bottom || left > right ) ) )
      return -1;

   s_ctBoardSet = set;
   s_ctBoardTop = top;
   s_ctBoardLeft = left;
   s_ctBoardBottom = bottom;
   s_ctBoardRight = right;

   return 0;
}

/* Which board borders windows may cross; -1 keeps a setting */
int x_ctMode( int top, int left, int bottom, int right )
{
   s_ctModeTop = IIF( top < 0, s_ctModeTop, top );
   s_ctModeLeft = IIF( left < 0, s_ctModeLeft, left );
   s_ctModeBottom = IIF( bottom < 0, s_ctModeBottom, bottom );
   s_ctModeRight = IIF( right < 0, s_ctModeRight, right );

   return 0;
}

/* The shadow color of the windows opened later, -1 without a shadow; returns the previous color */
int x_ctSetShadow( int color )
{
   int previous = s_ctShadowColor;

   s_ctShadowColor = color;

   return previous;
}

/* The shadow color of the windows opened later */
int x_ctShadow( void )
{
   return s_ctShadowColor;
}

/* The interactive moving setting; -1 only returns it */
int x_ctSetMove( int mode )
{
   int previous = s_ctMoveMode;

   if( mode >= 0 )
      s_ctMoveMode = mode;

   return previous;
}

/* WStep(): the step of the interactive moving, at most 6 rows and 20 columns; it may be set only while no
   window is open */
int x_ctStep( int rows, int cols )
{
   if( s_ctScreen || rows < 1 || cols < 1 )
      return -1;

   s_ctStepRows = IIF( rows > 6, 6, rows );
   s_ctStepCols = IIF( cols > 20, 20, cols );

   return 0;
}

/* The end of the interactive moving: the next arrow keys use the step of WStep() again */
static void s_ctMoveEnd( void )
{
   s_ctMoveLast = 0;
   s_ctFineRows = 0;
   s_ctFineCols = 0;
}

/* Interactive moving: with WSetMove() and Scroll Lock on, an arrow key moves the current window by the step
   of WStep(); two opposite keys one after the other move that axis by 1. 1 when the key was used */
int x_ctMoveKey( int key )
{
   X_ITEM shifts __attribute__( ( cleanup( x_itemClear ) ) ) = X_ITEM_NIL_INIT;
   X_GT_INFO info = { NULL, NULL, &shifts };
   X_CT_WINDOW *window;
   int rows = 0, cols = 0;

   key = x_inkeyKeyStd( key );
   if( key == K_UP || key == K_DOWN )
      rows = IIF( key == K_UP, -1, 1 );
   else if( key == K_LEFT || key == K_RIGHT )
      cols = IIF( key == K_LEFT, -1, 1 );

   if( !s_ctScreen || !s_ctMoveMode || !s_ctCurrent || ( !rows && !cols ) )
   {
      s_ctMoveEnd();
      return 0;
   }

   s_ctScreen->pFuncTable->Info( s_ctScreen, X_GTI_KBDSHIFTS, &info );
   if( !( x_itemGetNI( &shifts ) & X_GTI_KBD_SCROLOCK ) )
   {
      s_ctMoveEnd();
      return 0;
   }

   if( rows && ( s_ctMoveLast == K_UP || s_ctMoveLast == K_DOWN ) && s_ctMoveLast != key )
      s_ctFineRows = 1;

   if( cols && ( s_ctMoveLast == K_LEFT || s_ctMoveLast == K_RIGHT ) && s_ctMoveLast != key )
      s_ctFineCols = 1;

   s_ctMoveLast = key;
   window = s_ctWindows[s_ctCurrent];
   rows *= IIF( s_ctFineRows, 1, s_ctStepRows );
   cols *= IIF( s_ctFineCols, 1, s_ctStepCols );
   x_ctMove( window->top + rows, window->left + cols );

   return 1;
}

/* ---------------------------------------------------------------------------------------------
   CT window functions of the API
   --------------------------------------------------------------------------------------------- */

/* WSetShadow(): define the shadow color for windows opened later; -1 turns the shadow off */
void x_ctWSetShadow( X_CALL_CTX *ctx )
{
   if( X_ISCHAR( ctx, 1 ) )
      x_retni( ctx, x_ctSetShadow( x_gtColorToN( x_parc( ctx, 1 ) ) ) );
   else if( X_ISNUM( ctx, 1 ) )
      x_retni( ctx, x_ctSetShadow( x_parni( ctx, 1 ) ) );
   else
      x_retni( ctx, x_ctShadow() );
}
