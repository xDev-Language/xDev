/*
 * Copyright (c) 2024-2026 xDev-Language
 *
 * Library...: xDev Frontend
 * File......: parser.cpp
 *
 */

#include "internal.h"
#include "classes.h"

#include "parser.hpp"

#include <cctype>
#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <unordered_set>

struct X_TYPE_NAME
{
   const char *name;
   X_DECL_TYPE type;
};

/* Simple types after AS; INT and FLOAT are INT64 and FLOAT64 */
static const X_TYPE_NAME s_simpleTypes[] = {
   { "ANY",     X_DECL_TYPE::ANY     },
   { "INT8",    X_DECL_TYPE::INT8    },
   { "INT16",   X_DECL_TYPE::INT16   },
   { "INT32",   X_DECL_TYPE::INT32   },
   { "INT64",   X_DECL_TYPE::INT64   },
   { "UINT8",   X_DECL_TYPE::UINT8   },
   { "UINT16",  X_DECL_TYPE::UINT16  },
   { "UINT32",  X_DECL_TYPE::UINT32  },
   { "UINT64",  X_DECL_TYPE::UINT64  },
   { "FLOAT32", X_DECL_TYPE::FLOAT32 },
   { "FLOAT64", X_DECL_TYPE::FLOAT64 },
   { "LOGIC",   X_DECL_TYPE::LOGIC   },
   { "STRING",  X_DECL_TYPE::STRING  },
   { "PSZ",     X_DECL_TYPE::PSZ     },
   { "PTR",     X_DECL_TYPE::PTR     },
   { "INT",     X_DECL_TYPE::INT64   },
   { "FLOAT",   X_DECL_TYPE::FLOAT64 },
   { nullptr,   X_DECL_TYPE::ANY     }
};

/* xDev value types written as names after AS */
static const X_TYPE_NAME s_valueTypes[] = {
   { "ARRAY",     X_DECL_TYPE::ARRAY     },
   { "CODEBLOCK", X_DECL_TYPE::CODEBLOCK },
   { "OBJECT",    X_DECL_TYPE::OBJECT    },
   { "DATE",      X_DECL_TYPE::DATE      },
   { "HASH",      X_DECL_TYPE::HASH      },
   { nullptr,     X_DECL_TYPE::ANY       }
};

static std::string s_upper( const char *text )
{
   std::string out( text );

   for( char &c : out )
      c = ( char )std::toupper( ( unsigned char )c );

   return out;
}

static bool s_findType( const X_TYPE_NAME *table, const std::string &name, X_DECL_TYPE &type )
{
   for( ; table->name != nullptr; ++table )
   {
      if( name == table->name )
      {
         type = table->type;
         return true;
      }
   }

   return false;
}

/* Binding strength of a binary operator; 0 is not a binary operator */
static int s_precedence( X_TOKTYPE type )
{
   switch( type )
   {
   case TOK_OR:       return 1;
   case TOK_AND:      return 2;
   case TOK_EQ:
   case TOK_EXACT_EQ:
   case TOK_NE:
   case TOK_LT:
   case TOK_LE:
   case TOK_GT:
   case TOK_GE:
   case TOK_CONTAINS: return 3;
   case TOK_PLUS:
   case TOK_MINUS:    return 4;
   case TOK_MUL:
   case TOK_DIV:
   case TOK_MOD:      return 5;
   case TOK_POWER:    return 6;
   default:           return 0;
   }
}

static bool s_isRightAssociative( X_TOKTYPE type )
{
   return type == TOK_POWER;
}

/* Operator text stored in the AST */
static const char *s_operatorName( X_TOKTYPE type )
{
   switch( type )
   {
   case TOK_PLUS:         return "+";
   case TOK_MINUS:        return "-";
   case TOK_MUL:          return "*";
   case TOK_DIV:          return "/";
   case TOK_MOD:          return "%";
   case TOK_POWER:        return "^";
   case TOK_EQ:           return "=";
   case TOK_EXACT_EQ:     return "==";
   case TOK_NE:           return "!=";
   case TOK_LT:           return "<";
   case TOK_LE:           return "<=";
   case TOK_GT:           return ">";
   case TOK_GE:           return ">=";
   case TOK_CONTAINS:     return "$";
   case TOK_AND:          return ".AND.";
   case TOK_OR:           return ".OR.";
   case TOK_NOT:          return ".NOT.";
   case TOK_INC:          return "++";
   case TOK_DEC:          return "--";
   case TOK_ADD_ASSIGN:   return "+=";
   case TOK_SUB_ASSIGN:   return "-=";
   case TOK_MUL_ASSIGN:   return "*=";
   case TOK_DIV_ASSIGN:   return "/=";
   case TOK_MOD_ASSIGN:   return "%=";
   case TOK_POWER_ASSIGN: return "^=";
   case TOK_ALIAS:        return "->";
   case TOK_SEND:         return ":";
   default:               return nullptr;
   }
}

static X_EXPR_PTR s_makeExpr( X_EXPR_KIND kind )
{
   X_EXPR_PTR expr = std::make_unique<X_EXPR>();

   expr->kind = kind;

   return expr;
}

/* obj:name without parentheses: a property that can be assigned */
static bool s_isProperty( const X_EXPR *expr )
{
   return expr->kind == X_EXPR_KIND::SEND && !expr->sendHasParentheses && expr->args.size() == 1;
}

/* A variable, a field, an element or a property */
static bool s_isLvalue( const X_EXPR *expr )
{
   if( !expr )
      return false;

   if( expr->kind == X_EXPR_KIND::NAME || expr->kind == X_EXPR_KIND::FIELD || expr->kind == X_EXPR_KIND::INDEX )
      return true;

   return s_isProperty( expr );
}

struct X_ASSIGN_TOKEN
{
   X_TOKTYPE token;
   X_ASSIGN_OP op;
};

static const X_ASSIGN_TOKEN s_assignTokens[] = {
   { TOK_ASSIGN,       X_ASSIGN_OP::SET },
   { TOK_ADD_ASSIGN,   X_ASSIGN_OP::ADD },
   { TOK_SUB_ASSIGN,   X_ASSIGN_OP::SUB },
   { TOK_MUL_ASSIGN,   X_ASSIGN_OP::MUL },
   { TOK_DIV_ASSIGN,   X_ASSIGN_OP::DIV },
   { TOK_MOD_ASSIGN,   X_ASSIGN_OP::MOD },
   { TOK_POWER_ASSIGN, X_ASSIGN_OP::POW }
};

/* ---------------------------------------------------------------------------------------------
   Parser state
   --------------------------------------------------------------------------------------------- */

X_PARSER::X_PARSER( const X_TOKLIST &tokens ) : m_tokens( tokens )
{
}

bool X_PARSER::is( X_TOKTYPE type ) const
{
   return m_pos < m_tokens.count && m_tokens.items[m_pos].type == type;
}

/* Type of the token offset places ahead; TOK_EOF past the end */
X_TOKTYPE X_PARSER::peek( int offset ) const
{
   if( m_pos + offset >= m_tokens.count )
      return TOK_EOF;

   return m_tokens.items[m_pos + offset].type;
}

/* Text of the token offset places ahead; the caller knows that it exists */
const char *X_PARSER::value( int offset ) const
{
   return m_tokens.items[m_pos + offset].value;
}

/* A token that may serve as a member name; callers allow it only after : / :: and in METHOD / DATA */
bool X_PARSER::isMemberName() const
{
   if( m_pos >= m_tokens.count )
      return false;

   const X_TOKTYPE type = m_tokens.items[m_pos].type;

   return type == TOK_IDENT || x_lexIsKeyword( type );
}

bool X_PARSER::match( X_TOKTYPE type )
{
   if( !is( type ) )
      return false;

   ++m_pos;

   return true;
}

bool X_PARSER::at( const char *value ) const
{
   return m_pos < m_tokens.count && s_upper( m_tokens.items[m_pos].value ) == value;
}

bool X_PARSER::atBinaryOperatorToken() const
{
   return m_pos < m_tokens.count && s_precedence( m_tokens.items[m_pos].type ) != 0;
}

/* The first error of a statement is kept at the current token; recover() lets the next statement report again */
void X_PARSER::fail( const char *message )
{
   int line = 0;
   int column = 0;

   if( !m_error.empty() )
      return;

   if( m_pos < m_tokens.count )
   {
      line = m_tokens.items[m_pos].line;
      column = m_tokens.items[m_pos].column;
   }

   m_error = message;
   m_diagnostics.push_back( { line, column, message } );
}

const std::vector<X_DIAGNOSTIC> &X_PARSER::diagnostics() const
{
   return m_diagnostics;
}

/* FUNCTION, PROCEDURE, STATIC/INIT/EXIT FUNCTION or PROCEDURE and CREATE CLASS start a new routine */
bool X_PARSER::atRoutineStart() const
{
   std::string next;

   if( at( "FUNCTION" ) || at( "PROCEDURE" ) )
      return true;

   if( peek( 1 ) != TOK_EOF )
      next = s_upper( value( 1 ) );

   if( at( "CREATE" ) )
      return next == "CLASS";

   return ( at( "STATIC" ) || at( "INIT" ) || at( "EXIT" ) ) && ( next == "FUNCTION" || next == "PROCEDURE" );
}

/* After an error the parser goes on at the next line, but not past the start of a routine; start is the token
   where the failed part began, so the parser always moves on. false at the end or after too many errors */
bool X_PARSER::recover( int start )
{
   const size_t limit = 20;

   m_error.clear();

   if( m_pos == start && !is( TOK_EOF ) )
      ++m_pos;

   if( !atRoutineStart() )
   {
      while( !is( TOK_EOF ) && !is( TOK_EOL ) )
         ++m_pos;
   }

   skipLines();

   return m_diagnostics.size() < limit && !is( TOK_EOF );
}

void X_PARSER::addClassType( const std::string &name )
{
   m_classTypes.insert( s_upper( name.c_str() ) );
}

void X_PARSER::skipLines()
{
   while( is( TOK_EOL ) )
      ++m_pos;
}

bool X_PARSER::expect( const char *value )
{
   if( !at( value ) )
   {
      fail( ( std::string( "oczekiwano " ) + value ).c_str() );
      return false;
   }

   ++m_pos;

   return true;
}

/* STRUCT, class and CALLBACK names declared in this file */
bool X_PARSER::namedType( const std::string &name, X_DECL_TYPE &type ) const
{
   if( m_structTypes.count( name ) )
      type = X_DECL_TYPE::STRUCT;
   else if( m_classTypes.count( name ) )
      type = X_DECL_TYPE::OBJECT;
   else if( m_callbackTypes.count( name ) )
      type = X_DECL_TYPE::CALLBACK;
   else
      return false;

   return true;
}

/* The type after AS; m_pos is at AS */
bool X_PARSER::parseType( X_DECL_TYPE &type, std::string *typeName )
{
   std::string name;

   ++m_pos;

   if( typeName )
      typeName->clear();

   if( m_pos < m_tokens.count )
      name = s_upper( m_tokens.items[m_pos].value );

   if( is( TOK_IDENT ) && namedType( name, type ) )
   {
      if( typeName )
         *typeName = name;

      return acceptType();
   }

   if( is( TOK_IDENT ) && s_findType( s_valueTypes, name, type ) )
      return acceptType();

   if( !is( TOK_TYPE ) )
   {
      fail( "oczekiwano typu prostego po AS" );
      return false;
   }

   if( !s_findType( s_simpleTypes, name, type ) )
   {
      fail( "nieobslugiwany typ po AS" );
      return false;
   }

   return acceptType();
}

bool X_PARSER::acceptType()
{
   ++m_pos;
   m_hasTypeAnnotations = true;

   return true;
}

/* ---------------------------------------------------------------------------------------------
   Expressions
   --------------------------------------------------------------------------------------------- */

X_EXPR_PTR X_PARSER::makeUpdate( X_UPDATE_OP op, X_UPDATE_MODE mode, X_EXPR_PTR target )
{
   X_EXPR_PTR expr;

   if( !s_isLvalue( target.get() ) )
   {
      if( op == X_UPDATE_OP::INC )
         fail( "operator ++ wymaga lvalue" );
      else
         fail( "operator -- wymaga lvalue" );

      return nullptr;
   }

   expr = s_makeExpr( X_EXPR_KIND::UPDATE );
   expr->line = target->line;
   expr->updateOp = op;
   expr->updateMode = mode;
   expr->args.push_back( std::move( target ) );

   return expr;
}

X_EXPR_PTR X_PARSER::makeAssignment( X_ASSIGN_OP op, X_EXPR_PTR target, X_EXPR_PTR value )
{
   X_EXPR_PTR expr;

   /* obj:name := value is the message obj:_name( value ) */
   if( target && s_isProperty( target.get() ) && op == X_ASSIGN_OP::SET )
   {
      target->text.insert( target->text.begin(), '_' );
      target->args.push_back( std::move( value ) );
      target->argByReference.push_back( false );
      return target;
   }

   if( !s_isLvalue( target.get() ) )
   {
      if( op == X_ASSIGN_OP::SET )
         fail( "nieprawidlowa lewa strona przypisania" );
      else
         fail( "przypisanie zlozone wymaga zmiennej lub wlasciwosci" );

      return nullptr;
   }

   expr = s_makeExpr( X_EXPR_KIND::ASSIGN );
   expr->line = target->line;
   expr->assignOp = op;
   expr->args.push_back( std::move( target ) );
   expr->args.push_back( std::move( value ) );

   return expr;
}

/* A hash literal is the call Hash( xKey, xValue, ... ) */
static X_EXPR_PTR s_hashCall( int line )
{
   X_EXPR_PTR expr = s_makeExpr( X_EXPR_KIND::CALL );

   expr->line = line;
   expr->text = "Hash";
   expr->hashLiteral = true;

   return expr;
}

/* 0x...: an integer up to the INT64 maximum, a larger value is a double */
static void s_hexNumber( X_EXPR &expr, const char *digits )
{
   unsigned long long hex;

   errno = 0;
   hex = std::strtoull( digits, nullptr, 16 );
   expr.number = static_cast<double>( hex );
   expr.numberIsInteger = errno != ERANGE && hex <= 0x7FFFFFFFFFFFFFFFULL;

   if( expr.numberIsInteger )
      expr.integer = static_cast<int64_t>( hex );
}

/* X_ITEM tells INTEGER from DOUBLE: 7.0 stays DOUBLE although its value is integral */
static void s_decimalNumber( X_EXPR &expr, const char *literal )
{
   expr.number = std::strtod( literal, nullptr );
   expr.numberIsInteger = std::strpbrk( literal, ".eE" ) == nullptr;

   if( !expr.numberIsInteger )
      return;

   errno = 0;
   expr.integer = std::strtoll( literal, nullptr, 10 );

   if( errno == ERANGE )
      expr.numberIsInteger = false;
}

/* @Name() */
X_EXPR_PTR X_PARSER::parseFunctionAddress()
{
   X_EXPR_PTR expr = s_makeExpr( X_EXPR_KIND::FUNCTION_ADDRESS );

   expr->text = value( 1 );
   m_pos += 4;

   return expr;
}

X_EXPR_PTR X_PARSER::parseNumber()
{
   const char *literal = value();
   X_EXPR_PTR expr = s_makeExpr( X_EXPR_KIND::NUMBER );

   ++m_pos;
   expr->text = literal;

   if( literal[0] == '0' && ( literal[1] == 'x' || literal[1] == 'X' ) )
      s_hexNumber( *expr, literal + 2 );
   else
      s_decimalNumber( *expr, literal );

   expr->type = IIF( expr->numberIsInteger, X_DECL_TYPE::INT64, X_DECL_TYPE::FLOAT64 );

   return expr;
}

/* ( expr ) or ( a, b, c ): a group has the value of its last expression */
X_EXPR_PTR X_PARSER::parseGroup( int line )
{
   X_EXPR_PTR expr = parseExpression();

   if( expr && match( TOK_COMMA ) )
   {
      X_EXPR_PTR group = s_makeExpr( X_EXPR_KIND::GROUP );

      group->line = line;
      group->args.push_back( std::move( expr ) );

      do
      {
         X_EXPR_PTR item = parseExpression();

         if( !item )
            return nullptr;

         group->args.push_back( std::move( item ) );
      } while( match( TOK_COMMA ) );

      expr = std::move( group );
   }

   if( !expr )
      return nullptr;

   if( !match( TOK_RPAREN ) )
   {
      fail( "oczekiwano )" );
      return nullptr;
   }

   expr->parenthesized = true;

   return expr;
}

/* { key => value, ... }: key is the first key, already read */
X_EXPR_PTR X_PARSER::parseHash( X_EXPR_PTR key, int line )
{
   X_EXPR_PTR expr = s_hashCall( line );

   while( true )
   {
      X_EXPR_PTR item = parseExpression();

      if( !item )
         return nullptr;

      expr->args.push_back( std::move( key ) );
      expr->args.push_back( std::move( item ) );
      expr->argByReference.push_back( false );
      expr->argByReference.push_back( false );

      if( !match( TOK_COMMA ) )
         break;

      key = parseExpression();
      if( !key )
         return nullptr;

      if( !match( TOK_ARROW ) )
      {
         fail( "oczekiwano =>" );
         return nullptr;
      }
   }

   if( !match( TOK_RBRACE ) )
   {
      fail( "oczekiwano }" );
      return nullptr;
   }

   return expr;
}

/* { a, , c }: an empty element is NIL; first is the element already read, if any */
X_EXPR_PTR X_PARSER::parseArray( X_EXPR_PTR first )
{
   X_EXPR_PTR expr = s_makeExpr( X_EXPR_KIND::ARRAY );

   if( first || !is( TOK_RBRACE ) )
   {
      while( true )
      {
         X_EXPR_PTR element;

         if( first )
            element = std::move( first );
         else if( is( TOK_COMMA ) || is( TOK_RBRACE ) )
            element = s_makeExpr( X_EXPR_KIND::NIL );
         else
         {
            element = parseExpression();
            if( !element )
               return nullptr;
         }

         expr->args.push_back( std::move( element ) );

         if( !match( TOK_COMMA ) )
            break;
      }
   }

   if( !match( TOK_RBRACE ) )
   {
      fail( "oczekiwano }" );
      return nullptr;
   }

   return expr;
}

/* After {: a code block, a hash, an empty hash { => } or an array */
X_EXPR_PTR X_PARSER::parseBrace( int line )
{
   X_EXPR_PTR first;

   if( is( TOK_PIPE ) )
   {
      X_EXPR_PTR block = parseCodeBlock();

      if( block )
         block->line = line;

      return block;
   }

   if( is( TOK_ARROW ) && peek( 1 ) == TOK_RBRACE )
   {
      m_pos += 2;
      return s_hashCall( line );
   }

   if( !is( TOK_RBRACE ) && !is( TOK_COMMA ) )
   {
      first = parseExpression();
      if( !first )
         return nullptr;

      if( match( TOK_ARROW ) )
         return parseHash( std::move( first ), line );
   }

   return parseArray( std::move( first ) );
}

/* IIF( condition, a, b ) and IF( condition, a, b ) */
X_EXPR_PTR X_PARSER::parseIif()
{
   X_EXPR_PTR expr = s_makeExpr( X_EXPR_KIND::IIF );

   m_pos += 2;

   for( int i = 0; i < 3; ++i )
   {
      X_EXPR_PTR arg;

      if( i > 0 && !match( TOK_COMMA ) )
      {
         fail( "IIF wymaga trzech argumentow" );
         return nullptr;
      }

      arg = parseExpression();
      if( !arg )
         return nullptr;

      expr->args.push_back( std::move( arg ) );
   }

   if( !match( TOK_RPAREN ) )
   {
      fail( "IIF wymaga trzech argumentow" );
      return nullptr;
   }

   return expr;
}

/* A name, or the value of an ENUM constant */
X_EXPR_PTR X_PARSER::parseName()
{
   const std::string text = value();
   const std::string key = s_upper( text.c_str() );
   const auto entry = m_enums.find( key );
   X_EXPR_PTR expr = s_makeExpr( X_EXPR_KIND::NAME );

   ++m_pos;
   expr->text = text;

   if( entry == m_enums.end() )
   {
      m_names.insert( key );
      return expr;
   }

   expr->kind = X_EXPR_KIND::NUMBER;
   expr->type = X_DECL_TYPE::INT64;
   expr->integer = entry->second;
   expr->number = static_cast<double>( entry->second );
   expr->numberIsInteger = true;
   expr->text.clear();

   return expr;
}

X_EXPR_PTR X_PARSER::parsePrimary()
{
   const int sourceLine = IIF( m_pos < m_tokens.count, m_tokens.items[m_pos].line, 0 );
   X_EXPR_PTR expr;

   if( atFunctionAddress() )
      expr = parseFunctionAddress();
   else if( is( TOK_SEND ) && peek( 1 ) == TOK_SEND )
   {
      /* ::name is Self:name; the second : stays for parsePostfix() */
      ++m_pos;
      expr = s_makeExpr( X_EXPR_KIND::NAME );
      expr->text = "SELF";
   }
   else if( match( TOK_NIL ) )
      expr = s_makeExpr( X_EXPR_KIND::NIL );
   else if( is( TOK_NUM ) )
      expr = parseNumber();
   else if( is( TOK_STR ) )
   {
      expr = s_makeExpr( X_EXPR_KIND::STRING );
      expr->type = X_DECL_TYPE::STRING;
      expr->text = value();
      ++m_pos;
   }
   else if( is( TOK_TRUE ) || is( TOK_FALSE ) )
   {
      expr = s_makeExpr( X_EXPR_KIND::LOGICAL );
      expr->type = X_DECL_TYPE::LOGIC;
      expr->logical = is( TOK_TRUE );
      ++m_pos;
   }
   else if( match( TOK_LPAREN ) )
      expr = parseGroup( sourceLine );
   else if( match( TOK_LBRACE ) )
      expr = parseBrace( sourceLine );
   else if( is( TOK_IDENT ) && at( "IIF" ) && peek( 1 ) == TOK_LPAREN )
      expr = parseIif();
   else if( is( TOK_IDENT ) || ( is( TOK_FIELD ) && peek( 1 ) == TOK_ALIAS ) )
      expr = parseName();
   else
   {
      fail( "oczekiwano wyrazenia" );
      return nullptr;
   }

   if( expr && expr->line == 0 )
      expr->line = sourceLine;

   return expr;
}

/* a, b in {| a, b | ... }: unique names */
bool X_PARSER::parseBlockParams( X_EXPR &block )
{
   while( true )
   {
      std::string name;

      if( !is( TOK_IDENT ) )
      {
         fail( "oczekiwano nazwy parametru code blocku" );
         return false;
      }

      name = value();
      ++m_pos;

      if( !bindName( name ) )
         return false;

      for( const std::string &existing : block.blockParams )
      {
         if( s_upper( existing.c_str() ) == s_upper( name.c_str() ) )
         {
            fail( "powtorzona nazwa parametru code blocku" );
            return false;
         }
      }

      block.blockParams.push_back( name );

      if( !match( TOK_COMMA ) )
         return true;
   }
}

/* expr, expr, ... */
bool X_PARSER::parseExpressionList( std::vector<X_EXPR_PTR> &list )
{
   while( true )
   {
      X_EXPR_PTR expr = parseExpression();

      if( !expr )
         return false;

      list.push_back( std::move( expr ) );

      if( !match( TOK_COMMA ) )
         return true;
   }
}

/* {| a, b | expr, expr }: the last expression gives the value */
X_EXPR_PTR X_PARSER::parseCodeBlock()
{
   X_EXPR_PTR expr = s_makeExpr( X_EXPR_KIND::BLOCK );

   if( !match( TOK_PIPE ) )
   {
      fail( "oczekiwano \"|\" po \"{\" code blocku" );
      return nullptr;
   }

   if( !is( TOK_PIPE ) && !parseBlockParams( *expr ) )
      return nullptr;

   if( !match( TOK_PIPE ) )
   {
      fail( "oczekiwano \"|\" po parametrach code blocku" );
      return nullptr;
   }

   if( !is( TOK_RBRACE ) && !parseExpressionList( expr->args ) )
      return nullptr;

   if( !match( TOK_RBRACE ) )
   {
      fail( "oczekiwano }" );
      return nullptr;
   }

   return expr;
}

bool X_PARSER::atFunctionAddress() const
{
   return is( TOK_REFERENCE ) && peek( 1 ) == TOK_IDENT && peek( 2 ) == TOK_LPAREN && peek( 3 ) == TOK_RPAREN;
}

bool X_PARSER::atDot() const
{
   return is( TOK_OP ) && std::strcmp( value(), "." ) == 0;
}

bool X_PARSER::atPostfix() const
{
   return is( TOK_LPAREN ) || is( TOK_LBRACKET ) || is( TOK_SEND ) || is( TOK_ALIAS ) || atDot();
}

/* st.field */
X_EXPR_PTR X_PARSER::parseDotField( X_EXPR_PTR expr )
{
   X_EXPR_PTR member;

   ++m_pos;

   if( !is( TOK_IDENT ) )
   {
      fail( "po . oczekiwano nazwy pola STRUCT" );
      return nullptr;
   }

   member = s_makeExpr( X_EXPR_KIND::SEND );
   member->line = expr->line;
   member->text = value();
   member->dotMemberAccess = true;
   member->args.push_back( std::move( expr ) );
   ++m_pos;

   return member;
}

/* alias->field and alias->( expr, ... ); FIELD-> is the current work area */
X_EXPR_PTR X_PARSER::parseAliased( X_EXPR_PTR expr )
{
   X_EXPR_PTR qualified = s_makeExpr( X_EXPR_KIND::FIELD );

   /* A plain name before -> is the alias text */
   if( expr->kind == X_EXPR_KIND::NAME && !expr->parenthesized )
   {
      const std::string name = s_upper( expr->text.c_str() );

      if( name == "FIELD" || name == "_FIELD" )
      {
         expr->kind = X_EXPR_KIND::NIL;
         expr->text.clear();
      }
      else
         expr->kind = X_EXPR_KIND::STRING;
   }

   qualified->line = expr->line;
   qualified->args.push_back( std::move( expr ) );

   if( match( TOK_LPAREN ) )
   {
      qualified->kind = X_EXPR_KIND::ALIAS;

      if( !parseExpressionList( qualified->args ) )
         return nullptr;

      if( !match( TOK_RPAREN ) )
      {
         fail( "oczekiwano )" );
         return nullptr;
      }

      return qualified;
   }

   if( !is( TOK_IDENT ) )
   {
      fail( "po -> oczekiwano pola albo (wyrazenia)" );
      return nullptr;
   }

   qualified->text = value();
   ++m_pos;

   return qualified;
}

/* @name: the variable is passed by reference */
X_EXPR_PTR X_PARSER::parseReference()
{
   X_EXPR_PTR arg;

   if( !is( TOK_IDENT ) )
   {
      fail( "po @ oczekiwano nazwy zmiennej" );
      return nullptr;
   }

   arg = s_makeExpr( X_EXPR_KIND::NAME );
   arg->line = m_tokens.items[m_pos].line;
   arg->text = value();
   ++m_pos;

   if( !bindName( arg->text ) )
      return nullptr;

   return arg;
}

/* CAST( value AS type ): one value, not STRUCT or OBJECT; m_pos is at AS */
bool X_PARSER::parseCast( X_EXPR &call, bool byReference )
{
   if( call.args.size() != 1 || byReference )
   {
      fail( "CAST wymaga jednego wyrazenia przed AS" );
      return false;
   }

   if( !parseType( call.type ) )
      return false;

   if( call.type == X_DECL_TYPE::STRUCT || call.type == X_DECL_TYPE::OBJECT )
   {
      fail( "CAST STRUCT i OBJECT nie jest obslugiwany" );
      return false;
   }

   call.kind = X_EXPR_KIND::CAST;
   call.text.clear();
   call.argByReference.clear();

   return true;
}

/* Arguments of a call: @name passes a variable, @Name() is a function address, an empty argument is NIL */
bool X_PARSER::parseArguments( X_EXPR &call )
{
   while( true )
   {
      const bool address = atFunctionAddress();
      const bool byReference = !address && match( TOK_REFERENCE );
      X_EXPR_PTR arg;

      if( address )
         arg = parsePrimary();
      else if( byReference )
         arg = parseReference();
      else if( is( TOK_COMMA ) || is( TOK_RPAREN ) )
      {
         arg = s_makeExpr( X_EXPR_KIND::NIL );
         arg->line = call.line;
      }
      else
         arg = parseExpression();

      if( !arg )
         return false;

      call.args.push_back( std::move( arg ) );
      call.argByReference.push_back( byReference );

      if( s_upper( call.text.c_str() ) == "CAST" && is( TOK_AS ) )
         return parseCast( call, byReference );

      if( !match( TOK_COMMA ) )
         return true;
   }
}

/* name( args ) */
X_EXPR_PTR X_PARSER::parseCall( X_EXPR_PTR expr )
{
   X_EXPR_PTR call;

   if( expr->kind != X_EXPR_KIND::NAME )
   {
      fail( "wywolanie wymaga nazwy funkcji" );
      return nullptr;
   }

   call = s_makeExpr( X_EXPR_KIND::CALL );
   call->line = expr->line;
   call->text = expr->text;

   if( !is( TOK_RPAREN ) && !parseArguments( *call ) )
      return nullptr;

   if( !match( TOK_RPAREN ) )
   {
      fail( "oczekiwano )" );
      return nullptr;
   }

   return call;
}

/* a[ i ] and a[ i, j ], which is a[ i ][ j ] */
X_EXPR_PTR X_PARSER::parseIndex( X_EXPR_PTR expr )
{
   do
   {
      X_EXPR_PTR index = parseExpression();
      X_EXPR_PTR indexed;

      if( !index )
         return nullptr;

      indexed = s_makeExpr( X_EXPR_KIND::INDEX );
      indexed->line = expr->line;
      indexed->args.push_back( std::move( expr ) );
      indexed->args.push_back( std::move( index ) );
      expr = std::move( indexed );
   } while( match( TOK_COMMA ) );

   if( !match( TOK_RBRACKET ) )
   {
      fail( "oczekiwano ]" );
      return nullptr;
   }

   return expr;
}

/* var:enumIndex is a property; var:enumIndex( n ) with one argument changes the FOR EACH */
X_EXPR_PTR X_PARSER::parseEnumMessage( const X_EXPR &receiver, X_EXPR_PTR property )
{
   const char *message = value();
   X_EXPR_PTR argument;

   ++m_pos;

   if( !match( TOK_LPAREN ) || match( TOK_RPAREN ) )
      return property;

   argument = parseExpression();
   if( !argument )
      return nullptr;

   if( !match( TOK_RPAREN ) )
   {
      fail( "komunikat enum* przyjmuje jeden argument" );
      return nullptr;
   }

   return enumMessage( receiver, message, std::move( argument ) );
}

/* Arguments of a message: @name passes a variable */
bool X_PARSER::parseMessageArguments( X_EXPR &send )
{
   while( true )
   {
      const bool byReference = match( TOK_REFERENCE );
      X_EXPR_PTR arg;

      if( byReference )
         arg = parseReference();
      else
         arg = parseExpression();

      if( !arg )
         return false;

      send.args.push_back( std::move( arg ) );
      send.argByReference.push_back( byReference );

      if( !match( TOK_COMMA ) )
         return true;
   }
}

/* obj:message and obj:message( args ); m_pos is at : */
X_EXPR_PTR X_PARSER::parseSend( X_EXPR_PTR expr )
{
   X_EXPR_PTR send;
   X_EXPR_PTR property;

   ++m_pos;

   if( !isMemberName() )
   {
      fail( "po : oczekiwano nazwy komunikatu" );
      return nullptr;
   }

   property = enumMessage( *expr, value() );
   if( property )
      return parseEnumMessage( *expr, std::move( property ) );

   send = s_makeExpr( X_EXPR_KIND::SEND );
   send->line = expr->line;
   send->text = value();
   send->sendToSuper = expr->kind == X_EXPR_KIND::NAME && s_upper( expr->text.c_str() ) == "SUPER";
   send->args.push_back( std::move( expr ) );
   ++m_pos;

   if( !match( TOK_LPAREN ) )
      return send;

   send->sendHasParentheses = true;

   if( !is( TOK_RPAREN ) && !parseMessageArguments( *send ) )
      return nullptr;

   if( !match( TOK_RPAREN ) )
   {
      fail( "oczekiwano )" );
      return nullptr;
   }

   return send;
}

/* Calls, indexes, messages, aliases and STRUCT fields after a primary expression, then x++ / x-- */
X_EXPR_PTR X_PARSER::parsePostfix()
{
   X_EXPR_PTR expr = parsePrimary();

   while( expr && atPostfix() )
   {
      if( atDot() )
         expr = parseDotField( std::move( expr ) );
      else if( match( TOK_ALIAS ) )
         expr = parseAliased( std::move( expr ) );
      else if( match( TOK_LPAREN ) )
         expr = parseCall( std::move( expr ) );
      else if( match( TOK_LBRACKET ) )
         expr = parseIndex( std::move( expr ) );
      else
         expr = parseSend( std::move( expr ) );
   }

   if( !expr )
      return nullptr;

   if( match( TOK_INC ) )
      return makeUpdate( X_UPDATE_OP::INC, X_UPDATE_MODE::POSTFIX, std::move( expr ) );

   if( match( TOK_DEC ) )
      return makeUpdate( X_UPDATE_OP::DEC, X_UPDATE_MODE::POSTFIX, std::move( expr ) );

   return expr;
}

X_EXPR_PTR X_PARSER::parseUnary()
{
   if( match( TOK_INC ) )
      return makeUpdate( X_UPDATE_OP::INC, X_UPDATE_MODE::PREFIX, parseUnary() );

   if( match( TOK_DEC ) )
      return makeUpdate( X_UPDATE_OP::DEC, X_UPDATE_MODE::PREFIX, parseUnary() );

   if( is( TOK_PLUS ) || is( TOK_MINUS ) || is( TOK_NOT ) )
   {
      X_EXPR_PTR expr = s_makeExpr( X_EXPR_KIND::UNARY );

      expr->line = m_tokens.items[m_pos].line;
      expr->text = s_operatorName( peek( 0 ) );
      ++m_pos;
      expr->args.push_back( parseUnary() );

      if( !expr->args[0] )
         return nullptr;

      return expr;
   }

   return parsePostfix();
}

/* .NOT. binds looser than comparisons and tighter than .AND.: !a == b is !( a == b ) */
X_EXPR_PTR X_PARSER::parseBinary( int minPrecedence )
{
   X_EXPR_PTR left;

   if( is( TOK_NOT ) && minPrecedence <= s_precedence( TOK_EQ ) )
   {
      left = s_makeExpr( X_EXPR_KIND::UNARY );
      left->line = m_tokens.items[m_pos].line;
      left->text = s_operatorName( TOK_NOT );
      ++m_pos;
      left->args.push_back( parseBinary( s_precedence( TOK_EQ ) ) );

      if( !left->args[0] )
         return nullptr;
   }
   else
      left = parseUnary();

   while( left && atBinaryOperatorToken() )
   {
      const X_TOKTYPE type = peek( 0 );
      const int precedence = s_precedence( type );
      const int next = precedence + IIF( s_isRightAssociative( type ), 0, 1 );
      X_EXPR_PTR right;
      X_EXPR_PTR result;

      if( precedence < minPrecedence )
         break;

      ++m_pos;
      right = parseBinary( next );
      if( !right )
         return nullptr;

      result = s_makeExpr( X_EXPR_KIND::BINARY );
      result->line = left->line;
      result->text = s_operatorName( type );
      result->args.push_back( std::move( left ) );
      result->args.push_back( std::move( right ) );
      left = std::move( result );
   }

   return left;
}

bool X_PARSER::matchAssignOperator( X_ASSIGN_OP &op )
{
   for( const X_ASSIGN_TOKEN &entry : s_assignTokens )
   {
      if( match( entry.token ) )
      {
         op = entry.op;
         return true;
      }
   }

   return false;
}

/* Assignments are right-associative: a := b := c */
X_EXPR_PTR X_PARSER::parseAssignment()
{
   X_EXPR_PTR expr = parseBinary( 1 );
   X_ASSIGN_OP op = X_ASSIGN_OP::SET;
   X_EXPR_PTR right;

   if( !expr )
      return nullptr;

   if( !matchAssignOperator( op ) )
      return expr;

   right = parseAssignment();

   if( !right )
      return nullptr;

   return makeAssignment( op, std::move( expr ), std::move( right ) );
}

X_EXPR_PTR X_PARSER::parseExpression()
{
   return parseAssignment();
}

/* ---------------------------------------------------------------------------------------------
   Statements
   --------------------------------------------------------------------------------------------- */

static bool s_isIntegerLiteral( const X_EXPR &expr )
{
   return expr.kind == X_EXPR_KIND::NUMBER && expr.numberIsInteger;
}

/* -5 and +5 in a CASE label become the literal itself; false on overflow */
static bool s_foldSign( X_EXPR &label )
{
   if( label.kind != X_EXPR_KIND::UNARY || ( label.text != "-" && label.text != "+" ) )
      return true;

   if( !s_isIntegerLiteral( *label.args[0] ) )
      return true;

   if( label.text == "+" )
   {
      label.integer = label.args[0]->integer;
      label.number = label.args[0]->number;
   }
   else
   {
      if( __builtin_sub_overflow( int64_t( 0 ), label.args[0]->integer, &label.integer ) )
         return false;

      label.number = -label.args[0]->number;
   }

   label.numberIsInteger = true;
   label.kind = X_EXPR_KIND::NUMBER;
   label.args.clear();

   return true;
}

/* The largest integer a double holds exactly */
static const double s_exactLimit = 9007199254740991.0;

static bool s_sameLabel( const X_EXPR &label, const X_EXPR &previous )
{
   if( label.kind != previous.kind )
      return false;

   if( label.kind == X_EXPR_KIND::STRING )
      return label.text == previous.text;

   return label.number == previous.number;
}

bool X_PARSER::atAny( const std::vector<std::string> &words ) const
{
   for( const std::string &word : words )
   {
      if( at( word.c_str() ) )
         return true;
   }

   return false;
}

/* The statement ends here: end of line or end of file */
bool X_PARSER::expectLineEnd( const char *message )
{
   if( is( TOK_EOL ) || is( TOK_EOF ) )
      return true;

   fail( message );

   return false;
}

/* Statements up to one of the stop words, which is not consumed */
bool X_PARSER::parseBlock( std::vector<X_STMT> &out, const std::vector<std::string> &stopWords )
{
   skipLines();

   /* A routine that starts here means a missing end word; the caller reports it */
   while( !is( TOK_EOF ) && !atAny( stopWords ) && !atRoutineStart() )
   {
      const int start = m_pos;
      X_STMT statement;

      if( !parseStatement( statement ) )
      {
         if( !recover( start ) )
            return false;

         continue;
      }

      out.push_back( std::move( statement ) );

      if( !is( TOK_EOL ) && !is( TOK_EOF ) && !atAny( stopWords ) )
      {
         fail( "oczekiwano konca linii" );
         if( !recover( start ) )
            return false;

         continue;
      }

      skipLines();
   }

   return true;
}

/* IF and ELSEIF have the same syntax; ELSEIF becomes one nested IF in elseBody, so the backend needs no ELSEIF */
bool X_PARSER::parseIf( X_STMT &statement )
{
   bool isElseIf;
   const char *lineEnd;

   if( !is( TOK_IF ) && !is( TOK_ELSEIF ) )
   {
      fail( "oczekiwano IF albo ELSEIF" );
      return false;
   }

   isElseIf = is( TOK_ELSEIF );
   lineEnd = IIF( isElseIf, "oczekiwano konca linii po warunku ELSEIF", "oczekiwano konca linii po warunku IF" );

   ++m_pos;
   statement.kind = X_STMT_KIND::IF;
   statement.expr = parseExpression();

   if( !statement.expr || !expectLineEnd( lineEnd ) )
      return false;

   if( !parseBlock( statement.body, { "ELSEIF", "ELSE", "ENDIF" } ) )
      return false;

   if( is( TOK_ELSEIF ) )
   {
      X_STMT nestedIf;

      nestedIf.line = m_tokens.items[m_pos].line;

      if( !parseIf( nestedIf ) )
         return false;

      statement.elseBody.push_back( std::move( nestedIf ) );
      return true;
   }

   if( match( TOK_ELSE ) )
   {
      if( !expectLineEnd( "oczekiwano konca linii po ELSE" ) )
         return false;

      if( !parseBlock( statement.elseBody, { "ENDIF" } ) )
         return false;
   }

   return expect( "ENDIF" );
}

/* DO WHILE condition ... ENDDO; WHILE condition ... ENDDO is the same */
bool X_PARSER::parseWhile( X_STMT &statement )
{
   bool ok;

   match( TOK_DO );

   if( !expect( "WHILE" ) )
      return false;

   statement.kind = X_STMT_KIND::WHILE;
   statement.expr = parseExpression();

   if( !statement.expr || !expectLineEnd( "oczekiwano konca linii po warunku WHILE" ) )
      return false;

   ++m_loopDepth;
   ok = parseBlock( statement.body, { "ENDDO" } );
   --m_loopDepth;

   return ok && expect( "ENDDO" );
}

/* OTHERWISE is the last branch of DO CASE and SWITCH */
bool X_PARSER::parseOtherwise( X_STMT &statement, const std::vector<std::string> &stopWords )
{
   statement.hasOtherwise = true;

   if( !expectLineEnd( "oczekiwano konca linii po OTHERWISE" ) )
      return false;

   if( !parseBlock( statement.otherwiseBody, stopWords ) )
      return false;

   if( is( TOK_CASE ) || is( TOK_OTHERWISE ) )
   {
      fail( "CASE ani OTHERWISE nie moga wystapic po OTHERWISE" );
      return false;
   }

   return true;
}

bool X_PARSER::parseDoCase( X_STMT &statement )
{
   const std::vector<std::string> stopWords = { "CASE", "OTHERWISE", "ENDCASE" };

   ++m_pos;

   if( !match( TOK_CASE ) )
   {
      fail( "oczekiwano CASE po DO" );
      return false;
   }

   statement.kind = X_STMT_KIND::DO_CASE;

   if( !expectLineEnd( "oczekiwano konca linii po DO CASE" ) )
      return false;

   skipLines();

   while( match( TOK_CASE ) )
   {
      X_CASE_BRANCH branch;

      branch.condition = parseExpression();

      if( !branch.condition || !expectLineEnd( "oczekiwano konca linii po CASE" ) )
         return false;

      if( !parseBlock( branch.body, stopWords ) )
         return false;

      statement.cases.push_back( std::move( branch ) );
   }

   if( match( TOK_OTHERWISE ) && !parseOtherwise( statement, stopWords ) )
      return false;

   if( !match( TOK_ENDCASE ) )
   {
      fail( "brak ENDCASE dla DO CASE" );
      return false;
   }

   return true;
}

/* A SWITCH label: NIL, a STRING or an integer literal with an optional sign; no label twice */
bool X_PARSER::switchLabel( const X_STMT &statement, X_EXPR &label )
{
   if( !s_foldSign( label ) )
   {
      fail( "integer overflow in CASE label" );
      return false;
   }

   if( label.kind != X_EXPR_KIND::NIL && label.kind != X_EXPR_KIND::STRING && !s_isIntegerLiteral( label ) )
   {
      fail( "CASE w SWITCH wymaga literalu NIL, INTEGER lub STRING" );
      return false;
   }

   /* Retain the Stage I label range; widening SWITCH is a separate change */
   if( label.kind == X_EXPR_KIND::NUMBER && ( label.number < -s_exactLimit || label.number > s_exactLimit ) )
   {
      fail( "literal CASE poza dokladnym zakresem INTEGER w AST xDev" );
      return false;
   }

   for( const X_CASE_BRANCH &previous : statement.cases )
   {
      if( s_sameLabel( label, *previous.condition ) )
      {
         fail( "powtorzony literal CASE w SWITCH" );
         return false;
      }
   }

   return true;
}

bool X_PARSER::parseSwitchBody( X_STMT &statement )
{
   const std::vector<std::string> stopWords = { "CASE", "OTHERWISE", "ENDSWITCH" };

   while( match( TOK_CASE ) )
   {
      X_CASE_BRANCH branch;

      branch.condition = parseExpression();

      if( !branch.condition || !switchLabel( statement, *branch.condition ) )
         return false;

      if( !expectLineEnd( "oczekiwano konca linii po CASE" ) )
         return false;

      if( !parseBlock( branch.body, stopWords ) )
         return false;

      statement.cases.push_back( std::move( branch ) );
   }

   if( match( TOK_OTHERWISE ) && !parseOtherwise( statement, stopWords ) )
      return false;

   if( !match( TOK_ENDSWITCH ) )
   {
      fail( "brak ENDSWITCH dla SWITCH" );
      return false;
   }

   return true;
}

bool X_PARSER::parseSwitch( X_STMT &statement )
{
   bool ok;

   ++m_pos;
   statement.kind = X_STMT_KIND::SWITCH;
   statement.expr = parseExpression();

   if( !statement.expr || !expectLineEnd( "oczekiwano konca linii po SWITCH" ) )
      return false;

   skipLines();

   ++m_switchDepth;
   ok = parseSwitchBody( statement );
   --m_switchDepth;

   return ok;
}

/* ---------------------------------------------------------------------------------------------
   FOR EACH
   --------------------------------------------------------------------------------------------- */

static X_EXPR_PTR s_nameExpr( const std::string &text, int line )
{
   X_EXPR_PTR expr = s_makeExpr( X_EXPR_KIND::NAME );

   expr->line = line;
   expr->text = text;

   return expr;
}

static X_EXPR_PTR s_integerExpr( int64_t value, int line )
{
   X_EXPR_PTR expr = s_makeExpr( X_EXPR_KIND::NUMBER );

   expr->line = line;
   expr->integer = value;
   expr->number = ( double )value;
   expr->numberIsInteger = true;

   return expr;
}

static X_EXPR_PTR s_logicalExpr( bool value, int line )
{
   X_EXPR_PTR expr = s_makeExpr( X_EXPR_KIND::LOGICAL );

   expr->line = line;
   expr->logical = value;

   return expr;
}

static X_EXPR_PTR s_groupExpr( int line )
{
   X_EXPR_PTR expr = s_makeExpr( X_EXPR_KIND::GROUP );

   expr->line = line;

   return expr;
}

static X_EXPR_PTR s_binaryExpr( const char *op, X_EXPR_PTR left, X_EXPR_PTR right, int line )
{
   X_EXPR_PTR expr = s_makeExpr( X_EXPR_KIND::BINARY );

   expr->line = line;
   expr->text = op;
   expr->args.push_back( std::move( left ) );
   expr->args.push_back( std::move( right ) );

   return expr;
}

/* A call of a C function of the runtime, by its exact name */
static X_EXPR_PTR s_internalCall( const char *function, int line )
{
   X_EXPR_PTR expr = s_makeExpr( X_EXPR_KIND::CALL );

   expr->line = line;
   expr->text = function;
   expr->internalCall = true;

   return expr;
}

static void s_addArg( X_EXPR &target, X_EXPR_PTR value, bool byReference = false )
{
   target.args.push_back( std::move( value ) );
   target.argByReference.push_back( byReference );
}

static X_STMT s_statement( X_STMT_KIND kind, int line )
{
   X_STMT result;

   result.kind = kind;
   result.line = line;

   return result;
}

static X_STMT s_exprStatement( X_EXPR_PTR expr, int line )
{
   X_STMT result = s_statement( X_STMT_KIND::CALL, line );

   result.expr = std::move( expr );

   return result;
}

static X_STMT s_when( X_EXPR_PTR condition, std::vector<X_STMT> body, int line )
{
   X_STMT result = s_statement( X_STMT_KIND::IF, line );

   result.expr = std::move( condition );
   result.body = std::move( body );

   return result;
}

/* var:enumValue( x ): x_eachSwap( @var, x ) */
static X_EXPR_PTR s_enumSetValue( const X_EXPR &receiver, X_EXPR_PTR argument )
{
   X_EXPR_PTR expr = s_internalCall( "x_eachSwap", receiver.line );

   s_addArg( *expr, s_nameExpr( receiver.text, receiver.line ), true );
   s_addArg( *expr, std::move( argument ) );

   return expr;
}

/* A hidden local of the loop: __XEACHn_suffix */
std::string X_PARSER::eachHidden( const X_EACH &each, const std::string &suffix )
{
   const std::string name = each.tag + suffix;

   m_enumLocals.push_back( name );
   m_names.insert( s_upper( name.c_str() ) );

   return name;
}

/* The expression after IN; IN @var gives the base back to var after the loop */
bool X_PARSER::parseEachSource( X_EACH_PAIR &pair, int line )
{
   if( !match( TOK_REFERENCE ) )
   {
      pair.expr = parseExpression();
      return pair.expr != nullptr;
   }

   if( !is( TOK_IDENT ) )
   {
      fail( "po @ oczekiwano nazwy zmiennej" );
      return false;
   }

   pair.byReference = true;
   pair.expr = s_nameExpr( value(), line );
   ++m_pos;

   return bindName( pair.expr->text );
}

/* a, b IN x, @y [DESCEND]; m_pos is after EACH */
bool X_PARSER::parseEachHead( X_EACH &each )
{
   do
   {
      X_EACH_PAIR pair;

      if( !is( TOK_IDENT ) )
      {
         fail( "oczekiwano zmiennej po FOR EACH" );
         return false;
      }

      pair.var = value();
      ++m_pos;

      if( !bindName( pair.var ) )
         return false;

      each.pairs.push_back( std::move( pair ) );
   } while( match( TOK_COMMA ) );

   if( !match( TOK_IN ) )
   {
      fail( "oczekiwano IN po zmiennych FOR EACH" );
      return false;
   }

   for( size_t i = 0; i < each.pairs.size(); ++i )
   {
      if( i > 0 && !match( TOK_COMMA ) )
      {
         fail( "FOR EACH wymaga tylu wyrazen po IN, ile jest zmiennych" );
         return false;
      }

      if( !parseEachSource( each.pairs[i], each.line ) )
         return false;
   }

   if( is( TOK_IDENT ) && at( "DESCEND" ) )
   {
      ++m_pos;
      each.descend = true;
   }

   return expectLineEnd( "oczekiwano konca linii po FOR EACH" );
}

/* The hidden locals: TURN and COUNT of the loop, BASE, POS, SAVED and LINKED of every pair */
void X_PARSER::eachNames( X_EACH &each )
{
   each.turn = eachHidden( each, "TURN" );
   each.count = eachHidden( each, "COUNT" );

   for( size_t i = 0; i < each.pairs.size(); ++i )
   {
      X_EACH_PAIR &pair = each.pairs[i];
      const std::string suffix = std::to_string( i + 1 );

      pair.hidden.name = s_upper( pair.var.c_str() );
      pair.hidden.base = eachHidden( each, "BASE" + suffix );
      pair.hidden.pos = eachHidden( each, "POS" + suffix );
      pair.hidden.turn = each.turn;
      pair.hidden.count = each.count;
      pair.hidden.descend = each.descend;
      eachHidden( each, "SAVED" + suffix );

      if( pair.byReference )
      {
         pair.hidden.target = pair.expr->text;
         pair.hidden.linked = eachHidden( each, "LINKED" + suffix );
      }
   }
}

/* target := value as a statement */
X_STMT X_PARSER::eachAssign( const std::string &target, X_EXPR_PTR value, int line )
{
   X_EXPR_PTR assignment = makeAssignment( X_ASSIGN_OP::SET, s_nameExpr( target, line ), std::move( value ) );

   return s_exprStatement( std::move( assignment ), line );
}

/* x_eachPut( @base, pos, var ) for every pair: the variable stands for its element */
std::vector<X_STMT> X_PARSER::eachPutBack( const X_EACH &each ) const
{
   std::vector<X_STMT> body;

   for( const X_EACH_PAIR &pair : each.pairs )
   {
      X_EXPR_PTR put = s_internalCall( "x_eachPut", each.line );

      s_addArg( *put, s_nameExpr( pair.hidden.base, each.line ), true );
      s_addArg( *put, s_nameExpr( pair.hidden.pos, each.line ) );
      s_addArg( *put, s_nameExpr( pair.var, each.line ) );
      body.push_back( s_exprStatement( std::move( put ), each.line ) );
   }

   return body;
}

/* base := expr; saved := var; linked := .T. for IN @var; count := x_eachCount( bases ); turn := 0 */
void X_PARSER::eachPrologue( X_EACH &each, X_STMT &statement )
{
   const int line = each.line;
   X_EXPR_PTR counted = s_internalCall( "x_eachCount", line );

   for( size_t i = 0; i < each.pairs.size(); ++i )
   {
      X_EACH_PAIR &pair = each.pairs[i];
      const std::string saved = each.tag + "SAVED" + std::to_string( i + 1 );
      X_EXPR_PTR source;

      /* IN @var reads the variable; its name stays in pair.expr for the epilogue */
      if( pair.byReference )
         source = s_nameExpr( pair.expr->text, line );
      else
         source = std::move( pair.expr );

      statement.body.push_back( eachAssign( pair.hidden.base, std::move( source ), line ) );
      statement.body.push_back( eachAssign( saved, s_nameExpr( pair.var, line ), line ) );

      if( pair.byReference )
         statement.body.push_back( eachAssign( pair.hidden.linked, s_logicalExpr( true, line ), line ) );

      s_addArg( *counted, s_nameExpr( pair.hidden.base, line ) );
   }

   statement.body.push_back( eachAssign( each.count, std::move( counted ), line ) );
   statement.body.push_back( eachAssign( each.turn, s_integerExpr( 0, line ), line ) );
}

/* DO WHILE .T. ... ENDDO around the body; see the comment above parseForEach() */
X_STMT X_PARSER::eachLoop( X_EACH &each, std::vector<X_STMT> &body )
{
   const int line = each.line;
   X_STMT loop = s_statement( X_STMT_KIND::WHILE, line );
   X_EXPR_PTR started = s_binaryExpr( ">", s_nameExpr( each.turn, line ), s_integerExpr( 0, line ), line );
   X_EXPR_PTR next = s_binaryExpr( "+", s_nameExpr( each.turn, line ), s_integerExpr( 1, line ), line );
   X_EXPR_PTR finished = s_binaryExpr( ">", s_nameExpr( each.turn, line ), s_nameExpr( each.count, line ), line );
   std::vector<X_STMT> leave;

   leave.push_back( s_statement( X_STMT_KIND::EXIT, line ) );

   loop.expr = s_logicalExpr( true, line );
   loop.body.push_back( s_when( std::move( started ), eachPutBack( each ), line ) );
   loop.body.push_back( eachAssign( each.turn, std::move( next ), line ) );
   loop.body.push_back( s_when( std::move( finished ), std::move( leave ), line ) );

   for( const X_EACH_PAIR &pair : each.pairs )
   {
      X_EXPR_PTR pos = s_internalCall( "x_eachPos", line );
      X_EXPR_PTR get = s_internalCall( "x_eachGet", line );

      s_addArg( *pos, s_nameExpr( pair.hidden.base, line ) );
      s_addArg( *pos, s_nameExpr( each.turn, line ) );
      s_addArg( *pos, s_logicalExpr( each.descend, line ) );
      loop.body.push_back( eachAssign( pair.hidden.pos, std::move( pos ), line ) );

      s_addArg( *get, s_nameExpr( pair.hidden.base, line ) );
      s_addArg( *get, s_nameExpr( pair.hidden.pos, line ) );
      loop.body.push_back( eachAssign( pair.var, std::move( get ), line ) );
   }

   for( X_STMT &inner : body )
      loop.body.push_back( std::move( inner ) );

   return loop;
}

/* After the loop: the last element is put back, IN @var gets the base, the variables get their saved values */
void X_PARSER::eachEpilogue( X_EACH &each, X_STMT &statement )
{
   const int line = each.line;
   X_EXPR_PTR first = s_binaryExpr( ">=", s_nameExpr( each.turn, line ), s_integerExpr( 1, line ), line );
   X_EXPR_PTR last = s_binaryExpr( "<=", s_nameExpr( each.turn, line ), s_nameExpr( each.count, line ), line );
   X_EXPR_PTR inside = s_binaryExpr( ".AND.", std::move( first ), std::move( last ), line );

   statement.body.push_back( s_when( std::move( inside ), eachPutBack( each ), line ) );

   for( size_t i = 0; i < each.pairs.size(); ++i )
   {
      const X_EACH_PAIR &pair = each.pairs[i];
      const std::string saved = each.tag + "SAVED" + std::to_string( i + 1 );

      if( pair.byReference )
      {
         std::vector<X_STMT> back;

         back.push_back( eachAssign( pair.expr->text, s_nameExpr( pair.hidden.base, line ), line ) );
         statement.body.push_back( s_when( s_nameExpr( pair.hidden.linked, line ), std::move( back ), line ) );
      }

      statement.body.push_back( eachAssign( pair.var, s_nameExpr( saved, line ), line ) );
   }
}

/*
 * FOR EACH <var1> [, <varN>] IN [@]<expr1> [, [@]<exprN>] [DESCEND] ... NEXT
 *
 * The loop is written with the statements already known, around hidden locals added at the head of the function:
 *
 *    base := expr; saved := var; count := x_eachCount( base, ... ); turn := 0
 *    DO WHILE .T.
 *       IF turn > 0; x_eachPut( @base, pos, var ); ENDIF      // the variable stands for its element
 *       turn := turn + 1
 *       IF turn > count; EXIT; ENDIF
 *       pos := x_eachPos( base, turn, lDescend ); var := x_eachGet( base, pos )
 *       <body>                                                    // LOOP goes to the top, EXIT below
 *    ENDDO
 *    IF turn >= 1 .AND. turn <= count; x_eachPut( @base, pos, var ); ENDIF
 *    expr := base (only IN @expr); var := saved
 */
bool X_PARSER::parseForEach( X_STMT &statement )
{
   X_EACH each;
   std::vector<X_STMT> body;
   bool ok;

   each.line = m_tokens.items[m_pos].line;
   each.tag = "__XEACH" + std::to_string( ++m_enumCount ) + "_";
   m_pos += 2;

   if( !parseEachHead( each ) )
      return false;

   eachNames( each );

   /* The body sees the loop variables for :enumIndex and the other enum messages */
   for( const X_EACH_PAIR &pair : each.pairs )
      m_enumVars.push_back( pair.hidden );

   ++m_loopDepth;
   ok = parseBlock( body, { "NEXT" } );
   --m_loopDepth;
   m_enumVars.resize( m_enumVars.size() - each.pairs.size() );

   if( !ok || !expect( "NEXT" ) )
      return false;

   /* NEXT var */
   if( is( TOK_IDENT ) && s_upper( value() ) == s_upper( each.pairs[0].var.c_str() ) )
      ++m_pos;

   statement.kind = X_STMT_KIND::IF;
   statement.line = each.line;
   statement.expr = s_logicalExpr( true, each.line );

   eachPrologue( each, statement );
   statement.body.push_back( eachLoop( each, body ) );
   eachEpilogue( each, statement );

   return true;
}

/* The innermost FOR EACH variable with this name */
const X_PARSER::X_ENUM_VAR *X_PARSER::findEnumVar( const std::string &name ) const
{
   const std::string key = s_upper( name.c_str() );

   for( size_t i = m_enumVars.size(); i > 0; --i )
   {
      if( m_enumVars[i - 1].name == key )
         return &m_enumVars[i - 1];
   }

   return nullptr;
}

/* var:enumIndex( n ): x_eachIndex( @base, @pos, @turn, @var, n, descend ) moves the loop to position n */
X_EXPR_PTR X_PARSER::enumSetIndex( const X_ENUM_VAR &var, const X_EXPR &receiver, X_EXPR_PTR argument ) const
{
   const int line = receiver.line;
   X_EXPR_PTR expr = s_internalCall( "x_eachIndex", line );

   s_addArg( *expr, s_nameExpr( var.base, line ), true );
   s_addArg( *expr, s_nameExpr( var.pos, line ), true );
   s_addArg( *expr, s_nameExpr( var.turn, line ), true );
   s_addArg( *expr, s_nameExpr( receiver.text, line ), true );
   s_addArg( *expr, std::move( argument ) );
   s_addArg( *expr, s_logicalExpr( var.descend, line ) );

   return expr;
}

/* IN @target: the variable gets the old base with the changes made so far and keeps it */
X_EXPR_PTR X_PARSER::enumLinkTarget( const X_ENUM_VAR &var, const X_EXPR &receiver, X_EXPR_PTR expr )
{
   const int line = receiver.line;
   X_EXPR_PTR group = s_groupExpr( line );
   X_EXPR_PTR put = s_internalCall( "x_eachPut", line );
   X_EXPR_PTR target = makeAssignment( X_ASSIGN_OP::SET, s_nameExpr( var.target, line ), s_nameExpr( var.base, line ) );
   X_EXPR_PTR unlink = makeAssignment( X_ASSIGN_OP::SET, s_nameExpr( var.linked, line ), s_logicalExpr( false, line ) );

   s_addArg( *put, s_nameExpr( var.base, line ), true );
   s_addArg( *put, s_nameExpr( var.pos, line ) );
   s_addArg( *put, s_nameExpr( receiver.text, line ) );

   group->args.push_back( std::move( put ) );
   group->args.push_back( std::move( target ) );
   group->args.push_back( std::move( unlink ) );
   group->args.push_back( std::move( expr ) );

   return group;
}

/* var:enumBase( a ): x_eachBase( @base, a, @count, @pos, @var, the other bases of the same loop ) */
X_EXPR_PTR X_PARSER::enumSetBase( const X_ENUM_VAR &var, const X_EXPR &receiver, X_EXPR_PTR argument )
{
   const int line = receiver.line;
   X_EXPR_PTR expr = s_internalCall( "x_eachBase", line );

   s_addArg( *expr, s_nameExpr( var.base, line ), true );
   s_addArg( *expr, std::move( argument ) );
   s_addArg( *expr, s_nameExpr( var.count, line ), true );
   s_addArg( *expr, s_nameExpr( var.pos, line ), true );
   s_addArg( *expr, s_nameExpr( receiver.text, line ), true );

   for( const X_ENUM_VAR &other : m_enumVars )
   {
      if( other.count == var.count && other.base != var.base )
         s_addArg( *expr, s_nameExpr( other.base, line ) );
   }

   if( var.target.empty() )
      return expr;

   return enumLinkTarget( var, receiver, std::move( expr ) );
}

/* enumIsFirst and enumIsLast look at the position in the own base, so DESCEND starts with the last element */
X_EXPR_PTR X_PARSER::enumProperty( const X_ENUM_VAR &var, const X_EXPR &receiver, const std::string &key ) const
{
   const int line = receiver.line;
   X_EXPR_PTR property;

   if( key == "ENUMINDEX" )
      return s_nameExpr( var.pos, line );

   if( key == "ENUMVALUE" )
      return s_nameExpr( receiver.text, line );

   if( key == "ENUMBASE" )
      return s_nameExpr( var.base, line );

   if( key == "ENUMKEY" )
   {
      property = s_internalCall( "x_eachKey", line );
      s_addArg( *property, s_nameExpr( var.base, line ) );
      s_addArg( *property, s_nameExpr( var.pos, line ) );
      return property;
   }

   if( key == "ENUMISFIRST" )
      return s_binaryExpr( "<=", s_nameExpr( var.pos, line ), s_integerExpr( 1, line ), line );

   if( key == "ENUMISLAST" )
   {
      property = s_internalCall( "x_eachCount", line );
      s_addArg( *property, s_nameExpr( var.base, line ) );
      return s_binaryExpr( ">=", s_nameExpr( var.pos, line ), std::move( property ), line );
   }

   return nullptr;
}

/*
 * var:enumIndex and the other properties of a FOR EACH variable; (var):enumIndex goes to the value.
 * With an argument: enumIndex( n ) moves the loop to position n, enumValue( x ) gives the element x,
 * enumBase( a ) goes on through a; each returns the old property. enumKey, enumIsFirst and enumIsLast
 * evaluate the argument and ignore it.
 */
X_EXPR_PTR X_PARSER::enumMessage( const X_EXPR &receiver, const char *message, X_EXPR_PTR argument )
{
   const std::string key = s_upper( message );
   const X_ENUM_VAR *var;
   X_EXPR_PTR property;

   if( receiver.kind != X_EXPR_KIND::NAME || receiver.parenthesized || key.rfind( "ENUM", 0 ) != 0 )
      return nullptr;

   var = findEnumVar( receiver.text );
   if( !var )
      return nullptr;

   if( argument && key == "ENUMINDEX" )
      return enumSetIndex( *var, receiver, std::move( argument ) );

   if( argument && key == "ENUMVALUE" )
      return s_enumSetValue( receiver, std::move( argument ) );

   if( argument && key == "ENUMBASE" )
      return enumSetBase( *var, receiver, std::move( argument ) );

   property = enumProperty( *var, receiver, key );
   if( !property || !argument )
      return property;

   X_EXPR_PTR group = s_groupExpr( receiver.line );

   group->args.push_back( std::move( argument ) );
   group->args.push_back( std::move( property ) );

   return group;
}

/* ---------------------------------------------------------------------------------------------
   FOR, LOCAL, STATIC, FIELD
   --------------------------------------------------------------------------------------------- */

/* FOR var := start TO limit [STEP step] ... NEXT [var] */
bool X_PARSER::parseFor( X_STMT &statement )
{
   bool ok;

   if( peek( 1 ) == TOK_IDENT && s_upper( value( 1 ) ) == "EACH" )
      return parseForEach( statement );

   ++m_pos;
   statement.kind = X_STMT_KIND::FOR;

   if( !is( TOK_IDENT ) )
   {
      fail( "oczekiwano zmiennej sterujacej po FOR" );
      return false;
   }

   statement.name = value();
   ++m_pos;

   if( !bindName( statement.name ) || !expect( ":=" ) )
      return false;

   statement.expr = parseExpression();
   if( !statement.expr || !expect( "TO" ) )
      return false;

   statement.forLimit = parseExpression();
   if( !statement.forLimit )
      return false;

   if( match( TOK_STEP ) )
   {
      statement.forStep = parseExpression();
      if( !statement.forStep )
         return false;
   }

   if( !expectLineEnd( "oczekiwano konca linii po FOR" ) )
      return false;

   ++m_loopDepth;
   ok = parseBlock( statement.body, { "NEXT" } );
   --m_loopDepth;

   if( !ok || !expect( "NEXT" ) )
      return false;

   if( is( TOK_IDENT ) )
   {
      const std::string nextName = value();

      ++m_pos;

      if( s_upper( nextName.c_str() ) != s_upper( statement.name.c_str() ) )
      {
         fail( "zmienna po NEXT nie odpowiada zmiennej FOR" );
         return false;
      }
   }

   return true;
}

/* name[ dims ] [AS type] of LOCAL and STATIC */
bool X_PARSER::parseDeclaration( X_LOCAL_DECL &decl, const char *missing )
{
   if( !is( TOK_IDENT ) )
   {
      fail( missing );
      return false;
   }

   decl.line = m_tokens.items[m_pos].line;
   decl.name = value();
   ++m_pos;

   if( !bindName( decl.name ) || !parseArrayDimensions( decl ) )
      return false;

   if( is( TOK_AS ) )
   {
      X_DECL_TYPE type;

      if( !parseType( type, &decl.declaredTypeName ) )
         return false;

      decl.declaredType = type;
   }

   return true;
}

bool X_PARSER::parseLocal( X_STMT &statement )
{
   ++m_pos;
   statement.kind = X_STMT_KIND::LOCAL;

   while( true )
   {
      X_LOCAL_DECL decl;

      if( !parseDeclaration( decl, "oczekiwano nazwy zmiennej po LOCAL" ) )
         return false;

      if( at( ":=" ) )
      {
         if( !decl.dimensions.empty() )
         {
            fail( "tablica LOCAL nie moze miec inicjalizatora" );
            return false;
         }

         ++m_pos;
         decl.init = parseExpression();
         if( !decl.init )
            return false;
      }

      statement.locals.push_back( std::move( decl ) );

      if( !at( "," ) )
         break;

      ++m_pos;
   }

   return expectLineEnd( "oczekiwano konca linii po LOCAL" );
}

/* name[ a, b ][ c ]: the dimensions of an array declaration */
bool X_PARSER::parseArrayDimensions( X_LOCAL_DECL &decl )
{
   while( at( "[" ) )
   {
      ++m_pos;

      if( at( "]" ) )
      {
         fail( "oczekiwano wymiaru tablicy" );
         return false;
      }

      while( true )
      {
         X_EXPR_PTR dimension = parseExpression();

         if( !dimension )
            return false;

         decl.dimensions.push_back( std::move( dimension ) );

         if( !at( "," ) )
            break;

         ++m_pos;
      }

      if( !expect( "]" ) )
         return false;
   }

   return true;
}

bool X_PARSER::constantArgs( const X_EXPR &expr ) const
{
   for( const X_EXPR_PTR &arg : expr.args )
   {
      if( !isConstantExpr( *arg ) )
         return false;
   }

   return true;
}

/* Literals, operators, CAST and array or hash literals of constants */
bool X_PARSER::isConstantExpr( const X_EXPR &expr ) const
{
   switch( expr.kind )
   {
   case X_EXPR_KIND::NIL:
   case X_EXPR_KIND::NUMBER:
   case X_EXPR_KIND::STRING:
   case X_EXPR_KIND::LOGICAL: return true;
   case X_EXPR_KIND::UNARY:
   case X_EXPR_KIND::BINARY:
   case X_EXPR_KIND::ARRAY:
   case X_EXPR_KIND::CAST:    return constantArgs( expr );
   case X_EXPR_KIND::CALL:    return expr.hashLiteral && constantArgs( expr );
   default:                   return false;
   }
}

/* STATIC dimensions and initializers are compile-time constants */
bool X_PARSER::parseStaticInit( X_LOCAL_DECL &decl )
{
   for( const X_EXPR_PTR &dimension : decl.dimensions )
   {
      if( !isConstantExpr( *dimension ) )
      {
         fail( "wymiar STATIC musi byc poprawna stala kompilacyjna" );
         return false;
      }
   }

   if( !at( ":=" ) )
      return true;

   if( !decl.dimensions.empty() )
   {
      fail( "tablica STATIC nie moze miec inicjalizatora" );
      return false;
   }

   ++m_pos;
   decl.init = parseExpression();
   if( !decl.init )
      return false;

   if( !isConstantExpr( *decl.init ) )
   {
      fail( "inicjalizator STATIC musi byc stala kompilacyjna (literaly, operatory, literal tablicy i haszmapy)" );
      return false;
   }

   return true;
}

bool X_PARSER::parseStatic( X_STMT &statement )
{
   ++m_pos;
   statement.kind = X_STMT_KIND::STATIC;

   while( true )
   {
      X_LOCAL_DECL decl;

      if( !parseDeclaration( decl, "oczekiwano nazwy zmiennej po STATIC" ) )
         return false;

      if( !parseStaticInit( decl ) )
         return false;

      statement.locals.push_back( std::move( decl ) );

      if( !at( "," ) )
         break;

      ++m_pos;
   }

   return expectLineEnd( "oczekiwano konca linii po STATIC" );
}

/* REQUEST name, ...: anchors for the linker */
bool X_PARSER::parseRequest( X_STMT &statement )
{
   ++m_pos;
   statement.kind = X_STMT_KIND::REQUEST;

   do
   {
      if( !is( TOK_IDENT ) )
      {
         fail( "REQUEST wymaga nazwy symbolu" );
         return false;
      }

      statement.requests.push_back( s_upper( value() ) );
      ++m_pos;
   } while( match( TOK_COMMA ) );

   return expectLineEnd( "REQUEST wymaga listy nazw, bez argumentow ani wyrazen" );
}

/* ANNOUNCE name: the module name is an empty procedure, so REQUEST name in another file links this module */
bool X_PARSER::parseAnnounce( X_PROGRAM &program )
{
   X_FUNCTION module;

   module.line = m_tokens.items[m_pos].line;
   ++m_pos;

   if( !is( TOK_IDENT ) )
   {
      fail( "ANNOUNCE wymaga nazwy modulu" );
      return false;
   }

   if( m_announced )
   {
      fail( "plik .prg ma tylko jedna nazwe modulu ANNOUNCE" );
      return false;
   }

   module.name = value();
   ++m_pos;

   if( !expectLineEnd( "ANNOUNCE przyjmuje jedna nazwe modulu" ) )
      return false;

   module.isProcedure = true;
   program.functions.push_back( std::move( module ) );
   m_announced = true;

   return true;
}

/* FIELD name, ... [IN alias] */
bool X_PARSER::parseFieldDeclaration( X_STMT &statement )
{
   ++m_pos;
   statement.kind = X_STMT_KIND::FIELD;

   do
   {
      X_LOCAL_DECL field;

      if( !is( TOK_IDENT ) )
      {
         fail( "FIELD wymaga nazwy pola" );
         return false;
      }

      field.name = value();
      ++m_pos;

      if( !bindName( field.name ) )
         return false;

      statement.locals.push_back( std::move( field ) );
   } while( match( TOK_COMMA ) );

   if( match( TOK_IN ) )
   {
      if( !is( TOK_IDENT ) )
      {
         fail( "FIELD IN wymaga nazwy aliasu" );
         return false;
      }

      statement.name = value();
      ++m_pos;
   }

   return expectLineEnd( "oczekiwano konca deklaracji FIELD" );
}

/* ---------------------------------------------------------------------------------------------
   FIELD binding
   --------------------------------------------------------------------------------------------- */

/* LOCAL, STATIC and RECOVER USING names anywhere in the body */
static void s_variables( const std::vector<X_STMT> &body, X_NAME_SET &names )
{
   for( const X_STMT &stmt : body )
   {
      if( stmt.kind == X_STMT_KIND::LOCAL || stmt.kind == X_STMT_KIND::STATIC )
      {
         for( const X_LOCAL_DECL &decl : stmt.locals )
            names.insert( s_upper( decl.name.c_str() ) );
      }

      if( stmt.kind == X_STMT_KIND::SEQUENCE && !stmt.name.empty() )
         names.insert( s_upper( stmt.name.c_str() ) );

      s_variables( stmt.body, names );
      s_variables( stmt.elseBody, names );
      s_variables( stmt.otherwiseBody, names );

      for( const X_CASE_BRANCH &branch : stmt.cases )
         s_variables( branch.body, names );
   }
}

/* NAME becomes FIELD with the alias as a STRING selector, or NIL for the current area */
static void s_bindField( X_EXPR &expr, const X_FIELD_MAP &fields, bool areaContext )
{
   const auto field = fields.find( s_upper( expr.text.c_str() ) );
   X_EXPR_PTR selector;

   if( field == fields.end() && !areaContext )
      return;

   selector = s_makeExpr( X_EXPR_KIND::NIL );
   selector->line = expr.line;

   if( field != fields.end() && !field->second.empty() )
   {
      selector->kind = X_EXPR_KIND::STRING;
      selector->text = field->second;
   }

   expr.kind = X_EXPR_KIND::FIELD;
   expr.args.push_back( std::move( selector ) );
}

/* FIELD declarations of one body; a name is declared once */
bool X_PARSER::fieldDeclarations( const std::vector<X_STMT> &body, X_FIELD_MAP &fields, X_NAME_SET &declared )
{
   for( const X_STMT &stmt : body )
   {
      if( stmt.kind != X_STMT_KIND::FIELD )
         continue;

      for( const X_LOCAL_DECL &field : stmt.locals )
      {
         const std::string key = s_upper( field.name.c_str() );

         if( !declared.insert( key ).second )
         {
            fail( ( "powtorzona deklaracja FIELD: " + field.name ).c_str() );
            return false;
         }

         fields[key] = stmt.name;
      }
   }

   return true;
}

/* A FIELD may not have the name of a variable */
bool X_PARSER::fieldCollision( const X_NAME_SET &declared, const X_NAME_SET &names )
{
   for( const std::string &name : declared )
   {
      if( names.count( name ) )
      {
         fail( ( "FIELD koliduje ze zmienna: " + name ).c_str() );
         return true;
      }
   }

   return false;
}

/* A FIELD name, or any free name inside alias->( ... ), becomes a field access; shadow holds the variables */
bool X_PARSER::bindExpr( X_EXPR &expr, const X_FIELD_MAP &fields, const X_NAME_SET &shadow, bool areaContext )
{
   X_NAME_SET nested = shadow;
   size_t offset;

   if( expr.kind == X_EXPR_KIND::NAME && !shadow.count( s_upper( expr.text.c_str() ) ) )
      s_bindField( expr, fields, areaContext );

   if( expr.kind == X_EXPR_KIND::BLOCK )
   {
      for( const std::string &param : expr.blockParams )
         nested.insert( s_upper( param.c_str() ) );
   }

   for( size_t i = 0; i < expr.args.size(); ++i )
   {
      const bool inArea = areaContext || ( expr.kind == X_EXPR_KIND::ALIAS && i != 0 );

      if( !bindExpr( *expr.args[i], fields, nested, inArea ) )
         return false;
   }

   /* @ passes a variable slot; in a SEND args[0] is the receiver */
   offset = IIF( expr.kind == X_EXPR_KIND::SEND, 1, 0 );

   for( size_t i = 0; i < expr.argByReference.size(); ++i )
   {
      if( expr.argByReference[i] && expr.args[i + offset]->kind == X_EXPR_KIND::FIELD )
      {
         fail( "pole DBF nie jest slotem parametru @; uzyj FieldBlock()" );
         return false;
      }
   }

   return true;
}

bool X_PARSER::bindBody( std::vector<X_STMT> &body, const X_FIELD_MAP &fields, const X_NAME_SET &shadow )
{
   for( X_STMT &stmt : body )
   {
      X_EXPR *values[] = { stmt.expr.get(), stmt.target.get(), stmt.forLimit.get(), stmt.forStep.get() };

      for( X_EXPR *expr : values )
      {
         if( expr && !bindExpr( *expr, fields, shadow, false ) )
            return false;
      }

      for( X_LOCAL_DECL &decl : stmt.locals )
      {
         if( decl.init && !bindExpr( *decl.init, fields, shadow, false ) )
            return false;

         for( X_EXPR_PTR &dimension : decl.dimensions )
         {
            if( !bindExpr( *dimension, fields, shadow, false ) )
               return false;
         }
      }

      if( !bindBody( stmt.body, fields, shadow ) || !bindBody( stmt.elseBody, fields, shadow ) )
         return false;

      if( !bindBody( stmt.otherwiseBody, fields, shadow ) )
         return false;

      for( X_CASE_BRANCH &branch : stmt.cases )
      {
         if( branch.condition && !bindExpr( *branch.condition, fields, shadow, false ) )
            return false;

         if( !bindBody( branch.body, fields, shadow ) )
            return false;
      }
   }

   return true;
}

/* Parameters, Self, LOCAL and STATIC hide module FIELD names */
bool X_PARSER::bindFunctionFields( X_FUNCTION &function, const X_FIELD_MAP &moduleFields, const X_NAME_SET &statics )
{
   X_FIELD_MAP fields = moduleFields;
   X_NAME_SET declared;
   X_NAME_SET names = statics;

   for( const std::string &param : function.params )
      names.insert( s_upper( param.c_str() ) );

   if( !function.className.empty() )
      names.insert( "SELF" );

   s_variables( function.body, names );

   if( !fieldDeclarations( function.body, fields, declared ) || fieldCollision( declared, names ) )
      return false;

   if( !bindBody( function.body, fields, names ) )
      return false;

   for( X_LOCAL_DECL &decl : function.dataInitializers )
   {
      if( decl.init && !bindExpr( *decl.init, fields, names, false ) )
         return false;
   }

   return true;
}

/* FIELD names in the module and in every function become field accesses */
bool X_PARSER::bindFields( X_PROGRAM &program )
{
   X_FIELD_MAP moduleFields;
   X_NAME_SET moduleDeclared;
   X_NAME_SET moduleStatics;
   X_NAME_SET mainNames;

   if( !fieldDeclarations( program.statements, moduleFields, moduleDeclared ) )
      return false;

   for( const X_STMT &stmt : program.statements )
   {
      if( stmt.kind != X_STMT_KIND::STATIC )
         continue;

      for( const X_LOCAL_DECL &decl : stmt.locals )
         moduleStatics.insert( s_upper( decl.name.c_str() ) );
   }

   s_variables( program.statements, mainNames );

   if( fieldCollision( moduleDeclared, mainNames ) )
      return false;

   if( !bindBody( program.statements, moduleFields, mainNames ) )
      return false;

   for( X_FUNCTION &function : program.functions )
   {
      if( !bindFunctionFields( function, moduleFields, moduleStatics ) )
         return false;
   }

   return true;
}

/* ---------------------------------------------------------------------------------------------
   Statement
   --------------------------------------------------------------------------------------------- */

/* Expressions that can stand alone as a statement */
static bool s_isStatementExpr( const X_EXPR &expr )
{
   switch( expr.kind )
   {
   case X_EXPR_KIND::CALL:
   case X_EXPR_KIND::SEND:
   case X_EXPR_KIND::IIF:
   case X_EXPR_KIND::CAST:
   case X_EXPR_KIND::ALIAS:
   case X_EXPR_KIND::GROUP:
   case X_EXPR_KIND::UPDATE:
   case X_EXPR_KIND::ASSIGN: return true;
   case X_EXPR_KIND::UNARY:  return expr.text == "+";
   default:                  return false;
   }
}

/* A RETURN with a value anywhere in the body */
static bool s_hasReturnValue( const std::vector<X_STMT> &statements )
{
   for( const X_STMT &statement : statements )
   {
      if( statement.kind == X_STMT_KIND::RETURN && statement.expr != nullptr )
         return true;

      if( s_hasReturnValue( statement.body ) || s_hasReturnValue( statement.elseBody ) )
         return true;

      if( s_hasReturnValue( statement.otherwiseBody ) )
         return true;

      for( const X_CASE_BRANCH &branch : statement.cases )
      {
         if( s_hasReturnValue( branch.body ) )
            return true;
      }
   }

   return false;
}

bool X_PARSER::parseRecover( X_STMT &statement )
{
   ++m_pos;
   statement.hasOtherwise = true;

   if( at( "USING" ) )
   {
      ++m_pos;

      if( !is( TOK_IDENT ) )
      {
         fail( "RECOVER USING wymaga nazwy zmiennej" );
         return false;
      }

      statement.name = value();
      ++m_pos;

      if( !bindName( statement.name ) )
         return false;
   }

   if( !is( TOK_EOL ) )
   {
      fail( "oczekiwano konca linii po RECOVER" );
      return false;
   }

   return parseBlock( statement.elseBody, { "ENDSEQUENCE" } );
}

/* BEGIN SEQUENCE ... [RECOVER [USING var] ...] ENDSEQUENCE */
bool X_PARSER::parseSequence( X_STMT &statement )
{
   ++m_pos;

   if( !expect( "SEQUENCE" ) || !is( TOK_EOL ) )
   {
      fail( "oczekiwano BEGIN SEQUENCE i konca linii" );
      return false;
   }

   statement.kind = X_STMT_KIND::SEQUENCE;

   if( !parseBlock( statement.body, { "RECOVER", "ENDSEQUENCE" } ) )
      return false;

   if( at( "RECOVER" ) && !parseRecover( statement ) )
      return false;

   return expect( "ENDSEQUENCE" );
}

/* BREAK [value]; Break( x ) is a function call */
bool X_PARSER::parseBreak( X_STMT &statement )
{
   ++m_pos;
   statement.kind = X_STMT_KIND::BREAK;

   if( is( TOK_EOL ) || is( TOK_EOF ) )
      return true;

   statement.expr = parseExpression();

   return statement.expr != nullptr;
}

/* EXIT leaves a loop or a SWITCH, LOOP starts the next turn of a loop */
bool X_PARSER::parseExitLoop( X_STMT &statement )
{
   const bool isExit = is( TOK_EXIT );

   if( m_loopDepth <= 0 && ( !isExit || m_switchDepth <= 0 ) )
   {
      fail( IIF( isExit, "EXIT poza petla", "LOOP poza petla" ) );
      return false;
   }

   ++m_pos;
   statement.kind = IIF( isExit, X_STMT_KIND::EXIT, X_STMT_KIND::LOOP );

   return true;
}

/* PROCEDURE and METHOD accept an empty RETURN; a PROCEDURE always returns NIL */
bool X_PARSER::parseReturn( X_STMT &statement )
{
   ++m_pos;
   statement.kind = X_STMT_KIND::RETURN;

   if( is( TOK_EOL ) || is( TOK_EOF ) )
   {
      if( m_inProcedure || m_inMethod )
         return true;

      fail( "RETURN w FUNCTION wymaga wyrazenia" );
      return false;
   }

   if( m_inProcedure )
   {
      fail( "RETURN w PROCEDURE nie przyjmuje wyrazenia" );
      return false;
   }

   statement.expr = parseExpression();

   return statement.expr != nullptr;
}

/* DO CASE or DO WHILE; a procedure is called as name( args ) */
bool X_PARSER::parseDo( X_STMT &statement )
{
   if( peek( 1 ) == TOK_CASE )
      return parseDoCase( statement );

   if( peek( 1 ) != TOK_EOF && s_upper( value( 1 ) ) == "WHILE" )
      return parseWhile( statement );

   ++m_pos;
   fail( "po DO oczekiwano CASE albo WHILE; procedure wywoluje sie jako nazwa( argumenty )" );

   return false;
}

/* A call, a message, an assignment or ++ / -- used as a statement */
bool X_PARSER::parseExpressionStatement( X_STMT &statement )
{
   X_EXPR_PTR expr = parseExpression();

   if( !expr )
      return false;

   if( expr->kind == X_EXPR_KIND::BINARY && expr->text == "=" )
   {
      fail( "= porownuje; przypisanie to :=" );
      return false;
   }

   if( !s_isStatementExpr( *expr ) )
   {
      fail( "oczekiwano := albo wywolania funkcji" );
      return false;
   }

   statement.kind = X_STMT_KIND::CALL;
   statement.expr = std::move( expr );

   return true;
}

/* Words that cannot start a statement inside a function */
const char *X_PARSER::misplacedWord() const
{
   if( at( "RECOVER" ) )
      return "RECOVER poza BEGIN SEQUENCE";

   if( is( TOK_REQUEST ) )
      return "REQUEST wymaga sekcji deklaracji funkcji lub poziomu modulu";

   if( is( TOK_STATIC ) )
      return "STATIC wymaga sekcji deklaracji funkcji lub poziomu modulu";

   if( is( TOK_INIT ) || ( is( TOK_EXIT ) && peek( 1 ) == TOK_PROCEDURE ) )
      return "INIT/EXIT PROCEDURE wymaga poziomu modulu";

   /* FIELD->name is an expression */
   if( is( TOK_FIELD ) && peek( 1 ) != TOK_ALIAS )
      return "FIELD wymaga sekcji deklaracji funkcji lub poziomu modulu";

   if( is( TOK_ENDSWITCH ) || is( TOK_END ) )
      return "END/ENDSWITCH poza SWITCH";

   if( is( TOK_NEXT ) )
      return "NEXT poza FOR";

   if( is( TOK_CASE ) )
      return "CASE poza DO CASE";

   if( is( TOK_OTHERWISE ) )
      return "OTHERWISE poza DO CASE";

   if( is( TOK_ENDCASE ) )
      return "ENDCASE bez DO CASE";

   return nullptr;
}

bool X_PARSER::parseStatement( X_STMT &statement )
{
   const char *misplaced;

   if( statement.line == 0 && m_pos < m_tokens.count )
      statement.line = m_tokens.items[m_pos].line;

   if( at( "BEGIN" ) )
      return parseSequence( statement );

   if( at( "BREAK" ) && peek( 1 ) != TOK_LPAREN )
      return parseBreak( statement );

   misplaced = misplacedWord();
   if( misplaced )
   {
      fail( misplaced );
      return false;
   }

   switch( peek( 0 ) )
   {
   case TOK_LOCAL:  return parseLocal( statement );
   case TOK_IF:     return parseIf( statement );
   case TOK_DO:     return parseDo( statement );
   case TOK_WHILE:  return parseWhile( statement );
   case TOK_FOR:    return parseFor( statement );
   case TOK_SWITCH: return parseSwitch( statement );
   case TOK_EXIT:
   case TOK_LOOP:   return parseExitLoop( statement );
   case TOK_RETURN: return parseReturn( statement );
   default:         return parseExpressionStatement( statement );
   }
}

/* ---------------------------------------------------------------------------------------------
   Functions
   --------------------------------------------------------------------------------------------- */

/* Names starting with __XDEV belong to the compiler */
static bool s_isInternal( const std::string &name )
{
   return s_upper( name.c_str() ).rfind( "__XDEV", 0 ) == 0;
}

static bool s_hasParam( const std::vector<std::string> &params, const std::string &key )
{
   for( const std::string &previous : params )
   {
      if( s_upper( previous.c_str() ) == key )
         return true;
   }

   return false;
}

/* A name already used by a type, a constant or a variable */
bool X_PARSER::nameTaken( const std::string &upper ) const
{
   if( s_isInternal( upper ) || m_callbackTypes.count( upper ) || m_structTypes.count( upper ) )
      return true;

   return m_classTypes.count( upper ) || m_enums.count( upper ) || m_names.count( upper );
}

/* AS type after the parameters */
bool X_PARSER::parseResult( std::optional<X_DECL_TYPE> &result, std::string &typeName )
{
   X_DECL_TYPE type;

   if( !is( TOK_AS ) || !parseType( type, &typeName ) )
      return false;

   result = type;

   return true;
}

/* CALLBACK FUNCTION or CALLBACK PROCEDURE */
bool X_PARSER::atCallback() const
{
   std::string kind;

   if( !at( "CALLBACK" ) || peek( 1 ) == TOK_EOF )
      return false;

   kind = s_upper( value( 1 ) );

   return kind == "FUNCTION" || kind == "PROCEDURE";
}

/* p AS type, ...: every CALLBACK parameter has a type */
bool X_PARSER::parseCallbackParams( X_CALLBACK_DECL &callback )
{
   while( !at( ")" ) )
   {
      std::string name;
      std::string key;
      std::string typeName;
      X_DECL_TYPE type;

      if( !is( TOK_IDENT ) )
      {
         fail( "CALLBACK wymaga parametru z AS" );
         return false;
      }

      name = value();
      key = s_upper( name.c_str() );
      ++m_pos;

      if( s_hasParam( callback.params, key ) )
      {
         fail( "powtorzona nazwa parametru CALLBACK" );
         return false;
      }

      if( !is( TOK_AS ) || !parseType( type, &typeName ) )
      {
         fail( "CALLBACK wymaga typu parametru po AS" );
         return false;
      }

      callback.params.push_back( name );
      callback.paramTypes.emplace( key, type );
      callback.paramTypeNames.emplace( key, std::move( typeName ) );

      if( !match( TOK_COMMA ) )
         break;

      if( at( ")" ) )
      {
         fail( "oczekiwano parametru CALLBACK po przecinku" );
         return false;
      }
   }

   return true;
}

/* CALLBACK FUNCTION Name( p AS type, ... ) AS type: the signature of a C function pointer */
bool X_PARSER::parseCallback( X_PROGRAM &program )
{
   X_CALLBACK_DECL callback;
   const char *kind;

   callback.line = m_tokens.items[m_pos].line;
   ++m_pos;
   callback.isProcedure = at( "PROCEDURE" );
   kind = IIF( callback.isProcedure, "PROCEDURE", "FUNCTION" );

   if( !expect( kind ) || !is( TOK_IDENT ) )
   {
      fail( "CALLBACK FUNCTION/PROCEDURE wymaga nazwy" );
      return false;
   }

   callback.name = s_upper( value() );
   ++m_pos;

   if( nameTaken( callback.name ) )
   {
      fail( ( "powtorzona albo kolidujaca nazwa CALLBACK: " + callback.name ).c_str() );
      return false;
   }

   if( !expect( "(" ) || !parseCallbackParams( callback ) || !expect( ")" ) )
      return false;

   if( callback.isProcedure && is( TOK_AS ) )
   {
      fail( "CALLBACK PROCEDURE nie przyjmuje typu wyniku po AS" );
      return false;
   }

   if( !callback.isProcedure && !parseResult( callback.resultType, callback.resultTypeName ) )
   {
      fail( "CALLBACK FUNCTION wymaga typu wyniku po AS" );
      return false;
   }

   if( !expectLineEnd( "oczekiwano konca linii po CALLBACK" ) )
      return false;

   m_callbackTypes.insert( callback.name );
   m_names.insert( callback.name );
   program.callbacks.push_back( std::move( callback ) );

   return true;
}

/* DLL [STATIC] FUNCTION or DLL [STATIC] PROCEDURE */
bool X_PARSER::atDllFunction() const
{
   int next = 1;
   std::string kind;

   if( !at( "DLL" ) || peek( 1 ) == TOK_EOF )
      return false;

   if( s_upper( value( 1 ) ) == "STATIC" )
      next = 2;

   if( peek( next ) == TOK_EOF )
      return false;

   kind = s_upper( value( next ) );

   return kind == "FUNCTION" || kind == "PROCEDURE";
}

/* ... as three dots */
bool X_PARSER::atEllipsis() const
{
   if( !at( "." ) || peek( 2 ) == TOK_EOF )
      return false;

   return std::strcmp( value( 1 ), "." ) == 0 && std::strcmp( value( 2 ), "." ) == 0;
}

/* p AS type, ...: every DLL parameter has a C type; ... is not allowed */
bool X_PARSER::parseDllParams( X_DLL_FUNCTION &function )
{
   while( !at( ")" ) )
   {
      std::string name;
      std::string key;
      std::string typeName;
      X_DECL_TYPE type;

      if( atEllipsis() )
      {
         fail( "DLL FUNCTION/PROCEDURE nie obsluguje teraz zmiennej liczby argumentow (...)" );
         return false;
      }

      if( !is( TOK_IDENT ) )
      {
         fail( "DLL FUNCTION/PROCEDURE wymaga parametru z AS" );
         return false;
      }

      name = value();
      key = s_upper( name.c_str() );
      ++m_pos;

      if( !bindName( name ) || !is( TOK_AS ) || !parseType( type, &typeName ) )
      {
         fail( "DLL FUNCTION/PROCEDURE wymaga typu parametru po AS" );
         return false;
      }

      function.paramTypeNames.emplace( key, typeName );

      if( !function.paramTypes.emplace( key, type ).second )
      {
         fail( "powtorzona nazwa parametru DLL FUNCTION/PROCEDURE" );
         return false;
      }

      function.params.push_back( name );

      if( !match( TOK_COMMA ) )
         break;

      if( at( ")" ) )
      {
         fail( "oczekiwano parametru po przecinku" );
         return false;
      }
   }

   return true;
}

/* [PASCAL] [FROM symbol] LIB "file.dll"; the declaration ends the line */
bool X_PARSER::parseDllLibrary( X_DLL_FUNCTION &function )
{
   if( at( "PASCAL" ) )
      ++m_pos;

   if( at( "FROM" ) )
   {
      ++m_pos;

      if( !is( TOK_IDENT ) && !is( TOK_STR ) )
      {
         fail( "FROM wymaga nazwy symbolu DLL" );
         return false;
      }

      function.symbol = value();
      ++m_pos;
   }

   if( !expect( "LIB" ) || !is( TOK_STR ) )
   {
      fail( "LIB wymaga nazwy pliku DLL w cudzyslowie" );
      return false;
   }

   function.library = value();
   ++m_pos;

   if( function.library.empty() || function.symbol.empty() || ( !is( TOK_EOL ) && !is( TOK_EOF ) ) )
   {
      fail( "niepoprawna deklaracja DLL FUNCTION" );
      return false;
   }

   skipLines();

   return true;
}

/* DLL [STATIC] FUNCTION Name( p AS type, ... ) AS type [PASCAL] [FROM symbol] LIB "file.dll" */
bool X_PARSER::parseDllFunction( X_DLL_FUNCTION &function )
{
   const char *kind;

   function.isDll = true;
   function.line = m_tokens.items[m_pos].line;
   ++m_pos;
   function.isStatic = at( "STATIC" );

   if( function.isStatic )
      ++m_pos;

   function.isProcedure = at( "PROCEDURE" );
   kind = IIF( function.isProcedure, "PROCEDURE", "FUNCTION" );

   if( !expect( kind ) || !is( TOK_IDENT ) )
   {
      fail( "DLL FUNCTION/PROCEDURE wymaga nazwy" );
      return false;
   }

   function.name = value();
   function.symbol = function.name;
   ++m_pos;

   if( !bindName( function.name ) || s_isInternal( function.name ) || !expect( "(" ) )
   {
      fail( "niepoprawna nazwa DLL FUNCTION/PROCEDURE" );
      return false;
   }

   if( !parseDllParams( function ) || !expect( ")" ) )
      return false;

   if( function.isProcedure && is( TOK_AS ) )
   {
      fail( "DLL PROCEDURE nie przyjmuje typu wyniku po AS" );
      return false;
   }

   if( !function.isProcedure && !parseResult( function.resultType, function.resultTypeName ) )
   {
      fail( "DLL FUNCTION wymaga typu wyniku po AS" );
      return false;
   }

   return parseDllLibrary( function );
}

bool X_PARSER::atMethodWord() const
{
   if( at( "METHOD" ) || at( "CLASSMETHOD" ) || at( "ACCESS" ) )
      return true;

   return at( "ASSIGN" ) || at( "DESTRUCTOR" ) || at( "ONERROR" );
}

/* The next function, class or module declaration ends the body of the current function */
bool X_PARSER::atFunctionEnd() const
{
   std::string next;

   if( is( TOK_EOF ) || atCallback() || atDllFunction() || atMethodWord() )
      return true;

   if( at( "FUNCTION" ) || at( "PROCEDURE" ) || at( "INIT" ) || at( "ENUM" ) )
      return true;

   if( peek( 1 ) != TOK_EOF )
      next = s_upper( value( 1 ) );

   if( at( "CREATE" ) && next == "CLASS" )
      return true;

   if( at( "EXIT" ) && ( next == "PROCEDURE" || next == "FUNCTION" ) )
      return true;

   return at( "STATIC" ) && ( next == "FUNCTION" || next == "PROCEDURE" );
}

/* ( p [AS type], ... ): unique names */
bool X_PARSER::parseParams( X_FUNCTION &function )
{
   ++m_pos;

   if( at( ")" ) )
      return expect( ")" );

   while( true )
   {
      std::string parameter;

      if( !is( TOK_IDENT ) )
      {
         fail( "oczekiwano parametru" );
         return false;
      }

      parameter = value();
      ++m_pos;

      if( !bindName( parameter ) )
         return false;

      if( s_hasParam( function.params, s_upper( parameter.c_str() ) ) )
      {
         fail( "powtorzona nazwa parametru" );
         return false;
      }

      function.params.push_back( parameter );

      if( is( TOK_AS ) )
      {
         const std::string key = s_upper( parameter.c_str() );
         X_DECL_TYPE type;
         std::string typeName;

         if( !parseType( type, &typeName ) )
            return false;

         function.paramTypes.emplace( key, type );
         function.paramTypeNames.emplace( key, std::move( typeName ) );
      }

      if( !at( "," ) )
         break;

      ++m_pos;
   }

   return expect( ")" );
}

/* METHOD name ... CLASS cls: the method body is the function __XDEV_CLS_KIND_NAME */
bool X_PARSER::methodClass( X_FUNCTION &function )
{
   if( !expect( "CLASS" ) || !is( TOK_IDENT ) )
   {
      fail( "metoda wymaga CLASS i nazwy klasy" );
      return false;
   }

   function.className = s_upper( value() );
   ++m_pos;
   function.methodName = s_upper( function.name.c_str() );
   function.name = "__XDEV_" + function.className + "_" + function.methodKind + "_" + function.methodName;
   function.isStatic = true;

   return true;
}

/* Declarations allowed only before the first executable statement; nullptr for any other statement */
const char *X_PARSER::lateDeclaration() const
{
   if( is( TOK_REQUEST ) )
      return "REQUEST musi wystapic przed instrukcjami wykonywalnymi";

   if( is( TOK_FIELD ) && m_pos + 1 < m_tokens.count && peek( 1 ) != TOK_ALIAS )
      return "FIELD musi wystapic przed instrukcjami wykonywalnymi";

   if( at( "STATIC" ) )
      return "STATIC musi wystapic przed instrukcjami wykonywalnymi w funkcji";

   return nullptr;
}

/* STATIC, FIELD and REQUEST are parsed here, outside parseStatement(), so nested blocks cannot hold them */
bool X_PARSER::parseBodyStatement( X_FUNCTION &function, bool &sawExecutable )
{
   const char *late = lateDeclaration();
   X_STMT statement;
   bool ok;

   if( is( TOK_ANNOUNCE ) )
   {
      fail( "ANNOUNCE musi wystapic na poziomie modulu, poza funkcja" );
      return false;
   }

   statement.line = IIF( m_pos < m_tokens.count, m_tokens.items[m_pos].line, 0 );

   if( late && sawExecutable )
   {
      fail( late );
      return false;
   }

   if( !late )
      ok = parseStatement( statement );
   else if( is( TOK_REQUEST ) )
      ok = parseRequest( statement );
   else if( is( TOK_FIELD ) )
      ok = parseFieldDeclaration( statement );
   else
      ok = parseStatic( statement );

   if( !ok )
      return false;

   if( !late && statement.kind != X_STMT_KIND::LOCAL )
      sawExecutable = true;

   function.body.push_back( std::move( statement ) );

   if( !expectLineEnd( "oczekiwano konca linii" ) )
      return false;

   skipLines();

   return true;
}

/* The hidden locals of FOR EACH come at the head of the function */
void X_PARSER::eachLocals( X_FUNCTION &function )
{
   X_STMT hidden;

   if( m_enumLocals.empty() )
      return;

   hidden.kind = X_STMT_KIND::LOCAL;
   hidden.line = function.line;

   for( const std::string &name : m_enumLocals )
   {
      X_LOCAL_DECL decl;

      decl.name = name;
      decl.line = function.line;
      hidden.locals.push_back( std::move( decl ) );
   }

   function.body.insert( function.body.begin(), std::move( hidden ) );
}

/* Statements up to the next declaration */
bool X_PARSER::parseFunctionBody( X_FUNCTION &function, bool method )
{
   const bool previousInProcedure = m_inProcedure;
   const bool previousInMethod = m_inMethod;
   std::vector<std::string> outerEnumLocals = std::move( m_enumLocals );
   bool sawExecutable = false;
   bool ok = true;

   m_inProcedure = function.isProcedure;
   m_inMethod = method;
   m_enumLocals.clear();

   while( ok && !atFunctionEnd() )
   {
      const int start = m_pos;

      if( !parseBodyStatement( function, sawExecutable ) )
         ok = recover( start );
   }

   if( ok )
      eachLocals( function );

   m_enumLocals = std::move( outerEnumLocals );
   m_inProcedure = previousInProcedure;
   m_inMethod = previousInMethod;

   return ok;
}

/* FUNCTION, PROCEDURE and method bodies; FUNCTION and PROCEDURE share the syntax and the X_ITEM ABI */
bool X_PARSER::parseFunction( X_FUNCTION &function, bool isStatic )
{
   const bool method = atMethodWord();

   function.line = m_tokens.items[m_pos].line;

   if( method )
      function.methodKind = s_upper( value() );

   function.isProcedure = at( "PROCEDURE" );
   function.isStatic = isStatic;
   ++m_pos;

   if( !IIF( function.methodKind == "METHOD", isMemberName(), is( TOK_IDENT ) ) )
   {
      fail( "oczekiwano nazwy funkcji" );
      return false;
   }

   function.name = value();
   ++m_pos;

   if( !method && !bindName( function.name ) )
      return false;

   if( s_isInternal( function.name ) )
   {
      fail( "zastrzezona nazwa funkcji lub metody" );
      return false;
   }

   if( at( "(" ) && !parseParams( function ) )
      return false;

   if( is( TOK_AS ) )
   {
      if( function.isProcedure )
      {
         fail( "PROCEDURE nie przyjmuje typu wyniku po AS" );
         return false;
      }

      if( !parseResult( function.resultType, function.resultTypeName ) )
         return false;
   }

   if( method && !methodClass( function ) )
      return false;

   if( !expectLineEnd( "oczekiwano konca linii" ) )
      return false;

   skipLines();

   const size_t errors = m_diagnostics.size();

   if( !parseFunctionBody( function, method ) )
      return false;

   /* A PROCEDURE returns NIL after an empty RETURN; a body with errors may have lost its RETURN */
   if( errors == m_diagnostics.size() && !function.isProcedure && !method && !s_hasReturnValue( function.body ) )
   {
      fail( "FUNCTION musi zawierac RETURN z wyrazeniem" );
      return false;
   }

   return true;
}

/* ---------------------------------------------------------------------------------------------
   Classes
   --------------------------------------------------------------------------------------------- */

struct X_OPERATOR_INFO
{
   X_TOKTYPE token;
   int id;
   const char *message;
};

/* Operators a METHOD can overload */
static const X_OPERATOR_INFO s_overloads[] = {
   { TOK_PLUS,     X_OO_OP_PLUS,         "__OPPLUS"         },
   { TOK_MINUS,    X_OO_OP_MINUS,        "__OPMINUS"        },
   { TOK_MUL,      X_OO_OP_MULT,         "__OPMULT"         },
   { TOK_DIV,      X_OO_OP_DIVIDE,       "__OPDIVIDE"       },
   { TOK_MOD,      X_OO_OP_MOD,          "__OPMOD"          },
   { TOK_POWER,    X_OO_OP_POWER,        "__OPPOWER"        },
   { TOK_INC,      X_OO_OP_INC,          "__OPINC"          },
   { TOK_DEC,      X_OO_OP_DEC,          "__OPDEC"          },
   { TOK_EQ,       X_OO_OP_EQUAL,        "__OPEQUAL"        },
   { TOK_EXACT_EQ, X_OO_OP_EXACTEQUAL,   "__OPEXACTEQUAL"   },
   { TOK_NE,       X_OO_OP_NOTEQUAL,     "__OPNOTEQUAL"     },
   { TOK_LT,       X_OO_OP_LESS,         "__OPLESS"         },
   { TOK_LE,       X_OO_OP_LESSEQUAL,    "__OPLESSEQUAL"    },
   { TOK_GT,       X_OO_OP_GREATER,      "__OPGREATER"      },
   { TOK_GE,       X_OO_OP_GREATEREQUAL, "__OPGREATEREQUAL" },
   { TOK_NOT,      X_OO_OP_NOT,          "__OPNOT"          },
   { TOK_AND,      X_OO_OP_AND,          "__OPAND"          },
   { TOK_OR,       X_OO_OP_OR,           "__OPOR"           },
   { TOK_ASSIGN,   X_OO_OP_ASSIGN,       "__OPASSIGN"       },
   { TOK_LBRACKET, X_OO_OP_ARRAYINDEX,   "__OPARRAYINDEX"   },
   { TOK_CONTAINS, X_OO_OP_INSTRING,     "__OPINSTRING"     }
};

/* Words that start a member declaration; ON ERROR is stored as ONERROR */
static const char *s_memberKinds[] = {
   "DATA", "CLASSDATA", "METHOD", "CLASSMETHOD", "ACCESS", "ASSIGN",
   "DESTRUCTOR", "VIRTUAL", "ONERROR", "DELEGATE"
};

static bool s_isMemberKind( const std::string &kind )
{
   for( const char *word : s_memberKinds )
   {
      if( kind == word )
         return true;
   }

   return false;
}

/* Types a DATA can hold */
static bool s_dataType( X_DECL_TYPE type )
{
   return type != X_DECL_TYPE::STRUCT && type != X_DECL_TYPE::PSZ && type != X_DECL_TYPE::CALLBACK;
}

static bool s_isData( const X_CLASS_MEMBER &member )
{
   return member.kind == "DATA" || member.kind == "CLASSDATA";
}

static uint32_t s_scope( const std::string &word )
{
   if( word == "HIDDEN" )
      return X_OO_CLSTP_HIDDEN;

   if( word == "PROTECTED" )
      return X_OO_CLSTP_PROTECTED;

   return X_OO_CLSTP_EXPORTED;
}

/* The message of a member inside the class: __ACCESS_name, __ASSIGN_name, __DESTRUCTOR, __ONERROR or name */
static std::string s_memberMessage( const X_CLASS_MEMBER &member )
{
   if( member.kind == "ACCESS" )
      return "__ACCESS_" + member.name;

   if( member.kind == "ASSIGN" )
      return "__ASSIGN_" + member.name;

   if( member.kind == "DESTRUCTOR" )
      return "__DESTRUCTOR";

   if( member.kind == "ONERROR" )
      return "__ONERROR";

   return member.name;
}

/* The message seen from outside: ASSIGN answers _name */
static std::string s_publicMessage( const X_CLASS_MEMBER &member )
{
   if( member.kind == "ASSIGN" )
      return "_" + member.name;

   if( member.kind == "DESTRUCTOR" )
      return "__DESTRUCTOR";

   if( member.kind == "ONERROR" )
      return "__ONERROR";

   return member.name;
}

/* The start value of a typed DATA without INIT: 0, .F. or ""; nullptr for other types */
static X_EXPR_PTR s_defaultValue( X_DECL_TYPE type, int line )
{
   X_EXPR_PTR value;

   if( type == X_DECL_TYPE::LOGIC )
      value = s_makeExpr( X_EXPR_KIND::LOGICAL );
   else if( type == X_DECL_TYPE::STRING )
      value = s_makeExpr( X_EXPR_KIND::STRING );
   else if( type >= X_DECL_TYPE::INT8 && type <= X_DECL_TYPE::FLOAT64 )
   {
      value = s_makeExpr( X_EXPR_KIND::NUMBER );
      value->text = "0";
   }
   else
      return nullptr;

   value->line = line;
   value->numberIsInteger = type != X_DECL_TYPE::FLOAT32 && type != X_DECL_TYPE::FLOAT64;

   return value;
}

/* CLASSDATA: a function that returns the start value */
static X_FUNCTION s_classDataInit( const std::string &className, const std::string &memberName, X_EXPR_PTR init )
{
   X_FUNCTION function;
   X_STMT valueReturn;

   function.name = "__XDEV_" + className + "_CLASSDATA_" + memberName;
   function.isStatic = true;
   function.isClassDataInitializer = true;

   valueReturn.kind = X_STMT_KIND::RETURN;
   valueReturn.line = IIF( init, init->line, 0 );
   valueReturn.expr = std::move( init );
   function.body.push_back( std::move( valueReturn ) );

   return function;
}

static const X_OPERATOR_INFO *s_findOverload( X_TOKTYPE token )
{
   for( const X_OPERATOR_INFO &info : s_overloads )
   {
      if( info.token == token )
         return &info;
   }

   return nullptr;
}

/* Unary operators take no parameter, binary ones take one; - may be unary, [] takes two */
static bool s_operatorParams( const X_CLASS_MEMBER &member )
{
   const int id = member.operatorId;
   const bool unary = id == X_OO_OP_INC || id == X_OO_OP_DEC || id == X_OO_OP_NOT;

   if( member.params.size() == IIF( unary, 0u, 1u ) )
      return true;

   if( id == X_OO_OP_MINUS && member.params.empty() )
      return true;

   return id == X_OO_OP_ARRAYINDEX && member.params.size() == 2;
}

/* ACCESS and DESTRUCTOR take no parameter, ASSIGN takes one */
static bool s_accessorParams( const X_CLASS_MEMBER &member )
{
   if( member.kind == "ACCESS" || member.kind == "DESTRUCTOR" )
      return member.params.empty();

   if( member.kind == "ASSIGN" )
      return member.params.size() == 1;

   return true;
}

/* CREATE CLASS name [FROM parents]; m_pos is after CREATE */
bool X_PARSER::parseClassHead( X_CLASS_DECL &cls, const X_PROGRAM &program )
{
   if( !expect( "CLASS" ) || !is( TOK_IDENT ) )
   {
      fail( "CREATE CLASS wymaga nazwy klasy" );
      return false;
   }

   cls.line = m_tokens.items[m_pos].line;
   cls.name = s_upper( value() );
   ++m_pos;

   if( !bindName( cls.name ) )
      return false;

   if( cls.name == "MAIN" || cls.name == "SELF" || cls.name == "SUPER" || s_isInternal( cls.name ) )
   {
      fail( "zastrzezona nazwa klasy" );
      return false;
   }

   m_classTypes.insert( cls.name );

   for( const X_CLASS_DECL &other : program.classes )
   {
      if( other.name == cls.name )
      {
         fail( "powtorzona deklaracja klasy" );
         return false;
      }
   }

   if( at( "FROM" ) )
   {
      ++m_pos;

      do
      {
         if( !is( TOK_IDENT ) )
         {
            fail( "oczekiwano nazwy klasy bazowej" );
            return false;
         }

         cls.parents.push_back( s_upper( value() ) );
         ++m_pos;
      } while( match( TOK_COMMA ) );
   }

   if( !is( TOK_EOL ) )
   {
      fail( "oczekiwano konca deklaracji klasy" );
      return false;
   }

   skipLines();

   return true;
}

/* The kind and the name of a member; the messages of the class stay unique */
bool X_PARSER::parseMemberName( X_CLASS_BUILD &build, X_CLASS_MEMBER &member )
{
   member.kind = s_upper( value() );
   ++m_pos;

   /* ON ERROR */
   if( member.kind == "ON" )
   {
      if( !expect( "ERROR" ) )
         return false;

      member.kind = "ONERROR";
   }

   if( !s_isMemberKind( member.kind ) )
   {
      std::string message = "oczekiwano DATA, CLASSDATA, METHOD, CLASSMETHOD, VIRTUAL, ACCESS, ASSIGN, DESTRUCTOR, ";

      message += "ON ERROR lub DELEGATE";
      fail( message.c_str() );
      return false;
   }

   if( !IIF( member.kind == "METHOD" || member.kind == "DATA", isMemberName(), is( TOK_IDENT ) ) )
   {
      fail( "oczekiwano nazwy skladowej klasy" );
      return false;
   }

   member.name = s_upper( value() );
   ++m_pos;

   if( s_isInternal( member.name ) || member.name == "SELF" || member.name == "SUPER" )
   {
      fail( "zastrzezona nazwa skladowej" );
      return false;
   }

   if( !build.messages.insert( s_memberMessage( member ) ).second )
   {
      fail( "powtorzona skladowa klasy" );
      return false;
   }

   if( !build.publicMessages.insert( s_publicMessage( member ) ).second )
   {
      fail( "kolizja nazw pola, metody lub accessora klasy" );
      return false;
   }

   /* DATA also answers the assignment message _name */
   if( s_isData( member ) && !build.publicMessages.insert( "_" + member.name ).second )
   {
      fail( "kolizja nazw pola, metody lub accessora klasy" );
      return false;
   }

   return true;
}

/* DATA name [AS type] [INIT value]; DATA goes to the initializer of the class, CLASSDATA gets its own function */
bool X_PARSER::parseData( X_CLASS_BUILD &build, X_CLASS_MEMBER &member, X_PROGRAM &program )
{
   X_LOCAL_DECL data;

   data.name = member.name;
   data.line = m_tokens.items[m_pos - 1].line;

   if( is( TOK_AS ) )
   {
      if( !parseType( member.type, &member.typeName ) )
         return false;

      if( !s_dataType( member.type ) )
      {
         fail( "DATA i CLASSDATA nie przyjmuja typu STRUCT, PSZ ani CALLBACK" );
         return false;
      }

      data.declaredType = member.type;
      data.declaredTypeName = member.typeName;
   }

   if( at( "INIT" ) )
   {
      ++m_pos;
      data.init = parseExpression();
      if( !data.init )
         return false;
   }
   else
      data.init = s_defaultValue( member.type, data.line );

   if( member.kind == "DATA" )
      build.initializer.dataInitializers.push_back( std::move( data ) );
   else
      program.functions.push_back( s_classDataInit( build.cls.name, member.name, std::move( data.init ) ) );

   return true;
}

/* ( p, ... ) of a method: unique names, not Self or Super */
bool X_PARSER::parseMemberParams( X_CLASS_MEMBER &member )
{
   if( !is( TOK_RPAREN ) )
   {
      do
      {
         std::string parameter;

         if( !is( TOK_IDENT ) )
         {
            fail( "oczekiwano parametru metody" );
            return false;
         }

         parameter = s_upper( value() );
         ++m_pos;

         if( parameter == "SELF" || parameter == "SUPER" )
         {
            fail( "zastrzezona nazwa parametru metody" );
            return false;
         }

         if( s_hasParam( member.params, parameter ) )
         {
            fail( "powtorzona nazwa parametru metody" );
            return false;
         }

         member.params.push_back( parameter );
      } while( match( TOK_COMMA ) );
   }

   if( !match( TOK_RPAREN ) )
   {
      fail( "oczekiwano )" );
      return false;
   }

   return true;
}

/* METHOD name( p ) OPERATOR op: the method overloads an operator */
bool X_PARSER::parseOperator( X_CLASS_BUILD &build, X_CLASS_MEMBER &member )
{
   const X_OPERATOR_INFO *info;

   if( member.kind != "METHOD" )
   {
      fail( "OPERATOR jest dozwolone tylko dla METHOD" );
      return false;
   }

   ++m_pos;

   info = s_findOverload( peek( 0 ) );
   if( !info )
   {
      fail( "operator nieobslugiwany przez przeciazanie w .prg" );
      return false;
   }

   member.operatorId = info->id;
   member.operatorMessage = info->message;
   member.operatorSymbol = s_upper( value() );

   if( info->id == X_OO_OP_ARRAYINDEX )
   {
      member.operatorSymbol = "[]";
      ++m_pos;

      if( !is( TOK_RBRACKET ) )
      {
         fail( "oczekiwano ] w OPERATOR []" );
         return false;
      }
   }

   ++m_pos;

   if( !s_operatorParams( member ) )
   {
      fail( "nieprawidlowa liczba parametrow metody operatora" );
      return false;
   }

   if( member.operatorMessage == member.name )
      return true;

   if( !build.messages.insert( member.operatorMessage ).second )
   {
      fail( "powtorzony operator lub kolizja z nazwa skladowej klasy" );
      return false;
   }

   if( !build.publicMessages.insert( member.operatorMessage ).second )
   {
      fail( "powtorzony operator lub kolizja z nazwa skladowej klasy" );
      return false;
   }

   return true;
}

/* DELEGATE name TO object */
bool X_PARSER::parseDelegate( X_CLASS_MEMBER &member )
{
   member.delegateMessage = member.name;

   if( !expect( "TO" ) )
      return false;

   if( !is( TOK_IDENT ) )
   {
      fail( "oczekiwano nazwy skladowej zwracajacej obiekt po TO" );
      return false;
   }

   member.delegateObject = s_upper( value() );
   ++m_pos;

   if( member.delegateObject == "SELF" || member.delegateObject == "SUPER" )
   {
      fail( "zastrzezona nazwa w delegacji" );
      return false;
   }

   if( s_isInternal( member.delegateObject ) || s_isInternal( member.delegateMessage ) )
   {
      fail( "zastrzezona nazwa w delegacji" );
      return false;
   }

   return true;
}

/* READONLY and SHARED (CLASSDATA only), each once */
bool X_PARSER::parseModifiers( X_CLASS_MEMBER &member )
{
   while( at( "READONLY" ) || at( "SHARED" ) )
   {
      const bool shared = at( "SHARED" );
      const uint32_t flag = IIF( shared, X_OO_CLSTP_SHARED, X_OO_CLSTP_READONLY );

      if( shared && member.kind != "CLASSDATA" )
      {
         fail( "SHARED jest dozwolone tylko dla CLASSDATA" );
         return false;
      }
      else if( member.scope & flag )
      {
         fail( IIF( shared, "powtorzony modyfikator SHARED", "powtorzony modyfikator READONLY" ) );
         return false;
      }
      else
         member.scope |= flag;

      ++m_pos;
   }

   return true;
}

/* kind name [( params )] [AS type] [INIT value] [OPERATOR op] [TO object] [modifiers] */
bool X_PARSER::parseMember( X_CLASS_BUILD &build, X_CLASS_MEMBER &member, X_PROGRAM &program )
{
   if( !parseMemberName( build, member ) )
      return false;

   if( s_isData( member ) )
   {
      if( !parseData( build, member, program ) )
         return false;
   }
   else if( match( TOK_LPAREN ) && !parseMemberParams( member ) )
      return false;

   if( at( "OPERATOR" ) && !parseOperator( build, member ) )
      return false;

   if( member.kind == "DELEGATE" && !parseDelegate( member ) )
      return false;

   if( !s_accessorParams( member ) )
   {
      fail( "nieprawidlowa liczba parametrow accessora lub destruktora" );
      return false;
   }

   if( member.kind == "ONERROR" && member.params.size() != 2 )
   {
      fail( "ON ERROR wymaga dwoch parametrow: nazwy komunikatu i tablicy argumentow" );
      return false;
   }

   return parseModifiers( member );
}

/* One line of the class: a scope word or a member */
bool X_PARSER::parseClassLine( X_CLASS_BUILD &build, X_PROGRAM &program )
{
   if( at( "EXPORTED" ) || at( "PROTECTED" ) || at( "HIDDEN" ) )
   {
      build.scope = s_scope( s_upper( value() ) );
      ++m_pos;
      match( TOK_SEND );
   }
   else
   {
      X_CLASS_MEMBER member;

      member.scope = build.scope;

      if( !parseMember( build, member, program ) )
         return false;

      build.cls.members.push_back( std::move( member ) );
   }

   if( !is( TOK_EOL ) )
   {
      fail( "oczekiwano konca deklaracji skladowej" );
      return false;
   }

   skipLines();

   return true;
}

/* CREATE CLASS ... ENDCLASS: the members, a factory function and the DATA initializer */
bool X_PARSER::parseClass( X_PROGRAM &program )
{
   X_CLASS_BUILD build;
   X_FUNCTION factory;

   ++m_pos;

   if( !parseClassHead( build.cls, program ) )
      return false;

   build.scope = X_OO_CLSTP_EXPORTED;
   build.initializer.name = "__XDEV_" + build.cls.name + "_INITDATA";
   build.initializer.className = build.cls.name;
   build.initializer.isStatic = true;
   build.initializer.isProcedure = true;
   build.initializer.isDataInitializer = true;

   while( !is( TOK_EOF ) && !at( "ENDCLASS" ) )
   {
      if( !parseClassLine( build, program ) )
         return false;
   }

   if( !expect( "ENDCLASS" ) || !expectLineEnd( "oczekiwano konca linii po ENDCLASS" ) )
      return false;

   factory.name = build.cls.name;
   factory.factoryClass = build.cls.name;
   factory.isProcedure = true;
   program.functions.push_back( std::move( factory ) );
   program.functions.push_back( std::move( build.initializer ) );
   program.classes.push_back( std::move( build.cls ) );

   return true;
}

/* ---------------------------------------------------------------------------------------------
   ENUM and STRUCT
   --------------------------------------------------------------------------------------------- */

/* Integer power by squaring; false on overflow */
static bool s_power( int64_t base, int64_t exponent, int64_t &value )
{
   bool overflow = false;

   value = 1;

   while( exponent != 0 && !overflow )
   {
      if( exponent & 1 )
         overflow = __builtin_mul_overflow( value, base, &value );

      exponent >>= 1;

      if( exponent != 0 && !overflow )
         overflow = __builtin_mul_overflow( base, base, &base );
   }

   return !overflow;
}

/* Field names that the lexer reads as .AND. or .OR. after a value */
static bool s_isDottedWord( const std::string &key )
{
   return key == "AND" || key == "OR";
}

/* STRUCT fields: numbers, LOGIC, PTR and other STRUCTs */
static bool s_structFieldType( X_DECL_TYPE type )
{
   if( type >= X_DECL_TYPE::INT8 && type <= X_DECL_TYPE::FLOAT64 )
      return true;

   return type == X_DECL_TYPE::STRUCT || type == X_DECL_TYPE::PTR || type == X_DECL_TYPE::LOGIC;
}

static bool s_validCount( uint64_t count )
{
   return count != 0 && count <= static_cast<uint64_t>( std::numeric_limits<int64_t>::max() );
}

static bool s_isFunctionWord( const char *text )
{
   const std::string word = s_upper( text );

   return word == "FUNCTION" || word == "PROCEDURE";
}

/* Every name is remembered; a name may not hide an ENUM constant */
bool X_PARSER::bindName( const std::string &name )
{
   const std::string key = s_upper( name.c_str() );

   if( m_enums.count( key ) )
   {
      fail( ( "nazwa koliduje ze stala: " + name ).c_str() );
      return false;
   }

   m_names.insert( key );

   return true;
}

/* + - * / % ^ of ENUM values; overflow, division by zero and a fraction are errors, as / gives a fraction in code */
bool X_PARSER::enumBinary( const std::string &op, int64_t left, int64_t right, int64_t &value )
{
   bool overflow;

   if( op == "+" )
      overflow = __builtin_add_overflow( left, right, &value );
   else if( op == "-" )
      overflow = __builtin_sub_overflow( left, right, &value );
   else if( op == "*" )
      overflow = __builtin_mul_overflow( left, right, &value );
   else if( op == "^" && right >= 0 )
      overflow = !s_power( left, right, value );
   else if( op == "/" || op == "%" )
   {
      if( right == 0 )
      {
         fail( "dzielenie przez zero w wartosci ENUM" );
         return false;
      }

      if( op == "/" && right != -1 && left % right != 0 )
      {
         fail( "wartosc ENUM musi byc liczba calkowita; / daje tu ulamek" );
         return false;
      }

      overflow = left == std::numeric_limits<int64_t>::min() && right == -1;

      if( !overflow )
         value = IIF( op == "/", left / right, left % right );
   }
   else
   {
      fail( "wartosc ENUM wymaga arytmetyki calkowitej" );
      return false;
   }

   if( overflow )
   {
      fail( "przepelnienie wartosci ENUM" );
      return false;
   }

   return true;
}

/* The value of an ENUM constant: integer literals, unary + and -, and + - * / % ^ */
bool X_PARSER::enumValue( const X_EXPR &expr, int64_t &value )
{
   int64_t left;
   int64_t right;

   if( s_isIntegerLiteral( expr ) )
   {
      value = expr.integer;
      return true;
   }

   if( expr.kind == X_EXPR_KIND::UNARY && ( expr.text == "-" || expr.text == "+" ) )
   {
      if( !enumValue( *expr.args[0], left ) )
         return false;

      if( expr.text == "+" )
      {
         value = left;
         return true;
      }

      if( !__builtin_sub_overflow( int64_t( 0 ), left, &value ) )
         return true;

      fail( "ENUM integer overflow" );
      return false;
   }

   if( expr.kind == X_EXPR_KIND::BINARY )
   {
      if( !enumValue( *expr.args[0], left ) || !enumValue( *expr.args[1], right ) )
         return false;

      return enumBinary( expr.text, left, right, value );
   }

   fail( "ENUM value must be an integer constant expression" );

   return false;
}

/* NAME [:= value]; without a value a constant is the previous one + 1 */
bool X_PARSER::parseEnumConstant( int64_t &number, bool first )
{
   std::string name;

   if( !is( TOK_IDENT ) )
   {
      fail( "expected ENUM constant name" );
      return false;
   }

   name = s_upper( value() );
   ++m_pos;

   if( name == "SELF" || name == "SUPER" || s_isInternal( name ) || m_enums.count( name ) || m_names.count( name ) )
   {
      fail( ( "duplicate or conflicting ENUM constant: " + name ).c_str() );
      return false;
   }

   if( match( TOK_ASSIGN ) )
   {
      X_EXPR_PTR expr = parseExpression();

      if( !expr || !enumValue( *expr, number ) )
         return false;
   }
   else if( !first && __builtin_add_overflow( number, int64_t( 1 ), &number ) )
   {
      fail( "ENUM integer overflow" );
      return false;
   }

   m_enums.emplace( name, number );

   return true;
}

/*
 * ENUM [Name] ... ENDENUM: the constants start at 1, like array indexes, so 0 stays free for "no value".
 * The C counterparts of ENUMs from .ch files (version.h, gfxdef.h) start at 1 too.
 */
bool X_PARSER::parseEnum()
{
   int64_t number = 1;
   bool first = true;

   ++m_pos;

   if( is( TOK_IDENT ) )
   {
      const std::string name = s_upper( value() );

      ++m_pos;

      if( !m_tags.insert( name ).second )
      {
         fail( ( "duplicate ENUM name: " + name ).c_str() );
         return false;
      }
   }

   if( !is( TOK_EOL ) )
   {
      fail( "expected end of line after ENUM" );
      return false;
   }

   skipLines();

   while( true )
   {
      bool comma;

      if( !parseEnumConstant( number, first ) )
         return false;

      first = false;
      comma = match( TOK_COMMA );
      skipLines();

      if( at( "ENDENUM" ) )
         break;

      if( !comma )
      {
         fail( "expected comma or ENDENUM" );
         return false;
      }
   }

   ++m_pos;

   return expectLineEnd( "expected end of line after ENDENUM" );
}

/* [ N ]: a fixed array of N elements, N > 0 */
bool X_PARSER::parseFieldCount( X_STRUCT_FIELD &field )
{
   char *end = nullptr;

   if( !is( TOK_NUM ) )
   {
      fail( "rozmiar tablicy STRUCT musi byc stala liczba calkowita" );
      return false;
   }

   errno = 0;
   field.arrayCount = std::strtoull( value(), &end, 10 );

   if( errno != 0 || end == value() || *end != '\0' || !s_validCount( field.arrayCount ) )
   {
      fail( "rozmiar tablicy STRUCT musi byc dodatnia liczba calkowita" );
      return false;
   }

   ++m_pos;

   if( !match( TOK_RBRACKET ) )
   {
      fail( "oczekiwano ]" );
      return false;
   }

   return true;
}

/* name[ N ] AS type */
bool X_PARSER::parseStructField( const X_STRUCT_DECL &structure, X_STRUCT_FIELD &field )
{
   std::string key;

   if( !is( TOK_IDENT ) )
   {
      fail( "oczekiwano pola STRUCT albo ENDSTRUCT" );
      return false;
   }

   field.line = m_tokens.items[m_pos].line;
   field.name = value();
   key = s_upper( field.name.c_str() );
   ++m_pos;

   if( s_isDottedWord( key ) )
   {
      fail( "nazwa pola STRUCT koliduje z operatorem .AND. lub .OR." );
      return false;
   }

   for( const X_STRUCT_FIELD &previous : structure.fields )
   {
      if( s_upper( previous.name.c_str() ) == key )
      {
         fail( "powtorzone pole STRUCT" );
         return false;
      }
   }

   if( match( TOK_LBRACKET ) && !parseFieldCount( field ) )
      return false;

   if( !is( TOK_AS ) || !parseType( field.type, &field.typeName ) )
   {
      fail( "pole STRUCT wymaga typu po AS" );
      return false;
   }

   if( !s_structFieldType( field.type ) )
   {
      fail( "nieobslugiwany typ pola STRUCT" );
      return false;
   }

   return expectLineEnd( "oczekiwano konca linii po polu STRUCT" );
}

/* STRUCT Name ... ENDSTRUCT: fields with C types, laid out as in C */
bool X_PARSER::parseStruct( X_PROGRAM &program )
{
   X_STRUCT_DECL structure;

   structure.line = m_tokens.items[m_pos].line;
   ++m_pos;

   if( !is( TOK_IDENT ) )
   {
      fail( "STRUCT wymaga nazwy" );
      return false;
   }

   structure.name = s_upper( value() );
   ++m_pos;

   if( m_structTypes.count( structure.name ) || m_enums.count( structure.name ) )
   {
      fail( "powtorzona nazwa STRUCT" );
      return false;
   }

   if( !expectLineEnd( "oczekiwano konca linii po STRUCT" ) )
      return false;

   skipLines();

   while( !is( TOK_EOF ) && !at( "ENDSTRUCT" ) )
   {
      X_STRUCT_FIELD field;

      if( match( TOK_EOL ) )
         continue;

      if( !parseStructField( structure, field ) )
         return false;

      structure.fields.push_back( std::move( field ) );
      skipLines();
   }

   if( !expect( "ENDSTRUCT" ) )
      return false;

   if( structure.fields.empty() )
   {
      fail( "STRUCT musi zawierac co najmniej jedno pole" );
      return false;
   }

   if( !expectLineEnd( "oczekiwano konca linii po ENDSTRUCT" ) )
      return false;

   m_structTypes.insert( structure.name );
   program.structs.push_back( std::move( structure ) );

   return true;
}

/* ---------------------------------------------------------------------------------------------
   Program
   --------------------------------------------------------------------------------------------- */

bool X_PARSER::parseDll( X_PROGRAM &program )
{
   X_DLL_FUNCTION function;

   if( !parseDllFunction( function ) )
      return false;

   program.dllFunctions.push_back( std::move( function ) );

   return true;
}

/* REQUEST, FIELD and STATIC variables at module level */
bool X_PARSER::parseModuleDeclaration( X_PROGRAM &program )
{
   X_STMT statement;
   bool ok;

   statement.line = m_tokens.items[m_pos].line;

   if( is( TOK_REQUEST ) )
      ok = parseRequest( statement );
   else if( is( TOK_FIELD ) )
      ok = parseFieldDeclaration( statement );
   else
      ok = parseStatic( statement );

   if( !ok )
      return false;

   program.statements.push_back( std::move( statement ) );
   skipLines();

   return true;
}

bool X_PARSER::parseModuleFunction( X_PROGRAM &program, bool isStatic, X_PROC_ROLE role )
{
   X_FUNCTION function;

   if( !parseFunction( function, isStatic ) )
      return false;

   function.role = role;

   if( role == X_PROC_ROLE::EXIT && !function.params.empty() )
   {
      fail( "EXIT PROCEDURE nie przyjmuje parametrow" );
      return false;
   }

   program.functions.push_back( std::move( function ) );

   return true;
}

/* Code outside functions is kept here and rejected by the semantic pass */
bool X_PARSER::parseModuleStatement( X_PROGRAM &program )
{
   X_STMT statement;

   if( !parseStatement( statement ) )
      return false;

   program.statements.push_back( std::move( statement ) );

   if( !expectLineEnd( "oczekiwano konca linii" ) )
      return false;

   skipLines();

   return true;
}

/* STATIC FUNCTION and STATIC PROCEDURE are file-local functions; STATIC name is a file-scope variable */
bool X_PARSER::parseModuleItem( X_PROGRAM &program )
{
   const bool isStatic = at( "STATIC" ) && peek( 1 ) != TOK_EOF && s_isFunctionWord( value( 1 ) );
   X_PROC_ROLE role = X_PROC_ROLE::NORMAL;

   if( !isStatic && lateDeclaration() )
      return parseModuleDeclaration( program );

   if( isStatic )
      ++m_pos;

   if( at( "INIT" ) || at( "EXIT" ) )
   {
      role = IIF( at( "INIT" ), X_PROC_ROLE::INIT, X_PROC_ROLE::EXIT );
      ++m_pos;

      if( !at( "PROCEDURE" ) )
      {
         fail( "INIT/EXIT wymaga PROCEDURE" );
         return false;
      }
   }

   if( at( "FUNCTION" ) || at( "PROCEDURE" ) || atMethodWord() )
      return parseModuleFunction( program, isStatic, role );

   return parseModuleStatement( program );
}

/* One declaration at module level */
bool X_PARSER::parseTopLevel( X_PROGRAM &program )
{
   bool ok;

   if( atCallback() )
      ok = parseCallback( program );
   else if( atDllFunction() )
      ok = parseDll( program );
   else if( at( "ENUM" ) )
      ok = parseEnum();
   else if( at( "STRUCT" ) )
      ok = parseStruct( program );
   else if( at( "CREATE" ) && peek( 1 ) != TOK_EOF && s_upper( value( 1 ) ) == "CLASS" )
      ok = parseClass( program );
   else if( is( TOK_ANNOUNCE ) )
      ok = parseAnnounce( program );
   else
      return parseModuleItem( program );

   if( ok )
      skipLines();

   return ok;
}

/* The handler may not share its name with another member; its METHOD body becomes the ONERROR function */
void X_PARSER::onErrorHandler( X_PROGRAM &program, const X_CLASS_DECL &cls, const X_CLASS_MEMBER &member )
{
   for( const X_CLASS_MEMBER &other : cls.members )
   {
      if( other.kind != "ONERROR" && other.name == member.name )
      {
         m_error = "nazwa handlera ON ERROR koliduje ze skladowa klasy: " + cls.name + ":" + member.name;
         m_diagnostics.push_back( { cls.line, 0, m_error } );
         break;
      }
   }

   for( X_FUNCTION &function : program.functions )
   {
      if( function.className != cls.name || function.methodName != member.name || function.methodKind != "METHOD" )
         continue;

      function.methodKind = "ONERROR";
      function.name = "__XDEV_" + cls.name + "_ONERROR_" + member.name;
   }
}

void X_PARSER::onErrorHandlers( X_PROGRAM &program )
{
   for( const X_CLASS_DECL &cls : program.classes )
   {
      for( const X_CLASS_MEMBER &member : cls.members )
      {
         if( member.kind == "ONERROR" )
            onErrorHandler( program, cls, member );
      }
   }
}

/* The program: declarations, functions, module STATIC, FIELD and REQUEST */
bool X_PARSER::parse( X_PROGRAM &program, std::string &error )
{
   skipLines();

   while( !is( TOK_EOF ) )
   {
      const int start = m_pos;

      if( parseTopLevel( program ) )
         continue;

      if( !recover( start ) )
         break;

      /* The rest of a broken declaration is skipped up to the next line that starts a routine */
      while( !is( TOK_EOF ) && !atRoutineStart() )
      {
         while( !is( TOK_EOF ) && !is( TOK_EOL ) )
            ++m_pos;

         skipLines();
      }
   }

   if( m_diagnostics.empty() )
      onErrorHandlers( program );

   if( m_diagnostics.empty() )
      bindFields( program );

   program.hasTypeAnnotations = m_hasTypeAnnotations;
   error = IIF( m_diagnostics.empty(), std::string(), m_diagnostics[0].message );

   return m_diagnostics.empty();
}
