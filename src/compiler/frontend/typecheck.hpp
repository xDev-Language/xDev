/*
 * Copyright (c) 2024-2026 xDev-Language
 *
 * Library...: xDev Frontend
 * File......: typecheck.hpp
 *
 */

#ifndef TYPECHECK_HPP
#define TYPECHECK_HPP

#include "ast.hpp"

#include <string>

bool x_checkTypes( X_PROGRAM &program, std::string &error, const X_LOCATION &location = {} );

#endif /* TYPECHECK_HPP */
