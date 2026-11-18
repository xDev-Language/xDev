/*
 * Copyright (c) 2024-2026 xDev-Language
 *
 * Library: xDev GTGLFW
 * File: gtglfw.c
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "gtglfw.h"
#include "../../fonts/xfont9x18.h"

#if X_OS_WINDOWS
#include <windows.h>
#include <commctrl.h>
#include <mmsystem.h>
#include <shellapi.h>
#include <shobjidl.h>
#include <wincodec.h>
#define GLFW_EXPOSE_NATIVE_WIN32
#define GLFW_NATIVE_INCLUDE_NONE
#include <GLFW/glfw3native.h>
#pragma comment( lib, "opengl32" )
#pragma comment( lib, "advapi32" )
#pragma comment( lib, "comctl32" )
#pragma comment( lib, "ole32" )
#pragma comment( lib, "shell32" )
#pragma comment( lib, "uuid" )
#pragma comment( lib, "winmm" )
#pragma comment( lib, "windowscodecs" )
#pragma comment( linker, "\"/manifestdependency:type='win32' "                                                         \
                         "name='Microsoft.Windows.Common-Controls' "                                                   \
                         "version='6.0.0.0' processorArchitecture='*' "                                                \
                         "publicKeyToken='6595b64144ccf1df' language='*'\"" )
#elif X_OS_LINUX
#include <errno.h>
#include <fcntl.h>
#include <spawn.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
typedef struct _XDisplay Display;
typedef unsigned long    Window;
typedef unsigned long    RRCrtc;
typedef unsigned long    RROutput;
#define GLFW_EXPOSE_NATIVE_X11
#define GLFW_NATIVE_INCLUDE_NONE
#include <GLFW/glfw3native.h>
#pragma comment( lib, "GL" )
#endif

#include <GL/gl.h>

#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int        s_GtId;
static X_GT_FUNCS s_super;
static X_GTGLFW  *s_windows;
static int        s_glfwUsers;
static int        s_pumpingEvents;
static int        s_errorCode;
static char       s_errorText[1024];

#define X_GTGLFW_GET( pGT ) ( ( X_GTGLFW * )( pGT )->driverData[s_GtId] )

static const uint8_t s_defaultColors[16][3] = {
   { 0, 0, 0 },     { 0, 0, 170 },     { 0, 170, 0 },    { 0, 170, 170 },  { 170, 0, 0 },   { 170, 0, 170 },
   { 170, 85, 0 },  { 170, 170, 170 }, { 85, 85, 85 },   { 85, 85, 255 },  { 85, 255, 85 }, { 85, 255, 255 },
   { 255, 85, 85 }, { 255, 85, 255 },  { 255, 255, 85 }, { 255, 255, 255 }
};

static const int s_floodDX[4] = { -1, 1, 0, 0 };
static const int s_floodDY[4] = { 0, 0, -1, 1 };

typedef struct
{
   int top;
   int left;
   int bottom;
   int right;
} X_GTGLFW_RECT;

typedef struct
{
   uint32_t *pixels; /* straight alpha, 0xAARRGGBB */
   int       width;
   int       height;
} X_GTGLFW_IMAGE;

static void        s_gt_glfw_ClearSelection( X_GTGLFW *glfw, int skipUp );
static const char *s_gt_glfw_Version( X_GT_BASE *pGT, int type );
static void        s_gt_glfw_UpdateMetrics( X_GTGLFW *glfw );
static void        s_gt_glfw_UpdateSizeLimits( X_GTGLFW *glfw );
static int         s_gt_glfw_ResizeFromWindow( X_GTGLFW *glfw );

static void s_gt_glfw_AddKeyEvent( X_GTGLFW *glfw, int key, int scancode, int repeat )
{
   int next = ( glfw->keyIn + 1 ) % X_GTGLFW_KEY_QUEUE_SIZE;

   if( glfw->keyIn != glfw->keyOut )
   {
      int last = IIF( glfw->keyIn == 0, X_GTGLFW_KEY_QUEUE_SIZE - 1, glfw->keyIn - 1 );

      if( key == K_RESIZE && glfw->keyQueue[last] == K_RESIZE )
         return;

      if( X_INKEY_ISMOUSEPOS( key ) && X_INKEY_ISMOUSEPOS( glfw->keyQueue[last] ) )
      {
         glfw->keyQueue[last] = key;
         glfw->keyScancode[last] = scancode;
         glfw->keyRepeat[last] = IIF( repeat, 1, 0 );
         return;
      }
   }
   if( next != glfw->keyOut )
   {
      glfw->keyQueue[glfw->keyIn] = key;
      glfw->keyScancode[glfw->keyIn] = scancode;
      glfw->keyRepeat[glfw->keyIn] = IIF( repeat, 1, 0 );
      glfw->keyIn = next;
   }
}

static void s_gt_glfw_AddKey( X_GTGLFW *glfw, int key )
{
   s_gt_glfw_AddKeyEvent( glfw, key, 0, 0 );
}

static void s_gt_glfw_DiscardKeyRepeats( X_GTGLFW *glfw, int scancode )
{
   int read = glfw->keyOut;
   int write = glfw->keyOut;

   while( read != glfw->keyIn )
   {
      int next = ( read + 1 ) % X_GTGLFW_KEY_QUEUE_SIZE;

      if( !glfw->keyRepeat[read] || ( scancode != 0 && glfw->keyScancode[read] != scancode ) )
      {
         if( write != read )
         {
            glfw->keyQueue[write] = glfw->keyQueue[read];
            glfw->keyScancode[write] = glfw->keyScancode[read];
            glfw->keyRepeat[write] = glfw->keyRepeat[read];
         }
         write = ( write + 1 ) % X_GTGLFW_KEY_QUEUE_SIZE;
      }
      read = next;
   }
   glfw->keyIn = write;
}

static void s_gt_glfw_ProcessWindowRequests( void )
{
   X_GTGLFW *glfw = s_windows;

   while( glfw )
   {
      if( glfw->closePending )
      {
         glfw->closePending = 0;
         if( glfw->closeMode == 0 )
         {
            x_quitRaise();
            return;
         }
         if( glfw->closeMode == 1 )
            s_gt_glfw_AddKey( glfw, K_CLOSE );
      }
      glfw = glfw->next;
   }
}

static void s_gt_glfw_PumpEvents( void )
{
   if( !s_pumpingEvents )
   {
      s_pumpingEvents = 1;
      x_callbackEnterDll();
      glfwPollEvents();
      {
         X_GTGLFW *glfw = s_windows;

         while( glfw )
         {
            if( glfw->windowResizePending )
            {
               glfw->windowResizePending = 0;
               s_gt_glfw_UpdateMetrics( glfw );
               s_gt_glfw_UpdateSizeLimits( glfw );

               /* On the full screen too, so the character grid fills the monitor as in GTWVT */
               if( s_gt_glfw_ResizeFromWindow( glfw ) )
                  x_gtResizeNotify( glfw->pGT );
            }
            glfw = glfw->next;
         }
      }
      x_callbackLeaveDll();
      s_pumpingEvents = 0;
      s_gt_glfw_ProcessWindowRequests();
      x_callbackRaisePending();
   }
}

static int s_gt_glfw_FindGlyphExact( uint16_t code )
{
   int left = 0;
   int right = X_FONT9X18_GLYPH_COUNT - 1;

   while( left <= right )
   {
      int      middle = left + ( right - left ) / 2;
      uint16_t current = s_font9x18Codepoints[middle];

      if( current < code )
         left = middle + 1;
      else if( current > code )
         right = middle - 1;
      else
         return middle;
   }

   return -1;
}

static int s_gt_glfw_FindGlyph( uint16_t code )
{
   int glyph = s_gt_glfw_FindGlyphExact( code );

   if( glyph < 0 && code == 0x00A0 )
      glyph = s_gt_glfw_FindGlyphExact( 0x0020 );
   if( glyph < 0 )
      glyph = s_gt_glfw_FindGlyphExact( 0xFFFD );
   if( glyph < 0 )
      glyph = s_gt_glfw_FindGlyphExact( 0x003F );

   return glyph;
}

static uint32_t s_gt_glfw_Color( X_GTGLFW *glfw, int color )
{
   const uint8_t *rgb = glfw->colors[color & 0x0F];

   return ( ( uint32_t )rgb[0] << 16 ) | ( ( uint32_t )rgb[1] << 8 ) | rgb[2];
}

/* colors of a cell as 0xRRGGBB: the 24-bit colors of DispOutAtExt() and DispBoxExt(), otherwise
   the palette entries of its color attribute */
static void s_gt_glfw_CellColors( X_GTGLFW *glfw, int row, int col, int color, uint32_t *fore, uint32_t *back )
{
   const X_GT_CELL *cell = &glfw->pGT->screenBuffer[( size_t )row * glfw->pGT->cols + col];

   *fore = IIF( cell->foreground != X_GT_RGB_DEFAULT, cell->foreground, s_gt_glfw_Color( glfw, color ) );
   *back = IIF( cell->background != X_GT_RGB_DEFAULT, cell->background, s_gt_glfw_Color( glfw, color >> 4 ) );
}

/* clears the framebuffer with the SetColorExt() background, otherwise the background of the
   SetColor() standard colour, so the area outside the grid is the margin of a terminal */
static void s_gt_glfw_ClearMargin( X_GTGLFW *glfw )
{
   uint32_t back = glfw->pGT->background;
   float    red, green, blue;

   if( back == X_GT_RGB_DEFAULT )
      back = s_gt_glfw_Color( glfw, glfw->pGT->colors[0] >> 4 );
   red = ( float )( ( back >> 16 ) & 0xFF ) / 255.0f;
   green = ( float )( ( back >> 8 ) & 0xFF ) / 255.0f;
   blue = ( float )( back & 0xFF ) / 255.0f;
   glClearColor( red, green, blue, 1.0f );
   glClear( GL_COLOR_BUFFER_BIT );
}

static void s_gt_glfw_FillRect( X_GTGLFW *glfw, int top, int left, int bottom, int right, uint32_t color )
{
   int row;

   top = GTGLFW_MAX( top, 0 );
   left = GTGLFW_MAX( left, 0 );
   bottom = GTGLFW_MIN( bottom, glfw->canvasHeight - 1 );
   right = GTGLFW_MIN( right, glfw->canvasWidth - 1 );

   for( row = top; row <= bottom; ++row )
   {
      uint32_t *pixel = glfw->canvas + ( size_t )row * glfw->canvasWidth + left;
      int       col;

      for( col = left; col <= right; ++col )
         *pixel++ = color;
   }
}

static void s_gt_glfw_DrawGlyph( X_GTGLFW *glfw, uint16_t code, int top, int left, uint32_t color )
{
   int glyph = s_gt_glfw_FindGlyph( code );
   int y;

   if( glyph < 0 )
      return;

   for( y = 0; y < X_FONT9X18_HEIGHT; ++y )
   {
      int      targetY = top + y;
      uint16_t bits = s_font9x18Bitmaps[glyph][y];
      int      x;

      if( targetY < 0 || targetY >= glfw->canvasHeight )
         continue;
      for( x = 0; x < X_FONT9X18_WIDTH; ++x )
      {
         int targetX = left + x;

         if( targetX >= 0 && targetX < glfw->canvasWidth && ( bits & ( 0x8000U >> x ) ) != 0 )
            glfw->canvas[( size_t )targetY * glfw->canvasWidth + targetX] = color;
      }
   }
}

static void s_gt_glfw_DestroyCanvas( X_GTGLFW *glfw )
{
   if( glfw->window && glfw->texture != 0 )
   {
      glfwMakeContextCurrent( glfw->window );
      glDeleteTextures( 1, &glfw->texture );
      glfw->texture = 0;
   }
   x_xfree( glfw->frame );
   x_xfree( glfw->canvas );
   glfw->frame = NULL;
   glfw->canvas = NULL;
}

static int s_gt_glfw_CreateCanvas( X_GTGLFW *glfw, int rows, int cols )
{
   size_t pixelCount;

   if( rows < X_GTGLFW_MIN_ROWS || rows > X_GTGLFW_MAX_ROWS || cols < X_GTGLFW_MIN_COLS || cols > X_GTGLFW_MAX_COLS )
      return 0;

   s_gt_glfw_DestroyCanvas( glfw );
   glfw->rows = rows;
   glfw->cols = cols;
   glfw->canvasWidth = cols * X_GTGLFW_CELL_WIDTH;
   glfw->canvasHeight = rows * X_GTGLFW_CELL_HEIGHT;
   glfw->frameWidth = glfw->canvasWidth;
   glfw->frameHeight = glfw->canvasHeight;
   pixelCount = ( size_t )glfw->canvasWidth * glfw->canvasHeight;
   glfw->canvas = ( uint32_t * )x_xgrabzArray( pixelCount, sizeof( uint32_t ) );
   glfw->frame = ( uint8_t * )x_xgrabArray( pixelCount, 4 );

   glfwMakeContextCurrent( glfw->window );
   glGenTextures( 1, &glfw->texture );
   if( glfw->texture == 0 )
   {
      s_gt_glfw_DestroyCanvas( glfw );
      return 0;
   }
   glBindTexture( GL_TEXTURE_2D, glfw->texture );
   glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST );
   glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST );
   glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP );
   glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP );
   glTexImage2D( GL_TEXTURE_2D, 0, GL_RGBA, glfw->canvasWidth, glfw->canvasHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL );

   glfw->clipTop = 0;
   glfw->clipLeft = 0;
   glfw->clipBottom = glfw->canvasHeight - 1;
   glfw->clipRight = glfw->canvasWidth - 1;
   glfw->canvasDirty = 1;
   glfw->presentPending = 1;

   return 1;
}

static void s_gt_glfw_UpdateMetrics( X_GTGLFW *glfw )
{
   glfwGetWindowSize( glfw->window, &glfw->windowWidth, &glfw->windowHeight );
   glfwGetFramebufferSize( glfw->window, &glfw->framebufferWidth, &glfw->framebufferHeight );
   glfwGetWindowContentScale( glfw->window, &glfw->contentScaleX, &glfw->contentScaleY );

   if( glfw->contentScaleX <= 0.0f )
      glfw->contentScaleX = 1.0f;

   if( glfw->contentScaleY <= 0.0f )
      glfw->contentScaleY = 1.0f;

   glfw->focused = glfwGetWindowAttrib( glfw->window, GLFW_FOCUSED ) == GLFW_TRUE;
   glfw->minimized = glfwGetWindowAttrib( glfw->window, GLFW_ICONIFIED ) == GLFW_TRUE;
   glfw->maximized = glfwGetWindowAttrib( glfw->window, GLFW_MAXIMIZED ) == GLFW_TRUE;
}

static void s_gt_glfw_ScaledWindowSize( X_GTGLFW *glfw, int logicalWidth, int logicalHeight, int *width, int *height )
{
   double densityX = 1.0;
   double densityY = 1.0;
   double scaledWidth, scaledHeight;

   if( glfw->windowWidth > 0 && glfw->framebufferWidth > 0 )
      densityX = glfw->framebufferWidth / ( double )glfw->windowWidth;

   if( glfw->windowHeight > 0 && glfw->framebufferHeight > 0 )
      densityY = glfw->framebufferHeight / ( double )glfw->windowHeight;

   scaledWidth = logicalWidth * glfw->contentScaleX / densityX;
   scaledHeight = logicalHeight * glfw->contentScaleY / densityY;

   *width = GTGLFW_MAX( ( int )( scaledWidth + 0.999999 ), 1 );
   *height = GTGLFW_MAX( ( int )( scaledHeight + 0.999999 ), 1 );
}

static void s_gt_glfw_UpdateSizeLimits( X_GTGLFW *glfw )
{
   int minWidth, minHeight, maxWidth, maxHeight;
   int gridWidth = X_GTGLFW_MAX_COLS * X_GTGLFW_CELL_WIDTH;
   int gridHeight = X_GTGLFW_MAX_ROWS * X_GTGLFW_CELL_HEIGHT;

   s_gt_glfw_ScaledWindowSize( glfw, X_GTGLFW_CELL_WIDTH, X_GTGLFW_CELL_HEIGHT, &minWidth, &minHeight );
   s_gt_glfw_ScaledWindowSize( glfw, gridWidth, gridHeight, &maxWidth, &maxHeight );
   glfwSetWindowSizeLimits( glfw->window, minWidth, minHeight, maxWidth, maxHeight );
}

static void s_gt_glfw_SetWindowSizeForGrid( X_GTGLFW *glfw )
{
   int width, height;

   if( glfw->fullscreen )
      return;

   s_gt_glfw_ScaledWindowSize( glfw, glfw->canvasWidth, glfw->canvasHeight, &width, &height );
   if( width != glfw->windowWidth || height != glfw->windowHeight )
   {
      glfw->resizeActive = 1;
      glfwSetWindowSize( glfw->window, width, height );
      glfw->resizeActive = 0;
   }
   s_gt_glfw_UpdateMetrics( glfw );
}

static int s_gt_glfw_ResizeGrid( X_GTGLFW *glfw, int rows, int cols, int sizeWindow, int notify )
{
   int changed = rows != glfw->rows || cols != glfw->cols;

   if( rows < X_GTGLFW_MIN_ROWS || rows > X_GTGLFW_MAX_ROWS || cols < X_GTGLFW_MIN_COLS || cols > X_GTGLFW_MAX_COLS )
      return 0;

   if( changed )
   {
      s_gt_glfw_ClearSelection( glfw, 1 );
      if( !s_gt_glfw_CreateCanvas( glfw, rows, cols ) )
         return 0;

      X_GTSELF_RESIZE( glfw->pGT, rows, cols );
      glfw->mouseRow = GTGLFW_MIN( glfw->mouseRow, rows - 1 );
      glfw->mouseCol = GTGLFW_MIN( glfw->mouseCol, cols - 1 );
      glfw->selectionAnchor = GTGLFW_MIN( glfw->selectionAnchor, rows * cols - 1 );
      glfw->selectionEnd = GTGLFW_MIN( glfw->selectionEnd, rows * cols - 1 );
      glfw->resizePending = 1;
      X_GTSELF_EXPOSEAREA( glfw->pGT, 0, 0, rows - 1, cols - 1 );
      if( notify )
         s_gt_glfw_AddKey( glfw, K_RESIZE );
   }

   if( sizeWindow )
      s_gt_glfw_SetWindowSizeForGrid( glfw );

   return 1;
}

static int s_gt_glfw_ResizeFromWindow( X_GTGLFW *glfw )
{
   int logicalWidth, logicalHeight;
   int rows, cols;
   int changed;

   if( glfw->resizeActive || glfw->windowWidth <= 0 || glfw->windowHeight <= 0 )
      return 0;

   if( glfw->framebufferWidth <= 0 || glfw->framebufferHeight <= 0 )
      return 0;

   logicalWidth = GTGLFW_MAX( ( int )( glfw->framebufferWidth / ( double )glfw->contentScaleX + 0.5 ), 1 );
   logicalHeight = GTGLFW_MAX( ( int )( glfw->framebufferHeight / ( double )glfw->contentScaleY + 0.5 ), 1 );
   cols = GTGLFW_MIN( GTGLFW_MAX( logicalWidth / X_GTGLFW_CELL_WIDTH, X_GTGLFW_MIN_COLS ), X_GTGLFW_MAX_COLS );
   rows = GTGLFW_MIN( GTGLFW_MAX( logicalHeight / X_GTGLFW_CELL_HEIGHT, X_GTGLFW_MIN_ROWS ), X_GTGLFW_MAX_ROWS );

   changed = rows != glfw->rows || cols != glfw->cols;
   if( changed && !s_gt_glfw_ResizeGrid( glfw, rows, cols, 0, 1 ) )
      return 0;

   if( glfw->resizeStep && !glfw->maximized && !glfw->fullscreen )
      s_gt_glfw_SetWindowSizeForGrid( glfw );

   return changed;
}

static GLFWmonitor *s_gt_glfw_CurrentMonitor( X_GTGLFW *glfw )
{
   GLFWmonitor **monitors;
   GLFWmonitor  *best = NULL;
   int           count = 0;
   int           index;
   int64_t       bestArea = -1;

   monitors = glfwGetMonitors( &count );
   for( index = 0; index < count; ++index )
   {
      const GLFWvidmode *mode = glfwGetVideoMode( monitors[index] );
      int                monitorX, monitorY;
      int64_t            left, top, right, bottom, area;

      if( !mode )
         continue;
      glfwGetMonitorPos( monitors[index], &monitorX, &monitorY );
      left = GTGLFW_MAX( glfw->windowX, monitorX );
      top = GTGLFW_MAX( glfw->windowY, monitorY );
      right = GTGLFW_MIN( ( int64_t )glfw->windowX + glfw->windowWidth, ( int64_t )monitorX + mode->width );
      bottom = GTGLFW_MIN( ( int64_t )glfw->windowY + glfw->windowHeight, ( int64_t )monitorY + mode->height );
      area = GTGLFW_MAX( right - left, 0 ) * GTGLFW_MAX( bottom - top, 0 );
      if( area > bestArea )
      {
         bestArea = area;
         best = monitors[index];
      }
   }

   return IIF( best, best, glfwGetPrimaryMonitor() );
}

/* Put the window with its frame in the centre of the work area of its monitor, as GTWVT and GTSDL3 do */
static void s_gt_glfw_CenterWindow( X_GTGLFW *glfw )
{
   GLFWmonitor *monitor = s_gt_glfw_CurrentMonitor( glfw );
   int          areaX, areaY, areaWidth, areaHeight;
   int          frameLeft, frameTop, frameRight, frameBottom;
   int          width, height, x, y;

   if( !monitor || glfw->fullscreen )
      return;

   glfwGetMonitorWorkarea( monitor, &areaX, &areaY, &areaWidth, &areaHeight );
   glfwGetWindowFrameSize( glfw->window, &frameLeft, &frameTop, &frameRight, &frameBottom );
   width = glfw->windowWidth + frameLeft + frameRight;
   height = glfw->windowHeight + frameTop + frameBottom;
   x = areaX + GTGLFW_MAX( ( areaWidth - width ) / 2, 0 ) + frameLeft;
   y = areaY + GTGLFW_MAX( ( areaHeight - height ) / 2, 0 ) + frameTop;
   glfwSetWindowPos( glfw->window, x, y );
   glfwGetWindowPos( glfw->window, &glfw->windowX, &glfw->windowY );
}

static void s_gt_glfw_SetFullscreen( X_GTGLFW *glfw, int fullscreen )
{
   if( fullscreen == glfw->fullscreen )
      return;

   if( fullscreen )
   {
      GLFWmonitor       *monitor = s_gt_glfw_CurrentMonitor( glfw );
      const GLFWvidmode *mode = IIF( monitor, glfwGetVideoMode( monitor ), NULL );

      if( !monitor || !mode )
         return;

      glfwGetWindowPos( glfw->window, &glfw->savedX, &glfw->savedY );
      glfwGetWindowSize( glfw->window, &glfw->savedWidth, &glfw->savedHeight );
      glfw->fullscreen = 1;
      glfwSetWindowMonitor( glfw->window, monitor, 0, 0, mode->width, mode->height, mode->refreshRate );
   }
   else
   {
      int width = IIF( glfw->savedWidth > 0, glfw->savedWidth, glfw->canvasWidth );
      int height = IIF( glfw->savedHeight > 0, glfw->savedHeight, glfw->canvasHeight );

      glfw->fullscreen = 0;
      glfwSetWindowMonitor( glfw->window, NULL, glfw->savedX, glfw->savedY, width, height, GLFW_DONT_CARE );
   }
   s_gt_glfw_UpdateMetrics( glfw );
   glfwGetWindowPos( glfw->window, &glfw->windowX, &glfw->windowY );
   glfw->presentPending = 1;
}

static int s_gt_glfw_KeyFlags( int mods, int key )
{
   int flags = 0;

   if( ( mods & GLFW_MOD_SHIFT ) != 0 )
      flags |= KF_SHIFT;
   if( ( mods & GLFW_MOD_CONTROL ) != 0 )
      flags |= KF_CTRL;
   if( ( mods & GLFW_MOD_ALT ) != 0 )
      flags |= KF_ALT;
   if( key >= GLFW_KEY_KP_0 && key <= GLFW_KEY_KP_EQUAL )
      flags |= KF_KEYPAD;

   return flags;
}

static int s_gt_glfw_KeypadKey( int key, int mods, int *isText )
{
   int character, special;

   switch( key )
   {
      case GLFW_KEY_KP_DIVIDE: *isText = 1; return '/';
      case GLFW_KEY_KP_MULTIPLY: *isText = 1; return '*';
      case GLFW_KEY_KP_SUBTRACT: *isText = 1; return '-';
      case GLFW_KEY_KP_ADD: *isText = 1; return '+';
      case GLFW_KEY_KP_EQUAL: *isText = 1; return '=';
      case GLFW_KEY_KP_1:
         character = '1';
         special = KX_END;
         break;
      case GLFW_KEY_KP_2:
         character = '2';
         special = KX_DOWN;
         break;
      case GLFW_KEY_KP_3:
         character = '3';
         special = KX_PGDN;
         break;
      case GLFW_KEY_KP_4:
         character = '4';
         special = KX_LEFT;
         break;
      case GLFW_KEY_KP_5:
         character = '5';
         special = KX_CENTER;
         break;
      case GLFW_KEY_KP_6:
         character = '6';
         special = KX_RIGHT;
         break;
      case GLFW_KEY_KP_7:
         character = '7';
         special = KX_HOME;
         break;
      case GLFW_KEY_KP_8:
         character = '8';
         special = KX_UP;
         break;
      case GLFW_KEY_KP_9:
         character = '9';
         special = KX_PGUP;
         break;
      case GLFW_KEY_KP_0:
         character = '0';
         special = KX_INS;
         break;
      case GLFW_KEY_KP_DECIMAL:
         character = '.';
         special = KX_DEL;
         break;
      default: return 0;
   }

   if( ( mods & GLFW_MOD_NUM_LOCK ) != 0 )
   {
      *isText = 1;
      return character;
   }

   return special;
}

static int s_gt_glfw_SpecialKey( int key )
{
   switch( key )
   {
      case GLFW_KEY_ESCAPE: return KX_ESC;
      case GLFW_KEY_ENTER:
      case GLFW_KEY_KP_ENTER: return KX_ENTER;
      case GLFW_KEY_BACKSPACE: return KX_BS;
      case GLFW_KEY_TAB: return KX_TAB;
      case GLFW_KEY_UP: return KX_UP;
      case GLFW_KEY_DOWN: return KX_DOWN;
      case GLFW_KEY_LEFT: return KX_LEFT;
      case GLFW_KEY_RIGHT: return KX_RIGHT;
      case GLFW_KEY_HOME: return KX_HOME;
      case GLFW_KEY_END: return KX_END;
      case GLFW_KEY_PAGE_UP: return KX_PGUP;
      case GLFW_KEY_PAGE_DOWN: return KX_PGDN;
      case GLFW_KEY_INSERT: return KX_INS;
      case GLFW_KEY_DELETE: return KX_DEL;
      case GLFW_KEY_PRINT_SCREEN: return KX_PRTSCR;
      case GLFW_KEY_PAUSE: return KX_PAUSE;
      default:
         if( key >= GLFW_KEY_F1 && key <= GLFW_KEY_F12 )
            return KX_F1 + key - GLFW_KEY_F1;

         /* The xDev standard table currently names F1 through F12.  Keep
            F13 through F25 distinct in extended input until public names
            are added to the shared inkey API. */
         if( key >= GLFW_KEY_F13 && key <= GLFW_KEY_F25 )
            return 0x1200 + key - GLFW_KEY_F13;

         return 0;
   }
}

static int s_gt_glfw_PrintableKey( int key )
{
   if( key >= GLFW_KEY_SPACE && key <= GLFW_KEY_GRAVE_ACCENT )
      return key;

   return 0;
}

static void s_gt_glfw_KeyCallback( GLFWwindow *window, int key, int scancode, int action, int mods )
{
   X_GTGLFW *glfw = ( X_GTGLFW * )glfwGetWindowUserPointer( window );
   int       flags;
   int       value;
   int       isText = 0;

   if( !glfw )
      return;

   flags = s_gt_glfw_KeyFlags( mods, key );
   glfw->keyFlags = flags & ( KF_SHIFT | KF_CTRL | KF_ALT );
   glfw->keyMods = mods;

   if( action == GLFW_RELEASE )
   {
      s_gt_glfw_DiscardKeyRepeats( glfw, scancode );
      if( glfw->textScancode == scancode )
      {
         glfw->textScancode = 0;
         glfw->textFlags = 0;
         glfw->textRepeat = 0;
      }
      return;
   }
   if( action != GLFW_PRESS && action != GLFW_REPEAT )
      return;

   if( action == GLFW_PRESS && key == GLFW_KEY_ENTER && ( mods & GLFW_MOD_ALT ) != 0 && glfw->altEnter )
   {
      glfw->textScancode = 0;
      glfw->textFlags = 0;
      glfw->textRepeat = 0;
      s_gt_glfw_SetFullscreen( glfw, !glfw->fullscreen );
      return;
   }

   value = s_gt_glfw_SpecialKey( key );
   if( value == 0 && ( flags & KF_KEYPAD ) != 0 )
      value = s_gt_glfw_KeypadKey( key, mods, &isText );

   if( ( isText || value == 0 ) && ( flags & ( KF_CTRL | KF_ALT ) ) == 0 )
   {
      glfw->textScancode = scancode;
      glfw->textFlags = IIF( isText, flags, flags & KF_SHIFT );
      glfw->textRepeat = action == GLFW_REPEAT;
      return;
   }
   if( value != 0 )
   {
      s_gt_glfw_AddKeyEvent( glfw, X_INKEY_NEW_KEY( value, flags ), scancode, action == GLFW_REPEAT );
      return;
   }

   value = s_gt_glfw_PrintableKey( key );
   if( value != 0 && ( flags & ( KF_CTRL | KF_ALT ) ) != ( KF_CTRL | KF_ALT ) )
      s_gt_glfw_AddKeyEvent( glfw, X_INKEY_NEW_KEY( value, flags ), scancode, action == GLFW_REPEAT );
   else
   {
      glfw->textScancode = scancode;
      glfw->textFlags = 0;
      glfw->textRepeat = action == GLFW_REPEAT;
   }
}

static void s_gt_glfw_CharCallback( GLFWwindow *window, unsigned int codepoint )
{
   X_GTGLFW *glfw = ( X_GTGLFW * )glfwGetWindowUserPointer( window );
   int       key;

   if( !glfw || codepoint == 0 )
      return;

   if( codepoint > 0x10FFFF || ( codepoint >= 0xD800 && codepoint <= 0xDFFF ) )
      codepoint = 0xFFFD;

   key = X_INKEY_NEW_UNICODEF( ( int )codepoint, glfw->textFlags );
   s_gt_glfw_AddKeyEvent( glfw, key, glfw->textScancode, glfw->textRepeat );
}

static unsigned int s_gt_glfw_MouseButtonMask( int button )
{
   return IIF( button >= X_MBUTTON_LEFT && button <= X_MBUTTON_MIDDLE, 1U << button, 0 );
}

static int s_gt_glfw_MouseButtonIndex( int button )
{
   switch( button )
   {
      case GLFW_MOUSE_BUTTON_LEFT: return X_MBUTTON_LEFT;
      case GLFW_MOUSE_BUTTON_RIGHT: return X_MBUTTON_RIGHT;
      case GLFW_MOUSE_BUTTON_MIDDLE: return X_MBUTTON_MIDDLE;
      default: return -1;
   }
}

static int s_gt_glfw_MouseButtonKey( int button, int down, int doubleClick )
{
   if( button == X_MBUTTON_LEFT )
      return IIF( down, IIF( doubleClick, K_LDBLCLK, K_LBUTTONDOWN ), K_LBUTTONUP );

   if( button == X_MBUTTON_RIGHT )
      return IIF( down, IIF( doubleClick, K_RDBLCLK, K_RBUTTONDOWN ), K_RBUTTONUP );

   if( button == X_MBUTTON_MIDDLE )
      return IIF( down, IIF( doubleClick, K_MDBLCLK, K_MBUTTONDOWN ), K_MBUTTONUP );

   return 0;
}

static void s_gt_glfw_ClearSelection( X_GTGLFW *glfw, int skipUp )
{
   int wasSelecting = glfw->selecting;

   if( glfw->selectionActive )
      glfw->presentPending = 1;

   glfw->selecting = 0;
   glfw->selectionActive = 0;

   if( wasSelecting )
      glfw->selectionSkipUp = skipUp;
}

static void s_gt_glfw_UpdateSelection( X_GTGLFW *glfw )
{
   int end = glfw->mouseRow * glfw->cols + glfw->mouseCol;

   if( glfw->selecting && end != glfw->selectionEnd )
   {
      glfw->selectionEnd = end;
      glfw->presentPending = 1;
   }
}

static int s_gt_glfw_SetClipboard( X_GTGLFW *glfw, const char *text, size_t length )
{
   int error;

   if( !text || memchr( text, '\0', length ) != NULL )
      return 0;

   glfwGetError( NULL );
   glfwSetClipboardString( glfw->window, text );
   error = glfwGetError( NULL );

   return error == GLFW_NO_ERROR;
}

static int s_gt_glfw_PasteClipboard( X_GTGLFW *glfw )
{
   const char *text = glfwGetClipboardString( glfw->window );

   if( !text )
      return 0;

   glfw->pGT->pFuncTable->InkeySetText( glfw->pGT, text, strlen( text ), 1 );

   return 1;
}

static void s_gt_glfw_CopySelection( X_GTGLFW *glfw )
{
   int         anchorRow = glfw->selectionAnchor / glfw->cols;
   int         anchorCol = glfw->selectionAnchor % glfw->cols;
   int         endRow = glfw->selectionEnd / glfw->cols;
   int         endCol = glfw->selectionEnd % glfw->cols;
   int         left = GTGLFW_MIN( anchorCol, endCol );
   int         top = GTGLFW_MIN( anchorRow, endRow );
   int         right = GTGLFW_MAX( anchorCol, endCol );
   int         bottom = GTGLFW_MAX( anchorRow, endRow );
   size_t      rowCount = ( size_t )( bottom - top + 1 );
   size_t      colCount = ( size_t )( right - left + 1 );
   const char *eol = x_eol();
   size_t      eolLength = IIF( rowCount > 1, strlen( eol ), 0 );
   size_t      capacity = rowCount * ( colCount * 3 + eolLength ) + 1;
   size_t      length = 0;
   char       *text = ( char * )x_xgrab( capacity );
   int         row;

   for( row = top; row <= bottom; ++row )
   {
      int col;

      for( col = left; col <= right; ++col )
      {
         int      color;
         uint8_t  attr;
         uint16_t character;

         if( X_GTSELF_GETSCRCHAR( glfw->pGT, row, col, &color, &attr, &character ) )
            length += x_utf8Encode( text + length, x_u16Ctrl( character ) );
      }
      if( eolLength > 0 )
      {
         memcpy( text + length, eol, eolLength );
         length += eolLength;
      }
   }
   text[length] = '\0';
   s_gt_glfw_SetClipboard( glfw, text, length );
   x_xfree( text );
}

static void s_gt_glfw_UpdateMousePosition( X_GTGLFW *glfw, double x, double y, int addEvent )
{
   /* window coordinates to grid pixels: the grid is not stretched, it starts in the top left corner */
   int mouseX = 0;
   int mouseY = 0;
   int oldX = glfw->mouseX;
   int oldY = glfw->mouseY;

   if( glfw->windowWidth > 0 )
      mouseX = ( int )( x * glfw->framebufferWidth / ( glfw->windowWidth * ( double )glfw->contentScaleX ) );

   if( glfw->windowHeight > 0 )
      mouseY = ( int )( y * glfw->framebufferHeight / ( glfw->windowHeight * ( double )glfw->contentScaleY ) );

   mouseX = GTGLFW_MIN( GTGLFW_MAX( mouseX, 0 ), glfw->canvasWidth - 1 );
   mouseY = GTGLFW_MIN( GTGLFW_MAX( mouseY, 0 ), glfw->canvasHeight - 1 );
   glfw->mouseX = mouseX;
   glfw->mouseY = mouseY;
   glfw->mouseCol = GTGLFW_MIN( GTGLFW_MAX( mouseX / X_GTGLFW_CELL_WIDTH, 0 ), glfw->cols - 1 );
   glfw->mouseRow = GTGLFW_MIN( GTGLFW_MAX( mouseY / X_GTGLFW_CELL_HEIGHT, 0 ), glfw->rows - 1 );

   if( addEvent && ( mouseX != oldX || mouseY != oldY ) )
      s_gt_glfw_AddKey( glfw, X_INKEY_NEW_MPOS( glfw->mouseCol, glfw->mouseRow ) );
}

static void s_gt_glfw_CursorPositionCallback( GLFWwindow *window, double x, double y )
{
   X_GTGLFW *glfw = ( X_GTGLFW * )glfwGetWindowUserPointer( window );

   if( glfw && ( glfw->mouseOn || glfw->selecting ) )
   {
      s_gt_glfw_UpdateMousePosition( glfw, x, y, glfw->mouseOn && !glfw->selecting );
      s_gt_glfw_UpdateSelection( glfw );
   }
}

static void s_gt_glfw_CursorEnterCallback( GLFWwindow *window, int entered )
{
   X_GTGLFW *glfw = ( X_GTGLFW * )glfwGetWindowUserPointer( window );

   if( glfw )
   {
      glfw->cursorInside = entered == GLFW_TRUE;
      if( !glfw->cursorInside && glfw->mouseOn )
         s_gt_glfw_AddKey( glfw, K_NCMOUSEMOVE );
   }
}

static void s_gt_glfw_MouseButtonCallback( GLFWwindow *window, int glfwButton, int action, int mods )
{
   X_GTGLFW    *glfw = ( X_GTGLFW * )glfwGetWindowUserPointer( window );
   int          button = s_gt_glfw_MouseButtonIndex( glfwButton );
   int          down = action == GLFW_PRESS;
   int          selectionStart;
   int          doubleClick = 0;
   unsigned int mask;
   double       x, y;
   int          key;

   if( !glfw || button < 0 || ( action != GLFW_PRESS && action != GLFW_RELEASE ) )
      return;

   if( !glfw->mouseOn && !glfw->selectCopy && !glfw->selecting )
      return;

   glfw->keyMods = mods;
   glfwGetCursorPos( window, &x, &y );
   selectionStart = 0;
   if( down && button == X_MBUTTON_LEFT && glfw->selectCopy && !glfw->selecting )
      selectionStart = ( mods & GLFW_MOD_SHIFT ) != 0;
   s_gt_glfw_UpdateMousePosition( glfw, x, y, glfw->mouseOn && !glfw->selecting && !selectionStart );
   mask = s_gt_glfw_MouseButtonMask( button );

   if( selectionStart )
   {
      glfw->selectionAnchor = glfw->selectionEnd = glfw->mouseRow * glfw->cols + glfw->mouseCol;
      glfw->selecting = 1;
      glfw->selectionActive = 1;
      glfw->selectionSkipUp = 0;
      glfw->mouseButtons &= ~mask;
      glfw->clickTime[button] = 0.0;
      glfw->presentPending = 1;
      return;
   }
   if( !down && button == X_MBUTTON_LEFT && glfw->selecting )
   {
      s_gt_glfw_UpdateSelection( glfw );
      s_gt_glfw_CopySelection( glfw );
      s_gt_glfw_ClearSelection( glfw, 0 );
      glfw->mouseButtons &= ~mask;
      glfw->clickTime[button] = 0.0;
      return;
   }
   if( !down && button == X_MBUTTON_LEFT && glfw->selectionSkipUp )
   {
      glfw->selectionSkipUp = 0;
      return;
   }
   if( !glfw->mouseOn )
      return;

   if( down )
   {
      double now = glfwGetTime();
      int    speed = X_GTSELF_MOUSEGETDOUBLECLICKSPEED( glfw->pGT );

      doubleClick = glfw->clickTime[button] > 0.0 && ( now - glfw->clickTime[button] ) * 1000.0 <= speed;
      glfw->clickTime[button] = now;
      glfw->mouseButtons |= mask;
   }
   else
      glfw->mouseButtons &= ~mask;

   key = s_gt_glfw_MouseButtonKey( button, down, doubleClick );
   s_gt_glfw_AddKey( glfw, X_INKEY_NEW_MKEY( key, s_gt_glfw_KeyFlags( mods, GLFW_KEY_UNKNOWN ) ) );
}

static void s_gt_glfw_ScrollCallback( GLFWwindow *window, double xOffset, double yOffset )
{
   X_GTGLFW *glfw = ( X_GTGLFW * )glfwGetWindowUserPointer( window );
   double    x, y;
   int       flags;

   if( !glfw || !glfw->mouseOn )
      return;

   glfwGetCursorPos( window, &x, &y );
   s_gt_glfw_UpdateMousePosition( glfw, x, y, 1 );
   flags = glfw->keyFlags;
   glfw->wheelX += xOffset;
   glfw->wheelY += yOffset;

   while( glfw->wheelY >= 1.0 )
   {
      s_gt_glfw_AddKey( glfw, X_INKEY_NEW_MKEY( K_MWFORWARD, flags ) );
      glfw->wheelY -= 1.0;
   }
   while( glfw->wheelY <= -1.0 )
   {
      s_gt_glfw_AddKey( glfw, X_INKEY_NEW_MKEY( K_MWBACKWARD, flags ) );
      glfw->wheelY += 1.0;
   }
   while( glfw->wheelX >= 1.0 )
   {
      s_gt_glfw_AddKey( glfw, X_INKEY_NEW_MKEY( K_MWRIGHT, flags ) );
      glfw->wheelX -= 1.0;
   }
   while( glfw->wheelX <= -1.0 )
   {
      s_gt_glfw_AddKey( glfw, X_INKEY_NEW_MKEY( K_MWLEFT, flags ) );
      glfw->wheelX += 1.0;
   }
}

static void s_gt_glfw_WindowPositionCallback( GLFWwindow *window, int x, int y )
{
   X_GTGLFW *glfw = ( X_GTGLFW * )glfwGetWindowUserPointer( window );

   if( glfw )
   {
      glfw->windowX = x;
      glfw->windowY = y;
   }
}

static void s_gt_glfw_CloseCallback( GLFWwindow *window )
{
   X_GTGLFW *glfw = ( X_GTGLFW * )glfwGetWindowUserPointer( window );

   glfwSetWindowShouldClose( window, GLFW_FALSE );
   if( glfw )
      glfw->closePending = 1;
}

static void s_gt_glfw_WindowSizeCallback( GLFWwindow *window, int width, int height )
{
   X_GTGLFW *glfw = ( X_GTGLFW * )glfwGetWindowUserPointer( window );

   if( glfw )
   {
      glfw->windowWidth = width;
      glfw->windowHeight = height;
      glfw->windowResizePending = 1;
      glfw->presentPending = 1;
   }
}

static void s_gt_glfw_FramebufferSizeCallback( GLFWwindow *window, int width, int height )
{
   X_GTGLFW *glfw = ( X_GTGLFW * )glfwGetWindowUserPointer( window );

   if( glfw )
   {
      glfw->framebufferWidth = width;
      glfw->framebufferHeight = height;
      glfw->windowResizePending = 1;
      glfw->presentBothBuffers = 1;
      glfw->presentPending = 1;
   }
}

static void s_gt_glfw_ContentScaleCallback( GLFWwindow *window, float scaleX, float scaleY )
{
   X_GTGLFW *glfw = ( X_GTGLFW * )glfwGetWindowUserPointer( window );

   if( glfw )
   {
      glfw->contentScaleX = IIF( scaleX > 0.0f, scaleX, 1.0f );
      glfw->contentScaleY = IIF( scaleY > 0.0f, scaleY, 1.0f );
      glfw->windowResizePending = 1;
      glfw->presentPending = 1;
   }
}

static void s_gt_glfw_WindowRefreshCallback( GLFWwindow *window )
{
   X_GTGLFW *glfw = ( X_GTGLFW * )glfwGetWindowUserPointer( window );

   if( glfw )
      glfw->presentPending = 1;
}

static void s_gt_glfw_FocusCallback( GLFWwindow *window, int focused )
{
   X_GTGLFW *glfw = ( X_GTGLFW * )glfwGetWindowUserPointer( window );

   if( glfw )
   {
      int newFocused = focused == GLFW_TRUE;

      if( glfw->focused != newFocused )
      {
         glfw->focused = newFocused;
         if( newFocused )
            s_gt_glfw_AddKey( glfw, K_GOTFOCUS );
         else
         {
            glfw->keyFlags = 0;
            glfw->keyMods = 0;
            glfw->textScancode = 0;
            glfw->textFlags = 0;
            glfw->textRepeat = 0;
            glfw->mouseButtons = 0;
            s_gt_glfw_ClearSelection( glfw, 1 );
            glfw->clickTime[0] = glfw->clickTime[1] = glfw->clickTime[2] = 0.0;
            glfw->wheelX = glfw->wheelY = 0.0;
            s_gt_glfw_DiscardKeyRepeats( glfw, 0 );
            s_gt_glfw_AddKey( glfw, K_LOSTFOCUS );
         }
      }
      glfw->cursorVisible = 1;
      glfw->cursorTime = glfwGetTime();
      glfw->presentPending = 1;
   }
}

static void s_gt_glfw_IconifyCallback( GLFWwindow *window, int iconified )
{
   X_GTGLFW *glfw = ( X_GTGLFW * )glfwGetWindowUserPointer( window );

   if( glfw )
   {
      glfw->minimized = iconified == GLFW_TRUE;
      glfw->presentPending = 1;
   }
}

static void s_gt_glfw_MaximizeCallback( GLFWwindow *window, int maximized )
{
   X_GTGLFW *glfw = ( X_GTGLFW * )glfwGetWindowUserPointer( window );

   if( glfw )
   {
      glfw->maximized = maximized == GLFW_TRUE;
      glfw->windowResizePending = 1;
      glfw->presentPending = 1;
   }
}

static void s_gt_glfw_RegisterWindowCallbacks( X_GTGLFW *glfw )
{
   glfwSetKeyCallback( glfw->window, s_gt_glfw_KeyCallback );
   glfwSetCharCallback( glfw->window, s_gt_glfw_CharCallback );
   glfwSetCursorPosCallback( glfw->window, s_gt_glfw_CursorPositionCallback );
   glfwSetCursorEnterCallback( glfw->window, s_gt_glfw_CursorEnterCallback );
   glfwSetMouseButtonCallback( glfw->window, s_gt_glfw_MouseButtonCallback );
   glfwSetScrollCallback( glfw->window, s_gt_glfw_ScrollCallback );
   glfwSetWindowPosCallback( glfw->window, s_gt_glfw_WindowPositionCallback );
   glfwSetWindowSizeCallback( glfw->window, s_gt_glfw_WindowSizeCallback );
   glfwSetFramebufferSizeCallback( glfw->window, s_gt_glfw_FramebufferSizeCallback );
   glfwSetWindowContentScaleCallback( glfw->window, s_gt_glfw_ContentScaleCallback );
   glfwSetWindowRefreshCallback( glfw->window, s_gt_glfw_WindowRefreshCallback );
   glfwSetWindowFocusCallback( glfw->window, s_gt_glfw_FocusCallback );
   glfwSetWindowIconifyCallback( glfw->window, s_gt_glfw_IconifyCallback );
   glfwSetWindowMaximizeCallback( glfw->window, s_gt_glfw_MaximizeCallback );
   glfwSetWindowCloseCallback( glfw->window, s_gt_glfw_CloseCallback );
}

static void s_gt_glfw_UnregisterWindowCallbacks( X_GTGLFW *glfw )
{
   glfwSetKeyCallback( glfw->window, NULL );
   glfwSetCharCallback( glfw->window, NULL );
   glfwSetCursorPosCallback( glfw->window, NULL );
   glfwSetCursorEnterCallback( glfw->window, NULL );
   glfwSetMouseButtonCallback( glfw->window, NULL );
   glfwSetScrollCallback( glfw->window, NULL );
   glfwSetWindowPosCallback( glfw->window, NULL );
   glfwSetWindowSizeCallback( glfw->window, NULL );
   glfwSetFramebufferSizeCallback( glfw->window, NULL );
   glfwSetWindowContentScaleCallback( glfw->window, NULL );
   glfwSetWindowRefreshCallback( glfw->window, NULL );
   glfwSetWindowFocusCallback( glfw->window, NULL );
   glfwSetWindowIconifyCallback( glfw->window, NULL );
   glfwSetWindowMaximizeCallback( glfw->window, NULL );
   glfwSetWindowCloseCallback( glfw->window, NULL );
}

/* The cursor of the screen. A visible cursor moves only with syncPos, in ReadKey() after the program has finished
   its screen updates, as in GTSDL3; a Refresh() in between does not show it at a passing position */
static void s_gt_glfw_UpdateCursor( X_GTGLFW *glfw, int syncPos )
{
   int    oldVisible = glfw->cursorVisible;
   double now = glfwGetTime();
   int    moved = 0;
   int    row, col, style, changed;

   X_GTSELF_GETSCRCURSOR( glfw->pGT, &row, &col, &style );
   if( row < 0 || col < 0 || row >= glfw->rows || col >= glfw->cols )
      style = SC_NONE;
   if( style < SC_NONE || style > SC_SPECIAL2 )
      style = SC_NONE;

   changed = row != glfw->cursorRow || col != glfw->cursorCol || style != glfw->cursorStyle;
   if( ( syncPos || style == SC_NONE ) && changed )
   {
      glfw->cursorRow = row;
      glfw->cursorCol = col;
      glfw->cursorStyle = style;
      glfw->cursorTime = now;
      glfw->cursorVisible = 1;
      moved = 1;
   }
   if( glfw->cursorBlinkRate == 0 || !glfw->focused || glfw->cursorStyle == SC_NONE )
   {
      glfw->cursorVisible = 1;
      glfw->cursorTime = now;
   }
   else if( now - glfw->cursorTime >= glfw->cursorBlinkRate / 1000.0 )
   {
      int steps = ( int )( ( now - glfw->cursorTime ) * 1000.0 / glfw->cursorBlinkRate );

      glfw->cursorTime += steps * glfw->cursorBlinkRate / 1000.0;
      if( ( steps & 1 ) != 0 )
         glfw->cursorVisible = !glfw->cursorVisible;
   }
   if( moved || oldVisible != glfw->cursorVisible )
      glfw->presentPending = 1;
}

static void s_gt_glfw_OverlayCursor( X_GTGLFW *glfw )
{
   int      y, height;
   int      color;
   uint8_t  attr;
   uint16_t character;
   uint32_t rgb, back;
   int      top, left, row, col;

   if( !glfw->cursorVisible || glfw->cursorStyle == SC_NONE )
      return;

   switch( glfw->cursorStyle )
   {
      case SC_NORMAL:
         height = 2;
         y = X_GTGLFW_CELL_HEIGHT - 3;
         break;
      case SC_INSERT:
         height = ( X_GTGLFW_CELL_HEIGHT - 2 ) >> 1;
         y = X_GTGLFW_CELL_HEIGHT - height - 1;
         break;
      case SC_SPECIAL1:
         height = X_GTGLFW_CELL_HEIGHT - 2;
         y = 1;
         break;
      case SC_SPECIAL2:
         height = ( X_GTGLFW_CELL_HEIGHT - 2 ) >> 1;
         y = 1;
         break;
      default: return;
   }

   if( !X_GTSELF_GETSCRCHAR( glfw->pGT, glfw->cursorRow, glfw->cursorCol, &color, &attr, &character ) )
      return;

   UNUSED( attr );
   UNUSED( character );
   s_gt_glfw_CellColors( glfw, glfw->cursorRow, glfw->cursorCol, color, &rgb, &back );
   top = glfw->cursorRow * X_GTGLFW_CELL_HEIGHT + y;
   left = glfw->cursorCol * X_GTGLFW_CELL_WIDTH;

   for( row = top; row < top + height; ++row )
      for( col = left; col < left + X_GTGLFW_CELL_WIDTH; ++col )
      {
         uint8_t *pixel = glfw->frame + ( ( size_t )row * glfw->canvasWidth + col ) * 4;

         pixel[0] = ( uint8_t )( rgb >> 16 );
         pixel[1] = ( uint8_t )( rgb >> 8 );
         pixel[2] = ( uint8_t )rgb;
         pixel[3] = 255;
      }
}

static void s_gt_glfw_OverlaySelection( X_GTGLFW *glfw )
{
   int anchorRow = glfw->selectionAnchor / glfw->cols;
   int anchorCol = glfw->selectionAnchor % glfw->cols;
   int endRow = glfw->selectionEnd / glfw->cols;
   int endCol = glfw->selectionEnd % glfw->cols;
   int left = GTGLFW_MIN( anchorCol, endCol ) * X_GTGLFW_CELL_WIDTH;
   int top = GTGLFW_MIN( anchorRow, endRow ) * X_GTGLFW_CELL_HEIGHT;
   int right = ( GTGLFW_MAX( anchorCol, endCol ) + 1 ) * X_GTGLFW_CELL_WIDTH;
   int bottom = ( GTGLFW_MAX( anchorRow, endRow ) + 1 ) * X_GTGLFW_CELL_HEIGHT;
   int row, col;

   for( row = top; row < bottom; ++row )
      for( col = left; col < right; ++col )
      {
         uint8_t *pixel = glfw->frame + ( ( size_t )row * glfw->canvasWidth + col ) * 4;

         pixel[0] = ( uint8_t )( ( 64 * 112 + pixel[0] * 143 + 127 ) / 255 );
         pixel[1] = ( uint8_t )( ( 128 * 112 + pixel[1] * 143 + 127 ) / 255 );
         pixel[2] = ( uint8_t )( ( 255 * 112 + pixel[2] * 143 + 127 ) / 255 );
      }
}

static void s_gt_glfw_Present( X_GTGLFW *glfw )
{
   size_t pixelCount;
   size_t index;
   int    width, height;
   int    pass, passes;
   int    canvasWidth = glfw->canvasWidth;
   int    canvasHeight = glfw->canvasHeight;

   if( !glfw->window || !glfw->canvas || !glfw->frame || glfw->texture == 0 || X_GTSELF_DISPCOUNT( glfw->pGT ) != 0 )
      return;

   glfwGetFramebufferSize( glfw->window, &glfw->framebufferWidth, &glfw->framebufferHeight );
   if( glfw->framebufferWidth <= 0 || glfw->framebufferHeight <= 0 )
      return;

   pixelCount = ( size_t )glfw->canvasWidth * glfw->canvasHeight;
   for( index = 0; index < pixelCount; ++index )
   {
      uint32_t color = glfw->canvas[index];
      uint8_t *pixel = glfw->frame + index * 4;

      pixel[0] = ( uint8_t )( color >> 16 );
      pixel[1] = ( uint8_t )( color >> 8 );
      pixel[2] = ( uint8_t )color;
      pixel[3] = 255;
   }
   if( glfw->selectionActive )
      s_gt_glfw_OverlaySelection( glfw );
   else
      s_gt_glfw_OverlayCursor( glfw );

   glfwMakeContextCurrent( glfw->window );
   glMatrixMode( GL_PROJECTION );
   glLoadIdentity();
   glMatrixMode( GL_MODELVIEW );
   glLoadIdentity();
   glDisable( GL_DEPTH_TEST );
   glDisable( GL_BLEND );
   glEnable( GL_TEXTURE_2D );
   glBindTexture( GL_TEXTURE_2D, glfw->texture );
   glTexSubImage2D( GL_TEXTURE_2D, 0, 0, 0, canvasWidth, canvasHeight, GL_RGBA, GL_UNSIGNED_BYTE, glfw->frame );
   /* the grid is drawn 1:1 in the top left corner, the strip outside it is the margin */
   width = GTGLFW_MIN( ( int )( glfw->canvasWidth * glfw->contentScaleX + 0.5f ), glfw->framebufferWidth );
   height = GTGLFW_MIN( ( int )( glfw->canvasHeight * glfw->contentScaleY + 0.5f ), glfw->framebufferHeight );
   passes = IIF( glfw->presentBothBuffers, 2, 1 );
   glfw->presentBothBuffers = 0;
   for( pass = 0; pass < passes; ++pass )
   {
      glViewport( 0, 0, glfw->framebufferWidth, glfw->framebufferHeight );
      s_gt_glfw_ClearMargin( glfw );
      glViewport( 0, glfw->framebufferHeight - height, width, height );
      glColor4ub( 255, 255, 255, 255 );
      glBegin( GL_QUADS );
      glTexCoord2f( 0.0f, 0.0f );
      glVertex2f( -1.0f, 1.0f );
      glTexCoord2f( 1.0f, 0.0f );
      glVertex2f( 1.0f, 1.0f );
      glTexCoord2f( 1.0f, 1.0f );
      glVertex2f( 1.0f, -1.0f );
      glTexCoord2f( 0.0f, 1.0f );
      glVertex2f( -1.0f, -1.0f );
      glEnd();
      glfwSwapBuffers( glfw->window );
   }
   glfw->canvasDirty = 0;
   glfw->presentPending = 0;
}

static void s_gt_glfw_Error( int code, const char *description )
{
   s_errorCode = code;
   snprintf( s_errorText, sizeof( s_errorText ), "%s", IIF( description, description, "unknown GLFW error" ) );
}

static int s_gt_glfw_Start( void )
{
   if( s_glfwUsers == 0 )
   {
      const char *description = NULL;

      s_errorCode = GLFW_NO_ERROR;
      s_errorText[0] = '\0';
      glfwSetErrorCallback( s_gt_glfw_Error );

      if( !glfwInit() )
      {
         int code = glfwGetError( &description );

         if( s_errorText[0] == '\0' && description )
            s_gt_glfw_Error( code, description );
         glfwSetErrorCallback( NULL );
         return 0;
      }
   }

   ++s_glfwUsers;

   return 1;
}

static void s_gt_glfw_Stop( void )
{
   if( s_glfwUsers > 0 && --s_glfwUsers == 0 )
   {
      glfwTerminate();
      glfwSetErrorCallback( NULL );
      s_errorCode = GLFW_NO_ERROR;
      s_errorText[0] = '\0';
   }
}

static void s_gt_glfw_InitFailed( X_GTGLFW *glfw, const char *message )
{
   fprintf( stderr, "xDev GTGLFW: %s", message );
   if( s_errorText[0] )
      fprintf( stderr, " (%d): %s", s_errorCode, s_errorText );
   fputc( '\n', stderr );

   if( glfw )
   {
      s_gt_glfw_DestroyCanvas( glfw );
      if( glfw->window )
         glfwDestroyWindow( glfw->window );
      x_xfree( glfw->title );
      x_xfree( glfw );
   }
   s_gt_glfw_Stop();
   x_quitRaise();
}

static void s_gt_glfw_Redraw( X_GT_BASE *pGT, int row, int col, int size )
{
   X_GTGLFW *glfw = X_GTGLFW_GET( pGT );

   if( !glfw || row < 0 || row >= glfw->rows )
      return;

   while( size-- > 0 && col < glfw->cols )
   {
      int      color;
      uint8_t  attr;
      uint16_t character;
      uint32_t fore, back;
      int      top = row * X_GTGLFW_CELL_HEIGHT;
      int      left = col * X_GTGLFW_CELL_WIDTH;

      if( col >= 0 && X_GTSELF_GETSCRCHAR( pGT, row, col, &color, &attr, &character ) )
      {
         UNUSED( attr );
         s_gt_glfw_CellColors( glfw, row, col, color, &fore, &back );
         s_gt_glfw_FillRect( glfw, top, left, top + X_GTGLFW_CELL_HEIGHT - 1, left + X_GTGLFW_CELL_WIDTH - 1, back );
         character = x_u16Ctrl( character );
         if( character != ' ' && character != 0x00A0 )
            s_gt_glfw_DrawGlyph( glfw, character, top, left, fore );
      }
      ++col;
   }

   glfw->canvasDirty = 1;
   glfw->presentPending = 1;
}

static void s_gt_glfw_Refresh( X_GT_BASE *pGT )
{
   X_GTGLFW *glfw = X_GTGLFW_GET( pGT );

   if( !glfw )
      return;

   s_gt_glfw_PumpEvents();
   s_super.Refresh( pGT );
   glfw->resizePending = 0;
   s_gt_glfw_UpdateCursor( glfw, 0 );
   if( glfw->presentPending )
      s_gt_glfw_Present( glfw );
}

static int s_gt_glfw_ReadKey( X_GT_BASE *pGT, int mask )
{
   X_GTGLFW *glfw = X_GTGLFW_GET( pGT );

   UNUSED( mask );
   s_gt_glfw_PumpEvents();
   if( glfw->resizePending )
   {
      s_super.Refresh( pGT );
      glfw->resizePending = 0;
   }
   s_gt_glfw_UpdateCursor( glfw, 1 );
   if( glfw->presentPending )
      s_gt_glfw_Present( glfw );

   if( glfw->keyOut != glfw->keyIn )
   {
      int key = glfw->keyQueue[glfw->keyOut];

      glfw->keyOut = ( glfw->keyOut + 1 ) % X_GTGLFW_KEY_QUEUE_SIZE;
      return key;
   }

   return 0;
}

static int s_gt_glfw_mouse_IsPresent( X_GT_BASE *pGT )
{
   X_GTGLFW *glfw = X_GTGLFW_GET( pGT );

   return glfw->mouseOn;
}

static void s_gt_glfw_mouse_GetPos( X_GT_BASE *pGT, int *row, int *col )
{
   X_GTGLFW *glfw = X_GTGLFW_GET( pGT );

   *row = glfw->mouseRow;
   *col = glfw->mouseCol;
}

static void s_gt_glfw_mouse_SetPos( X_GT_BASE *pGT, int row, int col )
{
   X_GTGLFW *glfw = X_GTGLFW_GET( pGT );
   double    x, y;

   row = GTGLFW_MIN( GTGLFW_MAX( row, 0 ), glfw->rows - 1 );
   col = GTGLFW_MIN( GTGLFW_MAX( col, 0 ), glfw->cols - 1 );
   glfw->mouseRow = row;
   glfw->mouseCol = col;
   glfw->mouseX = col * X_GTGLFW_CELL_WIDTH + X_GTGLFW_CELL_WIDTH / 2;
   glfw->mouseY = row * X_GTGLFW_CELL_HEIGHT + X_GTGLFW_CELL_HEIGHT / 2;
   x = ( glfw->mouseX + 0.5 ) * glfw->windowWidth / glfw->canvasWidth;
   y = ( glfw->mouseY + 0.5 ) * glfw->windowHeight / glfw->canvasHeight;
   glfwSetCursorPos( glfw->window, x, y );
}

static int s_gt_glfw_mouse_ButtonState( X_GT_BASE *pGT, int button )
{
   X_GTGLFW    *glfw = X_GTGLFW_GET( pGT );
   unsigned int mask = s_gt_glfw_MouseButtonMask( button );

   return glfw->mouseOn && ( glfw->mouseButtons & mask ) != 0;
}

static int s_gt_glfw_mouse_CountButton( X_GT_BASE *pGT )
{
   UNUSED( pGT );

   return 3;
}

static int s_gt_glfw_PaletteValue( const uint8_t color[3] )
{
   return color[0] | ( ( int )color[1] << 8 ) | ( ( int )color[2] << 16 );
}

static int s_gt_glfw_SetPaletteValue( X_GTGLFW *glfw, int index, int64_t value )
{
   if( value < 0 || value > 0xFFFFFF || value == s_gt_glfw_PaletteValue( glfw->colors[index] ) )
      return 0;

   glfw->colors[index][0] = ( uint8_t )value;
   glfw->colors[index][1] = ( uint8_t )( value >> 8 );
   glfw->colors[index][2] = ( uint8_t )( value >> 16 );

   return 1;
}

static int s_gt_glfw_InfoInteger( X_ITEM *item, int minimum, int maximum, int64_t *value )
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

static void s_gt_glfw_InfoPairResult( X_GT_INFO *info, int first, int second )
{
   if( !info->pResult )
      info->pResult = x_itemNew( NULL );
   x_arrayNew( info->pResult, 2 );
   x_arraySetNI( info->pResult, 1, first );
   x_arraySetNI( info->pResult, 2, second );
}

static int s_gt_glfw_InfoPairInput( X_GT_INFO *info, int *first, int *second )
{
   int64_t firstValue, secondValue;
   X_ITEM *firstItem = info->pNewVal;
   X_ITEM *secondItem = info->pNewVal2;

   if( firstItem && X_IS_ARRAY( firstItem ) && x_arrayLen( firstItem ) == 2 )
   {
      secondItem = x_arrayGetItemPtr( firstItem, 2 );
      firstItem = x_arrayGetItemPtr( firstItem, 1 );
   }
   if( !s_gt_glfw_InfoInteger( firstItem, INT_MIN, INT_MAX, &firstValue ) )
      return 0;

   if( !s_gt_glfw_InfoInteger( secondItem, INT_MIN, INT_MAX, &secondValue ) )
      return 0;

   *first = ( int )firstValue;
   *second = ( int )secondValue;

   return 1;
}

static int s_gt_glfw_SetScreenSize( X_GTGLFW *glfw, int width, int height )
{
   int rows, cols;

   if( width <= 0 || height <= 0 )
      return 0;

   cols = GTGLFW_MAX( width / X_GTGLFW_CELL_WIDTH, X_GTGLFW_MIN_COLS );
   rows = GTGLFW_MAX( height / X_GTGLFW_CELL_HEIGHT, X_GTGLFW_MIN_ROWS );
   if( cols > X_GTGLFW_MAX_COLS || rows > X_GTGLFW_MAX_ROWS )
      return 0;

   return s_gt_glfw_ResizeGrid( glfw, rows, cols, 1, 1 );
}

static int s_gt_glfw_KbdShifts( X_GTGLFW *glfw )
{
   int shifts = 0;
   int leftShift = glfwGetKey( glfw->window, GLFW_KEY_LEFT_SHIFT ) == GLFW_PRESS;
   int rightShift = glfwGetKey( glfw->window, GLFW_KEY_RIGHT_SHIFT ) == GLFW_PRESS;
   int leftCtrl = glfwGetKey( glfw->window, GLFW_KEY_LEFT_CONTROL ) == GLFW_PRESS;
   int rightCtrl = glfwGetKey( glfw->window, GLFW_KEY_RIGHT_CONTROL ) == GLFW_PRESS;
   int leftAlt = glfwGetKey( glfw->window, GLFW_KEY_LEFT_ALT ) == GLFW_PRESS;
   int rightAlt = glfwGetKey( glfw->window, GLFW_KEY_RIGHT_ALT ) == GLFW_PRESS;
   int leftSuper = glfwGetKey( glfw->window, GLFW_KEY_LEFT_SUPER ) == GLFW_PRESS;
   int rightSuper = glfwGetKey( glfw->window, GLFW_KEY_RIGHT_SUPER ) == GLFW_PRESS;

   if( leftShift || rightShift )
      shifts |= X_GTI_KBD_SHIFT;
   if( leftCtrl || rightCtrl )
      shifts |= X_GTI_KBD_CTRL;
   if( leftAlt || rightAlt )
      shifts |= X_GTI_KBD_ALT;
   if( leftShift )
      shifts |= X_GTI_KBD_LSHIFT;
   if( rightShift )
      shifts |= X_GTI_KBD_RSHIFT;
   if( leftCtrl )
      shifts |= X_GTI_KBD_LCTRL;
   if( rightCtrl )
      shifts |= X_GTI_KBD_RCTRL;
   if( leftAlt )
      shifts |= X_GTI_KBD_LALT;
   if( rightAlt )
      shifts |= X_GTI_KBD_RALT;
   if( leftSuper )
      shifts |= X_GTI_KBD_LWIN;
   if( rightSuper )
      shifts |= X_GTI_KBD_RWIN;
   if( ( glfw->keyMods & GLFW_MOD_NUM_LOCK ) != 0 )
      shifts |= X_GTI_KBD_NUMLOCK;
   if( ( glfw->keyMods & GLFW_MOD_CAPS_LOCK ) != 0 )
      shifts |= X_GTI_KBD_CAPSLOCK;

   return shifts;
}

static X_ITEM *s_gt_glfw_RendererName( X_ITEM *result, X_GTGLFW *glfw )
{
   GLFWwindow    *previous = glfwGetCurrentContext();
   const GLubyte *renderer;
   const GLubyte *version;
   const char    *rendererText = "unknown renderer";
   const char    *versionText = "unknown version";
   char           value[1024];

   if( previous != glfw->window )
      glfwMakeContextCurrent( glfw->window );
   renderer = glGetString( GL_RENDERER );
   version = glGetString( GL_VERSION );
   if( renderer )
      rendererText = ( const char * )renderer;
   if( version )
      versionText = ( const char * )version;
   snprintf( value, sizeof( value ), "GLFW/OpenGL %s %s", rendererText, versionText );
   if( previous != glfw->window )
      glfwMakeContextCurrent( previous );

   return x_itemPutC( result, value );
}

static X_ITEM *s_gt_glfw_WindowHandle( X_ITEM *result, X_GTGLFW *glfw )
{
#if X_OS_WINDOWS
   return x_itemPutPtr( result, glfwGetWin32Window( glfw->window ) );
#elif X_OS_LINUX
#if GLFW_VERSION_MAJOR > 3 || ( GLFW_VERSION_MAJOR == 3 && GLFW_VERSION_MINOR >= 4 )
   if( glfwGetPlatform() == GLFW_PLATFORM_X11 )
      return x_itemPutNInt( result, ( int64_t )glfwGetX11Window( glfw->window ) );
#endif
   return x_itemPutPtr( result, NULL );
#else
   UNUSED( glfw );

   return x_itemPutPtr( result, NULL );
#endif
}

/* ---------------------------------------------------------------------------------------------
   Images shared by ICONFILE and DISPIMAGE
   --------------------------------------------------------------------------------------------- */

static uint16_t s_gt_glfw_ImageU16( const unsigned char *data )
{
   return ( uint16_t )( data[0] | ( ( uint16_t )data[1] << 8 ) );
}

static uint32_t s_gt_glfw_ImageU32( const unsigned char *data )
{
   return ( uint32_t )s_gt_glfw_ImageU16( data ) | ( ( uint32_t )s_gt_glfw_ImageU16( data + 2 ) << 16 );
}

static void s_gt_glfw_ImageFree( X_GTGLFW_IMAGE *image )
{
   x_xfree( image->pixels );
   image->pixels = NULL;
   image->width = image->height = 0;
}

/* Portable Linux BMP reader. It accepts the common uncompressed 24 and 32 bit variants;
   raw xDev images cover the remaining terminal formats. Windows uses WIC below. */
#if !X_OS_WINDOWS
static int s_gt_glfw_ImageLoadBMP( const char *filename, X_GTGLFW_IMAGE *image )
{
   unsigned char  header[54];
   unsigned char *row = NULL;
   FILE          *file = NULL;
   uint32_t       offset, dibSize, compression;
   int32_t        width, signedHeight;
   int            height, sourceY, y, x;
   unsigned int   depth;
   size_t         stride, count;
   int            result = 0;

   memset( image, 0, sizeof( *image ) );
   file = fopen( filename, "rb" );
   if( !file || fread( header, 1, sizeof( header ), file ) != sizeof( header ) || header[0] != 'B' || header[1] != 'M' )
      goto done;
   offset = s_gt_glfw_ImageU32( header + 10 );
   dibSize = s_gt_glfw_ImageU32( header + 14 );
   width = ( int32_t )s_gt_glfw_ImageU32( header + 18 );
   signedHeight = ( int32_t )s_gt_glfw_ImageU32( header + 22 );
   depth = s_gt_glfw_ImageU16( header + 28 );
   compression = s_gt_glfw_ImageU32( header + 30 );
   if( dibSize < 40 || width <= 0 || signedHeight == 0 || signedHeight == INT_MIN )
      goto done;
   if( ( depth != 24 && depth != 32 ) || compression != 0 )
      goto done;
   height = IIF( signedHeight < 0, -signedHeight, signedHeight );
   if( width > 16384 || height > 16384 || ( size_t )width > 16777216U / ( size_t )height )
      goto done;
   stride = ( ( ( size_t )width * depth + 31 ) / 32 ) * 4;
   count = ( size_t )width * height;
   if( stride > SIZE_MAX / ( size_t )height || offset > LONG_MAX || fseek( file, ( long )offset, SEEK_SET ) != 0 )
      goto done;
   row = ( unsigned char * )x_xgrab( stride );
   image->pixels = ( uint32_t * )x_xgrabArray( count, sizeof( uint32_t ) );
   image->width = width;
   image->height = height;
   for( sourceY = 0; sourceY < height; ++sourceY )
   {
      if( fread( row, 1, stride, file ) != stride )
         goto done;
      y = IIF( signedHeight < 0, sourceY, height - sourceY - 1 );
      for( x = 0; x < width; ++x )
      {
         const unsigned char *pixel = row + ( size_t )x * ( depth / 8 );
         uint32_t             color = ( ( uint32_t )pixel[2] << 16 ) | ( ( uint32_t )pixel[1] << 8 ) | pixel[0];

         image->pixels[( size_t )y * width + x] = 0xFF000000u | color;
      }
   }
   result = 1;

done:
   if( file )
      fclose( file );
   x_xfree( row );
   if( !result )
      s_gt_glfw_ImageFree( image );

   return result;
}
#endif

static int s_gt_glfw_ImageLoadFile( const char *filename, X_GTGLFW_IMAGE *image )
{
#if X_OS_WINDOWS
   HRESULT                init = CoInitializeEx( NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE );
   IWICImagingFactory    *factory = NULL;
   IWICBitmapDecoder     *decoder = NULL;
   IWICBitmapFrameDecode *frame = NULL;
   IWICFormatConverter   *converter = NULL;
   IWICBitmapSource      *source = NULL;
   const GUID            *format = &GUID_WICPixelFormat32bppBGRA;
   WICDecodeOptions       options = WICDecodeMetadataCacheOnDemand;
   WICBitmapPaletteType   palette = WICBitmapPaletteTypeCustom;
   void                  *object = NULL;
   wchar_t               *wide;
   size_t                 wideLength;
   GUID                   container;
   UINT                   width = 0, height = 0;
   int                    supported = 0;
   HRESULT                hr;
   int                    result = 0;

   memset( image, 0, sizeof( *image ) );
   wide = ( wchar_t * )x_utf8ToUtf16Alloc( filename, strlen( filename ), &wideLength );
   hr = CoCreateInstance( &CLSID_WICImagingFactory, NULL, CLSCTX_INPROC_SERVER, &IID_IWICImagingFactory, &object );
   factory = ( IWICImagingFactory * )object;
   if( SUCCEEDED( hr ) )
      hr = factory->lpVtbl->CreateDecoderFromFilename( factory, wide, NULL, GENERIC_READ, options, &decoder );
   if( SUCCEEDED( hr ) )
      hr = decoder->lpVtbl->GetContainerFormat( decoder, &container );
   if( SUCCEEDED( hr ) )
   {
      if( IsEqualGUID( &container, &GUID_ContainerFormatBmp ) )
         supported = 1;
      if( IsEqualGUID( &container, &GUID_ContainerFormatPng ) )
         supported = 1;
      if( !supported )
         hr = E_FAIL;
   }
   if( SUCCEEDED( hr ) )
      hr = decoder->lpVtbl->GetFrame( decoder, 0, &frame );
   if( SUCCEEDED( hr ) )
      hr = factory->lpVtbl->CreateFormatConverter( factory, &converter );
   if( SUCCEEDED( hr ) )
   {
      source = ( IWICBitmapSource * )frame;
      hr = converter->lpVtbl->Initialize( converter, source, format, WICBitmapDitherTypeNone, NULL, 0.0, palette );
   }
   if( SUCCEEDED( hr ) )
      hr = converter->lpVtbl->GetSize( converter, &width, &height );
   if( SUCCEEDED( hr ) && ( width == 0 || height == 0 || width > 16384 || height > 16384 ) )
      hr = E_FAIL;
   if( SUCCEEDED( hr ) && ( uint64_t )width * height > 16777216U )
      hr = E_FAIL;
   if( SUCCEEDED( hr ) )
   {
      image->pixels = ( uint32_t * )x_xgrabArray( ( size_t )width * height, sizeof( uint32_t ) );
      hr = converter->lpVtbl->CopyPixels( converter, NULL, width * 4, width * height * 4, ( BYTE * )image->pixels );
   }
   if( SUCCEEDED( hr ) )
   {
      image->width = ( int )width;
      image->height = ( int )height;
      result = 1;
   }
   if( converter )
      converter->lpVtbl->Release( converter );
   if( frame )
      frame->lpVtbl->Release( frame );
   if( decoder )
      decoder->lpVtbl->Release( decoder );
   if( factory )
      factory->lpVtbl->Release( factory );
   if( SUCCEEDED( init ) )
      CoUninitialize();
   x_xfree( wide );
   if( !result )
      s_gt_glfw_ImageFree( image );
   return result;
#else
   return s_gt_glfw_ImageLoadBMP( filename, image );
#endif
}

static int s_gt_glfw_ImageLoadRaw( X_ITEM *value, X_GTGLFW_IMAGE *image )
{
   X_ITEM              *data;
   const unsigned char *source;
   size_t               length = 0, tight, pitch, required;
   int                  width, height, depth, x, y;

   memset( image, 0, sizeof( *image ) );
   if( !value || !X_IS_ARRAY( value ) || ( x_arrayLen( value ) != 3 && x_arrayLen( value ) != 4 ) )
      return 0;

   data = x_arrayGetItemPtr( value, 1 );
   width = x_arrayGetNI( value, 2 );
   height = x_arrayGetNI( value, 3 );
   depth = IIF( x_arrayLen( value ) == 4, x_arrayGetNI( value, 4 ), 32 );
   if( depth == 0 )
      depth = 32;
   if( width <= 0 || height <= 0 || width > 16384 || height > 16384 )
      return 0;

   if( ( size_t )width > 16777216U / ( size_t )height )
      return 0;

   if( depth != 1 && depth != 16 && depth != 24 && depth != 32 )
      return 0;

   tight = ( ( size_t )width * depth + 7 ) / 8;
   pitch = ( tight + 3 ) & ~( size_t )3;
   if( X_IS_STRING( data ) )
   {
      source = ( const unsigned char * )x_itemGetCPtr( data );
      length = x_itemGetCLen( data );
      if( length == ( size_t )height * tight )
         pitch = tight;
      required = ( size_t )height * pitch;
      if( length < required )
         return 0;
   }
   else
      source = ( const unsigned char * )x_itemGetPtr( data );
   if( !source )
      return 0;

   image->pixels = ( uint32_t * )x_xgrabArray( ( size_t )width * height, sizeof( uint32_t ) );
   image->width = width;
   image->height = height;
   for( y = 0; y < height; ++y )
   {
      const unsigned char *line = source + ( size_t )y * pitch;

      for( x = 0; x < width; ++x )
      {
         uint32_t color;

         if( depth == 1 )
            color = IIF( line[x >> 3] & ( 0x80U >> ( x & 7 ) ), 0xFF000000u, 0xFFFFFFFFu );
         else if( depth == 16 )
         {
            uint16_t pixel = s_gt_glfw_ImageU16( line + ( size_t )x * 2 );
            uint32_t red = ( pixel >> 11 ) & 31;
            uint32_t green = ( pixel >> 5 ) & 63;
            uint32_t blue = pixel & 31;

            /* Replicate the high bits, as SDL's RGB565 conversion does. */
            red = ( red << 3 ) | ( red >> 2 );
            green = ( green << 2 ) | ( green >> 4 );
            blue = ( blue << 3 ) | ( blue >> 2 );
            color = 0xFF000000u | ( red << 16 ) | ( green << 8 ) | blue;
         }
         else if( depth == 24 )
         {
            const unsigned char *pixel = line + ( size_t )x * 3;

            color = 0xFF000000u | ( ( uint32_t )pixel[0] << 16 ) | ( ( uint32_t )pixel[1] << 8 ) | pixel[2];
         }
         else
            color = 0xFF000000u | ( s_gt_glfw_ImageU32( line + ( size_t )x * 4 ) & 0xFFFFFFu );
         image->pixels[( size_t )y * width + x] = color;
      }
   }

   return 1;
}

static int s_gt_glfw_ImageDestination( X_GTGLFW *glfw, const X_GTGLFW_IMAGE *image, X_ITEM *area,
                                       X_GTGLFW_RECT *destination )
{
   destination->left = destination->top = 0;
   destination->right = glfw->canvasWidth - 1;
   destination->bottom = glfw->canvasHeight - 1;
   if( area && X_IS_ARRAY( area ) )
   {
      size_t length = x_arrayLen( area );

      if( length != 2 && length != 4 )
         return 0;

      destination->left = x_arrayGetNI( area, 1 );
      destination->top = x_arrayGetNI( area, 2 );
      if( length == 2 )
      {
         int width = IIF( image, image->width, glfw->canvasWidth - destination->left );
         int height = IIF( image, image->height, glfw->canvasHeight - destination->top );

         destination->right = destination->left + width - 1;
         destination->bottom = destination->top + height - 1;
      }
      else
      {
         destination->right = x_arrayGetNI( area, 3 );
         destination->bottom = x_arrayGetNI( area, 4 );
      }
   }
   else if( image && area && X_IS_LOGICAL( area ) && x_itemGetL( area ) )
   {
      double scaleX = ( double )glfw->canvasWidth / image->width;
      double scaleY = ( double )glfw->canvasHeight / image->height;
      double scale = GTGLFW_MIN( scaleX, scaleY );
      int    width = GTGLFW_MAX( ( int )floor( image->width * scale + 0.5 ), 1 );
      int    height = GTGLFW_MAX( ( int )floor( image->height * scale + 0.5 ), 1 );

      destination->left = ( glfw->canvasWidth - width ) / 2;
      destination->top = ( glfw->canvasHeight - height ) / 2;
      destination->right = destination->left + width - 1;
      destination->bottom = destination->top + height - 1;
   }

   if( destination->right < destination->left || destination->bottom < destination->top )
      return 0;

   if( destination->left >= glfw->canvasWidth || destination->top >= glfw->canvasHeight )
      return 0;

   return destination->right >= 0 && destination->bottom >= 0;
}

static int s_gt_glfw_DispImage( X_GT_BASE *pGT, X_GTGLFW *glfw, X_ITEM *value, X_ITEM *area )
{
   X_GTGLFW_IMAGE image;
   X_GTGLFW_RECT  destination;
   int            loaded = 0;
   int            left, top, right, bottom, x, y;
   int            width, height;

   if( value && X_IS_STRING( value ) && x_itemGetCLen( value ) == 0 )
   {
      if( !s_gt_glfw_ImageDestination( glfw, NULL, area, &destination ) )
         return 0;

      top = GTGLFW_MAX( destination.top, 0 ) / X_GTGLFW_CELL_HEIGHT;
      left = GTGLFW_MAX( destination.left, 0 ) / X_GTGLFW_CELL_WIDTH;
      bottom = GTGLFW_MIN( destination.bottom, glfw->canvasHeight - 1 ) / X_GTGLFW_CELL_HEIGHT;
      right = GTGLFW_MIN( destination.right, glfw->canvasWidth - 1 ) / X_GTGLFW_CELL_WIDTH;
      X_GTSELF_EXPOSEAREA( pGT, top, left, bottom, right );
      s_super.Refresh( pGT );
      glfw->canvasDirty = glfw->presentPending = 1;
      s_gt_glfw_Present( glfw );
      return 1;
   }
   memset( &image, 0, sizeof( image ) );
   if( value && X_IS_STRING( value ) && x_itemGetCLen( value ) == strlen( x_itemGetCPtr( value ) ) )
      loaded = s_gt_glfw_ImageLoadFile( x_itemGetCPtr( value ), &image );
   else
      loaded = s_gt_glfw_ImageLoadRaw( value, &image );
   if( !loaded || !s_gt_glfw_ImageDestination( glfw, &image, area, &destination ) )
   {
      s_gt_glfw_ImageFree( &image );
      return 0;
   }

   s_super.Refresh( pGT );
   left = GTGLFW_MAX( destination.left, 0 );
   top = GTGLFW_MAX( destination.top, 0 );
   right = GTGLFW_MIN( destination.right, glfw->canvasWidth - 1 );
   bottom = GTGLFW_MIN( destination.bottom, glfw->canvasHeight - 1 );
   width = destination.right - destination.left + 1;
   height = destination.bottom - destination.top + 1;
   for( y = top; y <= bottom; ++y )
      for( x = left; x <= right; ++x )
      {
         int          sourceX = ( int )( ( int64_t )( x - destination.left ) * image.width / width );
         int          sourceY = ( int )( ( int64_t )( y - destination.top ) * image.height / height );
         uint32_t     source = image.pixels[( size_t )sourceY * image.width + sourceX];
         unsigned int alpha = source >> 24;

         if( alpha == 255 )
            glfw->canvas[( size_t )y * glfw->canvasWidth + x] = source & 0xFFFFFFu;
         else if( alpha != 0 )
         {
            uint32_t     target = glfw->canvas[( size_t )y * glfw->canvasWidth + x];
            unsigned int inverse = 255 - alpha;
            unsigned int red = ( ( source >> 16 ) & 255 ) * alpha + ( ( target >> 16 ) & 255 ) * inverse;
            unsigned int green = ( ( source >> 8 ) & 255 ) * alpha + ( ( target >> 8 ) & 255 ) * inverse;
            unsigned int blue = ( source & 255 ) * alpha + ( target & 255 ) * inverse;

            red = ( red + 127 ) / 255;
            green = ( green + 127 ) / 255;
            blue = ( blue + 127 ) / 255;
            glfw->canvas[( size_t )y * glfw->canvasWidth + x] = ( red << 16 ) | ( green << 8 ) | blue;
         }
      }
   s_gt_glfw_ImageFree( &image );
   glfw->canvasDirty = glfw->presentPending = 1;
   if( X_GTSELF_DISPCOUNT( pGT ) == 0 )
      s_gt_glfw_Present( glfw );

   return 1;
}

static int s_gt_glfw_SetIconFile( X_GTGLFW *glfw, X_ITEM *value )
{
   if( !value || !X_IS_STRING( value ) || x_itemGetCLen( value ) != strlen( x_itemGetCPtr( value ) ) )
      return 0;
#if X_OS_WINDOWS
   {
      size_t   count;
      wchar_t *filename = ( wchar_t * )x_utf8ToUtf16Alloc( x_itemGetCPtr( value ), x_itemGetCLen( value ), &count );
      HICON    icon = NULL;
      HWND     window = glfwGetWin32Window( glfw->window );

      if( *filename )
         icon = ( HICON )LoadImageW( NULL, filename, IMAGE_ICON, 0, 0, LR_LOADFROMFILE | LR_DEFAULTSIZE );
      x_xfree( filename );
      if( *x_itemGetCPtr( value ) && !icon )
         return 0;

      SendMessageW( window, WM_SETICON, ICON_SMALL, ( LPARAM )icon );
      SendMessageW( window, WM_SETICON, ICON_BIG, ( LPARAM )icon );
      if( glfw->iconOwned && glfw->iconHandle )
         DestroyIcon( ( HICON )glfw->iconHandle );
      glfw->iconHandle = icon;
      glfw->iconOwned = icon != NULL;
      return 1;
   }
#else
   {
      X_GTGLFW_IMAGE image;
      GLFWimage      icon;
      unsigned char *rgba;
      size_t         index, count;

      if( x_itemGetCLen( value ) == 0 )
      {
         glfwSetWindowIcon( glfw->window, 0, NULL );
         return 1;
      }
      if( !s_gt_glfw_ImageLoadFile( x_itemGetCPtr( value ), &image ) )
         return 0;

      count = ( size_t )image.width * image.height;
      rgba = ( unsigned char * )x_xgrabArray( count, 4 );
      for( index = 0; index < count; ++index )
      {
         rgba[index * 4] = ( unsigned char )( image.pixels[index] >> 16 );
         rgba[index * 4 + 1] = ( unsigned char )( image.pixels[index] >> 8 );
         rgba[index * 4 + 2] = ( unsigned char )image.pixels[index];
         rgba[index * 4 + 3] = ( unsigned char )( image.pixels[index] >> 24 );
      }
      icon.width = image.width;
      icon.height = image.height;
      icon.pixels = rgba;
      glfwSetWindowIcon( glfw->window, 1, &icon );
      x_xfree( rgba );
      s_gt_glfw_ImageFree( &image );
      return 1;
   }
#endif
}

/* ---------------------------------------------------------------------------------------------
   Native services used by the second GtInfo() group
   --------------------------------------------------------------------------------------------- */

static X_ITEM *s_gt_glfw_FileDialogResult( X_ITEM *result, int accepted, X_ITEM *files, int filter, const char *error )
{
   X_ITEM value = X_ITEM_NIL_INIT;

   if( result )
      x_arrayNew( result, 4 );
   else
      result = x_itemArrayNew( 4 );
   x_itemPutL( &value, accepted );
   x_arraySet( result, 1, &value );
   x_arraySet( result, 2, files );
   x_itemPutNI( &value, filter );
   x_arraySet( result, 3, &value );
   x_itemPutC( &value, IIF( error, error, "" ) );
   x_arraySet( result, 4, &value );
   x_itemClear( &value );

   return result;
}

#if X_OS_WINDOWS

static wchar_t *s_gt_glfw_ItemToWide( X_ITEM *item )
{
   size_t count;

   return ( wchar_t * )x_utf8ToUtf16Alloc( x_itemGetCPtr( item ), x_itemGetCLen( item ), &count );
}

static X_ITEM *s_gt_glfw_PutWide( X_ITEM *item, const wchar_t *text, size_t length )
{
   size_t count;
   char  *utf8 = x_utf16ToUtf8Alloc( ( const uint16_t * )text, length, &count );

   return x_itemPutCLPtr( item, utf8, count );
}

static const char *s_gt_glfw_SystemTheme( void )
{
   const wchar_t *key = L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize";
   DWORD          value, size = sizeof( value );
   LONG           status;

   status = RegGetValueW( HKEY_CURRENT_USER, key, L"AppsUseLightTheme", RRF_RT_REG_DWORD, NULL, &value, &size );
   if( status != ERROR_SUCCESS )
      return "UNKNOWN";

   return IIF( value, "LIGHT", "DARK" );
}

static int s_gt_glfw_OpenUrl( X_ITEM *item )
{
   wchar_t *url;
   int      opened;

   if( !item || !X_IS_STRING( item ) || x_itemGetCLen( item ) != strlen( x_itemGetCPtr( item ) ) )
      return 0;

   url = s_gt_glfw_ItemToWide( item );
   opened = ( INT_PTR )ShellExecuteW( NULL, L"open", url, NULL, NULL, SW_SHOWNORMAL ) > 32;
   x_xfree( url );

   return opened;
}

static int s_gt_glfw_Sound( X_ITEM *item )
{
   wchar_t *filename;
   int      played;

   if( !item || !X_IS_STRING( item ) || x_itemGetCLen( item ) != strlen( x_itemGetCPtr( item ) ) )
      return 0;

   filename = s_gt_glfw_ItemToWide( item );
   played = PlaySoundW( filename, NULL, SND_ASYNC | SND_FILENAME | SND_NODEFAULT ) != FALSE;
   x_xfree( filename );

   return played;
}

static int s_gt_glfw_MessageBox( X_GTGLFW *glfw, X_ITEM *text, X_ITEM *buttons )
{
   typedef HRESULT( WINAPI * X_TASKDIALOGINDIRECT )( const TASKDIALOGCONFIG *, int *, int *, BOOL * );
   X_ITEM              *parts[3] = { text, NULL, NULL };
   X_ITEM              *titleItem = NULL;
   wchar_t             *wideParts[3] = { NULL, NULL, NULL };
   size_t               lengths[3] = { 0, 0, 0 }, total = 0;
   wchar_t             *message, *target, *title = NULL;
   TASKDIALOG_BUTTON   *nativeButtons = NULL;
   wchar_t            **buttonText = NULL;
   TASKDIALOGCONFIG     config;
   HMODULE              commonControls;
   X_TASKDIALOGINDIRECT taskDialog = NULL;
   int                  count = 0, selected = 0, index;
   HRESULT              result;

   if( text && X_IS_ARRAY( text ) )
   {
      titleItem = x_arrayGetItemPtr( text, 1 );
      parts[0] = x_arrayGetItemPtr( text, 2 );
      parts[1] = x_arrayGetItemPtr( text, 3 );
      parts[2] = x_arrayGetItemPtr( text, 4 );
      if( !buttons )
         buttons = x_arrayGetItemPtr( text, 5 );
   }
   for( index = 0; index < 3; ++index )
   {
      wideParts[index] = s_gt_glfw_ItemToWide( parts[index] );
      lengths[index] = wcslen( wideParts[index] );
      total += lengths[index];
   }
   message = target = ( wchar_t * )x_xgrabArray( total + 5, sizeof( wchar_t ) );
   for( index = 0; index < 3; ++index )
   {
      if( lengths[index] )
      {
         if( target != message )
         {
            *target++ = L'\n';
            *target++ = L'\n';
         }
         memcpy( target, wideParts[index], lengths[index] * sizeof( wchar_t ) );
         target += lengths[index];
      }
      x_xfree( wideParts[index] );
   }
   *target = L'\0';
   title = IIF( titleItem, s_gt_glfw_ItemToWide( titleItem ), s_gt_glfw_ItemToWide( NULL ) );
   if( !*title )
   {
      x_xfree( title );
      title = ( wchar_t * )x_utf8ToUtf16Alloc( glfw->title, strlen( glfw->title ), &total );
   }
   count = IIF( buttons && X_IS_ARRAY( buttons ), ( int )x_arrayLen( buttons ), 0 );
   if( count > 0 )
   {
      nativeButtons = ( TASKDIALOG_BUTTON * )x_xgrabArray( count, sizeof( TASKDIALOG_BUTTON ) );
      buttonText = ( wchar_t ** )x_xgrabArray( count, sizeof( wchar_t * ) );
      for( index = 0; index < count; ++index )
      {
         buttonText[index] = s_gt_glfw_ItemToWide( x_arrayGetItemPtr( buttons, index + 1 ) );
         nativeButtons[index].nButtonID = index + 1;
         nativeButtons[index].pszButtonText = buttonText[index];
      }
   }
   memset( &config, 0, sizeof( config ) );
   config.cbSize = sizeof( config );
   config.hwndParent = glfwGetWin32Window( glfw->window );
   config.dwFlags = TDF_ALLOW_DIALOG_CANCELLATION | TDF_SIZE_TO_CONTENT;
   config.pszWindowTitle = title;
   config.pszContent = message;
   config.pszMainIcon = TD_INFORMATION_ICON;
   if( count )
   {
      config.cButtons = ( UINT )count;
      config.pButtons = nativeButtons;
      config.nDefaultButton = 1;
   }
   else
      config.dwCommonButtons = TDCBF_OK_BUTTON;
   /* TaskDialogIndirect exists only in Common Controls 6. A direct import makes
      the whole program fail at startup when that assembly is not activated by
      its manifest, even when the application never opens a dialog. */
   commonControls = LoadLibraryW( L"comctl32.dll" );
   if( commonControls )
      taskDialog = ( X_TASKDIALOGINDIRECT )( void * )GetProcAddress( commonControls, "TaskDialogIndirect" );
   result = IIF( taskDialog, taskDialog( &config, &selected, NULL, NULL ), E_NOTIMPL );
   if( commonControls )
      FreeLibrary( commonControls );
   if( SUCCEEDED( result ) && !count && selected == IDOK )
      selected = 1;
   if( FAILED( result ) )
      selected = IIF( MessageBoxW( config.hwndParent, message, title, MB_OK | MB_ICONINFORMATION ) == IDOK, 1, 0 );
   if( buttonText )
   {
      for( index = 0; index < count; ++index )
         x_xfree( buttonText[index] );
   }
   x_xfree( buttonText );
   x_xfree( nativeButtons );
   x_xfree( title );
   x_xfree( message );

   return IIF( selected >= 1 && selected <= count, selected, IIF( count, 0, selected ) );
}

static X_ITEM *s_gt_glfw_FileDialog( X_GTGLFW *glfw, X_ITEM *options, X_ITEM *result )
{
   int                isArray = options && X_IS_ARRAY( options );
   X_ITEM            *typeItem = IIF( isArray, x_arrayGetItemPtr( options, 1 ), NULL );
   X_ITEM            *titleItem = IIF( isArray, x_arrayGetItemPtr( options, 2 ), NULL );
   X_ITEM            *locationItem = IIF( isArray, x_arrayGetItemPtr( options, 3 ), NULL );
   X_ITEM            *filtersItem = IIF( isArray, x_arrayGetItemPtr( options, 4 ), NULL );
   X_ITEM            *manyItem = IIF( isArray, x_arrayGetItemPtr( options, 5 ), NULL );
   const char        *type = x_itemGetCPtr( typeItem );
   int                save = x_stricmp( type, "SAVE" ) == 0;
   int                folder = x_stricmp( type, "FOLDER" ) == 0;
   const CLSID       *dialogClass = IIF( save, &CLSID_FileSaveDialog, &CLSID_FileOpenDialog );
   const IID         *dialogInterface = IIF( save, &IID_IFileSaveDialog, &IID_IFileOpenDialog );
   HRESULT            init = CoInitializeEx( NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE );
   IFileDialog       *dialog = NULL;
   COMDLG_FILTERSPEC *specs = NULL;
   X_ITEM            *files = x_itemArrayNew( 0 );
   X_ITEM             value = X_ITEM_NIL_INIT;
   int                count = 0, filter = 0, index;
   HRESULT            hr;
   int                failed;
   int                invalid = 0;
   char               errorText[128] = "";

   if( options && !X_IS_NIL( options ) && !isArray )
      invalid = 1;
   if( isArray && x_arrayLen( options ) > 5 )
      invalid = 1;
   if( *type && !save && !folder && x_stricmp( type, "OPEN" ) != 0 )
      invalid = 1;
   if( invalid )
   {
      const char *usage = "File dialog options must be { OPEN|SAVE|FOLDER, title, location, filters, multiple }.";

      result = s_gt_glfw_FileDialogResult( result, 0, files, 0, usage );
      x_itemRelease( files );
      if( SUCCEEDED( init ) )
         CoUninitialize();
      return result;
   }
   hr = CoCreateInstance( dialogClass, NULL, CLSCTX_INPROC_SERVER, dialogInterface, ( void ** )&dialog );
   if( SUCCEEDED( hr ) )
   {
      FILEOPENDIALOGOPTIONS flags;

      dialog->lpVtbl->GetOptions( dialog, &flags );
      flags |= FOS_FORCEFILESYSTEM;
      if( folder )
         flags |= FOS_PICKFOLDERS;
      else if( !save && x_itemGetL( manyItem ) )
         flags |= FOS_ALLOWMULTISELECT;
      dialog->lpVtbl->SetOptions( dialog, flags );
      if( titleItem && X_IS_STRING( titleItem ) )
      {
         wchar_t *title = s_gt_glfw_ItemToWide( titleItem );

         dialog->lpVtbl->SetTitle( dialog, title );
         x_xfree( title );
      }
      if( locationItem && X_IS_STRING( locationItem ) )
      {
         wchar_t    *path = s_gt_glfw_ItemToWide( locationItem );
         DWORD       attributes = GetFileAttributesW( path );
         IShellItem *shellItem = NULL;

         if( attributes != INVALID_FILE_ATTRIBUTES && ( attributes & FILE_ATTRIBUTE_DIRECTORY ) != 0 )
            SHCreateItemFromParsingName( path, NULL, &IID_IShellItem, ( void ** )&shellItem );
         if( shellItem )
         {
            dialog->lpVtbl->SetFolder( dialog, shellItem );
            shellItem->lpVtbl->Release( shellItem );
         }
         else if( *path )
            dialog->lpVtbl->SetFileName( dialog, path );
         x_xfree( path );
      }
      if( !folder && filtersItem && X_IS_ARRAY( filtersItem ) )
         count = ( int )x_arrayLen( filtersItem );
      if( count > 0 )
      {
         specs = ( COMDLG_FILTERSPEC * )x_xgrabzArray( count, sizeof( COMDLG_FILTERSPEC ) );
         for( index = 0; index < count; ++index )
         {
            X_ITEM        *entry = x_arrayGetItemPtr( filtersItem, index + 1 );
            wchar_t       *pattern = s_gt_glfw_ItemToWide( x_arrayGetItemPtr( entry, 2 ) );
            wchar_t       *spec = ( wchar_t * )x_xgrabArray( wcslen( pattern ) * 3 + 4, sizeof( wchar_t ) );
            const wchar_t *part = pattern;
            size_t         length = 0;

            while( *part )
            {
               const wchar_t *end = wcschr( part, L';' );
               size_t         size = IIF( end, ( size_t )( end - part ), wcslen( part ) );

               if( length )
                  spec[length++] = L';';
               spec[length++] = L'*';
               spec[length++] = L'.';
               if( size == 1 && *part == L'*' )
                  spec[length++] = L'*';
               else
               {
                  memcpy( spec + length, part, size * sizeof( wchar_t ) );
                  length += size;
               }
               part += size + IIF( *( part + size ), 1, 0 );
            }
            spec[length] = L'\0';
            x_xfree( pattern );
            specs[index].pszName = s_gt_glfw_ItemToWide( x_arrayGetItemPtr( entry, 1 ) );
            specs[index].pszSpec = spec;
         }
         dialog->lpVtbl->SetFileTypes( dialog, ( UINT )count, specs );
         dialog->lpVtbl->SetFileTypeIndex( dialog, 1 );
      }
      hr = dialog->lpVtbl->Show( dialog, glfwGetWin32Window( glfw->window ) );
      if( SUCCEEDED( hr ) )
      {
         IShellItemArray *items = NULL;
         IShellItem      *item = NULL;
         DWORD            total = 1, position;

         if( save )
            hr = dialog->lpVtbl->GetResult( dialog, &item );
         else
         {
            hr = ( ( IFileOpenDialog * )dialog )->lpVtbl->GetResults( ( IFileOpenDialog * )dialog, &items );
            if( SUCCEEDED( hr ) )
               items->lpVtbl->GetCount( items, &total );
         }
         for( position = 0; SUCCEEDED( hr ) && position < total; ++position )
         {
            wchar_t *path = NULL;

            if( items )
               hr = items->lpVtbl->GetItemAt( items, position, &item );
            if( SUCCEEDED( hr ) )
               hr = item->lpVtbl->GetDisplayName( item, SIGDN_FILESYSPATH, &path );
            if( SUCCEEDED( hr ) )
            {
               x_arrayAdd( files, s_gt_glfw_PutWide( &value, path, wcslen( path ) ) );
               CoTaskMemFree( path );
            }
            if( item )
            {
               item->lpVtbl->Release( item );
               item = NULL;
            }
         }
         if( items )
            items->lpVtbl->Release( items );
      }
      if( count )
      {
         UINT selected = 1;

         dialog->lpVtbl->GetFileTypeIndex( dialog, &selected );
         filter = ( int )selected;
      }
      dialog->lpVtbl->Release( dialog );
   }
   for( index = 0; index < count; ++index )
   {
      x_xfree( ( void * )specs[index].pszName );
      x_xfree( ( void * )specs[index].pszSpec );
   }
   x_xfree( specs );
   if( SUCCEEDED( init ) )
      CoUninitialize();
   failed = FAILED( hr ) && hr != HRESULT_FROM_WIN32( ERROR_CANCELLED );
   if( failed )
      snprintf( errorText, sizeof( errorText ), "Windows file dialog failed (0x%08lX).", ( unsigned long )hr );
   result = s_gt_glfw_FileDialogResult( result, !failed && x_arrayLen( files ) > 0, files, filter, errorText );
   x_itemClear( &value );
   x_itemRelease( files );

   return result;
}

static int s_gt_glfw_SetIconResource( X_GTGLFW *glfw, X_ITEM *value )
{
   HINSTANCE instance = GetModuleHandleW( NULL );
   HICON     icon = NULL;

   if( value && X_IS_NUMERIC( value ) )
      icon = LoadIconW( instance, MAKEINTRESOURCEW( x_itemGetNI( value ) ) );
   else if( value && X_IS_STRING( value ) )
   {
      wchar_t *name = s_gt_glfw_ItemToWide( value );

      icon = LoadIconW( instance, name );
      x_xfree( name );
   }
   if( !icon )
      return 0;

   SendMessageW( glfwGetWin32Window( glfw->window ), WM_SETICON, ICON_SMALL, ( LPARAM )icon );
   SendMessageW( glfwGetWin32Window( glfw->window ), WM_SETICON, ICON_BIG, ( LPARAM )icon );
   if( glfw->iconOwned && glfw->iconHandle )
      DestroyIcon( ( HICON )glfw->iconHandle );
   glfw->iconHandle = icon;
   glfw->iconOwned = 0;

   return 1;
}

#elif X_OS_LINUX

extern char **environ;

static char *s_gt_glfw_FindProgram( const char *name )
{
   const char *path = getenv( "PATH" );
   const char *position;

   if( strchr( name, '/' ) )
      return IIF( access( name, X_OK ) == 0, x_strdup( name ), NULL );

   if( !path )
      return NULL;

   position = path;
   while( *position )
   {
      const char *end = strchr( position, ':' );
      size_t      length = IIF( end, ( size_t )( end - position ), strlen( position ) );
      char       *candidate = ( char * )x_xgrab( length + strlen( name ) + 2 );

      memcpy( candidate, position, length );
      candidate[length] = '/';
      strcpy( candidate + length + 1, name );
      if( access( candidate, X_OK ) == 0 )
         return candidate;

      x_xfree( candidate );
      if( !end )
         break;
      position = end + 1;
   }

   return NULL;
}

static char *s_gt_glfw_Option( const char *name, const char *value )
{
   size_t first = strlen( name );
   size_t second = strlen( value );
   char  *result = ( char * )x_xgrab( first + second + 1 );

   memcpy( result, name, first );
   memcpy( result + first, value, second + 1 );

   return result;
}

static int s_gt_glfw_RunCapture( char *const arguments[], char **output )
{
   int    descriptors[2];
   pid_t  child;
   char  *buffer = NULL;
   size_t length = 0, capacity = 0;
   int    status = -1;

   *output = NULL;
   if( pipe( descriptors ) != 0 )
      return -1;

   child = fork();
   if( child == 0 )
   {
      int nullHandle = open( "/dev/null", O_RDWR );

      close( descriptors[0] );
      dup2( descriptors[1], STDOUT_FILENO );
      close( descriptors[1] );
      if( nullHandle >= 0 )
      {
         dup2( nullHandle, STDIN_FILENO );
         dup2( nullHandle, STDERR_FILENO );
         if( nullHandle > STDERR_FILENO )
            close( nullHandle );
      }
      execvp( arguments[0], arguments );
      _exit( 127 );
   }
   close( descriptors[1] );
   if( child < 0 )
   {
      close( descriptors[0] );
      return -1;
   }
   for( ;; )
   {
      ssize_t count;

      if( capacity - length < 1024 )
      {
         capacity = IIF( capacity, capacity * 2, 4096 );
         buffer = ( char * )x_xrealloc( buffer, capacity );
      }
      count = read( descriptors[0], buffer + length, capacity - length - 1 );
      if( count > 0 )
         length += ( size_t )count;
      else if( count == 0 )
         break;
      else if( errno != EINTR )
         break;
   }
   close( descriptors[0] );
   while( waitpid( child, &status, 0 ) < 0 && errno == EINTR )
      ;
   if( !buffer )
      buffer = ( char * )x_xgrab( 1 );
   buffer[length] = '\0';
   *output = buffer;

   return IIF( WIFEXITED( status ), WEXITSTATUS( status ), -1 );
}

static int s_gt_glfw_Launch( const char *program, const char *argument )
{
   pid_t child = fork();

   if( child < 0 )
      return 0;

   if( child == 0 )
   {
      pid_t grandchild = fork();

      if( grandchild == 0 )
      {
         int nullHandle = open( "/dev/null", O_RDWR );

         if( nullHandle >= 0 )
         {
            dup2( nullHandle, STDIN_FILENO );
            dup2( nullHandle, STDOUT_FILENO );
            dup2( nullHandle, STDERR_FILENO );
            if( nullHandle > STDERR_FILENO )
               close( nullHandle );
         }
         execlp( program, program, argument, ( char * )NULL );
         _exit( 127 );
      }
      _exit( IIF( grandchild < 0, 127, 0 ) );
   }
   while( waitpid( child, NULL, 0 ) < 0 && errno == EINTR )
      ;

   return 1;
}

static const char *s_gt_glfw_SystemTheme( void )
{
   const char *query = "gdbus call --session --dest org.freedesktop.portal.Desktop "
                       "--object-path /org/freedesktop/portal/desktop --method "
                       "org.freedesktop.portal.Settings.ReadOne --timeout 1 "
                       "org.freedesktop.appearance color-scheme 2>/dev/null";
   char        output[256];
   FILE       *command = popen( query, "r" );

   if( command )
   {
      size_t length = fread( output, 1, sizeof( output ) - 1, command );

      output[length] = '\0';
      pclose( command );
      if( strstr( output, "uint32 1" ) )
         return "DARK";

      if( strstr( output, "uint32 2" ) )
         return "LIGHT";
   }

   return "UNKNOWN";
}

static int s_gt_glfw_OpenUrl( X_ITEM *item )
{
   char *program;
   int   opened;

   if( !item || !X_IS_STRING( item ) || x_itemGetCLen( item ) != strlen( x_itemGetCPtr( item ) ) )
      return 0;

   program = s_gt_glfw_FindProgram( "xdg-open" );
   if( !program )
      return 0;

   opened = s_gt_glfw_Launch( program, x_itemGetCPtr( item ) );
   x_xfree( program );

   return opened;
}

static int s_gt_glfw_Sound( X_ITEM *item )
{
   const char *configured = getenv( "XDEV_SOUND_PLAYER" );
   const char *players[] = { "paplay", "pw-play", "aplay" };
   char       *program = NULL;
   int         played;
   size_t      index;

   if( !item || !X_IS_STRING( item ) || x_itemGetCLen( item ) != strlen( x_itemGetCPtr( item ) ) )
      return 0;

   if( configured && *configured )
      program = s_gt_glfw_FindProgram( configured );
   for( index = 0; !program && index < sizeof( players ) / sizeof( players[0] ); ++index )
      program = s_gt_glfw_FindProgram( players[index] );
   if( !program )
      return 0;

   played = s_gt_glfw_Launch( program, x_itemGetCPtr( item ) );
   x_xfree( program );

   return played;
}

static int s_gt_glfw_MessageBox( X_GTGLFW *glfw, X_ITEM *text, X_ITEM *buttons )
{
   X_ITEM     *titleItem = NULL;
   X_ITEM     *parts[3] = { text, NULL, NULL };
   const char *title = glfw->title;
   char      **arguments;
   char      **allocated;
   char       *message, *target, *output = NULL;
   size_t      total = 1;
   int         buttonCount, argumentCount = 0, allocatedCount = 0, index, status, result = 0;

   if( text && X_IS_ARRAY( text ) )
   {
      titleItem = x_arrayGetItemPtr( text, 1 );
      parts[0] = x_arrayGetItemPtr( text, 2 );
      parts[1] = x_arrayGetItemPtr( text, 3 );
      parts[2] = x_arrayGetItemPtr( text, 4 );
      if( !buttons )
         buttons = x_arrayGetItemPtr( text, 5 );
   }
   for( index = 0; index < 3; ++index )
      total += x_itemGetCLen( parts[index] ) + 2;
   message = target = ( char * )x_xgrab( total );
   for( index = 0; index < 3; ++index )
   {
      size_t length = x_itemGetCLen( parts[index] );

      if( length )
      {
         if( target != message )
         {
            *target++ = '\n';
            *target++ = '\n';
         }
         memcpy( target, x_itemGetCPtr( parts[index] ), length );
         target += length;
      }
   }
   *target = '\0';
   buttonCount = IIF( buttons && X_IS_ARRAY( buttons ), ( int )x_arrayLen( buttons ), 0 );
   arguments = ( char ** )x_xgrabzArray( ( size_t )buttonCount + 10, sizeof( char * ) );
   allocated = ( char ** )x_xgrabzArray( ( size_t )buttonCount + 4, sizeof( char * ) );
   arguments[argumentCount++] = ( char * )"zenity";
   arguments[argumentCount++] = ( char * )"--question";
   arguments[argumentCount++] = ( char * )"--modal";
   if( titleItem && X_IS_STRING( titleItem ) )
      title = x_itemGetCPtr( titleItem );
   arguments[argumentCount++] = allocated[allocatedCount++] = s_gt_glfw_Option( "--title=", title );
   arguments[argumentCount++] = allocated[allocatedCount++] = s_gt_glfw_Option( "--text=", message );
   for( index = 0; index < buttonCount; ++index )
   {
      const char *label = x_itemGetCPtr( x_arrayGetItemPtr( buttons, index + 1 ) );
      const char *name = "--extra-button=";

      if( index == 0 )
         name = "--ok-label=";
      else if( index == 1 )
         name = "--cancel-label=";
      arguments[argumentCount++] = allocated[allocatedCount++] = s_gt_glfw_Option( name, label );
   }
   arguments[argumentCount] = NULL;
   status = s_gt_glfw_RunCapture( arguments, &output );
   if( output )
   {
      size_t length = strlen( output );

      while( length && ( output[length - 1] == '\r' || output[length - 1] == '\n' ) )
         output[--length] = '\0';
      for( index = 2; index < buttonCount; ++index )
         if( strcmp( output, x_itemGetCPtr( x_arrayGetItemPtr( buttons, index + 1 ) ) ) == 0 )
            result = index + 1;
   }
   if( result == 0 && status == 0 )
      result = 1;
   else if( result == 0 && status == 1 && buttonCount > 1 )
      result = 2;
   for( index = 0; index < allocatedCount; ++index )
      x_xfree( allocated[index] );
   x_xfree( output );
   x_xfree( allocated );
   x_xfree( arguments );
   x_xfree( message );

   return result;
}

static X_ITEM *s_gt_glfw_FileDialog( X_GTGLFW *glfw, X_ITEM *options, X_ITEM *result )
{
   X_ITEM     *values[5] = { NULL, NULL, NULL, NULL, NULL };
   X_ITEM     *files = x_itemArrayNew( 0 );
   char      **arguments = NULL, **allocated = NULL;
   char       *output = NULL;
   char       *option;
   const char *failure = "";
   int         mode = 0, many = 0, filterCount = 0;
   int         argumentCount = 0, allocatedCount = 0, index, status = -1;

   UNUSED( glfw );
   if( options && !X_IS_NIL( options ) )
   {
      if( !X_IS_ARRAY( options ) || x_arrayLen( options ) > 5 )
      {
         failure = "File dialog options must be { type, title, location, filters, multiple }.";
         goto linuxFileDone;
      }
      for( index = 0; index < 5; ++index )
         values[index] = x_arrayGetItemPtr( options, index + 1 );
   }
   if( values[0] && x_itemGetCLen( values[0] ) )
   {
      if( x_stricmp( x_itemGetCPtr( values[0] ), "SAVE" ) == 0 )
         mode = 1;
      else if( x_stricmp( x_itemGetCPtr( values[0] ), "FOLDER" ) == 0 )
         mode = 2;
      else if( x_stricmp( x_itemGetCPtr( values[0] ), "OPEN" ) != 0 )
      {
         failure = "File dialog type must be OPEN, SAVE or FOLDER.";
         goto linuxFileDone;
      }
   }
   many = mode != 1 && values[4] && X_IS_LOGICAL( values[4] ) && x_itemGetL( values[4] );
   filterCount = IIF( mode != 2 && values[3] && X_IS_ARRAY( values[3] ), ( int )x_arrayLen( values[3] ), 0 );
   arguments = ( char ** )x_xgrabzArray( ( size_t )filterCount + 12, sizeof( char * ) );
   allocated = ( char ** )x_xgrabzArray( ( size_t )filterCount + 4, sizeof( char * ) );
   arguments[argumentCount++] = ( char * )"zenity";
   arguments[argumentCount++] = ( char * )"--file-selection";
   arguments[argumentCount++] = ( char * )"--modal";
   arguments[argumentCount++] = ( char * )"--separator=\n";
   if( mode == 1 )
      arguments[argumentCount++] = ( char * )"--save";
   else if( mode == 2 )
      arguments[argumentCount++] = ( char * )"--directory";
   if( many )
      arguments[argumentCount++] = ( char * )"--multiple";
   if( values[1] && X_IS_STRING( values[1] ) )
   {
      option = s_gt_glfw_Option( "--title=", x_itemGetCPtr( values[1] ) );
      arguments[argumentCount++] = allocated[allocatedCount++] = option;
   }
   if( values[2] && X_IS_STRING( values[2] ) )
   {
      option = s_gt_glfw_Option( "--filename=", x_itemGetCPtr( values[2] ) );
      arguments[argumentCount++] = allocated[allocatedCount++] = option;
   }
   for( index = 0; index < filterCount; ++index )
   {
      X_ITEM     *entry = x_arrayGetItemPtr( values[3], index + 1 );
      const char *label;
      const char *patterns;
      char       *filter, *target;
      size_t      capacity;

      if( !entry || !X_IS_ARRAY( entry ) || x_arrayLen( entry ) < 2 )
      {
         failure = "Each file filter must be { label, extensions }.";
         goto linuxFileDone;
      }
      label = x_itemGetCPtr( x_arrayGetItemPtr( entry, 1 ) );
      patterns = x_itemGetCPtr( x_arrayGetItemPtr( entry, 2 ) );
      capacity = strlen( label ) + strlen( patterns ) * 3 + 32;
      filter = target = ( char * )x_xgrab( capacity );
      target += snprintf( target, capacity, "--file-filter=%s | ", label );
      while( *patterns )
      {
         const char *end = strchr( patterns, ';' );
         size_t      length = IIF( end, ( size_t )( end - patterns ), strlen( patterns ) );

         *target++ = '*';
         *target++ = '.';
         memcpy( target, patterns, length );
         target += length;
         *target++ = ' ';
         patterns += length + IIF( end, 1, 0 );
      }
      *target = '\0';
      arguments[argumentCount++] = allocated[allocatedCount++] = filter;
   }
   arguments[argumentCount] = NULL;
   status = s_gt_glfw_RunCapture( arguments, &output );
   if( status == 0 && output )
   {
      char  *position = output;
      size_t count = 0;

      while( *position )
      {
         char *end = strchr( position, '\n' );

         if( end )
            *end = '\0';
         if( *position )
            ++count;
         position = IIF( end, end + 1, position + strlen( position ) );
      }
      x_arrayNew( files, count );
      position = output;
      count = 0;
      while( *position )
      {
         size_t length = strlen( position );

         if( length )
            x_arraySetCL( files, ++count, position, length );
         position += length + 1;
      }
   }
   else if( status == 127 )
      failure = "zenity was not found in PATH. Install zenity to use file dialogs.";
   else if( status != 1 && status != 5 )
      failure = "zenity could not open the file dialog.";

linuxFileDone:
   result = s_gt_glfw_FileDialogResult( result, status == 0 && x_arrayLen( files ) > 0, files, 0, failure );
   if( allocated )
      for( index = 0; index < allocatedCount; ++index )
         x_xfree( allocated[index] );
   x_xfree( output );
   x_xfree( allocated );
   x_xfree( arguments );
   x_itemRelease( files );

   return result;
}

#else

static const char *s_gt_glfw_SystemTheme( void )
{
   return "UNKNOWN";
}

static int s_gt_glfw_OpenUrl( X_ITEM *item )
{
   UNUSED( item );

   return 0;
}

static int s_gt_glfw_Sound( X_ITEM *item )
{
   UNUSED( item );

   return 0;
}

static int s_gt_glfw_MessageBox( X_GTGLFW *glfw, X_ITEM *text, X_ITEM *buttons )
{
   UNUSED( glfw );
   UNUSED( text );
   UNUSED( buttons );

   return 0;
}

static X_ITEM *s_gt_glfw_FileDialog( X_GTGLFW *glfw, X_ITEM *options, X_ITEM *result )
{
   X_ITEM *files = x_itemArrayNew( 0 );
   UNUSED( glfw );
   UNUSED( options );
   result = s_gt_glfw_FileDialogResult( result, 0, files, 0, "File dialogs are unavailable." );
   x_itemRelease( files );

   return result;
}

#endif

static int s_gt_glfw_Info( X_GT_BASE *pGT, int type, X_GT_INFO *info )
{
   X_GTGLFW *glfw = X_GTGLFW_GET( pGT );

   s_gt_glfw_PumpEvents();

   switch( type )
   {
      case X_GTI_ISGRAPHIC:
      case X_GTI_ISSCREENPOS:
      case X_GTI_KBDSUPPORT:
      case X_GTI_ISUNICODE: info->pResult = x_itemPutL( info->pResult, 1 ); return 1;

      case X_GTI_ISCTWIN:
      case X_GTI_ISMULTIWIN: info->pResult = x_itemPutL( info->pResult, 0 ); return 1;

      case X_GTI_ONLINE: info->pResult = x_itemPutL( info->pResult, glfw->window != NULL ); return 1;

      case X_GTI_VERSION:
         info->pResult = x_itemPutC( info->pResult, s_gt_glfw_Version( pGT, x_itemGetNI( info->pNewVal ) ) );
         return 1;

      case X_GTI_SCREENWIDTH:
      {
         int64_t value;

         info->pResult = x_itemPutNI( info->pResult, glfw->canvasWidth );
         if( s_gt_glfw_InfoInteger( info->pNewVal, 1, INT_MAX, &value ) )
            s_gt_glfw_SetScreenSize( glfw, ( int )value, glfw->canvasHeight );
         return 1;
      }

      case X_GTI_SCREENHEIGHT:
      {
         int64_t value;

         info->pResult = x_itemPutNI( info->pResult, glfw->canvasHeight );
         if( s_gt_glfw_InfoInteger( info->pNewVal, 1, INT_MAX, &value ) )
            s_gt_glfw_SetScreenSize( glfw, glfw->canvasWidth, ( int )value );
         return 1;
      }

      case X_GTI_SCREENSIZE:
      {
         int width, height;

         s_gt_glfw_InfoPairResult( info, glfw->canvasWidth, glfw->canvasHeight );
         if( s_gt_glfw_InfoPairInput( info, &width, &height ) )
            s_gt_glfw_SetScreenSize( glfw, width, height );
         return 1;
      }

      case X_GTI_WINDOWWIDTH:
         s_gt_glfw_UpdateMetrics( glfw );
         info->pResult = x_itemPutNI( info->pResult, glfw->windowWidth );
         return 1;

      case X_GTI_WINDOWHEIGHT:
         s_gt_glfw_UpdateMetrics( glfw );
         info->pResult = x_itemPutNI( info->pResult, glfw->windowHeight );
         return 1;

      case X_GTI_OUTPUTWIDTH:
         s_gt_glfw_UpdateMetrics( glfw );
         info->pResult = x_itemPutNI( info->pResult, glfw->framebufferWidth );
         return 1;

      case X_GTI_OUTPUTHEIGHT:
         s_gt_glfw_UpdateMetrics( glfw );
         info->pResult = x_itemPutNI( info->pResult, glfw->framebufferHeight );
         return 1;

      case X_GTI_LOGICALWIDTH: info->pResult = x_itemPutNI( info->pResult, glfw->canvasWidth ); return 1;

      case X_GTI_LOGICALHEIGHT: info->pResult = x_itemPutNI( info->pResult, glfw->canvasHeight ); return 1;

      case X_GTI_PIXELDENSITY:
      {
         double density;

         s_gt_glfw_UpdateMetrics( glfw );
         density = IIF( glfw->windowWidth > 0, glfw->framebufferWidth / ( double )glfw->windowWidth, 1.0 );
         info->pResult = x_itemPutND( info->pResult, density );
         return 1;
      }

      case X_GTI_DISPLAYSCALE:
         s_gt_glfw_UpdateMetrics( glfw );
         info->pResult = x_itemPutND( info->pResult, glfw->contentScaleX );
         return 1;

      case X_GTI_CONTENTSCALE:
      {
         double density;

         s_gt_glfw_UpdateMetrics( glfw );
         density = IIF( glfw->windowWidth > 0, glfw->framebufferWidth / ( double )glfw->windowWidth, 1.0 );
         info->pResult = x_itemPutND( info->pResult, IIF( density > 0.0, glfw->contentScaleX / density, 1.0 ) );
         return 1;
      }

      case X_GTI_SCREENDEPTH: info->pResult = x_itemPutNI( info->pResult, 32 ); return 1;

      case X_GTI_RENDERERNAME: info->pResult = s_gt_glfw_RendererName( info->pResult, glfw ); return 1;

      case X_GTI_VIEWMAXHEIGHT:
      case X_GTI_VIEWPORTHEIGHT: info->pResult = x_itemPutNI( info->pResult, glfw->rows ); return 1;

      case X_GTI_VIEWMAXWIDTH:
      case X_GTI_VIEWPORTWIDTH: info->pResult = x_itemPutNI( info->pResult, glfw->cols ); return 1;

      case X_GTI_FONTSIZE: info->pResult = x_itemPutNI( info->pResult, X_GTGLFW_CELL_HEIGHT ); return 1;

      case X_GTI_FONTWIDTH: info->pResult = x_itemPutNI( info->pResult, X_GTGLFW_CELL_WIDTH ); return 1;

      case X_GTI_FONTWEIGHT: info->pResult = x_itemPutNI( info->pResult, X_GTI_FONTW_NORMAL ); return 1;

      case X_GTI_FONTQUALITY: info->pResult = x_itemPutNI( info->pResult, X_GTI_FONTQ_DRAFT ); return 1;

      case X_GTI_FONTNAME: info->pResult = x_itemPutC( info->pResult, "Fixed 9x18" ); return 1;

      case X_GTI_FONTSEL: info->pResult = x_itemPutC( info->pResult, "" ); return 1;

      case X_GTI_FONTATTRIBUTE:
         info->pResult = x_itemPutNI( info->pResult, X_GTI_FONTA_FIXMETRIC | X_GTI_FONTA_CLRBKG );
         return 1;

      case X_GTI_WINTITLE:
         info->pResult = x_itemPutC( info->pResult, glfw->title );
         if( info->pNewVal )
         {
            const char *title;
            size_t      length;
            char       *copy;

            if( !X_IS_STRING( info->pNewVal ) )
            {
               x_errorRaiseNamed( info->pResult, 1102, "GtInfo(WINTITLE)" );
               return 0;
            }
            title = x_itemGetCPtr( info->pNewVal );
            length = x_itemGetCLen( info->pNewVal );
            if( memchr( title, '\0', length ) != NULL )
            {
               x_errorRaiseNamed( info->pResult, 1102, "GtInfo(WINTITLE): embedded zero" );
               return 0;
            }
            copy = ( char * )x_xgrab( length + 1 );
            memcpy( copy, title, length );
            copy[length] = '\0';
            x_xfree( glfw->title );
            glfw->title = copy;
            glfwSetWindowTitle( glfw->window, glfw->title );
         }
         return 1;

      case X_GTI_SETPOS_XY:
      case X_GTI_SETPOS_ROWCOL:
      {
         int first, second;

         glfwGetWindowPos( glfw->window, &glfw->windowX, &glfw->windowY );
         if( type == X_GTI_SETPOS_ROWCOL )
            s_gt_glfw_InfoPairResult( info, glfw->windowY / X_GTGLFW_CELL_HEIGHT, glfw->windowX / X_GTGLFW_CELL_WIDTH );
         else
            s_gt_glfw_InfoPairResult( info, glfw->windowX, glfw->windowY );
         if( s_gt_glfw_InfoPairInput( info, &first, &second ) )
         {
            int x = first;
            int y = second;

            if( type == X_GTI_SETPOS_ROWCOL )
            {
               int64_t windowX = ( int64_t )second * X_GTGLFW_CELL_WIDTH;
               int64_t windowY = ( int64_t )first * X_GTGLFW_CELL_HEIGHT;

               if( windowX < INT_MIN || windowX > INT_MAX || windowY < INT_MIN || windowY > INT_MAX )
                  return 1;

               x = ( int )windowX;
               y = ( int )windowY;
            }
            glfwSetWindowPos( glfw->window, x, y );
            glfwGetWindowPos( glfw->window, &glfw->windowX, &glfw->windowY );
         }
         return 1;
      }

      case X_GTI_WINHANDLE: info->pResult = s_gt_glfw_WindowHandle( info->pResult, glfw ); return 1;

      case X_GTI_RESIZABLE:
         info->pResult = x_itemPutL( info->pResult, glfw->resizable );
         if( info->pNewVal && X_IS_LOGICAL( info->pNewVal ) )
         {
            int resizable = x_itemGetL( info->pNewVal );

            if( resizable != glfw->resizable )
            {
               glfw->resizable = resizable;
               glfwSetWindowAttrib( glfw->window, GLFW_RESIZABLE, IIF( resizable, GLFW_TRUE, GLFW_FALSE ) );
            }
         }
         return 1;

      case X_GTI_RESIZEFONT:
         /* TODO: .T. (fixed grid, the font follows the window) like GTWVT and GTXWC */
         info->pResult = x_itemPutL( info->pResult, 0 );
         return 1;

      case X_GTI_RESIZESTEP:
         info->pResult = x_itemPutL( info->pResult, glfw->resizeStep );
         if( info->pNewVal && X_IS_LOGICAL( info->pNewVal ) )
         {
            int resizeStep = x_itemGetL( info->pNewVal );

            if( resizeStep != glfw->resizeStep )
            {
               glfw->resizeStep = resizeStep;
               if( resizeStep && !glfw->maximized && !glfw->fullscreen )
                  s_gt_glfw_SetWindowSizeForGrid( glfw );
            }
         }
         return 1;

      case X_GTI_CLOSABLE:
         info->pResult = x_itemPutL( info->pResult, glfw->closeMode == 0 );
         if( info->pNewVal && X_IS_LOGICAL( info->pNewVal ) )
         {
            int closable = x_itemGetL( info->pNewVal );

            if( IIF( closable, glfw->closeMode != 0, glfw->closeMode == 0 ) )
               glfw->closeMode = IIF( glfw->closeMode == 0, 1, 0 );
         }
         return 1;

      case X_GTI_CLOSEMODE:
      {
         int64_t value;

         info->pResult = x_itemPutNI( info->pResult, glfw->closeMode );
         if( s_gt_glfw_InfoInteger( info->pNewVal, 0, 2, &value ) )
            glfw->closeMode = ( int )value;
         return 1;
      }

      case X_GTI_ISFULLSCREEN:
         info->pResult = x_itemPutL( info->pResult, glfw->fullscreen );
         if( info->pNewVal && X_IS_LOGICAL( info->pNewVal ) )
            s_gt_glfw_SetFullscreen( glfw, x_itemGetL( info->pNewVal ) );
         return 1;

      case X_GTI_MAXIMIZED:
      {
         int maximized = glfwGetWindowAttrib( glfw->window, GLFW_MAXIMIZED ) == GLFW_TRUE;

         glfw->maximized = maximized;
         info->pResult = x_itemPutL( info->pResult, maximized );
         if( info->pNewVal && X_IS_LOGICAL( info->pNewVal ) && !glfw->fullscreen )
         {
            if( x_itemGetL( info->pNewVal ) )
               glfwMaximizeWindow( glfw->window );
            else if( maximized )
               glfwRestoreWindow( glfw->window );
         }
         return 1;
      }

      case X_GTI_MINIMIZED:
      {
         int minimized = glfwGetWindowAttrib( glfw->window, GLFW_ICONIFIED ) == GLFW_TRUE;

         glfw->minimized = minimized;
         info->pResult = x_itemPutL( info->pResult, minimized );
         if( info->pNewVal && X_IS_LOGICAL( info->pNewVal ) )
         {
            if( x_itemGetL( info->pNewVal ) )
               glfwIconifyWindow( glfw->window );
            else if( minimized )
               glfwRestoreWindow( glfw->window );
         }
         return 1;
      }

      case X_GTI_ALTENTER:
         info->pResult = x_itemPutL( info->pResult, glfw->altEnter );
         if( info->pNewVal && X_IS_LOGICAL( info->pNewVal ) )
            glfw->altEnter = x_itemGetL( info->pNewVal );
         return 1;

      case X_GTI_CURSORBLINKRATE:
      {
         int64_t value;

         info->pResult = x_itemPutNI( info->pResult, glfw->cursorBlinkRate );
         if( s_gt_glfw_InfoInteger( info->pNewVal, 0, INT_MAX, &value ) )
         {
            glfw->cursorBlinkRate = ( int )value;
            glfw->cursorVisible = 1;
            glfw->cursorTime = glfwGetTime();
            glfw->presentPending = 1;
         }
         return 1;
      }

      case X_GTI_KBDSHIFTS: info->pResult = x_itemPutNI( info->pResult, s_gt_glfw_KbdShifts( glfw ) ); return 1;

      case X_GTI_MOUSESTATUS:
         info->pResult = x_itemPutL( info->pResult, glfw->mouseOn );
         if( info->pNewVal && X_IS_LOGICAL( info->pNewVal ) )
         {
            glfw->mouseOn = x_itemGetL( info->pNewVal );
            if( !glfw->mouseOn )
            {
               glfw->mouseButtons = 0;
               glfw->clickTime[0] = glfw->clickTime[1] = glfw->clickTime[2] = 0.0;
               glfw->wheelX = glfw->wheelY = 0.0;
               s_gt_glfw_ClearSelection( glfw, 1 );
            }
         }
         return 1;

      case X_GTI_MOUSEPOS_XY:
      {
         double cursorX, cursorY;
         int    x, y;

         s_gt_glfw_InfoPairResult( info, glfw->mouseX, glfw->mouseY );
         if( !s_gt_glfw_InfoPairInput( info, &x, &y ) )
            return 1;

         if( x >= 0 && x < glfw->canvasWidth && y >= 0 && y < glfw->canvasHeight )
         {
            cursorX = x * ( double )glfw->windowWidth / glfw->canvasWidth;
            cursorY = y * ( double )glfw->windowHeight / glfw->canvasHeight;
            glfwSetCursorPos( glfw->window, cursorX, cursorY );
            s_gt_glfw_UpdateMousePosition( glfw, cursorX, cursorY, 0 );
         }
         return 1;
      }

      case X_GTI_PALETTE:
      {
         int     index;
         int64_t value;
         int     changed = 0;

         if( info->pNewVal && X_IS_NUMERIC( info->pNewVal ) )
         {
            if( !s_gt_glfw_InfoInteger( info->pNewVal, 0, 15, &value ) )
               return 0;

            index = ( int )value;
            info->pResult = x_itemPutNI( info->pResult, s_gt_glfw_PaletteValue( glfw->colors[index] ) );
            if( s_gt_glfw_InfoInteger( info->pNewVal2, 0, 0xFFFFFF, &value ) )
               changed = s_gt_glfw_SetPaletteValue( glfw, index, value );
         }
         else
         {
            if( !info->pResult )
               info->pResult = x_itemNew( NULL );
            x_arrayNew( info->pResult, 16 );
            for( index = 0; index < 16; ++index )
               x_arraySetNI( info->pResult, index + 1, s_gt_glfw_PaletteValue( glfw->colors[index] ) );
            if( info->pNewVal )
            {
               int64_t values[16];

               if( !X_IS_ARRAY( info->pNewVal ) || x_arrayLen( info->pNewVal ) != 16 )
                  return 0;

               for( index = 0; index < 16; ++index )
               {
                  X_ITEM *item = x_arrayGetItemPtr( info->pNewVal, index + 1 );

                  if( !s_gt_glfw_InfoInteger( item, 0, 0xFFFFFF, &values[index] ) )
                     return 0;
               }

               for( index = 0; index < 16; ++index )
                  changed |= s_gt_glfw_SetPaletteValue( glfw, index, values[index] );
            }
         }
         if( changed )
         {
            X_GTSELF_EXPOSEAREA( pGT, 0, 0, glfw->rows - 1, glfw->cols - 1 );
            glfw->canvasDirty = 1;
            glfw->presentPending = 1;
         }
         return 1;
      }

      case X_GTI_SELECTCOPY:
         info->pResult = x_itemPutL( info->pResult, glfw->selectCopy );
         if( info->pNewVal && X_IS_LOGICAL( info->pNewVal ) )
         {
            glfw->selectCopy = x_itemGetL( info->pNewVal );
            if( !glfw->selectCopy )
               s_gt_glfw_ClearSelection( glfw, 1 );
         }
         return 1;

      case X_GTI_CLIPBOARDDATA:
         if( info->pNewVal )
         {
            const char *text;
            size_t      length;

            if( !X_IS_STRING( info->pNewVal ) )
            {
               x_errorRaiseNamed( info->pResult, 1102, "GtInfo(CLIPBOARDDATA)" );
               return 0;
            }
            text = x_itemGetCPtr( info->pNewVal );
            length = x_itemGetCLen( info->pNewVal );
            if( memchr( text, '\0', length ) != NULL )
            {
               x_errorRaiseNamed( info->pResult, 1102, "GtInfo(CLIPBOARDDATA): embedded zero" );
               return 0;
            }
            info->pResult = x_itemPutL( info->pResult, s_gt_glfw_SetClipboard( glfw, text, length ) );
         }
         else
         {
            const char *text = glfwGetClipboardString( glfw->window );

            info->pResult = x_itemPutC( info->pResult, IIF( text, text, "" ) );
         }
         return 1;

      case X_GTI_CLIPBOARDPASTE:
         info->pResult = x_itemPutL( info->pResult, s_gt_glfw_PasteClipboard( glfw ) );
         return 1;

      case X_GTI_DESKTOPWIDTH:
      case X_GTI_DESKTOPHEIGHT:
      case X_GTI_DESKTOPROWS:
      case X_GTI_DESKTOPCOLS:
      case X_GTI_DESKTOPDEPTH:
      {
         GLFWmonitor       *monitor = s_gt_glfw_CurrentMonitor( glfw );
         const GLFWvidmode *mode = IIF( monitor, glfwGetVideoMode( monitor ), NULL );
         int                value = 0;

         if( mode )
         {
            switch( type )
            {
               case X_GTI_DESKTOPWIDTH: value = mode->width; break;
               case X_GTI_DESKTOPHEIGHT: value = mode->height; break;
               case X_GTI_DESKTOPCOLS: value = mode->width / X_GTGLFW_CELL_WIDTH; break;
               case X_GTI_DESKTOPROWS: value = mode->height / X_GTGLFW_CELL_HEIGHT; break;
               default: value = mode->redBits + mode->greenBits + mode->blueBits; break;
            }
         }
         info->pResult = x_itemPutNI( info->pResult, value );
         return 1;
      }

      case X_GTI_WINDOWBORDERED:
      {
         int bordered = glfwGetWindowAttrib( glfw->window, GLFW_DECORATED ) == GLFW_TRUE;
         int enable;

         info->pResult = x_itemPutL( info->pResult, bordered );
         if( info->pNewVal && X_IS_LOGICAL( info->pNewVal ) && x_itemGetL( info->pNewVal ) != bordered )
         {
            enable = IIF( x_itemGetL( info->pNewVal ), GLFW_TRUE, GLFW_FALSE );
            glfwSetWindowAttrib( glfw->window, GLFW_DECORATED, enable );
         }
         return 1;
      }

      case X_GTI_WINDOWALWAYSONTOP:
      {
         int floating = glfwGetWindowAttrib( glfw->window, GLFW_FLOATING ) == GLFW_TRUE;
         int enable;

         info->pResult = x_itemPutL( info->pResult, floating );
         if( info->pNewVal && X_IS_LOGICAL( info->pNewVal ) && x_itemGetL( info->pNewVal ) != floating )
         {
            enable = IIF( x_itemGetL( info->pNewVal ), GLFW_TRUE, GLFW_FALSE );
            glfwSetWindowAttrib( glfw->window, GLFW_FLOATING, enable );
         }
         return 1;
      }

      case X_GTI_WINDOWOPACITY:
      {
         float opacity = glfwGetWindowOpacity( glfw->window );

         info->pResult = x_itemPutND( info->pResult, opacity );
         if( info->pNewVal && X_IS_NUMERIC( info->pNewVal ) )
         {
            double value = x_itemGetND( info->pNewVal );

            if( isfinite( value ) )
               glfwSetWindowOpacity( glfw->window, ( float )GTGLFW_MAX( 0.0, GTGLFW_MIN( value, 1.0 ) ) );
         }
         return 1;
      }

      case X_GTI_ICONFILE:
         info->pResult = x_itemPutL( info->pResult, s_gt_glfw_SetIconFile( glfw, info->pNewVal ) );
         return 1;

      case X_GTI_ICONRES:
#if X_OS_WINDOWS
         info->pResult = x_itemPutL( info->pResult, s_gt_glfw_SetIconResource( glfw, info->pNewVal ) );
#else
         info->pResult = x_itemPutL( info->pResult, 0 );
#endif
         return 1;

      case X_GTI_SYSTEMTHEME: info->pResult = x_itemPutC( info->pResult, s_gt_glfw_SystemTheme() ); return 1;

      case X_GTI_OPENURL: info->pResult = x_itemPutL( info->pResult, s_gt_glfw_OpenUrl( info->pNewVal ) ); return 1;

      case X_GTI_SOUND: info->pResult = x_itemPutL( info->pResult, s_gt_glfw_Sound( info->pNewVal ) ); return 1;

      case X_GTI_MSGBOX:
         if( !info->pNewVal || X_IS_LOGICAL( info->pNewVal ) || X_IS_NIL( info->pNewVal ) )
         {
            info->pResult = x_itemPutL( info->pResult, glfw->msgAlert );
            if( info->pNewVal && X_IS_LOGICAL( info->pNewVal ) )
               glfw->msgAlert = x_itemGetL( info->pNewVal );
         }
         else
            info->pResult = x_itemPutNI( info->pResult, s_gt_glfw_MessageBox( glfw, info->pNewVal, info->pNewVal2 ) );
         return 1;

      case X_GTI_FILEDIALOG: info->pResult = s_gt_glfw_FileDialog( glfw, info->pNewVal, info->pResult ); return 1;

      case X_GTI_DISPIMAGE:
         info->pResult = x_itemPutL( info->pResult, s_gt_glfw_DispImage( pGT, glfw, info->pNewVal, info->pNewVal2 ) );
         return 1;

      default: return s_super.Info( pGT, type, info );
   }
}

static int s_gt_glfw_Alert( X_GT_BASE *pGT, X_ITEM *message, X_ITEM *options, int normal, int high, double delay )
{
   X_GTGLFW *glfw = X_GTGLFW_GET( pGT );

   if( glfw->msgAlert && delay * 100 < 1 )
      return s_gt_glfw_MessageBox( glfw, message, options );

   return s_super.Alert( pGT, message, options, normal, high, delay );
}

/* ---------------------------------------------------------------------------------------------
   Graphics on the shared 0xRRGGBB software canvas
   --------------------------------------------------------------------------------------------- */

static int s_gt_glfw_GetGfxClip( X_GTGLFW *glfw, X_GTGLFW_RECT *clip )
{
   if( glfw->clipOn )
   {
      clip->top = GTGLFW_MAX( glfw->clipTop, 0 );
      clip->left = GTGLFW_MAX( glfw->clipLeft, 0 );
      clip->bottom = GTGLFW_MIN( glfw->clipBottom, glfw->canvasHeight - 1 );
      clip->right = GTGLFW_MIN( glfw->clipRight, glfw->canvasWidth - 1 );
   }
   else
   {
      clip->top = clip->left = 0;
      clip->bottom = glfw->canvasHeight - 1;
      clip->right = glfw->canvasWidth - 1;
   }

   return clip->top <= clip->bottom && clip->left <= clip->right;
}

static void s_gt_glfw_GfxPixel( X_GTGLFW *glfw, int x, int y, uint32_t color )
{
   uint32_t *pixel = glfw->canvas + ( size_t )y * glfw->canvasWidth + x;

   if( glfw->gfxMode == GFX_MODE_XOR )
      *pixel ^= color;
   else
      *pixel = color;
}

static void s_gt_glfw_GfxPlot( X_GTGLFW *glfw, const X_GTGLFW_RECT *clip, int64_t x, int64_t y, uint32_t color )
{
   if( x >= clip->left && x <= clip->right && y >= clip->top && y <= clip->bottom )
      s_gt_glfw_GfxPixel( glfw, ( int )x, ( int )y, color );
}

static void s_gt_glfw_GfxLine( X_GTGLFW *glfw, const X_GTGLFW_RECT *clip, int64_t top, int64_t left, int64_t bottom,
                               int64_t right, uint32_t color )
{
   int64_t dx = llabs( right - left );
   int64_t dy = llabs( bottom - top );
   int64_t first, last, position;
   int     direction;

   if( dx == 0 && dy == 0 )
   {
      s_gt_glfw_GfxPlot( glfw, clip, left, top, color );
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
      first = GTGLFW_MAX( left, clip->left );
      last = GTGLFW_MIN( right, clip->right );
      for( position = first; position <= last; ++position )
      {
         int64_t offset = ( int64_t )( ( ( __int128 )( position - left ) * dy + dx / 2 ) / dx );

         s_gt_glfw_GfxPlot( glfw, clip, position, top + direction * offset, color );
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
      first = GTGLFW_MAX( top, clip->top );
      last = GTGLFW_MIN( bottom, clip->bottom );
      for( position = first; position <= last; ++position )
      {
         int64_t offset = ( int64_t )( ( ( __int128 )( position - top ) * dx + dy / 2 ) / dy );

         s_gt_glfw_GfxPlot( glfw, clip, left + direction * offset, position, color );
      }
   }
}

static int64_t s_gt_glfw_EllipseOffset( int64_t position, int center, int radius, int otherRadius )
{
   int64_t     distance = llabs( position - center );
   long double span = sqrtl( ( long double )( radius - distance ) * ( radius + distance ) );

   return ( int64_t )floorl( span * otherRadius / radius + 0.5L );
}

static void s_gt_glfw_GfxEllipse( X_GTGLFW *glfw, const X_GTGLFW_RECT *clip, int centerY, int centerX, int radiusY,
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
      s_gt_glfw_GfxLine( glfw, clip, centerY, left, centerY, right, color );
      return;
   }
   if( radiusX == 0 )
   {
      s_gt_glfw_GfxLine( glfw, clip, top, centerX, bottom, centerX, color );
      return;
   }

   start = GTGLFW_MAX( ( int64_t )clip->top, top );
   end = GTGLFW_MIN( ( int64_t )clip->bottom, bottom );
   for( position = start; position <= end; ++position )
   {
      int64_t offset = s_gt_glfw_EllipseOffset( position, centerY, radiusY, radiusX );
      int64_t lineLeft = ( int64_t )centerX - offset;
      int64_t lineRight = ( int64_t )centerX + offset;

      if( filled )
         s_gt_glfw_GfxLine( glfw, clip, position, lineLeft, position, lineRight, color );
      else
      {
         s_gt_glfw_GfxPlot( glfw, clip, lineLeft, position, color );
         if( offset != 0 )
            s_gt_glfw_GfxPlot( glfw, clip, lineRight, position, color );
      }
   }
   if( !filled )
   {
      start = GTGLFW_MAX( ( int64_t )clip->left, left );
      end = GTGLFW_MIN( ( int64_t )clip->right, right );
      for( position = start; position <= end; ++position )
      {
         int64_t offset = s_gt_glfw_EllipseOffset( position, centerX, radiusX, radiusY );
         int64_t rowOffset = s_gt_glfw_EllipseOffset( ( int64_t )centerY - offset, centerY, radiusY, radiusX );

         if( llabs( position - centerX ) != rowOffset )
         {
            s_gt_glfw_GfxPlot( glfw, clip, position, ( int64_t )centerY - offset, color );
            if( offset != 0 )
               s_gt_glfw_GfxPlot( glfw, clip, position, ( int64_t )centerY + offset, color );
         }
      }
   }
}

static void s_gt_glfw_GfxFloodFill( X_GTGLFW *glfw, const X_GTGLFW_RECT *clip, int startY, int startX, uint32_t color )
{
   if( startX >= clip->left && startX <= clip->right && startY >= clip->top && startY <= clip->bottom )
   {
      uint32_t source = glfw->canvas[( size_t )startY * glfw->canvasWidth + startX];
      uint32_t target = IIF( glfw->gfxMode == GFX_MODE_XOR, source ^ color, color );

      if( source != target )
      {
         size_t  used = 0;
         size_t *stack = ( size_t * )x_xgrabArray( ( size_t )glfw->canvasWidth * glfw->canvasHeight, sizeof( size_t ) );

         glfw->canvas[( size_t )startY * glfw->canvasWidth + startX] = target;
         stack[used++] = ( size_t )startY * glfw->canvasWidth + startX;
         while( used )
         {
            size_t index = stack[--used];
            int    x = ( int )( index % glfw->canvasWidth );
            int    y = ( int )( index / glfw->canvasWidth );
            int    direction;

            for( direction = 0; direction < 4; ++direction )
            {
               int nextX = x + s_floodDX[direction];
               int nextY = y + s_floodDY[direction];

               if( nextX >= clip->left && nextX <= clip->right && nextY >= clip->top && nextY <= clip->bottom )
               {
                  size_t next = ( size_t )nextY * glfw->canvasWidth + nextX;

                  if( glfw->canvas[next] == source )
                  {
                     glfw->canvas[next] = target;
                     stack[used++] = next;
                  }
               }
            }
         }
         x_xfree( stack );
      }
   }
}

static void s_gt_glfw_DrawGfxGlyph( X_GTGLFW *glfw, uint16_t code, int top, int left, int height, int width,
                                    uint32_t color, const X_GTGLFW_RECT *clip )
{
   int glyph;
   int firstX, firstY, lastX, lastY;
   int x, y;

   if( height <= 0 || width <= 0 )
      return;

   firstX = ( int )GTGLFW_MIN( ( int64_t )width, GTGLFW_MAX( 0, ( int64_t )clip->left - left ) );
   firstY = ( int )GTGLFW_MIN( ( int64_t )height, GTGLFW_MAX( 0, ( int64_t )clip->top - top ) );
   lastX = ( int )GTGLFW_MIN( ( int64_t )width - 1, ( int64_t )clip->right - left );
   lastY = ( int )GTGLFW_MIN( ( int64_t )height - 1, ( int64_t )clip->bottom - top );
   if( firstX > lastX || firstY > lastY )
      return;

   glyph = s_gt_glfw_FindGlyph( code );
   if( glyph < 0 )
      return;

   for( y = firstY; y <= lastY; ++y )
   {
      int      sourceY = ( int )( ( ( int64_t )y * 2 + 1 ) * X_FONT9X18_HEIGHT / ( ( int64_t )height * 2 ) );
      int      targetY = top + y;
      uint16_t bits = s_font9x18Bitmaps[glyph][sourceY];

      for( x = firstX; x <= lastX; ++x )
      {
         int sourceX = ( int )( ( ( int64_t )x * 2 + 1 ) * X_FONT9X18_WIDTH / ( ( int64_t )width * 2 ) );
         int targetX = left + x;

         if( ( bits & ( 0x8000U >> sourceX ) ) != 0 )
            s_gt_glfw_GfxPixel( glfw, targetX, targetY, color );
      }
   }
}

static int s_gt_glfw_gfx_Primitive( X_GT_BASE *pGT, int type, int top, int left, int bottom, int right, int color )
{
   X_GTGLFW     *glfw = X_GTGLFW_GET( pGT );
   X_GTGLFW_RECT clip;

   switch( type )
   {
      case GFX_ACQUIRESCREEN:
         ++glfw->gfxDepth;
         X_GTSELF_DISPBEGIN( pGT );
         return 1;
      case GFX_RELEASESCREEN:
         if( glfw->gfxDepth == 0 )
            return 0;

         --glfw->gfxDepth;
         X_GTSELF_DISPEND( pGT );
         return 1;
      case GFX_MAKECOLOR:
      {
         int red = GTGLFW_MIN( GTGLFW_MAX( top, 0 ), 255 );
         int green = GTGLFW_MIN( GTGLFW_MAX( left, 0 ), 255 );
         int blue = GTGLFW_MIN( GTGLFW_MAX( bottom, 0 ), 255 );

         return ( red << 16 ) | ( green << 8 ) | blue;
      }
      case GFX_CLIPTOP: return glfw->clipTop;
      case GFX_CLIPLEFT: return glfw->clipLeft;
      case GFX_CLIPBOTTOM: return glfw->clipBottom;
      case GFX_CLIPRIGHT: return glfw->clipRight;
      case GFX_SETCLIP:
         if( top == 0 && left == 0 && bottom == 0 && right == 0 )
         {
            glfw->clipTop = glfw->clipLeft = glfw->clipBottom = glfw->clipRight = 0;
            glfw->clipOn = 0;
         }
         else
         {
            glfw->clipTop = GTGLFW_MIN( top, bottom );
            glfw->clipLeft = GTGLFW_MIN( left, right );
            glfw->clipBottom = GTGLFW_MAX( top, bottom );
            glfw->clipRight = GTGLFW_MAX( left, right );
            glfw->clipOn = 1;
         }
         return 1;
      case GFX_DRAWINGMODE:
         if( top == GFX_MODE_SOLID || top == GFX_MODE_XOR )
            glfw->gfxMode = top;
         return glfw->gfxMode;
      case GFX_GETPIXEL:
         s_super.Refresh( pGT );
         if( top >= 0 && left >= 0 && top < glfw->canvasHeight && left < glfw->canvasWidth )
            return ( int )( glfw->canvas[( size_t )top * glfw->canvasWidth + left] & 0xFFFFFF );

         return 0;
      default: break;
   }

   if( type < GFX_PUTPIXEL || type > GFX_FLOODFILL )
      return s_super.GfxPrimitive( pGT, type, top, left, bottom, right, color );

   if( !s_gt_glfw_GetGfxClip( glfw, &clip ) )
      return 1;

   s_super.Refresh( pGT );
   if( type == GFX_PUTPIXEL || type == GFX_FLOODFILL )
      color = bottom;
   else if( type == GFX_CIRCLE || type == GFX_FILLEDCIRCLE )
      color = right;
   color &= 0xFFFFFF;

   switch( type )
   {
      case GFX_PUTPIXEL: s_gt_glfw_GfxPlot( glfw, &clip, left, top, ( uint32_t )color ); break;
      case GFX_LINE: s_gt_glfw_GfxLine( glfw, &clip, top, left, bottom, right, ( uint32_t )color ); break;
      case GFX_RECT:
      {
         int64_t firstY = GTGLFW_MIN( top, bottom );
         int64_t lastY = GTGLFW_MAX( top, bottom );
         int64_t firstX = GTGLFW_MIN( left, right );
         int64_t lastX = GTGLFW_MAX( left, right );

         s_gt_glfw_GfxLine( glfw, &clip, firstY, firstX, firstY, lastX, ( uint32_t )color );
         if( lastY != firstY )
            s_gt_glfw_GfxLine( glfw, &clip, lastY, firstX, lastY, lastX, ( uint32_t )color );
         if( lastY - firstY > 1 )
         {
            s_gt_glfw_GfxLine( glfw, &clip, firstY + 1, firstX, lastY - 1, firstX, ( uint32_t )color );
            if( lastX != firstX )
               s_gt_glfw_GfxLine( glfw, &clip, firstY + 1, lastX, lastY - 1, lastX, ( uint32_t )color );
         }
         break;
      }
      case GFX_FILLEDRECT:
      {
         int firstY = GTGLFW_MAX( GTGLFW_MIN( top, bottom ), clip.top );
         int lastY = GTGLFW_MIN( GTGLFW_MAX( top, bottom ), clip.bottom );
         int firstX = GTGLFW_MAX( GTGLFW_MIN( left, right ), clip.left );
         int lastX = GTGLFW_MIN( GTGLFW_MAX( left, right ), clip.right );
         int y;

         for( y = firstY; y <= lastY; ++y )
         {
            int x;

            for( x = firstX; x <= lastX; ++x )
               s_gt_glfw_GfxPixel( glfw, x, y, ( uint32_t )color );
         }
         break;
      }
      case GFX_CIRCLE: s_gt_glfw_GfxEllipse( glfw, &clip, top, left, bottom, bottom, 0, ( uint32_t )color ); break;
      case GFX_FILLEDCIRCLE:
         s_gt_glfw_GfxEllipse( glfw, &clip, top, left, bottom, bottom, 1, ( uint32_t )color );
         break;
      case GFX_ELLIPSE: s_gt_glfw_GfxEllipse( glfw, &clip, top, left, bottom, right, 0, ( uint32_t )color ); break;
      case GFX_FILLEDELLIPSE:
         s_gt_glfw_GfxEllipse( glfw, &clip, top, left, bottom, right, 1, ( uint32_t )color );
         break;
      case GFX_FLOODFILL: s_gt_glfw_GfxFloodFill( glfw, &clip, top, left, ( uint32_t )color ); break;
      default: break;
   }

   glfw->canvasDirty = 1;
   glfw->presentPending = 1;
   if( glfw->gfxDepth == 0 )
      s_gt_glfw_Present( glfw );

   return 1;
}

static void s_gt_glfw_gfx_Text( X_GT_BASE *pGT, int top, int left, const char *text, int color, int size, int width )
{
   X_GTGLFW     *glfw = X_GTGLFW_GET( pGT );
   X_GTGLFW_RECT clip;
   uint16_t      stackBuffer[256];
   uint16_t     *characters;
   size_t        sourceLength = strlen( text );
   size_t        length;
   size_t        index;
   int64_t       position = left;
   uint32_t      glyphColor = ( uint32_t )color & 0xFFFFFF;

   size = IIF( size == 0, X_GTGLFW_CELL_HEIGHT, size );
   width = IIF( width == 0, GTGLFW_MAX( size / 2, 1 ), width );
   if( size <= 0 || width <= 0 || !s_gt_glfw_GetGfxClip( glfw, &clip ) )
      return;

   if( ( int64_t )top + size <= clip.top || top > clip.bottom || left > clip.right )
      return;

   characters = stackBuffer;
   if( sourceLength >= 256 )
      characters = ( uint16_t * )x_xgrabArray( sourceLength + 1, sizeof( uint16_t ) );
   length = x_utf8ToU16( text, sourceLength, characters, sourceLength + 1 );
   s_super.Refresh( pGT );

   for( index = 0; index < length && position <= clip.right; ++index, position += width )
   {
      if( characters[index] == '\r' || characters[index] == '\n' )
         break;
      if( position + width > clip.left )
         s_gt_glfw_DrawGfxGlyph( glfw, characters[index], top, ( int )position, size, width, glyphColor, &clip );
   }

   if( characters != stackBuffer )
      x_xfree( characters );
   glfw->canvasDirty = 1;
   glfw->presentPending = 1;
   if( glfw->gfxDepth == 0 )
      s_gt_glfw_Present( glfw );
}

static void s_gt_glfw_Init( X_GT_BASE *pGT, int hIn, int hOut, int hErr )
{
   X_GTGLFW *glfw;

   if( !s_gt_glfw_Start() )
   {
      const char *description = IIF( s_errorText[0], s_errorText, "no error description" );

      fprintf( stderr, "xDev GTGLFW: GLFW initialization failed (%d): %s\n", s_errorCode, description );
      x_quitRaise();
   }

   glfw = ( X_GTGLFW * )x_xgrabz( sizeof( X_GTGLFW ) );
   glfw->pGT = pGT;
   glfw->title = x_strdup( "xDev GTGLFW" );
   glfw->rows = X_GTGLFW_DEFAULT_ROWS;
   glfw->cols = X_GTGLFW_DEFAULT_COLS;
   glfw->canvasWidth = glfw->cols * X_GTGLFW_CELL_WIDTH;
   glfw->canvasHeight = glfw->rows * X_GTGLFW_CELL_HEIGHT;
   glfw->windowWidth = glfw->canvasWidth;
   glfw->windowHeight = glfw->canvasHeight;
   glfw->framebufferWidth = glfw->canvasWidth;
   glfw->framebufferHeight = glfw->canvasHeight;
   glfw->contentScaleX = 1.0f;
   glfw->contentScaleY = 1.0f;
   glfw->cursorRow = -1;
   glfw->cursorCol = -1;
   glfw->cursorStyle = SC_NONE;
   glfw->cursorBlinkRate = X_GT_CURSOR_BLINK;
   glfw->cursorTime = 0.0;
   glfw->cursorVisible = 1;
   glfw->mouseOn = 1;
   glfw->msgAlert = 1;
   glfw->resizable = 1;
   glfw->resizeStep = 1;
   glfw->resizeMode = X_GTI_RESIZEMODE_ROWS;
   glfw->gfxMode = GFX_MODE_SOLID;
   glfw->clipBottom = glfw->canvasHeight - 1;
   glfw->clipRight = glfw->canvasWidth - 1;
   glfw->clipOn = 1;
   glfw->canvasDirty = 1;
   glfw->presentPending = 1;
   memcpy( glfw->colors, s_defaultColors, sizeof( glfw->colors ) );

   glfwDefaultWindowHints();
   glfwWindowHint( GLFW_CONTEXT_VERSION_MAJOR, 2 );
   glfwWindowHint( GLFW_CONTEXT_VERSION_MINOR, 1 );
   glfwWindowHint( GLFW_DOUBLEBUFFER, GLFW_TRUE );
   glfwWindowHint( GLFW_RESIZABLE, GLFW_TRUE );
   glfwWindowHint( GLFW_SCALE_TO_MONITOR, GLFW_TRUE );
   glfwWindowHint( GLFW_SCALE_FRAMEBUFFER, GLFW_TRUE );
   glfwWindowHint( GLFW_VISIBLE, GLFW_FALSE );
   glfw->window = glfwCreateWindow( glfw->windowWidth, glfw->windowHeight, glfw->title, NULL, NULL );
   if( !glfw->window )
   {
      s_gt_glfw_InitFailed( glfw, "cannot create the GLFW window or OpenGL 2.1 context" );
      return;
   }

   glfwSetWindowUserPointer( glfw->window, glfw );
   glfwSetInputMode( glfw->window, GLFW_LOCK_KEY_MODS, GLFW_TRUE );
   glfwMakeContextCurrent( glfw->window );
   glfwSwapInterval( 1 );
   if( glGetString( GL_VERSION ) == NULL )
   {
      s_gt_glfw_InitFailed( glfw, "the OpenGL context is unavailable" );
      return;
   }
   s_gt_glfw_UpdateMetrics( glfw );
   glfwGetWindowPos( glfw->window, &glfw->windowX, &glfw->windowY );
   s_gt_glfw_UpdateSizeLimits( glfw );
   s_gt_glfw_SetWindowSizeForGrid( glfw );
   if( !s_gt_glfw_CreateCanvas( glfw, glfw->rows, glfw->cols ) )
   {
      s_gt_glfw_InitFailed( glfw, "cannot create the RGB canvas and OpenGL texture" );
      return;
   }

   pGT->driverData[s_GtId] = glfw;
   s_super.Init( pGT, hIn, hOut, hErr );
   X_GTSELF_SETFLAG( pGT, X_GTI_COMPATBUFFER, 0 );
   X_GTSELF_RESIZE( pGT, glfw->rows, glfw->cols );
   X_GTSELF_SETFLAG( pGT, X_GTI_REDRAWMAX, 1 );

   glfw->next = s_windows;
   s_windows = glfw;

   s_gt_glfw_RegisterWindowCallbacks( glfw );
   s_gt_glfw_CenterWindow( glfw );
   glfwShowWindow( glfw->window );
   s_gt_glfw_UpdateMetrics( glfw );
   /* Showing the window can change its actual size, for example when Windows starts it
      maximized or applies the monitor DPI.  Synchronize the character grid now; otherwise
      it stays at the creation size until the user resizes the window for the first time. */
   s_gt_glfw_ResizeFromWindow( glfw );
   glfwGetWindowPos( glfw->window, &glfw->windowX, &glfw->windowY );
   {
      double mouseX, mouseY;

      glfwGetCursorPos( glfw->window, &mouseX, &mouseY );
      s_gt_glfw_UpdateMousePosition( glfw, mouseX, mouseY, 0 );
   }
   glfw->cursorTime = glfwGetTime();
   X_GTSELF_SEMICOLD( pGT );
   X_GTSELF_EXPOSEAREA( pGT, 0, 0, glfw->rows - 1, glfw->cols - 1 );
   s_gt_glfw_Refresh( pGT );
}

static void s_gt_glfw_Exit( X_GT_BASE *pGT )
{
   X_GTGLFW *glfw = X_GTGLFW_GET( pGT );

   if( glfw )
   {
      X_GTGLFW **link = &s_windows;

      while( *link && *link != glfw )
         link = &( *link )->next;
      if( *link )
         *link = glfw->next;

      if( glfw->window )
         s_gt_glfw_UnregisterWindowCallbacks( glfw );
      s_gt_glfw_DestroyCanvas( glfw );
#if X_OS_WINDOWS
      if( glfw->iconOwned && glfw->iconHandle )
         DestroyIcon( ( HICON )glfw->iconHandle );
#endif
      if( glfw->window )
         glfwDestroyWindow( glfw->window );
      x_xfree( glfw->title );
      pGT->driverData[s_GtId] = NULL;
      x_xfree( glfw );
      s_gt_glfw_Stop();
   }

   s_super.Exit( pGT );
}

static int s_gt_glfw_SetMode( X_GT_BASE *pGT, int rows, int cols )
{
   X_GTGLFW *glfw = X_GTGLFW_GET( pGT );

   if( !s_gt_glfw_ResizeGrid( glfw, rows, cols, 1, 1 ) )
      return 0;

   s_gt_glfw_Refresh( pGT );

   return 1;
}

static const char *s_gt_glfw_Version( X_GT_BASE *pGT, int type )
{
   UNUSED( pGT );

   return IIF( type == 0, "GLFW", "xDev Terminal: GLFW" );
}

/* cells keep the 24-bit colors of DispOutAtExt() and DispBoxExt(), see s_gt_glfw_CellColors() */
static int s_gt_glfw_TrueColor( X_GT_BASE *pGT )
{
   UNUSED( pGT );

   return 1;
}

static int s_gt_glfw_FuncInit( X_GT_FUNCS *pFuncTable )
{
   pFuncTable->Init = s_gt_glfw_Init;
   pFuncTable->Exit = s_gt_glfw_Exit;
   pFuncTable->SetMode = s_gt_glfw_SetMode;
   pFuncTable->Redraw = s_gt_glfw_Redraw;
   pFuncTable->Refresh = s_gt_glfw_Refresh;
   pFuncTable->Version = s_gt_glfw_Version;
   pFuncTable->Info = s_gt_glfw_Info;
   pFuncTable->Alert = s_gt_glfw_Alert;
   pFuncTable->ReadKey = s_gt_glfw_ReadKey;
   pFuncTable->TrueColor = s_gt_glfw_TrueColor;

   pFuncTable->MouseIsPresent = s_gt_glfw_mouse_IsPresent;
   pFuncTable->MouseGetPos = s_gt_glfw_mouse_GetPos;
   pFuncTable->MouseSetPos = s_gt_glfw_mouse_SetPos;
   pFuncTable->MouseButtonState = s_gt_glfw_mouse_ButtonState;
   pFuncTable->MouseCountButton = s_gt_glfw_mouse_CountButton;

   pFuncTable->GfxPrimitive = s_gt_glfw_gfx_Primitive;
   pFuncTable->GfxText = s_gt_glfw_gfx_Text;

   return 1;
}

static const X_GT_INIT s_gtInit = { "GLFW", s_gt_glfw_FuncInit, &s_super, &s_GtId };

static void __attribute__( ( constructor ) ) s_gt_glfw_Register( void )
{
   x_gtRegister( &s_gtInit );
}

/* Linker anchor used by REQUEST GTGLFW. */
X_FUNC( GTGLFW )
{
   UNUSED( ctx );
}
