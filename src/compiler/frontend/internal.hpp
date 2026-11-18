/*
 * Copyright (c) 2024-2026 xDev-Language
 *
 * Library: xDev Frontend
 * File: internal.hpp
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#ifndef X_INTERNAL_HPP
#define X_INTERNAL_HPP

#include <string>
#include <vector>
#include <unordered_set>

struct PP_TOKEN
{
   std::string text;
   bool identifier;
   bool attached;
   std::unordered_set<std::string> disabled;
};

using PP_TOKENS = std::vector<PP_TOKEN>;

bool x_ppCondition( const PP_TOKENS &tokens, bool &value, std::string &error );

#endif /* X_INTERNAL_HPP */
