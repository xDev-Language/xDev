/*
 * Copyright (c) 2024-2026 xDev-Language
 *
 * Library...: xDev Runtime
 * File......: terminal.c
 *
 */

#include "platform.h"

#include "terminal.h"
#include "../../include/xdev/inkey.ch"
#include "gtinfo.h"
#include "../../include/xdev/setcurs.ch"

#include <ctype.h>
#include <wctype.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <limits.h>
#include <math.h>

#if X_OS_WINDOWS
#include <windows.h>
#else
#include <time.h>
#endif

/* The bytes of one cell in a saved screen: character, color, attribute, foreground and background */
enum
{
   X_GT_SAVED_CELL = 12
};

/* Alert(): the text in UTF-16, the layout for the current screen size and the saved screen */
typedef struct
{
   uint16_t *text; /* message, ';' separates lines */
   int textLength;
   uint16_t **options;
   int *optionLength;
   int *optionCol; /* screen column of each option */
   int optionCount;
   int optionWidth; /* all options with their spaces */
   const uint16_t **lines; /* wrapped message lines */
   int *lineLength;
   int lineCount;
   int top, left, bottom, right;
   unsigned char *saved; /* screen under the box */
   int colorNormal;
   int colorHigh;
   int choice; /* selected option, from 0 */
} X_GT_ALERT;

/* The registered drivers and the current screen */
static const X_GT_INIT *s_gtRegistry[32];
static int s_gtCount;
static X_GT_BASE *s_gtCurrent;

/* The last key of a program without GT, e.g. set by Read of the GUI */
static int s_inkeyLastNoGt;

/* A special key and its codes with Alt, Ctrl and Shift; the position is the value of the extended key */
typedef struct
{
   int plain;
   int alt;
   int ctrl;
   int shift;
} X_INKEY_TRANSLATION;

static const X_INKEY_TRANSLATION s_inkeySpecial[29] = {
   { K_F1, K_ALT_F1, K_CTRL_F1, K_SH_F1 },
   { K_F2, K_ALT_F2, K_CTRL_F2, K_SH_F2 },
   { K_F3, K_ALT_F3, K_CTRL_F3, K_SH_F3 },
   { K_F4, K_ALT_F4, K_CTRL_F4, K_SH_F4 },
   { K_F5, K_ALT_F5, K_CTRL_F5, K_SH_F5 },
   { K_F6, K_ALT_F6, K_CTRL_F6, K_SH_F6 },
   { K_F7, K_ALT_F7, K_CTRL_F7, K_SH_F7 },
   { K_F8, K_ALT_F8, K_CTRL_F8, K_SH_F8 },
   { K_F9, K_ALT_F9, K_CTRL_F9, K_SH_F9 },
   { K_F10, K_ALT_F10, K_CTRL_F10, K_SH_F10 },
   { K_F11, K_ALT_F11, K_CTRL_F11, K_SH_F11 },
   { K_F12, K_ALT_F12, K_CTRL_F12, K_SH_F12 },
   { K_UP, K_ALT_UP, K_CTRL_UP, K_UP },
   { K_DOWN, K_ALT_DOWN, K_CTRL_DOWN, K_DOWN },
   { K_LEFT, K_ALT_LEFT, K_CTRL_LEFT, K_LEFT },
   { K_RIGHT, K_ALT_RIGHT, K_CTRL_RIGHT, K_RIGHT },
   { K_HOME, K_ALT_HOME, K_CTRL_HOME, K_HOME },
   { K_END, K_ALT_END, K_CTRL_END, K_END },
   { K_PGUP, K_ALT_PGUP, K_CTRL_PGUP, K_PGUP },
   { K_PGDN, K_ALT_PGDN, K_CTRL_PGDN, K_PGDN },
   { K_INS, K_ALT_INS, K_CTRL_INS, K_INS },
   { K_DEL, K_ALT_DEL, K_CTRL_DEL, K_DEL },
   { K_BS, K_ALT_BS, K_CTRL_BS, K_SH_BS },
   { K_TAB, K_ALT_TAB, K_CTRL_TAB, K_SH_TAB },
   { K_ESC, K_ALT_ESC, K_ESC, K_ESC },
   { K_ENTER, K_ALT_ENTER, K_CTRL_ENTER, K_ENTER },
   { KP_CENTER, 0, KP_CTRL_5, '5' },
   { 0, 0, K_CTRL_PRTSCR, 0 },
   { 0, 0, 0, 0 }
};

/* Alt + A to Alt + Z */
static const int s_inkeyAltLetter[26] = {
   K_ALT_A, K_ALT_B, K_ALT_C, K_ALT_D, K_ALT_E, K_ALT_F, K_ALT_G, K_ALT_H, K_ALT_I,
   K_ALT_J, K_ALT_K, K_ALT_L, K_ALT_M, K_ALT_N, K_ALT_O, K_ALT_P, K_ALT_Q, K_ALT_R,
   K_ALT_S, K_ALT_T, K_ALT_U, K_ALT_V, K_ALT_W, K_ALT_X, K_ALT_Y, K_ALT_Z
};

static uint16_t s_gtNextCharacter( const X_GT_BASE *gt, const unsigned char *text, size_t length, size_t *offset );

/* ---------------------------------------------------------------------------------------------
   The base driver: the screen buffer that every GT draws from
   --------------------------------------------------------------------------------------------- */

static void s_gtReleaseHeld( X_GT_BASE **gt )
{
   if( *gt )
      x_gtRelease( *gt );
}

/* A new screen buffer of rows x cols; the old cells are kept where they fit */
static int s_gtResize( X_GT_BASE *gt, int rows, int cols )
{
   X_GT_CELL *screen, *previous;
   uint8_t *dirty;
   size_t count, i;
   int row, copyRows, copyCols;

   if( rows <= 0 || cols <= 0 || ( size_t )rows > SIZE_MAX / sizeof( X_GT_CELL ) / ( size_t )cols )
      return 0;

   if( gt->screenBuffer && rows == gt->rows && cols == gt->cols )
      return 1;

   count = ( size_t )rows * cols;
   screen = ( X_GT_CELL * )x_xgrabArray( count, sizeof( X_GT_CELL ) );
   previous = ( X_GT_CELL * )x_xgrabArray( count, sizeof( X_GT_CELL ) );
   dirty = ( uint8_t * )x_xgrab( ( size_t )rows );

   for( i = 0; i < count; ++i )
   {
      screen[i].character = ' ';
      screen[i].color = ( uint8_t )gt->colors[0]; /* new cells take the standard SetColor() color */
      screen[i].attr = 0;
      screen[i].foreground = gt->foreground; /* and the SetColorExt() colors, when they are set */
      screen[i].background = gt->background;
   }

   memset( previous, 0xFF, count * sizeof( X_GT_CELL ) );
   memset( dirty, IIF( gt->screenActive, 1, 0 ), ( size_t )rows );

   if( gt->screenBuffer )
   {
      copyRows = IIF( rows < gt->rows, rows, gt->rows );
      copyCols = IIF( cols < gt->cols, cols, gt->cols );
      for( row = 0; row < copyRows; ++row )
      {
         X_GT_CELL *target = screen + ( size_t )row * cols;
         const X_GT_CELL *source = gt->screenBuffer + ( size_t )row * gt->cols;

         memcpy( target, source, ( size_t )copyCols * sizeof( X_GT_CELL ) );
      }

      x_xfree( gt->screenBuffer );
      x_xfree( gt->prevBuffer );
      x_xfree( gt->dirtyLines );
   }

   gt->screenBuffer = screen;
   gt->prevBuffer = previous;
   gt->dirtyLines = dirty;
   gt->rows = rows;
   gt->cols = cols;
   gt->cursorDirty = 1;

   return 1;
}

/* The start of a screen: the standard handles, the colors of SetColor() and 25 x 80 cells */
static void s_gtInit( X_GT_BASE *gt, int inputHandle, int outputHandle, int errorHandle )
{
   gt->hStdin = inputHandle;
   gt->hStdout = outputHandle;
   gt->hStderr = errorHandle;
   gt->cursorStyle = 1;
   gt->blink = 1;
   gt->colors[0] = 7;
   gt->colors[1] = 0x70;
   gt->colors[2] = 0;
   gt->colors[3] = 0;
   gt->colors[4] = 0x70;
   gt->colorCount = 5;
   gt->foreground = X_GT_RGB_DEFAULT;
   gt->background = X_GT_RGB_DEFAULT;
   gt->pFuncTable->Resize( gt, 25, 80 );
}

/* Release the blocks and the cargo of GtInfo() */
static void s_gtReleaseItems( X_GT_BASE *gt )
{
   X_ITEM *filter = gt->inkeyFilterBlock;
   X_ITEM *reader = gt->inkeyReadBlock;
   X_ITEM *resize = gt->resizeBlock;
   X_ITEM *cargo = gt->cargo;

   gt->inkeyFilterBlock = NULL;
   gt->inkeyReadBlock = NULL;
   gt->resizeBlock = NULL;
   gt->cargo = NULL;
   x_itemRelease( filter );
   x_itemRelease( reader );
   x_itemRelease( resize );
   x_itemRelease( cargo );
}

/* Nothing to do in this step of the program */
static void s_gtModuleNone( X_CALL_CTX *ctx )
{
   UNUSED( ctx );
}

/* At the end of the program the windows close and the blocks of GtInfo() are released */
static void s_gtModuleExit( X_CALL_CTX *ctx )
{
   UNUSED( ctx );
   x_ctExit();
   if( s_gtCurrent )
      s_gtReleaseItems( s_gtCurrent );
}

__attribute__( ( constructor ) ) static void s_gtModuleRegister( void )
{
   x_moduleRegister( s_gtModuleNone, s_gtModuleNone, s_gtModuleExit, s_gtModuleNone );
}

/* Release a screen */
static void s_gtExit( X_GT_BASE *gt )
{
   s_gtReleaseItems( gt );
   if( gt->inkeyText )
      x_xfree( gt->inkeyText );

   if( gt->savedScreen )
      x_xfree( gt->savedScreen );

   if( gt->screenBuffer )
   {
      x_xfree( gt->screenBuffer );
      x_xfree( gt->prevBuffer );
      x_xfree( gt->dirtyLines );
   }

   x_xfree( gt->pFuncTable );
   x_xfree( gt );
}

/* Do two cells look the same? */
static int s_gtCellEqual( const X_GT_CELL *a, const X_GT_CELL *b )
{
   if( a->character != b->character || a->color != b->color || a->attr != b->attr )
      return 0;

   return a->foreground == b->foreground && a->background == b->background;
}

/* Draw the changed parts of the dirty lines; gaps up to redrawMax equal cells are drawn with them */
static void s_gtRedrawDiff( X_GT_BASE *gt )
{
   int row, col, first, last;
   size_t offset, size;

   for( row = 0; row < gt->rows; ++row )
   {
      if( !gt->dirtyLines[row] )
         continue;

      offset = ( size_t )row * gt->cols;
      col = 0;
      while( col < gt->cols )
      {
         if( s_gtCellEqual( &gt->screenBuffer[offset + col], &gt->prevBuffer[offset + col] ) )
         {
            ++col;
            continue;
         }

         first = col++;
         last = col;
         while( col < gt->cols )
         {
            if( !s_gtCellEqual( &gt->screenBuffer[offset + col], &gt->prevBuffer[offset + col] ) )
               last = col + 1;
            else if( col - last >= gt->redrawMax )
               break;

            ++col;
         }

         col = last;
         gt->pFuncTable->Redraw( gt, row, first, col - first );
         if( gt->outputError )
            return;

         size = ( size_t )( col - first ) * sizeof( X_GT_CELL );
         memcpy( &gt->prevBuffer[offset + first], &gt->screenBuffer[offset + first], size );
      }

      gt->dirtyLines[row] = 0;
   }
}

/* Show the changes */
static void s_gtRefresh( X_GT_BASE *gt )
{
   gt->pFuncTable->RedrawDiff( gt );
}

/* The base driver has no device to draw on */
static void s_gtRedraw( X_GT_BASE *gt, int row, int col, int count )
{
   UNUSED( gt );
   UNUSED( row );
   UNUSED( col );
   UNUSED( count );
}

/* Show the changes unless DispBegin() holds them */
static void s_gtFlush( X_GT_BASE *gt )
{
   if( gt->dispCount == 0 )
      gt->pFuncTable->Refresh( gt );
}

/* The base driver has no bell */
static void s_gtBell( X_GT_BASE *gt )
{
   UNUSED( gt );
}

/* Before the program gives the screen to another program */
static int s_gtSuspend( X_GT_BASE *gt )
{
   gt->pFuncTable->Refresh( gt );

   return 1;
}

/* After another program used the screen: everything is drawn again */
static int s_gtResume( X_GT_BASE *gt )
{
   memset( gt->prevBuffer, 0xFF, ( size_t )gt->rows * gt->cols * sizeof( X_GT_CELL ) );
   memset( gt->dirtyLines, 1, ( size_t )gt->rows );
   gt->screenActive = 1;
   gt->cursorDirty = 1;
   gt->pFuncTable->Refresh( gt );

   return !gt->outputError;
}

/* The base driver cannot change the screen size */
static int s_gtSetMode( X_GT_BASE *gt, int rows, int cols )
{
   return rows == gt->rows && cols == gt->cols;
}

/* The name of the base driver */
static const char *s_gtVersion( X_GT_BASE *gt, int type )
{
   UNUSED( gt );
   UNUSED( type );

   return "NUL";
}

/* The base driver has no keyboard */
static int s_gtReadKey( X_GT_BASE *gt, int eventMask )
{
   UNUSED( gt );
   UNUSED( eventMask );

   return 0;
}

/* The screen size */
static void s_gtGetSize( X_GT_BASE *gt, int *rows, int *cols )
{
   *rows = gt->rows;
   *cols = gt->cols;
}

/* Move the cursor */
static void s_gtSetPos( X_GT_BASE *gt, int row, int col )
{
   gt->cursorDirty = 1;
   gt->row = row;
   gt->col = col;
}

/* Set the cursor shape */
static void s_gtSetCursorStyle( X_GT_BASE *gt, int style )
{
   gt->cursorDirty = 1;
   gt->cursorStyle = style;
}

/* Forget the changes that were not shown */
static void s_gtSemiCold( X_GT_BASE *gt )
{
   memset( gt->dirtyLines, 0, ( size_t )gt->rows );
}

/* Draw an area again, for example after another window covered it */
static void s_gtExposeArea( X_GT_BASE *gt, int top, int left, int bottom, int right )
{
   int row, col, swap;

   if( top > bottom )
   {
      swap = top;
      top = bottom;
      bottom = swap;
   }

   if( left > right )
   {
      swap = left;
      left = right;
      right = swap;
   }

   top = IIF( top < 0, 0, top );
   left = IIF( left < 0, 0, left );
   bottom = IIF( bottom >= gt->rows, gt->rows - 1, bottom );
   right = IIF( right >= gt->cols, gt->cols - 1, right );
   for( row = top; row <= bottom; ++row )
   {
      for( col = left; col <= right; ++col )
         memset( &gt->prevBuffer[( size_t )row * gt->cols + col], 0xFF, sizeof( X_GT_CELL ) );

      gt->dirtyLines[row] = 1;
   }
}

/* The cursor; its shape is 0 when it is outside the screen */
static void s_gtGetScrCursor( X_GT_BASE *gt, int *row, int *col, int *style )
{
   *row = gt->row;
   *col = gt->col;
   *style = IIF( *row < 0 || *col < 0 || *row >= gt->rows || *col >= gt->cols, 0, gt->cursorStyle );
}

/* The color, attribute and character of a cell; 0 outside the screen */
static int s_gtGetScrChar( X_GT_BASE *gt, int row, int col, int *color, uint8_t *attr, uint16_t *character )
{
   const X_GT_CELL *cell;

   if( row < 0 || row >= gt->rows || col < 0 || col >= gt->cols )
      return 0;

   cell = &gt->screenBuffer[( size_t )row * gt->cols + col];
   *color = cell->color;
   *attr = cell->attr;
   *character = cell->character;

   return 1;
}

/* DispBegin(): hold the changes */
static void s_gtDispBegin( X_GT_BASE *gt )
{
   if( gt->dispCount < UINT_MAX )
      ++gt->dispCount;
}

/* DispEnd(): show the changes after the last DispEnd() */
static void s_gtDispEnd( X_GT_BASE *gt )
{
   if( gt->dispCount )
      --gt->dispCount;

   gt->pFuncTable->Flush( gt );
}

/* DispCount() */
static int s_gtDispCount( X_GT_BASE *gt )
{
   return ( int )gt->dispCount;
}

/* SaveScreen(): the cells of an area, X_GT_SAVED_CELL bytes each; cells outside the screen are spaces */
static void s_gtSaveBuffer( X_GT_BASE *gt, int top, int left, int bottom, int right, unsigned char *buffer )
{
   X_GT_CELL cell;
   int row, col;

   for( row = top; row <= bottom; ++row )
   {
      for( col = left; col <= right; ++col )
      {
         cell.character = ' ';
         cell.color = 7;
         cell.attr = 0;
         cell.foreground = X_GT_RGB_DEFAULT;
         cell.background = X_GT_RGB_DEFAULT;
         if( row < gt->rows && col < gt->cols )
            cell = gt->screenBuffer[( size_t )row * gt->cols + col];

         *buffer++ = ( unsigned char )cell.character;
         *buffer++ = ( unsigned char )( cell.character >> 8 );
         *buffer++ = cell.color;
         *buffer++ = cell.attr;
         *buffer++ = ( unsigned char )cell.foreground;
         *buffer++ = ( unsigned char )( cell.foreground >> 8 );
         *buffer++ = ( unsigned char )( cell.foreground >> 16 );
         *buffer++ = ( unsigned char )( cell.foreground >> 24 );
         *buffer++ = ( unsigned char )cell.background;
         *buffer++ = ( unsigned char )( cell.background >> 8 );
         *buffer++ = ( unsigned char )( cell.background >> 16 );
         *buffer++ = ( unsigned char )( cell.background >> 24 );
      }
   }
}

/* A color of a saved cell: 4 bytes, low byte first */
static uint32_t s_gtSavedRgb( const unsigned char *bytes )
{
   uint32_t value = ( uint32_t )bytes[0] | ( ( uint32_t )bytes[1] << 8 );

   return value | ( ( uint32_t )bytes[2] << 16 ) | ( ( uint32_t )bytes[3] << 24 );
}

/* RestScreen(): put saved cells back; cells outside the screen are skipped */
static void s_gtRestBuffer( X_GT_BASE *gt, int top, int left, int bottom, int right, const unsigned char *buffer )
{
   int row, col;
   int lastRow = IIF( bottom < gt->rows, bottom, gt->rows - 1 );
   int lastCol = IIF( right < gt->cols, right, gt->cols - 1 );
   size_t width = ( size_t )right - left + 1;

   for( row = top; row <= lastRow; ++row )
   {
      for( col = left; col <= lastCol; ++col )
      {
         const unsigned char *cell = buffer + ( ( size_t )( row - top ) * width + col - left ) * X_GT_SAVED_CELL;
         uint16_t character = ( uint16_t )( cell[0] | ( ( unsigned int )cell[1] << 8 ) );
         uint32_t foreground = s_gtSavedRgb( cell + 4 );
         uint32_t background = s_gtSavedRgb( cell + 8 );

         gt->pFuncTable->PutChar( gt, row, col, cell[2], cell[3], character );
         if( foreground != X_GT_RGB_DEFAULT || background != X_GT_RGB_DEFAULT )
         {
            X_GT_CELL *target = &gt->screenBuffer[( size_t )row * gt->cols + col];

            target->foreground = foreground;
            target->background = background;
         }
      }
   }
}

/* GTI_INKEYFILTER, GTI_INKEYREAD and GTI_RESIZEBLOCK: the old block; a new block or NIL replaces it */
static int s_gtInfoBlock( X_GT_INFO *info, X_ITEM **slot )
{
   if( !info->pResult )
      info->pResult = x_itemNew( NULL );

   if( *slot )
      x_itemCopy( info->pResult, *slot );
   else
      x_itemClear( info->pResult );

   if( info->pNewVal && ( X_IS_BLOCK( info->pNewVal ) || X_IS_NIL( info->pNewVal ) ) )
   {
      X_ITEM *previous = *slot;

      *slot = IIF( X_IS_BLOCK( info->pNewVal ), x_itemNew( info->pNewVal ), NULL );
      x_itemRelease( previous );
   }

   return 1;
}

/* GTI_CARGO: the old value; a new value replaces it */
static int s_gtInfoCargo( X_GT_BASE *gt, X_GT_INFO *info )
{
   if( !info->pResult )
      info->pResult = x_itemNew( NULL );

   if( gt->cargo )
      x_itemCopy( info->pResult, gt->cargo );
   else
      x_itemClear( info->pResult );

   if( info->pNewVal )
   {
      if( !gt->cargo )
         gt->cargo = x_itemNew( info->pNewVal );
      else
         x_itemCopy( gt->cargo, info->pNewVal );
   }

   return 1;
}

/* GTI_NEWWIN and GTI_EXPOSEAREA: { nTop, nLeft, nBottom, nRight } is drawn again */
static int s_gtInfoArea( X_GT_BASE *gt, X_GT_INFO *info )
{
   X_ITEM *area = info->pNewVal;
   int64_t values[4];
   size_t index;

   info->pResult = x_itemPutL( info->pResult, 0 );
   if( !area || !X_IS_ARRAY( area ) || x_arrayLen( area ) != 4 )
      return 1;

   for( index = 0; index < 4; ++index )
   {
      X_ITEM *item = x_arrayGetItemPtr( area, index + 1 );

      if( !X_IS_NUMERIC( item ) || x_itemGetNInt( item ) < INT_MIN || x_itemGetNInt( item ) > INT_MAX )
         return 1;

      values[index] = x_itemGetNInt( item );
   }

   gt->pFuncTable->ExposeArea( gt, ( int )values[0], ( int )values[1], ( int )values[2], ( int )values[3] );
   info->pResult = x_itemPutL( info->pResult, 1 );

   return 1;
}

/* GTI_GETWIN: { nRow, nCol, nCursor, nMaxRow, nMaxCol, cScreen, NIL } of the whole screen */
static int s_gtInfoGetWin( X_GT_BASE *gt, int type, X_GT_INFO *info )
{
   size_t length;
   char *buffer;

   if( type == X_GTI_NEWWIN )
   {
      gt->pFuncTable->DispBegin( gt );
      gt->pFuncTable->Scroll( gt, 0, 0, gt->rows - 1, gt->cols - 1, gt->colors[gt->colorIndex], ' ', 0, 0 );
      gt->pFuncTable->SetPos( gt, 0, 0 );
      gt->pFuncTable->SetCursorStyle( gt, SC_NORMAL );
      gt->pFuncTable->DispEnd( gt );
   }

   length = ( size_t )gt->rows * gt->cols * X_GT_SAVED_CELL;
   buffer = ( char * )x_xgrab( length + 1 );

   s_gtSaveBuffer( gt, 0, 0, gt->rows - 1, gt->cols - 1, ( unsigned char * )buffer );
   buffer[length] = '\0';
   if( !info->pResult )
      info->pResult = x_itemNew( NULL );

   x_arrayNew( info->pResult, 7 );
   x_arraySetNI( info->pResult, 1, gt->row );
   x_arraySetNI( info->pResult, 2, gt->col );
   x_arraySetNI( info->pResult, 3, gt->cursorStyle );
   x_arraySetNI( info->pResult, 4, gt->rows - 1 );
   x_arraySetNI( info->pResult, 5, gt->cols - 1 );
   x_itemPutCLPtr( x_arrayGetItemPtr( info->pResult, 6 ), buffer, length );

   return 1;
}

/* GTI_SETWIN: the screen of GTI_GETWIN back; .F. for an array that is not one */
static int s_gtInfoSetWin( X_GT_BASE *gt, X_GT_INFO *info )
{
   X_ITEM *state = info->pNewVal;
   int64_t values[5];
   size_t index, rows, cols;
   X_ITEM *buffer;
   const unsigned char *cells;

   info->pResult = x_itemPutL( info->pResult, 0 );
   if( !state || !X_IS_ARRAY( state ) || x_arrayLen( state ) != 7 )
      return 1;

   for( index = 0; index < 5; ++index )
   {
      X_ITEM *item = x_arrayGetItemPtr( state, index + 1 );

      if( !X_IS_NUMERIC( item ) )
         return 1;

      values[index] = x_itemGetNInt( item );
   }

   if( values[0] < INT_MIN || values[0] > INT_MAX || values[1] < INT_MIN || values[1] > INT_MAX )
      return 1;

   if( values[2] < SC_NONE || values[2] > SC_SPECIAL2 )
      return 1;

   if( values[3] < 0 || values[3] >= INT_MAX || values[4] < 0 || values[4] >= INT_MAX )
      return 1;

   rows = ( size_t )values[3] + 1;
   cols = ( size_t )values[4] + 1;
   buffer = x_arrayGetItemPtr( state, 6 );
   if( rows > SIZE_MAX / X_GT_SAVED_CELL / cols || !X_IS_STRING( buffer ) )
      return 1;

   if( x_itemGetCLen( buffer ) != rows * cols * X_GT_SAVED_CELL )
      return 1;

   cells = ( const unsigned char * )x_itemGetCPtr( buffer );
   gt->pFuncTable->DispBegin( gt );
   s_gtRestBuffer( gt, 0, 0, ( int )values[3], ( int )values[4], cells );
   gt->pFuncTable->SetPos( gt, ( int )values[0], ( int )values[1] );
   gt->pFuncTable->SetCursorStyle( gt, ( int )values[2] );
   gt->pFuncTable->DispEnd( gt );
   info->pResult = x_itemPutL( info->pResult, gt->outputError == 0 );

   return 1;
}

/* GtInfo() questions every driver answers the same way; 0 for a question the base driver does not know */
static int s_gtInfo( X_GT_BASE *gt, int type, X_GT_INFO *info )
{
   int console;

   switch( type )
   {
   case X_GTI_INKEYFILTER:
      return s_gtInfoBlock( info, &gt->inkeyFilterBlock );

   case X_GTI_INKEYREAD:
      return s_gtInfoBlock( info, &gt->inkeyReadBlock );

   case X_GTI_RESIZEBLOCK:
      return s_gtInfoBlock( info, &gt->resizeBlock );

   case X_GTI_CARGO:
      return s_gtInfoCargo( gt, info );

   case X_GTI_COMPATBUFFER:
      info->pResult = x_itemPutL( info->pResult, gt->compatBuffer );
      if( info->pNewVal && X_IS_LOGICAL( info->pNewVal ) )
         gt->pFuncTable->SetFlag( gt, type, x_itemGetL( info->pNewVal ) );

      return 1;

   case X_GTI_REDRAWMAX:
      info->pResult = x_itemPutNI( info->pResult, gt->redrawMax );
      if( info->pNewVal && X_IS_NUMERIC( info->pNewVal ) )
      {
         double number = x_itemGetND( info->pNewVal );

         if( isfinite( number ) && number >= 0 && number <= INT_MAX )
            gt->pFuncTable->SetFlag( gt, type, ( int )number );
      }

      return 1;

   case X_GTI_STDOUTCON:
   case X_GTI_STDERRCON:
      console = IIF( type == X_GTI_STDOUTCON, gt->stdoutConsole, gt->stderrConsole );
      info->pResult = x_itemPutL( info->pResult, console );
      if( info->pNewVal && X_IS_LOGICAL( info->pNewVal ) )
         gt->pFuncTable->SetFlag( gt, type, x_itemGetL( info->pNewVal ) );

      return 1;

   case X_GTI_NEWWIN:
   case X_GTI_EXPOSEAREA:
      return s_gtInfoArea( gt, info );

   case X_GTI_GETWIN:
      return s_gtInfoGetWin( gt, type, info );

   case X_GTI_SETWIN:
      return s_gtInfoSetWin( gt, info );

   default:
      return 0;
   }
}

/* Parses one color of a color string ("W+/B") up to a comma or the end; the result is -1 for an
   invalid character, otherwise whether a color was given */
static int s_gtColorParse( const unsigned char **text, int *color )
{
   const unsigned char *p = *text;
   int part = 0;
   int seen = 0;
   int ch, value;

   *color = 0;
   while( *p && *p != ',' )
   {
      ch = toupper( *p++ );
      if( isspace( ch ) )
         continue;

      seen = 1;
      switch( ch )
      {
      case '/':
         part = 1;
         break;

      case '*':
         *color |= 128;
         break;

      case '+':
         *color |= 8;
         break;

      case 'N':
         *color &= IIF( part, ~0x70, ~7 );
         break;

      case 'B':
         *color |= IIF( part, 0x10, 1 );
         break;

      case 'G':
         *color |= IIF( part, 0x20, 2 );
         break;

      case 'R':
         *color |= IIF( part, 0x40, 4 );
         break;

      case 'W':
         *color |= IIF( part, 0x70, 7 );
         break;

      case 'I':
         *color = ( *color & 0x88 ) | 0x70;
         part = 1;
         break;

      case 'X':
         *color &= 0x88;
         break;

      default:
         if( ch < '0' || ch > '9' )
            return -1;

         /* A color number: the background after '/', else the foreground */
         value = ch - '0';
         while( *p >= '0' && *p <= '9' )
            value = ( value * 10 + *p++ - '0' ) & 15;

         if( part )
            *color = ( *color & 0x0F ) | ( ( value & 15 ) << 4 );
         else
            *color = ( *color & 0xF0 ) | ( value & 15 );
      }
   }

   *text = p;

   return seen;
}

/* UTF-8 text of an item as UTF-16 cells, in an x_xgrab() buffer */
static uint16_t *s_gtAlertText( const X_GT_BASE *gt, const X_ITEM *item, int *length )
{
   const unsigned char *source = ( const unsigned char * )x_itemGetCPtr( item );
   size_t size = x_itemGetCLen( item );
   size_t offset = 0;
   uint16_t *text = ( uint16_t * )x_xgrab( ( size + 1 ) * sizeof( uint16_t ) );

   *length = 0;
   while( offset < size )
      text[( *length )++] = s_gtNextCharacter( gt, source, size, &offset );

   return text;
}

/* Lays the box out for the current screen size and saves the screen under it. Lines are split
   at ';' and wrapped at a space when they are wider than the screen. */
static void s_gtAlertLayout( X_GT_BASE *gt, X_GT_ALERT *alert )
{
   int maxWidth = IIF( gt->cols > 5, gt->cols - 4, 1 );
   int maxLines = IIF( gt->rows > 5, gt->rows - 4, 1 );
   int start = 0;
   int width = alert->optionWidth;
   int height, index, first;

   alert->lineCount = 0;
   while( start <= alert->textLength && alert->lineCount < maxLines )
   {
      int end = start;
      int length;

      while( end < alert->textLength && alert->text[end] != ';' )
         ++end;

      length = end - start;
      if( length > maxWidth )
      {
         for( length = maxWidth; length > 0 && alert->text[start + length] != ' '; --length )
            ;

         if( length == 0 )
            length = maxWidth;

         end = start + length - IIF( alert->text[start + length] == ' ', 0, 1 );
      }

      alert->lines[alert->lineCount] = alert->text + start;
      alert->lineLength[alert->lineCount++] = length;
      width = IIF( length > width, length, width );
      start = end + 1;
   }

   width = IIF( width > maxWidth, maxWidth, width );
   height = alert->lineCount + 4;
   alert->top = IIF( gt->rows > height, ( gt->rows - height ) / 2, 0 );
   alert->left = IIF( gt->cols > width + 4, ( gt->cols - width - 4 ) / 2, 0 );
   alert->bottom = alert->top + height - 1;
   alert->right = alert->left + width + 3;

   /* The options are centred in the box, two spaces apart */
   first = alert->left + 2 + IIF( width > alert->optionWidth, ( width - alert->optionWidth ) / 2, 0 );
   for( index = 0; index < alert->optionCount; ++index )
   {
      if( index == 0 )
         alert->optionCol[index] = first;
      else
         alert->optionCol[index] = alert->optionCol[index - 1] + alert->optionLength[index - 1] + 4;
   }

   x_xfree( alert->saved );
   alert->saved = ( unsigned char * )x_xgrab( ( size_t )height * ( width + 4 ) * X_GT_SAVED_CELL );
   s_gtSaveBuffer( gt, alert->top, alert->left, alert->bottom, alert->right, alert->saved );
}

/* The frame character of a cell of a box: 0 to 7 clockwise from the top left corner, 8 inside */
static int s_gtFrameIndex( int row, int col, int top, int left, int bottom, int right )
{
   if( row == top )
   {
      if( col == left )
         return 0;

      return IIF( col == right, 2, 1 );
   }

   if( row == bottom )
   {
      if( col == left )
         return 6;

      return IIF( col == right, 4, 5 );
   }

   if( col == left )
      return 7;

   return IIF( col == right, 3, 8 );
}

/* Draw the box, the message lines and the options; the chosen option is highlighted */
static void s_gtAlertDraw( X_GT_BASE *gt, const X_GT_ALERT *alert )
{
   static const uint16_t frame[9] = { 0x250C, 0x2500, 0x2510, 0x2502, 0x2518, 0x2500, 0x2514, 0x2502, ' ' };
   int row, col, index, i;

   gt->pFuncTable->DispBegin( gt );
   for( row = alert->top; row <= alert->bottom; ++row )
   {
      for( col = alert->left; col <= alert->right; ++col )
      {
         index = s_gtFrameIndex( row, col, alert->top, alert->left, alert->bottom, alert->right );
         gt->pFuncTable->PutChar( gt, row, col, alert->colorNormal, ( uint8_t )IIF( index == 8, 0, 1 ), frame[index] );
      }
   }

   /* Centred lines, then the options below an empty line */
   for( i = 0; i < alert->lineCount; ++i )
   {
      int first = alert->left + 2 + ( alert->right - alert->left - 3 - alert->lineLength[i] ) / 2;

      for( col = 0; col < alert->lineLength[i]; ++col )
         gt->pFuncTable->PutChar( gt, alert->top + 1 + i, first + col, alert->colorNormal, 0, alert->lines[i][col] );
   }

   for( i = 0; i < alert->optionCount; ++i )
   {
      int color = IIF( i == alert->choice, alert->colorHigh, alert->colorNormal );

      for( col = 0; col < alert->optionLength[i] + 2 && alert->optionCol[i] + col < alert->right - 1; ++col )
      {
         uint16_t character = IIF( col == 0 || col > alert->optionLength[i], ' ', alert->options[i][col - 1] );

         gt->pFuncTable->PutChar( gt, alert->bottom - 1, alert->optionCol[i] + col, color, 0, character );
      }
   }

   gt->pFuncTable->DispEnd( gt );
}

/* The option under the mouse (from 1); 0 when there is none */
static int s_gtAlertClicked( X_GT_BASE *gt, const X_GT_ALERT *alert )
{
   int row, col, index;

   gt->pFuncTable->MouseGetPos( gt, &row, &col );
   if( row != alert->bottom - 1 )
      return 0;

   for( index = 0; index < alert->optionCount; ++index )
   {
      if( col >= alert->optionCol[index] && col < alert->optionCol[index] + alert->optionLength[index] + 2 )
         return index + 1;
   }

   return 0;
}

/* The option whose first letter is the key, without case (from 1); 0 when there is none */
static int s_gtAlertLetter( const X_GT_ALERT *alert, int key )
{
   int index;

   if( key < 32 || key > 0xFFFF )
      return 0;

   for( index = 0; index < alert->optionCount; ++index )
   {
      if( alert->optionLength[index] == 0 )
         continue;

      if( towupper( ( wint_t )alert->options[index][0] ) == towupper( ( wint_t )key ) )
         return index + 1;
   }

   return 0;
}

/* Alert(): a box in the centre of the screen. After K_RESIZE the old screen comes back and the
   box is laid out again for the new size, so it stays in the centre of a resized window. */
static int s_gtAlert( X_GT_BASE *gt, X_ITEM *message, X_ITEM *options, int colorNormal, int colorHigh, double delay )
{
   static const uint16_t okText[] = { 'O', 'k' };
   X_GT_ALERT alert;
   int cursorRow, cursorCol, cursorStyle, key, index;
   int result = -1;

   memset( &alert, 0, sizeof( alert ) );
   alert.colorNormal = colorNormal;
   alert.colorHigh = colorHigh;
   alert.text = s_gtAlertText( gt, message, &alert.textLength );
   alert.lines = ( const uint16_t ** )x_xgrab( ( ( size_t )alert.textLength + 1 ) * sizeof( uint16_t * ) );
   alert.lineLength = ( int * )x_xgrab( ( ( size_t )alert.textLength + 1 ) * sizeof( int ) );

   alert.optionCount = IIF( options && X_IS_ARRAY( options ), ( int )x_arrayLen( options ), 0 );
   index = IIF( alert.optionCount, alert.optionCount, 1 );
   alert.options = ( uint16_t ** )x_xgrab( ( size_t )index * sizeof( uint16_t * ) );
   alert.optionLength = ( int * )x_xgrab( ( size_t )index * sizeof( int ) );
   alert.optionCol = ( int * )x_xgrab( ( size_t )index * sizeof( int ) );
   for( index = 0; index < alert.optionCount; ++index )
   {
      const X_ITEM *option = x_arrayGetItemPtr( options, ( size_t )index + 1 );

      alert.options[index] = s_gtAlertText( gt, option, &alert.optionLength[index] );
   }

   if( alert.optionCount == 0 )
   {
      alert.options[0] = ( uint16_t * )okText;
      alert.optionLength[0] = 2;
      alert.optionCount = 1;
   }

   for( index = 0; index < alert.optionCount; ++index )
      alert.optionWidth += alert.optionLength[index] + 2 + IIF( index, 2, 0 );

   gt->pFuncTable->GetScrCursor( gt, &cursorRow, &cursorCol, &cursorStyle );
   gt->pFuncTable->SetCursorStyle( gt, SC_NONE );
   s_gtAlertLayout( gt, &alert );
   s_gtAlertDraw( gt, &alert );

   while( result < 0 )
   {
      /* The timeout and Escape give 0 */
      key = gt->pFuncTable->InkeyGet( gt, 1, delay, INKEY_KEYBOARD | INKEY_LDOWN | INKEY_GTEVENT );
      switch( key )
      {
      case 0:
      case K_ESC:
         result = 0;
         break;

      case K_ENTER:
      case K_SPACE:
         result = alert.choice + 1;
         break;

      case K_LEFT:
      case K_SH_TAB:
         alert.choice = IIF( alert.choice > 0, alert.choice - 1, alert.optionCount - 1 );
         s_gtAlertDraw( gt, &alert );
         break;

      case K_RIGHT:
      case K_TAB:
         alert.choice = IIF( alert.choice < alert.optionCount - 1, alert.choice + 1, 0 );
         s_gtAlertDraw( gt, &alert );
         break;

      case K_LBUTTONDOWN:
         index = s_gtAlertClicked( gt, &alert );
         if( index > 0 )
            result = index;
         break;

      case K_RESIZE:
         s_gtRestBuffer( gt, alert.top, alert.left, alert.bottom, alert.right, alert.saved );
         s_gtAlertLayout( gt, &alert );
         s_gtAlertDraw( gt, &alert );
         break;

      default:
         /* The first letter of an option chooses it */
         index = s_gtAlertLetter( &alert, key );
         if( index > 0 )
            result = index;
         break;
      }
   }

   s_gtRestBuffer( gt, alert.top, alert.left, alert.bottom, alert.right, alert.saved );
   gt->pFuncTable->SetCursorStyle( gt, cursorStyle );
   gt->pFuncTable->SetPos( gt, cursorRow, cursorCol );
   gt->pFuncTable->Flush( gt );

   for( index = 0; index < alert.optionCount && alert.options[0] != okText; ++index )
      x_xfree( alert.options[index] );

   x_xfree( alert.options );
   x_xfree( alert.optionLength );
   x_xfree( alert.optionCol );
   x_xfree( alert.lines );
   x_xfree( alert.lineLength );
   x_xfree( alert.text );
   x_xfree( alert.saved );

   return result;
}

/* GTI_COMPATBUFFER, GTI_REDRAWMAX, GTI_STDOUTCON and GTI_STDERRCON; returns the previous value */
static int s_gtSetFlag( X_GT_BASE *gt, int type, int value )
{
   int previous = 0;

   if( type == X_GTI_COMPATBUFFER )
   {
      previous = gt->compatBuffer;
      gt->compatBuffer = value != 0;
   }
   else if( type == X_GTI_REDRAWMAX )
   {
      previous = gt->redrawMax;
      gt->redrawMax = value;
   }
   else if( type == X_GTI_STDOUTCON )
   {
      previous = gt->stdoutConsole;
      gt->stdoutConsole = value != 0;
   }
   else if( type == X_GTI_STDERRCON )
   {
      previous = gt->stderrConsole;
      gt->stderrConsole = value != 0;
   }

   return previous;
}

/* Is there a mouse? */
static int s_gtMouseIsPresent( X_GT_BASE *gt )
{
   return gt->mousePresent;
}

/* The mouse position */
static void s_gtMouseGetPos( X_GT_BASE *gt, int *row, int *col )
{
   *row = gt->mouseRow;
   *col = gt->mouseCol;
}

/* Move the mouse */
static void s_gtMouseSetPos( X_GT_BASE *gt, int row, int col )
{
   gt->mouseRow = row;
   gt->mouseCol = col;
}

/* The double click time in milliseconds */
static int s_gtMouseGetDoubleClickSpeed( X_GT_BASE *gt )
{
   return gt->mouseDoubleClickSpeed;
}

/* The number of mouse buttons */
static int s_gtMouseCountButton( X_GT_BASE *gt )
{
   return gt->mouseButtonCount;
}

/* Is mouse button 0 (left), 1 (right) or 2 (middle) down? */
static int s_gtMouseButtonState( X_GT_BASE *gt, int button )
{
   return button >= 0 && button < 3 && ( gt->mouseButtons & ( 1u << button ) ) != 0;
}

/* The base driver draws no graphics */
static int s_gtGfxPrimitive( X_GT_BASE *gt, int type, int top, int left, int bottom, int right, int color )
{
   UNUSED( gt );
   UNUSED( type );
   UNUSED( top );
   UNUSED( left );
   UNUSED( bottom );
   UNUSED( right );
   UNUSED( color );

   return 0;
}

/* The base driver draws no text in graphics */
static void s_gtGfxText( X_GT_BASE *gt, int top, int left, const char *text, int color, int size, int width )
{
   UNUSED( gt );
   UNUSED( top );
   UNUSED( left );
   UNUSED( text );
   UNUSED( color );
   UNUSED( size );
   UNUSED( width );
}

/* The base driver has only the 16 colors of the palette */
static int s_gtTrueColor( X_GT_BASE *gt )
{
   UNUSED( gt );

   return 0;
}

/* Put a character in a palette color; a 24-bit color of the cell is cleared */
static int s_gtPutChar( X_GT_BASE *gt, int row, int col, int color, uint8_t attr, uint16_t character )
{
   X_GT_CELL *cell;
   int same;

   if( row < 0 || row >= gt->rows || col < 0 || col >= gt->cols )
      return 0;

   cell = &gt->screenBuffer[( size_t )row * gt->cols + col];
   gt->screenActive = 1;
   gt->dirtyLines[row] = 1;

   same = cell->character == character && cell->color == color && cell->attr == attr;
   if( !same || cell->foreground != X_GT_RGB_DEFAULT || cell->background != X_GT_RGB_DEFAULT )
   {
      cell->character = character;
      cell->color = ( uint8_t )color;
      cell->attr = attr;
      cell->foreground = X_GT_RGB_DEFAULT;
      cell->background = X_GT_RGB_DEFAULT;
   }

   return 1;
}

/* Put a character in 24-bit colors */
static int s_gtPutCharExt( X_GT_BASE *gt, int row, int col, uint8_t attr, uint16_t character,
                           uint32_t foreground, uint32_t background )
{
   X_GT_CELL *cell;

   if( row < 0 || row >= gt->rows || col < 0 || col >= gt->cols )
      return 0;

   cell = &gt->screenBuffer[( size_t )row * gt->cols + col];
   gt->screenActive = 1;
   gt->dirtyLines[row] = 1;
   cell->character = character;
   cell->color = ( uint8_t )gt->colors[gt->colorIndex];
   cell->attr = attr;
   cell->foreground = foreground;
   cell->background = background;

   return 1;
}

/* Put a character in the current color: SetColorExt() when it is set, else SetColor() */
static int s_gtPutCurrent( X_GT_BASE *gt, int row, int col, uint8_t attr, uint16_t character )
{
   if( gt->foreground != X_GT_RGB_DEFAULT )
      return s_gtPutCharExt( gt, row, col, attr, character, gt->foreground, gt->background );

   return gt->pFuncTable->PutChar( gt, row, col, gt->colors[gt->colorIndex], attr, character );
}

/* Scroll an area by rows and cols; the cells that come in get the character and the color */
static void s_gtScroll( X_GT_BASE *gt, int top, int left, int bottom, int right, int color, uint16_t character,
                        int rows, int cols )
{
   int row, col, first, last, step, sourceRow, sourceCol;
   int inside;
   X_GT_CELL cell;

   top = IIF( top < 0, 0, top );
   left = IIF( left < 0, 0, left );
   bottom = IIF( bottom >= gt->rows, gt->rows - 1, bottom );
   right = IIF( right >= gt->cols, gt->cols - 1, right );
   if( top > bottom || left > right )
      return;

   first = IIF( rows < 0, bottom, top );
   last = IIF( rows < 0, top - 1, bottom + 1 );
   step = IIF( rows < 0, -1, 1 );
   for( row = first; row != last; row += step )
   {
      col = IIF( cols < 0, right, left );
      while( col >= left && col <= right )
      {
         sourceRow = row + rows;
         sourceCol = col + cols;
         cell.character = character;
         cell.color = ( uint8_t )color;
         cell.attr = 0;
         cell.foreground = gt->foreground;
         cell.background = gt->background;

         inside = sourceRow >= top && sourceRow <= bottom && sourceCol >= left && sourceCol <= right;
         if( ( rows || cols ) && inside )
            cell = gt->screenBuffer[( size_t )sourceRow * gt->cols + sourceCol];

         gt->screenBuffer[( size_t )row * gt->cols + col] = cell;
         gt->screenActive = 1;
         gt->dirtyLines[row] = 1;
         col += IIF( cols < 0, -1, 1 );
      }
   }
}

/* The next character of a UTF-8 text as a screen cell; a character above U+FFFF becomes U+FFFD */
static uint16_t s_gtNextCharacter( const X_GT_BASE *gt, const unsigned char *text, size_t length, size_t *offset )
{
   uint32_t codepoint;

   UNUSED( gt );
   x_utf8Decode( ( const char * )text, length, offset, &codepoint );

   return ( uint16_t )IIF( codepoint <= 0xFFFF, codepoint, 0xFFFD );
}

/* A box of frame characters 0 to 7; frame[8] fills the inside when it is not 0 */
static int s_gtBox( X_GT_BASE *gt, int top, int left, int bottom, int right, const uint16_t *frame,
                    uint32_t foreground, uint32_t background )
{
   int row, col, swap, maxRow, maxCol, index;
   int cursorRow = top + 1;
   int cursorCol = left + 1;

   if( top > bottom )
   {
      swap = top;
      top = bottom;
      bottom = swap;
   }

   if( left > right )
   {
      swap = left;
      left = right;
      right = swap;
   }

   maxRow = IIF( bottom < gt->rows, bottom, gt->rows - 1 );
   maxCol = IIF( right < gt->cols, right, gt->cols - 1 );
   for( row = top; row <= maxRow; ++row )
   {
      for( col = left; col <= maxCol; ++col )
      {
         /* A box of one row or one column is a line */
         if( top == bottom )
            index = 1;
         else if( left == right )
            index = 3;
         else
            index = s_gtFrameIndex( row, col, top, left, bottom, right );

         if( index == 8 && !frame[8] )
            continue;

         if( foreground == X_GT_RGB_DEFAULT )
            s_gtPutCurrent( gt, row, col, 1, frame[index] );
         else
            s_gtPutCharExt( gt, row, col, 1, frame[index], foreground, background );
      }
   }

   gt->pFuncTable->SetPos( gt, cursorRow, cursorCol );
   gt->pFuncTable->Flush( gt );

   return gt->outputError;
}

/* Scroll the whole screen up by one line in the current color */
static void s_gtScrollUp( X_GT_BASE *gt )
{
   gt->pFuncTable->Scroll( gt, 0, 0, gt->rows - 1, gt->cols - 1, gt->colors[gt->colorIndex], ' ', 1, 0 );
}

/* Console output at the cursor: CR, LF, backspace, bell and tab work; the screen scrolls at the bottom */
static void s_gtWriteCon( X_GT_BASE *gt, const char *text, size_t length )
{
   size_t i;
   int row = gt->row;
   int col = gt->col;
   int repeat;
   uint16_t ch;

   if( row < 0 )
      row = 0;
   else if( row >= gt->rows )
      row = gt->rows - 1;

   if( col < 0 || col >= gt->cols )
      col = 0;

   for( i = 0; i < length; )
   {
      ch = s_gtNextCharacter( gt, ( const unsigned char * )text, length, &i );
      repeat = 1;
      if( ch == '\r' )
         col = 0;
      else if( ch == '\n' )
      {
         col = 0;
         ++row;
      }
      else if( ch == '\b' )
      {
         if( col > 0 )
            --col;
         else if( row > 0 )
         {
            --row;
            col = gt->cols - 1;
         }

         s_gtPutCurrent( gt, row, col, 0, ' ' );
      }
      else if( ch == 7 )
         gt->pFuncTable->Bell( gt );
      else
      {
         if( ch == '\t' )
         {
            repeat = 8 - col % 8;
            ch = ' ';
         }

         while( repeat-- )
         {
            s_gtPutCurrent( gt, row, col, 0, ch );
            if( ++col >= gt->cols )
            {
               col = 0;
               ++row;
            }

            if( row >= gt->rows )
            {
               s_gtScrollUp( gt );
               row = gt->rows - 1;
            }
         }
      }

      if( row >= gt->rows )
      {
         s_gtScrollUp( gt );
         row = gt->rows - 1;
      }
   }

   gt->pFuncTable->SetPos( gt, row, col );
}

/* Add a key at the end of the keyboard buffer; a full buffer drops its last key */
static void s_gtInkeyPut( X_GT_BASE *gt, int key )
{
   size_t position;

   if( !key )
      return;

   position = ( gt->inkeyHead + gt->inkeyCount ) % X_DEFAULT_INKEY_BUFSIZE;
   if( gt->inkeyCount == X_DEFAULT_INKEY_BUFSIZE )
      position = ( position + X_DEFAULT_INKEY_BUFSIZE - 1 ) % X_DEFAULT_INKEY_BUFSIZE;
   else
      ++gt->inkeyCount;

   gt->inkeyBuffer[position] = key;
}

/* Put a key in front of the keyboard buffer */
static void s_gtInkeyIns( X_GT_BASE *gt, int key )
{
   if( !key )
      return;

   gt->inkeyHead = ( gt->inkeyHead + X_DEFAULT_INKEY_BUFSIZE - 1 ) % X_DEFAULT_INKEY_BUFSIZE;
   gt->inkeyBuffer[gt->inkeyHead] = key;
   if( gt->inkeyCount < X_DEFAULT_INKEY_BUFSIZE )
      ++gt->inkeyCount;
}

/* ---------------------------------------------------------------------------------------------
   Key codes: extended keys to the key codes of inkey.ch
   --------------------------------------------------------------------------------------------- */

/* Alt + a printable character */
static int s_inkeyAltCharacter( int key )
{
   int upper = IIF( key >= 'a' && key <= 'z', key - ( 'a' - 'A' ), key );

   if( upper >= 'A' && upper <= 'Z' )
      return s_inkeyAltLetter[upper - 'A'];

   if( key >= '1' && key <= '9' )
      return K_ALT_1 + key - '1';

   if( key == '0' )
      return K_ALT_0;

   switch( key )
   {
   case '-':
   case '_':
      return K_ALT_MINUS;

   case '=':
      return K_ALT_EQUALS;

   case '[':
   case '{':
      return K_ALT_OSB;

   case ']':
   case '}':
      return K_ALT_CSB;

   case '\\':
   case '|':
      return K_ALT_BACKSLASH;

   case ';':
      return K_ALT_SC;

   case '\'':
      return K_ALT_QUOTE;

   case ',':
      return K_ALT_COMMA;

   case '.':
      return K_ALT_PERIOD;

   case '/':
      return K_ALT_SLASH;

   case '`':
   case '~':
      return K_ALT_BACKQUOTE;

   default:
      return 0;
   }
}

/* Ctrl + a printable character */
static int s_inkeyCtrlCharacter( int key )
{
   int upper = IIF( key >= 'a' && key <= 'z', key - ( 'a' - 'A' ), key );

   if( upper >= 'A' && upper <= 'Z' )
      return upper - 'A' + K_CTRL_A;

   switch( key )
   {
   case '\'':
      return K_CTRL_G;

   case '-':
   case '_':
      return K_CTRL_PGUP;

   case '/':
      return K_CTRL_BS;

   case '?':
      return K_CTRL_QUESTION;

   case '2':
   case '@':
      return 259;

   case '3':
   case '[':
   case '{':
      return K_ESC;

   case '4':
   case '\\':
   case '|':
      return 28;

   case '5':
   case ']':
   case '}':
      return 29;

   case '6':
   case '^':
      return 30;

   case '7':
      return 31;

   case '8':
   case 127:
      return K_CTRL_BS;

   case '`':
   case '~':
      return K_ALT_BACKQUOTE;

   default:
      return 0;
   }
}

/* Alt or Ctrl + a key of the keypad; 0 for another key */
static int s_inkeyKeypad( int key, int flags )
{
   static const int s_ctrlDigit[10] = {
      K_CTRL_INS, K_CTRL_END, K_CTRL_DOWN, K_CTRL_PGDN, K_CTRL_LEFT,
      KP_CTRL_5, K_CTRL_RIGHT, K_CTRL_HOME, K_CTRL_UP, K_CTRL_PGUP
   };

   if( !( flags & KF_KEYPAD ) || !( flags & ( KF_ALT | KF_CTRL ) ) )
      return 0;

   if( key == KX_ENTER && ( flags & KF_ALT ) )
      return KP_ALT_ENTER;

   if( key == '+' )
      return IIF( flags & KF_ALT, KP_ALT_PLUS, KP_CTRL_PLUS );

   if( key == '-' )
      return IIF( flags & KF_ALT, KP_ALT_MINUS, KP_CTRL_MINUS );

   if( key == '*' )
      return IIF( flags & KF_ALT, KP_ALT_ASTERISK, KP_CTRL_ASTERISK );

   if( key == '/' )
      return IIF( flags & KF_ALT, KP_ALT_SLASH, KP_CTRL_SLASH );

   if( ( key == '.' || key == ',' ) && ( flags & KF_CTRL ) )
      return K_CTRL_DEL;

   if( key >= '0' && key <= '9' && ( flags & KF_CTRL ) )
      return s_ctrlDigit[key - '0'];

   return 0;
}

/* Alt or Ctrl + a character from 32 to 127; 0 when the character has no such key */
static int s_inkeyModified( int value, int flags )
{
   int translated = 0;

   if( value < 32 || value > 127 )
      return 0;

   if( flags & KF_ALT )
      translated = s_inkeyAltCharacter( value );

   if( translated == 0 && ( flags & KF_CTRL ) )
      translated = s_inkeyCtrlCharacter( value );

   return translated;
}

/* The key code of an extended key */
int x_inkeyKeyStd( int key )
{
   int flags, value, translated;

   if( !X_INKEY_ISEXT( key ) )
      return key;

   if( X_INKEY_ISMOUSEPOS( key ) )
      return K_MOUSEMOVE;

   if( X_INKEY_ISMOUSEKEY( key ) || X_INKEY_ISEVENT( key ) )
      return X_INKEY_VALUE( key );

   flags = X_INKEY_FLAGS( key );
   value = X_INKEY_VALUE( key );
   if( X_INKEY_ISKEY( key ) )
   {
      translated = s_inkeyKeypad( value, flags );
      if( translated )
         return translated;

      if( value > 0 && value <= ( int )( sizeof( s_inkeySpecial ) / sizeof( s_inkeySpecial[0] ) ) )
      {
         const X_INKEY_TRANSLATION *entry = &s_inkeySpecial[value - 1];

         if( ( flags & KF_ALT ) && entry->alt )
            return entry->alt;

         if( ( flags & KF_CTRL ) && entry->ctrl )
            return entry->ctrl;

         if( ( flags & KF_SHIFT ) && entry->shift )
            return entry->shift;

         return entry->plain;
      }

      translated = s_inkeyModified( value, flags );

      return IIF( translated, translated, value );
   }

   if( X_INKEY_ISCHAR( key ) || X_INKEY_ISUNICODE( key ) )
   {
      translated = s_inkeyModified( value, flags );
      if( translated )
         return translated;

      return IIF( value < 128, value, key );
   }

   return value;
}

/* The event kind of a key */
static int s_gtInkeyEvent( int key )
{
   int value = IIF( X_INKEY_ISEXT( key ), X_INKEY_VALUE( key ), key );

   if( X_INKEY_ISMOUSEPOS( key ) )
      return INKEY_MOVE;

   if( X_INKEY_ISEXT( key ) && !X_INKEY_ISMOUSEKEY( key ) && !X_INKEY_ISEVENT( key ) )
      return INKEY_KEYBOARD;

   switch( value )
   {
   case K_MOUSEMOVE:
   case K_NCMOUSEMOVE:
      return INKEY_MOVE;

   case K_LBUTTONDOWN:
   case K_LDBLCLK:
      return INKEY_LDOWN;

   case K_LBUTTONUP:
      return INKEY_LUP;

   case K_RBUTTONDOWN:
   case K_RDBLCLK:
      return INKEY_RDOWN;

   case K_RBUTTONUP:
      return INKEY_RUP;

   case K_MBUTTONDOWN:
   case K_MBUTTONUP:
   case K_MDBLCLK:
      return INKEY_MMIDDLE;

   case K_MWFORWARD:
   case K_MWBACKWARD:
   case K_MWLEFT:
   case K_MWRIGHT:
      return INKEY_MWHEEL;

   case K_RESIZE:
   case K_CLOSE:
   case K_GOTFOCUS:
   case K_LOSTFOCUS:
   case K_CONNECT:
   case K_DISCONNECT:
   case K_TERMINATE:
   case K_MENU:
      return INKEY_GTEVENT;

   default:
      return INKEY_KEYBOARD;
   }
}

/* The key when the event mask lets it through, as a key without INKEY_EXT; 0 otherwise */
static int s_gtInkeyFilter( int key, int mask )
{
   if( !( mask & s_gtInkeyEvent( key ) ) )
      return 0;

   return IIF( X_INKEY_ISEXT( key ) && !( mask & INKEY_EXT ), x_inkeyKeyStd( key ), key );
}

/* ---------------------------------------------------------------------------------------------
   Keyboard buffer
   --------------------------------------------------------------------------------------------- */

/* Read a key from the driver into the buffer */
static void s_gtInkeyPoll( X_GT_BASE *gt )
{
   int key = gt->pFuncTable->ReadKey( gt, INKEY_ALLEVENTS | INKEY_EXT );

   if( key == K_RESIZE )
      gt->pFuncTable->Flush( gt );

   /* SET CANCEL ON: Alt+C ends the program */
   if( x_setCancel && key && x_inkeyKeyStd( key ) == K_ALT_C )
      x_quitRaise();

   /* an arrow key that moved a window with Scroll Lock on does not reach the program */
   if( key && !x_ctMoveKey( key ) )
      gt->pFuncTable->InkeyPut( gt, key );
}

/* Drop the first key: of the text of KEYBOARD, else of the buffer */
static void s_gtInkeyPop( X_GT_BASE *gt )
{
   if( gt->inkeyText )
   {
      if( ++gt->inkeyTextPos == gt->inkeyTextLength )
      {
         x_xfree( gt->inkeyText );
         gt->inkeyText = NULL;
         gt->inkeyTextLength = 0;
         gt->inkeyTextPos = 0;
      }
   }
   else
   {
      gt->inkeyHead = ( gt->inkeyHead + 1 ) % X_DEFAULT_INKEY_BUFSIZE;
      --gt->inkeyCount;
   }
}

/* The first key that the mask lets through; a key it stops is dropped */
static int s_gtInkeyPeek( X_GT_BASE *gt, int mask )
{
   int key;

   if( gt->inkeyText )
   {
      key = gt->inkeyText[gt->inkeyTextPos];
      if( key >= 128 )
         key = X_INKEY_NEW_UNICODE( key );
   }
   else if( gt->inkeyCount )
      key = gt->inkeyBuffer[gt->inkeyHead];
   else
      return 0;

   key = s_gtInkeyFilter( key, mask );
   if( !key )
      s_gtInkeyPop( gt );

   return key;
}

/* NextKey() */
static int s_gtInkeyNext( X_GT_BASE *gt, int mask )
{
   gt->pFuncTable->InkeyPoll( gt );

   return s_gtInkeyPeek( gt, mask );
}

/* LastKey() */
static int s_gtInkeyLast( X_GT_BASE *gt, int mask )
{
   gt->pFuncTable->InkeyPoll( gt );

   return s_gtInkeyFilter( gt->inkeyLast, mask );
}

/* Set LastKey(); returns the previous key */
static int s_gtInkeySetLast( X_GT_BASE *gt, int key )
{
   int previous = gt->inkeyLast;

   gt->inkeyLast = key;

   return previous;
}

/* CLEAR TYPEAHEAD: the buffer and the text of KEYBOARD are emptied */
static void s_gtInkeyReset( X_GT_BASE *gt )
{
   gt->inkeyHead = 0;
   gt->inkeyCount = 0;
   if( gt->inkeyText )
   {
      x_xfree( gt->inkeyText );
      gt->inkeyText = NULL;
   }

   gt->inkeyTextLength = 0;
   gt->inkeyTextPos = 0;
}

/* KEYBOARD: the characters of a UTF-8 text come as keys; ';' (or a line end with eol) is Enter */
static void s_gtInkeySetText( X_GT_BASE *gt, const char *text, size_t length, int eol )
{
   uint32_t *buffer = NULL;
   uint32_t previous = 0;
   uint32_t character;
   size_t offset = 0;
   size_t count = 0;

   if( text && length )
   {
      if( length > SIZE_MAX / sizeof( uint32_t ) )
      {
         X_ITEM error __attribute__( ( cleanup( x_itemClear ) ) ) = X_ITEM_NIL_INIT;

         x_errorRaiseNamed( &error, 3000, "x_inkeySetText" );
         x_itemClear( &error );
         return;
      }

      buffer = ( uint32_t * )x_xgrabArray( length, sizeof( uint32_t ) );
      while( offset < length )
      {
         if( !x_utf8Decode( text, length, &offset, &character ) )
            break;

         if( eol && character == '\n' )
         {
            /* CR LF is one Enter */
            if( previous == '\r' )
            {
               previous = 0;
               continue;
            }

            character = '\r';
         }
         else if( !eol && character == ';' )
            character = '\r';

         buffer[count++] = character;
         previous = character;
      }
   }

   if( gt->inkeyText )
      x_xfree( gt->inkeyText );

   gt->inkeyText = buffer;
   gt->inkeyTextLength = count;
   gt->inkeyTextPos = 0;
}

/* Seconds of a monotonic clock; -1 when there is none */
static double s_gtInkeyTime( void )
{
#if X_OS_WINDOWS
   return ( double )GetTickCount64() / 1000.0;
#else
   struct timespec now;

   if( clock_gettime( CLOCK_MONOTONIC, &now ) != 0 )
      return -1.0;

   return ( double )now.tv_sec + ( double )now.tv_nsec / 1000000000.0;
#endif
}

/* Wait less than a second */
static void s_gtInkeySleep( double seconds )
{
#if X_OS_WINDOWS
   DWORD milliseconds = ( DWORD )( seconds * 1000.0 );

   Sleep( IIF( milliseconds > 0, milliseconds, 1 ) );
#else
   struct timespec pause;

   pause.tv_sec = 0;
   pause.tv_nsec = ( long )( seconds * 1000000000.0 );
   nanosleep( &pause, NULL );
#endif
}

static void s_gtInkeyBlockEnd( int **active )
{
   **active = 0;
}

/* Evaluate the block of GTI_INKEYREAD (no argument) or GTI_INKEYFILTER (the key); its number is the key */
static int s_gtInkeyBlock( X_ITEM *code, int *active, int key, int filter )
{
   int *guard __attribute__( ( cleanup( s_gtInkeyBlockEnd ) ) ) = active;
   X_ITEM block __attribute__( ( cleanup( x_itemClear ) ) ) = X_ITEM_NIL_INIT;
   X_ITEM result __attribute__( ( cleanup( x_itemClear ) ) ) = X_ITEM_NIL_INIT;
   X_ITEM value = { X_IT_INTEGER, { .integer = key } };
   X_ARG argument = { &value, 0 };
   X_CALL_CTX context = { 0 };
   double number;

   *active = 1;
   x_itemCopy( &block, code );
   x_evalBlock( &context, &result, &block, IIF( filter, 1, 0 ), IIF( filter, &argument, NULL ) );
   number = x_itemGetND( &result );

   return IIF( isfinite( number ) && number >= INT_MIN && number <= INT_MAX, ( int )number, 0 );
}

/* Inkey(): the next key, waiting with wait (seconds 0 waits for ever) */
static int s_gtInkeyGet( X_GT_BASE *gt, int wait, double seconds, int mask )
{
   int timed = wait && seconds >= 0.01;
   int key;
   double started = 0.0;
   double now, remaining, pause;

   if( wait && !isfinite( seconds ) )
      return 0;

   gt->pFuncTable->Flush( gt );
   if( gt->inkeyReadBlock && !gt->inkeyReadActive )
   {
      key = s_gtInkeyBlock( gt->inkeyReadBlock, &gt->inkeyReadActive, 0, 0 );
      if( gt != x_ctScreen( s_gtCurrent ) )
         return 0;

      key = s_gtInkeyFilter( key, mask );
      if( key )
      {
         gt->inkeyLast = key;
         return key;
      }
   }

   if( timed )
   {
      started = s_gtInkeyTime();
      if( started < 0 )
         return 0;
   }

   for( ;; )
   {
      gt->pFuncTable->InkeyPoll( gt );
      key = s_gtInkeyPeek( gt, mask );
      if( key )
      {
         s_gtInkeyPop( gt );
         gt->inkeyLast = key;
         if( gt->inkeyFilterBlock && !gt->inkeyFilterActive )
         {
            key = s_gtInkeyBlock( gt->inkeyFilterBlock, &gt->inkeyFilterActive, key, 1 );
            if( gt != x_ctScreen( s_gtCurrent ) )
               return 0;

            gt->inkeyLast = key;
         }

         if( key )
            return key;
      }

      if( !wait )
         return 0;

      pause = 0.005;
      if( timed )
      {
         now = s_gtInkeyTime();
         if( now < 0 )
            return 0;

         remaining = seconds - ( now - started );
         if( remaining <= 0 )
            return 0;

         pause = IIF( remaining < pause, remaining, pause );
      }

      s_gtInkeySleep( pause );
   }
}

/* Are two driver names equal, ignoring case? */
static int s_gtNameEqual( const char *left, const char *right )
{
   while( *left && toupper( ( unsigned char )*left ) == toupper( ( unsigned char )*right ) )
   {
      ++left;
      ++right;
   }

   return *left == *right;
}

/* --------------------------------------------------------------------------------------------
   General Terminal
   -------------------------------------------------------------------------------------------- */

/* Register a driver; 0 when the name is taken or the table is full */
int x_gtRegister( const X_GT_INIT *gtInit )
{
   int i;

   for( i = 0; i < s_gtCount; ++i )
   {
      if( s_gtNameEqual( s_gtRegistry[i]->id, gtInit->id ) )
         return 0;
   }

   if( s_gtCount == ( int )( sizeof( s_gtRegistry ) / sizeof( s_gtRegistry[0] ) ) )
      return 0;

   if( gtInit->pGtId )
      *gtInit->pGtId = s_gtCount;

   s_gtRegistry[s_gtCount++] = gtInit;

   return 1;
}

/* A screen with the functions of a driver over the base driver, or a driver added to a screen */
X_GT_BASE *x_gtLoad( const char *gtName, X_GT_BASE *gt, X_GT_FUNCS *superTable )
{
   X_GT_FUNCS core = {
      .Init = s_gtInit,
      .Exit = s_gtExit,
      .Resize = s_gtResize,
      .SetMode = s_gtSetMode,
      .GetSize = s_gtGetSize,
      .SemiCold = s_gtSemiCold,
      .ExposeArea = s_gtExposeArea,
      .Redraw = s_gtRedraw,
      .RedrawDiff = s_gtRedrawDiff,
      .Refresh = s_gtRefresh,
      .Flush = s_gtFlush,
      .SetPos = s_gtSetPos,
      .SetCursorStyle = s_gtSetCursorStyle,
      .GetScrCursor = s_gtGetScrCursor,
      .GetScrChar = s_gtGetScrChar,
      .DispBegin = s_gtDispBegin,
      .DispEnd = s_gtDispEnd,
      .DispCount = s_gtDispCount,
      .PutChar = s_gtPutChar,
      .WriteCon = s_gtWriteCon,
      .Scroll = s_gtScroll,
      .Version = s_gtVersion,
      .Suspend = s_gtSuspend,
      .Resume = s_gtResume,
      .Bell = s_gtBell,
      .Info = s_gtInfo,
      .Alert = s_gtAlert,
      .SetFlag = s_gtSetFlag,
      .ReadKey = s_gtReadKey,
      .InkeyGet = s_gtInkeyGet,
      .InkeyPut = s_gtInkeyPut,
      .InkeyIns = s_gtInkeyIns,
      .InkeyLast = s_gtInkeyLast,
      .InkeyNext = s_gtInkeyNext,
      .InkeyPoll = s_gtInkeyPoll,
      .InkeyReset = s_gtInkeyReset,
      .InkeySetText = s_gtInkeySetText,
      .InkeySetLast = s_gtInkeySetLast,
      .MouseIsPresent = s_gtMouseIsPresent,
      .MouseGetPos = s_gtMouseGetPos,
      .MouseSetPos = s_gtMouseSetPos,
      .MouseGetDoubleClickSpeed = s_gtMouseGetDoubleClickSpeed,
      .MouseCountButton = s_gtMouseCountButton,
      .MouseButtonState = s_gtMouseButtonState,
      .GfxPrimitive = s_gtGfxPrimitive,
      .GfxText = s_gtGfxText,
      .TrueColor = s_gtTrueColor
   };
   X_GT_FUNCS previous;
   const X_GT_INIT *driver;
   int i;
   int isNew;

   if( !gtName )
      return NULL;

   for( i = 0; i < s_gtCount; ++i )
   {
      if( s_gtNameEqual( s_gtRegistry[i]->id, gtName ) )
         break;
   }

   if( i == s_gtCount )
      return NULL;

   driver = s_gtRegistry[i];
   isNew = gt == NULL;
   if( !isNew && ( gt->drivers & ( 1u << i ) ) )
      return NULL;

   if( isNew )
   {
      gt = ( X_GT_BASE * )x_xgrabz( sizeof( X_GT_BASE ) );
      gt->references = 1;
      gt->mouseDoubleClickSpeed = 500;
      gt->pFuncTable = ( X_GT_FUNCS * )x_xgrab( sizeof( X_GT_FUNCS ) );
      *gt->pFuncTable = core;
   }

   previous = *gt->pFuncTable;
   if( !superTable )
      superTable = driver->pSuperTable;

   if( superTable )
      *superTable = previous;

   if( driver->init( gt->pFuncTable ) )
   {
      gt->drivers |= 1u << i;
      return gt;
   }

   *gt->pFuncTable = previous;
   if( isNew )
      s_gtExit( gt );

   return NULL;
}

/* A new screen of a driver */
void *x_gtCreate( const char *gtName, int inputHandle, int outputHandle, int errorHandle )
{
   X_GT_BASE *gt = x_gtLoad( gtName, NULL, NULL );

   if( gt )
      gt->pFuncTable->Init( gt, inputHandle, outputHandle, errorHandle );

   return gt;
}

/* Make a screen current; returns the previous one */
void *x_gtSwap( void *handle )
{
   X_GT_BASE *previous = s_gtCurrent;

   s_gtCurrent = ( X_GT_BASE * )handle;

   return previous;
}

/* Hold a screen, the current one for NULL */
void *x_gtAlloc( void *handle )
{
   X_GT_BASE *gt = IIF( handle, ( X_GT_BASE * )handle, s_gtCurrent );

   if( gt )
      ++gt->references;

   return gt;
}

/* Make a screen current and release the previous one */
void x_gtAttach( void *handle )
{
   if( handle != s_gtCurrent )
   {
      x_gtRelease( NULL );
      s_gtCurrent = ( X_GT_BASE * )handle;
   }
}

/* Release a screen, the current one for NULL; the last release ends it */
void x_gtRelease( void *handle )
{
   X_GT_BASE *gt = IIF( handle, ( X_GT_BASE * )handle, s_gtCurrent );

   if( gt && --gt->references == 0 )
   {
      if( gt == s_gtCurrent )
         s_gtCurrent = NULL;

      gt->dispCount = 0;
      gt->pFuncTable->Flush( gt );
      gt->pFuncTable->Exit( gt );
   }
}

/* Start the screen of XDEV_GT or of the first registered driver; 1 when it cannot start */
int x_gtInit( int inputHandle, int outputHandle, int errorHandle )
{
   const char *name;

   if( s_gtCurrent )
      return 0;

   name = getenv( "XDEV_GT" );
   if( !name || !*name )
   {
      if( s_gtCount == 0 )
         return 0;

      name = s_gtRegistry[0]->id;
   }

   s_gtCurrent = ( X_GT_BASE * )x_gtCreate( name, inputHandle, outputHandle, errorHandle );
   if( !s_gtCurrent )
      fprintf( stderr, "xDev: cannot load GT %s%s", name, x_eol() );

   return IIF( s_gtCurrent, 0, 1 );
}

/* The current GT; while CT windows are open it is the GT of the current window */
X_GT_BASE *x_gtCurrent( void )
{
   return s_gtCurrent;
}

/* End the screen */
int x_gtExit( void )
{
   x_ctExit();
   x_gtRelease( NULL );
   s_gtCurrent = NULL;

   return 0;
}

/* Change the driver of the program */
int x_gtReload( const char *gtName, int inputHandle, int outputHandle, int errorHandle )
{
   X_GT_BASE *gt = x_gtLoad( gtName, NULL, NULL );

   if( !gt )
      return 0;

   x_ctExit();
   x_gtRelease( NULL );
   s_gtCurrent = gt;
   gt->pFuncTable->Init( gt, inputHandle, outputHandle, errorHandle );

   return 1;
}

/* Extract a character from the keyboard buffer or a mouse event; non-blocking, returns 0 if nothing is pending */
int x_gtReadKey( int eventMask )
{
   if( !s_gtCurrent )
      return 0;

   return s_gtCurrent->pFuncTable->ReadKey( s_gtCurrent, eventMask );
}

/* Inkey() */
int x_inkey( int wait, double seconds, int eventMask )
{
   X_GT_BASE *gt __attribute__( ( cleanup( s_gtReleaseHeld ) ) ) = ( X_GT_BASE * )x_gtAlloc( NULL );

   return IIF( gt, gt->pFuncTable->InkeyGet( gt, wait, seconds, eventMask ), 0 );
}

/* Add a key at the end of the keyboard buffer */
void x_inkeyPut( int key )
{
   if( s_gtCurrent )
      s_gtCurrent->pFuncTable->InkeyPut( s_gtCurrent, key );
}

/* Put a key in front of the keyboard buffer */
void x_inkeyIns( int key )
{
   if( s_gtCurrent )
      s_gtCurrent->pFuncTable->InkeyIns( s_gtCurrent, key );
}

/* LastKey() */
int x_inkeyLast( int eventMask )
{
   if( !s_gtCurrent )
      return s_inkeyLastNoGt;

   return s_gtCurrent->pFuncTable->InkeyLast( s_gtCurrent, eventMask );
}

/* NextKey() */
int x_inkeyNext( int eventMask )
{
   if( !s_gtCurrent )
      return 0;

   return s_gtCurrent->pFuncTable->InkeyNext( s_gtCurrent, eventMask );
}

/* Read a key from the driver into the buffer */
void x_inkeyPoll( void )
{
   if( s_gtCurrent )
      s_gtCurrent->pFuncTable->InkeyPoll( s_gtCurrent );
}

/* CLEAR TYPEAHEAD */
void x_inkeyReset( void )
{
   if( s_gtCurrent )
      s_gtCurrent->pFuncTable->InkeyReset( s_gtCurrent );
}

/* KEYBOARD */
void x_inkeySetText( const char *text, size_t length, int eol )
{
   if( s_gtCurrent )
      s_gtCurrent->pFuncTable->InkeySetText( s_gtCurrent, text, length, eol );
}

/* Set LastKey(); returns the previous key */
int x_inkeySetLast( int key )
{
   if( !s_gtCurrent )
   {
      int previous = s_inkeyLastNoGt;

      s_inkeyLastNoGt = key;
      return previous;
   }

   return s_gtCurrent->pFuncTable->InkeySetLast( s_gtCurrent, key );
}

/* The 9 frame characters of a UTF-8 box string: missing sides repeat the last character given;
   an empty string gives a frame of spaces that clears the inside */
static void s_gtFrameFromText( uint16_t frame[9], const unsigned char *boxString )
{
   size_t length = IIF( boxString, strlen( ( const char * )boxString ), 0 );
   size_t offset = 0;
   int count = 0;
   uint16_t pad = ' ';

   while( offset < length && count < 9 )
   {
      frame[count] = s_gtNextCharacter( s_gtCurrent, boxString, length, &offset );
      pad = frame[count++];
   }

   if( count == 0 )
      frame[8] = ' ';

   while( count < 8 )
      frame[count++] = pad;
}

/* Draw a box on the screen */
int x_gtBox( unsigned short top, unsigned short left, unsigned short bottom, unsigned short right,
             unsigned char *boxString )
{
   uint16_t frame[9] = { 0 };

   if( !s_gtCurrent )
      return 1;

   s_gtFrameFromText( frame, boxString );

   return s_gtBox( s_gtCurrent, top, left, bottom, right, frame, X_GT_RGB_DEFAULT, X_GT_RGB_DEFAULT );
}

/* Draw a double-line box on the screen */
int x_gtBoxD( unsigned short top, unsigned short left, unsigned short bottom, unsigned short right )
{
   static const uint16_t frame[9] = { 0x2554, 0x2550, 0x2557, 0x2551, 0x255D, 0x2550, 0x255A, 0x2551, 0 };

   if( !s_gtCurrent )
      return 1;

   return s_gtBox( s_gtCurrent, top, left, bottom, right, frame, X_GT_RGB_DEFAULT, X_GT_RGB_DEFAULT );
}

/* Draw a single-line box on the screen */
int x_gtBoxS( unsigned short top, unsigned short left, unsigned short bottom, unsigned short right )
{
   static const uint16_t frame[9] = { 0x250C, 0x2500, 0x2510, 0x2502, 0x2518, 0x2500, 0x2514, 0x2502, 0 };

   if( !s_gtCurrent )
      return 1;

   return s_gtBox( s_gtCurrent, top, left, bottom, right, frame, X_GT_RGB_DEFAULT, X_GT_RGB_DEFAULT );
}

/* Draw a box in 24-bit colors */
int x_gtBoxExt( unsigned short top, unsigned short left, unsigned short bottom, unsigned short right,
                unsigned char *boxString, uint32_t foreground, uint32_t background )
{
   uint16_t frame[9] = { 0 };

   if( !s_gtCurrent || !s_gtCurrent->pFuncTable->TrueColor( s_gtCurrent ) )
      return 1;

   s_gtFrameFromText( frame, boxString );

   return s_gtBox( s_gtCurrent, top, left, bottom, right, frame, foreground, background );
}

/* Draw a double-line box in 24-bit colors */
int x_gtBoxDExt( unsigned short top, unsigned short left, unsigned short bottom, unsigned short right,
                 uint32_t foreground, uint32_t background )
{
   static const uint16_t frame[9] = { 0x2554, 0x2550, 0x2557, 0x2551, 0x255D, 0x2550, 0x255A, 0x2551, 0 };

   if( !s_gtCurrent || !s_gtCurrent->pFuncTable->TrueColor( s_gtCurrent ) )
      return 1;

   return s_gtBox( s_gtCurrent, top, left, bottom, right, frame, foreground, background );
}

/* Draw a single-line box in 24-bit colors */
int x_gtBoxSExt( unsigned short top, unsigned short left, unsigned short bottom, unsigned short right,
                 uint32_t foreground, uint32_t background )
{
   static const uint16_t frame[9] = { 0x250C, 0x2500, 0x2510, 0x2502, 0x2518, 0x2500, 0x2514, 0x2502, 0 };

   if( !s_gtCurrent || !s_gtCurrent->pFuncTable->TrueColor( s_gtCurrent ) )
      return 1;

   return s_gtBox( s_gtCurrent, top, left, bottom, right, frame, foreground, background );
}

/* Activate an attribute in current xDev color setting */
int x_gtColorSelect( unsigned short colorIndex )
{
   if( !s_gtCurrent || colorIndex >= s_gtCurrent->colorCount )
      return 1;

   s_gtCurrent->colorIndex = colorIndex;

   return 0;
}

/* Begin buffering screen output */
int x_gtDispBegin( void )
{
   if( !s_gtCurrent || s_gtCurrent->dispCount == UINT_MAX )
      return 1;

   s_gtCurrent->pFuncTable->DispBegin( s_gtCurrent );

   return 0;
}

/* Return the number of pending x_gtDispEnd() requests */
unsigned short x_gtDispCount( void )
{
   return IIF( s_gtCurrent, ( unsigned short )s_gtCurrent->pFuncTable->DispCount( s_gtCurrent ), 0 );
}

/* Release the display buffer */
int x_gtDispEnd( void )
{
   if( !s_gtCurrent )
      return 1;

   s_gtCurrent->pFuncTable->DispEnd( s_gtCurrent );

   return s_gtCurrent->outputError;
}

/* Is '*' in a color blinking? */
int x_gtGetBlink( int *blink )
{
   *blink = s_gtCurrent->blink;

   return 0;
}

/* A color attribute as a color pair, e.g. 30 as "W+/B"; returns like snprintf() */
int x_gtColorName( int color, char *buffer, size_t size )
{
   static const char *names[8] = { "N", "B", "G", "BG", "R", "RB", "GR", "W" };

   const char *bright = IIF( color & 8, "+", "" );
   const char *blink = IIF( color & 128, "*", "" );

   return snprintf( buffer, size, "%s%s%s/%s", names[color & 7], bright, blink, names[( color >> 4 ) & 7] );
}

/* Get the xDev color attributes setting */
int x_gtGetColorStr( unsigned char *colorString )
{
   char *text = ( char * )colorString;
   unsigned int i;
   int written;
   size_t remaining = X_CLRSTR_LEN;

   if( !s_gtCurrent )
      return 1;

   for( i = 0; i < s_gtCurrent->colorCount; ++i )
   {
      written = IIF( i, snprintf( text, remaining, "," ), 0 );
      if( written >= 0 && ( size_t )written < remaining )
         written += x_gtColorName( s_gtCurrent->colors[i], text + written, remaining - ( size_t )written );

      if( written < 0 || ( size_t )written >= remaining )
      {
         *text = '\0';
         break;
      }

      text += written;
      remaining -= ( size_t )written;
   }

   return 0;
}

/* Get the current cursor shape */
int x_gtGetCursor( unsigned short *cursorShape )
{
   if( !s_gtCurrent )
      return 1;

   *cursorShape = ( unsigned short )s_gtCurrent->cursorStyle;

   return 0;
}

/* Get the cursor location */
int x_gtGetPos( unsigned short *row, unsigned short *col )
{
   if( !s_gtCurrent )
      return 1;

   *row = ( unsigned short )s_gtCurrent->row;
   *col = ( unsigned short )s_gtCurrent->col;

   return 0;
}

/* Determine if current screen display has color capability */
int x_gtIsColor( void )
{
   return s_gtCurrent != NULL;
}

/* Determine the maximum screen column */
unsigned short x_gtMaxCol( void )
{
   unsigned short rows, cols;

   return IIF( x_gtScrDim( &rows, &cols ) == 0, cols - 1, 0 );
}

/* Determine the maximum screen row */
unsigned short x_gtMaxRow( void )
{
   unsigned short rows, cols;

   return IIF( x_gtScrDim( &rows, &cols ) == 0, rows - 1, 0 );
}

/* Return control to the General Terminal system */
int x_gtPostExt( void )
{
   size_t count;

   if( !s_gtCurrent )
      return 1;

   count = ( size_t )s_gtCurrent->rows * s_gtCurrent->cols;
   memset( s_gtCurrent->prevBuffer, 0xFF, count * sizeof( X_GT_CELL ) );
   memset( s_gtCurrent->dirtyLines, 1, ( size_t )s_gtCurrent->rows );
   s_gtCurrent->cursorDirty = 1;
   s_gtCurrent->pFuncTable->Flush( s_gtCurrent );

   return s_gtCurrent->outputError;
}

/* Prepare the area manager for manipulation of the terminal */
int x_gtPreExt( void )
{
   if( !s_gtCurrent )
      return 1;

   s_gtCurrent->pFuncTable->Refresh( s_gtCurrent );

   return s_gtCurrent->outputError;
}

/* Determine the buffer size needed to save a screen region */
int x_gtRectSize( unsigned short top, unsigned short left, unsigned short bottom, unsigned short right,
                  size_t *bufferSize )
{
   size_t rows, cols;

   *bufferSize = 0;
   if( top > bottom || left > right )
      return 1;

   rows = ( size_t )bottom - top + 1;
   cols = ( size_t )right - left + 1;
   if( rows > SIZE_MAX / X_GT_SAVED_CELL / cols )
      return 1;

   *bufferSize = rows * cols * X_GT_SAVED_CELL;

   return 0;
}

/* Replicate a character to the screen */
int x_gtRepChar( unsigned short row, unsigned short column, unsigned short character, unsigned short count )
{
   int col, end;

   if( !s_gtCurrent )
      return 1;

   end = IIF( ( int )column + count < s_gtCurrent->cols, ( int )column + count, s_gtCurrent->cols );
   for( col = column; col < end; ++col )
      s_gtPutCurrent( s_gtCurrent, row, col, 0, character );

   s_gtCurrent->pFuncTable->Flush( s_gtCurrent );

   return s_gtCurrent->outputError;
}

/* The color attribute, the frame flag and the character of one screen cell; 0 outside the screen */
int x_gtGetCell( int row, int col, int *color, uint8_t *attr, uint16_t *character )
{
   if( !s_gtCurrent || row < 0 || col < 0 || row >= s_gtCurrent->rows || col >= s_gtCurrent->cols )
      return 0;

   return s_gtCurrent->pFuncTable->GetScrChar( s_gtCurrent, row, col, color, attr, character );
}

/* Writes one screen cell; the change shows after x_gtFlush() */
void x_gtPutCell( int row, int col, int color, uint8_t attr, uint16_t character )
{
   if( s_gtCurrent && row >= 0 && col >= 0 && row < s_gtCurrent->rows && col < s_gtCurrent->cols )
      s_gtCurrent->pFuncTable->PutChar( s_gtCurrent, row, col, color, attr, character );
}

/* Show the changes */
void x_gtFlush( void )
{
   if( s_gtCurrent )
      s_gtCurrent->pFuncTable->Flush( s_gtCurrent );
}

/* Display a saved screen region at a specified location */
int x_gtRest( unsigned short top, unsigned short left, unsigned short bottom, unsigned short right,
              void *buffer )
{
   if( !s_gtCurrent || top > bottom || left > right )
      return 1;

   s_gtRestBuffer( s_gtCurrent, top, left, bottom, right, ( const unsigned char * )buffer );
   s_gtCurrent->pFuncTable->Flush( s_gtCurrent );

   return s_gtCurrent->outputError;
}

/* Save a screen region for later display */
int x_gtSave( unsigned short top, unsigned short left, unsigned short bottom, unsigned short right,
              void *buffer )
{
   if( !s_gtCurrent || top > bottom || left > right )
      return 1;

   s_gtSaveBuffer( s_gtCurrent, top, left, bottom, right, ( unsigned char * )buffer );

   return 0;
}

/* Save the whole screen and the cursor position, target of SAVE SCREEN */
int x_gtSaveScreen( void )
{
   X_GT_BASE *gt = s_gtCurrent;
   size_t size;

   if( !gt )
      return 1;

   x_gtGetPos( &gt->savedRow, &gt->savedCol );
   gt->savedMaxRow = x_gtMaxRow();
   gt->savedMaxCol = x_gtMaxCol();
   x_gtRectSize( 0, 0, gt->savedMaxRow, gt->savedMaxCol, &size );
   x_xfree( gt->savedScreen );
   gt->savedScreen = ( char * )x_xgrab( size );

   return x_gtSave( 0, 0, gt->savedMaxRow, gt->savedMaxCol, gt->savedScreen );
}

/* Restore the screen saved by x_gtSaveScreen(), target of RESTORE SCREEN */
int x_gtRestScreen( void )
{
   X_GT_BASE *gt = s_gtCurrent;

   if( !gt || !gt->savedScreen )
      return 1;

   x_gtRest( 0, 0, gt->savedMaxRow, gt->savedMaxCol, gt->savedScreen );
   x_xfree( gt->savedScreen );
   gt->savedScreen = NULL;

   return x_gtSetPos( gt->savedRow, gt->savedCol );
}

/* Get the current screen dimensions */
int x_gtScrDim( unsigned short *height, unsigned short *width )
{
   int rows, cols;

   if( !s_gtCurrent )
      return 1;

   s_gtCurrent->pFuncTable->GetSize( s_gtCurrent, &rows, &cols );
   *height = ( unsigned short )rows;
   *width = ( unsigned short )cols;

   return 0;
}

/* Scroll a region of the screen */
int x_gtScroll( unsigned short top, unsigned short left, unsigned short bottom, unsigned short right,
                int rows, int cols )
{
   int color;

   if( !s_gtCurrent || top > bottom || left > right )
      return 1;

   color = s_gtCurrent->colors[s_gtCurrent->colorIndex];
   s_gtCurrent->pFuncTable->Scroll( s_gtCurrent, top, left, bottom, right, color, ' ', rows, cols );
   s_gtCurrent->pFuncTable->Flush( s_gtCurrent );

   return s_gtCurrent->outputError;
}

/* Scroll() with a color (a color string or an attribute) and a fill character (a string or a code) */
int x_gtScrollEx( unsigned short top, unsigned short left, unsigned short bottom, unsigned short right,
                  int rows, int cols, const X_ITEM *color, const X_ITEM *fill )
{
   int attr;
   uint16_t ch = ' ';

   if( !s_gtCurrent || top > bottom || left > right )
      return 1;

   attr = s_gtCurrent->colors[s_gtCurrent->colorIndex];
   if( color != NULL && color->nType == X_IT_STRING )
   {
      /* The color string is parsed by SetColor() and the colors are set back */
      int colors[16];
      unsigned int count = s_gtCurrent->colorCount;
      unsigned int index = s_gtCurrent->colorIndex;
      int result;

      memcpy( colors, s_gtCurrent->colors, sizeof( colors ) );
      result = x_gtSetColorStr( ( unsigned char * )color->u.string.data );

      attr = s_gtCurrent->colors[0];
      memcpy( s_gtCurrent->colors, colors, sizeof( colors ) );
      s_gtCurrent->colorCount = count;
      s_gtCurrent->colorIndex = index;
      if( result != 0 )
         return result;
   }
   else if( color != NULL && color->nType == X_IT_INTEGER )
      attr = ( int )color->u.integer;
   else if( color != NULL && color->nType == X_IT_DOUBLE )
      attr = ( int )color->u.dbl;

   if( attr < 0 )
      attr = s_gtCurrent->colors[s_gtCurrent->colorIndex];

   if( fill != NULL && fill->nType == X_IT_STRING && fill->u.string.len )
   {
      const unsigned char *text = ( const unsigned char * )fill->u.string.data;
      size_t pos = 0;

      ch = s_gtNextCharacter( s_gtCurrent, text, fill->u.string.len, &pos );
   }
   else if( fill != NULL && fill->nType == X_IT_INTEGER && fill->u.integer >= 0 )
      ch = ( uint16_t )fill->u.integer;
   else if( fill != NULL && fill->nType == X_IT_DOUBLE && fill->u.dbl >= 0 )
      ch = ( uint16_t )fill->u.dbl;

   s_gtCurrent->pFuncTable->Scroll( s_gtCurrent, top, left, bottom, right, attr, ch, rows, cols );
   s_gtCurrent->pFuncTable->Flush( s_gtCurrent );

   return s_gtCurrent->outputError;
}

/* Toggle asterisk (*) interpretation in color strings */
int x_gtSetBlink( int blink )
{
   if( !s_gtCurrent )
      return 1;

   if( s_gtCurrent->blink != ( blink != 0 ) )
   {
      s_gtCurrent->blink = blink != 0;
      return x_gtPostExt();
   }

   return 0;
}

/* Convert the first color of a color string, such as "W+/B", to a color attribute; -1 when invalid */
int x_gtColorToN( const char *colorString )
{
   const unsigned char *text = ( const unsigned char * )colorString;
   int color;

   return IIF( s_gtColorParse( &text, &color ) < 0, -1, color );
}

/* Change the xDev color attributes setting */
int x_gtSetColorStr( unsigned char *colorString )
{
   const unsigned char *text = colorString;
   int colors[16], color, seen;
   unsigned int count = 0;

   if( !s_gtCurrent )
      return 1;

   memcpy( colors, s_gtCurrent->colors, sizeof( colors ) );
   if( !text || !*text )
      text = ( const unsigned char * )"W/N,N/W,N/N,N/N,N/W";

   /* A missing color keeps the old one; the enhanced color is also the unselected one */
   for( ;; )
   {
      if( count == 16 )
         return 1;

      seen = s_gtColorParse( &text, &color );
      if( seen < 0 )
         return 1;

      if( seen )
      {
         colors[count] = color;
         if( count == 1 )
            colors[4] = color;
      }

      ++count;
      if( !*text )
         break;

      ++text;
   }

   memcpy( s_gtCurrent->colors, colors, sizeof( colors ) );
   s_gtCurrent->colorCount = IIF( count > s_gtCurrent->colorCount, count, s_gtCurrent->colorCount );
   s_gtCurrent->colorIndex = 0;
   s_gtCurrent->foreground = X_GT_RGB_DEFAULT;
   s_gtCurrent->background = X_GT_RGB_DEFAULT;

   return 0;
}

/* SetColorExt(): 24-bit colors for the output */
int x_gtSetColorRGB( uint32_t foreground, uint32_t background )
{
   if( !s_gtCurrent || !s_gtCurrent->pFuncTable->TrueColor( s_gtCurrent ) )
      return 1;

   s_gtCurrent->foreground = foreground;
   s_gtCurrent->background = background;

   return 0;
}

/* The 24-bit colors of the output; 1 when they are not set */
int x_gtGetColorRGB( uint32_t *foreground, uint32_t *background )
{
   if( !s_gtCurrent || s_gtCurrent->foreground == X_GT_RGB_DEFAULT )
      return 1;

   *foreground = s_gtCurrent->foreground;
   *background = s_gtCurrent->background;

   return 0;
}

/* Set the cursor shape */
int x_gtSetCursor( unsigned short cursorShape )
{
   if( !s_gtCurrent || cursorShape > 4 )
      return 1;

   s_gtCurrent->pFuncTable->SetCursorStyle( s_gtCurrent, cursorShape );
   s_gtCurrent->pFuncTable->Flush( s_gtCurrent );

   return s_gtCurrent->outputError;
}

/* Change display mode to specified number of rows & columns */
int x_gtSetMode( unsigned short rows, unsigned short cols )
{
   if( !s_gtCurrent || !rows || !cols )
      return 1;

   return IIF( s_gtCurrent->pFuncTable->SetMode( s_gtCurrent, rows, cols ), 0, 1 );
}

/* The call of the GTI_RESIZEBLOCK block */
static void s_gtResizeCall( X_CALL_CTX *ctx )
{
   x_evalBlock( ctx, ctx->result, ( const X_ITEM * )ctx->args[0].value, 0, NULL );
}

/* Evaluate the GTI_RESIZEBLOCK block after the window changed the screen size, also while the user
   drags the window border; an error in the block is raised when the GT returns to the program */
void x_gtResizeNotify( X_GT_BASE *gt )
{
   X_ITEM block __attribute__( ( cleanup( x_itemClear ) ) ) = X_ITEM_NIL_INIT;
   X_ITEM result __attribute__( ( cleanup( x_itemClear ) ) ) = X_ITEM_NIL_INIT;
   X_ARG argument = { &block, 0 };

   x_ctScreenResize( gt );
   if( !gt->resizeBlock || gt->resizeActive )
      return;

   x_itemCopy( &block, gt->resizeBlock );
   gt->resizeActive = 1;
   x_callbackInvoke( s_gtResizeCall, &result, 1, &argument );
   gt->resizeActive = 0;
}

/* Move the cursor to a new position */
int x_gtSetPos( unsigned short row, unsigned short column )
{
   if( !s_gtCurrent )
      return 1;

   s_gtCurrent->pFuncTable->SetPos( s_gtCurrent, row, column );
   s_gtCurrent->pFuncTable->Flush( s_gtCurrent );

   return s_gtCurrent->outputError;
}

/* Toggle snow suppression */
int x_gtSetSnowFlag( int noSnow )
{
   UNUSED( noSnow );

   return 0;
}

/* Write a string to the current screen location */
int x_gtWrite( unsigned char *text, unsigned short length )
{
   if( !s_gtCurrent )
      return 1;

   return x_gtWriteAt( ( unsigned short )s_gtCurrent->row, ( unsigned short )s_gtCurrent->col, text, length );
}

/* Write a string at the specified screen location */
int x_gtWriteAt( unsigned short row, unsigned short column, unsigned char *text, unsigned short length )
{
   size_t i = 0;
   int col = column;
   uint16_t character;

   if( !s_gtCurrent )
      return 1;

   while( i < length && col < s_gtCurrent->cols )
   {
      character = s_gtNextCharacter( s_gtCurrent, text, length, &i );
      s_gtPutCurrent( s_gtCurrent, row, col++, 0, character );
   }

   s_gtCurrent->pFuncTable->SetPos( s_gtCurrent, row, IIF( col > s_gtCurrent->cols, s_gtCurrent->cols, col ) );
   s_gtCurrent->pFuncTable->Flush( s_gtCurrent );

   return s_gtCurrent->outputError;
}

/* Write a string at the cursor in 24-bit colors */
int x_gtWriteExt( unsigned char *text, unsigned short length, uint32_t foreground, uint32_t background )
{
   unsigned short row, col;

   if( !s_gtCurrent )
      return 1;

   row = ( unsigned short )s_gtCurrent->row;
   col = ( unsigned short )s_gtCurrent->col;

   return x_gtWriteAtExt( row, col, text, length, foreground, background, 1 );
}

/* Write a string at a position in 24-bit colors */
int x_gtWriteAtExt( unsigned short row, unsigned short column, unsigned char *text, unsigned short length,
                    uint32_t foreground, uint32_t background, int setPos )
{
   size_t i = 0;
   int col = column;
   uint16_t character;

   if( !s_gtCurrent || !s_gtCurrent->pFuncTable->TrueColor( s_gtCurrent ) )
      return 1;

   while( i < length && col < s_gtCurrent->cols )
   {
      character = s_gtNextCharacter( s_gtCurrent, text, length, &i );
      s_gtPutCharExt( s_gtCurrent, row, col++, 0, character, foreground, background );
   }

   if( setPos )
      s_gtCurrent->pFuncTable->SetPos( s_gtCurrent, row, IIF( col > s_gtCurrent->cols, s_gtCurrent->cols, col ) );

   s_gtCurrent->pFuncTable->Flush( s_gtCurrent );

   return s_gtCurrent->outputError;
}

/* Perform a console style write to the screen */
int x_gtWriteCon( const char *text, size_t length )
{
   if( !s_gtCurrent )
      return 1;

   s_gtCurrent->pFuncTable->WriteCon( s_gtCurrent, text, length );
   s_gtCurrent->pFuncTable->Flush( s_gtCurrent );

   return s_gtCurrent->outputError;
}

/* Not implemented: x_gtWCreate, x_gtWDestroy, x_gtWFlash, x_gtWApp, x_gtWCurrent, x_gtWPos, x_gtWVis,
   x_gtSLR, x_gtModalRead, x_gtBeginWrite, x_gtEndWrite, x_gtFlushCursor, x_gtSetColor, x_gtGetColor and
   x_gtSetBorder */

/* Before the program gives the screen to another program */
int x_gtSuspend( void )
{
   return IIF( s_gtCurrent && s_gtCurrent->pFuncTable->Suspend( s_gtCurrent ), 0, 1 );
}

/* After another program used the screen */
int x_gtResume( void )
{
   return IIF( s_gtCurrent && s_gtCurrent->pFuncTable->Resume( s_gtCurrent ), 0, 1 );
}

/* Whether a GT driver is running. This is the availability contract; x_gtVersion() is only informational. */
int x_gtActive( void )
{
   return s_gtCurrent != NULL;
}

/* Does the screen show 24-bit colors? */
int x_gtHasTrueColor( void )
{
   return s_gtCurrent && s_gtCurrent->pFuncTable->TrueColor( s_gtCurrent );
}

/* Does the stream (0 input, 1 output, 2 error) go to the screen? */
int x_gtIsConsole( int stream )
{
   if( !s_gtCurrent )
      return 0;

   if( stream == 1 )
      return s_gtCurrent->stdoutConsole;

   if( stream == 2 )
      return s_gtCurrent->stderrConsole;

   return stream == 0;
}

/* The name of the driver */
const char *x_gtVersion( int type )
{
   return IIF( s_gtCurrent, s_gtCurrent->pFuncTable->Version( s_gtCurrent, type ), "" );
}

/* GtInfo() */
int x_gtInfo( int type, X_ITEM *newValue, X_ITEM *newValue2, X_ITEM *result )
{
   X_GT_INFO info = { newValue, newValue2, result };

   return s_gtCurrent && s_gtCurrent->pFuncTable->Info( s_gtCurrent, type, &info );
}

/* Alert() */
int x_gtAlert( X_ITEM *message, X_ITEM *options, int colorNormal, int colorHigh, double delay )
{
   if( !s_gtCurrent )
      return 0;

   return s_gtCurrent->pFuncTable->Alert( s_gtCurrent, message, options, colorNormal, colorHigh, delay );
}

/* GfxPrimitive() */
int x_gtGfxPrimitive( int type, int top, int left, int bottom, int right, int color )
{
   if( !s_gtCurrent )
      return 0;

   return s_gtCurrent->pFuncTable->GfxPrimitive( s_gtCurrent, type, top, left, bottom, right, color );
}

/* GfxText() */
void x_gtGfxText( int top, int left, const char *text, int color, int size, int width )
{
   if( s_gtCurrent )
      s_gtCurrent->pFuncTable->GfxText( s_gtCurrent, top, left, text, color, size, width );
}

/* MPresent() */
int x_mouseIsPresent( void )
{
   return s_gtCurrent && s_gtCurrent->pFuncTable->MouseIsPresent( s_gtCurrent );
}

/* NumButtons() */
int x_mouseCountButton( void )
{
   return IIF( s_gtCurrent, s_gtCurrent->pFuncTable->MouseCountButton( s_gtCurrent ), 0 );
}

/* MLeftDown() and MRightDown() */
int x_mouseButtonState( int button )
{
   return s_gtCurrent && s_gtCurrent->pFuncTable->MouseButtonState( s_gtCurrent, button );
}

/* The mouse position, 0, 0 without a screen */
void x_mouseGetPos( int *row, int *col )
{
   if( s_gtCurrent )
      s_gtCurrent->pFuncTable->MouseGetPos( s_gtCurrent, row, col );
   else
   {
      *row = 0;
      *col = 0;
   }
}

/* SetMouse() */
void x_mouseSetPos( int row, int col )
{
   if( s_gtCurrent )
      s_gtCurrent->pFuncTable->MouseSetPos( s_gtCurrent, row, col );
}

/* MDblClk() */
int x_mouseGetDoubleClickSpeed( void )
{
   return IIF( s_gtCurrent, s_gtCurrent->pFuncTable->MouseGetDoubleClickSpeed( s_gtCurrent ), 0 );
}

/* MDblClk( nNew ): returns the previous time */
int x_mouseSetDoubleClickSpeed( int speed )
{
   int previous = 0;

   if( s_gtCurrent )
   {
      previous = s_gtCurrent->mouseDoubleClickSpeed;
      s_gtCurrent->mouseDoubleClickSpeed = IIF( speed >= 0, speed, 0 );
   }

   return previous;
}

/* MRow() */
int x_mouseRow( void )
{
   int row, col;

   x_mouseGetPos( &row, &col );

   return row;
}

/* MCol() */
int x_mouseCol( void )
{
   int row, col;

   x_mouseGetPos( &row, &col );

   return col;
}
