/*
 * Copyright (c) 2024-2026 xDev-Language
 *
 * Library...: xDev QRCODE
 * File......: internal-qrcode.c
 *
 */

#include "internal-qrcode.h"

#include <stdlib.h>
#include <string.h>

/* QR code model 2 (ISO/IEC 18004): numeric, alphanumeric or byte mode for the whole text, versions
   1-40, error correction L/M/Q/H and the mask with the lowest penalty */

/* The modules of the code and the ones taken by the function patterns */
typedef struct
{
   uint8_t *modules;
   uint8_t *function;
   int size;
} X_QR_GRID;

/* Error correction codewords per block, by level (L, M, Q, H) and version */
static const int8_t s_eccPerBlock[4][41] =
{
   { -1, 7, 10, 15, 20, 26, 18, 20, 24, 30, 18, 20, 24, 26, 30, 22, 24, 28, 30, 28, 28,
          28, 28, 30, 30, 26, 28, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30 },
   { -1, 10, 16, 26, 18, 24, 16, 18, 22, 22, 26, 30, 22, 22, 24, 24, 28, 28, 26, 26, 26,
          26, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28 },
   { -1, 13, 22, 18, 26, 18, 24, 18, 22, 20, 24, 28, 26, 24, 20, 30, 24, 28, 28, 26, 30,
          28, 30, 30, 30, 30, 28, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30 },
   { -1, 17, 28, 22, 16, 22, 28, 26, 26, 24, 28, 24, 28, 22, 24, 24, 30, 28, 28, 26, 28,
          30, 24, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30 }
};

/* Number of error correction blocks, by level (L, M, Q, H) and version */
static const int8_t s_numBlocks[4][41] =
{
   { -1, 1, 1, 1, 1, 1, 2, 2, 2, 2, 4, 4, 4, 4, 4, 6, 6, 6, 6, 7, 8,
           8, 9, 9, 10, 12, 12, 12, 13, 14, 15, 16, 17, 18, 19, 19, 20, 21, 22, 24, 25 },
   { -1, 1, 1, 1, 2, 2, 4, 4, 4, 5, 5, 5, 8, 9, 9, 10, 10, 11, 13, 14, 16,
          17, 17, 18, 20, 21, 23, 25, 26, 28, 29, 31, 33, 35, 37, 38, 40, 43, 45, 47, 49 },
   { -1, 1, 1, 2, 2, 4, 4, 6, 6, 8, 8, 8, 10, 12, 16, 12, 17, 16, 18, 21, 20,
          23, 23, 25, 27, 29, 34, 34, 35, 38, 40, 43, 45, 48, 51, 53, 56, 59, 62, 65, 68 },
   { -1, 1, 1, 2, 4, 4, 4, 5, 6, 8, 8, 11, 11, 16, 16, 18, 16, 19, 21, 25, 25,
          25, 34, 30, 32, 35, 37, 40, 42, 45, 48, 51, 54, 57, 60, 63, 66, 70, 74, 77, 81 }
};

/* Level bits of the format information for L, M, Q, H */
static const int s_formatLevel[4] = { 1, 0, 3, 2 };

/* The characters of the alphanumeric mode, in the order of their values */
static const char s_alphanumeric[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ $%*+-./:";

/* ---------------------------------------------------------------------------------------------
   Capacity
   --------------------------------------------------------------------------------------------- */

/* The modules of a version that carry data and error correction */
static int s_rawModules( int version )
{
   int result = ( 16 * version + 128 ) * version + 64;

   if( version >= 2 )
   {
      int count = version / 7 + 2;

      result -= ( 25 * count - 10 ) * count - 55;
      if( version >= 7 )
         result -= 36;
   }

   return result;
}

/* The data codewords of a version and level */
static int s_dataCodewords( int version, int ecc )
{
   return s_rawModules( version ) / 8 - s_eccPerBlock[ecc][version] * s_numBlocks[ecc][version];
}

/* The centers of the alignment patterns along one axis; returns their number */
static int s_alignmentPositions( int version, int *positions )
{
   int count;
   int step;
   int pos;
   int i;

   if( version == 1 )
      return 0;

   count = version / 7 + 2;
   step = IIF( version == 32, 26, ( version * 4 + count * 2 + 1 ) / ( count * 2 - 2 ) * 2 );

   positions[0] = 6;
   for( i = count - 1, pos = version * 4 + 10; i >= 1; i--, pos -= step )
      positions[i] = pos;

   return count;
}

/* ---------------------------------------------------------------------------------------------
   Data bits
   --------------------------------------------------------------------------------------------- */

/* Append the count lowest bits of value, the highest first */
static void s_bits( uint8_t *buffer, int *bitLength, uint32_t value, int count )
{
   int i;

   for( i = count - 1; i >= 0; i-- )
   {
      if( ( value >> i ) & 1 )
         buffer[*bitLength >> 3] |= ( uint8_t )( 0x80 >> ( *bitLength & 7 ) );

      ( *bitLength )++;
   }
}

/* The mode indicator: 1 numeric, 2 alphanumeric, 4 byte */
static int s_mode( const char *text, size_t length )
{
   int numeric = 1;
   size_t i;

   for( i = 0; i < length; i++ )
   {
      if( text[i] < '0' || text[i] > '9' )
         numeric = 0;

      if( text[i] == '\0' || strchr( s_alphanumeric, text[i] ) == NULL )
         return 4;
   }

   return IIF( numeric, 1, 2 );
}

/* The bits of the character count of a mode, longer for larger versions */
static int s_countBits( int mode, int version )
{
   int range = 2;

   if( version <= 9 )
      range = 0;
   else if( version <= 26 )
      range = 1;

   switch( mode )
   {
   case 1: return 10 + range * 2;
   case 2: return 9 + range * 2;
   }

   return IIF( range == 0, 8, 16 );
}

/* The bits of the text in a mode, without the mode indicator and the count */
static long s_dataBits( int mode, size_t length )
{
   long bits;

   switch( mode )
   {
   case 1:
      bits = ( long )( length / 3 * 10 );
      if( length % 3 == 1 )
         bits += 4;
      else if( length % 3 == 2 )
         bits += 7;

      return bits;

   case 2:
      return ( long )( length / 2 * 11 + length % 2 * 6 );
   }

   return ( long )length * 8;
}

/* Append the text in its mode: three digits in 10 bits, two characters in 11 bits, or bytes */
static void s_encodeData( uint8_t *buffer, int *bitLength, int mode, const char *text, size_t length )
{
   size_t i;

   if( mode == 1 )
   {
      for( i = 0; i < length; i += 3 )
      {
         size_t digits = IIF( length - i >= 3, 3, length - i );
         uint32_t value = 0;
         size_t j;

         for( j = 0; j < digits; j++ )
            value = value * 10 + ( uint32_t )( text[i + j] - '0' );

         s_bits( buffer, bitLength, value, ( int )digits * 3 + 1 );
      }
   }
   else if( mode == 2 )
   {
      for( i = 0; i + 1 < length; i += 2 )
      {
         uint32_t first = ( uint32_t )( strchr( s_alphanumeric, text[i] ) - s_alphanumeric );
         uint32_t second = ( uint32_t )( strchr( s_alphanumeric, text[i + 1] ) - s_alphanumeric );

         s_bits( buffer, bitLength, first * 45 + second, 11 );
      }

      if( i < length )
         s_bits( buffer, bitLength, ( uint32_t )( strchr( s_alphanumeric, text[i] ) - s_alphanumeric ), 6 );
   }
   else
   {
      for( i = 0; i < length; i++ )
         s_bits( buffer, bitLength, ( uint8_t )text[i], 8 );
   }
}

/* ---------------------------------------------------------------------------------------------
   Reed-Solomon over GF(256) with the polynomial 0x11D
   --------------------------------------------------------------------------------------------- */

/* The product of two elements of GF(256) */
static uint8_t s_gfMultiply( uint8_t x, uint8_t y )
{
   int z = 0;
   int i;

   for( i = 7; i >= 0; i-- )
   {
      z = ( z << 1 ) ^ ( ( z >> 7 ) * 0x11D );
      z ^= ( ( y >> i ) & 1 ) * x;
   }

   return ( uint8_t )z;
}

/* The generator polynomial of degree error correction codewords, the highest term left out */
static void s_rsDivisor( int degree, uint8_t *result )
{
   uint8_t root = 1;
   int i;
   int j;

   memset( result, 0, ( size_t )degree );
   result[degree - 1] = 1;

   for( i = 0; i < degree; i++ )
   {
      for( j = 0; j < degree; j++ )
      {
         result[j] = s_gfMultiply( result[j], root );
         if( j + 1 < degree )
            result[j] ^= result[j + 1];
      }

      root = s_gfMultiply( root, 0x02 );
   }
}

/* The error correction codewords of a block: the remainder of the division by the generator */
static void s_rsRemainder( const uint8_t *data, int length, const uint8_t *divisor, int degree, uint8_t *result )
{
   int i;
   int j;

   memset( result, 0, ( size_t )degree );

   for( i = 0; i < length; i++ )
   {
      uint8_t factor = data[i] ^ result[0];

      memmove( result, result + 1, ( size_t )degree - 1 );
      result[degree - 1] = 0;

      for( j = 0; j < degree; j++ )
         result[j] ^= s_gfMultiply( divisor[j], factor );
   }
}

/* ---------------------------------------------------------------------------------------------
   Modules
   --------------------------------------------------------------------------------------------- */

/* Set a module dark or light */
static void s_set( X_QR_GRID *grid, int x, int y, int dark )
{
   grid->modules[y * grid->size + x] = ( uint8_t )( dark != 0 );
}

/* Set a module of a function pattern; the data and the mask leave it alone */
static void s_setFunction( X_QR_GRID *grid, int x, int y, int dark )
{
   s_set( grid, x, y, dark );
   grid->function[y * grid->size + x] = 1;
}

/* Draw the format information of the level and the mask twice; with function it is reserved */
static void s_drawFormat( X_QR_GRID *grid, int ecc, int mask, int function )
{
   void ( *set )( X_QR_GRID *, int, int, int ) = IIF( function, s_setFunction, s_set );
   int size = grid->size;
   int data = s_formatLevel[ecc] << 3 | mask;
   int rem = data;
   int bits;
   int i;

   for( i = 0; i < 10; i++ )
      rem = ( rem << 1 ) ^ ( ( rem >> 9 ) * 0x537 );

   bits = ( data << 10 | rem ) ^ 0x5412;

   for( i = 0; i <= 5; i++ )
      set( grid, 8, i, ( bits >> i ) & 1 );

   set( grid, 8, 7, ( bits >> 6 ) & 1 );
   set( grid, 8, 8, ( bits >> 7 ) & 1 );
   set( grid, 7, 8, ( bits >> 8 ) & 1 );
   for( i = 9; i < 15; i++ )
      set( grid, 14 - i, 8, ( bits >> i ) & 1 );

   for( i = 0; i < 8; i++ )
      set( grid, size - 1 - i, 8, ( bits >> i ) & 1 );

   for( i = 8; i < 15; i++ )
      set( grid, 8, size - 15 + i, ( bits >> i ) & 1 );

   set( grid, 8, size - 8, 1 );
}

/* Draw the version information twice, from version 7 on */
static void s_drawVersion( X_QR_GRID *grid, int version )
{
   int rem = version;
   int bits;
   int i;

   if( version < 7 )
      return;

   for( i = 0; i < 12; i++ )
      rem = ( rem << 1 ) ^ ( ( rem >> 11 ) * 0x1F25 );

   bits = version << 12 | rem;

   for( i = 0; i < 18; i++ )
   {
      int a = grid->size - 11 + i % 3;
      int b = i / 3;

      s_setFunction( grid, a, b, ( bits >> i ) & 1 );
      s_setFunction( grid, b, a, ( bits >> i ) & 1 );
   }
}

/* Draw a finder pattern with its light separator around the center x, y */
static void s_drawFinder( X_QR_GRID *grid, int x, int y )
{
   int dx;
   int dy;

   for( dy = -4; dy <= 4; dy++ )
   {
      for( dx = -4; dx <= 4; dx++ )
      {
         int distance = IIF( abs( dx ) > abs( dy ), abs( dx ), abs( dy ) );

         if( x + dx >= 0 && x + dx < grid->size && y + dy >= 0 && y + dy < grid->size )
            s_setFunction( grid, x + dx, y + dy, distance != 2 && distance != 4 );
      }
   }
}

/* Draw an alignment pattern around the center x, y */
static void s_drawAlignment( X_QR_GRID *grid, int x, int y )
{
   int dx;
   int dy;

   for( dy = -2; dy <= 2; dy++ )
   {
      for( dx = -2; dx <= 2; dx++ )
      {
         int distance = IIF( abs( dx ) > abs( dy ), abs( dx ), abs( dy ) );

         s_setFunction( grid, x + dx, y + dy, distance != 1 );
      }
   }
}

/* Draw the timing, finder and alignment patterns and reserve the format and version areas */
static void s_drawFunctionPatterns( X_QR_GRID *grid, int version, int ecc )
{
   int positions[7];
   int count = s_alignmentPositions( version, positions );
   int size = grid->size;
   int i;
   int j;

   for( i = 0; i < size; i++ )
   {
      s_setFunction( grid, 6, i, i % 2 == 0 );
      s_setFunction( grid, i, 6, i % 2 == 0 );
   }

   s_drawFinder( grid, 3, 3 );
   s_drawFinder( grid, size - 4, 3 );
   s_drawFinder( grid, 3, size - 4 );

   /* The three corners with a finder pattern get no alignment pattern */
   for( i = 0; i < count; i++ )
   {
      for( j = 0; j < count; j++ )
      {
         if( !( ( i == 0 && j == 0 ) || ( i == 0 && j == count - 1 ) || ( i == count - 1 && j == 0 ) ) )
            s_drawAlignment( grid, positions[i], positions[j] );
      }
   }

   s_drawFormat( grid, ecc, 0, 1 );
   s_drawVersion( grid, version );
}

/* Place the codewords in the zigzag of two columns, from the bottom right corner */
static void s_drawCodewords( X_QR_GRID *grid, const uint8_t *data, int length )
{
   int size = grid->size;
   int bit = 0;
   int right;
   int vert;
   int j;

   for( right = size - 1; right >= 1; right -= 2 )
   {
      if( right == 6 )
         right = 5;

      for( vert = 0; vert < size; vert++ )
      {
         for( j = 0; j < 2; j++ )
         {
            int x = right - j;
            int y = IIF( ( ( right + 1 ) & 2 ) == 0, size - 1 - vert, vert );

            if( !grid->function[y * size + x] && bit < length * 8 )
            {
               s_set( grid, x, y, ( data[bit >> 3] >> ( 7 - ( bit & 7 ) ) ) & 1 );
               bit++;
            }
         }
      }
   }
}

/* Invert the data modules of a mask pattern; a second call takes it away again */
static void s_applyMask( X_QR_GRID *grid, int mask )
{
   int size = grid->size;
   int x;
   int y;

   for( y = 0; y < size; y++ )
   {
      for( x = 0; x < size; x++ )
      {
         int invert = 0;

         switch( mask )
         {
         case 0: invert = ( x + y ) % 2 == 0; break;
         case 1: invert = y % 2 == 0; break;
         case 2: invert = x % 3 == 0; break;
         case 3: invert = ( x + y ) % 3 == 0; break;
         case 4: invert = ( x / 3 + y / 2 ) % 2 == 0; break;
         case 5: invert = x * y % 2 + x * y % 3 == 0; break;
         case 6: invert = ( x * y % 2 + x * y % 3 ) % 2 == 0; break;
         case 7: invert = ( ( x + y ) % 2 + x * y % 3 ) % 2 == 0; break;
         }

         if( invert && !grid->function[y * size + x] )
            grid->modules[y * size + x] ^= 1;
      }
   }
}

/* Penalty of the four rules of the standard: long runs, 2x2 blocks, finder-like patterns, balance */
static long s_penalty( const X_QR_GRID *grid )
{
   static const uint8_t patterns[2][11] = {
      { 1, 0, 1, 1, 1, 0, 1, 0, 0, 0, 0 },
      { 0, 0, 0, 0, 1, 0, 1, 1, 1, 0, 1 }
   };
   const uint8_t *m = grid->modules;
   int size = grid->size;
   long total = ( long )size * size;
   long dark = 0;
   long result = 0;
   int a;
   int b;
   int axis;

   /* Axis 0 reads the rows, axis 1 the columns */
   for( axis = 0; axis < 2; axis++ )
   {
      for( a = 0; a < size; a++ )
      {
         int run = 1;

         for( b = 1; b <= size; b++ )
         {
            int previous = IIF( axis == 0, m[a * size + b - 1], m[( b - 1 ) * size + a] );
            int current = -1;

            if( b < size )
               current = IIF( axis == 0, m[a * size + b], m[b * size + a] );

            if( previous == current )
               run++;
            else
            {
               if( run >= 5 )
                  result += 3 + run - 5;

               run = 1;
            }
         }

         for( b = 0; b + 11 <= size; b++ )
         {
            int p;

            for( p = 0; p < 2; p++ )
            {
               int k;

               for( k = 0; k < 11; k++ )
               {
                  int module = IIF( axis == 0, m[a * size + b + k], m[( b + k ) * size + a] );

                  if( module != patterns[p][k] )
                     break;
               }

               if( k == 11 )
                  result += 40;
            }
         }
      }
   }

   for( a = 0; a < size; a++ )
   {
      for( b = 0; b < size; b++ )
      {
         int c = m[a * size + b];

         dark += c;

         if( a + 1 < size && b + 1 < size )
         {
            if( c == m[a * size + b + 1] && c == m[( a + 1 ) * size + b] && c == m[( a + 1 ) * size + b + 1] )
               result += 3;
         }
      }
   }

   result += ( ( labs( dark * 20 - total * 10 ) + total - 1 ) / total - 1 ) * 10;

   return result;
}

/* ---------------------------------------------------------------------------------------------
   Encoder
   --------------------------------------------------------------------------------------------- */

/* The smallest version from minVersion that holds the text, the blocks with error correction
   interleaved, then the mask with the lowest penalty */
int x_qrEncode( const char *text, size_t length, int ecc, int minVersion, uint8_t *modules )
{
   int mode = s_mode( text, length );
   int version;
   int capacity = 0;
   int bitLength = 0;
   uint8_t *data;
   uint8_t *codewords;
   uint8_t *blocks;
   uint8_t divisor[30];
   uint8_t pad;
   int numBlocks, blockEcc, raw, numShort, shortLen;
   int i, j, k, n;
   int best = 0;
   long bestPenalty = -1;
   X_QR_GRID grid;

   for( version = IIF( minVersion > 1, minVersion, 1 ); version <= 40; version++ )
   {
      int count = s_countBits( mode, version );

      capacity = s_dataCodewords( version, ecc ) * 8;
      if( length < ( ( size_t )1 << count ) && 4 + count + s_dataBits( mode, length ) <= capacity )
         break;
   }

   if( version > 40 )
      return 0;

   data = ( uint8_t * )x_xgrabz( ( size_t )capacity / 8 + 1 );
   s_bits( data, &bitLength, ( uint32_t )mode, 4 );
   s_bits( data, &bitLength, ( uint32_t )length, s_countBits( mode, version ) );
   s_encodeData( data, &bitLength, mode, text, length );

   /* The terminator, then the pad bytes 0xEC and 0x11 in turn */
   bitLength += IIF( capacity - bitLength < 4, capacity - bitLength, 4 );
   bitLength = ( bitLength + 7 ) & ~7;
   for( pad = 0xEC; bitLength < capacity; pad ^= 0xEC ^ 0x11 )
      s_bits( data, &bitLength, pad, 8 );

   numBlocks = s_numBlocks[ecc][version];
   blockEcc = s_eccPerBlock[ecc][version];
   raw = s_rawModules( version ) / 8;
   numShort = numBlocks - raw % numBlocks;
   shortLen = raw / numBlocks;

   blocks = ( uint8_t * )x_xgrabz( ( size_t )numBlocks * ( size_t )( shortLen + 1 ) );
   codewords = ( uint8_t * )x_xgrab( ( size_t )raw );
   s_rsDivisor( blockEcc, divisor );

   for( i = 0, k = 0; i < numBlocks; i++ )
   {
      uint8_t *block = blocks + i * ( shortLen + 1 );
      int dataLength = shortLen - blockEcc + IIF( i < numShort, 0, 1 );

      memcpy( block, data + k, ( size_t )dataLength );
      s_rsRemainder( block, dataLength, divisor, blockEcc, block + shortLen + 1 - blockEcc );
      k += dataLength;
   }

   /* The codewords of the blocks in turn; the short blocks have no byte at the end of the data */
   for( i = 0, n = 0; i <= shortLen; i++ )
   {
      for( j = 0; j < numBlocks; j++ )
      {
         if( i != shortLen - blockEcc || j >= numShort )
            codewords[n++] = blocks[j * ( shortLen + 1 ) + i];
      }
   }

   grid.size = version * 4 + 17;
   grid.modules = modules;
   grid.function = ( uint8_t * )x_xgrabz( ( size_t )grid.size * ( size_t )grid.size );
   memset( modules, 0, ( size_t )grid.size * ( size_t )grid.size );

   s_drawFunctionPatterns( &grid, version, ecc );
   s_drawCodewords( &grid, codewords, raw );

   for( i = 0; i < 8; i++ )
   {
      long penalty;

      s_applyMask( &grid, i );
      s_drawFormat( &grid, ecc, i, 0 );
      penalty = s_penalty( &grid );
      if( bestPenalty < 0 || penalty < bestPenalty )
      {
         best = i;
         bestPenalty = penalty;
      }

      s_applyMask( &grid, i );
   }

   s_applyMask( &grid, best );
   s_drawFormat( &grid, ecc, best, 0 );

   x_xfree( grid.function );
   x_xfree( codewords );
   x_xfree( blocks );
   x_xfree( data );

   return grid.size;
}
