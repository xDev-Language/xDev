/*
 * Copyright (c) 2024-2026 xDev-Language
 *
 * Library: xDev Frontend
 * File: lexer.h
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#ifndef LEXER_H
#define LEXER_H

#ifdef __cplusplus
extern "C"
{
#endif

typedef struct _X_TOKEN X_TOKEN;
typedef struct _X_TOKLIST X_TOKLIST;

typedef enum
{
   TOK_NUM,
   TOK_STR,
   TOK_IDENT,
   TOK_TYPE,

   TOK_NIL,
   TOK_TRUE,
   TOK_FALSE,

   TOK_FUNCTION,
   TOK_PROCEDURE,
   TOK_INIT,
   TOK_STATIC,
   TOK_RETURN,
   TOK_LOCAL,
   TOK_AS,
   TOK_REQUEST,
   TOK_ANNOUNCE,
   TOK_FIELD,
   TOK_IN,

   TOK_IF,
   TOK_ELSEIF,
   TOK_ELSE,
   TOK_ENDIF,

   TOK_DO,
   TOK_WITH,
   TOK_WHILE,
   TOK_ENDDO,
   TOK_CASE,
   TOK_OTHERWISE,
   TOK_ENDCASE,
   TOK_SWITCH,
   TOK_ENDSWITCH,
   TOK_END,

   TOK_FOR,
   TOK_TO,
   TOK_STEP,
   TOK_NEXT,

   TOK_EXIT,
   TOK_LOOP,

   TOK_LPAREN,
   TOK_RPAREN,
   TOK_LBRACKET,
   TOK_RBRACKET,
   TOK_LBRACE,
   TOK_RBRACE,
   TOK_COMMA,
   TOK_PIPE,

   TOK_PLUS,
   TOK_MINUS,
   TOK_MUL,
   TOK_DIV,
   TOK_MOD,
   TOK_POWER,
   TOK_INC,
   TOK_DEC,

   TOK_ASSIGN,
   TOK_EQ,
   TOK_EXACT_EQ,
   TOK_NE,
   TOK_LT,
   TOK_LE,
   TOK_GT,
   TOK_GE,

   TOK_ADD_ASSIGN,
   TOK_SUB_ASSIGN,
   TOK_MUL_ASSIGN,
   TOK_DIV_ASSIGN,
   TOK_MOD_ASSIGN,
   TOK_POWER_ASSIGN,

   TOK_AND,
   TOK_OR,
   TOK_NOT,

   TOK_CONTAINS,
   TOK_ALIAS,
   TOK_REFERENCE,
   TOK_SEND,
   TOK_ARROW,

   TOK_EOL,
   TOK_EOF,

   TOK_KEYWORD,
   TOK_OP
} X_TOKTYPE;

struct _X_TOKEN
{
   X_TOKTYPE type;
   char *value;
   int line;
};

struct _X_TOKLIST
{
   X_TOKEN *items;
   int count;
   int capacity;
   char *error;
   int errorLine;
};

X_TOKLIST x_lex( const char *src );
void x_toklistFree( X_TOKLIST *toklist );
int x_lexIsKeyword( X_TOKTYPE type );

#ifdef __cplusplus
}
#endif

#endif /* LEXER_H */
