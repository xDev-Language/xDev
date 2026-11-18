/*
 * Copyright (c) 2024-2026 xDev-Language
 *
 * Library: xDev Frontend
 * File: preprocessor.hpp
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#ifndef PREPROCESSOR_HPP
#define PREPROCESSOR_HPP

#include <string>
#include <vector>

struct X_SOURCE_LOCATION
{
   std::string file;
   int line;
};

struct X_PREPROCESSED
{
   std::string text;
   std::vector<X_SOURCE_LOCATION> lines;
   std::vector<std::string> warnings;
   std::vector<std::string> messages;
   std::vector<std::string> files;
   std::string location( int line ) const;
};

bool x_preprocess( const std::string &file, const std::vector<std::string> &includePaths,
                   const std::vector<std::string> &definitions, X_PREPROCESSED &result, std::string &error );

#endif /* PREPROCESSOR_HPP */
