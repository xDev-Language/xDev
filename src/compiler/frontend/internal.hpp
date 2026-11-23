/*
 * Copyright (c) 2024-2026 xDev-Language
 *
 * Library...: xDev Frontend
 * File......: internal.hpp
 *
 */

#ifndef X_INTERNAL_HPP
#define X_INTERNAL_HPP

#include <string>
#include <vector>
#include <unordered_set>

/* line and column (from 1, in characters) are the place of the token in its source file */
struct PP_TOKEN
{
   std::string text;
   bool identifier;
   bool attached;
   std::unordered_set<std::string> disabled;
   int line = 0;
   int column = 0;
};

using PP_TOKENS = std::vector<PP_TOKEN>;

bool x_ppCondition( const PP_TOKENS &tokens, bool &value, std::string &error );

#endif /* X_INTERNAL_HPP */
