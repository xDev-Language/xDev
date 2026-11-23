/*
 * Copyright (c) 2024-2026 xDev-Language
 *
 * Library...: xDev Backend
 * File......: llvm.cpp
 *
 */

#include "internal.h"
#include "platform.h"
#include "classes.h"

#include "llvm.hpp"
#include "constant.hpp"
#include "native.hpp"
#include "interface.hpp"
#include "operators.h"
#include "statements.h"
#include "version.h"

#include <algorithm>
#include <cmath>
#include <functional>
#include <unordered_set>

#include <llvm/Config/llvm-config.h>
#include <llvm/Support/SaveAndRestore.h>
#include <llvm/IR/BasicBlock.h>
#include <llvm/IR/Constants.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Intrinsics.h>
#include <llvm/IR/LegacyPassManager.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Verifier.h>
#include <llvm/MC/TargetRegistry.h>
#include <llvm/Pass.h>
#include <llvm/Passes/PassBuilder.h>
#include <llvm/Support/CodeGen.h>
#include <llvm/Support/FileSystem.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/Support/TargetSelect.h>
#include <llvm/Target/TargetMachine.h>
#include <llvm/Target/TargetOptions.h>
#include <llvm/TargetParser/Host.h>
#include <llvm/TargetParser/Triple.h>
#include <llvm/Transforms/Utils.h>
#include <llvm/Transforms/Utils/ModuleUtils.h>

#include <cctype>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <memory>
#include <optional>
#include <set>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

/* The code generator writes these runtime structures directly: X_ITEM is { i32, i64, i64 } */
static_assert( sizeof( X_ITEM ) == 24, "X_ITEM: nieoczekiwany rozmiar - sprawdz layout przed zmiana codegenu" );
static_assert( alignof( X_ITEM ) == 8, "X_ITEM: nieoczekiwane wyrownanie" );
static_assert( offsetof( X_ITEM, u ) == 8, "X_ITEM: pole 'u' nie zaczyna sie od bajtu 8" );
static_assert( sizeof( X_ARG ) == 16, "X_ARG: nieoczekiwany rozmiar" );
static_assert( alignof( X_ARG ) == 8, "X_ARG: nieoczekiwane wyrownanie" );
static_assert( offsetof( X_ARG, value ) == 0, "X_ARG: pole 'value' nie jest pierwsze" );
static_assert( offsetof( X_ARG, byReference ) == 8, "X_ARG: pole 'byReference' nie zaczyna sie od bajtu 8" );
static_assert( sizeof( X_CALL_CTX ) == 40, "X_CALL_CTX: nieoczekiwany rozmiar" );
static_assert( alignof( X_CALL_CTX ) == 8, "X_CALL_CTX: nieoczekiwane wyrownanie" );
static_assert( offsetof( X_CALL_CTX, args ) == 0, "X_CALL_CTX: nieoczekiwany uklad pol" );
static_assert( offsetof( X_CALL_CTX, argc ) == 8, "X_CALL_CTX: nieoczekiwany uklad pol" );
static_assert( offsetof( X_CALL_CTX, result ) == 16, "X_CALL_CTX: nieoczekiwany uklad pol" );
static_assert( offsetof( X_CALL_CTX, self ) == 24, "X_CALL_CTX: nieoczekiwany uklad pol" );
static_assert( offsetof( X_CALL_CTX, methodContext ) == 32, "X_CALL_CTX: nieoczekiwany uklad pol" );

static std::string s_key( const std::string &name );

/* Runtime error codes of failed native checks */
static int s_nativeErrorCode( X_NATIVE_ERROR kind )
{
   switch( kind )
   {
   case X_NATIVE_ERROR::RANGE:    return 3105;
   case X_NATIVE_ERROR::OVERFLOW: return 3106;
   default:                       return 3108;
   }
}

/* A STATIC initializer of literals, operators and CAST only; typed is set when a native type takes part */
static bool s_nativeInitializer( const X_EXPR &expr, bool &typed )
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

   typed |= expr.nativeType || expr.kind == X_EXPR_KIND::CAST;

   for( const X_EXPR_PTR &arg : expr.args )
   {
      if( !s_nativeInitializer( *arg, typed ) )
         return false;
   }

   return true;
}

/* STATIC types kept in an X_ITEM global */
static bool s_itemStaticType( X_DECL_TYPE type )
{
   if( type == X_DECL_TYPE::ANY || type == X_DECL_TYPE::OBJECT || type == X_DECL_TYPE::ARRAY )
      return true;

   return type == X_DECL_TYPE::CODEBLOCK || type == X_DECL_TYPE::DATE || type == X_DECL_TYPE::HASH;
}

/* ---------------------------------------------------------------------------------------------
   Backend implementation
   --------------------------------------------------------------------------------------------- */

struct X_LLVM::IMPL
{
   struct LOOP_TARGETS
   {
      llvm::BasicBlock *condition; /* NULL for SWITCH: EXIT target only, LOOP skips it */
      llvm::BasicBlock *done;
   };

   struct STATIC_SLOT : X_OWNED_ITEM
   {
      const X_EXPR *initializer = nullptr;
      llvm::GlobalVariable *storage = nullptr;
      const std::vector<X_EXPR_PTR> *dimensions = nullptr;
   };

   struct NATIVE_VALUE
   {
      llvm::Value *value = nullptr;
      X_DECL_TYPE type = X_DECL_TYPE::ANY;
      llvm::Value *box = nullptr;
      bool optionalBox = false;
      std::string typeName;
      llvm::Value *item = nullptr; /* STRING only: the item that owns the text, shared by nativeItem() */

      NATIVE_VALUE() = default;

      NATIVE_VALUE( llvm::Value *nativeValue, X_DECL_TYPE nativeType, llvm::Value *nativeBox = nullptr,
                    bool nativeOptionalBox = false, std::string nativeTypeName = {} )
      {
         value = nativeValue;
         type = nativeType;
         box = nativeBox;
         optionalBox = nativeOptionalBox;
         typeName = std::move( nativeTypeName );
      }
   };

   struct NATIVE_STATIC
   {
      llvm::GlobalVariable *storage;
      X_DECL_TYPE type;
      const X_EXPR *init;
      int line;
      std::string typeName;

      NATIVE_STATIC( llvm::GlobalVariable *nativeStorage, X_DECL_TYPE nativeType, const X_EXPR *nativeInit,
                     int nativeLine, std::string nativeTypeName = {} )
      {
         storage = nativeStorage;
         type = nativeType;
         init = nativeInit;
         line = nativeLine;
         typeName = std::move( nativeTypeName );
      }
   };

   struct NATIVE_FUNCTION
   {
      llvm::Function *function;
      const X_FUNCTION *declaration;
      std::vector<X_DECL_TYPE> parameters;
      std::vector<std::string> parameterTypeNames;
      X_DECL_TYPE result;
      std::string resultTypeName;
   };

   struct CALLBACK_JOB
   {
      llvm::Function *trampoline;
      const X_CALLBACK_DECL *signature;
      std::string target;
   };

   enum class STATIC_TYPE
   {
      UNKNOWN,
      NIL,
      LOGICAL,
      INTEGER,
      DOUBLE,
      NUMERIC,
      STRING,
      PERSISTENT
   };

   struct DIRECT_METHOD
   {
      llvm::Function *target;
      uint32_t handle;
      uint32_t owner;
      llvm::Function *entry = nullptr;
      bool stable = false;
      bool self = false;
      std::string className;
      std::string message;
      const NATIVE_FUNCTION *native = nullptr;
      llvm::Function *nativeEntry = nullptr;
   };

   struct BLOCK_JOB
   {
      const X_EXPR *expression;
      llvm::Function *function;
      std::vector<std::string> captures;
      std::unordered_map<std::string, llvm::Value *> statics;
      std::unordered_map<std::string, X_DECL_TYPE> nativeCaptures;
      std::unordered_map<std::string, NATIVE_VALUE> nativeStatics;
      bool classMethod = false;
   };

   using GLOBALS = std::unordered_map<std::string, llvm::GlobalVariable *>;
   using NAME_SET = std::set<std::string>;
   using VALUES = std::vector<llvm::Value *>;
   using LOCAL_TYPES = std::unordered_map<std::string, STATIC_TYPE>;

   llvm::LLVMContext context;
   llvm::IRBuilder<> builder;
   std::unique_ptr<llvm::Module> module;
   X_LLVM_NATIVE native;
   std::unordered_map<std::string, NATIVE_VALUE> nativeLocals;
   std::unordered_map<std::string, llvm::StructType *> nativeStructTypes;
   std::unordered_map<std::string, const X_STRUCT_DECL *> nativeStructDeclarations;
   std::unordered_set<std::string> nativeDllNames;
   std::unordered_map<std::string, std::unordered_map<std::string, NATIVE_VALUE>> nativeStatics;
   std::vector<NATIVE_STATIC> nativeStaticSlots;
   std::unordered_map<std::string, NATIVE_FUNCTION> nativeFunctions;
   std::unordered_map<std::string, const X_CALLBACK_DECL *> callbackTypes;
   std::unordered_map<std::string, llvm::Function *> callbackTrampolines;
   std::vector<CALLBACK_JOB> callbackJobs;
   llvm::Value *nativeResult = nullptr;
   X_DECL_TYPE nativeResultType = X_DECL_TYPE::ANY;

   llvm::StructType *ctxType = nullptr;
   llvm::StructType *itemStructType = nullptr;
   llvm::StructType *argStructType = nullptr; /* { ptr value, i32 byReference }, see getArgType() */

   llvm::Function *itemCopyF = nullptr;
   llvm::Function *itemClearF = nullptr;
   llvm::Function *xgrabF = nullptr;

   llvm::Value *trace = nullptr;
   int srcLine = 0;
   std::function<std::string( int )> sourceLoc;

   llvm::Function *currentFunc = nullptr;
   llvm::Value *callCtx = nullptr;
   llvm::Value *currentResult = nullptr;
   llvm::Value *currentArgc = nullptr;
   llvm::Value *currentArgs = nullptr;
   std::string currentFunctionName; /* upper-case key, the source of PROCNAME() */
   bool currentClassMethod = false;
   bool sharedModule = false; /* --shared: the public functions are exported from the DLL */
   unsigned tempIndex = 0;

   std::unordered_map<std::string, llvm::Value *> locals;
   std::unordered_map<std::string, STATIC_TYPE> localTypes;
   std::unordered_map<std::string, STATIC_TYPE> scalars;
   std::unordered_set<std::string> factories;
   std::unordered_map<std::string, DIRECT_METHOD> methods;
   std::unordered_map<const X_EXPR *, llvm::Function *> sends;
   std::unordered_map<const X_EXPR *, DIRECT_METHOD *> nativeSends;
   std::vector<llvm::Value *> itemSlots;
   std::vector<llvm::Value *> stringSlots;
   std::vector<llvm::Value *> boxSlots;
   std::vector<BLOCK_JOB> blockJobs;
   llvm::BasicBlock *cleanupBB = nullptr;
   llvm::BasicBlock *unwindBB = nullptr;
   llvm::BasicBlock *unwindTop = nullptr;
   llvm::BasicBlock *unwindEnd = nullptr;
   llvm::PHINode *unwindEx = nullptr;
   std::vector<LOOP_TARGETS> loopTargets;

   std::unordered_map<std::string, llvm::Function *> functions;
   std::unordered_map<std::string, llvm::Function *> classRegistrars;
   GLOBALS classHandles;
   std::unordered_set<std::string> lifecycleNames;
   std::vector<llvm::Function *> initProcedures;
   std::vector<llvm::Function *> exitProcedures;
   GLOBALS fileStatics;
   std::unordered_map<std::string, GLOBALS> functionStatics;
   std::vector<std::unique_ptr<STATIC_SLOT>> staticSlots;

   /* C2018 analysis, see blockExpression(); the keys are block AST node addresses,
      valid for the lifetime of the X_PROGRAM passed to generate() */
   std::unordered_map<const X_EXPR *, std::vector<std::string>> blockFreeVars;

   std::string error;

   IMPL() : builder( context ), module( std::make_unique<llvm::Module>( "xdev", context ) ), native( builder, guard() )
   {
   }

   /* The guard of native operations: a failed check raises the runtime error of its kind */
   X_LLVM_NATIVE::GUARD guard()
   {
      return [this]( llvm::Value *valid, X_NATIVE_ERROR kind ) { nativeGuard( valid, s_nativeErrorCode( kind ) ); };
   }

   /* ---------------------------------------------------------------------------------------------
      Errors and types
      --------------------------------------------------------------------------------------------- */

   /* Only the first error is kept */
   void fail( const std::string &text )
   {
      if( error.empty() )
         error = text;
   }

   void fail( int line, const std::string &text )
   {
      if( line <= 0 )
         fail( text );
      else if( sourceLoc )
         fail( sourceLoc( line ) + ": error: " + text );
      else
         fail( "<input>:" + std::to_string( line ) + ": error: " + text );
   }

   bool failed() const
   {
      return !error.empty();
   }

   llvm::Type *ptrTy()
   {
      return llvm::PointerType::get( context, 0 );
   }

   llvm::Type *i32Ty()
   {
      return llvm::Type::getInt32Ty( context );
   }

   llvm::Type *i64Ty()
   {
      return llvm::Type::getInt64Ty( context );
   }

   llvm::Type *i1Ty()
   {
      return llvm::Type::getInt1Ty( context );
   }

   llvm::Type *doubleTy()
   {
      return llvm::Type::getDoubleTy( context );
   }

   llvm::Type *voidTy()
   {
      return llvm::Type::getVoidTy( context );
   }

   llvm::Constant *constant32( uint64_t value )
   {
      return llvm::ConstantInt::get( i32Ty(), value );
   }

   llvm::Constant *constant64( uint64_t value )
   {
      return llvm::ConstantInt::get( i64Ty(), value );
   }

   /* A function of the runtime library, declared on first use */
   llvm::FunctionCallee runtime( llvm::StringRef name, llvm::Type *result, llvm::ArrayRef<llvm::Type *> params = {} )
   {
      return module->getOrInsertFunction( name, llvm::FunctionType::get( result, params, false ) );
   }

   llvm::Function *intrinsicDeclaration( llvm::Intrinsic::ID id, llvm::ArrayRef<llvm::Type *> types )
   {
      if( !llvm::Intrinsic::isOverloaded( id ) )
         types = {};

#if LLVM_VERSION_MAJOR >= 20
      return llvm::Intrinsic::getOrInsertDeclaration( module.get(), id, types );
#else
      return llvm::Intrinsic::getDeclaration( module.get(), id, types );
#endif
   }

   llvm::StructType *getItemType()
   {
      if( itemStructType == nullptr )
         itemStructType = llvm::StructType::create( context, { i32Ty(), i64Ty(), i64Ty() }, "X_ITEM" );

      return itemStructType;
   }

   llvm::StructType *getArgType()
   {
      if( argStructType == nullptr )
         argStructType = llvm::StructType::create( context, { ptrTy(), i32Ty() }, "X_ARG" );

      return argStructType;
   }

   llvm::StructType *getCtxType()
   {
      if( ctxType == nullptr )
         ctxType = llvm::StructType::create( context, { ptrTy(), i32Ty(), ptrTy(), ptrTy(), ptrTy() }, "X_CALL_CTX" );

      return ctxType;
   }

   /* ---------------------------------------------------------------------------------------------
      Calls and unwinding
      --------------------------------------------------------------------------------------------- */

   llvm::StructType *exceptionType()
   {
      return llvm::StructType::get( context, { ptrTy(), i32Ty() } );
   }

   llvm::Constant *exceptionInfo( const char *name )
   {
      return llvm::cast<llvm::Constant>( module->getOrInsertGlobal( name, llvm::Type::getInt8Ty( context ) ) );
   }

   /* From here calls become invokes; their exceptions reach the landing pad made by finishUnwind() */
   void beginUnwind( bool cleanup = true )
   {
      setPersonality( currentFunc );
      unwindTop = llvm::BasicBlock::Create( context, "unwind", currentFunc );
      unwindBB = unwindTop;

      if( cleanup )
      {
         unwindEnd = llvm::BasicBlock::Create( context, "unwind.cleanup", currentFunc );

         llvm::IRBuilder<> build( unwindEnd );

         unwindEx = build.CreatePHI( exceptionType(), 0, "exception" );
      }
   }

   /* X_TRACE: prev, location, function and line */
   llvm::StructType *traceType()
   {
      return llvm::StructType::get( context, { ptrTy(), ptrTy(), ptrTy(), i32Ty() } );
   }

   /* The trace frame gets the source position of the next call */
   void traceSite()
   {
      const std::string site = IIF( sourceLoc, sourceLoc( srcLine ), "line " + std::to_string( srcLine ) );
      llvm::StructType *type = traceType();

      builder.CreateStore( builder.CreateGlobalString( site ), builder.CreateStructGEP( type, trace, 1 ) );
      builder.CreateStore( constant32( ( uint32_t )srcLine ), builder.CreateStructGEP( type, trace, 3 ) );
   }

   /* Inside an unwind region a call becomes an invoke */
   llvm::CallBase *emitCall( llvm::FunctionCallee callee, llvm::ArrayRef<llvm::Value *> args = {} )
   {
      llvm::BasicBlock *next;
      llvm::CallBase *call;

      if( trace != nullptr && srcLine > 0 )
         traceSite();

      if( unwindBB == nullptr )
         return builder.CreateCall( callee, args );

      next = llvm::BasicBlock::Create( context, "call.continue", currentFunc );
      call = builder.CreateInvoke( callee, next, unwindBB, args );
      builder.SetInsertPoint( next );

      return call;
   }

   llvm::CallBase *emitCall( llvm::FunctionType *type, llvm::Value *callee, llvm::ArrayRef<llvm::Value *> args )
   {
      return emitCall( llvm::FunctionCallee( type, callee ), args );
   }

   void unwindToExit( llvm::Value *value )
   {
      unwindEx->addIncoming( value, builder.GetInsertBlock() );
      builder.CreateBr( unwindEnd );
   }

   /* The landing pad releases the result and the temporaries, discards a pending class, then resumes */
   void finishUnwind( llvm::Value *pending = nullptr )
   {
      llvm::LandingPadInst *pad;

      unwindBB = nullptr;
      builder.SetInsertPoint( unwindTop );
      pad = builder.CreateLandingPad( exceptionType(), 0 );
      pad->setCleanup( true );
      unwindToExit( pad );

      builder.SetInsertPoint( unwindEnd );

      if( currentResult != nullptr )
         emitCall( itemClearFn(), { currentResult } );

      emitCleanup();

      if( pending != nullptr )
         emitCall( runtime( "x_clsDiscard", i32Ty(), { i32Ty() } ), { builder.CreateLoad( i32Ty(), pending ) } );

      if( trace != nullptr )
         builder.CreateCall( runtime( "x_traceLeave", voidTy(), { ptrTy() } ), { trace } );

      builder.CreateResume( unwindEx );
   }

   /* An X_CALL_CTX in the entry block: args, argc and result are set here, self and methodContext are NULL */
   llvm::Value *callFrame( llvm::Value *result, llvm::Value *argc, llvm::Value *args )
   {
      llvm::BasicBlock &entry = currentFunc->getEntryBlock();
      llvm::IRBuilder<> entryBuilder( &entry, entry.getFirstInsertionPt() );
      llvm::AllocaInst *frame = entryBuilder.CreateAlloca( getCtxType(), nullptr, "call.frame" );

      frame->setAlignment( llvm::Align( alignof( X_CALL_CTX ) ) );
      builder.CreateStore( llvm::Constant::getNullValue( getCtxType() ), frame );
      builder.CreateStore( args, builder.CreateStructGEP( getCtxType(), frame, 0 ) );
      builder.CreateStore( argc, builder.CreateStructGEP( getCtxType(), frame, 1 ) );
      builder.CreateStore( result, builder.CreateStructGEP( getCtxType(), frame, 2 ) );

      return frame;
   }

   void loadCall( llvm::Value *ctx )
   {
      callCtx = ctx;
      currentArgs = builder.CreateLoad( ptrTy(), builder.CreateStructGEP( getCtxType(), ctx, 0 ) );
      currentArgc = builder.CreateLoad( i32Ty(), builder.CreateStructGEP( getCtxType(), ctx, 1 ) );
      currentResult = builder.CreateLoad( ptrTy(), builder.CreateStructGEP( getCtxType(), ctx, 2 ) );
   }

   /* Every xDev function: void NAME( X_CALL_CTX *ctx ) */
   llvm::FunctionType *functionAbiType()
   {
      return llvm::FunctionType::get( voidTy(), { ptrTy() }, false );
   }

   llvm::Function *itemCopyFn()
   {
      if( itemCopyF == nullptr )
      {
         llvm::FunctionType *type = llvm::FunctionType::get( voidTy(), { ptrTy(), ptrTy() }, false );

         itemCopyF = llvm::Function::Create( type, llvm::Function::ExternalLinkage, "x_itemCopy", module.get() );
      }

      return itemCopyF;
   }

   llvm::Function *itemClearFn()
   {
      if( itemClearF == nullptr )
      {
         llvm::FunctionType *type = llvm::FunctionType::get( voidTy(), { ptrTy() }, false );

         itemClearF = llvm::Function::Create( type, llvm::Function::ExternalLinkage, "x_itemClear", module.get() );
      }

      return itemClearF;
   }

   llvm::Function *xgrabFn()
   {
      if( xgrabF == nullptr )
      {
         llvm::FunctionType *type = llvm::FunctionType::get( ptrTy(), { i64Ty() }, false );

         xgrabF = llvm::Function::Create( type, llvm::Function::ExternalLinkage, "x_xgrab", module.get() );
      }

      return xgrabF;
   }

   /* An xDev function by its upper-case name; the linker finds functions of other modules */
   llvm::Function *resolveFunction( const std::string &key )
   {
      auto found = functions.find( key );
      llvm::Function *function;

      if( found != functions.end() )
         return found->second;

      function = llvm::Function::Create( functionAbiType(), llvm::Function::ExternalLinkage, key, module.get() );
      functions[key] = function;

      return function;
   }

   /* ---------------------------------------------------------------------------------------------
      STATIC variables
      --------------------------------------------------------------------------------------------- */

   /* A STATIC with a native type is a zeroed global; its initializer runs at module start */
   void nativeStatic( const X_LOCAL_DECL &decl, const std::string &owner, llvm::Type *type )
   {
      const X_DECL_TYPE declared = decl.declaredType.value_or( X_DECL_TYPE::ANY );
      const std::string name = "x.native.static." + std::to_string( nativeStaticSlots.size() );
      const auto linkage = llvm::GlobalValue::InternalLinkage;
      llvm::Constant *zero = llvm::Constant::getNullValue( type );
      auto *storage = new llvm::GlobalVariable( *module, type, false, linkage, zero, name );

      nativeStatics[owner][s_key( decl.name )] = { storage, declared, nullptr, false, decl.declaredTypeName };
      nativeStaticSlots.push_back( { storage, declared, decl.init.get(), decl.line, decl.declaredTypeName } );
   }

   /* A constant of native types is computed at module start; any other constant is folded now */
   bool staticInitializer( const X_LOCAL_DECL &decl, STATIC_SLOT &slot )
   {
      bool typed = false;
      const bool nativeConstant = s_nativeInitializer( *decl.init, typed ) && typed;

      if( !nativeConstant && !x_foldConstant( *decl.init, &slot.value ) )
      {
         fail( "blad wewnetrzny: nie mozna odtworzyc sprawdzonej stalej STATIC: " + decl.name );
         return false;
      }

      if( nativeConstant )
         slot.initializer = decl.init.get();

      return true;
   }

   /* The global X_ITEM of a STATIC with its folded value; strings, arrays and hashes start as NIL */
   llvm::GlobalVariable *staticGlobal( const X_ITEM &value )
   {
      const std::string name = "x.static." + std::to_string( staticSlots.size() );
      const auto linkage = llvm::GlobalValue::InternalLinkage;
      llvm::Constant *payload = constant64( 0 );
      int type = value.nType;
      llvm::Constant *initial;
      llvm::GlobalVariable *storage;

      if( value.nType == X_IT_INTEGER )
         payload = constant64( ( uint64_t )value.u.integer );

      if( value.nType == X_IT_LOGICAL )
         payload = constant64( value.u.logical );

      if( value.nType == X_IT_DOUBLE )
      {
         llvm::Constant *number = llvm::ConstantFP::get( llvm::Type::getDoubleTy( context ), value.u.dbl );

         payload = llvm::ConstantExpr::getBitCast( number, i64Ty() );
      }

      if( value.nType == X_IT_STRING || value.nType == X_IT_ARRAY || value.nType == X_IT_HASH )
         type = X_IT_NIL;

      initial = llvm::ConstantStruct::get( getItemType(), { constant32( type ), payload, constant64( 0 ) } );
      storage = new llvm::GlobalVariable( *module, getItemType(), false, linkage, initial, name );
      storage->setAlignment( llvm::Align( alignof( X_ITEM ) ) );

      return storage;
   }

   bool collectStatic( const X_LOCAL_DECL &decl, const std::string &owner, GLOBALS &names )
   {
      const std::string key = s_key( decl.name );
      const X_DECL_TYPE declared = decl.declaredType.value_or( X_DECL_TYPE::ANY );
      llvm::Type *type = nativeType( declared, decl.declaredTypeName );
      std::unique_ptr<STATIC_SLOT> slot;

      if( type )
      {
         nativeStatic( decl, owner, type );
         return true;
      }

      if( !s_itemStaticType( declared ) )
      {
         const std::string typeName = x_declTypeName( declared );

         fail( decl.line, "STATIC AS " + typeName + " wymaga kolejnego etapu backendu" );
         return false;
      }

      slot = std::make_unique<STATIC_SLOT>();

      /* The frontend has validated the dimensions; their one-time initialization is emitted later */
      if( !decl.dimensions.empty() )
         slot->dimensions = &decl.dimensions;

      if( decl.init && !staticInitializer( decl, *slot ) )
         return false;

      if( declared == X_DECL_TYPE::DATE && !decl.init )
         slot->value.nType = X_IT_DATE;

      slot->storage = staticGlobal( slot->value );
      names[key] = slot->storage;
      staticSlots.push_back( std::move( slot ) );

      return true;
   }

   /* STATIC variables of the module (empty owner) or of one function */
   bool collectStatics( const X_SCOPE &scope, const std::string &owner )
   {
      GLOBALS &names = IIF( owner.empty(), fileStatics, functionStatics[owner] );

      for( const auto &entry : scope.symbols )
      {
         if( entry.second.kind != X_BINDING_KIND::STATIC || !entry.second.declaration )
            continue;

         if( !collectStatic( *entry.second.declaration, owner, names ) )
            return false;
      }

      return true;
   }

   /* ---------------------------------------------------------------------------------------------
      Items
      --------------------------------------------------------------------------------------------- */

   std::string uniqueName( const char *prefix )
   {
      return std::string( prefix ) + "." + std::to_string( tempIndex++ );
   }

   /* A NIL X_ITEM in the entry block, cleared where it is used and at every exit of the function */
   llvm::Value *freshItem( const char *prefix )
   {
      llvm::BasicBlock &entry = currentFunc->getEntryBlock();
      llvm::IRBuilder<> entryBuilder( &entry, entry.getFirstInsertionPt() );
      llvm::AllocaInst *slot = entryBuilder.CreateAlloca( getItemType(), nullptr, uniqueName( prefix ) );

      slot->setAlignment( llvm::Align( alignof( X_ITEM ) ) );
      entryBuilder.CreateStore( llvm::Constant::getNullValue( getItemType() ), slot );
      itemSlots.push_back( slot );
      emitCall( itemClearFn(), { slot } );

      return slot;
   }

   void storeNType( llvm::Value *slot, int32_t type )
   {
      builder.CreateStore( constant32( ( uint32_t )type ), builder.CreateStructGEP( getItemType(), slot, 0 ) );
   }

   void storePayload( llvm::Value *slot, llvm::Value *value )
   {
      builder.CreateStore( value, builder.CreateStructGEP( getItemType(), slot, 1 ) );
   }

   llvm::Value *nilItem()
   {
      return freshItem( "nil" );
   }

   llvm::Value *emptyDateItem()
   {
      llvm::Value *slot = freshItem( "date.empty" );

      storeNType( slot, X_IT_DATE );
      storePayload( slot, constant64( 0 ) );

      return slot;
   }

   llvm::Value *numberItem( double value, bool integer, int64_t payload )
   {
      llvm::Value *slot = freshItem( "number" );

      if( integer )
      {
         storeNType( slot, ( int32_t )X_IT_INTEGER );
         storePayload( slot, llvm::ConstantInt::get( i64Ty(), payload, true ) );
      }
      else
      {
         storeNType( slot, ( int32_t )X_IT_DOUBLE );
         storePayload( slot, llvm::ConstantFP::get( llvm::Type::getDoubleTy( context ), value ) );
      }

      return slot;
   }

   llvm::Value *newArrayItem( size_t length )
   {
      llvm::Value *result = freshItem( "array" );

      emitCall( runtime( "x_arrayNew", i32Ty(), { ptrTy(), i64Ty() } ), { result, constant64( length ) } );

      return result;
   }

   void fillArray( llvm::Value *array, size_t index, llvm::Value *value )
   {
      llvm::FunctionCallee set = runtime( "x_arraySet", i32Ty(), { ptrTy(), i64Ty(), ptrTy() } );

      emitCall( set, { array, constant64( index ), value } );
   }

   /* { a, b, c }: the elements are evaluated first, then the array is filled */
   llvm::Value *arrayExpression( const X_EXPR &expr )
   {
      std::vector<llvm::Value *> values;
      llvm::Value *result;

      for( const X_EXPR_PTR &element : expr.args )
      {
         llvm::Value *value = expression( *element );

         if( !value )
            return nullptr;

         values.push_back( value );
      }

      result = newArrayItem( values.size() );

      for( size_t i = 0; i < values.size(); ++i )
         fillArray( result, i + 1, values[i] );

      return result;
   }

   /* LOCAL a[ n, m ]: x_arrayDimensions() takes the dimensions as an X_ARG array */
   llvm::Value *dimensionExpression( const std::vector<X_EXPR_PTR> &dimensions )
   {
      llvm::FunctionCallee create = runtime( "x_arrayDimensions", i32Ty(), { ptrTy(), i32Ty(), ptrTy() } );
      std::vector<llvm::Value *> values;
      llvm::Value *args;
      llvm::Value *result;

      for( const X_EXPR_PTR &dim : dimensions )
      {
         llvm::Value *value = expression( *dim );

         if( !value )
            return nullptr;

         values.push_back( value );
      }

      llvm::BasicBlock &entry = currentFunc->getEntryBlock();
      llvm::IRBuilder<> entryBuilder( &entry, entry.begin() );

      args = entryBuilder.CreateAlloca( getArgType(), constant32( values.size() ), "dimensions" );

      for( size_t i = 0; i < values.size(); ++i )
      {
         llvm::Value *arg = builder.CreateGEP( getArgType(), args, constant64( i ) );

         builder.CreateStore( values[i], builder.CreateStructGEP( getArgType(), arg, 0 ) );
         builder.CreateStore( constant32( 0 ), builder.CreateStructGEP( getArgType(), arg, 1 ) );
      }

      result = freshItem( "dimensioned.array" );
      emitCall( create, { result, constant32( values.size() ), args } );

      return result;
   }

   llvm::Value *constantArray( const X_ITEM &value )
   {
      llvm::Value *result = newArrayItem( x_arrayLen( &value ) );

      for( int64_t i = 1; i <= x_arrayLen( &value ); ++i )
      {
         llvm::Value *element = constantItem( *x_arrayGetItemPtr( &value, i ) );

         fillArray( result, i, element );
         emitCall( itemClearFn(), { element } );
      }

      return result;
   }

   llvm::Value *constantHash( const X_ITEM &value )
   {
      llvm::Value *result = freshItem( "hash" );

      emitCall( runtime( "x_hashNew", i32Ty(), { ptrTy() } ), { result } );

      for( size_t i = 1; i <= x_hashLen( &value ); ++i )
      {
         llvm::Value *key = constantItem( *x_hashKeyAt( &value, i ) );
         llvm::Value *element = constantItem( *x_hashValueAt( &value, i ) );

         emitCall( runtime( "x_hashAdd", i32Ty(), { ptrTy(), ptrTy(), ptrTy() } ), { result, key, element } );
         emitCall( itemClearFn(), { key } );
         emitCall( itemClearFn(), { element } );
      }

      return result;
   }

   /* An item built from a constant folded at compile time */
   llvm::Value *constantItem( const X_ITEM &value )
   {
      if( value.nType == X_IT_STRING )
         return stringItem( std::string( value.u.string.data, value.u.string.len ) );

      if( value.nType == X_IT_INTEGER )
         return numberItem( 0, true, value.u.integer );

      if( value.nType == X_IT_DOUBLE )
         return numberItem( value.u.dbl, false, 0 );

      if( value.nType == X_IT_LOGICAL )
         return logicalItem( value.u.logical != 0 );

      if( value.nType == X_IT_DATE )
      {
         llvm::Value *slot = emptyDateItem();

         storePayload( slot, constant64( ( uint64_t )value.u.integer ) );
         return slot;
      }

      if( value.nType == X_IT_ARRAY )
         return constantArray( value );

      if( value.nType == X_IT_HASH )
         return constantHash( value );

      return nilItem();
   }

   llvm::Value *logicalItem( bool value )
   {
      llvm::Value *slot = freshItem( "logical" );

      storeNType( slot, ( int32_t )X_IT_LOGICAL );
      storePayload( slot, constant64( IIF( value, 1, 0 ) ) );

      return slot;
   }

   /* A string literal is copied into a block of x_xgrab() owned by the item */
   llvm::Value *stringItem( const std::string &text )
   {
      llvm::Value *slot = freshItem( "string" );
      llvm::Value *global = builder.CreateGlobalString( text );
      llvm::Value *len = constant64( text.size() + 1 );
      llvm::Value *buf = emitCall( xgrabFn(), { len } );

      builder.CreateMemCpy( buf, llvm::MaybeAlign( 1 ), global, llvm::MaybeAlign( 1 ), len );
      storeNType( slot, ( int32_t )X_IT_STRING );
      storePayload( slot, buf );
      builder.CreateStore( constant64( text.size() ), builder.CreateStructGEP( getItemType(), slot, 2 ) );

      return slot;
   }

   /* A copy of the value of a variable */
   llvm::Value *variableItem( const std::string &name, int line = 0 )
   {
      auto it = locals.find( s_key( name ) );
      llvm::Value *snapshot;

      if( it == locals.end() )
      {
         fail( line, "blad wewnetrzny: brak magazynu rozstrzygnietej zmiennej " + name );
         return nullptr;
      }

      if( llvm::isa<llvm::GlobalVariable>( it->second ) )
      {
         snapshot = freshItem( "static.read" );
         emitCall( itemCopyFn(), { snapshot, it->second } );
         return snapshot;
      }

      snapshot = freshItem( "local.read" );
      emitCall( itemCopyFn(), { snapshot, localStorage( it->second ) } );

      return snapshot;
   }

   /* The storage of a variable itself, for @var and assignments */
   llvm::Value *referenceItem( const std::string &name, int line = 0 )
   {
      auto native = nativeLocals.find( s_key( name ) );
      auto it = locals.find( s_key( name ) );

      if( native != nativeLocals.end() )
      {
         llvm::Value *box = nativeReference( native->second );

         if( !box )
            return nullptr;

         return localStorage( box );
      }

      if( it != locals.end() )
         return localStorage( it->second );

      fail( line, "blad wewnetrzny: brak magazynu rozstrzygnietej zmiennej " + name );

      return nullptr;
   }

   /* The temporaries first..last are cleared with the collector suspended */
   void clearTemps( size_t first, size_t last )
   {
      if( first == last )
         return;

      builder.CreateCall( runtime( "x_gcSuspend", voidTy() ) );

      for( size_t i = first; i < last; ++i )
         builder.CreateCall( itemClearFn(), { itemSlots[i] } );

      builder.CreateCall( runtime( "x_gcResume", voidTy() ) );
   }

   /* At every exit the temporaries, the native strings and the boxes are released, with the collector suspended */
   void emitCleanup()
   {
      llvm::BasicBlock *saved = unwindBB;
      llvm::Type *stringType = nativeType( X_DECL_TYPE::STRING );

      unwindBB = nullptr;
      emitCall( runtime( "x_gcSuspend", voidTy() ) );

      for( llvm::Value *slot : itemSlots )
         emitCall( itemClearFn(), { slot } );

      for( llvm::Value *slot : stringSlots )
      {
         llvm::Value *value = builder.CreateLoad( stringType, slot );

         emitCall( runtime( "x_xrelease", voidTy(), { ptrTy() } ), { builder.CreateExtractValue( value, 0 ) } );
         builder.CreateStore( llvm::Constant::getNullValue( stringType ), slot );
      }

      for( llvm::Value *box : boxSlots )
         emitCall( runtime( "x_boxLeave", voidTy(), { ptrTy() } ), { box } );

      emitCall( runtime( "x_gcResume", voidTy() ) );
      unwindBB = saved;
   }

   /* A box holds one X_ITEM that code blocks can share; it is made in the entry block and left at every exit */
   llvm::Value *freshBox( const char *name )
   {
      llvm::BasicBlock &entry = currentFunc->getEntryBlock();
      llvm::IRBuilder<> entryBuilder( &entry, entry.getFirstInsertionPt() );
      llvm::Value *box = entryBuilder.CreateCall( runtime( "x_boxNew", ptrTy() ), {}, name );

      boxSlots.push_back( box );
      emitCall( itemClearFn(), { box } );

      return box;
   }

   /* The X_ITEM of a box; a STATIC global is its own storage */
   llvm::Value *localStorage( llvm::Value *slot )
   {
      if( llvm::isa<llvm::GlobalVariable>( slot ) )
         return slot;

      return emitCall( runtime( "x_boxItem", ptrTy(), { ptrTy() } ), { slot } );
   }

   /* ---------------------------------------------------------------------------------------------
      Native values
      --------------------------------------------------------------------------------------------- */

   /* The LLVM type of a native value: STRING is { ptr, len }, a STRUCT has its own type; nullptr for OBJECT */
   llvm::Type *nativeType( X_DECL_TYPE type, const std::string &typeName = {} )
   {
      if( type == X_DECL_TYPE::OBJECT )
         return nullptr;

      if( type == X_DECL_TYPE::STRUCT )
      {
         const auto found = nativeStructTypes.find( s_key( typeName ) );

         if( found == nativeStructTypes.end() )
            return nullptr;

         return found->second;
      }

      if( type == X_DECL_TYPE::STRING )
         return llvm::StructType::get( context, { ptrTy(), i64Ty() } );

      return native.type( type );
   }

   /* Win32 BOOL, used by callback-heavy APIs such as EnumWindows, is a 32-bit C integer;
      LOGIC stays i1 inside xDev and is normalized at the callback ABI boundary */
   llvm::Type *callbackType( X_DECL_TYPE type )
   {
      if( type == X_DECL_TYPE::LOGIC )
         return i32Ty();

      return nativeType( type );
   }

   /* The fields of a STRUCT; a fixed array field is an LLVM array */
   bool structBody( const X_STRUCT_DECL &structure )
   {
      std::vector<llvm::Type *> members;

      members.reserve( structure.fields.size() );

      for( const X_STRUCT_FIELD &field : structure.fields )
      {
         llvm::Type *type = nativeType( field.type, field.typeName );

         if( !type )
         {
            fail( field.line, "nieznany typ pola STRUCT: " + field.typeName );
            return false;
         }

         if( field.arrayCount != 0 )
            type = llvm::ArrayType::get( type, field.arrayCount );

         members.push_back( type );
      }

      nativeStructTypes.at( s_key( structure.name ) )->setBody( members, false );

      return true;
   }

   /* All STRUCT types are named first, so a field may have the type of a later STRUCT */
   bool createStructTypes( const std::vector<X_STRUCT_DECL> &structures )
   {
      for( const X_STRUCT_DECL &structure : structures )
      {
         const std::string key = s_key( structure.name );

         if( !nativeStructDeclarations.emplace( key, &structure ).second )
         {
            fail( structure.line, "powtorzona deklaracja STRUCT: " + structure.name );
            return false;
         }

         nativeStructTypes.emplace( key, llvm::StructType::create( context, "xdev.struct." + key ) );
      }

      for( const X_STRUCT_DECL &structure : structures )
      {
         if( !structBody( structure ) )
            return false;
      }

      return true;
   }

   /* The kind of a native box: 0x800 pointer, 0x400 string, 0x200 float, 0x100 unsigned, plus the bit width */
   int nativeKind( X_DECL_TYPE type )
   {
      const bool isUnsigned = type >= X_DECL_TYPE::UINT8 && type <= X_DECL_TYPE::UINT64;

      if( type == X_DECL_TYPE::PTR || type == X_DECL_TYPE::CALLBACK )
         return 0x800 | 64;

      if( type == X_DECL_TYPE::STRING || type == X_DECL_TYPE::PSZ )
         return 0x400;

      if( type == X_DECL_TYPE::LOGIC )
         return 1;

      if( type == X_DECL_TYPE::FLOAT32 )
         return 0x200 | 32;

      if( type == X_DECL_TYPE::FLOAT64 )
         return 0x200 | 64;

      return nativeType( type )->getIntegerBitWidth() | IIF( isUnsigned, 0x100, 0 );
   }

   llvm::Value *freshNativeBox( X_DECL_TYPE type, const std::string &name )
   {
      llvm::BasicBlock &entry = currentFunc->getEntryBlock();
      llvm::IRBuilder<> entryBuilder( &entry, entry.getFirstInsertionPt() );
      llvm::FunctionCallee create = runtime( "x_boxNativeNew", ptrTy(), { i32Ty() } );
      llvm::Value *box = entryBuilder.CreateCall( create, { constant32( nativeKind( type ) ) }, name );

      boxSlots.push_back( box );

      return box;
   }

   /* A native STATIC gets a box over its global, made right after the box in the entry block */
   void attachStaticBox( NATIVE_VALUE &storage )
   {
      llvm::FunctionCallee attach = runtime( "x_boxNativeStorage", voidTy(), { ptrTy(), ptrTy() } );
      llvm::IRBuilder<> entry( &currentFunc->getEntryBlock() );

      storage.box = freshNativeBox( storage.type, "native.static.reference" );
      entry.SetInsertPoint( llvm::cast<llvm::Instruction>( storage.box )->getNextNode() );
      entry.CreateCall( attach, { storage.box, storage.value } );
   }

   /* The shared box of a native variable, for @var */
   llvm::Value *nativeReference( NATIVE_VALUE &storage )
   {
      if( !storage.box && llvm::isa_and_nonnull<llvm::GlobalVariable>( storage.value ) )
         attachStaticBox( storage );

      if( !storage.box || storage.optionalBox )
      {
         fail( srcLine, "brak wspolnego magazynu zmiennej z AS" );
         return nullptr;
      }

      return storage.box;
   }

   /* A snapshot of the text of a STRING storage that shares its buffer instead of copying it;
      the text of a storage is always a block of x_xgrab() from x_stringStore(), or NULL */
   llvm::Value *sharedStringItem( llvm::Value *text )
   {
      llvm::BasicBlock &entry = currentFunc->getEntryBlock();
      llvm::IRBuilder<> entryBuilder( &entry, entry.getFirstInsertionPt() );
      llvm::AllocaInst *view = entryBuilder.CreateAlloca( getItemType(), nullptr, uniqueName( "string.share" ) );
      llvm::Value *slot;

      view->setAlignment( llvm::Align( alignof( X_ITEM ) ) );

      storeNType( view, X_IT_STRING );
      builder.CreateStore( text, builder.CreateStructGEP( getItemType(), view, 1 ) );

      slot = freshItem( "string.shared" );
      emitCall( itemCopyFn(), { slot, view } );

      return slot;
   }

   /* The text of a STRING storage as a native value backed by a shared snapshot item */
   NATIVE_VALUE sharedStringLoad( llvm::Value *text )
   {
      llvm::Value *item = sharedStringItem( text );
      NATIVE_VALUE result = nativeUnbox( item, X_DECL_TYPE::STRING, false );

      result.item = item;

      return result;
   }

   /* An optional box is NULL while the variable is not shared; then the box holds the value */
   NATIVE_VALUE optionalLoad( const NATIVE_VALUE &storage )
   {
      llvm::BasicBlock *local = llvm::BasicBlock::Create( context, "native.local.read", currentFunc );
      llvm::BasicBlock *shared = llvm::BasicBlock::Create( context, "native.shared.read", currentFunc );
      llvm::BasicBlock *done = llvm::BasicBlock::Create( context, "native.read.done", currentFunc );
      llvm::Value *localValue;
      llvm::BasicBlock *localEnd;
      NATIVE_VALUE sharedValue;
      llvm::BasicBlock *sharedEnd;
      llvm::PHINode *phi;

      builder.CreateCondBr( builder.CreateIsNull( storage.box ), local, shared );

      builder.SetInsertPoint( local );
      localValue = nativeLoad( { storage.value, storage.type } ).value;
      localEnd = builder.GetInsertBlock();
      builder.CreateBr( done );

      builder.SetInsertPoint( shared );
      sharedValue = nativeLoad( { nullptr, storage.type, storage.box } );
      sharedEnd = builder.GetInsertBlock();
      builder.CreateBr( done );

      builder.SetInsertPoint( done );
      phi = builder.CreatePHI( nativeType( storage.type, storage.typeName ), 2 );
      phi->addIncoming( localValue, localEnd );
      phi->addIncoming( sharedValue.value, sharedEnd );

      return { phi, storage.type, nullptr, false, storage.typeName };
   }

   /* The value of a native variable: from its global or alloca, from its box, or from either */
   NATIVE_VALUE nativeLoad( const NATIVE_VALUE &storage )
   {
      llvm::Type *type = nativeType( storage.type, storage.typeName );
      llvm::Value *address = storage.value;
      NATIVE_VALUE value;

      if( storage.box && storage.optionalBox )
         return optionalLoad( storage );

      if( storage.box )
         address = emitCall( runtime( "x_boxNativeData", ptrTy(), { ptrTy() } ), { storage.box } );

      value = { builder.CreateLoad( type, address ), storage.type, nullptr, false, storage.typeName };

      if( storage.type == X_DECL_TYPE::STRING )
         return sharedStringLoad( value.value );

      return value;
   }

   /* A STRING is copied by x_stringStore() or x_boxNativeStringStore() */
   void stringStore( const NATIVE_VALUE &storage, llvm::Value *value )
   {
      const char *name = IIF( storage.box, "x_boxNativeStringStore", "x_stringStore" );
      llvm::Value *target = IIF( storage.box, storage.box, storage.value );
      llvm::FunctionCallee store = runtime( name, voidTy(), { ptrTy(), ptrTy(), i64Ty() } );

      emitCall( store, { target, builder.CreateExtractValue( value, 0 ), builder.CreateExtractValue( value, 1 ) } );
   }

   void optionalStore( const NATIVE_VALUE &storage, llvm::Value *value )
   {
      llvm::BasicBlock *local = llvm::BasicBlock::Create( context, "native.local.write", currentFunc );
      llvm::BasicBlock *shared = llvm::BasicBlock::Create( context, "native.shared.write", currentFunc );
      llvm::BasicBlock *done = llvm::BasicBlock::Create( context, "native.write.done", currentFunc );

      builder.CreateCondBr( builder.CreateIsNull( storage.box ), local, shared );

      builder.SetInsertPoint( local );
      nativeStore( { storage.value, storage.type }, value );
      builder.CreateBr( done );

      builder.SetInsertPoint( shared );
      nativeStore( { nullptr, storage.type, storage.box }, value );
      builder.CreateBr( done );

      builder.SetInsertPoint( done );
   }

   /* A box keeps a native value in 64 bits: a pointer as an integer, a float as its bit pattern */
   void boxStore( llvm::Value *box, llvm::Value *value )
   {
      llvm::Type *type = value->getType();
      llvm::Value *bits = value;

      if( type->isPointerTy() )
         bits = builder.CreatePtrToInt( value, i64Ty() );
      else if( type->isFloatingPointTy() )
         bits = builder.CreateBitCast( value, IIF( type->isFloatTy(), i32Ty(), i64Ty() ) );

      bits = builder.CreateZExtOrTrunc( bits, i64Ty() );
      emitCall( runtime( "x_boxNativeStore", voidTy(), { ptrTy(), i64Ty() } ), { box, bits } );
   }

   void nativeStore( const NATIVE_VALUE &storage, llvm::Value *value )
   {
      if( storage.type == X_DECL_TYPE::STRING && !storage.optionalBox )
      {
         stringStore( storage, value );
         return;
      }

      if( !storage.box )
      {
         builder.CreateStore( value, storage.value );
         return;
      }

      if( storage.optionalBox )
      {
         optionalStore( storage, value );
         return;
      }

      boxStore( storage.box, value );
   }

   /* A STRUCT variable lives in an alloca of the function, never in a box */
   llvm::Value *structVariable( const X_EXPR &expr )
   {
      const auto found = nativeLocals.find( s_key( expr.text ) );

      if( found == nativeLocals.end() || found->second.type != X_DECL_TYPE::STRUCT || found->second.box )
      {
         fail( expr.line, "STRUCT wymaga lokalnej zmiennej o statycznym magazynie" );
         return nullptr;
      }

      return found->second.value;
   }

   /* st.field[ n ]: n is checked against 1..N, the element address is zero-based */
   llvm::Value *structElement( const X_EXPR &expr )
   {
      const X_EXPR &field = *expr.args[0];
      llvm::Value *base = nativeStructAddress( field );

      if( !base )
         return nullptr;

      llvm::Type *containerType = nativeType( X_DECL_TYPE::STRUCT, field.containerTypeName );
      const auto declaration = nativeStructDeclarations.find( s_key( field.containerTypeName ) );

      if( !containerType || declaration == nativeStructDeclarations.end() )
         return nullptr;

      const X_STRUCT_FIELD &member = declaration->second->fields.at( field.structFieldIndex );
      llvm::Type *elementType = nativeType( member.type, member.typeName );
      llvm::Type *arrayType = llvm::ArrayType::get( elementType, member.arrayCount );
      llvm::Value *arrayAddress = nativeStructAddress( field );

      if( !arrayAddress )
         return nullptr;

      NATIVE_VALUE index = nativeExpression( *expr.args[1] );

      if( !index.value )
         return nullptr;

      llvm::Value *first = builder.CreateICmpSGE( index.value, constant64( 1 ) );
      llvm::Value *last = builder.CreateICmpSLE( index.value, constant64( member.arrayCount ) );
      llvm::Value *zeroBased;

      nativeGuard( builder.CreateAnd( first, last ), 3105 );
      zeroBased = builder.CreateSub( index.value, constant64( 1 ) );

      return builder.CreateInBoundsGEP( arrayType, arrayAddress, { constant64( 0 ), zeroBased } );
   }

   /* The address of a STRUCT variable, of one of its fields or of an element of an array field */
   llvm::Value *nativeStructAddress( const X_EXPR &expr )
   {
      if( expr.kind == X_EXPR_KIND::NAME )
         return structVariable( expr );

      if( expr.kind == X_EXPR_KIND::SEND && expr.structFieldAccess )
      {
         llvm::Value *base = nativeStructAddress( *expr.args[0] );
         llvm::Type *type = nativeType( X_DECL_TYPE::STRUCT, expr.containerTypeName );

         if( !base || !type )
            return nullptr;

         return builder.CreateStructGEP( type, base, expr.structFieldIndex );
      }

      if( expr.kind == X_EXPR_KIND::INDEX && expr.structArrayIndex )
         return structElement( expr );

      fail( expr.line, "odwolanie do STRUCT nie jest l-wartoscia" );

      return nullptr;
   }

   /* ---------------------------------------------------------------------------------------------
      Code blocks
      --------------------------------------------------------------------------------------------- */

   llvm::Constant *nullPtr()
   {
      return llvm::ConstantPointerNull::get( llvm::PointerType::get( context, 0 ) );
   }

   void collectBlockFreeVars( const X_EXPR &expr, const NAME_SET &bound, NAME_SET &freeVars )
   {
      NAME_SET nestedBound;
      NAME_SET nestedFree;

      for( const std::string &name : expr.blockParams )
         nestedBound.insert( s_key( name ) );

      for( const X_EXPR_PTR &arg : expr.args )
         collectFreeVars( *arg, nestedBound, nestedFree );

      blockFreeVars[&expr] = std::vector<std::string>( nestedFree.begin(), nestedFree.end() );

      for( const std::string &name : nestedFree )
      {
         if( !bound.count( name ) )
            freeVars.insert( name );
      }
   }

   /* The free names of an expression; every nested block keeps its own list in blockFreeVars */
   void collectFreeVars( const X_EXPR &expr, const NAME_SET &bound, NAME_SET &freeVars )
   {
      if( expr.kind == X_EXPR_KIND::NAME )
      {
         const std::string key = s_key( expr.text );

         if( !bound.count( key ) )
            freeVars.insert( key );

         return;
      }

      if( expr.kind == X_EXPR_KIND::BLOCK )
      {
         collectBlockFreeVars( expr, bound, freeVars );
         return;
      }

      for( const X_EXPR_PTR &arg : expr.args )
         collectFreeVars( *arg, bound, freeVars );
   }

   /* The names that the blocks inside an expression need */
   void collectChildBlockNeeds( const X_EXPR *expr, NAME_SET &needed )
   {
      if( expr == nullptr )
         return;

      if( expr->kind == X_EXPR_KIND::BLOCK )
      {
         const auto found = blockFreeVars.find( expr );

         if( found != blockFreeVars.end() )
            needed.insert( found->second.begin(), found->second.end() );

         return;
      }

      for( const X_EXPR_PTR &arg : expr->args )
         collectChildBlockNeeds( arg.get(), needed );
   }

   /* STATIC variables seen by the block: the block function uses their globals directly */
   void blockStatics( BLOCK_JOB &job )
   {
      for( const auto &entry : locals )
      {
         if( llvm::isa<llvm::GlobalVariable>( entry.second ) )
            job.statics.emplace( entry );
      }

      for( const auto &entry : nativeLocals )
      {
         const NATIVE_VALUE &local = entry.second;

         if( llvm::isa_and_nonnull<llvm::GlobalVariable>( local.value ) )
         {
            NATIVE_VALUE value{ local.value, local.type, nullptr, false, local.typeName };

            job.nativeStatics.emplace( entry.first, value );
         }
      }
   }

   /* The variables the block captures; Super, and Self in a CLASSMETHOD, are not variables */
   bool blockCaptures( const X_EXPR &expr, BLOCK_JOB &job )
   {
      for( const std::string &name : expr.captures )
      {
         if( name == "SUPER" || ( name == "SELF" && currentClassMethod ) )
            continue;

         if( job.statics.count( name ) || job.nativeStatics.count( name ) )
            continue;

         if( nativeLocals.count( name ) )
         {
            job.captures.push_back( name );
            job.nativeCaptures[name] = nativeLocals.at( name ).type;
         }
         else if( locals.count( name ) )
            job.captures.push_back( name );
         else
         {
            fail( expr.line, "blad wewnetrzny: brak magazynu przechwyconej zmiennej " + name );
            return false;
         }
      }

      return true;
   }

   /* A native variable shares its box, kept alive by x_boxRetain(); an item variable gets one from x_boxCapture() */
   llvm::Value *captureBox( const BLOCK_JOB &job, const std::string &name )
   {
      llvm::Value *box;

      if( !job.nativeCaptures.count( name ) )
         return emitCall( runtime( "x_boxCapture", ptrTy(), { ptrTy() } ), { locals.at( name ) } );

      box = nativeReference( nativeLocals.at( name ) );
      if( !box )
         return nullptr;

      emitCall( runtime( "x_boxRetain", voidTy(), { ptrTy() } ), { box } );

      return box;
   }

   /* The boxes of the captured variables as an array in the entry block */
   llvm::Value *blockEnvironment( const BLOCK_JOB &job, VALUES &captures )
   {
      llvm::BasicBlock &entry = currentFunc->getEntryBlock();
      llvm::IRBuilder<> entryBuilder( &entry, entry.getFirstInsertionPt() );
      llvm::Value *environment = entryBuilder.CreateAlloca( ptrTy(), constant32( job.captures.size() ), "block.env" );

      for( size_t i = 0; i < job.captures.size(); ++i )
      {
         llvm::Value *box = captureBox( job, job.captures[i] );

         if( !box )
            return nullptr;

         builder.CreateStore( box, builder.CreateInBoundsGEP( ptrTy(), environment, constant64( i ) ) );
         captures.push_back( box );
      }

      return environment;
   }

   /* {| params | ... }: a closure over the boxes of the captured variables; the body is compiled later */
   llvm::Value *blockExpression( const X_EXPR &expr )
   {
      const std::string name = "__XDEV_BLOCK_" + std::to_string( blockJobs.size() );
      const std::vector<llvm::Type *> closureParams = { ptrTy(), ptrTy(), ptrTy(), ptrTy(), i32Ty() };
      llvm::FunctionType *blockType = llvm::FunctionType::get( voidTy(), { ptrTy(), ptrTy() }, false );
      llvm::Value *environment = nullPtr();
      VALUES captures;
      BLOCK_JOB job;
      llvm::Value *result;

      blockFreeVars[&expr] = expr.captures;
      job.expression = &expr;
      job.classMethod = currentClassMethod;
      blockStatics( job );

      if( !blockCaptures( expr, job ) )
         return nullptr;

      job.function = llvm::Function::Create( blockType, llvm::Function::InternalLinkage, name, module.get() );

      if( !job.captures.empty() )
      {
         environment = blockEnvironment( job, captures );
         if( !environment )
            return nullptr;
      }

      result = freshItem( "block.value" );

      llvm::FunctionCallee closure = runtime( "x_itemClosureInto", voidTy(), closureParams );

      emitCall( closure, { callCtx, result, job.function, environment, constant32( job.captures.size() ) } );

      for( llvm::Value *box : captures )
         emitCall( runtime( "x_boxRelease", voidTy(), { ptrTy() } ), { box } );

      blockJobs.push_back( std::move( job ) );

      return result;
   }

   /* ---------------------------------------------------------------------------------------------
      IIF and calls
      --------------------------------------------------------------------------------------------- */

   /* A variable keeps its type after two branches only when both branches agree on it */
   void mergeTypes( const LOCAL_TYPES &before, const LOCAL_TYPES &first, const LOCAL_TYPES &second )
   {
      for( const auto &entry : before )
      {
         const auto left = first.find( entry.first );
         const auto right = second.find( entry.first );

         if( left != first.end() && right != second.end() && left->second == right->second )
            localTypes[entry.first] = left->second;
         else
            localTypes[entry.first] = STATIC_TYPE::UNKNOWN;
      }
   }

   /* One value of IIF, evaluated in its own block and copied into the result */
   bool iifBranch( llvm::BasicBlock *block, const X_EXPR &value, llvm::Value *result, llvm::BasicBlock *done )
   {
      size_t first;
      llvm::Value *item;

      builder.SetInsertPoint( block );
      first = itemSlots.size();

      item = expression( value );
      if( item == nullptr )
         return false;

      emitCall( itemCopyFn(), { result, item } );
      clearTemps( first, itemSlots.size() );
      builder.CreateBr( done );

      return true;
   }

   /* The condition of IIF is LOGIC without an implicit conversion */
   llvm::Value *iifExpression( const X_EXPR &expr )
   {
      llvm::Value *condition;
      llvm::Value *result;
      llvm::BasicBlock *trueBB;
      llvm::BasicBlock *falseBB;
      llvm::BasicBlock *doneBB;
      LOCAL_TYPES before;
      LOCAL_TYPES trueTypes;
      LOCAL_TYPES falseTypes;

      if( !logicalType( *expr.args[0] ) )
      {
         fail( expr.line, "warunek IIF musi zwracac X_IT_LOGICAL (brak niejawnego rzutowania)" );
         return nullptr;
      }

      condition = scalarCondition( *expr.args[0] );
      if( condition == nullptr )
         return nullptr;

      result = freshItem( "iif.value" );
      trueBB = llvm::BasicBlock::Create( context, "iif.true", currentFunc );
      falseBB = llvm::BasicBlock::Create( context, "iif.false", currentFunc );
      doneBB = llvm::BasicBlock::Create( context, "iif.done", currentFunc );
      builder.CreateCondBr( condition, trueBB, falseBB );

      before = localTypes;

      if( !iifBranch( trueBB, *expr.args[1], result, doneBB ) )
         return nullptr;

      trueTypes = localTypes;
      localTypes = before;

      if( !iifBranch( falseBB, *expr.args[2], result, doneBB ) )
         return nullptr;

      falseTypes = localTypes;
      builder.SetInsertPoint( doneBB );
      localTypes = before;
      mergeTypes( before, trueTypes, falseTypes );

      return result;
   }

   /* The X_ARG array of a call, in the entry block; NULL without arguments */
   llvm::Value *argArray( const VALUES &items, const std::vector<bool> &byReference, const char *name )
   {
      llvm::BasicBlock &entry = currentFunc->getEntryBlock();
      llvm::Value *array;

      if( items.empty() )
         return nullPtr();

      llvm::IRBuilder<> entryBuilder( &entry, entry.getFirstInsertionPt() );

      array = entryBuilder.CreateAlloca( getArgType(), constant32( items.size() ), name );

      for( size_t i = 0; i < items.size(); ++i )
      {
         llvm::Value *element = builder.CreateInBoundsGEP( getArgType(), array, constant32( i ) );
         llvm::Value *flag = constant32( IIF( byReference[i], 1, 0 ) );

         builder.CreateStore( items[i], builder.CreateStructGEP( getArgType(), element, 0 ) );
         builder.CreateStore( flag, builder.CreateStructGEP( getArgType(), element, 1 ) );
      }

      return array;
   }

   /* @var passes the storage; an argument of a typed function that needs a runtime type check is an item */
   bool callArguments( const X_EXPR &expr, bool typed, VALUES &items, std::vector<bool> &byReference )
   {
      items.reserve( expr.args.size() );
      byReference.reserve( expr.args.size() );

      for( size_t i = 0; i < expr.args.size(); ++i )
      {
         const X_EXPR &arg = *expr.args[i];
         const bool isReference = i < expr.argByReference.size() && expr.argByReference[i];
         const bool checked = typed && arg.conversion && arg.conversion->checkType;
         llvm::Value *value;

         if( isReference )
            value = referenceItem( arg.text, arg.line );
         else if( checked )
            value = itemExpression( arg );
         else
            value = argumentItem( expr, i );

         if( value == nullptr )
            return false;

         items.push_back( value );
         byReference.push_back( isReference );
      }

      return true;
   }

   /* A typed function called directly with native arguments */
   llvm::Value *typedCall( const X_EXPR &expr, X_DECL_TYPE resultType )
   {
      llvm::Value *result = freshItem( "native.call.result" );
      llvm::Value *value = nativeCall( expr, result );

      if( !value )
         return nullptr;

      if( resultType == X_DECL_TYPE::ANY )
         return result;

      return nativeItem( { value, resultType } );
   }

   /* A call of an xDev function; PCount(), AParams(), ProcName(), ProcLine() and Unused() are built in */
   llvm::Value *callExpression( const X_EXPR &expr )
   {
      const std::string key = IIF( expr.internalCall, expr.text, s_key( expr.text ) );
      const auto typed = nativeFunctions.find( key );
      const bool isTyped = typed != nativeFunctions.end();
      VALUES items;
      std::vector<bool> byReference;
      llvm::Function *callee;
      llvm::Value *args;
      llvm::Value *result;

      if( isTyped && directNativeCall( expr ) )
         return typedCall( expr, typed->second.result );

      if( key == "PCOUNT" )
      {
         result = freshItem( "pcount" );
         storeNType( result, ( int32_t )X_IT_INTEGER );
         storePayload( result, builder.CreateZExt( currentArgc, i64Ty() ) );
         return result;
      }

      if( key == "APARAMS" )
      {
         llvm::FunctionCallee params = runtime( "x_arrayParams", voidTy(), { ptrTy(), i32Ty(), ptrTy() } );

         result = freshItem( "aparams" );
         emitCall( params, { result, currentArgc, currentArgs } );
         return result;
      }

      /* Without a level the current function is known here; ProcName( n ) reads the call trace */
      if( key == "PROCNAME" && expr.args.empty() )
         return stringItem( currentFunctionName );

      if( key == "PROCLINE" && expr.args.empty() )
         return numberItem( 0, true, expr.line );

      /* Unused( var, ... ) only tells the compiler that the variables are not used on purpose */
      if( key == "UNUSED" )
         return freshItem( "unused" );

      callee = resolveFunction( key );

      if( !callArguments( expr, isTyped, items, byReference ) )
         return nullptr;

      args = argArray( items, byReference, "call.args" );
      result = freshItem( "call.result" );
      emitCall( callee->getFunctionType(), callee, { callFrame( result, constant32( expr.args.size() ), args ) } );

      return result;
   }

   /* A typed function is called directly when every typed parameter gets a typed argument */
   bool directNativeCall( const X_EXPR &expr ) const
   {
      const std::string key = s_key( expr.text );
      const auto found = nativeFunctions.find( key );

      if( key.rfind( "__XDEV", 0 ) == 0 || lifecycleNames.count( key ) || found == nativeFunctions.end() )
         return false;

      for( size_t i = 0; i < found->second.parameters.size(); ++i )
      {
         if( found->second.parameters[i] == X_DECL_TYPE::ANY )
            continue;

         if( i >= expr.args.size() || expr.args[i]->type == X_DECL_TYPE::ANY )
            return false;
      }

      return true;
   }

   /* A typed argument: its native value, then the box of @var or NULL; @st passes the address of a STRUCT */
   bool nativeArgument( const X_EXPR &argument, X_DECL_TYPE parameter, bool byReference, VALUES &arguments )
   {
      const bool pointerParameter = parameter == X_DECL_TYPE::STRUCT || parameter == X_DECL_TYPE::PTR;
      const bool structAddress = byReference && argument.type == X_DECL_TYPE::STRUCT && pointerParameter;
      llvm::Value *reference = nullPtr();
      NATIVE_VALUE value;

      if( structAddress )
         value = NATIVE_VALUE{ nativeStructAddress( argument ), parameter };
      else
         value = nativeExpression( argument );

      if( !value.value )
         return false;

      arguments.push_back( value.value );

      if( byReference && !structAddress )
      {
         reference = nativeReference( nativeLocals.at( s_key( argument.text ) ) );
         if( !reference )
            return false;
      }

      arguments.push_back( reference );

      return true;
   }

   /* The native entry of a typed function: the call frame, then a value and a box per typed parameter;
      untyped arguments go as X_ARG items */
   llvm::Value *nativeCall( const X_EXPR &expr, llvm::Value *result )
   {
      const NATIVE_FUNCTION &callee = nativeFunctions.at( s_key( expr.text ) );
      VALUES nativeArguments;
      VALUES items;
      std::vector<bool> byReference;
      llvm::Value *args;

      items.reserve( expr.args.size() );

      for( size_t i = 0; i < expr.args.size(); ++i )
      {
         const X_DECL_TYPE parameter = IIF( i < callee.parameters.size(), callee.parameters[i], X_DECL_TYPE::ANY );
         const bool reference = i < expr.argByReference.size() && expr.argByReference[i];
         llvm::Value *item = nullPtr();

         if( parameter != X_DECL_TYPE::ANY && !nativeArgument( *expr.args[i], parameter, reference, nativeArguments ) )
            return nullptr;

         if( parameter == X_DECL_TYPE::ANY )
         {
            if( reference )
               item = referenceItem( expr.args[i]->text, expr.args[i]->line );
            else
               item = expression( *expr.args[i] );

            if( !item )
               return nullptr;
         }

         items.push_back( item );
         byReference.push_back( reference );
      }

      args = argArray( items, byReference, "native.call.args" );
      nativeArguments.insert( nativeArguments.begin(), callFrame( result, constant32( items.size() ), args ) );

      return emitCall( callee.function, nativeArguments );
   }

   /* NIL, a number, a string, a logical or an array of them */
   bool classLiteral( const X_EXPR &expr ) const
   {
      if( expr.kind == X_EXPR_KIND::NIL || expr.kind == X_EXPR_KIND::NUMBER || expr.kind == X_EXPR_KIND::STRING )
         return true;

      if( expr.kind == X_EXPR_KIND::LOGICAL )
         return true;

      if( expr.kind != X_EXPR_KIND::ARRAY )
         return false;

      for( const X_EXPR_PTR &arg : expr.args )
      {
         if( !classLiteral( *arg ) )
            return false;
      }

      return true;
   }

   /* ---------------------------------------------------------------------------------------------
      Direct method calls
      --------------------------------------------------------------------------------------------- */

   using FIELD_TYPES = std::unordered_map<std::string, std::pair<STATIC_TYPE, bool>>;
   using SLOT = std::pair<const X_CLASS_DECL *, size_t>;

   /* What collectMethods() learns about the classes of the module */
   struct METHOD_SCAN
   {
      std::unordered_map<std::string, const X_CLASS_DECL *> defs;
      std::unordered_map<std::string, std::unordered_map<std::string, SLOT>> tables;
      std::unordered_map<std::string, std::vector<std::string>> lineage;
      std::unordered_map<std::string, std::unordered_set<std::string>> bound;
      std::unordered_set<std::string> visited;
      std::unordered_map<std::string, const X_FUNCTION *> bodies;
      LOCAL_TYPES data;
   };

   /* The X_METHOD_CONTEXT and the Self item of a direct method call */
   struct METHOD_FRAME
   {
      llvm::Value *slot;
      llvm::Value *self;
   };

   /* How statements end: NEXT goes on, OWN returns Self, OTHER returns something else, STOP leaves a loop */
   enum SELF_FLOW
   {
      FLOW_NEXT = 1,
      FLOW_OWN = 2,
      FLOW_OTHER = 4,
      FLOW_STOP = 8
   };

   static bool isSelf( const X_EXPR &expr )
   {
      return expr.kind == X_EXPR_KIND::NAME && s_key( expr.text ) == "SELF";
   }

   /* Self:field */
   static bool selfField( const X_EXPR &expr )
   {
      if( expr.kind != X_EXPR_KIND::SEND || expr.sendToSuper || expr.args.size() != 1 )
         return false;

      return isSelf( *expr.args[0] );
   }

   /* Self:_field( value ) */
   static bool selfSetter( const X_EXPR &expr )
   {
      if( expr.sendToSuper || expr.args.size() != 2 || expr.text.front() != '_' )
         return false;

      return isSelf( *expr.args[0] );
   }

   static bool comparison( const std::string &op )
   {
      if( op == "=" || op == "==" || op == "!=" )
         return true;

      return op == "<" || op == "<=" || op == ">" || op == ">=";
   }

   /* Whether a method keeps the types of its locals and of the DATA of Self known, so a direct call is safe */
   struct STABILITY
   {
      const X_FUNCTION &fn;
      const FIELD_TYPES &fields;
      LOCAL_TYPES vars;

      /* The type of a local or of Self:field, and whether it may be assigned */
      std::pair<STATIC_TYPE, bool> place( const X_EXPR &expr ) const
      {
         if( expr.kind == X_EXPR_KIND::NAME )
         {
            const auto found = vars.find( s_key( expr.text ) );

            if( found != vars.end() )
               return { found->second, true };
         }

         if( selfField( expr ) )
         {
            const auto found = fields.find( s_key( expr.text ) );

            if( found != fields.end() )
               return found->second;
         }

         return { STATIC_TYPE::UNKNOWN, false };
      }

      STATIC_TYPE sendValue( const X_EXPR &expr ) const
      {
         for( bool reference : expr.argByReference )
         {
            if( reference )
               return STATIC_TYPE::UNKNOWN;
         }

         if( expr.args.size() == 1 )
            return place( expr ).first;

         if( !selfSetter( expr ) )
            return STATIC_TYPE::UNKNOWN;

         const auto found = fields.find( s_key( expr.text.substr( 1 ) ) );

         if( found != fields.end() && found->second.second && value( *expr.args[1] ) == found->second.first )
            return found->second.first;

         return STATIC_TYPE::UNKNOWN;
      }

      /* := keeps the type; += -= *= keep a number; ++ and -- need a number */
      STATIC_TYPE assignValue( const X_EXPR &expr ) const
      {
         const std::pair<STATIC_TYPE, bool> left = place( *expr.args[0] );
         const X_ASSIGN_OP op = expr.assignOp;
         STATIC_TYPE right;
         bool numbers;

         if( !left.second )
            return STATIC_TYPE::UNKNOWN;

         if( expr.kind == X_EXPR_KIND::UPDATE )
            return IIF( left.first == STATIC_TYPE::NUMERIC, STATIC_TYPE::NUMERIC, STATIC_TYPE::UNKNOWN );

         right = value( *expr.args[1] );
         if( op == X_ASSIGN_OP::SET && left.first == right )
            return right;

         numbers = left.first == STATIC_TYPE::NUMERIC && right == STATIC_TYPE::NUMERIC;
         if( numbers && ( op == X_ASSIGN_OP::ADD || op == X_ASSIGN_OP::SUB || op == X_ASSIGN_OP::MUL ) )
            return STATIC_TYPE::NUMERIC;

         return STATIC_TYPE::UNKNOWN;
      }

      STATIC_TYPE unaryValue( const X_EXPR &expr ) const
      {
         const STATIC_TYPE type = value( *expr.args[0] );

         if( expr.text == "-" && type == STATIC_TYPE::NUMERIC )
            return type;

         if( expr.text == ".NOT." && type == STATIC_TYPE::LOGICAL )
            return type;

         return STATIC_TYPE::UNKNOWN;
      }

      STATIC_TYPE binaryValue( const X_EXPR &expr ) const
      {
         const STATIC_TYPE left = value( *expr.args[0] );
         const STATIC_TYPE right = value( *expr.args[1] );
         const std::string &op = expr.text;
         const bool numbers = left == STATIC_TYPE::NUMERIC && right == STATIC_TYPE::NUMERIC;

         if( numbers && ( op == "+" || op == "-" || op == "*" ) )
            return STATIC_TYPE::NUMERIC;

         if( numbers && comparison( op ) )
            return STATIC_TYPE::LOGICAL;

         if( left == STATIC_TYPE::LOGICAL && right == STATIC_TYPE::LOGICAL && ( op == ".AND." || op == ".OR." ) )
            return STATIC_TYPE::LOGICAL;

         if( left == STATIC_TYPE::STRING && right == STATIC_TYPE::STRING && op == "+" )
            return STATIC_TYPE::STRING;

         return STATIC_TYPE::UNKNOWN;
      }

      STATIC_TYPE value( const X_EXPR &expr ) const
      {
         switch( expr.kind )
         {
         case X_EXPR_KIND::NIL:     return STATIC_TYPE::NIL;
         case X_EXPR_KIND::NUMBER:  return STATIC_TYPE::NUMERIC;
         case X_EXPR_KIND::STRING:  return STATIC_TYPE::STRING;
         case X_EXPR_KIND::LOGICAL: return STATIC_TYPE::LOGICAL;
         case X_EXPR_KIND::NAME:    return place( expr ).first;
         case X_EXPR_KIND::SEND:    return sendValue( expr );
         case X_EXPR_KIND::ASSIGN:
         case X_EXPR_KIND::UPDATE:  return assignValue( expr );
         case X_EXPR_KIND::UNARY:   return unaryValue( expr );
         case X_EXPR_KIND::BINARY:  return binaryValue( expr );
         default:                   return STATIC_TYPE::UNKNOWN;
         }
      }

      bool isParameter( const std::string &key ) const
      {
         for( const std::string &param : fn.params )
         {
            if( s_key( param ) == key )
               return true;
         }

         return false;
      }

      /* LOCAL at the head of the method: a new name, no dimensions, an initializer of known type */
      bool declare( const X_STMT &stmt )
      {
         for( const X_LOCAL_DECL &decl : stmt.locals )
         {
            const std::string key = s_key( decl.name );
            STATIC_TYPE type = STATIC_TYPE::NIL;

            if( !decl.dimensions.empty() || vars.count( key ) || isParameter( key ) )
               return false;

            if( decl.init )
               type = value( *decl.init );

            if( type == STATIC_TYPE::UNKNOWN )
               return false;

            vars.emplace( key, type );
         }

         return true;
      }

      /* RETURN, RETURN Self or RETURN of a value of known type */
      bool returnStable( const X_STMT &stmt ) const
      {
         if( !stmt.expr || isSelf( *stmt.expr ) )
            return true;

         return value( *stmt.expr ) != STATIC_TYPE::UNKNOWN;
      }

      /* The locals at the head, then statements whose types stay known up to RETURN, EXIT or LOOP */
      bool walk( const std::vector<X_STMT> &body, bool head )
      {
         for( const X_STMT &stmt : body )
         {
            if( stmt.kind == X_STMT_KIND::LOCAL && head )
            {
               if( !declare( stmt ) )
                  return false;

               continue;
            }

            head = false;

            if( stmt.kind == X_STMT_KIND::RETURN )
               return returnStable( stmt );

            if( stmt.kind == X_STMT_KIND::CALL && value( *stmt.expr ) != STATIC_TYPE::UNKNOWN )
               continue;

            if( stmt.kind == X_STMT_KIND::IF || stmt.kind == X_STMT_KIND::WHILE )
            {
               const bool logical = value( *stmt.expr ) == STATIC_TYPE::LOGICAL;

               if( logical && walk( stmt.body, false ) && walk( stmt.elseBody, false ) )
                  continue;

               return false;
            }

            return stmt.kind == X_STMT_KIND::EXIT || stmt.kind == X_STMT_KIND::LOOP;
         }

         return true;
      }
   };

   bool methodStable( const X_FUNCTION &fn, const FIELD_TYPES &fields ) const
   {
      STABILITY stability{ fn, fields, {} };

      return stability.walk( fn.body, true );
   }

   static unsigned statementFlow( const X_STMT &stmt )
   {
      switch( stmt.kind )
      {
      case X_STMT_KIND::LOCAL:
      case X_STMT_KIND::CALL:   return FLOW_NEXT;
      case X_STMT_KIND::RETURN: return IIF( stmt.expr && isSelf( *stmt.expr ), FLOW_OWN, FLOW_OTHER );
      case X_STMT_KIND::IF:     return selfFlow( stmt.body ) | selfFlow( stmt.elseBody );
      case X_STMT_KIND::WHILE:  return FLOW_NEXT | ( selfFlow( stmt.body ) & ( FLOW_OWN | FLOW_OTHER ) );
      case X_STMT_KIND::EXIT:
      case X_STMT_KIND::LOOP:   return FLOW_STOP;
      default:                  return FLOW_OTHER;
      }
   }

   static unsigned selfFlow( const std::vector<X_STMT> &body )
   {
      unsigned flow = FLOW_NEXT;

      for( const X_STMT &stmt : body )
      {
         if( !( flow & FLOW_NEXT ) )
            break;

         flow = ( flow & ~FLOW_NEXT ) | statementFlow( stmt );
      }

      return flow;
   }

   /* A method that always returns Self */
   bool methodSelf( const X_FUNCTION &fn ) const
   {
      return selfFlow( fn.body ) == FLOW_OWN;
   }

   /* The start type of a DATA: NIL, a number, a string or a logical literal */
   static STATIC_TYPE dataStartType( const X_LOCAL_DECL &decl )
   {
      if( !decl.init && decl.declaredType != X_DECL_TYPE::DATE )
         return STATIC_TYPE::NIL;

      /* A DATE without INIT starts as an empty date */
      if( !decl.init )
         return STATIC_TYPE::UNKNOWN;

      switch( decl.init->kind )
      {
      case X_EXPR_KIND::NIL:     return STATIC_TYPE::NIL;
      case X_EXPR_KIND::NUMBER:  return STATIC_TYPE::NUMERIC;
      case X_EXPR_KIND::STRING:  return STATIC_TYPE::STRING;
      case X_EXPR_KIND::LOGICAL: return STATIC_TYPE::LOGICAL;
      default:                   return STATIC_TYPE::UNKNOWN;
      }
   }

   /* The message that calls a member: the OPERATOR message, __ACCESS_name, __ONERROR, ... or the name */
   static std::string memberMessage( const X_CLASS_MEMBER &member )
   {
      if( member.operatorId >= 0 )
         return member.operatorMessage;

      if( member.kind == "ACCESS" || member.kind == "ASSIGN" )
         return "__" + member.kind + "_" + member.name;

      if( member.kind == "ONERROR" || member.kind == "DESTRUCTOR" )
         return "__" + member.kind;

      return member.name;
   }

   /* DATA and CLASSDATA that start with literals only */
   bool classPure( const X_PROGRAM &program, const X_CLASS_DECL &cls ) const
   {
      const std::string prefix = "__XDEV_" + cls.name + "_CLASSDATA_";
      bool pure = true;

      for( const X_FUNCTION &fn : program.functions )
      {
         if( fn.isDataInitializer && fn.className == cls.name )
         {
            for( const X_LOCAL_DECL &decl : fn.dataInitializers )
               pure &= !decl.init || classLiteral( *decl.init );
         }

         if( fn.isClassDataInitializer && fn.name.rfind( prefix, 0 ) == 0 )
         {
            for( const X_STMT &stmt : fn.body )
               pure &= !stmt.expr || classLiteral( *stmt.expr );
         }
      }

      return pure;
   }

   /* The lineage of the class, its message table (the first parent wins, own members win over all)
      and its hidden or non-virtual names */
   void classTable( METHOD_SCAN &scan, const X_CLASS_DECL &cls )
   {
      std::vector<std::string> &order = scan.lineage[cls.name];
      std::unordered_map<std::string, SLOT> &table = scan.tables[cls.name];
      std::unordered_set<std::string> &scoped = scan.bound[cls.name];

      for( const std::string &name : cls.parents )
      {
         const std::unordered_set<std::string> &flags = scan.bound.at( name );

         for( const std::string &owner : scan.lineage.at( name ) )
         {
            if( std::find( order.begin(), order.end(), owner ) == order.end() )
               order.push_back( owner );
         }

         scoped.insert( flags.begin(), flags.end() );
      }

      order.push_back( cls.name );

      for( auto it = cls.parents.rbegin(); it != cls.parents.rend(); ++it )
      {
         for( const auto &entry : scan.tables.at( *it ) )
            table[entry.first] = entry.second;
      }

      for( size_t i = 0; i < cls.members.size(); ++i )
      {
         const X_CLASS_MEMBER &member = cls.members[i];

         table[memberMessage( member )] = { &cls, i };

         if( member.scope & ( X_OO_CLSTP_HIDDEN | X_OO_CLSTP_NONVIRTUAL ) )
            scoped.insert( member.name );
      }
   }

   /* The DATA a method of owner may use as Self:name, with their types; a PROTECTED field only in its lineage.
      A field is settable unless READONLY or with its own setter */
   FIELD_TYPES methodFields( const METHOD_SCAN &scan, const X_CLASS_DECL &cls, const X_CLASS_DECL &owner ) const
   {
      const std::unordered_map<std::string, SLOT> &table = scan.tables.at( cls.name );
      const std::unordered_set<std::string> &scoped = scan.bound.at( cls.name );
      const std::vector<std::string> &ownerLineage = scan.lineage.at( owner.name );
      FIELD_TYPES fields;

      for( const auto &slot : table )
      {
         const X_CLASS_DECL &base = *slot.second.first;
         const X_CLASS_MEMBER &field = base.members[slot.second.second];
         const bool inherited = std::find( ownerLineage.begin(), ownerLineage.end(), base.name ) != ownerLineage.end();
         STATIC_TYPE type;
         bool settable;

         if( field.kind != "DATA" || field.name.front() == '_' || scoped.count( field.name ) )
            continue;

         if( ( field.scope & X_OO_CLSTP_PROTECTED ) && !inherited )
            continue;

         type = scan.data.at( base.name + ":" + field.name );
         if( type == STATIC_TYPE::UNKNOWN )
            continue;

         settable = !( field.scope & X_OO_CLSTP_READONLY ) && !table.count( "_" + field.name );
         settable = settable && !table.count( "__ASSIGN_" + field.name );
         fields.emplace( field.name, std::make_pair( type, settable ) );
      }

      return fields;
   }

   /* An exported METHOD that is neither hidden nor an operator gets a direct entry */
   void directMethod( const METHOD_SCAN &scan, const X_CLASS_DECL &cls, const SLOT &slot )
   {
      const X_CLASS_DECL &owner = *slot.first;
      const size_t index = slot.second;
      const X_CLASS_MEMBER &member = owner.members[index];
      const std::unordered_set<std::string> &scoped = scan.bound.at( cls.name );
      const std::vector<std::string> &order = scan.lineage.at( cls.name );

      if( member.kind != "METHOD" || member.operatorId >= 0 || member.scope != X_OO_CLSTP_EXPORTED )
         return;

      if( member.name.front() == '_' || scoped.count( member.name ) )
         return;

      const std::string symbol = "__XDEV_" + owner.name + "_METHOD_" + member.name;
      const auto pos = std::find( order.begin(), order.end(), owner.name ) - order.begin();
      const X_FUNCTION &body = *scan.bodies.at( symbol );
      const bool stable = methodStable( body, methodFields( scan, cls, owner ) );
      const auto native = nativeFunctions.find( s_key( symbol ) );
      DIRECT_METHOD method;

      method.target = functions.at( symbol );
      method.handle = static_cast<uint32_t>( index + 1 );
      method.owner = static_cast<uint32_t>( pos );
      method.stable = stable;
      method.self = stable && methodSelf( body );
      method.className = cls.name;
      method.message = member.name;
      method.native = IIF( native != nativeFunctions.end(), &native->second, nullptr );

      methods.emplace( cls.name + ":" + member.name, method );
   }

   /* A class after its parents; a parent that is not a pure class of this module stops it */
   void visitClass( const X_PROGRAM &program, METHOD_SCAN &scan, const X_CLASS_DECL &cls )
   {
      if( !scan.visited.insert( cls.name ).second )
         return;

      for( const std::string &name : cls.parents )
      {
         const auto found = scan.defs.find( name );

         if( found == scan.defs.end() )
            return;

         visitClass( program, scan, *found->second );

         if( !factories.count( name ) )
            return;
      }

      if( !classPure( program, cls ) )
         return;

      factories.insert( cls.name );
      classTable( scan, cls );

      for( const auto &entry : scan.tables[cls.name] )
         directMethod( scan, cls, entry.second );
   }

   /* Direct method calls: the classes whose DATA start with literals get a factory and a method table */
   void collectMethods( const X_PROGRAM &program )
   {
      METHOD_SCAN scan;

      for( const X_FUNCTION &fn : program.functions )
      {
         scan.bodies.emplace( fn.name, &fn );

         if( !fn.isDataInitializer )
            continue;

         for( const X_LOCAL_DECL &decl : fn.dataInitializers )
            scan.data.emplace( fn.className + ":" + s_key( decl.name ), dataStartType( decl ) );
      }

      for( const X_CLASS_DECL &cls : program.classes )
         scan.defs.emplace( cls.name, &cls );

      for( const X_CLASS_DECL &cls : program.classes )
         visitClass( program, scan, cls );
   }

   /* x_objGetClass( receiver ) must be the class, which is registered on first use and cached in x.class.NAME */
   llvm::Value *exactClass( llvm::IRBuilder<> &build, const std::string &className, llvm::Value *receiver,
                            llvm::BasicBlock *lookup, llvm::BasicBlock *check )
   {
      const auto linkage = llvm::GlobalValue::InternalLinkage;
      llvm::GlobalVariable *&cache = classHandles[className];
      llvm::BasicBlock *start;
      llvm::Value *cached;
      llvm::Value *registered;
      llvm::PHINode *expected;
      llvm::Value *actual;

      if( cache == nullptr )
         cache = new llvm::GlobalVariable( *module, i32Ty(), false, linkage, constant32( 0 ), "x.class." + className );

      start = build.GetInsertBlock();
      cached = build.CreateLoad( i32Ty(), cache );
      build.CreateCondBr( build.CreateICmpNE( cached, constant32( 0 ) ), check, lookup );

      build.SetInsertPoint( lookup );
      registered = build.CreateCall( classRegistrars.at( className ) );
      build.CreateStore( registered, cache );
      build.CreateBr( check );

      build.SetInsertPoint( check );
      expected = build.CreatePHI( i32Ty(), 2 );
      expected->addIncoming( cached, start );
      expected->addIncoming( registered, lookup );
      actual = build.CreateCall( runtime( "x_objGetClass", i32Ty(), { ptrTy() } ), { receiver } );

      return build.CreateAnd( build.CreateICmpEQ( actual, expected ), build.CreateICmpNE( expected, constant32( 0 ) ) );
   }

   /* The C++ personality of a function with landing pads */
   void setPersonality( llvm::Function *function )
   {
      llvm::Triple triple( llvm::sys::getDefaultTargetTriple() );
      const char *name = IIF( triple.isOSWindows(), "__gxx_personality_seh0", "__gxx_personality_v0" );
      llvm::FunctionCallee personality = module->getOrInsertFunction( name, llvm::FunctionType::get( i32Ty(), true ) );

      function->setPersonalityFn( llvm::cast<llvm::Constant>( personality.getCallee() ) );
   }

   /* x_objSend( ctx, result, receiver, message, argc, args ) */
   llvm::FunctionCallee objSend()
   {
      const std::vector<llvm::Type *> params = { ptrTy(), ptrTy(), ptrTy(), ptrTy(), i32Ty(), ptrTy() };

      return runtime( "x_objSend", i32Ty(), params );
   }

   llvm::FunctionCallee methodLeave()
   {
      return runtime( "x_objMethodLeave", voidTy(), { ptrTy() } );
   }

   METHOD_FRAME methodFrame( llvm::IRBuilder<> &build )
   {
      llvm::Type *frameType = llvm::ArrayType::get( llvm::Type::getInt8Ty( context ), sizeof( X_METHOD_CONTEXT ) );
      llvm::AllocaInst *slot = build.CreateAlloca( frameType, nullptr, "method.frame" );
      llvm::AllocaInst *self;

      slot->setAlignment( llvm::Align( alignof( X_METHOD_CONTEXT ) ) );
      self = build.CreateAlloca( getItemType(), nullptr, "method.self" );
      self->setAlignment( llvm::Align( alignof( X_ITEM ) ) );
      build.CreateStore( llvm::Constant::getNullValue( getItemType() ), self );

      return { slot, self };
   }

   /* x_objMethodEnter( ctx, frame, self, receiver, owner, handle ) */
   void methodEnter( llvm::IRBuilder<> &build, llvm::Function *fn, const METHOD_FRAME &frame,
                    const DIRECT_METHOD &method )
   {
      const std::vector<llvm::Type *> params = { ptrTy(), ptrTy(), ptrTy(), ptrTy(), i32Ty(), i32Ty() };
      llvm::FunctionCallee enter = runtime( "x_objMethodEnter", voidTy(), params );
      llvm::Value *owner = constant32( method.owner );
      llvm::Value *handle = constant32( method.handle );

      build.CreateCall( enter, { fn->getArg( 0 ), frame.slot, frame.self, fn->getArg( 1 ), owner, handle } );
   }

   /* The unwind block of a direct method: the method context is left, then the exception goes on */
   void methodUnwind( llvm::IRBuilder<> &build, llvm::BasicBlock *unwind, llvm::Value *frame )
   {
      llvm::LandingPadInst *pad;

      build.SetInsertPoint( unwind );
      pad = build.CreateLandingPad( exceptionType(), 0 );
      pad->setCleanup( true );
      build.CreateCall( methodLeave(), { frame } );
      build.CreateResume( pad );
   }

   /* The native entry of a typed method: ( ctx, receiver, native arguments ) around the typed body */
   llvm::Function *methodNativeEntry( DIRECT_METHOD &method )
   {
      if( method.nativeEntry != nullptr )
         return method.nativeEntry;

      llvm::Function *target = method.native->function;
      llvm::FunctionType *targetType = target->getFunctionType();
      const std::string name = method.target->getName().str() + ".native";
      std::vector<llvm::Type *> params{ ptrTy(), ptrTy() };
      llvm::FunctionType *type;
      llvm::Function *fn;
      llvm::BasicBlock *entry;
      llvm::BasicBlock *done;
      llvm::BasicBlock *unwind;
      METHOD_FRAME frame;
      VALUES args;
      llvm::Value *value;

      params.insert( params.end(), targetType->param_begin() + 1, targetType->param_end() );
      type = llvm::FunctionType::get( target->getReturnType(), params, false );
      fn = llvm::Function::Create( type, llvm::Function::InternalLinkage, name, module.get() );
      method.nativeEntry = fn;
      setPersonality( fn );

      entry = llvm::BasicBlock::Create( context, "entry", fn );
      done = llvm::BasicBlock::Create( context, "done", fn );
      unwind = llvm::BasicBlock::Create( context, "unwind", fn );

      llvm::IRBuilder<> build( entry );

      frame = methodFrame( build );
      methodEnter( build, fn, frame, method );
      args.push_back( fn->getArg( 0 ) );

      for( unsigned i = 2; i < fn->arg_size(); ++i )
         args.push_back( fn->getArg( i ) );

      value = build.CreateInvoke( target, done, unwind, args );

      build.SetInsertPoint( done );
      build.CreateCall( methodLeave(), { frame.slot } );

      if( type->getReturnType()->isVoidTy() )
         build.CreateRetVoid();
      else
         build.CreateRet( value );

      methodUnwind( build, unwind, frame.slot );

      return fn;
   }

   /* The fallback of a typed send passes a typed argument as an item */
   void storeArgument( llvm::Value *args, size_t index, const NATIVE_VALUE &value )
   {
      llvm::Value *element = builder.CreateInBoundsGEP( getArgType(), args, constant32( index ) );

      builder.CreateStore( nativeItem( value ), builder.CreateStructGEP( getArgType(), element, 0 ) );
   }

   /* A typed method of the exact class gets native arguments; any other class gets x_objSend() */
   NATIVE_VALUE nativeSend( const X_EXPR &expr, llvm::Value *result )
   {
      DIRECT_METHOD &method = *nativeSends.at( &expr );
      const NATIVE_FUNCTION &callee = *method.native;
      const size_t argc = expr.args.size() - 1;
      llvm::Value *receiver = expression( *expr.args[0] );
      std::vector<NATIVE_VALUE> typed( argc );
      VALUES items( argc, nullPtr() );
      VALUES nativeArguments;
      llvm::Value *args;
      llvm::BasicBlock *lookup;
      llvm::BasicBlock *check;
      llvm::BasicBlock *direct;
      llvm::BasicBlock *fallback;
      llvm::BasicBlock *done;
      llvm::Value *directValue;
      llvm::BasicBlock *directEnd;
      NATIVE_VALUE fallbackValue;
      llvm::BasicBlock *fallbackEnd;
      llvm::PHINode *phi;

      if( receiver == nullptr )
         return {};

      for( size_t i = 0; i < argc; ++i )
      {
         const X_EXPR &arg = *expr.args[i + 1];
         const bool isTyped = i < callee.parameters.size() && callee.parameters[i] != X_DECL_TYPE::ANY;

         if( !isTyped )
         {
            items[i] = expression( arg );
            if( items[i] == nullptr )
               return {};

            continue;
         }

         typed[i] = nativeExpression( arg );
         if( !typed[i].value )
            return {};

         nativeArguments.push_back( typed[i].value );
         nativeArguments.push_back( nullPtr() );
      }

      args = argArray( items, std::vector<bool>( argc, false ), "native.send.args" );

      lookup = llvm::BasicBlock::Create( context, "native.send.lookup", currentFunc );
      check = llvm::BasicBlock::Create( context, "native.send.check", currentFunc );
      direct = llvm::BasicBlock::Create( context, "native.send.direct", currentFunc );
      fallback = llvm::BasicBlock::Create( context, "native.send.fallback", currentFunc );
      done = llvm::BasicBlock::Create( context, "native.send.done", currentFunc );
      builder.CreateCondBr( exactClass( builder, method.className, receiver, lookup, check ), direct, fallback );

      builder.SetInsertPoint( direct );
      nativeArguments.insert( nativeArguments.begin(), { callFrame( result, constant32( argc ), args ), receiver } );
      directValue = emitCall( methodNativeEntry( method ), nativeArguments );
      directEnd = builder.GetInsertBlock();
      builder.CreateBr( done );

      builder.SetInsertPoint( fallback );

      for( size_t i = 0; i < argc; ++i )
      {
         if( typed[i].value != nullptr )
            storeArgument( args, i, typed[i] );
      }

      sendMessage( receiver, result, s_key( expr.text ), constant32( argc ), args );

      if( callee.result != X_DECL_TYPE::ANY )
         fallbackValue = nativeUnbox( result, callee.result, false );

      fallbackEnd = builder.GetInsertBlock();
      builder.CreateBr( done );

      builder.SetInsertPoint( done );

      if( callee.result == X_DECL_TYPE::ANY )
         return {};

      phi = builder.CreatePHI( directValue->getType(), 2 );
      phi->addIncoming( directValue, directEnd );
      phi->addIncoming( fallbackValue.value, fallbackEnd );

      return { phi, callee.result, nullptr, false, callee.resultTypeName };
   }

   /* Another class: x_objSend() with the result, argc and args of the call frame */
   void methodFallback( llvm::IRBuilder<> &build, llvm::Function *fn, const DIRECT_METHOD &method )
   {
      llvm::Value *ctx = fn->getArg( 0 );
      llvm::Value *result = build.CreateLoad( ptrTy(), build.CreateStructGEP( getCtxType(), ctx, 2 ) );
      llvm::Value *message = build.CreateGlobalString( s_key( method.message ) );
      llvm::Value *argc = build.CreateLoad( i32Ty(), build.CreateStructGEP( getCtxType(), ctx, 1 ) );
      llvm::Value *args = build.CreateLoad( ptrTy(), build.CreateStructGEP( getCtxType(), ctx, 0 ) );

      build.CreateCall( objSend(), { ctx, result, fn->getArg( 1 ), message, argc, args } );
      build.CreateRetVoid();
   }

   /* The direct entry of a method: the exact class calls the body, any other class sends the message */
   llvm::Function *methodEntry( DIRECT_METHOD &method )
   {
      if( method.entry != nullptr )
         return method.entry;

      llvm::FunctionType *type = llvm::FunctionType::get( voidTy(), { ptrTy(), ptrTy() }, false );
      const std::string name = method.target->getName().str() + ".direct";
      llvm::Function *fn = llvm::Function::Create( type, llvm::Function::InternalLinkage, name, module.get() );
      llvm::BasicBlock *entry = llvm::BasicBlock::Create( context, "entry", fn );
      llvm::BasicBlock *lookup = llvm::BasicBlock::Create( context, "class.lookup", fn );
      llvm::BasicBlock *check = llvm::BasicBlock::Create( context, "class.check", fn );
      llvm::BasicBlock *call = llvm::BasicBlock::Create( context, "direct", fn );
      llvm::BasicBlock *fallback = llvm::BasicBlock::Create( context, "fallback", fn );
      llvm::BasicBlock *done = llvm::BasicBlock::Create( context, "done", fn );
      llvm::BasicBlock *unwind = llvm::BasicBlock::Create( context, "unwind", fn );
      llvm::IRBuilder<> build( entry );
      METHOD_FRAME frame;
      llvm::Value *isClass;

      method.entry = fn;
      setPersonality( fn );

      frame = methodFrame( build );
      isClass = exactClass( build, method.className, fn->getArg( 1 ), lookup, check );
      build.CreateCondBr( isClass, call, fallback );

      build.SetInsertPoint( fallback );
      methodFallback( build, fn, method );

      build.SetInsertPoint( call );
      methodEnter( build, fn, frame, method );
      build.CreateInvoke( method.target, done, unwind, { fn->getArg( 0 ) } );

      build.SetInsertPoint( done );
      build.CreateCall( methodLeave(), { frame.slot } );
      build.CreateRetVoid();

      methodUnwind( build, unwind, frame.slot );

      return fn;
   }

   /* ---------------------------------------------------------------------------------------------
      Message sends
      --------------------------------------------------------------------------------------------- */

   /* What collectSends() knows while it walks a function: the locals that may keep a class, the names
      that may not, and the class held by each name ("#" stands for NIL) */
   struct SEND_SCAN
   {
      std::unordered_set<std::string> names;
      std::unordered_set<std::string> blocked;
      std::unordered_set<std::string> declared;
      std::unordered_map<std::string, std::string> known;
      std::string selfClass;
   };

   /* LOCAL, STATIC and REQUEST: the declarations at the head of a function */
   static bool headStatement( X_STMT_KIND kind )
   {
      return kind == X_STMT_KIND::LOCAL || kind == X_STMT_KIND::STATIC || kind == X_STMT_KIND::REQUEST;
   }

   /* An argument that cannot change a local */
   static bool plainArgument( const X_EXPR &arg )
   {
      switch( arg.kind )
      {
      case X_EXPR_KIND::NAME:
      case X_EXPR_KIND::NIL:
      case X_EXPR_KIND::NUMBER:
      case X_EXPR_KIND::STRING:
      case X_EXPR_KIND::LOGICAL: return true;
      default:                   return false;
      }
   }

   /* Something that may change a local: nothing is known any more */
   static std::string forget( SEND_SCAN &scan )
   {
      scan.known.clear();

      return {};
   }

   /* The free variables of a code block are blocked */
   void blockCaptured( std::unordered_set<std::string> &blocked, const X_EXPR &block )
   {
      NAME_SET bound;
      NAME_SET freeVars;

      for( const std::string &param : block.blockParams )
         bound.insert( s_key( param ) );

      for( const X_EXPR_PTR &arg : block.args )
         collectFreeVars( *arg, bound, freeVars );

      blocked.insert( freeVars.begin(), freeVars.end() );
   }

   /* The names passed by @ are blocked */
   static void blockReferences( std::unordered_set<std::string> &blocked, const X_EXPR &expr )
   {
      const size_t offset = IIF( expr.kind == X_EXPR_KIND::SEND, 1, 0 );

      for( size_t i = 0; i < expr.argByReference.size(); ++i )
      {
         if( expr.argByReference[i] )
            blocked.insert( s_key( expr.args[i + offset]->text ) );
      }
   }

   /* Locals captured by a code block or passed by @ are blocked */
   void blockExpr( SEND_SCAN &scan, const X_EXPR &expr )
   {
      if( expr.kind == X_EXPR_KIND::BLOCK )
         blockCaptured( scan.blocked, expr );

      blockReferences( scan.blocked, expr );

      for( const X_EXPR_PTR &arg : expr.args )
         blockExpr( scan, *arg );
   }

   /* A candidate is declared once, by LOCAL at the head of the function and without dimensions */
   void blockStatements( SEND_SCAN &scan, const std::vector<X_STMT> &body, bool head )
   {
      for( const X_STMT &stmt : body )
      {
         head = head && headStatement( stmt.kind );

         for( const X_LOCAL_DECL &decl : stmt.locals )
         {
            const std::string key = s_key( decl.name );
            const bool fresh = scan.declared.insert( key ).second;

            if( fresh && head && stmt.kind == X_STMT_KIND::LOCAL && decl.dimensions.empty() )
               scan.names.insert( key );
            else
               scan.blocked.insert( key );

            if( decl.init )
               blockExpr( scan, *decl.init );

            for( const X_EXPR_PTR &dim : decl.dimensions )
               blockExpr( scan, *dim );
         }

         if( stmt.expr )
            blockExpr( scan, *stmt.expr );

         if( stmt.forLimit )
            blockExpr( scan, *stmt.forLimit );

         if( stmt.forStep )
            blockExpr( scan, *stmt.forStep );

         blockStatements( scan, stmt.body, false );
         blockStatements( scan, stmt.elseBody, false );

         for( const X_CASE_BRANCH &branch : stmt.cases )
         {
            blockExpr( scan, *branch.condition );
            blockStatements( scan, branch.body, false );
         }

         blockStatements( scan, stmt.otherwiseBody, false );
      }
   }

   static std::string nameClass( const SEND_SCAN &scan, const X_EXPR &expr )
   {
      const std::string key = s_key( expr.text );
      const auto found = scan.known.find( key );

      if( found != scan.known.end() )
         return found->second;

      return IIF( key == "SELF", scan.selfClass, std::string() );
   }

   /* A call may change anything; a factory called without arguments gives its class */
   std::string callClass( SEND_SCAN &scan, const X_EXPR &expr )
   {
      const std::string key = s_key( expr.text );

      scan.known.clear();

      return IIF( expr.args.empty() && factories.count( key ), key, std::string() );
   }

   /* x := value gives a candidate that held NIL the class of the value */
   std::string assignClass( SEND_SCAN &scan, const X_EXPR &expr )
   {
      const X_EXPR &target = *expr.args[0];
      std::string key;
      bool nil;
      std::string cls;

      if( expr.assignOp != X_ASSIGN_OP::SET || target.kind != X_EXPR_KIND::NAME )
         return forget( scan );

      key = s_key( target.text );
      nil = scan.known.count( key ) && scan.known.at( key ) == "#";
      cls = classOf( scan, *expr.args[1] );

      if( nil && scan.names.count( key ) )
      {
         scan.known[key] = cls;
         return cls;
      }

      return forget( scan );
   }

   /* A send to a known class with plain or typed arguments calls the method directly; a stable method
      that returns Self gives the class again */
   std::string sendClass( SEND_SCAN &scan, const X_EXPR &expr )
   {
      const X_EXPR &receiver = *expr.args[0];
      std::string cls = classOf( scan, receiver );
      const bool untyped = cls.empty() || cls == "#";
      const bool typed = untyped && receiver.type == X_DECL_TYPE::OBJECT && !receiver.typeName.empty();
      bool plain = true;
      bool byReference = false;

      if( typed )
         cls = s_key( receiver.typeName );

      for( size_t i = 1; i < expr.args.size(); ++i )
         plain = plain && plainArgument( *expr.args[i] );

      if( !plain )
      {
         for( size_t i = 1; i < expr.args.size(); ++i )
            classOf( scan, *expr.args[i] );
      }

      for( bool reference : expr.argByReference )
         byReference = byReference || reference;

      const auto found = methods.find( cls + ":" + s_key( expr.text ) );

      if( expr.sendToSuper || !( plain || typed ) || byReference || found == methods.end() )
         return forget( scan );

      DIRECT_METHOD &method = found->second;
      const bool exact = receiver.type == X_DECL_TYPE::OBJECT && s_key( receiver.typeName ) == cls;

      sends[&expr] = methodEntry( method );

      /* A receiver declared AS this class gets native arguments, unless the method returns a STRING */
      if( method.native != nullptr && method.native->result != X_DECL_TYPE::STRING && exact )
         nativeSends[&expr] = &method;

      if( method.stable )
         return IIF( method.self, cls, std::string() );

      return forget( scan );
   }

   /* The class an expression gives: a class name, "#" for NIL or nothing */
   std::string classOf( SEND_SCAN &scan, const X_EXPR &expr )
   {
      switch( expr.kind )
      {
      case X_EXPR_KIND::NAME:    return nameClass( scan, expr );
      case X_EXPR_KIND::NIL:     return "#";
      case X_EXPR_KIND::NUMBER:
      case X_EXPR_KIND::STRING:
      case X_EXPR_KIND::LOGICAL: return {};
      case X_EXPR_KIND::CALL:    return callClass( scan, expr );
      case X_EXPR_KIND::ASSIGN:  return assignClass( scan, expr );
      case X_EXPR_KIND::SEND:    return sendClass( scan, expr );
      default:                   return forget( scan );
      }
   }

   /* LOCAL x AS CLASS or LOCAL x := value: a candidate keeps the class, any other name forgets all */
   void sendLocals( SEND_SCAN &scan, const X_STMT &stmt )
   {
      for( const X_LOCAL_DECL &decl : stmt.locals )
      {
         const std::string key = s_key( decl.name );
         std::string cls = "#";

         if( decl.init )
            cls = classOf( scan, *decl.init );

         if( decl.declaredType == X_DECL_TYPE::OBJECT )
            cls = s_key( decl.declaredTypeName );

         if( scan.names.count( key ) )
            scan.known[key] = cls;
         else
            scan.known.clear();
      }
   }

   /* The heads and the bodies of IF, loops and CASE start with nothing known */
   void sendBranches( SEND_SCAN &scan, const X_STMT &stmt )
   {
      scan.known.clear();

      for( const X_EXPR *head : { stmt.expr.get(), stmt.forLimit.get(), stmt.forStep.get() } )
      {
         if( head != nullptr )
         {
            classOf( scan, *head );
            scan.known.clear();
         }
      }

      sendStatements( scan, stmt.body );
      scan.known.clear();
      sendStatements( scan, stmt.elseBody );

      for( const X_CASE_BRANCH &branch : stmt.cases )
      {
         scan.known.clear();
         classOf( scan, *branch.condition );
         scan.known.clear();
         sendStatements( scan, branch.body );
      }

      scan.known.clear();
      sendStatements( scan, stmt.otherwiseBody );
      scan.known.clear();
   }

   /* The statements in order, up to the first RETURN */
   void sendStatements( SEND_SCAN &scan, const std::vector<X_STMT> &body )
   {
      for( const X_STMT &stmt : body )
      {
         if( stmt.kind == X_STMT_KIND::STATIC || stmt.kind == X_STMT_KIND::REQUEST )
            continue;

         if( stmt.kind == X_STMT_KIND::LOCAL )
         {
            sendLocals( scan, stmt );
            continue;
         }

         if( stmt.kind == X_STMT_KIND::CALL || stmt.kind == X_STMT_KIND::RETURN )
         {
            if( stmt.expr )
               classOf( scan, *stmt.expr );

            if( stmt.kind == X_STMT_KIND::RETURN )
               break;

            continue;
         }

         sendBranches( scan, stmt );
      }
   }

   /* Direct sends: the receiver is Self, a typed parameter, a typed expression or a local that holds
      the result of a factory */
   void collectSends( const X_FUNCTION &fn )
   {
      SEND_SCAN scan;

      for( const auto &entry : locals )
         scan.blocked.insert( entry.first );

      for( const std::string &param : fn.params )
      {
         const std::string key = s_key( param );
         const auto declared = fn.paramTypes.find( key );
         const auto named = fn.paramTypeNames.find( key );
         const bool object = declared != fn.paramTypes.end() && declared->second == X_DECL_TYPE::OBJECT;

         scan.blocked.insert( key );

         if( object && named != fn.paramTypeNames.end() )
            scan.known[key] = s_key( named->second );
      }

      scan.blocked.insert( "SELF" );

      if( !fn.className.empty() && fn.methodKind != "CLASSMETHOD" )
         scan.selfClass = s_key( fn.className );

      if( !scan.selfClass.empty() )
         scan.known["SELF"] = scan.selfClass;

      blockStatements( scan, fn.body, true );

      for( const std::string &key : scan.blocked )
         scan.names.erase( key );

      sendStatements( scan, fn.body );
   }

   /* The item of a typed send: the result itself for an untyped method, else the native value boxed */
   llvm::Value *nativeSendItem( const X_EXPR &expr )
   {
      llvm::Value *result = freshItem( "native.send.result" );
      NATIVE_VALUE value = nativeSend( expr, result );

      if( nativeSends.at( &expr )->native->result == X_DECL_TYPE::ANY )
         return result;

      return nativeItem( value );
   }

   /* The arguments of a send, @ passing the storage; the value of an assignment comes last */
   bool sendArguments( const X_EXPR &expr, llvm::Value *assignedValue, VALUES &items, std::vector<bool> &byReference )
   {
      items.reserve( expr.args.size() - 1 );
      byReference.reserve( expr.args.size() - 1 );

      for( size_t i = 1; i < expr.args.size(); ++i )
      {
         const bool isReference = argByReference( expr, i );
         llvm::Value *value;

         if( isReference )
            value = referenceItem( expr.args[i]->text, expr.args[i]->line );
         else
            value = argumentItem( expr, i );

         if( value == nullptr )
            return false;

         items.push_back( value );
         byReference.push_back( isReference );
      }

      if( assignedValue != nullptr )
      {
         items.push_back( assignedValue );
         byReference.push_back( false );
      }

      return true;
   }

   /* The method entry gets a call frame with self and the method context of the caller */
   void directSend( llvm::Function *entry, llvm::Value *receiver, llvm::Value *result, llvm::Value *argc,
                    llvm::Value *args )
   {
      llvm::Value *frame = callFrame( result, argc, args );

      for( unsigned field : { 3u, 4u } )
      {
         llvm::Value *value = builder.CreateLoad( ptrTy(), builder.CreateStructGEP( getCtxType(), callCtx, field ) );

         builder.CreateStore( value, builder.CreateStructGEP( getCtxType(), frame, field ) );
      }

      emitCall( entry, { frame, receiver } );
   }

   /* SUPER:message and Self:message in a class method: x_objSendSuper() or x_clsSendSelf() */
   void implicitSend( const char *name, llvm::Value *result, const std::string &message, llvm::Value *argc,
                      llvm::Value *args )
   {
      const std::vector<llvm::Type *> params = { ptrTy(), ptrTy(), ptrTy(), i32Ty(), ptrTy() };
      llvm::Value *text = builder.CreateGlobalString( message );

      emitCall( runtime( name, i32Ty(), params ), { callCtx, result, text, argc, args } );
   }

   /* receiver:name without parentheses goes through a cache of the DATA position */
   void dataSend( llvm::Value *receiver, llvm::Value *result, const std::string &message, llvm::Value *argc,
                  llvm::Value *args )
   {
      llvm::Type *cacheType = llvm::ArrayType::get( llvm::Type::getInt8Ty( context ), sizeof( X_DATA_CACHE ) );
      llvm::Constant *empty = llvm::Constant::getNullValue( cacheType );
      const auto linkage = llvm::GlobalValue::InternalLinkage;
      const std::vector<llvm::Type *> params = { ptrTy(), ptrTy(), ptrTy(), ptrTy(), i32Ty(), ptrTy(), ptrTy() };
      llvm::GlobalVariable *cache;
      llvm::Value *text;

      cache = new llvm::GlobalVariable( *module, cacheType, false, linkage, empty, "x.data.cache" );
      cache->setAlignment( llvm::Align( alignof( X_DATA_CACHE ) ) );
      text = builder.CreateGlobalString( message );
      emitCall( runtime( "x_objSendData", i32Ty(), params ), { callCtx, result, receiver, text, argc, args, cache } );
   }

   /* x_objSend( ctx, result, receiver, message, argc, args ) */
   void sendMessage( llvm::Value *receiver, llvm::Value *result, const std::string &message, llvm::Value *argc,
                     llvm::Value *args )
   {
      llvm::Value *text = builder.CreateGlobalString( message );

      emitCall( objSend(), { callCtx, result, receiver, text, argc, args } );
   }

   /* receiver:message( args ); savedReceiver and assignedValue serve receiver:message := value */
   llvm::Value *sendExpression( const X_EXPR &expr, llvm::Value *savedReceiver = nullptr,
                                llvm::Value *assignedValue = nullptr )
   {
      const bool ordinary = savedReceiver == nullptr && assignedValue == nullptr;
      llvm::Value *receiver = savedReceiver;
      VALUES items;
      std::vector<bool> byReference;
      bool classSelf;
      bool anyReference = false;
      llvm::Value *args;
      llvm::Value *argc;
      llvm::Value *result;
      std::string message;

      if( expr.args.empty() )
      {
         fail( expr.line, "nieprawidlowa wysylka komunikatu" );
         return nullptr;
      }

      if( ordinary && nativeSends.count( &expr ) )
         return nativeSendItem( expr );

      classSelf = currentClassMethod && isSelf( *expr.args[0] );
      if( !expr.sendToSuper && !classSelf && receiver == nullptr )
      {
         receiver = expression( *expr.args[0] );
         if( receiver == nullptr )
            return nullptr;
      }

      if( !sendArguments( expr, assignedValue, items, byReference ) )
         return nullptr;

      for( bool reference : byReference )
         anyReference = anyReference || reference;

      args = argArray( items, byReference, "send.args" );
      argc = constant32( items.size() );
      result = freshItem( "send.result" );
      message = s_key( IIF( assignedValue != nullptr, "_" + expr.text, expr.text ) );

      if( ordinary && sends.count( &expr ) )
         directSend( sends.at( &expr ), receiver, result, argc, args );
      else if( expr.sendToSuper )
         implicitSend( "x_objSendSuper", result, message, argc, args );
      else if( classSelf )
         implicitSend( "x_clsSendSelf", result, message, argc, args );
      else if( !expr.sendHasParentheses && items.size() <= 1 && !anyReference )
         dataSend( receiver, result, message, argc, args );
      else
         sendMessage( receiver, result, message, argc, args );

      return result;
   }

   /* ---------------------------------------------------------------------------------------------
      Native conversions
      --------------------------------------------------------------------------------------------- */

   bool scalarNumeric( STATIC_TYPE type ) const
   {
      return type == STATIC_TYPE::INTEGER || type == STATIC_TYPE::DOUBLE || type == STATIC_TYPE::NUMERIC;
   }

   /* x_stringStore( target, text, length ) copies a text into a STRING */
   llvm::FunctionCallee stringStoreFn()
   {
      return runtime( "x_stringStore", voidTy(), { ptrTy(), ptrTy(), i64Ty() } );
   }

   /* A typed operation that fails raises the error code; 3105 and 3107 are typed conversions */
   void nativeGuard( llvm::Value *valid, int code )
   {
      llvm::BasicBlock *pass = llvm::BasicBlock::Create( context, "typed.valid", currentFunc );
      llvm::BasicBlock *invalid = llvm::BasicBlock::Create( context, "typed.error", currentFunc );
      const char *operation = IIF( code == 3105 || code == 3107, "typed conversion", "typed arithmetic" );
      llvm::FunctionCallee raise = runtime( "x_errorRaiseNamed", i32Ty(), { ptrTy(), i32Ty(), ptrTy() } );
      llvm::Value *unused;

      builder.CreateCondBr( valid, pass, invalid );

      builder.SetInsertPoint( invalid );
      unused = freshItem( "typed.error.result" );
      emitCall( raise, { unused, constant32( code ), builder.CreateGlobalString( operation ) } );
      builder.CreateUnreachable();

      builder.SetInsertPoint( pass );
   }

   /* A STRING with a zero byte inside cannot be a PSZ */
   NATIVE_VALUE stringToPsz( llvm::Value *text )
   {
      llvm::Value *data = builder.CreateExtractValue( text, 0 );
      llvm::Value *length = builder.CreateExtractValue( text, 1 );
      llvm::FunctionCallee search = runtime( "memchr", ptrTy(), { ptrTy(), i32Ty(), i64Ty() } );
      llvm::Value *found = emitCall( search, { data, constant32( 0 ), length } );

      nativeGuard( builder.CreateIsNull( found ), 3107 );

      return { data, X_DECL_TYPE::PSZ };
   }

   /* A NULL PSZ cannot be a STRING; the text is copied into a string slot of the function */
   NATIVE_VALUE pszToString( llvm::Value *text )
   {
      llvm::Type *stringType = nativeType( X_DECL_TYPE::STRING );
      llvm::BasicBlock &entry = currentFunc->getEntryBlock();
      llvm::Value *length;
      llvm::AllocaInst *storage;

      nativeGuard( builder.CreateIsNotNull( text ), 3107 );
      length = emitCall( runtime( "strlen", i64Ty(), { ptrTy() } ), { text } );

      llvm::IRBuilder<> entryBuilder( &entry, entry.getFirstInsertionPt() );

      storage = entryBuilder.CreateAlloca( stringType, nullptr, "psz.string" );
      entryBuilder.CreateStore( llvm::Constant::getNullValue( stringType ), storage );
      stringSlots.push_back( storage );

      emitCall( stringStoreFn(), { storage, text, length } );

      return { builder.CreateLoad( stringType, storage ), X_DECL_TYPE::STRING };
   }

   /* A native value converted for a typed target: STRING and PSZ here, the rest by X_LLVM_NATIVE */
   NATIVE_VALUE nativeConvert( NATIVE_VALUE value, const X_TYPE_CONVERSION &conversion )
   {
      llvm::Value *converted;

      if( !value.value || conversion.target == X_DECL_TYPE::ANY || value.type == conversion.target )
         return value;

      if( value.type == X_DECL_TYPE::STRING && conversion.target == X_DECL_TYPE::PSZ )
         return stringToPsz( value.value );

      if( value.type == X_DECL_TYPE::PSZ && conversion.target == X_DECL_TYPE::STRING )
         return pszToString( value.value );

      converted = native.convert( value.value, value.type, conversion );
      if( !converted )
      {
         const std::string from = x_declTypeName( value.type );

         fail( srcLine, "nieobslugiwana konwersja natywna: " + from + " -> " + x_declTypeName( conversion.target ) );
      }

      return { converted, conversion.target };
   }

   /* ---------------------------------------------------------------------------------------------
      Call arguments
      --------------------------------------------------------------------------------------------- */

   /* Whether argument index of a CALL or SEND is passed by reference */
   static bool argByReference( const X_EXPR &call, size_t index )
   {
      const size_t offset = IIF( call.kind == X_EXPR_KIND::SEND, 1, 0 );

      return index >= offset && index - offset < call.argByReference.size() && call.argByReference[index - offset];
   }

   static bool isName( const X_EXPR &expr, const std::string &key )
   {
      return expr.kind == X_EXPR_KIND::NAME && s_key( expr.text ) == key;
   }

   /* Whether argument index passes the variable key by reference */
   static bool passesName( const X_EXPR &call, size_t index, const std::string &key )
   {
      return argByReference( call, index ) && s_key( call.args[index]->text ) == key;
   }

   /* Whether the expression may change the variable key: an assignment, ++, -- or @key */
   static bool writesName( const X_EXPR &expr, const std::string &key )
   {
      const bool update = expr.kind == X_EXPR_KIND::ASSIGN || expr.kind == X_EXPR_KIND::UPDATE;

      if( update && !expr.args.empty() && isName( *expr.args[0], key ) )
         return true;

      for( size_t i = 0; i < expr.args.size(); ++i )
      {
         if( passesName( expr, i, key ) || writesName( *expr.args[i], key ) )
            return true;
      }

      return false;
   }

   /* A STRING local in its own alloca, neither boxed nor shared */
   static bool stringAlloca( const NATIVE_VALUE &local )
   {
      if( local.type != X_DECL_TYPE::STRING || local.box || local.optionalBox )
         return false;

      return llvm::isa_and_nonnull<llvm::AllocaInst>( local.value );
   }

   /* A STRING item that borrows the text of a local. A local without a value gets a real empty string
      first, so callees may share its buffer */
   llvm::Value *stringView( llvm::Value *storage )
   {
      llvm::Type *stringType = nativeType( X_DECL_TYPE::STRING );
      llvm::BasicBlock &entry = currentFunc->getEntryBlock();
      llvm::IRBuilder<> entryBuilder( &entry, entry.getFirstInsertionPt() );
      llvm::AllocaInst *slot = entryBuilder.CreateAlloca( getItemType(), nullptr, uniqueName( "string.view" ) );
      llvm::BasicBlock *empty;
      llvm::BasicBlock *ready;
      llvm::Value *text;

      slot->setAlignment( llvm::Align( alignof( X_ITEM ) ) );

      empty = llvm::BasicBlock::Create( context, "string.view.empty", currentFunc );
      ready = llvm::BasicBlock::Create( context, "string.view.ready", currentFunc );
      text = builder.CreateLoad( stringType, storage );
      builder.CreateCondBr( builder.CreateIsNull( builder.CreateExtractValue( text, 0 ) ), empty, ready );

      builder.SetInsertPoint( empty );
      emitCall( stringStoreFn(), { storage, builder.CreateGlobalString( "" ), constant64( 0 ) } );
      builder.CreateBr( ready );

      builder.SetInsertPoint( ready );
      text = builder.CreateLoad( stringType, storage );
      storeNType( slot, X_IT_STRING );
      builder.CreateStore( text, builder.CreateStructGEP( getItemType(), slot, 1 ) );

      return slot;
   }

   /* A typed STRING local passed to a call without copying its text; the item is never cleared. Callees
      get arguments read-only and copy what they keep. NULL when a copy is needed: boxed, shared or static
      storage, or another argument of the call may change the variable */
   llvm::Value *nativeStringView( const X_EXPR &call, size_t index )
   {
      const X_EXPR &arg = *call.args[index];
      const X_DECL_TYPE target = IIF( arg.conversion, arg.conversion->target, X_DECL_TYPE::ANY );

      if( arg.kind != X_EXPR_KIND::NAME || ( target != X_DECL_TYPE::ANY && target != X_DECL_TYPE::STRING ) )
         return nullptr;

      const std::string key = s_key( arg.text );
      const auto found = nativeLocals.find( key );

      if( found == nativeLocals.end() || !stringAlloca( found->second ) )
         return nullptr;

      for( size_t i = 0; i < call.args.size(); ++i )
      {
         if( i != index && ( passesName( call, i, key ) || writesName( *call.args[i], key ) ) )
            return nullptr;
      }

      return stringView( found->second.value );
   }

   /* Argument index of a CALL or SEND passed by value */
   llvm::Value *argumentItem( const X_EXPR &call, size_t index )
   {
      llvm::Value *view = nativeStringView( call, index );

      if( view != nullptr )
         return view;

      return expression( *call.args[index] );
   }

   /* ---------------------------------------------------------------------------------------------
      Native values and items
      --------------------------------------------------------------------------------------------- */

   /* The type and the payload of an item that holds a native value */
   void fillNativeItem( llvm::Value *slot, const NATIVE_VALUE &value )
   {
      const X_DECL_TYPE type = value.type;
      const bool isSigned = type < X_DECL_TYPE::UINT8 || type > X_DECL_TYPE::UINT64;
      llvm::Value *payload;
      llvm::Value *data;
      llvm::Value *length;

      if( type == X_DECL_TYPE::PTR || type == X_DECL_TYPE::CALLBACK )
      {
         storeNType( slot, X_IT_POINTER );
         storePayload( slot, builder.CreatePtrToInt( value.value, i64Ty() ) );
      }
      else if( type == X_DECL_TYPE::STRING )
      {
         storeNType( slot, X_IT_STRING );
         payload = builder.CreateStructGEP( getItemType(), slot, 1 );
         data = builder.CreateExtractValue( value.value, 0 );
         length = builder.CreateExtractValue( value.value, 1 );
         emitCall( stringStoreFn(), { payload, data, length } );
      }
      else if( type == X_DECL_TYPE::LOGIC )
      {
         storeNType( slot, X_IT_LOGICAL );
         storePayload( slot, builder.CreateZExt( value.value, i64Ty() ) );
      }
      else if( value.value->getType()->isFloatingPointTy() )
      {
         storeNType( slot, X_IT_DOUBLE );
         storePayload( slot, builder.CreateFPCast( value.value, doubleTy() ) );
      }
      else
      {
         storeNType( slot, IIF( type == X_DECL_TYPE::UINT64, X_IT_UINT64, X_IT_INTEGER ) );
         storePayload( slot, builder.CreateIntCast( value.value, i64Ty(), isSigned ) );
      }
   }

   /* A native value as an item: a PSZ becomes a STRING, a STRING with an item is copied from it */
   llvm::Value *nativeItem( NATIVE_VALUE value )
   {
      llvm::Value *slot;

      if( !value.value )
         return nullptr;

      if( value.type == X_DECL_TYPE::STRUCT )
      {
         fail( srcLine, "STRUCT nie moze byc opakowany jako X_ITEM" );
         return nullptr;
      }

      if( value.type == X_DECL_TYPE::PSZ )
         return nativeItem( nativeConvert( value, { X_DECL_TYPE::STRING } ) );

      slot = freshItem( "typed.item" );

      if( value.type == X_DECL_TYPE::STRING && value.item != nullptr )
         emitCall( itemCopyFn(), { slot, value.item } );
      else
         fillNativeItem( slot, value );

      return slot;
   }

   llvm::Value *tagIs( llvm::Value *tag, int type )
   {
      return builder.CreateICmpEQ( tag, constant32( type ) );
   }

   llvm::Value *loadTag( llvm::Value *item )
   {
      return builder.CreateLoad( i32Ty(), builder.CreateStructGEP( getItemType(), item, 0 ) );
   }

   llvm::Value *loadPayload( llvm::Value *item, llvm::Type *type )
   {
      return builder.CreateLoad( type, builder.CreateStructGEP( getItemType(), item, 1 ) );
   }

   /* NIL is a NULL pointer; a cast also takes an integer that is not negative */
   NATIVE_VALUE unboxPointer( llvm::Value *item, llvm::Value *tag, X_DECL_TYPE target, bool cast )
   {
      llvm::Value *nil = tagIs( tag, X_IT_NIL );
      llvm::Value *valid = builder.CreateOr( nil, tagIs( tag, X_IT_POINTER ) );
      llvm::Value *integer = tagIs( tag, X_IT_INTEGER );
      llvm::Value *bits;
      llvm::Value *positive;

      if( cast )
         valid = builder.CreateOr( valid, builder.CreateOr( integer, tagIs( tag, X_IT_UINT64 ) ) );

      nativeGuard( valid, 3107 );
      bits = loadPayload( item, i64Ty() );

      if( cast )
      {
         positive = builder.CreateICmpSGE( bits, constant64( 0 ) );
         nativeGuard( builder.CreateOr( builder.CreateNot( integer ), positive ), 3105 );
      }

      bits = builder.CreateSelect( nil, constant64( 0 ), bits );

      return { builder.CreateIntToPtr( bits, ptrTy() ), target };
   }

   /* A number for a numeric or LOGIC target, a cast also takes a LOGICAL; the double, integer and
      unsigned payloads are converted in their own blocks */
   NATIVE_VALUE unboxNumber( llvm::Value *item, llvm::Value *tag, X_DECL_TYPE target, bool cast )
   {
      const X_TYPE_CONVERSION conversion = { target, cast, false, true };
      llvm::Value *integer = tagIs( tag, X_IT_INTEGER );
      llvm::Value *unsignedInteger = tagIs( tag, X_IT_UINT64 );
      llvm::Value *real = tagIs( tag, X_IT_DOUBLE );
      llvm::Value *logical = tagIs( tag, X_IT_LOGICAL );
      llvm::Value *valid;
      llvm::BasicBlock *realBB;
      llvm::BasicBlock *intBB;
      llvm::BasicBlock *unsignedBB;
      llvm::BasicBlock *integralBB;
      llvm::BasicBlock *done;
      NATIVE_VALUE realValue;
      NATIVE_VALUE intValue;
      NATIVE_VALUE unsignedValue;
      llvm::BasicBlock *realEnd;
      llvm::BasicBlock *intEnd;
      llvm::BasicBlock *unsignedEnd;
      llvm::Value *bits;
      llvm::PHINode *value;

      if( target == X_DECL_TYPE::LOGIC && !cast )
      {
         nativeGuard( logical, 3107 );
         return { logicalPayload( item ), target };
      }

      valid = builder.CreateOr( builder.CreateOr( integer, unsignedInteger ), real );
      if( cast )
         valid = builder.CreateOr( valid, logical );

      nativeGuard( valid, 3107 );

      realBB = llvm::BasicBlock::Create( context, "typed.double", currentFunc );
      intBB = llvm::BasicBlock::Create( context, "typed.integer", currentFunc );
      unsignedBB = llvm::BasicBlock::Create( context, "typed.unsigned", currentFunc );
      integralBB = llvm::BasicBlock::Create( context, "typed.integral", currentFunc );
      done = llvm::BasicBlock::Create( context, "typed.unbox", currentFunc );
      builder.CreateCondBr( real, realBB, integralBB );

      builder.SetInsertPoint( integralBB );
      builder.CreateCondBr( unsignedInteger, unsignedBB, intBB );

      builder.SetInsertPoint( realBB );
      realValue = nativeConvert( { loadPayload( item, doubleTy() ), X_DECL_TYPE::FLOAT64 }, conversion );
      if( !realValue.value )
         return {};

      realEnd = builder.GetInsertBlock();
      builder.CreateBr( done );

      builder.SetInsertPoint( intBB );
      bits = loadPayload( item, i64Ty() );
      if( cast )
         bits = builder.CreateSelect( logical, builder.CreateZExt( logicalPayload( item ), i64Ty() ), bits );

      intValue = nativeConvert( { bits, X_DECL_TYPE::INT64 }, conversion );
      if( !intValue.value )
         return {};

      intEnd = builder.GetInsertBlock();
      builder.CreateBr( done );

      builder.SetInsertPoint( unsignedBB );
      unsignedValue = nativeConvert( { loadPayload( item, i64Ty() ), X_DECL_TYPE::UINT64 }, conversion );
      if( !unsignedValue.value )
         return {};

      unsignedEnd = builder.GetInsertBlock();
      builder.CreateBr( done );

      builder.SetInsertPoint( done );
      value = builder.CreatePHI( nativeType( target ), 3 );
      value->addIncoming( realValue.value, realEnd );
      value->addIncoming( intValue.value, intEnd );
      value->addIncoming( unsignedValue.value, unsignedEnd );

      return { value, target };
   }

   /* The native value of an item for a typed target; an item of a wrong type raises error 3107 */
   NATIVE_VALUE nativeUnbox( llvm::Value *item, X_DECL_TYPE target, bool cast )
   {
      llvm::Value *tag;
      llvm::Value *text;

      if( !item )
         return {};

      tag = loadTag( item );

      if( target == X_DECL_TYPE::PSZ || target == X_DECL_TYPE::STRING )
      {
         nativeGuard( tagIs( tag, X_IT_STRING ), 3107 );
         text = loadPayload( item, nativeType( X_DECL_TYPE::STRING ) );
         return nativeConvert( { text, X_DECL_TYPE::STRING }, { target } );
      }

      if( target == X_DECL_TYPE::PTR || target == X_DECL_TYPE::CALLBACK )
         return unboxPointer( item, tag, target, cast );

      return unboxNumber( item, tag, target, cast );
   }

   /* ---------------------------------------------------------------------------------------------
      Callbacks
      --------------------------------------------------------------------------------------------- */

   /* The declared type of each CALLBACK parameter */
   bool callbackParamTypes( const X_CALLBACK_DECL &signature, int line, std::vector<X_DECL_TYPE> &types )
   {
      for( const std::string &name : signature.params )
      {
         const auto parameter = signature.paramTypes.find( s_key( name ) );

         if( parameter == signature.paramTypes.end() )
         {
            fail( line, "brak typu parametru CALLBACK " + signature.name + ": " + name );
            return false;
         }

         types.push_back( parameter->second );
      }

      return true;
   }

   /* The trampoline of a CALLBACK signature for a target function; its body is generated later */
   llvm::Function *callbackAddress( const X_EXPR &expr )
   {
      const std::string signatureKey = s_key( expr.typeName );
      const std::string targetKey = s_key( expr.text );
      const std::string key = signatureKey + ":" + targetKey;
      const std::string name = "x.callback." + signatureKey + "." + targetKey;
      const auto existing = callbackTrampolines.find( key );
      const auto declared = callbackTypes.find( signatureKey );
      std::vector<X_DECL_TYPE> types;
      std::vector<llvm::Type *> parameters;
      llvm::Type *result;
      llvm::FunctionType *type;
      llvm::Function *trampoline;

      if( existing != callbackTrampolines.end() )
         return existing->second;

      if( declared == callbackTypes.end() || !functions.count( targetKey ) )
      {
         fail( expr.line, "brak deklaracji CALLBACK albo funkcji docelowej: " + expr.text );
         return nullptr;
      }

      const X_CALLBACK_DECL &signature = *declared->second;

      if( !callbackParamTypes( signature, expr.line, types ) )
         return nullptr;

      for( X_DECL_TYPE parameter : types )
         parameters.push_back( callbackType( parameter ) );

      if( !signature.isProcedure && !signature.resultType )
      {
         fail( expr.line, "brak typu wyniku CALLBACK: " + signature.name );
         return nullptr;
      }

      result = IIF( signature.isProcedure, voidTy(), callbackType( *signature.resultType ) );
      type = llvm::FunctionType::get( result, parameters, false );
      trampoline = llvm::Function::Create( type, llvm::Function::InternalLinkage, name, module.get() );
      callbackTrampolines.emplace( key, trampoline );
      callbackJobs.push_back( { trampoline, &signature, targetKey } );

      return trampoline;
   }

   /* The item type of a native CALLBACK argument */
   static int callbackTag( X_DECL_TYPE type )
   {
      switch( type )
      {
      case X_DECL_TYPE::PTR:     return X_IT_POINTER;
      case X_DECL_TYPE::LOGIC:   return X_IT_LOGICAL;
      case X_DECL_TYPE::UINT64:  return X_IT_UINT64;
      case X_DECL_TYPE::FLOAT32:
      case X_DECL_TYPE::FLOAT64: return X_IT_DOUBLE;
      default:                   return X_IT_INTEGER;
      }
   }

   /* The 64-bit payload of a native CALLBACK argument */
   llvm::Value *callbackPayload( llvm::IRBuilder<> &code, X_DECL_TYPE type, llvm::Value *argument )
   {
      const bool isSigned = type < X_DECL_TYPE::UINT8 || type > X_DECL_TYPE::UINT64;

      switch( type )
      {
      case X_DECL_TYPE::PTR:     return code.CreatePtrToInt( argument, i64Ty() );
      case X_DECL_TYPE::LOGIC:   return code.CreateZExt( code.CreateICmpNE( argument, constant32( 0 ) ), i64Ty() );
      case X_DECL_TYPE::FLOAT32: return code.CreateBitCast( code.CreateFPExt( argument, doubleTy() ), i64Ty() );
      case X_DECL_TYPE::FLOAT64: return code.CreateBitCast( argument, i64Ty() );
      default:                   return code.CreateIntCast( argument, i64Ty(), isSigned );
      }
   }

   /* A numeric, LOGIC or pointer argument as an item */
   void callbackItem( llvm::IRBuilder<> &code, llvm::Value *item, X_DECL_TYPE type, llvm::Value *argument )
   {
      llvm::Value *payload;

      code.CreateStore( constant32( callbackTag( type ) ), code.CreateStructGEP( getItemType(), item, 0 ) );
      payload = callbackPayload( code, type, argument );
      code.CreateStore( payload, code.CreateStructGEP( getItemType(), item, 1 ) );
   }

   /* Argument index of a trampoline as an item, passed by value; a PSZ is copied by x_callbackPutString() */
   void callbackArgument( llvm::IRBuilder<> &code, llvm::Value *items, llvm::Value *arguments, size_t index,
                          X_DECL_TYPE type, llvm::Value *argument )
   {
      llvm::Value *number = constant32( index );
      llvm::Value *item = code.CreateInBoundsGEP( getItemType(), items, number );
      llvm::Value *arg;

      code.CreateStore( llvm::Constant::getNullValue( getItemType() ), item );

      if( type == X_DECL_TYPE::PSZ )
         code.CreateCall( runtime( "x_callbackPutString", voidTy(), { ptrTy(), ptrTy() } ), { item, argument } );
      else
         callbackItem( code, item, type, argument );

      arg = code.CreateInBoundsGEP( getArgType(), arguments, number );
      code.CreateStore( item, code.CreateStructGEP( getArgType(), arg, 0 ) );
      code.CreateStore( constant32( 0 ), code.CreateStructGEP( getArgType(), arg, 1 ) );
   }

   /* The native result of a trampoline, taken from the result item */
   llvm::Value *callbackResult( llvm::IRBuilder<> &code, llvm::Value *result, X_DECL_TYPE type )
   {
      llvm::Value *payload = code.CreateStructGEP( getItemType(), result, 1 );

      if( type == X_DECL_TYPE::FLOAT64 )
         return code.CreateLoad( doubleTy(), payload );

      if( type == X_DECL_TYPE::FLOAT32 )
         return code.CreateFPTrunc( code.CreateLoad( doubleTy(), payload ), nativeType( type ) );

      if( type == X_DECL_TYPE::PTR )
         return code.CreateIntToPtr( code.CreateLoad( i64Ty(), payload ), ptrTy() );

      return code.CreateIntCast( code.CreateLoad( i64Ty(), payload ), callbackType( type ), false );
   }

   /* The trampoline puts the native arguments into items, calls the target by x_callbackInvoke() and returns
      its result as a native value; the items are cleared on both paths */
   bool generateCallbackTrampoline( const CALLBACK_JOB &job )
   {
      const X_CALLBACK_DECL &signature = *job.signature;
      const size_t count = signature.params.size();
      llvm::FunctionCallee invoke = runtime( "x_callbackInvoke", i32Ty(), { ptrTy(), ptrTy(), i32Ty(), ptrTy() } );
      std::vector<X_DECL_TYPE> types;
      llvm::IRBuilder<> code( context );
      llvm::BasicBlock *entry;
      llvm::BasicBlock *success;
      llvm::BasicBlock *cleanup;
      llvm::AllocaInst *result;
      llvm::Value *items = nullPtr();
      llvm::Value *arguments = nullPtr();
      llvm::AllocaInst *returned = nullptr;
      llvm::Value *target;
      llvm::Value *called;
      size_t index = 0;

      if( job.trampoline->arg_size() != count )
      {
         fail( signature.line, "niezgodna liczba parametrow trampoliny CALLBACK: " + signature.name );
         return false;
      }

      if( !callbackParamTypes( signature, signature.line, types ) )
         return false;

      entry = llvm::BasicBlock::Create( context, "entry", job.trampoline );
      success = llvm::BasicBlock::Create( context, "callback.success", job.trampoline );
      cleanup = llvm::BasicBlock::Create( context, "callback.cleanup", job.trampoline );

      code.SetInsertPoint( entry );
      result = code.CreateAlloca( getItemType(), nullptr, "callback.result" );
      result->setAlignment( llvm::Align( alignof( X_ITEM ) ) );
      code.CreateStore( llvm::Constant::getNullValue( getItemType() ), result );

      if( count != 0 )
      {
         items = code.CreateAlloca( getItemType(), constant32( count ), "callback.items" );
         arguments = code.CreateAlloca( getArgType(), constant32( count ), "callback.arguments" );
      }

      for( llvm::Argument &argument : job.trampoline->args() )
      {
         callbackArgument( code, items, arguments, index, types[index], &argument );
         ++index;
      }

      if( !signature.isProcedure )
      {
         llvm::Type *type = callbackType( *signature.resultType );

         returned = code.CreateAlloca( type, nullptr, "callback.return" );
         code.CreateStore( llvm::Constant::getNullValue( type ), returned );
      }

      target = functions.at( job.target );
      called = code.CreateCall( invoke, { target, result, constant32( count ), arguments } );
      code.CreateCondBr( code.CreateICmpNE( called, constant32( 0 ) ), success, cleanup );

      code.SetInsertPoint( success );
      if( returned )
         code.CreateStore( callbackResult( code, result, *signature.resultType ), returned );

      code.CreateBr( cleanup );

      code.SetInsertPoint( cleanup );

      for( size_t i = 0; i < count; ++i )
         code.CreateCall( itemClearFn(), { code.CreateInBoundsGEP( getItemType(), items, constant32( i ) ) } );

      code.CreateCall( itemClearFn(), { result } );

      if( returned )
         code.CreateRet( code.CreateLoad( callbackType( *signature.resultType ), returned ) );
      else
         code.CreateRetVoid();

      return true;
   }

   /* ---------------------------------------------------------------------------------------------
      Native expressions
      --------------------------------------------------------------------------------------------- */

   /* STRUCT:_field := value */
   static bool fieldSetter( const X_EXPR &expr )
   {
      if( expr.kind != X_EXPR_KIND::SEND || !expr.structFieldAccess || expr.args.size() != 2 )
         return false;

      return !expr.text.empty() && expr.text[0] == '_';
   }

   /* STRUCT:field or an element of a STRUCT array field */
   static bool fieldRead( const X_EXPR &expr )
   {
      if( expr.kind == X_EXPR_KIND::INDEX )
         return expr.structArrayIndex;

      return expr.kind == X_EXPR_KIND::SEND && expr.structFieldAccess && expr.type != X_DECL_TYPE::ARRAY;
   }

   /* A number, a logical and a CAST have a native type of their own */
   static bool ownNativeType( X_EXPR_KIND kind )
   {
      return kind == X_EXPR_KIND::NUMBER || kind == X_EXPR_KIND::LOGICAL || kind == X_EXPR_KIND::CAST;
   }

   /* A send to a typed method with a native result */
   bool typedSend( const X_EXPR &expr ) const
   {
      const auto found = nativeSends.find( &expr );

      if( expr.kind != X_EXPR_KIND::SEND || found == nativeSends.end() )
         return false;

      return found->second->native->result != X_DECL_TYPE::ANY;
   }

   /* The value with the conversion asked for by the type checker */
   NATIVE_VALUE withConversion( const X_EXPR &expr, const NATIVE_VALUE &value )
   {
      if( !expr.conversion )
         return value;

      return nativeConvert( value, *expr.conversion );
   }

   /* STRUCT:_field := value stores the converted value and gives it */
   NATIVE_VALUE nativeFieldStore( const X_EXPR &expr )
   {
      llvm::Value *address = nativeStructAddress( expr );
      NATIVE_VALUE value = nativeExpression( *expr.args[1] );
      llvm::Type *type = nativeType( expr.type, expr.typeName );

      if( !address || !type || !value.value )
         return {};

      value = nativeConvert( value, { expr.type, false, false, true } );
      builder.CreateStore( value.value, address );

      return value;
   }

   NATIVE_VALUE nativeFieldLoad( const X_EXPR &expr )
   {
      llvm::Value *address = nativeStructAddress( expr );
      llvm::Type *type = nativeType( expr.type, expr.typeName );

      if( !address || !type )
         return {};

      return { builder.CreateLoad( type, address ), expr.type, nullptr, false, expr.typeName };
   }

   /* An expression without a native type: its item is unboxed into the conversion target or its own type */
   NATIVE_VALUE nativeAny( const X_EXPR &expr )
   {
      X_DECL_TYPE target = expr.type;
      bool cast = false;

      if( expr.conversion && nativeType( expr.conversion->target ) )
         target = expr.conversion->target;

      if( expr.conversion )
         cast = expr.conversion->explicitCast;

      if( !nativeType( target ) )
      {
         fail( expr.line, "wyrazenie ANY wymaga znanego typu docelowego" );
         return {};
      }

      return nativeUnbox( itemExpression( expr ), target, cast );
   }

   /* A number literal is INT64 or FLOAT64; an integer above INT64_MAX is UINT64 */
   NATIVE_VALUE nativeNumber( const X_EXPR &expr )
   {
      llvm::StringRef spelling( expr.text );
      const bool hex = spelling.starts_with_insensitive( "0x" );
      uint64_t exact;

      if( hex )
         spelling = spelling.drop_front( 2 );

      if( !spelling.empty() && !spelling.getAsInteger( IIF( hex, 16, 10 ), exact ) && exact > uint64_t( INT64_MAX ) )
         return { constant64( exact ), X_DECL_TYPE::UINT64 };

      if( expr.numberIsInteger )
         return { llvm::ConstantInt::get( i64Ty(), expr.integer, true ), X_DECL_TYPE::INT64 };

      return { llvm::ConstantFP::get( doubleTy(), expr.number ), X_DECL_TYPE::FLOAT64 };
   }

   /* A typed function is called directly, any other call is unboxed */
   NATIVE_VALUE nativeCallValue( const X_EXPR &expr )
   {
      if( !directNativeCall( expr ) )
         return nativeUnbox( callExpression( expr ), expr.type, false );

      const NATIVE_FUNCTION &callee = nativeFunctions.at( s_key( expr.text ) );
      llvm::Value *scratch = freshItem( "native.call.context.result" );

      return { nativeCall( expr, scratch ), callee.result, nullptr, false, callee.resultTypeName };
   }

   /* Constants, a CAST and a CALL always give a value, checked by nativeExpression() */
   bool nativeOperand( const X_EXPR &expr, NATIVE_VALUE &result )
   {
      if( expr.kind == X_EXPR_KIND::STRING )
         result = nativeUnbox( stringItem( expr.text ), X_DECL_TYPE::STRING, false );
      else if( expr.kind == X_EXPR_KIND::NUMBER )
         result = nativeNumber( expr );
      else if( expr.kind == X_EXPR_KIND::LOGICAL )
         result = { llvm::ConstantInt::get( nativeType( X_DECL_TYPE::LOGIC ), expr.logical ), X_DECL_TYPE::LOGIC };
      else if( expr.kind == X_EXPR_KIND::FUNCTION_ADDRESS )
         result = { callbackAddress( expr ), X_DECL_TYPE::CALLBACK, nullptr, false, expr.typeName };
      else if( expr.kind == X_EXPR_KIND::CAST )
         result = nativeExpression( *expr.args[0] );
      else
         result = nativeCallValue( expr );

      return true;
   }

   bool nativeName( const X_EXPR &expr, NATIVE_VALUE &result )
   {
      const auto found = nativeLocals.find( s_key( expr.text ) );

      if( found == nativeLocals.end() )
      {
         fail( expr.line, "blad wewnetrzny: brak natywnego magazynu rozstrzygnietej zmiennej " + expr.text );
         return false;
      }

      result = nativeLoad( found->second );

      return true;
   }

   /* ( a, b, c ): the expressions before the last one are evaluated for their effects */
   bool nativeGroup( const X_EXPR &expr, NATIVE_VALUE &result )
   {
      for( const X_EXPR_PTR &arg : expr.args )
      {
         if( arg.get() == expr.args.back().get() )
            result = nativeExpression( *arg );
         else if( !expression( *arg ) )
            return false;
      }

      return true;
   }

   bool nativeUnary( const X_EXPR &expr, NATIVE_VALUE &result )
   {
      const NATIVE_VALUE value = nativeExpression( *expr.args[0] );

      if( !value.value )
         return false;

      if( expr.text == "+" )
         result = { value.value, expr.type };
      else if( expr.text == ".NOT." )
         result = { builder.CreateNot( value.value ), expr.type };
      else
         result = { native.negate( value.value, value.type ), expr.type };

      return true;
   }

   /* .AND. and .OR. evaluate the right operand only when it decides */
   bool nativeLogical( const X_EXPR &expr, llvm::Value *left, NATIVE_VALUE &result )
   {
      llvm::BasicBlock *before = builder.GetInsertBlock();
      llvm::BasicBlock *rightBB = llvm::BasicBlock::Create( context, "typed.right", currentFunc );
      llvm::BasicBlock *done = llvm::BasicBlock::Create( context, "typed.logical", currentFunc );
      NATIVE_VALUE right;
      llvm::BasicBlock *rightEnd;
      llvm::PHINode *value;

      if( expr.text == ".AND." )
         builder.CreateCondBr( left, rightBB, done );
      else
         builder.CreateCondBr( left, done, rightBB );

      builder.SetInsertPoint( rightBB );
      right = nativeExpression( *expr.args[1] );
      if( !right.value )
         return false;

      rightEnd = builder.GetInsertBlock();
      builder.CreateBr( done );

      builder.SetInsertPoint( done );
      value = builder.CreatePHI( nativeType( X_DECL_TYPE::LOGIC ), 2 );
      value->addIncoming( left, before );
      value->addIncoming( right.value, rightEnd );
      result = { value, X_DECL_TYPE::LOGIC };

      return true;
   }

   /* A comparison gives a LOGIC, any other operator a value of the operation type */
   bool nativeBinary( const X_EXPR &expr, NATIVE_VALUE &result )
   {
      const NATIVE_VALUE left = nativeExpression( *expr.args[0] );
      NATIVE_VALUE right;

      if( !left.value )
         return false;

      if( expr.text == ".AND." || expr.text == ".OR." )
         return nativeLogical( expr, left.value, result );

      right = nativeExpression( *expr.args[1] );
      if( !right.value )
         return false;

      if( expr.type == X_DECL_TYPE::LOGIC )
         result = { native.compare( left.value, right.value, expr.operationType, expr.text ), expr.type };
      else
         result = { native.arithmetic( left.value, right.value, expr.operationType, expr.text ), expr.type };

      return true;
   }

   /* The storage of an assignment: a native local or a STRUCT field */
   NATIVE_VALUE nativeTarget( const X_EXPR &target )
   {
      if( target.kind == X_EXPR_KIND::NAME )
      {
         const auto found = nativeLocals.find( s_key( target.text ) );

         if( found != nativeLocals.end() )
            return found->second;

         return {};
      }

      if( target.structFieldAccess || target.structArrayIndex )
         return { nativeStructAddress( target ), target.type, nullptr, false, target.typeName };

      return {};
   }

   /* The operator of a compound assignment */
   static const char *assignOperator( X_ASSIGN_OP op )
   {
      switch( op )
      {
      case X_ASSIGN_OP::ADD: return "+";
      case X_ASSIGN_OP::SUB: return "-";
      case X_ASSIGN_OP::MUL: return "*";
      case X_ASSIGN_OP::DIV: return "/";
      case X_ASSIGN_OP::MOD: return "%";
      default:               return "^";
      }
   }

   /* The runtime function of a compound assignment of items */
   static const char *assignFunction( X_ASSIGN_OP op )
   {
      switch( op )
      {
      case X_ASSIGN_OP::ADD: return "x_itemAdd";
      case X_ASSIGN_OP::SUB: return "x_itemSub";
      case X_ASSIGN_OP::MUL: return "x_itemMul";
      case X_ASSIGN_OP::DIV: return "x_itemDiv";
      case X_ASSIGN_OP::MOD: return "x_itemMod";
      default:               return "x_itemPow";
      }
   }

   /* A STRING in its own alloca or global, neither boxed nor shared, whose text has an item */
   static bool appendable( const NATIVE_VALUE &storage, const NATIVE_VALUE &previous )
   {
      if( storage.type != X_DECL_TYPE::STRING || storage.box || storage.optionalBox || previous.item == nullptr )
         return false;

      return llvm::isa<llvm::AllocaInst>( storage.value ) || llvm::isa<llvm::GlobalVariable>( storage.value );
   }

   /* x += value of a STRING: the runtime appends to the text in place and gives the new text */
   NATIVE_VALUE stringAddAssign( const NATIVE_VALUE &storage, const NATIVE_VALUE &previous, llvm::Value *left,
                                 llvm::Value *right )
   {
      const std::vector<llvm::Type *> params = { ptrTy(), ptrTy(), ptrTy(), ptrTy(), ptrTy(), ptrTy() };
      llvm::FunctionCallee append = runtime( "x_stringAddAssign", i32Ty(), params );
      llvm::Value *sum = freshItem( "compound.result" );
      NATIVE_VALUE result;

      emitCall( append, { callCtx, sum, storage.value, previous.item, left, right } );

      result = nativeUnbox( sum, storage.type, false );
      result.item = sum;
      result.typeName = storage.typeName;

      return result;
   }

   /* ++ and -- add or subtract one in the operation type */
   NATIVE_VALUE nativeStep( const X_EXPR &expr, const NATIVE_VALUE &previous )
   {
      const X_EXPR &target = *expr.args[0];
      const NATIVE_VALUE promoted = IIF( target.conversion, nativeConvert( previous, *target.conversion ), previous );
      llvm::Type *type = promoted.value->getType();
      const char *op = IIF( expr.updateOp == X_UPDATE_OP::INC, "+", "-" );
      llvm::Value *one;

      if( type->isFloatingPointTy() )
         one = llvm::ConstantFP::get( type, 1.0 );
      else
         one = llvm::ConstantInt::get( type, 1 );

      return { native.arithmetic( promoted.value, one, expr.operationType, op ), expr.operationType };
   }

   /* The assigned value is converted, stored and given in the type of the variable; x++ and x-- give
      the previous value */
   bool nativeFinish( const X_EXPR &expr, const NATIVE_VALUE &storage, const NATIVE_VALUE &previous,
                      NATIVE_VALUE &result )
   {
      if( expr.storeConversion && !expr.storeConversion->checkType )
         result = nativeConvert( result, *expr.storeConversion );

      if( !result.value )
         return false;

      nativeStore( storage, result.value );
      result.type = storage.type;
      result.typeName = storage.typeName;

      if( expr.kind == X_EXPR_KIND::UPDATE && expr.updateMode == X_UPDATE_MODE::POSTFIX )
         result = previous;

      return true;
   }

   /* x op= value of an ANY or STRING operation goes through items; x += value of a STRING variable
      appends in place and is stored already */
   bool compoundItem( const X_EXPR &expr, const NATIVE_VALUE &storage, NATIVE_VALUE &result )
   {
      const NATIVE_VALUE previous = nativeLoad( storage );
      llvm::Value *left = nativeItem( previous );
      llvm::Value *right = expression( *expr.args[1] );

      if( !left || !right )
         return false;

      if( expr.assignOp == X_ASSIGN_OP::ADD && appendable( storage, previous ) )
      {
         result = stringAddAssign( storage, previous, left, right );
         return true;
      }

      result = nativeUnbox( operatorCall( assignFunction( expr.assignOp ), left, right ), storage.type, false );

      return nativeFinish( expr, storage, {}, result );
   }

   /* x := value, x op= value, ++x and x++ of a native variable or a STRUCT field */
   bool nativeAssign( const X_EXPR &expr, NATIVE_VALUE &result )
   {
      const X_EXPR &target = *expr.args[0];
      const bool update = expr.kind == X_EXPR_KIND::UPDATE;
      const bool compound = !update && expr.assignOp != X_ASSIGN_OP::SET;
      const bool itemType = expr.operationType == X_DECL_TYPE::ANY || expr.operationType == X_DECL_TYPE::STRING;
      const NATIVE_VALUE storage = nativeTarget( target );
      NATIVE_VALUE previous;

      if( ( !storage.value && !storage.box ) || ( update && target.kind != X_EXPR_KIND::NAME ) )
      {
         fail( expr.line, "natywne przypisanie wymaga zmiennej lub pola STRUCT" );
         return false;
      }

      if( update )
      {
         previous = nativeLoad( storage );
         result = nativeStep( expr, previous );
         return nativeFinish( expr, storage, previous, result );
      }

      if( compound && itemType )
         return compoundItem( expr, storage, result );

      if( compound )
         previous = nativeExpression( target );

      result = nativeExpression( *expr.args[1] );

      if( compound && previous.value && result.value )
      {
         const char *op = assignOperator( expr.assignOp );

         result = { native.arithmetic( previous.value, result.value, expr.operationType, op ), expr.operationType };
      }

      return nativeFinish( expr, storage, previous, result );
   }

   /* IIF() of native values: both branches give the native type of the IIF() */
   bool nativeIif( const X_EXPR &expr, NATIVE_VALUE &result )
   {
      llvm::Value *condition = scalarCondition( *expr.args[0] );
      llvm::BasicBlock *trueBB;
      llvm::BasicBlock *falseBB;
      llvm::BasicBlock *done;
      NATIVE_VALUE whenTrue;
      NATIVE_VALUE whenFalse;
      llvm::BasicBlock *trueEnd;
      llvm::BasicBlock *falseEnd;
      llvm::PHINode *value;

      if( !condition )
         return false;

      trueBB = llvm::BasicBlock::Create( context, "typed.iif.true", currentFunc );
      falseBB = llvm::BasicBlock::Create( context, "typed.iif.false", currentFunc );
      done = llvm::BasicBlock::Create( context, "typed.iif.done", currentFunc );
      builder.CreateCondBr( condition, trueBB, falseBB );

      builder.SetInsertPoint( trueBB );
      whenTrue = nativeExpression( *expr.args[1] );
      if( !whenTrue.value )
         return false;

      trueEnd = builder.GetInsertBlock();
      builder.CreateBr( done );

      builder.SetInsertPoint( falseBB );
      whenFalse = nativeExpression( *expr.args[2] );
      if( !whenFalse.value )
         return false;

      falseEnd = builder.GetInsertBlock();
      builder.CreateBr( done );

      builder.SetInsertPoint( done );
      value = builder.CreatePHI( nativeType( expr.type ), 2 );
      value->addIncoming( whenTrue.value, trueEnd );
      value->addIncoming( whenFalse.value, falseEnd );
      result = { value, expr.type };

      return true;
   }

   /* The native value by the kind of the expression; false when the error is reported already */
   bool nativeByKind( const X_EXPR &expr, NATIVE_VALUE &result )
   {
      switch( expr.kind )
      {
      case X_EXPR_KIND::STRING:
      case X_EXPR_KIND::NUMBER:
      case X_EXPR_KIND::LOGICAL:
      case X_EXPR_KIND::FUNCTION_ADDRESS:
      case X_EXPR_KIND::CAST:
      case X_EXPR_KIND::CALL:   return nativeOperand( expr, result );
      case X_EXPR_KIND::NAME:   return nativeName( expr, result );
      case X_EXPR_KIND::GROUP:  return nativeGroup( expr, result );
      case X_EXPR_KIND::UNARY:  return nativeUnary( expr, result );
      case X_EXPR_KIND::BINARY: return nativeBinary( expr, result );
      case X_EXPR_KIND::ASSIGN:
      case X_EXPR_KIND::UPDATE: return nativeAssign( expr, result );
      case X_EXPR_KIND::IIF:    return nativeIif( expr, result );
      default:                  break;
      }

      fail( expr.line, "wyrazenie natywne wymaga kolejnego etapu backendu" );

      return false;
   }

   /* The native value of an expression: STRUCT fields, STRING operations and typed sends first, then an
      expression without a native type, then the value by kind */
   NATIVE_VALUE nativeExpression( const X_EXPR &expr )
   {
      llvm::SaveAndRestore<int> lineScope( srcLine, expr.line );
      NATIVE_VALUE result;

      if( fieldSetter( expr ) )
         return nativeFieldStore( expr );

      if( fieldRead( expr ) )
         return withConversion( expr, nativeFieldLoad( expr ) );

      if( expr.kind == X_EXPR_KIND::BINARY && expr.operationType == X_DECL_TYPE::STRING )
         return withConversion( expr, nativeUnbox( itemExpression( expr ), expr.type, false ) );

      if( typedSend( expr ) )
         return withConversion( expr, nativeSend( expr, freshItem( "native.send.result" ) ) );

      if( !expr.nativeType && !ownNativeType( expr.kind ) )
         return nativeAny( expr );

      if( !nativeByKind( expr, result ) )
         return {};

      if( !result.value )
      {
         fail( expr.line, "nieobslugiwana operacja natywna" );
         return {};
      }

      return withConversion( expr, result );
   }

   /* ---------------------------------------------------------------------------------------------
      Scalar locals
      --------------------------------------------------------------------------------------------- */

   /* An integer may leave the INTEGER range: - and ++ of an integer give NUMERIC */
   static STATIC_TYPE widened( STATIC_TYPE type )
   {
      return IIF( type == STATIC_TYPE::DOUBLE, type, STATIC_TYPE::NUMERIC );
   }

   /* + - * of two numbers: DOUBLE when one of them is DOUBLE, else NUMERIC */
   static STATIC_TYPE numberResult( STATIC_TYPE left, STATIC_TYPE right )
   {
      if( left == STATIC_TYPE::DOUBLE || right == STATIC_TYPE::DOUBLE )
         return STATIC_TYPE::DOUBLE;

      return STATIC_TYPE::NUMERIC;
   }

   STATIC_TYPE scalarName( const X_EXPR &expr ) const
   {
      const auto found = scalars.find( s_key( expr.text ) );

      if( found == scalars.end() )
         return STATIC_TYPE::UNKNOWN;

      return found->second;
   }

   STATIC_TYPE scalarUnary( const X_EXPR &expr ) const
   {
      const STATIC_TYPE type = scalarType( *expr.args[0] );

      if( expr.text == "+" && scalarNumeric( type ) )
         return type;

      if( expr.text == "-" && scalarNumeric( type ) )
         return widened( type );

      if( expr.text == ".NOT." && type == STATIC_TYPE::LOGICAL )
         return type;

      return STATIC_TYPE::UNKNOWN;
   }

   /* ++ and -- of a scalar local */
   STATIC_TYPE scalarUpdate( const X_EXPR &expr ) const
   {
      const STATIC_TYPE type = scalarType( *expr.args[0] );

      if( expr.args[0]->kind == X_EXPR_KIND::NAME && scalarNumeric( type ) )
         return widened( type );

      return STATIC_TYPE::UNKNOWN;
   }

   /* := keeps the type of the value; += -= *= of numbers give a number, %= gives NUMERIC */
   STATIC_TYPE scalarAssign( const X_EXPR &expr ) const
   {
      const X_ASSIGN_OP op = expr.assignOp;
      STATIC_TYPE left;
      STATIC_TYPE right;
      bool numbers;

      if( expr.args[0]->kind != X_EXPR_KIND::NAME )
         return STATIC_TYPE::UNKNOWN;

      left = scalarType( *expr.args[0] );
      right = scalarType( *expr.args[1] );
      numbers = scalarNumeric( left ) && scalarNumeric( right );

      if( op == X_ASSIGN_OP::SET )
         return IIF( left == right || numbers, right, STATIC_TYPE::UNKNOWN );

      if( numbers && op == X_ASSIGN_OP::MOD )
         return STATIC_TYPE::NUMERIC;

      if( numbers && ( op == X_ASSIGN_OP::ADD || op == X_ASSIGN_OP::SUB || op == X_ASSIGN_OP::MUL ) )
         return numberResult( left, right );

      return STATIC_TYPE::UNKNOWN;
   }

   STATIC_TYPE scalarBinary( const X_EXPR &expr ) const
   {
      const STATIC_TYPE left = scalarType( *expr.args[0] );
      const STATIC_TYPE right = scalarType( *expr.args[1] );
      const std::string &op = expr.text;
      const bool numbers = scalarNumeric( left ) && scalarNumeric( right );
      const bool logical = left == STATIC_TYPE::LOGICAL && right == STATIC_TYPE::LOGICAL;

      if( ( numbers || logical ) && comparison( op ) )
         return STATIC_TYPE::LOGICAL;

      if( logical && ( op == ".AND." || op == ".OR." ) )
         return STATIC_TYPE::LOGICAL;

      if( numbers && op == "%" )
         return STATIC_TYPE::NUMERIC;

      if( numbers && ( op == "+" || op == "-" || op == "*" ) )
         return numberResult( left, right );

      return STATIC_TYPE::UNKNOWN;
   }

   /* The type an expression keeps: INTEGER, DOUBLE, NUMERIC when an integer may grow, LOGICAL or UNKNOWN */
   STATIC_TYPE scalarType( const X_EXPR &expr ) const
   {
      switch( expr.kind )
      {
      case X_EXPR_KIND::NUMBER:  return IIF( expr.numberIsInteger, STATIC_TYPE::INTEGER, STATIC_TYPE::DOUBLE );
      case X_EXPR_KIND::LOGICAL: return STATIC_TYPE::LOGICAL;
      case X_EXPR_KIND::NAME:    return scalarName( expr );
      case X_EXPR_KIND::UNARY:   return scalarUnary( expr );
      case X_EXPR_KIND::UPDATE:  return scalarUpdate( expr );
      case X_EXPR_KIND::ASSIGN:  return scalarAssign( expr );
      case X_EXPR_KIND::BINARY:  return scalarBinary( expr );
      default:                   return STATIC_TYPE::UNKNOWN;
      }
   }

   /* A SWITCH without CASE and OTHERWISE */
   static bool emptySwitch( const X_STMT &stmt )
   {
      return stmt.kind == X_STMT_KIND::SWITCH && stmt.cases.empty() && !stmt.hasOtherwise;
   }

   /* Whether a local without INIT gets a value before every read; code blocks are not followed */
   struct READINESS
   {
      const std::string &key;
      bool valid = true;

      /* Code that may not run: what it assigns does not count */
      void mayRun( const X_EXPR &expr, bool ready )
      {
         visit( expr, ready );
      }

      void mayRun( const std::vector<X_STMT> &body, bool ready )
      {
         walk( body, ready );
      }

      /* x := value makes key ready after the value; an element gets its value before the array is read */
      void assign( const X_EXPR &expr, bool &ready )
      {
         const X_EXPR &target = *expr.args[0];

         if( target.kind == X_EXPR_KIND::NAME )
         {
            if( expr.assignOp != X_ASSIGN_OP::SET )
               visit( target, ready );

            visit( *expr.args[1], ready );

            if( s_key( target.text ) == key )
               ready = true;
         }
         else if( target.kind == X_EXPR_KIND::INDEX )
         {
            visit( *expr.args[1], ready );
            visit( target, ready );
         }
         else
         {
            visit( target, ready );
            visit( *expr.args[1], ready );
         }
      }

      /* Key is ready after IIF() when both branches make it ready */
      void iif( const X_EXPR &expr, bool &ready )
      {
         bool whenTrue;
         bool whenFalse;

         visit( *expr.args[0], ready );
         whenTrue = ready;
         whenFalse = ready;
         visit( *expr.args[1], whenTrue );
         visit( *expr.args[2], whenFalse );
         ready = whenTrue && whenFalse;
      }

      /* A read of key before it is ready spoils it */
      void visit( const X_EXPR &expr, bool &ready )
      {
         const bool shortCircuit = expr.kind == X_EXPR_KIND::BINARY && ( expr.text == ".AND." || expr.text == ".OR." );

         if( expr.kind == X_EXPR_KIND::BLOCK )
            return;

         if( expr.kind == X_EXPR_KIND::NAME && s_key( expr.text ) == key )
            valid &= ready;

         if( expr.kind == X_EXPR_KIND::ASSIGN )
         {
            assign( expr, ready );
            return;
         }

         if( shortCircuit )
         {
            visit( *expr.args[0], ready );
            mayRun( *expr.args[1], ready );
            return;
         }

         if( expr.kind == X_EXPR_KIND::IIF )
         {
            iif( expr, ready );
            return;
         }

         for( const X_EXPR_PTR &arg : expr.args )
            visit( *arg, ready );
      }

      /* LOCAL key is not ready; with INIT or dimensions it is ready afterwards */
      void declare( const X_LOCAL_DECL &decl, bool &ready )
      {
         const bool own = s_key( decl.name ) == key;

         if( own )
            ready = false;

         if( decl.init )
            visit( *decl.init, ready );

         for( const X_EXPR_PTR &dim : decl.dimensions )
            visit( *dim, ready );

         if( own && ( decl.init || !decl.dimensions.empty() ) )
            ready = true;
      }

      /* The counter is ready; the body may not run and the step runs again after it */
      void forLoop( const X_STMT &stmt, bool &ready )
      {
         if( s_key( stmt.name ) == key )
            ready = true;

         visit( *stmt.forLimit, ready );
         if( stmt.forStep )
            visit( *stmt.forStep, ready );

         mayRun( stmt.body, ready );
         if( stmt.forStep )
            mayRun( *stmt.forStep, ready );
      }

      /* IF and BEGIN SEQUENCE: key is ready when both branches make it ready */
      void bothBranches( const X_STMT &stmt, bool &ready )
      {
         bool first = ready;
         bool second = ready;

         walk( stmt.body, first );
         walk( stmt.elseBody, second );
         ready = first && second;
      }

      /* A CASE condition runs when the ones before it fail; no branch makes key ready for later code */
      void caseBranches( const X_STMT &stmt, bool next )
      {
         for( const X_CASE_BRANCH &branch : stmt.cases )
         {
            visit( *branch.condition, next );
            mayRun( branch.body, next );
         }

         walk( stmt.otherwiseBody, next );
      }

      void walk( const std::vector<X_STMT> &body, bool &ready )
      {
         for( const X_STMT &stmt : body )
         {
            if( stmt.kind == X_STMT_KIND::STATIC || emptySwitch( stmt ) )
               continue;

            for( const X_LOCAL_DECL &decl : stmt.locals )
               declare( decl, ready );

            if( stmt.expr )
               visit( *stmt.expr, ready );

            if( stmt.kind == X_STMT_KIND::FOR )
               forLoop( stmt, ready );
            else if( stmt.kind == X_STMT_KIND::WHILE )
               mayRun( stmt.body, ready );
            else if( stmt.kind == X_STMT_KIND::IF || stmt.kind == X_STMT_KIND::SEQUENCE )
               bothBranches( stmt, ready );
            else if( stmt.kind == X_STMT_KIND::DO_CASE || stmt.kind == X_STMT_KIND::SWITCH )
               caseBranches( stmt, ready );
         }
      }
   };

   bool scalarReady( const X_FUNCTION &fn, const std::string &key ) const
   {
      READINESS readiness{ key };
      bool ready = false;

      readiness.walk( fn.body, ready );

      return readiness.valid;
   }

   /* What collectScalars() finds in a function */
   struct SCALAR_SCAN
   {
      std::vector<const X_LOCAL_DECL *> decls;
      std::vector<const X_EXPR *> writes;
      std::vector<const X_STMT *> loops;
      std::unordered_set<std::string> blocked;
      std::unordered_set<std::string> declared;
   };

   /* Assignments are kept; locals captured by a code block or passed by @ are blocked */
   void scanScalarExpr( SCALAR_SCAN &scan, const X_EXPR &expr )
   {
      if( expr.kind == X_EXPR_KIND::BLOCK )
      {
         blockCaptured( scan.blocked, expr );
         return;
      }

      if( expr.kind == X_EXPR_KIND::ASSIGN || expr.kind == X_EXPR_KIND::UPDATE )
         scan.writes.push_back( &expr );

      blockReferences( scan.blocked, expr );

      for( const X_EXPR_PTR &arg : expr.args )
         scanScalarExpr( scan, *arg );
   }

   /* A candidate is declared once, by LOCAL at the head of the function and without dimensions; the
      FOR loops are kept and a BEGIN SEQUENCE variable is blocked */
   void scanScalarStatements( SCALAR_SCAN &scan, const std::vector<X_STMT> &body, bool head )
   {
      for( const X_STMT &stmt : body )
      {
         head = head && headStatement( stmt.kind );

         if( stmt.kind == X_STMT_KIND::FOR )
            scan.loops.push_back( &stmt );

         if( stmt.kind == X_STMT_KIND::SEQUENCE )
            scan.blocked.insert( s_key( stmt.name ) );

         for( const X_LOCAL_DECL &decl : stmt.locals )
         {
            const std::string key = s_key( decl.name );
            const bool fresh = scan.declared.insert( key ).second;
            const bool local = head && stmt.kind == X_STMT_KIND::LOCAL;

            if( !fresh || !local )
               scan.blocked.insert( key );

            if( local && decl.dimensions.empty() )
               scan.decls.push_back( &decl );

            if( decl.init )
               scanScalarExpr( scan, *decl.init );

            for( const X_EXPR_PTR &dim : decl.dimensions )
               scanScalarExpr( scan, *dim );
         }

         if( stmt.expr )
            scanScalarExpr( scan, *stmt.expr );

         if( stmt.target )
            scanScalarExpr( scan, *stmt.target );

         if( stmt.forLimit )
            scanScalarExpr( scan, *stmt.forLimit );

         if( stmt.forStep )
            scanScalarExpr( scan, *stmt.forStep );

         scanScalarStatements( scan, stmt.body, false );
         scanScalarStatements( scan, stmt.elseBody, false );

         for( const X_CASE_BRANCH &branch : stmt.cases )
         {
            scanScalarExpr( scan, *branch.condition );
            scanScalarStatements( scan, branch.body, false );
         }

         scanScalarStatements( scan, stmt.otherwiseBody, false );
      }
   }

   /* The start type of a candidate: its INIT, or NUMERIC when it gets a value before every read */
   void startScalar( const X_FUNCTION &fn, const SCALAR_SCAN &scan, const X_LOCAL_DECL &decl )
   {
      const std::string key = s_key( decl.name );
      STATIC_TYPE type = STATIC_TYPE::NUMERIC;

      if( scan.blocked.count( key ) )
         return;

      if( !decl.init && !scalarReady( fn, key ) )
         return;

      if( decl.init )
         type = scalarType( *decl.init );

      if( type != STATIC_TYPE::UNKNOWN )
         scalars.emplace( key, type );
   }

   /* Two numbers join into NUMERIC, other types make the local no scalar; true when the type changed */
   bool mergeScalar( const std::string &key, STATIC_TYPE type )
   {
      const auto found = scalars.find( key );
      STATIC_TYPE joined = STATIC_TYPE::UNKNOWN;

      if( found == scalars.end() || found->second == type )
         return false;

      if( scalarNumeric( found->second ) && scalarNumeric( type ) )
         joined = STATIC_TYPE::NUMERIC;

      if( joined == found->second )
         return false;

      if( joined == STATIC_TYPE::UNKNOWN )
         scalars.erase( found );
      else
         found->second = joined;

      return true;
   }

   /* The FOR counter stays a scalar when the start, the limit and the step are numbers */
   bool widenLoop( const X_STMT &loop )
   {
      const std::string key = s_key( loop.name );
      const auto found = scalars.find( key );
      STATIC_TYPE start;
      STATIC_TYPE limit;
      STATIC_TYPE step = STATIC_TYPE::INTEGER;
      bool numbers;
      bool changed;

      if( found == scalars.end() )
         return false;

      start = scalarType( *loop.expr );
      limit = scalarType( *loop.forLimit );
      if( loop.forStep )
         step = scalarType( *loop.forStep );

      numbers = scalarNumeric( start ) && scalarNumeric( limit ) && scalarNumeric( step );
      if( !scalarNumeric( found->second ) || !numbers )
         return mergeScalar( key, STATIC_TYPE::UNKNOWN );

      changed = mergeScalar( key, start );
      changed |= mergeScalar( key, numberResult( scalars.at( key ), step ) );

      return changed;
   }

   /* One pass over the assignments, the initializers and the FOR loops; true when a type changed */
   bool widenScalars( const SCALAR_SCAN &scan )
   {
      bool changed = false;

      for( const X_EXPR *expr : scan.writes )
      {
         if( expr->args[0]->kind == X_EXPR_KIND::NAME )
            changed |= mergeScalar( s_key( expr->args[0]->text ), scalarType( *expr ) );
      }

      for( const X_LOCAL_DECL *decl : scan.decls )
      {
         if( decl->init )
            changed |= mergeScalar( s_key( decl->name ), scalarType( *decl->init ) );
      }

      for( const X_STMT *loop : scan.loops )
         changed |= widenLoop( *loop );

      return changed;
   }

   /* Locals that hold only numbers or only logicals become native scalars; their types widen until no
      assignment changes them */
   void collectScalars( const X_FUNCTION &fn )
   {
      SCALAR_SCAN scan;
      bool changed;

      for( const std::string &param : fn.params )
         scan.blocked.insert( s_key( param ) );

      for( const auto &entry : locals )
         scan.blocked.insert( entry.first );

      for( const auto &entry : nativeLocals )
         scan.blocked.insert( entry.first );

      if( !fn.className.empty() )
         scan.blocked.insert( "SELF" );

      scanScalarStatements( scan, fn.body, true );

      for( const X_LOCAL_DECL *decl : scan.decls )
         startScalar( fn, scan, *decl );

      do
      {
         changed = widenScalars( scan );
      } while( changed );
   }

   /* The LLVM type of a scalar; NUMERIC is { is double, bits } */
   llvm::Type *scalarLLVM( STATIC_TYPE type )
   {
      if( type == STATIC_TYPE::NUMERIC )
         return llvm::StructType::get( context, { i1Ty(), i64Ty() } );

      if( type == STATIC_TYPE::DOUBLE )
         return doubleTy();

      if( type == STATIC_TYPE::INTEGER )
         return i64Ty();

      return i1Ty();
   }

   /* A scalar number as a double */
   llvm::Value *scalarDouble( llvm::Value *value )
   {
      llvm::Value *bits;
      llvm::Value *real;
      llvm::Value *fromBits;
      llvm::Value *fromInteger;

      if( value->getType()->isDoubleTy() )
         return value;

      if( !value->getType()->isStructTy() )
         return builder.CreateSIToFP( value, doubleTy() );

      bits = builder.CreateExtractValue( value, 1 );
      real = builder.CreateExtractValue( value, 0 );
      fromBits = builder.CreateBitCast( bits, doubleTy() );
      fromInteger = builder.CreateSIToFP( bits, doubleTy() );

      return builder.CreateSelect( real, fromBits, fromInteger );
   }

   /* A scalar number as a NUMERIC */
   llvm::Value *scalarNumber( llvm::Value *value )
   {
      const bool real = value->getType()->isDoubleTy();
      llvm::Value *empty;
      llvm::Value *pair;
      llvm::Value *bits = value;

      if( value->getType()->isStructTy() )
         return value;

      empty = llvm::PoisonValue::get( scalarLLVM( STATIC_TYPE::NUMERIC ) );
      pair = builder.CreateInsertValue( empty, builder.getInt1( real ), 0 );
      if( real )
         bits = builder.CreateBitCast( value, i64Ty() );

      return builder.CreateInsertValue( pair, bits, 1 );
   }

   /* The number of an item as a NUMERIC */
   llvm::Value *scalarNumberFromItem( llvm::Value *item )
   {
      llvm::Value *tag = loadTag( item );
      llvm::Value *bits = loadPayload( item, i64Ty() );
      llvm::Value *empty = llvm::PoisonValue::get( scalarLLVM( STATIC_TYPE::NUMERIC ) );
      llvm::Value *value = builder.CreateInsertValue( empty, tagIs( tag, X_IT_DOUBLE ), 0 );

      return builder.CreateInsertValue( value, bits, 1 );
   }

   /* Whether a scalar number holds a double */
   llvm::Value *scalarReal( llvm::Value *value )
   {
      if( value->getType()->isStructTy() )
         return builder.CreateExtractValue( value, 0 );

      return builder.getInt1( value->getType()->isDoubleTy() );
   }

   /* The 64 bits of a scalar number */
   llvm::Value *scalarBits( llvm::Value *value )
   {
      if( value->getType()->isStructTy() )
         return builder.CreateExtractValue( value, 1 );

      return value;
   }

   /* The integer of a scalar number; a double gives 0 */
   llvm::Value *scalarIntegerBits( llvm::Value *value )
   {
      if( value->getType()->isStructTy() )
         return builder.CreateExtractValue( value, 1 );

      if( value->getType()->isDoubleTy() )
         return constant64( 0 );

      return builder.CreateIntCast( value, i64Ty(), true );
   }

   /* INT64_MIN % -1 overflows in SRem: the divisor becomes 1 and the result 0 */
   llvm::Value *integerRemainder( llvm::Value *left, llvm::Value *right )
   {
      llvm::Value *leftBits = scalarIntegerBits( left );
      llvm::Value *rightBits = scalarIntegerBits( right );
      llvm::Value *minimum = builder.CreateICmpEQ( leftBits, constant64( INT64_MIN ) );
      llvm::Value *minusOne = builder.CreateICmpEQ( rightBits, llvm::ConstantInt::getSigned( i64Ty(), -1 ) );
      llvm::Value *overflow = builder.CreateAnd( minimum, minusOne );
      llvm::Value *safeRight = builder.CreateSelect( overflow, constant64( 1 ), rightBits );
      llvm::Value *remainder = builder.CreateSRem( leftBits, safeRight );

      return scalarNumber( builder.CreateSelect( overflow, constant64( 0 ), remainder ) );
   }

   /* The remainder of a divisor that is not zero: FRem when an operand is a double, else SRem */
   llvm::Value *remainderValue( llvm::Value *left, llvm::Value *right )
   {
      llvm::BasicBlock *realBB = llvm::BasicBlock::Create( context, "numeric.remainder.double", currentFunc );
      llvm::BasicBlock *integerBB = llvm::BasicBlock::Create( context, "numeric.remainder.integer", currentFunc );
      llvm::BasicBlock *normalBB = llvm::BasicBlock::Create( context, "numeric.remainder.result", currentFunc );
      llvm::Value *realValue;
      llvm::BasicBlock *realEnd;
      llvm::Value *integerValue;
      llvm::BasicBlock *integerEnd;
      llvm::PHINode *value;

      builder.CreateCondBr( builder.CreateOr( scalarReal( left ), scalarReal( right ) ), realBB, integerBB );

      builder.SetInsertPoint( realBB );
      realValue = scalarNumber( builder.CreateFRem( scalarDouble( left ), scalarDouble( right ) ) );
      builder.CreateBr( normalBB );
      realEnd = builder.GetInsertBlock();

      builder.SetInsertPoint( integerBB );
      integerValue = integerRemainder( left, right );
      integerEnd = builder.GetInsertBlock();
      builder.CreateBr( normalBB );

      builder.SetInsertPoint( normalBB );
      value = builder.CreatePHI( scalarLLVM( STATIC_TYPE::NUMERIC ), 2 );
      value->addIncoming( realValue, realEnd );
      value->addIncoming( integerValue, integerEnd );

      return value;
   }

   /* % of scalar numbers; a zero divisor goes to x_itemMod() for its error */
   llvm::Value *scalarRemainder( llvm::Value *left, llvm::Value *right )
   {
      llvm::Value *zero = builder.CreateFCmpOEQ( scalarDouble( right ), llvm::ConstantFP::get( doubleTy(), 0.0 ) );
      llvm::BasicBlock *zeroBB = llvm::BasicBlock::Create( context, "numeric.remainder.zero", currentFunc );
      llvm::BasicBlock *validBB = llvm::BasicBlock::Create( context, "numeric.remainder.valid", currentFunc );
      llvm::BasicBlock *doneBB = llvm::BasicBlock::Create( context, "numeric.remainder.done", currentFunc );
      llvm::Value *fallbackValue;
      llvm::BasicBlock *fallbackEnd;
      llvm::Value *normalValue;
      llvm::BasicBlock *normalEnd;
      llvm::PHINode *value;

      builder.CreateCondBr( zero, zeroBB, validBB );

      builder.SetInsertPoint( zeroBB );
      fallbackValue = scalarNumberFromItem( operatorCall( "x_itemMod", scalarItem( left ), scalarItem( right ) ) );
      fallbackEnd = builder.GetInsertBlock();
      builder.CreateBr( doneBB );

      builder.SetInsertPoint( validBB );
      normalValue = remainderValue( left, right );
      normalEnd = builder.GetInsertBlock();
      builder.CreateBr( doneBB );

      builder.SetInsertPoint( doneBB );
      value = builder.CreatePHI( scalarLLVM( STATIC_TYPE::NUMERIC ), 2 );
      value->addIncoming( fallbackValue, fallbackEnd );
      value->addIncoming( normalValue, normalEnd );

      return value;
   }

   /* + - * as doubles; '~' negates the right operand */
   llvm::Value *floatMath( llvm::Value *left, llvm::Value *right, char op )
   {
      llvm::Value *rightReal = scalarDouble( right );
      llvm::Value *leftReal;

      if( op == '~' )
         return builder.CreateFNeg( rightReal );

      leftReal = scalarDouble( left );

      if( op == '+' )
         return builder.CreateFAdd( leftReal, rightReal );

      if( op == '*' )
         return builder.CreateFMul( leftReal, rightReal );

      return builder.CreateFSub( leftReal, rightReal );
   }

   static llvm::Intrinsic::ID overflowIntrinsic( char op )
   {
      if( op == '+' )
         return llvm::Intrinsic::sadd_with_overflow;

      if( op == '*' )
         return llvm::Intrinsic::smul_with_overflow;

      return llvm::Intrinsic::ssub_with_overflow;
   }

   /* + - * % and the negation '~' of scalar numbers; an integer that overflows is computed again as a double */
   llvm::Value *scalarMath( llvm::Value *left, llvm::Value *right, char op )
   {
      const bool numeric = left->getType()->isStructTy() || right->getType()->isStructTy();
      llvm::BasicBlock *realBB;
      llvm::BasicBlock *done;
      llvm::Function *fn;
      llvm::Value *sum;
      llvm::Value *exact;
      llvm::BasicBlock *intEnd;
      llvm::Value *real;
      llvm::BasicBlock *realEnd;
      llvm::PHINode *value;

      if( op == '%' )
         return scalarRemainder( left, right );

      if( left->getType()->isDoubleTy() || right->getType()->isDoubleTy() )
         return floatMath( left, right, op );

      realBB = llvm::BasicBlock::Create( context, "numeric.double", currentFunc );
      done = llvm::BasicBlock::Create( context, "numeric.done", currentFunc );

      /* A NUMERIC that holds a double goes straight to the double operation */
      if( numeric )
      {
         llvm::BasicBlock *intBB = llvm::BasicBlock::Create( context, "numeric.integer", currentFunc );

         builder.CreateCondBr( builder.CreateOr( scalarReal( left ), scalarReal( right ) ), realBB, intBB );
         builder.SetInsertPoint( intBB );
      }

      fn = intrinsicDeclaration( overflowIntrinsic( op ), { i64Ty() } );
      sum = builder.CreateCall( fn, { scalarBits( left ), scalarBits( right ) } );
      exact = scalarNumber( builder.CreateExtractValue( sum, 0 ) );
      intEnd = builder.GetInsertBlock();
      builder.CreateCondBr( builder.CreateExtractValue( sum, 1 ), realBB, done );

      builder.SetInsertPoint( realBB );
      real = scalarNumber( floatMath( left, right, op ) );
      realEnd = builder.GetInsertBlock();
      builder.CreateBr( done );

      builder.SetInsertPoint( done );
      value = builder.CreatePHI( scalarLLVM( STATIC_TYPE::NUMERIC ), 2 );
      value->addIncoming( exact, intEnd );
      value->addIncoming( real, realEnd );

      return value;
   }

   llvm::Value *scalarLiteral( const X_EXPR &expr )
   {
      if( expr.numberIsInteger )
         return llvm::ConstantInt::get( i64Ty(), expr.integer, true );

      return llvm::ConstantFP::get( doubleTy(), expr.number );
   }

   llvm::Value *scalarLoad( const X_EXPR &expr )
   {
      const auto found = locals.find( s_key( expr.text ) );

      if( found == locals.end() )
      {
         fail( expr.line, "blad wewnetrzny: brak magazynu rozstrzygnietej zmiennej " + expr.text );
         return nullptr;
      }

      return builder.CreateLoad( scalarLLVM( scalarType( expr ) ), found->second );
   }

   /* The operator of a compound assignment of scalars */
   static char scalarOperator( X_ASSIGN_OP op )
   {
      switch( op )
      {
      case X_ASSIGN_OP::ADD: return '+';
      case X_ASSIGN_OP::SUB: return '-';
      case X_ASSIGN_OP::MUL: return '*';
      default:               return '%';
      }
   }

   /* x := value and x op= value of a scalar local; a NUMERIC local stores the value as a NUMERIC */
   llvm::Value *scalarAssignment( const X_EXPR &expr )
   {
      const std::string &name = expr.args[0]->text;
      const bool compound = expr.assignOp != X_ASSIGN_OP::SET;
      const auto found = locals.find( s_key( name ) );
      llvm::Value *left = nullptr;
      llvm::Value *right;
      llvm::Value *stored;

      if( compound )
      {
         left = scalarExpr( *expr.args[0] );
         if( left == nullptr )
            return nullptr;
      }

      right = scalarExpr( *expr.args[1] );
      if( right == nullptr )
         return nullptr;

      if( compound )
         right = scalarMath( left, right, scalarOperator( expr.assignOp ) );

      if( found == locals.end() )
      {
         fail( expr.line, "blad wewnetrzny: brak magazynu rozstrzygnietej zmiennej " + name );
         return nullptr;
      }

      stored = right;
      if( scalars.at( s_key( name ) ) == STATIC_TYPE::NUMERIC )
         stored = scalarNumber( right );

      builder.CreateStore( stored, found->second );

      return right;
   }

   /* ++ and -- store the new value; x++ and x-- give the previous one */
   llvm::Value *scalarIncrement( const X_EXPR &expr, llvm::Value *left )
   {
      const char op = IIF( expr.updateOp == X_UPDATE_OP::INC, '+', '-' );
      llvm::Value *value = scalarMath( left, constant64( 1 ), op );

      builder.CreateStore( value, locals.at( s_key( expr.args[0]->text ) ) );

      return IIF( expr.updateMode == X_UPDATE_MODE::POSTFIX, left, value );
   }

   /* Unary + gives the value, - negates a number and .NOT. a logical */
   llvm::Value *scalarUnaryValue( const X_EXPR &expr, llvm::Value *left )
   {
      if( expr.text == "+" )
         return left;

      if( expr.text == "-" )
         return scalarMath( constant64( 0 ), left, '~' );

      return builder.CreateNot( left );
   }

   /* .AND. and .OR. of scalars evaluate the right operand only when it decides */
   llvm::Value *scalarLogical( const X_EXPR &expr, llvm::Value *left )
   {
      llvm::BasicBlock *start = builder.GetInsertBlock();
      llvm::BasicBlock *rightBB = llvm::BasicBlock::Create( context, "scalar.right", currentFunc );
      llvm::BasicBlock *done = llvm::BasicBlock::Create( context, "scalar.done", currentFunc );
      llvm::Value *right;
      llvm::BasicBlock *last;
      llvm::PHINode *value;

      if( expr.text == ".AND." )
         builder.CreateCondBr( left, rightBB, done );
      else
         builder.CreateCondBr( left, done, rightBB );

      builder.SetInsertPoint( rightBB );
      right = scalarExpr( *expr.args[1] );
      if( right == nullptr )
         return nullptr;

      last = builder.GetInsertBlock();
      builder.CreateBr( done );

      builder.SetInsertPoint( done );
      value = builder.CreatePHI( i1Ty(), 2 );
      value->addIncoming( left, start );
      value->addIncoming( right, last );

      return value;
   }

   /* The value of a scalar expression in its native form */
   llvm::Value *scalarExpr( const X_EXPR &expr )
   {
      llvm::SaveAndRestore<int> lineScope( srcLine, expr.line );
      llvm::Value *left;
      llvm::Value *right;

      switch( expr.kind )
      {
      case X_EXPR_KIND::NUMBER:  return scalarLiteral( expr );
      case X_EXPR_KIND::LOGICAL: return builder.getInt1( expr.logical );
      case X_EXPR_KIND::NAME:    return scalarLoad( expr );
      case X_EXPR_KIND::ASSIGN:  return scalarAssignment( expr );
      default:                   break;
      }

      left = scalarExpr( *expr.args[0] );
      if( left == nullptr )
         return nullptr;

      if( expr.kind == X_EXPR_KIND::UPDATE )
         return scalarIncrement( expr, left );

      if( expr.kind == X_EXPR_KIND::UNARY )
         return scalarUnaryValue( expr, left );

      if( expr.text == ".AND." || expr.text == ".OR." )
         return scalarLogical( expr, left );

      right = scalarExpr( *expr.args[1] );
      if( right == nullptr )
         return nullptr;

      if( expr.text == "+" || expr.text == "-" || expr.text == "*" || expr.text == "%" )
         return scalarMath( left, right, expr.text[0] );

      return scalarCompare( left, right, expr.text );
   }

   static llvm::CmpInst::Predicate realPredicate( const std::string &op )
   {
      if( op == "=" || op == "==" )
         return llvm::CmpInst::FCMP_UEQ;

      if( op == "!=" )
         return llvm::CmpInst::FCMP_ONE;

      if( op == "<" )
         return llvm::CmpInst::FCMP_OLT;

      if( op == "<=" )
         return llvm::CmpInst::FCMP_ULE;

      if( op == ">" )
         return llvm::CmpInst::FCMP_OGT;

      return llvm::CmpInst::FCMP_UGE;
   }

   /* Logicals compare unsigned, integers signed */
   static llvm::CmpInst::Predicate integerPredicate( const std::string &op, bool logical )
   {
      if( op == "=" || op == "==" )
         return llvm::CmpInst::ICMP_EQ;

      if( op == "!=" )
         return llvm::CmpInst::ICMP_NE;

      if( op == "<" )
         return IIF( logical, llvm::CmpInst::ICMP_ULT, llvm::CmpInst::ICMP_SLT );

      if( op == "<=" )
         return IIF( logical, llvm::CmpInst::ICMP_ULE, llvm::CmpInst::ICMP_SLE );

      if( op == ">" )
         return IIF( logical, llvm::CmpInst::ICMP_UGT, llvm::CmpInst::ICMP_SGT );

      return IIF( logical, llvm::CmpInst::ICMP_UGE, llvm::CmpInst::ICMP_SGE );
   }

   llvm::Value *realCompare( llvm::Value *left, llvm::Value *right, const std::string &op )
   {
      return builder.CreateFCmp( realPredicate( op ), scalarDouble( left ), scalarDouble( right ) );
   }

   llvm::Value *integerCompare( llvm::Value *left, llvm::Value *right, const std::string &op )
   {
      const bool logical = left->getType()->isIntegerTy( 1 );

      return builder.CreateICmp( integerPredicate( op, logical ), scalarBits( left ), scalarBits( right ) );
   }

   /* A comparison of scalars; a NUMERIC operand chooses the double or the integer comparison at run time */
   llvm::Value *scalarCompare( llvm::Value *left, llvm::Value *right, const std::string &op )
   {
      llvm::BasicBlock *realBB;
      llvm::BasicBlock *intBB;
      llvm::BasicBlock *done;
      llvm::Value *real;
      llvm::Value *exact;
      llvm::PHINode *value;

      if( left->getType()->isDoubleTy() || right->getType()->isDoubleTy() )
         return realCompare( left, right, op );

      if( !left->getType()->isStructTy() && !right->getType()->isStructTy() )
         return integerCompare( left, right, op );

      realBB = llvm::BasicBlock::Create( context, "compare.double", currentFunc );
      intBB = llvm::BasicBlock::Create( context, "compare.integer", currentFunc );
      done = llvm::BasicBlock::Create( context, "compare.done", currentFunc );
      builder.CreateCondBr( builder.CreateOr( scalarReal( left ), scalarReal( right ) ), realBB, intBB );

      builder.SetInsertPoint( realBB );
      real = realCompare( left, right, op );
      builder.CreateBr( done );

      builder.SetInsertPoint( intBB );
      exact = integerCompare( left, right, op );
      builder.CreateBr( done );

      builder.SetInsertPoint( done );
      value = builder.CreatePHI( i1Ty(), 2 );
      value->addIncoming( real, realBB );
      value->addIncoming( exact, intBB );

      return value;
   }

   /* A scalar as an item; a NUMERIC chooses DOUBLE or INTEGER at run time */
   llvm::Value *scalarItem( llvm::Value *value )
   {
      const bool logical = value->getType()->isIntegerTy( 1 );
      const bool real = value->getType()->isDoubleTy();
      const int32_t tag = IIF( logical, X_IT_LOGICAL, IIF( real, X_IT_DOUBLE, X_IT_INTEGER ) );
      llvm::Value *slot = freshItem( "scalar.item" );
      llvm::Value *type;
      llvm::Value *payload = value;

      if( value->getType()->isStructTy() )
      {
         type = builder.CreateSelect( scalarReal( value ), constant32( X_IT_DOUBLE ), constant32( X_IT_INTEGER ) );
         builder.CreateStore( type, builder.CreateStructGEP( getItemType(), slot, 0 ) );
         storePayload( slot, scalarBits( value ) );
         return slot;
      }

      storeNType( slot, tag );
      if( logical )
         payload = builder.CreateZExt( value, i64Ty() );

      storePayload( slot, payload );

      return slot;
   }

   /* A condition as an i1; its temporaries are cleared at once */
   llvm::Value *scalarCondition( const X_EXPR &expr )
   {
      const size_t first = itemSlots.size();
      const NATIVE_VALUE value = nativeExpression( expr );

      if( value.type != X_DECL_TYPE::LOGIC )
      {
         fail( expr.line, "blad wewnetrzny: warunek bez konwersji do LOGIC" );
         return nullptr;
      }

      if( value.value )
         clearTemps( first, itemSlots.size() );

      return value.value;
   }

   /* ---------------------------------------------------------------------------------------------
      Expression types and logical operators
      --------------------------------------------------------------------------------------------- */

   /* A static may be changed by other calls or by recursion: PERSISTENT */
   STATIC_TYPE nameType( const X_EXPR &expr ) const
   {
      const std::string key = s_key( expr.text );
      const auto slot = locals.find( key );
      const auto found = localTypes.find( key );

      if( slot != locals.end() && llvm::isa<llvm::GlobalVariable>( slot->second ) )
         return STATIC_TYPE::PERSISTENT;

      if( found == localTypes.end() )
         return STATIC_TYPE::UNKNOWN;

      return found->second;
   }

   STATIC_TYPE binaryType( const X_EXPR &expr ) const
   {
      const std::string &op = expr.text;

      if( comparison( op ) || op == "$" || op == ".AND." || op == ".OR." )
         return STATIC_TYPE::LOGICAL;

      if( op != "+" || expressionType( *expr.args[0] ) != STATIC_TYPE::STRING )
         return STATIC_TYPE::UNKNOWN;

      return IIF( expressionType( *expr.args[1] ) == STATIC_TYPE::STRING, STATIC_TYPE::STRING, STATIC_TYPE::UNKNOWN );
   }

   /* The type of an expression as far as it is known before run time; an array element is PERSISTENT,
      it is shared and its type is checked at run time */
   STATIC_TYPE expressionType( const X_EXPR &expr ) const
   {
      const STATIC_TYPE type = scalarType( expr );

      if( type != STATIC_TYPE::UNKNOWN )
         return type;

      switch( expr.kind )
      {
      case X_EXPR_KIND::NIL:     return STATIC_TYPE::NIL;
      case X_EXPR_KIND::NUMBER:  return IIF( expr.numberIsInteger, STATIC_TYPE::INTEGER, STATIC_TYPE::DOUBLE );
      case X_EXPR_KIND::STRING:  return STATIC_TYPE::STRING;
      case X_EXPR_KIND::LOGICAL: return STATIC_TYPE::LOGICAL;
      case X_EXPR_KIND::INDEX:   return STATIC_TYPE::PERSISTENT;
      case X_EXPR_KIND::NAME:    return nameType( expr );
      case X_EXPR_KIND::UNARY:   return IIF( expr.text == ".NOT.", STATIC_TYPE::LOGICAL, STATIC_TYPE::UNKNOWN );
      case X_EXPR_KIND::BINARY:  return binaryType( expr );
      default:                   return STATIC_TYPE::UNKNOWN;
      }
   }

   /* The LOGICAL of an item as an i1; the C int member is read, not the padding of the payload union */
   llvm::Value *logicalPayload( llvm::Value *item )
   {
      llvm::Value *raw = loadPayload( item, i32Ty() );

      return builder.CreateICmpNE( raw, constant32( 0 ) );
   }

   /* An operand that may be a LOGICAL */
   bool logicalType( const X_EXPR &expr ) const
   {
      return expr.type == X_DECL_TYPE::LOGIC || expr.type == X_DECL_TYPE::ANY;
   }

   /* The condition of an item by x_errorCondition() */
   llvm::Value *logicalCondition( llvm::Value *item )
   {
      llvm::Value *value = emitCall( runtime( "x_errorCondition", i32Ty(), { ptrTy() } ), { item } );

      return builder.CreateICmpNE( value, constant32( 0 ) );
   }

   /* An operator of the runtime: name( ctx, result, left [, right] ) */
   llvm::Value *operatorCall( const char *name, llvm::Value *left, llvm::Value *right = nullptr )
   {
      llvm::Value *result = freshItem( "operator.result" );
      VALUES args{ callCtx, result, left };

      if( right != nullptr )
         args.push_back( right );

      emitCall( runtime( name, i32Ty(), std::vector<llvm::Type *>( args.size(), ptrTy() ) ), args );

      return result;
   }

   /* A substituted AND/OR error supplies the whole result: its error edge goes straight to done, never
      back to the evaluation of an operand */
   void checkLogicalOperand( llvm::Value *value, llvm::Value *result, int code, llvm::Value *left, llvm::Value *right,
                             llvm::BasicBlock *done )
   {
      llvm::BasicBlock *valid = llvm::BasicBlock::Create( context, "logical.valid", currentFunc );
      llvm::BasicBlock *invalid = llvm::BasicBlock::Create( context, "logical.invalid", currentFunc );
      llvm::Value *type = loadTag( value );
      llvm::Value *second = IIF( right, right, nullPtr() );
      llvm::FunctionCallee raise;

      builder.CreateCondBr( tagIs( type, X_IT_LOGICAL ), valid, invalid );

      builder.SetInsertPoint( invalid );
      raise = runtime( "x_errorRaise", i32Ty(), { ptrTy(), i32Ty(), ptrTy(), ptrTy() } );
      emitCall( raise, { result, constant32( code ), left, second } );
      builder.CreateBr( done );

      builder.SetInsertPoint( valid );
   }

   /* An object with the AND/OR operator gets x_objOperatorCall() with the right operand */
   bool logicalOperator( const X_EXPR &expr, llvm::Value *operation, llvm::Value *result, llvm::Value *left )
   {
      const std::vector<llvm::Type *> params = { ptrTy(), i32Ty(), ptrTy(), ptrTy(), ptrTy(), ptrTy() };
      llvm::Value *argument = expression( *expr.args[1] );
      llvm::FunctionCallee call;

      if( argument == nullptr )
         return false;

      call = runtime( "x_objOperatorCall", i32Ty(), params );
      emitCall( call, { callCtx, operation, result, left, argument, nullPtr() } );

      return true;
   }

   /* .AND. and .OR. of items: an object with the operator gets the message, otherwise the operands must be
      LOGICAL and the right one is evaluated only when it decides */
   llvm::Value *logicalBinary( const X_EXPR &expr )
   {
      const bool isAnd = expr.text == ".AND.";
      const int code = IIF( isAnd, 1078, 1079 );
      llvm::Value *left = expression( *expr.args[0] );
      llvm::Value *snapshot;
      llvm::Value *result;
      llvm::Function *fn;
      llvm::BasicBlock *rightBB;
      llvm::BasicBlock *shortBB;
      llvm::BasicBlock *doneBB;
      llvm::BasicBlock *objectBB;
      llvm::BasicBlock *ordinaryBB;
      llvm::Value *operation;
      llvm::Value *hasOperator;
      llvm::Value *leftValue;
      llvm::Value *right;

      if( left == nullptr )
         return nullptr;

      snapshot = freshItem( "logical.left" );
      emitCall( itemCopyFn(), { snapshot, left } );
      left = snapshot;

      result = freshItem( "logical.result" );
      storeNType( result, X_IT_LOGICAL );

      fn = builder.GetInsertBlock()->getParent();
      rightBB = llvm::BasicBlock::Create( context, "logical.right", fn );
      shortBB = llvm::BasicBlock::Create( context, "logical.short", fn );
      doneBB = llvm::BasicBlock::Create( context, "logical.done", fn );
      objectBB = llvm::BasicBlock::Create( context, "logical.object", fn );
      ordinaryBB = llvm::BasicBlock::Create( context, "logical.ordinary", fn );
      operation = constant32( IIF( isAnd, X_OO_OP_AND, X_OO_OP_OR ) );
      hasOperator = emitCall( runtime( "x_objHasOperator", i32Ty(), { ptrTy(), i32Ty() } ), { left, operation } );
      builder.CreateCondBr( builder.CreateICmpNE( hasOperator, constant32( 0 ) ), objectBB, ordinaryBB );

      builder.SetInsertPoint( objectBB );
      if( !logicalOperator( expr, operation, result, left ) )
         return nullptr;

      builder.CreateBr( doneBB );

      builder.SetInsertPoint( ordinaryBB );
      checkLogicalOperand( left, result, code, left, nullptr, doneBB );
      leftValue = logicalPayload( left );
      builder.CreateCondBr( leftValue, IIF( isAnd, rightBB, shortBB ), IIF( isAnd, shortBB, rightBB ) );

      builder.SetInsertPoint( shortBB );
      storePayload( result, constant64( IIF( isAnd, 0, 1 ) ) );
      builder.CreateBr( doneBB );

      builder.SetInsertPoint( rightBB );
      right = expression( *expr.args[1] );
      if( right == nullptr )
         return nullptr;

      checkLogicalOperand( right, result, code, left, right, doneBB );
      storePayload( result, builder.CreateZExt( logicalPayload( right ), i64Ty() ) );
      builder.CreateBr( doneBB );

      builder.SetInsertPoint( doneBB );

      return result;
   }

   /* ---------------------------------------------------------------------------------------------
      Fields, aliases and assignments
      --------------------------------------------------------------------------------------------- */

   /* FIELD->name: its value, or a reference block for an assignment */
   llvm::Value *fieldExpression( const X_EXPR &expr, bool reference = false )
   {
      const char *name = IIF( reference, "x_dbfFieldReference", "x_dbfFieldRead" );
      llvm::Value *selector = expression( *expr.args[0] );
      llvm::FunctionCallee callee;
      llvm::Value *result;
      llvm::Value *text;

      if( selector == nullptr )
         return nullptr;

      callee = runtime( name, voidTy(), { ptrTy(), ptrTy(), ptrTy(), ptrTy() } );
      result = freshItem( IIF( reference, "field.reference", "field.value" ) );
      text = builder.CreateGlobalString( expr.text );
      emitCall( callee, { callCtx, result, selector, text } );

      return result;
   }

   /* Evaluates the reference block of a field: without a value it reads the field, with a value it assigns it */
   llvm::Value *evalFieldReference( llvm::Value *reference, llvm::Value *value = nullptr )
   {
      const std::vector<llvm::Type *> params = { ptrTy(), ptrTy(), ptrTy(), i32Ty(), ptrTy() };
      llvm::Value *argc = constant32( value != nullptr );
      llvm::Value *args = nullPtr();
      llvm::Value *result;

      if( value != nullptr )
      {
         llvm::BasicBlock &entry = currentFunc->getEntryBlock();
         llvm::IRBuilder<> entryBuilder( &entry, entry.getFirstInsertionPt() );

         args = entryBuilder.CreateAlloca( getArgType(), nullptr, "field.argument" );
         builder.CreateStore( value, builder.CreateStructGEP( getArgType(), args, 0 ) );
         builder.CreateStore( constant32( 0 ), builder.CreateStructGEP( getArgType(), args, 1 ) );
      }

      result = freshItem( "field.result" );
      emitCall( runtime( "x_evalBlock", voidTy(), params ), { callCtx, result, reference, argc, args } );

      return result;
   }

   /* x++ or x-- gives the previous value */
   static bool postfixUpdate( const X_EXPR &expr )
   {
      return expr.kind == X_EXPR_KIND::UPDATE && expr.updateMode == X_UPDATE_MODE::POSTFIX;
   }

   static const char *stepFunction( X_UPDATE_OP op )
   {
      return IIF( op == X_UPDATE_OP::INC, "x_itemInc", "x_itemDec" );
   }

   /* The runtime function of a compound assignment; an unknown operator is an error */
   const char *compoundFunction( const X_EXPR &expr )
   {
      switch( expr.assignOp )
      {
      case X_ASSIGN_OP::ADD: return "x_itemAdd";
      case X_ASSIGN_OP::SUB: return "x_itemSub";
      case X_ASSIGN_OP::MUL: return "x_itemMul";
      case X_ASSIGN_OP::DIV: return "x_itemDiv";
      case X_ASSIGN_OP::MOD: return "x_itemMod";
      case X_ASSIGN_OP::POW: return "x_itemPow";
      default:               break;
      }

      fail( expr.line, "nieznany operator przypisania zlozonego" );

      return nullptr;
   }

   /* x_itemArraySet( ctx, result, array, index, value ) */
   llvm::FunctionCallee arraySetFn()
   {
      return runtime( "x_itemArraySet", i32Ty(), std::vector<llvm::Type *>( 5, ptrTy() ) );
   }

   /* FIELD->name := value, op= value, ++ and -- go through the reference block of the field */
   llvm::Value *fieldAssignmentExpression( const X_EXPR &expr )
   {
      const bool update = expr.kind == X_EXPR_KIND::UPDATE;
      const bool compound = !update && expr.assignOp != X_ASSIGN_OP::SET;
      llvm::Value *reference = fieldExpression( *expr.args[0], true );
      llvm::Value *previous = nullptr;
      llvm::Value *value;
      llvm::Value *result;

      if( reference == nullptr )
         return nullptr;

      if( update || compound )
         previous = evalFieldReference( reference );

      if( update )
         value = operatorCall( stepFunction( expr.updateOp ), previous );
      else
         value = expression( *expr.args[1] );

      if( value == nullptr )
         return nullptr;

      if( compound )
         value = operatorCall( assignFunction( expr.assignOp ), previous, value );

      result = evalFieldReference( reference, value );

      return IIF( postfixUpdate( expr ), previous, result );
   }

   /* ( a, b, ... ) from begin on: the value of the last expression */
   llvm::Value *groupExpression( const X_EXPR &expr, size_t begin = 0 )
   {
      llvm::Value *result = freshItem( "group.value" );

      for( size_t i = begin; i < expr.args.size(); ++i )
      {
         llvm::Value *value = expression( *expr.args[i] );

         if( value == nullptr )
            return nullptr;

         emitCall( itemCopyFn(), { result, value } );
      }

      return result;
   }

   /* alias->( a, b, ... ): the work area is selected for the group and restored on every exit, also by an
      exception */
   llvm::Value *aliasExpression( const X_EXPR &expr )
   {
      llvm::Value *selector = expression( *expr.args[0] );
      llvm::Value *previous;
      llvm::FunctionCallee leave;
      llvm::BasicBlock *outerEh;
      llvm::BasicBlock *caught;
      llvm::BasicBlock *done;
      llvm::Value *result;
      llvm::LandingPadInst *pad;

      if( selector == nullptr )
         return nullptr;

      previous = emitCall( runtime( "x_dbfAliasEnter", i32Ty(), { ptrTy(), ptrTy() } ), { callCtx, selector } );
      leave = runtime( "x_dbfAliasLeave", voidTy(), { i32Ty() } );
      outerEh = unwindBB;
      caught = llvm::BasicBlock::Create( context, "alias.unwind", currentFunc );
      done = llvm::BasicBlock::Create( context, "alias.done", currentFunc );

      unwindBB = caught;
      result = groupExpression( expr, 1 );
      unwindBB = outerEh;
      if( result == nullptr )
         return nullptr;

      builder.CreateCall( leave, { previous } );
      builder.CreateBr( done );

      builder.SetInsertPoint( caught );
      pad = builder.CreateLandingPad( exceptionType(), 1 );
      pad->setCleanup( true );
      pad->addClause( nullPtr() );
      builder.CreateCall( leave, { previous } );
      emitCall( runtime( "x_exceptionResume", voidTy(), { ptrTy() } ), { builder.CreateExtractValue( pad, 0 ) } );
      builder.CreateUnreachable();

      builder.SetInsertPoint( done );

      return result;
   }

   /* The value of an expression copied into an item of its own, so it is evaluated once */
   llvm::Value *copiedItem( const X_EXPR &expr, const char *name )
   {
      llvm::Value *value = expression( expr );
      llvm::Value *copy;

      if( value == nullptr )
         return nullptr;

      copy = freshItem( name );
      emitCall( itemCopyFn(), { copy, value } );

      return copy;
   }

   /* The new value of an update: ++ or -- of the previous value, or previous op value */
   llvm::Value *updatedValue( const X_EXPR &expr, llvm::Value *previous )
   {
      llvm::Value *right;
      const char *name;

      if( expr.kind == X_EXPR_KIND::UPDATE )
         return operatorCall( stepFunction( expr.updateOp ), previous );

      right = expression( *expr.args[1] );
      if( right == nullptr )
         return nullptr;

      name = compoundFunction( expr );
      if( name == nullptr )
         return nullptr;

      return operatorCall( name, previous, right );
   }

   /* receiver:property op= value, ++ and --: the receiver is evaluated once, then the getter and the setter
      are sent */
   llvm::Value *propertyUpdateExpression( const X_EXPR &expr )
   {
      const X_EXPR &target = *expr.args[0];
      llvm::Value *receiver = nullptr;
      bool classSelf;
      llvm::Value *previous;
      llvm::Value *updated;

      if( target.sendHasParentheses || target.args.size() != 1 )
      {
         fail( expr.line, "aktualizacja wymaga wlasciwosci" );
         return nullptr;
      }

      classSelf = currentClassMethod && isSelf( *target.args[0] );
      if( !target.sendToSuper && !classSelf )
      {
         receiver = copiedItem( *target.args[0], "property.receiver" );
         if( receiver == nullptr )
            return nullptr;
      }

      previous = sendExpression( target, receiver );
      if( previous == nullptr )
         return nullptr;

      updated = updatedValue( expr, previous );
      if( updated == nullptr || sendExpression( target, receiver, updated ) == nullptr )
         return nullptr;

      return IIF( postfixUpdate( expr ), previous, updated );
   }

   /* array[ index ] op= value, ++ and --: the array and the index are evaluated once */
   llvm::Value *indexUpdateExpression( const X_EXPR &expr )
   {
      const X_EXPR &target = *expr.args[0];
      llvm::Value *array = copiedItem( *target.args[0], "index.array" );
      llvm::Value *index;
      llvm::Value *previous;
      llvm::Value *updated;
      llvm::Value *result;

      if( array == nullptr )
         return nullptr;

      index = copiedItem( *target.args[1], "index.position" );
      if( index == nullptr )
         return nullptr;

      previous = operatorCall( "x_itemArrayGet", array, index );
      updated = updatedValue( expr, previous );
      if( updated == nullptr )
         return nullptr;

      result = freshItem( "index.update" );
      emitCall( arraySetFn(), { callCtx, result, array, index, updated } );

      return IIF( postfixUpdate( expr ), previous, result );
   }

   /* array[ index ] := value: the value is evaluated first */
   llvm::Value *arrayAssignment( const X_EXPR &expr )
   {
      const X_EXPR &target = *expr.args[0];
      llvm::Value *value = expression( *expr.args[1] );
      llvm::Value *array;
      llvm::Value *index;
      llvm::Value *result;

      if( !value )
         return nullptr;

      array = expression( *target.args[0] );
      if( !array )
         return nullptr;

      index = expression( *target.args[1] );
      if( !index )
         return nullptr;

      result = freshItem( "array.assign" );
      emitCall( arraySetFn(), { callCtx, result, array, index, value } );

      return result;
   }

   /* x := value copies the value into the local and remembers its type */
   llvm::Value *localSet( const X_EXPR &expr, const std::string &key, llvm::Value *destination )
   {
      llvm::Value *value = expression( *expr.args[1] );
      llvm::Value *result;

      if( value == nullptr )
         return nullptr;

      emitCall( runtime( "x_itemAssign", i32Ty(), { ptrTy(), ptrTy(), ptrTy() } ), { callCtx, destination, value } );
      result = freshItem( "assign.result" );
      emitCall( itemCopyFn(), { result, destination } );
      localTypes[key] = expressionType( *expr.args[1] );

      return result;
   }

   /* x := value and x op= value of a local item; x += value appends in place by x_itemAddAssign() */
   llvm::Value *localAssignment( const X_EXPR &expr )
   {
      const X_EXPR &target = *expr.args[0];
      const std::string key = s_key( target.text );
      const auto found = locals.find( key );
      llvm::Value *destination;
      llvm::Value *left;
      llvm::Value *right;
      const char *name;
      llvm::Value *result;

      if( found == locals.end() )
      {
         fail( target.line, "blad wewnetrzny: brak magazynu rozstrzygnietej zmiennej " + target.text );
         return nullptr;
      }

      destination = localStorage( found->second );

      if( expr.assignOp == X_ASSIGN_OP::SET )
         return localSet( expr, key, destination );

      left = freshItem( "compound.left" );
      emitCall( itemCopyFn(), { left, destination } );
      right = expression( *expr.args[1] );
      if( right == nullptr )
         return nullptr;

      name = compoundFunction( expr );
      if( name == nullptr )
         return nullptr;

      if( expr.assignOp == X_ASSIGN_OP::ADD )
      {
         llvm::FunctionCallee append = runtime( "x_itemAddAssign", i32Ty(), std::vector<llvm::Type *>( 5, ptrTy() ) );

         result = freshItem( "compound.result" );
         emitCall( append, { callCtx, result, destination, left, right } );
      }
      else
      {
         result = operatorCall( name, left, right );
         emitCall( itemCopyFn(), { destination, result } );
         emitCall( itemCopyFn(), { result, destination } );
      }

      localTypes[key] = STATIC_TYPE::UNKNOWN;

      return result;
   }

   /* x := value and x op= value; a FIELD, a SEND and an INDEX target have their own forms */
   llvm::Value *assignmentExpression( const X_EXPR &expr )
   {
      if( expr.args.size() != 2 )
      {
         fail( expr.line, "nieprawidlowe przypisanie" );
         return nullptr;
      }

      const X_EXPR &target = *expr.args[0];
      const bool compound = expr.assignOp != X_ASSIGN_OP::SET;

      if( target.kind == X_EXPR_KIND::FIELD )
         return fieldAssignmentExpression( expr );

      if( target.kind == X_EXPR_KIND::SEND )
         return propertyUpdateExpression( expr );

      if( compound && target.kind == X_EXPR_KIND::INDEX )
         return indexUpdateExpression( expr );

      if( compound && target.kind != X_EXPR_KIND::NAME )
      {
         fail( expr.line, "przypisanie zlozone wymaga nazwy zmiennej" );
         return nullptr;
      }

      if( target.kind == X_EXPR_KIND::INDEX )
         return arrayAssignment( expr );

      if( target.kind != X_EXPR_KIND::NAME )
      {
         fail( expr.line, "nieprawidlowa lewa strona przypisania" );
         return nullptr;
      }

      return localAssignment( expr );
   }

   /* ---------------------------------------------------------------------------------------------
      Item expressions
      --------------------------------------------------------------------------------------------- */

   /* ARRAY, CODEBLOCK, DATE and HASH are checked by x_itemIsValueType() */
   static bool checkedValueType( X_DECL_TYPE type )
   {
      if( type == X_DECL_TYPE::ARRAY || type == X_DECL_TYPE::CODEBLOCK )
         return true;

      return type == X_DECL_TYPE::DATE || type == X_DECL_TYPE::HASH;
   }

   llvm::Value *isKindOf( llvm::Value *value, const std::string &className )
   {
      llvm::Value *text = builder.CreateGlobalString( className );
      llvm::Value *kind = emitCall( runtime( "x_objIsKindOf", i32Ty(), { ptrTy(), ptrTy() } ), { value, text } );

      return builder.CreateICmpNE( kind, constant32( 0 ) );
   }

   llvm::Value *isValueType( llvm::Value *value, X_DECL_TYPE type )
   {
      llvm::Value *code = constant32( dataValueType( type ) );
      llvm::Value *kind = emitCall( runtime( "x_itemIsValueType", i32Ty(), { ptrTy(), i32Ty() } ), { value, code } );

      return builder.CreateICmpNE( kind, constant32( 0 ) );
   }

   /* A checked conversion of an item: an item of a wrong type raises error 3107 */
   void checkConversion( const X_EXPR &expr, llvm::Value *value )
   {
      const X_DECL_TYPE target = expr.conversion->target;
      llvm::Value *valid;

      if( target == X_DECL_TYPE::STRING )
         valid = tagIs( loadTag( value ), X_IT_STRING );
      else if( target == X_DECL_TYPE::OBJECT )
         valid = isKindOf( value, expr.conversionClass );
      else if( checkedValueType( target ) )
         valid = isValueType( value, target );
      else
         return;

      nativeGuard( valid, 3107 );
   }

   /* The item of an expression; a native expression is boxed */
   llvm::Value *expression( const X_EXPR &expr )
   {
      llvm::SaveAndRestore<int> lineScope( srcLine, expr.line );
      const bool isNative = expr.nativeType && nativeType( expr.type, expr.typeName );
      const bool toNative = expr.conversion && nativeType( expr.conversion->target, expr.typeName );
      llvm::Value *value;

      if( isNative || toNative )
         return nativeItem( nativeExpression( expr ) );

      value = itemExpression( expr );
      if( value && expr.conversion && expr.conversion->checkType )
         checkConversion( expr, value );

      return value;
   }

   llvm::Value *boxedScalar( const X_EXPR &expr )
   {
      llvm::Value *value = scalarExpr( expr );

      if( value == nullptr )
         return nullptr;

      return scalarItem( value );
   }

   /* array[ index ] */
   llvm::Value *indexExpression( const X_EXPR &expr )
   {
      llvm::Value *array = expression( *expr.args[0] );
      llvm::Value *index;

      if( !array )
         return nullptr;

      index = expression( *expr.args[1] );
      if( !index )
         return nullptr;

      return operatorCall( "x_itemArrayGet", array, index );
   }

   /* ++x and x++ of a local item */
   llvm::Value *localUpdate( const X_EXPR &expr )
   {
      const std::string &name = expr.args[0]->text;
      const bool postfix = expr.updateMode == X_UPDATE_MODE::POSTFIX;
      const auto found = locals.find( s_key( name ) );
      llvm::Value *destination;
      llvm::Value *oldValue = nullptr;
      llvm::Value *updated;

      if( found == locals.end() )
      {
         fail( expr.args[0]->line, "blad wewnetrzny: brak magazynu rozstrzygnietej zmiennej " + name );
         return nullptr;
      }

      destination = localStorage( found->second );
      if( postfix )
      {
         oldValue = freshItem( "update.old" );
         emitCall( itemCopyFn(), { oldValue, destination } );
      }

      updated = operatorCall( stepFunction( expr.updateOp ), destination );
      emitCall( itemCopyFn(), { destination, updated } );
      if( !postfix )
         emitCall( itemCopyFn(), { updated, destination } );

      localTypes[s_key( name )] = STATIC_TYPE::UNKNOWN;

      return IIF( postfix, oldValue, updated );
   }

   /* ++ and -- of a local item; a FIELD, a SEND and an INDEX have their own forms */
   llvm::Value *updateExpression( const X_EXPR &expr )
   {
      const bool increment = expr.updateOp == X_UPDATE_OP::INC;
      const char *message = IIF( increment, "operator ++ wymaga lvalue", "operator -- wymaga lvalue" );
      const X_EXPR *target = IIF( expr.args.empty(), nullptr, expr.args[0].get() );

      if( target != nullptr && target->kind == X_EXPR_KIND::FIELD )
         return fieldAssignmentExpression( expr );

      if( target != nullptr && target->kind == X_EXPR_KIND::SEND )
         return propertyUpdateExpression( expr );

      if( target != nullptr && target->kind == X_EXPR_KIND::INDEX )
         return indexUpdateExpression( expr );

      if( target == nullptr || target->kind != X_EXPR_KIND::NAME )
      {
         fail( expr.line, message );
         return nullptr;
      }

      return localUpdate( expr );
   }

   llvm::Value *unaryExpression( const X_EXPR &expr )
   {
      llvm::Value *value = expression( *expr.args[0] );

      if( value == nullptr )
         return nullptr;

      if( expr.text == "+" )
         return value;

      if( expr.text == "-" )
         return operatorCall( "x_itemNeg", value );

      if( expr.text == ".NOT." )
         return operatorCall( "x_itemNot", value );

      fail( expr.line, "nieznany operator unarny: " + expr.text );

      return nullptr;
   }

   /* The runtime function of a binary operator; NULL for an unknown one */
   static const char *binaryFunction( const std::string &op )
   {
      static const char *const table[][2] =
      {
         { "+",  "x_itemAdd"      },
         { "-",  "x_itemSub"      },
         { "*",  "x_itemMul"      },
         { "/",  "x_itemDiv"      },
         { "%",  "x_itemMod"      },
         { "^",  "x_itemPow"      },
         { "$",  "x_itemContains" },
         { "=",  "x_itemEqS"      },
         { "==", "x_itemEq"       },
         { "!=", "x_itemNe"       },
         { "<",  "x_itemLt"       },
         { "<=", "x_itemLe"       },
         { ">",  "x_itemGt"       },
         { ">=", "x_itemGe"       }
      };

      for( const auto &entry : table )
      {
         if( op == entry[0] )
            return entry[1];
      }

      return nullptr;
   }

   llvm::Value *binaryExpression( const X_EXPR &expr )
   {
      llvm::Value *left;
      llvm::Value *right;
      const char *name;

      if( expr.text == ".AND." || expr.text == ".OR." )
         return logicalBinary( expr );

      left = expression( *expr.args[0] );
      if( left == nullptr )
         return nullptr;

      right = expression( *expr.args[1] );
      if( right == nullptr )
         return nullptr;

      name = binaryFunction( expr.text );
      if( name == nullptr )
      {
         fail( expr.line, "nieznany operator: " + expr.text );
         return nullptr;
      }

      return operatorCall( name, left, right );
   }

   /* The item of an expression by its kind; a scalar expression is computed natively and boxed */
   llvm::Value *itemExpression( const X_EXPR &expr )
   {
      llvm::SaveAndRestore<int> lineScope( srcLine, expr.line );
      std::string kind;

      if( scalarType( expr ) != STATIC_TYPE::UNKNOWN )
         return boxedScalar( expr );

      switch( expr.kind )
      {
      case X_EXPR_KIND::NIL:    return nilItem();
      case X_EXPR_KIND::STRING: return stringItem( expr.text );
      case X_EXPR_KIND::ARRAY:  return arrayExpression( expr );
      case X_EXPR_KIND::INDEX:  return indexExpression( expr );
      case X_EXPR_KIND::NAME:   return variableItem( expr.text, expr.line );
      case X_EXPR_KIND::FIELD:  return fieldExpression( expr );
      case X_EXPR_KIND::ALIAS:  return aliasExpression( expr );
      case X_EXPR_KIND::GROUP:  return groupExpression( expr );
      case X_EXPR_KIND::CALL:   return callExpression( expr );
      case X_EXPR_KIND::SEND:   return sendExpression( expr );
      case X_EXPR_KIND::BLOCK:  return blockExpression( expr );
      case X_EXPR_KIND::IIF:    return iifExpression( expr );
      case X_EXPR_KIND::CAST:   return expression( *expr.args[0] );
      case X_EXPR_KIND::UPDATE: return updateExpression( expr );
      case X_EXPR_KIND::ASSIGN: return assignmentExpression( expr );
      case X_EXPR_KIND::UNARY:  return unaryExpression( expr );
      case X_EXPR_KIND::BINARY: return binaryExpression( expr );
      default:                  break;
      }

      kind = std::to_string( ( int )expr.kind );
      fail( expr.line, "wyrazenie (rodzaj AST nr " + kind + ") nieobslugiwane jeszcze w backendzie" );

      return nullptr;
   }

   /* ---------------------------------------------------------------------------------------------
      Simple statements
      --------------------------------------------------------------------------------------------- */

   /* An assignment statement: native, scalar or item */
   bool simpleAssign( const X_EXPR &expr )
   {
      if( expr.nativeType && nativeType( expr.type, expr.typeName ) )
         return nativeExpression( expr ).value != nullptr;

      if( scalarType( expr ) != STATIC_TYPE::UNKNOWN )
         return scalarExpr( expr ) != nullptr;

      return assignmentExpression( expr ) != nullptr;
   }

   /* A native local starts with its INIT or with zero */
   bool nativeLocal( const X_LOCAL_DECL &decl, const std::string &key, X_DECL_TYPE declared, llvm::Type *type )
   {
      const auto found = nativeLocals.find( key );
      NATIVE_VALUE value = { llvm::Constant::getNullValue( type ), declared, nullptr, false, decl.declaredTypeName };

      if( decl.init )
         value = nativeExpression( *decl.init );

      if( !value.value )
         return false;

      nativeStore( found->second, value.value );

      return true;
   }

   /* A scalar local gets its alloca here; a NUMERIC local stores its INIT as a NUMERIC */
   bool scalarLocal( const X_LOCAL_DECL &decl, const std::string &key, STATIC_TYPE type )
   {
      llvm::BasicBlock &entry = currentFunc->getEntryBlock();
      llvm::Value *value = nullptr;
      llvm::AllocaInst *slot;

      if( decl.init )
      {
         value = scalarExpr( *decl.init );
         if( value == nullptr )
            return false;
      }

      llvm::IRBuilder<> entryBuilder( &entry, entry.getFirstInsertionPt() );

      slot = entryBuilder.CreateAlloca( scalarLLVM( type ), nullptr, "local." + decl.name );
      if( decl.init )
         builder.CreateStore( IIF( type == STATIC_TYPE::NUMERIC, scalarNumber( value ), value ), slot );

      locals[key] = slot;
      localTypes[key] = type;

      return true;
   }

   /* An item local starts with its INIT, its dimensions, an empty date or NIL */
   bool itemLocal( const X_LOCAL_DECL &decl, const std::string &key, X_DECL_TYPE declared )
   {
      const size_t first = itemSlots.size();
      const bool initialized = decl.init || !decl.dimensions.empty();
      llvm::Value *slot = locals.at( key );
      STATIC_TYPE type = STATIC_TYPE::NIL;
      llvm::Value *value = nullptr;

      if( !decl.dimensions.empty() || declared == X_DECL_TYPE::DATE )
         type = STATIC_TYPE::UNKNOWN;
      else if( decl.init )
         type = expressionType( *decl.init );

      localTypes[key] = type;

      if( decl.init )
         value = expression( *decl.init );
      else if( !decl.dimensions.empty() )
         value = dimensionExpression( decl.dimensions );
      else if( declared == X_DECL_TYPE::DATE )
         value = emptyDateItem();

      if( initialized && value == nullptr )
         return false;

      if( value != nullptr )
         emitCall( itemCopyFn(), { slot, value } );

      clearTemps( first, itemSlots.size() );

      return true;
   }

   bool localDeclaration( const X_LOCAL_DECL &decl )
   {
      const std::string key = s_key( decl.name );
      const X_DECL_TYPE declared = decl.declaredType.value_or( X_DECL_TYPE::ANY );
      llvm::Type *type = nativeType( declared, decl.declaredTypeName );
      const auto scalar = scalars.find( key );

      if( type != nullptr )
         return nativeLocal( decl, key, declared, type );

      if( scalar != scalars.end() )
         return scalarLocal( decl, key, scalar->second );

      return itemLocal( decl, key, declared );
   }

   /* LOCAL declarations: a native local, a scalar local or an item */
   bool localStatement( const X_STMT &stmt )
   {
      for( const X_LOCAL_DECL &decl : stmt.locals )
      {
         if( !localDeclaration( decl ) )
            return false;
      }

      return true;
   }

   /* A native result is stored directly; a STRING keeps its text in the result item */
   bool nativeReturn( const X_EXPR &expr )
   {
      NATIVE_VALUE value = nativeExpression( expr );

      if( !value.value )
         return false;

      if( nativeResultType == X_DECL_TYPE::STRING )
      {
         emitCall( itemCopyFn(), { currentResult, nativeItem( value ) } );
         value = nativeUnbox( currentResult, X_DECL_TYPE::STRING, false );
      }

      builder.CreateStore( value.value, nativeResult );
      builder.CreateBr( cleanupBB );

      return true;
   }

   /* RETURN [value] goes to the cleanup of the function */
   bool returnStatement( const X_STMT &stmt )
   {
      llvm::Value *value;

      if( stmt.expr && nativeResult )
         return nativeReturn( *stmt.expr );

      if( stmt.expr )
      {
         value = expression( *stmt.expr );
         if( value == nullptr )
            return false;

         emitCall( itemCopyFn(), { currentResult, value } );
      }

      builder.CreateBr( cleanupBB );

      return true;
   }

   /* A local whose type changes in a loop body becomes UNKNOWN; true when the type changed */
   bool mergeLoopType( const std::string &name, STATIC_TYPE type )
   {
      const auto found = localTypes.find( s_key( name ) );

      if( found == localTypes.end() || found->second == STATIC_TYPE::UNKNOWN || found->second == type )
         return false;

      found->second = STATIC_TYPE::UNKNOWN;

      return true;
   }

   /* The types a loop body may assign; true when a type changed */
   bool mergeLoopTypes( const std::vector<X_STMT> &body )
   {
      bool changed = false;

      for( const X_STMT &child : body )
      {
         const bool assign = child.kind == X_STMT_KIND::CALL && child.expr && child.expr->kind == X_EXPR_KIND::ASSIGN;

         if( child.kind == X_STMT_KIND::FOR )
            changed |= mergeLoopType( child.name, STATIC_TYPE::UNKNOWN );

         if( child.kind == X_STMT_KIND::SEQUENCE && !child.name.empty() )
            changed |= mergeLoopType( child.name, STATIC_TYPE::UNKNOWN );

         if( assign && child.expr->args[0]->kind == X_EXPR_KIND::NAME )
            changed |= mergeLoopType( child.expr->args[0]->text, expressionType( *child.expr->args[1] ) );

         for( const X_LOCAL_DECL &decl : child.locals )
            changed |= mergeLoopType( decl.name, IIF( decl.init, expressionType( *decl.init ), STATIC_TYPE::NIL ) );

         changed |= mergeLoopTypes( child.body );
         changed |= mergeLoopTypes( child.elseBody );

         for( const X_CASE_BRANCH &branch : child.cases )
            changed |= mergeLoopTypes( branch.body );

         changed |= mergeLoopTypes( child.otherwiseBody );
      }

      return changed;
   }

   /* ---------------------------------------------------------------------------------------------
      Statement bodies
      --------------------------------------------------------------------------------------------- */

   /* Whether RETURN, EXIT or LOOP closed the current block; getTerminator() of LLVM 23 asserts on an open
      block, so the last instruction is checked */
   bool terminated()
   {
      llvm::BasicBlock *block = builder.GetInsertBlock();

      return !block->empty() && block->back().isTerminator();
   }

   /* Closes the current block with a branch, unless RETURN, EXIT or LOOP closed it */
   void closeBlock( llvm::BasicBlock *target )
   {
      if( !terminated() )
         builder.CreateBr( target );
   }

   /* The statements of a body, up to one that closes the block */
   bool statementList( const std::vector<X_STMT> &body )
   {
      for( const X_STMT &child : body )
      {
         if( !statement( child ) )
            return false;

         if( terminated() )
            break;
      }

      return true;
   }

   /* The body of a loop: LOOP goes to next, EXIT to done */
   bool loopBody( const std::vector<X_STMT> &body, llvm::BasicBlock *next, llvm::BasicBlock *done )
   {
      bool ok;

      loopTargets.push_back( { next, done } );
      ok = statementList( body );
      loopTargets.pop_back();

      return ok;
   }

   /* The types a loop body may assign are merged until none changes */
   void settleLoopTypes( const std::vector<X_STMT> &body )
   {
      bool changed;

      do
      {
         changed = mergeLoopTypes( body );
      } while( changed );
   }

   /* ---------------------------------------------------------------------------------------------
      FOR and DO WHILE
      --------------------------------------------------------------------------------------------- */

   /* x_forTest( ctx, result, counter, limit, step ) as an i1 */
   llvm::Value *forTest( const char *name, llvm::Value *counter, llvm::Value *limit, llvm::Value *step )
   {
      llvm::Value *result = freshItem( name );
      llvm::FunctionCallee test = runtime( "x_forTest", i32Ty(), std::vector<llvm::Type *>( 5, ptrTy() ) );

      emitCall( test, { callCtx, result, counter, limit, step } );

      return logicalCondition( result );
   }

   /* The step of a typed FOR: its expression, or one in the operation type */
   NATIVE_VALUE typedForStep( const X_STMT &stmt )
   {
      llvm::Type *type;
      llvm::Value *one;

      if( stmt.forStep )
         return nativeExpression( *stmt.forStep );

      type = nativeType( stmt.operationType );
      if( type->isFloatingPointTy() )
         one = llvm::ConstantFP::get( type, 1.0 );
      else
         one = llvm::ConstantInt::get( type, 1 );

      return { one, stmt.operationType };
   }

   /* The test of a typed counter with an ANY operation or limit: x_forTest() of items */
   llvm::Value *dynamicForTest( const X_STMT &stmt, llvm::Value *saved )
   {
      llvm::Value *limit = expression( *stmt.forLimit );
      llvm::Value *limitCopy;
      llvm::Value *step;

      if( !saved || !limit )
         return nullptr;

      limitCopy = freshItem( "typed.for.limit" );
      emitCall( itemCopyFn(), { limitCopy, limit } );
      step = IIF( stmt.forStep, expression( *stmt.forStep ), numberItem( 1, true, 1 ) );
      if( !step )
         return nullptr;

      return forTest( "typed.for.test", saved, limitCopy, step );
   }

   /* The test of a typed counter: a negative step stops below the limit, any other above it */
   llvm::Value *typedForTest( const X_STMT &stmt, const NATIVE_VALUE &counter )
   {
      const bool dynamicTest = stmt.operationType == X_DECL_TYPE::ANY || stmt.forLimit->type == X_DECL_TYPE::ANY;
      NATIVE_VALUE saved = nativeLoad( counter );
      NATIVE_VALUE limit;
      NATIVE_VALUE step;
      llvm::Value *up;
      llvm::Value *down;
      llvm::Value *back;

      if( dynamicTest )
         return dynamicForTest( stmt, nativeItem( saved ) );

      limit = nativeExpression( *stmt.forLimit );
      if( !limit.value )
         return nullptr;

      step = typedForStep( stmt );
      if( !step.value )
         return nullptr;

      saved = nativeConvert( saved, { limit.type } );
      if( !saved.value )
         return nullptr;

      up = native.compare( saved.value, limit.value, limit.type, ">" );
      down = native.compare( saved.value, limit.value, limit.type, "<" );
      back = native.compare( step.value, llvm::Constant::getNullValue( step.value->getType() ), step.type, "<" );

      return builder.CreateSelect( back, down, up );
   }

   /* The next value of a typed counter: x_itemAdd() for an ANY operation, else a native + */
   NATIVE_VALUE typedForNext( const X_STMT &stmt, const NATIVE_VALUE &counter )
   {
      llvm::Value *increment;
      llvm::Value *current;
      NATIVE_VALUE step;
      NATIVE_VALUE value;
      NATIVE_VALUE next;

      if( stmt.operationType == X_DECL_TYPE::ANY )
      {
         increment = IIF( stmt.forStep, expression( *stmt.forStep ), numberItem( 1, true, 1 ) );
         current = nativeItem( nativeLoad( counter ) );
         if( !increment || !current )
            return {};

         return nativeUnbox( operatorCall( "x_itemAdd", current, increment ), counter.type, false );
      }

      step = typedForStep( stmt );
      if( !step.value )
         return {};

      value = nativeConvert( nativeLoad( counter ), { stmt.operationType } );
      if( !value.value )
         return {};

      next = { native.arithmetic( value.value, step.value, stmt.operationType, "+" ), stmt.operationType };
      if( stmt.storeConversion )
         next = nativeConvert( next, *stmt.storeConversion );

      return next;
   }

   /* FOR of a typed counter */
   bool typedFor( const X_STMT &stmt, const NATIVE_VALUE counter )
   {
      size_t first = itemSlots.size();
      NATIVE_VALUE start = nativeExpression( *stmt.expr );
      llvm::BasicBlock *testBB;
      llvm::BasicBlock *bodyBB;
      llvm::BasicBlock *updateBB;
      llvm::BasicBlock *doneBB;
      llvm::Value *stop;
      NATIVE_VALUE next;

      if( !start.value )
         return false;

      nativeStore( counter, start.value );
      clearTemps( first, itemSlots.size() );
      testBB = llvm::BasicBlock::Create( context, "typed.for.test", currentFunc );
      bodyBB = llvm::BasicBlock::Create( context, "typed.for.body", currentFunc );
      updateBB = llvm::BasicBlock::Create( context, "typed.for.update", currentFunc );
      doneBB = llvm::BasicBlock::Create( context, "typed.for.done", currentFunc );
      builder.CreateBr( testBB );

      builder.SetInsertPoint( testBB );
      first = itemSlots.size();
      stop = typedForTest( stmt, counter );
      if( stop == nullptr )
         return false;

      clearTemps( first, itemSlots.size() );
      builder.CreateCondBr( stop, doneBB, bodyBB );

      builder.SetInsertPoint( bodyBB );
      if( !loopBody( stmt.body, updateBB, doneBB ) )
         return false;

      closeBlock( updateBB );

      builder.SetInsertPoint( updateBB );
      first = itemSlots.size();
      next = typedForNext( stmt, counter );
      if( !next.value )
         return false;

      nativeStore( counter, next.value );
      clearTemps( first, itemSlots.size() );
      builder.CreateBr( testBB );

      builder.SetInsertPoint( doneBB );

      return true;
   }

   /* The test of a scalar counter; with a STEP its sign chooses the direction */
   bool scalarForTest( const X_STMT &stmt, STATIC_TYPE kind, llvm::Value *counter, llvm::BasicBlock *body,
                       llvm::BasicBlock *done )
   {
      llvm::Value *saved = builder.CreateLoad( scalarLLVM( kind ), counter );
      llvm::Value *limit = scalarExpr( *stmt.forLimit );
      llvm::Value *step;

      if( limit == nullptr )
         return false;

      step = IIF( stmt.forStep, scalarExpr( *stmt.forStep ), constant64( 1 ) );
      if( step == nullptr )
         return false;

      if( stmt.forStep )
      {
         llvm::Value *back = scalarCompare( step, constant64( 0 ), "<" );
         llvm::BasicBlock *downBB = llvm::BasicBlock::Create( context, "for.down", currentFunc );
         llvm::BasicBlock *upBB = llvm::BasicBlock::Create( context, "for.up", currentFunc );

         builder.CreateCondBr( back, downBB, upBB );

         builder.SetInsertPoint( downBB );
         builder.CreateCondBr( scalarCompare( saved, limit, "<" ), done, body );

         builder.SetInsertPoint( upBB );
      }

      builder.CreateCondBr( scalarCompare( saved, limit, ">" ), done, body );

      return true;
   }

   /* The test of an item counter: x_forTest() with copies of the counter and the limit */
   bool itemForTest( const X_STMT &stmt, llvm::Value *counter, llvm::BasicBlock *body, llvm::BasicBlock *done )
   {
      const size_t first = itemSlots.size();
      llvm::Value *saved = freshItem( "for.counter" );
      llvm::Value *limit;
      llvm::Value *copy;
      llvm::Value *step;
      llvm::Value *stop;

      emitCall( itemCopyFn(), { saved, counter } );
      limit = expression( *stmt.forLimit );
      if( limit == nullptr )
         return false;

      copy = freshItem( "for.limit" );
      emitCall( itemCopyFn(), { copy, limit } );
      step = IIF( stmt.forStep, expression( *stmt.forStep ), numberItem( 1, true, 1 ) );
      if( step == nullptr )
         return false;

      stop = forTest( "for.result", saved, copy, step );
      clearTemps( first, itemSlots.size() );
      builder.CreateCondBr( stop, done, body );

      return true;
   }

   /* counter += step of a scalar counter */
   bool scalarForNext( const X_STMT &stmt, STATIC_TYPE kind, llvm::Value *counter )
   {
      llvm::Value *step = IIF( stmt.forStep, scalarExpr( *stmt.forStep ), constant64( 1 ) );
      llvm::Value *value;
      llvm::Value *next;

      if( step == nullptr )
         return false;

      value = builder.CreateLoad( scalarLLVM( kind ), counter );
      next = scalarMath( value, step, '+' );
      builder.CreateStore( IIF( kind == STATIC_TYPE::NUMERIC, scalarNumber( next ), next ), counter );

      return true;
   }

   /* counter += step of an item counter; without STEP x_forIncrement() */
   bool itemForNext( const X_STMT &stmt, llvm::Value *counter )
   {
      llvm::Value *increment = nullptr;
      llvm::Value *next;

      if( stmt.forStep )
      {
         increment = expression( *stmt.forStep );
         if( increment == nullptr )
            return false;
      }

      if( increment != nullptr )
         next = operatorCall( "x_itemAdd", counter, increment );
      else
         next = operatorCall( "x_forIncrement", counter );

      emitCall( itemCopyFn(), { counter, next } );

      return true;
   }

   /* FOR of a local: a numeric scalar counter is native, an item counter is tested by x_forTest() */
   bool itemFor( const X_STMT &stmt, const std::string &key )
   {
      const auto found = locals.find( key );
      const auto type = scalars.find( key );
      const STATIC_TYPE kind = IIF( type == scalars.end(), STATIC_TYPE::UNKNOWN, type->second );
      const bool scalar = scalarNumeric( kind );
      size_t first = itemSlots.size();
      llvm::Value *start;
      llvm::Value *counter;
      LOCAL_TYPES loopTypes;
      llvm::BasicBlock *testBB;
      llvm::BasicBlock *bodyBB;
      llvm::BasicBlock *updateBB;
      llvm::BasicBlock *doneBB;

      if( found == locals.end() )
      {
         fail( stmt.line, "FOR wymaga istniejacej zmiennej lokalnej lub parametru: " + stmt.name );
         return false;
      }

      start = IIF( scalar, scalarExpr( *stmt.expr ), expression( *stmt.expr ) );
      if( start == nullptr )
         return false;

      counter = IIF( scalar, found->second, localStorage( found->second ) );
      if( scalar )
         builder.CreateStore( IIF( kind == STATIC_TYPE::NUMERIC, scalarNumber( start ), start ), counter );
      else
         emitCall( itemCopyFn(), { counter, start } );

      clearTemps( first, itemSlots.size() );
      localTypes[key] = kind;
      settleLoopTypes( stmt.body );
      loopTypes = localTypes;

      testBB = llvm::BasicBlock::Create( context, "for.test", currentFunc );
      bodyBB = llvm::BasicBlock::Create( context, "for.body", currentFunc );
      updateBB = llvm::BasicBlock::Create( context, "for.update", currentFunc );
      doneBB = llvm::BasicBlock::Create( context, "for.done", currentFunc );
      builder.CreateBr( testBB );

      builder.SetInsertPoint( testBB );
      if( scalar && !scalarForTest( stmt, kind, counter, bodyBB, doneBB ) )
         return false;

      if( !scalar && !itemForTest( stmt, counter, bodyBB, doneBB ) )
         return false;

      builder.SetInsertPoint( bodyBB );
      if( !loopBody( stmt.body, updateBB, doneBB ) )
         return false;

      closeBlock( updateBB );

      builder.SetInsertPoint( updateBB );
      localTypes = loopTypes;
      first = itemSlots.size();
      if( scalar && !scalarForNext( stmt, kind, counter ) )
         return false;

      if( !scalar && !itemForNext( stmt, counter ) )
         return false;

      clearTemps( first, itemSlots.size() );
      builder.CreateBr( testBB );

      builder.SetInsertPoint( doneBB );
      localTypes = loopTypes;

      return true;
   }

   /* FOR: a typed counter or a local */
   bool forStatement( const X_STMT &stmt )
   {
      const std::string key = s_key( stmt.name );
      const auto typedCounter = nativeLocals.find( key );

      if( typedCounter != nativeLocals.end() )
         return typedFor( stmt, typedCounter->second );

      return itemFor( stmt, key );
   }

   /* DO WHILE: the condition is tested before each pass */
   bool whileStatement( const X_STMT &stmt )
   {
      LOCAL_TYPES loopTypes;
      llvm::BasicBlock *conditionBB;
      llvm::BasicBlock *bodyBB;
      llvm::BasicBlock *doneBB;
      llvm::Value *value;

      settleLoopTypes( stmt.body );
      loopTypes = localTypes;

      if( !logicalType( *stmt.expr ) )
      {
         fail( stmt.line, "warunek DO WHILE musi zwracac X_IT_LOGICAL (brak niejawnego rzutowania)" );
         return false;
      }

      conditionBB = llvm::BasicBlock::Create( context, "while.condition", currentFunc );
      bodyBB = llvm::BasicBlock::Create( context, "while.body", currentFunc );
      doneBB = llvm::BasicBlock::Create( context, "while.done", currentFunc );
      builder.CreateBr( conditionBB );

      builder.SetInsertPoint( conditionBB );
      value = scalarCondition( *stmt.expr );
      if( value == nullptr )
         return false;

      /* The condition may have added blocks for its short-circuit evaluation: the branch leaves the last one */
      builder.CreateCondBr( value, bodyBB, doneBB );

      builder.SetInsertPoint( bodyBB );
      if( !loopBody( stmt.body, conditionBB, doneBB ) )
         return false;

      closeBlock( conditionBB );

      builder.SetInsertPoint( doneBB );
      localTypes = loopTypes;

      return true;
   }

   /* ---------------------------------------------------------------------------------------------
      IF, DO CASE, SWITCH and BEGIN SEQUENCE
      --------------------------------------------------------------------------------------------- */

   /* IF: both branches start with the types from before; after ENDIF a local keeps the type both branches
      agree on */
   bool ifStatement( const X_STMT &stmt )
   {
      llvm::Value *value;
      llvm::Function *fn;
      llvm::BasicBlock *thenBB;
      llvm::BasicBlock *elseBB;
      llvm::BasicBlock *doneBB;
      LOCAL_TYPES before;
      LOCAL_TYPES thenTypes;
      LOCAL_TYPES elseTypes;
      bool thenTerminated;
      bool elseTerminated;

      if( !logicalType( *stmt.expr ) )
      {
         fail( stmt.line, "warunek IF musi zwracac X_IT_LOGICAL (brak niejawnego rzutowania)" );
         return false;
      }

      value = scalarCondition( *stmt.expr );
      if( value == nullptr )
         return false;

      fn = builder.GetInsertBlock()->getParent();
      thenBB = llvm::BasicBlock::Create( context, "if.then", fn );
      elseBB = llvm::BasicBlock::Create( context, "if.else", fn );
      doneBB = llvm::BasicBlock::Create( context, "if.done", fn );
      builder.CreateCondBr( value, thenBB, elseBB );

      before = localTypes;
      builder.SetInsertPoint( thenBB );
      if( !statementList( stmt.body ) )
         return false;

      thenTerminated = terminated();
      thenTypes = localTypes;
      if( !thenTerminated )
         builder.CreateBr( doneBB );

      localTypes = before;
      builder.SetInsertPoint( elseBB );
      if( !statementList( stmt.elseBody ) )
         return false;

      elseTerminated = terminated();
      elseTypes = localTypes;
      if( !elseTerminated )
         builder.CreateBr( doneBB );

      builder.SetInsertPoint( doneBB );
      if( thenTerminated && elseTerminated )
      {
         builder.CreateUnreachable();
         return true;
      }

      localTypes = before;
      mergeTypes( before, thenTypes, elseTypes );

      return true;
   }

   /* The end of DO CASE and the types of the branches that reach it */
   struct BRANCH_JOIN
   {
      llvm::BasicBlock *done;
      LOCAL_TYPES types;
      bool reached = false;
   };

   /* A branch that is not closed by RETURN, EXIT or LOOP joins its types and goes to the end */
   void finishBranch( BRANCH_JOIN &join )
   {
      if( terminated() )
         return;

      for( auto &entry : join.types )
      {
         const auto found = localTypes.find( entry.first );
         const STATIC_TYPE type = IIF( found == localTypes.end(), STATIC_TYPE::UNKNOWN, found->second );

         if( !join.reached || entry.second == type )
            entry.second = type;
         else
            entry.second = STATIC_TYPE::UNKNOWN;
      }

      join.reached = true;
      builder.CreateBr( join.done );
   }

   /* One CASE: its condition must be LOGICAL; a false condition goes on to the next CASE */
   bool caseBranch( const X_CASE_BRANCH &branch, BRANCH_JOIN &join )
   {
      const STATIC_TYPE type = expressionType( *branch.condition );
      llvm::Value *value;
      llvm::BasicBlock *bodyBB;
      llvm::BasicBlock *nextBB;

      if( type != STATIC_TYPE::LOGICAL && type != STATIC_TYPE::UNKNOWN && type != STATIC_TYPE::PERSISTENT )
      {
         fail( branch.condition->line, "warunek CASE musi zwracac X_IT_LOGICAL (brak niejawnego rzutowania)" );
         return false;
      }

      value = scalarCondition( *branch.condition );
      if( value == nullptr )
         return false;

      bodyBB = llvm::BasicBlock::Create( context, "case.body", currentFunc );
      nextBB = llvm::BasicBlock::Create( context, "case.next", currentFunc );
      builder.CreateCondBr( value, bodyBB, nextBB );

      builder.SetInsertPoint( bodyBB );
      if( !statementList( branch.body ) )
         return false;

      finishBranch( join );
      builder.SetInsertPoint( nextBB );

      return true;
   }

   /* DO CASE: every branch starts with the types from before; after ENDCASE a local keeps the type all
      branches that reach the end agree on */
   bool caseStatement( const X_STMT &stmt )
   {
      const LOCAL_TYPES before = localTypes;
      BRANCH_JOIN join = { llvm::BasicBlock::Create( context, "case.done", currentFunc ), before };

      for( const X_CASE_BRANCH &branch : stmt.cases )
      {
         localTypes = before;
         if( !caseBranch( branch, join ) )
            return false;
      }

      localTypes = before;
      if( !statementList( stmt.otherwiseBody ) )
         return false;

      finishBranch( join );
      builder.SetInsertPoint( join.done );
      localTypes = join.types;
      if( !join.reached )
         builder.CreateUnreachable();

      return true;
   }

   /* The selector of an item SWITCH, kept by x_switchValue() */
   llvm::Value *switchSelector( const X_EXPR &expr )
   {
      llvm::Value *value = expression( expr );
      llvm::Value *selector;

      if( value == nullptr )
         return nullptr;

      selector = freshItem( "switch.value" );
      emitCall( runtime( "x_switchValue", i32Ty(), { ptrTy(), ptrTy() } ), { selector, value } );

      return selector;
   }

   /* A CASE label of a typed SWITCH: a native comparison */
   llvm::Value *typedCaseMatch( const X_STMT &stmt, const X_CASE_BRANCH &branch, const NATIVE_VALUE &saved )
   {
      NATIVE_VALUE converted = saved;
      NATIVE_VALUE label;
      llvm::Value *equal;

      if( branch.selectorConversion )
         converted = nativeConvert( saved, *branch.selectorConversion );

      label = nativeExpression( *branch.condition );
      if( !converted.value || !label.value )
         return nullptr;

      equal = native.compare( converted.value, label.value, branch.operationType, "==" );
      if( !equal )
         fail( stmt.line, "SWITCH z typem wymaga liczbowych etykiet w biezacym etapie backendu" );

      return equal;
   }

   /* A CASE label of an item SWITCH: x_switchMatch() */
   llvm::Value *itemCaseMatch( const X_CASE_BRANCH &branch, llvm::Value *selector )
   {
      llvm::Value *label = expression( *branch.condition );
      llvm::Value *match;

      if( !label )
         return nullptr;

      match = emitCall( runtime( "x_switchMatch", i32Ty(), { ptrTy(), ptrTy() } ), { selector, label } );

      return builder.CreateICmpNE( match, constant32( 0 ) );
   }

   /* A SWITCH body starts with the temporaries of the tests cleared and the types from before the bodies */
   bool switchBody( const std::vector<X_STMT> &body, size_t first, size_t last, const LOCAL_TYPES &types )
   {
      clearTemps( first, last );
      localTypes = types;

      return statementList( body );
   }

   /* SWITCH: the selector is evaluated once and the CASE labels are compared in order; a CASE body falls
      through to the next one and EXIT leaves the SWITCH */
   bool switchStatement( const X_STMT &stmt )
   {
      const size_t first = itemSlots.size();
      bool typed;
      NATIVE_VALUE saved;
      llvm::Value *selector = nullptr;
      llvm::BasicBlock *doneBB;
      llvm::BasicBlock *defaultBB;
      std::vector<llvm::BasicBlock *> bodies;
      size_t last;
      bool changed;
      LOCAL_TYPES switchTypes;
      bool ok = true;

      if( stmt.cases.empty() && !stmt.hasOtherwise )
         return true;

      typed = stmt.expr->nativeType && nativeType( stmt.expr->type, stmt.expr->typeName );
      typed = typed && stmt.expr->type != X_DECL_TYPE::STRING;
      if( typed )
         saved = nativeExpression( *stmt.expr );
      else
         selector = switchSelector( *stmt.expr );

      /* Only one of them holds the selector */
      if( !saved.value && selector == nullptr )
         return false;

      doneBB = llvm::BasicBlock::Create( context, "switch.done", currentFunc );
      defaultBB = doneBB;
      if( stmt.hasOtherwise )
         defaultBB = llvm::BasicBlock::Create( context, "switch.otherwise", currentFunc );

      for( size_t i = 0; i < stmt.cases.size(); ++i )
         bodies.push_back( llvm::BasicBlock::Create( context, "switch.case", currentFunc ) );

      for( size_t i = 0; i < stmt.cases.size(); ++i )
      {
         const X_CASE_BRANCH &branch = stmt.cases[i];
         llvm::Value *equal = IIF( typed, typedCaseMatch( stmt, branch, saved ), itemCaseMatch( branch, selector ) );
         llvm::BasicBlock *nextBB;

         if( equal == nullptr )
            return false;

         nextBB = llvm::BasicBlock::Create( context, "switch.next", currentFunc );
         builder.CreateCondBr( equal, bodies[i], nextBB );
         builder.SetInsertPoint( nextBB );
      }

      last = itemSlots.size();
      clearTemps( first, last );
      builder.CreateBr( defaultBB );

      do
      {
         changed = mergeLoopTypes( stmt.otherwiseBody );

         for( const X_CASE_BRANCH &branch : stmt.cases )
            changed |= mergeLoopTypes( branch.body );
      } while( changed );

      switchTypes = localTypes;
      loopTargets.push_back( { nullptr, doneBB } );

      for( size_t i = 0; i < stmt.cases.size() && ok; ++i )
      {
         builder.SetInsertPoint( bodies[i] );
         ok = switchBody( stmt.cases[i].body, first, last, switchTypes );
         if( ok )
            closeBlock( IIF( i + 1 < bodies.size(), bodies[i + 1], defaultBB ) );
      }

      if( ok && stmt.hasOtherwise )
      {
         builder.SetInsertPoint( defaultBB );
         ok = switchBody( stmt.otherwiseBody, first, last, switchTypes );
         if( ok )
            closeBlock( doneBB );
      }

      loopTargets.pop_back();
      builder.SetInsertPoint( doneBB );
      localTypes = switchTypes;

      return ok;
   }

   /* The landing pad of BEGIN SEQUENCE: X_BREAK goes to recover, any other exception to resume */
   llvm::LandingPadInst *breakPad( llvm::BasicBlock *recover, llvm::BasicBlock *resume )
   {
      llvm::Constant *info = exceptionInfo( "_ZTI7X_BREAK" );
      llvm::LandingPadInst *pad = builder.CreateLandingPad( exceptionType(), 1 );
      llvm::Value *type;
      llvm::Value *selector;

      pad->setCleanup( true );
      pad->addClause( info );
      type = builder.CreateCall( intrinsicDeclaration( llvm::Intrinsic::eh_typeid_for, { ptrTy() } ), { info } );
      selector = builder.CreateExtractValue( pad, 1 );
      builder.CreateCondBr( builder.CreateICmpEQ( selector, type ), recover, resume );

      return pad;
   }

   /* RECOVER USING name: a typed variable gets the native value, any other one the item */
   bool recoverInto( const std::string &name, llvm::Value *value )
   {
      const auto found = nativeLocals.find( s_key( name ) );
      NATIVE_VALUE converted;
      llvm::Value *slot;

      if( found != nativeLocals.end() )
      {
         converted = nativeUnbox( value, found->second.type, false );
         if( !converted.value )
            return false;

         nativeStore( found->second, converted.value );
         return true;
      }

      slot = referenceItem( name );
      if( slot == nullptr )
         return false;

      emitCall( itemCopyFn(), { slot, value } );

      return true;
   }

   /* After BEGIN SEQUENCE a local may hold anything: its type is checked at run time */
   void forgetTypes()
   {
      for( auto &entry : localTypes )
         entry.second = STATIC_TYPE::PERSISTENT;
   }

   /* BEGIN SEQUENCE: BREAK lands in RECOVER with its value, any other exception goes on */
   bool sequenceStatement( const X_STMT &stmt )
   {
      llvm::BasicBlock *outerEh = unwindBB;
      llvm::BasicBlock *caught = llvm::BasicBlock::Create( context, "sequence.catch", currentFunc );
      llvm::BasicBlock *recover = llvm::BasicBlock::Create( context, "sequence.recover", currentFunc );
      llvm::BasicBlock *resume = llvm::BasicBlock::Create( context, "sequence.resume", currentFunc );
      llvm::BasicBlock *done = llvm::BasicBlock::Create( context, "sequence.done", currentFunc );
      llvm::Value *value = freshItem( "sequence.value" );
      const size_t firstTemp = itemSlots.size();
      size_t lastTemp;
      llvm::LandingPadInst *pad;
      llvm::Value *exception;

      unwindBB = caught;
      if( !statementList( stmt.body ) )
         return false;

      closeBlock( done );
      lastTemp = itemSlots.size();
      unwindBB = outerEh;

      builder.SetInsertPoint( caught );
      pad = breakPad( recover, resume );

      builder.SetInsertPoint( resume );
      unwindToExit( pad );

      builder.SetInsertPoint( recover );
      clearTemps( firstTemp, lastTemp );
      exception = builder.CreateExtractValue( pad, 0 );
      builder.CreateCall( runtime( "x_recoverBreak", voidTy(), { ptrTy(), ptrTy() } ), { exception, value } );
      if( !stmt.name.empty() && !recoverInto( stmt.name, value ) )
         return false;

      emitCall( itemClearFn(), { value } );
      forgetTypes();
      if( !statementList( stmt.elseBody ) )
         return false;

      closeBlock( done );
      builder.SetInsertPoint( done );
      forgetTypes();

      return true;
   }

   /* ---------------------------------------------------------------------------------------------
      Statements
      --------------------------------------------------------------------------------------------- */

   /* BREAK [value] raises X_BREAK by x_breakRaise() */
   bool breakStatement( const X_STMT &stmt )
   {
      llvm::Value *value = IIF( stmt.expr, expression( *stmt.expr ), freshItem( "break.nil" ) );

      if( value == nullptr )
         return false;

      emitCall( runtime( "x_breakRaise", voidTy(), { ptrTy() } ), { value } );
      builder.CreateUnreachable();

      return true;
   }

   /* EXIT leaves the innermost loop or SWITCH */
   bool exitStatement( const X_STMT &stmt )
   {
      if( loopTargets.empty() )
      {
         fail( stmt.line, "EXIT poza petla" );
         return false;
      }

      builder.CreateBr( loopTargets.back().done );

      return true;
   }

   /* LOOP goes to the next pass of the innermost loop; a SWITCH is skipped */
   bool loopStatement( const X_STMT &stmt )
   {
      auto target = loopTargets.rbegin();

      while( target != loopTargets.rend() && target->condition == nullptr )
         ++target;

      if( target == loopTargets.rend() )
      {
         fail( stmt.line, "LOOP poza petla" );
         return false;
      }

      builder.CreateBr( target->condition );

      return true;
   }

   /* Expressions that may stand as a statement: calls, sends, ++, --, IIF(), alias and group expressions
      and unary + */
   static bool itemStatement( const X_EXPR &expr )
   {
      switch( expr.kind )
      {
      case X_EXPR_KIND::CALL:
      case X_EXPR_KIND::SEND:
      case X_EXPR_KIND::UPDATE:
      case X_EXPR_KIND::IIF:
      case X_EXPR_KIND::ALIAS:
      case X_EXPR_KIND::GROUP: return true;
      case X_EXPR_KIND::UNARY: return expr.text == "+";
      default:                 return false;
      }
   }

   /* An expression as a statement: native, an assignment, a scalar update or an item */
   bool expressionStatement( const X_EXPR &inner, int line )
   {
      std::string kind;

      if( inner.nativeType && nativeType( inner.type, inner.typeName ) )
         return nativeExpression( inner ).value != nullptr;

      if( inner.kind == X_EXPR_KIND::ASSIGN )
         return simpleAssign( inner );

      if( inner.kind == X_EXPR_KIND::UPDATE && scalarType( inner ) != STATIC_TYPE::UNKNOWN )
         return scalarExpr( inner ) != nullptr;

      if( itemStatement( inner ) )
         return expression( inner ) != nullptr;

      kind = std::to_string( ( int )inner.kind );
      fail( line, "instrukcja (wyrazenie rodzaju " + kind + " jako instrukcja) nieobslugiwana jeszcze w backendzie" );

      return false;
   }

   /* The temporaries of an expression statement are cleared after it */
   bool callStatement( const X_STMT &stmt )
   {
      const size_t first = itemSlots.size();

      if( !expressionStatement( *stmt.expr, stmt.line ) )
         return false;

      clearTemps( first, itemSlots.size() );

      return true;
   }

   /* One statement. FIELD needs no code, the storage of a STATIC is prepared before INIT and REQUEST only
      names symbols for the linker */
   bool statement( const X_STMT &stmt )
   {
      llvm::SaveAndRestore<int> lineScope( srcLine, stmt.line );
      std::string kind;

      switch( stmt.kind )
      {
      case X_STMT_KIND::SEQUENCE: return sequenceStatement( stmt );
      case X_STMT_KIND::BREAK:    return breakStatement( stmt );
      case X_STMT_KIND::LOCAL:    return localStatement( stmt );
      case X_STMT_KIND::FIELD:
      case X_STMT_KIND::STATIC:
      case X_STMT_KIND::REQUEST:  return true;
      case X_STMT_KIND::RETURN:   return returnStatement( stmt );
      case X_STMT_KIND::IF:       return ifStatement( stmt );
      case X_STMT_KIND::WHILE:    return whileStatement( stmt );
      case X_STMT_KIND::FOR:      return forStatement( stmt );
      case X_STMT_KIND::DO_CASE:  return caseStatement( stmt );
      case X_STMT_KIND::SWITCH:   return switchStatement( stmt );
      case X_STMT_KIND::EXIT:     return exitStatement( stmt );
      case X_STMT_KIND::LOOP:     return loopStatement( stmt );
      case X_STMT_KIND::CALL:     return callStatement( stmt );
      default:                    break;
      }

      kind = std::to_string( ( int )stmt.kind );
      fail( stmt.line, "instrukcja (rodzaj AST nr " + kind + ") nieobslugiwana jeszcze w backendzie" );

      return false;
   }

   /* ---------------------------------------------------------------------------------------------
      Class registration
      --------------------------------------------------------------------------------------------- */

   /* The value type that x_clsSetDataType() checks for a typed DATA */
   static X_OO_VALUE_TYPE dataValueType( X_DECL_TYPE type )
   {
      switch( type )
      {
      case X_DECL_TYPE::INT8:      return X_OO_VALUE_INT8;
      case X_DECL_TYPE::INT16:     return X_OO_VALUE_INT16;
      case X_DECL_TYPE::INT32:     return X_OO_VALUE_INT32;
      case X_DECL_TYPE::INT64:     return X_OO_VALUE_INT64;
      case X_DECL_TYPE::UINT8:     return X_OO_VALUE_UINT8;
      case X_DECL_TYPE::UINT16:    return X_OO_VALUE_UINT16;
      case X_DECL_TYPE::UINT32:    return X_OO_VALUE_UINT32;
      case X_DECL_TYPE::UINT64:    return X_OO_VALUE_UINT64;
      case X_DECL_TYPE::FLOAT32:   return X_OO_VALUE_FLOAT32;
      case X_DECL_TYPE::FLOAT64:   return X_OO_VALUE_FLOAT64;
      case X_DECL_TYPE::LOGIC:     return X_OO_VALUE_LOGIC;
      case X_DECL_TYPE::STRING:    return X_OO_VALUE_STRING;
      case X_DECL_TYPE::PTR:       return X_OO_VALUE_PTR;
      case X_DECL_TYPE::ARRAY:     return X_OO_VALUE_ARRAY;
      case X_DECL_TYPE::CODEBLOCK: return X_OO_VALUE_CODEBLOCK;
      case X_DECL_TYPE::OBJECT:    return X_OO_VALUE_OBJECT;
      case X_DECL_TYPE::DATE:      return X_OO_VALUE_DATE;
      case X_DECL_TYPE::HASH:      return X_OO_VALUE_HASH;
      default:                     return X_OO_VALUE_ANY;
      }
   }

   /* The temporaries and the direct calls of the previous function are gone */
   void clearFunctionState()
   {
      scalars.clear();
      sends.clear();
      nativeSends.clear();
      itemSlots.clear();
      stringSlots.clear();
      boxSlots.clear();
      tempIndex = 0;
   }

   /* A call of the class API, which gives 0 on failure */
   llvm::Value *classCall( const char *name, const std::vector<llvm::Type *> &types, const VALUES &args )
   {
      return emitCall( runtime( name, i32Ty(), types ), args );
   }

   /* A result of 0 goes to failed */
   void classCheck( llvm::Value *value, llvm::BasicBlock *failed )
   {
      llvm::BasicBlock *next = llvm::BasicBlock::Create( context, "registered", currentFunc );

      builder.CreateCondBr( builder.CreateICmpNE( value, constant32( 0 ) ), next, failed );
      builder.SetInsertPoint( next );
   }

   /* A parent from another module: x_clsRequire() with its class function */
   llvm::Value *requireClass( const std::string &parent )
   {
      llvm::FunctionCallee factory = module->getOrInsertFunction( parent, functionAbiType() );
      llvm::Value *text = builder.CreateGlobalString( parent );

      return classCall( "x_clsRequire", { ptrTy(), ptrTy() }, { text, factory.getCallee() } );
   }

   /* A parent class, registered by its registrar in this module or required from another one */
   llvm::Value *parentClass( const std::string &parent, llvm::BasicBlock *failed )
   {
      const auto local = classRegistrars.find( parent );
      llvm::Value *handle;

      if( local != classRegistrars.end() )
         handle = emitCall( local->second );
      else
         handle = requireClass( parent );

      classCheck( handle, failed );
      classCheck( classCall( "x_clsIsAssociated", { i32Ty() }, { handle } ), failed );

      return handle;
   }

   static size_t dataMembers( const X_CLASS_DECL &cls )
   {
      size_t count = 0;

      for( const X_CLASS_MEMBER &member : cls.members )
         count += member.kind == "DATA";

      return count;
   }

   llvm::Value *addDelegate( const X_CLASS_MEMBER &member, llvm::Value *handle, llvm::Value *memberName )
   {
      const std::vector<llvm::Type *> types = { i32Ty(), ptrTy(), ptrTy(), ptrTy() };
      llvm::Value *object = builder.CreateGlobalString( member.delegateObject );
      llvm::Value *message = builder.CreateGlobalString( member.delegateMessage );

      return classCall( "x_clsAddDelegate", types, { handle, memberName, object, message } );
   }

   /* An item that is still NIL becomes an empty DATE */
   void nilToDate( llvm::Value *item )
   {
      llvm::Value *tag = builder.CreateStructGEP( getItemType(), item, 0 );
      llvm::Value *nil = builder.CreateICmpEQ( builder.CreateLoad( i32Ty(), tag ), constant32( X_IT_NIL ) );
      llvm::Value *type = builder.CreateSelect( nil, constant32( X_IT_DATE ), builder.CreateLoad( i32Ty(), tag ) );

      builder.CreateStore( type, tag );
   }

   /* CLASSDATA gets the value of its initializer function; a DATE starts as an empty date */
   llvm::Value *addClassData( const X_CLASS_DECL &cls, const X_CLASS_MEMBER &member, llvm::Value *handle,
                              llvm::Value *memberName, uint64_t index )
   {
      const std::vector<llvm::Type *> types = { i32Ty(), ptrTy(), i64Ty(), ptrTy() };
      llvm::Value *value = freshItem( "class.initialValue" );
      llvm::Function *initializer = functions.at( "__XDEV_" + cls.name + "_CLASSDATA_" + member.name );
      llvm::Value *added;

      emitCall( initializer, { callFrame( value, constant32( 0 ), nullPtr() ) } );
      if( member.type == X_DECL_TYPE::DATE )
         nilToDate( value );

      added = classCall( "x_clsAddClassData", types, { handle, memberName, constant64( index ), value } );
      emitCall( itemClearFn(), { value } );

      return added;
   }

   static const char *methodApi( const std::string &kind )
   {
      if( kind == "ACCESS" )
         return "x_clsAddAccess";

      if( kind == "ASSIGN" )
         return "x_clsAddAssign";

      if( kind == "CLASSMETHOD" )
         return "x_clsAddClassMethod";

      return "x_clsAdd";
   }

   /* A method, an OPERATOR, ON ERROR, a DESTRUCTOR, an ACCESS, an ASSIGN or a CLASSMETHOD */
   llvm::Value *addMethod( const X_CLASS_DECL &cls, const X_CLASS_MEMBER &member, llvm::Value *handle,
                           llvm::Value *memberName )
   {
      const std::string symbol = "__XDEV_" + cls.name + "_" + member.kind + "_" + member.name;
      llvm::Function *method = functions.at( symbol );
      const char *api;

      if( member.operatorId >= 0 )
      {
         llvm::Value *id = constant32( member.operatorId );

         return classCall( "x_clsAddOperator", { i32Ty(), i32Ty(), ptrTy() }, { handle, id, method } );
      }

      if( member.kind == "DESTRUCTOR" || member.kind == "ONERROR" )
      {
         api = IIF( member.kind == "ONERROR", "x_clsAddOnError", "x_clsAddDestructor" );
         return classCall( api, { i32Ty(), ptrTy() }, { handle, method } );
      }

      return classCall( methodApi( member.kind ), { i32Ty(), ptrTy(), ptrTy() }, { handle, memberName, method } );
   }

   /* The members in declaration order; each one gets its scope, a typed DATA its value type */
   void addMembers( const X_CLASS_DECL &cls, llvm::Value *handle, llvm::BasicBlock *failed )
   {
      const std::vector<llvm::Type *> dataTypes = { i32Ty(), ptrTy(), i64Ty() };
      const std::vector<llvm::Type *> scopeTypes = { i32Ty(), ptrTy(), i32Ty() };
      uint64_t index = 0;
      uint64_t classDataIndex = 0;

      for( const X_CLASS_MEMBER &member : cls.members )
      {
         const bool data = member.kind == "DATA" || member.kind == "CLASSDATA";
         const bool method = !data && member.kind != "VIRTUAL" && member.kind != "DELEGATE";
         const std::string scopeName = IIF( method, memberMessage( member ), member.name );
         const X_OO_VALUE_TYPE valueType = dataValueType( member.type );
         llvm::Value *memberName = builder.CreateGlobalString( member.name );
         llvm::Value *added;
         llvm::Value *scope;

         if( member.kind == "DATA" )
            added = classCall( "x_clsAddData", dataTypes, { handle, memberName, constant64( ++index ) } );
         else if( member.kind == "VIRTUAL" )
            added = classCall( "x_clsAddVirtual", { i32Ty(), ptrTy() }, { handle, memberName } );
         else if( member.kind == "DELEGATE" )
            added = addDelegate( member, handle, memberName );
         else if( member.kind == "CLASSDATA" )
            added = addClassData( cls, member, handle, memberName, ++classDataIndex );
         else
            added = addMethod( cls, member, handle, memberName );

         classCheck( added, failed );
         scope = builder.CreateGlobalString( scopeName );
         classCheck( classCall( "x_clsSetScope", scopeTypes, { handle, scope, constant32( member.scope ) } ), failed );
         if( data && valueType != X_OO_VALUE_ANY )
         {
            added = classCall( "x_clsSetDataType", scopeTypes, { handle, memberName, constant32( valueType ) } );
            classCheck( added, failed );
         }
      }
   }

   /* The registrar of a class: a class of the same name that is associated already is reused, otherwise
      the class is created with its parents and members. A failure discards the pending class */
   bool generateClassRegistration( const X_CLASS_DECL &cls )
   {
      const std::vector<llvm::Type *> scopeTypes = { i32Ty(), ptrTy(), i32Ty() };
      llvm::Function *function = classRegistrars.at( cls.name );
      llvm::BasicBlock *entry;
      llvm::BasicBlock *create;
      llvm::BasicBlock *existing;
      llvm::BasicBlock *failed;
      llvm::AllocaInst *pendingClass;
      llvm::Value *name;
      llvm::Value *found;
      VALUES parents;
      llvm::Value *handle;
      llvm::Value *initData;
      llvm::Value *text;

      currentFunc = function;
      currentResult = nullptr;
      clearFunctionState();

      entry = llvm::BasicBlock::Create( context, "entry", function );
      create = llvm::BasicBlock::Create( context, "create", function );
      existing = llvm::BasicBlock::Create( context, "existing", function );
      failed = llvm::BasicBlock::Create( context, "failed", function );
      builder.SetInsertPoint( entry );
      beginUnwind();
      pendingClass = builder.CreateAlloca( i32Ty(), nullptr, "class.pending" );
      builder.CreateStore( constant32( 0 ), pendingClass );

      name = builder.CreateGlobalString( cls.name );
      found = classCall( "x_clsFindClass", { ptrTy(), ptrTy() }, { name, nullPtr() } );
      builder.CreateCondBr( builder.CreateICmpNE( found, constant32( 0 ) ), existing, create );

      builder.SetInsertPoint( existing );
      classCheck( classCall( "x_clsIsAssociated", { i32Ty() }, { found } ), failed );
      builder.CreateRet( found );

      builder.SetInsertPoint( create );

      for( const std::string &parent : cls.parents )
         parents.push_back( parentClass( parent, failed ) );

      handle = classCall( "x_clsCreate", { i64Ty(), ptrTy() }, { constant64( dataMembers( cls ) ), name } );
      builder.CreateStore( handle, pendingClass );
      classCheck( handle, failed );

      for( llvm::Value *parent : parents )
         classCheck( classCall( "x_clsAddParent", { i32Ty(), i32Ty() }, { handle, parent } ), failed );

      addMembers( cls, handle, failed );

      initData = functions.at( "__XDEV_" + cls.name + "_INITDATA" );
      text = builder.CreateGlobalString( "__XDEV_INITDATA" );
      classCheck( classCall( "x_clsAdd", { i32Ty(), ptrTy(), ptrTy() }, { handle, text, initData } ), failed );
      text = builder.CreateGlobalString( "__XDEV_INITDATA" );
      classCheck( classCall( "x_clsSetScope", scopeTypes, { handle, text, constant32( X_OO_CLSTP_HIDDEN ) } ), failed );
      classCheck( classCall( "x_clsAssociate", { i32Ty() }, { handle } ), failed );
      builder.CreateRet( handle );

      builder.SetInsertPoint( failed );
      emitCleanup();
      classCall( "x_clsDiscard", { i32Ty() }, { builder.CreateLoad( i32Ty(), pendingClass ) } );
      builder.CreateRet( constant32( 0 ) );
      finishUnwind( pendingClass );

      return true;
   }

   /* ---------------------------------------------------------------------------------------------
      Function bodies
      --------------------------------------------------------------------------------------------- */

   /* The name of a function in the trace: NAME, CLASS:METHOD or CLASS:INIT for the DATA initializer */
   static std::string functionTitle( const X_FUNCTION &fn, const std::string &key )
   {
      if( fn.className.empty() )
         return key;

      return fn.className + ":" + IIF( fn.isDataInitializer, "INIT", fn.methodName );
   }

   /* A zeroed alloca at the head of the entry block */
   llvm::AllocaInst *zeroedLocal( llvm::Type *type, const std::string &name )
   {
      llvm::BasicBlock &entry = currentFunc->getEntryBlock();
      llvm::IRBuilder<> entryBuilder( &entry, entry.getFirstInsertionPt() );
      llvm::AllocaInst *slot = entryBuilder.CreateAlloca( type, nullptr, name );

      entryBuilder.CreateStore( llvm::Constant::getNullValue( type ), slot );

      return slot;
   }

   /* A new function starts with its statics, its typed statics and no temporaries */
   void startFunction( const X_FUNCTION &fn, const BLOCK_JOB *block, const std::string &key )
   {
      locals.clear();

      for( const auto &entry : fileStatics )
         locals[entry.first] = entry.second;

      for( const auto &entry : functionStatics[key] )
         locals[entry.first] = entry.second;

      nativeLocals.clear();

      if( block != nullptr )
      {
         locals = block->statics;
         nativeLocals = block->nativeStatics;
      }
      else
      {
         for( const auto &entry : nativeStatics[""] )
            nativeLocals[entry.first] = entry.second;

         for( const auto &entry : nativeStatics[key] )
            nativeLocals[entry.first] = entry.second;
      }

      for( const std::string &param : fn.params )
         nativeLocals.erase( s_key( param ) );

      clearFunctionState();
      if( block == nullptr )
         collectSends( fn );

      localTypes.clear();
      loopTargets.clear();
   }

   /* The source frame of the trace: x_traceEnter( frame, name ) */
   void enterTrace()
   {
      llvm::Value *name;

      trace = builder.CreateAlloca( traceType(), nullptr, "source.frame" );
      name = builder.CreateGlobalString( currentFunctionName );
      builder.CreateCall( runtime( "x_traceEnter", voidTy(), { ptrTy(), ptrTy() } ), { trace, name } );
   }

   void leaveTrace()
   {
      builder.CreateCall( runtime( "x_traceLeave", voidTy(), { ptrTy() } ), { trace } );
   }

   /* The entry loads the call frame and enters the source frame of the trace */
   void enterFunction( llvm::Function *f )
   {
      llvm::BasicBlock *entry = llvm::BasicBlock::Create( context, "entry", f );

      cleanupBB = llvm::BasicBlock::Create( context, "cleanup", f );
      builder.SetInsertPoint( entry );

      loadCall( f->getArg( 0 ) );
      enterTrace();
      beginUnwind();

      if( nativeType( nativeResultType ) )
      {
         nativeResult = builder.CreateAlloca( nativeType( nativeResultType ), nullptr, "native.result" );
         builder.CreateStore( llvm::Constant::getNullValue( nativeType( nativeResultType ) ), nativeResult );
      }
   }

   /* A local passed by @ escapes, except a STRUCT passed by @ to a DLL function */
   bool escapingArgument( const X_EXPR &expr, size_t i ) const
   {
      const bool byReference = i < expr.argByReference.size() && expr.argByReference[i];
      const bool dllCall = expr.kind == X_EXPR_KIND::CALL && nativeDllNames.count( s_key( expr.text ) );

      if( !byReference )
         return false;

      return !( dllCall && expr.args[i]->type == X_DECL_TYPE::STRUCT );
   }

   /* The locals captured by a code block or passed by @ escape the function */
   void escapingExpr( NAME_SET &escaping, const X_EXPR *expr )
   {
      if( !expr )
         return;

      if( expr->kind == X_EXPR_KIND::BLOCK )
         collectFreeVars( *expr, {}, escaping );

      for( size_t i = 0; i < expr->args.size(); ++i )
      {
         if( escapingArgument( *expr, i ) )
            escaping.insert( s_key( expr->args[i]->text ) );

         escapingExpr( escaping, expr->args[i].get() );
      }
   }

   void escapingStatements( NAME_SET &escaping, const std::vector<X_STMT> &body )
   {
      for( const X_STMT &stmt : body )
      {
         escapingExpr( escaping, stmt.expr.get() );
         escapingExpr( escaping, stmt.target.get() );
         escapingExpr( escaping, stmt.forLimit.get() );
         escapingExpr( escaping, stmt.forStep.get() );

         for( const X_LOCAL_DECL &decl : stmt.locals )
            escapingExpr( escaping, decl.init.get() );

         escapingStatements( escaping, stmt.body );
         escapingStatements( escaping, stmt.elseBody );
         escapingStatements( escaping, stmt.otherwiseBody );

         for( const X_CASE_BRANCH &branch : stmt.cases )
         {
            escapingExpr( escaping, branch.condition.get() );
            escapingStatements( escaping, branch.body );
         }
      }
   }

   /* A typed local lives in the entry block, or in a box when it escapes; an untyped one is no native local */
   bool typedLocal( const std::string &key, const X_BINDING &symbol, const NAME_SET &escaping )
   {
      const X_DECL_TYPE declared = symbol.type;
      llvm::Type *type = nativeType( declared, symbol.typeName );
      llvm::Value *box;
      llvm::AllocaInst *slot;

      if( type == nullptr )
      {
         nativeLocals.erase( key );
         return true;
      }

      if( escaping.count( key ) && declared == X_DECL_TYPE::STRUCT )
      {
         const char *message = "blad wewnetrzny: niedozwolone przechwycenie STRUCT po analizie semantycznej";

         fail( symbol.declaration->line, message );
         return false;
      }

      if( escaping.count( key ) )
      {
         box = freshNativeBox( declared, "native.local." + key );
         nativeLocals[key] = { nullptr, declared, box, false, symbol.typeName };
         return true;
      }

      slot = zeroedLocal( type, "typed.local." + key );
      if( declared == X_DECL_TYPE::STRING )
         stringSlots.push_back( slot );

      nativeLocals[key] = { slot, declared, nullptr, false, symbol.typeName };

      return true;
   }

   bool typedLocals( const X_FUNCTION &fn, const NAME_SET &escaping )
   {
      for( const auto &entry : fn.scope.symbols )
      {
         const X_BINDING &symbol = entry.second;

         if( symbol.kind != X_BINDING_KIND::LOCAL || !symbol.declaration )
            continue;

         if( !typedLocal( entry.first, symbol, escaping ) )
            return false;
      }

      return true;
   }

   /* The other locals of a function get a box; a scalar gets its slot at its LOCAL */
   void itemLocals( const X_FUNCTION &fn )
   {
      for( const auto &entry : fn.scope.symbols )
      {
         const X_BINDING &symbol = entry.second;

         if( symbol.kind != X_BINDING_KIND::LOCAL || nativeLocals.count( entry.first ) || scalars.count( entry.first ) )
            continue;

         locals[entry.first] = freshBox( ( "local." + symbol.name ).c_str() );
         localTypes[entry.first] = STATIC_TYPE::UNKNOWN;
      }
   }

   /* A code block gets the boxes of its captured variables from its second argument */
   void captureBoxes( const BLOCK_JOB &block )
   {
      for( size_t i = 0; i < block.captures.size(); ++i )
      {
         llvm::Value *address = builder.CreateInBoundsGEP( ptrTy(), currentFunc->getArg( 1 ), constant64( i ) );
         llvm::Value *box = builder.CreateLoad( ptrTy(), address, "block.capture" );
         const auto typed = block.nativeCaptures.find( block.captures[i] );

         if( typed != block.nativeCaptures.end() )
            nativeLocals[typed->first] = { nullptr, typed->second, box };
         else
            locals[block.captures[i]] = box;
      }
   }

   /* A method keeps Self in a box; ON ERROR gets the message and the arguments as its two parameters */
   void methodLocals( const X_FUNCTION &fn )
   {
      if( !fn.className.empty() && !currentClassMethod )
      {
         llvm::Value *self = builder.CreateLoad( ptrTy(), builder.CreateStructGEP( getCtxType(), callCtx, 3 ) );
         llvm::Value *slot = freshBox( "method.self" );

         emitCall( itemCopyFn(), { slot, self } );
         locals["SELF"] = slot;
      }

      if( fn.methodKind == "ONERROR" )
      {
         llvm::Value *message = freshBox( "onerror.message" );
         llvm::Value *arguments = freshBox( "onerror.arguments" );
         llvm::FunctionCallee info = runtime( "x_objOnErrorInfo", i32Ty(), std::vector<llvm::Type *>( 4, ptrTy() ) );

         emitCall( info, { callCtx, currentResult, message, arguments } );
         locals[s_key( fn.params[0] )] = message;
         locals[s_key( fn.params[1] )] = arguments;
      }
   }

   /* A typed parameter of a typed function arrives as a native value and a reference. The reference is NULL
      for a value; an escaping parameter then keeps the value in a box of its own */
   void typedParameter( const X_FUNCTION &fn, const NATIVE_FUNCTION &function, size_t i, const NAME_SET &escaping )
   {
      const X_DECL_TYPE declared = function.parameters[i];
      const std::string key = s_key( fn.params[i] );
      llvm::AllocaInst *slot = zeroedLocal( nativeType( declared ), "native.param." + fn.params[i] );
      size_t position = 1;
      llvm::Value *reference;
      llvm::Value *own;
      llvm::Value *selected;

      for( size_t j = 0; j < i; ++j )
      {
         if( function.parameters[j] != X_DECL_TYPE::ANY )
            position += 2;
      }

      reference = currentFunc->getArg( position + 1 );
      if( escaping.count( key ) )
      {
         own = freshNativeBox( declared, "native.param.box." + fn.params[i] );
         nativeStore( { nullptr, declared, own }, currentFunc->getArg( position ) );
         selected = builder.CreateSelect( builder.CreateIsNull( reference ), own, reference );
         nativeLocals[key] = { nullptr, declared, selected };
         return;
      }

      if( declared == X_DECL_TYPE::STRING )
         stringSlots.push_back( slot );

      nativeStore( { slot, declared }, currentFunc->getArg( position ) );
      nativeLocals[key] = { slot, declared, reference, true };
   }

   /* Parameter i arrives as an X_ARG: a missing one stays NIL, by reference its box is bound, else it is copied */
   void bindParameter( llvm::Value *slot, size_t i )
   {
      llvm::Function *f = currentFunc;
      llvm::Value *idx = constant32( i );
      llvm::Value *present = builder.CreateICmpULT( idx, currentArgc );
      llvm::BasicBlock *haveBB = llvm::BasicBlock::Create( context, "param.have", f );
      llvm::BasicBlock *copyBB = llvm::BasicBlock::Create( context, "param.copy", f );
      llvm::BasicBlock *referenceBB = llvm::BasicBlock::Create( context, "param.reference", f );
      llvm::BasicBlock *doneBB = llvm::BasicBlock::Create( context, "param.done", f );
      llvm::Value *elem;
      llvm::Value *valuePtr;
      llvm::Value *byReference;

      builder.CreateCondBr( present, haveBB, doneBB );

      builder.SetInsertPoint( haveBB );
      elem = builder.CreateInBoundsGEP( getArgType(), currentArgs, idx );
      valuePtr = builder.CreateLoad( ptrTy(), builder.CreateStructGEP( getArgType(), elem, 0 ) );
      byReference = builder.CreateLoad( i32Ty(), builder.CreateStructGEP( getArgType(), elem, 1 ) );
      builder.CreateCondBr( builder.CreateICmpNE( byReference, constant32( 0 ) ), referenceBB, copyBB );

      builder.SetInsertPoint( copyBB );
      emitCall( itemCopyFn(), { slot, valuePtr } );
      builder.CreateBr( doneBB );

      builder.SetInsertPoint( referenceBB );
      emitCall( runtime( "x_boxBind", voidTy(), { ptrTy(), ptrTy(), i32Ty() } ), { slot, valuePtr, constant32( 1 ) } );
      builder.CreateBr( doneBB );

      builder.SetInsertPoint( doneBB );
   }

   /* A typed parameter of an untyped function: its item is unboxed into a native local or a box of its own */
   bool unboxParameter( const X_FUNCTION &fn, size_t i, X_DECL_TYPE type, const NAME_SET &escaping )
   {
      const std::string paramKey = s_key( fn.params[i] );
      NATIVE_VALUE value = nativeUnbox( variableItem( fn.params[i], fn.line ), type, false );
      llvm::Value *own;
      llvm::AllocaInst *slot;

      if( !value.value )
         return false;

      locals.erase( paramKey );
      if( escaping.count( paramKey ) )
      {
         own = freshNativeBox( type, "native.param.box." + fn.params[i] );
         nativeStore( { nullptr, type, own }, value.value );
         nativeLocals[paramKey] = { nullptr, type, own };
         return true;
      }

      slot = zeroedLocal( nativeType( type ), "native.param." + fn.params[i] );
      if( type == X_DECL_TYPE::STRING )
         stringSlots.push_back( slot );

      nativeStore( { slot, type }, value.value );
      nativeLocals[paramKey] = { slot, type };

      return true;
   }

   /* An item parameter; a typed parameter of an untyped function is then unboxed */
   bool itemParameter( const X_FUNCTION &fn, size_t i, bool typedFunction, const NAME_SET &escaping )
   {
      llvm::Value *slot = freshBox( ( "param." + fn.params[i] ).c_str() );
      const std::string paramKey = s_key( fn.params[i] );
      const auto declared = fn.paramTypes.find( paramKey );

      bindParameter( slot, i );
      locals[paramKey] = slot;

      if( typedFunction || declared == fn.paramTypes.end() || nativeType( declared->second ) == nullptr )
         return true;

      return unboxParameter( fn, i, declared->second, escaping );
   }

   /* The parameters; ON ERROR has its own two */
   bool functionParameters( const X_FUNCTION &fn, const NATIVE_FUNCTION *typed, const NAME_SET &escaping )
   {
      for( size_t i = 0; fn.methodKind != "ONERROR" && i < fn.params.size(); ++i )
      {
         if( typed != nullptr && typed->parameters[i] != X_DECL_TYPE::ANY )
            typedParameter( fn, *typed, i, escaping );
         else if( !itemParameter( fn, i, typed != nullptr, escaping ) )
            return false;
      }

      return true;
   }

   /* A factory function registers its class, then x_clsFactoryCreate() makes the object */
   void factoryObject( const X_FUNCTION &fn )
   {
      llvm::Value *handle = emitCall( classRegistrars.at( fn.factoryClass ) );
      llvm::BasicBlock *ready = llvm::BasicBlock::Create( context, "class.ready", currentFunc );
      llvm::BasicBlock *failed = llvm::BasicBlock::Create( context, "class.failed", currentFunc );
      llvm::FunctionCallee raise = runtime( "x_errorRaiseNamed", i32Ty(), { ptrTy(), i32Ty(), ptrTy() } );
      llvm::FunctionCallee create = runtime( "x_clsFactoryCreate", i32Ty(), { ptrTy(), ptrTy(), i32Ty() } );
      llvm::Value *text;

      builder.CreateCondBr( builder.CreateICmpNE( handle, constant32( 0 ) ), ready, failed );

      builder.SetInsertPoint( failed );
      text = builder.CreateGlobalString( "class registration: " + fn.factoryClass );
      emitCall( raise, { currentResult, constant32( 3000 ), text } );
      builder.CreateUnreachable();

      builder.SetInsertPoint( ready );
      emitCall( create, { callCtx, currentResult, handle } );
   }

   /* INIT of the DATA of a class: a DATA with INIT or of type DATE gets its value in its slot */
   bool initializeData( const X_FUNCTION &fn )
   {
      for( size_t i = 0; i < fn.dataInitializers.size(); ++i )
      {
         const X_LOCAL_DECL &decl = fn.dataInitializers[i];
         const size_t first = itemSlots.size();
         llvm::Value *value;
         llvm::Value *slot;

         if( !decl.init && decl.declaredType != X_DECL_TYPE::DATE )
            continue;

         value = IIF( decl.init, expression( *decl.init ), emptyDateItem() );
         if( value == nullptr )
            return false;

         slot = emitCall( runtime( "x_objDataSlot", ptrTy(), { ptrTy(), i64Ty() } ), { callCtx, constant64( i + 1 ) } );
         emitCall( itemCopyFn(), { slot, value } );
         clearTemps( first, itemSlots.size() );
      }

      return true;
   }

   /* A code block gives the value of its last expression */
   bool blockValue( const BLOCK_JOB &block )
   {
      const std::vector<X_EXPR_PTR> &args = block.expression->args;

      for( size_t i = 0; i < args.size(); ++i )
      {
         const size_t first = itemSlots.size();
         llvm::Value *value = expression( *args[i] );

         if( value == nullptr )
            return false;

         if( i + 1 == args.size() )
            emitCall( itemCopyFn(), { currentResult, value } );

         clearTemps( first, itemSlots.size() );
      }

      return true;
   }

   /* A typed function that ends without RETURN raises error 3107; the cleanup leaves the source frame */
   void finishFunction()
   {
      if( !terminated() )
      {
         if( nativeResult )
            nativeGuard( builder.getFalse(), 3107 );

         builder.CreateBr( cleanupBB );
      }

      builder.SetInsertPoint( cleanupBB );
      emitCleanup();
      leaveTrace();
      if( nativeResult )
         builder.CreateRet( builder.CreateLoad( nativeType( nativeResultType ), nativeResult ) );
      else
         builder.CreateRetVoid();

      finishUnwind();
   }

   /* The body of a function, a method or a code block */
   bool generateFunctionBody( const X_FUNCTION &fn, const BLOCK_JOB *block = nullptr )
   {
      llvm::SaveAndRestore<llvm::Value *> traceScope( trace, nullptr );
      llvm::SaveAndRestore<int> lineScope( srcLine, 0 );
      const std::string key = s_key( fn.name );
      const auto nativeFunction = nativeFunctions.find( key );
      const NATIVE_FUNCTION *typed = nullptr;
      NAME_SET escaping;

      currentClassMethod = IIF( block != nullptr, block->classMethod, fn.methodKind == "CLASSMETHOD" );
      if( block == nullptr && nativeFunction != nativeFunctions.end() )
         typed = &nativeFunction->second;

      if( block != nullptr )
         currentFunc = block->function;
      else if( typed != nullptr )
         currentFunc = typed->function;
      else
         currentFunc = functions[key];

      nativeResult = nullptr;
      nativeResultType = IIF( typed != nullptr, typed->result, X_DECL_TYPE::ANY );
      currentFunctionName = functionTitle( fn, key );
      startFunction( fn, block, key );
      enterFunction( currentFunc );

      escapingStatements( escaping, fn.body );
      if( block != nullptr )
      {
         for( const X_EXPR_PTR &arg : block->expression->args )
            escapingExpr( escaping, arg.get() );
      }

      if( !typedLocals( fn, escaping ) )
         return false;

      if( block == nullptr )
      {
         collectScalars( fn );
         itemLocals( fn );
      }
      else
         captureBoxes( *block );

      methodLocals( fn );
      if( !functionParameters( fn, typed, escaping ) )
         return false;

      if( !fn.factoryClass.empty() )
         factoryObject( fn );

      if( fn.isDataInitializer && !initializeData( fn ) )
         return false;

      if( block != nullptr && !blockValue( *block ) )
         return false;

      if( !statementList( fn.body ) )
         return false;

      finishFunction();

      return true;
   }

   /* ---------------------------------------------------------------------------------------------
      Wrappers of typed functions
      --------------------------------------------------------------------------------------------- */

   /* Argument i of a wrapper: the native value and its box as the reference, NULL when it is passed by
      value. A STRUCT cannot come from an item */
   void wrapperArgument( const NATIVE_FUNCTION &function, size_t i, bool dll, VALUES &arguments )
   {
      const X_DECL_TYPE type = function.parameters[i];
      llvm::Value *index;
      llvm::Value *element;
      llvm::Value *reference;
      llvm::Value *item;
      llvm::Value *box;
      NATIVE_VALUE value;
      llvm::Value *byValue;

      if( type == X_DECL_TYPE::ANY )
         return;

      if( type == X_DECL_TYPE::STRUCT )
      {
         nativeGuard( builder.getFalse(), 3107 );
         arguments.push_back( nullPtr() );
         arguments.push_back( nullPtr() );
         return;
      }

      index = constant32( i );
      nativeGuard( builder.CreateICmpULT( index, currentArgc ), 3107 );
      element = builder.CreateInBoundsGEP( getArgType(), currentArgs, index );
      reference = builder.CreateLoad( i32Ty(), builder.CreateStructGEP( getArgType(), element, 1 ) );
      if( dll )
         nativeGuard( builder.CreateICmpEQ( reference, constant32( 0 ) ), 3107 );

      item = builder.CreateLoad( ptrTy(), builder.CreateStructGEP( getArgType(), element, 0 ) );
      box = freshNativeBox( type, "native.wrapper.param" );
      emitCall( runtime( "x_boxNativeBind", voidTy(), { ptrTy(), ptrTy(), i32Ty() } ), { box, item, reference } );

      if( type == X_DECL_TYPE::PSZ )
         value = nativeConvert( nativeLoad( { nullptr, X_DECL_TYPE::STRING, box } ), { X_DECL_TYPE::PSZ } );
      else
         value = nativeLoad( { nullptr, type, box } );

      arguments.push_back( value.value );
      byValue = builder.CreateICmpEQ( reference, constant32( 0 ) );
      arguments.push_back( builder.CreateSelect( byValue, nullPtr(), box ) );
   }

   /* The item entry of a typed function: the X_ARG items are bound to native boxes, the typed function gets
      the values and the references, its result becomes the result item. A DLL function needs all its
      arguments and none of them by reference */
   bool generateFunctionWrapper( const NATIVE_FUNCTION &function )
   {
      const X_FUNCTION &fn = *function.declaration;
      llvm::SaveAndRestore<llvm::Value *> traceScope( trace, nullptr );
      llvm::SaveAndRestore<int> lineScope( srcLine, fn.line );
      VALUES arguments;
      llvm::Value *result;
      llvm::Value *item;

      currentFunc = functions.at( s_key( fn.name ) );
      currentFunctionName = s_key( fn.name );
      nativeResult = nullptr;
      nativeResultType = X_DECL_TYPE::ANY;
      locals.clear();
      nativeLocals.clear();
      clearFunctionState();
      localTypes.clear();
      loopTargets.clear();

      builder.SetInsertPoint( llvm::BasicBlock::Create( context, "entry", currentFunc ) );
      loadCall( currentFunc->getArg( 0 ) );
      enterTrace();
      beginUnwind();

      arguments.push_back( callCtx );
      if( fn.isDll )
         nativeGuard( builder.CreateICmpEQ( currentArgc, constant32( function.parameters.size() ) ), 3107 );

      for( size_t i = 0; i < function.parameters.size(); ++i )
         wrapperArgument( function, i, fn.isDll, arguments );

      result = emitCall( function.function, arguments );
      if( function.result != X_DECL_TYPE::ANY )
      {
         item = nativeItem( { result, function.result } );
         if( !item )
            return false;

         emitCall( itemCopyFn(), { currentResult, item } );
      }

      emitCleanup();
      leaveTrace();
      builder.CreateRetVoid();
      finishUnwind();

      return true;
   }

   /* ---------------------------------------------------------------------------------------------
      Module lifecycle
      --------------------------------------------------------------------------------------------- */

   bool initializeNativeStatic( const NATIVE_STATIC &slot )
   {
      llvm::SaveAndRestore<int> lineScope( srcLine, slot.line );
      const size_t first = itemSlots.size();
      NATIVE_VALUE value;

      if( !slot.init )
      {
         builder.CreateStore( llvm::Constant::getNullValue( nativeType( slot.type, slot.typeName ) ), slot.storage );
         return true;
      }

      value = nativeExpression( *slot.init );
      if( !value.value )
         return false;

      nativeStore( { slot.storage, slot.type, nullptr, false, slot.typeName }, value.value );
      clearTemps( first, itemSlots.size() );

      return true;
   }

   /* The start value of an item static: its initializer, its dimensions or its constant */
   llvm::Value *staticValue( const STATIC_SLOT &slot )
   {
      if( slot.initializer )
         return expression( *slot.initializer );

      if( slot.dimensions )
         return dimensionExpression( *slot.dimensions );

      return constantItem( slot.value );
   }

   /* x.module.statics: the typed statics, then the item statics get their start values */
   bool initializeStatics()
   {
      for( const NATIVE_STATIC &slot : nativeStaticSlots )
      {
         if( !initializeNativeStatic( slot ) )
            return false;
      }

      for( const auto &slot : staticSlots )
      {
         const size_t first = itemSlots.size();
         llvm::Value *value = staticValue( *slot );

         if( value == nullptr )
            return false;

         emitCall( itemCopyFn(), { slot->storage, value } );
         clearTemps( first, itemSlots.size() );
      }

      return true;
   }

   /* x.module.init: the INIT procedures from the last one collected to the first */
   void callInitProcedures()
   {
      for( auto it = initProcedures.rbegin(); it != initProcedures.rend(); ++it )
      {
         emitCall( functionAbiType(), *it, { callCtx } );
         emitCall( itemClearFn(), { currentResult } );
      }
   }

   /* x.module.exit: the EXIT procedures, each one by x_callCleanup() */
   void callExitProcedures()
   {
      for( llvm::Function *hook : exitProcedures )
      {
         emitCall( runtime( "x_callCleanup", voidTy(), { ptrTy(), ptrTy() } ), { hook, callCtx } );
         emitCall( itemClearFn(), { currentResult } );
      }
   }

   /* The text of a STRING static is released and the static emptied */
   void releaseString( llvm::Value *storage )
   {
      llvm::Type *type = nativeType( X_DECL_TYPE::STRING );
      llvm::Value *value = builder.CreateLoad( type, storage );

      emitCall( runtime( "x_xrelease", voidTy(), { ptrTy() } ), { builder.CreateExtractValue( value, 0 ) } );
      builder.CreateStore( llvm::Constant::getNullValue( type ), storage );
   }

   /* x.module.clear: the statics are cleared while the garbage collector is suspended */
   void clearStatics()
   {
      emitCall( runtime( "x_gcSuspend", voidTy() ) );

      for( const auto &slot : staticSlots )
         emitCall( itemClearFn(), { slot->storage } );

      for( const NATIVE_STATIC &slot : nativeStaticSlots )
      {
         if( slot.type == X_DECL_TYPE::STRING )
            releaseString( slot.storage );
      }

      emitCall( runtime( "x_gcResume", voidTy() ) );
   }

   /* One module callback: 0 statics, 1 INIT, 2 EXIT, 3 clear */
   bool lifecyclePhase( llvm::Function *function, int phase )
   {
      llvm::BasicBlock *entry;

      currentFunc = function;
      locals.clear();
      nativeLocals.clear();
      clearFunctionState();
      currentClassMethod = false;

      entry = llvm::BasicBlock::Create( context, "entry", function );
      cleanupBB = llvm::BasicBlock::Create( context, "cleanup", function );
      builder.SetInsertPoint( entry );
      loadCall( function->getArg( 0 ) );
      beginUnwind();

      if( phase == 0 && !initializeStatics() )
         return false;

      if( phase == 1 )
         callInitProcedures();
      else if( phase == 2 )
         callExitProcedures();
      else if( phase == 3 )
         clearStatics();

      builder.CreateBr( cleanupBB );
      builder.SetInsertPoint( cleanupBB );
      emitCleanup();
      builder.CreateRetVoid();
      finishUnwind();

      return true;
   }

   /* The four module callbacks, registered by x_moduleRegister() in a global constructor */
   bool generateModuleLifecycle( bool libraryModule )
   {
      const char *names[] = { "x.module.statics", "x.module.init", "x.module.exit", "x.module.clear" };
      const auto linkage = llvm::Function::InternalLinkage;
      llvm::FunctionType *registerType = llvm::FunctionType::get( voidTy(), false );
      llvm::Function *callbacks[4];
      llvm::Function *registration;
      llvm::FunctionCallee registerModule;

      for( int phase = 0; phase < 4; ++phase )
      {
         callbacks[phase] = llvm::Function::Create( functionAbiType(), linkage, names[phase], module.get() );
         if( !lifecyclePhase( callbacks[phase], phase ) )
            return false;
      }

      registration = llvm::Function::Create( registerType, linkage, "x.module.register", module.get() );
      builder.SetInsertPoint( llvm::BasicBlock::Create( context, "entry", registration ) );
      registerModule = runtime( "x_moduleRegister", voidTy(), std::vector<llvm::Type *>( 4, ptrTy() ) );
      emitCall( registerModule, { callbacks[0], callbacks[1], callbacks[2], callbacks[3] } );
      builder.CreateRetVoid();
      llvm::appendToGlobalCtors( *module, registration, IIF( libraryModule, 65534, 65535 ) );

      return true;
   }

   /* ---------------------------------------------------------------------------------------------
      Program entry
      --------------------------------------------------------------------------------------------- */

   /* A runtime function without arguments and result */
   void callVoid( const char *name )
   {
      emitCall( runtime( name, voidTy() ) );
   }

   /* A numeric result of Main() is the exit code; otherwise the value set by ErrorLevel() */
   llvm::Value *mainExitCode( llvm::Value *result )
   {
      llvm::Value *type = loadTag( result );
      llvm::Value *integer = builder.CreateOr( tagIs( type, X_IT_INTEGER ), tagIs( type, X_IT_UINT64 ) );
      llvm::Value *payload = loadPayload( result, i64Ty() );
      llvm::Value *level = emitCall( runtime( "x_errorLevelGet", i32Ty() ) );

      return builder.CreateSelect( integer, builder.CreateTrunc( payload, i32Ty() ), level );
   }

   void catchException( llvm::BasicBlock *block, const char *name, llvm::Value *pad, llvm::BasicBlock *errorBB )
   {
      builder.SetInsertPoint( block );
      emitCall( runtime( name, voidTy(), { ptrTy() } ), { builder.CreateExtractValue( pad, 0 ) } );
      builder.CreateBr( errorBB );
   }

   /* The landing pad of main: BREAK and an error are reported and go to errorBB, QUIT gives ErrorLevel() */
   llvm::Value *mainHandlers( llvm::BasicBlock *errorBB )
   {
      llvm::Constant *brkInfo = exceptionInfo( "_ZTI7X_BREAK" );
      llvm::Constant *errInfo = exceptionInfo( "_ZTI9X_FAILURE" );
      llvm::Constant *quitInfo = exceptionInfo( "_ZTI6X_QUIT" );
      llvm::LandingPadInst *pad = builder.CreateLandingPad( exceptionType(), 3 );
      llvm::Function *typeFor;
      llvm::Value *typeId;
      llvm::Value *quitId;
      llvm::Value *selector;
      llvm::BasicBlock *brkBB;
      llvm::BasicBlock *otherBB;
      llvm::BasicBlock *quitBB;
      llvm::BasicBlock *errBB;

      pad->addClause( brkInfo );
      pad->addClause( errInfo );
      pad->addClause( quitInfo );
      typeFor = intrinsicDeclaration( llvm::Intrinsic::eh_typeid_for, { ptrTy() } );
      typeId = emitCall( typeFor, { brkInfo } );
      quitId = emitCall( typeFor, { quitInfo } );
      selector = builder.CreateExtractValue( pad, 1 );
      brkBB = llvm::BasicBlock::Create( context, "entry.break", currentFunc );
      otherBB = llvm::BasicBlock::Create( context, "entry.other", currentFunc );
      quitBB = llvm::BasicBlock::Create( context, "entry.quit", currentFunc );
      errBB = llvm::BasicBlock::Create( context, "entry.failure", currentFunc );
      builder.CreateCondBr( builder.CreateICmpEQ( selector, typeId ), brkBB, otherBB );

      builder.SetInsertPoint( otherBB );
      builder.CreateCondBr( builder.CreateICmpEQ( selector, quitId ), quitBB, errBB );

      catchException( brkBB, "x_catchBreak", pad, errorBB );
      catchException( errBB, "x_catchError", pad, errorBB );

      builder.SetInsertPoint( quitBB );
      emitCall( runtime( "x_catchQuit", voidTy(), { ptrTy() } ), { builder.CreateExtractValue( pad, 0 ) } );

      /* QUIT ends the program with the value set by ErrorLevel() */
      return emitCall( runtime( "x_errorLevelGet", i32Ty() ) );
   }

   /* The modules exit and the runtime ends; a pending error is reported and makes the exit code 1 */
   llvm::Value *mainShutdown( llvm::Value *status, llvm::Value *count, llvm::Value *args )
   {
      llvm::Value *pending;
      llvm::Value *hasError;
      llvm::Value *finalStatus;
      llvm::BasicBlock *releaseClasses;
      llvm::BasicBlock *released;

      callVoid( "x_modulesExit" );
      emitCleanup();
      callVoid( "x_modulesClear" );
      callVoid( "x_errorBlockClear" );
      emitCall( runtime( "x_initArgsFree", voidTy(), { i32Ty(), ptrTy() } ), { count, args } );

      callVoid( "x_clsReleaseAll" );
      pending = emitCall( runtime( "x_errorCaught", i32Ty() ) );
      hasError = builder.CreateICmpNE( pending, constant32( 0 ) );
      finalStatus = builder.CreateSelect( hasError, constant32( 1 ), status );
      callVoid( "x_errorReport" );
      callVoid( "x_errorClear" );

      releaseClasses = llvm::BasicBlock::Create( context, "entry.release", currentFunc );
      released = llvm::BasicBlock::Create( context, "entry.released", currentFunc );
      builder.CreateCondBr( hasError, releaseClasses, released );

      builder.SetInsertPoint( releaseClasses );
      callVoid( "x_clsReleaseAll" );
      callVoid( "x_errorReport" );
      callVoid( "x_errorClear" );
      builder.CreateBr( released );

      builder.SetInsertPoint( released );
      callVoid( "x_runtimeExit" );

      return finalStatus;
   }

   /* main( argc, argv ): the runtime and the modules start and Main() runs; its exit code, a BREAK, an error
      or a QUIT end the program */
   bool generateEntryMain()
   {
      const auto entry = functions.find( "MAIN" );
      llvm::FunctionType *type = llvm::FunctionType::get( i32Ty(), { i32Ty(), ptrTy() }, false );
      llvm::Function *mainFn;
      llvm::BasicBlock *errorBB;
      llvm::BasicBlock *finish;
      llvm::Value *result;
      llvm::Value *argc;
      llvm::Value *positive;
      llvm::Value *rest;
      llvm::Value *count;
      llvm::Value *argv;
      llvm::Value *args;
      llvm::Value *runtimeStatus;
      llvm::BasicBlock *runtimeReady;
      llvm::Value *code;
      llvm::BasicBlock *success;
      llvm::Value *quitLevel;
      llvm::BasicBlock *quitDone;
      llvm::PHINode *status;

      if( entry == functions.end() || lifecycleNames.count( "MAIN" ) )
      {
         fail( "program wymaga FUNCTION Main(); biblioteke kompiluj przez -c" );
         return false;
      }

      mainFn = llvm::Function::Create( type, llvm::Function::ExternalLinkage, "main", module.get() );
      currentFunc = mainFn;
      locals.clear();
      clearFunctionState();
      currentClassMethod = false;

      builder.SetInsertPoint( llvm::BasicBlock::Create( context, "entry", mainFn ) );
      errorBB = llvm::BasicBlock::Create( context, "entry.error", mainFn );
      finish = llvm::BasicBlock::Create( context, "entry.finish", mainFn );
      result = freshItem( "entry.result" );
      argc = mainFn->getArg( 0 );
      positive = builder.CreateICmpSGT( argc, constant32( 0 ) );
      rest = builder.CreateSub( argc, constant32( 1 ) );
      count = builder.CreateSelect( positive, rest, constant32( 0 ) );
      argv = builder.CreateGEP( ptrTy(), mainFn->getArg( 1 ), constant64( 1 ) );
      args = emitCall( runtime( "x_initArgs", ptrTy(), { i32Ty(), ptrTy() } ), { count, argv } );
      beginUnwind( false );

      /* The generator knows only the lifecycle of the runtime, not the terminal (constitution, point 18) */
      runtimeStatus = emitCall( runtime( "x_runtimeInit", i32Ty() ) );
      runtimeReady = llvm::BasicBlock::Create( context, "entry.runtime", mainFn );
      builder.CreateCondBr( builder.CreateICmpEQ( runtimeStatus, constant32( 0 ) ), runtimeReady, errorBB );

      builder.SetInsertPoint( runtimeReady );
      emitCall( runtime( "x_commandArgsInit", voidTy(), { i32Ty(), ptrTy() } ), { argc, mainFn->getArg( 1 ) } );
      emitCall( runtime( "x_modulesInit", voidTy(), { i32Ty(), ptrTy() } ), { count, args } );
      emitCall( functionAbiType(), entry->second, { callFrame( result, count, args ) } );
      code = mainExitCode( result );
      success = builder.GetInsertBlock();
      builder.CreateBr( finish );

      unwindBB = nullptr;
      builder.SetInsertPoint( unwindTop );
      quitLevel = mainHandlers( errorBB );
      quitDone = builder.GetInsertBlock();
      builder.CreateBr( finish );

      builder.SetInsertPoint( errorBB );
      builder.CreateBr( finish );

      builder.SetInsertPoint( finish );
      status = builder.CreatePHI( i32Ty(), 3 );
      status->addIncoming( code, success );
      status->addIncoming( constant32( 1 ), errorBB );
      status->addIncoming( quitLevel, quitDone );
      builder.CreateRet( mainShutdown( status, count, args ) );

      return true;
   }

   /* ---------------------------------------------------------------------------------------------
      Function declarations
      --------------------------------------------------------------------------------------------- */

   /* The native signature of a function while it is built */
   struct SIGNATURE
   {
      std::vector<X_DECL_TYPE> parameters;
      std::vector<std::string> typeNames;
      std::vector<llvm::Type *> types;
      bool supported = true;
   };

   /* OBJECT, ARRAY, CODEBLOCK, DATE and HASH stay items: a function that has them has no native entry */
   static bool itemOnlyType( X_DECL_TYPE type )
   {
      switch( type )
      {
      case X_DECL_TYPE::OBJECT:
      case X_DECL_TYPE::ARRAY:
      case X_DECL_TYPE::CODEBLOCK:
      case X_DECL_TYPE::DATE:
      case X_DECL_TYPE::HASH: return true;
      default:                return false;
      }
   }

   /* A typed parameter adds its native value and its reference to the signature; a DLL gets a STRUCT by
      pointer */
   bool nativeParameter( const X_FUNCTION &fn, const std::string &name, SIGNATURE &signature )
   {
      const auto declared = fn.paramTypes.find( s_key( name ) );
      const auto named = fn.paramTypeNames.find( s_key( name ) );
      const X_DECL_TYPE type = IIF( declared == fn.paramTypes.end(), X_DECL_TYPE::ANY, declared->second );
      const std::string typeName = IIF( named != fn.paramTypeNames.end(), named->second, std::string() );
      llvm::Type *parameterType;

      signature.parameters.push_back( type );
      signature.typeNames.push_back( typeName );

      if( itemOnlyType( type ) )
      {
         signature.supported = false;
         return true;
      }

      if( type == X_DECL_TYPE::ANY )
         return true;

      parameterType = IIF( fn.isDll && type == X_DECL_TYPE::STRUCT, ptrTy(), nativeType( type, typeName ) );
      if( !parameterType || fn.methodKind == "ONERROR" )
      {
         fail( fn.line, "nieobslugiwany typ parametru w biezacym etapie ABI: " + name );
         return false;
      }

      signature.types.push_back( parameterType );
      signature.types.push_back( ptrTy() );

      return true;
   }

   /* A typed function gets the native entry x.native.NAME( ctx, value, reference, ... ) */
   bool declareNative( const X_FUNCTION &fn, const std::string &key )
   {
      const X_DECL_TYPE result = fn.resultType.value_or( X_DECL_TYPE::ANY );
      SIGNATURE signature;
      llvm::Type *resultType;
      llvm::FunctionType *type;
      llvm::Function *nativeFn;
      NATIVE_FUNCTION entry;

      signature.types.push_back( ptrTy() );

      for( const std::string &name : fn.params )
      {
         if( !nativeParameter( fn, name, signature ) )
            return false;
      }

      if( itemOnlyType( result ) )
         signature.supported = false;

      if( result != X_DECL_TYPE::ANY && !itemOnlyType( result ) && !nativeType( result ) )
      {
         const std::string resultName = x_declTypeName( result );

         fail( fn.line, "nieobslugiwany typ wyniku w biezacym etapie ABI: " + resultName );
         return false;
      }

      if( !signature.supported || !( fn.isDll || result != X_DECL_TYPE::ANY || signature.types.size() > 1 ) )
         return true;

      resultType = IIF( result == X_DECL_TYPE::ANY, voidTy(), nativeType( result ) );
      type = llvm::FunctionType::get( resultType, signature.types, false );
      nativeFn = llvm::Function::Create( type, llvm::Function::InternalLinkage, "x.native." + key, module.get() );
      nativeFn->getArg( 0 )->setName( "ctx" );

      entry.function = nativeFn;
      entry.declaration = &fn;
      entry.parameters = std::move( signature.parameters );
      entry.parameterTypeNames = std::move( signature.typeNames );
      entry.result = result;
      entry.resultTypeName = fn.resultTypeName;
      nativeFunctions.emplace( key, std::move( entry ) );

      return true;
   }

   /* The item entry NAME( ctx ) of a function; INIT and EXIT procedures and STATIC functions are internal */
   bool declareFunction( const X_FUNCTION &fn )
   {
      const std::string key = s_key( fn.name );
      const bool lifecycle = fn.role != X_PROC_ROLE::NORMAL;
      const bool internal = lifecycle || fn.isStatic;
      const auto linkage = IIF( internal, llvm::Function::InternalLinkage, llvm::Function::ExternalLinkage );
      llvm::Function *f = llvm::Function::Create( functionAbiType(), linkage, key, module.get() );

      if( sharedModule && !internal )
         f->setDLLStorageClass( llvm::GlobalValue::DLLExportStorageClass );

      if( lifecycle )
         lifecycleNames.insert( key );

      if( fn.role == X_PROC_ROLE::INIT )
         initProcedures.push_back( f );

      if( fn.role == X_PROC_ROLE::EXIT )
         exitProcedures.push_back( f );

      f->getArg( 0 )->setName( "ctx" );
      functions[key] = f;

      return declareNative( fn, key );
   }

   bool declareFunctions( const X_PROGRAM &program )
   {
      for( const X_FUNCTION &fn : program.functions )
      {
         if( !declareFunction( fn ) )
            return false;
      }

      for( const X_FUNCTION &fn : program.dllFunctions )
      {
         if( !declareFunction( fn ) )
            return false;
      }

      return true;
   }

   /* ---------------------------------------------------------------------------------------------
      DLL functions
      --------------------------------------------------------------------------------------------- */

   /* DLL FUNCTION on Linux takes numbers, PTR and PSZ: no LOGIC, STRUCT or CALLBACK */
   static bool linuxDllType( X_DECL_TYPE type )
   {
      return type != X_DECL_TYPE::LOGIC && type != X_DECL_TYPE::STRUCT && type != X_DECL_TYPE::CALLBACK;
   }

   static bool linuxDllSupported( const X_FUNCTION &fn, const NATIVE_FUNCTION &entry )
   {
      if( !fn.isProcedure && !linuxDllType( entry.result ) )
         return false;

      for( X_DECL_TYPE type : entry.parameters )
      {
         if( !linuxDllType( type ) )
            return false;
      }

      return true;
   }

   /* A DLL gets a STRUCT by pointer */
   llvm::Type *dllParameterType( const NATIVE_FUNCTION &entry, size_t i )
   {
      if( entry.parameters[i] == X_DECL_TYPE::STRUCT )
         return ptrTy();

      return nativeType( entry.parameters[i], entry.parameterTypeNames[i] );
   }

   /* The imported symbol: on Windows x.dll.LIBRARY.SYMBOL with dllimport, elsewhere the symbol itself */
   llvm::Function *importedFunction( const X_DLL_FUNCTION &fn, llvm::FunctionType *type )
   {
#if X_OS_WINDOWS
      const std::string name = "x.dll." + fn.library + "." + fn.symbol;
      llvm::Function *imported = llvm::Function::Create( type, llvm::Function::ExternalLinkage, name, module.get() );

      imported->setDLLStorageClass( llvm::GlobalValue::DLLImportStorageClass );

      return imported;
#else
      llvm::FunctionCallee callee = module->getOrInsertFunction( fn.symbol, type );
      llvm::Function *imported = llvm::dyn_cast<llvm::Function>( callee.getCallee() );

      if( !imported || imported->getFunctionType() != type )
      {
         fail( fn.line, "niezgodne deklaracje symbolu biblioteki: " + fn.symbol );
         return nullptr;
      }

      return imported;
#endif
   }

   /* A C caller extends small integers: INT8 and INT16 by sign, UINT8 and UINT16 by zero */
   static llvm::Attribute::AttrKind integerExtension( X_DECL_TYPE type )
   {
      if( type == X_DECL_TYPE::INT8 || type == X_DECL_TYPE::INT16 )
         return llvm::Attribute::SExt;

      if( type == X_DECL_TYPE::UINT8 || type == X_DECL_TYPE::UINT16 )
         return llvm::Attribute::ZExt;

      return llvm::Attribute::None;
   }

   /* Outside Windows small integers are extended; a LOGIC is a zero extended byte everywhere */
   void dllAttributes( [[maybe_unused]] const X_FUNCTION &fn, const NATIVE_FUNCTION &entry, llvm::Function *imported )
   {
#if !X_OS_WINDOWS
      const llvm::Attribute::AttrKind resultExtension = integerExtension( entry.result );

      if( !fn.isProcedure && resultExtension != llvm::Attribute::None )
         imported->addRetAttr( resultExtension );

      for( size_t i = 0; i < entry.parameters.size(); ++i )
      {
         const llvm::Attribute::AttrKind parameterExtension = integerExtension( entry.parameters[i] );

         if( parameterExtension != llvm::Attribute::None )
            imported->addParamAttr( i, parameterExtension );
      }
#endif

      if( entry.result == X_DECL_TYPE::LOGIC )
         imported->addRetAttr( llvm::Attribute::ZExt );

      for( size_t i = 0; i < entry.parameters.size(); ++i )
      {
         if( entry.parameters[i] == X_DECL_TYPE::LOGIC )
            imported->addParamAttr( i, llvm::Attribute::ZExt );
      }
   }

   /* The native entry of a DLL function calls the import between x_callbackEnterDll() and
      x_callbackLeaveDll(), then raises the error of a callback, if any */
   void dllEntry( const X_FUNCTION &fn, const NATIVE_FUNCTION &entry, llvm::Function *imported,
                  const VALUES &arguments )
   {
      llvm::CallInst *call;

      builder.SetInsertPoint( llvm::BasicBlock::Create( context, "entry", entry.function ) );
      builder.CreateCall( runtime( "x_callbackEnterDll", voidTy() ) );
      call = builder.CreateCall( imported, arguments );
      call->setAttributes( imported->getAttributes() );
      builder.CreateCall( runtime( "x_callbackLeaveDll", voidTy() ) );
      builder.CreateCall( runtime( "x_callbackRaisePending", voidTy() ) );
      if( fn.isProcedure )
         builder.CreateRetVoid();
      else
         builder.CreateRet( call );
   }

   /* A DLL function: its import, its native entry and its line LIBRARY, SYMBOL, import name in the metadata */
   bool importDllFunction( const X_DLL_FUNCTION &fn, std::string &dllMetadata )
   {
#if !X_OS_WINDOWS && !X_OS_LINUX
      fail( "DLL FUNCTION w tym etapie wymaga Windows albo Linux" );
      return false;
#endif
      const NATIVE_FUNCTION &entry = nativeFunctions.at( s_key( fn.name ) );
      std::vector<llvm::Type *> signature;
      VALUES arguments;
      llvm::Type *importedResult;
      llvm::FunctionType *importedType;
      llvm::Function *imported;

#if X_OS_LINUX
      if( !linuxDllSupported( fn, entry ) )
      {
         fail( fn.line, "DLL FUNCTION na Linuxie w tym etapie obsluguje liczby, PTR i PSZ" );
         return false;
      }
#endif

      for( size_t i = 0; i < entry.parameters.size(); ++i )
      {
         signature.push_back( dllParameterType( entry, i ) );
         arguments.push_back( entry.function->getArg( 1 + i * 2 ) );
      }

      importedResult = IIF( fn.isProcedure, voidTy(), nativeType( entry.result ) );
      importedType = llvm::FunctionType::get( importedResult, signature, false );
      imported = importedFunction( fn, importedType );
      if( imported == nullptr )
         return false;

      dllAttributes( fn, entry, imported );
      dllEntry( fn, entry, imported, arguments );

      dllMetadata += fn.library;
      dllMetadata.push_back( '\0' );
      dllMetadata += fn.symbol;
      dllMetadata.push_back( '\0' );
      dllMetadata += imported->getName().str();
      dllMetadata.push_back( '\0' );

      return true;
   }

   /* A private constant in a section of its own, kept by llvm.used */
   void embedSection( const std::string &text, const char *name, const char *section, bool exclude )
   {
      const auto linkage = llvm::GlobalValue::PrivateLinkage;
      llvm::Constant *data = llvm::ConstantDataArray::getString( context, text, false );
      llvm::GlobalVariable *metadata = new llvm::GlobalVariable( *module, data->getType(), true, linkage, data, name );

      metadata->setSection( section );
      metadata->setAlignment( llvm::Align( 1 ) );
      if( exclude )
         metadata->setMetadata( llvm::LLVMContext::MD_exclude, llvm::MDNode::get( context, {} ) );

      llvm::appendToUsed( *module, { metadata } );
   }

   /* The DLL functions; the linker finds their imports in the .xdevdll section */
   bool importDllFunctions( const X_PROGRAM &program )
   {
      std::string dllMetadata;

      for( const X_DLL_FUNCTION &fn : program.dllFunctions )
      {
         if( !importDllFunction( fn, dllMetadata ) )
            return false;
      }

      if( !dllMetadata.empty() )
         embedSection( dllMetadata, "x.dll.imports", ".xdevdll", false );

      return true;
   }

   /* ---------------------------------------------------------------------------------------------
      Module
      --------------------------------------------------------------------------------------------- */

   /* A requested function, declared when this module does not define it */
   llvm::Function *requestedFunction( const std::string &name )
   {
      const auto found = functions.find( name );
      llvm::Function *function;

      if( found != functions.end() )
         return found->second;

      function = llvm::Function::Create( functionAbiType(), llvm::Function::ExternalLinkage, name, module.get() );
      functions[name] = function;

      return function;
   }

   /* REQUEST name: the anchor x.request.NAME keeps the function in the link */
   void requestAnchor( const std::string &name, std::vector<llvm::Constant *> &roots )
   {
      const auto linkage = llvm::GlobalValue::LinkOnceODRLinkage;
      const std::string anchorName = "x.request." + name;
      llvm::Function *function = requestedFunction( name );
      llvm::GlobalVariable *anchor = new llvm::GlobalVariable( *module, ptrTy(), true, linkage, function, anchorName );

      anchor->setComdat( module->getOrInsertComdat( anchor->getName() ) );
      roots.push_back( anchor );

#if X_OS_WINDOWS
      llvm::MDString *option = llvm::MDString::get( context, "/include:x.request." + name );

      module->getOrInsertNamedMetadata( "llvm.linker.options" )->addOperand( llvm::MDNode::get( context, option ) );
#endif
   }

   void requestStatements( const std::vector<X_STMT> &body, std::unordered_set<std::string> &requested,
                           std::vector<llvm::Constant *> &roots )
   {
      for( const X_STMT &stmt : body )
      {
         for( const std::string &name : stmt.requests )
         {
            if( requested.insert( name ).second )
               requestAnchor( name, roots );
         }
      }
   }

   /* The REQUEST statements of the module; the anchors are kept by llvm.used */
   void collectRequests( const X_PROGRAM &program )
   {
      const auto linkage = llvm::GlobalValue::AppendingLinkage;
      std::unordered_set<std::string> requested;
      std::vector<llvm::Constant *> roots;
      llvm::ArrayType *type;
      llvm::Constant *list;
      llvm::GlobalVariable *used;

      requestStatements( program.statements, requested, roots );

      for( const X_FUNCTION &fn : program.functions )
         requestStatements( fn.body, requested, roots );

      if( roots.empty() )
         return;

      type = llvm::ArrayType::get( ptrTy(), roots.size() );
      list = llvm::ConstantArray::get( type, roots );
      used = new llvm::GlobalVariable( *module, type, false, linkage, list, "llvm.used" );
      used->setSection( "llvm.metadata" );
   }

   /* Every class gets its registrar; then the direct methods are collected and the registrars generated */
   bool generateClasses( const X_PROGRAM &program )
   {
      const auto linkage = llvm::Function::InternalLinkage;
      llvm::FunctionType *type = llvm::FunctionType::get( i32Ty(), false );

      for( const X_CLASS_DECL &cls : program.classes )
      {
         const std::string name = "__XDEV_REGISTER_" + cls.name;

         classRegistrars[cls.name] = llvm::Function::Create( type, linkage, name, module.get() );
      }

      collectMethods( program );

      for( const X_CLASS_DECL &cls : program.classes )
      {
         if( !generateClassRegistration( cls ) )
            return false;
      }

      return true;
   }

   /* The function bodies with the wrappers of the typed functions, the DLL wrappers and the trampolines */
   bool generateFunctions( const X_PROGRAM &program )
   {
      for( const X_FUNCTION &fn : program.functions )
      {
         if( !generateFunctionBody( fn ) )
            return false;

         const auto typed = nativeFunctions.find( s_key( fn.name ) );

         if( typed != nativeFunctions.end() && !generateFunctionWrapper( typed->second ) )
            return false;
      }

      for( const X_FUNCTION &fn : program.dllFunctions )
      {
         if( !generateFunctionWrapper( nativeFunctions.at( s_key( fn.name ) ) ) )
            return false;
      }

      for( const CALLBACK_JOB &job : callbackJobs )
      {
         if( !generateCallbackTrampoline( job ) )
            return false;
      }

      return true;
   }

   /* The code blocks; a block may add the jobs of the blocks inside it */
   bool generateBlocks()
   {
      for( size_t i = 0; i < blockJobs.size(); ++i )
      {
         const BLOCK_JOB block = blockJobs[i];
         X_FUNCTION function;

         function.name = block.function->getName().str();
         function.params = block.expression->blockParams;
         function.isStatic = true;
         if( !generateFunctionBody( function, &block ) )
            return false;
      }

      return true;
   }

   /* The whole module: declarations, DLL imports, the class interface, requests, classes, functions,
      the lifecycle, main and the code blocks */
   bool generateModule( const X_PROGRAM &program, bool libraryModule )
   {
      std::string classInterface;

      for( const X_FUNCTION &fn : program.functions )
      {
         if( sharedModule && fn.role == X_PROC_ROLE::NORMAL && s_key( fn.name ) == "MAIN" )
         {
            fail( "modul DLL (--shared) nie moze zawierac FUNCTION Main(); Main ma tylko program" );
            return false;
         }
      }

      for( const X_CALLBACK_DECL &callback : program.callbacks )
         callbackTypes.emplace( s_key( callback.name ), &callback );

      for( const X_FUNCTION &fn : program.dllFunctions )
         nativeDllNames.insert( s_key( fn.name ) );

      if( !createStructTypes( program.structs ) || !collectStatics( program.scope, "" ) )
         return false;

      for( const X_FUNCTION &fn : program.functions )
      {
         if( !collectStatics( fn.scope, s_key( fn.name ) ) )
            return false;
      }

      if( !declareFunctions( program ) || !importDllFunctions( program ) )
         return false;

      classInterface = x_classInterfaceWrite( program );
      if( !classInterface.empty() )
         embedSection( classInterface, "x.class.interface", ".xdevcls", true );

      collectRequests( program );

      if( !generateClasses( program ) || !generateFunctions( program ) )
         return false;

      if( !generateModuleLifecycle( libraryModule ) || ( !libraryModule && !generateEntryMain() ) )
         return false;

      if( !generateBlocks() )
         return false;

      if( sharedModule )
         generateModuleVersion();

      return true;
   }

   /* --shared: the exported x_moduleVersion() returns X_VER_MODULE for ModuleLoad(); every PRG of the module
      has it, the linker keeps one. Weak ODR, because the optimizer removes an unused link-once function */
   void generateModuleVersion()
   {
      const auto linkage = llvm::Function::WeakODRLinkage;
      llvm::FunctionType *type = llvm::FunctionType::get( i32Ty(), false );
      llvm::Function *version = llvm::Function::Create( type, linkage, "x_moduleVersion", module.get() );

      version->setComdat( module->getOrInsertComdat( "x_moduleVersion" ) );
      version->setDLLStorageClass( llvm::GlobalValue::DLLExportStorageClass );
      builder.SetInsertPoint( llvm::BasicBlock::Create( context, "entry", version ) );
      builder.CreateRet( constant32( X_VER_MODULE ) );
   }
};

/* The key of a name: names are not case sensitive */
static std::string s_key( const std::string &name )
{
   std::string key = name;

   for( char &c : key )
      c = ( char )std::toupper( ( unsigned char )c );

   return key;
}

/* --------------------------------------------------------------------------------------------
   Target
   -------------------------------------------------------------------------------------------- */

/* The target machine of the host; the module gets its triple and its data layout */
static std::unique_ptr<llvm::TargetMachine> s_targetMachine( llvm::Module &module, std::string &error )
{
   const auto relocation = llvm::Reloc::PIC_;
   const auto level = llvm::CodeGenOptLevel::Default;
   llvm::TargetOptions options;
   std::string targetError;
   const llvm::Target *target;
   std::unique_ptr<llvm::TargetMachine> machine;

   /* Every function and every variable in a section of its own, for --gc-sections of the linker */
   options.FunctionSections = true;
   options.DataSections = true;

   llvm::InitializeNativeTarget();
   llvm::InitializeNativeTargetAsmPrinter();
   llvm::InitializeNativeTargetAsmParser();

#if LLVM_VERSION_MAJOR >= 21
   const llvm::Triple triple( llvm::sys::getDefaultTargetTriple() );
#else
   const std::string triple = llvm::sys::getDefaultTargetTriple();
#endif

   target = llvm::TargetRegistry::lookupTarget( triple, targetError );
   if( target == nullptr )
   {
      error = "nie znaleziono celu LLVM: " + targetError;
      return nullptr;
   }

   machine.reset( target->createTargetMachine( triple, "generic", "", options, relocation, std::nullopt, level ) );
   if( !machine )
   {
      error = "nie mozna utworzyc maszyny docelowej LLVM";
      return nullptr;
   }

   module.setTargetTriple( triple );
   module.setDataLayout( machine->createDataLayout() );

   return machine;
}

static bool s_verify( llvm::Module &module, const char *what, std::string &error )
{
   std::string text;
   llvm::raw_string_ostream stream( text );

   if( !llvm::verifyModule( module, &stream ) )
      return true;

   stream.flush();
   error = what + text;

   return false;
}

/* The O2 pipeline of the new pass manager */
static void s_optimize( llvm::Module &module, llvm::TargetMachine &machine )
{
   llvm::LoopAnalysisManager loopAnalyses;
   llvm::FunctionAnalysisManager functionAnalyses;
   llvm::CGSCCAnalysisManager cgsccAnalyses;
   llvm::ModuleAnalysisManager moduleAnalyses;
   llvm::PassBuilder passBuilder( &machine );

   passBuilder.registerModuleAnalyses( moduleAnalyses );
   passBuilder.registerCGSCCAnalyses( cgsccAnalyses );
   passBuilder.registerFunctionAnalyses( functionAnalyses );
   passBuilder.registerLoopAnalyses( loopAnalyses );
   passBuilder.crossRegisterProxies( loopAnalyses, functionAnalyses, cgsccAnalyses, moduleAnalyses );

   llvm::ModulePassManager passes = passBuilder.buildPerModuleDefaultPipeline( llvm::OptimizationLevel::O2 );

   passes.run( module, moduleAnalyses );
}

static bool s_writeObject( llvm::Module &module, const std::string &path, std::string &error )
{
   std::unique_ptr<llvm::TargetMachine> machine = s_targetMachine( module, error );
   std::error_code ec;

   if( !machine )
      return false;

   llvm::raw_fd_ostream destination( path, ec, llvm::sys::fs::OF_None );
   if( ec )
   {
      error = "nie mozna zapisac pliku obiektowego: " + path;
      return false;
   }

   llvm::legacy::PassManager pass;
   if( machine->addPassesToEmitFile( pass, destination, nullptr, llvm::CodeGenFileType::ObjectFile ) )
   {
      error = "LLVM nie obsluguje generowania pliku obiektowego dla tego celu";
      return false;
   }

   pass.run( module );
   destination.flush();

   return true;
}

/* --------------------------------------------------------------------------------------------
   Public API
   -------------------------------------------------------------------------------------------- */

X_LLVM::X_LLVM() : m_impl( std::make_unique<IMPL>() )
{
}

X_LLVM::~X_LLVM() = default;

/* The module of a program, verified and optimized */
bool X_LLVM::generate( const X_PROGRAM &program, std::string &error, bool libraryModule,
                       const std::function<std::string( int )> &location, bool sharedModule )
try
{
   const char *invalid = "wygenerowany modul LLVM jest niepoprawny (blad wewnetrzny kompilatora): ";
   const char *optimized = "modul LLVM jest niepoprawny po optymalizacji (blad wewnetrzny kompilatora): ";
   IMPL &impl = *m_impl;
   std::unique_ptr<llvm::TargetMachine> machine;

   if( !program.symbolsResolved )
   {
      error = "blad wewnetrzny: backend wymaga rozstrzygnietych symboli frontendu";
      return false;
   }

   impl.sourceLoc = location;
   impl.sharedModule = sharedModule;
   if( !impl.generateModule( program, libraryModule ) )
   {
      error = impl.error;
      return false;
   }

   if( !s_verify( *impl.module, invalid, error ) )
      return false;

   machine = s_targetMachine( *impl.module, error );
   if( !machine )
      return false;

   s_optimize( *impl.module, *machine );

   return s_verify( *impl.module, optimized, error );
}
catch( const std::out_of_range & )
{
   error = "blad wewnetrzny generatora LLVM: brak wpisu w metadanych lub indeks poza zakresem";
   return false;
}

std::string X_LLVM::ir() const
{
   std::string text;
   llvm::raw_string_ostream stream( text );

   if( m_impl && m_impl->module )
      m_impl->module->print( stream, nullptr );

   stream.flush();

   return text;
}

bool X_LLVM::writeIR( const std::string &path, std::string &error ) const
{
   std::error_code ec;

   if( !m_impl || !m_impl->module )
   {
      error = "brak modulu LLVM";
      return false;
   }

   llvm::raw_fd_ostream stream( path, ec );
   if( ec )
   {
      error = "nie mozna zapisac IR: " + path;
      return false;
   }

   m_impl->module->print( stream, nullptr );

   return true;
}

bool X_LLVM::writeObject( const std::string &path, std::string &error ) const
{
   if( !m_impl || !m_impl->module )
   {
      error = "brak modulu LLVM";
      return false;
   }

   return s_writeObject( *m_impl->module, path, error );
}

/* --------------------------------------------------------------------------------------------
   Embedded files
   -------------------------------------------------------------------------------------------- */

/* A private constant array of bytes; a name ends with a zero byte */
static llvm::GlobalVariable *s_privateBytes( llvm::Module &module, const std::string &bytes, bool zero )
{
   llvm::LLVMContext &context = module.getContext();
   llvm::Type *type = llvm::ArrayType::get( llvm::Type::getInt8Ty( context ), bytes.size() + IIF( zero, 1, 0 ) );
   llvm::Constant *value = llvm::ConstantDataArray::getString( context, bytes, zero );

   return new llvm::GlobalVariable( module, type, true, llvm::GlobalValue::PrivateLinkage, value );
}

/* One embedded file: { name, data, size }; two files may not have the same name */
static bool s_resourceEntry( llvm::Module &module, llvm::StructType *entryTy, const std::string &file,
                             std::unordered_set<std::string> &names, std::vector<llvm::Constant *> &entries,
                             std::string &error )
{
   std::ifstream stream( file, std::ios::binary );
   std::string data( ( std::istreambuf_iterator<char>( stream ) ), std::istreambuf_iterator<char>() );
   const std::string name = std::filesystem::path( file ).stem().string();
   llvm::Type *sizeTy = llvm::Type::getInt64Ty( module.getContext() );
   llvm::GlobalVariable *bytes;
   llvm::GlobalVariable *text;
   llvm::Constant *size;

   if( !stream.good() && !stream.eof() )
   {
      error = "nie mozna odczytac pliku: " + file;
      return false;
   }

   if( !names.insert( s_key( name ) ).second )
   {
      error = "powtorzona nazwa pliku w programie: " + name;
      return false;
   }

   bytes = s_privateBytes( module, data, false );
   text = s_privateBytes( module, name, true );
   size = llvm::ConstantInt::get( sizeTy, data.size() );
   entries.push_back( llvm::ConstantStruct::get( entryTy, { text, bytes, size } ) );

   return true;
}

/* Embedded files: the table { name without extension, data, size } ends with an unnamed entry; a global
   constructor gives it to the runtime through x_resourcesRegister(), also when the runtime is a DLL */
bool x_llvmResources( const std::vector<std::string> &files, const std::string &path, std::string &error )
{
   const auto linkage = llvm::GlobalValue::PrivateLinkage;
   llvm::LLVMContext context;
   llvm::Module module( "resources", context );
   llvm::PointerType *ptrTy = llvm::PointerType::getUnqual( context );
   llvm::Type *voidTy = llvm::Type::getVoidTy( context );
   llvm::Type *sizeTy = llvm::Type::getInt64Ty( context );
   llvm::StructType *entryTy = llvm::StructType::get( context, { ptrTy, ptrTy, sizeTy } );
   llvm::Constant *null = llvm::ConstantPointerNull::get( ptrTy );
   std::vector<llvm::Constant *> entries;
   std::unordered_set<std::string> names;
   llvm::ArrayType *tableTy;
   llvm::Constant *table;
   llvm::GlobalVariable *tableVar;
   llvm::Function *registration;
   llvm::FunctionCallee registerTable;

   for( const std::string &file : files )
   {
      if( !s_resourceEntry( module, entryTy, file, names, entries, error ) )
         return false;
   }

   entries.push_back( llvm::ConstantStruct::get( entryTy, { null, null, llvm::ConstantInt::get( sizeTy, 0 ) } ) );
   tableTy = llvm::ArrayType::get( entryTy, entries.size() );
   table = llvm::ConstantArray::get( tableTy, entries );
   tableVar = new llvm::GlobalVariable( module, tableTy, true, linkage, table, "x.resources" );

   registration = llvm::Function::Create( llvm::FunctionType::get( voidTy, false ), llvm::Function::InternalLinkage,
                                          "x.resources.register", module );
   registerTable = module.getOrInsertFunction( "x_resourcesRegister", voidTy, ptrTy );

   llvm::IRBuilder<> builder( llvm::BasicBlock::Create( context, "entry", registration ) );

   builder.CreateCall( registerTable, { tableVar } );
   builder.CreateRetVoid();
   llvm::appendToGlobalCtors( module, registration, 65535 );

   return s_writeObject( module, path, error );
}
