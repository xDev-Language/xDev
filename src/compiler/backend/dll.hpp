/*
 * Copyright (c) 2024-2026 xDev-Language
 *
 * Library: xDev Backend
 * File: dll.hpp
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#ifndef DLL_HPP
#define DLL_HPP

#include <string>
#include <vector>

/* The libraries for the DLL FUNCTION declarations found in the inputs: import libraries on Windows */
bool x_dllImports( const std::vector<std::string> &inputs, const std::vector<std::string> &searchPaths,
                   const std::string &directory, std::vector<std::string> &imports, std::string &error );

#endif /* DLL_HPP */
