/*
 * Copyright (c) 2024-2026 xDev-Language
 *
 * Library: xDev Backend
 * File: native.cpp
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "native.hpp"

#include <algorithm>
#include <cmath>

#include <llvm/ADT/APInt.h>
#include <llvm/IR/Constants.h>
#include <llvm/IR/Intrinsics.h>

/* The comparison of each operator for floats, unsigned and signed integers */
struct X_PREDICATE
{
   const char *op;
   llvm::CmpInst::Predicate floating;
   llvm::CmpInst::Predicate unsignedInt;
   llvm::CmpInst::Predicate signedInt;
};

static const X_PREDICATE s_predicates[] = {
   { "=",  llvm::CmpInst::FCMP_OEQ, llvm::CmpInst::ICMP_EQ,  llvm::CmpInst::ICMP_EQ  },
   { "==", llvm::CmpInst::FCMP_OEQ, llvm::CmpInst::ICMP_EQ,  llvm::CmpInst::ICMP_EQ  },
   { "!=", llvm::CmpInst::FCMP_UNE, llvm::CmpInst::ICMP_NE,  llvm::CmpInst::ICMP_NE  },
   { "<",  llvm::CmpInst::FCMP_OLT, llvm::CmpInst::ICMP_ULT, llvm::CmpInst::ICMP_SLT },
   { "<=", llvm::CmpInst::FCMP_OLE, llvm::CmpInst::ICMP_ULE, llvm::CmpInst::ICMP_SLE },
   { ">",  llvm::CmpInst::FCMP_OGT, llvm::CmpInst::ICMP_UGT, llvm::CmpInst::ICMP_SGT },
   { ">=", llvm::CmpInst::FCMP_OGE, llvm::CmpInst::ICMP_UGE, llvm::CmpInst::ICMP_SGE }
};

static bool s_unsigned( X_DECL_TYPE type )
{
   return type >= X_DECL_TYPE::UINT8 && type <= X_DECL_TYPE::UINT64;
}

static bool s_integer( X_DECL_TYPE type )
{
   return type >= X_DECL_TYPE::INT8 && type <= X_DECL_TYPE::UINT64;
}

/* UINT* and LOGIC widen with zeros, the signed integers with their sign */
static bool s_zeroExtends( X_DECL_TYPE type )
{
   return s_unsigned( type ) || type == X_DECL_TYPE::LOGIC;
}

/* An explicit conversion with a range check */
static X_TYPE_CONVERSION s_checkedCast( X_DECL_TYPE target )
{
   X_TYPE_CONVERSION conversion{ target };

   conversion.explicitCast = true;
   conversion.checkRange = true;

   return conversion;
}

static const X_PREDICATE *s_findPredicate( const std::string &op )
{
   for( const X_PREDICATE &entry : s_predicates )
   {
      if( op == entry.op )
         return &entry;
   }

   return nullptr;
}

/* + - * of integers with an overflow flag; not_intrinsic for other operators */
static llvm::Intrinsic::ID s_overflowIntrinsic( const std::string &op, bool unsignedType )
{
   if( op == "+" && unsignedType )
      return llvm::Intrinsic::uadd_with_overflow;

   if( op == "+" )
      return llvm::Intrinsic::sadd_with_overflow;

   if( op == "-" && unsignedType )
      return llvm::Intrinsic::usub_with_overflow;

   if( op == "-" )
      return llvm::Intrinsic::ssub_with_overflow;

   if( op == "*" && unsignedType )
      return llvm::Intrinsic::umul_with_overflow;

   if( op == "*" )
      return llvm::Intrinsic::smul_with_overflow;

   return llvm::Intrinsic::not_intrinsic;
}

X_LLVM_NATIVE::X_LLVM_NATIVE( llvm::IRBuilder<> &builder, const GUARD &guard ) : m_builder( builder ), m_guard( guard )
{
}

/* The LLVM type of a native xDev type; nullptr for the other types */
llvm::Type *X_LLVM_NATIVE::type( X_DECL_TYPE declared ) const
{
   llvm::LLVMContext &context = m_builder.getContext();

   switch( declared )
   {
   case X_DECL_TYPE::INT8:
   case X_DECL_TYPE::UINT8:    return llvm::Type::getInt8Ty( context );
   case X_DECL_TYPE::INT16:
   case X_DECL_TYPE::UINT16:   return llvm::Type::getInt16Ty( context );
   case X_DECL_TYPE::INT32:
   case X_DECL_TYPE::UINT32:   return llvm::Type::getInt32Ty( context );
   case X_DECL_TYPE::INT64:
   case X_DECL_TYPE::UINT64:   return llvm::Type::getInt64Ty( context );
   case X_DECL_TYPE::FLOAT32:  return llvm::Type::getFloatTy( context );
   case X_DECL_TYPE::FLOAT64:  return llvm::Type::getDoubleTy( context );
   case X_DECL_TYPE::LOGIC:    return llvm::Type::getInt1Ty( context );
   case X_DECL_TYPE::PSZ:
   case X_DECL_TYPE::PTR:
   case X_DECL_TYPE::CALLBACK: return llvm::PointerType::get( context, 0 );
   default:                    return nullptr;
   }
}

/* ---------------------------------------------------------------------------------------------
   Conversions
   --------------------------------------------------------------------------------------------- */

/* A number is .T. when it is not zero */
llvm::Value *X_LLVM_NATIVE::toLogic( llvm::Value *value, llvm::Type *sourceType )
{
   llvm::Constant *zero = llvm::Constant::getNullValue( sourceType );

   if( sourceType->isFloatingPointTy() )
      return m_builder.CreateFCmpUNE( value, zero );

   return m_builder.CreateICmpNE( value, zero );
}

/* The value must fit the target; both bounds are compared one bit wider than the wider type */
void X_LLVM_NATIVE::checkIntegerRange( llvm::Value *value, X_DECL_TYPE source, X_DECL_TYPE target )
{
   const unsigned sourceWidth = type( source )->getIntegerBitWidth();
   const unsigned targetWidth = type( target )->getIntegerBitWidth();
   const unsigned width = std::max( sourceWidth, targetWidth ) + 1;
   llvm::IntegerType *wideType = llvm::IntegerType::get( m_builder.getContext(), width );
   llvm::APInt minimum;
   llvm::APInt maximum;
   llvm::Value *wide;
   llvm::Value *atLeast;
   llvm::Value *atMost;

   if( s_zeroExtends( source ) )
      wide = m_builder.CreateZExt( value, wideType );
   else
      wide = m_builder.CreateSExt( value, wideType );

   if( s_unsigned( target ) )
   {
      minimum = llvm::APInt( width, 0 );
      maximum = llvm::APInt::getMaxValue( targetWidth ).zext( width );
   }
   else
   {
      minimum = llvm::APInt::getSignedMinValue( targetWidth ).sext( width );
      maximum = llvm::APInt::getSignedMaxValue( targetWidth ).sext( width );
   }

   atLeast = m_builder.CreateICmpSGE( wide, llvm::ConstantInt::get( wideType, minimum ) );
   atMost = m_builder.CreateICmpSLE( wide, llvm::ConstantInt::get( wideType, maximum ) );
   m_guard( m_builder.CreateAnd( atLeast, atMost ), X_NATIVE_ERROR::RANGE );
}

/* The whole part must lie in [ -2^bits, 2^bits ) for a signed target, [ 0, 2^bits ) for an unsigned one */
void X_LLVM_NATIVE::checkFloatRange( llvm::Value *whole, llvm::Type *sourceType, X_DECL_TYPE target )
{
   const bool unsignedTarget = s_unsigned( target );
   unsigned bits = type( target )->getIntegerBitWidth();
   double upper;
   double lower = 0.0;
   llvm::Value *atLeast;
   llvm::Value *below;

   if( !unsignedTarget )
      --bits;

   upper = std::ldexp( 1.0, bits );

   if( !unsignedTarget )
      lower = -upper;

   atLeast = m_builder.CreateFCmpOGE( whole, llvm::ConstantFP::get( sourceType, lower ) );
   below = m_builder.CreateFCmpOLT( whole, llvm::ConstantFP::get( sourceType, upper ) );
   m_guard( m_builder.CreateAnd( atLeast, below ), X_NATIVE_ERROR::RANGE );
}

/* The value of type source as conversion.target; nullptr when the conversion is not native */
llvm::Value *X_LLVM_NATIVE::convert( llvm::Value *value, X_DECL_TYPE source, const X_TYPE_CONVERSION &conversion )
{
   const X_DECL_TYPE target = conversion.target;
   llvm::Type *sourceType = type( source );
   llvm::Type *targetType = type( target );

   if( !sourceType || !targetType || conversion.checkType )
      return nullptr;

   if( source == target )
      return value;

   /* CAST between PTR and an integer goes through UINT64 */
   if( conversion.explicitCast && source == X_DECL_TYPE::PTR && s_integer( target ) )
   {
      llvm::Value *bits = m_builder.CreatePtrToInt( value, llvm::Type::getInt64Ty( m_builder.getContext() ) );

      return convert( bits, X_DECL_TYPE::UINT64, s_checkedCast( target ) );
   }

   if( conversion.explicitCast && target == X_DECL_TYPE::PTR && s_integer( source ) )
   {
      llvm::Value *bits = convert( value, source, s_checkedCast( X_DECL_TYPE::UINT64 ) );

      return m_builder.CreateIntToPtr( bits, targetType );
   }

   if( source == X_DECL_TYPE::PTR || target == X_DECL_TYPE::PTR )
      return nullptr;

   /* LOGIC converts only by CAST */
   if( ( source == X_DECL_TYPE::LOGIC || target == X_DECL_TYPE::LOGIC ) && !conversion.explicitCast )
      return nullptr;

   if( target == X_DECL_TYPE::LOGIC )
      return toLogic( value, sourceType );

   if( sourceType->isIntegerTy() && targetType->isIntegerTy() )
   {
      if( conversion.checkRange )
         checkIntegerRange( value, source, target );

      return m_builder.CreateIntCast( value, targetType, !s_zeroExtends( source ) );
   }

   if( sourceType->isIntegerTy() && s_zeroExtends( source ) )
      return m_builder.CreateUIToFP( value, targetType );

   if( sourceType->isIntegerTy() )
      return m_builder.CreateSIToFP( value, targetType );

   if( targetType->isFloatingPointTy() )
      return m_builder.CreateFPCast( value, targetType );

   /* A float to an integer: the fraction is cut towards zero */
   if( targetType->isIntegerTy() )
   {
      llvm::Value *whole = m_builder.CreateUnaryIntrinsic( llvm::Intrinsic::trunc, value );

      if( conversion.checkRange )
         checkFloatRange( whole, sourceType, target );

      if( s_unsigned( target ) )
         return m_builder.CreateFPToUI( whole, targetType );

      return m_builder.CreateFPToSI( whole, targetType );
   }

   return nullptr;
}

/* ---------------------------------------------------------------------------------------------
   Operators
   --------------------------------------------------------------------------------------------- */

/* + - * ^ / of floats; division by zero is an error */
llvm::Value *X_LLVM_NATIVE::floatArithmetic( llvm::Value *left, llvm::Value *right, const std::string &op )
{
   llvm::Value *zero;

   if( op == "+" )
      return m_builder.CreateFAdd( left, right );

   if( op == "-" )
      return m_builder.CreateFSub( left, right );

   if( op == "*" )
      return m_builder.CreateFMul( left, right );

   if( op == "^" )
      return m_builder.CreateBinaryIntrinsic( llvm::Intrinsic::pow, left, right );

   if( op != "/" )
      return nullptr;

   zero = llvm::ConstantFP::get( right->getType(), 0.0 );
   m_guard( m_builder.CreateFCmpUNE( right, zero ), X_NATIVE_ERROR::DIVZERO );

   return m_builder.CreateFDiv( left, right );
}

/* / and % of integers: division by zero and MIN / -1 are errors */
llvm::Value *X_LLVM_NATIVE::divide( llvm::Value *left, llvm::Value *right, X_DECL_TYPE operation, bool remainder )
{
   llvm::Type *valueType = type( operation );
   const bool unsignedType = s_unsigned( operation );

   m_guard( m_builder.CreateICmpNE( right, llvm::ConstantInt::get( valueType, 0 ) ), X_NATIVE_ERROR::DIVZERO );

   if( !unsignedType )
   {
      const llvm::APInt minimumValue = llvm::APInt::getSignedMinValue( valueType->getIntegerBitWidth() );
      llvm::Value *isMinimum = m_builder.CreateICmpEQ( left, llvm::ConstantInt::get( valueType, minimumValue ) );
      llvm::Value *isMinusOne = m_builder.CreateICmpEQ( right, llvm::ConstantInt::getAllOnesValue( valueType ) );

      m_guard( m_builder.CreateNot( m_builder.CreateAnd( isMinimum, isMinusOne ) ), X_NATIVE_ERROR::OVERFLOW );
   }

   if( remainder && unsignedType )
      return m_builder.CreateURem( left, right );

   if( remainder )
      return m_builder.CreateSRem( left, right );

   if( unsignedType )
      return m_builder.CreateUDiv( left, right );

   return m_builder.CreateSDiv( left, right );
}

/* + - * / % ^ of a native type; integer overflow is an error */
llvm::Value *X_LLVM_NATIVE::arithmetic( llvm::Value *left, llvm::Value *right, X_DECL_TYPE operation,
                                        const std::string &op )
{
   llvm::Type *valueType = type( operation );
   llvm::Intrinsic::ID intrinsic;
   llvm::Value *result;

   if( !valueType || operation == X_DECL_TYPE::LOGIC || operation == X_DECL_TYPE::PTR )
      return nullptr;

   if( valueType->isFloatingPointTy() )
      return floatArithmetic( left, right, op );

   if( op == "/" || op == "%" )
      return divide( left, right, operation, op == "%" );

   intrinsic = s_overflowIntrinsic( op, s_unsigned( operation ) );
   if( intrinsic == llvm::Intrinsic::not_intrinsic )
      return nullptr;

   result = m_builder.CreateBinaryIntrinsic( intrinsic, left, right );
   m_guard( m_builder.CreateNot( m_builder.CreateExtractValue( result, 1 ) ), X_NATIVE_ERROR::OVERFLOW );

   return m_builder.CreateExtractValue( result, 0 );
}

/* -x of an integer is 0 - x with the overflow check */
llvm::Value *X_LLVM_NATIVE::negate( llvm::Value *value, X_DECL_TYPE operation )
{
   llvm::Type *valueType = type( operation );

   if( s_integer( operation ) )
      return arithmetic( llvm::ConstantInt::get( valueType, 0 ), value, operation, "-" );

   if( valueType && valueType->isFloatingPointTy() )
      return m_builder.CreateFNeg( value );

   return nullptr;
}

/* PTR compares only for equality */
llvm::Value *X_LLVM_NATIVE::compare( llvm::Value *left, llvm::Value *right, X_DECL_TYPE operation,
                                     const std::string &op )
{
   llvm::Type *valueType = type( operation );
   const X_PREDICATE *entry = s_findPredicate( op );
   llvm::CmpInst::Predicate predicate;

   if( !valueType )
      return nullptr;

   if( operation == X_DECL_TYPE::PTR && op != "=" && op != "==" && op != "!=" )
      return nullptr;

   if( !entry )
      return nullptr;

   if( valueType->isFloatingPointTy() )
      predicate = entry->floating;
   else if( s_zeroExtends( operation ) )
      predicate = entry->unsignedInt;
   else
      predicate = entry->signedInt;

   return m_builder.CreateCmp( predicate, left, right );
}
