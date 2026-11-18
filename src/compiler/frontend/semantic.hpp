/*
 * Copyright (c) 2024-2026 xDev-Language
 *
 * Library: xDev Frontend
 * File: semantic.hpp
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#ifndef SEMANTIC_HPP
#define SEMANTIC_HPP

#include "ast.hpp"

bool x_analyzeProgram( X_PROGRAM &program, std::string &error, bool libraryModule, const X_LOCATION &location = {} );

#endif /* SEMANTIC_HPP */
