/*
 * Copyright (c) 2024-2026 xDev-Language
 *
 * Library...: xDev Frontend
 * File......: ast.cpp
 *
 */

#include "internal.h"

#include "ast.hpp"

#include <cstdio>
#include <cctype>

const char *x_declTypeName( X_DECL_TYPE type )
{
   switch( type )
   {
   case X_DECL_TYPE::ANY:       return "ANY";
   case X_DECL_TYPE::INT8:      return "INT8";
   case X_DECL_TYPE::INT16:     return "INT16";
   case X_DECL_TYPE::INT32:     return "INT32";
   case X_DECL_TYPE::INT64:     return "INT64";
   case X_DECL_TYPE::UINT8:     return "UINT8";
   case X_DECL_TYPE::UINT16:    return "UINT16";
   case X_DECL_TYPE::UINT32:    return "UINT32";
   case X_DECL_TYPE::UINT64:    return "UINT64";
   case X_DECL_TYPE::FLOAT32:   return "FLOAT32";
   case X_DECL_TYPE::FLOAT64:   return "FLOAT64";
   case X_DECL_TYPE::LOGIC:     return "LOGIC";
   case X_DECL_TYPE::STRING:    return "STRING";
   case X_DECL_TYPE::PSZ:       return "PSZ";
   case X_DECL_TYPE::PTR:       return "PTR";
   case X_DECL_TYPE::CALLBACK:  return "CALLBACK";
   case X_DECL_TYPE::STRUCT:    return "STRUCT";
   case X_DECL_TYPE::OBJECT:    return "OBJECT";
   case X_DECL_TYPE::NIL:       return "NIL";
   case X_DECL_TYPE::ARRAY:     return "ARRAY";
   case X_DECL_TYPE::CODEBLOCK: return "CODEBLOCK";
   case X_DECL_TYPE::DATE:      return "DATE";
   case X_DECL_TYPE::HASH:      return "HASH";
   }
   return "ANY";
}

/* ---------------------------------------------------------------------------------------------
   Expressions
   --------------------------------------------------------------------------------------------- */

static void s_dumpExpr( const X_EXPR &expr )
{
   switch( expr.kind )
   {
   case X_EXPR_KIND::NIL:
      std::printf( "NIL" );
      break;

   case X_EXPR_KIND::NUMBER:
      if( expr.numberIsInteger )
         std::printf( "%lld", ( long long )expr.integer );
      else
         std::printf( "%g", expr.number );
      break;

   case X_EXPR_KIND::STRING:
      std::printf( "\"%s\"", expr.text.c_str() );
      break;

   case X_EXPR_KIND::LOGICAL:
      std::printf( "%s", IIF( expr.logical, ".T.", ".F." ) );
      break;

   case X_EXPR_KIND::NAME:
      std::printf( "%s", expr.text.c_str() );
      break;

   case X_EXPR_KIND::FUNCTION_ADDRESS:
      std::printf( "@%s()", expr.text.c_str() );
      break;

   case X_EXPR_KIND::CAST:
      std::printf( "CAST( " );
      s_dumpExpr( *expr.args[0] );
      std::printf( " AS %s )", x_declTypeName( expr.type ) );
      break;

   case X_EXPR_KIND::FIELD:
      if( expr.args[0]->kind == X_EXPR_KIND::NIL )
         std::printf( "FIELD" );
      else
         s_dumpExpr( *expr.args[0] );
      std::printf( "->%s", expr.text.c_str() );
      break;

   case X_EXPR_KIND::ALIAS:
   case X_EXPR_KIND::GROUP:
   {
      const size_t begin = expr.kind == X_EXPR_KIND::ALIAS ? 1 : 0;
      if( begin )
      {
         s_dumpExpr( *expr.args[0] );
         std::printf( "->" );
      }
      std::printf( "(" );
      for( size_t i = begin; i < expr.args.size(); ++i )
      {
         if( i != begin )
            std::printf( ", " );
         s_dumpExpr( *expr.args[i] );
      }
      std::printf( ")" );
      break;
   }

   case X_EXPR_KIND::CALL:
      std::printf( "%s(", expr.text.c_str() );

      for( size_t i = 0; i < expr.args.size(); ++i )
      {
         if( i )
            std::printf( ", " );

         s_dumpExpr( *expr.args[i] );
      }

      std::printf( ")" );
      break;

   case X_EXPR_KIND::SEND:
      s_dumpExpr( *expr.args[0] );
      std::printf( ":%s", expr.text.c_str() );

      if( expr.args.size() > 1 )
      {
         std::printf( "(" );

         for( size_t i = 1; i < expr.args.size(); ++i )
         {
            if( i > 1 )
               std::printf( ", " );

            s_dumpExpr( *expr.args[i] );
         }

         std::printf( ")" );
      }
      break;

   case X_EXPR_KIND::ARRAY:
      std::printf( "{ " );

      for( size_t i = 0; i < expr.args.size(); ++i )
      {
         if( i )
            std::printf( ", " );

         s_dumpExpr( *expr.args[i] );
      }

      std::printf( " }" );
      break;

   case X_EXPR_KIND::INDEX:
      s_dumpExpr( *expr.args[0] );
      std::printf( "[" );
      s_dumpExpr( *expr.args[1] );
      std::printf( "]" );
      break;

   case X_EXPR_KIND::UNARY:
      std::printf( "%s", expr.text.c_str() );
      s_dumpExpr( *expr.args[0] );
      break;

   case X_EXPR_KIND::BINARY:
      std::printf( "(" );
      s_dumpExpr( *expr.args[0] );
      std::printf( " %s ", expr.text.c_str() );
      s_dumpExpr( *expr.args[1] );
      std::printf( ")" );
      break;

   case X_EXPR_KIND::UPDATE:
      if( expr.updateMode == X_UPDATE_MODE::PREFIX )
         std::printf( "%s", IIF( expr.updateOp == X_UPDATE_OP::INC, "++", "--" ) );

      s_dumpExpr( *expr.args[0] );

      if( expr.updateMode == X_UPDATE_MODE::POSTFIX )
         std::printf( "%s", IIF( expr.updateOp == X_UPDATE_OP::INC, "++", "--" ) );
      break;

   case X_EXPR_KIND::BLOCK:
      std::printf( "{ |" );

      for( size_t i = 0; i < expr.blockParams.size(); ++i )
      {
         if( i )
            std::printf( ", " );

         std::printf( "%s", expr.blockParams[i].c_str() );
      }

      std::printf( "| " );

      for( size_t i = 0; i < expr.args.size(); ++i )
      {
         if( i )
            std::printf( ", " );

         s_dumpExpr( *expr.args[i] );
      }

      std::printf( " }" );
      break;

   case X_EXPR_KIND::IIF:
      std::printf( "IIF(" );

      for( size_t i = 0; i < expr.args.size(); ++i )
      {
         if( i )
            std::printf( ", " );

         s_dumpExpr( *expr.args[i] );
      }

      std::printf( ")" );
      break;

   case X_EXPR_KIND::ASSIGN:
      s_dumpExpr( *expr.args[0] );

      switch( expr.assignOp )
      {
      case X_ASSIGN_OP::SET:
         std::printf( " := " );
         break;

      case X_ASSIGN_OP::ADD:
         std::printf( " += " );
         break;

      case X_ASSIGN_OP::SUB:
         std::printf( " -= " );
         break;

      case X_ASSIGN_OP::MUL:
         std::printf( " *= " );
         break;

      case X_ASSIGN_OP::DIV:
         std::printf( " /= " );
         break;

      case X_ASSIGN_OP::MOD:
         std::printf( " %%= " );
         break;

      case X_ASSIGN_OP::POW:
         std::printf( " ^= " );
         break;
      }

      s_dumpExpr( *expr.args[1] );
      break;
   }
}

/* ---------------------------------------------------------------------------------------------
   Statements
   --------------------------------------------------------------------------------------------- */

static void s_dumpStatements( const std::vector<X_STMT> &statements, int indent )
{
   for( const X_STMT &stmt : statements )
   {
      std::printf( "%*s", indent, "" );

      if( stmt.kind == X_STMT_KIND::ASSIGN )
      {
         std::printf( "%s := ", stmt.name.c_str() );
         s_dumpExpr( *stmt.expr );
         std::printf( "\n" );
      }
      else if( stmt.kind == X_STMT_KIND::INDEX_ASSIGN )
      {
         s_dumpExpr( *stmt.target );
         std::printf( " := " );
         s_dumpExpr( *stmt.expr );
         std::printf( "\n" );
      }
      else if( stmt.kind == X_STMT_KIND::LOCAL || stmt.kind == X_STMT_KIND::STATIC )
      {
         if( stmt.kind == X_STMT_KIND::LOCAL )
            std::printf( "LOCAL " );
         else
            std::printf( "STATIC " );

         for( size_t i = 0; i < stmt.locals.size(); ++i )
         {
            if( i )
               std::printf( ", " );

            std::printf( "%s", stmt.locals[i].name.c_str() );
            if( stmt.locals[i].declaredType )
               std::printf( " AS %s", x_declTypeName( *stmt.locals[i].declaredType ) );

            if( stmt.locals[i].init )
            {
               std::printf( " := " );
               s_dumpExpr( *stmt.locals[i].init );
            }
         }

         std::printf( "\n" );
      }
      else if( stmt.kind == X_STMT_KIND::FIELD )
      {
         std::printf( "FIELD " );
         for( size_t i = 0; i < stmt.locals.size(); ++i )
            std::printf( "%s%s", i ? ", " : "", stmt.locals[i].name.c_str() );
         if( !stmt.name.empty() )
            std::printf( " IN %s", stmt.name.c_str() );
         std::printf( "\n" );
      }
      else if( stmt.kind == X_STMT_KIND::RETURN )
      {
         std::printf( "RETURN" );
         if( stmt.expr )
         {
            std::printf( " " );
            s_dumpExpr( *stmt.expr );
         }
         std::printf( "\n" );
      }
      else if( stmt.kind == X_STMT_KIND::CALL )
      {
         s_dumpExpr( *stmt.expr );
         std::printf( "\n" );
      }
      else if( stmt.kind == X_STMT_KIND::BREAK )
      {
         std::printf( "BREAK" );
         if( stmt.expr )
         {
            std::printf( " " );
            s_dumpExpr( *stmt.expr );
         }
         std::printf( "\n" );
      }
      else if( stmt.kind == X_STMT_KIND::SEQUENCE )
      {
         std::printf( "BEGIN SEQUENCE\n" );
         s_dumpStatements( stmt.body, indent + 2 );
         if( stmt.hasOtherwise )
         {
            std::printf( "%*sRECOVER", indent, "" );
            if( !stmt.name.empty() )
               std::printf( " USING %s", stmt.name.c_str() );
            std::printf( "\n" );
            s_dumpStatements( stmt.elseBody, indent + 2 );
         }
         std::printf( "%*sEND SEQUENCE\n", indent, "" );
      }
      else if( stmt.kind == X_STMT_KIND::IF )
      {
         std::printf( "IF " );
         s_dumpExpr( *stmt.expr );
         std::printf( "\n" );
         s_dumpStatements( stmt.body, indent + 2 );

         if( !stmt.elseBody.empty() )
         {
            std::printf( "%*sELSE\n", indent, "" );
            s_dumpStatements( stmt.elseBody, indent + 2 );
         }

         std::printf( "%*sENDIF\n", indent, "" );
      }
      else if( stmt.kind == X_STMT_KIND::DO_CASE || stmt.kind == X_STMT_KIND::SWITCH )
      {
         if( stmt.kind == X_STMT_KIND::SWITCH )
         {
            std::printf( "SWITCH " );
            s_dumpExpr( *stmt.expr );
            std::printf( "\n" );
         }
         else
            std::printf( "DO CASE\n" );

         for( const X_CASE_BRANCH &branch : stmt.cases )
         {
            std::printf( "%*sCASE ", indent + 2, "" );
            s_dumpExpr( *branch.condition );
            std::printf( "\n" );
            s_dumpStatements( branch.body, indent + 4 );
         }

         if( stmt.hasOtherwise )
         {
            std::printf( "%*sOTHERWISE\n", indent + 2, "" );
            s_dumpStatements( stmt.otherwiseBody, indent + 4 );
         }

         std::printf( "%*s", indent, "" );
         std::printf( "%s\n", IIF( stmt.kind == X_STMT_KIND::SWITCH, "ENDSWITCH", "ENDCASE" ) );
      }
      else if( stmt.kind == X_STMT_KIND::WHILE )
      {
         std::printf( "DO WHILE " );
         s_dumpExpr( *stmt.expr );
         std::printf( "\n" );
         s_dumpStatements( stmt.body, indent + 2 );
         std::printf( "%*sENDDO\n", indent, "" );
      }
      else if( stmt.kind == X_STMT_KIND::FOR )
      {
         std::printf( "FOR %s := ", stmt.name.c_str() );
         s_dumpExpr( *stmt.expr );
         std::printf( " TO " );
         s_dumpExpr( *stmt.forLimit );

         if( stmt.forStep )
         {
            std::printf( " STEP " );
            s_dumpExpr( *stmt.forStep );
         }

         std::printf( "\n" );
         s_dumpStatements( stmt.body, indent + 2 );
         std::printf( "%*sNEXT\n", indent, "" );
      }
      else if( stmt.kind == X_STMT_KIND::REQUEST )
      {
         std::printf( "REQUEST " );

         for( size_t i = 0; i < stmt.requests.size(); ++i )
         {
            if( i )
               std::printf( ", " );

            std::printf( "%s", stmt.requests[i].c_str() );
         }

         std::printf( "\n" );
      }
      else if( stmt.kind == X_STMT_KIND::EXIT )
         std::printf( "EXIT\n" );
      else if( stmt.kind == X_STMT_KIND::LOOP )
         std::printf( "LOOP\n" );
   }
}

/* ---------------------------------------------------------------------------------------------
   Parameters
   --------------------------------------------------------------------------------------------- */

// Parameter types are stored under the upper-case parameter name
static std::string s_paramKey( const std::string &name )
{
   std::string key = name;

   for( char &ch : key )
      ch = ( char )std::toupper( ( unsigned char )ch );

   return key;
}

/* ---------------------------------------------------------------------------------------------
   DLL functions
   --------------------------------------------------------------------------------------------- */

static void s_dumpDllFunction( const X_DLL_FUNCTION &function )
{
   const char *prefix = IIF( function.isStatic, "STATIC ", "" );
   const char *kind = IIF( function.isProcedure, "PROCEDURE", "FUNCTION" );

   std::printf( "  DLL %s%s %s( ", prefix, kind, function.name.c_str() );

   for( size_t i = 0; i < function.params.size(); ++i )
   {
      const X_DECL_TYPE type = function.paramTypes.at( s_paramKey( function.params[i] ) );

      if( i )
         std::printf( ", " );

      std::printf( "%s AS %s", function.params[i].c_str(), x_declTypeName( type ) );
   }

   std::printf( " )" );

   /* DLL PROCEDURE has no result type */
   if( function.resultType )
      std::printf( " AS %s", x_declTypeName( *function.resultType ) );

   std::printf( " FROM %s LIB \"%s\"\n", function.symbol.c_str(), function.library.c_str() );
}

/* ---------------------------------------------------------------------------------------------
   Classes
   --------------------------------------------------------------------------------------------- */

// Keyword shown for a member: ON ERROR and DELEGATE are stored under internal names
static const char *s_memberKeyword( const X_CLASS_MEMBER &member )
{
   if( member.kind == "ONERROR" )
      return "ON ERROR";

   if( member.kind == "DELEGATE" )
      return "MESSAGE";

   return member.kind.c_str();
}

// INIT value of DATA: kept in the data initializer function of the class
static void s_dumpDataInit( const X_PROGRAM &program, const X_CLASS_DECL &cls, const X_CLASS_MEMBER &member )
{
   for( const X_FUNCTION &function : program.functions )
   {
      if( !function.isDataInitializer || function.className != cls.name )
         continue;

      for( const X_LOCAL_DECL &data : function.dataInitializers )
      {
         if( data.name == member.name && data.init )
         {
            std::printf( " INIT " );
            s_dumpExpr( *data.init );
         }
      }
   }
}

// INIT value of CLASSDATA: the first statement of its own initializer function
static void s_dumpClassDataInit( const X_PROGRAM &program, const X_CLASS_DECL &cls, const X_CLASS_MEMBER &member )
{
   const std::string name = "__XDEV_" + cls.name + "_CLASSDATA_" + member.name;

   for( const X_FUNCTION &function : program.functions )
   {
      if( !function.isClassDataInitializer || function.name != name )
         continue;

      if( !function.body.empty() && function.body[0].expr )
      {
         std::printf( " INIT " );
         s_dumpExpr( *function.body[0].expr );
      }
   }
}

static void s_dumpClass( const X_PROGRAM &program, const X_CLASS_DECL &cls )
{
   std::printf( "  CREATE CLASS %s", cls.name.c_str() );

   for( size_t i = 0; i < cls.parents.size(); ++i )
      std::printf( "%s%s", IIF( i == 0, " FROM ", ", " ), cls.parents[i].c_str() );

   std::puts( "" );

   for( const X_CLASS_MEMBER &member : cls.members )
   {
      std::printf( "    %s %s scope=%u", s_memberKeyword( member ), member.name.c_str(), ( unsigned )member.scope );

      if( member.kind == "DELEGATE" )
         std::printf( " IS %s TO %s", member.delegateMessage.c_str(), member.delegateObject.c_str() );

      if( member.operatorId >= 0 )
         std::printf( " OPERATOR %s", member.operatorSymbol.c_str() );

      if( member.kind == "DATA" )
         s_dumpDataInit( program, cls, member );

      if( member.kind == "CLASSDATA" )
         s_dumpClassDataInit( program, cls, member );

      std::puts( "" );
   }

   std::puts( "  ENDCLASS" );
}

/* ---------------------------------------------------------------------------------------------
   Functions and methods
   --------------------------------------------------------------------------------------------- */

// Word before FUNCTION / PROCEDURE: INIT, EXIT or STATIC
static const char *s_functionPrefix( const X_FUNCTION &function )
{
   if( function.role == X_PROC_ROLE::INIT )
      return "INIT ";

   if( function.role == X_PROC_ROLE::EXIT )
      return "EXIT ";

   return IIF( function.isStatic, "STATIC ", "" );
}

static void s_dumpFunction( const X_FUNCTION &function )
{
   if( !function.className.empty() )
   {
      const char *keyword = IIF( function.methodKind == "ONERROR", "METHOD", function.methodKind.c_str() );

      std::printf( "  %s %s( ", keyword, function.methodName.c_str() );
   }
   else
   {
      const char *kind = IIF( function.isProcedure, "PROCEDURE", "FUNCTION" );

      std::printf( "  %s%s %s( ", s_functionPrefix( function ), kind, function.name.c_str() );
   }

   for( size_t i = 0; i < function.params.size(); ++i )
   {
      const auto type = function.paramTypes.find( s_paramKey( function.params[i] ) );

      if( i )
         std::printf( ", " );

      std::printf( "%s", function.params[i].c_str() );

      if( type != function.paramTypes.end() )
         std::printf( " AS %s", x_declTypeName( type->second ) );
   }

   std::printf( " )" );

   if( function.resultType )
      std::printf( " AS %s", x_declTypeName( *function.resultType ) );

   if( !function.className.empty() )
      std::printf( " CLASS %s", function.className.c_str() );

   std::puts( "" );
   s_dumpStatements( function.body, 4 );
}

/* ---------------------------------------------------------------------------------------------
   AST dump
   --------------------------------------------------------------------------------------------- */

void x_astDump( const X_PROGRAM &program )
{
   std::puts( "Program" );
   s_dumpStatements( program.statements, 2 );

   for( const X_DLL_FUNCTION &function : program.dllFunctions )
      s_dumpDllFunction( function );

   for( const X_CLASS_DECL &cls : program.classes )
      s_dumpClass( program, cls );

   // Factories and DATA / CLASSDATA initializers are generated code: not shown
   for( const X_FUNCTION &function : program.functions )
   {
      if( function.factoryClass.empty() && !function.isDataInitializer && !function.isClassDataInitializer )
         s_dumpFunction( function );
   }
}
