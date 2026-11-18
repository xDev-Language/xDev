/*
 * Copyright (c) 2024-2026 xDev-Language
 *
 * Library: xDev Frontend
 * File: ast.hpp
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#ifndef AST_HPP
#define AST_HPP

#include <cstdint>
#include <functional>
#include <memory>
#include <map>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

struct X_EXPR;
struct X_LOCAL_DECL;
struct X_CASE_BRANCH;
struct X_STMT;
struct X_FUNCTION;
struct X_PROGRAM;

// Turns a preprocessed line number into "file: linia n" for error messages
using X_LOCATION = std::function<std::string( int )>;

// Owner of a subexpression
using X_EXPR_PTR = std::unique_ptr<X_EXPR>;

enum class X_DECL_TYPE
{
   ANY,
   INT8,
   INT16,
   INT32,
   INT64,
   UINT8,
   UINT16,
   UINT32,
   UINT64,
   FLOAT32,
   FLOAT64,
   LOGIC,
   STRING,
   PSZ,
   PTR,
   CALLBACK,
   STRUCT,
   OBJECT,
   NIL,
   ARRAY,
   CODEBLOCK,
   DATE,
   HASH
};

struct X_TYPE_CONVERSION
{
   X_DECL_TYPE target;
   bool explicitCast = false;
   bool checkType = false;
   bool checkRange = false;
};

enum class X_BINDING_KIND
{
   PARAMETER,
   LOCAL,
   STATIC,
   FIELD,
   SELF,
   BLOCK_PARAMETER
};

struct X_BINDING
{
   std::string name;
   X_BINDING_KIND kind = X_BINDING_KIND::LOCAL;
   X_DECL_TYPE type = X_DECL_TYPE::ANY;
   std::string typeName;
   int line = 0;
   const X_LOCAL_DECL *declaration = nullptr;
};

struct X_SCOPE
{
   std::map<std::string, X_BINDING> symbols;
};

enum class X_EXPR_KIND
{
   NIL,
   NUMBER,
   STRING,
   LOGICAL,
   NAME,
   FIELD,
   ALIAS,
   GROUP,
   CALL,
   SEND,
   ARRAY,
   INDEX,
   UNARY,
   BINARY,
   UPDATE,
   ASSIGN,
   BLOCK,
   IIF,
   CAST,
   FUNCTION_ADDRESS
};

enum class X_UPDATE_OP
{
   INC,
   DEC
};

enum class X_UPDATE_MODE
{
   PREFIX,
   POSTFIX
};

enum class X_ASSIGN_OP
{
   SET,
   ADD,
   SUB,
   MUL,
   DIV,
   MOD,
   POW
};

struct X_EXPR
{
   X_SCOPE scope;
   std::vector<std::string> captures;
   X_EXPR_KIND kind;
   X_DECL_TYPE type = X_DECL_TYPE::ANY;
   bool nativeType = false;
   X_DECL_TYPE operationType = X_DECL_TYPE::ANY;
   std::optional<X_TYPE_CONVERSION> conversion;
   std::string conversionClass;
   std::optional<X_TYPE_CONVERSION> storeConversion;
   bool checkOverflow = false;
   int line = 0;
   double number = 0.0;
   // exact INTEGER payload, never routed through double
   int64_t integer = 0;
   // NUMBER: preserve the lexer's distinction between 7 and 7.0
   bool numberIsInteger = false;
   bool logical = false;
   bool parenthesized = false;
   bool structFieldAccess = false;
   bool dotMemberAccess = false;
   bool structArrayIndex = false;
   size_t structFieldIndex = 0;
   uint64_t fixedArrayCount = 0;
   X_DECL_TYPE arrayElementType = X_DECL_TYPE::ANY;
   std::string typeName;
   std::string containerTypeName;
   std::string arrayElementTypeName;
   std::string text;
   std::vector<X_EXPR_PTR> args;
   // @name passes the caller's slot
   std::vector<bool> argByReference;
   // CALL written by the compiler: a C function of the runtime, called by its exact name
   bool internalCall = false;
   // CALL Hash written as { xKey => xValue, ... } or { => }: a STATIC constant when keys and values are constant
   bool hashLiteral = false;
   bool sendToSuper = false;
   bool sendHasParentheses = false;
   // BLOCK only: parameter names {|a,b| ...}; args is the body, the last expression supplies the value
   std::vector<std::string> blockParams;
   X_UPDATE_OP updateOp = X_UPDATE_OP::INC;
   X_UPDATE_MODE updateMode = X_UPDATE_MODE::PREFIX;
   X_ASSIGN_OP assignOp = X_ASSIGN_OP::SET;
};

struct X_LOCAL_DECL
{
   std::string name;
   int line = 0;
   std::optional<X_DECL_TYPE> declaredType;
   std::string declaredTypeName;
   uint64_t fixedArrayCount = 0;
   std::vector<X_EXPR_PTR> dimensions;
   X_EXPR_PTR init;
};

struct X_CASE_BRANCH
{
   X_DECL_TYPE operationType = X_DECL_TYPE::ANY;
   std::optional<X_TYPE_CONVERSION> selectorConversion;
   X_EXPR_PTR condition;
   std::vector<X_STMT> body;
};

enum class X_STMT_KIND
{
   ASSIGN,
   INDEX_ASSIGN,
   LOCAL,
   FIELD,
   STATIC,
   CALL,
   RETURN,
   IF,
   WHILE,
   FOR,
   DO_CASE,
   EXIT,
   LOOP,
   SWITCH,
   REQUEST,
   SEQUENCE,
   BREAK
};

struct X_STMT
{
   X_STMT_KIND kind;
   X_DECL_TYPE operationType = X_DECL_TYPE::ANY;
   std::optional<X_TYPE_CONVERSION> storeConversion;
   bool checkOverflow = false;
   int line = 0;
   std::string name;
   // REQUEST: linker symbols, never calls
   std::vector<std::string> requests;
   // ASSIGN/INDEX_ASSIGN/CALL/RETURN: value; IF/WHILE: condition; FOR: initial value
   X_EXPR_PTR expr;
   // INDEX_ASSIGN: indexed assignment target
   X_EXPR_PTR target;
   // FOR: TO, evaluated on each iteration check
   X_EXPR_PTR forLimit;
   // FOR: STEP; nullptr means the default value of 1
   X_EXPR_PTR forStep;
   // IF: THEN block; WHILE/FOR: loop body
   std::vector<X_STMT> body;
   // IF: ELSE block (empty when absent)
   std::vector<X_STMT> elseBody;
   // DO_CASE: conditions; SWITCH: literal labels (expr = selector)
   std::vector<X_CASE_BRANCH> cases;
   // DO_CASE / SWITCH: optional OTHERWISE
   std::vector<X_STMT> otherwiseBody;
   bool hasOtherwise = false;
   // LOCAL/STATIC: declarations with optional initializers
   std::vector<X_LOCAL_DECL> locals;
};

enum class X_PROC_ROLE
{
   NORMAL,
   INIT,
   EXIT
};

struct X_FUNCTION
{
   X_SCOPE scope;
   std::string name;
   int line = 0;
   std::string className;
   std::string methodName;
   std::string methodKind;
   std::string factoryClass;
   bool isDataInitializer = false;
   bool isClassDataInitializer = false;
   std::vector<X_LOCAL_DECL> dataInitializers;
   bool isProcedure = false;
   bool isDll = false;
   bool isStatic = false;
   X_PROC_ROLE role = X_PROC_ROLE::NORMAL;
   std::vector<std::string> params;
   std::unordered_map<std::string, X_DECL_TYPE> paramTypes;
   std::unordered_map<std::string, std::string> paramTypeNames;
   std::optional<X_DECL_TYPE> resultType;
   std::string resultTypeName;
   std::vector<X_STMT> body;
};

struct X_DLL_FUNCTION : X_FUNCTION
{
   std::string library;
   std::string symbol;
};

struct X_CALLBACK_DECL
{
   std::string name;
   int line = 0;
   bool isProcedure = false;
   std::vector<std::string> params;
   std::unordered_map<std::string, X_DECL_TYPE> paramTypes;
   std::unordered_map<std::string, std::string> paramTypeNames;
   std::optional<X_DECL_TYPE> resultType;
   std::string resultTypeName;
};

struct X_STRUCT_FIELD
{
   std::string name;
   int line = 0;
   X_DECL_TYPE type = X_DECL_TYPE::ANY;
   std::string typeName;
   uint64_t arrayCount = 0;
};

struct X_STRUCT_DECL
{
   std::string name;
   int line = 0;
   std::vector<X_STRUCT_FIELD> fields;
};

struct X_CLASS_MEMBER
{
   std::string name;
   std::string kind;
   std::string delegateObject;
   std::string delegateMessage;
   int operatorId = -1;
   std::string operatorMessage;
   std::string operatorSymbol;
   uint32_t scope = 1;
   std::vector<std::string> params;
   X_DECL_TYPE type = X_DECL_TYPE::ANY;
   std::string typeName;
};

struct X_CLASS_DECL
{
   std::string name;
   int line = 0;
   std::vector<std::string> parents;
   std::vector<X_CLASS_MEMBER> members;
};

struct X_PROGRAM
{
   X_SCOPE scope;
   bool symbolsResolved = false;
   bool hasTypeAnnotations = false;
   std::vector<X_STRUCT_DECL> structs;
   std::vector<X_CALLBACK_DECL> callbacks;
   std::vector<X_CLASS_DECL> classes;
   std::vector<X_CLASS_DECL> importedClasses;
   std::vector<X_FUNCTION> importedMethods;
   std::vector<X_STMT> statements;
   std::vector<X_FUNCTION> functions;
   std::vector<X_DLL_FUNCTION> dllFunctions;
};

const char *x_declTypeName( X_DECL_TYPE type );
void x_astDump( const X_PROGRAM &program );

#endif /* AST_HPP */
