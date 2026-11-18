/*
 * Copyright (c) 2024-2026 xDev-Language
 *
 * Library: xDev Frontend
 * File: constant.hpp
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#ifndef CONSTANT_HPP
#define CONSTANT_HPP

#include "ast.hpp"
#include "internal.h"

/* An X_ITEM released when it goes out of scope */
struct X_OWNED_ITEM
{
   X_ITEM value = X_ITEM_NIL_INIT;

   ~X_OWNED_ITEM()
   {
      x_itemClear( &value );
   }
};

bool x_foldConstant( const X_EXPR &expr, X_ITEM *result );
bool x_validateConstants( const X_PROGRAM &program, std::string &error, const X_LOCATION &location = {} );

#endif /* CONSTANT_HPP */
