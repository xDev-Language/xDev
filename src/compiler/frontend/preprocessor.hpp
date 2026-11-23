/*
 * Copyright (c) 2024-2026 xDev-Language
 *
 * Library...: xDev Frontend
 * File......: preprocessor.hpp
 *
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

/* A token of an output line: its byte offset in the line and its place in the source file */
struct X_SOURCE_COLUMN
{
   int offset;
   int line;
   int column;
};

/* location( line ) is "file:line"; location( line, column ) adds the column of the source file when it is known.
   column is the byte offset (from 1) in the output line */
struct X_PREPROCESSED
{
   std::string text;
   std::vector<X_SOURCE_LOCATION> lines;
   std::vector<std::vector<X_SOURCE_COLUMN>> columns;
   std::vector<std::string> warnings;
   std::vector<std::string> messages;
   std::vector<std::string> files;
   std::string location( int line ) const;
   std::string location( int line, int column ) const;
};

bool x_preprocess( const std::string &file, const std::vector<std::string> &includePaths,
                   const std::vector<std::string> &definitions, X_PREPROCESSED &result, std::string &error );

#endif /* PREPROCESSOR_HPP */
