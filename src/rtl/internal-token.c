/*
 * Copyright (c) 2024-2026 xDev-Language
 *
 * Library...: xDev Runtime
 * File......: internal-token.c
 *
 */

#include "internal-private.h"

#include <stdint.h>
#include <string.h>

/* The environment of the incremental tokenizer: this head, the tokens, then a copy of the text; it lives in
   s_tokenEnv or in a string variable passed as @cTokenEnv, and SaveToken() returns it as a string */
typedef struct
{
   char magic[4];
   uint32_t reserved;
   uint64_t count;
   uint64_t next;
   uint64_t textLength;
} S_TOKEN_ENV;

/* The default delimiters, without the bytes 138 and 141, which are part of UTF-8 characters */
static const char s_tokenDefault[] = "\0\t\n\r\x1A ,.;:!?/\\<>()^#&%+-*";
static const char s_tokenEnvMagic[4] = { 'X', 'T', 'K', '1' };

/* The delimiters before and after the token of the last Token() call, see TokenSep() */
static char s_tokenSepChar[2];
static int s_tokenSepFound[2];

/* The global environment of TokenInit() and TokenNext() */
static char *s_tokenEnv;
static size_t s_tokenEnvLength;

/* ---------------------------------------------------------------------------------------------
   Tokens: Token(), NumToken(), AtToken(), TokenLower(), TokenUpper() and TokenSep()
   --------------------------------------------------------------------------------------------- */

/* The delimiter table: the characters of list, or the default list when list is NULL */
void x_tokenDelimiters( const char *list, size_t length, unsigned char isDelimiter[256] )
{
   size_t i;

   if( list == NULL )
   {
      list = s_tokenDefault;
      length = sizeof( s_tokenDefault ) - 1;
   }

   memset( isDelimiter, 0, 256 );
   for( i = 0; i < length; ++i )
      isDelimiter[( unsigned char )list[i]] = 1;
}

/* Count the tokens of text and, when tokens is not NULL, store where they are; without skip the delimiters between
   tokens are skipped, with skip > 0 up to skip delimiters end one token, so empty tokens count too */
size_t x_tokenScan( const char *text, size_t length, const unsigned char isDelimiter[256], size_t skip,
                    X_TOKEN_SPAN *tokens )
{
   size_t count = 0;
   size_t i = 0;
   size_t start = 0;
   size_t n;

   if( length == 0 )
      return 0;

   if( skip == 0 )
   {
      while( i < length )
      {
         while( i < length && isDelimiter[( unsigned char )text[i]] )
            ++i;

         if( i == length )
            break;

         start = i;
         while( i < length && !isDelimiter[( unsigned char )text[i]] )
            ++i;

         if( tokens )
         {
            tokens[count].start = start;
            tokens[count].length = i - start;
         }

         ++count;
      }

      return count;
   }

   for( ;; )
   {
      while( i < length && !isDelimiter[( unsigned char )text[i]] )
         ++i;

      if( tokens )
      {
         tokens[count].start = start;
         tokens[count].length = i - start;
      }

      ++count;
      if( i == length )
         return count;

      for( n = 0; n < skip && i < length && isDelimiter[( unsigned char )text[i]]; ++n )
         ++i;

      start = i;
   }
}

/* All tokens of text, allocated by x_xgrab() */
X_TOKEN_SPAN *x_tokenList( const char *text, size_t length, const unsigned char isDelimiter[256], size_t skip,
                           size_t *count )
{
   X_TOKEN_SPAN *tokens;

   *count = x_tokenScan( text, length, isDelimiter, skip, NULL );
   tokens = ( X_TOKEN_SPAN * )x_xgrabArray( *count + 1, sizeof( X_TOKEN_SPAN ) );
   x_tokenScan( text, length, isDelimiter, skip, tokens );

   return tokens;
}

/* TokenLower() and TokenUpper(): change the first character of the first count tokens of text */
void x_tokenCase( char *text, size_t length, const unsigned char isDelimiter[256], size_t skip, int64_t count,
                  int upper )
{
   size_t found, i;
   X_TOKEN_SPAN *tokens = x_tokenList( text, length, isDelimiter, skip, &found );

   for( i = 0; i < found && ( int64_t )i < count; ++i )
   {
      if( tokens[i].length > 0 )
      {
         size_t start = tokens[i].start;
         size_t offset = start;
         uint32_t codepoint, changed;

         /* The text is changed in place, so a letter whose pair takes fewer bytes stays as it is */
         if( x_utf8Decode( text, start + tokens[i].length, &offset, &codepoint ) )
         {
            changed = IIF( upper, x_utf8UpperChar( codepoint ), x_utf8LowerChar( codepoint ) );
            if( x_utf8CharSize( changed ) == offset - start )
               x_utf8Encode( text + start, changed );
         }
      }
   }

   x_xfree( tokens );
}

/* Remember the separators of token for TokenSep(): the first characters of the delimiter runs before and after it;
   a NULL token clears them */
void x_tokenSepFind( const char *text, size_t length, const unsigned char isDelimiter[256], const X_TOKEN_SPAN *token )
{
   s_tokenSepFound[0] = 0;
   s_tokenSepFound[1] = 0;
   if( token == NULL )
      return;

   if( token->start > 0 && isDelimiter[( unsigned char )text[token->start - 1]] )
   {
      size_t first = token->start - 1;

      while( first > 0 && isDelimiter[( unsigned char )text[first - 1]] )
         --first;

      s_tokenSepChar[0] = text[first];
      s_tokenSepFound[0] = 1;
   }

   if( token->start + token->length < length )
   {
      s_tokenSepChar[1] = text[token->start + token->length];
      s_tokenSepFound[1] = 1;
   }
}

/* The separator before (after 0) or after the token of the last Token() call; the length is 0 or 1 */
size_t x_tokenSep( int after, const char **separator )
{
   int side = IIF( after, 1, 0 );

   *separator = &s_tokenSepChar[side];

   return ( size_t )s_tokenSepFound[side];
}

/* ---------------------------------------------------------------------------------------------
   Incremental tokenizer: TokenInit(), TokenNext(), TokenNum(), TokenAt(), TokenEnd(), TokenExit(),
   SaveToken() and RestToken()
   --------------------------------------------------------------------------------------------- */

/* A new environment for text, allocated by x_xgrab() */
char *x_tokenEnvBuild( const char *text, size_t length, const unsigned char isDelimiter[256], size_t skip,
                       size_t *envLength )
{
   size_t count = x_tokenScan( text, length, isDelimiter, skip, NULL );
   size_t spans = count * sizeof( X_TOKEN_SPAN );
   S_TOKEN_ENV head = { { 0 }, 0, count, 0, length };
   char *env;

   *envLength = sizeof( S_TOKEN_ENV ) + spans + length;
   env = ( char * )x_xgrab( *envLength );
   memcpy( head.magic, s_tokenEnvMagic, 4 );
   memcpy( env, &head, sizeof( head ) );
   x_tokenScan( text, length, isDelimiter, skip, ( X_TOKEN_SPAN * )( env + sizeof( S_TOKEN_ENV ) ) );
   memcpy( env + sizeof( S_TOKEN_ENV ) + spans, text, length );

   return env;
}

/* Is env an environment of length bytes? */
int x_tokenEnvValid( const char *env, size_t length )
{
   S_TOKEN_ENV head;

   if( env == NULL || length < sizeof( S_TOKEN_ENV ) )
      return 0;

   memcpy( &head, env, sizeof( head ) );
   if( memcmp( head.magic, s_tokenEnvMagic, 4 ) != 0 )
      return 0;

   return length == sizeof( S_TOKEN_ENV ) + head.count * sizeof( X_TOKEN_SPAN ) + head.textLength;
}

/* The environment of argument n when it is a string (a copy, *owned is 1), else the global one; NULL without one */
char *x_tokenEnvLoad( X_CALL_CTX *ctx, int n, size_t *length, int *owned )
{
   *owned = X_ISCHAR( ctx, n );
   if( *owned )
   {
      if( !x_tokenEnvValid( x_parc( ctx, n ), x_parclen( ctx, n ) ) )
         return NULL;

      *length = x_parclen( ctx, n );
      return ( char * )x_xmemdup( x_parc( ctx, n ), *length );
   }

   *length = s_tokenEnvLength;

   return IIF( x_tokenEnvValid( s_tokenEnv, s_tokenEnvLength ), s_tokenEnv, NULL );
}

/* Write a changed copy back to argument n; the global environment was changed in place */
void x_tokenEnvSave( X_CALL_CTX *ctx, int n, char *env, size_t length, int owned )
{
   if( owned )
   {
      x_storclen( ctx, env, length, n );
      x_xfree( env );
   }
}

/* Release an environment from x_tokenEnvLoad() that did not change */
void x_tokenEnvRelease( char *env, int owned )
{
   if( owned && env != NULL )
      x_xfree( env );
}

/* The global environment, NULL without one */
const char *x_tokenEnvGlobal( size_t *length )
{
   *length = s_tokenEnvLength;

   return IIF( x_tokenEnvValid( s_tokenEnv, s_tokenEnvLength ), s_tokenEnv, NULL );
}

/* Replace the global environment with env, which was allocated by x_xgrab(); NULL clears it */
void x_tokenEnvSetGlobal( char *env, size_t length )
{
   if( s_tokenEnv != NULL )
      x_xfree( s_tokenEnv );

   s_tokenEnv = env;
   s_tokenEnvLength = length;
}

/* The number of tokens of the environment */
uint64_t x_tokenEnvCount( const char *env )
{
   S_TOKEN_ENV head;

   memcpy( &head, env, sizeof( head ) );

   return head.count;
}

/* The index of the token that TokenNext() returns next */
uint64_t x_tokenEnvNext( const char *env )
{
   S_TOKEN_ENV head;

   memcpy( &head, env, sizeof( head ) );

   return head.next;
}

/* Set the index of the token that TokenNext() returns next */
void x_tokenEnvSetNext( char *env, uint64_t next )
{
   S_TOKEN_ENV head;

   memcpy( &head, env, sizeof( head ) );
   head.next = next;
   memcpy( env, &head, sizeof( head ) );
}

/* Token index of the environment: its text, *start in the text and *length; NULL when there is no such token */
const char *x_tokenEnvToken( const char *env, uint64_t index, size_t *start, size_t *length )
{
   S_TOKEN_ENV head;
   const X_TOKEN_SPAN *tokens = ( const X_TOKEN_SPAN * )( env + sizeof( S_TOKEN_ENV ) );
   const char *text;

   memcpy( &head, env, sizeof( head ) );
   if( index >= head.count )
      return NULL;

   *start = tokens[index].start;
   *length = tokens[index].length;
   text = env + sizeof( S_TOKEN_ENV ) + head.count * sizeof( X_TOKEN_SPAN );

   return text + tokens[index].start;
}

/* ---------------------------------------------------------------------------------------------
   Token functions of the API
   --------------------------------------------------------------------------------------------- */

/* AtToken(): return the position of the n-th token in a string */
void x_tokenAtToken( X_CALL_CTX *ctx )
{
   /* A number as the second argument is already nCount: AtToken( cString, 4 ) */
   int64_t wanted = x_parnintdef( ctx, IIF( X_ISNUM( ctx, 2 ), 2, 3 ), 0 );
   unsigned char isDelimiter[256];
   size_t count;
   X_TOKEN_SPAN *tokens;

   x_tokenDelimiters( IIF( X_ISCHAR( ctx, 2 ), x_parc( ctx, 2 ), NULL ), x_parclen( ctx, 2 ), isDelimiter );
   tokens = x_tokenList( x_parc( ctx, 1 ), x_parclen( ctx, 1 ), isDelimiter, 0, &count );
   /* nCount 0 or missing asks for the last token */
   if( wanted == 0 )
      wanted = ( int64_t )count;

   x_retnint( ctx, IIF( wanted >= 1 && ( size_t )wanted <= count, ( int64_t )tokens[wanted - 1].start + 1, 0 ) );
   x_xfree( tokens );
}

/* RestToken(): restore the global environment of the incremental tokenizer */
void x_tokenRestToken( X_CALL_CTX *ctx )
{
   /* A string that SaveToken() did not return clears the environment */
   if( x_tokenEnvValid( x_parc( ctx, 1 ), x_parclen( ctx, 1 ) ) )
      x_tokenEnvSetGlobal( ( char * )x_xmemdup( x_parc( ctx, 1 ), x_parclen( ctx, 1 ) ), x_parclen( ctx, 1 ) );
   else
      x_tokenEnvSetGlobal( NULL, 0 );

   x_retc( ctx, "" );
}

/* Token(): retrieve the n-th token from a string */
void x_tokenToken( X_CALL_CTX *ctx )
{
   /* A number as the second argument is already nTokenPos: Token( cString, 1 ) */
   int shift = IIF( X_ISNUM( ctx, 2 ), 1, 0 );
   const char *text = x_parc( ctx, 1 );
   size_t length = x_parclen( ctx, 1 );
   int64_t wanted = x_parnintdef( ctx, 3 - shift, 0 );
   unsigned char isDelimiter[256];
   const char *separator;
   size_t count, separatorLength;
   X_TOKEN_SPAN *tokens;

   x_tokenDelimiters( IIF( X_ISCHAR( ctx, 2 ), x_parc( ctx, 2 ), NULL ), x_parclen( ctx, 2 ), isDelimiter );
   tokens = x_tokenList( text, length, isDelimiter, ( size_t )x_parnintdef( ctx, 4 - shift, 0 ), &count );
   /* nTokenPos 0 or missing asks for the last token */
   if( wanted == 0 )
      wanted = ( int64_t )count;

   if( wanted >= 1 && ( size_t )wanted <= count )
   {
      x_tokenSepFind( text, length, isDelimiter, &tokens[wanted - 1] );
      x_retclen( ctx, text + tokens[wanted - 1].start, tokens[wanted - 1].length );
   }
   else
   {
      x_tokenSepFind( text, length, isDelimiter, NULL );
      x_retc( ctx, "" );
   }

   separatorLength = x_tokenSep( 0, &separator );
   x_storclen( ctx, separator, separatorLength, 5 - shift );
   separatorLength = x_tokenSep( 1, &separator );
   x_storclen( ctx, separator, separatorLength, 6 - shift );
   x_xfree( tokens );
}

/* TokenAt(): return the start and end position of a token */
void x_tokenTokenAt( X_CALL_CTX *ctx )
{
   size_t length, start, tokenLength;
   int owned;
   char *env = x_tokenEnvLoad( ctx, 3, &length, &owned );
   int64_t index;

   if( env == NULL )
   {
      x_retni( ctx, 0 );
      return;
   }

   /* The token last returned by TokenNext(), or token nCount; lNextDelimiter asks for the position after it */
   index = IIF( X_ISNUM( ctx, 2 ), x_parnint( ctx, 2 ) - 1, ( int64_t )x_tokenEnvNext( env ) - 1 );
   if( index >= 0 && x_tokenEnvToken( env, ( uint64_t )index, &start, &tokenLength ) != NULL )
      x_retnint( ctx, ( int64_t )( start + 1 + IIF( x_parl( ctx, 1 ), tokenLength, 0 ) ) );
   else
      x_retni( ctx, 0 );

   x_tokenEnvRelease( env, owned );
}

/* TokenEnd(): test if tokens can still be found with TokenNext() */
void x_tokenTokenEnd( X_CALL_CTX *ctx )
{
   size_t length;
   int owned;
   char *env = x_tokenEnvLoad( ctx, 1, &length, &owned );

   x_retl( ctx, env == NULL || x_tokenEnvNext( env ) >= x_tokenEnvCount( env ) );
   x_tokenEnvRelease( env, owned );
}

/* TokenInit(): initialize the environment for the incremental tokenizer */
void x_tokenTokenInit( X_CALL_CTX *ctx )
{
   unsigned char isDelimiter[256];
   size_t skip = ( size_t )x_parnintdef( ctx, 3, 0 );
   size_t length;
   int owned;
   char *env;

   /* Without cString TokenNext() starts again from the first token */
   if( !X_ISCHAR( ctx, 1 ) )
   {
      env = x_tokenEnvLoad( ctx, 4, &length, &owned );
      if( env == NULL )
      {
         x_retl( ctx, 0 );
         return;
      }

      x_tokenEnvSetNext( env, 0 );
      x_tokenEnvSave( ctx, 4, env, length, owned );
      x_retl( ctx, 1 );
      return;
   }

   /* The tokens are found once here; cString itself does not change */
   x_tokenDelimiters( IIF( X_ISCHAR( ctx, 2 ), x_parc( ctx, 2 ), NULL ), x_parclen( ctx, 2 ), isDelimiter );
   env = x_tokenEnvBuild( x_parc( ctx, 1 ), x_parclen( ctx, 1 ), isDelimiter, skip, &length );

   /* With @cTokenEnv the environment goes there, else it becomes the global one */
   if( X_ISBYREF( ctx, 4 ) )
   {
      x_storclen( ctx, env, length, 4 );
      x_xfree( env );
   }
   else
      x_tokenEnvSetGlobal( env, length );

   x_retl( ctx, 1 );
}

/* TokenLower(): change the first character of tokens to lower case */
void x_tokenTokenLower( X_CALL_CTX *ctx )
{
   /* A number as the second argument is already nTokenPos; without nTokenPos all tokens change */
   int shift = IIF( X_ISNUM( ctx, 2 ), 1, 0 );
   size_t length = x_parclen( ctx, 1 );
   char *result = ( char * )x_xmemdup( x_parc( ctx, 1 ), length + 1 );
   unsigned char isDelimiter[256];
   size_t skip = ( size_t )x_parnintdef( ctx, 4 - shift, 0 );
   int64_t count = x_parnintdef( ctx, 3 - shift, INT64_MAX );

   x_tokenDelimiters( IIF( X_ISCHAR( ctx, 2 ), x_parc( ctx, 2 ), NULL ), x_parclen( ctx, 2 ), isDelimiter );
   x_tokenCase( result, length, isDelimiter, skip, count, 0 );
   x_storclen( ctx, result, length, 1 );
   x_retclen_buffer( ctx, result, length );
}

/* TokenNext(): retrieve the next token from a string */
void x_tokenTokenNext( X_CALL_CTX *ctx )
{
   size_t length, start, tokenLength;
   int owned;
   char *env = x_tokenEnvLoad( ctx, 3, &length, &owned );
   uint64_t index;
   const char *token;

   if( env == NULL )
   {
      x_retc( ctx, "" );
      return;
   }

   /* The next token, or token nTokenPos; the text comes from the copy that TokenInit() made */
   if( X_ISNUM( ctx, 2 ) )
      index = IIF( x_parnint( ctx, 2 ) < 1, x_tokenEnvCount( env ), ( uint64_t )x_parnint( ctx, 2 ) - 1 );
   else
      index = x_tokenEnvNext( env );

   token = x_tokenEnvToken( env, index, &start, &tokenLength );
   if( token != NULL )
   {
      x_retclen( ctx, token, tokenLength );
      x_tokenEnvSetNext( env, index + 1 );
   }
   else
   {
      x_retc( ctx, "" );
      x_tokenEnvSetNext( env, x_tokenEnvCount( env ) );
   }

   x_tokenEnvSave( ctx, 3, env, length, owned );
}

/* TokenUpper(): change the first character of tokens to upper case */
void x_tokenTokenUpper( X_CALL_CTX *ctx )
{
   /* A number as the second argument is already nTokenPos; without nTokenPos all tokens change */
   int shift = IIF( X_ISNUM( ctx, 2 ), 1, 0 );
   size_t length = x_parclen( ctx, 1 );
   char *result = ( char * )x_xmemdup( x_parc( ctx, 1 ), length + 1 );
   unsigned char isDelimiter[256];
   size_t skip = ( size_t )x_parnintdef( ctx, 4 - shift, 0 );
   int64_t count = x_parnintdef( ctx, 3 - shift, INT64_MAX );

   x_tokenDelimiters( IIF( X_ISCHAR( ctx, 2 ), x_parc( ctx, 2 ), NULL ), x_parclen( ctx, 2 ), isDelimiter );
   x_tokenCase( result, length, isDelimiter, skip, count, 1 );
   x_storclen( ctx, result, length, 1 );
   x_retclen_buffer( ctx, result, length );
}
