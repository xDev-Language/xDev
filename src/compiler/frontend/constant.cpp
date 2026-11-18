/*
 * Copyright (c) 2024-2026 xDev-Language
 *
 * Library: xDev Frontend
 * File: constant.cpp
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "constant.hpp"
#include "operators.h"
#include "statements.h"

#include <algorithm>
#include <cmath>
#include <memory>

using X_OPERATOR_FUNC = int ( * )( X_CALL_CTX *, X_ITEM *, const X_ITEM *, const X_ITEM * );

struct X_CONST_OPERATOR
{
   const char *text;
   X_OPERATOR_FUNC function;
};

/* x_foldConstant(): the job run under x_gcCatch() */
struct X_FOLD_JOB
{
   const X_EXPR *expr;
   X_ITEM *result;
   bool valid;
};

/* Constants are evaluated with the same C operators as the target runtime */
static const X_CONST_OPERATOR s_operators[] = {
   { "+",  x_itemAdd      },
   { "-",  x_itemSub      },
   { "*",  x_itemMul      },
   { "/",  x_itemDiv      },
   { "%",  x_itemMod      },
   { "^",  x_itemPow      },
   { "$",  x_itemContains },
   { "=",  x_itemEqS      },
   { "==", x_itemEq       },
   { "!=", x_itemNe       },
   { "<",  x_itemLt       },
   { "<=", x_itemLe       },
   { ">",  x_itemGt       },
   { ">=", x_itemGe       }
};

static bool s_staticConstant( const X_EXPR &expr, X_ITEM *result );

/* ---------------------------------------------------------------------------------------------
   Literals
   --------------------------------------------------------------------------------------------- */

static bool s_constantArray( const X_EXPR &expr, X_ITEM *result )
{
   if( !x_arrayNew( result, expr.args.size() ) )
      return false;

   for( size_t i = 0; i < expr.args.size(); ++i )
   {
      if( !s_staticConstant( *expr.args[i], x_arrayGetItemPtr( result, i + 1 ) ) )
         return false;
   }

   return true;
}

/* The same x_hashNew() / x_hashAdd() as Hash() at runtime: the last duplicate key wins */
static bool s_constantHash( const X_EXPR &expr, X_ITEM *result )
{
   if( !x_hashNew( result ) )
      return false;

   for( size_t i = 0; i < expr.args.size(); i += 2 )
   {
      X_OWNED_ITEM key;
      X_OWNED_ITEM value;

      if( !s_staticConstant( *expr.args[i], &key.value ) || !s_staticConstant( *expr.args[i + 1], &value.value ) )
         return false;

      if( !x_hashAdd( result, &key.value, &value.value ) )
         return false;
   }

   return true;
}

static bool s_constantNumber( const X_EXPR &expr, X_ITEM *result )
{
   if( !std::isfinite( expr.number ) )
      return false;

   if( expr.numberIsInteger )
   {
      result->nType = X_IT_INTEGER;
      result->u.integer = expr.integer;
   }
   else
   {
      result->nType = X_IT_DOUBLE;
      result->u.dbl = expr.number;
   }

   return true;
}

/* ---------------------------------------------------------------------------------------------
   Operators
   --------------------------------------------------------------------------------------------- */

static X_OPERATOR_FUNC s_binaryOperator( const std::string &text )
{
   for( const X_CONST_OPERATOR &op : s_operators )
   {
      if( text == op.text )
         return op.function;
   }

   return nullptr;
}

/* Signed overflow is rejected instead of invoking C undefined behaviour */
static bool s_integerOverflow( const std::string &op, int64_t left, int64_t right )
{
   int64_t ignored;

   if( op == "+" )
      return __builtin_add_overflow( left, right, &ignored );

   if( op == "-" )
      return __builtin_sub_overflow( left, right, &ignored );

   if( op == "*" )
      return __builtin_mul_overflow( left, right, &ignored );

   return op == "%" && left == INT64_MIN && right == -1;
}

static bool s_constantUnary( const X_EXPR &expr, const X_ITEM *value, X_ITEM *result )
{
   X_CALL_CTX ctx = {};

   if( expr.text == "+" )
   {
      x_itemCopy( result, value );
      return true;
   }

   if( expr.text == "-" )
   {
      if( value->nType == X_IT_INTEGER && value->u.integer == INT64_MIN )
         return false;

      return x_itemNeg( &ctx, result, value ) != 0;
   }

   return expr.text == ".NOT." && x_itemNot( &ctx, result, value );
}

/* .AND. and .OR. skip the right operand when the left one decides, as at runtime */
static bool s_constantLogical( const X_EXPR &expr, const X_ITEM *left, X_ITEM *result )
{
   X_OWNED_ITEM right;

   if( left->nType != X_IT_LOGICAL )
      return false;

   if( ( expr.text == ".AND." && !left->u.logical ) || ( expr.text == ".OR." && left->u.logical ) )
   {
      x_itemCopy( result, left );
      return true;
   }

   if( !s_staticConstant( *expr.args[1], &right.value ) || right.value.nType != X_IT_LOGICAL )
      return false;

   x_itemCopy( result, &right.value );

   return true;
}

static bool s_constantBinary( const X_EXPR &expr, const X_ITEM *left, X_ITEM *result )
{
   X_CALL_CTX ctx = {};
   X_OWNED_ITEM right;
   X_OPERATOR_FUNC op;

   if( !s_staticConstant( *expr.args[1], &right.value ) )
      return false;

   if( left->nType == X_IT_INTEGER && right.value.nType == X_IT_INTEGER )
   {
      if( s_integerOverflow( expr.text, left->u.integer, right.value.u.integer ) )
         return false;
   }

   op = s_binaryOperator( expr.text );
   if( !op || !op( &ctx, result, left, &right.value ) )
      return false;

   return result->nType != X_IT_DOUBLE || std::isfinite( result->u.dbl );
}

/* ---------------------------------------------------------------------------------------------
   Constant expression
   --------------------------------------------------------------------------------------------- */

/* Compile-time evaluation, never a call to user PRG code */
static bool s_staticConstant( const X_EXPR &expr, X_ITEM *result )
{
   X_OWNED_ITEM left;

   if( expr.kind == X_EXPR_KIND::ARRAY )
      return s_constantArray( expr, result );

   if( expr.kind == X_EXPR_KIND::CALL && expr.hashLiteral )
      return s_constantHash( expr, result );

   if( expr.kind == X_EXPR_KIND::NIL )
      return true;

   if( expr.kind == X_EXPR_KIND::LOGICAL )
   {
      result->nType = X_IT_LOGICAL;
      result->u.logical = expr.logical;
      return true;
   }

   if( expr.kind == X_EXPR_KIND::STRING )
   {
      x_itemPutCL( result, expr.text.c_str(), expr.text.size() );
      return true;
   }

   if( expr.kind == X_EXPR_KIND::NUMBER )
      return s_constantNumber( expr, result );

   if( expr.kind != X_EXPR_KIND::UNARY && expr.kind != X_EXPR_KIND::BINARY )
      return false;

   if( !s_staticConstant( *expr.args[0], &left.value ) )
      return false;

   if( expr.kind == X_EXPR_KIND::UNARY )
      return s_constantUnary( expr, &left.value, result );

   if( expr.text == ".AND." || expr.text == ".OR." )
      return s_constantLogical( expr, &left.value, result );

   return s_constantBinary( expr, &left.value, result );
}

static X_ERROR_ACTION s_propagateError( const X_ERROR *, X_ITEM * )
{
   return X_ERROR_PROPAGATE;
}

static void s_foldJob( void *cargo )
{
   X_FOLD_JOB *job = static_cast<X_FOLD_JOB *>( cargo );

   job->valid = s_staticConstant( *job->expr, job->result );
}

/* A runtime error during folding means: not a constant */
bool x_foldConstant( const X_EXPR &expr, X_ITEM *result )
{
   X_FOLD_JOB job = { &expr, result, false };
   X_ERROR_HANDLER saved = x_errorSetHandler( s_propagateError );
   bool valid;

   x_gcCatch( s_foldJob, &job );

   valid = job.valid && !x_errorCaught();
   x_errorClear();
   x_errorSetHandler( saved );

   return valid;
}

/* ---------------------------------------------------------------------------------------------
   STATIC validation
   --------------------------------------------------------------------------------------------- */

static bool s_fail( std::string &error, const X_LOCATION &location, int line, const std::string &text )
{
   if( location )
      error = location( line ) + ": " + text;
   else
      error = "linia " + std::to_string( line ) + ": " + text;

   return false;
}

/* Literals, operators and CAST only; native is set when a native type takes part */
static bool s_nativeConstant( const X_EXPR &expr, bool &native )
{
   switch( expr.kind )
   {
   case X_EXPR_KIND::NIL:
   case X_EXPR_KIND::NUMBER:
   case X_EXPR_KIND::STRING:
   case X_EXPR_KIND::LOGICAL:
   case X_EXPR_KIND::ARRAY:
   case X_EXPR_KIND::UNARY:
   case X_EXPR_KIND::BINARY:
   case X_EXPR_KIND::CAST: break;
   default:                return false;
   }

   native |= expr.nativeType || expr.kind == X_EXPR_KIND::CAST;

   for( const X_EXPR_PTR &arg : expr.args )
   {
      if( !s_nativeConstant( *arg, native ) )
         return false;
   }

   return true;
}

/* Every dimension is a constant; after a zero dimension the length is not checked */
static bool s_validateDimensions( const X_LOCAL_DECL &decl, std::string &error, const X_LOCATION &location )
{
   bool active = true;

   for( const X_EXPR_PTR &dim : decl.dimensions )
   {
      X_OWNED_ITEM value;
      int64_t length;

      if( !x_foldConstant( *dim, &value.value ) )
         return s_fail( error, location, dim->line, "wymiar STATIC musi byc poprawna stala: " + decl.name );

      if( !active )
         continue;

      if( !x_arrayDimensionLength( &value.value, &length ) )
         return s_fail( error, location, dim->line, "nieprawidlowy wymiar STATIC: " + decl.name );

      active = length != 0;
   }

   return true;
}

/* A native STATIC is initialized by the backend; any other initializer must fold */
static bool s_validateInit( const X_BINDING &symbol, std::string &error, const X_LOCATION &location )
{
   const X_LOCAL_DECL &decl = *symbol.declaration;
   const bool nativeStorage = symbol.type >= X_DECL_TYPE::INT8 && symbol.type <= X_DECL_TYPE::STRUCT;
   bool native = false;
   X_OWNED_ITEM value;

   if( !decl.init || nativeStorage )
      return true;

   if( s_nativeConstant( *decl.init, native ) && native )
      return true;

   if( !x_foldConstant( *decl.init, &value.value ) )
      return s_fail( error, location, decl.line, "nieprawidlowy staly inicjalizator STATIC: " + decl.name );

   return true;
}

static bool s_validateScope( const X_SCOPE &scope, std::string &error, const X_LOCATION &location )
{
   for( const auto &entry : scope.symbols )
   {
      const X_BINDING &symbol = entry.second;

      if( symbol.kind != X_BINDING_KIND::STATIC || !symbol.declaration )
         continue;

      if( !s_validateDimensions( *symbol.declaration, error, location ) )
         return false;

      if( !s_validateInit( symbol, error, location ) )
         return false;
   }

   return true;
}

bool x_validateConstants( const X_PROGRAM &program, std::string &error, const X_LOCATION &location )
{
   if( !s_validateScope( program.scope, error, location ) )
      return false;

   for( const X_FUNCTION &function : program.functions )
   {
      if( !s_validateScope( function.scope, error, location ) )
         return false;
   }

   return true;
}
