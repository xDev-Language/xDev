/*
 * Copyright (c) 2024-2026 xDev-Language
 *
 * Library...: xDev Backend
 * File......: interface.hpp
 *
 */

#ifndef INTERFACE_HPP
#define INTERFACE_HPP

#include "ast.hpp"

#include <string>
#include <vector>

/* The classes of a library as text, kept in the section .xdevcls of its object */
std::string x_classInterfaceWrite( const X_PROGRAM &program );

/* The classes of the libraries among the inputs */
bool x_classInterfaceRead( const std::vector<std::string> &inputs, X_PROGRAM &program, std::string &error );

#endif /* INTERFACE_HPP */
