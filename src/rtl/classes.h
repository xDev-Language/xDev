/*
 * Copyright (c) 2024-2026 xDev-Language
 *
 * Library: xDev Runtime
 * File: classes.h
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#ifndef CLASSES_H
#define CLASSES_H

#include "internal.h"

#ifdef __cplusplus
extern "C"
{
#endif

/* ---------------------------------------------------------------------------------------------
   Types
   --------------------------------------------------------------------------------------------- */

typedef struct _X_SYMBOL X_SYMBOL;
typedef struct _X_METHOD X_METHOD;
typedef struct _X_RESOLVED_METHOD X_RESOLVED_METHOD;
typedef struct _X_CLASS X_CLASS;
typedef struct _X_METHOD_CONTEXT X_METHOD_CONTEXT;

typedef uint32_t X_CLASS_HANDLE;
typedef uint32_t X_METHOD_HANDLE;

typedef X_FUNCPTR X_METHODPTR;

/* Which messages a list of messages holds */
typedef enum
{
   X_MSGLIST_ALL,
   X_MSGLIST_CLASS,
   X_MSGLIST_PURE
} X_MSGLIST_TYPE;

/* The scope of a message */
typedef enum
{
   X_OO_CLSTP_EXPORTED = 1,
   X_OO_CLSTP_PROTECTED = 2,
   X_OO_CLSTP_HIDDEN = 4,
   X_OO_CLSTP_CTOR = 8,
   X_OO_CLSTP_READONLY = 16,
   X_OO_CLSTP_SHARED = 32,
   X_OO_CLSTP_CLASS = 64,
   X_OO_CLSTP_SUPER = 128,
   X_OO_CLSTP_PERSIST = 256,
   X_OO_CLSTP_NONVIRTUAL = 512,
   X_OO_CLSTP_OVERLOADED = 1024,
   X_OO_CLSTP_SYNC = 2048,
   /* EXPORTED, PROTECTED, HIDDEN, READONLY, SHARED and NONVIRTUAL */
   X_OO_CLSTP_SUPPORTED = 1 | 2 | 4 | 16 | 32 | 512
} X_OO_CLASS_SCOPE;

/* The kind of a message */
typedef enum
{
   X_OO_MSG_METHOD,
   X_OO_MSG_DATA,
   X_OO_MSG_CLASSDATA,
   X_OO_MSG_INLINE,
   X_OO_MSG_VIRTUAL,
   X_OO_MSG_SUPER,
   X_OO_MSG_ONERROR,
   X_OO_MSG_CLSMTHD,
   X_OO_MSG_ASSIGN,
   X_OO_MSG_ACCESS,
   X_OO_MSG_CLSASSIGN,
   X_OO_MSG_CLSACCESS,
   X_OO_MSG_REALCLASS,
   X_OO_MSG_DESTRUCTOR,
   X_OO_MSG_INITIALIZED,
   X_OO_MSG_PERFORM,
   X_OO_MSG_DELEGATE,
   X_OO_PROPERTY = 32,
   X_OO_MSG_PROPERTY = X_OO_PROPERTY + X_OO_MSG_DATA,
   X_OO_MSG_CLASSPROPERTY = X_OO_PROPERTY + X_OO_MSG_CLASSDATA
} X_OO_MESSAGE_TYPE;

/* The type of an instance variable after AS */
typedef enum
{
   X_OO_VALUE_ANY,
   X_OO_VALUE_INT8,
   X_OO_VALUE_INT16,
   X_OO_VALUE_INT32,
   X_OO_VALUE_INT64,
   X_OO_VALUE_UINT8,
   X_OO_VALUE_UINT16,
   X_OO_VALUE_UINT32,
   X_OO_VALUE_UINT64,
   X_OO_VALUE_FLOAT32,
   X_OO_VALUE_FLOAT64,
   X_OO_VALUE_LOGIC,
   X_OO_VALUE_STRING,
   X_OO_VALUE_PTR,
   X_OO_VALUE_ARRAY,
   X_OO_VALUE_CODEBLOCK,
   X_OO_VALUE_OBJECT,
   X_OO_VALUE_DATE,
   X_OO_VALUE_HASH
} X_OO_VALUE_TYPE;

/* The cache of one instance variable access in compiled code */
typedef struct
{
   X_CLASS_HANDLE objectClass;
   X_CLASS_HANDLE senderClass;
   X_OO_VALUE_TYPE valueType;
   size_t dataIndex;
} X_DATA_CACHE;

/* The fields of the arrays of class information */
typedef enum
{
   X_OO_DATA_SYMBOL = 1,
   X_OO_DATA_VALUE,
   X_OO_DATA_TYPE,
   X_OO_DATA_SCOPE
} X_OO_DATA_FIELD;

typedef enum
{
   X_OO_CLSD_SYMBOL = 1,
   X_OO_CLSD_VALUE,
   X_OO_CLSD_TYPE,
   X_OO_CLSD_SCOPE
} X_OO_CLASSDATA_FIELD;

typedef enum
{
   X_OO_MTHD_SYMBOL = 1,
   X_OO_MTHD_PFUNCTION,
   X_OO_MTHD_SCOPE
} X_OO_METHOD_FIELD;

typedef enum
{
   X_OO_CLSM_SYMBOL = 1,
   X_OO_CLSM_PFUNCTION,
   X_OO_CLSM_SCOPE
} X_OO_CLASSMETHOD_FIELD;

typedef enum
{
   X_OO_DELEG_SYMBOL = 1,
   X_OO_DELEG_MESSAGE,
   X_OO_DELEG_OBJECT,
   X_OO_DELEG_SCOPE
} X_OO_DELEGATE_FIELD;

/* The operators a class can overload */
typedef enum
{
   X_OO_OP_PLUS,
   X_OO_OP_MINUS,
   X_OO_OP_MULT,
   X_OO_OP_DIVIDE,
   X_OO_OP_MOD,
   X_OO_OP_POWER,
   X_OO_OP_INC,
   X_OO_OP_DEC,
   X_OO_OP_EQUAL,
   X_OO_OP_EXACTEQUAL,
   X_OO_OP_NOTEQUAL,
   X_OO_OP_LESS,
   X_OO_OP_LESSEQUAL,
   X_OO_OP_GREATER,
   X_OO_OP_GREATEREQUAL,
   X_OO_OP_ASSIGN,
   X_OO_OP_INSTRING,
   X_OO_OP_INCLUDE,
   X_OO_OP_NOT,
   X_OO_OP_AND,
   X_OO_OP_OR,
   X_OO_OP_ARRAYINDEX,
   X_OO_OP_ENUMINDEX,
   X_OO_OP_ENUMBASE,
   X_OO_OP_ENUMVALUE,
   X_OO_OP_ENUMSTART,
   X_OO_OP_ENUMSKIP,
   X_OO_OP_ENUMSTOP,
   X_OO_OP_ENUMISFIRST,
   X_OO_OP_ENUMISLAST,
   X_OO_OP_MAX = X_OO_OP_ENUMISLAST
} X_OO_OPERATOR;

/* ---------------------------------------------------------------------------------------------
   Structures
   --------------------------------------------------------------------------------------------- */

/* A message name, stored once */
struct _X_SYMBOL
{
   char *name;
   size_t length;
   uint32_t hash;
   X_SYMBOL *next;
};

/* One message of a class */
struct _X_METHOD
{
   X_METHOD_HANDLE handle;
   X_SYMBOL *message;
   X_CLASS_HANDLE definingClass;
   X_METHODPTR function;
   X_OO_MESSAGE_TYPE kind;
   uint32_t scope;
   size_t dataIndex;
   size_t ownOffset;
   X_SYMBOL *delegateObject;
   X_SYMBOL *delegateMessage;
   X_ITEM inlineBlock;
   X_OO_VALUE_TYPE valueType;
};

/* A message found for a class, with the place of its data in the object */
struct _X_RESOLVED_METHOD
{
   X_METHOD *method;
   size_t dataIndex;
};

/* A class */
struct _X_CLASS
{
   X_CLASS_HANDLE handle;
   X_SYMBOL *name;
   X_SYMBOL *factoryName;
   X_CLASS_HANDLE *parents;
   size_t parentCount;
   size_t parentCapacity;
   X_CLASS_HANDLE *friends;
   size_t friendCount;
   size_t friendCapacity;
   X_CLASS_HANDLE *lineage;
   size_t lineageCount;
   size_t lineageCapacity;
   X_METHOD *methods;
   size_t methodCount;
   size_t methodCapacity;
   X_METHOD *destructor;
   X_RESOLVED_METHOD *methodTable;
   size_t methodTableCount;
   size_t methodTableCapacity;
   size_t ownDataCount;
   size_t instanceDataCount;
   X_ITEM *ownClassData;
   size_t ownClassDataCount;
   size_t ownClassDataCapacity;
   X_ITEM *classData;
   size_t classDataCount;
   X_METHOD *ownOperators[X_OO_OP_MAX + 1];
   const X_RESOLVED_METHOD *operators[X_OO_OP_MAX + 1];
   int hasDestructor;
   int hasOnError;
   int associated;
};

/* The method that runs now; the frames make a stack through previous */
struct _X_METHOD_CONTEXT
{
   X_ITEM *self;
   X_CLASS_HANDLE objectClass;
   X_CLASS_HANDLE lookupClass;
   X_CLASS_HANDLE definingClass;
   X_METHOD_HANDLE method;
   const char *attemptedMessage;
   X_METHOD_CONTEXT *previous;
};

/* ---------------------------------------------------------------------------------------------
   Classes
   --------------------------------------------------------------------------------------------- */

void x_clsInit( void );
void x_clsReleaseAll( void );
X_CLASS_HANDLE x_clsCreate( size_t instanceDataCount, const char *className );
int x_clsAdd( X_CLASS_HANDLE classHandle, const char *methodName, X_METHODPTR function );
int x_clsAddClassMethod( X_CLASS_HANDLE classHandle, const char *methodName, X_METHODPTR function );
int x_clsAddData( X_CLASS_HANDLE classHandle, const char *dataName, size_t dataIndex );
int x_clsAddVirtual( X_CLASS_HANDLE classHandle, const char *methodName );
int x_clsAddInline( X_CLASS_HANDLE classHandle, const char *methodName, const X_ITEM *block );
int x_clsAddOnError( X_CLASS_HANDLE classHandle, X_METHODPTR function );
int x_clsAddAccess( X_CLASS_HANDLE classHandle, const char *propertyName, X_METHODPTR function );
int x_clsAddAssign( X_CLASS_HANDLE classHandle, const char *propertyName, X_METHODPTR function );
int x_clsAddClassAccess( X_CLASS_HANDLE classHandle, const char *propertyName, X_METHODPTR function );
int x_clsAddClassAssign( X_CLASS_HANDLE classHandle, const char *propertyName, X_METHODPTR function );
int x_clsAddDelegate( X_CLASS_HANDLE classHandle, const char *methodName, const char *objectMessage,
                      const char *delegateMessage );
int x_clsAddClassData( X_CLASS_HANDLE classHandle, const char *dataName, size_t dataIndex,
                       const X_ITEM *initialValue );
int x_clsSetDataType( X_CLASS_HANDLE classHandle, const char *dataName, X_OO_VALUE_TYPE valueType );
int x_clsAddDestructor( X_CLASS_HANDLE classHandle, X_METHODPTR function );
int x_clsAddOperator( X_CLASS_HANDLE classHandle, X_OO_OPERATOR operation, X_METHODPTR function );
int x_clsSetScope( X_CLASS_HANDLE classHandle, const char *message, uint32_t scope );
int x_clsAddFriendClass( X_CLASS_HANDLE classHandle, X_CLASS_HANDLE friendHandle );
int x_clsAddParent( X_CLASS_HANDLE classHandle, X_CLASS_HANDLE parentHandle );
int x_clsAssociate( X_CLASS_HANDLE classHandle );
int x_clsDiscard( X_CLASS_HANDLE classHandle );
int x_clsUsesCode( const void *start, size_t size );
void x_clsGCVisitInline( void );
void x_clsGCClearDeadInline( void );
int x_clsIsAssociated( X_CLASS_HANDLE classHandle );
int x_clsReturn( X_CALL_CTX *ctx, X_CLASS_HANDLE classHandle );
X_CLASS_HANDLE x_clsRequire( const char *className, X_FUNCPTR factory );
int x_clsFactoryCreate( X_CALL_CTX *ctx, X_ITEM *result, X_CLASS_HANDLE classHandle );
const X_CLASS *x_clsGet( X_CLASS_HANDLE classHandle );
X_CLASS_HANDLE x_clsFindClass( const char *className, const char *classFunction );
const char *x_clsName( X_CLASS_HANDLE classHandle );
const char *x_clsFuncName( X_CLASS_HANDLE classHandle );
const char *x_clsMethodName( X_CLASS_HANDLE classHandle, X_METHOD_HANDLE methodHandle );
int x_clsIsParent( X_CLASS_HANDLE classHandle, const char *parentName );
int x_clsHasDestructor( X_CLASS_HANDLE classHandle );
int x_clsSend( X_CALL_CTX *ctx, X_ITEM *result, X_CLASS_HANDLE classHandle, const char *message, int32_t argc,
               const X_ARG *args );
int x_clsSendSelf( X_CALL_CTX *ctx, X_ITEM *result, const char *message, int32_t argc, const X_ARG *args );

/* ---------------------------------------------------------------------------------------------
   Objects
   --------------------------------------------------------------------------------------------- */

const X_RESOLVED_METHOD *x_objFindMethod( const X_ITEM *object, const char *message );
X_CLASS_HANDLE x_objGetClass( const X_ITEM *item );
int x_objSetClass( X_ITEM *item, X_CLASS_HANDLE classHandle );
int x_objCreate( X_CALL_CTX *ctx, X_ITEM *result, X_CLASS_HANDLE classHandle );
int x_objSend( X_CALL_CTX *ctx, X_ITEM *result, X_ITEM *object, const char *message, int32_t argc,
               const X_ARG *args );
int x_objSendData( X_CALL_CTX *ctx, X_ITEM *result, X_ITEM *object, const char *message, int32_t argc,
                   const X_ARG *args, X_DATA_CACHE *cache );
int x_objIsKindOf( const X_ITEM *item, const char *className );
int x_itemIsValueType( const X_ITEM *item, X_OO_VALUE_TYPE valueType );
int x_objSendSuper( X_CALL_CTX *ctx, X_ITEM *result, const char *message, int32_t argc, const X_ARG *args );
int x_objOperatorCall( X_CALL_CTX *ctx, X_OO_OPERATOR operation, X_ITEM *result, X_ITEM *object,
                       const X_ITEM *argument1, const X_ITEM *argument2 );
void x_objDestructorCall( X_ITEM *object );
const X_METHOD_CONTEXT *x_objMethodContext( X_CALL_CTX *ctx );
void x_objMethodEnter( X_CALL_CTX *ctx, X_METHOD_CONTEXT *frame, X_ITEM *self, const X_ITEM *object, uint32_t owner,
                       X_METHOD_HANDLE method );
void x_objMethodLeave( X_METHOD_CONTEXT *frame );
void x_objEvalClosure( X_BLOCKFUNC function, X_ITEM *result, int32_t argc, const X_ARG *args, X_ITEM *const *env,
                       const X_METHOD_CONTEXT *saved, X_ITEM *self );
int x_objOnErrorInfo( X_CALL_CTX *ctx, X_ITEM *result, X_ITEM *message, X_ITEM *arguments );
X_ITEM *x_objDataSlot( X_CALL_CTX *ctx, size_t index );
const char *x_objGetClsName( const X_ITEM *object );
int x_objHasMsg( X_CALL_CTX *ctx, const X_ITEM *object, const char *message );
int x_objHasOperator( const X_ITEM *object, X_OO_OPERATOR operation );

#ifdef __cplusplus
}
#endif

#endif /* CLASSES_H */
