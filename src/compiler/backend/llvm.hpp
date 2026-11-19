/*
 * Copyright (c) 2024-2026 xDev-Language
 *
 * Library: xDev Backend
 * File: llvm.hpp
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#ifndef LLVM_HPP
#define LLVM_HPP

#include "ast.hpp"

#include <functional>
#include <memory>
#include <string>
#include <vector>

class X_LLVM
{
public:
   X_LLVM();
   ~X_LLVM();

   /* ---------------------------------------------------------------------------------------------
      Code generation
      --------------------------------------------------------------------------------------------- */

   bool generate( const X_PROGRAM &program, std::string &error, bool libraryModule = false,
                  const X_LOCATION &location = {}, bool sharedModule = false );

   /* ---------------------------------------------------------------------------------------------
      Output
      --------------------------------------------------------------------------------------------- */

   std::string ir() const;
   bool writeIR( const std::string &path, std::string &error ) const;
   bool writeObject( const std::string &path, std::string &error ) const;

private:
   struct IMPL;
   std::unique_ptr<IMPL> m_impl;
};

/* An object with embedded files (.rmk images), which programs reach through x_resourceFind() */
bool x_llvmResources( const std::vector<std::string> &files, const std::string &path, std::string &error );

#endif /* LLVM_HPP */
