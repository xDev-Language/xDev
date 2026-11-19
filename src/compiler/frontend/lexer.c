/*
 * Copyright (c) 2024-2026 xDev-Language
 *
 * Library: xDev Frontend
 * File: lexer.c
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "internal.h"

#include "lexer.h"

#include <ctype.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct _X_KEYWORD X_KEYWORD;
typedef struct _X_OPERATOR X_OPERATOR;

struct _X_KEYWORD
{
   const char *name;
   X_TOKTYPE type;
};

struct _X_OPERATOR
{
   const char *text;
   X_TOKTYPE type;
};

/* ---------------------------------------------------------------------------------------------
   Tables
   --------------------------------------------------------------------------------------------- */

static const X_KEYWORD s_keywords[] = {
   { "FUNCTION",  TOK_FUNCTION  },
   { "INIT",      TOK_INIT      },
   { "PROCEDURE", TOK_PROCEDURE },
   { "STATIC",    TOK_STATIC    },
   { "RETURN",    TOK_RETURN    },
   { "LOCAL",     TOK_LOCAL     },
   { "AS",        TOK_AS        },
   { "REQUEST",   TOK_REQUEST   },
   { "ANNOUNCE",  TOK_ANNOUNCE  },
   { "FIELD",     TOK_FIELD     },
   { "IN",        TOK_IN        },
   { "NIL",       TOK_NIL       },
   { "IF",        TOK_IF        },
   { "ELSEIF",    TOK_ELSEIF    },
   { "ELSE",      TOK_ELSE      },
   { "ENDIF",     TOK_ENDIF     },
   { "DO",        TOK_DO        },
   { "WITH",      TOK_WITH      },
   { "WHILE",     TOK_WHILE     },
   { "ENDDO",     TOK_ENDDO     },
   { "CASE",      TOK_CASE      },
   { "OTHERWISE", TOK_OTHERWISE },
   { "ENDCASE",   TOK_ENDCASE   },
   { "SWITCH",    TOK_SWITCH    },
   { "ENDSWITCH", TOK_ENDSWITCH },
   { "END",       TOK_END       },
   { "ENUM",      TOK_KEYWORD   },
   { "ENDENUM",   TOK_KEYWORD   },
   { "STRUCT",    TOK_KEYWORD   },
   { "ENDSTRUCT", TOK_KEYWORD   },
   { "FOR",       TOK_FOR       },
   { "TO",        TOK_TO        },
   { "STEP",      TOK_STEP      },
   { "NEXT",      TOK_NEXT      },
   { "EXIT",      TOK_EXIT      },
   { "LOOP",      TOK_LOOP      },
   { NULL,        TOK_KEYWORD   }
};

/* Type names are keywords only after AS */
static const X_KEYWORD s_types[] = {
   { "PTR",     TOK_TYPE },
   { "PSZ",     TOK_TYPE },
   { "INT8",    TOK_TYPE },
   { "INT16",   TOK_TYPE },
   { "INT32",   TOK_TYPE },
   { "INT64",   TOK_TYPE },
   { "UINT8",   TOK_TYPE },
   { "UINT16",  TOK_TYPE },
   { "UINT32",  TOK_TYPE },
   { "UINT64",  TOK_TYPE },
   { "INT",     TOK_TYPE },
   { "FLOAT32", TOK_TYPE },
   { "FLOAT64", TOK_TYPE },
   { "FLOAT",   TOK_TYPE },
   { "LOGIC",   TOK_TYPE },
   { "STRING",  TOK_TYPE },
   { "ANY",     TOK_TYPE },
   { NULL,      TOK_TYPE }
};

/* Words between dots: .AND. .OR. .NOT. and the logical constants */
static const X_KEYWORD s_dotted[] = {
   { "AND", TOK_AND     },
   { "OR",  TOK_OR      },
   { "NOT", TOK_NOT     },
   { "T",   TOK_TRUE    },
   { "F",   TOK_FALSE   },
   { NULL,  TOK_KEYWORD }
};

static const X_OPERATOR s_operators[] = {
   { ":=", TOK_ASSIGN       },
   { "+=", TOK_ADD_ASSIGN   },
   { "-=", TOK_SUB_ASSIGN   },
   { "*=", TOK_MUL_ASSIGN   },
   { "/=", TOK_DIV_ASSIGN   },
   { "%=", TOK_MOD_ASSIGN   },
   { "^=", TOK_POWER_ASSIGN },
   { "==", TOK_EXACT_EQ     },
   { "!=", TOK_NE           },
   { "<>", TOK_NE           },
   { "<=", TOK_LE           },
   { ">=", TOK_GE           },
   { "**", TOK_POWER        },
   { "++", TOK_INC          },
   { "--", TOK_DEC          },
   { "->", TOK_ALIAS        },
   { "=>", TOK_ARROW        },
   { NULL, TOK_OP           }
};

/* ---------------------------------------------------------------------------------------------
   Lookup
   --------------------------------------------------------------------------------------------- */

static int s_lookupKeyword( const X_KEYWORD *keywords, const char *word, X_TOKTYPE *outType )
{
   int i;

   for( i = 0; keywords[i].name != NULL; i++ )
   {
      if( strcmp( word, keywords[i].name ) == 0 )
      {
         *outType = keywords[i].type;
         return 1;
      }
   }

   return 0;
}

/* Two-character operator at text */
static int s_lookupOperator( const char *text, X_TOKTYPE *outType )
{
   int i;

   for( i = 0; s_operators[i].text != NULL; i++ )
   {
      if( strncmp( text, s_operators[i].text, 2 ) == 0 )
      {
         *outType = s_operators[i].type;
         return 1;
      }
   }

   return 0;
}

static char *s_dupUpper( const char *src, int len )
{
   char *out = ( char * )x_xgrab( len + 1 );
   int i;

   for( i = 0; i < len; i++ )
      out[i] = ( char )toupper( ( unsigned char )src[i] );

   out[len] = '\0';

   return out;
}

static char *s_dupRaw( const char *src, size_t len )
{
   char *out = ( char * )x_xgrab( len + 1 );

   memcpy( out, src, len );
   out[len] = '\0';

   return out;
}

/* ---------------------------------------------------------------------------------------------
   Token list
   --------------------------------------------------------------------------------------------- */

static void s_toklistInit( X_TOKLIST *toklist )
{
   toklist->capacity = 64;
   toklist->count = 0;
   toklist->items = ( X_TOKEN * )x_xgrab( sizeof( X_TOKEN ) * toklist->capacity );
   toklist->error = NULL;
   toklist->errorLine = 0;
}

/* Only the first error is kept */
static void s_toklistFail( X_TOKLIST *toklist, int line, const char *message )
{
   if( toklist->error == NULL )
   {
      toklist->error = s_dupRaw( message, strlen( message ) );
      toklist->errorLine = line;
   }
}

static void s_toklistAdd( X_TOKLIST *toklist, X_TOKTYPE type, char *value, int line )
{
   if( toklist->count >= toklist->capacity )
   {
      int newCapacity;
      X_TOKEN *newItems;

      if( toklist->capacity > INT_MAX / 2 || ( size_t )toklist->capacity > SIZE_MAX / ( 2 * sizeof( X_TOKEN ) ) )
      {
         x_xfree( value );
         s_toklistFail( toklist, line, "zbyt wiele tokenow" );
         return;
      }

      newCapacity = toklist->capacity * 2;
      newItems = ( X_TOKEN * )x_xgrab( sizeof( X_TOKEN ) * ( size_t )newCapacity );
      memcpy( newItems, toklist->items, sizeof( X_TOKEN ) * toklist->count );
      x_xfree( toklist->items );
      toklist->items = newItems;
      toklist->capacity = newCapacity;
   }

   toklist->items[toklist->count].type = type;
   toklist->items[toklist->count].value = value;
   toklist->items[toklist->count].line = line;
   toklist->count++;
}

/* A '[' after one of these tokens is an index, otherwise it opens a string */
static int s_tokenCanEndExpr( const X_TOKLIST *toklist )
{
   if( toklist->count <= 0 )
      return 0;

   switch( toklist->items[toklist->count - 1].type )
   {
   case TOK_IDENT:
   case TOK_NUM:
   case TOK_STR:
   case TOK_NIL:
   case TOK_TRUE:
   case TOK_FALSE:
   case TOK_RPAREN:
   case TOK_RBRACKET:
   case TOK_RBRACE: return 1;
   default:         return 0;
   }
}

/* ---------------------------------------------------------------------------------------------
   Strings
   --------------------------------------------------------------------------------------------- */

/* e"..." and e'...': the escapes \n, \t, \\ and the matching quote */
static int s_lexEString( X_TOKLIST *list, const char *src, int len, int i, int line )
{
   char quote = src[i + 1];
   char *text = ( char * )x_xgrab( ( size_t )( len - i ) );
   size_t textLen = 0;
   int j = i + 2;

   while( list->error == NULL && j < len && src[j] != quote && src[j] != '\n' && src[j] != '\r' )
   {
      char next;

      if( src[j] != '\\' )
      {
         text[textLen++] = src[j++];
         continue;
      }

      next = src[j + 1];

      if( next == 'n' )
         text[textLen++] = '\n';
      else if( next == 't' )
         text[textLen++] = '\t';
      else if( next == '\\' || next == quote )
         text[textLen++] = next;
      else
         s_toklistFail( list, line, "nieznana sekwencja w napisie e-string (dozwolone: \\n, \\t, cytat, \\\\)" );

      j += 2;
   }

   if( list->error == NULL && ( j >= len || src[j] != quote ) )
      s_toklistFail( list, line, "niedomkniety string" );

   if( list->error != NULL )
   {
      x_xfree( text );
      return j;
   }

   text[textLen] = '\0';
   s_toklistAdd( list, TOK_STR, text, line );

   return j + 1;
}

/* "...", '...' and [...]: no escapes, the string ends on the same line */
static int s_lexString( X_TOKLIST *list, const char *src, int len, int i, int line )
{
   char quote = IIF( src[i] == '[', ']', src[i] );
   int start = i + 1;
   int j = start;

   while( j < len && src[j] != quote )
   {
      if( src[j] == '\n' || src[j] == '\r' )
      {
         s_toklistFail( list, line, "niedomkniety string" );
         return j;
      }

      j++;
   }

   if( j == len )
   {
      s_toklistFail( list, line, "niedomkniety string" );
      return j;
   }

   s_toklistAdd( list, TOK_STR, s_dupRaw( src + start, j - start ), line );

   return j + 1;
}

/* ---------------------------------------------------------------------------------------------
   Numbers
   --------------------------------------------------------------------------------------------- */

/* 0x or 0X followed by a hex digit */
static int s_isHexPrefix( const char *src, int len, int i )
{
   if( src[i] != '0' || i + 2 >= len )
      return 0;

   if( src[i + 1] != 'x' && src[i + 1] != 'X' )
      return 0;

   return isxdigit( ( unsigned char )src[i + 2] ) != 0;
}

/* 123, 1.5, .5, 1e10, 1.5E-3 and 0xFF */
static int s_lexNumber( X_TOKLIST *list, const char *src, int len, int i, int line )
{
   int start = i;
   int hasDot = src[i] == '.';

   if( s_isHexPrefix( src, len, i ) )
   {
      i += 2;

      while( i < len && isxdigit( ( unsigned char )src[i] ) )
         ++i;

      s_toklistAdd( list, TOK_NUM, s_dupRaw( src + start, i - start ), line );

      return i;
   }

   if( hasDot )
      ++i;

   while( i < len && isdigit( ( unsigned char )src[i] ) )
      ++i;

   if( !hasDot && i < len && src[i] == '.' )
   {
      ++i;

      while( i < len && isdigit( ( unsigned char )src[i] ) )
         ++i;
   }

   if( i < len && src[i] == '.' )
      s_toklistFail( list, line, "nieprawidlowa liczba" );

   if( list->error == NULL && i < len && ( src[i] == 'e' || src[i] == 'E' ) )
   {
      ++i;

      if( i < len && ( src[i] == '+' || src[i] == '-' ) )
         ++i;

      if( i >= len || !isdigit( ( unsigned char )src[i] ) )
         s_toklistFail( list, line, "nieprawidlowy wykladnik liczby" );

      while( i < len && isdigit( ( unsigned char )src[i] ) )
         ++i;
   }

   if( list->error == NULL )
      s_toklistAdd( list, TOK_NUM, s_dupRaw( src + start, i - start ), line );

   return i;
}

/* ---------------------------------------------------------------------------------------------
   Words and operators
   --------------------------------------------------------------------------------------------- */

/* Length of .WORD. at src[i] in s_dotted, otherwise 0; right after a value only .AND. and .OR. count */
static int s_tryDotted( const X_TOKLIST *list, const char *src, int i, int len, X_TOKTYPE *outType )
{
   char word[16];
   int j = i + 1;
   int wordLen;
   int k;
   int afterValue;

   while( j < len && isalpha( ( unsigned char )src[j] ) )
      j++;

   if( j >= len || src[j] != '.' )
      return 0;

   wordLen = j - ( i + 1 );
   if( wordLen <= 0 || wordLen > 15 )
      return 0;

   for( k = 0; k < wordLen; k++ )
      word[k] = ( char )toupper( ( unsigned char )src[i + 1 + k] );

   word[wordLen] = '\0';

   if( !s_lookupKeyword( s_dotted, word, outType ) )
      return 0;

   afterValue = i > 0 && !isspace( ( unsigned char )src[i - 1] ) && s_tokenCanEndExpr( list );
   if( afterValue && *outType != TOK_AND && *outType != TOK_OR )
      return 0;

   return j - i + 1;
}

/* Upper-case word: a type name after AS, a keyword or a name */
static X_TOKTYPE s_wordType( const X_TOKLIST *list, const char *word )
{
   X_TOKTYPE type;
   int afterAs = list->count > 0 && list->items[list->count - 1].type == TOK_AS;

   if( afterAs && s_lookupKeyword( s_types, word, &type ) )
      return type;

   if( s_lookupKeyword( s_keywords, word, &type ) )
      return type;

   return TOK_IDENT;
}

static int s_lexWord( X_TOKLIST *list, const char *src, int len, int i, int line )
{
   int start = i;
   char *upper;
   X_TOKTYPE type;

   while( i < len && ( isalnum( ( unsigned char )src[i] ) || src[i] == '_' ) )
      i++;

   upper = s_dupUpper( src + start, i - start );
   type = s_wordType( list, upper );
   x_xfree( upper );

   s_toklistAdd( list, type, s_dupRaw( src + start, i - start ), line );

   return i;
}

/* One-character operators and punctuation; '@' is the reference */
static X_TOKTYPE s_charType( char c )
{
   switch( c )
   {
   case '+':  return TOK_PLUS;
   case '-':  return TOK_MINUS;
   case '*':  return TOK_MUL;
   case '/':  return TOK_DIV;
   case '%':  return TOK_MOD;
   case '^':  return TOK_POWER;
   case '(':  return TOK_LPAREN;
   case ')':  return TOK_RPAREN;
   case '=':  return TOK_EQ;
   case ',':  return TOK_COMMA;
   case '<':  return TOK_LT;
   case '>':  return TOK_GT;
   case '$':  return TOK_CONTAINS;
   case '#':  return TOK_NE;
   case '!':  return TOK_NOT;
   case ':':  return TOK_SEND;
   case '[':  return TOK_LBRACKET;
   case ']':  return TOK_RBRACKET;
   case '{':  return TOK_LBRACE;
   case '}':  return TOK_RBRACE;
   case '|':  return TOK_PIPE;
   case '.':
   case '\\': return TOK_OP;
   }

   return TOK_REFERENCE;
}

/* ---------------------------------------------------------------------------------------------
   Lexer
   --------------------------------------------------------------------------------------------- */

X_TOKLIST x_lex( const char *src )
{
   X_TOKLIST list;
   size_t sourceLen = strlen( src );
   int len;
   int i = 0;
   int line = 1;

   s_toklistInit( &list );

   if( sourceLen >= INT_MAX )
   {
      s_toklistFail( &list, line, "plik zrodlowy jest zbyt duzy" );
      return list;
   }

   len = ( int )sourceLen;

   /* Comments never reach the lexer: the preprocessor removes them */
   while( i < len && list.error == NULL )
   {
      char c = src[i];
      X_TOKTYPE type;
      int matchLen;
      char message[64];

      if( c == ' ' || c == '\t' )
      {
         i++;
         continue;
      }

      /* ; at the end of a line continues the statement, otherwise it ends the statement */
      if( c == ';' )
      {
         int j = i + 1;

         while( j < len && ( src[j] == ' ' || src[j] == '\t' ) )
            j++;

         if( j < len && ( src[j] == '\n' || src[j] == '\r' ) )
         {
            if( src[j] == '\r' && j + 1 < len && src[j + 1] == '\n' )
               ++j;

            i = j + 1;
            line++;
            continue;
         }

         s_toklistAdd( &list, TOK_EOL, s_dupRaw( "", 0 ), line );
         i++;
         continue;
      }

      /* End of line: \n, \r or \r\n */
      if( c == '\n' || c == '\r' )
      {
         s_toklistAdd( &list, TOK_EOL, s_dupRaw( "", 0 ), line );
         line++;
         i++;

         if( c == '\r' && i < len && src[i] == '\n' )
            ++i;

         continue;
      }

      if( ( c == 'e' || c == 'E' ) && i + 1 < len && ( src[i + 1] == '"' || src[i + 1] == '\'' ) )
      {
         i = s_lexEString( &list, src, len, i, line );
         continue;
      }

      if( c == '"' || c == '\'' || ( c == '[' && !s_tokenCanEndExpr( &list ) ) )
      {
         i = s_lexString( &list, src, len, i, line );
         continue;
      }

      if( isdigit( ( unsigned char )c ) || ( c == '.' && i + 1 < len && isdigit( ( unsigned char )src[i + 1] ) ) )
      {
         i = s_lexNumber( &list, src, len, i, line );
         continue;
      }

      /* Dotted logical operators and constants; all other dots can mark STRUCT fields */
      matchLen = IIF( c == '.', s_tryDotted( &list, src, i, len, &type ), 0 );
      if( matchLen > 0 )
      {
         s_toklistAdd( &list, type, s_dupRaw( src + i, matchLen ), line );
         i += matchLen;
         continue;
      }

      if( isalpha( ( unsigned char )c ) || c == '_' )
      {
         i = s_lexWord( &list, src, len, i, line );
         continue;
      }

      if( i + 1 < len && s_lookupOperator( src + i, &type ) )
      {
         s_toklistAdd( &list, type, s_dupRaw( src + i, 2 ), line );
         i += 2;
         continue;
      }

      if( strchr( "+-*/%^()=,<>[]{}@$!#:|.\\", c ) != NULL )
      {
         s_toklistAdd( &list, s_charType( c ), s_dupRaw( &c, 1 ), line );
         i++;
         continue;
      }

      snprintf( message, sizeof( message ), "nieznany znak: '%c'", c );
      s_toklistFail( &list, line, message );
   }

   s_toklistAdd( &list, TOK_EOF, s_dupRaw( "", 0 ), line );

   return list;
}

/* ---------------------------------------------------------------------------------------------
   Keywords
   --------------------------------------------------------------------------------------------- */

/* Return 1 when type is the token type of a word in s_keywords */
int x_lexIsKeyword( X_TOKTYPE type )
{
   const X_KEYWORD *keyword;

   for( keyword = s_keywords; keyword->name != NULL; keyword++ )
   {
      if( keyword->type == type )
         return 1;
   }

   return 0;
}

/* ---------------------------------------------------------------------------------------------
   Token list cleanup
   --------------------------------------------------------------------------------------------- */

void x_toklistFree( X_TOKLIST *toklist )
{
   int i;

   for( i = 0; i < toklist->count; i++ )
      x_xfree( toklist->items[i].value );

   x_xfree( toklist->items );
   x_xfree( toklist->error );
   toklist->items = NULL;
   toklist->count = 0;
   toklist->capacity = 0;
   toklist->error = NULL;
   toklist->errorLine = 0;
}
