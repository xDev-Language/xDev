/*
 * Copyright (c) 2024-2026 xDev-Language
 *
 * Library: xDev Backend
 * File: native.hpp
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#ifndef NATIVE_HPP
#define NATIVE_HPP

#include "ast.hpp"

#include <functional>
#include <string>

#include <llvm/IR/IRBuilder.h>

enum class X_NATIVE_ERROR
{
   RANGE,
   OVERFLOW,
   DIVZERO
};

/* Native values: numbers, LOGIC and pointers as LLVM values; errors go to the guard */
class X_LLVM_NATIVE
{
public:
   /* Emits the error unless the condition holds */
   using GUARD = std::function<void( llvm::Value *, X_NATIVE_ERROR )>;

   X_LLVM_NATIVE( llvm::IRBuilder<> &builder, const GUARD &guard );

   llvm::Type *type( X_DECL_TYPE declared ) const;
   llvm::Value *convert( llvm::Value *value, X_DECL_TYPE source, const X_TYPE_CONVERSION &conversion );
   llvm::Value *arithmetic( llvm::Value *left, llvm::Value *right, X_DECL_TYPE operation, const std::string &op );
   llvm::Value *negate( llvm::Value *value, X_DECL_TYPE operation );
   llvm::Value *compare( llvm::Value *left, llvm::Value *right, X_DECL_TYPE operation, const std::string &op );

private:
   llvm::IRBuilder<> &m_builder;
   GUARD m_guard;

   llvm::Value *toLogic( llvm::Value *value, llvm::Type *sourceType );
   void checkIntegerRange( llvm::Value *value, X_DECL_TYPE source, X_DECL_TYPE target );
   void checkFloatRange( llvm::Value *whole, llvm::Type *sourceType, X_DECL_TYPE target );
   llvm::Value *floatArithmetic( llvm::Value *left, llvm::Value *right, const std::string &op );
   llvm::Value *divide( llvm::Value *left, llvm::Value *right, X_DECL_TYPE operation, bool remainder );
};

#endif /* NATIVE_HPP */
