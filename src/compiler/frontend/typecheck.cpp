/*
 * Copyright (c) 2024-2026 xDev-Language
 *
 * Library: xDev Frontend
 * File: typecheck.cpp
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "internal.h"
#include "typecheck.hpp"

#include <cctype>
#include <unordered_map>
#include <unordered_set>

/* Error messages too long for one line of code */
static const char *s_mixedSigns = " wymaga jawnego CAST; brak typu ze znakiem mieszczacego oba zakresy";
static const char *s_noStruct = ": STRUCT wymaga zgodnego typu struktury; nie mozna przekazac przez ANY ani wartosc";
static const char *s_callbackKind = ": CALLBACK PROCEDURE wymaga PROCEDURE, a CALLBACK FUNCTION wymaga FUNCTION";
static const char *s_callbackParameter = "CALLBACK: parametr wymaga liczby, LOGIC, PTR albo PSZ: ";
static const char *s_callbackResult = "CALLBACK FUNCTION: wynik wymaga liczby, LOGIC albo PTR; PSZ jest niedozwolony";

/* ---------------------------------------------------------------------------------------------
   Types
   --------------------------------------------------------------------------------------------- */

static std::string s_key( const std::string &name )
{
   std::string key = name;

   for( char &ch : key )
      ch = ( char )std::toupper( ( unsigned char )ch );

   return key;
}

static bool s_integer( X_DECL_TYPE type )
{
   return type >= X_DECL_TYPE::INT8 && type <= X_DECL_TYPE::UINT64;
}

static bool s_numeric( X_DECL_TYPE type )
{
   return s_integer( type ) || type == X_DECL_TYPE::FLOAT32 || type == X_DECL_TYPE::FLOAT64;
}

static bool s_unsigned( X_DECL_TYPE type )
{
   return type >= X_DECL_TYPE::UINT8 && type <= X_DECL_TYPE::UINT64;
}

static int s_width( X_DECL_TYPE type )
{
   switch( type )
   {
   case X_DECL_TYPE::INT8:
   case X_DECL_TYPE::UINT8:  return 8;
   case X_DECL_TYPE::INT16:
   case X_DECL_TYPE::UINT16: return 16;
   case X_DECL_TYPE::INT32:
   case X_DECL_TYPE::UINT32: return 32;
   case X_DECL_TYPE::INT64:
   case X_DECL_TYPE::UINT64: return 64;
   default:                  return 0;
   }
}

/* Integers narrower than 32 bits are computed as INT32 */
static X_DECL_TYPE s_promote( X_DECL_TYPE type )
{
   if( s_integer( type ) && s_width( type ) < 32 )
      return X_DECL_TYPE::INT32;

   return type;
}

/* PTR and CALLBACK allow only == and != */
static bool s_isAddress( X_DECL_TYPE type )
{
   return type == X_DECL_TYPE::PTR || type == X_DECL_TYPE::CALLBACK;
}

/* STRUCT, OBJECT and CALLBACK carry the name of their type */
static bool s_namedType( X_DECL_TYPE type )
{
   return type == X_DECL_TYPE::STRUCT || type == X_DECL_TYPE::OBJECT || type == X_DECL_TYPE::CALLBACK;
}

/* Types that can cross the C boundary of a DLL */
static bool s_abiType( X_DECL_TYPE type )
{
   if( s_numeric( type ) || type == X_DECL_TYPE::PTR || type == X_DECL_TYPE::PSZ || type == X_DECL_TYPE::LOGIC )
      return true;

   return type == X_DECL_TYPE::STRUCT || type == X_DECL_TYPE::CALLBACK;
}

/* A parameter of a CALLBACK: a number, LOGIC, PTR or PSZ */
static bool s_callbackValue( X_DECL_TYPE type )
{
   return s_abiType( type ) && type != X_DECL_TYPE::STRUCT && type != X_DECL_TYPE::CALLBACK;
}

static bool s_callbackResultType( const std::optional<X_DECL_TYPE> &type )
{
   return type && s_callbackValue( *type ) && *type != X_DECL_TYPE::PSZ;
}

static bool s_dllResultType( const std::optional<X_DECL_TYPE> &type )
{
   return type && s_abiType( *type ) && *type != X_DECL_TYPE::STRUCT;
}

/* "INT32 i UINT64" for error messages */
static std::string s_typePair( X_DECL_TYPE left, X_DECL_TYPE right )
{
   return std::string( x_declTypeName( left ) ) + " i " + x_declTypeName( right );
}

/* The required type in OBJECT errors */
static std::string s_objectTarget( X_DECL_TYPE target, const std::string &targetName )
{
   if( target != X_DECL_TYPE::OBJECT )
      return x_declTypeName( target );

   if( targetName.empty() )
      return "wymagany typ OBJECT";

   return targetName;
}

/* ---------------------------------------------------------------------------------------------
   Conversions
   --------------------------------------------------------------------------------------------- */

/* NIL is the empty value of these types */
static bool s_nilTarget( X_DECL_TYPE type )
{
   switch( type )
   {
   case X_DECL_TYPE::PTR:
   case X_DECL_TYPE::CALLBACK:
   case X_DECL_TYPE::ARRAY:
   case X_DECL_TYPE::CODEBLOCK:
   case X_DECL_TYPE::HASH: return true;
   default:                return false;
   }
}

static bool s_logicOrNumber( X_DECL_TYPE type )
{
   return type == X_DECL_TYPE::LOGIC || s_numeric( type );
}

/* Implicit conversions; CAST also allows LOGIC <-> number and PTR <-> integer */
static bool s_convertible( X_DECL_TYPE source, X_DECL_TYPE target, bool cast )
{
   if( source == X_DECL_TYPE::ANY || target == X_DECL_TYPE::ANY )
      return true;

   if( s_numeric( source ) && s_numeric( target ) )
      return true;

   if( source == X_DECL_TYPE::NIL && s_nilTarget( target ) )
      return true;

   if( source == X_DECL_TYPE::STRING && target == X_DECL_TYPE::PSZ )
      return true;

   if( source == X_DECL_TYPE::PSZ && target == X_DECL_TYPE::STRING )
      return true;

   if( !cast )
      return false;

   if( s_logicOrNumber( source ) && s_logicOrNumber( target ) )
      return true;

   if( source == X_DECL_TYPE::PTR && s_integer( target ) )
      return true;

   return s_integer( source ) && target == X_DECL_TYPE::PTR;
}

/* An integer target needs a range check unless every source value fits */
static bool s_needsRangeCheck( X_DECL_TYPE source, X_DECL_TYPE target )
{
   if( source == X_DECL_TYPE::ANY || !s_integer( source ) )
      return true;

   if( s_width( source ) > s_width( target ) )
      return true;

   if( s_unsigned( source ) == s_unsigned( target ) )
      return false;

   /* Signed to unsigned, or unsigned to a signed type of the same width */
   return s_unsigned( target ) || s_width( source ) == s_width( target );
}

/* ---------------------------------------------------------------------------------------------
   Operators
   --------------------------------------------------------------------------------------------- */

static bool s_callbackSide( X_DECL_TYPE type )
{
   return type == X_DECL_TYPE::CALLBACK || type == X_DECL_TYPE::NIL;
}

/* == of CALLBACK values: CALLBACK or NIL on both sides, at least one CALLBACK */
static bool s_callbackPair( X_DECL_TYPE a, X_DECL_TYPE b )
{
   if( a != X_DECL_TYPE::CALLBACK && b != X_DECL_TYPE::CALLBACK )
      return false;

   return s_callbackSide( a ) && s_callbackSide( b );
}

static bool s_pointerSide( X_DECL_TYPE type )
{
   return type == X_DECL_TYPE::PTR || type == X_DECL_TYPE::ANY || type == X_DECL_TYPE::NIL;
}

/* == of PTR values: PTR, ANY or NIL on both sides, at least one PTR */
static bool s_pointerPair( X_DECL_TYPE a, X_DECL_TYPE b )
{
   if( a != X_DECL_TYPE::PTR && b != X_DECL_TYPE::PTR )
      return false;

   return s_pointerSide( a ) && s_pointerSide( b );
}

/* Two values of the same type: STRING compares and joins, LOGIC compares, == on ARRAY and HASH */
static bool s_sameTypeOperator( X_DECL_TYPE type, const std::string &op, bool comparison )
{
   if( type == X_DECL_TYPE::STRING )
      return comparison || op == "+" || op == "-";

   if( type == X_DECL_TYPE::LOGIC )
      return comparison;

   return op == "==" && ( type == X_DECL_TYPE::ARRAY || type == X_DECL_TYPE::HASH );
}

/* x += y is checked as x + y */
static const char *s_assignOperator( X_ASSIGN_OP op )
{
   switch( op )
   {
   case X_ASSIGN_OP::SUB: return "-";
   case X_ASSIGN_OP::MUL: return "*";
   case X_ASSIGN_OP::DIV: return "/";
   case X_ASSIGN_OP::MOD: return "%";
   case X_ASSIGN_OP::POW: return "^";
   default:               return "+";
   }
}

/* ---------------------------------------------------------------------------------------------
   Expressions
   --------------------------------------------------------------------------------------------- */

static X_DECL_TYPE s_numberType( const X_EXPR &expr )
{
   if( expr.numberIsInteger )
      return X_DECL_TYPE::INT64;

   return X_DECL_TYPE::FLOAT64;
}

static bool s_indexable( X_DECL_TYPE type )
{
   return type == X_DECL_TYPE::ANY || type == X_DECL_TYPE::ARRAY || type == X_DECL_TYPE::HASH;
}

/* type and NIL in any order */
static bool s_withNil( X_DECL_TYPE left, X_DECL_TYPE right, X_DECL_TYPE type )
{
   if( left == type && right == X_DECL_TYPE::NIL )
      return true;

   return right == type && left == X_DECL_TYPE::NIL;
}

/* obj:_name( value ) assigns name */
static bool s_isSetter( const X_EXPR &expr )
{
   return expr.args.size() == 2 && !expr.text.empty() && expr.text[0] == '_';
}

/* st.field and st:_field( value ) */
static bool s_structField( const X_EXPR &expr )
{
   if( !expr.dotMemberAccess || expr.sendHasParentheses || expr.args.empty() )
      return false;

   if( expr.args[0]->type != X_DECL_TYPE::STRUCT )
      return false;

   return expr.args.size() == 1 || s_isSetter( expr );
}

static size_t s_fieldIndex( const X_STRUCT_DECL &structure, const std::string &name )
{
   size_t i;

   for( i = 0; i < structure.fields.size(); ++i )
   {
      if( s_key( structure.fields[i].name ) == s_key( name ) )
         break;
   }

   return i;
}

/* Members called with parentheses */
static bool s_isMethod( const std::string &kind )
{
   return kind == "METHOD" || kind == "VIRTUAL" || kind == "CLASSMETHOD" || kind == "DELEGATE";
}

static std::string s_methodKey( const X_FUNCTION &function )
{
   return s_key( function.className ) + ":" + s_key( function.methodName );
}

/* ---------------------------------------------------------------------------------------------
   Calls
   --------------------------------------------------------------------------------------------- */

/* Declared type of parameter i; ANY when the function or the type is not known */
static X_DECL_TYPE s_parameterType( const X_FUNCTION *function, size_t i, std::string &typeName )
{
   if( !function || i >= function->params.size() )
      return X_DECL_TYPE::ANY;

   const std::string key = s_key( function->params[i] );
   const auto declared = function->paramTypes.find( key );
   const auto named = function->paramTypeNames.find( key );

   if( named != function->paramTypeNames.end() )
      typeName = named->second;

   if( declared != function->paramTypes.end() )
      return declared->second;

   return X_DECL_TYPE::ANY;
}

/* @st to a DLL: a STRUCT variable passed by pointer to a PTR or to the same STRUCT */
static bool s_structPointer( const X_EXPR &arg, X_DECL_TYPE parameter, const std::string &parameterName )
{
   if( arg.type != X_DECL_TYPE::STRUCT || arg.kind != X_EXPR_KIND::NAME )
      return false;

   if( parameter == X_DECL_TYPE::PTR )
      return true;

   return parameter == X_DECL_TYPE::STRUCT && parameterName == arg.typeName;
}

/* A typed parameter after the last argument has no default value */
static bool s_omittedTyped( const X_FUNCTION *function, size_t count )
{
   for( size_t i = count; function && i < function->params.size(); ++i )
   {
      if( function->paramTypes.count( s_key( function->params[i] ) ) != 0 )
         return true;
   }

   return false;
}

/* ---------------------------------------------------------------------------------------------
   Type checker
   --------------------------------------------------------------------------------------------- */

class X_TYPECHECKER
{
private:
   using TYPES = std::unordered_map<std::string, X_DECL_TYPE>;
   using TYPE_NAMES = std::unordered_map<std::string, std::string>;
   using VISITED = std::unordered_set<std::string>;

   /* Types and type names of the variables visible in a function */
   struct TYPE_SCOPE
   {
      TYPES types;
      TYPE_NAMES names;
   };

   std::string &m_error;
   const X_LOCATION &m_location;
   bool m_resolved = false;
   TYPES m_module;
   TYPE_NAMES m_moduleTypeNames;
   std::unordered_map<std::string, const X_STRUCT_DECL *> m_structs;
   std::unordered_map<std::string, const X_CLASS_DECL *> m_classes;
   std::unordered_map<std::string, const X_CALLBACK_DECL *> m_callbacks;
   std::unordered_map<std::string, const X_FUNCTION *> m_functions;
   std::unordered_map<std::string, const X_DLL_FUNCTION *> m_dllFunctions;
   std::unordered_map<std::string, const X_FUNCTION *> m_methods;
   std::unordered_map<std::string, const X_CLASS_MEMBER *> m_classData;
   std::unordered_set<std::string> m_importedFactories;

   bool fail( int line, const std::string &message )
   {
      if( m_location )
         m_error = m_location( line ) + ": " + message;
      else
         m_error = "linia " + std::to_string( line ) + ": " + message;

      return false;
   }

   /* Local scope first, then module STATIC */
   X_DECL_TYPE lookup( const TYPES &locals, const std::string &name ) const
   {
      const std::string key = s_key( name );
      const auto local = locals.find( key );
      const auto global = m_module.find( key );

      if( local != locals.end() )
         return local->second;

      if( global != m_module.end() )
         return global->second;

      return X_DECL_TYPE::ANY;
   }

   std::string lookupTypeName( const TYPE_NAMES &locals, const std::string &name ) const
   {
      const std::string key = s_key( name );
      const auto local = locals.find( key );
      const auto global = m_moduleTypeNames.find( key );

      if( local != locals.end() )
         return local->second;

      if( global != m_moduleTypeNames.end() )
         return global->second;

      return std::string();
   }

   /* Common type of two numbers; mixing signed and unsigned needs a signed type that holds both ranges */
   bool common( X_DECL_TYPE left, X_DECL_TYPE right, int line, const std::string &context, X_DECL_TYPE &result )
   {
      X_DECL_TYPE signedType;
      X_DECL_TYPE unsignedType;

      if( left == X_DECL_TYPE::ANY || right == X_DECL_TYPE::ANY )
      {
         result = X_DECL_TYPE::ANY;
         return true;
      }

      if( left == X_DECL_TYPE::FLOAT64 || right == X_DECL_TYPE::FLOAT64 )
      {
         result = X_DECL_TYPE::FLOAT64;
         return true;
      }

      /* FLOAT32 with a 32 or 64-bit integer becomes FLOAT64 */
      if( left == X_DECL_TYPE::FLOAT32 || right == X_DECL_TYPE::FLOAT32 )
      {
         if( s_width( left ) >= 32 || s_width( right ) >= 32 )
            result = X_DECL_TYPE::FLOAT64;
         else
            result = X_DECL_TYPE::FLOAT32;

         return true;
      }

      if( s_unsigned( left ) == s_unsigned( right ) )
      {
         if( s_width( left ) >= s_width( right ) )
            result = s_promote( left );
         else
            result = s_promote( right );

         return true;
      }

      signedType = s_promote( IIF( s_unsigned( left ), right, left ) );
      unsignedType = IIF( s_unsigned( left ), left, right );

      if( s_width( signedType ) > s_width( unsignedType ) )
      {
         result = signedType;
         return true;
      }

      if( s_width( unsignedType ) < 64 )
      {
         result = X_DECL_TYPE::INT64;
         return true;
      }

      return fail( line, context + ": mieszanie " + s_typePair( left, right ) + s_mixedSigns );
   }

   /* ---------------------------------------------------------------------------------------------
      Classes
      --------------------------------------------------------------------------------------------- */

   bool unknownClass( const std::string &name ) const
   {
      return !name.empty() && !m_classes.count( s_key( name ) );
   }

   bool inherits( const std::string &name, const std::string &target, VISITED &visited ) const
   {
      const std::string key = s_key( name );
      const auto found = m_classes.find( key );

      if( key == target )
         return true;

      if( !visited.insert( key ).second || found == m_classes.end() )
         return false;

      for( const std::string &parent : found->second->parents )
      {
         if( inherits( parent, target, visited ) )
            return true;
      }

      return false;
   }

   /* source is target or inherits from it */
   bool classAssignable( const std::string &source, const std::string &target ) const
   {
      VISITED visited;

      return inherits( source, s_key( target ), visited );
   }

   /* The last declaration in the class wins, then the parents in order */
   const X_CLASS_MEMBER *findMember( const std::string &name, const std::string &member, VISITED &visited ) const
   {
      const std::string key = s_key( name );
      const auto found = m_classes.find( key );

      if( !visited.insert( key ).second || found == m_classes.end() )
         return nullptr;

      const std::vector<X_CLASS_MEMBER> &members = found->second->members;

      for( size_t i = members.size(); i > 0; --i )
      {
         if( s_key( members[i - 1].name ) == member )
            return &members[i - 1];
      }

      for( const std::string &parent : found->second->parents )
      {
         const X_CLASS_MEMBER *inherited = findMember( parent, member, visited );

         if( inherited )
            return inherited;
      }

      return nullptr;
   }

   const X_CLASS_MEMBER *classMember( const std::string &className, const std::string &memberName ) const
   {
      VISITED visited;

      return findMember( className, s_key( memberName ), visited );
   }

   bool hasOnError( const std::string &name, VISITED &visited ) const
   {
      const std::string key = s_key( name );
      const auto found = m_classes.find( key );

      if( !visited.insert( key ).second || found == m_classes.end() )
         return false;

      for( const X_CLASS_MEMBER &member : found->second->members )
      {
         if( member.kind == "ONERROR" )
            return true;
      }

      for( const std::string &parent : found->second->parents )
      {
         if( hasOnError( parent, visited ) )
            return true;
      }

      return false;
   }

   bool classHasOnError( const std::string &className ) const
   {
      VISITED visited;

      return hasOnError( className, visited );
   }

   /* A message without a member goes to ON ERROR when the class has one */
   bool onErrorSend( const X_EXPR &expr ) const
   {
      const X_EXPR &receiver = *expr.args[0];
      const std::string name = IIF( s_isSetter( expr ), expr.text.substr( 1 ), expr.text );

      if( classMember( receiver.typeName, name ) )
         return false;

      return classHasOnError( receiver.typeName );
   }

   /* The method body of the class or of the nearest parent */
   const X_FUNCTION *findMethod( const std::string &name, const std::string &method, VISITED &visited ) const
   {
      const std::string key = s_key( name );
      const auto own = m_methods.find( key + ":" + method );
      const auto found = m_classes.find( key );

      if( !visited.insert( key ).second )
         return nullptr;

      if( own != m_methods.end() )
         return own->second;

      if( found == m_classes.end() )
         return nullptr;

      for( const std::string &parent : found->second->parents )
      {
         const X_FUNCTION *inherited = findMethod( parent, method, visited );

         if( inherited )
            return inherited;
      }

      return nullptr;
   }

   const X_FUNCTION *classMethod( const std::string &className, const std::string &methodName ) const
   {
      VISITED visited;

      return findMethod( className, s_key( methodName ), visited );
   }

   /* ---------------------------------------------------------------------------------------------
      Conversions
      --------------------------------------------------------------------------------------------- */

   /* @Function() for a CALLBACK: the parameter types and the result must match the signature */
   bool callbackAddress( X_EXPR &expr, const std::string &targetName, const std::string &context )
   {
      const auto callback = m_callbacks.find( s_key( targetName ) );
      const auto function = m_functions.find( s_key( expr.text ) );

      if( callback == m_callbacks.end() )
         return fail( expr.line, context + ": nieznany typ CALLBACK " + targetName );

      if( function == m_functions.end() || function->second->isDll || !function->second->className.empty() )
         return fail( expr.line, context + ": @" + expr.text + "() nie wskazuje modulowej funkcji xDev" );

      const X_CALLBACK_DECL &signature = *callback->second;
      const X_FUNCTION &target = *function->second;

      if( signature.params.size() != target.params.size() )
         return fail( expr.line, context + ": niezgodna liczba parametrow funkcji " + expr.text );

      for( size_t i = 0; i < signature.params.size(); ++i )
      {
         const auto declared = signature.paramTypes.find( s_key( signature.params[i] ) );
         const auto found = target.paramTypes.find( s_key( target.params[i] ) );
         X_DECL_TYPE expected;

         if( declared == signature.paramTypes.end() )
            return fail( expr.line, context + ": brak typu parametru CALLBACK: " + signature.params[i] );

         /* PSZ of C arrives as STRING */
         expected = IIF( declared->second == X_DECL_TYPE::PSZ, X_DECL_TYPE::STRING, declared->second );

         if( found == target.paramTypes.end() || found->second != expected )
         {
            std::string message = context + ": parametr " + std::to_string( i + 1 ) + " funkcji " + expr.text;

            return fail( expr.line, message + " wymaga typu " + x_declTypeName( expected ) );
         }
      }

      if( signature.isProcedure != target.isProcedure )
         return fail( expr.line, context + s_callbackKind );

      if( !signature.isProcedure && ( !target.resultType || *target.resultType != *signature.resultType ) )
         return fail( expr.line, context + ": niezgodny typ wyniku funkcji " + expr.text );

      expr.type = X_DECL_TYPE::CALLBACK;
      expr.typeName = s_key( targetName );
      expr.nativeType = true;
      expr.conversion.reset();

      return true;
   }

   bool conversion( X_DECL_TYPE source, X_DECL_TYPE target, int line, const std::string &context,
                    std::optional<X_TYPE_CONVERSION> &result, bool cast = false )
   {
      X_TYPE_CONVERSION convert{ target };

      result.reset();

      if( source == target )
         return true;

      if( source == X_DECL_TYPE::STRUCT || target == X_DECL_TYPE::STRUCT )
         return fail( line, context + s_noStruct );

      if( !s_convertible( source, target, cast ) )
      {
         std::string types = std::string( x_declTypeName( source ) ) + " -> " + x_declTypeName( target );

         return fail( line, context + ": niezgodne typy " + types );
      }

      convert.explicitCast = cast;
      convert.checkType = source == X_DECL_TYPE::ANY && target != X_DECL_TYPE::ANY;

      if( s_integer( target ) && source != X_DECL_TYPE::LOGIC )
         convert.checkRange = s_needsRangeCheck( source, target );

      result = convert;

      return true;
   }

   /* CALLBACK takes @Function(), NIL or a value of the same CALLBACK type */
   bool coerceCallback( X_EXPR &expr, const std::string &name, const std::string &context, bool cast )
   {
      if( expr.kind == X_EXPR_KIND::FUNCTION_ADDRESS )
         return callbackAddress( expr, name, context );

      if( expr.type == X_DECL_TYPE::NIL )
      {
         expr.typeName = s_key( name );
         return conversion( expr.type, X_DECL_TYPE::CALLBACK, expr.line, context, expr.conversion, cast );
      }

      if( expr.type == X_DECL_TYPE::CALLBACK && s_key( expr.typeName ) == s_key( name ) )
         return true;

      return fail( expr.line, context + ": niezgodny typ CALLBACK " + expr.typeName + " i " + name );
   }

   /* OBJECT takes NIL, a class that inherits from the target class, or ANY checked at runtime */
   bool coerceObject( X_EXPR &expr, X_DECL_TYPE target, const std::string &context, const std::string &targetName )
   {
      const std::string required = s_objectTarget( target, targetName );

      if( target == X_DECL_TYPE::OBJECT )
      {
         const std::string &className = IIF( targetName.empty(), expr.typeName, targetName );

         if( expr.type == X_DECL_TYPE::OBJECT && classAssignable( expr.typeName, className ) )
            return true;

         if( expr.type == X_DECL_TYPE::NIL )
            return true;

         if( expr.type == X_DECL_TYPE::ANY || ( expr.type == X_DECL_TYPE::OBJECT && expr.typeName.empty() ) )
         {
            X_TYPE_CONVERSION convert{ X_DECL_TYPE::OBJECT };

            convert.checkType = true;
            expr.conversion = convert;
            expr.conversionClass = s_key( targetName );
            return true;
         }
      }

      if( expr.type == X_DECL_TYPE::OBJECT && target == X_DECL_TYPE::ANY )
         return true;

      return fail( expr.line, context + ": niezgodne typy OBJECT " + expr.typeName + " i " + required );
   }

   /* expr gets the conversion to target; CALLBACK, STRUCT and OBJECT need the same named type */
   bool coerce( X_EXPR &expr, X_DECL_TYPE target, const std::string &context, bool cast = false,
                const std::string &targetName = {} )
   {
      if( target == X_DECL_TYPE::CALLBACK )
         return coerceCallback( expr, targetName, context, cast );

      if( expr.type == X_DECL_TYPE::CALLBACK )
         return fail( expr.line, context + ": niezgodny typ CALLBACK " + expr.typeName + " i " + targetName );

      if( expr.type == X_DECL_TYPE::STRUCT || target == X_DECL_TYPE::STRUCT )
      {
         if( expr.type != X_DECL_TYPE::STRUCT || target != X_DECL_TYPE::STRUCT )
            return fail( expr.line, context + ": STRUCT nie moze przejsc do ANY ani innego typu" );

         if( !targetName.empty() && expr.typeName != targetName )
            return fail( expr.line, context + ": niezgodne typy STRUCT " + expr.typeName + " i " + targetName );

         expr.conversion.reset();
         return true;
      }

      if( expr.type == X_DECL_TYPE::OBJECT || target == X_DECL_TYPE::OBJECT )
         return coerceObject( expr, target, context, targetName );

      return conversion( expr.type, target, expr.line, context, expr.conversion, cast );
   }

   /* ---------------------------------------------------------------------------------------------
      Declarations
      --------------------------------------------------------------------------------------------- */

   bool collectDecl( const X_STMT &stmt, const X_LOCAL_DECL &decl, TYPE_SCOPE &scope, bool moduleScope )
   {
      const X_DECL_TYPE type = decl.declaredType.value_or( X_DECL_TYPE::ANY );
      const std::string key = s_key( decl.name );

      if( type == X_DECL_TYPE::PSZ )
         return fail( decl.line, "PSZ jest typem ABI DLL; uzyj STRING dla zmiennej xDev" );

      if( type == X_DECL_TYPE::STRUCT && stmt.kind == X_STMT_KIND::STATIC )
         return fail( decl.line, "STRUCT obslugiwany jest obecnie jako LOCAL" );

      /* Module STATIC are registered in registerStatics() */
      if( stmt.kind != X_STMT_KIND::STATIC || !moduleScope )
      {
         /* After name resolution the scope already holds every declaration */
         if( !m_resolved )
         {
            const auto inserted = scope.types.emplace( key, type );

            if( !inserted.second && ( type != X_DECL_TYPE::ANY || inserted.first->second != X_DECL_TYPE::ANY ) )
               return fail( decl.line, "powtorzona deklaracja zmiennej z typem: " + decl.name );
         }

         if( s_namedType( type ) )
            scope.names[key] = decl.declaredTypeName;

         if( type == X_DECL_TYPE::OBJECT && unknownClass( decl.declaredTypeName ) )
            return fail( decl.line, "nieznany typ OBJECT: " + decl.declaredTypeName );
      }

      if( !decl.dimensions.empty() && type != X_DECL_TYPE::ANY )
         return fail( decl.line, "deklaracja tablicowa wymaga ANY: " + decl.name );

      return true;
   }

   /* LOCAL and STATIC anywhere in the body */
   bool collect( const std::vector<X_STMT> &body, TYPE_SCOPE &scope, bool moduleScope = false )
   {
      for( const X_STMT &stmt : body )
      {
         if( stmt.kind == X_STMT_KIND::LOCAL || stmt.kind == X_STMT_KIND::STATIC )
         {
            for( const X_LOCAL_DECL &decl : stmt.locals )
            {
               if( !collectDecl( stmt, decl, scope, moduleScope ) )
                  return false;
            }
         }

         if( !collect( stmt.body, scope, moduleScope ) || !collect( stmt.elseBody, scope, moduleScope ) )
            return false;

         if( !collect( stmt.otherwiseBody, scope, moduleScope ) )
            return false;

         for( const X_CASE_BRANCH &branch : stmt.cases )
         {
            if( !collect( branch.body, scope, moduleScope ) )
               return false;
         }
      }

      return true;
   }

   /* ---------------------------------------------------------------------------------------------
      Operators
      --------------------------------------------------------------------------------------------- */

   /* Two CALLBACK operands need the same named type; @Function() alone has no type name */
   bool sameCallback( const X_EXPR &expr, const std::string &op, const X_EXPR &left, const X_EXPR &right )
   {
      const std::string &leftName = left.typeName;
      const std::string &rightName = right.typeName;

      if( leftName.empty() && rightName.empty() )
         return fail( expr.line, "operator " + op + ": adresy funkcji CALLBACK wymagaja kontekstu nazwanego typu" );

      if( !leftName.empty() && !rightName.empty() && s_key( leftName ) != s_key( rightName ) )
         return fail( expr.line, "operator " + op + ": niezgodne typy CALLBACK " + leftName + " i " + rightName );

      return true;
   }

   /* Numbers without native types are computed at runtime; only the result type is known */
   bool dynamicNumeric( X_EXPR &expr, const std::string &op, bool comparison, X_EXPR &left, X_EXPR &right )
   {
      const bool integers = s_integer( left.type ) && s_integer( right.type );

      expr.operationType = X_DECL_TYPE::ANY;

      if( comparison )
         expr.type = X_DECL_TYPE::LOGIC;
      else if( op == "/" || op == "^" || !integers )
         expr.type = X_DECL_TYPE::FLOAT64;
      else if( op == "%" )
         expr.type = X_DECL_TYPE::INT64;
      else
         expr.type = X_DECL_TYPE::ANY;

      return coerce( left, X_DECL_TYPE::ANY, "operator " + op ) && coerce( right, X_DECL_TYPE::ANY, "operator " + op );
   }

   bool binary( X_EXPR &expr, const std::string &op, X_EXPR &left, X_EXPR &right )
   {
      const X_DECL_TYPE a = left.type;
      const X_DECL_TYPE b = right.type;
      const bool equality = op == "=" || op == "==" || op == "!=";
      const bool comparison = equality || op == "<" || op == "<=" || op == ">" || op == ">=";
      const std::string context = "operator " + op;
      X_DECL_TYPE operation = X_DECL_TYPE::ANY;
      std::string operationTypeName;

      if( a == X_DECL_TYPE::STRUCT || b == X_DECL_TYPE::STRUCT )
         return fail( expr.line, "operatory nie sa zdefiniowane dla STRUCT" );

      expr.nativeType = left.nativeType || right.nativeType;

      if( !equality && ( s_isAddress( a ) || s_isAddress( b ) ) )
         return fail( expr.line, context + " nie jest dozwolony dla PTR/CALLBACK" );

      if( op == ".AND." || op == ".OR." )
         operation = X_DECL_TYPE::LOGIC;
      else if( equality && s_callbackPair( a, b ) )
      {
         if( a == X_DECL_TYPE::CALLBACK && b == X_DECL_TYPE::CALLBACK && !sameCallback( expr, op, left, right ) )
            return false;

         operation = X_DECL_TYPE::CALLBACK;
         operationTypeName = IIF( !left.typeName.empty(), left.typeName, right.typeName );
      }
      else if( equality && s_pointerPair( a, b ) )
         operation = X_DECL_TYPE::PTR;
      else if( op == "$" )
         operation = X_DECL_TYPE::STRING;
      else if( a == X_DECL_TYPE::DATE || b == X_DECL_TYPE::DATE )
         operation = X_DECL_TYPE::ANY;
      else if( a == X_DECL_TYPE::ANY || b == X_DECL_TYPE::ANY )
         operation = X_DECL_TYPE::ANY;
      else if( s_numeric( a ) && s_numeric( b ) )
      {
         if( expr.nativeType && op == "%" && ( !s_integer( a ) || !s_integer( b ) ) )
            return fail( expr.line, "operator % wymaga typow calkowitych" );

         if( !expr.nativeType )
            return dynamicNumeric( expr, op, comparison, left, right );

         if( !common( a, b, expr.line, context, operation ) )
            return false;

         if( op == "^" || op == "/" )
            operation = X_DECL_TYPE::FLOAT64;
      }
      else if( equality && ( a == X_DECL_TYPE::NIL || b == X_DECL_TYPE::NIL ) )
         operation = X_DECL_TYPE::ANY;
      else if( equality && a == X_DECL_TYPE::OBJECT && b == X_DECL_TYPE::OBJECT )
         operation = X_DECL_TYPE::ANY;
      else if( a == b && s_sameTypeOperator( a, op, comparison ) )
         operation = a;
      else
         return fail( expr.line, context + ": niezgodne typy " + s_typePair( a, b ) );

      if( !coerce( left, operation, context, false, operationTypeName ) )
         return false;

      if( !coerce( right, operation, context, false, operationTypeName ) )
         return false;

      expr.operationType = operation;
      expr.checkOverflow = !comparison && s_integer( operation );

      if( comparison || op == "$" || op == ".AND." || op == ".OR." )
         expr.type = X_DECL_TYPE::LOGIC;
      else
         expr.type = operation;

      /* PTR and CALLBACK stay native; other operations only when their type is known */
      expr.nativeType = ( expr.nativeType && operation != X_DECL_TYPE::ANY ) || s_isAddress( operation );

      return true;
   }

   /* ---------------------------------------------------------------------------------------------
      Expressions
      --------------------------------------------------------------------------------------------- */

   bool literal( X_EXPR &expr, X_DECL_TYPE type )
   {
      expr.type = type;

      return true;
   }

   bool arrayLiteral( X_EXPR &expr )
   {
      for( const X_EXPR_PTR &arg : expr.args )
      {
         if( arg->type == X_DECL_TYPE::STRUCT )
            return fail( expr.line, "STRUCT nie moze byc elementem ARRAY" );
      }

      expr.type = X_DECL_TYPE::ARRAY;

      return true;
   }

   bool name( X_EXPR &expr, const TYPE_SCOPE &scope )
   {
      expr.type = lookup( scope.types, expr.text );
      expr.typeName = lookupTypeName( scope.names, expr.text );
      expr.nativeType = expr.type != X_DECL_TYPE::ANY;

      return true;
   }

   bool functionAddress( X_EXPR &expr )
   {
      if( !m_functions.count( s_key( expr.text ) ) )
         return fail( expr.line, "nieistniejaca funkcja w @" + expr.text + "()" );

      expr.type = X_DECL_TYPE::CALLBACK;
      expr.nativeType = true;

      return true;
   }

   /* CAST( value AS type ): the parser has set the type */
   bool cast( X_EXPR &expr )
   {
      expr.nativeType = expr.type != X_DECL_TYPE::ANY;

      return coerce( *expr.args[0], expr.type, "CAST", true );
   }

   /* Block parameters are ANY inside the block */
   bool block( X_EXPR &expr, const TYPE_SCOPE &scope )
   {
      TYPE_SCOPE nested = scope;

      for( const std::string &param : expr.blockParams )
         nested.types[s_key( param )] = X_DECL_TYPE::ANY;

      for( X_EXPR_PTR &arg : expr.args )
      {
         if( !expression( *arg, nested ) )
            return false;
      }

      expr.type = X_DECL_TYPE::CODEBLOCK;

      return true;
   }

   bool fieldAlias( X_EXPR &expr )
   {
      if( !coerce( *expr.args[0], X_DECL_TYPE::ANY, "alias pola" ) )
         return false;

      expr.type = X_DECL_TYPE::ANY;

      return true;
   }

   bool alias( X_EXPR &expr )
   {
      if( !coerce( *expr.args[0], X_DECL_TYPE::ANY, "alias" ) )
         return false;

      expr.type = expr.args.back()->type;
      expr.nativeType = expr.args.back()->nativeType;

      return true;
   }

   /* ( a, b, c ) has the value of the last expression */
   bool group( X_EXPR &expr )
   {
      expr.type = expr.args.back()->type;
      expr.typeName = expr.args.back()->typeName;
      expr.nativeType = expr.args.back()->nativeType;

      return true;
   }

   /* st.field[n] of a fixed array field: the index is an integer, the element is native */
   bool structIndex( X_EXPR &expr )
   {
      const X_EXPR &field = *expr.args[0];

      if( !coerce( *expr.args[1], X_DECL_TYPE::INT64, "indeks tablicy STRUCT" ) )
         return false;

      expr.structArrayIndex = true;
      expr.fixedArrayCount = field.fixedArrayCount;
      expr.type = field.arrayElementType;
      expr.typeName = field.arrayElementTypeName;
      expr.nativeType = true;

      return true;
   }

   bool subscript( X_EXPR &expr )
   {
      const X_EXPR &container = *expr.args[0];
      X_DECL_TYPE key;

      if( container.structFieldAccess && container.fixedArrayCount != 0 )
         return structIndex( expr );

      if( !s_indexable( container.type ) )
         return fail( expr.line, "indeksowanie wymaga tablicy" );

      /* HASH and ANY (which may hold a hash) accept a key of any type; the runtime checks it */
      key = IIF( container.type == X_DECL_TYPE::ARRAY, X_DECL_TYPE::INT64, X_DECL_TYPE::ANY );
      if( !coerce( *expr.args[1], key, "indeks tablicy" ) )
         return false;

      expr.type = X_DECL_TYPE::ANY;

      return true;
   }

   bool unary( X_EXPR &expr )
   {
      X_EXPR &arg = *expr.args[0];

      expr.nativeType = arg.nativeType;

      if( expr.text == ".NOT." )
      {
         if( !coerce( arg, X_DECL_TYPE::LOGIC, "operator .NOT." ) )
            return false;

         expr.type = X_DECL_TYPE::LOGIC;
         expr.operationType = IIF( expr.nativeType, expr.type, X_DECL_TYPE::ANY );
         return true;
      }

      /* Unary + of a dynamic value changes nothing */
      if( expr.text == "+" && !expr.nativeType )
      {
         expr.type = arg.type;
         return true;
      }

      if( arg.type != X_DECL_TYPE::ANY && !s_numeric( arg.type ) )
         return fail( expr.line, "operator " + expr.text + " wymaga liczby" );

      if( expr.nativeType )
         expr.type = s_promote( arg.type );
      else if( arg.type == X_DECL_TYPE::INT64 )
         expr.type = X_DECL_TYPE::ANY;
      else
         expr.type = arg.type;

      if( !coerce( arg, expr.type, "operator " + expr.text ) )
         return false;

      expr.checkOverflow = expr.text == "-" && s_integer( expr.type );
      expr.operationType = IIF( expr.nativeType, expr.type, X_DECL_TYPE::ANY );

      return true;
   }

   bool assign( X_EXPR &expr )
   {
      X_EXPR &target = *expr.args[0];
      X_EXPR &value = *expr.args[1];
      const X_DECL_TYPE type = target.type;

      if( expr.assignOp == X_ASSIGN_OP::SET )
      {
         if( !coerce( value, type, "przypisanie", false, target.typeName ) )
            return false;
      }
      else
      {
         /* x += y is checked as x + y, then stored back into x */
         if( !binary( expr, s_assignOperator( expr.assignOp ), target, value ) )
            return false;

         if( !conversion( expr.type, type, expr.line, "przypisanie z operatorem", expr.storeConversion ) )
            return false;
      }

      expr.type = type;
      expr.typeName = target.typeName;
      expr.nativeType = target.nativeType;

      return true;
   }

   /* ++ and -- compute in the promoted type and store back into the variable type */
   bool update( X_EXPR &expr )
   {
      X_EXPR &arg = *expr.args[0];

      if( arg.type != X_DECL_TYPE::ANY && !s_numeric( arg.type ) )
         return fail( expr.line, "operator ++/-- wymaga liczby" );

      expr.type = arg.type;
      expr.nativeType = arg.nativeType;
      expr.operationType = s_promote( arg.type );
      expr.checkOverflow = s_integer( expr.operationType );

      if( !coerce( arg, expr.operationType, "operator ++/--" ) )
         return false;

      return conversion( expr.operationType, expr.type, expr.line, "operator ++/--", expr.storeConversion );
   }

   /* Two OBJECT values: the class that the other one inherits from, else ANY */
   void iifClass( X_EXPR &expr, const std::string &leftName, const std::string &rightName )
   {
      expr.type = X_DECL_TYPE::OBJECT;

      if( classAssignable( rightName, leftName ) )
         expr.typeName = leftName;
      else if( classAssignable( leftName, rightName ) )
         expr.typeName = rightName;
      else
         expr.type = X_DECL_TYPE::ANY;
   }

   /* Result type of IIF: the type of both values, a named type with NIL, a common number type, else ANY */
   bool iifType( X_EXPR &expr, const X_EXPR &first, const X_EXPR &second )
   {
      const X_DECL_TYPE left = first.type;
      const X_DECL_TYPE right = second.type;

      if( left == right && left == X_DECL_TYPE::OBJECT )
      {
         iifClass( expr, first.typeName, second.typeName );
         return true;
      }

      if( left == right )
      {
         expr.type = left;
         expr.typeName = first.typeName;
         return true;
      }

      if( s_withNil( left, right, X_DECL_TYPE::OBJECT ) || s_withNil( left, right, X_DECL_TYPE::CALLBACK ) )
      {
         expr.type = IIF( left == X_DECL_TYPE::NIL, right, left );
         expr.typeName = IIF( left == X_DECL_TYPE::NIL, second.typeName, first.typeName );
         return true;
      }

      if( expr.nativeType && s_numeric( left ) && s_numeric( right ) )
         return common( left, right, expr.line, "wynik IIF", expr.type );

      expr.type = X_DECL_TYPE::ANY;

      return true;
   }

   bool iif( X_EXPR &expr )
   {
      const X_EXPR &first = *expr.args[1];
      const X_EXPR &second = *expr.args[2];

      if( !coerce( *expr.args[0], X_DECL_TYPE::LOGIC, "warunek IIF" ) )
         return false;

      if( first.type == X_DECL_TYPE::STRUCT || second.type == X_DECL_TYPE::STRUCT )
         return fail( expr.line, "IIF nie obsluguje STRUCT; przypisz strukture bezposrednio" );

      expr.nativeType = first.nativeType || second.nativeType;

      if( !iifType( expr, first, second ) )
         return false;

      expr.nativeType = expr.nativeType && expr.type != X_DECL_TYPE::ANY && expr.type != X_DECL_TYPE::OBJECT;

      if( !coerce( *expr.args[1], expr.type, "wynik IIF", false, expr.typeName ) )
         return false;

      return coerce( *expr.args[2], expr.type, "wynik IIF", false, expr.typeName );
   }

   bool expression( X_EXPR &expr, const TYPE_SCOPE &scope )
   {
      if( expr.kind == X_EXPR_KIND::BLOCK )
         return block( expr, scope );

      for( X_EXPR_PTR &arg : expr.args )
      {
         if( !expression( *arg, scope ) )
            return false;
      }

      switch( expr.kind )
      {
      case X_EXPR_KIND::NIL:              return literal( expr, X_DECL_TYPE::NIL );
      case X_EXPR_KIND::NUMBER:           return literal( expr, s_numberType( expr ) );
      case X_EXPR_KIND::STRING:           return literal( expr, X_DECL_TYPE::STRING );
      case X_EXPR_KIND::LOGICAL:          return literal( expr, X_DECL_TYPE::LOGIC );
      case X_EXPR_KIND::ARRAY:            return arrayLiteral( expr );
      case X_EXPR_KIND::NAME:             return name( expr, scope );
      case X_EXPR_KIND::FUNCTION_ADDRESS: return functionAddress( expr );
      case X_EXPR_KIND::CAST:             return cast( expr );
      case X_EXPR_KIND::CALL:             return call( expr );
      case X_EXPR_KIND::SEND:             return send( expr );
      case X_EXPR_KIND::FIELD:            return fieldAlias( expr );
      case X_EXPR_KIND::ALIAS:            return alias( expr );
      case X_EXPR_KIND::GROUP:            return group( expr );
      case X_EXPR_KIND::INDEX:            return subscript( expr );
      case X_EXPR_KIND::UNARY:            return unary( expr );
      case X_EXPR_KIND::BINARY:           return binary( expr, expr.text, *expr.args[0], *expr.args[1] );
      case X_EXPR_KIND::ASSIGN:           return assign( expr );
      case X_EXPR_KIND::UPDATE:           return update( expr );
      case X_EXPR_KIND::IIF:              return iif( expr );
      case X_EXPR_KIND::BLOCK:            return true;
      }

      return true;
   }

   /* ---------------------------------------------------------------------------------------------
      Calls
      --------------------------------------------------------------------------------------------- */

   /* Argument i: @ passes a variable of the same type; a DLL takes @ only for a STRUCT */
   bool argument( X_EXPR &expr, const X_FUNCTION *function, bool dll, size_t i )
   {
      X_EXPR &arg = *expr.args[i];
      std::string parameterName;
      const X_DECL_TYPE parameter = s_parameterType( function, i, parameterName );
      const bool reference = i < expr.argByReference.size() && expr.argByReference[i];
      const std::string context = "argument " + std::to_string( i + 1 ) + " funkcji " + expr.text;

      if( reference && dll && s_structPointer( arg, parameter, parameterName ) )
         return true;

      if( reference && dll )
         return fail( arg.line, "DLL FUNCTION nie przyjmuje @ poza przekazaniem STRUCT przez wskaznik" );

      if( reference && arg.type != X_DECL_TYPE::ANY && parameter != X_DECL_TYPE::ANY && arg.type != parameter )
         return fail( arg.line, context + ": parametr @ wymaga identycznego typu" );

      if( !coerce( arg, parameter, context, false, parameterName ) )
         return false;

      if( reference && !conversion( parameter, arg.type, arg.line, context, arg.storeConversion ) )
         return false;

      return true;
   }

   /* Missing arguments must not have a type; a class factory returns its OBJECT */
   bool callResult( X_EXPR &expr, const X_FUNCTION &function )
   {
      for( size_t i = expr.args.size(); i < function.params.size(); ++i )
      {
         const auto declared = function.paramTypes.find( s_key( function.params[i] ) );

         if( declared != function.paramTypes.end() && declared->second != X_DECL_TYPE::ANY )
            return fail( expr.line, "brak argumentu z typem: " + expr.text + ", " + function.params[i] );
      }

      if( !function.factoryClass.empty() )
      {
         expr.type = X_DECL_TYPE::OBJECT;
         expr.typeName = function.factoryClass;
         expr.nativeType = false;
         return true;
      }

      if( function.isProcedure )
         expr.type = X_DECL_TYPE::NIL;
      else
         expr.type = function.resultType.value_or( X_DECL_TYPE::ANY );

      expr.typeName = function.resultTypeName;
      expr.nativeType = !function.isProcedure && expr.type != X_DECL_TYPE::ANY && expr.type != X_DECL_TYPE::OBJECT;

      return true;
   }

   bool call( X_EXPR &expr )
   {
      const std::string key = s_key( expr.text );
      const auto found = m_functions.find( key );
      const auto dll = m_dllFunctions.find( key );
      const X_FUNCTION *function = nullptr;
      const bool isDll = dll != m_dllFunctions.end();

      if( found != m_functions.end() )
         function = found->second;

      if( isDll && expr.args.size() != dll->second->params.size() )
      {
         std::string count = std::to_string( dll->second->params.size() );

         return fail( expr.line, "DLL FUNCTION wymaga dokladnie " + count + " argumentow: " + expr.text );
      }

      for( size_t i = 0; i < expr.args.size(); ++i )
      {
         if( !argument( expr, function, isDll, i ) )
            return false;
      }

      if( function )
         return callResult( expr, *function );

      /* Factory of an imported class */
      if( expr.args.empty() && m_importedFactories.count( key ) )
      {
         expr.type = X_DECL_TYPE::OBJECT;
         expr.typeName = key;
         expr.nativeType = false;
         return true;
      }

      expr.type = X_DECL_TYPE::ANY;

      return true;
   }

   /* ---------------------------------------------------------------------------------------------
      Messages
      --------------------------------------------------------------------------------------------- */

   /* A message to an OBJECT of a known class is checked against the class */
   bool knownClassSend( const X_EXPR &expr ) const
   {
      if( expr.args.empty() || expr.sendToSuper )
         return false;

      if( expr.args[0]->type != X_DECL_TYPE::OBJECT || expr.args[0]->typeName.empty() )
         return false;

      return !onErrorSend( expr );
   }

   /* Too many arguments, or a missing typed argument, is an error */
   bool methodCall( X_EXPR &expr, const X_CLASS_MEMBER &member )
   {
      const std::string &className = expr.args[0]->typeName;
      const X_FUNCTION *function = classMethod( className, expr.text );
      const size_t count = expr.args.size() - 1;

      if( count > member.params.size() || s_omittedTyped( function, count ) )
      {
         std::string required = std::to_string( member.params.size() );

         return fail( expr.line, "metoda " + className + ":" + expr.text + " wymaga " + required + " argumentow" );
      }

      for( size_t i = 1; i < expr.args.size(); ++i )
      {
         std::string parameterName;
         const X_DECL_TYPE parameter = s_parameterType( function, i - 1, parameterName );

         if( !coerce( *expr.args[i], parameter, "argument metody " + expr.text, false, parameterName ) )
            return false;
      }

      expr.type = X_DECL_TYPE::ANY;
      expr.typeName.clear();
      expr.nativeType = false;

      if( function && !function->isProcedure )
         expr.type = function->resultType.value_or( X_DECL_TYPE::ANY );

      if( function )
         expr.typeName = function->resultTypeName;

      return true;
   }

   /* DATA and CLASSDATA have their declared type; other members are ANY */
   bool dataAccess( X_EXPR &expr, const X_CLASS_MEMBER &member, bool setting, const std::string &memberName )
   {
      const bool data = member.kind == "DATA" || member.kind == "CLASSDATA";

      if( data && setting )
      {
         if( !coerce( *expr.args[1], member.type, "przypisanie pola " + memberName, false, member.typeName ) )
            return false;
      }

      expr.type = IIF( data, member.type, X_DECL_TYPE::ANY );
      expr.typeName = IIF( data, member.typeName, std::string() );
      expr.nativeType = false;

      return true;
   }

   bool classSend( X_EXPR &expr )
   {
      const X_EXPR &receiver = *expr.args[0];
      const bool setting = s_isSetter( expr );
      const std::string memberName = IIF( setting, expr.text.substr( 1 ), expr.text );
      const X_CLASS_MEMBER *member = classMember( receiver.typeName, memberName );
      bool method;

      if( !member )
         return fail( expr.line, "brak skladowej " + memberName + " w klasie " + receiver.typeName );

      method = s_isMethod( member->kind );

      if( method && !expr.sendHasParentheses )
         return fail( expr.line, "metoda wymaga nawiasow: " + expr.text );

      if( !method && expr.sendHasParentheses )
         return fail( expr.line, "skladowa danych nie jest wywolaniem: " + expr.text );

      if( method )
         return methodCall( expr, *member );

      return dataAccess( expr, *member, setting, memberName );
   }

   bool structField( X_EXPR &expr )
   {
      const std::string &structName = expr.args[0]->typeName;
      const auto structure = m_structs.find( structName );
      const bool setting = expr.args.size() == 2;
      const std::string fieldName = IIF( setting, expr.text.substr( 1 ), expr.text );
      const X_STRUCT_FIELD *field;
      size_t index;

      if( structure == m_structs.end() )
         return fail( expr.line, "nieznany STRUCT: " + structName );

      index = s_fieldIndex( *structure->second, fieldName );
      if( index == structure->second->fields.size() )
         return fail( expr.line, "brak pola " + fieldName + " w STRUCT " + structName );

      field = &structure->second->fields[index];
      if( setting && field->arrayCount != 0 )
         return fail( expr.line, "tablica STRUCT wymaga przypisania elementow" );

      expr.structFieldAccess = true;
      expr.structFieldIndex = index;
      expr.containerTypeName = structName;
      expr.typeName = field->typeName;
      expr.fixedArrayCount = field->arrayCount;
      expr.type = field->type;
      expr.nativeType = true;

      /* A fixed array field: the elements have the field type */
      if( field->arrayCount != 0 )
      {
         expr.type = X_DECL_TYPE::ARRAY;
         expr.arrayElementType = field->type;
         expr.arrayElementTypeName = field->typeName;
      }

      if( setting && !coerce( *expr.args[1], field->type, "przypisanie pola STRUCT", false, field->typeName ) )
         return false;

      return true;
   }

   bool send( X_EXPR &expr )
   {
      if( knownClassSend( expr ) )
         return classSend( expr );

      if( s_structField( expr ) )
         return structField( expr );

      if( !expr.args.empty() && expr.args[0]->type == X_DECL_TYPE::STRUCT )
         return fail( expr.line, "STRUCT nie obsluguje komunikatow; uzyj dostepu wartosc.pole" );

      if( expr.dotMemberAccess )
         return fail( expr.line, "dostep przez kropke wymaga znanej zmiennej STRUCT" );

      /* Any other message is sent at runtime */
      for( size_t i = 0; i < expr.args.size(); ++i )
      {
         X_EXPR &arg = *expr.args[i];
         const bool reference = i > 0 && i - 1 < expr.argByReference.size() && expr.argByReference[i - 1];

         if( !coerce( arg, X_DECL_TYPE::ANY, "wywolanie metody" ) )
            return false;

         if( !reference )
            continue;

         if( !conversion( X_DECL_TYPE::ANY, arg.type, arg.line, "argument @ metody", arg.storeConversion ) )
            return false;
      }

      expr.type = X_DECL_TYPE::ANY;

      return true;
   }

   /* ---------------------------------------------------------------------------------------------
      Statements
      --------------------------------------------------------------------------------------------- */

   /* Dimensions are integers; the initializer converts to the declared type */
   bool localDecl( X_LOCAL_DECL &decl, const TYPE_SCOPE &scope )
   {
      const X_DECL_TYPE type = decl.declaredType.value_or( X_DECL_TYPE::ANY );

      for( X_EXPR_PTR &dimension : decl.dimensions )
      {
         if( !expression( *dimension, scope ) || !coerce( *dimension, X_DECL_TYPE::INT64, "wymiar tablicy" ) )
            return false;
      }

      if( !decl.init )
         return true;

      if( !expression( *decl.init, scope ) )
         return false;

      return coerce( *decl.init, type, "inicjalizacja " + decl.name, false, decl.declaredTypeName );
   }

   bool assignStatement( X_STMT &stmt, const TYPE_SCOPE &scope )
   {
      const X_DECL_TYPE type = lookup( scope.types, stmt.name );
      const std::string typeName = lookupTypeName( scope.names, stmt.name );

      return coerce( *stmt.expr, type, "przypisanie " + stmt.name, false, typeName );
   }

   bool returnStatement( X_STMT &stmt, X_DECL_TYPE result, const std::string &resultName )
   {
      if( stmt.expr )
         return coerce( *stmt.expr, result, "RETURN", false, resultName );

      if( result != X_DECL_TYPE::ANY )
         return fail( stmt.line, "RETURN wymaga wyniku typu " + std::string( x_declTypeName( result ) ) );

      return true;
   }

   /* A typed counter fixes the type of the limit and the step; without STEP the step has the counter type */
   bool typedFor( X_STMT &stmt, X_DECL_TYPE counter )
   {
      X_EXPR &limit = *stmt.forLimit;
      X_DECL_TYPE limitType;
      X_DECL_TYPE stepType = counter;
      int stepLine = stmt.line;

      if( !common( counter, limit.type, limit.line, "granica FOR", limitType ) )
         return false;

      if( !coerce( limit, limitType, "granica FOR" ) )
         return false;

      if( stmt.forStep )
      {
         stepType = stmt.forStep->type;
         stepLine = stmt.forStep->line;
      }

      if( !common( counter, stepType, stepLine, "krok FOR", stmt.operationType ) )
         return false;

      stmt.checkOverflow = s_integer( stmt.operationType );

      if( stmt.forStep && !coerce( *stmt.forStep, stmt.operationType, "krok FOR" ) )
         return false;

      return conversion( stmt.operationType, counter, stmt.line, "licznik FOR", stmt.storeConversion );
   }

   bool forStatement( X_STMT &stmt, const TYPE_SCOPE &scope )
   {
      const X_DECL_TYPE counter = lookup( scope.types, stmt.name );
      const X_EXPR *bounds[] = { stmt.expr.get(), stmt.forLimit.get(), stmt.forStep.get() };

      if( counter != X_DECL_TYPE::ANY && !s_numeric( counter ) )
         return fail( stmt.line, "licznik FOR wymaga liczby: " + stmt.name );

      for( const X_EXPR *value : bounds )
      {
         if( value && value->type != X_DECL_TYPE::ANY && !s_numeric( value->type ) )
            return fail( value->line, "FOR wymaga liczbowych granic i kroku" );
      }

      if( !coerce( *stmt.expr, counter, "poczatek FOR" ) )
         return false;

      if( counter == X_DECL_TYPE::ANY )
         return true;

      return typedFor( stmt, counter );
   }

   /* RECOVER USING x: the error value arrives as ANY */
   bool recoverUsing( X_STMT &stmt, const TYPE_SCOPE &scope )
   {
      const X_DECL_TYPE type = lookup( scope.types, stmt.name );

      if( stmt.name.empty() )
         return true;

      return conversion( X_DECL_TYPE::ANY, type, stmt.line, "RECOVER USING", stmt.storeConversion );
   }

   bool statement( X_STMT &stmt, const TYPE_SCOPE &scope, X_DECL_TYPE result, const std::string &resultName )
   {
      switch( stmt.kind )
      {
      case X_STMT_KIND::ASSIGN:       return assignStatement( stmt, scope );
      case X_STMT_KIND::INDEX_ASSIGN: return coerce( *stmt.expr, X_DECL_TYPE::ANY, "przypisanie elementu tablicy" );
      case X_STMT_KIND::RETURN:       return returnStatement( stmt, result, resultName );
      case X_STMT_KIND::IF:
      case X_STMT_KIND::WHILE:        return coerce( *stmt.expr, X_DECL_TYPE::LOGIC, "warunek" );
      case X_STMT_KIND::FOR:          return forStatement( stmt, scope );
      case X_STMT_KIND::SEQUENCE:     return recoverUsing( stmt, scope );
      case X_STMT_KIND::BREAK:        return !stmt.expr || coerce( *stmt.expr, X_DECL_TYPE::ANY, "BREAK" );
      default:                        return true;
      }
   }

   /* DO CASE takes LOGIC conditions; SWITCH compares each label with the selector as == */
   bool caseCondition( const X_STMT &stmt, X_CASE_BRANCH &branch, const TYPE_SCOPE &scope )
   {
      X_EXPR selector{};
      X_EXPR comparison{};

      if( !expression( *branch.condition, scope ) )
         return false;

      if( stmt.kind == X_STMT_KIND::DO_CASE )
         return coerce( *branch.condition, X_DECL_TYPE::LOGIC, "warunek CASE" );

      selector.type = stmt.expr->type;
      selector.nativeType = stmt.expr->nativeType;
      selector.line = stmt.expr->line;
      comparison.line = branch.condition->line;

      if( !binary( comparison, "==", selector, *branch.condition ) )
         return false;

      branch.operationType = comparison.operationType;
      branch.selectorConversion = selector.conversion;

      return true;
   }

   bool statements( std::vector<X_STMT> &body, const TYPE_SCOPE &scope, X_DECL_TYPE result,
                    const std::string &resultName = {} )
   {
      for( X_STMT &stmt : body )
      {
         X_EXPR *values[] = { stmt.expr.get(), stmt.target.get(), stmt.forLimit.get(), stmt.forStep.get() };

         for( X_EXPR *value : values )
         {
            if( value && !expression( *value, scope ) )
               return false;
         }

         for( X_LOCAL_DECL &decl : stmt.locals )
         {
            if( !localDecl( decl, scope ) )
               return false;
         }

         if( !statement( stmt, scope, result, resultName ) )
            return false;

         for( X_CASE_BRANCH &branch : stmt.cases )
         {
            if( branch.condition && !caseCondition( stmt, branch, scope ) )
               return false;

            if( !statements( branch.body, scope, result, resultName ) )
               return false;
         }

         if( !statements( stmt.body, scope, result, resultName ) )
            return false;

         if( !statements( stmt.elseBody, scope, result, resultName ) )
            return false;

         if( !statements( stmt.otherwiseBody, scope, result, resultName ) )
            return false;
      }

      return true;
   }

   /* ---------------------------------------------------------------------------------------------
      Program
      --------------------------------------------------------------------------------------------- */

   bool registerStructs( const X_PROGRAM &program )
   {
      for( const X_STRUCT_DECL &structure : program.structs )
      {
         if( !m_structs.emplace( structure.name, &structure ).second )
            return fail( structure.line, "powtorzona deklaracja STRUCT: " + structure.name );
      }

      /* A field of STRUCT type needs a declared STRUCT */
      for( const X_STRUCT_DECL &structure : program.structs )
      {
         for( const X_STRUCT_FIELD &field : structure.fields )
         {
            if( field.type == X_DECL_TYPE::STRUCT && !m_structs.count( field.typeName ) )
               return fail( field.line, "nieznany typ pola STRUCT: " + field.typeName );
         }
      }

      return true;
   }

   /* Own and imported classes; an imported class has a factory and its methods */
   bool registerClasses( const X_PROGRAM &program )
   {
      for( const X_CLASS_DECL &cls : program.classes )
      {
         if( !m_classes.emplace( s_key( cls.name ), &cls ).second )
            return fail( 0, "powtorzona deklaracja klasy: " + cls.name );
      }

      for( const X_CLASS_DECL &cls : program.importedClasses )
      {
         if( m_classes.emplace( s_key( cls.name ), &cls ).second )
            m_importedFactories.insert( s_key( cls.name ) );
      }

      for( const X_FUNCTION &function : program.importedMethods )
      {
         if( m_importedFactories.count( s_key( function.className ) ) )
            m_methods.emplace( s_methodKey( function ), &function );
      }

      for( const X_CLASS_DECL &cls : program.classes )
      {
         for( const std::string &parent : cls.parents )
         {
            if( !m_classes.count( s_key( parent ) ) )
               return fail( 0, "nieznana klasa bazowa " + parent + " klasy " + cls.name );
         }
      }

      return true;
   }

   /* Typed CLASSDATA: its initializer function is checked against the member type */
   void registerClassData( const X_PROGRAM &program )
   {
      for( const X_CLASS_DECL &cls : program.classes )
      {
         for( const X_CLASS_MEMBER &member : cls.members )
         {
            if( member.kind == "CLASSDATA" && member.type != X_DECL_TYPE::ANY )
               m_classData.emplace( "__XDEV_" + cls.name + "_CLASSDATA_" + member.name, &member );
         }
      }
   }

   bool registerCallbacks( const X_PROGRAM &program )
   {
      for( const X_CALLBACK_DECL &callback : program.callbacks )
      {
         if( !m_callbacks.emplace( s_key( callback.name ), &callback ).second )
            return fail( callback.line, "powtorzona deklaracja CALLBACK: " + callback.name );

         for( const std::string &name : callback.params )
         {
            if( !callback.paramTypes.count( s_key( name ) ) )
               return fail( callback.line, "CALLBACK: brak typu parametru: " + name );
         }

         for( const auto &parameter : callback.paramTypes )
         {
            if( !s_callbackValue( parameter.second ) )
               return fail( callback.line, s_callbackParameter + parameter.first );
         }

         if( !callback.isProcedure && !s_callbackResultType( callback.resultType ) )
            return fail( callback.line, s_callbackResult );
      }

      return true;
   }

   /* DLL parameters have C types; a STRUCT or CALLBACK parameter names a declared type */
   bool dllParameters( const X_DLL_FUNCTION &function )
   {
      for( const auto &parameter : function.paramTypes )
      {
         const auto named = function.paramTypeNames.find( parameter.first );
         const bool known = named != function.paramTypeNames.end();

         if( !s_abiType( parameter.second ) )
            return fail( function.line, "DLL FUNCTION/PROCEDURE: parametr wymaga typu C: " + parameter.first );

         if( parameter.second == X_DECL_TYPE::STRUCT && ( !known || !m_structs.count( named->second ) ) )
            return fail( function.line, "DLL FUNCTION: nieznany typ STRUCT parametru: " + parameter.first );

         if( parameter.second == X_DECL_TYPE::CALLBACK && ( !known || !m_callbacks.count( named->second ) ) )
            return fail( function.line, "DLL FUNCTION/PROCEDURE: nieznany typ CALLBACK parametru: " + parameter.first );
      }

      for( const std::string &name : function.params )
      {
         if( !function.paramTypes.count( s_key( name ) ) )
            return fail( function.line, "DLL FUNCTION/PROCEDURE: brak typu parametru: " + name );
      }

      return true;
   }

   bool registerDllFunctions( const X_PROGRAM &program )
   {
      for( const X_DLL_FUNCTION &function : program.dllFunctions )
      {
         if( !m_functions.emplace( s_key( function.name ), &function ).second )
            return fail( function.line, "powtorzona funkcja: " + function.name );

         m_dllFunctions.emplace( s_key( function.name ), &function );

         if( !function.isProcedure && !s_dllResultType( function.resultType ) )
            return fail( function.line, "DLL FUNCTION: wynik wymaga typu C (liczba, LOGIC, PTR lub PSZ)" );

         if( !dllParameters( function ) )
            return false;

         if( function.resultType == X_DECL_TYPE::CALLBACK && !m_callbacks.count( function.resultTypeName ) )
            return fail( function.line, "DLL FUNCTION: nieznany typ CALLBACK wyniku: " + function.resultTypeName );
      }

      return true;
   }

   /* PSZ and STRUCT by value are for DLL only; OBJECT names a known class */
   bool functionTypes( const X_FUNCTION &function )
   {
      if( function.resultType == X_DECL_TYPE::PSZ )
         return fail( function.line, "PSZ moze byc wynikiem DLL FUNCTION; funkcja xDev powinna zwracac STRING" );

      if( function.resultType == X_DECL_TYPE::STRUCT )
         return fail( function.line, "wyniki STRUCT przez wartosc sa odlozone" );

      if( function.resultType == X_DECL_TYPE::OBJECT && unknownClass( function.resultTypeName ) )
         return fail( function.line, "nieznany typ OBJECT wyniku: " + function.resultTypeName );

      for( const auto &parameter : function.paramTypes )
      {
         const auto named = function.paramTypeNames.find( parameter.first );

         if( parameter.second == X_DECL_TYPE::PSZ )
            return fail( function.line, "PSZ jest typem ABI DLL; parametr funkcji xDev powinien miec STRING" );

         if( parameter.second == X_DECL_TYPE::STRUCT )
            return fail( function.line, "parametry STRUCT funkcji xDev przez wartosc sa odlozone" );

         if( parameter.second != X_DECL_TYPE::OBJECT || named == function.paramTypeNames.end() )
            continue;

         if( unknownClass( named->second ) )
            return fail( function.line, "nieznany typ OBJECT parametru: " + parameter.first );
      }

      return true;
   }

   bool registerFunctions( const X_PROGRAM &program )
   {
      for( const X_FUNCTION &function : program.functions )
      {
         if( !functionTypes( function ) )
            return false;

         if( !m_functions.emplace( s_key( function.name ), &function ).second )
            return fail( function.line, "powtorzona funkcja: " + function.name );

         if( !function.className.empty() && !function.methodName.empty() )
            m_methods[s_methodKey( function )] = &function;

         if( function.isProcedure && function.resultType && *function.resultType != X_DECL_TYPE::ANY )
            return fail( function.line, "PROCEDURE zwraca NIL; wynik AS wymaga ANY" );
      }

      return true;
   }

   /* Module STATIC when names were not resolved; otherwise check() copies the resolved scope */
   bool registerStatics( const X_PROGRAM &program )
   {
      if( m_resolved )
         return true;

      for( const X_STMT &stmt : program.statements )
      {
         if( stmt.kind != X_STMT_KIND::STATIC )
            continue;

         for( const X_LOCAL_DECL &decl : stmt.locals )
         {
            const X_DECL_TYPE type = decl.declaredType.value_or( X_DECL_TYPE::ANY );

            if( !m_module.emplace( s_key( decl.name ), type ).second )
               return fail( decl.line, "powtorzona deklaracja STATIC: " + decl.name );

            if( s_namedType( type ) )
               m_moduleTypeNames[s_key( decl.name )] = decl.declaredTypeName;
         }
      }

      return true;
   }

   /* Resolved names, then parameters and Self */
   void functionScope( const X_FUNCTION &function, TYPE_SCOPE &scope )
   {
      if( m_resolved )
      {
         for( const auto &entry : function.scope.symbols )
         {
            scope.types[entry.first] = entry.second.type;
            scope.names[entry.first] = entry.second.typeName;
         }
      }

      for( const std::string &param : function.params )
      {
         const std::string key = s_key( param );
         const auto declared = function.paramTypes.find( key );
         const auto named = function.paramTypeNames.find( key );
         X_DECL_TYPE type = X_DECL_TYPE::ANY;

         if( declared != function.paramTypes.end() )
            type = declared->second;

         scope.types.emplace( key, type );

         if( named != function.paramTypeNames.end() )
            scope.names[key] = named->second;
      }

      if( !function.className.empty() && function.methodKind != "CLASSMETHOD" )
      {
         scope.types["SELF"] = X_DECL_TYPE::OBJECT;
         scope.names["SELF"] = function.className;
      }
   }

   /* DATA initializers of a class are typed like LOCAL initializers */
   bool dataInit( X_LOCAL_DECL &decl, const TYPE_SCOPE &scope )
   {
      const X_DECL_TYPE type = decl.declaredType.value_or( X_DECL_TYPE::ANY );

      if( !expression( *decl.init, scope ) )
         return false;

      return coerce( *decl.init, type, "inicjalizacja pola " + decl.name, false, decl.declaredTypeName );
   }

   bool checkFunction( X_FUNCTION &function )
   {
      const X_DECL_TYPE result = function.resultType.value_or( X_DECL_TYPE::ANY );
      const auto shared = m_classData.find( function.name );
      TYPE_SCOPE scope;

      functionScope( function, scope );

      if( !collect( function.body, scope ) )
         return false;

      if( !statements( function.body, scope, result, function.resultTypeName ) )
         return false;

      for( X_LOCAL_DECL &decl : function.dataInitializers )
      {
         if( decl.init && !dataInit( decl, scope ) )
            return false;
      }

      /* CLASSDATA initializer: the value of its first statement */
      if( shared == m_classData.end() || !function.body.front().expr )
         return true;

      const X_CLASS_MEMBER &member = *shared->second;
      X_EXPR &value = *function.body.front().expr;

      return coerce( value, member.type, "inicjalizacja pola " + member.name, false, member.typeName );
   }

public:
   X_TYPECHECKER( std::string &error, const X_LOCATION &location ) : m_error( error ), m_location( location )
   {
   }

   bool check( X_PROGRAM &program )
   {
      TYPE_SCOPE module;

      m_resolved = program.symbolsResolved;

      if( m_resolved )
      {
         for( const auto &entry : program.scope.symbols )
         {
            m_module[entry.first] = entry.second.type;
            m_moduleTypeNames[entry.first] = entry.second.typeName;
         }
      }

      if( !registerStructs( program ) || !registerClasses( program ) )
         return false;

      registerClassData( program );

      if( !registerCallbacks( program ) || !registerDllFunctions( program ) )
         return false;

      if( !registerFunctions( program ) || !registerStatics( program ) )
         return false;

      if( !collect( program.statements, module, true ) )
         return false;

      if( !statements( program.statements, module, X_DECL_TYPE::ANY ) )
         return false;

      for( X_FUNCTION &function : program.functions )
      {
         if( !checkFunction( function ) )
            return false;
      }

      return true;
   }
};

bool x_checkTypes( X_PROGRAM &program, std::string &error, const X_LOCATION &location )
{
   X_TYPECHECKER checker( error, location );

   return checker.check( program );
}
