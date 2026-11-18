/*
 * Copyright (c) 2024-2026 xDev-Language
 *
 * Library: xDev GTSDL3
 * File: gtsdl3.c
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "gtsdl3.h"
#include "../../fonts/xfont9x18.h"

#include <limits.h>
#include <stdio.h>
#include <string.h>

#if X_OS_WINDOWS
#include <windows.h>
#endif

#define X_GTSDL3_KEY_QUEUE_SIZE 1024
#define X_GTSDL3_CELL_WIDTH X_FONT9X18_WIDTH
#define X_GTSDL3_CELL_HEIGHT X_FONT9X18_HEIGHT
#define X_GTSDL3_DEFAULT_ROWS 25
#define X_GTSDL3_DEFAULT_COLS 80
#define X_GTSDL3_ATLAS_COLS 32
#define X_GTSDL3_GFX_TEXT_STACK 256

typedef struct
{
   SDL_Texture  *oldTarget;
   SDL_Rect      oldClip;
   SDL_BlendMode oldBlend;
   Uint8         oldRed;
   Uint8         oldGreen;
   Uint8         oldBlue;
   Uint8         oldAlpha;
   int           hadClip;
} X_GTSDL3_GFX_STATE;

typedef struct _X_GTSDL3
{
   X_GT_BASE         *pGT;
   struct _X_GTSDL3  *next;
   SDL_Window        *window;
   void              *titleHandle;
   const char        *title;
   SDL_Surface       *icon;
   SDL_AudioStream   *sound;
   int                audioOn;
   SDL_WindowID       windowID;
   SDL_Renderer      *renderer;
   SDL_Texture       *font;
   SDL_Texture       *screen;
   int                rows;
   int                cols;
   int                winWidth;
   int                winHgt;
   int                outWidth;
   int                outHgt;
   int                logWidth;
   int                logHgt;
   float              density;
   float              dispScal;
   float              scale;
   Uint8              colors[16][3];
   Uint32             fgColor;
   int                curRow;
   int                curCol;
   int                curStyle;
   int                blink;
   Uint64             curTime;
   int                curOn;
   int                mouseRow;
   int                mouseCol;
   int                mouseX;
   int                mouseY;
   Uint32             buttons;
   Uint64             btnTime[3];
   float              wheelX;
   float              wheelY;
   int                mouseOn;
   int                copyOn;
   int                selectOn;
   int                skipUp;
   int                clipTop;
   int                clipLeft;
   int                clipBtm;
   int                clipRgt;
   int                clipOn;
   int                gfxDepth;
   int                gfxOn;
   X_GTSDL3_GFX_STATE gfxState;
   int                selRow;
   int                selCol;
   int                endRow;
   int                endCol;
   int                canSize;
   int                sizeStep;
   int                altEnter;
   int                msgAlert;
   int                resized;
   int                newWidth;
   int                newHgt;
   int                pixelChg;
   int                scaleChg;
   int                drawing;
   SDL_AtomicInt      paint;
   int                close;
   int                keyQueue[X_GTSDL3_KEY_QUEUE_SIZE];
   SDL_Scancode       keyScan[X_GTSDL3_KEY_QUEUE_SIZE];
   X_BYTE             keyRpt[X_GTSDL3_KEY_QUEUE_SIZE];
   int                keyIn;
   int                keyOut;
   SDL_Scancode       textScan;
   int                textFlag;
   int                textRpt;
} X_GTSDL3;

static void s_gt_sdl3_BeginGfx( X_GTSDL3 *sdl, X_GTSDL3_GFX_STATE *state, int useClip );
static void s_gt_sdl3_EndGfx( X_GTSDL3 *sdl, X_GTSDL3_GFX_STATE *state );
static void s_gt_sdl3_ResizeFromWindow( X_GTSDL3 *sdl, int width, int height );

typedef struct
{
   SDL_AtomicInt done;
   char        **files;
   int           count;
   int           filter;
   char         *error;
} X_GTSDL3_FILEDIALOG_RESULT;

static int        s_GtId;
static X_GT_FUNCS s_super;
static X_GTSDL3  *s_windows;
static int        s_pumpingEvents;

#define X_GTSUPER ( &s_super )
#define X_GTID_PTR ( &s_GtId )
#define X_GTLOCAL( pGT ) ( ( pGT )->driverData[s_GtId] )
#define X_GTSDL3_GET( pGT ) ( ( X_GTSDL3 * )X_GTLOCAL( pGT ) )

static X_GTSDL3 *s_gt_sdl3_FindByWindowID( SDL_WindowID windowID )
{
   X_GTSDL3 *sdl = s_windows;

   while( sdl && sdl->windowID != windowID )
      sdl = sdl->next;

   return sdl;
}

static const Uint8 s_defaultColors[16][3] = { { 0, 0, 0 },     { 0, 0, 170 },    { 0, 170, 0 },    { 0, 170, 170 },
                                              { 170, 0, 0 },   { 170, 0, 170 },  { 170, 85, 0 },   { 170, 170, 170 },
                                              { 85, 85, 85 },  { 85, 85, 255 },  { 85, 255, 85 },  { 85, 255, 255 },
                                              { 255, 85, 85 }, { 255, 85, 255 }, { 255, 255, 85 }, { 255, 255, 255 } };

static int s_gt_sdl3_FindGlyphExact( X_USHORT code )
{
   int left = 0;
   int right = X_FONT9X18_GLYPH_COUNT - 1;

   while( left <= right )
   {
      int      middle = left + ( right - left ) / 2;
      X_USHORT current = s_font9x18Codepoints[middle];

      if( current < code )
         left = middle + 1;
      else if( current > code )
         right = middle - 1;
      else
         return middle;
   }

   return -1;
}

static int s_gt_sdl3_FindGlyph( X_USHORT code )
{
   int glyph = s_gt_sdl3_FindGlyphExact( code );

   if( glyph < 0 && code == 0x00A0 )
      glyph = s_gt_sdl3_FindGlyphExact( 0x0020 );
   if( glyph < 0 )
      glyph = s_gt_sdl3_FindGlyphExact( 0xFFFD );
   if( glyph < 0 )
      glyph = s_gt_sdl3_FindGlyphExact( 0x003F );

   return glyph;
}

static void s_gt_sdl3_CreateFontTexture( X_GTSDL3 *sdl )
{
   int                           atlasW = X_GTSDL3_ATLAS_COLS * X_FONT9X18_WIDTH;
   int                           atlasRow = ( X_FONT9X18_GLYPH_COUNT + X_GTSDL3_ATLAS_COLS - 1 ) / X_GTSDL3_ATLAS_COLS;
   int                           atlasH = atlasRow * X_FONT9X18_HEIGHT;
   const SDL_PixelFormatDetails *format = SDL_GetPixelFormatDetails( SDL_PIXELFORMAT_RGBA32 );
   Uint32                        opaque;
   Uint32                        clear;
   Uint32                       *pixels;
   SDL_Texture                  *texture;
   int                           glyph;

   opaque = SDL_MapRGBA( format, NULL, 255, 255, 255, 255 );
   clear = SDL_MapRGBA( format, NULL, 255, 255, 255, 0 );
   pixels = ( Uint32 * )x_xgrabzArray( ( X_SIZE )atlasW * atlasH, sizeof( Uint32 ) );

   for( glyph = 0; glyph < X_FONT9X18_GLYPH_COUNT; ++glyph )
   {
      int atlasX = ( glyph % X_GTSDL3_ATLAS_COLS ) * X_FONT9X18_WIDTH;
      int atlasY = ( glyph / X_GTSDL3_ATLAS_COLS ) * X_FONT9X18_HEIGHT;
      int y, x;

      for( y = 0; y < X_FONT9X18_HEIGHT; ++y )
      {
         uint16_t bits = s_font9x18Bitmaps[glyph][y];

         for( x = 0; x < X_FONT9X18_WIDTH; ++x )
            pixels[( atlasY + y ) * atlasW + atlasX + x] = IIF( ( bits & ( 0x8000 >> x ) ) != 0, opaque, clear );
      }
   }

   texture = SDL_CreateTexture( sdl->renderer, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STATIC, atlasW, atlasH );
   SDL_UpdateTexture( texture, NULL, pixels, atlasW * ( int )sizeof( Uint32 ) );
   SDL_SetTextureBlendMode( texture, SDL_BLENDMODE_BLEND );
   SDL_SetTextureScaleMode( texture, SDL_SCALEMODE_NEAREST );

   x_xfree( pixels );
   sdl->font = texture;
   sdl->fgColor = -1;
}

static SDL_Texture *s_gt_sdl3_CreateScreenTexture( X_GTSDL3 *sdl, int rows, int cols )
{
   SDL_Texture *texture;
   const Uint8 *bg = sdl->colors[0];
   int          width = cols * X_GTSDL3_CELL_WIDTH;
   int          height = rows * X_GTSDL3_CELL_HEIGHT;

   texture = SDL_CreateTexture( sdl->renderer, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_TARGET, width, height );
   SDL_SetTextureScaleMode( texture, SDL_SCALEMODE_NEAREST );
   SDL_SetRenderTarget( sdl->renderer, texture );
   SDL_SetRenderDrawColor( sdl->renderer, bg[0], bg[1], bg[2], SDL_ALPHA_OPAQUE );
   SDL_RenderClear( sdl->renderer );
   SDL_SetRenderTarget( sdl->renderer, NULL );

   return texture;
}

static void s_gt_sdl3_UpdateMetrics( X_GTSDL3 *sdl )
{
   int    winWidth, winHgt, outWidth, outHgt;
   int    logWidth, logHgt;
   float  density, dispScal;
   double widthVal, hgtVal;

   SDL_GetWindowSize( sdl->window, &winWidth, &winHgt );
   SDL_GetRenderOutputSize( sdl->renderer, &outWidth, &outHgt );
   density = SDL_GetWindowPixelDensity( sdl->window );
   dispScal = SDL_GetWindowDisplayScale( sdl->window );
   widthVal = outWidth / ( double )dispScal;
   hgtVal = outHgt / ( double )dispScal;
   logWidth = GTSDL3_MAX( ( int )( widthVal + 0.5 ), 1 );
   logHgt = GTSDL3_MAX( ( int )( hgtVal + 0.5 ), 1 );

   if( logWidth != sdl->logWidth || logHgt != sdl->logHgt )
   {
      SDL_Texture *target = SDL_GetRenderTarget( sdl->renderer );

      SDL_SetRenderTarget( sdl->renderer, NULL );
      SDL_SetRenderLogicalPresentation( sdl->renderer, logWidth, logHgt, SDL_LOGICAL_PRESENTATION_STRETCH );
      SDL_SetRenderTarget( sdl->renderer, target );
   }

   sdl->winWidth = winWidth;
   sdl->winHgt = winHgt;
   sdl->outWidth = outWidth;
   sdl->outHgt = outHgt;
   sdl->logWidth = logWidth;
   sdl->logHgt = logHgt;
   sdl->density = density;
   sdl->dispScal = dispScal;
   sdl->scale = dispScal / density;
}

static void s_gt_sdl3_DrawGlyph( X_GTSDL3 *sdl, X_USHORT code, int row, int col, int height, int width )
{
   int glyph = s_gt_sdl3_FindGlyph( code );

   if( glyph >= 0 )
   {
      SDL_FRect src = { ( float )( ( glyph % X_GTSDL3_ATLAS_COLS ) * X_FONT9X18_WIDTH ),
                        ( float )( ( glyph / X_GTSDL3_ATLAS_COLS ) * X_FONT9X18_HEIGHT ), X_FONT9X18_WIDTH,
                        X_FONT9X18_HEIGHT };
      SDL_FRect dst = { ( float )col, ( float )row, ( float )width, ( float )height };

      SDL_RenderTexture( sdl->renderer, sdl->font, &src, &dst );
   }
}

static int s_gt_sdl3_UpdateCursor( X_GTSDL3 *sdl, int syncPos )
{
   int    row, col, style;
   Uint64 now = SDL_GetTicks();

   X_GTSELF_GETSCRCURSOR( sdl->pGT, &row, &col, &style );

   if( row < 0 || col < 0 || row >= sdl->rows || col >= sdl->cols || style < SC_NONE || style > SC_SPECIAL2 )
      style = SC_NONE;

   /* Keep the last visible position during consecutive automatic Flush()
      calls. ReadKey() synchronizes the final position after the application
      finishes its screen updates, like an asynchronous native caret. */
   if( ( syncPos || style == SC_NONE ) && ( row != sdl->curRow || col != sdl->curCol || style != sdl->curStyle ) )
   {
      sdl->curRow = row;
      sdl->curCol = col;
      sdl->curStyle = style;
      sdl->curOn = style != SC_NONE;
      sdl->curTime = now;
      return 1;
   }

   if( style == SC_NONE )
      return 0;

   if( sdl->blink == 0 )
   {
      if( !sdl->curOn )
      {
         sdl->curOn = 1;
         return 1;
      }
   }
   else
   {
      Uint64 interval = ( Uint64 )sdl->blink;
      Uint64 elapsed = now - sdl->curTime;

      if( elapsed >= interval )
      {
         Uint64 steps = elapsed / interval;

         sdl->curTime += steps * interval;

         if( ( steps & 1 ) != 0 )
         {
            sdl->curOn = !sdl->curOn;
            return 1;
         }
      }
   }

   return 0;
}

static void s_gt_sdl3_DrawCursor( X_GTSDL3 *sdl )
{
   int y, height;

   if( !sdl->curOn || sdl->curStyle == SC_NONE )
      return;

   switch( sdl->curStyle )
   {
      case SC_NORMAL:
         height = 2;
         y = X_GTSDL3_CELL_HEIGHT - 3;
         break;
      case SC_INSERT:
         height = ( X_GTSDL3_CELL_HEIGHT - 2 ) >> 1;
         y = X_GTSDL3_CELL_HEIGHT - height - 1;
         break;
      case SC_SPECIAL1:
         height = X_GTSDL3_CELL_HEIGHT - 2;
         y = 1;
         break;
      case SC_SPECIAL2:
         height = ( X_GTSDL3_CELL_HEIGHT - 2 ) >> 1;
         y = 1;
         break;
      default: return;
   }

   {
      int          color;
      X_BYTE       attr;
      X_USHORT     ch;
      const Uint8 *fg;
      SDL_FRect    rect = { ( float )( sdl->curCol * X_GTSDL3_CELL_WIDTH ),
                            ( float )( sdl->curRow * X_GTSDL3_CELL_HEIGHT + y ), X_GTSDL3_CELL_WIDTH, ( float )height };

      X_GTSELF_GETSCRCHAR( sdl->pGT, sdl->curRow, sdl->curCol, &color, &attr, &ch );
      fg = sdl->colors[color & 0x0F];
      SDL_SetRenderDrawColor( sdl->renderer, fg[0], fg[1], fg[2], SDL_ALPHA_OPAQUE );
      SDL_RenderFillRect( sdl->renderer, &rect );
   }
}

static void s_gt_sdl3_DrawSelection( X_GTSDL3 *sdl )
{
   int           left = GTSDL3_MIN( sdl->selCol, sdl->endCol );
   int           top = GTSDL3_MIN( sdl->selRow, sdl->endRow );
   int           right = GTSDL3_MAX( sdl->selCol, sdl->endCol );
   int           bottom = GTSDL3_MAX( sdl->selRow, sdl->endRow );
   SDL_BlendMode oldBlend = SDL_BLENDMODE_NONE;
   SDL_FRect     rect = { ( float )( left * X_GTSDL3_CELL_WIDTH ), ( float )( top * X_GTSDL3_CELL_HEIGHT ),
                          ( float )( ( right - left + 1 ) * X_GTSDL3_CELL_WIDTH ),
                          ( float )( ( bottom - top + 1 ) * X_GTSDL3_CELL_HEIGHT ) };

   SDL_GetRenderDrawBlendMode( sdl->renderer, &oldBlend );
   SDL_SetRenderDrawBlendMode( sdl->renderer, SDL_BLENDMODE_BLEND );
   SDL_SetRenderDrawColor( sdl->renderer, 64, 128, 255, 112 );
   SDL_RenderFillRect( sdl->renderer, &rect );
   SDL_SetRenderDrawBlendMode( sdl->renderer, oldBlend );
}

/* clears the window with the SetColorExt() background, otherwise the background of the SetColor()
   standard colour, so the area outside the grid is the margin of a terminal */
static void s_gt_sdl3_ClearMargin( X_GTSDL3 *sdl )
{
   uint32_t     back = sdl->pGT->background;
   const Uint8 *bg = sdl->colors[( sdl->pGT->colors[0] >> 4 ) & 0x0F];
   Uint8        red = bg[0], green = bg[1], blue = bg[2];

   if( back != X_GT_RGB_DEFAULT )
   {
      red = ( Uint8 )( back >> 16 );
      green = ( Uint8 )( back >> 8 );
      blue = ( Uint8 )back;
   }
   SDL_SetRenderDrawColor( sdl->renderer, red, green, blue, SDL_ALPHA_OPAQUE );
   SDL_RenderClear( sdl->renderer );
}

static void s_gt_sdl3_Present( X_GTSDL3 *sdl )
{
   SDL_Texture *oldTarget;
   float        width = ( float )( sdl->cols * X_GTSDL3_CELL_WIDTH );
   float        height = ( float )( sdl->rows * X_GTSDL3_CELL_HEIGHT );
   SDL_FRect    rect = { 0.0f, 0.0f, width, height };

   if( sdl->drawing )
      return;

   sdl->drawing = 1;
   s_gt_sdl3_UpdateMetrics( sdl );
   SDL_SetAtomicInt( &sdl->paint, 0 );
   oldTarget = SDL_GetRenderTarget( sdl->renderer );
   SDL_SetRenderTarget( sdl->renderer, NULL );
   s_gt_sdl3_ClearMargin( sdl );

   /* Logical presentation maps display-scaled content to physical output;
      the grid remains 8x16 and NEAREST keeps bitmap glyph edges sharp. */
   SDL_RenderTexture( sdl->renderer, sdl->screen, NULL, &rect );

   if( sdl->selectOn )
      s_gt_sdl3_DrawSelection( sdl );
   else
      s_gt_sdl3_DrawCursor( sdl );

   SDL_RenderPresent( sdl->renderer );
   SDL_SetRenderTarget( sdl->renderer, oldTarget );
   sdl->drawing = 0;
}

static bool SDLCALL s_gt_sdl3_EventWatch( void *cargo, SDL_Event *event )
{
   X_GTSDL3 *sdl = ( X_GTSDL3 * )cargo;
   Uint32    type = event->type;
   int       paint = type == SDL_EVENT_WINDOW_EXPOSED || type == SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED;
   int       resized = type == SDL_EVENT_WINDOW_RESIZED;

   if( type == SDL_EVENT_WINDOW_DISPLAY_SCALE_CHANGED )
      paint = 1;
   if( ( !paint && !resized ) || event->window.windowID != sdl->windowID )
      return true;

   if( paint )
   {
      SDL_SetAtomicInt( &sdl->paint, 1 );

      /* Keep live resizing responsive, but never render from a foreign
         event-producing thread. */
      if( type == SDL_EVENT_WINDOW_EXPOSED && SDL_IsMainThread() )
         s_gt_sdl3_Present( sdl );
   }
   else if( sdl->pGT->resizeBlock && SDL_IsMainThread() )
   {
      int sizeStep = sdl->sizeStep;

      sdl->sizeStep = 0;
      s_gt_sdl3_ResizeFromWindow( sdl, event->window.data1, event->window.data2 );
      sdl->sizeStep = sizeStep;
      s_gt_sdl3_Present( sdl );
   }

   return true;
}

static void s_gt_sdl3_AddKeyEvent( X_GTSDL3 *sdl, int iKey, SDL_Scancode scancode, int repeat )
{
   int iNext = ( sdl->keyIn + 1 ) % X_GTSDL3_KEY_QUEUE_SIZE;

   if( sdl->keyIn != sdl->keyOut )
   {
      int iLast = IIF( sdl->keyIn == 0, X_GTSDL3_KEY_QUEUE_SIZE - 1, sdl->keyIn - 1 );

      if( iKey == K_RESIZE && sdl->keyQueue[iLast] == K_RESIZE )
         return;

      if( X_INKEY_ISMOUSEPOS( iKey ) && X_INKEY_ISMOUSEPOS( sdl->keyQueue[iLast] ) )
      {
         sdl->keyQueue[iLast] = iKey;
         sdl->keyScan[iLast] = scancode;
         sdl->keyRpt[iLast] = IIF( repeat, 1, 0 );
         return;
      }
   }

   if( iNext != sdl->keyOut )
   {
      sdl->keyQueue[sdl->keyIn] = iKey;
      sdl->keyScan[sdl->keyIn] = scancode;
      sdl->keyRpt[sdl->keyIn] = IIF( repeat, 1, 0 );
      sdl->keyIn = iNext;
   }
}

static void s_gt_sdl3_AddKey( X_GTSDL3 *sdl, int iKey )
{
   s_gt_sdl3_AddKeyEvent( sdl, iKey, SDL_SCANCODE_UNKNOWN, 0 );
}

static void s_gt_sdl3_DiscardKeyRepeats( X_GTSDL3 *sdl, SDL_Scancode scancode )
{
   int read = sdl->keyOut;
   int write = sdl->keyOut;

   while( read != sdl->keyIn )
   {
      int next = ( read + 1 ) % X_GTSDL3_KEY_QUEUE_SIZE;

      if( !sdl->keyRpt[read] || ( scancode != SDL_SCANCODE_UNKNOWN && sdl->keyScan[read] != scancode ) )
      {
         if( write != read )
         {
            sdl->keyQueue[write] = sdl->keyQueue[read];
            sdl->keyScan[write] = sdl->keyScan[read];
            sdl->keyRpt[write] = sdl->keyRpt[read];
         }

         write = ( write + 1 ) % X_GTSDL3_KEY_QUEUE_SIZE;
      }

      read = next;
   }

   sdl->keyIn = write;
}

static int s_gt_sdl3_KeyFlags( SDL_Keymod mod, SDL_Scancode scancode )
{
   int flags = 0;

   if( ( mod & SDL_KMOD_SHIFT ) != 0 )
      flags |= KF_SHIFT;
   if( ( mod & SDL_KMOD_CTRL ) != 0 )
      flags |= KF_CTRL;
   if( ( mod & SDL_KMOD_ALT ) != 0 )
      flags |= KF_ALT;
   if( scancode >= SDL_SCANCODE_KP_DIVIDE && scancode <= SDL_SCANCODE_KP_PERIOD )
      flags |= KF_KEYPAD;
   if( scancode == SDL_SCANCODE_KP_COMMA )
      flags |= KF_KEYPAD;

   return flags;
}

static int s_gt_sdl3_KbdShifts( void )
{
   SDL_Keymod mod = SDL_GetModState();
   int        shifts = 0;

   if( ( mod & SDL_KMOD_SHIFT ) != 0 )
      shifts |= X_GTI_KBD_SHIFT;
   if( ( mod & SDL_KMOD_CTRL ) != 0 )
      shifts |= X_GTI_KBD_CTRL;
   if( ( mod & SDL_KMOD_ALT ) != 0 )
      shifts |= X_GTI_KBD_ALT;
   if( ( mod & SDL_KMOD_LSHIFT ) != 0 )
      shifts |= X_GTI_KBD_LSHIFT;
   if( ( mod & SDL_KMOD_RSHIFT ) != 0 )
      shifts |= X_GTI_KBD_RSHIFT;
   if( ( mod & SDL_KMOD_LCTRL ) != 0 )
      shifts |= X_GTI_KBD_LCTRL;
   if( ( mod & SDL_KMOD_RCTRL ) != 0 )
      shifts |= X_GTI_KBD_RCTRL;
   if( ( mod & SDL_KMOD_LALT ) != 0 )
      shifts |= X_GTI_KBD_LALT;
   if( ( mod & SDL_KMOD_RALT ) != 0 )
      shifts |= X_GTI_KBD_RALT;
   if( ( mod & SDL_KMOD_LGUI ) != 0 )
      shifts |= X_GTI_KBD_LWIN;
   if( ( mod & SDL_KMOD_RGUI ) != 0 )
      shifts |= X_GTI_KBD_RWIN;
   if( ( mod & SDL_KMOD_SCROLL ) != 0 )
      shifts |= X_GTI_KBD_SCROLOCK;
   if( ( mod & SDL_KMOD_NUM ) != 0 )
      shifts |= X_GTI_KBD_NUMLOCK;
   if( ( mod & SDL_KMOD_CAPS ) != 0 )
      shifts |= X_GTI_KBD_CAPSLOCK;

   return shifts;
}

static int s_gt_sdl3_KeypadKey( SDL_Scancode scancode, SDL_Keymod mod, int *isText )
{
   int ch, key;

   switch( scancode )
   {
      case SDL_SCANCODE_KP_DIVIDE: *isText = 1; return '/';
      case SDL_SCANCODE_KP_MULTIPLY: *isText = 1; return '*';
      case SDL_SCANCODE_KP_MINUS: *isText = 1; return '-';
      case SDL_SCANCODE_KP_PLUS: *isText = 1; return '+';
      case SDL_SCANCODE_KP_COMMA: *isText = 1; return ',';
      case SDL_SCANCODE_KP_1:
         ch = '1';
         key = KX_END;
         break;
      case SDL_SCANCODE_KP_2:
         ch = '2';
         key = KX_DOWN;
         break;
      case SDL_SCANCODE_KP_3:
         ch = '3';
         key = KX_PGDN;
         break;
      case SDL_SCANCODE_KP_4:
         ch = '4';
         key = KX_LEFT;
         break;
      case SDL_SCANCODE_KP_5:
         ch = '5';
         key = KX_CENTER;
         break;
      case SDL_SCANCODE_KP_6:
         ch = '6';
         key = KX_RIGHT;
         break;
      case SDL_SCANCODE_KP_7:
         ch = '7';
         key = KX_HOME;
         break;
      case SDL_SCANCODE_KP_8:
         ch = '8';
         key = KX_UP;
         break;
      case SDL_SCANCODE_KP_9:
         ch = '9';
         key = KX_PGUP;
         break;
      case SDL_SCANCODE_KP_0:
         ch = '0';
         key = KX_INS;
         break;
      case SDL_SCANCODE_KP_PERIOD:
         ch = '.';
         key = KX_DEL;
         break;
      default: return 0;
   }

   if( ( mod & SDL_KMOD_NUM ) != 0 )
   {
      *isText = 1;
      return ch;
   }

   return key;
}

static int s_gt_sdl3_SpecialKey( SDL_Scancode scancode )
{
   switch( scancode )
   {
      case SDL_SCANCODE_ESCAPE: return KX_ESC;
      case SDL_SCANCODE_RETURN:
      case SDL_SCANCODE_KP_ENTER: return KX_ENTER;
      case SDL_SCANCODE_BACKSPACE: return KX_BS;
      case SDL_SCANCODE_TAB: return KX_TAB;
      case SDL_SCANCODE_UP: return KX_UP;
      case SDL_SCANCODE_DOWN: return KX_DOWN;
      case SDL_SCANCODE_LEFT: return KX_LEFT;
      case SDL_SCANCODE_RIGHT: return KX_RIGHT;
      case SDL_SCANCODE_HOME: return KX_HOME;
      case SDL_SCANCODE_END: return KX_END;
      case SDL_SCANCODE_PAGEUP: return KX_PGUP;
      case SDL_SCANCODE_PAGEDOWN: return KX_PGDN;
      case SDL_SCANCODE_INSERT: return KX_INS;
      case SDL_SCANCODE_DELETE: return KX_DEL;
      case SDL_SCANCODE_F1: return KX_F1;
      case SDL_SCANCODE_F2: return KX_F2;
      case SDL_SCANCODE_F3: return KX_F3;
      case SDL_SCANCODE_F4: return KX_F4;
      case SDL_SCANCODE_F5: return KX_F5;
      case SDL_SCANCODE_F6: return KX_F6;
      case SDL_SCANCODE_F7: return KX_F7;
      case SDL_SCANCODE_F8: return KX_F8;
      case SDL_SCANCODE_F9: return KX_F9;
      case SDL_SCANCODE_F10: return KX_F10;
      case SDL_SCANCODE_F11: return KX_F11;
      case SDL_SCANCODE_F12: return KX_F12;
      default: return 0;
   }
}

static Uint32 s_gt_sdl3_MouseButtonMask( int button )
{
   return IIF( button >= X_MBUTTON_LEFT && button <= X_MBUTTON_MIDDLE, ( Uint32 )1 << button, 0 );
}

static int s_gt_sdl3_MouseButtonIndex( Uint8 button )
{
   switch( button )
   {
      case SDL_BUTTON_LEFT: return X_MBUTTON_LEFT;
      case SDL_BUTTON_RIGHT: return X_MBUTTON_RIGHT;
      case SDL_BUTTON_MIDDLE: return X_MBUTTON_MIDDLE;
      default: return -1;
   }
}

static void s_gt_sdl3_UpdateMousePosition( X_GTSDL3 *sdl, float x, float y, int addEvent )
{
   int mouseX = ( int )x;
   int mouseY = ( int )y;
   int col = mouseX / X_GTSDL3_CELL_WIDTH;
   int row = mouseY / X_GTSDL3_CELL_HEIGHT;
   int pixelChg = mouseX != sdl->mouseX || mouseY != sdl->mouseY;

   sdl->mouseX = mouseX;
   sdl->mouseY = mouseY;
   col = GTSDL3_MAX( 0, GTSDL3_MIN( col, sdl->cols - 1 ) );
   row = GTSDL3_MAX( 0, GTSDL3_MIN( row, sdl->rows - 1 ) );

   sdl->mouseRow = row;
   sdl->mouseCol = col;
   /* Keep terminal coordinates cell-based, but wake pixel consumers for
      every logical-pixel change. Consecutive MPOS events are coalesced by
      s_gt_sdl3_AddKeyEvent(). */
   if( addEvent && pixelChg )
      s_gt_sdl3_AddKey( sdl, X_INKEY_NEW_MPOS( col, row ) );
}

static int s_gt_sdl3_MouseButtonKey( int button, int down, int dblClick )
{
   switch( button )
   {
      case X_MBUTTON_LEFT:
         if( !down )
            return K_LBUTTONUP;

         return IIF( dblClick, K_LDBLCLK, K_LBUTTONDOWN );

      case X_MBUTTON_RIGHT:
         if( !down )
            return K_RBUTTONUP;

         return IIF( dblClick, K_RDBLCLK, K_RBUTTONDOWN );

      case X_MBUTTON_MIDDLE:
         if( !down )
            return K_MBUTTONUP;

         return IIF( dblClick, K_MDBLCLK, K_MBUTTONDOWN );
   }

   return 0;
}

static void s_gt_sdl3_AddWheelEvents( X_GTSDL3 *sdl, const SDL_MouseWheelEvent *wheel )
{
   float x = wheel->x;
   float y = wheel->y;
   int   flags = s_gt_sdl3_KeyFlags( SDL_GetModState(), SDL_SCANCODE_UNKNOWN );

   if( wheel->direction == SDL_MOUSEWHEEL_FLIPPED )
   {
      x = -x;
      y = -y;
   }

   sdl->wheelX += x;
   sdl->wheelY += y;

   while( sdl->wheelY >= 1.0f )
   {
      s_gt_sdl3_AddKey( sdl, X_INKEY_NEW_MKEY( K_MWFORWARD, flags ) );
      sdl->wheelY -= 1.0f;
   }

   while( sdl->wheelY <= -1.0f )
   {
      s_gt_sdl3_AddKey( sdl, X_INKEY_NEW_MKEY( K_MWBACKWARD, flags ) );
      sdl->wheelY += 1.0f;
   }

   while( sdl->wheelX >= 1.0f )
   {
      s_gt_sdl3_AddKey( sdl, X_INKEY_NEW_MKEY( K_MWRIGHT, flags ) );
      sdl->wheelX -= 1.0f;
   }

   while( sdl->wheelX <= -1.0f )
   {
      s_gt_sdl3_AddKey( sdl, X_INKEY_NEW_MKEY( K_MWLEFT, flags ) );
      sdl->wheelX += 1.0f;
   }
}

static void s_gt_sdl3_UpdateSelection( X_GTSDL3 *sdl )
{
   if( sdl->selectOn && ( sdl->endRow != sdl->mouseRow || sdl->endCol != sdl->mouseCol ) )
   {
      sdl->endRow = sdl->mouseRow;
      sdl->endCol = sdl->mouseCol;
      SDL_SetAtomicInt( &sdl->paint, 1 );
   }
}

static void s_gt_sdl3_ClearSelection( X_GTSDL3 *sdl, int skipUp )
{
   if( sdl->selectOn )
   {
      sdl->selectOn = 0;
      sdl->skipUp = skipUp;
      sdl->buttons &= ~s_gt_sdl3_MouseButtonMask( X_MBUTTON_LEFT );
      sdl->btnTime[X_MBUTTON_LEFT] = 0;
      SDL_CaptureMouse( false );
      SDL_SetAtomicInt( &sdl->paint, 1 );
   }
}

static void s_gt_sdl3_CopySelection( X_GTSDL3 *sdl )
{
   int         left = GTSDL3_MIN( sdl->selCol, sdl->endCol );
   int         top = GTSDL3_MIN( sdl->selRow, sdl->endRow );
   int         right = GTSDL3_MAX( sdl->selCol, sdl->endCol );
   int         bottom = GTSDL3_MAX( sdl->selRow, sdl->endRow );
   X_SIZE      rowCount = ( X_SIZE )( bottom - top + 1 );
   X_SIZE      colCount = ( X_SIZE )( right - left + 1 );
   const char *eol = x_eol();
   X_SIZE      eolLen = IIF( rowCount > 1, ( X_SIZE )SDL_strlen( eol ), 0 );
   X_SIZE      rowSize;
   X_SIZE      bufSize;
   X_SIZE      length = 0;
   char       *buffer;
   int         row;

   rowSize = colCount * 3 + eolLen;
   bufSize = rowCount * rowSize;
   buffer = ( char * )x_xgrab( bufSize + 1 );

   for( row = top; row <= bottom; ++row )
   {
      int col;

      for( col = left; col <= right; ++col )
      {
         int      color;
         X_BYTE   attr;
         X_USHORT ch;

         X_GTSELF_GETSCRCHAR( sdl->pGT, row, col, &color, &attr, &ch );
         length += x_utf8Encode( buffer + length, x_u16Ctrl( ch ) );
      }

      if( eolLen > 0 )
      {
         SDL_memcpy( buffer + length, eol, eolLen );
         length += eolLen;
      }
   }

   buffer[length] = '\0';

   SDL_SetClipboardText( buffer );
   SDL_SetPrimarySelectionText( buffer );
   x_xfree( buffer );
}

static void s_gt_sdl3_ScaledWindowSize( X_GTSDL3 *sdl, int logWidth, int logHgt, int *width, int *height )
{
   *width = ( int )SDL_ceil( logWidth * ( double )sdl->scale );
   *height = ( int )SDL_ceil( logHgt * ( double )sdl->scale );
}

static void s_gt_sdl3_SetWindowSizeForGrid( X_GTSDL3 *sdl, int rows, int cols )
{
   int width, height;

   s_gt_sdl3_ScaledWindowSize( sdl, cols * X_GTSDL3_CELL_WIDTH, rows * X_GTSDL3_CELL_HEIGHT, &width, &height );

   if( width != sdl->winWidth || height != sdl->winHgt )
   {
      SDL_SetWindowSize( sdl->window, width, height );
      SDL_SyncWindow( sdl->window );
   }

   s_gt_sdl3_UpdateMetrics( sdl );
}

static void s_gt_sdl3_UpdateSizeLimits( X_GTSDL3 *sdl )
{
   int width, height;

   s_gt_sdl3_ScaledWindowSize( sdl, X_GTSDL3_CELL_WIDTH, X_GTSDL3_CELL_HEIGHT, &width, &height );
   SDL_SetWindowMinimumSize( sdl->window, width, height );
}

static void s_gt_sdl3_ResizeGrid( X_GTSDL3 *sdl, int rows, int cols, int sizeWin )
{
   if( sizeWin )
      s_gt_sdl3_SetWindowSizeForGrid( sdl, rows, cols );
   if( rows == sdl->rows && cols == sdl->cols )
      return;

   SDL_DestroyTexture( sdl->screen );
   sdl->screen = s_gt_sdl3_CreateScreenTexture( sdl, rows, cols );
   X_GTSELF_RESIZE( sdl->pGT, rows, cols );

   sdl->rows = rows;
   sdl->cols = cols;
   sdl->clipTop = 0;
   sdl->clipLeft = 0;
   sdl->clipBtm = rows * X_GTSDL3_CELL_HEIGHT - 1;
   sdl->clipRgt = cols * X_GTSDL3_CELL_WIDTH - 1;
   sdl->clipOn = 1;
   sdl->mouseRow = GTSDL3_MIN( sdl->mouseRow, rows - 1 );
   sdl->mouseCol = GTSDL3_MIN( sdl->mouseCol, cols - 1 );

   if( sdl->selectOn )
   {
      sdl->selRow = GTSDL3_MIN( sdl->selRow, rows - 1 );
      sdl->selCol = GTSDL3_MIN( sdl->selCol, cols - 1 );
      sdl->endRow = GTSDL3_MIN( sdl->endRow, rows - 1 );
      sdl->endCol = GTSDL3_MIN( sdl->endCol, cols - 1 );
   }

   sdl->resized = 1;
   X_GTSELF_EXPOSEAREA( sdl->pGT, 0, 0, rows - 1, cols - 1 );
}

static void s_gt_sdl3_ResizeFromWindow( X_GTSDL3 *sdl, int width, int height )
{
   SDL_WindowFlags winFlags;
   int             rows;
   int             cols;
   int             changed;

   if( width <= 0 || height <= 0 )
      return;

   SDL_SetAtomicInt( &sdl->paint, 1 );
   s_gt_sdl3_UpdateMetrics( sdl );
   rows = GTSDL3_MAX( sdl->logHgt / X_GTSDL3_CELL_HEIGHT, 1 );
   cols = GTSDL3_MAX( sdl->logWidth / X_GTSDL3_CELL_WIDTH, 1 );
   changed = rows != sdl->rows || cols != sdl->cols;

   if( changed )
   {
      s_gt_sdl3_ResizeGrid( sdl, rows, cols, 0 );
      s_gt_sdl3_AddKey( sdl, K_RESIZE );
      x_gtResizeNotify( sdl->pGT );
   }

   winFlags = SDL_GetWindowFlags( sdl->window );

   if( sdl->sizeStep && ( winFlags & ( SDL_WINDOW_FULLSCREEN | SDL_WINDOW_MAXIMIZED ) ) == 0 )
      s_gt_sdl3_SetWindowSizeForGrid( sdl, sdl->rows, sdl->cols );
}

static void s_gt_sdl3_DisplayScaleChanged( X_GTSDL3 *sdl )
{
   s_gt_sdl3_UpdateMetrics( sdl );
   s_gt_sdl3_UpdateSizeLimits( sdl );
   s_gt_sdl3_SetWindowSizeForGrid( sdl, sdl->rows, sdl->cols );
   SDL_SetAtomicInt( &sdl->paint, 1 );
}

static void s_gt_sdl3_CloseRequest( X_GTSDL3 *sdl )
{
   switch( sdl->close )
   {
      case 0: x_quitRaise(); break;
      case 1: s_gt_sdl3_AddKey( sdl, K_CLOSE ); break;
      case 2: break;
   }
}

static SDL_WindowID s_gt_sdl3_EventWindowID( const SDL_Event *event )
{
   switch( event->type )
   {
      case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
      case SDL_EVENT_WINDOW_EXPOSED:
      case SDL_EVENT_WINDOW_RESIZED:
      case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
      case SDL_EVENT_WINDOW_DISPLAY_SCALE_CHANGED:
      case SDL_EVENT_WINDOW_FOCUS_LOST:
      case SDL_EVENT_WINDOW_FOCUS_GAINED:
      case SDL_EVENT_WINDOW_MOUSE_LEAVE: return event->window.windowID;

      case SDL_EVENT_KEY_DOWN:
      case SDL_EVENT_KEY_UP: return event->key.windowID;

      case SDL_EVENT_TEXT_INPUT: return event->text.windowID;

      case SDL_EVENT_MOUSE_MOTION: return event->motion.windowID;

      case SDL_EVENT_MOUSE_BUTTON_DOWN:
      case SDL_EVENT_MOUSE_BUTTON_UP: return event->button.windowID;

      case SDL_EVENT_MOUSE_WHEEL: return event->wheel.windowID;
   }

   return 0;
}

static void s_gt_sdl3_DispatchEvent( X_GTSDL3 *sdl, SDL_Event *event )
{
   int mouseUsed = sdl->mouseOn || sdl->copyOn || sdl->selectOn || sdl->skipUp;

   if( event->type == SDL_EVENT_WINDOW_CLOSE_REQUESTED )
      s_gt_sdl3_CloseRequest( sdl );
   else if( event->type == SDL_EVENT_WINDOW_EXPOSED )
      SDL_SetAtomicInt( &sdl->paint, 1 );
   else if( event->type == SDL_EVENT_WINDOW_RESIZED )
   {
      sdl->newWidth = event->window.data1;
      sdl->newHgt = event->window.data2;
   }
   else if( event->type == SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED )
   {
      sdl->outWidth = event->window.data1;
      sdl->outHgt = event->window.data2;
      sdl->pixelChg = 1;
      SDL_SetAtomicInt( &sdl->paint, 1 );
   }
   else if( event->type == SDL_EVENT_WINDOW_DISPLAY_SCALE_CHANGED )
   {
      sdl->scaleChg = 1;
      SDL_SetAtomicInt( &sdl->paint, 1 );
   }
   else if( event->type == SDL_EVENT_WINDOW_MOUSE_LEAVE && sdl->mouseOn )
      s_gt_sdl3_AddKey( sdl, K_NCMOUSEMOVE );
   else if( event->type == SDL_EVENT_MOUSE_MOTION && ( sdl->mouseOn || sdl->selectOn ) )
   {
      SDL_ConvertEventToRenderCoordinates( sdl->renderer, event );
      s_gt_sdl3_UpdateMousePosition( sdl, event->motion.x, event->motion.y, sdl->mouseOn && !sdl->selectOn );
      s_gt_sdl3_UpdateSelection( sdl );
   }
   else if( ( event->type == SDL_EVENT_MOUSE_BUTTON_DOWN || event->type == SDL_EVENT_MOUSE_BUTTON_UP ) && mouseUsed )
   {
      int        button = s_gt_sdl3_MouseButtonIndex( event->button.button );
      int        down = event->type == SDL_EVENT_MOUSE_BUTTON_DOWN;
      SDL_Keymod mods = SDL_GetModState();
      int        selStart = 0;
      int        moveEvent;
      int        dblClick = 0;
      Uint32     mask;
      int        key;

      if( down && button == X_MBUTTON_LEFT && sdl->copyOn && !sdl->selectOn )
         selStart = ( mods & SDL_KMOD_SHIFT ) != 0;
      moveEvent = sdl->mouseOn && !sdl->selectOn && !selStart;
      SDL_ConvertEventToRenderCoordinates( sdl->renderer, event );
      s_gt_sdl3_UpdateMousePosition( sdl, event->button.x, event->button.y, moveEvent );

      if( button == X_MBUTTON_LEFT )
      {
         if( down )
         {
            sdl->skipUp = 0;

            if( selStart )
            {
               sdl->selectOn = 1;
               sdl->selRow = sdl->endRow = sdl->mouseRow;
               sdl->selCol = sdl->endCol = sdl->mouseCol;
               sdl->buttons &= ~s_gt_sdl3_MouseButtonMask( X_MBUTTON_LEFT );
               sdl->btnTime[X_MBUTTON_LEFT] = 0;
               SDL_CaptureMouse( true );
               SDL_SetAtomicInt( &sdl->paint, 1 );
               return;
            }
         }
         else if( sdl->selectOn )
         {
            s_gt_sdl3_CopySelection( sdl );
            sdl->selectOn = 0;
            sdl->buttons &= ~s_gt_sdl3_MouseButtonMask( X_MBUTTON_LEFT );
            sdl->btnTime[X_MBUTTON_LEFT] = 0;
            SDL_CaptureMouse( false );
            SDL_SetAtomicInt( &sdl->paint, 1 );
            return;
         }
         else if( sdl->skipUp )
         {
            sdl->skipUp = 0;
            return;
         }
      }

      if( button < 0 || !sdl->mouseOn )
         return;

      mask = s_gt_sdl3_MouseButtonMask( button );

      if( down )
      {
         Uint64 now = SDL_NS_TO_MS( event->button.timestamp );
         int    speed = X_GTSELF_MOUSEGETDOUBLECLICKSPEED( sdl->pGT );

         dblClick = sdl->btnTime[button] != 0 && now - sdl->btnTime[button] <= ( Uint64 )speed;
         sdl->btnTime[button] = now;
         sdl->buttons |= mask;
      }
      else
         sdl->buttons &= ~mask;

      key = s_gt_sdl3_MouseButtonKey( button, down, dblClick );
      s_gt_sdl3_AddKey( sdl, X_INKEY_NEW_MKEY( key, s_gt_sdl3_KeyFlags( mods, SDL_SCANCODE_UNKNOWN ) ) );
   }
   else if( event->type == SDL_EVENT_MOUSE_WHEEL && sdl->mouseOn )
   {
      SDL_ConvertEventToRenderCoordinates( sdl->renderer, event );
      s_gt_sdl3_UpdateMousePosition( sdl, event->wheel.mouse_x, event->wheel.mouse_y, 1 );
      s_gt_sdl3_AddWheelEvents( sdl, &event->wheel );
   }
   else if( event->type == SDL_EVENT_KEY_DOWN )
   {
      int key = s_gt_sdl3_SpecialKey( event->key.scancode );
      int flags = s_gt_sdl3_KeyFlags( event->key.mod, event->key.scancode );
      int isText = 0;
      int altEnter;

      altEnter = sdl->altEnter && !event->key.repeat && event->key.scancode == SDL_SCANCODE_RETURN;
      if( ( event->key.mod & SDL_KMOD_ALT ) == 0 || ( event->key.mod & SDL_KMOD_CTRL ) != 0 )
         altEnter = 0;
      if( altEnter )
      {
         SDL_WindowFlags winFlags = SDL_GetWindowFlags( sdl->window );

         SDL_SetWindowFullscreen( sdl->window, ( winFlags & SDL_WINDOW_FULLSCREEN ) == 0 );
         SDL_SyncWindow( sdl->window );
         sdl->textScan = SDL_SCANCODE_UNKNOWN;
         sdl->textFlag = 0;
         sdl->textRpt = 0;
         return;
      }

      if( key == 0 && ( flags & KF_KEYPAD ) != 0 )
         key = s_gt_sdl3_KeypadKey( event->key.scancode, event->key.mod, &isText );

      if( ( isText || key == 0 ) && ( flags & ( KF_CTRL | KF_ALT ) ) == 0 )
      {
         sdl->textScan = event->key.scancode;
         sdl->textFlag = IIF( isText, flags, flags & KF_SHIFT );
         sdl->textRpt = event->key.repeat;
      }
      else if( key != 0 )
         s_gt_sdl3_AddKeyEvent( sdl, X_INKEY_NEW_KEY( key, flags ), event->key.scancode, event->key.repeat );
      else if( ( flags & ( KF_CTRL | KF_ALT ) ) != ( KF_CTRL | KF_ALT ) )
      {
         SDL_Keycode keycode = event->key.key;

         if( keycode >= 'a' && keycode <= 'z' )
            keycode -= 'a' - 'A';

         if( keycode > 0 && keycode < 128 )
         {
            int keyEvent = X_INKEY_NEW_KEY( ( int )keycode, flags );

            s_gt_sdl3_AddKeyEvent( sdl, keyEvent, event->key.scancode, event->key.repeat );
         }
      }
      else
      {
         sdl->textScan = event->key.scancode;
         sdl->textFlag = 0;
         sdl->textRpt = event->key.repeat;
      }
   }
   else if( event->type == SDL_EVENT_KEY_UP )
   {
      s_gt_sdl3_DiscardKeyRepeats( sdl, event->key.scancode );

      if( event->key.scancode == sdl->textScan )
      {
         sdl->textScan = SDL_SCANCODE_UNKNOWN;
         sdl->textFlag = 0;
         sdl->textRpt = 0;
      }
   }
   else if( event->type == SDL_EVENT_WINDOW_FOCUS_LOST )
   {
      s_gt_sdl3_ClearSelection( sdl, 1 );
      sdl->textScan = SDL_SCANCODE_UNKNOWN;
      sdl->textFlag = 0;
      sdl->textRpt = 0;
      s_gt_sdl3_DiscardKeyRepeats( sdl, SDL_SCANCODE_UNKNOWN );
      sdl->buttons = 0;
      sdl->btnTime[0] = sdl->btnTime[1] = sdl->btnTime[2] = 0;
      sdl->wheelX = sdl->wheelY = 0.0f;
      s_gt_sdl3_AddKey( sdl, K_LOSTFOCUS );
   }
   else if( event->type == SDL_EVENT_WINDOW_FOCUS_GAINED )
   {
      s_gt_sdl3_AddKey( sdl, K_GOTFOCUS );
   }
   else if( event->type == SDL_EVENT_TEXT_INPUT )
   {
      const char  *text = event->text.text;
      size_t       length = SDL_strlen( text );
      SDL_Scancode textScan = sdl->textScan;
      int          textFlag = sdl->textFlag;
      int          textRpt = sdl->textRpt;

      sdl->textScan = SDL_SCANCODE_UNKNOWN;
      sdl->textFlag = 0;
      sdl->textRpt = 0;

      while( length > 0 )
      {
         Uint32 ch = SDL_StepUTF8( &text, &length );

         if( ch > 0 )
         {
            /* Extended input stores one Unicode value in 16 bits. */
            if( ch > X_INKEY_EXT_VALMASK )
               ch = SDL_INVALID_UNICODE_CODEPOINT;

            s_gt_sdl3_AddKeyEvent( sdl, X_INKEY_NEW_UNICODEF( ( int )ch, textFlag ), textScan, textRpt );
            textFlag = 0;
         }
      }
   }
}

static void s_gt_sdl3_FinishEvents( X_GTSDL3 *sdl )
{
   int newWidth = sdl->newWidth;
   int newHgt = sdl->newHgt;
   int pixelChg = sdl->pixelChg;
   int scaleChg = sdl->scaleChg;

   sdl->newWidth = 0;
   sdl->newHgt = 0;
   sdl->pixelChg = 0;
   sdl->scaleChg = 0;

   if( scaleChg )
      s_gt_sdl3_DisplayScaleChanged( sdl );
   else if( newWidth > 0 && newHgt > 0 )
      s_gt_sdl3_ResizeFromWindow( sdl, newWidth, newHgt );
   else if( pixelChg )
      s_gt_sdl3_ResizeFromWindow( sdl, sdl->winWidth, sdl->winHgt );
}

static void s_gt_sdl3_PumpEvents( void )
{
   SDL_Event event;
   X_GTSDL3 *sdl;

   if( s_pumpingEvents )
      return;

   s_pumpingEvents = 1;
   x_callbackEnterDll();

   while( SDL_PollEvent( &event ) )
   {
      if( event.type == SDL_EVENT_QUIT )
         x_quitRaise();
      else
      {
         SDL_WindowID windowID = s_gt_sdl3_EventWindowID( &event );

         if( windowID != 0 && ( sdl = s_gt_sdl3_FindByWindowID( windowID ) ) != NULL )
            s_gt_sdl3_DispatchEvent( sdl, &event );
      }
   }

   sdl = s_windows;

   while( sdl )
   {
      X_GTSDL3 *next = sdl->next;

      s_gt_sdl3_FinishEvents( sdl );
      sdl = next;
   }

   x_callbackLeaveDll();
   s_pumpingEvents = 0;
   x_callbackRaisePending();
}

static void s_gt_sdl3_Init( X_GT_BASE *pGT, X_FHANDLE hIn, X_FHANDLE hOut, X_FHANDLE hErr )
{
   X_GTSDL3       *sdl;
   const char     *driver = NULL;
   SDL_WindowFlags windowFlags = SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY;

   sdl = ( X_GTSDL3 * )x_xgrabz( sizeof( X_GTSDL3 ) );

   sdl->pGT = pGT;
   sdl->rows = X_GTSDL3_DEFAULT_ROWS;
   sdl->cols = X_GTSDL3_DEFAULT_COLS;
   sdl->winWidth = sdl->cols * X_GTSDL3_CELL_WIDTH;
   sdl->winHgt = sdl->rows * X_GTSDL3_CELL_HEIGHT;
   sdl->titleHandle = x_strdup( "xDev" );
   sdl->title = ( const char * )sdl->titleHandle;

   SDL_memcpy( sdl->colors, s_defaultColors, sizeof( sdl->colors ) );
   X_GTLOCAL( pGT ) = sdl;
   s_super.Init( pGT, hIn, hOut, hErr );
   X_GTSELF_SETFLAG( pGT, X_GTI_COMPATBUFFER, 0 );

   SDL_InitSubSystem( SDL_INIT_VIDEO );
   SDL_SetHint( SDL_HINT_QUIT_ON_LAST_WINDOW_CLOSE, "0" );

   sdl->curRow = -1;
   sdl->curCol = -1;
   sdl->curStyle = SC_NONE;
   sdl->blink = X_GT_CURSOR_BLINK;
   sdl->curOn = 1;
   sdl->curTime = SDL_GetTicks();
   sdl->mouseOn = 1;
   sdl->canSize = 1;
   sdl->sizeStep = 1;
   sdl->clipBtm = sdl->rows * X_GTSDL3_CELL_HEIGHT - 1;
   sdl->clipRgt = sdl->cols * X_GTSDL3_CELL_WIDTH - 1;
   sdl->clipOn = 1;

   sdl->window = SDL_CreateWindow( sdl->title, sdl->winWidth, sdl->winHgt, windowFlags );
   sdl->windowID = SDL_GetWindowID( sdl->window );

#if X_OS_WINDOWS
   driver = "software";
#endif
   sdl->renderer = SDL_CreateRenderer( sdl->window, driver );
   s_gt_sdl3_UpdateMetrics( sdl );
   s_gt_sdl3_SetWindowSizeForGrid( sdl, sdl->rows, sdl->cols );
   s_gt_sdl3_UpdateSizeLimits( sdl );
   s_gt_sdl3_CreateFontTexture( sdl );

   sdl->screen = s_gt_sdl3_CreateScreenTexture( sdl, sdl->rows, sdl->cols );

   SDL_AddEventWatch( s_gt_sdl3_EventWatch, sdl );
   SDL_StartTextInput( sdl->window );
   X_GTSELF_RESIZE( pGT, sdl->rows, sdl->cols );
   sdl->next = s_windows;
   s_windows = sdl;
   X_GTSELF_SETFLAG( pGT, X_GTI_REDRAWMAX, 1 );
   X_GTSELF_SEMICOLD( pGT );
}

static void s_gt_sdl3_Exit( X_GT_BASE *pGT )
{
   X_GTSDL3  *sdl = X_GTSDL3_GET( pGT );
   X_GTSDL3 **current = &s_windows;

   SDL_RemoveEventWatch( s_gt_sdl3_EventWatch, sdl );

   while( *current != sdl )
      current = &( *current )->next;
   *current = sdl->next;

   if( sdl->selectOn )
      SDL_CaptureMouse( false );

   SDL_StopTextInput( sdl->window );

   if( sdl->gfxOn )
      s_gt_sdl3_EndGfx( sdl, &sdl->gfxState );

   SDL_DestroyTexture( sdl->screen );
   SDL_DestroyTexture( sdl->font );
   SDL_DestroyRenderer( sdl->renderer );
   SDL_DestroyWindow( sdl->window );
   SDL_DestroySurface( sdl->icon );
   if( sdl->sound )
      SDL_DestroyAudioStream( sdl->sound );
   if( sdl->audioOn )
      SDL_QuitSubSystem( SDL_INIT_AUDIO );

   x_strfree( sdl->titleHandle );
   SDL_QuitSubSystem( SDL_INIT_VIDEO );
   X_GTLOCAL( pGT ) = NULL;
   x_xfree( sdl );
   s_super.Exit( pGT );
}

static int s_gt_sdl3_SetMode( X_GT_BASE *pGT, int iRows, int iCols )
{
   X_GTSDL3 *sdl = X_GTSDL3_GET( pGT );
   int       resume;
   int       changed;

   resume = sdl->gfxOn;

   if( resume )
   {
      s_gt_sdl3_EndGfx( sdl, &sdl->gfxState );
      sdl->gfxOn = 0;
   }

   changed = iRows != sdl->rows || iCols != sdl->cols;
   s_gt_sdl3_ResizeGrid( sdl, iRows, iCols, 1 );

   if( changed )
      s_gt_sdl3_AddKey( sdl, K_RESIZE );
   if( resume )
   {
      s_gt_sdl3_BeginGfx( sdl, &sdl->gfxState, 0 );
      sdl->gfxOn = 1;
   }

   return 1;
}

static void s_gt_sdl3_Redraw( X_GT_BASE *pGT, int row, int col, int size )
{
   X_GTSDL3    *sdl = X_GTSDL3_GET( pGT );
   SDL_Texture *target = SDL_GetRenderTarget( sdl->renderer );

   SDL_SetRenderTarget( sdl->renderer, sdl->screen );

   while( size-- > 0 )
   {
      int              color;
      X_BYTE           attr;
      X_USHORT         ch;
      const Uint8     *bg;
      Uint8            fgRed, fgGreen, fgBlue;
      Uint8            bgRed, bgGreen, bgBlue;
      const X_GT_CELL *cell;
      SDL_FRect        rect = { ( float )( col * X_GTSDL3_CELL_WIDTH ), ( float )( row * X_GTSDL3_CELL_HEIGHT ),
                                X_GTSDL3_CELL_WIDTH, X_GTSDL3_CELL_HEIGHT };

      X_GTSELF_GETSCRCHAR( pGT, row, col, &color, &attr, &ch );
      cell = &pGT->screenBuffer[( size_t )row * pGT->cols + col];
      if( cell->foreground != X_GT_RGB_DEFAULT )
      {
         fgRed = ( Uint8 )( cell->foreground >> 16 );
         fgGreen = ( Uint8 )( cell->foreground >> 8 );
         fgBlue = ( Uint8 )cell->foreground;
         bgRed = ( Uint8 )( cell->background >> 16 );
         bgGreen = ( Uint8 )( cell->background >> 8 );
         bgBlue = ( Uint8 )cell->background;
      }
      else
      {
         bg = sdl->colors[( color >> 4 ) & 0x0F];
         bgRed = bg[0];
         bgGreen = bg[1];
         bgBlue = bg[2];
         bg = sdl->colors[color & 0x0F];
         fgRed = bg[0];
         fgGreen = bg[1];
         fgBlue = bg[2];
      }
      SDL_SetRenderDrawColor( sdl->renderer, bgRed, bgGreen, bgBlue, SDL_ALPHA_OPAQUE );
      SDL_RenderFillRect( sdl->renderer, &rect );
      ch = x_u16Ctrl( ch );

      if( ch != ' ' && ch != 0x00A0 )
      {
         Uint32 fg = ( Uint32 )fgRed << 16 | ( Uint32 )fgGreen << 8 | fgBlue;
         int    top = row * X_GTSDL3_CELL_HEIGHT;
         int    left = col * X_GTSDL3_CELL_WIDTH;

         if( sdl->fgColor != fg )
         {
            SDL_SetTextureColorMod( sdl->font, fgRed, fgGreen, fgBlue );
            sdl->fgColor = fg;
         }

         s_gt_sdl3_DrawGlyph( sdl, ch, top, left, X_GTSDL3_CELL_HEIGHT, X_GTSDL3_CELL_WIDTH );
      }

      ++col;
   }

   SDL_SetRenderTarget( sdl->renderer, target );
}

static int s_gt_sdl3_TrueColor( X_GT_BASE *pGT )
{
   X_SYMBOL_UNUSED( pGT );

   return 1;
}

static void s_gt_sdl3_Refresh( X_GT_BASE *pGT )
{
   X_GTSDL3 *sdl = X_GTSDL3_GET( pGT );

   s_gt_sdl3_PumpEvents();
   s_super.Refresh( pGT );
   sdl->resized = 0;
   s_gt_sdl3_UpdateCursor( sdl, 0 );
   s_gt_sdl3_Present( sdl );
}

static int s_gt_sdl3_ReadKey( X_GT_BASE *pGT, int mask )
{
   X_GTSDL3 *sdl = X_GTSDL3_GET( pGT );
   int       resized;
   int       changed;

   X_SYMBOL_UNUSED( mask );
   s_gt_sdl3_PumpEvents();
   resized = sdl->resized;

   if( resized )
   {
      s_super.Refresh( pGT );
      sdl->resized = 0;
   }

   changed = s_gt_sdl3_UpdateCursor( sdl, 1 );

   if( resized || changed || SDL_GetAtomicInt( &sdl->paint ) )
      s_gt_sdl3_Present( sdl );

   if( sdl->keyOut != sdl->keyIn )
   {
      int key = sdl->keyQueue[sdl->keyOut];

      sdl->keyOut = ( sdl->keyOut + 1 ) % X_GTSDL3_KEY_QUEUE_SIZE;
      return key;
   }

   return 0;
}

static int s_gt_sdl3_mouse_IsPresent( X_GT_BASE *pGT )
{
   X_GTSDL3 *sdl = X_GTSDL3_GET( pGT );

   return sdl->mouseOn;
}

static void s_gt_sdl3_mouse_GetPos( X_GT_BASE *pGT, int *piRow, int *piCol )
{
   X_GTSDL3 *sdl = X_GTSDL3_GET( pGT );

   *piRow = sdl->mouseRow;
   *piCol = sdl->mouseCol;
}

static void s_gt_sdl3_mouse_SetPos( X_GT_BASE *pGT, int iRow, int iCol )
{
   X_GTSDL3 *sdl = X_GTSDL3_GET( pGT );
   float     renderX, renderY, windowX, windowY;

   iRow = GTSDL3_MAX( 0, GTSDL3_MIN( iRow, sdl->rows - 1 ) );
   iCol = GTSDL3_MAX( 0, GTSDL3_MIN( iCol, sdl->cols - 1 ) );
   sdl->mouseRow = iRow;
   sdl->mouseCol = iCol;

   renderX = ( float )( iCol * X_GTSDL3_CELL_WIDTH ) + X_GTSDL3_CELL_WIDTH / 2.0f;
   renderY = ( float )( iRow * X_GTSDL3_CELL_HEIGHT ) + X_GTSDL3_CELL_HEIGHT / 2.0f;

   sdl->mouseX = ( int )renderX;
   sdl->mouseY = ( int )renderY;

   SDL_RenderCoordinatesToWindow( sdl->renderer, renderX, renderY, &windowX, &windowY );
   SDL_WarpMouseInWindow( sdl->window, windowX, windowY );
}

static int s_gt_sdl3_mouse_ButtonState( X_GT_BASE *pGT, int iButton )
{
   X_GTSDL3 *sdl = X_GTSDL3_GET( pGT );
   Uint32    mask = s_gt_sdl3_MouseButtonMask( iButton );

   return sdl->mouseOn && ( sdl->buttons & mask ) != 0;
}

static int s_gt_sdl3_mouse_CountButton( X_GT_BASE *pGT )
{
   X_SYMBOL_UNUSED( pGT );

   return 3;
}

static const char *s_gt_sdl3_Version( X_GT_BASE *pGT, int iType )
{
   X_SYMBOL_UNUSED( pGT );

   if( iType == 0 )
      return "SDL3";

   return "xDev Terminal: SDL3";
}

static int s_gt_sdl3_PaletteValue( const Uint8 color[3] )
{
   return ( int )color[0] | ( ( int )color[1] << 8 ) | ( ( int )color[2] << 16 );
}

static int s_gt_sdl3_SetPaletteValue( X_GTSDL3 *sdl, int index, int value )
{
   Uint32 color = ( Uint32 )value;
   Uint8  red = ( Uint8 )color;
   Uint8  green = ( Uint8 )( color >> 8 );
   Uint8  blue = ( Uint8 )( color >> 16 );

   if( sdl->colors[index][0] != red || sdl->colors[index][1] != green || sdl->colors[index][2] != blue )
   {
      sdl->colors[index][0] = red;
      sdl->colors[index][1] = green;
      sdl->colors[index][2] = blue;
      sdl->fgColor = -1;
      return 1;
   }

   return 0;
}

static void s_gt_sdl3_SetScreenSize( X_GTSDL3 *sdl, int width, int height )
{
   if( width > 0 && height > 0 )
   {
      int rows = GTSDL3_MAX( height / X_GTSDL3_CELL_HEIGHT, 1 );
      int cols = GTSDL3_MAX( width / X_GTSDL3_CELL_WIDTH, 1 );
      int changed = rows != sdl->rows || cols != sdl->cols;

      s_gt_sdl3_ResizeGrid( sdl, rows, cols, 1 );

      if( changed )
         s_gt_sdl3_AddKey( sdl, K_RESIZE );
   }
}

static X_ITEM *s_gt_sdl3_WindowHandle( X_ITEM *pResult, X_GTSDL3 *sdl )
{
   SDL_PropertiesID props = SDL_GetWindowProperties( sdl->window );
   void            *handle;

   /* Return the native window object, not SDL_Window. Keep this lookup
      live because some backends, notably Wayland, can replace it. */
   handle = SDL_GetPointerProperty( props, SDL_PROP_WINDOW_WIN32_HWND_POINTER, NULL );

   if( !handle )
      handle = SDL_GetPointerProperty( props, SDL_PROP_WINDOW_COCOA_WINDOW_POINTER, NULL );
   if( !handle )
      handle = SDL_GetPointerProperty( props, SDL_PROP_WINDOW_UIKIT_WINDOW_POINTER, NULL );
   if( !handle )
      handle = SDL_GetPointerProperty( props, SDL_PROP_WINDOW_ANDROID_WINDOW_POINTER, NULL );
   if( !handle )
      handle = SDL_GetPointerProperty( props, SDL_PROP_WINDOW_WAYLAND_SURFACE_POINTER, NULL );
   if( !handle )
      handle = SDL_GetPointerProperty( props, SDL_PROP_WINDOW_VIVANTE_WINDOW_POINTER, NULL );

   if( handle )
      return x_itemPutPtr( pResult, handle );
   else
   {
      Sint64 x11Win = SDL_GetNumberProperty( props, SDL_PROP_WINDOW_X11_WINDOW_NUMBER, 0 );

      if( x11Win != 0 )
         return x_itemPutNInt( pResult, ( X_MAXINT )x11Win );
   }

   return x_itemPutPtr( pResult, NULL );
}

static int s_gt_sdl3_SetIconFile( X_GTSDL3 *sdl, X_ITEM *pItem )
{
   void       *hString;
   const char *fileName = x_itemGetStrUTF8( pItem, &hString, NULL );
#if SDL_VERSION_ATLEAST( 3, 4, 0 )
   SDL_Surface *icon = SDL_LoadSurface( fileName );
#else
   SDL_Surface *icon = SDL_LoadBMP( fileName );
#endif
   int result = SDL_SetWindowIcon( sdl->window, icon );

   x_strfree( hString );
   SDL_DestroySurface( sdl->icon );
   sdl->icon = icon;

   return result;
}

#if X_OS_WINDOWS
static int s_gt_sdl3_SetIconResource( X_GTSDL3 *sdl, X_ITEM *pItem )
{
   SDL_PropertiesID props = SDL_GetWindowProperties( sdl->window );
   HWND             window = ( HWND )SDL_GetPointerProperty( props, SDL_PROP_WINDOW_WIN32_HWND_POINTER, NULL );
   HINSTANCE        module = GetModuleHandle( NULL );
   HICON            icon;

   if( pItem && X_IS_STRING( pItem ) )
   {
      const char *name = x_itemGetCPtr( pItem );

      icon = ( HICON )LoadImageA( module, name, IMAGE_ICON, 0, 0, LR_DEFAULTSIZE | LR_SHARED );
   }
   else
   {
      LPCSTR resource = MAKEINTRESOURCEA( ( WORD )x_itemGetNI( pItem ) );

      icon = ( HICON )LoadImageA( module, resource, IMAGE_ICON, 0, 0, LR_DEFAULTSIZE | LR_SHARED );
   }
   SendMessage( window, WM_SETICON, ICON_SMALL, ( LPARAM )icon );
   SendMessage( window, WM_SETICON, ICON_BIG, ( LPARAM )icon );

   return icon != NULL;
}
#endif

static int s_gt_sdl3_MessageBox( X_GTSDL3 *sdl, X_ITEM *pText, X_ITEM *pButtons )
{
   X_ITEM                   *items[3] = { pText, NULL, NULL };
   X_ITEM                   *pTitle = NULL;
   void                     *strings[3];
   void                     *hTitle;
   X_SIZE                    lengths[3];
   X_SIZE                    total = 0;
   const char               *parts[3];
   const char               *title;
   SDL_MessageBoxButtonData  defBtn = { SDL_MESSAGEBOX_BUTTON_RETURNKEY_DEFAULT, 1, "OK" };
   SDL_MessageBoxButtonData *buttons = NULL;
   void                    **handles = NULL;
   SDL_MessageBoxData        data;
   char                     *message;
   char                     *dest;
   int                       count, index, buttonID = 0;

   /* xDev has no hash type.  The documented message box descriptor is:
      { cTitle, cText, cInfo, cDetail, aButtons }. */
   if( pText && X_IS_ARRAY( pText ) )
   {
      if( !pButtons )
         pButtons = x_arrayGetItemPtr( pText, 5 );

      pTitle = x_arrayGetItemPtr( pText, 1 );
      items[0] = x_arrayGetItemPtr( pText, 2 );
      items[1] = x_arrayGetItemPtr( pText, 3 );
      items[2] = x_arrayGetItemPtr( pText, 4 );
   }

   for( index = 0; index < 3; ++index )
   {
      parts[index] = x_itemGetStrUTF8( items[index], &strings[index], &lengths[index] );
      total += lengths[index];
   }

   message = ( char * )x_xgrab( total + 5 );
   dest = message;

   for( index = 0; index < 3; ++index )
   {
      if( lengths[index] )
      {
         if( dest != message )
         {
            *dest++ = '\n';
            *dest++ = '\n';
         }

         memcpy( dest, parts[index], lengths[index] );
         dest += lengths[index];
      }

      x_strfree( strings[index] );
   }
   *dest = '\0';

   title = x_itemGetStrUTF8( pTitle, &hTitle, NULL );
   count = IIF( pButtons, ( int )x_arrayLen( pButtons ), 0 );

   if( count )
   {
      buttons = ( SDL_MessageBoxButtonData * )x_xgrabArray( count, sizeof( *buttons ) );
      handles = ( void ** )x_xgrabArray( count, sizeof( *handles ) );

      for( index = 0; index < count; ++index )
      {
         buttons[index].flags = IIF( index == 0, SDL_MESSAGEBOX_BUTTON_RETURNKEY_DEFAULT, 0 );
         buttons[index].buttonID = index + 1;
         buttons[index].text = x_arrayGetStrUTF8( pButtons, index + 1, &handles[index], NULL );
      }
   }

   SDL_zero( data );
   data.flags = SDL_MESSAGEBOX_INFORMATION;
   data.window = sdl->window;
   data.title = IIF( pTitle, title, SDL_GetWindowTitle( sdl->window ) );
   data.message = message;
   data.numbuttons = IIF( count, count, 1 );
   data.buttons = IIF( count, buttons, &defBtn );
   SDL_ShowMessageBox( &data, &buttonID );

   for( index = 0; index < count; ++index )
      x_strfree( handles[index] );

   x_xfree( handles );
   x_xfree( buttons );
   x_strfree( hTitle );
   x_xfree( message );
   SDL_SetAtomicInt( &sdl->paint, 1 );

   return GTSDL3_MAX( buttonID, 0 );
}

static void SDLCALL s_gt_sdl3_FileDialogCallback( void *userdata, const char *const *filelist, int filter )
{
   X_GTSDL3_FILEDIALOG_RESULT *result = ( X_GTSDL3_FILEDIALOG_RESULT * )userdata;
   int                         index;

   result->filter = filter;

   if( filelist )
   {
      while( filelist[result->count] )
         ++result->count;
      if( result->count )
      {
         result->files = ( char ** )x_xgrabArray( result->count, sizeof( *result->files ) );

         for( index = 0; index < result->count; ++index )
            result->files[index] = x_strdup( filelist[index] );
      }
   }
   else
      result->error = x_strdup( SDL_GetError() );

   SDL_SetAtomicInt( &result->done, 1 );
}

static X_ITEM *s_gt_sdl3_FileDialog( X_GTSDL3 *sdl, X_ITEM *pOptions, X_ITEM *pResult )
{
   SDL_FileDialogType         type = SDL_FILEDIALOG_OPENFILE;
   SDL_PropertiesID           props = SDL_CreateProperties();
   SDL_DialogFileFilter      *filters = NULL;
   X_GTSDL3_FILEDIALOG_RESULT dialog;

   /* File dialog options use { cType, cTitle, cDirectory, aFilters, lMany }.
      The result is { lOk, aFiles, nFilter, cError }. */
   int     optionsAreArray = pOptions && X_IS_ARRAY( pOptions );
   X_ITEM *pType = IIF( optionsAreArray, x_arrayGetItemPtr( pOptions, 1 ), NULL );
   X_ITEM *pTitle = IIF( optionsAreArray, x_arrayGetItemPtr( pOptions, 2 ), NULL );
   X_ITEM *pDir = IIF( optionsAreArray, x_arrayGetItemPtr( pOptions, 3 ), NULL );
   X_ITEM *pFilters = IIF( optionsAreArray, x_arrayGetItemPtr( pOptions, 4 ), NULL );
   X_ITEM *pMany = IIF( optionsAreArray, x_arrayGetItemPtr( pOptions, 5 ), NULL );
   X_ITEM *pFiles;
   X_ITEM  value = X_ITEM_NIL_INIT;

   void      **handles = NULL;
   void       *hTitle;
   void       *hDir;
   const char *name = x_itemGetCPtr( pType );
   const char *title = x_itemGetStrUTF8( pTitle, &hTitle, NULL );
   const char *dir = x_itemGetStrUTF8( pDir, &hDir, NULL );
   int         count = 0;
   int         index;
   int         many;

   SDL_zero( dialog );
   dialog.filter = -1;

   if( x_stricmp( name, "SAVE" ) == 0 )
      type = SDL_FILEDIALOG_SAVEFILE;
   else if( x_stricmp( name, "FOLDER" ) == 0 )
      type = SDL_FILEDIALOG_OPENFOLDER;

   if( type != SDL_FILEDIALOG_OPENFOLDER && pFilters )
      count = ( int )x_arrayLen( pFilters );
   if( count )
   {
      filters = ( SDL_DialogFileFilter * )x_xgrabArray( count, sizeof( *filters ) );
      handles = ( void ** )x_xgrabArray( count, 2 * sizeof( *handles ) );

      for( index = 0; index < count; ++index )
      {
         X_ITEM *pFilter = x_arrayGetItemPtr( pFilters, index + 1 );

         filters[index].name = x_arrayGetStrUTF8( pFilter, 1, &handles[index * 2], NULL );
         filters[index].pattern = x_arrayGetStrUTF8( pFilter, 2, &handles[index * 2 + 1], NULL );
      }
   }

   SDL_SetPointerProperty( props, SDL_PROP_FILE_DIALOG_WINDOW_POINTER, sdl->window );

   if( pTitle )
      SDL_SetStringProperty( props, SDL_PROP_FILE_DIALOG_TITLE_STRING, title );
   if( pDir )
      SDL_SetStringProperty( props, SDL_PROP_FILE_DIALOG_LOCATION_STRING, dir );

   many = x_itemGetL( pMany ) && type != SDL_FILEDIALOG_SAVEFILE;
   SDL_SetBooleanProperty( props, SDL_PROP_FILE_DIALOG_MANY_BOOLEAN, many );
   if( count )
   {
      SDL_SetPointerProperty( props, SDL_PROP_FILE_DIALOG_FILTERS_POINTER, filters );
      SDL_SetNumberProperty( props, SDL_PROP_FILE_DIALOG_NFILTERS_NUMBER, count );
   }

   SDL_ShowFileDialogWithProperties( type, s_gt_sdl3_FileDialogCallback, &dialog, props );
   SDL_DestroyProperties( props );

   while( SDL_GetAtomicInt( &dialog.done ) == 0 )
   {
      s_gt_sdl3_PumpEvents();
      SDL_Delay( 10 );
   }

   SDL_SetAtomicInt( &sdl->paint, 1 );

   if( pResult == NULL )
      pResult = x_itemArrayNew( 4 );
   else
      x_arrayNew( pResult, 4 );
   x_itemPutL( &value, dialog.error == NULL && dialog.count > 0 );
   x_arraySet( pResult, 1, &value );
   pFiles = x_itemArrayNew( dialog.count );

   for( index = 0; index < dialog.count; ++index )
   {
      x_arraySetStrUTF8( pFiles, index + 1, dialog.files[index] );
      x_xfree( dialog.files[index] );
   }

   x_arraySet( pResult, 2, pFiles );
   x_itemPutNI( &value, dialog.filter + 1 );
   x_arraySet( pResult, 3, &value );
   x_itemPutStrUTF8( &value, dialog.error );
   x_arraySet( pResult, 4, &value );
   x_itemRelease( pFiles );
   x_itemClear( &value );

   for( index = 0; index < count * 2; ++index )
      x_strfree( handles[index] );

   x_xfree( handles );
   x_xfree( filters );
   x_strfree( hTitle );
   x_strfree( hDir );
   x_xfree( dialog.files );
   x_xfree( dialog.error );

   return pResult;
}

static int s_gt_sdl3_Alert( X_GT_BASE *pGT, X_ITEM *pMessage, X_ITEM *pOptions, int iClrNorm, int iClrHigh,
                            double dDelay )
{
   X_GTSDL3 *sdl = X_GTSDL3_GET( pGT );

   if( sdl->msgAlert )
   {
      double delay = dDelay;

      /* SDL_ShowMessageBox() has no portable timeout. Preserve timed Alert()
         behavior by leaving such calls to the base implementation. */
      if( delay * 100 < 1 )
         return s_gt_sdl3_MessageBox( sdl, pMessage, pOptions );
   }

   return s_super.Alert( pGT, pMessage, pOptions, iClrNorm, iClrHigh, dDelay );
}

static int s_gt_sdl3_DispImage( X_GT_BASE *pGT, X_GTSDL3 *sdl, X_ITEM *image, X_ITEM *areaItem );

static int s_gt_sdl3_Info( X_GT_BASE *pGT, int iType, X_GT_INFO *pInfo )
{
   X_GTSDL3 *sdl = X_GTSDL3_GET( pGT );
   int       fHandled = 1;

   switch( iType )
   {
      case X_GTI_ISCTWIN:
      case X_GTI_ISMULTIWIN: pInfo->pResult = x_itemPutL( pInfo->pResult, 0 ); break;
      case X_GTI_ISGRAPHIC:
      case X_GTI_ISSCREENPOS:
      case X_GTI_KBDSUPPORT:
      case X_GTI_ISUNICODE:
      case X_GTI_ONLINE: pInfo->pResult = x_itemPutL( pInfo->pResult, 1 ); break;
      case X_GTI_VERSION:
         pInfo->pResult = x_itemPutC( pInfo->pResult, s_gt_sdl3_Version( pGT, x_itemGetNI( pInfo->pNewVal ) ) );
         break;

      case X_GTI_SCREENWIDTH:
         pInfo->pResult = x_itemPutNI( pInfo->pResult, sdl->logWidth );

         if( pInfo->pNewVal )
            s_gt_sdl3_SetScreenSize( sdl, x_itemGetNI( pInfo->pNewVal ), sdl->logHgt );
         break;
      case X_GTI_SCREENHEIGHT:
         pInfo->pResult = x_itemPutNI( pInfo->pResult, sdl->logHgt );

         if( pInfo->pNewVal )
            s_gt_sdl3_SetScreenSize( sdl, sdl->logWidth, x_itemGetNI( pInfo->pNewVal ) );
         break;
      case X_GTI_WINDOWWIDTH: pInfo->pResult = x_itemPutNI( pInfo->pResult, sdl->winWidth ); break;
      case X_GTI_WINDOWHEIGHT: pInfo->pResult = x_itemPutNI( pInfo->pResult, sdl->winHgt ); break;
      case X_GTI_OUTPUTWIDTH: pInfo->pResult = x_itemPutNI( pInfo->pResult, sdl->outWidth ); break;
      case X_GTI_OUTPUTHEIGHT: pInfo->pResult = x_itemPutNI( pInfo->pResult, sdl->outHgt ); break;
      case X_GTI_LOGICALWIDTH: pInfo->pResult = x_itemPutNI( pInfo->pResult, sdl->logWidth ); break;
      case X_GTI_LOGICALHEIGHT: pInfo->pResult = x_itemPutNI( pInfo->pResult, sdl->logHgt ); break;
      case X_GTI_PIXELDENSITY: pInfo->pResult = x_itemPutND( pInfo->pResult, sdl->density ); break;
      case X_GTI_DISPLAYSCALE: pInfo->pResult = x_itemPutND( pInfo->pResult, sdl->dispScal ); break;
      case X_GTI_CONTENTSCALE: pInfo->pResult = x_itemPutND( pInfo->pResult, sdl->scale ); break;
      case X_GTI_RENDERERNAME:
      {
         static const char prefix[] = "SDL3/";
         const char       *name = SDL_GetRendererName( sdl->renderer );
         X_SIZE            length = strlen( name ) + sizeof( prefix ) - 1;
         char             *value = ( char * )x_xgrab( length + 1 );

         snprintf( value, length + 1, "%s%s", prefix, name );
         pInfo->pResult = x_itemPutCLPtr( pInfo->pResult, value, length );
         break;
      }
      case X_GTI_SCREENDEPTH: pInfo->pResult = x_itemPutNI( pInfo->pResult, 32 ); break;
      case X_GTI_DESKTOPWIDTH:
      case X_GTI_DESKTOPHEIGHT:
      case X_GTI_DESKTOPROWS:
      case X_GTI_DESKTOPCOLS:
      {
         SDL_DisplayID display = SDL_GetDisplayForWindow( sdl->window );
         SDL_Rect      bounds;
         int           value;

         SDL_GetDisplayBounds( display, &bounds );

         switch( iType )
         {
            case X_GTI_DESKTOPWIDTH: value = bounds.w; break;
            case X_GTI_DESKTOPHEIGHT: value = bounds.h; break;
            case X_GTI_DESKTOPROWS:
               value = ( int )( bounds.h / ( ( double )X_GTSDL3_CELL_HEIGHT * sdl->scale ) );
               break;
            default: value = ( int )( bounds.w / ( ( double )X_GTSDL3_CELL_WIDTH * sdl->scale ) ); break;
         }

         pInfo->pResult = x_itemPutNI( pInfo->pResult, value );
         break;
      }
      case X_GTI_DESKTOPDEPTH:
      {
         SDL_DisplayID                 display = SDL_GetDisplayForWindow( sdl->window );
         const SDL_DisplayMode        *mode = SDL_GetDesktopDisplayMode( display );
         const SDL_PixelFormatDetails *format = SDL_GetPixelFormatDetails( mode->format );

         pInfo->pResult = x_itemPutNI( pInfo->pResult, format->bits_per_pixel );
         break;
      }
      case X_GTI_VIEWMAXHEIGHT:
      case X_GTI_VIEWPORTHEIGHT: pInfo->pResult = x_itemPutNI( pInfo->pResult, sdl->rows ); break;
      case X_GTI_VIEWMAXWIDTH:
      case X_GTI_VIEWPORTWIDTH: pInfo->pResult = x_itemPutNI( pInfo->pResult, sdl->cols ); break;
      case X_GTI_SCREENSIZE:
      {
         if( !pInfo->pResult )
            pInfo->pResult = x_itemNew( NULL );

         x_arrayNew( pInfo->pResult, 2 );
         x_arraySetNI( pInfo->pResult, 1, sdl->logWidth );
         x_arraySetNI( pInfo->pResult, 2, sdl->logHgt );

         if( pInfo->pNewVal )
         {
            int width = x_arrayGetNI( pInfo->pNewVal, 1 );
            int height = x_arrayGetNI( pInfo->pNewVal, 2 );

            s_gt_sdl3_SetScreenSize( sdl, width, height );
         }
         break;
      }
      case X_GTI_SETPOS_XY:
      case X_GTI_SETPOS_ROWCOL:
      {
         int x = 0;
         int y = 0;

         SDL_GetWindowPosition( sdl->window, &x, &y );

         if( !pInfo->pResult )
            pInfo->pResult = x_itemNew( NULL );

         x_arrayNew( pInfo->pResult, 2 );

         if( iType == X_GTI_SETPOS_ROWCOL )
         {
            x_arraySetNI( pInfo->pResult, 1, y / X_GTSDL3_CELL_HEIGHT );
            x_arraySetNI( pInfo->pResult, 2, x / X_GTSDL3_CELL_WIDTH );
         }
         else
         {
            x_arraySetNI( pInfo->pResult, 1, x );
            x_arraySetNI( pInfo->pResult, 2, y );
         }

         if( pInfo->pNewVal2 )
         {
            x = x_itemGetNI( pInfo->pNewVal );
            y = x_itemGetNI( pInfo->pNewVal2 );
         }
         else if( pInfo->pNewVal )
         {
            x = x_arrayGetNI( pInfo->pNewVal, 1 );
            y = x_arrayGetNI( pInfo->pNewVal, 2 );
         }
         else
            break;

         if( iType == X_GTI_SETPOS_ROWCOL )
         {
            X_I64 windowX = ( X_I64 )y * X_GTSDL3_CELL_WIDTH;
            X_I64 windowY = ( X_I64 )x * X_GTSDL3_CELL_HEIGHT;

            if( windowX < INT_MIN || windowX > INT_MAX || windowY < INT_MIN || windowY > INT_MAX )
               break;

            x = ( int )windowX;
            y = ( int )windowY;
         }

         SDL_SetWindowPosition( sdl->window, x, y );
         SDL_SyncWindow( sdl->window );
         break;
      }

      case X_GTI_FONTSIZE: pInfo->pResult = x_itemPutNI( pInfo->pResult, X_GTSDL3_CELL_HEIGHT ); break;
      case X_GTI_FONTWIDTH: pInfo->pResult = x_itemPutNI( pInfo->pResult, X_GTSDL3_CELL_WIDTH ); break;
      case X_GTI_FONTWEIGHT: pInfo->pResult = x_itemPutNI( pInfo->pResult, X_GTI_FONTW_NORMAL ); break;
      case X_GTI_FONTQUALITY: pInfo->pResult = x_itemPutNI( pInfo->pResult, X_GTI_FONTQ_DRAFT ); break;
      case X_GTI_FONTNAME: pInfo->pResult = x_itemPutC( pInfo->pResult, "Fixed 9x18" ); break;
      case X_GTI_FONTSEL: pInfo->pResult = x_itemPutC( pInfo->pResult, NULL ); break;
      case X_GTI_FONTATTRIBUTE:
         pInfo->pResult = x_itemPutNI( pInfo->pResult, X_GTI_FONTA_FIXMETRIC | X_GTI_FONTA_CLRBKG );
         break;
      case X_GTI_PALETTE:
      {
         if( pInfo->pNewVal && X_IS_NUMERIC( pInfo->pNewVal ) )
         {
            int index = x_itemGetNI( pInfo->pNewVal );

            if( index >= 0 && index < 16 )
            {
               pInfo->pResult = x_itemPutNI( pInfo->pResult, s_gt_sdl3_PaletteValue( sdl->colors[index] ) );

               if( pInfo->pNewVal2 && s_gt_sdl3_SetPaletteValue( sdl, index, x_itemGetNI( pInfo->pNewVal2 ) ) )
                  X_GTSELF_EXPOSEAREA( pGT, 0, 0, sdl->rows - 1, sdl->cols - 1 );
            }
         }
         else
         {
            int index;

            if( !pInfo->pResult )
               pInfo->pResult = x_itemNew( NULL );

            x_arrayNew( pInfo->pResult, 16 );

            for( index = 0; index < 16; ++index )
               x_arraySetNI( pInfo->pResult, index + 1, s_gt_sdl3_PaletteValue( sdl->colors[index] ) );

            if( pInfo->pNewVal )
            {
               int changed = 0;

               for( index = 0; index < 16; ++index )
               {
                  if( s_gt_sdl3_SetPaletteValue( sdl, index, x_arrayGetNI( pInfo->pNewVal, index + 1 ) ) )
                     changed = 1;
               }

               if( changed )
                  X_GTSELF_EXPOSEAREA( pGT, 0, 0, sdl->rows - 1, sdl->cols - 1 );
            }
         }
         break;
      }

      case X_GTI_KBDSHIFTS: pInfo->pResult = x_itemPutNI( pInfo->pResult, s_gt_sdl3_KbdShifts() ); break;
      case X_GTI_ALTENTER:
         pInfo->pResult = x_itemPutL( pInfo->pResult, sdl->altEnter );

         if( pInfo->pNewVal )
            sdl->altEnter = x_itemGetL( pInfo->pNewVal );
         break;

      /* SDL clipboard access is subject to the same main-thread contract as
         the other gtsdl3 window and renderer operations. */
      case X_GTI_CLIPBOARDDATA:
      {
         if( pInfo->pNewVal )
         {
            void       *hString = NULL;
            const char *text = x_itemGetStrUTF8( pInfo->pNewVal, &hString, NULL );

            SDL_SetClipboardText( text );

            x_strfree( hString );
         }
         else
         {
            char *text = SDL_GetClipboardText();

            pInfo->pResult = x_itemPutStrUTF8( pInfo->pResult, text );
            SDL_free( text );
         }
         break;
      }

      case X_GTI_CLIPBOARDPASTE:
      {
         char *text = SDL_GetClipboardText();
         int   read = text && *text;

         if( read )
            pGT->pFuncTable->InkeySetText( pGT, text, strlen( text ), 1 );
         SDL_free( text );
         pInfo->pResult = x_itemPutL( pInfo->pResult, read );
         break;
      }

      case X_GTI_SOUND:
      {
         SDL_AudioSpec spec;
         Uint8        *data = NULL;
         Uint32        length = 0;
         int           played = 0;
         int           valid = pInfo->pNewVal && X_IS_STRING( pInfo->pNewVal );

         if( valid && x_itemGetCLen( pInfo->pNewVal ) != strlen( x_itemGetCPtr( pInfo->pNewVal ) ) )
            valid = 0;
         if( valid && !sdl->audioOn )
            sdl->audioOn = SDL_InitSubSystem( SDL_INIT_AUDIO );
         if( valid && !sdl->audioOn )
            valid = 0;
         if( valid && !SDL_LoadWAV( x_itemGetCPtr( pInfo->pNewVal ), &spec, &data, &length ) )
            valid = 0;
         if( valid && length > INT_MAX )
            valid = 0;
         if( valid )
         {
            /* A new sound stops the previous one, like PlaySound() with SND_ASYNC */
            if( sdl->sound )
               SDL_DestroyAudioStream( sdl->sound );
            sdl->sound = SDL_OpenAudioDeviceStream( SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, NULL, NULL );
            played = sdl->sound != NULL;
            if( played )
               played = SDL_PutAudioStreamData( sdl->sound, data, ( int )length );
            if( played )
               played = SDL_FlushAudioStream( sdl->sound );
            if( played )
               played = SDL_ResumeAudioStreamDevice( sdl->sound );
         }
         SDL_free( data );
         pInfo->pResult = x_itemPutL( pInfo->pResult, played );
         break;
      }

      case X_GTI_CURSORBLINKRATE:
         pInfo->pResult = x_itemPutNI( pInfo->pResult, sdl->blink );

         if( pInfo->pNewVal )
         {
            sdl->blink = GTSDL3_MAX( x_itemGetNI( pInfo->pNewVal ), 0 );
            sdl->curTime = SDL_GetTicks();
         }
         break;

      case X_GTI_MOUSESTATUS:
         pInfo->pResult = x_itemPutL( pInfo->pResult, sdl->mouseOn );

         if( pInfo->pNewVal )
         {
            sdl->mouseOn = x_itemGetL( pInfo->pNewVal );

            if( !sdl->mouseOn )
            {
               sdl->buttons = 0;
               sdl->btnTime[0] = sdl->btnTime[1] = sdl->btnTime[2] = 0;
               sdl->wheelX = sdl->wheelY = 0.0f;
            }
         }
         break;
      case X_GTI_MOUSEPOS_XY:
         if( !pInfo->pResult )
            pInfo->pResult = x_itemNew( NULL );

         x_arrayNew( pInfo->pResult, 2 );
         x_arraySetNI( pInfo->pResult, 1, sdl->mouseX );
         x_arraySetNI( pInfo->pResult, 2, sdl->mouseY );
         break;

      case X_GTI_WINTITLE:
      {
         pInfo->pResult = x_itemPutStrUTF8( pInfo->pResult, sdl->title );

         if( pInfo->pNewVal )
         {
            x_strfree( sdl->titleHandle );
            sdl->title = x_itemGetStrUTF8( pInfo->pNewVal, &sdl->titleHandle, NULL );
            SDL_SetWindowTitle( sdl->window, sdl->title );
         }
         break;
      }
      case X_GTI_SYSTEMTHEME:
      {
         switch( SDL_GetSystemTheme() )
         {
            case SDL_SYSTEM_THEME_LIGHT: pInfo->pResult = x_itemPutC( pInfo->pResult, "LIGHT" ); break;
            case SDL_SYSTEM_THEME_DARK: pInfo->pResult = x_itemPutC( pInfo->pResult, "DARK" ); break;
            default: pInfo->pResult = x_itemPutC( pInfo->pResult, "UNKNOWN" ); break;
         }
         break;
      }
      case X_GTI_OPENURL:
      {
         int opened = 0;

         if( pInfo->pNewVal )
         {
            void       *hString = NULL;
            const char *url = x_itemGetStrUTF8( pInfo->pNewVal, &hString, NULL );

            opened = SDL_OpenURL( url );
            x_strfree( hString );
         }

         pInfo->pResult = x_itemPutL( pInfo->pResult, opened );
         break;
      }
      case X_GTI_FILEDIALOG: pInfo->pResult = s_gt_sdl3_FileDialog( sdl, pInfo->pNewVal, pInfo->pResult ); break;
      case X_GTI_WINDOWOPACITY:
         pInfo->pResult = x_itemPutND( pInfo->pResult, SDL_GetWindowOpacity( sdl->window ) );

         if( pInfo->pNewVal )
            SDL_SetWindowOpacity( sdl->window, ( float )x_itemGetND( pInfo->pNewVal ) );
         break;
      case X_GTI_WINDOWALWAYSONTOP:
      {
         int onTop = ( SDL_GetWindowFlags( sdl->window ) & SDL_WINDOW_ALWAYS_ON_TOP ) != 0;

         pInfo->pResult = x_itemPutL( pInfo->pResult, onTop );

         if( pInfo->pNewVal && x_itemGetL( pInfo->pNewVal ) != onTop )
            SDL_SetWindowAlwaysOnTop( sdl->window, x_itemGetL( pInfo->pNewVal ) );
         break;
      }
      case X_GTI_WINDOWBORDERED:
      {
         int bordered = ( SDL_GetWindowFlags( sdl->window ) & SDL_WINDOW_BORDERLESS ) == 0;

         pInfo->pResult = x_itemPutL( pInfo->pResult, bordered );

         if( pInfo->pNewVal && x_itemGetL( pInfo->pNewVal ) != bordered )
            SDL_SetWindowBordered( sdl->window, x_itemGetL( pInfo->pNewVal ) );
         break;
      }
      case X_GTI_ICONFILE:
         pInfo->pResult = x_itemPutL( pInfo->pResult, s_gt_sdl3_SetIconFile( sdl, pInfo->pNewVal ) );
         break;
      case X_GTI_ICONRES:
#if X_OS_WINDOWS
         pInfo->pResult = x_itemPutL( pInfo->pResult, s_gt_sdl3_SetIconResource( sdl, pInfo->pNewVal ) );
#else
         pInfo->pResult = x_itemPutL( pInfo->pResult, 0 );
#endif
         break;
      case X_GTI_SELECTCOPY:
         pInfo->pResult = x_itemPutL( pInfo->pResult, sdl->copyOn );

         if( pInfo->pNewVal )
         {
            sdl->copyOn = x_itemGetL( pInfo->pNewVal );

            if( !sdl->copyOn )
               s_gt_sdl3_ClearSelection( sdl, 1 );
         }
         break;
      case X_GTI_RESIZABLE:
      {
         pInfo->pResult = x_itemPutL( pInfo->pResult, sdl->canSize );

         if( pInfo->pNewVal )
         {
            int canSize = x_itemGetL( pInfo->pNewVal );

            SDL_SetWindowResizable( sdl->window, canSize );
            sdl->canSize = canSize;
         }
         break;
      }
      case X_GTI_CLOSABLE:
         pInfo->pResult = x_itemPutL( pInfo->pResult, sdl->close == 0 );

         if( pInfo->pNewVal )
         {
            if( x_itemGetL( pInfo->pNewVal ) )
               sdl->close = 0;
            else if( sdl->close == 0 )
               sdl->close = 1;
         }
         break;
      case X_GTI_RESIZEFONT:
         /* TODO: .T. (fixed grid, the font follows the window) like GTWVT and GTXWC */
         pInfo->pResult = x_itemPutL( pInfo->pResult, 0 );
         break;
      case X_GTI_ISFULLSCREEN:
      {
         int full = ( SDL_GetWindowFlags( sdl->window ) & SDL_WINDOW_FULLSCREEN ) != 0;

         pInfo->pResult = x_itemPutL( pInfo->pResult, full );

         if( pInfo->pNewVal && x_itemGetL( pInfo->pNewVal ) != full )
         {
            SDL_SetWindowFullscreen( sdl->window, !full );
            SDL_SyncWindow( sdl->window );
         }
         break;
      }
      case X_GTI_MAXIMIZED:
      {
         int maxed = ( SDL_GetWindowFlags( sdl->window ) & SDL_WINDOW_MAXIMIZED ) != 0;

         pInfo->pResult = x_itemPutL( pInfo->pResult, maxed );

         if( pInfo->pNewVal )
         {
            if( x_itemGetL( pInfo->pNewVal ) )
            {
               SDL_MaximizeWindow( sdl->window );
               SDL_SyncWindow( sdl->window );
            }
            else if( maxed )
            {
               SDL_RestoreWindow( sdl->window );
               SDL_SyncWindow( sdl->window );
            }
         }
         break;
      }
      case X_GTI_WINHANDLE: pInfo->pResult = s_gt_sdl3_WindowHandle( pInfo->pResult, sdl ); break;
      case X_GTI_RESIZESTEP:
         pInfo->pResult = x_itemPutL( pInfo->pResult, sdl->sizeStep );

         if( pInfo->pNewVal )
         {
            int sizeStep = x_itemGetL( pInfo->pNewVal );

            if( sizeStep != sdl->sizeStep )
            {
               sdl->sizeStep = sizeStep;

               if( sizeStep )
                  s_gt_sdl3_ResizeFromWindow( sdl, sdl->winWidth, sdl->winHgt );
            }
         }
         break;
      case X_GTI_CLOSEMODE:
         pInfo->pResult = x_itemPutNI( pInfo->pResult, sdl->close );

         if( pInfo->pNewVal )
            sdl->close = x_itemGetNI( pInfo->pNewVal );
         break;
      case X_GTI_MINIMIZED:
      {
         int mined = ( SDL_GetWindowFlags( sdl->window ) & SDL_WINDOW_MINIMIZED ) != 0;

         pInfo->pResult = x_itemPutL( pInfo->pResult, mined );

         if( pInfo->pNewVal )
         {
            if( x_itemGetL( pInfo->pNewVal ) )
            {
               SDL_MinimizeWindow( sdl->window );
               SDL_SyncWindow( sdl->window );
            }
            else if( mined )
            {
               SDL_RestoreWindow( sdl->window );
               SDL_SyncWindow( sdl->window );
            }
         }
         break;
      }
      case X_GTI_MSGBOX:
         if( !pInfo->pNewVal || X_IS_LOGICAL( pInfo->pNewVal ) || X_IS_NIL( pInfo->pNewVal ) )
         {
            pInfo->pResult = x_itemPutL( pInfo->pResult, sdl->msgAlert );

            if( pInfo->pNewVal && X_IS_LOGICAL( pInfo->pNewVal ) )
               sdl->msgAlert = x_itemGetL( pInfo->pNewVal );
         }
         else
         {
            int button = s_gt_sdl3_MessageBox( sdl, pInfo->pNewVal, pInfo->pNewVal2 );

            pInfo->pResult = x_itemPutNI( pInfo->pResult, button );
         }
         break;

      case X_GTI_DISPIMAGE:
      {
         int shown = s_gt_sdl3_DispImage( pGT, sdl, pInfo->pNewVal, pInfo->pNewVal2 );

         pInfo->pResult = x_itemPutL( pInfo->pResult, shown );
         break;
      }

      default: fHandled = 0; break;
   }

   return IIF( fHandled, 1, s_super.Info( pGT, iType, pInfo ) );
}

static int s_gt_sdl3_GetGfxClip( X_GTSDL3 *sdl, SDL_Rect *clip )
{
   int left, top, right, bottom;

   if( sdl->clipOn )
   {
      left = GTSDL3_MAX( sdl->clipLeft, 0 );
      top = GTSDL3_MAX( sdl->clipTop, 0 );
      right = GTSDL3_MIN( sdl->clipRgt, sdl->cols * X_GTSDL3_CELL_WIDTH - 1 );
      bottom = GTSDL3_MIN( sdl->clipBtm, sdl->rows * X_GTSDL3_CELL_HEIGHT - 1 );
   }
   else
   {
      left = top = 0;
      right = sdl->cols * X_GTSDL3_CELL_WIDTH - 1;
      bottom = sdl->rows * X_GTSDL3_CELL_HEIGHT - 1;
   }

   if( left > right || top > bottom )
   {
      clip->x = clip->y = clip->w = clip->h = 0;
      return 0;
   }

   clip->x = left;
   clip->y = top;
   clip->w = right - left + 1;
   clip->h = bottom - top + 1;

   return 1;
}

static void s_gt_sdl3_BeginGfx( X_GTSDL3 *sdl, X_GTSDL3_GFX_STATE *state, int useClip )
{
   SDL_Rect clip;

   state->oldTarget = SDL_GetRenderTarget( sdl->renderer );
   SDL_SetRenderTarget( sdl->renderer, sdl->screen );

   state->hadClip = SDL_RenderClipEnabled( sdl->renderer );

   if( state->hadClip )
      SDL_GetRenderClipRect( sdl->renderer, &state->oldClip );

   SDL_GetRenderDrawBlendMode( sdl->renderer, &state->oldBlend );
   SDL_GetRenderDrawColor( sdl->renderer, &state->oldRed, &state->oldGreen, &state->oldBlue, &state->oldAlpha );

   if( useClip && sdl->clipOn )
   {
      s_gt_sdl3_GetGfxClip( sdl, &clip );
      SDL_SetRenderClipRect( sdl->renderer, &clip );
   }
   else
      SDL_SetRenderClipRect( sdl->renderer, NULL );

   SDL_SetRenderDrawBlendMode( sdl->renderer, SDL_BLENDMODE_NONE );
}

static void s_gt_sdl3_EndGfx( X_GTSDL3 *sdl, X_GTSDL3_GFX_STATE *state )
{
   SDL_SetRenderDrawColor( sdl->renderer, state->oldRed, state->oldGreen, state->oldBlue, state->oldAlpha );
   SDL_SetRenderDrawBlendMode( sdl->renderer, state->oldBlend );
   SDL_SetRenderClipRect( sdl->renderer, IIF( state->hadClip, &state->oldClip, NULL ) );
   SDL_SetRenderTarget( sdl->renderer, state->oldTarget );
   SDL_SetAtomicInt( &sdl->paint, 1 );
}

static void s_gt_sdl3_BeginGfxOperation( X_GTSDL3 *sdl, X_GTSDL3_GFX_STATE *state, int useClip )
{
   SDL_Rect clip;

   if( !sdl->gfxOn )
   {
      s_gt_sdl3_BeginGfx( sdl, state, useClip );
      return;
   }

   SDL_SetRenderTarget( sdl->renderer, sdl->screen );

   if( useClip && sdl->clipOn )
   {
      s_gt_sdl3_GetGfxClip( sdl, &clip );
      SDL_SetRenderClipRect( sdl->renderer, &clip );
   }
   else
      SDL_SetRenderClipRect( sdl->renderer, NULL );

   SDL_SetRenderDrawBlendMode( sdl->renderer, SDL_BLENDMODE_NONE );
}

static void s_gt_sdl3_EndGfxOperation( X_GTSDL3 *sdl, X_GTSDL3_GFX_STATE *state )
{
   if( sdl->gfxOn )
   {
      /* Do not leave a GFX clip active between VM calls: pending terminal
         redraws and asynchronous window presentation share the renderer. */
      SDL_SetRenderClipRect( sdl->renderer, NULL );
   }
   else
      s_gt_sdl3_EndGfx( sdl, state );
}

static SDL_Surface *s_gt_sdl3_RawImageSurface( X_ITEM *image )
{
   X_ITEM         *data;
   X_SIZE          imageLength;
   X_SIZE          dataLength;
   X_SIZE          required;
   int             width;
   int             height;
   int             depth;
   int             tight, pitch;
   const void     *pixels;
   SDL_PixelFormat format;
   SDL_Surface    *surface;

   if( !image || !X_IS_ARRAY( image ) || ( imageLength = x_arrayLen( image ) ) < 3 )
      return NULL;

   data = x_arrayGetItemPtr( image, 1 );
   width = x_arrayGetNI( image, 2 );
   height = x_arrayGetNI( image, 3 );
   depth = IIF( imageLength == 4, x_arrayGetNI( image, 4 ), 32 );
   if( width <= 0 || height <= 0 )
      return NULL;

   if( depth == 0 )
      depth = 32;
   switch( depth )
   {
      case 1: format = SDL_PIXELFORMAT_INDEX1MSB; break;
      case 16: format = SDL_PIXELFORMAT_RGB565; break;
      case 24: format = SDL_PIXELFORMAT_RGB24; break;
      case 32: format = SDL_PIXELFORMAT_XRGB8888; break;
      default: return NULL;
   }

   if( ( X_I64 )width * depth > INT_MAX - 7 )
      return NULL;

   tight = ( width * depth + 7 ) / 8;
   pitch = ( tight + 3 ) & ~3;
   if( pitch <= 0 || ( X_SIZE )height > SIZE_MAX / ( X_SIZE )pitch )
      return NULL;

   if( data && X_IS_STRING( data ) )
   {
      pixels = x_itemGetCPtr( data );
      dataLength = x_itemGetCLen( data );

      if( dataLength == ( X_SIZE )height * tight )
         pitch = tight;
      required = ( X_SIZE )height * pitch;
      if( dataLength < required )
         return NULL;
   }
   else
      pixels = x_itemGetPtr( data );

   if( !pixels )
      return NULL;

   surface = SDL_CreateSurfaceFrom( width, height, format, X_UNCONST( pixels ), pitch );

   if( surface && depth == 1 )
      SDL_CreateSurfacePalette( surface );

   return surface;
}

static int s_gt_sdl3_ImageDestination( X_GTSDL3 *sdl, SDL_Surface *surface, X_ITEM *areaItem, SDL_FRect *dest )
{
   int scrWidth = sdl->cols * X_GTSDL3_CELL_WIDTH;
   int scrHgt = sdl->rows * X_GTSDL3_CELL_HEIGHT;

   dest->x = 0.0f;
   dest->y = 0.0f;
   dest->w = ( float )scrWidth;
   dest->h = ( float )scrHgt;

   if( areaItem && X_IS_ARRAY( areaItem ) )
   {
      X_SIZE length = x_arrayLen( areaItem );
      int    left, top;

      left = x_arrayGetNI( areaItem, 1 );
      top = x_arrayGetNI( areaItem, 2 );
      dest->x = ( float )left;
      dest->y = ( float )top;

      if( length == 2 )
      {
         X_I64 width = IIF( surface, surface->w, ( X_I64 )scrWidth - left );
         X_I64 height = IIF( surface, surface->h, ( X_I64 )scrHgt - top );

         if( width <= 0 || height <= 0 || width > INT_MAX || height > INT_MAX )
            return 0;

         dest->w = ( float )width;
         dest->h = ( float )height;
      }
      else
      {
         X_I64 width, height;
         int   right, bottom;

         right = x_arrayGetNI( areaItem, 3 );
         bottom = x_arrayGetNI( areaItem, 4 );
         width = ( X_I64 )right - left + 1;
         height = ( X_I64 )bottom - top + 1;

         if( width <= 0 || height <= 0 || width > INT_MAX || height > INT_MAX )
            return 0;

         dest->w = ( float )width;
         dest->h = ( float )height;
      }
   }
   else if( surface && x_itemGetL( areaItem ) )
   {
      double scaleX = ( double )scrWidth / surface->w;
      double scaleY = ( double )scrHgt / surface->h;
      double scale = GTSDL3_MIN( scaleX, scaleY );
      int    width = GTSDL3_MAX( ( int )SDL_floor( surface->w * scale + 0.5 ), 1 );
      int    height = GTSDL3_MAX( ( int )SDL_floor( surface->h * scale + 0.5 ), 1 );

      dest->x = ( float )( ( scrWidth - width ) / 2 );
      dest->y = ( float )( ( scrHgt - height ) / 2 );
      dest->w = ( float )width;
      dest->h = ( float )height;
   }

   return dest->x < scrWidth && dest->y < scrHgt && dest->x + dest->w > 0.0f && dest->y + dest->h > 0.0f;
}

static int s_gt_sdl3_RestoreImageArea( X_GT_BASE *pGT, X_GTSDL3 *sdl, X_ITEM *areaItem )
{
   SDL_FRect dest;
   int       scrWidth = sdl->cols * X_GTSDL3_CELL_WIDTH;
   int       scrHgt = sdl->rows * X_GTSDL3_CELL_HEIGHT;
   double    rightEnd, btmEnd;
   int       left, top, right, bottom;

   if( !s_gt_sdl3_ImageDestination( sdl, NULL, areaItem, &dest ) )
      return 0;

   left = ( int )SDL_floor( GTSDL3_MAX( ( double )dest.x, 0.0 ) );
   top = ( int )SDL_floor( GTSDL3_MAX( ( double )dest.y, 0.0 ) );
   rightEnd = GTSDL3_MIN( ( double )dest.x + dest.w, ( double )scrWidth );
   btmEnd = GTSDL3_MIN( ( double )dest.y + dest.h, ( double )scrHgt );
   right = ( int )SDL_ceil( rightEnd ) - 1;
   bottom = ( int )SDL_ceil( btmEnd ) - 1;

   /* the pixel area in text cells */
   top /= X_GTSDL3_CELL_HEIGHT;
   left /= X_GTSDL3_CELL_WIDTH;
   bottom /= X_GTSDL3_CELL_HEIGHT;
   right /= X_GTSDL3_CELL_WIDTH;
   X_GTSELF_EXPOSEAREA( pGT, top, left, bottom, right );
   s_super.Refresh( pGT );

   if( X_GTSELF_DISPCOUNT( pGT ) == 0 )
      s_gt_sdl3_Present( sdl );

   return 1;
}

static int s_gt_sdl3_DispImage( X_GT_BASE *pGT, X_GTSDL3 *sdl, X_ITEM *image, X_ITEM *areaItem )
{
   SDL_Surface       *surface;
   SDL_Texture       *texture;
   SDL_FRect          dest;
   X_GTSDL3_GFX_STATE state;
   int                result;

   if( image && X_IS_STRING( image ) )
   {
      void       *hString = NULL;
      X_SIZE      length = 0;
      const char *fileName = x_itemGetStrUTF8( image, &hString, &length );

      if( length == 0 )
      {
         x_strfree( hString );
         return s_gt_sdl3_RestoreImageArea( pGT, sdl, areaItem );
      }
#if SDL_VERSION_ATLEAST( 3, 4, 0 )
      surface = SDL_LoadSurface( fileName );
#else
      surface = SDL_LoadBMP( fileName );
#endif
      x_strfree( hString );
   }
   else
      surface = s_gt_sdl3_RawImageSurface( image );

   if( !surface )
      return 0;

   if( !s_gt_sdl3_ImageDestination( sdl, surface, areaItem, &dest ) )
   {
      SDL_DestroySurface( surface );
      return 0;
   }

   {
      SDL_Surface *convert = SDL_ConvertSurface( surface, SDL_PIXELFORMAT_RGBA32 );

      SDL_DestroySurface( surface );
      surface = convert;
   }

   if( !surface )
      return 0;

   texture = SDL_CreateTextureFromSurface( sdl->renderer, surface );
   if( !texture )
   {
      SDL_DestroySurface( surface );
      return 0;
   }

   SDL_SetTextureBlendMode( texture, SDL_BLENDMODE_BLEND );
   SDL_SetTextureScaleMode( texture, SDL_SCALEMODE_LINEAR );

   s_super.Refresh( pGT );
   s_gt_sdl3_BeginGfx( sdl, &state, 0 );
   result = SDL_RenderTexture( sdl->renderer, texture, NULL, &dest );
   s_gt_sdl3_EndGfx( sdl, &state );

   if( X_GTSELF_DISPCOUNT( pGT ) == 0 )
      s_gt_sdl3_Present( sdl );

   SDL_DestroyTexture( texture );
   SDL_DestroySurface( surface );

   return result;
}

static void s_gt_sdl3_GfxLine( X_GTSDL3 *sdl, const SDL_Rect *clip, int top, int left, int bottom, int right )
{
   X_I64 dx, dy, error;
   int   sx, sy;

   if( !SDL_GetRectAndLineIntersection( clip, &left, &top, &right, &bottom ) )
      return;

   dx = IIF( left < right, ( X_I64 )right - left, ( X_I64 )left - right );
   dy = IIF( top < bottom, ( X_I64 )bottom - top, ( X_I64 )top - bottom );
   sx = IIF( left < right, 1, -1 );
   sy = IIF( top < bottom, 1, -1 );
   error = dx - dy;

   for( ;; )
   {
      X_I64 twice;

      SDL_RenderPoint( sdl->renderer, ( float )left, ( float )top );

      if( left == right && top == bottom )
         break;

      twice = error * 2;

      if( twice > -dy )
      {
         error -= dy;
         left += sx;
      }

      if( twice < dx )
      {
         error += dx;
         top += sy;
      }
   }
}

static void s_gt_sdl3_GfxFilledRect( X_GTSDL3 *sdl, const SDL_Rect *clip, int top, int left, int bottom, int right )
{
   SDL_FRect rect;
   int       value;

   if( top > bottom )
   {
      value = top;
      top = bottom;
      bottom = value;
   }

   if( left > right )
   {
      value = left;
      left = right;
      right = value;
   }

   top = GTSDL3_MAX( top, clip->y );
   left = GTSDL3_MAX( left, clip->x );
   bottom = GTSDL3_MIN( bottom, clip->y + clip->h - 1 );
   right = GTSDL3_MIN( right, clip->x + clip->w - 1 );

   if( top > bottom || left > right )
      return;

   rect.x = ( float )left;
   rect.y = ( float )top;
   rect.w = ( float )( right - left + 1 );
   rect.h = ( float )( bottom - top + 1 );
   SDL_RenderFillRect( sdl->renderer, &rect );
}

static void s_gt_sdl3_GfxRect( X_GTSDL3 *sdl, const SDL_Rect *clip, int top, int left, int bottom, int right )
{
   int value;

   if( top > bottom )
   {
      value = top;
      top = bottom;
      bottom = value;
   }

   if( left > right )
   {
      value = left;
      left = right;
      right = value;
   }

   s_gt_sdl3_GfxLine( sdl, clip, top, left, top, right );
   s_gt_sdl3_GfxLine( sdl, clip, bottom, left, bottom, right );
   s_gt_sdl3_GfxLine( sdl, clip, top, left, bottom, left );
   s_gt_sdl3_GfxLine( sdl, clip, top, right, bottom, right );
}

static int s_gt_sdl3_ClampInt( X_I64 value )
{
   if( value < INT_MIN )
      return INT_MIN;

   if( value > INT_MAX )
      return INT_MAX;

   return ( int )value;
}

static void s_gt_sdl3_GfxEllipse( X_GTSDL3 *sdl, const SDL_Rect *clip, int centerY, int centerX, int radiusY,
                                  int radiusX, int filled )
{
   X_I64 start, end;
   int   position;

   if( radiusY < 0 || radiusX < 0 )
      return;

   if( radiusY == 0 )
   {
      int left = s_gt_sdl3_ClampInt( ( X_I64 )centerX - radiusX );
      int right = s_gt_sdl3_ClampInt( ( X_I64 )centerX + radiusX );

      s_gt_sdl3_GfxLine( sdl, clip, centerY, left, centerY, right );
      return;
   }

   if( radiusX == 0 )
   {
      int top = s_gt_sdl3_ClampInt( ( X_I64 )centerY - radiusY );
      int bottom = s_gt_sdl3_ClampInt( ( X_I64 )centerY + radiusY );

      s_gt_sdl3_GfxLine( sdl, clip, top, centerX, bottom, centerX );
      return;
   }

   start = GTSDL3_MAX( ( X_I64 )clip->y, ( X_I64 )centerY - radiusY );
   end = GTSDL3_MIN( ( X_I64 )clip->y + clip->h - 1, ( X_I64 )centerY + radiusY );

   for( position = ( int )start; position <= ( int )end; ++position )
   {
      double ratio = ( ( double )position - centerY ) / radiusY;
      double value = 1.0 - ratio * ratio;
      int    offset = ( int )SDL_floor( radiusX * SDL_sqrt( GTSDL3_MAX( value, 0.0 ) ) + 0.5 );
      X_I64  left = ( X_I64 )centerX - offset;
      X_I64  right = ( X_I64 )centerX + offset;

      if( filled )
         s_gt_sdl3_GfxLine( sdl, clip, position, s_gt_sdl3_ClampInt( left ), position, s_gt_sdl3_ClampInt( right ) );
      else
      {
         SDL_RenderPoint( sdl->renderer, ( float )left, ( float )position );

         if( right != left )
            SDL_RenderPoint( sdl->renderer, ( float )right, ( float )position );
      }
   }

   if( !filled )
   {
      start = GTSDL3_MAX( ( X_I64 )clip->x, ( X_I64 )centerX - radiusX );
      end = GTSDL3_MIN( ( X_I64 )clip->x + clip->w - 1, ( X_I64 )centerX + radiusX );

      for( position = ( int )start; position <= ( int )end; ++position )
      {
         double ratio = ( ( double )position - centerX ) / radiusX;
         double value = 1.0 - ratio * ratio;
         int    offset = ( int )SDL_floor( radiusY * SDL_sqrt( GTSDL3_MAX( value, 0.0 ) ) + 0.5 );
         X_I64  top = ( X_I64 )centerY - offset;
         X_I64  bottom = ( X_I64 )centerY + offset;

         SDL_RenderPoint( sdl->renderer, ( float )position, ( float )top );

         if( bottom != top )
            SDL_RenderPoint( sdl->renderer, ( float )position, ( float )bottom );
      }
   }
}

static Uint32 *s_gt_sdl3_SurfacePixel( SDL_Surface *surface, int x, int y )
{
   return &( ( Uint32 * )( ( Uint8 * )surface->pixels + y * surface->pitch ) )[x];
}

static int s_gt_sdl3_GfxFloodFill( X_GTSDL3 *sdl, const SDL_Rect *clip, int startY, int startX, int color )
{
   SDL_Surface                  *readback;
   SDL_Surface                  *surface;
   SDL_Texture                  *texture;
   SDL_FRect                     dest;
   X_SIZE                       *stack;
   X_SIZE                        used = 0;
   Uint32                        srcColor, dstColor;
   int                           result;
   Uint8                         red = ( Uint8 )( ( color >> 16 ) & 0xFF );
   Uint8                         green = ( Uint8 )( ( color >> 8 ) & 0xFF );
   Uint8                         blue = ( Uint8 )( color & 0xFF );
   const SDL_PixelFormatDetails *format = SDL_GetPixelFormatDetails( SDL_PIXELFORMAT_RGBA32 );

   if( startX < clip->x || startX >= clip->x + clip->w || startY < clip->y || startY >= clip->y + clip->h )
      return 1;

   readback = SDL_RenderReadPixels( sdl->renderer, NULL );
   surface = SDL_ConvertSurface( readback, SDL_PIXELFORMAT_RGBA32 );
   SDL_DestroySurface( readback );

   srcColor = *s_gt_sdl3_SurfacePixel( surface, startX, startY );
   dstColor = SDL_MapRGBA( format, NULL, red, green, blue, SDL_ALPHA_OPAQUE );
   if( srcColor == dstColor )
   {
      SDL_DestroySurface( surface );
      return 1;
   }

   stack = ( X_SIZE * )x_xgrabArray( ( X_SIZE )surface->w * surface->h, sizeof( X_SIZE ) );
   *s_gt_sdl3_SurfacePixel( surface, startX, startY ) = dstColor;
   stack[used++] = ( X_SIZE )startY * surface->w + startX;

   while( used > 0 )
   {
      X_SIZE index = stack[--used];
      int    x = ( int )( index % surface->w );
      int    y = ( int )( index / surface->w );

#define X_GTSDL3_FLOOD_NEIGHBOR( nx, ny )                                                                              \
   do                                                                                                                  \
   {                                                                                                                   \
      Uint32 *pixel = s_gt_sdl3_SurfacePixel( surface, ( nx ), ( ny ) );                                               \
      if( *pixel == srcColor )                                                                                         \
      {                                                                                                                \
         *pixel = dstColor;                                                                                            \
         stack[used++] = ( X_SIZE )( ny ) * surface->w + ( nx );                                                       \
      }                                                                                                                \
   } while( 0 )

      if( x > clip->x )
         X_GTSDL3_FLOOD_NEIGHBOR( x - 1, y );
      if( x + 1 < clip->x + clip->w )
         X_GTSDL3_FLOOD_NEIGHBOR( x + 1, y );
      if( y > clip->y )
         X_GTSDL3_FLOOD_NEIGHBOR( x, y - 1 );
      if( y + 1 < clip->y + clip->h )
         X_GTSDL3_FLOOD_NEIGHBOR( x, y + 1 );

#undef X_GTSDL3_FLOOD_NEIGHBOR
   }

   texture = SDL_CreateTextureFromSurface( sdl->renderer, surface );
   dest.x = dest.y = 0.0f;
   dest.w = ( float )surface->w;
   dest.h = ( float )surface->h;
   SDL_SetTextureBlendMode( texture, SDL_BLENDMODE_NONE );
   SDL_SetTextureScaleMode( texture, SDL_SCALEMODE_NEAREST );
   result = SDL_RenderTexture( sdl->renderer, texture, NULL, &dest );
   SDL_DestroyTexture( texture );
   x_xfree( stack );
   SDL_DestroySurface( surface );

   return result;
}

static int s_gt_sdl3_GfxGetPixel( X_GT_BASE *pGT, X_GTSDL3 *sdl, int top, int left )
{
   X_GTSDL3_GFX_STATE state;
   SDL_Surface       *surface;
   SDL_Rect           rect = { left, top, 1, 1 };
   Uint8              red, green, blue;

   if( top < 0 || left < 0 || top >= sdl->rows * X_GTSDL3_CELL_HEIGHT || left >= sdl->cols * X_GTSDL3_CELL_WIDTH )
      return 0;

   s_super.Refresh( pGT );
   s_gt_sdl3_BeginGfxOperation( sdl, &state, 0 );
   surface = SDL_RenderReadPixels( sdl->renderer, &rect );
   SDL_ReadSurfacePixel( surface, 0, 0, &red, &green, &blue, NULL );
   SDL_DestroySurface( surface );
   s_gt_sdl3_EndGfxOperation( sdl, &state );

   return ( red << 16 ) | ( green << 8 ) | blue;
}

static int s_gt_sdl3_GfxDraw( X_GT_BASE *pGT, X_GTSDL3 *sdl, int iType, int iTop, int iLeft, int iBottom, int iRight,
                              int iColor )
{
   X_GTSDL3_GFX_STATE state;
   SDL_Rect           clip;
   int                hasClip;
   int                result;
   int                color = iColor;

   if( iType == GFX_PUTPIXEL || iType == GFX_FLOODFILL )
      color = iBottom;
   else if( iType == GFX_CIRCLE || iType == GFX_FILLEDCIRCLE )
      color = iRight;

   s_super.Refresh( pGT );
   s_gt_sdl3_BeginGfxOperation( sdl, &state, iType != GFX_FLOODFILL );

   hasClip = s_gt_sdl3_GetGfxClip( sdl, &clip );

   if( !hasClip )
      result = 1;
   else if( iType == GFX_FLOODFILL )
      result = s_gt_sdl3_GfxFloodFill( sdl, &clip, iTop, iLeft, color );
   else
   {
      Uint8 red = ( Uint8 )( color >> 16 );
      Uint8 green = ( Uint8 )( color >> 8 );
      Uint8 blue = ( Uint8 )color;

      SDL_SetRenderDrawColor( sdl->renderer, red, green, blue, SDL_ALPHA_OPAQUE );
      result = 1;

      switch( iType )
      {
         case GFX_PUTPIXEL: result = SDL_RenderPoint( sdl->renderer, ( float )iLeft, ( float )iTop ); break;
         case GFX_LINE: s_gt_sdl3_GfxLine( sdl, &clip, iTop, iLeft, iBottom, iRight ); break;
         case GFX_RECT: s_gt_sdl3_GfxRect( sdl, &clip, iTop, iLeft, iBottom, iRight ); break;
         case GFX_FILLEDRECT: s_gt_sdl3_GfxFilledRect( sdl, &clip, iTop, iLeft, iBottom, iRight ); break;
         case GFX_CIRCLE: s_gt_sdl3_GfxEllipse( sdl, &clip, iTop, iLeft, iBottom, iBottom, 0 ); break;
         case GFX_FILLEDCIRCLE: s_gt_sdl3_GfxEllipse( sdl, &clip, iTop, iLeft, iBottom, iBottom, 1 ); break;
         case GFX_ELLIPSE: s_gt_sdl3_GfxEllipse( sdl, &clip, iTop, iLeft, iBottom, iRight, 0 ); break;
         case GFX_FILLEDELLIPSE: s_gt_sdl3_GfxEllipse( sdl, &clip, iTop, iLeft, iBottom, iRight, 1 ); break;
      }
   }

   s_gt_sdl3_EndGfxOperation( sdl, &state );

   return result;
}

static int s_gt_sdl3_gfx_Primitive( X_GT_BASE *pGT, int iType, int iTop, int iLeft, int iBottom, int iRight,
                                    int iColor )
{
   X_GTSDL3 *sdl = X_GTSDL3_GET( pGT );
   int       fHandled = 1;
   int       iResult = 0;

   switch( iType )
   {
      case GFX_ACQUIRESCREEN:
         s_super.Refresh( pGT );

         if( sdl->gfxDepth == 0 )
         {
            s_gt_sdl3_BeginGfx( sdl, &sdl->gfxState, 0 );
            sdl->gfxOn = 1;
         }

         ++sdl->gfxDepth;
         X_GTSELF_DISPBEGIN( pGT );
         iResult = 1;
         break;
      case GFX_RELEASESCREEN:
         if( sdl->gfxDepth > 0 )
         {
            --sdl->gfxDepth;

            if( sdl->gfxDepth == 0 && sdl->gfxOn )
            {
               s_gt_sdl3_EndGfx( sdl, &sdl->gfxState );
               sdl->gfxOn = 0;
            }
         }

         iResult = 1;
         X_GTSELF_DISPEND( pGT );
         break;

      case GFX_MAKECOLOR:
      {
         int red = GTSDL3_MIN( GTSDL3_MAX( iTop, 0 ), 255 );
         int green = GTSDL3_MIN( GTSDL3_MAX( iLeft, 0 ), 255 );
         int blue = GTSDL3_MIN( GTSDL3_MAX( iBottom, 0 ), 255 );

         iResult = ( red << 16 ) | ( green << 8 ) | blue;
         break;
      }

      case GFX_CLIPTOP: iResult = sdl->clipTop; break;
      case GFX_CLIPLEFT: iResult = sdl->clipLeft; break;
      case GFX_CLIPBOTTOM: iResult = sdl->clipBtm; break;
      case GFX_CLIPRIGHT: iResult = sdl->clipRgt; break;
      case GFX_SETCLIP:
         /* hbgfx.ch defines inclusive top, left, bottom and right
          * coordinates, consistently with GTALLEG and all other
          * coordinate-based X_GFX_* primitives. */
         if( iTop == 0 && iLeft == 0 && iBottom == 0 && iRight == 0 )
         {
            sdl->clipTop = 0;
            sdl->clipLeft = 0;
            sdl->clipBtm = 0;
            sdl->clipRgt = 0;
            sdl->clipOn = 0;
         }
         else
         {
            sdl->clipTop = GTSDL3_MIN( iTop, iBottom );
            sdl->clipLeft = GTSDL3_MIN( iLeft, iRight );
            sdl->clipBtm = GTSDL3_MAX( iTop, iBottom );
            sdl->clipRgt = GTSDL3_MAX( iLeft, iRight );
            sdl->clipOn = 1;
         }

         iResult = 1;
         break;

      /* SDL3 implements the portable SOLID drawing mode. */
      case GFX_DRAWINGMODE: iResult = GFX_MODE_SOLID; break;

      case GFX_GETPIXEL: iResult = s_gt_sdl3_GfxGetPixel( pGT, sdl, iTop, iLeft ); break;
      case GFX_PUTPIXEL:
      case GFX_LINE:
      case GFX_RECT:
      case GFX_FILLEDRECT:
      case GFX_CIRCLE:
      case GFX_FILLEDCIRCLE:
      case GFX_ELLIPSE:
      case GFX_FILLEDELLIPSE:
      case GFX_FLOODFILL: iResult = s_gt_sdl3_GfxDraw( pGT, sdl, iType, iTop, iLeft, iBottom, iRight, iColor ); break;

      default: fHandled = 0; break;
   }

   return IIF( fHandled, iResult, s_super.GfxPrimitive( pGT, iType, iTop, iLeft, iBottom, iRight, iColor ) );
}

static void s_gt_sdl3_gfx_Text( X_GT_BASE *pGT, int iTop, int iLeft, const char *text, int iColor, int iSize,
                                int iWidth )
{
   X_GTSDL3          *sdl = X_GTSDL3_GET( pGT );
   int                scrWidth = sdl->cols * X_GTSDL3_CELL_WIDTH;
   int                scrHgt = sdl->rows * X_GTSDL3_CELL_HEIGHT;
   int                height = IIF( iSize == 0, X_GTSDL3_CELL_HEIGHT, GTSDL3_MIN( iSize, scrHgt ) );
   int                width = IIF( iWidth == 0, GTSDL3_MAX( height / 2, 1 ), GTSDL3_MIN( iWidth, scrWidth ) );
   X_GTSDL3_GFX_STATE state;
   X_WCHAR            stackBuf[X_GTSDL3_GFX_TEXT_STACK];
   X_SIZE             srcLen = strlen( text );
   X_WCHAR           *wideText;
   X_SIZE             length;
   X_SIZE             index;
   X_I64              left = iLeft;

   if( ( X_I64 )iTop + height <= 0 || iTop >= scrHgt )
      return;

   wideText = stackBuf;
   if( srcLen >= X_GTSDL3_GFX_TEXT_STACK )
      wideText = ( X_WCHAR * )x_xgrabArray( srcLen + 1, sizeof( X_WCHAR ) );
   length = x_utf8ToU16( text, srcLen, wideText, srcLen + 1 );
   s_super.Refresh( pGT );
   s_gt_sdl3_BeginGfxOperation( sdl, &state, 1 );
   SDL_SetTextureColorMod( sdl->font, ( Uint8 )( iColor >> 16 ), ( Uint8 )( iColor >> 8 ), ( Uint8 )iColor );
   sdl->fgColor = -1;

   for( index = 0; index < length && left < scrWidth; ++index, left += width )
   {
      if( wideText[index] == '\r' || wideText[index] == '\n' )
         break;
      if( left + width > 0 )
         s_gt_sdl3_DrawGlyph( sdl, wideText[index], iTop, ( int )left, height, width );
   }

   s_gt_sdl3_EndGfxOperation( sdl, &state );

   if( wideText != stackBuf )
      x_xfree( wideText );
}

static int s_gt_sdl3_FuncInit( X_GT_FUNCS *pFuncTable )
{
   pFuncTable->Init = s_gt_sdl3_Init;
   pFuncTable->Exit = s_gt_sdl3_Exit;
   pFuncTable->SetMode = s_gt_sdl3_SetMode;
   pFuncTable->Redraw = s_gt_sdl3_Redraw;
   pFuncTable->Refresh = s_gt_sdl3_Refresh;
   pFuncTable->Version = s_gt_sdl3_Version;
   pFuncTable->Info = s_gt_sdl3_Info;
   pFuncTable->Alert = s_gt_sdl3_Alert;
   pFuncTable->ReadKey = s_gt_sdl3_ReadKey;
   pFuncTable->TrueColor = s_gt_sdl3_TrueColor;

   pFuncTable->MouseIsPresent = s_gt_sdl3_mouse_IsPresent;
   pFuncTable->MouseGetPos = s_gt_sdl3_mouse_GetPos;
   pFuncTable->MouseSetPos = s_gt_sdl3_mouse_SetPos;
   pFuncTable->MouseButtonState = s_gt_sdl3_mouse_ButtonState;
   pFuncTable->MouseCountButton = s_gt_sdl3_mouse_CountButton;

   pFuncTable->GfxPrimitive = s_gt_sdl3_gfx_Primitive;
   pFuncTable->GfxText = s_gt_sdl3_gfx_Text;

   return 1;
}

static const X_GT_INIT s_gtInit = { "SDL3", s_gt_sdl3_FuncInit, &s_super, &s_GtId };

static void __attribute__( ( constructor ) ) s_gt_sdl3_Register( void )
{
   x_gtRegister( &s_gtInit );
}

/* Linker anchor used by REQUEST GTSDL3. */
X_FUNC( GTSDL3 )
{
   UNUSED( ctx );
}
