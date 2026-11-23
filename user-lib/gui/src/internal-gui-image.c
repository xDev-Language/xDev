/*
 * Copyright (c) 2024-2026 xDev-Language
 *
 * Library...: xDev GUI
 * File......: internal-gui-image.c
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "internal-gui.h"
#include <zlib.h>

/* A font file of icons given to this program and the name of its family */
typedef struct
{
   char *file;
   wchar_t family[LF_FACESIZE];
} X_GUI_ICONFONT;

static X_GUI_ICONFONT *s_iconFonts;
static size_t s_iconFontCount;

/* ---------------------------------------------------------------------------------------------
   Pictures
   --------------------------------------------------------------------------------------------- */

/* A big-endian 32-bit number of a PNG file */
static unsigned s_pngNumber( const unsigned char *bytes )
{
   return ( ( unsigned )bytes[0] << 24 ) | ( ( unsigned )bytes[1] << 16 ) | ( ( unsigned )bytes[2] << 8 ) | bytes[3];
}

/* Sample number index of a row of samples of depth bits */
static unsigned s_pngSample( const unsigned char *row, unsigned index, int depth )
{
   unsigned bit;

   if( depth == 8 )
      return row[index];

   if( depth == 16 )
      return ( ( unsigned )row[index * 2] << 8 ) | row[index * 2 + 1];

   bit = index * ( unsigned )depth;

   return ( row[bit >> 3] >> ( 8 - depth - ( int )( bit & 7 ) ) ) & ( ( 1u << depth ) - 1 );
}

/* A sample made 8-bit */
static unsigned char s_pngByte( unsigned sample, int depth )
{
   if( depth == 16 )
      return ( unsigned char )( sample >> 8 );

   return ( unsigned char )( sample * 255 / ( ( 1u << depth ) - 1 ) );
}

/* The Paeth predictor of PNG: the neighbor nearest to a + b - c */
static int s_pngPaeth( int a, int b, int c )
{
   int p = a + b - c, pa = abs( p - a ), pb = abs( p - b ), pc = abs( p - c );

   if( pa <= pb && pa <= pc )
      return a;

   return IIF( pb <= pc, b, c );
}

/* PNG without interlacing: the pixels as rows of BGRA from the top, alpha not premultiplied; NULL for a
   damaged or unsupported file */
static unsigned char *s_pngDecode( const unsigned char *data, size_t size, unsigned *width, unsigned *height )
{
   static const unsigned char signature[8] = { 0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A };
   static const int channels[7] = { 1, 0, 3, 1, 2, 0, 4 };

   unsigned char palette[256][4];
   unsigned char *packed = NULL, *raw = NULL, *pixels = NULL;
   size_t packedSize = 0, pos = 8, stride, step;
   unsigned colors = 0, x, y, i, length, transparent[3] = { 0x10000, 0x10000, 0x10000 };
   int depth = 0, type = -1, interlace = 0, bits, done = 0, valid, lowDepth;
   uLongf rawSize;

   *width = 0;
   *height = 0;
   if( size < 8 || memcmp( data, signature, 8 ) != 0 )
      return NULL;

   memset( palette, 0, sizeof( palette ) );
   for( i = 0; i < 256; ++i )
      palette[i][3] = 255;

   while( !done && pos + 12 <= size )
   {
      const unsigned char *kind = data + pos + 4;
      const unsigned char *body = data + pos + 8;

      length = s_pngNumber( data + pos );
      if( length > size - pos - 12 )
         break;

      if( memcmp( kind, "IHDR", 4 ) == 0 && length >= 13 )
      {
         *width = s_pngNumber( body );
         *height = s_pngNumber( body + 4 );
         depth = body[8];
         type = body[9];
         interlace = body[12];
      }
      else if( memcmp( kind, "PLTE", 4 ) == 0 )
      {
         for( colors = 0; colors < 256 && colors * 3 + 2 < length; ++colors )
         {
            palette[colors][0] = body[colors * 3];
            palette[colors][1] = body[colors * 3 + 1];
            palette[colors][2] = body[colors * 3 + 2];
         }
      }
      else if( memcmp( kind, "tRNS", 4 ) == 0 )
      {
         if( type == 3 )
         {
            for( i = 0; i < length && i < 256; ++i )
               palette[i][3] = body[i];
         }
         else
         {
            for( i = 0; i < 3 && i * 2 + 1 < length; ++i )
               transparent[i] = ( ( unsigned )body[i * 2] << 8 ) | body[i * 2 + 1];
         }
      }
      else if( memcmp( kind, "IDAT", 4 ) == 0 )
      {
         packed = ( unsigned char * )x_xrealloc( packed, packedSize + length + 1 );
         memcpy( packed + packedSize, body, length );
         packedSize += length;
      }
      else if( memcmp( kind, "IEND", 4 ) == 0 )
         done = 1;

      pos += 12 + ( size_t )length;
   }

   valid = packed != NULL && *width > 0 && *height > 0 && *width <= 16384 && *height <= 16384 && interlace == 0;
   valid = valid && type >= 0 && type <= 6 && channels[type] != 0;
   lowDepth = ( type == 0 || type == 3 ) && ( depth == 1 || depth == 2 || depth == 4 || depth == 8 );
   valid = valid && ( lowDepth || ( type != 3 && depth == 16 ) || ( type != 3 && depth == 8 ) );
   if( !valid )
   {
      x_xfree( packed );
      return NULL;
   }

   bits = channels[type] * depth;
   stride = ( ( size_t )*width * ( size_t )bits + 7 ) / 8;
   step = ( size_t )IIF( bits >= 8, bits / 8, 1 );
   rawSize = ( uLongf )( ( stride + 1 ) * *height );
   raw = ( unsigned char * )x_xgrab( ( size_t )rawSize );
   if( uncompress( raw, &rawSize, packed, ( uLong )packedSize ) != Z_OK || rawSize != ( stride + 1 ) * *height )
   {
      x_xfree( packed );
      x_xfree( raw );
      return NULL;
   }

   x_xfree( packed );

   /* The filters of the rows are undone in place */
   for( y = 0; y < *height; ++y )
   {
      unsigned char *row = raw + y * ( stride + 1 ) + 1;
      const unsigned char *previous = IIF( y > 0, row - ( stride + 1 ), NULL );
      size_t n;

      for( n = 0; n < stride; ++n )
      {
         int a = IIF( n >= step, row[n - step], 0 );
         int b = IIF( previous != NULL, previous[n], 0 );
         int c = IIF( previous != NULL && n >= step, previous[n - step], 0 );

         switch( row[-1] )
         {
         case 0:
            break;
         case 1:
            row[n] = ( unsigned char )( row[n] + a );
            break;
         case 2:
            row[n] = ( unsigned char )( row[n] + b );
            break;
         case 3:
            row[n] = ( unsigned char )( row[n] + ( a + b ) / 2 );
            break;
         case 4:
            row[n] = ( unsigned char )( row[n] + s_pngPaeth( a, b, c ) );
            break;
         default:
            x_xfree( raw );
            return NULL;
         }
      }
   }

   pixels = ( unsigned char * )x_xgrab( ( size_t )*width * *height * 4 );
   for( y = 0; y < *height; ++y )
   {
      const unsigned char *row = raw + y * ( stride + 1 ) + 1;

      for( x = 0; x < *width; ++x )
      {
         unsigned char *pixel = pixels + ( ( size_t )y * *width + x ) * 4;
         unsigned first = s_pngSample( row, x * ( unsigned )channels[type], depth );
         unsigned r, g, b;

         switch( type )
         {
         case 0:
            pixel[0] = pixel[1] = pixel[2] = s_pngByte( first, depth );
            pixel[3] = ( unsigned char )IIF( first == transparent[0], 0, 255 );
            break;
         case 2:
            r = first;
            g = s_pngSample( row, x * 3 + 1, depth );
            b = s_pngSample( row, x * 3 + 2, depth );
            pixel[0] = s_pngByte( b, depth );
            pixel[1] = s_pngByte( g, depth );
            pixel[2] = s_pngByte( r, depth );
            pixel[3] = 255;
            if( r == transparent[0] && g == transparent[1] && b == transparent[2] )
               pixel[3] = 0;
            break;
         case 3:
            pixel[0] = palette[first & 0xFF][2];
            pixel[1] = palette[first & 0xFF][1];
            pixel[2] = palette[first & 0xFF][0];
            pixel[3] = palette[first & 0xFF][3];
            break;
         case 4:
            pixel[0] = pixel[1] = pixel[2] = s_pngByte( first, depth );
            pixel[3] = s_pngByte( s_pngSample( row, x * 2 + 1, depth ), depth );
            break;
         default:
            pixel[2] = s_pngByte( first, depth );
            pixel[1] = s_pngByte( s_pngSample( row, x * 4 + 1, depth ), depth );
            pixel[0] = s_pngByte( s_pngSample( row, x * 4 + 2, depth ), depth );
            pixel[3] = s_pngByte( s_pngSample( row, x * 4 + 3, depth ), depth );
            break;
         }
      }
   }

   x_xfree( raw );
   UNUSED( colors );

   return pixels;
}

/* The bytes of a picture: put into the program by xdev under its name or under the name of the file without
   the folder and the extension, otherwise read from the file */
static unsigned char *s_pictureBytes( const char *name, size_t *size, int *loaded )
{
   const unsigned char *found = x_resourceFind( name, size );
   const char *start = name;
   const char *dot;
   char stem[260];
   int stemLength;

   *loaded = 0;
   if( found == NULL )
   {
      for( dot = name; *dot != '\0'; ++dot )
      {
         if( *dot == '\\' || *dot == '/' )
            start = dot + 1;
      }

      dot = strrchr( start, '.' );
      stemLength = ( int )IIF( dot != NULL, dot - start, ( ptrdiff_t )strlen( start ) );
      snprintf( stem, sizeof( stem ), "%.*s", stemLength, start );
      found = x_resourceFind( stem, size );
   }

   if( found != NULL )
      return ( unsigned char * )found;

   *loaded = 1;

   return ( unsigned char * )x_fileLoad( name, ( size_t )64 * 1024 * 1024, size );
}

/* A big-endian 16-bit number of a font file */
static unsigned s_fontWord( const unsigned char *bytes )
{
   return ( ( unsigned )bytes[0] << 8 ) | bytes[1];
}

/* The family of a TrueType font: its Windows name in UTF-16 in the table "name"; 0 without it */
static int s_fontFamily( const unsigned char *data, size_t size, wchar_t *family )
{
   const unsigned char *entry = NULL;
   size_t tables, at, records, start, length, offset, i;

   tables = IIF( size >= 12, s_fontWord( data + 4 ), 0 );
   for( i = 0; i < tables && 12 + i * 16 + 16 <= size && entry == NULL; ++i )
   {
      if( memcmp( data + 12 + i * 16, "name", 4 ) == 0 )
         entry = data + 12 + i * 16;
   }

   if( entry == NULL )
      return 0;

   at = s_pngNumber( entry + 8 );
   if( at + 6 > size )
      return 0;

   records = s_fontWord( data + at + 2 );
   start = at + s_fontWord( data + at + 4 );
   for( i = 0; i < records && at + 6 + i * 12 + 12 <= size; ++i )
   {
      entry = data + at + 6 + i * 12;
      length = s_fontWord( entry + 8 ) / 2;
      offset = start + s_fontWord( entry + 10 );
      if( s_fontWord( entry ) != 3 || s_fontWord( entry + 6 ) != 1 || length >= LF_FACESIZE )
         continue;

      if( offset + length * 2 > size )
         return 0;

      for( i = 0; i < length; ++i )
         family[i] = ( wchar_t )s_fontWord( data + offset + i * 2 );

      family[length] = L'\0';

      return 1;
   }

   return 0;
}

/* The family of the font in the file, given to this program at its first use; NULL when it is not a font */
static const wchar_t *s_iconFamily( const char *name )
{
   wchar_t family[LF_FACESIZE];
   unsigned char *bytes;
   size_t size = 0, i;
   DWORD count = 0;
   int loaded = 0, valid;

   for( i = 0; i < s_iconFontCount; ++i )
   {
      if( strcmp( s_iconFonts[i].file, name ) == 0 )
         return s_iconFonts[i].family;
   }

   bytes = s_pictureBytes( name, &size, &loaded );
   if( bytes == NULL )
      return NULL;

   valid = s_fontFamily( bytes, size, family );
   valid = valid && AddFontMemResourceEx( bytes, ( DWORD )size, NULL, &count ) != NULL;
   if( loaded )
      x_xfree( bytes );

   if( !valid )
      return NULL;

   s_iconFonts = ( X_GUI_ICONFONT * )x_xrealloc( s_iconFonts, ( s_iconFontCount + 1 ) * sizeof( X_GUI_ICONFONT ) );
   s_iconFonts[s_iconFontCount].file = x_strdup( name );
   lstrcpynW( s_iconFonts[s_iconFontCount].family, family, LF_FACESIZE );
   ++s_iconFontCount;

   return s_iconFonts[s_iconFontCount - 1].family;
}

/* The character code as UTF-16 in text, a pair of surrogates above 0xFFFF; returns its length */
int x_guiGlyphText( unsigned code, wchar_t *text )
{
   if( code <= 0xFFFF )
   {
      text[0] = ( wchar_t )code;
      return 1;
   }

   text[0] = ( wchar_t )( 0xD800 + ( ( code - 0x10000 ) >> 10 ) );
   text[1] = ( wchar_t )( 0xDC00 + ( ( code - 0x10000 ) & 0x3FF ) );

   return 2;
}

/* A 32-bit DIB section of size x size pixels from the top row down; its pixels in bits */
static HBITMAP s_iconSquare( int size, void **bits )
{
   BITMAPINFO format;

   memset( &format, 0, sizeof( format ) );
   format.bmiHeader.biSize = sizeof( format.bmiHeader );
   format.bmiHeader.biWidth = size;
   format.bmiHeader.biHeight = -size;
   format.bmiHeader.biPlanes = 1;
   format.bmiHeader.biBitCount = 32;
   format.bmiHeader.biCompression = BI_RGB;

   return CreateDIBSection( NULL, &format, DIB_RGB_COLORS, bits, NULL, 0 );
}

/* The character code of the font drawn white on black on a square of size pixels with ClearType, whose lines keep
   to the pixels as in code editors; the mean of the three colors of a pixel is its alpha */
static HBITMAP s_iconDraw( const wchar_t *family, unsigned code, COLORREF color, int size )
{
   RECT square = { 0, 0, size, size };
   wchar_t text[2];
   int length;
   LOGFONTW face;
   unsigned char *pixel;
   HBITMAP picture;
   HGDIOBJ oldPicture, oldFont;
   HFONT font;
   void *bits;
   HDC dc;
   int alpha, i;

   length = x_guiGlyphText( code, text );
   picture = s_iconSquare( size, &bits );
   if( picture == NULL )
      return NULL;

   memset( &face, 0, sizeof( face ) );
   face.lfHeight = -size;
   face.lfWeight = FW_NORMAL;
   face.lfCharSet = DEFAULT_CHARSET;
   face.lfOutPrecision = OUT_TT_ONLY_PRECIS;
   face.lfQuality = CLEARTYPE_QUALITY;
   lstrcpynW( face.lfFaceName, family, LF_FACESIZE );
   font = CreateFontIndirectW( &face );
   dc = CreateCompatibleDC( NULL );
   oldPicture = SelectObject( dc, picture );
   oldFont = SelectObject( dc, font );
   SetBkMode( dc, TRANSPARENT );
   SetTextColor( dc, RGB( 255, 255, 255 ) );
   DrawTextW( dc, text, length, &square, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX );
   GdiFlush();
   SelectObject( dc, oldFont );
   SelectObject( dc, oldPicture );
   DeleteDC( dc );
   DeleteObject( font );

   for( i = 0; i < size * size; ++i )
   {
      pixel = ( unsigned char * )bits + ( size_t )i * 4;
      alpha = ( pixel[0] + pixel[1] + pixel[2] ) / 3;
      pixel[0] = GetBValue( color );
      pixel[1] = GetGValue( color );
      pixel[2] = GetRValue( color );
      pixel[3] = ( unsigned char )alpha;
   }

   return picture;
}

/* A data of a gui_Icon object */
static int s_iconValue( X_CALL_CTX *ctx, const X_ITEM *icon, const char *name, X_ITEM *value )
{
   x_itemClear( value );

   return x_objSend( ctx, value, ( X_ITEM * )icon, name, 0, NULL );
}

/* The font, the character, the color and the size of a gui_Icon; 0 when the object is not complete or its file is
   not a font */
int x_guiFontIconGlyph( X_CALL_CTX *ctx, const X_ITEM *icon, X_GUI_GLYPH *glyph )
{
   X_ITEM file __attribute__( ( cleanup( x_itemClear ) ) ) = X_ITEM_NIL_INIT;
   X_ITEM code __attribute__( ( cleanup( x_itemClear ) ) ) = X_ITEM_NIL_INIT;
   X_ITEM color __attribute__( ( cleanup( x_itemClear ) ) ) = X_ITEM_NIL_INIT;
   X_ITEM size __attribute__( ( cleanup( x_itemClear ) ) ) = X_ITEM_NIL_INIT;
   const wchar_t *family = NULL;
   void *handle = NULL;
   size_t length = 0;
   const char *name;
   int valid;

   valid = s_iconValue( ctx, icon, "FILE", &file ) && X_IS_STRING( &file );
   valid = valid && s_iconValue( ctx, icon, "CODE", &code ) && X_IS_NUMERIC( &code );
   valid = valid && s_iconValue( ctx, icon, "COLOR", &color ) && X_IS_NUMERIC( &color );
   valid = valid && s_iconValue( ctx, icon, "SIZE", &size ) && X_IS_NUMERIC( &size ) && x_itemGetNI( &size ) > 0;
   if( valid )
   {
      name = x_itemGetStrUTF8( &file, &handle, &length );
      if( memchr( name, '\0', length ) == NULL )
         family = s_iconFamily( name );

      x_strfree( handle );
   }

   if( family == NULL )
      return 0;

   lstrcpynW( glyph->family, family, LF_FACESIZE );
   glyph->code = ( unsigned )x_itemGetNInt( &code );
   glyph->color = ( int )x_itemGetNInt( &color );
   glyph->size = x_itemGetNI( &size );

   return 1;
}

/* The picture of a gui_Icon; NULL with an error raised */
static HBITMAP s_iconPicture( X_CALL_CTX *ctx, const X_ITEM *icon, const char *operation )
{
   COLORREF ink = GetSysColor( COLOR_BTNTEXT );
   HBITMAP picture = NULL;
   X_GUI_GLYPH glyph;

   if( x_guiFontIconGlyph( ctx, icon, &glyph ) )
   {
      if( glyph.color >= 0 )
         ink = x_guiColorRef( glyph.color );

      picture = s_iconDraw( glyph.family, glyph.code, ink, glyph.size );
   }

   if( picture == NULL )
      x_guiRaise( ctx, operation );

   return picture;
}

/* Whether an argument names a picture: the name of a PNG or BMP file or a gui_Icon */
int x_guiIsPicture( const X_ITEM *item )
{
   return item != NULL && !X_IS_NIL( item ) && ( X_IS_STRING( item ) || x_objIsKindOf( item, "GUIFONTICON" ) );
}

/* A PNG or BMP picture or a gui_Icon as a DIB section; NULL with an error raised */
HBITMAP x_guiPicture( X_CALL_CTX *ctx, const X_ITEM *file, const char *operation )
{
   void *handle = NULL;
   size_t length = 0, size = 0;
   const char *name = IIF( file != NULL && X_IS_STRING( file ), x_itemGetStrUTF8( file, &handle, &length ), NULL );
   unsigned char *bytes = NULL, *pixels;
   const BITMAPFILEHEADER *header;
   const BITMAPINFO *info;
   BITMAPINFO format;
   HBITMAP picture = NULL;
   void *bits;
   unsigned width, height;
   int loaded = 0, valid;
   size_t stride, rows;

   if( file != NULL && !X_IS_NIL( file ) && x_objIsKindOf( file, "GUIFONTICON" ) )
      return s_iconPicture( ctx, file, operation );

   if( name != NULL && memchr( name, '\0', length ) == NULL )
      bytes = s_pictureBytes( name, &size, &loaded );

   x_strfree( handle );
   if( bytes == NULL )
   {
      x_guiRaise( ctx, operation );
      return NULL;
   }

   pixels = s_pngDecode( bytes, size, &width, &height );
   if( pixels != NULL )
   {
      memset( &format, 0, sizeof( format ) );
      format.bmiHeader.biSize = sizeof( format.bmiHeader );
      format.bmiHeader.biWidth = ( LONG )width;
      format.bmiHeader.biHeight = -( LONG )height;
      format.bmiHeader.biPlanes = 1;
      format.bmiHeader.biBitCount = 32;
      format.bmiHeader.biCompression = BI_RGB;
      picture = CreateDIBSection( NULL, &format, DIB_RGB_COLORS, &bits, NULL, 0 );
      if( picture != NULL )
         memcpy( bits, pixels, ( size_t )width * height * 4 );

      x_xfree( pixels );
   }
   else if( size > sizeof( BITMAPFILEHEADER ) + sizeof( BITMAPINFOHEADER ) && bytes[0] == 'B' && bytes[1] == 'M' )
   {
      header = ( const BITMAPFILEHEADER * )bytes;
      info = ( const BITMAPINFO * )( bytes + sizeof( BITMAPFILEHEADER ) );
      stride = ( ( ( size_t )info->bmiHeader.biWidth * info->bmiHeader.biBitCount + 31 ) / 32 ) * 4;
      rows = ( size_t )abs( info->bmiHeader.biHeight );
      valid = info->bmiHeader.biCompression == BI_RGB && info->bmiHeader.biWidth > 0 && header->bfOffBits < size;
      if( valid && stride * rows <= size - header->bfOffBits )
         picture = CreateDIBSection( NULL, info, DIB_RGB_COLORS, &bits, NULL, 0 );

      if( picture != NULL )
         memcpy( bits, bytes + header->bfOffBits, stride * rows );
   }

   if( loaded )
      x_xfree( bytes );

   if( picture == NULL )
      x_guiRaise( ctx, operation );

   return picture;
}
