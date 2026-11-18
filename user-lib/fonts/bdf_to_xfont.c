/*
 * Copyright (c) 2024-2026 xDev-Language
 *
 * Library: xDev Fonts
 * File: bdf_to_xfont.c
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

/*
 * Convert a fixed-cell BDF font up to 16 pixels wide into an xDev C header.
 *
 * Build:
 *    clang -std=c11 -O2 -Wall -Wextra -pedantic bdf_to_xfont.c -o bdf_to_xfont.exe
 *
 * Run:
 *    bdf_to_xfont.exe font.bdf xfont.h
 */

#include <ctype.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define LINE_CAPACITY 1024
#define MAX_FONT_HEIGHT 64

/* One glyph: its code point and one 16-bit word per pixel row */
typedef struct
{
   uint16_t codepoint;
   uint16_t rows[MAX_FONT_HEIGHT];
} GLYPH;

/* State of a SHA-256 digest */
typedef struct
{
   uint32_t state[8];
   uint64_t bitCount;
   uint8_t block[64];
   size_t blockLength;
} SHA256_CTX;

static const uint32_t s_sha256Constants[64] =
{
   0x428A2F98u, 0x71374491u, 0xB5C0FBCFu, 0xE9B5DBA5u,
   0x3956C25Bu, 0x59F111F1u, 0x923F82A4u, 0xAB1C5ED5u,
   0xD807AA98u, 0x12835B01u, 0x243185BEu, 0x550C7DC3u,
   0x72BE5D74u, 0x80DEB1FEu, 0x9BDC06A7u, 0xC19BF174u,
   0xE49B69C1u, 0xEFBE4786u, 0x0FC19DC6u, 0x240CA1CCu,
   0x2DE92C6Fu, 0x4A7484AAu, 0x5CB0A9DCu, 0x76F988DAu,
   0x983E5152u, 0xA831C66Du, 0xB00327C8u, 0xBF597FC7u,
   0xC6E00BF3u, 0xD5A79147u, 0x06CA6351u, 0x14292967u,
   0x27B70A85u, 0x2E1B2138u, 0x4D2C6DFCu, 0x53380D13u,
   0x650A7354u, 0x766A0ABBu, 0x81C2C92Eu, 0x92722C85u,
   0xA2BFE8A1u, 0xA81A664Bu, 0xC24B8B70u, 0xC76C51A3u,
   0xD192E819u, 0xD6990624u, 0xF40E3585u, 0x106AA070u,
   0x19A4C116u, 0x1E376C08u, 0x2748774Cu, 0x34B0BCB5u,
   0x391C0CB3u, 0x4ED8AA4Au, 0x5B9CCA4Fu, 0x682E6FF3u,
   0x748F82EEu, 0x78A5636Fu, 0x84C87814u, 0x8CC70208u,
   0x90BEFFFAu, 0xA4506CEBu, 0xBEF9A3F7u, 0xC67178F2u
};

/* Print an error (with the line of the BDF file when not 0) and end the program */
static void s_fail( const char *message, size_t lineNumber )
{
   if( lineNumber != 0 )
      fprintf( stderr, "bdf_to_xfont: line %zu: %s\n", lineNumber, message );
   else
      fprintf( stderr, "bdf_to_xfont: %s\n", message );

   exit( EXIT_FAILURE );
}

/* Grow a block of memory; the program ends when there is no memory */
static void *s_allocate( void *memory, size_t size )
{
   void *result = realloc( memory, size );

   if( result == NULL )
   {
      free( memory );
      s_fail( "out of memory", 0 );
   }

   return result;
}

/* Rotate the bits of a 32-bit value to the right */
static uint32_t s_rotateRight( uint32_t value, unsigned int count )
{
   return ( value >> count ) | ( value << ( 32u - count ) );
}

/* Process one 64-byte block of SHA-256 */
static void s_sha256Transform( SHA256_CTX *ctx, const uint8_t block[64] )
{
   uint32_t words[64];
   uint32_t a, b, c, d, e, f, g, h;
   size_t index;

   for( index = 0; index < 16; ++index )
   {
      size_t offset = index * 4;

      words[index] = ( uint32_t )block[offset] << 24;
      words[index] |= ( uint32_t )block[offset + 1] << 16;
      words[index] |= ( uint32_t )block[offset + 2] << 8;
      words[index] |= ( uint32_t )block[offset + 3];
   }

   for( index = 16; index < 64; ++index )
   {
      uint32_t w15 = words[index - 15];
      uint32_t w2 = words[index - 2];
      uint32_t s0 = s_rotateRight( w15, 7 ) ^ s_rotateRight( w15, 18 ) ^ ( w15 >> 3 );
      uint32_t s1 = s_rotateRight( w2, 17 ) ^ s_rotateRight( w2, 19 ) ^ ( w2 >> 10 );

      words[index] = words[index - 16] + s0 + words[index - 7] + s1;
   }

   a = ctx->state[0];
   b = ctx->state[1];
   c = ctx->state[2];
   d = ctx->state[3];
   e = ctx->state[4];
   f = ctx->state[5];
   g = ctx->state[6];
   h = ctx->state[7];

   for( index = 0; index < 64; ++index )
   {
      uint32_t sum1 = s_rotateRight( e, 6 ) ^ s_rotateRight( e, 11 ) ^ s_rotateRight( e, 25 );
      uint32_t choose = ( e & f ) ^ ( ( ~e ) & g );
      uint32_t temporary1 = h + sum1 + choose + s_sha256Constants[index] + words[index];
      uint32_t sum0 = s_rotateRight( a, 2 ) ^ s_rotateRight( a, 13 ) ^ s_rotateRight( a, 22 );
      uint32_t majority = ( a & b ) ^ ( a & c ) ^ ( b & c );
      uint32_t temporary2 = sum0 + majority;

      h = g;
      g = f;
      f = e;
      e = d + temporary1;
      d = c;
      c = b;
      b = a;
      a = temporary1 + temporary2;
   }

   ctx->state[0] += a;
   ctx->state[1] += b;
   ctx->state[2] += c;
   ctx->state[3] += d;
   ctx->state[4] += e;
   ctx->state[5] += f;
   ctx->state[6] += g;
   ctx->state[7] += h;
}

/* Start a SHA-256 digest */
static void s_sha256Init( SHA256_CTX *ctx )
{
   static const uint32_t initial[8] =
   {
      0x6A09E667u, 0xBB67AE85u, 0x3C6EF372u, 0xA54FF53Au,
      0x510E527Fu, 0x9B05688Cu, 0x1F83D9ABu, 0x5BE0CD19u
   };

   memcpy( ctx->state, initial, sizeof( initial ) );
   ctx->bitCount = 0;
   ctx->blockLength = 0;
}

/* Add data to a SHA-256 digest */
static void s_sha256Update( SHA256_CTX *ctx, const uint8_t *data, size_t length )
{
   while( length > 0 )
   {
      size_t available = sizeof( ctx->block ) - ctx->blockLength;
      size_t count = available;

      if( length < available )
         count = length;

      memcpy( ctx->block + ctx->blockLength, data, count );
      ctx->blockLength += count;
      ctx->bitCount += ( uint64_t )count * 8u;
      data += count;
      length -= count;

      if( ctx->blockLength == sizeof( ctx->block ) )
      {
         s_sha256Transform( ctx, ctx->block );
         ctx->blockLength = 0;
      }
   }
}

/* Finish a SHA-256 digest: 32 bytes */
static void s_sha256Final( SHA256_CTX *ctx, uint8_t digest[32] )
{
   uint64_t bitCount = ctx->bitCount;
   size_t index;

   ctx->block[ctx->blockLength++] = 0x80;
   if( ctx->blockLength > 56 )
   {
      memset( ctx->block + ctx->blockLength, 0, sizeof( ctx->block ) - ctx->blockLength );
      s_sha256Transform( ctx, ctx->block );
      ctx->blockLength = 0;
   }

   memset( ctx->block + ctx->blockLength, 0, 56 - ctx->blockLength );

   for( index = 0; index < 8; ++index )
      ctx->block[63 - index] = ( uint8_t )( bitCount >> ( index * 8 ) );

   s_sha256Transform( ctx, ctx->block );

   for( index = 0; index < 8; ++index )
   {
      digest[index * 4] = ( uint8_t )( ctx->state[index] >> 24 );
      digest[index * 4 + 1] = ( uint8_t )( ctx->state[index] >> 16 );
      digest[index * 4 + 2] = ( uint8_t )( ctx->state[index] >> 8 );
      digest[index * 4 + 3] = ( uint8_t )ctx->state[index];
   }
}

/* SHA-256 of a file as 64 hexadecimal digits */
static void s_fileSha256( const char *path, char result[65] )
{
   SHA256_CTX ctx;
   uint8_t buffer[16384];
   uint8_t digest[32];
   FILE *file = fopen( path, "rb" );
   size_t length;
   size_t index;

   if( file == NULL )
      s_fail( "cannot open input file", 0 );

   s_sha256Init( &ctx );

   length = fread( buffer, 1, sizeof( buffer ), file );
   while( length != 0 )
   {
      s_sha256Update( &ctx, buffer, length );
      length = fread( buffer, 1, sizeof( buffer ), file );
   }

   if( ferror( file ) )
   {
      fclose( file );
      s_fail( "cannot read input file", 0 );
   }

   fclose( file );
   s_sha256Final( &ctx, digest );

   for( index = 0; index < sizeof( digest ); ++index )
      sprintf( result + index * 2, "%02X", digest[index] );

   result[64] = '\0';
}

/* Remove the line end */
static void s_trimLine( char *line )
{
   size_t length = strlen( line );

   while( length > 0 && ( line[length - 1] == '\n' || line[length - 1] == '\r' ) )
      line[--length] = '\0';
}

/* Order of glyphs for qsort(): by code point */
static int s_compareGlyphs( const void *left, const void *right )
{
   const GLYPH *first = ( const GLYPH * )left;
   const GLYPH *second = ( const GLYPH * )right;

   if( first->codepoint < second->codepoint )
      return -1;

   return first->codepoint > second->codepoint;
}

/* The file name without its directory */
static const char *s_baseName( const char *path )
{
   const char *name = path;
   const char *cursor;

   for( cursor = path; *cursor != '\0'; ++cursor )
   {
      if( *cursor == '/' || *cursor == '\\' )
         name = cursor + 1;
   }

   return name;
}

/* Write the C header with the code points and the bitmaps of the glyphs */
static void s_writeHeader( const char *path, const char *sourceName, const char *sourceHash, const char *copyrightText,
                           const GLYPH *glyphs, size_t glyphCount, int width, int height )
{
   FILE *output = fopen( path, "wb" );
   size_t index;
   int row;

   if( output == NULL )
      s_fail( "cannot create output file", 0 );

   fprintf( output,
            "/*\n"
            " * Fixed %dx%d bitmap font, generated from %s.\n"
            " * Source SHA-256: %s\n"
            " * Source notice: %s\n"
            " *\n"
            " * Generated file. Do not edit manually.\n"
            " */\n\n"
            "#ifndef X_FONT%dX%d_H\n"
            "#define X_FONT%dX%d_H\n\n"
            "#include <stdint.h>\n\n"
            "#define X_FONT%dX%d_WIDTH         %d\n"
            "#define X_FONT%dX%d_HEIGHT       %d\n"
            "#define X_FONT%dX%d_GLYPH_COUNT  %zu\n\n",
            width, height, sourceName, sourceHash, copyrightText, width, height, width, height, width, height, width,
            width, height, height, width, height, glyphCount );

   fprintf( output, "static const uint16_t s_font%dx%dCodepoints[ X_FONT%dX%d_GLYPH_COUNT ] =\n{\n", width, height,
            width, height );

   for( index = 0; index < glyphCount; ++index )
   {
      const char *comma = ",";

      if( index + 1 == glyphCount )
         comma = "";

      if( index % 8 == 0 )
         fputs( "   ", output );
      else
         fputc( ' ', output );

      fprintf( output, "0x%04X%s", glyphs[index].codepoint, comma );
      if( index % 8 == 7 || index + 1 == glyphCount )
         fputc( '\n', output );
   }

   fputs( "};\n\n", output );

   fprintf( output, "static const uint16_t s_font%dx%dBitmaps[ X_FONT%dX%d_GLYPH_COUNT ][ X_FONT%dX%d_HEIGHT ] =\n{\n",
            width, height, width, height, width, height );

   for( index = 0; index < glyphCount; ++index )
   {
      const char *comma = ",";

      if( index + 1 == glyphCount )
         comma = "";

      fputs( "   { ", output );

      for( row = 0; row < height; ++row )
      {
         if( row + 1 == height )
            fprintf( output, "0x%04X", glyphs[index].rows[row] );
         else
            fprintf( output, "0x%04X, ", glyphs[index].rows[row] );
      }

      fprintf( output, " }%s /* U+%04X */\n", comma, glyphs[index].codepoint );
   }

   fprintf( output, "};\n\n#endif /* X_FONT%dX%d_H */\n", width, height );

   if( fclose( output ) != 0 )
      s_fail( "cannot finish output file", 0 );
}

/* Read the BDF file, check that every glyph fills the same cell and write the C header */
int main( int argc, char **argv )
{
   FILE *input;
   GLYPH *glyphs = NULL;
   size_t glyphCount = 0;
   size_t glyphCapacity = 0;
   size_t declaredGlyphCount = 0;
   size_t lineNumber = 0;
   char line[LINE_CAPACITY];
   char copyrightText[256] = "Public domain font. Share and enjoy.";
   char sourceHash[65];
   GLYPH current;
   long currentEncoding = -1;
   int width = 0;
   int height = 0;
   int xOffset = 0;
   int yOffset = 0;
   int bitmapRows = 0;
   int inGlyph = 0;
   int inBitmap = 0;
   int haveEncoding = 0;
   int haveDWidth = 0;
   int haveBbx = 0;
   int sawStartFont = 0;
   int sawEndFont = 0;
   size_t normalizedPaddingRows = 0;
   size_t index;

   if( argc != 3 )
   {
      const char *program = "bdf_to_xfont";

      if( argc > 0 )
         program = argv[0];

      fprintf( stderr, "Usage: %s input.bdf output.h\n", program );
      return EXIT_FAILURE;
   }

   s_fileSha256( argv[1], sourceHash );

   input = fopen( argv[1], "rb" );
   if( input == NULL )
      s_fail( "cannot open input file", 0 );

   while( fgets( line, sizeof( line ), input ) != NULL )
   {
      char extra;
      int fields;

      ++lineNumber;
      if( strchr( line, '\n' ) == NULL && !feof( input ) )
         s_fail( "line is too long", lineNumber );

      s_trimLine( line );

      if( strncmp( line, "STARTFONT ", 10 ) == 0 )
         sawStartFont = 1;
      else if( sscanf( line, "FONTBOUNDINGBOX %d %d %d %d %c", &width, &height, &xOffset, &yOffset, &extra ) == 4 )
      {
         if( width < 1 || width > 16 || height < 1 || height > MAX_FONT_HEIGHT )
            s_fail( "font dimensions must be 1..16 by 1..64 pixels", lineNumber );
      }
      else if( sscanf( line, "CHARS %zu %c", &declaredGlyphCount, &extra ) == 1 )
      {
         if( declaredGlyphCount == 0 )
            s_fail( "CHARS must be greater than zero", lineNumber );
      }
      else if( strncmp( line, "COPYRIGHT \"", 11 ) == 0 )
      {
         size_t length = strlen( line );

         if( length >= 13 && line[length - 1] == '\"' )
         {
            size_t textLength = length - 12;

            if( textLength >= sizeof( copyrightText ) )
               s_fail( "COPYRIGHT property is too long", lineNumber );

            memcpy( copyrightText, line + 11, textLength );
            copyrightText[textLength] = '\0';
         }
      }
      else if( strncmp( line, "STARTCHAR ", 10 ) == 0 )
      {
         if( inGlyph )
            s_fail( "nested STARTCHAR", lineNumber );

         memset( &current, 0, sizeof( current ) );

         currentEncoding = -1;
         bitmapRows = 0;
         inGlyph = 1;
         inBitmap = 0;
         haveEncoding = 0;
         haveDWidth = 0;
         haveBbx = 0;
      }
      else if( inGlyph && strncmp( line, "ENCODING ", 9 ) == 0 )
      {
         fields = sscanf( line + 9, "%ld %c", &currentEncoding, &extra );
         if( fields != 1 || currentEncoding < 0 || currentEncoding > UINT16_MAX )
            s_fail( "ENCODING must be a BMP Unicode code point", lineNumber );

         haveEncoding = 1;
      }
      else if( inGlyph && strncmp( line, "DWIDTH ", 7 ) == 0 )
      {
         int glyphWidth;
         int vertical;

         fields = sscanf( line + 7, "%d %d %c", &glyphWidth, &vertical, &extra );
         if( fields != 2 || glyphWidth != width || vertical != 0 )
            s_fail( "glyph DWIDTH differs from the fixed font cell", lineNumber );

         haveDWidth = 1;
      }
      else if( inGlyph && strncmp( line, "BBX ", 4 ) == 0 )
      {
         int glyphWidth = 0;
         int glyphHeight = 0;
         int glyphXOffset = 0;
         int glyphYOffset = 0;
         int same;

         fields = sscanf( line + 4, "%d %d %d %d %c", &glyphWidth, &glyphHeight, &glyphXOffset, &glyphYOffset, &extra );
         same = glyphWidth == width && glyphHeight == height && glyphXOffset == xOffset && glyphYOffset == yOffset;
         if( fields != 4 || !same )
            s_fail( "glyph BBX differs from the fixed font cell", lineNumber );

         haveBbx = 1;
      }
      else if( inGlyph && strcmp( line, "BITMAP" ) == 0 )
      {
         if( !haveEncoding || !haveDWidth || !haveBbx )
            s_fail( "incomplete glyph before BITMAP", lineNumber );

         inBitmap = 1;
      }
      else if( inGlyph && inBitmap && strcmp( line, "ENDCHAR" ) != 0 )
      {
         char *end;
         unsigned long value;
         int storedBits = ( ( width + 7 ) / 8 ) * 8;
         uint16_t paddingMask = ( uint16_t )( ( 1u << ( storedBits - width ) ) - 1u );

         if( bitmapRows >= height )
            s_fail( "too many bitmap rows", lineNumber );

         value = strtoul( line, &end, 16 );

         while( isspace( ( unsigned char )*end ) )
            ++end;

         if( line == end || *end != '\0' || value > UINT16_MAX )
            s_fail( "invalid bitmap row", lineNumber );

         /* BDF pads every row to a whole byte. Bits outside the BBX width are not image data. */
         if( ( value & paddingMask ) != 0 )
            ++normalizedPaddingRows;

         current.rows[bitmapRows++] = ( uint16_t )( value & ~paddingMask );
      }
      else if( inGlyph && strcmp( line, "ENDCHAR" ) == 0 )
      {
         if( !inBitmap || bitmapRows != height )
            s_fail( "glyph bitmap has the wrong height", lineNumber );

         current.codepoint = ( uint16_t )currentEncoding;
         if( glyphCount == glyphCapacity )
         {
            if( glyphCapacity == 0 )
               glyphCapacity = 256;
            else
               glyphCapacity *= 2;

            glyphs = ( GLYPH * )s_allocate( glyphs, glyphCapacity * sizeof( *glyphs ) );
         }

         glyphs[glyphCount++] = current;
         inGlyph = 0;
         inBitmap = 0;
      }
      else if( strcmp( line, "ENDFONT" ) == 0 )
         sawEndFont = 1;
   }

   if( ferror( input ) )
   {
      fclose( input );
      free( glyphs );
      s_fail( "cannot read input file", lineNumber );
   }

   fclose( input );

   if( !sawStartFont || !sawEndFont || inGlyph )
   {
      free( glyphs );
      s_fail( "incomplete BDF file", lineNumber );
   }

   if( width == 0 || height == 0 || declaredGlyphCount == 0 )
   {
      free( glyphs );
      s_fail( "missing font dimensions or CHARS declaration", 0 );
   }

   if( glyphCount != declaredGlyphCount )
   {
      free( glyphs );
      s_fail( "parsed glyph count differs from CHARS declaration", 0 );
   }

   qsort( glyphs, glyphCount, sizeof( *glyphs ), s_compareGlyphs );

   for( index = 1; index < glyphCount; ++index )
   {
      if( glyphs[index - 1].codepoint == glyphs[index].codepoint )
      {
         free( glyphs );
         s_fail( "duplicate Unicode code point", 0 );
      }
   }

   s_writeHeader( argv[2], s_baseName( argv[1] ), sourceHash, copyrightText, glyphs, glyphCount, width, height );
   printf( "Generated %s: %zu glyphs, %dx%d, SHA-256 %s\n", argv[2], glyphCount, width, height, sourceHash );
   if( normalizedPaddingRows != 0 )
      printf( "Ignored non-image padding bits in %zu bitmap rows.\n", normalizedPaddingRows );

   free( glyphs );

   return EXIT_SUCCESS;
}
