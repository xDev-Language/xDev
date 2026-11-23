/*
 * Copyright (c) 2024-2026 xDev-Language
 *
 * Library...: xDev Frontend
 * File......: semantic.hpp
 *
 */

#ifndef SEMANTIC_HPP
#define SEMANTIC_HPP

#include "ast.hpp"

/* warnings: the unused variables and parameters of the functions; nullptr does not look for them */
bool x_analyzeProgram( X_PROGRAM &program, std::string &error, bool libraryModule, const X_LOCATION &location = {},
                       std::vector<std::string> *warnings = nullptr );

#endif /* SEMANTIC_HPP */
