/*
 * Copyright (c) 2024-2026 xDev-Language
 *
 * Library...: xDev Frontend
 * File......: parser.hpp
 *
 */

#ifndef PARSER_HPP
#define PARSER_HPP

#include "ast.hpp"
#include "lexer.h"

#include <memory>
#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>

using X_FIELD_MAP = std::unordered_map<std::string, std::string>;
using X_NAME_SET = std::unordered_set<std::string>;

/* An error of the parser: line and column of the token in the preprocessed text */
struct X_DIAGNOSTIC
{
   int line;
   int column;
   std::string message;
};

class X_PARSER
{
public:
   explicit X_PARSER( const X_TOKLIST &tokens );
   bool parse( X_PROGRAM &program, std::string &error );
   void addClassType( const std::string &name );
   const std::vector<X_DIAGNOSTIC> &diagnostics() const;

private:
   /* FOR EACH: the hidden locals of the function and the loop variables open now */
   struct X_ENUM_VAR
   {
      std::string name;
      std::string base;
      std::string pos;
      std::string turn;
      std::string count;
      bool descend = false;
      std::string target; /* IN @target: the variable that gets the base back at the end of the loop */
      std::string linked; /* .F. after enumBase( x ): the variable keeps the old base */
   };

   /* FOR EACH: one loop variable and its expression */
   struct X_EACH_PAIR
   {
      std::string var;
      X_EXPR_PTR expr;
      bool byReference = false;
      X_ENUM_VAR hidden;
   };

   /* FOR EACH: the loop being built */
   struct X_EACH
   {
      std::vector<X_EACH_PAIR> pairs;
      std::string tag;
      std::string turn;
      std::string count;
      bool descend = false;
      int line = 0;
   };

   /* CREATE CLASS: the class being built */
   struct X_CLASS_BUILD
   {
      X_CLASS_DECL cls;
      X_FUNCTION initializer;
      uint32_t scope = 0;
      std::unordered_set<std::string> messages;
      std::unordered_set<std::string> publicMessages;
   };

   const X_TOKLIST &m_tokens;
   int m_pos = 0;
   int m_loopDepth = 0;
   int m_switchDepth = 0;
   bool m_inProcedure = false;
   bool m_inMethod = false;
   bool m_hasTypeAnnotations = false;
   bool m_announced = false; /* a .prg file has one module name */
   std::string m_error;
   std::vector<X_DIAGNOSTIC> m_diagnostics;
   std::unordered_map<std::string, int64_t> m_enums;
   std::unordered_set<std::string> m_tags;
   std::unordered_set<std::string> m_names;
   std::unordered_set<std::string> m_structTypes;
   std::unordered_set<std::string> m_classTypes;
   std::unordered_set<std::string> m_callbackTypes;
   std::vector<std::string> m_enumLocals;
   std::vector<X_ENUM_VAR> m_enumVars;
   int m_enumCount = 0;

   /* ---------------------------------------------------------------------------------------------
      Parser state
      --------------------------------------------------------------------------------------------- */

   bool is( X_TOKTYPE type ) const;
   X_TOKTYPE peek( int offset ) const;
   const char *value( int offset = 0 ) const;
   bool isMemberName() const;
   bool match( X_TOKTYPE type );
   bool at( const char *value ) const;
   bool atBinaryOperatorToken() const;
   void fail( const char *message );
   bool recover( int start );
   bool atRoutineStart() const;
   void skipLines();
   bool expect( const char *value );
   bool namedType( const std::string &name, X_DECL_TYPE &type ) const;
   bool parseType( X_DECL_TYPE &type, std::string *typeName = nullptr );
   bool acceptType();

   /* ---------------------------------------------------------------------------------------------
      Expressions
      --------------------------------------------------------------------------------------------- */

   X_EXPR_PTR makeUpdate( X_UPDATE_OP op, X_UPDATE_MODE mode, X_EXPR_PTR target );
   X_EXPR_PTR makeAssignment( X_ASSIGN_OP op, X_EXPR_PTR target, X_EXPR_PTR value );
   X_EXPR_PTR parseFunctionAddress();
   X_EXPR_PTR parseNumber();
   X_EXPR_PTR parseGroup( int line );
   X_EXPR_PTR parseHash( X_EXPR_PTR key, int line );
   X_EXPR_PTR parseArray( X_EXPR_PTR first );
   X_EXPR_PTR parseBrace( int line );
   X_EXPR_PTR parseIif();
   X_EXPR_PTR parseName();
   X_EXPR_PTR parsePrimary();
   bool parseBlockParams( X_EXPR &block );
   bool parseExpressionList( std::vector<X_EXPR_PTR> &list );
   X_EXPR_PTR parseCodeBlock();
   bool atFunctionAddress() const;
   bool atDot() const;
   bool atPostfix() const;
   X_EXPR_PTR parseDotField( X_EXPR_PTR expr );
   X_EXPR_PTR parseAliased( X_EXPR_PTR expr );
   X_EXPR_PTR parseReference();
   bool parseCast( X_EXPR &call, bool byReference );
   bool parseArguments( X_EXPR &call );
   X_EXPR_PTR parseCall( X_EXPR_PTR expr );
   X_EXPR_PTR parseIndex( X_EXPR_PTR expr );
   X_EXPR_PTR parseEnumMessage( const X_EXPR &receiver, X_EXPR_PTR property );
   bool parseMessageArguments( X_EXPR &send );
   X_EXPR_PTR parseSend( X_EXPR_PTR expr );
   X_EXPR_PTR parsePostfix();
   X_EXPR_PTR parseUnary();
   X_EXPR_PTR parseBinary( int minPrecedence );
   bool matchAssignOperator( X_ASSIGN_OP &op );
   X_EXPR_PTR parseAssignment();
   X_EXPR_PTR parseExpression();

   /* ---------------------------------------------------------------------------------------------
      Statements
      --------------------------------------------------------------------------------------------- */

   bool atAny( const std::vector<std::string> &words ) const;
   bool expectLineEnd( const char *message );
   bool parseBlock( std::vector<X_STMT> &out, const std::vector<std::string> &stopWords );
   bool parseIf( X_STMT &statement );
   bool parseWhile( X_STMT &statement );
   bool parseOtherwise( X_STMT &statement, const std::vector<std::string> &stopWords );
   bool parseDoCase( X_STMT &statement );
   bool switchLabel( const X_STMT &statement, X_EXPR &label );
   bool parseSwitchBody( X_STMT &statement );
   bool parseSwitch( X_STMT &statement );

   /* ---------------------------------------------------------------------------------------------
      FOR EACH
      --------------------------------------------------------------------------------------------- */

   std::string eachHidden( const X_EACH &each, const std::string &suffix );
   bool parseEachSource( X_EACH_PAIR &pair, int line );
   bool parseEachHead( X_EACH &each );
   void eachNames( X_EACH &each );
   X_STMT eachAssign( const std::string &target, X_EXPR_PTR value, int line );
   std::vector<X_STMT> eachPutBack( const X_EACH &each ) const;
   void eachPrologue( X_EACH &each, X_STMT &statement );
   X_STMT eachLoop( X_EACH &each, std::vector<X_STMT> &body );
   void eachEpilogue( X_EACH &each, X_STMT &statement );
   bool parseForEach( X_STMT &statement );
   const X_ENUM_VAR *findEnumVar( const std::string &name ) const;
   X_EXPR_PTR enumSetIndex( const X_ENUM_VAR &var, const X_EXPR &receiver, X_EXPR_PTR argument ) const;
   X_EXPR_PTR enumLinkTarget( const X_ENUM_VAR &var, const X_EXPR &receiver, X_EXPR_PTR expr );
   X_EXPR_PTR enumSetBase( const X_ENUM_VAR &var, const X_EXPR &receiver, X_EXPR_PTR argument );
   X_EXPR_PTR enumProperty( const X_ENUM_VAR &var, const X_EXPR &receiver, const std::string &key ) const;
   X_EXPR_PTR enumMessage( const X_EXPR &receiver, const char *message, X_EXPR_PTR argument = nullptr );

   /* ---------------------------------------------------------------------------------------------
      FOR, LOCAL, STATIC, FIELD
      --------------------------------------------------------------------------------------------- */

   bool parseFor( X_STMT &statement );
   bool parseDeclaration( X_LOCAL_DECL &decl, const char *missing );
   bool parseLocal( X_STMT &statement );
   bool parseArrayDimensions( X_LOCAL_DECL &decl );
   bool constantArgs( const X_EXPR &expr ) const;
   bool isConstantExpr( const X_EXPR &expr ) const;
   bool parseStaticInit( X_LOCAL_DECL &decl );
   bool parseStatic( X_STMT &statement );
   bool parseRequest( X_STMT &statement );
   bool parseAnnounce( X_PROGRAM &program );
   bool parseFieldDeclaration( X_STMT &statement );

   /* ---------------------------------------------------------------------------------------------
      FIELD binding
      --------------------------------------------------------------------------------------------- */

   bool fieldDeclarations( const std::vector<X_STMT> &body, X_FIELD_MAP &fields, X_NAME_SET &declared );
   bool fieldCollision( const X_NAME_SET &declared, const X_NAME_SET &names );
   bool bindExpr( X_EXPR &expr, const X_FIELD_MAP &fields, const X_NAME_SET &shadow, bool areaContext );
   bool bindBody( std::vector<X_STMT> &body, const X_FIELD_MAP &fields, const X_NAME_SET &shadow );
   bool bindFunctionFields( X_FUNCTION &function, const X_FIELD_MAP &moduleFields, const X_NAME_SET &statics );
   bool bindFields( X_PROGRAM &program );

   /* ---------------------------------------------------------------------------------------------
      Statement
      --------------------------------------------------------------------------------------------- */

   bool parseRecover( X_STMT &statement );
   bool parseSequence( X_STMT &statement );
   bool parseBreak( X_STMT &statement );
   bool parseExitLoop( X_STMT &statement );
   bool parseReturn( X_STMT &statement );
   bool parseDo( X_STMT &statement );
   bool parseExpressionStatement( X_STMT &statement );
   const char *misplacedWord() const;
   bool parseStatement( X_STMT &statement );

   /* ---------------------------------------------------------------------------------------------
      Functions
      --------------------------------------------------------------------------------------------- */

   bool nameTaken( const std::string &upper ) const;
   bool parseResult( std::optional<X_DECL_TYPE> &result, std::string &typeName );
   bool atCallback() const;
   bool parseCallbackParams( X_CALLBACK_DECL &callback );
   bool parseCallback( X_PROGRAM &program );
   bool atDllFunction() const;
   bool atEllipsis() const;
   bool parseDllParams( X_DLL_FUNCTION &function );
   bool parseDllLibrary( X_DLL_FUNCTION &function );
   bool parseDllFunction( X_DLL_FUNCTION &function );
   bool atMethodWord() const;
   bool atFunctionEnd() const;
   bool parseParams( X_FUNCTION &function );
   bool methodClass( X_FUNCTION &function );
   const char *lateDeclaration() const;
   bool parseBodyStatement( X_FUNCTION &function, bool &sawExecutable );
   void eachLocals( X_FUNCTION &function );
   bool parseFunctionBody( X_FUNCTION &function, bool method );
   bool parseFunction( X_FUNCTION &function, bool isStatic );

   /* ---------------------------------------------------------------------------------------------
      Classes
      --------------------------------------------------------------------------------------------- */

   bool parseClassHead( X_CLASS_DECL &cls, const X_PROGRAM &program );
   bool parseMemberName( X_CLASS_BUILD &build, X_CLASS_MEMBER &member );
   bool parseData( X_CLASS_BUILD &build, X_CLASS_MEMBER &member, X_PROGRAM &program );
   bool parseMemberParams( X_CLASS_MEMBER &member );
   bool parseOperator( X_CLASS_BUILD &build, X_CLASS_MEMBER &member );
   bool parseDelegate( X_CLASS_MEMBER &member );
   bool parseModifiers( X_CLASS_MEMBER &member );
   bool parseMember( X_CLASS_BUILD &build, X_CLASS_MEMBER &member, X_PROGRAM &program );
   bool parseClassLine( X_CLASS_BUILD &build, X_PROGRAM &program );
   bool parseClass( X_PROGRAM &program );

   /* ---------------------------------------------------------------------------------------------
      ENUM and STRUCT
      --------------------------------------------------------------------------------------------- */

   bool bindName( const std::string &name );
   bool enumBinary( const std::string &op, int64_t left, int64_t right, int64_t &value );
   bool enumValue( const X_EXPR &expr, int64_t &value );
   bool parseEnumConstant( int64_t &number, bool first );
   bool parseEnum();
   bool parseFieldCount( X_STRUCT_FIELD &field );
   bool parseStructField( const X_STRUCT_DECL &structure, X_STRUCT_FIELD &field );
   bool parseStruct( X_PROGRAM &program );

   /* ---------------------------------------------------------------------------------------------
      Program
      --------------------------------------------------------------------------------------------- */

   bool parseDll( X_PROGRAM &program );
   bool parseModuleDeclaration( X_PROGRAM &program );
   bool parseModuleFunction( X_PROGRAM &program, bool isStatic, X_PROC_ROLE role );
   bool parseModuleStatement( X_PROGRAM &program );
   bool parseModuleItem( X_PROGRAM &program );
   bool parseTopLevel( X_PROGRAM &program );
   void onErrorHandler( X_PROGRAM &program, const X_CLASS_DECL &cls, const X_CLASS_MEMBER &member );
   void onErrorHandlers( X_PROGRAM &program );
};

#endif /* PARSER_HPP */
