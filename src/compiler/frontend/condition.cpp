/*
 * Copyright (c) 2024-2026 xDev-Language
 *
 * Library: xDev Frontend
 * File: condition.cpp
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "internal.h"

#include "internal.hpp"

#include <cerrno>
#include <cctype>
#include <cstdint>
#include <cstdlib>
#include <limits>

struct PP_OPERATOR
{
   const char *text;
   int precedence;
};

/* Binary operators of #if, from the weakest */
static const PP_OPERATOR s_operators[] = {
   { ".OR.",  1 },
   { ".AND.", 2 },
   { "=",     3 },
   { "==",    3 },
   { "!=",    3 },
   { "<>",    3 },
   { "#",     3 },
   { "<",     4 },
   { "<=",    4 },
   { ">",     4 },
   { ">=",    4 },
   { "+",     5 },
   { "-",     5 },
   { "*",     6 },
   { "/",     6 },
   { "%",     6 }
};

/* ---------------------------------------------------------------------------------------------
   Operators
   --------------------------------------------------------------------------------------------- */

/* 0 when op is not a binary operator */
static int s_precedence( const std::string &op )
{
   for( const PP_OPERATOR &entry : s_operators )
   {
      if( op == entry.text )
         return entry.precedence;
   }

   return 0;
}

/* Integer overflow and division by zero are errors */
static int64_t s_binary( const std::string &op, int64_t value, int64_t right, std::string &error )
{
   int64_t result = 0;

   if( op == ".AND." )
      return value && right;

   if( op == ".OR." )
      return value || right;

   if( op == "=" || op == "==" )
      return value == right;

   if( op == "!=" || op == "<>" || op == "#" )
      return value != right;

   if( op == "<" )
      return value < right;

   if( op == "<=" )
      return value <= right;

   if( op == ">" )
      return value > right;

   if( op == ">=" )
      return value >= right;

   if( op == "+" || op == "-" || op == "*" )
   {
      bool overflow;

      if( op == "+" )
         overflow = __builtin_add_overflow( value, right, &result );
      else if( op == "-" )
         overflow = __builtin_sub_overflow( value, right, &result );
      else
         overflow = __builtin_mul_overflow( value, right, &result );

      if( overflow )
         error = "przepelnienie w #if";

      return result;
   }

   if( right == 0 )
   {
      error = "dzielenie przez zero w #if";
      return value;
   }

   if( value == std::numeric_limits<int64_t>::min() && right == -1 )
   {
      error = "przepelnienie w #if";
      return value;
   }

   if( op == "/" )
      return value / right;

   return value % right;
}

/* ---------------------------------------------------------------------------------------------
   Expression
   --------------------------------------------------------------------------------------------- */

namespace
{

/* #if expression of integer and logical constants; a name that is not defined is 0 */
class PP_CONDITION_PARSER
{
private:
   const PP_TOKENS &m_tokens;
   std::string &m_error;
   size_t m_position = 0;
   size_t m_depth = 0;

   std::string current() const
   {
      std::string text;

      if( m_position < m_tokens.size() )
         text = m_tokens[m_position].text;

      for( char &ch : text )
         ch = ( char )std::toupper( ( unsigned char )ch );

      return text;
   }

   /* A number, .T., .F. or a name */
   int64_t constant( const std::string &word, const PP_TOKEN &token )
   {
      int64_t value;
      char *end;

      if( word == ".T." )
         return 1;

      if( word == ".F." || token.identifier )
         return 0;

      errno = 0;
      value = std::strtoll( token.text.c_str(), &end, IIF( word.compare( 0, 2, "0X" ) == 0, 16, 10 ) );

      if( end == token.text.c_str() || *end != '\0' || errno == ERANGE )
         m_error = "#if wymaga stalej calkowitej lub logicznej: " + token.text;

      return value;
   }

   /* A unary operator, ( expression ) or a constant */
   int64_t operand( bool evaluate )
   {
      const std::string word = current();
      int64_t value;

      if( word.empty() )
      {
         m_error = "brak argumentu w #if";
         return 0;
      }

      if( word == "+" || word == "-" || word == "!" || word == ".NOT." )
      {
         ++m_position;
         value = expression( IIF( word == ".NOT.", 3, 7 ), evaluate );

         if( word == "!" || word == ".NOT." )
            return !value;

         if( word != "-" || !evaluate )
            return value;

         if( value == std::numeric_limits<int64_t>::min() )
         {
            m_error = "przepelnienie w #if";
            return value;
         }

         return -value;
      }

      if( word == "(" )
      {
         ++m_position;
         value = expression( 1, evaluate );

         if( current() != ")" )
            m_error = "brak ) w #if";
         else
            ++m_position;

         return value;
      }

      return constant( word, m_tokens[m_position++] );
   }

   /* Precedence climbing; the side of .AND. / .OR. that is not needed is parsed, not evaluated */
   int64_t expression( int minimum, bool evaluate )
   {
      int64_t value;

      if( ++m_depth > 128 )
      {
         m_error = "zbyt glebokie wyrazenie #if";
         return 0;
      }

      value = operand( evaluate );

      while( m_error.empty() )
      {
         const std::string op = current();
         const int level = s_precedence( op );
         bool skip;
         int64_t right;

         if( level < minimum )
            break;

         ++m_position;
         skip = ( op == ".AND." && !value ) || ( op == ".OR." && value );
         right = expression( level + 1, evaluate && !skip );

         if( evaluate && m_error.empty() )
            value = s_binary( op, value, right, m_error );
      }

      --m_depth;

      return value;
   }

public:
   PP_CONDITION_PARSER( const PP_TOKENS &tokens, std::string &error ) : m_tokens( tokens ), m_error( error )
   {
   }

   bool run( bool &value )
   {
      value = expression( 1, true ) != 0;

      if( m_error.empty() && m_position != m_tokens.size() )
         m_error = "nieprawidlowe wyrazenie #if";

      return m_error.empty();
   }
};

} // namespace

bool x_ppCondition( const PP_TOKENS &tokens, bool &value, std::string &error )
{
   PP_CONDITION_PARSER parser( tokens, error );

   return parser.run( value );
}
