/*
 * Copyright (c) 2024-2026 xDev-Language
 *
 * Library...: xDev GUI
 * File......: internal-gui-xfont.c
 *
 */

#include <string.h>
#include "internal-gui.h"
#include "../../fonts/xfont-9x18.h"

/* One layout of the children of a window */
typedef struct
{
   HWND parent;
   HDWP defer;
   int width, height;
} X_GUI_LAYOUT;

/* A growing buffer of the font file */
typedef struct
{
   unsigned char *data;
   size_t size;
   size_t capacity;
} X_XFONT_BUF;

/* ---------------------------------------------------------------------------------------------
   The fixed font 9x18 of the GT drivers for GETs and tables: its bitmaps are made once into a TrueType
   font in memory, every pixel a square of 64 x 64 units, and given to this program only
   --------------------------------------------------------------------------------------------- */

/* Font units of one pixel and the pixels above the baseline */
#define X_XFONT_UNIT 64
#define X_XFONT_ASCENT 14

const wchar_t x_guiXfontName[] = L"xFont 9x18";

/* Append bytes to the buffer */
static void s_xbufPut( X_XFONT_BUF *buf, const void *data, size_t size )
{
   if( buf->size + size > buf->capacity )
   {
      buf->capacity = ( buf->size + size ) * 2 + 256;
      buf->data = ( unsigned char * )x_xrealloc( buf->data, buf->capacity );
   }

   memcpy( buf->data + buf->size, data, size );
   buf->size += size;
}

/* Append a big-endian 16-bit word */
static void s_xbufU16( X_XFONT_BUF *buf, unsigned value )
{
   const unsigned char bytes[2] = { ( unsigned char )( value >> 8 ), ( unsigned char )value };

   s_xbufPut( buf, bytes, 2 );
}

/* Append a big-endian 32-bit word */
static void s_xbufU32( X_XFONT_BUF *buf, uint32_t value )
{
   s_xbufU16( buf, ( unsigned )( value >> 16 ) & 0xFFFF );
   s_xbufU16( buf, ( unsigned )value & 0xFFFF );
}

/* Fill the buffer with zeros up to a 4-byte boundary */
static void s_xbufAlign( X_XFONT_BUF *buf )
{
   static const unsigned char zero[4] = { 0, 0, 0, 0 };

   if( buf->size % 4 != 0 )
      s_xbufPut( buf, zero, 4 - buf->size % 4 );
}

/* The checksum of TrueType: the sum of big-endian 32-bit words, the last one filled with zeros */
static uint32_t s_xbufSum( const unsigned char *data, size_t size )
{
   uint32_t sum = 0;
   size_t i, j;

   for( i = 0; i < size; i += 4 )
   {
      uint32_t word = 0;

      for( j = 0; j < 4; ++j )
         word = ( word << 8 ) | IIF( i + j < size, data[i + j], 0 );

      sum += word;
   }

   return sum;
}

/* One glyph: every run of pixels in a row is a rectangle drawn clockwise; returns its left side */
static int s_xfontGlyph( X_XFONT_BUF *glyf, size_t index, unsigned *points, unsigned *contours )
{
   int runs[XFONT9X18_HEIGHT * 5][3];
   int count = 0, row, x, start, i, j;
   int xMin = XFONT9X18_WIDTH, xMax = 0, yMin = X_XFONT_ASCENT, yMax = X_XFONT_ASCENT - XFONT9X18_HEIGHT;
   int previousX = 0, previousY = 0;

   for( row = 0; row < XFONT9X18_HEIGHT; ++row )
   {
      for( x = 0; x < XFONT9X18_WIDTH; )
      {
         if( !( s_font9x18Bitmaps[index][row] & ( 0x8000 >> x ) ) )
         {
            ++x;
            continue;
         }

         for( start = x; x < XFONT9X18_WIDTH && ( s_font9x18Bitmaps[index][row] & ( 0x8000 >> x ) ); ++x )
            ;

         runs[count][0] = start;
         runs[count][1] = x;
         runs[count][2] = row;
         xMin = IIF( start < xMin, start, xMin );
         xMax = IIF( x > xMax, x, xMax );
         yMax = IIF( X_XFONT_ASCENT - row > yMax, X_XFONT_ASCENT - row, yMax );
         yMin = IIF( X_XFONT_ASCENT - row - 1 < yMin, X_XFONT_ASCENT - row - 1, yMin );
         ++count;
      }
   }

   if( count == 0 )
      return 0;

   s_xbufU16( glyf, ( unsigned )count );
   s_xbufU16( glyf, ( unsigned )( xMin * X_XFONT_UNIT ) & 0xFFFF );
   s_xbufU16( glyf, ( unsigned )( yMin * X_XFONT_UNIT ) & 0xFFFF );
   s_xbufU16( glyf, ( unsigned )( xMax * X_XFONT_UNIT ) & 0xFFFF );
   s_xbufU16( glyf, ( unsigned )( yMax * X_XFONT_UNIT ) & 0xFFFF );
   for( i = 0; i < count; ++i )
      s_xbufU16( glyf, ( unsigned )( i * 4 + 3 ) );

   s_xbufU16( glyf, 0 );
   for( i = 0; i < count * 4; ++i )
      s_xbufPut( glyf, "\x01", 1 );

   for( i = 0; i < count; ++i )
   {
      const int xs[4] = { runs[i][0], runs[i][0], runs[i][1], runs[i][1] };

      for( j = 0; j < 4; ++j )
      {
         s_xbufU16( glyf, ( unsigned )( ( xs[j] - previousX ) * X_XFONT_UNIT ) & 0xFFFF );
         previousX = xs[j];
      }
   }

   for( i = 0; i < count; ++i )
   {
      const int bottom = X_XFONT_ASCENT - runs[i][2] - 1;
      const int ys[4] = { bottom, bottom + 1, bottom + 1, bottom };

      for( j = 0; j < 4; ++j )
      {
         s_xbufU16( glyf, ( unsigned )( ( ys[j] - previousY ) * X_XFONT_UNIT ) & 0xFFFF );
         previousY = ys[j];
      }
   }

   *points = IIF( ( unsigned )count * 4 > *points, ( unsigned )count * 4, *points );
   *contours = IIF( ( unsigned )count > *contours, ( unsigned )count, *contours );

   return xMin * X_XFONT_UNIT;
}

/* A negative number of font units as an unsigned 16-bit word */
static unsigned s_xfontNegative( unsigned value )
{
   return ( 0x10000 - value ) & 0xFFFF;
}

/* The whole TrueType font: OS/2, cmap, glyf, head, hhea, hmtx, loca, maxp, name and post */
static X_XFONT_BUF s_xfontBuild( void )
{
   static const char tags[10][5] = { "OS/2", "cmap", "glyf", "head", "hhea", "hmtx", "loca", "maxp", "name", "post" };
   static const char *names[5] = { "xFont 9x18", "Regular", "xFont 9x18", "xFont 9x18", "xFont9x18" };
   static const int ids[5] = { 1, 2, 3, 4, 6 };
   X_XFONT_BUF table[10], font, strings;
   const unsigned glyphs = XFONT9X18_GLYPH_COUNT;
   const unsigned u = X_XFONT_UNIT;
   const unsigned descent = ( XFONT9X18_HEIGHT - X_XFONT_ASCENT ) * X_XFONT_UNIT;
   unsigned points = 0, contours = 0, segments = 1, search = 1, entry = 0, i;
   uint32_t *loca = ( uint32_t * )x_xgrabArray( glyphs + 1, sizeof( uint32_t ) );
   int *lsb = ( int * )x_xgrabArray( glyphs, sizeof( int ) );
   size_t offset, headAt = 0;
   const char *name;
   uint32_t adjust;

   memset( table, 0, sizeof( table ) );
   memset( &font, 0, sizeof( font ) );
   memset( &strings, 0, sizeof( strings ) );
   for( i = 0; i < glyphs; ++i )
   {
      loca[i] = ( uint32_t )table[2].size;
      lsb[i] = s_xfontGlyph( &table[2], i, &points, &contours );
      s_xbufAlign( &table[2] );
   }

   loca[glyphs] = ( uint32_t )table[2].size;

   /* OS/2 version 3: regular, monospaced, Latin 1 and Latin 2 */
   s_xbufU16( &table[0], 3 );
   s_xbufU16( &table[0], 9 * u );
   s_xbufU16( &table[0], 400 );
   s_xbufU16( &table[0], 5 );
   s_xbufU16( &table[0], 0 );
   for( i = 0; i < 8; ++i )
      s_xbufU16( &table[0], IIF( i % 4 < 2, 9 * u, 0 ) );

   s_xbufU16( &table[0], u );
   s_xbufU16( &table[0], 5 * u );
   s_xbufU16( &table[0], 0 );
   s_xbufPut( &table[0], "\x02\x00\x05\x09\x00\x00\x00\x00\x00\x00", 10 );
   s_xbufU32( &table[0], 0x7 );
   s_xbufU32( &table[0], 0 );
   s_xbufU32( &table[0], 0 );
   s_xbufU32( &table[0], 0 );
   s_xbufPut( &table[0], "XDEV", 4 );
   s_xbufU16( &table[0], 0x40 );
   s_xbufU16( &table[0], 0x20 );
   s_xbufU16( &table[0], 0xFFFD );
   s_xbufU16( &table[0], X_XFONT_ASCENT * u );
   s_xbufU16( &table[0], s_xfontNegative( descent ) );
   s_xbufU16( &table[0], 0 );
   s_xbufU16( &table[0], X_XFONT_ASCENT * u );
   s_xbufU16( &table[0], descent );
   s_xbufU32( &table[0], 0x3 );
   s_xbufU32( &table[0], 0 );
   s_xbufU16( &table[0], 7 * u );
   s_xbufU16( &table[0], 10 * u );
   s_xbufU16( &table[0], 0 );
   s_xbufU16( &table[0], 32 );
   s_xbufU16( &table[0], 1 );

   /* cmap format 4: a segment for every run of following code points; glyph 0 is .notdef */
   for( i = 2; i < glyphs; ++i )
   {
      if( s_font9x18Codepoints[i] != s_font9x18Codepoints[i - 1] + 1 )
         ++segments;
   }

   ++segments;
   for( ; search * 2 <= segments; search *= 2, ++entry )
      ;

   s_xbufU16( &table[1], 0 );
   s_xbufU16( &table[1], 1 );
   s_xbufU16( &table[1], 3 );
   s_xbufU16( &table[1], 1 );
   s_xbufU32( &table[1], 12 );
   s_xbufU16( &table[1], 4 );
   s_xbufU16( &table[1], 16 + 8 * segments );
   s_xbufU16( &table[1], 0 );
   s_xbufU16( &table[1], segments * 2 );
   s_xbufU16( &table[1], search * 2 );
   s_xbufU16( &table[1], entry );
   s_xbufU16( &table[1], segments * 2 - search * 2 );
   for( i = 1; i < glyphs; ++i )
   {
      if( i + 1 == glyphs || s_font9x18Codepoints[i + 1] != s_font9x18Codepoints[i] + 1 )
         s_xbufU16( &table[1], s_font9x18Codepoints[i] );
   }

   s_xbufU16( &table[1], 0xFFFF );
   s_xbufU16( &table[1], 0 );
   for( i = 1; i < glyphs; ++i )
   {
      if( i == 1 || s_font9x18Codepoints[i] != s_font9x18Codepoints[i - 1] + 1 )
         s_xbufU16( &table[1], s_font9x18Codepoints[i] );
   }

   s_xbufU16( &table[1], 0xFFFF );
   for( i = 1; i < glyphs; ++i )
   {
      if( i == 1 || s_font9x18Codepoints[i] != s_font9x18Codepoints[i - 1] + 1 )
         s_xbufU16( &table[1], ( i - s_font9x18Codepoints[i] ) & 0xFFFF );
   }

   s_xbufU16( &table[1], 1 );
   for( i = 0; i < segments; ++i )
      s_xbufU16( &table[1], 0 );

   /* head: 18 pixels to the em, long offsets in loca */
   s_xbufU32( &table[3], 0x00010000 );
   s_xbufU32( &table[3], 0x00010000 );
   s_xbufU32( &table[3], 0 );
   s_xbufU32( &table[3], 0x5F0F3CF5 );
   s_xbufU16( &table[3], 0x000B );
   s_xbufU16( &table[3], XFONT9X18_HEIGHT * u );
   for( i = 0; i < 4; ++i )
      s_xbufU32( &table[3], 0 );

   s_xbufU16( &table[3], 0 );
   s_xbufU16( &table[3], s_xfontNegative( descent ) );
   s_xbufU16( &table[3], 9 * u );
   s_xbufU16( &table[3], X_XFONT_ASCENT * u );
   s_xbufU16( &table[3], 0 );
   s_xbufU16( &table[3], 8 );
   s_xbufU16( &table[3], 2 );
   s_xbufU16( &table[3], 1 );
   s_xbufU16( &table[3], 0 );

   /* hhea and hmtx: one advance of 9 pixels for all glyphs */
   s_xbufU32( &table[4], 0x00010000 );
   s_xbufU16( &table[4], X_XFONT_ASCENT * u );
   s_xbufU16( &table[4], s_xfontNegative( descent ) );
   s_xbufU16( &table[4], 0 );
   s_xbufU16( &table[4], 9 * u );
   s_xbufU16( &table[4], 0 );
   s_xbufU16( &table[4], 0 );
   s_xbufU16( &table[4], 9 * u );
   s_xbufU16( &table[4], 1 );
   for( i = 0; i < 7; ++i )
      s_xbufU16( &table[4], 0 );

   s_xbufU16( &table[4], 1 );
   s_xbufU16( &table[5], 9 * u );
   for( i = 0; i < glyphs; ++i )
      s_xbufU16( &table[5], ( unsigned )lsb[i] );

   /* loca and maxp */
   for( i = 0; i <= glyphs; ++i )
      s_xbufU32( &table[6], loca[i] );

   s_xbufU32( &table[7], 0x00010000 );
   s_xbufU16( &table[7], glyphs );
   s_xbufU16( &table[7], points );
   s_xbufU16( &table[7], contours );
   s_xbufU16( &table[7], 0 );
   s_xbufU16( &table[7], 0 );
   s_xbufU16( &table[7], 2 );
   for( i = 0; i < 8; ++i )
      s_xbufU16( &table[7], 0 );

   /* name: Windows, Unicode, English */
   s_xbufU16( &table[8], 0 );
   s_xbufU16( &table[8], 5 );
   s_xbufU16( &table[8], 6 + 12 * 5 );
   for( i = 0; i < 5; ++i )
   {
      s_xbufU16( &table[8], 3 );
      s_xbufU16( &table[8], 1 );
      s_xbufU16( &table[8], 0x409 );
      s_xbufU16( &table[8], ( unsigned )ids[i] );
      s_xbufU16( &table[8], ( unsigned )strlen( names[i] ) * 2 );
      s_xbufU16( &table[8], ( unsigned )strings.size );
      for( name = names[i]; *name != '\0'; ++name )
         s_xbufU16( &strings, ( unsigned char )*name );
   }

   s_xbufPut( &table[8], strings.data, strings.size );
   x_xfree( strings.data );

   /* post version 3: fixed pitch, no glyph names */
   s_xbufU32( &table[9], 0x00030000 );
   s_xbufU32( &table[9], 0 );
   s_xbufU16( &table[9], s_xfontNegative( 2 * u ) );
   s_xbufU16( &table[9], u );
   s_xbufU32( &table[9], 1 );
   for( i = 0; i < 4; ++i )
      s_xbufU32( &table[9], 0 );

   /* The file: the table directory, then the tables on 4-byte boundaries */
   s_xbufU32( &font, 0x00010000 );
   s_xbufU16( &font, 10 );
   s_xbufU16( &font, 8 * 16 );
   s_xbufU16( &font, 3 );
   s_xbufU16( &font, 10 * 16 - 8 * 16 );
   offset = 12 + 16 * 10;
   for( i = 0; i < 10; ++i )
   {
      s_xbufPut( &font, tags[i], 4 );
      s_xbufU32( &font, s_xbufSum( table[i].data, table[i].size ) );
      s_xbufU32( &font, ( uint32_t )offset );
      s_xbufU32( &font, ( uint32_t )table[i].size );
      offset += ( table[i].size + 3 ) & ~( size_t )3;
   }

   for( i = 0; i < 10; ++i )
   {
      if( i == 3 )
         headAt = font.size;

      s_xbufPut( &font, table[i].data, table[i].size );
      s_xbufAlign( &font );
      x_xfree( table[i].data );
   }

   adjust = 0xB1B0AFBA - s_xbufSum( font.data, font.size );
   font.data[headAt + 8] = ( unsigned char )( adjust >> 24 );
   font.data[headAt + 9] = ( unsigned char )( adjust >> 16 );
   font.data[headAt + 10] = ( unsigned char )( adjust >> 8 );
   font.data[headAt + 11] = ( unsigned char )adjust;
   x_xfree( loca );
   x_xfree( lsb );

   return font;
}

/* The font is made and given to this program once, at its first use */
static int s_xfontReady( void )
{
   static int state;
   static X_XFONT_BUF font;
   DWORD count = 0;

   if( state == 0 )
   {
      font = s_xfontBuild();
      state = IIF( AddFontMemResourceEx( font.data, ( DWORD )font.size, NULL, &count ) != NULL, 1, -1 );
   }

   return state == 1;
}

/* GETs and tables write with the fixed font 9x18 unless the program gives them another one */
void x_guiXfontAttach( HWND control )
{
   X_GUI_OWNFONT *own;

   if( GetPropW( control, x_guiFontProperty ) != NULL )
      return;

   own = ( X_GUI_OWNFONT * )x_xgrab( sizeof( X_GUI_OWNFONT ) );
   memset( own, 0, sizeof( *own ) );
   lstrcpynW( own->name, x_guiXfontName, LF_FACESIZE );
   SetPropW( control, x_guiFontProperty, own );
}

/* The system font changed by the gui_Font of the control and made for the DPI of its window */
HFONT x_guiOwnFontCreate( const X_GUI_OWNFONT *own, UINT dpi )
{
   NONCLIENTMETRICSW metrics;
   LOGFONTW font;

   memset( &metrics, 0, sizeof( metrics ) );
   metrics.cbSize = sizeof( metrics );
   if( SystemParametersInfoForDpi( SPI_GETNONCLIENTMETRICS, sizeof( metrics ), &metrics, 0, dpi ) )
      font = metrics.lfMessageFont;
   else
   {
      memset( &font, 0, sizeof( font ) );
      font.lfHeight = -MulDiv( 9, ( int )dpi, 72 );
      lstrcpynW( font.lfFaceName, L"Segoe UI", LF_FACESIZE );
   }

   if( own->size > 0 )
      font.lfHeight = -MulDiv( own->size, ( int )dpi, 72 );

   font.lfWeight = IIF( own->bold, FW_BOLD, FW_NORMAL );
   font.lfItalic = ( BYTE )IIF( own->italic, TRUE, FALSE );
   if( own->name[0] != L'\0' )
      lstrcpynW( font.lfFaceName, own->name, LF_FACESIZE );

   /* The fixed font 9x18 keeps its pixels: 18 pixels high at 96 DPI, without smoothing */
   if( lstrcmpiW( own->name, x_guiXfontName ) == 0 && s_xfontReady() )
   {
      if( own->size == 0 )
         font.lfHeight = -x_guiScale( XFONT9X18_HEIGHT, dpi );

      font.lfWeight = FW_NORMAL;
      font.lfQuality = NONANTIALIASED_QUALITY;
      font.lfPitchAndFamily = FIXED_PITCH | FF_MODERN;
   }

   return CreateFontIndirectW( &font );
}

/* The font of a control at dpi: its own one, otherwise the system font */
HFONT x_guiFontOf( HWND window, UINT dpi )
{
   X_GUI_OWNFONT *own = ( X_GUI_OWNFONT * )GetPropW( window, x_guiFontProperty );
   HFONT font;

   if( own == NULL )
      return x_guiFont( dpi );

   if( own->font == NULL || own->dpi != dpi )
   {
      font = x_guiOwnFontCreate( own, dpi );
      if( font == NULL )
         return x_guiFont( dpi );

      if( own->font != NULL )
         DeleteObject( own->font );

      own->font = font;
      own->dpi = dpi;
   }

   return own->font;
}

/* Release the own font of a control */
void x_guiFontRelease( HWND window )
{
   X_GUI_OWNFONT *own = ( X_GUI_OWNFONT * )RemovePropW( window, x_guiFontProperty );

   if( own == NULL )
      return;

   if( own->font != NULL )
      DeleteObject( own->font );

   x_xfree( own );
}

/* The rectangle of a child window in the client coordinates of its parent */
void x_guiChildFrame( HWND child, RECT *frame )
{
   GetWindowRect( child, frame );
   MapWindowPoints( HWND_DESKTOP, GetParent( child ), ( POINT * )frame, 2 );
}

/* Move, size and anchor a child for a new DPI; data points to the old and the new DPI */
BOOL CALLBACK x_guiRescaleChild( HWND child, LPARAM data )
{
   const UINT *dpi = ( const UINT * )data;
   X_GUI_ANCHOR *anchor = ( X_GUI_ANCHOR * )GetPropW( child, x_guiAnchorProperty );
   int from = ( int )dpi[0];
   int to = ( int )dpi[1];
   int x, y, width, height;
   RECT frame;

   x_guiChildFrame( child, &frame );
   x = MulDiv( frame.left, to, from );
   y = MulDiv( frame.top, to, from );
   width = MulDiv( frame.right - frame.left, to, from );
   height = MulDiv( frame.bottom - frame.top, to, from );
   SetWindowPos( child, NULL, x, y, width, height, SWP_NOZORDER | SWP_NOACTIVATE );
   SendMessageW( child, WM_SETFONT, ( WPARAM )x_guiFontOf( child, dpi[1] ), TRUE );
   if( anchor != NULL )
   {
      anchor->left = MulDiv( anchor->left, to, from );
      anchor->top = MulDiv( anchor->top, to, from );
      anchor->right = MulDiv( anchor->right, to, from );
      anchor->bottom = MulDiv( anchor->bottom, to, from );
   }

   return TRUE;
}

/* Move and size an anchored child for the new size of its parent */
static BOOL CALLBACK s_layoutChild( HWND child, LPARAM data )
{
   X_GUI_LAYOUT *layout = ( X_GUI_LAYOUT * )data;
   const X_GUI_ANCHOR *anchor = ( const X_GUI_ANCHOR * )GetPropW( child, x_guiAnchorProperty );
   RECT frame;
   int x, y, width, height;

   if( anchor == NULL || layout->defer == NULL || GetParent( child ) != layout->parent )
      return TRUE;

   x_guiChildFrame( child, &frame );
   x = frame.left;
   y = frame.top;
   width = frame.right - frame.left;
   height = frame.bottom - frame.top;
   if( ( anchor->flags & X_GUI_ANCHOR_LEFT ) && ( anchor->flags & X_GUI_ANCHOR_RIGHT ) )
   {
      x = anchor->left;
      width = layout->width - anchor->left - anchor->right;
   }
   else if( anchor->flags & X_GUI_ANCHOR_RIGHT )
      x = layout->width - anchor->right - width;

   if( ( anchor->flags & X_GUI_ANCHOR_TOP ) && ( anchor->flags & X_GUI_ANCHOR_BOTTOM ) )
   {
      y = anchor->top;
      height = layout->height - anchor->top - anchor->bottom;
   }
   else if( anchor->flags & X_GUI_ANCHOR_BOTTOM )
      y = layout->height - anchor->bottom - height;

   if( width < 0 )
      width = 0;

   if( height < 0 )
      height = 0;

   layout->defer = DeferWindowPos( layout->defer, child, NULL, x, y, width, height, SWP_NOZORDER | SWP_NOACTIVATE );

   return TRUE;
}

/* Lay out the anchored children of a window for its new client size */
void x_guiLayout( HWND window, int width, int height )
{
   X_GUI_LAYOUT layout = { window, BeginDeferWindowPos( 8 ), width, height };

   if( layout.defer == NULL )
      return;

   EnumChildWindows( window, s_layoutChild, ( LPARAM )&layout );
   if( layout.defer != NULL )
      EndDeferWindowPos( layout.defer );
}
