/*
 * Copyright (c) 2024-2026 xDev-Language
 *
 * Library...: xDev Runtime
 * File......: classes.c
 *
 */

#include "classes.h"

#include <math.h>
#include <string.h>

/* The classes by handle, the message names and the method that runs now */
static X_CLASS **s_classes;
static size_t s_classCount;
static X_SYMBOL *s_symbols;
static int s_initialized;
static int s_releasing;
static X_METHOD_CONTEXT *s_methodContext;

/* The messages of the operators in the order of X_OO_OPERATOR */
static const char *s_operatorNames[X_OO_OP_MAX + 1] = {
   "__OPPLUS", "__OPMINUS", "__OPMULT", "__OPDIVIDE", "__OPMOD", "__OPPOWER", "__OPINC", "__OPDEC",
   "__OPEQUAL", "__OPEXACTEQUAL", "__OPNOTEQUAL", "__OPLESS", "__OPLESSEQUAL", "__OPGREATER",
   "__OPGREATEREQUAL", "__OPASSIGN", "__OPINSTRING", "__OPINCLUDE", "__OPNOT", "__OPAND", "__OPOR",
   "__OPARRAYINDEX", "__ENUMINDEX", "__ENUMBASE", "__ENUMVALUE", "__ENUMSTART", "__ENUMSKIP", "__ENUMSTOP",
   "__ENUMISFIRST", "__ENUMISLAST"
};

static const X_RESOLVED_METHOD *s_classFindMethod( const X_CLASS *classInfo, const char *message );
static int s_objectSendMethod( X_CALL_CTX *ctx, X_ITEM *result, X_ITEM *object,
                               const X_RESOLVED_METHOD *resolvedMethod, X_CLASS_HANDLE lookupClass, int32_t argc,
                               const X_ARG *args, int scopeCheck, int destructorCall, const char *attemptedMessage );

/* ---------------------------------------------------------------------------------------------
   Helpers
   --------------------------------------------------------------------------------------------- */

static void s_releaseRestore( int *previous )
{
   s_releasing = *previous;
}

static void s_methodRestore( X_METHOD_CONTEXT **previous )
{
   s_methodContext = *previous;
}

/* The end of a destructor call */
static void s_destructorLeave( X_ITEM *anchor )
{
   x_gcEndDestructor( anchor );
   x_itemClear( anchor );
}

/* The method context of the call; NULL when the call is not the method that runs now */
static const X_METHOD_CONTEXT *s_currentMethodContext( X_CALL_CTX *ctx )
{
   return IIF( ctx->methodContext == s_methodContext, s_methodContext, NULL );
}

/* ---------------------------------------------------------------------------------------------
   Message names
   --------------------------------------------------------------------------------------------- */

/* FNV-1a of a name without case */
static uint32_t s_symbolHash( const char *name, size_t length )
{
   uint32_t hash = 2166136261u;
   size_t i;

   for( i = 0; i < length; ++i )
   {
      int ch = ( unsigned char )name[i];

      if( ch >= 'a' && ch <= 'z' )
         ch -= 'a' - 'A';

      hash ^= ( uint32_t )ch;
      hash *= 16777619u;
   }

   return hash;
}

/* The symbol of a name; NULL when there is none */
static X_SYMBOL *s_symbolFind( const char *name )
{
   X_SYMBOL *symbol;
   size_t length = strlen( name );
   uint32_t hash = s_symbolHash( name, length );

   for( symbol = s_symbols; symbol != NULL; symbol = symbol->next )
   {
      if( symbol->hash == hash && symbol->length == length && x_stricmp( symbol->name, name ) == 0 )
         return symbol;
   }

   return NULL;
}

/* The symbol of a name, created on the first use; names are stored in upper case */
static X_SYMBOL *s_symbolIntern( const char *name )
{
   X_SYMBOL *symbol = s_symbolFind( name );
   size_t length;
   uint32_t hash;

   if( symbol != NULL )
      return symbol;

   length = strlen( name );
   hash = s_symbolHash( name, length );

   symbol = ( X_SYMBOL * )x_xgrab( sizeof( X_SYMBOL ) );
   symbol->name = x_strdup( name );
   x_strupr( symbol->name );
   symbol->length = length;
   symbol->hash = hash;
   symbol->next = s_symbols;
   s_symbols = symbol;

   return symbol;
}

/* The symbol of prefix + propertyName, created when create is 1 */
static X_SYMBOL *s_propertySymbol( const char *prefix, const char *propertyName, int create )
{
   size_t prefixLength;
   size_t propertyLength;
   char *name;
   X_SYMBOL *symbol;

   if( propertyName == NULL )
      return NULL;

   prefixLength = strlen( prefix );
   propertyLength = strlen( propertyName );

   if( propertyLength > SIZE_MAX - prefixLength - 1 )
      return NULL;

   name = ( char * )x_xgrab( prefixLength + propertyLength + 1 );
   memcpy( name, prefix, prefixLength );
   memcpy( name + prefixLength, propertyName, propertyLength + 1 );
   symbol = IIF( create, s_symbolIntern( name ), s_symbolFind( name ) );
   x_xfree( name );

   return symbol;
}

/* ---------------------------------------------------------------------------------------------
   Building a class
   --------------------------------------------------------------------------------------------- */

/* A class that can still be changed; NULL while the classes are released */
static X_CLASS *s_classGetMutable( X_CLASS_HANDLE classHandle )
{
   return IIF( s_releasing, NULL, ( X_CLASS * )x_clsGet( classHandle ) );
}

/* A message defined by the class itself */
static X_METHOD *s_classFindOwnMethod( X_CLASS *classInfo, const X_SYMBOL *message )
{
   size_t i;

   for( i = 0; i < classInfo->methodCount; ++i )
   {
      X_METHOD *method = &classInfo->methods[i];

      if( method->message == message )
         return method;
   }

   return NULL;
}

/* A message defined by the class itself, by name without case */
static X_METHOD *s_classFindOwnMethodByName( X_CLASS *classInfo, const char *message )
{
   size_t i;

   for( i = 0; i < classInfo->methodCount; ++i )
   {
      X_METHOD *method = &classInfo->methods[i];

      if( x_stricmp( method->message->name, message ) == 0 )
         return method;
   }

   return NULL;
}

/* The position of a method in the methods of the class; SIZE_MAX for NULL */
static size_t s_classMethodIndex( const X_CLASS *classInfo, const X_METHOD *method )
{
   if( method == NULL )
      return SIZE_MAX;

   return ( size_t )( method - classInfo->methods );
}

/* The method at a position; NULL for SIZE_MAX */
static X_METHOD *s_classMethodAt( X_CLASS *classInfo, size_t index )
{
   if( index == SIZE_MAX )
      return NULL;

   return &classInfo->methods[index];
}

/* Room for one more method; the destructor and the operators point into the methods, so they move too */
static int s_classGrowMethods( X_CLASS *classInfo )
{
   size_t capacity;
   size_t destructorIndex;
   size_t operatorIndices[X_OO_OP_MAX + 1];
   size_t i;

   if( classInfo->methodCount < classInfo->methodCapacity )
      return 1;

   capacity = IIF( classInfo->methodCapacity == 0, 4, classInfo->methodCapacity * 2 );

   if( capacity < classInfo->methodCapacity || capacity > SIZE_MAX / sizeof( X_METHOD ) )
      return 0;

   destructorIndex = s_classMethodIndex( classInfo, classInfo->destructor );
   for( i = 0; i <= X_OO_OP_MAX; ++i )
      operatorIndices[i] = s_classMethodIndex( classInfo, classInfo->ownOperators[i] );

   classInfo->methods = ( X_METHOD * )x_xreallocArray( classInfo->methods, capacity, sizeof( X_METHOD ) );
   classInfo->destructor = s_classMethodAt( classInfo, destructorIndex );
   for( i = 0; i <= X_OO_OP_MAX; ++i )
      classInfo->ownOperators[i] = s_classMethodAt( classInfo, operatorIndices[i] );

   classInfo->methodCapacity = capacity;

   return 1;
}

/* The messages a method answers: DATA also answers _name, ACCESS and ASSIGN answer the property name */
static size_t s_methodPublicMessages( X_SYMBOL *message, X_OO_MESSAGE_TYPE kind, X_SYMBOL **messages )
{
   const char *name = message->name;

   messages[0] = message;
   switch( kind )
   {
   case X_OO_MSG_DATA:
   case X_OO_MSG_CLASSDATA:
      messages[1] = s_propertySymbol( "_", name, 1 );
      return 2;

   case X_OO_MSG_ACCESS:
      messages[0] = s_symbolIntern( name + strlen( "__ACCESS_" ) );
      break;

   case X_OO_MSG_CLSACCESS:
      messages[0] = s_symbolIntern( name + strlen( "__CLSACCESS_" ) );
      break;

   case X_OO_MSG_ASSIGN:
      messages[0] = s_propertySymbol( "_", name + strlen( "__ASSIGN_" ), 1 );
      break;

   case X_OO_MSG_CLSASSIGN:
      messages[0] = s_propertySymbol( "_", name + strlen( "__CLSASSIGN_" ), 1 );
      break;

   default:
      break;
   }

   return 1;
}

/* A new method of the class; NULL when one of its messages is taken by another method */
static X_METHOD *s_classAddMethod( X_CLASS *classInfo, X_SYMBOL *message, X_OO_MESSAGE_TYPE kind )
{
   X_METHOD *method = s_classFindOwnMethod( classInfo, message );
   X_SYMBOL *messages[2];
   size_t count, i, j, k;

   if( method != NULL )
      return IIF( method->kind == kind, method, NULL );

   count = s_methodPublicMessages( message, kind, messages );
   for( i = 0; i < classInfo->methodCount; ++i )
   {
      X_METHOD *other = &classInfo->methods[i];
      X_SYMBOL *existing[2];
      size_t existingCount = s_methodPublicMessages( other->message, other->kind, existing );

      for( j = 0; j < count; ++j )
      {
         for( k = 0; k < existingCount; ++k )
         {
            if( messages[j] == existing[k] )
               return NULL;
         }
      }
   }

   if( classInfo->methodCount == UINT32_MAX || !s_classGrowMethods( classInfo ) )
      return NULL;

   method = &classInfo->methods[classInfo->methodCount++];
   *method = ( X_METHOD ){ 0 };
   method->handle = ( X_METHOD_HANDLE )classInfo->methodCount;
   method->message = message;
   method->definingClass = classInfo->handle;
   method->kind = kind;
   method->scope = X_OO_CLSTP_EXPORTED;

   return method;
}

/* Append a class handle to a growing list */
static int s_classAppendHandle( X_CLASS_HANDLE **values, size_t *count, size_t *capacity, X_CLASS_HANDLE value )
{
   X_CLASS_HANDLE *newValues;
   size_t newCapacity;

   if( *count == *capacity )
   {
      newCapacity = IIF( *capacity == 0, 4, *capacity * 2 );

      if( newCapacity < *capacity || newCapacity > SIZE_MAX / sizeof( X_CLASS_HANDLE ) )
         return 0;

      newValues = ( X_CLASS_HANDLE * )x_xreallocArray( *values, newCapacity, sizeof( X_CLASS_HANDLE ) );
      *values = newValues;
      *capacity = newCapacity;
   }

   ( *values )[( *count )++] = value;

   return 1;
}

/* Is the class handle in the lineage of the class? */
static int s_classHasHandle( const X_CLASS *classInfo, X_CLASS_HANDLE classHandle )
{
   size_t i;

   for( i = 0; i < classInfo->lineageCount; ++i )
   {
      if( classInfo->lineage[i] == classHandle )
         return 1;
   }

   return 0;
}

/* Is the class the given class or one of its descendants? */
static int s_classHasAncestor( const X_CLASS *classInfo, X_CLASS_HANDLE classHandle )
{
   size_t i;

   if( classInfo->handle == classHandle )
      return 1;

   for( i = 0; i < classInfo->parentCount; ++i )
   {
      const X_CLASS *parent = x_clsGet( classInfo->parents[i] );

      if( parent != NULL && s_classHasAncestor( parent, classHandle ) )
         return 1;
   }

   return 0;
}

/* A PROTECTED message is open to the class that defines it and to its descendants */
static int s_classCanAccessProtected( X_CLASS_HANDLE senderHandle, X_CLASS_HANDLE ownerHandle )
{
   const X_CLASS *sender = x_clsGet( senderHandle );

   return sender != NULL && s_classHasAncestor( sender, ownerHandle );
}

/* Is the class a friend of the class? */
static int s_classHasFriend( const X_CLASS *classInfo, X_CLASS_HANDLE friendHandle )
{
   size_t i;

   for( i = 0; i < classInfo->friendCount; ++i )
   {
      if( classInfo->friends[i] == friendHandle )
         return 1;
   }

   return 0;
}

/* Where the instance data of a class of the lineage starts in an object */
static size_t s_classDataOffset( const X_CLASS *classInfo, X_CLASS_HANDLE definingClass )
{
   size_t i;
   size_t offset = 0;

   for( i = 0; i < classInfo->lineageCount; ++i )
   {
      const X_CLASS *owner = x_clsGet( classInfo->lineage[i] );

      if( owner->handle == definingClass )
         return offset;

      offset += owner->ownDataCount;
   }

   return 0;
}

/* Is a class variable SHARED with the subclasses? */
static int s_classClassDataIsShared( const X_CLASS *classInfo, size_t dataIndex )
{
   size_t i;

   for( i = 0; i < classInfo->methodCount; ++i )
   {
      const X_METHOD *method = &classInfo->methods[i];

      if( method->kind == X_OO_MSG_CLASSDATA && method->dataIndex == dataIndex )
         return method->scope & X_OO_CLSTP_SHARED;
   }

   return 0;
}

/* Does the class keep its own copy of a class variable of owner? */
static int s_classStoresClassData( const X_CLASS *classInfo, const X_CLASS *owner, size_t dataIndex )
{
   return owner == classInfo || !s_classClassDataIsShared( owner, dataIndex );
}

/* The position (from 1) of a class variable in the class data of the class; 0 when it has none */
static size_t s_classClassDataIndex( const X_CLASS *classInfo, X_CLASS_HANDLE definingClass, size_t dataIndex )
{
   size_t i;
   size_t index = 0;

   for( i = 0; i < classInfo->lineageCount; ++i )
   {
      const X_CLASS *owner = x_clsGet( classInfo->lineage[i] );
      size_t j;

      for( j = 0; j < owner->ownClassDataCount; ++j )
      {
         if( s_classStoresClassData( classInfo, owner, j + 1 ) )
         {
            ++index;

            if( owner->handle == definingClass && j + 1 == dataIndex )
               return index;
         }
      }
   }

   return 0;
}

/* Set the initial value of a class variable defined by the class */
static int s_classSetOwnClassData( X_CLASS *classInfo, size_t dataIndex, const X_ITEM *initialValue )
{
   X_ITEM nilItem = X_ITEM_NIL_INIT;
   size_t capacity;
   size_t i;

   if( dataIndex > classInfo->ownClassDataCapacity )
   {
      capacity = IIF( classInfo->ownClassDataCapacity == 0, 4, classInfo->ownClassDataCapacity );

      while( capacity < dataIndex )
      {
         if( capacity > SIZE_MAX / 2 || capacity > SIZE_MAX / sizeof( X_ITEM ) / 2 )
            return 0;

         capacity *= 2;
      }

      classInfo->ownClassData = ( X_ITEM * )x_xreallocArray( classInfo->ownClassData, capacity, sizeof( X_ITEM ) );
      classInfo->ownClassDataCapacity = capacity;
   }

   for( i = classInfo->ownClassDataCount; i < dataIndex; ++i )
      classInfo->ownClassData[i] = nilItem;

   if( classInfo->ownClassDataCount < dataIndex )
      classInfo->ownClassDataCount = dataIndex;

   if( initialValue != NULL )
      x_itemCopy( &classInfo->ownClassData[dataIndex - 1], initialValue );
   else
      x_itemClear( &classInfo->ownClassData[dataIndex - 1] );

   return 1;
}

/* Put a method into the method table of the class, with the place of its data in this class */
static void s_classSetResolvedMethod( X_CLASS *classInfo, X_METHOD *method )
{
   size_t mask = classInfo->methodTableCapacity - 1;
   size_t slot = method->message->hash & mask;
   X_RESOLVED_METHOD *entry;

   while( classInfo->methodTable[slot].method != NULL )
   {
      if( classInfo->methodTable[slot].method->message == method->message )
         break;

      slot = ( slot + 1 ) & mask;
   }

   entry = &classInfo->methodTable[slot];
   if( entry->method == NULL )
      ++classInfo->methodTableCount;

   entry->method = method;
   if( method->kind == X_OO_MSG_DATA )
      entry->dataIndex = s_classDataOffset( classInfo, method->definingClass ) + method->dataIndex;
   else if( method->kind == X_OO_MSG_CLASSDATA )
      entry->dataIndex = s_classClassDataIndex( classInfo, method->definingClass, method->dataIndex );
   else
      entry->dataIndex = 0;
}

/* Append a class to the lineage of the class */
static int s_classAddLineage( X_CLASS *classInfo, X_CLASS_HANDLE handle )
{
   return s_classAppendHandle( &classInfo->lineage, &classInfo->lineageCount, &classInfo->lineageCapacity, handle );
}

/* The lineage: the classes of the parents in order, then the class itself */
static int s_classBuildLineage( X_CLASS *classInfo )
{
   size_t i;

   for( i = 0; i < classInfo->parentCount; ++i )
   {
      const X_CLASS *parent = x_clsGet( classInfo->parents[i] );
      size_t j;

      for( j = 0; j < parent->lineageCount; ++j )
      {
         X_CLASS_HANDLE handle = parent->lineage[j];

         if( s_classHasHandle( classInfo, handle ) )
            continue;

         if( !s_classAddLineage( classInfo, handle ) )
            return 0;
      }
   }

   return s_classAddLineage( classInfo, classInfo->handle );
}

/* The class data: the class variables of the lineage, copied from the parents or from the own values */
static int s_classBuildClassData( X_CLASS *classInfo )
{
   X_ITEM nilItem = X_ITEM_NIL_INIT;
   size_t count = 0;
   size_t i;

   for( i = 0; i < classInfo->lineageCount; ++i )
   {
      const X_CLASS *owner = x_clsGet( classInfo->lineage[i] );
      size_t j;

      for( j = 0; j < owner->ownClassDataCount; ++j )
      {
         if( s_classStoresClassData( classInfo, owner, j + 1 ) )
         {
            if( count == SIZE_MAX )
               return 0;

            ++count;
         }
      }
   }

   if( count > SIZE_MAX / sizeof( X_ITEM ) )
      return 0;

   if( count == 0 )
      return 1;

   classInfo->classData = ( X_ITEM * )x_xgrabArray( count, sizeof( X_ITEM ) );
   classInfo->classDataCount = count;

   for( i = 0; i < count; ++i )
      classInfo->classData[i] = nilItem;

   for( i = 0; i < classInfo->lineageCount; ++i )
   {
      const X_CLASS *owner = x_clsGet( classInfo->lineage[i] );
      const X_CLASS *source = classInfo;
      size_t j;

      if( owner != classInfo )
      {
         for( j = 0; j < classInfo->parentCount; ++j )
         {
            const X_CLASS *parent = x_clsGet( classInfo->parents[j] );

            if( s_classHasHandle( parent, owner->handle ) )
            {
               source = parent;
               break;
            }
         }
      }

      for( j = 0; j < owner->ownClassDataCount; ++j )
      {
         size_t targetIndex;

         if( !s_classStoresClassData( classInfo, owner, j + 1 ) )
            continue;

         targetIndex = s_classClassDataIndex( classInfo, owner->handle, j + 1 );

         if( source == classInfo )
            x_itemCopy( &classInfo->classData[targetIndex - 1], &classInfo->ownClassData[j] );
         else
         {
            size_t sourceIndex = s_classClassDataIndex( source, owner->handle, j + 1 );

            x_itemCopy( &classInfo->classData[targetIndex - 1], &source->classData[sourceIndex - 1] );
         }
      }
   }

   for( i = 0; i < classInfo->methodCount; ++i )
   {
      X_METHOD *method = &classInfo->methods[i];

      if( method->kind == X_OO_MSG_CLASSDATA && method->scope & X_OO_CLSTP_SHARED )
         method->ownOffset = s_classClassDataIndex( classInfo, classInfo->handle, method->dataIndex );
   }

   return 1;
}

/* The method table: the methods of the parents, the last parent first, then the own methods */
static int s_classBuildMethodTable( X_CLASS *classInfo )
{
   size_t capacity = 4;
   size_t maximum = classInfo->methodCount;
   size_t i;

   for( i = 0; i < classInfo->parentCount; ++i )
   {
      const X_CLASS *parent = x_clsGet( classInfo->parents[i] );

      if( maximum > SIZE_MAX - parent->methodTableCount )
         return 0;

      maximum += parent->methodTableCount;
   }

   if( maximum > SIZE_MAX / 2 )
      return 0;

   while( capacity < maximum * 2 )
   {
      if( capacity > SIZE_MAX / 2 || capacity > SIZE_MAX / sizeof( X_RESOLVED_METHOD ) / 2 )
         return 0;

      capacity *= 2;
   }

   classInfo->methodTable = ( X_RESOLVED_METHOD * )x_xgrabzArray( capacity, sizeof( X_RESOLVED_METHOD ) );
   classInfo->methodTableCapacity = capacity;

   for( i = classInfo->parentCount; i > 0; --i )
   {
      const X_CLASS *parent = x_clsGet( classInfo->parents[i - 1] );
      size_t j;

      for( j = 0; j < parent->methodTableCapacity; ++j )
      {
         if( parent->methodTable[j].method != NULL )
            s_classSetResolvedMethod( classInfo, parent->methodTable[j].method );
      }
   }

   for( i = 0; i < classInfo->methodCount; ++i )
      s_classSetResolvedMethod( classInfo, &classInfo->methods[i] );

   return 1;
}

/* The methods of the operators, found once when the class is associated */
static void s_classBuildOperators( X_CLASS *classInfo )
{
   size_t operation;

   for( operation = 0; operation <= X_OO_OP_MAX; ++operation )
      classInfo->operators[operation] = s_classFindMethod( classInfo, s_operatorNames[operation] );
}

/* ---------------------------------------------------------------------------------------------
   Finding messages
   --------------------------------------------------------------------------------------------- */

/* A message in the method table of the class, by name without case; NULL when there is none */
static const X_RESOLVED_METHOD *s_classFindMethod( const X_CLASS *classInfo, const char *message )
{
   uint32_t hash;
   size_t slot;

   if( classInfo->methodTableCapacity == 0 )
      return NULL;

   hash = s_symbolHash( message, strlen( message ) );
   slot = hash & ( classInfo->methodTableCapacity - 1 );

   while( classInfo->methodTable[slot].method != NULL )
   {
      const X_RESOLVED_METHOD *method = &classInfo->methodTable[slot];

      if( method->method->message->hash == hash && x_stricmp( method->method->message->name, message ) == 0 )
         return method;

      slot = ( slot + 1 ) & ( classInfo->methodTableCapacity - 1 );
   }

   return NULL;
}

/* The ACCESS (no argument) or ASSIGN (one argument) method of a property */
static const X_RESOLVED_METHOD *s_classFindPropertyMethod( const X_CLASS *classInfo, const char *propertyName,
                                                            int32_t argc )
{
   const char *prefix;
   X_SYMBOL *symbol;

   if( propertyName[0] == '_' && argc == 1 )
      ++propertyName;

   if( argc == 0 )
      prefix = "__ACCESS_";
   else if( argc == 1 )
      prefix = "__ASSIGN_";
   else
      return NULL;

   symbol = s_propertySymbol( prefix, propertyName, 0 );

   return IIF( symbol != NULL, s_classFindMethod( classInfo, symbol->name ), NULL );
}

/* The class ACCESS (no argument) or class ASSIGN (one argument) method of a property */
static const X_RESOLVED_METHOD *s_classFindClassPropertyMethod( const X_CLASS *classInfo,
                                                                 const char *propertyName, int32_t argc )
{
   const char *prefix;
   X_SYMBOL *symbol;

   if( propertyName[0] == '_' && argc == 1 )
      ++propertyName;

   if( argc == 0 )
      prefix = "__CLSACCESS_";
   else if( argc == 1 )
      prefix = "__CLSASSIGN_";
   else
      return NULL;

   symbol = s_propertySymbol( prefix, propertyName, 0 );

   return IIF( symbol != NULL, s_classFindMethod( classInfo, symbol->name ), NULL );
}

/* The message of DELEGATE that gives the object: a method or an ACCESS */
static const X_RESOLVED_METHOD *s_classFindDelegateObject( const X_CLASS *classInfo, const char *objectMessage )
{
   const X_RESOLVED_METHOD *method = s_classFindMethod( classInfo, objectMessage );

   return IIF( method != NULL, method, s_classFindPropertyMethod( classInfo, objectMessage, 0 ) );
}

/* The method of a message: a method, an ACCESS or ASSIGN, _name of a variable or a class property */
static const X_RESOLVED_METHOD *s_classResolveMessage( const X_CLASS *classInfo, const char *message, int32_t argc,
                                                       int classOnly )
{
   const X_RESOLVED_METHOD *method = s_classFindMethod( classInfo, message );

   if( method == NULL && !classOnly )
      method = s_classFindPropertyMethod( classInfo, message, argc );

   if( method == NULL && message[0] == '_' && argc == 1 )
   {
      const X_RESOLVED_METHOD *data = s_classFindMethod( classInfo, message + 1 );

      if( data != NULL )
      {
         X_OO_MESSAGE_TYPE kind = data->method->kind;

         if( kind == X_OO_MSG_CLASSDATA || ( !classOnly && kind == X_OO_MSG_DATA ) )
            method = data;
      }
   }

   if( method == NULL )
      method = s_classFindClassPropertyMethod( classInfo, message, argc );

   return method;
}

/* The method of a message sent from a method: a NONVIRTUAL or an own HIDDEN message of the sending class wins */
static const X_RESOLVED_METHOD *s_classResolveCall( X_CALL_CTX *ctx, const X_CLASS *classInfo, const char *message,
                                                    int32_t argc, int classOnly )
{
   const X_METHOD_CONTEXT *sender = s_currentMethodContext( ctx );

   if( sender != NULL && s_classHasAncestor( classInfo, sender->definingClass ) )
   {
      const X_CLASS *owner = x_clsGet( sender->definingClass );
      const X_RESOLVED_METHOD *local = s_classResolveMessage( owner, message, argc, classOnly );

      if( local != NULL )
      {
         uint32_t scope = local->method->scope;
         int ownHidden = ( scope & X_OO_CLSTP_HIDDEN ) && local->method->definingClass == owner->handle;

         if( ( scope & X_OO_CLSTP_NONVIRTUAL ) || ownHidden )
            return local;
      }
   }

   return s_classResolveMessage( classInfo, message, argc, classOnly );
}

/* ---------------------------------------------------------------------------------------------
   Scope checks
   --------------------------------------------------------------------------------------------- */

/* Can the method that runs now send a HIDDEN or PROTECTED message? */
static int s_classCanAccess( X_CALL_CTX *ctx, const X_METHOD *method )
{
   const X_METHOD_CONTEXT *sender = s_currentMethodContext( ctx );
   const X_CLASS *owner;

   if( !( method->scope & ( X_OO_CLSTP_HIDDEN | X_OO_CLSTP_PROTECTED ) ) )
      return 1;

   owner = x_clsGet( method->definingClass );
   if( sender == NULL )
      return 0;

   if( sender->definingClass == method->definingClass )
      return 1;

   if( s_classHasFriend( owner, sender->definingClass ) )
      return 1;

   if( method->scope & X_OO_CLSTP_HIDDEN )
      return 0;

   return s_classCanAccessProtected( sender->definingClass, method->definingClass );
}

/* 1, or error 3001 when the message is out of scope */
static int s_classCheckAccess( X_CALL_CTX *ctx, X_ITEM *result, const X_METHOD *method )
{
   if( s_classCanAccess( ctx, method ) )
      return 1;

   return x_errorRaiseNamed( result, 3001, method->message->name );
}

/* READONLY guards a member from outside: the methods of the class that defines it, of its subclasses and of
   its friends may write it */
static int s_classWritesReadonly( X_CALL_CTX *ctx, const X_METHOD *member )
{
   const X_METHOD_CONTEXT *sender = s_currentMethodContext( ctx );
   const X_CLASS *owner = x_clsGet( member->definingClass );

   if( sender == NULL )
      return 0;

   if( s_classCanAccessProtected( sender->definingClass, member->definingClass ) )
      return 1;

   return owner != NULL && s_classHasFriend( owner, sender->definingClass );
}

/* Is the member READONLY for the method that runs now? */
static int s_classReadonlyFor( X_CALL_CTX *ctx, const X_METHOD *member )
{
   return ( member->scope & X_OO_CLSTP_READONLY ) && !s_classWritesReadonly( ctx, member );
}

/* May the message write? An assignment to a READONLY variable or property is refused */
static int s_classAllowsWrite( X_CALL_CTX *ctx, const X_CLASS *classInfo, const X_METHOD *method, int32_t argc )
{
   X_OO_MESSAGE_TYPE kind = method->kind;
   const char *name = method->message->name;
   const char *property = NULL;
   const X_RESOLVED_METHOD *getter = NULL;
   const X_RESOLVED_METHOD *ownerGetter = NULL;
   int classProperty = kind == X_OO_MSG_CLSASSIGN;
   int assignment = kind == X_OO_MSG_DATA || kind == X_OO_MSG_CLASSDATA || kind == X_OO_MSG_ASSIGN || classProperty;

   if( argc == 0 )
      return 1;

   if( kind == X_OO_MSG_ASSIGN )
      property = name + strlen( "__ASSIGN_" );
   else if( classProperty )
      property = name + strlen( "__CLSASSIGN_" );
   else if( name[0] == '_' && name[1] != '_' )
   {
      property = name + 1;
      assignment = 1;
   }

   if( property != NULL )
   {
      getter = s_classResolveMessage( classInfo, property, 0, classProperty );
      ownerGetter = s_classResolveMessage( x_clsGet( method->definingClass ), property, 0, classProperty );
   }

   if( !assignment )
      return 1;

   if( s_classReadonlyFor( ctx, method ) )
      return 0;

   if( getter != NULL && s_classReadonlyFor( ctx, getter->method ) )
      return 0;

   return ownerGetter == NULL || !s_classReadonlyFor( ctx, ownerGetter->method );
}

/* 1, or error 3002 when the message may not write */
static int s_classCheckReadonly( X_CALL_CTX *ctx, X_ITEM *result, const X_CLASS *classInfo, const X_METHOD *method,
                                 int32_t argc )
{
   if( s_classAllowsWrite( ctx, classInfo, method, argc ) )
      return 1;

   return x_errorRaiseNamed( result, 3002, method->message->name );
}

/* Check a message and its arguments before sending it */
static int s_messageArguments( X_ITEM *result, const char *message, int32_t argc, const X_ARG *args )
{
   int32_t i;

   if( result == NULL )
      return 0;

   if( message == NULL || *message == '\0' )
      return x_errorRaiseNamed( result, 3000, "message" );

   if( argc < 0 || ( argc > 0 && args == NULL ) )
      return x_errorRaiseNamed( result, 3003, message );

   for( i = 0; i < argc; ++i )
   {
      if( args[i].value == NULL )
         return x_errorRaiseNamed( result, 3003, message );
   }

   return 1;
}

/* The number of arguments a kind of method takes */
static int s_methodAcceptsArity( const X_METHOD *method, int32_t argc )
{
   switch( method->kind )
   {
   case X_OO_MSG_ACCESS:
   case X_OO_MSG_CLSACCESS:
   case X_OO_MSG_DESTRUCTOR:
      return argc == 0;

   case X_OO_MSG_ASSIGN:
   case X_OO_MSG_CLSASSIGN:
      return argc == 1;

   case X_OO_MSG_DATA:
   case X_OO_MSG_CLASSDATA:
      return argc <= 1;

   default:
      return 1;
   }
}

/* 1, or error 3003 for a wrong number of arguments */
static int s_methodArity( X_ITEM *result, const X_METHOD *method, int32_t argc )
{
   if( s_methodAcceptsArity( method, argc ) )
      return 1;

   return x_errorRaiseNamed( result, 3003, method->message->name );
}

/* Is there a variable or a property of this name, with or without the leading _? */
static int s_classPropertyExists( const X_CLASS *classInfo, const char *message, int classOnly )
{
   const char *property = IIF( message[0] == '_', message + 1, message );
   const X_RESOLVED_METHOD *data = s_classFindMethod( classInfo, property );

   if( data != NULL )
   {
      X_OO_MESSAGE_TYPE kind = data->method->kind;

      if( kind == X_OO_MSG_CLASSDATA || ( !classOnly && kind == X_OO_MSG_DATA ) )
         return 1;
   }

   if( !classOnly )
   {
      if( s_classFindPropertyMethod( classInfo, property, 0 ) != NULL )
         return 1;

      if( s_classFindPropertyMethod( classInfo, property, 1 ) != NULL )
         return 1;
   }

   if( s_classFindClassPropertyMethod( classInfo, property, 0 ) != NULL )
      return 1;

   return s_classFindClassPropertyMethod( classInfo, property, 1 ) != NULL;
}

/* Does ON ERROR already handle this message for this object? */
static int s_onErrorActive( const X_ITEM *object, const char *message )
{
   const X_METHOD_CONTEXT *frame;

   for( frame = s_methodContext; frame != NULL; frame = frame->previous )
   {
      if( frame->attemptedMessage == NULL || frame->self == NULL )
         continue;

      if( x_arrayId( frame->self ) == x_arrayId( object ) && x_stricmp( frame->attemptedMessage, message ) == 0 )
         return 1;
   }

   return 0;
}

/* ---------------------------------------------------------------------------------------------
   Typed instance variables
   --------------------------------------------------------------------------------------------- */

/* Does a value fit a non numeric type after AS? NIL fits every type but DATE */
int x_itemIsValueType( const X_ITEM *item, X_OO_VALUE_TYPE valueType )
{
   if( valueType == X_OO_VALUE_DATE )
      return X_IS_DATE( item );

   if( X_ITEM_TYPERAW( item ) == X_IT_NIL )
      return 1;

   switch( valueType )
   {
   case X_OO_VALUE_ARRAY:
      return X_IS_ARRAY( item ) && x_objGetClass( item ) == 0;

   case X_OO_VALUE_CODEBLOCK:
      return X_ITEM_TYPERAW( item ) == X_IT_BLOCK;

   case X_OO_VALUE_OBJECT:
      return x_objGetClass( item ) != 0;

   case X_OO_VALUE_HASH:
      return X_IS_HASH( item );

   default:
      return 0;
   }
}

/* Error 3107: the value has a wrong type */
static int s_typeError( X_ITEM *result )
{
   return x_errorRaiseNamed( result, 3107, "typed conversion" );
}

/* Convert a value to the type of a typed variable; error 3107 for a wrong type, 3105 out of range */
static int s_dataConvert( X_ITEM *result, X_OO_VALUE_TYPE valueType, X_ITEM *value )
{
   static const int bits[] = { 8, 16, 32, 64, 8, 16, 32, 64 };
   X_TYPE type = X_ITEM_TYPERAW( value );
   int signedTarget, width;
   uint64_t limit, magnitude;
   double whole, upper, lower;

   switch( valueType )
   {
   case X_OO_VALUE_ANY:
      return 1;

   case X_OO_VALUE_LOGIC:
      return IIF( type == X_IT_LOGICAL, 1, s_typeError( result ) );

   case X_OO_VALUE_STRING:
      return IIF( type == X_IT_STRING, 1, s_typeError( result ) );

   case X_OO_VALUE_PTR:
      return IIF( type == X_IT_NIL || type == X_IT_POINTER, 1, s_typeError( result ) );

   case X_OO_VALUE_ARRAY:
   case X_OO_VALUE_CODEBLOCK:
   case X_OO_VALUE_OBJECT:
   case X_OO_VALUE_DATE:
   case X_OO_VALUE_HASH:
      return IIF( x_itemIsValueType( value, valueType ), 1, s_typeError( result ) );

   case X_OO_VALUE_FLOAT32:
   case X_OO_VALUE_FLOAT64:
      if( type == X_IT_INTEGER )
         x_itemPutND( value, ( double )value->u.integer );
      else if( type == X_IT_UINT64 )
         x_itemPutND( value, ( double )value->u.unsignedInteger );
      else if( type != X_IT_DOUBLE )
         return s_typeError( result );

      if( valueType == X_OO_VALUE_FLOAT32 )
         value->u.dbl = ( double )( float )value->u.dbl;

      return 1;

   default:
      break;
   }

   signedTarget = valueType <= X_OO_VALUE_INT64;
   width = bits[valueType - X_OO_VALUE_INT8];
   limit = IIF( signedTarget, UINT64_MAX >> ( 65 - width ), UINT64_MAX >> ( 64 - width ) );

   if( type == X_IT_DOUBLE )
   {
      whole = trunc( value->u.dbl );
      upper = ldexp( 1.0, width - signedTarget );
      lower = IIF( signedTarget, -upper, 0.0 );
      if( !( whole >= lower && whole < upper ) )
         return x_errorRaiseNamed( result, 3105, "typed conversion" );

      if( whole < 0 )
         x_itemPutNInt( value, ( int64_t )whole );
      else
      {
         x_itemClear( value );
         value->nType = X_IT_UINT64;
         value->u.unsignedInteger = ( uint64_t )whole;
      }

      type = X_ITEM_TYPERAW( value );
   }

   if( type == X_IT_INTEGER )
   {
      magnitude = IIF( value->u.integer < 0, ( uint64_t )-( value->u.integer + 1 ), ( uint64_t )value->u.integer );
      if( magnitude > limit || ( value->u.integer < 0 && !signedTarget ) )
         return x_errorRaiseNamed( result, 3105, "typed conversion" );
   }
   else if( type == X_IT_UINT64 )
   {
      if( value->u.unsignedInteger > limit )
         return x_errorRaiseNamed( result, 3105, "typed conversion" );
   }
   else
      return s_typeError( result );

   if( valueType == X_OO_VALUE_UINT64 && type == X_IT_INTEGER )
   {
      magnitude = ( uint64_t )value->u.integer;
      value->nType = X_IT_UINT64;
      value->u.unsignedInteger = magnitude;
   }
   else if( valueType != X_OO_VALUE_UINT64 && type == X_IT_UINT64 )
      x_itemPutNInt( value, ( int64_t )value->u.unsignedInteger );

   return 1;
}

/* Read (no argument) or assign (one argument) a class variable; a SHARED one lives in the defining class */
static int s_classDataSend( X_CALL_CTX *ctx, X_ITEM *result, const X_CLASS *actual, const X_METHOD *method,
                            int32_t argc, const X_ARG *args )
{
   X_ITEM value __attribute__( ( cleanup( x_itemClear ) ) ) = X_ITEM_NIL_INIT;
   const X_CLASS *storage;
   size_t index;

   if( method->scope & X_OO_CLSTP_SHARED )
   {
      storage = x_clsGet( method->definingClass );
      index = method->ownOffset;
   }
   else
   {
      storage = actual;
      index = s_classClassDataIndex( actual, method->definingClass, method->dataIndex );
   }

   if( index == 0 || index > storage->classDataCount )
      return x_errorRaiseNamed( result, 3000, method->message->name );

   if( argc == 0 )
   {
      x_itemCopy( result, &storage->classData[index - 1] );
      return 1;
   }

   x_itemCopy( &value, args[0].value );
   if( !s_dataConvert( result, method->valueType, &value ) )
   {
      x_itemClear( result );
      return 0;
   }

   if( x_itemAssign( ctx, &storage->classData[index - 1], &value ) )
      x_itemCopy( result, &storage->classData[index - 1] );
   else
      x_itemClear( result );

   x_itemClear( &value );

   return 1;
}

/* Every DELEGATE needs an object message that is not VIRTUAL */
static int s_classValidateDelegates( const X_CLASS *classInfo )
{
   size_t i;

   for( i = 0; i < classInfo->methodCount; ++i )
   {
      const X_METHOD *method = &classInfo->methods[i];
      const X_RESOLVED_METHOD *delegateObject;

      if( method->kind != X_OO_MSG_DELEGATE )
         continue;

      if( method->delegateObject == NULL || method->delegateMessage == NULL )
         return 0;

      delegateObject = s_classFindDelegateObject( classInfo, method->delegateObject->name );

      if( delegateObject == NULL || delegateObject->method->kind == X_OO_MSG_VIRTUAL )
         return 0;
   }

   return 1;
}

/* Is a class of this name a parent of the class, at any depth? */
static int s_classIsParent( const X_CLASS *classInfo, const char *parentName )
{
   size_t i;

   for( i = 0; i < classInfo->parentCount; ++i )
   {
      const X_CLASS *parent = x_clsGet( classInfo->parents[i] );

      if( parent != NULL && parent->name != NULL )
      {
         if( x_stricmp( parent->name->name, parentName ) == 0 || s_classIsParent( parent, parentName ) )
            return 1;
      }
   }

   return 0;
}

/* ---------------------------------------------------------------------------------------------
   Start and end of the class system
   --------------------------------------------------------------------------------------------- */

/* The class system is ready */
void x_clsInit( void )
{
   s_initialized = 1;
}

/* Mark the INLINE blocks of all classes for the garbage collector */
void x_clsGCVisitInline( void )
{
   size_t i, j;

   for( i = 0; i < s_classCount; ++i )
   {
      if( s_classes[i] == NULL )
         continue;

      for( j = 0; j < s_classes[i]->methodCount; ++j )
         x_gcMarkItem( &s_classes[i]->methods[j].inlineBlock );
   }
}

/* Clear the INLINE blocks the garbage collector found dead */
void x_clsGCClearDeadInline( void )
{
   size_t i, j;

   for( i = 0; i < s_classCount; ++i )
   {
      if( s_classes[i] == NULL )
         continue;

      for( j = 0; j < s_classes[i]->methodCount; ++j )
      {
         X_ITEM *block = &s_classes[i]->methods[j].inlineBlock;

         if( x_gcIsDeadBlock( block ) )
            x_itemClear( block );
      }
   }
}

/* Release all classes at the end of the program; error 3000 while objects or class data are still alive */
static void s_clsRelease( void *cargo )
{
   X_SYMBOL *symbol;
   size_t i, j;

   UNUSED( cargo );
   if( !s_initialized || s_releasing )
      return;

   int previous __attribute__( ( cleanup( s_releaseRestore ) ) ) = s_releasing;

   s_releasing = 1;

   if( s_methodContext != NULL || x_gcIsBusy() )
      goto busy;

   for( i = 0; i < s_classCount; ++i )
   {
      X_CLASS *classInfo = s_classes[i];

      if( classInfo == NULL )
         continue;

      for( j = 0; j < classInfo->classDataCount; ++j )
         x_itemClear( &classInfo->classData[j] );

      for( j = 0; j < classInfo->ownClassDataCount; ++j )
         x_itemClear( &classInfo->ownClassData[j] );
   }

   x_gcCollectClasses();
   if( x_gcHasObjects() )
      goto busy;

   for( i = 0; i < s_classCount; ++i )
   {
      X_CLASS *classInfo = s_classes[i];

      if( classInfo == NULL )
         continue;

      for( j = 0; j < classInfo->methodCount; ++j )
         x_itemClear( &classInfo->methods[j].inlineBlock );
   }

   x_gcCollect();
   if( x_gcHasObjects() )
      goto busy;

   for( i = 0; i < s_classCount; ++i )
   {
      const X_CLASS *classInfo = s_classes[i];

      if( classInfo == NULL )
         continue;

      for( j = 0; j < classInfo->classDataCount; ++j )
      {
         if( classInfo->classData[j].nType != X_IT_NIL )
            goto busy;
      }

      for( j = 0; j < classInfo->ownClassDataCount; ++j )
      {
         if( classInfo->ownClassData[j].nType != X_IT_NIL )
            goto busy;
      }
   }

   for( i = 0; i < s_classCount; ++i )
   {
      X_CLASS *classInfo = s_classes[i];

      if( classInfo == NULL )
         continue;

      x_xfree( classInfo->classData );
      x_xfree( classInfo->ownClassData );
      x_xfree( classInfo->methodTable );
      x_xfree( classInfo->methods );
      x_xfree( classInfo->lineage );
      x_xfree( classInfo->friends );
      x_xfree( classInfo->parents );
      x_xfree( classInfo );
   }

   x_xfree( s_classes );
   s_classes = NULL;
   s_classCount = 0;
   s_methodContext = NULL;

   while( s_symbols != NULL )
   {
      symbol = s_symbols;
      s_symbols = symbol->next;
      x_xfree( symbol->name );
      x_xfree( symbol );
   }

   s_initialized = 0;
   return;

busy:
   {
      X_ITEM result __attribute__( ( cleanup( x_itemClear ) ) ) = X_ITEM_NIL_INIT;

      x_errorRaiseNamed( &result, 3000, "class shutdown: active objects, callbacks or class data" );
   }
}

/* Release all classes; errors stay inside */
void x_clsReleaseAll( void )
{
   x_gcCleanup( s_clsRelease, NULL );
}

/* ---------------------------------------------------------------------------------------------
   Defining classes
   --------------------------------------------------------------------------------------------- */

/* A class that can still get members: it exists and is not associated yet */
static X_CLASS *s_classOpen( X_CLASS_HANDLE classHandle )
{
   X_CLASS *classInfo = s_classGetMutable( classHandle );

   if( classInfo == NULL || classInfo->associated )
      return NULL;

   return classInfo;
}

/* Is a name given and not empty? */
static int s_nameValid( const char *name )
{
   return name != NULL && *name != '\0';
}

/* A new class with instanceDataCount own instance variables; 0 when the name is taken or not valid */
X_CLASS_HANDLE x_clsCreate( size_t instanceDataCount, const char *className )
{
   X_CLASS *classInfo;
   X_SYMBOL *name;

   if( s_releasing )
      return 0;

   if( !s_initialized )
      x_clsInit();

   if( !s_nameValid( className ) )
      return 0;

   if( instanceDataCount > ( size_t )INT64_MAX || instanceDataCount > SIZE_MAX / sizeof( X_ITEM ) )
      return 0;

   if( s_classCount == UINT32_MAX || s_classCount == SIZE_MAX / sizeof( X_CLASS * ) )
      return 0;

   if( x_clsFindClass( className, NULL ) != 0 )
      return 0;

   name = s_symbolIntern( className );
   classInfo = ( X_CLASS * )x_xgrab( sizeof( X_CLASS ) );
   *classInfo = ( X_CLASS ){ 0 };
   classInfo->handle = ( X_CLASS_HANDLE )( s_classCount + 1 );
   classInfo->name = name;
   classInfo->factoryName = name;
   classInfo->ownDataCount = instanceDataCount;

   s_classes = ( X_CLASS ** )x_xreallocArray( s_classes, s_classCount + 1, sizeof( X_CLASS * ) );
   s_classes[s_classCount++] = classInfo;

   return classInfo->handle;
}

/* METHOD */
int x_clsAdd( X_CLASS_HANDLE classHandle, const char *methodName, X_METHODPTR function )
{
   X_CLASS *classInfo = s_classOpen( classHandle );
   X_METHOD *method;

   if( classInfo == NULL || !s_nameValid( methodName ) || function == NULL )
      return 0;

   method = s_classAddMethod( classInfo, s_symbolIntern( methodName ), X_OO_MSG_METHOD );
   if( method == NULL )
      return 0;

   method->function = function;

   return 1;
}

/* CLASSMETHOD */
int x_clsAddClassMethod( X_CLASS_HANDLE classHandle, const char *methodName, X_METHODPTR function )
{
   X_CLASS *classInfo = s_classOpen( classHandle );
   X_METHOD *method;

   if( classInfo == NULL || !s_nameValid( methodName ) || function == NULL )
      return 0;

   method = s_classAddMethod( classInfo, s_symbolIntern( methodName ), X_OO_MSG_CLSMTHD );
   if( method == NULL )
      return 0;

   method->function = function;

   return 1;
}

/* DATA at position dataIndex (from 1) of the own instance variables */
int x_clsAddData( X_CLASS_HANDLE classHandle, const char *dataName, size_t dataIndex )
{
   X_CLASS *classInfo = s_classOpen( classHandle );
   X_METHOD *method;

   if( classInfo == NULL || !s_nameValid( dataName ) )
      return 0;

   if( dataIndex == 0 || dataIndex > classInfo->ownDataCount )
      return 0;

   method = s_classAddMethod( classInfo, s_symbolIntern( dataName ), X_OO_MSG_DATA );
   if( method == NULL )
      return 0;

   method->dataIndex = dataIndex;

   return 1;
}

/* VIRTUAL */
int x_clsAddVirtual( X_CLASS_HANDLE classHandle, const char *methodName )
{
   X_CLASS *classInfo = s_classOpen( classHandle );

   if( classInfo == NULL || !s_nameValid( methodName ) )
      return 0;

   return s_classAddMethod( classInfo, s_symbolIntern( methodName ), X_OO_MSG_VIRTUAL ) != NULL;
}

/* METHOD ... INLINE with a block */
int x_clsAddInline( X_CLASS_HANDLE classHandle, const char *methodName, const X_ITEM *block )
{
   X_CLASS *classInfo = s_classOpen( classHandle );
   X_METHOD *method;

   if( classInfo == NULL || !s_nameValid( methodName ) || block == NULL || block->nType != X_IT_BLOCK )
      return 0;

   method = s_classAddMethod( classInfo, s_symbolIntern( methodName ), X_OO_MSG_INLINE );
   if( method == NULL )
      return 0;

   x_itemCopy( &method->inlineBlock, block );

   return 1;
}

/* ON ERROR */
int x_clsAddOnError( X_CLASS_HANDLE classHandle, X_METHODPTR function )
{
   X_CLASS *classInfo = s_classOpen( classHandle );
   X_METHOD *method;

   if( classInfo == NULL || function == NULL )
      return 0;

   method = s_classAddMethod( classInfo, s_symbolIntern( "__ONERROR" ), X_OO_MSG_ONERROR );
   if( method == NULL )
      return 0;

   method->function = function;
   classInfo->hasOnError = 1;

   return 1;
}

/* An ACCESS, ASSIGN, class ACCESS or class ASSIGN method stored under prefix + the property name */
static int s_classAddProperty( X_CLASS_HANDLE classHandle, const char *prefix, const char *propertyName,
                               X_METHODPTR function, X_OO_MESSAGE_TYPE kind )
{
   X_CLASS *classInfo = s_classOpen( classHandle );
   X_SYMBOL *message;
   X_METHOD *method;

   if( classInfo == NULL || !s_nameValid( propertyName ) || function == NULL )
      return 0;

   message = s_propertySymbol( prefix, propertyName, 1 );
   if( message == NULL )
      return 0;

   method = s_classAddMethod( classInfo, message, kind );
   if( method == NULL )
      return 0;

   method->function = function;

   return 1;
}

/* ACCESS */
int x_clsAddAccess( X_CLASS_HANDLE classHandle, const char *propertyName, X_METHODPTR function )
{
   return s_classAddProperty( classHandle, "__ACCESS_", propertyName, function, X_OO_MSG_ACCESS );
}

/* ASSIGN */
int x_clsAddAssign( X_CLASS_HANDLE classHandle, const char *propertyName, X_METHODPTR function )
{
   return s_classAddProperty( classHandle, "__ASSIGN_", propertyName, function, X_OO_MSG_ASSIGN );
}

/* Class ACCESS */
int x_clsAddClassAccess( X_CLASS_HANDLE classHandle, const char *propertyName, X_METHODPTR function )
{
   return s_classAddProperty( classHandle, "__CLSACCESS_", propertyName, function, X_OO_MSG_CLSACCESS );
}

/* Class ASSIGN */
int x_clsAddClassAssign( X_CLASS_HANDLE classHandle, const char *propertyName, X_METHODPTR function )
{
   return s_classAddProperty( classHandle, "__CLSASSIGN_", propertyName, function, X_OO_MSG_CLSASSIGN );
}

/* DELEGATE: the message goes on as delegateMessage to the object that objectMessage returns */
int x_clsAddDelegate( X_CLASS_HANDLE classHandle, const char *methodName, const char *objectMessage,
                      const char *delegateMessage )
{
   X_CLASS *classInfo = s_classOpen( classHandle );
   X_METHOD *method;

   if( classInfo == NULL || !s_nameValid( methodName ) )
      return 0;

   if( !s_nameValid( objectMessage ) || !s_nameValid( delegateMessage ) )
      return 0;

   method = s_classAddMethod( classInfo, s_symbolIntern( methodName ), X_OO_MSG_DELEGATE );
   if( method == NULL )
      return 0;

   method->delegateObject = s_symbolIntern( objectMessage );
   method->delegateMessage = s_symbolIntern( delegateMessage );

   return 1;
}

/* CLASSDATA at position dataIndex (from 1) of the own class variables */
int x_clsAddClassData( X_CLASS_HANDLE classHandle, const char *dataName, size_t dataIndex,
                       const X_ITEM *initialValue )
{
   X_CLASS *classInfo = s_classOpen( classHandle );
   X_METHOD *method;

   if( classInfo == NULL || !s_nameValid( dataName ) || dataIndex == 0 )
      return 0;

   method = s_classAddMethod( classInfo, s_symbolIntern( dataName ), X_OO_MSG_CLASSDATA );
   if( method == NULL || !s_classSetOwnClassData( classInfo, dataIndex, initialValue ) )
      return 0;

   method->dataIndex = dataIndex;

   return 1;
}

/* The type after AS of a DATA or CLASSDATA */
int x_clsSetDataType( X_CLASS_HANDLE classHandle, const char *dataName, X_OO_VALUE_TYPE valueType )
{
   X_CLASS *classInfo = s_classOpen( classHandle );
   X_METHOD *method;

   if( classInfo == NULL || dataName == NULL || valueType > X_OO_VALUE_HASH )
      return 0;

   method = s_classFindOwnMethodByName( classInfo, dataName );
   if( method == NULL || ( method->kind != X_OO_MSG_DATA && method->kind != X_OO_MSG_CLASSDATA ) )
      return 0;

   method->valueType = valueType;

   return 1;
}

/* DESTRUCTOR */
int x_clsAddDestructor( X_CLASS_HANDLE classHandle, X_METHODPTR function )
{
   X_CLASS *classInfo = s_classOpen( classHandle );
   X_METHOD *method;

   if( classInfo == NULL || function == NULL )
      return 0;

   method = s_classAddMethod( classInfo, s_symbolIntern( "__DESTRUCTOR" ), X_OO_MSG_DESTRUCTOR );
   if( method == NULL )
      return 0;

   method->function = function;
   classInfo->destructor = method;
   classInfo->hasDestructor = 1;

   return 1;
}

/* OPERATOR */
int x_clsAddOperator( X_CLASS_HANDLE classHandle, X_OO_OPERATOR operation, X_METHODPTR function )
{
   X_CLASS *classInfo = s_classOpen( classHandle );
   X_METHOD *method;

   if( classInfo == NULL || ( unsigned int )operation > X_OO_OP_MAX || function == NULL )
      return 0;

   method = s_classAddMethod( classInfo, s_symbolIntern( s_operatorNames[operation] ), X_OO_MSG_METHOD );
   if( method == NULL )
      return 0;

   method->function = function;
   classInfo->ownOperators[operation] = method;

   return 1;
}

/* The scope of a message: one of EXPORTED, PROTECTED, HIDDEN (EXPORTED without one), with READONLY,
   NONVIRTUAL or SHARED (only CLASSDATA); a property is found by its name or by _name */
int x_clsSetScope( X_CLASS_HANDLE classHandle, const char *message, uint32_t scope )
{
   X_CLASS *classInfo = s_classOpen( classHandle );
   X_METHOD *method;
   uint32_t visibility = scope & ( X_OO_CLSTP_EXPORTED | X_OO_CLSTP_PROTECTED | X_OO_CLSTP_HIDDEN );

   if( classInfo == NULL || !s_nameValid( message ) )
      return 0;

   if( scope & ~( uint32_t )X_OO_CLSTP_SUPPORTED )
      return 0;

   if( visibility != 0 && ( visibility & ( visibility - 1 ) ) != 0 )
      return 0;

   if( visibility == 0 )
      scope |= X_OO_CLSTP_EXPORTED;

   method = s_classFindOwnMethodByName( classInfo, message );
   if( method == NULL )
   {
      const char *prefixes[] = { "__ACCESS_", "__ASSIGN_", "__CLSACCESS_", "__CLSASSIGN_" };
      int setter = message[0] == '_';
      const char *property = IIF( setter, message + 1, message );
      size_t i;

      for( i = IIF( setter, 1, 0 ); i < 4 && method == NULL; i += IIF( setter, 2, 1 ) )
      {
         X_SYMBOL *symbol = s_propertySymbol( prefixes[i], property, 0 );

         if( symbol != NULL )
            method = s_classFindOwnMethod( classInfo, symbol );
      }
   }

   if( method == NULL )
      return 0;

   if( ( scope & X_OO_CLSTP_SHARED ) && method->kind != X_OO_MSG_CLASSDATA )
      return 0;

   method->scope = scope;

   return 1;
}

/* FRIEND CLASS */
int x_clsAddFriendClass( X_CLASS_HANDLE classHandle, X_CLASS_HANDLE friendHandle )
{
   X_CLASS *classInfo = s_classOpen( classHandle );

   if( classInfo == NULL || x_clsGet( friendHandle ) == NULL || s_classHasFriend( classInfo, friendHandle ) )
      return 0;

   return s_classAppendHandle( &classInfo->friends, &classInfo->friendCount, &classInfo->friendCapacity,
                               friendHandle );
}

/* INHERIT: the parent must be associated and must not descend from the class */
int x_clsAddParent( X_CLASS_HANDLE classHandle, X_CLASS_HANDLE parentHandle )
{
   X_CLASS *classInfo = s_classOpen( classHandle );
   const X_CLASS *parent = x_clsGet( parentHandle );
   size_t i;

   if( classInfo == NULL || parent == NULL || !parent->associated )
      return 0;

   if( s_classHasAncestor( parent, classHandle ) )
      return 0;

   for( i = 0; i < classInfo->parentCount; ++i )
   {
      if( classInfo->parents[i] == parentHandle )
         return 0;
   }

   return s_classAppendHandle( &classInfo->parents, &classInfo->parentCount, &classInfo->parentCapacity,
                               parentHandle );
}

/* Forget what x_clsAssociate() built */
static void s_classResetAssociation( X_CLASS *classInfo )
{
   size_t i;

   for( i = 0; i < classInfo->classDataCount; ++i )
      x_itemClear( &classInfo->classData[i] );

   x_xfree( classInfo->classData );
   x_xfree( classInfo->methodTable );
   x_xfree( classInfo->lineage );
   classInfo->classData = NULL;
   classInfo->classDataCount = 0;
   classInfo->methodTable = NULL;
   classInfo->methodTableCount = 0;
   classInfo->methodTableCapacity = 0;
   classInfo->lineage = NULL;
   classInfo->lineageCount = 0;
   classInfo->lineageCapacity = 0;
   memset( classInfo->operators, 0, sizeof( classInfo->operators ) );
}

/* Remove a class that is not associated */
int x_clsDiscard( X_CLASS_HANDLE classHandle )
{
   X_CLASS *classInfo = s_classGetMutable( classHandle );
   size_t i;

   if( classInfo == NULL || classInfo->associated )
      return 0;

   s_releasing = 1;
   s_classes[classHandle - 1] = NULL;
   s_classResetAssociation( classInfo );

   for( i = 0; i < classInfo->ownClassDataCount; ++i )
      x_itemClear( &classInfo->ownClassData[i] );

   for( i = 0; i < classInfo->methodCount; ++i )
      x_itemClear( &classInfo->methods[i].inlineBlock );

   x_xfree( classInfo->ownClassData );
   x_xfree( classInfo->methods );
   x_xfree( classInfo->friends );
   x_xfree( classInfo->parents );
   x_xfree( classInfo );
   s_releasing = 0;

   return 1;
}

/* 1 when a method of a class has its code in [start, start + size): a DLL module that defined the class */
int x_clsUsesCode( const void *start, size_t size )
{
   const uintptr_t first = ( uintptr_t )start;
   size_t i;
   size_t m;

   for( i = 0; i < s_classCount; ++i )
   {
      const X_CLASS *classInfo = s_classes[i];

      if( classInfo == NULL )
         continue;

      for( m = 0; m < classInfo->methodCount; ++m )
      {
         const uintptr_t code = ( uintptr_t )classInfo->methods[m].function;

         if( code >= first && code - first < size )
            return 1;
      }
   }

   return 0;
}

/* Finish a class: lineage, class data, method table and operators; a class that fails is discarded */
int x_clsAssociate( X_CLASS_HANDLE classHandle )
{
   X_CLASS *classInfo = s_classGetMutable( classHandle );
   const X_RESOLVED_METHOD *onError;
   size_t instanceDataCount = 0;
   size_t i;

   if( classInfo == NULL )
      return 0;

   if( classInfo->associated )
      return 1;

   s_classResetAssociation( classInfo );

   if( !s_classBuildLineage( classInfo ) )
      goto failed;

   for( i = 0; i < classInfo->lineageCount; ++i )
   {
      const X_CLASS *owner = x_clsGet( classInfo->lineage[i] );

      if( instanceDataCount > SIZE_MAX - owner->ownDataCount )
         goto failed;

      instanceDataCount += owner->ownDataCount;
   }

   classInfo->instanceDataCount = instanceDataCount;
   if( instanceDataCount > ( size_t )INT64_MAX || instanceDataCount > SIZE_MAX / sizeof( X_ITEM ) )
      goto failed;

   if( !s_classBuildClassData( classInfo ) )
      goto failed;

   classInfo->hasDestructor = classInfo->destructor != NULL;
   for( i = 0; i < classInfo->lineageCount; ++i )
   {
      if( x_clsGet( classInfo->lineage[i] )->destructor != NULL )
      {
         classInfo->hasDestructor = 1;
         break;
      }
   }

   if( !s_classBuildMethodTable( classInfo ) )
      goto failed;

   onError = s_classFindMethod( classInfo, "__ONERROR" );
   classInfo->hasOnError = onError != NULL && onError->method->kind == X_OO_MSG_ONERROR;

   if( !s_classValidateDelegates( classInfo ) )
      goto failed;

   s_classBuildOperators( classInfo );
   classInfo->associated = 1;

   return 1;

failed:
   x_clsDiscard( classHandle );

   return 0;
}

/* Is the class finished? */
int x_clsIsAssociated( X_CLASS_HANDLE classHandle )
{
   const X_CLASS *classInfo = x_clsGet( classHandle );

   return classInfo != NULL && classInfo->associated;
}

/* The class of a handle; NULL when there is none */
const X_CLASS *x_clsGet( X_CLASS_HANDLE classHandle )
{
   if( !s_initialized || classHandle == 0 || classHandle > s_classCount )
      return NULL;

   return s_classes[classHandle - 1];
}

/* ---------------------------------------------------------------------------------------------
   Base classes registered on the first use
   --------------------------------------------------------------------------------------------- */

/* The classes being registered now, to find a cycle; a class that failed is discarded */
typedef struct X_CLASS_REQUIRE
{
   const char *name;
   struct X_CLASS_REQUIRE *previous;
   int discard;
} X_CLASS_REQUIRE;

static X_CLASS_REQUIRE *s_classRequire;

/* The end of a registration: a class whose factory did not finish is discarded */
static void s_requireRestore( X_CLASS_REQUIRE *frame )
{
   s_classRequire = frame->previous;
   if( frame->discard )
      x_clsDiscard( x_clsFindClass( frame->name, NULL ) );
}

/* The class of a name; the factory function creates it when it is not associated yet */
X_CLASS_HANDLE x_clsRequire( const char *className, X_FUNCPTR factory )
{
   X_CLASS_HANDLE handle;
   X_CLASS_REQUIRE *previous;
   X_ITEM result __attribute__( ( cleanup( x_itemClear ) ) ) = X_ITEM_NIL_INIT;

   if( className == NULL || *className == '\0' || factory == NULL )
   {
      x_errorRaiseNamed( &result, 3000, "base class registration" );
      return 0;
   }

   handle = x_clsFindClass( className, NULL );
   if( handle != 0 && x_clsIsAssociated( handle ) )
      return handle;

   for( previous = s_classRequire; previous != NULL; previous = previous->previous )
   {
      if( x_stricmp( previous->name, className ) == 0 )
      {
         x_errorRaiseNamed( &result, 3000, "cyclic class registration" );
         return 0;
      }
   }

   {
      X_CLASS_REQUIRE frame __attribute__( ( cleanup( s_requireRestore ) ) ) = { className, s_classRequire, 0 };
      X_CALL_CTX call = { NULL, 0, &result, NULL, NULL };

      frame.discard = handle == 0;
      s_classRequire = &frame;
      factory( &call );
      frame.discard = 0;
   }

   x_itemClear( &result );
   handle = x_clsFindClass( className, NULL );
   if( !x_clsIsAssociated( handle ) )
   {
      x_clsDiscard( handle );
      x_errorRaiseNamed( &result, 3000, className );
      return 0;
   }

   return handle;
}

/* A new object for a class function; inside the registration of the same class the result stays NIL */
int x_clsFactoryCreate( X_CALL_CTX *ctx, X_ITEM *result, X_CLASS_HANDLE classHandle )
{
   const X_CLASS *classInfo = x_clsGet( classHandle );

   if( classInfo == NULL || !classInfo->associated )
      return 0;

   if( s_classRequire != NULL && x_stricmp( s_classRequire->name, classInfo->name->name ) == 0 )
   {
      x_itemClear( result );
      return 1;
   }

   return x_objCreate( ctx, result, classHandle );
}

/* The end of a class function: associate the class and return a new object */
int x_clsReturn( X_CALL_CTX *ctx, X_CLASS_HANDLE classHandle )
{
   if( !x_clsAssociate( classHandle ) || !x_clsFactoryCreate( ctx, ctx->result, classHandle ) )
      return x_errorRaiseNamed( ctx->result, 3000, "class factory" );

   return 1;
}

/* ---------------------------------------------------------------------------------------------
   Class information
   --------------------------------------------------------------------------------------------- */

/* The handle of a class by its name or by its class function name; 0 when there is none */
X_CLASS_HANDLE x_clsFindClass( const char *className, const char *classFunction )
{
   size_t i;

   if( !s_initialized )
      return 0;

   for( i = 0; i < s_classCount; ++i )
   {
      const X_CLASS *classInfo = s_classes[i];
      const X_SYMBOL *factory;

      if( classInfo == NULL )
         continue;

      if( className != NULL && classInfo->name != NULL && x_stricmp( classInfo->name->name, className ) == 0 )
         return classInfo->handle;

      factory = classInfo->factoryName;
      if( classFunction != NULL && factory != NULL && x_stricmp( factory->name, classFunction ) == 0 )
         return classInfo->handle;
   }

   return 0;
}

/* The name of a class */
const char *x_clsName( X_CLASS_HANDLE classHandle )
{
   const X_CLASS *classInfo = x_clsGet( classHandle );

   return IIF( classInfo != NULL && classInfo->name != NULL, classInfo->name->name, NULL );
}

/* The name of the class function of a class */
const char *x_clsFuncName( X_CLASS_HANDLE classHandle )
{
   const X_CLASS *classInfo = x_clsGet( classHandle );

   return IIF( classInfo != NULL && classInfo->factoryName != NULL, classInfo->factoryName->name, NULL );
}

/* The message of a method handle */
const char *x_clsMethodName( X_CLASS_HANDLE classHandle, X_METHOD_HANDLE methodHandle )
{
   const X_CLASS *classInfo = x_clsGet( classHandle );
   size_t i;

   if( classInfo == NULL )
      return NULL;

   for( i = 0; i < classInfo->methodCount; ++i )
   {
      const X_METHOD *method = &classInfo->methods[i];

      if( method->handle == methodHandle )
         return method->message->name;
   }

   return NULL;
}

/* Is a class of this name a parent of the class? */
int x_clsIsParent( X_CLASS_HANDLE classHandle, const char *parentName )
{
   const X_CLASS *classInfo = x_clsGet( classHandle );

   return classInfo != NULL && parentName != NULL && s_classIsParent( classInfo, parentName );
}

/* Does the class or one of its ancestors have a destructor? */
int x_clsHasDestructor( X_CLASS_HANDLE classHandle )
{
   const X_CLASS *classInfo = x_clsGet( classHandle );

   return classInfo != NULL && classInfo->hasDestructor;
}

/* ---------------------------------------------------------------------------------------------
   Objects
   --------------------------------------------------------------------------------------------- */

/* A message of the class of an object */
const X_RESOLVED_METHOD *x_objFindMethod( const X_ITEM *object, const char *message )
{
   const X_CLASS *classInfo = x_clsGet( x_objGetClass( object ) );

   if( classInfo == NULL || message == NULL )
      return NULL;

   return s_classFindMethod( classInfo, message );
}

/* The class handle of an object; 0 for any other value */
X_CLASS_HANDLE x_objGetClass( const X_ITEM *item )
{
   return x_arrayClass( item );
}

/* Make an array an object of a class */
int x_objSetClass( X_ITEM *item, X_CLASS_HANDLE classHandle )
{
   if( x_clsGet( classHandle ) == NULL )
      return 0;

   return x_arraySetClass( item, classHandle );
}

/* A new object; the initial values of DATA come from the __XDEV_INITDATA method of each class of the lineage */
int x_objCreate( X_CALL_CTX *ctx, X_ITEM *result, X_CLASS_HANDLE classHandle )
{
   const X_CLASS *classInfo = x_clsGet( classHandle );
   size_t i;

   if( classInfo == NULL || !classInfo->associated || classInfo->instanceDataCount > ( size_t )INT64_MAX )
      return 0;

   if( !x_arrayNew( result, ( int64_t )classInfo->instanceDataCount ) )
      return 0;

   if( !x_arraySetClass( result, classHandle ) )
      return 0;

   for( i = 0; i < classInfo->lineageCount; ++i )
   {
      X_CLASS *owner = ( X_CLASS * )x_clsGet( classInfo->lineage[i] );
      X_METHOD *initializer = s_classFindOwnMethodByName( owner, "__XDEV_INITDATA" );
      X_RESOLVED_METHOD resolved = { initializer, 0 };
      X_ITEM value __attribute__( ( cleanup( x_itemClear ) ) ) = X_ITEM_NIL_INIT;
      int success;

      if( initializer == NULL )
         continue;

      success = s_objectSendMethod( ctx, &value, result, &resolved, owner->handle, 0, NULL, 0, 0, NULL );
      x_itemClear( &value );
      if( !success )
      {
         x_itemClear( result );
         return 0;
      }
   }

   return 1;
}

/* Read (no argument) or assign (one argument) an instance variable at dataIndex of the object */
static int s_dataSend( X_CALL_CTX *ctx, X_ITEM *result, X_ITEM *object, size_t dataIndex, X_OO_VALUE_TYPE valueType,
                       int32_t argc, const X_ARG *args )
{
   X_ITEM value __attribute__( ( cleanup( x_itemClear ) ) ) = X_ITEM_NIL_INIT;

   if( argc == 0 )
      return x_arrayGet( object, dataIndex, result );

   if( argc != 1 )
   {
      x_itemClear( result );
      return 0;
   }

   x_itemCopy( &value, args[0].value );
   if( !s_dataConvert( result, valueType, &value ) )
   {
      x_itemClear( &value );
      x_itemClear( result );
      return 0;
   }

   if( !x_itemAssign( ctx, x_arrayGetItemPtr( object, dataIndex ), &value ) )
   {
      x_itemClear( &value );
      x_itemClear( result );
      return 0;
   }

   x_arrayGet( object, dataIndex, result );
   x_itemClear( &value );

   return 1;
}

/* Is the message a class message: CLASSMETHOD, class ACCESS or class ASSIGN? */
static int s_isClassMessage( X_OO_MESSAGE_TYPE kind )
{
   return kind == X_OO_MSG_CLSMTHD || kind == X_OO_MSG_CLSACCESS || kind == X_OO_MSG_CLSASSIGN;
}

/* Is the message a function or block to call: METHOD, ON ERROR, INLINE, ACCESS, ASSIGN, or the destructor? */
static int s_isCallable( const X_METHOD *method, int destructorCall )
{
   switch( method->kind )
   {
   case X_OO_MSG_METHOD:
   case X_OO_MSG_ONERROR:
   case X_OO_MSG_INLINE:
   case X_OO_MSG_ACCESS:
   case X_OO_MSG_ASSIGN:
      return 1;

   case X_OO_MSG_DESTRUCTOR:
      return destructorCall;

   default:
      return 0;
   }
}

/* DELEGATE: the message goes on to the object that the object message returns */
static int s_delegateSend( X_ITEM *result, X_ITEM *object, const X_METHOD *method, X_CLASS_HANDLE lookupClass,
                           int32_t argc, const X_ARG *args, const char *attemptedMessage )
{
   X_METHOD_CONTEXT *previous __attribute__( ( cleanup( s_methodRestore ) ) ) = s_methodContext;
   X_ITEM delegateObject __attribute__( ( cleanup( x_itemClear ) ) ) = X_ITEM_NIL_INIT;
   const X_CLASS *classInfo = x_clsGet( lookupClass );
   const X_RESOLVED_METHOD *objectMethod = NULL;
   const X_METHOD_CONTEXT *frame;
   X_METHOD_CONTEXT context;
   X_CALL_CTX call;
   int success = 0;

   /* The same delegation already runs for this object: a cycle */
   for( frame = s_methodContext; frame != NULL; frame = frame->previous )
   {
      if( frame->self == NULL || x_arrayId( frame->self ) != x_arrayId( object ) )
         continue;

      if( frame->definingClass == method->definingClass && frame->method == method->handle )
         return x_errorRaiseNamed( result, 3000, method->message->name );
   }

   context.self = object;
   context.objectClass = x_objGetClass( object );
   context.lookupClass = lookupClass;
   context.definingClass = method->definingClass;
   context.method = method->handle;
   context.attemptedMessage = attemptedMessage;
   context.previous = previous;
   s_methodContext = &context;

   call.args = args;
   call.argc = argc;
   call.result = result;
   call.self = object;
   call.methodContext = &context;

   if( classInfo != NULL )
      objectMethod = s_classFindDelegateObject( classInfo, method->delegateObject->name );

   if( objectMethod != NULL )
      success = s_objectSendMethod( &call, &delegateObject, object, objectMethod, lookupClass, 0, NULL, 1, 0, NULL );

   if( success )
      success = x_objSend( &call, result, &delegateObject, method->delegateMessage->name, argc, args );
   else
      x_itemClear( result );

   return success;
}

/* Send a found method to an object: checks of scope, arguments and READONLY, then the variable, the delegation
   or the call */
static int s_objectSendMethod( X_CALL_CTX *ctx, X_ITEM *result, X_ITEM *object,
                               const X_RESOLVED_METHOD *resolvedMethod, X_CLASS_HANDLE lookupClass, int32_t argc,
                               const X_ARG *args, int scopeCheck, int destructorCall, const char *attemptedMessage )
{
   const X_METHOD *method = resolvedMethod->method;
   const X_CLASS *actual = x_clsGet( x_objGetClass( object ) );

   if( scopeCheck && !s_classCheckAccess( ctx, result, method ) )
      return 0;

   if( !s_methodArity( result, method, argc ) )
      return 0;

   if( !s_classCheckReadonly( ctx, result, actual, method, argc ) )
      return 0;

   if( method->kind == X_OO_MSG_DATA )
   {
      size_t dataIndex = s_classDataOffset( actual, method->definingClass ) + method->dataIndex;

      return s_dataSend( ctx, result, object, dataIndex, method->valueType, argc, args );
   }

   if( method->kind == X_OO_MSG_CLASSDATA )
      return s_classDataSend( ctx, result, actual, method, argc, args );

   if( method->kind == X_OO_MSG_DELEGATE )
      return s_delegateSend( result, object, method, lookupClass, argc, args, attemptedMessage );

   if( s_isCallable( method, destructorCall ) )
   {
      X_METHOD_CONTEXT *previous __attribute__( ( cleanup( s_methodRestore ) ) ) = s_methodContext;
      X_METHOD_CONTEXT context;
      X_CALL_CTX call;

      context.self = object;
      context.objectClass = x_objGetClass( object );
      context.lookupClass = lookupClass;
      context.definingClass = method->definingClass;
      context.method = method->handle;
      context.attemptedMessage = attemptedMessage;
      context.previous = previous;
      s_methodContext = &context;

      call = ( X_CALL_CTX ){ args, argc, result, object, &context };
      if( method->kind == X_OO_MSG_INLINE )
         x_evalBlock( &call, result, &method->inlineBlock, argc, args );
      else if( destructorCall )
         x_callCleanup( method->function, &call );
      else
         method->function( &call );

      return 1;
   }

   return x_errorRaiseNamed( result, IIF( method->kind == X_OO_MSG_VIRTUAL, 1004, 3000 ), method->message->name );
}

/* The start of a method written in xDev: self, the frame and the defining class from the lineage position owner */
void x_objMethodEnter( X_CALL_CTX *ctx, X_METHOD_CONTEXT *frame, X_ITEM *self, const X_ITEM *object, uint32_t owner,
                       X_METHOD_HANDLE method )
{
   x_itemCopy( self, object );
   frame->self = self;
   frame->objectClass = x_objGetClass( self );
   frame->lookupClass = frame->objectClass;
   frame->definingClass = x_clsGet( frame->objectClass )->lineage[owner];
   frame->method = method;
   frame->attemptedMessage = NULL;
   frame->previous = s_methodContext;
   ctx->self = self;
   ctx->methodContext = frame;
   s_methodContext = frame;
}

/* The end of a method written in xDev */
void x_objMethodLeave( X_METHOD_CONTEXT *frame )
{
   s_methodContext = frame->previous;
   x_itemClear( frame->self );
}

/* Send a found class message: CLASSDATA, CLASSMETHOD, class ACCESS or class ASSIGN */
static int s_classSendMethod( X_CALL_CTX *ctx, X_ITEM *result, const X_CLASS *classInfo,
                              const X_RESOLVED_METHOD *resolvedMethod, X_CLASS_HANDLE lookupClass, int32_t argc,
                              const X_ARG *args )
{
   const X_METHOD *method = resolvedMethod->method;
   X_METHOD_CONTEXT *previous __attribute__( ( cleanup( s_methodRestore ) ) ) = s_methodContext;
   X_METHOD_CONTEXT context;
   X_CALL_CTX call;

   if( !s_classCheckAccess( ctx, result, method ) || !s_classCheckReadonly( ctx, result, classInfo, method, argc ) )
      return 0;

   if( !s_methodArity( result, method, argc ) )
      return 0;

   if( method->kind == X_OO_MSG_CLASSDATA )
      return s_classDataSend( ctx, result, classInfo, method, argc, args );

   if( !s_isClassMessage( method->kind ) )
   {
      x_itemClear( result );
      return 0;
   }

   context.self = NULL;
   context.objectClass = classInfo->handle;
   context.lookupClass = lookupClass;
   context.definingClass = method->definingClass;
   context.method = method->handle;
   context.attemptedMessage = NULL;
   context.previous = previous;
   s_methodContext = &context;

   call = ( X_CALL_CTX ){ args, argc, result, NULL, &context };
   method->function( &call );

   return 1;
}

/* A message of the parents of the class of the method that runs now; *lookupClass gets the parent */
static const X_RESOLVED_METHOD *s_classFindSuperMethod( const X_METHOD_CONTEXT *context, const char *message,
                                                       int32_t argc, X_CLASS_HANDLE *lookupClass )
{
   const X_CLASS *classInfo = x_clsGet( context->definingClass );
   size_t i;

   for( i = 0; i < classInfo->parentCount; ++i )
   {
      const X_CLASS *parent = x_clsGet( classInfo->parents[i] );
      const X_RESOLVED_METHOD *method = s_classResolveMessage( parent, message, argc, context->self == NULL );

      if( method != NULL )
      {
         *lookupClass = parent->handle;
         return method;
      }
   }

   return NULL;
}

/* A message to the class of a class method that runs now (::message in a CLASSMETHOD) */
int x_clsSendSelf( X_CALL_CTX *ctx, X_ITEM *result, const char *message, int32_t argc, const X_ARG *args )
{
   const X_METHOD_CONTEXT *context = s_currentMethodContext( ctx );

   if( !s_messageArguments( result, message, argc, args ) )
      return 0;

   if( context == NULL || context->self != NULL )
      return x_errorRaiseNamed( result, 3000, message );

   return x_clsSend( ctx, result, context->objectClass, message, argc, args );
}

/* A message to a class */
int x_clsSend( X_CALL_CTX *ctx, X_ITEM *result, X_CLASS_HANDLE classHandle, const char *message, int32_t argc,
               const X_ARG *args )
{
   const X_CLASS *classInfo = x_clsGet( classHandle );
   const X_RESOLVED_METHOD *method;
   int success;

   if( !s_messageArguments( result, message, argc, args ) )
      return 0;

   if( classInfo == NULL || !classInfo->associated )
      return x_errorRaiseNamed( result, 3000, message );

   method = s_classResolveCall( ctx, classInfo, message, argc, 1 );
   if( method == NULL )
      return x_errorRaiseNamed( result, IIF( s_classPropertyExists( classInfo, message, 1 ), 3003, 1004 ), message );

   success = s_classSendMethod( ctx, result, classInfo, method, classHandle, argc, args );
   if( !success )
      x_errorRaiseNamed( result, 3000, message );

   return success;
}

/* A message to an object; an unknown message goes to ON ERROR when the class has one */
int x_objSend( X_CALL_CTX *ctx, X_ITEM *result, X_ITEM *object, const char *message, int32_t argc, const X_ARG *args )
{
   const X_RESOLVED_METHOD *method;
   const X_CLASS *classInfo = IIF( object != NULL, x_clsGet( x_objGetClass( object ) ), NULL );
   const char *attempted = NULL;
   X_ITEM receiver __attribute__( ( cleanup( x_itemClear ) ) ) = X_ITEM_NIL_INIT;
   X_CLASS_HANDLE lookup;
   int scopeCheck;
   int success;

   if( !s_messageArguments( result, message, argc, args ) )
      return 0;

   if( classInfo == NULL )
      return x_errorRaiseNamed( result, 1004, message );

   if( !classInfo->associated )
      return x_errorRaiseNamed( result, 3000, message );

   method = s_classResolveCall( ctx, classInfo, message, argc, 0 );
   if( method == NULL && s_classPropertyExists( classInfo, message, 0 ) )
      return x_errorRaiseNamed( result, 3003, message );

   if( method == NULL && classInfo->hasOnError && !s_onErrorActive( object, message ) )
   {
      method = s_classFindMethod( classInfo, "__ONERROR" );
      attempted = IIF( method != NULL, message, NULL );
   }

   if( method == NULL )
      return x_errorRaiseNamed( result, 1004, message );

   x_itemCopy( &receiver, object );
   lookup = x_objGetClass( &receiver );
   scopeCheck = attempted == NULL;
   if( s_isClassMessage( method->method->kind ) )
      success = s_classSendMethod( ctx, result, classInfo, method, classInfo->handle, argc, args );
   else
      success = s_objectSendMethod( ctx, result, &receiver, method, lookup, argc, args, scopeCheck, 0, attempted );

   if( !success )
      x_errorRaiseNamed( result, 3000, message );

   x_itemClear( &receiver );

   return success;
}

/* A C function run as a method of definingClass, without a message: self and the data of that class as in its
   methods; definingClass is in the lineage of the class of the object */
void x_objCall( X_ITEM *result, X_ITEM *object, X_CLASS_HANDLE definingClass, X_METHODPTR function, int32_t argc,
                const X_ARG *args )
{
   X_METHOD_CONTEXT *previous __attribute__( ( cleanup( s_methodRestore ) ) ) = s_methodContext;
   X_ITEM receiver __attribute__( ( cleanup( x_itemClear ) ) ) = X_ITEM_NIL_INIT;
   X_METHOD_CONTEXT context;
   X_CALL_CTX call;

   x_itemCopy( &receiver, object );
   context.self = &receiver;
   context.objectClass = x_objGetClass( &receiver );
   context.lookupClass = context.objectClass;
   context.definingClass = definingClass;
   context.method = 0;
   context.attemptedMessage = NULL;
   context.previous = previous;
   s_methodContext = &context;

   call = ( X_CALL_CTX ){ args, argc, result, &receiver, &context };
   function( &call );
}

/* Is the class or one of its ancestors of this name? */
static int s_classHasAncestorNamed( const X_CLASS *classInfo, const char *className )
{
   size_t i;

   if( classInfo->name != NULL && x_stricmp( classInfo->name->name, className ) == 0 )
      return 1;

   for( i = 0; i < classInfo->parentCount; ++i )
   {
      const X_CLASS *parent = x_clsGet( classInfo->parents[i] );

      if( parent != NULL && s_classHasAncestorNamed( parent, className ) )
         return 1;
   }

   return 0;
}

/* Is the value an object of the class or of a subclass? NIL fits every class, "" every object */
int x_objIsKindOf( const X_ITEM *item, const char *className )
{
   const X_CLASS *classInfo;

   if( X_ITEM_TYPERAW( item ) == X_IT_NIL )
      return 1;

   classInfo = x_clsGet( x_objGetClass( item ) );
   if( classInfo == NULL )
      return 0;

   return *className == '\0' || s_classHasAncestorNamed( classInfo, className );
}

/* A message that compiled code expects to be an instance variable; the cache keeps where the variable is
   for the class of the object and the sending class */
int x_objSendData( X_CALL_CTX *ctx, X_ITEM *result, X_ITEM *object, const char *message, int32_t argc,
                   const X_ARG *args, X_DATA_CACHE *cache )
{
   const X_METHOD_CONTEXT *sender = s_currentMethodContext( ctx );
   X_CLASS_HANDLE objectClass = x_objGetClass( object );
   X_CLASS_HANDLE senderClass = IIF( sender != NULL, sender->definingClass, 0 );
   int success;

   if( objectClass == 0 || argc > 1 )
      return x_objSend( ctx, result, object, message, argc, args );

   if( objectClass != cache->objectClass || senderClass != cache->senderClass )
   {
      const X_CLASS *classInfo = x_clsGet( objectClass );
      const X_RESOLVED_METHOD *resolved = NULL;
      const X_METHOD *method;

      if( classInfo != NULL && classInfo->associated )
         resolved = s_classResolveCall( ctx, classInfo, message, argc, 0 );

      cache->dataIndex = 0;
      method = IIF( resolved != NULL, resolved->method, NULL );
      if( method != NULL && method->kind == X_OO_MSG_DATA && s_classCanAccess( ctx, method ) )
      {
         if( s_classAllowsWrite( ctx, classInfo, method, argc ) )
         {
            cache->dataIndex = s_classDataOffset( classInfo, method->definingClass ) + method->dataIndex;
            cache->valueType = method->valueType;
         }
      }

      cache->objectClass = objectClass;
      cache->senderClass = senderClass;
   }

   if( cache->dataIndex == 0 )
      return x_objSend( ctx, result, object, message, argc, args );

   success = s_dataSend( ctx, result, object, cache->dataIndex, cache->valueType, argc, args );
   if( !success )
      x_errorRaiseNamed( result, 3000, message );

   return success;
}

/* ::Super:message, a message to the parents of the class of the method that runs now */
int x_objSendSuper( X_CALL_CTX *ctx, X_ITEM *result, const char *message, int32_t argc, const X_ARG *args )
{
   const X_METHOD_CONTEXT *context = s_currentMethodContext( ctx );
   const X_RESOLVED_METHOD *method;
   X_CLASS_HANDLE lookupClass;
   int success;

   if( !s_messageArguments( result, message, argc, args ) )
      return 0;

   if( context == NULL )
      return x_errorRaiseNamed( result, 3000, message );

   method = s_classFindSuperMethod( context, message, argc, &lookupClass );
   if( method == NULL )
   {
      const X_CLASS *owner = x_clsGet( context->definingClass );
      size_t i;

      for( i = 0; i < owner->parentCount; ++i )
      {
         if( s_classPropertyExists( x_clsGet( owner->parents[i] ), message, context->self == NULL ) )
            return x_errorRaiseNamed( result, 3003, message );
      }

      return x_errorRaiseNamed( result, 1004, message );
   }

   if( context->self == NULL || s_isClassMessage( method->method->kind ) )
      success = s_classSendMethod( ctx, result, x_clsGet( context->objectClass ), method, lookupClass, argc, args );
   else
      success = s_objectSendMethod( ctx, result, context->self, method, lookupClass, argc, args, 1, 0, NULL );

   if( !success )
      x_errorRaiseNamed( result, 3000, message );

   return success;
}

/* Call an operator overloaded by the class of object with up to two arguments */
int x_objOperatorCall( X_CALL_CTX *ctx, X_OO_OPERATOR operation, X_ITEM *result, X_ITEM *object,
                       const X_ITEM *argument1, const X_ITEM *argument2 )
{
   const X_CLASS *classInfo = IIF( object != NULL, x_clsGet( x_objGetClass( object ) ), NULL );
   X_ITEM receiver __attribute__( ( cleanup( x_itemClear ) ) ) = X_ITEM_NIL_INIT;
   X_ARG arguments[2];
   const X_ARG *args = NULL;
   int32_t argc = 0;
   const X_RESOLVED_METHOD *operatorMethod;
   int success;

   if( result == NULL )
      return 0;

   if( classInfo == NULL || !classInfo->associated || ( unsigned int )operation > X_OO_OP_MAX )
   {
      x_itemClear( result );
      return 0;
   }

   if( classInfo->operators[operation] == NULL || ( argument2 != NULL && argument1 == NULL ) )
   {
      x_itemClear( result );
      return 0;
   }

   if( argument1 != NULL )
   {
      arguments[0].value = argument1;
      arguments[0].byReference = 0;
      args = arguments;
      argc = 1;
   }

   if( argument2 != NULL )
   {
      arguments[1].value = argument2;
      arguments[1].byReference = 0;
      argc = 2;
   }

   x_itemCopy( &receiver, object );
   operatorMethod = classInfo->operators[operation];
   success = s_objectSendMethod( ctx, result, &receiver, operatorMethod, classInfo->handle, argc, args, 1, 0, NULL );
   if( !success )
      x_errorRaiseNamed( result, 3000, s_operatorNames[operation] );

   x_itemClear( &receiver );

   return success;
}

/* Run the destructors of an object, the class of the object first, at most once */
void x_objDestructorCall( X_ITEM *object )
{
   X_CALL_CTX ctx = { 0 };
   const X_CLASS *classInfo = IIF( object != NULL, x_clsGet( x_objGetClass( object ) ), NULL );
   size_t i;

   if( classInfo == NULL || !classInfo->hasDestructor || !x_gcBeginDestructor( object ) )
      return;

   X_ITEM anchor __attribute__( ( cleanup( s_destructorLeave ) ) ) = X_ITEM_NIL_INIT;

   x_itemCopy( &anchor, object );
   for( i = classInfo->lineageCount; i > 0; --i )
   {
      const X_CLASS *owner = x_clsGet( classInfo->lineage[i - 1] );
      X_ITEM receiver __attribute__( ( cleanup( x_itemClear ) ) ) = X_ITEM_NIL_INIT;
      X_ITEM result __attribute__( ( cleanup( x_itemClear ) ) ) = X_ITEM_NIL_INIT;
      X_RESOLVED_METHOD method;

      if( owner->destructor == NULL )
         continue;

      method.method = owner->destructor;
      method.dataIndex = 0;
      x_itemCopy( &receiver, &anchor );
      s_objectSendMethod( &ctx, &result, &receiver, &method, owner->handle, 0, NULL, 0, 1, NULL );
   }
}

/* ON ERROR: the message that was not found and its arguments */
int x_objOnErrorInfo( X_CALL_CTX *ctx, X_ITEM *result, X_ITEM *message, X_ITEM *arguments )
{
   const X_METHOD_CONTEXT *context = s_currentMethodContext( ctx );
   int32_t i;

   if( result == NULL || message == NULL || arguments == NULL )
      return 0;

   if( context == NULL || context->attemptedMessage == NULL )
      return x_errorRaiseNamed( result, 3000, "ONERROR context" );

   x_itemPutStringBuffer( message, x_strdup( context->attemptedMessage ), strlen( context->attemptedMessage ) );
   if( !x_arrayNew( arguments, ctx->argc ) )
      return x_errorRaiseNamed( result, 3000, "ONERROR arguments" );

   for( i = 0; i < ctx->argc; ++i )
   {
      if( !x_arraySet( arguments, ( size_t )i + 1, ctx->args[i].value ) )
         return x_errorRaiseNamed( result, 3000, "ONERROR arguments" );
   }

   return 1;
}

/* Evaluate a block made inside a method, with the method context it was made in */
void x_objEvalClosure( X_BLOCKFUNC function, X_ITEM *result, int32_t argc, const X_ARG *args,
                       X_ITEM *const *env, const X_METHOD_CONTEXT *saved, X_ITEM *self )
{
   X_METHOD_CONTEXT context = *saved;
   X_METHOD_CONTEXT *previous __attribute__( ( cleanup( s_methodRestore ) ) ) = s_methodContext;
   X_CALL_CTX call = { args, argc, result, NULL, NULL };

   if( context.definingClass != 0 )
   {
      context.self = IIF( self->nType == X_IT_NIL, NULL, self );
      context.previous = previous;
      context.attemptedMessage = NULL;
      s_methodContext = &context;
      call.self = context.self;
      call.methodContext = &context;
   }

   function( &call, env );
}

/* The method context of the call */
const X_METHOD_CONTEXT *x_objMethodContext( X_CALL_CTX *ctx )
{
   return s_currentMethodContext( ctx );
}

/* The own instance variable index (from 1) of the class of the method that runs now; NULL outside a method */
X_ITEM *x_objDataSlot( X_CALL_CTX *ctx, size_t index )
{
   const X_METHOD_CONTEXT *context = s_currentMethodContext( ctx );
   const X_CLASS *owner;
   const X_CLASS *actual;

   if( context == NULL || context->self == NULL || x_stackSelfItem( ctx ) != context->self )
      return NULL;

   owner = x_clsGet( context->definingClass );
   actual = x_clsGet( context->objectClass );
   if( index == 0 || index > owner->ownDataCount )
      return NULL;

   return x_arrayGetItemPtr( context->self, s_classDataOffset( actual, owner->handle ) + index );
}

/* The class name of an object */
const char *x_objGetClsName( const X_ITEM *object )
{
   return x_clsName( x_objGetClass( object ) );
}

/* Can the method that runs now send this message to the object? */
int x_objHasMsg( X_CALL_CTX *ctx, const X_ITEM *object, const char *message )
{
   const X_CLASS *classInfo = x_clsGet( x_objGetClass( object ) );
   int32_t argc;

   if( classInfo == NULL || !classInfo->associated || message == NULL || *message == '\0' )
      return 0;

   for( argc = IIF( message[0] == '_' && message[1] != '_', 1, 0 ); argc <= 1; ++argc )
   {
      const X_RESOLVED_METHOD *resolved = s_classResolveCall( ctx, classInfo, message, argc, 0 );
      const X_METHOD *method;

      if( resolved == NULL )
         continue;

      method = resolved->method;
      if( method->kind == X_OO_MSG_VIRTUAL || method->kind == X_OO_MSG_DESTRUCTOR )
         continue;

      if( method->kind == X_OO_MSG_ONERROR || !s_classCanAccess( ctx, method ) )
         continue;

      if( !s_methodAcceptsArity( method, argc ) || !s_classAllowsWrite( ctx, classInfo, method, argc ) )
         continue;

      return 1;
   }

   return 0;
}

/* Does the class of the object overload the operator? */
int x_objHasOperator( const X_ITEM *object, X_OO_OPERATOR operation )
{
   const X_CLASS *classInfo = x_clsGet( x_objGetClass( object ) );

   return classInfo != NULL && ( unsigned int )operation <= X_OO_OP_MAX && classInfo->operators[operation] != NULL;
}
