/*
 * Copyright (c) 2024-2026 xDev-Language
 *
 * Library...: xDev GTWVT
 * File......: gtwvt.c
 *
 */

#include "gtwvt.h"

#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#define X_KF_ALTGR 0x10

#define _WVT_WS_DEF ( WS_OVERLAPPEDWINDOW )
#define _WVT_WS_NORESIZE ( WS_OVERLAPPEDWINDOW & ~( WS_THICKFRAME ) )

#define X_GTWVT_GET( gt ) ( ( X_GTWVT * )( gt )->driverData[s_gtId] )

#define X_GTWVT_IS( item, type ) ( ( item ) != NULL && ( X_ITEM_TYPERAW( item ) & ( type ) ) != 0 )

static int        s_gtId;
static X_GT_FUNCS s_super;

static X_GTWVT *s_windows[WVT_MAX_WINDOWS];
static int      s_windowCount = 0;

static const wchar_t s_className[] = L"xDev_WVT_Class";

/* The same default palette as GTSDL3. */
static const uint8_t s_defaultColors[16][3] = {
   { 0, 0, 0 },     { 0, 0, 170 },     { 0, 170, 0 },    { 0, 170, 170 },  { 170, 0, 0 },   { 170, 0, 170 },
   { 170, 85, 0 },  { 170, 170, 170 }, { 85, 85, 85 },   { 85, 85, 255 },  { 85, 255, 85 }, { 85, 255, 255 },
   { 255, 85, 85 }, { 255, 85, 255 },  { 255, 255, 85 }, { 255, 255, 255 }
};

/* CP437 glyphs shown for control characters, unless X_GTI_FONTA_CTRLCHARS is set. */
static const uint16_t s_ctrlChars[32] = { 0x0020, 0x263A, 0x263B, 0x2665, 0x2666, 0x2663, 0x2660, 0x2022,
                                          0x25D8, 0x25CB, 0x25D9, 0x2642, 0x2640, 0x266A, 0x266B, 0x263C,
                                          0x25BA, 0x25C4, 0x2195, 0x203C, 0x00B6, 0x00A7, 0x25AC, 0x21A8,
                                          0x2191, 0x2193, 0x2192, 0x2190, 0x221F, 0x2194, 0x25B2, 0x25BC };

/* Alt+<key> arrives as WM_SYSCHAR; the scan code gives the key independently of the layout. */
static const char s_scanKeys[] = "\0\0"
                                 "1234567890\0=\0\0"
                                 "QWERTYUIOP\0\0\0\0"
                                 "ASDFGHJKL\0\0\0\0\0"
                                 "ZXCVBNM";

/* X_GTI_KBDSHIFTS: pressed keys are tested with 0x8000, lock states with 0x01. */
static const struct
{
   int vk;
   int flag;
   int mask;
} s_kbdKeys[] = { { VK_SHIFT, X_GTI_KBD_SHIFT, 0x8000 },    { VK_CONTROL, X_GTI_KBD_CTRL, 0x8000 },
                  { VK_MENU, X_GTI_KBD_ALT, 0x8000 },       { VK_LSHIFT, X_GTI_KBD_LSHIFT, 0x8000 },
                  { VK_RSHIFT, X_GTI_KBD_RSHIFT, 0x8000 },  { VK_LCONTROL, X_GTI_KBD_LCTRL, 0x8000 },
                  { VK_RCONTROL, X_GTI_KBD_RCTRL, 0x8000 }, { VK_LMENU, X_GTI_KBD_LALT, 0x8000 },
                  { VK_RMENU, X_GTI_KBD_RALT, 0x8000 },     { VK_LWIN, X_GTI_KBD_LWIN, 0x8000 },
                  { VK_RWIN, X_GTI_KBD_RWIN, 0x8000 },      { VK_APPS, X_GTI_KBD_MENU, 0x8000 },
                  { VK_SCROLL, X_GTI_KBD_SCROLOCK, 0x01 },  { VK_NUMLOCK, X_GTI_KBD_NUMLOCK, 0x01 },
                  { VK_CAPITAL, X_GTI_KBD_CAPSLOCK, 0x01 }, { VK_INSERT, X_GTI_KBD_INSERT, 0x01 } };

/* X_GTI_FONTA_DRAWBOX: the arms of the box drawing characters U+2500 to U+257F, two bits for each
   arm from the centre of the cell (up, right, down, left): 0 none, 1 light, 2 heavy, 3 double.
   Dashed lines, arcs and diagonals take their arms from here and are drawn separately. */
#define X_BOX( up, right, down, left ) ( ( up ) | ( right ) << 2 | ( down ) << 4 | ( left ) << 6 )
static const uint8_t s_boxArms[0x80] = {
   X_BOX( 0, 1, 0, 1 ),
   X_BOX( 0, 2, 0, 2 ),
   X_BOX( 1, 0, 1, 0 ),
   X_BOX( 2, 0, 2, 0 ), /* ─ ━ │ ┃ */
   X_BOX( 0, 1, 0, 1 ),
   X_BOX( 0, 2, 0, 2 ),
   X_BOX( 1, 0, 1, 0 ),
   X_BOX( 2, 0, 2, 0 ), /* ┄ ┅ ┆ ┇ */
   X_BOX( 0, 1, 0, 1 ),
   X_BOX( 0, 2, 0, 2 ),
   X_BOX( 1, 0, 1, 0 ),
   X_BOX( 2, 0, 2, 0 ), /* ┈ ┉ ┊ ┋ */
   X_BOX( 0, 1, 1, 0 ),
   X_BOX( 0, 2, 1, 0 ),
   X_BOX( 0, 1, 2, 0 ),
   X_BOX( 0, 2, 2, 0 ), /* ┌ ┍ ┎ ┏ */
   X_BOX( 0, 0, 1, 1 ),
   X_BOX( 0, 0, 1, 2 ),
   X_BOX( 0, 0, 2, 1 ),
   X_BOX( 0, 0, 2, 2 ), /* ┐ ┑ ┒ ┓ */
   X_BOX( 1, 1, 0, 0 ),
   X_BOX( 1, 2, 0, 0 ),
   X_BOX( 2, 1, 0, 0 ),
   X_BOX( 2, 2, 0, 0 ), /* └ ┕ ┖ ┗ */
   X_BOX( 1, 0, 0, 1 ),
   X_BOX( 1, 0, 0, 2 ),
   X_BOX( 2, 0, 0, 1 ),
   X_BOX( 2, 0, 0, 2 ), /* ┘ ┙ ┚ ┛ */
   X_BOX( 1, 1, 1, 0 ),
   X_BOX( 1, 2, 1, 0 ),
   X_BOX( 2, 1, 1, 0 ),
   X_BOX( 1, 1, 2, 0 ), /* ├ ┝ ┞ ┟ */
   X_BOX( 2, 1, 2, 0 ),
   X_BOX( 2, 2, 1, 0 ),
   X_BOX( 1, 2, 2, 0 ),
   X_BOX( 2, 2, 2, 0 ), /* ┠ ┡ ┢ ┣ */
   X_BOX( 1, 0, 1, 1 ),
   X_BOX( 1, 0, 1, 2 ),
   X_BOX( 2, 0, 1, 1 ),
   X_BOX( 1, 0, 2, 1 ), /* ┤ ┥ ┦ ┧ */
   X_BOX( 2, 0, 2, 1 ),
   X_BOX( 2, 0, 1, 2 ),
   X_BOX( 1, 0, 2, 2 ),
   X_BOX( 2, 0, 2, 2 ), /* ┨ ┩ ┪ ┫ */
   X_BOX( 0, 1, 1, 1 ),
   X_BOX( 0, 1, 1, 2 ),
   X_BOX( 0, 2, 1, 1 ),
   X_BOX( 0, 2, 1, 2 ), /* ┬ ┭ ┮ ┯ */
   X_BOX( 0, 1, 2, 1 ),
   X_BOX( 0, 1, 2, 2 ),
   X_BOX( 0, 2, 2, 1 ),
   X_BOX( 0, 2, 2, 2 ), /* ┰ ┱ ┲ ┳ */
   X_BOX( 1, 1, 0, 1 ),
   X_BOX( 1, 1, 0, 2 ),
   X_BOX( 1, 2, 0, 1 ),
   X_BOX( 1, 2, 0, 2 ), /* ┴ ┵ ┶ ┷ */
   X_BOX( 2, 1, 0, 1 ),
   X_BOX( 2, 1, 0, 2 ),
   X_BOX( 2, 2, 0, 1 ),
   X_BOX( 2, 2, 0, 2 ), /* ┸ ┹ ┺ ┻ */
   X_BOX( 1, 1, 1, 1 ),
   X_BOX( 1, 1, 1, 2 ),
   X_BOX( 1, 2, 1, 1 ),
   X_BOX( 1, 2, 1, 2 ), /* ┼ ┽ ┾ ┿ */
   X_BOX( 2, 1, 1, 1 ),
   X_BOX( 1, 1, 2, 1 ),
   X_BOX( 2, 1, 2, 1 ),
   X_BOX( 2, 1, 1, 2 ), /* ╀ ╁ ╂ ╃ */
   X_BOX( 2, 2, 1, 1 ),
   X_BOX( 1, 1, 2, 2 ),
   X_BOX( 1, 2, 2, 1 ),
   X_BOX( 2, 2, 1, 2 ), /* ╄ ╅ ╆ ╇ */
   X_BOX( 1, 2, 2, 2 ),
   X_BOX( 2, 1, 2, 2 ),
   X_BOX( 2, 2, 2, 1 ),
   X_BOX( 2, 2, 2, 2 ), /* ╈ ╉ ╊ ╋ */
   X_BOX( 0, 1, 0, 1 ),
   X_BOX( 0, 2, 0, 2 ),
   X_BOX( 1, 0, 1, 0 ),
   X_BOX( 2, 0, 2, 0 ), /* ╌ ╍ ╎ ╏ */
   X_BOX( 0, 3, 0, 3 ),
   X_BOX( 3, 0, 3, 0 ),
   X_BOX( 0, 3, 1, 0 ),
   X_BOX( 0, 1, 3, 0 ), /* ═ ║ ╒ ╓ */
   X_BOX( 0, 3, 3, 0 ),
   X_BOX( 0, 0, 1, 3 ),
   X_BOX( 0, 0, 3, 1 ),
   X_BOX( 0, 0, 3, 3 ), /* ╔ ╕ ╖ ╗ */
   X_BOX( 1, 3, 0, 0 ),
   X_BOX( 3, 1, 0, 0 ),
   X_BOX( 3, 3, 0, 0 ),
   X_BOX( 1, 0, 0, 3 ), /* ╘ ╙ ╚ ╛ */
   X_BOX( 3, 0, 0, 1 ),
   X_BOX( 3, 0, 0, 3 ),
   X_BOX( 1, 3, 1, 0 ),
   X_BOX( 3, 1, 3, 0 ), /* ╜ ╝ ╞ ╟ */
   X_BOX( 3, 3, 3, 0 ),
   X_BOX( 1, 0, 1, 3 ),
   X_BOX( 3, 0, 3, 1 ),
   X_BOX( 3, 0, 3, 3 ), /* ╠ ╡ ╢ ╣ */
   X_BOX( 0, 3, 1, 3 ),
   X_BOX( 0, 1, 3, 1 ),
   X_BOX( 0, 3, 3, 3 ),
   X_BOX( 1, 3, 0, 3 ), /* ╤ ╥ ╦ ╧ */
   X_BOX( 3, 1, 0, 1 ),
   X_BOX( 3, 3, 0, 3 ),
   X_BOX( 1, 3, 1, 3 ),
   X_BOX( 3, 1, 3, 1 ), /* ╨ ╩ ╪ ╫ */
   X_BOX( 3, 3, 3, 3 ),
   X_BOX( 0, 1, 1, 0 ),
   X_BOX( 0, 0, 1, 1 ),
   X_BOX( 1, 0, 0, 1 ), /* ╬ ╭ ╮ ╯ */
   X_BOX( 1, 1, 0, 0 ),
   0,
   0,
   0, /* ╰ ╱ ╲ ╳ */
   X_BOX( 0, 0, 0, 1 ),
   X_BOX( 1, 0, 0, 0 ),
   X_BOX( 0, 1, 0, 0 ),
   X_BOX( 0, 0, 1, 0 ), /* ╴ ╵ ╶ ╷ */
   X_BOX( 0, 0, 0, 2 ),
   X_BOX( 2, 0, 0, 0 ),
   X_BOX( 0, 2, 0, 0 ),
   X_BOX( 0, 0, 2, 0 ), /* ╸ ╹ ╺ ╻ */
   X_BOX( 0, 2, 0, 1 ),
   X_BOX( 1, 0, 2, 0 ),
   X_BOX( 0, 1, 0, 2 ),
   X_BOX( 2, 0, 1, 0 ) /* ╼ ╽ ╾ ╿ */
};
#undef X_BOX

/* X_GTI_FONTA_DRAWBOX: quadrants of U+2596 to U+259F, 1 upper left, 2 upper right, 4 lower left,
   8 lower right */
static const uint8_t s_boxQuadrants[10] = { 4, 8, 1, 13, 9, 7, 11, 2, 6, 14 };

/* X_GTI_DISPIMAGE colors of 1 bit pixels 0 and 1, as the default palette of SDL in GTSDL3 */
static const uint32_t s_monoColors[2] = { 0xFFFFFFFFu, 0xFF000000u };

/* the four neighbours visited by GFX_FLOODFILL */
static const int s_floodDX[4] = { -1, 1, 0, 0 };
static const int s_floodDY[4] = { 0, 0, -1, 1 };

static LRESULT CALLBACK s_gt_wvt_WndProc( HWND window, UINT message, WPARAM wParam, LPARAM lParam );
static int              s_gt_wvt_SetFullscreen( X_GT_BASE *gt );
static void             s_gt_wvt_BeginGfx( X_GT_BASE *gt, X_GTWVT *wvt );

/* ---------------------------------------------------------------------------------------------
   UTF-8 and UTF-16 text, clipboard
   --------------------------------------------------------------------------------------------- */

/* Returns an x_xgrab() buffer terminated by zero. */
static wchar_t *s_gt_wvt_ToWide( const char *text, size_t length, int *wideLen )
{
   size_t   count;
   wchar_t *wide;

   _Static_assert( sizeof( wchar_t ) == sizeof( uint16_t ), "GTWVT requires 16-bit wchar_t" );
   wide = ( wchar_t * )x_utf8ToUtf16Alloc( text, length, &count );
   if( count > INT_MAX )
   {
      x_xfree( wide );
      fputs( "xDev GTWVT: converted text is too long\n", stderr );
      x_quitRaise();
   }
   if( wideLen )
      *wideLen = ( int )count;

   return wide;
}

static wchar_t *s_gt_wvt_ItemToWide( X_ITEM *item )
{
   return s_gt_wvt_ToWide( x_itemGetCPtr( item ), x_itemGetCLen( item ), NULL );
}

static X_ITEM *s_gt_wvt_PutWide( X_ITEM *item, const wchar_t *text, int length )
{
   size_t count;
   char  *utf8 = x_utf16ToUtf8Alloc( ( const uint16_t * )text, ( size_t )X_MAX( length, 0 ), &count );

   return x_itemPutCLPtr( item, utf8, count );
}

static void s_gt_wvt_SetClipboard( const wchar_t *text, size_t length )
{
   if( OpenClipboard( NULL ) )
   {
      HGLOBAL memory = GlobalAlloc( GMEM_MOVEABLE, ( length + 1 ) * sizeof( wchar_t ) );

      EmptyClipboard();
      if( memory )
      {
         wchar_t *target = ( wchar_t * )GlobalLock( memory );

         memcpy( target, text, length * sizeof( wchar_t ) );
         target[length] = L'\0';
         GlobalUnlock( memory );
         if( !SetClipboardData( CF_UNICODETEXT, memory ) )
            GlobalFree( memory );
      }
      CloseClipboard();
   }
}

/* Puts the clipboard text into item, an empty string without text; returns whether text was read. */
static int s_gt_wvt_GetClipboard( X_ITEM *item )
{
   int read = 0;

   x_itemPutC( item, "" );
   if( IsClipboardFormatAvailable( CF_UNICODETEXT ) && OpenClipboard( NULL ) )
   {
      HGLOBAL memory = GetClipboardData( CF_UNICODETEXT );

      if( memory )
      {
         const wchar_t *text = ( const wchar_t * )GlobalLock( memory );

         if( text )
         {
            s_gt_wvt_PutWide( item, text, ( int )wcslen( text ) );
            GlobalUnlock( memory );
            read = 1;
         }
      }
      CloseClipboard();
   }

   return read;
}

/* ---------------------------------------------------------------------------------------------
   Canvas: one bitmap shared by text cells and x_gfx*(), like the GTSDL3 screen texture
   --------------------------------------------------------------------------------------------- */

static COLORREF s_gt_wvt_Color( X_GTWVT *wvt, int index )
{
   const uint8_t *rgb = wvt->colors[index & 0x0F];

   return RGB( rgb[0], rgb[1], rgb[2] );
}

/* colors of a cell as 0xRRGGBB: the 24-bit colors of DispOutAtExt() and DispBoxExt(), otherwise
   the palette entries of its color attribute */
static void s_gt_wvt_CellColors( X_GTWVT *wvt, int row, int col, int color, uint32_t *fore, uint32_t *back )
{
   const X_GT_CELL *cell = &wvt->gt->screenBuffer[( size_t )row * wvt->gt->cols + col];
   const uint8_t   *rgb;

   rgb = wvt->colors[color & 0x0F];
   *fore = cell->foreground;
   if( *fore == X_GT_RGB_DEFAULT )
      *fore = ( uint32_t )rgb[0] << 16 | ( uint32_t )rgb[1] << 8 | rgb[2];
   rgb = wvt->colors[( color >> 4 ) & 0x0F];
   *back = cell->background;
   if( *back == X_GT_RGB_DEFAULT )
      *back = ( uint32_t )rgb[0] << 16 | ( uint32_t )rgb[1] << 8 | rgb[2];
}

/* the window rarely holds whole cells (maximized, full screen, before the grid follows the window):
   the area outside the grid takes the SetColorExt() background, otherwise the background of the
   SetColor() standard colour, as the margin of a terminal */
static void s_gt_wvt_FillEdges( X_GTWVT *wvt, HDC hdc, const RECT *paint )
{
   uint32_t back = wvt->gt->background;
   COLORREF color = s_gt_wvt_Color( wvt, wvt->gt->colors[0] >> 4 );

   if( back != X_GT_RGB_DEFAULT )
      color = RGB( ( back >> 16 ) & 0xFF, ( back >> 8 ) & 0xFF, back & 0xFF );
   SetDCBrushColor( hdc, color );
   FillRect( hdc, paint, ( HBRUSH )GetStockObject( DC_BRUSH ) );
}

/* As in GTSDL3 and GTGLFW, programs use logical pixels of 1/96 inch, while the window and the
   canvas have the pixels of the monitor, so text stays sharp on a scaled display. */
static int s_gt_wvt_ToPixel( X_GTWVT *wvt, double logical )
{
   return ( int )floor( logical * wvt->dpi / 96.0 );
}

/* sizes in pixels of the window to logical sizes, rounded */
static int s_gt_wvt_ToLogical( X_GTWVT *wvt, double pixel )
{
   return ( int )floor( pixel * 96.0 / wvt->dpi + 0.5 );
}

/* The canvas (graphics, the clip, GfxText and the screen size) is measured in logical cells: a cell
   has GTI_FONTWIDTH x GTI_FONTSIZE logical pixels, as in GTSDL3 and GTGLFW, so graphics meet the
   text cells exactly also on a scaled display. */
static int s_gt_wvt_CellLogical( X_GTWVT *wvt, int vertical )
{
   return X_MAX( s_gt_wvt_ToLogical( wvt, IIF( vertical, wvt->cellSize.y, wvt->cellSize.x ) ), 1 );
}

static int s_gt_wvt_CanvasToPixel( X_GTWVT *wvt, double logical, int vertical )
{
   int cell = IIF( vertical, wvt->cellSize.y, wvt->cellSize.x );

   return ( int )floor( logical * cell / s_gt_wvt_CellLogical( wvt, vertical ) );
}

static int s_gt_wvt_CanvasToLogical( X_GTWVT *wvt, double pixel, int vertical )
{
   int cell = IIF( vertical, wvt->cellSize.y, wvt->cellSize.x );

   return ( int )floor( pixel * s_gt_wvt_CellLogical( wvt, vertical ) / cell + 0.5 );
}

static void s_gt_wvt_DestroyCanvas( X_GTWVT *wvt )
{
   if( wvt->canvasDc )
   {
      SelectObject( wvt->canvasDc, wvt->canvasOld );
      SelectObject( wvt->canvasDc, GetStockObject( SYSTEM_FONT ) );
      DeleteDC( wvt->canvasDc );
      DeleteObject( wvt->canvasBitmap );
      wvt->canvasDc = NULL;
      wvt->canvasBitmap = NULL;
      wvt->pixels = NULL;
   }
}

/* Replaces the cell font; the canvas holding the old font is released first. */
static void s_gt_wvt_SetFont( X_GTWVT *wvt, HFONT font )
{
   if( font != wvt->font )
   {
      s_gt_wvt_DestroyCanvas( wvt );
      if( wvt->font )
         DeleteObject( wvt->font );
      wvt->font = font;
   }
}

/* A new canvas has the current grid size and an empty screen; text is repainted, graphics are lost. */
static void s_gt_wvt_CreateCanvas( X_GTWVT *wvt )
{
   BITMAPINFO bmi;
   HDC        hdc;
   void      *bits = NULL;
   int        width = wvt->cols * wvt->cellSize.x;
   int        height = wvt->rows * wvt->cellSize.y;
   uint32_t   bg;
   size_t     i, count;

   s_gt_wvt_DestroyCanvas( wvt );

   memset( &bmi, 0, sizeof( bmi ) );
   bmi.bmiHeader.biSize = sizeof( BITMAPINFOHEADER );
   bmi.bmiHeader.biWidth = width;
   bmi.bmiHeader.biHeight = -height;
   bmi.bmiHeader.biPlanes = 1;
   bmi.bmiHeader.biBitCount = 32;
   bmi.bmiHeader.biCompression = BI_RGB;

   hdc = GetDC( wvt->window );
   wvt->canvasDc = CreateCompatibleDC( hdc );
   wvt->canvasBitmap = CreateDIBSection( hdc, &bmi, DIB_RGB_COLORS, &bits, NULL, 0 );
   ReleaseDC( wvt->window, hdc );

   wvt->canvasOld = SelectObject( wvt->canvasDc, wvt->canvasBitmap );
   wvt->pixels = ( uint32_t * )bits;
   wvt->canvasWidth = width;
   wvt->canvasHeight = height;
   SelectObject( wvt->canvasDc, wvt->font );
   SetTextAlign( wvt->canvasDc, TA_LEFT );
   SetTextCharacterExtra( wvt->canvasDc, 0 );

   bg = ( ( uint32_t )wvt->colors[0][0] << 16 ) | ( ( uint32_t )wvt->colors[0][1] << 8 ) | wvt->colors[0][2];
   count = ( size_t )width * height;
   for( i = 0; i < count; ++i )
      wvt->pixels[i] = bg;

   wvt->clipTop = 0;
   wvt->clipLeft = 0;
   wvt->clipBottom = s_gt_wvt_CanvasToLogical( wvt, height, 1 ) - 1; /* the clip is in logical pixels */
   wvt->clipRight = s_gt_wvt_CanvasToLogical( wvt, width, 0 ) - 1;
   wvt->clipOn = 1;
   wvt->resized = 1;
   X_GTSELF_EXPOSEAREA( wvt->gt, 0, 0, wvt->rows - 1, wvt->cols - 1 );
   InvalidateRect( wvt->window, NULL, FALSE );
}

static void s_gt_wvt_InvalidateCanvas( X_GTWVT *wvt )
{
   RECT rect = { wvt->marginLeft, wvt->marginTop, wvt->marginLeft + wvt->canvasWidth,
                 wvt->marginTop + wvt->canvasHeight };

   InvalidateRect( wvt->window, &rect, FALSE );
}

/* ---------------------------------------------------------------------------------------------
   Font, coordinates and caret
   --------------------------------------------------------------------------------------------- */

static HFONT s_gt_wvt_GetFont( const wchar_t *face, int height, int width, int weight, int quality )
{
   if( height > 0 )
   {
      LOGFONTW logfont;

      memset( &logfont, 0, sizeof( logfont ) );
      logfont.lfWeight = weight;
      logfont.lfCharSet = DEFAULT_CHARSET;
      logfont.lfQuality = ( BYTE )quality;
      logfont.lfPitchAndFamily = FIXED_PITCH | FF_MODERN; /* all mapping depends on fixed width fonts */
      logfont.lfHeight = height;
      logfont.lfWidth = IIF( width < 0, -width, width );
      wcsncpy( logfont.lfFaceName, face, LF_FACESIZE - 1 );
      return CreateFontIndirectW( &logfont );
   }

   return ( HFONT )GetStockObject( OEM_FIXED_FONT );
}

static RECT s_gt_wvt_GetXYFromColRowRect( X_GTWVT *wvt, RECT colrow )
{
   RECT xy;

   xy.left = colrow.left * wvt->cellSize.x + wvt->marginLeft;
   xy.top = colrow.top * wvt->cellSize.y + wvt->marginTop;
   xy.right = ( colrow.right + 1 ) * wvt->cellSize.x + wvt->marginLeft;
   xy.bottom = ( colrow.bottom + 1 ) * wvt->cellSize.y + wvt->marginTop;

   return xy;
}

static RECT s_gt_wvt_GetColRowFromXYRect( X_GTWVT *wvt, RECT xy )
{
   RECT colrow;

   if( wvt->maximized )
   {
      xy.left = X_MAX( xy.left - wvt->marginLeft, 0 );
      xy.right = X_MAX( xy.right - wvt->marginLeft, 0 );
      xy.top = X_MAX( xy.top - wvt->marginTop, 0 );
      xy.bottom = X_MAX( xy.bottom - wvt->marginTop, 0 );
   }

   colrow.left = xy.left / wvt->cellSize.x;
   colrow.top = xy.top / wvt->cellSize.y;
   /* a rectangle exactly overlapping characters ends in the previous cell */
   colrow.right = xy.right / wvt->cellSize.x - IIF( xy.right % wvt->cellSize.x, 0, 1 );
   colrow.bottom = xy.bottom / wvt->cellSize.y - IIF( xy.bottom % wvt->cellSize.y, 0, 1 );

   return colrow;
}

static void s_gt_wvt_UpdateCursor( X_GTWVT *wvt )
{
   int  row, col, style, caretSize;
   RECT rect;

   if( !wvt->window )
      return;

   X_GTSELF_GETSCRCURSOR( wvt->gt, &row, &col, &style );

   if( row < 0 || col < 0 || row >= wvt->rows || col >= wvt->cols )
      caretSize = 0;
   else
   {
      switch( style )
      {
         case SC_INSERT: caretSize = wvt->cellSize.y >> 1; break;
         case SC_SPECIAL1: caretSize = wvt->cellSize.y; break;
         case SC_SPECIAL2: caretSize = -( wvt->cellSize.y >> 1 ); break;
         case SC_NORMAL: caretSize = X_MAX( ( wvt->cellSize.y >> 2 ) - 1, 1 ); break;
         default: caretSize = 0; break;
      }
   }

   if( caretSize == 0 || !wvt->caretFocus )
      SetRectEmpty( &rect );
   else
   {
      rect.left = col * wvt->cellSize.x + wvt->marginLeft;
      rect.top = row * wvt->cellSize.y + wvt->marginTop + IIF( caretSize > 0, wvt->cellSize.y - caretSize, 0 );
      rect.right = rect.left + wvt->cellSize.x;
      rect.bottom = rect.top + IIF( caretSize > 0, caretSize, -caretSize );
   }

   if( EqualRect( &rect, &wvt->caretRect ) )
      return;

   /* a moved or changed cursor is shown at once and blinks again from the start */
   if( !IsRectEmpty( &wvt->caretRect ) )
      InvalidateRect( wvt->window, &wvt->caretRect, FALSE );
   wvt->caretRect = rect;
   wvt->caretOn = 1;
   if( IsRectEmpty( &rect ) )
      KillTimer( wvt->window, WVT_CARET_TIMER );
   else
   {
      InvalidateRect( wvt->window, &rect, FALSE );
      if( wvt->caretBlink == 0 )
         KillTimer( wvt->window, WVT_CARET_TIMER );
      else
         SetTimer( wvt->window, WVT_CARET_TIMER, ( UINT )wvt->caretBlink, NULL );
   }
}

static void s_gt_wvt_AddKey( X_GTWVT *wvt, int key )
{
   int pos = wvt->keyIn;

   if( wvt->keyIn != wvt->keyOut && X_INKEY_ISMOUSEPOS( key ) && X_INKEY_ISMOUSEPOS( wvt->keys[wvt->keyLast] ) )
   {
      wvt->keys[wvt->keyLast] = key;
      return;
   }

   /* when the buffer is full a new event overwrites the last one */
   wvt->keys[wvt->keyLast = pos] = key;
   if( ++pos >= WVT_KEY_QUEUE_SIZE )
      pos = 0;
   if( pos != wvt->keyOut )
      wvt->keyIn = pos;
}

/* ---------------------------------------------------------------------------------------------
   Window size
   --------------------------------------------------------------------------------------------- */

static void s_gt_wvt_ResizeFromWindow( X_GTWVT *wvt )
{
   RECT ci;

   wvt->maximized = IsZoomed( wvt->window );

   GetClientRect( wvt->window, &ci );

   if( ci.bottom > 0 )
   {
      int oldCentred = wvt->centred;

      wvt->centred = 0;
      wvt->gt->pFuncTable->SetMode( wvt->gt, ci.bottom / wvt->cellSize.y, ci.right / wvt->cellSize.x );
      wvt->centred = oldCentred;
   }
}

static void s_gt_wvt_FitSize( X_GTWVT *wvt )
{
   RECT wi, ci;
   int  maxWidth, maxHeight, borderWidth, borderHeight, left, top;
   int  fontHeight, fontWidth;
   int  i = 0, j = 0, calcWidth = 0, calcHeight = 0;

   wvt->maximized = IsZoomed( wvt->window );

   GetClientRect( wvt->window, &ci );
   GetWindowRect( wvt->window, &wi );

   borderWidth = ( wi.right - wi.left ) - ci.right;
   borderHeight = ( wi.bottom - wi.top ) - ci.bottom;

   maxWidth = ci.right;
   maxHeight = ci.bottom;

   left = wi.left;
   top = wi.top;

   fontHeight = maxHeight / wvt->rows;
   fontWidth = maxWidth / wvt->cols;

   for( ;; )
   {
      HFONT font = s_gt_wvt_GetFont( wvt->fontFace, fontHeight, fontWidth, wvt->fontWeight, wvt->fontQuality );

      if( font )
      {
         HDC        hdc;
         HFONT      oldFont;
         int        width, height;
         TEXTMETRIC tm;

         hdc = GetDC( wvt->window );
         oldFont = ( HFONT )SelectObject( hdc, font );
         SetTextCharacterExtra( hdc, 0 );
         GetTextMetrics( hdc, &tm );
         SelectObject( hdc, oldFont );
         ReleaseDC( wvt->window, hdc );

         width = tm.tmAveCharWidth * wvt->cols;
         height = tm.tmHeight * wvt->rows;

         if( width <= maxWidth && height <= maxHeight && tm.tmAveCharWidth >= 4 && tm.tmHeight >= 8 )
         {
            s_gt_wvt_SetFont( wvt, font );
            wvt->fontHeight = tm.tmHeight;
            wvt->fontWidth = tm.tmAveCharWidth;
            wvt->cellSize.x = tm.tmAveCharWidth;
            wvt->cellSize.y = tm.tmHeight;

            width = wvt->cellSize.x * wvt->cols + borderWidth;
            height = wvt->cellSize.y * wvt->rows + borderHeight;

            if( wvt->maximized )
            {
               /* the grid stays in the top left corner, as in every GT */
               wvt->marginLeft = 0;
               wvt->marginTop = 0;
            }
            else
            {
               wvt->marginLeft = 0;
               wvt->marginTop = 0;
               if( wvt->sizeStep && ( wi.right - wi.left != width || wi.bottom - wi.top != height ) )
                  SetWindowPos( wvt->window, NULL, left, top, width, height, SWP_NOZORDER );
            }

            s_gt_wvt_UpdateCursor( wvt );

            s_gt_wvt_CreateCanvas( wvt );
         }
         else
         {
            /* "Courier New" does not scale linearly: some heights work only with a slightly
               smaller width, so the font size is adjusted and the font is created again */
            DeleteObject( font );

            if( calcWidth == 0 && calcHeight == 0 )
            {
               calcWidth = fontWidth;
               calcHeight = fontHeight;
            }

            if( i == j )
            {
               j = 0;
               i++;
            }
            else if( i > j )
            {
               j++;
               if( j == i )
                  i = 0;
            }
            else
               i++;

            fontWidth = calcWidth - i;
            fontHeight = calcHeight - j;

            if( fontWidth < 4 || fontHeight < 8 )
            {
               width = wvt->cellSize.x * wvt->cols + borderWidth;
               height = wvt->cellSize.y * wvt->rows + borderHeight;
               SetWindowPos( wvt->window, NULL, left, top, width, height, SWP_NOZORDER );
               break;
            }

            continue;
         }
      }

      break;
   }
}

static void s_gt_wvt_ResetWindowSize( X_GTWVT *wvt, HFONT font )
{
   HDC        hdc;
   HFONT      oldFont;
   int        height, width;
   RECT       wi, ci;
   TEXTMETRIC tm;

   if( !wvt->font || font )
   {
      if( !font )
         font = s_gt_wvt_GetFont( wvt->fontFace, wvt->fontHeight, wvt->fontWidth, wvt->fontWeight, wvt->fontQuality );
      s_gt_wvt_SetFont( wvt, font );
   }

   hdc = GetDC( wvt->window );
   oldFont = ( HFONT )SelectObject( hdc, wvt->font );
   GetTextMetrics( hdc, &tm );
   SetTextCharacterExtra( hdc, 0 ); /* no extra char spacing even if bold */
   SelectObject( hdc, oldFont );
   ReleaseDC( wvt->window, hdc );

   wvt->cellSize.x = IIF( wvt->fontWidth < 0, -wvt->fontWidth, tm.tmAveCharWidth );
   wvt->cellSize.y = tm.tmHeight;

   /* resize the window to get the specified number of rows and columns */
   GetWindowRect( wvt->window, &wi );
   GetClientRect( wvt->window, &ci );

   height = wvt->cellSize.y * wvt->rows + ( wi.bottom - wi.top - ci.bottom );
   width = wvt->cellSize.x * wvt->cols + ( wi.right - wi.left - ci.right );

   if( wvt->maximized )
   {
      /* the grid stays in the top left corner, as in every GT */
      wvt->marginLeft = 0;
      wvt->marginTop = 0;
   }
   else
   {
      RECT workArea;

      wvt->marginLeft = 0;
      wvt->marginTop = 0;

      if( wvt->centred && SystemParametersInfoW( SPI_GETWORKAREA, 0, &workArea, 0 ) )
      {
         int recenter = 0;
         int move;

         if( width > workArea.right - workArea.left )
         {
            /* the window is wider than the work area: fit it */
            width = workArea.right - workArea.left;
            recenter = 1;
         }

         if( height > workArea.bottom - workArea.top )
         {
            height = workArea.bottom - workArea.top;
            recenter = 1;
         }

         move = recenter || wi.left != wvt->newX || wi.top != wvt->newY;
         if( workArea.left + width > workArea.right || workArea.top + height > workArea.bottom )
            move = 1;
         if( move )
         {
            wi.left = workArea.left + ( ( workArea.right - workArea.left - width ) / 2 );
            wi.top = workArea.top + ( ( workArea.bottom - workArea.top - height ) / 2 );
         }

         if( wvt->resizeMode == X_GTI_RESIZEMODE_ROWS )
         {
            wvt->resizeMode = X_GTI_RESIZEMODE_FONT;
            SetWindowPos( wvt->window, NULL, wi.left, wi.top, width, height, SWP_NOZORDER );
            wvt->resizeMode = X_GTI_RESIZEMODE_ROWS;
         }
         else
            SetWindowPos( wvt->window, NULL, wi.left, wi.top, width, height, SWP_NOZORDER );

         if( recenter )
         {
            GetWindowRect( wvt->window, &wi );
            width = wi.right - wi.left;
            height = wi.bottom - wi.top;
            wi.left = workArea.left + ( ( workArea.right - workArea.left - width ) / 2 );
            wi.top = workArea.top + ( ( workArea.bottom - workArea.top - height ) / 2 );
            SetWindowPos( wvt->window, NULL, wi.left, wi.top, width, height, SWP_NOSIZE | SWP_NOZORDER );
         }
      }
      else if( wvt->sizeStep )
         /* the user resized the window: it follows whole cells without moving its left top origin */
         SetWindowPos( wvt->window, NULL, wi.left, wi.top, width, height, SWP_NOZORDER );
   }

   s_gt_wvt_CreateCanvas( wvt );

   s_gt_wvt_UpdateCursor( wvt );
}

static int s_gt_wvt_SetWindowSize( X_GTWVT *wvt, int rows, int cols )
{
   if( !X_GTSELF_RESIZE( wvt->gt, rows, cols ) )
      return 0;

   if( wvt->window && ( rows != wvt->rows || cols != wvt->cols ) )
      s_gt_wvt_AddKey( wvt, K_RESIZE );

   wvt->rows = rows;
   wvt->cols = cols;

   return 1;
}

static int s_gt_wvt_InitWindow( X_GTWVT *wvt, int row, int col, HFONT font )
{
   int result = s_gt_wvt_SetWindowSize( wvt, row, col );

   s_gt_wvt_ResetWindowSize( wvt, font );

   return result;
}

static void s_gt_wvt_SetMousePos( X_GTWVT *wvt, int row, int col )
{
   wvt->mousePos.y = row;
   wvt->mousePos.x = col;
}

static int s_gt_wvt_KeyFlags( void )
{
   int flags = 0;

   if( GetKeyState( VK_SHIFT ) & 0x8000 )
      flags |= KF_SHIFT;
   if( GetKeyState( VK_CONTROL ) & 0x8000 )
      flags |= KF_CTRL;
   if( GetKeyState( VK_LMENU ) & 0x8000 )
      flags |= KF_ALT;
   if( GetKeyState( VK_RMENU ) & 0x8000 )
      flags |= X_KF_ALTGR;

   return flags;
}

static int s_gt_wvt_UpdateKeyFlags( int flags )
{
   if( flags & X_KF_ALTGR )
   {
      flags |= KF_ALT;
      flags &= ~X_KF_ALTGR;
   }

   return flags;
}

static void s_gt_wvt_SetCloseButton( X_GTWVT *wvt )
{
   HMENU sysMenu = GetSystemMenu( wvt->window, FALSE );
   UINT  state = IIF( wvt->closeMode < 2, MF_ENABLED, MF_GRAYED );

   if( sysMenu )
      EnableMenuItem( sysMenu, SC_CLOSE, MF_BYCOMMAND | state );
}

/* ---------------------------------------------------------------------------------------------
   Window procedure and window creation
   --------------------------------------------------------------------------------------------- */

static LRESULT CALLBACK s_gt_wvt_WndProc( HWND window, UINT message, WPARAM wParam, LPARAM lParam )
{
   X_GTWVT *wvt = NULL;
   int      remaining = s_windowCount, pos;

   for( pos = 0; remaining && pos < WVT_MAX_WINDOWS; ++pos )
   {
      if( s_windows[pos] )
      {
         if( s_windows[pos]->window == window )
         {
            wvt = s_windows[pos];
            break;
         }
         --remaining;
      }
   }

   if( !wvt && message == WM_CREATE )
   {
      wvt = ( X_GTWVT * )( ( LPCREATESTRUCT )lParam )->lpCreateParams;
      if( wvt && s_windows[wvt->number] == wvt )
         wvt->window = window;
      else
         wvt = NULL;
   }

   if( wvt )
   {
      switch( message )
      {
         case WM_CREATE:
         {
            /* the window may open on a monitor with another DPI than the system */
            int dpi = ( int )GetDpiForWindow( window );

            wvt->fontHeight = MulDiv( wvt->fontHeight, dpi, wvt->dpi );
            wvt->fontWidth = MulDiv( wvt->fontWidth, dpi, wvt->dpi );
            wvt->dpi = dpi;
            return IIF( s_gt_wvt_InitWindow( wvt, wvt->rows, wvt->cols, NULL ), 0, -1 );
         }

         case WM_DPICHANGED:
         {
            /* moved to a monitor with another DPI: the font follows the monitor and the window
            keeps its rows and columns at the position Windows suggests; a new canvas is made
            for the new font, as for X_GTI_FONTSIZE */
            const RECT    *suggested = ( const RECT * )lParam;
            const wchar_t *face = wvt->fontFace;
            int            centre = wvt->centred, sizeStep = wvt->sizeStep;
            UINT           flags = SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE;
            HFONT          font;

            wvt->fontHeight = MulDiv( wvt->fontHeight, HIWORD( wParam ), wvt->dpi );
            wvt->fontWidth = MulDiv( wvt->fontWidth, HIWORD( wParam ), wvt->dpi );
            wvt->dpi = HIWORD( wParam );
            wvt->centred = 0;
            wvt->sizeStep = 1;
            SetWindowPos( window, NULL, suggested->left, suggested->top, 0, 0, flags );
            font = s_gt_wvt_GetFont( face, wvt->fontHeight, wvt->fontWidth, wvt->fontWeight, wvt->fontQuality );
            s_gt_wvt_ResetWindowSize( wvt, font );
            wvt->centred = centre;
            wvt->sizeStep = sizeStep;
            return 0;
         }

         case WM_PAINT:
         {
            /* the canvas is copied to the window; the area outside the canvas takes the program background */
            PAINTSTRUCT ps;
            HDC         hdc = BeginPaint( window, &ps );
            int         saved = SaveDC( hdc );
            int         right = wvt->marginLeft + wvt->canvasWidth;
            int         bottom = wvt->marginTop + wvt->canvasHeight;

            if( wvt->canvasDc )
               ExcludeClipRect( hdc, wvt->marginLeft, wvt->marginTop, right, bottom );
            s_gt_wvt_FillEdges( wvt, hdc, &ps.rcPaint );
            RestoreDC( hdc, saved );

            if( wvt->canvasDc )
            {
               int width = ps.rcPaint.right - ps.rcPaint.left;
               int height = ps.rcPaint.bottom - ps.rcPaint.top;
               int sourceX = ps.rcPaint.left - wvt->marginLeft;
               int sourceY = ps.rcPaint.top - wvt->marginTop;

               GdiFlush();
               BitBlt( hdc, ps.rcPaint.left, ps.rcPaint.top, width, height, wvt->canvasDc, sourceX, sourceY, SRCCOPY );
            }

            /* the cursor inverts the cell part it covers, like the Windows caret did */
            if( wvt->caretOn && !IsRectEmpty( &wvt->caretRect ) )
            {
               const RECT *caret = &wvt->caretRect;
               int         width = caret->right - caret->left;
               int         height = caret->bottom - caret->top;

               PatBlt( hdc, caret->left, caret->top, width, height, DSTINVERT );
            }

            EndPaint( window, &ps );
            return 0;
         }

         case WM_ERASEBKGND: return 1;

         case WM_SETFOCUS:
            wvt->caretFocus = 1;
            s_gt_wvt_UpdateCursor( wvt );
            return 0;

         case WM_KILLFOCUS:
            wvt->caretFocus = 0;
            s_gt_wvt_UpdateCursor( wvt );
            return 0;

         case WM_TIMER:
            if( wParam == WVT_CARET_TIMER )
            {
               wvt->caretOn = !wvt->caretOn;
               InvalidateRect( window, &wvt->caretRect, FALSE );
               return 0;
            }
            break;

         case WM_KEYDOWN:
         case WM_SYSKEYDOWN:
         {
            int key = 0, flags = s_gt_wvt_KeyFlags(), keypad = 0;

            wvt->ignoreSysChar = 0;
            switch( wParam )
            {
               case VK_BACK:
                  wvt->ignoreSysChar = 1;
                  key = KX_BS;
                  break;
               case VK_TAB:
                  wvt->ignoreSysChar = 1;
                  key = KX_TAB;
                  break;
               case VK_RETURN:
                  wvt->ignoreSysChar = 1;
                  if( wvt->altEnter && ( flags & KF_ALT ) != 0 )
                     s_gt_wvt_SetFullscreen( wvt->gt );
                  else
                  {
                     key = KX_ENTER;
                     if( lParam & WVT_EXTKEY_FLAG )
                        flags |= KF_KEYPAD;
                  }
                  break;
               case VK_ESCAPE:
                  wvt->ignoreSysChar = 1;
                  key = KX_ESC;
                  break;

               case VK_PRIOR: keypad = KX_PGUP; break;
               case VK_NEXT: keypad = KX_PGDN; break;
               case VK_END: keypad = KX_END; break;
               case VK_HOME: keypad = KX_HOME; break;
               case VK_LEFT: keypad = KX_LEFT; break;
               case VK_UP: keypad = KX_UP; break;
               case VK_RIGHT: keypad = KX_RIGHT; break;
               case VK_DOWN: keypad = KX_DOWN; break;
               case VK_INSERT: keypad = KX_INS; break;
               case VK_DELETE:
                  key = KX_DEL;
                  if( ( lParam & WVT_EXTKEY_FLAG ) == 0 )
                     flags |= KF_KEYPAD;
                  break;

               case VK_F1:
               case VK_F2:
               case VK_F3:
               case VK_F4:
               case VK_F5:
               case VK_F6:
               case VK_F7:
               case VK_F8:
               case VK_F9:
               case VK_F10:
               case VK_F11:
               case VK_F12: key = KX_F1 + ( int )( wParam - VK_F1 ); break;

               case VK_SNAPSHOT: key = KX_PRTSCR; break;
               case VK_CANCEL:
                  if( ( lParam & WVT_EXTKEY_FLAG ) == 0 )
                     break;
                  flags |= KF_CTRL;
                  /* fallthrough */
               case VK_PAUSE:
                  wvt->ignoreSysChar = 1;
                  key = KX_PAUSE;
                  break;

               case VK_CLEAR: keypad = KX_CENTER; break;

               case VK_NUMPAD0:
               case VK_NUMPAD1:
               case VK_NUMPAD2:
               case VK_NUMPAD3:
               case VK_NUMPAD4:
               case VK_NUMPAD5:
               case VK_NUMPAD6:
               case VK_NUMPAD7:
               case VK_NUMPAD8:
               case VK_NUMPAD9:
                  if( flags & KF_CTRL )
                  {
                     wvt->ignoreSysChar = 1;
                     key = ( int )wParam - VK_NUMPAD0 + '0';
                  }
                  else if( flags == KF_ALT || flags == X_KF_ALTGR )
                     flags = 0; /* for ALT + <ASCII_VALUE_FROM_KEYPAD> */
                  flags |= KF_KEYPAD;
                  break;
               case VK_DECIMAL:
               case VK_SEPARATOR:
                  flags |= KF_KEYPAD;
                  if( flags & KF_CTRL )
                  {
                     wvt->ignoreSysChar = 1;
                     key = '.';
                  }
                  break;

               case VK_DIVIDE:
                  flags |= KF_KEYPAD;
                  if( flags & KF_CTRL )
                     key = '/';
                  break;
               case VK_MULTIPLY:
                  flags |= KF_KEYPAD;
                  if( flags & KF_CTRL )
                     key = '*';
                  break;
               case VK_SUBTRACT:
                  flags |= KF_KEYPAD;
                  if( flags & KF_CTRL )
                     key = '-';
                  break;
               case VK_ADD:
                  flags |= KF_KEYPAD;
                  if( flags & KF_CTRL )
                     key = '+';
                  break;
               case VK_OEM_2:
                  if( ( flags & KF_CTRL ) != 0 && ( flags & KF_SHIFT ) != 0 )
                     key = '?';
                  break;
               case VK_APPS: key = K_MENU; break;
            }
            if( keypad != 0 )
            {
               key = keypad;
               if( ( lParam & WVT_EXTKEY_FLAG ) == 0 )
               {
                  if( flags == KF_ALT || flags == X_KF_ALTGR )
                     flags = key = 0; /* for ALT + <ASCII_VALUE_FROM_KEYPAD> */
                  else
                     flags |= KF_KEYPAD;
               }
            }
            wvt->keyFlags = flags;
            if( key != 0 && key != K_MENU )
               key = X_INKEY_NEW_KEY( key, s_gt_wvt_UpdateKeyFlags( flags ) );
            if( key != 0 )
               s_gt_wvt_AddKey( wvt, key );
            return 0;
         }

         case WM_CHAR:
         case WM_SYSCHAR:
         {
            int key = 0, flags = wvt->keyFlags;
            int altGr;

            altGr = ( ( flags & KF_CTRL ) != 0 && ( flags & KF_ALT ) != 0 ) || ( flags & X_KF_ALTGR ) != 0;
            if( message == WM_CHAR && altGr )
               /* workaround for AltGr and some German and Italian keyboards */
               flags &= ~( KF_CTRL | KF_ALT | X_KF_ALTGR );
            flags = s_gt_wvt_UpdateKeyFlags( flags );

            if( !wvt->ignoreSysChar )
            {
               key = ( int )wParam;

               if( ( flags & KF_CTRL ) != 0 && key >= 0 && key < 32 )
                  key = X_INKEY_NEW_KEY( key + 'A' - 1, flags );
               else if( key >= 0xDC00 && key <= 0xDFFF )
                  /* the low surrogate follows a high surrogate reported as U+FFFD */
                  key = 0;
               else
               {
                  if( message == WM_SYSCHAR && ( flags & KF_ALT ) != 0 )
                  {
                     int scan = HIWORD( lParam ) & 0xFF;
                     int lower = key | 0x20;
                     int swapped = ( scan == 21 || scan == 44 ) && ( lower == 'y' || lower == 'z' );

                     /* Y and Z swap places on some keyboards, so the typed letter is kept */
                     if( scan < ( int )sizeof( s_scanKeys ) && s_scanKeys[scan] && !swapped )
                        key = s_scanKeys[scan];
                  }

                  /* extended input stores one Unicode value in 16 bits, as in GTSDL3 */
                  if( key >= 0xD800 && key <= 0xDBFF )
                     key = 0xFFFD;

                  if( key < 127 && ( flags & ( KF_CTRL | KF_ALT ) ) )
                     key = X_INKEY_NEW_KEY( key, flags );
                  else
                     key = X_INKEY_NEW_UNICODEF( key, flags );
               }
            }
            wvt->ignoreSysChar = 0;
            if( key != 0 )
               s_gt_wvt_AddKey( wvt, key );
            return 0;
         }

         case WM_RBUTTONDOWN:
         case WM_LBUTTONDOWN:
         case WM_RBUTTONUP:
         case WM_LBUTTONUP:
         case WM_RBUTTONDBLCLK:
         case WM_LBUTTONDBLCLK:
         case WM_MBUTTONDOWN:
         case WM_MBUTTONUP:
         case WM_MBUTTONDBLCLK:
         case WM_MOUSEMOVE:
         case WM_MOUSEWHEEL:
         {
            int   keyCode = 0;
            POINT xy;

            xy.x = ( short )LOWORD( lParam );
            xy.y = ( short )HIWORD( lParam );

            if( message == WM_MOUSEWHEEL )
               ScreenToClient( window, &xy );

            /* as in GTSDL3: cells stay inside the screen and every change of the pixel position
            wakes pixel consumers; the queue merges consecutive position events */
            if( !wvt->marking && wvt->mouseOn )
            {
               int mouseX = xy.x - wvt->marginLeft;
               int mouseY = xy.y - wvt->marginTop;
               int pixelChg = mouseX != wvt->mouseX || mouseY != wvt->mouseY;
               int row = X_MAX( 0, X_MIN( mouseY / wvt->cellSize.y, wvt->rows - 1 ) );
               int col = X_MAX( 0, X_MIN( mouseX / wvt->cellSize.x, wvt->cols - 1 ) );

               wvt->mouseX = mouseX;
               wvt->mouseY = mouseY;
               s_gt_wvt_SetMousePos( wvt, row, col );
               if( pixelChg )
                  s_gt_wvt_AddKey( wvt, X_INKEY_NEW_MPOS( wvt->mousePos.x, wvt->mousePos.y ) );
            }

            switch( message )
            {
               case WM_LBUTTONDBLCLK: keyCode = K_LDBLCLK; break;

               case WM_RBUTTONDBLCLK: keyCode = K_RDBLCLK; break;

               case WM_LBUTTONDOWN:
                  /* "Mark" from the system menu, or Shift and the left button when X_GTI_SELECTCOPY is
               on, as in GTSDL3, starts a selection that is copied when the button is released */
                  wvt->skipUp = 0;
                  if( wvt->markStarted || ( wvt->selectCopy && ( GetKeyState( VK_SHIFT ) & 0x8000 ) != 0 ) )
                  {
                     wvt->marking = 1;
                     SetCapture( window );

                     wvt->selectionNew.left = xy.x;
                     wvt->selectionNew.top = xy.y;
                     wvt->selectionNew.right = xy.x;
                     wvt->selectionNew.bottom = xy.y;

                     SetRectEmpty( &wvt->selectionOld );
                     return 0;
                  }
                  keyCode = K_LBUTTONDOWN;
                  break;

               case WM_RBUTTONDOWN: keyCode = K_RBUTTONDOWN; break;

               case WM_RBUTTONUP: keyCode = K_RBUTTONUP; break;

               case WM_LBUTTONUP:
                  if( wvt->marking )
                  {
                     wchar_t *buffer;
                     size_t   n = 0;
                     size_t   count;
                     int      row, col;
                     RECT     rect;

                     wvt->markStarted = 0;
                     wvt->marking = 0;
                     ReleaseCapture();

                     RedrawWindow( window, NULL, NULL, RDW_INVALIDATE | RDW_UPDATENOW );

                     /* the marked cells go to the clipboard, one line per row */
                     rect.left = X_MIN( wvt->selectionNew.left, wvt->selectionNew.right );
                     rect.top = X_MIN( wvt->selectionNew.top, wvt->selectionNew.bottom );
                     rect.right = X_MAX( wvt->selectionNew.left, wvt->selectionNew.right );
                     rect.bottom = X_MAX( wvt->selectionNew.top, wvt->selectionNew.bottom );
                     rect = s_gt_wvt_GetColRowFromXYRect( wvt, rect );

                     count = ( size_t )( rect.bottom - rect.top + 1 ) * ( rect.right - rect.left + 1 + 2 ) + 1;
                     buffer = ( wchar_t * )x_xgrab( count * sizeof( wchar_t ) );

                     for( row = rect.top; row <= rect.bottom; row++ )
                     {
                        for( col = rect.left; col <= rect.right; col++ )
                        {
                           int      color;
                           uint8_t  attr;
                           uint16_t character;

                           if( !X_GTSELF_GETSCRCHAR( wvt->gt, row, col, &color, &attr, &character ) )
                              break;
                           /* control characters as the glyphs on the screen, as in GTSDL3 */
                           buffer[n++] = ( wchar_t )x_u16Ctrl( character );
                        }
                        if( rect.top < rect.bottom )
                        {
                           buffer[n++] = L'\r';
                           buffer[n++] = L'\n';
                        }
                     }

                     if( n > 0 )
                        s_gt_wvt_SetClipboard( buffer, n );
                     x_xfree( buffer );
                     return 0;
                  }
                  if( wvt->skipUp )
                  {
                     /* the release of a selection cancelled by X_GTI_SELECTCOPY */
                     wvt->skipUp = 0;
                     return 0;
                  }
                  keyCode = K_LBUTTONUP;
                  break;

               case WM_MBUTTONDOWN: keyCode = K_MBUTTONDOWN; break;

               case WM_MBUTTONUP: keyCode = K_MBUTTONUP; break;

               case WM_MBUTTONDBLCLK: keyCode = K_MDBLCLK; break;

               case WM_MOUSEMOVE:
                  if( wvt->marking )
                  {
                     RECT rect;

                     wvt->selectionNew.right = xy.x;
                     wvt->selectionNew.bottom = xy.y;

                     rect.left = X_MIN( wvt->selectionNew.left, wvt->selectionNew.right );
                     rect.top = X_MIN( wvt->selectionNew.top, wvt->selectionNew.bottom );
                     rect.right = X_MAX( wvt->selectionNew.left, wvt->selectionNew.right );
                     rect.bottom = X_MAX( wvt->selectionNew.top, wvt->selectionNew.bottom );
                     /* out of band coordinates may appear due to margins in maximized mode */
                     rect.left = X_MAX( rect.left, 0 );
                     rect.top = X_MAX( rect.top, 0 );
                     rect.right = X_MIN( rect.right, wvt->cols * wvt->cellSize.x );
                     rect.bottom = X_MIN( rect.bottom, wvt->rows * wvt->cellSize.y );

                     rect = s_gt_wvt_GetXYFromColRowRect( wvt, s_gt_wvt_GetColRowFromXYRect( wvt, rect ) );

                     if( !EqualRect( &rect, &wvt->selectionOld ) )
                     {
                        /* the marked area is inverted on the window only, the canvas keeps the screen */
                        const RECT *old = &wvt->selectionOld;
                        HRGN        rgn1 = CreateRectRgn( old->left, old->top, old->right, old->bottom );
                        HRGN        rgn2 = CreateRectRgn( rect.left, rect.top, rect.right, rect.bottom );
                        HRGN        rgn3 = CreateRectRgn( 0, 0, 0, 0 );

                        if( CombineRgn( rgn3, rgn1, rgn2, RGN_XOR ) != 0 )
                        {
                           HDC hdc = GetDC( window );

                           InvertRgn( hdc, rgn3 );
                           ReleaseDC( window, hdc );
                        }

                        DeleteObject( rgn1 );
                        DeleteObject( rgn2 );
                        DeleteObject( rgn3 );

                        wvt->selectionOld = rect;
                     }
                     return 0;
                  }
                  break;

               case WM_MOUSEWHEEL: keyCode = IIF( ( short )HIWORD( wParam ) > 0, K_MWFORWARD, K_MWBACKWARD ); break;
            }

            if( keyCode != 0 && wvt->mouseOn )
               s_gt_wvt_AddKey( wvt, X_INKEY_NEW_MKEY( keyCode, s_gt_wvt_UpdateKeyFlags( s_gt_wvt_KeyFlags() ) ) );
            return 0;
         }

         case WM_QUERYENDSESSION: /* we can shut down or log off */ return 1;

         case WM_ENDSESSION: /* shutdown started */
            /* a quit cannot unwind through the window procedure; it is raised by the message loop */
            if( wParam )
               wvt->quitRequested = 1;
            return 0;

         case WM_CLOSE: /* the close button or the system menu */
            if( wvt->closeMode == 0 )
               wvt->quitRequested = 1;
            else if( wvt->closeMode == 1 )
               s_gt_wvt_AddKey( wvt, K_CLOSE );
            return 0;

         case WM_ACTIVATE:
            s_gt_wvt_AddKey( wvt, IIF( LOWORD( wParam ) == WA_INACTIVE, K_LOSTFOCUS, K_GOTFOCUS ) );
            return 0;

         case WM_ENTERSIZEMOVE: wvt->resizing = 1; return 0;

         case WM_EXITSIZEMOVE:
            wvt->resizing = 0;
            if( wvt->sizing )
            {
               /* the user was resizing, not moving, the window */
               s_gt_wvt_ResizeFromWindow( wvt );
               wvt->sizing = 0;
            }
            return 0;

         case WM_SIZE:
            /* a frame change that keeps the client area (X_GTI_WINDOWBORDERED) keeps the canvas */
            if( wvt->fullScreen || wParam == SIZE_MINIMIZED )
               return 0;

            if( LOWORD( lParam ) == wvt->lastClient.right && HIWORD( lParam ) == wvt->lastClient.bottom )
               return 0;

            wvt->lastClient.right = LOWORD( lParam );
            wvt->lastClient.bottom = HIWORD( lParam );
            if( wvt->resizing && wvt->resizeMode == X_GTI_RESIZEMODE_ROWS )
            {
               wvt->sizing = 1;
               if( wvt->gt->resizeBlock )
               {
                  int sizeStep = wvt->sizeStep;

                  wvt->sizeStep = 0;
                  s_gt_wvt_ResizeFromWindow( wvt );
                  wvt->sizeStep = sizeStep;
                  x_gtResizeNotify( wvt->gt );
               }
            }
            else if( wvt->resizeMode == X_GTI_RESIZEMODE_FONT )
            {
               if( !wvt->sizing )
                  s_gt_wvt_FitSize( wvt );
            }
            else
            {
               /* the resize came from maximize, restore or another non mouse operation */
               s_gt_wvt_ResizeFromWindow( wvt );
               x_gtResizeNotify( wvt->gt );
            }
            return 0;

         case WM_SYSCOMMAND:
            if( wParam == SYS_EV_MARK )
            {
               wvt->markStarted = 1;
               return 0;
            }
            else if( wParam > SYS_EV_MARK )
            {
               X_GTWVT_MENU *menu;

               for( menu = wvt->menu; menu; menu = menu->next )
               {
                  if( ( WPARAM )menu->event == wParam )
                  {
                     s_gt_wvt_AddKey( wvt, menu->key );
                     return 0;
                  }
               }
            }
            break;
      }
   }

   return DefWindowProcW( window, message, wParam, lParam );
}

static void s_gt_wvt_PumpEvents( X_GTWVT *wvt )
{
   MSG msg;

   x_callbackEnterDll();
   while( PeekMessageW( &msg, NULL, 0, 0, PM_REMOVE ) )
   {
      TranslateMessage( &msg );
      DispatchMessageW( &msg );
   }
   x_callbackLeaveDll();
   x_callbackRaisePending();

   if( wvt->quitRequested )
   {
      wvt->quitRequested = 0;
      x_quitRaise();
   }
}

static void s_gt_wvt_CreateConsoleWindow( X_GTWVT *wvt )
{
   HMENU sysMenu;
   DWORD style = IIF( wvt->resizable, _WVT_WS_DEF, _WVT_WS_NORESIZE );
   UINT  closeState = IIF( wvt->closeMode < 2, MF_ENABLED, MF_GRAYED );
   int   show;

   if( wvt->window )
      return;

   CreateWindowW( s_className, wvt->title, style, 0, 0, CW_USEDEFAULT, CW_USEDEFAULT, NULL, NULL, wvt->instance, wvt );
   if( !wvt->window )
   {
      fputs( "xDev: failed to create WVT window\n", stderr );
      x_quitRaise();
   }

   if( wvt->icon )
   {
      SendNotifyMessage( wvt->window, WM_SETICON, ICON_SMALL, ( LPARAM )wvt->icon ); /* title bar icon */
      SendNotifyMessage( wvt->window, WM_SETICON, ICON_BIG, ( LPARAM )wvt->icon );   /* task list icon */
   }

   sysMenu = GetSystemMenu( wvt->window, FALSE );
   if( sysMenu )
   {
      X_GTWVT_MENU *menu;

      /* "Mark" in the system menu starts a console like copy operation */
      AppendMenuW( sysMenu, MF_STRING, SYS_EV_MARK, wvt->selectCopyText );
      if( !wvt->selectCopy )
         EnableMenuItem( sysMenu, SYS_EV_MARK, MF_BYCOMMAND | MF_GRAYED );
      EnableMenuItem( sysMenu, SC_CLOSE, MF_BYCOMMAND | closeState );

      for( menu = wvt->menu; menu; menu = menu->next )
         AppendMenuW( sysMenu, MF_STRING, menu->event, menu->name );
   }

   if( wvt->fullScreen )
   {
      wvt->maximized = 0;
      wvt->fullScreen = 0;
      s_gt_wvt_SetFullscreen( wvt->gt );
   }
   else
   {
      if( wvt->newX >= 0 && wvt->newY >= 0 )
      {
         RECT wi = { 0, 0, 0, 0 };
         int  width, height;

         GetWindowRect( wvt->window, &wi );
         width = wi.right - wi.left;
         height = wi.bottom - wi.top;
         SetWindowPos( wvt->window, NULL, wvt->newX, wvt->newY, width, height, SWP_NOSIZE | SWP_NOZORDER );
      }
      show = IIF( wvt->maximized, SW_SHOWMAXIMIZED, wvt->showCommand );
      ShowWindow( wvt->window, show );
      UpdateWindow( wvt->window );
   }
}

static int s_gt_wvt_SetFullscreen( X_GT_BASE *gt )
{
   X_GTWVT *wvt = X_GTWVT_GET( gt );
   LONG_PTR style = GetWindowLongPtrW( wvt->window, GWL_STYLE );
   RECT     rt;

   /* as in GTSDL3, full screen does not change X_GTI_WINDOWALWAYSONTOP */
   if( wvt->fullScreen )
   {
      style |= WS_CAPTION | WS_BORDER;

      if( wvt->resizable )
         style |= WS_THICKFRAME;

      wvt->marginLeft = 0;
      wvt->marginTop = 0;
      wvt->fullScreen = 0;
   }
   else
   {
      style &= ~( WS_CAPTION | WS_BORDER | WS_THICKFRAME );
      wvt->fullScreen = 1;
   }

   SetWindowLongPtrW( wvt->window, GWL_STYLE, style );

   if( !wvt->fullScreen )
   {
      ShowWindow( wvt->window, SW_RESTORE );
      return 0;
   }

   if( !wvt->maximized )
      ShowWindow( wvt->window, SW_SHOWMAXIMIZED );

   /* Windows maximizes to the nearest monitor as long as its RECT is kept */
   GetClientRect( GetDesktopWindow(), &rt );
   SetWindowPos( wvt->window, HWND_TOP, rt.left, rt.top, rt.right - rt.left, rt.bottom - rt.top, SWP_FRAMECHANGED );

   if( wvt->resizeMode == X_GTI_RESIZEMODE_FONT )
      s_gt_wvt_FitSize( wvt );
   else
      s_gt_wvt_ResizeFromWindow( wvt );

   return 1;
}

/* ---------------------------------------------------------------------------------------------
   GT functions
   --------------------------------------------------------------------------------------------- */

static void s_gt_wvt_Init( X_GT_BASE *gt, int stdinHandle, int stdoutHandle, int stderrHandle )
{
   STARTUPINFOW si;
   X_GTWVT     *wvt;
   wchar_t      path[MAX_PATH];
   wchar_t     *name, *dot;
   DWORD        length;
   int          pos;

   if( s_windowCount >= WVT_MAX_WINDOWS )
   {
      fputs( "xDev: maximum number of WVT windows reached, cannot create another one\n", stderr );
      x_quitRaise();
   }

   wvt = ( X_GTWVT * )x_xgrabz( sizeof( X_GTWVT ) );
   wvt->gt = gt;
   wvt->instance = GetModuleHandleW( NULL );

   for( pos = 0; s_windows[pos] != NULL; ++pos )
      ;
   s_windows[pos] = wvt;
   wvt->number = pos;
   if( ++s_windowCount == 1 )
   {
      WNDCLASSW wndclass;

      memset( &wndclass, 0, sizeof( wndclass ) );
      wndclass.style = CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS;
      wndclass.lpfnWndProc = s_gt_wvt_WndProc;
      wndclass.hInstance = wvt->instance;
      wndclass.hCursor = LoadCursor( NULL, IDC_ARROW );
      wndclass.lpszClassName = s_className;

      if( !RegisterClassW( &wndclass ) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS )
         fputs( "xDev: failed to register WVT window class\n", stderr );
   }

   GetStartupInfoW( &si );
   wvt->showCommand = IIF( si.dwFlags & STARTF_USESHOWWINDOW, si.wShowWindow, SW_SHOWNORMAL );
   wvt->rows = WVT_DEFAULT_ROWS;
   wvt->cols = WVT_DEFAULT_COLS;

   memcpy( wvt->colors, s_defaultColors, sizeof( wvt->colors ) );

   /* as SDL3 and GLFW do, the process draws in the pixels of the monitor; when another part of
      the program has set the DPI awareness already, the call fails and that one stays */
   SetProcessDpiAwarenessContext( DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2 );
   wvt->dpi = ( int )GetDpiForSystem();

   /* default font parameters, if not changed by the program */
   wvt->cellSize.x = s_gt_wvt_ToPixel( wvt, WVT_DEFAULT_FONT_WIDTH );
   wvt->cellSize.y = s_gt_wvt_ToPixel( wvt, WVT_DEFAULT_FONT_HEIGHT );
   wvt->fontWidth = wvt->cellSize.x;
   wvt->fontHeight = wvt->cellSize.y;
   wvt->fontWeight = FW_NORMAL;
   wvt->fontQuality = DEFAULT_QUALITY;
   wvt->fontAttribute = WVT_DEFAULT_FONT_ATTR;
   wcsncpy( wvt->fontFace, WVT_DEFAULT_FONT_NAME, LF_FACESIZE - 1 );

   wvt->caretBlink = X_GT_CURSOR_BLINK;
   wvt->centred = 1;
   wvt->newX = -1;
   wvt->newY = -1;

   wvt->selectCopyText = ( wchar_t * )x_xgrab( sizeof( L"Mark and Copy" ) );
   wcscpy( wvt->selectCopyText, L"Mark and Copy" );
   wvt->selectCopy = 0; /* off as in GTSDL3; "Mark" in the system menu is grayed until it is on */

   /* the default window title is the program name without its directory and extension */
   length = GetModuleFileNameW( NULL, path, MAX_PATH );
   path[IIF( length < MAX_PATH, length, MAX_PATH - 1 )] = L'\0';
   name = wcsrchr( path, L'\\' );
   name = IIF( name, name + 1, path );
   dot = wcsrchr( name, L'.' );
   if( dot )
      *dot = L'\0';
   wvt->title = ( wchar_t * )x_xgrabArray( wcslen( name ) + 1, sizeof( wchar_t ) );
   wcscpy( wvt->title, name );

   wvt->resizable = 1;
   wvt->resizeMode = X_GTI_RESIZEMODE_ROWS;
   wvt->mouseOn = 1;
   wvt->sizeStep = 1;
   wvt->opacity = 1.0f;

   gt->driverData[s_gtId] = wvt;
   s_super.Init( gt, stdinHandle, stdoutHandle, stderrHandle );
   X_GTSELF_SETFLAG( gt, X_GTI_COMPATBUFFER, 0 );
   X_GTSELF_RESIZE( gt, wvt->rows, wvt->cols );
   X_GTSELF_SETFLAG( gt, X_GTI_REDRAWMAX, 1 );
   X_GTSELF_SEMICOLD( gt );
}

static void s_gt_wvt_Exit( X_GT_BASE *gt )
{
   X_GTWVT *wvt = X_GTWVT_GET( gt );

   s_windows[wvt->number] = NULL;
   if( --s_windowCount == 0 )
      UnregisterClassW( s_className, wvt->instance );

   while( wvt->menu )
   {
      X_GTWVT_MENU *menu = wvt->menu;

      wvt->menu = menu->next;
      x_xfree( menu->name );
      x_xfree( menu );
   }

   x_xfree( wvt->selectCopyText );
   x_xfree( wvt->title );

   s_gt_wvt_DestroyCanvas( wvt );
   if( wvt->font )
      DeleteObject( wvt->font );

   if( wvt->window )
      DestroyWindow( wvt->window );

   if( wvt->ownedIcon )
      DestroyIcon( wvt->ownedIcon );

   x_xfree( wvt );

   gt->driverData[s_gtId] = NULL;
   s_super.Exit( gt );
}

static int s_gt_wvt_SetMode( X_GT_BASE *gt, int row, int col )
{
   X_GTWVT *wvt = X_GTWVT_GET( gt );
   int      result = 0;

   if( wvt->window )
   {
      if( wvt->resizable && !wvt->maximized )
      {
         result = s_gt_wvt_InitWindow( wvt, row, col, NULL );
         gt->pFuncTable->Refresh( gt );
      }
      else
      {
         /* maximized, full screen or not resizable: the font size changes to fit the new mode */
         int   height = ( wvt->fontHeight * wvt->rows ) / row;
         int   width = ( wvt->fontWidth * wvt->cols ) / col;
         HFONT font = s_gt_wvt_GetFont( wvt->fontFace, height, width, wvt->fontWeight, wvt->fontQuality );

         if( font )
         {
            result = s_gt_wvt_InitWindow( wvt, row, col, font );
            s_gt_wvt_FitSize( wvt );
            if( wvt->maximized )
            {
               /* the window size is the same, but the margins may have changed */
               InvalidateRect( wvt->window, NULL, TRUE );
               UpdateWindow( wvt->window );
            }

            gt->pFuncTable->Refresh( gt );
         }
      }
   }
   else
   {
      result = s_gt_wvt_SetWindowSize( wvt, row, col );
      X_GTSELF_SEMICOLD( gt );
   }

   return result;
}

static const char *s_gt_wvt_Version( X_GT_BASE *gt, int type )
{
   UNUSED( gt );

   if( type == 0 )
      return "WVT";

   return "xDev Terminal: Windows GUI console (WVT)";
}

/* cells keep the 24-bit colors of DispOutAtExt() and DispBoxExt(), see s_gt_wvt_CellColors() */
static int s_gt_wvt_TrueColor( X_GT_BASE *gt )
{
   UNUSED( gt );

   return 1;
}

static int s_gt_wvt_ReadKey( X_GT_BASE *gt, int eventMask )
{
   X_GTWVT *wvt = X_GTWVT_GET( gt );
   int      key = 0;

   UNUSED( eventMask );

   /* like GTSDL3, the window is open while the program waits for input */
   s_gt_wvt_CreateConsoleWindow( wvt );
   s_gt_wvt_PumpEvents( wvt );

   if( wvt->resized )
   {
      wvt->resized = 0;
      s_super.Refresh( gt );
   }

   if( wvt->keyOut != wvt->keyIn )
   {
      key = wvt->keys[wvt->keyOut];
      if( ++wvt->keyOut >= WVT_KEY_QUEUE_SIZE )
         wvt->keyOut = 0;
   }

   return key;
}

static int s_gt_wvt_mouse_IsPresent( X_GT_BASE *gt )
{
   return X_GTWVT_GET( gt )->mouseOn;
}

static void s_gt_wvt_mouse_GetPos( X_GT_BASE *gt, int *row, int *col )
{
   X_GTWVT *wvt = X_GTWVT_GET( gt );

   *row = wvt->mousePos.y;
   *col = wvt->mousePos.x;
}

static void s_gt_wvt_mouse_SetPos( X_GT_BASE *gt, int row, int col )
{
   s_gt_wvt_SetMousePos( X_GTWVT_GET( gt ), row, col );
}

static int s_gt_wvt_mouse_ButtonState( X_GT_BASE *gt, int button )
{
   if( !X_GTWVT_GET( gt )->mouseOn )
      return 0;

   switch( button )
   {
      case X_MBUTTON_LEFT: return ( GetKeyState( VK_LBUTTON ) & 0x8000 ) != 0;
      case X_MBUTTON_RIGHT: return ( GetKeyState( VK_RBUTTON ) & 0x8000 ) != 0;
      case X_MBUTTON_MIDDLE: return ( GetKeyState( VK_MBUTTON ) & 0x8000 ) != 0;
   }

   return 0;
}

static int s_gt_wvt_mouse_CountButton( X_GT_BASE *gt )
{
   UNUSED( gt );

   return GetSystemMetrics( SM_CMOUSEBUTTONS );
}

static int s_gt_wvt_PaletteValue( const uint8_t color[3] )
{
   return ( int )color[0] | ( ( int )color[1] << 8 ) | ( ( int )color[2] << 16 );
}

static int s_gt_wvt_SetPaletteValue( X_GTWVT *wvt, int index, int value )
{
   uint32_t color = ( uint32_t )value;
   uint8_t  red = ( uint8_t )color;
   uint8_t  green = ( uint8_t )( color >> 8 );
   uint8_t  blue = ( uint8_t )( color >> 16 );

   if( wvt->colors[index][0] != red || wvt->colors[index][1] != green || wvt->colors[index][2] != blue )
   {
      wvt->colors[index][0] = red;
      wvt->colors[index][1] = green;
      wvt->colors[index][2] = blue;
      return 1;
   }

   return 0;
}

/* ---------------------------------------------------------------------------------------------
   X_GTI_MSGBOX: a modal dialog with any buttons, like SDL_ShowMessageBox() in GTSDL3
   --------------------------------------------------------------------------------------------- */

static INT_PTR CALLBACK s_gt_wvt_MsgBoxProc( HWND dlg, UINT message, WPARAM wParam, LPARAM lParam )
{
   switch( message )
   {
      case WM_INITDIALOG:
      {
         X_GTWVT_MSGBOX *box = ( X_GTWVT_MSGBOX * )lParam;
         HWND            ownerWindow = GetWindow( dlg, GW_OWNER );
         HDC             hdc = GetDC( dlg );
         HGDIOBJ         oldFont = SelectObject( hdc, box->font );
         int             iconSize = GetSystemMetrics( SM_CXICON );
         int            *widths = ( int * )x_xgrab( ( size_t )box->count * sizeof( int ) );
         int             unit, gap, buttonH, buttonsW = 0, clientW, clientH, frameW, frameH, x, y, i;
         DWORD           style, exStyle;
         TEXTMETRICW     tm;
         RECT            text = { 0, 0, 0, 0 }, frame, owner;
         HWND            child;

         /* sizes follow the message font: margins of one line height, buttons as in MessageBox() */
         GetTextMetricsW( hdc, &tm );
         unit = tm.tmHeight;
         gap = unit / 2;
         buttonH = unit * 7 / 4;
         text.right = unit * 30;
         DrawTextW( hdc, box->text, -1, &text, DT_CALCRECT | DT_WORDBREAK | DT_NOPREFIX | DT_EXPANDTABS );
         for( i = 0; i < box->count; ++i )
         {
            SIZE size;

            GetTextExtentPoint32W( hdc, box->buttons[i], ( int )wcslen( box->buttons[i] ), &size );
            widths[i] = X_MAX( size.cx + unit * 2, unit * 5 );
            buttonsW += widths[i] + IIF( i > 0, gap, 0 );
         }
         SelectObject( hdc, oldFont );
         ReleaseDC( dlg, hdc );

         clientW = X_MAX( unit + iconSize + unit + text.right + unit, unit + buttonsW + unit );
         y = unit + X_MAX( iconSize, text.bottom ) + unit;
         clientH = y + buttonH + unit;

         SetWindowTextW( dlg, box->title );
         SendMessageW( dlg, WM_SETFONT, ( WPARAM )box->font, FALSE );

         style = WS_CHILD | WS_VISIBLE | SS_ICON;
         child = CreateWindowW( L"STATIC", NULL, style, unit, unit, iconSize, iconSize, dlg, NULL, NULL, NULL );
         SendMessageW( child, STM_SETICON, ( WPARAM )LoadIconW( NULL, IDI_INFORMATION ), 0 );

         style = WS_CHILD | WS_VISIBLE | SS_LEFT | SS_NOPREFIX;
         x = unit + iconSize + unit;
         child = CreateWindowW( L"STATIC", box->text, style, x, unit, text.right, text.bottom, dlg, NULL, NULL, NULL );
         SendMessageW( child, WM_SETFONT, ( WPARAM )box->font, FALSE );

         /* buttons in the given order, aligned to the right; the first one is the default */
         x = clientW - unit - buttonsW;
         for( i = 0; i < box->count; ++i )
         {
            HMENU id = ( HMENU )( INT_PTR )( WVT_MSGBOX_FIRST_ID + i );

            style = WS_CHILD | WS_VISIBLE | WS_TABSTOP | IIF( i == 0, BS_DEFPUSHBUTTON, BS_PUSHBUTTON );
            child = CreateWindowW( L"BUTTON", box->buttons[i], style, x, y, widths[i], buttonH, dlg, id, NULL, NULL );
            SendMessageW( child, WM_SETFONT, ( WPARAM )box->font, FALSE );
            if( i == 0 )
               SetFocus( child );
            x += widths[i] + gap;
         }
         x_xfree( widths );
         SendMessageW( dlg, DM_SETDEFID, WVT_MSGBOX_FIRST_ID, 0 );

         /* the dialog is centred on the terminal window, or on the work area */
         SetRect( &frame, 0, 0, clientW, clientH );
         style = ( DWORD )GetWindowLongW( dlg, GWL_STYLE );
         exStyle = ( DWORD )GetWindowLongW( dlg, GWL_EXSTYLE );
         AdjustWindowRectEx( &frame, style, FALSE, exStyle );
         if( !ownerWindow || IsIconic( ownerWindow ) || !GetWindowRect( ownerWindow, &owner ) )
            SystemParametersInfoW( SPI_GETWORKAREA, 0, &owner, 0 );
         frameW = frame.right - frame.left;
         frameH = frame.bottom - frame.top;
         x = owner.left + ( owner.right - owner.left - frameW ) / 2;
         y = owner.top + ( owner.bottom - owner.top - frameH ) / 2;
         SetWindowPos( dlg, NULL, x, y, frameW, frameH, SWP_NOZORDER );
         return FALSE;
      }

      case WM_COMMAND:
         /* Escape and the close button give 0, as a message box closed without a button in GTSDL3 */
         if( LOWORD( wParam ) == IDCANCEL )
            EndDialog( dlg, 0 );
         else if( LOWORD( wParam ) >= WVT_MSGBOX_FIRST_ID )
            EndDialog( dlg, LOWORD( wParam ) - WVT_MSGBOX_FIRST_ID + 1 );
         return TRUE;
   }

   return FALSE;
}

/* text is the message or the descriptor { cTitle, cText, cInfo, cDetail, aButtons } (xDev has no
   hash type); the result is the number of the chosen button or 0 */
static int s_gt_wvt_MessageBox( X_GTWVT *wvt, X_ITEM *text, X_ITEM *buttons )
{
   X_ITEM           *items[3] = { text, NULL, NULL };
   X_ITEM           *titleItem = NULL;
   wchar_t          *parts[3], *message, *dest, *title;
   wchar_t          *defButton = L"OK";
   size_t            lengths[3], total = 0;
   NONCLIENTMETRICSW ncm;
   X_GTWVT_MSGBOX    box;
   INT_PTR           result;
   int               index;

   union
   {
      DLGTEMPLATE tpl;
      WORD        words[12]; /* the template, then empty menu, class and title */
   } dlg;

   if( X_GTWVT_IS( text, X_IT_ARRAY ) )
   {
      if( !buttons )
         buttons = x_arrayGetItemPtr( text, 5 );

      titleItem = x_arrayGetItemPtr( text, 1 );
      items[0] = x_arrayGetItemPtr( text, 2 );
      items[1] = x_arrayGetItemPtr( text, 3 );
      items[2] = x_arrayGetItemPtr( text, 4 );
   }

   /* non-empty parts are separated by an empty line */
   for( index = 0; index < 3; ++index )
   {
      parts[index] = s_gt_wvt_ItemToWide( items[index] );
      lengths[index] = wcslen( parts[index] );
      total += lengths[index];
   }
   message = dest = ( wchar_t * )x_xgrab( ( total + 5 ) * sizeof( wchar_t ) );
   for( index = 0; index < 3; ++index )
   {
      if( lengths[index] )
      {
         if( dest != message )
         {
            *dest++ = L'\n';
            *dest++ = L'\n';
         }
         memcpy( dest, parts[index], lengths[index] * sizeof( wchar_t ) );
         dest += lengths[index];
      }
      x_xfree( parts[index] );
   }
   *dest = L'\0';

   title = IIF( titleItem, s_gt_wvt_ItemToWide( titleItem ), NULL );
   box.title = IIF( title, title, wvt->title );
   box.text = message;
   box.count = IIF( X_GTWVT_IS( buttons, X_IT_ARRAY ), ( int )x_arrayLen( buttons ), 0 );
   if( box.count )
   {
      box.buttons = ( wchar_t ** )x_xgrab( ( size_t )box.count * sizeof( wchar_t * ) );
      for( index = 0; index < box.count; ++index )
         box.buttons[index] = s_gt_wvt_ItemToWide( x_arrayGetItemPtr( buttons, ( size_t )index + 1 ) );
   }
   else
   {
      box.buttons = &defButton;
      box.count = 1;
   }

   ncm.cbSize = sizeof( ncm );
   SystemParametersInfoW( SPI_GETNONCLIENTMETRICS, sizeof( ncm ), &ncm, 0 );
   box.font = CreateFontIndirectW( &ncm.lfMessageFont );

   memset( &dlg, 0, sizeof( dlg ) );
   dlg.tpl.style = DS_MODALFRAME | WS_POPUP | WS_CAPTION | WS_SYSMENU;

   s_gt_wvt_CreateConsoleWindow( wvt );
   result = DialogBoxIndirectParamW( wvt->instance, &dlg.tpl, wvt->window, s_gt_wvt_MsgBoxProc, ( LPARAM )&box );

   DeleteObject( box.font );
   if( box.buttons != &defButton )
   {
      for( index = 0; index < box.count; ++index )
         x_xfree( box.buttons[index] );
      x_xfree( box.buttons );
   }
   x_xfree( title );
   x_xfree( message );

   return ( int )X_MAX( result, 0 );
}

static int s_gt_wvt_Alert( X_GT_BASE *gt, X_ITEM *message, X_ITEM *options, int colorNormal, int colorHigh,
                           double delay )
{
   X_GTWVT *wvt = X_GTWVT_GET( gt );

   /* the message box has no timeout, so a timed Alert() keeps the base implementation */
   if( wvt->msgAlert && delay * 100 < 1 )
      return s_gt_wvt_MessageBox( wvt, message, options );

   return s_super.Alert( gt, message, options, colorNormal, colorHigh, delay );
}

static int s_gt_wvt_Info( X_GT_BASE *gt, int type, X_GT_INFO *info )
{
   X_GTWVT *wvt = X_GTWVT_GET( gt );
   int      number;
   int      change;
   int      valid;

   switch( type )
   {
      case X_GTI_MAXIMIZED:
         info->pResult = x_itemPutL( info->pResult, wvt->maximized );
         change = X_GTWVT_IS( info->pNewVal, X_IT_LOGICAL ) && x_itemGetL( info->pNewVal ) != wvt->maximized;
         if( change && !wvt->fullScreen )
         {
            if( !wvt->window )
               wvt->maximized = x_itemGetL( info->pNewVal );
            else
               ShowWindow( wvt->window, IIF( wvt->maximized, SW_RESTORE, SW_SHOWMAXIMIZED ) );
         }
         break;

      case X_GTI_ISFULLSCREEN:
         info->pResult = x_itemPutL( info->pResult, wvt->fullScreen );
         if( X_GTWVT_IS( info->pNewVal, X_IT_LOGICAL ) && x_itemGetL( info->pNewVal ) != wvt->fullScreen )
         {
            if( wvt->window )
               s_gt_wvt_SetFullscreen( gt );
            else
               wvt->fullScreen = x_itemGetL( info->pNewVal );
         }
         break;

      case X_GTI_ALTENTER:
         info->pResult = x_itemPutL( info->pResult, wvt->altEnter );
         if( X_GTWVT_IS( info->pNewVal, X_IT_LOGICAL ) )
            wvt->altEnter = x_itemGetL( info->pNewVal );
         break;

      case X_GTI_ISSCREENPOS:
      case X_GTI_KBDSUPPORT:
      case X_GTI_ISGRAPHIC:
      case X_GTI_ISUNICODE: info->pResult = x_itemPutL( info->pResult, 1 ); break;

      case X_GTI_ONLINE: info->pResult = x_itemPutL( info->pResult, wvt->window != NULL ); break;

      case X_GTI_VERSION:
         info->pResult = x_itemPutC( info->pResult, s_gt_wvt_Version( gt, x_itemGetNI( info->pNewVal ) ) );
         break;

      case X_GTI_FONTSIZE:
         /* font sizes are logical pixels, as all pixel values of the program */
         info->pResult = x_itemPutNI( info->pResult, s_gt_wvt_ToLogical( wvt, wvt->cellSize.y ) );
         number = s_gt_wvt_ToPixel( wvt, x_itemGetNI( info->pNewVal ) );
         if( number > 0 )
         {
            HFONT font = s_gt_wvt_GetFont( wvt->fontFace, number, wvt->fontWidth, wvt->fontWeight, wvt->fontQuality );
            if( font )
            {
               wvt->fontHeight = number;
               if( wvt->window )
               {
                  s_gt_wvt_ResetWindowSize( wvt, font );
                  gt->pFuncTable->Refresh( gt );
               }
               else
               {
                  TEXTMETRIC tm;
                  HWND       desktop = GetDesktopWindow();
                  HDC        hdc = GetDC( desktop );
                  HFONT      oldFont = ( HFONT )SelectObject( hdc, font );

                  SetTextCharacterExtra( hdc, 0 );
                  GetTextMetrics( hdc, &tm );
                  SelectObject( hdc, oldFont );
                  ReleaseDC( desktop, hdc );

                  wvt->cellSize.x = tm.tmAveCharWidth;
                  wvt->cellSize.y = tm.tmHeight;

                  DeleteObject( font );
               }
            }
         }
         break;

      case X_GTI_FONTWIDTH:
         info->pResult = x_itemPutNI( info->pResult, s_gt_wvt_ToLogical( wvt, wvt->cellSize.x ) );
         number = s_gt_wvt_ToPixel( wvt, x_itemGetNI( info->pNewVal ) );
         if( number > 0 )
            wvt->fontWidth = number; /* used by the next font size change */
         break;

      case X_GTI_FONTNAME:
         info->pResult = s_gt_wvt_PutWide( info->pResult, wvt->fontFace, ( int )wcslen( wvt->fontFace ) );
         if( X_GTWVT_IS( info->pNewVal, X_IT_STRING ) )
         {
            wchar_t *face = s_gt_wvt_ItemToWide( info->pNewVal );

            wcsncpy( wvt->fontFace, face, LF_FACESIZE - 1 );
            wvt->fontFace[LF_FACESIZE - 1] = L'\0';
            x_xfree( face );
         }
         break;

      case X_GTI_FONTWEIGHT:
         switch( wvt->fontWeight )
         {
            case FW_THIN:
            case FW_EXTRALIGHT:
            case FW_LIGHT: number = X_GTI_FONTW_THIN; break;
            case FW_DONTCARE:
            case FW_NORMAL:
            case FW_MEDIUM: number = X_GTI_FONTW_NORMAL; break;
            case FW_SEMIBOLD:
            case FW_BOLD:
            case FW_EXTRABOLD:
            case FW_HEAVY: number = X_GTI_FONTW_BOLD; break;
            default: number = 0; break;
         }
         info->pResult = x_itemPutNI( info->pResult, number );
         if( X_GTWVT_IS( info->pNewVal, X_IT_NUMERIC ) )
         {
            /* used by the next font size change */
            switch( x_itemGetNI( info->pNewVal ) )
            {
               case X_GTI_FONTW_THIN: wvt->fontWeight = FW_LIGHT; break;
               case X_GTI_FONTW_NORMAL: wvt->fontWeight = FW_NORMAL; break;
               case X_GTI_FONTW_BOLD: wvt->fontWeight = FW_BOLD; break;
            }
         }
         break;

      case X_GTI_FONTQUALITY:
         switch( wvt->fontQuality )
         {
            case ANTIALIASED_QUALITY: number = X_GTI_FONTQ_HIGH; break;
            case DEFAULT_QUALITY:
            case DRAFT_QUALITY: number = X_GTI_FONTQ_NORMAL; break;
            case NONANTIALIASED_QUALITY:
            case PROOF_QUALITY: number = X_GTI_FONTQ_DRAFT; break;
            default: number = 0; break;
         }
         info->pResult = x_itemPutNI( info->pResult, number );
         if( X_GTWVT_IS( info->pNewVal, X_IT_NUMERIC ) )
         {
            switch( x_itemGetNI( info->pNewVal ) )
            {
               case X_GTI_FONTQ_HIGH: wvt->fontQuality = ANTIALIASED_QUALITY; break;
               case X_GTI_FONTQ_NORMAL: wvt->fontQuality = DEFAULT_QUALITY; break;
               case X_GTI_FONTQ_DRAFT: wvt->fontQuality = DRAFT_QUALITY; break;
            }
         }
         break;

      case X_GTI_FONTATTRIBUTE:
         info->pResult = x_itemPutNI( info->pResult, wvt->fontAttribute );
         if( X_GTWVT_IS( info->pNewVal, X_IT_NUMERIC ) )
         {
            /* FIXMETRIC and CLRBKG are kept for compatibility; every cell is already drawn
            on its own with a filled background. DRAWBOX draws box characters and blocks. */
            int mask = X_GTI_FONTA_FIXMETRIC | X_GTI_FONTA_CLRBKG | X_GTI_FONTA_DRAWBOX | X_GTI_FONTA_CTRLCHARS;

            wvt->fontAttribute = x_itemGetNI( info->pNewVal ) & mask;
            if( wvt->window )
               X_GTSELF_EXPOSEAREA( gt, 0, 0, wvt->rows - 1, wvt->cols - 1 );
         }
         break;

      case X_GTI_FONTSEL: info->pResult = x_itemPutC( info->pResult, "" ); break;

      case X_GTI_SCREENHEIGHT:
         info->pResult = x_itemPutNI( info->pResult, s_gt_wvt_CellLogical( wvt, 1 ) * wvt->rows );
         number = s_gt_wvt_ToPixel( wvt, x_itemGetNI( info->pNewVal ) );
         /* not allowed when maximized or in full screen; follows the resize mode */
         if( number > 0 && !wvt->maximized && !wvt->fullScreen && wvt->window )
         {
            RECT ci;

            GetClientRect( wvt->window, &ci );
            if( ci.bottom != number )
            {
               RECT wi;

               GetWindowRect( wvt->window, &wi );
               number += wi.bottom - wi.top - ci.bottom;
               SetWindowPos( wvt->window, NULL, wi.left, wi.top, wi.right - wi.left, number, SWP_NOZORDER );
            }
         }
         break;

      case X_GTI_SCREENWIDTH:
         info->pResult = x_itemPutNI( info->pResult, s_gt_wvt_CellLogical( wvt, 0 ) * wvt->cols );
         number = s_gt_wvt_ToPixel( wvt, x_itemGetNI( info->pNewVal ) );
         if( number > 0 && !wvt->maximized && !wvt->fullScreen && wvt->window )
         {
            RECT ci;

            GetClientRect( wvt->window, &ci );
            if( ci.right != number )
            {
               RECT wi;

               GetWindowRect( wvt->window, &wi );
               number += wi.right - wi.left - ci.right;
               SetWindowPos( wvt->window, NULL, wi.left, wi.top, number, wi.bottom - wi.top, SWP_NOZORDER );
            }
         }
         break;

      case X_GTI_DESKTOPWIDTH:
      case X_GTI_DESKTOPHEIGHT:
      {
         RECT desktopRect;

         GetWindowRect( GetDesktopWindow(), &desktopRect );
         if( type == X_GTI_DESKTOPWIDTH )
            info->pResult = x_itemPutNI( info->pResult, desktopRect.right - desktopRect.left );
         else
            info->pResult = x_itemPutNI( info->pResult, desktopRect.bottom - desktopRect.top );
         break;
      }

      case X_GTI_DESKTOPCOLS:
      case X_GTI_DESKTOPROWS:
      {
         RECT desktopRect;

         GetClientRect( GetDesktopWindow(), &desktopRect );
         if( type == X_GTI_DESKTOPCOLS )
            info->pResult = x_itemPutNI( info->pResult, ( desktopRect.right - desktopRect.left ) / wvt->cellSize.x );
         else
            info->pResult = x_itemPutNI( info->pResult, ( desktopRect.bottom - desktopRect.top ) / wvt->cellSize.y );
         break;
      }

      case X_GTI_WINTITLE:
         info->pResult = s_gt_wvt_PutWide( info->pResult, wvt->title, ( int )wcslen( wvt->title ) );
         if( X_GTWVT_IS( info->pNewVal, X_IT_STRING ) )
         {
            x_xfree( wvt->title );
            wvt->title = s_gt_wvt_ItemToWide( info->pNewVal );
            if( wvt->window )
               SetWindowTextW( wvt->window, wvt->title );
         }
         break;

      case X_GTI_ICONFILE:
      case X_GTI_ICONRES:
      {
         HICON icon = NULL, iconToFree = NULL;

         if( X_GTWVT_IS( info->pNewVal, X_IT_STRING ) )
         {
            wchar_t *name = s_gt_wvt_ItemToWide( info->pNewVal );

            if( type == X_GTI_ICONFILE )
            {
               icon = ( HICON )LoadImageW( NULL, name, IMAGE_ICON, 0, 0, LR_LOADFROMFILE | LR_DEFAULTSIZE );
               iconToFree = icon;
            }
            else
               icon = LoadIconW( wvt->instance, name );
            x_xfree( name );
         }
         else if( X_GTWVT_IS( info->pNewVal, X_IT_NUMERIC ) )
            icon = LoadIconW( wvt->instance, MAKEINTRESOURCEW( x_itemGetNI( info->pNewVal ) ) );

         if( icon != wvt->icon )
         {
            if( wvt->window )
            {
               SendNotifyMessage( wvt->window, WM_SETICON, ICON_SMALL, ( LPARAM )icon ); /* title bar icon */
               SendNotifyMessage( wvt->window, WM_SETICON, ICON_BIG, ( LPARAM )icon );   /* task list icon */
            }
            if( wvt->ownedIcon )
               DestroyIcon( wvt->ownedIcon );
            wvt->ownedIcon = iconToFree;
            wvt->icon = icon;
         }
         info->pResult = x_itemPutPtr( info->pResult, ( void * )wvt->icon );
         break;
      }

      case X_GTI_VIEWPORTWIDTH:
      case X_GTI_VIEWMAXWIDTH: info->pResult = x_itemPutNI( info->pResult, wvt->cols ); break;

      case X_GTI_VIEWPORTHEIGHT:
      case X_GTI_VIEWMAXHEIGHT: info->pResult = x_itemPutNI( info->pResult, wvt->rows ); break;

      case X_GTI_KBDSHIFTS:
      {
         size_t i;
         int    shifts = 0;

         for( i = 0; i < sizeof( s_kbdKeys ) / sizeof( s_kbdKeys[0] ); ++i )
         {
            if( GetKeyState( s_kbdKeys[i].vk ) & s_kbdKeys[i].mask )
               shifts |= s_kbdKeys[i].flag;
         }
         info->pResult = x_itemPutNI( info->pResult, shifts );

         /* only the lock keys can be changed; Windows toggles them by a key press */
         if( X_GTWVT_IS( info->pNewVal, X_IT_NUMERIC ) )
         {
            number = x_itemGetNI( info->pNewVal );
            for( i = 0; i < sizeof( s_kbdKeys ) / sizeof( s_kbdKeys[0] ); ++i )
            {
               if( s_kbdKeys[i].mask == 0x01 && ( ( shifts ^ number ) & s_kbdKeys[i].flag ) != 0 )
               {
                  keybd_event( ( BYTE )s_kbdKeys[i].vk, 0, KEYEVENTF_EXTENDEDKEY, 0 );
                  keybd_event( ( BYTE )s_kbdKeys[i].vk, 0, KEYEVENTF_EXTENDEDKEY | KEYEVENTF_KEYUP, 0 );
               }
            }
         }
         break;
      }

      case X_GTI_CLIPBOARDDATA:
         if( X_GTWVT_IS( info->pNewVal, X_IT_STRING ) )
         {
            int      length;
            wchar_t *text = s_gt_wvt_ToWide( x_itemGetCPtr( info->pNewVal ), x_itemGetCLen( info->pNewVal ), &length );

            s_gt_wvt_SetClipboard( text, ( size_t )length );
            x_xfree( text );
         }
         else
         {
            if( !info->pResult )
               info->pResult = x_itemNew( NULL );
            s_gt_wvt_GetClipboard( info->pResult );
         }
         break;

      case X_GTI_CLIPBOARDPASTE:
      {
         X_ITEM *text = x_itemNew( NULL );
         int     read = s_gt_wvt_GetClipboard( text );

         if( read )
            gt->pFuncTable->InkeySetText( gt, x_itemGetCPtr( text ), x_itemGetCLen( text ), 1 );
         x_itemRelease( text );
         info->pResult = x_itemPutL( info->pResult, read );
         break;
      }

      case X_GTI_SOUND:
         valid = X_GTWVT_IS( info->pNewVal, X_IT_STRING );
         if( valid && x_itemGetCLen( info->pNewVal ) != strlen( x_itemGetCPtr( info->pNewVal ) ) )
            valid = 0;
         if( valid )
         {
            wchar_t *filename = s_gt_wvt_ItemToWide( info->pNewVal );
            int      played = PlaySoundW( filename, NULL, SND_ASYNC | SND_FILENAME | SND_NODEFAULT ) != FALSE;

            info->pResult = x_itemPutL( info->pResult, played );
            x_xfree( filename );
         }
         else
            info->pResult = x_itemPutL( info->pResult, 0 );
         break;

      case X_GTI_CURSORBLINKRATE:
         info->pResult = x_itemPutNI( info->pResult, wvt->caretBlink );
         if( X_GTWVT_IS( info->pNewVal, X_IT_NUMERIC ) )
         {
            /* the new rate starts with the cursor shown */
            wvt->caretBlink = X_MAX( x_itemGetNI( info->pNewVal ), 0 );
            if( wvt->window && !IsRectEmpty( &wvt->caretRect ) )
               InvalidateRect( wvt->window, &wvt->caretRect, FALSE );
            SetRectEmpty( &wvt->caretRect );
            s_gt_wvt_UpdateCursor( wvt );
         }
         break;

      case X_GTI_SCREENSIZE:
         if( !info->pResult )
            info->pResult = x_itemNew( NULL );

         x_arrayNew( info->pResult, 2 );
         x_arraySetNI( info->pResult, 2, s_gt_wvt_CellLogical( wvt, 1 ) * wvt->rows );
         x_arraySetNI( info->pResult, 1, s_gt_wvt_CellLogical( wvt, 0 ) * wvt->cols );

         if( X_GTWVT_IS( info->pNewVal, X_IT_ARRAY ) && x_arrayLen( info->pNewVal ) == 2 )
         {
            int y = s_gt_wvt_ToPixel( wvt, x_arrayGetNI( info->pNewVal, 2 ) );
            int x = s_gt_wvt_ToPixel( wvt, x_arrayGetNI( info->pNewVal, 1 ) );

            /* not allowed when maximized or in full screen; follows the resize mode */
            if( y > 0 && x > 0 && !wvt->maximized && !wvt->fullScreen && wvt->window )
            {
               RECT ci;

               GetClientRect( wvt->window, &ci );
               if( ci.right != x || ci.bottom != y )
               {
                  RECT wi;

                  GetWindowRect( wvt->window, &wi );
                  x += wi.right - wi.left - ci.right;
                  y += wi.bottom - wi.top - ci.bottom;
                  SetWindowPos( wvt->window, NULL, wi.left, wi.top, x, y, SWP_NOZORDER );
               }
            }
         }
         break;

      case X_GTI_RESIZABLE:
         info->pResult = x_itemPutL( info->pResult, wvt->resizable );
         if( info->pNewVal )
         {
            int newValue = x_itemGetL( info->pNewVal );

            if( newValue != wvt->resizable )
            {
               wvt->resizable = newValue;
               if( wvt->window )
               {
                  LONG_PTR style = IIF( wvt->resizable, _WVT_WS_DEF, _WVT_WS_NORESIZE );
                  UINT     flags = SWP_NOACTIVATE | SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_DEFERERASE;

                  SetWindowLongPtrW( wvt->window, GWL_STYLE, style );
                  SetWindowPos( wvt->window, NULL, 0, 0, 0, 0, flags );
                  ShowWindow( wvt->window, SW_HIDE );
                  ShowWindow( wvt->window, SW_NORMAL );
               }
            }
         }
         break;

      case X_GTI_SYSMENUADD:
         info->pResult = x_itemPutL( info->pResult, 0 );
         if( X_GTWVT_IS( info->pNewVal, X_IT_NUMERIC ) )
         {
            number = x_itemGetNI( info->pNewVal );
            if( number != 0 )
            {
               int            add = X_GTWVT_IS( info->pNewVal2, X_IT_STRING );
               X_GTWVT_MENU **menu = &wvt->menu;
               int            event = SYS_EV_MARK;

               while( *menu )
               {
                  if( ( *menu )->key == number )
                     break;
                  event = X_MAX( event, ( *menu )->event );
                  menu = &( *menu )->next;
               }
               if( *menu )
               {
                  x_xfree( ( *menu )->name );
                  ( *menu )->name = NULL;
                  event = ( *menu )->event;
                  if( !add )
                  {
                     X_GTWVT_MENU *freed = *menu;

                     *menu = ( *menu )->next;
                     x_xfree( freed );
                  }
               }
               else
               {
                  if( add )
                  {
                     *menu = ( X_GTWVT_MENU * )x_xgrab( sizeof( X_GTWVT_MENU ) );
                     ( *menu )->key = number;
                     ( *menu )->event = event + 1;
                     ( *menu )->next = NULL;
                  }
                  event = 0;
               }
               if( add )
                  ( *menu )->name = s_gt_wvt_ItemToWide( info->pNewVal2 );
               if( wvt->window )
               {
                  HMENU sysMenu = GetSystemMenu( wvt->window, FALSE );

                  if( sysMenu )
                  {
                     if( event != 0 )
                        DeleteMenu( sysMenu, event, MF_BYCOMMAND );
                     if( add )
                        AppendMenuW( sysMenu, MF_STRING, ( *menu )->event, ( *menu )->name );
                  }
               }
               info->pResult = x_itemPutL( info->pResult, add || event != 0 );
            }
         }
         break;

      case X_GTI_SELECTCOPY:
         if( X_GTWVT_IS( info->pNewVal, X_IT_STRING ) )
         {
            const wchar_t *copyText = wvt->selectCopyText;

            info->pResult = s_gt_wvt_PutWide( info->pResult, copyText, ( int )wcslen( copyText ) );

            if( x_itemGetCLen( info->pNewVal ) )
            {
               HMENU sysMenu = IIF( wvt->window, GetSystemMenu( wvt->window, FALSE ), NULL );

               if( sysMenu || !wvt->window )
               {
                  UINT flags = MF_BYCOMMAND | MF_STRING | MF_ENABLED;

                  x_xfree( wvt->selectCopyText );
                  wvt->selectCopyText = s_gt_wvt_ItemToWide( info->pNewVal );
                  wvt->selectCopy = 1;
                  if( sysMenu )
                     ModifyMenuW( sysMenu, SYS_EV_MARK, flags, SYS_EV_MARK, wvt->selectCopyText );
               }
            }
         }
         else
         {
            info->pResult = x_itemPutL( info->pResult, wvt->selectCopy );
            if( info->pNewVal )
            {
               HMENU sysMenu = IIF( wvt->window, GetSystemMenu( wvt->window, FALSE ), NULL );
               UINT  state;

               wvt->selectCopy = x_itemGetL( info->pNewVal );
               state = IIF( wvt->selectCopy, MF_ENABLED, MF_GRAYED );
               if( sysMenu )
                  EnableMenuItem( sysMenu, SYS_EV_MARK, MF_BYCOMMAND | state );

               /* as in GTSDL3, switching the copy off cancels a selection; its release is skipped */
               if( !wvt->selectCopy )
               {
                  wvt->markStarted = 0;
                  if( wvt->marking )
                  {
                     wvt->marking = 0;
                     wvt->skipUp = 1;
                     ReleaseCapture();
                     RedrawWindow( wvt->window, NULL, NULL, RDW_INVALIDATE | RDW_UPDATENOW );
                  }
               }
            }
         }
         break;

      case X_GTI_CLOSABLE:
         info->pResult = x_itemPutL( info->pResult, wvt->closeMode == 0 );
         change = X_GTWVT_IS( info->pNewVal, X_IT_LOGICAL ) && x_itemGetL( info->pNewVal ) != ( wvt->closeMode == 0 );
         if( change )
         {
            wvt->closeMode = IIF( wvt->closeMode == 0, 1, 0 );
            if( wvt->window )
               s_gt_wvt_SetCloseButton( wvt );
         }
         break;

      case X_GTI_CLOSEMODE:
         info->pResult = x_itemPutNI( info->pResult, wvt->closeMode );
         if( X_GTWVT_IS( info->pNewVal, X_IT_NUMERIC ) )
         {
            number = x_itemGetNI( info->pNewVal );
            if( number >= 0 && number <= 2 && wvt->closeMode != number )
            {
               wvt->closeMode = number;
               if( wvt->window )
                  s_gt_wvt_SetCloseButton( wvt );
            }
         }
         break;

      case X_GTI_PALETTE:
         if( X_GTWVT_IS( info->pNewVal, X_IT_NUMERIC ) )
         {
            int index = x_itemGetNI( info->pNewVal );

            if( index >= 0 && index < 16 )
            {
               info->pResult = x_itemPutNI( info->pResult, s_gt_wvt_PaletteValue( wvt->colors[index] ) );

               if( info->pNewVal2 && s_gt_wvt_SetPaletteValue( wvt, index, x_itemGetNI( info->pNewVal2 ) ) )
                  X_GTSELF_EXPOSEAREA( gt, 0, 0, wvt->rows - 1, wvt->cols - 1 );
            }
         }
         else
         {
            int index;

            if( !info->pResult )
               info->pResult = x_itemNew( NULL );

            x_arrayNew( info->pResult, 16 );
            for( index = 0; index < 16; ++index )
               x_arraySetNI( info->pResult, index + 1, s_gt_wvt_PaletteValue( wvt->colors[index] ) );

            if( X_GTWVT_IS( info->pNewVal, X_IT_ARRAY ) && x_arrayLen( info->pNewVal ) == 16 )
            {
               int changed = 0;

               for( index = 0; index < 16; ++index )
               {
                  if( s_gt_wvt_SetPaletteValue( wvt, index, x_arrayGetNI( info->pNewVal, index + 1 ) ) )
                     changed = 1;
               }

               if( changed )
                  X_GTSELF_EXPOSEAREA( gt, 0, 0, wvt->rows - 1, wvt->cols - 1 );
            }
         }
         break;

      case X_GTI_RESIZEFONT:
         info->pResult = x_itemPutL( info->pResult, wvt->resizeMode == X_GTI_RESIZEMODE_FONT );
         if( X_GTWVT_IS( info->pNewVal, X_IT_LOGICAL ) )
            wvt->resizeMode = IIF( x_itemGetL( info->pNewVal ), X_GTI_RESIZEMODE_FONT, X_GTI_RESIZEMODE_ROWS );
         break;

      case X_GTI_SETPOS_XY:
      case X_GTI_SETPOS_ROWCOL:
      {
         RECT wi = { 0, 0, 0, 0 };
         int  x = 0, y = 0;

         if( wvt->window )
         {
            GetWindowRect( wvt->window, &wi );
            if( type == X_GTI_SETPOS_ROWCOL )
            {
               y = wi.left / wvt->cellSize.x;
               x = wi.top / wvt->cellSize.y;
            }
            else
            {
               x = wi.left;
               y = wi.top;
            }
         }

         if( !info->pResult )
            info->pResult = x_itemNew( NULL );
         x_arrayNew( info->pResult, 2 );
         x_arraySetNI( info->pResult, 1, x );
         x_arraySetNI( info->pResult, 2, y );

         if( X_GTWVT_IS( info->pNewVal, X_IT_NUMERIC ) && X_GTWVT_IS( info->pNewVal2, X_IT_NUMERIC ) )
         {
            x = x_itemGetNI( info->pNewVal );
            y = x_itemGetNI( info->pNewVal2 );
         }
         else if( X_GTWVT_IS( info->pNewVal, X_IT_ARRAY ) && x_arrayLen( info->pNewVal ) == 2 )
         {
            x = x_arrayGetNI( info->pNewVal, 1 );
            y = x_arrayGetNI( info->pNewVal, 2 );
         }
         else
            break;

         if( type == X_GTI_SETPOS_ROWCOL )
         {
            int c = y;

            y = x * wvt->cellSize.y;
            x = c * wvt->cellSize.x;
         }
         if( wvt->window )
            SetWindowPos( wvt->window, NULL, x, y, wi.right - wi.left, wi.bottom - wi.top, SWP_NOSIZE | SWP_NOZORDER );
         else
         {
            wvt->newX = x;
            wvt->newY = y;
         }
         break;
      }

      case X_GTI_WINHANDLE:
         info->pResult = x_itemPutPtr( info->pResult, wvt->window );
         break;

         /* the selectors below follow GTSDL3 */

      case X_GTI_ISCTWIN:
      case X_GTI_ISMULTIWIN: info->pResult = x_itemPutL( info->pResult, 0 ); break;

      case X_GTI_SCREENDEPTH: info->pResult = x_itemPutNI( info->pResult, 32 ); break;

      case X_GTI_DESKTOPDEPTH:
      {
         HDC hdc = GetDC( NULL );

         info->pResult = x_itemPutNI( info->pResult, GetDeviceCaps( hdc, BITSPIXEL ) );
         ReleaseDC( NULL, hdc );
         break;
      }

      /* the scale of the monitor of the window; window pixels are logical pixels times the scale */
      case X_GTI_DISPLAYSCALE:
      case X_GTI_CONTENTSCALE: info->pResult = x_itemPutND( info->pResult, wvt->dpi / 96.0 ); break;

      /* GDI draws in window pixels: the client area is both the window and the output size;
      the canvas in logical pixels is the logical screen */
      case X_GTI_WINDOWWIDTH:
      case X_GTI_WINDOWHEIGHT:
      case X_GTI_OUTPUTWIDTH:
      case X_GTI_OUTPUTHEIGHT:
      {
         RECT ci = { 0, 0, wvt->cellSize.x * wvt->cols, wvt->cellSize.y * wvt->rows };

         if( wvt->window )
            GetClientRect( wvt->window, &ci );
         if( type == X_GTI_WINDOWWIDTH || type == X_GTI_OUTPUTWIDTH )
            info->pResult = x_itemPutNI( info->pResult, ci.right );
         else
            info->pResult = x_itemPutNI( info->pResult, ci.bottom );
         break;
      }

      case X_GTI_LOGICALWIDTH:
         info->pResult = x_itemPutNI( info->pResult, s_gt_wvt_CellLogical( wvt, 0 ) * wvt->cols );
         break;

      case X_GTI_LOGICALHEIGHT:
         info->pResult = x_itemPutNI( info->pResult, s_gt_wvt_CellLogical( wvt, 1 ) * wvt->rows );
         break;

      case X_GTI_PIXELDENSITY: info->pResult = x_itemPutND( info->pResult, 1.0 ); break;

      case X_GTI_RENDERERNAME: info->pResult = x_itemPutC( info->pResult, "WVT/GDI" ); break;

      case X_GTI_MINIMIZED:
      {
         int mined = wvt->window && IsIconic( wvt->window );

         info->pResult = x_itemPutL( info->pResult, mined );
         if( info->pNewVal )
         {
            if( x_itemGetL( info->pNewVal ) )
            {
               s_gt_wvt_CreateConsoleWindow( wvt );
               ShowWindow( wvt->window, SW_MINIMIZE );
            }
            else if( mined )
               ShowWindow( wvt->window, SW_RESTORE );
         }
         break;
      }

      case X_GTI_MOUSESTATUS:
         info->pResult = x_itemPutL( info->pResult, wvt->mouseOn );
         if( info->pNewVal )
            wvt->mouseOn = x_itemGetL( info->pNewVal );
         break;

      case X_GTI_MOUSEPOS_XY:
      {
         int x = ( int )floor( ( double )wvt->mouseX * s_gt_wvt_CellLogical( wvt, 0 ) / wvt->cellSize.x );
         int y = ( int )floor( ( double )wvt->mouseY * s_gt_wvt_CellLogical( wvt, 1 ) / wvt->cellSize.y );

         if( !info->pResult )
            info->pResult = x_itemNew( NULL );
         x_arrayNew( info->pResult, 2 );
         x_arraySetNI( info->pResult, 1, x );
         x_arraySetNI( info->pResult, 2, y );
         break;
      }

      case X_GTI_SYSTEMTHEME:
      {
         const wchar_t *key = L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize";
         DWORD          value, size = sizeof( value );
         LONG           status;

         status = RegGetValueW( HKEY_CURRENT_USER, key, L"AppsUseLightTheme", RRF_RT_REG_DWORD, NULL, &value, &size );
         if( status == ERROR_SUCCESS )
            info->pResult = x_itemPutC( info->pResult, IIF( value, "LIGHT", "DARK" ) );
         else
            info->pResult = x_itemPutC( info->pResult, "UNKNOWN" );
         break;
      }

      case X_GTI_OPENURL:
      {
         int opened = 0;

         if( info->pNewVal )
         {
            wchar_t *url = s_gt_wvt_ItemToWide( info->pNewVal );
            HRESULT  comInit = CoInitializeEx( NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE );

            opened = ( INT_PTR )ShellExecuteW( NULL, L"open", url, NULL, NULL, SW_SHOWNORMAL ) > 32;
            if( SUCCEEDED( comInit ) )
               CoUninitialize();
            x_xfree( url );
         }
         info->pResult = x_itemPutL( info->pResult, opened );
         break;
      }

      case X_GTI_FILEDIALOG:
      {
         /* options { cType, cTitle, cDirectory, aFilters, lMany }, filters { cName, cPattern } with
         patterns such as "png;jpg" or "*"; the result is { lOk, aFiles, nFilter, cError } */
         int                isArray = X_GTWVT_IS( info->pNewVal, X_IT_ARRAY );
         X_ITEM            *typeItem = IIF( isArray, x_arrayGetItemPtr( info->pNewVal, 1 ), NULL );
         X_ITEM            *title = IIF( isArray, x_arrayGetItemPtr( info->pNewVal, 2 ), NULL );
         X_ITEM            *dir = IIF( isArray, x_arrayGetItemPtr( info->pNewVal, 3 ), NULL );
         X_ITEM            *filters = IIF( isArray, x_arrayGetItemPtr( info->pNewVal, 4 ), NULL );
         X_ITEM            *many = IIF( isArray, x_arrayGetItemPtr( info->pNewVal, 5 ), NULL );
         int                isSave = x_stricmp( x_itemGetCPtr( typeItem ), "SAVE" ) == 0;
         int                isFolder = x_stricmp( x_itemGetCPtr( typeItem ), "FOLDER" ) == 0;
         const CLSID       *dialogClass = IIF( isSave, &CLSID_FileSaveDialog, &CLSID_FileOpenDialog );
         const IID         *dialogInterface = IIF( isSave, &IID_IFileSaveDialog, &IID_IFileOpenDialog );
         HRESULT            comInit = CoInitializeEx( NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE );
         IFileDialog       *dialog = NULL;
         COMDLG_FILTERSPEC *specs = NULL;
         X_ITEM            *files = x_itemArrayNew( 0 );
         X_ITEM             value = X_ITEM_NIL_INIT;
         X_ITEM             error = X_ITEM_NIL_INIT;
         int                count = 0, filter = -1, index;
         HRESULT            hr;
         int                failed;

         s_gt_wvt_CreateConsoleWindow( wvt );
         hr = CoCreateInstance( dialogClass, NULL, CLSCTX_INPROC_SERVER, dialogInterface, ( void ** )&dialog );
         if( SUCCEEDED( hr ) )
         {
            FILEOPENDIALOGOPTIONS options;

            dialog->lpVtbl->GetOptions( dialog, &options );
            options |= FOS_FORCEFILESYSTEM;
            if( isFolder )
               options |= FOS_PICKFOLDERS;
            else if( x_itemGetL( many ) && !isSave )
               options |= FOS_ALLOWMULTISELECT;
            dialog->lpVtbl->SetOptions( dialog, options );

            if( title )
            {
               wchar_t *text = s_gt_wvt_ItemToWide( title );

               dialog->lpVtbl->SetTitle( dialog, text );
               x_xfree( text );
            }

            /* the location is a folder to start in, or a file name */
            if( dir )
            {
               wchar_t    *path = s_gt_wvt_ItemToWide( dir );
               DWORD       attributes = GetFileAttributesW( path );
               IShellItem *folder;

               if( attributes != INVALID_FILE_ATTRIBUTES && ( attributes & FILE_ATTRIBUTE_DIRECTORY ) != 0 )
               {
                  if( SUCCEEDED( SHCreateItemFromParsingName( path, NULL, &IID_IShellItem, ( void ** )&folder ) ) )
                  {
                     dialog->lpVtbl->SetFolder( dialog, folder );
                     folder->lpVtbl->Release( folder );
                  }
               }
               else if( *path )
                  dialog->lpVtbl->SetFileName( dialog, path );
               x_xfree( path );
            }

            if( !isFolder && X_GTWVT_IS( filters, X_IT_ARRAY ) )
               count = ( int )x_arrayLen( filters );
            if( count )
            {
               specs = ( COMDLG_FILTERSPEC * )x_xgrab( ( size_t )count * sizeof( COMDLG_FILTERSPEC ) );
               for( index = 0; index < count; ++index )
               {
                  X_ITEM        *filterItem = x_arrayGetItemPtr( filters, ( size_t )index + 1 );
                  wchar_t       *pattern = s_gt_wvt_ItemToWide( x_arrayGetItemPtr( filterItem, 2 ) );
                  wchar_t       *spec = ( wchar_t * )x_xgrab( ( wcslen( pattern ) * 3 + 4 ) * sizeof( wchar_t ) );
                  const wchar_t *part = pattern;
                  size_t         length = 0;

                  /* "png;jpg" becomes "*.png;*.jpg" and "*" becomes "*.*" */
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
                     part += size;
                     if( *part )
                        ++part;
                  }
                  spec[length] = L'\0';
                  x_xfree( pattern );

                  specs[index].pszName = s_gt_wvt_ItemToWide( x_arrayGetItemPtr( filterItem, 1 ) );
                  specs[index].pszSpec = spec;
               }
               dialog->lpVtbl->SetFileTypes( dialog, ( UINT )count, specs );
               dialog->lpVtbl->SetFileTypeIndex( dialog, 1 );
            }

            hr = dialog->lpVtbl->Show( dialog, wvt->window );
            if( SUCCEEDED( hr ) )
            {
               IShellItemArray *items = NULL;
               IShellItem      *item = NULL;
               DWORD            total = 1, position;

               if( isSave )
                  hr = dialog->lpVtbl->GetResult( dialog, &item );
               else
               {
                  hr = ( ( IFileOpenDialog * )dialog )->lpVtbl->GetResults( ( IFileOpenDialog * )dialog, &items );
                  if( SUCCEEDED( hr ) )
                     items->lpVtbl->GetCount( items, &total );
               }

               for( position = 0; SUCCEEDED( hr ) && position < total; ++position )
               {
                  wchar_t *path;

                  if( items )
                     hr = items->lpVtbl->GetItemAt( items, position, &item );
                  if( SUCCEEDED( hr ) )
                     hr = item->lpVtbl->GetDisplayName( item, SIGDN_FILESYSPATH, &path );
                  if( SUCCEEDED( hr ) )
                  {
                     x_arrayAdd( files, s_gt_wvt_PutWide( &value, path, ( int )wcslen( path ) ) );
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

            /* as in GTSDL3 the chosen filter is reported also for a cancelled dialog */
            if( count )
            {
               UINT typeIndex;

               dialog->lpVtbl->GetFileTypeIndex( dialog, &typeIndex );
               filter = ( int )typeIndex - 1;
            }
            dialog->lpVtbl->Release( dialog );

            for( index = 0; index < count; ++index )
            {
               x_xfree( ( void * )specs[index].pszName );
               x_xfree( ( void * )specs[index].pszSpec );
            }
            x_xfree( specs );
         }
         if( SUCCEEDED( comInit ) )
            CoUninitialize();

         /* cancelling the dialog is not an error */
         failed = FAILED( hr ) && hr != HRESULT_FROM_WIN32( ERROR_CANCELLED );
         if( failed )
         {
            wchar_t *text = NULL;
            DWORD flags = FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS;
            DWORD length = FormatMessageW( flags, NULL, ( DWORD )hr, 0, ( LPWSTR )&text, 0, NULL );

            while( length > 0 && ( text[length - 1] == L'\r' || text[length - 1] == L'\n' ) )
               --length;
            s_gt_wvt_PutWide( &error, text, ( int )length );
            LocalFree( text );
         }
         else
            x_itemPutStrUTF8( &error, NULL );

         if( info->pResult == NULL )
            info->pResult = x_itemArrayNew( 4 );
         else
            x_arrayNew( info->pResult, 4 );
         x_itemPutL( &value, !failed && x_arrayLen( files ) > 0 );
         x_arraySet( info->pResult, 1, &value );
         x_arraySet( info->pResult, 2, files );
         x_itemPutNI( &value, filter + 1 );
         x_arraySet( info->pResult, 3, &value );
         x_arraySet( info->pResult, 4, &error );
         x_itemRelease( files );
         x_itemClear( &value );
         x_itemClear( &error );
         break;
      }

      case X_GTI_WINDOWOPACITY:
         info->pResult = x_itemPutND( info->pResult, wvt->opacity );
         if( info->pNewVal )
         {
            float    opacity = ( float )x_itemGetND( info->pNewVal );
            LONG_PTR exStyle;

            s_gt_wvt_CreateConsoleWindow( wvt );
            exStyle = GetWindowLongPtrW( wvt->window, GWL_EXSTYLE );
            if( opacity < 0.0f )
               opacity = 0.0f;
            if( opacity > 1.0f )
               opacity = 1.0f;
            wvt->opacity = opacity;
            if( wvt->opacity == 1.0f )
               SetWindowLongPtrW( wvt->window, GWL_EXSTYLE, exStyle & ~WS_EX_LAYERED );
            else
            {
               SetWindowLongPtrW( wvt->window, GWL_EXSTYLE, exStyle | WS_EX_LAYERED );
               SetLayeredWindowAttributes( wvt->window, 0, ( BYTE )( int )( wvt->opacity * 255.0f ), LWA_ALPHA );
            }
         }
         break;

      case X_GTI_WINDOWALWAYSONTOP:
      {
         int onTop = wvt->window && ( GetWindowLongPtrW( wvt->window, GWL_EXSTYLE ) & WS_EX_TOPMOST ) != 0;

         info->pResult = x_itemPutL( info->pResult, onTop );
         if( info->pNewVal && x_itemGetL( info->pNewVal ) != onTop )
         {
            HWND insertAfter = IIF( onTop, HWND_NOTOPMOST, HWND_TOPMOST );

            s_gt_wvt_CreateConsoleWindow( wvt );
            SetWindowPos( wvt->window, insertAfter, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE );
         }
         break;
      }

      case X_GTI_WINDOWBORDERED:
      {
         int bordered = !wvt->window || ( GetWindowLongPtrW( wvt->window, GWL_STYLE ) & WS_CAPTION ) != 0;

         info->pResult = x_itemPutL( info->pResult, bordered );
         if( info->pNewVal && x_itemGetL( info->pNewVal ) != bordered )
         {
            LONG_PTR style;
            POINT    origin = { 0, 0 };
            RECT     frame;
            UINT     flags = SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED;
            int      width, height;

            s_gt_wvt_CreateConsoleWindow( wvt );
            style = GetWindowLongPtrW( wvt->window, GWL_STYLE );
            if( bordered )
               style &= ~( WS_CAPTION | WS_THICKFRAME | WS_SYSMENU | WS_MINIMIZEBOX | WS_MAXIMIZEBOX );
            else
               style |= IIF( wvt->resizable, _WVT_WS_DEF, _WVT_WS_NORESIZE );

            /* the client area keeps its place and size, so the canvas is kept */
            GetClientRect( wvt->window, &frame );
            ClientToScreen( wvt->window, &origin );
            OffsetRect( &frame, origin.x, origin.y );
            SetWindowLongPtrW( wvt->window, GWL_STYLE, style );
            AdjustWindowRectEx( &frame, ( DWORD )style, FALSE, ( DWORD )GetWindowLongPtrW( wvt->window, GWL_EXSTYLE ) );
            width = frame.right - frame.left;
            height = frame.bottom - frame.top;
            SetWindowPos( wvt->window, NULL, frame.left, frame.top, width, height, flags );
         }
         break;
      }

      case X_GTI_RESIZESTEP:
         info->pResult = x_itemPutL( info->pResult, wvt->sizeStep );
         if( info->pNewVal )
         {
            int sizeStep = x_itemGetL( info->pNewVal );

            if( sizeStep != wvt->sizeStep )
            {
               wvt->sizeStep = sizeStep;

               /* as in GTSDL3 the window follows whole cells again at once */
               if( sizeStep && wvt->window && !wvt->maximized && !wvt->fullScreen )
               {
                  RECT  frame = { 0, 0, wvt->cellSize.x * wvt->cols, wvt->cellSize.y * wvt->rows };
                  DWORD style = ( DWORD )GetWindowLongPtrW( wvt->window, GWL_STYLE );
                  DWORD exStyle = ( DWORD )GetWindowLongPtrW( wvt->window, GWL_EXSTYLE );
                  UINT  flags = SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE;

                  AdjustWindowRectEx( &frame, style, FALSE, exStyle );
                  SetWindowPos( wvt->window, NULL, 0, 0, frame.right - frame.left, frame.bottom - frame.top, flags );
               }
            }
         }
         break;

      case X_GTI_MSGBOX:
         if( !info->pNewVal || X_IS_LOGICAL( info->pNewVal ) || X_IS_NIL( info->pNewVal ) )
         {
            info->pResult = x_itemPutL( info->pResult, wvt->msgAlert );
            if( info->pNewVal && X_IS_LOGICAL( info->pNewVal ) )
               wvt->msgAlert = x_itemGetL( info->pNewVal );
         }
         else
            info->pResult = x_itemPutNI( info->pResult, s_gt_wvt_MessageBox( wvt, info->pNewVal, info->pNewVal2 ) );
         break;

      case X_GTI_DISPIMAGE:
      {
         /* the image is a BMP or PNG file name, which GTSDL3 reads with SDL_LoadSurface(), or raw
         pixels { data, nWidth, nHeight, [nDepth] } with 1, 16, 24 or 32 bits; an empty file name
         restores the text in the area. The area is { nLeft, nTop } for the natural size,
         { nLeft, nTop, nRight, nBottom } to stretch, .T. to fit centred, or NIL for the screen */
         X_ITEM   *image = info->pNewVal;
         X_ITEM   *areaItem = info->pNewVal2;
         int       restore = X_GTWVT_IS( image, X_IT_STRING ) && x_itemGetCLen( image ) == 0;
         int       result = 0;
         uint32_t *source = NULL; /* 0xAARRGGBB with straight alpha */
         int       srcW = 0, srcH = 0, scrW, scrH;
         double    destX = 0.0, destY = 0.0, destW, destH;

         s_gt_wvt_BeginGfx( gt, wvt );
         scrW = s_gt_wvt_CanvasToLogical( wvt, wvt->canvasWidth, 0 );
         scrH = s_gt_wvt_CanvasToLogical( wvt, wvt->canvasHeight, 1 );
         destW = scrW;
         destH = scrH;

         if( X_GTWVT_IS( image, X_IT_STRING ) && !restore )
         {
            HRESULT                comInit = CoInitializeEx( NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE );
            wchar_t               *fileName = s_gt_wvt_ItemToWide( image );
            IWICImagingFactory    *factory = NULL;
            IWICBitmapDecoder     *decoder = NULL;
            IWICBitmapFrameDecode *frame = NULL;
            IWICFormatConverter   *converter = NULL;
            IWICBitmapSource      *bitmap;
            const GUID            *format = &GUID_WICPixelFormat32bppBGRA;
            DWORD                  context = CLSCTX_INPROC_SERVER;
            DWORD                  access = GENERIC_READ;
            WICDecodeOptions       options = WICDecodeMetadataCacheOnDemand;
            WICBitmapDitherType    dither = WICBitmapDitherTypeNone;
            WICBitmapPaletteType   palette = WICBitmapPaletteTypeCustom;
            void                  *object = NULL;
            GUID                   container;
            UINT                   width = 0, height = 0;
            int                    supported = 0;
            HRESULT                hr;

            hr = CoCreateInstance( &CLSID_WICImagingFactory, NULL, context, &IID_IWICImagingFactory, &object );
            factory = ( IWICImagingFactory * )object;
            if( SUCCEEDED( hr ) )
               hr = factory->lpVtbl->CreateDecoderFromFilename( factory, fileName, NULL, access, options, &decoder );
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
               bitmap = ( IWICBitmapSource * )frame;
               hr = converter->lpVtbl->Initialize( converter, bitmap, format, dither, NULL, 0.0, palette );
            }
            if( SUCCEEDED( hr ) )
               hr = converter->lpVtbl->GetSize( converter, &width, &height );
            if( SUCCEEDED( hr ) && ( width == 0 || height == 0 || width > INT_MAX || height > INT_MAX ) )
               hr = E_FAIL;
            if( SUCCEEDED( hr ) && ( uint64_t )width * height * 4 > UINT_MAX )
               hr = E_FAIL;
            if( SUCCEEDED( hr ) )
            {
               source = ( uint32_t * )x_xgrab( ( size_t )width * height * 4 );
               hr = converter->lpVtbl->CopyPixels( converter, NULL, width * 4, width * height * 4, ( BYTE * )source );
               if( SUCCEEDED( hr ) )
               {
                  srcW = ( int )width;
                  srcH = ( int )height;
               }
               else
               {
                  x_xfree( source );
                  source = NULL;
               }
            }
            if( converter )
               converter->lpVtbl->Release( converter );
            if( frame )
               frame->lpVtbl->Release( frame );
            if( decoder )
               decoder->lpVtbl->Release( decoder );
            if( factory )
               factory->lpVtbl->Release( factory );
            if( SUCCEEDED( comInit ) )
               CoUninitialize();
            x_xfree( fileName );
         }
         else if( X_GTWVT_IS( image, X_IT_ARRAY ) && x_arrayLen( image ) >= 3 )
         {
            X_ITEM        *data = x_arrayGetItemPtr( image, 1 );
            int            width = x_arrayGetNI( image, 2 );
            int            height = x_arrayGetNI( image, 3 );
            int            depth = IIF( x_arrayLen( image ) == 4, x_arrayGetNI( image, 4 ), 32 );
            int            tight = 0, pitch = 0, x, y;
            int            accepted;
            const uint8_t *pixels = NULL;

            if( depth == 0 )
               depth = 32;
            accepted = width > 0 && height > 0 && ( depth == 1 || depth == 16 || depth == 24 || depth == 32 );
            if( accepted && ( ( int64_t )width * depth > INT_MAX - 7 || ( uint64_t )width * height > SIZE_MAX / 4 ) )
               accepted = 0;
            if( accepted )
            {
               /* rows are aligned to 4 bytes, unless a string holds exactly the tight rows */
               tight = ( width * depth + 7 ) / 8;
               pitch = ( tight + 3 ) & ~3;
               if( X_GTWVT_IS( data, X_IT_STRING ) )
               {
                  if( x_itemGetCLen( data ) == ( size_t )height * tight )
                     pitch = tight;
                  if( x_itemGetCLen( data ) >= ( size_t )height * pitch )
                     pixels = ( const uint8_t * )x_itemGetCPtr( data );
               }
               else
                  pixels = ( const uint8_t * )x_itemGetPtr( data );
            }

            if( pixels )
            {
               srcW = width;
               srcH = height;
               source = ( uint32_t * )x_xgrab( ( size_t )width * height * 4 );
               for( y = 0; y < height; ++y )
               {
                  const uint8_t *row = pixels + ( size_t )y * pitch;
                  uint32_t      *target = source + ( size_t )y * width;

                  for( x = 0; x < width; ++x )
                  {
                     switch( depth )
                     {
                        case 1: target[x] = s_monoColors[( row[x >> 3] >> ( 7 - ( x & 7 ) ) ) & 1]; break;
                        case 16:
                        {
                           /* RGB565, each channel widened by repeating its high bits */
                           unsigned value = row[x * 2] | ( unsigned )row[x * 2 + 1] << 8;
                           unsigned red = value >> 11, green = ( value >> 5 ) & 0x3F, blue = value & 0x1F;

                           red = red << 3 | red >> 2;
                           green = green << 2 | green >> 4;
                           blue = blue << 3 | blue >> 2;
                           target[x] = 0xFF000000u | red << 16 | green << 8 | blue;
                           break;
                        }
                        case 24: /* bytes R, G, B */
                        {
                           const uint8_t *pixel = row + x * 3;

                           target[x] = 0xFF000000u | ( uint32_t )pixel[0] << 16 | ( uint32_t )pixel[1] << 8 | pixel[2];
                           break;
                        }
                        default: /* XRGB8888: bytes B, G, R, unused */
                        {
                           const uint8_t *pixel = row + x * 4;

                           target[x] = 0xFF000000u | ( uint32_t )pixel[2] << 16 | ( uint32_t )pixel[1] << 8 | pixel[0];
                           break;
                        }
                     }
                  }
               }
            }
         }

         if( restore || source )
         {
            int valid = 1;

            if( X_GTWVT_IS( areaItem, X_IT_ARRAY ) )
            {
               int     left = x_arrayGetNI( areaItem, 1 );
               int     top = x_arrayGetNI( areaItem, 2 );
               int64_t width, height;

               destX = left;
               destY = top;
               if( x_arrayLen( areaItem ) == 2 )
               {
                  width = IIF( source, srcW, ( int64_t )scrW - left );
                  height = IIF( source, srcH, ( int64_t )scrH - top );
               }
               else
               {
                  width = ( int64_t )x_arrayGetNI( areaItem, 3 ) - left + 1;
                  height = ( int64_t )x_arrayGetNI( areaItem, 4 ) - top + 1;
               }
               valid = width > 0 && height > 0 && width <= INT_MAX && height <= INT_MAX;
               destW = ( double )width;
               destH = ( double )height;
            }
            else if( source && x_itemGetL( areaItem ) )
            {
               double fitX = ( double )scrW / srcW;
               double fitY = ( double )scrH / srcH;
               double scale = X_MIN( fitX, fitY );
               int    width = X_MAX( ( int )floor( srcW * scale + 0.5 ), 1 );
               int    height = X_MAX( ( int )floor( srcH * scale + 0.5 ), 1 );

               destX = ( scrW - width ) / 2;
               destY = ( scrH - height ) / 2;
               destW = width;
               destH = height;
            }

            if( valid && destX < scrW && destY < scrH && destX + destW > 0.0 && destY + destH > 0.0 )
            {
               /* the logical area in window pixels */
               double scaleX = ( double )wvt->cellSize.x / s_gt_wvt_CellLogical( wvt, 0 );
               double scaleY = ( double )wvt->cellSize.y / s_gt_wvt_CellLogical( wvt, 1 );
               int    left = ( int )floor( X_MAX( destX, 0.0 ) * scaleX );
               int    top = ( int )floor( X_MAX( destY, 0.0 ) * scaleY );
               double areaRight = X_MIN( destX + destW, ( double )scrW );
               double areaBottom = X_MIN( destY + destH, ( double )scrH );
               int    right = X_MIN( ( int )ceil( areaRight * scaleX ), wvt->canvasWidth ) - 1;
               int    bottom = X_MIN( ( int )ceil( areaBottom * scaleY ), wvt->canvasHeight ) - 1;
               int    x, y;

               if( restore )
               {
                  int rowTop = top / wvt->cellSize.y;
                  int colLeft = left / wvt->cellSize.x;
                  int rowBottom = bottom / wvt->cellSize.y;
                  int colRight = right / wvt->cellSize.x;

                  X_GTSELF_EXPOSEAREA( gt, rowTop, colLeft, rowBottom, colRight );
                  s_super.Refresh( gt );
               }
               else
               {
                  /* linear filtering and alpha blending, as the SDL renderer draws the image
                  texture; a pixel belongs to the image when its centre is inside the area */
                  for( y = top; y <= bottom; ++y )
                  {
                     double v = ( ( y + 0.5 ) / scaleY - destY ) / destH * srcH - 0.5;
                     int    v0 = ( int )floor( v );
                     double fy = v - v0;
                     int    r0 = X_MAX( 0, X_MIN( v0, srcH - 1 ) ), r1 = X_MAX( 0, X_MIN( v0 + 1, srcH - 1 ) );
                     uint32_t *dst = wvt->pixels + ( size_t )y * wvt->canvasWidth;

                     if( ( y + 0.5 ) / scaleY < destY || ( y + 0.5 ) / scaleY >= destY + destH )
                        continue;
                     for( x = left; x <= right; ++x )
                     {
                        double u = ( ( x + 0.5 ) / scaleX - destX ) / destW * srcW - 0.5;
                        int    u0 = ( int )floor( u );
                        double fx = u - u0;
                        int c0 = X_MAX( 0, X_MIN( u0, srcW - 1 ) ), c1 = X_MAX( 0, X_MIN( u0 + 1, srcW - 1 ) );
                        uint32_t p00 = source[( size_t )r0 * srcW + c0], p01 = source[( size_t )r0 * srcW + c1];
                        uint32_t p10 = source[( size_t )r1 * srcW + c0], p11 = source[( size_t )r1 * srcW + c1];
                        double   channel[4], alpha;
                        uint32_t red, green, blue;
                        int      shift;

                        if( ( x + 0.5 ) / scaleX < destX || ( x + 0.5 ) / scaleX >= destX + destW )
                           continue;
                        for( shift = 0; shift < 4; ++shift )
                        {
                           int    bits = shift * 8;
                           double upper = ( p00 >> bits & 0xFF ) * ( 1.0 - fx ) + ( p01 >> bits & 0xFF ) * fx;
                           double lower = ( p10 >> bits & 0xFF ) * ( 1.0 - fx ) + ( p11 >> bits & 0xFF ) * fx;

                           channel[shift] = upper * ( 1.0 - fy ) + lower * fy;
                        }
                        alpha = channel[3] / 255.0;
                        red = ( uint32_t )( channel[2] * alpha + ( dst[x] >> 16 & 0xFF ) * ( 1.0 - alpha ) + 0.5 );
                        green = ( uint32_t )( channel[1] * alpha + ( dst[x] >> 8 & 0xFF ) * ( 1.0 - alpha ) + 0.5 );
                        blue = ( uint32_t )( channel[0] * alpha + ( dst[x] & 0xFF ) * ( 1.0 - alpha ) + 0.5 );
                        dst[x] = red << 16 | green << 8 | blue;
                     }
                  }
                  s_gt_wvt_InvalidateCanvas( wvt );
               }
               result = 1;
            }
         }

         x_xfree( source );
         info->pResult = x_itemPutL( info->pResult, result );
         break;
      }

      default: return s_super.Info( gt, type, info );
   }

   return 1;
}

/* ---------------------------------------------------------------------------------------------
   Graphics on the canvas, the same model and the same pixels as GTSDL3

   GfxPrimitive() parameters may have different meanings: the color is bottom for PUTPIXEL
   and FLOODFILL and right for CIRCLE; red is top, green left and blue bottom for MAKECOLOR.
   Colors are 0xRRGGBB, which is also the canvas pixel format.
   --------------------------------------------------------------------------------------------- */

/* The clip in logical pixels, inside the canvas. */
static int s_gt_wvt_GetGfxClip( X_GTWVT *wvt, RECT *clip )
{
   int width = s_gt_wvt_CanvasToLogical( wvt, wvt->canvasWidth, 0 );
   int height = s_gt_wvt_CanvasToLogical( wvt, wvt->canvasHeight, 1 );

   if( wvt->clipOn )
   {
      clip->left = X_MAX( wvt->clipLeft, 0 );
      clip->top = X_MAX( wvt->clipTop, 0 );
      clip->right = X_MIN( wvt->clipRight, width - 1 );
      clip->bottom = X_MIN( wvt->clipBottom, height - 1 );
   }
   else
   {
      clip->left = clip->top = 0;
      clip->right = width - 1;
      clip->bottom = height - 1;
   }

   return clip->left <= clip->right && clip->top <= clip->bottom;
}

/* Fills the window pixels of the logical rectangle left..right, top..bottom (inclusive), as
   GTSDL3 stretches its logical canvas to the window. */
static void s_gt_wvt_FillLogical( X_GTWVT *wvt, int64_t left, int64_t top, int64_t right, int64_t bottom,
                                  uint32_t color )
{
   int x0 = X_MAX( s_gt_wvt_CanvasToPixel( wvt, ( double )left, 0 ), 0 );
   int y0 = X_MAX( s_gt_wvt_CanvasToPixel( wvt, ( double )top, 1 ), 0 );
   int x1 = X_MIN( s_gt_wvt_CanvasToPixel( wvt, ( double )right + 1, 0 ), wvt->canvasWidth );
   int y1 = X_MIN( s_gt_wvt_CanvasToPixel( wvt, ( double )bottom + 1, 1 ), wvt->canvasHeight );
   int x, y;

   for( y = y0; y < y1; ++y )
   {
      uint32_t *row = wvt->pixels + ( size_t )y * wvt->canvasWidth;

      for( x = x0; x < x1; ++x )
         row[x] = color;
   }
}

static void s_gt_wvt_GfxPlot( X_GTWVT *wvt, const RECT *clip, int64_t x, int64_t y, uint32_t color )
{
   if( x >= clip->left && x <= clip->right && y >= clip->top && y <= clip->bottom )
      s_gt_wvt_FillLogical( wvt, x, y, x, y, color );
}

static int s_gt_wvt_OutCode( const RECT *clip, int64_t x, int64_t y )
{
   int code = 0;

   if( x < clip->left )
      code |= 1;
   if( x > clip->right )
      code |= 2;
   if( y < clip->top )
      code |= 4;
   if( y > clip->bottom )
      code |= 8;

   return code;
}

/* Cohen-Sutherland clipping of the end points, then Bresenham, as SDL_GetRectAndLineIntersection() in GTSDL3. */
static void s_gt_wvt_GfxLine( X_GTWVT *wvt, const RECT *clip, int64_t top, int64_t left, int64_t bottom, int64_t right,
                              uint32_t color )
{
   int     code1 = s_gt_wvt_OutCode( clip, left, top ), code2 = s_gt_wvt_OutCode( clip, right, bottom );
   int64_t dx, dy, error;
   int     sx, sy;

   while( code1 | code2 )
   {
      int     code;
      int64_t x = 0, y = 0;

      if( code1 & code2 )
         return;

      code = IIF( code1, code1, code2 );
      if( code & 4 )
      {
         y = clip->top;
         x = left + ( right - left ) * ( y - top ) / ( bottom - top );
      }
      else if( code & 8 )
      {
         y = clip->bottom;
         x = left + ( right - left ) * ( y - top ) / ( bottom - top );
      }
      else if( code & 1 )
      {
         x = clip->left;
         y = top + ( bottom - top ) * ( x - left ) / ( right - left );
      }
      else
      {
         x = clip->right;
         y = top + ( bottom - top ) * ( x - left ) / ( right - left );
      }

      if( code == code1 )
      {
         left = x;
         top = y;
         code1 = s_gt_wvt_OutCode( clip, left, top );
      }
      else
      {
         right = x;
         bottom = y;
         code2 = s_gt_wvt_OutCode( clip, right, bottom );
      }
   }

   dx = IIF( left < right, right - left, left - right );
   dy = IIF( top < bottom, bottom - top, top - bottom );
   sx = IIF( left < right, 1, -1 );
   sy = IIF( top < bottom, 1, -1 );
   error = dx - dy;

   for( ;; )
   {
      int64_t twice;

      s_gt_wvt_GfxPlot( wvt, clip, left, top, color );

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

static void s_gt_wvt_GfxEllipse( X_GTWVT *wvt, const RECT *clip, int centerY, int centerX, int radiusY, int radiusX,
                                 int filled, uint32_t color )
{
   int64_t start, end, position;

   if( radiusY < 0 || radiusX < 0 )
      return;

   if( radiusY == 0 )
   {
      int64_t left = ( int64_t )centerX - radiusX, right = ( int64_t )centerX + radiusX;

      s_gt_wvt_GfxLine( wvt, clip, centerY, left, centerY, right, color );
      return;
   }

   if( radiusX == 0 )
   {
      int64_t top = ( int64_t )centerY - radiusY, bottom = ( int64_t )centerY + radiusY;

      s_gt_wvt_GfxLine( wvt, clip, top, centerX, bottom, centerX, color );
      return;
   }

   start = X_MAX( ( int64_t )clip->top, ( int64_t )centerY - radiusY );
   end = X_MIN( ( int64_t )clip->bottom, ( int64_t )centerY + radiusY );

   for( position = start; position <= end; ++position )
   {
      double  ratio = ( ( double )position - centerY ) / radiusY;
      double  value = 1.0 - ratio * ratio;
      int64_t offset = ( int64_t )floor( radiusX * sqrt( X_MAX( value, 0.0 ) ) + 0.5 );
      int64_t left = ( int64_t )centerX - offset;
      int64_t right = ( int64_t )centerX + offset;

      if( filled )
         s_gt_wvt_GfxLine( wvt, clip, position, left, position, right, color );
      else
      {
         s_gt_wvt_GfxPlot( wvt, clip, left, position, color );
         s_gt_wvt_GfxPlot( wvt, clip, right, position, color );
      }
   }

   if( !filled )
   {
      start = X_MAX( ( int64_t )clip->left, ( int64_t )centerX - radiusX );
      end = X_MIN( ( int64_t )clip->right, ( int64_t )centerX + radiusX );

      for( position = start; position <= end; ++position )
      {
         double  ratio = ( ( double )position - centerX ) / radiusX;
         double  value = 1.0 - ratio * ratio;
         int64_t offset = ( int64_t )floor( radiusY * sqrt( X_MAX( value, 0.0 ) ) + 0.5 );

         s_gt_wvt_GfxPlot( wvt, clip, position, ( int64_t )centerY - offset, color );
         s_gt_wvt_GfxPlot( wvt, clip, position, ( int64_t )centerY + offset, color );
      }
   }
}

/* Graphics need the window and the current text on the canvas. */
static void s_gt_wvt_BeginGfx( X_GT_BASE *gt, X_GTWVT *wvt )
{
   s_gt_wvt_CreateConsoleWindow( wvt );
   s_super.Refresh( gt );
   GdiFlush();
}

static int s_gt_wvt_gfx_Primitive( X_GT_BASE *gt, int type, int top, int left, int bottom, int right, int color )
{
   X_GTWVT *wvt = X_GTWVT_GET( gt );
   int      result = 0;

   switch( type )
   {
      case GFX_ACQUIRESCREEN:
         s_super.Refresh( gt );
         ++wvt->gfxDepth;
         X_GTSELF_DISPBEGIN( gt );
         result = 1;
         break;
      case GFX_RELEASESCREEN:
         if( wvt->gfxDepth > 0 )
            --wvt->gfxDepth;
         result = 1;
         X_GTSELF_DISPEND( gt );
         break;

      case GFX_MAKECOLOR:
      {
         int red = X_MIN( X_MAX( top, 0 ), 255 );
         int green = X_MIN( X_MAX( left, 0 ), 255 );
         int blue = X_MIN( X_MAX( bottom, 0 ), 255 );

         result = ( red << 16 ) | ( green << 8 ) | blue;
         break;
      }

      case GFX_CLIPTOP: result = wvt->clipTop; break;
      case GFX_CLIPLEFT: result = wvt->clipLeft; break;
      case GFX_CLIPBOTTOM: result = wvt->clipBottom; break;
      case GFX_CLIPRIGHT: result = wvt->clipRight; break;
      case GFX_SETCLIP:
         /* inclusive top, left, bottom and right coordinates; all zeros remove the clip */
         if( top == 0 && left == 0 && bottom == 0 && right == 0 )
         {
            wvt->clipTop = 0;
            wvt->clipLeft = 0;
            wvt->clipBottom = 0;
            wvt->clipRight = 0;
            wvt->clipOn = 0;
         }
         else
         {
            wvt->clipTop = X_MIN( top, bottom );
            wvt->clipLeft = X_MIN( left, right );
            wvt->clipBottom = X_MAX( top, bottom );
            wvt->clipRight = X_MAX( left, right );
            wvt->clipOn = 1;
         }
         result = 1;
         break;

      /* the portable SOLID drawing mode, as in GTSDL3 */
      case GFX_DRAWINGMODE: result = GFX_MODE_SOLID; break;

      case GFX_GETPIXEL:
      {
         int screenHeight, screenWidth;

         s_gt_wvt_BeginGfx( gt, wvt );
         screenHeight = s_gt_wvt_CanvasToLogical( wvt, wvt->canvasHeight, 1 );
         screenWidth = s_gt_wvt_CanvasToLogical( wvt, wvt->canvasWidth, 0 );
         if( top >= 0 && left >= 0 && top < screenHeight && left < screenWidth )
         {
            /* the first window pixel of the logical pixel */
            int y = X_MIN( s_gt_wvt_CanvasToPixel( wvt, top, 1 ), wvt->canvasHeight - 1 );
            int x = X_MIN( s_gt_wvt_CanvasToPixel( wvt, left, 0 ), wvt->canvasWidth - 1 );

            result = ( int )( wvt->pixels[( size_t )y * wvt->canvasWidth + x] & 0xFFFFFF );
         }
         break;
      }

      case GFX_PUTPIXEL:
      case GFX_LINE:
      case GFX_RECT:
      case GFX_FILLEDRECT:
      case GFX_CIRCLE:
      case GFX_FILLEDCIRCLE:
      case GFX_ELLIPSE:
      case GFX_FILLEDELLIPSE:
      case GFX_FLOODFILL:
      {
         RECT     clip;
         uint32_t rgb;

         if( type == GFX_PUTPIXEL || type == GFX_FLOODFILL )
            color = bottom;
         else if( type == GFX_CIRCLE || type == GFX_FILLEDCIRCLE )
            color = right;
         rgb = ( uint32_t )color & 0xFFFFFF;

         s_gt_wvt_BeginGfx( gt, wvt );

         if( s_gt_wvt_GetGfxClip( wvt, &clip ) )
         {
            switch( type )
            {
               case GFX_PUTPIXEL: s_gt_wvt_GfxPlot( wvt, &clip, left, top, rgb ); break;
               case GFX_LINE: s_gt_wvt_GfxLine( wvt, &clip, top, left, bottom, right, rgb ); break;
               case GFX_RECT:
                  s_gt_wvt_GfxLine( wvt, &clip, top, left, top, right, rgb );
                  s_gt_wvt_GfxLine( wvt, &clip, bottom, left, bottom, right, rgb );
                  s_gt_wvt_GfxLine( wvt, &clip, top, left, bottom, left, rgb );
                  s_gt_wvt_GfxLine( wvt, &clip, top, right, bottom, right, rgb );
                  break;
               case GFX_FILLEDRECT:
               {
                  int boxTop = X_MAX( X_MIN( top, bottom ), ( int )clip.top );
                  int boxLeft = X_MAX( X_MIN( left, right ), ( int )clip.left );
                  int boxBottom = X_MIN( X_MAX( top, bottom ), ( int )clip.bottom );
                  int boxRight = X_MIN( X_MAX( left, right ), ( int )clip.right );

                  if( boxTop <= boxBottom && boxLeft <= boxRight )
                     s_gt_wvt_FillLogical( wvt, boxLeft, boxTop, boxRight, boxBottom, rgb );
                  break;
               }
               case GFX_CIRCLE: s_gt_wvt_GfxEllipse( wvt, &clip, top, left, bottom, bottom, 0, rgb ); break;
               case GFX_FILLEDCIRCLE: s_gt_wvt_GfxEllipse( wvt, &clip, top, left, bottom, bottom, 1, rgb ); break;
               case GFX_ELLIPSE: s_gt_wvt_GfxEllipse( wvt, &clip, top, left, bottom, right, 0, rgb ); break;
               case GFX_FILLEDELLIPSE: s_gt_wvt_GfxEllipse( wvt, &clip, top, left, bottom, right, 1, rgb ); break;
               case GFX_FLOODFILL:
                  /* the 4-connected area of the start pixel color inside the clip gets the new color;
               it is filled in window pixels, inside the window pixels of the logical clip */
                  if( left >= clip.left && left <= clip.right && top >= clip.top && top <= clip.bottom )
                  {
                     uint32_t *pixels = wvt->pixels;
                     int       width = wvt->canvasWidth;
                     int       startX = s_gt_wvt_CanvasToPixel( wvt, left, 0 );
                     int       startY = s_gt_wvt_CanvasToPixel( wvt, top, 1 );
                     uint32_t  srcColor = pixels[( size_t )startY * width + startX];

                     clip.left = s_gt_wvt_CanvasToPixel( wvt, clip.left, 0 );
                     clip.top = s_gt_wvt_CanvasToPixel( wvt, clip.top, 1 );
                     clip.right = X_MIN( s_gt_wvt_CanvasToPixel( wvt, clip.right + 1, 0 ), wvt->canvasWidth ) - 1;
                     clip.bottom = X_MIN( s_gt_wvt_CanvasToPixel( wvt, clip.bottom + 1, 1 ), wvt->canvasHeight ) - 1;
                     if( srcColor != rgb )
                     {
                        size_t  used = 0;
                        size_t  count = ( size_t )width * wvt->canvasHeight;
                        size_t *stack = ( size_t * )x_xgrabArray( count, sizeof( size_t ) );

                        pixels[( size_t )startY * width + startX] = rgb;
                        stack[used++] = ( size_t )startY * width + startX;

                        while( used > 0 )
                        {
                           size_t index = stack[--used];
                           int    x = ( int )( index % width );
                           int    y = ( int )( index / width );
                           int    k;

                           for( k = 0; k < 4; ++k )
                           {
                              int nx = x + s_floodDX[k];
                              int ny = y + s_floodDY[k];

                              if( nx >= clip.left && nx <= clip.right && ny >= clip.top && ny <= clip.bottom )
                              {
                                 size_t next = ( size_t )ny * width + nx;

                                 if( pixels[next] == srcColor )
                                 {
                                    pixels[next] = rgb;
                                    stack[used++] = next;
                                 }
                              }
                           }
                        }

                        x_xfree( stack );
                     }
                  }
                  break;
            }
            s_gt_wvt_InvalidateCanvas( wvt );
         }

         result = 1;
         break;
      }

      default: return s_super.GfxPrimitive( gt, type, top, left, bottom, right, color );
   }

   return result;
}

/* Text in pixels with a transparent background; the cell font face scaled to size x width. */
static void s_gt_wvt_gfx_Text( X_GT_BASE *gt, int top, int left, const char *text, int color, int size, int charWidth )
{
   X_GTWVT *wvt = X_GTWVT_GET( gt );
   int      height, width, length, count, index, saved, screenWidth, screenHeight;
   int      textX, textY;
   wchar_t *wideText;
   int     *advances;
   HFONT    font, oldFont;
   RECT     clip, area;

   s_gt_wvt_BeginGfx( gt, wvt );

   /* sizes and the position are logical pixels; the text is drawn with a font of the window */
   screenWidth = s_gt_wvt_CanvasToLogical( wvt, wvt->canvasWidth, 0 );
   screenHeight = s_gt_wvt_CanvasToLogical( wvt, wvt->canvasHeight, 1 );
   height = IIF( size == 0, s_gt_wvt_CellLogical( wvt, 1 ), X_MIN( size, screenHeight ) );
   width = IIF( charWidth == 0, X_MAX( height / 2, 1 ), X_MIN( charWidth, screenWidth ) );

   if( ( int64_t )top + height <= 0 || top >= screenHeight || !s_gt_wvt_GetGfxClip( wvt, &clip ) )
      return;

   height = IIF( size == 0, wvt->cellSize.y, X_MAX( s_gt_wvt_CanvasToPixel( wvt, height, 1 ), 1 ) );
   width = X_MAX( s_gt_wvt_CanvasToPixel( wvt, width, 0 ), 1 );

   wideText = s_gt_wvt_ToWide( text, strlen( text ), &length );
   for( count = 0; count < length && wideText[count] != L'\r' && wideText[count] != L'\n'; ++count )
      ;

   advances = ( int * )x_xgrabArray( ( size_t )count + 1, sizeof( int ) );
   for( index = 0; index < count; ++index )
      advances[index] = width;

   font = s_gt_wvt_GetFont( wvt->fontFace, height, width, wvt->fontWeight, wvt->fontQuality );
   saved = SaveDC( wvt->canvasDc );
   oldFont = ( HFONT )SelectObject( wvt->canvasDc, font );
   area.left = s_gt_wvt_CanvasToPixel( wvt, clip.left, 0 );
   area.top = s_gt_wvt_CanvasToPixel( wvt, clip.top, 1 );
   area.right = s_gt_wvt_CanvasToPixel( wvt, clip.right + 1, 0 );
   area.bottom = s_gt_wvt_CanvasToPixel( wvt, clip.bottom + 1, 1 );
   IntersectClipRect( wvt->canvasDc, area.left, area.top, area.right, area.bottom );
   SetBkMode( wvt->canvasDc, TRANSPARENT );
   SetTextColor( wvt->canvasDc, RGB( ( color >> 16 ) & 0xFF, ( color >> 8 ) & 0xFF, color & 0xFF ) );
   textX = s_gt_wvt_CanvasToPixel( wvt, left, 0 );
   textY = s_gt_wvt_CanvasToPixel( wvt, top, 1 );
   ExtTextOutW( wvt->canvasDc, textX, textY, 0, NULL, wideText, ( UINT )count, advances );
   SelectObject( wvt->canvasDc, oldFont );
   RestoreDC( wvt->canvasDc, saved );
   DeleteObject( font );

   x_xfree( advances );
   x_xfree( wideText );
   s_gt_wvt_InvalidateCanvas( wvt );
}

/* Fills x0..x1-1, y0..y1-1 of a cell on the canvas, clipped to the cell. */
static void s_gt_wvt_FillCell( X_GTWVT *wvt, const RECT *cell, int x0, int y0, int x1, int y1, uint32_t color )
{
   int x, y;

   x0 = X_MAX( x0, 0 );
   y0 = X_MAX( y0, 0 );
   x1 = X_MIN( x1, ( int )( cell->right - cell->left ) );
   y1 = X_MIN( y1, ( int )( cell->bottom - cell->top ) );
   for( y = y0; y < y1; ++y )
   {
      uint32_t *row = wvt->pixels + ( size_t )( cell->top + y ) * wvt->canvasWidth + cell->left;

      for( x = x0; x < x1; ++x )
         row[x] = color;
   }
}

/* One arm of a box drawing character, from the edge of the cell towards its centre, across
   q0..q1-1 from the centre line. The arm reaches the centre and goes on for -start pixels;
   a positive start stops it before the centre. Arms: 0 up, 1 right, 2 down, 3 left. */
static void s_gt_wvt_BoxArm( X_GTWVT *wvt, const RECT *cell, int arm, int start, int q0, int q1, uint32_t color )
{
   int width = cell->right - cell->left, height = cell->bottom - cell->top;
   int cx = width / 2, cy = height / 2;

   switch( arm )
   {
      case 0: s_gt_wvt_FillCell( wvt, cell, cx + q0, 0, cx + q1, cy - start + 1, color ); break;
      case 1: s_gt_wvt_FillCell( wvt, cell, cx + start, cy + q0, width, cy + q1, color ); break;
      case 2: s_gt_wvt_FillCell( wvt, cell, cx + q0, cy + start, cx + q1, height, color ); break;
      default: s_gt_wvt_FillCell( wvt, cell, 0, cy + q0, cx - start + 1, cy + q1, color ); break;
   }
}

/* Cells are painted on the canvas; the window gets them through WM_PAINT. Every cell is
   drawn and clipped on its own, as in GTSDL3, so a glyph wider than its cell (for example
   "—" in Courier New) cannot leave pixels in a neighbouring cell that is not redrawn.

   GDI uses font fallback (Uniscribe) only for text that needs complex processing, and one
   character alone never does. A character outside ASCII is therefore followed by U+200B
   (zero width space), which is placed after the cell and clipped away, so glyphs missing
   from Courier New come from a fallback font instead of being drawn as boxes. */
static void s_gt_wvt_Redraw( X_GT_BASE *gt, int row, int startCol, int size )
{
   X_GTWVT *wvt = X_GTWVT_GET( gt );
   int      ctrl = ( wvt->fontAttribute & X_GTI_FONTA_CTRLCHARS ) != 0;
   int      drawBox = ( wvt->fontAttribute & X_GTI_FONTA_DRAWBOX ) != 0;
   int      flushed = 0;
   int      col, color;
   int      fits;
   UINT     textLength;
   uint32_t fore, back;
   INT      advance[2];
   uint8_t  attr;
   uint16_t character;
   wchar_t  text[2];
   RECT     cell, rect;

   if( !wvt->canvasDc )
   {
      wvt->openRequested = 1;
      return;
   }

   SetBkMode( wvt->canvasDc, OPAQUE );
   cell.top = row * wvt->cellSize.y;
   cell.bottom = cell.top + wvt->cellSize.y;
   advance[0] = wvt->cellSize.x;
   advance[1] = 0;
   text[1] = 0x200B;

   for( col = startCol; col < startCol + size; ++col )
   {
      if( !X_GTSELF_GETSCRCHAR( gt, row, col, &color, &attr, &character ) )
         break;
      s_gt_wvt_CellColors( wvt, row, col, color, &fore, &back );
      if( !ctrl )
      {
         if( character < 32 )
            character = s_ctrlChars[character];
         else if( character == 0x7F )
            character = 0x2302;
      }

      cell.left = col * wvt->cellSize.x;
      cell.right = cell.left + wvt->cellSize.x;

      /* X_GTI_FONTA_DRAWBOX: box drawing characters and blocks fill the whole cell, as the
         bitmap font of GTSDL3 does, so frames join without gaps between rows. Pixels written
         directly are not clipped like GDI output, and the canvas gets a new grid size only
         when SetMode() creates it again, so the cell is checked against the canvas. */
      fits = cell.right <= wvt->canvasWidth && cell.bottom <= wvt->canvasHeight;
      if( drawBox && character >= 0x2500 && character <= 0x259F && fits )
      {
         int width = wvt->cellSize.x, height = wvt->cellSize.y;
         int cx = width / 2, cy = height / 2;
         int light = X_MAX( 1, ( width + 4 ) / 10 ), heavy = light * 2;
         int x, y, k;

         if( !flushed )
         {
            /* text still waiting in the GDI batch must not cover the pixels written below */
            GdiFlush();
            flushed = 1;
         }
         s_gt_wvt_FillCell( wvt, &cell, 0, 0, width, height, back );

         if( character == 0x2580 ) /* upper half */
            s_gt_wvt_FillCell( wvt, &cell, 0, 0, width, height / 2, fore );
         else if( character >= 0x2581 && character <= 0x2588 ) /* lower eighths up to the full block */
         {
            int top = height - X_MAX( 1, height * ( character - 0x2580 ) / 8 );

            s_gt_wvt_FillCell( wvt, &cell, 0, top, width, height, fore );
         }
         else if( character >= 0x2589 && character <= 0x258F ) /* left eighths */
            s_gt_wvt_FillCell( wvt, &cell, 0, 0, X_MAX( 1, width * ( 0x2590 - character ) / 8 ), height, fore );
         else if( character == 0x2590 ) /* right half */
            s_gt_wvt_FillCell( wvt, &cell, width / 2, 0, width, height, fore );
         else if( character >= 0x2591 && character <= 0x2593 )
         {
            /* shades of 25, 50 and 75 percent; the pattern follows the canvas, so cells tile */
            for( y = 0; y < height; ++y )
               for( x = 0; x < width; ++x )
               {
                  int ax = cell.left + x, ay = cell.top + y;
                  int filled;

                  if( character == 0x2591 )
                     filled = ax % 2 == 0 && ay % 2 == 0;
                  else if( character == 0x2592 )
                     filled = ( ax + ay ) % 2 == 0;
                  else
                     filled = ax % 2 == 0 || ay % 2 == 0;
                  if( filled )
                     s_gt_wvt_FillCell( wvt, &cell, x, y, x + 1, y + 1, fore );
               }
         }
         else if( character == 0x2594 ) /* upper eighth */
            s_gt_wvt_FillCell( wvt, &cell, 0, 0, width, X_MAX( 1, height / 8 ), fore );
         else if( character == 0x2595 ) /* right eighth */
            s_gt_wvt_FillCell( wvt, &cell, width - X_MAX( 1, width / 8 ), 0, width, height, fore );
         else if( character >= 0x2596 )
         {
            int quadrants = s_boxQuadrants[character - 0x2596];

            if( quadrants & 1 )
               s_gt_wvt_FillCell( wvt, &cell, 0, 0, cx, cy, fore );
            if( quadrants & 2 )
               s_gt_wvt_FillCell( wvt, &cell, cx, 0, width, cy, fore );
            if( quadrants & 4 )
               s_gt_wvt_FillCell( wvt, &cell, 0, cy, cx, height, fore );
            if( quadrants & 8 )
               s_gt_wvt_FillCell( wvt, &cell, cx, cy, width, height, fore );
         }
         else if( character >= 0x2571 && character <= 0x2573 )
         {
            /* diagonals from corner to corner */
            int steps = ( width + height ) * 2;

            for( k = 0; k <= steps; ++k )
            {
               int left, mirror;

               x = ( width - 1 ) * k / steps;
               y = ( height - 1 ) * k / steps - light / 2;
               left = x - light / 2;
               mirror = width - 1 - x - light / 2;
               if( character != 0x2571 )
                  s_gt_wvt_FillCell( wvt, &cell, left, y, left + light, y + light, fore );
               if( character != 0x2572 )
                  s_gt_wvt_FillCell( wvt, &cell, mirror, y, mirror + light, y + light, fore );
            }
         }
         else
         {
            int arms = s_boxArms[character - 0x2500], weight[4], wide[4], arm;
            int dashes = 0;

            if( character >= 0x2504 && character <= 0x250B )
               dashes = IIF( character <= 0x2507, 3, 4 );
            else if( character >= 0x254C && character <= 0x254F )
               dashes = 2;

            /* a double arm is two light lines with a light gap between them */
            for( arm = 0; arm < 4; ++arm )
            {
               weight[arm] = arms >> ( arm * 2 ) & 3;
               if( weight[arm] == 3 )
                  wide[arm] = light * 3;
               else if( weight[arm] == 2 )
                  wide[arm] = heavy;
               else
                  wide[arm] = IIF( weight[arm], light, 0 );
            }

            if( dashes )
            {
               /* the line is cut into equal parts, each with a gap at its end */
               int horizontal = weight[1] != 0, length = IIF( horizontal, width, height );
               int line = wide[IIF( horizontal, 1, 0 )];

               for( k = 0; k < dashes; ++k )
               {
                  int from = length * k / dashes, to = from + X_MAX( 1, length * 2 / ( dashes * 3 ) );

                  if( horizontal )
                     s_gt_wvt_FillCell( wvt, &cell, from, cy - line / 2, to, cy - line / 2 + line, fore );
                  else
                     s_gt_wvt_FillCell( wvt, &cell, cx - line / 2, from, cx - line / 2 + line, to, fore );
               }
            }
            else if( character >= 0x256D && character <= 0x2570 )
            {
               /* arcs: a quarter circle joins the vertical and the horizontal arm */
               int sx = IIF( character == 0x256D || character == 0x2570, 1, -1 );
               int sy = IIF( character <= 0x256E, 1, -1 );
               int radius = X_MIN( cx, cy ), steps = radius * 8 + 8;

               s_gt_wvt_BoxArm( wvt, &cell, IIF( sy > 0, 2, 0 ), radius, -( light / 2 ), light - light / 2, fore );
               s_gt_wvt_BoxArm( wvt, &cell, IIF( sx > 0, 1, 3 ), radius, -( light / 2 ), light - light / 2, fore );
               for( k = 0; k <= steps; ++k )
               {
                  double angle = 1.5707963267948966 * k / steps;

                  x = cx + sx * radius - sx * ( int )floor( radius * cos( angle ) + 0.5 ) - light / 2;
                  y = cy + sy * radius - sy * ( int )floor( radius * sin( angle ) + 0.5 ) - light / 2;
                  s_gt_wvt_FillCell( wvt, &cell, x, y, x + light, y + light, fore );
               }
            }
            else
            {
               for( arm = 0; arm < 4; ++arm )
               {
                  int positive = arm == 1 || arm == 2, opposite = ( arm + 2 ) & 3;

                  if( weight[arm] == 3 )
                  {
                     int side;

                     /* each line of a double arm stops at the inner line of a double neighbour,
                        touches a single neighbour, goes on into the opposite arm, or turns the
                        outer corner of a double arm on its other side */
                     for( side = -1; side <= 1; side += 2 )
                     {
                        int neighbour;
                        int inset, start;
                        int q0 = side * light - light / 2;

                        if( arm == 0 || arm == 2 )
                           neighbour = IIF( side < 0, 3, 1 );
                        else
                           neighbour = IIF( side < 0, 0, 2 );
                        if( weight[neighbour] == 3 )
                           inset = light;
                        else if( weight[neighbour] || weight[opposite] )
                           inset = 0;
                        else
                           inset = IIF( weight[( neighbour + 2 ) & 3] == 3, -light, 0 );
                        start = IIF( positive, inset - light / 2, inset - ( light - 1 - light / 2 ) );
                        s_gt_wvt_BoxArm( wvt, &cell, arm, start, q0, q0 + light, fore );
                     }
                  }
                  else if( weight[arm] )
                  {
                     /* a single arm covers the widest neighbour, so corners and junctions close */
                     int across = X_MAX( wide[arm], X_MAX( wide[( arm + 1 ) & 3], wide[( arm + 3 ) & 3] ) );
                     int start = IIF( positive, -( across / 2 ), -( across - 1 - across / 2 ) );

                     s_gt_wvt_BoxArm( wvt, &cell, arm, start, -( wide[arm] / 2 ), wide[arm] - wide[arm] / 2, fore );
                  }
               }
            }
         }
         continue;
      }

      text[0] = ( wchar_t )character;
      SetBkColor( wvt->canvasDc, RGB( ( back >> 16 ) & 0xFF, ( back >> 8 ) & 0xFF, back & 0xFF ) );
      SetTextColor( wvt->canvasDc, RGB( ( fore >> 16 ) & 0xFF, ( fore >> 8 ) & 0xFF, fore & 0xFF ) );
      textLength = IIF( character < 0x80, 1, 2 );
      ExtTextOutW( wvt->canvasDc, cell.left, cell.top, ETO_CLIPPED | ETO_OPAQUE, &cell, text, textLength, advance );
   }

   rect.top = rect.bottom = row;
   rect.left = startCol;
   rect.right = startCol + size - 1;
   rect = s_gt_wvt_GetXYFromColRowRect( wvt, rect );
   /* writing the edge cells also repaints the strip outside the grid */
   if( startCol + size >= wvt->cols )
      rect.right = SHRT_MAX;
   if( row >= wvt->rows - 1 )
      rect.bottom = SHRT_MAX;
   InvalidateRect( wvt->window, &rect, FALSE );
}

static void s_gt_wvt_Refresh( X_GT_BASE *gt )
{
   X_GTWVT *wvt = X_GTWVT_GET( gt );

   s_super.Refresh( gt );

   if( !wvt->window && wvt->openRequested )
      s_gt_wvt_CreateConsoleWindow( wvt );

   if( wvt->window )
   {
      if( wvt->resized )
      {
         wvt->resized = 0;
         s_super.Refresh( gt );
      }
      s_gt_wvt_UpdateCursor( wvt );
      s_gt_wvt_PumpEvents( wvt );
   }
}

/* ---------------------------------------------------------------------------------------------
   Registration
   --------------------------------------------------------------------------------------------- */

static int s_gt_wvt_FuncInit( X_GT_FUNCS *funcs )
{
   funcs->Init = s_gt_wvt_Init;
   funcs->Exit = s_gt_wvt_Exit;
   funcs->SetMode = s_gt_wvt_SetMode;
   funcs->Redraw = s_gt_wvt_Redraw;
   funcs->Refresh = s_gt_wvt_Refresh;
   funcs->Version = s_gt_wvt_Version;
   funcs->Info = s_gt_wvt_Info;
   funcs->Alert = s_gt_wvt_Alert;
   funcs->ReadKey = s_gt_wvt_ReadKey;
   funcs->TrueColor = s_gt_wvt_TrueColor;

   funcs->MouseIsPresent = s_gt_wvt_mouse_IsPresent;
   funcs->MouseGetPos = s_gt_wvt_mouse_GetPos;
   funcs->MouseSetPos = s_gt_wvt_mouse_SetPos;
   funcs->MouseButtonState = s_gt_wvt_mouse_ButtonState;
   funcs->MouseCountButton = s_gt_wvt_mouse_CountButton;

   funcs->GfxPrimitive = s_gt_wvt_gfx_Primitive;
   funcs->GfxText = s_gt_wvt_gfx_Text;

   return 1;
}

static const X_GT_INIT s_gtInit = { "WVT", s_gt_wvt_FuncInit, &s_super, &s_gtId };

static void __attribute__( ( constructor ) ) s_gt_wvt_Register( void )
{
   x_gtRegister( &s_gtInit );
}

/* Linker anchor used by REQUEST GTWVT. */
X_FUNC( GTWVT )
{
   UNUSED( ctx );
}
